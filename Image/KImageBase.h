//
// KImageBase.h 

#ifndef KIMAGE_BASE_H
#define KIMAGE_BASE_H

struct KCoord {
	int x;
	int y;
	int z;
	int t;
};

struct KPoint {
	float x, y, z, t;
};

//////////////////////////////////////////////////////////
// The Base class for image files
//
class KImageBase {

public:
	         KImageBase();
	virtual ~KImageBase();
	
protected:
	KCoord mOrigin;
	int     mWidth, mHeight, mDepth, mLength;  // x, y, z & t sizes.
	KCoord mCur;

public:
	virtual int  getWidth(void) { return mWidth; };
	virtual int  getHeight(void) { return mHeight; };
	virtual int  getDepth(void) { return mDepth;};
	virtual int  getLength(void){return mLength;}
	
	virtual void getMaxBounds(KCoord &outCoord);
	virtual void getMinBounds(KCoord &outCoord);
	        
	virtual void setCur(KCoord &inCoord);
	virtual void getCur(KCoord &inCoord);
	virtual void setCurPos(KCoord &inCoord);
	virtual void getCurPos(KCoord &inCoord);
	virtual void setCurTime(KCoord &inCoord);
	virtual void getCurTime(KCoord &inCoord);
	virtual int  fixCoord(KCoord &ioCoord);
};

#endif
