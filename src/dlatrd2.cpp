/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Raffaele Solca
       @author Stan Tomov

       @generated from zlatrd2.cpp normal z -> d, Fri Jul 18 17:34:18 2014

*/
#include "common_magma.h"

#include <cblas.h>
#include <assert.h>

#define PRECISION_d

/**
    Purpose
    -------
    DLATRD2 reduces NB rows and columns of a real symmetric matrix A to
    symmetric tridiagonal form by an orthogonal similarity
    transformation Q' * A * Q, and returns the matrices V and W which are
    needed to apply the transformation to the unreduced part of A.

    If UPLO = MagmaUpper, DLATRD reduces the last NB rows and columns of a
    matrix, of which the upper triangle is supplied;
    if UPLO = MagmaLower, DLATRD reduces the first NB rows and columns of a
    matrix, of which the lower triangle is supplied.

    This is an auxiliary routine called by DSYTRD2_GPU. It uses an
    accelerated HEMV that needs extra memory.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies whether the upper or lower triangular part of the
            symmetric matrix A is stored:
      -     = MagmaUpper: Upper triangular
      -     = MagmaLower: Lower triangular

    @param[in]
    n       INTEGER
            The order of the matrix A.

    @param[in]
    nb      INTEGER
            The number of rows and columns to be reduced.

    @param[in,out]
    A       DOUBLE_PRECISION array, dimension (LDA,N)
            On entry, the symmetric matrix A.  If UPLO = MagmaUpper, the leading
            n-by-n upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = MagmaLower, the
            leading n-by-n lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
            On exit:
      -     if UPLO = MagmaUpper, the last NB columns have been reduced to
              tridiagonal form, with the diagonal elements overwriting
              the diagonal elements of A; the elements above the diagonal
              with the array TAU, represent the orthogonal matrix Q as a
              product of elementary reflectors;
      -     if UPLO = MagmaLower, the first NB columns have been reduced to
              tridiagonal form, with the diagonal elements overwriting
              the diagonal elements of A; the elements below the diagonal
              with the array TAU, represent the  orthogonal matrix Q as a
              product of elementary reflectors.
            See Further Details.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= (1,N).

    @param[out]
    e       DOUBLE_PRECISION array, dimension (N-1)
            If UPLO = MagmaUpper, E(n-nb:n-1) contains the superdiagonal
            elements of the last NB columns of the reduced matrix;
            if UPLO = MagmaLower, E(1:nb) contains the subdiagonal elements of
            the first NB columns of the reduced matrix.

    @param[out]
    tau     DOUBLE_PRECISION array, dimension (N-1)
            The scalar factors of the elementary reflectors, stored in
            TAU(n-nb:n-1) if UPLO = MagmaUpper, and in TAU(1:nb) if UPLO = MagmaLower.
            See Further Details.

    @param[out]
    W       DOUBLE_PRECISION array, dimension (LDW,NB)
            The n-by-nb matrix W required to update the unreduced part
            of A.

    @param[in]
    ldw     INTEGER
            The leading dimension of the array W. LDW >= max(1,N).

    Further Details
    ---------------
    If UPLO = MagmaUpper, the matrix Q is represented as a product of elementary
    reflectors

        Q = H(n) H(n-1) . . . H(n-nb+1).

    Each H(i) has the form

        H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with
    v(i:n) = 0 and v(i-1) = 1; v(1:i-1) is stored on exit in A(1:i-1,i),
    and tau in TAU(i-1).

    If UPLO = MagmaLower, the matrix Q is represented as a product of elementary
    reflectors

        Q = H(1) H(2) . . . H(nb).

    Each H(i) has the form

        H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with
    v(1:i) = 0 and v(i+1) = 1; v(i+1:n) is stored on exit in A(i+1:n,i),
    and tau in TAU(i).

    The elements of the vectors v together form the n-by-nb matrix V
    which is needed, with W, to apply the transformation to the unreduced
    part of the matrix, using a symmetric rank-2k update of the form:
    A := A - V*W' - W*V'.

    The contents of A on exit are illustrated by the following examples
    with n = 5 and nb = 2:

    if UPLO = MagmaUpper:                       if UPLO = MagmaLower:

        (  a   a   a   v4  v5 )              (  d                  )
        (      a   a   v4  v5 )              (  1   d              )
        (          a   1   v5 )              (  v1  1   a          )
        (              d   1  )              (  v1  v2  a   a      )
        (                  d  )              (  v1  v2  a   a   a  )

    where d denotes a diagonal element of the reduced matrix, a denotes
    an element of the original matrix that is unchanged, and vi denotes
    an element of the vector defining H(i).

    @ingroup magma_dsyev_aux
    ********************************************************************/
