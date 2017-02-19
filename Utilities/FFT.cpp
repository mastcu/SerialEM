// FFT.cpp               Hand-translation of our Fortran FFT routines, can be
//                          used if fftw is not available

#include "FFT.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#pragma warning ( disable : 4244 4305 )

static void srfp (int pts, int pmax, int twogrp, int *factor, int *sym, int *psym,
     int *unsym, int *error);
static void odfft(float *array, int nx, int ny, int idir);
static void realft (float *even, float *odd, int n, int *dim);
static void cmplft (float *x, float *y, int n, int *d);
static void hermft(float *x, float *y, int n, int *dim);
static void diprp (int pts, int *sym, int psym, int *unsym, int *dim,
      float *x, float *y);
static void mdftkd (int n, int *factor, int *dim, float *x, float *y);
static void r2cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       int *dim);
static void r3cftk (int n, int m, float *x0, float *y0, float *x1, float *y1, 
       float *x2, float *y2, int *dim);
static void r4cftk (int n, int m, float *x0, float *y0, float *x1, float *y1, 
       float *x2, float *y2, float *x3, float *y3, int *dim);
static void r5cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       float *x2, float *y2, float *x3, float *y3, float *x4, float *y4,
       int *dim);
static void r8cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       float *x2, float *y2, float *x3, float *y3, float *x4,
       float *y4, float *x5, float *y5, float *x6, float *y6,
       float *x7, float *y7, int *dim);
static void rpcftk (int n, int m, int p, int r, float *x, float *y, int *dim);

/*
C*TODFFT.FOR********************************************************
C
C TWO-DIMENSIONAL FOURIER TRANSFORM SUBROUTINE FOR IMAGE
C PROCESSING. DOES BOTH FOWARD & INVERSE TRANSFORMS
C USES LYNN TENEYCK'S MIXED-RADIX ROUTINES
C THUS THE ONLY RESTRICTION IS THAT THE IMAGE SIZE BE
C AN EVEN NUMBER AND HAVE NO PRIME FACTORS LARGER THAN 19!!
C
C IDIR =  0 FOWARD  TRANSFORM  :  exp(+2PIirs)
C IDIR =  1 INVERSE TRANSFORM  :  exp(-2PIirs)
C IDIR = -1 INVERSE TRANSFORM BUT NO COMPLEX CONJUGATE
C
C DATA SET UP AS NY ROWS OF NX NUMBERS
C NOTE NX,NY ALWAYS REFER TO THE REAL-SPACE IMAGE SIZE
C
C NX,NY IMAGE IS TRANSFORMED IN-PLACE INTO NY STRIPS OF
C NX/2 + 1 COMPLEX FOURIER COEFFICIENTS
C THE ORIGIN IS LOCATED AT THE FIRST POINT!!!
C
C ARRAY MUST BE DIMENSIONED TO ALLOW FOR EXTRA STRIP OF COMPLEX
C NUMBERS ON OUTPUT.
C THUS FOR A 300X400 TRANSFORM, ARRAY MUST BE DIMENSIONED:
C REAL ARRAY(302,400)
C
C A NORMALIZATION FACTOR IS APPLIED DURING BOTH  TRANSFORMATIONS
C
C VERSION 1.00  OCT 11 1981   DAA
C VERSION 1.02  APR 19 1982   DAA
C VERSION 1.03  NOV 23 1982   DAA
*/

void FFTtodfft(float *array, int nx, int ny, int idir)
{
     int idim[6]; /* Make it 6 so that indexes 1 to 5 work */
     int nxo2, nxt, nxt1, j, nxp2, nxm1, index, iy;
     float onevol;

     nxo2 = nx/2;
     if (2*nxo2 != nx) {
    printf(" todfft: nx= %d must be even!!!\n", nx);
    return;  //exit(1);
     }
     nxp2 = nx + 2;
     nxt = nxp2*ny;
     nxt1 = nxt - 1;
     onevol = sqrt((double)(1.0/(nx*ny)));
     if (idir == 0) {
    /*
      c********      foward transforms come here       ******************
      c
      c
      c  set up for first dimension of transform
    */
    idim[1] = nxp2*ny;
    idim[2] = 2;
    idim[3] = idim[1];
    idim[4] = idim[1];
    idim[5] = nxp2;

    realft(array, &(array[1]), nxo2, idim);
    /*
      c  set up for second dimension of transform
    */
    idim[2] = nxp2;
    idim[4] = idim[2];
    idim[5] = 2;

    cmplft(array, &(array[1]), ny, idim);
    /*
      c   take complex conjugate (to make proper foward transform)& scale by 1/volume
    */
    for (j = 0; j < nxt1; j += 2) {
         array[j] = array[j]*onevol;
         array[j+1] = -array[j+1]*onevol;
    }

    return;
     }
     /*
       c**********        inverse transform     *******************
       c
       c
       c  set up for first dimension of transform
     */
     nxm1 = nx - 1;
     idim[1] = nxp2*ny;
     idim[2] = nxp2;
     idim[3] = idim[1];
     idim[4] = idim[2];
     idim[5] = 2;
     /*
       c   take complex conjugate to do inverse & scale by 1/volume
     */
     if (idir != 1) {
    for (j = 0; j < nxt1; j += 2) {
         array[j] = array[j]*onevol;
         array[j+1] = -array[j+1]*onevol;
    }
     } else {
    /*
      c   idir = 1 just scale by 1/volume (for standard inverse transform)
    */
    for (j = 0; j < nxt1; j += 2) {
         array[j] = array[j]*onevol;
         array[j+1] = array[j+1]*onevol;
    }
     }

     cmplft(array, &(array[1]), ny, idim);
     /*
       c  set up for second dimension of transform
     */
     idim[2] = 2;
     idim[4] = idim[1];
     idim[5] = nxp2;
     /*
       c    change data storage mode complex conjugate done by hermft
     */
     index = 1;
     for (iy = 0; iy < ny; iy++) {
          array[index] = array[nxm1 + index];
          index = index + nxp2;
     }

     hermft(array, &(array[1]), nxo2, idim);

     return;
}


