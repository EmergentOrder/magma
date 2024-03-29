/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Mark Gates

       @generated from zunmlq.cpp normal z -> s, Fri Jul 18 17:34:17 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    SORMLQ overwrites the general real M-by-N matrix C with

    @verbatim
                             SIDE = MagmaLeft     SIDE = MagmaRight
    TRANS = MagmaNoTrans:    Q * C                C * Q
    TRANS = MagmaTrans:  Q**T * C             C * Q**T
    @endverbatim

    where Q is a realunitary matrix defined as the product of k
    elementary reflectors

          Q = H(k)**T . . . H(2)**T H(1)**T

    as returned by SGELQF. Q is of order M if SIDE = MagmaLeft and of order N
    if SIDE = MagmaRight.

    Arguments
    ---------
    @param[in]
    side    magma_side_t
      -     = MagmaLeft:      apply Q or Q**T from the Left;
      -     = MagmaRight:     apply Q or Q**T from the Right.

    @param[in]
    trans   magma_trans_t
      -     = MagmaNoTrans:    No transpose, apply Q;
      -     = MagmaTrans:  Conjugate transpose, apply Q**T.

    @param[in]
    m       INTEGER
            The number of rows of the matrix C. M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix C. N >= 0.

    @param[in]
    k       INTEGER
            The number of elementary reflectors whose product defines
            the matrix Q.
            If SIDE = MagmaLeft,  M >= K >= 0;
            if SIDE = MagmaRight, N >= K >= 0.

    @param[in]
    A       REAL array, dimension
                (LDA,M) if SIDE = MagmaLeft,
                (LDA,N) if SIDE = MagmaRight.
            The i-th row must contain the vector which defines the
            elementary reflector H(i), for i = 1,2,...,k, as returned by
            SGELQF in the first k rows of its array argument A.
            A is modified by the routine but restored on exit.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A. LDA >= max(1,K).

    @param[in]
    tau     REAL array, dimension (K)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by SGELQF.

    @param[in,out]
    C       REAL array, dimension (LDC,N)
            On entry, the M-by-N matrix C.
            On exit, C is overwritten by Q*C or Q**T*C or C*Q**T or C*Q.

    @param[in]
    ldc     INTEGER
            The leading dimension of the array C. LDC >= max(1,M).

    @param[out]
    work    (workspace) REAL array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.
            If SIDE = MagmaLeft,  LWORK >= max(1,N);
            if SIDE = MagmaRight, LWORK >= max(1,M).
            For optimum performance
            if SIDE = MagmaLeft,  LWORK >= N*NB;
            if SIDE = MagmaRight, LWORK >= M*NB,
            where NB is the optimal blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_sgelqf_comp
    ********************************************************************/
