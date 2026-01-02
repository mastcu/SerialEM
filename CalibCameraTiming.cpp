// CalibCameraTiming.cpp: implementation of the CCalibCameraTiming class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "CalibCameraTiming.h"
#include "CameraController.h"
#include "ProcessImage.h"
#include "ShiftManager.h"
#include "Shared\b3dutil.h"
#include "Utilities\KGetOne.h"
#include "Utilities\XCorr.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCalibCameraTiming::CCalibCameraTiming()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSet = mWinApp->GetConSets() + TRACK_CONSET;
  mImBufs = mWinApp->GetImBufs();
  mExtraBeamTime = 1.0f;
  mMinimumDrift = 0.05f;
  mDefRegExposure = 0.4f;
  mDefSTEMexposure = 1.2f;
  mInitialStartup = 0.01f;
  mK2InitialStartup = 2.f;
  mBuiltInDecrement = 0.1f;
  mCalIndex = -1;
  mNumRangeShots = 3;
  mHighQuickDelays = 3;
  mHighQuickShots = 3;
  mLowQuickDelays = 4;
  mLowQuickShots = 4;
  mLowHighThreshold = 5.f;
  mDefRegTestShots = 100;
  mDefSTEMTestShots = 30;
  mMeanValues = NULL;
  mDeadIndex = -1;
  mDeadStartExp = 0.04f;
  mDeadExpInc = 0.01f;
  mNumDeadExp = MAX_DEAD_TIMES;
  mFlybackArray.SetSize(0, 4);
  mNeedToSaveFlybacks = false;
}

CCalibCameraTiming::~CCalibCameraTiming()
{

}


