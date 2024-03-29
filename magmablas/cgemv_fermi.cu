/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgemv_fermi.cu normal z -> c, Fri Jul 18 17:34:13 2014
*/
#include "common_magma.h"
#include "commonblas_c.h"

#define PRECISION_c

#define num_threads 128
#define gemv_bs      32
#define threadSize  128

__global__ void
cgemvn_kernel1_fermi(
    int m, int n, int n1, magmaFloatComplex alpha,
    const magmaFloatComplex * __restrict__ A, int lda,
    const magmaFloatComplex * __restrict__ x, magmaFloatComplex beta,
    magmaFloatComplex       * __restrict__ y)
{
#if (__CUDA_ARCH__ >= 200)
    int ind = blockIdx.x*num_threads + threadIdx.x;
    
    A += ind;
    
    magmaFloatComplex res = MAGMA_C_ZERO;
    
    for( int i=0; i < n1; i += gemv_bs ) {
        #pragma unroll
        for(int j=0; j < gemv_bs; j++) {
            res += A[0] * x[j];
            A   += lda;
        }
        x += gemv_bs;
    }
    
    if ( n > n1 ) {
        for(int j=0; j < (n-n1); j++) {
            res += A[0] * x[j];
            A   += lda;
        }
    }
    
    if ( ind < m )
        y[ind] = alpha * res + beta * y[ind];
#endif /* (__CUDA_ARCH__ >= 200) */
}

__global__ void
cgemvn_kernel2_fermi(
    int m, int n, int n1, magmaFloatComplex alpha,
    const magmaFloatComplex * __restrict__ A, int lda,
    const magmaFloatComplex * __restrict__ x, magmaFloatComplex beta,
    magmaFloatComplex       * __restrict__ y)
{
#if (__CUDA_ARCH__ >= 200)
    int ind = blockIdx.x*num_threads + threadIdx.x;
    
    A += ind;
    x += threadIdx.x;
    
    magmaFloatComplex res = MAGMA_C_ZERO;
    
    __shared__ magmaFloatComplex buff[num_threads];
    for( int i=0; i < n1; i += num_threads ) {
        __syncthreads();
        buff[threadIdx.x] = x[i];
        
        __syncthreads();
        #pragma unroll
        for(int j=0; j < num_threads; j++) {
            res += A[0]*buff[j];
            A += lda;
        }
    }
    __syncthreads();
    
    if ( n > n1 ) {
        buff[threadIdx.x] = x[n1];
        
        __syncthreads();
        for(int j=0; j<(n-n1); j++) {
            res += A[0]*buff[j];
            A += lda;
        }
    }
    
    if ( ind < m )
        y[ind] = alpha * res + beta * y[ind];
#endif /* (__CUDA_ARCH__ >= 200) */
}

/**
    Purpose
    -------

    This routine computes Y = alpha A x + beta y, on the GPU.

    @param[in]
    m       INTEGER.
            On entry, M specifies the number of rows of the matrix A.

    @param[in]
    n       INTEGER.
            On entry, N specifies the number of columns of the matrix A

    @param[in]
    alpha   COMPLEX.
            On entry, ALPHA specifies the scalar alpha.

    @param[in]
    A       COMPLEX array of dimension ( LDA, n ) on the GPU.
   
    @param[in]
    lda     INTEGER.
            LDA specifies the leading dimension of A.

    @param[in]
    x       COMPLEX array of dimension n.

    @param[in]
    beta    REAL.
            On entry, BETA specifies the scalar beta.

    @param[out]
    y       COMPLEX array of dimension n.
            On exit Y = alpha A X + beta Y.

    @ingroup magma_cblas2_internal
    ********************************************************************/
extern "C" void
magmablas_cgemvn_fermi(
    magma_int_t m, magma_int_t n, magmaFloatComplex alpha,
    const magmaFloatComplex *A, magma_int_t lda,
    const magmaFloatComplex *x, magmaFloatComplex beta,
    magmaFloatComplex       *y)
{
    magma_int_t blocks = (m - 1)/num_threads + 1;
    dim3 grid(blocks, 1, 1);
    dim3 threads(num_threads, 1, 1);
    /*
    if ( m <= 8500 )
        cgemvn_kernel1_fermi<<< grid, threads, 0, magma_stream >>>
            (m, n, (n / gemv_bs)*gemv_bs, alpha, A, lda, x, y);
    else
    */
        cgemvn_kernel2_fermi<<< grid, threads, 0, magma_stream >>>
            (m, n, (n / num_threads)*num_threads, alpha, A, lda, x, beta, y);
}

