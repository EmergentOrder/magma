/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Stan Tomov
       @generated from zgetrf.cpp normal z -> d, Fri Jul 18 17:34:16 2014
*/
#include "common_magma.h"



/**
    Purpose
    -------
    DGETRF computes an LU factorization of a general M-by-N matrix A
    using partial pivoting with row interchanges.  This version does not
    require work space on the GPU passed as input. GPU memory is allocated
    in the routine.

    The factorization has the form
        A = P * L * U
    where P is a permutation matrix, L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.
    If the current stream is NULL, this version replaces it with user defined
    stream to overlap computation with communication.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    A       DOUBLE_PRECISION array, dimension (LDA,N)
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.
    \n
            Higher performance is achieved if A is in pinned memory, e.g.
            allocated using magma_malloc_pinned.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,M).

    @param[out]
    ipiv    INTEGER array, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was interchanged with row IPIV(i).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.
      -     > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.

    @ingroup magma_dgesv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_dgetrf(magma_int_t m, magma_int_t n, double *A, magma_int_t lda,
             magma_int_t *ipiv, magma_int_t *info)
{
#define dAT(i,j) (dAT + (i)*nb*ldda + (j)*nb)

    double *dAT, *dA, *da, *work;
    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;
    magma_int_t     iinfo, nb;

    *info = 0;

    if (m < 0)
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (lda < max(1,m))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        return *info;

    nb = magma_get_dgetrf_nb(m);

    if ( (nb <= 1) || (nb >= min(m,n)) ) {
        /* Use CPU code. */
        lapackf77_dgetrf(&m, &n, A, &lda, ipiv, info);
    } else {
        /* Use hybrid blocked code. */
        magma_int_t maxm, maxn, ldda, maxdim;
        magma_int_t i, rows, cols, s = min(m, n)/nb;
        
        maxm = ((m + 31)/32)*32;
        maxn = ((n + 31)/32)*32;
        maxdim = max(maxm, maxn);

        /* set number of GPUs */
        magma_int_t num_gpus = magma_num_gpus();
        if ( num_gpus > 1 ) {
            /* call multi-GPU non-GPU-resident interface  */
            magma_dgetrf_m(num_gpus, m, n, A, lda, ipiv, info);
            return *info;
        }

        /* explicitly checking the memory requirement */
        size_t freeMem, totalMem;
        cudaMemGetInfo( &freeMem, &totalMem );
        freeMem /= sizeof(double);

        int h = 1+(2+num_gpus), num_gpus2 = num_gpus;
        int NB = (magma_int_t)(0.8*freeMem/maxm-h*nb);
        const char* ngr_nb_char = getenv("MAGMA_NGR_NB");
        if ( ngr_nb_char != NULL )
            NB = max( nb, min( NB, atoi(ngr_nb_char) ) );

        if ( num_gpus > ceil((double)NB/nb) ) {
            num_gpus2 = (int)ceil((double)NB/nb);
            h = 1+(2+num_gpus2);
            NB = (magma_int_t)(0.8*freeMem/maxm-h*nb);
        }
        if ( num_gpus2*NB < n ) {
            /* require too much memory, so call non-GPU-resident version */
            magma_dgetrf_m(num_gpus, m, n, A, lda, ipiv, info);
            return *info;
        }

        ldda = maxn;
        work = A;
        if (maxdim*maxdim < 2*maxm*maxn) {
            // if close to square, allocate square matrix and transpose in-place
            if (MAGMA_SUCCESS != magma_dmalloc( &dA, nb*maxm + maxdim*maxdim )) {
                /* alloc failed so call non-GPU-resident version */
                magma_dgetrf_m(num_gpus, m, n, A, lda, ipiv, info);
                return *info;
            }
            da = dA + nb*maxm;
            
            ldda = maxdim;
            magma_dsetmatrix( m, n, A, lda, da, ldda );
            
            dAT = da;
            magmablas_dtranspose_inplace( ldda, dAT, ldda );
        }
        else {
            // if very rectangular, allocate dA and dAT and transpose out-of-place
            if (MAGMA_SUCCESS != magma_dmalloc( &dA, (nb + maxn)*maxm )) {
                /* alloc failed so call non-GPU-resident version */
                magma_dgetrf_m(num_gpus, m, n, A, lda, ipiv, info);
                return *info;
            }
            da = dA + nb*maxm;
            
            magma_dsetmatrix( m, n, A, lda, da, maxm );
            
            if (MAGMA_SUCCESS != magma_dmalloc( &dAT, maxm*maxn )) {
                /* alloc failed so call non-GPU-resident version */
                magma_free( dA );
                magma_dgetrf_m(num_gpus, m, n, A, lda, ipiv, info);
                return *info;
            }

            magmablas_dtranspose( m, n, da, maxm, dAT, ldda );
        }
        
        lapackf77_dgetrf( &m, &nb, work, &lda, ipiv, &iinfo);

        /* Define user stream if current stream is NULL */
        cudaStream_t stream[2], current_stream;
        magmablasGetKernelStream(&current_stream);

        magma_queue_create( &stream[0] );
        if (current_stream == NULL) {
            magma_queue_create( &stream[1] );
            magmablasSetKernelStream(stream[1]);
        }
        else
            stream[1] = current_stream;

        for( i = 0; i < s; i++ ) {
            // download i-th panel
            cols = maxm - i*nb;
            
            if (i > 0) {
                // download i-th panel
                magmablas_dtranspose( nb, cols, dAT(i,i), ldda, dA, cols );

                // make sure that gpu queue is empty
                magma_device_sync();

                magma_dgetmatrix_async( m-i*nb, nb, dA, cols, work, lda,
                                        stream[0]);
                
                magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                             n - (i+1)*nb, nb,
                             c_one, dAT(i-1,i-1), ldda,
                                    dAT(i-1,i+1), ldda );
                magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                             n-(i+1)*nb, m-i*nb, nb,
                             c_neg_one, dAT(i-1,i+1), ldda,
                                        dAT(i,  i-1), ldda,
                             c_one,     dAT(i,  i+1), ldda );

                // do the cpu part
                rows = m - i*nb;
                magma_queue_sync( stream[0] );
                lapackf77_dgetrf( &rows, &nb, work, &lda, ipiv+i*nb, &iinfo);
            }
            if (*info == 0 && iinfo > 0)
                *info = iinfo + i*nb;

            // upload i-th panel
            magma_dsetmatrix_async( m-i*nb, nb, work, lda, dA, cols,
                                    stream[0]);

            magmablas_dpermute_long2( ldda, dAT, ldda, ipiv, nb, i*nb );

            magma_queue_sync( stream[0] );
            magmablas_dtranspose( cols, nb, dA, cols, dAT(i,i), ldda );

            // do the small non-parallel computations
            if (s > (i+1)) {
                magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                             nb, nb,
                             c_one, dAT(i, i  ), ldda,
                                    dAT(i, i+1), ldda);
                magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                             nb, m-(i+1)*nb, nb,
                             c_neg_one, dAT(i,   i+1), ldda,
                                        dAT(i+1, i  ), ldda,
                             c_one,     dAT(i+1, i+1), ldda );
            }
            else {
                magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                             n-s*nb, nb,
                             c_one, dAT(i, i  ), ldda,
                                    dAT(i, i+1), ldda);
                magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                             n-(i+1)*nb, m-(i+1)*nb, nb,
                             c_neg_one, dAT(i,   i+1), ldda,
                                        dAT(i+1, i  ), ldda,
                             c_one,     dAT(i+1, i+1), ldda );
            }
        }
        
        magma_int_t nb0 = min(m - s*nb, n - s*nb);
        if ( nb0 > 0 ) {
            rows = m - s*nb;
            cols = maxm - s*nb;
    
            magmablas_dtranspose( nb0, rows, dAT(s,s), ldda, dA, cols );
            magma_dgetmatrix( rows, nb0, dA, cols, work, lda );
    
            // make sure that gpu queue is empty
            magma_device_sync();
    
            // do the cpu part
            lapackf77_dgetrf( &rows, &nb0, work, &lda, ipiv+s*nb, &iinfo);
            if (*info == 0 && iinfo > 0)
                *info = iinfo + s*nb;
            magmablas_dpermute_long2( ldda, dAT, ldda, ipiv, nb0, s*nb );
    
            magma_dsetmatrix( rows, nb0, work, lda, dA, cols );
            magmablas_dtranspose( rows, nb0, dA, cols, dAT(s,s), ldda );
    
            magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                         n-s*nb-nb0, nb0,
                         c_one, dAT(s, s),     ldda,
                                dAT(s, s)+nb0, ldda);
        }
       
        if (maxdim*maxdim < 2*maxm*maxn) {
            magmablas_dtranspose_inplace( ldda, dAT, ldda );
            magma_dgetmatrix( m, n, da, ldda, A, lda );
        } else {
            magmablas_dtranspose( n, m, dAT, ldda, da, maxm );
            magma_dgetmatrix( m, n, da, maxm, A, lda );
            magma_free( dAT );
        }

        magma_free( dA );
 
        magma_queue_destroy( stream[0] );
        if (current_stream == NULL) {
            magma_queue_destroy( stream[1] );
            magmablasSetKernelStream(NULL);
        }
    }
    
    return *info;
} /* magma_dgetrf */

#undef dAT