/*
C     REAL FOURIER TRANSFORM
C
C     GIVEN A REAL SEQUENCE OF LENGTH 2N THIS SUBROUTINE CALCULATES THE
C     UNIQUE PART OF THE FOURIER TRANSFORM.  THE FOURIER TRANSFORM HAS
C     N + 1 UNIQUE REAL PARTS AND N - 1 UNIQUE IMAGINARY PARTS.  SINCE
C     THE REAL PART AT X(N) IS FREQUENTLY OF INTEREST, THIS SUBROUTINE
C     STORES IT AT X(N) RATHER THAN IN Y(0).  THEREFORE X AND Y MUST BE
C     OF LENGTH N + 1 INSTEAD OF N.  NOTE THAT THIS STORAGE ARRANGEMENT
C     IS DIFFERENT FROM THAT EMPLOYED BY THE HERMITIAN FOURIER TRANSFORM
C     SUBROUTINE.
C
C     FOR CONVENIENCE THE DATA IS PRESENTED IN TWO PARTS, THE FIRST
C     CONTAINING THE EVEN NUMBERED REAL TERMS AND THE SECOND CONTAINING
C     THE ODD NUMBERED TERMS (NUMBERING STARTING AT 0).  ON RETURN THE
C     REAL PART OF THE TRANSFORM REPLACES THE EVEN TERMS AND THE
C     IMAGINARY PART OF THE TRANSFORM REPLACES THE ODD TERMS.
*/

static void realft (float *even, float *odd, int n, int *dim)
{
     float a, b, c, d, e, f, co, si, twopi, two_n;
     double angle;
     int nt, d2, d3, d4, d5;
     int i, j, k, l, nover2, i0, i1, i2;

     twopi = 6.2831853;
     two_n = 2*n;

     cmplft (even, odd, n, dim);

     nt = dim[1];
     d2 = dim[2];
     d3 = dim[3];
     d4 = dim[4] - 1;
     d5 = dim[5];
     nover2 = n/2 + 1;

     if (nover2 >= 2) {
    /*     do 300 i = 2, nover2 */
    for (i = 2; i <= nover2; i++) {
         angle = twopi*(i-1)/two_n;
         co = cos(angle);
         si = sin(angle);
         i0 = (i - 1)*d2 + 1;
         j = (n + 2 - 2*i)*d2;
         /*      do 200 i1 = i0, nt, d3 */
         for (i1 = i0; i1 <= nt; i1 += d3) {
        i2 = i1 + d4;
        /*      do 100 k = i1, i2, d5 */
        for (k = i1 - 1; k < i2; k += d5) {
       l = k + j;
       a = (even[l] + even[k])/2.0;
       c = (even[l] - even[k])/2.0;
       b = (odd[l] + odd[k])/2.0;
       d = (odd[l] - odd[k])/2.0;
       e = c*si + b*co;
       f = c*co - b*si;
       even[k] = a + e;
       even[l] = a - e;
       odd[k] = f - d;
       odd[l] = f + d;
        }
         }
    }
     }
     if (n < 1)
    return;
     j = n*d2;
     /*      do 500 i1 = 1, nt, d3 */
     for (i1 = 1; i1 <= nt; i1 += d3) {
    i2 = i1 + d4;
    /*      do 500 k = i1, i2, d5 */
    for (k = i1 - 1; k < i2; k += d5) {
         l = k + j;
         even[l] = even[k] - odd[k];
         odd[l] = 0.0;
         even[k] = even[k] + odd[k];
         odd[k] = 0.0;
    }
     }
     return;
}


/*
C     COMPLEX FINITE DISCRETE FOURIER TRANSFORM
C     TRANSFORMS ONE DIMENSION OF MULTI-DIMENSIONAL DATA
C     MODIFIED BY L. F. TEN EYCK FROM A ONE-DIMENSIONAL VERSION WRITTEN
C     BY G. T. SANDE, 1969.
C
C     THIS PROGRAM CALCULATES THE TRANSFORM
C               (X(T) + I*Y(T))*(COS(2*PI*T/N) - I*SIN(2*PI*T/N))
C
C     INDEXING -- THE ARRANGEMENT OF THE MULTI-DIMENSIONAL DATA IS
C     SPECIFIED BY THE INTEGER ARRAY D, THE VALUES OF WHICH ARE USED AS
C     CONTROL PARAMETERS IN DO LOOPS.  WHEN IT IS DESIRED TO COVER ALL
C     ELEMENTS OF THE DATA FOR WHICH THE SUBSCRIPT BEING TRANSFORMED HAS
C     THE VALUE I0, THE FOLLOWING IS USED.
C
C               I1 = (I0 - 1)*D(2) + 1
C               DO 100 I2 = I1, D(1), D(3)
C               I3 = I2 + D(4) - 1
C               DO 100 I = I2, I3, D(5)
C                  .
C                  .
C           100 CONTINUE
C
C     WITH THIS INDEXING IT IS POSSIBLE TO USE A NUMBER OF ARRANGEMENTS
C     OF THE DATA, INCLUDING NORMAL FORTRAN COMPLEX NUMBERS (D(5) = 2)
C     OR SEPARATE STORAGE OF REAL AND IMAGINARY PARTS.
*/

static void cmplft (float *x, float *y, int n, int *d)
{
     int error;
     int pmax, psym, twogrp;
     int factor[16], sym[16], unsym[16];
     /*
       c     pmax is the largest prime factor that will be tolerated by this
       c     program.
       c     twogrp is the largest power of two that is treated as a special
       c     case.
     */
     pmax = 19;
     twogrp = 8;

     if (n <= 1)
    return;
     srfp (n, pmax, twogrp, factor, sym, &psym, unsym, &error);

     if (error) {
    printf ("invalid number of points for cmplft.  n = %d\n", n);
    return;  //exit(1);
     }
     mdftkd (n, factor, d, x, y);
     diprp (n, sym, psym, unsym, d, x, y);

     return;
}


