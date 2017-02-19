// MultiTSTasks.cpp  - module for more complex tasks related to multiple tilt series
//
// Copyright (C) 2008 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\MultiTSTasks.h"
#include "ComplexTasks.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "TSController.h"
#include "CameraController.h"
#include "AutocenSetupDlg.h"
#include "ProcessImage.h"
#include "TSRangeDlg.h"
#include "TSViewRange.h"
#include "FocusManager.h"
#include "Utilities\XCorr.h"

enum {TRACK_SHOT_DONE, SCREEN_DROPPED, START_WAITING, DOSE_DONE, TILT_RESTORED, 
  SCREEN_RESTORED, ALIGN_TRACK_SHOT, RESET_SHIFT_DONE};

enum {TSR_ZERO_SHOT_DONE, TSR_EUCEN_DONE, TSR_AUTOFOCUS, TSR_TAKE_IMAGE, TSR_IMAGE_DONE,
TSR_BACK_AT_ZERO, TSR_ALIGN_ZERO_SHOT};

#define MAX_STEPS 20

CMultiTSTasks::CMultiTSTasks(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mCkParams = mWinApp->GetCookParams();
  mTrParams = mWinApp->GetTSRangeParams();
  mCooking = false;
  mNumDoseReports = 10;
  mMinCookField = 8.;
  mCkISLimit = 2.0f;
  mCkNumAlignSteps = 7;
  mCkAlignStep = 0.02f;
  mCkNumStepCuts = 2;
  mAutoCentering = false;
  mRangeConsets[0] = TRIAL_CONSET;
  mRangeConsets[1] = RECORD_CONSET;
  mRangeConsets[2] = -1;
  mRangeConsets[3] = VIEW_CONSET;
  mRangeConsets[4] = TRIAL_CONSET;
  mRangeConsets[5] = PREVIEW_CONSET;
  mMaxTrStackMemory = 320;
  mAssessingRange = false;
  mTrMeanCrit = 0.25f;
  mTrPrevMeanCrit = 0.17f;
  mTrFirstMeanCrit = 0.125;
  mTrMaxAngleErr = 1.;
  mRangeViewer = NULL;
  mTrAutoLowAngle = mTrAutoHighAngle = mTrUserLowAngle = mTrUserHighAngle = -999.;
  mTiltRangeLowStamp = 0.;
  mTiltRangeHighStamp = 0.;
  mAcPostSettingDelay = 200;
  mAcSetupDlgOpen = false;
  mBfcCopyIndex = -1;
  mBfcDoingCopy = 0;
  mBfcStopFlag = false;
  mBaiSavedViewMag = -9;
  mSkipNextBeamShift = false;
}

CMultiTSTasks::~CMultiTSTasks(void)
{
  AutocenParams *parmP;
  for (int index = 0; index < mAcParamArray.GetSize(); index++) {
    parmP = mAcParamArray[index];
    delete parmP;
  }
}

// Set module pointers
void CMultiTSTasks::Initialize(void)
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mComplexTasks = mWinApp->mComplexTasks;
  mShiftManager = mWinApp->mShiftManager;
  mBeamAssessor = mWinApp->mBeamAssessor;
  mBufferManager = mWinApp->mBufferManager;
}

// Start the cooking operation
int CMultiTSTasks::StartCooker(void)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  int mDoseTimeout = 0;
  double dose, stretch;
  ControlSet  *conSet = mWinApp->GetConSets() + TRACK_CONSET;

  if (mCkParams->timeInstead && mCkParams->minutes <= 0. || !mCkParams->timeInstead &&
    (mCkParams->magIndex <= 0 || mCkParams->spotSize <= 0 || mCkParams->intensity < 0)) {
    SEMMessageBox("Parameters for cooking are not defined - run Cooker Setup",
      MB_EXCLAME);
    return 1;
  }

  if (!mCkParams->timeInstead) {
    dose = mWinApp->mBeamAssessor->GetElectronDose(mCkParams->spotSize, 
      mCkParams->intensity, 1.);
    if (!dose) {
      SEMMessageBox("Dose is not calibrated for the selected spot size and intensity"
        "\n\nRun Cooker Setup to see what settings have a calibrated dose,\n"
        "calibrate the dose, \nor just use a timed exposure instead", MB_EXCLAME);
      return 1;
    }

    // Set the timeout to something past the expected time
    mDoseTimeout = (int)(1200 * mCkParams->targetDose / dose);
    mExpectedMin = mCkParams->targetDose / (60. * dose);
  } else
    mExpectedMin = mCkParams->minutes;

  // Record current conditions and determine whether to tilt
  mCkIntensity = mScope->GetIntensity();
  mCkMagInd = mScope->GetMagIndex();
  mCkSpot = mScope->GetSpotSize();
  mCkDoTilted = false;
  if (mCkParams->cookAtTilt) {
    mCkTiltAngle = mScope->GetTiltAngle();
    stretch = cos(DTOR * mCkTiltAngle) / cos(DTOR * mCkParams->tiltAngle);
    mCkDoTilted = stretch < 0.95 || stretch > 1.05;
  }
  mCkLowDose = mWinApp->LowDoseMode();
  if (mCkLowDose)
    mWinApp->mLowDoseDlg.SetLowDoseMode(false);
  mWinApp->mScopeStatus.SetWatchDose(true);
  mDoseStart = mWinApp->mScopeStatus.GetFullCumDose();
  mDoseNextReport = (mCkParams->timeInstead ? mCkParams->minutes : mCkParams->targetDose)
    / mNumDoseReports;

  mCooking = true;
  mAcquiring = mTilting = mMovingScreen = mWaitingForDose = mResettingShift = false;
  mDidReset = mLoweredMag = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "COOKING SPECIMEN");

  // Start with tracking image if called for
  if (mCkParams->trackImage) {
    mComplexTasks->MakeTrackingConSet(conSet, targetSize);
    mCamera->SetRequiredRoll(2);
    StartTrackingShot(true, TRACK_SHOT_DONE);
    return 0;
  }
  
  // Check screen and see if tilt needed
  if (mScope->GetScreenPos() != spDown || mCkDoTilted)
    CookerNextTask(TRACK_SHOT_DONE);
  else
    CookerNextTask(START_WAITING);
  return 0;
}

// Do the next task
void CMultiTSTasks::CookerNextTask(int param)
{
  double shiftX, shiftY;
  float scaleMax;
  if (!mCooking)
    return;

  switch(param) {

    // Starting out or after acquire: save buffer, clear flags
    case TRACK_SHOT_DONE:
      if (mAcquiring) {
        mImBufs->mCaptured = BUFFER_TRACKING;
        mComplexTasks->RestoreMagIfNeeded();
        //mBufferManager->CopyImageBuffer(6, 0);  // For testing
      }
      mAcquiring = false;
      mLoweredMag = false;

      // Set state before dropping the screen
      mScope->SetMagIndex(mCkParams->magIndex);
      mScope->DelayedSetIntensity(mCkParams->intensity, GetTickCount());
      mScope->SetSpotSize(mCkParams->spotSize);
      if (mScope->GetScreenPos() != spDown) {
        TiltAndMoveScreen(false, mCkParams->tiltAngle, true, spDown);
        mWinApp->AddIdleTask(TASK_COOKER, mCkDoTilted ? SCREEN_DROPPED : START_WAITING,
          60000);
        return;
      }
      if (mCkDoTilted) {
        TiltAndMoveScreen(true, mCkParams->tiltAngle, false, spDown);
        mWinApp->AddIdleTask(TASK_COOKER, START_WAITING, 60000);
        return;
      }
      // DROP THROUGH TO WAITING

      // Waiting: clear flags, then just start the task
    case START_WAITING:
      mTilting = false;
      mMovingScreen = false;
      mDoseTime = GetTickCount();
      mWaitingForDose = true;
      mWinApp->AddIdleTask(TASK_COOKER, DOSE_DONE, mDoseTimeout);
      TimeToSimplePane(mExpectedMin);
      if (!mCkParams->timeInstead)
        mExpectedMin = 0;
      return;

    case SCREEN_DROPPED:
      mMovingScreen = false;
      TiltAndMoveScreen(true, mCkParams->tiltAngle, false, spDown);
      mWinApp->AddIdleTask(TASK_COOKER, START_WAITING, 60000);
      return;

      // When done, tilt back if needed, raise screen unconditionally
    case DOSE_DONE:
      mWaitingForDose = false;
      mWinApp->SetStatusText(SIMPLE_PANE, "");
      if (mCkDoTilted) {
        TiltAndMoveScreen(true, mCkTiltAngle, false, spUp);
        mCkDoTilted = false;
        mWinApp->AddIdleTask(TASK_COOKER, TILT_RESTORED, 60000);
        return;
      }
      // FALL THROUGH to TILT_RESTORED

    case TILT_RESTORED:
      TiltAndMoveScreen(false, mCkTiltAngle, true, spUp);
      mWinApp->AddIdleTask(TASK_COOKER, SCREEN_RESTORED, 60000);
      return;

      // After restoring screen/tilt, restore beam, take track shot if needed
    case SCREEN_RESTORED:
      mTilting = false;
      mMovingScreen = false;
      RestoreScopeState();
      if (mCkParams->trackImage) {
        StartTrackingShot(true, ALIGN_TRACK_SHOT);
        return;
      }

      // Otherwise it is done
      StopCooker();
      break;

      // Align tracking shot and reset image shift if it is high
    case ALIGN_TRACK_SHOT:
      mAcquiring = false;
      //mBufferManager->CopyImageBuffer(mDidReset ? 8 : 7, 0); // for testing
      mImBufs->mCaptured = BUFFER_TRACKING;
      if (AlignWithScaling(mDidReset ? 2 : 1, true, scaleMax)) {
        SEMMessageBox(_T("Failure to autoalign after cooking specimen"));
        StopCooker();
        return;
      }
      if (!mDidReset) {
        mScope->GetImageShift(shiftX, shiftY);
        if (mShiftManager->RadialShiftOnSpecimen(shiftX, shiftY, 
          mScope->FastMagIndex()) > mCkISLimit) {
          if (mShiftManager->ResetImageShift(true, false)) {
            CookerCleanup(1);
            return;
          }
          mResettingShift = true;
          mWinApp->AddIdleTask(TASK_COOKER, RESET_SHIFT_DONE, 0);
          return;
        }
      }

      // Otherwise again it is done
      StopCooker();
      break;

      // After reset image shift, redo the tracking shot and align
    case RESET_SHIFT_DONE:
      mDidReset = true;
      mResettingShift = false;
      StartTrackingShot(false, ALIGN_TRACK_SHOT);
      return;
  }

}

