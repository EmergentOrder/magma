/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver

       @date July 2014
            
       @author Stan Tomov
       @author Hartwig Anzt

       @generated from zlobpcg.cpp normal z -> c, Fri Jul 18 17:34:29 2014
*/

#include <sys/time.h>
#include <time.h>

#include "common_magma.h"
#include "../include/magmasparse.h"
#include "trace.h"
#include "magmablas.h"     

#define PRECISION_c

//==================================================================================
extern "C" magma_int_t
magma_ccompact(magma_int_t m, magma_int_t n,
               magmaFloatComplex *dA, magma_int_t ldda,
               float *dnorms, float tol, 
               magma_int_t *activeMask, magma_int_t *cBlockSize);

extern "C" magma_int_t
magma_ccompactActive(magma_int_t m, magma_int_t n,
                     magmaFloatComplex *dA, magma_int_t ldda, 
                     magma_int_t *active);
//==================================================================================










/**
    Purpose
    -------
    Solves an eigenvalue problem

       A * X = evalues X

    where A is a complex sparse matrix stored in the GPU memory.
    X and B are complex vectors stored on the GPU memory.

    This is a GPU implementation of the LOBPCG method.
    
    This method allocates all required memory space inside the routine.
    Also, the memory is not allocated as one big chunk, but seperatly for
    the different blocks. This allows to use texture also for large matrices.

    Arguments
    ---------
    @param
    A           magma_c_sparse_matrix
                input matrix A

    @param
    solver_par  magma_c_solver_par*
                solver parameters

                                            make sure to fill:
                                            num_eigenvalues
                                            length_ev

    @ingroup magmasparse_cheev
    ********************************************************************/

extern "C" magma_int_t
magma_clobpcg( magma_c_sparse_matrix A, magma_c_solver_par *solver_par ){


#define  residualNorms(i,iter)  ( residualNorms + (i) + (iter)*n )
#define magmablas_swap(x, y)    { pointer = x; x = y; y = pointer; }
#define hresidualNorms(i,iter)  (hresidualNorms + (i) + (iter)*n )

#define gramA(    m, n)   (gramA     + (m) + (n)*ldgram)
#define gramB(    m, n)   (gramB     + (m) + (n)*ldgram)
#define gevectors(m, n)   (gevectors + (m) + (n)*ldgram) 
#define h_gramB(  m, n)   (h_gramB   + (m) + (n)*ldgram)

#define magma_c_bspmv_tuned(m, n, alpha, A, X, beta, AX)       {        \
            magmablas_ctranspose( m, n, X, m, blockW, n );        	\
            magma_c_vector x, ax;                                       \
            x.memory_location = Magma_DEV;  x.num_rows = m*n;  x.nnz = m*n;  x.val = blockW; \
            ax.memory_location= Magma_DEV; ax.num_rows = m*n; ax.nnz = m*n; ax.val = AX;     \
            magma_c_spmv(alpha, A, x, beta, ax );                           \
            magmablas_ctranspose( n, m, blockW, n, X, m );            		\
}




//**************************************************************

    // Memory allocation for the eigenvectors, eigenvalues, and workspace
    solver_par->solver = Magma_LOBPCG;
    magma_int_t m = A.num_rows;
    magma_int_t n =(solver_par->num_eigenvalues);
    magmaFloatComplex *blockX = solver_par->eigenvectors;
    float *evalues = solver_par->eigenvalues;


    magmaFloatComplex *dwork, *hwork;
    magmaFloatComplex *blockP, *blockAP, *blockR, *blockAR, *blockAX, *blockW;
    magmaFloatComplex *gramA, *gramB, *gramM;
    magmaFloatComplex *gevectors, *h_gramB;

    magmaFloatComplex *pointer, *origX = blockX;
    float *eval_gpu;

    magma_int_t lwork = max( 2*n+n*magma_get_dsytrd_nb(n),
                                            1 + 6*3*n + 2* 3*n* 3*n);

    magma_cmalloc_pinned( &hwork   ,        lwork );
    magma_cmalloc(        &blockAX   ,        m*n );
    magma_cmalloc(        &blockAR   ,        m*n );
    magma_cmalloc(        &blockAP   ,        m*n );
    magma_cmalloc(        &blockR    ,        m*n );
    magma_cmalloc(        &blockP    ,        m*n );
    magma_cmalloc(        &blockW    ,        m*n );
    magma_cmalloc(        &dwork     ,        m*n );            
    magma_smalloc(        &eval_gpu  ,        3*n );




