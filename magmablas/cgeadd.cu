/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgeadd.cu normal z -> c, Fri Jul 18 17:34:11 2014
       @author Mark Gates
*/
#include "common_magma.h"
#include <assert.h>

#define NB 64

/* =====================================================================
    Matrix is m x n, and is divided into block rows, each NB x n.
    Each CUDA block has NB threads to handle one block row.
    Each thread adds one row, iterating across all columns.
    The bottom block of rows may be partially outside the matrix;
    if so, rows outside the matrix (i >= m) are disabled.
    
    TODO. Block in both directions, for large matrices.
    E.g., each block does 64x64 tile, instead of 64xN tile.
*/
__global__ void
cgeadd_kernel(
    int m, int n,
    magmaFloatComplex alpha,
    const magmaFloatComplex *dA, int ldda,
    magmaFloatComplex       *dB, int lddb )
{
    // dA and dB iterate across row i
    int i = blockIdx.x*blockDim.x + threadIdx.x;
    if ( i < m ) {
        dA += i;
        dB += i;
        const magmaFloatComplex *dAend = dA + n*ldda;
        while( dA < dAend ) {
            *dB = alpha*(*dA) + (*dB);
            dA += ldda;
            dB += lddb;
        }
    }
}


/* ===================================================================== */
/**
    Purpose
    -------
    ZGEADD adds two matrices, dB = alpha*dA + dB.
    
    Arguments
    ---------
    
    @param[in]
    m       INTEGER
            The number of rows of the matrix dA.  M >= 0.
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix dA.  N >= 0.
    
    @param[in]
    alpha   COMPLEX
            The scalar alpha.
            
    @param[in]
    dA      COMPLEX array, dimension (LDDA,N)
            The m by n matrix dA.
    
    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,M).
            
    @param[in,out]
    dB      COMPLEX array, dimension (LDDB,N)
            The m by n matrix dB.
    
    @param[in]
    lddb    INTEGER
            The leading dimension of the array dB.  LDDB >= max(1,M).
    

    @ingroup magma_caux2
    ********************************************************************/
extern "C" void
magmablas_cgeadd(
    magma_int_t m, magma_int_t n,
    magmaFloatComplex alpha,
    const magmaFloatComplex *dA, magma_int_t ldda,
    magmaFloatComplex       *dB, magma_int_t lddb )
{
    magma_int_t info = 0;
    if ( m < 0 )
        info = -1;
    else if ( n < 0 )
        info = -2;
    else if ( ldda < max(1,m))
        info = -5;
    else if ( lddb < max(1,m))
        info = -7;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    if ( m == 0 || n == 0 )
        return;
    
    dim3 threads( NB );
    dim3 grid( (m + NB - 1)/NB );
    
    cgeadd_kernel<<< grid, threads, 0, magma_stream >>>(
        m, n, alpha, dA, ldda, dB, lddb );
}
