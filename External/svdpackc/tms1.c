/*************************************************************************
                           (c) Copyright 1993
                        University of Tennessee
                          All Rights Reserved                          
 *************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include "tmsg.h"
#include "tmsc.h"

/* #define  UNIX_CREAT */

#ifdef UNIX_CREAT
#define PERMS 0664
#endif

long tsvd1(FILE *, long, long, long, long, double, double,
           double *, long, long *, long *, long *, double *, double *,
           long *, double **, double **, double **, double **,
           double **, double **, long **, long *);
long   check_parameter(long, FILE *, long, long, long, long, long, long);
double norm_1();
float  timer(void);


/***********************************************************************
 *                                                                     *
 *                        main()                                       *
 *     Sparse SVD Via Eigensystem of Equivalent 2-Cyclic Matrix        *
 *                  (double precision)                                 *
 *                                                                     *
 ***********************************************************************

   Description
   -----------
   This sample program tms1 uses  subroutine tsvd1 to compute the p-largest
   singular triplets of A via the equivalent symmetric eigensystem of

       [alpha*I   A]
   B = [           ]  , where A is m (nrow) by n (ncol) (nrow >> ncol),
       [A'  alpha*I]

   so that {u, abs(lambda-alpha), v} is a singular triplet of A.
   alpha is chosen so that B is (symmetric) positive definite. In
   the calling program,alpha is determined as the matrix 1-norm of
   A. The user can set alpha to be any known upper bound for the 
   largest singular value of the matrix A.
   (A' = transpose of A)

   User supplied routines:  opb,opat,timer

   opat(x,y) takes an m-vector x and should return A'*x in y.
   opb(n,x,y,shift) takes an n-vector x and should return D*x in y,
                where D=[B-shift*I],I is the n-th order identity matrix.

   User should edit timer() with an appropriate call to an intrinsic
   timing routine that returns elapsed user cpu time.

   tms1 utilizes ritz-shifting for acceleration of convergence.

   External parameters
   -------------------
   Defined and documented in tmsg.h

   Local parameters
   ----------------
  (input)

   p     : number of singular triplets (largest) desired
   s     : initial subspace dimension
   job   : controls use of ritz shifting (0=no , 1=yes)
   tol   : user-specified tolerance for residuals
   red   : residual norm reduction factor for initiating shifting
   v     : output singular vectors ? (boolean)
   n     : dimension of the eigenproblem for matrix B(nrow + ncol)

  (output)

  sig    : array of singular approximate values
  time   : time breakdown
  iwall  : wall clock time   

   Functions used
   --------------

   BLAS         daxpy,dscal,ddot,dcopy,dswap,dgemm,dgemv,dasum
   USER         opb,opat,timer
   MISC         write_header,check_parameters
   TMS1         tsvd1

   Precision
   ---------

   All floating-point calculations use double precision,
   variables are declared as long or double.

   TMS1 development
   ----------------

   TMS1 is a C translation of the Fortran-77 TMS1 from the SVDPACK
   library written by Michael W. Berry, University of Tennessee,
   Dept. of Computer Science, 107 Ayres Hall, Knoxville, TN, 37996-1301

   August  24 1992:  Date written
   October 23 1996:  Updated main() to produce correct s-vector output
                     to fp_out2

   Vijay Krishna K.
   University of Tennessee
   Dept. of Computer Science
   107 Ayres Hall
   Knoxville, TN, 37996-1301
   internet: krishna@cs.utk.edu

 **********************************************************************/

