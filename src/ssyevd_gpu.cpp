/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Stan Tomov
       @author Raffaele Solca
       @author Mark Gates
       @author Azzam Haidar

       @generated from dsyevd_gpu.cpp normal d -> s, Fri Jul 18 17:34:18 2014

*/
#include "common_magma.h"
#include "timer.h"

/**
    Purpose
    -------
    SSYEVD_GPU computes all eigenvalues and, optionally, eigenvectors of
    a real symmetric matrix A.  If eigenvectors are desired, it uses a
    divide and conquer algorithm.

    The divide and conquer algorithm makes very mild assumptions about
    floating point arithmetic. It will work on machines with a guard
    digit in add/subtract, or on those binary machines without guard
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or
    Cray-2. It could conceivably fail on hexadecimal or decimal machines
    without guard digits, but we know of none.

    Arguments
    ---------
    @param[in]
    jobz    magma_vec_t
      -     = MagmaNoVec:  Compute eigenvalues only;
      -     = MagmaVec:    Compute eigenvalues and eigenvectors.

    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    dA      REAL array on the GPU,
            dimension (LDDA, N).
            On entry, the symmetric matrix A.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
            On exit, if JOBZ = MagmaVec, then if INFO = 0, A contains the
            orthonormal eigenvectors of the matrix A.
            If JOBZ = MagmaNoVec, then on exit the lower triangle (if UPLO=MagmaLower)
            or the upper triangle (if UPLO=MagmaUpper) of A, including the
            diagonal, is destroyed.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array DA.  LDDA >= max(1,N).

    @param[out]
    w       REAL array, dimension (N)
            If INFO = 0, the eigenvalues in ascending order.

    @param
    wA      (workspace) REAL array, dimension (LDWA, N)

    @param[in]
    ldwa    INTEGER
            The leading dimension of the array wA.  LDWA >= max(1,N).

    @param[out]
    work    (workspace) REAL array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.
            If N <= 1,                      LWORK >= 1.
            If JOBZ = MagmaNoVec and N > 1, LWORK >= 2*N + N*NB.
            If JOBZ = MagmaVec   and N > 1, LWORK >= max( 2*N + N*NB, 1 + 6*N + 2*N**2 ).
            NB can be obtained through magma_get_ssytrd_nb(N).
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal sizes of the WORK and IWORK
            arrays, returns these values as the first entries of the WORK
            and IWORK arrays, and no error message related to LWORK or
            LIWORK is issued by XERBLA.

    @param[out]
    iwork   (workspace) INTEGER array, dimension (MAX(1,LIWORK))
            On exit, if INFO = 0, IWORK[0] returns the optimal LIWORK.

    @param[in]
    liwork  INTEGER
            The dimension of the array IWORK.
            If N <= 1,                       LIWORK >= 1.
            If JOBZ = MagmaNoVec and N > 1, LIWORK >= 1.
            If JOBZ  = MagmaVec   and N > 1, LIWORK >= 3 + 5*N.
    \n
            If LIWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK and
            IWORK arrays, returns these values as the first entries of
            the WORK and IWORK arrays, and no error message related to
            LWORK or LIWORK is issued by XERBLA.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i and JOBZ = MagmaNoVec, then the algorithm failed
                  to converge; i off-diagonal elements of an intermediate
                  tridiagonal form did not converge to zero;
                  if INFO = i and JOBZ = MagmaVec, then the algorithm failed
                  to compute an eigenvalue while working on the submatrix
                  lying in rows and columns INFO/(N+1) through
                  mod(INFO,N+1).

    Further Details
    ---------------
    Based on contributions by
       Jeff Rutter, Computer Science Division, University of California
       at Berkeley, USA

    Modified description of INFO. Sven, 16 Feb 05.

    @ingroup magma_ssyev_driver
    ********************************************************************/
