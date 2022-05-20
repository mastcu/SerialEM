// XCorr.cpp:             Free standing functions for computations, prefixed with
//                          XCorr, Proc, or Stat generally.  This is the one
//                          module that gets optimized for a debug build
//
// Copyright (C) 2003-2017 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include <math.h>
#include <string.h>
#include "XCorr.h"
#include "b3dutil.h"
#include "mrcslice.h"
#include "..\Shared\CorrectDefects.h"

void SEMTrace(char key, char *fmt, ...);
void PrintfToLog(char *fmt, ...);

#pragma warning ( disable : 4244 )

#define BYTE SLICE_MODE_BYTE
#define UNSIGNED_SHORT SLICE_MODE_USHORT
#define SIGNED_SHORT SLICE_MODE_SHORT
#define FLOAT SLICE_MODE_FLOAT

#define DTOR 0.01745329252

#if defined(USE_FFTW2) && !defined(_WIN64)
#include "rfftw.h"
#else
#include "cfft.h"
#endif

static float amax(float x1, float x2)
{
     return x2 > x1 ? x2 : x1;
}
static int imax(int i1, int i2)
{
     return i2 > i1 ? i2 : i1;
}
static float amin(float x1, float x2)
{
     return x2 < x1 ? x2 : x1;
}
static int imin(int i1, int i2)
{
     return i2 < i1 ? i2 : i1;
}

// A generalized 2-D FFT routine with calling convention compatible with IMOD todfft
void twoDfft(float *array, int *nxpad, int *nypad, int *dir)
{
#ifdef USE_FFTW2
  rfftwnd_plan plan;
  fftw_real dumreal;
  fftw_complex dumcomp;
  plan = rfftw2d_create_plan(*nypad, *nxpad, (*dir) ? FFTW_BACKWARD : FFTW_FORWARD, 
    FFTW_IN_PLACE);
  if (*dir)
    rfftwnd_one_complex_to_real(plan, (fftw_complex *)array, &dumreal);
  else
    rfftwnd_one_real_to_complex(plan, array, &dumcomp);
  rfftwnd_destroy_plan(plan);

#else
  todfftc(array, *nxpad, *nypad, *dir);
#endif
}

void XCorrCrossCorr(float *array, float *brray, int nxpad, int nypad, 
                    float deltap, float *ctfp, float *crray)
{
#ifdef USE_FFTW2
  rfftwnd_plan plan;
  fftw_real dumreal;
  fftw_complex dumcomp;
#endif
  
  /* set mean of one of the arrays to zero */
  XCorrMeanZero(array, nxpad+2, nxpad, nypad);
  
  /* take forward transforms */
#ifndef USE_FFTW2
  todfftc(array, nxpad, nypad, 0);
  if (array != brray)
    todfftc(brray, nxpad, nypad, 0);
#else
  plan = rfftw2d_create_plan(nypad, nxpad, FFTW_FORWARD, FFTW_IN_PLACE);
  rfftwnd_one_real_to_complex(plan, array, &dumcomp);
  if (array != brray)
    rfftwnd_one_real_to_complex(plan, brray, &dumcomp);
  rfftwnd_destroy_plan(plan);
#endif       
  /* filter if desired */
  if(deltap != 0.)
    XCorrFilterPart(array, array, nxpad, nypad, ctfp, deltap);
  if (crray) {
    if(deltap != 0.)
      XCorrFilterPart(brray, brray, nxpad, nypad, ctfp, deltap);
    memcpy(crray, array, (nxpad + 2) * nypad * sizeof(float));
  }

  /* multiply array by complex conjugate of brray, put back in array */
  conjugateProduct(array, brray, nxpad, nypad);  
  
  
#ifndef USE_FFTW2
  todfftc(array, nxpad, nypad, 1);
  if (crray) {
    todfftc(brray, nxpad, nypad, 1);
    todfftc(crray, nxpad, nypad, 1);
  }
#else
  plan = rfftw2d_create_plan(nypad, nxpad, FFTW_BACKWARD, FFTW_IN_PLACE);
  rfftwnd_one_complex_to_real(plan, (fftw_complex *)array, &dumreal);
  if (crray) {
    rfftwnd_one_complex_to_real(plan, (fftw_complex *)brray, &dumreal);
    rfftwnd_one_complex_to_real(plan, (fftw_complex *)crray, &dumreal);
  }
  rfftwnd_destroy_plan(plan);
#endif
}

void XCorrRealCorr(float *array, float *brray, int nxpad, int nypad, int maxdist,
  float deltap, float *ctfp, float *peak)
{
  double suma, tsuma, sumb, tsumb, meana, meanb, sumab, abar, bbar, corr;
     int dx, dy, jx, jy, xbase;
#ifdef USE_FFTW2
     rfftwnd_plan plan;
     fftw_real dumreal;
     fftw_complex dumcomp;
#endif

     /* filter one array if desired */
     if(deltap != 0.) {
#ifndef USE_FFTW2
        todfftc(brray, nxpad, nypad, 0);
#else
    plan = rfftw2d_create_plan(nypad, nxpad, FFTW_FORWARD, FFTW_IN_PLACE);
    rfftwnd_one_real_to_complex(plan, brray, &dumcomp);
    rfftwnd_destroy_plan(plan);
#endif        

    XCorrFilterPart(brray, brray, nxpad, nypad, ctfp, deltap);

#ifndef USE_FFTW2
    todfftc(brray, nxpad, nypad, 1);
#else
    plan = rfftw2d_create_plan(nypad, nxpad, FFTW_BACKWARD, FFTW_IN_PLACE);
    rfftwnd_one_complex_to_real(plan, (fftw_complex *)brray, &dumreal);
    rfftwnd_destroy_plan(plan);
#endif
   }

  // loop on positions
  *peak = -1.e20f;
  for (dy = -maxdist; dy <= maxdist; dy++) {
    for (dx = -maxdist; dx <= maxdist; dx++) {
      
      // Get means
      suma = 0.;
      sumb = 0.;
      for (jy = maxdist + dy; jy < nypad + dy - maxdist - 1; jy++) {
        xbase = jy * (nxpad + 2) + dx;
        tsuma = 0.;
        tsumb = 0.;
        for (jx = xbase + maxdist; jx < xbase + nxpad - maxdist - 1; jx++) {
          tsuma += array[jx];
          tsumb += brray[jx];
        }
        suma += tsuma;
        sumb += tsumb;
      }
      meana = suma / ((nxpad - 2 * maxdist) * (nypad - 2 * maxdist));
      meanb = sumb / ((nxpad - 2 * maxdist) * (nypad - 2 * maxdist));

      // Get sum of squares and cross-products
      suma = 0.;
      sumb = 0.;
      sumab = 0.;
      for (jy = maxdist + dy; jy < nypad + dy - maxdist - 1; jy++) {
        xbase = jy * (nxpad + 2) + dx;
        for (jx = xbase + maxdist; jx < xbase + nxpad - maxdist - 1; jx++) {
          abar = array[jx] - meana;
          bbar = brray[jx] - meanb;
          suma += abar * abar;
          sumb += bbar * bbar;
          sumab += abar * bbar;
        }
      }
      
      corr = sumab / sqrt(suma * sumb);
      if (corr > *peak)
        *peak = (float)corr;
    }
  }
}

void XCorrTripleCorr(float *array, float *brray, float *crray, int nxpad, int nypad, 
  float deltap, float *ctfp)
{
  float a, b, c, d;
  int jx, jp1;
#ifdef USE_FFTW2
  rfftwnd_plan plan;
  fftw_real dumreal;
  fftw_complex dumcomp;
#endif
  
  /* set mean of the middle array to zero */
  XCorrMeanZero(brray, nxpad+2, nxpad, nypad);
  
  /* take forward transforms */
#ifndef USE_FFTW2
  todfftc(array, nxpad, nypad, 0);
  todfftc(brray, nxpad, nypad, 0);
  todfftc(crray, nxpad, nypad, 0);
#else
  plan = rfftw2d_create_plan(nypad, nxpad, FFTW_FORWARD, FFTW_IN_PLACE);
  rfftwnd_one_real_to_complex(plan, array, &dumcomp);
  rfftwnd_one_real_to_complex(plan, brray, &dumcomp);
  rfftwnd_one_real_to_complex(plan, crray, &dumcomp);
  rfftwnd_destroy_plan(plan);
#endif       
  
  /* filter middle array if desired */
  if(deltap != 0.) 
    XCorrFilterPart(brray, brray, nxpad, nypad, ctfp, deltap);
  
  /* multiply array by complex conjugate of brray, put back in array */
  
  for (jx = 0; jx < nypad*(nxpad+2); jx += 2) {
    jp1 = jx + 1;
    a = array[jx];
    b = array[jp1];
    c = brray[jx];
    d = brray[jp1];
    
    array[jx] = a * c + b * d;
    array[jp1] = b * c - a * d;
    
    a = c;
    b = d;
    c = crray[jx];
    d = crray[jp1];
    brray[jx] = a * c + b * d;
    brray[jp1] = b * c - a * d;
  }
  
  /* take reverse transform */
  
#ifndef USE_FFTW2
  todfftc(array, nxpad, nypad, 1);
  todfftc(brray, nxpad, nypad, 1);
#else
  plan = rfftw2d_create_plan(nypad, nxpad, FFTW_BACKWARD, FFTW_IN_PLACE);
  rfftwnd_one_complex_to_real(plan, (fftw_complex *)array, &dumreal);
  rfftwnd_one_complex_to_real(plan, (fftw_complex *)brray, &dumreal);
  rfftwnd_destroy_plan(plan);
#endif
}

// Filters the image in array with the given type and size into the array brray, which
// must be a (nxpad +2) * nypad float array, with the standard filter specified in ctf
// and delta.  array can be the same as brray as long as it is big enough
void XCorrFilter(float *array, int type, int nxin, int nyin, float *brray, int nxpad, 
  int nypad, float delta, float *ctf)
{
  float *inp, *outp = brray;
#ifdef USE_FFTW2
  rfftwnd_plan plan;
  fftw_real dumreal;
  fftw_complex dumcomp;
#endif
  int ixlo = (nxpad - nxin) / 2;
  int iylo = (nypad - nyin) / 2;

  // Move data into center of padded array
  sliceTaperOutPad(array, type, nxin, nyin, brray, nxpad + 2, nxpad, nypad,
    0, 0.);

  // Filter it laboriously
  if (delta) {
#ifndef USE_FFTW2
    todfftc(brray, nxpad, nypad, 0);
#else
    plan = rfftw2d_create_plan(nypad, nxpad, FFTW_FORWARD, FFTW_IN_PLACE);
    rfftwnd_one_real_to_complex(plan, brray, &dumcomp);
    rfftwnd_destroy_plan(plan);
#endif        

    XCorrFilterPart(brray, brray, nxpad, nypad, ctf, delta);

#ifndef USE_FFTW2
    todfftc(brray, nxpad, nypad, 1);
#else
    plan = rfftw2d_create_plan(nypad, nxpad, FFTW_BACKWARD, FFTW_IN_PLACE);
    rfftwnd_one_complex_to_real(plan, (fftw_complex *)brray, &dumreal);
    rfftwnd_destroy_plan(plan);
#endif
  }

  //Move data back to contiguous space in brray
  for (int iy = iylo; iy < iylo + nyin; iy++) {
    inp = &brray[ixlo + (nxpad + 2) * iy];
    for (int ix = 0; ix < nxin; ix++)
      *outp++ = *inp++;
  }
}

#define SCALE_ARRAY(nam, in, out, lo, hi, typ) \
  case nam:   \
    for (ind = 0; ind < nx * ny; ind++) {  \
      value = in[ind] * factor + offset;   \
      B3DCLAMP(value, lo, hi);   \
      out[ind] = (typ)value;  \
    }   \
    break;

// Scale an array of the given type by a factor and add an offset, clamping as needed
// for non-float types
void ProcScaleImage(void *array, int type, int nx, int ny, float factor, float offset,
  void *brray)
{
  int ind;
  float value;
  float *fData = (float *)array;
  unsigned char *bData = (unsigned char *)array;
  unsigned short *usData = (unsigned short *)array;
  short *sData = (short *)array;
  float *fOut = (float *)brray;
  unsigned char *bOut = (unsigned char *)brray;
  unsigned short *usOut = (unsigned short *)brray;
  short *sOut = (short *)brray;

  switch (type) {
    SCALE_ARRAY(BYTE, bData, bOut, 0., 255., unsigned char);
    SCALE_ARRAY(SIGNED_SHORT, sData, sOut, -32768., 32767., short);
    SCALE_ARRAY(UNSIGNED_SHORT, usData, usOut, 0., 65535., unsigned short);

  case FLOAT:
    for (ind = 0; ind < nx * ny; ind++)
      fOut[ind] = fData[ind] * factor + offset;
    break;
  }
}

void XCorrBinBy2(void *array, int type, int nxin, int nyin, short int *brray)
{
  XCorrBinByN(array, type, nxin, nyin, 2, brray);
}

void XCorrBinByN(void *array, int type, int nxin, int nyin, int nbin, 
         short int *brray)
{
  int nxr, nyr;
  reduceByBinning(array, type, nxin, nyin, nbin, brray, 0, &nxr, &nyr);
}


