/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zsetmatrix_transpose_mgpu.cu normal z -> d, Fri Jul 18 17:34:13 2014
       @author Ichitaro Yamazaki
*/
#include "common_magma.h"
#define PRECISION_d
#include "commonblas.h"

//
//    m, n - dimensions in the source (input) matrix.
//             This routine copies the ha matrix from the CPU
//             to dat on the GPU. In addition, the output matrix
//             is transposed. The routine uses a buffer of size
//             2*lddb*nb pointed to by dB (lddb > m) on the GPU. 
//             Note that lda >= m and lddat >= n.
//
extern "C" void 
magmablas_dsetmatrix_transpose_mgpu(
                  magma_int_t ngpus, magma_queue_t stream[][2],
                  const double *ha,  magma_int_t lda, 
                  double       *dat[], magma_int_t ldda, 
                  double       *db[],  magma_int_t lddb,
                  magma_int_t m, magma_int_t n, magma_int_t nb)
{
#define   A(j)    (ha       + (j)*lda)
#define  dB(d, j) (db[(d)]  + (j)*nb*lddb)
#define dAT(d, j) (dat[(d)] + (j)*nb)
    magma_int_t nstreams = 2, d, j, j_local, id, ib;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || ngpus*ldda < n || lddb < m){
        printf( "Wrong arguments in magmablas_dsetmatrix_transpose_mgpu (%d<%d), (%d*%d<%d), or (%d<%d).\n",
                (int) lda, (int) m, (int) ngpus, (int) ldda, (int) n, (int) lddb, (int) m );
        return;
    }
    
    /* Move data from CPU to GPU by block columns and transpose it */
    for(j=0; j<n; j+=nb){
       d       = (j/nb)%ngpus;
       j_local = (j/nb)/ngpus;
       id      = j_local%nstreams;
       magma_setdevice(d);

       ib = min(n-j, nb);
       magma_dsetmatrix_async( m, ib,
                               A(j),      lda,
                               dB(d, id), lddb, 
                               stream[d][id] );

       magmablas_dtranspose_stream( m, ib, dB(d,id), lddb, dAT(d,j_local), ldda, stream[d][id] );
    }
}
