// BeamAssessor.cpp      Calibrates beam intensity, adjusts intensity using the
//                          calibration, and calibrates beam movement
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include ".\BeamAssessor.h"
#include "EMscope.h"
#include "ProcessImage.h"
#include "Utilities\XCorr.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "ComplexTasks.h"
#include "MultiTSTasks.h"
#include "NavHelper.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define LOG_BASE 0.001
#define SHIFT_CAL_CONSET  2
#define SPOT_CAL_CONSET  2
#define AUTO_PURGE_HOURS  12

enum CalTasks {CAL_RANGE_SHOT, CAL_FIRST_SHOT, CAL_TEST_SHOT, CAL_MATCH_BEFORE, 
CAL_MATCH_AFTER};
enum CalChanges {CAL_CHANGE_MAG = 1, CAL_CHANGE_BIN, CAL_CHANGE_EXP};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBeamAssessor::CBeamAssessor()
{
  int i, j;
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mLDParam = mWinApp->GetLowDoseParams();
  mNumTables = 0;
  mNumDoseTables = 0;
  mFreeIndex = 0;
  mStartIntensity = 0.;
  mBeamTables[0].intensities = &mAllIntensities[0];
  mBeamTables[0].currents = &mAllCurrents[0];
  mBeamTables[0].logCurrents = &mAllLogs[0];
  mIntensityThread = NULL;
  mCalibratingIntensity = false;
  mShiftCalIndex = -1;
  mRefiningShiftCal = false;
  mRBSCmaxShift = 6.;
  mCalMinField = 8.;
  mExtraRangeAtMinMag = 10.f;
  mIntervalTolerance = 0.3;     // Maximum deviation of interval from target
  mPostSettingDelay = 400;      // Delay after setting intensity
  mInitialIncrement = 0.0001;   // starting increment for beam intensity
  mSpacingFactor = 1.1;
  mUseTrialSettling = false;
  mMinExposure = 0.1f;
  mMaxExposure = 3.f;
  mMaxCounts = 8000;
  mMinCounts = 100;
  mCntK2MinExposure = 0.1f;
  mCntK2MaxExposure = 0.4f;
  mCntK2MinCounts = 2000;
  mLinK2MinCounts = 10000;
  mCntK2MaxCounts = 30000;
  mFavorMagChanges = false;
  mNumMatchAfter = 2;
  mNumMatchBefore = 3;
  mMagMaxDelCurrent = 8.;
  mMagExtraDelta = 8.;
  for (i = 0; i <= MAX_SPOT_SIZE; i++) {
    mDoseTables[i][0][0].dose = 0.;
    mDoseTables[i][1][0].dose = 0.;
    mDoseTables[i][0][1].dose = 0.;
    mDoseTables[i][1][1].dose = 0.;
    mSpotTables[0].ratio[i] = 0.;
    mSpotTables[1].ratio[i] = 0.;
  }
  mSpotCalIndex = 0;
  mNumSpotTables = 0;
  mCurrentAperture = 0;
  mNumC2Apertures = 0;
  mCalIAlimitSpot = -1;
  mCalIAtestValue = 10.f;
  for (j = 0; j < 4; j++)
    mSpotCalAperture[j] = 0;
  mCrossCalAperture[0] = mCrossCalAperture[1] = 0;
  for (i = 0; i < MAX_ALPHAS;i++)
    mBSCalAlphaFactors[i] = 1.;
}

CBeamAssessor::~CBeamAssessor()
{

}

void CBeamAssessor::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mShiftManager = mWinApp->mShiftManager;
}


// CALIBRATE BEAM WITH CCD CAMERA
void CBeamAssessor::CalIntensityCCD()
{
  double ratio;
  int in, out, i, camera, aboveCross, fracField, probe;
  float minDoseRates[2] = {2.5f, 9.4f};
  float maxDoseRates[2] = {20.f, 75.f};
  camera = mWinApp->GetCurrentCamera();
  mCamParam = mWinApp->GetCamParams() + camera;
  CString message;
  int minMagIndNeeded = mWinApp->mComplexTasks->FindMaxMagInd(mCalMinField);
  int K2ind = mCamParam->K2Type == K3_TYPE ? 1 : 0;
  AssignCrossovers();
  probe = mScope->ReadProbeMode();

  if (mWinApp->LowDoseMode()) {
    AfxMessageBox("Beam intensity calibration cannot be done in Low Dose mode");
    return;
  }
  
  if (mNumTables >= MAX_INTENSITY_TABLES || mFreeIndex > 0.9 * MAX_INTENSITY_ARRAYS) {
    AfxMessageBox("No more space for intensity calibrations", MB_EXCLAME);
    return;
  }

  mCountingK2 = mCamParam->K2Type && mCamParam->K2Type != K2_BASE && 
    mCamParam->countsPerElectron;
 
  // To protect against Tecnai intensity problem, just get and set intensity
  mStartIntensity = mScope->GetIntensity();
  mScope->SetIntensity(mStartIntensity);
  mScope->NormalizeCondenser();
  mCalSpotSize = mScope->GetSpotSize();
  aboveCross = GetAboveCrossover(mCalSpotSize, mStartIntensity, probe);
  message.Format("To calibrate beam intensity for a spot size:\n"
    "1) Make sure there is no specimen in the beam.\n"
    "2) Make sure the beam is on the side of crossover where you want to work.\n"
    "3) Start at the highest mag and brightest beam that you want calibrated\n"
    "  (It might be safest to start at a mag 2 times higher).\n"
    "4) Center the beam and spread it so that it just covers the camera (for FEG) or "
    "fills the screen (non-FEG)\n%s" 
    "5) Be sure to cover the viewing port after setting up the beam.", 
    FEIscope ? "   (Check it now - it could have changed due to normalization or setting"
    " intensity.)\n" : "");
  
  if (mScope->GetCrossover(mCalSpotSize, probe) > 0.) {
    message = message + "\n\nThe calibration will be done for intensities " + 
      (aboveCross ? "above" : "below") + " crossover.";
    if (IntensityCalibrated())
      message += "\nIt will replace an existing calibration there.";
  }
  if (AfxMessageBox(message, MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
    return;

  mStartIntensity = mScope->GetIntensity();
  mCalSpotSize = mScope->GetSpotSize();
  mCalMagInd = mScope->GetMagIndex();
  if (!FEIscope && !(JEOLscope && mScope->GetJeolHasBrightnessZoom())) {
    message.Format("Make sure that Brightness %s is turned off",
      HitachiScope ? "Link" : "Zoom");
    AfxMessageBox(message,  MB_EXCLAME); 
  } else {
    mUsersIntensityZoom = mScope->GetIntensityZoom();
    mScope->SetIntensityZoom(false);
  }
  if (!mCalMagInd || !mCalSpotSize) {
    AfxMessageBox("Error getting spot size or mag index", MB_EXCLAME);
    return;
  }
  if (aboveCross != GetAboveCrossover(mCalSpotSize, mStartIntensity, probe)) {
    AfxMessageBox("The beam now seems to be on the other side of crossover, cannot"
      " proceed.", MB_EXCLAME);
    return;
  }

  // Wipe out all tables at this spot size and this side of crossover
  mFreeIndex = 0;
  mNumExpectedCals = CountNonEmptyBeamCals();
  for (in = 0, out = 0; in < mNumTables; in++) {
    if (!mBeamTables[in].numIntensities)
      continue;
    if (CheckCalForZeroIntensities(mBeamTables[in], "Before doing a beam calibration",
      3)) {
        mNumExpectedCals--;
    } else if (mBeamTables[in].spotSize != mCalSpotSize ||
      mBeamTables[in].aboveCross != aboveCross || mBeamTables[in].probeMode != probe) {
        SEMTrace('c', "Retaining table for spot %d  aboveCross %d  probeMode %d", 
          mBeamTables[in].spotSize, mBeamTables[in].aboveCross,mBeamTables[in].probeMode);
      mBeamTables[out] = mBeamTables[in];

      // Set up the addresses in the master arrays and copy elements
      mBeamTables[out].intensities = &mAllIntensities[mFreeIndex];
      mBeamTables[out].currents = &mAllCurrents[mFreeIndex];
      mBeamTables[out].logCurrents = &mAllLogs[mFreeIndex];

      for (i = 0; i < mBeamTables[in].numIntensities; i++) {
        mBeamTables[out].intensities[i] = mBeamTables[in].intensities[i];
        mBeamTables[out].currents[i] = mBeamTables[in].currents[i];
        mBeamTables[out].logCurrents[i] = mBeamTables[in].logCurrents[i];
      }

      mFreeIndex += mBeamTables[in].numIntensities;
      out++;
    } else {
      SEMTrace('c', "Deleting table for spot %d  aboveCross %d  probeMode %d", 
          mBeamTables[in].spotSize, mBeamTables[in].aboveCross,mBeamTables[in].probeMode);
      mNumExpectedCals--;
    }
  }
  mNumTables = out;

  // Set up new table
  mTableInd = mNumTables++;
  mCalTable = &mBeamTables[mTableInd];
  mCurrents = mCalTable->currents = &mAllCurrents[mFreeIndex];
  mIntensities = mCalTable->intensities = &mAllIntensities[mFreeIndex];
  mCalTable->logCurrents = &mAllLogs[mFreeIndex];
  mCalTable->spotSize = mCalSpotSize;
  mCalTable->magIndex = mCalMagInd;
  mCalTable->dontExtrapFlags = 0;
  mCalTable->crossover = mScope->GetCrossover(mCalSpotSize, probe);
  mCalTable->measuredAperture = 0;
  mCalTable->probeMode = probe;
  mCalTable->aboveCross = aboveCross;
  
  // Get total range of intensity that needs to be covered to be able to work
  // at the minimum mag: based on pixel size and extra range desired
  ratio = mShiftManager->GetPixelSize(camera, mCalMagInd) /
    mShiftManager->GetPixelSize(camera, minMagIndNeeded);
  mChangeNeeded = ratio * ratio / mExtraRangeAtMinMag;

  mTestIncrement = mInitialIncrement;
  if (mCalTable->crossover > 0. && mStartIntensity < mCalTable->crossover)
    mTestIncrement = -mInitialIncrement;
  mTestIntensity = mStartIntensity;
  mNumFail = 0;
  mNumTry = 0;
  mNumIntensities = 0;
  mMatchFactor = 1.;
  mNextMagInd = mCalMagInd;
  mUseMaxCounts = mMaxCounts;
  mUseMinCounts = mMinCounts;
  mUseMaxExposure = mMaxExposure;
  mMagMaxDelCurrent = B3DMIN(mMagMaxDelCurrent, (float)mMaxCounts / mMinCounts);
  mExposure = mMinExposure;
  if (mCountingK2) {
    mMinDoseRate = minDoseRates[K2ind];
    mMaxDoseRate = maxDoseRates[K2ind];
    mUseMaxCounts = mCntK2MaxCounts;
    mUseMaxExposure = mCntK2MaxExposure;
    mExposure = mCntK2MinExposure;
    mUseMinCounts = mLinK2MinCounts;
  }

  mBinning = mCamParam->binnings[CamHasDoubledBinnings(mCamParam) ? 1 : 0];
  fracField = mCamera->IsDirectDetector(mCamParam) ? 2 : 8;
  if (mCamParam->subareasBad)
    fracField = mCamParam->subareasBad > 1 ? 1 : 2;
  mSaveUseK3CDS = mCamera->GetUseK3CorrDblSamp();
  mCamera->SetUseK3CorrDblSamp(false);
  MakeControlSet(fracField, false);
  mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CCD_CAL_INTENSITY, 
    CAL_RANGE_SHOT, 0);
  mCalibratingIntensity = true;
  mStoppingCal = false;
  mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING BEAM");
  mWinApp->UpdateBufferWindows();
}

// HANDLE THE NEXT IMAGE FROM BEAM INTENSITY CALIBRATION
void CBeamAssessor::CalIntensityImage(int param)
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  double testInterval, ratio, testCurrent, meanCounts, bufferMean;
  int task = 0;
  float doseRate = 0., extraDelta = mMagExtraDelta;
  double targetInterval = -log(mSpacingFactor);
  double targetLo = targetInterval + mIntervalTolerance * targetInterval;
  double targetHi = targetInterval - mIntervalTolerance * targetInterval;
  bool k2TooLow;
  imBuf->mCaptured = BUFFER_CALIBRATION;
  CString report;
  if (!mCalibratingIntensity)
    return;

  // Get a dose rate for counting K2 camera
  bufferMean = mWinApp->mProcessImage->WholeImageMean(imBuf);
  meanCounts = bufferMean *
    (mCamParam->unsignedImages && mCamera->GetDivideBy2() ? 2. : 1.);
  if (mCountingK2 && 
    !mWinApp->mProcessImage->DoseRateFromMean(imBuf, (float)bufferMean, doseRate))
      meanCounts = doseRate * mCamParam->countsPerElectron * mExposure * 
        mBinning * mBinning / 4.;

  testCurrent = mMatchFactor * meanCounts / (mBinning * mBinning * mExposure);

  /*report.Format("Task %d bin %d exp %f int %.5f mean %.0f current %.2f", param, mBinning,
    mExposure, mTestIntensity, meanCounts, testCurrent);
  mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);*/
  
  switch (param) {
    
    // FIRST SHOT FOR FINDING RANGE
  case CAL_RANGE_SHOT:

    // For K2, if dose rate is in range for counting, test against minimum and also set
    // the rest of the counting-specific parameters; otherwise drop back to linear mode
    if (mCountingK2) {
      if (doseRate > 0. && doseRate < 1.05 * mMaxDoseRate) {
        if (doseRate < 0.95 * mMinDoseRate) {
          report.Format("The dose rate is %.1f e/pixel/sec, below the minimum rate"
            " of %.1f for starting this calibration with this camera\n\n"
            "Condense the beam or use a larger condenser aperture", doseRate,
            mMinDoseRate);
          AfxMessageBox(report, MB_EXCLAME);
          mStoppingCal = true;
          break;
        }
        mUseMinCounts = mCntK2MinCounts;
        mMagMaxDelCurrent = B3DMIN(mMagMaxDelCurrent, mMaxDoseRate / mMinDoseRate);
        if (doseRate / mMinDoseRate < mMagMaxDelCurrent)
          mMagMaxDelCurrent = B3DMAX(2.f, doseRate / mMinDoseRate);
      } else {
        mCountingK2 = false;
      }
      report.Format("Dose rate is %.1f e/ubpix/sec; using %s mode for the calibration",
        doseRate, mCountingK2 ? "counting" : "linear");
      mWinApp->AppendToLog(report);
    }
    
    // increase binning as much as possible within range of allowed counts
    while (testCurrent * mExposure * mBinning * mBinning * 4 <= mUseMaxCounts && 
      CanDoubleBinning()) {
      mBinning *= 2;
      if (mCountingK2 && mBinning >= 8)
        break;
    }
    
    // Set up for half-field image with this binning
    MakeControlSet(mCamParam->subareasBad > 1 ? 1 : 2, mCountingK2);
    task = CAL_FIRST_SHOT;
    break;
    
    // FIRST SHOT FOR REAL; start the table
  case CAL_FIRST_SHOT:
    mStartCurrent = testCurrent;
    mCurrents[0] = (float)testCurrent;
    mIntensities[0] = (float)mStartIntensity;
    mTestIntensity = mStartIntensity + mTestIncrement;
    mNumIntensities = 1;
    mTrials[0] = 1;

    // For K2, do not do all the extra spreading at the highest mag, only do a factor of
    // 2 to increase beam safety, to keep beam from getting too weak.
    if (mCamParam->K2Type || mFavorMagChanges) {
      extraDelta = 1.;
      if (mCamParam->countsPerElectron > 0. ) {
        SEMTrace('c', "Raw counts %.1f, counts and electrons per unbinned pixel / sec ="
          " %.1f  %.3f", meanCounts, 4 * testCurrent, 
          4. * testCurrent / mCamParam->countsPerElectron);
        if (4. * testCurrent / mCamParam->countsPerElectron > 100.)
          extraDelta = 2.;
      }
    }
    CalSetupNextMag(extraDelta);
    if (SetAndTestIntensity())
      task = CAL_TEST_SHOT;
    break;
    
    // A SHOT IN THE TEST LOOP
  case CAL_TEST_SHOT:

    // Get the log interval and adjust the increment to hit target interval
    // After the first time, do not let increment change sign
    // and restrict how much it can change
    testInterval = log((testCurrent + LOG_BASE) / 
      (mCurrents[mNumIntensities - 1] + LOG_BASE));
    
    ratio = targetInterval / testInterval;
    if (mNumIntensities == 1 && ratio < 0 && mCalTable->crossover >= 0.) {
     SEMTrace('c', "%d %.1f %7.1f %8.3f %8.3f %8.5f %.5f %.5f %.5f %.5f %.5f", mBinning, 
       mExposure, meanCounts, testCurrent, mCurrents[0], mTestIntensity, testInterval, 
       targetInterval, mTestIncrement, mIntensities[0], mCalTable->crossover);
     AfxMessageBox("The counts increased even though intensity was changed in the "
        "direction away from crossover.\r\n\r\nYou need to either redo the "
        "crossover calibration or start farther from crossover", MB_EXCLAME);
      break;
    }
    if (mNumIntensities > 1) {
      if (ratio < 0.) {
        ratio = 3.;
        SEMTrace('c', "COUNTS INCREASED ABOVE START, tripling increment");
      } else
        B3DCLAMP(ratio, 0.3, 3.);
    }
    
    SEMTrace('c', "%d %d %d %.1f mc %7.1f dr %4.1f tc %8.3f int %8.5f ti %.5f inc %.5f",
      mNumIntensities, mNumTry, mBinning, mExposure, meanCounts, doseRate, testCurrent, 
      mTestIntensity, testInterval, mTestIncrement);
    
    // If interval acceptable, save values; otherwise repeat
    // Adjust the increment if successful
    if ((testInterval >= targetLo && testInterval <= targetHi) || mNumTry == 10) {
      mTestIncrement *= ratio;
      
      // Count failures and quit on fifth one
      if (!(testInterval >= targetLo && testInterval <= targetHi)) {
        mNumFail++;
        if (mNumFail > 4) {
          AfxMessageBox("Failed to find proper interval between\n"
            "intensities too many times", MB_EXCLAME);
          mStoppingCal = true;
          break;
        }
      }
      
      // Save values; quit if enough change has been measured
      mTrials[mNumIntensities] = mNumTry + 1;
      mIntensities[mNumIntensities] = (float)mTestIntensity;
      mCurrents[mNumIntensities++] = (float)testCurrent;
      report.Format("%9.2f  %9.5f   %d", testCurrent, mTestIntensity, mNumTry + 1);
      mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);
      mNumTry = 0;
      mCalTable->numIntensities = mNumIntensities;
      CheckCalForZeroIntensities(mBeamTables[mTableInd], "While doing the calibration", 
        1);
      
      if (testCurrent / mStartCurrent < mChangeNeeded)
        break;
      
      // Test for changing the acquisition parameters:
      // If binning can be doubled;
      // If counts and current are below criteria for mag change; or for K2, if current is
      // below and the counts are below the min counts
      // if counts are down 10 & exposure is still small or if counts are down to the min
      mCalChangeType = 0;
      k2TooLow = mCountingK2 && doseRate > 0 && doseRate < mMinDoseRate;
      if (meanCounts * 4 < mUseMaxCounts && CanDoubleBinning() && !mCountingK2)
        mCalChangeType = CAL_CHANGE_BIN;
      else if ((k2TooLow && testCurrent < mNextMagMaxCurrent) ||
        (testCurrent < mMagChgMaxCurrent && (meanCounts < mMagChgMaxCounts || 
        k2TooLow || (meanCounts  < mUseMinCounts && ((mCamParam->K2Type && 
        (!mCountingK2 || !doseRate)) || mFavorMagChanges))))) {
        mCalChangeType = CAL_CHANGE_MAG;
        mDropBinningOnMagChg = meanCounts > mMagChgMaxCounts && mBinning > 2;
      } else if ((meanCounts * 10 < mMaxCounts && mExposure <= 0.25) || 
        (meanCounts < mUseMinCounts && mExposure < mUseMaxExposure))
        mCalChangeType = CAL_CHANGE_EXP;
      if (mCalChangeType) {
        SEMTrace('c', "Changing %s", mCalChangeType == CAL_CHANGE_BIN ? "binning" : 
          (mCalChangeType == CAL_CHANGE_MAG ? "mag" : "exposure"));
        mMatchSum = testCurrent;
        mMatchInd = 1;
        task = CAL_MATCH_BEFORE;
        break;
      }
      
    } else {
      
      // If not successful, adjust increment if on the first
      // intensity, or on every other trial
      if (mNumIntensities <= 1 || mNumTry % 2)
        mTestIncrement *= ratio;
      mNumTry++;
    }
    
    // Set up intensity for next round and test it for termination
    mTestIntensity = mIntensities[mNumIntensities - 1] + mTestIncrement;
    if (SetAndTestIntensity())
      task = CAL_TEST_SHOT;
    break;
    
    // SHOT FOR GETTING AVERAGE BEFORE A CHANGE; accumulate value in sum
  case CAL_MATCH_BEFORE:
    mMatchSum += testCurrent;
    mMatchInd++;
    SEMTrace('c', "Before change shot %d  counts %.1f current %.2f", mMatchInd, 
      meanCounts, testCurrent);
    if (mMatchInd < mNumMatchBefore) {
      task = CAL_MATCH_BEFORE;
      break;
    }
   
    // If got enough, replace the value in the table with the mean and set up for
    // matching shots after change
    mCurrents[mNumIntensities - 1] = (float)(mMatchSum / mMatchInd);
    mMatchSum = 0.;
    mMatchInd = 0;
    
    // Make the indicated change
    if (mCalChangeType == CAL_CHANGE_BIN)
      mBinning *= 2;
    else if (mCalChangeType == CAL_CHANGE_MAG) {
      mScope->SetMagIndex(mNextMagInd);
      if (mDropBinningOnMagChg)
        mBinning /= 2;
      CalSetupNextMag(1.);
      Sleep(1000);

      // Due to unresolved issue on JEOL F200(?), wait a bit and check and reset intensity
      ratio = mScope->GetIntensity();
      SEMTrace('c', "Intensity after mag change %.5f", ratio);
      if (fabs(ratio - mIntensities[mNumIntensities - 1]) > mTestIncrement)
        mScope->SetIntensity(mIntensities[mNumIntensities - 1], mCalSpotSize);
    } else
      mExposure *= 4.;
    if (mExposure > mUseMaxExposure)
      mExposure = mUseMaxExposure;
    MakeControlSet(mCamParam->subareasBad > 1 ? 1 : 2, mCountingK2);
    task = CAL_MATCH_AFTER;
    break;
    
    // SHOT FOR GETTING AVERAGE AFTER A CHANGE: accumulate for mean
  case CAL_MATCH_AFTER:
    mMatchSum += testCurrent;
    mMatchInd++;
    SEMTrace('c', "After change shot %d  counts %.1f current %.2f", mMatchInd, 
      meanCounts, testCurrent);
    if (mMatchInd < mNumMatchAfter) {
      task = CAL_MATCH_AFTER;
      break;
    }
    
    // Adjust the match factor to make new numbers match old ones
    mMatchFactor *= mCurrents[mNumIntensities - 1] / (mMatchSum / mMatchInd);
    SEMTrace('c', "New matching factor %f", mMatchFactor);
    mTestIntensity = mIntensities[mNumIntensities - 1] + mTestIncrement;
    if (SetAndTestIntensity())
      task = CAL_TEST_SHOT;
    break;

  }
    
  // If task is set and user did not stop, take the next shot
  if (task && !mStoppingCal) {
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CCD_CAL_INTENSITY, 
      task, 0);
    return;
  }
  
  CalIntensityCCDCleanup(0);
    
}

