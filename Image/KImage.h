// Header file for KImage byte image class and KImageShort short int class
// Original Author James Kremer, 1997
// Adapted for Tecnai, DNM 2/15/02
// Removed KimageInt.  Changed Handle to SAFEARRAY.  Removed GetDataFromHandle.

#ifndef KIMAGE_H
#define KIMAGE_H

//#include <math.h>
//#include <Quickdraw.h>

#include "KImageBase.h"
#include "..\Shared\mrcslice.h"
class EMimageExtra;

// mode constants.
const int kGray = 1;
const int kRGBmode  = 2;
const int kRGBAmode = 3;
const int kBGRmode  = 4;

// type constants.
const int kUBYTE  = SLICE_MODE_BYTE;
const int kBYTE   = 9;
const int kUSHORT = SLICE_MODE_USHORT;
const int kSHORT  = SLICE_MODE_SHORT;
const int kUINT   = 10;
const int kINT    = 11;
const int kFLOAT  = SLICE_MODE_FLOAT;
const int kRGB    = SLICE_MODE_RGB;

class KImage
{	
protected:
  unsigned char *mData;   // The image data.
  int     *mReference;     // Number of classes using data.
  int     mWidth;		    // Image dimensions.
  int     mHeight;
  int     mRowWidth;      // Aligned Storage dimensions.
  int     mRowBytes;
  
  float   mXshift, mYshift;   // Origin of image.
  float   mMin, mMax, mMean;
  short   mMode;
  short   mType;
  KCoord  mPos;
  EMimageExtra  *mUserData;
  SAFEARRAY *mHData;
  int       mIsLocked;
  bool     mNeedToReflip;
  HRESULT mHR;		// result of latest SafeArray call
  //	char    mHState;
  
public:
  BOOL TestHResult(CString inString);
  KImage();
  KImage(KImage *inImage);
  KImage(int inWidth, int inHeight);
  virtual       ~KImage();
  virtual void CommonInit(int inType, int inWidth, int inHeight);
  virtual void   useData(char * inData, int inWidth, int inHeight);
  virtual void    useData(SAFEARRAY *inHandle  /*, int inWidth, int inHeight */);
  virtual void   detachData();
	 
  virtual void   getSize(int &outX, int &outY);
  virtual int    getWidth(void) { return mWidth; }
  virtual int    getHeight(void) { return mHeight; }
  virtual void  *getData(void) { return mData; }
  virtual int    getRowBytes(void) { return mRowBytes; }
  virtual char  *getRowData(int inRow = 0) ;
  virtual int    getMode(void) { return mMode; }
  virtual void   setMode(int inMode) {mMode = inMode;}
  virtual int    getType(void) { return mType; }
  virtual float  getPixel(int inX, int inY);
  virtual void   getShifts(float &outX, float &outY) {outX = mXshift; outY = mYshift;}
  virtual void   setShifts(float inX, float inY) {mXshift = inX; mYshift = inY;}	
  virtual void   setType(int inType);
  virtual void   setMinMaxMean(float min, float max, float mean) {mMin = min; mMax = max; mMean = mean;};
  
  virtual void   applyPad();
  virtual void   flipY(void);
  virtual int    setPixel(int inX, int inY, int inPixel);
  
  virtual int    MatchPos(KCoord &inCoord);
  virtual void   SetPos(KCoord &inCoord);
  
  virtual EMimageExtra  *GetUserData() { return mUserData; }
  virtual void   SetUserData(EMimageExtra *inUserData) { mUserData = inUserData; }
  
  virtual void  Lock();
  virtual void  UnLock();
  
  virtual int getRefCount(void) {return mReference ? *mReference : 0;};
  virtual int *getRefPtr(void) {return mReference;};
  virtual bool getNeedToReflip() {return mNeedToReflip;};
  virtual void setNeedToReflip(bool inVal) {mNeedToReflip = inVal;};
};


class KImageShort : public KImage
{
protected:
  short *mDataShort;
public:
  void Lock();
  KImageShort();
  KImageShort(KImage *inRect);
  KImageShort(int inWidth, int inHeight);
  virtual      ~KImageShort();
  virtual void useData(char * inData, int inWidth, int inHeight);
  virtual void useData(SAFEARRAY *inHandle  /*, int inWidth, int inHeight */);

  virtual int   setPixel(int inX, int inY, int inValue);
  virtual float getPixel(int inX, int inY);
  int setPixel(int inX, int inY, 
    unsigned char red, 
    unsigned char green, 
    unsigned char blue);
  
  
};

class KImageFloat : public KImage
{
protected:
  float *mDataFloat;
public:
  void Lock();
  KImageFloat();
  KImageFloat(KImage *inRect);
  KImageFloat(int inWidth, int inHeight);
  virtual      ~KImageFloat();
  virtual float getPixel(int inX, int inY);
  virtual void useData(char * inData, int inWidth, int inHeight);
};

class KImageRGB : public KImage
{
public:
  KImageRGB();
  KImageRGB(KImage *inRect);
  KImageRGB(int inWidth, int inHeight);
  virtual      ~KImageRGB();
  virtual void getPixel(int inX, int inY, int &red, int &green, int &blue);
  virtual void useData(char *inData, int inWidth, int inHeight);
};
#endif