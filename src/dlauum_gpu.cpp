/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zlauum_gpu.cpp normal z -> d, Fri Jul 18 17:34:15 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    DLAUUM computes the product U * U' or L' * L, where the triangular
    factor U or L is stored in the upper or lower triangular part of
    the array dA.

    If UPLO = MagmaUpper then the upper triangle of the result is stored,
    overwriting the factor U in dA.
    If UPLO = MagmaLower then the lower triangle of the result is stored,
    overwriting the factor L in dA.
    This is the blocked form of the algorithm, calling Level 3 BLAS.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies whether the triangular factor stored in the array dA
            is upper or lower triangular:
      -     = MagmaUpper:  Upper triangular
      -     = MagmaLower:  Lower triangular

    @param[in]
    n       INTEGER
            The order of the triangular factor U or L.  N >= 0.

    @param[in,out]
    dA      DOUBLE PRECISION array on the GPU, dimension (LDDA,N)
            On entry, the triangular factor U or L.
            On exit, if UPLO = MagmaUpper, the upper triangle of dA is
            overwritten with the upper triangle of the product U * U';
            if UPLO = MagmaLower, the lower triangle of dA is overwritten with
            the lower triangle of the product L' * L.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0: successful exit
      -     < 0: if INFO = -k, the k-th argument had an illegal value

    @ingroup magma_dposv_aux
    ***************************************************************************/
extern "C" magma_int_t
magma_dlauum_gpu(magma_uplo_t uplo, magma_int_t n,
                 double  *dA, magma_int_t ldda, magma_int_t *info)
{
#define dA(i, j) (dA + (j)*ldda + (i))

    /* Local variables */
    const char* uplo_ = lapack_uplo_const( uplo );
    magma_int_t         nb, i, ib;
    double              d_one = MAGMA_D_ONE;
    double  c_one = MAGMA_D_ONE;
    double  *work;

    int upper  = (uplo == MagmaUpper);

    *info = 0;

    if (! upper && uplo != MagmaLower)
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (ldda < max(1,n))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    nb = magma_get_dpotrf_nb(n);

    if (MAGMA_SUCCESS != magma_dmalloc_pinned( &work, nb*nb )) {
        *info = MAGMA_ERR_HOST_ALLOC;
        return *info;
    }

    magma_queue_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );

    if (nb <= 1 || nb >= n) {
        magma_dgetmatrix( n, n, dA, ldda, work, n );
        lapackf77_dlauum(uplo_, &n, work, &n, info);
        magma_dsetmatrix( n, n, work, n, dA, ldda );
    }
    else {
        if (upper) {
            /* Compute inverse of upper triangular matrix */
            for (i=0; i < n; i += nb) {
                ib = min(nb, (n-i));

                /* Compute the product U * U'. */
                magma_dtrmm( MagmaRight, MagmaUpper,
                             MagmaTrans, MagmaNonUnit, i, ib,
                             c_one, dA(i,i), ldda, dA(0, i),ldda);

                magma_dgetmatrix( ib, ib,
                                  dA(i, i), ldda,
                                  work,     ib );

                lapackf77_dlauum(MagmaUpperStr, &ib, work, &ib, info);

                magma_dsetmatrix( ib, ib,
                                  work,     ib,
                                  dA(i, i), ldda );

                if (i+ib < n) {
                    magma_dgemm( MagmaNoTrans, MagmaTrans,
                                 i, ib, (n-i-ib), c_one, dA(0,i+ib),
                                 ldda, dA(i, i+ib), ldda, c_one,
                                 dA(0,i), ldda);

                    magma_dsyrk( MagmaUpper, MagmaNoTrans, ib,(n-i-ib),
                                 d_one, dA(i, i+ib), ldda,
                                 d_one, dA(i, i),    ldda);
                }
            }
        }
        else {
            /* Compute the product L' * L. */
            for (i=0; i < n; i += nb) {
                ib=min(nb,(n-i));

                magma_dtrmm( MagmaLeft, MagmaLower,
                             MagmaTrans, MagmaNonUnit, ib,
                             i, c_one, dA(i,i), ldda,
                             dA(i, 0),ldda);

                magma_dgetmatrix( ib, ib,
                                  dA(i, i), ldda,
                                  work,     ib );

                lapackf77_dlauum(MagmaLowerStr, &ib, work, &ib, info);

                magma_dsetmatrix( ib, ib,
                                  work,     ib,
                                  dA(i, i), ldda );

                if (i+ib < n) {
                    magma_dgemm( MagmaTrans, MagmaNoTrans,
                                 ib, i, (n-i-ib), c_one, dA( i+ib,i),
                                 ldda, dA(i+ib, 0),ldda, c_one,
                                 dA(i,0), ldda);
                    magma_dsyrk( MagmaLower, MagmaTrans, ib, (n-i-ib),
                                 d_one, dA(i+ib, i), ldda,
                                 d_one, dA(i, i),    ldda);
                }
            }
        }
    }

    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );

    magma_free_pinned( work );

    return *info;
}
