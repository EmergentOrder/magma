/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Stan Tomov
       @author Raffaele Solca
       @author Azzam Haidar

       @generated from dsyevdx_2stage_m.cpp normal d -> s, Fri Jul 18 17:34:19 2014

*/
#include "common_magma.h"
#include "timer.h"
#include "magma_bulge.h"
#include "magma_sbulge.h"

#include <cblas.h>

#define PRECISION_s

/**
    Purpose
    -------
    ZHEEVD_2STAGE computes all eigenvalues and, optionally, eigenvectors of a
    complex Hermitian matrix A. It uses a two-stage algorithm for the tridiagonalization.
    If eigenvectors are desired, it uses a divide and conquer algorithm.

    The divide and conquer algorithm makes very mild assumptions about
    floating point arithmetic. It will work on machines with a guard
    digit in add/subtract, or on those binary machines without guard
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or
    Cray-2. It could conceivably fail on hexadecimal or decimal machines
    without guard digits, but we know of none.

    Arguments
    ---------
    @param[in]
    nrgpu   INTEGER
            Number of GPUs to use.

    @param[in]
    jobz    magma_vec_t
      -     = MagmaNoVec:  Compute eigenvalues only;
      -     = MagmaVec:    Compute eigenvalues and eigenvectors.

    @param[in]
    range   magma_range_t
      -     = MagmaRangeAll: all eigenvalues will be found.
      -     = MagmaRangeV:   all eigenvalues in the half-open interval (VL,VU]
                   will be found.
      -     = MagmaRangeI:   the IL-th through IU-th eigenvalues will be found.

    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    A       COMPLEX_16 array, dimension (LDA, N)
            On entry, the Hermitian matrix A.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
            On exit, if JOBZ = MagmaVec, then if INFO = 0, the first m columns
            of A contains the required
            orthonormal eigenvectors of the matrix A.
            If JOBZ = MagmaNoVec, then on exit the lower triangle (if UPLO=MagmaLower)
            or the upper triangle (if UPLO=MagmaUpper) of A, including the
            diagonal, is destroyed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in]
    vl      REAL
    @param[in]
    vu      REAL
            If RANGE=MagmaRangeV, the lower and upper bounds of the interval to
            be searched for eigenvalues. VL < VU.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeI.

    @param[in]
    il      INTEGER
    @param[in]
    iu      INTEGER
            If RANGE=MagmaRangeI, the indices (in ascending order) of the
            smallest and largest eigenvalues to be returned.
            1 <= IL <= IU <= N, if N > 0; IL = 1 and IU = 0 if N = 0.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeV.
            
    @param[out]
    m       INTEGER
            The total number of eigenvalues found.  0 <= M <= N.
            If RANGE = MagmaRangeAll, M = N, and if RANGE = MagmaRangeI, M = IU-IL+1.

    @param[out]
    w       REAL array, dimension (N)
            If INFO = 0, the required m eigenvalues in ascending order.

    @param[out]
    work    (workspace) COMPLEX_16 array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.
            If N <= 1,                      LWORK >= 1.
            If JOBZ = MagmaNoVec and N > 1, LWORK >= LQ2 + N * (NB + 2).
            If JOBZ = MagmaVec   and N > 1, LWORK >= LQ2 + 1 + 6*N + 2*N**2.
            where LQ2 is the size needed to store the Q2 matrix
            and is returned by magma_bulge_get_lq2.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal sizes of the WORK, RWORK and
            IWORK arrays, returns these values as the first entries of
            the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    @param[out]
    iwork   (workspace) INTEGER array, dimension (MAX(1,LIWORK))
            On exit, if INFO = 0, IWORK(1) returns the optimal LIWORK.

    @param[in]
    liwork  INTEGER
            The dimension of the array IWORK.
            If N <= 1,                      LIWORK >= 1.
            If JOBZ = MagmaNoVec and N > 1, LIWORK >= 1.
            If JOBZ = MagmaVec   and N > 1, LIWORK >= 3 + 5*N.
    \n
            If LIWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

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
magma_ssyevdx_2stage_m(magma_int_t nrgpu, magma_vec_t jobz, magma_range_t range, magma_uplo_t uplo,
                       magma_int_t n,
                       float *A, magma_int_t lda,
                       float vl, float vu, magma_int_t il, magma_int_t iu,
                       magma_int_t *m, float *w,
                       float *work, magma_int_t lwork,
                       magma_int_t *iwork, magma_int_t liwork,
                       magma_int_t *info)
{
    #define A(i_,j_)  (A  + (i_) + (j_)*lda)
    #define A2(i_,j_) (A2 + (i_) + (j_)*lda2)
    
    const char* uplo_  = lapack_uplo_const( uplo  );
    const char* jobz_  = lapack_vec_const( jobz  );
    float d_one  = 1.;
    magma_int_t ione = 1;
    magma_int_t izero = 0;

    float d__1;

    float eps;
    float anrm;
    magma_int_t imax;
    float rmin, rmax;
    float sigma;
    magma_int_t lwmin, liwmin;
    magma_int_t lower;
    magma_int_t wantz;
    magma_int_t iscale;
    float safmin;
    float bignum;
    float smlnum;
    magma_int_t lquery;
    magma_int_t alleig, valeig, indeig;

    /* determine the number of threads */
    magma_int_t parallel_threads = magma_get_parallel_numthreads();

    wantz = (jobz == MagmaVec);
    lower = (uplo == MagmaLower);

    alleig = (range == MagmaRangeAll);
    valeig = (range == MagmaRangeV);
    indeig = (range == MagmaRangeI);

    lquery = (lwork == -1 || liwork == -1);

    *info = 0;
    if (! (wantz || (jobz == MagmaNoVec))) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (lower || (uplo == MagmaUpper))) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (lda < max(1,n)) {
        *info = -6;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -8;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -9;
            } else if (iu < min(n,il) || iu > n) {
                *info = -10;
            }
        }
    }

    magma_int_t nb = magma_get_sbulge_nb(n, parallel_threads);
    magma_int_t Vblksiz = magma_sbulge_get_Vblksiz(n, nb, parallel_threads);

    magma_int_t ldt = Vblksiz;
    magma_int_t ldv = nb + Vblksiz;
    magma_int_t blkcnt = magma_bulge_get_blkcnt(n, nb, Vblksiz);
    magma_int_t lq2 = magma_sbulge_get_lq2(n, parallel_threads);

    if (wantz) {
        lwmin  = lq2 + 1 + 6*n + 2*n*n;
        liwmin = 5*n + 3;
    } else {
        lwmin  = lq2 + n*(nb + 1);
        liwmin = 1;
    }

    // multiply by 1+eps (in Double!) to ensure length gets rounded up,
    // if it cannot be exactly represented in floating point.
    real_Double_t one_eps = 1. + lapackf77_slamch("Epsilon");
    work[0]  = lwmin * one_eps;
    iwork[0] = liwmin;

    if ((lwork < lwmin) && !lquery) {
        *info = -14;
    } else if ((liwork < liwmin) && ! lquery) {
        *info = -16;
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (n == 0) {
        return *info;
    }

    if (n == 1) {
        w[0] = A[0];
        if (wantz) {
            A[0] = MAGMA_S_ONE;
        }
        return *info;
    }

    timer_printf("using %d parallel_threads\n", (int) parallel_threads);

    /* Check if matrix is very small then just call LAPACK on CPU, no need for GPU */
    magma_int_t ntiles = n/nb;
    if ( ( ntiles < 2 ) || ( n <= 128 ) ) {
        #ifdef ENABLE_DEBUG
        printf("--------------------------------------------------------------\n");
        printf("  warning matrix too small N=%d NB=%d, calling lapack on CPU  \n", (int) n, (int) nb);
        printf("--------------------------------------------------------------\n");
        #endif
        lapackf77_ssyevd(jobz_, uplo_, &n,
                         A, &lda, w,
                         work, &lwork,
                         iwork, &liwork,
                         info);
        *m = n;
        return *info;
    }
    
    /* Get machine constants. */
    safmin = lapackf77_slamch("Safe minimum");
    eps = lapackf77_slamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_ssqrt(smlnum);
    rmax = magma_ssqrt(bignum);

    /* Scale matrix to allowable range, if necessary. */
    anrm = lapackf77_slansy("M", uplo_, &n, A, &lda, work);
    iscale = 0;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        lapackf77_slascl(uplo_, &izero, &izero, &d_one, &sigma, &n, &n, A,
                         &lda, info);
    }

    magma_int_t inde    = 0;
    magma_int_t indT2   = inde + n;
    magma_int_t indTAU2 = indT2  + blkcnt*ldt*Vblksiz;
    magma_int_t indV2   = indTAU2+ blkcnt*Vblksiz;
    magma_int_t indtau1 = indV2  + blkcnt*ldv*Vblksiz;
    magma_int_t indwrk  = indtau1+ n;
    magma_int_t indwk2  = indwrk + n*n;
    magma_int_t llwork = lwork - indwrk;
    magma_int_t llwrk2 = lwork - indwk2;

    magma_timer_t time=0, time_total=0, time_alloc=0, time_sist=0, time_band=0;
    timer_start( time_total );