// The busy function actually watches for the dose too
int CMultiTSTasks::TaskCookerBusy(void)
{
  double diff, dose, togo;
  CString report;
  if (!mCooking)
    return 0;
  if (mAcquiring)
    return mCamera->CameraBusy() ? 1 : 0;
  if (mResettingShift)
    return mShiftManager->ResettingIS() ? 1 : 0;
  if (mTilting || mMovingScreen) {

    // Check busy only if was busy last time
    if (mTilting && mStageBusy > 0)
      mStageBusy = mScope->StageBusy();
    if (mMovingScreen && mScreenBusy > 0)
      mScreenBusy = mScope->ScreenBusy();

    //If either still busy, it's busy
    if (mScreenBusy > 0 || mStageBusy > 0)
      return 1;

    // If either errored, it's an error
    if (mScreenBusy < 0 || mStageBusy < 0)
      return -1;
    return 0;
  }

  // If accumulating dose: do periodic reports
  if (mWaitingForDose) {

    if (mCkParams->timeInstead) {
      diff = SEMTickInterval(mDoseTime) / 60000.;
      togo = mCkParams->minutes - diff;
      if (togo > 0.) {
        if (B3DNINT(10. * mExpectedMin) != B3DNINT(10. * togo))
          TimeToSimplePane(togo);
        if (diff >= mDoseNextReport) {
          report.Format("Time elapsed %.1f minutes, time remaining %.1f minutes", diff,
            mCkParams->minutes - diff);
          mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
          mDoseNextReport += mCkParams->minutes / mNumDoseReports;
        }
        Sleep(10);
        return 1;
      }

    } else {

      dose = mWinApp->mScopeStatus.GetFullCumDose() - mDoseStart;
      if (dose < mCkParams->targetDose) {
        diff = SEMTickInterval(mDoseTime);
        if (dose > 0. && diff > 0.) {
          togo = (mCkParams->targetDose - dose) * (diff / dose) / 60000.;
          if (mExpectedMin && togo < mExpectedMin && 
            B3DNINT(10. * mExpectedMin) != B3DNINT(10. * togo))
            TimeToSimplePane(togo);
        }
        if (dose >= mDoseNextReport) {
          report.Format("Dose so far %.1f electrons/A2, time remaining ~%.1f minutes", 
            dose, togo);
          mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
          TimeToSimplePane(togo);
          mDoseNextReport += mCkParams->targetDose / mNumDoseReports;
        }
        Sleep(10);
        return 1;
      }
    }
    mWinApp->AppendToLog("Specimen cooking completed", LOG_OPEN_IF_CLOSED);
  }
  return 0;
}

void CMultiTSTasks::CookerCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out in some operation involved in "
    "cooking specimen"));
  StopCooker();
  mWinApp->ErrorOccurred(error);
}

void CMultiTSTasks::StopCooker(void)
{
  if (!mCooking)
    return;
  bool moveScreen = mScope->GetScreenPos() != spUp;

  // If haven't tilted back yet or restored screen, do it now, wait a bit
  // This will only happen in case of error or stop, so it should be returning
  // to user control
  if (mCkDoTilted || moveScreen) {
    TiltAndMoveScreen(mCkDoTilted, mCkTiltAngle, moveScreen, spUp);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
    Sleep(1000);
    mScope->WaitForStageReady(1000);
  }

  // Restore mag if still lowered; otherwise restore state
  if (mLoweredMag)
    mComplexTasks->RestoreMagIfNeeded();
  else 
    RestoreScopeState();
  mCooking = false;
  if (mCkLowDose)
    mWinApp->mLowDoseDlg.SetLowDoseMode(true);

  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  mWinApp->mScopeStatus.SetWatchDose(false);
  mCamera->SetRequiredRoll(0);
}

// Potentially start both screen move and stage tilt, but now both only on a stop, and
// try to let the stage finish before starting the screen
void CMultiTSTasks::TiltAndMoveScreen(bool tilt, double angle, bool move, int position)
{
  mStageBusy = 0;
  mScreenBusy = 0;

  if (tilt) {
    mScope->WaitForStageReady(5000);
    mScope->TiltTo(angle);
    mTilting = true;
    mStageBusy = 1;
  }
  if (move) {
    if (mTilting)
      mScope->WaitForStageReady(10000);
    mScope->SetScreenPos(position);
    mMovingScreen = true;
    mScreenBusy = 1;
  }
}