// Start the calibration process
void CCalibCameraTiming::CalibrateTiming(int setNum, float exposure, bool confirmOK)
{
  int targetSize = 1024;
  int i, ind, chan;
  float flyback, startup, fullexp;
  static bool firstTime = true;
  CString report;
  ControlSet *trialSet = mWinApp->GetConSets() + TRIAL_CONSET;

  mCamera = mWinApp->mCamera;
  mCamParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  mSTEMcamera = mCamParam->STEMcamera ? 1 : 0;
  mQuickFlyback = setNum >= 0;
  mConfirmOK = confirmOK;

  // Save the current camera timing
  mMinDriftSave = mCamParam->minimumDrift;
  mStartupSave = mCamParam->startupDelay;
  mExtraTimeSave = mCamParam->extraBeamTime;
  mBuiltInSave = mCamParam->builtInSettling;
  mMinBlankSave = mCamera->GetMinBlankingTime();
  mNoShutterSave = mCamParam->noShutter;

  if (mQuickFlyback) {
    trialSet = mWinApp->GetConSets() + setNum;
    mConSet->left = trialSet->left;
    mConSet->right = trialSet->right;
    mConSet->top = trialSet->top;
    mConSet->bottom = trialSet->bottom;
    mExposure = exposure > 0. ? exposure : trialSet->exposure;
    mConSet->binning = trialSet->binning;

  } else {

    // Find biggest binning where both dimensions exceed target size
    for (i = mCamParam->numBinnings - 1; i >= 0; i--) {
      mConSet->binning = mCamParam->binnings[i];
      if (mCamParam->sizeX / mConSet->binning >= targetSize &&
        mCamParam->sizeY / mConSet->binning >= targetSize)
        break;
    }

    // Take full-sized image
    mConSet->left = 0;
    mConSet->right = mCamParam->sizeX;
    mConSet->top = 0;
    mConSet->bottom = mCamParam->sizeY;
  }

  // Set all other parameters for film shutter shots, no settling
  mConSet->drift = 0.;
  mConSet->forceDark = false;
  mConSet->onceDark = true;
  mConSet->mode = SINGLE_FRAME;
  mConSet->processing = GAIN_NORMALIZED;
  mConSet->dynamicFocus = false;
  mConSet->lineSyncOrPattern = false;
  mStartupOnly = !mCamParam->GatanCam || mCamParam->onlyOneShutter || mSTEMcamera ||
    mCamParam->noShutter || mCamParam->OneViewType;
  mConSet->shuttering = mStartupOnly ? USE_BEAM_BLANK : USE_FILM_SHUTTER;

  // For a STEM camera, get the first selected channel of the Trial set
  if (mSTEMcamera) {
    if (mCamParam->FEItype)
      mWinApp->mScope->GetFEIChannelList(mCamParam, true);
    for (chan = 0; chan < mCamParam->numChannels; chan++) {
      ind = trialSet->channelIndex[chan];
      if (ind >= 0 && ind < MAX_STEM_CHANNELS && mCamParam->availableChan[ind]) {
        mConSet->channelIndex[0] = ind;
        break;
      }
    }
    for (i = 1; i < MAX_STEM_CHANNELS; i++)
      mConSet->channelIndex[i] = -1;
  }

  // Set up number of shots to take in initial reference and in finding startup range
  mNumRefShots = mSTEMcamera ? 1 : 5;
  mNumRangeShots = mSTEMcamera ? 7 : 3;

  if (!mQuickFlyback) {
    mExposure = mSTEMcamera ? mDefSTEMexposure : mDefRegExposure;
    chan = mConSet->binning / BinDivisorI(mCamParam);
    report.Format("To calibrate timing, the beam should be set to give\nmoderately"
      " high counts (but not saturated exposures) with\nthe selected binning "
      "(default %d) and exposure time (default %.2f seconds)\n\nAre you ready to proceed?"
      , chan, mExposure);
    if (AfxMessageBox(report, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return;

    // Get parameters, check limits
    mNumTestShots = mSTEMcamera ? mDefSTEMTestShots : mDefRegTestShots;
    if ((mCamParam->K2Type == K2_SUMMIT || mCamParam->K2Type == K3_TYPE) && firstTime)
      mNumTestShots = (mNumTestShots * 4) / 10;
    firstTime = false;
    if (!KGetOneFloat("Exposure time for test images (sec):", mExposure, 2))
      return;

    if (!KGetOneInt("Binning:", chan))
      return;
    mConSet->binning = chan * BinDivisorI(mCamParam);
    if (!KGetOneInt("Number of test pictures:", mNumTestShots))
      return;
    if (mSTEMcamera) {
      mDefSTEMexposure = mExposure;
      mDefSTEMTestShots = mNumTestShots;
    } else {
      mDefRegExposure = mExposure;
      mDefRegTestShots = mNumTestShots;
    }

    // Set startup increment
    if (mExposure > 1.2)
      mStartupIncrement = 0.13333f * mExposure;
    else if (mSTEMcamera)
      mStartupIncrement = 0.16f;
    else
      mStartupIncrement = 0.08f;

  } else {

  // and set up the increments for the quick flyback
    mNumRangeShots = mExposure > mLowHighThreshold ? mHighQuickShots : mLowQuickShots;
    mNumQuickDelays = mExposure > mLowHighThreshold ? mHighQuickDelays : mLowQuickDelays;
    ind = FlybackTimeFromTable(mConSet->binning,
      (mConSet->right - mConSet->left) / mConSet->binning, mWinApp->mScope->GetMagIndex(),
      mExposure, flyback, startup);
    if (ind == FLYBACK_ERROR || ind == FLYBACK_NONE || ind == FLYBACK_SINGLE) {
      flyback = mCamParam->flyback;
      startup = mCamParam->startupDelay;
    }
    fullexp = mExposure + (float)(1.e-6 * flyback * (mConSet->bottom - mConSet->top) /
      mConSet->binning);
    mStartupIncrement = (float)(0.7 * fullexp / (mNumQuickDelays - 1));
    mFirstQuickStartup = startup + 0.15f * fullexp;
    SEMTrace('c', "Assuming flyback %f  first  %.2f  increment %.2f", flyback,
      mFirstQuickStartup, mStartupIncrement);
  }
  if (mNumTestShots <= mNumRefShots)
    mNumTestShots = mNumRefShots;
  if (mExposure < 0.1)
    mExposure = 0.1f;
  mConSet->exposure = mExposure;
  mConSet->binning = B3DMAX(mCamParam->binnings[0],
    B3DMIN(mCamParam->binnings[mCamParam->numBinnings-1], mConSet->binning));
  mConSet->doseFrac = trialSet->doseFrac;
  mConSet->K2ReadMode = (mCamParam->K2Type == K2_SUMMIT || mCamParam->K2Type == K3_TYPE) ?
    K2_SUPERRES_MODE : trialSet->K2ReadMode;
  mConSet->frameTime = trialSet->frameTime;
  mConSet->alignFrames = trialSet->alignFrames;
  mConSet->useFrameAlign = trialSet->useFrameAlign;
  mConSet->saveFrames = 0;
  mConSet->filtTypeOrPreset = trialSet->filtTypeOrPreset;

  // Assess blanking time
  mBlankCycleTime = 0.;
  if (!mQuickFlyback) {
    BOOL blanked = mWinApp->mScope->GetBlankSet();
    DWORD startBlank, startTime = GetTickCount();
    double partTime = 0.;
    for (i = 0; i < 10; i++) {
      startBlank = GetTickCount();
      mWinApp->mScope->BlankBeam(!blanked);
      partTime += SEMTickInterval((double)startBlank) / 10000.;
      mWinApp->mScope->BlankBeam(blanked);
    }
    mBlankCycleTime = (float)(SEMTickInterval((double)startTime) / 10000.);
    report.Format("Mean time for blanking beam = %.3f, for unblanking = %.3f, "
      "both = %.3f sec", B3DCHOICE(blanked, mBlankCycleTime - partTime, partTime),
      B3DCHOICE(blanked, partTime, mBlankCycleTime - partTime), mBlankCycleTime);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    if (mBlankCycleTime > 0.025) {
      mCamera->SetMinBlankingTime(mBlankCycleTime);
      mWinApp->AppendToLog("This time will be used while running this routine",
        LOG_OPEN_IF_CLOSED);
    }
  }

  // Get array to store values, start first shot
  i = B3DMAX(mNumTestShots, mNumRefShots);
  i = B3DMAX(i, mNumRangeShots);
  mMeanValues = new float[i + 4];
  mLineString = "";
  mCalIndex = 0;
  mDoingRefs = true;
  mRangeIndex = -1;
  mLowerRefRatio = -1.;
  mBlankMean = 0.;

  mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_CAM_TIMING, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "CALIBRATING CAMERA TIMING");
  mWinApp->UpdateBufferWindows();
  mWinApp->AppendToLog("Doing reference exposures:", LOG_OPEN_IF_CLOSED);
}


// Process an image
void CCalibCameraTiming::CalTimeNextTask()
{
  CString oneVal;
  int nx, ny, iy;
  KImage *image = mImBufs->mImage;
  float minCounts = 100000000.;
  float maxCounts = -10000000.;
  int numHigh = 0;
  int numLow = 0;
  FlybackTime fbTime;
  float meanRatio, intcp, slope, adjFactor, ypred, prederr, rostat, intcp2, slope2;
  float threshold, estStartup;
  double minDelay, line, maxDelay, newDelay, newMinDrift, delBIS = 0.;

  if (mCalIndex < 0)
    return;

  //SEMTrace('1', "CalTimeNextTask");
  // Get image mean, print raw value, then subtract blank mean
  mImBufs->mCaptured = BUFFER_CALIBRATION;
  mMeanValues[mCalIndex] = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
  oneVal.Format("%9.1f", mMeanValues[mCalIndex]);
  mMeanValues[mCalIndex] -= mBlankMean;
  meanRatio = 0.;
  if (!mDoingRefs) {
    meanRatio = mMeanValues[mCalIndex] / mMeanReference;

    // For STEM, look for first line above midpoint of intensity range
    if (mSTEMcamera) {
      nx = image->getWidth();
      ny = image->getHeight();
      if (mWinApp->mScope->GetSimulationMode()) {
        iy = (int)((mCamParam->startupDelay - mStartupSave) /
          (mExposure / ny + mCamParam->flyback / 1.e6));
        B3DCLAMP(iy, 1, ny - 1);
      } else {
        image->Lock();
        for (iy = 0; iy < ny; iy++) {
          line = ProcImageMean(image->getData(), image->getType(), nx, ny, 0, nx - 1, iy,
            iy);
          if (line > mBlankMean + 0.5 * mMeanReference)
            break;
        }
      }
      // Get ratio based on number of lines remaining and revise the actual mean
      meanRatio = (float)(ny - iy) / (float)ny;
      mMeanValues[mCalIndex] = mMeanReference * meanRatio;
      oneVal.Format("%9.1f", mMeanValues[mCalIndex]);
    }
  }

  // Add value to line string, output string if appropriate
  mLineString += oneVal;
  mCalIndex++;
  if (mCalIndex % 8 == 0 || (mCalIndex == mNumTestShots && !mDoingRefs &&
    mRangeIndex < 0) || (mCalIndex >= mNumRefShots && mDoingRefs) ||
    (mRangeIndex >= 0 && (mCalIndex == mNumRangeShots || meanRatio > 0.99))) {
    mWinApp->AppendToLog(mLineString, LOG_OPEN_IF_CLOSED);
    mLineString = "";
  }

  // If reference shots are done, get the mean, change the parameters, and go on to range
  if (mCalIndex == mNumRefShots && mDoingRefs) {
    mMeanReference = 0.;
    for (nx = 0; nx < mNumRefShots; nx++)
      mMeanReference += mMeanValues[nx] / mNumRefShots;

    // Or, for a STEM camera, get one blank image, record blank mean next time in
    if (mSTEMcamera)
      mCamera->SetBlankNextShot(true);
  }
  if (mCalIndex == mNumRefShots + mSTEMcamera && mDoingRefs) {
    if (mSTEMcamera) {
      mBlankMean = mMeanValues[mCalIndex-1];
      if (mWinApp->mScope->GetSimulationMode())
        mBlankMean = 0.;
      mMeanReference -= mBlankMean;
    }
    mCamParam->startupDelay = mQuickFlyback ? mFirstQuickStartup : mInitialStartup;
    if (mCamParam->K2Type == 1)
      mCamParam->startupDelay = mK2InitialStartup;
    if (mStartupOnly) {

      // Some cameras just need the one param to force initial blanking
      mCamParam->extraBeamTime = -1.;

      // Run no shutter camera as no shutter 1; the negative delays will be imposed in the
      // blanker
      if (!mCamParam->STEMcamera && mCamParam->noShutter == 2)
        mCamParam->noShutter = 1;
    } else {

      // Gatan: back off on built in settling but keep it as big as minimum drift
      // set up for blanking through to minimum drift time
      if (mCamParam->builtInSettling > mMinimumDrift)
        mCamParam->builtInSettling -= mBuiltInDecrement;
      if (mCamParam->builtInSettling < mMinimumDrift)
        mCamParam->builtInSettling = mMinimumDrift;

      mCamParam->minimumDrift = mMinimumDrift;
      mCamParam->extraBeamTime = mExtraBeamTime;
      mConSet->drift = mMinimumDrift;
      mConSet->shuttering = USE_DUAL_SHUTTER;
    }
    if (mSTEMcamera) {
      oneVal.Format("Measuring timing for %.2f sec, binning %d, size %dx%d, mag %d",
        mExposure, mConSet->binning, image->getWidth(), image->getHeight(),
        MagForCamera(mCamParam, mWinApp->mScope->FastMagIndex()));
      mWinApp->AppendToLog(oneVal);
    } else
      mWinApp->AppendToLog("Finding startup delay range:", LOG_OPEN_IF_CLOSED);
    mLineString.Format("Delay %.2f: ", mCamParam->startupDelay);
    mCalIndex = 0;
    mDoingRefs = false;
    mRangeIndex = 0;
  }

  // If a set of range shots are done, get mean, and see if startup delay long enough
  if (mRangeIndex >= 0 && (mCalIndex == mNumRangeShots || meanRatio > 0.99)) {
    if (mQuickFlyback && meanRatio > 0.99) {
      AfxMessageBox("Got an image with no blanking at the beginning of quick flyback "
        "timing\n\nYou should just try again with the same exposure time");
      StopCalTiming();
      mWinApp->ErrorOccurred(1);
      return;
    }
    meanRatio = 0.;
    for (nx = 0; nx < mCalIndex; nx++)
      meanRatio += mMeanValues[nx] / (mCalIndex * mMeanReference);
    mCalIndex = 0;

    // Get an adjusted reference based on the first point that is definitely dropped
    // 12/31/11: Don't understand this, but it's clearly wrong for STEM
    threshold = 1.f - mStartupIncrement / mExposure;
    if (!mSTEMcamera && mLowerRefRatio < -0.9 && meanRatio <= threshold && meanRatio >0.5)
      mLowerRefRatio = meanRatio / threshold;
    adjFactor = meanRatio;
    if (mLowerRefRatio > 0.)
      adjFactor = meanRatio / mLowerRefRatio;

    // Save means within not at extremes in arrays for fits
    if (adjFactor >= 0.05 && meanRatio <= 0.95) {
      mRangeMeans[mRangeIndex] = meanRatio;
      mRangeStartups[mRangeIndex++] = mCamParam->startupDelay;
    }
    if (adjFactor < 0.04 || (mSTEMcamera && mExposure > 4. && mCamParam->startupDelay >=
      mExposure + image->getHeight() * mCamParam->flyback / 1.e6) ||
      (mQuickFlyback && (mCamParam->startupDelay - mFirstQuickStartup) /
      mStartupIncrement >= mNumQuickDelays - 1.1)) {

      // Image is blanked - find delay in midpoint of range
      if (mRangeIndex < 2) {
        mWinApp->AppendToLog("There is too much blanking even with the shortest amount"
          " of blanking.\r\nSomething is wrong - giving up", LOG_OPEN_IF_CLOSED);
        StopCalTiming();
        return;
      }

      StatLSFit(mRangeStartups, mRangeMeans, mRangeIndex, slope, intcp);
      SEMTrace('c', "Slope %f  intercept %f from fit to all %d points, 0 at %.3f",
        slope, intcp, mRangeIndex, -slope / intcp);
      if (mRangeIndex > 3) {

        // If there are enough points, see if first one is an outlier
        StatLSFitPred(&mRangeStartups[1], &mRangeMeans[1], mRangeIndex - 1, &slope2,
          &intcp2, &rostat, mRangeStartups[0], &ypred, &prederr);
       SEMTrace('c', "Slope %f  intcp %f zero %.3f from restricted fit, rm0 %.4f  pred "
         "%.4f prederr %.4f", slope2, intcp2, -slope2 / intcp2, mRangeMeans[0], ypred,
         prederr);
       if (fabs((double)(ypred - mRangeMeans[0])) > 2. * prederr) {
          slope = slope2;
          intcp = intcp2;
          mWinApp->AppendToLog("Dropping first point from fit as an outlier",
            LOG_OPEN_IF_CLOSED);
        }
      }

      // Compute delay now by going back half of the exposure time from 0 intercept
      // For STEM camera, add putative delay in unblanking here and below
      // This will have to be undone if camera controller handles startup delay and
      // unblanking time better
      // 11/20/16: But now I am confused, because the timing in the blanker routine
      // is such that the end of the unblank will occur at the startup delay time!
      // So based on this, we are measuring true startup delay in nonSTEM situations
      mStartupDelay = 0.;
      if (fabs((double)slope) > 0.001) {
        mStartupDelay = -intcp / slope - mExposure / 2.f;
        if (mSTEMcamera)
          mStartupDelay = (1.f - intcp) / slope + mCamera->GetMinBlankingTime() / 2.f;
      }
      if (mStartupDelay <= 0. || mStartupDelay > B3DMAX(1., mExposure) * MAX_RANGE / 10.){
        PrintfToLog("Fit to mean counts implied an unreasonable startup delay "
          "(%.2f).\r\nSomething is wrong - giving up", mStartupDelay);
        StopCalTiming();
        return;
      }
      mCamParam->startupDelay = mStartupDelay;
      mRangeIndex = -1;

      // Compute flyback time for a STEM camera
      if (mSTEMcamera) {
        int fitStart = 0;
        int nfit = mRangeIndex;

        // Peel off outer points as long as that leaves at least 5
        while (nfit > 5) {
          if (mRangeMeans[fitStart] > 0.2 && mRangeMeans[fitStart + nfit - 1] < 0.8)
            break;
          if (mRangeMeans[fitStart] < 1. - mRangeMeans[fitStart + nfit - 1])
            fitStart++;
          nfit--;
        }

        // Get a new fit if anything was dropped
        if (nfit < mRangeIndex)
          StatLSFit(&mRangeStartups[fitStart], &mRangeMeans[fitStart], nfit, slope,intcp);
        mFlyback = -(float)(1.e6 * (1. / slope + mExposure) / image->getHeight());
        mCamParam->startupDelay = mStartupDelay = (0.5f - intcp) / slope +
          mCamera->GetMinBlankingTime() / 2.f;
        estStartup = (1.f - intcp) / slope + mCamera->GetMinBlankingTime() / 2.f;
        oneVal.Format("Flyback %.1f usec, startup delay %.3f sec computed using slope %f,"
          " intercept %f", mFlyback, estStartup, slope, intcp);
        mWinApp->AppendToLog(oneVal);
        if (mQuickFlyback) {
          CTime ctdt = CTime::GetCurrentTime();
          fbTime.exposure = mExposure;
          fbTime.binning = mConSet->binning;
          fbTime.xSize = image->getWidth();
          fbTime.magIndex = mWinApp->mScope->GetMagIndex();
          fbTime.flybackTime = mFlyback;
          fbTime.startupDelay = estStartup;
          oneVal.Format("%4d/%02d/%02d %02d:%02d:%02d", ctdt.GetYear(), ctdt.GetMonth(),
            ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
          strncpy(fbTime.dateTime, (LPCTSTR)oneVal, DATETIME_SIZE);
          fbTime.dateTime[DATETIME_SIZE - 1] = 0x00;
          if (mMatchedFlybackInd < 0)
            oneVal.Format("The data imply a flyback time of %.1f usec\nand a startup "
              "delay of %.2f sec\n\nDo you want to use these values\nand add them to the"
              " table of flyback times?", mFlyback, estStartup);
          else
            oneVal.Format("The data imply a flyback time of %.1f usec\nand a startup "
              "delay of %.2f sec\n\nDo you want to use these values and replace the\n"
              "existing values (%.1f and %.2f) in the table of flyback times?", mFlyback,
              estStartup, mFlybackArray[mMatchedFlybackInd].flybackTime,
              mFlybackArray[mMatchedFlybackInd].startupDelay);
          if (!mConfirmOK || AfxMessageBox(oneVal, MB_QUESTION) == IDYES) {
            if (mMatchedFlybackInd >= 0) {
              oneVal.Format("Replacing existing values of %1.f and %.2f in the table of "
                "flyback times", mFlybackArray[mMatchedFlybackInd].flybackTime,
                mFlybackArray[mMatchedFlybackInd].startupDelay);
              mWinApp->AppendToLog(oneVal);
              mFlybackArray.RemoveAt(mMatchedFlybackInd);
            }
            mFlybackArray.Add(fbTime);
            mNeedToSaveFlybacks = true;
          }
          StopCalTiming();
          return;
        }
      }
      oneVal.Format("Measuring timing with startup delay %.2f:", mStartupDelay);
      mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
      mCalIndex = 0;

    } else {

      // Image not blanked yet - increment and bail if gone too far
      if (mRangeIndex >= MAX_RANGE || mCamParam->startupDelay >
        B3DMAX(1., mExposure) * MAX_RANGE / 10.) {
        oneVal.Format("Cannot get a blanked image even with initial blanking up to %.1f"
          " seconds\r\nSomething is wrong - giving up", mCamParam->startupDelay);
        mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
        StopCalTiming();
        return;
      }
      mCamParam->startupDelay += mStartupIncrement;
      mLineString.Format("Delay %.2f: ", mCamParam->startupDelay);
    }
  }

  // Take the next picture unless it is time to stop
  if (mDoingRefs || mRangeIndex >= 0 || mCalIndex < mNumTestShots) {
    //SEMTrace('1', "OFF TO SHOOT");
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_CAM_TIMING, 0, 0);
    return;
  }

  // Find min and max, number of ones at extremes of range
  for (nx = 0; nx < mNumTestShots; nx++) {
    if (minCounts > mMeanValues[nx])
      minCounts = mMeanValues[nx];
    if (maxCounts < mMeanValues[nx])
      maxCounts = mMeanValues[nx];
    if (mMeanValues[nx] < 0.01 * mMeanReference)
      numLow++;
    if (mMeanValues[nx] > 0.99 * mMeanReference)
      numHigh++;
  }

  // Compute the dreaded encroachment time and implied startup delays
  adjFactor = 0.;
  if (!mStartupOnly)
    adjFactor = mMinimumDrift + mBuiltInSave - mCamParam->builtInSettling;

  minDelay = mStartupDelay - (adjFactor +
    (1. - minCounts / mMeanReference) * mExposure);
  maxDelay = mStartupDelay - (adjFactor +
    (1. - maxCounts / mMeanReference) * mExposure);
  if (mSTEMcamera) {
    minDelay -= 1.e-6 * mFlyback * image->getHeight() / 2.;
    maxDelay -= 1.e-6 * mFlyback * image->getHeight() / 2.;
  }
  newMinDrift = 0.05 * ceil(15. * (maxDelay - minDelay));
  newDelay = 0.005 * floor(100.* (minDelay + maxDelay) + 0.5);
  oneVal.Format("The minimum and maximum mean counts were %.1f and %.1f\r\n"
    "Based on these numbers, the startup delay ranged from %.3f to %.3f\r\n",
    minCounts, maxCounts, minDelay, maxDelay);
  mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);

  if (mStartupOnly) {
    if (newDelay < 0.01)
      newDelay = 0.01;
    oneVal.Format("In SerialEMproperties.txt, for this camera, set:\r\n"
      "StartupDelay  %.3f \r\nExtraBeamTime  %.2f", newDelay, newMinDrift);
    if (mSTEMcamera) {
      mLineString.Format("\r\nAddedFlybackTime   %.1f", mFlyback-mCamParam->basicFlyback);
      oneVal += mLineString;
      if (mBlankCycleTime > 0.025) {
        mLineString.Format("\r\nAlso, in the general properties (before CameraProperties)"
          " set:\r\nMinimumBlankingTime  %.3f", mBlankCycleTime);
        oneVal += mLineString;
      }
    } else if (mCamParam->GatanCam)
      oneVal += "\r\nBuiltInSettling  0.0";
    mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
    if (mCamParam->K2Type == 1 && newDelay < mCamera->GetK2MinStartupDelay()) {
      oneVal.Format("This StartupDelay is less than the currently allowed minimum\r\n"
        "delay for the K2, %.2f, and would not be safe to use.\r\n"
        "Just set StartupDelay to %.2f\r\n",
        mCamera->GetK2MinStartupDelay(), mCamera->GetK2MinStartupDelay() + 0.1);
      mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
    }

  } else {

    if (newDelay < 0.01) {
      delBIS = 0.01 - newDelay;
      newDelay = 0.01;
      oneVal.Format("This implies that the average startup delay is negative.\r\n"
        "To avoid negative delay, the BuiltInSettling needs to be decreased by %.3f",
        delBIS);
      mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
    }
    if (delBIS > mBuiltInSave) {
      mWinApp->AppendToLog("But this amount is bigger than BuiltInSettling -"
        " something is wrong", LOG_OPEN_IF_CLOSED);
    } else {
      oneVal.Format("In SerialEMproperties.txt, for this camera, set:\r\n"
        "StartupDelay  %.3f \r\nMinimumDriftSettling  %.2f \r\nExtraBeamTime  %.2f",
        newDelay, newMinDrift, newMinDrift);
      mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
      if (delBIS > 0.) {
        oneVal.Format("BuiltInSettling  %.3f", mBuiltInSave - delBIS);
        mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
      }
      if (mBlankCycleTime > 0.025) {
        oneVal.Format("\r\nAlso, in the general properties (before CameraProperties) "
          "set:\r\nMinimumBlankingTime  %.3f", mBlankCycleTime);
        mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
      }
    }
  }
  if (numLow > 1 || numHigh > 1) {
    oneVal.Format("\r\nThe values were at the end of the range more than once.\r\n"
      "You should rerun this procedure with an exposure time of %.2f", mExposure + 0.2);
    mWinApp->AppendToLog(oneVal, LOG_OPEN_IF_CLOSED);
  }

  StopCalTiming();
}

