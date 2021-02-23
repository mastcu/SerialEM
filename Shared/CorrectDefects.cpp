// CorrectDefects.cpp:  Functions for defect correction, management of a structure 
//                      specifying corrections, and some general image coordinate 
//                      conversion routines
//
// Depends on IMOD library libcfshr (uses parse_params.c and samplemeansd.c)
// This module is shared between IMOD and SerialEM and is thus GPL.
//
// Copyright (C) 2014-2019 by the Regents of the University of Colorado.
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
#include "mxmlwrap.h"
#include "iimage.h"

// Local functions
static void ScaleRowsOrColumns(UShortVec &colStart, ShortVec &colWidth, 
                               UShortVec &partial, ShortVec &partWidth, UShortVec &startY,
                               UShortVec &endY, int upFac, int downFac, int addFac);
static void ScaleDefectsByFactors(CameraDefects *param, int upFac, int downFac, 
                                  int addFac);
static void CorrectEdge(void *array, int type, int numBad, int taper, int length,
                        int sumLength, int indStart, int stepAlong, int stepBetween);
static int RandomIntFillFromIntSum(int isum, int nsum);
static int RandomIntFillFromFloat(float value);
static void CorrectColumn(void *array, int type, int nx, int ny, int xstride, int ystride,
                          int indStart, int num, int ystart, int yend, int superFac,
                          int numAvgSuper);
static void CorrectPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                         int ypix, int useMean, float mean);
static void CorrectSuperPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                              int ypix, int useMean, float mean);
static void CorrectJumboPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                              int ypix, int useMean, float mean);
static void CorrectPixels3Ways(CameraDefects *param, void *array, int type, int sizeX, 
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
static void clearDefectList(CameraDefects &defects);

/////////////////////////////////////////////////////////////
// ACTUAL DEFECT CORRECTION ROUTINES
/////////////////////////////////////////////////////////////

// Correct edge and column defects in an image: the master routine
// Coordinates are binned camera acquisition coordinates (top < bottom) where the
// image runs from left to right -1, top to bottom - 1, inclusive
void CorDefCorrectDefects(CameraDefects *param, void *array, int type, 
                        int binning, int top, int left, int bottom, int right)
{
  int firstBad, numBad, i, badStart, badEnd, yStart, yEnd, superFac = 0;
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
  
  if (param->FalconType && param->wasScaled == 1)
    superFac = 2;
  if (param->FalconType && param->wasScaled == 2)
    superFac = 4;

  // If there are any pixels that should use mean, get the mean and correct them first
  if (param->pixUseMean.size()) {
    CorDefSampleMeanSD(array, type, sizeX, sizeY, &mean, &SD);
    CorrectPixels3Ways(param, array, type, sizeX, sizeY, binning, top, left, 1, mean);
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

    CorrectColumn(array, type, sizeX, sizeY, 1, sizeX, badStart - left, 
                  badEnd + 1 - badStart, 0, sizeY - 1, superFac, param->numAvgSuperRes);
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
      CorrectColumn(array, type, sizeX, sizeY, 1, sizeX, badStart - left, 
                    badEnd + 1 - badStart, yStart, yEnd, superFac,param->numAvgSuperRes); 
    }
  }

  // Correct row defects
  for (i = 0; i < (int)param->badRowStart.size(); i++) {
  
    // convert starting and ending columns to columns in binned image
    // to get starting column and width
    badStart = param->badRowStart[i] / binning;
    badEnd = (param->badRowStart[i] + param->badRowHeight[i] - 1) / binning;

    CorrectColumn(array, type, sizeY, sizeX, sizeX, 1, badStart - top, 
                  badEnd + 1 - badStart, 0, sizeX - 1, superFac, param->numAvgSuperRes);
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
      CorrectColumn(array, type, sizeY, sizeX, sizeX, 1, badStart - top, 
                    badEnd + 1 - badStart, yStart, yEnd, superFac,param->numAvgSuperRes); 
    }
  }

  // Correct bad pixels with neighboring values
  CorrectPixels3Ways(param, array, type, sizeX, sizeY, binning, top, left, 0, 0.);
}

// Macro to get initial sum at beginning of last good row
#define BEGINNING_ROW_SUM(swty, data)                                   \
  case swty:                                                            \
  for (i = 0, ind = indStart; i < sumLength; i++, ind += stepAlong)     \
    sum += data[ind];                                                   \
  break;                                                                \
  
// Macro to correct a whole edge row
#define CORRECT_EDGE_ROW(swty, data, typ, csty, func)                   \
  case swty:                                                            \
  for (i = 0, ind = indStart; i < length; i++, ind += stepAlong, indDest += stepAlong) { \
    if (i >= addStart && i < addEnd) {                                  \
      sum += (csty)data[indAdd] - data[indDrop];                        \
      indAdd += stepAlong;                                              \
      indDrop += stepAlong;                                             \
    }                                                                   \
    data[indDest] = (typ)func(frac * (float)data[ind] + (float)(sumFac * sum)); \
  }                                                                     \
  break;


// Correct one edge of an image
static void CorrectEdge(void *array, int type, int numBad, int taper, int length,
              int sumLength, int indStart, int stepAlong, int stepBetween)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  double sum, sumFac;
  float frac;
  int ind, i, indDest, row, indDrop, indAdd, addStart, addEnd;

  if (sumLength > length)
    sumLength = length;
  addStart = sumLength / 2;
  addEnd = length - (sumLength - sumLength / 2);

  for (row = 0; row < numBad; row++) {
    
    // First get the mean at beginning of the last good row
    sum = 0;
    switch (type) {
      BEGINNING_ROW_SUM(SLICE_MODE_BYTE, bdata);
      BEGINNING_ROW_SUM(SLICE_MODE_SHORT, sdata);
      BEGINNING_ROW_SUM(SLICE_MODE_USHORT, usdata);
      BEGINNING_ROW_SUM(SLICE_MODE_FLOAT, fdata);
    }
    indDrop = indStart;
    indAdd = ind;

    // For the row, set the index to place pixels, compute fraction if tapering
    indDest = indStart + (row + 1) * stepBetween;
    frac = 1.;
    sumFac = 0.;
    if (taper > 0) {
      frac = (float)((taper - row - 1.) / taper);
      if (frac < 0.)
        frac = 0.;
      sumFac = (1. - frac) / sumLength;
    }
    
    // Replace the row
    switch (type) {
      CORRECT_EDGE_ROW(SLICE_MODE_BYTE, bdata, unsigned char, int,
                       RandomIntFillFromFloat);
      CORRECT_EDGE_ROW(SLICE_MODE_SHORT, sdata, short, int, RandomIntFillFromFloat);
      CORRECT_EDGE_ROW(SLICE_MODE_USHORT, usdata, unsigned short, int, 
                       RandomIntFillFromFloat);
      CORRECT_EDGE_ROW(SLICE_MODE_FLOAT, fdata, float, float, +);
    }
  }
}