// Align images, with search for best scaling and possible rotation
int CMultiTSTasks::AlignWithScaling(int buffer, bool doImShift, float &scaleMax,
  float startScale, float rotation)
{
  float peaks[MAX_STEPS];
  int numSteps = B3DMIN(mCkNumAlignSteps, MAX_STEPS - 1);
  int ist, idir;
  float peakmax = -1.e30f;
  float scale, curMax, peak, shiftX, shiftY, CCC, fracPix, usePeak;
  float step = mCkAlignStep;
  float *CCCp = &CCC, *fracPixP = &fracPix;
  double overlapPow = 0.166667;
  CString report;

  if (!startScale) {
    startScale = 1.;
    CCCp = NULL;
    fracPixP = NULL;
  }

  // Scan for the number of steps
  for (ist = 0; ist < numSteps; ist++) {
    scale = startScale + (float)ist * step;
    if (mShiftManager->AutoAlign(buffer, 0, false, false, &peaks[ist], 0., 0., 0., 
      scale, rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
      return 1;
    usePeak = peaks[ist];
    if (CCCp)
      usePeak = (float)(CCC * pow((double)fracPix, overlapPow));
    if (usePeak > peakmax) {
      peakmax = usePeak;
      scaleMax = scale;
    }
    if (CCCp) {
      SEMTrace('1', "Scale %.3f  peak  %g  CCC %.4f  frac %.3f  shift %.1f %.1f", scale,
        usePeak, CCC, fracPix, shiftX, shiftY);
    } else {
      report.Format("Scale %.3f  peak  %g  shift %.1f %.1f", scale, peaks[ist], shiftX, 
        shiftY);
      if (mComplexTasks->GetVerbose())
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
  }

  // Cut the step and look on either side of the peak 
  for (ist = 0; ist < mCkNumStepCuts; ist++) {
    step /= 2.f;
    curMax = scaleMax;
    for (idir = -1; idir <= 1; idir += 2) {
      scale = curMax + idir * step;
      if (mShiftManager->AutoAlign(buffer, 0, false, false, &peak, 0., 0., 0., scale,
        rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
        return 1;
      if (CCCp)
        peak = (float)(CCC * pow((double)fracPix, overlapPow));
      if (peak > peakmax) {
        peakmax = peak;
        scaleMax = scale;
      }
      if (CCCp) {
        SEMTrace('1', "Scale %.3f  peak  %g  CCC %.4f  frac %.3f  shift %.1f %.1f", scale,
          peak, CCC, fracPix, shiftX, shiftY);
      } else {
        report.Format("Scale %.3f  peak  %g  shift %.1f %.1f", scale, peak, shiftX, 
          shiftY);
        if (mComplexTasks->GetVerbose())
          mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      }
    }
  }

  // Align for real at the best peak.  Need to reset shifts of image for
  // scope image shift to work right
  if (!CCCp) {
    report.Format("The two images correlate most strongly assuming a shrinkage of %.1f%%",
      100. * (scaleMax - 1.));
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  }
  return mShiftManager->AutoAlign(buffer, 0, doImShift, false, &peak, 0., 0., 0., 
        scaleMax, rotation);
}

// Do common actions for starting a tracking shot
void CMultiTSTasks::StartTrackingShot(bool lowerMag, int nextAction)
{
  int newMagInd;
  if (lowerMag) {
    newMagInd = mComplexTasks->FindMaxMagInd(mMinCookField, -2);
    mComplexTasks->LowerMagIfNeeded(newMagInd, 0.75f, 0.5f, TRACK_CONSET);
    mLoweredMag = true;
  }
  mComplexTasks->StartCaptureAddDose(TRACK_CONSET);
  mAcquiring = true;
  mWinApp->AddIdleTask(TASK_COOKER, nextAction, 0);
}

// Restore the scope state
void CMultiTSTasks::RestoreScopeState(void)
{
  DWORD postMag;
  mScope->SetMagIndex(mCkMagInd);
  postMag = GetTickCount();
  mScope->SetSpotSize(mCkSpot);
  mScope->DelayedSetIntensity(mCkIntensity, postMag);
}

// Display time to go in simple pane
void CMultiTSTasks::TimeToSimplePane(double time)
{
  CString timeStr;
  timeStr.Format("~%.1f MIN TO GO", time);
  mWinApp->SetStatusText(SIMPLE_PANE, timeStr);
  mExpectedMin = time;
}

///////////////////////////////////////////////////////////////////////////////////////
// AUTOCENTER BEAM


AutocenParams *CMultiTSTasks::GetAutocenSettings(int camera, int magInd, int spot,
  int probe, double roughInt, bool & synthesized, int &bestMag,int &bestSpot)
{
  static AutocenParams acParams;
  AutocenParams *parmP;
  int index, i, j, newbin, bin, indTab, aboveCross, aboveNeeded;
  SpotTable *tables = mBeamAssessor->GetSpotTables();
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  int camsize = B3DMIN(camParam->sizeX, camParam->sizeY);
  int minCamsize = 400;
  double newIntensity, newexp, intChg, posSpotChg, lowestSpotChg, posMagChg;
  double lowestMagChg, outFactor;
  double minSpotChg = 0.1;
  double maxSpotChg = 5.;
  double maxMagChg = 5.;
  double maxExposure = 2.;
  float pixel2, ratio, useRatio;
  float deadTime = camParam->deadTime;
  float minExposure = B3DMAX(mComplexTasks->GetMinTaskExposure(), camParam->minExposure);
  float pixelSize = mShiftManager->GetPixelSize(camera, magInd);
  ControlSet *conSet = mWinApp->GetConSets() + TRIAL_CONSET;

  // If there is an exact match, return it
  synthesized = false;
  bestMag = magInd;
  bestSpot = spot;
  parmP = LookupAutocenParams(camera, magInd, spot, probe, roughInt, index);
  if (parmP)
    return parmP;
  synthesized = true;
  aboveNeeded = mBeamAssessor->GetAboveCrossover(spot, roughInt, probe);

  // Set up fallback params
  bestMag = -1;
  bestSpot = -1;
  lowestMagChg = lowestSpotChg = 1.e20;
  acParams.camera = camera;
  acParams.magIndex = magInd;
  acParams.spotSize = spot;
  acParams.intensity = -1.;
  acParams.binning = conSet->binning;
  acParams.exposure = 0.25 * (conSet->exposure - deadTime) + deadTime;
  acParams.exposure = B3DMAX(acParams.exposure, minExposure);
  acParams.useCentroid = false;
  acParams.probeMode = probe;

  // Now look at each setting and evaluate feasibility of adapting it
  for (i = 0; i < mAcParamArray.GetSize(); i++) {
    parmP = mAcParamArray[i];

    // skip if no camera match, or no spot match and spot has matched, or other side of
    // crossover
    if (parmP->camera != camera || (parmP->spotSize != spot && minSpotChg == 1.) || 
      parmP->intensity < 0. || parmP->probeMode != probe)
      continue;
    aboveCross = mBeamAssessor->GetAboveCrossover(parmP->spotSize, parmP->intensity, 
      parmP->probeMode);
    if (aboveCross != aboveNeeded)
      continue;

    newbin = parmP->binning;
    newexp = parmP->exposure;
    if (parmP->spotSize != spot) {

      // If spot doesn't match, first find ratio of intensities
      indTab = mBeamAssessor->FindSpotTableIndex(aboveCross, probe);
      if (indTab < 0 || !tables[indTab].ratio[spot] || 
        !tables[indTab].ratio[parmP->spotSize])
        continue;
      intChg = tables[indTab].ratio[spot] / tables[indTab].ratio[parmP->spotSize];
      posSpotChg = intChg > 1. ? intChg : 1. / intChg;
      if (posSpotChg > lowestSpotChg || intChg > maxSpotChg || intChg < minSpotChg)
        continue;
      
      // Modify binning and exposure to work with this intensity change
      if (intChg < 1.) {

        // Dimmer beam, increased exposure is needed, take it up with binning first
        useRatio = 1.;
        for (j = 0; j < camParam->numBinnings; j++) {
          bin = camParam->binnings[j];
          ratio = (float)(bin * bin) / (float)(parmP->binning * parmP->binning);
          if (bin < parmP->binning || camsize / bin < minCamsize || ratio > posSpotChg)
            continue;
          newbin = bin;
          useRatio = ratio;
        }
        newexp = (parmP->exposure - deadTime) * (posSpotChg / useRatio) + deadTime;
        if (newexp > maxExposure)
          continue;

      } else {

        // Brighter beam, decrease exposure time first, then binning
        newexp = (parmP->exposure - deadTime) / intChg + deadTime;
        if (newexp <  minExposure) {
          
          // Search for a binning that allows an exposure above limits
          for (j = camParam->numBinnings; j >= 0; j--) {
            bin = camParam->binnings[j];
            if (bin >= parmP->binning)
              continue;
            ratio = (float)(bin * bin) / (float)(parmP->binning * parmP->binning);
            newexp = (parmP->exposure - deadTime) / (intChg * ratio) + deadTime;
            if (newexp >= minExposure) {
              newbin = bin;
              break;
            }
          }
          if (newbin == parmP->binning)
            continue;
             
        }
      }
    } else
      posSpotChg = 1.;

    // Now if mag doesn't match, evaluate intensity change needed for that
    newIntensity = parmP->intensity;
    if (parmP->magIndex != magInd) {
      pixel2 = mShiftManager->GetPixelSize(camera, parmP->magIndex);
      intChg = (pixel2 * pixel2) / (pixelSize * pixelSize);
      posMagChg = intChg > 1. ? intChg : 1. / intChg;

      // Skip it if outside limits, or spot is same as best spot and it is worse than
      // best change due to mag so far
      if (posMagChg > maxMagChg || 
        (posSpotChg >= lowestSpotChg && posMagChg > lowestMagChg))
        continue;
      if (mBeamAssessor->AssessBeamChange(parmP->intensity, spot, probe, intChg, 
        newIntensity, outFactor))
        continue;
    } else
      posMagChg = 1.;

    // If it is either a better spot size, or a better mag at same spot size, take it as
    // new best source
    if (posSpotChg < lowestSpotChg || posMagChg < lowestMagChg) {
      lowestSpotChg = posSpotChg;
      lowestMagChg = posMagChg;
      bestMag = parmP->magIndex;
      bestSpot = parmP->spotSize;
      acParams.intensity = newIntensity;
      acParams.binning = newbin;
      acParams.exposure = 0.001 * B3DNINT(1000. * newexp);
      acParams.useCentroid = parmP->useCentroid;
    }
  }
  return &acParams;
}

// Add a set of params to the array
void CMultiTSTasks::AddAutocenParams(AutocenParams *params)
{
  AutocenParams *parmP = new AutocenParams;
  *parmP = *params;
  mAcParamArray.Add(parmP);
}

// Delete matching params from the array
int CMultiTSTasks::DeleteAutocenParams(int camera, int magInd, int spot, int probe, 
  double roughInt)
{
  int index;
  AutocenParams *parmP = LookupAutocenParams(camera, magInd, spot, probe, roughInt,index);
  if (index < 0)
    return 1;
  delete parmP;
  mAcParamArray.RemoveAt(index);
  return 0;
}

// Find a matching param
AutocenParams * CMultiTSTasks::LookupAutocenParams(int camera, int magInd, int spot,
                                                   int probe, double roughInt, int &index)
{
  int aboveCross, aboveNeeded;
  AutocenParams *parmP;
  aboveNeeded = mBeamAssessor->GetAboveCrossover(spot, roughInt, probe);
  for (index = 0; index < mAcParamArray.GetSize(); index++) {
    parmP = mAcParamArray[index];
    aboveCross = mBeamAssessor->GetAboveCrossover(parmP->spotSize, parmP->intensity, 
      parmP->probeMode);
    if (parmP->camera == camera && parmP->magIndex == magInd && parmP->spotSize == spot
      && aboveCross == aboveNeeded && parmP->probeMode == probe)
      return parmP;
  }
  index = -1;
  return NULL;
}

// Determine if a param exists for this camera and mag index
bool CMultiTSTasks::AutocenParamExists(int camera, int magInd, int probe)
{
  AutocenParams *parmP;
  for (int index = 0; index < mAcParamArray.GetSize(); index++) {
    parmP = mAcParamArray[index];
    if (parmP->camera == camera && parmP->magIndex == magInd && parmP->probeMode == probe)
      return true;
  }
  return false;
}

// Open or reopen the autocenter setup dialog
void CMultiTSTasks::SetupAutocenter(bool reopen)
{
  bool synth;
  BOOL lowDose = mWinApp->LowDoseMode();
  int bestMag, bestSpot, magInd, probe, alpha;
  AutocenParams *param;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
  ScaleMat bsCal;
  if (lowDose) {
    if (!ldParm->magIndex || !ldParm->spotSize || !ldParm->intensity) {
      SEMMessageBox("You need to set up the Trial low dose area\nbefore trying to "
        "set up autocentering in low dose", MB_EXCLAME);
      return;
    }
    magInd = ldParm->magIndex;
    probe = ldParm->probeMode;
    alpha = (int)ldParm->beamAlpha;
  } else {
    magInd = mScope->GetMagIndex();
    probe = mScope->ReadProbeMode();
    alpha = mScope->GetAlpha();
  }

  bsCal = mWinApp->mShiftManager->GetBeamShiftCal(magInd, alpha, probe);
  if (!bsCal.xpx) {
    AfxMessageBox("There is no beam shift calibration in the current\n"
      "magnification range.\n\nCalibrate beam shift to use autocentering.", MB_EXCLAME);
    return;
  }
  CAutocenSetupDlg *dlg = new CAutocenSetupDlg;
  mWinApp->mAutocenDlg = dlg;
  if (reopen) {
    dlg->m_bSetState = true;
    dlg->mSavedMagInd = mAcSavedMagInd;
    dlg->mSavedSpot = mAcSavedSpot;
    dlg->mSavedProbe = mAcSavedProbe;
    dlg->mSavedIntensity = mAcSavedIntensity;

    // Do not drop screen again in low dose mode, intensity gets set to the Trial value
    if (mWinApp->LowDoseMode())
      ldParm->intensity = mAcLDTrialIntensity;
    else if (mAcSavedScreen == spDown)
      mScope->SetScreenPos(spDown);
  } else if (lowDose) {
    mAcSavedLDDownArea = mScope->GetLowDoseDownArea();
    mScope->SetLowDoseDownArea(TRIAL_CONSET);
    mWinApp->mLowDoseDlg.m_iShowArea = TRIAL_CONSET;
    mWinApp->mLowDoseDlg.SetContinuousUpdate(false);
    mWinApp->mLowDoseDlg.UpdateSettings();
  }
  mAcSetupDlgOpen = true;
  dlg->DoModal();
  mAcSetupDlgOpen = false;
  if (dlg->mTestAcquire) {
    mAcSavedMagInd = dlg->mSavedMagInd;
    mAcSavedSpot = dlg->mSavedSpot;
    mAcSavedProbe = dlg->mSavedProbe;
    mAcSavedIntensity = dlg->mSavedIntensity;
    mAcSavedScreen = mScope->GetScreenPos();
    param = GetAutocenSettings(dlg->mCamera, dlg->mCurMagInd, dlg->mCurSpot, 
      dlg->mCurProbe, dlg->mParam->intensity, synth, bestMag, bestSpot);
    MakeAutocenConset(param);
    if (mWinApp->LowDoseMode()) {
      mAcLDTrialIntensity = ldParm->intensity;
      ldParm->intensity = param->intensity;
    }
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_TEST_AUTOCEN, 0, 0);
  } else if (lowDose) {
    mScope->SetLowDoseDownArea(mAcSavedLDDownArea);
    mWinApp->mLowDoseDlg.m_iShowArea = mAcSavedLDDownArea;
    mWinApp->mLowDoseDlg.UpdateSettings();
  }
  mWinApp->mAutocenDlg = NULL;
  delete dlg;
}

// Make a control set for taking an autocenter image
void CMultiTSTasks::MakeAutocenConset(AutocenParams * param)
{
  int sizeX, sizeY, left, top, bottom, right;
  ControlSet  *conSets = mWinApp->GetConSets();
  CameraParameters *camParams = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  conSets[TRACK_CONSET] = conSets[TRIAL_CONSET];
  conSets[TRACK_CONSET].binning = param->binning;
  conSets[TRACK_CONSET].exposure = (float)param->exposure;
  conSets[TRACK_CONSET].forceDark = false;
  conSets[TRACK_CONSET].onceDark = false;
  conSets[TRACK_CONSET].drift = 0.;
  conSets[TRACK_CONSET].mode = SINGLE_FRAME;
  conSets[TRACK_CONSET].processing = GAIN_NORMALIZED;

  // Use the largest centered square that fits in the field
  sizeX = sizeY = B3DMIN(camParams->sizeX, camParams->sizeY) / param->binning;
  mCamera->CenteredSizes(sizeX, camParams->sizeX, camParams->moduloX, left, right, 
    sizeY, camParams->sizeY, camParams->moduloY, top, bottom, param->binning);
  conSets[TRACK_CONSET].left = left * param->binning;
  conSets[TRACK_CONSET].top = top * param->binning;
  conSets[TRACK_CONSET].right = right * param->binning;
  conSets[TRACK_CONSET].bottom = bottom * param->binning;
}

// Autocenter beam
int CMultiTSTasks::AutocenterBeam(void)
{
  int magInd, spotSize, probe;
  int bestMag, bestSpot;
  bool synth, raise;
  CString report;
  AutocenParams *param;
  LowDoseParams *ldParm;
  FilterParams *filtParm = mWinApp->GetFilterParams();
    
  if (mWinApp->LowDoseMode()) {
    ldParm = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
    magInd = ldParm->magIndex;
    spotSize = ldParm->spotSize;
    mAcSavedIntensity = ldParm->intensity;
    probe = ldParm->probeMode;
    if (!magInd || !spotSize || !mAcSavedIntensity) {
      SEMMessageBox("You need to set up the Trial low dose area\nbefore trying to "
      "autocenter in low dose", MB_EXCLAME);
      return 1;
    }
  } else {
    magInd = mScope->GetMagIndex();
    spotSize = mScope->GetSpotSize();
    mAcSavedIntensity = mScope->GetIntensity();
    probe = mScope->ReadProbeMode();
  }
  SEMTrace('I', "AutocenBeam saving intensity %.5f  %.3f%%", mAcSavedIntensity, 
    mScope->GetC2Percent(spotSize, mAcSavedIntensity));

  // Get the param and make sure it works
  param = mWinApp->mMultiTSTasks->GetAutocenSettings(mWinApp->GetCurrentCamera(), magInd,
    spotSize, probe, mAcSavedIntensity, synth, bestMag, bestSpot);
  if (param->intensity < 0) {
    SEMMessageBox("There are no usable settings for autocentering at this magnification"
      " and spot size", MB_EXCLAME);
    return 1;
  }
  if (bestSpot != spotSize) {
    report.Format("Warning: using intensity setting for spot %d to center beam at spot"
      " %d", bestSpot, spotSize);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED); 
  }

  // Zero image shift except in low dose, set the intensity and get an image
  if (!mWinApp->LowDoseMode()) {
    mScope->GetLDCenteredShift(mAcISX, mAcISY);
    mScope->IncImageShift(-mAcISX, -mAcISY);
  }
  MakeAutocenConset(param);

  // Have to raise screen before setting intensity if in EFTEM with auto mag changes
  // Otherwise set it now or set LD intensity
  raise = !mWinApp->LowDoseMode() && mWinApp->GetEFTEMMode() && filtParm->autoMag && 
    mScope->GetScreenPos() == spDown;
  if (mWinApp->LowDoseMode())
    ldParm->intensity = param->intensity;
  else if (!raise) {
    SEMTrace('I', "AutocenBeam setting intensity to %.5f  %.3f%%", param->intensity, 
      mScope->GetC2Percent(spotSize, param->intensity));
    mScope->SetIntensity(param->intensity);
    Sleep(mAcPostSettingDelay);
  }

  mAutoCentering = true;
  mAcUseCentroid = param->useCentroid ? 1 : 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "AUTOCENTERING BEAM");
  if (raise) {

    // If raising screen, save the intensity that needs to be used
    mAcIntensityToUse = param->intensity;
    mScope->SetScreenPos(spUp);
    mWinApp->AddIdleTask(CEMscope::TaskScreenBusy, TASK_AUTOCEN_BEAM, 1, 60000);
    mWinApp->SetStatusText(SIMPLE_PANE, "RAISING SCREEN");
  } else {
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_AUTOCEN_BEAM, 0, 0);
  }
  return 0;
}

// The next task is simply to center the beam based on the image
void CMultiTSTasks::AutocenNextTask(int param)
{
  if (!mAutoCentering)
    return;

  // If an image was taken, center beam with it
  if (!param) {
    mImBufs->mCaptured = BUFFER_TRACKING;
    mWinApp->mProcessImage->CenterBeamFromActiveImage(0., 0., mAcUseCentroid > 0);
  }

  // Need another shot if screen was raised or centroid being used; save and set 
  // intensity now if screen was raised in EFTEM mode
  if (param || mAcUseCentroid == 1) {
    if (param) {
      mAcSavedIntensity = mScope->GetIntensity();
      SEMTrace('I', "AutocenNextTask saving intensity %.5f  setting %.5f", 
        mAcSavedIntensity, mAcIntensityToUse);
      mScope->SetIntensity(mAcIntensityToUse);
      Sleep(mAcPostSettingDelay);
    }else
      mAcUseCentroid++;
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_AUTOCEN_BEAM, 0, 0);
    return;
  }
  StopAutocen();
}

