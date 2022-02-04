#ifndef EMBUFFERMANAGER_H
#define EMBUFFERMANAGER_H

#include "SerialEM.h"
#include "EMimageBuffer.h"
#include "Image\KImageStore.h"
#define READ_MONTAGE_OK  -97

struct SaveThreadData {
  KImageStore *store;
  KImageStore *fromStore;
  KImage *image;
  int section;
  int fromSection;
  int error;
};

class EMbufferManager
{
public :
  int CheckSaveConditions(KImageStore *inStoreMRC, EMimageBuffer *toBuf);
  GetSetMember(BOOL, AlignToB)
  int AutoalignBasicIndex();
  int AutoalignBufferIndex();
  EMbufferManager(CString *inModeNamep, EMimageBuffer *inImBufs);
  ~EMbufferManager();
  void SetImageBuffers(EMimageBuffer *inImBufp) { mImBufsp = inImBufp; }
  GetSetMember(int, ShiftsOnAcquire)
  GetSetMember(int, CopyOnSave)
  GetSetMember(int, BufToReadInto)
  GetSetMember(int, FifthCopyToBuf);
  GetSetMember(int, BufToSave);
  GetSetMember(int, AlignOnSave);
  GetSetMember(int, StackWinMaxXY);
  GetSetMember(int, AutoZoom);
  GetSetMember(int, Antialias);
  GetSetMember(int, FixedZoomStep);
  GetSetMember(int, DrawScaleBar);
  GetSetMember(BOOL, DrawCrosshairs);
  GetSetMember(BOOL, DrawTiltAxis);
  GetSetMember(float, UnsignedTruncLimit);
  GetSetMember(BOOL, SaveAsynchronously);
  GetSetMember(float, HdfUpdateTimePerSect);
  SetMember(CString, OtherFile);
  int GetConfirmBeforeDestroy (int inWhich)
    { return mConfirmDestroy[inWhich]; }
  void SetConfirmBeforeDestroy(int inWhich, int inVal)
    {  mConfirmDestroy[inWhich] = inVal; }
  int CopyImageBuffer(int inFrom, int inTo);
  int CopyImBuf(EMimageBuffer *fromBuf, EMimageBuffer *toBuf, BOOL display = true);
  int SaveImageBuffer(KImageStore *inStoreMRC, bool skipCheck = false, int inSect = -1);
  BOOL IsBufferSavable(EMimageBuffer *toBuf, KImageStore *inStoreMRC = NULL);
  BOOL DoesBufferExist (int inWhich);
  BOOL OKtoDestroy(int inWhich, char *inMessage);
  int  OverwriteImage(KImageStore *inStoreMRC, int inSect = NO_SUPPLIED_SECTION);
  int   ReadOtherFile();
  int   RereadOtherFile(CString &message);
  BOOL IsOtherFileReadable() { return (!mOtherFile.IsEmpty()); }
  int   ReadFromFile(KImageStore *inStore, int inSect = NO_SUPPLIED_SECTION,
    int inBufTo = -1, BOOL readPiece = false, BOOL synchronous = false,
    CString *message = NULL);
  EMimageBuffer *GetSaveBuffer();
  int ReplaceImage(char *inData, int inType, int inX, int inY,
                   int inBuf, int inCap, int inConSet, bool fftBuf = false);
  int MainImBufIndex(EMimageBuffer *imBuf);
  GetSetMember(BOOL, RotateAxisAngle);
  void ReportError (int inErr);
  GetMember(bool, ImageAsyncFailed);
  SetMember(int, NextSecToRead);
  GetMember(int, AsyncTimeout);
  GetMember(BOOL, DoingAsyncSave);
  bool DoingSychroThread() {return mSynchronousThread != NULL; };

 private:
  int mShiftsOnAcquire;
  int mCopyOnSave;
  int mBufToSave;
  int mBufToReadInto;
  int mFifthCopyToBuf;
  BOOL mAlignToB;
  int mConfirmDestroy[MAX_CONSETS];
  int mAlignOnSave;
  int mAutoZoom;
  int mAntialias;
  int mFixedZoomStep;
  int mDrawScaleBar;
  BOOL mDrawCrosshairs;
  BOOL mDrawTiltAxis;
  CString *mModeNames;
  EMimageBuffer *mImBufsp;
  CSerialEMApp *mWinApp;
  CString mOtherFile;
  KImageStore *mOtherStoreMRC;
  int mStackWinMaxXY;
  BOOL mRotateAxisAngle;
  float mUnsignedTruncLimit;   // Fraction of truncation allowed when saving unsigned
  BOOL mSaveAsynchronously;    // Flag to write to files asynchronously
  EMimageBuffer mImBufForSave;  // Copy of image buffer being saved
  CWinThread *mSavingThread;   // Thread pointer
  SaveThreadData mSaveTD;      // Data passed to thread
  CWinThread *mSynchronousThread;  // Synchronous save thread for large save to other
  BOOL mDoingAsyncSave;
  int mAsyncTimeout;           // timeout for the async save
  double mAsyncStartTime;
  bool mAsyncFromImage;
  bool mImageAsyncFailed;
  int mNextSecToRead;
  float mHdfUpdateTimePerSect;  // Maximum time per section to spend updating HDF header

public:
  int AddToStackWindow(int bufNum, int binning, int secNum, bool convert, int angleOrder);
  void FindScaling(EMimageBuffer * imBuf);
  void DeleteOtherStore() {delete mOtherStoreMRC;};
  int StartAsyncSave(KImageStore *store, EMimageBuffer *buf, int section);
  int StartAsyncSave(KImageStore *store, KImage *image, int section);
  static UINT SavingProc(LPVOID pParam);
  static UINT SynchronousProc(LPVOID pParam);
  int AsyncSaveBusy(void);
  void AsyncSaveDone(void);
  void AsyncSaveCleanup(int error);
  CString ComposeErrorMessage(int inErr, char * writeType);
  int CheckAsyncSaving(void);
  int StartAsyncCopy(KImageStore *fromStore, KImageStore *toStore, int fromSection,
    int toSection, bool synchronous);
  void StartAsyncThread(KImageStore *fromStore, KImageStore *store, int section,
    bool synchronous);
  EMimageExtra *SetChangeWhenSaved(EMimageBuffer *imBuf, KImageStore *inStore, int &oldDivided);
};

#endif
