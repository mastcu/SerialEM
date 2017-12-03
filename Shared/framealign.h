/*
 *  framealign.h -- header file for framealign
 *
 *  $Id$
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
#define GPU_FOR_SUMMING 1
#define GPU_DO_EVEN_ODD 2
#define GPU_FOR_ALIGNING 4

class FrameAlign {
 public:
  FrameAlign(void);
  ~FrameAlign(void);
  int initialize(int binSum, int binAlign, float trimFrac, int numAllVsAll,
                 int cumAlignAtEnd, int useHybrid, int deferSum, int groupSize,
                 int nx, int ny, float padFrac, float taperFrac, int antiFiltType,
                 float radius1, float *radius2, float sigma1, float *sigma2, 
                 int numFilters, int maxShift, float kFactor, float maxMaxWeight,
                 int summingMode, int expectedZ, int gpuFlags, int debug);
  int setupDoseWeighting(float priorDose, float *frameDoses, float pixelSize, 
                         float critScale, float aFac, float bFac, float cFac);
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
  void cleanup(void);
  int splineSmooth(float *xShifts, float *yShifts, int numShifts, 
                   float *smoothedX, float *smoothedY, float &splineDist);
  void setPrintFunc(CharArgType func);
  void analyzeFRCcrossings(float *ringCorrs, float frcDelta, float &halfCross, 
                           float &quartCross, float &eighthCross, float &halfNyq);
  float *getFullWorkArray() {return mWorkFullSize;};
  
 private:
  int alignTwoFrames(int refInd, int binInd, float nearXshift, float nearYshift,
                     int filtInd, float &xShift, float &yShift, bool filterSubarea,
                     bool dump);
  int leastCommonMultiple(int num1, int num2);
  int addToSums(float *fullArr, int binInd, int frameNum, int filtInd = -1);
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

  CharArgType mPrintFunc;
  float *mFullEvenSum;
  float *mFullOddSum;
  float *mAlignSum;
  float *mWorkFullSize;
  std::vector<float *>mSavedFullSize;
  std::vector<float *>mSavedBinPad;
  std::vector<float *>mSavedGroups;
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
  int mNx, mNy;
  int mXstart, mXend;
  int mYstart, mYend;
  int mFullXpad, mFullYpad;
  int mSumXpad, mSumYpad;
  int mAlignXpad, mAlignYpad;
  int mAlignPix, mAlignBytes;
  int mXreduceTemp, mYreduceTemp;
  int mAliFiltSize;
  float mTrimFrac, mTaperFrac;
  int mSummingMode;
  int mUseHybrid;
  int mNumFrames;
  int mBinSum, mBinAlign;
  int mNumAllVsAll;
  int mNumFilters;
  int mBestFilt;
  float mMaxMaxWeight;
  int mMaxShift;
  int mDumpInd;
  double mWallFullFFT, mWallBinPad, mWallBinFFT, mWallReduce, mWallShift, mWallStart;
  double mWallFilter, mWallConjProd, mWallPreProc;
  int mDebug;
  bool mDumpCorrs;
  bool mDumpRefCorrs;
  bool mDumpEvenOdd;
  bool mPickedBestFilt;
  bool mDeferSumming;
  bool mGpuAligning;
  bool mGpuSumming;
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
  int mGpuLibLoaded;
  int mNumExpectedFrames;
  bool mDoingDoseWeighting;
  std::vector<float> mFrameDoses;
  std::vector<float> mDoseWgtFilter;
  float mPriorDoseCum;
  float mPixelSize;
  float mCritDoseScale;
  float mCritDoseAfac, mCritDoseBfac, mCritDoseCfac;
  float mDWFdelta;
};
#endif
