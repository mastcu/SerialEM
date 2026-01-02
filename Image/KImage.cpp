// KImage.cpp:            Has the basic image class and KImageShort and KImageFloat
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer

#include "stdafx.h"
#include "KImage.h"
#include "..\Utilities\XCorr.h"
#include "..\SerialEM.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

KImage::KImage()
{
  CommonInit(kUBYTE, 0, 0);
  mRowBytes    = 0;
}

// Make a new image from existing one, sharing the image data
KImage::KImage(KImage *inRect)
{
  // Copy the structure values
  *this = *inRect;

  // Increment the reference count
  if (mReference)
    (*mReference)++;

  // If there is extra data, make a new copy
  // Notice this is building in the assumption that the
  // the user data is an EMimageExtra!
  if (mUserData) {
    EMimageExtra *theExtra = new EMimageExtra();
    *theExtra = *(EMimageExtra *)mUserData;
    mUserData = theExtra;
  }
}

// Make a new image of the given size
KImage::KImage(int inWidth, int inHeight)
{
  CommonInit(kUBYTE, inWidth, inHeight);
  mRowBytes = inWidth;

  // DNM 2/15/02: make the calling program set the size
  // to multiple of 4 if needed; don't do it here
  // if (inWidth%4) mRowBytes += (4 - (inWidth%4));

  if (((size_t)mRowWidth * inHeight) <= 0){
    mWidth = mHeight = mRowBytes = mRowWidth = 0;
    return;
  }

  NewArray2(mData, unsigned char, mRowWidth, inHeight);
  if (mData == NULL){
    mRowWidth = mRowBytes = mWidth = 0;
    mHeight = 0;
    return;
  };
  mReference = new int;
  *mReference = 1;

}

// Dereference data; Unlock and destroy SAFEARRAY or just delete mData
KImage::~KImage()
{
  if(mReference) {
    (*mReference)--;
    if (*mReference <= 0){
      if (mHData) {
        UnLock();
        mHR = SafeArrayDestroy(mHData);
        TestHResult("from SafeArrayDestroy when deleting image");
      } else if (mData)
        delete[] mData;
      delete mReference;
    }
  }

  // Also delete the user data, which is a unique copy
  if (mUserData)
    delete mUserData;
}

// Common initialization for all KImage types
void KImage::CommonInit(int inType, int inWidth, int inHeight)
{
  mType        = inType;
  mMode        = kGray;
  mXshift        = 0.f;
  mYshift        = 0.f;
  mWidth       = inWidth;
  mHeight      = inHeight;
  mRowWidth    = inWidth;
  mMin = mMax = mMean = 0.;

  mReference   = NULL;
  mData        = NULL;
  mUserData    = NULL;
  mHData = NULL;
  mIsLocked = false;
  mNeedToReflip = false;
}

// Set a KImage to use existing data
void KImage::useData(char *inData, int inWidth, int inHeight)
{
  mHData = NULL;
  mData     = (unsigned char *)inData;
  mWidth    = inWidth;
  mHeight   = inHeight;
  mRowBytes = mWidth;
  mRowWidth = mWidth;
  mReference = new int;
  *mReference = 1;
}

// Call with a SAFEARRAY to use date in one.  Then call lock
// before use and unlock after
void KImage::useData(SAFEARRAY *inHandle)
{
  mHData = inHandle;
  mMode     = kGray;

  // This shouldn't become valid unless the array is locked
  // mData     = (unsigned char *) (*inHandle);
  // mWidth    = inWidth;
  // mHeight   = inHeight;
  mHR = SafeArrayGetUBound(inHandle, 1, (long *)&mWidth);
  TestHResult("getting SafeArray X upper bound");
  mHR = SafeArrayGetUBound(inHandle, 2, (long *)&mHeight);
  TestHResult("getting SafeArray Y upper bound");
  mWidth++;   // The upper bounds are size - 1
  mHeight++;

  mRowBytes = mWidth;
  mRowWidth = mWidth;
  mReference = new int;
  *mReference = 1;
}

// Detach data from the image; this image will not delete data when destroyed
void KImage::detachData()
{
  mData = NULL;
  mHData = NULL;
  if(mReference) {
    (*mReference)--;
    if (*mReference <= 0) {
      delete mReference;
      mReference = NULL;
    }
  }
}

