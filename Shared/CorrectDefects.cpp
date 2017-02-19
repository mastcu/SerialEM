// CorrectDefects.cpp:  Functions for defect correction, management of a structure 
//                      specifying corrections, and some general image coordinate 
//                      conversion routines
//
// Depends on IMOD library libcfshr (uses parse_params.c and samplemeansd.c)
// This module is shared between IMOD and SerialEM and is thus GPL.
//
// Copyright (C) 2014 by the Regents of the University of Colorado.
//
// Author: David Mastronarde
//
//  No ID line: it is shared among three different projects
//
//////////////////////////////////////////////////////////////////////

#include <set>
#include <map>
#include <string.h>
#include <iostream>
#include "CorrectDefects.h"
#include "parse_params.h"
#include "b3dutil.h"

// Local functions
static void ScaleRowsOrColumns(UShortVec &colStart, ShortVec &colWidth, 
                               UShortVec &partial, ShortVec &partWidth, UShortVec &startY,
                               UShortVec &endY, int upFac, int downFac, int addFac);

static void CorrectEdge(void *array, int type, int numBad, int taper, int length,
                     int sumLength, int indStart, int stepAlong, int stepBetween);
static void CorrectColumn(void *array, int type, int nx, int xstride, int ystride, 
                       int indStart, int num, int ystart, int yend);
static void CorrectPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                      int ypix, int useMean, float mean);
static void CorrectSuperPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                      int ypix, int useMean, float mean);
static void CorrectPixels2Ways(CameraDefects *param, void *array, int type, int sizeX, 
                               int sizeY, int binning, int top, int left, int useMean, 
                               float mean);
static int CheckIfPointInFullLines(int xx, UShortVec &columns, ShortVec &widths);
static int CheckIfPointInPartialLines(int xx, int yy, UShortVec &columns, 
                                      ShortVec &widths, UShortVec &startY, 
                                      UShortVec &endY);
static int CheckPointNearFullLines(int xx, UShortVec &columns, ShortVec &widths,
                                   int leftDiff);
static int CheckPointNearPartialLines(int xx, int yy, UShortVec &columns, 
                                      ShortVec &widths, UShortVec &startY, 
                                      UShortVec &endY, int leftDiff);
static void AddOneRowOrColumn(int xmin, int xmax, int ymin, int ymax, int camSize,
                              UShortVec &colStart, ShortVec &colWidth, 
                              UShortVec &partial, ShortVec &partWidth, 
                              UShortVec &startY, UShortVec &endY);
static void AddRowsOrColumns(int xmin, int xmax, int ymin, int ymax, int camSize,
                             unsigned short *xyPairs, IntVec &inGroup, int indXY,
                             UShortVec &colStart, ShortVec &colWidth, 
                             UShortVec &partial, ShortVec &partWidth, 
                             UShortVec &startY, UShortVec &endY, UShortVec &badPixelX,
                             UShortVec &badPixelY);
static void BadRowsOrColsToString(UShortVec &starts, ShortVec &widths, std::string &strng,
                                  std::string &strbuf, char *buf, const char *name);

/////////////////////////////////////////////////////////////
// ACTUAL DEFECT CORRECTION ROUTINES
/////////////////////////////////////////////////////////////

// Correct edge and column defects in an image: the master routine
// Coordinates are binned camera acquisition coordinates (top < bottom) where the
// image runs from left to right -1, top to bottom - 1, inclusive
void CorDefCorrectDefects(CameraDefects *param, void *array, int type, 
                        int binning, int top, int left, int bottom, int right)
{
  int firstBad, numBad, i, badStart, badEnd, yStart, yEnd;
  int taper = 5;
  float mean, SD;
  int sizeX = right - left;
  int sizeY = bottom - top;
  int sumX = (sizeX + 9) / 10;
  int sumY = (sizeY + 9) / 10;
  if (sumX > 50)
    sumX = 50;
  if (sumY > 50)
    sumY = 50;
  
  // If there are any pixels that should use mean, get the mean and correct them first
  if (param->pixUseMean.size()) {
    CorDefSampleMeanSD(array, type, sizeX, sizeY, &mean, &SD);
    CorrectPixels2Ways(param, array, type, sizeX, sizeY, binning, top, left, 1, mean);
  }

  // Correct on the top
  // Number of bad rows  = index of first good one
  numBad = (param->usableTop - 1) / binning + 1 - top;
  if (param->usableTop > 0 && numBad > 0)
    
    // pass length of row, starting index at good row, step along row,
    // step between rows
    CorrectEdge(array, type, numBad, taper, sizeX, sumX,
      numBad * sizeX, 1, -sizeX);
  
  // first bad row index on the bottom: correct if it is in image
  firstBad = (param->usableBottom + 1) / binning - top;
  numBad = sizeY - firstBad;
  if (param->usableBottom > 0 && numBad > 0)
    CorrectEdge(array, type, numBad, taper, sizeX, sumX,
              (firstBad - 1) * sizeX, 1, sizeX);
  
  // Correct on the left
  // Number of bad columns  = index of first good one
  numBad = (param->usableLeft - 1) / binning + 1 - left;
  if (param->usableLeft > 0 && numBad > 0)
    CorrectEdge(array, type, numBad, taper, sizeY, sumY, numBad,
      sizeX, -1);
  
  // first bad column index on the right: correct if it is in image
  firstBad = (param->usableRight + 1) / binning - left;
  numBad = sizeX - firstBad;
  if (param->usableRight > 0 && numBad > 0)
    CorrectEdge(array, type, numBad, taper, sizeY, sumY, firstBad - 1,
              sizeX, 1);
  
  // Correct column defects
  for (i = 0; i < (int)param->badColumnStart.size(); i++) {
  
    // convert starting and ending columns to columns in binned image
    // to get starting column and width
    badStart = param->badColumnStart[i] / binning;
    badEnd = (param->badColumnStart[i] + param->badColumnWidth[i] - 1) / binning;

    CorrectColumn(array, type, sizeX, 1, sizeX, badStart - left, 
      badEnd + 1 - badStart, 0, sizeY - 1);
  }

  // Correct partial bad columns
  for (i = 0; i < (int)param->partialBadCol.size(); i++) {
  
    // convert column to column and start and end in binned image
    badStart = param->partialBadCol[i] / binning;
    badEnd = (param->partialBadCol[i] + param->partialBadWidth[i] - 1) / binning;
    yStart = param->partialBadStartY[i] / binning - top;
    yEnd = param->partialBadEndY[i] / binning - top;
    if (yStart < sizeY && yEnd >= 0 && yStart <= yEnd) {
      yStart = B3DMAX(0, yStart);
      yEnd = B3DMIN(sizeY - 1, yEnd);
      CorrectColumn(array, type, sizeX, 1, sizeX, badStart - left, 
        badEnd + 1 - badStart, yStart, yEnd); 
    }
  }

  // Correct row defects
  for (i = 0; i < (int)param->badRowStart.size(); i++) {
  
    // convert starting and ending columns to columns in binned image
    // to get starting column and width
    badStart = param->badRowStart[i] / binning;
    badEnd = (param->badRowStart[i] + param->badRowHeight[i] - 1) / binning;

    CorrectColumn(array, type, sizeY, sizeX, 1, badStart - top, 
      badEnd + 1 - badStart, 0, sizeX - 1);
  }

  // Correct partial bad rows
  for (i = 0; i < (int)param->partialBadRow.size(); i++) {
  
    // convert column to column and start and end in binned image
    badStart = param->partialBadRow[i] / binning;
    badEnd = (param->partialBadRow[i] + param->partialBadHeight[i] - 1) / binning;
    yStart = param->partialBadStartX[i] / binning - left;
    yEnd = param->partialBadEndX[i] / binning - left;
    if (yStart < sizeX && yEnd >= 0 && yStart <= yEnd) {
      yStart = B3DMAX(0, yStart);
      yEnd = B3DMIN(sizeX - 1, yEnd);
      CorrectColumn(array, type, sizeY, sizeX, 1, badStart - top, 
        badEnd + 1 - badStart, yStart, yEnd); 
    }
  }

  // Correct bad pixels with neighboring values
  CorrectPixels2Ways(param, array, type, sizeX, sizeY, binning, top, left, 0, 0.);
}

