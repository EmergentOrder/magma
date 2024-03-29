/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from testing_zhemv_mgpu.cpp normal z -> c, Fri Jul 18 17:34:23 2014
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas_v2.h>
#include <cblas.h>

#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_c

int main(int argc, char **argv)
{
    TESTING_INIT();
    magma_setdevice(0);

    real_Double_t gflops, gpu_time, gpu_perf, cuda_time, cuda_perf;
    float      error, work[1];
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magma_int_t n_local[MagmaMaxGPUs];

    magma_int_t N, d, j, lda, nb, blocks, lwork, matsize, vecsize;
    magma_int_t incx = 1;

    magmaFloatComplex alpha = MAGMA_C_MAKE(  1.5, -2.3 );  // MAGMA_C_ONE;
    magmaFloatComplex beta  = MAGMA_C_MAKE( -0.6,  0.8 );  // MAGMA_C_ZERO;
    magmaFloatComplex *A, *X, *Y[MagmaMaxGPUs], *Ycublas, *Ymagma;
    magmaFloatComplex *dA, *dX[MagmaMaxGPUs], *dY[MagmaMaxGPUs], *d_lA[MagmaMaxGPUs], *dYcublas;

    //magma_queue_t stream[MagmaMaxGPUs][10];
    magmaFloatComplex *C_work;
    magmaFloatComplex *dC_work[MagmaMaxGPUs];
    magma_int_t     status = 0;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    float tol = opts.tolerance * lapackf77_slamch("E");

    nb = 32;
    //nb = 64;

    printf("uplo = %s, block size = %d, offset %d\n",
            lapack_uplo_const(opts.uplo), (int) nb, (int) opts.offset );
    printf( "    N   CUBLAS, Gflop/s   MAGMABLAS, Gflop/s      \"error\"\n"
            "==============================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda = ((N+31)/32)*32;
            matsize = N*lda;
            vecsize = N*incx;
            gflops = FLOPS_CHEMV( N ) / 1e9;

            TESTING_MALLOC_CPU( A,       magmaFloatComplex, matsize );
            TESTING_MALLOC_CPU( X,       magmaFloatComplex, vecsize );
            TESTING_MALLOC_CPU( Ycublas, magmaFloatComplex, vecsize );
            TESTING_MALLOC_CPU( Ymagma,  magmaFloatComplex, vecsize );
            for(d=0; d < opts.ngpu; d++) {
                TESTING_MALLOC_CPU( Y[d], magmaFloatComplex, vecsize );
            }
            
            magma_setdevice(0);
            TESTING_MALLOC_DEV( dA,       magmaFloatComplex, matsize );
            TESTING_MALLOC_DEV( dYcublas, magmaFloatComplex, vecsize );
            
            for(d=0; d < opts.ngpu; d++) {
                n_local[d] = ((N/nb)/opts.ngpu)*nb;
                if (d < (N/nb)%opts.ngpu)
                    n_local[d] += nb;
                else if (d == (N/nb)%opts.ngpu)
                    n_local[d] += N%nb;
                
                magma_setdevice(d);
                
                TESTING_MALLOC_DEV( d_lA[d], magmaFloatComplex, lda*n_local[d] ); // potentially bugged
                TESTING_MALLOC_DEV( dX[d],   magmaFloatComplex, vecsize );
                TESTING_MALLOC_DEV( dY[d],   magmaFloatComplex, vecsize );
                
                printf("device %2d n_local = %4d\n", (int) d, (int) n_local[d]);
            }
            magma_setdevice(0);
            
            //////////////////////////////////////////////////////////////////////////
            
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &matsize, A );
            magma_cmake_hermitian( N, A, lda );
            
            blocks = (N-1) / nb + 1;
            lwork = lda * (blocks + 1);
            TESTING_MALLOC_CPU( C_work, magmaFloatComplex, lwork );
            for(d=0; d < opts.ngpu; d++) {
                magma_setdevice(d);
                TESTING_MALLOC_DEV( dC_work[d], magmaFloatComplex, lwork );
            }
            magma_setdevice(0);
            
            lapackf77_clarnv( &ione, ISEED, &vecsize, X );
            lapackf77_clarnv( &ione, ISEED, &vecsize, Y[0] );
            
            /* =====================================================================
               Performs operation using CUBLAS
               =================================================================== */
            magma_setdevice(0);
            magma_csetmatrix_1D_col_bcyclic(N, N, A, lda, d_lA, lda, opts.ngpu, nb);
            magma_setdevice(0);
            
            magma_csetmatrix( N, N, A, lda, dA, lda );
            magma_csetvector( N, Y[0], incx, dYcublas, incx );
            
            for(d=0; d < opts.ngpu; d++) {
                magma_setdevice(d);
                magma_csetvector( N, X, incx, dX[d], incx );
                magma_csetvector( N, Y[0], incx, dY[d], incx );
                magma_csetmatrix( lda, blocks, C_work, lda, dC_work[d], lda );
            }
            
            magma_setdevice(0);
            cuda_time = magma_wtime();
            cublasChemv( handle, cublas_uplo_const(opts.uplo), N-opts.offset,
                         &alpha, dA + opts.offset + opts.offset*lda, lda,
                                 dX[0] + opts.offset,    incx,
                         &beta,  dYcublas + opts.offset, incx );
            cuda_time = magma_wtime() - cuda_time;
            cuda_perf = gflops / cuda_time;
            
            magma_cgetvector( N, dYcublas, incx, Ycublas, incx );
            
            /* =====================================================================
               Performs operation using MAGMABLAS
               =================================================================== */
            magma_setdevice(0);
            gpu_time = magma_wtime();
            
            if (nb == 32) {
                magmablas_chemv2_mgpu_32_offset(
                    opts.uplo, N, alpha, d_lA, lda, dX, incx, beta, dY, incx,
                    dC_work, lwork, opts.ngpu, nb, opts.offset);
            }
            else { // nb = 64
                magmablas_chemv2_mgpu_offset(
                    opts.uplo, N, alpha, d_lA, lda, dX, incx, beta, dY, incx,
                    dC_work, lwork, opts.ngpu, nb, opts.offset);
            }
            
            // todo probably don't need sync here; getvector will sync
            for(d=1; d < opts.ngpu; d++) {
                magma_setdevice(d);
                cudaDeviceSynchronize();
            }
            
            for(d=0; d < opts.ngpu; d++) {
                magma_setdevice(d);
                magma_cgetvector( N, dY[d], incx, Y[d], incx );
            }
            magma_setdevice(0);
            
            for( j = opts.offset; j < N; j++) {
                for(d=1; d < opts.ngpu; d++) {
                    //printf("Y[%d][%d] = %15.14f\n", d, j, Y[d][j].x);
                    #if defined(PRECISION_z) || defined(PRECISION_c)
                    Y[0][j].x = Y[0][j].x + Y[d][j].x;
                                Y[0][j].y = Y[0][j].y + Y[d][j].y;
                    #else
                    Y[0][j] = Y[0][j] + Y[d][j];
                    #endif
                }
            }
            
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;

/*

#if defined(PRECISION_z) || defined(PRECISION_c)

            for( j = opts.offset; j < N; j++) {
                if (Y[0][j].x != Ycublas[j].x) {
                    printf("Y-multi[%d] = %f, %f\n",  j, Y[0][j].x, Y[0][j].y );
                    printf("Ycublas[%d] = %f, %f\n",  j, Ycublas[j].x, Ycublas[j].y);
                }
            }

#else

            for( j = opts.offset; j < N; j++) {
                if (Y[0][j] != Ycublas[j]) {
                    printf("Y-multi[%d] = %f\n",  j, Y[0][j] );
                    printf("Ycublas[%d] = %f\n",  j, Ycublas[j]);
                }
            }

#endif

*/
            /* =====================================================================
               Compute the Difference Cublas VS Magma
               =================================================================== */
            magma_int_t nw = N - opts.offset;
            blasf77_caxpy( &nw, &c_neg_one, Y[0] + opts.offset, &incx, Ycublas + opts.offset, &incx);
            error = lapackf77_clange( "N", &nw, &ione, Ycublas + opts.offset, &nw, work ) / N;

#if  0
            /*
             * Extra check with cblas vs magma
             */
            cblas_ccopy( N, Y, incx, Ycublas, incx );
            cblas_chemv( CblasColMajor, CblasLower, N,
                         CBLAS_SADDR(alpha), A, lda, X, incx,
                         CBLAS_SADDR(beta), Ycublas, incx );
            
            blasf77_caxpy( &N, &c_neg_one, Ymagma, &incx, Ycublas, &incx);
            error = lapackf77_clange( "N", &N, &ione, Ycublas, &N, work ) / N;
#endif
            printf( "%5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s",
                    (int) N, cuda_perf, cuda_time, gpu_perf, gpu_time,
                    error, (error < tol ? "ok" : "failed") );
            status += ! (error < tol);
            
            /* Free Memory */
            TESTING_FREE_CPU( A );
            TESTING_FREE_CPU( X );
            TESTING_FREE_CPU( Ycublas );
            TESTING_FREE_CPU( Ymagma  );
            TESTING_FREE_CPU( C_work  );
            
            magma_setdevice(0);
            TESTING_FREE_DEV( dA );
            TESTING_FREE_DEV( dYcublas );
            
            for(d=0; d < opts.ngpu; d++) {
                TESTING_FREE_CPU( Y[d] );
                magma_setdevice(d);
                
                TESTING_FREE_DEV( d_lA[d]    );
                TESTING_FREE_DEV( dX[d]      );
                TESTING_FREE_DEV( dY[d]      );
                TESTING_FREE_DEV( dC_work[d] );
            }
            magma_setdevice(0);
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return status;
}
