/*
 *  framealign.h -- header file for framealign
 *
 */
#ifndef FRAMEALIGN_H
#define FRAMEALIGN_H

// Define macro for import of gpuframe functions under Windows
#ifndef DLL_EX_IM
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#define DLL_EX_IM _declspec(dllimport)
#else
#define DLL_EX_IM
#endif
#endif

#include <vector>
#include "CorrectDefects.h"
#include "gpuframe.h"

#define MAX_ALL_VS_ALL 100
#define MAX_FILTERS 6
#define GPU_FOR_SUMMING     (1)
#define GPU_DO_EVEN_ODD     (1 << 1)
#define GPU_FOR_ALIGNING    (1 << 2)
#define GPU_DO_NOISE_TAPER  (1 << 3)
#define GPU_DO_BIN_PAD      (1 << 4)
#define STACK_FULL_ON_GPU   (1 << 5)
#define GPU_DO_GAIN_NORM    (1 << 6)
#define GPU_CORRECT_DEFECTS (1 << 7)
#define GPU_DO_PREPROCESS   (1 << 8)
#define GPU_STACK_LIMITED   (1 << 9)
#define GPU_DO_UNWGT_SUM    (1 << 10)
#define GPU_STACK_LIM_SHIFT 20
#define GPU_STACK_LIM_MASK  0xFFF
#define GPU_NUMBER_SHIFT    16           // This is used by SerialEM -> SerialEMCCD
#define GPU_NUMBER_MASK     0x7          // in calling SetupFrameAligning

