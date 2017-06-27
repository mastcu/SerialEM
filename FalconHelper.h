// FalconHelper.h: declarations for FalconHelper.cpp
#pragma once
#include <set>
#include <string>
#include <map>
#include <vector>

struct ScopePluginFuncs;

class CFalconHelper
{
public:
  CFalconHelper(void);
  ~CFalconHelper(void);
  void Initialize(bool skipConfigs);
  void DistributeSubframes(ShortVec &summedFrameList, int numReadouts, int newFrames, 
    FloatVec &userFrameFrac, FloatVec &userSubframeFrac);
  float AdjustForExposure(ShortVec &summedFrameList, int numSkipBefore,
    int numSkipAfter, float exposure, float readoutInterval, FloatVec &userFrameFrac, 
    FloatVec &userSubframeFrac);
  GetMember(bool, StackingFrames);

private:
  CSerialEMApp *mWinApp;
  CCameraController *mCamera;
  float mReadoutInterval;
  CArray<CString, CString> mOtherLines;
  CString mStorePrefix, mFramePrefix;
  int mNumStartLines;
  BOOL mConfigBackedUp;
  BOOL mLastSavedDummy;
  CFileStatus mLastDummyStatus;
  std::set<std::string> mPreExistingRaws;
  std::map<int, std::string> mFileMap;
  int mBaseFileIndex;
  ScopePluginFuncs *mPlugFuncs;
  long mNumFiles;
  long mHeadNums[8];
  int mNx, mNy;
  int mChunkSize, mNumChunks;
  short *mRotData;
  int mRotateFlip, mDivideBy;
  long mDivideBy2;
  float mPixel;
  long mNumStacked;
  CString mLocalPath, mDirectory, mRootname;
  std::map<int, std::string>::iterator mIter;
  int mFileInd;
  int mStackError;
  KStoreMRC *mStoreMRC;
  KImageShort *mImage, *mImage2;
  bool mDeletePrev;
  double mWaitStart;
  int mWaitCount;
  int mWaitMaxCount;
  int mWaitInterval;
  bool mStackingFrames;
  int mUseImage2;
  std::vector<long> mDeleteList;
  int mStartedAsyncSave;

public:
  int SetupConfigFile(ControlSet &conSet, CString localPath, CString &directory, 
    CString &filename, CString &configFile, BOOL stackingDeferred, 
    CameraParameters *camParams, long &numFrames);
  int StackFrames(CString localPath, CString &directory, CString &rootname, 
    long divideBy2, int divideBy, int rotateFlip, float pixel, BOOL doAsync, 
    long & numStacked);
  const char * GetErrorString(int code);
  int BuildFileMap(CString localPath, CString &directory);
  void StackNextTask(int param);
  void CleanupStacking(void);
  int StackingWaiting();
  void CheckAsyncSave(void);
  int GetFrameTotals(ShortVec & summedFrameList, int & totalSubframes, 
    int maxFrames = -1);
  float AdjustSumsForExposure(CameraParameters * camParams, ControlSet * conSet, float exposure);
  int CheckFalconConfig(int setState, int & state, const char * message);
};
