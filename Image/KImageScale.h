// KImageScale.h
#ifndef KIMAGE_SCALE_H
#define KIMAGE_SCALE_H

#include "KImage.h"

class KImageScale
{

friend class CImageLevelDlg;
friend class KPixMap;

protected:
	// Contrast and brightness are used for setting the color map
	// for displaying a bitmap after an image has been converted to bytes
	short mBrightness; // -127 to 128   
	short mContrast;
  short mInverted;
  short mFalseColor;
  float mBoostContrast;
  float mMeanForBoost;
	
	// mMinScale ands mMaxScale are the min and max values for converting
	// the raw image into the byte bitmap
	float   mMinScale;
	float   mMaxScale;

  // Sample min and max values provide outer limits for black/white sliders
  float   mSampleMin;
  float   mSampleMax;
		
public:
	
	KImageScale();
	int operator==(KImageScale &inScale);
	virtual float GetMinScale() {return mMinScale;}
	virtual float GetMaxScale() {return mMaxScale;}
	virtual void GetScaleFactors(float &outMin, float &outRange);
	virtual float GetSampleMin() {return mSampleMin;}
	virtual float GetSampleMax() {return mSampleMax;}
  virtual int GetBrightness() {return mBrightness;};
  virtual int GetContrast() {return mContrast;};
  virtual int GetInverted() { return mInverted; };
  virtual short GetFalseColor() { return mFalseColor; };
  void SetFalseColor(short inVal) { mFalseColor = inVal; };

	unsigned char DoScale(int inValue);
  void SetMinMax(float inMin, float inMax) {mMinScale = inMin; mMaxScale = inMax;};
  void SetSampleMinMax(float inMin, float inMax) {mSampleMin = inMin; mSampleMax = inMax;};
	void DoRamp(unsigned int *inRamp, int inRampSize = 256);
	
	void FindPctStretch(KImage *inImage, float pctLo, float pctHi, float fracUse, 
    int FFTbkgdGray = -1, float FFTTruncDiam = 0., bool partialScan = false);
  float GetImageValue(unsigned char **linePtrs, int type, int nx, int ny, int ix, int iy);
};

#endif