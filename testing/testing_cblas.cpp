/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from testing_zblas.cpp normal z -> c, Fri Jul 18 17:34:23 2014
       @author Mark Gates
       
       These tests ensure that the MAGMA wrappers around CUBLAS calls are
       correct, for example,
       magma_ctrmm(...) and cublasCtrmm(...) produce /exactly/ the same results.
       It tests all combinations of options (trans, uplo, ...) for each size.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas_v2.h>

// make sure that asserts are enabled
#undef NDEBUG
#include <assert.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define A(i,j)  &A[  (i) + (j)*ld ]
#define dA(i,j) &dA[ (i) + (j)*ld ]
#define dB(i,j) &dB[ (i) + (j)*ld ]
#define C2(i,j) &C2[ (i) + (j)*ld ]
#define LU(i,j) &LU[ (i) + (j)*ld ]

int main( int argc, char** argv )
{
    TESTING_INIT();
    
    real_Double_t   gflops, t1, t2;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magma_int_t ione = 1;
    magma_trans_t trans[] = { MagmaNoTrans, MagmaConjTrans, MagmaTrans };
    magma_uplo_t  uplo [] = { MagmaLower, MagmaUpper };
    magma_diag_t  diag [] = { MagmaUnit, MagmaNonUnit };
    magma_side_t  side [] = { MagmaLeft, MagmaRight };
    
    magmaFloatComplex  *A,  *B,  *C,   *C2, *LU;
    magmaFloatComplex *dA, *dB, *dC1, *dC2;
    magmaFloatComplex alpha = MAGMA_C_MAKE( 0.5, 0.1 );
    magmaFloatComplex beta  = MAGMA_C_MAKE( 0.7, 0.2 );
    float dalpha = 0.6;
    float dbeta  = 0.8;
    float work[1], error, total_error;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t m, n, k, size, maxn, ld, info;
    magma_int_t *piv;
    magma_int_t err;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf( "Compares magma wrapper function to cublas function; all diffs should be exactly 0.\n\n" );
    
    total_error = 0.;
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        m = opts.msize[itest];
        n = opts.nsize[itest];
        k = opts.ksize[itest];
        printf("=========================================================================\n");
        printf( "m=%d, n=%d, k=%d\n", (int) m, (int) n, (int) k );
        
        // allocate matrices
        // over-allocate so they can be any combination of {m,n,k} x {m,n,k}.
        maxn = max( max( m, n ), k );
        ld = max( 1, maxn );
        size = ld*maxn;
        err = magma_malloc_cpu( (void**) &piv, maxn*sizeof(magma_int_t) );  assert( err == 0 );
        err = magma_cmalloc_pinned( &A,  size );  assert( err == 0 );
        err = magma_cmalloc_pinned( &B,  size );  assert( err == 0 );
        err = magma_cmalloc_pinned( &C,  size );  assert( err == 0 );
        err = magma_cmalloc_pinned( &C2, size );  assert( err == 0 );
        err = magma_cmalloc_pinned( &LU, size );  assert( err == 0 );
        err = magma_cmalloc( &dA,  size );        assert( err == 0 );
        err = magma_cmalloc( &dB,  size );        assert( err == 0 );
        err = magma_cmalloc( &dC1, size );        assert( err == 0 );
        err = magma_cmalloc( &dC2, size );        assert( err == 0 );
        
        // initialize matrices
        size = maxn*maxn;
        lapackf77_clarnv( &ione, ISEED, &size, A  );
        lapackf77_clarnv( &ione, ISEED, &size, B  );
        lapackf77_clarnv( &ione, ISEED, &size, C  );
        
        printf( "========== Level 1 BLAS ==========\n" );
        
        // ----- test CSWAP
        // swap columns 2 and 3 of dA, then copy to C2 and compare with A
        if ( n >= 3 ) {
            magma_csetmatrix( m, n, A, ld, dA, ld );
            magma_csetmatrix( m, n, A, ld, dB, ld );
            magma_cswap( m, dA(0,1), 1, dA(0,2), 1 );
            magma_cswap( m, dB(0,1), 1, dB(0,2), 1 );
            
            // check results, storing diff between magma and cuda calls in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dA, 1, dB, 1 );
            magma_cgetmatrix( m, n, dB, ld, C2, ld );
            error = lapackf77_clange( "F", &m, &k, C2, &ld, work );
            total_error += error;
            printf( "cswap             diff %.2g\n", error );
        }
        else {
            printf( "cswap skipped for n < 3\n" );
        }
        
        // ----- test ICAMAX
        // get argmax of column of A
        magma_csetmatrix( m, k, A, ld, dA, ld );
        error = 0;
        for( int j = 0; j < k; ++j ) {
            magma_int_t i1 = magma_icamax( m, dA(0,j), 1 );
            int i2;  // NOT magma_int_t, for cublas
            cublasIcamax( handle, m, dA(0,j), 1, &i2 );
            // todo need sync here?
            assert( i1 == i2 );
            error += abs( i1 - i2 );
        }
        total_error += error;
        gflops = (float)m * k / 1e9;
        printf( "icamax            diff %.2g\n", error );
        printf( "\n" );
        
        printf( "========== Level 2 BLAS ==========\n" );
        
        // ----- test CGEMV
        // c = alpha*A*b + beta*c,  with A m*n; b,c m or n-vectors
        // try no-trans/trans
        for( int ia = 0; ia < 3; ++ia ) {
            magma_csetmatrix( m, n, A,  ld, dA,  ld );
            magma_csetvector( maxn, B, 1, dB,  1 );
            magma_csetvector( maxn, C, 1, dC1, 1 );
            magma_csetvector( maxn, C, 1, dC2, 1 );
            
            t1 = magma_sync_wtime( 0 );
            magma_cgemv( trans[ia], m, n, alpha, dA, ld, dB, 1, beta, dC1, 1 );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCgemv( handle, cublas_trans_const(trans[ia]),
                         m, n, &alpha, dA, ld, dB, 1, &beta, dC2, 1 );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            size = (trans[ia] == MagmaNoTrans ? m : n);
            cublasCaxpy( handle, size, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetvector( size, dC2, 1, C2, 1 );
            error = lapackf77_clange( "F", &size, &ione, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CGEMV( m, n ) / 1e9;
            printf( "cgemv( %c )        diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_trans_const(trans[ia]), error, gflops/t1, gflops/t2 );
        }
        printf( "\n" );
        
        // ----- test CHEMV
        // c = alpha*A*b + beta*c,  with A m*m symmetric; b,c m-vectors
        // try upper/lower
        for( int iu = 0; iu < 2; ++iu ) {
            magma_csetmatrix( m, m, A, ld, dA, ld );
            magma_csetvector( m, B, 1, dB,  1 );
            magma_csetvector( m, C, 1, dC1, 1 );
            magma_csetvector( m, C, 1, dC2, 1 );
            
            t1 = magma_sync_wtime( 0 );
            magma_chemv( uplo[iu], m, alpha, dA, ld, dB, 1, beta, dC1, 1 );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasChemv( handle, cublas_uplo_const(uplo[iu]),
                         m, &alpha, dA, ld, dB, 1, &beta, dC2, 1 );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, m, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetvector( m, dC2, 1, C2, 1 );
            error = lapackf77_clange( "F", &m, &ione, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CHEMV( m ) / 1e9;
            printf( "chemv( %c )        diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), error, gflops/t1, gflops/t2 );
        }
        printf( "\n" );
        
        // ----- test CTRSV
        // solve A*c = c,  with A m*m triangular; c m-vector
        // try upper/lower, no-trans/trans, unit/non-unit diag
        // Factor A into LU to get well-conditioned triangles, else solve yields garbage.
        // Still can give garbage if solves aren't consistent with LU factors,
        // e.g., using unit diag for U, so copy lower triangle to upper triangle.
        // Also used for trsm later.
        lapackf77_clacpy( "Full", &maxn, &maxn, A, &ld, LU, &ld );
        lapackf77_cgetrf( &maxn, &maxn, LU, &ld, piv, &info );
        for( int j = 0; j < maxn; ++j ) {
            for( int i = 0; i < j; ++i ) {
                *LU(i,j) = *LU(j,i);
            }
        }
        for( int iu = 0; iu < 2; ++iu ) {
        for( int it = 0; it < 3; ++it ) {
        for( int id = 0; id < 2; ++id ) {
            magma_csetmatrix( m, m, LU, ld, dA, ld );
            magma_csetvector( m, C, 1, dC1, 1 );
            magma_csetvector( m, C, 1, dC2, 1 );
            
            t1 = magma_sync_wtime( 0 );
            magma_ctrsv( uplo[iu], trans[it], diag[id], m, dA, ld, dC1, 1 );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCtrsv( handle, cublas_uplo_const(uplo[iu]), cublas_trans_const(trans[it]),
                         cublas_diag_const(diag[id]), m, dA, ld, dC2, 1 );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, m, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetvector( m, dC2, 1, C2, 1 );
            error = lapackf77_clange( "F", &m, &ione, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CTRSM( MagmaLeft, m, 1 ) / 1e9;
            printf( "ctrsv( %c, %c, %c )  diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), lapacke_trans_const(trans[it]), lapacke_diag_const(diag[id]),
                    error, gflops/t1, gflops/t2 );
        }}}
        printf( "\n" );
        
        printf( "========== Level 3 BLAS ==========\n" );
        
        // ----- test CGEMM
        // C = alpha*A*B + beta*C,  with A m*k or k*m; B k*n or n*k; C m*n
        // try combinations of no-trans/trans
        for( int ia = 0; ia < 3; ++ia ) {
        for( int ib = 0; ib < 3; ++ib ) {
            bool nta = (trans[ia] == MagmaNoTrans);
            bool ntb = (trans[ib] == MagmaNoTrans);
            magma_csetmatrix( (nta ? m : k), (nta ? m : k), A, ld, dA,  ld );
            magma_csetmatrix( (ntb ? k : n), (ntb ? n : k), B, ld, dB,  ld );
            magma_csetmatrix( m, n, C, ld, dC1, ld );
            magma_csetmatrix( m, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_cgemm( trans[ia], trans[ib], m, n, k, alpha, dA, ld, dB, ld, beta, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCgemm( handle, cublas_trans_const(trans[ia]), cublas_trans_const(trans[ib]),
                         m, n, k, &alpha, dA, ld, dB, ld, &beta, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( m, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &m, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CGEMM( m, n, k ) / 1e9;
            printf( "cgemm( %c, %c )     diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_trans_const(trans[ia]), lapacke_trans_const(trans[ib]),
                    error, gflops/t1, gflops/t2 );
        }}
        printf( "\n" );
        
        // ----- test CHEMM
        // C = alpha*A*B + beta*C  (left)  with A m*m symmetric; B,C m*n; or
        // C = alpha*B*A + beta*C  (right) with A n*n symmetric; B,C m*n
        // try left/right, upper/lower
        for( int is = 0; is < 2; ++is ) {
        for( int iu = 0; iu < 2; ++iu ) {
            magma_csetmatrix( m, m, A, ld, dA,  ld );
            magma_csetmatrix( m, n, B, ld, dB,  ld );
            magma_csetmatrix( m, n, C, ld, dC1, ld );
            magma_csetmatrix( m, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_chemm( side[is], uplo[iu], m, n, alpha, dA, ld, dB, ld, beta, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasChemm( handle, cublas_side_const(side[is]), cublas_uplo_const(uplo[iu]),
                         m, n, &alpha, dA, ld, dB, ld, &beta, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( m, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &m, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CHEMM( side[is], m, n ) / 1e9;
            printf( "chemm( %c, %c )     diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_side_const(side[is]), lapacke_uplo_const(uplo[iu]),
                    error, gflops/t1, gflops/t2 );
        }}
        printf( "\n" );
        
        // ----- test CHERK
        // C = alpha*A*A^H + beta*C  (no-trans) with A m*k and C m*m symmetric; or
        // C = alpha*A^H*A + beta*C  (trans)    with A k*m and C m*m symmetric
        // try upper/lower, no-trans/trans
        for( int iu = 0; iu < 2; ++iu ) {
        for( int it = 0; it < 3; ++it ) {
            magma_csetmatrix( n, k, A, ld, dA,  ld );
            magma_csetmatrix( n, n, C, ld, dC1, ld );
            magma_csetmatrix( n, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_cherk( uplo[iu], trans[it], n, k, dalpha, dA, ld, dbeta, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCherk( handle, cublas_uplo_const(uplo[iu]), cublas_trans_const(trans[it]),
                         n, k, &dalpha, dA, ld, &dbeta, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( n, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &n, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CHERK( k, n ) / 1e9;
            printf( "cherk( %c, %c )     diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), lapacke_trans_const(trans[it]),
                    error, gflops/t1, gflops/t2 );
        }}
        printf( "\n" );
        
        // ----- test CHER2K
        // C = alpha*A*B^H + ^alpha*B*A^H + beta*C  (no-trans) with A,B n*k; C n*n symmetric; or
        // C = alpha*A^H*B + ^alpha*B^H*A + beta*C  (trans)    with A,B k*n; C n*n symmetric
        // try upper/lower, no-trans/trans
        for( int iu = 0; iu < 2; ++iu ) {
        for( int it = 0; it < 3; ++it ) {
            bool nt = (trans[it] == MagmaNoTrans);
            magma_csetmatrix( (nt ? n : k), (nt ? n : k), A, ld, dA,  ld );
            magma_csetmatrix( n, n, C, ld, dC1, ld );
            magma_csetmatrix( n, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_cher2k( uplo[iu], trans[it], n, k, alpha, dA, ld, dB, ld, dbeta, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCher2k( handle, cublas_uplo_const(uplo[iu]), cublas_trans_const(trans[it]),
                          n, k, &alpha, dA, ld, dB, ld, &dbeta, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( n, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &n, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CHER2K( k, n ) / 1e9;
            printf( "cher2k( %c, %c )    diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), lapacke_trans_const(trans[it]),
                    error, gflops/t1, gflops/t2 );
        }}
        printf( "\n" );
        
        // ----- test CTRMM
        // C = alpha*A*C  (left)  with A m*m triangular; C m*n; or
        // C = alpha*C*A  (right) with A n*n triangular; C m*n
        // try left/right, upper/lower, no-trans/trans, unit/non-unit
        for( int is = 0; is < 2; ++is ) {
        for( int iu = 0; iu < 2; ++iu ) {
        for( int it = 0; it < 3; ++it ) {
        for( int id = 0; id < 2; ++id ) {
            bool left = (side[is] == MagmaLeft);
            magma_csetmatrix( (left ? m : n), (left ? m : n), A, ld, dA,  ld );
            magma_csetmatrix( m, n, C, ld, dC1, ld );
            magma_csetmatrix( m, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_ctrmm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            // note cublas does trmm out-of-place (i.e., adds output matrix C),
            // but allows C=B to do in-place.
            t2 = magma_sync_wtime( 0 );
            cublasCtrmm( handle, cublas_side_const(side[is]), cublas_uplo_const(uplo[iu]),
                         cublas_trans_const(trans[it]), cublas_diag_const(diag[id]),
                         m, n, &alpha, dA, ld, dC2, ld, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( m, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &n, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CTRMM( side[is], m, n ) / 1e9;
            printf( "ctrmm( %c, %c )     diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), lapacke_trans_const(trans[it]),
                    error, gflops/t1, gflops/t2 );
        }}}}
        printf( "\n" );
        
        // ----- test CTRSM
        // solve A*X = alpha*B  (left)  with A m*m triangular; B m*n; or
        // solve X*A = alpha*B  (right) with A n*n triangular; B m*n
        // try left/right, upper/lower, no-trans/trans, unit/non-unit
        for( int is = 0; is < 2; ++is ) {
        for( int iu = 0; iu < 2; ++iu ) {
        for( int it = 0; it < 3; ++it ) {
        for( int id = 0; id < 2; ++id ) {
            bool left = (side[is] == MagmaLeft);
            magma_csetmatrix( (left ? m : n), (left ? m : n), LU, ld, dA,  ld );
            magma_csetmatrix( m, n, C, ld, dC1, ld );
            magma_csetmatrix( m, n, C, ld, dC2, ld );
            
            t1 = magma_sync_wtime( 0 );
            magma_ctrsm( side[is], uplo[iu], trans[it], diag[id], m, n, alpha, dA, ld, dC1, ld );
            t1 = magma_sync_wtime( 0 ) - t1;
            
            t2 = magma_sync_wtime( 0 );
            cublasCtrsm( handle, cublas_side_const(side[is]), cublas_uplo_const(uplo[iu]),
                         cublas_trans_const(trans[it]), cublas_diag_const(diag[id]),
                         m, n, &alpha, dA, ld, dC2, ld );
            t2 = magma_sync_wtime( 0 ) - t2;
            
            // check results, storing diff between magma and cuda call in C2
            cublasCaxpy( handle, ld*n, &c_neg_one, dC1, 1, dC2, 1 );
            magma_cgetmatrix( m, n, dC2, ld, C2, ld );
            error = lapackf77_clange( "F", &n, &n, C2, &ld, work );
            total_error += error;
            gflops = FLOPS_CTRSM( side[is], m, n ) / 1e9;
            printf( "ctrsm( %c, %c )     diff %.2g,  Gflop/s %7.2f, %7.2f\n",
                    lapacke_uplo_const(uplo[iu]), lapacke_trans_const(trans[it]),
                    error, gflops/t1, gflops/t2 );
        }}}}
        printf( "\n" );
        
        // cleanup
        magma_free_cpu( piv );
        magma_free_pinned( A  );
        magma_free_pinned( B  );
        magma_free_pinned( C  );
        magma_free_pinned( C2 );
        magma_free_pinned( LU );
        magma_free( dA  );
        magma_free( dB  );
        magma_free( dC1 );
        magma_free( dC2 );
    }
    
    if ( total_error != 0. ) {
        printf( "total error %.2g -- ought to be 0 -- some test failed (see above).\n",
                total_error );
    }
    else {
        printf( "all tests passed\n" );
    }
    
    TESTING_FINALIZE();
    
    int status = (total_error != 0.);
    return status;
}