void KImage::getSize(int &outX, int &outY)
{
  outX = mWidth;
  outY = mHeight;
}

char *KImage::getRowData(int inRow)
{
  char *rowBuffer = (char *) mData;
  rowBuffer += (mRowBytes * (size_t)inRow);
  return(rowBuffer);
}

float KImage::getPixel(int inX, int inY)
{
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
      return(0.);
  return((float)mData[inX + ((size_t)inY * mRowWidth)]);
}

int KImage::setPixel(int inX, int inY, int inValue)
{
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
  return(1);
  mData[inX + ((size_t)inY * mRowWidth)] = inValue;
  return(0);
}

int KImage::MatchPos(KCoord &inCoord)
{
  if (mPos.z != inCoord.z) return(0);
  if (mPos.t != inCoord.t) return(0);
  return(1);
}
void KImage::SetPos(KCoord &inCoord)
{
  mPos = inCoord;
}

void KImage::applyPad()
{
  unsigned char *from, *to;
  int mrow = mWidth;
  int mcol = mHeight/2;
  int x, y;

  if (mWidth != mRowWidth)
  for(y = mHeight - 1; y > 0; y--){
    from = to = mData;
    from += mWidth * y;
    to   += mRowBytes * (size_t)y;
    for(x = mrow-1; x > 0; x--)
      to[x] = from[x];
  }
}

void KImage::flipY()
{
  ProcSimpleFlipY(mData, mRowBytes, mHeight);
}

// Lock and unlock the data: if it is a SAFEARRAY, get access or unaccess it
void KImage::Lock()
{
  if (mHData != NULL) {
    mHR = SafeArrayAccessData(mHData, (void **)(&mData));
    TestHResult("accessing/locking SafeArray data");
    mIsLocked = true;     // Set flag
  }
}

void KImage::UnLock()
{
  if (mIsLocked) {
    mHR = SafeArrayUnaccessData(mHData);
    TestHResult("unaccessing/unlocking SafeArray data");
    mIsLocked = false;      // Clear flag
  }
}


BOOL KImage::TestHResult(CString inString)
{
  if (SUCCEEDED(mHR))
    return false;

  if (inString) {
    CString message;
    message.Format(_T("Error 0x%x %s"), mHR, inString);
    AfxMessageBox(message);
  }
  return true;
}

void KImage::setType(int inType)
{
  if ((mType == kSHORT || mType == kUSHORT) && (inType == kSHORT || inType == kUSHORT))
    mType = inType;
}


// ---Short Data---

////////////////////////////////////////////////////////
//
//
KImageShort::KImageShort()
  : KImage()
{
  mType     = kSHORT;
}
KImageShort::KImageShort(KImage *inRect)
  : KImage(inRect)
{
  mDataShort = (short *)mData;
  return;
}

KImageShort::KImageShort(int inWidth, int inHeight)
{
  CommonInit(kSHORT, inWidth, inHeight);

  // Again, leave this to calling routine
  // if (inWidth %  2) mRowWidth += 1;

  if (((size_t)mRowWidth * inHeight) == 0){
    mDataShort  = NULL;
    mWidth = mHeight = mRowBytes = mRowWidth = 0;
    return;
  }

  NewArray2(mDataShort, short, mRowWidth, inHeight);

  mData = (unsigned char *)mDataShort;
  mRowBytes = mRowWidth * sizeof(short);

  if (mDataShort == NULL){
    mRowWidth = mRowBytes = mWidth = 0;
    mHeight = 0;
    return;
  }
  mReference = new int;
  *mReference = 1;
}

KImageShort::~KImageShort()
{

}

float KImageShort::getPixel(int inX, int inY)
{
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
      return(0.);
  if (mType == kSHORT)
    return((float)mDataShort[inX + ((size_t)inY * mWidth)]);
  unsigned short int *usData = (unsigned short int *)mDataShort;
  return((float)usData[inX + (inY * mWidth)]);
}