// For making a binned gain reference, need to bin the inverse
// This is needed only for unsigned short and float
void XCorrBinInverse(void *array, int type, int nxin, int nyin, int ixofs, int iyofs, 
                     int nbin, int nxout, int nyout, void *brray)
{
  int i, j;
  int nbinsq = nbin * nbin;
  //int nxout = nxin / nbin;
  //int nyout = nyin / nbin;
  //int ixofs = (nxin % nbin) / 2;
  //int iyofs = (nyin % nbin) / 2;
  int ix, iy;
  int xofslo, xofshi, yofslo, yofshi;
  float fsum;
  unsigned short int *usdata = (unsigned short int *)brray;
  unsigned short int *usline1, *usline2;
  float *fdata = (float *)brray;
  float *fline1, *fline2;

  // Determine offsets into the region of good pixels that can be composed without
  // testing for pixels off the edge
  xofslo = (-ixofs + nbin - 1) / nbin;
  if (xofslo < 0)
    xofslo = 0;
  xofshi = (nbin * nxout + ixofs - nxin + nbin - 1) / nbin;
  if (xofshi < 0)
    xofshi = 0;
  yofslo = (-iyofs + nbin - 1) / nbin;
  if (yofslo < 0)
    yofslo = 0;
  yofshi = (nbin * nyout + iyofs - nyin + nbin - 1) / nbin;
  if (yofshi < 0)
    yofshi = 0;

  switch (type) {  
  case UNSIGNED_SHORT:
    for (iy = 0; iy < nyout; iy++) {

      // Do lines above and below good region pixel-by-pixel with tests
      if (iy < yofslo || iy >= nyout - yofshi) {
        for (ix = 0; ix < nxout; ix++)
          *usdata++ = (unsigned short int)XCorrOneInverseBin(array, type, nxin, nyin,
          ixofs, iyofs, nbin, ix, iy);
      } else {

        // Then do testing on left and right edge of lines, g fast in the middle
        for (ix = 0; ix < xofslo; ix++)
          *usdata++ = (unsigned short int)XCorrOneInverseBin(array, type, nxin, nyin,
          ixofs, iyofs, nbin, ix, iy);
        usline1 = ((unsigned short int *)array) + (nbin * iy + iyofs) * nxin +
          xofslo * nbin + ixofs;
        for (ix = xofslo; ix < nxout - xofshi; ix++) {
          fsum = 0.;
          usline2 = usline1;
          for (j = 0; j < nbin; j++) {
            for (i = 0; i < nbin; i++) 
              fsum += 1. / usline2[i];
            usline2 += nxin;
          }
          *usdata++ = (unsigned short int)(nbinsq / fsum);
          usline1 += nbin;
        }
        for (ix = nxout - xofshi; ix < nxout; ix++)
          *usdata++ = (unsigned short int)XCorrOneInverseBin(array, type, nxin, nyin,
                       ixofs, iyofs, nbin, ix, iy);
      }
    }
    break;
    
  case FLOAT:
    for (iy = 0; iy < nyout; iy++) {
      if (iy < yofslo || iy >= nyout - yofshi) {
        for (ix = 0; ix < nxout; ix++)
          *fdata++ = XCorrOneInverseBin(array, type, nxin, nyin,
          ixofs, iyofs, nbin, ix, iy);
      } else {
        for (ix = 0; ix < xofslo; ix++)
          *fdata++ = XCorrOneInverseBin(array, type, nxin, nyin,
          ixofs, iyofs, nbin, ix, iy);
        fline1 = ((float *)array) + (nbin * iy + iyofs) * nxin + xofslo * nbin + ixofs;
        for (ix = xofslo; ix < nxout - xofshi; ix++) {
          fsum = 0.;
          fline2 = fline1;
          for (j = 0; j < nbin; j++) {
            for (i = 0; i < nbin; i++) 
              fsum += 1.f / fline2[i];
            fline2 += nxin;
          }
          *fdata++ = nbinsq / fsum;
          fline1 += nbin;
        }
        for (ix = nxout - xofshi; ix < nxout; ix++)
          *fdata++ = XCorrOneInverseBin(array, type, nxin, nyin,
          ixofs, iyofs, nbin, ix, iy);
      }
    }
    break;
  }
}

// Do the inverse binning at one pixel that requires checking of whether there are
// input data at each input pixel
float XCorrOneInverseBin(void *array, int type, int nxin, int nyin, int ixofs, int iyofs, 
                     int nbin, int ixout, int iyout)
{
  int i, j, js, is;
  float fsum = 0.;
  int nsum = 0;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;

  js = iyout * nbin + iyofs;
  is = ixout * nbin + ixofs;
  switch (type) {  
  case UNSIGNED_SHORT:
    for (j = js; j < js + nbin; j++) {
      for (i = is; i < is + nbin; i++) {
        if (i >= 0 && i < nxin && j >= 0 && j < nyin) {
          fsum += 1.f / usdata[j * nxin + i];
          nsum++;
        }
      }
    }
    break;

  case FLOAT:
    for (j = js; j < js + nbin; j++) {
      for (i = is; i < is + nbin; i++) {
        if (i >= 0 && i < nxin && j >= 0 && j < nyin) {
          fsum += 1.f / fdata[j * nxin + i];
          nsum++;
        }
      }
    }
    break;
  }
  if (nsum)
    return (float)(nsum / fsum);
  return 1.;
}

/*  NICEFRAME returns NUM if it has no prime factor greater
    than LIMIT, or adds IDNUM to NUM until it reaches a number with this
    tractable property */

int XCorrNiceFrame(int num, int idnum, int limit)
{
  if (limit == 19)
#ifdef USE_FFTW2
    limit = 13;
#else
     limit = niceFFTlimit();
#endif
  int numin, numtmp, ifac;
  numin=2 * ((num + 1) / 2);
  if (numin <= 0)
    return 2;
  do {
    numtmp=numin;
    for (ifac = 2; ifac <= limit; ifac++)
      while (numtmp % ifac == 0)
        numtmp=numtmp/ifac;

    if (numtmp > 1)
      numin += idnum;
  } while (numtmp > 1);
  return numin;
}

#define FILL_TAPER_GAP(dat)  \
  for (ix = ix0; ix <= ix1; ix++) {   \
    if (idirx * (xdata - ix) > ntaper && disty > ntaper) {  \
      dat[ibase + ix] = dmean;   \
    } else {   \
      distx = idirx * (xdata - ix);   \
      if (distx < disty) {       \
        frac = (float)distx / ntaper;    \
        dat[ibase + ix] = frac * dmean + (1. - frac) * dat[ibase + xdata];   \
      } else {    \
        frac = (float)disty / ntaper;  \
        dat[ibase + ix] = frac * dmean + (1. - frac) * dat[ydata * nx + ix];   \
      }   \
    }    \
  }


// XCORRFILLTAPERGAP will fill in the box in the data array, dimensioned nx by ny,
// specified by starting and ending coordinates ix0, ix1 and iy0, iy1.  The iy coordinates
// are assumed to need inversion.  Dmean specifies the fill value.  Values are tapered to
// the mean when they are within ntaper pixels of the edge of the data, where idirx and
// idiry indicate the direction in which data lie (-1 to bottom/left, 1 to top/right)

void XCorrFillTaperGap(short int *data, int type, int nx, int ny, float dmean, int ix0, 
  int ix1, int idirx, int iy0, int iy1, int idiry, int ntaper)
{
  int distx, disty;
  int iy, ix;
  float frac;
  unsigned short*usData = (unsigned short *)data;
  float *fData = (float *)data;


  // Invert sense of Y values
  int iytmp = (ny - 1) - iy1;
  iy1 = (ny - 1) - iy0;
  iy0 = iytmp;
  idiry = -idiry;

  if (ix0 > ix1 || iy0 > iy1)
    return;

  int xdata = ix0 - 1;
  if (idirx > 0)
    xdata = ix1 + 1;

  int ydata = iy0 - 1;
  if (idiry > 0)
    ydata = iy1 + 1;

  for (iy = iy0; iy <= iy1; iy++) {
    int ibase = iy * nx;
    disty = idiry * (ydata - iy);
    if (type == UNSIGNED_SHORT) {
      FILL_TAPER_GAP(usData);

    } else if (type == FLOAT) {
      FILL_TAPER_GAP(fData);
    } else {
      FILL_TAPER_GAP(data);
    }
  }
}

void ProcSampleMeanSD(void *array, int type, int nx, int ny, float *mean, float *sd)
{
  CorDefSampleMeanSD(array, type, nx, ny, mean, sd);
}

// Stretch an array by stretch along the given axis (in degrees)
void XCorrStretch(void *array, int type, int nx, int ny, float stretch, float axis,
          void *brray, float *a11, float *a12, float *a21, float *a22)
{
  double cosphi, sinphi;
  double phi = axis * 0.01745329252;
  cosphi = cos(phi);
  sinphi = sin(phi);
  *a11 = stretch * cosphi * cosphi + sinphi * sinphi;
  *a12 = (stretch - 1.) * cosphi * sinphi;
  *a21 = *a12;
  *a22 = cosphi * cosphi + stretch * sinphi * sinphi;
  XCorrFastInterp(array, type, brray, nx, ny, nx, ny, *a11, *a12, *a21, *a22, nx / 2.f,
    ny / 2.f, 0.f, 0.f);
}

/* Fastinterp

  This function will perform coordinate transformations
  (rotations,translations, etc.) by linear interpolation.
       It eliminates all range tests from the inner loop, but falls back
  to using range tests around the edges of
  the input image area.

  BRAY = T[ ARRAY ]

  ARRAY - The input image array
  BRAY  - The output image array
  NXA,NYA - The dimensions of ARRAY
  NXB,NYB - The dimensions of BRAY
  AMAT  - A 2x2 matrix to specify rotation,scaling,skewing
  XC,YC - The cooridinates of the Center of ARRAY
  XT,YT - The translation to add to the final image. The
      center of the output array is normally taken as
      NXB/2, NYB/2
  
  Xo = a11(Xi - Xc) + a12(Yi - Yc) + NXB/2. + XT
  Yo = a21(Xi - Xc) + a22(Yi - Yc) + NYB/2. + YT
    
    written in Fortran by DNM, April 2000.
    translated to C for SerialEM, 3/26/02
    incorporated bug fixes from revisions 3.1 and 3.3 in IMOD, 1/5/07
*/

#define FALLBACK_INTERP(tnam, dtyp, tmean, tdata, tbray)      \
  case tnam:                                                  \
  tmean = (dtyp)mean;                                           \
  for (ix = iqst; ix <= iqnd; ix++) {                           \
    xp = a11*ix + xbase;                                          \
    yp = a21*ix + ybase;                                                   \
    ixp = (int)xp;                                                          \
    iyp = (int)yp;                                                          \
    if (ixp  >=  0 && ixp  <  nxa - 1 && iyp  >=  0 && iyp  <  nya - 1) {   \
      dx = xp - ixp;                                                        \
      dy = yp - iyp;                                                        \
      omdy = 1. - dy;       \
      indbase = ixp + iyp * nxa;                       \
      tbray[iybase + ix] = (1.-dx) * (omdy * tdata[indbase] +          \
                                      dy * tdata[indbase + nxa]) +    \
        dx * (omdy * tdata[indbase + 1] +                             \
              dy * tdata[indbase + 1 + nxa]);                         \
   } else                                                            \
     tbray[iybase + ix] = tmean;                                     \
 }                                                                       \
 break;

#define FAST_INTERP(tnam, tdata, tbray)                         \
  case tnam:                                                    \
  for (ix = ixst; ix <= ixnd; ix++) {                           \
    ixp = (int)dxp;                                             \
    iyp = (int)dyp;                                             \
    dx = dxp - ixp;                                             \
    dy = dyp - iyp;                                             \
    omdy = 1.-dy;                                               \
    indbase = ixp + iyp * nxa;                                  \
    tbray[iybase + ix] = (1.-dx) *(omdy * tdata[indbase] +      \
                                   dy * tdata[indbase + nxa]) + \
      dx * (omdy * tdata[indbase + 1] +                         \
            dy * tdata[indbase + 1 + nxa]);                     \
    dxp += a11;                                                 \
    dyp += a21;                                                 \
  }                                                             \
  break;                                                        \