/*  For debugging the 5+ column correction
void  PrintfToLog(char *fmt, ...);
if (i == fullStart + 5)
PrintfToLog("col %d  pseudo %d  vary %f flUse %f %d %d %d %d", col,
pseudo & 0x3FFFF, ranFac  * (pseudo & 0x3FFFF) - maxVary, flUse, iy1, ifx1, iy2,
ifx2);*/
/* A note on the linear congruential generator:
  The full 20 bit value had a bad bias to higher values, 20% more than half in small
  sample.  The 18-bit value used here had a 3% bias to lower half in two samples of 
  320-384 values.  The bits picked out for generating the random int fill and the 
  index factors for 5+ row correction are fairly unbiased.
  GPUtest2018/Dec06_16.02.21.tif has a mean of 0.0617 and middle mean of 0.0624 
  corrected bad pixels averaged 0.0597, central 2-row correction averaged 0.0631
  GPUtest2018/Dec27_13.33.00.tif has a mean of 0.905 and middle mean of 0.930
  corrected bad pixels averaged 0.893, central 2-row correction averaged 0.934
  The row results have lots of samples and may reflect the bias in the generator.
  Verified that >= in the comparisons gives much bigger result; > is correct
 */

// Generate an int value that will average out to the floating point value of isum / nsum
// by comparison or remainder with reaminder from a random number
static int RandomIntFillFromIntSum(int isum, int nsum)
{
  int ranInt, retval;
  static int pseudo = 482945;
  pseudo = (197 * (pseudo + 1)) & 0xFFFFF;
  ranInt = (pseudo >> 2) % nsum;
  retval = isum / nsum;
  if (isum % nsum > ranInt)
    retval++;
  return retval;
}

// Generate an int value from the given float that will average out to the float value
// by comparison with a random number
static int RandomIntFillFromFloat(float value)
{
  int retval;
  static int pseudo = 843295;
  pseudo = (197 * (pseudo + 1)) & 0xFFFFF;
  retval = (int)value;
  if ((value - retval) * (float)0x3FFFF > (pseudo & 0x3FFFF))
    retval++;
  return retval;
}


