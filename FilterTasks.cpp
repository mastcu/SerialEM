// FilterTasks.cpp       GIF-related tasks, specfically calibrating mag-dependent
//                         energy shifts
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "FilterTasks.h"
#include "EMScope.h"
#include "CameraController.h"
#include "TSController.h"
#include "BeamAssessor.h"
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "ComplexTasks.h"
#include "Utilities\KGetOne.h"
#include "Utilities\XCorr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFilterTasks

//IMPLEMENT_DYNCREATE(CFilterTasks, CCmdTarget)

CFilterTasks::CFilterTasks()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSets = mWinApp->GetConSets();
  mImBufs = mWinApp->GetImBufs();
  mFiltParams = mWinApp->GetFilterParams();
  mMagTab = mWinApp->GetMagTable();
  mCurMag = 0;
  mCMSMinField = 8.;
  mEnergies = NULL;
  mMeanCounts = NULL;
  mDeltaCounts = NULL;
  mCMSSlitWidth = 10.;
  mCMSEnergyRange = 48.;
  mCMSStepSize = 3.;
  mNumMags = 13;
  mPeakThreshold = 0.02f; // Fraction below peak that is considered below concern
  mMinPeakRatio = 0.1f; // Fraction of overall peak that is recognized as a scan peak
  mMinPeakSize = 100.f;   // Minimum height to consider a peak
  mGoodPeakSize = 2000.f;   // A height that qualifies absolutely as a peak
  mBasicIntegrationWidth = 20.f;  // Width to integrate, minus slit width
  mBigEnergyShiftDelay = 1000;  // Delays, more conservative than the ones that
  mLittleEnergyShiftDelay = 250;  // filter setup would use if called with true
  mRZlpIndex = -1;
  mRZlpSlitWidth = 20.f;    // Slit width for refine ZLP
  mRZlpStepSize = 2.f;      // Step size
  mRZlpMinExposure = 0.005f;  // Minimum exposure time
  mRZlpDeltaCrit = 0.1f;    // Final delta must be below this fraction of maximum
  mRZlpMeanCrit = 0.02f;    // Final mean must be below this fraction of maximum
  mRZlpUserCancelFrac = 0.5f;  // Automatically cancel loss less than this frac of width
  mRZlpRedoInLowDose = false;  // Do not try again in low dose
}

CFilterTasks::~CFilterTasks()
{
}

void CFilterTasks::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mShiftManager = mWinApp->mShiftManager;

}

BEGIN_MESSAGE_MAP(CFilterTasks, CCmdTarget)
  //{{AFX_MSG_MAP(CFilterTasks)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFilterTasks message handlers


void CFilterTasks::MagShiftCalNewMag(int inMag)
{
  // If we need to drop the mag, save the current mag and set intensity zoom
  double newIntensity, delBeam, newDelta;
  int magInd = mScope->GetMagIndex();
  int camera = mWinApp->GetCurrentCamera();
  int error;
  BOOL intZoomOK;
  float expFactor = 1.0;

  // Can it be done with beam intensity calibration?
  delBeam = mShiftManager->GetPixelSize(camera, mSavedMagInd) /
    mShiftManager->GetPixelSize(camera, inMag);
  delBeam *= delBeam;
  error = mWinApp->mBeamAssessor->AssessBeamChange(delBeam, newIntensity, 
    newDelta, -1);
  if (!error || error == BEAM_ENDING_OUT_OF_RANGE) {

    // If so, turn OFF intensity zoom, and make the mag and intensity changes
    // reduce the beam change if necessary and change exposure to compensate
    mWinApp->mBeamAssessor->ChangeBeamStrength(delBeam / newDelta, -1);
    expFactor = (float)newDelta;
    mIntensityZoom = false;
      //CString message;
      //message.Format("Added exposure change of %.3f needed", newDelta);
      //mWinApp->AppendToLog(message, LOG_SWALLOW_IF_CLOSED);
  } else {

    // If not, rely on intensity zoom, and reduce the exposure time for a margin
    // of safety
    mIntensityZoom = true;
    expFactor = 0.4f;
  }
  intZoomOK = mScope->SetIntensityZoom(mIntensityZoom);

  // If intensity zoom is not OK, change flag and use exposure change entirely
  if (!intZoomOK && mIntensityZoom) {
    mIntensityZoom = false;
    expFactor *= (float)delBeam;
  }

  mScope->SetMagIndex(inMag);
  if (expFactor != 1.0)
      mConSets[TRACK_CONSET].exposure *= expFactor;
}

