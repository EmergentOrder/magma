/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zpotrs_gpu.cpp normal z -> s, Fri Jul 18 17:34:15 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    SPOTRS solves a system of linear equations A*X = B with a symmetric
    positive definite matrix A using the Cholesky factorization
    A = U**T*U or A = L*L**T computed by SPOTRF.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in]
    dA      REAL array on the GPU, dimension (LDDA,N)
            The triangular factor U or L from the Cholesky factorization
            A = U**T*U or A = L*L**T, as computed by SPOTRF.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[in,out]
    dB      REAL array on the GPU, dimension (LDDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    @param[in]
    lddb    INTEGER
            The leading dimension of the array B.  LDDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_sposv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_spotrs_gpu(magma_uplo_t uplo, magma_int_t n, magma_int_t nrhs,
                 float *dA, magma_int_t ldda,
                 float *dB, magma_int_t lddb, magma_int_t *info)
{
    float c_one = MAGMA_S_ONE;

    *info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower )
        *info = -1;
    if ( n < 0 )
        *info = -2;
    if ( nrhs < 0)
        *info = -3;
    if ( ldda < max(1, n) )
        *info = -5;
    if ( lddb < max(1, n) )
        *info = -7;
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if ( (n == 0) || (nrhs == 0) ) {
        return *info;
    }

    if ( uplo == MagmaUpper ) {
        if ( nrhs == 1) {
            magma_strsv(MagmaUpper, MagmaTrans, MagmaNonUnit, n, dA, ldda, dB, 1 );
            magma_strsv(MagmaUpper, MagmaNoTrans,   MagmaNonUnit, n, dA, ldda, dB, 1 );
        } else {
            magma_strsm(MagmaLeft, MagmaUpper, MagmaTrans, MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb);
            magma_strsm(MagmaLeft, MagmaUpper, MagmaNoTrans,   MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb);
        }
    }
    else {
        if ( nrhs == 1) {
            magma_strsv(MagmaLower, MagmaNoTrans,   MagmaNonUnit, n, dA, ldda, dB, 1 );
            magma_strsv(MagmaLower, MagmaTrans, MagmaNonUnit, n, dA, ldda, dB, 1 );
        } else {
            magma_strsm(MagmaLeft, MagmaLower, MagmaNoTrans,   MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb);
            magma_strsm(MagmaLeft, MagmaLower, MagmaTrans, MagmaNonUnit, n, nrhs, c_one, dA, ldda, dB, lddb);
        }
    }

    return *info;
}