// Finish CCD-based intensity calibration normally or upon error
void CBeamAssessor::CalIntensityCCDCleanup(int error)
{
  CString report;
  int newAp;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  if (!mCalibratingIntensity)
    return;

  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out trying to get image for beam intensity calibration"), 
      MB_EXCLAME);

  if (mStoppingCal) {
    report.Format("Intensity cal stopped at %d x, %.4f%s %s, exposure %.3f, binning %f,"
      "\r\n   binned image size %d and %d in X and Y", 
      MagForCamera(mCamParam, mScope->GetMagIndex()),
      mScope->GetC2Percent(mCalSpotSize, mScope->GetIntensity()), mScope->GetC2Units(),
      mScope->GetC2Name(), mExposure, mBinning / BinDivisorF(mCamParam), 
      (conSet->right - conSet->left) / mBinning,
      (conSet->bottom - conSet->top) / mBinning);
    mWinApp->AppendToLog(report);
  }

  mCalTable->numIntensities = mNumIntensities;
  if (mNumIntensities) {
    if (mCurrents[0] / mCurrents[mNumIntensities - 1] < 1.5) {
      mCalTable->numIntensities = mNumIntensities = 0;
      mWinApp->AppendToLog("Beam intensity calibration too small to be usable",
        LOG_MESSAGE_IF_CLOSED);
    } else {
      report.Format("Beam intensity calibration completed, covering a %d-fold change in"
        " intensity", B3DNINT(mCurrents[0] / mCurrents[mNumIntensities - 1]));
      mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
      if (CheckCalForZeroIntensities(mBeamTables[mTableInd], 
        "Right after running the calibration", 1)) {
        mCalTable->numIntensities = mNumIntensities = 0;

      // For Titan, get the current aperture and scale all tables to it
      } else if (mScope->GetUseIllumAreaForC2()) {
        newAp = RequestApertureSize();
        if (newAp) {
          mCalTable->measuredAperture = newAp;
          ScaleTablesForAperture(newAp, false);
        } else
          mWinApp->AppendToLog("No aperture size is being recorded for this intensity"
          " calibration");
      }
    }
  }

  if (error)
    mWinApp->ErrorOccurred(error);

  // sort the table and take logs of currents
  if (mCalTable->numIntensities) {
    SortAndTakeLogs(mCalTable, true);
    if (CheckCalForZeroIntensities(mBeamTables[mTableInd], 
      "After sorting the new calibration and taking logs", 2))
      mCalTable->numIntensities = mNumIntensities = 0;
  }
  if (mCalTable->numIntensities)
    mNumExpectedCals++;
  newAp = CountNonEmptyBeamCals();
  if (newAp != mNumExpectedCals) {
    report.Format("After finishing or ending that beam calibration\r\n"
      "  there are %d calibrations, while %d were expected.\r\n  Please report this "
      "situation and the steps leading up to it to the SerialEM developer.", newAp, 
      mNumExpectedCals);
    AfxMessageBox(report, MB_EXCLAME);
    mWinApp->AppendToLog("WARNING: " + report);
  }
  mFreeIndex += mCalTable->numIntensities;
  mScope->SetIntensity(mStartIntensity, mCalSpotSize);
  mScope->SetMagIndex(mCalMagInd);
  if (!HitachiScope)
    mScope->SetIntensityZoom(mUsersIntensityZoom);
  mCalibratingIntensity = false;
  mCamera->SetUseK3CorrDblSamp(mSaveUseK3CDS);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  if (mNumIntensities)
    mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_BEAM);
}

// Make a control set with current parameters for give fraction of field
void CBeamAssessor::MakeControlSet(int fracField, bool useCounting)
{
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  ControlSet *trialSet = mWinApp->GetConSets() + TRIAL_CONSET;

  // This was needed on two 3100's which had terribly slow beam turn-on
  if (mUseTrialSettling) {
    conSet->drift = trialSet->drift;
    conSet->shuttering = trialSet->shuttering;
  } else {
    conSet->drift = 0.f;
    conSet->shuttering = mCamParam->DMbeamShutterOK ? USE_BEAM_BLANK : USE_FILM_SHUTTER;
  }
  conSet->exposure = mExposure;
  conSet->binning = mBinning;
  conSet->onceDark = 1;
  conSet->forceDark = 0;
  conSet->processing = GAIN_NORMALIZED;
  conSet->mode = SINGLE_FRAME;
  conSet->left = ((fracField - 1) * mCamParam->sizeX) / (2 * fracField);
  conSet->right = ((fracField + 1) * mCamParam->sizeX) / (2 * fracField);
  conSet->top = ((fracField - 1) * mCamParam->sizeY) / (2 * fracField);
  conSet->bottom = ((fracField + 1) * mCamParam->sizeY) / (2 * fracField);
  conSet->K2ReadMode = useCounting ? COUNTING_MODE : LINEAR_MODE;
  conSet->doseFrac = 0;
  conSet->saveFrames = 0;
  conSet->alignFrames = 0;
  conSet->useFrameAlign = 0;
}

// Test for whether binning can be doubled
BOOL CBeamAssessor::CanDoubleBinning()
{
  for (int i = 0; i < mCamParam->numBinnings; i++)
    if (mBinning * 2 == mCamParam->binnings[i])
      return true;
  return false;
}

// Determine next mag that can be dropped to, if any, and set criteria for going to it
void CBeamAssessor::CalSetupNextMag(float extraDelta)
{
  double ratio;
  int magNow = mNextMagInd;
  int camera = mWinApp->GetCurrentCamera();

  // Set to zero in case no mag change can be done
  mMagChgMaxCurrent = 0.;
  mNextMagMaxCurrent = 0.;

  // Loop down from one below current mag to lowest M mag index, getting ratio of
  // pixel sizes squared as current change ratio
  for (int mag = mNextMagInd - 1; mag >= mScope->GetLowestMModeMagInd(); mag--) {
    ratio = mShiftManager->GetPixelSize(camera, mag) /
      mShiftManager->GetPixelSize(camera, magNow);
    ratio *= ratio;

    // If ratio is too big, done looping; otherwise it is a valid mag change, so
    // compute current criterion for it, allowing extra change given by argument
    // and get maximum counts allowed before mag change, allowing some margin
    if (ratio > mMagMaxDelCurrent && mag < magNow - 1)
      break;
    mMagChgMaxCurrent = mCurrents[mNumIntensities - 1] / (ratio * extraDelta);
    mMagChgMaxCounts = 0.9 * mUseMaxCounts / ratio;
    mNextMagInd = mag;

    // Get a max current that should allow switching to the very next mag if the dose rate
    // gets too low, reference this to initial mag/current so it doesn't decline through
    // several changes
    if (mag == magNow - 1) {
      ratio = mShiftManager->GetPixelSize(camera, mag) /
        mShiftManager->GetPixelSize(camera, mCalMagInd);
      mNextMagMaxCurrent = 0.8 * mCurrents[0] / (ratio * ratio);
    }
  }
  // Exit with the biggest mag change that fits within range allowed
  SEMTrace('c', "Next mag %d max current %f  max counts %f", 
    MagForCamera(camera, mNextMagInd), mMagChgMaxCurrent, mMagChgMaxCounts);
}