/*
C     HERMITIAN SYMMETRIC FOURIER TRANSFORM
C
C     GIVEN THE UNIQUE TERMS OF A HERMITIAN SYMMETRIC SEQUENCE OF LENGTH
C     2N THIS SUBROUTINE CALCULATES THE 2N REAL NUMBERS WHICH ARE ITS
C     FOURIER TRANSFORM.  THE EVEN NUMBERED ELEMENTS OF THE TRANSFORM
C     (0, 2, 4, . . ., 2N-2) ARE RETURNED IN X AND THE ODD NUMBERED
C     ELEMENTS (1, 3, 5, . . ., 2N-1) IN Y.
C
C     A FINITE HERMITIAN SEQUENCE OF LENGTH 2N CONTAINS N + 1 UNIQUE
C     REAL NUMBERS AND N - 1 UNIQUE IMAGINARY NUMBERS.  FOR CONVENIENCE
C     THE REAL VALUE FOR X(N) IS STORED AT Y(0).
*/

static void hermft(float *x, float *y, int n, int *dim)
{
     float a, b, c, d, e, f, co, si, two_n, twopi;
     double angle ;
     int nt, d2, d3, d4, d5;
     int i, j, k, nover2, i0, i1, i2;
     int k1;

     twopi = 6.2831853;
     two_n = 2*n;

     nt = dim[1];
     d2 = dim[2];
     d3 = dim[3];
     d4 = dim[4] - 1;
     d5 = dim[5];

     /*      do 100 i0 = 1, nt, d3 */
     for (i0 = 1; i0 <= nt; i0 += d3) {
    i1 = i0 + d4;
    /*      do 100 i = i0, i1, d5 */
    for (i = i0 - 1; i < i1; i+= d5) {
         a = x[i];
         b = y[i];
         x[i] = a + b;
         y[i] = a - b;
    }
     }

     nover2 = n/2 + 1;
     if (nover2 < 2) 
    return;
     /*      do 400 i0 = 2, nover2 */
     for (i0 = 2; i0 <= nover2; i0++) {
    angle = twopi*(i0-1)/two_n;
    co = cos(angle);
    si = sin(angle);
    k = (n + 2 - 2*i0)*d2;
    k1 = (i0 - 1)*d2 + 1;
    /*      do 300 i1 = k1, nt, d3 */
    for (i1 = k1; i1 <= nt; i1 += d3) {
         i2 = i1 + d4;
         /*      do 200 i = i1, i2, d5 */
         for (i = i1 - 1; i < i2; i += d5) {
        j = i + k;
        a = x[i] + x[j];
        b = x[i] - x[j];
        c = y[i] + y[j];
        d = y[i] - y[j];
        e = b*co + c*si;
        f = b*si - c*co;
        x[i] = a + f;
        x[j] = a - f;
        y[i] = e + d;
        y[j] = e - d;
         }
    }
     }
     cmplft (x, y, n, dim);
     return;
}


     /*     symmetrized reordering factoring programme */
     /* USE FORTRAN INDEXING TO AVOID REWRITING INDEXING LOGIC */

static void srfp (int pts, int pmax, int twogrp, int *factor, int *sym, int *psym,
     int *unsym, int *error)
{
     int pp[15], qq [8];
     int f,j,jj,n,nest,p,ptwo,q,r;

     nest=14;

     n=pts;
     *psym=1;
     f=2;
     p=0;
     q=0;
     /*  100 continue */
     while (n > 1) {
    /*      if (n <= 1) go to 500 */
    for (j = f; j <= pmax; j++) {
         if (n == (n/j)*j)
        break;
    }
    if (j > pmax) {
         printf("largest factor exceeds %d.  n = %d.\n", pmax, pts);
         *error=1;
         return;
    }
    if (2*p+q >= nest) {
         printf("factor count exceeds %d.  n = %d.\n", nest, pts);
         *error=1;
         return;
    }

    f=j;
    n=n/f;
    if (n != (n/f)*f) {
         q=q+1;
         qq[q]=f;
    } else {
         n=n/f;
         p=p+1;
         pp[p]=f;
         *psym=*psym*f;
    }
     }
     /*      go to 100
       500  continue */

     r=1;
     if (q == 0) r=0;
     if (p >= 1) {
    /*    do 600 j=1,p */
    for (j = 1; j <= p; j++) {
         jj=p+1-j;
         sym[j]=pp[jj];
         factor[j]=pp[jj];
         jj=p+q+j;
         factor[jj]=pp[j];
         jj=p+r+j;
         sym[jj]=pp[j];
    }
     }
     if (q >= 1) {
    /*     do 800 j=1,q */
    for (j = 1; j <= q; j++) {
         jj=p+j;
         unsym[j]=qq[j];
         factor[jj]=qq[j];
    }
    sym[p+1]=pts / (*psym * *psym);
     }
     jj=2*p+q;
     factor[jj+1]=0;
     ptwo=1;
     j=0;
     while (factor[j+1] != 0) {
    j=j+1;
    /*      if (factor[j].eq.0) go to 1200 */
    if (factor[j] != 2)
         continue;
    ptwo=ptwo*2;
    factor[j]=1;
    if (ptwo < twogrp && factor[j+1] == 2)
         continue;
    factor[j]=ptwo;
    ptwo=1;
     }

     if (p == 0) r=0;
     jj=2*p+r;
     sym[jj+1]=0;
     if (q <= 1) q=0;
     unsym[q+1]=0;
     *error=0;
     return;
}


/*     multi-dimensional complex fourier transform kernel driver */

static void mdftkd (int n, int *factor, int *dim, float *x, float *y)
{
     int f, m, p, r, s;

     s = dim[2];
     f = 0;
     m = n;
     while (factor[f + 1] != 0) {
    f = f + 1;
    p = factor[f];
    /*       if (p.eq.0) return */
    m = m/p;
    r = m*s;
    switch (p) {
         /*      if (p.gt.8) go to 700
           go to (100, 200, 300, 400, 500, 800, 700, 600), p
           go to 800 */
       case 1:
         break;

       case 2:
         r2cftk (n, m, x, y, &x[r], &y[r], dim);
         break;

       case 3:
         r3cftk (n, m, x, y, &x[r], &y[r], &x[2*r], &y[2*r], dim);
         break;

       case 4:
         r4cftk (n, m, x, y, &x[r], &y[r], &x[2*r], &y[2*r],
           &x[3*r], &y[3*r], dim);
         break;

       case 5:
         r5cftk (n, m, x, y, &x[r], &y[r], &x[2*r], &y[2*r], 
           &x[3*r], &y[3*r], &x[4*r], &y[4*r], dim);
         break;

       case 8:
         r8cftk (n, m, x, y, &x[r], &y[r], &x[2*r], &y[2*r],
           &x[3*r], &y[3*r], &x[4*r], &y[4*r], &x[5*r], &y[5*r],
           &x[6*r], &y[6*r], &x[7*r], &y[7*r], dim);
         break;

       case 6:
         printf("\ntransfer error detected in mdftkd\n\n");
         return;

       case 7:
       default:
         rpcftk (n, m, p, r, x, y, dim);
         break;
    }
     }
     return;
}