extern "C" magma_int_t
magma_dlatrd2(magma_uplo_t uplo, magma_int_t n, magma_int_t nb,
              double *A,  magma_int_t lda,
              double *e, double *tau,
              double *W,  magma_int_t ldw,
              double *dA, magma_int_t ldda,
              double *dW, magma_int_t lddw,
              double *dwork, magma_int_t ldwork)
{
#define A(i, j) (A + (j)*lda + (i))
#define W(i, j) (W + (j)*ldw + (i))

#define dA(i, j) (dA + (j)*ldda + (i))
#define dW(i, j) (dW + (j)*lddw + (i))

    magma_int_t i;
    
    double c_neg_one = MAGMA_D_NEG_ONE;
    double c_one     = MAGMA_D_ONE;
    double c_zero    = MAGMA_D_ZERO;

    double value = MAGMA_D_ZERO;
    
    magma_int_t ione = 1;

    magma_int_t i_n, i_1, iw;
    
    double alpha;
    double *f;

    if (n <= 0) {
        return 0;
    }

    magma_queue_t stream;
    magma_queue_create( &stream );
    magma_dmalloc_cpu( &f, n );
    assert( f != NULL );  // TODO return error, or allocate outside dlatrd
    
    if (uplo == MagmaUpper) {
        /* Reduce last NB columns of upper triangle */
        for (i = n-1; i >= n - nb; --i) {
            i_1 = i + 1;
            i_n = n - i - 1;
            
            iw = i - n + nb;
            if (i < n-1) {
                /* Update A(1:i,i) */
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_dlacgv(&i_n, W(i, iw+1), &ldw);
                #endif
                blasf77_dgemv("No transpose", &i_1, &i_n, &c_neg_one, A(0, i+1), &lda,
                              W(i, iw+1), &ldw, &c_one, A(0, i), &ione);
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_dlacgv(&i_n, W(i, iw+1), &ldw);
                lapackf77_dlacgv(&i_n, A(i, i+1), &ldw);
                #endif
                blasf77_dgemv("No transpose", &i_1, &i_n, &c_neg_one, W(0, iw+1), &ldw,
                              A(i, i+1), &lda, &c_one, A(0, i), &ione);
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_dlacgv(&i_n, A(i, i+1), &ldw);
                #endif
            }
            if (i > 0) {
                /* Generate elementary reflector H(i) to annihilate A(1:i-2,i) */
                
                alpha = *A(i-1, i);
                
                lapackf77_dlarfg(&i, &alpha, A(0, i), &ione, &tau[i - 1]);
                
                e[i-1] = MAGMA_D_REAL( alpha );
                *A(i-1,i) = MAGMA_D_MAKE( 1, 0 );
                
                /* Compute W(1:i-1,i) */
                // 1. Send the block reflector  A(0:n-i-1,i) to the GPU
                magma_dsetvector( i, A(0, i), 1, dA(0, i), 1 );
                
                //#if (GPUSHMEM < 200)
                //magma_dsymv(MagmaUpper, i, c_one, dA(0, 0), ldda,
                //            dA(0, i), ione, c_zero, dW(0, iw), ione);
                //#else
                magmablas_dsymv_work(MagmaUpper, i, c_one, dA(0, 0), ldda,
                                     dA(0, i), ione, c_zero, dW(0, iw), ione,
                                     dwork, ldwork);
                //#endif
                
                // 2. Start putting the result back (asynchronously)
                magma_dgetmatrix_async( i, 1,
                                        dW(0, iw),         lddw,
                                        W(0, iw) /*test*/, ldw, stream );
                
                if (i < n-1) {
                    blasf77_dgemv(MagmaTransStr, &i, &i_n, &c_one, W(0, iw+1), &ldw,
                                  A(0, i), &ione, &c_zero, W(i+1, iw), &ione);
                }
                
                // 3. Here is where we need it // TODO find the right place
                magma_queue_sync( stream );
                
                if (i < n-1) {
                    blasf77_dgemv("No transpose", &i, &i_n, &c_neg_one, A(0, i+1), &lda,
                                  W(i+1, iw), &ione, &c_one, W(0, iw), &ione);
                    
                    blasf77_dgemv(MagmaTransStr, &i, &i_n, &c_one, A(0, i+1), &lda,
                                  A(0, i), &ione, &c_zero, W(i+1, iw), &ione);
                    
                    blasf77_dgemv("No transpose", &i, &i_n, &c_neg_one, W(0, iw+1), &ldw,
                                  W(i+1, iw), &ione, &c_one, W(0, iw), &ione);
                }
                
                blasf77_dscal(&i, &tau[i - 1], W(0, iw), &ione);
                
                #if defined(PRECISION_z) || defined(PRECISION_c)
                cblas_ddot_sub( i, W(0,iw), ione, A(0,i), ione, &value );
                #else
                value = cblas_ddot( i, W(0,iw), ione, A(0,i), ione );
                #endif
                alpha = tau[i - 1] * -0.5f * value;
                blasf77_daxpy(&i, &alpha, A(0, i), &ione,
                              W(0, iw), &ione);
            }
        }
    }
    else {
        /*  Reduce first NB columns of lower triangle */
        for (i = 0; i < nb; ++i) {
            
            /* Update A(i:n,i) */
            i_n = n - i;
            #if defined(PRECISION_z) || defined(PRECISION_c)
            lapackf77_dlacgv(&i, W(i, 0), &ldw);
            #endif
            blasf77_dgemv("No transpose", &i_n, &i, &c_neg_one, A(i, 0), &lda,
                          W(i, 0), &ldw, &c_one, A(i, i), &ione);
            #if defined(PRECISION_z) || defined(PRECISION_c)
            lapackf77_dlacgv(&i, W(i, 0), &ldw);
            lapackf77_dlacgv(&i, A(i, 0), &lda);
            #endif
            blasf77_dgemv("No transpose", &i_n, &i, &c_neg_one, W(i, 0), &ldw,
                          A(i, 0), &lda, &c_one, A(i, i), &ione);
            #if defined(PRECISION_z) || defined(PRECISION_c)
            lapackf77_dlacgv(&i, A(i, 0), &lda);
            #endif
        
            if (i < n-1) {
                /* Generate elementary reflector H(i) to annihilate A(i+2:n,i) */
                i_n = n - i - 1;
                alpha = *A(i+1, i);
                lapackf77_dlarfg(&i_n, &alpha, A(min(i+2,n-1), i), &ione, &tau[i]);
                e[i] = MAGMA_D_REAL( alpha );
                *A(i+1,i) = MAGMA_D_MAKE( 1, 0 );
        
                /* Compute W(i+1:n,i) */
                // 1. Send the block reflector  A(i+1:n,i) to the GPU
                magma_dsetvector( i_n, A(i+1, i), 1, dA(i+1, i), 1 );
            
                //#if (GPUSHMEM < 200)
                //magma_dsymv(MagmaLower, i_n, c_one, dA(i+1, i+1), ldda, dA(i+1, i), ione, c_zero,
                //            dW(i+1, i), ione);
                //#else
                magmablas_dsymv_work(MagmaLower, i_n, c_one, dA(i+1, i+1), ldda, dA(i+1, i), ione, c_zero,
                                     dW(i+1, i), ione,
                                     dwork, ldwork);
                //#endif
        
                // 2. Start putting the result back (asynchronously)
                magma_dgetmatrix_async( i_n, 1,
                                        dW(i+1, i), lddw,
                                        W(i+1, i),  ldw, stream );
        
                blasf77_dgemv(MagmaTransStr, &i_n, &i, &c_one, W(i+1, 0), &ldw,
                              A(i+1, i), &ione, &c_zero, W(0, i), &ione);
            
                blasf77_dgemv("No transpose", &i_n, &i, &c_neg_one, A(i+1, 0), &lda,
                              W(0, i), &ione, &c_zero, f, &ione);
                
                blasf77_dgemv(MagmaTransStr, &i_n, &i, &c_one, A(i+1, 0), &lda,
                              A(i+1, i), &ione, &c_zero, W(0, i), &ione);
        
                // 3. Here is where we need it
                magma_queue_sync( stream );
        
                if (i != 0)
                    blasf77_daxpy(&i_n, &c_one, f, &ione, W(i+1, i), &ione);
        
                blasf77_dgemv("No transpose", &i_n, &i, &c_neg_one, W(i+1, 0), &ldw,
                              W(0, i), &ione, &c_one, W(i+1, i), &ione);
                blasf77_dscal(&i_n, &tau[i], W(i+1,i), &ione);
                #if defined(PRECISION_z) || defined(PRECISION_c)
                cblas_ddot_sub( i_n, W(i+1,i), ione, A(i+1,i), ione, &value );
                #else
                value = cblas_ddot( i_n, W(i+1,i), ione, A(i+1,i), ione );
                #endif
                alpha = tau[i] * -0.5f * value;
                blasf77_daxpy(&i_n, &alpha, A(i+1, i), &ione, W(i+1,i), &ione);
            }
        }
    }

    magma_free_cpu(f);
    magma_queue_destroy( stream );

    return 0;
} /* magma_dlatrd */
