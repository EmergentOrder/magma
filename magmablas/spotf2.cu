/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014
       
       @generated from zpotf2.cu normal z -> s, Fri Jul 18 17:34:13 2014
*/

#include <stdio.h>
#include "common_magma.h"
#include "magmablas.h"

#define PRECISION_s

//#if (GPUSHMEM < 200)
#define sdot_max_bs 512  // 512 is max threads for 1.x cards
//#else
//#define sdot_max_bs 1024
//#endif

#define A(i, j)  (A + (i) + (j)*lda)   // A(i, j) means at i row, j column

void spotf2_sscal(magma_int_t n, float *x, magma_int_t incx);
void spotf2_sdot(magma_int_t n, float *x, magma_int_t incx);

#if defined(PRECISION_z) || defined(PRECISION_c)
void slacgv(magma_int_t n, float *x, magma_int_t incx);
#endif

/**
    Purpose
    -------

    spotf2 computes the Cholesky factorization of a real symmetric
    positive definite matrix A.

    The factorization has the form
        A = U' * U , if UPLO = MagmaUpper, or
        A = L  * L', if UPLO = MagmaLower,
    where U is an upper triangular matrix and L is lower triangular.

    This is the unblocked version of the algorithm, calling Level 2 BLAS.

    Arguments
    ---------

    @param[in]
    uplo    magma_uplo_t
            Specifies whether the upper or lower triangular part of the
            symmetric matrix A is stored.
      -     = MagmaUpper:  Upper triangular
      -     = MagmaLower:  Lower triangular

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0 and N <= 1024.
            On CUDA architecture 1.x cards, N <= 512.

    @param[in,out]
    A       REAL array, dimension (LDA,N)
            On entry, the symmetric matrix A.  If UPLO = MagmaUpper, the leading
            n by n upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = MagmaLower, the
            leading n by n lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
    \n
            On exit, if INFO = 0, the factor U or L from the Cholesky
            factorization A = U'*U  or A = L*L'.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0: successful exit
      -     < 0: if INFO = -k, the k-th argument had an illegal value
      -     > 0: if INFO = k, the leading minor of order k is not
                 positive definite, and the factorization could not be
                 completed.

    @ingroup magma_sposv_aux
    ********************************************************************/
extern "C" magma_int_t
magma_spotf2_gpu(
    magma_uplo_t uplo, magma_int_t n,
    float *A, magma_int_t lda,
    magma_int_t *info )
{
    magma_int_t j;

    *info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower) {
        *info = -1;
    } else if (n < 0 || n > sdot_max_bs) {
        *info = -2;
    } else if (lda < max(1,n)) {
        *info = -4;
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    // Quick return if possible
    if (n == 0) {
        return *info;
    }

    float alpha = MAGMA_S_NEG_ONE;
    float beta  = MAGMA_S_ONE;

    if (uplo == MagmaUpper) {
        for(j = 0; j < n; j++) {
            spotf2_sdot(j, A(0,j), 1); // including sdot product and update a(j,j)
            if (j < n) {
                #if defined(PRECISION_z) || defined(PRECISION_c)
                slacgv(j, A(0, j), 1);
                #endif
                magma_sgemv( MagmaTrans, j, n-j-1,
                             alpha, A(0, j+1), lda,
                                    A(0, j),   1,
                             beta,  A(j, j+1), lda); // cublas is better in upper case

                #if defined(PRECISION_z) || defined(PRECISION_c)
                slacgv(j, A(0, j), 1);
                #endif
                spotf2_sscal(n-j, A(j,j), lda);
            }
        }
    }
    else {
        for(j = 0; j < n; j++) {
            spotf2_sdot(j, A(j,0), lda); // including sdot product and update a(j,j)
            if (j < n) {
                #if defined(PRECISION_z) || defined(PRECISION_c)
                slacgv(j, A(j, 0), lda);
                #endif
                magmablas_sgemv( MagmaNoTrans, n-j-1, j,
                                 alpha, A(j+1, 0), lda,
                                        A(j,0),    lda,
                                 beta,  A(j+1, j), 1 );// magmablas is better in lower case

                #if defined(PRECISION_z) || defined(PRECISION_c)
                slacgv(j, A(j, 0), lda);
                #endif
                spotf2_sscal(n-j, A(j,j), 1);
            }
        }
    }

    return *info;
}

#define sscal_bs  32
#define sdot_bs  512
#define slacgv_bs 512

