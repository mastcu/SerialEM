/*
 *  framealign.cpp - module for aligning movie frames passed in sequentially
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 2016-2026 by the Regents of the University of
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 *  No ID line: it is shared between 3 different projects
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#include <Windows.h>
#include <sys/stat.h>

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif
#ifndef _delayimp_h
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif
#endif
#include "b3dutil.h"
#include "cppdefs.h"
#include "cfft.h"
#include "framealign.h"
#include "frameutil.h"

#define START_TIMER  if (mReportTimes) mWallStart = wallTime();
#define ADD_TIME(a) if (mReportTimes) a += wallTime() - mWallStart;

#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
#define GET_PROC(t, s, n) s = (t)GetProcAddress(sGpuModule, #n); if (!s) err++;
#define GPU_DLL_NAME "FrameGPU.dll"

typedef void (*FgpuNoArgType)(void);
typedef int (*FgpuTwoIntType)(int, int);
typedef int (*ReturnIntType)(void);
typedef void (*ReturnTwoIntType)(int *, int *);
typedef void (*SetGroupSizeType)(int);
typedef int (*GpuAvailableType)(int, float *, int);
typedef void (*SetUnpaddedSizeType)(int, int , int, int);
typedef int (*SetPreProcParamsType)(float *, int, int, float, unsigned char *, int, int);
typedef void (*SetBinPadParamsType)(int, int, int, int, int, int, int, int, int, int);
typedef int (*SetupSummingType)(int, int, int, int, int);
typedef int (*SetupAligningType)(int, int, int, int, float *, int, int, int, int);
typedef int (*SetupDoseWeightingType)(float *, int, float);
typedef int (*AddToFullSumType)(float *, float, float);
typedef int (*ReturnSumsType)(float *, float *, float *, int);
typedef int (*NewFilterMaskType)(float *);
typedef int (*ShiftAddToAlignSumType)(int, float, float, int);
typedef int (*CrossCorrelateType)(int, int, float *, int, int);
typedef int (*ProcessAlignImageType)(float *, int, int, int);
typedef int (*ReturnAlignFFTsType)(float **, float **, float *, float *);
typedef int (*ReturnStackedFrameType)(float *, int *);
typedef void (*SetPrintFuncType)(CharArgType func);

static HMODULE sGpuModule;
static GpuAvailableType sFgpuGpuAvailable;
static SetUnpaddedSizeType sFgpuSetUnpaddedSize;
static SetPreProcParamsType sFgpuSetPreProcParams;
static SetBinPadParamsType sFgpuSetBinPadParams;
static SetupSummingType sFgpuSetupSumming;
static SetupAligningType sFgpuSetupAligning;
static SetupDoseWeightingType sFgpuSetupDoseWeighting;
static AddToFullSumType sFgpuAddToFullSum;
static ReturnSumsType sFgpuReturnSums;
static NewFilterMaskType sFgpuReturnUnweightedSum;
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
static ReturnStackedFrameType sFgpuReturnStackedFrame;
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
#define sFgpuSetUnpaddedSize fgpuSetUnpaddedSize
#define sFgpuSetPreProcParams fgpuSetPreProcParams
#define sFgpuSetBinPadParams fgpuSetBinPadParams
#define sFgpuSetupSumming fgpuSetupSumming
#define sFgpuSetupAligning fgpuSetupAligning
#define sFgpuSetupDoseWeighting fgpuSetupDoseWeighting
#define sFgpuAddToFullSum fgpuAddToFullSum
#define sFgpuReturnSums fgpuReturnSums
#define sFgpuReturnUnweightedSum fgpuReturnUnweightedSum
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
#define sFgpuReturnStackedFrame fgpuReturnStackedFrame
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
  mUnweightSum = NULL;
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

// 1/3/19: tried to free library in cleanup so that it could be replaced wih DM running
// but DM holds on to all DLL's in the Plugins directory
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
                           int nx, int ny, float padFrac, float taperFrac,
                           int antiFiltType, float radius1, float *radius2, float sigma1,
                           float *sigma2, int numFilters, int maxShift, float kFactor,
                           float maxMaxWeight, int summingMode, int expectedZ,
                           int makeUnwgtSum, int gpuFlags, int debug)
{
  int alignXpad, alignYpad, sumXpad, sumYpad, fullXpad, fullYpad, ind, filt;
  int divisor, niceLimit, nxPad, nyPad, nxTrim, nyTrim, nxUse, nyUse;
  int expectStack, gpuStackLimit, aliFiltSize, minFiltSize = 128;
  int xReduceTemp = 0, yReduceTemp = 0;
  bool doBinPad = trimFrac != 0. || taperFrac != 0.;

  // Default calls from alignframes/SEM have taperfrac (for align) 0.1,
  // fullTaperFrac -> padFrac for full image at 0.02, and trimFrac of 0.1

  // Make just a full filter mask with high-frequency included if only one filter,
  // no GPU, and not refining at end
  bool justFullFilt = numFilters == 1 && !cumAlignAtEnd &&
    (gpuFlags & GPU_FOR_ALIGNING) == 0;
  int niceGpuLimit = 5;
  bool noisePadOnGpu, binPadOnGpu, stackOnGpu;

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
    mPredMeanSum[filt] = 0.;
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

  mAntiFiltType = antiFiltType;
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
    mSavedFullFrameNum.clear();
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
    mShiftTemp = B3DMALLOC(float, 2 * B3DMAX(nx, ny) + (fullXpad - nx) + (fullYpad - ny));
    mLinePtrs = B3DMALLOC(unsigned char *, fullYpad);
    if (testAndCleanup(!mWorkFullSize || !mShiftTemp || !mLinePtrs))
      return 2;
    mFullXpad = fullXpad;
    mFullYpad = fullYpad;
  }

  // And size of final sum(s)
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
  }

  if (sumXpad != mSumXpad || sumYpad != mSumYpad || makeUnwgtSum != mMakeUnwgtSum) {
    B3DFREE(mUnweightSum);
    if (makeUnwgtSum) {
      mUnweightSum = B3DMALLOC(float, (sumXpad + 2) * sumYpad);
      if (testAndCleanup(!mUnweightSum))
        return 2;
    }
  }
  mSumXpad = sumXpad;
  mSumYpad = sumYpad;

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
  ACCUM_MIN(aliFiltSize, alignXpad);
  ACCUM_MIN(aliFiltSize, alignYpad);
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
  noisePadOnGpu = doBinPad && mGpuSumming && (gpuFlags & GPU_DO_NOISE_TAPER);
  binPadOnGpu = doBinPad && mGpuAligning && (gpuFlags & GPU_DO_BIN_PAD);
  stackOnGpu = noisePadOnGpu && binPadOnGpu && (gpuFlags & STACK_FULL_ON_GPU);
  gpuStackLimit = (gpuFlags >> GPU_STACK_LIM_SHIFT) & GPU_STACK_LIM_MASK;

  mNumExpectedFrames = expectedZ;
  expectStack = B3DMIN(numAllVsAll, expectedZ);
  if (cumAlignAtEnd)
    expectStack = expectedZ;
  if (gpuFlags) {
    mFlagsForUnpadCall = (noisePadOnGpu ? GPU_DO_NOISE_TAPER : 0) |
      (binPadOnGpu ? GPU_DO_BIN_PAD : 0) |
      (stackOnGpu ? STACK_FULL_ON_GPU : 0) |
      ((stackOnGpu && gpuStackLimit > 0) ? GPU_STACK_LIMITED : 0) |
      (gpuFlags & (GPU_AVG_SUPER_2X | GPU_AVG_SUPER_4X));
    sFgpuSetUnpaddedSize(nx, ny, mFlagsForUnpadCall,
                         (mDebug ? 1 : 0) + (mReportTimes ? 10 : 0));
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
    mEvenOddForSumSetup = ((gpuFlags & GPU_DO_EVEN_ODD) ? 1 : 0) +
      (((gpuFlags & GPU_DO_UNWGT_SUM) != 0 && makeUnwgtSum) ? 2 :0);
    if (gpuFlags && mGpuSumming && !mDeferSumming &&
        sFgpuSetupSumming(fullXpad, fullYpad, sumXpad, sumYpad, mEvenOddForSumSetup)) {
      sFgpuCleanup();
      gpuFlags = 0;
    }
  }

  // Save all members for current state
  mGpuFlags = gpuFlags;
  mGpuAligning = (gpuFlags & GPU_FOR_ALIGNING) != 0 && summingMode >= 0;
  mGpuSumming = (gpuFlags & GPU_FOR_SUMMING) != 0 && summingMode <= 0;
  mNoisePadOnGpu = gpuFlags != 0 && noisePadOnGpu;
  mBinPadOnGpu = gpuFlags != 0 && binPadOnGpu;
  mStackUnpadOnGpu = gpuFlags != 0 && stackOnGpu;
  mUnwgtOnGpu = mGpuSumming && (gpuFlags & GPU_DO_UNWGT_SUM) && makeUnwgtSum;

  // Set this to 0 if no stacking, but it also has to be 0 to indicate no limit
  mGpuStackLimit = mStackUnpadOnGpu ? gpuStackLimit : 0;
  mNumStackedOnGpu = 0;
  mNumFullSaved = 0;
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
  if (mUnweightSum)
    memset(mUnweightSum, 0, (sumXpad + 2) * sumYpad * sizeof(float));
  mWallFullFFT = mWallBinPad = mWallBinFFT = mWallReduce = mWallShift = 0.;
  mWallConjProd = mWallFilter = mWallPreProc = mWallNoise = 0.;
  mDoingDoseWeighting = false;
  CLEAR_RESIZE(mDoseWgtFilter, float, 0);
  CLEAR_RESIZE(mReweightFilt, float, 0);
  mDWFdelta = 0.;
  mMakeUnwgtSum = makeUnwgtSum;
  return 0;
}

/*
 * Store parameters and resize arrays for dose weighting, also save a reweighting
 * filter at this time.  Pass NULL for reweightFilt if no reweighting is to be done;
 * pass an array with all 1's to have a normalizing reweighting computed here
 */