// Correct one edge of an image
static void CorrectEdge(void *array, int type, int numBad, int taper, int length,
              int sumLength, int indStart, int stepAlong, int stepBetween)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  double sum;
  double sumFac, frac;
  int ind, i, indDest, row, indDrop, indAdd, addStart, addEnd;

  if (sumLength > length)
    sumLength = length;
  addStart = sumLength / 2;
  addEnd = length - (sumLength - sumLength / 2);
  
  for (row = 0; row < numBad; row++) {
    
    // First get the mean at beginning of the last good row
    sum = 0;
    switch (type) {
    case SLICE_MODE_BYTE:
      for (i = 0, ind = indStart; i < sumLength; i++, ind += stepAlong)
        sum += bdata[ind];
      break;
    case SLICE_MODE_SHORT:
      for (i = 0, ind = indStart; i < sumLength; i++, ind += stepAlong)
        sum += sdata[ind];
      break;
    case SLICE_MODE_USHORT:
      for (i = 0, ind = indStart; i < sumLength; i++, ind += stepAlong)
        sum += usdata[ind];
      break;
    case SLICE_MODE_FLOAT:
      for (i = 0, ind = indStart; i < sumLength; i++, ind += stepAlong)
        sum += fdata[ind];
      break;
    }
    indDrop = indStart;
    indAdd = ind;

    // For the row, set the index to place pixels, compute fraction if tapering
    indDest = indStart + (row + 1) * stepBetween;
    frac = 1.;
    sumFac = 0.;
    if (taper > 0) {
      frac = (double)(taper - row - 1.) / taper;
      if (frac < 0.)
        frac = 0.;
      sumFac = (1. - frac) / sumLength;
    }
    
    // Replace the row
    switch (type) {
    case SLICE_MODE_BYTE:
      for (i = 0, ind = indStart; i < length; i++, ind += stepAlong,
        indDest += stepAlong) {
        if (i >= addStart && i < addEnd) {
          sum += (int)bdata[indAdd] - bdata[indDrop];
          indAdd += stepAlong;
          indDrop += stepAlong;
        }
        bdata[indDest] = (unsigned char)(frac * bdata[ind] + sumFac * sum);
      }
      break;
    case SLICE_MODE_SHORT:
      for (i = 0, ind = indStart; i < length; i++, ind += stepAlong,
        indDest += stepAlong) {
        if (i >= addStart && i < addEnd) {
          sum += sdata[indAdd] - sdata[indDrop];
          indAdd += stepAlong;
          indDrop += stepAlong;
        }
        sdata[indDest] = (short int)(frac * sdata[ind] + sumFac * sum);
      }
      break;
    case SLICE_MODE_USHORT:
      for (i = 0, ind = indStart; i < length; i++, ind += stepAlong, 
        indDest += stepAlong) {
        if (i >= addStart && i < addEnd) {
          sum += usdata[indAdd] - usdata[indDrop];
          indAdd += stepAlong;
          indDrop += stepAlong;
        }
        usdata[indDest] = (unsigned short int)(frac * usdata[ind] + sumFac * sum);
      }
      break;
    case SLICE_MODE_FLOAT:
      for (i = 0, ind = indStart; i < length; i++, ind += stepAlong, 
        indDest += stepAlong) {
        if (i >= addStart && i < addEnd) {
          sum += fdata[indAdd] - fdata[indDrop];
          indAdd += stepAlong;
          indDrop += stepAlong;
        }
        fdata[indDest] = (float)(frac * fdata[ind] + sumFac * sum);
      }
      break;
    }
  }
}

// Macros for computing the column correction, with summing of one or two rows of
// edge values for thicker columns
// Arguments are MRC type, array, type for cast assignment, [type for cast to start sum],
// and amount to add for rounding of integers
#define CORRECT_ONE_TWO_COL(c, a, b, e)                                 \
  case c:                                                               \
  for (i = ystart; i <= yend; i++, ind += yStride, indLeft += yStride,  \
         indRight += yStride)                                           \
    a[ind] = (b)(fLeft * a[indLeft] + fRight * a[indRight] + e);        \
  break;

#define CORRECT_THREE_FOUR_COL(c, a, b, d, e)                           \
  case c:                                                               \
  a[ind] = (b)((fLeft * ((d)a[indLeft] + a[indLeft + yStride]) +        \
                fRight * ((d)a[indRight] + a[indRight + yStride])) / 2. + e); \
  ind += yStride;                                                       \
  indLeft += yStride;                                                   \
  indRight += yStride;                                                  \
  for (i = ystart; i <= yend; i++, ind += yStride, indLeft += yStride,  \
         indRight += yStride)                                           \
    a[ind] = (b)((fLeft * ((d)a[indLeft - yStride] + a[indLeft] +       \
                           a[indLeft + yStride]) +                      \
                  fRight * ((d)a[indRight - yStride] + a[indRight] +    \
                            a[indRight + yStride])) / 3. + e);          \
  a[ind] = (b)((fLeft * ((d)a[indLeft - yStride] + a[indLeft]) +        \
                fRight * ((d)a[indRight - yStride] + a[indRight])) / 2. + e); \
  break;

#define CORRECT_FIVE_PLUS_COL(c, a, b, d, e)                            \
  case c:                                                               \
  a[ind] = (b)((fLeft * ((d)a[indLeft] + a[indLeft + yStride] +         \
                         a[indLeft - xStride] + a[indLeft + yStride - xStride]) + \
                fRight * ((d)a[indRight] + a[indRight + yStride] +      \
                          a[indRight + xStride] +                       \
                          a[indRight + yStride + xStride])) / 4. + e);  \
  ind += yStride;                                                       \
  indLeft += yStride;                                                   \
  indRight += yStride;                                                  \
  for (i = ystart; i <= yend; i++, ind += yStride, indLeft += yStride,  \
         indRight += yStride)                                           \
    a[ind] = (b)((fLeft * ((d)a[indLeft - yStride] + a[indLeft] + a[indLeft + yStride] + \
                           a[indLeft - xStride - yStride] + a[indLeft - xStride] + \
                           a[indLeft - xStride + yStride]) +            \
                  fRight * ((d)a[indRight - yStride] + a[indRight] +    \
                            a[indRight + yStride] +                     \
                            a[indRight + xStride - yStride] + a[indRight + xStride] + \
                            a[indRight + xStride + yStride])) / 6. + e); \
  a[ind] = (b)((fLeft * ((d)a[indLeft - yStride] + a[indLeft] +         \
                         a[indLeft - xStride - yStride] + a[indLeft - xStride]) + \
                fRight * ((d)a[indRight - yStride] + a[indRight] +      \
                          a[indRight + xStride - yStride] +             \
                          a[indRight + xStride])) / 4. + e);            \
  break;

// Correct a column defect which can be multiple columns
static void CorrectColumn(void *array, int type, int nx, int xStride, int yStride, 
                       int indStart, int num, int ystart, int yend)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  double fLeft, fRight;
  bool fivePlusOK;
  int ind, i, col, indLeft, indRight;
  
  // Adjust indStart or num if on edge of image; return if nothing left
  if (indStart < 0) {
    num += indStart;
    indStart = 0;
  }

  if (indStart + num > nx)
    num = nx - indStart;

  if (num <= 0)
    return;

  for (col = 0; col < num; col++) {
    
    // Set up fractions on left and right columns
    fRight = (col + 1.) / (num + 1.);
    fLeft = 1. - fRight;

    // Set up indexes for left and right columns; just use one side if on edge
    indLeft = indStart - 1;
    indRight = indStart + num;
    if (indLeft < 0)
      indLeft = indRight;
    if (indRight >= nx)
      indRight = indLeft;
    fivePlusOK = indLeft > 0 && indRight < nx - 1;
    indLeft *= xStride;
    indRight *= xStride;
    ind = (indStart + col) * xStride;
    ind += yStride * ystart;
    indLeft += yStride * ystart;
    indRight += yStride * ystart;

    if (num >= 5 && yend - ystart >= 3 && fivePlusOK) {
      switch (type) {
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_BYTE, bdata, unsigned char, int, 0.5);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_SHORT, sdata, short int, int, 0.5);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_USHORT, usdata, unsigned short int, int, 0.5);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_FLOAT, fdata, float, float, 0.);
      }
    } else if (num >= 3 && yend - ystart >= 1) {
      switch (type) {
        CORRECT_THREE_FOUR_COL(SLICE_MODE_BYTE, bdata, unsigned char, int, 0.5);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_SHORT, sdata, short int, int, 0.5);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_USHORT, usdata, unsigned short int, int, 0.5);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_FLOAT, fdata, float, float, 0.);
      }
    } else {
      switch (type) {
        CORRECT_ONE_TWO_COL(SLICE_MODE_BYTE,  bdata, unsigned char, 0.5);
        CORRECT_ONE_TWO_COL(SLICE_MODE_SHORT, sdata, short int, 0.5);
        CORRECT_ONE_TWO_COL(SLICE_MODE_USHORT, usdata, unsigned short int, 0.5);
        CORRECT_ONE_TWO_COL(SLICE_MODE_FLOAT, fdata, float, 0.);
      }
    }
  }
}

// Correct a single-pixel defect
static void CorrectPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                      int ypix, int useMean, float mean)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  int idx[4] = {-1, 1, 0, 0};
  int idy[4] = {0, 0, -1, 1};
  int isum = 0, nsum = 0;
  float fsum = 0.f;
  int ix, iy, i, index = xpix + ypix * nxdim;

  if (useMean) {
    if (type == SLICE_MODE_BYTE)
      bdata[index] = (unsigned char)(mean + 0.5);
    else if (type == SLICE_MODE_SHORT)
      sdata[index] = (short)(mean + 0.5);
    else if (type == SLICE_MODE_USHORT)
      usdata[index] = (unsigned short)(mean + 0.5);
    else
      fdata[index] = mean;
    return;
  }

  // Average 4 pixels around the point if they all exist
  if (xpix > 0 && xpix < nx - 1 && ypix > 0 && ypix < ny - 1) {
    switch (type) {
    case SLICE_MODE_BYTE:
      bdata[index] = ((int)bdata[index - 1] + bdata[index + 1] + bdata[index - nxdim] +
        bdata[index + nxdim] + 2) / 4;
      break;
    case SLICE_MODE_SHORT:
      sdata[index] = ((int)sdata[index - 1] + sdata[index + 1] + sdata[index - nxdim] +
        sdata[index + nxdim] + 2) / 4;
      break;
    case SLICE_MODE_USHORT:
      usdata[index] = ((int)usdata[index - 1] + usdata[index + 1] + usdata[index - nxdim]
        + usdata[index + nxdim] + 2) / 4;
      break;
    case SLICE_MODE_FLOAT:
      fdata[index] = (fdata[index - 1] + fdata[index + 1] + fdata[index - nxdim] +
        fdata[index + nxdim]) / 4.f;
      break;
    }
    return;
  }

  // Or average whatever is there
  for (i = 0 ; i < 4; i++) {
    ix = xpix + idx[i];
    iy = ypix + idy[i];
    if (ix >= 0 && ix < nx && iy >= 0 && iy < ny) {
      nsum++;
      if (type == SLICE_MODE_SHORT)
        isum += sdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_USHORT)
        isum += usdata[ix + iy * nxdim];
      else
        fsum += fdata[ix + iy * nxdim];
    }
  }
  if (type == SLICE_MODE_BYTE)
    bdata[index] = (unsigned char)((isum + nsum / 2) / nsum);
  else if (type == SLICE_MODE_SHORT)
    sdata[index] = (short)((isum + nsum / 2) / nsum);
  else if (type == SLICE_MODE_USHORT)
    usdata[index] = (unsigned short)((isum + nsum / 2) / nsum);
  else
    fdata[index] = (float)(fsum / nsum);
}

