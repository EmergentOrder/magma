/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Stan Tomov

       @generated from testing_zhetrd_mgpu.cpp normal z -> c, Fri Jul 18 17:34:25 2014

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

#define PRECISION_c

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing chetrd
*/

int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, cpu_perf, gpu_time, cpu_time;
    magmaFloatComplex *h_A, *h_R, *h_Q, *h_work, *work;
    magmaFloatComplex *tau;
    float *diag, *offdiag, *rwork;
    float result[2] = {0., 0.};
    magma_int_t N, n2, lda, lwork, info, nb;
    magma_int_t ione     = 1;
    magma_int_t itwo     = 2;
    magma_int_t ithree   = 3;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;
    magma_int_t k = 1;  // TODO: UNKNOWN, UNDOCUMENTED VARIABLE (number of streams?)

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    float tol = opts.tolerance * lapackf77_slamch("E");
    float eps = lapackf77_slamch( "E" );

    /* To avoid uninitialized variable warning */
    h_Q   = NULL;
    work  = NULL;
    rwork = NULL;

    printf("uplo = %s\n", lapack_uplo_const(opts.uplo) );
    printf("  N     CPU GFlop/s (sec)   GPU GFlop/s (sec)   |A-QHQ'|/N|A|   |I-QQ'|/N\n");
    printf("===========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N      = opts.nsize[itest];
            lda    = N;
            n2     = N*lda;
            nb     = magma_get_chetrd_nb(N);
            /* We suppose the magma nb is bigger than lapack nb */
            lwork  = N*nb;
            gflops = FLOPS_CHETRD( N ) / 1e9;
            
            /* Allocate host memory for the matrix */
            TESTING_MALLOC_PIN( h_R,     magmaFloatComplex, lda*N );
            TESTING_MALLOC_PIN( h_work,  magmaFloatComplex, lwork );
            
            TESTING_MALLOC_CPU( h_A,     magmaFloatComplex, lda*N );
            TESTING_MALLOC_CPU( tau,     magmaFloatComplex, N     );
            TESTING_MALLOC_CPU( diag,    float, N   );
            TESTING_MALLOC_CPU( offdiag, float, N-1 );
            
            if ( opts.check ) {
                TESTING_MALLOC_CPU( h_Q,  magmaFloatComplex, lda*N );
                TESTING_MALLOC_CPU( work, magmaFloatComplex, 2*N*N );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_MALLOC_CPU( rwork, float, N );
                #endif
            }
        
            /* ====================================================================
               Initialize the matrix
               =================================================================== */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            magma_cmake_hermitian( N, h_A, lda );
            
            lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            if( opts.ngpu == 0 ) {
                magma_chetrd(opts.uplo, N, h_R, lda, diag, offdiag,
                             tau, h_work, lwork, &info);
            } else {
                magma_chetrd_mgpu(opts.ngpu, k, opts.uplo, N, h_R, lda, diag, offdiag,
                                  tau, h_work, lwork, &info);
            }
            gpu_time = magma_wtime() - gpu_time;
            if ( info != 0 )
                printf("magma_chetrd returned error %d\n", (int) info);
            
            gpu_perf = gflops / gpu_time;
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.check ) {
                lapackf77_clacpy( lapack_uplo_const(opts.uplo), &N, &N, h_R, &lda, h_Q, &lda);
                lapackf77_cungtr( lapack_uplo_const(opts.uplo), &N, h_Q, &lda, tau, h_work, &lwork, &info);

#if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_chet21(&itwo, lapack_uplo_const(opts.uplo), &N, &ione,
                                 h_A, &lda, diag, offdiag,
                                 h_Q, &lda, h_R, &lda,
                                 tau, work, rwork, &result[0]);
                
                lapackf77_chet21(&ithree, lapack_uplo_const(opts.uplo), &N, &ione,
                                 h_A, &lda, diag, offdiag,
                                 h_Q, &lda, h_R, &lda,
                                 tau, work, rwork, &result[1]);
#else
                lapackf77_chet21(&itwo, lapack_uplo_const(opts.uplo), &N, &ione,
                                 h_A, &lda, diag, offdiag,
                                 h_Q, &lda, h_R, &lda,
                                 tau, work, &result[0]);
                
                lapackf77_chet21(&ithree, lapack_uplo_const(opts.uplo), &N, &ione,
                                 h_A, &lda, diag, offdiag,
                                 h_Q, &lda, h_R, &lda,
                                 tau, work, &result[1]);
#endif
                result[0] *= eps;
                result[1] *= eps;
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            lapackf77_chetrd(lapack_uplo_const(opts.uplo), &N, h_A, &lda, diag, offdiag, tau,
                             h_work, &lwork, &info);
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            if ( info != 0 )
                printf("lapackf77_chetrd returned error %d\n", (int) info);
            
            /* =====================================================================
               Print performance and error.
               =================================================================== */
            if ( opts.check ) {
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e        %8.2e   %s\n",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time,
                       result[0], result[1], (result[0] < tol && result[1] < tol ? "ok" : "failed") );
                status += ! (result[0] < tol && result[1] < tol);
            } else {
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)     ---  \n",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            }

            TESTING_FREE_PIN( h_R );
            TESTING_FREE_PIN( h_work );

            TESTING_FREE_CPU( h_A );
            TESTING_FREE_CPU( tau );
            TESTING_FREE_CPU( diag );
            TESTING_FREE_CPU( offdiag );
            
            if ( opts.check ) {
                TESTING_FREE_CPU( h_Q );
                TESTING_FREE_CPU( work );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_FREE_CPU( rwork );
                #endif
            }
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return status;
}