main()
{
   double  time[5], red, exetime, *sig, *res;
   double  *tempptr1, *workptr1, total, **tempptr2, **workptr2, meps;

   long    *workptr3, **workptr4, *tempptr3, **tempptr4,length,memsize;
   long    k, i, j, id, p, size1, size2, s, job, mem, vec, nnzero;
   long    n,iwall, mxv[3], itime[4], *itcgt, *titer, maxi;

   char    title[73], name[41], v[6], *in1, *in2, *out1, *out2;
   FILE    *fp_in1, *fp_in2;

   in1 = "tmp1";
   in2 = "matrix";
   out1 = "tmo1";
   out2 = "tmv1";

   /* open files for input/output */
   if(!(fp_in1 = fopen(in1, "r"))) {
     printf("cannot open file %s for reading\n", in1);
     exit(-1);
   }
   if(!(fp_in2 = fopen(in2, "r"))) {
     printf("cannot open file %s for reading\n", in2);
     exit(-1);
   }
   if(!(fp_out1 = fopen(out1, "w"))) {
     printf("cannot open output file %s \n", out1);
     exit(-1);
   }

   /* read data */
   fscanf (fp_in2,"%72c%*s%*s%*s%ld%ld%ld%*d",
           title, &nrow, &ncol, &nnzero);
   title[73] = '\0';
   fscanf (fp_in1,"%s %ld %ld %ld %lf %lf %s %ld", 
           name, &p, &s, &job, &tol,&red,v,&maxi);
   if(!(strcmp(v, "TRUE"))) {
     vec = 1;
#if  !defined UNIX_CREAT
       if ((fp_out2 = open(out2, O_CREAT | O_RDWR)) == -1) {
          printf("cannot open output file %s \n", out2);
          exit(-1);
       }
#else
       if ((fp_out2 = creat(out2, PERMS )) == -1) {
          printf("cannot open output file %s \n", out2);
          exit(-1);
       }
#endif
   }
   else vec = 0;

   n = nrow + ncol;

   /* even though the validity of the parameters will be checked in the
    * SVD code, some parameter checking should also be done before
    * allocating memory to ensure that they are non-negative */

   if(check_parameter(n, fp_out1, maxi, NMAX, NZMAX, p, vec, nnzero)) {
     fclose(fp_in1);
     fclose(fp_in2);
     fclose(fp_out1);
     if(vec) close(fp_out2);
     exit(-1);
   }
   /*******************************************************************
    * Allocate Memory                                                 *
    * pointr - column start array of harwell-boeing sparse matrix     *
    *          format                                       (ncol+1)  *
    * rowind - row indices array of harwell-boeing format   (nnzero)  *
    * value  - nonzero values array of harwell-boeing sparse matrix   *
    *          format                                       (nnzero)  *
    *******************************************************************/
        size1 = sizeof(double) * (nnzero);
        size2 = sizeof(long) * (ncol + nnzero + 1);
        if(!(pointr = (long *)   malloc(size2))   ||
           !(value  = (double *) malloc(size1))) {
            perror("FIRST MALLOC FAILED IN TMS1()");
            exit(errno);
        }

   /* calculate memory allocated */
   mem = size1 + size2;

   rowind = pointr + (ncol + 1);

   /* skip data format line */
   fscanf(fp_in2,"%*s %*s %*s %*s");

   /* read data */
   for(i=0; i<=ncol; i++) fscanf(fp_in2, "%ld", &pointr[i]);
   for(i=0; i<ncol; i++) pointr[i] -= 1; 

   /* define last element of pointr in case it is not */
   pointr[i] = nnzero;
   for(i=0; i<nnzero; i++) fscanf(fp_in2, "%ld", &rowind[i]);
   for(i=0; i<nnzero; i++) rowind[i] -= 1;
   for(i=0; i<nnzero; i++) fscanf(fp_in2, "%lf", &value[i]);

  /* allocate memory for arrays */

   memsize = sizeof(double) * ((n * (s + s + 3 + s)) + ( s*(4 + s) ) 
                                + s + s  );

   mem   += memsize;

   /*******************************************************************
    *       Allocate work area and initialize pointers                *
    *                                                                 *
    *       pointer                size                               *
    *                                                                 *
    *    w1   (work1)           n  ( n x  s)                          *
    *    w2   (work2)           n  ( n x  s)                          *
    *    w3   (work3)           s  ( s x  s)                          *
    *    w4   (work4)           n  ( n x  3)                          *
    *    w5   (work5)           s  ( s x  4)                          *
    *    yy   (y     )          n  ( n x  s)                          *
    *    sig                    s                                     *
    *    res                    s                                     *
    *                                                                 *
    *    (memory allocated separately)                                *
    *                                                                 *
    *    iwork(iw   )           s  ( s x  2)                          *
    *    lwork                  s                                     *
    *    titer                  s                                     *
    *    itcgt                  s                                     *
    *                                                                 *
    *******************************************************************/
   if(!(workptr1 = (double *)malloc(memsize))) {
      perror("SECOND MALLOC FAILED IN TMS1()");
     exit(errno);
   }
  
   memsize = sizeof(double *) * (s + s + s + 3 + 4 + s);
   mem   += memsize;

   if(!(workptr2 = (double **)malloc(memsize))) {
     perror("THIRD MALLOC FAILED IN TMS1()");
     exit(errno);
   }
   tempptr1 = workptr1;
   tempptr2 = workptr2;
 
   length    = n * s;
   w1        = tempptr1;
   tempptr1 += length;
   work1     = tempptr2;
   tempptr2 += s;
   j=0;
   for(i=0; i<length; i+=n) work1[j++] = &w1[i];
 
   length    = n * s;
   w2        = tempptr1;
   tempptr1 += length;
   work2     = tempptr2;
   tempptr2 += s;
   j=0;
   for(i=0; i<length; i+=n) work2[j++] = &w2[i];
 
   length    = s * s;
   w3        = tempptr1;
   tempptr1 += length;
   work3     = tempptr2;
   tempptr2 += s;
   j=0;
   for(i=0; i<length; i+=s) work3[j++] = &w3[i];
 
   length    = n * 3;
   w4        = tempptr1;
   tempptr1 += length;
   work4     = tempptr2;
   tempptr2 += 3;
   j=0;
   for(i=0; i<length; i+=n) work4[j++] = &w4[i];

   length    = s * 4;
   w5        = tempptr1;
   tempptr1 += length;
   work5     = tempptr2;
   tempptr2 += 4;
   j=0;
   for(i=0; i<length; i+=s) work5[j++] = &w5[i];
 
   length    = n * s;
   yy        = tempptr1;
   tempptr1 += length;
   y         = tempptr2;
   tempptr2 += s;
   j=0;
   for(i=0; i<length; i+=n) y[j++] = &yy[i];
 
   sig       = tempptr1;
   tempptr1 += s;
 
   res       = tempptr1;
   tempptr1 += s;
 
   /* Allocate memory for logical array lwork (long 0= false and 1=true)
      and integer array iwork                                    */
 
   memsize = sizeof(long) * ( s * (2 + 1) + s + s );
   mem   += memsize;
 
   if(!(workptr3 = (long *)malloc(memsize)) ) {
      perror("FOURTH MALLOC FAILED IN TMS1()");
      exit(errno);
   }
 
   memsize = sizeof(long *) * 2;
   mem   += memsize;
 
   if(!(workptr4 = (long **)malloc(memsize))) {
      perror("FIFTH MALLOC FAILED IN TMS1()");
      exit(errno);
   }
 
   tempptr3 = workptr3;
   tempptr4 = workptr4;
 
   length    = s * 2;
   iw        = tempptr3;
   tempptr3 += length;
   iwork     = tempptr4;
   tempptr4 += 2;
   j=0;
   for(i=0; i<length; i+=s) iwork[j++] = &iw[i];
 
   lwork     = tempptr3;
   tempptr3 += s;
 
   titer     = tempptr3;
   tempptr3 += s;
 
   itcgt     = tempptr3;
   tempptr3 += s;
 

 
   /* estimate alpha (via matrix 1-norm)
    * used for upper bound on max singular value of matrix A */

   alpha = norm_1();
   exetime = timer();

   /* make a trace min. run; exit upon error */ 
   ierr = tsvd1(fp_out1, n, p, s, job, tol, red, sig, maxi,
                &mem, itcgt, titer, time, res,  mxv, work1,
                work2, work3, work4, work5, y, iwork, lwork);
   if(ierr) {
     free(value);
     free(pointr);
     fclose(fp_in1);
     fclose(fp_in2);
     fclose(fp_out1);
     if(vec) close(fp_out2);
     free(workptr1);
     free(workptr2);
     free(workptr3);
     free(workptr4);
     exit(-1);
   } 
   exetime  = timer() - exetime;
   iwall    = (int) (fmod(exetime+1000000.0,1000000.0));
   itime[0] = (int) (100*(time[0]/time[4]));
   itime[1] = (int) (100*(time[1]/time[4]));
   itime[2] = (int) (100*(time[2]/time[4]));
   itime[3] = (int) (100*(time[3]/time[4])); 

   /* write output */

   fprintf(fp_out1, " ... \n");
   fprintf(fp_out1, " ... TRACE MINIMIZATION ON THE  \n");
   fprintf(fp_out1, " ... EQUIVALENT CYCLIC   E_PROBLEM\n");
   fprintf(fp_out1, " ... NO. OF EQUATIONS            =%10ld\n", n);
   fprintf(fp_out1, " ... MAX. NO. OF ITERATIONS      =%10ld\n", maxi);
   fprintf(fp_out1, " ... NO. OF ITERATIONS TAKEN     =%10ld\n",
           titer[p-1]);
   fprintf(fp_out1, " ... NO. OF DESIRED EIGENPAIRS   =%10ld\n",p);
   fprintf(fp_out1, " ... INITIAL SUBSPACE DIM.       =%10ld\n",s);
   fprintf(fp_out1, " ... FINAL   SUBSPACE DIM.       =%10ld\n",s-p);
   fprintf(fp_out1, " ... MEMORY REQUIRED (BYTES)     =%10ld\n",mem);
   fprintf(fp_out1, " ... JOB: 0=NO SHIFT, 1=SHIFT    =%10ld\n",job);
   if (vec)
     fprintf(fp_out1, " ... WANT S-VECTORS?   [T/F]   =           T\n");
   else
     fprintf(fp_out1, " ... WANT S-VECTORS?   [T/F]   =           F\n");
   fprintf(fp_out1, " ... ALPHA                       =%10.2E\n",alpha);
   fprintf(fp_out1, " ... FINAL RESIDUAL TOLERANCE    =%10.2E\n" ,tol);
   fprintf(fp_out1, " ... RESIDUAL REDUCTION TOL.     =%10.2E\n",red);
   fprintf(fp_out1, " ... MULTIPLICATIONS BY A        =%10ld\n",mxv[0]);
   fprintf(fp_out1, " ... MULTIPLICATIONS BY A^T      =%10ld\n",mxv[1]);
   fprintf(fp_out1, " ... TOTAL NUMBER OF MULT.       =%10ld\n",mxv[2]);
   fprintf(fp_out1, " ... IERR FROM SUBR. TSVD1       =%10ld\n\n",ierr);
   fprintf(fp_out1,"... CPU TIME BREAKDOWN:\n");
   fprintf(fp_out1,"... GRAM-SCHMIDT ORTHOG.          =%10.2E (%3ld%%) \n",
           time[0],itime[0]);
   fprintf(fp_out1,"... SPECTRAL DECOMPOSITION        =%10.2E (%3ld%%) \n",
           time[1],itime[1]);
   fprintf(fp_out1,"... CONVERGENCE CRITERIA          =%10.2E (%3ld%%) \n",
           time[2],itime[2]);
   fprintf(fp_out1,"... CONJUGATE GRADIENT            =%10.2E (%3ld%%) \n",
           time[3], itime[3]);
   fprintf(fp_out1,"... TOTAL CPU  TIME (SEC)         =%10.2E\n", time[4]);
   fprintf(fp_out1,"... WALL-CLOCK TIME (SEC)         =%10ld\n\n", iwall);
   fprintf(fp_out1, " %s\n", title);
   fprintf(fp_out1, "           %s\n", name);
   fprintf(fp_out1, " ... NO. OF TERMS     (ROWS)   = %10ld\n", nrow);
   fprintf(fp_out1, " ... NO. OF DOCUMENTS (COLS)   = %10ld\n", ncol);
   fprintf(fp_out1, " ... ORDER OF MATRIX A         = %10ld\n", n);
   fprintf(fp_out1,"......\n");
   fprintf(fp_out1, "...... COMPUTED SINGULAR VALUES  (RESIDUAL NORMS)  T-MIN STEPS CG STEPS\n"); 
   fprintf(fp_out1,"......\n");
   for(i=0; i<p; i++)
   fprintf(fp_out1, "... %2ld  %24.14E ( %10.2E)       %ld           %ld \n",
           i+1, sig[i], res[i],titer[i],itcgt[i]); 
   if (vec) {
       for (i = 0;  i < p;  i++)
          write(fp_out2, (void *) &y[i][0], sizeof(double) * n); }

   free(value);
   free(pointr);
   fclose(fp_in1);
   fclose(fp_in2);
   fclose(fp_out1);
   if (vec) close(fp_out2);
   free(workptr1);
   free(workptr2);
   free(workptr3);
   free(workptr4);
   exit(0);
}
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include "tmsc.h"

extern long    nrow, ncol;
extern double  *z,*v2,*r,*r2,*pp,*z2,alpha;

double epslon(double);
double random(long *);
void orthg(long, long, long, double **, double **, double *);
void opat(double *,double *);
void dgemm3(long, long, long, long, long, double, double **,
            long, long, double **, long, long, double,
            double **, long, long);
double dasum(long, double *, long);
float timer(void);
void opb(long, double *, double *,double);
void daxpy(long, double, double *,long, double *, long);
void tred2(long, long, double **, double *, double *, double **);
long tql2(long, long, double *, double *, double **);
void disk(long, double *, double *, long *, long *);
void isol(long, double, double, long *, double *, double *, long);
void clus(long, long, double *, double, long *, double *, double *,
          double *, long);
void cgt(long, long, double *, double **, long *, double, double,
         long *, double);
void cgts(long, long, double *, double **, long *, double, double,
          double, double, long *, double);
double ddot(long, double *, long, double *, long);

/***********************************************************************
 *                       tsvd1()                                       *
 *     Sparse SVD Via Eigensystem of Equivalent 2-Cyclic Matrix        *
 *                  (double precision)                                 *
 ***********************************************************************
   Description
   -----------

   tsvd1 implements a trace-minimization svd method for determining
   the p-largest singular triplets of a large sparse matrix A.

   tms1 computes the singular triplets of the matrix a via the 
   equivalent symmetric eigenvalue problem.
   
       [alpha*I     A]
   B = [             ] , where A is m (=nrow) by n (=ncol),
       [A'    alpha*I]
    
   so that {u,abs(lambda-alpha),v} is a singular triplet of A.
   alpha is chosen so that B is (symmetric) positive definite. In
   the calling program, alpha is determined as the matrix 1-norm of
   A.  The user can set alpha to be any known upper bound for the
   the largest singular values of the matrix A.
   (A' = transpose of A)
     
   User supplied routines: opat,opb,timer
 
   opat(x,y)                takes an m-vector x and should return A'*x
                            in y.
   opb(nrow+ncol,x,y,shift) takes an (m+n) vector x and should return
                            D*x in y, where D=[B-shift*I], I is the 
                            (m+n) order identity matrix.

   User should replace calls to timer() with an appropriate call to
   an intrinsic timing routine that returns delta user cpu time
   (i.e., user cpu time elapsed from a previous call to timing routine)

   tsvd1 utilizes ritz-shifting for acceleration of convergence.

   External parameters
   -------------------

   Defined and documented in tmsg.h

   Local parameters:
   ----------------
   (input) 
   n               order of equivalent symmetric eigenvalue problem
                   should be equal to nrows+ncols, where nrows is the
                   number of rows of A and ncols is the number of
                   columns of A.

   p               number of desired singular triplets (largest) of A.

   s               dimension of initial subspace (s should be greater
                   than p;s=2*p is usually safe but since the complexity
                   of the method is determined by s, the user should
                   try to keep s as close to p as possible).

   job             acceleration strategy switch:

                   job = 0, no acceleration used,
                       = 1, ritz-shifting used.

   maxi            maximum number of trace minimization steps allowed.

   tol             user-supplied tolerance for residual of an
                   equivalent eigenpair of B (singular triplet of A).

   red             user-supplied tolerance for residual reduction to
                   initiate ritz-shifting when job=1.
   lwork(s)        (logical 0,1) work array.

   (output parameters):
    -----------------

   ierr            ierr=99, input parameter job invalid.
   (error flag)
   mem             number of bytes of memory needed

   mxv(3)          matrix-vector multiplication counters:
                   mxv[0] = number of A*x,
                   mxv[1] = number of A'*x,
                   mxv[2] = mxv[0] + mxv[1]

   sig(s)          contains p-desired singular values.

   y(n,s)          first p columns contains left and right singular
                   vectors, i.e.,
                   y(1:n,1:p)  = [u(1:r,1:p)' | v(1:c,1:p)']',
                   where r = no. of rows of matrix A and
                   c = no. of cols of matrix A.

   titer(s)        titer(i) := number of trace min. steps
                   need for i-th triplet of A.

   itcgt(s)        itcgt(i) := number of cg steps needed for
                   i-th triplet of A.

   time(5)         timing breakdown array:
                   time[0]  = gram-schmidt orthogonalization
                   time[1]  = section-formation (spectral decomposition)
                   time[2]  = convergence criteria
                   time[3]  = conjugate gradient method
                   time[4]  = total execution time (user-cpu)

   res(s)          2-norms of residual vectors
                   (A*y[i]-sig(i)*y[i]), i=1,2,...,p.

   Functions used
   --------------
    BLAS       daxpy, ddot, dgemm, dgemv
    USER       timer,opb,opat
    RNG        random
    PRECISION  epslon
    MATH       dsqrt,fabs
    EISPACK    tred2,tql2,pythag

   Precision
   ---------
   All floating-point calculations use double precision;
   variables are declared as long or double. */