// Macros for computing the column correction, with summing of one or two rows of
// edge values for thicker columns
// Arguments are MRC type, array, type for cast assignment, [type for cast to start sum],
// either RandomIntFillFromFloat or "+" as a no-op
#define CORRECT_ONE_TWO_COL(c, a, b, e)                                 \
  case c:                                                               \
  for (i = ystart; i <= yend; i++, ind += yStride, indLeft += yStride,  \
         indRight += yStride) {                                         \
    fill = (fLeft * (float)a[indLeft] + fRight * (float)a[indRight]);   \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  break;

#define CORRECT_THREE_FOUR_COL(c, a, b, d, e)                           \
  case c:                                                               \
  if (ystart < fullStart) {                                             \
    fill = ((fLeft * ((d)a[indLeft] + a[indLeft + yStride]) +           \
             fRight * ((d)a[indRight] + a[indRight + yStride])) / 2.f); \
    a[ind] = (b)e(fill);                                                \
    ind += yStride;                                                     \
    indLeft += yStride;                                                 \
    indRight += yStride;                                                \
  }                                                                     \
  for (i = fullStart; i <= fullEnd; i++, ind += yStride, indLeft += yStride, \
         indRight += yStride) {                                         \
    fill = ((fLeft * (float)((d)a[indLeft - yStride] + a[indLeft] +     \
                             a[indLeft + yStride]) +                    \
             fRight * (float)((d)a[indRight - yStride] + a[indRight] +  \
                              a[indRight + yStride])) / 3.f);           \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  if (yend > fullEnd) {                                                 \
    fill = ((fLeft * ((d)a[indLeft - yStride] + a[indLeft]) +           \
             fRight * ((d)a[indRight - yStride] + a[indRight])) / 2.f); \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  break;

// This one uses up to 16 columns x 15 rows on one side to sample pixels with random
// indexes, and randomly alternates between the two sides.  
#define CORRECT_FIVE_PLUS_COL(c, a, b, d, e)                            \
  case c:                                                               \
  for (i = ystart; i < fullStart; i++, ind += yStride, indLeft += yStride, \
         indRight += yStride) {                                         \
    fill = ((fLeft * ((d)a[indLeft] + a[indLeft + yStride] +            \
                      a[indLeft - xStride] + a[indLeft + yStride - xStride]) + \
             fRight * ((d)a[indRight] + a[indRight + yStride] +         \
                       a[indRight + xStride] +                          \
                       a[indRight + yStride + xStride])) / 4.f);        \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  for (i = fullStart; i <= fullEnd; i++, ind += yStride, indLeft += yStride, \
         indRight += yStride) {                                         \
    pseudo = (197 * (pseudo + 1)) & 0xFFFFF;                            \
    iy1 = (pseudo >> 2) % 15;                                           \
    ifx1 = (pseudo >> 6) & 15;                                          \
    if (pseudo & 2048)                                                  \
      fill = a[indLeft + (iy1 - 7) * yStride - ifx1 * xStride];         \
    else                                                                \
      fill = a[indRight + (iy1 - 7) * yStride + ifx1 * xStride];         \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  for (i = fullEnd + 1; i <= yend; i++, ind += yStride, indLeft += yStride, \
         indRight += yStride) {                                         \
    fill = ((fLeft * ((d)a[indLeft - yStride] + a[indLeft] +            \
                      a[indLeft - xStride - yStride] + a[indLeft - xStride]) + \
             fRight * ((d)a[indRight - yStride] + a[indRight] +         \
                       a[indRight + xStride - yStride] +                \
                       a[indRight + xStride])) / 4.f);                  \
    a[ind] = (b)e(fill);                                                \
  }                                                                     \
  break;

// Get the sum of the 2 or 4 pixels across the line and distribute the mean evenly
// That is it for floats, but there is a remainder for the rest
#define CAC_SUM_TWO(typ, sum, dat, men)                                 \
  case typ:                                                             \
  sum = dat[ind] + dat[ind + xStride];                                  \
  men = sum / 2;                                                        \
  dat[ind] = men;                                                       \
  dat[ind + xStride] = men;                                             \
  break;

#define CAC_SUM_FOUR(typ, sum, dat, men)                                \
  case typ:                                                             \
  sum = dat[ind] + dat[ind + xStride] + dat[ind + 2 * xStride] +        \
    dat[ind + 3 * xStride];                                             \
  men = sum / 4;                                                        \
  dat[ind] = men;                                                       \
  dat[ind + xStride] = men;                                             \
  dat[ind + 2 * xStride] = men;                                         \
  dat[ind + 3 * xStride] = men;                                         \
  break;

// Randomly choose which pixel gets the remainder
#define CAC_ADD_ONE_REM(typ, dat)                                       \
  case typ:                                                             \
  pseudo = (197 * (pseudo + 1)) & 0xFFFFF;                              \
  ifx1 = (pseudo >> 2) & 1;                                             \
  dat[ind + ifx1 * xStride]++;                                          \
  break;

#define CAC_ADD_REMAINDER(typ, dat)                                     \
  case typ:                                                             \
  for (i = 0; i < irem; i++) {                                          \
    pseudo = (197 * (pseudo + 1)) & 0xFFFFF;                            \
    ifx1 = (pseudo >> 2) & 3;                                           \
    dat[ind + ifx1 * xStride]++;                                        \
  }                                                                     \
  break;


// Correct a column defect which can be multiple columns
static void CorrectColumn(void *array, int type, int nx, int ny, int xStride, int yStride,
                          int indStart, int num, int ystart, int yend, int superFac,
                          int numAvgSuper)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  float fLeft, fRight, fill, fsum, fmean;
  static int pseudo = 456789;
  bool fivePlusOK;
  int ind, i, col, indLeft, indRight, fullStart, fullEnd, ifx1, iy1;
  int irem, isum, imean, nloop, loop;
  float maxVary = 0.33f;
  float ranFac = maxVary / (float)0x1FFFF;
  int sideStarts[2 * MAX_AVG_SUPER_RES];
  
  // Adjust indStart or num if on edge of image; return if nothing left
  if (indStart < 0) {
    num += indStart;
    indStart = 0;
  }

  if (indStart + num > nx)
    num = nx - indStart;

  if (num <= 0)
    return;

  // Remove super-resolution in pixels outside the columns perpendicular to the column
  if (superFac > 0) {
    nloop = 0;
    for (i = 0; i < numAvgSuper; i++) {
      indLeft = indStart - (i + 1) * superFac;
      if (indLeft >= 0)
        sideStarts[nloop++] = indLeft;
      indLeft = indStart + num + i * superFac;
      if (indLeft < nx)
        sideStarts[nloop++] = indLeft;
    }

    for (loop = 0; loop < nloop; loop++) {
      indLeft = sideStarts[loop];
      if (superFac == 2) {
        for (iy1 = ystart; iy1 <= yend; iy1++) {
          ind = indLeft * xStride + iy1 * yStride;
          switch (type) {
            CAC_SUM_TWO(SLICE_MODE_BYTE, isum, bdata, imean);
            CAC_SUM_TWO(SLICE_MODE_SHORT, isum, sdata, imean);
            CAC_SUM_TWO(SLICE_MODE_USHORT, isum, usdata, imean);
            CAC_SUM_TWO(SLICE_MODE_FLOAT, fsum, fdata, fmean);
          }
          if (type != SLICE_MODE_FLOAT) {
            if (isum % 2) {
              switch (type) {
                CAC_ADD_ONE_REM(SLICE_MODE_BYTE, bdata);
                CAC_ADD_ONE_REM(SLICE_MODE_SHORT, sdata);
                CAC_ADD_ONE_REM(SLICE_MODE_USHORT, usdata);
              }
            }
          }
        }
      } else {
        for (iy1 = ystart; iy1 <= yend; iy1++) {
          ind = indLeft * xStride + iy1 * yStride;
          switch (type) {
            CAC_SUM_FOUR(SLICE_MODE_BYTE, isum, bdata, imean);
            CAC_SUM_FOUR(SLICE_MODE_SHORT, isum, sdata, imean);
            CAC_SUM_FOUR(SLICE_MODE_USHORT, isum, usdata, imean);
            CAC_SUM_FOUR(SLICE_MODE_FLOAT, fsum, fdata, fmean);
          }
          if (type != SLICE_MODE_FLOAT) {
            irem = isum % 4;
            switch (type) {
              CAC_ADD_REMAINDER(SLICE_MODE_BYTE, bdata);
              CAC_ADD_REMAINDER(SLICE_MODE_SHORT, sdata);
              CAC_ADD_REMAINDER(SLICE_MODE_USHORT, usdata);
            }
          }
        }
      }
    }
  }

  for (col = 0; col < num; col++) {
    
    // Set up fractions on left and right columns
    fRight = (float)((col + 1.) / (num + 1.));
    fLeft = 1.f - fRight;

    // Set up indexes for left and right columns; just use one side if on edge
    indLeft = indStart - 1;
    indRight = indStart + num;
    if (indLeft < 0)
      indLeft = indRight;
    if (indRight >= nx)
      indRight = indLeft;
    fivePlusOK = indLeft > 14 && indRight < nx - 15;
    indLeft *= xStride;
    indRight *= xStride;
    ind = (indStart + col) * xStride;
    ind += yStride * ystart;
    indLeft += yStride * ystart;
    indRight += yStride * ystart;
    fullStart = ystart;
    fullEnd = yend;
    if (ystart < 7)
      fullStart += 7 - ystart;
    if (yend >= ny - 7)
      fullEnd -= yend + 8 - ny;

    if (num >= 3 && yend - ystart >= 15 && fivePlusOK) {
      switch (type) {
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_BYTE, bdata, unsigned char, int, +);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_SHORT, sdata, short int, int, +);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_USHORT, usdata, unsigned short int, int, +);
        CORRECT_FIVE_PLUS_COL(SLICE_MODE_FLOAT, fdata, float, float, +);
      }
    } else if (num >= 3 && yend - ystart >= 1) {
      switch (type) {
        CORRECT_THREE_FOUR_COL(SLICE_MODE_BYTE, bdata, unsigned char, int,
                               RandomIntFillFromFloat);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_SHORT, sdata, short int, int,
                               RandomIntFillFromFloat);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_USHORT, usdata, unsigned short int, int,
                               RandomIntFillFromFloat);
        CORRECT_THREE_FOUR_COL(SLICE_MODE_FLOAT, fdata, float, float, +);
      }
    } else {
      switch (type) {
        CORRECT_ONE_TWO_COL(SLICE_MODE_BYTE,  bdata, unsigned char,
                            RandomIntFillFromFloat);
        CORRECT_ONE_TWO_COL(SLICE_MODE_SHORT, sdata, short int,
                            RandomIntFillFromFloat);
        CORRECT_ONE_TWO_COL(SLICE_MODE_USHORT, usdata, unsigned short int,
                            RandomIntFillFromFloat);
        CORRECT_ONE_TWO_COL(SLICE_MODE_FLOAT, fdata, float, +);
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
      bdata[index] = (unsigned char)RandomIntFillFromFloat(mean);
    else if (type == SLICE_MODE_SHORT)
      sdata[index] = (short)RandomIntFillFromFloat(mean);
    else if (type == SLICE_MODE_USHORT)
      usdata[index] = (unsigned short)RandomIntFillFromFloat(mean);
    else
      fdata[index] = mean;
    return;
  }

  // Average 4 pixels around the point if they all exist
  if (xpix > 0 && xpix < nx - 1 && ypix > 0 && ypix < ny - 1) {
    switch (type) {
    case SLICE_MODE_BYTE:
      bdata[index] = (unsigned char)RandomIntFillFromIntSum((int)bdata[index - 1] + 
        bdata[index + 1] + bdata[index - nxdim] + bdata[index + nxdim], 4);
      break;
    case SLICE_MODE_SHORT:
      sdata[index] = (short)RandomIntFillFromIntSum((int)sdata[index - 1] + 
        sdata[index + 1] + sdata[index - nxdim] + sdata[index + nxdim], 4);
      break;
    case SLICE_MODE_USHORT:
      usdata[index] = (unsigned short)RandomIntFillFromIntSum((int)usdata[index - 1] + 
        usdata[index + 1] + usdata[index - nxdim] + usdata[index + nxdim], 4);
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
    bdata[index] = (unsigned char)RandomIntFillFromIntSum(isum, nsum);
  else if (type == SLICE_MODE_SHORT)
    sdata[index] = (short)RandomIntFillFromIntSum(isum, nsum);
  else if (type == SLICE_MODE_USHORT)
    usdata[index] = (unsigned short)RandomIntFillFromIntSum(isum, nsum);
  else
    fdata[index] = (float)(fsum / nsum);
}

// Macro to correct all 4 pixel with the same value from a sum of 16 surrounding ones
#define CORRECT_FOUR_PIXELS(swty, data, typ)                            \
  case swty:                                                            \
  isum = ((int)data[index - 1] + data[index - 2] + data[ipn - 1] + data[ipn - 2] + \
          data[index + 2] + data[index + 3] + data[ipn + 2] + data[ipn + 3] + \
          data[imn] + data[imn + 1] + data[im2n] + data[im2n + 1] +     \
          data[ip2n] + data[ip2n + 1] + data[ip3n] + data[ip3n + 1]);   \
  isum = RandomIntFillFromIntSum(isum, 16);                             \
  data[index] = data[index + 1] = data[ipn] = data[ipn + 1] = (typ)isum; \
  break;


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
        (unsigned char)RandomIntFillFromFloat(mean);
    else if (type == SLICE_MODE_SHORT)
      sdata[index] = sdata[index + 1] = sdata[ipn] = sdata[ipn + 1] = 
        (short)RandomIntFillFromFloat(mean);
    else if (type == SLICE_MODE_USHORT)
      usdata[index] = usdata[index + 1] = usdata[ipn] = usdata[ipn + 1] = 
        (unsigned short)RandomIntFillFromFloat(mean);
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
      CORRECT_FOUR_PIXELS(SLICE_MODE_BYTE, bdata, unsigned char);
      CORRECT_FOUR_PIXELS(SLICE_MODE_SHORT, sdata, short);
      CORRECT_FOUR_PIXELS(SLICE_MODE_USHORT, usdata, unsigned short);
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
      if (type == SLICE_MODE_BYTE) 
        isum += bdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_SHORT)
        isum += sdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_USHORT)
        isum += usdata[ix + iy * nxdim];
      else
        fsum += fdata[ix + iy * nxdim];
    }
  }
  
  if (type == SLICE_MODE_BYTE)
    bdata[index] = bdata[index + 1] = bdata[ipn] = bdata[ipn + 1] = 
      (unsigned char)RandomIntFillFromIntSum(isum, nsum);
  else if (type == SLICE_MODE_SHORT)
    sdata[index] = sdata[index + 1] = sdata[ipn] = sdata[ipn + 1] = 
      (short)RandomIntFillFromIntSum(isum, nsum);
  else if (type == SLICE_MODE_USHORT)
    usdata[index] = usdata[index + 1] = usdata[ipn] = usdata[ipn + 1] = 
      (unsigned short)RandomIntFillFromIntSum(isum, nsum);
  else
    fdata[index] = fdata[index + 1] = fdata[ipn] = fdata[ipn + 1] = (float)(fsum / nsum);
}

