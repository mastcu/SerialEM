/*
 *  framealign.cpp - module for aligning movie frames passed in sequentially
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 2016 by the Regents of the University of 
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 *  No ID line: it is shared between 3 different projects
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#include <Windows.h>
#endif
#include "b3dutil.h"
#include "cppdefs.h"
#include "cfft.h"
#include "framealign.h"
#include "frameutil.h"

#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#define GET_PROC(t, s, n) s = (t)GetProcAddress(sGpuModule, #n); if (!s) err++;
#define GPU_DLL_NAME "FrameGPU.dll"

typedef void (*FgpuNoArgType)(void);
typedef int (*FgpuTwoIntType)(int, int);
typedef int (*ReturnIntType)(void);
typedef void (*ReturnTwoIntType)(int *, int *);
typedef void (*SetGroupSizeType)(int);
typedef int (*GpuAvailableType)(int, float *, int);
typedef int (*SetupSummingType)(int, int, int, int, int);
typedef int (*SetupAligningType)(int, int, int, int, float *, int, int, int, int);
typedef int (*AddToFullSumType)(float *, float, float);
typedef int (*ReturnSumsType)(float *, float *, float *, int);
typedef int (*NewFilterMaskType)(float *);
typedef int (*ShiftAddToAlignSumType)(int, float, float, int);
typedef int (*CrossCorrelateType)(int, int, float *, int, int);
typedef int (*ProcessAlignImageType)(float *, int, int);
typedef int (*ReturnAlignFFTsType)(float **, float **, float *, float *);
typedef void (*SetPrintFuncType)(CharArgType func);

static HMODULE sGpuModule;
static GpuAvailableType sFgpuGpuAvailable;
static SetupSummingType sFgpuSetupSumming;
static SetupAligningType sFgpuSetupAligning;
static AddToFullSumType sFgpuAddToFullSum;
static ReturnSumsType sFgpuReturnSums;
static FgpuNoArgType sFgpuCleanup;
static FgpuNoArgType sFgpuRollAlignStack;
static FgpuNoArgType sFgpuRollGroupStack;
static FgpuTwoIntType sFgpuSubtractAndFilterAlignSum;
static NewFilterMaskType sFgpuNewFilterMask;
static ShiftAddToAlignSumType sFgpuShiftAddToAlignSum;
static CrossCorrelateType sFgpuCrossCorrelate;
static ProcessAlignImageType sFgpuProcessAlignImage;
static ReturnTwoIntType sFgpuNumberOfAlignFFTs;
static ReturnAlignFFTsType sFgpuReturnAlignFFTs;
static FgpuNoArgType sFgpuCleanSumItems;
static FgpuNoArgType sFgpuCleanAlignItems;
static FgpuNoArgType sFgpuZeroTimers;
static FgpuNoArgType sFgpuPrintTimers;
static ReturnIntType sFgpuClearAlignSum;
static FgpuTwoIntType sFgpuSumIntoGroup;
static SetGroupSizeType sFgpuSetGroupSize;
static ReturnIntType sFgpuGetVersion;
static SetPrintFuncType sFgpuSetPrintFunc;
#else
#define sFgpuGpuAvailable fgpuGpuAvailable
#define sFgpuSetupSumming fgpuSetupSumming
#define sFgpuSetupAligning fgpuSetupAligning
#define sFgpuAddToFullSum fgpuAddToFullSum
#define sFgpuReturnSums fgpuReturnSums
#define sFgpuCleanup fgpuCleanup
#define sFgpuRollAlignStack fgpuRollAlignStack
#define sFgpuRollGroupStack fgpuRollGroupStack
#define sFgpuSubtractAndFilterAlignSum fgpuSubtractAndFilterAlignSum
#define sFgpuNewFilterMask fgpuNewFilterMask
#define sFgpuShiftAddToAlignSum fgpuShiftAddToAlignSum
#define sFgpuCrossCorrelate fgpuCrossCorrelate
#define sFgpuProcessAlignImage fgpuProcessAlignImage
#define sFgpuNumberOfAlignFFTs fgpuNumberOfAlignFFTs
#define sFgpuReturnAlignFFTs fgpuReturnAlignFFTs
#define sFgpuCleanSumItems fgpuCleanSumItems
#define sFgpuCleanAlignItems fgpuCleanAlignItems
#define sFgpuZeroTimers fgpuZeroTimers
#define sFgpuPrintTimers fgpuPrintTimers
#define sFgpuClearAlignSum fgpuClearAlignSum
#define sFgpuSumIntoGroup fgpuSumIntoGroup
#define sFgpuSetGroupSize fgpuSetGroupSize
#define sFgpuSetPrintFunc fgpuSetPrintFunc
#endif


/* 
 * Constructor: Initialized all the pointers and call cleanup routine for other 
 * variables
 */
FrameAlign::FrameAlign()
{
  int i;
  mFullEvenSum = NULL;
  mFullOddSum = NULL;
  mAlignSum = NULL;
  mWorkFullSize = NULL;
  mWorkBinPad = NULL;
  mCorrBinPad = NULL;
  mCorrFiltTemp = NULL;
  mShiftTemp = NULL;
  mLinePtrs = NULL;
  mReduceTemp = NULL;
  mFitMat = NULL;
  mFitWork = NULL;
  mFullFiltMask = NULL;
  mTempSubFilt = NULL;
  mWrapTemp = NULL;
  mDumpInd = 0;
  for (i = 0; i < MAX_FILTERS; i++)
    mSubFiltMask[i] = NULL;
  mPickDiffCrit = 5;
  mPickRatioCrit = 3.;
  mFailedOftenCrit = 3;
  mGpuFlags = 0;
  mGroupSizeInitial = 1;
  mGpuLibLoaded = -1;
  mPrintFunc = NULL;
  cleanup();
}

FrameAlign::~FrameAlign(void)
{
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
  if (mGpuLibLoaded > 0)
    FreeLibrary(sGpuModule);
#endif
}

void FrameAlign::setPrintFunc(CharArgType func)
{
  mPrintFunc = func;
  utilSetPrintFunc(func);
  if (mGpuLibLoaded > 0)
    sFgpuSetPrintFunc(func);
}


/*
 * Initialize before sending a set of frames
 */