// Correct a single-pixel defect in super-res image
static void CorrectSuperPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                      int ypix, int useMean, float mean)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  int idx[16] = {-2, -1, 2, 3, -2, -1, 2, 3, 0, 1, 0, 1, 0, 1, 0, 1};
  int idy[16] = {0, 0, 0, 0, 1, 1, 1, 1, -2, -2, -1, -1, 2, 2, 3, 3};
  int isum = 0, nsum = 0;
  float fsum = 0.f;
  int ix, iy, i, ip2n, ip3n, imn, im2n, index = xpix + ypix * nxdim;
  int ipn = index + nxdim;

  if (useMean) {
    if (type == SLICE_MODE_BYTE)
      bdata[index] = bdata[index + 1] = bdata[ipn] = bdata[ipn + 1] = 
        (unsigned char)(mean + 0.5);
    else if (type == SLICE_MODE_SHORT)
      sdata[index] = sdata[index + 1] = sdata[ipn] = sdata[ipn + 1] = 
        (short)(mean + 0.5);
    else if (type == SLICE_MODE_USHORT)
      usdata[index] = usdata[index + 1] = usdata[ipn] = usdata[ipn + 1] = 
        (unsigned short)(mean + 0.5);
    else
      fdata[index] = fdata[index + 1] = fdata[ipn] = fdata[ipn + 1] = mean;
    return;
  }
  
  // Average 4 chip pixels around the point if they all exist
  if (xpix > 1 && xpix < nx - 3 && ypix > 1 && ypix < ny - 3) {
    ip2n = index + 2 * nxdim;
    ip3n = index + 3 * nxdim;
    imn = index - nxdim;
    im2n = index - 2 * nxdim;
    switch (type) {
    case SLICE_MODE_BYTE:
      isum = ((int)bdata[index - 1] + bdata[index - 2] + bdata[ipn - 1] + bdata[ipn - 2] +
        bdata[index + 2] + bdata[index + 3] + bdata[ipn + 2] + bdata[ipn + 3] +
        bdata[imn] + bdata[imn + 1] + bdata[im2n] + bdata[im2n + 1] +
        bdata[ip2n] + bdata[ip2n + 1] + bdata[ip3n] + bdata[ip3n + 1] + 8) / 16;
      bdata[index] = isum;
      bdata[index + 1] = isum;
      bdata[ipn] = isum;
      bdata[ipn + 1] = isum;
      break;
    case SLICE_MODE_SHORT:
      isum = ((int)sdata[index - 1] + sdata[index - 2] + sdata[ipn - 1] + sdata[ipn - 2] +
        sdata[index + 2] + sdata[index + 3] + sdata[ipn + 2] + sdata[ipn + 3] +
        sdata[imn] + sdata[imn + 1] + sdata[im2n] + sdata[im2n + 1] +
        sdata[ip2n] + sdata[ip2n + 1] + sdata[ip3n] + sdata[ip3n + 1] + 8) / 16;
      sdata[index] = isum;
      sdata[index + 1] = isum;
      sdata[ipn] = isum;
      sdata[ipn + 1] = isum;
      break;
    case SLICE_MODE_USHORT:
      isum = ((int)usdata[index - 1] + usdata[index - 2] + usdata[ipn - 1] + 
        usdata[ipn - 2] +
        usdata[index + 2] + usdata[index + 3] + usdata[ipn + 2] + usdata[ipn + 3] +
        usdata[imn] + usdata[imn + 1] + usdata[im2n] + usdata[im2n + 1] +
        usdata[ip2n] + usdata[ip2n + 1] + usdata[ip3n] + usdata[ip3n + 1] + 8) / 16;
      usdata[index] = isum;
      usdata[index + 1] = isum;
      usdata[ipn] = isum;
      usdata[ipn + 1] = isum;
      break;
    case SLICE_MODE_FLOAT:
      fsum = (fdata[index - 1] + fdata[index - 2] + fdata[ipn - 1] + fdata[ipn - 2] +
        fdata[index + 2] + fdata[index + 3] + fdata[ipn + 2] + fdata[ipn + 3] +
        fdata[imn] + fdata[imn + 1] + fdata[im2n] + fdata[im2n + 1] +
        fdata[ip2n] + fdata[ip2n + 1] + fdata[ip3n] + fdata[ip3n + 1]) / 16.f;
      fdata[index] = fsum;
      fdata[index + 1] = fsum;
      fdata[ipn] = fsum;
      fdata[ipn + 1] = fsum;
      break;
    }
    return;
  }

  // Or average whatever is there
  for (i = 0 ; i < 16; i++) {
    ix = xpix + idx[i];
    iy = ypix + idy[i];
    if (ix >= 0 && ix < nx && iy >= 0 && iy < ny) {
      nsum++;
      if (type == SLICE_MODE_SHORT)
        isum += sdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_USHORT)
        isum += usdata[ix + iy * nxdim];
      else
        fsum += fdata[ix + iy * nxdim];
    }
  }
  
  if (type == SLICE_MODE_BYTE)
    bdata[index] = bdata[index + 1] = bdata[ipn] = bdata[ipn + 1] = 
      (unsigned char)((isum + nsum / 2) / nsum);
  else if (type == SLICE_MODE_SHORT)
    sdata[index] = sdata[index + 1] = sdata[ipn] = sdata[ipn + 1] = 
      (short)((isum + nsum / 2) / nsum);
  else if (type == SLICE_MODE_USHORT)
    usdata[index] = usdata[index + 1] = usdata[ipn] = usdata[ipn + 1] = 
      (unsigned short)((isum + nsum / 2) / nsum);
  else
    fdata[index] = fdata[index + 1] = fdata[ipn] = fdata[ipn + 1] = (float)(fsum / nsum);
}

// Correct pixels with either the mean or with neighboring values
static void CorrectPixels2Ways(CameraDefects *param, void *array, int type, int sizeX, 
                               int sizeY, int binning, int top, int left, int useMean, 
                               float mean)
{
  int i, xx, yy;
  for (i = 0; i < (int)param->badPixelX.size(); i++) {
    if (i < (int)param->pixUseMean.size() && param->pixUseMean[i] == useMean) {
      xx = param->badPixelX[i] / binning - left;
      yy = param->badPixelY[i] / binning - top;
      if (xx >= 0 && yy >= 0 && xx < sizeX && yy < sizeY) {
        if (param->wasScaled > 0 && binning == 1)
          CorrectSuperPixel(array, type, sizeX, sizeX, sizeY, xx, yy, useMean, mean); 
        else
          CorrectPixel(array, type, sizeX, sizeX, sizeY, xx, yy, useMean, mean);
      }
    }
  }
}

/*
 * Compute the mean surrounding a pixel above the truncation threshold, based on pixels
 * in a 7x7 square with the central 9 pixels omitted, and with any pixels above
 * truncLimit skipped
 */
float CorDefSurroundingMean(void *frame, int type, int nx, int ny, float truncLimit,
                            int ix, int iy)
{
  int halfBox = 3;
  int ixStart = B3DMAX(0, ix - halfBox);
  int ixEnd = B3DMIN(nx - 1, ix + halfBox);
  int iyStart = B3DMAX(0, iy - halfBox);
  int iyEnd = B3DMIN(ny - 1, iy + halfBox);
  int ax, ay, numPix = 0;
  float val, sum = 0.;
  for (ay = iyStart; ay <= iyEnd; ay++) {
    for (ax = ixStart; ax <= ixEnd; ax++) {
      if (B3DABS(ax - ix) > 1 || B3DABS(ay - iy) > 1) {
        switch (type) {
        case MRC_MODE_BYTE:
          val = ((unsigned char *)frame)[ax + ay * nx];
          break;
        case MRC_MODE_SHORT:
          val = ((short *)frame)[ax + ay * nx];
          break;
        case MRC_MODE_USHORT:
          val = ((unsigned short *)frame)[ax + ay * nx];
          break;
        case MRC_MODE_FLOAT:
          val = ((float *)frame)[ax + ay * nx];
          break;
        }
        if (val <= truncLimit) {
          sum += val;
          numPix++;
        }
      }
    }
  }
  return sum / B3DMAX(1, numPix);
}

///////////////////////////////////////////////////////////////
// ROUTINES FOR MANIPULATING DEFECTS STRUCTURE
///////////////////////////////////////////////////////////////

// Scale defects for a K2 up or down by AND set the flag that it has been scaled
void CorDefScaleDefectsForK2(CameraDefects *param, bool scaleDown)
{
  int i, upFac = scaleDown ? 1 : 2, downFac = scaleDown ? 2 : 1;
  int addFac = scaleDown ? 0 : 1;
  param->wasScaled = scaleDown ? -1 : 1;
  param->usableTop = param->usableTop * upFac / downFac;
  param->usableLeft = param->usableLeft * upFac / downFac;
  param->usableBottom = param->usableBottom * upFac / downFac;
  if (param->usableBottom)
    param->usableBottom += addFac;
  param->usableRight = param->usableRight * upFac / downFac;
  if (param->usableRight)
    param->usableRight += addFac;
  ScaleRowsOrColumns(param->badColumnStart, param->badColumnWidth,
    param->partialBadCol, param->partialBadWidth, param->partialBadStartY, 
    param->partialBadEndY, upFac, downFac, addFac);
  ScaleRowsOrColumns(param->badRowStart, param->badRowHeight,
    param->partialBadRow, param->partialBadHeight, param->partialBadStartX, 
    param->partialBadEndX, upFac, downFac, addFac);
  for (i = 0; i < (int)param->badPixelX.size(); i++) {
    param->badPixelX[i] = param->badPixelX[i] * upFac / downFac;
    param->badPixelY[i] = param->badPixelY[i] * upFac / downFac;
  }
}