long tsvd1(FILE *fp_out1, long n, long p, long s, long job,
           double tol, double red, double *sig, long maxi,
           long *mem, long *itcgt, long *titer, double *time,
           double *res, long *mxv, double **work1, double **work2,
           double **work3, double **work4, double **work5,
           double **y, long **iwork, long *lwork)
{
  double  t1,sec1,sec2,sec21,sec22,sec23,sec3,sec4,meps,total;
  double  *tempptr5,*workptr5;

  long    iptr, left, i, j, k, l, irand, iter, ierr;
  long    shift, memsize, length, temp_left;

  /* allocate memory for temporary arrays in cgt and cgts subr. */

  memsize = sizeof(double) * n * 6;
  if(!(workptr5 = (double *) malloc(memsize))) {
    perror("MALLOC FAILED IN TSVD1()\n");
    exit(errno);
  }
  *mem += memsize;
  tempptr5 = workptr5;

  z = tempptr5;
  tempptr5 += n;
  v2 = tempptr5;
  tempptr5 +=n;
  r = tempptr5;
  tempptr5 += n;
  r2 = tempptr5;
  tempptr5 += n;
  pp = tempptr5;
  tempptr5 += n;
  z2 = tempptr5;

  /* get machine epsilon (meps) */

  meps  = epslon(ONE);
  shift = FALSE;
  ierr  = 0;
  if((job != 0) &&  (job != 1)) {
     ierr = 99;
     return(ierr);
  }

  if(job == 1) {
     for(i=0; i<p; i++) {
         lwork[i]    =  FALSE;
         work5[2][i] = -ONE;
         work5[3][i] = -ONE;
     }
  }

  /* initialize timers and counters: */
  sec1  = ZERO;
  sec21 = ZERO;
  sec22 = ZERO;
  sec23 = ZERO;
  sec3  = ZERO;
  sec4  = ZERO;
  mxv[0] = 0;
  mxv[1] = 0;

  /* initialize y(1:n,1:s) = random matrix
     (carry s vectors in the tmin iterations, assuming s.ge.p) */

  irand = SEED;
  for(k=0; k<s; k++) {
     sig[k]      = ZERO;
     work5[1][k] = ZERO;
  }

  for(k=0; k<s; k++) {
       for (i=0; i<n; i++) y[k][i] = random(&irand);
  }

  /*----------------------------------------------------------
       pointer and counter for hybrid monitor:
  
              1 2 3 4 5 ... i ... p
             -----------------------
         sig:| | | | | |...| |...| | (ascending order)
             -----------------------
                            ^
                            |
                          iptr : points to first e-value of B
                                 that has not converged
  
       left:=s-iptr+1 (ie how many y-vectors remain for tsvd1)
  -------------------------------------------------------------- */
  
  /* initialize a few pointers and counters: */
  total = ZERO;
  iptr = 1;
  left = s;
  for(i=0; i<s; i++) {
      titer[i] = 0;
      itcgt[i] = 0;
  }

  /*--------------------------------------------------------------
       main tmin iteration loop (nmax iterations)
   --------------------------------------------------------------*/
  for(iter=0; iter<maxi; iter++) {

     t1 = timer();

     for(i=0; i<s; i++) {
      for(j=0; j<n; j++) work2[i][j] = y[i][j];
     }
     orthg(s,0,n,work1,work2,&work4[0][0]);

     t1    = timer() - t1;
     sec1 += t1;
  
     /* form work1(1:n,iptr:s) = b(1:n,1:n)*work2(1:n,iptr:s) */
  
     t1 = timer();
  
     /* economization of 2-cyclic form used here */
  
     for(i=iptr-1; i<s; i++) opat(work2[i], work1[i]);
     mxv[1] += s - iptr + 1;
  
     /* form work3(1:left,1:left)=work2(1:n,iptr:s)'*work1(1:n,iptr:s)*/

     dgemm3(TRANSP, NTRANSP, left, left, ncol, ONE, work2, nrow,iptr-1,
            work1, 0, iptr-1, ZERO, work3, 0, 0);
     
     for(j=0; j<left; j++) {
         for(i=j; i<left; i++) work3[j][i] += work3[i][j];
         for(i=j; i<left; i++) work3[i][j] = work3[j][i];
         work3[j][j] += alpha;
     }

     /* load gershgorin radii */

     if(job == 1) {
       for (j=0; j<left; j++)
       work4[0][j] = dasum(left, &work3[0][j], s) - fabs(work3[j][j]);
     }

     t1     = timer() - t1;
     sec21 += t1;

     /* compute the eigenvalues and eigenvectors of work3:  */
     
     t1 = timer();

     /* eigenvectors overwrite array work3(:,:)
        store current e-values in work5(:,2)  */
     
     for(j=iptr-1; j<s; j++) work5[1][j] = sig[j];
     
     tred2(0,left, work3, &sig[iptr-1], &work4[1][0], work3);
     ierr = tql2(0,left, &sig[iptr-1], &work4[1][0], work3);
     
     t1 = timer() - t1;
     sec22 += t1;
     
     /* form y(1:n,iptr:s) = work2(1:n,iptr:s)*work3(1:left,1:left) */

     t1 = timer();

     dgemm3(NTRANSP, NTRANSP, n, left, left, ONE, work2, 0, iptr-1,
            work3, 0, 0, ZERO, y, 0, iptr-1);

     t1     = timer() - t1;
     sec23 += t1;
     
     /* test for convergence here */
     
     t1 = timer();

     for(j=iptr-1; j<s; j++) {
         opb(n, y[j], work2[j], ZERO);
         daxpy(n, -sig[j], y[j], 1, work2[j],1);
         work4[2][j-iptr+1]  = sqrt(ddot(n, work2[j], 1,work2[j],1));
         if(j < p) titer[j] += 1;
     }
     mxv[0] += s-iptr+1;
     mxv[1] += s-iptr+1;
     t1 = timer() - t1;
     sec3 += t1;

    /* array work4(:,3) stores the vector 2-norms of residual vectors */

     temp_left=left; 
     for(k=0; k<temp_left; k++)
         if(fabs(work4[2][k]) <= tol) {
            iptr = iptr + 1;
            if(iptr > p) goto L10;
            left = s-iptr+1;
         }

     if((!shift) && (job==1)) {
     
       /* work4(:,1) stores gershgorin radii  *
        * iwork(:,1) is the clustering vector */ 

       disk(left, &sig[iptr-1], work4[0], iwork[0], iwork[1]);
       
       for(k=iptr-1; k<s; k++) {
          if(iwork[0][k-iptr+1] == 1)
            isol(k, work4[2][k-iptr+1], red, lwork, work5[2],
                 work5[3], s);
          else {
             if(iwork[0][k-iptr+1] > 1)
               clus(k, iwork[0][k-iptr+1], &work4[2][k-iptr+1], red,
                    lwork, work5[2], work5[3], work4[1], s);
          }
       }
     }
       
     /* continue algorithm...  */
       
     /* shift start */
     
     if((iter>0) && (!shift) && (job==1)) {
        if(lwork[iptr-1]) {
           shift = TRUE;
           orthg(s, 0, n, work1, y, &work4[0][0]);
        }
     }

     if(shift) {
     
     /* compute shifts in work5(:,1) here: */
  
      for(k=iptr-1; k<s; k++) {
         if((k>iptr-1) && (k<=p-1)) {
           work5[0][k] = ZERO;
           for(j=iptr-2; j<k; j++) {
              if((j!=-1) && (sig[j] <= (sig[k] - work4[2][k-iptr+1]))) 
                work5[0][k] = sig[j];
           }
           if((work5[0][k] == sig[k-1]) && (work5[0][k-1]==sig[k-1]))
             work5[0][k] = sig[k];
           if(work5[0][k] == ZERO) 
             work5[0][k] = work5[0][k-1];
         }
         else if(k>p-1) work5[0][k] = work5[0][p-1];
              else if(k==iptr-1) work5[0][k] = sig[k];
      }
     }

     t1 = timer();
     
     /* do cg iterations */

     /* load y into work1 array for orthog. projector in subr. cgt */

     for(j=0; j<left; j++) {
        for(i=0; i<n; i++) work1[j][i] = y[iptr+j-1][i];
     }
           

    /* cg loop for independent systems (no shifting used)  */
     if(!shift) 
       for(i=0; i<left; i++) {
           cgt(n, left, y[iptr+i-1], work1, &itcgt[iptr+i-1],
               sig[iptr+i-1], sig[s-1], &iwork[0][i], meps);
       }
     else

         /* shift with work5(:,1) in cg iterations */

       for(i=0; i<left; i++) {
          cgts(n, left, y[iptr+i-1], work1, &itcgt[iptr+i-1],
               sig[iptr+i-1], work5[1][iptr+i-1], sig[s-1],
               work5[0][iptr+i-1], &iwork[0][i], meps);
       }

     for(i=0; i<left; i++) {
        mxv[0] += iwork[0][i];
        mxv[1] += iwork[0][i];
     }

     t1    = timer() - t1;
     sec4 += t1;
  }
L10:     

   /* compute final 2-norms of residual vectors w.r.t. b (indefinite) */

   for(j=0; j<p; j++) {
     opb(n, y[j], work2[j], alpha);
     sig[j]=ddot(n, y[j], 1, work2[j], 1)/ddot( n, y[j], 1, y[j], 1);
     daxpy(n, -sig[j], y[j], 1, work2[j],1);
     work4[2][j] = sqrt(ddot(n, work2[j], 1,work2[j],1));
     sig[j]      = fabs(sig[j]);
   }
   mxv[0] += p;
   mxv[1] += p;
   
   sec2   = sec21 + sec22 + sec23;
   total += sec1 + sec2 + sec3 + sec4;
   
   /* load output time and mxv arrays */
   time[0] = sec1;
   time[1] = sec2;
   time[2] = sec3;
   time[3] = sec4;
   time[4] = total;
   mxv[2]  = mxv[0]+mxv[1];
  
  /* load residual vector */

  for(i=0; i<p; i++) res[i] = work4[2][i];
  return(0);
}  
/* end of tsvd1 */
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

