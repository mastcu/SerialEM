/*
 *  frameutil.cpp -- Utilities needed from framealign and gpuframe
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 2016-2026 by the Regents of the University of
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 *  $Id$
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "cppdefs.h"
#include "b3dutil.h"
#include "cfft.h"
#include "frameutil.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static int sDumpInd = 0;
static CharArgType sPrintFunc = NULL;
static const char *sDot = ".";
static const char *sDumpDir = sDot;
static bool sSetDumpDir = false;

static void checkDumpDir(void);

/*
 * Compute coordinates for wrapping an image or subset of it to move corner to center
 */
void utilCoordsForWrap(int nxFrom, int nyFrom, int nxTo, int nyTo, int xOffset,
                         int yOffset, int ixFrom0[4], int ixTo0[4], int iyFrom0[4],
                         int iyTo0[4], int ixFrom1[4], int ixTo1[4], int iyFrom1[4],
                         int iyTo1[4])
{
  int num;
  ixFrom0[0] = ixFrom0[3] = B3DMAX(0, xOffset - nxTo / 2);
  ixFrom1[0] = ixFrom1[3] = nxTo / 2 + xOffset - 1;
  num = ixFrom1[0] + 1 - ixFrom0[0];
  ixTo1[0] = ixTo1[3] = nxTo - 1;
  ixTo0[0] = ixTo0[3] = nxTo - num;
  //PRINT4(ixFrom0[0], ixFrom1[0], ixTo0[0], ixTo1[0]);

  iyFrom0[0] = iyFrom0[1] = B3DMAX(0, yOffset - nyTo / 2);
  iyFrom1[0] = iyFrom1[1] = nyTo / 2 + yOffset - 1;
  num = iyFrom1[0] + 1 - iyFrom0[0];
  iyTo1[0] = iyTo1[1] = nyTo - 1;
  iyTo0[0] = iyTo0[1] = nyTo - num;

  ixFrom0[1] = ixFrom0[2] = xOffset + nxFrom - nxTo / 2;
  ixFrom1[1] = ixFrom1[2] = B3DMIN(nxFrom - 1, ixFrom0[1] + nxTo - 1);
  num = ixFrom1[1] + 1 - ixFrom0[1];
  ixTo0[1] = ixTo0[2] = 0;
  ixTo1[1] = ixTo1[2] = num - 1;
  //PRINT4(ixFrom0[1], ixFrom1[1], ixTo0[1], ixTo1[1]);

  iyFrom0[2] = iyFrom0[3] = yOffset + nyFrom - nyTo / 2;
  iyFrom1[2] = iyFrom1[3] = B3DMIN(nyFrom - 1, iyFrom0[2] + nyTo - 1);
  num = iyFrom1[2] + 1 - iyFrom0[2];
  iyTo0[2] = iyTo0[3] = 0;
  iyTo1[2] = iyTo1[3] = num - 1;
  //PRINT4(ixFrom0[0], ixFrom0[2], iyFrom0[0], iyFrom0[2]);
}

void utilRollSavedFrames(std::vector<float *> &savedVec, int numFrames)
{
  float *tempBin = savedVec[0];
  for (int ind = 0; ind < numFrames - 1; ind++)
    savedVec[ind] = savedVec[ind + 1];
  savedVec[numFrames - 1] = tempBin;
}

/*
 * Output an FFT, or convert to image and dump that and convert back.
 */
