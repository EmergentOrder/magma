/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Raffaele Solca
       @author Stan Tomov

       @generated from zhetrd2_gpu.cpp normal z -> c, Fri Jul 18 17:34:18 2014

*/
#include "common_magma.h"

/**
    Purpose
    -------
    CHETRD2_GPU reduces a complex Hermitian matrix A to real symmetric
    tridiagonal form T by an orthogonal similarity transformation:
    Q**H * A * Q = T.
    This version passes a workspace that is used in an optimized
    GPU matrix-vector product.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    dA      COMPLEX array, dimension (LDA,N)
            On entry, the Hermitian matrix A.  If UPLO = MagmaUpper, the leading
            N-by-N upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = MagmaLower, the
            leading N-by-N lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
            On exit, if UPLO = MagmaUpper, the diagonal and first superdiagonal
            of A are overwritten by the corresponding elements of the
            tridiagonal matrix T, and the elements above the first
            superdiagonal, with the array TAU, represent the orthogonal
            matrix Q as a product of elementary reflectors; if UPLO
            = MagmaLower, the diagonal and first subdiagonal of A are over-
            written by the corresponding elements of the tridiagonal
            matrix T, and the elements below the first subdiagonal, with
            the array TAU, represent the orthogonal matrix Q as a product
            of elementary reflectors. See Further Details.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    d       COMPLEX array, dimension (N)
            The diagonal elements of the tridiagonal matrix T:
            D(i) = A(i,i).

    @param[out]
    e       COMPLEX array, dimension (N-1)
            The off-diagonal elements of the tridiagonal matrix T:
            E(i) = A(i,i+1) if UPLO = MagmaUpper, E(i) = A(i+1,i) if UPLO = MagmaLower.

    @param[out]
    tau     COMPLEX array, dimension (N-1)
            The scalar factors of the elementary reflectors (see Further
            Details).

    @param[out]
    wA      (workspace) COMPLEX array, dimension (LDA,N)
            On exit the diagonal, the  upper part (UPLO=MagmaUpper)
            or the lower part (UPLO=MagmaLower) are copies of DA

    @param[in]
    ldwa    INTEGER
            The leading dimension of the array wA.  LDWA >= max(1,N).

    @param[out]
    work    (workspace) COMPLEX array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.  LWORK >= 1.
            For optimum performance LWORK >= N*NB, where NB is the
            optimal blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param[out]
    dwork   (workspace) COMPLEX array on the GPU, dim (MAX(1,LDWORK))

    @param[in]
    ldwork  INTEGER
            The dimension of the array DWORK.
            LDWORK >= (n*n+64-1)/64 + 2*n*nb, where nb = magma_get_chetrd_nb(n)

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    Further Details
    ---------------
    If UPLO = MagmaUpper, the matrix Q is represented as a product of elementary
    reflectors

        Q = H(n-1) . . . H(2) H(1).

    Each H(i) has the form

        H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector with
    v(i+1:n) = 0 and v(i) = 1; v(1:i-1) is stored on exit in
    A(1:i-1,i+1), and tau in TAU(i).

    If UPLO = MagmaLower, the matrix Q is represented as a product of elementary
    reflectors

        Q = H(1) H(2) . . . H(n-1).

    Each H(i) has the form

        H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector with
    v(1:i) = 0 and v(i+1) = 1; v(i+2:n) is stored on exit in A(i+2:n,i),
    and tau in TAU(i).

    The contents of A on exit are illustrated by the following examples
    with n = 5:

    if UPLO = MagmaUpper:                if UPLO = MagmaLower:

        (  d   e   v2  v3  v4 )              (  d                  )
        (      d   e   v3  v4 )              (  e   d              )
        (          d   e   v4 )              (  v1  e   d          )
        (              d   e  )              (  v1  v2  e   d      )
        (                  d  )              (  v1  v2  v3  e   d  )

    where d and e denote diagonal and off-diagonal elements of T, and vi
    denotes an element of the vector defining H(i).

    @ingroup magma_cheev_comp
    ********************************************************************/