double epslon(double x)
{
  /* Estimate unit roundoff in quantities of size x 
     
     This function should work properly on all systems
     satisfying the following two assumptions,
     1. The base used in representiong floating point
        numbers is not a power of three.
     2. The quantity a in statement 10 is represented to 
        the accuracy used in floating point variables
        that are stored in memory.
     The statement number 10 and the while are intended 
     to force optimizing compilers to generate code
     satisfying assumption 2.
     Under these assumptions, it should be true that,
         A is not exactly equal to four-thirds,
         B has a zero for its last bit or digit,
         C is not exactly equal to one,
         EPS measures the separation of 1.0 from
             the next larger floating point number.
     This routine is based on Eispack. The developers of
     Eispack would appreciate being informed about any
     systems where these assumptions do not hold. */

  double  a,b,c,eps;

  a = 4.0/3.0;
  do {
     b = a - ONE; /* statement 10 */
     c = b + b + b;
     eps = fabs(c -ONE);
  }
  while (eps == ZERO);  
  eps = eps*fabs(x);
  return(eps);
}
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>

extern long ncol,nrow;
extern char *error[];
/***********************************************************************
 *                    check_parameters()                               *
 ***********************************************************************
   Description
   -----------
   Function validates input parameters and returns error code (long)

   Parameters
   ----------
  (input)
   p        desired number of eigenpairs of B.
   maxi     upper limit of desired number of trace steps or iterations.
   n        dimension of the eigenproblem for matrix B
   vec      1 indicates both singular values and singular vectors are wanted.
            0 indicates singular values only.
   nnzero   number of nonzero elements in input matrix (matrix A)

**************************************************************************/

long check_parameter(long n, FILE *fp_out1, long maxi, long nmax,
                     long nzmax, long p, long vec, long nnzero)
{
   long error_index, ncells;

   error_index = 0;
   if(ncol >= nmax || nnzero > nzmax) error_index = 1;
   else if(p > maxi)  error_index = 3;
   else if(n <= 0)  error_index = 4;
   else if(maxi <= 0 )  error_index = 5;
   else if(p <= 0 || p > maxi)  error_index = 6;
   if(error_index)
     fprintf(fp_out1, "%s\n", error[error_index]);
   return(error_index);
}
/************************************************************** 
 * Function copies a vector x to a vector y	     	      *
 * Based on Fortran-77 routine from Linpack by J. Dongarra    *
 **************************************************************/ 

void dcopy(long n,double *dx,long incx,double *dy,long incy)

{
   long i;

   if (n <= 0 || incx == 0 || incy == 0) return;
   if (incx == 1 && incy == 1) 
      for (i=0; i < n; i++) *dy++ = *dx++;

   else {
      if (incx < 0) dx += (-n+1) * incx;
      if (incy < 0) dy += (-n+1) * incy;
      for (i=0; i < n; i++) {
         *dy = *dx;
         dx += incx;
         dy += incy;
      }
   }
   return;
}
/************************************************************** 
 * Function scales a vector by a constant.     		      *
 * Based on Fortran-77 routine from Linpack by J. Dongarra    *
 **************************************************************/ 

void dscal(long n,double da,double *dx,long incx)

{
   long i;

   if (n <= 0 || incx == 0) return;
   if (incx < 0) dx += (-n+1) * incx;
   for (i=0; i < n; i++) {
      *dx *= da;
      dx += incx;
   }
   return;
}
/************************************************************** 
 * Function interchanges two vectors		     	      *
 * Based on Fortran-77 routine from Linpack by J. Dongarra    *
 **************************************************************/ 

void dswap(long n,double *dx,long incx,double *dy,long incy)

{
   long i;
   double dtemp;

   if (n <= 0 || incx == 0 || incy == 0) return;
   if (incx == 1 && incy == 1) {
      for (i=0; i < n; i++) {
	 dtemp = *dy;
	 *dy++ = *dx;
	 *dx++ = dtemp;
      }	
   }
   else {
      if (incx < 0) dx += (-n+1) * incx;
      if (incy < 0) dy += (-n+1) * incy;
      for (i=0; i < n; i++) {
         dtemp = *dy;
         *dy = *dx;
         *dx = dtemp;
         dx += incx;
         dy += incy;
      }
   }
}
#include <math.h>
#include <stdio.h>
#include "tmsc.h"

#define		CONST    100.0

void   dgemv(long, long, long, double, double **,
             double *, double, double *);
double ddot(long, double *,long, double *, long);
void   dscal(long, double, double *,long);
void   daxpy(long, double, double *,long, double *, long);
void   dcopy(long, double *, long, double *, long);

/***********************************************************************
 *                                                                     *
 *                        orthg()                                      *
 *         Gram-Schmidt orthogonalization procedure                    *
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   The p by n matrix Z stored row-wise in rows f to (f+p-1) of
   array X is reorthogonalized w.r.t. the first f rows of array X.
   The resulting matrix Z is then factored into the product of a
   p by n orthonormal matrix (stored over matrix Z) and a p by p
   upper-triangular matrix (stored in the first p rows and columns 
   of array B).  (Based on orthog from Rutishauser) 


   Parameters
   ----------

   (input)
   p           number of consecutive vectors of array x (stored row-wise)
	       to be orthogonalized
   f           number of rows against which the next p rows are to be
	       orthogonalized
   n           column dimension of x
   x           2-dimensional array whose p rows are to be orthogonalized
	       against its first f rows
   temp        work array


   (output)
   x           output matrix whose f+p rows are orthonormalized
   b           p by p upper-triangular matrix


   Functions called
   --------------

   BLAS         dgemv, ddot, dscal, daxpy, dcopy

 ***********************************************************************/

void orthg(long p, long f, long n, double **b, double **x, double *temp)

{
   long fp, k, km1;
   long orig, small;
   double t, s;

   if (!p) return;
   if (f == 0 && p > n) {
      fprintf(stderr,"%s\n",
         "*** ON ENTRY TO ORTHG, MATRIX TO BE ORTHONORMALIZED IS SINGULAR");
      exit(-1);
   }
   fp = f + p;

   for (k = f; k < fp; k++) {
      km1 = k - 1;
      orig = TRUE;

      while(TRUE) {
         t = ZERO;

	 if (km1 >= 0) {
	    if (km1 > 0) {
	       dgemv(NTRANSP, k, n, ONE, x, x[k], ZERO, temp);
	       t += ddot(k, temp, 1, temp, 1);
	    }

	    else {
	       temp[0] = ddot(n, x[0], 1, x[k], 1);
	       t += temp[0] * temp[0];
	    }

	    if (orig && km1 >= f) 
               dcopy(k - f, &temp[f], 1, &b[k - f][0], 1); 

            if (km1 > 0) 
	       dgemv(TRANSP, k, n, -ONE, x, temp, ONE, &x[k][0]);
            else
	       daxpy(n, -temp[0], x[0], 1, x[k], 1);
         }

	 if (km1 < 0 || p != 1) {
	    s = ddot(n, x[k], 1, x[k], 1);
	    t += s;
	    if (s > t/CONST) {
	       small = FALSE;
	       s = sqrt(s);
               b[k - f][k - f] = s;
	       if (s != ZERO) s = ONE/s;
	       dscal(n, s, x[k], 1);
	    }
	    else {
	       small = TRUE;
	       orig  = FALSE;
	    }
	 }
	 if (small == FALSE || p == 1) break;
      }
   }
}
#include <math.h>
#include "tmsc.h"
double fsign(double, double);
double pythag(double, double);

/***********************************************************************
 *                                                                     *
 *				tql2()   			       *
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   tql2() is a translation of a Fortran version of the Algol
   procedure TQL2, Num. Math. 11, 293-306(1968) by Dowdler, Martin, 
   Reinsch and Wilkinson.
   Handbook for Auto. Comp., vol.II-Linear Algebra, 227-240(1971).  

   This function finds the eigenvalues and eigenvectors of a symmetric
   tridiagonal matrix by the QL method.


   Arguments
   ---------

   (input)                                                             
   offset the index of the leading element  of the input(full) matrix
          to be factored.
   n      order of the symmetric tridiagonal matrix           
   d      contains the diagonal elements of the input matrix        
   e      contains the subdiagonal elements of the input matrix in its
            first n-1 positions.
   z      contains the identity matrix				    
                                                                   
   (output)                                                       
   d      contains the eigenvalues in ascending order.  if an error
            exit is made, the eigenvalues are correct but unordered for
            for indices 0,1,...,ierr.				   
   e      has been destroyed.					  
   z      contains orthonormal eigenvectors of the symmetric   
            tridiagonal (or full) matrix.  if an error exit is made,
            z contains the eigenvectors associated with the stored 
          eigenvalues.					
   ierr   set to zero for normal return, j if the j-th eigenvalue has
            not been determined after 30 iterations.		    


   Functions used
   --------------
   UTILITY	fsign
   MISC		pythag

 ***********************************************************************/