static void ScaleRowsOrColumns(UShortVec &colStart, ShortVec &colWidth, 
                               UShortVec &partial, ShortVec &partWidth, UShortVec &startY,
                               UShortVec &endY, int upFac, int downFac, int addFac)
{
  int i;
  for (i = 0; i < (int)colStart.size(); i++) {
    colStart[i] = colStart[i] * upFac / downFac;
    colWidth[i] = colWidth[i] * upFac / downFac;
  }
  for (i = 0; i < (int)partial.size(); i++) {
    partial[i] = partial[i] * upFac / downFac;
    partWidth[i] = partWidth[i] * upFac / downFac;
    startY[i] = startY[i] * upFac / downFac;
    endY[i] = endY[i] * upFac / downFac + addFac;
  }
}

// Flip the defect list in Y, which is needed to process data that are stored right-handed
// as in IMOD.  If wasScaled is non-zero, it overrides the wasScaled setting in the param
void CorDefFlipDefectsInY(CameraDefects *param, int camSizeX, int camSizeY, int wasScaled)
{
  int pixelFlip, i, tmp, yflip = camSizeY - 1;;
  if (!wasScaled)
    wasScaled = param->wasScaled;
  pixelFlip = camSizeY - (wasScaled > 0 ? 2 : 1);
  tmp = param->usableBottom ? (yflip - param->usableBottom) : 0;
  param->usableBottom = param->usableTop ? (yflip - param->usableTop) : 0;
  param->usableTop = tmp;
  for (i = 0; i < (int)param->badRowStart.size(); i++)
    param->badRowStart[i] = yflip - (param->badRowStart[i] + param->badRowHeight[i] - 1);
  for (i = 0; i < (int)param->partialBadRow.size(); i++)
    param->partialBadRow[i] = yflip - (param->partialBadRow[i] + 
                                       param->partialBadHeight[i] - 1);
  for (i = 0; i < (int)param->partialBadCol.size(); i++) {
    tmp = yflip - param->partialBadEndY[i];
    param->partialBadEndY[i] = yflip - param->partialBadStartY[i];
    param->partialBadStartY[i] = tmp;
  }
  for (i = 0; i < (int)param->badPixelY.size(); i++)
    param->badPixelY[i] = pixelFlip - param->badPixelY[i];
}

// Adjusts a defect list for the given rotationFlip operation; specifically, if the 
// rotationFlip is not the same as the one in the defects, it either applies it if it
// is non-zero, or applies the inverse of the existing one if it is zero.  camSize are the 
// sizes that we are going to
void CorDefRotateFlipDefects(CameraDefects &defects, int rotationFlip, int camSizeX,
                             int camSizeY)
{
  CameraDefects newDef;
  int ind, xx1, xx2, yy1, yy2, operation = rotationFlip;
  if (defects.rotationFlip == rotationFlip)
    return;

  // If operation is zero, set up the inverse of the existing operation
  if (!operation) {
    operation = defects.rotationFlip;
    if (operation == 1 || operation == 3)
      operation = 4 - operation;
  }

  // Adjust points
  for (ind = 0; ind < (int)defects.badPixelX.size(); ind++) {
    xx1 = defects.badPixelX[ind];
    yy1 = defects.badPixelY[ind];

    // For super-res, need the converted coordinates of the other corner of the cluster of
    // doubled pixels, then take the minimum of the operated result
    xx2 = xx1 + (defects.wasScaled > 0 ? 1 : 0);
    yy2 = yy1 + (defects.wasScaled > 0 ? 1 : 0);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
    defects.badPixelX[ind] = B3DMIN(xx1, xx2);
    defects.badPixelY[ind] = B3DMIN(yy1, yy2);
  }

  // Process columns
  for (ind = 0; ind < (int)defects.badColumnStart.size(); ind++) {
    xx1 = defects.badColumnStart[ind];
    xx2 = xx1 + defects.badColumnWidth[ind] - 1;
    yy1 = yy2 = 0;
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
    if (operation % 2) {
      newDef.badRowStart.push_back(B3DMIN(yy1, yy2));
      newDef.badRowHeight.push_back(defects.badColumnWidth[ind]);
    } else {
      newDef.badColumnStart.push_back(B3DMIN(xx1, xx2));
      newDef.badColumnWidth.push_back(defects.badColumnWidth[ind]);
    }
  }

  // Process rows
  for (ind = 0; ind < (int)defects.badRowStart.size(); ind++) {
    yy1 = defects.badRowStart[ind];
    yy2 = yy1 + defects.badRowHeight[ind] - 1;
    xx1 = xx2 = 0;
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
    if (operation % 2) {
      newDef.badColumnStart.push_back(B3DMIN(xx1, xx2));
      newDef.badColumnWidth.push_back(defects.badRowHeight[ind]);
    } else {
      newDef.badRowStart.push_back(B3DMIN(yy1, yy2));
      newDef.badRowHeight.push_back(defects.badRowHeight[ind]);
    }
  }

  // Process partial bad columns
  for (ind = 0; ind < (int)defects.partialBadCol.size(); ind++) {
    xx1 = defects.partialBadCol[ind];
    xx2 = xx1 + defects.partialBadWidth[ind] - 1;
    yy1 = defects.partialBadStartY[ind];
    yy2 = defects.partialBadEndY[ind];
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
    if (operation % 2) {
      newDef.partialBadRow.push_back(B3DMIN(yy1, yy2));
      newDef.partialBadHeight.push_back(defects.partialBadWidth[ind]);
      newDef.partialBadStartX.push_back(B3DMIN(xx1, xx2));
      newDef.partialBadEndX.push_back(B3DMAX(xx1, xx2));
    } else {
      newDef.partialBadCol.push_back(B3DMIN(xx1, xx2));
      newDef.partialBadWidth.push_back(defects.partialBadWidth[ind]);
      newDef.partialBadStartY.push_back(B3DMIN(yy1, yy2));
      newDef.partialBadEndY.push_back(B3DMAX(yy1, yy2));
    }
  }

  // Process partial bad rows
  for (ind = 0; ind < (int)defects.partialBadRow.size(); ind++) {
    yy1 = defects.partialBadRow[ind];
    yy2 = yy1 + defects.partialBadHeight[ind] - 1;
    xx1 = defects.partialBadStartX[ind];
    xx2 = defects.partialBadEndX[ind];
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
    CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
    if (operation % 2) {
      newDef.partialBadCol.push_back(B3DMIN(xx1, xx2));
      newDef.partialBadWidth.push_back(defects.partialBadHeight[ind]);
      newDef.partialBadStartY.push_back(B3DMIN(yy1, yy2));
      newDef.partialBadEndY.push_back(B3DMAX(yy1, yy2));
    } else {
      newDef.partialBadRow.push_back(B3DMIN(yy1, yy2));
      newDef.partialBadHeight.push_back(defects.partialBadHeight[ind]);
      newDef.partialBadStartX.push_back(B3DMIN(xx1, xx2));
      newDef.partialBadEndX.push_back(B3DMAX(xx1, xx2));
    }
  }

  // Adjust usable area limits
  xx1 = defects.usableLeft;
  xx2 = defects.usableRight ? defects.usableRight : 
    (operation % 2 ? camSizeY : camSizeX);
  yy1 = defects.usableTop;
  yy2 = defects.usableBottom ? defects.usableBottom : 
    (operation % 2 ? camSizeX : camSizeY);
  CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx1, yy1);
  CorDefRotFlipCCDcoord(operation, camSizeX, camSizeY, xx2, yy2);
  defects.usableTop = B3DMIN(yy1, yy2);
  defects.usableBottom = B3DMAX(yy1, yy2);
  defects.usableLeft = B3DMIN(xx1, xx2);
  defects.usableRight = B3DMAX(xx1, xx2);

  // Assign newDef vectors to defects
  defects.badColumnStart = newDef.badColumnStart;
  defects.badColumnWidth = newDef.badColumnWidth;
  defects.badRowStart = newDef.badRowStart;
  defects.badRowHeight = newDef.badRowHeight;
  defects.partialBadCol = newDef.partialBadCol;
  defects.partialBadWidth = newDef.partialBadWidth;
  defects.partialBadStartY = newDef.partialBadStartY;
  defects.partialBadEndY = newDef.partialBadEndY;
  defects.partialBadRow = newDef.partialBadRow;
  defects.partialBadHeight = newDef.partialBadHeight;
  defects.partialBadStartX = newDef.partialBadStartX;
  defects.partialBadEndX = newDef.partialBadEndX;
  defects.rotationFlip = rotationFlip;
}