__global__ void
cgemvt_kernel_fermi(
    int m, int n, magmaFloatComplex alpha, int n1,
    const magmaFloatComplex * __restrict__ A, int lda,
    const magmaFloatComplex * __restrict__ x, magmaFloatComplex beta,
    magmaFloatComplex       * __restrict__ y)
{
#if (__CUDA_ARCH__ >= 200)
    unsigned int tx = threadIdx.x;

    __shared__ magmaFloatComplex sdata[threadSize];

    magmaFloatComplex res = MAGMA_C_ZERO;
    magmaFloatComplex c_zero = MAGMA_C_ZERO;
 
    for(int i=0; i < n1; i += threadSize) {
        res += A[tx + i + lda * blockIdx.y] * x[tx + i];
    }

    if ( m > n1 ) {
        if ( tx + n1 < m ) {
            res += A[tx + n1 + lda*blockIdx.y] * x[tx + n1];
        }
        else {
            res += c_zero;
        }
    }

    sdata[tx] = res;
    __syncthreads();

    for(int s=blockDim.x/2; s > 32; s /= 2) {
        if ( tx < s ) {
            sdata[tx] += sdata[tx+s];
        }
        __syncthreads();
    }

    if ( tx < 32 ) {
        sdata[tx] += sdata[tx + 32];
    }

    if ( tx == 0 ) {
        for(int i=1; i < 32; i++) {
            sdata[tx] += sdata[tx + i];
        }
    }

    if ( tx == 0 ) {
        if ( blockIdx.y < n ) {
            y[blockIdx.y] = sdata[0] * alpha + beta * y[blockIdx.y];
        }
    }
#endif /* (__CUDA_ARCH__ >= 200) */
}


/**
    Purpose
    -------

    This routine computes y = alpha * A^T * x + beta*y, on the GPU.

    @param[in]
    m       INTEGER.
            On entry, M specifies the number of rows of the matrix A.

    @param[in]
    n       INTEGER.
            On entry, N specifies the number of columns of the matrix A

    @param[in]
    alpha   COMPLEX.
            On entry, ALPHA specifies the scalar alpha.

    @param[in]
    A       COMPLEX array of dimension ( LDA, n ) on the GPU.

    @param[in]
    lda     INTEGER.
            LDA specifies the leading dimension of A.

    @param[in]
    x       COMPLEX array of dimension m.

    @param[in]
    beta    COMPLEX.
            On entry, BETA specifies the scalar beta.

    @param[out]
    y       COMPLEX array of dimension n.
            On exit Y = alpha A^T X + beta Y.

    @ingroup magma_cblas2_internal
    ********************************************************************/
extern "C" void
magmablas_cgemvt_fermi(
    magma_int_t m, magma_int_t n, magmaFloatComplex alpha,
    const magmaFloatComplex *A, magma_int_t lda,
    const magmaFloatComplex *x, magmaFloatComplex beta,
    magmaFloatComplex       *y)
{
    dim3 grid    ( 1, n, 1 );
    dim3 threads ( threadSize, 1, 1 );
    cgemvt_kernel_fermi<<< grid, threads, 0, magma_stream >>>
        (m, n, alpha, (m / threadSize) * threadSize, A, lda, x, beta, y );
}

__global__ void
cgemvc_kernel_fermi(
    int m, int n, magmaFloatComplex alpha, int n1,
    const magmaFloatComplex * __restrict__ A, int lda,
    const magmaFloatComplex * __restrict__ x, magmaFloatComplex beta,
    magmaFloatComplex       * __restrict__ y)
{
#if (__CUDA_ARCH__ >= 200)
    unsigned int tx = threadIdx.x;

    __shared__ magmaFloatComplex sdata[threadSize];

    magmaFloatComplex res = MAGMA_C_ZERO;
    magmaFloatComplex c_zero = MAGMA_C_ZERO;
 
    for(int i=0; i < n1; i += threadSize) {
        res += cuConjf(A[tx + i + lda * blockIdx.y]) * x[tx + i];
    }

    if ( m > n1 ) {
        if ( tx + n1 < m ) {
            res += cuConjf(A[tx + n1 + lda*blockIdx.y]) * x[tx + n1];
        }
        else {
            res += c_zero;
        }
    }

    sdata[tx] = res;
    __syncthreads();

    /*
    if ( tx < 128 ) {
        sdata[tx] += sdata[tx + 128];
    }
    __syncthreads();
    */

    if ( tx < 64 ) {
        sdata[tx] += sdata[tx + 64];
    }
    __syncthreads();

    if ( tx < 32 ) {
        sdata[tx] += sdata[tx + 32];
    }

    if ( tx == 0 ) {
        for(int i=1; i < 32; i++) {
            sdata[tx] += sdata[tx + i];
        }
    }

    if ( tx == 0 ) {
        if ( blockIdx.y < n ) {
            y[blockIdx.y] = sdata[0] * alpha + beta * y[blockIdx.y];
        }
    }
#endif /* (__CUDA_ARCH__ >= 200) */
}


/**
    Purpose
    -------

    This routine computes y = alpha * A^H * x + beta*y, on the GPU.

    @param[in]
    m       INTEGER.
            On entry, M specifies the number of rows of the matrix A.

    @param[in]
    n       INTEGER.
            On entry, N specifies the number of columns of the matrix A

    @param[in]
    alpha   COMPLEX.
            On entry, ALPHA specifies the scalar alpha.

    @param[in]
    A       COMPLEX array of dimension ( LDA, n ) on the GPU.

    @param[in]
    lda     INTEGER.
            LDA specifies the leading dimension of A.

    @param[in]
    x       COMPLEX array of dimension m.

    @param[in]
    beta    COMPLEX.
            On entry, BETA specifies the scalar beta.

    @param[out]
    y       COMPLEX array of dimension n.
            On exit Y = alpha A^H X + beta y.

    @ingroup magma_cblas2_internal
    ********************************************************************/
