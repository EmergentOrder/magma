/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgeqr2x_gpu-v2.cpp normal z -> d, Fri Jul 18 17:34:16 2014

*/
#include "common_magma.h"
    
/**
    Purpose
    -------
    DGEQR2 computes a QR factorization of a real m by n matrix A:
    A = Q * R.

    This expert routine requires two more arguments than the standard
    dgeqr2, namely, dT and ddA, explained below. The storage for A is
    also not as in the LAPACK's dgeqr2 routine (see below).

    The first is used to output the triangular
    n x n factor T of the block reflector used in the factorization.
    The second holds the diagonal nxn blocks of A, i.e., the diagonal
    submatrices of R. This routine implements the left looking QR.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    dA      DOUBLE_PRECISION array, dimension (LDA,N)
            On entry, the m by n matrix A.
            On exit, the unitary matrix Q as a
            product of elementary reflectors (see Further Details).
    \n
            the elements on and above the diagonal of the array
            contain the min(m,n) by n upper trapezoidal matrix R (R is
            upper triangular if m >= n); the elements below the diagonal,
            with the array TAU, represent the unitary matrix Q as a
            product of elementary reflectors (see Further Details).

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDA >= max(1,M).

    @param[out]
    dtau    DOUBLE_PRECISION array, dimension (min(M,N))
            The scalar factors of the elementary reflectors (see Further
            Details).

    @param[out]
    dT      DOUBLE_PRECISION array, dimension N x N.
            Stores the triangular N x N factor T of the block reflector
            used in the factorization. The lower triangular part is 0.

    @param[out]
    ddA     DOUBLE_PRECISION array, dimension N x N.
            Stores the elements of the upper N x N diagonal block of A.
            LAPACK stores this array in A. There are 0s below the diagonal.

    @param
    dwork   (workspace) DOUBLE_PRECISION array, dimension (3 N)

    @param[out]
    info    INTEGER
      -     = 0: successful exit
      -     < 0: if INFO = -i, the i-th argument had an illegal value

    Further Details
    ---------------
    The matrix Q is represented as a product of elementary reflectors

       Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with
    v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in A(i+1:m,i),
    and tau in TAU(i).

    @ingroup magma_dgeqrf_comp
    ********************************************************************/
extern "C" magma_int_t
magma_dgeqr2x2_gpu(magma_int_t *m, magma_int_t *n, double *dA,
                  magma_int_t *ldda, double *dtau,
                  double *dT, double *ddA,
                  double *dwork, magma_int_t *info)
{
    #define dA(a_1,a_2) (dA + (a_2)*(*ldda) + (a_1))
    
    magma_int_t i, k;
    
    double *work = (double *)dwork;
    double *dnorm = dwork + 4*(*n);


    *info = 0;
    if (*m < 0) {
        *info = -1;
    } else if (*n < 0) {
        *info = -2;
    } else if (*ldda < max(1,*m)) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Compute the norms of the trailing columns */
    k = min(*m,*n);
    magmablas_dnrm2_cols(*m, k, dA(0,0), *ldda, dnorm);

    for (i = 0; i < k; ++i) {
        /*   1. Apply H' to A(:,i) from the left
             2. Adjust the dnorm[i] to hold the norm of A(i:m,i) */
        if (i > 0) {
            magma_dlarfbx_gpu(*m, i, dA(0, 0), *ldda,
                              dT, k, dA(0, i), work);
            magmablas_dnrm2_adjust(i, dnorm+i, dA(0, i));
        }

        /*  Generate elementary reflector H(i) to annihilate A(i+1:m,i)
            1. 1 is not yet put on the diagonal of A
            2. Elements above the diagonal are copied in ddA and the ones
               in A are set to zero
            3. update T                                                  */
        magma_dlarfgtx_gpu(*m-i, dA(i, i), dA(min(i+1,*m), i), dtau+i,
                           dnorm+i, ddA + i + i*(*n), i,
                           dA(i,0), *ldda,  dT, k, work);
    }

    return *info;
} /* magma_dgeqr2 */