extern "C" magma_int_t
magma_chetrd2_gpu(magma_uplo_t uplo, magma_int_t n,
                  magmaFloatComplex *dA, magma_int_t ldda,
                  float *d, float *e, magmaFloatComplex *tau,
                  magmaFloatComplex *wA,  magma_int_t ldwa,
                  magmaFloatComplex *work, magma_int_t lwork,
                  magmaFloatComplex *dwork, magma_int_t ldwork,
                  magma_int_t *info)
{
#define  A(i, j) (wA + (j)*ldwa + (i))
#define dA(i, j) (dA + (j)*ldda + (i))

    const char* uplo_ = lapack_uplo_const( uplo );

    magma_int_t nb = magma_get_chetrd_nb(n);

    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magmaFloatComplex c_one     = MAGMA_C_ONE;
    float          d_one     = MAGMA_D_ONE;
    
    magma_int_t kk, nx;
    magma_int_t i, j, i_n;
    magma_int_t iinfo;
    magma_int_t ldw, lddw, lwkopt;
    magma_int_t lquery;

    *info = 0;
    int upper = (uplo == MagmaUpper);
    lquery = (lwork == -1);
    if (! upper && uplo != MagmaLower) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (ldda < max(1,n)) {
        *info = -4;
    } else if (ldwa < max(1,n)) {
        *info = -9;
    } else if (lwork < 1 && ! lquery) {
        *info = -11;
    }

    /* Determine the block size. */
    ldw = lddw = n;
    lwkopt = n * nb;
    if (*info == 0) {
        work[0] = MAGMA_C_MAKE( lwkopt, 0 );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery)
        return *info;

    /* Quick return if possible */
    if (n == 0) {
        work[0] = c_one;
        return *info;
    }

    if (n < 1024)
        nx = n;
    else
        nx = 300;

    if (ldwork < (ldw*n+64-1)/64 + 2*ldw*nb) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    if (upper) {
        /*  Reduce the upper triangle of A.
            Columns 1:kk are handled by the unblocked method. */
        kk = n - (n - nx + nb - 1) / nb * nb;
        
        for (i = n - nb; i >= kk; i -= nb) {
            /* Reduce columns i:i+nb-1 to tridiagonal form and form the
               matrix W which is needed to update the unreduced part of
               the matrix */
            
            /*   Get the current panel */
            magma_cgetmatrix( i+nb, nb, dA(0, i), ldda, A(0, i), ldwa );
            
            magma_clatrd2(uplo, i+nb, nb, A(0, 0), ldwa, e, tau,
                          work, ldw, dA(0, 0), ldda, dwork, lddw, dwork + 2*ldw*nb, ldwork - 2*ldw*nb);
            
            /* Update the unreduced submatrix A(0:i-2,0:i-2), using an
               update of the form:  A := A - V*W' - W*V' */
            
            magma_csetmatrix( i + nb, nb, work, ldw, dwork, lddw );
            
            magma_cher2k(uplo, MagmaNoTrans, i, nb, c_neg_one,
                         dA(0, i), ldda, dwork,
                         lddw, d_one, dA(0, 0), ldda);
            
            /* Copy superdiagonal elements back into A, and diagonal
               elements into D */
            for (j = i; j < i+nb; ++j) {
                *A(j-1,j) = MAGMA_C_MAKE( e[j - 1], 0 );
                d[j] = MAGMA_C_REAL( *A(j, j) );
            }
        }
        
        magma_cgetmatrix( kk, kk, dA(0, 0), ldda, A(0, 0), ldwa );
        
        /*  Use CPU code to reduce the last or only block */
        lapackf77_chetrd(uplo_, &kk, A(0, 0), &ldwa, d, e, tau, work, &lwork, &iinfo);
        
        magma_csetmatrix( kk, kk, A(0, 0), ldwa, dA(0, 0), ldda );
    }
    else {
        /* Reduce the lower triangle of A */
        for (i = 0; i < n-nx; i += nb) {
            /* Reduce columns i:i+nb-1 to tridiagonal form and form the
               matrix W which is needed to update the unreduced part of
               the matrix */
            
            /*   Get the current panel */
            magma_cgetmatrix( n-i, nb, dA(i, i), ldda, A(i, i), ldwa );
            
            magma_clatrd2(uplo, n-i, nb, A(i, i), ldwa, &e[i],
                          &tau[i], work, ldw,
                          dA(i, i), ldda,
                          dwork, lddw,
                          dwork + 2*ldw*nb, ldwork - 2*ldw*nb);
            
            /* Update the unreduced submatrix A(i+ib:n,i+ib:n), using
               an update of the form:  A := A - V*W' - W*V' */
            magma_csetmatrix( n-i, nb, work, ldw, dwork, lddw );
            
            magma_cher2k(MagmaLower, MagmaNoTrans, n-i-nb, nb, c_neg_one,
                         dA(i+nb, i), ldda,
                         &dwork[nb], lddw, d_one,
                         dA(i+nb, i+nb), ldda);
            
            /* Copy subdiagonal elements back into A, and diagonal
               elements into D */
            for (j = i; j < i+nb; ++j) {
                *A(j+1,j) = MAGMA_C_MAKE( e[j], 0 );
                d[j] = MAGMA_C_REAL( *A(j, j) );
            }
        }
        /* Use unblocked code to reduce the last or only block */
        magma_cgetmatrix( n-i, n-i, dA(i, i), ldda, A(i, i), ldwa );
        
        i_n = n-i;
        lapackf77_chetrd(uplo_, &i_n, A(i, i), &ldwa, &d[i], &e[i],
                         &tau[i], work, &lwork, &iinfo);
        
        magma_csetmatrix( n-i, n-i, A(i, i), ldwa, dA(i, i), ldda );
    }
    
    work[0] = MAGMA_C_MAKE( lwkopt, 0 );

    return *info;
} /* magma_chetrd2_gpu */