//**********************************************************+

    magma_int_t verbosity = 1;
    magma_int_t *iwork, liwork = 15*n+9;

    // === Set solver parameters ===
    float residualTolerance  = solver_par->epsilon;
    magma_int_t maxIterations = solver_par->maxiter;

    // === Set some constants & defaults ===
    magmaFloatComplex c_one = MAGMA_C_ONE, c_zero = MAGMA_C_ZERO;

    float *residualNorms, *condestGhistory, condestG;
    float *gevalues;
    magma_int_t *activeMask;

    // === Check some parameters for possible quick exit ===
    solver_par->info = 0;
    if (m < 2)
        solver_par->info = -1;
    else if (n > m)
        solver_par->info = -2;

    if (solver_par->info != 0) {
        magma_xerbla( __func__, -(solver_par->info) );
        return solver_par->info;
    }
    magma_int_t *info = &(solver_par->info); // local info variable;

    // === Allocate GPU memory for the residual norms' history ===
    magma_smalloc(&residualNorms, (maxIterations+1) * n);
    magma_malloc( (void **)&activeMask, (n+1) * sizeof(magma_int_t) );

    // === Allocate CPU work space ===
    magma_smalloc_cpu(&condestGhistory, maxIterations+1);
    magma_smalloc_cpu(&gevalues, 3 * n);
    magma_malloc_cpu((void **)&iwork, liwork * sizeof(magma_int_t));

    magmaFloatComplex *hW;
    magma_cmalloc_pinned(&hW, n*n);
    magma_cmalloc_pinned(&gevectors, 9*n*n); 
    magma_cmalloc_pinned(&h_gramB  , 9*n*n);

    // === Allocate GPU workspace ===
    magma_cmalloc(&gramM, n * n);
    magma_cmalloc(&gramA, 9 * n * n);
    magma_cmalloc(&gramB, 9 * n * n);

    #if defined(PRECISION_z) || defined(PRECISION_c)
    float *rwork;
    magma_int_t lrwork = 1 + 5*(3*n) + 2*(3*n)*(3*n);

    magma_smalloc_cpu(&rwork, lrwork);
    #endif

    // === Set activemask to one ===
    for(int k =0; k<n; k++)
        iwork[k]=1;
    magma_setmatrix(n, 1, sizeof(magma_int_t), iwork, n ,activeMask, n);

    magma_int_t gramDim, ldgram  = 3*n, ikind = 4;
       
    // === Make the initial vectors orthonormal ===
    magma_cgegqr_gpu(ikind, m, n, blockX, m, dwork, hwork, info );
    //magma_corthomgs( m, n, blockX );
    
    magma_c_bspmv_tuned(m, n, c_one, A, blockX, c_zero, blockAX );

    // === Compute the Gram matrix = (X, AX) & its eigenstates ===
    magma_cgemm(MagmaConjTrans, MagmaNoTrans, n, n, m,
                c_one,  blockX, m, blockAX, m, c_zero, gramM, n);

    magma_cheevd_gpu( MagmaVec, MagmaUpper,
                      n, gramM, n, evalues, hW, n, hwork, lwork,
                      #if defined(PRECISION_z) || defined(PRECISION_c)
                      rwork, lrwork,
                      #endif
                      iwork, liwork, info );

    // === Update  X =  X * evectors ===
    magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one,  blockX, m, gramM, n, c_zero, blockW, m);
    magmablas_swap(blockW, blockX);

    // === Update AX = AX * evectors ===
    magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one,  blockAX, m, gramM, n, c_zero, blockW, m);
    magmablas_swap(blockW, blockAX);

    condestGhistory[1] = 7.82;
    magma_int_t iterationNumber, cBlockSize, restart = 1, iter;

    //Chronometry
    real_Double_t tempo1, tempo2;
    magma_device_sync(); tempo1=magma_wtime();
    // === Main LOBPCG loop ============================================================
    for(iterationNumber = 1; iterationNumber < maxIterations; iterationNumber++)
        { 
            // === compute the residuals (R = Ax - x evalues )
            magmablas_clacpy( MagmaUpperLower, m, n, blockAX, m, blockR, m);

/*
            for(int i=0; i<n; i++){
               magma_caxpy(m, MAGMA_C_MAKE(-evalues[i],0), blockX+i*m, 1, blockR+i*m, 1);
            }
  */        
            #if defined(PRECISION_z) || defined(PRECISION_d)
                magma_dsetmatrix( 3*n, 1, evalues, 3*n, eval_gpu, 3*n );
            #else
                magma_ssetmatrix( 3*n, 1, evalues, 3*n, eval_gpu, 3*n );
            #endif

            magma_clobpcg_res( m, n, eval_gpu, blockX, blockR, eval_gpu);

            magmablas_scnrm2_cols(m, n, blockR, m, residualNorms(0, iterationNumber));

            // === remove the residuals corresponding to already converged evectors
            magma_ccompact(m, n, blockR, m,
                           residualNorms(0, iterationNumber), residualTolerance, 
                           activeMask, &cBlockSize);
            
            if (cBlockSize == 0)
                break;
        
            // === apply a preconditioner P to the active residulas: R_new = P R_old
            // === for now set P to be identity (no preconditioner => nothing to be done )
            // magmablas_clacpy( MagmaUpperLower, m, cBlockSize, blockR, m, blockW, m);

            /*
            // === make the preconditioned residuals orthogonal to X
            magma_cgemm(MagmaConjTrans, MagmaNoTrans, n, cBlockSize, m,
                        c_one, blockX, m, blockR, m, c_zero, gramB(0,0), ldgram);
            magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, cBlockSize, n,
                        c_mone, blockX, m, gramB(0,0), ldgram, c_one, blockR, m);
            */

            // === make the active preconditioned residuals orthonormal
            magma_cgegqr_gpu(ikind, m, cBlockSize, blockR, m, dwork, hwork, info );
            //magma_corthomgs( m, cBlockSize, blockR );
            
            // === compute AR
            magma_c_bspmv_tuned(m, cBlockSize, c_one, A, blockR, c_zero, blockAR );
 
            if (!restart) {
                // === compact P & AP as well
                magma_ccompactActive(m, n, blockP,  m, activeMask);
                magma_ccompactActive(m, n, blockAP, m, activeMask);
          
                /*
                // === make P orthogonal to X ?
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, n, cBlockSize, m,
                            c_one, blockX, m, blockP, m, c_zero, gramB(0,0), ldgram);
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, cBlockSize, n,
                            c_mone, blockX, m, gramB(0,0), ldgram, c_one, blockP, m);

                // === make P orthogonal to R ?
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockR, m, blockP, m, c_zero, gramB(0,0), ldgram);
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, cBlockSize, cBlockSize,
                            c_mone, blockR, m, gramB(0,0), ldgram, c_one, blockP, m);
                */

                // === Make P orthonormal & properly change AP (without multiplication by A)
                magma_cgegqr_gpu(ikind, m, cBlockSize, blockP, m, dwork, hwork, info );
                //magma_corthomgs( m, cBlockSize, blockP );

                //magma_c_bspmv_tuned(m, cBlockSize, c_one, A, blockP, c_zero, blockAP );
                magma_csetmatrix( cBlockSize, cBlockSize, hwork, cBlockSize, dwork, cBlockSize);


//                magma_ctrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit, 
  //                           m, cBlockSize, c_one, dwork, cBlockSize, blockAP, m);

            // replacement according to Stan
#if defined(PRECISION_s) || defined(PRECISION_d)
            magmablas_ctrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit, 
                        m, cBlockSize, c_one, dwork, cBlockSize, blockAP, m);
#else
            magma_ctrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit, m, 
                            cBlockSize, c_one, dwork, cBlockSize, blockAP, m);
