// Adapted for Tecnai 2/17/02.

#pragma once

#include "KImageStore.h"
#include "../MontageParam.h"

typedef short Int16;
typedef int Int32;
#define MRC_MODE_USHORT  6

// 3/1/13: Modified to conform to current IMOD/MRC definitions
struct HeaderMRC
{
	Int32 nx, ny, nz, mode;
	Int32 mxst, myst, mzst;
	Int32 mx, my,mz;
	float dx, dy, dz;
	float alpha,beta,gamma;
	Int32 mapc, mapr, maps;
	float amin, amax, amean;
	Int32 nspg;
	Int32 next;
	Int32 oldId;
  Int32 unused;
  char  extType[4];
  int   nversion;
	char  blank[16];
	Int16 nbytext, typext;	//Deviates from Delta
	char  blank2[20];
  int imodStamp, imodFlags;
	Int16 type,lensid,n1,n2,v1,v2;
	float tiltAngles[6];	
  // New style
  float xo,yo,zo;
  char cmap[4];
  unsigned char stamp[4];
  float rms;
	Int32 nttl;
	char  labels[10][80];
	
	HeaderMRC();
	void Init(int inX, int inY, int inZ, int inMode);
};

class KStoreMRC : public KImageStore 
{	
	Int32      mHeadSize;
	HeaderMRC *mHead;
	char       *mExtra;

public:
       KStoreMRC(CFile *inFile);
       KStoreMRC(CString inFilename, FileOptions inFileOpt);
	virtual ~KStoreMRC();
	void Seek(int offset, UINT flag);
	void Read(void *buf, int count);
	void Write(void *buf, int count);
	void SetPixelSpacing(float pixel);
	static BOOL IsMRC(CFile *inFile);
  const char *GetTitle(int index);
	
	virtual KImage  *getRect(void);
	virtual float      getPixel(KCoord &inCoord);
	virtual int     AppendImage(KImage *inImage);
	virtual int    WriteSection(KImage * inImage, int inSect);
	virtual int    AddTitle(const char *inTitle);
  virtual int     setMode(int inMode);
  virtual int   CheckMontage(MontParam *inParam); 
	virtual int   getPcoord(int inSect, int &outX, int &outY, int &outZ);
  virtual int   getStageCoord(int inSect, double &outX, double &outY);
  virtual void  BigSeek(int base, int size1, int size2, UINT flag);
  int getExtraData(EMimageExtra * extra, int section);
  virtual int ReorderPieceZCoords(int *sectOrder);
};
