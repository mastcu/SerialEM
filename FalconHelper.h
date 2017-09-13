// FalconHelper.h: declarations for FalconHelper.cpp
#pragma once
#include <set>
#include <string>
#include <map>
#include <vector>
#include "mrcfiles.h"

struct ScopePluginFuncs;
class FrameAlign;
struct CameraThreadData;

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
  GetMember(float, GpuMemory);
  GetMember(bool, GettingFRC);
  GetMember(int, AlignError);
  GetMember(int, NumAligned);
  GetMember(bool, AlignSubset);
  GetMember(int, AlignStart);
  GetMember(int, AlignEnd);
  SetMember(CString, LastFrameDir);
 
  int GetUseGpuForAlign(int inIMOD) {return mUseGpuForAlign[inIMOD ? 1 : 0];};
  void SetUseGpuForAlign(int inIMOD, int inVal) {mUseGpuForAlign[inIMOD ? 1 : 0] = inVal;};

private:
  CSerialEMApp *mWinApp;
  CCameraController *mCamera;
  FrameAlign *mFrameAli;
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
  int mRotateFlip, mDivideBy, mAliSumRotFlip;
  long mDivideBy2;
  float mPixel;
  long mNumStacked;
  CString mLocalPath, mDirectory, mRootname, mLastFrameDir;
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
  float mGpuMemory;
  int mUseGpuForAlign[2];
  int mUseFrameAlign;
  int mFAparamSetInd;
  int mAliComParamInd;
  bool mJustAlignNotSave;
  bool mGettingFRC;
  bool mAlignSubset;
  int mAlignStart, mAlignEnd;
  bool mDoingAdvancedFrames;
  bool mReadLocally;
  FILE *mFrameFP;
  MrcHeader mMrcHeader;
  int mAlignError;
  int mNumAligned;
  CameraThreadData *mCamTD;


public:
  int SetupConfigFile(ControlSet &conSet, CString localPath, CString &directory, 
    CString &filename, CString &configFile, BOOL stackingDeferred, 
    CameraParameters *camParams, long &numFrames);
  int StackFrames(CString localPath, CString &directory, CString &rootname, 
    long divideBy2, int divideBy, int rotateFlip, int aliSumRotFlip, float pixel, BOOL doAsync, 
    bool readLocally, int aliParamInd, long & numStacked, CameraThreadData *camTD);
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
  int SetupFrameAlignment(ControlSet & conSet, CameraParameters *camParams, float gpuMemory,
    int * useGPU, int numFrames);
  void FinishFrameAlignment(void);
  void CleanupAndFinishAlign(bool saving, int async);
  int WriteAlignComFile(CString inputFile, CString comName, int faParamInd, int useGPU,
    bool ifMdoc);
  float AlignedSubsetExposure(ShortVec & summedFrameList, float frameTime, int subsetStart,
    int subsetEnd, int maxFrames = -1);
};