// Find all the bad pixels that touch either a bad row/column or another bad pixel
// and mark them to use the mean instead of the average of neighbors
void CorDefFindTouchingPixels(CameraDefects &defects, int camSizeX, int camSizeY,
                              int wasScaled)
{
  std::map<unsigned int, int> pointMap;
  unsigned int mapKey;
  int ind, xx, yy, xdiff, ydiff;

  if (!wasScaled)
    wasScaled = defects.wasScaled;
  xdiff = wasScaled > 0 ? 2 : 1;
  ydiff = 65536 * (wasScaled > 0 ? 2 : 1);
  
  defects.pixUseMean.clear();

  // Make a map of pixels
  for (ind = 0; ind < (int)defects.badPixelX.size(); ind++) {
    mapKey = (defects.badPixelX[ind] << 16) | defects.badPixelY[ind];
    if (!pointMap.count(mapKey))
      pointMap.insert(std::pair<unsigned int, int>(mapKey, ind));
  }

  for (ind = 0; ind < (int)defects.badPixelX.size(); ind++) {
    xx = defects.badPixelX[ind];
    yy = defects.badPixelY[ind];
    mapKey = (xx << 16) | yy;
    if ((defects.usableLeft > 0 && xx <= defects.usableLeft) ||
      (defects.usableRight > 0 && defects.usableRight < camSizeX - 1 && 
      xx >= defects.usableRight) ||
      (defects.usableTop > 0 && yy <= defects.usableTop) ||
      (defects.usableBottom > 0 && defects.usableBottom < camSizeY - 1 && 
      yy >= defects.usableBottom) ||
      CheckPointNearFullLines(xx, defects.badColumnStart, defects.badColumnWidth, xdiff) 
      || CheckPointNearFullLines(yy, defects.badRowStart, defects.badRowHeight, xdiff) ||
      CheckPointNearPartialLines(xx, yy, defects.partialBadCol, defects.partialBadWidth,
      defects.partialBadStartY, defects.partialBadEndY, xdiff) ||
      CheckPointNearPartialLines(yy, xx, defects.partialBadRow, defects.partialBadHeight,
      defects.partialBadStartX, defects.partialBadEndX, xdiff) ||
      (xx > 0 && pointMap.count(mapKey - ydiff)) || 
      (xx < camSizeX - 1 && pointMap.count(mapKey + ydiff)) ||
      (yy > 0 && pointMap.count(mapKey - xdiff)) || 
      (yy < camSizeY - 1 && pointMap.count(mapKey + xdiff))) {
        if (!defects.pixUseMean.size()) 
          defects.pixUseMean.resize(defects.badPixelX.size(), 0);
        defects.pixUseMean[ind] = 1;
    }
  }
}

static int CheckPointNearFullLines(int xx, UShortVec &columns, ShortVec &widths, 
                                   int leftDiff)
{
  for (int ind = 0; ind < (int)columns.size(); ind++)
    if (xx >= columns[ind] - leftDiff && xx <= columns[ind] + widths[ind])
      return 1;
  return 0;
}

static int CheckPointNearPartialLines(int xx, int yy, UShortVec &columns, 
                                      ShortVec &widths, UShortVec &startY, 
                                      UShortVec &endY, int leftDiff)
{
  for (int ind = 0; ind < (int)columns.size(); ind++)
    if (xx >= columns[ind] - leftDiff && xx <= columns[ind] + widths[ind] && 
      yy >= startY[ind] - leftDiff && yy <= endY[ind] + 1)
      return 1;
  return 0;
}


// Merge the defect list in properties with one from the DM system defect list, given
// the rotated camera size and the rotation/flip operation that needs to be applied to
// the system list
void CorDefMergeDefectLists(CameraDefects &defects, unsigned short *xyPairs, 
                            int numPoints, int camSizeX, int camSizeY, int rotationFlip)
{
  int ixy, xx, yy, ind, jxy, checkInd, xmin, xmax, ymin, ymax;
  IntVec inGroup;
  unsigned int mapKey, neighKey;
  std::map<unsigned int, int> pointMap;
  std::set<unsigned int> groupSet;
  std::map<unsigned int, int>::iterator mapit;
  defects.rotationFlip = rotationFlip;

  // First scan for points that are already in the list and remove them, after converting
  // them to rotated coordinate system
  for (ixy = 0; ixy < numPoints; ixy++) {
    xx = xyPairs[2 * ixy];
    yy = xyPairs[2 * ixy + 1];
    CorDefRotFlipCCDcoord(rotationFlip, camSizeX, camSizeY, xx, yy);
    xyPairs[2 * ixy] = (unsigned short)xx;
    xyPairs[2 * ixy + 1] = (unsigned short)yy;
    if ((defects.usableLeft > 0 && xx < defects.usableLeft) ||
      (defects.usableRight > 0 && xx > defects.usableRight) ||
      (defects.usableTop > 0 && yy < defects.usableTop) ||
      (defects.usableBottom > 0 && yy > defects.usableBottom) ||
      CheckIfPointInFullLines(xx, defects.badColumnStart, defects.badColumnWidth) ||
      CheckIfPointInFullLines(yy, defects.badRowStart, defects.badRowHeight) ||
      CheckIfPointInPartialLines(xx, yy, defects.partialBadCol, defects.partialBadWidth,
      defects.partialBadStartY, defects.partialBadEndY) ||
      CheckIfPointInPartialLines(yy, xx, defects.partialBadRow, defects.partialBadHeight,
      defects.partialBadStartX, defects.partialBadEndX))
          xx = xyPairs[2 * ixy] = 65535;
    for (ind = 0; ind < (int)defects.badPixelX.size() && xx < 65535; ind++)
      if (xx == defects.badPixelX[ind] && yy == defects.badPixelY[ind])
        xx = xyPairs[2 * ixy] = 65535;
    if (xx != 65535) {
      mapKey = (xx << 16) | yy; 
      if (!pointMap.count(mapKey))
        pointMap.insert(std::pair<unsigned int, int>(mapKey, 2 * ixy));
      else
        xyPairs[2 * ixy] = 65535;
    }
  }

  // Now create groups of points
  for (ixy = 0; ixy < numPoints; ixy++) {
    if (xyPairs[2 * ixy] == 65535)
      continue;

    // Initialize group with next unhandled point
    inGroup.resize(0);
    groupSet.clear();
    checkInd = 0;
    inGroup.push_back(2 * ixy);
    groupSet.insert((xx << 16) | yy);
    xmin = xmax = xyPairs[2 * ixy];
    ymin = ymax = xyPairs[2 * ixy + 1];
    while (checkInd < (int)inGroup.size()) {
      jxy = inGroup[checkInd];
      xx = xyPairs[jxy];
      yy = xyPairs[jxy + 1];
      mapKey = (xx << 16) | yy;
      neighKey = mapKey - 65536;
      if (xx > 0 && pointMap.count(neighKey) && !groupSet.count(neighKey)) {
        groupSet.insert(neighKey);
        mapit = pointMap.find(neighKey);
        inGroup.push_back(mapit->second);
      }
      neighKey = mapKey + 65536;
      if (xx < camSizeX - 1 && pointMap.count(neighKey) && !groupSet.count(neighKey)) {
        groupSet.insert(neighKey);
        mapit = pointMap.find(neighKey);
        inGroup.push_back(mapit->second);
      }
      neighKey = mapKey - 1;
      if (yy > 0 && pointMap.count(neighKey) && !groupSet.count(neighKey)) {
        groupSet.insert(neighKey);
        mapit = pointMap.find(neighKey);
        inGroup.push_back(mapit->second);
      }
      neighKey = mapKey + 1;
      if (yy < camSizeY - 1 && pointMap.count(neighKey) && !groupSet.count(neighKey)) {
        groupSet.insert(neighKey);
        mapit = pointMap.find(neighKey);
        inGroup.push_back(mapit->second);
      }

      checkInd++;
    }

    // Group is complete after all have been checked; get the min and max
    for (ind = 1; ind < (int)inGroup.size(); ind++) {
      xx = xyPairs[inGroup[ind]];
      yy = xyPairs[inGroup[ind] + 1];
      if (yy < 10) 
        jxy = 1;
      xmin = B3DMIN(xmin, xx);
      xmax = B3DMAX(xmax, xx);
      ymin = B3DMIN(ymin, yy);
      ymax = B3DMAX(ymax, yy);
    }

    // Use the longest dimension to decide whether they are rows or columns
    if (inGroup.size() == 1) {
      defects.badPixelX.push_back(xyPairs[2 * ixy]);
      defects.badPixelY.push_back(xyPairs[2 * ixy + 1]);
    } else if (ymax - ymin > xmax - xmin) {
      AddRowsOrColumns(xmin, xmax, ymin, ymax, camSizeY, xyPairs, inGroup, 0, 
        defects.badColumnStart, defects.badColumnWidth, defects.partialBadCol, 
        defects.partialBadWidth, defects.partialBadStartY, defects.partialBadEndY,
        defects.badPixelX, defects.badPixelY);
    } else {
      AddRowsOrColumns(ymin, ymax, xmin, xmax, camSizeX, xyPairs, inGroup, 1, 
        defects.badRowStart, defects.badRowHeight, defects.partialBadRow, 
        defects.partialBadHeight, defects.partialBadStartX, defects.partialBadEndX,
        defects.badPixelX, defects.badPixelY);
    }

    // Wipe the points from the list and remove from the map
    for (ind = 0; ind < (int)inGroup.size(); ind++) {
      xx = xyPairs[inGroup[ind]];
      yy = xyPairs[inGroup[ind] + 1];
      mapKey = (xx << 16) | yy;
      pointMap.erase(mapKey);
      xyPairs[inGroup[ind]] = 65535;
    }
  }

}

static int CheckIfPointInFullLines(int xx, UShortVec &columns, ShortVec &widths)
{
  for (int ind = 0; ind < (int)columns.size(); ind++)
    if (xx >= columns[ind] && xx <= columns[ind] + widths[ind] - 1)
      return 1;
  return 0;
}

static int CheckIfPointInPartialLines(int xx, int yy, UShortVec &columns, 
                                      ShortVec &widths, UShortVec &startY, 
                                      UShortVec &endY)
{
  for (int ind = 0; ind < (int)columns.size(); ind++)
    if (xx >= columns[ind] && xx <= columns[ind] + widths[ind] - 1 && yy >= startY[ind]
      && yy <= endY[ind])
      return 1;
  return 0;
}