int FrameAlign::initialize(int binSum, int binAlign, float trimFrac, int numAllVsAll,
                           int cumAlignAtEnd, int useHybrid, int deferSum, int groupSize,
                           int nx, int ny,
                           float padFrac, float taperFrac, int antiFiltType,
                           float radius1, float *radius2, float sigma1, float *sigma2,
                           int numFilters, int maxShift, float kFactor,
                           float maxMaxWeight, int summingMode, int expectedZ,
                           int gpuFlags, int debug)
{
  int alignXpad, alignYpad, sumXpad, sumYpad, fullXpad, fullYpad, ind, filt;
  int divisor, niceLimit, nxPad, nyPad, nxTrim, nyTrim, nxUse, nyUse;
  int expectStack, aliFiltSize, minFiltSize = 128;
  int xReduceTemp = 0, yReduceTemp = 0;
  bool doBinPad = trimFrac != 0. || taperFrac != 0.;

  // Make just a full filter mask with high-frequency included if only one filter,
  // no GPU, and not refining at end
  bool justFullFilt = numFilters == 1 && !cumAlignAtEnd && 
    (gpuFlags & GPU_FOR_ALIGNING) == 0;
  int niceGpuLimit = 5;

  mDebug = debug % 10;
  mReportTimes = (debug / 10) % 10 != 0;
  mDumpCorrs = (debug / 100) % 10 != 0;
  mDumpRefCorrs = (debug / 1000) % 10 != 0;
  mDumpEvenOdd = (debug / 10000) % 10 != 0;
  if (numAllVsAll < 2 + groupSize)
    numAllVsAll = 0;
  if (numAllVsAll > MAX_ALL_VS_ALL || numFilters > MAX_FILTERS || 
      (!numAllVsAll && numFilters > 1) || (gpuFlags && !doBinPad))
    return 1;

  for (filt = 0; filt < numFilters; filt++) {
    CLEAR_RESIZE(mXallShifts[filt], float, numAllVsAll * numAllVsAll);
    CLEAR_RESIZE(mYallShifts[filt], float, numAllVsAll * numAllVsAll);
    mRadius2[filt] = radius2[filt];
    mSigma2[filt] = sigma2[filt];
  }
  for (filt = 0; filt <= numFilters; filt++) {
    CLEAR_RESIZE(mXshifts[filt], float, 0);
    CLEAR_RESIZE(mYshifts[filt], float, 0);
    mResMeanSum[filt] = mResSDsum[filt] = mMaxResMax[filt] = 0.;
    mResMaxSum[filt] = mMaxRawMax[filt] = mRawMaxSum[filt] = 0.;
    mCumulXdiff[filt] = mCumulYdiff[filt] = 0.;
  }

  divisor = binSum;
  if (!doBinPad)
    divisor = leastCommonMultiple(binSum, binAlign);
  divisor *= 2;
  niceLimit = niceFFTlimit();
  if (gpuFlags)
    niceLimit = niceGpuLimit;

  // Get size of full as a multiple of the necessary divisor
  nxPad = (int)B3DMAX(32, padFrac * nx);
  nyPad = (int)B3DMAX(32, padFrac * ny);
  fullXpad = divisor * ((nx + 2 * nxPad + divisor - 1) / divisor);
  fullYpad = divisor * ((ny + 2 * nyPad + divisor - 1) / divisor);
  fullXpad = niceFrame(fullXpad, divisor, niceLimit);
  fullYpad = niceFrame(fullYpad, divisor, niceLimit);

  if (doBinPad && binAlign > 1 && selectZoomFilter(antiFiltType, 1. / binAlign, &ind))
    return 3;

  // Manage things when there is change in number of all-vs-all
  if (numAllVsAll != mNumAllVsAll || summingMode != mSummingMode || 
      cumAlignAtEnd != mCumAlignAtEnd || useHybrid != mUseHybrid ||
      groupSize != mGroupSizeInitial) {
    for (ind = 0; ind < (int)mSavedBinPad.size(); ind++)
      B3DFREE(mSavedBinPad[ind]);
    for (ind = 0; ind < (int)mSavedFullSize.size(); ind++)
      B3DFREE(mSavedFullSize[ind]);
    for (ind = 0; ind < (int)mSavedGroups.size(); ind++)
      B3DFREE(mSavedGroups[ind]);
    mSavedBinPad.clear();
    mSavedFullSize.clear();
    mSavedGroups.clear();
    B3DFREE(mFitMat);
    B3DFREE(mFitWork);
    if (numAllVsAll) {
      mFitMat = B3DMALLOC(float, ((numAllVsAll + 3) * (numAllVsAll - 1) * numAllVsAll) / 
                          2);
      mFitWork = B3DMALLOC(float, (numAllVsAll + 2) * (numAllVsAll + 2));
      if (testAndCleanup(!mFitMat || !mFitWork))
        return 2;
    }
  }    

  if (fullXpad != mFullXpad || fullYpad != mFullYpad || numAllVsAll != mNumAllVsAll ||
      summingMode != mSummingMode) {
    B3DFREE(mShiftTemp);
    B3DFREE(mWorkFullSize);
    mWorkFullSize = B3DMALLOC(float, (fullXpad + 2) * fullYpad);
    mShiftTemp = B3DMALLOC(float, fullXpad + 2);
    mLinePtrs = B3DMALLOC(unsigned char *, fullYpad);
    if (testAndCleanup(!mWorkFullSize || !mShiftTemp || !mLinePtrs))
      return 2;
    mFullXpad = fullXpad;
    mFullYpad = fullYpad;
  }

  // And size of final sum
  sumXpad = fullXpad / binSum;
  sumYpad = fullYpad / binSum;
  if (sumXpad != mSumXpad || sumYpad != mSumYpad || summingMode != mSummingMode) {
    B3DFREE(mFullEvenSum);
    B3DFREE(mFullOddSum);
    if (summingMode <= 0) {
      mFullEvenSum = B3DMALLOC(float, (sumXpad + 2) * sumYpad);
      mFullOddSum = B3DMALLOC(float, (sumXpad + 2) * sumYpad);
      if (testAndCleanup(!mFullEvenSum || !mFullOddSum))
        return 2;
    }
    mSumXpad = sumXpad;
    mSumYpad = sumYpad;
  }

  // And size of align sum
  if (!doBinPad) {
    alignXpad = fullXpad / binAlign;
    alignYpad = fullYpad / binAlign;
  } else {
    nxTrim = (int)(trimFrac * nx);
    nyTrim = (int)(trimFrac * ny);
    nxUse = 2 * binAlign * ((nx - 2 * nxTrim) / (2 * binAlign));
    nyUse = 2 * binAlign * ((ny - 2 * nyTrim) / (2 * binAlign));
    if (mDebug)
      utilPrint("nxTrim = %d  nyTrim = %d nxUse = %d  nyUse = %d\n", nxTrim, nyTrim,
                nxUse, nyUse);
    mXstart = (nx - nxUse) / 2;
    mXend = mXstart + nxUse - 1;
    mYstart = (ny - nyUse) / 2;
    mYend = mYstart + nyUse - 1;
    nxPad = (int)B3DMAX(16, padFrac * nxUse / binAlign);
    nyPad = (int)B3DMAX(16, padFrac * nyUse / binAlign);
    divisor = 2;
    alignXpad = nxUse / binAlign;
    alignYpad = nyUse / binAlign;
    if (gpuFlags & GPU_FOR_ALIGNING) {
      divisor = NICE_GPU_DIVISOR;
      alignXpad = divisor * ((alignXpad + divisor - 1) / divisor);
      alignYpad = divisor * ((alignYpad + divisor - 1) / divisor);
    }
    alignXpad = niceFrame(alignXpad, divisor, niceLimit);
    alignYpad = niceFrame(alignYpad, divisor, niceLimit);
  }
  if (mDebug)
    utilPrint("fullXpad = %d  fullYpad = %d  alignXpad = %d  alignYpad = %d\n",
              fullXpad, fullYpad, alignXpad, alignYpad);

  // Clean up extra arrays if no trimming now and there was previously
  if ((mTrimFrac || mTaperFrac) && !doBinPad) {
    for (ind = 0; ind < (int)mSavedBinPad.size(); ind++)
      B3DFREE(mSavedBinPad[ind]);
    mSavedBinPad.clear();
    B3DFREE(mCorrBinPad);
  }
  
  // Allocate the align arrays if needed
  mAlignPix = (alignXpad + 2) * alignYpad;
  mAlignBytes = mAlignPix * sizeof(float);
  if (alignXpad != mAlignXpad || alignYpad != mAlignYpad || numAllVsAll != mNumAllVsAll) {
    B3DFREE(mAlignSum);
    mAlignSum = B3DMALLOC(float, mAlignPix);
    B3DFREE(mWorkBinPad)
      mWorkBinPad = B3DMALLOC(float, mAlignPix);
    B3DFREE(mCorrBinPad);
    mCorrBinPad = B3DMALLOC(float, mAlignPix);
    if (testAndCleanup(!mAlignSum || !mWorkBinPad || !mSubFiltMask || !mCorrBinPad))
      return 2;
    for (ind = 0; ind < (int)mSavedBinPad.size(); ind++)
      B3DFREE(mSavedBinPad[ind]);
    mSavedBinPad.clear();
  }
  aliFiltSize = B3DMAX(minFiltSize, 4 * (maxShift / binAlign));
  aliFiltSize = niceFrame(aliFiltSize, 2, niceLimit);

  // Manage filter mask arrays
  if (numFilters != mNumFilters || alignXpad != mAlignXpad || alignYpad != mAlignYpad ||
      aliFiltSize != mAliFiltSize) {
    B3DFREE(mFullFiltMask);
    mFullFiltMask = B3DMALLOC(float, mAlignPix);
    if (testAndCleanup(!mFullFiltMask))
      return 2;
    
    B3DFREE(mCorrFiltTemp);
    B3DFREE(mTempSubFilt);
    B3DFREE(mWrapTemp);
    for (ind = 0; ind <= mNumFilters; ind++)
      B3DFREE(mSubFiltMask[ind]);
    if (!justFullFilt) {
      mCorrFiltTemp = B3DMALLOC(float, mAlignPix);
      mTempSubFilt = B3DMALLOC(float, (aliFiltSize + 2 ) * aliFiltSize);
      mWrapTemp = B3DMALLOC(float, (aliFiltSize + 2 ) * aliFiltSize);
      for (ind = 0; ind < numFilters; ind++) {
        mSubFiltMask[ind] = B3DMALLOC(float, (aliFiltSize + 2 ) * aliFiltSize);
        if (testAndCleanup(!mSubFiltMask[ind] || !mCorrFiltTemp || !mTempSubFilt ||
                           !mWrapTemp))
          return 2;
      }
    }
  }
  mAlignXpad = alignXpad;
  mAlignYpad = alignYpad;
  
  // And construct the filter masks.  Take square root of the full filter as it just
  // gets applied to each stored FFT before correlation
  XCorrSetCTF(sigma1, sigma2[0] * binAlign, radius1,
              !justFullFilt ? 0.71f : radius2[0] * binAlign,
              mFiltFunc, mAlignXpad, mAlignYpad, &mFiltDelta);
  for (ind = 0; ind < 8193; ind++)
    mFiltFunc[ind] = sqrt(mFiltFunc[ind]);
  for (ind = 0; ind < mAlignPix; ind++)
    mFullFiltMask[ind] = 1.;
  XCorrFilterPart(mFullFiltMask, mFullFiltMask, alignXpad, alignYpad, mFiltFunc,
                  mFiltDelta);
  mFullFiltMask[0] = 0.;
  /*for (ind = 0; ind < alignXpad + 2; ind++)
    mFullFiltMask[ind] = 0.;
    for (ind = 0; ind < alignYpad; ind++)
    mFullFiltMask[ind * (alignXpad + 2)] = 0.;*/

  for (filt = 0; filt < numFilters && !justFullFilt; filt++) {
    XCorrSetCTF(0., sigma2[filt] * binAlign, 0., radius2[filt] * binAlign,
                mFiltFunc, aliFiltSize, aliFiltSize, &mFiltDelta);
    for (ind = 0; ind < (aliFiltSize + 2 ) * aliFiltSize; ind++)
      mSubFiltMask[filt][ind] = 1.;
    XCorrFilterPart(mSubFiltMask[filt], mSubFiltMask[filt], aliFiltSize, aliFiltSize,
                    mFiltFunc, mFiltDelta);
  }

  //  Now manage the reduction temp array
  if (binSum > 1) {
    xReduceTemp = B3DMAX(sumXpad, xReduceTemp);
    yReduceTemp = B3DMAX(sumYpad, yReduceTemp);
  }
  if (!doBinPad) {
    xReduceTemp = B3DMAX(alignXpad, xReduceTemp);
    yReduceTemp = B3DMAX(alignYpad, yReduceTemp);
  }
  if (xReduceTemp != mXreduceTemp || yReduceTemp != mYreduceTemp) {
    B3DFREE(mReduceTemp);
    if (xReduceTemp) {
      mReduceTemp = B3DMALLOC(float, (xReduceTemp + 2) * yReduceTemp);
      if (testAndCleanup(mReduceTemp == NULL))
        return 2;
    }
  }
  mXreduceTemp = xReduceTemp;
  mYreduceTemp = yReduceTemp;

  mDeferSumming = (cumAlignAtEnd || (!useHybrid && numFilters > 1) || deferSum) && 
    summingMode == 0;

  // Do not use GPU for group size above the limit
  // If GPU was used and is not going to be, clean it up
  if (gpuFlags && groupSize > 5 && (gpuFlags & GPU_FOR_ALIGNING) != 0)
    gpuFlags = 0;
  if (mGpuFlags && !gpuFlags)
    sFgpuCleanup();

  // See which GPU components are not being used, and clear the component that is not
  // needed immediately
  mGpuAligning = (gpuFlags & GPU_FOR_ALIGNING) != 0 && summingMode >= 0;
  mGpuSumming = (gpuFlags & GPU_FOR_SUMMING) != 0 && summingMode <= 0;
  expectStack = B3DMIN(numAllVsAll, expectedZ);
  if (cumAlignAtEnd)
    expectStack = expectedZ;
  if (gpuFlags) {
    if (!mGpuSumming || mDeferSumming)
      sFgpuCleanSumItems();
    if (!mGpuAligning)
      sFgpuCleanAlignItems();

    // Set up aligning unconditionally
    if (mGpuAligning && sFgpuSetupAligning
        (alignXpad, alignYpad, mGpuSumming ? sumXpad : 0, mGpuSumming ? sumYpad : 0,
         mFullFiltMask, aliFiltSize, groupSize, expectStack, cumAlignAtEnd))
      gpuFlags = 0;
    
    // Set up summing unless it is deferred
    if (gpuFlags && mGpuSumming && !mDeferSumming && sFgpuSetupSumming
        (fullXpad, fullYpad, sumXpad, sumYpad, (gpuFlags & GPU_DO_EVEN_ODD) ? 1 : 0))
      gpuFlags = 0;
  }

  // Save all members for current state
  mGpuFlags = gpuFlags;
  mGpuAligning = (gpuFlags & GPU_FOR_ALIGNING) != 0 && summingMode >= 0;
  mGpuSumming = (gpuFlags & GPU_FOR_SUMMING) != 0 && summingMode <= 0;
  if (gpuFlags)
    sFgpuZeroTimers();
  mGroupSize = groupSize;
  mGroupSizeInitial = groupSize;
  mTrimFrac = trimFrac;
  mTaperFrac = taperFrac;
  mNumFrames = 0;
  mBinSum = binSum;
  mBinAlign = binAlign;
  mNumAllVsAll = numAllVsAll;
  mMaxShift = maxShift;
  mNumFilters = numFilters;
  mSummingMode = summingMode;
  mNumFits = 0;
  mKfactor = kFactor;
  mMaxMaxWeight = maxMaxWeight;
  mBestFilt = 0;
  mPickedBestFilt = false;
  mUseHybrid = useHybrid;
  for (filt = 0; filt < numFilters; filt++)
    mNumAsBestFilt[filt] = 0;
  mMaxShift = maxShift;
  mCumAlignAtEnd = cumAlignAtEnd;
  mAliFiltSize = aliFiltSize;
  mNx = nx;
  mNy = ny;
  memset(mAlignSum, 0, mAlignBytes);
  if (summingMode <= 0) {
    memset(mFullEvenSum, 0, (sumXpad + 2) * sumYpad * sizeof(float));
    memset(mFullOddSum, 0, (sumXpad + 2) * sumYpad * sizeof(float));
  }
  mWallFullFFT = mWallBinPad = mWallBinFFT = mWallReduce = mWallShift = 0.;
  mWallConjProd = mWallFilter = mWallPreProc = 0.;
  return 0;
}

/* Cleanup on failure of the given test */
int FrameAlign::testAndCleanup(bool failed)
{
  if (!failed)
    return 0;
  cleanup();
  return 2;
}

/*
 * Free all memory and reset sizes etc
 */
void FrameAlign::cleanup()
{
  int ind;
  B3DFREE(mFullOddSum);
  B3DFREE(mFullEvenSum);
  B3DFREE(mAlignSum);
  B3DFREE(mWorkBinPad);
  B3DFREE(mWorkFullSize);
  B3DFREE(mCorrBinPad);
  B3DFREE(mCorrFiltTemp);
  B3DFREE(mShiftTemp);
  B3DFREE(mLinePtrs);
  B3DFREE(mReduceTemp);
  B3DFREE(mFullFiltMask);
  B3DFREE(mTempSubFilt);
  B3DFREE(mWrapTemp);
  B3DFREE(mFitMat);
  B3DFREE(mFitWork);
  for (ind = 0; ind < (int)mSavedBinPad.size(); ind++)
    B3DFREE(mSavedBinPad[ind]);
  for (ind = 0; ind < (int)mSavedFullSize.size(); ind++)
    B3DFREE(mSavedFullSize[ind]);
  for (ind = 0; ind < (int)mSavedGroups.size(); ind++)
    B3DFREE(mSavedGroups[ind]);
  CLEAR_RESIZE(mSavedBinPad, float *, 0);
  CLEAR_RESIZE(mSavedFullSize, float *, 0);
  CLEAR_RESIZE(mSavedGroups, float *, 0);
  for (ind = 0; ind < MAX_FILTERS; ind++)
    B3DFREE(mSubFiltMask[ind]);
  mNumFilters = 0;
  mTrimFrac = 0.;
  mTaperFrac = 0.;
  mCumAlignAtEnd = 0;
  mAliFiltSize = 0;
  mUseHybrid = 0;
  mFullXpad = mFullYpad = 0;
  mAlignXpad = mAlignYpad - 0;
  mSumXpad = mSumYpad = 0;
  mXreduceTemp = mYreduceTemp = 0;
  mNumAllVsAll = -1;
  for (ind = 0; ind <= MAX_FILTERS; ind++) {
    CLEAR_RESIZE(mXshifts[ind], float, 0);
    CLEAR_RESIZE(mYshifts[ind], float, 0);
    if (ind < MAX_FILTERS) {
      CLEAR_RESIZE(mXallShifts[ind], float, 0);
      CLEAR_RESIZE(mYallShifts[ind], float, 0);
    }
  }
  if (mGpuFlags)
    sFgpuCleanup();
  mGpuFlags = 0;
}

/* 
 * Find out if GPU is available
 */