extern "C" magma_int_t
magma_ssyevd_gpu(magma_vec_t jobz, magma_uplo_t uplo,
                 magma_int_t n,
                 float *dA, magma_int_t ldda,
                 float *w,
                 float *wA,  magma_int_t ldwa,
                 float *work, magma_int_t lwork,
                 magma_int_t *iwork, magma_int_t liwork,
                 magma_int_t *info)
{
    magma_int_t ione = 1;

    float d__1;

    float eps;
    magma_int_t inde;
    float anrm;
    float rmin, rmax;
    float sigma;
    magma_int_t iinfo, lwmin;
    magma_int_t lower;
    magma_int_t wantz;
    magma_int_t indwk2, llwrk2;
    magma_int_t iscale;
    float safmin;
    float bignum;
    magma_int_t indtau;
    magma_int_t indwrk, liwmin;
    magma_int_t llwork;
    float smlnum;
    magma_int_t lquery;

    float *dwork;
    magma_int_t lddc = ldda;

    wantz = (jobz == MagmaVec);
    lower = (uplo == MagmaLower);
    lquery = (lwork == -1 || liwork == -1);

    *info = 0;
    if (! (wantz || (jobz == MagmaNoVec))) {
        *info = -1;
    } else if (! (lower || (uplo == MagmaUpper))) {
        *info = -2;
    } else if (n < 0) {
        *info = -3;
    } else if (ldda < max(1,n)) {
        *info = -5;
    }

    magma_int_t nb = magma_get_ssytrd_nb( n );
    if ( n <= 1 ) {
        lwmin  = 1;
        liwmin = 1;
    }
    else if ( wantz ) {
        lwmin  = max( 2*n + n*nb, 1 + 6*n + 2*n*n );
        liwmin = 3 + 5*n;
    }
    else {
        lwmin  = 2*n + n*nb;
        liwmin = 1;
    }
    
    // multiply by 1+eps (in Double!) to ensure length gets rounded up,
    // if it cannot be exactly represented in floating point.
    real_Double_t one_eps = 1. + lapackf77_slamch("Epsilon");
    work[0]  = lwmin * one_eps;
    iwork[0] = liwmin;

    if ((lwork < lwmin) && !lquery) {
        *info = -10;
    } else if ((liwork < liwmin) && ! lquery) {
        *info = -12;
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Check if matrix is very small then just call LAPACK on CPU, no need for GPU */
    if (n <= 128) {
        #ifdef ENABLE_DEBUG
        printf("--------------------------------------------------------------\n");
        printf("  warning matrix too small N=%d NB=%d, calling lapack on CPU  \n", (int) n, (int) nb);
        printf("--------------------------------------------------------------\n");
        #endif
        const char* jobz_ = lapack_vec_const( jobz );
        const char* uplo_ = lapack_uplo_const( uplo );
        float *A;
        magma_smalloc_cpu( &A, n*n );
        magma_sgetmatrix(n, n, dA, ldda, A, n);
        lapackf77_ssyevd(jobz_, uplo_,
                         &n, A, &n,
                         w, work, &lwork,
                         iwork, &liwork, info);
        magma_ssetmatrix( n, n, A, n, dA, ldda);
        magma_free_cpu(A);
        return *info;
    }

    magma_queue_t stream;
    magma_queue_create( &stream );

    // n*lddc for ssytrd2_gpu
    // n for slansy
    magma_int_t ldwork = n*lddc;
    if ( wantz ) {
        // need 3n^2/2 for sstedx
        ldwork = max( ldwork, 3*n*(n/2 + 1));
    }

    if (MAGMA_SUCCESS != magma_smalloc( &dwork, ldwork )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    /* Get machine constants. */
    safmin = lapackf77_slamch("Safe minimum");
    eps    = lapackf77_slamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_ssqrt(smlnum);
    rmax = magma_ssqrt(bignum);

    /* Scale matrix to allowable range, if necessary. */
    anrm = magmablas_slansy(MagmaMaxNorm, uplo, n, dA, ldda, dwork);
    iscale = 0;
    sigma  = 1;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        magmablas_slascl(uplo, 0, 0, 1., sigma, n, n, dA, ldda, info);
    }

    /* Call SSYTRD to reduce symmetric matrix to tridiagonal form. */
    // ssytrd work: e (n) + tau (n) + llwork (n*nb)  ==>  2n + n*nb
    // sstedx work: e (n) + tau (n) + z (n*n) + llwrk2 (1 + 4*n + n^2)  ==>  1 + 6n + 2n^2
    inde   = 0;
    indtau = inde   + n;
    indwrk = indtau + n;
    indwk2 = indwrk + n*n;
    llwork = lwork - indwrk;
    llwrk2 = lwork - indwk2;

    magma_timer_t time=0;
    timer_start( time );

#ifdef FAST_SYMV
    magma_ssytrd2_gpu(uplo, n, dA, ldda, w, &work[inde],
                      &work[indtau], wA, ldwa, &work[indwrk], llwork,
                      dwork, n*lddc, &iinfo);
#else
    magma_ssytrd_gpu(uplo, n, dA, ldda, w, &work[inde],
                     &work[indtau], wA, ldwa, &work[indwrk], llwork,
                     &iinfo);
#endif

    timer_stop( time );
    #ifdef FAST_SYMV
    timer_printf( "time ssytrd2 = %6.2f\n", time );
    #else
    timer_printf( "time ssytrd = %6.2f\n", time );
    #endif

    /* For eigenvalues only, call SSTERF.  For eigenvectors, first call
       SSTEDC to generate the eigenvector matrix, WORK(INDWRK), of the
       tridiagonal matrix, then call SORMTR to multiply it to the Householder
       transformations represented as Householder vectors in A. */
    if (! wantz) {
        lapackf77_ssterf(&n, w, &work[inde], info);
    }
    else {
        timer_start( time );

        magma_sstedx(MagmaRangeAll, n, 0., 0., 0, 0, w, &work[inde],
                     &work[indwrk], n, &work[indwk2],
                     llwrk2, iwork, liwork, dwork, info);

        timer_stop( time );
        timer_printf( "time sstedx = %6.2f\n", time );
        timer_start( time );

        magma_ssetmatrix( n, n, &work[indwrk], n, dwork, lddc );

        magma_sormtr_gpu(MagmaLeft, uplo, MagmaNoTrans, n, n, dA, ldda, &work[indtau],
                         dwork, lddc, wA, ldwa, &iinfo);

        magma_scopymatrix( n, n, dwork, lddc, dA, ldda );

        timer_stop( time );
        timer_printf( "time sormtr + copy = %6.2f\n", time );
    }

    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        d__1 = 1. / sigma;
        blasf77_sscal(&n, &d__1, w, &ione);
    }

    work[0]  = lwmin * one_eps;  // round up
    iwork[0] = liwmin;

    magma_queue_destroy( stream );
    magma_free( dwork );

    return *info;
} /* magma_ssyevd_gpu */