void XCorrFastInterp(void *array, int type, void *bray, int nxa, int nya,
    int nxb, int nyb, float amat11, float amat12, float amat21,
    float amat22, float xc, float yc, float xt, float yt)
{
  float xcen, ycen, xco, yco, a11, a12, a21, a22, denom;
  float dyo, xbase, ybase,xst, xnd, xlft,xrt, dx, dy, omdy, xp, yp, mean;
  int iy, ix, iybase, ixnd, ixst,iqst, iqnd, ifall, ixp, iyp, indbase;
  int indbasex, indbasey, indbasexy;
  double dxp, dyp;
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  short int *sbray = (short int *)bray;
  unsigned short int *ubray = (unsigned short int *)bray;
  unsigned char *bbray = (unsigned char *)bray;
  unsigned char *cdata = (unsigned char *)array;
  short int smean;
  unsigned char cmean;
  unsigned short int usmean;
  float *fdata = (float *)array;
  float *fbray = (float *)bray;
  int maxThreads = 8, numThreads;


  /* To deal with the fact that images are inverted in Y, negate the
    cross-terms, which is the same as pre and post inverting in Y */
  amat12 = -amat12;
  amat21 = -amat21;

  // Get a mean for fill
  if (type == SLICE_MODE_RGB) 
    mean = 127.;
  else
    ProcSampleMeanSD(array, type, nxa, nya, &mean, &denom);

  /*   Calc inverse transformation */
  /* remove + 1 from the next 4 lines in hopes that it is appropriate
  for c not fortran */
  xcen = nxb/2. + xt;
  ycen = nyb/2. + yt;
  xco = xc;
  yco = yc;
  denom = amat11*amat22 - amat12*amat21;
  a11 =  amat22/denom;
  a12 = -amat12/denom;
  a21 = -amat21/denom;
  a22 =  amat11/denom;

  numThreads = B3DNINT(0.04 * sqrt((double)nxb * nyb));
  B3DCLAMP(numThreads, 1, maxThreads);
  numThreads = numOMPthreads(numThreads);

  /* loop over output image */
#pragma omp parallel for num_threads(numThreads) default(none) \
  shared(a11, a12, a21, a22, xco, yco, xcen, ycen, nxa, nya, nxb, nyb, type, \
         mean, bbray, cdata, ubray, usdata, sbray, sdata, fbray, fdata)  \
  private(iy, iybase, dyo, xbase, ybase, xst, xnd, xlft, xrt, ixnd, ixst, iqst, \
    iqnd, ifall, cmean, smean, usmean, xp, yp, ixp, iyp, dx, dy, omdy, indbase, \
    indbasex, indbasey, indbasexy, ix, dxp, dyp)
  for (iy = 0; iy < nyb; iy++) {
	  iybase = iy * nxb;
    dyo = iy - ycen;
    xbase = a12*dyo +xco - a11*xcen;
    ybase = a22*dyo + yco - a21*xcen;
    xst = 0;
    xnd = nxb -1;
    if(fabs(a11) > 1.e-10) {
      xlft = (1.01 - xbase) / a11;
      xrt = (nxa-2.01 - xbase) / a11;
      xst = amax(xst, amin(xlft, xrt));
      xnd = amin(xnd, amax(xlft, xrt));
    } else if (xbase < 1. || xbase >= nxa - 2.) {
      xst = nxb - 1;
      xnd = 0;
    }
    if(fabs(a21) > 1.e-10) {
      xlft = (1.01-ybase) / a21;
      xrt = (nya - 2.01 - ybase) / a21;
      xst = amax(xst, amin(xlft, xrt));
      xnd = amin(xnd, amax(xlft, xrt));
    } else if (ybase < 1. || ybase >= nya - 2.) {
      xst = nxb - 1;
      xnd = 0;
    }
    
    /*    truncate the ending value down and the starting value up */
    ixnd = (int)amax(-1.e5, xnd);
    ixst = nxb + 1 - (int)(nxb + 1 -amin(xst, (float)(nxb + 1.)));

	/*  if they're crossed, set them up so fallback will do whole line */
    if(ixst > ixnd) {
      ixst = nxb / 2;
      ixnd = ixst - 1;
    }
    
    /*    do fallback to testing */
    iqst = 0;
    iqnd = ixst - 1;
    for (ifall = 0 ; ifall < 2; ifall++) {
      switch (type) {
        FALLBACK_INTERP(BYTE, unsigned char, cmean, cdata, bbray);
        FALLBACK_INTERP(SIGNED_SHORT, short int, smean, sdata, sbray);
        FALLBACK_INTERP(UNSIGNED_SHORT, unsigned short int, usmean, usdata, ubray);
        FALLBACK_INTERP(FLOAT, float, mean, fdata, fbray);

      case SLICE_MODE_RGB:
        cmean = (unsigned char)mean;
        for (ix = iqst; ix <= iqnd; ix++) {
          xp = a11 * ix + xbase;
          yp = a21 * ix + ybase;
          ixp = (int)xp;
          iyp = (int)yp;
          if (ixp  >=  0 && ixp  <  nxa - 1 && iyp  >=  0 && iyp  <  nya - 1) {
            dx = xp - ixp;
            dy = yp - iyp;
            omdy = 1. - dy;
            indbase = 3 * (ixp + iyp * nxa);
            indbasey = indbase + 3 * nxa;
            indbasex = indbase + 3;
            indbasexy = indbasey + 3;
            bbray[3 * (iybase + ix)] = (1. - dx) *(omdy * cdata[indbase] + 
              dy * cdata[indbasey]) + dx * (omdy * cdata[indbasex] + 
              dy * cdata[indbasexy]);
            bbray[3 * (iybase + ix) + 1] = (1. - dx) *(omdy * cdata[indbase+1] + 
              dy * cdata[indbasey+1]) + dx * (omdy * cdata[indbasex+1] + 
              dy * cdata[indbasexy+1]);
            bbray[3 * (iybase + ix) + 2] = (1. - dx) *(omdy * cdata[indbase+2] + 
              dy * cdata[indbasey+2]) + dx * (omdy * cdata[indbasex+2] + 
              dy * cdata[indbasexy+2]);
          } else {
            bbray[3 * (iybase + ix)] = cmean;
            bbray[3 * (iybase + ix) + 1] = cmean;
            bbray[3 * (iybase + ix) + 2] = cmean;
          }
        }
        break;
      }
      iqst = ixnd+1;
      iqnd = nxb-1;
    }
    
    /* Now do the safe region */
    dxp = a11 * ixst + xbase;
    dyp = a21 * ixst + ybase;
    switch (type) {
      FAST_INTERP(BYTE, cdata, bbray);
      FAST_INTERP(SIGNED_SHORT, sdata, sbray);
      FAST_INTERP(UNSIGNED_SHORT, usdata, ubray);
      FAST_INTERP(FLOAT, fdata, fbray);

    case SLICE_MODE_RGB:
      for (ix = ixst; ix <= ixnd; ix++) {
        ixp = (int)dxp;
        iyp = (int)dyp;
        dx = dxp - ixp;
        dy = dyp - iyp;
        omdy = 1. - dy;
        indbase = 3 * (ixp + iyp * nxa);
        indbasey = indbase + 3 * nxa;
        indbasex = indbase + 3;
        indbasexy = indbasey + 3;
        bbray[3 * (iybase + ix)] = (1. - dx) *(omdy * cdata[indbase] + 
          dy * cdata[indbasey]) + dx * (omdy * cdata[indbasex] + 
          dy * cdata[indbasexy]);
        bbray[3 * (iybase + ix) + 1] = (1. - dx) *(omdy * cdata[indbase+1] + 
          dy * cdata[indbasey+1]) + dx * (omdy * cdata[indbasex+1] + 
          dy * cdata[indbasexy+1]);
        bbray[3 * (iybase + ix) + 2] = (1. - dx) *(omdy * cdata[indbase+2] + 
          dy * cdata[indbasey+2]) + dx * (omdy * cdata[indbasex+2] + 
          dy * cdata[indbasexy+2]);
         dxp += a11;
        dyp += a21;
      }
      break;
    }
  }
}


// calculate the mean of an array within the given limits
double ProcImageMean(void *array, int type, int nx, int ny, int ix0, int ix1,
           int iy0, int iy1)
{
  double sum = 0.;
  int tsum, ix, iy;
  unsigned char *bdata;
  short int *sdata;
  unsigned short int *usdata;
  float *fdata;
  double fsum;

  for (iy = iy0; iy <= iy1; iy++) {
    tsum = 0;
    switch (type) {
    case BYTE:
      bdata = (unsigned char *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++)
        tsum += *bdata++;
      break;
      
    case SIGNED_SHORT:
      sdata = (short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++)
        tsum += *sdata++;
      break;

    case UNSIGNED_SHORT:
      usdata = (unsigned short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++)
        tsum += *usdata++;
      break;

    case FLOAT:
      fsum = 0.;
      fdata = (float *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++)
        fsum += *fdata++;
      sum += fsum;
      break;
    }
    sum += tsum;
  }
  return sum / ((ix1 + 1 - ix0) * (iy1 + 1 - iy0));
}

// Calculate the mean of a circular area specified by center point and radius
double ProcImageMeanCircle(void *array, int type, int nx, int ny, int cx, int cy,
  int radius)
{
  double sum = 0.;
  int area = 0;
  int ix0, ix1, iy0, iy1;
  int ysquared;
  int tsum, ix, iy;
  unsigned char *bdata;
  short int *sdata;
  unsigned short int *usdata;
  float *fdata;
  double fsum;
  int radSquared = radius * radius;

  ix0 = B3DMAX(0, cx - radius);
  ix1 = B3DMIN(nx - 1, cx + radius);
  iy0 = B3DMAX(0, cy - radius);
  iy1 = B3DMIN(ny - 1, cy + radius);

  for (iy = iy0; iy <= iy1; iy++) {
    tsum = 0;
    ysquared = (cy - iy) * (cy - iy);
    switch (type) {
    case BYTE:
      bdata = (unsigned char *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        if ((cx - ix) * (cx - ix) + ysquared <= radSquared) {
          tsum += *bdata;
          area++;
        }
        *bdata++;
      }
      break;

    case SIGNED_SHORT:
      sdata = (short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        if ((cx - ix) * (cx - ix) + ysquared <= radSquared) {
          tsum += *sdata;
          area++;
        }
        *sdata++;
      }
      break;

    case UNSIGNED_SHORT:
      usdata = (unsigned short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        if ((cx - ix) * (cx - ix) + ysquared <= radSquared) {
          tsum += *usdata;
          area++;
        }
        *usdata++;
      }
      break;

    case FLOAT:
      fsum = 0.;
      fdata = (float *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        if ((cx - ix) * (cx - ix) + ysquared <= radSquared) {
          fsum += *fdata;
          area++;
        }
        *fdata++;
      }
      sum += fsum;
      break;
    }
    sum += tsum;
  }
  if (!area)
    return 0.;
  return sum / area;
}

// Compute the min, max, mean and sd of the defined area of an image
void ProcMinMaxMeanSD(void *array, int type, int nx, int ny, int ix0, int ix1,
           int iy0, int iy1, float *mean, float *min, float *max, float *sd)
{
  double tmean = ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1);
  *mean = tmean;
  int iMin = 1000000;
  int iMax = -iMin;
  double tsum, sxsq = 0.;
  int ix, iy, ival;
  unsigned char *bdata;
  short int *sdata;
  unsigned short int *usdata;
  float *fdata;
  float fval;
  float fMin = 1.e38f;
  float fMax = -fMin;

  for (iy = iy0; iy <= iy1; iy++) {
    tsum = 0;
    switch (type) {
    case BYTE:
      bdata = (unsigned char *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        ival = *bdata++;
        if (ival < iMin)
          iMin = ival;
        if (ival > iMax)
          iMax = ival;
        tsum += (ival - tmean) * (ival - tmean);
      }
      break;
      
    case SIGNED_SHORT:
      sdata = (short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        ival = *sdata++;
        if (ival < iMin)
          iMin = ival;
        if (ival > iMax)
          iMax = ival;
        tsum += (ival - tmean) * (ival - tmean);
      }
      break;
      
    case UNSIGNED_SHORT:
      usdata = (unsigned short int *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        ival = *usdata++;
        if (ival < iMin)
          iMin = ival;
        if (ival > iMax)
          iMax = ival;
        tsum += (ival - tmean) * (ival - tmean);
      }
      break;
      
    case FLOAT:
      fdata = (float *)array + nx * iy + ix0;
      for (ix = ix0; ix <= ix1; ix++) {
        fval = *fdata++;
        if (fval < fMin)
          fMin = fval;
        if (fval > fMax)
          fMax = fval;
        tsum += (fval - tmean) * (fval - tmean);
      }
      break;
    }
    sxsq += tsum;
  }
  *sd = sqrt(sxsq / ((ix1 + 1 - ix0) * (iy1 + 1 - iy0) - 1));
  if (type != FLOAT) {
    *min = iMin;
    *max = iMax;
  } else {
    *min = fMin;
    *max = fMax;
  }
}

// Macro for ProcCentroid
#define CENTROID_SUMS(tnam, data, typ) \
    case tnam:  \
data = (typ *)array + nx * iy + ix0; \
for (ix = ix0; ix <= ix1; ix++) {  \
  tval = *data++ - baseval;  \
  tval = B3DMIN(tval, thresh);  \
  if (tval > 0) {  \
    xsum += ix * tval;  \
    ysum += iy * tval;  \
    wsum += tval;   \
  }   \
}  \
break;

// Compute the centroid of pixels in the defined area that are above the baseval
void ProcCentroid(void *array, int type, int nx, int ny, int ix0, int ix1,
           int iy0, int iy1, double baseval, float &xcen, float &ycen, double thresh)
{
  double xsum = 0.;
  double ysum = 0.;
  double wsum = 0.;
  double tval;
  int ix, iy;
  unsigned char *bdata;
  short int *sdata;
  unsigned short int *usdata;
  float *fdata;

  for (iy = iy0; iy <= iy1; iy++) {
    switch (type) {
      CENTROID_SUMS(BYTE, bdata, unsigned char);
      CENTROID_SUMS(SIGNED_SHORT, sdata, short int);
      CENTROID_SUMS(UNSIGNED_SHORT, usdata, unsigned short int);
      CENTROID_SUMS(FLOAT, fdata, float);
    }
  }
  xcen = xsum / wsum;
  ycen = ysum / wsum;
}

// These two routines are switched from expectations because Y is inverted
// Byte and int data are both returned as int, float is returned as float
// and rgb is returned as rgb
void ProcRotateRight(void *array, int type, int nx, int ny, short int *brray)
{
  int ix, iy;
  unsigned char *bdata;
  short int *sdata;
  float *fdata;
  float *fbray = (float *)brray;
  unsigned char *ubray = (unsigned char *)brray;

  for (ix = 0; ix < nx; ix++) {
    switch (type) {
    case BYTE:
      bdata = (unsigned char *)array + nx * (ny - 1) + ix;
      for (iy = 0; iy < ny; iy++) {
        *brray++ = *bdata;
        bdata -= nx;
      }
      break;
      
    case UNSIGNED_SHORT:
    case SIGNED_SHORT:
      sdata = (short int *)array + nx * (ny - 1) + ix;
      for (iy = 0; iy < ny; iy++) {
        *brray++ = *sdata;
        sdata -= nx;
      }
      break;

    case FLOAT:
      fdata = (float *)array + nx * (ny - 1) + ix;
      for (iy = 0; iy < ny; iy++) {
        *fbray++ = *fdata;
        fdata -= nx;
      }
      break;

    case SLICE_MODE_RGB:
      bdata = (unsigned char *)array + 3 * (nx * (ny - 1) + ix);
      for (iy = 0; iy < ny; iy++) {
        *ubray++ = bdata[0];
        *ubray++ = bdata[1];
        *ubray++ = bdata[2];
        bdata -= 3 * nx;
      }
      break;
    }
  }
}