int FrameAlign::gpuAvailable(int nGPU, float *memory, int debug)
{
  int err = 0;
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
  int lastErr = 0;
  *memory = 0.;
  if (mGpuLibLoaded == 0)
    return 0;
  if (mGpuLibLoaded < 0) {
    sGpuModule = LoadLibrary(GPU_DLL_NAME);
    if (!sGpuModule) {
      lastErr = GetLastError();
      if (lastErr != ERROR_MOD_NOT_FOUND)
        utilPrint("GPU is not available: error %d occurred trying to load %s\n",
                  lastErr, GPU_DLL_NAME);
      mGpuLibLoaded = 0;
      return 0;
    }
    GET_PROC(GpuAvailableType, sFgpuGpuAvailable, fgpuGpuAvailable);
    GET_PROC(SetupSummingType, sFgpuSetupSumming, fgpuSetupSumming);
    GET_PROC(SetupAligningType, sFgpuSetupAligning, fgpuSetupAligning);
    GET_PROC(AddToFullSumType, sFgpuAddToFullSum, fgpuAddToFullSum);
    GET_PROC(ReturnSumsType, sFgpuReturnSums, fgpuReturnSums);
    GET_PROC(FgpuNoArgType, sFgpuCleanup, fgpuCleanup);
    GET_PROC(FgpuNoArgType, sFgpuRollAlignStack, fgpuRollAlignStack);
    GET_PROC(FgpuNoArgType, sFgpuRollGroupStack, fgpuRollGroupStack);
    GET_PROC(FgpuTwoIntType, sFgpuSubtractAndFilterAlignSum, 
             fgpuSubtractAndFilterAlignSum);
    GET_PROC(NewFilterMaskType, sFgpuNewFilterMask, fgpuNewFilterMask);
    GET_PROC(ShiftAddToAlignSumType, sFgpuShiftAddToAlignSum, fgpuShiftAddToAlignSum);
    GET_PROC(CrossCorrelateType, sFgpuCrossCorrelate, fgpuCrossCorrelate);
    GET_PROC(ProcessAlignImageType, sFgpuProcessAlignImage, fgpuProcessAlignImage);
    GET_PROC(ReturnTwoIntType, sFgpuNumberOfAlignFFTs, fgpuNumberOfAlignFFTs);
    GET_PROC(ReturnAlignFFTsType, sFgpuReturnAlignFFTs, fgpuReturnAlignFFTs);
    GET_PROC(FgpuNoArgType, sFgpuCleanSumItems, fgpuCleanSumItems);
    GET_PROC(FgpuNoArgType, sFgpuCleanAlignItems, fgpuCleanAlignItems);
    GET_PROC(FgpuNoArgType, sFgpuZeroTimers, fgpuZeroTimers);
    GET_PROC(FgpuNoArgType, sFgpuPrintTimers, fgpuPrintTimers);
    GET_PROC(ReturnIntType, sFgpuClearAlignSum, fgpuClearAlignSum);
    GET_PROC(FgpuTwoIntType, sFgpuSumIntoGroup, fgpuSumIntoGroup);
    GET_PROC(SetGroupSizeType, sFgpuSetGroupSize, fgpuSetGroupSize);
    GET_PROC(ReturnIntType, sFgpuGetVersion, fgpuGetVersion);
    GET_PROC(SetPrintFuncType, sFgpuSetPrintFunc, fgpuSetPrintFunc);
    if (err)
      utilPrint("GPU is not available: %d functions failed to load from %s\n",
                err, GPU_DLL_NAME);
    if (!err && sFgpuGetVersion() != GPUFRAME_VERSION) {
      utilPrint("GPU is not available: FrameGPU version (%d) does not match "
                "framealign version (%d)\n",  sFgpuGetVersion(), GPUFRAME_VERSION);
      err = 1;
    }
    if (err) {
      FreeLibrary(sGpuModule);
      mGpuLibLoaded = 0;
      return 0;
    }
    mGpuLibLoaded = 1;
    if (mPrintFunc)
      sFgpuSetPrintFunc(mPrintFunc);
  }
#endif
  err = sFgpuGpuAvailable(nGPU, memory, debug);
  if (!err)
    utilPrint("GPU is not available%s\n",
              debug ? "" : "; run with debugging output for details");
  return err;
}

/*
 * MACROS for gain normalization and trunction
 */
#define NORM_TRUNC(a, b)                                                \
  case a:                                                               \
  for (ix = 0; ix < nxt; ix++) {                                        \
    val = (b) * gainp[ix];                                              \
    if (val > truncLimit)                                               \
      val = CorDefSurroundingMean(frame, type, nxt, nyt, truncLimit, ix, iy); \
    fOut[base + ix] = val;                                              \
  }                                                                     \
  break;

#define NORM_ONLY(a, b)                                 \
  case a:                                               \
  for (ix = 0; ix < nxt; ix++) {                        \
    val = (b) * gainp[ix];                              \
    fOut[base + ix] = val;                              \
  }                                                     \
  break;

#define TRUNC_ONLY(a, b)                                                \
  case a:                                                               \
  for (ix = 0; ix < nxt; ix++) {                                        \
    val = (b);                                                          \
    if (val > truncLimit)                                               \
      val = CorDefSurroundingMean(frame, type, nxt, nyt, truncLimit, ix, iy); \
    fOut[base + ix] = val;                                              \
  }                                                                     \
  break;

#define JUST_COPY(a, b)                                 \
  case a:                                               \
  for (ix = 0; ix < nxt; ix++)                          \
    fOut[base + ix] = (b);                              \
  break;

/*
 * Operate on the next frame
 */
