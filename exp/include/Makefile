#//////////////////////////////////////////////////////////////////////////////
#   -- MAGMA (version 1.5.0-beta3) --
#      Univ. of Tennessee, Knoxville
#      Univ. of California, Berkeley
#      Univ. of Colorado, Denver
#      @date July 2014
#//////////////////////////////////////////////////////////////////////////////

MAGMA_DIR = ..
include ../Makefile.internal

ZHDR =  magma_z.h 	\
	magma_zc.h 	\
	magmablas_z.h	\
	magmablas_zc.h	\
	magma_zlapack.h

-include Makefile.src

ALLHDR= magma.h 	\
	magmablas.h	\
	magma_lapack.h	\
	$(ZHDR) $(CHDR) $(DHDR) $(SHDR)

all: $(ALLHDR)

clean:
	rm -f *~ *.mod

cleanall: clean