long tql2(long offset, long n, double *d, double *e, double **z)

{
   long j, last, l, l1, l2, m, i, k, iteration;
   double tst1, tst2, g, r, s, s2, c, c2, c3, p, f, h, el1, dl1;
   if (n == 1) return(0);
   f = ZERO;
   last = n - 1;
   tst1 = ZERO;
   e[last] = ZERO;

   for (l = offset; l < n; l++) {
      iteration = 0;
      h = fabs(d[l]) + fabs(e[l]);
      if (tst1 < h) tst1 = h;

      /* look for small sub-diagonal element */
      for (m = l; m < n; m++) {
	 tst2 = tst1 + fabs(e[m]);
	 if (tst2 == tst1) break;
      }
      if (m != l) {
	 while (iteration < 30) {
	    iteration += 1;

            /*  form shift */
	    l1 = l + 1;
	    l2 = l1 + 1;
	    g = d[l];
            p = (d[l1] - g) / (2.0 * e[l]);
	    r = pythag(p, ONE);
	    d[l] = e[l] / (p + fsign(r, p));
	    d[l1] = e[l] * (p + fsign(r, p));
	    dl1 = d[l1];
	    h = g - d[l];
	    if (l2 < n) 
	       for (i = l2; i < n; i++) d[i] -= h;
            f += h;

	    /* QL transformation */
	    p = d[m];
	    c = ONE;
	    c2 = c;
	    el1 = e[l1];
	    s = ZERO;
	    i = m - 1;
	    while (i >= l) {
	       c3 = c2;
	       c2 = c;
	       s2 = s;
	       g = c * e[i];
	       h = c * p;
	       r = pythag(p, e[i]);
	       e[i + 1] = s * r;
	       s = e[i] / r;
	       c = p / r;
	       p = c * d[i] - s * g;
	       d[i + 1]= h + s * (c * g + s * d[i]);

	       /*  form vector */
	       for (k = offset; k < n; k ++) {
	          h = z[i + 1][k];
	          z[i + 1][k] = s * z[i][k] + c * h;
	          z[i][k] = c * z[i][k] - s * h;
	       }
	       i--;
	    }
	    p = -s * s2 * c3 *el1 * e[l] / dl1;
	    e[l] = s * p;
	    d[l] = c * p;
	    tst2 = tst1 + fabs(e[l]);
	    if (tst2 <= tst1) break;
	    if (iteration == 30) 
	       return(l);
         }
      }
      d[l] += f;
   }

   /* order the eigenvalues */
   for (l = 1+offset; l < n; l++) {
      i = l - 1;
      k = i;
      p = d[i];
      for (j = l; j < n; j++) {
	 if (d[j] < p) {
	    k = j;
	    p = d[j];
	 }
      }
      /* ...and corresponding eigenvectors */
      if (k != i) {
	 d[k] = d[i];
	 d[i] = p;
	  for (j = offset; j < n; j ++) {
	     p = z[i][j];
	     z[i][j] = z[k][j];
	     z[k][j] = p;
	  }
      }   
   }
   return(0);
}		/*...... end main ............................*/
#include <math.h>
double dmax(double, double);
double dmin(double, double);

/************************************************************** 
 *							      *
 * Function finds sqrt(a^2 + b^2) without overflow or         *
 * destructive underflow.				      *
 *							      *
 **************************************************************/ 
/************************************************************** 

   Funtions used
   -------------

   UTILITY	dmax, dmin

 **************************************************************/ 

double pythag(double a, double b)

{
   double p, r, s, t, u, temp;

   p = dmax(fabs(a), fabs(b));
   if (p != 0.0) {
      temp = dmin(fabs(a), fabs(b)) / p;
      r = temp * temp; 
      t = 4.0 + r;
      while (t != 4.0) {
	 s = r / t;
	 u = 1.0 + 2.0 * s;
	 p *= u;
	 temp = s / u;
	 r *= temp * temp;
	 t = 4.0 + r;
      }
   }
   return(p);
}
#include <math.h>
/***********************************************************************
 *                                                                     *
 *				random()                               *
 *                        (double precision)                           *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   This is a translation of a Fortran-77 uniform random number
   generator.  The code is based  on  theory and suggestions  given in
   D. E. Knuth (1969),  vol  2.  The argument to the function should 
   be initialized to an arbitrary integer prior to the first call to 
   random.  The calling program should  not  alter  the value of the
   argument between subsequent calls to random.  Random returns values
   within the the interval (0,1).


   Arguments 
   ---------

   (input)
   iy	   an integer seed whose value must not be altered by the caller
	   between subsequent calls

   (output)
   random  a double precision random number between (0,1)

 ***********************************************************************/
double random(long *iy)

{
   static long m2 = 0;
   static long ia, ic, mic;
   static double halfm, s;

   /* If first entry, compute (max int) / 2 */
   if (!m2) {
      m2 = 1 << (8 * (int)sizeof(int) - 2); 
      halfm = m2;

      /* compute multiplier and increment for linear congruential 
       * method */
      ia = 8 * (long)(halfm * atan(1.0) / 8.0) + 5;
      ic = 2 * (long)(halfm * (0.5 - sqrt(3.0)/6.0)) + 1;
      mic = (m2-ic) + m2;

      /* s is the scale factor for converting to floating point */
      s = 0.5 / halfm;
   }

   /* compute next random number */
   *iy = *iy * ia;

   /* for computers which do not allow integer overflow on addition */
   if (*iy > mic) *iy = (*iy - m2) - m2;

   *iy = *iy + ic;

   /* for computers whose word length for addition is greater than
    * for multiplication */
   if (*iy / 2 > m2) *iy = (*iy - m2) - m2;
  
   /* for computers whose integer overflow affects the sign bit */
   if (*iy < 0) *iy = (*iy + m2) + m2;

   return((double)(*iy) * s);
}
#include <stdio.h>
#include "tmsc.h"

extern long ncol,nrow;
extern long *pointr,*rowind;
extern double *value; 

void opat(double *x, double *y)
{
  /*  multiplication of transpose (nrow by ncol sparse matrix A)
    by the vector x, store in y  */

  long i,j,upper;

  for(i=0; i<ncol; i++)
     y[i]=ZERO;

  for(i=0; i<ncol; i++) {
     upper = pointr[i+1];
     for(j=pointr[i]; j<upper; j++)
        y[i] += value[j]*x[rowind[j]];
  }

  return;
}

#include <stdio.h>
#include "tmsc.h"


/***********************************************************************
 *                                                                     *
 *                         dgemm3()                                    *
 *                                                                     *
 * A C-translation of the level 3 BLAS routine DGEMM by Dongarra,      *
 * Duff, du Croz, and Hammarling (see LAPACK Users' Guide).            *
 * In this version, the arrays which store the matrices used in this   *
 * matrix-matrix multiplication are accessed as two-dimensional arrays.*
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   dgemm3() performs one of the matrix-matrix operations

	      C := alpha * op(A) * op(B) + beta * C,

   where op(X) = X or op(X) = X', alpha and beta are scalars, and A, B
   and C are matrices, with op(A) an m by k matrix, op(B) a k by n
   matrix and C an m by n matrix.


   Parameters
   ----------

   (input)
   transa   TRANSP indicates op(A) = A' is to be used in the multiplication
	    NTRANSP indicates op(A) = A is to be used in the multiplication

   transb   TRANSP indicates op(B) = B' is to be used in the multiplication
	    NTRANSP indicates op(B) = B is to be used in the multiplication

   m        on entry, m specifies the number of rows of the matrix op(A)
	    and of the matrix C.  m must be at least zero.  Unchanged
	    upon exit.

   n        on entry, n specifies the number of columns of the matrix op(B)
	    and of the matrix C.  n must be at least zero.  Unchanged
	    upon exit.

   k        on entry, k specifies the number of columns of the matrix op(A)
	    and the number of rows of the matrix B.  k must be at least 
	    zero.  Unchanged upon exit.

   alpha    a scalar multiplier

   a        matrix A as a 2-dimensional array.  When transa = NTRANSP, the
            leading m by k part of a must contain the matrix A. Otherwise,
	    the leading k by m part of a must contain  the matrix A.
   ira,ica  row and column indices of matrix a, where mxn part starts.

   b        matrix B as a 2-dimensional array.  When transb = NTRANSP, the
            leading k by n part of a must contain the matrix B. Otherwise,
	    the leading n by k part of a must contain  the matrix B.
   irb,icb  row and column indices of matrix b, where kxn starts.

   beta     a scalar multiplier.  When beta is supplied as zero then C
	    need not be set on input.

   c        matrix C as a 2-dimensional array.  On entry, the leading
	    m by n part of c must contain the matrix C, except when
	    beta = 0.  In that case, c need not be set on entry. 
	    On exit, c is overwritten by the m by n matrix
	    (alpha * op(A) * op(B) + beta * C).
   irc,icc  row and column indices of matrix c, where the mxn part is stored.

***********************************************************************/
void dgemm3(long transa, long transb, long m, long n, long k, 
            double alpha, double **a, long ira, long ica, double **b,
            long irb, long icb, double beta, double **c, long irc,
            long icc)
{
   long info;
   long i, j, l, nrowa, ncola, nrowb, ncolb;
   double temp;

   info = 0;
   if      ( transa != TRANSP && transa != NTRANSP ) info = 1;
   else if ( transb != TRANSP && transb != NTRANSP ) info = 2;
   else if ( m < 0 ) 				     info = 3;
   else if ( n < 0 )				     info = 4;
   else if ( k < 0 )        			     info = 5;

   if (info) {
      fprintf(stderr, "%s %1ld %s\n",
      "*** ON ENTRY TO DGEMM3, PARAMETER NUMBER",info,"HAD AN ILLEGAL VALUE");
      exit(info);
   }

   if (transa) {
      nrowa = m;
      ncola = k;
   }
   else { 
      nrowa = k;
      ncola = m;
   }
   if (transb) {
      nrowb = k;
      ncolb = n;
   }
   else {
      nrowb = n;
      ncolb = k;
   }
   if (!m || !n || ((alpha == ZERO || !k) && beta == ONE))
      return;

   if (alpha == ZERO) {
      if (beta == ZERO) 
         for (j = 0; j < n; j++)
            for (i = 0; i < m; i++) c[icc+j][irc+i] = ZERO;
      else 
         for (j = 0; j < n; j++)
             for (i = 0; i < m; i++) c[icc+j][irc+i] *= beta;
      return;
   }
   if (!transb) { 

      switch(transa) {

	 /* form C := alpha * A * B + beta * C */
	 case NTRANSP: for(j = 0; j < n; j++) {
                          for(i=0; i<m; i++) c[icc+j][irc+i]=0.0;
                          for(l=0; l<k; l++) 
                             if(b[icb+j][irb+l]!=0.0) {
                               temp = alpha*b[icb+j][irb+l];
                               for(i=0; i<m; i++) 
                                c[icc+j][irc+i] += temp*a[ica+l][ira+i];
      		  	      }
                        }
			break;

	 /* form C := alpha * A' * B + beta * C */
	 case TRANSP:   for(j = 0; j < n; j++) {
                           for(i=0; i<m; i++) {
                              temp = 0.0;
      		  	      for(l = 0; l< k;l++) 
                                 temp += a[ica+i][ira+l]*b[icb+j][irb+l];
                              if(beta==0.0) c[icc+j][irc+i]=alpha*temp; 
                              else 
                       c[icc+j][irc+i] = alpha*temp + beta*c[icc+j][irc+i];
      		  	   }
   	    		}
			break;
      }
   }
   else { 
      switch(transa) {

	 /* form C := alpha * A * B' + beta * C */
	 case NTRANSP: for(j=0; j<n; j++) {
      		  	   for(i=0; i<m; i++) c[icc+j][irc+i]=ZERO;
      		  	   for(l=0; l<k; l++) {
	 	     	      temp = alpha*b[l+icb][j+irb];
         	     	      for(i=0; i<m; i++) 
	    			 c[j+icc][i+irc] += temp*a[l+ica][i+ira];
      		  	   }
   	    		}
			break;

	 /* form C := alpha * A' * B' + beta * C */
	 case TRANSP:   for(j=0; j<n; j++) {
			   for (i=0; i<m; i++) {
      	       		      temp = ZERO;
	 		      for(l=0; l<k; l++) 
				 temp += a[i+ica][l+ira]*b[l+icb][j+irb];
                              if(!beta) c[j+icc][i+irc] += alpha * temp;
                              else 
                                 c[j+icc][i+irc]= alpha*temp+
                                                  beta*c[j+icc][i+irc];
			   }
   	    		}
			break;
      }
   }
}
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