int FrameAlign::nextFrame(void *frame, int type, float *gainRef, int nxGain, int nyGain,
                          void *darkRef, float truncLimit,
                          CameraDefects *defects, int camSizeX, int camSizeY, int defBin,
                          float shiftX, float shiftY)
{
  bool saving = (mNumAllVsAll > 0 || mCumAlignAtEnd || mDeferSumming) && mSummingMode >=0;
  bool doBinPad = mTaperFrac != 0. || mTrimFrac != 0.;
  float *binArr = mWorkBinPad;
  float *fullArr = mWorkFullSize;
  float *tempBin, *groupArr;
  int nxBin, nyBin, nxTaper, nyTaper, ind, ref, xOffset, yOffset, useInd;
  int top, left, bottom, right, gainXoff, gainYoff, ix, iy, filt, useFilt, base;
  int numThreads, maxThreads;
  float xShift, yShift, val, nearXshift = 0., nearYshift = 0.;
  bool needExtract = true;
  bool addToFull = mSummingMode < 0 || (mSummingMode == 0 &&  !mDeferSumming);
  bool filterSubarea = mCumAlignAtEnd || (mGpuFlags & GPU_FOR_ALIGNING) != 0;
  int useType = type;
  int numFrameForAVA = mNumAllVsAll + mGroupSize - 1;
  int numBinPadForAVA = mGroupSize > 1 ? mGroupSize : mNumAllVsAll;
  int sumFrameInd = mDeferSumming ? mNumFrames : B3DMIN(mNumFrames, numFrameForAVA - 1);
  int aliFrameInd = mCumAlignAtEnd ? mNumFrames : B3DMIN(mNumFrames, numBinPadForAVA - 1);
  int groupInd = B3DMIN(mNumFrames + 1 - mGroupSize, mNumAllVsAll - 1);
  int nxDimForBP = mNx;
  int nxForBP = mNx, nyForBP = mNy;
  float *useFrame = (float *)frame;
  float *fOut, *gainp;
  unsigned char *bFrame = (unsigned char *)frame;
  short *sFrame = (short *)frame;
  unsigned short *usFrame = (unsigned short *)frame;
  float *fFrame = (float *)frame;
  short *sDark = (short *)darkRef;
  unsigned short *usDark = (unsigned short *)darkRef;

  // PROCESS ALL-VS-ALL RESULT FIRST
  if (mNumAllVsAll && mNumFrames > mGroupSize - 1 && mSummingMode >= 0)
    findAllVsAllAlignment(mNumFrames < numFrameForAVA);
  if (mNumAllVsAll && mNumFrames >= numFrameForAVA && mSummingMode >= 0) {
    
    for (filt = 0; filt <= mNumFilters; filt++) {
      useFilt = filt == mNumFilters ? mBestFilt : filt;

      // First time, just set the first two shifts and add in 0
      if (mNumFrames == numFrameForAVA) {
        mXshifts[filt].push_back(mXfitShifts[useFilt][0]);
        mYshifts[filt].push_back(mYfitShifts[useFilt][0]);
        mXshifts[filt].push_back(mXfitShifts[useFilt][1]);
        mYshifts[filt].push_back(mYfitShifts[useFilt][1]);
        if (addToFull && filt == mNumFilters && addToSums(mSavedFullSize[0], -9, 0)) {
          cleanup();
          return 2;
        }
      } else {

        // Later, get mean difference between last and this set of shifts and adjust
        adjustAndPushShifts(1, filt, useFilt);
      }
      if (addToFull && filt == mNumFilters && addToSums
          (mSavedFullSize[sumFrameInd + 2 - numFrameForAVA], -9,
           mNumFrames + 1 - numFrameForAVA)) {
        cleanup();
        return 2;
      }

      // Save the shifts
      for (ind = 0; ind < mNumAllVsAll; ind++) {
        mLastXfit[filt][ind] = mXfitShifts[useFilt][ind];
        mLastYfit[filt][ind] = mYfitShifts[useFilt][ind];
      }
      
      // Shift the all-vs-all matrix down
      if (filt < mNumFilters) {
        for (ref = 1; ref < mNumAllVsAll - 1; ref++) {
          for (ind = ref + 1; ind < mNumAllVsAll; ind++) {
            mXallShifts[filt][(ref - 1) * mNumAllVsAll + ind - 1] = 
              mXallShifts[filt][ref * mNumAllVsAll + ind];
            mYallShifts[filt][(ref - 1) * mNumAllVsAll + ind - 1] = 
              mYallShifts[filt][ref * mNumAllVsAll + ind];
          }
        }
      }
    }

    // Roll the saved sum array if no cumulative alignment and using hybrid or only filter
    if (!mDeferSumming && mSummingMode <= 0) {
      utilRollSavedFrames(mSavedFullSize, numFrameForAVA);
    }
  }

  // Roll the saved align array if no cumulative alignment
  if (mNumAllVsAll && mNumFrames >= numBinPadForAVA && mSummingMode >= 0 && 
      !mCumAlignAtEnd) {
    if (mGpuAligning) {
      sFgpuRollAlignStack();
      if (mGroupSize > 1 && mNumFrames >= mNumAllVsAll + mGroupSize - 1)
        sFgpuRollGroupStack();
    } else {
      utilRollSavedFrames(mSavedBinPad, numBinPadForAVA);
      if (mGroupSize > 1 && mNumFrames >= mNumAllVsAll + mGroupSize - 1) {
        utilRollSavedFrames(mSavedGroups, mNumAllVsAll);
      }
    }
  }

  // Substitute the save arrays for the working ones; create new and push if needed
  if (saving) {
    if (!mGpuAligning) {
      if (aliFrameInd < (int)mSavedBinPad.size()) {
        binArr = mSavedBinPad[aliFrameInd];
      } else {
        binArr = B3DMALLOC(float, mAlignPix);
        if (testAndCleanup(!binArr))
          return 1;
        mSavedBinPad.push_back(binArr);
      }
    }
    if (mSummingMode <= 0) {
      if (sumFrameInd < (int)mSavedFullSize.size()) {
        fullArr = mSavedFullSize[sumFrameInd];
      } else {
        fullArr =  B3DMALLOC(float, (mFullXpad + 2) * mFullYpad);
        if (testAndCleanup(!fullArr))
          return 1;
        mSavedFullSize.push_back(fullArr);
      }
    }
  }

  // PRE-PROCESS IF ANY
  mWallStart = wallTime();
  if (gainRef || truncLimit > 0 || camSizeX > 0) {
    if (gainRef) {
      gainXoff = (nxGain - mNx) / 2;
      gainYoff = (nyGain - mNy) / 2;
    }
    fOut = fullArr;
    maxThreads = 1;
    if (truncLimit > 0 && gainRef)
      maxThreads = 6;
    else if (truncLimit > 0 || gainRef)
      maxThreads = 3;
    numThreads = numOMPthreads(maxThreads);
    int nxt = mNx;
    int nyt = mNy;
#pragma omp parallel for num_threads(numThreads)                        \
  shared(nxt, nyt, gainRef, nxGain, gainXoff, gainYoff, bFrame, sFrame, usFrame, fFrame, \
         fOut, sDark, usDark, type, frame, truncLimit)              \
         private(base, iy, ix, gainp, val)
    for (iy = 0; iy < nyt; iy++) {
      base = iy * nxt;
      if (gainRef) {
        gainp = &gainRef[(iy + gainYoff) * nxGain + gainXoff];
        if (darkRef && truncLimit > 0) {
        
          // Dark and gain with truncation
          switch (type) {
            NORM_TRUNC(MRC_MODE_BYTE, bFrame[base + ix] - sDark[base + ix]);
            NORM_TRUNC(MRC_MODE_SHORT, sFrame[base + ix] - sDark[base + ix]);
            NORM_TRUNC(MRC_MODE_USHORT, usFrame[base + ix] - usDark[base + ix]);
            NORM_TRUNC(MRC_MODE_FLOAT, fFrame[base + ix] - sDark[base + ix]);
          }
        } else if (truncLimit > 0) {

          // Gain norm with trunction
          switch (type) {
            NORM_TRUNC(MRC_MODE_BYTE, bFrame[base + ix]);
            NORM_TRUNC(MRC_MODE_SHORT, sFrame[base + ix]);
            NORM_TRUNC(MRC_MODE_USHORT, usFrame[base + ix]);
            NORM_TRUNC(MRC_MODE_FLOAT, fFrame[base + ix]);
          }
        } else if (darkRef) {

          // Dark and Gain without truncation
          switch (type) {
            NORM_ONLY(MRC_MODE_BYTE, bFrame[base + ix] - sDark[base + ix]);
            NORM_ONLY(MRC_MODE_SHORT, sFrame[base + ix] - sDark[base + ix]);
            NORM_ONLY(MRC_MODE_USHORT, usFrame[base + ix] - usDark[base + ix]);
            NORM_ONLY(MRC_MODE_FLOAT, fFrame[base + ix] - sDark[base + ix]);
          }
        } else {

          // Gain norm without truncation
          switch (type) {
            NORM_ONLY(MRC_MODE_BYTE, bFrame[base + ix]);
            NORM_ONLY(MRC_MODE_SHORT, sFrame[base + ix]);
            NORM_ONLY(MRC_MODE_USHORT, usFrame[base + ix]);
            NORM_ONLY(MRC_MODE_FLOAT, fFrame[base + ix]);
          }
        }
      } else if (truncLimit > 0) {

        // Truncation only
        switch (type) {
          TRUNC_ONLY(MRC_MODE_BYTE, bFrame[base + ix]);
          TRUNC_ONLY(MRC_MODE_SHORT, sFrame[base + ix]);
          TRUNC_ONLY(MRC_MODE_USHORT, usFrame[base + ix]);
          TRUNC_ONLY(MRC_MODE_FLOAT, fFrame[base + ix]);
        }
      } else {

        // Or copying to the float array for defect correction
        switch (type) {
          JUST_COPY(MRC_MODE_BYTE, bFrame[base + ix]);
          JUST_COPY(MRC_MODE_SHORT, sFrame[base + ix]);
          JUST_COPY(MRC_MODE_USHORT, usFrame[base + ix]);
          JUST_COPY(MRC_MODE_FLOAT, fFrame[base + ix]);
        }
      }
    }

    // Defect correction: pass one past edge on right and bottom
    if (camSizeX > 0) {
      left = (camSizeX / defBin- mNx) / 2;
      top = (camSizeY /defBin - mNy) / 2;
      right = left + mNx;
      bottom = top + mNy;
      CorDefCorrectDefects(defects, fullArr, MRC_MODE_FLOAT, defBin, top, left,
                           bottom, right);
    }

    // Substitute source pointer and type
    useFrame = fullArr;
    useType = MRC_MODE_FLOAT;
    //utilDumpImage(fullArr, mNx, mNx, mNy, 0, "processed original");
  }

  // PROCESS THE CURRENT IMAGE: Get the padded full image
  if (mSummingMode <= 0 || !doBinPad) {
    // Big artifacts from the taper method.
    //sliceTaperOutPad(useFrame, useType, mNx, mNy, fullArr, mFullXpad + 2, mFullXpad,
    //mFullYpad, 0, 0.);
    sliceNoiseTaperPad(useFrame, useType, mNx, mNy, fullArr, mFullXpad + 2, mFullXpad,
                       mFullYpad, 80, 4, mShiftTemp);
    nxDimForBP = mFullXpad + 2;
    nxForBP = mFullXpad;
    nyForBP = mFullYpad;
  } else if (useType != MRC_MODE_FLOAT) {

    // This can't be speeded up even though copyToCenter looks suspicious.  Tried copying
    // forward in a simple function and it was actually twice as fast to copy backwards
    sliceTaperOutPad(useFrame, useType, mNx, mNy, fullArr, mNx,  mNx, mNy, 0, 0.);
  } else {
    fullArr = (float *)useFrame;
  }
  mWallPreProc += wallTime() - mWallStart;
  //utilDumpImage(fullArr, nxDimForBP, nxForBP, nyForBP, 0, "padded tapered original");

  // If doing trim or taper, bin the subarea and taper inside, take the FFT
  if (doBinPad && mSummingMode >= 0) {
    xOffset = (nxForBP - mNx) / 2;
    yOffset = (nyForBP - mNy) / 2;
    mWallStart = wallTime();

    // Use zoomdown routine for binning, it is a lot faster
    if (mBinAlign > 1) {
      nxBin = (mXend + 1 - mXstart) / mBinAlign;
      nyBin = (mYend + 1 - mYstart) / mBinAlign;
      for (ind = 0; ind < nyForBP; ind++)
        mLinePtrs[ind] = (unsigned char *)(fullArr + ind * nxDimForBP);
      if (!zoomWithFilter(mLinePtrs, nxForBP, nyForBP, (float)(mXstart + xOffset),
                          (float)(mYstart + yOffset), nxBin, nyBin,
                          nxBin, 0, MRC_MODE_FLOAT, binArr, NULL, NULL))
        needExtract = false;
    }
    if (needExtract)
      extractWithBinning(fullArr, MRC_MODE_FLOAT, nxDimForBP, mXstart + xOffset,
                         mXend + xOffset, mYstart + yOffset, mYend + yOffset,
                         mBinAlign, binArr, 0, &nxBin, &nyBin);
    //dumpImage(binArr, nxBin, nxBin, nyBin, 0, "binned");

    nxTaper = (int)(mTaperFrac * nxBin);
    nyTaper = (int)(mTaperFrac * nyBin);
    sliceTaperInPad(binArr, MRC_MODE_FLOAT, nxBin, 0, nxBin - 1, 0, nyBin - 1, binArr,
                    mAlignXpad + 2, mAlignXpad, mAlignYpad, nxTaper, nyTaper);
    //dumpImage(binArr, mAlignXpad + 2, mAlignXpad, mAlignYpad, 0, "padded tapered");
    mWallBinPad += wallTime() - mWallStart;
    mWallStart = wallTime();
    if (mGpuAligning) {

      // Take FFT and save in stack on GPU if needed; if there is an error here, try to
      // recover the stack or the sum and just turn off GPU aligning
      if (sFgpuProcessAlignImage(binArr, saving ? aliFrameInd : -1, groupInd)) {
        if (prepareToFetchAlignFFTs(saving ? aliFrameInd : -1))
          return 3;
        if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL,
                                 mSavedGroups.size() ? &mSavedGroups[0] : NULL,
                                 mNumAllVsAll ? NULL : mAlignSum, 
                                 saving ? NULL : mWorkBinPad)) {
          cleanup();
          return 3;
        }
        mGpuAligning = false;

        // Grouping with no refine at end was saved as real space, so need to take FFTs
        if (mGroupSize > 1 && !mCumAlignAtEnd) {
          for (ind = 0; ind < (int)mSavedBinPad.size() - (saving ? 1 : 0); ind++) {
            todfftc(mSavedBinPad[ind], mAlignXpad, mAlignYpad, 0);
            for (ix = 0; ix < mAlignPix; ix++)
              mSavedBinPad[ind][ix] *= mFullFiltMask[ix];
          }
        }
        if (saving) {
          tempBin = mSavedBinPad[aliFrameInd];
          memcpy(tempBin, binArr, mAlignBytes);
          binArr = tempBin;
        }
        utilPrint("Switching to aligning with the CPU\n");
      }
    }

    if (!mGpuAligning)
      todfftc(binArr, mAlignXpad, mAlignYpad, 0);
    mWallBinFFT += wallTime() - mWallStart;
  }

  // Take the full FFT
  if (!doBinPad || (mSummingMode <= 0 && !mGpuSumming)) {
    mWallStart = wallTime();
    todfftc(fullArr, mFullXpad, mFullYpad, 0);
    mWallFullFFT += wallTime() - mWallStart;
    //dumpFFT(fullArr, mFullXpad, mFullYpad, "full FFT", 0);
  }
  
  // If just summing, add into sum and return
  if (mSummingMode < 0) {
    mXshifts[0].push_back(shiftX);
    mYshifts[0].push_back(shiftY);
    mXshifts[1].push_back(shiftX);
    mYshifts[1].push_back(shiftY);
    if (addToSums(fullArr, -9, mNumFrames)) {
      cleanup();
      return 2;
    }
    mNumFrames++;
    return 0;
  }

  // Now if not doing taper/pad, reduce the FFT into the align array
  if (!doBinPad) {
    mWallStart = wallTime();
    fourierReduceImage(fullArr, mFullXpad, mFullYpad, binArr, mAlignXpad, mAlignYpad, 
                       0., 0., mShiftTemp);
    mWallReduce += wallTime() - mWallStart;
  }

  // Apply full filter to the align array unless on GPU
  if (!mGpuAligning) {
    mWallStart = wallTime();
    for (ind = 0; ind < mAlignPix; ind++)
      binArr[ind] *= mFullFiltMask[ind];
    mWallFilter += wallTime() - mWallStart;
  }

  // Make a new group if ready
  useInd = aliFrameInd;
  if (mGroupSize > 1 && mNumFrames >= mGroupSize - 1) {
    if (!mGpuAligning) {
      if (groupInd < (int)mSavedGroups.size()) {
        groupArr = mSavedGroups[groupInd];
      } else {
        groupArr = B3DMALLOC(float, mAlignPix);
        if (testAndCleanup(!groupArr))
          return 1;
        mSavedGroups.push_back(groupArr);
      }        
      memset(groupArr, 0, mAlignBytes);
      for (ind = aliFrameInd + 1 - mGroupSize; ind <= aliFrameInd; ind++) {
        tempBin = mSavedBinPad[ind];
        for (ix = 0; ix < mAlignPix; ix++)
          groupArr[ix] += tempBin[ix];
      }
    }
    useInd = groupInd;
  }
  
  if (!mNumAllVsAll && mXshifts[0].size() > 0) {
    nearXshift = mXshifts[0].back();
    nearYshift = mYshifts[0].back();
  }
  if (mNumAllVsAll && mNumFrames + mGroupSize - 1 > 0) {
      
    // Align this frame with each previous frame, or nonoverlapping group
    ind = B3DMIN(mNumFrames + 1 - mGroupSize, mNumAllVsAll - 1);
    for (ref = 0; ref <= ind - mGroupSize; ref++) {
      for (filt = 0; filt < mNumFilters; filt++) {
        nearXshift = mXnearShifts[ind - 1] - mXnearShifts[ref];
        nearYshift = mYnearShifts[ind - 1] - mYnearShifts[ref];
        if (alignTwoFrames(useInd + ref - ind, useInd, nearXshift, nearYshift,
                           filt, xShift, yShift, filterSubarea || mNumFilters > 1,
                           mDumpCorrs)) {
          cleanup();
          return 2;
        }
        mXallShifts[filt][ref * mNumAllVsAll + ind] = xShift;
        mYallShifts[filt][ref * mNumAllVsAll + ind] = yShift;
        if (mDebug > 1)
          utilPrint("%d to %d  %.2f  %.2f   near %.2f  %.2f\n", useInd,
                    useInd + ref - ind, xShift, yShift, nearXshift, nearYshift);
      }
    }
  } else if (!mNumAllVsAll) {
   
    // Or align with the sum and add to the sums
    xShift = yShift = 0.;
    if (!mCumAlignAtEnd)
      useInd = -1;
    if (mNumFrames && alignTwoFrames(-1, useInd, nearXshift, nearYshift, 0, xShift,
                                     yShift, filterSubarea, mDumpCorrs)) {
      cleanup();
      return 2;
    }
    mXshifts[0].push_back(xShift);
    mYshifts[0].push_back(yShift);
    mXshifts[1].push_back(xShift);
    mYshifts[1].push_back(yShift);
    if (addToSums((mSummingMode <= 0 && !mDeferSumming) ? fullArr : NULL, useInd, 
                  mNumFrames)) {
      cleanup();
      return 2;
    }
  }

  // Increase the frame count after processing a frame
  mNumFrames++;
  return 0;
}

/*
 * Solve for the alignment of the current group of frames
 */