static void r2cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       int *dim)
/*     radix 2 multi-dimensional complex fourier transform kernel */
{
     int fold,zero;
     int j,k,k0,m2,mover2;
     int k1, k2, kk, l, l1, mm2, nt, size, sep;
     int ns;
     double angle;
     float c,is,iu,rs,ru,s,twopi=6.2831853;
     float fjm1, fm2;
     int itrip, ntrip;

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     m2=m*2;
     fm2 = m2;
     mover2=m/2+1;
     mm2 = sep*m2;

     fjm1 = -1.0;
     /*      do 600 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold=j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fm2;
    zero=angle == 0.0;
    if (!zero) {
         c=cos(angle);
         s=sin(angle);
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {
         
        /*      do 500 kk = k0, ns, mm2
          do 440 l = kk, nt, l1 */
        for (kk = k0; kk <= ns; kk += mm2) {
       for (l = kk; l <= nt; l += l1) {
            k1 = l + size;
            /*      do 420 k = l, k1, k2 */
            for (k = l - 1; k < k1; k += k2) {
           rs=x0[k]+x1[k];
           is=y0[k]+y1[k];
           ru=x0[k]-x1[k];
           iu=y0[k]-y1[k];
           x0[k]=rs;
           y0[k]=is;
           if (!zero) {
          x1[k]=ru*c+iu*s;
          y1[k]=iu*c-ru*s;
           } else {
          x1[k]=ru;
          y1[k]=iu;
           }
            }
       }
        }
         k0 = (m+1-j)*sep + 1;
         c=-c;
    }
     }

     return;
}

/*     radix 3 multi-dimensional complex fourier transform kernel */
static void r3cftk (int n, int m, float *x0, float *y0, float *x1, float *y1, 
       float *x2, float *y2, int *dim)
{
     int fold,zero;
     int j,k,k0,m3,mover2;
     int k1, k2, kk, l, l1, mm3, nt, size, sep;
     int ns;
     double angle;
     float a = -0.5, b = 0.86602540, c1,c2,s1,s2,t,twopi=6.2831853;
     float i0,i1,i2,ia,ib,is,r0,r1,r2,ra,rb,rs;
     float fjm1, fm3;
     int itrip, ntrip;
     /*      data twopi/6.2831853/, a/-0.5/, b/0.86602540/ */

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     m3=m*3;
     fm3 = m3;
     mm3 = sep*m3;
     mover2=m/2+1;

     fjm1 = -1.0;
     /*      do 600 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold = j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fm3;
    zero=angle == 0.0;
    if (!zero) {
         c1=cos(angle);
         s1=sin(angle);
         c2=c1*c1-s1*s1;
         s2=s1*c1+c1*s1;
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {
         /*      do 500 kk = k0, ns, mm3
           do 440 l = kk, nt, l1 */
         for (kk = k0; kk <= ns; kk += mm3) {
        for (l = kk; l <= nt; l += l1) {
       k1 = l + size;
       /*      do 420 k = l, k1, k2 */
       for (k = l - 1; k < k1; k += k2) {
            r0=x0[k];
            i0=y0[k];
            rs=x1[k]+x2[k];
            is=y1[k]+y2[k];
            x0[k]=r0+rs;
            y0[k]=i0+is;
            ra=r0+rs*a;
            ia=i0+is*a;
            rb=(x1[k]-x2[k])*b;
            ib=(y1[k]-y2[k])*b;
            if (!zero) {
           r1=ra+ib;
           i1=ia-rb;
           r2=ra-ib;
           i2=ia+rb;
           x1[k]=r1*c1+i1*s1;
           y1[k]=i1*c1-r1*s1;
           x2[k]=r2*c2+i2*s2;
           y2[k]=i2*c2-r2*s2;
            } else {
           x1[k]=ra+ib;
           y1[k]=ia-rb;
           x2[k]=ra-ib;
           y2[k]=ia+rb;
            }
       }
        }
         }
         k0 = (m+1-j)*sep + 1;
         t=c1*a+s1*b;
         s1=c1*b-s1*a;
         c1=t;
         t=c2*a-s2*b;
         s2=-c2*b-s2*a;
         c2=t;
    }
     }
     return;
}

