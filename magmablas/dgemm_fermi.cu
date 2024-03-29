/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgemm_fermi.cu normal z -> d, Fri Jul 18 17:34:13 2014

       @author Jakub Kurzak
       @author Stan Tomov
       @author Mark Gates

       [zcds]gemm_fermi.cu          defines the CPU driver.
       [zcds]gemm_fermi_kernels.h   defines the block sizes for each precision.
       gemm_stencil_defs.h          defines types and functions for precision-independent code.
       gemm_stencil.cu              defines the GPU kernel. It gets included
                                    multiple times, once for each transpose version.
*/
#include "common_magma.h"
#include "commonblas_d.h"
#include <assert.h>

#define PRECISION_d

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "dgemm_fermi_kernels.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
    Purpose
    -------
    DGEMM performs one of the matrix-matrix operations
    
        C = alpha*op( A )*op( B ) + beta*C,
    
    where op( X ) is one of
    
        op( X ) = X   or   op( X ) = X**T   or   op( X ) = X**T,
    
    alpha and beta are scalars, and A, B and C are matrices, with
    op( A ) an m by k matrix, op( B ) a k by n matrix and C an m by n matrix.
    
    Parameters
    ----------
    @param[in]
    TRANSA  CHARACTER*1.
            On entry, TRANSA specifies the form of op( A ) to be used in
            the matrix multiplication as follows:
      -     = 'N':  op( A ) = A.
      -     = 'T':  op( A ) = A**T.
      -     = 'C':  op( A ) = A**T.
    
    @param[in]
    TRANSB  CHARACTER*1.
            On entry, TRANSB specifies the form of op( B ) to be used in
            the matrix multiplication as follows:
      -     = 'N':  op( B ) = B.
      -     = 'T':  op( B ) = B**T.
      -     = 'C':  op( B ) = B**T.
    
    @param[in]
    m       INTEGER.
            On entry,  M  specifies  the number  of rows  of the  matrix
            op( d_A )  and of the  matrix d_C.  M  must  be at least  zero.
    
    @param[in]
    n       INTEGER.
            On entry,  N  specifies the number  of columns of the matrix
            op( d_B ) and the number of columns of the matrix d_C. N must be
            at least zero.
    
    @param[in]
    k       INTEGER.
            On entry,  K  specifies  the number of columns of the matrix
            op( d_A ) and the number of rows of the matrix op( d_B ). K must
            be at least  zero.
    
    @param[in]
    alpha   DOUBLE_PRECISION
            On entry, ALPHA specifies the scalar alpha.
    
    @param[in]
    d_A     DOUBLE_PRECISION array of DIMENSION ( LDA, ka ), where ka is
            k  when  TRANSA = MagmaNoTrans,  and is  m  otherwise.
            Before entry with  TRANSA = MagmaNoTrans,  the leading  m by k
            part of the array d_A must contain the matrix d_A, otherwise
            the leading  k by m  part of the array d_A must contain  the
            matrix d_A.
    
    @param[in]
    lda     INTEGER.
            On entry, LDA specifies the first dimension of A as declared
            in the calling (sub) program. When  TRANSA = MagmaNoTrans then
            LDA must be at least  max( 1, m ), otherwise  LDA must be at
            least  max( 1, k ).
    
    @param[in]
    d_B     DOUBLE_PRECISION array of DIMENSION ( LDB, kb ), where kb is
            n  when  TRANSB = MagmaNoTrans,  and is  k  otherwise.
            Before entry with  TRANSB = MagmaNoTrans,  the leading  k by n
            part of the array d_B must contain the matrix d_B, otherwise
            the leading  n by k  part of the array d_B must contain  the
            matrix d_B.
    
    @param[in]
    ldb     INTEGER.
            On entry, LDB specifies the first dimension of d_B as declared
            in the calling (sub) program. When  TRANSB = MagmaNoTrans then
            LDB must be at least  max( 1, k ), otherwise  LDB must be at
            least  max( 1, n ).
    
    @param[in]
    beta    DOUBLE_PRECISION.
            On entry,  BETA  specifies the scalar  beta.  When  BETA  is
            supplied as zero then d_C need not be set on input.
    
    @param[in,out]
    d_C     DOUBLE_PRECISION array of DIMENSION ( LDC, n ).
            Before entry, the leading  m by n  part of the array  d_C must
            contain the matrix  d_C,  except when  beta  is zero, in which
            case d_C need not be set on entry.
            On exit, the array  d_C  is overwritten by the  m by n  matrix
            ( alpha*op( d_A )*op( d_B ) + beta*d_C ).
    
    @param[in]
    ldc     INTEGER.
            On entry, LDC specifies the first dimension of d_C as declared
            in  the  calling  (sub)  program.   LDC  must  be  at  least
            max( 1, m ).

    @ingroup magma_dblas3
    ********************************************************************/