void FrameAlign::findAllVsAllAlignment(bool justForLimits)
{
  int row, col, numData, numCol, numInCol, ref, ind, numIter, maxZeroWgt, filt, allInd;
  int maxAsBest, indOfMax = -1;
  bool pickable, failed[MAX_FILTERS];
  float solMat[2 * MAX_ALL_VS_ALL], xMean[MAX_ALL_VS_ALL], xSD[MAX_ALL_VS_ALL];
  float maxChange = 0.02f;
  float maxOscill = 0.05f;
  int maxIter = 50;
  int numFrames = B3DMIN(mNumFrames, mNumAllVsAll + mGroupSize - 1);
  int numGroups = numFrames + 1 - mGroupSize;
  float resMean[MAX_FILTERS], resSD[MAX_FILTERS];
  float maxWgtRes[MAX_FILTERS], maxRaw[MAX_FILTERS];
  int numFailed[MAX_FILTERS];
  
  bool doRobust = numFrames >= 5 && mKfactor > 0.;
  float finalX = 0., finalY = 0.;
  float errx, erry, resid, wgtResid, resSum, resSumSq, distFilt, dist0;
  float minWgt = 2.f, minError = 1.e30f;
  float maxFitDist = 0., fitDist[MAX_FILTERS], errMeasure[MAX_FILTERS];
  float smoothDist[MAX_FILTERS], maxSmoothDist = 0.;
  float distCrit = 0.75f;
  float notZeroCrit = 4.f;
  float absZeroCrit = (float)(0.15 * mBinAlign);
  float relZeroCrit = (float)(0.5 * mBinAlign);
  float closerRatio = 0.2f;

  // Evaluate failures of higher filters relative to lower ones
  for (filt = 0; filt < MAX_FILTERS; filt++)
    numFailed[filt] = 0;
  if (mNumFilters > 1) {
    for (ind = 1; ind < numGroups; ind++) {
      for (ref = 0; ref <= ind - mGroupSize; ref++) {
        allInd = ref * mNumAllVsAll + ind;
        dist0 = sqrt(mXallShifts[0][allInd] * mXallShifts[0][allInd] +
                     mYallShifts[0][allInd] * mYallShifts[0][allInd]);
        if (dist0 > notZeroCrit) {
          for (filt = 1; filt < mNumFilters; filt++) {
            distFilt = sqrt(mXallShifts[filt][allInd] * mXallShifts[filt][allInd] +
                            mYallShifts[filt][allInd] * mYallShifts[filt][allInd]);
            if (distFilt < absZeroCrit || (distFilt < relZeroCrit && 
                                           distFilt < closerRatio * dist0))
              numFailed[filt]++;
          }
        }
      }
    }
    if (mDebug > 1)
      utilPrint("numFailed %d %d %d %d %d\n", numFailed[1], numFailed[2], numFailed[3],
                numFailed[4], numFailed[5]);
  }
  maxAsBest =0;
  if (!mPickedBestFilt)
    mBestFilt = 0;
  numData = (numGroups + 1 - mGroupSize) * (numGroups - mGroupSize) / 2;
  doRobust = numData >= 2 * numGroups && mKfactor > 0.;
  for (filt = 0; filt < mNumFilters; filt++) {
    maxRaw[filt] = maxWgtRes[filt] = 0.;
    resSum = resSumSq = 0; 
    if (mGroupSize > 1 && (numGroups < mGroupSize || numData < numGroups)) {
      for (ind = 0; ind < numGroups; ind++)
        mXnearShifts[ind] = mYnearShifts[ind] = 0.;
      for (ind = 1; ind < numGroups; ind++) {
        for (ref = 0; ref <= ind - mGroupSize; ref++) {
          if (mXnearShifts[ind] == 0.) {
            mXnearShifts[ind] = mXnearShifts[ref] + 
              mXallShifts[0][ref * mNumAllVsAll + ind];
            mYnearShifts[ind] = mYnearShifts[ref] + 
              mYallShifts[0][ref * mNumAllVsAll + ind];
          }
        }
      }
      continue;

    } else if (numGroups == 1) {

      // Deal with having only 1 or 2 frames
      mXfitShifts[filt][0] = 0.;
      mYfitShifts[filt][0] = 0.;
      mXnearShifts[0] = 0.;
      mYnearShifts[0] = 0.;
      continue;
    } else if (numGroups == 2) {
      mXfitShifts[filt][0] = -mXallShifts[filt][1] / 2.f;
      mYfitShifts[filt][0] = -mYallShifts[filt][1] / 2.f;
      mXfitShifts[filt][1] = mXallShifts[filt][1] / 2.f;
      mYfitShifts[filt][1] = mYallShifts[filt][1] / 2.f;
      if (!filt) {
        mXnearShifts[0] = -mXallShifts[0][1] / 2.f;
        mYnearShifts[0] = -mYallShifts[0][1] / 2.f;
        mXnearShifts[1] = mXallShifts[0][1] / 2.f;
        mYnearShifts[1] = mYallShifts[0][1] / 2.f;
      }
      continue;
    } 

    // Otherwise, do the fitting
    // Load the data matrix with the correlations
    row = 0;
    numCol = numGroups + 3;
    numInCol = numGroups - 1;
    for (ind = 1; ind < numGroups; ind++) {
      for (ref = 0; ref <= ind - mGroupSize; ref++) {
        mFitMat[numCol * row + numInCol] = mXallShifts[filt][ref * mNumAllVsAll + ind];
        mFitMat[numCol * row + numInCol + 1] = 
          mYallShifts[filt][ref * mNumAllVsAll + ind];
        for (col = 0; col < numInCol; col++)
          mFitMat[numCol * row + col] = ((ind == numGroups - 1) ? -1.f : 0.f);
        mFitMat[numCol * row + ref] += -1.f;
        if (ind < numGroups - 1)
          mFitMat[numCol * row + ind] += 1.f;
        /*for (col = 0; col < numInCol; col++)
          printf("%.1f ", mFitMat[numCol * row + col]);
          printf("%.2f %.2f\n", mFitMat[numCol * row + numInCol], 
          mFitMat[numCol * row + numInCol + 1]); */
        row++;
      }
    }
    numData = row;
    
    // Do robust fitting if enough data, fall back to regular fit on error
    maxZeroWgt = (int)B3DMIN(0.1 * numData, numFrames - 3);
    if (doRobust) {
      row = robustRegress(mFitMat, numCol, 1, numInCol, numData, 2, solMat, 
                                  numInCol, NULL, xMean, xSD, mFitWork, mKfactor, 
                          &numIter, maxIter, maxZeroWgt, maxChange, maxOscill);
      if (row) {
        if (mDebug)
          utilPrint("robustRegress failed with error %d\n", row);
        doRobust = false;
      }
    }
    if (!doRobust)
      multRegress(mFitMat, numCol, 1, numInCol, numData, 2, 0, solMat, numInCol, NULL,
                  xMean, xSD, mFitWork);
    finalX = finalY = 0.;
    for (ind = 0; ind < numInCol; ind++) {
      finalX -= solMat[ind];
      finalY -= solMat[ind + numInCol];
      mXfitShifts[filt][ind] = solMat[ind];
      mYfitShifts[filt][ind] = solMat[ind + numInCol];
      //PRINT2(solMat[ind], solMat[ind + numInCol]);
    }
    mXfitShifts[filt][numInCol] = finalX;
    mYfitShifts[filt][numInCol] = finalY;
    //PRINT2(finalX, finalY);

    // For first filter, copy to the shifts used for predictions
    if (!filt) {
      for (ind = 0; ind < numGroups; ind++) {
        mXnearShifts[ind] = mXfitShifts[0][ind];
        mYnearShifts[ind] = mYfitShifts[0][ind];
      }
    }
    if (justForLimits)
      continue;

    // Compute residuals
    row = 0;
    fitDist[filt] = 0.;
    for (ind = 1; ind < numGroups; ind++) {
      for (ref = 0; ref <= ind - mGroupSize; ref++) {
        allInd = ref * mNumAllVsAll + ind;
        errx = (mXfitShifts[filt][ind] - mXfitShifts[filt][ref]) -
          mXallShifts[filt][allInd];
        erry = (mYfitShifts[filt][ind] - mYfitShifts[filt][ref]) - 
          mYallShifts[filt][allInd];
        resid = sqrt(errx * errx + erry * erry);
        wgtResid = resid;
        if (doRobust) {
          wgtResid = resid * mFitMat[numCol * row + numInCol + 2];
          minWgt = B3DMIN(minWgt, mFitMat[numCol * row + numInCol + 2]);
        }
        resSum += wgtResid;
        resSumSq += wgtResid * wgtResid;
        ACCUM_MAX(maxRaw[filt], resid);
        ACCUM_MAX(maxWgtRes[filt], wgtResid);
        row++;
        if (ind == ref + 1)
          fitDist[filt] += (float)
            sqrt(pow((double)mXfitShifts[filt][ind] - mXfitShifts[filt][ref], 2.) +
                 pow((double)mYfitShifts[filt][ind] - mYfitShifts[filt][ref], 2.));
      }
    }
    
    // Maintain stats for this filter
    sumsToAvgSD(resSum, resSumSq, numData, &resMean[filt], &resSD[filt]);
    mResMeanSum[filt] += resMean[filt];
    mResSDsum[filt] += resSD[filt];
    mResMaxSum[filt] += maxWgtRes[filt];
    mRawMaxSum[filt] += maxRaw[filt];
    ACCUM_MAX(mMaxResMax[filt], maxWgtRes[filt]);
    ACCUM_MAX(mMaxRawMax[filt], maxRaw[filt]);
    errMeasure[filt] = (1.f - mMaxMaxWeight) * resMean[filt] + 
      mMaxMaxWeight * maxWgtRes[filt];
    failed[filt] = numFailed[filt] >= B3DMAX(1, numFrames - 2);
    if (failed[filt])
      mNumAsBestFilt[filt]--;
    smoothDist[filt] = 0.;
    if (mXshifts[filt].size() >= 3)
      smoothDist[filt] = smoothedTotalDistance(&mXshifts[filt][0], &mYshifts[filt][0],
                                               (int)mXshifts[filt].size(), dist0);
    ACCUM_MAX(maxFitDist, fitDist[filt]);
    ACCUM_MAX(maxSmoothDist, smoothDist[filt]);
    //PRINT3(filt, fitDist[filt],  smoothDist[filt]);
    if (maxAsBest < mNumAsBestFilt[filt]) {
      indOfMax = filt;
      maxAsBest = mNumAsBestFilt[filt];
    }

    // On the last fit, we now know the best filter and can manage the hybrid values 
    if (filt == mNumFilters - 1) {
      //PRINT2(maxFitDist, maxSmoothDist);

      for (ind = 0; ind < mNumFilters; ind++) {
        if (!mPickedBestFilt && mNumAsBestFilt[ind] > -mFailedOftenCrit && !failed[ind] &&
            (errMeasure[ind] < minError && fitDist[ind] >= distCrit * maxFitDist) &&
            !(maxAsBest >= mNumAsBestFilt[ind] + mPickDiffCrit && 
              maxAsBest >= mPickRatioCrit * mNumAsBestFilt[ind])) {
          minError = errMeasure[ind];
          mBestFilt = ind;
        }
        //utilPrint("%d  ", mNumAsBestFilt[ind]);
      }
      //PRINT2(mBestFilt, indOfMax);

      mResMeanSum[mNumFilters] += resMean[mBestFilt];
      mResSDsum[mNumFilters] += resSD[mBestFilt];
      mResMaxSum[mNumFilters] += maxWgtRes[mBestFilt];
      mRawMaxSum[mNumFilters] += maxRaw[mBestFilt];
      ACCUM_MAX(mMaxResMax[mNumFilters], maxWgtRes[mBestFilt]);
      ACCUM_MAX(mMaxRawMax[mNumFilters], maxRaw[mBestFilt]);
      mNumFits++;
      if (!mPickedBestFilt && mNumFilters > 1 && mNumFrames >= mNumAllVsAll + 
          mGroupSize -1) {
        mNumAsBestFilt[mBestFilt]++;
        pickable = true;
        for (ind = 0; ind < mNumFilters; ind++) {
          if (ind != indOfMax && (maxAsBest < mNumAsBestFilt[ind] + mPickDiffCrit ||
                                  maxAsBest < mPickRatioCrit * mNumAsBestFilt[ind]))
            pickable = false;
        }
        if (pickable) {
          if (mDebug)
            utilPrint("After %d frames, picking filter %d as best\n", mNumFrames,
                      indOfMax +1);
          mBestFilt = indOfMax;
          mPickedBestFilt = true;
        }
      }
    }
    if (mDebug > 1) {
      utilPrint("%sresidual: mean = %.2f, SD = %.2f, max = %.2f\n", 
                doRobust ? "weighted " : "", resMean[filt], resSD[filt], maxWgtRes[filt]);
      if (doRobust)
        utilPrint("    unweighted max residual = %.2f, min weight = %.3f\n",
                  maxRaw[filt], minWgt);
    }
  }
}

/*
 * Finish aligning and averaging the remaining frames and return results
 */
