//
// KImageStore.h
// Original Author: James Kremer, 1997
// Adapted for Tecnai, DNM, 2-16-02
// Chnage LFileStream to CFile, substitute mFilename for mFSSpec usage
// Eliminate some functions

#ifndef KIMAGE_STORE_H
#define KIMAGE_STORE_H

/////////////////////////////////////////////////////////
// The image storage class.
// This will be derived for different 
// graphics formats.
//

#include "KImage.h"
#include "../FileOptions.h"

#define STORE_TYPE_MRC 0
#define STORE_TYPE_IMOD 1
#define STORE_TYPE_TIFF 2
#define STORE_TYPE_ADOC 3
#define STORE_TYPE_JPEG 4

class KImageStore : public KImageBase
{

protected:
	CString  mFilename;
	CFile *mFile;
	
	int mMin, mMax;
  int mStoreType;
	int mMode;
	int mPixSize;
  int mAdocIndex;
  float mFracIntTrunc;
	FileOptions  mFileOpt;
  BOOL  mMontCoordsInMdoc;   // Flag for montage if piece coords not in header
  int mNumTitles;
  CString mTitles;
	
public:
	         KImageStore(CString inFilename);
 	         KImageStore(CFile *inFile);
	         KImageStore();
	virtual ~KImageStore(void);
	
	virtual int     Open(CFile *inFile);
	virtual int     Open(CFile *inFile, int inWidth, int inHeight, int inSections);
	virtual void    Close();
	virtual int     Flush();
	virtual CString getFilePath();
  virtual int     getStoreType() {return mStoreType;};    
	virtual int     getMode() {return mMode;};
  virtual int     GetAdocIndex(void) {return mAdocIndex;};

  // Methods that need to be defined appropriately in subclasses
  virtual int     AppendImage(KImage *inImage) {return 1;};
	virtual int     WriteSection(KImage * inImage, int inSect) {return 1;};
  virtual	void    SetPixelSpacing(float pixel) {};
  virtual int     AddTitle(const char *inTitle);
  virtual int     CheckMontage(MontParam *inParam) {return 0;}; 
  virtual int     getPcoord(int inSect, int &outX, int &outY, int &outZ) {return -1;};
  virtual int     getStageCoord(int inSect, double &outX, double &outY) {return -1;};
  virtual KImage  *getRect(void) {return NULL;};
  virtual int     InvertPieceZCoords(int zmax) {return -1;};
  virtual int     setMode(int inMode) {return 0;};

  int CheckMontage(MontParam *inParam, int nx, int ny, int nz);
  virtual void    setTime(int inTime);
	virtual void    setSection(int inSection);
	virtual CString getName(void) {return mFilename;};
	virtual BOOL    FileOK() { return (mFile != NULL); };
  static int      lookupPixSize(int inMode);
	virtual void    SetTruncation(float inTruncLo, float inTruncHi)
			{ mFileOpt.pctTruncLo = inTruncLo; mFileOpt.pctTruncHi = inTruncHi; }
  virtual void    SetUnsignedOption(int inOpt) {mFileOpt.unsignOpt = inOpt;};
  virtual void    SetSignedOption(int inOpt) {mFileOpt.signToUnsignOpt = inOpt;};
  virtual void    SetCompression(int inComp) {mFileOpt.compression = inComp;};
  virtual int     GetCompression() {return mFileOpt.compression;};
	
	virtual float   getPixel(KCoord &inCoord);
  virtual float   getLastIntTruncation() {return mFracIntTrunc;};
  virtual void    setName(CString inName) {mFilename = inName;};
  virtual void    minMaxMean(char * idata, int dataSize, float & outMin, float & outMax, 
    double & outMean);
  virtual char    *convertForWriting(KImage *inImage, bool needFlipped, bool &needToReflip,
    bool &needToDelete);
  virtual void CommonInit(void);
  virtual bool montCoordsInAdoc() {return mAdocIndex >= 0 && mMontCoordsInMdoc;};
  virtual CString getAdocName(void);
  virtual int getImageShift(int inSect, double &outISX, double &outISY);
  virtual unsigned int getChecksum();
  virtual int getUnsignOpt() {return mFileOpt.unsignOpt;};
  virtual int getSignToUnsignOpt() {return mFileOpt.signToUnsignOpt;};
};


#endif