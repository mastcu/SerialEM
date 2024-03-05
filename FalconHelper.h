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
struct DarkRef;

class CFalconHelper
{
public:
  CFalconHelper(void);
  ~CFalconHelper(void);
  void Initialize(int skipConfigs);
  void DistributeSubframes(ShortVec &summedFrameList, int numReadouts, int newFrames, 
    FloatVec &userFrameFrac, FloatVec &userSubframeFrac, bool aligningInFalcon);
  float AdjustForExposure(ShortVec &summedFrameList, int numSkipBefore,
    int numSkipAfter, float exposure, float readoutInterval, FloatVec &userFrameFrac, 
    FloatVec &userSubframeFrac, bool aligningInFalcon);
  GetMember(bool, StackingFrames);
  GetMember(float, GpuMemory);
  GetMember(bool, GettingFRC);
  GetMember(int, AlignError);
  GetMember(int, NumAligned);
  GetMember(bool, AlignSubset);
  GetMember(int, AlignStart);
  GetMember(int, AlignEnd);
  SetMember(CString, LastFrameDir);
  GetMember(long, NumFiles);
  SetMember(int, RotFlipForComFile);
  SetMember(int, EERsumming);
  SetMember(int, EERsuperRes);
  GetSetMember(int, GpuForContinuousAli);
  SetMember(float, LastEERcountScaling);
  GetMember(CString, FalconRefName);
  GetSetMember(BOOL, ReadEERantialiased);
  void SetContinAliBinning(int val) { mContinuousAliParam.aliBinning = val; }
  int GetContinAliBinning() { return mContinuousAliParam.aliBinning; }
  void SetContinAliPairwise(int val) { mContinuousAliParam.numAllVsAll = val; }
  int GetContinAliPairwise() { return mContinuousAliParam.numAllVsAll; }
  void SetContinAliFilter(float val) { mContinuousAliParam.rad2Filt1 = val; }
  float GetContinAliFilter() { return mContinuousAliParam.rad2Filt1; }
  int DEFrameAlignBusy() {return 0;};
 
  int GetUseGpuForAlign(int inIMOD) {return mUseGpuForAlign[inIMOD ? 1 : 0];};
  void SetUseGpuForAlign(int inIMOD, int inVal) {mUseGpuForAlign[inIMOD ? 1 : 0] = inVal;};

private:
  CSerialEMApp *mWinApp;
  CCameraController *mCamera;
  FrameAlign *mFrameAli;
  float mReadoutInterval;               // Local copy of frame time
  CArray<CString, CString> mOtherLines; // Saved other lines from the config file
  CString mStorePrefix, mFramePrefix;
  int mNumStartLines;
  BOOL mConfigBackedUp;
  BOOL mLastSavedDummy;
  CFileStatus mLastDummyStatus;
  std::set<std::string> mPreExistingRaws;
  std::map<int, std::string> mFileMap;
  int mBaseFileIndex;
  ScopePluginFuncs *mPlugFuncs;
  long mNumFiles;                       // Number of frames found on first look
  long mHeadNums[8];                    // Header values from raw file/stack
  int mNx, mNy;                         // The X and Y size from first file or stack
  int mChunkSize, mNumChunks;           // Variables to set writing in chunks
  short *mRotData;                      // temporary buffer if doing rotation
  int mRotateFlip, mDivideBy, mAliSumRotFlip;  // Saved values of passed-in variables
  long mDivideBy2;
  float mPixel, mFloatScale;                         // Pixel size for header
  int mRotFlipForComFile;               // Value to put in com file
  long mNumStacked;                     // NUmber of frames stacked so far
  CString mLocalPath;  // Original folder of files when stacking, or name of stack to read
  CString mDirectory, mRootname;        // folder and filename when stacking
  CString mLastFrameDir;                // Last dir frame stack was saved in
  std::map<int, std::string>::iterator mIter;
  int mFileInd;                         // Running index to file/frame being processed
  int mStackError;                      // Error flag from reading/stacking
  KStoreMRC *mStoreMRC;                 // Input or output file
  KImageShort *mImage, *mImage2;        // Main image, second image when saving async
  bool mDeletePrev;                     // Flag that it is OK to delete previous frame
  double mWaitStart;                    // Variables controlling wait for more frames
  int mWaitCount;
  int mWaitMaxCount;
  int mWaitInterval;
  std::vector<long> mReadouts;          // Vector of subframes per frame
  int mEERsumming;                      // Number of frames to sum if reading from EER
  int mEERsuperRes;                     // Super-resolution to read: 0, 1, or 2
  BOOL mReadEERantialiased;             // Flag to read EER files with antialiasing
  bool mStackingFrames;                 // Flag that we are stacking frames
  int mUseImage2;                       // Flag to use the second buffer
  std::vector<long> mDeleteList;        // List of frames files to delete
  int mStartedAsyncSave;                // Flag if started asynchronous saving
  CString mFalconRefName;               // Name of Falcon 4 reference without path
  int mLastFalconRefCheck;              // Minute time of last check
  int mLastUseOfFalconRef;              // Minute time when last used
  CString mLastRefSavedInDir;           // Last directory reference was copied to
  float *mFalconGainRef;                // Buffer for gain reference
  CString mLastGainRefRead;             // Name without path of last one read
  int mSizeOfRawGainRef;                // Size of gain reference as read in
  int mGainRefSuperRes;                 // Superres of gain reference as stored
  CameraDefects mFalconDefects;         // Defects from gain reference
  bool mFoundFalconDefects;             // Flag that defects were found
  int mGainSizeX, mGainSizeY;           // Size of (expanded) gain reference
  float mLastEERcountScaling;           // Count scaling used in acquiring in an EER shot
  float mAlignTruncation;               // truncation - allowed in EER aligning