int FrameAlign::finishAlignAndSum(float refineRadius2, float refineSigma2,
                                  float iterCrit, int groupRefine, int doSpline,
                                  float *alisum, float *xShifts, float *yShifts,
                                  float *rawXshifts, float *rawYshifts, float *ringCorrs,
                                  float deltaR, int &bestFilt, float *smoothDist,
                                  float *rawDist, float *resMean, float *resSD,
                                  float *meanResMax, float *maxResMax, float *meanRawMax,
                                  float *maxRawMax)
{
  int ind, numPix, frame, maxAsBest, filt, useFilt, iter, ierr, useFrame, numAlign,binInd;
  float shiftX, shiftY, error, minError, maxRefine;
  FloatVec refXshift, refYshift, cumXshift, cumYshift, groupXshift, groupYshift;
  float *realSum = mFullEvenSum;
  float *binArr;
  int nxBin = mNx / mBinSum;
  int nyBin = mNy / mBinSum;
  int xStart = (mSumXpad - nxBin) / 2;
  int xEnd = xStart + nxBin - 1;
  int yStart = (mSumYpad - nyBin) / 2;
  int yEnd = yStart + nyBin - 1;
  bool processFull = mSummingMode <= 0 && !mDeferSumming;
  double wallRefine = 0;
  int numAVAforFrames = mNumAllVsAll + mGroupSize - 1;
  
  // If nothing is aligned, it is an error
  if (!mNumFrames)
    return 1;
  
  // Finish up with all-vs-all
  if (mNumAllVsAll && mSummingMode >= 0) {
    findAllVsAllAlignment(false);
    
    // pick a best filter if haven't got one yet
    if (mNumFilters > 1 && !mPickedBestFilt) {
      maxAsBest = 0;
      for (ind = 0; ind < mNumFilters; ind++)
        ACCUM_MAX(maxAsBest, mNumAsBestFilt[ind]);
      mBestFilt = 0;
      minError = 1.e30f;
      for (ind = 0; ind < mNumFilters; ind++) {
        error = (1.f - mMaxMaxWeight) * mResMeanSum[ind] / B3DMAX(1, mNumFits) +
          mMaxMaxWeight * mMaxResMax[ind];
        if (mNumAsBestFilt[ind] > -mFailedOftenCrit && error < minError &&
            !(maxAsBest >= mNumAsBestFilt[ind] + mPickDiffCrit && 
              maxAsBest >= mPickRatioCrit * mNumAsBestFilt[ind])) {
          minError = error;
          mBestFilt = ind;
        }
      }
    }
  
    // Then take care of shifts
    if (mNumFrames <= numAVAforFrames) {
      
      // Take shifts as is if never got any before and add these images
      for (ind = 0; ind < mNumFrames; ind++) {
        for (filt = 0; filt <= mNumFilters && ind < mNumFrames + 1 - mGroupSize; filt++) {
          useFilt = filt == mNumFilters ? mBestFilt : filt;
          mXshifts[filt].push_back(mXfitShifts[useFilt][ind]);
          mYshifts[filt].push_back(mYfitShifts[useFilt][ind]);
        }
        if (processFull && addToSums(mSavedFullSize[ind], -9, ind))
          return 3;

      }
    } else {

      // Or adjust and add in ALL the shifts this time and add the images
      for (filt = 0; filt <= mNumFilters; filt++) {
        useFilt = filt == mNumFilters ? mBestFilt : filt;
        adjustAndPushShifts(mNumAllVsAll - 1, filt, useFilt);
      }
      for (ind = 1; ind < numAVAforFrames; ind++)
        if (processFull && addToSums(mSavedFullSize[ind], -9,
                                     mNumFrames + ind - numAVAforFrames))
          return 3;
    }
  }
  bestFilt = mBestFilt;
  useFilt = mUseHybrid ? mNumFilters : mBestFilt;

  // Convert the shifts from group to frame
  groupXshift = mXshifts[useFilt];
  groupYshift = mYshifts[useFilt];
  getAllFrameShifts(refXshift, refYshift, useFilt);
  mXshifts[useFilt] = refXshift;
  mYshifts[useFilt] = refYshift;

  // Now do a refinement with alignment to leave-one-out reference
  if (mCumAlignAtEnd && mSummingMode >= 0) {
    wallRefine = wallTime();
    if (mDebug) {
      smoothDist[useFilt] = smoothedTotalDistance
        (&mXshifts[useFilt][0], &mYshifts[useFilt][0], (int)mXshifts[useFilt].size(), 
         rawDist[useFilt]);
      utilPrint("Original distance raw %.2f  smoothed %.2f\n", rawDist[useFilt], 
                smoothDist[useFilt]);
    }
    
    // Get a new high-frequency filter mask
    numPix = mAlignPix;
    if (!refineRadius2) {
      refineRadius2 = mRadius2[mBestFilt];
      refineSigma2 = mSigma2[mBestFilt];
    }
    XCorrSetCTF(0., refineSigma2 * mBinAlign, 0., refineRadius2 * mBinAlign,
                mFiltFunc, mAlignXpad, mAlignYpad, &mFiltDelta);
    for (ind = 0; ind < numPix; ind++)
      mFullFiltMask[ind] = 1.;
    XCorrFilterPart(mFullFiltMask, mFullFiltMask, mAlignXpad, mAlignYpad, mFiltFunc,
                    mFiltDelta);
    if (mGpuAligning && sFgpuNewFilterMask(mFullFiltMask)) {
      if (prepareToFetchAlignFFTs(-1))
        return 3;
      if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL,
                               mSavedGroups.size() ? &mSavedGroups[0] : NULL, NULL, NULL))
        return 3;
      mGpuAligning = false;
    }

    // The real accumulated shifts are kept in cumXYshift; the mXYshifts are the ones 
    // that get applied to frames on each iteration
    if (mGroupSize > 1 && groupRefine) {
      //cumXshift = groupXshift;
      //cumYshift = groupYshift;
      numAlign = mNumFrames + 1 - mGroupSize;
    } else {
      groupRefine = 0;
      numAlign = mNumFrames;
    }
    cumXshift = mXshifts[useFilt];
    cumYshift = mYshifts[useFilt];
      
    for (iter = 0; iter < mCumAlignAtEnd; iter++) {
      mGroupSize = 1;
      if (mGpuAligning)
        sFgpuSetGroupSize(1);
      refXshift.clear();
      refYshift.clear();
      if (iter) {
        if (mGpuAligning && sFgpuClearAlignSum())
          return 3;
        else if (!mGpuAligning)
          memset(mAlignSum, 0, mAlignBytes);
      }

      // Loop on frames to shift the bin pad image into alignment and add to sum
      // Skip this on the first iteration for cumulative alignment
      if (iter || mNumAllVsAll) {
        for (frame = 0; frame < mNumFrames; frame++) {
          ierr = addToSums(NULL, frame, frame, useFilt);
          if (ierr)
            return ierr;
        }
      }
      
      // Loop on frames to align
      maxRefine = 0.;
      for (frame = 0; frame < numAlign; frame++) {
        useFrame = frame;
        if (!mGpuAligning)
          binArr = mSavedBinPad[frame];

        // Subtract this frame from the align sum and filter it
        if (groupRefine) {

          // For group refine, add up the frames in group
          mGroupSize = mGroupSizeInitial;
          sFgpuSetGroupSize(mGroupSize);
          useFrame = 0;

          // Do that on the GPU or in memory
          if (mGpuAligning) {
            if (sFgpuSumIntoGroup(frame + mGroupSize - 1, 0)) {
              if (prepareToFetchAlignFFTs(-1))
                return 3;
              if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL,
                                       mSavedGroups.size() ? &mSavedGroups[0] : NULL,
                                       mAlignSum, NULL))
                return 3;
              mGpuAligning = false;
            }
          }
          if (!mGpuAligning) {
            binArr = mSavedGroups[0];
            memset(binArr, 0, mAlignBytes);
            for (binInd = frame; binInd < frame + mGroupSize; binInd++)
              for (ind = 0; ind < mAlignPix; ind++)
                binArr[ind] += mSavedBinPad[binInd][ind];
          }
        }
        
        // If on GPU, that now needs subtracting and filtering
        if (mGpuAligning) {
          if (sFgpuSubtractAndFilterAlignSum(useFrame, groupRefine)) {
            if (prepareToFetchAlignFFTs(-1))
              return 3;
            if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL,
                                     mSavedGroups.size() ? &mSavedGroups[0] : NULL, 
                                     mAlignSum, NULL))
              return 3;
            mGpuAligning = false;
            binArr = groupRefine ? mSavedGroups[0] : mSavedBinPad[frame];
          }
        }

        // Or, subtract and filter on CPU
        if (!mGpuAligning)
          for (ind = 0; ind < numPix; ind++)
            mWorkBinPad[ind] = (mAlignSum[ind] - binArr[ind]) * mFullFiltMask[ind];
        
        // Align it to LOO sum.  May want to pass a smaller max shift
        ierr = alignTwoFrames(-2, useFrame, 0., 0., 0, shiftX, shiftY, false, 
                              mDumpRefCorrs);
        if (ierr)
          return ierr;
        refXshift.push_back(shiftX);
        refYshift.push_back(shiftY);
        error = sqrt(shiftX * shiftX + shiftY * shiftY);
        ACCUM_MAX(maxRefine, error);
      }

      if (groupRefine) {
        for (frame = 0; frame < mNumFrames; frame++) {
          mXshifts[useFilt][frame] = 0.;
          mYshifts[useFilt][frame] = 0.;
        }
        for (frame = 0; frame < numAlign; frame++) {
          for (ind = frame; ind < frame + mGroupSize; ind++) {
            mXshifts[useFilt][ind] += refXshift[frame] / mGroupSize;
            mYshifts[useFilt][ind] += refYshift[frame] / mGroupSize;
          }
        }
        refXshift = mXshifts[useFilt];
        refYshift = mYshifts[useFilt];
      } else {
      
        // And copy the refineshift over to be applied next time
        mXshifts[useFilt] = refXshift;
        mYshifts[useFilt] = refYshift;
      }

      // Adjust the shifts: add them to cumulative shift
      for (frame = 0; frame < mNumFrames; frame++) {
        cumXshift[frame] += refXshift[frame];
        cumYshift[frame] += refYshift[frame];
        if (mDebug)
          utilPrint("%d %2d %.2f  %.2f   %.2f  %.2f\n", iter, frame, refXshift[frame],
                    refYshift[frame], cumXshift[frame], cumYshift[frame]);
      }


      if (maxRefine < iterCrit)
        break;
    }

    // At end, put the full shifts back
    mXshifts[useFilt] = cumXshift;
    mYshifts[useFilt] = cumYshift;
    wallRefine = wallTime() - wallRefine;
  }

  // adjust shifts for initial cumulative alignment to have a mean of 0
  if (!mNumAllVsAll && (mDeferSumming || mSummingMode > 0)) {
    shiftX = shiftY = 0.;
    for (ind = 0; ind < mNumFrames; ind++) {
      shiftX += mXshifts[useFilt][ind] / mNumFrames;
      shiftY += mYshifts[useFilt][ind] / mNumFrames;
    }
    for (ind = 0; ind < mNumFrames; ind++) {
      mXshifts[useFilt][ind] -= shiftX;
      mYshifts[useFilt][ind] -= shiftY;
    }
  }

  // Save to raw shifts and apply spline smoothing now before shifts get used
  for (ind = 0; ind < mNumFrames; ind++) {
    rawXshifts[ind] = mXshifts[useFilt][ind];
    rawYshifts[ind] = mYshifts[useFilt][ind];
  }
  
  // Spline smoothing: Get the raw distance first, use the spline as smoothed distance
  if (doSpline && mSummingMode >= 0) {
    smoothedTotalDistance(&mXshifts[useFilt][0], &mYshifts[useFilt][0],
                           (int)mXshifts[useFilt].size(), rawDist[useFilt]);
    ind = splineSmooth(rawXshifts, rawYshifts, mNumFrames,
                       &mXshifts[useFilt][0], &mYshifts[useFilt][0], smoothDist[useFilt]);
    if (ind) {
      utilPrint("Spline smoothing of shifts failed with return value %d", ind);
      return 1;
    }
  }

  // Sum now if needed, do FRC, then inverse transform and extract appropriate area
  mGroupSize = 1;
  if (mSummingMode <= 0) {
    if (mDeferSumming) {

      // Make the sum
      if (mGpuSumming && (mGpuFlags & GPU_FOR_ALIGNING)) {
        sFgpuCleanAlignItems();
        if (sFgpuSetupSumming(mFullXpad, mFullYpad, mSumXpad, mSumYpad, 
                              (mGpuFlags & GPU_DO_EVEN_ODD) ? 1 : 0))
          mGpuSumming = false;
      }
      for (frame = 0; frame < mNumFrames; frame++)
        if (addToSums(mSavedFullSize[frame], -9, frame, useFilt))
          return 3;
    }

    if (mGpuSumming) {
      ind = sFgpuReturnSums(mWorkFullSize, mFullEvenSum, mFullOddSum, 0);

      // If there is no real sum and no even sum, we can't do anything
      if (ind == 3)
        return 3;

      // If there is an error getting real sum but even/odd is there, just cancel flags
      // to process the FFT(s) in the even/odd sums
      if (ind & 2)
        mGpuFlags = 0;

      // If there is no error, use the real array for extraction
      if (!ind)
        realSum = mWorkFullSize;
      if ((ind & 1) || !(mGpuFlags & GPU_DO_EVEN_ODD))
        ringCorrs = NULL;
    }

    if (ringCorrs)
      fourierRingCorr(mFullEvenSum, mFullOddSum, mSumXpad, mSumYpad, ringCorrs, 
                      (int)floor(0.5 / deltaR), deltaR, mWorkFullSize);
    if (mDumpEvenOdd) {
      utilDumpFFT(mFullEvenSum, mSumXpad, mSumYpad, "even sum", 1);
      utilDumpFFT(mFullOddSum, mSumXpad, mSumYpad, "odd sum", 1);
    }
    if (!mGpuFlags) {
      for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
        mFullEvenSum[ind] += mFullOddSum[ind];
      mWallStart = wallTime();
      todfftc(mFullEvenSum, mSumXpad, mSumYpad, 1);
      mWallFullFFT += wallTime() - mWallStart;
    }
    extractWithBinning(realSum, MRC_MODE_FLOAT, mSumXpad + 2, xStart, xEnd, yStart,
                       yEnd, 1, alisum, 0, &nxBin, &nyBin);
  }

  // return best shifts
  for (ind = 0; ind < mNumFrames; ind++) {
    //frameShiftFromGroups(ind, useFilt, xShifts[ind], yShifts[ind]);
    xShifts[ind] = mXshifts[useFilt][ind];
    yShifts[ind] = mYshifts[useFilt][ind];
  }
  
  // Return all the results
  for (filt = 0; filt <= mNumFilters; filt++) {
    if (!doSpline || filt != useFilt)
      smoothDist[filt] = smoothedTotalDistance(&mXshifts[filt][0], &mYshifts[filt][0], 
                                               (int)mXshifts[filt].size(), rawDist[filt], 
                                               &refXshift[0], &refYshift[0]);
    ind = B3DMAX(1, mNumFits);
    resMean[filt] = mResMeanSum[filt] / ind;
    resSD[filt] = mResSDsum[filt] / ind;
    meanResMax[filt] = mResMaxSum[filt] / ind;
    maxResMax[filt] = mMaxResMax[filt];
    meanRawMax[filt] = mRawMaxSum[filt] / ind;
    maxRawMax[filt] = mMaxRawMax[filt];
  }
  if (mReportTimes)
    utilPrint("FullFFT %.3f  BinPad %.3f  BinFFT %.3f  Reduce %.3f  Shift %.3f Filt "
              "%.3f\nConjProd %.3f   PreProc %.3f  Sum of those %.3f  Refine %.3f\n",
              mWallFullFFT, mWallBinPad, mWallBinFFT, mWallReduce, mWallShift, 
              mWallFilter, mWallConjProd, mWallPreProc, mWallFullFFT + mWallBinPad +
              mWallBinFFT + mWallReduce + mWallShift + mWallFilter + mWallConjProd + 
              mWallPreProc, wallRefine);
  if (mGpuFlags && mReportTimes)
    sFgpuPrintTimers();
  return 0;
}