// Macro to correct 16 pixels with random values from a sum of 64 surrounding ones
#define CORRECT_JUMBO_PIXEL(swty, data, typ)                            \
  case swty:                                                            \
  for (i = 0 ; i < 64; i++) {                                           \
    ix = xpix + idx[i];                                                 \
    iy = ypix + idy[i];                                                 \
    isum += data[ix + iy * nxdim];                                      \
  }                                                                     \
  for (iy = 0; iy < 4; iy++)                                            \
    for (ix = 0; ix < 4; ix++)                                          \
      data[index + ix + iy * nxdim] =                                   \
        (typ)RandomIntFillFromIntSum(isum, 64);                         \
  break;


// Correct a single-pixel defect in super-res x4 image
static void CorrectJumboPixel(void *array, int type, int nxdim, int nx, int ny, int xpix, 
                      int ypix, int useMean, float mean)
{
  short int *sdata = (short int *)array;
  unsigned short int *usdata = (unsigned short int *)array;
  float *fdata = (float *)array;
  unsigned char *bdata = (unsigned char *)array;
  int jdx[4] = {-4, 4, 0, 0};
  int jdy[4] = {0, 0, -4, 4};
  int idx[64], idy[64];
  int isum = 0, nsum = 0;
  float fsum = 0.f;
  int ix, iy, jx, jy, i, index = xpix + ypix * nxdim;

  if (useMean) {
    if (type == SLICE_MODE_BYTE) {
      for (iy = 0; iy < 4; iy++)
        for (ix = 0; ix < 4; ix++)
          bdata[index + ix + iy * nxdim] = (unsigned char)RandomIntFillFromFloat(mean);
    } else if (type == SLICE_MODE_SHORT) {
      for (iy = 0; iy < 4; iy++)
        for (ix = 0; ix < 4; ix++)
          sdata[index + ix + iy * nxdim] = (short)RandomIntFillFromFloat(mean);
    } else if (type == SLICE_MODE_USHORT) {
      for (iy = 0; iy < 4; iy++)
        for (ix = 0; ix < 4; ix++)
          usdata[index + ix + iy * nxdim] = (unsigned short)RandomIntFillFromFloat(mean);
    } else {
      for (iy = 0; iy < 4; iy++)
        for (ix = 0; ix < 4; ix++)
          fdata[index + ix + iy * nxdim] = mean;
    }
    return;
  }


  // Set up deltas
  ix = 0;
  for (i = 0 ; i < 4; i++) {
    for (jy = 0; jy < 4; jy++) {
      for (jx = 0; jx < 4; jx++) {
        idx[ix] = jdx[i] + jx;
        idy[ix++] = jdy[i] + jy;
      }
    }
  }

  
  // Average 4 chip pixels around the point if they all exist
  if (xpix > 5 && xpix < nx - 7 && ypix > 5 && ypix < ny - 7) {
    switch (type) {
      CORRECT_JUMBO_PIXEL(SLICE_MODE_BYTE, bdata, unsigned char);
      CORRECT_JUMBO_PIXEL(SLICE_MODE_SHORT, sdata, short);
      CORRECT_JUMBO_PIXEL(SLICE_MODE_USHORT, usdata, unsigned short);
    case SLICE_MODE_FLOAT:
      for (i = 0 ; i < 64; i++) {
        ix = xpix + idx[i];
        iy = ypix + idy[i];
        fsum += fdata[ix + iy * nxdim];
      }
      fsum /= 64.;
      for (iy = 0; iy < 4; iy++)
        for (ix = 0; ix < 4; ix++)
          fdata[index + ix + iy * nxdim] = fsum;
      break;
    }
    return;
  }
  
  // Or average whatever is there
  for (i = 0 ; i < 64; i++) {
    ix = xpix + idx[i];
    iy = ypix + idy[i];
    if (ix >= 0 && ix < nx && iy >= 0 && iy < ny) {
      nsum++;
      if (type == SLICE_MODE_BYTE) 
        isum += bdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_SHORT)
        isum += sdata[ix + iy * nxdim];
      else if (type == SLICE_MODE_USHORT)
        isum += usdata[ix + iy * nxdim];
      else
        fsum += fdata[ix + iy * nxdim];
    }
  }
  
  if (type == SLICE_MODE_BYTE) {
    for (iy = 0; iy < 4; iy++)
      for (ix = 0; ix < 4; ix++)
        bdata[index + ix + iy * nxdim] =
          (unsigned char)RandomIntFillFromIntSum(isum, nsum);
  } else if (type == SLICE_MODE_SHORT) {
    for (iy = 0; iy < 4; iy++)
      for (ix = 0; ix < 4; ix++)
        sdata[index + ix + iy * nxdim] = (short)RandomIntFillFromIntSum(isum, nsum);
  } else if (type == SLICE_MODE_USHORT) {
    for (iy = 0; iy < 4; iy++)
      for (ix = 0; ix < 4; ix++)
        usdata[index + ix + iy * nxdim] =
          (unsigned short)RandomIntFillFromIntSum(isum, nsum);
  } else {
    for (iy = 0; iy < 4; iy++)
      for (ix = 0; ix < 4; ix++)
        fdata[index + ix + iy * nxdim] = (float)(fsum / nsum);
  }
}