void CMultiTSTasks::AutocenCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mWinApp->mTSController->TSMessageBox(_T("Timeout getting image for autocentering "
    "beam"));
  StopAutocen();
  mWinApp->ErrorOccurred(error);
}

void CMultiTSTasks::StopAutocen(void)
{
  LowDoseParams *ldParm;
  if (!mAutoCentering)
    return;
  SEMTrace('I', "StopAutocen restoring intensity %.5f", mAcSavedIntensity);
  mScope->SetIntensity(mAcSavedIntensity);
  if (mWinApp->LowDoseMode()) {

    // Restore both trial and focus if tied, in case the trial value leaked into focus
    ldParm = mWinApp->GetLowDoseParams();
    ldParm[TRIAL_CONSET].intensity = mAcSavedIntensity;
    if (mWinApp->mLowDoseDlg.m_bTieFocusTrial)
      ldParm[FOCUS_CONSET].intensity = mAcSavedIntensity;

    // Go back to area to restore the intensity in case continuous update is on
    mScope->GotoLowDoseArea(TRIAL_CONSET);
  } else
    mScope->IncImageShift(mAcISX, mAcISY);
  mAutoCentering = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

////////////////////////////////////////////////////////////////////////////////////
// TILT SERIES RANGE FINDER
//
void CMultiTSTasks::RangeFinder()
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  ControlSet  *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  int numShots, maxSize;
  double stageZ;

  if (mRangeViewer) {
    mRangeViewer->BringWindowToTop();
    return;
  }

  CTSRangeDlg dlg;
  if (dlg.DoModal() != IDOK)
    return;

  // Make conset if needed for zero-deg shots
  if (mWinApp->LowDoseMode()) {
    mTrZeroDegConSet = 0;
  } else {
    mComplexTasks->MakeTrackingConSet(conSet, targetSize);
    mTrZeroDegConSet = TRACK_CONSET;
  }

  mShiftsOnAcquireSave = mBufferManager->GetShiftsOnAcquire();
  mBufferManager->SetShiftsOnAcquire(1);
  mTrPostActions = mWinApp->ActPostExposure();
  mLowDose = mWinApp->LowDoseMode() ? 1 : 0;

  // Allow a large field in case there is a pole touch that moves X/Y
  mMinTrZeroDegField = mComplexTasks->GetMinRSRAField();
  if (mTrParams[mLowDose].eucentricity)
    mMinTrZeroDegField = B3DMAX(mMinTrZeroDegField, 
    mComplexTasks->GetMinFEFineAlignField());

  mTrZeroRefBuf = 2;
  if (mTrParams[mLowDose].walkup)
    mTrZeroRefBuf = 3;

  // Save these regardless, as a signature of the position
  mScope->GetImageShift(mTrISXstart, mTrISYstart);
  mScope->GetStagePosition(mTrStageX, mTrStageY, stageZ);

  // Figure out size for stack window limit
  mStackWinMaxSave = mBufferManager->GetStackWinMaxXY();
  numShots = 1 + B3DNINT((mTrParams[mLowDose].endAngle - mTrParams[mLowDose].startAngle)
    / mTrParams[mLowDose].angleInc);
  if (mTrParams[mLowDose].direction == 0)
    numShots *= 2;
  maxSize = (int)sqrt(mMaxTrStackMemory * 1024. * 1024. / numShots);
  mBufferManager->SetStackWinMaxXY(maxSize);
  mWinApp->DetachStackView();

  // Start with minus direction unless doing plus only
  mTrDirection = mTrParams[mLowDose].direction == 2 ? 1 : -1;
  mTrDoingEucen = mTrWalkingUp = mTrTilting = mTrFocusing = false;
  if (mTrParams[mLowDose].direction < 2)
    mTrAutoLowAngle = mTrUserLowAngle = -999.;
  if (mTrParams[mLowDose].direction != 1)
    mTrAutoHighAngle =  mTrUserHighAngle = -999.;
  mAssessingRange = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "ASSESSING TILT RANGE");
  StartZeroDegShot(TSR_ZERO_SHOT_DONE);

}