/*     radix 4 multi-dimensional complex fourier transform kernel */
static void r4cftk (int n, int m, float *x0, float *y0, float *x1, float *y1, 
       float *x2, float *y2, float *x3, float *y3, int *dim)
{
     int fold,zero;
     int j,k,k0,m4,mover2;
     int k1, k2, kk, l, l1, mm4, nt, size, sep;
     int ns;
     double angle;
     float c1,c2,c3,s1,s2,s3,t,twopi=6.2831853;
     float i1,i2,i3,is0,is1,iu0,iu1,r1,r2,r3,rs0,rs1,ru0,ru1;
     float fjm1, fm4;
     int itrip, ntrip;

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     m4=m*4;
     fm4 = m4;
     mm4 = sep*m4;
     mover2=m/2+1;
     
     fjm1 = -1.0;
     /*      do 600 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold=j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fm4;
    zero=angle == 0.0;
    if (!zero) {
         c1=cos(angle);
         s1=sin(angle);
         c2=c1*c1-s1*s1;
         s2=s1*c1+c1*s1;
         c3=c2*c1-s2*s1;
         s3=s2*c1+c2*s1;
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {


         /*      do 500 kk = k0, ns, mm4
           do 440 l = kk, nt, l1 */
         for (kk = k0; kk <= ns; kk += mm4) {
        for (l = kk; l <= nt; l += l1) {

       k1 = l + size;
       /*      do 420 k = l, k1, k2 */
       for (k = l - 1; k < k1; k += k2) {
            rs0=x0[k]+x2[k];
            is0=y0[k]+y2[k];
            ru0=x0[k]-x2[k];
            iu0=y0[k]-y2[k];
            rs1=x1[k]+x3[k];
            is1=y1[k]+y3[k];
            ru1=x1[k]-x3[k];
            iu1=y1[k]-y3[k];
            x0[k]=rs0+rs1;
            y0[k]=is0+is1;
            if (!zero) {
           r1=ru0+iu1;
           i1=iu0-ru1;
           r2=rs0-rs1;
           i2=is0-is1;
           r3=ru0-iu1;
           i3=iu0+ru1;
           x2[k]=r1*c1+i1*s1;
           y2[k]=i1*c1-r1*s1;
           x1[k]=r2*c2+i2*s2;
           y1[k]=i2*c2-r2*s2;
           x3[k]=r3*c3+i3*s3;
           y3[k]=i3*c3-r3*s3;
            } else {
           x2[k]=ru0+iu1;
           y2[k]=iu0-ru1;
           x1[k]=rs0-rs1;
           y1[k]=is0-is1;
           x3[k]=ru0-iu1;
           y3[k]=iu0+ru1;

            }
       }
        }
         }

         k0 = (m+1-j)*sep + 1;
         t=c1;
         c1=s1;
         s1=t;
         c2=-c2;
         t=c3;
         c3=-s3;
         s3=-t;
    }
     }
     return;
}


/*     radix 5 multi-dimensional complex fourier transform kernel */
static void r5cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       float *x2, float *y2, float *x3, float *y3, float *x4, float *y4,
       int *dim)
{
     int fold,zero;
     int j,k,k0,m5,mover2;
     int k1, k2, kk, l, l1, mm5, nt, size, sep;
     int ns;
     double angle;
     float a1 = 0.30901699, a2 = -0.80901699, b1 = 0.95105652;
     float b2 = 0.58778525,c1,c2,c3,c4,s1,s2,s3,s4,t,twopi=6.2831853;
     float r0,r1,r2,r3,r4,ra1,ra2,rb1,rb2,rs1,rs2,ru1,ru2;
     float i0,i1,i2,i3,i4,ia1,ia2,ib1,ib2,is1,is2,iu1,iu2;
     float fjm1, fm5;
     int itrip, ntrip;
     /*     data twopi/6.2831853/, a1/0.30901699/, b1/0.95105652/,
      .      a2/-0.80901699/, b2/0.58778525/ */

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     m5=m*5;
     fm5 = m5;
     mm5 = sep*m5;
     mover2=m/2+1;

     fjm1 = -1.0;
     /*      do 600 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold=j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fm5;
    zero=angle == 0.0;
    if (!zero) {
         c1=cos(angle);
         s1=sin(angle);
         c2=c1*c1-s1*s1;
         s2=s1*c1+c1*s1;
         c3=c2*c1-s2*s1;
         s3=s2*c1+c2*s1;
         c4=c2*c2-s2*s2;
         s4=s2*c2+c2*s2;
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {

         /*      do 500 kk = k0, ns, mm5
           do 440 l = kk, nt, l1 */
         for (kk = k0; kk <= ns; kk += mm5) {
        for (l = kk; l <= nt; l += l1) {
       k1 = l + size;
       /*     do 420 k = l, k1, k2 */
       for (k = l - 1; k < k1; k += k2) {
            r0=x0[k];
            i0=y0[k];
            rs1=x1[k]+x4[k];
            is1=y1[k]+y4[k];
            ru1=x1[k]-x4[k];
            iu1=y1[k]-y4[k];
            rs2=x2[k]+x3[k];
            is2=y2[k]+y3[k];
            ru2=x2[k]-x3[k];
            iu2=y2[k]-y3[k];
            x0[k]=r0+rs1+rs2;
            y0[k]=i0+is1+is2;
            ra1=r0+rs1*a1+rs2*a2;
            ia1=i0+is1*a1+is2*a2;
            ra2=r0+rs1*a2+rs2*a1;
            ia2=i0+is1*a2+is2*a1;
            rb1=ru1*b1+ru2*b2;
            ib1=iu1*b1+iu2*b2;
            rb2=ru1*b2-ru2*b1;
            ib2=iu1*b2-iu2*b1;
            if (!zero) {
           r1=ra1+ib1;
           i1=ia1-rb1;
           r2=ra2+ib2;
           i2=ia2-rb2;
           r3=ra2-ib2;
           i3=ia2+rb2;
           r4=ra1-ib1;
           i4=ia1+rb1;
           x1[k]=r1*c1+i1*s1;
           y1[k]=i1*c1-r1*s1;
           x2[k]=r2*c2+i2*s2;
           y2[k]=i2*c2-r2*s2;
           x3[k]=r3*c3+i3*s3;
           y3[k]=i3*c3-r3*s3;
           x4[k]=r4*c4+i4*s4;
           y4[k]=i4*c4-r4*s4;
            } else {
           x1[k]=ra1+ib1;
           y1[k]=ia1-rb1;
           x2[k]=ra2+ib2;
           y2[k]=ia2-rb2;
           x3[k]=ra2-ib2;
           y3[k]=ia2+rb2;
           x4[k]=ra1-ib1;
           y4[k]=ia1+rb1;
            }
       }
        }
         }
         k0 = (m+1-j)*sep + 1;
         t=c1*a1+s1*b1;
         s1=c1*b1-s1*a1;
         c1=t;
         t=c2*a2+s2*b2;
         s2=c2*b2-s2*a2;
         c2=t;
         t=c3*a2-s3*b2;
         s3=-c3*b2-s3*a2;
         c3=t;
         t=c4*a1-s4*b1;
         s4=-c4*b1-s4*a1;
         c4=t;
    }
     }
     return;
}


