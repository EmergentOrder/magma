/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgetrs_gpu.cpp normal z -> d, Fri Jul 18 17:34:15 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    Solves a system of linear equations
      A * X = B,  A**T * X = B,  or  A**T * X = B
    with a general N-by-N matrix A using the LU factorization computed by DGETRF_GPU.

    Arguments
    ---------
    @param[in]
    trans   magma_trans_t
            Specifies the form of the system of equations:
      -     = MagmaNoTrans:    A    * X = B  (No transpose)
      -     = MagmaTrans:      A**T * X = B  (Transpose)
      -     = MagmaTrans:  A**T * X = B  (Conjugate transpose)

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in]
    dA      DOUBLE_PRECISION array on the GPU, dimension (LDA,N)
            The factors L and U from the factorization A = P*L*U as computed
            by DGETRF_GPU.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in]
    ipiv    INTEGER array, dimension (N)
            The pivot indices from DGETRF; for 1 <= i <= N, row i of the
            matrix was interchanged with row IPIV(i).

    @param[in,out]
    dB      DOUBLE_PRECISION array on the GPU, dimension (LDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    @param[in]
    lddb    INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_dgesv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_dgetrs_gpu(magma_trans_t trans, magma_int_t n, magma_int_t nrhs,
                 double *dA, magma_int_t ldda,
                 magma_int_t *ipiv,
                 double *dB, magma_int_t lddb,
                 magma_int_t *info)
{
    double c_one = MAGMA_D_ONE;
    double *work = NULL;
    int notran = (trans == MagmaNoTrans);
    magma_int_t i1, i2, inc;

    *info = 0;
    if ( (! notran) &&
         (trans != MagmaTrans) &&
         (trans != MagmaTrans) ) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (nrhs < 0) {
        *info = -3;
    } else if (ldda < max(1,n)) {
        *info = -5;
    } else if (lddb < max(1,n)) {
        *info = -8;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (n == 0 || nrhs == 0) {
        return *info;
    }

    magma_dmalloc_cpu( &work, n * nrhs );
    if ( work == NULL ) {
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }
      
    i1 = 1;
    i2 = n;
    if (notran) {
        inc = 1;

        /* Solve A * X = B. */
        magma_dgetmatrix( n, nrhs, dB, lddb, work, n );
        lapackf77_dlaswp(&nrhs, work, &n, &i1, &i2, ipiv, &inc);
        magma_dsetmatrix( n, nrhs, work, n, dB, lddb );

        if ( nrhs == 1) {
            magma_dtrsv(MagmaLower, MagmaNoTrans, MagmaUnit,    n, dA, ldda, dB, 1 );
            magma_dtrsv(MagmaUpper, MagmaNoTrans, MagmaNonUnit, n, dA, ldda, dB, 1 );
        } else {
            magma_dtrsm(MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,    n, nrhs, c_one, dA, ldda, dB, lddb );
            magma_dtrsm(MagmaLeft, MagmaUpper, MagmaNoTrans, MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb );
        }
    } else {
        inc = -1;

        /* Solve A**T * X = B  or  A**T * X = B. */
        if ( nrhs == 1) {
            magma_dtrsv(MagmaUpper, trans, MagmaNonUnit, n, dA, ldda, dB, 1 );
            magma_dtrsv(MagmaLower, trans, MagmaUnit,    n, dA, ldda, dB, 1 );
        } else {
            magma_dtrsm(MagmaLeft, MagmaUpper, trans, MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb );
            magma_dtrsm(MagmaLeft, MagmaLower, trans, MagmaUnit,    n, nrhs, c_one, dA, ldda, dB, lddb );
        }

        magma_dgetmatrix( n, nrhs, dB, lddb, work, n );
        lapackf77_dlaswp(&nrhs, work, &n, &i1, &i2, ipiv, &inc);
        magma_dsetmatrix( n, nrhs, work, n, dB, lddb );
    }
    magma_free_cpu(work);

    return *info;
}
