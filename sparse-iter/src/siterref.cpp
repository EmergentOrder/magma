/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Hartwig Anzt 

       @generated from ziterref.cpp normal z -> s, Fri Jul 18 17:34:29 2014
*/

#include "common_magma.h"
#include "../include/magmasparse.h"

#include <assert.h>

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a real symmetric N-by-N positive definite matrix A.
    This is a GPU implementation of the Iterative Refinement method.
    The inner solver is passed via the preconditioner argument.

    Arguments
    ---------

    @param
    A           magma_s_sparse_matrix
                input matrix A

    @param
    b           magma_s_vector
                RHS b

    @param
    x           magma_s_vector*
                solution approximation

    @param
    solver_par  magma_s_solver_par*
                solver parameters

    @param
    precond_par magma_s_preconditioner*
                inner solver

    @ingroup magmasparse_sgesv
    ********************************************************************/

magma_int_t
magma_siterref( magma_s_sparse_matrix A, magma_s_vector b, magma_s_vector *x,  
   magma_s_solver_par *solver_par, magma_s_preconditioner *precond_par ){

    // prepare solver feedback
    solver_par->solver = Magma_ITERREF;
    solver_par->numiter = 0;
    solver_par->info = 0;

    float residual;
    magma_sresidual( A, b, *x, &residual );
    solver_par->init_res = residual;

    // some useful variables
    float c_zero = MAGMA_S_ZERO, c_one = MAGMA_S_ONE, 
                                                c_mone = MAGMA_S_NEG_ONE;
    
    magma_int_t dofs = A.num_rows;

    // workspace
    magma_s_vector r,z;
    magma_s_vinit( &r, Magma_DEV, dofs, c_zero );
    magma_s_vinit( &z, Magma_DEV, dofs, c_zero );

    // solver variables
    float nom, nom0, r0;

    // solver setup
    magma_sscal( dofs, c_zero, x->val, 1) ;                    // x = 0

    magma_s_spmv( c_mone, A, *x, c_zero, r );                  // r = - A x
    magma_saxpy(dofs,  c_one, b.val, 1, r.val, 1);             // r = r + b
    nom0 = magma_snrm2(dofs, r.val, 1);                       // nom0 = || r ||
    nom = nom0 * nom0;
    
    if ( (r0 = nom * solver_par->epsilon) < ATOLERANCE ) 
        r0 = ATOLERANCE;
    if ( nom < r0 )
        return MAGMA_SUCCESS;
    
    //Chronometry
    real_Double_t tempo1, tempo2;
    magma_device_sync(); tempo1=magma_wtime();
    if( solver_par->verbose > 0 ){
        solver_par->res_vec[0] = nom0;
        solver_par->timing[0] = 0.0;
    }
    
    // start iteration
    for( solver_par->numiter= 1; solver_par->numiter<solver_par->maxiter; 
                                                    solver_par->numiter++ ){

        magma_sscal( dofs, MAGMA_S_MAKE(1./nom, 0.), r.val, 1) ;  // scale it
        magma_s_precond( A, r, &z, *precond_par );  // inner solver:  A * z = r
        magma_sscal( dofs, MAGMA_S_MAKE(nom, 0.), z.val, 1) ;  // scale it
        magma_saxpy(dofs,  c_one, z.val, 1, x->val, 1);        // x = x + z
        magma_s_spmv( c_mone, A, *x, c_zero, r );              // r = - A x
        magma_saxpy(dofs,  c_one, b.val, 1, r.val, 1);         // r = r + b
        nom = magma_snrm2(dofs, r.val, 1);                    // nom = || r || 

        if( solver_par->verbose > 0 ){
            magma_device_sync(); tempo2=magma_wtime();
            if( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) nom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        if (  nom  < r0 ) {
            break;
        }
    } 
    magma_device_sync(); tempo2=magma_wtime();
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    magma_sresidual( A, b, *x, &residual );
    solver_par->final_res = residual;
    solver_par->iter_res = nom;

    if( solver_par->numiter < solver_par->maxiter){
        solver_par->info = 0;
    }else if( solver_par->init_res > solver_par->final_res ){
        if( solver_par->verbose > 0 ){
            if( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) nom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = -2;
    }
    else{
        if( solver_par->verbose > 0 ){
            if( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) nom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = -1;
    }   
    magma_s_vfree(&r);
    magma_s_vfree(&z);


    return MAGMA_SUCCESS;
}   /* magma_siterref */