class FrameAlign {
 public:
  FrameAlign(void);
  ~FrameAlign(void);
  int initialize(int binSum, int binAlign, float trimFrac, int numAllVsAll,
                 int cumAlignAtEnd, int useHybrid, int deferSum, int groupSize,
                 int nx, int ny, float padFrac, float taperFrac, int antiFiltType,
                 float radius1, float *radius2, float sigma1, float *sigma2, 
                 int numFilters, int maxShift, float kFactor, float maxMaxWeight,
                 int summingMode, int expectedZ, int makeUnwgtSum, int gpuFlags,
                 int debug);
  int setupDoseWeighting(float priorDose, float *frameDoses, float pixelSize, 
                         float critScale, float aFac, float bFac, float cFac,
                         float *reweightFilt, int &filtSize);
  int gpuAvailable(int nGPU, float *memory, int debug);
  int nextFrame(void *frame, int type, float *gainRef, int nxGain, int nyGain,
                void *darkRef, float truncLimit,
                CameraDefects *defects, int camSizeX, int camSizeY, int defBin,
                float shiftX, float shiftY);
  int finishAlignAndSum(float refineRadius2, float refineSigma2, 
                        float iterCrit, int groupRefine, int doSpline, float *alisum,
                        float *xShifts, float *yShifts, float *rawXshifts,
                        float *rawYshifts, float *ringCorrs,
                        float deltaR, int &bestFilt, float *smoothDist, float *rawDist,
                        float *resMean, float *resSD, float *meanResMax, float *maxResMax,
                        float *meanRawMax, float *maxRawMax);
  int getUnweightedSum(float *nonDWsum);
  void cleanup(void);
  int splineSmooth(float *xShifts, float *yShifts, int numShifts, 
                   float *smoothedX, float *smoothedY, float &splineDist);
  void setPrintFunc(CharArgType func);
  void analyzeFRCcrossings(float *ringCorrs, float frcDelta, float &halfCross, 
                           float &quartCross, float &eighthCross, float &halfNyq);
  float *getFullWorkArray() {return mWorkFullSize;};
  static void getPadSizesBytes(int nx, int ny, float fullTaperFrac, int sumBin,
                               int alignBin, float &fullPadSize, float &sumPadSize,
                               float &alignPadSize);
  static void gpuMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize, 
                             int numAllVsAll, int nzAlign, int refineAtEnd, int groupSize,
                             float &needForGpuSum, float &needForGpuAli);
  static float totalMemoryNeeds(float fullPadSize, int fullDataSize, float sumPadSize,
                                float alignPadSize, int numAllVsAll, int nzAlign,
                                int refineAtEnd, int numBinTests, int numFiltTests,
                                int hybridShifts, int groupSize, int doSpline,
                                int gpuFlags, int deferSum, int testMode, 
                                int startAssess, bool &sumInOnePass, int &numHoldFull);
  static float findPreprocPadGpuFlags(int unpaddedX, int unpaddedY, int dataType,
                                      int binning, bool hasGain, bool hasDefect,
                                      bool hasTrunc, int numExpected, float freeMem,
                                      float stackMargin, int inFlags, int &outFlags);

  
 private:
  int alignTwoFrames(int refInd, int binInd, float nearXshift, float nearYshift,
                     int filtInd, float &xShift, float &yShift, bool filterSubarea,
                     bool dump);
  int leastCommonMultiple(int num1, int num2);
  int addToSums(float *fullArr, int sumInd, int binInd, int frameNum, int filtInd = -1);
  void findAllVsAllAlignment(bool justForLimits);
  void adjustAndPushShifts(int topInd, int filt, int useFilt);
  void wrapImage(float *bufFrom, int nxDimFrom, int nxFrom, int nyFrom,
                 float *bufTo, int nxDimTo, int nxTo, int nyTo, int xOffset,
                 int yOffset);
  int testAndCleanup(bool failed);
  float smoothedTotalDistance(float *xShifts, float *yShifts, int numShifts, 
                              float &rawDist, float *xSmoothed = NULL, 
                              float *ySmoothed = NULL, double *variance = NULL);
  float surroundingMean(void *frame, int type, float truncLimit, int ix, int iy);
  void frameShiftFromGroups(int frame, int filt, float &shiftX, float &shiftY);
  void getAllFrameShifts(FloatVec &frameXshift, FloatVec &frameYshift, int useFilt);
  int prepareToFetchAlignFFTs(int aliFrameInd);
  void filterAndAddToSum(float *fft, float *array, int nx, int ny, float *ctf, 
                         float delta);
  void preProcessFrame(void *frame, void *darkRef, int defBin, float *fOut);
  static bool preprocPadGpuMemoryFits(int unpaddedX, int unpaddedY, int dataType,
                                      int binning, bool hasGain, bool hasDefect,
                                      bool hasTrunc, bool doPreProc, bool doNoise, 
                                      bool doBinPad, float freeMem, float &needsMem);
  int recoverGpuFullStack(bool stacking, float **binArr);
  int recoverGpuAlignFFTs(bool saving, int aliFrameInd, float *alignSum,
                          float *workArr, float **binArr, bool stacking);
  int recoverFromSummingFailure(float **fullArr, int frameInd, int sumInd);
  void cancelInitialStepsOnGPU();

  CharArgType mPrintFunc;
  float *mFullEvenSum;
  float *mFullOddSum;
  float *mUnweightSum;
  float *mAlignSum;
  float *mWorkFullSize;
  std::vector<float *>mSavedFullSize;  // Full noise taper padded arrays saved for sum
  IntVec mSavedFullFrameNum;           // List of frames saved here
  std::vector<float *>mSavedBinPad;    // Stack of binned, padded arrays for aligning
  std::vector<float *>mSavedGroups;    // Stack of groups sums for aligning
  float *mWorkBinPad;
  float *mCorrBinPad;
  float *mCorrFiltTemp;
  float *mReduceTemp;
  float *mShiftTemp;
  unsigned char **mLinePtrs;
  float *mFitMat;
  float *mFitWork;
  float *mSubFiltMask[MAX_FILTERS];
  float *mFullFiltMask;
  float *mTempSubFilt;
  float *mWrapTemp;
  FloatVec mXshifts[MAX_FILTERS + 1];
  FloatVec mYshifts[MAX_FILTERS + 1];
  FloatVec mXallShifts[MAX_FILTERS];
  FloatVec mYallShifts[MAX_FILTERS];
  float mXfitShifts[MAX_FILTERS][MAX_ALL_VS_ALL];
  float mYfitShifts[MAX_FILTERS][MAX_ALL_VS_ALL];
  float mXnearShifts[MAX_ALL_VS_ALL];
  float mYnearShifts[MAX_ALL_VS_ALL];
  float mLastXfit[MAX_FILTERS + 1][MAX_ALL_VS_ALL];
  float mLastYfit[MAX_FILTERS + 1][MAX_ALL_VS_ALL];
  float mCumulXdiff[MAX_FILTERS + 1], mCumulYdiff[MAX_FILTERS + 1];
  int mNumAsBestFilt[MAX_FILTERS];
  float mRadius2[MAX_FILTERS];
  float mSigma2[MAX_FILTERS];
  int mGpuFlags;
  int mFlagsForUnpadCall;      // Save the flags for consistency  in the two calls
  int mEvenOddForSumSetup;     // Similarly here for setupSumming calls
  int mNxGain, mNyGain;        // Saved size of last gain reference
  float *mGainRef;             // Pointer to last gain ref
  int mCamSizeX, mCamSizeY;    // Camera size non-zero for defect correction
  CameraDefects *mCamDefects;  // Pointer to last defects structure
  float mTruncLimit;           // Truncation limit
  int mDefectBin;              // Binning of defects
  int mNoiseLength;            // Length for noise-taper-pad
  int mFrameType;              // Original type of frames
  int mStackType;              // Type of images saved in full image stack
  int mNx, mNy;                // Original size of image
  int mAntiFiltType;           // Type of filter for antialias reduction
  int mXstart, mXend;          // Starting and ending coordinates of region to trim
  int mYstart, mYend;
  int mFullXpad, mFullYpad;    // Padded size of full images
  int mSumXpad, mSumYpad;      // Padded size of sum(s)
  int mAlignXpad, mAlignYpad;   // Padded size of align images
  int mAlignPix, mAlignBytes;   // Number of pixels/bytes in align images including + 2
  int mXreduceTemp, mYreduceTemp;
  int mAliFiltSize;
  float mTrimFrac, mTaperFrac;   // Trim fraction and taper fraction for align
  int mSummingMode;              // 1 for align only, -1 for sum only, 0 for both
  int mUseHybrid;
  int mNumFrames;
  int mNumFullSaved;             // Number of full frames saved in CPU stack
  int mBinSum, mBinAlign;
  int mNumAllVsAll;
  int mNumFilters;
  int mBestFilt;
  float mMaxMaxWeight;
  int mMaxShift;
  int mDumpInd;
  double mWallFullFFT, mWallBinPad, mWallBinFFT, mWallReduce, mWallShift, mWallStart;
  double mWallFilter, mWallConjProd, mWallPreProc, mWallNoise;
  int mDebug;
  int mMakeUnwgtSum;        // Flag to make a non dose-weighted sum
  bool mUnwgtOnGpu;         // Flag to make it on the GPU
  bool mDumpCorrs;
  bool mDumpRefCorrs;
  bool mDumpEvenOdd;
  bool mPickedBestFilt;
  bool mDeferSumming;       // Flag that summing is done entirely at end
  bool mGpuAligning;        // Flag that aligning is on the GPU
  bool mGpuSumming;         // Flag that summing is on the GPU
  bool mNoisePadOnGpu;      // Flag to do noise pad on GPU
  bool mBinPadOnGpu;        // Flag to do align bin pad on GPU
  bool mStackUnpadOnGpu;    // Flag to stack unpadded normed array on GPU
  int mGpuStackLimit;       // Limit to # to stack on GPU
  int mNumStackedOnGpu;     // Number currently stacked on GPU
  int mGroupSize, mGroupSizeInitial;
  float mKfactor;
  int mCumAlignAtEnd;
  float mPickRatioCrit;
  float mPickDiffCrit;
  int mFailedOftenCrit;
  int mNumFits;
  bool mReportTimes;
  float mResMeanSum[MAX_FILTERS+ 1], mResSDsum[MAX_FILTERS+ 1];
  float mResMaxSum[MAX_FILTERS+ 1], mMaxResMax[MAX_FILTERS+ 1];
  float mMaxRawMax[MAX_FILTERS+ 1], mRawMaxSum[MAX_FILTERS+ 1];
  float mFiltFunc[8193], mFiltDelta;
  int mGpuLibLoaded;          // If FrameGPU loaded: -1 untested, 0 not available, 1 yes,
  int mNumExpectedFrames;
  bool mDoingDoseWeighting;   // Overall flag for dose weighting
  FloatVec mFrameDoses;       // List of doses per frame
  FloatVec mDoseWgtFilter;    // Current filter
  float mPriorDoseCum;        // Accumulated prior dose
  float mPixelSize;           // Pixel size for these calculations
  float mCritDoseScale;       // Scaling factor (0.8 for 200 KV, etc)
  float mCritDoseAfac, mCritDoseBfac, mCritDoseCfac;   // Parameters
  float mDWFdelta;            // Scaling from pixel to frequency in /pixel
  FloatVec mReweightFilt;     // Filter to multiple by for reweighting
};
#endif