// dynamically allocated shared memory, set to size number of threads when the kernel is launched.
// See CUDA Guide B.2.3
extern __shared__ float shared_data[];

__global__ void kernel_sdot(int n, float *x, int incx, int threadSize)
{
    int tx = threadIdx.x;

    float *sdata = shared_data;

    float res = MAGMA_S_ZERO;

    if (tx < n) {
       res = x[tx*incx];
    }

    sdata[tx] = MAGMA_S_REAL(res * MAGMA_S_CNJG(res));

    __syncthreads();

    for(int s = blockDim.x/2; s > 32; s >>= 1 ) {
        if (tx < s) {
            sdata[tx] += sdata[tx+s];
        }
        __syncthreads();
    }

    if (tx < 32) {
        volatile float* smem = sdata;
        smem[tx] += smem[tx+32];
        smem[tx] += smem[tx+16];
        smem[tx] += smem[tx+8];
        smem[tx] += smem[tx+4];
        smem[tx] += smem[tx+2];
        smem[tx] += smem[tx+1];
    }

    if (tx == 0) {
        float xreal = MAGMA_S_REAL(x[n*incx]);
        x[n*incx] = MAGMA_S_MAKE( sqrt(xreal - sdata[0]), 0 );
    }
}

void spotf2_sdot(magma_int_t n, float *x, magma_int_t incx)
{
/*
    Specialized Sdot
    1) performs sdot sum = x[0:n-1]*(x[0:n-1])
    2) updates x[n] = sqrt(x[n]-sum);

*/
    if (n > sdot_max_bs) {
        printf("n = %d > %d is not supported in spotf2_sdot\n", (int) n, (int) sdot_max_bs);
        exit(1);
    }
    int threadSize;

    if (n <= 1024 && n > 512) {
        threadSize = 1024;
    }
    else if (n <= 512 && n > 256 ) {
        threadSize = 512;
    }
    else if (n <= 256 && n > 128) {
        threadSize = 256;
    }
    else if (n <= 128 && n > 64) {
        threadSize = 128;
    }
    else {
        threadSize = 64;
    }

    kernel_sdot<<< 1, threadSize, threadSize * sizeof(float), magma_stream>>> (n, x, incx, threadSize);
}

__global__ void kernel_sscal(int n, float *x, int incx)
{
    int id = blockIdx.x * sscal_bs + threadIdx.x;

    __shared__ float factor;

    if (threadIdx.x == 0) {
        factor = MAGMA_S_MAKE(1.0/MAGMA_S_REAL(x[0]), 0.0);
    }

    __syncthreads();

    if ( id < n && id >0) {
        x[id*incx] = x[id*incx] * factor;
    }
}


void spotf2_sscal(magma_int_t n, float *x, magma_int_t incx)
{
/*
    Specialized Zsscal perform x[1:n-1]/x[0]

*/
    dim3 threads(sscal_bs, 1, 1);
    int num_blocks = (n - 1)/sscal_bs + 1;
    dim3 grid(num_blocks,1);
    kernel_sscal<<< grid, threads, 0, magma_stream >>> (n, x, incx);
}


#if defined(PRECISION_z) || defined(PRECISION_c)

__global__ void kernel_slacgv(int n, float *x, int incx)
{
    int id = blockIdx.x * slacgv_bs + threadIdx.x;

    if ( id < n ) {
        x[id*incx] = MAGMA_S_CNJG(x[id*incx]);
    }
}


/**
    Purpose
    -------

    SLACGV conjugates a real vector of length N.

    Arguments
    ---------

    @param[in]
    n       INTEGER
            The length of the vector X.  N >= 0.

    @param[in,out]
    x       REAL array, dimension
                           (1+(N-1)*abs(INCX))
            On entry, the vector of length N to be conjugated.
            On exit, X is overwritten with conjg(X).

    @param[in]
    incx    INTEGER
            The spacing between successive elements of X.

    @ingroup magma_sposv_aux
    ********************************************************************/
void slacgv(magma_int_t n, float *x, magma_int_t incx)
{
    dim3 threads(slacgv_bs, 1, 1);
    int num_blocks = (n - 1)/slacgv_bs + 1;
    dim3 grid(num_blocks,1);
    kernel_slacgv<<< grid, threads, 0, magma_stream >>> (n, x, incx);
}

#endif // defined(PRECISION_z) || defined(PRECISION_c)
