/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014
 
       @author Mark Gates
       @generated from zset_get.cpp normal z -> d, Fri Jul 18 17:34:21 2014
*/

#include <stdlib.h>
#include <stdio.h>

#include "magma.h"
#include "error.h"

#ifdef HAVE_CUBLAS

// ========================================
// copying vectors
extern "C"
void magma_dsetvector_internal(
    magma_int_t n,
    double const* hx_src, magma_int_t incx,
    double*       dy_dst, magma_int_t incy,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasSetVector(
        n, sizeof(double),
        hx_src, incx,
        dy_dst, incy );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dgetvector_internal(
    magma_int_t n,
    double const* dx_src, magma_int_t incx,
    double*       hy_dst, magma_int_t incy,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasGetVector(
        n, sizeof(double),
        dx_src, incx,
        hy_dst, incy );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dsetvector_async_internal(
    magma_int_t n,
    double const* hx_src, magma_int_t incx,
    double*       dy_dst, magma_int_t incy,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasSetVectorAsync(
        n, sizeof(double),
        hx_src, incx,
        dy_dst, incy, stream );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dgetvector_async_internal(
    magma_int_t n,
    double const* dx_src, magma_int_t incx,
    double*       hy_dst, magma_int_t incy,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasGetVectorAsync(
        n, sizeof(double),
        dx_src, incx,
        hy_dst, incy, stream );
    check_xerror( status, func, file, line );
}

// --------------------
// TODO compare performance with cublasDcopy BLAS function.
// But this implementation can handle any element size, not just [sdcz] precisions.
extern "C"
void magma_dcopyvector_internal(
    magma_int_t n,
    double const* dx_src, magma_int_t incx,
    double*       dy_dst, magma_int_t incy,
    const char* func, const char* file, int line )
{
    if ( incx == 1 && incy == 1 ) {
        cudaError_t status;
        status = cudaMemcpy(
            dy_dst,
            dx_src,
            n*sizeof(double), cudaMemcpyDeviceToDevice );
        check_xerror( status, func, file, line );
    }
    else {
        magma_dcopymatrix_internal(
            1, n, dx_src, incx, dy_dst, incy, func, file, line );
    }
}

// --------------------
extern "C"
void magma_dcopyvector_async_internal(
    magma_int_t n,
    double const* dx_src, magma_int_t incx,
    double*       dy_dst, magma_int_t incy,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    if ( incx == 1 && incy == 1 ) {
        cudaError_t status;
        status = cudaMemcpyAsync(
            dy_dst,
            dx_src,
            n*sizeof(double), cudaMemcpyDeviceToDevice, stream );
        check_xerror( status, func, file, line );
    }
    else {
        magma_dcopymatrix_async_internal(
            1, n, dx_src, incx, dy_dst, incy, stream, func, file, line );
    }
}


// ========================================
// copying sub-matrices (contiguous columns)
extern "C"
void magma_dsetmatrix_internal(
    magma_int_t m, magma_int_t n,
    double const* hA_src, magma_int_t lda,
    double*       dB_dst, magma_int_t ldb,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasSetMatrix(
        m, n, sizeof(double),
        hA_src, lda,
        dB_dst, ldb );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dgetmatrix_internal(
    magma_int_t m, magma_int_t n,
    double const* dA_src, magma_int_t lda,
    double*       hB_dst, magma_int_t ldb,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasGetMatrix(
        m, n, sizeof(double),
        dA_src, lda,
        hB_dst, ldb );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dsetmatrix_async_internal(
    magma_int_t m, magma_int_t n,
    double const* hA_src, magma_int_t lda,
    double*       dB_dst, magma_int_t ldb,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasSetMatrixAsync(
        m, n, sizeof(double),
        hA_src, lda,
        dB_dst, ldb, stream );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dgetmatrix_async_internal(
    magma_int_t m, magma_int_t n,
    double const* dA_src, magma_int_t lda,
    double*       hB_dst, magma_int_t ldb,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    cublasStatus_t status;
    status = cublasGetMatrixAsync(
        m, n, sizeof(double),
        dA_src, lda,
        hB_dst, ldb, stream );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dcopymatrix_internal(
    magma_int_t m, magma_int_t n,
    double const* dA_src, magma_int_t lda,
    double*       dB_dst, magma_int_t ldb,
    const char* func, const char* file, int line )
{
    cudaError_t status;
    status = cudaMemcpy2D(
        dB_dst, ldb*sizeof(double),
        dA_src, lda*sizeof(double),
        m*sizeof(double), n, cudaMemcpyDeviceToDevice );
    check_xerror( status, func, file, line );
}

// --------------------
extern "C"
void magma_dcopymatrix_async_internal(
    magma_int_t m, magma_int_t n,
    double const* dA_src, magma_int_t lda,
    double*       dB_dst, magma_int_t ldb,
    cudaStream_t stream,
    const char* func, const char* file, int line )
{
    cudaError_t status;
    status = cudaMemcpy2DAsync(
        dB_dst, ldb*sizeof(double),
        dA_src, lda*sizeof(double),
        m*sizeof(double), n, cudaMemcpyDeviceToDevice, stream );
    check_xerror( status, func, file, line );
}

#endif // HAVE_CUBLAS
