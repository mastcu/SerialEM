/*
 *  gpuframe.h -- header file for gpuframe
 *
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

#define GPUFRAME_VERSION 102
#define NICE_GPU_DIVISOR 32
#define MAX_GPU_GROUP_SIZE 5
typedef void (*CharArgType)(const char *message);

class FrameGPU {
  
 public:
  FrameGPU();
  int gpuAvailable(int nGPU, float *memory, int debug);
  int setupSumming(int fullXpad, int fullYpad, int sumXpad, int sumYpad, int evenOdd);
  void setUnpaddedSize(int unpadX, int unpadY, int flags, int debug);
  int setPreProcParams(float *gainRef, int nxGain, int nyGain, float truncLimit,
                        unsigned char *defectMap, int camSizeX, int camSizeY);
  void setBinPadParams(int xstart, int xend, int ystart, int yend, int binning,
                      int nxTaper, int nyTaper, int type, int filtType, int noiseLen);
  int setupAligning(int alignXpad, int alignYpad, int sumXpad, int sumYpad,
                    float *alignMask, int aliFiltSize, int groupSize, int expectStackSize,
                    int doAlignSum);
  int setupDoseWeighting(float *filter, int filtSize, float delta);
  int addToFullSum(float *fullArr, float shiftX, float shiftY);
  int returnSums(float *sumArr, float *evenArr, float *oddArr, int evenOddOnly);
  int returnUnweightedSum(float *sumArr);
  void cleanup();
  void rollAlignStack();
  void rollGroupStack();
  int subtractAndFilterAlignSum(int stackInd, int groupRefine);
  int newFilterMask(float *alignMask);
  int shiftAddToAlignSum(int stackInd, float shiftX, float shiftY, int shiftSource);
  int crossCorrelate(int aliInd, int refInd, float *subarea, int subXoffset,
                     int subYoffset);
  int processAlignImage(float *binArr, int stackInd, int groupInd, int stackOnGpu);
  void numberOfAlignFFTs(int *numBinPad, int *numGroups);
  int returnAlignFFTs(float **saved, float **groups, float *alignSum, float *workArr);
  int returnStackedFrame(float *array, int *frameNum);
  void cleanSumItems();
  void cleanAlignItems();
  void zeroTimers() {mWallCopy = mWallFFT = mWallShift = mWallFilt = mWallConj = 
      mWallExtract = mWallSubtract = mWallAddEO = mWallGroup = mWallPreproc = 
      mWallRedPad = mWallNoise = 0.;};
  void printTimers();
  int clearAlignSum();
  int sumIntoGroup(int stackInd, int groupInd);
  setMember(int, GroupSize);
  
 private:
  void clearAllItems();
  void cleanPreProc();
  void unbindVariableBindings();
  int manageShiftTrigs(int xpad, int ypad);
  void freeCudaArray(float **array);
  void freeCudaStack(std::vector<float *> &saved);
  int runPreprocess(void *dev2dArr, int type2d, int frame = 0);
  void dumpFFT(float *fft, int nxPad, int nyPad, const char *descrip, int doReal);
  void dumpImage(float *image, int nxDim, int nxPad, int nyPad, int isCorr,
                 const char *descrip, int frame = 0);
  void dumpUnpadArray(void *array, int sizeX, int sizeY, int type, const char *descrip,
                      int frame = 0);
  int testReportErr(const char *mess);
  int testErrCode(int errCode, const char *mess, int cleanAll);
  void pflerr(const char *format, ...);
  int bindSumArray(int needBound);
  void unbindUnpadArray(int type2d);
  int bindFullOrCorrArray(float *fullArr, size_t sizeTmp);
  void normalize(float *data, float scale, int numPix);
  float frameEdgeMean(void *array, int type, int nxdim, int ixlo, int ixhi, int iylo,
                      int iyhi);
  int manageRawTempArray(int aligning);
  int manageUnpadWorkArray();
  int shiftAddCommon(float *fullArr, float *sumArr, int needBound, 
                     int fullXpad, int fullYpad, int sumXpad, int sumYpad,
                     float shiftX, float shiftY, int shiftSource, bool applyDoseFilt);
  void makeReductionWeights(int startCoord, int support, FloatVec &weights,
                            int &delStart, int &delEnd);
  
  float *mWorkFullSize;               // Temp array for full-size padded images
  float *mEvenSum;                    // For even or only sum
  float *mOddSum;                     // For odd sum
  float *mNonDWsum;                   // For non-dose-weighted sum
  std::vector<float *>mSavedBinPad;   // Device arrays with bin/pad FFTs or images
  std::vector<float *>mSavedGroups;   // Device arrays with bin/pad group FFTs
  FloatVec mUnpadEdgeMeans;           // Edge means for full unpadded images on stack
  IntVec mSavedFrameNums;             // Frame numbers for full images on stack
  float *mWorkBinPad;                 // Temp array for bin/padded images
  float *mCorrBinPad;
  float *mAlignSum;                   // Cumulative alignment sum
  float *mFiltMask;                   // Filter image mask
  float *mRealCorr;
  float *mSubareaCorr;
  float *mBoundToFull;
  float *mHostSubarea;
  float *mReducedInXlinear;           // Output array for reduction in X if no surfaces
  float *mProcessedLinear;            // Output array for pre-processing if no surfaces
  
  float *mXshiftTrig;
  float *mYshiftTrig;
  int mXtrigSize, mYtrigSize;
  int mUnpaddedX, mUnpaddedY;   // Original size in X/Y
  int mUnpaddedBytes;
  int mFullXpad, mFullYpad;     // Full padded size in X/Y
  int mSumXpad, mSumYpad;       // Size for padded sum arrays
  int mAlignXpad, mAlignYpad;   // Size for bin/pad align arrays
  int mDoEvenOdd;             // Flag to make even and odd sums
  int mDoUnDWsum;             // flag to make unweighted sum
  int mDoNoiseTaper;          // Flag to do noise-taper-pad here
  int mDoAlignBinPad;         // Flag to do bin-pad for aligning here
  int mStackUnpadded;         // Flag to keep stack of unpadded frames
  int mStackIsLimited;        // Flag that temp array may be needed
  int mDoGainNorm;            // Flag to do gain normalization
  int mCorrectDefects;        // Flag to correct defects from map
  float mTruncLimit;          // Limit for truncating normalized value
  int mDoPreprocess;          // Summary flag of whether doing pre-processing
  int mNxGain, mNyGain;       // Size of stored gain ref
  int mCamSizeX, mCamSizeY;   // Size of stored defect map
  int mNxRaw, mNyRaw;         // Size of allocated raw temp array
  int mRedColX, mRedColY;     // Size of allocated array for column reduction
  int mXstart, mXend;         // Limits for trim in the bin-pad
  int mYstart, mYend;
  int mAliBinning;            // Binning/reduction for align
  int mNxTaper, mNyTaper;     // Tapering for the bin-pad
  int mStackType;             // Mode of stack (will be type for bin-pad if no stack)
  int mNoiseLength;           // Length of noise region for noise-pad
  int mAntiFiltType;          // Type of filter for antialias reduction
  int mGpuStackLimit;         // Limit to # to stack on GPU
  int mNumOnUnpadStack;       // Number currently on this stack
  int mTempFloatSizeX;        // Size of float temp array for preprocessing
  int mTempFloatSizeY;
  int mDebug;                 // General debug flag % 10
  int mTrackTime;             // (debug / 10) % 10 as in framealign, for tracking time
  int mNumFramesSummed;       // Number of frames added to full sums
  int mNumAlignedFrames;      // Number of frames send to ProcessAlignImage
  int mMax_gflops_device;
  int mDeviceSelected;
  int mBoundSum;
  int mExpectStackSize;
  int mDoAlignSum;
  int mGroupSize;
  int mAliFiltSize, mBigSubareaSize;
  int mNoSurfaces;            // Flag that there are no surfaces
  size_t mFullBytes, mSumBytes, mAlignBytes;
  double mWallStart, mWallCopy, mWallFFT, mWallShift, mWallFilt, mWallConj, mWallExtract;
  double mWallSubtract, mWallAddEO, mWallGroup, mWallPreproc, mWallRedPad, mWallNoise;
  int mDWFilterSize;          // Size of dose weight filter
  float mDWFilterDelta;       // Delta to get from index to frequency
  float *mDoseWgtFilter;      // Dose weight filter
};

extern "C" {
  DLL_EX_IM int fgpuGpuAvailable(int nGPU, float *memory, int debug);
  DLL_EX_IM void fgpuSetUnpaddedSize(int unpadX, int unpadY, int flags, int debug);
  DLL_EX_IM int fgpuSetPreProcParams(float *gainRef, int nxGain, int nyGain,
                                      float truncLimit, unsigned char *defectMap,
                                      int camSizeX, int camSizeY);
  DLL_EX_IM void fgpuSetBinPadParams(int xstart, int xend, int ystart, int yend,
                                    int binning, int nxTaper, int nyTaper, 
                                    int type, int filtType, int noiseLen);
  DLL_EX_IM int fgpuSetupSumming(int fullXpad, int fullYpad, int sumXpad, int sumYpad,
                                 int evenOdd);
  DLL_EX_IM int fgpuSetupAligning(int alignXpad, int alignYpad, int sumXpad, int sumYpad,
                    float *alignMask, int aliFiltSize, int groupSize, int expectStackSize,
                    int doAlignSum);
  DLL_EX_IM int fgpuSetupDoseWeighting(float *filter, int filtSize, float delta);
  DLL_EX_IM int fgpuAddToFullSum(float *fullArr, float shiftX, float shiftY);
  DLL_EX_IM int fgpuReturnSums(float *sumArr, float *evenArr, float *oddArr,
                               int evenOddOnly);
  DLL_EX_IM int fgpuReturnUnweightedSum(float *sumArr);
  DLL_EX_IM void fgpuCleanup();
  DLL_EX_IM void fgpuRollAlignStack();
  DLL_EX_IM void fgpuRollGroupStack();
  DLL_EX_IM int fgpuSubtractAndFilterAlignSum(int stackInd, int groupRefine);
  DLL_EX_IM int fgpuNewFilterMask(float *alignMask);
  DLL_EX_IM int fgpuShiftAddToAlignSum(int stackInd, float shiftX, float shiftY,
                                       int shiftSource);
  DLL_EX_IM int fgpuCrossCorrelate(int aliInd, int refInd, float *subarea, int subXoffset,
                     int subYoffset);
  DLL_EX_IM int fgpuProcessAlignImage(float *binArr, int stackInd, int groupInd, 
                                      int stackOnGpu);
  DLL_EX_IM void fgpuNumberOfAlignFFTs(int *numBinPad, int *numGroups);
  DLL_EX_IM int fgpuReturnAlignFFTs(float **saved, float **groups, float *alignSum, 
                                    float *workArr);
  DLL_EX_IM int fgpuReturnStackedFrame(float *array, int *frameNum);
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
