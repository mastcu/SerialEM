// KImageScale.cpp:       A class with methods for determining and keeping track of
//                          image scaling
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer

#include "stdafx.h"
#include "KImageScale.h"
#include "..\Shared\b3dutil.h"
#include "..\Shared\mrcslice.h"
#include "..\SerialEM.h"

KImageScale::KImageScale()
{
  mBrightness  = 0;
  mContrast    = 0;
  mInverted    = 0;
  mBoostContrast = 1.;
  mMeanForBoost = 128.;
  
  mMinScale    = 0.;
  mMaxScale    = 255.;
  mSampleMin   = 0.;
  mSampleMax   = 255.;
  
}


int KImageScale::operator==(KImageScale &inScale)
{
  if (inScale.mBrightness != mBrightness || inScale.mContrast != mContrast ||
    inScale.mMinScale != mMinScale || inScale.mMaxScale != mMaxScale ||
    inScale.mSampleMin != mSampleMin || inScale.mSampleMax != mSampleMax ||
    inScale.mSampleMin != mSampleMin || inScale.mInverted != mInverted ||
    fabs(inScale.mBoostContrast - mBoostContrast) > 1.e-3 ||
    fabs(inScale.mMeanForBoost - mMeanForBoost) > 1.e-3)
    return false;
  return true;
}

unsigned char KImageScale::DoScale(int inValue)
{
  double fval = 256. * (inValue - mMinScale) / (mMaxScale-mMinScale+1);
  if (fval < 0) fval = 0.;
  if (fval > 255) fval = 255.;
  return (unsigned char)fval;
}

void KImageScale::GetScaleFactors(float &outMin, float &outRange)
{
  outMin = mMinScale;
  outRange = mMaxScale - mMinScale;
  if(!outRange) outRange = 1.;
}

// Computes a ramp from current contrast and brightness
// The header declares default inRampSize to 256
void KImageScale::DoRamp(unsigned int *inRamp, int inRampSize)
{
  short gLevel;
  
  
  int   scale, offset;
  int   c = mContrast + 128;
  
  
  if (c < 129) {
    scale = c;
  } else {
    scale = 16384 / (256 - c);
  }

  offset  =  scale * (mBrightness - 128) ;
  offset /= 128;
  offset += 128;

  // If boosting contrast, modify offset to maintain the mean
  if (mBoostContrast > 1.) {
    offset += B3DNINT((1. - mBoostContrast) * mMeanForBoost * scale / 128.);
    scale = B3DNINT(mBoostContrast * scale);
  }
  
  for (int i = 0; i < inRampSize; i++) {
    // ((i + mBrightness - 128) * scale) / 128 + 128
    gLevel = ((i * scale) / 128) + offset;
    
    if (gLevel < 0) gLevel = 0;
    if (gLevel > 255) gLevel = 255;
    
    if (mInverted)
      inRamp[inRampSize - 1 - i] = gLevel;     
    else  
      inRamp[i] = gLevel;     
  }
}