int FrameAlign::setupDoseWeighting(float priorDose, float *frameDoses, float pixelSize,
                                   float critScale, float aFac, float bFac, float cFac,
                                   float *reweightFilt, int &filtSize)
{
  int ind, frame;
  bool allOnes = true;
  mDoingDoseWeighting = true;
  mPriorDoseCum = priorDose;
  CLEAR_RESIZE(mFrameDoses, float, mNumExpectedFrames);
  for (ind = 0; ind < mNumExpectedFrames; ind++)
    mFrameDoses[ind] = frameDoses[ind];
  mPixelSize = pixelSize;
  mCritDoseScale = critScale;
  mCritDoseAfac = aFac;
  mCritDoseBfac = bFac;
  mCritDoseCfac = cFac;
  filtSize = 2 * B3DMAX(mFullXpad, mFullYpad);
  B3DCLAMP(filtSize, 1024, 8193);
  CLEAR_RESIZE(mDoseWgtFilter, float, filtSize);
  if (reweightFilt) {
    CLEAR_RESIZE(mReweightFilt, float, filtSize);
    for (ind = 0; ind < filtSize; ind++) {
      mReweightFilt[ind] = reweightFilt[ind];
      if (reweightFilt[ind] != 1.0)
        allOnes = false;
    }

    // If the reweight filter is all ones, compute a normalizing filter here from the
    // inverse of the sum of filters to be used
    if (allOnes) {
      for (ind = 0; ind < filtSize; ind++)
        mReweightFilt[ind] = 0.;
      for (frame = 0; frame < mNumExpectedFrames; frame++) {
        doseWeightFilter(priorDose, priorDose + mFrameDoses[frame], mPixelSize,
                         mCritDoseAfac, mCritDoseBfac, mCritDoseCfac, mCritDoseScale,
                         &mDoseWgtFilter[0], (int)mDoseWgtFilter.size(), 0.71f,
                         &mDWFdelta);
        priorDose += mFrameDoses[frame];
        for (ind = 0; ind < filtSize; ind++)
          mReweightFilt[ind] += mDoseWgtFilter[ind];
      }
      for (ind = 0; ind < filtSize; ind++) {
        if (mReweightFilt[ind] > 0.)
          mReweightFilt[ind] = mNumExpectedFrames / mReweightFilt[ind];
      }
    }
  }
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
  B3DFREE(mUnweightSum);
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
  CLEAR_RESIZE(mFrameDoses, float, 0);
  CLEAR_RESIZE(mDoseWgtFilter, float, 0);
  mGpuFlags = 0;
}

/*
 * Find out if GPU is available
 */
