// KPixMap.cpp:           A class that builds and holds a byte pixmap for an image
//                          being displayed and sets up color tables and ramps
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer

#include "stdafx.h"
#include "KPixMap.h"
#include "..\Shared\b3dutil.h"
#include "..\Utilities\SEMUtilities.h"
#include <math.h>


KPixMap::KPixMap()
{
  mRect = NULL;
  mLut = NULL;
  mHasScaled = 0;
  mBMInfo = (BITMAPINFO *)(new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)]);
  BITMAPINFO *pbmi = mBMInfo;
  if (pbmi == NULL)
      return;
    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
    pbmi->bmiHeader.biWidth = 0; 
    pbmi->bmiHeader.biHeight = 0;
    pbmi->bmiHeader.biPlanes = 1; 
    pbmi->bmiHeader.biBitCount = 8; 
    pbmi->bmiHeader.biClrUsed = 256; 

    // If the bitmap is not compressed, set the BI_RGB flag. 
    pbmi->bmiHeader.biCompression = BI_RGB; 

     // Set biClrImportant to 0, indicating that all of the 
    // device colors are important. 
  pbmi->bmiHeader.biClrImportant = 0; 

  
  // setLevels(0,0);
}

// Sets up the member byte image mRect: just points to existing byte data, or creates a new
// byte image in other cases and copies scaled data there
int KPixMap::useRect(KImage *inRect)
{
  int i, j;
  int theShiftX, theShiftY, fillFac;
  float fShiftX, fShiftY;
  int theMode = inRect->getMode();
  int theType = inRect->getType();
  int width, height;
  unsigned char *ucdata;
  short *sdata;
  unsigned short *usdata;
  float *fdata;
  unsigned char *bdata;
  int theMin, theRange;
  float fMin, fRange, fScale, fval;

  inRect->getSize(width, height);
  inRect->getShifts(fShiftX, fShiftY);
  theShiftX = B3DNINT(fShiftX);
  theShiftY = B3DNINT(fShiftY);

  // Limit the shifts so that there is at least one pixel left
  if (theShiftX > width - 1)
    theShiftX = width - 1;
  if (theShiftX < 1 - width)
    theShiftX = 1 - width;
  if (theShiftY > height - 1)
    theShiftY = height - 1;
  if (theShiftY < 1 - height)
    theShiftY = 1 - height;

  int startFillY = theShiftY > 0 ? theShiftY : 0;
  int endFillY = theShiftY > 0 ? height : height + theShiftY;
  int nCopyX = theShiftX > 0 ? width - theShiftX : width + theShiftX;
  int theOffset = theShiftX > 0 ? 0 : -theShiftX;
  
  // Height is negative to indicate top-down image
  mBMInfo->bmiHeader.biWidth = 4 *((width + 3) / 4);
  mBMInfo->bmiHeader.biHeight = -height;
  mBMInfo->bmiHeader.biSizeImage = height * mBMInfo->bmiHeader.biWidth;
  if (theMode == kRGBmode || theMode == kBGRmode) {
    mBMInfo->bmiHeader.biBitCount = 24; 
    mBMInfo->bmiHeader.biClrUsed = 0;
    fillFac = 3;
  } else {
    mBMInfo->bmiHeader.biBitCount = 8; 
    mBMInfo->bmiHeader.biClrUsed = 256; 
    fillFac = 1;
  }

  mScale.GetScaleFactors(fMin, fRange);
  fScale = 255.f / fRange;
  theMin = (int)floor((double)fMin + 0.5);
  theRange = (int)floor((double)fRange + 0.5);
  inRect->Lock();
  
  // If there's no scaling and the image is already the right size for
  // display, use it as is after deleting current image if any
  if ((theType == kUBYTE || theMode == kBGRmode) && theMin == 0 && theRange == 255 && 
    (inRect->getRowBytes() % 4) == 0 && !theShiftX && !theShiftY) {
     doneWithRect();
     mRect = new KImage(inRect);
       
  } else {
    // Otherwise, need to create a new image or reuse existing image of the right
    // size (unless it is a copy), and copy it over with scaling
    if (mRect && (mRect->getWidth() != mBMInfo->bmiHeader.biWidth || 
      mRect->getHeight() != height || mRect->getRefCount() > 1 || 
      mRect->getMode() != theMode))
      doneWithRect();
    if (!mRect && UtilOKtoAllocate((theMode == kGray ? 1 : 3) * 
      mBMInfo->bmiHeader.biWidth * height)) {
        if (theMode == kGray) {
          mRect = new KImage(mBMInfo->bmiHeader.biWidth, height);
        } else {
          mRect = new KImageRGB(mBMInfo->bmiHeader.biWidth, height);
          mRect->setMode(kBGRmode);
        }
    }
    if (!mRect || !mRect->getData()) {
      inRect->UnLock();
      return 1;
    }
    
    if (theType != kFLOAT && theType != kRGB)
      SetLut(theType, theMin, theRange);
    
    // Copy data row by row with look-up    
    for (j = 0; j < startFillY; j++) {
      bdata = (unsigned char *)mRect->getRowData(j);
      for (i = 0; i < fillFac * width; i++)
        bdata[i] = 127;
    }

    for(j = startFillY; j < endFillY; j++){
      bdata = (unsigned char *)mRect->getRowData(j);
      for (i = 0; i < fillFac * theShiftX; i++)
        *bdata++ = 127;
      
      switch(theType){
      case kUBYTE:
        ucdata = (unsigned char *)inRect->getRowData(j - theShiftY) + theOffset;
        for(i = 0; i < nCopyX; i++)
          *bdata++ = mLut[*ucdata++];
        break;
        
      case kSHORT:
        sdata = (short *)inRect->getRowData(j - theShiftY) + theOffset;
        for(i = 0; i < nCopyX; i++) 
          *bdata++ = mLut[*sdata++ + 32768];
        break;
        
      case kUSHORT:
        usdata = (unsigned short *)inRect->getRowData(j - theShiftY) + theOffset;
        for(i = 0; i < nCopyX; i++) 
          *bdata++ = mLut[*usdata++];
        break;
        
      case kFLOAT:
        fdata = (float *)inRect->getRowData(j - theShiftY) + theOffset;
        for(i = 0; i < nCopyX; i++) {
          fval = fScale * (*fdata++ - fMin);
          if (fval < 0.)
            fval = 0.f;
          if (fval > 255.)
            fval = 255.f;
          *bdata++ = (unsigned char)fval;
        }
        break;

     case kRGB:
        ucdata = (unsigned char *)inRect->getRowData(j - theShiftY) + 3 * theOffset;

        // The pixmap is BGR like the RGBQUAD structure used in bmiColors
        for(i = 0; i < nCopyX; i++) {
          *bdata++ = ucdata[2];
          *bdata++ = ucdata[1];
          *bdata++ = ucdata[0];
          ucdata += 3;
        }
        break;

      }

      for (i = 0; i < -fillFac * theShiftX; i++)
        *bdata++ = 127;
    }

    for (j = endFillY; j < height; j++) {
      bdata = (unsigned char *)mRect->getRowData(j);
      for (i = 0; i < fillFac * width; i++)
        bdata[i] = 127;
    }             
  }
  
  inRect->UnLock();
  
  return 0;
}