// Test for a start out of range: add  the zlp offset and see if it is positive
// Give message and stop the calibration if not
int CFilterTasks::TestCalMagStart(float start)
{
  if (!mFiltParams->positiveLossOnly || start + mFiltParams->refineZLPOffset >= 0)
    return 0;
  AfxMessageBox("The current scan cannot be started because\n"
    " this filter only allows positive energy shifts.\n\n"
    "You should do the \"Adjust Slit Offset\" procedure on the Filter Control Panel\n"
    "to get an energy offset big enough to accommodate half of the range to be "
    "scanned.", MB_EXCLAME);
  StopCalMagShift();
  return 1;
}

// Initialize a primary scan: start at minus half the energy range, adjust for slit
// because LossToApply will not (because mag shifts off), and adjust for old mag shuft
int CFilterTasks::InitializeRegularScan()
{
  float eStart = -mCMSEnergyRange / 2.f;
  if (mFiltParams->alignedSlitWidth && mFiltParams->adjustForSlitWidth)
    eStart += (mFiltParams->alignedSlitWidth - mFiltParams->slitWidth) / 2.f;
  float oldShift = mWinApp->mFilterControl.LookUpShift(mCurMag);
  if (oldShift > -900.)
    eStart += oldShift;
  if (TestCalMagStart(eStart))
    return 1;
  InitializeScan(eStart);
  return 0;
}

// Initialize energy and other variables for a scan  
void CFilterTasks::InitializeScan(float startEnergy)
{
  mEnergyCount = 0;
  mScanPeak = -1.e10;
  mPeakIndex = 0;
  mCurEnergy = startEnergy;
  mRedoing = false;
}

// Start an acquisition with the given control set and add dose to tasks
void CFilterTasks::StartAcquisition(DWORD delay, int conSetNum, int taskNum)
{
  mFiltParams->energyLoss = mCurEnergy;
  if (delay) {
    mCamera->SetupFilter();
    Sleep(delay);
  }
  if (taskNum == TASK_REFINE_ZLP && mLowDoseMode)
    mLDParam->energyLoss = mCurEnergy;

  mWinApp->mComplexTasks->StartCaptureAddDose(conSetNum);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, taskNum, 0, 0);
}

