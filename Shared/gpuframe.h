/*
 *  gpuframe.h -- header file for gpuframe
 *
 *  $Id$
 */
#ifndef GPUFRAME_H
#define GPUFRAME_H

#include "cppdefs.h"

// Define macro for export of gpuframe functions under Windows
#ifndef DLL_EX_IM
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#include "Windows.h"
#define DLL_EX_IM _declspec(dllexport)
#else
#define DLL_EX_IM
#endif
#endif

#define GPUFRAME_VERSION 100
#define NICE_GPU_DIVISOR 32
#define MAX_GPU_GROUP_SIZE 5
typedef void (*CharArgType)(const char *message);

class FrameGPU {
  
 public:
  FrameGPU();
  int gpuAvailable(int nGPU, float *memory, int debug);
  int setupSumming(int fullXpad, int fullYpad, int sumXpad, int sumYpad, int evenOdd);
  int setupAligning(int alignXpad, int alignYpad, int sumXpad, int sumYpad,
                    float *alignMask, int aliFiltSize, int groupSize, int expectStackSize,
                    int doAlignSum);
  int addToFullSum(float *fullArr, float shiftX, float shiftY);
  int returnSums(float *sumArr, float *evenArr, float *oddArr, int evenOddOnly);
  void cleanup();
  void rollAlignStack();
  void rollGroupStack();
  int subtractAndFilterAlignSum(int stackInd, int groupRefine);
  int newFilterMask(float *alignMask);
  int shiftAddToAlignSum(int stackInd, float shiftX, float shiftY, int shiftSource);
  int crossCorrelate(int aliInd, int refInd, float *subarea, int subXoffset,
                     int subYoffset);
  int processAlignImage(float *binArr, int stackInd, int groupInd);
  void numberOfAlignFFTs(int *numBinPad, int *numGroups);
  int returnAlignFFTs(float **saved, float **groups, float *alignSum, float *workArr);
  void cleanSumItems();
  void cleanAlignItems();
  void zeroTimers() {mWallCopy = mWallFFT = mWallShift = mWallFilt = mWallConj = 
      mWallExtract = mWallSubtract = mWallAddEO = mWallGroup = 0.;};
  void printTimers();
  int clearAlignSum();
  int sumIntoGroup(int stackInd, int groupInd);
  setMember(int, GroupSize);
  
 private:
  void clearAllItems();
  void unbindVariableBindings();
  int manageShiftTrigs(int xpad, int ypad);
  void freeCudaArray(float **array);
  void dumpFFT(float *fft, int nxPad, int nyPad, const char *descrip, int doReal);
  void dumpImage(float *image, int nxDim, int nxPad, int nyPad, int isCorr,
                 const char *descrip);
  int testReportErr(const char *mess);
  void pflerr(const char *format, ...);
  int bindSumArray(int needBound);
  int bindFullOrCorrArray(float *fullArr, size_t sizeTmp);
  void normalize(float *data, float scale, int numPix);
  int shiftAddCommon(float *fullArr, float *sumArr, int needBound, 
                     int fullXpad, int fullYpad, int sumXpad, int sumYpad,
                     float shiftX, float shiftY, int shiftSource);
  
  float *mWorkFullSize;
  float *mEvenSum;
  float *mOddSum;
  std::vector<float *>mSavedBinPad;
  std::vector<float *>mSavedGroups;
  float *mWorkBinPad;
  float *mCorrBinPad;
  float *mAlignSum;
  float *mFiltMask;
  float *mRealCorr;
  float *mSubareaCorr;
  float *mBoundToFull;
  float *mHostSubarea;
  
  float *mXshiftTrig;
  float *mYshiftTrig;
  int mXtrigSize, mYtrigSize;
  int mFullXpad, mFullYpad;
  int mSumXpad, mSumYpad;
  int mAlignXpad, mAlignYpad;
  int mDoEvenOdd;
  int mDebug;
  int mNumFrames;
  int mMax_gflops_device;
  int mDeviceSelected;
  int mBoundSum;
  int mExpectStackSize;
  int mDoAlignSum;
  int mGroupSize;
  int mAliFiltSize, mBigSubareaSize;
  size_t mFullBytes, mSumBytes, mAlignBytes;
  double mWallStart, mWallCopy, mWallFFT, mWallShift, mWallFilt, mWallConj, mWallExtract;
  double mWallSubtract, mWallAddEO, mWallGroup;
};

extern "C" {
  DLL_EX_IM int fgpuGpuAvailable(int nGPU, float *memory, int debug);
  DLL_EX_IM int fgpuSetupSumming(int fullXpad, int fullYpad, int sumXpad, int sumYpad,
                                 int evenOdd);
  DLL_EX_IM int fgpuSetupAligning(int alignXpad, int alignYpad, int sumXpad, int sumYpad,
                    float *alignMask, int aliFiltSize, int groupSize, int expectStackSize,
                    int doAlignSum);
  DLL_EX_IM int fgpuAddToFullSum(float *fullArr, float shiftX, float shiftY);
  DLL_EX_IM int fgpuReturnSums(float *sumArr, float *evenArr, float *oddArr,
                               int evenOddOnly);
  DLL_EX_IM void fgpuCleanup();
  DLL_EX_IM void fgpuRollAlignStack();
  DLL_EX_IM void fgpuRollGroupStack();
  DLL_EX_IM int fgpuSubtractAndFilterAlignSum(int stackInd, int groupRefine);
  DLL_EX_IM int fgpuNewFilterMask(float *alignMask);
  DLL_EX_IM int fgpuShiftAddToAlignSum(int stackInd, float shiftX, float shiftY,
                                       int shiftSource);
  DLL_EX_IM int fgpuCrossCorrelate(int aliInd, int refInd, float *subarea, int subXoffset,
                     int subYoffset);
  DLL_EX_IM int fgpuProcessAlignImage(float *binArr, int stackInd, int groupInd);
  DLL_EX_IM void fgpuNumberOfAlignFFTs(int *numBinPad, int *numGroups);
  DLL_EX_IM int fgpuReturnAlignFFTs(float **saved, float **groups, float *alignSum, 
                                    float *workArr);
  DLL_EX_IM void fgpuCleanSumItems();
  DLL_EX_IM void fgpuCleanAlignItems();
  DLL_EX_IM void fgpuZeroTimers();
  DLL_EX_IM void fgpuPrintTimers();
  DLL_EX_IM int fgpuClearAlignSum();
  DLL_EX_IM int fgpuSumIntoGroup(int stackInd, int groupInd);
  DLL_EX_IM void fgpuSetGroupSize(int inVal);
  DLL_EX_IM int fgpuGetVersion(void);
  DLL_EX_IM void fgpuSetPrintFunc(CharArgType func);
}

#endif