// Find the image scaling that saturates a fraction of pixels
void KImageScale::FindPctStretch(KImage *inImage, float pctLo, float pctHi, float fracUse,
  int FFTbkgdGray, float FFTTruncDiam)
{
  float nSample = 10000.;
  float matt = 0.5f * (1.f - fracUse);
  int ix, iy, nx, ny, type, ixStart, iyStart, nxUse, nyUse, loop, dsize = 1;
  int maxRing, minRing, nsum;
  float sample, scaleLo, scaleHi, val, sum, valMax, valSecond, bkgd;
  double ringRad, rad;
  char *theImage;
  unsigned char **linePtrs;
  type = inImage->getType();
  inImage->getSize(nx, ny);
  if (type != SLICE_MODE_BYTE)
    dsize = type == SLICE_MODE_FLOAT ? 4 : 2;
  inImage->Lock();
  theImage = inImage->getRowData(0);
  linePtrs = makeLinePointers(theImage, nx, ny, dsize);
  if (!linePtrs) {
    inImage->UnLock();
    return;
  }

  // Loop twice, first finding the normal sample, then getting a much denser sample to set
  // the limits for the black/white sliders
  for (loop = 0; loop < 2; loop ++) {
    ixStart = (int)(nx * matt);
    nxUse = nx - 2 * ixStart;
    iyStart = (int)(ny * matt);
    nyUse = ny - 2 * iyStart;
    sample = nSample / (fracUse * nx * fracUse * ny);
    if (sample > 1.0)
      sample = 1.0;
    if (percentileStretch(linePtrs, type, nx, ny, sample, ixStart, iyStart, nxUse, nyUse,
      pctLo, pctHi, &scaleLo, &scaleHi) == 0) {

        // Spread the values out a bit for integer images if they are close together
        if (type != SLICE_MODE_FLOAT) {
          val = scaleLo;
          scaleLo = B3DMIN(scaleLo, scaleHi - 2);
          scaleHi = B3DMAX(scaleHi, val + 2);
        }

        // Autocorrelations have a single pixel sharp peak, so just get center point and
        // include it in the max range, 
        // Or, get 8 points around the center and include second-highest in scale
        valMax = -1.e37f;  
        valSecond = -1.e37f;
        if (nx > 3 && ny > 3) {
          for (iy = -1; iy <= 1; iy++) {
            for (ix = -1; ix <= 1; ix++) {
              if ((loop && (ix || iy)) || (!loop && !ix && !iy))
                continue;
              val = GetImageValue(linePtrs, type, nx, ny, nx / 2 + ix, ny / 2 + iy);
              if (val > valMax) {
                valSecond = valMax;
                valMax = val;
              } else if (val > valSecond) {
                valSecond = val;
              }
            }
          }
        }

        if (loop) {
          mSampleMin = scaleLo;
          mSampleMax = B3DMAX(valMax, scaleHi);
        } else {
          mMinScale = scaleLo;
          mMaxScale = B3DMAX(valSecond, scaleHi);
        }
    }
    nSample = 100000.;
    matt = 0.;
    pctLo = 0.;
    pctHi = 0.;
  }

  // New analysis of FFT for ring saturation and background gray level
  if (FFTbkgdGray > 0 && nx > 50 && ny > 50) {

    // Get background mean from left edge
    sum = 0;
    for (iy = 0; iy < ny; iy++)
      for (ix = 2; ix < 5; ix++)
        sum += GetImageValue(linePtrs, type, nx, ny, ix, iy);
    bkgd = sum / (3 * ny);

    // Average a ring at the given diameter relative to image size
    ringRad = sqrt((double)nx * ny) * FFTTruncDiam / 2.;
    maxRing = (int)ceil(ringRad) + 1;
    minRing = (int)(0.7 * ringRad) - 1;
    sum = 0.;
    nsum = 0;
    for (iy = -maxRing; iy <= maxRing; iy++) {
      for (ix = -maxRing; ix <= maxRing; ix++) {
        if (iy < -minRing || iy > minRing || ix < -minRing || ix > minRing) {
          rad = sqrt((double)ix * ix + iy * iy);
          if (fabs(rad - ringRad) < 0.71) {
            sum +=  GetImageValue(linePtrs, type, nx, ny, nx / 2 + ix, ny / 2 + iy);
            nsum++;
          }
        }
      }
    }

    // Set the max from the ring average and the min to place the background at the
    // given gray level
    if (nsum > 3) {
      mMaxScale = sum / (float)nsum;
      val = (float)FFTbkgdGray / 256.f;
      mMinScale = (bkgd - mMaxScale * val) / (1.f - val);
    }
  }

  // Finish up
  free(linePtrs);
  inImage->UnLock();
  if (mSampleMax < mMaxScale)
    mSampleMax = mMaxScale;
  if (mSampleMin > mMinScale)
    mSampleMin = mMinScale;
  mBoostContrast = 1.;
}

float KImageScale::GetImageValue(unsigned char **linePtrs, int type, int nx, int ny, 
  int ix, int iy)
{
  float val;
  if (type == SLICE_MODE_BYTE)
    val = linePtrs[iy][ix];
  else if (type == SLICE_MODE_FLOAT)
    val = *(((float *)linePtrs[iy]) + ix);
  else if (type == SLICE_MODE_SHORT)
    val = *(((short *)linePtrs[iy]) + ix);
  else
    val = *(((unsigned short *)linePtrs[iy]) + ix);
  return val;
}