//
// START CALIBRATING MAG ENERGY SHIFTS
//
void CFilterTasks::CalibrateMagShift()
{
  CString *modeNames = mWinApp->GetModeNames();
  CameraParameters *camParams = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  ControlSet *conSet = &mConSets[TRACK_CONSET];
  int i, limlo, limhi;

  // This index can vary if there is a secondary mode
  mLowestMagInd = mWinApp->mComplexTasks->FindMaxMagInd(mCMSMinField);
  int highestMag = mMagTab[mLowestMagInd + mNumMags - 1].EFTEMmag;
 
  CString message = "To calibrate the mag-dependent energy shifts, dozens of\n"
          "pictures will be taken with the " + modeNames[2] + " parameter set\n";

  // Save the starting state of the system and reset image shift
  mSavedMagInd = mScope->GetMagIndex();
  mSavedIntensity = mScope->GetIntensity();
  mUsersIntensityZoom = mScope->GetIntensityZoom();
  *conSet = mConSets[2];
  mFullExposure = mConSets[2].exposure;
  mSavedParams = *mFiltParams;
  if (mWinApp->mShiftManager->ResetImageShift(false, false))
    return;

  // If beam shuttering is not OK, fix the control set to use regular shutter, no drift
  if (!camParams->DMbeamShutterOK && conSet->shuttering != USE_FILM_SHUTTER) {
    conSet->drift = 0.;
    conSet->shuttering = USE_FILM_SHUTTER;
    message += "(modified to use the regular shutter because beam shuttering is inaccurate)\n";
  }

  message += "\nThe binning should be set to give fast pictures (a subset area can be used).\n"
    "The beam and exposure time should be adjusted to give a moderate number of counts.\n\n"
    "The procedure will work best if you have aligned the zero loss peak recently and if\n"
    "no sample appears in the beam at the lowest mag\n\n";

  if (mWinApp->GetAdministrator())
    message += "The procedure will measure shifts twice, from the lowest to the highest mag\n"
      "then back to the lowest, because you are in Administrator mode.\n\n";
  else
    message += "The procedure will measure shifts once then stop\n"
      "because you are not in Administrator mode.\n\n";

  message += "Are you ready to proceed?";

  if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;

  // Get the parameters: cancel means cancel procedure
  message.Format("Highest mag to calibrate, starting from %d", 
    mMagTab[mLowestMagInd].EFTEMmag);
  if (!KGetOneInt(message, highestMag))
    return;
  mWinApp->GetMagRangeLimits(mWinApp->GetCurrentCamera(), mLowestMagInd, limlo, limhi);
  for (i = mLowestMagInd + 1; i <= limhi; i++)
    if (mMagTab[i].EFTEMmag > highestMag || 
      mMagTab[i].EFTEMmag <= mMagTab[i - 1].EFTEMmag)
      break;
  mNumMags = i - mLowestMagInd;

  if (!KGetOneFloat("Total range of energy shifts to scan", mCMSEnergyRange, 0))
    return;
  if (mCMSEnergyRange > 200.)
    mCMSEnergyRange = 200.;

  if (mFiltParams->positiveLossOnly && 
    mFiltParams->refineZLPOffset < mCMSEnergyRange / 2.) {
    AfxMessageBox("This filter only allows positive energy shifts.\n"
      "The current energy offset is less than half of the energy range you specified,\n"
      "so this procedure is unlikely to work correctly.\n\n"
      "You should do the \"Adjust Slit Offset\" procedure on the Filter Control Panel\n"
      "to get an energy offset big enough to accommodate half of the range to be "
      "scanned.", MB_EXCLAME);
    return;
  }

  if (!KGetOneFloat("Energy step size for scanning range", mCMSStepSize, 1))
    return;
  if (mCMSStepSize < 1.)
    mCMSStepSize = 1.;

  if (!KGetOneFloat("Slit width to use for scanning energy shifts", mCMSSlitWidth, 0))
    return;
  if (mCMSSlitWidth < mFiltParams->minWidth)
    mCMSSlitWidth = mFiltParams->minWidth;
  if (mCMSSlitWidth > mFiltParams->maxWidth)
    mCMSSlitWidth = mFiltParams->maxWidth;

  // Set the integration width, the minimum number of points to scan, and the
  // fuller extent that allows the scan to be extended once a peak is started
  mFullIntegrationWidth = mBasicIntegrationWidth + mCMSSlitWidth;
  mNumScan= (int)(mCMSEnergyRange / mCMSStepSize + 1.);
  mNumEnergies = (int)((mCMSEnergyRange + mFullIntegrationWidth)/ mCMSStepSize + 1.);

  // Set up filter parameters
  mFiltParams->slitIn = true;
  mFiltParams->slitWidth = mCMSSlitWidth;
  mFiltParams->doMagShifts = false;
  mFiltParams->zeroLoss = false;

  // Initialize the table of shifts; get arrays for scan
  for (i = 0; i < MAX_MAGS; i++) {
    mEnergyShifts[0][i] = -999.;
    mEnergyShifts[1][i] = -999.;
  }

  mEnergies = new float[mNumEnergies];
  mMeanCounts = new float[mNumEnergies];
  if (!mEnergies || !mMeanCounts) {
    AfxMessageBox("Failed to get memory for working tables", MB_EXCLAME);
    if (mEnergies)
      delete [] mEnergies;
    if (mMeanCounts)
      delete [] mMeanCounts;
    mEnergies = NULL;
    mMeanCounts = NULL;
    return;
  }

  // Go to the starting mag and set up counters
  MagShiftCalNewMag(mLowestMagInd);
  mCurMag = mLowestMagInd;
  mMagCount = 0;
  mDirInd = 0;
  mOverallPeak = 0.;
  
  mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING ENERGY SHIFTS");
  mWinApp->UpdateBufferWindows();
  if (InitializeRegularScan())
    return;
  mCamera->SetIgnoreFilterDiffs(true);        
  mWinApp->mFilterControl.UpdateSettings();

  // NEED MORE DELAY HERE
  // Fire off starting shot
  StartAcquisition(mBigEnergyShiftDelay, TRACK_CONSET, TASK_CAL_MAGSHIFT);
  return;
}