/*     radix 8 multi-dimensional complex fourier transform kernel */
static void r8cftk (int n, int m, float *x0, float *y0, float *x1, float *y1,
       float *x2, float *y2, float *x3, float *y3, float *x4,
       float *y4, float *x5, float *y5, float *x6, float *y6,
       float *x7, float *y7, int *dim)
{
     int fold,zero;
     int j,k,k0,m8,mover2;
     int k1, k2, kk, l, l1, mm8, nt, size, sep;
     int ns;
     double angle;
     float c1,c2,c3,c4,c5,c6,c7,e = 0.70710678;
     float s1,s2,s3,s4,s5,s6,s7,t,twopi=6.2831853;
     float r1,r2,r3,r4,r5,r6,r7,rs0,rs1,rs2,rs3,ru0,ru1,ru2,ru3;
     float i1,i2,i3,i4,i5,i6,i7,is0,is1,is2,is3,iu0,iu1,iu2,iu3;
     float rss0,rss1,rsu0,rsu1,rus0,rus1,ruu0,ruu1;
     float iss0,iss1,isu0,isu1,ius0,ius1,iuu0,iuu1;
     float fjm1, fm8;
     int itrip, ntrip;
     /*      data twopi/6.2831853/, e/0.70710678/ */

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     m8=m*8;
     fm8 = m8;
     mm8 = sep*m8;
     mover2=m/2+1;

     fjm1 = -1.0;
     /*      do 600 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold=j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fm8;
    zero=angle == 0.0;
    if (!zero) {
         c1=cos(angle);
         s1=sin(angle);
         c2=c1*c1-s1*s1;
         s2=s1*c1+c1*s1;
         c3=c2*c1-s2*s1;
         s3=s2*c1+c2*s1;
         c4=c2*c2-s2*s2;
         s4=s2*c2+c2*s2;
         c5=c4*c1-s4*s1;
         s5=s4*c1+c4*s1;
         c6=c4*c2-s4*s2;
         s6=s4*c2+c4*s2;
         c7=c4*c3-s4*s3;
         s7=s4*c3+c4*s3;
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {

         /*      do 500 kk = k0, ns, mm8
           do 440 l = kk, nt, l1 */
         for (kk = k0; kk <= ns; kk += mm8) {
        for (l = kk; l <= nt; l += l1) {
       k1 = l + size;
       /*      do 420 k = l, k1, k2 */
       for (k = l - 1; k < k1; k += k2) {
            rs0=x0[k]+x4[k];
            is0=y0[k]+y4[k];
            ru0=x0[k]-x4[k];
            iu0=y0[k]-y4[k];
            rs1=x1[k]+x5[k];
            is1=y1[k]+y5[k];
            ru1=x1[k]-x5[k];
            iu1=y1[k]-y5[k];
            rs2=x2[k]+x6[k];
            is2=y2[k]+y6[k];
            ru2=x2[k]-x6[k];
            iu2=y2[k]-y6[k];
            rs3=x3[k]+x7[k];
            is3=y3[k]+y7[k];
            ru3=x3[k]-x7[k];
            iu3=y3[k]-y7[k];
            rss0=rs0+rs2;
            iss0=is0+is2;
            rsu0=rs0-rs2;
            isu0=is0-is2;
            rss1=rs1+rs3;
            iss1=is1+is3;
            rsu1=rs1-rs3;
            isu1=is1-is3;
            rus0=ru0-iu2;
            ius0=iu0+ru2;
            ruu0=ru0+iu2;
            iuu0=iu0-ru2;
            rus1=ru1-iu3;
            ius1=iu1+ru3;
            ruu1=ru1+iu3;
            iuu1=iu1-ru3;
            t=(rus1+ius1)*e;
            ius1=(ius1-rus1)*e;
            rus1=t;
            t=(ruu1+iuu1)*e;
            iuu1=(iuu1-ruu1)*e;
            ruu1=t;
            x0[k]=rss0+rss1;
            y0[k]=iss0+iss1;
            if (!zero) {
           r1=ruu0+ruu1;
           i1=iuu0+iuu1;
           r2=rsu0+isu1;
           i2=isu0-rsu1;
           r3=rus0+ius1;
           i3=ius0-rus1;
           r4=rss0-rss1;
           i4=iss0-iss1;
           r5=ruu0-ruu1;
           i5=iuu0-iuu1;
           r6=rsu0-isu1;
           i6=isu0+rsu1;
           r7=rus0-ius1;
           i7=ius0+rus1;
           x4[k]=r1*c1+i1*s1;
           y4[k]=i1*c1-r1*s1;
           x2[k]=r2*c2+i2*s2;
           y2[k]=i2*c2-r2*s2;
           x6[k]=r3*c3+i3*s3;
           y6[k]=i3*c3-r3*s3;
           x1[k]=r4*c4+i4*s4;
           y1[k]=i4*c4-r4*s4;
           x5[k]=r5*c5+i5*s5;
           y5[k]=i5*c5-r5*s5;
           x3[k]=r6*c6+i6*s6;
           y3[k]=i6*c6-r6*s6;
           x7[k]=r7*c7+i7*s7;
           y7[k]=i7*c7-r7*s7;
            } else {
           x4[k]=ruu0+ruu1;
           y4[k]=iuu0+iuu1;
           x2[k]=rsu0+isu1;
           y2[k]=isu0-rsu1;
           x6[k]=rus0+ius1;
           y6[k]=ius0-rus1;
           x1[k]=rss0-rss1;
           y1[k]=iss0-iss1;
           x5[k]=ruu0-ruu1;
           y5[k]=iuu0-iuu1;
           x3[k]=rsu0-isu1;
           y3[k]=isu0+rsu1;
           x7[k]=rus0-ius1;
           y7[k]=ius0+rus1;
            }
       }
        }
         }
         k0 = (m+1-j)*sep + 1;
         t=(c1+s1)*e;
         s1=(c1-s1)*e;
         c1=t;
         t=s2;
         s2=c2;
         c2=t;
         t=(-c3+s3)*e;
         s3=(c3+s3)*e;
         c3=t;
         c4=-c4;
         t=-(c5+s5)*e;
         s5=(-c5+s5)*e;
         c5=t;
         t=-s6;
         s6=-c6;
         c6=t;
         t=(c7-s7)*e;
         s7=-(c7+s7)*e;
         c7=t;
    }
     }
     return;
}