// Set the intensity and test if it changes enough; return true if it did
BOOL CBeamAssessor::SetAndTestIntensity()
{
  int delay;
  double realIntensity;

  if (mTestIntensity < 0.01 || mTestIntensity > 0.99) {
     mCalTable->dontExtrapFlags |= BEAM_DONT_EXTRAP_LOW;
     SEMTrace('c', "Test intensity %f out of range", mTestIntensity);
    return false;
  }

  // If this is a new increment for a later trial, and it is less than the last one,
  // back up to the starting intensity to counteract hysteresis or overshooting
  if (mNumIntensities > 1 && mLastNumIntensities == mNumIntensities &&
    fabs(mLastIncrement) > fabs(1.000001 * mTestIncrement)) {
      SEMTrace('c', "Backing up to %.5f", mIntensities[mNumIntensities - 1]);
      mScope->SetIntensity(mIntensities[mNumIntensities - 1], mCalSpotSize);
      Sleep(B3DNINT(0.25 * B3DMAX(mPostSettingDelay, 1000)));
  }

  mScope->SetIntensity(mTestIntensity, mCalSpotSize);
  delay = mPostSettingDelay - (int)(1000. * mCamParam->builtInSettling);
  if (delay > 0)
    Sleep(delay);
  realIntensity = mScope->FastIntensity();
  if (fabs(realIntensity - mTestIntensity) > fabs(0.5 * mTestIncrement)) {
    // It really can't go to 0.9
    SEMTrace('c', "Failed to set intensity %f; got %f, increment %f", mTestIntensity,
      realIntensity, mTestIncrement);
    mCalTable->dontExtrapFlags |= BEAM_DONT_EXTRAP_LOW;
    return false;
  }
  mTestIntensity = realIntensity;
  mLastIncrement = mTestIncrement;
  mLastNumIntensities = mNumIntensities;
  return true;
}


// Sort a table by currents, take the log of the currents, and set min and max intensity
void CBeamAssessor::SortAndTakeLogs(BeamTable *inTable, BOOL printLog)
{
  int i, j, itmp;
  float temp;
  double dtmp;
  float *currents = &inTable->currents[0];
  double *intensities = &inTable->intensities[0];
  float *logCurrents = &inTable->logCurrents[0];
  if (!inTable->numIntensities)
    return;

  for (i = 0; i < inTable->numIntensities - 1; i++) {
    for (j = i + 1; j < inTable->numIntensities; j++) {
      if (currents[i] > currents[j]) {
        temp = currents[i];
        currents[i] = currents[j];
        currents[j] = temp;
        dtmp = intensities[i];
        intensities[i] = intensities[j];
        intensities[j] = dtmp;
        itmp = mTrials[i];
        mTrials[i] = mTrials[j];
        mTrials[j] = itmp;
      }
    }
  }
  
  // Get min and max intensity
  if (intensities[0] < intensities[inTable->numIntensities - 1]) {
    inTable->minIntensity = (float)intensities[0];
    inTable->maxIntensity = (float)intensities[inTable->numIntensities - 1];
  } else {
    inTable->maxIntensity = (float)intensities[0];
    inTable->minIntensity = (float)intensities[inTable->numIntensities - 1];
  }

  CString report;
  for (i = 0; i < inTable->numIntensities; i++) {
    logCurrents[i] = (float)log((double)currents[i] + LOG_BASE);
     /*report.Format("%8.3f  %9.5f   %d", currents[i], intensities[i], mTrials[i]);
     if (printLog)
       mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED); */
  }
}

// Set the above crossover index to 1 if crossover calibrated and mean intensity is above
void CBeamAssessor::AssignCrossovers()
{
  int index;
  for (index = 0; index < mNumTables; index++) {
    mBeamTables[index].aboveCross = GetAboveCrossover(mBeamTables[index].spotSize,
      (mBeamTables[index].minIntensity + mBeamTables[index].maxIntensity) / 2.,
      mBeamTables[index].probeMode);
  }
}

// Determine if a given intensity is above crossover.  Probe defaults to -1 for current
int CBeamAssessor::GetAboveCrossover(int spotSize, double intensity, int probe)
{
  if (probe < 0 || probe > 1)
    probe = mScope->GetProbeMode();
  return (mScope->GetCrossover(spotSize, probe) > 0. && 
    intensity > mScope->GetCrossover(spotSize, probe)) ? 1 : 0;
}

// Return index of spot table on given side of crossover or -1 if none
int CBeamAssessor::FindSpotTableIndex(int aboveCross, int probe)
{
  for (int i = 0; i < mNumSpotTables; i++)
    for (int spot = 1; spot <= mScope->GetNumSpotSizes(); spot++)
      if (mSpotTables[i].probeMode == probe && mSpotTables[i].ratio[spot] > 0. &&
        GetAboveCrossover(spot, mSpotTables[i].intensity[spot], probe) == aboveCross)
        return i;
  return -1;
}

// Find the table closest to containing the given intensity for the given spot size
int CBeamAssessor::FindBestTable(int spotSize, double startIntensity, int probe)
{
  int aboveCross, bestTable = -1;
  double minDist = 1.e10;
  BeamTable *btp;
  if (probe < 0 || probe > 1)
    probe = mScope->GetProbeMode();
  AssignCrossovers();
  aboveCross = GetAboveCrossover(spotSize, startIntensity, probe);
  for (int index = 0; index < mNumTables; index++) {
    btp = &mBeamTables[index];
    if (btp->numIntensities && btp->aboveCross == aboveCross
      && btp->spotSize == spotSize && btp->probeMode == probe) {
      if (startIntensity >= btp->minIntensity &&
        startIntensity <= btp->maxIntensity) {
        minDist = 0.;
        bestTable = index;
        break;
      }
      if (startIntensity < btp->minIntensity && 
        btp->minIntensity - startIntensity < minDist) {
        minDist = btp->minIntensity - startIntensity;
        bestTable = index;
      } else if (startIntensity > btp->maxIntensity && 
        startIntensity - btp->maxIntensity < minDist) {
        minDist = startIntensity - btp->maxIntensity;
        bestTable = index;
      }
    }
  }

  return bestTable;
}

// Set the mIntensities and mLogCurrents variables for the given table and compute
// the extrapolation limits and the polarity of the table
int CBeamAssessor::SetTableAccessAndLimits(int bestTable, double &leftExtrapLimit, 
                                           double &rightExtrapLimit, double &diffSign)
{
  int nExtrapolate = 4; // How far to extrapolate: number of intensity intervals
  double rightIntensity;
  int numIntensities = mBeamTables[bestTable].numIntensities;
  int nExtrapLeft, nExtrapRight;
  nExtrapolate = B3DMIN(nExtrapolate, numIntensities - 1);
  nExtrapLeft = (mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_LOW) ? 0 :
    nExtrapolate;
  nExtrapRight = (mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_HIGH) ? 0 :
    nExtrapolate;
  mIntensities = &mBeamTables[bestTable].intensities[0];
  mLogCurrents = &mBeamTables[bestTable].logCurrents[0];
  rightIntensity = mIntensities[numIntensities - 1];
  leftExtrapLimit = 2. * mIntensities[0] - mIntensities[nExtrapLeft];
  rightExtrapLimit = 2. * rightIntensity - 
    mIntensities[numIntensities - 1 - nExtrapRight];
  diffSign = rightIntensity > mIntensities[0] ? 1. : -1.;
  return numIntensities;
}

// Tests whether the given intensity is in calibrated range for the given spotsize 
// and returns the polarity of the table there.  Returns 0 or BEAM error code
// If warnIfExtrap (default false) is true, it returns BEAM_ENDING_OUT_OF_RANGE if in the
// extrapolation region
int CBeamAssessor::OutOfCalibratedRange(double intensity, int spotSize, int probe,
                                        int &polarity, bool warnIfExtrap)
{
  int bestTable, numIntensities;
  double leftExtrapLimit, rightExtrapLimit, diffSign;
  if (!mNumTables)
    return BEAM_STRENGTH_NOT_CAL;
  if (probe < 0 || probe > 1)
    probe = mScope->GetProbeMode();

  bestTable = FindBestTable(spotSize, intensity, probe);
  if (bestTable < 0)
    return BEAM_STRENGTH_WRONG_SPOT;
  numIntensities = SetTableAccessAndLimits(bestTable, leftExtrapLimit, rightExtrapLimit,
    diffSign);
  polarity = B3DNINT(diffSign);
  if (diffSign * (intensity - leftExtrapLimit) < 0. ||
    diffSign * (intensity - rightExtrapLimit) > 0.)
    return BEAM_STARTING_OUT_OF_RANGE;
  if (warnIfExtrap && (diffSign * (intensity - mIntensities[0]) < 0. ||
    diffSign * (intensity - mIntensities[numIntensities - 1]) > 0.))
    return BEAM_ENDING_OUT_OF_RANGE;
  return 0;
}

#define MAX_FIT   10

// Change the beam strength by the given inFactor, or as much as possible in calibrated
// range; do it for given lowDoseArea or generally if lowDoseArea < 0
int CBeamAssessor::ChangeBeamStrength(double inFactor, int lowDoseArea)
{
  double newIntensity, newFactor;
  int err = AssessBeamChange(inFactor, newIntensity, newFactor, lowDoseArea);
  if (err && err != BEAM_ENDING_OUT_OF_RANGE)
    return err;

  // Set the intensity if regular mode or if the area is currently active
  if (lowDoseArea < 0 || mScope->GetLowDoseArea() == lowDoseArea) {
    if (!mScope->SetIntensity(newIntensity))
      return BEAM_STRENGTH_SCOPE_ERROR;
  }

  //Change area's value if there was no error; change focus/trial together if needed
  if (lowDoseArea >= 0) {
    mLDParam[lowDoseArea].intensity = newIntensity;
    if (mWinApp->mLowDoseDlg.m_bTieFocusTrial && (lowDoseArea + 1) / 2 == 1)
      mLDParam[3 - lowDoseArea].intensity = newIntensity;
  }
  //CString report;
  //report.Format("New intensity %.2f", 100. * newIntensity);
  //mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  return err;
}

// Assess whether beam can be changed by inFactor for a low dose area or generally
// (if lowDoseArea < 0).  The C2 intensity that can be achieved is returned in
// newIntensity, and the remaining factor by which it failed to change intensity 
// is returned in outFactor
int CBeamAssessor::AssessBeamChange(double inFactor, double &newIntensity, 
                  double &outFactor, int lowDoseArea)
{
  double startIntensity;
  int spotSize, probe;

  // Get spot size and intensity from scope unless in low dose, then use area value
  spotSize = lowDoseArea < 0 ? mScope->GetSpotSize() : mLDParam[lowDoseArea].spotSize;
  probe = lowDoseArea < 0 ? mScope->GetProbeMode() : mLDParam[lowDoseArea].probeMode;
  startIntensity = 
    lowDoseArea < 0 ? mScope->GetIntensity() : mLDParam[lowDoseArea].intensity;
  if (!startIntensity)
    return BEAM_STRENGTH_SCOPE_ERROR;
  return AssessBeamChange(startIntensity, spotSize, probe, inFactor, newIntensity, 
    outFactor);
}

int CBeamAssessor::AssessBeamChange(double startIntensity, int spotSize, int probe,
  double inFactor, double &newIntensity, double &outFactor)
{
  int nFit = 5;     // Number of points to fit for extrapolation
  float a, b, con, left, right, logLimit, intensityLimit;
  double startLog, newLog, rightLog;
  int index, indStart, bestTable, numIntensities, dontExtrap;
  double leftExtrapLimit, rightExtrapLimit;
  int retval = 0;
  int aboveCross;
  double diffSign;
  outFactor = 1.;

  //CString report;
  //report.Format("STart intensity %.2f", 100. * startIntensity);
  //mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  
  // First make sure there is a calibration at this spot size and find closest table
  if (!mNumTables)
    return BEAM_STRENGTH_NOT_CAL;

  bestTable = FindBestTable(spotSize, startIntensity, probe);
  if (bestTable < 0)
    return BEAM_STRENGTH_WRONG_SPOT;
  aboveCross = GetAboveCrossover(spotSize, startIntensity, probe);

  // Enter a loop with starting intensity value, desired change and best table
  for (;;) {

    numIntensities = SetTableAccessAndLimits(bestTable, leftExtrapLimit, 
      rightExtrapLimit, diffSign);

    // Look up this intensity in the table
    if (!LookupCurrent(numIntensities, startIntensity, startLog)) {

      // If not found, figure out if on left or right end
      if (diffSign * (startIntensity - mIntensities[0]) < 0.) {

        // On left: check not too far out
        if (diffSign * (startIntensity - leftExtrapLimit) < 0.)
          return BEAM_STARTING_OUT_OF_RANGE;
        indStart = 0;
      } else {
        
        // On right
        if (diffSign * (startIntensity - rightExtrapLimit) > 0.)
          return BEAM_STARTING_OUT_OF_RANGE;
        indStart = numIntensities - nFit;
      }

      // Fit points and get fitted starting log current
      FitCurrentCurve(indStart, nFit, a, b, con);
      startLog = a * startIntensity * startIntensity + b * startIntensity + con;
    }
    newLog = startLog + log(inFactor);

    // If the new log current is in range of this table, look it up 
    if (newLog >= mLogCurrents[0] && newLog <= mLogCurrents[numIntensities - 1]) {
      for (index = 0; index < numIntensities - 1; index++) {
        left = mLogCurrents[index];
        right = mLogCurrents[index + 1];
        if (newLog >= left && newLog <= right) {
          newIntensity = mIntensities[index] + (mIntensities[index + 1] -
            mIntensities[index]) * (newLog - left) / (right - left);
          return 0; 
        }
      }
    }

    // Get the extreme intensity in this table and use that to find overlapping table
    if (newLog < mLogCurrents[0]) {
      intensityLimit = (float)mIntensities[0];
      logLimit = mLogCurrents[0];
      dontExtrap = mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_LOW;
    } else {
      intensityLimit = (float)mIntensities[numIntensities - 1];
      logLimit = mLogCurrents[numIntensities - 1];
      dontExtrap = mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_HIGH;
    }

    // But if extrapolation is not allowed, return the limiting
    // intensity and the remaining factor to change
    if (dontExtrap) {
      newIntensity = intensityLimit;
      outFactor = inFactor / exp(logLimit - startLog);
      return BEAM_ENDING_OUT_OF_RANGE;
    }

    for (index = 0; index < mNumTables; index++) {
      if (index != bestTable && mBeamTables[index].numIntensities &&
        mBeamTables[index].spotSize == spotSize && mBeamTables[index].probeMode == probe
        && mBeamTables[index].aboveCross == aboveCross &&
        intensityLimit >= mBeamTables[index].minIntensity &&
        intensityLimit <= mBeamTables[index].maxIntensity) {
        
        // Found a table: then set the intensity to the limit and reduce the
        // needed change, and loop on new table
        startIntensity = intensityLimit;
        inFactor /= exp(logLimit - startLog);
        bestTable = index;
        break;
      }
    }
    if (index < mNumTables)
      continue;

    // Otherwise, we need to extrapolate from the current table
    // Determine if off right or left end
    rightLog = mLogCurrents[numIntensities - 1];
    if (newLog < mLogCurrents[0])
      indStart = 0;
    else
      indStart = numIntensities - nFit;
      
    // Get parabola fit to endpoints and compute intensity at newLog
    FitIntensityCurve(indStart, nFit, a, b, con);
    newIntensity = a * newLog * newLog + b * newLog + con;

    // If past either extrapolation limit, just take it to the limit
    if ((indStart && diffSign * (newIntensity - rightExtrapLimit) > 0.) ||
      (!indStart && diffSign * (newIntensity - leftExtrapLimit) < 0.)) {
      newIntensity = intensityLimit;
      outFactor = inFactor / exp(logLimit - startLog);

      // But if that takes it farther from where we started, then just stay at the
      // starting intensity
      if ((inFactor >= 1. && outFactor > inFactor) || 
        (inFactor < 1. && outFactor < inFactor)) {
          newIntensity = startIntensity;
          outFactor = inFactor;
      }
      retval = BEAM_ENDING_OUT_OF_RANGE;
    }
    return retval;
  }
}