void CCalibCameraTiming::CalTimeCleanup(int param)
{
  if (param == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out trying to get image for camera timing"), MB_EXCLAME);
  StopCalTiming();
  mWinApp->ErrorOccurred(param);
}

void CCalibCameraTiming::StopCalTiming()
{
  if (mMeanValues)
    delete [] mMeanValues;
  mCalIndex = -1;
  mCamParam->minimumDrift = mMinDriftSave;
  mCamParam->startupDelay = mStartupSave;
  mCamParam->extraBeamTime = mExtraTimeSave;
  mCamParam->builtInSettling = mBuiltInSave;
  mCamParam->noShutter = mNoShutterSave;
  mConSet->doseFrac = 0;
  mConSet->alignFrames = 0;
  //mConSet->K2ReadMode = ??; Should this be reset? Do most track conset users copy trial?
  mCamera->SetMinBlankingTime(mMinBlankSave);
  mCamera->SetBlankNextShot(false);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

//////////////////////////////////////////////////////////////////
// Calibrate camera dead time by taking low-exposure-time pictures
//
void CCalibCameraTiming::CalibrateDeadTime()
{
  CString report;
  mCamera = mWinApp->mCamera;
  mCamParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  ControlSet *trialSet = mWinApp->GetConSets() + 2;
  CString *modeNames = mWinApp->GetModeNames();

  // Copy trial parameters
  *mConSet = *trialSet;
  report.Format("This procedure will take %d pictures at a series of exposure\n"
    "times, based on the " + modeNames[2] + " parameter set.\n\n"
    "The beam and the binning should be set up so that a " + modeNames[2] +
    "\nimage with 0.1 second exposure gives moderate counts.\n\n"
    "Are you ready to proceed?", mNumDeadExp * 2);
  if (AfxMessageBox(report, MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;

  // Get parameters, check limits
  if (!KGetOneFloat("Starting (minimum) exposure time (sec):", mDeadStartExp, 3))
    return;
  if (mDeadStartExp < 0.001)
    mDeadStartExp = 0.001f;
  if (mDeadStartExp > 0.1)
    mDeadStartExp = 0.1f;
  mConSet->exposure = mDeadStartExp;
  mConSet->onceDark = 1;
  mConSet->shuttering = USE_BEAM_BLANK;
  mDeadIndex = 0;
  mDoingRefs = true;
  mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_DEAD_TIME, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "CALIBRATING DEAD TIME");
  mWinApp->UpdateBufferWindows();
  mWinApp->AppendToLog("Exposure time and counts:", LOG_OPEN_IF_CLOSED);
}

void CCalibCameraTiming::CalDeadNextTask()
{
  CString str;
  float slope, intcp;
  if (mDeadIndex < 0)
    return;

  // Get image mean and save it and report it
  mImBufs->mCaptured = BUFFER_CALIBRATION;
  mDeadCounts[mDeadIndex] = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
  mDeadExposure[mDeadIndex] = mConSet->exposure;
  str.Format("%.3f sec:  %.1f", mConSet->exposure, mDeadCounts[mDeadIndex]);
  mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);

  mDeadIndex++;
  if (mDeadIndex >= mNumDeadExp) {

    // Do second round unless start is clode to 0.01 past the dead time
    StatLSFit(mDeadCounts, mDeadExposure, mDeadIndex, slope, intcp);
    if (!mDoingRefs ||
      (mDeadStartExp >= intcp + 0.005 &&  mDeadStartExp <= intcp + 0.015)) {
      StopCalDead();
      return;
    }
    mDoingRefs = false;
    mDeadIndex = 0;
    mDeadStartExp = intcp + 0.01f;
    if (mDeadStartExp < 0.001)
      mDeadStartExp = 0.001f;
    if (mDeadStartExp > 0.1)
      mDeadStartExp = 0.1f;
    str.Format("First series gives dead time %.3f; restarting with minimum %.3f",
      intcp, mDeadStartExp);
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  }
  mConSet->exposure = mDeadStartExp + mDeadIndex * mDeadExpInc;
  mConSet->onceDark = 1;
  mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_DEAD_TIME, 0, 0);
}

void CCalibCameraTiming::CalDeadCleanup(int param)
{
  if (param == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out trying to get image for measuring shutter dead time"),
      MB_EXCLAME);
  StopCalDead();
  mWinApp->ErrorOccurred(param);
}

void CCalibCameraTiming::StopCalDead()
{
  CString str;
  float slope, intcp;
  if (mDeadIndex >= 3) {
    StatLSFit(mDeadCounts, mDeadExposure, mDeadIndex, slope, intcp);
    str.Format("A line fit to %d points gives slope %f msec/count and intercept %.3f sec",
      mDeadIndex, 1000. * slope, intcp);
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
    if (intcp > 0.0005)
      str.Format("Set ShutterDeadTime for this camera to %.3f", intcp);
    else
      str = "Set ShutterDeadTime to 0 or leave the property out for this camera";
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  }
  mDeadIndex = -1;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Look up flyback times in the table that match the binning and size and return an
// exact, interpolated, or extrapolated value
int CCalibCameraTiming::FlybackTimeFromTable(int binning, int xSize, int magIndex,
                                             float exposure, float &flyback,
                                             float &startup, CString *message)
{
  int i, j, indi, indrp, last, dropInd;
  const int maxFit = 5;
  float xfit[maxFit], yfit[maxFit], prederr[maxFit];
  int *distInd;
  int *tableInd;
  float *distances;
  float tol = 0.01f;
  float errCrit = 2.f;
  float slope, intcp, roCorr, errOfPred, predY;
  int numFittable = 0;
  float minExposure = 1000000., maxExposure = -10.;

  // Scan table for exact match within tolerance and for number eligible to fit
  mMatchedFlybackInd = -1;
  for (i = 0; i < mFlybackArray.GetSize(); i++) {
    if (mFlybackArray[i].binning == binning && mFlybackArray[i].xSize == xSize &&
      mFlybackArray[i].magIndex == magIndex) {
        last = i;
        if (fabs(exposure - mFlybackArray[i].exposure) <= tol) {
          flyback = mFlybackArray[i].flybackTime;
          startup = mFlybackArray[i].startupDelay;
          mMatchedFlybackInd = i;
          return FLYBACK_EXACT;
        }
        numFittable++;
        ACCUM_MIN(minExposure, mFlybackArray[i].exposure);
        ACCUM_MAX(maxExposure, mFlybackArray[i].exposure);
    }
  }
  SEMTrace('c', "FlybackTimeFromTable: table size %d, numFittable %d exposure %f",
    mFlybackArray.GetSize(), numFittable, exposure);
  if (message)
    *message = "No calibrated flyback times are available for this binning and "
      "magnification";
  if (!numFittable)
    return FLYBACK_NONE;
  if (numFittable == 1) {
    flyback = mFlybackArray[last].flybackTime;
    startup = mFlybackArray[last].startupDelay;
    if (message)
      message->Format("Using the one calibrated flyback time, at exposure %g, available "
      "for this binning and magnification", mFlybackArray[last].exposure);
    return FLYBACK_SINGLE;
  }

  // Get arrays for variable amount of data;
  NewArray(distInd, int, numFittable);
  NewArray(tableInd, int, numFittable);
  NewArray(distances, float, numFittable);
  if (!distInd || !tableInd || !distances) {
    delete [] distInd;
    delete [] tableInd;
    delete [] distances;
    return FLYBACK_ERROR;
  }

  // Fill the arrays then sort on distance
  numFittable = 0;
  for (i = 0; i < mFlybackArray.GetSize(); i++) {
    if (mFlybackArray[i].binning == binning && mFlybackArray[i].xSize == xSize &&
      mFlybackArray[i].magIndex == magIndex) {
        distances[numFittable] = (float)fabs(exposure - mFlybackArray[i].exposure);
        distInd[numFittable] = numFittable;
        tableInd[numFittable++] = i;
    }
  }
  rsSortIndexedFloats(distances, distInd, numFittable);

  dropInd = -1;
  numFittable = B3DMIN(numFittable, maxFit);
  if (numFittable > 3) {

    // Drop each point in turn and compute difference between predicted and actual value
    for (int idrop = 0; idrop < numFittable; idrop++) {
      j = 0;
      indrp = tableInd[distInd[idrop]];
      for (i = 0; i < numFittable; i++) {
        indi = tableInd[distInd[i]];
        if (i != idrop) {
          xfit[j] = mFlybackArray[indi].exposure;
          yfit[j++] = mFlybackArray[indi].flybackTime;
        }
      }
      StatLSFitPred(&xfit[0], &yfit[0], j, &slope, &intcp, &roCorr,
        mFlybackArray[indrp].exposure, &predY, &errOfPred);
    SEMTrace('c', "Fit with drop %d: slope %f intcp %f ro %f err %f  predY %f table %f",
    idrop, slope, intcp, roCorr, errOfPred, predY, mFlybackArray[indrp].flybackTime);
      prederr[idrop] = (float)fabs(predY - mFlybackArray[indrp].flybackTime) /
        B3DMAX(0.001f, errOfPred);
      SEMTrace('c', "prederr %f", prederr[idrop]);
      if (prederr[idrop] > errCrit && (dropInd < 0 || prederr[idrop] > prederr[dropInd]))
        dropInd = idrop;
      SEMTrace('c', "dropInd %d", dropInd);
    }
  }

  // Now do fits with all points, or the one dropped, and fit to startup times too
  j = 0;
  for (i = 0; i < numFittable; i++) {
    indi = tableInd[distInd[i]];
    if (i != dropInd) {
      xfit[j] = mFlybackArray[indi].exposure;
      yfit[j] = mFlybackArray[indi].flybackTime;
      prederr[j++] = mFlybackArray[indi].startupDelay;
      SEMTrace('c', "%d %f %f %f", j, xfit[j-1], yfit[j-1], prederr[j-1]);
    }
  }
  StatLSFitPred(&xfit[0], &yfit[0], j, &slope, &intcp, &roCorr, exposure, &flyback,
    &errOfPred);
  SEMTrace('c', "Fit to %d flybacks, drop %d: slope %f intcp %f ro %f flyback %f",
    j, dropInd, slope, intcp, roCorr, flyback);
  StatLSFitPred(&xfit[0], &prederr[0], j, &slope, &intcp, &roCorr, exposure, &startup,
    &errOfPred);
  SEMTrace('c', "Fit to SUDs: slope %f intcp %f ro %f SUD %f", slope, intcp, roCorr,
    startup);
  if (message)
    message->Format("%spolating from fit to %d calibrated flyback times, nearest one at "
    "exposure %g", (exposure < minExposure || exposure > maxExposure) ? "Extra" : "Inter",
    numFittable, mFlybackArray[tableInd[distInd[0]]]);
  delete [] distInd;
  delete [] tableInd;
  delete [] distances;
  return ((exposure < minExposure || exposure > maxExposure) ? FLYBACK_EXTRAP :
    FLYBACK_INTERP);
}
