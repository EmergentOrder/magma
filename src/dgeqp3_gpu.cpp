/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014
  
       @generated from zgeqp3_gpu.cpp normal z -> d, Fri Jul 18 17:34:17 2014

*/
#include "common_magma.h"
#include <cblas.h>

#define PRECISION_d
#define REAL

/**
    Purpose
    -------
    DGEQP3 computes a QR factorization with column pivoting of a
    matrix A:  A*P = Q*R  using Level 3 BLAS.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A. M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    dA      DOUBLE_PRECISION array on the GPU, dimension (LDDA,N)
            On entry, the M-by-N matrix A.
            On exit, the upper triangle of the array contains the
            min(M,N)-by-N upper trapezoidal matrix R; the elements below
            the diagonal, together with the array TAU, represent the
            unitary matrix Q as a product of min(M,N) elementary
            reflectors.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A. LDDA >= max(1,M).

    @param[in,out]
    jpvt    INTEGER array, dimension (N)
            On entry, if JPVT(J).ne.0, the J-th column of A is permuted
            to the front of A*P (a leading column); if JPVT(J)=0,
            the J-th column of A is a free column.
            On exit, if JPVT(J)=K, then the J-th column of A*P was the
            the K-th column of A.

    @param[out]
    tau     DOUBLE_PRECISION array, dimension (min(M,N))
            The scalar factors of the elementary reflectors.

    @param[out]
    dwork   (workspace) DOUBLE_PRECISION array on the GPU, dimension (MAX(1,LWORK))
            On exit, if INFO=0, WORK(1) returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.
            For [sd]geqp3, LWORK >= (N+1)*NB + 2*N;
            for [cz]geqp3, LWORK >= (N+1)*NB,
            where NB is the optimal blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param
    rwork   (workspace, for [cz]geqp3 only) DOUBLE PRECISION array, dimension (2*N)

    @param[out]
    info    INTEGER
      -     = 0: successful exit.
      -     < 0: if INFO = -i, the i-th argument had an illegal value.

    Further Details
    ---------------
    The matrix Q is represented as a product of elementary reflectors

        Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

        H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector
    with v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in
    A(i+1:m,i), and tau in TAU(i).

    @ingroup magma_dgeqp3_comp
    ********************************************************************/