double dasum(long n, double *dx, long incx)
{
  /**************************************************************
   *  Function forms the sum of the absolute values.            *
   *  Uses unrolled loops for increment equal to one.           *
   *  Based on Fortran-77 routine from Linpack by J.Dongarra.
   **************************************************************/

  double dtemp,dsum;
  long   i,ix,m,mp1;

  dsum = ZERO;
  dtemp = ZERO;
  if(n <= 0) return;
  if(incx != 1) {

  /* code for increment not equal to 1 */

    ix = 0;
    if(incx < 0) ix = (-n+1)*incx + 1;
    for(i=0; i<n; i++) {
       dtemp += fabs(dx[ix]);
       ix    += incx;
    }
    dsum = dtemp;
    return(dsum);
  }  

  /* code for increment equal to 1 */
  
  /* clean-up loop */

  m = n % 6;
  if(m) {
    for(i=0; i<m; i++)
       dtemp += fabs(dx[i]);
  }
  if(n>=6) {
    for(i=m; i<n; i+=6)
       dtemp += fabs(dx[i]) + fabs(dx[i+1]) + fabs(dx[i+2]) +
                fabs(dx[i+3]) + fabs(dx[i+4]) + fabs(dx[i+5]);
  }
  dsum = dtemp;
  return(dsum);
}
#include <math.h>

#define ZERO 0.0e+0
#define ONE 1.0e+0
double fsign(double, double);


/***********************************************************************
 *                                                                     *
 *                              tred2()                                *
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

  Description
  -----------
  
  tred2() is a translation of the algol procedure TRED2, Num. Math. 11, 
  181-195 (1968) by Martin, Reinsch, and Wikinson.  Handbook for Auto.
  Comp., Vol. II- Linear Algebra, 212-226 (1971)

  This subroutine reduces a real symmetric matrix to a symmetric
  tridiagonal matrix using and accumulating orthogonal similarity
  transformations.

  Arguments
  ---------

  (input)
  offset index of the leading element of the matrix to be
         tridiagonalized. The matrix tridiagonalized should be 
         stored in a[offset:n-1, offset:n-1]

  n	 order of the matrix

  a	 contains the real symmetric input matrix. Only the upper
	 triangle of the matrix need be supplied

  (output)
  d	 contains the diagonal elements of the tridiagonal matrix.
  
  e	 contains the subdiagonal elements of the tridiagonal matrix
	 in its first n-1 positions.

  z	 contains the orthogonal transformation matrix produced in the
	 reduction.

  a and z may coincide. If distinct, a is unaltered.

  Functions used:
  UTILITY: fsign

***********************************************************************/

void tred2(long offset, long n, double **a, double *d, double *e, double **z)
{
 long jj,ii,i,j,k,l, jp1;
 double *zptr,*aptr,h, scale, f, g,  hh, tmp;

 long i1;

 for (i=offset;i<n;i++) 
  { 
   for (j=i;j<n;j++)
     {
      z[j][i]=a[i][j];   /*fix this later?.. the rest of the routine 
                           assumes that z has the lower triangular part
                           of the symmetric matrix */
     }
   d[i]=a[i][n-1];
  }


  if (n==1) 
   {
    for (i=offset;i<n;i++)
     {
       d[i]=z[n-1][i];
       z[n-1][i]=ZERO;
     }
    z[n-1][n-1]=ONE;
    e[1]=ZERO;
    return;
   }

  /*for i = n step -1 until 2 do*/

  for (ii=3;ii<n+2-offset;ii++)
   {
     i=n+2-ii;
     l=i-1;
     h=ZERO; 
     scale=ZERO;

    /*scale row (algol tol then not needed)*/
     if (l>=1)
       for (k=offset;k<=l;k++)
        {
         scale+= fabs(d[k]);
        }
	
    if ((scale==ZERO)||(l<1))
     {
      e[i]=d[l];
      for (j=offset;j<=l;j++)
          {
            d[j]=z[l][j];
            z[i][j]=ZERO;
            z[j][i]=ZERO;
          }
     }
   else                   /*scale <> ZERO */
     {
       for (k=offset;k<=l;k++)
        {
         d[k]=d[k]/scale;
         h+=d[k]*d[k];
        }


       f=d[l];
       g=-fsign(sqrt(h), f);
       e[i]=scale * g;
       h-=f*g;
       d[l]=f-g;
   
       /* form A*u */
  
       for (j=offset; j<=l; j++)
          e[j]=ZERO;
          
          for (j=offset;j<=l;j++)
            {
             f=d[j];
             z[j][i]=f;
             g= e[j] + z[j][j] * f;
             
             jp1= j + 1;
   
             if (l >= jp1) 
                 {
                  for (k=jp1; k<=l; k++)
                   {
                     g+= z[k][j] * d[k];
                     e[k] += z[k][j] * f;
                   }
                 };
             e[j]=g;
           }

       /* form P */
 
       f= ZERO;
 
       for (j=offset; j<=l; j++)
        {
          e[j]=e[j]/h;
          f+= e[j] * d[j];
        }

       hh= f/ (h+h);
  
       /* form Q */
  
      for (j=offset; j<=l; j++)
       e[j] -= hh * d[j];

      /* form reduced A */

      for (j=offset; j<=l; j++)
       {
         f= d[j];
         g = e[j];

         for (k=j; k<=l; k++)
          z[k][j]= z[k][j] - f * e[k] - g * d[k];

         d[j]=z[l][j];
         z[i][j]=ZERO;
       }
    }  /* end scale <> zero */

    d[i]=h;
   }   /* end for ii */
   /*accumulation of transformation matrices */

   for (i=offset + 1;i<n;i++)
    {
     l=i-1;
     z[n-1][l] = z[l][l];
     z[l][l] = ONE;
     h=d[i];

     if (h != ZERO) 
       {
        for (k=offset; k<=l; k++)
          d[k]= z[k][i]/h;

        for (j=offset; j<=l; j++)
         {
           g= ZERO;
           
           for (k=offset;k<=l; k++)
            g+= z[k][i]*z[k][j];

	   for (k=offset;k<=l;k++)
            z[k][j] -= g * d[k];
         }
       }
       for (k=offset;k<=l;k++) z[k][i]=ZERO;
     }
  
     for (i=offset;i<n;i++)
       {
        d[i]=z[n-1][i];
        z[n-1][i]=ZERO;
       }
     z[n-1][n-1]=ONE;
     e[0]=ZERO;

/*preparation for tql2.c.. reorder e[]*/
for (i=1+offset;i<n;i++) e[i-1]=e[i]; 

/*preparation for tql2.c.. z has to be transposed for 
  tql2 to give correct eigenvectors */
for (ii=offset; ii<n; ii++)
 for (jj=ii; jj<n; jj++)
 {
   tmp=z[ii][jj];
  z[ii][jj]=z[jj][ii];
  z[jj][ii]=tmp;
 }

     return;
}
           
        
#include <stdio.h>
#include <math.h>

extern long ncol,nrow;
extern long *pointr,*rowind;
extern double *value,alpha;

