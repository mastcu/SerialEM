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
  virtual BOOL  FileOK();
  virtual bool    fileIsShrMem();
  virtual KImage  *getRect(void);
  virtual int     CheckMontage(MontParam *inParam);
  virtual int     getPcoord(int inSect, int &outX, int &outY, int &outZ, bool gotMutex = false);
  virtual int     getStageCoord(int inSect, double &outX, double &outY);
  virtual int     ReorderPieceZCoords(int *sectOrder);
  virtual int   getMode() {return mMode;}
	virtual void  Close();
  virtual int getTiffField(int tag, void *value);
  virtual int getTiffValue(int tag, int type, int tokenNum, 
    double &dvalue, CString &valstring);
  virtual int getTiffArray(int tag, b3dUInt32 *count, void *value);
  virtual int WriteSection(KImage * inImage);
  virtual void CommonInit(void);
  virtual int     AppendImage(KImage *inImage);
  virtual int    WriteSection(KImage * inImage, int inSect);
  virtual int    AddTitle(const char *inTitle);
  virtual int     setMode(int inMode);
  virtual int  AssignIIfileTypeAndFormat();
  virtual CString getFilePath();
  virtual void UpdateHdfMrcHeader();
  virtual int ReorderHdfStackZvalues(int *sectOrder);
private:
  ImodImageFile *mIIfile;
  int mUpdateCounter;
};