/*
 * Align the frame in binArr to the one in refArr and return the shifts
 * Reference refInd >= 0 for saved frame, -1 for mAlignSum, -2 for mWorkBinPad
 * Image to align: aliInd >= 0 for saved frame, -1 for mWorkBinPad
 */
int FrameAlign::alignTwoFrames(int refInd, int aliInd, float nearXshift, float nearYshift,
                               int filtInd, float &xShift, float &yShift,
                               bool filterSubarea, bool dump)
{
  int limXlo, limXhi, limYlo, limYhi, ind, indPeak;
  float peaks[3], xpeaks[3], ypeaks[3], widths[3], minWidths[3];
  float xTemp[2] = {0., 0.}, yTemp[2] = {0., 0.}, expDist[2];
  float atZeroCrit = (float)(0.1 / mBinAlign);
  float peakRatioCrit = 2.;
  float thirdPeakCrit = 3.;
  float expDistRatioCrit = 2.;
  float minExpDist = 4.;
  float widthRatioCrit = 0.8f;
  float *refArr, *binArr;
  bool useSubarea = filterSubarea || mGpuAligning;
  float *corrTemp = useSubarea ? mCorrFiltTemp : mCorrBinPad;
  int subXoffset = useSubarea ? B3DNINT(-nearXshift / mBinAlign) : 0;
  int subYoffset = useSubarea ? B3DNINT(-nearYshift / mBinAlign) : 0;
  int aliXsize = useSubarea ? mAliFiltSize : mAlignXpad;
  int aliYsize = useSubarea ? mAliFiltSize : mAlignYpad;
  std::vector<float *> *savedVec = mGroupSize > 1 ? &mSavedGroups : &mSavedBinPad;

  // Going to store shifts but they will be negative because we are getting shift to align
  // reference to frame.  So take negative shift here
  limXlo = (int)((-nearXshift - mMaxShift) / mBinAlign) - subXoffset;
  limXhi = (int)ceil((-nearXshift + mMaxShift) / mBinAlign) - subXoffset;
  limYlo = (int)((-nearYshift - mMaxShift) / mBinAlign) - subYoffset;
  limYhi = (int)ceil((-nearYshift + mMaxShift) / mBinAlign) - subYoffset;

  if (!filtInd) {

    if (mGpuAligning) {

      // For GPU alignment, it extracts the wrapped image with origin in center
      // which is ready for filtering the subarea
      if (sFgpuCrossCorrelate(aliInd, refInd, mTempSubFilt, subXoffset,
                              subYoffset)) {
        if (prepareToFetchAlignFFTs(-1))
          return 3;
        if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL, 
                                 mSavedGroups.size() ? &mSavedGroups[0] : NULL, 
                                 refInd == -1 ? mAlignSum : NULL,
                                 (refInd < -1 || aliInd < 0) ? mWorkBinPad : NULL))
          return 3;
        mGpuAligning = false;
      } else if (!filterSubarea) {

        // But if we are not filtering, need to wrap back into corr array
        wrapImage(mTempSubFilt, aliXsize + 2, aliXsize, aliYsize, corrTemp, aliXsize + 2, 
              aliXsize, aliYsize, 0, 0);
      }
    }
    
    if (!mGpuAligning) {

      // Assign arrays from indexes if not aligning on GPU
      if (refInd >= (int)savedVec->size() || aliInd >= (int)savedVec->size())
        return 2;
      if (refInd >= 0)
        refArr = (*savedVec)[refInd];
      else if (refInd == -1)
        refArr = mAlignSum;
      else
        refArr = mWorkBinPad;
      if (aliInd >= 0)
        binArr = (*savedVec)[aliInd];
      else
        binArr = mWorkBinPad;

      // Copy into the correlation array
      memcpy(mCorrBinPad, binArr, mAlignBytes);
      
      // Get product
      mWallStart = wallTime();
      conjugateProduct(mCorrBinPad, refArr,  mAlignXpad, mAlignYpad);
      mWallConjProd += wallTime() - mWallStart;
      
      // Inverse FFT
      mWallStart = wallTime();
      todfftc(mCorrBinPad, mAlignXpad, mAlignYpad, 1);
      mWallBinFFT += wallTime() - mWallStart;
      if (dump && filterSubarea)
        utilDumpImage(mCorrBinPad, mAlignXpad + 2, mAlignXpad, mAlignYpad, 1,
                      "lf correlation", mNumFrames);

      // If high frequency filter being applied to subarea, extract subarea
      if (filterSubarea)
        wrapImage(mCorrBinPad, mAlignXpad + 2, mAlignXpad, mAlignYpad, mTempSubFilt,
                  mAliFiltSize + 2, mAliFiltSize, mAliFiltSize, subXoffset, subYoffset);
    }

    if (filterSubarea)
      sliceTaperInPad(mTempSubFilt, SLICE_MODE_FLOAT, mAliFiltSize + 2, 0, 
                      mAliFiltSize - 1, 0, mAliFiltSize - 1, mTempSubFilt,
                      mAliFiltSize + 2, mAliFiltSize, mAliFiltSize, 8, 8);
  }
  //dumpImage(mTempSubFilt, aliXsize + 2, aliXsize, aliYsize, 0, "extract");

  // Filter subarea to temp array if doing that
  if (filterSubarea) {
    mWallStart = wallTime();
    memcpy(mWrapTemp, mTempSubFilt, (aliXsize + 2) * aliYsize * sizeof(float));
    todfftc(mWrapTemp, aliXsize, aliYsize, 0);
    for (ind = 0; ind < (aliXsize + 2) * aliYsize; ind++)
      mWrapTemp[ind] = mWrapTemp[ind] * mSubFiltMask[filtInd][ind];
    todfftc(mWrapTemp, aliXsize, aliYsize, 1);
    wrapImage(mWrapTemp, aliXsize + 2, aliXsize, aliYsize, corrTemp, aliXsize + 2, 
              aliXsize, aliYsize, 0, 0);
    mWallFilter += wallTime() - mWallStart;
  }

  if (dump)
    utilDumpImage(corrTemp, aliXsize + 2, aliXsize, aliYsize, 1, "correlation",
                  mNumFrames);

  setPeakFindLimits(limXlo, limXhi, limYlo, limYhi, 1);
  XCorrPeakFindWidth(corrTemp, aliXsize + 2, aliYsize, xpeaks, ypeaks, peaks, widths,
                     minWidths, 2, 0);
  indPeak = 0;
  for (ind = 0; ind < 2; ind++) {
    if (peaks[ind] > -1.e29) {
      xTemp[ind] = -(subXoffset + xpeaks[ind]) * mBinAlign;
      yTemp[ind] = -(subYoffset + ypeaks[ind]) * mBinAlign;
      expDist[ind] = (float)sqrt(pow((double)xTemp[ind] - nearXshift, 2.) + 
                                 pow((double)yTemp[ind] - nearYshift, 2.));
      if (ind && fabs(xTemp[0]) < atZeroCrit && fabs(yTemp[0]) < atZeroCrit && 
          peaks[1] > peakRatioCrit * peaks[0] && widths[1] < widthRatioCrit * widths[0] &&
          peaks[2] < thirdPeakCrit * peaks[1] &&
          (expDist[1] < expDistRatioCrit * expDist[0] || 
           (expDist[0] < minExpDist && expDist[1] < minExpDist))) {
        indPeak = 1;
        if (mDebug)
          utilPrint("reject peak at %.2f %.2f for %.2f %.2f\n"
                    "peaks: %g %g %g  widths %.2f %.2f  expDist  %.2f %.2f\n",
                    xTemp[0], yTemp[0], xTemp[1], yTemp[1], peaks[0], peaks[1], peaks[2],
                    widths[0], widths[1], expDist[0], expDist[1]);
      }
    }
  }

  xShift = xTemp[indPeak];
  yShift = yTemp[indPeak];
  return 0;
}

/*
 * Shift and add image to full sum, and to cumulative alignment sum if binarr is supplied
 * Here binInd >= 0 for a saved frame, -1 for mWorkBinPad, < -1 for nothing
 */
int FrameAlign::addToSums(float *fullArr, int binInd, int frameNum, int filtInd)
{
  int ind;
  float *binArr;
  if (filtInd < 0)
    filtInd = mNumFilters;
  float xShift, yShift;
  float *fullSum = (frameNum % 2) ? mFullOddSum : mFullEvenSum;
  frameShiftFromGroups(frameNum, filtInd, xShift, yShift);

  // Shift full image and add into final sum if one is passed
  if (fullArr) {
    //dumpFFT(fullArr, mFullXpad, mFullYpad, "full pad to sum", 1);
    // Try to do sum on GPU if flag set
    if (mGpuSumming) {
      mWallStart = wallTime();
      if (sFgpuAddToFullSum(fullArr, xShift, yShift)) {

        // Recover by getting existing sum back and taking FFT of current array
        if (mDebug)
          utilPrint("Switching to summing on CPU\n");
        if (sFgpuReturnSums(mFullEvenSum, mFullEvenSum, mFullOddSum, 1)) { 
          if (frameNum > 0)
            return 3;
          memset(mFullEvenSum, 0, (mSumXpad + 2) * mSumYpad);
          memset(mFullOddSum, 0, (mSumXpad + 2) * mSumYpad);
        }
        mGpuSumming = false;
        todfftc(fullArr, mFullXpad, mFullYpad, 0);
      }
      mWallShift += wallTime() - mWallStart;
    }

    // Do sum into arrays here
    if (mBinSum > 1 && !mGpuSumming) {
      mWallStart = wallTime();
      fourierReduceImage(fullArr, mFullXpad, mFullYpad, mReduceTemp, mSumXpad, mSumYpad,
                         xShift, yShift, mShiftTemp);
      for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
        fullSum[ind] += mReduceTemp[ind];
      //dumpFFT(fullSum, mSumXpad, mSumYpad, "reduced sum", 0);
      mWallReduce += wallTime() - mWallStart;
    } else if (!mGpuSumming) {
      mWallStart = wallTime();
      fourierShiftImage(fullArr, mFullXpad, mFullYpad, xShift, yShift , mShiftTemp);
      mWallShift += wallTime() - mWallStart;
      for (ind = 0; ind < (mFullXpad + 2) * mFullYpad; ind++)
        fullSum[ind] += fullArr[ind];
    }
  }

  // If there is a legal binInd, shift it and add to align sum

  if (binInd < -1)
    return 0;

  mWallStart = wallTime();
  if (mGpuAligning) {

    // Shift and add: but don't bother shifting the source when doing simple cum corr
    // An error is fatal when shifting in place since we have no idea if it happened,
    // and we also don't know the state of the align sum
    if (sFgpuShiftAddToAlignSum(binInd, xShift / mBinAlign, yShift / mBinAlign,
                                binInd < 0 ? 0 : 1))
      return 3;

  } else {
    if (binInd >= (int)mSavedBinPad.size())
      return 2;
    binArr = binInd < 0 ? mWorkBinPad : mSavedBinPad[binInd];
    fourierShiftImage(binArr, mAlignXpad, mAlignYpad, xShift / mBinAlign,
                      yShift / mBinAlign, mShiftTemp);
    for (ind = 0; ind < mAlignPix; ind++)
      mAlignSum[ind] += binArr[ind];
  }
  mWallShift += wallTime() - mWallStart;
  return 0;
}

/*
 * Adjust shifts by the cumulative difference from the first set of shifts used, and 
 * store them
 */