void CMultiTSTasks::TiltRangeNextTask(int param)
{
  float curMean;
  int action, nx, ny, maxsize = 1024;
  KImage *image;
  StageMoveInfo smi;
  double shiftX, shiftY;
  bool convert = mRangeConsets[3 * mLowDose + mTrParams[mLowDose].imageType] == 
    RECORD_CONSET;
  if (!mAssessingRange)
    return;

  if (param == TSR_ZERO_SHOT_DONE || param == TSR_IMAGE_DONE) {
    image = mImBufs->mImage;
    nx = image->getWidth();
    ny = image->getHeight();
    image->Lock();
    curMean = (float)ProcImageMean(image->getData(), image->getType(), 
      nx, ny, nx / 4, 3 * nx / 4, ny / 4, 3 * ny / 4);
    image->UnLock();
  }

  switch (param) {
    case TSR_ZERO_SHOT_DONE:

      // Save the image in buffer C and restore mag
      mImBufs->mCaptured = BUFFER_TRACKING;
      mAcquiring = false;
      mBufferManager->CopyImageBuffer(0, mTrZeroRefBuf);
      RestoreFromZeroShot();
      mTrZeroMean = curMean;
      if (mTrParams[mLowDose].eucentricity) {

        // Start eucentricity and return
        action = FIND_EUCENTRICITY_FINE;
        if (mTrParams[mLowDose].walkup)
          action |= REFINE_EUCENTRICITY_ALIGN;
        mComplexTasks->FindEucentricity(action);
        mTrDoingEucen = true;
        mWinApp->AddIdleTask(TASK_TILT_RANGE, TSR_EUCEN_DONE, 0);
        return;
      }

      // Otherwise go to the starting angle
      WalkupOrTilt();
      return;

    case TSR_EUCEN_DONE:
      mTrDoingEucen = false;
      WalkupOrTilt();
      return;

    case TSR_AUTOFOCUS:
      if (mTrWalkingUp)
        mNeedStageRestore = mComplexTasks->GetWalkDidResetShift();
      mTrWalkingUp = mTrTilting = false;
      mWinApp->mFocusManager->AutoFocusStart(1);
      mTrFocusing = true;
      mWinApp->AddIdleTask(TASK_TILT_RANGE, TSR_TAKE_IMAGE, 0);
      return;

    case TSR_TAKE_IMAGE:
      if (mTrWalkingUp)
        mNeedStageRestore = mComplexTasks->GetWalkDidResetShift();
      mTrWalkingUp = mTrTilting = mTrFocusing = false;
      TiltRangeCapture(-1.e10);
      return;

    case TSR_IMAGE_DONE:
      mAcquiring = false;
      
      // For Record, do not limit size but convert to bytes; otherwise limit to 1024
      // and leave as integers
      if (convert)
        maxsize = B3DMAX(nx, ny);
      mBufferManager->AddToStackWindow(0, maxsize, -1, convert);

      // Autoalign if did walkup and it is past the first image and shift is not too big
      if (mTrParams[mLowDose].walkup && fabs(mImBufs->mTiltAngleOrig) - 
        mTrParams[mLowDose].startAngle > mTrParams[mLowDose].angleInc / 2.) {
        mScope->GetLDCenteredShift(shiftX, shiftY);
        if (mShiftManager->RadialShiftOnSpecimen(shiftX, shiftY, mScope->FastMagIndex()) 
          < (mWinApp->LowDoseMode() ? 
          mComplexTasks->GetWULowDoseISLimit() : mComplexTasks->GetWalkShiftLimit()))
          mShiftManager->AutoAlign(1, 0);
      }

      // If there were post actions and tilt was not needed, get the shot (it will make
      // sure we are at zero)  Include a test for the last picture
      if (mTrPostActions && !mTrTiltNeeded) {
        MeansDropped(curMean);
        StartZeroDegShot(TSR_ALIGN_ZERO_SHOT);
        return;
      }

      // If doing post actions, start next shot, but only if this image was bright enough
      if (mTrPostActions && !MeansDropped(curMean) && !mEndTiltRange) {
        TiltRangeCapture(curMean);
      } else {

        // otherwise tilt up or back down; in latter case restore starting IS and stage
        if (IsTiltNeeded(curMean)) {
          smi.axisBits = axisA;
          smi.alpha = mTrCurTilt;
          action = TSR_TAKE_IMAGE;
        } else {
          if (mTrParams[mLowDose].walkup)
            mScope->SetImageShift(mTrISXstart, mTrISYstart);
          SetupStageReturnInfo(&smi);
          action = TSR_BACK_AT_ZERO;
        }
        mScope->MoveStage(smi);
        mTrTilting = true;
        mWinApp->AddIdleTask(TASK_TILT_RANGE, action, 60000);
      }

      // Roll the means and return
      mTrPrevTilt = mTrLastTilt;
      mImBufs->GetTiltAngle(mTrLastTilt);
      mTrPreviousMean = mTrCenterMean;
      mTrCenterMean = curMean;
      return;

    case TSR_BACK_AT_ZERO:
      mTrTilting = false;
      StartZeroDegShot(TSR_ALIGN_ZERO_SHOT);
      return;

    case TSR_ALIGN_ZERO_SHOT:

      // After aligning zero shot, start other direction 
      mImBufs->mCaptured = BUFFER_TRACKING;
      mAcquiring = false;
      mShiftManager->AutoAlign(mTrZeroRefBuf, 0);

      // Restore after autoalign so autoalign will call trimming routine
      RestoreFromZeroShot();
      if (mTrDirection < 0 && mTrParams[mLowDose].direction == 0) {
        mTrDirection = 1;
        WalkupOrTilt();
        return;
      }
      break;

  }
  FinishTiltRange();
}

