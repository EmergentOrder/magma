/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from dgemm_tesla_ab_0.cu normal d -> s, Fri Jul 18 17:34:13 2014
*/
#include "common_magma.h"
#include "commonblas_s.h"

/*
 * Computes C = alpha*A*B + beta*C when alpha == 0 and beta == 0.
 * That is, C = 0.
 */
__global__ void
sgemm_kernel_ab_0(
    float*       __restrict__ C,
    const float* __restrict__ A,
    const float* __restrict__ B,
    int m, int n, int k,
    int lda, int ldb, int ldc,
    float alpha, float beta )
{
    const int tx = threadIdx.x;
    const int ty = threadIdx.y;

    int ibx = blockIdx.x * 64;
    int iby = blockIdx.y * 16;

    const int idt = ty * 16 + tx;

    C += ibx + idt + __mul24(iby, ldc);

    ibx = ibx + idt - m;
    
    if ( (iby+16) >= n ) {
        lda = n - iby;
    }
    else {
        lda = 16;
    }
    if ( ibx >= 0 )
        lda = 0;
    else
        lda = lda;

    switch(lda) {
        case 16:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                C[11*ldc] = 0;
                C[12*ldc] = 0;
                C[13*ldc] = 0;
                C[14*ldc] = 0;
                C[15*ldc] = 0;
                break;
        case 15:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                C[11*ldc] = 0;
                C[12*ldc] = 0;
                C[13*ldc] = 0;
                C[14*ldc] = 0;
                break;
        case 14:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                C[11*ldc] = 0;
                C[12*ldc] = 0;
                C[13*ldc] = 0;
                break;
        case 13:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                C[11*ldc] = 0;
                C[12*ldc] = 0;
                break;
        case 12:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                C[11*ldc] = 0;
                break;
        case 11:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                C[10*ldc] = 0;
                break;
        case 10:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                C[ 9*ldc] = 0;
                break;
        case 9:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                C[ 8*ldc] = 0;
                break;
        case 8:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                C[ 7*ldc] = 0;
                break;
        case 7:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                C[ 6*ldc] = 0;
                break;
        case 6:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                C[ 5*ldc] = 0;
                break;
        case 5:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                C[ 4*ldc] = 0;
                break;
        case 4:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                C[ 3*ldc] = 0;
                break;
        case 3:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                C[ 2*ldc] = 0;
                break;
        case 2:
                C[ 0    ] = 0;
                C[ 1*ldc] = 0;
                break;
        case 1:
                C[ 0    ] = 0;
                break;
        case 0:
                break;
    }
}


extern "C" void
magmablas_sgemm_ab_0(
    float *C, const float *A, const float *B,
    magma_int_t m, magma_int_t n, magma_int_t k,
    magma_int_t lda, magma_int_t ldb, magma_int_t ldc,
    float alpha, float beta )
{
    dim3 threads( 16, 4 );
    dim3 grid( (m - 1)/64 + 1, (n - 1)/16 + 1 );
    sgemm_kernel_ab_0<<< grid, threads, 0, magma_stream >>>
        ( C, A, B, m, n, k, lda, ldb, ldc, alpha, beta );
}