void CBeamAssessor::FitIntensityCurve(int indStart, int nFit, float &a, float &b,
                    float &con)
{
  float xsq[MAX_FIT], tmpInt[MAX_FIT];
  int i;
  for (i = 0; i < nFit; i++) {
    xsq[i] = mLogCurrents[i + indStart] * mLogCurrents[i + indStart];
    tmpInt[i] = (float)mIntensities[i + indStart];
  }
  StatLSFit2(xsq, &mLogCurrents[indStart], tmpInt, nFit, &a, &b, &con);
}

void CBeamAssessor::FitCurrentCurve(int indStart, int nFit, float &a, float &b,
                    float &con)
{
  float xsq[MAX_FIT], tmpInt[MAX_FIT];
  int i;
  for (i = 0; i < nFit; i++) {
    tmpInt[i] = (float)mIntensities[i + indStart];
    xsq[i] = tmpInt[i] * tmpInt[i];
  }
  StatLSFit2(xsq, tmpInt, &mLogCurrents[indStart], nFit, &a, &b, &con);
}


BOOL CBeamAssessor::IntensityCalibrated()
{
  // This could get fancy, but for now just make sure there is a table at 
  // the current spot size on same side of crossover
  int spotSize, ldArea, aboveCross, probe;
  double intensity;
  AssignCrossovers();
  if (mWinApp->LowDoseMode() && (ldArea = mScope->GetLowDoseArea()) >= 0) {
    spotSize = mLDParam[ldArea].spotSize;
    intensity = mLDParam[ldArea].intensity;
    probe = mLDParam[ldArea].probeMode;
  } else {
    spotSize = mScope->GetSpotSize();
    intensity = mScope->GetIntensity();
    probe = mScope->GetProbeMode();
  }
  aboveCross = GetAboveCrossover(spotSize, intensity, probe);
  for (int i = 0; i < mNumTables; i++)
    if (mBeamTables[i].numIntensities > 0 && mBeamTables[i].spotSize == spotSize &&
      mBeamTables[i].aboveCross == aboveCross && mBeamTables[i].probeMode == probe)
      return true;
  return false;
}

// Look up a current in the current table at the given intensity; return true if succeed
BOOL CBeamAssessor::LookupCurrent(int numIntensities, double inIntensity, double &newLog)
{
  double diffSign = mIntensities[numIntensities - 1] > mIntensities[0] ? 1. : -1.;
  int index;
  double left, right;

  for (index = 0; index < numIntensities - 1; index++) {
    left = mIntensities[index];
    right = mIntensities[index + 1];
    if (inIntensity >= left && inIntensity <= right || 
      inIntensity <= left && inIntensity >= right) {
      newLog = mLogCurrents[index] + (mLogCurrents[index + 1] - mLogCurrents[index])
        * (inIntensity - left) / (right - left);
      break;
    }
  }
  return index < numIntensities - 1;
}

// Compute electron dose for the given spot size, intensity, and exposure time
double CBeamAssessor::GetElectronDose(int inSpotSize, double inIntensity, 
                                      float exposure, int probe)
{
  double cumFactor = 1.;
  double startIntensity;
  int index,  idiff, idir, spotSize, indTab;
  if (probe < 0 || probe > 1)
    probe = mScope->GetProbeMode();
  int aboveCross = GetAboveCrossover(inSpotSize, inIntensity, probe);
  DoseTable *dtp = &mDoseTables[inSpotSize][aboveCross][probe];

  // If calibration exists at this spot size, use it
  AssignCrossovers();
  if (dtp->dose) {
    cumFactor = FindCurrentRatio(dtp->intensity, inIntensity, inSpotSize, aboveCross,
      probe);
    if (!cumFactor)
      SEMTrace('d', "(spot %d int. %f  aC. %d  pM %d)", inSpotSize, inIntensity, 
      aboveCross, probe);
    return cumFactor * exposure * dtp->dose;
  }
        
  // To use calibration at another spot, first find proper spot table and give up
  // if none or no ratio for this spot size
  indTab = FindSpotTableIndex(aboveCross, probe);
  if (indTab < 0 || !mSpotTables[indTab].ratio[inSpotSize]) {
    SEMTrace('d', "GetElectronDose: No spot tables or no ratio on this side of crossover "
     "(spot %d int. %f  aC. %d  pM. %d  indTab %d)", inSpotSize, inIntensity, aboveCross,
     probe, indTab);

    return 0.;
  }

  // Search for closest calibrated spot size where both spots have intensity factors
  // and set up cumulative factor as ratio of spot intensities
  spotSize = -1;
  for (idiff = 1; idiff < mScope->GetNumSpotSizes() && spotSize < 0; idiff++) {
    for (idir = 1; idir >= -1; idir -= 2) {
      index = inSpotSize + idir * idiff;
      if (index >= 1 && index <= mScope->GetNumSpotSizes() && 
        mSpotTables[indTab].ratio[index] && mSpotTables[indTab].ratio[inSpotSize] &&
        mDoseTables[index][aboveCross][probe].dose) {
        spotSize = index;
        cumFactor = mSpotTables[indTab].ratio[inSpotSize] / mSpotTables[indTab].ratio[index];
        break;
      }
    }
  }

  if (spotSize < 0) {
    SEMTrace('d', "GetElectronDose: No other calibrated spot in spot tables this side of "
      "crossover (spot %d int. %f  aC. %d)", inSpotSize, inIntensity, aboveCross);
    return 0.;
  }

  // Get the cumulative factor for going from calibrated intensity to intensity that
  // ratios were done at, at the calibrated spot size, then to current spot size, then
  // from there to the current intensity
  startIntensity = mDoseTables[spotSize][aboveCross][probe].intensity;
  cumFactor *= FindCurrentRatio(startIntensity, mSpotTables[indTab].intensity[spotSize],
    spotSize, aboveCross, probe);
  cumFactor *= FindCurrentRatio(mSpotTables[indTab].intensity[inSpotSize], inIntensity,
    inSpotSize, aboveCross, probe);
  if (!cumFactor)
    SEMTrace('d', "(spot %d int. %f  aC. %d  pM %d)", inSpotSize, inIntensity, 
      aboveCross, probe);

  return cumFactor * exposure * mDoseTables[spotSize][aboveCross][probe].dose;
}

// Return the factor for change in current from startIntensity to inIntensity
double CBeamAssessor::FindCurrentRatio(double startIntensity, double inIntensity, 
                                       int spotSize, int aboveCross, int probe)
{
  double cumFactor = 1.;
  double dist, dist2, minDist;
  int nFit = 5;     // Number of points to fit for extrapolation
  float a, b, con, logLimit, intensityLimit;
  double startLog, newLog, diffSign;
  int index, indStart, bestTable, numIntensities, dontExtrap;
  double leftExtrapLimit, rightExtrapLimit;

  // Search for the best table containing the calibrated intensity
  bestTable = -1;
  minDist = 1.e10;
  for (index = 0; index < mNumTables; index++) {
    if (mBeamTables[index].numIntensities && mBeamTables[index].spotSize == spotSize &&
      mBeamTables[index].aboveCross == aboveCross && mBeamTables[index].probeMode == probe
      && startIntensity >= mBeamTables[index].minIntensity &&
      startIntensity <= mBeamTables[index].maxIntensity) {
      
      // Found a table containing the start intensity
      // If it contains the target, this is the best table for sure
      if (inIntensity >= mBeamTables[index].minIntensity &&
        inIntensity <= mBeamTables[index].maxIntensity) {
        minDist = 0.;
        bestTable = index;
        break;
      }
      
      // Otherwise get distance of target from endpoints of table, best table is
      // the one that is closest to the target
      dist = fabs(inIntensity - mBeamTables[index].minIntensity);
      dist2 = fabs(inIntensity - mBeamTables[index].maxIntensity);
      if (dist > dist2)
        dist = dist2;
      if (dist < minDist) {
        minDist = dist;
        bestTable = index;
      }
    }
  }
  
  if (bestTable < 0) {
    SEMTrace('d', "FindCurrentRatio: no table for intensities %f %f", startIntensity, 
      inIntensity);
    return 0.;
  }
  
  // Start a loop with best table, starting intensity and intensity to reach
  for (;;) {
    numIntensities = SetTableAccessAndLimits(bestTable, leftExtrapLimit, 
      rightExtrapLimit, diffSign);

    // Look up starting intensity in the table
    LookupCurrent(numIntensities, startIntensity, startLog);

    // If the target is in the table, look it up and finish
    if (LookupCurrent(numIntensities, inIntensity, newLog)) {
       cumFactor *= exp(newLog - startLog);
       return cumFactor;
    }

    // Get the extreme intensity in this table and use that to find overlapping table
    if (diffSign * (inIntensity - mIntensities[0]) < 0.) {
      intensityLimit = (float)mIntensities[0];
      logLimit = mLogCurrents[0];
      dontExtrap = mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_LOW;
      indStart = 0;
    } else {
      intensityLimit = (float)mIntensities[numIntensities - 1];
      logLimit = mLogCurrents[numIntensities - 1];
      dontExtrap = mBeamTables[bestTable].dontExtrapFlags & BEAM_DONT_EXTRAP_HIGH;
      indStart = numIntensities - nFit;
    }

    // But if extrapolation is not allowed, no good.
    if (dontExtrap) {
      SEMTrace('d', "FindCurrentRatio: Cannot extrapolate %f - %f to %f", mIntensities[0],
        mIntensities[numIntensities - 1], inIntensity);
      return 0.;
    }

    for (index = 0; index < mNumTables; index++) {
      if (index != bestTable && mBeamTables[index].numIntensities &&
        mBeamTables[index].spotSize == spotSize &&
        mBeamTables[index].aboveCross == aboveCross && 
        mBeamTables[index].probeMode == probe &&
        intensityLimit >= mBeamTables[index].minIntensity &&
        intensityLimit <= mBeamTables[index].maxIntensity) {
        
        // Found a table: then set the intensity to the limit and accumulate the
        // change in current, and loop on new table
        startIntensity = intensityLimit;
        cumFactor *= exp(logLimit - startLog);
        bestTable = index;
        break;
      }
    }
    if (index < mNumTables)
      continue;

    // Otherwise, we need to extrapolate from the current table
    // Unless past either extrapolation limit,
    if ((indStart && diffSign * (inIntensity - rightExtrapLimit) > 0.) ||
      (!indStart && diffSign * (inIntensity - leftExtrapLimit) < 0.)) {
      SEMTrace('d', "FindCurrentRatio: Cannot extrapolate to %f (ind %d diff %f rel %f"
        " lel %f)", inIntensity, indStart, diffSign, leftExtrapLimit,
        rightExtrapLimit);
      return 0.;
    }
      
    // Get parabola fit to endpoints and compute current at target
    FitCurrentCurve(indStart, nFit, a, b, con);
    newLog = a * inIntensity * inIntensity + b * inIntensity + con;
    cumFactor *= exp(newLog - startLog);
    return cumFactor;
  }
  return 0.;
}

// Get the dose of the given conset under current conditions - UNUSED
double CBeamAssessor::GetCurrentElectronDose(int conSetNum)
{
  ControlSet *conSet = mWinApp->GetConSets() + conSetNum;
  int spotSize = mScope->GetSpotSize();
  double intensity = mScope->GetIntensity();
  return GetElectronDose(spotSize, intensity, 
    mCamera->SpecimenBeamExposure(mWinApp->GetCurrentCamera(), conSet));
}

// Return electron dose for the given camera and set number, at the current spotsize
// and intensity, which are returned as well
double CBeamAssessor::GetCurrentElectronDose(int camera, int setNum, int &spotSize, 
                                             double &intensity)
{
  ControlSet *conSet = mWinApp->GetCamConSets() + setNum + camera * MAX_CONSETS;
  return GetCurrentElectronDose(camera, setNum, conSet->exposure, conSet->drift, spotSize,
    intensity);
}

double CBeamAssessor::GetCurrentElectronDose(int camera, int setNum, float csExposure, 
                                             float csDrift, int &spotSize, 
                                             double &intensity)
{
  LowDoseParams *ldParam = mWinApp->GetCamLowDoseParams();
  int set, GIF, probe;
  CameraParameters *camParam = mWinApp->GetCamParams();
  
  // Need to synchronize back to camera LDP since we are accessing them
  mWinApp->CopyCurrentToCameraLDP();
  float exposure = mCamera->SpecimenBeamExposure(camera, csExposure, csDrift);

  if (mWinApp->LowDoseMode()) {
    GIF = camParam[camera].GIF ? 1 : 0;
    set = mCamera->ConSetToLDArea(setNum);
    spotSize = ldParam[GIF * MAX_LOWDOSE_SETS + set].spotSize;
    intensity = ldParam[GIF * MAX_LOWDOSE_SETS + set].intensity;
    probe = ldParam[GIF * MAX_LOWDOSE_SETS + set].probeMode;
  } else {
    spotSize = mWinApp->mScope->GetSpotSize();
    intensity = mWinApp->mScope->GetIntensity();
    probe = mWinApp->mScope->ReadProbeMode();
  }

  return mWinApp->mBeamAssessor->GetElectronDose(spotSize, intensity, exposure, probe);
}