/*     radix prime multi-dimensional complex fourier transform kernel */
static void rpcftk (int n, int m, int p, int r, float *x, float *y, int *dim)
{
     /*      float x[r,p], y[r,p] */

     int fold,zero;
     double angle;
     float is,iu,rs,ru,t,twopi=6.2831853,xt,yt;
     float fu, fp, fjm1, fmp;
     int j,jj,k0,k,mover2,mp,pm,pp,u,v;
     int k1, k2, kk, l, l1, mmp, nt, size, sep;
     int ns;

     float aa[10][10], bb[10][10];
     float a[19], b[19], c[19], s[19];
     float ia[10], ib[10], ra[10], rb[10];
     int itrip, ntrip;

     nt = dim[1];
     sep = dim[2];
     l1 = dim[3];
     size = dim[4] - 1;
     k2 = dim[5];
     ns = n*sep;
     mover2=m/2+1;
     mp=m*p;
     fmp = mp;
     mmp = sep*mp;
     pp=p/2;
     pm=p-1;
     fp = p;
     fu = 0.0;
     /*      do 100 u=1,pp */
     for (u = 1; u <= pp; u++) {
    fu = fu + 1.0;
    angle = twopi*fu/fp;
    jj=p-u;
    a[u]=cos(angle);
    b[u]=sin(angle);
    a[jj]=a[u];
    b[jj]=-b[u];
     }
     /*      do 300 u=1,pp
  do 200 v=1,pp */
     for (u = 1; u <= pp; u++) {
    for (v = 1; v <= pp; v++) {
         jj=u*v-u*v/p*p;
         aa[v][u]=a[jj];
         bb[v][u]=b[jj];

    }
     }

     fjm1 = -1.0;
     /*      do 1500 j=1,mover2 */
     for (j = 1; j <= mover2; j++) {
    fold=j > 1 && 2*j < m+2;
    k0 = (j-1)*sep + 1;
    fjm1 = fjm1 + 1.0;
    angle = twopi*fjm1/fmp;
    zero=angle == 0.0;
    if (!zero) {
         c[1]=cos(angle);
         s[1]=sin(angle);
         /*      do 400 u=2,pm */
         for (u = 2; u <= pm; u++) {
        c[u]=c[u-1]*c[1]-s[u-1]*s[1];
        s[u]=s[u-1]*c[1]+c[u-1]*s[1];
         }
    }
    ntrip = fold ? 2 : 1;
    for (itrip = 0; itrip < ntrip; itrip++) {

         /*      do 1400 kk = k0, ns, mmp
           do 1340 l = kk, nt, l1 */
         for (kk = k0; kk <= ns; kk += mmp) {
        for (l = kk; l <= nt; l += l1) {
       k1 = l + size;
       /*      do 1320 k = l, k1, k2 */
       for (k = l - 1; k < k1; k += k2) {
            /*      xt=x[k][1]
              yt=y[k,1]
              rs=x[k,2]+x[k,p]
              is=y[k,2]+y[k,p]
              ru=x[k,2]-x[k,p]
              iu=y[k,2]-y[k,p] */
            xt=x[k];
            yt=y[k];
            rs=x[k + r]+x[k + r * pm];
            is=y[k + r]+y[k + r * pm];
            ru=x[k + r]-x[k + r * pm];
            iu=y[k + r]-y[k + r * pm];
            /*      do 800 u=1,pp */
            for (u = 1; u <= pp; u++) {
           ra[u]=xt+rs*aa[u][1];
           ia[u]=yt+is*aa[u][1];
           rb[u]=ru*bb[u][1];
           ib[u]=iu*bb[u][1];
            }
            xt=xt+rs;
            yt=yt+is;

            /*      do 1000 u=2,pp */
            for (u = 2; u <= pp; u++) {    /* u numbers from 1 not 0 */
           jj=p-u;
           /*      rs=x[k,u+1]+x[k,jj+1]
             is=y[k,u+1]+y[k,jj+1]
             ru=x[k,u+1]-x[k,jj+1]
             iu=y[k,u+1]-y[k,jj+1] */
           rs=x[k + u * r]+x[k + jj * r];
           is=y[k + u * r]+y[k + jj * r];
           ru=x[k + u * r]-x[k + jj * r];
           iu=y[k + u * r]-y[k + jj * r];
           xt=xt+rs;
           yt=yt+is;
           /*      do 900 v=1,pp */
           for (v = 1; v <= pp; v++) {
          ra[v]=ra[v]+rs*aa[v][u];
          ia[v]=ia[v]+is*aa[v][u];
          rb[v]=rb[v]+ru*bb[v][u];
          ib[v]=ib[v]+iu*bb[v][u];
           }
            }
            x[k]=xt;
            y[k]=yt;
            /*      do 1300 u=1,pp */
            for (u = 1; u <= pp; u++) {
           jj=p-u;
           if (!zero) {
          xt=ra[u]+ib[u];
          yt=ia[u]-rb[u];
          x[k + u * r]=xt*c[u]+yt*s[u];
          y[k + u * r]=yt*c[u]-xt*s[u];
          xt=ra[u]-ib[u];
          yt=ia[u]+rb[u];
          x[k + jj * r]=xt*c[jj]+yt*s[jj];
          y[k + jj * r]=yt*c[jj]-xt*s[jj];
           } else {
          x[k + u * r]=ra[u]+ib[u];
          y[k + u * r]=ia[u]-rb[u];
          x[k + jj * r]=ra[u]-ib[u];
          y[k + jj * r]=ia[u]+rb[u];
           }
            }
       }
        }
         }
         if (!fold) break;
         k0 = (m+1-j)*sep + 1;
         /*      do 600 u=1,pm */
         for (u = 1; u <= pm; u++) {
        t=c[u]*a[u]+s[u]*b[u];
        s[u]=-s[u]*a[u]+c[u]*b[u];
        c[u]=t;
         }
    }
     }

     return;
}


/*     double in place reordering programme */
#define al u[1]
#define bs s[2]
#define bl u[2]
#define cs s[3]
#define cl u[3]
#define ds s[4]
#define dl u[4]
#define es s[5]
#define el u[5]
#define fs s[6]
#define fl u[6]
#define gs s[7]
#define gl u[7]
#define hs s[8]
#define hl u[8]
#define is s[9]
#define il u[9]
#define js s[10]
#define jl u[10]
#define ks s[11]
#define kl u[11]
#define ls s[12]
#define ll u[12]
#define ms s[13]
#define ml u[13]
#define ns s[14]
#define nl u[14]