  bool mProcessingPlugin;               // Flag that processing frames from plugin
  float mGpuMemory;                     // Memory of GPU here, or 0 if none
  int mUseGpuForAlign[2];               // Flags for using GPU here and in IMOD
  int mUseFrameAlign;                   // Set only if doing frame alignment here
  int mFAparamSetInd;                   // Parameter set index if doing alignment here
  int mAliComParamInd;                  // And separate index if writing  a com file
  bool mJustAlignNotSave;               // Flag if aligning without saving (i.e. deleting)
  bool mGettingFRC;                     // Flag if getting the FRC in the alignment
  bool mAlignSubset;                    // Flag if aligning subset
  int mAlignStart, mAlignEnd;           // The subset start and end
  DarkRef *mDarkp;                      // Dark and gain references to apply in align
  DarkRef *mGainp;
  bool mNeedNormHere;                   // Flag that we need to normalize in camcontroller
  int mProcessing;                      // Parameters to save and send for normalizing
  int mRemoveXrays;
  DarkRef *mNormGain;
  DarkRef *mNormDark;
  CameraParameters *mNormParam;
  int mNormTop, mNormBot, mNormRight, mNormLeft;
  int mNormBinning;
  int mNormDMSizeX, mNormDMSizeY;
  int mNormImageType;
  bool mDoingAdvancedFrames;            // Flag fror using advanced vs basic interface
  bool mReadLocally;                    // Flag to read FEI stack directly
  FILE *mFrameFP;                       // file pointer for local reading
  MrcHeader mMrcHeader;                 // An MRC header for that file
  int mAlignError;                      // Error code from alignment errors
  int mNumAligned;                      // Number of frames aligned
  int mTrimXstart, mTrimXend;           // Limits for trimming DE/F4 frame sum to subarea
  int mTrimYstart, mTrimYend;
  bool mTrimDEsum;                      // Trim a DE or Falcon 4 sum
  bool mSumNeedsRotFlip;
  bool mDEframeNeedsTrim;
  int mFrameTrimStart, mFrameTrimEnd;
  int mSumBinning;                      // Binning relative to frames being aligned
  CameraThreadData *mCamTD;
  FrameAliParams mContinuousAliParam;   // Parameters for aligning continuous mode
  int mGpuForContinuousAli;

public:
  int SetupConfigFile(ControlSet &conSet, int consNum, CString localPath, CString &directory, 
    CString &filename, CString &configFile, BOOL stackingDeferred, 
    CameraParameters *camParams, long &numFrames);
  int ManageFalconReference(bool saving, bool aligning, CString localFramePath);
  CString FindEERGainReference();
  int StackFrames(CString localPath, CString &directory, CString &rootname, 
    long divideBy2, int divideBy, float floatScale, int rotateFlip, int aliSumRotFlip, float pixel, BOOL doAsync, 
    bool readLocally, int aliParamInd, long & numStacked, CameraThreadData *camTD);
  const char * GetErrorString(int code);
  int BuildFileMap(CString localPath, CString &directory);
  void StackNextTask(int param);
  void CleanupStacking(void);
  int StackingWaiting();
  static int TaskStackingBusy();
  void CheckAsyncSave(void);
  int GetFrameTotals(ShortVec & summedFrameList, int & totalSubframes, 
    int maxFrames = -1);
  float AdjustSumsForExposure(CameraParameters * camParams, ControlSet * conSet, float exposure, int conSsum = RECORD_CONSET);
  int CheckFalconConfig(int setState, int & state, const char * message);
  int SetupFrameAlignment(ControlSet & conSet, CameraParameters *camParams, float gpuMemory,
    int * useGPU, int numFrames);
  void FinishFrameAlignment(int binning);
  void CleanupAndFinishAlign(bool saving, int async);
  int WriteAlignComFile(CString inputFile, CString comName, int faParamInd, int useGPU,
    bool ifMdoc, int frameX, int frameY, int EERsumming, float pixelSize);
  float AlignedSubsetExposure(ShortVec & summedFrameList, float frameTime, int subsetStart,
    int subsetEnd, int maxFrames = -1);
  int AlignFramesFromFile(CString filename, ControlSet &conSet, int rotateFlip, int divideBy2, 
    float scaling, int &numFrames, CameraThreadData *td);
  void AlignNextFrameTask(int param);
  void GetSavedFrameSizes(CameraParameters *camParams, const ControlSet *conSet, 
    int & frameX, int & frameY, bool acquiredSize);
  int SuperResHardwareBinDivisor(CameraParameters *camParams, const ControlSet *conSet);
  int GetEERsuperFactor(int superRes);
  bool AreDefectsNonEmpty(CameraDefects *defects);
  void InitializePointers(void);
  int ProcessPluginFrames(CString &directory, CString &rootname, ControlSet &conSet, bool save, bool align,
    int aliParamInd, int divideBy2, float pixel, CameraThreadData *camTD);
  int MakeBackupIfFileExists(CString &str);
  int SetupContinuousAlign(ControlSet & conSet, CameraThreadData *camTD, int divideBy2,
    int numExpected);
  int NextContinuousFrame();

};