#ifdef HE2HB_SINGLEGPU
    float *dT1;
    if (MAGMA_SUCCESS != magma_smalloc( &dT1, n*nb)) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    magma_ssytrd_sy2sb(uplo, n, nb, A, lda, &work[indtau1], &work[indwrk], llwork, dT1, info);
    magma_free(dT1);
#else
    magma_int_t nstream = max(3,nrgpu+2);
    magma_queue_t streams[MagmaMaxGPUs][20];
    float *dA[MagmaMaxGPUs], *dT1[MagmaMaxGPUs];
    magma_int_t ldda = ((n+31)/32)*32;

    magma_int_t ver = 0;
    magma_int_t distblk = max(256, 4*nb);

    #ifdef ENABLE_DEBUG
    printf("voici ngpu %d distblk %d NB %d nstream %d version %d \n ", nrgpu, distblk, nb, nstream, ver);
    #endif

    timer_start( time_alloc );
    for( magma_int_t dev = 0; dev < nrgpu; ++dev ) {
        magma_int_t mlocal = ((n / distblk) / nrgpu + 1) * distblk;
        magma_setdevice( dev );
        // TODO check malloc
        magma_smalloc(&dA[dev], ldda*mlocal );
        magma_smalloc(&dT1[dev], (n*nb) );
        for( int i = 0; i < nstream; ++i ) {
            magma_queue_create( &streams[dev][i] );
        }
    }
    timer_stop( time_alloc );

    timer_start( time_sist );
    magma_ssetmatrix_1D_col_bcyclic( n, n, A, lda, dA, ldda, nrgpu, distblk );
    magma_setdevice(0);
    timer_stop( time_sist );

    timer_start( time_band );
    if (ver == 30) {
        magma_ssytrd_sy2sb_mgpu_spec(uplo, n, nb, A, lda, &work[indtau1], &work[indwrk], llwork, dA, ldda, dT1, nb, nrgpu, distblk, streams, nstream, info);
    } else {
        magma_ssytrd_sy2sb_mgpu(uplo, n, nb, A, lda, &work[indtau1], &work[indwrk], llwork, dA, ldda, dT1, nb, nrgpu, distblk, streams, nstream, info);
    }
    timer_stop( time_band );
    timer_printf("  time alloc %7.4f, ditribution %7.4f, ssytrd_he2hb only = %7.4f\n", time_alloc, time_sist, time_band );

    for( magma_int_t dev = 0; dev < nrgpu; ++dev ) {
        magma_setdevice( dev );
        magma_free( dA[dev] );
        magma_free( dT1[dev] );
        for( int i = 0; i < nstream; ++i ) {
            magma_queue_destroy( streams[dev][i] );
        }
    }
