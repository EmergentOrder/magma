/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from testing_zungqr_m.cpp normal z -> d, Fri Jul 18 17:34:25 2014

       @author Stan Tomov
       @author Mathieu Faverge
       @author Mark Gates
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <assert.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dorgqr
*/
int main( int argc, char** argv )
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           error, work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *hA, *hR, *hT, *tau, *h_work;
    double *dA, *dT;
    magma_int_t m, n, k;
    magma_int_t n2, lda, ldda, lwork, min_mn, nb, info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    double tol = opts.tolerance * lapackf77_dlamch("E");
    
    printf("    m     n     k   CPU GFlop/s (sec)   GPU GFlop/s (sec)   ||R|| / ||A||\n");
    printf("=========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            m = opts.msize[itest];
            n = opts.nsize[itest];
            k = opts.ksize[itest];
            if ( m < n || n < k ) {
                printf( "%5d %5d %5d   skipping because m < n or n < k\n", (int) m, (int) n, (int) k );
                continue;
            }
            
            lda  = m;
            ldda = ((m + 31)/32)*32;
            n2 = lda*n;
            min_mn = min(m, n);
            nb = magma_get_dgeqrf_nb( m );
            lwork  = (m + 2*n+nb)*nb;
            gflops = FLOPS_DORGQR( m, n, k ) / 1e9;
            
            TESTING_MALLOC_PIN( hA,     double, lda*n     );
            TESTING_MALLOC_PIN( h_work, double, lwork     );
            
            TESTING_MALLOC_CPU( hR,     double, lda*n     );
            TESTING_MALLOC_CPU( hT,     double, min_mn*nb );
            TESTING_MALLOC_CPU( tau,    double, min_mn    );
                                        
            TESTING_MALLOC_DEV( dA,     double, ldda*n    );
            TESTING_MALLOC_DEV( dT,     double, ( 2*min_mn + ((n + 31)/32)*32 )*nb );
            
            lapackf77_dlarnv( &ione, ISEED, &n2, hA );
            lapackf77_dlacpy( MagmaUpperLowerStr, &m, &n, hA, &lda, hR, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            // first, get QR factors
            magma_dsetmatrix( m, n, hA, lda, dA, ldda );
            magma_dgeqrf_gpu( m, n, dA, ldda, tau, dT, &info );
            if ( info != 0 )
                printf("magma_dgeqrf_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            magma_dgetmatrix( m, n, dA, ldda, hR, lda );
            magma_dgetmatrix( nb, min_mn, dT, nb, hT, nb );
            
            gpu_time = magma_wtime();
            magma_dorgqr_m( m, n, k, hR, lda, tau, hT, nb, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if ( info != 0 )
                printf("magma_dorgqr_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                error = lapackf77_dlange("f", &m, &n, hA, &lda, work );
                
                lapackf77_dgeqrf( &m, &n, hA, &lda, tau, h_work, &lwork, &info );
                if ( info != 0 )
                    printf("lapackf77_dgeqrf returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                cpu_time = magma_wtime();
                lapackf77_dorgqr( &m, &n, &k, hA, &lda, tau, h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if ( info != 0 )
                    printf("lapackf77_dorgqr returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                // compute relative error |R|/|A| := |Q_magma - Q_lapack|/|A|
                blasf77_daxpy( &n2, &c_neg_one, hA, &ione, hR, &ione );
                error = lapackf77_dlange("f", &m, &n, hR, &lda, work) / error;
                
                printf("%5d %5d %5d   %7.1f (%7.2f)   %7.1f (%7.2f)   %8.2e   %s\n",
                       (int) m, (int) n, (int) k,
                       cpu_perf, cpu_time, gpu_perf, gpu_time,
                       error, (error < tol ? "ok" : "failed") );
                status += ! (error < tol);
            }
            else {
                printf("%5d %5d %5d     ---   (  ---  )   %7.1f (%7.2f)     ---  \n",
                       (int) m, (int) n, (int) k,
                       gpu_perf, gpu_time );
            }
            
            TESTING_FREE_PIN( hA     );
            TESTING_FREE_PIN( h_work );
            
            TESTING_FREE_CPU( hR  );
            TESTING_FREE_CPU( hT  );
            TESTING_FREE_CPU( tau );
            
            TESTING_FREE_DEV( dA  );
            TESTING_FREE_DEV( dT  );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return status;
}