// DO the next task after getting a picture for cal mag shifts
void CFilterTasks::CalMagShiftNextTask()
{
  int i;
  float curMean;
  BOOL finished, foundPeak;
  float tailCrit, integralStart, integralEnd;
  CString report;

  mImBufs->mCaptured = BUFFER_CALIBRATION;
  if (mCurMag <= 0)
    return;

  // Get and save the mean count of the image
  curMean = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
  mMeanCounts[mEnergyCount] = curMean;
  SEMTrace('c', "%7.1f  %7.1f", mCurEnergy, curMean);
  mEnergies[mEnergyCount++] = mCurEnergy;
  mCurEnergy += mCMSStepSize;

  // Analyze for completion or need to restart
  // Keep track of peak and its position in this scan
  if (mScanPeak < curMean) {
    mScanPeak = curMean;
    mPeakIndex = mEnergyCount - 1;
  }

  // The criterion for tails below threshold
  tailCrit = mPeakThreshold * mScanPeak;
  integralStart = mEnergies[mPeakIndex] - mFullIntegrationWidth / 2.f;
  integralEnd = mEnergies[mPeakIndex] + mFullIntegrationWidth / 2.f;

  // Found a peak if the peak so far is big enough relative to ones on previous mags
  // or if it qualifies absolutely
  foundPeak = (mOverallPeak > 0. && mScanPeak > mMinPeakRatio * mOverallPeak) ||
    mScanPeak > mGoodPeakSize;

  // Done if reached the maximum limit for energies, or if the peak on this scan has
  // been high enough and the current value is below threshold, or if no peak was
  // found and we reached the scan limit
  finished = mEnergyCount >= mNumEnergies || (foundPeak && curMean < tailCrit) ||
    (!foundPeak && mEnergyCount >= mNumScan); 

  // If we never found a peak high enough, quit
  if (finished && (mScanPeak < mMinPeakSize || mOverallPeak > 0. && !foundPeak)) {
    AfxMessageBox("Cannot find a high enough peak in intensity.\n"
      "Check zero-loss alignment and exposure settings", MB_EXCLAME);
    StopCalMagShift();
    return;
  }

  // But need to restart if we are done and the end was too high and the peak is too
  // close to the end of the range, 
  // or if the peak has been found or scan is finished and the start is too high and
  // whatever peak occurred is too close to the start of the range
  if ((finished && curMean >= tailCrit && integralEnd > mEnergies[mEnergyCount - 1])
    || ((finished || foundPeak) && mMeanCounts[0] >= tailCrit &&
    integralStart < mEnergies[0])) {
    if (mRedoing) {
      if (finished) {
        report.Format("Warning: Peak is still too close to end after redoing "
          "scan at mag %d", mMagTab[mCurMag].EFTEMmag);
        mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      }
    } else {
      report.Format("Redoing mag %d because peak is too close to end of scan",
        mMagTab[mCurMag].EFTEMmag);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      if (TestCalMagStart(integralStart))
        return;
      InitializeScan(integralStart);
      mRedoing = true;
      StartAcquisition(mBigEnergyShiftDelay, TRACK_CONSET, TASK_CAL_MAGSHIFT);
      return; 
    }
  }

  // Get next energy if not done
  if (!finished) {
    StartAcquisition(mLittleEnergyShiftDelay, TRACK_CONSET, TASK_CAL_MAGSHIFT);
    return;
  }

  // DONE with scan: integrate around peak
  float countSum = 0;
  float productSum = 0;
  for (i = 0; i < mEnergyCount; i++) {
    if (mEnergies[i] >= integralStart && mEnergies[i] <= integralEnd) {
      countSum += mMeanCounts[i];
      productSum += mMeanCounts[i] * mEnergies[i];
    }
  }
  mEnergyShifts[mDirInd][mCurMag] = productSum / countSum;
  if (mOverallPeak < mScanPeak)
    mOverallPeak = mScanPeak;

  report.Format("Mag %d:  shift = %.1f", mMagTab[mCurMag].EFTEMmag, 
    mEnergyShifts[mDirInd][mCurMag]);
  mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

  // Increment mag counter and test if at end of mag loop
  mMagCount++;
  if (mMagCount >= mNumMags) {
    
    // Increment direction index and stop if we are really done
    mDirInd++;
    if (mDirInd > mWinApp->GetAdministrator() ? 1 : 0) {
      StopCalMagShift();
      return;
    }

    // Otherwise, we are just repeating the current mag.  Might as well start at
    // starting point of current scan
    mMagCount = 0;
    InitializeScan(mEnergies[0]);
    StartAcquisition(mBigEnergyShiftDelay, TRACK_CONSET, TASK_CAL_MAGSHIFT);
    return;
  }

  // Going to a new mag.  If intensity zoom is on, just go to the mag
  mCurMag += mDirInd ? -1 : 1;
  if (mIntensityZoom) {
    mScope->SetMagIndex(mCurMag);
  } else {
  
    // If not, restore intensity and exposure before going into routine
    mScope->SetIntensity(mSavedIntensity);
    mConSets[TRACK_CONSET].exposure = mFullExposure;
    MagShiftCalNewMag(mCurMag);
  }

  if (InitializeRegularScan())
    return;
  StartAcquisition(mBigEnergyShiftDelay, TRACK_CONSET, TASK_CAL_MAGSHIFT);
  return;
}