#endif
            }

            iter = max(1,iterationNumber-10- (int)(log(1.*cBlockSize)));
            float condestGmean = 0.;
            for(int i = 0; i<iterationNumber-iter+1; i++)
                condestGmean += condestGhistory[i];
            condestGmean = condestGmean / (iterationNumber-iter+1);

            if (restart)
                gramDim = n+cBlockSize;
            else
                gramDim = n+2*cBlockSize;

            /* --- The Raileight-Ritz method for [X R P] -----------------------
               [ X R P ]'  [AX  AR  AP] y = evalues [ X R P ]' [ X R P ], i.e.,
       
                      GramA                                 GramB
                / X'AX  X'AR  X'AP \                 / X'X  X'R  X'P \
               |  R'AX  R'AR  R'AP  | y   = evalues |  R'X  R'R  R'P  |
                \ P'AX  P'AR  P'AP /                 \ P'X  P'R  P'P /       
               -----------------------------------------------------------------   */

            // === assemble GramB; first, set it to I
            magmablas_claset(MagmaFull, ldgram, ldgram, c_zero, c_one, gramB, ldgram);  // identity

            if (!restart) {
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                            c_one, blockP, m, blockX, m, c_zero, gramB(n+cBlockSize,0), ldgram);
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockP, m, blockR, m, c_zero, gramB(n+cBlockSize,n), ldgram);
            }
            magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                        c_one, blockR, m, blockX, m, c_zero, gramB(n,0), ldgram);

            // === get GramB from the GPU to the CPU and compute its eigenvalues only
            magma_cgetmatrix(gramDim, gramDim, gramB, ldgram, h_gramB, ldgram);
            lapackf77_cheev("N", "L", &gramDim, h_gramB, &ldgram, gevalues, 
                            hwork, &lwork,
                            #if defined(PRECISION_z) || defined(PRECISION_c)
                            rwork, 
                            #endif
                            info);

            // === check stability criteria if we need to restart
            condestG = log10( gevalues[gramDim-1]/gevalues[0] ) + 1.;
            if ((condestG/condestGmean>2 && condestG>2) || condestG>8) {
                // Steepest descent restart for stability
                restart=1;
                printf("restart at step #%d\n", (int) iterationNumber);
            }

            // === assemble GramA; first, set it to I
            magmablas_claset(MagmaFull, ldgram, ldgram, c_zero, c_one, gramA, ldgram);  // identity

            magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                        c_one, blockR, m, blockAX, m, c_zero, gramA(n,0), ldgram);
            magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                        c_one, blockR, m, blockAR, m, c_zero, gramA(n,n), ldgram);

            if (!restart) {
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m, 
                            c_one, blockP, m, blockAX, m, c_zero, 
                            gramA(n+cBlockSize,0), ldgram);
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m, 
                            c_one, blockP, m, blockAR, m, c_zero, 
                            gramA(n+cBlockSize,n), ldgram);
                magma_cgemm(MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m, 
                            c_one, blockP, m, blockAP, m, c_zero, 
                            gramA(n+cBlockSize,n+cBlockSize), ldgram);
            }

            /*
            // === Compute X' AX or just use the eigenvalues below ?
            magma_cgemm(MagmaConjTrans, MagmaNoTrans, n, n, m,
                        c_one, blockX, m, blockAX, m, c_zero,
                        gramA(0,0), ldgram);
            */

            if (restart==0) {
                magma_cgetmatrix(gramDim, gramDim, gramA, ldgram, gevectors, ldgram);
            }
            else {
                gramDim = n+cBlockSize;
                magma_cgetmatrix(gramDim, gramDim, gramA, ldgram, gevectors, ldgram);
            }

            for(int k=0; k<n; k++)
                *gevectors(k,k) = MAGMA_C_MAKE(evalues[k], 0);

            // === the previous eigensolver destroyed what is in h_gramB => must copy it again
            magma_cgetmatrix(gramDim, gramDim, gramB, ldgram, h_gramB, ldgram);

            magma_int_t itype = 1;
            lapackf77_chegvd(&itype, "V", "L", &gramDim, 
                             gevectors, &ldgram, h_gramB, &ldgram,
                             gevalues, hwork, &lwork, 
                             #if defined(PRECISION_z) || defined(PRECISION_c)
                             rwork, &lrwork,
                             #endif
                             iwork, &liwork, info);
 
            for(int k =0; k<n; k++)
                evalues[k] = gevalues[k];
            
            // === copy back the result to gramA on the GPU and use it for the updates
            magma_csetmatrix(gramDim, gramDim, gevectors, ldgram, gramA, ldgram);

            if (restart == 0) {
                // === contribution from P to the new X (in new search direction P)
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize, 
                            c_one, blockP, m, gramA(n+cBlockSize,0), ldgram, c_zero, dwork, m);
                magmablas_swap(dwork, blockP);
 
                // === contribution from R to the new X (in new search direction P)
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockR, m, gramA(n,0), ldgram, c_one, blockP, m);

                // === corresponding contribution from AP to the new AX (in AP)
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockAP, m, gramA(n+cBlockSize,0), ldgram, c_zero, dwork, m);
                magmablas_swap(dwork, blockAP);

                // === corresponding contribution from AR to the new AX (in AP)
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockAR, m, gramA(n,0), ldgram, c_one, blockAP, m);
            }
            else {
                // === contribution from R (only) to the new X
                magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockR, m, gramA(n,0), ldgram, c_zero, blockP, m);

                // === corresponding contribution from AR (only) to the new AX
                magma_cgemm(MagmaNoTrans, MagmaNoTrans,m, n, cBlockSize,
                            c_one, blockAR, m, gramA(n,0), ldgram, c_zero, blockAP, m);
            }
            
            // === contribution from old X to the new X + the new search direction P
            magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                        c_one, blockX, m, gramA, ldgram, c_zero, dwork, m);
            magmablas_swap(dwork, blockX);
            //magma_caxpy(m*n, c_one, blockP, 1, blockX, 1);
            magma_clobpcg_maxpy( m, n, blockP, blockX );    

            
            // === corresponding contribution from old AX to new AX + AP
            magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                        c_one, blockAX, m, gramA, ldgram, c_zero, dwork, m);
            magmablas_swap(dwork, blockAX);
            //magma_caxpy(m*n, c_one, blockAP, 1, blockAX, 1);
            magma_clobpcg_maxpy( m, n, blockAP, blockAX );    

            condestGhistory[iterationNumber+1]=condestG;
            if (verbosity==1) {
                // float res;
                // magma_cgetmatrix(1, 1, 
                //                  (magmaFloatComplex*)residualNorms(0, iterationNumber), 1, 
                //                  (magmaFloatComplex*)&res, 1);
                // 
                //  printf("Iteration %4d, CBS %4d, Residual: %10.7f\n",
                //         iterationNumber, cBlockSize, res);
                printf("%4d-%2d ", (int) iterationNumber, (int) cBlockSize); 
                magma_sprint_gpu(1, n, residualNorms(0, iterationNumber), 1);
            }

            restart = 0;
        }   // === end for iterationNumber = 1,maxIterations =======================


    // fill solver info
    magma_device_sync(); tempo2=magma_wtime();
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    solver_par->numiter = iterationNumber;
    if( solver_par->numiter < solver_par->maxiter){
        solver_par->info = 0;
    }else if( solver_par->init_res > solver_par->final_res )
        solver_par->info = -2;
    else
        solver_par->info = -1;
    
    // =============================================================================
    // === postprocessing;
    // =============================================================================

    // === compute the real AX and corresponding eigenvalues
    magma_c_bspmv_tuned(m, n, c_one, A, blockX, c_zero, blockAX );
    magma_cgemm(MagmaConjTrans, MagmaNoTrans, n, n, m,
                c_one,  blockX, m, blockAX, m, c_zero, gramM, n);

    magma_cheevd_gpu( MagmaVec, MagmaUpper,
                      n, gramM, n, gevalues, dwork, n, hwork, lwork, 
                      #if defined(PRECISION_z) || defined(PRECISION_c)
                      rwork, lrwork,
                      #endif
                      iwork, liwork, info );
   
    for(int k =0; k<n; k++)
        evalues[k] = gevalues[k];

    // === update X = X * evectors
    magmablas_swap(blockX, dwork);
    magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one, dwork, m, gramM, n, c_zero, blockX, m);

    // === update AX = AX * evectors to compute the final residual
    magmablas_swap(blockAX, dwork);
    magma_cgemm(MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one, dwork, m, gramM, n, c_zero, blockAX, m);

    // === compute R = AX - evalues X
    magmablas_clacpy( MagmaUpperLower, m, n, blockAX, m, blockR, m);
    for(int i=0; i<n; i++)
        magma_caxpy(m, MAGMA_C_MAKE(-evalues[i], 0), blockX+i*m, 1, blockR+i*m, 1);

    // === residualNorms[iterationNumber] = || R ||    
    magmablas_scnrm2_cols(m, n, blockR, m, residualNorms(0, iterationNumber));

    // === restore blockX if needed
    if (blockX != origX)
        magmablas_clacpy( MagmaUpperLower, m, n, blockX, m, origX, m);

    printf("Eigenvalues:\n");
    for(int i =0; i<n; i++)
        printf("%e  ", evalues[i]);
    printf("\n\n");

    printf("Final residuals:\n");
    magma_sprint_gpu(1, n, residualNorms(0, iterationNumber), 1);
    printf("\n\n");

    //=== Print residual history in a file for plotting ====
    float *hresidualNorms;
    magma_smalloc_cpu(&hresidualNorms, (iterationNumber+1) * n);
    magma_cgetmatrix(n, iterationNumber, 
                     (magmaFloatComplex*)residualNorms, n, 
                     (magmaFloatComplex*)hresidualNorms, n);

    printf("Residuals are stored in file residualNorms\n");
    printf("Plot the residuals using: myplot \n");
    
    FILE *residuals_file;
    residuals_file = fopen("residualNorms", "w");
    for(int i =1; i<iterationNumber; i++) {
        for(int j = 0; j<n; j++)
            fprintf(residuals_file, "%f ", *hresidualNorms(j,i));
        fprintf(residuals_file, "\n");
    }
    fclose(residuals_file);
    magma_free_cpu(hresidualNorms);

    // === free work space
    magma_free(     residualNorms   );
    magma_free_cpu( condestGhistory );
    magma_free_cpu( gevalues        );
    magma_free_cpu( iwork           );

    magma_free_pinned( hW           );
    magma_free_pinned( gevectors    );
    magma_free_pinned( h_gramB      );

    magma_free(     gramM           );
    magma_free(     gramA           );
    magma_free(     gramB           );
    magma_free(  activeMask         );

    magma_free(     blockAX    );
    magma_free(     blockAR    );
    magma_free(     blockAP    );
    magma_free(     blockR    );
    magma_free(     blockP    );
    magma_free(     blockW    );
    magma_free(     dwork    );   
    magma_free(     eval_gpu    );    

    magma_free_pinned( hwork    );


    #if defined(PRECISION_z) || defined(PRECISION_c)
    magma_free_cpu( rwork           );
    #endif

    return MAGMA_SUCCESS;
}
