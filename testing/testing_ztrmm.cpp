/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @precisions normal z -> c d s
       @author Chongxiao Cao
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas_v2.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ztrmm
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, cublas_perf, cublas_time, cpu_perf, cpu_time;
    double          cublas_error, Cnorm, work[1];
    magma_int_t M, N;
    magma_int_t Ak;
    magma_int_t sizeA, sizeB;
    magma_int_t lda, ldb, ldda, lddb;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magmaDoubleComplex *h_A, *h_B, *h_Bcublas;
    magmaDoubleComplex *d_A, *d_B;
    magmaDoubleComplex c_neg_one = MAGMA_Z_NEG_ONE;
    magmaDoubleComplex alpha = MAGMA_Z_MAKE(  0.29, -0.86 );
    magma_int_t status = 0;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    double tol = opts.tolerance * lapackf77_dlamch("E");
    
    printf("If running lapack (option --lapack), CUBLAS error is computed\n"
           "relative to CPU BLAS result.\n\n");
    printf("side = %s, uplo = %s, transA = %s, diag = %s \n",
           lapack_side_const(opts.side), lapack_uplo_const(opts.uplo),
           lapack_trans_const(opts.transA), lapack_diag_const(opts.diag) );
    printf("    M     N   CUBLAS Gflop/s (ms)   CPU Gflop/s (ms)  CUBLAS error\n");
    printf("==================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            gflops = FLOPS_ZTRMM(opts.side, M, N) / 1e9;

            if ( opts.side == MagmaLeft ) {
                lda = M;
                Ak = M;
            } else {
                lda = N;
                Ak = N;
            }
            
            ldb = M;
            
            ldda = ((lda+31)/32)*32;
            lddb = ((ldb+31)/32)*32;
            
            sizeA = lda*Ak;
            sizeB = ldb*N;
            
            TESTING_MALLOC_CPU( h_A,       magmaDoubleComplex, lda*Ak );
            TESTING_MALLOC_CPU( h_B,       magmaDoubleComplex, ldb*N  );
            TESTING_MALLOC_CPU( h_Bcublas, magmaDoubleComplex, ldb*N  );
            
            TESTING_MALLOC_DEV( d_A, magmaDoubleComplex, ldda*Ak );
            TESTING_MALLOC_DEV( d_B, magmaDoubleComplex, lddb*N  );
            
            /* Initialize the matrices */
            lapackf77_zlarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_zlarnv( &ione, ISEED, &sizeB, h_B );
            
            /* =====================================================================
               Performs operation using CUBLAS
               =================================================================== */
            magma_zsetmatrix( Ak, Ak, h_A, lda, d_A, ldda );
            magma_zsetmatrix( M, N, h_B, ldb, d_B, lddb );
            
            // note cublas does trmm out-of-place (i.e., adds output matrix C),
            // but allows C=B to do in-place.
            cublas_time = magma_sync_wtime( NULL );
            cublasZtrmm( handle, cublas_side_const(opts.side), cublas_uplo_const(opts.uplo),
                         cublas_trans_const(opts.transA), cublas_diag_const(opts.diag),
                         M, N, 
                         &alpha, d_A, ldda,
                                 d_B, lddb,
                                 d_B, lddb );
            cublas_time = magma_sync_wtime( NULL ) - cublas_time;
            cublas_perf = gflops / cublas_time;
            
            magma_zgetmatrix( M, N, d_B, lddb, h_Bcublas, ldb );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                blasf77_ztrmm( lapack_side_const(opts.side), lapack_uplo_const(opts.uplo),
                               lapack_trans_const(opts.transA), lapack_diag_const(opts.diag), 
                               &M, &N,
                               &alpha, h_A, &lda,
                                       h_B, &ldb );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                // compute relative error for both magma & cublas, relative to lapack,
                // |C_magma - C_lapack| / |C_lapack|
                Cnorm = lapackf77_zlange( "M", &M, &N, h_B, &ldb, work );
                
                blasf77_zaxpy( &sizeB, &c_neg_one, h_B, &ione, h_Bcublas, &ione );
                cublas_error = lapackf77_zlange( "M", &M, &N, h_Bcublas, &ldb, work ) / Cnorm;
                
                printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)    %8.2e   %s\n",
                       (int) M, (int) N,
                       cublas_perf, 1000.*cublas_time,
                       cpu_perf,    1000.*cpu_time,
                       cublas_error, (cublas_error < tol ? "ok" : "failed"));
                status += ! (cublas_error < tol);
            }
            else {
                printf("%5d %5d   %7.2f (%7.2f)    ---   (  ---  )    ---     ---\n",
                       (int) M, (int) N,
                       cublas_perf, 1000.*cublas_time);
            }
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( h_B );
            TESTING_FREE_CPU( h_Bcublas );
            
            TESTING_FREE_DEV( d_A );
            TESTING_FREE_DEV( d_B );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