void ProcRotateLeft(void *array, int type, int nx, int ny, short int *brray)
{
  int ix, iy;
  unsigned char *bdata;
  short int *sdata;
  float *fdata;
  float *fbray = (float *)brray;
  unsigned char *ubray = (unsigned char *)brray;

  for (ix = nx - 1; ix >= 0; ix--) {
    switch (type) {
    case BYTE:
      bdata = (unsigned char *)array + ix;
      for (iy = 0; iy < ny; iy++) {
        *brray++ = *bdata;
        bdata += nx;
      }
      break;
      
    case UNSIGNED_SHORT:
    case SIGNED_SHORT:
      sdata = (short int *)array + ix;
      for (iy = 0; iy < ny; iy++) {
        *brray++ = *sdata;
        sdata += nx;
      }
      break;

    case FLOAT:
      fdata = (float *)array + ix;
      for (iy = 0; iy < ny; iy++) {
        *fbray++ = *fdata;
        fdata += nx;
      }
      break;

    case SLICE_MODE_RGB:
      bdata = (unsigned char *)array + 3 * ix;
      for (iy = 0; iy < ny; iy++) {
        *ubray++ = bdata[0];
        *ubray++ = bdata[1];
        *ubray++ = bdata[2];
        bdata += 3 * nx;
      }
      break;
    }
  }
}

#define SHIFT_IN_PLACE(in, out, arr, typ) \
  in = &arr[yin * nx + xCopyStrt];   \
  out = &arr[yout * nx + xCopyStrt + shiftX];   \
  if (xCopyDir > 0) {   \
    for (x = 0; x < numXcopy; x++)   \
      *out++ = *in++;    \
  } else {    \
    for (x = 0; x < numXcopy; x++)   \
      *out-- = *in--;     \
  }    \
  for (x = xFillStrt; x < xFillEnd; x++)     \
    arr[yout * nx + x] = (typ)fill; 


// Shift an integer, float or byte buffer in place by the given amounts
void ProcShiftInPlace(short int *array, int type, int nx, int ny, int shiftX, int shiftY,
                      float fill)
{
  int yFillStrt, yFillEnd, yCopyStrt, yCopyEnd, yCopyDir;
  int xFillStrt, xFillEnd, xCopyStrt, numXcopy, xCopyDir;
  int x, yin, yout;
  short int *inp, *outp;
  unsigned char *inbp, *outbp, *brray;
  unsigned short *usArray = (unsigned short *)array;
  float *infp, *outfp;
  unsigned short *inusp, *outusp;
  float *fArray = (float *)array;
  brray = (unsigned char *)array;

  if (shiftY > 0) {
    yFillStrt = 0;
    yFillEnd = shiftY;
    yCopyDir = -1;
    yCopyStrt = ny - 1 - shiftY;
    yCopyEnd = 0;
  } else {
    yFillStrt = ny + shiftY;
    yFillEnd = ny;
    yCopyDir = 1;
    yCopyStrt = -shiftY;
    yCopyEnd = ny - 1;
  }

  xCopyDir = 1;
  if (shiftX > 0) {
    xFillStrt = 0;
    xFillEnd = shiftX;
    xCopyStrt = 0;
    numXcopy = nx - shiftX;
    if (!shiftY) {
      xCopyDir = -1;
      xCopyStrt = nx - 1 - shiftX;
    }
  } else {
    xFillStrt = nx + shiftX;
    xFillEnd = nx;
    xCopyStrt = -shiftX;
    numXcopy = nx + shiftX;
  }

  for (yin = yCopyStrt; yCopyDir * (yin - yCopyEnd) <= 0; yin += yCopyDir) {
    yout = yin + shiftY;

    if (type == BYTE) {
      SHIFT_IN_PLACE(inbp, outbp, brray, unsigned char);
    } else if (type == FLOAT) {
      SHIFT_IN_PLACE(infp, outfp, fArray, float);
    } else if (type == UNSIGNED_SHORT) {
      SHIFT_IN_PLACE(inusp, outusp, usArray, unsigned short);
    } else {
      SHIFT_IN_PLACE(inp, outp, array, short);
    }
  }

  // Fill lines in Y
  for (yout = yFillStrt; yout < yFillEnd; yout++)
    if (type == BYTE)
      for (x = 0; x < nx; x++)
        brray[yout * nx + x] = (unsigned char)fill;
    else if (type == FLOAT)
      for (x = 0; x < nx; x++)
        fArray[yout * nx + x] = fill;
    else if (type == UNSIGNED_SHORT)
      for (x = 0; x < nx; x++)
        usArray[yout * nx + x] = (unsigned short)fill;
    else
      for (x = 0; x < nx; x++)
        array[yout * nx + x] = (short)fill;
}

// Perform any combination of rotation and Y flipping for a short array: 
// operation = 0-3 for rotation by 90 * operation, plus 4 for flipping around Y axis 
// before rotation or 8 for flipping around Y after.  Also invert contrast between
// existing min and max if invertCon is nonzero
void ProcRotateFlip(short int *array, int type, int nx, int ny, int operation, 
                    int invertCon, short int *brray, int *nxout, int *nyout)
{
  rotateFlipImage(array, type, nx, ny, operation, 1, 0, invertCon, brray, nxout, nyout,0);
}

// Does a simple flip in Y in place in the given array
void ProcSimpleFlipY(void *array, int rowBytes, int height)
{
  int y;
  int mcol = height / 2;
  unsigned char *data = (unsigned char *)array;
  unsigned char *tmpRowData = new unsigned char[rowBytes];

  unsigned char *from, *to;
  for (y = 0; y < mcol; y++) {
    from = data + rowBytes * y;
    to = data + rowBytes * (height - y - 1);

    memcpy(tmpRowData, to, rowBytes);
    memcpy(to, from, rowBytes);
    memcpy(from, tmpRowData, rowBytes);
  }
  delete tmpRowData;

}

void ProcFFT(void *array, int type, int nx, int ny, int binning, float *fftarray, 
       short int *brray, int nPadSize, int nFinalSize)
{
  float fracTaper = 0.025f;
  double logScale = 5.;   // Higher numbers no longer make the background brighter
  void *data = array;
  int dataType = type;
  int numThreads, maxThreads = 6;
  int nxTaper, nyTaper;
#ifdef USE_FFTW2
     rfftwnd_plan plan;
     fftw_complex dumcomp;
#endif
  double val, cenMax;  
  double scale, sum;
  int i, ixbase = 0, iyin, iyout, iyinBase;
  short int *sdata = NULL, *sdata2 = NULL;
  int cenLim;
  double wallStart = wallTime();

  // Bin the array, replace data and dataType and sizes
  if (binning > 1) {
    XCorrBinByN(array, type, nx, ny, binning, brray);
    data = brray;
    dataType = (type == BYTE || type == SLICE_MODE_RGB) ? SIGNED_SHORT : type;
    nx /= binning;
    ny /= binning;
  }

  // Get the tapered, padded real array
  nxTaper = nx * fracTaper;
  nyTaper = ny * fracTaper;
  XCorrTaperInPad(data, dataType, nx, 0, nx - 1, 0, ny - 1, fftarray, nPadSize + 2,
    nPadSize, nPadSize, nxTaper, nyTaper);
  //PrintfToLog("Taperpad time %.1f msec", (wallTime() - wallStart) * 1000.);

  // Take the FFT with chosen code
  wallStart = wallTime();
#ifndef USE_FFTW2
    todfftc(fftarray, nPadSize, nPadSize, 0);
#else
  plan = rfftw2d_create_plan(nPadSize, nPadSize, FFTW_FORWARD, FFTW_IN_PLACE);
  rfftwnd_one_real_to_complex(plan, fftarray, &dumcomp);
  rfftwnd_destroy_plan(plan);
#endif        
  //PrintfToLog("Size %d  threads %d  time %.1f msec", nPadSize, numOMPthreads(16), (wallTime() - wallStart) * 1000.);
  wallStart = wallTime();

  // Determine magnitudes around center, ignore 0,0
  cenMax = 0.;
  cenLim = nPadSize / 10;
  if (cenLim < 5)
    cenLim = 5;
  if (cenLim >= nPadSize / 2)
    cenLim = nPadSize / 2 -1;
  for (i = 0; i < cenLim; i++) {
    for (iyin = 0; iyin < cenLim; iyin++) {
      if (!i && !iyin)
        continue;

      val = ProcFFTMagnitude(fftarray, nPadSize, nPadSize, i, iyin);
      if (cenMax < val)
        cenMax = val;
      val = ProcFFTMagnitude(fftarray, nPadSize, nPadSize, i, nPadSize - 1 - iyin);
      if (cenMax < val)
        cenMax = val;
    }
  }

  // Determine inside log scaling from mean of border
  sum = 0.;
  for (iyin = 0; iyin < nPadSize; iyin++)
    sum += ProcFFTMagnitude(fftarray, nPadSize, nPadSize, nPadSize / 2, iyin);
  logScale /= sum / nPadSize;

  scale = 32000. / log(logScale * cenMax + 1.);

  // Fill lower right of output array from top of FFT then wrap into bottom
  // of FFT to fill upper right
  // This switches to 2 threads at 1668, efficiency on 2-core system was 69% for 1584
  numThreads = B3DNINT(0.0009 * nFinalSize);
  B3DCLAMP(numThreads, 1, maxThreads);
  numThreads = numOMPthreads(numThreads);
  iyinBase = nPadSize / 2 + (nPadSize - nFinalSize) / 2;
#pragma omp parallel for num_threads(numThreads) \
  shared(nFinalSize, iyinBase, nPadSize, fftarray, brray, logScale) \
  private(iyout, iyin, ixbase, sdata, i, val)
  for (iyout = 0; iyout < nFinalSize; iyout++) {
    iyin = (iyout + iyinBase) % nPadSize;
    ixbase = iyin * (nPadSize + 2);
    sdata = brray + iyout * nFinalSize + nFinalSize / 2;
    for (i = ixbase; i < ixbase + nFinalSize; i += 2) {
      val = sqrt((double)(fftarray[i] * fftarray[i] + 
              fftarray[i + 1] * fftarray[i + 1]));
      *sdata++ = (int)(scale * log(logScale * val + 1.));
    }

    // Stick the next value (which could be the extra pixel) on the left edge
    val = sqrt((double)(fftarray[i] * fftarray[i] + 
            fftarray[i + 1] * fftarray[i + 1]));
    brray[iyout * nFinalSize] = (int)(scale * log(logScale * val + 1.));
  }

  // Mirror the lines in Y around the center
#pragma omp parallel for num_threads(numThreads) \
  shared(nFinalSize) \
  private(iyout, iyin, sdata, sdata2, i)
  for (iyout = 0; iyout < nFinalSize; iyout++) {
    iyin = iyout ? nFinalSize - iyout : nFinalSize - 1;
    sdata = brray + iyin * nFinalSize + nFinalSize / 2 + 1;
    sdata2 = brray + iyout * nFinalSize + nFinalSize / 2 - 1;
    for (i = 0; i < nFinalSize / 2 - 1; i++)
      *sdata2-- = *sdata++;
  }
  brray[nFinalSize * nFinalSize / 2 + nFinalSize / 2] = 32000;
  //PrintfToLog("Scale/mirror time %.1f msec", (wallTime() - wallStart) * 1000.);
}

double ProcFFTMagnitude(float *array, int nx, int ny, int ix, int iy)
{
  int i = ix * 2 + iy * (nx + 2);
  return sqrt((double)(array[i] * array[i] + array[i + 1] * array[i + 1]));
}

// Compute a rotationally averaged power spectrum
void ProcRotAvFFT(void *array, int type, int nxdim, int ix0, int ix1, int iy0, int iy1,
                  float *fftarray, int nxpad, int nypad, float *rotav, int *ninRing, 
                  int numRotPts)
{
  float fracTaper = 0.025f;
  int nxTaper, nyTaper, i, ind, index, indf, ix, iy, nx, ny;
  double y, y2, x, scale = 1.;
  float delx, dely, delRing, s, fmag;
#ifdef USE_FFTW2
  rfftwnd_plan plan;
  fftw_complex dumcomp;
#endif

  // Get the tapered, padded real array
  nx = ix1 + 1 - ix0;
  ny = iy1 + 1 - iy0;
  nxTaper = nx * fracTaper;
  nyTaper = ny * fracTaper;
  for (ix = 0; ix < nypad*(nxpad+2); ix++)
    fftarray[ix] = 0.;
  XCorrTaperInPad(array, type, nxdim, ix0, ix1, iy0, iy1, fftarray, nxpad + 2,
    nxpad, nypad, nxTaper, nyTaper);

  // Take the FFT with chosen code
#ifndef USE_FFTW2
  todfftc(fftarray, nxpad, nypad, 0);
#else
  plan = rfftw2d_create_plan(nypad, nxpad, FFTW_FORWARD, FFTW_IN_PLACE);
  rfftwnd_one_real_to_complex(plan, fftarray, &dumcomp);
  rfftwnd_destroy_plan(plan);
  scale = sqrt((double) nxpad * nypad);
#endif        

  // Compute rotationally averaged power spectrum as in fortran program rotavfft
  delRing = 0.5 / numRotPts;
  delx = 1. / nxpad;
  dely = 1. / nypad;
  for (i = 0; i < numRotPts; i++) {
    rotav[i] = 0.;
    ninRing[i] = 0;
  }
  index = 0;
  for (iy = 0; iy < nypad; iy++) {
    y = iy * dely;
    if (y > 0.5)
      y = 1. - y;
    y2 = y * y;
    x = 0.;
    for (ix = 0; ix <= nxpad / 2; ix++) {
      ind = index + ix;
      s = sqrt(x * x + y2);
      indf = s / delRing;
      if (indf < numRotPts && (ix || iy)) {
        fmag = sqrt((double)(fftarray[2*ind] * fftarray[2*ind] + 
          fftarray[2*ind + 1] * fftarray[2*ind + 1]));
        rotav[indf] += fmag;
        ninRing[indf]++;
      }
      x += delx;
    }
    index += (nxpad + 2) / 2;
  }
  for (i = 0; i < numRotPts; i++) {
    rotav[i] /= scale * ninRing[i];
    //if (!(i%5))SEMTrace('1', "%d %10.0f", i, rotav[i]);
  }
}