void KPixMap::doneWithRect()
{
  if (mRect)
    delete mRect;
  mRect = NULL;
}

KPixMap::~KPixMap()
{
  doneWithRect();
  if (mLut)
    delete [] mLut;
  if (mBMInfo)
    delete mBMInfo;
}

// Set up the LUT for getting from image to bitmap values
void KPixMap::SetLut(int inType, int inMin, int inRange)
{ 
  int theVal, i;
  int loVal = 0;
  int hiVal = 256;
  if (inType == kSHORT) {
    loVal = -32768;
    hiVal = 32768;
  } else if (inType == kUSHORT) {
    loVal = 0;
    hiVal = 65536;
  }

  // Make a look-up table unless old one matches
  if (mLut == NULL || mLutType != inType || mLutMin != inMin ||
    mLutRange != inRange) {

    // Allocate space if needed or if type has changed
    if (mLut == NULL || mLutType != inType) {
      if (mLut)
        delete [] mLut;
      mLut = new unsigned char [hiVal - loVal];
      mLutType = inType;
    }
        
    // Fill the table
    for (i = loVal; i< hiVal; i++) {
      theVal =  ( (i - inMin) * 256) / inRange;
      if (theVal < 0) theVal =0;
      if (theVal > 255) theVal = 255;
      mLut[i - loVal] = (unsigned char)theVal; 
    } 
    mLutMin = inMin;
    mLutRange = inRange;
  }

}

// takes the incoming brightness and contrast, transfers them to the brightness and
// contrast of THIS pixmap, and computes a ramp and sets the color table from it.
void KPixMap::setLevels(int inBrightness, int inContrast, int inInverted, 
                        float boostContrast, float mean)
{
  if (mHasScaled && (inBrightness == mScale.mBrightness) && 
    (inContrast == mScale.mContrast) && inInverted == mScale.mInverted && 
    fabs(boostContrast - mScale.mBoostContrast) < 1.e-3 && 
    fabs(mean - mScale.mMeanForBoost) < 1.e-3) 
    return;
  mScale.mBrightness = inBrightness;  // -127 to 128
  mScale.mContrast = inContrast;
  mScale.mInverted = inInverted;
  mScale.mBoostContrast = boostContrast;
  mScale.mMeanForBoost = mean;

  setLevels();
}


// If the scaling has changed or has not been set yet, calls the setLevels below
void KPixMap::setLevels(KImageScale &inScale)
{
  // If the pixmap isn't mutable we don't have to
  // do anything.
  
  // If the levels have already been set
  // we don't have to set them again.
  if (&inScale == NULL)
    return;
  if ((mHasScaled) && (inScale == mScale))
    return;
  mScale = inScale;
  setLevels();
}

// gets a ramp based upon the contrast and brightness from KImageScale, then converts 
// that to RGBs and changes the color table

void KPixMap::setLevels()
{ 
  // Get the ramp from the image scale.
  unsigned int ramp[256];
  mScale.DoRamp(ramp);
  
  // Copy the ramp to the BITMAPINFO ColorTable.
  for(short i = 0; i < 256; i++){
    mBMInfo->bmiColors[i].rgbRed = ramp[i];
    mBMInfo->bmiColors[i].rgbGreen = ramp[i];
    mBMInfo->bmiColors[i].rgbBlue = ramp[i];
    mBMInfo->bmiColors[i].rgbReserved = 0;
  }

  mHasScaled = true;  
}