void utilDumpFFT(float *fft, int nxPad, int nyPad, const char *descrip, int real,
                 int frame, int scale)
{
#ifndef NO_DUMPS_IN_DLL
  MrcHeader hdr;
  char fname[320];
  FILE *fp;
  float scaleFac = (float)(1./sqrt((double)nxPad * nyPad));
  int ind;
  checkDumpDir();
  if (real) {
    todfftc(fft, nxPad, nyPad, 1);
    utilDumpImage(fft, nxPad + 2, nxPad, nyPad, 0, descrip);
    todfftc(fft, nxPad, nyPad, 0);
    return;
  }
  float *shiftTemp = B3DMALLOC(float, 2 * nxPad + 4);
  if (!shiftTemp)
    return;
  wrapFFTslice(fft, shiftTemp, (nxPad + 2) / 2, nyPad, 0);
  if (scale)
    for (ind = 0; ind < (nxPad + 2) * nyPad; ind++)
      fft[ind] *= scaleFac;
  mrc_head_new(&hdr, (nxPad + 2) / 2, nyPad, 1, MRC_MODE_COMPLEX_FLOAT);
  sprintf(fname, "%s/fafft-%d.mrc", sDumpDir, sDumpInd);
  fp = fopen(fname, "wb");
  if (fp) {
    mrc_head_write(fp, &hdr);
    mrc_write_slice(fft, fp, &hdr, 0, 'Z');
    fclose(fp);
    utilPrint("Saved %s fft frame %d in %s\n", descrip, frame, fname);
  }
  sDumpInd++;
  if (scale)
    for (ind = 0; ind < (nxPad + 2) * nyPad; ind++)
      fft[ind] /= scaleFac;
  wrapFFTslice(fft, shiftTemp, (nxPad + 2) / 2, nyPad, 1);
  free(shiftTemp);
#endif
}

/*
 * Output an image, wrapping it properly if it is a correlation with ifCorr > 0.
 * To output a non-float image, pass ifCorr as -1 - mode
 */
void utilDumpImage(float *buf, int nxDim, int nxPad, int nyPad, int ifCorr,
                   const char *descrip, int frame)
{
#ifndef NO_DUMPS_IN_DLL
  MrcHeader hdr;
  int dataSize, csize;
  char fname[320];
  FILE *fp;
  int mode = MRC_MODE_FLOAT;
  Islice slice;
  checkDumpDir();
  if (ifCorr < 0)
    mode = -ifCorr - 1;
  if (dataSizeForMode(mode, &dataSize, &csize) < 0)
    return;
  float *temp = (float *)malloc(nxPad * nyPad * dataSize);
  if (!temp)
    return;
  if (ifCorr > 0) {
    int iy, iyIn, ixIn, iout, ix;
    ixIn = nxPad / 2;
    iyIn = nyPad / 2;
    iout = 0;
    for (iy = 0; iy < nyPad; iy++) {
      for (ix = 0; ix < nxPad; ix++) {
        temp[iout++] = buf[ixIn + iyIn * nxDim];
        ixIn = (ixIn + 1) % nxPad;
      }
      iyIn = (iyIn + 1) % nyPad;
    }

  } else {
    for (int iy = 0; iy < nyPad; iy++)
      memcpy((char *)temp + iy * nxPad * dataSize, (char *)buf + iy * nxDim * dataSize,
             dataSize * nxPad);
  }
  mrc_head_new(&hdr, nxPad, nyPad, 1, mode);
  sprintf(fname, "%s/faimg-%d.mrc", sDumpDir, sDumpInd);
  imodBackupFile(fname);
  fp = fopen(fname, "wb");
  sliceInit(&slice, nxPad, nyPad, mode, temp);
  sliceMMM(&slice);
  hdr.amin = slice.min;
  hdr.amax = slice.max;
  hdr.amean = slice.mean;
  if (fp) {
    mrc_head_write(fp, &hdr);
    mrc_write_slice(temp, fp, &hdr, 0, 'Z');
    fclose(fp);
    utilPrint("Saved %s image frame %d in %s    mean %.2f\n", descrip, frame, fname,
           hdr.amean);
  }
  sDumpInd++;
  free(temp);
#endif
}

static void checkDumpDir(void)
{
  if (sSetDumpDir)
    return;
  if (getenv("FRAMEALIGN_DUMPDIR") != NULL)
    sDumpDir = strdup(getenv("FRAMEALIGN_DUMPDIR"));
  sSetDumpDir = true;
  if (!sDumpDir)
    sDumpDir = sDot;
}

void utilSetPrintFunc(CharArgType func)
{
  sPrintFunc = func;
}

// Print a message with flushes that were needed for fortran
void utilPrint(const char *format, ...)
{
  char errorMess[512];
  va_list args;
  va_start(args, format);
  vsprintf(errorMess, format, args);
  if (sPrintFunc) {
    sPrintFunc(errorMess);
  } else {
    printf("%s", errorMess);
    fflush(stdout);
    fflush(stdout);
  }
  va_end(args);
}