int ProcTrimCircle(void *array, int type, int nx, int ny, float cenFrac, int testSize,
          int delTrim, int minTrimX, int minTrimY, float passRatio, bool centered,
          int &trimX, int &trimY, int &trimXright, int &trimYtop)
{
  int ix0, ix1, iy0, iy1, minArea, tryX, tryY, minWidth, newArea, xind, yind;
  int xtry[2], ytry[2], xlim, ylim, xcen, ycen, halfCenX, halfCenY;
  int maxDelX, maxDelY, tryYtop, tryXright, delX, minDelX, useXind, useYind;
  float fxcen, fycen;
  double critMean;

  halfCenX = cenFrac * nx / 2.;
  halfCenY = cenFrac * ny / 2.;
  xcen = nx / 2;
  ycen = ny / 2;

  // If non-centered is allowed, get the centroid and set the center from that
  if (!centered) {
    ProcCentroid(array, type, nx, ny, 0, nx - 1, 0, ny - 1, 0., fxcen, fycen);
    xcen = fxcen;
    ycen = fycen;

    // Adjust the center so at least one eval happens
    xlim = halfCenX + minTrimX + 1;
    xcen = B3DMAX(xlim, B3DMIN(nx - 1 - xlim, xcen));
    ylim = halfCenY + minTrimY + 1;
    ycen = B3DMAX(ylim, B3DMIN(ny - 1 - ylim, ycen));
  }

  // Get "center" mean and criterion mean for corners
  ix0 = xcen - halfCenX;
  ix1 = xcen + halfCenX;
  iy0 = ycen - halfCenY;
  iy1 = ycen + halfCenY;
  critMean = passRatio * ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1);

  // Examine progressively smaller "squares" until one is found that passes
  minTrimX = B3DMAX(minTrimX, testSize);
  minTrimY = B3DMAX(minTrimY, testSize);
  trimX = trimXright = minTrimX;
  trimY = trimYtop = minTrimY;
  while (trimX <= ix0 && trimY <= iy0 && trimXright < nx - ix1 && trimYtop <  ny - iy1) {
    if (ProcEvaluateTrim(array, type, nx, ny, testSize, trimX, trimY, trimXright, trimYtop, 
      critMean))
      break;

    // Add trim to one side only for non-centered trim if there is a big enough imbalance
    // between the distances from center to edge on the two side
    // Trim by double there, or quadruple if the imbalance is big enough
    if (!centered && xcen - (trimX + 3 * delTrim / 2) > (nx - trimXright) - xcen) {
      trimX += delTrim * (xcen - trimX > 2 * ((nx - trimXright) - xcen) ? 4 : 2);
    } else if (!centered && xcen - trimX < (nx - (trimXright + 3 * delTrim / 2)) - xcen) {
      trimXright += delTrim * ((2 * (xcen - trimX) < (nx - trimXright) - xcen) ? 4 : 2);
    } else {
      trimX += delTrim;
      trimXright += delTrim;
    }
    if (!centered && ycen - (trimY + 3 * delTrim / 2) > (ny - trimYtop) - ycen) {
      trimY += delTrim * (ycen - trimY > 2 * ((ny - trimYtop) - ycen) ? 4 : 2);
    } else if (!centered && ycen - trimY < (ny - (trimYtop + 3 * delTrim / 2)) - ycen) {
      trimYtop += delTrim * ((2 * (ycen - trimY) < (ny - trimYtop) - ycen) ? 4 : 2);
    } else {
      trimY += delTrim;
      trimYtop += delTrim;
    }
  }

  // If it never passed, give up and set to minimum trims
  if (trimX > ix0 || trimY > iy0 || trimXright >= nx - ix1 || trimYtop >=  ny - iy1) {
    trimX = trimXright = minTrimX;
    trimY = trimYtop = minTrimY;
    return 0;
  }

  // Now operate with symmetric changes in these trimming values
  minArea = (nx - (trimX + trimXright)) * (ny - (trimY + trimYtop));
  maxDelY = B3DMIN(trimY, trimYtop) - minTrimY;
  tryY = trimY - maxDelY;
  tryYtop = trimYtop - maxDelY;
  while (tryY < iy0 && tryYtop < ny - iy1) {

    // For each possible trim in Y, start at a minimum width determined by current
    // minimum area, and try progressively smaller trims in X until it fails
    minWidth = minArea / (ny - (tryY + tryYtop));
    if (minWidth < testSize)
      minWidth = testSize;
    maxDelX = B3DMIN(trimX, trimXright) - minTrimX;
    minDelX = -B3DMIN(ix0 - trimX, (nx - 1 - trimXright) - ix1);

    delX = ((nx - (trimX + trimXright)) - minWidth) / 2;
    delX = B3DMAX(minDelX, B3DMIN(maxDelX, delX));
    tryX = trimX - delX;
    tryXright = trimXright - delX;

    // Stop when the biggest trim in X is too small
    if (tryX < minTrimX || tryXright < minTrimX)
      break;

    while (tryX >= minTrimX && tryXright >= minTrimX) {
      if (!ProcEvaluateTrim(array, type, nx, ny, testSize, tryX, tryY, tryXright, tryYtop, 
        critMean))
        break;

      // If this trim passes, make sure area is larger and replace current values
      newArea = (nx - (tryX + tryXright)) * (ny - (tryY + tryYtop));
      if (newArea > minArea) {
        trimX = tryX;
        trimXright = tryXright;
        trimY = tryY;
        trimYtop = tryYtop;
        minArea = newArea;
      }
      tryX -= delTrim;
      tryXright -= delTrim;
    }
    tryY += delTrim;
    tryYtop += delTrim;
  }
  if (centered)
    return 1;

  // If non-centered trimming is allowed, first find two directions that can be expanded
  xind = yind = -1;
  if (trimX - delTrim >= minTrimX && ProcEvaluateTrim(array, type, nx, ny, testSize, 
    trimX - delTrim, trimY, trimXright, trimYtop, critMean))
    xind = 0;
  if (trimXright - delTrim >= minTrimX && ProcEvaluateTrim(array, type, nx, ny, testSize, 
    trimX, trimY, trimXright - delTrim, trimYtop, critMean))
    xind = 1;
  if (trimY - delTrim >= minTrimY && ProcEvaluateTrim(array, type, nx, ny, testSize, 
    trimX, trimY - delTrim, trimXright, trimYtop, critMean))
    yind = 0;
  if (trimYtop - delTrim >= minTrimY && ProcEvaluateTrim(array, type, nx, ny, testSize, 
    trimX, trimY, trimXright, trimYtop - delTrim, critMean))
    yind = 1;

  // Set up to loop on trims in Y if any movement works
  xtry[0] = trimX;
  xtry[1] = trimXright;
  ytry[0] = trimY;
  ytry[1] = trimYtop;
  useXind = B3DMAX(0, xind);
  useYind = B3DMAX(0, yind);
  xlim = xtry[useXind];
  ylim = ytry[useYind] + 1;
  if (yind >= 0) {
    ytry[yind] = minTrimY;
    ylim = yind ? ny - iy1 : iy0;
  }
  while (ytry[useYind] < ylim) {

    // Now set up to loop on X if possible
    if (xind >= 0) {

      // Again, start at minimum width for this Y trimming to give maximum X trim, and try
      // smaller trims in X
      minWidth = minArea / (ny - ytry[0] - ytry[1]);
      minWidth = B3DMAX(minWidth, testSize);
      xtry[xind] = nx - minWidth - xtry[1 - xind];
      xlim = minTrimX;

      // Stop when the biggest trim in X is too small
      if (xtry[xind] < minTrimX)
        break;
    } else
      xtry[0] = trimX;

    while (xtry[useXind] >= xlim) {
      if (!ProcEvaluateTrim(array, type, nx, ny, testSize, xtry[0], ytry[0], xtry[1], 
        ytry[1], critMean))
        break;

      // If this trim passes, make sure area is larger and replace current values
      newArea = (nx - xtry[0] - xtry[1]) * (ny - ytry[0] - ytry[1]);
      if (newArea > minArea) {
        trimX = xtry[0];
        trimXright = xtry[1];
        trimY = ytry[0];
        trimYtop = ytry[1];
        minArea = newArea;
      }
      xtry[useXind] -= delTrim;
    }
    ytry[useYind] += delTrim;
  }
  return 1;
}

// Returns true if all four test areas outside corners of the trimmed area are above
// the criterion mean
int ProcEvaluateTrim(void *array, int type, int nx, int ny, int testSize, int trimXleft,
            int trimYbot, int trimXright, int trimYtop, double critMean)
{
  int ix0, ix1, iy0, iy1;
  ix0 = trimXleft - testSize;
  ix1 = trimXleft - 1;
  iy0 = trimYbot - testSize;
  iy1 = trimYbot - 1;
  if (ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1) < critMean)
    return 0;

  ix0 = nx - trimXright;
  ix1 = ix0 + testSize - 1;
  if (ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1) < critMean)
    return 0;

  iy0 = ny - trimYtop;
  iy1 = iy0 + testSize - 1;
  if (ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1) < critMean)
    return 0;

  ix0 = trimXleft - testSize;
  ix1 = trimXleft - 1;
  if (ProcImageMean(array, type, nx, ny, ix0, ix1, iy0, iy1) < critMean)
    return 0;
  return 1;
}

#define PFCE_BOX_SUM(tnam, data) \
  case tnam:    \
  for (i = 0; i < numInc; i++) {  \
    ix = ixc + idx[i];  \
    iy = iyc + idy[i];  \
    if (ix >= border && ix <= xhi && iy >= border && iy <= yhi) {  \
      sum += data[ix + iy * nx];  \
      npix++;  \
    }   \
  }   \
  break;


// Find edges of beam
int ProcFindCircleEdges(void *array, int type, int nx, int ny, float xcen, float ycen,
                        int border, float angleInc, int boxLen, int boxWidth, 
                        float outCrit, float edgeCrit, float *xx, float *yy, 
                        float *angle, float *grad)
{
  float xcorn[4], ycorn[4];
  int *idx = new int[2 * boxLen * boxWidth];
  int *idy = new int[2 * boxLen * boxWidth];
  int numPts = 0;
  int numAng = (int)(360. / angleInc);
  int numInc, xhi, yhi, ix, iy, ixc, iyc, lastxc, lastyc, npix, ixst, ixnd, iyst, iynd;
  int iray, ixray, iyray, i, rayst, gotCross;
  double rayAng, xray, yray, rayFac, sum, boxMean, frac, lastMean, mean2, mean3;
  float xmin, xmax, ymin, ymax, wcos, wsin, lcos, lsin;
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  
  xhi = nx - 1 - border;
  yhi = ny - 1 - border;

  // Loop on all angles
  for (rayAng = angleInc - 180.; rayAng <= 180.; rayAng += angleInc) {
    
    // Make up the box increments
    numInc = 0;
    gotCross =0;
    lastMean = mean2 = mean3 = -1.e10;
    wcos = (float)(0.5 * boxWidth * cos(DTOR * rayAng));
    wsin = (float)(0.5 * boxWidth * sin(DTOR * rayAng));
    lcos = (float)(0.5 * boxLen * cos(DTOR * (rayAng + 90)));
    lsin = (float)(0.5 * boxLen * sin(DTOR * (rayAng + 90)));
    xcorn[0] = -wcos - lcos;
    ycorn[0] = -wsin - lsin;
    xcorn[1] =  wcos - lcos;
    ycorn[1] =  wsin - lsin;
    xcorn[2] =  wcos + lcos;
    ycorn[2] =  wsin + lsin;
    xcorn[3] = -wcos + lcos;
    ycorn[3] = -wsin + lsin;
    xmin = xmax = xcorn[0];
    ymin = ymax = ycorn[0];
    for (i = 1; i < 4; i++) {
      xmin = B3DMIN(xmin, xcorn[i]);
      ymin = B3DMIN(ymin, ycorn[i]);
      xmax = B3DMAX(xmax, xcorn[i]);
      ymax = B3DMAX(ymax, ycorn[i]);
    }
    ixst = (int)(xmin - 1.5);
    ixnd = (int)(xmax + 1.5);
    iyst = (int)(ymin - 1.5);
    iynd = (int)(ymax + 1.5);

    for (iy = iyst; iy <= iynd; iy++) {
      for (ix = ixst; ix <= ixnd; ix++) {
        if (InsideContour(xcorn, ycorn, 4, (float)ix, (float)iy)) {
          idx[numInc] = ix;
          idy[numInc++] = iy;
        }
      }
    }

    // Get the basic pixel increment along the ray
    xray = cos(DTOR * rayAng);
    yray = sin(DTOR * rayAng);
    rayFac = B3DMAX(fabs(xray), fabs(yray));
    xray /= rayFac;
    yray /= rayFac;

    // Find the intersection point to start doing boxes at
    ixray = iyray = B3DMAX(nx, ny);
    if (xray > .01)
      ixray = (int)(xhi - boxWidth / 2. - xcen) / xray;
    else if (xray < -0.01)
      ixray = (int)(border + boxWidth / 2. - xcen) / xray;
    if (yray > .01)
      iyray = (int)(yhi - boxWidth / 2. - ycen) / yray;
    else if (yray < -0.01)
      iyray = (int)(border + boxWidth / 2. - ycen) / yray;
    rayst = B3DMIN(ixray, iyray);
 

    // Loop from there back to the center
    for (iray = rayst; iray > 1; iray--) {
      ixc = (int)(xcen + iray * xray);
      iyc = (int)(ycen + iray * yray);

      // Get the box mean
      sum = 0.;
      npix = 0;
      switch (type) {
        PFCE_BOX_SUM(SIGNED_SHORT, sdata);
        PFCE_BOX_SUM(UNSIGNED_SHORT, usdata);
        PFCE_BOX_SUM(FLOAT, fdata);
      }
      boxMean = sum / npix;

      // If first point and it does not meet outside criterion, done with ray
      if (iray == rayst && boxMean > outCrit)
        break;

      // Otherwise, if above the edge criterion, we have the edge; interpolate to position
      if (boxMean > edgeCrit && !gotCross) {
        frac = (edgeCrit - lastMean) / (boxMean - lastMean);
        xx[numPts] = (float)(lastxc + frac * (ixc - lastxc));
        yy[numPts] = (float)(lastyc + frac * (iyc - lastyc));
        angle[numPts] = rayAng;
        gotCross = 1;
      }

      // Go through one more time to get gradient across 3 steps
      if (gotCross == 2) {
        if (mean3 > -1.e9)
          grad[numPts++] = boxMean - mean3;
        break;
      } else if (gotCross)
        gotCross = 2;

      mean3 = mean2;
      mean2 = lastMean;
      lastMean = boxMean;
      lastxc = ixc;
      lastyc = iyc;
    }
  }
  delete [] idx;
  delete [] idy;
  return numPts;
}