extern "C" magma_int_t
magma_sormlq(
    magma_side_t side, magma_trans_t trans,
    magma_int_t m, magma_int_t n, magma_int_t k,
    float *A, magma_int_t lda,
    float *tau,
    float *C, magma_int_t ldc,
    float *work, magma_int_t lwork,
    magma_int_t *info)
{
    #define  A(i_,j_) ( A + (i_) + (j_)*lda)
    #define dC(i_,j_) (dC + (i_) + (j_)*lddc)

    float *T, *T2;
    magma_int_t i, i1, i2, ib, ic, jc, nb, mi, ni, nq, nq_i, nw, step;
    magma_int_t iinfo, ldwork, lwkopt;
    magma_int_t left, notran, lquery;
    magma_trans_t transt;

    *info = 0;
    left   = (side  == MagmaLeft);
    notran = (trans == MagmaNoTrans);
    lquery = (lwork == -1);

    /* NQ is the order of Q and NW is the minimum dimension of WORK */
    if (left) {
        nq = m;
        nw = n;
    } else {
        nq = n;
        nw = m;
    }
    
    /* Test the input arguments */
    if (! left && side != MagmaRight) {
        *info = -1;
    } else if (! notran && trans != MagmaTrans) {
        *info = -2;
    } else if (m < 0) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (k < 0 || k > nq) {
        *info = -5;
    } else if (lda < max(1,k)) {
        *info = -7;
    } else if (ldc < max(1,m)) {
        *info = -10;
    } else if (lwork < max(1,nw) && ! lquery) {
        *info = -12;
    }

    if (*info == 0) {
        nb = magma_get_sgelqf_nb( min( m, n ));
        lwkopt = max(1,nw)*nb;
        work[0] = MAGMA_S_MAKE( lwkopt, 0 );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0 || k == 0) {
        work[0] = MAGMA_S_ONE;
        return *info;
    }

    ldwork = nw;
    
    if (nb >= k) {
        /* Use CPU code */
        lapackf77_sormlq( lapack_side_const(side), lapack_trans_const(trans),
            &m, &n, &k, A, &lda, tau, C, &ldc, work, &lwork, &iinfo);
    }
    else {
        /* Use hybrid CPU-GPU code */
        /* Allocate work space on the GPU.
         * nw*nb  for dwork (m or n) by nb
         * nq*nb  for dV    (n or m) by nb
         * nb*nb  for dT
         * lddc*n for dC.
         */
        magma_int_t lddc = ((m+31)/32)*32;
        float *dwork, *dV, *dT, *dC;
        magma_smalloc( &dwork, (nw + nq + nb)*nb + lddc*n );
        if ( dwork == NULL ) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        dV = dwork + nw*nb;
        dT = dV    + nq*nb;
        dC = dT    + nb*nb;
        
        /* work space on CPU.
         * nb*nb for T
         * nb*nb for T2, used to save and restore diagonal block of panel  */
        magma_smalloc_cpu( &T, 2*nb*nb );
        if ( T == NULL ) {
            magma_free( dwork );
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        T2 = T + nb*nb;
        
        /* Copy matrix C from the CPU to the GPU */
        magma_ssetmatrix( m, n, C, ldc, dC, lddc );
        
        if ( (left && notran) || (! left && ! notran) ) {
            i1 = 0;
            i2 = k;
            step = nb;
        } else {
            i1 = ((k - 1) / nb)*nb;
            i2 = 0;
            step = -nb;
        }

        // silence "uninitialized" warnings
        mi = 0;
        ni = 0;
        
        if (left) {
            ni = n;
            jc = 0;
        } else {
            mi = m;
            ic = 0;
        }

        if (notran) {
            transt = MagmaTrans;
        } else {
            transt = MagmaNoTrans;
        }

        for (i = i1; (step < 0 ? i >= i2 : i < i2); i += step) {
            ib = min(nb, k - i);
            
            /* Form the triangular factor of the block reflector
               H = H(i) H(i + 1) . . . H(i + ib-1) */
            nq_i = nq - i;
            lapackf77_slarft("Forward", "Rowwise", &nq_i, &ib,
                             A(i,i), &lda, &tau[i], T, &ib);

            /* 1) set upper triangle of panel in A to identity,
               2) copy the panel from A to the GPU, and
               3) restore A                                      */
            spanel_to_q( MagmaLower, ib, A(i,i), lda, T2 );
            magma_ssetmatrix( ib, nq_i,  A(i,i), lda, dV, ib );
            sq_to_panel( MagmaLower, ib, A(i,i), lda, T2 );
            
            if (left) {
                /* H or H**T is applied to C(i:m,1:n) */
                mi = m - i;
                ic = i;
            }
            else {
                /* H or H**T is applied to C(1:m,i:n) */
                ni = n - i;
                jc = i;
            }
            
            /* Apply H or H**T; First copy T to the GPU */
            magma_ssetmatrix( ib, ib, T, ib, dT, ib );
            magma_slarfb_gpu( side, transt, MagmaForward, MagmaRowwise,
                              mi, ni, ib,
                              dV, ib,
                              dT, ib,
                              dC(ic,jc), lddc,
                              dwork, ldwork );
        }
        magma_sgetmatrix( m, n, dC, lddc, C, ldc );
        
        magma_free( dwork );
        magma_free_cpu( T );
    }
    work[0] = MAGMA_S_MAKE( lwkopt, 0 );
    
    return *info;
} /* magma_sormlq */
