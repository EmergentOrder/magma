/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014
  
       @generated from testing_zpotri_gpu.cpp normal z -> s, Fri Jul 18 17:34:24 2014
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing spotrf
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float *h_A, *h_R;
    float *d_A;
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t N, n2, lda, ldda, info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float      work[1], error;
    magma_int_t status = 0;

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("uplo = %s\n", lapack_uplo_const(opts.uplo) );
    printf("    N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R||_F / ||A||_F\n");
    printf("=================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            n2     = lda*N;
            ldda   = ((N+31)/32)*32;
            gflops = FLOPS_SPOTRI( N ) / 1e9;
            
            TESTING_MALLOC_CPU( h_A, float, n2 );
            TESTING_MALLOC_PIN( h_R, float, n2 );
            TESTING_MALLOC_DEV( d_A, float, ldda*N );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            magma_smake_hpd( N, h_A, lda );
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            /* factorize matrix */
            magma_ssetmatrix( N, N, h_A, lda, d_A, ldda );
            magma_spotrf_gpu( opts.uplo, N, d_A, ldda, &info );
            
            // check for exact singularity
            //magma_sgetmatrix( N, N, d_A, ldda, h_R, lda );
            //h_R[ 10 + 10*lda ] = MAGMA_S_MAKE( 0.0, 0.0 );
            //magma_ssetmatrix( N, N, h_R, lda, d_A, ldda );
            
            gpu_time = magma_wtime();
            magma_spotri_gpu( opts.uplo, N, d_A, ldda, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_spotri_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                lapackf77_spotrf( lapack_uplo_const(opts.uplo), &N, h_A, &lda, &info );
                
                cpu_time = magma_wtime();
                lapackf77_spotri( lapack_uplo_const(opts.uplo), &N, h_A, &lda, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_spotri returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                /* =====================================================================
                   Check the result compared to LAPACK
                   =================================================================== */
                magma_sgetmatrix( N, N, d_A, ldda, h_R, lda );
                error = lapackf77_slange("f", &N, &N, h_A, &lda, work);
                blasf77_saxpy(&n2, &c_neg_one, h_A, &ione, h_R, &ione);
                error = lapackf77_slange("f", &N, &N, h_R, &lda, work) / error;
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
                       error, (error < tol ? "ok" : "failed") );
                status += ! (error < tol);
            }
            else {
                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)     ---\n",
                       (int) N, gpu_perf, gpu_time );
            }
            
            TESTING_FREE_CPU( h_A );
            TESTING_FREE_PIN( h_R );
            TESTING_FREE_DEV( d_A );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