// Perform dark subtraction on an image, interpolating between two dark references
// if ref2 is non-NULL.
int ProcDarkSubtract(void *image, int type, int nx, int ny, short int *ref1, 
  double exp1, short int *ref2, double exp2, double exp, int darkScale)
{
  int i, f1, f2;
  short int *sdata;
  unsigned short int *usdata;
  unsigned short int *usref1 = (unsigned short int *)ref1;
  unsigned short int *usref2 = (unsigned short int *)ref2;
  unsigned short int usrval;
  float *fdata;
  float *fref = (float *)ref1;
  short int srval;
  int darkBits = 12;
  int darkFac = 1 << darkBits;
  int roundFac = darkFac / 2;
  if (ref2) {
    f1 = (int)(darkFac * (exp2 - exp) / (exp2 - exp1) + 0.5);
    f2 = darkFac - f1;
  }

  switch (type) {
  case SIGNED_SHORT:
    sdata = (short int *)image;
    for (i = 0; i < nx * ny; i++) {
      srval = darkScale * (ref2 ? (short int)((f1 * *ref1++ + f2 * *ref2++ + roundFac) 
        >> darkBits) : *ref1++);
      *sdata++ -= srval;
    }
    break;

  case UNSIGNED_SHORT:
    usdata = (unsigned short int *)image;
    for (i = 0; i < nx * ny; i++) {
      usrval = darkScale * (ref2 ? (unsigned short int)
        ((f1 * *usref1++ + f2 * *usref2++ + roundFac) >> darkBits) : *usref1++);
      *usdata = *usdata > usrval ? *usdata - usrval : 0;
      usdata++; 
    }
    break;

  case FLOAT:
   fdata = (float *)image;
   for (i = 0; i < nx * ny; i++)
      fdata[i] -= darkScale * fref[i];
   break;

  default:
    return 1;
  }
  return 0;
}

#define SCALED_DARK_VAL(d1, d2) rval = \
  darkScale * (dark2 ? ((f1 * d1[ix] + f2 * d2[ix] + roundFac) >> darkBits) : d1[ix]);
   

// Perform gain normalization using either a float or scaled integer gain reference
// and interpolating between two dark references.
// Old Speed note: dividing by a fixed number (10000) was fast; dividing by an int passed
// in was 4 times slower.  Shifting by an int passed in is fast.
// On Pentium M scaling by a float is 9 times slower.
// New note on adding interpolation: Validated single-reference case against old code
// and compared speeds, on Core 2 Duo it is 5 ms slower for small images and somewhat
// faster for large images; on Pentium 4 is faster all around (?).
int ProcGainNormalize(void *image, int type, int nxFull, int top, int left, int bottom,
  int right, short int *dark1, double exp1, short int *dark2, double exp2, double exp, 
  int darkScale, int darkByteSize, void *gainRef, int gainBytes, int gainBits)
{
  float *gainp = NULL;
  short int *sdata = NULL, *sdark1 = NULL, *sdark2 = NULL;
  unsigned short int *usdata = NULL;
  unsigned short int *usgain = NULL;
  unsigned short int *usdark1 = (unsigned short int *)dark1;
  unsigned short int *usdark2 = (unsigned short int *)dark2;
  float *fdark = (float *)dark1;
  float *fdata = NULL;
  int iy, ix = 0, itmp = 0, rval = 0, f1, f2, dataBase = 0, gainBase = 0, error = 0;
  int nxImage = right - left;
  int darkBits = 12;
  int darkFac = 1 << darkBits;
  int roundFac = darkFac / 2;
  int maxThreads = 6, numThreads;
  if (dark2) {
    f1 = (int)(darkFac * (exp2 - exp) / (exp2 - exp1) + 0.5);
    f2 = darkFac - f1;
  }
  /*static double wallCum = 0.;
  static int round = 0;
  double wallStart = wallTime();*/

  // Thread selection, not much evaluation went into this
  numThreads = B3DNINT(sqrt((double)nxImage * (bottom - top)) / 1000.);
  B3DCLAMP(numThreads, 1, maxThreads);
  numThreads = numOMPthreads(numThreads);

  // Loop on lines in Y
#pragma omp parallel for num_threads(numThreads) default(none)  \
shared(top, bottom, nxImage, nxFull, type, gainBytes, gainRef, dark1, dark2, image, \
  darkScale, f1, f2, darkBits, darkByteSize, roundFac, gainBits, left, error) \
private(iy, ix, gainBase, dataBase, gainp, sdark1, sdark2, usdark1, usdark2, usgain, sdata, \
  usdata, itmp, rval, fdark, fdata)
  for (iy = top; iy < bottom; iy++) {
    gainBase = iy * nxFull + left;
    dataBase = (iy - top) * nxImage;

    // All types need to put result in a long integer and test for saturation
    switch (type) {
    case SIGNED_SHORT:
      if (gainBytes == 4) {

        // signed image with float gain reference - use it directly
        gainp = (float *)gainRef + gainBase;
        sdark1 = (short *)dark1 + dataBase;
        sdark2 = (short *)dark2 + dataBase;
        sdata = (short int *)image + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          SCALED_DARK_VAL(sdark1, sdark2);
          itmp = (int)((sdata[ix] - rval) * gainp[ix]);
          itmp = itmp < 32767 ? itmp : 32767;
          sdata[ix] = (short int)itmp;
        }
      } else {

        // signed image with scaled integer gain reference
        usgain = (unsigned short int *)gainRef + gainBase;
        sdark1 = (short *)dark1 + dataBase;
        sdark2 = (short *)dark2 + dataBase;
        sdata = (short int *)image + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          SCALED_DARK_VAL(sdark1, sdark2);
          itmp = ((int)(sdata[ix] - rval) * usgain[ix]) >> gainBits;
          itmp = itmp < 32767 ? itmp : 32767;
          sdata[ix] = (short int)itmp;
        }
      }
      break;

    case UNSIGNED_SHORT:
      if (gainBytes == 4) {

        // Unsigned image with floating gain reference - test for image > dark
        gainp = (float *)gainRef + gainBase;
        usdark1 = (unsigned short *)dark1 + dataBase;
        usdark2 = (unsigned short *)dark2 + dataBase;
        usdata = (unsigned short int *)image + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          SCALED_DARK_VAL(usdark1, usdark2);
          itmp = usdata[ix] > rval ? (int)((usdata[ix] - rval) * gainp[ix]) : 0;
          itmp = itmp < 65535 ? itmp : 65535;
          usdata[ix] = (unsigned short int)itmp;
        }
      } else {

        // Unsigned image with scaled integer gain reference
        usgain = (unsigned short int *)gainRef + gainBase;
        usdark1 = (unsigned short *)dark1 + dataBase;
        usdark2 = (unsigned short *)dark2 + dataBase;
        usdata = (unsigned short int *)image + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          SCALED_DARK_VAL(usdark1, usdark2);
          itmp = usdata[ix] > rval ?
            ((int)(usdata[ix] - rval) * usgain[ix]) >> gainBits : 0;
          itmp = itmp < 65535 ? itmp : 65535;
          usdata[ix] = (unsigned short int)itmp;
        }
      }
      break;

    case FLOAT:


      // float image with float references, but dark may be integer or float
      gainp = (float *)gainRef + gainBase;
      fdata = (float *)image + dataBase;
      if (darkByteSize == 4) {
        fdark = (float *)dark1 + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          fdata[ix] = (fdata[ix] - darkScale * fdark[ix]) * gainp[ix];
        }
      } else {
        sdark1 = (short *)dark1 + dataBase;
        for (ix = 0; ix < nxImage; ix++) {
          fdata[ix] = (fdata[ix] - darkScale * sdark1[ix]) * gainp[ix];
        }
      }
      break;

    default:
      error = 1;
    }
  }
  /*wallCum += wallTime() - wallStart;
  round++;
  if (round == 40) {
    PrintfToLog("time for 40 = %.3f", wallCum);
    round = 0;
    wallCum = 0.;
  }*/
  return error;
}

// Convert a gain reference to unsigned shorts
int ProcConvertGainRef(float *fGain, unsigned short *usGain, int arrSize, int scaledMax,
             int minBits, int *bits)
{
  int i;
  float fMax = 0.;
  int factor;

  if (!usGain || !scaledMax)
    return 1;

  // Find maximum of float array
  for (i = 0; i < arrSize; i++) {
    if (fMax < fGain[i])
      fMax = fGain[i];
  }

  // get factor, return error if too small
  *bits = (int)(log((double)(scaledMax / fMax)) / log(2.));
  
  if (*bits < minBits)
    return 2;

  factor = 1 << *bits;

  // Scale the data
  for (i = 0; i < arrSize; i++)
    *usGain++ = (unsigned int)(factor * *fGain++);
  return 0;
}

#define REPLACE_ON_LINE(data) \
  for (ix = 0; ix < nx; ix++) { \
    if (data[ix + ixbase] > intcrit) { \
      for (critLoop = 0; critLoop < critIterations; critLoop++) { \
        if (data[ix + ixbase] > (int)(mean +  (1. + critLoop * critFac) * crit)) { \
          ninPatch = ProcReplacePixels(array, type, nx, ny, nHotCol, hotCol, nHotPixel, \
            hotX, hotY, maxPixel, minDist, maxDist, \
            mean +  (1. + critLoop * critFac) * crit, ix, iy, listX, listY); \
          if (ninPatch > 0) { \
            if (totPatches < replacedSize) { \
              replacedX[totPatches] = listX[ninPatch]; \
              replacedY[totPatches] = listY[ninPatch]; \
            } \
            totPatches++; \
            totReplaced += ninPatch; \
          } \
          if (ninPatch >= 0) \
            break; \
        } \
      } \
      if (ninPatch < 0) \
        (*nSkipped)++; \
      else if (critLoop) \
        (*nTruncated)++; \
    } \
  }