// 7/5/10: Nothing calls these setPixel functions
int KImageShort::setPixel(int inX, int inY,
unsigned char red, unsigned char green, unsigned char blue)
{

  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
  return(1);

  short inValue = 0;
  short r, g, b;
  r = red >> 3;
  g = green >> 3;
  b = blue >> 3;
  inValue = (r<<10) + (g<<5) + b;
  mDataShort[inX + ((size_t)inY * mRowWidth)] = inValue;
  return(0);
}

int KImageShort::setPixel(int inX, int inY, int inValue)
{
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
      return(1);
  mDataShort[inX + ((size_t)inY * mRowWidth)] = (short)inValue;
  return(0);
}

void KImageShort::useData(char * inData, int inWidth, int inHeight)
{
  KImage::useData(inData, inWidth, inHeight);
  mRowBytes = mWidth * 2;
  mDataShort = (short *)mData;
}

void KImageShort::useData(SAFEARRAY *inHandle)
{
  KImage::useData(inHandle);
  mRowBytes = mWidth * 2;
}

void KImageShort::Lock()
{
  KImage::Lock();
  mDataShort = (short int *)mData;
}



// ---Float Data---

////////////////////////////////////////////////////////
//
//
KImageFloat::KImageFloat()
  : KImage()
{
  mType     = kFLOAT;
}

KImageFloat::KImageFloat(KImage *inRect)
  : KImage(inRect)
{
  mDataFloat = (float *)mData;
}

KImageFloat::KImageFloat(int inWidth, int inHeight)
{
  CommonInit(kFLOAT, inWidth, inHeight);

  if (((size_t)mRowWidth * inHeight) == 0){
    mDataFloat  = NULL;
    mWidth = mHeight = mRowBytes = mRowWidth = 0;
    return;
  }
  NewArray2(mDataFloat, float, mRowWidth, inHeight);

  mData = (unsigned char *)mDataFloat;
  mRowBytes = mRowWidth * sizeof(float);

  if (mDataFloat == NULL){
    mRowWidth = mRowBytes = mWidth = 0;
    mHeight = 0;
    return;
  }
  mReference = new int;
  *mReference = 1;
}

KImageFloat::~KImageFloat()
{

}

void KImageFloat::useData(char * inData, int inWidth, int inHeight)
{
  KImage::useData(inData, inWidth, inHeight);
  mRowBytes = mWidth * 4;
  mDataFloat = (float *)mData;
}

void KImageFloat::Lock()
{
  KImage::Lock();
  mDataFloat = (float *)mData;
}

float KImageFloat::getPixel(int inX, int inY)
{
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
      return(0.);
  return(mDataFloat[inX + ((size_t)inY * mWidth)]);
}


// RGB IMAGE DATA
///////////////////////////////////
//
KImageRGB::KImageRGB()
  : KImage()
{
  mType     = kRGB;
  mMode     = kRGBmode;
}

KImageRGB::KImageRGB(KImage *inRect)
  : KImage(inRect)
{
}

KImageRGB::KImageRGB(int inWidth, int inHeight)
{
  CommonInit(kRGB, inWidth, inHeight);
  mMode     = kRGBmode;

  if (((size_t)mRowWidth * inHeight) == 0){
    mWidth = mHeight = mRowBytes = mRowWidth = 0;
    return;
  }
  mRowBytes = mRowWidth * 3;
  NewArray2(mData, unsigned char, mRowBytes, inHeight);

  if (mData == NULL){
    mRowWidth = mRowBytes = mWidth = 0;
    mHeight = 0;
    return;
  }
  mReference = new int;
  *mReference = 1;
}

KImageRGB::~KImageRGB()
{

}

void KImageRGB::useData(char * inData, int inWidth, int inHeight)
{
  KImage::useData(inData, inWidth, inHeight);
  mRowBytes = mWidth * 3;
}

void KImageRGB::getPixel(int inX, int inY, int &red, int &green, int &blue)
{
  red = green = blue = 0;
  if ((inX < 0) || (inY < 0) ||
    (inX >= mWidth) || (inY >= mHeight))
      return;
  unsigned char *datap = &mData[3 * inX + mRowBytes * (size_t)inY];
  red = datap[0];
  green = datap[1];
  blue = datap[2];
}
