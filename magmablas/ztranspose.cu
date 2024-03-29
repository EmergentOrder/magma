/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @precisions normal z -> s d c

       @author Stan Tomov
       @author Mark Gates
*/
#include "common_magma.h"
//#include "commonblas.h"

#define PRECISION_z

#if defined(PRECISION_z)
    #define NX 16
#else
    #define NX 32
#endif

#define NB 32
#define NY 8


// tile M-by-N matrix with ceil(M/NB) by ceil(N/NB) tiles sized NB-by-NB.
// uses NX-by-NY threads, where NB/NX, NB/NY, NX/NY evenly.
// subtile each NB-by-NB tile with (NB/NX) subtiles sized NX-by-NB
// for each subtile
//     load NX-by-NB subtile transposed from A into sA, as (NB/NY) blocks sized NX-by-NY
//     save NB-by-NX subtile from sA into AT,   as (NB/NX)*(NX/NY) blocks sized NX-by-NY
//     A  += NX
//     AT += NX*ldat
//
// e.g., with NB=32, NX=32, NY=8 ([sdc] precisions)
//     load 32x32 subtile as 4   blocks of 32x8 columns: (A11  A12  A13  A14 )
//     save 32x32 subtile as 1*4 blocks of 32x8 columns: (AT11 AT12 AT13 AT14)
//
// e.g., with NB=32, NX=16, NY=8 (z precision)
//     load 16x32 subtile as 4   blocks of 16x8 columns: (A11  A12  A13  A14)
//     save 32x16 subtile as 2*2 blocks of 16x8 columns: (AT11 AT12)
//                                                       (AT21 AT22)
__global__ void
ztranspose_kernel(
    int m, int n,
    const magmaDoubleComplex *A, int lda,
    magmaDoubleComplex *AT,      int ldat)
{
    __shared__ magmaDoubleComplex sA[NB][NX+1];

    int tx  = threadIdx.x;
    int ty  = threadIdx.y;
    int ibx = blockIdx.x*NB;
    int iby = blockIdx.y*NB;
    int i, j;
    
    A  += ibx + tx + (iby + ty)*lda;
    AT += iby + tx + (ibx + ty)*ldat;
    
    #pragma unroll
    for( int tile=0; tile < NB/NX; ++tile ) {
        // load NX-by-NB subtile transposed from A into sA
        i = ibx + tx + tile*NX;
        j = iby + ty;
        if (i < m) {
            #pragma unroll
            for( int j2=0; j2 < NB; j2 += NY ) {
                if (j + j2 < n) {
                    sA[ty + j2][tx] = A[j2*lda];
                }
            }
        }
        __syncthreads();
        
        // save NB-by-NX subtile from sA into AT
        i = iby + tx;
        j = ibx + ty + tile*NX;
        #pragma unroll
        for( int i2=0; i2 < NB; i2 += NX ) {
            if (i + i2 < n) {
                #pragma unroll
                for( int j2=0; j2 < NX; j2 += NY ) {
                    if (j + j2 < m) {
                        AT[i2 + j2*ldat] = sA[tx + i2][ty + j2];
                    }
                }
            }
        }
        __syncthreads();
        
        // move to next subtile
        A  += NX;
        AT += NX*ldat;
    }
}


/**
    Purpose
    -------
    ztranspose_stream copies and transposes a matrix dA to matrix dAT.
    
    Same as ztranspose, but adds stream argument.
        
    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix dA.  M >= 0.
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix dA.  N >= 0.
    
    @param[in]
    dA      COMPLEX_16 array, dimension (LDDA,N)
            The M-by-N matrix dA.
    
    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDDA >= M.
    
    @param[in]
    dAT     COMPLEX_16 array, dimension (LDDA,N)
            The N-by-M matrix dAT.
    
    @param[in]
    lddat   INTEGER
            The leading dimension of the array dAT.  LDDAT >= N.
    
    @param[in]
    stream  magma_queue_t
            Stream to execute in.
    
    @ingroup magma_zaux2
    ********************************************************************/
extern "C" void
magmablas_ztranspose_stream(
    magma_int_t m, magma_int_t n,
    const magmaDoubleComplex *dA,  magma_int_t ldda,
    magmaDoubleComplex       *dAT, magma_int_t lddat, magma_queue_t stream )
{
    magma_int_t info = 0;
    if ( m < 0 )
        info = -1;
    else if ( n < 0 )
        info = -2;
    else if ( ldda < m )
        info = -4;
    else if ( lddat < n )
        info = -6;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }
    
    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    dim3 threads( NX, NY );
    dim3 grid( (m+NB-1)/NB, (n+NB-1)/NB );
    ztranspose_kernel<<< grid, threads, 0, stream >>>
        ( m, n, dA, ldda, dAT, lddat );
}


/**
    @see magmablas_ztranspose_stream
    @ingroup magma_zaux2
    ********************************************************************/
extern "C" void
magmablas_ztranspose(
    magma_int_t m, magma_int_t n,
    const magmaDoubleComplex *dA,  magma_int_t ldda,
    magmaDoubleComplex       *dAT, magma_int_t lddat )
{
    magmablas_ztranspose_stream( m, n, dA, ldda, dAT, lddat, magma_stream );
}