void opb(long n, double *x, double *y, double shift)
{
/* multiplication of a shifted 2-cylic matrix c by a vector x , where

       [D    A]  
   c = [      ]
       [A'   D] , where A is nrow by ncol (nrow>>ncol),
                  and D = (alpha-shift)*I , alpha an upper bound
                  for the largest singular value of A, and 
                  shift is the approximate singular value of A.

   Hence, C is of order N=nrow+ncol (y stores product vector) */

  long i,j,upper;

  for(i=0; i<n; i++)
     y[i]=(alpha-shift)*x[i];

  for(i=0; i<ncol; i++) {
     upper = pointr[i+1];
     for(j=pointr[i]; j<upper; j++)
        y[rowind[j]] += value[j]*x[nrow+i];
  }

  for(i=nrow; i<n; i++) {
     upper = pointr[i-nrow+1];
     for(j=pointr[i-nrow]; j<upper; j++)
        y[i] += value[j]*x[rowind[j]];
  }

  return;
}
/************************************************************** 
 * Constant times a vector plus a vector     		      *
 * Based on Fortran-77 routine from Linpack by J. Dongarra    *
 **************************************************************/ 

void daxpy (long n,double da,double *dx,long incx,double *dy,long incy)

{
   long i;

   if (n <= 0 || incx == 0 || incy == 0 || da == 0.0) return;
   if (incx == 1 && incy == 1) 
      for (i=0; i < n; i++) {
	 *dy += da * (*dx++);
	 dy++;
      }
   else {
      if (incx < 0) dx += (-n+1) * incx;
      if (incy < 0) dy += (-n+1) * incy;
      for (i=0; i < n; i++) {
         *dy += da * (*dx);
         dx += incx;
         dy += incy;
      }
   }
   return;
}
/************************************************************** 
 * Function forms the dot product of two vectors.      	      *
 * Based on Fortran-77 routine from Linpack by J. Dongarra    *
 **************************************************************/ 

double ddot(long n,double *dx,long incx,double *dy,long incy)

{
   long i;
   double dot_product;

   if (n <= 0 || incx == 0 || incy == 0) return(0.0);
   dot_product = 0.0;
   if (incx == 1 && incy == 1) 
      for (i=0; i < n; i++) dot_product += (*dx++) * (*dy++);
   else {
      if (incx < 0) dx += (-n+1) * incx;
      if (incy < 0) dy += (-n+1) * incy;
      for (i=0; i < n; i++) {
         dot_product += (*dx) * (*dy);
         dx += incx;
         dy += incy;
      }
   }
   return(dot_product);
}
#include <stdio.h>

void disk(long n, double *sig, double *rad, long *csize, long *clus)
{
  /* monitor separation of gershgorin disks */

  double radi,radipi,tmp;
  long   cptr,i,k,flag,upper;

  /* assume ordering: sig[0] <= sig[1] <= ...<= sig[n-1] for sig 
     array in tsvd1.
     rad := radii for approximate e-values  

     csize array indicates location and size of clusters:
     csize[i]=k  (k != 0) gives cluster of size k with first disk
     centered at sig[i]. */

  upper = n-1;
  for(i=0; i<upper; i++) {
     radi = rad[i];
     radipi = rad[i+1];
     tmp = (radi+radipi) - (sig[i+1] - sig[i]);
     if(tmp<=0.0) clus[i] = 1;
     else  clus[i] = 0;
  }
  /* clustering vector filled, now locate clusters and their size */

  for(i=0; i<n; i++) csize[i] = 1;

  for(i=0; i<upper; i++) {
     cptr = i;
     flag = 1;
     k = i-1;
     while((flag) && (k>=0)) {
        if(csize[k])  flag = 0;
        else  cptr=k;
        k -= 1;
     }
   if(!clus[i]) {
     if(csize[i]) csize[i] += 1;
     else csize[cptr-1] += 1;
     csize[i+1] -= 1;
   }
  }
return;
}
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

void isol(long i, double resid, double tol, long *init, double *ireso,
          double *creso, long s)
{
  /* monitor residual reduction for isolated singuar value
     approximations.
     Assume ordering : sig[0]<=sig[1]<=...<=sig[s-1] for sig 
     array in tsvd1. 
     Resid : 2-norm of B-eigenpair residual   */

  if((ireso[i]<ZERO) || (creso[i]>=ZERO)) {

      if(creso[i]<ZERO) ireso[i]=resid;
      else {
           ireso[i]=resid;
           creso[i]=-ONE;
      }
    }
  else 
       if(resid<=tol*ireso[i]) init[i] = TRUE;

  return;
}
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

double dasum(long, double *, long);

void clus(long i, long size, double *resid, double tol, long *init,
          double *ireso, double *creso, double *tmp, long s)
{
  /* Monitor residual reduction for clustered singular value 
     approxmations.
     Assume ordering : sig[0] <= sig[1] <=...<= sig[n-1] for 
     sig array in tsvd1.

     resid = 2-norm of B-eigenpair residuals. 
     i     = first disk in cluster.
     size  = number of disks in cluster. */


  double error;
  long   k,upper;

  for(k=0; k<size; k++) tmp[k] = resid[k]*resid[k];
  error = sqrt(dasum(size, tmp, 1));
  if(creso[i] < ZERO) { 
    if(ireso[i] < ZERO) creso[i] = error;
    else {
         creso[i] = error;
         ireso[i] = -ONE;
    }
  }
  else
     if(error <= (tol*creso[i])) {
       upper = i+size;
       for(k=i; k<upper; k++) init[k] = TRUE;
     }
  return;
}
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

extern double *z,*v2,*r,*r2,*pp,*z2;

void daxpy(long, double, double *,long, double *, long);
double ddot(long, double *,long, double *, long);
void dscal(long, double, double *,long);
void dgemv2(long, long, long, double, double **, long, long,
        double *, double, double *);
void opb(long, double *, double *, double);

void cgts(long n, long left, double *w, double **v, long *cgiter,
          double sig, double sigold, double sigmax, double shift,
          long *kount, double eps)
{
  /* cg for independent systems in trace min. optimization step.
   (shift incorporated)
   v stores current orthog basis for r[y]. */

  double denom, bound, a, a0, b, error, rnorm, rnorm0, vnorm, temp;
  long   maxi, i, j, k, ii;

  maxi = n;
  *kount = 0;
  if(sig!=shift) {
    bound = (sig-shift)/(sigmax-shift);
    bound=bound*bound;
  }
  else {
       bound = (sigold-shift)/(sigmax-shift);
       bound=bound*bound;
  }
  /* w0=w by definition , get first redidual residual via orthog.
     projector */

  opb(n, w, z, shift);
  *kount += 1;
  dgemv2(TRANSP, n, left, ONE, v, 0, 0, z, ZERO, r);
  dgemv2(NTRANSP, n, left, ONE, v, 0, 0, r, ZERO, v2);
  for(i=0; i<n; i++) r[i] = z[i]-v2[i];
  rnorm0 = sqrt(ddot(n,r,1,r,1));
  for(i=0; i<n; i++) pp[i] = r[i];

  /* main iteration loop 30*/

  for( ii=0; ii<maxi; ii++) {

     opb(n, pp, v2, shift);
     *kount += 1;
     denom = ddot(n,pp,1,v2,1);
     if(denom<=ZERO){
       return;
     }
     a = ddot(n, r, 1, r, 1) / denom;
     if(ii==0) a0 = a;
     daxpy(n, -a, pp, 1, w, 1);
     for(j=0; j<n; j++) r2[j] = r[j];
     dgemv2(TRANSP, n, left, ONE, v, 0, 0, v2, ZERO, z);
     dgemv2(NTRANSP, n, left, ONE, v, 0, 0, z, ZERO, z2);
     for(i=0; i<n; i++) v2[i] -= z2[i];
     daxpy(n, -a, v2, 1, r2, 1);
     rnorm = sqrt(ddot(n,r2,1,r2,1));
     for(k=0; k<n; k++) v2[k] = pp[k];
     dscal(n, a, v2, 1);
     vnorm = sqrt(ddot(n,v2,1,v2,1));

  /* early termination code: */

     error = fabs(a*rnorm*rnorm/(a0*rnorm0*rnorm0));
     if((error<=bound) || (vnorm <= eps))  {
       *cgiter += ii+1;
       return;
       }
     else if (ii==maxi-1) {
          *cgiter += maxi;
           printf("cgts failed to converge in  %ld  iterations\n",maxi);
           return;
          }

     for(j=0; j<n; j++) v2[j] = r2[j];
     b=ddot(n, r2, 1, r2, 1) / ddot(n, r, 1, r, 1);
     daxpy(n, b, pp, 1, v2, 1);
     for(j=0; j<n; j++) pp[j] = v2[j];
     for(j=0; j<n; j++) r[j] = r2[j];
  }
return;
}
#include <stdio.h>
#include <math.h>
#include "tmsc.h"

extern double *z,*v2,*r,*r2,*pp,*z2;

void daxpy(long, double, double *,long, double *, long);
double ddot(long, double *,long, double *, long);
void dscal(long, double, double *,long);
void dgemv2(long, long, long, double, double **, long, long,
        double *, double, double *);
void opb(long, double *, double *, double);