static void diprp (int pts, int *sym, int psym, int *unsym, int *dim,
      float *x, float *y)
{
     float t;
     int onemod;
     int modulo [15];
     int dk,jj,kk,lk,mods,mult,nest,punsym,test;
     int nt, sep, delta, p, p0, p1, p2, p3, p4, p5, size;

     int s [15], u [15];
     int a,b,c,d,e,f,g,h,i,j,k,l,m,n;
     /*      int bs,cs,ds,es,fs,gs,hs,is,js,ks,ls,ms,ns;
       int al,bl,cl,dl,el,fl,gl,hl,il,jl,kl,ll,ml,nl; */
     /*      equivalence           (al,u(1)),(bs,s(2)),(bl,u(2))
       equivalence (cs,s(3)),(cl,u(3)),(ds,s(4)),(dl,u(4))
       equivalence (es,s(5)),(el,u(5)),(fs,s(6)),(fl,u(6))
       equivalence (gs,s(7)),(gl,u(7)),(hs,s(8)),(hl,u(8))
       equivalence (is,s(9)),(il,u(9)),(js,s(10)),(jl,u(10))
       equivalence (ks,s(11)),(kl,u(11)),(ls,s(12)),(ll,u(12))
       equivalence (ms,s(13)),(ml,u(13)),(ns,s(14)),(nl,u(14)) */


     nest=14;

     nt = dim[1];
     sep = dim[2];
     p2 = dim[3];
     size = dim[4] - 1;
     p4 = dim[5];
     if (sym[1] != 0) {
    for (j = 1; j <= nest; j++) { 
         /*     do 100 j=1,nest */
         u[j]=1;
         s[j]=1;
    }
    n=pts;
    /*      do 200 j=1,nest */
    for (j = 1; j <= nest; j++) {
         if (sym[j] == 0) 
        break;
         jj=nest+1-j;
         u[jj]=n;
         s[jj]=n/sym[j];
         n=n/sym[j];
    }

    jj=0;
    /*      do 400 a=1,al
      do 400 b=a,bl,bs
      do 400 c=b,cl,cs
      do 400 d=c,dl,ds
      do 400 e=d,el,es
      do 400 f=e,fl,fs
      do 400 g=f,gl,gs
      do 400 h=g,hl,hs
      do 400 i=h,il,is
      do 400 j=i,jl,js
      do 400 k=j,kl,ks
      do 400 l=k,ll,ls
      do 400 m=l,ml,ms
      do 400 n=m,nl,ns */
    for (a = 1; a <= al; a++)
         for (b = a; b <= bl; b += bs)
        for (c = b; c <= cl; c += cs)
       for (d = c; d <= dl; d += ds)
            for (e = d; e <= el; e += es)
           for (f = e; f <= fl; f += fs)
          for (g = f; g <= gl; g += gs)
               for (h = g; h <= hl; h += hs)
              for (i = h; i <= il; i += is)
                   for (j = i; j <= jl; j += js)
                  for (k = j; k <= kl; k += ks)
                 for (l = k; l <= ll; l += ls)
                      for (m = l; m <= ml; m += ms)
                     for (n = m; n <= nl; n += ns) {
                    jj=jj+1;
                    if (jj >= n)
                         continue;
                    delta = (n-jj)*sep;
                    p1 = (jj-1)*sep + 1;
                    /*      do 350 p0 = p1, nt, p2 */
                    for (p0 = p1; p0 <= nt; p0 += p2) {
                         p3 = p0 + size;
                         /*      do 350 p = p0, p3, p4 */
                         for (p = p0 - 1; p < p3; p += p4) {
                        p5 = p + delta;
                        t = x[p];
                        x[p] = x[p5];
                        x[p5] = t;
                        t = y[p];
                        y[p] = y[p5];
                        y[p5] = t;
                         }
                    }
                     }
     }

     if (unsym[1] == 0)
    return;
     punsym=pts/(psym*psym);
     mult=punsym/unsym[1];
     test=(unsym[1]*unsym[2]-1)*mult*psym;
     lk=mult;
     dk=mult;
     /*      do 600 k=2,nest */
     for (k = 2; k <= nest; k++) {
    if (unsym[k] == 0) 
         break;
    lk=lk*unsym[k-1];
    dk=dk/unsym[k];
    u[k]=(lk-dk)*psym;
    mods=k;
     }
     onemod=mods < 3;
     if (!onemod) {
    for (j = 3; j <= mods; j++) {
         /*      do 800 j=3,mods */
         jj=mods+3-j;
         modulo[jj]=u[j];
    }
     }
     modulo[2]=u[2];
     jl=(punsym-3)*psym;
     ms=punsym*psym;

     /*     do 1800 j=psym,jl,psym */
     for (j = psym; j <= jl; j += psym) {
    k=j;

    /*  1000 continue */
    do {
         k=k*mult;
         if (!onemod)
        /*      do 1100 i=3,mods */
        for (i = 3; i <= mods; i++)
       k=k-(k/modulo[i])*modulo[i];

         if (k < test)
        k=k-(k/modulo[2])*modulo[2];
         else
        k=k-(k/modulo[2])*modulo[2]+modulo[2];
         /*      if (k.lt.j) go to 1000 */
    } while (k < j);
    
    if (k != j) {
         delta = (k-j)*sep;
         /*      do 1600 l=1,psym
           do 1500 m=l,pts,ms */
         for (l = 1; l <= psym; l++) {
        for (m = l; m <= pts; m += ms) {
       p1 = (m+j-1)*sep + 1;
       /*      do 1500 p0 = p1, nt, p2 */
       for (p0 = p1; p0 <= nt; p0 += p2) {
            p3 = p0 + size;
            /*      do 1500 jj = p0, p3, p4 */
            for (jj = p0 - 1; jj < p3; jj += p4) {
           kk = jj + delta;
           t=x[jj];
          x[jj]=x[kk];
          x[kk]=t;
          t=y[jj];
          y[jj]=y[kk];
          y[kk]=t;
            }
       }
        }
         }
    }
     }
     return;
}
