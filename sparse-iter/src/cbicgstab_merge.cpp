/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zbicgstab_merge.cpp normal z -> c, Fri Jul 18 17:34:29 2014
       @author Hartwig Anzt

*/
#include "common_magma.h"
#include "../include/magmasparse.h"
#include <cblas.h>
#include <assert.h>

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )

#define  q(i)     (q.val + (i)*dofs)

/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a complex Hermitian N-by-N positive definite matrix A.
    This is a GPU implementation of the Biconjugate Gradient Stabelized method.
    The difference to magma_cbicgstab is that we use specifically designed 
    kernels merging multiple operations into one kernel.

    Arguments
    ---------

    @param
    A           magma_c_sparse_matrix
                input matrix A

    @param
    b           magma_c_vector
                RHS b

    @param
    x           magma_c_vector*
                solution approximation

    @param
    solver_par  magma_c_solver_par*
                solver parameters

    @ingroup magmasparse_cgesv
    ********************************************************************/

magma_int_t
magma_cbicgstab_merge( magma_c_sparse_matrix A, magma_c_vector b, 
        magma_c_vector *x, magma_c_solver_par *solver_par ){

    // prepare solver feedback
    solver_par->solver = Magma_BICGSTABMERGE;
    solver_par->numiter = 0;
    solver_par->info = 0;

    // some useful variables
    magmaFloatComplex c_zero = MAGMA_C_ZERO, c_one = MAGMA_C_ONE;
    
    magma_int_t dofs = A.num_rows;

    // GPU stream
    magma_queue_t stream[2];
    magma_event_t event[1];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );
    magma_event_create( &event[0] );

    // workspace
    magma_c_vector q, r,rr,p,v,s,t;
    magmaFloatComplex *d1, *d2, *skp;
    magma_cmalloc( &d1, dofs*(2) );
    magma_cmalloc( &d2, dofs*(2) );
    // array for the parameters
    magma_cmalloc( &skp, 8 );       
    // skp = [alpha|beta|omega|rho_old|rho|nom|tmp1|tmp2]
    magma_c_vinit( &q, Magma_DEV, dofs*6, c_zero );

    // q = rr|r|p|v|s|t
    rr.memory_location = Magma_DEV; rr.val = NULL; rr.num_rows = rr.nnz = dofs;
    r.memory_location = Magma_DEV; r.val = NULL; r.num_rows = r.nnz = dofs;
    p.memory_location = Magma_DEV; p.val = NULL; p.num_rows = p.nnz = dofs;
    v.memory_location = Magma_DEV; v.val = NULL; v.num_rows = v.nnz = dofs;
    s.memory_location = Magma_DEV; s.val = NULL; s.num_rows = s.nnz = dofs;
    t.memory_location = Magma_DEV; t.val = NULL; t.num_rows = t.nnz = dofs;

    rr.val = q(0);
    r.val = q(1);
    p.val = q(2);
    v.val = q(3);
    s.val = q(4);
    t.val = q(5);
    
    // solver variables
    magmaFloatComplex alpha, beta, omega, rho_old, rho_new, *skp_h;
    float nom, nom0, betanom, r0, den;

    // solver setup
    magma_cscal( dofs, c_zero, x->val, 1) ;                            // x = 0
    magma_ccopy( dofs, b.val, 1, q(0), 1 );                            // rr = b
    magma_ccopy( dofs, b.val, 1, q(1), 1 );                            // r = b

    rho_new = magma_cdotc( dofs, r.val, 1, r.val, 1 );             // rho=<rr,r>
    nom = MAGMA_C_REAL(magma_cdotc( dofs, r.val, 1, r.val, 1 ));    
    nom0 = betanom = sqrt(nom);                                 // nom = || r ||                            
    rho_old = omega = alpha = MAGMA_C_MAKE( 1.0, 0. );
    beta = rho_new;
    solver_par->init_res = nom0;
    // array on host for the parameters    
    magma_cmalloc_cpu( &skp_h, 8 );
    skp_h[0]=alpha; 
    skp_h[1]=beta; 
    skp_h[2]=omega; 
    skp_h[3]=rho_old; 
    skp_h[4]=rho_new; 
    skp_h[5]=MAGMA_C_MAKE(nom, 0.0);
    magma_csetvector( 8, skp_h, 1, skp, 1 );
    magma_c_spmv( c_one, A, r, c_zero, v );                     // z = A r
    den = MAGMA_C_REAL( magma_cdotc(dofs, v.val, 1, r.val, 1) );// den = z dot r

    if ( (r0 = nom * solver_par->epsilon) < ATOLERANCE ) 
        r0 = ATOLERANCE;
    if ( nom < r0 )
        return MAGMA_SUCCESS;
    // check positive definite  
    if (den <= 0.0) {
        printf("Operator A is not postive definite. (Ar,r) = %f\n", den);
        return -100;
    }

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

        magmablasSetKernelStream(stream[0]);

        // computes p=r+beta*(p-omega*v)
        magma_cbicgmerge1( dofs, skp, v.val, r.val, p.val );

        magma_c_spmv( c_one, A, p, c_zero, v );                 // v = Ap

        magma_cmdotc( dofs, 1, q.val, v.val, d1, d2, skp );                     
        magma_cbicgmerge4(  1, skp );
        magma_cbicgmerge2( dofs, skp, r.val, v.val, s.val );    // s=r-alpha*v

        magma_c_spmv( c_one, A, s, c_zero, t );                 // t=As

        magma_cmdotc( dofs, 2, q.val+4*dofs, t.val, d1, d2, skp+6 );
        magma_cbicgmerge4(  2, skp );
        magma_cbicgmerge3( dofs, skp, p.val, s.val,     // x=x+alpha*p+omega*s
                            t.val, x->val, r.val );     // r=s-omega*t
        magma_cmdotc( dofs, 2, q.val, r.val, d1, d2, skp+4);
        magma_cbicgmerge4(  3, skp );

        // check stopping criterion (asynchronous copy)
        magma_cgetvector_async( 1 , skp+5, 1, 
                                                        skp_h+5, 1, stream[1] );
        betanom = sqrt(MAGMA_C_REAL(skp_h[5]));

        if( solver_par->verbose > 0 ){
            magma_device_sync(); tempo2=magma_wtime();
            if( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        
        if (  betanom  < r0 ) {
            break;
        }
    }
    magma_device_sync(); tempo2=magma_wtime();
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    float residual;
    magma_cresidual( A, b, *x, &residual );
    solver_par->iter_res = betanom;
    solver_par->final_res = residual;

    if( solver_par->numiter < solver_par->maxiter){
        solver_par->info = 0;
    }else if( solver_par->init_res > solver_par->final_res ){
        if( solver_par->verbose > 0 ){
            if( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) betanom;
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
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = -1;
    }
    magma_c_vfree(&q);  // frees all vectors

    magma_free(d1);
    magma_free(d2);
    magma_free( skp );
    magma_free_cpu( skp_h );

    return MAGMA_SUCCESS;
}   /* cbicgstab_merge */