void FrameAlign::adjustAndPushShifts(int topInd, int filt, int useFilt)
{
  int ind;
  float xDiff, yDiff, xSD, ySD, sem;

  // Get mean difference between last and this set of shifts
  for (ind = 0; ind < mNumAllVsAll - 1; ind++) {
    mLastXfit[filt][ind + 1] = mXfitShifts[useFilt][ind] - mLastXfit[filt][ind + 1];
    mLastYfit[filt][ind + 1] = mYfitShifts[useFilt][ind] - mLastYfit[filt][ind + 1];
    //xDiff += (mXfitShifts[ind] - mLastXfit[ind + 1]) / (mNumAllVsAll - 1);
    //yDiff += (mYfitShifts[ind] - mLastYfit[ind + 1]) / (mNumAllVsAll - 1);
  }
  avgSD(&mLastXfit[filt][1], mNumAllVsAll - 1, &xDiff, &xSD, &sem);
  avgSD(&mLastYfit[filt][1], mNumAllVsAll - 1, &yDiff, &ySD, &sem);
  /* utilPrint("filt %d useFilt %d diff mean %.2f  %.2f  SD %.2f  %.2f\n", filt, useFilt,
     xDiff, yDiff, xSD, ySD); */

  //Add to cumulative difference and push new shifts adjusted by this diff
  mCumulXdiff[filt] += xDiff;
  mCumulYdiff[filt] += yDiff;
  for (ind = 1; ind <= topInd; ind++) {
    mXshifts[filt].push_back(mXfitShifts[useFilt][ind] - mCumulXdiff[filt]);
    mYshifts[filt].push_back(mYfitShifts[useFilt][ind] - mCumulYdiff[filt]);
  }
}

/*
 * Determine how many frames need to be copied from GPU after a failure and allocate
 * arrays here; the vector operations at least do not work when vector is shared with
 * a DLL for SEMCCD
 */
int FrameAlign::prepareToFetchAlignFFTs(int aliFrameInd)
{
  int numBinPads, numGroups, ind;
  float *tempBin;
  sFgpuNumberOfAlignFFTs(&numBinPads, &numGroups);
  if (aliFrameInd >= numBinPads)
    numBinPads = aliFrameInd + 1;
  for (ind = (int)mSavedBinPad.size(); ind < numBinPads; ind++) {
    tempBin = B3DMALLOC(float, mAlignPix);
    if (testAndCleanup(!tempBin))
      return 1;
    mSavedBinPad.push_back(tempBin);
  }
  for (ind = (int)mSavedGroups.size(); ind < numGroups; ind++) {
    tempBin = B3DMALLOC(float, mAlignPix);
    if (testAndCleanup(!tempBin))
      return 1;
    mSavedGroups.push_back(tempBin);
  }
  return 0;
}

/*
 * Return smallest multiple of the two numbers that includes all their divisors
 */
int FrameAlign::leastCommonMultiple(int num1, int num2)
{
  int fac;
  for (fac = 64; fac > 1; fac--)
    if (num2 % fac == 0 && num1 % fac == 0)
      num2 /= fac;
  return num1 * num2;
}

/*
 * Wrap an image to go between a correlation with the origin in the corner and one with
 * the origin in the middle
 */
void FrameAlign::wrapImage(float *bufFrom, int nxDimFrom, int nxFrom, int nyFrom,
                           float *bufTo, int nxDimTo, int nxTo, int nyTo, int xOffset,
                           int yOffset)
{
  int ixFrom0[4], ixTo0[4], iyFrom0[4], iyTo0[4], ixFrom1[4], ixTo1[4], iyFrom1[4];
  int iyTo1[4], xnum, ynum, iy, quad;
  utilCoordsForWrap(nxFrom, nyFrom, nxTo, nyTo, xOffset, yOffset, ixFrom0, ixTo0,
                    iyFrom0, iyTo0, ixFrom1, ixTo1, iyFrom1, iyTo1);

  for (quad = 0; quad < 4; quad++) {
    ynum = iyFrom1[quad] + 1 - iyFrom0[quad];
    xnum = ixFrom1[quad] + 1 - ixFrom0[quad];
    if (xnum > 0 && ynum > 0) {
      for (iy = 0; iy < ynum; iy++)
        memcpy(&bufTo[(iy + iyTo0[quad]) * nxDimTo + ixTo0[quad]],
               &bufFrom[(iy + iyFrom0[quad]) * nxDimFrom + ixFrom0[quad]], xnum * 4);
    }
  }
}

/*
 * Smooth the trajectory of shifts and compute the total length of it
 */
float FrameAlign::smoothedTotalDistance(float *xShifts, float *yShifts, int numShifts,
                                        float &rawTotal, float *xSmoothed, 
                                        float *ySmoothed, double *variance)
{
  int numFit = B3DMIN(7, numShifts);
  int order = numShifts > 4 ? 2 : 1;
  int numBefore = numFit / 2;
  float delx, dely, intcpt, slopes[2], ro, sa, sb, se, xpred, prederr, ypred;
  float lastXpred, lastYpred, dist, varSum = 0;
  float frame[7] = {0, 1, 2, 3, 4, 5, 6};
  float frameSq[7] = {0, 1, 4, 9, 16, 25, 36};
  int ind, fitStart, fitEnd;
  rawTotal = 0;
  if (numShifts < 2)
    return 0.;
  if (numShifts == 2) {
    delx = xShifts[1] - xShifts[0];
    dely = yShifts[1] - yShifts[0];
    dist = sqrt(delx * delx + dely * dely);
    rawTotal = dist;
    return dist;
  }

  dist = 0.;
  for (ind = 0; ind < numShifts; ind++) {
    if (ind) {
      delx = xShifts[ind] - xShifts[ind -1];
      dely = yShifts[ind] - yShifts[ind -1];
      rawTotal += sqrt(delx * delx + dely * dely);
    }
      
    fitStart = B3DMAX(0, ind - numBefore);
    fitEnd = B3DMIN(fitStart + numFit - 1, numShifts - 1);
    fitStart = fitEnd + 1 - numFit;

    // Get predicted value from fit
    if (order == 1) {
      lsFitPred(frame, &xShifts[fitStart], numFit, &slopes[0], &intcpt, &ro, &sa, &sb, 
                &se, frame[ind - fitStart], &xpred, &prederr);
      lsFitPred(frame, &yShifts[fitStart], numFit, &slopes[0], &intcpt, &ro, &sa, &sb, 
                &se, frame[ind - fitStart], &ypred, &prederr);
    } else {
      lsFit2Pred(frame, frameSq, &xShifts[fitStart], numFit, &slopes[0], &slopes[1],
                 &intcpt, frame[ind - fitStart], frameSq[ind - fitStart], &xpred, 
                 &prederr);
      lsFit2Pred(frame, frameSq, &yShifts[fitStart], numFit, &slopes[0], &slopes[1],
                 &intcpt, frame[ind - fitStart], frameSq[ind - fitStart], &ypred, 
                 &prederr);
    }
    if (ind) {
      delx = xpred - lastXpred;
      dely = ypred - lastYpred;
      dist += sqrt(delx * delx + dely * dely);
    }
    lastXpred = xpred;
    lastYpred = ypred;
    if (xSmoothed)
      xSmoothed[ind] = xpred;
    if (ySmoothed)
      ySmoothed[ind] = ypred;
    delx = xpred - xShifts[ind];
    dely = ypred - yShifts[ind];
    varSum += xpred * xpred + ypred * ypred;
  }
  if (variance)
    *variance = varSum / (2. * numShifts);
  return dist;
}

/* 
 * Get a shift for one frame from shifts that may be for groups
 */
void FrameAlign::frameShiftFromGroups(int frame, int filt, float &shiftX, float &shiftY)
{
  // Spacing between frames is 1 and first frame of group is at 
  // -(group size - 1) / 2 frames relative to group center
  float realInd = (float)(frame - (mGroupSize - 1.) / 2.);
  float frac;
  int ind;
  if (mGroupSize == 1 || (int)mXshifts[filt].size() < 2) {
    shiftX = mXshifts[filt][frame];
    shiftY = mYshifts[filt][frame];
  } else if (realInd < -1) {
    shiftX = mXshifts[filt][0];
    shiftY = mYshifts[filt][0];
  } else if (realInd > (int)mXshifts[filt].size()) {
    shiftX = mXshifts[filt].back();
    shiftY = mYshifts[filt].back();
  } else {

    // Allow a bit of extrapolation.  The (int)mXshifts is crucial here for comparisons
    ind = (int)floor(realInd);
    B3DCLAMP(ind, 0, (int)mXshifts[filt].size() - 2);
    frac = realInd - ind;
    shiftX = (1.f - frac) * mXshifts[filt][ind] + frac * mXshifts[filt][ind + 1];
    shiftY = (1.f - frac) * mYshifts[filt][ind] + frac * mYshifts[filt][ind + 1];
  }
}

void FrameAlign::getAllFrameShifts(FloatVec &frameXshift, FloatVec &frameYshift, 
                                   int useFilt)
{
  frameXshift.resize(mNumFrames);
  frameYshift.resize(mNumFrames);
  for (int ind = 0; ind < mNumFrames; ind++)
    frameShiftFromGroups(ind, useFilt, frameXshift[ind], frameYshift[ind]);
}

/*
 * Do a spline smoothing of the shifts and return the smoothed coordinates and a distance
 */
int FrameAlign::splineSmooth(float *xShifts, float *yShifts, int numShifts, 
                             float *smoothedX, float *smoothedY, float &splineDist)
{
  int ind, ierr, mOrder = 2, mode = 2;
  double xVar, yVar, variance = .5;
  float delx, dely;
  double *allWork = B3DMALLOC(double, 6 * (numShifts * mOrder + 1) + numShifts + 
                              5 * numShifts + 10);
  double *dXshifts = allWork;
  double *dYshifts = dXshifts + numShifts;
  double *orderedX = dYshifts + numShifts;
  double *weights = orderedX + numShifts;
  double *coeff = weights + numShifts;
  double *work = coeff + numShifts;
  if (!allWork)
    return -1;
  splineDist = 0.;
  for (ind = 0; ind < numShifts; ind++) {
    dXshifts[ind] = xShifts[ind];
    dYshifts[ind] = yShifts[ind];
    orderedX[ind] = ind;
    weights[ind] = 1.;
  }

  // Fit to X then get spline values
  gcvspl(orderedX, dXshifts, numShifts, weights, weights, mOrder, numShifts, 1,
         mode,  variance, coeff, numShifts, work, &ierr);
  if (ierr) {
    free(allWork);
    return ierr;
  }
  xVar = work[4];
  for (ind = 0; ind < numShifts; ind++) {
    ierr = ind;
    smoothedX[ind] = (float)splder(0, mOrder, numShifts, (double)ind, orderedX, coeff, 
                                   &ierr, work);
  }

  // Fit to Y
  gcvspl(orderedX, dYshifts, numShifts, weights, weights, mOrder, numShifts, 1,
         mode,  variance, coeff, numShifts, work, &ierr);
  if (ierr) {
    free(allWork);
    return ierr;
  }
  yVar = work[4];
  for (ind = 0; ind < numShifts; ind++) {
    ierr = ind;
    smoothedY[ind] = (float)splder(0, mOrder, numShifts, (double)ind, orderedX, coeff, 
                                   &ierr, work);
    if (ind) {
      delx = smoothedX[ind] - smoothedX[ind - 1];
      dely = smoothedY[ind] - smoothedY[ind - 1];
      splineDist += sqrt(delx * delx + dely * dely);
    }
  }
  if (mDebug)
    utilPrint("GCV variance estimates = %f  %f\n", xVar, yVar);
  return 0;
}

/*
 * Find some crossings of the FRC and the level at half-nyquist
 */
void FrameAlign::analyzeFRCcrossings(float *ringCorrs, float frcDeltaR, float &halfCross, 
                                     float &quartCross, float &eighthCross,float &halfNyq)
{
  int cenBin, numBins, ind;
  halfCross = 0.;
  quartCross = 0.;
  eighthCross = 0.;
  for (ind = 1; ind < (int)floor(0.5 / frcDeltaR); ind++) {
    if (!halfCross && ringCorrs[ind - 1] >= 0.5 && ringCorrs[ind] <= 0.5)
      halfCross =(float)(frcDeltaR * (ind - 0.5 + (ringCorrs[ind - 1] - 0.5) / 
                                      (ringCorrs[ind - 1] - ringCorrs[ind])));
    if (!quartCross && ringCorrs[ind - 1] >= 0.25 && ringCorrs[ind] <= 0.25)
      quartCross = (float)(frcDeltaR * (ind - 0.5 + (ringCorrs[ind - 1] - 0.25) / 
                                        (ringCorrs[ind - 1] - ringCorrs[ind])));
    if (!eighthCross && ringCorrs[ind - 1] >= 0.125 && ringCorrs[ind] <= 0.125) {
      eighthCross = (float)(frcDeltaR * (ind - 0.5 + (ringCorrs[ind - 1] - 0.125) / 
                                         (ringCorrs[ind - 1] - ringCorrs[ind])));
    }
  }
  cenBin = B3DNINT(0.25 / frcDeltaR - 0.5);
  numBins = B3DMAX(1, B3DNINT(0.075 / frcDeltaR));
  halfNyq = 0.;
  for (ind = 0; ind < numBins; ind++)
    halfNyq += (float)(ringCorrs[ind + cenBin - numBins / 2] / numBins);
}