// Add a set of bad rows or bad columns from the collection of adjacent points,
// columns where there are many fewer points than other columns and adding their points
// as single points instead
static void AddRowsOrColumns(int xmin, int xmax, int ymin, int ymax, int camSize,
                             unsigned short *xyPairs, IntVec &inGroup, int indXY,
                             UShortVec &colStart, ShortVec &colWidth, 
                             UShortVec &partial, ShortVec &partWidth, 
                             UShortVec &startY, UShortVec &endY, UShortVec &badPixelX,
                             UShortVec &badPixelY)
{
  int ind, col, oneWidth, oneYmin, oneYmax, oneXmin, maxHist = 0;
  int yy, width = xmax + 1 - xmin;
  int threshFac = 50;
  int *colHist = new int[width];
  int *colYmin = new int[width];
  int *colYmax = new int[width];

  for (ind = 0; ind < width; ind++)
    colHist[ind] = 0;

  // first build histogram of # of points in each column and accumulate ymin/ymax
  for (ind = 0; ind < (int)inGroup.size(); ind++) {
    col = xyPairs[inGroup[ind] + indXY] - xmin;
    yy = xyPairs[inGroup[ind] + 1 - indXY];
    if (colHist[col]) {
      colYmin[col] = B3DMIN(colYmin[col], yy);
      colYmax[col] = B3DMAX(colYmax[col], yy);
    } else {
      colYmin[col] = yy;
      colYmax[col] = yy;
    }
    colHist[col]++;
    maxHist = B3DMAX(maxHist, colHist[col]);
  }

  oneWidth = 0;

  // Go through the columns one by one, see if it is above or below threshold for 
  // treating as full column versus points
  for (col = 0; col < width; col++) {
    if (colHist[col] < maxHist / threshFac) {

      // Treat as points
      for (ind = 0; ind < (int)inGroup.size(); ind++) { 
        if (xyPairs[inGroup[ind] + indXY] - xmin == col) {
          badPixelX.push_back(xyPairs[inGroup[ind]]);
          badPixelY.push_back(xyPairs[inGroup[ind] + 1]);
        }
      }

      // Set up previous columns if any
      if (oneWidth) {
        AddOneRowOrColumn(oneXmin, oneXmin + oneWidth - 1, oneYmin, oneYmax, camSize, 
          colStart, colWidth, partial, partWidth, startY, endY);
        oneWidth = 0;
      }

    } else {

      // Start a new set of columns or add to them
      if (!oneWidth) {
        oneXmin = xmin + col;
        oneYmin = colYmin[col];
        oneYmax = colYmax[col];
      } else {
        oneYmin = B3DMIN(oneYmin, colYmin[col]);
        oneYmax = B3DMAX(oneYmax, colYmax[col]);
      }
      oneWidth++;
    }
  }

  // And at end, if there are columns, set them up
  if (oneWidth)
    AddOneRowOrColumn(oneXmin, oneXmin + oneWidth - 1, oneYmin, oneYmax, camSize, 
      colStart, colWidth, partial, partWidth, startY, endY);
  delete [] colHist;
  delete [] colYmin;
  delete [] colYmax;
}

// Really add one unitary set of bad rows or columns to the vectors, where camSize is
// the size in the long dimension (Y for column, X for row)
static void AddOneRowOrColumn(int xmin, int xmax, int ymin, int ymax, int camSize,
                              UShortVec &colStart, ShortVec &colWidth, 
                              UShortVec &partial, ShortVec &partWidth, 
                              UShortVec &startY, UShortVec &endY)
{
  int ind, width = xmax + 1 - xmin;

  // If the columns are full:
  if (!ymin && ymax == camSize - 1) {

    // Look for adjacent columns, if found, increase width and change start if on left
    for (ind = 0; ind < (int)colStart.size(); ind++) {
      if (xmax == colStart[ind] - 1 || xmin == colStart[ind] + colWidth[ind]) {
        colStart[ind] = B3DMIN(colStart[ind], xmin);
        colWidth[ind] += width;
        return;
      }
    }

    // Now add a new column if no adjacent one found
    colStart.push_back(xmin);
    colWidth.push_back(xmax + 1 - xmin);
  } else {

    // Add a partial column
    partial.push_back(xmin);
    partWidth.push_back(xmax + 1 - xmin);
    startY.push_back(ymin);
    endY.push_back(ymax);
  }
}

// Add one bad row/column to vectors, increasing an existing column width if one is found
void CorDefAddBadColumn(int col, UShortVec &badColumnStart, ShortVec &badColumnWidth)
{
  int ind;

  // First see if there is an existing column this is adjacent
  // to and make the column wider
  for (ind = 0; ind < (int)badColumnStart.size(); ind++) {
    if (col == badColumnStart[ind] + badColumnWidth[ind]) {
      badColumnWidth[ind]++;
      break;
    }
  }

  // Otherwise add a new entry
  if (ind == (int)badColumnStart.size()) {
    badColumnStart.push_back(col);
    badColumnWidth.push_back(1);
  }
}

// Add one partial bad row/column to the vectors
void CorDefAddPartialBadCol(int *values, UShortVec &partialBadCol, 
                            ShortVec &partialBadWidth, UShortVec &partialBadStartY, 
                            UShortVec &partialBadEndY)
{
  partialBadCol.push_back((unsigned short)values[0]);
  partialBadWidth.push_back((short)values[1]);
  partialBadStartY.push_back((unsigned short)values[2]);
  partialBadEndY.push_back((unsigned short)values[3]);
}

/////////////////////////////////////////////////////////
// CONVERSIONS BETWEEN DEFECTS STRUCTURE AND STRINGS
/////////////////////////////////////////////////////////

// Convert the defects structure to a string specification returned in strng.  Lines
// all end in newline only.
void CorDefDefectsToString(CameraDefects &defects, std::string &strng, int camSizeX,
                           int camSizeY)
{
  int ind, numOut;
  char buf[256];
  std::string strbuf;
  sprintf(buf, "CameraSizeX %d\n", camSizeX);
  strng += buf;
  sprintf(buf, "CameraSizeY %d\n", camSizeY);
  strng += buf;
  sprintf(buf, "RotationAndFlip %d\n", defects.rotationFlip);
  strng += buf;
  sprintf(buf, "WasScaled %d\n", defects.wasScaled);
  strng += buf;
  sprintf(buf, "K2Type %d\n", defects.K2Type);
  strng += buf;
  sprintf(buf, "UsableArea %d %d %d %d\n", defects.usableTop, defects.usableLeft,
    defects.usableBottom, defects.usableRight);
  strng += buf;
  for (ind = 0; ind < (int)defects.partialBadCol.size(); ind++) {
    sprintf(buf, "PartialBadColumn %d %d %d %d\n", defects.partialBadCol[ind], 
      defects.partialBadWidth[ind], defects.partialBadStartY[ind], 
      defects.partialBadEndY[ind]);
    strng += buf;
  }
  for (ind = 0; ind < (int)defects.partialBadRow.size(); ind++) {
    sprintf(buf, "PartialBadRow %d %d %d %d\n", defects.partialBadRow[ind], 
      defects.partialBadHeight[ind], defects.partialBadStartX[ind], 
      defects.partialBadEndX[ind]);
    strng += buf;
  }

  numOut = 0;
  for (ind = 0; ind < (int)defects.badPixelX.size(); ind++) {
    if (!numOut)
      strbuf = "BadPixels";
    sprintf(buf, " %d %d", defects.badPixelX[ind], defects.badPixelY[ind]);
    strbuf += buf;
    numOut++;
    if (numOut == 10 || ind == (int)defects.badPixelX.size() - 1) {
      strbuf += "\n";
      strng += strbuf;
      numOut = 0;
    }
  }

  BadRowsOrColsToString(defects.badColumnStart, defects.badColumnWidth, strng, strbuf, 
    buf, "BadColumns");
  BadRowsOrColsToString(defects.badRowStart, defects.badRowHeight, strng, strbuf, 
    buf, "BadRows");
}

// Tedious output of numerous numbers per line
static void BadRowsOrColsToString(UShortVec &starts, ShortVec &widths, std::string &strng,
                                  std::string &strbuf, char *buf, const char *name)
{
  int ind, col, numOut;
  numOut = 0;
  for (ind = 0; ind < (int)starts.size(); ind++) {
    for (col = 0; col < widths[ind]; col++) {
      if (!numOut)
        strbuf = name;
      sprintf(buf, " %d", starts[ind] + col);
      strbuf += buf;
      numOut++;
      if (numOut == 20) {
        strbuf += "\n";
        strng += strbuf;
        numOut = 0;
      }
    }
  }
  if (numOut) {
    strbuf += "\n";
    strng += strbuf;
  }
}

#define MAX_LINE 512
#define MAX_VALUES 50

