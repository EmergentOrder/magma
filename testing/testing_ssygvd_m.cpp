/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

    @author Raffaele Solca
    @author Azzam Haidar

    @generated from testing_zhegvd_m.cpp normal z -> s, Fri Jul 18 17:34:25 2014

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"

#define PRECISION_s


#include "testings.h"

#define absv(v1) ((v1)>0? (v1): -(v1))

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssygvd
*/
int main( int argc, char** argv)
{
    TESTING_INIT_MGPU();

    float *h_A, *h_Ainit, *h_B, *h_Binit, *h_work;
    #if defined(PRECISION_z) || defined(PRECISION_c)
    float *rwork;
    #endif
    float *w1, *w2, result;
    magma_int_t *iwork;
    real_Double_t mgpu_time, gpu_time, cpu_time;

    /* Matrix size */
    magma_int_t N=0, n2;

    magma_int_t info;
    magma_int_t ione = 1;

    float c_zero    = MAGMA_S_ZERO;
    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;

    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t status = 0;

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    float tol    = opts.tolerance * lapackf77_slamch("E");
    float tolulp = opts.tolerance * lapackf77_slamch("P");

    if ( opts.check && opts.jobz == MagmaNoVec ) {
        fprintf( stderr, "checking results requires vectors; setting jobz=V (option -JV)\n" );
        opts.jobz = MagmaVec;
    }

    printf("using: ngpu = %d, itype = %d, jobz = %s, uplo = %s, check = %d\n",
           (int) opts.ngpu, (int) opts.itype,
           lapack_vec_const(opts.jobz), lapack_uplo_const(opts.uplo), (int) opts.check);

    printf("    N   CPU Time (sec)   GPU Time (sec)   MGPU Time (sec)\n");
    printf("=========================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            n2     = N*N;
            #if defined(PRECISION_z) || defined(PRECISION_c)
            magma_int_t lwork = 2*N + N*N;
            magma_int_t lrwork = 1 + 5*N +2*N*N;
            // MKL's ssygvd has a bug for small N - it looks like what is returned by a
            // query (consistent with LAPACK's number above) is different from the memory
            // requirement check (that returns info -11). The lwork increase below is needed
            // to pass this check.
            if (N<32)
                lwork = 34*32;
            #else
            magma_int_t lwork  = 1 + 6*N + 2*N*N;
            #endif
            magma_int_t liwork = 3 + 5*N;

            TESTING_MALLOC_PIN( h_A,    float, n2    );
            TESTING_MALLOC_PIN( h_B,    float, n2    );
            TESTING_MALLOC_PIN( h_work, float, lwork );
            #if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_MALLOC_PIN( rwork, float, lrwork );
            #endif

            TESTING_MALLOC_CPU( w1,    float, N );
            TESTING_MALLOC_CPU( w2,    float, N );
            TESTING_MALLOC_CPU( iwork, magma_int_t, liwork );

            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            lapackf77_slarnv( &ione, ISEED, &n2, h_B );
            magma_smake_hpd( N, h_B, N );
            magma_smake_symmetric( N, h_A, N );

            if ( opts.warmup || opts.check ) {
                TESTING_MALLOC_CPU( h_Ainit, float, n2 );
                TESTING_MALLOC_CPU( h_Binit, float, n2 );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_Ainit, &N );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_B, &N, h_Binit, &N );
            }

            if (opts.warmup) {
                // ==================================================================
                // Warmup using MAGMA.
                // ==================================================================
                magma_ssygvd_m( opts.ngpu, opts.itype, opts.jobz, opts.uplo,
                                N, h_A, N, h_B, N, w1,
                                h_work, lwork,
                                #if defined(PRECISION_z) || defined(PRECISION_c)
                                rwork, lrwork,
                                #endif
                                iwork, liwork,
                                &info);
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_Ainit, &N, h_A, &N );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_Binit, &N, h_B, &N );
            }

            // ===================================================================
            // Performs operation using MAGMA
            // ===================================================================

            mgpu_time = magma_wtime();
            magma_ssygvd_m( opts.ngpu, opts.itype, opts.jobz, opts.uplo,
                            N, h_A, N, h_B, N, w1,
                            h_work, lwork,
                            #if defined(PRECISION_z) || defined(PRECISION_c)
                            rwork, lrwork,
                            #endif
                            iwork, liwork,
                            &info);
            mgpu_time = magma_wtime() - mgpu_time;

            if (info != 0)
                printf("magma_ssygvd_m returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));

            if ( opts.check ) {
                /* =====================================================================
                   Check the results following the LAPACK's [zc]hegvd routine.
                   A x = lambda B x is solved
                   and the following 3 tests computed:
                   (1)    | A Z - B Z D | / ( |A||Z| N )  (itype = 1)
                          | A B Z - Z D | / ( |A||Z| N )  (itype = 2)
                          | B A Z - Z D | / ( |A||Z| N )  (itype = 3)
                   =================================================================== */

                #if defined(PRECISION_d) || defined(PRECISION_s)
                float *rwork = h_work + N*N;
                #endif

                result = 1.;
                result /= lapackf77_slansy("1", lapack_uplo_const(opts.uplo), &N, h_Ainit, &N, rwork);
                result /= lapackf77_slange("1", &N, &N, h_A, &N, rwork);

                if (opts.itype == 1) {
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_one, h_Ainit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_neg_one, h_Binit, &N, h_A, &N, &c_one, h_work, &N);
                    result *= lapackf77_slange("1", &N, &N, h_work, &N, rwork)/N;
                }
                else if (opts.itype == 2) {
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_one, h_Binit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_one, h_Ainit, &N, h_work, &N, &c_neg_one, h_A, &N);
                    result *= lapackf77_slange("1", &N, &N, h_A, &N, rwork)/N;
                }
                else if (opts.itype == 3) {
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_one, h_Ainit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &N, &c_one, h_Binit, &N, h_work, &N, &c_neg_one, h_A, &N);
                    result *= lapackf77_slange("1", &N, &N, h_A, &N, rwork)/N;
                }

                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_Ainit, &N, h_A, &N );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_Binit, &N, h_B, &N );

                /* ====================================================================
                   Performs operation using MAGMA
                   =================================================================== */
                gpu_time = magma_wtime();
                magma_ssygvd(opts.itype, opts.jobz, opts.uplo,
                             N, h_A, N, h_B, N, w2,
                             h_work, lwork,
                             #if defined(PRECISION_z) || defined(PRECISION_c)
                             rwork, lrwork,
                             #endif
                             iwork, liwork,
                             &info);
                gpu_time = magma_wtime() - gpu_time;

                if (info != 0)
                    printf("magma_ssygvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));

                /* =====================================================================
                   Performs operation using LAPACK
                   =================================================================== */
                cpu_time = magma_wtime();
                lapackf77_ssygvd(&opts.itype, lapack_vec_const(opts.jobz), lapack_uplo_const(opts.uplo),
                                 &N, h_Ainit, &N, h_Binit, &N, w2,
                                 h_work, &lwork,
                                 #if defined(PRECISION_z) || defined(PRECISION_c)
                                 rwork, &lrwork,
                                 #endif
                                 iwork, &liwork,
                                 &info);
                cpu_time = magma_wtime() - cpu_time;
                if (info != 0)
                    printf("lapackf77_ssygvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));

                float temp1 = 0;
                float temp2 = 0;
                for(int j=0; j<N; j++) {
                    temp1 = max(temp1, absv(w1[j]));
                    temp1 = max(temp1, absv(w2[j]));
                    temp2 = max(temp2, absv(w1[j]-w2[j]));
                }
                float result2 = temp2 / (((float)N)*temp1);

                /* =====================================================================
                   Print execution time
                   =================================================================== */
                printf("%5d   %7.2f          %7.2f          %7.2f\n",
                       (int) N, cpu_time, gpu_time, mgpu_time);
                printf("Testing the eigenvalues and eigenvectors for correctness:\n");
                if (opts.itype==1) {
                    printf("(1)    | A Z - B Z D | / (|A| |Z| N) = %8.2e   %s\n",   result,  (result  < tol    ? "ok" : "failed") );
                }
                else if (opts.itype==2) {
                    printf("(1)    | A B Z - Z D | / (|A| |Z| N) = %8.2e   %s\n",   result,  (result  < tol    ? "ok" : "failed") );
                }
                else if (opts.itype==3) {
                    printf("(1)    | B A Z - Z D | / (|A| |Z| N) = %8.2e   %s\n",   result,  (result  < tol    ? "ok" : "failed") );
                }
                printf(    "(3)    | D(MGPU)-D(LAPACK) |/ |D|    = %8.2e   %s\n\n", result2, (result2 < tolulp ? "ok" : "failed") );
                status += ! (result < tol && result2 < tolulp);
            }
            else {
                printf("%5d     ---              ---            %7.2f\n",
                       (int) N, mgpu_time);
            }

            /* Memory clean up */
            TESTING_FREE_PIN( h_A    );
            TESTING_FREE_PIN( h_B    );
            TESTING_FREE_PIN( h_work );
            #if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_FREE_PIN( rwork  );
            #endif
            
            TESTING_FREE_CPU( w1    );
            TESTING_FREE_CPU( w2    );
            TESTING_FREE_CPU( iwork );

            if ( opts.warmup || opts.check ) {
                TESTING_FREE_CPU( h_Ainit );
                TESTING_FREE_CPU( h_Binit );
            }
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    /* Shutdown */
    TESTING_FINALIZE_MGPU();
    return status;
}