int CMultiTSTasks::TiltRangeBusy(void)
{
  //if (mTrWalkingUp)
   // SEMTrace('1', "Testing busy acq %d ti %d euc %d wal %d task %d", mAcquiring?1:0, mTrTilting?1:0,
    //mTrDoingEucen?1:0, mTrWalkingUp?1:0, mComplexTasks->DoingTasks() ? 1 : 0);
  if (mAcquiring)
    return mCamera->CameraBusy();
  if (mTrTilting)
    return mScope->TaskStageBusy();
  return ((mTrDoingEucen && mComplexTasks->DoingEucentricity()) ||
    (mTrWalkingUp && mComplexTasks->DoingWalkUp()) ||
    (mTrFocusing && mWinApp->mFocusManager->DoingFocus())) ? 1 : 0;
}

void CMultiTSTasks::TiltRangeCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mWinApp->mTSController->TSMessageBox(_T("Timeout during tilt range assessment"));
  StopTiltRange();
  mWinApp->ErrorOccurred(error);
}

// For external stops - restore image shift and stage positions
void CMultiTSTasks::StopTiltRange(void)
{
  StageMoveInfo smi;
  if (!mAssessingRange)
    return;
  RestoreFromZeroShot();
  if (mTrParams[mLowDose].walkup)
    mScope->SetImageShift(mTrISXstart, mTrISYstart);
  SetupStageReturnInfo(&smi);
  mScope->MoveStage(smi);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  FinishTiltRange();
}

// The finishing up steps
void CMultiTSTasks::FinishTiltRange(void)
{
  CRect rect;
  int dialogOffset;
  mAssessingRange = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  mBufferManager->SetShiftsOnAcquire(mShiftsOnAcquireSave);
  mBufferManager->SetStackWinMaxXY(mStackWinMaxSave);
  if (mTrParams[mLowDose].direction < 2)
    mTiltRangeLowStamp = GetTickCount();
  if (mTrParams[mLowDose].direction != 1)
    mTiltRangeHighStamp = GetTickCount();
  if (mWinApp->mStackView) {
    mWinApp->ResizeStackView(70, 70);
    mRangeViewer = new CTSViewRange();
    mRangeViewer->Create(IDD_TSSETANGLE);
    mWinApp->GetMainRect(&rect, dialogOffset);
    mRangeViewer->SetWindowPos(&CWnd::wndTopMost, rect.left + dialogOffset + 70, 
      rect.top + 60, 100, 100, SWP_NOSIZE | SWP_SHOWWINDOW);
    mWinApp->mStackView->SetFocus();
  }
}


// Start a shot at zero degrees, lower mag if necessary
void CMultiTSTasks::StartZeroDegShot(int nextAction)
{
  StageMoveInfo smi;
  int newMagInd;

  // But first make sure we actually got to zero!
  if (nextAction == TSR_ALIGN_ZERO_SHOT && fabs(mScope->GetTiltAngle()) > 1.) {
    SEMTrace('1', "StartZeroDegShot found stage not at 0, returning to zero first");
    SetupStageReturnInfo(&smi);
    mScope->MoveStage(smi);
    mTrTilting = true;
    mWinApp->AddIdleTask(TASK_TILT_RANGE, TSR_BACK_AT_ZERO, 60000);
    return;
  }

  newMagInd = mComplexTasks->FindMaxMagInd(mMinTrZeroDegField, -2);
  mComplexTasks->LowerMagIfNeeded(newMagInd, 0.75f, 0.5f, TRACK_CONSET);
  mCamera->SetObeyTiltDelay(true);
  mCamera->SetCancelNextContinuous(true);
  mCamera->InitiateCapture(mTrZeroDegConSet);
  mAcquiring = true;
  mLoweredMag = true;
  mEndTiltRange = false;

  // Clear this flag here: in every case the restore is no longer needed
  mNeedStageRestore = false;
  mWinApp->AddIdleTask(TASK_TILT_RANGE, nextAction, 0);
}

// Restore mag and other settings after zero-deg shot
void CMultiTSTasks::RestoreFromZeroShot(void)
{
  if (mLoweredMag)
    mComplexTasks->RestoreMagIfNeeded();
  mLoweredMag = false;
  mCamera->SetObeyTiltDelay(false);
}

// Either start a walkup or tilt to the starting angle, and set the next action as needed
void CMultiTSTasks::WalkupOrTilt(void)
{
  float angle = mTrDirection * mTrParams[mLowDose].startAngle;
  int timeout = 0;
  int action = mTrParams[mLowDose].autofocus ? TSR_AUTOFOCUS : TSR_TAKE_IMAGE;
  if (mTrParams[mLowDose].walkup) {
    mComplexTasks->WalkUp(angle, -1, 0.);
    mTrWalkingUp = true;
  } else {
    mScope->TiltTo((double)angle);
    mTrTilting = true;
    timeout = 60000;
  }
  mTrCurTilt = angle;
  mTrLastTilt = 0.;
  mTrCenterMean = -1.e10;
  mTrPreviousMean = -1.e10;
  mTrDidStopMessage = false;
  mWinApp->AddIdleTask(TASK_TILT_RANGE, action, timeout);
}