extern "C" magma_int_t
magma_dgeqp3_gpu( magma_int_t m, magma_int_t n,
                  double *dA, magma_int_t ldda,
                  magma_int_t *jpvt, double *tau,
                  double *dwork, magma_int_t lwork,
                  #ifdef COMPLEX
                  double *rwork,
                  #endif
                  magma_int_t *info )
{
    #define dA(i_, j_) (dA + (i_) + (j_)*ldda)

    magma_int_t ione = 1;

    //magma_int_t na;
    magma_int_t n_j;
    magma_int_t j, jb, nb, sm, sn, fjb, nfxd, minmn;
    magma_int_t topbmn, sminmn, lwkopt, lquery;
    
    *info = 0;
    lquery = (lwork == -1);
    if (m < 0) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (ldda < max(1,m)) {
        *info = -4;
    }
    
    nb = magma_get_dgeqp3_nb(min(m, n));
    minmn = min(m,n);
    if (*info == 0) {
        if (minmn == 0) {
            lwkopt = 1;
        } else {
            lwkopt = (n + 1)*nb;
            #ifdef REAL
            lwkopt += 2*n;
            #endif
        }
        //dwork[0] = MAGMA_D_MAKE( lwkopt, 0. );

        if (lwork < lwkopt && ! lquery) {
            *info = -8;
        }
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    } else if (lquery) {
        return *info;
    }

    if (minmn == 0)
        return *info;

    #ifdef REAL
    double *rwork = dwork + (n + 1)*nb;
    #endif
    double   *df;
    if (MAGMA_SUCCESS != magma_dmalloc( &df, (n+1)*nb )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    cudaMemset( df, 0, (n+1)*nb*sizeof(double) );

    nfxd = 0;
    /* Move initial columns up front.
     * Note jpvt uses 1-based indices for historical compatibility. */
    for (j = 0; j < n; ++j) {
        if (jpvt[j] != 0) {
            if (j != nfxd) {
                blasf77_dswap(&m, dA(0, j), &ione, dA(0, nfxd), &ione);  // TODO: matrix not on CPU!
                jpvt[j]    = jpvt[nfxd];
                jpvt[nfxd] = j + 1;
            }
            else {
                jpvt[j] = j + 1;
            }
            ++nfxd;
        }
        else {
            jpvt[j] = j + 1;
        }
    }

    /*     Factorize fixed columns
           =======================
           Compute the QR factorization of fixed columns and update
           remaining columns.
    if (nfxd > 0) {
        na = min(m,nfxd);
        lapackf77_dgeqrf(&m, &na, A, &lda, tau, dwork, &lwork, info);
        if (na < n) {
            n_j = n - na;
            lapackf77_dormqr( MagmaLeftStr, MagmaTransStr, &m, &n_j, &na,
                              A, &lda, tau, dA(0, na), &lda,
                              dwork, &lwork, info );
        }
    }*/
    
    /*  Factorize free columns */
    if (nfxd < minmn) {
        sm = m - nfxd;
        sn = n - nfxd;
        sminmn = minmn - nfxd;
        
        /*if (nb < sminmn) {
            j = nfxd;
            
            // Set the original matrix to the GPU
            magma_dsetmatrix_async( m, sn,
                                     A(0,j), lda,
                                    dA(0,j), ldda, stream[0] );
        }*/

        /* Initialize partial column norms. */
        magmablas_dnrm2_cols(sm, sn, dA(nfxd,nfxd), ldda, &rwork[nfxd]);
#if defined(PRECISION_d) || defined(PRECISION_z)
        magma_dcopymatrix( sn, 1, &rwork[nfxd], sn, &rwork[n+nfxd], sn);
#else
        magma_scopymatrix( sn, 1, &rwork[nfxd], sn, &rwork[n+nfxd], sn);
#endif
        /*for (j = nfxd; j < n; ++j) {
            rwork[j] = cblas_dnrm2(sm, dA(nfxd, j), ione);
            rwork[n + j] = rwork[j];
        }*/
        
        j = nfxd;
        //if (nb < sminmn)
        {
            /* Use blocked code initially. */
            //magma_queue_sync( stream[0] );
            
            /* Compute factorization: while loop. */
            topbmn = minmn; // - nb;
            while(j < topbmn) {
                jb = min(nb, topbmn - j);
                
                /* Factorize JB columns among columns J:N. */
                n_j = n - j;
                
                /*if (j > nfxd) {
                    // Get panel to the CPU
                    magma_dgetmatrix( m-j, jb,
                                      dA(j,j), ldda,
                                       A(j,j), lda );
                    
                    // Get the rows
                    magma_dgetmatrix( jb, n_j - jb,
                                      dA(j,j + jb), ldda,
                                       A(j,j + jb), lda );
                }*/

                //magma_dlaqps_gpu    // this is a cpp-file
                magma_dlaqps2_gpu   // this is a cuda-file
                    ( m, n_j, j, jb, &fjb,
                      dA(0, j), ldda,
                      &jpvt[j], &tau[j], &rwork[j], &rwork[n + j],
                      dwork,
                      &df[jb],   n_j );
                
                j += fjb;  /* fjb is actual number of columns factored */
            }
        }
        
        /* Use unblocked code to factor the last or only block.
        if (j < minmn) {
            n_j = n - j;
            if (j > nfxd) {
                magma_dgetmatrix( m-j, n_j,
                                  dA(j,j), ldda,
                                   A(j,j), lda );
            }
            lapackf77_dlaqp2(&m, &n_j, &j, dA(0, j), &lda, &jpvt[j],
                             &tau[j], &rwork[j], &rwork[n+j], dwork );
        }*/
    }
    //dwork[0] = MAGMA_D_MAKE( lwkopt, 0. );
    magma_free(df);

    return *info;
} /* magma_dgeqp3_gpu */
