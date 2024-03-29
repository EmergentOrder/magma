/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zpotrf_mgpu.cpp normal z -> s, Fri Jul 18 17:34:14 2014

*/
#include "common_magma.h"


/**
    Purpose
    -------
    SPOTRF computes the Cholesky factorization of a real symmetric
    positive definite matrix dA.

    The factorization has the form
       dA = U**T * U,   if UPLO = MagmaUpper, or
       dA = L  * L**T,  if UPLO = MagmaLower,
    where U is an upper triangular matrix and L is lower triangular.

    This is the block version of the algorithm, calling Level 3 BLAS.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of dA is stored;
      -     = MagmaLower:  Lower triangle of dA is stored.

    @param[in]
    n       INTEGER
            The order of the matrix dA.  N >= 0.

    @param[in,out]
    dA      REAL array on the GPU, dimension (LDDA,N)
            On entry, the symmetric matrix dA.  If UPLO = MagmaUpper, the leading
            N-by-N upper triangular part of dA contains the upper
            triangular part of the matrix dA, and the strictly lower
            triangular part of dA is not referenced.  If UPLO = MagmaLower, the
            leading N-by-N lower triangular part of dA contains the lower
            triangular part of the matrix dA, and the strictly upper
            triangular part of dA is not referenced.
    \n
            On exit, if INFO = 0, the factor U or L from the Cholesky
            factorization dA = U**T * U or dA = L * L**T.

    @param[in]
    ldda     INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,N).
            To benefit from coalescent memory accesses LDDA must be
            divisible by 16.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, the leading minor of order i is not
                  positive definite, and the factorization could not be
                  completed.

    @ingroup magma_sposv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_spotrf_mgpu(magma_int_t num_gpus, magma_uplo_t uplo, magma_int_t n,
                  float **d_lA, magma_int_t ldda, magma_int_t *info)
{
    magma_int_t     j, nb, d, lddp, h;
    const char* uplo_ = lapack_uplo_const( uplo );
    float *work;
    int upper = (uplo == MagmaUpper);
    float *dwork[MagmaMaxGPUs];
    magma_queue_t    stream[MagmaMaxGPUs][3];
    magma_event_t     event[MagmaMaxGPUs][5];

    *info = 0;
    nb = magma_get_spotrf_nb(n);
    if (! upper && uplo != MagmaLower) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (!upper) {
        lddp = nb*(n/(nb*num_gpus));
        if ( n%(nb*num_gpus) != 0 ) lddp += min(nb,n-num_gpus*lddp);
        if ( ldda < lddp ) *info = -4;
    } else if ( ldda < n ) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    if (num_gpus == 1 && ((nb <= 1) || (nb >= n)) ) {
        /*  Use unblocked code. */
        magma_setdevice(0);
        if (MAGMA_SUCCESS != magma_smalloc_pinned( &work, n*nb )) {
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        magma_sgetmatrix( n, n, d_lA[0], ldda, work, n );
        lapackf77_spotrf(uplo_, &n, work, &n, info);
        magma_ssetmatrix( n, n, work, n, d_lA[0], ldda );
        magma_free_pinned( work );
    }
    else {
        lddp = nb*((n+nb-1)/nb);
        for( d=0; d < num_gpus; d++ ) {
            magma_setdevice(d);
            if (MAGMA_SUCCESS != magma_smalloc( &dwork[d], num_gpus*nb*lddp )) {
                for( j=0; j < d; j++ ) {
                    magma_setdevice(j);
                    magma_free( dwork[j] );
                }
                *info = MAGMA_ERR_DEVICE_ALLOC;
                return *info;
            }
            for( j=0; j < 3; j++ )
                magma_queue_create( &stream[d][j] );
            for( j=0; j < 5; j++ )
                magma_event_create( &event[d][j]  );
        }
        magma_setdevice(0);
        h = 1; //num_gpus; //(n+nb-1)/nb;
        if (MAGMA_SUCCESS != magma_smalloc_pinned( &work, n*nb*h )) {
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        if (upper) {
            /* with two streams */
            //magma_spotrf2_mgpu(num_gpus, uplo, n, n, 0, 0, nb, d_lA, ldda, dwork, lddp, work, n,
            //                   h, stream, event, info);
            /* with three streams */
            magma_spotrf3_mgpu(num_gpus, uplo, n, n, 0, 0, nb, d_lA, ldda, dwork, lddp, work, n,
                               h, stream, event, info);
        } else {
            /* with two streams */
            //magma_spotrf2_mgpu(num_gpus, uplo, n, n, 0, 0, nb, d_lA, ldda, dwork, lddp, work, nb*h,
            //                   h, stream, event, info);
            /* with three streams */
            magma_spotrf3_mgpu(num_gpus, uplo, n, n, 0, 0, nb, d_lA, ldda, dwork, lddp, work, nb*h,
                               h, stream, event, info);
        }

        /* clean up */
        for( d=0; d < num_gpus; d++ ) {
            magma_setdevice(d);
            for( j=0; j < 3; j++ ) {
                magma_queue_sync( stream[d][j] );
                magma_queue_destroy( stream[d][j] );
            }
            magmablasSetKernelStream(NULL);
            
            for( j=0; j < 5; j++ )
                magma_event_destroy( event[d][j] );
            
            magma_free( dwork[d] );
        }
        magma_setdevice(0);
        magma_free_pinned( work );
    } /* end of not lapack */

    return *info;
} /* magma_spotrf_mgpu */