#endif  // not HE2HB_SINGLEGPU

    timer_stop( time_total );
    timer_printf( "  time ssytrd_sy2sb_mgpu = %6.2f\n", time_total );
    timer_start( time_total );
    timer_start( time );

    /* copy the input matrix into WORK(INDWRK) with band storage */
    /* PAY ATTENTION THAT work[indwrk] should be able to be of size lda2*n which it should be checked in any future modification of lwork.*/
    magma_int_t lda2 = 2*nb; //nb+1+(nb-1);
    float* A2 = &work[indwrk];
    memset(A2, 0, n*lda2*sizeof(float));

    for (magma_int_t j = 0; j < n-nb; j++) {
        cblas_scopy(nb+1, A(j,j), 1, A2(0,j), 1);
        memset(A(j,j), 0, (nb+1)*sizeof(float));
        *A(nb+j,j) = d_one;
    }
    for (magma_int_t j = 0; j < nb; j++) {
        cblas_scopy(nb-j, A(j+n-nb,j+n-nb), 1, A2(0,j+n-nb), 1);
        memset(A(j+n-nb,j+n-nb), 0, (nb-j)*sizeof(float));
    }

    timer_stop( time );
    timer_printf( "  time ssytrd_convert = %6.2f\n", time );
    timer_start( time );

    magma_ssytrd_sb2st(uplo, n, nb, Vblksiz, A2, lda2, w, &work[inde], &work[indV2], ldv, &work[indTAU2], wantz, &work[indT2], ldt);

    timer_stop( time );
    timer_stop( time_total );
    timer_printf( "  time ssytrd_sy2st = %6.2f\n", time );
    timer_printf( "  time ssytrd = %6.2f\n", time_total );

    /* For eigenvalues only, call SSTERF.  For eigenvectors, first call
       ZSTEDC to generate the eigenvector matrix, WORK(INDWRK), of the
       tridiagonal matrix, then call ZUNMTR to multiply it to the Householder
       transformations represented as Householder vectors in A. */
    if (! wantz) {
        timer_start( time );

        lapackf77_ssterf(&n, w, &work[inde], info);
        magma_smove_eig(range, n, w, &il, &iu, vl, vu, m);

        timer_stop( time );
        timer_printf( "  time sstedc = %6.2f\n", time );
    }
    else {
        timer_start( time_total );
        timer_start( time );

        magma_sstedx_m(nrgpu, range, n, vl, vu, il, iu, w, &work[inde],
                       &work[indwrk], n, &work[indwk2],
                       llwrk2, iwork, liwork, info);

        timer_stop( time );
        timer_printf( "  time sstedx_m = %6.2f\n", time );
        timer_start( time );

        magma_smove_eig(range, n, w, &il, &iu, vl, vu, m);

        magma_sbulge_back_m(nrgpu, uplo, n, nb, *m, Vblksiz, &work[indwrk + n * (il-1)], n,
                            &work[indV2], ldv, &work[indTAU2], &work[indT2], ldt, info);

        timer_stop( time );
        timer_printf( "  time sbulge_back_m = %6.2f\n", time );
        timer_start( time );
        
        magma_sormqr_m(nrgpu, MagmaLeft, MagmaNoTrans, n-nb, *m, n-nb, A+nb, lda, &work[indtau1],
                       &work[indwrk + n * (il-1) + nb], n, &work[indwk2], llwrk2, info);

        lapackf77_slacpy("A", &n, m, &work[indwrk  + n * (il-1)], &n, A, &lda);

        timer_stop( time );
        timer_stop( time_total );
        timer_printf( "  time sormqr_m + copy = %6.2f\n", time );
        timer_printf( "  time eigenvectors backtransf. = %6.2f\n", time_total );
    }

    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        if (*info == 0) {
            imax = n;
        } else {
            imax = *info - 1;
        }
        d__1 = 1. / sigma;
        blasf77_sscal(&imax, &d__1, w, &ione);
    }

    work[0]  = lwmin * one_eps;
    iwork[0] = liwmin;

    return *info;
} /* magma_zheevdx_2stage */