// Fill a defects structure from lines in a file whose filename is in strng, or from 
// lines in strng if fromString is non-zero, returning camera size if one is found
// Lines can end in newline only or in return-newline.
int CorDefParseDefects(const char *strng, int fromString, CameraDefects &defects, 
                        int &camSizeX, int &camSizeY)
{
  char buf[MAX_LINE];
  int values[MAX_VALUES];
  FILE *fp;
  int ind, pipErr, numToGet, fullLen, nextInd, len, curInd = 0;
  bool endReached = false;
  const char *tab, *space, *nextEol;
  defects.usableTop = 0;
  defects.usableLeft = 0;
  defects.usableBottom = 0;
  defects.usableRight = 0;
  defects.rotationFlip = 0;
  camSizeX = camSizeY = 0;
  defects.wasScaled = 0;
  defects.K2Type = 0;
  defects.badColumnStart.clear();
  defects.badColumnWidth.clear();
  defects.partialBadCol.clear();
  defects.partialBadWidth.clear();
  defects.partialBadStartY.clear();
  defects.partialBadEndY.clear();
  defects.badRowStart.clear();
  defects.badRowHeight.clear();
  defects.partialBadRow.clear();
  defects.partialBadHeight.clear();
  defects.partialBadStartX.clear();
  defects.partialBadEndX.clear();
  defects.badPixelX.clear();
  defects.badPixelY.clear();
  defects.pixUseMean.clear();

  if (!fromString) {
    fp = fopen(strng, "r");
    if (!fp)
      return 1;
  } else {
    fullLen = (int)strlen(strng);
  }

  // Loop on lines from string or file
  while (true) {
    if (endReached)
      break;
    if (fromString) {

      // Process the next piece of the string, producing  same output as fgetline
      if (curInd >= fullLen) {
        len = -2;
      } else {
        nextEol = strchr(&strng[curInd], '\n');
        if (nextEol) {
          nextInd = (int)(nextEol + 1 - strng);
          len = (int)(nextEol - &strng[curInd]);
        } else {
          nextInd = fullLen;
          len = fullLen - curInd;
        }
        if (strng[curInd + len - 1] == '\r')
          len--;
        len = B3DMIN(len, MAX_LINE - 1);
        if (len)
          strncpy(buf, &strng[curInd], len);
        buf[len] = 0x00;
        curInd = nextInd;
        if (!nextEol)
          len = -(len + 2);
      }
    } else {
      len = fgetline(fp, buf, MAX_LINE);
    }

    // interpret the len value
    if (!len)
      continue;
    if (len == -2)
      break;
    if (len == -1)
      return 2;
    if (len < 0) {
      endReached = true;
      len = -len - 2;
    }

    // Find end of tag
    space = strchr(buf, ' ');
    tab = strchr(buf, '\t');
    if (buf[0] == '#')
      continue;
    if (space && tab)
      len = (int)B3DMIN(space - &buf[0], tab - &buf[0]);
    else if (space)
      len = (int)(space - &buf[0]);
    else if (tab)
      len = (int)(tab - &buf[0]);
    else
      continue;

    // Get all values on line regardless
    numToGet = 0;
    pipErr = PipGetLineOfValues(" ", &buf[len], values, 1, &numToGet, MAX_VALUES);
    
    // Check for known options
    if (!strncmp(buf, "CameraSizeX", len)) {
      if (!numToGet || pipErr)
        return 2;
      camSizeX = values[0];
    } else if (!strncmp(buf, "CameraSizeY", len)) {
      if (!numToGet || pipErr)
        return 2;
      camSizeY = values[0];
    } else if (!strncmp(buf, "RotationAndFlip", len)) {
      if (!numToGet || pipErr)
        return 2;
      defects.rotationFlip = values[0];
    } else if (!strncmp(buf, "WasScaled", len)) {
      if (!numToGet || pipErr)
        return 2;
      defects.wasScaled = values[0];
    } else if (!strncmp(buf, "K2Type", len)) {
      if (!numToGet || pipErr)
        return 2;
      defects.K2Type = values[0];
    } else if (!strncmp(buf, "UsableArea", len)) {
      if (numToGet < 4 || pipErr)
        return 2;
      defects.usableTop = values[0];
      defects.usableLeft = values[1];
      defects.usableBottom = values[2];
      defects.usableRight = values[3];
    } else if (!strncmp(buf, "PartialBadColumn", len)) {
      if (numToGet < 4 || pipErr)
        return 2;
      CorDefAddPartialBadCol(values, defects.partialBadCol, defects.partialBadWidth, 
        defects.partialBadStartY, defects.partialBadEndY);
    } else if (!strncmp(buf, "PartialBadRow", len)) {
      if (numToGet < 4 || pipErr)
        return 2;
      CorDefAddPartialBadCol(values, defects.partialBadRow, defects.partialBadHeight,
        defects.partialBadStartX, defects.partialBadEndX);
    } else if (!strncmp(buf, "BadPixels", len)) {
      if (numToGet < 2 || pipErr)
        return 2;
      for (ind = 0; ind < numToGet; ind += 2) {
        defects.badPixelX.push_back((unsigned short)values[ind]);
        defects.badPixelY.push_back((unsigned short)values[ind + 1]);
      }
    } else if (!strncmp(buf, "BadColumns", len)) {
      if (!numToGet || pipErr)
        return 2;
      for (ind = 0; ind < numToGet; ind++)
        CorDefAddBadColumn(values[ind], defects.badColumnStart, defects.badColumnWidth);
    } else if (!strncmp(buf, "BadRows", len)) {
      if (!numToGet || pipErr)
        return 2;
      for (ind = 0; ind < numToGet; ind++)
        CorDefAddBadColumn(values[ind], defects.badRowStart, defects.badRowHeight);
    }
  }
  return 0;
}

// Find edges of an image that are dark because drift correction was done without filling
// the area outside and frames that were added in.  analyzeLen is the extent to analyze 
// along each edge, maxWidth is the width to analyze.  The mean and SD are measured for
// the difference between pixels in successive lines and the median and MADN are computed
// for the ratio of mean to the SD of this difference.  critMADNs is the criterion number
// of MADNs above the median for a difference to be taken as the start of the edge that 
// needs correcting.  xLow, xHigh, yLow, and yHigh are returned with the limiting low and
// and high good coordinates in X and Y.
//
int CorDefFindDriftCorrEdges(void *array, int type, int nx, int ny, int analyzeLen,
                             int maxWidth, float critMADNs, int &xLow, int &xHigh,
                             int &yLow, int &yHigh)
{
  double diffSumSq;
  int diff, lineSum, diffSum, ix, iy, loop, idir, ind, wid, indAbove[4];
  int xStart = B3DMAX(0, nx / 2 - analyzeLen / 2);
  int yStart = B3DMAX(0, ny / 2 - analyzeLen / 2);
  int xEnd = B3DMIN(nx, xStart + analyzeLen);
  int yEnd = B3DMIN(ny, yStart + analyzeLen);
  unsigned short *usData, *usNext;
  short *sData, *sNext;
  float maxBelow, ratioMedian, ratioMADN, minAbove;
  float *lineMean, *diffMean, *diffSD, *diffRatio, *temp;
  int *xLineSums, *xDiffSums;
  double *xDiffSumSq;

  if (type != SLICE_MODE_SHORT && type != SLICE_MODE_USHORT)
    return 1;
  lineMean = B3DMALLOC(float, 4 * maxWidth);
  diffMean = B3DMALLOC(float, 4 * maxWidth);
  diffSD = B3DMALLOC(float, 4 * maxWidth);
  diffRatio = B3DMALLOC(float, 4 * maxWidth);
  xLineSums = B3DMALLOC(int, 2 * maxWidth);
  xDiffSums = B3DMALLOC(int, 2 * maxWidth);
  xDiffSumSq = B3DMALLOC(double, 2 * maxWidth);
  if (!lineMean || !diffMean || !diffSD || !xLineSums || !xDiffSums || !xDiffSumSq ||
      !diffRatio) {
    B3DFREE(lineMean);
    B3DFREE(diffMean);
    B3DFREE(diffSD);
    B3DFREE(diffRatio);
    B3DFREE(xLineSums);
    B3DFREE(xDiffSums);
    B3DFREE(xDiffSumSq);
    return 2;
  }
  temp = (float *)xDiffSumSq;
  memset(xLineSums, 0, 2 * maxWidth * sizeof(int));
  memset(xDiffSums, 0, 2 * maxWidth * sizeof(int));
  memset(xDiffSumSq, 0, 2 * maxWidth * sizeof(double));

  // Do lines at Y levels, store their results first in arrays
  for (wid = 0; wid < maxWidth; wid++) {
    for (idir = 0; idir <= 1; idir++) {
      iy = idir ? ny - 1 - wid : wid;
      lineSum = diffSum = 0;
      diffSumSq = 0.;
      if (type == SLICE_MODE_SHORT) {
        sData = (short *)array + iy * nx;
        sNext = sData + (1 - 2 * idir) * nx;
        for (ix = xStart; ix < xEnd; ix++) {
          diff = sNext[ix] - sData[ix];
          lineSum += sData[ix];
          diffSum += diff;
          diffSumSq += (double)diff * diff;
        }
      } else {
        usData = (unsigned short *)array + iy * nx;
        usNext = usData + idir * nx;
        for (ix = xStart; ix < xEnd; ix++) {
          diff = usNext[ix] - usData[ix];
          lineSum += usData[ix];
          diffSum += diff;
          diffSumSq += (double)diff * diff;
        }
      }
      ind = idir * maxWidth + wid;
      sumsToAvgSDdbl((double)diffSum, diffSumSq, 1, xEnd - xStart, &diffMean[ind],
                     &diffSD[ind]);
      lineMean[ind] = (float)lineSum / (float)(xEnd - xStart);
      diffRatio[ind] = diffMean[ind] / B3DMAX(0.1f, diffSD[ind]);
    }
  }

  // Go across lines adding to the sums for each column in X
  for (iy = yStart; iy < yEnd; iy++) {
    for (loop = 0; loop < 2; loop++) {
      idir = 1 - 2 * loop;
      ix = loop ? nx - 1: 0;
      if (type == SLICE_MODE_SHORT) {
        sData = (short *)array + iy * nx;
        for (wid = 0; wid < maxWidth; wid++) {
          ind = wid + loop * maxWidth;
          diff = sData[ix + idir] - sData[ix];
          xLineSums[ind] += sData[ix];
          xDiffSums[ind] += diff;
          xDiffSumSq[ind] += (double)diff * diff;
          ix += idir;
        }
      } else {
        usData = (unsigned short *)array + iy * nx;
        for (wid = 0; wid < maxWidth; wid++) {
          ind = wid + loop * maxWidth;
          diff = usData[ix + idir] - usData[ix];
          xLineSums[ind] += usData[ix];
          xDiffSums[ind] += diff;
          xDiffSumSq[ind] += (double)diff * diff;
          ix += idir;
        }
      }
    }
  }

  // Get the mean and SD and ratio for each column
  for (wid = 0; wid < 2 * maxWidth; wid++) {
    ind = wid + 2 * maxWidth;
    sumsToAvgSDdbl((double)xDiffSums[wid], xDiffSumSq[wid], 1, yEnd - yStart, 
      &diffMean[ind], &diffSD[ind]);
    lineMean[ind] = (float)xLineSums[wid] / (float)(yEnd - yStart);
    diffRatio[ind] = diffMean[ind] / B3DMAX(0.1f, diffSD[ind]);
  }

  // Get the overall median and MADN
  rsMedian(diffRatio, 4 * maxWidth, temp, &ratioMedian);
  rsMADN(diffRatio, 4 * maxWidth, ratioMedian, temp, &ratioMADN);
  maxBelow = -1.e10f;
  minAbove = 1.e10f;
  for (ind = 4 * maxWidth - 1; ind >= 0; ind--) {
    temp[ind] = (diffRatio[ind] - ratioMedian) / ratioMADN;
    if (temp[ind] < critMADNs)
      ACCUM_MAX(maxBelow, temp[ind]);
    else
      ACCUM_MIN(minAbove, temp[ind]);
  }
  /* printf("Last ratio deviations below and above threshold = %.2f  %.2f MADNs\n", 
     maxBelow, minAbove); */

  // Find first line, if any, where the deviation exceeds the criterion
  for (loop = 0; loop < 4; loop++) {
    indAbove[loop] = -1;
    for (wid = maxWidth - 1; wid >= 0; wid--) {
      ind = wid + loop * maxWidth;
      if (temp[ind] > critMADNs) {
        indAbove[loop] = wid;
        break;
      }
    }
    /* if (indAbove[loop] >= 0) {
      for (wid = 0; wid <= B3DMIN(maxWidth - 1, indAbove[loop] + 3); wid++) {
        ind = wid + loop * maxWidth;
        printf("%d  %2d  %7.1f  %7.1f  %7.1f  %7.2f  %7.2f\n", loop, wid, lineMean[ind],
               diffMean[ind], diffSD[ind], diffRatio[ind], temp[ind]);
               }
               } */
  }

  // Just return the limits.  Correction by partial filling is problematic and not as 
  // nice as the standard edge correction
  yLow = indAbove[0] + 1;
  yHigh = (ny - 1) - (indAbove[1] + 1);
  xLow = indAbove[2] + 1;
  xHigh = (nx - 1) - (indAbove[3] + 1);

  B3DFREE(lineMean);
  B3DFREE(diffMean);
  B3DFREE(diffSD);
  B3DFREE(diffRatio);
  B3DFREE(xLineSums);
  B3DFREE(xDiffSums);
  B3DFREE(xDiffSumSq);
  return 0;
}

