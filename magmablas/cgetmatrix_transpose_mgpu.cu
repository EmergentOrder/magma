/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @generated from zgetmatrix_transpose_mgpu.cu normal z -> c, Fri Jul 18 17:34:13 2014
       @author Ichitaro Yamazaki
*/
#include "common_magma.h"
#define PRECISION_c
#include "commonblas.h"

//
//    m, n - dimensions in the output (ha) matrix.
//             This routine copies the dat matrix from the GPU
//             to ha on the CPU. In addition, the output matrix
//             is transposed. The routine uses a buffer of size
//             2*lddb*nb pointed to by dB (lddb > m) on the GPU. 
//             Note that lda >= m and lddat >= n.
//
extern "C" void 
magmablas_cgetmatrix_transpose_mgpu(
                  magma_int_t ngpus, magma_queue_t stream[][2],
                  magmaFloatComplex *dat[], magma_int_t ldda,
                  magmaFloatComplex   *ha, magma_int_t lda,
                  magmaFloatComplex  *db[], magma_int_t lddb,
                  magma_int_t m, magma_int_t n, magma_int_t nb)
{
#define   A(j)     (ha  + (j)*lda)
#define  dB(d, j)  (db[(d)]  + (j)*nb*lddb)
#define  dAT(d, j) (dat[(d)] + (j)*nb)
    int nstreams = 2, j, j_local, d, id, ib;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || ngpus*ldda < n || lddb < m){
        printf( "Wrong arguments in magmablas_cgetmatrix_transpose_mgpu (%d<%d), (%d*%d<%d), or (%d<%d).\n",
                (int) lda, (int) m, (int) ngpus, (int) ldda, (int) n, (int) lddb, (int) m );
        return;
    }
    
    /* Move data from GPU to CPU using two buffers; first transpose the data on the GPU */
    for(j=0; j<n; j+=nb){
       d       = (j/nb)%ngpus;
       j_local = (j/nb)/ngpus;
       id      = j_local%nstreams;
       magma_setdevice(d);

       ib = min(n-j, nb);
       magmablas_ctranspose_stream( ib, m, dAT(d,j_local), ldda, dB(d,id), lddb, stream[d][id] );
       magma_cgetmatrix_async( m, ib,
                               dB(d, id), lddb,
                               A(j),      lda, 
                               stream[d][id] );
    }
}