// Calibrate the dose from the current image
void CBeamAssessor::CalibrateElectronDose(BOOL interactive)
{
  double intensity = mScope->GetIntensity();
  int spotSize = mScope->GetSpotSize();
  int indTab, i;
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  CameraParameters *camParam = mWinApp->GetCamParams() + imBuf->mCamera;
  ControlSet *allConSets = mWinApp->GetCamConSets();
  int probe = mScope->ReadProbeMode();
  int aboveCross = GetAboveCrossover(spotSize, intensity, probe); 
  DoseTable *dtp = &mDoseTables[spotSize][aboveCross][probe];
  double saveIntensity = dtp->intensity;
  double saveDose = dtp->dose;
  double mean, pixel;
  CString message, addon;
  float doseRate;
  EMimageExtra *extra;

  if (!(imBuf->mImage && imBuf->mBinning && (imBuf->mCaptured == 1 || 
    imBuf->mCaptured == BUFFER_CALIBRATION) && imBuf->mMagInd)) {
    if (interactive)
      AfxMessageBox("To calibrate electron dose, Buffer A must contain an \n"
      "image acquired from a camera with calibrated gain.\n\n"
      "To proceed, acquire an image.", MB_EXCLAME);
    return;
  }
  extra = imBuf->mImage->GetUserData();

  if (!camParam->countsPerElectron) {
    if (interactive)
      AfxMessageBox("The SerialEM properties file does not contain a gain in \n"
      "counts per electron for the camera from which this image was taken\n\n"
      "Dose cannot be calibrated with this image", MB_EXCLAME);
    return;
  }

  if (imBuf->mCamera >= 0 && imBuf->mConSetUsed >= 0 && camParam->OneViewType &&
    allConSets[imBuf->mConSetUsed + imBuf->mCamera * MAX_CONSETS].correctDrift) {
    if (AfxMessageBox("It appears that this image was taken with drift correction "
      "turned on.\n\nBlank images with drift correction can lose frames and give"
      " a bad dose estimate.\n\nDo you want to go take another image without this"
      " setting?", MB_QUESTION) == IDYES)
      return;
  }

  if (extra && extra->m_fIntensity > -1.)
    intensity = extra->m_fIntensity;
  if (extra && extra->mSpotSize > 0)
    spotSize = extra->mSpotSize;

  // Go ahead and compute the dose and set it so that the intensity can be checked
  mean = mWinApp->mProcessImage->WholeImageMean(imBuf);
  mWinApp->mProcessImage->DoseRateFromMean(imBuf, (float)mean, doseRate);
  pixel = 10000. * BinDivisorI(camParam) * 
    mShiftManager->GetPixelSize(imBuf->mCamera, imBuf->mMagInd);

  dtp->dose = doseRate / (pixel * pixel);
  dtp->intensity = intensity;

  if (GetElectronDose(spotSize, intensity, imBuf->mExposure)) {
    if (!interactive || AfxMessageBox("To calibrate electron dose, Buffer A must contain"
      " an image\nof the blank beam (with no specimen).\n\n"
      "Does the image in Buffer A meet these requirements?", MB_YESNO | MB_ICONQUESTION)
      == IDYES) {
      message.Format("Dose rate calculated to be %.2f electrons/square Angstrom/sec "
        "for spot %d, %s %.2f%s", dtp->dose, spotSize,
        mScope->GetC2Name(), mScope->GetC2Percent(spotSize, intensity, probe), 
        mScope->GetC2Units());
      if (mCamera->IsDirectDetector(camParam)) {
        addon.Format("\r\n    %.2f electrons/unbinned pixel/sec", doseRate);
        message += addon;
      }
      mWinApp->AppendToLog(message, 
        interactive ? LOG_MESSAGE_IF_CLOSED : LOG_SWALLOW_IF_CLOSED);
      int timeStamp = mWinApp->MinuteTimeStamp();
      dtp->timeStamp = timeStamp;
      dtp->currentAperture = 0;

      // Get an aperture size and scale tables
      if (mScope->GetUseIllumAreaForC2()) {
        i = RequestApertureSize();
        if (i) {
          dtp->currentAperture = i;
          ScaleTablesForAperture(i, false);
        }
      }
      mWinApp->mDocWnd->SetShortTermNotSaved();

      // Purge older calibrations that are linked by spot ratios
      indTab = FindSpotTableIndex(aboveCross, probe);
      if (indTab < 0 || !mSpotTables[indTab].ratio[spotSize])
        return;
      for (i = 0; i < mScope->GetNumSpotSizes(); i++)
        if (i != spotSize && mSpotTables[indTab].ratio[i] && 
          mDoseTables[i][aboveCross][probe].dose && 
          timeStamp - mDoseTables[i][aboveCross][probe].timeStamp > 60 * AUTO_PURGE_HOURS)
          mDoseTables[i][aboveCross][probe].dose = 0.;

      return;
    }

  } else if (interactive) {
    AfxMessageBox("The beam intensity is outside the calibrated\n"
    "range for this spot size.\n\n"
    "You need to take a picture with brightness set\n"
    "in the calibrated range for this spot size.", MB_EXCLAME);
  } else

    // SHOULD CHECK ON WHETHER THERE IS CALIBRATION AT ALL FOR THIS SPOT
    mWinApp->AppendToLog("Unable to calibrate electron dose with this picture.\r\n"
      "The beam intensity is outside the calibrated range for this spot size.", 
      LOG_SWALLOW_IF_CLOSED);


  // Restore table entries
  dtp->dose = saveDose;
  dtp->intensity = saveIntensity;
}

//////////////////////////////////////////////////////////////////////////////
// CALIBRATE BEAM SHIFT
//
void CBeamAssessor::CalibrateBeamShift()
{
  int i;
  for (i = 0; i < 6; i++)
    mBSCshiftX[i] = mBSCshiftY[i] = 0.;

  CString *modeNames = mWinApp->GetModeNames();
  if (AfxMessageBox("Beam shift calibration takes pictures with the " 
    + modeNames[SHIFT_CAL_CONSET] + " parameter set.\n\n"
    "You must have the spot condensed so that its diameter is less\n"
    "than half of the size of a " + modeNames[SHIFT_CAL_CONSET] + " image.\n\n"
    "You should also choose a spot size that gives a moderate\n"
    "number of counts in a " + modeNames[SHIFT_CAL_CONSET] + " image.\n\n"
    "Are you ready to proceed?", MB_ICONQUESTION | MB_YESNO) == IDNO)
    return;
    

  mScope->GetBeamShift(mBaseShiftX, mBaseShiftY);
  mShiftCalIndex = 0;
  mCamera->InitiateCapture(SHIFT_CAL_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_BEAMSHIFT, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "CALIBRATING BEAM SHIFT");
  mWinApp->UpdateBufferWindows();
}

// Get an image from beam shift calibration
void CBeamAssessor::ShiftCalImage()
{
  double dx, dy, dist;
  int size;

  if (mShiftCalIndex < 0)
    return;

  // Get some general parameters about the image and scaling
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  int curCam = mWinApp->GetCurrentCamera();
  int magInd = imBuf->mMagInd;
  float pixel = mShiftManager->GetPixelSize(curCam, magInd);
  int binning = imBuf->mBinning;

  mUseEdgeForRefine = false;
  FindCenterForShiftCal(8, size);

  // FIRST SHOT WITH CENTERED BEAM
  if (!mShiftCalIndex) {


    // Set up the shifts as 1/30 of field since the scope alignment could be off by 10
    if (magInd < mScope->GetLowestMModeMagInd() && mScope->GetLMBeamShiftFactor())
      pixel /= (float)mScope->GetLMBeamShiftFactor();
    if (magInd >= mScope->GetLowestMModeMagInd() && JEOLscope && !mScope->GetHasNoAlpha())
      pixel *= mBSCalAlphaFactors[mScope->GetAlpha()];
    mBSCshiftX[0] = mBSCshiftY[1] = 0.03f * size * pixel * binning;
  }

  // THIRD SHOT - TWO SMALL SHIFTS DONE
  if (mShiftCalIndex == 2) {
    // Get distance moved from X, and increase X shift to make it 1/6 of field
    dx = mBSCcenX[1] - mBSCcenX[0];
    dy = mBSCcenY[1] - mBSCcenY[0];
    dist = sqrt(dx * dx + dy * dy);
    mBSCshiftX[2] = (float)(mBSCshiftX[0] * (size / 6.) / dist);
    mBSCshiftX[3] = - mBSCshiftX[2];

    // DO the same for Y shifts
    dx = mBSCcenX[2] - mBSCcenX[0];
    dy = mBSCcenY[2] - mBSCcenY[0];
    dist = sqrt(dx * dx + dy * dy);
    mBSCshiftY[4] = (float)(mBSCshiftY[1] * (size / 6.) / dist);
    mBSCshiftY[5] = - mBSCshiftY[4];
  }

  // If not done, set up next shift and shot
  if (mShiftCalIndex < 6) {
    mScope->SetBeamShift(mBaseShiftX + mBSCshiftX[mShiftCalIndex], 
      mBaseShiftY + mBSCshiftY[mShiftCalIndex]);
    mShiftCalIndex++;
    mCamera->InitiateCapture(SHIFT_CAL_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_BEAMSHIFT, 0, 0);
    return;
  }

  // DONE: COMPUTE CALIBRATION MATRIX - invert Y at this point
  ScaleMat bsToCam;
  bsToCam.xpx = (float)(0.5 * binning * (mBSCcenX[4] - mBSCcenX[3]) / mBSCshiftX[3]);
  bsToCam.ypx = (float)(0.5 * binning * (mBSCcenY[3] - mBSCcenY[4]) / mBSCshiftX[3]);
  bsToCam.xpy = (float)(0.5 * binning * (mBSCcenX[6] - mBSCcenX[5]) / mBSCshiftY[5]);
  bsToCam.ypy = (float)(0.5 * binning * (mBSCcenY[5] - mBSCcenY[6]) / mBSCshiftY[5]);
  
  // Convert to a matrix for getting from image shift to beam shift
  ScaleMat bMat = mShiftManager->IStoCamera(magInd);
  ScaleMat camToBS = mShiftManager->MatInv(bsToCam);
  mIStoBScal = mShiftManager->MatMul(bMat, camToBS);
  StoreBeamShiftCal(magInd, 0);
}

// Find the beam center for either kind fo shift calibration
int CBeamAssessor::FindCenterForShiftCal(int edgeDivisor, int &size)
{
  double sum;
  int ix0, ix1, iy0, iy1, nPix, nPixTot, binning, err;
  float xcen, ycen, radius, xcenUse, ycenUse, radUse, fracUse, shiftX, shiftY, fitErr;
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  imBuf->mCaptured = BUFFER_CALIBRATION;
  KImage *image = imBuf->mImage;
  int nx = image->getWidth();
  int ny = image->getHeight();
  void *data = image->getData();
  int type = image->getType();
  size = B3DMIN(nx, ny);

  if (mUseEdgeForRefine) {

    // Find beam center when refining
    err = mWinApp->mProcessImage->FindBeamCenter(imBuf, xcen, ycen, radius, xcenUse, 
      ycenUse, radUse, fracUse, binning, ix0, shiftX, shiftY, fitErr);
    if (err) {
      if (err < 0)
        mWinApp->AppendToLog("No beam edges detectable in this image");
      else
        mWinApp->AppendToLog("Error analyzing image for beam edges");
      return 1;
    }
    xcen *= binning;
    ycen *= binning;
    mBSCcenX[mShiftCalIndex] = xcen;
    mBSCcenY[mShiftCalIndex] = ycen;
    PrintfToLog("Position %d: center at %.0f %.0f, radius %.0f, fit error %.3f",
      mShiftCalIndex, xcen, ycen, radius * binning, fitErr * binning);

  } else {

    // Or get the centroid
    image->Lock();

    // FIRST SHOT WITH CENTERED BEAM
    if (!mShiftCalIndex) {

      // Compute the mean around the outer fraction of field
      ix0 = nx / edgeDivisor;
      ix1 = nx - ix0;
      iy0 = ny / edgeDivisor;
      iy1 = ny - iy0;
      nPix = ny * ix0;
      sum = nPix * ProcImageMean(data, type, nx, ny, 0, ix0 - 1, 0, ny - 1);
      sum += nPix * ProcImageMean(data, type, nx, ny, ix1, nx - 1, 0, ny - 1);
      nPixTot = 2 * nPix;
      nPix = (nx - 2 * ix0) * iy0;
      sum += nPix * ProcImageMean(data, type, nx, ny, ix0, ix1 - 1, 0, iy0 - 1);
      sum += nPix * ProcImageMean(data, type, nx, ny, ix0, ix1 - 1, iy1, ny - 1);
      nPixTot += 2 * nPix;
      mBorderMean = sum / nPixTot;

      // Get the centroid of beam
      ProcCentroid(data, type, nx, ny, ix0, ix1, iy0, iy1, mBorderMean, mBSCcenX[0],
        mBSCcenY[0]);
    } else {
      // Every other shot, get the centroid
      ProcCentroid(data, type, nx, ny, 0, nx - 1, 0, ny - 1, mBorderMean,
        mBSCcenX[mShiftCalIndex], mBSCcenY[mShiftCalIndex]);
    }
    SEMTrace('1', "Position %d: center at %.0f %.0f", mShiftCalIndex, 
      mBSCcenX[mShiftCalIndex], mBSCcenY[mShiftCalIndex]);
    image->UnLock();
  }
  return 0;
}

// Store the beam shift calibration for the given mag with the given retain flag
void CBeamAssessor::StoreBeamShiftCal(int magInd, int retain)
{
  CString report;

  // Pass a negative mag index for JEOL EFTEM mode
  if (JEOLscope && !mScope->GetHasOmegaFilter() && mWinApp->GetEFTEMMode())
    magInd = -magInd;
  mShiftManager->SetBeamShiftCal(mIStoBScal, magInd, mScope->GetAlpha(),
    mScope->ReadProbeMode(), retain);
  if (!mRefiningShiftCal) {
    report.Format("Image shift to beam shift matrix is:\r\n  %.3f  %.3f  %.3f  %.3f",
      mIStoBScal.xpx, mIStoBScal.xpy, mIStoBScal.ypx, mIStoBScal.ypy);
    mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
  }
  StopShiftCalibration();
  mWinApp->SetCalibrationsNotSaved(true);
}

// Common cleanup routine for either shift cal
void CBeamAssessor::ShiftCalCleanup(int error)
{
  CString str;
  str.Format("Time out trying to get image for %s beam shift calibration",
    mRefiningShiftCal ? "refining" : "");
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(str, MB_EXCLAME);
  StopShiftCalibration();
  mWinApp->ErrorOccurred(error);
}

/*
 *  End or stop either shift calibration: restore beam shift and image shift if refining
 */
