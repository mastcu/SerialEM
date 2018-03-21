
#ifndef KPIXMAP_H
#define KPIXMAP_H

#include "KImage.h"
#include "KImageScale.h"

// A wrapper for bitmap image, with BITMAPINFO and scaling members
class KPixMap 
{
protected:
	BITMAPINFO    *mBMInfo;
	KImage       *mRect;
	KImageScale   mScale;
	
	bool          mHasScaled;

	unsigned char *mLut;
	int           mLutMin;
	int           mLutRange;
	int           mLutType;

public:
		     KPixMap();
	virtual ~KPixMap();	
	virtual  KImage*    getImRectPtr() { return mRect; }
	virtual  int  useRect(KImage *inImage, bool keepBGRorder = false);
	virtual  void doneWithRect();	
	virtual  void KPixMap::SetLut(int inType, int inMin, int inRange);
	virtual  void setLevels(int inBrightness, int inContrast, int inInverted, 
    float boostContrast, float mean);
	virtual  void setLevels(KImageScale &inScale);
	virtual  void setLevels();
	virtual  BITMAPINFO *getBMInfo() { return mBMInfo; };	
};

#endif