int ProcRemoveXRays(void *array, int type, int nx, int ny, int nHotCol, int *hotCol,
          int nHotPixel, int *hotX, int *hotY, int maxPixel, float minDist, float maxDist, 
          float absCrit, float sdCrit, int eitherBoth, int critIterations, float critFac,
          float *sdev, int *nReplaced, int *nSkipped, int *nTruncated, int *replacedX,
            int *replacedY, int replacedSize)
{
  double dsum, dsumsq, mean, sd, crit, val, diff;
  int nsum, nxSample, nySample, intcrit, ninPatch, nxUse, nyUse;
  int ixSample, dxSample, iyStart, iySample, dySample, i, j, ix, iy, ixbase;
  int totPatches = 0;
  int totReplaced = 0;
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  int hot, critLoop;
  int sampleIndent = 4;
  int nSample = 100;
  int *listX = new int[maxPixel + 10];
  int *listY = new int[maxPixel + 10];

  *nReplaced = 0;
  *nSkipped = 0;
  *nTruncated = 0;
  *sdev = 0.;
  nsum = 0;
  dsum = 0.;
  dsumsq = 0.;

  // Figure out number of samples in X and Y
  nxUse = nx - 2 * sampleIndent;
  ixSample = sampleIndent;
  if (nxUse < 4) {
    nxUse = nx;
    ixSample = 0;
  }
  dxSample = nxUse / nSample;
  if (!dxSample)
    dxSample = 1;
  nxSample = (nxUse -1) / dxSample + 1;
  
  nyUse = ny - 2 * sampleIndent;
  iyStart = sampleIndent;
  if (nyUse < 4) {
    nyUse = ny;
    iyStart = 0;
  }
  dySample = nyUse / nSample;
  if (!dySample)
    dySample = 1;
  nySample = (nyUse -1) / dySample + 1;

  // Sample the image to get mean and sd
  for (i = 0; i < nxSample; i++, ixSample += dxSample) {
    
    // Skip a column if it is hot or has a hot pixel
    hot = 0;
    for (j = 0; j < nHotCol && !hot; j++)
      hot = (ixSample == hotCol[j]) ? 1 : 0;
    for (j = 0; j < nHotPixel && !hot; j++)
      hot = (ixSample == hotX[j]) ? 1 : 0;
    if (hot)
      continue;

    // sample the column
    switch (type) {
    case SIGNED_SHORT:
      for (j = 0, iySample = iyStart; j < nySample; j++, iySample += dySample) {
        val = sdata[ixSample + nx * iySample];
        dsum += val;
        dsumsq += val * val;
      }
      break;

    case UNSIGNED_SHORT:
      for (j = 0, iySample = iyStart; j < nySample; j++, iySample += dySample) {
        val = usdata[ixSample + nx * iySample];
        dsum += val;
        dsumsq += val * val;
      }
      break;

    case FLOAT:
      for (j = 0, iySample = iyStart; j < nySample; j++, iySample += dySample) {
        val = fdata[ixSample + nx * iySample];
        dsum += val;
        dsumsq += val * val;
      }
      break;
    }
    nsum += nySample;
  }

  // Get mean and sd
  mean = dsum / nsum;
  sd = 0.;
  diff = dsumsq - nsum * mean * mean;
  if (nsum > 1 && diff > 0.)
    sd = sqrt(diff / (nsum - 1.));
  if (!sd)
    return 0;
  *sdev = (float)sd;

  // The standard deviation criterion
  crit = sdCrit * sd;

  // If both criteria were supplied, pick the larger if both is wanted, or the
  // smaller if either is wanted; otherwise just use the one criterion
  if (crit > 0. && absCrit > 0) {
    if ((eitherBoth && crit < absCrit) || (!eitherBoth && crit > absCrit))
      crit = absCrit;
  } else if (absCrit > 0.)
    crit = absCrit;
    
  intcrit = (int)(crit + mean);

  // Loop in natural order
  for (iy = 0; iy < ny; iy++) {
    ixbase = iy * nx;
    switch (type) {
    case SIGNED_SHORT:
      REPLACE_ON_LINE(sdata)
      break;
    case UNSIGNED_SHORT:
      REPLACE_ON_LINE(usdata)
      break;
    case FLOAT:
      REPLACE_ON_LINE(fdata)
      break;
    }
 }
  
  *nReplaced = totReplaced;
  return totPatches;
}

float ProcGetPixel(void *array, int type, int nx, int ix, int iy)
{
  int ind = ix + iy * nx;
  float val;
  switch (type) {
  case BYTE:
    val = (float)(*((char *)array + ind));
    break;
  case SIGNED_SHORT:
    val = (float)(*((short int *)array + ind));
    break;
  case UNSIGNED_SHORT:
    val = (float)(*((unsigned short int *)array + ind));
    break;
  case FLOAT:
    val = *((float *)array + ind);
    break;
  }
  return val;
}

int ProcReplacePixels(void *array, int type, int nx, int ny, int nHotCol, int *hotCol,
          int nHotPixel, int *hotX, int *hotY, int maxPixel, float minDist, float maxDist,
          double crit, int ix, int iy, int *listX, int *listY)
{
  int i, minx, maxx, miny, maxy, ixc, iyc, dMinsq, distsq, ixCheck, iyCheck, del;
  int nsum, ind;
  double dsum, mean, minsq, maxsq;

  // initialize the list of points
  int ninList = 1;
  int nChecked = 0;
  listX[0] = ix;
  listY[0] = iy;
  maxx = minx = ix;
  maxy = miny = iy;

  // Check pixel against hot list
  for (i = 0; i < nHotCol; i++) {
    if (ix == hotCol[i])
      return 0;
  }
  for (i = 0; i < nHotPixel; i++) {
    if (ix == hotX[i] && iy == hotY[i])
      return 0;
  }


  // Loop until all on list are neighbor-checked or # in list exceeds maximum
  while (nChecked < ninList && ninList <= maxPixel) {
    ixCheck = listX[nChecked];
    iyCheck = listY[nChecked];
    for (iyc = iyCheck - 1; iyc <= iyCheck + 1; iyc++) {
      for (ixc = ixCheck - 1; ixc <= ixCheck + 1; ixc++) {
        if (ixc == ixCheck && iyc == iyCheck)
          continue;

        // check legality of coordinate
        if (ixc < 0 || ixc >= nx || iyc < 0 || iyc >= ny)
          continue;

          // Check pixel itself
        if (ProcGetPixel(array, type, nx, ixc, iyc) <= crit) 
          continue;

       // check if already on list
        for (i = 0; i < ninList; i++) {
          if (ixc == listX[i] && iyc == listY[i])
            break;
        }
        if (i < ninList)
          continue;
        
        // check against hot list
        for (i = 0; i < nHotCol; i++) {
          if (ixc == hotCol[i])
            break;
        }
        if (i < nHotCol)
          continue;

        for (i = 0; i < nHotPixel; i++) {
          if (ixc == hotX[i] && iyc == hotY[i])
            break;
        }
        if (i < nHotPixel)
          continue;

        // Finally add neighbor to list after passing all these tests
        listX[ninList] = ixc;
        listY[ninList++] = iyc;
        if (ixc > maxx)
          maxx = ixc;
        if (ixc < minx)
          minx = ixc;
        if (iyc > maxy)
          maxy = iyc;
        if (iyc < miny)
          miny = iyc;
      }
    }
    nChecked++;
  }

  // If too many in patch, skip it, return error code
  if (ninList > maxPixel)
    return -1;

  // Sum neigboring pixels that fall within the min and max distances
  minsq = minDist * minDist;
  maxsq = maxDist * maxDist;
  del = (int)(maxDist + 0.5);
  nsum = 0;
  dsum = 0.;
  for (iyc = miny - del; iyc <= maxy + del; iyc++) {
    for (ixc = minx - del; ixc <= maxx + del; ixc++) {
 
      // check legality of coordinate
      if (ixc < 0 || ixc >= nx || iyc < 0 || iyc >= ny)
        continue;

      // Find minimum distance of this pixel from one in patch
      dMinsq = 10000000;
      for (i = 0; i < ninList; i++) {
        distsq = (ixc - listX[i]) * (ixc - listX[i]) + (iyc - listY[i]) * (iyc - listY[i]);
        if (distsq < dMinsq)
          dMinsq = distsq;
        if (dMinsq < minsq)
          break;
      }

      if (dMinsq >= minsq && dMinsq <= maxsq) {
        nsum++;
        dsum += ProcGetPixel(array, type, nx, ixc, iyc);
      }
    }
  }

  if (!nsum)
    return 0;
  mean = dsum / nsum;

  // Replace pixels with mean
  for (i = 0; i < ninList; i++) {
    ind = listX[i] + nx * listY[i];
    switch (type) {
    case SIGNED_SHORT:
      *((short int*)array + ind) = (short)(mean + 0.5);
      break;
    case UNSIGNED_SHORT:
      *((unsigned short int*)array + ind) = (unsigned short)(mean + 0.5);
      break;
    case FLOAT:
      *((float*)array + ind) = (float)mean;
      break;
    }
  }

  listX[ninList] = (minx + maxx) / 2;
  listY[ninList] = (miny + maxy) / 2;
  return ninList;
}

// Adjust two halves of an image to have the same intensity at a boundary line
void ProcBalanceHalves(void *array, int type, int nx, int ny, int top, int left, 
                       int boundary, int ifY)
{
  int sumBelow = 0, sumAbove = 0, ix, iy, diff, diffUse;
  short *sdata;
  unsigned short *usdata;
  if (ifY) {

    // Horizontal boundary: Get sum of pixels below and above boundary
    if (boundary <= top || boundary >= top + ny)
      return;
    boundary -= top;
    if (type == MRC_MODE_SHORT) {
      sdata = (short *)array;
      for (ix = 0; ix < nx; ix++) {
        sumBelow += sdata[ix + nx * (boundary - 1)];
        sumAbove += sdata[ix + nx * boundary];
      }
    } else {
      usdata = (unsigned short *)array;
      for (ix = 0; ix < nx; ix++) {
        sumBelow += usdata[ix + nx * (boundary - 1)];
        sumAbove += usdata[ix + nx * boundary];
      }
    }
    diff = B3DNINT((0.5 * (sumAbove - sumBelow)) / ny);

    // Shift each line up or down depending on which side of boundary it is on
    for (iy = 0; iy < ny; iy++) {
      diffUse = iy < boundary ? diff : -diff;
      if (type == MRC_MODE_SHORT) {
        sdata = (short *)array + iy * nx;
        for (ix = 0; ix < nx; ix++)
          *sdata++ += diffUse;
      } else {
        usdata = (unsigned short *)array + iy * nx;
        for (ix = 0; ix < nx; ix++)
          *usdata++ += diffUse;
      }   
    }

  } else {

    // Vertical boundary: Get sum of pixels below and above boundary
    if (boundary <= left || boundary >= left + nx)
      return;
    boundary -= left;
    if (type == MRC_MODE_SHORT) {
      sdata = (short *)array;
      for (iy = 0; iy < ny; iy++) {
        sumBelow += sdata[iy * nx + boundary - 1];
        sumAbove += sdata[iy * nx + boundary];
      }
    } else {
      usdata = (unsigned short *)array;
      for (iy = 0; iy < ny; iy++) {
        sumBelow += usdata[iy * nx + boundary - 1];
        sumAbove += usdata[iy * nx + boundary];
      }
    }
    diff = B3DNINT((0.5 * (sumAbove - sumBelow)) / ny);

    // Shift left half of each line up, rihgt half down
    for (iy = 0; iy < ny; iy++) {
      if (type == MRC_MODE_SHORT) {
        sdata = (short *)array + iy * nx;
        for (ix = 0; ix < boundary; ix++)
          *sdata++ += diff;
        for (; ix < nx; ix++)
          *sdata++ -= diff;
      } else {
        usdata = (unsigned short *)array + iy * nx;
        for (ix = 0; ix < boundary; ix++)
          *usdata++ += diff;
        for (; ix < nx; ix++)
          *usdata++ -= diff;
      }   
    }
  }
}

void DLL_IM_EX ProcPasteByteImages(unsigned char *first, int nx1, int ny1, 
  unsigned char *second, int nx2, int ny2, unsigned char *outImage, bool vertical)
{
  int nxOut, nyOut, loop, iyOut, ix, iy, numLeft, numRight, numBot, numTop;
  int nxCopy, nyCopy, offset = 0;
  unsigned char *inPtr, *outPtr;

  // Get output size
  if (vertical) {
    nxOut = B3DMAX(nx1, nx2);
    nyOut = ny1 + ny2;
  } else {
    nxOut = nx1 + nx2;
    nyOut = B3DMAX(ny1, ny2);
  }

  // setup to copy first image, with zero left offset
  inPtr = first;
  iyOut = 0;
  nxCopy = nx1;
  nyCopy = ny1;
  offset = 0;
  for (loop = 0; loop < 2; loop++) {

    // Set up pad to left and right, or pad line below and above
    if (vertical) {
      numLeft = (nxOut - nxCopy) / 2;
      numRight = (nxOut - nxCopy) - numLeft;
      numBot = 0;
      numTop = 0;
    } else {
      numBot = (nyOut - nyCopy) / 2;
      numTop = (nyOut - nyCopy) - numBot;
      numLeft = 0;
      numRight = 0;
    }

    // Set output pointer with possible offset
    // Do pad lines, the each line with possible pad, then top pad lines
    outPtr = outImage + iyOut * nxOut + offset;
    for (iy = 0; iy < numBot; iy++) {
      for (ix = 0; ix < nxCopy; ix++)
        *outPtr++ = 127;
      outPtr += nxOut - nxCopy;
    }
    for (iy = 0; iy < nyCopy; iy++) {
      for (ix = 0; ix < numLeft; ix++)
        *outPtr++ = 127;
      for (ix = 0; ix < nxCopy; ix++)
        *outPtr++ = *inPtr++;
      for (ix = 0; ix < numRight; ix++)
        *outPtr++ = 127;
      if (!vertical)
        outPtr += nxOut - nxCopy;
    }
    for (iy = 0; iy < numTop; iy++) {
      for (ix = 0; ix < nxCopy; ix++)
        *outPtr++ = 127;
      outPtr += nxOut - nxCopy;
    }

    // Set up for second image
    inPtr = second;
    nxCopy = nx2;
    nyCopy = ny2;
    if (vertical) {
      iyOut = ny1;
    } else {
      iyOut = 0;
      offset = nx1;
    }
  }
}

/*
c subroutine multr
c from Cooley and Lohnes - Multivariate Procedures for the
c Behavioral Sciences, Wiley, 1962 (!).
c computes multiple regression coefficients for an input data matrix
c with the dependent variable in the last column
  subroutine multr(x,mpin,ng,sx,ss,ssd,d,r,xm,sd,b,b1,c,rsq,f)
c x = data matrix, msiz columns by any # of rows
c mp = # of columns (parameters),  ng = # of rows (subjects)
c sx, xm, sd = sum, mean and sd of each column
c ss, ssd = raw and deviation sums of squares and cross products
c d, r = dispersion and correlation matrices
c b, b1 = beta and b weights,  c = constant term
c rsq is r squared, f is anova f with mp-1 and ng-mp degrees of freedom
c pass mpin as negative if there are weights in column mp+1

5/16/02: translate to C for SerialEM.  Pass in xsiz, the first progressing
 dimension of the x matrix.  Keep xm and sd as arguments but make the 2-D 
 matrices and sx and r12 all internally declared
 Also added argument invmat to receive the inverse matrix, if it is not NULL

Here are the rules: x has fortran ordering so x(i,j) -> x[i + j * xsiz]
while everybody else would have C subscripts in same order as for fortran
so  a(i,j) -> a[i * msiz + j]

*/


#define MSIZE 50