void CBeamAssessor::StopShiftCalibration()
{
  if (mShiftCalIndex < 0)
    return;
  if (mRefiningShiftCal)
    mScope->SetImageShift(mBaseISX, mBaseISY);
  mScope->SetBeamShift(mBaseShiftX, mBaseShiftY);
  mShiftCalIndex = -1;
  mRefiningShiftCal = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Start refining the beam shift calibration by image shifting and finding the error
// in the beam shift (the movement of the beam)
void CBeamAssessor::RefineBeamShiftCal()
{
  CString *modeNames = mWinApp->GetModeNames();
  int which;

  // So far this is called only from MenuTargets which checks all three cals needed
  mIStoBScal = mShiftManager->GetBeamShiftCal(mScope->GetMagIndex());
  which = SEMThreeChoiceBox("Beam shift calibration refinement takes pictures with the\n"
    + modeNames[SHIFT_CAL_CONSET] + " parameter set.  It can find the beam center either"
    "\nfrom the centroid of the image or from detected beam edges.\n\n"
    "You should also choose a spot size that gives a moderate\n"
    "number of counts in a " + modeNames[SHIFT_CAL_CONSET] + " image.\n\n"
    "Press:\n\"Use Centroid\" to use the centroid of the image above\n"
    "background.  You must have the spot condensed so that it is\n"
    "less than 3/4 of the size of a " + modeNames[SHIFT_CAL_CONSET] + " image and stays"
    " within the image when image shift is applied.\n\n"
    "\"Use Edge\" to use the beam edge.  Almost all of the beam\n"
    "edge should be in the image for accurate measurement.\n\n"
    "\"Cancel\" to abort the procedure",
    "Use Centroid", "Use Edge", "Cancel", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (which == IDCANCEL)
    return;
  mUseEdgeForRefine = which == IDNO;
  if (!KGetOneFloat("Enter maximum image shift to apply, in microns", mRBSCmaxShift, 1))
    return;

  // 
  mScope->GetBeamShift(mBaseShiftX, mBaseShiftY);
  mScope->GetImageShift(mBaseISX, mBaseISY);
  SEMTrace('1', "Base IS %.2f %.2f  Base BS %.1f %.1f", mBaseISX, mBaseISY, mBaseShiftX, mBaseShiftY);
  PrintfToLog("Starting IS to BS matrix %.4f %.4f %.4f %.4f", mIStoBScal.xpx, 
    mIStoBScal.xpy, mIStoBScal.ypx, mIStoBScal.ypy);
  mShiftCalIndex = 0;
  mRefiningShiftCal = true;
  mCamera->InitiateCapture(SHIFT_CAL_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_REFINE_BS_CAL, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "REFINING BEAM SHIFT CAL");
  mWinApp->UpdateBufferWindows();
}

// Get the next shot in the refinement routine
void CBeamAssessor::RefineShiftCalImage()
{
  double dx, dy;
  int size, ind, dir;
  float shiftInUm, delISX, delISY, delSpecX, delSpecY, dist;
  float tooBigCrit = 0.5f;
  ScaleMat IStoSpec, BStoIS, camToIS, newIStoBS;

  if (mShiftCalIndex < 0)
    return;

  // Get some general parameters about the image and scaling
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  int curCam = mWinApp->GetCurrentCamera();
  int magInd = imBuf->mMagInd;
  float pixel = mShiftManager->GetPixelSize(curCam, magInd);
  int binning = imBuf->mBinning;

  // Get center position, and record shifts for this position
  FindCenterForShiftCal(16, size);
  if (mShiftCalIndex > 0) {
    ind = mShiftCalIndex - 1;
    mScope->GetImageShift(dx, dy);
    mRBSCshiftISX[ind] = (float)(dx - mBaseISX);
    mRBSCshiftISY[ind] = (float)(dy - mBaseISY);
    mScope->GetBeamShift(dx, dy);
    mBSCshiftX[ind] = (float)(dx - mBaseShiftX);
    mBSCshiftY[ind] = (float)(dy - mBaseShiftY);

    // Adjust image shift by the shift that would be needed to move the image by the
    // same amount as the beam has moved (Y inversion here)
    camToIS = mShiftManager->CameraToIS(magInd);
    mShiftManager->ApplyScaleMatrix(camToIS, 
      binning * (mBSCcenX[mShiftCalIndex] - mBSCcenX[0]),
      -binning * (mBSCcenY[mShiftCalIndex] - mBSCcenY[0]), delISX, delISY);
    SEMTrace('1', "At IS %.2f %.2f, IS needed to center %.2f %.2f", mRBSCshiftISX[ind], 
      mRBSCshiftISY[ind], delISX, delISY);
    mRBSCshiftISX[ind] += delISX;
    mRBSCshiftISY[ind] += delISY;
  }

  // On the fifth and last images, take the last set of measurements and use it to get a
  // new calibration matrix
  if (mShiftCalIndex == 4 || mShiftCalIndex == 8 || mShiftCalIndex == 12) {
    ind = mShiftCalIndex - 4;
    BStoIS.xpx = (mRBSCshiftISX[ind] - mRBSCshiftISX[ind + 1]) /
      (mBSCshiftX[ind] - mBSCshiftX[ind + 1]);
    BStoIS.xpy = (mRBSCshiftISX[ind + 2] - mRBSCshiftISX[ind + 3]) /
      (mBSCshiftY[ind + 2] - mBSCshiftY[ind + 3]);
    BStoIS.ypx = (mRBSCshiftISY[ind] - mRBSCshiftISY[ind + 1]) /
      (mBSCshiftX[ind] - mBSCshiftX[ind + 1]);
    BStoIS.ypy = (mRBSCshiftISY[ind + 2] - mRBSCshiftISY[ind + 3]) /
      (mBSCshiftY[ind + 2] - mBSCshiftY[ind + 3]);
    newIStoBS = MatInv(BStoIS);
    PrintfToLog("Refined matrix to %.4f %.4f %.4f %.4f", newIStoBS.xpx, newIStoBS.xpy,
      newIStoBS.ypx, newIStoBS.ypy);

    // Make sure it hasn't changed too much
    dist = fabsf(newIStoBS.xpx);
    ACCUM_MAX(dist, fabsf(newIStoBS.xpy));
    ACCUM_MAX(dist, fabsf(newIStoBS.ypx));
    ACCUM_MAX(dist, fabsf(newIStoBS.ypy));
    if (fabs(newIStoBS.xpx - mIStoBScal.xpx) / dist > tooBigCrit ||
      fabs(newIStoBS.xpy - mIStoBScal.xpy) / dist > tooBigCrit ||
      fabs(newIStoBS.ypx - mIStoBScal.ypx) / dist > tooBigCrit ||
      fabs(newIStoBS.ypy - mIStoBScal.ypy) / dist > tooBigCrit) {
      AfxMessageBox("The new matrix has changed too much from the previous one\n\n"
        "Stopping because something seems to be wrong", MB_EXCLAME);
      StopShiftCalibration();
      return;
    }

    mIStoBScal = newIStoBS;
    
    // Finish up if done
    if (mShiftCalIndex == 12) {
      StoreBeamShiftCal(magInd, 1);
      return;
    }
  }

  // On the first and fifth images, take the existing cal matrix and use it to set up the
  // image shift vectors to apply
  if (!mShiftCalIndex || mShiftCalIndex == 4 || mShiftCalIndex == 8) {

    // Determine microns to move
    if (!mShiftCalIndex)
      shiftInUm = (float)B3DMIN(mRBSCmaxShift / 2., size * pixel * binning);
    else
      shiftInUm = mRBSCmaxShift;
    BStoIS = MatInv(mIStoBScal);
    IStoSpec = mShiftManager->IStoSpecimen(magInd);
    for (dir = 0; dir < 2; dir++) {

      // Get IS shift that one BS unit gives and convert to microns on specimen,
      // scale up the IS to give target shift
      mShiftManager->ApplyScaleMatrix(BStoIS, (float)(1 - dir), (float)dir, delISX,
        delISY);
      mShiftManager->ApplyScaleMatrix(IStoSpec, delISX, delISY, delSpecX, delSpecY);
      dist = (float)sqrt(delSpecX * delSpecX + delSpecY * delSpecY);
      delISX *= shiftInUm / dist;
      delISY *= shiftInUm / dist;

      // Store these image shifts for the + and - direction on this axis
      mRBSCshiftISX[mShiftCalIndex + 2 * dir] = delISX;
      mRBSCshiftISY[mShiftCalIndex + 2 * dir] = delISY;
      mRBSCshiftISX[mShiftCalIndex + 2 * dir + 1] = -delISX;
      mRBSCshiftISY[mShiftCalIndex + 2 * dir + 1] = -delISY;
    }
  }

  // For any shot, apply the IS, read out the BS, and adjust BS to be 0 on one axis
  mScope->SetImageShift(mBaseISX + mRBSCshiftISX[mShiftCalIndex], 
    mBaseISY + mRBSCshiftISY[mShiftCalIndex]);
  if (mShiftCalIndex < 0) {

    // If IS was clipping, restore the center here, since the restore in the stop routine
    // was overridden by the clipped IS being set
    mScope->SetImageShift(mBaseISX, mBaseISY);
    mScope->SetBeamShift(mBaseShiftX, mBaseShiftY);
    return;
  }

  // Get the shift that should have been applied with the current matrix and make sure one
  // axis is 0
  mShiftManager->ApplyScaleMatrix(mIStoBScal, mRBSCshiftISX[mShiftCalIndex],
    mRBSCshiftISY[mShiftCalIndex], delISX, delISY);
  if (GetDebugOutput('1'))
    mScope->GetBeamShift(dx, dy);
  if (fabs(delISX) < fabs(delISY)) {
    SEMTrace('1', "Beam shift %.1f %.1f adjusting to %.1f %.1f",dx, dy, mBaseShiftX, 
      mBaseShiftY + delISY);
    mScope->SetBeamShift(mBaseShiftX, mBaseShiftY + delISY);
  } else {
    SEMTrace('1', "Beam shift  %.1f %.1f adjusting to %.1f %.1f", dx, dy, 
      mBaseShiftX + delISX, mBaseShiftY);
    mScope->SetBeamShift(mBaseShiftX + delISX, mBaseShiftY);
  }

  // Start next shot
  mShiftCalIndex++;
  mCamera->InitiateCapture(SHIFT_CAL_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_REFINE_BS_CAL, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////
// CALIBRATE SPOT INTENSITIES
//
void CBeamAssessor::CalibrateSpotIntensity()
{
  int dimmestSpot = HitachiScope ? mScope->GetMinSpotSize() : mScope->GetNumSpotSizes();
  mNumSpotsToCal = mScope->GetNumSpotSizes();
  mSpotCalStartSpot = mScope->GetSpotSize();
  ControlSet *conSet = mWinApp->GetConSets() + SPOT_CAL_CONSET;

  CString *modeNames = mWinApp->GetModeNames();
  CString message;
  if (mWinApp->LowDoseMode()) {
    AfxMessageBox("Spot intensity calibration cannot be done in Low Dose mode");
    return;
  }

  message.Format("Spot intensity calibration takes pictures with the %s" 
    " parameter set at a series of spot sizes.\n\n"
    "You must make sure that the beam is spread enough so that it will\nfill the frame"
    " of a %s image at all the spot sizes to be measured."
    "\n\nThe exposure time and brightness should be set so that you get a\nhigh number"
    " of counts (~2/3 of saturation) in a %s"
    " image at spot size %d.\n\nAre you ready to proceed?", 
    (LPCTSTR)modeNames[SPOT_CAL_CONSET], (LPCTSTR)modeNames[SPOT_CAL_CONSET],
    (LPCTSTR)modeNames[SPOT_CAL_CONSET], HitachiScope ? mNumSpotsToCal : 1);
  if (AfxMessageBox(message, MB_ICONQUESTION | MB_YESNO) == IDNO)
    return;

  for (int ind = 1; ind <= mNumSpotsToCal; ind++)
    mTempIntensities[ind] = mTempCounts[ind] = 0.;
  mSpotCalIndex = HitachiScope ? mNumSpotsToCal : mScope->GetMinSpotSize();
  if (!KGetOneInt("Brightest spot number to measure:", mSpotCalIndex))
    return;
  if (!KGetOneInt("Dimmest spot number to measure:", dimmestSpot))
    return;
  B3DCLAMP(mSpotCalIndex, mScope->GetMinSpotSize(), mNumSpotsToCal);
  B3DCLAMP(dimmestSpot, 1, mNumSpotsToCal);
  mNumSpotsToCal = B3DCHOICE(HitachiScope, mSpotCalIndex + 1 - dimmestSpot,
    dimmestSpot + 1 - mSpotCalIndex);
  if (mNumSpotsToCal < 2)
    return;

  mFirstSpotToCal = mSpotCalIndex;
  if (mSpotCalStartSpot != mSpotCalIndex) {
    if (SetAndCheckSpotSize(mSpotCalIndex, true))
      return;
  } else
    mScope->NormalizeCondenser();
  conSet->onceDark = 1;
  mCamera->InitiateCapture(SPOT_CAL_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_SPOT_INTENSITY, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "CALIBRATING SPOT INTENSITY");
  mWinApp->UpdateBufferWindows();
}

// Get the next image and process it
void CBeamAssessor::SpotCalImage(int param)
{
  int newIndex = mSpotCalIndex + (HitachiScope ? -1 : 1);
  int numDone =  1 + (mSpotCalIndex - mFirstSpotToCal) * (HitachiScope ? -1 : 1);
  if (!mSpotCalIndex)
    return;

  // Get image mean
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  imBuf->mCaptured = BUFFER_CALIBRATION;
  mTempIntensities[mSpotCalIndex] = mScope->GetIntensity();
  mTempCounts[mSpotCalIndex] = (float)mWinApp->mProcessImage->WholeImageMean(imBuf);

  // See if there is another spot to do. 
  // Upon stop, mSpotCalIndex will now point to the end of the acquired data
  if (numDone >= mNumSpotsToCal) {
    StopSpotCalibration();
    return;
  }

  // Start next spot with normalization
  mSpotCalIndex = newIndex;
  if (SetAndCheckSpotSize(mSpotCalIndex, true)) {
    StopSpotCalibration();
    return;
  }
  mCamera->InitiateCapture(SPOT_CAL_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_SPOT_INTENSITY, 0, 0);
}

void CBeamAssessor::SpotCalCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out trying to get image for spot intensity calibration"), 
      MB_EXCLAME);
  StopSpotCalibration();
  mWinApp->ErrorOccurred(error);
}

// Finish up spot calibration
void CBeamAssessor::StopSpotCalibration()
{
  int i, indTab, aboveCross, numDone, firstSpot, idir, lastSpot;
  BOOL saveData = true;
  float ratio;
  int probe = mScope->ReadProbeMode();
  CString report;
  if (!mSpotCalIndex)
    return;

  // Back up if it didn't actually do the last one
  if (!mTempIntensities[mSpotCalIndex] && !mTempCounts[mSpotCalIndex])
    mSpotCalIndex -= (HitachiScope ? -1 : 1);
  
  firstSpot = 1;
  idir = 1;
  numDone = mSpotCalIndex + 1 - mFirstSpotToCal;
  lastSpot = mScope->GetNumSpotSizes();
  if (HitachiScope) {
    firstSpot = lastSpot;
    idir = -1;
    numDone = mFirstSpotToCal + 1 - mSpotCalIndex;
    lastSpot = 1;
  }
  if (numDone > 1) {
    
    if (numDone < mNumSpotsToCal)
      saveData = AfxMessageBox("Not all the spots were done.\n\nDo you want to replace"
      " existing calibrations with the data that were acquired?", 
      MB_ICONQUESTION | MB_YESNO) != IDNO;

    // Find a table on the same side of crossover or use the next one (but not > 4)
    if (saveData) {
      aboveCross = GetAboveCrossover(mFirstSpotToCal, mTempIntensities[mFirstSpotToCal]);
      indTab = FindSpotTableIndex(aboveCross, probe);
      if (indTab < 0) {
        if (mNumSpotTables < 4)
          mNumSpotTables++;
        indTab = mNumSpotTables - 1;
      }
      mSpotTables[indTab].probeMode = probe;
    }  
    
    report.Format("Spot  Counts  Ratio   %s %s", mScope->GetC2Units(),
      mScope->GetC2Name());
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    for (i = mFirstSpotToCal; idir * i <= idir * mSpotCalIndex; i += idir) {
      ratio = mTempCounts[i] / mTempCounts[mFirstSpotToCal];
      if (saveData) {
        mSpotTables[indTab].ratio[i] = ratio;
        mSpotTables[indTab].intensity[i] = mTempIntensities[i];
        mSpotTables[indTab].crossover[i] = mScope->GetCrossover(i, probe);
      }
      report.Format("%4d    %6.1f    %.4f   %.2f", i, mTempCounts[i], ratio, 
        mTempIntensities[i] * 100.);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
      
    // Wipe out the ratios at the rest of the spot sizes
    if (saveData) {
      for (; idir * i <= idir * lastSpot; i += idir)
        mSpotTables[indTab].ratio[i] = 0.;
      for (i = firstSpot; idir * i < idir * mFirstSpotToCal; i += idir)
        mSpotTables[indTab].ratio[i] = 0.;
      if (mScope->GetUseIllumAreaForC2()) {
        i = RequestApertureSize();
        if (i) {
          mSpotCalAperture[indTab] = i;
          ScaleTablesForAperture(i, false);
        }
      }
      mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_SPOT);
    }
  }
  
  mScope->SetSpotSize(mSpotCalStartSpot, 1);
  mSpotCalIndex = 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Calibrate the crossover intensity
// The user sets beam to crossover, and intensities are shifted in other cals
void CBeamAssessor::CalibrateCrossover(void)
{
  double delCross, crossover, intensity;
 	int spotSize, spot, i, j;
  int minSpot = mScope->GetMinSpotSize();
  int numBeamChg = 0, numWarn = 0;
  BOOL changed = false;
  bool focChanged = false;
  CString message;
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals = 
    mShiftManager->GetFocusMagCals();
 	int spotSave = mScope->GetSpotSize();
  int probe = mScope->ReadProbeMode();
  if (AfxMessageBox("To calibrate crossover, the program will go to each spot\n"
    "size and ask you to bring the beam to crossover.\n\n"
    "You should make sure you are at eucentric focus and\n"
    " go to a high mag (~100K) to do this.", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
    return;
  for (spotSize = minSpot; spotSize <= mScope->GetNumSpotSizes(); spotSize++) {
      if (SetAndCheckSpotSize(spotSize))
        return;
      if (AfxMessageBox("Adjust brightness to bring the beam to crossover",
        MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
        break;
      intensity = mScope->GetIntensity();
      mScope->SetCrossover(spotSize, probe, intensity);
      mWinApp->SetCalibrationsNotSaved(true);
  }
  mScope->SetSpotSize(spotSave);
  if (mScope->GetUseIllumAreaForC2()) {
    i = RequestApertureSize();
    if (i) {
      mCrossCalAperture[probe] = i;
      ScaleTablesForAperture(i, false);
    }
  }

  // Look at the beam and spot calibrations and shift intensity values by change in 
  //crossover.  First check if this shifts any beam tables out of range
  for (spot = minSpot; spot < spotSize; spot++) {
    crossover = mScope->GetCrossover(spot);
    for (i = 0; i < mNumTables; i++) {
      delCross = crossover - mBeamTables[i].crossover;
      if (mBeamTables[i].spotSize == spot && mBeamTables[i].probeMode == probe && 
        mBeamTables[i].crossover && 
        fabs(delCross) > 1.e-6 && (mBeamTables[i].minIntensity + delCross <= 0. || 
            mBeamTables[i].minIntensity + delCross >= 1. ||
            mBeamTables[i].maxIntensity + delCross <= 0. || 
            mBeamTables[i].maxIntensity + delCross >= 1.)) {
              message.Format("Cannot shift spot %d, old c/o %f new c/o %f delta %f old "
                "min %f max %f", spot, mBeamTables[i].crossover, crossover, delCross, 
                mBeamTables[i].minIntensity, mBeamTables[i].maxIntensity);
              mWinApp->AppendToLog(message);
              numWarn++;
      }
    }
  }

  // Then go through again if no problems there
  for (spot = minSpot; !numWarn && spot < spotSize; spot++) {
    crossover = mScope->GetCrossover(spot);
    for (i = 0; i < mNumSpotTables; i++) {
      if (mSpotTables[i].ratio[spot] && mSpotTables[i].probeMode == probe && 
        mSpotTables[i].crossover[spot] && mSpotTables[i].crossover[spot] != crossover) {
        mSpotTables[i].intensity[spot] += crossover - mSpotTables[i].crossover[spot];
        mSpotTables[i].crossover[spot] = crossover;
        changed = true;
      }
    }
 
    for (i = 0; i < mNumTables; i++) {
      delCross = crossover - mBeamTables[i].crossover;
      if (mBeamTables[i].spotSize == spot && mBeamTables[i].probeMode == probe &&
        mBeamTables[i].crossover && fabs(delCross) > 1.e-6) {
          for (j = 0; j < mBeamTables[i].numIntensities; j++)
            mBeamTables[i].intensities[j] += (float)(delCross);
          mBeamTables[i].minIntensity += (float)(delCross);
          mBeamTables[i].maxIntensity += (float)(delCross);
          mBeamTables[i].crossover = crossover;
          numBeamChg++;
        }
    }
  }

  // Shift High focus mag cals if any
  for (i = 0; i < focusMagCals->GetSize(); i++) {
    HighFocusMagCal &focCal = focusMagCals->ElementAt(i);
    if (focCal.probeMode == probe && focCal.crossover > 0.) {
      crossover = mScope->GetCrossover(focCal.spot, probe);
      delCross = crossover - focCal.crossover;
      focCal.intensity += delCross;
      focCal.crossover = crossover;
      focChanged = true;
    }
  }

  // Let the user know what happened
  if (numBeamChg)
    message.Format("%s values in %d beam calibration tables\nwere shifted by the changes"
    " in crossover.", mScope->GetC2Name(), numBeamChg);
  else if (numWarn)
    message.Format("No beam calibration tables were shifted because the %s\n"
    "values would have gone out of range for %d of them.\n\nSee log for details.\n\n"
    "To get help, first rename SerialEMcalibrations.bak to SerialEMcalibrations-old.bak."
    "\nThen save the log and these calibrations.\nSend David the log, "
    "serialEMcalibrations.txt, SerialEMcalibrations-old.bak, and "
    "SerialEMcalibrations.bak\n", mScope->GetC2Name(), numWarn);
  if (changed)
    message += "\n" + CString(mScope->GetC2Name()) + " values in the spot intensity "
    "calibrations\nwere shifted by the changes in crossover.";
  if (focChanged)
    message += "\n" + CString(mScope->GetC2Name()) + " values in the high defocus mag "
    "calibrations\nwere shifted by the changes in crossover.";

  if (numBeamChg || changed || numWarn || focChanged)
    AfxMessageBox(message, MB_EXCLAME);
}

// Given a known value for the current C2 aperture on a Titan, scale the illuminated areas
// by this value from either the current value or the value at which each calibration
// was measured and save the aperture size
// Notice that the "current or measured aperture" for a value is not modified here except
// for dose cal, which is saved in short-term cal along with current aperture.
// Items are always scaled from the current aperture except on program startup, so they
// stay valid after multiple aperture changes.  Upon writing, the values being written are
// scaled BACK to the measured aperture from the current one
void CBeamAssessor::ScaleTablesForAperture(int currentAp, bool fromMeasured)
{
  BeamTable *btp;
  DoseTable *dtp;
  double cross;
  CString mess;
  int i, j, spot, probe, fromAp = mCurrentAperture;
  mScope = mWinApp->mScope;
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals = 
    mWinApp->mShiftManager->GetFocusMagCals();

  // Scale beam tables
  for (i = 0; i < mNumTables; i++) {
    btp = &mBeamTables[i];
    if (btp->numIntensities && CheckCalForZeroIntensities(mBeamTables[i], 
      "Before scaling for an aperture size", 2))
      btp->numIntensities = 0;
    if (fromMeasured)
      fromAp = btp->measuredAperture;
    btp->crossover = mScope->IntensityAfterApertureChange(btp->crossover, fromAp,
      currentAp, btp->spotSize, btp->probeMode);
    for (j = 0; j < btp->numIntensities; j++)
      btp->intensities[j] = mScope->IntensityAfterApertureChange(
        btp->intensities[j], fromAp, currentAp, btp->spotSize, btp->probeMode);
    mess.Format("After scaling for an aperture size from %d to %d", fromAp, currentAp);
    if (btp->numIntensities && CheckCalForZeroIntensities(mBeamTables[i], 
      (LPCTSTR)mess, 1))
      btp->numIntensities = 0;
  }

  // scale crossovers, electron doses, and spot tables
  for (spot = 1; spot < MAX_SPOT_SIZE; spot++) {
    for (probe = 0; probe < 2; probe++) {
      cross = mScope->GetCrossover(spot, probe);
      if (cross)
        mScope->SetCrossover(spot, probe, mScope->IntensityAfterApertureChange(cross, 
        fromMeasured ? mCrossCalAperture[probe] : mCurrentAperture, currentAp, spot, 
          probe));

      // Assign new current aperture for electron dose if old one was valid
      for (j = 0; j < 2; j++) {
        dtp = &mDoseTables[spot][j][probe];
        if (dtp->dose > 0. && dtp->currentAperture > 0) {
          mScope->IntensityAfterApertureChange(dtp->intensity, 
            fromMeasured ? dtp->currentAperture : mCurrentAperture, currentAp, spot, 
            probe);
          dtp->currentAperture = currentAp;
        }
      }
    }

    for (i = 0; i < mNumSpotTables; i++) {
      if (fromMeasured)
        fromAp = mSpotCalAperture[i];
      if (mSpotTables[i].ratio[spot]) {
        if (mSpotTables[i].crossover[spot])
          mSpotTables[i].crossover[spot] = mScope->IntensityAfterApertureChange(
          mSpotTables[i].crossover[spot], fromAp, currentAp, spot, 
            mSpotTables[i].probeMode);
        mSpotTables[i].intensity[spot] = mScope->IntensityAfterApertureChange(
          mSpotTables[i].intensity[spot], fromAp, currentAp, spot,
          mSpotTables[i].probeMode);
      }
    }
  }

  // Scale high focus mag cals
  for (i = 0; i < focusMagCals->GetSize(); i++) {
    HighFocusMagCal &focCal = focusMagCals->ElementAt(i);
    if (fromMeasured)
      fromAp = focCal.measuredAperture;
    focCal.intensity = mScope->IntensityAfterApertureChange(focCal.intensity, fromAp, 
      currentAp, focCal.spot, focCal.probeMode);
    focCal.crossover = mScope->IntensityAfterApertureChange(focCal.crossover, fromAp, 
      currentAp, focCal.spot, focCal.probeMode);
  }
  if (currentAp != mCurrentAperture)
    mWinApp->mDocWnd->SetShortTermNotSaved();
  mCurrentAperture = currentAp;
}

// Ask the user to enter an aperture size, require it to be within generous limits or to
// match a property for sizes
int CBeamAssessor::RequestApertureSize(void)
{
  while (true) {
    int newAp = mCurrentAperture;
    if (!KGetOneInt("Enter the current C2 aperture size in microns:", newAp))
      break;
    if ((!mNumC2Apertures && newAp >= 10 && newAp <= 500) || 
      numberInList(newAp, mC2Apertures, mNumC2Apertures, 0))
      return newAp;
    if (AfxMessageBox("That aperture size does not seem to be valid (check property "
      "C2ApertureSizes)\n\nPress OK to try again or Cancel to skip entering an aperture", 
      MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
      break;
  }
  return 0;
}

// This is called on program startup to inform the user of the situation about aperture
// and calibrations
void CBeamAssessor::InitialSetupForAperture(void)
{
  int i, numMeas = 0, numNotMeas = 0;
  CString mess;
  if (!mWinApp->mScope->GetUseIllumAreaForC2())
    return;
  for (i = 0; i < mNumTables; i++) {
    if (mBeamTables[i].measuredAperture)
      numMeas++;
    else
      numNotMeas++;
  }
  if (numMeas) {
    if (mCurrentAperture)
      mess.Format("Last known C2 aperture size was %d microns.\r\n   %d intensity "
      "calibrations have been set to work with that size.\r\n   Use Calibration - Set"
      " Aperture Size if the size is now different or you change the aperture.", 
      mCurrentAperture, numMeas);
    else
      mess.Format("The C2 aperture size is not known from a previous session.\r\n"
        "   For %d intensity calibrations, the aperture size was recorded at the time."
        "\r\n   These will work correctly if you run Calibration - Set Aperture Size", 
        numMeas);
    mWinApp->AppendToLog(mess);
  }
  if (numNotMeas) {
    mess.Format("The C2 aperture size was not recorded for %d beam intensity "
      "calibrations.\r\n   These will work correctly only at the same aperture size as "
      "when they were obtained.\r\n   To fix the calibration file, see the Help for "
      "Calibration - Beam Intensity", numNotMeas);
    mWinApp->AppendToLog(mess);
  }
}

//////////////////////////////////////////////////////////////
// CALIBRATE ILLUMINATE AREA LIMITS
//
// Start the calibration: get the biggest aperture in and save original values
void CBeamAssessor::CalibrateIllumAreaLimits(void)
{
  if (AfxMessageBox("This procedure will measure the limits of illuminated\n"
    "area at each spot size in both probe modes\n\n"
    "You MUST have the largest condenser (C2) aperture in for this measurement!\n\n"
    "Are you ready to proceed?", MB_QUESTION) == IDNO)
    return;
  mCalIAlimitSpot = 1;
  mCalIAspotSave = mScope->GetSpotSize();
  mCalIAsavedIA = mScope->GetIlluminatedArea();
  mCalIAfirstProbe = mScope->ReadProbeMode();
  mWinApp->UpdateBufferWindows();

  // Drop the screen if necessary, otherwise go right to first spot
  if (mScope->GetScreenPos() == spDown) {
    CalIllumAreaNextTask();
  } else {
    mScope->SetScreenPos(spDown);
    mWinApp->AddIdleTask(CEMscope::TaskScreenBusy, TASK_CAL_IA_LIMITS, 0, 60000);
  }
}

// Next operation when calibrating illuminated area limits: 
void CBeamAssessor::CalIllumAreaNextTask(void)
{
  BeamTable *btp;
  DoseTable *dtp;
  int probe, i, j, spot;
  double cross;
  float *lowLimits = mScope->GetCalLowIllumAreaLim();
  float *highLimits = mScope->GetCalHighIllumAreaLim();
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals =
    mWinApp->mShiftManager->GetFocusMagCals();
  bool notCal = highLimits[5] <= lowLimits[5];

  if (mCalIAlimitSpot <= 0)
    return;

  // Set the spot size with no normalization and set IA to large values and read back
  mScope->SetSpotSize(mCalIAlimitSpot, -1);
  probe = mScope->GetProbeMode();
  mScope->SetIlluminatedArea(mCalIAtestValue);
  mCalIAhighLimits[mCalIAlimitSpot][probe] = (float)mScope->GetIlluminatedArea();
  mScope->SetIlluminatedArea(-mCalIAtestValue);
  mCalIAlowLimits[mCalIAlimitSpot][probe] = (float)mScope->GetIlluminatedArea();
  PrintfToLog("%s %d: %.4f %.4f", probe ? "uP" : "nP", mCalIAlimitSpot, 
    mCalIAlowLimits[mCalIAlimitSpot][probe], mCalIAhighLimits[mCalIAlimitSpot][probe]);
  mCalIAlimitSpot++;
  if (mCalIAlimitSpot > mScope->GetNumSpotSizes()) {
    if (probe != mCalIAfirstProbe) {

      // Finish up
      // Copy the new calibrations into place
      for (spot = 1; spot <= mScope->GetNumSpotSizes(); spot++) {
        lowLimits[4 * spot] = mCalIAlowLimits[spot][0];
        highLimits[4 * spot] = mCalIAhighLimits[spot][0];
        lowLimits[4 * spot + 1] = mCalIAlowLimits[spot][1];
        highLimits[4 * spot + 1] = mCalIAhighLimits[spot][1];
      }

      // If they have not been calibrated before, have to convert various intensities
      if (notCal) {

        // AND store these as the first-time cals
        for (spot = 1; spot <= mScope->GetNumSpotSizes(); spot++) {
          lowLimits[4 * spot + 2] = mCalIAlowLimits[spot][0];
          highLimits[4 * spot + 2] = mCalIAhighLimits[spot][0];
          lowLimits[4 * spot + 3] = mCalIAlowLimits[spot][1];
          highLimits[4 * spot + 3] = mCalIAhighLimits[spot][1];
        }

        // Scale beam tables
       for (i = 0; i < mNumTables; i++) {
          btp = &mBeamTables[i];
          if (btp->numIntensities && CheckCalForZeroIntensities(mBeamTables[i],
            "Before scaling for an illum area limits", 2))
            btp->numIntensities = 0;
          ConvertIntensityForIACal(btp->crossover, btp->spotSize, btp->probeMode);
          for (j = 0; j < btp->numIntensities; j++)
            ConvertIntensityForIACal(btp->intensities[j], btp->spotSize, btp->probeMode);
        }

        // scale crossovers, electron doses, and spot tables
        for (spot = 1; spot < mScope->GetNumSpotSizes(); spot++) {
          for (probe = 0; probe < 2; probe++) {
            cross = mScope->GetCrossover(spot, probe);
            if (cross) {
              ConvertIntensityForIACal(cross, spot, probe);
              mScope->SetCrossover(spot, probe, cross);
            }

            // Assign new current aperture for electron dose if old one was valid
            for (j = 0; j < 2; j++) {
              dtp = &mDoseTables[spot][j][probe];
              if (dtp->dose > 0.)
                ConvertIntensityForIACal(dtp->intensity, spot, probe);
            }
          }

          for (i = 0; i < mNumSpotTables; i++) {
            if (mSpotTables[i].ratio[spot]) {
              if (mSpotTables[i].crossover[spot])
                ConvertIntensityForIACal(mSpotTables[i].crossover[spot], spot, probe);
              ConvertIntensityForIACal(mSpotTables[i].intensity[spot], spot, probe);
            }
          }


        }

        // Scale high focus mag cals
        for (i = 0; i < focusMagCals->GetSize(); i++) {
          HighFocusMagCal &focCal = focusMagCals->ElementAt(i);
          ConvertIntensityForIACal(focCal.intensity, focCal.spot, focCal.probeMode);
          ConvertIntensityForIACal(focCal.crossover, focCal.spot, focCal.probeMode);
        }

        ConvertSettingsForFirstIALimitCal(false);
      }

      mWinApp->SetCalibrationsNotSaved(true);
      StopCalIllumAreaLimits();
      return;
    }

    // Otherwise toggle the probe mode and start over
    mScope->SetProbeMode(1 - probe);
    mCalIAlimitSpot = 1;
  }
  mWinApp->AddIdleTask(TASK_CAL_IA_LIMITS, 0, 0);
}

// Stop the procedure: restore initial values
void CBeamAssessor::StopCalIllumAreaLimits()
{
  if (mCalIAlimitSpot <= 0)
    return;
  mScope->SetProbeMode(mCalIAfirstProbe);
  mScope->SetSpotSize(mCalIAspotSave);
  mScope->SetIlluminatedArea(mCalIAsavedIA);
  mCalIAlimitSpot = -1;
  mWinApp->UpdateBufferWindows();
}

void CBeamAssessor::CalIllumAreaCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out trying to drop screen before calibrating IA limits"),
      MB_EXCLAME);
  StopCalIllumAreaLimits();
  mWinApp->ErrorOccurred(error);
}

// Scale various intensities in the user settings.  This is done both to the current
// settings when the first cal is done, and the first time settings that haven't been 
// scaled are read in.
void CBeamAssessor::ConvertSettingsForFirstIALimitCal(bool skipLDcopy)
{
  int i, j;
  LowDoseParams *allLDparams = mWinApp->GetCamLowDoseParams();
  LowDoseParams *ldParams;
  CArray<StateParams *, StateParams *> *stateParams =
    mWinApp->mNavHelper->GetStateArray();
  StateParams *state;
  CookParams *cookParams = mWinApp->GetCookParams();
  CArray<AutocenParams *, AutocenParams *> *autocenArray =
    mWinApp->mMultiTSTasks->GetAutocenParams();
  AutocenParams *acParams;
  VppConditionParams *vppParams = mWinApp->mMultiTSTasks->GetVppConditionParams();

  // Low dose parameters
  mWinApp->SetDebugOutput('1');
  if (!skipLDcopy)
    mWinApp->CopyCurrentToCameraLDP();
  for (j = 0; j < 3; j++) {
    for (i = 0; i < MAX_LOWDOSE_SETS; i++) {
      ldParams = allLDparams + j * MAX_LOWDOSE_SETS + i;
      if ((ldParams->magIndex > 0 || ldParams->camLenIndex > 0) &&
        ldParams->intensity >= 0)
        ConvertIntensityForIACal(ldParams->intensity, ldParams->spotSize,
          ldParams->probeMode);
    }
  }
  if (!skipLDcopy)
    mWinApp->CopyCameraToCurrentLDP();

  // State parameters
  for (i = 0; i < (int)stateParams->GetSize(); i++) {
    state = stateParams->GetAt(i);
    ConvertIntensityForIACal(state->intensity, state->spotSize, state->probeMode);
  }

  // Cooker params FWIW
  if (cookParams->magIndex > 0 && cookParams->intensity > 0)
    ConvertIntensityForIACal(cookParams->intensity, cookParams->spotSize, 1);

  // Autocenter params
  for (i = 0; i < (int)autocenArray->GetSize(); i++) {
    acParams = autocenArray->GetAt(i);
    ConvertIntensityForIACal(acParams->intensity, acParams->spotSize,
      acParams->probeMode);
  }

  // VPP conditioning
  if (vppParams->magIndex > 0 && vppParams->intensity > 0)
    ConvertIntensityForIACal(vppParams->intensity, vppParams->spotSize,
      vppParams->probeMode);

  mWinApp->SetSettingsFixedForIACal(true);
}

// Convert one value given its spot size and probe mode for first-time cal
// The conversion cannot use the routines in 
void CBeamAssessor::ConvertIntensityForIACal(double &intensity, int spot, int probe)
{
  float lowMapTo, highMapTo, lowOld, highOld;
  double illum;
  float *lowCal = mScope->GetCalLowIllumAreaLim();
  float *highCal = mScope->GetCalHighIllumAreaLim();
  int index = 4 * spot + probe + 2;
  mScope->GetIllumAreaMapTo(lowMapTo, highMapTo);
  mScope->GetIllumAreaLimits(lowOld, highOld);

  // Convert intensity to illuminated area with old fixed limits
  illum = (intensity - lowMapTo) * (highOld - lowOld) / (highMapTo - lowMapTo) + lowOld;
  //SEMTrace('1',"i %f s %d p %d old %f %f map %f %f  illum %f", intensity, spot, probe, 
  //  lowOld, highOld, lowMapTo, highMapTo, illum);

  // Convert illum to intensity with the first time calibration limits, columns 2 and 3
  intensity = (illum - lowCal[index]) * (highMapTo - lowMapTo) /
    (highCal[index] - lowCal[index]) + lowMapTo;
  //SEMTrace('1', "cal ind %d  %f %f int %f", index, lowCal[index], highCal[index], 
  //  intensity);
}

void CBeamAssessor::ConvertIntensityForIACal(float &intensity, int spot, int probe)
{
  double dval = intensity;
  ConvertIntensityForIACal(dval, spot, probe);
  intensity = (float)dval;
}

// Calibrates the beam shift and tilt that occur on alpha changes
void CBeamAssessor::CalibrateAlphaBeamShifts(void)
{
  int alpha;
  int startAlpha = mScope->GetAlpha();
  float *beamShifts = mScope->GetAlphaBeamShifts();
  float *beamTilts = mScope->GetAlphaBeamTilts();
  double bsx, bsy;
  int numToCal = mScope->GetNumAlphaBeamShifts();
  if (startAlpha < 0)
    return;
  if (!KGetOneInt("You will be asked to adjust beam shift and possibly tilt at each alpha"
    " setting, starting with the lowest", 
    "Enter number of alpha values at which to calibrate beam shift and tilt:", numToCal))
    return;
  if (numToCal <= 0) {
    mScope->SetNumAlphaBeamShifts(0);
    mScope->SetNumAlphaBeamTilts(0);
    return;
  }
  for (alpha = 0; alpha < numToCal; alpha++) {
    if (!mScope->SetAlpha(alpha))
      break;
    if (AfxMessageBox("Center the beam at this alpha setting and adjust beam tilt if "
      "desired", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
      break;
    mScope->GetBeamShift(bsx, bsy);
    beamShifts[2 * alpha] = (float)bsx;
    beamShifts[2 * alpha + 1] = (float)bsy;
    mScope->GetBeamTilt(bsx, bsy);
    beamTilts[2 * alpha] = (float)bsx;
    beamTilts[2 * alpha + 1] = (float)bsy;
  }
  mScope->SetNumAlphaBeamShifts(alpha);
  mScope->SetNumAlphaBeamTilts(alpha);
  mWinApp->SetCalibrationsNotSaved(true);
  mScope->SetAlpha(startAlpha);
}

// Calibrates shift between spots, essential on Hitachi
void CBeamAssessor::CalibrateSpotBeamShifts(void)
{
  int startSpot = mScope->GetSpotSize();
  int numSpots = mScope->GetNumSpotSizes();
  double shifts[MAX_SPOT_SIZE + 1][2];
  double *calShifts = mScope->GetSpotBeamShifts();
  int minSpot, maxSpot, firstSpot, spot;
  int secondary = mScope->GetSecondaryMode();
  int calBase = secondary * 2 * (MAX_SPOT_SIZE + 1);
  int iDir = HitachiScope ? 1 : -1;
  int lastSpot = HitachiScope ? numSpots : mScope->GetMinSpotSize();
  mScope->GetMinMaxBeamShiftSpots(secondary, minSpot, maxSpot);
  if (!minSpot || !maxSpot)
    firstSpot = HitachiScope ? mScope->GetMinSpotSize() : numSpots;
  else
    firstSpot = HitachiScope ? minSpot : maxSpot;
  if (!KGetOneInt("You will be asked to center the beam at each spot"
    " size, going from dim to bright", 
    "Enter dimmest spot size at which to calibrate beam shift:", firstSpot))
    return;
  B3DCLAMP(firstSpot, 1, numSpots);
  if (firstSpot == lastSpot)
    return;
  minSpot = maxSpot = firstSpot;
  for (spot = firstSpot; spot * iDir <= lastSpot * iDir; spot += iDir) {
    if (SetAndCheckSpotSize(spot))
      return;
    if (AfxMessageBox("Center the beam at this spot size", 
      MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
      break;
    mScope->GetBeamShift(shifts[spot][0], shifts[spot][1]);
    minSpot = B3DMIN(firstSpot, spot);
    maxSpot = B3DMAX(firstSpot, spot);
  }
  mScope->SetSpotSize(startSpot);
  if (minSpot == maxSpot) {
    PrintfToLog("No relative shifts between spots were calibrated; previous calibration "
      "(if any) is unchanged");
    return;
  }
  for (spot = 0; spot <= 2 * numSpots; spot++)
    calShifts[calBase + spot] = 0;
  for (spot = minSpot; spot <= maxSpot; spot++) {
    calShifts[calBase + 2 * spot] = shifts[spot][0];
    calShifts[calBase + 2 * spot + 1] = shifts[spot][1];
  }
  mScope->SetMinMaxBeamShiftSpots(secondary, minSpot, maxSpot);
  PrintfToLog("Relative shifts will be applied between spots %d and %d", minSpot, 
    maxSpot);
  mWinApp->SetCalibrationsNotSaved(true);
}

// Set a spot size, testing for error and for whether it actually set
int CBeamAssessor::SetAndCheckSpotSize(int newSize, BOOL normalize)
{
  CString str;
  BOOL OK = mScope->SetSpotSize(newSize, normalize);
  if (!OK || mScope->GetSpotSize() != newSize) {
    str.Format("Failed to set spot size %d\n\n%sheck the SerialEM property "
      "NumberOfSpotSizes", newSize, 
      FEIscope ? "Check if the holder is in; if not, then\nc" : "C");
    AfxMessageBox(str, MB_EXCLAME);
    return 1;
  }
  return 0;
}

// Count up number of beam cals with entries in them
int CBeamAssessor::CountNonEmptyBeamCals(void)
{
  int ind, num = 0;
  for (ind = 0; ind < mNumTables; ind++)
    if (mBeamTables[ind].numIntensities > 0)
      num++;
  return num;
}

// Check a beam calibration for all zero values and issue a message box and to log
// Do not call this with an empty table unless you want the message to appear
int CBeamAssessor::CheckCalForZeroIntensities(BeamTable &table, const char *message, 
  int postMessType)
{
  CString str, str2;
  int ind;
  for (ind = 0; ind < table.numIntensities; ind++)
    if (table.intensities[ind])
      return 0;
  str.Format("%s, the beam calibration table has all zero intensities\r\n"
    "  for spot %d, # values %d, starting mag %d, extrap flags %d\r\n"
    "(table %p, calTable is %p)\r\n", message, 
    table.spotSize, table.numIntensities, table.magIndex, table.dontExtrapFlags,
    &table, mCalTable);
  if (table.aboveCross / 2 == 0) {
    str2.Format(", aboveCross %d, start intensity %.5f, crossover %.5f", table.aboveCross,
      mStartIntensity, mWinApp->mScope->GetCrossover(table.spotSize));
    str += str2;
  }
  if (FEIscope)
    str += (table.probeMode ? ", microprobe" : ", nanoprobe");
  if (mWinApp->mScope->GetUseIllumAreaForC2()) {
    str2.Format(", done with %d um aperture", table.measuredAperture);
    str += str2;
  }
  if (postMessType == 1)
    str += "\r\nPlease report this problem and the steps leading up to it to the "
    "SerialEM developer";
  if (postMessType == 2)
    str += "\r\nTo eliminate this calibration from the file, turn on\r\nAdministrator"
    "  mode in the Calibration menu and save calibrations";
  if (postMessType == 3)
    str += "\r\nThis calibration will be removed when calibrations are saved again.";
  mWinApp->AppendToLog(str);
  AfxMessageBox(str, MB_EXCLAME);
  return 1;
}

// List the intensity calibrations
void CBeamAssessor::ListIntensityCalibrations() {
  int i, j;
  CString report, s, t;
  mWinApp->AppendToLog("\r\nBeam intensity calibrations:", LOG_OPEN_IF_CLOSED);
  s = (FEIscope ? "Probe   " : "");
  t = (FEIscope && mScope->GetUseIllumAreaForC2() ? "C2 Aperture" : "");
  // TODO DNM Test output for JEOL
  report.Format("Spot   %sSide of crossover   Intensity range   Fold change   %s", s, t);
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  for (i = 0; i < mNumTables; i++) {
    if (mBeamTables[i].numIntensities > 0) {
      j = B3DNINT(mBeamTables[i].currents[mBeamTables[i].numIntensities - 1] /
        mBeamTables[i].currents[0]);
      s = (FEIscope ? (mBeamTables[i].probeMode ? "uP" : "nP") : "");
      if (FEIscope && mBeamTables[i].measuredAperture != 0)
        t.Format("%d", mBeamTables[i].measuredAperture);
      else
        t = "";
      report.Format("     %2d    %s          %s                         %.4f - %.4f   "
        "%5d                  %s", mBeamTables[i].spotSize, s,
        (mBeamTables[i].aboveCross == 1 ? "above" : "below"),
        mBeamTables[i].minIntensity, mBeamTables[i].maxIntensity, j, t);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
  }
}

// List the spot calibrations
void CBeamAssessor::ListSpotCalibrations() {
  int i, j;
  CString report, s, t;
  for (i = 0; i < mNumSpotTables; i++) {
    {
      s = (FEIscope ? (mSpotTables[i].probeMode ? "uP, " : "nP, ") : "");
      t = (mSpotTables[i].intensity[1] < mSpotTables[i].crossover[1] ? "below" : "above");
      report.Format("\r\nSpot intensity calibrations (%s%s crossover):", s, t);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      mWinApp->AppendToLog("Spot  Ratio", LOG_OPEN_IF_CLOSED);
      for (j = 1; j <= mScope->GetNumSpotSizes(); j++) {
        report.Format("   %4d   %.4f", j, mSpotTables[i].ratio[j]);
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
       }
    }
  }
}
