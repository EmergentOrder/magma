!
!   -- MAGMA (version 1.5.0-beta3) --
!      Univ. of Tennessee, Knoxville
!      Univ. of California, Berkeley
!      Univ. of Colorado, Denver
!      @date July 2014
!
!  @generated from testing_zgetrf_f.f90 normal z -> s, Fri Jul 18 17:34:25 2014
!
      program testing_sgetrf_f

      use magma

      external slamch, slange, sgemm, sgesv, sgetrs

      real slange, slamch

      real              :: rnumber(2), Anorm, Bnorm, Xnorm, Rnorm 
      real, allocatable :: work(:)
      real, allocatable       :: A(:), B(:), X(:)
      real, allocatable       :: A2(:)
      integer,    allocatable       :: ipiv(:)

      real                    :: zone, mzone
      integer                       :: i, n, info, lda
      integer                       :: nrhs
      real(kind=8)                  :: flops, t, tstart, tend

      PARAMETER          ( nrhs = 1, zone = 1., mzone = -1. )
      
      call cublas_init()

      n   = 2048
      lda = n
 
!------ Allocate CPU memory
      allocate(A(lda*n))
      allocate(A2(lda*n))
      allocate(B(lda*nrhs))
      allocate(X(lda*nrhs))
      allocate(ipiv(n))
      allocate(work(n))

!---- Initializa the matrix
      do i=1,n*n
         call random_number(rnumber)
         A(i) = rnumber(1)
      end do
      A2(:) = A(:)

      do i=1,n*nrhs
         call random_number(rnumber)
         B(i) = rnumber(1)
      end do
      X(:) = B(:)

!---- Call magma LU ----------------
      call magma_wtime_f(tstart)
      call magmaf_sgetrf(n, n, A, lda, ipiv, info)
      call magma_wtime_f(tend)

      if ( info .ne. 0 )  then
         write(*,*) "Info : ", info
      end if

!---- Call solve -------------
      call sgetrs('n', n, nrhs, A, lda, ipiv, X, lda, info)

      if ( info .ne. 0 )  then
         write(*,*) "Info : ", info
      end if

!---- Compare the two results ------
      Anorm = slange('I', n, n,    A2, lda, work)
      Bnorm = slange('I', n, nrhs, B,  lda, work)
      Xnorm = slange('I', n, nrhs, X,  lda, work)

      call sgemm('n', 'n', n,  nrhs, n, zone, A2, lda, X, lda, mzone, B, lda)
      Rnorm = slange('I', n, nrhs, B, lda, work)
            
      write(*,*)
      write(*,*  ) 'Solving A x = b using LU factorization:'
      write(*,105) '  || A || = ', Anorm
      write(*,105) '  || b || = ', Bnorm
      write(*,105) '  || x || = ', Xnorm
      write(*,105) '  || b - A x || = ', Rnorm
      
      flops = 2. * n * n * n / 3.
      t = tend - tstart

      write(*,*)   '  Gflops  = ',  flops / t / 1e9
      write(*,*)

      Rnorm = Rnorm / ( (Anorm*Xnorm+Bnorm) * n * slamch('E') )

      if ( Rnorm > 60. ) then
         write(*,105) '  Solution is suspicious, ', Rnorm
      else
         write(*,105) '  Solution is CORRECT' 
      end if

!---- Free CPU memory
      deallocate(A, A2, B, X, ipiv, work)

!---- Free GPU memory
      call cublas_shutdown()

 105  format((a35,es10.3))

      end
