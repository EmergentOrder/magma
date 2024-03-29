/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zmergebicgstab.cu normal z -> d, Fri Jul 18 17:34:28 2014
       @author Hartwig Anzt

*/
#include "common_magma.h"

#define BLOCK_SIZE 512

#define PRECISION_d


// These routines merge multiple kernels from dmergebicgstab into one
// The difference to dmergedbicgstab2 is that the SpMV is not merged into the
// kernes. This results in higher flexibility at the price of lower performance.

/* -------------------------------------------------------------------------- */

__global__ void 
magma_dbicgmerge1_kernel(  
                    int n, 
                    double *skp,
                    double *v, 
                    double *r, 
                    double *p ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    double beta=skp[1];
    double omega=skp[2];
    if( i<n ){
        p[i] =  r[i] + beta * ( p[i] - omega * v[i] );

    }

}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    p = beta*p
    p = p-omega*beta*v
    p = p+r
    
    -> p = r + beta * ( p - omega * v ) 

    Arguments
    ---------

    @param
    n           int
                dimension n

    @param
    skp         double*
                set of scalar parameters

    @param
    v           double*
                input v

    @param
    r           double*
                input r

    @param
    p           double*
                input/output p


    @ingroup magmasparse_dgegpuk
    ********************************************************************/

extern "C" int
magma_dbicgmerge1(  int n, 
                    double *skp,
                    double *v, 
                    double *r, 
                    double *p ){

    
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( (n+BLOCK_SIZE-1)/BLOCK_SIZE );
    magma_dbicgmerge1_kernel<<<Gs, Bs, 0>>>( n, skp, v, r, p );

   return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void 
magma_dbicgmerge2_kernel(  
                    int n, 
                    double *skp, 
                    double *r,
                    double *v, 
                    double *s ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    double alpha=skp[0];
    if( i<n ){
        s[i] =  r[i] - alpha * v[i] ;
    }

}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    s=r
    s=s-alpha*v
        
    -> s = r - alpha * v

    Arguments
    ---------

    @param
    n           int
                dimension n

    @param
    skp         double*
                set of scalar parameters

    @param
    r           double*
                input r

    @param
    v           double*
                input v

    @param
    s           double*
                input/output s


    @ingroup magmasparse_dgegpuk
    ********************************************************************/

extern "C" int
magma_dbicgmerge2(  int n, 
                    double *skp, 
                    double *r,
                    double *v, 
                    double *s ){

    
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( (n+BLOCK_SIZE-1)/BLOCK_SIZE );

    magma_dbicgmerge2_kernel<<<Gs, Bs, 0>>>( n, skp, r, v, s );

   return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void 
magma_dbicgmerge3_kernel(  
                    int n, 
                    double *skp, 
                    double *p,
                    double *se,
                    double *t,
                    double *x, 
                    double *r ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    double alpha=skp[0];
    double omega=skp[2];
    if( i<n ){
        double s;
        s = se[i];
        x[i] = x[i] + alpha * p[i] + omega * s;
        r[i] = s - omega * t[i];
    }

}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    x=x+alpha*p
    x=x+omega*s
    r=s
    r=r-omega*t
        
    -> x = x + alpha * p + omega * s
    -> r = s - omega * t

    Arguments
    ---------

    @param
    n           int
                dimension n

    @param
    skp         double*
                set of scalar parameters

    @param
    p           double*
                input p

    @param
    s           double*
                input s

    @param
    t           double*
                input t

    @param
    x           double*
                input/output x

    @param
    r           double*
                input/output r


    @ingroup magmasparse_dgegpuk
    ********************************************************************/

extern "C" int
magma_dbicgmerge3(  int n, 
                    double *skp,
                    double *p,
                    double *s,
                    double *t,
                    double *x, 
                    double *r ){

    
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( (n+BLOCK_SIZE-1)/BLOCK_SIZE );
    magma_dbicgmerge3_kernel<<<Gs, Bs, 0>>>( n, skp, p, s, t, x, r );

   return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void 
magma_dbicgmerge4_kernel_1(  
                    double *skp ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if( i==0 ){
        double tmp = skp[0];
        skp[0] = skp[4]/tmp;
    }
}

__global__ void 
magma_dbicgmerge4_kernel_2(  
                    double *skp ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if( i==0 ){
        skp[2] = skp[6]/skp[7];
        skp[3] = skp[4];
    }
}

__global__ void 
magma_dbicgmerge4_kernel_3(  
                    double *skp ){
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if( i==0 ){
        double tmp1 = skp[4]/skp[3];
        double tmp2 = skp[0] / skp[2];
        skp[1] =  tmp1*tmp2;
        //skp[1] =  skp[4]/skp[3] * skp[0] / skp[2];

    }
}

/**
    Purpose
    -------

    Performs some parameter operations for the BiCGSTAB with scalars on GPU.

    Arguments
    ---------

    @param
    type        int
                kernel type

    @param
    skp         double*
                vector with parameters


    @ingroup magmasparse_dgegpuk
    ********************************************************************/

extern "C" int
magma_dbicgmerge4(  int type, 
                    double *skp ){

    dim3 Bs( 2 );
    dim3 Gs( 1 );
    if( type == 1 )
        magma_dbicgmerge4_kernel_1<<<Gs, Bs, 0>>>( skp );
    else if( type == 2 )
        magma_dbicgmerge4_kernel_2<<<Gs, Bs, 0>>>( skp );
    else if( type == 3 )
        magma_dbicgmerge4_kernel_3<<<Gs, Bs, 0>>>( skp );
    else
        printf("error: no kernel called\n");

   return MAGMA_SUCCESS;
}