extern "C" void
magmablas_cgemvc_fermi(
    magma_int_t m, magma_int_t n, magmaFloatComplex alpha,
    const magmaFloatComplex *A, magma_int_t lda,
    const magmaFloatComplex *x,
    magmaFloatComplex beta,
    magmaFloatComplex *y)
{
    dim3 grid    ( 1, n, 1 );
    dim3 threads ( threadSize, 1, 1 );
    cgemvc_kernel_fermi<<< grid, threads, 0, magma_stream >>>
        (m, n, alpha, (m / threadSize) * threadSize, A, lda, x, beta, y);
}


/**
    Purpose
    -------
    CGEMV performs one of the matrix-vector operations
    
        y := alpha*A*x    + beta*y,   or
        y := alpha*A**T*x + beta*y,   or
        y := alpha*A**H*x + beta*y,
    
    where alpha and beta are scalars, x and y are vectors and A is an
    m by n matrix.

    Arguments
    ----------
    @param[in]
    trans   magma_trans_t
            On entry, TRANS specifies the operation to be performed as
            follows:
      -     = MagmaNoTrans:    y := alpha*A  *x + beta*y
      -     = MagmaTrans:      y := alpha*A^T*x + beta*y
      -     = MagmaConjTrans:  y := alpha*A^H*x + beta*y

    @param[in]
    m       INTEGER
            On entry, m specifies the number of rows of the matrix A.

    @param[in]
    n       INTEGER
            On entry, n specifies the number of columns of the matrix A
 
    @param[in]
    alpha   COMPLEX
            On entry, ALPHA specifies the scalar alpha.

    @param[in]
    A       COMPLEX array of dimension ( LDA, n ) on the GPU.
   
    @param[in]
    lda     INTEGER
            LDA specifies the leading dimension of A.

    @param[in]
    x       COMPLEX array of dimension
            n if trans == MagmaNoTrans
            m if trans == MagmaTrans or MagmaConjTrans
     
    @param[in]
    incx    Specifies the increment for the elements of X.
            INCX must not be zero.
  
    @param[in]
    beta    DOUBLE REAL
            On entry, BETA specifies the scalar beta. When BETA is
            supplied as zero then Y need not be set on input.

    @param[out]
    y       REAL array of dimension
            m if trans == MagmaNoTrans
            n if trans == MagmaTrans or MagmaConjTrans

    @param[in]
    incy    Specifies the increment for the elements of Y.
            INCY must not be zero.

    @ingroup magma_dblas2
    ********************************************************************/
extern "C" void
magmablas_cgemv(
    magma_trans_t trans, magma_int_t m, magma_int_t n, magmaFloatComplex alpha,
    const magmaFloatComplex *A, magma_int_t lda,
    const magmaFloatComplex *x, magma_int_t incx,
    magmaFloatComplex beta,
    magmaFloatComplex *y, magma_int_t incy)
{
    magma_int_t info = 0;
    if ( trans != MagmaNoTrans && trans != MagmaTrans && trans != MagmaConjTrans )
        info = -1;
    else if ( m < 0 )
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( lda < m )
        info = -6;
    else if ( incx == 0 )
        info = -8;
    else if ( incy == 0 )
        info = -11;
    
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }
    
    magma_int_t arch = magma_getdevice_arch();
    if ( arch < 200  ) {
        // --------------------
        // call CUDA ARCH 1.x version
        // magmablas for [sd] precisions, cublas for [zc] precisions.
        #if defined(PRECISION_z) || defined(PRECISION_c)
        magma_cgemv( trans, m, n, alpha, A, lda, x, incx, beta, y, incy );
        #else
        magmablas_cgemv_tesla( trans, m, n, alpha, A, lda, x, incx, beta, y, incy );
        #endif
        return;
    }
    
    // --------------------
    // CUDA ARCH 2.x (Fermi) version
    if ( incx == 1 && incy == 1 ) {
        if ( trans == MagmaNoTrans ) {
            if ( m < 7000 ) {
                magma_cgemv( trans, m, n, alpha, A, lda, x, incx, beta, y, incy);
            }
            else {
                magmablas_cgemvn_fermi(m, n, alpha, A, lda, x, beta, y);
            }
        }
        else if ( trans == MagmaTrans ) {
            magmablas_cgemvt_fermi(m, n, alpha, A, lda, x, beta, y);
        }
        else if ( trans == MagmaConjTrans ) {
            magmablas_cgemvc_fermi(m, n, alpha, A, lda, x, beta, y);
        }
        else {
            fprintf( stderr, "trans = %c is invalid\n", lapacke_trans_const(trans) );
        }
    }
    else {
        magma_cgemv( trans, m, n, alpha, A, lda, x, incx, beta, y, incy);
    }
}

#undef num_threads
#undef gemv_bs
#undef threadSize