extern "C" void
magmablas_dgemm(
    magma_trans_t TRANSA, magma_trans_t TRANSB, magma_int_t m, magma_int_t n, magma_int_t k,
    double alpha,
    const double *d_A, magma_int_t lda,
    const double *d_B, magma_int_t ldb,
    double beta,
    double *d_C, magma_int_t ldc )
{
    magma_int_t info = 0;
    if      ( TRANSA != MagmaNoTrans && TRANSA != MagmaTrans && TRANSA != Magma_ConjTrans )
        info = -1;
    else if ( TRANSB != MagmaNoTrans && TRANSB != MagmaTrans && TRANSB != Magma_ConjTrans )
        info = -2;
    else if ( m < 0 )
        info = -3;
    else if ( n < 0 )
        info = -4;
    else if ( k < 0 )
        info = -5;
    else if ( TRANSA == MagmaNoTrans ? lda < m : lda < k )
        info = -8;
    else if ( TRANSB == MagmaNoTrans ? ldb < k : ldb < n )
        info = -10;
    else if ( ldc < m )
        info = -13;
    
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
        magma_dgemm(
            TRANSA, TRANSB,
            m, n, k, alpha, d_A, lda, d_B, ldb, beta, d_C, ldc );
        #else
        magmablas_dgemm_tesla(
            TRANSA, TRANSB, m, n, k, alpha, d_A, lda, d_B, ldb, beta, d_C, ldc );
        #endif
        return;
    }
    
    // --------------------
    // CUDA ARCH 2.x (Fermi) version
    if ( m <= 0 || n <= 0 || k <= 0 )
        return;
    
    size_t offsetA = 0;
    size_t offsetB = 0;

    int TransA = 2, TransB = 2;
    if      ( TRANSA == MagmaTrans )
        TransA = 1;
    else if ( TRANSA == MagmaNoTrans )
        TransA = 0;
                    
    if      ( TRANSB == MagmaTrans )
        TransB = 1;
    else if ( TRANSB == MagmaNoTrans )
        TransB = 0;

    size_t sizeA = (size_t) lda * (size_t) (!TransA ? k : m);
    size_t sizeB = (size_t) ldb * (size_t) (!TransB ? n : k);

    size_t CUBLAS_MAX_1DBUF_SIZE = ((1 << 27) - 512);
    if ( sizeA >= CUBLAS_MAX_1DBUF_SIZE ||
         sizeB >= CUBLAS_MAX_1DBUF_SIZE )
    {
        magma_dgemm( TRANSA, TRANSB, m, n, k, alpha,
                     d_A, lda, d_B, ldb,
                     beta, d_C, ldc );
        return;
    }

    #ifdef TEXTURE_1D
        // Set textures parameters
        tex_ref_A.normalized = false;
        tex_ref_A.filterMode = cudaFilterModePoint;
        tex_ref_A.addressMode[0] = cudaAddressModeClamp;

        tex_ref_B.normalized = false;
        tex_ref_B.filterMode = cudaFilterModePoint;
        tex_ref_B.addressMode[0] = cudaAddressModeClamp;

        // Bind A and B to texture references
        cudaError_t err;
        err = cudaBindTexture(&offsetA, tex_ref_A, d_A, sizeA*sizeof(double));
        if ( err != cudaSuccess ) {
            fprintf( stderr, "cannot bind A to texture: %s (%d)\n", cudaGetErrorString(err), err );
            return;
        }
        err = cudaBindTexture(&offsetB, tex_ref_B, d_B, sizeB*sizeof(double));
        if ( err != cudaSuccess ) {
            fprintf( stderr, "cannot bind B to texture: %s (%d)\n", cudaGetErrorString(err), err );
            cudaUnbindTexture( tex_ref_A );
            return;
        }
    #endif

    // Set up grids
    dim3 dimBlock(DIM_X, DIM_Y);

    offsetA = offsetA/sizeof(d_A[0]);
    offsetB = offsetB/sizeof(d_B[0]);
 
    if ( TransA == 0 && TransB == 0 ) {
        dim3 dimGrid( (m - 1)/BLK_M_nn + 1,
                      (n - 1)/BLK_N_nn + 1 );
        dgemm_kernel_fermi_nn<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 0 && TransB == 1 ) {
        dim3 dimGrid( (m - 1)/BLK_M_nt + 1,
                      (n - 1)/BLK_N_nt + 1 );
        dgemm_kernel_fermi_nt<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 0 && TransB == 2 ) {
        dim3 dimGrid( (m - 1)/BLK_M_nc + 1,
                      (n - 1)/BLK_N_nc + 1 );
        dgemm_kernel_fermi_nc<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 1 && TransB == 0 ) {
        dim3 dimGrid( (m - 1)/BLK_M_tn + 1,
                      (n - 1)/BLK_N_tn + 1 );
        dgemm_kernel_fermi_tn<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 1 && TransB == 1 ) {
        dim3 dimGrid( (m - 1)/BLK_M_tt + 1,
                      (n - 1)/BLK_N_tt + 1 );
        dgemm_kernel_fermi_tt<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 1 && TransB == 2 ) {
        dim3 dimGrid( (m - 1)/BLK_M_tc + 1,
                      (n - 1)/BLK_N_tc + 1 );
        dgemm_kernel_fermi_tc<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 2 && TransB == 0 ) {
        dim3 dimGrid( (m - 1)/BLK_M_cn + 1,
                      (n - 1)/BLK_N_cn + 1 );
        dgemm_kernel_fermi_cn<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 2 && TransB == 1 ) {
        dim3 dimGrid( (m - 1)/BLK_M_ct + 1,
                      (n - 1)/BLK_N_ct + 1 );
        dgemm_kernel_fermi_ct<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }
    else if ( TransA == 2 && TransB == 2 ) {
        dim3 dimGrid( (m - 1)/BLK_M_cc + 1,
                      (n - 1)/BLK_N_cc + 1 );
        dgemm_kernel_fermi_cc<<< dimGrid, dimBlock, 0, magma_stream >>>(
            m, n, k, d_A, lda, d_B, ldb, d_C, ldc, alpha, beta,
            (int)offsetA, (int)offsetB );
    }

    cudaUnbindTexture( tex_ref_A );
    cudaUnbindTexture( tex_ref_B );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