void cgt(long n, long left, double *w, double **v, long *cgiter, 
         double sig, double sigmax, long *kount, double eps)
{
  /* cg for independent systems in trace min. optimization step.
     v stores current orthog basis for r[y]. */

  double denom, bound, a, a0, b, error, rnorm, rnorm0, vnorm, temp;
  long   maxi, i, j, k, ii,size1;

  maxi = n;
  *kount = 0;
  bound = sig/sigmax ;
  bound = bound*bound;

  /* w0=w by definition , get first residual via orthog, projector */

  opb(n, w, z, ZERO);
  *kount += 1;
  dgemv2(TRANSP, n, left, ONE, v, 0, 0, z, ZERO, r);
  dgemv2(NTRANSP, n, left, ONE, v, 0, 0, r, ZERO, v2);
  for(i=0; i<n; i++) r[i] = z[i]-v2[i];
  rnorm0 = sqrt(ddot(n,r,1,r,1));
  for(i=0; i<n; i++) pp[i] = r[i];

  /* main iteration loop */

  for( ii=0; ii<maxi; ii++) {

     opb(n, pp, v2, ZERO);
     *kount += 1;
     denom = ddot(n, pp, 1, v2, 1);
     if(denom <= ZERO) {
       return;
     }
     a = ddot(n, r, 1, r, 1)/denom;
     if(ii==0) a0 = a;
     daxpy(n, -a, pp, 1, w, 1);
     for(j=0; j<n; j++) r2[j] = r[j];
     dgemv2(TRANSP, n, left, ONE, v, 0, 0, v2, ZERO, z);
     dgemv2(NTRANSP, n, left, ONE, v, 0, 0, z, ZERO, z2);
     for(i=0; i<n; i++) v2[i] -= z2[i];
     daxpy(n, -a, v2, 1, r2, 1);
     rnorm = sqrt(ddot(n, r2, 1,r2,1));
     for(k=0; k<n; k++) v2[k] = pp[k];
     dscal(n, a, v2, 1);
     vnorm = sqrt(ddot(n,v2,1,v2,1));

    /* early termination code: */

     error = fabs(a*rnorm*rnorm/(a0*rnorm0*rnorm0));
     if((error <= bound) || (vnorm <= eps))  {
       *cgiter += ii+1;
       return;
     }
     else if(ii==maxi-1) {
           *cgiter += maxi;
           printf("cgt failed to converge in  %ld   iterations\n",maxi);
           return;
          }
   
     for(j=0; j<n; j++) v2[j] = r2[j];
     b=ddot(n, r2, 1, r2, 1)/ddot(n, r, 1, r, 1);
     daxpy(n, b, pp, 1, v2, 1);
     for(j=0; j<n; j++) pp[j]=v2[j];
     for(j=0; j<n; j++) r[j]=r2[j];
  }
  return;
}
#include <stdio.h>
#include "tmsc.h"

/***********************************************************************
 *                                                                     *
 *                         dgemv()                                     *
 * A C-translation of the level 2 BLAS routine DGEMV by Dongarra,      *
 * du Croz, and Hammarling, and Hanson (see LAPACK Users' Guide).      *
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   dgemv() performs one of the matrix-vector operations

   y := alpha * A * x + beta * y  or  y := alpha * A' * x + beta * y

   where alpha and beta are scalars, X, Y are vectors and A is an
   m by n matrix.

void dgemv(long transa, long m, long n, 
           double alpha, double **a, double *x, double beta, double *y)

   Parameters
   ----------

   (input)
   transa   TRANSP indicates op(A) = A' is to be used in the multiplication
	    NTRANSP indicates op(A) = A is to be used in the multiplication

   m        on entry, m specifies the number of rows of the matrix A.
	    m must be at least zero.  Unchanged upon exit.

   n        on entry, n specifies the number of columns of the matrix A.
	    n must be at least zero.  Unchanged upon exit.

   alpha    a scalar multiplier.  Unchanged upon exit.

   a        matrix A as a 2-dimensional array.  Before entry, the leading
	    m by n part of the array a must contain the matrix A.

   x        linear array of dimension of at least n if transa = NTRANSP
	    and at least m otherwise.

   beta     a scalar multiplier.  When beta is supplied as zero then y
	    need not be set on input.  Unchanged upon exit.

   y        linear array of dimension of at least m if transa = NTRANSP
	    and at leat n otherwise.  Before entry with beta nonzero,
	    the array y must contain the vector y.  On exit, y is 
	    overwritten by the updated vector y.


 ***********************************************************************/

void dgemv(long transa, long m, long n, 
           double alpha, double **a, double *x, double beta, double *y)

{
   long info, leny, i, j;
   double temp, *ptrtemp;

   info = 0;
   if      ( transa != TRANSP && transa != NTRANSP ) info = 1;
   else if ( m < 0 ) 				     info = 2;
   else if ( n < 0 )				     info = 3;

   if (info) {
      fprintf(stderr, "%s %1ld %s\n",
      "*** ON ENTRY TO DGEMV, PARAMETER NUMBER",info,"HAD AN ILLEGAL VALUE");
      exit(info);
   }

   if (transa) leny = n;
   else        leny = m;

   if (!m || !n || (alpha == ZERO && beta == ONE))
      return;

   ptrtemp = y; 

   /* form Y := beta * Y */
   if (beta == ZERO) 
      for (i = 0; i < leny; i++) *ptrtemp++ = ZERO;
   else if (beta != ONE) 
      for (i = 0; i < leny; i++) *ptrtemp++ *= beta;

   if (alpha == ZERO) return;

   switch(transa) {

      /* form Y := alpha * A * X + Y */
      case NTRANSP:  for(i = 0; i < m; i++) {
                        ptrtemp = *a++;
		        temp = ZERO;
		        for(j = 0; j < n; j++) 
			   temp += *ptrtemp++ * x[j];
			y[i] += alpha * temp;
		     }
		     break;
		     
      /* form Y := alpha * A' * X + Y */
      case TRANSP:   for(i = 0; i < m; i++) { 
                        ptrtemp = *a++;
			if (x[i] != ZERO) {
			   temp = alpha * x[i];
			   for(j = 0; j < n; j++)
			      y[j] += temp * (*ptrtemp++);
			}
		     }
		     break;
   }
}
#include <stdio.h>
#include "tmsc.h"

/***********************************************************************
 *                                                                     *
 *                        dgemv2()                                     *
 * A C-translation of the level 2 BLAS routine DGEMV by Dongarra,      *
 * du Croz, and Hammarling, and Hanson (see LAPACK Users' Guide).      *
 *                                                                     *
 ***********************************************************************/
/***********************************************************************

   Description
   -----------

   dgemv2() performs one of the matrix-vector operations

   y := alpha * A * x + beta * y  or  y := alpha * A' * x + beta * y

   where alpha and beta are scalars, X, Y are vectors and A is an
   m by n matrix.

   Parameters
   ----------

   (input)
   transa   TRANSP indicates op(A) = A' is to be used in the multiplication
	    NTRANSP indicates op(A) = A is to be used in the multiplication

   m        on entry, m specifies the number of rows of the matrix A.
	    m must be at least zero.  Unchanged upon exit.

   n        on entry, n specifies the number of columns of the matrix A.
	    n must be at least zero.  Unchanged upon exit.

   alpha    a scalar multiplier.  Unchanged upon exit.

   a        matrix A as a 2-dimensional array.  Before entry, the leading
	    m by n part of the array a must contain the matrix A.

   ira,ica  row and column indices of array A, where mxn part starts.

   x        linear array of dimension of at least n if transa = NTRANSP
	    and at least m otherwise.

   beta     a scalar multiplier.  When beta is supplied as zero then y
	    need not be set on input.  Unchanged upon exit.

   y        linear array of dimension of at least m if transa = NTRANSP
	    and at leat n otherwise.  Before entry with beta nonzero,
	    the array y must contain the vector y.  On exit, y is 
	    overwritten by the updated vector y.


***********************************************************************/

void dgemv2(long transa, long m, long n, 
           double alpha, double **a, long ira, long ica,
           double *x, double beta, double *y)

{
   long info, leny, i, j;
   double temp, *ptrtemp;

   info = 0;
   if      ( transa != TRANSP && transa != NTRANSP ) info = 1;
   else if ( m < 0 ) 				     info = 2;
   else if ( n < 0 )				     info = 3;

   if (info) {
      fprintf(stderr, "%s %1ld %s\n",
      "*** ON ENTRY TO DGEMV, PARAMETER NUMBER",info,"HAD AN ILLEGAL VALUE");
      exit(info);
   }

   if (transa) leny = n;
   else        leny = m;

   if (!m || !n || (alpha == ZERO && beta == ONE))
      return;


   /* form Y := beta * Y */
   if (beta == 0.0) 
      for (i = 0; i < leny; i++) y[i] = ZERO;
   else if (beta != 1.0) 
      for (i = 0; i < leny; i++) y[i] *= beta;

   if (alpha == ZERO) return;

   switch(transa) {

      /* form Y := alpha * A * X + Y */
      case NTRANSP:  for(j = 0; j < n; j++) {
		        temp = alpha*x[j];
		        for(i = 0; i < m; i++) 
			y[i] += temp*a[j+ica][i+ira];
		     }
		     break;
		     
      /* form Y := alpha * A' * X + Y */
      case TRANSP:   for(j = 0; j < n; j++) { 
                        temp = ZERO;
			for(i=0; i<m; i++) 
			   temp += a[j+ica][i+ira]*x[i];
                        y[j] += alpha*temp;
		     }
		     break;
   }
}
#include <stdio.h>
#include <math.h>

double dmax(double, double);
extern long ncol;
extern long *pointr,*rowind;
extern double *value;

double norm_1()
{
/* find matrix 1-norm  */

    double alpha,sum;
    long i,j,last;

    alpha = 0.0;
    for (j=0; j<ncol; ++j) {
        sum = 0.0;
        last= pointr[j+1];
        for (i=pointr[j]; i<last ; ++i)
            sum += fabs(value[i]);
        alpha = dmax(alpha,sum);
    }
    return(alpha);
}
double fsign(double a,double b)
/************************************************************** 
 * returns |a| if b is positive; else fsign returns -|a|      *
 **************************************************************/ 
{

   if ((a>=0.0 && b>=0.0) || (a<0.0 && b<0.0))return(a);
   if  ((a<0.0 && b>=0.0) || (a>=0.0 && b<0.0))return(-a);
}

double dmax(double a, double b)
/************************************************************** 
 * returns the larger of two double precision numbers         *
 **************************************************************/ 
{

   if (a > b) return(a);
   else return(b);
}

double dmin(double a, double b)
/************************************************************** 
 * returns the smaller of two double precision numbers        *
 **************************************************************/ 
{

   if (a < b) return(a);
   else return(b);
}

long imin(long a, long b)
/************************************************************** 
 * returns the smaller of two integers                        *
 **************************************************************/ 
{

   if (a < b) return(a);
   else return(b);
}

long imax(long a,long b)
/************************************************************** 
 * returns the larger of two integers                         *
 **************************************************************/ 
{

   if (a > b) return(a);
   else return(b);
}