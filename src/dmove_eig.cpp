/*
    -- MAGMA (version 1.5.0-beta3) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date July 2014

       @author Raffaele Solca
       @author Azzam Haidar

       @precisions normal d -> s

*/
#include "common_magma.h"
extern "C" void
magma_dmove_eig(magma_range_t range, magma_int_t n, double *w, magma_int_t *il,
                     magma_int_t *iu, double vl, double vu, magma_int_t *m)
{
    magma_int_t valeig, indeig, i;

    valeig = (range == MagmaRangeV);
    indeig = (range == MagmaRangeI);

    if (indeig) {
        *m = *iu - *il + 1;
        if (*il > 1)
            for (i = 0; i < *m; ++i)
                w[i] = w[*il - 1 + i];
    }
    else if (valeig) {
        *il=1;
        *iu=n;
        for (i = 0; i < n; ++i) {
            if (w[i] > vu) {
                *iu = i;
                break;
            }
            else if (w[i] < vl)
                ++*il;
            else if (*il > 1)
                w[i-*il+1]=w[i];
        }
        *m = *iu - *il + 1;
    }
    else {
        *il = 1;
        *iu = n;
        *m = n;
    }

    return;
}
