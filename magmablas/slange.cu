/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zlange.cu normal z -> s, Fri Jul 18 17:34:11 2014
       @author Mark Gates
*/
#include "common_magma.h"

/* Computes row sums dwork[i] = sum( abs( A(i,:) )), i=0:m-1, for || A ||_inf,
 * where m and n are any size.
 * Has ceil( m/64 ) blocks of 64 threads. Each thread does one row. */
extern "C" __global__ void
slange_inf_kernel(
    int m, int n, const float *A, int lda, float *dwork )
{
    int i = blockIdx.x*64 + threadIdx.x;
    float Cb[4] = {0, 0, 0, 0};
    int n_mod_4 = n % 4;
    n -= n_mod_4;
    
    // if beyond last row, skip row
    if ( i < m ) {
        A += i;
        
        if ( n >= 4 ) {
            const float *Aend = A + lda*n;
            float rA[4] = { A[0], A[lda], A[2*lda], A[3*lda] };
            A += 4*lda;
            
            while( A < Aend ) {
                Cb[0] += fabsf( rA[0] );  rA[0] = A[0];
                Cb[1] += fabsf( rA[1] );  rA[1] = A[lda];
                Cb[2] += fabsf( rA[2] );  rA[2] = A[2*lda];
                Cb[3] += fabsf( rA[3] );  rA[3] = A[3*lda];
                A += 4*lda;
            }
            
            Cb[0] += fabsf( rA[0] );
            Cb[1] += fabsf( rA[1] );
            Cb[2] += fabsf( rA[2] );
            Cb[3] += fabsf( rA[3] );
        }
    
        /* clean up code */
        switch( n_mod_4 ) {
            case 0:
                break;
    
            case 1:
                Cb[0] += fabsf( A[0] );
                break;
    
            case 2:
                Cb[0] += fabsf( A[0]   );
                Cb[1] += fabsf( A[lda] );
                break;
    
            case 3:
                Cb[0] += fabsf( A[0]     );
                Cb[1] += fabsf( A[lda]   );
                Cb[2] += fabsf( A[2*lda] );
                break;
        }
    
        /* compute final result */
        dwork[i] = Cb[0] + Cb[1] + Cb[2] + Cb[3];
    }
}

/**
    Purpose
    -------
    SLANGE  returns the value of the one norm, or the Frobenius norm, or
    the  infinity norm, or the  element of  largest absolute value  of a
    real matrix A.
    
    Description
    -----------
    SLANGE returns the value
    
       SLANGE = ( max(abs(A(i,j))), NORM = 'M' or 'm'            ** not yet supported
                (
                ( norm1(A),         NORM = '1', 'O' or 'o'       ** not yet supported
                (
                ( normI(A),         NORM = 'I' or 'i'
                (
                ( normF(A),         NORM = 'F', 'f', 'E' or 'e'  ** not yet supported
    
    where norm1 denotes the one norm of a matrix (maximum column sum),
    normI denotes the infinity norm of a matrix (maximum row sum) and
    normF denotes the Frobenius norm of a matrix (square root of sum of
    squares). Note that max(abs(A(i,j))) is not a consistent matrix norm.
    
    Arguments
    ---------
    @param[in]
    norm    CHARACTER*1
            Specifies the value to be returned in SLANGE as described
            above.
    
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.  When M = 0,
            SLANGE is set to zero.
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.  When N = 0,
            SLANGE is set to zero.
    
    @param[in]
    A       REAL array on the GPU, dimension (LDA,N)
            The m by n matrix A.
    
    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(M,1).
    
    @param
    dwork   (workspace) REAL array on the GPU, dimension (MAX(1,LWORK)),
            where LWORK >= M when NORM = 'I'; otherwise, WORK is not
            referenced.

    @ingroup magma_saux2
    ********************************************************************/
extern "C" float
magmablas_slange(
    magma_norm_t norm, magma_int_t m, magma_int_t n,
    const float *A, magma_int_t lda, float *dwork )
{
    magma_int_t info = 0;
    if ( norm != MagmaInfNorm )
        info = -1;
    else if ( m < 0 )
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( lda < m )
        info = -5;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return info;
    }
    
    /* Quick return */
    if ( m == 0 || n == 0 )
        return 0;
    
    dim3 threads( 64 );
    dim3 grid( (m-1)/64 + 1 );
    slange_inf_kernel<<< grid, threads, 0, magma_stream >>>( m, n, A, lda, dwork );
    int i = magma_isamax( m, dwork, 1 ) - 1;
    float res;
    cudaMemcpy( &res, &dwork[i], sizeof(float), cudaMemcpyDeviceToHost );
    return res;
}