void CFilterTasks::CalMagShiftCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out calibrating energy shifts"), MB_EXCLAME);
  StopCalMagShift();
  mWinApp->ErrorOccurred(error);
}

void CFilterTasks::StopCalMagShift()
{
  int i, numDone = 0;
  float newShift;
  CString report;

  if (mCurMag <= 0)
    return;

  mCurMag = 0;
  CleanupArrays();
  *mFiltParams = mSavedParams;
  mScope->SetIntensityZoom(mUsersIntensityZoom);
  mScope->SetMagIndex(mSavedMagInd);
  mScope->SetIntensity(mSavedIntensity);

  // This should get ignored if camera is still busy
  mCamera->SetupFilter();
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->mFilterControl.UpdateSettings();

  // If fewer than intended number of mags measured, ask for confirmation
  // Have all cleanup work done before these test and confirmation steps
  for (i = 0; i < MAX_MAGS; i++) {
    if (mEnergyShifts[0][i] > -900. || mEnergyShifts[1][i] > -900.)
      numDone++;
  }

  if (!numDone)
    return;

  if (numDone < mNumMags) {
    report.Format("Only %d of the %d mags were measured\n\n"
      "Do you actually want to use these and modify\n"
      "the rest of the existing calibrations to match?", numDone, mNumMags);
    if (AfxMessageBox(report, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return;
  }

  // First get the mean value into the 0 column, and a change into the second
  for (i = 0; i < MAX_MAGS; i++) {
    if (mEnergyShifts[0][i] > -900. && mEnergyShifts[1][i] > -900.)
      mEnergyShifts[0][i] = (mEnergyShifts[0][i] + mEnergyShifts[1][i]) / 2.f;
    else if (mEnergyShifts[1][i] > -900.)
      mEnergyShifts[0][i] = mEnergyShifts[1][i];
    if (mFiltParams->magShifts[i] > -900.)
      mEnergyShifts[1][i] = mEnergyShifts[0][i] - mFiltParams->magShifts[i];
  }

  // Now replace the values in the table; and if there is no replacement value,
  // shift by the nearest difference
  for (i = 0; i < MAX_MAGS; i++) {
    newShift = -999.;
    if (mEnergyShifts[0][i] > -900.) {
      newShift = mEnergyShifts[0][i];
      if (mFiltParams->magShifts[i] > -900.)
        report.Format("Mag %6d   shift %7.1f   replacing %.1f   change %.1f",
          mMagTab[i].EFTEMmag, newShift, mFiltParams->magShifts[i],
          mEnergyShifts[1][i]);
      else
        report.Format("Mag %6d   shift %7.1f", mMagTab[i].EFTEMmag, newShift);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    } else if (mFiltParams->magShifts[i] > -900.) {
      
      // If no new value but there is an old one, find nearest difference and
      // apply it
      for (int iDel = 1; iDel < MAX_MAGS; iDel++) {
        for (int iDir = -1; iDir <= 1; iDir +=2) {
          int iMag = i + iDel * iDir;
          if (iMag < 1 || iMag >= MAX_MAGS)
            continue;
          if (mEnergyShifts[0][iMag] > -900.) {
            newShift = mFiltParams->magShifts[i] + mEnergyShifts[1][iMag];
            report.Format("Mag %6d   shift %7.1f  (adjusted by %.1f)",
              mMagTab[i].EFTEMmag, newShift, mEnergyShifts[1][iMag]);
            mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
            iDel = MAX_MAGS;
            break;
          }
        }
      }
    } 
    mFiltParams->magShifts[i] = newShift;
  }
  mCamera->SetupFilter();
  mWinApp->SetCalibrationsNotSaved(true);
}


//////////////////////////////////////////////////////////////////////////
//  ROUTINES FOR REFINING ZERO LOSS PEAK
//////////////////////////////////////////////////////////////////////////

// Start the procedure. Return false if error
BOOL CFilterTasks::RefineZLP(bool interactive)
{
  mLowDoseMode = mWinApp->LowDoseMode();
  ControlSet *conSet;
  CameraParameters *camParams = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  mSavedParams = *mFiltParams;
  CString *modeNames = mWinApp->GetModeNames();
  float cancelSlit = 0.;

  // Set up maximum scan size, and arrays
  mNumEnergies = (int)(1.5 * mRZlpSlitWidth / mRZlpStepSize);
  if (AllocateArrays(mNumEnergies))
    return false;

  // Find out if there is a user loss to cancel
  mRZlpUserLoss = 0.;
  if (interactive) {
    if (mLowDoseMode) {
      
      // In low dose mode, take the current area if it has slit in, otherwise take
      // the Record area if its slit is in
      mRZlpCancelLDArea = mScope->GetLowDoseArea();
      mLDPcancel = mWinApp->GetLowDoseParams() + mRZlpCancelLDArea;
      if (mRZlpCancelLDArea < 0 || !mLDPcancel->slitIn) {
        mRZlpCancelLDArea = 3;
        mLDPcancel = mWinApp->GetLowDoseParams() + mRZlpCancelLDArea;
      }
      if (mLDPcancel->slitIn && !mLDPcancel->zeroLoss) {
        mRZlpUserLoss = mLDPcancel->energyLoss;
        cancelSlit = mLDPcancel->slitWidth;
      }
      
    } else if (mFiltParams->slitIn && !mFiltParams->zeroLoss) {
      mRZlpUserLoss = mFiltParams->energyLoss;
      cancelSlit = mFiltParams->slitWidth;
    }
    
    // If loss is too big, ignore it; if it is above threshold fraction, ask
    if (mRZlpUserLoss > cancelSlit)
      mRZlpUserLoss = 0.;
    else if (mRZlpUserLoss != 0. && mRZlpUserLoss > mRZlpUserCancelFrac * cancelSlit) {
      CString message;
      if (mLowDoseMode)
        message.Format("The energy loss for the %s area is currently %.1f", 
        modeNames[mRZlpCancelLDArea], mRZlpUserLoss);
      else 
        message.Format("The energy loss is currently %.1f", mRZlpUserLoss);
      message += " eV.\n\nDo you want to use this loss as a starting offset\n"
        "and set the loss to zero when done?";
      if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO)
        mRZlpUserLoss = 0.;
    }
  }
  
  // Check for enough offset to work on JEOL
  if (TestRefineZLPStart(mRZlpUserLoss, "run")) {
    CleanupArrays();
    return false;
  }

  // Set up filter parameters
  mFiltParams->slitIn = true;
  mFiltParams->slitWidth = mRZlpSlitWidth;
  mFiltParams->zeroLoss = false;

  // Use preview set in low dose, trial in regular mode
  mRZlpConSetNum = mLowDoseMode ? 4 : 2;
  mLDParam = mWinApp->GetLowDoseParams() + mCamera->ConSetToLDArea(mRZlpConSetNum);
  if (mLowDoseMode) {
    mLDPsave = *mLDParam;
    mLDParam->slitIn = true;
    mLDParam->slitWidth = mRZlpSlitWidth;
    mLDParam->zeroLoss = false;
  }

  conSet = &mConSets[mRZlpConSetNum];
  mRZlpConSetSave = *conSet;
  conSet->drift = 0.;
  conSet->forceDark = 0;
  conSet->mode = SINGLE_FRAME;
  conSet->saveFrames = 0;
  conSet->doseFrac = 0;

  // Turn off super-res mode so that exposure time constraints are finer
  if (conSet->K2ReadMode == K2_SUPERRES_MODE)
    conSet->K2ReadMode = K2_COUNTING_MODE;
  if (conSet->processing == UNPROCESSED)
    conSet->processing = DARK_SUBTRACTED;

  // Drop the exposure by 10 if possible.  Use dead time and also apply minimum exposure
  // then do standard constraint
  conSet->exposure = (conSet->exposure - camParams->deadTime) / 10.f + camParams->deadTime;
  conSet->exposure = B3DMAX(conSet->exposure, mRZlpMinExposure);
  mCamera->ConstrainExposureTime(camParams, conSet);
  
  // If exposure dropped by at least 4, try to increase binning by 2
  if ((mFullExposure - camParams->deadTime) >= 
    (conSet->exposure - camParams->deadTime) * 4.) {
    for (int i = 1; i < camParams->numBinnings; i++) {
      if (camParams->binnings[i] == 2 * conSet->binning) {
        conSet->binning *= 2;
        break;
      }
    }
  }

  // Set up starting energy and other scan parameters
  mRZlpTrial = 1;
  mCurEnergy = mRZlpUserLoss;
  mRZlpIndex = 0;
  mPeakDelta = mPeakMean = -1.e10;
  mPeakIndex = -1;
  
  mWinApp->SetStatusText(MEDIUM_PANE, "REFINING ZERO LOSS PEAK");
  mWinApp->UpdateBufferWindows();
  mCamera->SetIgnoreFilterDiffs(true);
  mWinApp->mFilterControl.UpdateSettings();

  // Fire off starting shot
  StartAcquisition(mBigEnergyShiftDelay, mRZlpConSetNum, TASK_REFINE_ZLP);
  return true;
}

// Process the image
void CFilterTasks::RefineZLPNextTask()
{
  float tradeoffFactor = 3.;     // Amount by which one ratio can make up for another
  float curMean, delta, cy, y1, y3, denom, meanRatio, deltaRatio;
  CString report;
  BOOL badEnergy = (mFiltParams->positiveLossOnly && 
    mWinApp->mFilterControl.LossToApply() < 0.);
  BOOL lastEnergy = (mFiltParams->positiveLossOnly && 
    mWinApp->mFilterControl.LossToApply() < mRZlpStepSize);

  mImBufs->mCaptured = BUFFER_CALIBRATION;
  if (mRZlpIndex < 0)
    return;

  // Get and save the mean count of the image
  curMean = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
  mMeanCounts[mRZlpIndex] = curMean;
  delta = 0.;
  if (mRZlpIndex > 0) {
    delta = mMeanCounts[mRZlpIndex - 1] - curMean;
    mDeltaCounts[mRZlpIndex - 1] = delta;
    
    // Keep track of peak and its position
    if (mPeakDelta < delta) {
      mPeakDelta = delta;
      mPeakIndex = mRZlpIndex - 1;
    }
  }

  // Keep track of maximum counts too
  if (mPeakMean < curMean)
    mPeakMean = curMean;

  SEMTrace('F', "%7.1f  %7.1f  %7.1f", mCurEnergy + mRZlpSlitWidth / 2., curMean, delta);
  mEnergies[mRZlpIndex++] = mCurEnergy;
  mCurEnergy -= mRZlpStepSize;

  // Allow the two criteria to trade against each other up to the limit set by factor
  meanRatio = 1.;
  if (mPeakMean > 1.)
    meanRatio = B3DMAX(curMean / mPeakMean, mRZlpMeanCrit / tradeoffFactor);
  deltaRatio = 1.;
  if (mPeakDelta > 1.)
    deltaRatio = B3DMAX(delta / mPeakDelta, mRZlpDeltaCrit / tradeoffFactor);

  // Done if we have points on either side of the peak and both the delta and the
  // mean counts has fallen below criterion, or the constrained ratio product is
  // below the product of criteria
  if (!badEnergy && mPeakIndex > 0 && mPeakIndex < mRZlpIndex - 1 && 
    ((delta < mPeakDelta * mRZlpDeltaCrit && curMean < mPeakMean * mRZlpMeanCrit) ||
    deltaRatio * meanRatio < mRZlpDeltaCrit * mRZlpMeanCrit)) {

    // Do parabolic fit to peak
    cy = 0.;
    y1 = mDeltaCounts[mPeakIndex - 1];
    y3 = mDeltaCounts[mPeakIndex + 1];
    denom = 2.f * (y1 + y3 - 2.f * mDeltaCounts[mPeakIndex]);
    if(denom != 0.0)
      cy = (y1 - y3) / denom;
    if (cy > 0.5)
      cy = 0.5f;
    if (cy < -0.5)
      cy = -0.5f;

    // Multiply fractional value by step (which is negative) and add the peak location
    cy = -cy * mRZlpStepSize + 
      0.5f *(mEnergies[mPeakIndex] + mEnergies[mPeakIndex + 1] +  mRZlpSlitWidth);
    
    // Implement by adding to saved parameter value, then restore and quit
    mSavedParams.refineZLPOffset += cy;
    mSavedParams.alignZLPTimeStamp = 0.001 * GetTickCount();
    mSavedParams.cumulNonRunTime = 0;
    mSavedParams.usedOldAlign = false;
    mWinApp->mDocWnd->SetShortTermNotSaved();
    report.Format("Refine ZLP: additional offset = %7.1f eV; total offset = %7.1f eV", cy, 
      mSavedParams.refineZLPOffset);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    
    // Set an energy loss to zero: in the Low dose set, in the saved set if that is
    // the record set, and in the filter params if that is low dose area
    if (mRZlpUserLoss) {
      report.Format("Setting energy loss of %.1f to 0.", mRZlpUserLoss);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      if (mLowDoseMode) {
        mLDPcancel->energyLoss = 0.;
        if (mRZlpCancelLDArea == 3)
          mLDPsave.energyLoss = 0.;
        if (mRZlpCancelLDArea == mScope->GetLowDoseArea())
          mSavedParams.energyLoss = 0.;
      } else
        mSavedParams.energyLoss = 0.;
    }
    StopRefineZLP();
    return;
  }

  // Need to restart if limit of index is reached, or if the peak is at the first point
  // and either delta or mean value has fallen below criterion fraction of its peak
  if (lastEnergy || badEnergy || mRZlpIndex >= mNumEnergies || 
    (mRZlpIndex > 1 && !mPeakIndex &&
    (delta < mPeakDelta * mRZlpDeltaCrit && curMean < mPeakMean * mRZlpMeanCrit))) {
    
    // Third time or low dose mode, give up
    if (mRZlpTrial >= 3 || (mLowDoseMode && !mRZlpRedoInLowDose)) {
      StopRefineZLP();
      report = "The Refine ZLP routine failed to find the zero-loss peak.";
      if (lastEnergy || badEnergy)
        report += "\n\nThis may be because the net energy shift was not able to "
        "become negative.\nYou should do the \"Adjust Slit Offset\" procedure.";
      mWinApp->mTSController->TSMessageBox(report);
      mWinApp->ErrorOccurred(1);
      return;
    }

    // It is safer to go to the left first then go right on the last trial
    if (mRZlpTrial++ == 1) {
      mCurEnergy = mRZlpUserLoss - mRZlpSlitWidth;
      if (TestRefineZLPStart(mCurEnergy, "run a second scan of")) {
        StopRefineZLP();
        mWinApp->ErrorOccurred(1);
        return;
      }

    } else
      mCurEnergy = mRZlpUserLoss + mRZlpSlitWidth;
    report.Format("Restarting ZLP search with offset of %7.1f eV", mCurEnergy);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

    // Reset parameters and go on
    mRZlpIndex = 0;
    mPeakDelta = mPeakMean = -1.e10;
    mPeakIndex = -1;
    StartAcquisition(mBigEnergyShiftDelay, mRZlpConSetNum, TASK_REFINE_ZLP);
    return;
  }

  // Take next picture
  StartAcquisition(mLittleEnergyShiftDelay, mRZlpConSetNum, TASK_REFINE_ZLP);
  return;
}

void CFilterTasks::RefineZLPCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mWinApp->mTSController->TSMessageBox(_T("Time out refining ZLP")); 
  StopRefineZLP();
  mWinApp->ErrorOccurred(error);
}