// Determine if another tilt is needed and set flag and angle if so
BOOL CMultiTSTasks::IsTiltNeeded(float curMean)
{
  CString str;
  mTrTiltNeeded = false;
  double curAngle = mScope->GetTiltAngle();
  if (mEndTiltRange) {
    str.Format("Stopping at %1.f because End was pressed", curAngle);
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
    return FALSE;
  }

  // Test in order so lowest angle will show up in stop message
  if (MeansDropped(curMean) || fabs(mTrCurTilt) + 0.75 * mTrParams[mLowDose].angleInc > 
    mTrParams[mLowDose].endAngle ||
    fabs(mTrCurTilt) - fabs(curAngle) > mTrMaxAngleErr) {
    if (fabs(mTrCurTilt) - fabs(curAngle) > mTrMaxAngleErr) {
      str.Format("Stopped at %.1f because the stage failed to tilt to %.1f", 
        curAngle, mTrCurTilt);
      mNeedStageRestore = true;
      LogStopMessage(str);
      StoreLimitAngle(true, (float)(curAngle - mTrDirection * 1.));
    }
    return FALSE;
  }
  mTrTiltNeeded = true;
  mTrCurTilt += mTrDirection * mTrParams[mLowDose].angleInc;
  return TRUE;
}

// Test for whether image center means dropped enough to stop
bool CMultiTSTasks::MeansDropped(float curMean)
{
  CString str;
  if ((mTrCenterMean > -1.e6 && curMean / mTrCenterMean < mTrMeanCrit) ||
      (mTrPreviousMean > -1.e6 && curMean / mTrPreviousMean < mTrPrevMeanCrit) ||
      (!mTrLastTilt && curMean > -1.e6 &&  // Only compare first shot for same image type
      ((mLowDose && mRangeConsets[3 + mTrParams[1].imageType] == 0) || 
      (!mLowDose && mRangeConsets[mTrParams[0].imageType] == TRIAL_CONSET)) &&
      curMean / mTrZeroMean < mTrFirstMeanCrit * cos(DTOR * mTrCurTilt))) {
    if (!mTrLastTilt) {
      str.Format("Stopped because center mean of %.0f for first shot is much less than "
        "mean of %.0f at zero degrees", curMean, mTrZeroMean);
    } else if (curMean / mTrCenterMean < mTrMeanCrit) {
      str.Format("Stopped because center mean dropped from %.0f to %.0f; last good tilt "
        "= %.1f", mTrCenterMean, curMean, mTrLastTilt);
      StoreLimitAngle(true, mTrLastTilt);
    } else {
      str.Format("Stopped because center mean dropped from %.0f to %.0f over two tilts; "
        "last good tilt = %.1f or %.1f", mTrPreviousMean, curMean, mTrPrevTilt, 
        mTrLastTilt);
      StoreLimitAngle(true, mTrPrevTilt);
    }
    LogStopMessage(str);
    return true;
  }
  return false;
}

// Send out the stop message only once
void CMultiTSTasks::LogStopMessage(CString str)
{
  if (!mTrDidStopMessage)
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  mTrDidStopMessage = true;
}

// Start a capture with tilting if appropriate
void CMultiTSTasks::TiltRangeCapture(float curMean)
{
  StageMoveInfo smi;
  smi.axisBits = axisA;
  int delay;
  if (mTrPostActions) {

    // If doing post-actions, either tilt to next angle or tilt back to zero
    if (IsTiltNeeded(curMean)) {
      smi.alpha = mTrCurTilt;
      delay = mShiftManager->GetMinTiltDelay();
    } else {
      SetupStageReturnInfo(&smi);
      delay = mShiftManager->GetAdjustedTiltDelay(mTrCurTilt);
    }
    mCamera->QueueStageMove(smi, delay);
  }

  mCamera->SetCancelNextContinuous(true);
  mCamera->InitiateCapture(mRangeConsets[3 * mLowDose + mTrParams[mLowDose].imageType]);
  mAcquiring = true;
  mWinApp->AddIdleTask(TASK_TILT_RANGE, TSR_IMAGE_DONE, 0);
}

// Set info for return to zero, and add X/Y movement if needed
void CMultiTSTasks::SetupStageReturnInfo(StageMoveInfo * smi)
{
  smi->alpha = 0;
  smi->axisBits = axisA;
  if (mNeedStageRestore) {
    SEMTrace('1', "Moving stage during tilt back to zero");
    smi->x = mTrStageX;
    smi->y = mTrStageY;
    smi->axisBits |= (axisX | axisY);
  }
}

// Store an angle as appropriate
void CMultiTSTasks::StoreLimitAngle(bool userAuto, float angle)
{
  if (angle < 0) {
    if (userAuto)
      mTrAutoLowAngle = angle;
    else
      mTrUserLowAngle = angle;
  } else {
    if (userAuto)
      mTrAutoHighAngle = angle;
    else
      mTrUserHighAngle = angle;
  }
}


////////////////////////////////////////
//  BIDIRECTIONAL TILT SERIES FILE COPY
////////////////////////////////////////

// Top-level routine starts file inversion for single-frame and inverts piece list Z for
// montage
int CMultiTSTasks::InvertFileInZ(int zmax)
{
  int err = 0;
  CString mess;
  if (mBufferManager->CheckAsyncSaving())
    return 1;
  if (mWinApp->Montaging()) {
    err = mWinApp->mStoreMRC->InvertPieceZCoords(zmax);
    if (err && err < 3)
      mess.Format("An error (code %d) occurred trying to invert the\nZ coordinates in the"
      " image file header.\nThe state of the file is unknown.\n\n", err);
    else
      mess.Format("An error (code %d) occurred trying to invert the Z coordinates in the"
      "\n.mdoc file, although the coordinates in the image file header inverted OK.\n\n",
      err);
  } else {

    // If it hasn't been set up on a previous call before, start it now
    if (mBfcCopyIndex < 0)
      err = SetupBidirFileCopy(zmax);
    if (err) {
      mess.Format("An error (code %d) occurred trying to initialize the\ncopying of the"
        " file to a new one with inverted Z values.\n\n", err);
    } else {
      err = StartBidirFileCopy(false);
      if (err > 0)
        mess.Format("An error (code %d) occurred trying to start or resume copying\n"
        "the file to a new one with inverted Z values.\n\n", err);
    }
  }
  if (err > 0) {
    mess += "This error cannot be recovered from;\nthe tilt series is being terminated.";
    SEMMessageBox(mess);
  }
  return err;
}

// Set up the copy operation
int CMultiTSTasks::SetupBidirFileCopy(int zmax)
{
  int err, savedBufToSave = mBufferManager->GetBufToSave();
  CString ext, root, adocName;
  if (!mWinApp->mStoreMRC)
    return 1;
  if (mBfcCopyIndex >= 0)
    return 2;
  CString filename = mWinApp->mStoreMRC->getFilePath();
  if (filename.IsEmpty())
    return 3;

  // Compose the filename for the first half
  UtilSplitExtension(filename, root, ext);
  mBfcNameFirstHalf = root + "-half1" + ext;
  mBfcAdocFirstHalf = "";

  if (mWinApp->mStoreMRC->GetAdocIndex() >= 0) {
    adocName = mWinApp->mStoreMRC->getAdocName();
    UtilSplitExtension(adocName, root, ext);
    mBfcAdocFirstHalf = mBfcNameFirstHalf + ext;
  }

  // Close the file, rename it, and reopen it by new name, then open new file in current
  // store.  If anything goes wrong, we simply have to close the current file and bail
  delete mWinApp->mStoreMRC;
  mWinApp->mStoreMRC = NULL;
  if (UtilRenameFile(filename, mBfcNameFirstHalf) || 
    (!adocName.IsEmpty() && UtilRenameFile(adocName, mBfcAdocFirstHalf))) {
    mWinApp->mDocWnd->DoCloseFile();
    return 4;
  }
  mBfcStoreFirstHalf = UtilOpenOldMRCFile(mBfcNameFirstHalf);
  if (!mBfcStoreFirstHalf) {
    mWinApp->mDocWnd->DoCloseFile();
    return 5;
  }
  if (mWinApp->mDocWnd->OpenNewReplaceCurrent(filename, !adocName.IsEmpty())) {
    delete mBfcStoreFirstHalf;
    return 6;
  }
  mBfcNumToCopy = zmax;
  mBfcCopyIndex = 0;

  // Now we need to read first image into the read buffer and save it out
  err = mBufferManager->ReadFromFile(mBfcStoreFirstHalf, zmax - 1);
  if (!err) {
    mBufferManager->SetBufToSave(mBufferManager->GetBufToReadInto());
    err = mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC, true);
    mBufferManager->SetBufToSave(savedBufToSave);
  }
  if (err) {
    mBfcCopyIndex = -1;
    delete mBfcStoreFirstHalf;
    return 7;
  }
  mBufferManager->CheckAsyncSaving();
  mBfcCopyIndex++;
  return 0;
}