// Correct pixels with either the mean or with neighboring values
static void CorrectPixels3Ways(CameraDefects *param, void *array, int type, int sizeX, 
                               int sizeY, int binning, int top, int left, int useMean, 
                               float mean)
{
  int i, xx, yy;
  for (i = 0; i < (int)param->badPixelX.size(); i++) {
    if ((i < (int)param->pixUseMean.size() && param->pixUseMean[i] == useMean) ||
        (i >= (int)param->pixUseMean.size() && !useMean)) {
      xx = param->badPixelX[i] / binning - left;
      yy = param->badPixelY[i] / binning - top;
      if (xx >= 0 && yy >= 0 && xx < sizeX && yy < sizeY) {
        if (param->wasScaled > 1)
          CorrectJumboPixel(array, type, sizeX, sizeX, sizeY, xx, yy, useMean, mean); 
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
  int upFac = scaleDown ? 1 : 2, downFac = scaleDown ? 2 : 1;
  int addFac = scaleDown ? 0 : 1;
  param->wasScaled = scaleDown ? -1 : 1;
  ScaleDefectsByFactors(param, upFac, downFac, addFac);
}

// Scale defects for a Falcon up by 2 or 4, or down for negative factors, AND set the
// flag that it has been scaled. Factor MUST be -8, -4, -2, 2, or 4
void CorDefScaleDefectsForFalcon(CameraDefects *param, int factor)
{
  param->wasScaled = factor / 2;
  if (factor > 0)
    ScaleDefectsByFactors(param, factor, 1, factor - 1);
  else
    ScaleDefectsByFactors(param, 1, -factor, 0);
}

// Scaling defects given the up and down factors
static void ScaleDefectsByFactors(CameraDefects *param, int upFac, int downFac, 
                                  int addFac)
{
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
  for (int i = 0; i < (int)param->badPixelX.size(); i++) {
    param->badPixelX[i] = param->badPixelX[i] * upFac / downFac;
    param->badPixelY[i] = param->badPixelY[i] * upFac / downFac;
  }
}

// Scale a row or column specification given the up and down factors
static void ScaleRowsOrColumns(UShortVec &colStart, ShortVec &colWidth, 
                               UShortVec &partial, ShortVec &partWidth, UShortVec &startY,
                               UShortVec &endY, int upFac, int downFac, int addFac)
{
  int i;
  for (i = 0; i < (int)colStart.size(); i++) {
    colStart[i] = colStart[i] * upFac / downFac;
    colWidth[i] = B3DMAX(1, colWidth[i] * upFac / downFac);
  }
  for (i = 0; i < (int)partial.size(); i++) {
    partial[i] = partial[i] * upFac / downFac;
    partWidth[i] = B3DMAX(1, partWidth[i] * upFac / downFac);
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
  pixelFlip = yflip;
  if (wasScaled > 0)
    pixelFlip = camSizeY - (wasScaled > 1 ? 4 : 2);
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
  int ind, xx1, xx2, yy1, yy2, operation = rotationFlip, pixelAdd = 0;
  if (defects.wasScaled > 0)
    pixelAdd = defects.wasScaled > 1 ? 3 : 1;
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
    xx2 = xx1 + pixelAdd;
    yy2 = yy1 + pixelAdd;
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
  int ind, xx, yy, xdiff = 1, ydiff;

  if (!wasScaled)
    wasScaled = defects.wasScaled;
  if (wasScaled > 0)
    xdiff = wasScaled > 1 ? 4 : 2;
  ydiff = 65536 * xdiff;
  
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
  sprintf(buf, "FalconType %d\n", defects.FalconType);
  strng += buf;
  sprintf(buf, "NumToAvgSuperRes %d\n", defects.numAvgSuperRes);
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
  camSizeX = camSizeY = 0;
  clearDefectList(defects);

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
    } else if (!strncmp(buf, "FalconType", len)) {
      if (!numToGet || pipErr)
        return 2;
      defects.FalconType = values[0];
    } else if (!strncmp(buf, "NumToAvgSuperRes", len)) {
      if (!numToGet || pipErr)
        return 2;
      defects.numAvgSuperRes = values[0];
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
static void clearDefectList(CameraDefects &defects)
{
  defects.usableTop = 0;
  defects.usableLeft = 0;
  defects.usableBottom = 0;
  defects.usableRight = 0;
  defects.rotationFlip = 0;
  defects.wasScaled = 0;
  defects.K2Type = 0;
  defects.FalconType = 0;
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
}

// Given a string from FEI gain reference file, parse it as an XML string and
// convert the defects into our structure
int CorDefParseFeiXml(const char *strng, CameraDefects &defects, int pad)
{
  int xmlInd, num, startInd, err, tagInd, elemInd, numList, colPad = 0;
  char *rootElement, *value;
  int *valList;
  const char *tags[6] = {"row", "col", "area", "nonmaskingpoint", "point", 
                         "nonmaskingpoint"};
  colPad = pad / 10;
  pad = pad % 10;

  // Set up as XML and make sure it is defects
  clearDefectList(defects);
  xmlInd = ixmlLoadString(strng, 0, &rootElement);
  if (xmlInd < 0)
    return xmlInd;
  if (!rootElement || strcmp(rootElement, "defects")) {
    free(rootElement);
    ixmlClear(xmlInd);
    return -4;
  }
  free(rootElement);

  // Loop on the different types of elements
  for (tagInd = 0; tagInd < 6; tagInd++) {
    err = ixmlFindElements(xmlInd, 0, tags[tagInd], &startInd, &num);
    if (err) {
      ixmlClear(xmlInd);
      return -err;
    }

    // Loop through the matching elements and get each value
    for (elemInd = 0; elemInd < num; elemInd++) {
      err = ixmlGetStringValue(xmlInd, startInd + elemInd, &value);
      if (err || !value) {
        if (!err)
          err = -5;
        free(value);
        ixmlClear(xmlInd);
        return -err;
      }

      // Parse the value as a list
      valList = parselist(value, &numList);
      free(value);
      if (!valList) {
        ixmlClear(xmlInd);
        return 6 - numList;
      }
 
      // Make sure the number of values is right for the type
      if (((tagInd == 2 || tagInd == 3) && numList != 4) || 
          ((tagInd == 4 || tagInd == 5) && numList != 2)) {
        free(valList);
        ixmlClear(xmlInd);
        return 10;
      }
      
      // Allow for padding of columns/rows
      err = B3DMAX(0, valList[0] - colPad);
      switch (tagInd) {
      case 0:
        defects.badRowStart.push_back(err);
        defects.badRowHeight.push_back(numList + colPad + valList[0] - err);
        break;

      case 1:
        defects.badColumnStart.push_back(err);
        defects.badColumnWidth.push_back(numList + colPad + valList[0] - err);
        break;

      case 2:
      case 3:
        if (valList[2] - valList[0] > valList[3] - valList[1]) {
          defects.partialBadRow.push_back(valList[1]);
          defects.partialBadHeight.push_back(1 + valList[3] - valList[1]);
          defects.partialBadStartX.push_back(valList[0]);
          defects.partialBadEndX.push_back(valList[2]);
        } else {
          defects.partialBadCol.push_back(valList[0]);
          defects.partialBadWidth.push_back(1 + valList[2] - valList[0]);
          defects.partialBadStartY.push_back(valList[1]);
          defects.partialBadEndY.push_back(valList[3]);
        }
        break;

      case 4:
      case 5:
        defects.badPixelX.push_back(valList[0]);
        defects.badPixelY.push_back(valList[1]);
        break;
      }

      free(valList);
    }
  }
  ixmlClear(xmlInd);
  defects.FalconType = 1;
  B3DCLAMP(pad, 0, MAX_AVG_SUPER_RES);
  defects.numAvgSuperRes = pad;
  return 0;
}

// Given a TIFF gain reference file, looks for the tag for FEI defects and parses them
// as a defect list with the given padding on line defects.  Writes a standard defect 
// file to dumpDefectName if that is non-NULL.  Processes them by flipping if flipY is 
// true, finding touching pixels, and scaling if superFac != 1.  nx and ny should be
// the size of the gain reference.  messBuf of size bufLen receives messages on error,
// with return value 1.  Return value -1 means no tag was found.
int CorDefProcessFeiDefects(ImodImageFile *iiFile, CameraDefects &defects, int nx, int ny,
                            bool flipY, int superFac, int feiDefPad,
                            const char *dumpDefectName, char *messBuf, int  bufLen)
{
  int retval;
  b3dUInt16 count;
  char *strng, *strCopy;
  FILE *defFP;
  std::string sstr;
  if (tiffGetArray(iiFile, FEI_TIFF_DEFECT_TAG, &count, &strng) <= 0 || !count) 
    return -1;
  strCopy = B3DMALLOC(char, count + 1);
  if (!strCopy) {
    snprintf(messBuf, bufLen, "Memory error copying defect string from TIFF file");
    return 1;
  }
  strncpy(strCopy, strng, count);
  strCopy[count] = 0x00;
  retval = CorDefParseFeiXml(strCopy, defects, feiDefPad);
  if (retval) {
    snprintf(messBuf, bufLen, "Parsing defect string from TIFF file (error %d)", retval);
    free(strCopy);
    return 1;
  }
  if (dumpDefectName) {
    imodBackupFile(dumpDefectName);
    defFP = fopen(dumpDefectName, "w");
    if (!defFP) {
      snprintf(messBuf, bufLen, "Opening file to write defects to, %s", dumpDefectName);
      1;
    }
    CorDefDefectsToString(defects, sstr, nx, ny);
    fprintf(defFP, "%s", sstr.c_str());
    fclose(defFP);
  }
  if (flipY)
    CorDefFlipDefectsInY(&defects, nx, ny, 0);
  CorDefFindTouchingPixels(defects, nx, ny, 0);
  if (superFac != 1)
    CorDefScaleDefectsForFalcon(&defects, superFac);
  free(strCopy);
  return 0;
}


#define SET_PIXEL(x, y, v) { ix = (x) + ixOffset;   \
    iy = (y) + iyOffset;                          \
    if (ix >= 0 && ix < nx && iy >= 0 && iy < ny) \
      array[ix + iy * nx] = v; }

// Fill an array with a binary map (0 and 1) of all pixels contained in defects.  The 
// camera size (after scaling, if any) is taken from the arguments rather than the defect
// list.  The array must have size nx x ny.  These sizes need not match the camera size;
// the image is assumed to be centered on the camera, and defects at the edge are extended
// if an image is oversized
int CorDefFillDefectArray(CameraDefects *param, int camSizeX, int camSizeY,
                          unsigned char *array, int nx, int ny, bool doFalconPad)
{
  int jx, jy, badX, badY, ind, row, col, xLowExtra, xHighExtra, yLowExtra, yHighExtra;
  int ix, iy;   // THESE ARE USED IN SET_PIXEL, DO NOT REUSE
  int ixOffset = (nx - camSizeX) / 2;
  int iyOffset = (ny - camSizeY) / 2;
  int numPix = param->wasScaled > 1 ? 4 : 2;
  if (nx <= 0 || ny <= 0 || camSizeX <= 0 || camSizeY <= 0)
    return 1;
  memset(array, 0, nx * ny);
  doFalconPad = doFalconPad && param->FalconType && param->numAvgSuperRes > 0 && 
    param->wasScaled;

  // Get the limits for defects at edge if the image is oversized
  xLowExtra = 0;
  xHighExtra = camSizeX;
  yLowExtra = 0;
  yHighExtra = camSizeY;
  if (ixOffset > 0) {
    xLowExtra = -ixOffset;
    xHighExtra = nx - ixOffset;
  }
  if (iyOffset > 0) {
    yLowExtra = -iyOffset;
    yHighExtra = ny - iyOffset;
  }

  // Bad pixels
  for (ind = 0; ind < (int)param->badPixelX.size(); ind++) {
    badX = param->badPixelX[ind];
    badY = param->badPixelY[ind];
    if (param->wasScaled > 0) {
      for (jy = 0; jy < numPix; jy++) {
        for (jx = 0; jx < numPix; jx++) {
          SET_PIXEL(badX + jx, badY + jy, 1);
        }
      }
    } else {
      SET_PIXEL(badX, badY, 1);
    }
  }

  // Bad columns and rows
  for (col = 0; col < (int)param->badColumnStart.size(); col++) {
    for (ind = 0; ind < param->badColumnWidth[col]; ind++) {
      badX = param->badColumnStart[col] + ind;
      for (badY = yLowExtra; badY < yHighExtra; badY++)
        SET_PIXEL(badX, badY, 1);
    }
    if (doFalconPad) {
      for (ind = 0; ind < param->numAvgSuperRes; ind++) {
        badX = param->badColumnStart[col] - (ind + 1) * numPix;
        if (badX >= 0)
          for (badY = yLowExtra; badY < yHighExtra; badY++)
            SET_PIXEL(badX, badY, MAP_COL_AVG_SUPER);
        badX = param->badColumnStart[col] + param->badColumnWidth[col] + ind * numPix;
        if (badX < nx)
          for (badY = yLowExtra; badY < yHighExtra; badY++)
            SET_PIXEL(badX, badY, MAP_COL_AVG_SUPER);
      }
    }
  }

  for (row = 0; row < (int)param->badRowStart.size(); row++) {
    for (ind = 0; ind < param->badRowHeight[row]; ind++) {
      badY = param->badRowStart[row] + ind;
      for (badX = xLowExtra; badX < xHighExtra; badX++)
        SET_PIXEL(badX, badY, 1);
    }
    if (doFalconPad) {
      for (ind = 0; ind < param->numAvgSuperRes; ind++) {
        badY = param->badRowStart[row] - (ind + 1) * numPix;
        if (badY >= 0)
          for (badX = xLowExtra; badX < xHighExtra; badX++)
            SET_PIXEL(badX, badY, MAP_ROW_AVG_SUPER);
        badY = param->badRowStart[row] + param->badRowHeight[row] + ind * numPix;
        if (badY < ny)
          for (badX = xLowExtra; badX < xHighExtra; badX++)
            SET_PIXEL(badX, badY, MAP_ROW_AVG_SUPER);
      }
    }
  }

  // Partial bad columns and rows
  for (col = 0; col < (int)param->partialBadCol.size(); col++) {
    for (ind = 0; ind < param->partialBadWidth[col]; ind++) {
      badX = param->partialBadCol[col] + ind;
      for (badY = param->partialBadStartY[col]; badY <= param->partialBadEndY[col];
           badY++)
        SET_PIXEL(badX, badY, 1);
    }
    if (doFalconPad) {
      for (ind = 0; ind < param->numAvgSuperRes; ind++) {
        badX = param->badColumnStart[col] - (ind + 1) * numPix;
        if (badX >= 0)
          for (badY = param->partialBadStartY[col]; badY <= param->partialBadEndY[col];
               badY++)
            SET_PIXEL(badX, badY, MAP_COL_AVG_SUPER);
        badX = param->badColumnStart[col] + param->badColumnWidth[col] + ind * numPix;
        if (badX < nx)
          for (badY = param->partialBadStartY[col]; badY <= param->partialBadEndY[col];
               badY++)
            SET_PIXEL(badX, badY, MAP_COL_AVG_SUPER);
      }
    }
  }
  
  for (row = 0; row < (int)param->partialBadRow.size(); row++) {
    for (ind = 0; ind < param->partialBadHeight[row]; ind++) {
      badY = param->partialBadRow[row] + ind;
      for (badX = param->partialBadStartX[row]; badX <= param->partialBadEndX[row]; 
           badX++)
        SET_PIXEL(badX, badY, 1);
    }
    if (doFalconPad) {
      for (ind = 0; ind < param->numAvgSuperRes; ind++) {
        badY = param->badRowStart[row] - (ind + 1) * numPix;
        if (badY >= 0)
          for (badX = param->partialBadStartX[row]; badX <= param->partialBadEndX[row]; 
               badX++)
            SET_PIXEL(badX, badY, MAP_ROW_AVG_SUPER);
        badY = param->badRowStart[row] + param->badRowHeight[row] + ind * numPix;
        if (badY < ny)
          for (badX = param->partialBadStartX[row]; badX <= param->partialBadEndX[row]; 
               badX++)
            SET_PIXEL(badX, badY, MAP_ROW_AVG_SUPER);
      }
    }
  }

  // Region outside usable area
  if (param->usableLeft > 0)
    for (badX = xLowExtra; badX < param->usableLeft; badX++)
      for (badY = yLowExtra; badY < yHighExtra; badY++)
        SET_PIXEL(badX, badY, 1);
  if (param->usableRight > 0)
    for (badX = param->usableRight + 1; badX < xHighExtra; badX++)
      for (badY = yLowExtra; badY < yHighExtra; badY++)
        SET_PIXEL(badX, badY, 1);
  if (param->usableTop > 0)
    for (badY = yLowExtra; badY < param->usableTop; badY++)
      for (badX = xLowExtra; badX < xHighExtra; badX++)
        SET_PIXEL(badX, badY, 1);
  if (param->usableBottom > 0)
    for (badY = param->usableBottom + 1; badY < xHighExtra; badY++)
      for (badX = xLowExtra; badX < xHighExtra; badX++)
        SET_PIXEL(badX, badY, 1);
  
  return 0;
}

// Expand a gain reference by a factor of 2 or 4 by simple replication of the value for a 
// pixel for all the super-resolution pixels within it.
void CorDefExpandGainReference(float *refIn, int nxIn, int nyIn, int factor,
                               float *refOut)
{
  int ix, iy, nxOut = nxIn * factor;
  float *fIn, *outLine, *fOut;
  for (iy = nyIn - 1; iy >= 0; iy--) {
    fIn = refIn + iy * nxIn;
    fOut = outLine = refOut + (iy * factor + factor - 1) * nxOut;
    if (factor == 2) {
      for (ix = 0; ix < nxIn; ix++) {
        *fOut++ = fIn[ix];
        *fOut++ = fIn[ix];
      }
    } else {
      for (ix = 0; ix < nxIn; ix++) {
        *fOut++ = fIn[ix];
        *fOut++ = fIn[ix];
        *fOut++ = fIn[ix];
        *fOut++ = fIn[ix];
      }
    }
    memcpy(outLine - nxOut, outLine, nxOut * sizeof(float));
    if (factor > 2) {
      memcpy(outLine - 2 * nxOut, outLine, nxOut * sizeof(float));
    memcpy(outLine - 3 * nxOut, outLine, nxOut * sizeof(float));
    }
  }
}

// Read a file of super-resolution gain adjustment factors and return an array
// of bias arrays at a grid of positions.  The centers of the analyzed regions
// start at xStart, yStart and have the given spacing.
//
int CorDefReadSuperGain(const char *filename, int superFac, 
                        std::vector<std::vector<float> > &biases, int &numInX, 
                        int &xStart, int &xSpacing, int &numInY, int &yStart,
                        int &ySpacing)
{
  std::vector<float> bias4, bias16;
  int scanRet, version, ix, iy, doneAtFac;
  FILE *fp = fopen(filename, "r");
  if (!fp)
    return 1;
  scanRet = fscanf(fp, "%d %d", &version, &doneAtFac);
  if (scanRet == EOF)
    return 2;
  if (scanRet && scanRet < 2)
    return 4;
  if (version != 1 || (doneAtFac != 2 && doneAtFac != 4) || doneAtFac < superFac)
    return 3;
  scanRet = fscanf(fp, "%d %d %d %d %d %d", &numInX, &xStart, &xSpacing, &numInY, &yStart,
                   &ySpacing);
  if (scanRet == EOF)
    return 2;
  if (scanRet && scanRet < 6)
    return 4;
  biases.clear();
  bias4.resize(4);
  bias16.resize(16);
  if (superFac != doneAtFac) {
    xStart /= superFac / doneAtFac;
    yStart /= superFac / doneAtFac;
    xSpacing /= superFac / doneAtFac;
    ySpacing /= superFac / doneAtFac;
  }
  for (iy = 0; iy < numInY; iy++) {
    for (ix = 0; ix < numInX; ix++) {
      scanRet = fscanf(fp, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                       &bias16[0], &bias16[1], &bias16[2], &bias16[3],
                       &bias16[4], &bias16[5], &bias16[6], &bias16[7],
                       &bias16[8], &bias16[9], &bias16[10], &bias16[11],
                       &bias16[12], &bias16[13], &bias16[14], &bias16[15]);
        if (scanRet == EOF)
          return 2;
      if (scanRet && scanRet < 16)
        return 4;
      scanRet = fscanf(fp, "%f %f %f %f", &bias4[0], &bias4[1], &bias4[2], &bias4[3]);
        if (scanRet == EOF)
          return 2;
      if (scanRet && scanRet < 4)
        return 4;
      if (superFac == 2)
        biases.push_back(bias4);
      else
        biases.push_back(bias16);
    }
  }
  fclose(fp);
  return 0;
}


// Multiply an expanded gain reference by super-resolution gain adjustment factors
// superFac must be the same as when the file was read
//
void CorDefRefineSuperResRef(float *ref, int nx, int ny, int superFac, 
                             std::vector<std::vector<float> > &biases, int numInX,
                             int xStart, int xSpacing, int numInY, int yStart, 
                             int ySpacing)
{
  int xdiv, ydiv, ix, iy, divInd, iy0, iy1, ix0, ix1, indBase, ixBase, ind;
  int xbase = xSpacing / 2 - xStart;
  int ybase = ySpacing / 2 - xStart;
  float *bias;

  // Loop on the divisions, find limits of pixels belonging to each
  for (ydiv = 0; ydiv < numInY; ydiv++) {
    iy0 = B3DMAX(0, yStart + ydiv * ySpacing - ySpacing / 2);
    iy1 = B3DMIN(ny - 1, yStart + (ydiv + 1) * ySpacing - ySpacing / 2);
    for (xdiv = 0; xdiv < numInX; xdiv++) {
      ix0 = B3DMAX(0, xStart + xdiv * xSpacing - xSpacing / 2);
      ix1 = B3DMIN(nx - 1, xStart + (xdiv + 1) * xSpacing - xSpacing / 2);
      divInd = xdiv + ydiv * numInX;
      bias = &biases[divInd][0];

      // Loop on pixels, apply bias
      for (iy = iy0; iy < iy1; iy++) {
        indBase = superFac * (iy % superFac);
        ixBase = iy * nx;
        for (ix = ix0; ix < ix1; ix++) {
          ind = ix % superFac + indBase;
          ref[ix + ixBase] *= bias[ind];
        }
      }
    }
  }
}


#define FDCE_LINE_DIFFS(typ, dat, nex, dif, lsm, dsm) \
  dat = (typ *)array + iy * nx;   \
  nex = dat + (1 - 2 * idir) * nx;  \
  for (ix = xStart; ix < xEnd; ix++) {  \
    dif = nex[ix] - dat[ix];    \
    lsm += dat[ix];    \
    dsm += dif;     \
    diffSumSq += (double)dif * dif;    \
  }

#define FDCE_COL_SUMS(typ, dat, dif, lsm, dsm) \
  dat = (typ *)array + iy * nx;  \
  for (wid = 0; wid < maxWidth; wid++) {   \
    ind = wid + loop * maxWidth;  \
    dif = dat[ix + idir] - dat[ix];  \
    lsm[ind] += dat[ix];   \
    dsm[ind] += dif;   \
    xDiffSumSq[ind] += (double)dif * dif;   \
    ix += idir;  \
  }

//void  PrintfToLog(char *fmt, ...);

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
  bool isFloat = type == SLICE_MODE_FLOAT;
  unsigned short *usData, *usNext;
  short *sData, *sNext;
  float *fData, *fNext;
  float maxBelow, ratioMedian, ratioMADN, minAbove, fDiff, fLineSum, fDiffSum;
  float *lineMean, *diffMean, *diffSD, *diffRatio, *temp;
  b3dFloat *fxLineSums, *fxDiffSums;
  b3dInt32 *xLineSums, *xDiffSums;
  double *xDiffSumSq;

  if (type != SLICE_MODE_SHORT && type != SLICE_MODE_USHORT && type != SLICE_MODE_FLOAT)
    return 1;
  lineMean = B3DMALLOC(float, 4 * maxWidth);
  diffMean = B3DMALLOC(float, 4 * maxWidth);
  diffSD = B3DMALLOC(float, 4 * maxWidth);
  diffRatio = B3DMALLOC(float, 4 * maxWidth);
  xLineSums = B3DMALLOC(b3dInt32, 2 * maxWidth);
  xDiffSums = B3DMALLOC(b3dInt32, 2 * maxWidth);
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
  fxLineSums = (b3dFloat *)xLineSums;
  fxDiffSums = (b3dFloat *)xDiffSums;
  memset(xLineSums, 0, 2 * maxWidth * sizeof(b3dInt32));
  memset(xDiffSums, 0, 2 * maxWidth * sizeof(b3dInt32));
  memset(xDiffSumSq, 0, 2 * maxWidth * sizeof(double));

  // Do lines at Y levels, store their results first in arrays
  for (wid = 0; wid < maxWidth; wid++) {
    for (idir = 0; idir <= 1; idir++) {
      iy = idir ? ny - 1 - wid : wid;
      lineSum = diffSum = 0;
      diffSumSq = 0.;
      if (isFloat) {
        fLineSum = fDiffSum = 0.;
        FDCE_LINE_DIFFS(b3dFloat, fData, fNext, fDiff, fLineSum, fDiffSum);
      } else {
        if (type == SLICE_MODE_SHORT) {
          FDCE_LINE_DIFFS(short, sData, sNext, diff, lineSum, diffSum);
        } else {
          FDCE_LINE_DIFFS(unsigned short, usData, usNext, diff, lineSum, diffSum);
        }
        fDiffSum = (float)diffSum;
        fLineSum = (float)lineSum;
      }
      ind = idir * maxWidth + wid;
      sumsToAvgSDdbl(fDiffSum, diffSumSq, 1, xEnd - xStart, &diffMean[ind],
                     &diffSD[ind]);
      lineMean[ind] = fLineSum / (float)(xEnd - xStart);
      diffRatio[ind] = diffMean[ind] / B3DMAX(0.1f, diffSD[ind]);
    }
  }

  // Go across lines adding to the sums for each column in X
  for (iy = yStart; iy < yEnd; iy++) {
    for (loop = 0; loop < 2; loop++) {
      idir = 1 - 2 * loop;
      ix = loop ? nx - 1 : 0;
      if (isFloat) {
        FDCE_COL_SUMS(b3dFloat, fData, fDiff, fxLineSums, fxDiffSums);
      } else if (type == SLICE_MODE_SHORT) {
        FDCE_COL_SUMS(short, sData, diff, xLineSums, xDiffSums);
      } else {
        FDCE_COL_SUMS(unsigned short, usData, diff, xLineSums, xDiffSums);
      }
    }
  }

  // Get the mean and SD and ratio for each column
  for (wid = 0; wid < 2 * maxWidth; wid++) {
    ind = wid + 2 * maxWidth;
    sumsToAvgSDdbl(B3DCHOICE(isFloat, fxDiffSums[wid], (double)xDiffSums[wid]),
      xDiffSumSq[wid], 1, yEnd - yStart, &diffMean[ind], &diffSD[ind]);
    lineMean[ind] = B3DCHOICE(isFloat, fxLineSums[wid], (float)xLineSums[wid]) / 
      (float)(yEnd - yStart);
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
  /* PrintfToLog("Last ratio deviations below and above threshold = %.2f  %.2f MADNs\r\n",
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
     /*if (indAbove[loop] >= 0) {
       for (wid = 0; wid <= B3DMIN(maxWidth - 1, indAbove[loop] + 3); wid++) {
         ind = wid + loop * maxWidth;
         PrintfToLog("%d  %2d  %7.1f  %7.1f  %7.1f  %7.2f  %7.2f\r\n", loop, wid, lineMean[ind],
           diffMean[ind], diffSD[ind], diffRatio[ind], temp[ind]);
       }
               }*/
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

// Handle initial scaling of a defect list for K2 or Falcon and deduce the binning
int CorDefSetupToCorrect(int nxFull, int nyFull, CameraDefects &defects, int &camSizeX,
                       int &camSizeY, int scaleDefects, float setBinning, int &useBinning,
                       const char *binOpt)
{
  char bintext[8];
  bool scaledForK2;
  int scaling;

  // Falcon must always be unscaled and scaling by 2 or 4 is allowed with an exact match
  if (defects.FalconType && (nxFull > camSizeX || nyFull > camSizeY)) {
    if (defects.wasScaled > 1)
      return 1;
    scaling = B3DNINT(nxFull / camSizeX);
    if (camSizeX * scaling != nxFull || camSizeY * scaling != nyFull || 
        (scaling != 2 && scaling != 4))
      return 1;
    CorDefScaleDefectsForFalcon(&defects, scaling);
    camSizeX *= scaling;
    camSizeY *= scaling;
  }
  if (defects.FalconType && (nxFull < camSizeX && nyFull < camSizeY)) {
    if (defects.wasScaled < 0)
      return 1;
    scaling = B3DNINT(camSizeX / nxFull);
    if (camSizeX * scaling != nxFull || camSizeY * scaling != nyFull || 
        (scaling != 2 && scaling != 4 && scaling != 8))
      return 1;
    CorDefScaleDefectsForFalcon(&defects, -scaling);
    camSizeX /= scaling;
    camSizeY /= scaling;
  }

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
        std::cout << "Assuming binning of " << bintext 
                  << " for defect correction instead of a small subarea;" 
                  << std::endl << "    use the " << binOpt 
                  << " option to set a binning if this is" " incorrect." << std::endl;
    }
  } else {
    useBinning = B3DNINT((scaledForK2 ? 2 : 1) * setBinning + 0.02);
  }
  return 0;
}