// Stop the refine ZLP operation
void CFilterTasks::StopRefineZLP()
{
  if (mRZlpIndex < 0)
    return;

  // Restore parameters
  *mFiltParams = mSavedParams;
  mConSets[mRZlpConSetNum] = mRZlpConSetSave;
  if (mLowDoseMode) {
    *mLDParam = mLDPsave;

    // If the current area is the area used for pictures, restore main
    // params from it so they will display correctly in filter control
    if (mScope->GetLowDoseArea() == mCamera->ConSetToLDArea(mRZlpConSetNum)) {
      mFiltParams->slitIn = mLDParam->slitIn;
      mFiltParams->slitWidth = mLDParam->slitWidth;
      mFiltParams->energyLoss = mLDParam->energyLoss;
      mFiltParams->zeroLoss = mLDParam->zeroLoss;
    }
  }

  CleanupArrays();
  mRZlpIndex = -1;

  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->mFilterControl.UpdateSettings();
  mCamera->SetupFilter();
}

// Allocate arrays; return true if error
BOOL CFilterTasks::AllocateArrays(int size)
{
  mEnergies = new float[size];
  mMeanCounts = new float[size];
  mDeltaCounts = new float[size];
  if (!mEnergies || !mMeanCounts || !mDeltaCounts) {
    mWinApp->mTSController->TSMessageBox("Failed to get memory for working tables"); 
    CleanupArrays();
    return true;
  }
  return false;
}

// Delete arrays if they were allocated
void CFilterTasks::CleanupArrays()
{
  if (mEnergies)
    delete [] mEnergies;
  if (mMeanCounts)
    delete [] mMeanCounts;
  if (mDeltaCounts)
    delete [] mDeltaCounts;
  mEnergies = NULL;
  mMeanCounts = NULL;
  mDeltaCounts = NULL;
}

// Test for whether a scan of the refine ZLP procedure has enough range to work
// when only positive offsets are allowed
int CFilterTasks::TestRefineZLPStart(float eStart, CString scanType)
{
  float netLoss;
  if (mFiltParams->positiveLossOnly) {
    netLoss = mFiltParams->refineZLPOffset + eStart;
    if (mFiltParams->doMagShifts)
      netLoss += mFiltParams->currentMagShift;
    if (netLoss < 0.75 * mRZlpSlitWidth) {
      mWinApp->mTSController->TSMessageBox(
        "This filter only allows positive energy shifts.\nThe current net energy "
        "shift is not big enough to " + scanType + " the Refine ZLP procedure.\n\n"
        "You should do the \"Adjust Slit Offset\" procedure\n");
      return 1;
    }
  }
  return 0;
}
