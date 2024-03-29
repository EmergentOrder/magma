/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zsymmetrize_tiles.cu normal z -> d, Fri Jul 18 17:34:12 2014
       @author Mark Gates
*/
#include "common_magma.h"
#include <assert.h>

#define NB 64

/*
    Symmetrizes ntile tiles at a time, e.g., all diagonal tiles of a matrix.
    Grid is ntile x ceil(m/NB).
    Each tile is m x m, and is divided into block rows, each NB x m.
    Each block has NB threads.
    Each thread copies one row, iterating across all columns below diagonal.
    The bottom block of rows may be partially outside the matrix;
    if so, rows outside the matrix (i >= m) are disabled.
*/
__global__ void
dsymmetrize_tiles_lower( int m, double *dA, int ldda, int mstride, int nstride )
{
    // shift dA to tile's top-left corner
    dA += blockIdx.x*(mstride + nstride*ldda);
    
    // dA iterates across row i and dAT iterates down column i.
    int i = blockIdx.y*NB + threadIdx.x;
    double *dAT = dA;
    if ( i < m ) {
        dA  += i;
        dAT += i*ldda;
        double *dAend = dA + i*ldda;
        while( dA < dAend ) {
            *dAT = (*dA);  // upper := lower
            dA  += ldda;
            dAT += 1;
        }
    }
}


// only difference with _lower version is direction dA=dAT instead of dAT=dA.
__global__ void
dsymmetrize_tiles_upper( int m, double *dA, int ldda, int mstride, int nstride )
{
    // shift dA to tile's top-left corner
    dA += blockIdx.x*(mstride + nstride*ldda);
    
    // dA iterates across row i and dAT iterates down column i.
    int i = blockIdx.y*NB + threadIdx.x;
    double *dAT = dA;
    if ( i < m ) {
        dA  += i;
        dAT += i*ldda;
        double *dAend = dA + i*ldda;
        while( dA < dAend ) {
            *dA  = (*dAT);  // lower := upper
            dA  += ldda;
            dAT += 1;
        }
    }
}


/**
    Purpose
    -------
    
    DSYMMETRIZE_TILES copies lower triangle to upper triangle, or vice-versa,
    to make some blocks of dA into general representations of a symmetric block.
    This processes NTILE blocks, typically the diagonal blocks.
    Each block is offset by mstride rows and nstride columns from the previous block.
    
    Arguments
    ---------
    
    @param[in]
    uplo    magma_uplo_t
            Specifies the part of the matrix dA that is valid on input.
      -     = MagmaUpper:      Upper triangular part
      -     = MagmaLower:      Lower triangular part
    
    @param[in]
    m       INTEGER
            The number of rows & columns of each square block of dA.  M >= 0.
    
    @param[in,out]
    dA      DOUBLE_PRECISION array, dimension (LDDA,N)
            The matrix dA. N = m + nstride*(ntile-1).
    
    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDDA >= max(1, m + mstride*(ntile-1)).
    
    @param[in]
    ntile   INTEGER
            Number of blocks to symmetrize.
    
    @param[in]
    mstride INTEGER
            Row offset from start of one block to start of next block.
    
    @param[in]
    nstride INTEGER
            Column offset from start of one block to start of next block.

    @ingroup magma_daux2
    ********************************************************************/
extern "C" void
magmablas_dsymmetrize_tiles( magma_uplo_t uplo, magma_int_t m, double *dA, magma_int_t ldda,
                             magma_int_t ntile, magma_int_t mstride, magma_int_t nstride )
{
    if ( m == 0 || ntile == 0 )
        return;
    
    assert( m >= 0 );
    assert( ldda >= m );
    assert( ldda >= (ntile - 1)*mstride + m );
    assert( ntile >= 0 );
    assert( mstride >= 0 );
    assert( nstride >= 0 );
    assert( mstride >= m || nstride >= m );  // prevent tile overlap
    
    dim3 threads( NB );
    dim3 grid( ntile, (m + NB - 1)/NB );
    
    //printf( "m %d, grid %d x %d, threads %d\n", m, grid.x, grid.y, threads.x );
    if ( uplo == MagmaUpper ) {
        dsymmetrize_tiles_upper<<< grid, threads, 0, magma_stream >>>( m, dA, ldda, mstride, nstride );
    }
    else if ( uplo == MagmaLower ) {
        dsymmetrize_tiles_lower<<< grid, threads, 0, magma_stream >>>( m, dA, ldda, mstride, nstride );
    }
    else {
        printf( "uplo has illegal value\n" );
        exit(1);
    }
}