// Start the next copy, return > 0 for error, -1 for synchronous copy
int CMultiTSTasks::StartBidirFileCopy(bool synchronous)
{
  bool doSynchro = synchronous || !mBufferManager->GetSaveAsynchronously();
  int err, wasDoing = mBfcDoingCopy;
  CString mess;
  mBfcDoingCopy = 0;

  // Return if no longer doing any copy, give error and clean up if file closed
  if (mBfcCopyIndex < 0)
    return 0;
  if (!mWinApp->mStoreMRC) {
    ResetFromBidirFileCopy();
    return 1;
  }

  // If there is more to copy
  if (mBfcCopyIndex < mBfcNumToCopy) {

    // First obey the stop flag
    if (mBfcStopFlag) {
      BidirFileCopyClearFlags();
      return 0;
    }

    // Set up for synchronous copy BEFORE it starts
    if (doSynchro) {
      mess.Format("COPYING SEC %d OF %d", mBfcCopyIndex + 1, mBfcNumToCopy);
      mWinApp->SetStatusText(MEDIUM_PANE, mess);
      mBfcDoingCopy = 1;
      if (!wasDoing)
        mWinApp->UpdateBufferWindows();
    } 

    // Launch or do the copy
    err = mBufferManager->StartAsyncCopy(mBfcStoreFirstHalf, mWinApp->mStoreMRC, 
      mBfcNumToCopy - 1 - mBfcCopyIndex, mBfcCopyIndex, doSynchro);
    if (err) {
      BidirFileCopyClearFlags();
      return err > 0 ? err : -err;
    }

    // Set flag for synchronous or not
    mBfcDoingCopy = doSynchro ? 1 : -1;
    if (!doSynchro)
      return 0;

    // If the operation is synchronous, it was successful, so inc the index, set an
    // idle task to give a chance to interrupt it, and return -1 so caller can set an
    // idle task for the whole transfer to be done
    mBfcCopyIndex++;
    mWinApp->AddIdleTask(TASK_BIDIR_COPY, 0, 0);
    return -1;
  }

  // The copy is done, finish up
  ResetFromBidirFileCopy();
  UtilRemoveFile(mBfcNameFirstHalf);
  if (!mBfcAdocFirstHalf.IsEmpty())
    UtilRemoveFile(mBfcAdocFirstHalf);
  return 0;
}

// Set a stop flag to stop after the current copy
void CMultiTSTasks::StopBidirFileCopy(void)
{
  if (mBfcDoingCopy)
    mBfcStopFlag = true;
}

// When an asynchronous copy succeeds, go ahead and restart if indicated, or clear flags
void CMultiTSTasks::AsyncCopySucceeded(bool restart)
{
  mBfcCopyIndex++;
  if (restart) {
    StartBidirFileCopy(false);
    return;
  } else
    SEMTrace('y', "Async save done, not restarting another (yet)");
  mBfcDoingCopy = 0;
  mBfcStopFlag = false;
}

// External call to abandon any current copying attempt and cleanup the mess
void CMultiTSTasks::CleanupBidirFileCopy(void)
{
  if (mBfcCopyIndex < 0)
    return;
  if (mBfcDoingCopy)
    mBfcStopFlag = true;
  mBufferManager->CheckAsyncSaving();
  ResetFromBidirFileCopy();
}

// Clear flags related to actively copying
void CMultiTSTasks::BidirFileCopyClearFlags(void)
{
  mBfcDoingCopy = 0;
  mBfcStopFlag = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Set everything back to where it belongs; an internal call that assumes it was active
void CMultiTSTasks::ResetFromBidirFileCopy(void)
{
  mBfcCopyIndex = -1;
  delete mBfcStoreFirstHalf;
  BidirFileCopyClearFlags();
}

//////////////////////////////////////////////////////////
//  BIDIRECTIONAL ANCHOR ROUTINES
//////////////////////////////////////////////////////////

// Start getting an anchor image or an image to align to the anchor
int CMultiTSTasks::BidirAnchorImage(int magInd, BOOL useView, bool alignNotSave, 
                                     bool setImageShifts)
{
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams() + VIEW_CONSET;
  ControlSet  *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  CString mess;
  int err, setNum = TRACK_CONSET;
  int camera = mWinApp->GetCurrentCamera();
  double outFactor, inFactor;
  if (alignNotSave && UtilOpenFileReadImage(mWinApp->mTSController->GetBidirAnchorName(), 
    "bidirectional anchor")) {
      mSkipNextBeamShift = false;
      return 1;
  }

  // Restart the background copy if it is pending after reading that image
  if (BidirCopyPending() && (err = StartBidirFileCopy(false)) > 0) {
    mess.Format("An error (code %d) occurred trying to resume copying\n"
        "the file to a new one with inverted Z values.\n\n", err);
    SEMMessageBox(mess);
    mSkipNextBeamShift = false;
    return 1;
  }

  mBaiSavedViewMag = -1;
  if (mWinApp->LowDoseMode()) {

    // In low dose, if not using View per se and the indicated mag is lower than the 
    // view mag, save the mag and intensity and change the intensity for the new mag
    setNum = VIEW_CONSET;
    if (!useView && magInd < ldParm->magIndex) {
      mBaiSavedViewMag = ldParm->magIndex;
      ldParm->magIndex = magInd;
      mBaiSavedIntensity = ldParm->intensity;
      inFactor = mShiftManager->GetPixelSize(camera, mBaiSavedViewMag) /
        mShiftManager->GetPixelSize(camera, magInd);
      mWinApp->mBeamAssessor->AssessBeamChange(inFactor * inFactor, ldParm->intensity,
        outFactor, VIEW_CONSET);
    }
    mScope->GotoLowDoseArea(VIEW_CONSET);
  } else {
    mComplexTasks->MakeTrackingConSet(conSet, 
      B3DMAX(1024, mCamera->TargetSizeForTasks()));
    mComplexTasks->LowerMagIfNeeded(magInd, 0.75f, 0.5f, TRACK_CONSET);
  }

  // Now that mag is changed, zero out the image shift if directed for align shot
  if (alignNotSave && setImageShifts) {
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    mScope->SetImageShift(mBaiISX, mBaiISY);
    mScope->SetSkipNextBeamShift(false);
  }
  mComplexTasks->StartCaptureAddDose(setNum);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_BIDIR_ANCHOR, alignNotSave ? 1 : 0, 0);
  return 0;
}

// Anchor image is done, either save it or align it to stored image
void CMultiTSTasks::BidirAnchorNextTask(int param)
{
  double stageZ;
  float backX, backY;
  TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
  mImBufs->mCaptured = BUFFER_TRACKING;
  int err;
  KStoreMRC *storeMRC;
  if (param) {

    // Align to saved anchor
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    err = mShiftManager->AutoAlign(1, 0);
    mScope->SetSkipNextBeamShift(false);
    if (tsParam->retainBidirAnchor) {
      err = mBufferManager->CheckAsyncSaving();
      if (!err) {
        storeMRC = UtilOpenOldMRCFile(mWinApp->mTSController->GetBidirAnchorName());
        if (!storeMRC) {
          err = 1;
          SEMMessageBox("Cannot open anchor image file for saving second image");
        } else {
          err = storeMRC->AppendImage(mImBufs->mImage);
          if (err)
            SEMMessageBox("Error appending anchor realign image to anchor image file");
        }
      }
    }
  } else {

    // Save the anchor
    err = UtilSaveSingleImage(mWinApp->mTSController->GetBidirAnchorName(),
      "bidirectional anchor", false);

    // Get the stage position and image shift so it can be restored
    mScope->GetStagePosition(mBaiMoveInfo.x, mBaiMoveInfo.y, stageZ);
    mScope->GetImageShift(mBaiISX, mBaiISY);
    mBaiMoveInfo.doBacklash = mScope->GetValidXYbacklash(mBaiMoveInfo.x, mBaiMoveInfo.y,
      backX, backY);
    mBaiMoveInfo.backX = backX;
    mBaiMoveInfo.backY = backY;
  }
  if (err)
    BidirAnchorCleanup(1);
  else  
    StopAnchorImage();
}

// Pass back the parameters for restoring the stage, including backlash
void CMultiTSTasks::SetAnchorStageXYMove(StageMoveInfo &smi)
{
  smi = mBaiMoveInfo;
  smi.axisBits = axisXY;
}

void CMultiTSTasks::StopAnchorImage()
{
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams() + VIEW_CONSET;
  mSkipNextBeamShift = false;
  if (mBaiSavedViewMag < -1)
    return;
  if (mBaiSavedViewMag >= 0) {
    mScope->GotoLowDoseArea(TRIAL_CONSET);
    ldParm->magIndex = mBaiSavedViewMag;
    ldParm->intensity = mBaiSavedIntensity;
  } else if (!mWinApp->LowDoseMode()) {
    mComplexTasks->RestoreMagIfNeeded();
  }
  mBaiSavedViewMag = -9;
}

void CMultiTSTasks::BidirAnchorCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out getting an anchor image for bidirectional series"));
  StopAnchorImage();
  mWinApp->ErrorOccurred(error);
}