///////////////////////////////////////////////////////////////////////
//  ROUTINES FOR MANIPULATING COORDINATES GIVEN ROTATION/FLIP PARAMETER
///////////////////////////////////////////////////////////////////////

// "Operation" is as defined for ProcRotateFlip: 
//     degrees CCW rotation divided by 90, plus for 4 flip before or 8 for flip after

// Convert program/user coordinates to coordinates on a chip where the given rotation/flip
// operation is needed.  This applies the inverse of the operation.
void CorDefUserToRotFlipCCD(int operation, int binning, int &camSizeX, int &camSizeY, 
                            int &imSizeX, int &imSizeY, int &top, int &left, int &bottom,
                            int &right)
{
  if (operation & 8) 
    CorDefMirrorCoords(binning, camSizeX, left, right);
  if (operation % 4 == 1) {
    CorDefRotateCoordsCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, bottom, 
      right);
  } else if (operation % 4 == 2) {
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
    CorDefMirrorCoords(binning, camSizeX, left, right);
  } else if (operation % 4 == 3) {
    CorDefRotateCoordsCCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left,
      bottom, right);
  }
  if (operation & 4) 
    CorDefMirrorCoords(binning, camSizeX, left, right);
}

// COnvert coordinates on chip needing rotation/flip back to user coordinates.  This
// applies the given operation in the forward direction
void CorDefRotFlipCCDtoUser(int operation, int binning, int &camSizeX, int &camSizeY, 
                            int &imSizeX, int &imSizeY, int &top, int &left, int &bottom,
                            int &right)
{
  if (operation & 4) 
    CorDefMirrorCoords(binning, camSizeX, left, right);
  if (operation % 4 == 1) {
    CorDefRotateCoordsCCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, 
      bottom, right);
  } else if (operation % 4 == 2) {
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
    CorDefMirrorCoords(binning, camSizeX, left, right);
  } else if (operation % 4 == 3) {
    CorDefRotateCoordsCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, bottom, 
      right);
  }
  if (operation & 8) 
    CorDefMirrorCoords(binning, camSizeX, left, right);
}

// Convert one unbinned coordinate on chip to user rotated/flipped coordinate, given the
// rotated/flipped camera size.  More generally, convert a coordinate by the given 
// operation given the camera size it is going to
void CorDefRotFlipCCDcoord(int operation, int camSizeX, int camSizeY, int &xx, int &yy)
{
  int bot = yy + 1, right = xx + 1;
  int imSizeX = camSizeX;
  int imSizeY = camSizeY;
  if (!operation)
    return;
  if (operation % 2) {
    imSizeX = camSizeY;
    imSizeY = camSizeX;
  }

  // The conversion routines are all in terms of camera acquire coordinates that go one
  // past right and bottom pixel coordinate, so we send it a 2x2 box and take the top/left
  // position of what comes back
  camSizeX = imSizeX;
  camSizeY = imSizeY;
  CorDefRotFlipCCDtoUser(operation, 1, camSizeX, camSizeY, imSizeX, imSizeY, yy, xx, bot, 
    right);
  xx = B3DMIN(xx, right);
  yy = B3DMIN(yy, bot);
}

#define SWAP_IN_TEMP(a,b,c) { \
  c = a; \
  a = b; \
  b = c; }

void CorDefMirrorCoords(int binning, int size, int &start, int &end)
{
  int temp = size / binning - start;
  start = size / binning - end;
  end = temp;
}

void CorDefRotateCoordsCW(int binning, int &camSizeX, int &camSizeY, int &imSizeX, 
                          int &imSizeY, int &top, int &left, int &bottom, int &right)
{
  int ttop = left;
  int tbot = right;
  left = camSizeY / binning - bottom;
  right = camSizeY / binning - top;
  top = ttop;
  bottom = tbot;
  SWAP_IN_TEMP(camSizeX, camSizeY, ttop);
  SWAP_IN_TEMP(imSizeX, imSizeY, ttop);
}

void CorDefRotateCoordsCCW(int binning, int &camSizeX, int &camSizeY, int &imSizeX, 
                           int &imSizeY, int &top, int &left, int &bottom, int &right)
{
  int ttop = camSizeX / binning - right;
  int tbot = camSizeX / binning - left;
  left = top;
  right = bottom;
  top = ttop;
  bottom = tbot;
  SWAP_IN_TEMP(camSizeX, camSizeY, ttop);
  SWAP_IN_TEMP(imSizeX, imSizeY, ttop);
}

// Make line pointers and get quick mean/sd for a full image, nx dimension = nx
void CorDefSampleMeanSD(void *array, int type, int nx, int ny, float *mean, float *sd)
{
  CorDefSampleMeanSD(array, type, nx, nx, ny, mean, sd);
}

// Make line pointers and get quick mean/sd for a full image, arbitrary nxdim
void CorDefSampleMeanSD(void *array, int type, int nxdim, int nx, int ny, float *mean, 
                        float *sd)
{
  CorDefSampleMeanSD(array, type, nxdim, nx, ny, 0, 0, nx, ny, mean, sd);
}

// Make line pointers and get quick mean/sd for arbitrary subarea, arbitrary nxdim
void CorDefSampleMeanSD(void *array, int type, int nxdim, int nx, int ny, int ixStart,
                        int iyStart, int nxUse, int nyUse, float *mean, float *sd)
{
  int dsize, callType;
  float sample;
  unsigned char **linePtrs;

  // Watch out for completely different definition of types in samplemeansd!
  switch (type) {
    case SLICE_MODE_BYTE:
      callType = 0;
      dsize = 1;
      break;
    case SLICE_MODE_SHORT:
    case SLICE_MODE_USHORT:
      callType = type == SLICE_MODE_USHORT ? 2 : 3;
      dsize = 2;
      break;
    case SLICE_MODE_FLOAT:
      dsize = 4;
      callType = 6;
      break;
  }
  linePtrs = makeLinePointers(array, nxdim, ny, dsize);
  if (!linePtrs)
    return;
  sample = (float)B3DMIN(1., 30000. / (nx * ny));
  sampleMeanSD(linePtrs, callType, nx, ny, sample, ixStart, iyStart, nxUse, nyUse, mean,
    sd);
  free(linePtrs);
}

int CorDefSetupToCorrect(int nxFull, int nyFull, CameraDefects &defects, int &camSizeX,
                       int &camSizeY, int scaleDefects, float setBinning, int &useBinning,
                       const char *binOpt)
{
  char bintext[8];
  bool scaledForK2;

  // See if the defects need to be scaled up: a one-time error or action
  if (nxFull > camSizeX * 2 || nyFull > camSizeY * 2)
    return 1;
  if (nxFull > camSizeX || nyFull > camSizeY || 
      (scaleDefects && defects.wasScaled <= 0)) {
    if (!(scaleDefects && defects.wasScaled <= 0) && binOpt)
      std::cout << "Scaling defect list up by 2 because images are larger than camera "
        "size in list\n" << std::endl;
    CorDefScaleDefectsForK2(&defects, 0);
    camSizeX *= 2;
    camSizeY *= 2;
  }
  scaledForK2 = defects.K2Type > 0 && defects.wasScaled > 0;

  // Now deduce the binning; this could be superceded by an option if needed
  // Increase it as long as image still fits within defect camera size
  if (setBinning <= 0) {
    useBinning = 1;
    while (nxFull * (useBinning + 1) <= camSizeX && 
           nyFull * (useBinning + 1) <= camSizeY) {
      useBinning += 1;
    }
    if (useBinning > 1) {
      if (scaledForK2)
        sprintf(bintext, "%.1f", useBinning / 2.);
      else
        sprintf(bintext, "%d", useBinning);
      if (binOpt)
        std::cout << "Assuming binning of " << bintext << " instead of a small subarea;" 
                  << std::endl << "    use the " << binOpt 
                  << " option to set a binning if this is" " incorrect." << std::endl;
    }
  } else {
    useBinning = B3DNINT((scaledForK2 ? 2 : 1) * setBinning + 0.02);
  }
  return 0;
}
