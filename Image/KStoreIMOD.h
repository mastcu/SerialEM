#pragma once
#include "KImageStore.h"
#include "..\FileOptions.h"

typedef struct  ImodImageFileStruct ImodImageFile;

enum {TIFFTAG_UINT16 = 1, TIFFTAG_UINT32, TIFFTAG_FLOAT, TIFFTAG_DOUBLE, TIFFTAG_STRING};

class KStoreIMOD :
  public KImageStore
{
public:
  KStoreIMOD(CString filename);
  KStoreIMOD(CString inFilename, FileOptions inFileOpt);
  ~KStoreIMOD(void);
	virtual BOOL  FileOK() { return (mIIfile != NULL); }
	virtual KImage  *getRect(void);
	virtual int   getMode() {return mMode;}	
	virtual void  Close();
  virtual int getTiffField(int tag, void *value);
  virtual int getTiffValue(int tag, int type, int tokenNum, 
    double &dvalue, CString &valstring);
  virtual int getTiffArray(int tag, b3dUInt16 *count, void *value);
  virtual int WriteSection(KImage * inImage);
  virtual void CommonInit(void);
private:
  ImodImageFile *mIIfile;
};