//void printmat(char *text, float *mat, int msiz, int nrow, int ncol);

int StatMultr(float *x, int *xsiz, int *mpin, int *ng, float *xm, float *sd, 
     float *b, float *b1, float *c, float *rsq, float *f, float *invmat)
{
/* dimension x(msiz,*), sx(msiz), xm(msiz), sd(msiz)
1, ss(msiz,msiz), ssd(msiz,msiz), d(msiz,msiz), r(msiz,msiz)
  2, r11(msiz,msiz), r12(msiz), b(msiz), b1(msiz) */
  
  float ss[MSIZE][MSIZE], d[MSIZE][MSIZE], r[MSIZE][MSIZE];
  float ssd[MSIZE][MSIZE], r11[MSIZE][MSIZE], sx[MSIZE], r12[MSIZE];
  int mp, m, i, j, err;
  int msiz = MSIZE;
  
  mp = *mpin;
  if (mp < 0)
    mp = -mp;
  
  StatCorrel(x, *xsiz, mp, MSIZE, ng, sx, &ss[0][0], &ssd[0][0], &d[0][0],
    &r[0][0], xm, sd, mpin);
  
    /* printmat("SS matrix", &ss[0][0], msiz, mp, mp);
    printmat("SSD matrix", &ssd[0][0], msiz, mp, mp);
    printmat("D matrix", &d[0][0], msiz, mp, mp);
    printmat("R matrix", &r[0][0], msiz, mp, mp);
    printmat("XM", xm, msiz, 1, mp);
  printmat("SD", sd, msiz, 1, mp); */
  
  m = mp - 1;
  for (i = 0; i< m; i++) {
    r12[i] = r[i][m];
    b[i] = r12[i];
    for (j = 0; j < m; j++) {
      r11[i][j] = r[i][j];
    }
  }
  if ((err = StatGaussj(&r11[0][0], m, msiz, b, 1, 1)))
    //printf("Error %d from gaussj\n", err);
    return err;
  
  *rsq=0.;
  for (i = 0; i< m; i++)
    *rsq += b[i] * r12[i];
  
  *f = 10000.;
  if(*rsq < 1.)
    *f = *rsq * (*ng - mp) / (m * (1. - *rsq));
  
  *c = xm[m];
  for (i = 0; i< m; i++) {
    b1[i] = b[i]*sd[m] / sd[i];
    *c -= b1[i] * xm[i];
    if (invmat)
      for (j = 0; j < m; j++)
        *invmat++ = r11[i][j];
  }
  return 0;
}

/*
void printmat(char *text, float *mat, int msiz, int nrow, int ncol)
{
     int i, j;
     printf("%s:\n", text);
    for (i = 0; i < nrow; i++) {
    for (j=0; j < ncol; j++)
         printf("%15.5f", mat[i * msiz + j]);
    printf("\n");
     }
}
*/

/*
c subroutine correl
c from Cooley and Lohnes - Multivariate Procedures for the
c Behavioral Sciences, Wiley, 1962 (!).
c computes basic matrices - means, sd's, variances, covariances,
c correlations - for an input data matrix
  subroutine correl(x,m,ng,sx,ss,ssd,d,r,xm,sd,ifdisp)
c x = data matrix, msiz columns by any # of rows
c m = # of columns (parameters),  ng = # of rows (subjects)
c or ng = 10000*(starting row #) + (ending row #)
c sx, xm, sd = sum, mean and sd of each column
c ss, ssd = raw and deviation sums of squares and cross products
c d, r = dispersion and correlation matrices, compute if ifdisp>0
c IF ifdisp <0, then compute ssd with weights in column m+1
c   
c   4/30/92: rewrote to calculate means first then form SSD from the
c   deviations from means; this is slower, but the old formula was
c   too inaccurate when fitting high-order polynomials.
c   Also indented loops and consolidated some loops
c
5/16/02: rewrote in C for SerialEM.  xsiz is the size of variable dimension
of the x data matrix (the dimension of first progression).  msiz is the
size of the 2-D matrices that are passed in.

Here are the rules: x has fortran ordering so x(i,j) -> x[i + j * xsiz]
while everybody else would have C subscripts in same order as for fortran
so  a(i,j) -> a[i * msiz + j]

*/

void StatCorrel(float *x, int xsiz, int m, int msiz, int *ng, float *sx,
       float *ss,
       float *ssd, float *d, float *r, float *xm, float *sd, int *ifdisp)
{
/*  dimension x(msiz,*), sx(msiz), xm(msiz), sd(msiz)
  1, ss(msiz,msiz), ssd(msiz,msiz), d(msiz,msiz), r(msiz,msiz) */
  int ns = 1;
  float eng, den, weight, wsum;
  int i, j, k;
  
  if (*ng > 10000) {
    ns = *ng / 10000;
    *ng = *ng - 10000 * ns;
  }
  
  eng = *ng + 1 - ns;
  
  for (i = 0; i< m; i++) {
    sx[i] = 0.;
    for (j = 0; j < m; j++) {
      ssd[msiz * i + j] = 0.;
      r[msiz * i + j] = 0.;
    }
  }
  
  if (*ifdisp >= 0) {
    for (i = 0; i< m; i++) {
      for (k = ns - 1; k < *ng; k++)
        sx[i] += x[i + k * xsiz];
      xm[i] = sx[i]/eng;
    }
  } else {
    wsum = 0.;
    for (k = ns - 1; k < *ng; k++)
      wsum += x[m + k * xsiz];
    for (i = 0; i< m; i++) {
      for (k = ns - 1; k < *ng; k++)
        sx[i] += x[i + k * xsiz] * x[m + k * xsiz];
      xm[i] = sx[i]/wsum;
    }
  }
  
  for (k = ns - 1; k < *ng; k++) {
    weight = 1.;
    if(*ifdisp < 0)
      weight = x[m + k * xsiz];
    for (i = 0; i< m; i++)
      for (j = 0; j < m; j++)
        ssd[i * msiz + j] += (x[i + k * xsiz] - xm[i]) * 
        (x[j + k * xsiz] - xm[j]) * weight;
  }
  
  for (i = 0; i< m; i++) {
    sd[i] = (float)sqrt((double)(ssd[i * msiz + i]/(eng-1.)));
    for (j = 0; j < m; j++) {
      ss[i * msiz + j] = ssd[i * msiz + j]+sx[i] * sx[j]/eng;
      ss[j * msiz + i] = ss[i * msiz + j];
      ssd[j * msiz + i] = ssd[i * msiz + j];
    }
  }
  if(*ifdisp == 0)
    return;
  for (i = 0; i< m; i++) {
    for (j = 0; j < m; j++) {
      d[i * msiz + j] = ssd[i * msiz + j]/(eng-1.);
      d[j * msiz + i] = d[i * msiz + j];
      den = sd[i] * sd[j];
      r[i * msiz + j] = 1.;
      if(den > 1.e-30)
        r[i * msiz + j] = d[i * msiz + j]/(sd[i] * sd[j]);
      r[j * msiz + i] = r[i * msiz + j];
    }
  }
  return;
}

/*
 * Wrappers to library functions, either for SerialEM use or so plugins do not
 * need to link with the library
 */
int StatGaussj(float *a, int n, int np, float *b, int m, int mp)
{
  return gaussj(a, n, np, b, m, mp);
}

void StatLSFit(float *x, float *y, int num, float &slope, float &intcp)
{
  float ro;
  lsFit(x, y, num, &slope, &intcp, &ro);
}

void StatLSFitPred(float *x, float *y, int n, float *slope, float *bint,
                   float *ro, float xpred, float *ypred, float *prederr)
{
  float sa, sb, se;
  lsFitPred(x, y, n, slope, bint, ro, &sa, &sb, &se, xpred, ypred, prederr);
}

void sliceTaperInPad2(void *inArray, int type, int nxDimIn, int ixStart, int ixEnd,
  int iyStart, int iyEnd, float *outArray, int nxDimOut, int nx, int ny,
  int nxTaper, int nyTaper)
{
  int lowBase, highBase, x1, x2, ixLow, ixHigh, iyLow, iyHigh, ix, iy, ixBase;
  int nxBox, nyBox, ixLim, y1, y2;
  float dmean, fracX, fracY, fmin;
  b3dUByte *byteIn;
  b3dInt16 *intIn;
  b3dUInt16 *uintIn;
  float *floatIn;
  float *out;
#ifdef PAD_TIMING
  double wallCen, wallSide, wallEdge, now, wallStart = wallTime();
#endif

  /* ixlo, iylo are last index below image location in output array,
  ixhi, iyhi are last index within image location */
  nxBox = ixEnd + 1 - ixStart;
  nyBox = iyEnd + 1 - iyStart;
  ixLow = nx / 2 - nxBox / 2 - 1;
  ixHigh = ixLow + nxBox;
  iyLow = ny / 2 - nyBox / 2 - 1;
  iyHigh = iyLow + nyBox;
  for (iy = iyEnd; iy >= iyStart; iy--) {
    out = outArray + ixHigh + (iyLow + 1 + iy - iyStart) * nxDimOut;
    switch (type) {
    case SLICE_MODE_BYTE:
      byteIn = (b3dUByte *)inArray + ixEnd + iy * nxDimIn;
      for (ix = ixEnd; ix >= ixStart; ix--)
        *out-- = *byteIn--;
      break;

    case SLICE_MODE_SHORT:
      intIn = (b3dInt16 *)inArray + ixEnd + iy * nxDimIn;
      for (ix = ixEnd; ix >= ixStart; ix--)
        *out-- = *intIn--;
      break;

    case SLICE_MODE_USHORT:
      uintIn = (b3dUInt16 *)inArray + ixEnd + iy * nxDimIn;
      for (ix = ixEnd; ix >= ixStart; ix--)
        *out-- = *uintIn--;
      break;

    case SLICE_MODE_FLOAT:
      floatIn = (float *)inArray + ixEnd + iy * nxDimIn;
      for (ix = ixEnd; ix >= ixStart; ix--)
        *out-- = *floatIn--;
      break;

    case SLICE_MODE_RGB:
      byteIn = (b3dUByte *)inArray + 3 * (ixEnd + iy * nxDimIn);
      for (ix = ixEnd; ix >= ixStart; ix--) {
        *out-- = byteIn[0] + byteIn[1] + byteIn[2];
        byteIn -= 3;
      }
      break;

    }
  }
#ifdef PAD_TIMING
  now = wallTime();
  wallCen = now - wallStart;
  wallStart = now;
#endif

  /* get edge mean */
  dmean = (float)sliceEdgeMean(outArray, nxDimOut, ixLow + 1, ixHigh, iyLow + 1, iyHigh);

  /* fill the rest of the array with dmean */
  if (nxBox != nx || nyBox != ny) {
    for (iy = iyLow + 1; iy <= iyHigh; iy++) {
      ixBase = iy * nxDimOut;
      for (ix = 0; ix <= ixLow; ix++)
        outArray[ix + ixBase] = dmean;
      for (ix = ixHigh + 1; ix < nx; ix++)
        outArray[ix + ixBase] = dmean;
    }
    for (iy = 0; iy <= iyLow; iy++) {
      ixBase = iy * nxDimOut;
      for (ix = 0; ix < nx; ix++)
        outArray[ix + ixBase] = dmean;
    }
    for (iy = iyHigh + 1; iy < ny; iy++) {
      ixBase = iy * nxDimOut;
      for (ix = 0; ix < nx; ix++)
        outArray[ix + ixBase] = dmean;
    }
  }
#ifdef PAD_TIMING
  now = wallTime();
  wallEdge = now - wallStart;
  wallStart = now;
#endif

  /* Taper the edges */
  for (iy = 0; iy < (nyBox + 1) / 2; iy++) {
    fracY = 1.;
    ixLim = nxTaper;
    if (iy < nyTaper) {
      fracY = (iy + 1.f) / (nyTaper + 1.f);
      ixLim = (nxBox + 1) / 2;
    }
    for (ix = 0; ix < ixLim; ix++) {
      fracX = 1.;
      if (ix < nxTaper)
        fracX = (ix + 1.f) / (nxTaper + 1.f);
      fmin = fracX < fracY ? fracX : fracY;
      if (fmin < 1.) {
        x1 = ix + 1 + ixLow;
        x2 = ixHigh - ix;
        y1 = iy + 1 + iyLow;
        y2 = iyHigh - iy;

        /*      DNM 4/28/02: for odd box sizes, deflect middle pixel to edge */
        /* to keep it from being attenuated twice */
        if (x1 == x2)
          x2 = 0;
        if (y1 == y2)
          y2 = 0;

        lowBase = y1 * nxDimOut;
        highBase = y2 * nxDimOut;
        outArray[x1 + lowBase] = fmin * (outArray[x1 + lowBase] - dmean) + dmean;
        outArray[x1 + highBase] = fmin * (outArray[x1 + highBase] - dmean) + dmean;
        outArray[x2 + lowBase] = fmin * (outArray[x2 + lowBase] - dmean) + dmean;
        outArray[x2 + highBase] = fmin * (outArray[x2 + highBase] - dmean) + dmean;
      }
    }
  }
#ifdef PAD_TIMING
  now = wallTime();
  wallSide = now - wallStart;
  printf("Cen %.1f edge %.1f  side %.1f\n", wallCen * 1000., wallEdge * 1000.,
    wallSide * 1000.);
  fflush(stdout);
#endif
}
void XCorrTaperInPad(void *array, int type, int nxdimin, int ix0, int ix1,
  int iy0, int iy1, float *brray, int nxdim, int nx,
  int ny, int nxtap, int nytap) {
  sliceTaperInPad2(array, type, nxdimin, ix0, ix1, iy0, iy1, brray, nxdim, nx,
    ny, nxtap, nytap);
}