int FrameAlign::gpuAvailable(int nGPU, float *memory, int debug)
{
  int err = 0;
#if defined(_WIN32) && defined(DELAY_LOAD_FGPU)
  int ind, lastErr = 0, lastSlash;

  // Try loading from current directory, from upper directory (for shrmemframe), and
  // from Shrmemframe directory because there seemed to be a point where GMS 3.42
  // would not load from the current dir, but would from a subdir
  const char *prefixDirs[] = {"", "..\\", "Shrmemframe\\"};
  *memory = 0.;
  if (mGpuLibLoaded == 0)
    return 0;
  if (mGpuLibLoaded < 0) {
    struct _stat statbuf;
    HMODULE thisModule = reinterpret_cast<HMODULE>(&__ImageBase);
    TCHAR dllPath[MAX_PATH];
    std::string absPathStr, hereStr;

    // Get the absolute path of the current plugin and strip off the plugin name
    if (GetModuleFileName(thisModule, &dllPath[0], MAX_PATH)) {
      absPathStr = dllPath;
      lastSlash = (int)absPathStr.rfind('\\');
      if (lastSlash >= 0)
        absPathStr.resize(lastSlash);
      absPathStr += '\\';
    }

    // Loop on the possibilities, first seeing if file exists, then trying to load it
    for (ind = 0; ind < sizeof(prefixDirs) / sizeof(const char *); ind++) {
      hereStr = absPathStr + prefixDirs[ind] + GPU_DLL_NAME;
      if (!_stat(hereStr.c_str(), &statbuf)) {
        sGpuModule = LoadLibrary(hereStr.c_str());
        if (sGpuModule)
          break;
        lastErr = GetLastError();
        utilPrint("Could not load %s  (error %d)\n", hereStr.c_str(), lastErr);
        err = 1;
      }
    }

    // Give message on definite failure to load
    if (!sGpuModule) {
      if (err)
#ifdef GMS_MAJOR_VERSION
        utilPrint("GPU is not directly available from SerialEMCCD plugin\n");
#else
        utilPrint("GPU is not available, could not load FrameGPU.dll\n");
#endif
      mGpuLibLoaded = 0;
      return 0;
    }
    GET_PROC(GpuAvailableType, sFgpuGpuAvailable, fgpuGpuAvailable);
    GET_PROC(SetUnpaddedSizeType, sFgpuSetUnpaddedSize, fgpuSetUnpaddedSize);
    GET_PROC(SetPreProcParamsType, sFgpuSetPreProcParams, fgpuSetPreProcParams);
    GET_PROC(SetBinPadParamsType, sFgpuSetBinPadParams, fgpuSetBinPadParams);
    GET_PROC(SetupSummingType, sFgpuSetupSumming, fgpuSetupSumming);
    GET_PROC(SetupAligningType, sFgpuSetupAligning, fgpuSetupAligning);
    GET_PROC(SetupDoseWeightingType, sFgpuSetupDoseWeighting, fgpuSetupDoseWeighting);
    GET_PROC(AddToFullSumType, sFgpuAddToFullSum, fgpuAddToFullSum);
    GET_PROC(ReturnSumsType, sFgpuReturnSums, fgpuReturnSums);
    GET_PROC(NewFilterMaskType, sFgpuReturnUnweightedSum, fgpuReturnUnweightedSum);
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
    GET_PROC(ReturnStackedFrameType, sFgpuReturnStackedFrame, fgpuReturnStackedFrame);
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
 * Function to preprocess an image with gain normalization, truncation and defect
 * correction
 */
void FrameAlign::preProcessFrame(void *frame, void *darkRef, int defBin, float *fOut)
{

  // OpenMP does not allow member variables to be shared
  int nxGain = mNxGain;
  float truncLimit = mTruncLimit;
  int type = mFrameType;
  int base = 0;
  float *gainRef = mGainRef;
  int numThreads, maxThreads, gainXoff, gainYoff, ix = 0, iy, top, left, bottom, right;
  float val = 0.;
  float *gainp = NULL;
  unsigned char *bFrame = (unsigned char *)frame;
  short *sFrame = (short *)frame;
  unsigned short *usFrame = (unsigned short *)frame;
  float *fFrame = (float *)frame;
  short *sDark = (short *)darkRef;
  unsigned short *usDark = (unsigned short *)darkRef;
  if (mGainRef) {
    gainXoff = (mNxGain - mNx) / 2;
    gainYoff = (mNyGain - mNy) / 2;
  }

  // All processing converts input to a float array: gain/trunction place it in float
  // otherwise it gets copied to float; then defect operates float->float
  maxThreads = 1;
  if (truncLimit > 0 && gainRef)
    maxThreads = 6;
  else if (truncLimit > 0 || gainRef)
    maxThreads = 3;
  numThreads = numOMPthreads(maxThreads);
  int nxt = mNx;
  int nyt = mNy;

  #pragma omp parallel for num_threads(numThreads)                    \
  shared(nxt, nyt, gainRef, nxGain, gainXoff, gainYoff, bFrame, sFrame, usFrame, fFrame, \
         fOut, sDark, usDark, type, frame, truncLimit)                  \
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
  case MRC_MODE_BYTE:
  for (ix = 0; ix < nxt; ix++) {
    val = bFrame[base + ix] * gainp[ix];
    fOut[base + ix] = val;
  }
  break;
  //          NORM_ONLY(MRC_MODE_BYTE, bFrame[base + ix]);
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
  if (mCamSizeX > 0) {
    left = (mCamSizeX / mDefectBin- mNx) / 2;
    top = (mCamSizeY / mDefectBin - mNy) / 2;
    right = left + mNx;
    bottom = top + mNy;
    CorDefCorrectDefects(mCamDefects, fOut, MRC_MODE_FLOAT, defBin, top, left,
                         bottom, right);
  }
}

/*
 * Operate on the next frame
 */
int FrameAlign::nextFrame(void *frame, int type, float *gainRef, int nxGain, int nyGain,
                          void *darkRef, float truncLimit,
                          CameraDefects *defects, int camSizeX, int camSizeY, int defBin,
                          float shiftX, float shiftY)
{
  bool saving = (mNumAllVsAll > 0 || mCumAlignAtEnd || mDeferSumming) && mSummingMode >=0;
  bool savingFull = (saving || (!mNumAllVsAll && mStackUnpadOnGpu)) && mSummingMode <= 0;
  bool doBinPad = mTaperFrac != 0. || mTrimFrac != 0.;
  bool needPreprocess = gainRef || truncLimit > 0 || camSizeX > 0;
  bool preprocOnGpu = (mGpuFlags & GPU_DO_PREPROCESS) && needPreprocess && !darkRef;
  bool preprocHere = needPreprocess && ((!mNoisePadOnGpu && mSummingMode <= 0) ||
                                        (!mBinPadOnGpu && mSummingMode >= 0) ||
                                        !preprocOnGpu);
  bool stackOnGpu;
  float *binArr = mWorkBinPad;
  float *fullArr = mWorkFullSize;
  float *tempBin, *groupArr;
  int nxBin, nyBin, nxTaper, nyTaper, ind, err, ref, xOffset, yOffset, useInd;
  int ix, filt, useFilt, stackByteSize;
  float xShift, yShift, nearXshift = 0., nearYshift = 0.;
  bool needExtract = true;
  bool didNoisePad = false;
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
  unsigned char *defectMap = NULL;
  CameraDefects gpuDefects;
  int camSizeXforGPU, camSizeYforGPU;

  // Save these as member variables to allow frame recovery from GPU
  mNoiseLength = B3DMAX(mNx, mNy) / 50;
  B3DCLAMP(mNoiseLength, 20, 120);
  mDefectBin = defBin;
  mFrameType = type;
  mNxGain = nxGain;
  mNyGain = nyGain;
  mGainRef = gainRef;
  mCamDefects = defects;
  mCamSizeX = camSizeX;
  mCamSizeY = camSizeY;
  mTruncLimit = truncLimit;

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
        if (addToFull && filt == mNumFilters && addToSums(NULL, 0, -9, 0)) {
          cleanup();
          return 2;
        }
      } else {

        // Later, get mean difference between last and this set of shifts and adjust
        adjustAndPushShifts(1, filt, useFilt);
      }
      if (addToFull && filt == mNumFilters && addToSums
          (NULL, 0, -9, mNumFrames + 1 - numFrameForAVA)) {
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

    // No need to roll saved sum array conditionally here, rolling happens in addToSum
  }

  // Roll the saved align array if no cumulative alignment, roll groups unconditionally
  if (mNumAllVsAll && mNumFrames >= numBinPadForAVA && mSummingMode >= 0) {
    if (mGpuAligning) {
      if (!mCumAlignAtEnd)
        sFgpuRollAlignStack();
      if (mGroupSize > 1 && mNumFrames >= mNumAllVsAll + mGroupSize - 1)
        sFgpuRollGroupStack();
    } else {
      if (!mCumAlignAtEnd)
        utilRollSavedFrames(mSavedBinPad, numBinPadForAVA);
      if (mGroupSize > 1 && mNumFrames >= mNumAllVsAll + mGroupSize - 1) {
        utilRollSavedFrames(mSavedGroups, mNumAllVsAll);
      }
    }
  }
  stackOnGpu = mStackUnpadOnGpu && (!mGpuStackLimit || mNumStackedOnGpu < mGpuStackLimit);

  // For first frame, set up pre-processing params on GPU; do this here so it is easy to
  // fall back to CPU entirely if failure
  if (!mNumFrames && mGpuFlags > 0) {
    err = 0;
    if (camSizeX > 0 && preprocOnGpu) {
      camSizeXforGPU = camSizeX;
      camSizeYforGPU = camSizeY;
      gpuDefects = *mCamDefects;
      defectMap = B3DMALLOC(unsigned char, mNx * mNy);
      if (!defectMap) {
        err = 1;
        preprocOnGpu = false;
      }

      // Scale the defects down if they are scaled for K2 and defect binning value is 2
      if (!err && gpuDefects.K2Type > 0 && gpuDefects.wasScaled > 0 && defBin > 1) {
        CorDefScaleDefectsForK2(&gpuDefects, true);
        camSizeXforGPU /= 2;
        camSizeYforGPU /= 2;
      }
      CorDefFillDefectArray(&gpuDefects, camSizeXforGPU, camSizeYforGPU, defectMap, mNx,
                            mNy, true);
    }

    // It tests for both defectMap and camSizeX, so no need to make it 0 if not preproc
    if (sFgpuSetPreProcParams(preprocOnGpu ? gainRef : NULL, nxGain, nyGain,
                              preprocOnGpu ? truncLimit : 0.f, defectMap,
                              camSizeX, camSizeY))
      err = 1;
    free(defectMap);

    // Fallback to doing all preproc and prep steps on CPU if there was an error
    if (err) {
      stackOnGpu = preprocOnGpu = false;
      preprocHere = needPreprocess;
      cancelInitialStepsOnGPU();
    }
  }

  // If noise padding is done here, floats will be saved, but if it is done on GPU
  // and either no preprocessing happens or it happens there, then save raw images
  mStackType = MRC_MODE_FLOAT;
  if (mNoisePadOnGpu && (preprocOnGpu || !needPreprocess)) {
    mStackType = type;
  }
  dataSizeForMode(mStackType, &stackByteSize, &ix);

  // Substitute the save arrays for the working ones; create new and push if needed
  if (saving) {
    if (mNumAllVsAll && !mGpuAligning) {
      if (aliFrameInd < (int)mSavedBinPad.size()) {
        binArr = mSavedBinPad[aliFrameInd];
      } else {
        binArr = B3DMALLOC(float, mAlignPix);
        if (testAndCleanup(!binArr))
          return 1;
        mSavedBinPad.push_back(binArr);
      }
    }
    if (savingFull && !stackOnGpu) {
      if (mNumFullSaved < (int)mSavedFullSize.size()) {
        fullArr = mSavedFullSize[mNumFullSaved];
        mSavedFullFrameNum[mNumFullSaved] = mNumFrames;
      } else {
        fullArr = (float *)malloc(stackByteSize * (mFullXpad + 2) * mFullYpad);
        if (testAndCleanup(!fullArr))
          return 1;
        mSavedFullSize.push_back(fullArr);
        mSavedFullFrameNum.push_back(mNumFrames);
      }
      mNumFullSaved++;
    }
  }

  /* Data flow for cases:
     fullArr is mWorkBinPad unless saving full images here, then it is in mSavedFullSize
     Have float in useFrame = fullArr after pre-preprocessing, otherwise useType in frame
     Doing preproc on GPU makes sense only if both noisepad and binpad are to be done
     there, since otherwise I have to CPU preproc anyway
     ALIGNING ONLY with regular doBinPad
     - Not saving on stack, no NTP to think about
     - For binpad on GPU:
     -   1) No preproc: send frame/useFrame to align
     -   2) Preproc here: send useFrame to align
     -   3) Preproc on GPU: send frame to align
     - No binpad on GPU:
     -   Send binArr as usual, prepared from fullArr
     -   Preproc: produces floats in useFrame = mWorkFullSize
     -   If preproc or input is floats: set fullArr to useFrame
     -   No preproc and no floats: need taperoutpad to float in fullArr
     SUMMING ONLY
     -  Should do NTP on GPU
     -  If no NTP here pass useFrame of form useType for summing
     -    set fullArr to useFrame to send to summing
     ALIGNING AND SUMMING for sure:
     No binpad or noisepad on GPU:
     - No regular doBinPad:
     -   Does NTP to float in fullArr
     - Regular doBinPad: NTP to float in fullArr
     NTP on GPU only, no stack on GPU:
     - Want to save unpadded image, but there has to be floats to get the binpad
     - Have to do PreProc here for binpad
     - 1) No preproc:
     -   Store input array
     -   If already floats, memcpy useFrame (frame) to fullArr
     -   If NOT floats, memcpy useFrame to fullArr then set fullArr back to mWorkFullSize
     -     It will then taperoutpad to float in mWorkFullSize
     - 2) Preproc, not on GPU:
     -   Need to store preproc array as floats and pass to GPU later.  It goes to fullArr
     - 3) Preproc here and on GPU:
     -   Should only do this if input not floats
     -   Store input array: stackType = type
     -   memcpy frame to fullArr then set fullArr back to mWorkFullSize
     -     No taperoutpad; useType is floats
     - addToSum must pass unpadded stackType array to get NTP'd
     - Recovery requires NTP of frames stacked here, with possible preproc for case 3)
     Binpad on GPU only (never stack unpad on GPU):
     - It needs the unpadded image to go to the GPU
     - But it wants to do the NTP here which will make floats
     - Have to do Preproc here for NTP
     - Could send raw image if there is space for preproc arrays in GPU
     - All of these stack NTP'd array as usual
     - 1) No preproc
     -   Send raw image in frame, in type form
     -   This is the same as useFrame, in useType form
     - 2) Preproc only here:
     -   Send unpadded image in useFrame, in useType form
     -   But need to preproc into mWorkFullSize not fullArr
     - 3) preproc in both places, only if input not float (no sense for float)
     -   Send raw image in frame, in type form
     Both on GPU, no stack on GPU
     - It needs the unpadded image to go to the GPU for both operations
     - It can do preproc twice there to save doing it here (if space and if not float)
     - 1) No preproc
     -   The original unpadded image needs to be stacked here (frame in type form)
     -   This is the same as useFrame, in useType form
     -   memcpy frame/useFrame to fullArr, what about set fullArr back to mWorkFullSize?
     - 2) Preproc only here:
     -   Send unpadded image in useFrame, in useType form
     -   Preproc into fullArr as usual, into stack
     - 3) Preproc on GPU:
     -   The image for aligning must be preproc'd and bin-padded by GPU
     -   addToSum must pass unpadded original type array to get PreProc'd and NTP'd
     -   Since no preproc done, this is the same as useFrame, in useType form
     -   memcpy frame/useFrame to fullArr, what about set fullArr back to mWorkFullSize?
     - Recovery requires NTP and possible preproc of frames stacked here
     Both on GPU, stack on GPU:
     - It needs to send the original unpadded image to be stacked AND binpadded
     - Send the same place and way as with no stack, but with flag to stack it
     - 1) No preproc
     -   Send frame/useFrame in type/useType form to align and stack
     - 2) Preproc only here:
     -   Send unpadded image in useFrame, in useType form
     - 3) Preproc on GPU:
     -   Send frame/useFrame in type/useType form to align and stack
     - addToSum must indicate which frame on stack to use
     - Recovery requires retrieving frames, putting in stack on front or end,
       and preproc and NTP of all frames
  */

  // Now set flags for all these cases as needed
  bool needTaperOut = false, setFullToUse = false, copyToStackHere = false;
  bool setFullToWork = false, copyRawInput = false, sendRawToBinPad = false;
  bool preprocIntoWork = false;
  if (mSummingMode > 0) {          // Aligning only
    if (doBinPad) {
      if (mBinPadOnGpu) {
        sendRawToBinPad = preprocOnGpu;
      } else {
        setFullToUse = needPreprocess || type == MRC_MODE_FLOAT;
        needTaperOut = !setFullToUse;
      }
    }
  } else if (mSummingMode < 0) {   // summing only
    setFullToUse = mNoisePadOnGpu;
  } else  {                        // Align and sum
    if (!mBinPadOnGpu && !mNoisePadOnGpu) {
      copyToStackHere = !needPreprocess && type == MRC_MODE_FLOAT;
    } else if (!mBinPadOnGpu && mNoisePadOnGpu) {
      if (!needPreprocess) {
        copyToStackHere = true;
        setFullToWork = true;
        needTaperOut = true;
      } else if (preprocOnGpu) {
        copyToStackHere = true;
        copyRawInput = true;
        setFullToWork = true;
      }
    } else if (mBinPadOnGpu && !mNoisePadOnGpu) {
      sendRawToBinPad = preprocOnGpu || !needPreprocess;
      preprocIntoWork = preprocHere && !preprocOnGpu;
    } else if (mBinPadOnGpu && mNoisePadOnGpu && !stackOnGpu) {
      if (!needPreprocess) {
        copyToStackHere = true;
      } else if (preprocOnGpu) {
        copyToStackHere = true;
      }
    }
  }

  if (doBinPad) {
    nxBin = (mXend + 1 - mXstart) / mBinAlign;
    nyBin = (mYend + 1 - mYstart) / mBinAlign;
    nxTaper = (int)(mTaperFrac * nxBin);
    nyTaper = (int)(mTaperFrac * nyBin);
  }

  // PRE-PROCESS IF ANY
  // Namely if there is gain reference, truncation, or defect correction

  // If using GPU, set parameters to do the pre-proc, bin-pad and regular processing,
  // and possible stacking there.  This is where it finds out the stack type
  if (mGpuFlags) {
    sFgpuSetBinPadParams(mXstart, mXend, mYstart, mYend, mBinAlign, nxTaper, nyTaper,
                         (preprocHere && !sendRawToBinPad) ? MRC_MODE_FLOAT : type,
                         mAntiFiltType, mNoiseLength);
  }

  // Copy into stack now if it is to be raw
  if (copyToStackHere && copyRawInput) {
    dataSizeForMode(type, &useInd, &ix);
    memcpy(fullArr, frame, useInd * mNx * mNy);
    if (setFullToWork)
      fullArr = mWorkFullSize;
  }

  // Otherwise process here if needed
  if (preprocHere) {

    // Substitute source pointer and type
    useFrame = preprocIntoWork ? mWorkFullSize : fullArr;
    useType = MRC_MODE_FLOAT;
    START_TIMER;
    preProcessFrame(frame, darkRef, defBin, useFrame);
    ADD_TIME(mWallPreProc);
    //utilDumpImage(fullArr, mNx, mNx, mNy, 0, "processed original");
  }
  if (mBinPadOnGpu)
    binArr = sendRawToBinPad ? (float *)frame : useFrame;

  // Now copy data into stack if flag is set
  if (copyToStackHere && !copyRawInput) {
    dataSizeForMode(useType, &useInd, &ix);
    memcpy(fullArr, useFrame, useInd * mNx * mNy);
    if (setFullToWork)
      fullArr = mWorkFullSize;
  }

  // PROCESS THE CURRENT IMAGE: Get the padded full image
  if (mBinPadOnGpu) {
    err = sFgpuProcessAlignImage(binArr, saving ? aliFrameInd : -1, groupInd,
                                        savingFull && stackOnGpu);
    if (err) {

      // If doing noise pad on GPU then need to possibly get stack back from GPU, and
      // also preprocess and noise pad it to be like normal ACPU stack here
      if (mNoisePadOnGpu) {
        if (recoverGpuFullStack(savingFull && stackOnGpu, &binArr)) {
          cleanup();
          return 3;
        }
        didNoisePad = true;
        fullArr = binArr;
        binArr = mWorkBinPad;
        stackOnGpu = false;
      }
      cancelInitialStepsOnGPU();
      setFullToUse = false;

      // But if it was a downstream error, now cancel aligning as well
      if (err == 1) {
        if (recoverGpuAlignFFTs(saving, aliFrameInd, mNumAllVsAll ? NULL : mAlignSum,
                                saving ? NULL : mWorkBinPad, NULL, false))
          return 3;
        binArr = mSavedBinPad[aliFrameInd];
      }
    } else if (stackOnGpu)
      mNumStackedOnGpu++;
  }

  if ((mSummingMode <= 0 && !mNoisePadOnGpu) || !doBinPad) {
    // Big artifacts from the taper method.
    //sliceTaperOutPad(useFrame, useType, mNx, mNy, fullArr, mFullXpad + 2, mFullXpad,
    //mFullYpad, 0, 0.);
    // This produced floats into the full array
    START_TIMER;
    if (!didNoisePad)
      sliceNoiseTaperPad(useFrame, useType, mNx, mNy, fullArr, mFullXpad + 2, mFullXpad,
                         mFullYpad, mNoiseLength, 4, mShiftTemp);
    ADD_TIME(mWallNoise);

    // Set larger size for bin/pad operation to come from this array
    nxDimForBP = mFullXpad + 2;
    nxForBP = mFullXpad;
    nyForBP = mFullYpad;
  } else if (needTaperOut) {

    // This simply converts to float array with no taper/pad
    // This can't be speeded up even though copyToCenter looks suspicious.  Tried copying
    // forward in a simple function and it was actually twice as fast to copy backwards
    START_TIMER;
    sliceTaperOutPad(useFrame, useType, mNx, mNy, fullArr, mNx,  mNx, mNy, 0, 0.);
    ADD_TIME(mWallNoise);
  } else if (setFullToUse) {

    // And if it is float, just assign it to replace the work array
    fullArr = useFrame;
  }
  //utilDumpImage(fullArr, nxDimForBP, nxForBP, nyForBP, 0, "padded tapered original");

  // If doing trim or taper, bin the subarea and taper inside, take the FFT
  if (doBinPad && mSummingMode >= 0 && !mBinPadOnGpu) {
    xOffset = (nxForBP - mNx) / 2;
    yOffset = (nyForBP - mNy) / 2;

    // Use zoomdown routine for binning, it is a lot faster
    // Have to select the filter again because reading EER selects filter too
    START_TIMER;
    if (mBinAlign > 1) {
      for (ind = 0; ind < nyForBP; ind++)
        mLinePtrs[ind] = (unsigned char *)(fullArr + ind * nxDimForBP);
      if (selectZoomFilter(mAntiFiltType, 1. / mBinAlign, &ind) ||
          !zoomWithFilter(mLinePtrs, nxForBP, nyForBP, (float)(mXstart + xOffset),
                          (float)(mYstart + yOffset), nxBin, nyBin,
                          nxBin, 0, MRC_MODE_FLOAT, binArr, NULL, NULL))
        needExtract = false;
    }
    if (needExtract)
      extractWithBinning(fullArr, MRC_MODE_FLOAT, nxDimForBP, mXstart + xOffset,
                         mXend + xOffset, mYstart + yOffset, mYend + yOffset,
                         mBinAlign, binArr, 0, &nxBin, &nyBin);
    //utilDumpImage(binArr, nxBin, nxBin, nyBin, 0, "binned");

    sliceTaperInPad(binArr, MRC_MODE_FLOAT, nxBin, 0, nxBin - 1, 0, nyBin - 1, binArr,
                    mAlignXpad + 2, mAlignXpad, mAlignYpad, nxTaper, nyTaper);
    //utilDumpImage(binArr, mAlignXpad + 2, mAlignXpad, mAlignYpad, 0, "padded tapered");
    ADD_TIME(mWallBinPad);;

  }

  if (mGpuAligning && doBinPad && !mBinPadOnGpu) {

    // Take FFT and save in stack on GPU if needed; if there is an error here, try to
    // recover the stack or the sum and just turn off GPU aligning
    if (sFgpuProcessAlignImage(binArr, saving ? aliFrameInd : -1, groupInd,
                               savingFull && stackOnGpu)) {

      if (recoverGpuAlignFFTs(saving, aliFrameInd, mNumAllVsAll ? NULL : mAlignSum,
                              saving ? NULL : mWorkBinPad, &binArr,
                              savingFull && stackOnGpu))
        return 3;
    } else if (stackOnGpu)
      mNumStackedOnGpu++;
  }

  if (!mGpuAligning) {
    START_TIMER;
    todfftc(binArr, mAlignXpad, mAlignYpad, 0);
    ADD_TIME(mWallBinFFT);
  }

  // Take the full FFT
  if (!doBinPad || (mSummingMode <= 0 && !mGpuSumming)) {
    START_TIMER;
    todfftc(fullArr, mFullXpad, mFullYpad, 0);
    ADD_TIME(mWallFullFFT);
  }

  // If just summing, add into sum and return
  if (mSummingMode < 0) {
    mXshifts[0].push_back(shiftX);
    mYshifts[0].push_back(shiftY);
    mXshifts[1].push_back(shiftX);
    mYshifts[1].push_back(shiftY);
    if (addToSums(fullArr, -1, -9, mNumFrames)) {
      cleanup();
      return 2;
    }
    mNumFrames++;
    return 0;
  }

  // Now if not doing taper/pad, reduce the FFT into the align array
  if (!doBinPad) {
    START_TIMER;
    fourierReduceImage(fullArr, mFullXpad, mFullYpad, binArr, mAlignXpad, mAlignYpad,
                       0., 0., mShiftTemp);
    ADD_TIME(mWallReduce);
  }

  // Apply full filter to the align array unless on GPU
  if (!mGpuAligning) {
    START_TIMER;
    for (ind = 0; ind < mAlignPix; ind++)
      binArr[ind] *= mFullFiltMask[ind];
    ADD_TIME(mWallFilter);
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
      //utilDumpFFT(groupArr, mAlignXpad, mAlignYpad, "group", 1, groupInd);
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
    if (mSummingMode <= 0 && !mDeferSumming && stackOnGpu)
      ix = 0;
    else
      ix = (mSummingMode <= 0 && !mDeferSumming) ? -1 : -9;
    if (addToSums((mSummingMode <= 0 && !mDeferSumming && !stackOnGpu) ? fullArr : NULL,
                  ix, useInd, mNumFrames)) {
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
  int row, numData, numCol, numInCol, ref, ind, filt, allInd;
  int maxAsBest, indOfMax = -1, maxDrop, numSets, dropRun, numPred, indMaxDist = 0;
  bool pickable, failed[MAX_FILTERS];
  int numFrames = B3DMIN(mNumFrames, mNumAllVsAll + mGroupSize - 1);
  int numGroups = numFrames + 1 - mGroupSize;
  float resMean[MAX_FILTERS], resSD[MAX_FILTERS], predMean[MAX_FILTERS];
  float maxWgtRes[MAX_FILTERS], maxRaw[MAX_FILTERS];
  int numFailed[MAX_FILTERS];

  bool doRobust = numFrames >= 5 && mKfactor > 0.;
  float finalX = 0., finalY = 0., fracDrop, predSum;
  float errx, erry, resid, wgtResid, resSum, resSumSq, distFilt, dist0, weight;
  float minWgt = 2.f, minError = 1.e30f;
  float maxFitDist = 0., fitDist[MAX_FILTERS], errMeasure[MAX_FILTERS];
  float smoothDist[MAX_FILTERS], maxSmoothDist = 0.;
  float distCrit = 0.75f;
  float notZeroCrit = 4.f;
  float absZeroCrit = (float)(0.15 * mBinAlign);
  float relZeroCrit = (float)(0.5 * mBinAlign);
  float closerRatio = 0.2f;
  FloatVec fullWeights;
  std::set<int> dropSet;
  int indResMin = 0, indPredMin = 0;
  numCol = numGroups + 3;
  numInCol = numGroups - 1;

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
    dropSet.clear();
    doRegression(doRobust, filt, filt, dropSet);

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
    fullWeights.clear();
    for (ind = 1; ind < numGroups; ind++) {
      for (ref = 0; ref <= ind - mGroupSize; ref++) {
        allInd = ref * mNumAllVsAll + ind;
        errx = (mXfitShifts[filt][ind] - mXfitShifts[filt][ref]) -
          mXallShifts[filt][allInd];
        erry = (mYfitShifts[filt][ind] - mYfitShifts[filt][ref]) -
          mYallShifts[filt][allInd];
        resid = sqrt(errx * errx + erry * erry);
        weight = 1.;
        if (doRobust)
          weight = mFitMat[numCol * row + numInCol + 2];
        wgtResid = resid * weight;
        ACCUM_MIN(minWgt, weight);
        fullWeights.push_back(weight);
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

    sumsToAvgSD(resSum, resSumSq, numData, &resMean[filt], &resSD[filt]);
    predMean[filt] = resMean[filt];

    // Cross-validation for >= 4 groups
    if (numGroups >= 4) {

      // This fancy graded ratio doesn't really matter, it ends up jumping to 2 points
      // at 21 frames
      fracDrop = (float)((0.01 * numData) / numGroups);
      B3DCLAMP(fracDrop, 0.05f, 0.1f);
      maxDrop = B3DMAX(1, (int)(fracDrop * numData));
      numSets = (numData + maxDrop - 1) / maxDrop;
      numPred = 0;
      predSum = 0.;

      // Loop on the runs, set up the drop set for a run, and do regression
      for (dropRun = 0; dropRun < numSets; dropRun++) {
        dropSet.clear();
        for (ind = dropRun; ind < numData; ind += numSets)
          dropSet.insert(ind);
        doRobust = numData - (int)dropSet.size() >= 2 * numGroups && mKfactor > 0.;
        doRegression(doRobust, filt, MAX_FILTERS, dropSet);

        // Compute the leave-out error
        row = 0;
        for (ind = 1; ind < numGroups; ind++) {
          for (ref = 0; ref <= ind - mGroupSize; ref++) {
            if (dropSet.count(row)) {
              allInd = ref * mNumAllVsAll + ind;
              errx = (mXfitShifts[MAX_FILTERS][ind] - mXfitShifts[MAX_FILTERS][ref]) -
                mXallShifts[filt][allInd];
              erry = (mYfitShifts[MAX_FILTERS][ind] - mYfitShifts[MAX_FILTERS][ref]) -
                mYallShifts[filt][allInd];
              resid = sqrt(errx * errx + erry * erry);
              predSum += (doRobust ? fullWeights[row] : 1.f) * resid;
              numPred++;
            }
            row++;
          }
        }
      }
      predMean[filt] = predSum / numPred;
    }

    // Maintain stats for this filter
    if (resMean[filt] < resMean[indResMin])
      indResMin = filt;
    if (predMean[filt] < predMean[indPredMin])
      indPredMin = filt;

    mPredMeanSum[filt] += predMean[filt];
    mResMeanSum[filt] += resMean[filt];
    mResSDsum[filt] += resSD[filt];
    mResMaxSum[filt] += maxWgtRes[filt];
    mRawMaxSum[filt] += maxRaw[filt];
    ACCUM_MAX(mMaxResMax[filt], maxWgtRes[filt]);
    ACCUM_MAX(mMaxRawMax[filt], maxRaw[filt]);
    errMeasure[filt] = (1.f - mMaxMaxWeight) * predMean[filt] +
      mMaxMaxWeight * maxWgtRes[filt];
    failed[filt] = numFailed[filt] >= B3DMAX(1, numFrames - 2);
    if (failed[filt])
      mNumAsBestFilt[filt]--;
    smoothDist[filt] = 0.;
    if (mXshifts[filt].size() >= 3)
      smoothDist[filt] = smoothedTotalDistance(&mXshifts[filt][0], &mYshifts[filt][0],
                                               (int)mXshifts[filt].size(), dist0);
    if (maxFitDist < fitDist[filt]) {
      indMaxDist = filt;
      maxFitDist = fitDist[filt];
    }
    ACCUM_MAX(maxSmoothDist, smoothDist[filt]);
    //PRINT3(filt, fitDist[filt],  smoothDist[filt]);
    if (maxAsBest < mNumAsBestFilt[filt]) {
      indOfMax = filt;
      maxAsBest = mNumAsBestFilt[filt];
    }

    // On the last fit, we now know the best filter and can manage the hybrid values
    if (filt == mNumFilters - 1) {
      //PRINT2(maxFitDist, maxSmoothDist);

      // Determine the best filter, discounting ones that have a much lower distance than
      // the maximum distance.  This is
      // driven by assumption/finding that ones with lower distance were contaminated by
      // bad fits from fixed pattern noise, but it should only be applied when the filter
      // is higher than the one that gives the maximum distance.  In the other direction
      // bigger distances reflect noise from overfitting.
      for (ind = 0; ind < mNumFilters; ind++) {
        if (!mPickedBestFilt && mNumAsBestFilt[ind] > -mFailedOftenCrit && !failed[ind] &&
            errMeasure[ind] < minError &&
            !(ind >= indMaxDist && fitDist[ind] < distCrit * maxFitDist) &&
            !(maxAsBest >= mNumAsBestFilt[ind] + mPickDiffCrit &&
              maxAsBest >= mPickRatioCrit * mNumAsBestFilt[ind])) {
          minError = errMeasure[ind];
          mBestFilt = ind;
        }
        //utilPrint("%d  %.3f  ", mNumAsBestFilt[ind], errMeasure[ind]);
      }
      //PRINT2(mBestFilt, indOfMax);

      mResMeanSum[mNumFilters] += resMean[mBestFilt];
      mPredMeanSum[mNumFilters] += predMean[mBestFilt];
      mResSDsum[mNumFilters] += resSD[mBestFilt];
      mResMaxSum[mNumFilters] += maxWgtRes[mBestFilt];
      mRawMaxSum[mNumFilters] += maxRaw[mBestFilt];
      ACCUM_MAX(mMaxResMax[mNumFilters], maxWgtRes[mBestFilt]);
      ACCUM_MAX(mMaxRawMax[mNumFilters], maxRaw[mBestFilt]);
      mNumFits++;
      if (!mPickedBestFilt && mNumFilters > 1 && mNumFrames >= mNumAllVsAll +
          mGroupSize -1) {
        mNumAsBestFilt[mBestFilt]++;

        // 3/15/22: Do not pick a filter at this point unless the hybrid solution is
        // actually going to be used
        pickable = mUseHybrid != 0;
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
  if (numGroups >= 4 && mDebug)
    for (ind = 0; ind < mNumFilters; ind++)
      if ((ind == indPredMin || ind == indResMin) && indResMin != indPredMin)
      utilPrint("filt %d res %.3f%s  pred %.3f%s\n", ind, resMean[ind], ind == indResMin
                ? "*":" ", predMean[ind], ind == indPredMin?"*":" ");
    }
    if (mDebug > 1) {
      utilPrint("%sresidual: mean = %.2f, SD = %.2f, max = %.2f,  n = %d\n",
                doRobust ? "weighted " : "", resMean[filt], resSD[filt], maxWgtRes[filt],
                numGroups);
      if (doRobust)
        utilPrint("    unweighted max residual = %.2f, min weight = %.3f\n",
                  maxRaw[filt], minWgt);
    }
  }
}

/*
 * Fill matrix for regression, optionally dropping the ones in the set, do robust or
 * regular regression, using values for filt but putting solution in fitInd
 */
void FrameAlign::doRegression(bool doRobust, int filt, int fitInd, std::set<int> &dropSet)
{
  int row, col, numData, numCol, numInCol, ref, ind, numIter, maxZeroWgt, allInd;
  float solMat[2 * MAX_ALL_VS_ALL], xMean[MAX_ALL_VS_ALL], xSD[MAX_ALL_VS_ALL];
  float maxChange = 0.02f;
  float maxOscill = 0.05f;
  int maxIter = 50;
  int numFrames = B3DMIN(mNumFrames, mNumAllVsAll + mGroupSize - 1);
  int numGroups = numFrames + 1 - mGroupSize;
  float finalX = 0., finalY = 0.;

  // Load the data matrix with the correlations
  row = 0;
  numCol = numGroups + 3;
  numInCol = numGroups - 1;
  allInd = 0;
  for (ind = 1; ind < numGroups; ind++) {
    for (ref = 0; ref <= ind - mGroupSize; ref++) {
      if (dropSet.count(allInd++))
        continue;
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
      printf("%.6f %.6f\n", mFitMat[numCol * row + numInCol],
      mFitMat[numCol * row + numInCol + 1]);*/
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
        utilPrint("robustRegress%s failed with error %d\n",
                  filt != fitInd ? " for CV" : "" ,row);
      doRobust = false;
    }
  }
  if (!doRobust)
    multRegress(mFitMat, numCol, 1, numInCol, numData, 2, 0, solMat, numInCol, NULL,
                xMean, xSD, mFitWork);

  // Copy to the appropriate fitShifts
  for (ind = 0; ind < numInCol; ind++) {
    finalX -= solMat[ind];
    finalY -= solMat[ind + numInCol];
    mXfitShifts[fitInd][ind] = solMat[ind];
    mYfitShifts[fitInd][ind] = solMat[ind + numInCol];
    //PRINT2(solMat[ind], solMat[ind + numInCol]);
  }
  mXfitShifts[fitInd][numInCol] = finalX;
  mYfitShifts[fitInd][numInCol] = finalY;
  //PRINT2(finalX, finalY);
}

/*
 * Finish aligning and averaging the remaining frames and return results
 */
int FrameAlign::finishAlignAndSum(float refineRadius2, float refineSigma2,
                                  float iterCrit, int groupRefine, int doSpline,
                                  float *alisum, float *xShifts, float *yShifts,
                                  float *rawXshifts, float *rawYshifts, float *ringCorrs,
                                  float deltaR, int &bestFilt, float *smoothDist,
                                  float *rawDist, float *resMean, float *predMean,
                                  float *meanResMax, float *maxResMax, float *meanRawMax,
                                  float *maxRawMax, float *evenSum, float *oddSum)
{
  int ind, numPix, frame, maxAsBest, filt, useFilt, iter, ierr, useFrame, numAlign,binInd;
  float shiftX, shiftY, error, minError, maxRefine;
  FloatVec refXshift, refYshift, cumXshift, cumYshift, groupXshift, groupYshift;
  float *realSum = mFullEvenSum;
  float *evenSource = mFullEvenSum;
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
        error = (1.f - mMaxMaxWeight) * mPredMeanSum[ind] / B3DMAX(1, mNumFits) +
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
        if (processFull && addToSums(NULL, ind, -9, ind))
          return 3;

      }
    } else {

      // Or adjust and add in ALL the shifts this time and add the images
      for (filt = 0; filt <= mNumFilters; filt++) {
        useFilt = filt == mNumFilters ? mBestFilt : filt;
        adjustAndPushShifts(mNumAllVsAll - 1, filt, useFilt);
      }
      for (ind = 1; ind < numAVAforFrames; ind++)
        if (processFull && addToSums(NULL, ind - 1, -9,
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
    START_TIMER;
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
      if (recoverGpuAlignFFTs(false, -1, NULL, NULL, NULL, false))
        return 3;
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
          ierr = addToSums(NULL, -9, frame, frame, useFilt);
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
              if (recoverGpuAlignFFTs(false, -1, mAlignSum, NULL, NULL, false))
                return 3;
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
            if (recoverGpuAlignFFTs(false, -1, mAlignSum, NULL, NULL, false))
              return 3;
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
    ADD_TIME(wallRefine);
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
        sFgpuSetUnpaddedSize(mNx, mNy, mFlagsForUnpadCall,
                             (mDebug ? 1 : 0) + (mReportTimes ? 10 : 0));
        if (sFgpuSetupSumming(mFullXpad, mFullYpad, mSumXpad, mSumYpad,
                              mEvenOddForSumSetup)) {
          if (recoverFromSummingFailure(NULL, 0, 0))
            return 3;
        }
      }
      for (frame = 0; frame < mNumFrames; frame++)
        if (addToSums(NULL, frame, -9, frame, useFilt))
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
    if (!mGpuSumming || !mGpuFlags) {

      // If doing even and odd, copy the even FFT to output buffer before it is added to
      if (evenSum && oddSum) {
        evenSource = evenSum;
        for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
          evenSum[ind] = mFullEvenSum[ind];
      }
      for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
        mFullEvenSum[ind] += mFullOddSum[ind];
      START_TIMER;
      todfftc(mFullEvenSum, mSumXpad, mSumYpad, 1);
      ADD_TIME(mWallFullFFT);
    }
    extractWithBinning(realSum, MRC_MODE_FLOAT, mSumXpad + 2, xStart, xEnd, yStart,
                       yEnd, 1, alisum, 0, &nxBin, &nyBin);

    // Do the even and odd sums if requested
    if (evenSum && oddSum) {
      todfftc(evenSource, mSumXpad, mSumYpad, 1);
      todfftc(mFullOddSum, mSumXpad, mSumYpad, 1);
      extractWithBinning(evenSource, MRC_MODE_FLOAT, mSumXpad + 2, xStart, xEnd, yStart,
                         yEnd, 1, evenSum, 0, &nxBin, &nyBin);
      extractWithBinning(mFullOddSum, MRC_MODE_FLOAT, mSumXpad + 2, xStart, xEnd, yStart,
                         yEnd, 1, oddSum, 0, &nxBin, &nyBin);
    }
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
    predMean[filt] = mPredMeanSum[filt] / ind;
    meanResMax[filt] = mResMaxSum[filt] / ind;
    maxResMax[filt] = mMaxResMax[filt];
    meanRawMax[filt] = mRawMaxSum[filt] / ind;
    maxRawMax[filt] = mMaxRawMax[filt];
    mPredMeanSum[filt] /= ind;
  }
  if (mReportTimes)
    utilPrint("FullFFT %.3f  BinPad %.3f  BinFFT %.3f  Reduce %.3f  Shift %.3f Filt "
              "%.3f\nConjProd %.3f   PreProc %.3f  Noise %.3f  Sum of those %.3f "
              " Refine %.3f\n", mWallFullFFT, mWallBinPad, mWallBinFFT, mWallReduce,
              mWallShift, mWallFilter, mWallConjProd, mWallPreProc, mWallNoise,
              mWallFullFFT + mWallBinPad + mWallBinFFT + mWallReduce + mWallShift +
              mWallFilter + mWallConjProd + mWallPreProc + mWallNoise, wallRefine);
  if (mGpuFlags && mReportTimes)
    sFgpuPrintTimers();
  return 0;
}

/*
 * Get the unweighted sum that was made in tandem with the dose-weighted one
 */
int FrameAlign::getUnweightedSum(float *nonDWsum)
{
  int nxBin = mNx / mBinSum;
  int nyBin = mNy / mBinSum;
  int xStart = (mSumXpad - nxBin) / 2;
  int xEnd = xStart + nxBin - 1;
  int yStart = (mSumYpad - nyBin) / 2;
  int yEnd = yStart + nyBin - 1;
  if (!mMakeUnwgtSum || (mMakeUnwgtSum && mUnwgtOnGpu && !mGpuSumming)) {
    utilPrint("There is no unweighted sum available");
    return 1;
  }
  if (mUnwgtOnGpu) {
    if (sFgpuReturnUnweightedSum(mUnweightSum))
      return 1;
  } else {

    START_TIMER;
    todfftc(mUnweightSum, mSumXpad, mSumYpad, 1);
    ADD_TIME(mWallFullFFT);
  }

  extractWithBinning(mUnweightSum, MRC_MODE_FLOAT, mSumXpad + 2, xStart, xEnd, yStart,
                     yEnd, 1, nonDWsum, 0, &nxBin, &nyBin);
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
        if (recoverGpuAlignFFTs(false, -1, refInd == -1 ? mAlignSum : NULL,
                                (refInd < -1 || aliInd < 0) ? mWorkBinPad : NULL, NULL,
                                false))
          return 3;
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
      START_TIMER;
      conjugateProduct(mCorrBinPad, refArr,  mAlignXpad, mAlignYpad);
      ADD_TIME(mWallConjProd);

      // Inverse FFT
      START_TIMER;
      todfftc(mCorrBinPad, mAlignXpad, mAlignYpad, 1);
      ADD_TIME(mWallBinFFT);
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
    START_TIMER;
    memcpy(mWrapTemp, mTempSubFilt, (aliXsize + 2) * aliYsize * sizeof(float));
    todfftc(mWrapTemp, aliXsize, aliYsize, 0);
    for (ind = 0; ind < (aliXsize + 2) * aliYsize; ind++)
      mWrapTemp[ind] = mWrapTemp[ind] * mSubFiltMask[filtInd][ind];
    todfftc(mWrapTemp, aliXsize, aliYsize, 1);
    wrapImage(mWrapTemp, aliXsize + 2, aliXsize, aliYsize, corrTemp, aliXsize + 2,
              aliXsize, aliYsize, 0, 0);
    ADD_TIME(mWallFilter);
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

/* CALLING USAGE:
   Nextframe:
   calls with first one when first numAVA is done
   calls with second one on stack that first time and later times
   But the stack is rolled by 1 only
   call with fullArr when SummingMode < 0 (just summing)
   For cumulative align, if not deferring, calls with fullArr and binInd
   finishAlignAndSum:
   If no deferred sum and no frames done, calls with each saved frame on stack
   If > numAVA frames done, calls with each frame from 1 to numAVA - 1
   If refining, calls with NULL and frame number to add up bin-pads
   Calls with every frame on stack for any deferred summing
*/

/*
 * Shift and add image to full sum, and bin/pad image to cumulative alignment sum if
 * binInd >= -1
 * sumInd is >= 0 if it comes from stack (so can be a frame number), -1 to use fullArr
 * for a full frame, or < -1 for no full frame sum
 * Here binInd >= 0 for a saved frame, -1 for mWorkBinPad, < -1 for nothing
 * frameNum is the absolute frame number
 */
int FrameAlign::addToSums(float *fullArr, int sumInd, int binInd, int frameNum,
                          int filtInd)
{
  int ind;
  float *binArr;
  if (filtInd < 0)
    filtInd = mNumFilters;
  float xShift, yShift;
  float *fullSum = (frameNum % 2) ? mFullOddSum : mFullEvenSum;
  frameShiftFromGroups(frameNum, filtInd, xShift, yShift);
  if (sumInd == -1 && !fullArr) {
    utilPrint("Program error in addToSums: sumInd = -1 and fullArr is NULL\n");
    return 1;
  }

  // Shift full image and add into final sum if one is passed
  if (sumInd >= -1) {

    // Get a full-sized dose-weight filter regardless of binning because that is needed
    // for the GPU case
    if (mDoingDoseWeighting) {
      doseWeightFilter(mPriorDoseCum, mPriorDoseCum + mFrameDoses[frameNum], mPixelSize,
                       mCritDoseAfac, mCritDoseBfac, mCritDoseCfac, mCritDoseScale,
                       &mDoseWgtFilter[0], (int)mDoseWgtFilter.size(), 0.71f, &mDWFdelta);
      if (mReweightFilt.size()) {
        for (ind = 0; ind < (int)mDoseWgtFilter.size(); ind++)
          mDoseWgtFilter[ind] *= mReweightFilt[ind];
      }
      if (mDebug > 1) {
        utilPrint("1/pixel  Attenuation   Dose weight filter for frame %d:\n", frameNum);
        for (ind = 0; ind < (int)mDoseWgtFilter.size(); ind += (int)mDoseWgtFilter.size()
               / 35) {
          utilPrint("%.4f  %.4f\n", ind, mDWFdelta * ind, mDoseWgtFilter[ind]);
          if (!mDoseWgtFilter[ind])
            break;
        }
      }
      mPriorDoseCum += mFrameDoses[frameNum];
    }

    // Replace fullArr if it is indeed the first one on the stack here, only test this
    // if there was local stacking at all, i.e. no GPU summing or not stacking or
    // stacking but with a limit
    if (sumInd >= 0 && mSavedFullFrameNum.size() > 0 &&
        (!mGpuSumming || !mStackUnpadOnGpu || (mStackUnpadOnGpu && mGpuStackLimit > 0))) {
      if (mSavedFullFrameNum[0] == frameNum) {
        fullArr = mSavedFullSize[0];
      } else if (!mGpuSumming || !mStackUnpadOnGpu) {
        utilPrint("Next frame to be summed is not the first on the saved memory stack\n");
        return 1;
      }
    }

    //dumpFFT(fullArr, mFullXpad, mFullYpad, "full pad to sum", 1);
    // Try to do sum on GPU if flag set
    if (mGpuSumming) {
      ind = (int)mDoseWgtFilter.size();
      if (sFgpuSetupDoseWeighting(ind > 0 ? &mDoseWgtFilter[0] : NULL, ind, mDWFdelta)
          || sFgpuAddToFullSum(fullArr, xShift, yShift)) {
        if (recoverFromSummingFailure(&fullArr, frameNum, sumInd))
          return 3;
      }
    }

    // Do sum into arrays here
    if (mBinSum > 1 && !mGpuSumming) {
      START_TIMER;
      fourierReduceImage(fullArr, mFullXpad, mFullYpad, mReduceTemp, mSumXpad, mSumYpad,
                         xShift, yShift, mShiftTemp);
      ADD_TIME(mWallReduce);
      START_TIMER;

      // Just scale the delta by the binning to use the initial part of the filter
      // on already-reduced images
      if (mDoingDoseWeighting) {
        filterAndAddToSum(mReduceTemp, fullSum, mSumXpad, mSumYpad, &mDoseWgtFilter[0],
                          mDWFdelta * mBinSum);
        if (mMakeUnwgtSum && !mUnwgtOnGpu) {
          for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
            mUnweightSum[ind] += mReduceTemp[ind];
        }
        //utilDumpFFT(mReduceTemp, mSumXpad, mSumYpad, "reduced", 0, frameNum);
      } else {
        for (ind = 0; ind < (mSumXpad + 2) * mSumYpad; ind++)
          fullSum[ind] += mReduceTemp[ind];
      }
      ADD_TIME(mWallFilter);
    } else if (!mGpuSumming) {
      START_TIMER;
      fourierShiftImage(fullArr, mFullXpad, mFullYpad, xShift, yShift , mShiftTemp);
      ADD_TIME(mWallShift);
      START_TIMER;
      if (mDoingDoseWeighting) {
        filterAndAddToSum(fullArr, fullSum, mFullXpad, mFullYpad, &mDoseWgtFilter[0],
                          mDWFdelta);
        if (mMakeUnwgtSum && !mUnwgtOnGpu) {
        for (ind = 0; ind < (mFullXpad + 2) * mFullYpad; ind++)
          mUnweightSum[ind] += fullArr[ind];
      }
        //utilDumpFFT(fullArr, mFullXpad, mFullYpad, "shifted", 0, frameNum);
      } else {
        for (ind = 0; ind < (mFullXpad + 2) * mFullYpad; ind++)
          fullSum[ind] += fullArr[ind];
      }
      ADD_TIME(mWallFilter);
      //utilDumpFFT(fullSum, mFullXpad, mFullYpad, "cur sum", 1, mNumFrames);
    }
  }

  // Roll the frame buffer and reduce the number saved
  if (sumInd >= -1 && (mNumAllVsAll || mDeferSumming)) {
    if (fullArr && mNumFullSaved > 0) {
      /*for (int jnd = 0; jnd < mNumFullSaved; jnd++)
        printf("%d  ", mSavedFullFrameNum[jnd]);
        printf("\n");*/
      utilRollSavedFrames(mSavedFullSize, mNumFullSaved);
      mNumFullSaved--;
      for (ind = 0; ind < mNumFullSaved; ind++)
        mSavedFullFrameNum[ind] = mSavedFullFrameNum[ind + 1];
      //PRINT3(mSavedFullFrameNum[0], mNumFullSaved, mSavedFullFrameNum[mNumFullSaved-1]);
      /*for (int jnd = 0; jnd < mNumFullSaved; jnd++)
        printf("%d  ", mSavedFullFrameNum[jnd]);
        printf("\n");*/
    } else if (!fullArr && mNumStackedOnGpu > 0) {
      mNumStackedOnGpu--;
    }
  }

  // If there is a legal binInd, shift it and add to align sum

  if (binInd < -1)
    return 0;

  if (mGpuAligning) {

    // Shift and add: but don't bother shifting the source when doing simple cum corr
    // An error is fatal when shifting in place since we have no idea if it happened,
    // and we also don't know the state of the align sum
    if (sFgpuShiftAddToAlignSum(binInd, xShift / mBinAlign, yShift / mBinAlign,
                                binInd < 0 ? 0 : 1))
      return 3;

  } else {
    START_TIMER;
    if (binInd >= (int)mSavedBinPad.size())
      return 2;
    binArr = binInd < 0 ? mWorkBinPad : mSavedBinPad[binInd];
    fourierShiftImage(binArr, mAlignXpad, mAlignYpad, xShift / mBinAlign,
                      yShift / mBinAlign, mShiftTemp);
    for (ind = 0; ind < mAlignPix; ind++)
      mAlignSum[ind] += binArr[ind];
    ADD_TIME(mWallShift);
  }
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
 * Higher level routine to recover align FFTs will also recover the full stack if there
 * is noise padding, and take FFTs of individual frames when grouping with no refine at
 * end
 */
int FrameAlign::recoverGpuAlignFFTs(bool saving, int aliFrameInd, float *alignSum,
                                    float *workArr, float **binArr, bool stacking)
{
  int ind, ix;
  float *tempBin;

  // Fix up the full stack if doing noise pad on GPU and cancel all initial steps there
  if (mNoisePadOnGpu) {
    if (recoverGpuFullStack(stacking, NULL)) {
      cleanup();
      return 3;
    }
  }
  cancelInitialStepsOnGPU();

  // Get the arrays made up
  if (prepareToFetchAlignFFTs(saving ? aliFrameInd : -1))
    return 3;

  // Get the data back
  if (sFgpuReturnAlignFFTs(mSavedBinPad.size() ? &mSavedBinPad[0] : NULL,
                           mSavedGroups.size() ? &mSavedGroups[0] : NULL,
                           alignSum, workArr)) {
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

  // Take care of current align array if provided: put it on the stack
  if (saving && binArr) {
    tempBin = mSavedBinPad[aliFrameInd];
    memcpy(tempBin, *binArr, mAlignBytes);
    *binArr = tempBin;
  }
  utilPrint("Switching to aligning with the CPU\n");
  return 0;
}

/*
 * Get back the stack of full-sized images from the GPU and get larger arrays for ones on
 * the CPU stack if necessary; preprocess if needed, and noise pad them.
 */
int FrameAlign::recoverGpuFullStack(bool stacking, float **binArr)
{
  int numSavedStart = mNumFullSaved;
  int ind, jnd, frameNum, sourceType;
  float *destArr, *sourceArr;
  bool needPreprocess = (mGainRef || mTruncLimit > 0. || mCamSizeX > 0) &&
    (mGpuFlags & GPU_DO_PREPROCESS);
  float *allocedArr = NULL;

  // Loop on frames in CPU stack then ones in GPU stack, then maybe the one being stacked
  for (ind = 0; ind < numSavedStart + mNumStackedOnGpu + (stacking ? 1 : 0); ind++) {

    // Allocate an array if stack is not floats or stack not big enough
    if (mStackType != MRC_MODE_FLOAT || ind >= (int)mSavedFullSize.size()) {
      allocedArr = B3DMALLOC(float, (mFullXpad + 2) * mFullYpad);
      if (!allocedArr)
        return 1;
      destArr = allocedArr;
    } else {

      // Otherwise point final array to stack buffer
      destArr = mSavedFullSize[ind];
    }

    // Set source as stack if on CPU and save frame number
    if (ind < numSavedStart) {
      sourceArr = mSavedFullSize[ind];
      frameNum = mSavedFullFrameNum[ind];
    } else if (ind < numSavedStart + mNumStackedOnGpu) {

      // Or fetch into the allocated array
      sourceArr = allocedArr;
      if (sFgpuReturnStackedFrame(sourceArr, &frameNum)) {
        free(allocedArr);
        return 1;
      }
      mNumFullSaved++;
    } else {

      // Or use the passed array for current frame
      sourceArr = *binArr;
      frameNum = mNumFrames;
      *binArr = destArr;
      mNumFullSaved++;
    }
    sourceType = mStackType;

    // Preprocess into work array if needed
    if (needPreprocess) {
      preProcessFrame(sourceArr, NULL, mDefectBin, mWorkFullSize);
      sourceType = MRC_MODE_FLOAT;
      sourceArr = mWorkFullSize;
    }

    // Noise taper pad into the destination array
    sliceNoiseTaperPad(sourceArr, sourceType, mNx, mNy, destArr, mFullXpad + 2, mFullXpad,
                       mFullYpad, mNoiseLength, 4, mShiftTemp);

    // Replace array on stack if alloced and < size of stack
    if (ind < (int)mSavedFullSize.size()) {
      if (allocedArr) {
        free(mSavedFullSize[ind]);
        mSavedFullSize[ind] = allocedArr;
        mSavedFullFrameNum[ind] = frameNum;
      }
    } else {

      // Otherwise push onto stack
      mSavedFullSize.push_back(allocedArr);
      mSavedFullFrameNum.push_back(frameNum);
    }
    allocedArr = NULL;
  }

  // Sort the stack by frame number
  for (ind = 0; ind < mNumFullSaved - 1; ind++) {
    for (jnd = ind + 1; jnd < mNumFullSaved; jnd++) {
      if (mSavedFullFrameNum[jnd] < mSavedFullFrameNum[ind]) {
        destArr = mSavedFullSize[ind];
        mSavedFullSize[ind] = mSavedFullSize[jnd];
        mSavedFullSize[jnd] = destArr;
        frameNum = mSavedFullFrameNum[ind];
        mSavedFullFrameNum[ind] = mSavedFullFrameNum[jnd];
        mSavedFullFrameNum[jnd] = frameNum;
      }
    }
  }
  mNumStackedOnGpu = 0;
  return 0;
}

/*
 * Turn off all the flags that make it think preprocessing, bin/pad or noise/pad are on
 * GPU
 */
void FrameAlign::cancelInitialStepsOnGPU()
{
  if (mNoisePadOnGpu || mBinPadOnGpu)
    utilPrint("Switching to preprocessing and other initial steps on CPU\n");
  mNoisePadOnGpu = mBinPadOnGpu = false;
  mStackUnpadOnGpu = false;
  mGpuFlags &= ~(GPU_DO_NOISE_TAPER | GPU_DO_BIN_PAD | STACK_FULL_ON_GPU |
                 GPU_DO_PREPROCESS);
  mFlagsForUnpadCall &= ~(GPU_DO_NOISE_TAPER | GPU_DO_BIN_PAD | STACK_FULL_ON_GPU |
                          GPU_DO_PREPROCESS);
  sFgpuSetUnpaddedSize(mNx, mNy, 0, (mDebug ? 1 : 0) + (mReportTimes ? 10 : 0));
}

/*
 * Higher-level return for recovering when deferred summing setup OR adding to sums
 * fails; pass the current fullArr address which may be modified, or NULL during setup,
 * pass the absolute frame number frameNum during summing or 0 in setup, pass the sumInd
 * when summing.  Does cleanup on failures
 */
int FrameAlign::recoverFromSummingFailure(float **fullArr, int frameNum, int sumInd)
{
  int ind;

  // Recover by getting existing sum back and taking FFT of current array
  utilPrint("Switching to summing on CPU\n");
  if (mNoisePadOnGpu) {
    if (recoverGpuFullStack(false, NULL)) {
      cleanup();
      return 3;
    }
    if (mSavedFullFrameNum[0] == frameNum) {
      if (fullArr)
        *fullArr = mSavedFullSize[0];
    } else {
      utilPrint("After recovering from GPU failure, frame to be summed is not "
                "first in stack\n");
      cleanup();
      return 3;
    }
  }
  cancelInitialStepsOnGPU();
  if (frameNum > 0) {
    if (sFgpuReturnSums(mFullEvenSum, mFullEvenSum, mFullOddSum, 1)) {
      cleanup();    // It didn't cleanup before
      return 3;
    }
  } else {
    memset(mFullEvenSum, 0, (mSumXpad + 2) * mSumYpad);
    memset(mFullOddSum, 0, (mSumXpad + 2) * mSumYpad);
  }
  //utilDumpFFT(mFullEvenSum, mFullXpad, mFullYpad, "recovered even", 1, mNumFrames);
  //utilDumpFFT(mFullOddSum, mFullXpad, mFullYpad, "recovered odd", 1, mNumFrames);
  mGpuSumming = false;
  if (sumInd < 0 && fullArr)
    todfftc(*fullArr, mFullXpad, mFullYpad, 0);
  for (ind = 0; ind < mNumFullSaved; ind++)
    todfftc(mSavedFullSize[ind], mFullXpad, mFullYpad, 0);
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

/*
 * Apply filter to image and add it to the sum; this is a simplified form of
 * XcorrFilterPart and saves the time of zeroing regions where the filter is zero, plus
 * saving the second pass through to add to sum
 */
void FrameAlign::filterAndAddToSum(float *fft, float *array, int nx, int ny, float *ctf,
                                   float delta)
{
  float x = 0., delx, dely, y = 0., s = 0., maxFreq;
  double ysq = 0.;
  int ix, iy, index = 0, ind = 0, indp1 = 0, indf = 0, nxDiv2, nxDiv2p1, nyMinus1;
  int nxMax;
  int numThreads, maxThreads = 16;

  nxDiv2 = nx / 2;
  nxDiv2p1 = nxDiv2 + 1;
  nyMinus1 = ny - 1;
  delx = (float)(1.0 / nx);
  dely = (float)(1.0 / ny);

  /* Find last non-zero filter value in range that matters */
  for (ix = (int)(0.707 / delta); ix > 1; ix--)
    if (ctf[ix])
      break;

  /* Get a frequency limit to apply in Y and a limit to X indexes */
  maxFreq = (ix + 1) * delta;
  nxMax = (int)(maxFreq / delx) + 1;
  B3DCLAMP(nxMax, 1, nxDiv2);

  /* This formula give 1.5+ at 128 and 11.5+ at 4096 */
  numThreads = B3DNINT(3.33 * (log10((double)nx * ny) - 3.75));
  B3DCLAMP(numThreads, 1, maxThreads);
  numThreads = numOMPthreads(numThreads);

  /*   apply filter function on fft, put result in array */
#pragma omp parallel for num_threads(numThreads)                        \
  shared(nyMinus1, dely, maxFreq, nxDiv2, array, nxMax, delta, fft, nxDiv2p1) \
  private(iy, y, ix, ind, index, ysq, x, indp1, s, indf)
  for (iy = 0; iy <= nyMinus1; iy++) {
    y = iy * dely;
    index = iy * nxDiv2p1;
    if (y > 0.5)
      y = 1.0f - y;
    if (y > maxFreq)
      continue;
    ysq = y * y;
    x = 0.0;
    for (ix = 0; ix <= nxMax; ix++) {
      ind = 2 * (index + ix);
      indp1 = ind + 1;
      s = (float)sqrt(x * x + ysq);
      indf = (int)(s / delta + 0.5f);
      array[ind] += fft[ind] * ctf[indf];
      array[indp1] += fft[indp1] * ctf[indf];
      x = x + delx;
    }
  }
}

/////////////////////////////////////
//  Routines for evaluating memory and capabilities allowed by it, needed in alignframes
//  and SerialEM.  SerialEM does not instantiate the class for K2, so these are statics
/////////////////////////////////////

/*
 * Get sizes in bytes for full, sum, and align images
 */
void FrameAlign::getPadSizesBytes(int nx, int ny, float fullTaperFrac, int sumBin,
                                  int alignBin, float &fullPadSize, float &sumPadSize,
                                  float &alignPadSize)
{
  fullPadSize = 4.f * (1.f + 2.f * fullTaperFrac) * (1.f + 2.f * fullTaperFrac) * nx * ny;
  sumPadSize = fullPadSize / (sumBin * sumBin);
  alignPadSize = (float)(4. * nx * ny) / ((float)alignBin * alignBin);
}

/*
 * Return the memory needed for aligning and summing on the GPU based on the conditions,
 * in bytes
 */
void FrameAlign::gpuMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize,
                                int numAllVsAll, int nzAlign, int refineAtEnd,
                                int groupSize, float &needForGpuSum, float &needForGpuAli)
{
  int numHoldAlign;

  // The FFTs require TWICE as much memory as the image, so allow for the biggest
  needForGpuSum = fullPadSize + sumPadSize + 2.f * B3DMAX(fullPadSize, sumPadSize);

  // Get needs for aligning on GPU
  // Simple cumulative sum needs all 5 arrays and FFT work area
  // Refining needs all arrays too plus one for each frame
  // Otherwise only 3 arrays and FFT and number of rolling frames
  if (!numAllVsAll && !refineAtEnd)
    numHoldAlign = 6;
  else if (refineAtEnd)
    numHoldAlign = 6 + nzAlign;
  else
    numHoldAlign = 4 + B3DMIN(numAllVsAll, nzAlign);
  if (groupSize > 1) {

    // If doing groups, refinement requires space for the group sums, and
    // no refinement requires space for groupSize single frames
    if (refineAtEnd)
      numHoldAlign += B3DMIN(numAllVsAll, nzAlign);
    else
      numHoldAlign += groupSize;
  }
  needForGpuAli = numHoldAlign * alignPadSize;
}

/*
 * Return the computer memory needed based on the conditions, in gigabytes, and set
 * flag for whether sums can be made in one pass, and return maximum number of full frames
 * that need to be held in memory somewhere
 */
float FrameAlign::totalMemoryNeeds(float fullPadSize, int fullDataSize, float sumPadSize,
                                   float alignPadSize, int numAllVsAll, int nzAlign,
                                   int refineAtEnd, int numBinTests, int numFiltTests,
                                   int hybridShifts, int groupSize, int doSpline,
                                   int gpuFlags, int deferSum, int testMode,
                                   int startAssess, bool &sumInOnePass, int &numHoldFull)
{
  int numHoldAlign, stackLimit;
  float memTot;

  // Make sums as you go if there is one binning and no subset assessment
  sumInOnePass = numBinTests == 1 && !testMode && startAssess < 0;

  // Initialize each hold to numAVA then adjust align holding needed
  numHoldFull = numHoldAlign = B3DMIN(numAllVsAll, nzAlign);
  if (numAllVsAll) {
    if (refineAtEnd)
      numHoldAlign = nzAlign;
    if (groupSize > 1 && refineAtEnd)
      numHoldAlign += B3DMIN(numAllVsAll, nzAlign);
    else if (groupSize > 1)
      numHoldAlign += groupSize;
  } else if (refineAtEnd) {
    numHoldAlign = nzAlign;
  }
  if (gpuFlags & GPU_FOR_ALIGNING)
    numHoldAlign = 0;

  // Adjust full holding needed and reduce by GPU stack if any
  if ((!hybridShifts && numFiltTests > 1 && sumInOnePass && numAllVsAll) ||
      refineAtEnd || deferSum || doSpline)
    numHoldFull = nzAlign;
  if (gpuFlags & STACK_FULL_ON_GPU) {
    if (gpuFlags & GPU_STACK_LIMITED) {
      stackLimit = (gpuFlags >> GPU_STACK_LIM_SHIFT) & GPU_STACK_LIM_MASK;
      numHoldFull = B3DMAX(0, numHoldFull - stackLimit);
    } else {
      numHoldFull = 0;
    }
  }
  if ((numBinTests > 1 || testMode) && numAllVsAll && startAssess < 0)
    numHoldFull = 0;
  memTot = (float)((2. * sumPadSize + (numHoldAlign + 4.) * alignPadSize + fullPadSize +
            numHoldFull * fullDataSize * fullPadSize / 4.) / (1024. * 1024. * 1024.));
  return memTot;
}

/*
 * Given the parameters for what processing is needed and how many frames need to be
 * held in a stack, try to add noise pad then bin pad on the GPU, trying first with
 * preprocessing on GPU because that is a preferable endpoint and allows a smaller stack
 * Memory is in bytes for GPU
 */
float FrameAlign::findPreprocPadGpuFlags(int unpaddedX, int unpaddedY, int dataSize,
                                         int binning, bool hasGain, bool hasDefect,
                                         bool hasTrunc, int numExpected, float freeMem,
                                         float stackMargin, int inFlags, int &outFlags)
{
  bool needsProc = hasGain || hasDefect || hasTrunc;
  bool doAlign = (inFlags & GPU_FOR_ALIGNING) != 0;
  bool doSum = (inFlags & GPU_FOR_SUMMING) != 0;
  int passSize, maxAdded;
  int stackLimit = 0;
  float tempNeeds, unpadBytes;
  int preprocFlags = GPU_DO_PREPROCESS | (hasGain ? GPU_DO_GAIN_NORM : 0) |
    (hasDefect ? GPU_CORRECT_DEFECTS : 0);
  outFlags = inFlags;

  if (doAlign && !doSum) {

    // Align only is separate
    if (!needsProc || !preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning,
                                               hasGain, hasDefect, hasTrunc, true, false,
                                               true, freeMem, tempNeeds)) {
      // No proc or proc does not fit, evaluate just binPad
      if (preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning,
                                  hasGain, hasDefect, hasTrunc, false, false,
                                  true, freeMem, tempNeeds)) {
        outFlags |= GPU_DO_BIN_PAD;
        return tempNeeds;
      }
      return 0.;
    } else {

      // Proc and binpad fit
      outFlags |= GPU_DO_BIN_PAD | preprocFlags;
        return tempNeeds;
    }
  }

  // Align and sum or sum only: first evaluate noise and preproc
  if (!needsProc || !preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning,
                                             hasGain, hasDefect, hasTrunc, true, true,
                                             false, freeMem, tempNeeds)) {

    // No proc, or proc and noise do not fit, evaluate just noise
    if (preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning, hasGain,
                                hasDefect, hasTrunc, false, true, false, freeMem,
                                tempNeeds)) {

      // Noise fits without preproc, try binpad too
      if (doAlign && preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning,
                                             hasGain, hasDefect, hasTrunc, false, true,
                                             true, freeMem, tempNeeds)) {

        // Noise and bin pad fit, set flags and fall through to stack testing
        outFlags |= GPU_DO_BIN_PAD | GPU_DO_NOISE_TAPER;
      } else {

        // Only noise fits or is needed, no bin pad or preproc
        outFlags |= GPU_DO_NOISE_TAPER;
        return tempNeeds;
      }
    } else {

      // Even noise does not fit
      return 0.;
    }
  } else {

    // Preproc and noise fit, evaluate binpad
    if (doAlign && preprocPadGpuMemoryFits(unpaddedX, unpaddedY, dataSize, binning,
                                           hasGain, hasDefect, hasTrunc, true, true,
                                           true, freeMem, tempNeeds)) {

      // Proc and binpad fit with noise, do it all and fall through to stack
      outFlags |= GPU_DO_BIN_PAD | GPU_DO_NOISE_TAPER | preprocFlags;
    } else {

      // Preproc and only noise fit or are needed, set flags
      outFlags |= GPU_DO_NOISE_TAPER | preprocFlags;
      return tempNeeds;
    }
  }

  // Now see if a stack can fit and how much
  // One frame on stack replaces the temp raw array, hence compute added number amd add 1
  passSize = (needsProc && !(outFlags & preprocFlags)) ? sizeof(float) : dataSize;
  unpadBytes = (float)(passSize * unpaddedX * unpaddedY);
  maxAdded = (int)((freeMem - tempNeeds - stackMargin) / unpadBytes);
  if (maxAdded >= numExpected - 1) {
    tempNeeds += (numExpected - 1) * unpadBytes;
    outFlags |= STACK_FULL_ON_GPU;
  } else if (maxAdded > 0) {
    stackLimit = maxAdded + 1;
    tempNeeds += (float)maxAdded * unpadBytes;
    outFlags |= STACK_FULL_ON_GPU | GPU_STACK_LIMITED |
      (stackLimit << GPU_STACK_LIM_SHIFT);
  }
  return tempNeeds;
}

/*
 * For one set of possible operations on GPU, determine if the needed memory fits
 * within freeMem, return needed amount in bytes in needsMem
 */
bool FrameAlign::preprocPadGpuMemoryFits(int unpaddedX, int unpaddedY, int dataSize,
                                         int binning, bool hasGain, bool hasDefect,
                                         bool hasTrunc, bool doPreProc, bool doNoise,
                                         bool doBinPad, float freeMem, float &needsMem)
{
  bool needsProc = hasGain || hasDefect || hasTrunc;
  int passSize = (needsProc && !doPreProc) ? sizeof(float) : dataSize;
  float unpadBytes = (float)(unpaddedX * unpaddedY);
  needsMem = 0.;

  // A gain reference needs a float array, defects need a byte array, noise or bin pad
  // needs the raw temp array if not stacking, binning needs the reduced in X float array
  if (doPreProc && hasGain)
    needsMem += 4.f * unpadBytes;
  if (doPreProc && hasDefect)
    needsMem += unpadBytes;
  if (doNoise || doBinPad)
    needsMem += (float)passSize * unpadBytes;
  if (doBinPad && binning > 1)
    needsMem += 4.f * unpadBytes / (float)binning;
  return needsMem < freeMem;
}

/*
 * Set the truncation limit to use: pass it on if >= 0, or find mean/sd and get threshold
 * if it is negative # of SD's.  This is here for convenience of framealign callers
 * Returns 1 for error making line pointers, or 2 for error getting sample arrays
 */
int FrameAlign::setTruncationLimit(void *array, int nx, int ny, int useMode,
                                   float truncLimit, float &truncUse)
{
  int err, type = typeForSampleMean(useMode);
  int dataSize, temp;
  int numSample = 20000;
  unsigned char **linePtrs;
  float sample, mean, sd;
  int trim = B3DMIN(nx, ny) / 20;
  int nxUse = nx - 2 * trim;
  int nyUse = ny - 2 * trim;
  truncUse = truncLimit;
  if (truncLimit >= 0.)
    return 0;
  dataSizeForMode(useMode, &dataSize, &temp);
  linePtrs = makeLinePointers(array, nx, ny, dataSize);
  if (!linePtrs)
    return 1;
  sample = B3DMIN(numSample, nxUse * nyUse) / (float)(nxUse * nyUse);
  err = sampleMeanSD(linePtrs, type, nx, ny, sample, trim, trim, nxUse, nyUse,
                     &mean, &sd);
  free(linePtrs);
  if (err)
    return 2;
  truncUse = mean - truncLimit * sd;
  return 0;
}
