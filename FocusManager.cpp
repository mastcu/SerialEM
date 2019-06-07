// FocusManager.cpp:      Measures beam-tilt dependent image shifts, calibrates and
//                         does autofocusing
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\FocusManager.h"
#include "ShiftManager.h"
#include "Utilities\XCorr.h"
#include "EMscope.h"
#include "CameraController.h"
#include "EMbufferManager.h"
#include "Utilities\KGetOne.h"
#include "ProcessImage.h"
#include "MacroProcessor.h"
#include "TSController.h"
#include "ComplexTasks.h"
#include "EMmontageController.h"
#include "AutoTuning.h"
#include "Utilities\STEMfocus.h"
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

enum {STEM_FOCUS_CAL, STEM_FOCUS_COARSE1, STEM_FOCUS_COARSE2, STEM_FOCUS_FINE1,
STEM_FOCUS_FINE2};

BEGIN_MESSAGE_MAP(CFocusManager, CCmdTarget)
  //{{AFX_MSG_MAP(CFocusManager)
  ON_COMMAND(ID_CALIBRATION_AUTOFOCUS, OnCalibrationAutofocus)
  ON_COMMAND(ID_FOCUS_AUTOFOCUS, OnFocusAutofocus)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_AUTOFOCUS, OnUpdateFocusAutofocus)
  ON_COMMAND(ID_FOCUS_DRIFTPROTECTION, OnFocusDriftprotection)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_DRIFTPROTECTION, OnUpdateFocusDriftprotection)
  ON_COMMAND(ID_FOCUS_MEASUREDEFOCUS, OnFocusMeasuredefocus)
  ON_COMMAND(ID_FOCUS_MOVECENTER, OnFocusMovecenter)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_MOVECENTER, OnUpdateFocusMovecenter)
  ON_COMMAND(ID_FOCUS_REPORTSHIFTDRIFT, OnFocusReportshiftdrift)
  ON_COMMAND(ID_FOCUS_SETBEAMTILT, OnFocusSetbeamtilt)
  ON_COMMAND(ID_FOCUS_SETTARGET, OnFocusSettarget)
  ON_COMMAND(ID_FOCUS_SETTHRESHOLD, OnFocusSetthreshold)
  ON_COMMAND(ID_CALIBRATION_SETFOCUSRANGE, OnCalibrationSetfocusrange)
  ON_COMMAND(ID_FOCUS_VERBOSE, OnFocusVerbose)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_VERBOSE, OnUpdateFocusVerbose)
  ON_COMMAND(ID_FOCUS_CHECKAUTOFOCUS, OnFocusCheckautofocus)
	ON_COMMAND(ID_FOCUS_REPORTONEXISTING, OnFocusReportonexisting)
	ON_UPDATE_COMMAND_UI(ID_FOCUS_REPORTONEXISTING, OnUpdateFocusReportonexisting)
	ON_COMMAND(ID_FOCUS_SETOFFSET, OnFocusSetoffset)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_MEASUREDEFOCUS, OnUpdateFocusAutofocus)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETTHRESHOLD, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_AUTOFOCUS, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_REPORTSHIFTDRIFT, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETBEAMTILT, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SETFOCUSRANGE, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SET_TILT_DIRECTION, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SET_DDD_MIN_BINNING, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SET_FILTER_CUTOFFS, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETABSOLUTELIIMITS, OnUpdateNoTasksNoSTEM)
	ON_COMMAND(ID_FOCUS_RESETDEFOCUS, OnFocusResetdefocus)
  ON_COMMAND(ID_FOCUS_SHOWEXISTINGCORR, OnFocusShowexistingcorr)
	//}}AFX_MSG_MAP
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETTARGET, OnUpdateFocusSettarget)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_CHECKAUTOFOCUS, OnUpdateFocusCheckautofocus)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SHOWEXISTINGCORR, OnUpdateFocusShowexistingcorr)
  ON_COMMAND(ID_CALIBRATION_STEMFOCUSVSZ, OnStemFocusVsZ)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_STEMFOCUSVSZ, OnUpdateStemFocusVsZ)
  ON_COMMAND(ID_FOCUS_SET_TILT_DIRECTION, OnFocusSetTiltDirection)
  ON_COMMAND(ID_AUTOFOCUSFOCUS_LISTCALIBRATIONS, OnAutofocusListCalibrations)
  ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_LISTCALIBRATIONS, OnUpdateNoTasks)
  ON_COMMAND(ID_FOCUS_SET_DDD_MIN_BINNING, OnFocusSetDddMinBinning)
  ON_COMMAND(ID_FOCUS_SET_FILTER_CUTOFFS, OnFocusSetFilterCutoffs)
  ON_COMMAND(ID_SPECIALOPTIONS_NORMALIZEVIAVIEWFORAU, OnNormalizeViaView)
  ON_UPDATE_COMMAND_UI(ID_SPECIALOPTIONS_NORMALIZEVIAVIEWFORAU, OnUpdateNormalizeViaView)
  ON_COMMAND(ID_FOCUS_SHOWEXISTINGSTRETCH, OnShowExistingStretch)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SHOWEXISTINGSTRETCH, OnUpdateFocusShowexistingcorr)
  ON_COMMAND(ID_FOCUS_SETABSOLUTELIIMITS, OnFocusSetAbsoluteLimits)
  ON_COMMAND(ID_FOCUS_USEABSOLUTERANGE, OnFocusUseAbsoluteLimits)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_USEABSOLUTERANGE, OnUpdateFocusUseAbsoluteLimits)
  ON_COMMAND(ID_FOCUS_TESTWHENUSINGOFFSET, OnLimitOffsetDefocus)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_TESTWHENUSINGOFFSET, OnUpdateLimitOffsetDefocus)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFocusManager::CFocusManager()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mConSets = mWinApp->GetConSets();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mImBufs = mWinApp->GetImBufs();
  mFocusIndex = -1;
  mFCindex = -1;
  for (int k = 0; k < 5; k++)
    mFocusBuf[k] = NULL;
  mScope = NULL;

  mCurrentDefocus = 0.;
  mTargetDefocus = 0.;
  mRefocusThreshold = 8.;
  mAbortThreshold = 1.;
  mNumCalLevels = 13;
  mSmoothFit = 3;
  mCalRange = 72.;
  mCalOffset = 0.;
  mFalconCalLimit = 18.;
  mFracTiltX = 1.;
  mFracTiltY = 0.;
  mTiltDirection = 0;
  mBeamTilt = 5.;
  mPostTiltDelay = 0;
  mLastFailed = false;
  mLastAborted = false;
  mRequiredBWMean = -1.;
  mVerbose = false;
  mAccuracyMeasured = false;
  mCheckDelta = 5.f;
  mTripleMode = true;
  mDefocusOffset = 0.;
  mMaxPeakDistanceRatio = 3.;
  mPadFrac = 0.1f;       // Fraction of largest dimension: pad equally in X and Y
  mTaperFrac = 0.1f;     // Fraction to taper, applied separately to X and Y
  mSigma1 = 0.04f;       // Sigma1 for the filtering
  mSigma2 = 0.05f;
  mRadius2 = 0.25f;
  mDDDminBinning = 1;
  mNormalizeViaView = false;
  mViewNormDelay = 500;
  mDriftStored = false;
  mUseOppositeLDArea = false;
  mNextMinDeltaFocus = 0.;
  mNextMaxDeltaFocus = 0.;
  mNextMinAbsFocus = 0.;
  mNextMaxAbsFocus = 0.;
  mNextXtiltOffset = 0.;
  mNextYtiltOffset = 0.;
  mEucenMinAbsFocus = 0.;
  mEucenMaxAbsFocus = 0.;
  mEucenMaxDefocus = 20.f;
  mEucenMinDefocus = -20.f;
  mUseEucenAbsLimits = false;
  mTestOffsetEucenAbs = true;
  mRFTbkgdStart = 0.4f;
  mRFTbkgdEnd = 0.5f;
  mRFTtotPowStart = 0.0;
  mRFTtotPowEnd = 0.15f;
  mRFTnumPoints = 100;
  mRFTfitStart = 0.;
  mRFTfitEnd = 0.2f;
  mSFnumCalSteps = 11;
  mSFcalRange = 8.;
  mNumRotavs = -1;
  mNumSincPts = 10000;
  mNumSincNodes = 50;
  mSFcoarseOnly = false;
  mSFbacklash = 0.;
  mSFstepLimit = 11;
  mSFmidFocusStep = 0.3f;
  mSFminFocusStep = 0.12f;
  mSFmaxFocusStep = 0.75f;
  mBackupSlope = 10.;
  mSFnormalizedSlope[0] = mSFnormalizedSlope[1] = 0.;
  mSincCurve = NULL;
  mSFtargetSize = 6.;
  mSFmaxTiltedSizeRange = 5.;
  mSFminTiltedFraction = 0.15f;
  mSFbestImBuf = NULL;
  mSFshowBestAtEnd = true;
  mModeWasContinuous = false;
  mSFVZindex = -3;
  mSTEMdefocusToDelZ[0] = 1.;    // Placeholder
  mSTEMdefocusToDelZ[1] = 30.;    // Close to what microprobe had!
  mSFVZbacklashZ = -1.;
  mSFVZinitialDelZ = 3.;
}

CFocusManager::~CFocusManager()
{

}


void CFocusManager::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  if (mSFVZbacklashZ < 0)
    mSFVZbacklashZ = FEIscope ? 3.f : 10.f;
  mSFVZrangeInZ = FEIscope ? 24.f : 20.f;
  mSFVZnumLevels = FEIscope ? 13 : 11;
}

/////////////////////////////////////////////////////////////////////////////
// FOCUS MESSAGES
/////////////////////////////////////////////////////////////////////////////

void CFocusManager::OnUpdateNoTasks(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CFocusManager::OnUpdateNoTasksNoSTEM(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode());
}

void CFocusManager::OnCalibrationAutofocus() 
{
  CalFocusStart();
}

void CFocusManager::OnFocusAutofocus() 
{
  AutoFocusStart(1);
}

void CFocusManager::OnUpdateFocusAutofocus(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && FocusReady() && !mScope->GetMovingStage());   
}

void CFocusManager::OnFocusDriftprotection() 
{
  mTripleMode = !mTripleMode;
}

void CFocusManager::OnUpdateFocusDriftprotection(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(mTripleMode ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode());
}

void CFocusManager::OnFocusSetDddMinBinning()
{
  if (!KGetOneInt("Images from direct detectors will be binned and/or filter parameters"
    " scaled", "to achieve the same effect as having images with this total amount of "
    "binning:",mDDDminBinning))
    return;
  B3DCLAMP(mDDDminBinning, 1, 4);
}

void CFocusManager::OnFocusSetFilterCutoffs()
{
  if (!KGetOneFloat("Frequency to start cutoff for high-frequency filter (lower value "
    "filters more)", mRadius2, 2))
    return;
  B3DCLAMP(mRadius2, 0.05f, 0.5f);
  if (!KGetOneFloat("Sigma for rolloff of high-frequency filter (lower value filters"
    " more)", mSigma2, 3))
    return;
  B3DCLAMP(mSigma2, 0.001f, 0.3f);
  if (!KGetOneFloat("Sigma for rolloff of low-frequency filter (higher value filters"
    " more)", mSigma1, 3))
    return;
  B3DCLAMP(mSigma1, 0.0f, 0.2f);
}

// Record absolute focus values around eucentric focus
void CFocusManager::OnFocusSetAbsoluteLimits()
{ double eucenFocus;
  float minDefocus;
  if (AfxMessageBox("Set microscope to eucentric focus,\n"
    " or go to eucentric height and focus a specimen", MB_OKCANCEL | MB_ICONINFORMATION)
    != IDOK)
    return;
  eucenFocus = mScope->GetFocus();
  minDefocus = mEucenMinDefocus;
  if (!KGetOneFloat("Maximum negative defocus change from eucentric focus",
    minDefocus, 1))
    return;
  if (!KGetOneFloat("Maximum positive defocus change from eucentric focus",
    mEucenMaxDefocus, 1))
    return;
  mEucenMinDefocus = -B3DABS(minDefocus);
  mEucenMaxDefocus = B3DABS(mEucenMaxDefocus);
  mScope->IncDefocus(mEucenMinDefocus);
  mEucenMinAbsFocus = mScope->GetFocus();
  mScope->IncDefocus(mEucenMaxDefocus - mEucenMinDefocus);
  mEucenMaxAbsFocus = mScope->GetFocus();
  mScope->IncDefocus(-mEucenMaxDefocus);
}


void CFocusManager::OnFocusUseAbsoluteLimits()
{
  mUseEucenAbsLimits = !mUseEucenAbsLimits; 
}

void CFocusManager::OnUpdateFocusUseAbsoluteLimits(CCmdUI *pCmdUI)
{
  bool haveLimits = mEucenMinAbsFocus || mEucenMaxAbsFocus;
  pCmdUI->SetCheck((mUseEucenAbsLimits && haveLimits) ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && haveLimits);
}

void CFocusManager::OnLimitOffsetDefocus()
{
  mTestOffsetEucenAbs = !mTestOffsetEucenAbs;
}

void CFocusManager::OnUpdateLimitOffsetDefocus(CCmdUI *pCmdUI)
{
  bool haveLimits = mEucenMinAbsFocus || mEucenMaxAbsFocus;
  pCmdUI->SetCheck((mTestOffsetEucenAbs && haveLimits) ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && haveLimits && 
    mUseEucenAbsLimits);
}

void CFocusManager::OnFocusMeasuredefocus() 
{
  AutoFocusStart(0);
}

void CFocusManager::OnFocusCheckautofocus() 
{
  CheckAccuracy(LOG_MESSAGE_IF_CLOSED); 
}

void CFocusManager::OnFocusReportshiftdrift() 
{
  DetectFocus(FOCUS_REPORT);
}

void CFocusManager::OnFocusReportonexisting() 
{
 if (mWinApp->GetSTEMMode()) {
    float focus;
    int numpic = mBufferManager->GetShiftsOnAcquire() + 1;
    if (!KGetOneInt("Number of images to analyze:", numpic))
      return;
    B3DCLAMP(numpic, 3, MAX_BUFFERS - 3);
    if (!mSFnormalizedSlope[mScope->GetProbeMode()])
      KGetOneFloat("Normalized slope to use:", mBackupSlope, 2);
    mDoChangeFocus = false;
    mNumRotavs = 0;
    mSFtask = STEM_FOCUS_COARSE1;
    mCalDelta = 1.;
    mSFbaseFocus = mScope->GetUnoffsetDefocus();
    for (int buf = numpic; buf > 0; buf--) {
      if (!mImBufs[buf - 1].GetDefocus(focus)) {
        AfxMessageBox("There is no defocus value for one of the images", MB_EXCLAME);
        break;
      }
      mCalDefocus = focus;
      if (!buf && !mSFcoarseOnly)
        mSFtask = STEM_FOCUS_FINE2;
      STEMfocusShot(buf);
    }
    StopSTEMfocus();

  } else
    DetectFocus(FOCUS_EXISTING);	
}

void CFocusManager::OnFocusShowexistingcorr()
{
  DetectFocus(FOCUS_SHOW_CORR);	
}

// Make sure all needed images exist and are the same size
void CFocusManager::OnUpdateFocusReportonexisting(CCmdUI* pCmdUI)  
{
  int nx, ny;
  BOOL enable = !mWinApp->DoingTasks() && mImBufs[0].mImage && mImBufs[1].mImage && 
    (!(mTripleMode || mWinApp->GetSTEMMode()) || mImBufs[2].mImage);
  if (enable) {
     mImBufs[0].mImage->getSize(nx, ny);
     enable &= (mImBufs[1].mImage->getWidth() == nx) && 
       (mImBufs[1].mImage->getHeight() == ny);
     if (mTripleMode || mWinApp->GetSTEMMode())
       enable &= (mImBufs[2].mImage->getWidth() == nx) && 
       (mImBufs[2].mImage->getHeight() == ny);
  }
  pCmdUI->Enable(enable);
}

void CFocusManager::OnShowExistingStretch()
{
  DetectFocus(FOCUS_SHOW_STRETCH);	
}

void CFocusManager::OnUpdateFocusCheckautofocus(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && FocusReady());
}

void CFocusManager::OnUpdateFocusShowexistingcorr(CCmdUI *pCmdUI)
{
  if (mWinApp->GetSTEMMode())
    pCmdUI->Enable(false);
  else
    OnUpdateFocusReportonexisting(pCmdUI);
}

void CFocusManager::OnFocusSetbeamtilt() 
{
  CString message = !FEIscope ? "Beam tilt for autofocus, as % of full scale:" :
    "Beam tilt for autofocus, in milliradians:";
  float oldVal = (float)GetBeamTilt();

  if (!KGetOneFloat(message, oldVal, 1))
    return;
  if (oldVal < 0.)
    oldVal *= -1.;
  SetBeamTilt(oldVal);
}

void CFocusManager::OnFocusResetdefocus() 
{
  mScope->ResetDefocus();	
}

void CFocusManager::OnFocusVerbose() 
{
  mVerbose = !mVerbose; 
}

void CFocusManager::OnUpdateFocusVerbose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mVerbose ? 1 : 0);
}

void CFocusManager::OnFocusSettarget() 
{
  float oldVal = GetTargetDefocus();
  KGetOneFloat("Target defocus in microns:", oldVal, 2);
  SetTargetDefocus(oldVal);
}

void CFocusManager::OnUpdateFocusSettarget(CCmdUI *pCmdUI)
{
  TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() &&
    !(mWinApp->StartedTiltSeries() && 
    tsParam->doVariations && mWinApp->mTSController->GetTypeVaries(TS_VARY_FOCUS)));
}

void CFocusManager::OnFocusSetthreshold() 
{
  float oldVal = GetRefocusThreshold();
  if (!KGetOneFloat("Threshold change for doing another autofocus run:", oldVal, 2))
    return;
  SetRefocusThreshold(B3DABS(oldVal));
  oldVal = GetAbortThreshold();
  if (!KGetOneFloat("Threshold change for aborting autofocus when successive autofocus",
    "runs give inconsistent results (or 0 to disable this feature):", oldVal, 2))
    return;
  SetAbortThreshold(B3DABS(oldVal));
}

void CFocusManager::OnFocusSetoffset() 
{
  KGetOneFloat("Amount to change focus before taking autofocus pictures:", mDefocusOffset, 2);
}

void CFocusManager::OnCalibrationSetfocusrange() 
{
  CString info;
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  if (IS_FALCON2_OR_3(camParam) && mCalRange > 72 - mFalconCalLimit)
    info.Format("You might need a range of only %.0f on the Falcon", 72 - mFalconCalLimit);
  if (!KGetOneFloat(info, "Total defocus range to measure shifts over:", mCalRange, 1))
    return;
  if (!KGetOneInt("Number of focus levels to test:", mNumCalLevels))
    return;
  mNumCalLevels = B3DMIN(mNumCalLevels, MAX_CAL_FOCUS_LEVELS);
  if (!KGetOneInt("Number of levels to smooth over (0 for none):", mSmoothFit))
    return;
  info = "";
  if (IS_FALCON2_OR_3(camParam) && mCalRange / 2. + mCalOffset > mFalconCalLimit)
    info.Format("If starting near focus, you might need an offset of %.0f because of "
    "the dose protector.", mFalconCalLimit - mCalRange / 2.);
  KGetOneFloat(info, "Offset to apply to the focus range (negative for more "
    "underfocused):", mCalOffset, 1);
}

void CFocusManager::OnFocusSetTiltDirection()
{
  int current = mTiltDirection;
  if (!KGetOneInt("Direction 0 uses X beam tilt coil, 2 uses Y, 1 and 3 use both",
    "Enter beam tilt direction, a number from 0 to 3:", current))
    return;
  SetTiltDirection(current);
  mWinApp->UpdateBufferWindows();
}

// Change the focus so that it would move center of focus from the user's
// specified point to the center of the field
void CFocusManager::OnFocusMovecenter() 
{
  int nx, ny;
  float shiftX, shiftY, specY, angle, focusFac;
  double newFocus;
  focusFac = mShiftManager->GetDefocusZFactor() * 
    (mShiftManager->GetStageInvertsZAxis() ? -1 : 1);
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  ScaleMat aInv = mShiftManager->CameraToSpecimen(imBuf->mMagInd);
  if (!aInv.xpx)
    return;
  SEMTrace('1', "Camera to Specimen %f  %f  %f  %f", aInv.xpx, aInv.xpy, aInv.ypx, aInv.ypy);
  imBuf->mImage->getSize(nx, ny);
  imBuf->mImage->getShifts(shiftX, shiftY);
  
  // Coordinates in field are sum of image align shift and user point coord
  shiftX += imBuf->mUserPtX - nx / 2;
  shiftY += imBuf->mUserPtY - ny / 2;

  // Convert to specimen coordinates, invert shiftY at this point.  Only need Y
  specY = imBuf->mBinning * (aInv.ypx * shiftX - aInv.ypy * shiftY);
  if (!imBuf->GetTiltAngle(angle))
    angle = (float)mScope->GetTiltAngle();
  if (mWinApp->GetSTEMMode())
     focusFac = -1.f / GetSTEMdefocusToDelZ(-1);;
  newFocus = imBuf->mDefocus + specY * tan(DTOR * angle) * focusFac;
  if (!mWinApp->GetSTEMMode() && mWinApp->LowDoseMode() && imBuf->mLowDoseArea && 
    IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed))
    newFocus -= mScope->GetLDViewDefocus(imBuf->mConSetUsed);
  SEMTrace('1', "Shift %.1f %.1f  specY %.2f focusFac  %f  newFocus %.2f", shiftX, shiftY,
    specY, focusFac, newFocus);
  mScope->SetUnoffsetDefocus(newFocus);
}

// For this operation, require that active image exist and have a user point,
// binning and a mag index
void CFocusManager::OnUpdateFocusMovecenter(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(!mWinApp->DoingTasks() && imBuf->mImage != NULL && 
    imBuf->mHasUserPt && imBuf->mMagInd && imBuf->mBinning);  
}

void CFocusManager::OnAutofocusListCalibrations()
{
  CameraParameters *camP = mWinApp->GetCamParams();
  MagTable *magTab = mWinApp->GetMagTable();
  int numCam = mWinApp->GetNumActiveCameras();
  int *active = mWinApp->GetActiveCameraList();
  int ind, iCam, iMag;
  CArray <AstigCalib, AstigCalib> *astigCal = mWinApp->mAutoTuning->GetAstigCals();
  CArray <ComaCalib, ComaCalib> *comaCal = mWinApp->mAutoTuning->GetComaCals();
  CArray <CtfBasedCalib, CtfBasedCalib> *ctfAstigCals =
    mWinApp->mAutoTuning->GetCtfBasedCals();
  CArray<STEMFocusZTable, STEMFocusZTable> *focusZtables = 
    mWinApp->mFocusManager->GetSFfocusZtables();
  STEMFocusZTable sfzTable;
  CtfBasedCalib ctfCal;
  AstigCalib astig;
  ComaCalib coma;
  CString str, str2;
  mWinApp->AppendToLog("Focus calibrations:\r\nCamera        Mag  Index  Direction");
  for (int actCam = 0; actCam < numCam; actCam++) {
    iCam = active[actCam];
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      for (ind = 0; ind < (int)mFocTab.GetSize(); ind++) {
        if (mFocTab[ind].camera == iCam && mFocTab[ind].magInd == iMag) {
          str.Format("%s   %d   %d   %d     %s", (LPCTSTR)camP[iCam].name,
            MagForCamera(iCam, iMag), iMag, mFocTab[ind].direction, 
            mFocTab[ind].probeMode ? "" : "nanoprobe");
          if (!mScope->GetHasNoAlpha() || mFocTab[ind].alpha >= 0) {
            str2.Format("  alpha %d", mFocTab[ind].alpha + 1);
            str += str2;
          }
          mWinApp->AppendToLog(str);
        }
      }
    }
  }
  if (mWinApp->ScopeHasSTEM()) {
    str = "";
    if (mSFnormalizedSlope[0])
      str = "\r\nSTEM autofocus is calibrated";
    if (FEIscope) {
      str += " in nanoprobe";
      if (mSFnormalizedSlope[1])
        str += "\r\nSTEM autofocus is calibrated in microprobe";
    }
    if (!str.IsEmpty())
      mWinApp->AppendToLog(str);
    PrintfToLog("There are %d tables of STEM focus versus Z", focusZtables->GetSize());
    if (focusZtables->GetSize()) {
      mWinApp->AppendToLog("Spot  Probe   middle slope");
      for (ind = 0; ind < (int)focusZtables->GetSize(); ind++) {
        sfzTable = focusZtables->GetAt(ind);
        iCam = B3DMAX(0, sfzTable.numPoints / 2 - 2);
        iMag = B3DMIN(sfzTable.numPoints - 1, sfzTable.numPoints / 2 + 2);
        PrintfToLog("%2d       %s      %.3f", sfzTable.spotSize, 
          sfzTable.probeMode ? "micro" : "nano", 
          (sfzTable.defocus[iMag] - sfzTable.defocus[iCam]) / 
          B3DMAX(0.01, sfzTable.stageZ[iMag] - sfzTable.stageZ[iCam]));
      }
    }
  }

  if (ctfAstigCals->GetSize()) {
    PrintfToLog("\r\nCTF-fitting based astigmatism calibrations:\r\n"
      "Mag  Index  Stigmator delta");
    for (ind = 0; ind < ctfAstigCals->GetSize(); ind++) {
      ctfCal = ctfAstigCals->GetAt(ind);
       PrintfToLog("%d   %d   %.3f", magTab[ctfCal.magInd].mag, ctfCal.magInd, 
        ctfCal.amplitude);
    }
  }

  if (astigCal->GetSize()) {
    PrintfToLog("\r\nBTID astigmatism calibrations:\r\nMag  Index  Tilt  Defocus  %s", 
      B3DCHOICE(JEOLscope && !mScope->GetHasNoAlpha(), "Alpha", ""));
    for (ind = 0; ind < astigCal->GetSize(); ind++) {
      astig = astigCal->GetAt(ind);
      str.Format("%d   %d   %.2f    %.1f", magTab[astig.magInd].mag, astig.magInd, 
        astig.beamTilt, astig.defocus);
      if (JEOLscope  && !mScope->GetHasNoAlpha()) {
        str2.Format("    %d", astig.alpha + 1);
        str += str2;
      }
      if (!astig.probeMode)
        str += "   nanoprobe";
      mWinApp->AppendToLog(str);
    }
  }

  if (comaCal->GetSize()) {
    PrintfToLog("\r\nOld (BTID) coma calibrations:\r\nMag  Index  Tilt  Defocus  %s", 
      B3DCHOICE(JEOLscope && !mScope->GetHasNoAlpha(), "Alpha", ""));
    for (ind = 0; ind < comaCal->GetSize(); ind++) {
      coma = comaCal->GetAt(ind);
      str.Format("%6d   %d   %.2f    %.1f", magTab[coma.magInd].mag, coma.magInd, 
        coma.beamTilt, coma.defocus);
      if (JEOLscope  && !mScope->GetHasNoAlpha()) {
        str2.Format("    %d", coma.alpha + 1);
        str += str2;
      }
      if (!coma.probeMode)
        str += "   nanoprobe";
      mWinApp->AppendToLog(str);
    }
  }

  mShiftManager->ListBeamShiftCals();
}

void CFocusManager::OnNormalizeViaView()
{
  mNormalizeViaView = !mNormalizeViaView;
}

void CFocusManager::OnUpdateNormalizeViaView(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(mNormalizeViaView ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode());
}

//////////////////////////////////////////////////////////////////
// FOCUS CALIBRATION AND AUTOFOCUS
  
// Entry for starting a focus calibration
void CFocusManager::CalFocusStart()
{
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  mFocusMag = mScope->GetMagIndex();
  mFocusProbe = mScope->ReadProbeMode();
  mFocusAlpha = mScope->GetAlpha();
  CString str;
  if (mWinApp->GetSTEMMode()) {
    CalSTEMfocus();
    return;
  }
  if (IS_FALCON2_OR_3(camParam) && mCalOffset + mCalRange / 2. > mFalconCalLimit) {
    str.Format("With the current defocus range and offset, the calibration\n"
      "may run into the Falcon Dose Protector unless you are currently\n"
      "at ~%.0f microns relative to eucentric focus.  See the help for\n"
      "the Calibration - Focus & Tuning - Set Focus Range command.\n\n"
      "Do you want to proceed?", mFalconCalLimit - (mCalOffset + mCalRange / 2.));
    if (AfxMessageBox(str, MB_QUESTION) == IDNO)
      return;
  }

  if (AfxMessageBox(
    "Focus detection takes many pictures with the " +  mModeNames[1] + " parameter set.\r"
    "Before proceeding, make sure that this parameter set takes\r"
    "fairly good pictures and that you are already well focused.\r\r"
    "Do you want to proceed?", MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;
  mFCindex = 0;
  mCalDelta = mCalRange / (mNumCalLevels - 1);
  mCalDefocus = mCalOffset - mCalRange / 2.;
  mScope->IncDefocus(mCalDefocus);
  mScope->NormalizeProjector();
  mWinApp->SetStatusText(MEDIUM_PANE, "CALIBRATING AUTOFOCUS");
  DetectFocus(FOCUS_CALIBRATE);
}

// Routine to process an image shift when calibrating focus
void CFocusManager::CalFocusData(float inX, float inY)
{
  float slopeX, slopeY, intcpX, intcpY, oldSlopeX, oldSlopeY;
  int i, iCen, iFitStr, focInd, numBad;
  double cenAng, oneAng;
  float fitSlope, fitIntcp;
  CString report;
  CString source = "measured directly";
  FocusTable oldCal;
  int camera = mWinApp->GetCurrentCamera();

  int wasCalibrated = GetFocusCal(mFocusMag, camera, mFocusProbe, mFocusAlpha, oldCal);
  if (wasCalibrated) {
    oldSlopeX = oldCal.slopeX;
    oldSlopeY = oldCal.slopeY;
    if (wasCalibrated == 2)
      source = "derived from calibrated image shifts";
    else if (wasCalibrated == 3)
      source = "derived from uncalibrated image shifts";
  }

  mNewCal.defocus[mFCindex] = (float)mCalDefocus;
  mNewCal.shiftX[mFCindex] = (float)(inX / mBeamTilt);
  mNewCal.shiftY[mFCindex] = (float)(inY / mBeamTilt);
  double diffX = inX - mBeamTilt * mNewCal.shiftX[B3DMAX(mFCindex - 1, 0)];
  double diffY = inY - mBeamTilt * mNewCal.shiftY[B3DMAX(mFCindex - 1, 0)];
  report.Format("%7.2f %7.2f %7.2f %11.2f %6.2f", mCalDefocus, inX, inY, diffX, diffY);

  mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
  mFCindex++;
  if (mFCindex < mNumCalLevels) {
    mCalDefocus += mCalDelta;
    mScope->IncDefocus(mCalDelta);
    DetectFocus(FOCUS_CALIBRATE);
    return;
  }

  mFCindex = -1;
  mScope->IncDefocus(-mCalDefocus);
  mCalDefocus = 0.;

  iCen = mNumCalLevels / 2;
  numBad = 0;
  cenAng = atan2((double)(mNewCal.shiftY[iCen + 1] - mNewCal.shiftY[iCen]), 
    (double)(mNewCal.shiftX[iCen + 1] - mNewCal.shiftX[iCen])) / DTOR;
  for (i = 0; i < mNumCalLevels - 1; i++) {
    oneAng = atan2((double)(mNewCal.shiftY[i + 1] - mNewCal.shiftY[i]), 
      (double)(mNewCal.shiftX[i + 1] - mNewCal.shiftX[i])) / DTOR;
    oneAng = UtilGoodAngle(oneAng - cenAng);
    if (fabs(oneAng) > 40.) {
      oneAng = atan2((double)(mNewCal.shiftY[i + 1] - mNewCal.shiftY[i]), 
        (double)(mNewCal.shiftX[i + 1] - mNewCal.shiftX[i])) / DTOR;
      report.Format("Curve slope is %.0f deg at point %d vs. %.0f deg at center",
        oneAng, i + 1, cenAng);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      numBad++;
    }
  }
  if (numBad) {
    report.Format("The slope of the curve is very different at %d points\n"
      "from the slope near zero defocus.\n\nYou should check conditions and redo"
      " this calibration.", numBad);
    AfxMessageBox(report, MB_EXCLAME);
    FocusTasksFinished();
    return;
  }

  StatLSFit(mNewCal.defocus, mNewCal.shiftX, mNumCalLevels, slopeX, intcpX);
  StatLSFit(mNewCal.defocus, mNewCal.shiftY, mNumCalLevels, slopeY, intcpY);
  mNewCal.slopeX = slopeX;
  mNewCal.slopeY = slopeY;
  mNewCal.beamTilt = mBeamTilt;
  mNewCal.numPoints = mNumCalLevels;
  mNewCal.calibrated = 1;
  mNewCal.direction = mTiltDirection;
  mNewCal.magInd = mFocusMag;
  camera = mWinApp->GetCurrentCamera();
  mNewCal.camera = camera;
  mNewCal.probeMode = mFocusProbe;
  mNewCal.alpha = mFocusAlpha;

  // copy the structure or append if this is a new calibration
  // First look for matching alpha then for one with no alpha defined, to replace
  if ((focInd = LookupFocusCal(mFocusMag, camera, mTiltDirection, mFocusProbe, 
    mFocusAlpha, false)) >= 0) {
      mFocTab[focInd] = mNewCal;
  } else if (mFocusAlpha >= 0 && (focInd = LookupFocusCal(mFocusMag, camera, 
    mTiltDirection, mFocusAlpha, mFocusProbe, true)) >= 0) {
      mFocTab[focInd] = mNewCal;
  } else {
    mFocTab.Add(mNewCal);
    focInd = (int)mFocTab.GetSize() - 1;
  }

  // If smoothing, do fits on mNewCal data, replace in focTab
  if (mSmoothFit > 2 && mSmoothFit < mNumCalLevels - 1) {
    for (iCen = 0; iCen < mNumCalLevels; iCen++) {
      iFitStr = iCen - mSmoothFit / 2;
      if (iFitStr < 0)
        iFitStr = 0;
      if (iFitStr + mSmoothFit > mNumCalLevels)
        iFitStr = mNumCalLevels - mSmoothFit;
      StatLSFit(&mNewCal.defocus[iFitStr], &mNewCal.shiftX[iFitStr], mSmoothFit,
        fitSlope, fitIntcp);
      mFocTab[focInd].shiftX[iCen] = mNewCal.defocus[iCen] * fitSlope + fitIntcp;
      StatLSFit(&mNewCal.defocus[iFitStr], &mNewCal.shiftY[iFitStr], mSmoothFit,
        fitSlope, fitIntcp);
      mFocTab[focInd].shiftY[iCen] = mNewCal.defocus[iCen] * fitSlope + fitIntcp;
    }
  }

  float originDist;

  CString axis = "X";
  if (mTiltDirection == 2)
    axis = "Y";
  else if (mTiltDirection)
    axis = "X and Y";

  // Get the relative defocus closest to origin along the current
  // and distance from origin at that point; adjust the relative
  // defocus values to make that give zero
  float zeroDefocus = DefocusFromCal(&mFocTab[focInd], 0., 0., originDist);
  for (i = 0; i < mNumCalLevels; i++)
    mFocTab[focInd].defocus[i] -= zeroDefocus;

  float newZero = DefocusFromCal(&mFocTab[focInd], 0., 0., originDist);

  report.Format("%dx: Slope & intercept in pixels/um for X: %.2f %.2f, Y: %.2f, %.2f\r\n"
    "%.2f X and %.2f Y pixels/um/mrad for %.2f mrad of %s beam tilt\r\n"
    "current defocus computed to be %.2f micron, line is %.2f pixels from origin",
    MagOrEFTEMmag(mCamParams[camera].GIF, mFocusMag),
    slopeX * mBeamTilt, intcpX * mBeamTilt, slopeY * mBeamTilt, intcpY * mBeamTilt,
    slopeX, slopeY, mBeamTilt, (LPCTSTR)axis, -zeroDefocus, originDist * mBeamTilt);
  mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);

  if (wasCalibrated) {
    double lenSqr = slopeX * slopeX + slopeY * slopeY;
    double delLength = sqrt(lenSqr / (oldSlopeX * oldSlopeX + oldSlopeY * oldSlopeY));
    double delDefocus = (lenSqr / (slopeX * oldSlopeX + slopeY * oldSlopeY) - 1.) 
      * 100.;
    double vectorRot = (atan2(slopeY, slopeX) - atan2(oldSlopeY, oldSlopeX)) / DTOR;
    if (vectorRot <= - 180.)
      vectorRot += 360.;
    else if (vectorRot > 180.)
      vectorRot -= 360;
    report.Format("The existing focus calibration vector, %s,\r\n"
      " changed in length by %.2f%% and rotated by %.1f degrees.\r\n"
      " Measured defocus would change by %.2f%%",
      source, (delLength - 1.) * 100., vectorRot, delDefocus);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN);

    float delDelta = (mNewCal.defocus[1] - mNewCal.defocus[0]) -
      (mNewCal.defocus[1] - oldCal.defocus[0]);

    // Ask admin whether to modify old calibrations, provided this is the first
    // calibration and the number of points and interval match the old one
    if (mWinApp->GetAdministrator() && !oldCal.calibrated && 
      oldCal.numPoints == mNewCal.numPoints && 
      delDelta > -0.01 && delDelta < 0.01) {
      if (AfxMessageBox("Do you want to modify all old focus calibration\r\n"
        "tables for this camera and tilt direction by these changes?", 
        MB_YESNO | MB_ICONQUESTION) == IDYES) {
        ScaleMat delMat;
        delMat.xpx = (float)(delLength * cos(DTOR * vectorRot));
        delMat.ypy = delMat.xpx;
        delMat.ypx = (float)(delLength * sin(DTOR * vectorRot));
        delMat.xpy = -delMat.ypx;
        mWinApp->mFocusManager->ModifyAllCalibrations(delMat, camera, mTiltDirection);
      }
    }
  }
  FocusTasksFinished();
  mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_FOCUS);
}


BOOL CFocusManager::FocusReady(int magInd)
{
  FocusTable focTmp;
  if (!mScope)
    return false;
  if (mWinApp->GetSTEMMode())
    return (mSFnormalizedSlope[mScope->GetProbeMode()] != 0.);
  if (magInd < 0)
    magInd = mScope->FastMagIndex();
  if (magInd < mScope->GetLowestMModeMagInd())
    return magInd > 0 && mScope->GetStandardLMFocus(magInd) > -900.;
  return (GetFocusCal(magInd, mWinApp->GetCurrentCamera(), 
    mScope->GetProbeMode(), mScope->FastAlpha(), focTmp) != 0);
}

// Call to start autofocus or measurement of defocus
void CFocusManager::AutoFocusStart(int inChange, int useViewInLD, int iterNum) 
{
  float slope, shiftX, shiftY;
  CString mess;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  mFocusSetNum = FOCUS_CONSET;
  mAutofocusIterNum = iterNum;
  if (useViewInLD && mWinApp->LowDoseMode() && !mWinApp->GetSTEMMode()) {
    mFocusSetNum = useViewInLD > 0 ? VIEW_CONSET : RECORD_CONSET;
    mScope->GotoLowDoseArea(mFocusSetNum);
    if (mUseOppositeLDArea) {
      SEMMessageBox("You can not use opposite low dose areas with the View area",
        MB_EXCLAME);
      return;
    }
  }
  ControlSet *conSet = &mConSets[mFocusSetNum];
  mDoChangeFocus = inChange;
  SEMTrace('M', "Autofocus Start");
  if (mWinApp->LowDoseMode() && ldParm[mFocusSetNum].magIndex)
    mFocusMag = ldParm[mFocusSetNum].magIndex;
  else
    mFocusMag = mScope->GetMagIndex();

  // Use up any limits that are set.  First apply standard limits available from menu,
  // then override that with a limit from a macro
  if (iterNum == 1) {
    mUseMinAbsFocus = mUseMaxAbsFocus = 0.;
    if (mUseEucenAbsLimits) {
      mUseMinAbsFocus = mEucenMinAbsFocus;
      mUseMaxAbsFocus = mEucenMaxAbsFocus;
    }
    if (mNextMinAbsFocus)
      mUseMinAbsFocus = mNextMinAbsFocus;
    if (mNextMaxAbsFocus)
      mUseMaxAbsFocus = mNextMaxAbsFocus;
    mUseMinDeltaFocus = mNextMinDeltaFocus;
    mUseMaxDeltaFocus = mNextMaxDeltaFocus;
    mNextMinDeltaFocus = 0.;
    mNextMaxDeltaFocus = 0.;
    mNextMinAbsFocus = 0.;
    mNextMaxAbsFocus = 0.;
    mNumNearZeroCorr = 0;
    mCumulFocusChange = 0.;
    mNumFullChangeIters = 0;
  }

  if (!mWinApp->GetSTEMMode()) { 

    // Regular
    if (inChange > 0 && mFocusMag < mScope->GetLowestMModeMagInd()) {
      double focus = mScope->GetStandardLMFocus(mFocusMag);
      if (focus > -900.) {
        mScope->SetFocus(focus);
        mess.Format("Setting to standard focus (%f) since this is low mag (%dx)",
          focus, MagForCamera(mWinApp->GetCurrentCamera(), mFocusMag));
        mWinApp->AppendToLog(mess, LOG_SWALLOW_IF_CLOSED);
      } else
        mWinApp->AppendToLog("There is no autofocus in low mag, and no standard focus"
        " defined; nothing was done", LOG_SWALLOW_IF_CLOSED);
      return;
    }
    if (mUseOppositeLDArea && mCamera->OppositeLDAreaNextShot()) {
      SEMMessageBox("You can not use opposite low dose areas when Balance Shifts is on",
        MB_EXCLAME);
      mUseOppositeLDArea = false;
      return;
    }
    if (iterNum == 1) {
      mLastWasOpposite = false;
    }
  } else {

    // STEM
    slope = mSFnormalizedSlope[mScope->GetProbeMode()];
    if (!slope)
      return;
    if (mShiftManager->ShiftAdjustmentForSet(FOCUS_CONSET, mFocusMag, shiftX, shiftY))
      mCamera->AdjustForShift(shiftX, shiftY);
    mModeWasContinuous = mConSets[FOCUS_CONSET].mode == CONTINUOUS;
    mConSets[FOCUS_CONSET].mode = SINGLE_FRAME;
    if (mCamera->CameraBusy()) 
      mCamera->StopCapture(0);
    mSFbaseFocus = mScope->GetDefocus();

    if (mConSets[FOCUS_CONSET].boostMag && !mWinApp->LowDoseMode())
      mFocusMag = mShiftManager->FindBoostedMagIndex(mFocusMag, 
        mConSets[FOCUS_CONSET].boostMag);
    slope /= conSet->binning * mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(),
      mFocusMag);
    mCalDelta = sqrt(mSFmidFocusStep * mSFtargetSize / slope);
    B3DCLAMP(mCalDelta, mSFminFocusStep, mSFmaxFocusStep);
    mCalDefocus = 0.;
    mNumRotavs = 0;
    if (mSFshowBestAtEnd)
      mSFbestImBuf = new EMimageBuffer;
    mSFtask = STEM_FOCUS_COARSE1;
    mCamera->SetRetainMagAndShift(FOCUS_CONSET);
    SEMTrace('f', "STEM focus starting %.3f  step %.2f with shift %.2f %.2f", 
      mSFbaseFocus, mCalDelta, shiftX, shiftY);
  }
  if (mDoChangeFocus > 0)
    mWinApp->SetStatusText(MEDIUM_PANE, "AUTOFOCUSING");
  else
    mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING DEFOCUS");
  if (mWinApp->GetSTEMMode()) {
    mWinApp->UpdateBufferWindows();
    StartSTEMfocusShot();
    return;
  }

  DetectFocus(FOCUS_AUTOFOCUS, useViewInLD);
}

// Routine that receives the shift from focus detection sequence
void CFocusManager::AutoFocusData(float inX, float inY)
{
  float changeLimit = 2.f * B3DMAX(8.f, mRefocusThreshold);
  double overShoot = 0.0;   // Microns to overshoot and return on negative change
  float closerCrit = 0.33f;
  int fullChangeIterLimit = 8;
  float nearZeroAbsCrit = 0.15f, nearZeroRelCrit = 0.1f;
  CString report, driftText, addon, changeText;
  float rawDefocus, lastDefocus = mCurrentDefocus;
  bool refocus, testAbs, testDelta;
  int abort = 0;
  double lastFullDiff, lastDiff, diff, fullDiff;
  if (CurrentDefocusFromShift(inX, inY, mCurrentDefocus, rawDefocus))
    return;

  report.Format("Measured defocus = %.2f microns", mCurrentDefocus);
  if (!mVerbose && mNumShots == 3)
    driftText.Format("    drift = %.2f nm/sec", mLastNmPerSec);
  if (mDoChangeFocus <= 0)
    mWinApp->AppendToLog(report + driftText, LOG_SWALLOW_IF_CLOSED);
  if (mDoChangeFocus > 0) {
    fullDiff = diff = mTargetDefocus - mCurrentDefocus;

    // Limit the change
    B3DCLAMP(diff, -changeLimit, changeLimit);
    changeText.Format("   changed by %.2f", diff);
    if (fabs(diff - fullDiff) < 0.1) {
      mNumFullChangeIters++;
      changeText += " to target";
    }
    mCumulFocusChange += (float)diff;
    if (fabs(rawDefocus) < B3DMAX(0.15, 0.1 * diff))
      mNumNearZeroCorr++;
      
    // Approach from below: if it is a positive change, do it; 
    // Otherwise overshoot then go back (THIS DIDN'T HELP)
    if (diff > 0. || overShoot == 0.)
      mScope->IncDefocus(diff);
    else {
      mScope->IncDefocus(diff - overShoot);
      mScope->IncDefocus(overShoot);
    }
    refocus = fabs(diff) > mRefocusThreshold && mRefocusThreshold >= 0.099;
    testDelta = mUseMinDeltaFocus != 0. || mUseMaxDeltaFocus != 0.;
    testAbs = mUseMinAbsFocus != 0. || mUseMaxAbsFocus != 0.;
    if (refocus || testAbs || testDelta) {
      if (refocus && mAutofocusIterNum > 1 && fabs(diff) > mAbortThreshold && 
        mAbortThreshold > 0.) {
          lastFullDiff = lastDiff = mTargetDefocus - lastDefocus;
          B3DCLAMP(lastDiff, -changeLimit, changeLimit);
          bool opposite = (lastDiff < 0 && diff > 0) || (lastDiff > 0 && diff < 0);

          // Abort if we are farther from target than before, on either side;
          if (fabs(fullDiff) > fabs(lastFullDiff)) {
            abort = FOCUS_ABORT_INCONSISTENT;
          } else if (opposite && !mLastWasOpposite) {

            // On second try, if it is on opposite side, try the middle point.  Undo
            // the move that was just done and an interpolated fraction of the last one
            mScope->IncDefocus(-diff + lastDiff * fullDiff / (lastFullDiff - fullDiff));
          } else if (opposite || 
            fabs(lastDefocus - mCurrentDefocus) < closerCrit * fabs(lastDiff)) {

              // Or abort if it is on opposite side after second try or if the measured
              // change was less than a fraction of what it should have been
              abort = FOCUS_ABORT_INCONSISTENT;
          }
          mLastWasOpposite = opposite;
      }
      addon = "of inconsistent behavior";
      if (!abort && testDelta) {
        addon = "focus change would exceed limits";
        diff = mScope->GetDefocus() - mOriginalDefocus;
        abort = B3DCHOICE((mUseMinDeltaFocus != 0. && diff < mUseMinDeltaFocus) ||
          (mUseMinDeltaFocus != 0 && diff > mUseMaxDeltaFocus), FOCUS_ABORT_DELTA_LIMIT, 
          0);
      }
      if (!abort && testAbs) {
        addon = "absolute focus would exceed limits";
        diff = mScope->GetFocus();
        abort = B3DCHOICE((mUseMinAbsFocus != 0. && diff < mUseMinAbsFocus) ||
          (mUseMinAbsFocus != 0. && diff > mUseMaxAbsFocus), FOCUS_ABORT_ABS_LIMIT, 0);
      }
      if (!abort && mAutofocusIterNum > 2 && fabs(mCumulFocusChange) > mAbortThreshold && 
        mAbortThreshold > 0. && mNumNearZeroCorr >= ceil(0.75 * mAutofocusIterNum)) {
          abort = FOCUS_ABORT_NEAR_ZERO;
          addon = "correlation was near zero too many times";
      }
      if (!abort && mNumFullChangeIters >= fullChangeIterLimit) {
        abort = FOCUS_ABORT_TOO_MANY_ITERS;
        addon = "too may iterations have been run";
      }
      mWinApp->AppendToLog(report + (abort ? "" : changeText) + driftText, 
        LOG_SWALLOW_IF_CLOSED);
      if (abort) {
        mLastAborted = abort;
        mScope->SetDefocus(mOriginalDefocus);
        report.Format("WARNING: Autofocus aborted after %d iterations and focus returned "
          "to starting value (%.2f)\r\n  because %s. Check focus "
          "calibration\r\n  and/or scope alignment if images and specimen look OK",
          mAutofocusIterNum, mOriginalDefocus, (LPCTSTR)addon);
        mWinApp->AppendToLog(report);
      } else if (refocus) {
        AutoFocusStart(FOCUS_AUTOFOCUS, mUseViewInLD, mAutofocusIterNum + 1);
        return;
      }
    } else {
      mWinApp->AppendToLog(report + changeText + driftText, LOG_SWALLOW_IF_CLOSED);
    }
  } else if (!mDoChangeFocus) {
    report.Format("The current defocus is computed to be %6.2f microns", 
      mCurrentDefocus);
    mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
  } 
  FocusTasksFinished();
  SEMTrace('M', "Autofocus Done");
}

// Determine the current defocus from the given shifts: get the calibration at the mag
// of image or current mag, lookup defocus, adjust for offset and image not centered
// Also return a raw defocus based just on the shift of the images
int CFocusManager::CurrentDefocusFromShift(float inX, float inY, float &defocus,
  float &rawDefocus)
{
  int shiftX, shiftY;
  float angle, specY, minDist;
  float focusFac = mShiftManager->GetDefocusZFactor() *
    (mShiftManager->GetStageInvertsZAxis() ? -1 : 1);
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  ScaleMat aInv;
  ControlSet *conSet = &mConSets[mFocusSetNum];
  mCurrentCamera = mWinApp->GetCurrentCamera();
  int magInd = mImBufs->mMagInd;
  FocusTable useCal;
  if (!magInd)
    magInd = mScope->FastMagIndex();
  if (!GetFocusCal(magInd, mCurrentCamera, mFocusProbe, mFocusAlpha, useCal))
    return 1;
  aInv = mShiftManager->CameraToSpecimen(magInd);
  defocus = DefocusFromCal(&useCal, inX / (float)mBeamTilt, inY / (float)mBeamTilt, 
    minDist);
  rawDefocus = defocus;

  // Subtract the defocus offset unless calibrating; also subtract the view offset
  // unless measuring with script argument 1 instead of 2
  if (mFCindex < 0)
    defocus -= (float)mDefocusOffset;
  if (mFocusSetNum == VIEW_CONSET && !(mDoChangeFocus == -1 && mUseViewInLD == 1))
    defocus -= mScope->GetLDViewDefocus(VIEW_CONSET);

  // Adjust for image not centered - get specimen Y of center of field
  // The sign of change in measured defocus is opposite to sign when moving
  // focus center from such a center
  shiftX = (conSet->right + conSet->left) / 2 - mCamParams[mCurrentCamera].sizeX / 2;
  shiftY = (conSet->bottom + conSet->top) / 2 - mCamParams[mCurrentCamera].sizeY / 2;
  specY = aInv.ypx * shiftX - aInv.ypy * shiftY;

  // And if the low dose axis is rotated, add Y component of the rotated axis separation
  if (mWinApp->LowDoseMode() && mWinApp->mLowDoseDlg.m_bRotateAxis && 
    mWinApp->mLowDoseDlg.m_iAxisAngle && mFocusSetNum == FOCUS_CONSET)
    specY += (float)((ldParm[1].axisPosition - ldParm[3].axisPosition) * 
      sin(DTOR * mWinApp->mLowDoseDlg.m_iAxisAngle));
  if (!mImBufs->GetTiltAngle(angle))
    angle = (float)mScope->GetTiltAngle();
  defocus -= (float)(specY * tan(DTOR * angle) * focusFac);
  return 0;
}


// Starting point for measuring defocus with 2 or 3 pictures
void CFocusManager::DetectFocus(int inWhere, int useViewInLD) 
{
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  bool setLowDose = false;
  double focus;
  CString str;
  mFocusSetNum = FOCUS_CONSET;
  if (useViewInLD && mWinApp->LowDoseMode() && !mWinApp->GetSTEMMode())
    mFocusSetNum = useViewInLD > 0 ? VIEW_CONSET : RECORD_CONSET;
  ControlSet  *set = mConSets + mFocusSetNum;
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();

  mModeWasContinuous = set->mode == CONTINUOUS;
  set->mode = SINGLE_FRAME;
  mFocusWhere = inWhere;
  mUseViewInLD = useViewInLD;
  mUsingExisting = inWhere == FOCUS_EXISTING || inWhere == FOCUS_SHOW_CORR || 
    inWhere == FOCUS_SHOW_STRETCH;
  if (mUsingExisting && mImBufs->mMagInd > 0)
    mFocusMag = mImBufs->mMagInd;
  else if (mWinApp->LowDoseMode())
    mFocusMag = ldParm[mFocusSetNum].magIndex;
  else
    mFocusMag = mScope->FastMagIndex();

  if (mWinApp->LowDoseMode()) {
    mFocusProbe = ldParm[mFocusSetNum].probeMode;
    mFocusAlpha = (int)ldParm[mFocusSetNum].beamAlpha;
  } else {
    mFocusProbe = mScope->ReadProbeMode();
    mFocusAlpha = mScope->GetAlpha();
  }
  if (mCamera->CameraBusy()) 
    mCamera->StopCapture(0);

  // Enforce drift correction any calibration and for coma-free alignment
  mNumShots = (mTripleMode || (inWhere == FOCUS_CALIBRATE || inWhere == FOCUS_CAL_ASTIG ||
    inWhere == FOCUS_COMA_FREE)) ? 3 : 2;
  mFocusIndex = 0;
  mLastFailed = true;
  mLastAborted = false;
  mCamera->SetObeyTiltDelay(true);
  if (mUseOppositeLDArea)
    mCamera->OppositeLDAreaNextShot();

  // Normalize through view in Low Dose if selected and it is not already done
  if (mNormalizeViaView && mFocusSetNum == FOCUS_CONSET && mWinApp->LowDoseMode() && 
    !mUsingExisting && (!mScope->GetFocusCameFromView() || 
    !mWinApp->mLowDoseDlg.SameAsFocusArea(mScope->GetLowDoseArea()))) {
      mScope->GotoLowDoseArea(VIEW_CONSET);
      mScope->ScopeUpdate(GetTickCount());
      SEMTrace('1', "Went to view, mag is: %d", mScope->GetMagIndex());
      if (mViewNormDelay > 0)
        Sleep(mViewNormDelay);
      SEMTrace('1', "Delay over, mag is: %d", mScope->GetMagIndex());
  }

  // Go to focus area now if in low dose and adjusting beam tilt too; also if alpha could
  // change and that could change beam tilt; also if not in area!  Gave up on this on
  // 5/25/17 because original focus has to be recorded after getting out of View
  if (!mUsingExisting && mWinApp->LowDoseMode() && mAutofocusIterNum == 1 &&
    (mScope->GetLDBeamTiltShifts() || !mScope->GetHasNoAlpha() || 
    mScope->GetProbeMode() != mFocusProbe || 
    !mWinApp->mLowDoseDlg.SameAsFocusArea(mScope->GetLowDoseArea()))) {
      mScope->GotoLowDoseArea(mFocusSetNum);
      setLowDose = !mUseOppositeLDArea;
  }
  if (mAutofocusIterNum == 1)
    mOriginalDefocus = mScope->GetDefocus();
  
  // Apply offset if nonzero and not calibrating: then test against absolute limits if
  // flag set for that
  mAppliedOffset = 0.;
  if ((inWhere == FOCUS_AUTOFOCUS || inWhere == FOCUS_REPORT) && mDefocusOffset) {
    mAppliedOffset = mDefocusOffset;
    mScope->IncDefocus(mAppliedOffset);
    if (mUseEucenAbsLimits && mTestOffsetEucenAbs) {
      focus = mScope->GetFocus();
      if ((mUseMinAbsFocus != 0. && focus < mUseMinAbsFocus) ||
        (mUseMaxAbsFocus != 0. && focus > mUseMaxAbsFocus)) {
          mScope->SetDefocus(mOriginalDefocus);
          str.Format("WARNING: Autofocus aborted before iteration %d and focus returned "
            "to starting value (%.2f) because\r\n  absolute focus would exceed limits "
            "with the %.1f micron offset applied for measuring defocus",
          mAutofocusIterNum, mOriginalDefocus, mAppliedOffset);
          mWinApp->AppendToLog(str);
          mAppliedOffset = 0.;
          mLastAborted = FOCUS_ABORT_ABS_LIMIT;
          StopFocusing();
          return;
      }
    }
  }

  mWinApp->UpdateBufferWindows();
  for (int k = 0; k < 5; k++)
    mFocusBuf[k] = NULL;
  mScope->GetBeamTilt(mBaseTiltX, mBaseTiltY);
  //PrintfToLog("Set beam tilt to %.2f, %.2f", mBaseTiltX + mNextXtiltOffset - mFracTiltX * mBeamTilt, 
  //  mBaseTiltY + mNextYtiltOffset - mFracTiltY * mBeamTilt);
  mScope->SetBeamTilt(mBaseTiltX + mNextXtiltOffset - mFracTiltX * mBeamTilt, 
    mBaseTiltY + mNextYtiltOffset - mFracTiltY * mBeamTilt);
  if (mPostTiltDelay > 0)
    Sleep(mPostTiltDelay);

  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TaskFocusDone, 
    TaskFocusError, 0, 0);
  if (!mUsingExisting) {
    if (setLowDose)
      mCamera->SetLDwasSetToArea(mFocusSetNum);
    mCamera->InitiateCapture(mFocusSetNum);
  }
}

// On idle tasks when done or error
void CFocusManager::TaskFocusDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (winApp->mFocusManager->DoingFocus())
    winApp->mFocusManager->FocusDone();
  else {
    winApp->mFocusManager->FocusTasksFinished();
  }

}

void CFocusManager::TaskFocusError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mFocusManager->StopFocusing();
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting focus detection pictures"), MB_EXCLAME);
}

// Process image when a focus image is done
void CFocusManager::FocusDone()
{
  int nxframe, nyframe, nxuse, nyuse, nxpad, nypad, type;
  int  ix0, ix1, iy0, iy1, ind1, ind2;
  KImage *imA;
  void *data, *temp;
  float xPeak1[2], yPeak1[2], xPeak2[2], yPeak2[2], peak1[2], peak2[2];
  float xShift, yShift, xDrift, yDrift;
  CString report;
  int trim =4;   // 3/19/06: set to 4 from 0 for Ultracam with bad lines
  int pad, minBinning, needBin = 1;
  float radius2use = mRadius2;
  float sigma2use = mSigma2;
  int nxTaper, nyTaper;
  float delta;
  void *stretchData = NULL;
  BOOL doStretch;
  bool removeData = false;
  double imRot, CCC;
  float tilt, pixel, cosphi, sinphi, tanTilt, a11, a12, a21, a22;
  int binning = mConSets[mFocusSetNum].binning;
  int iCam = mWinApp->GetCurrentCamera();
  FocusTable focCal;
  int ifCalibrated = GetFocusCal(mFocusMag, iCam, mFocusProbe, mFocusAlpha, focCal);
  int bufnum = 0;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  int divForK2 = BinDivisorI(camParam);
  if (mFocusIndex < 0)
    return;
  if (mUsingExisting)
    bufnum = mNumShots - mFocusIndex - 1;
  if (mUsingExisting && mImBufs[bufnum].mBinning > 0)
    binning = mImBufs[bufnum].mBinning;

  // If a required level is registered, test the image scale against it
  if (mRequiredBWMean > 0. && 
    mWinApp->mProcessImage->ImageTooWeak(mImBufs, mRequiredBWMean)) {
    StopFocusing();
    return;
  }

  // Extract the new image into a tapered, padded array
  imA = mImBufs[bufnum].mImage;
  nxframe = imA->getWidth();
  nyframe = imA->getHeight();
  type = imA->getType();

  // See if binning is needed, or at least scaling of filter parameters
  minBinning = mDDDminBinning * BinDivisorI(camParam);
  if (mCamera->IsDirectDetector(camParam) && binning > 0 && binning < minBinning) {

    // Get needed achievable binning then scale the filter down the rest of the way
    needBin = minBinning / binning;
    radius2use = (float)((mRadius2 * needBin * binning) / minBinning);
    sigma2use = (float)((mSigma2 * needBin * binning) / minBinning);
    if (needBin > 1) {
      imA->Lock();
      data = imA->getData();
      NewArray(temp, short int, (nxframe * nyframe) / needBin);
      if (!temp) {
        SEMMessageBox(_T("Error getting buffer for binning - focus aborted"));
        StopFocusing();
        imA->UnLock();
        return;
      }

      // Do the binning and replace parameters
      XCorrBinByN(data, type, nxframe, nyframe, needBin, (short *)temp);
      if (type == kUBYTE || type == kRGB)
        type = kSHORT;
      nxframe /= needBin;
      nyframe /= needBin;
      data = temp;
      removeData = true;
      binning *= needBin;
      imA->UnLock();
    }
  }

  nxuse = nxframe - 2 * trim;
  nyuse = nyframe - 2 * trim;
  ix0 = trim;
  ix1 = nxframe - trim - 1;
  iy0 = trim;
  iy1 = nyframe - trim - 1;
  pad = (int)(mPadFrac * (nxuse > nyuse ? nxuse : nyuse));
  nxpad = XCorrNiceFrame(nxuse + pad, 2, 19);
  nypad = XCorrNiceFrame(nyuse + pad, 2, 19);
  nxTaper = (int)(mTaperFrac * nxuse);
  nyTaper = (int)(mTaperFrac * nyuse);

  // Do stretch of the positive tilted shot if tilt is > 5 degrees, and there
  // is a focus calibration available
  if (!mImBufs[bufnum].GetTiltAngle(tilt))
    tilt = (float)mScope->GetTiltAngle();
  doStretch = (mFocusIndex == 1) && (tilt > 5. || tilt < -5.) && ifCalibrated;
  NewArray(mFocusBuf[mFocusIndex], float, nypad * (nxpad + 2));
  if (mFocusIndex < 2)
    NewArray(mFocusBuf[mFocusIndex + 3], float, nypad * (nxpad + 2));
  if (doStretch) {
    if (type == kUBYTE) {
      NewArray(stretchData, unsigned char, nxframe * nyframe);
    } else {
      NewArray(stretchData, short int, nxframe * nyframe);
    }
  }

  if (mFocusBuf[mFocusIndex] == NULL  || (doStretch && stretchData == NULL) ||
    (mFocusIndex < 2 && mFocusBuf[mFocusIndex + 3] == NULL)) {
    SEMMessageBox(_T("Error getting image buffers - focus aborted"), MB_EXCLAME);
    if (stretchData)
      delete [] stretchData;
    if (removeData)
      delete [] data;
    StopFocusing();
    return;
  }

  if (needBin < 2) {
    imA->Lock();
    data = imA->getData();
  }

  // Beam tilt should stretch the image if tilt is high. 
  if (doStretch) {
    imRot = mShiftManager->GetImageRotation(iCam, mFocusMag);
    pixel = mShiftManager->GetPixelSize(iCam, mFocusMag);
    sinphi = (float)sin(DTOR * imRot);
    cosphi = (float)cos(DTOR * imRot);
    tanTilt = (float)(tan(DTOR * tilt) * (mShiftManager->GetStageInvertsZAxis() ? -1 :1));
    /* (mShiftManager->GetInvertStageXAxis() ? -1 : 1)*/;
    pixel *= (float)mBeamTilt;    // Slopes are per mRad of tilt
    a11 = 1.f - focCal.slopeX * sinphi * tanTilt * pixel;
    a12 = focCal.slopeX * cosphi * tanTilt * pixel;
    a21 = -focCal.slopeY * sinphi * tanTilt * pixel;
    a22 = 1.f + focCal.slopeY * cosphi * tanTilt * pixel;
    XCorrFastInterp(data, type, stretchData, nxframe, nyframe, nxframe, 
      nyframe, a11, a12, a21, a22, nxframe / 2.f, nyframe / 2.f, 0.f, 0.f);
    if (type == kUBYTE)
      type = kSHORT;
    if (removeData)
      delete [] data;
    data = (void *)stretchData;
    removeData = true;
  }

  // This could be done with the first image if binning, but that is not needed as long
  // as the binning of this image gets set correctly
  if (mFocusWhere == FOCUS_SHOW_STRETCH && doStretch) {
    CString strAmat;
    if (!removeData)
      imA->UnLock();
    if (bufnum + 2 <= mBufferManager->GetShiftsOnAcquire())
      mBufferManager->CopyImageBuffer(bufnum, bufnum + 2);
    mBufferManager->ReplaceImage((char *)data, type, nxframe, nyframe,
      bufnum, BUFFER_PROCESSED, mFocusSetNum);
    mImBufs[bufnum].mBinning = binning;
    mImBufs[bufnum].mEffectiveBin = (float)binning;
    mImBufs[bufnum].mDivideBinToShow = divForK2;
    removeData = false;
    imA = mImBufs[bufnum].mImage;
    imA->Lock();
    if (doStretch) {
      mWinApp->SetCurrentBuffer(bufnum);
      strAmat.Format("stretch matrix  %.5f  %.5f  %.5f  %.5f", a11, a12, a21, a22);
      mWinApp->AppendToLog(strAmat, LOG_OPEN_IF_CLOSED);
    }
 }

  XCorrTaperInPad(data, type, nxframe, ix0, ix1, iy0, iy1, 
          mFocusBuf[mFocusIndex],
          nxpad + 2, nxpad, nypad, nxTaper, nyTaper);
  if (removeData)
    delete [] data;
  else
    imA->UnLock();
  if (mFocusIndex < 2)
    memcpy(mFocusBuf[mFocusIndex + 3], mFocusBuf[mFocusIndex], 4 * nypad * (nxpad + 2));
  
  mFocusIndex++;
  if (mFocusIndex < mNumShots) {
    int sign = 3 - 2 * mFocusIndex;
    //PrintfToLog("Set beam tilt to %.2f, %.2f", mBaseTiltX + mNextXtiltOffset + mFracTiltX * sign * mBeamTilt,
    //  mBaseTiltY + mNextYtiltOffset + mFracTiltY * sign * mBeamTilt);
    mScope->SetBeamTilt(mBaseTiltX + mNextXtiltOffset + mFracTiltX * sign * mBeamTilt,
      mBaseTiltY + mNextYtiltOffset + mFracTiltY * sign * mBeamTilt);
    if (mPostTiltDelay > 0)
      Sleep(mPostTiltDelay);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TaskFocusDone, 
      TaskFocusError, 0, 0);
    if (!mUsingExisting && mUseOppositeLDArea)
      mCamera->OppositeLDAreaNextShot();
    if (!mUsingExisting) {
        if (!mUseOppositeLDArea)
          mCamera->SetLDwasSetToArea(mFocusSetNum);
        mCamera->InitiateCapture(mFocusSetNum);
    }
    return;
  }

  //  All three shots done, get the triple correlation  
  XCorrSetCTF(mSigma1, sigma2use, 0., radius2use, mCTFa, nxpad, nypad, &delta);
  
  if (mNumShots == 3) {
    XCorrTripleCorr(mFocusBuf[0], mFocusBuf[1], mFocusBuf[2], nxpad, nypad,
           delta, mCTFa);

    XCorrPeakFind(mFocusBuf[0], nxpad+2, nypad, xPeak1, yPeak1, peak1, 2);
    XCorrPeakFind(mFocusBuf[1], nxpad+2, nypad, xPeak2, yPeak2, peak2, 2);
    ind1 = CheckZeroPeak(xPeak1, yPeak1, peak1, binning);
    ind2 = CheckZeroPeak(xPeak2, yPeak2, peak2, -binning);
    
    // INVERT THOSE Y VALUES to make shifts properly rotatable
    xShift = binning * (xPeak1[ind1] - xPeak2[ind2]) / 2;
    yShift = binning * (yPeak2[ind2] - yPeak1[ind1]) / 2;
    xDrift = binning * (xPeak2[ind2] + xPeak1[ind1]) / 2;
    yDrift = -binning * (yPeak2[ind2] + yPeak1[ind1]) / 2;
  } else {
    XCorrCrossCorr(mFocusBuf[0], mFocusBuf[1], nxpad, nypad, delta, mCTFa);
    XCorrPeakFind(mFocusBuf[0], nxpad+2, nypad, xPeak1, yPeak1, peak1, 2);
    ind1 = CheckZeroPeak(xPeak1, yPeak1, peak1, binning);
    xShift =  binning * xPeak1[ind1];
    yShift = -binning * yPeak1[ind1];
  }

  // Display correlation in buffer A if requested
  if (mFocusWhere == FOCUS_SHOW_CORR) {
    float corMin, corMax;
    mWinApp->mProcessImage->CorrelationToBufferA(mFocusBuf[0], nxpad, nypad, needBin,
      corMin, corMax);
    mWinApp->SetCurrentBuffer(0);
  }

  // Compute unfiltered normalized CCC
  CCC = XCorrCCCoefficient(mFocusBuf[3], mFocusBuf[4], nxpad + 2, nxpad, nypad, 
    -xPeak1[ind1], -yPeak1[ind1], (nxpad + 1 - nxuse) / 2, (nypad + 1 - nyuse) /2, &ix0);

  for (int ibuf = 0; ibuf < 5; ibuf++) {
    if (mFocusBuf[ibuf])
      delete [] mFocusBuf[ibuf];
    mFocusBuf[ibuf] = NULL;
  }

  mLastFailed = false;
  mDriftStored = mNumShots == 3;
  mRequiredBWMean = -1.;
  mFocusIndex = -1;
  mScope->SetBeamTilt(mBaseTiltX, mBaseTiltY);
  if (mAppliedOffset)
    mScope->IncDefocus(-mAppliedOffset);
  mAppliedOffset = 0.;

  // Prepare report and store the drift values
  CString strBin = binning > divForK2 ? "unbinned" : "";
  report.Format("Tilt-induced shift is %6.2f, %6.2f %s pixels, unfiltered CCC is %.4f",
    -xShift / divForK2, -yShift / divForK2, strBin, CCC);
  if (mNumShots == 3) {
    CString strTemp;
    imRot = 1000. * mShiftManager->GetPixelSize(iCam, mFocusMag)
      / (mImBufs[0].mTimeStamp - mImBufs[2].mTimeStamp);
    mLastDriftX = -(float)(xDrift * imRot);
    mLastDriftY = -(float)(yDrift * imRot);
    mLastNmPerSec = (float)(imRot * sqrt(xDrift * xDrift + yDrift * yDrift));
    strTemp.Format("\r\nThe average drift between shots was %6.1f, %6.1f %s pixels, "
      "%.2f nm/sec", xDrift / divForK2, yDrift / divForK2, strBin, mLastNmPerSec);
    report += strTemp;
  }

  // Temporary report
//  if (mFocusWhere != FOCUS_REPORT)
//    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN);

  switch (mFocusWhere) {
    case FOCUS_CALIBRATE:
      CalFocusData(xShift, yShift);
      break;
  
    case FOCUS_AUTOFOCUS:
      if (mVerbose)
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      AutoFocusData(xShift, yShift);
      break;

    case FOCUS_CAL_ASTIG:
    case FOCUS_ASTIGMATISM:
    case FOCUS_COMA_FREE:
      mWinApp->mAutoTuning->TuningFocusData(xShift, yShift);
      break;

    case FOCUS_EXISTING:
    case FOCUS_REPORT:
    case FOCUS_SHOW_CORR:
    case FOCUS_SHOW_STRETCH:
      mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
      FocusTasksFinished();
      break;
  }
}

void CFocusManager::StopFocusing()
{
  mCamera->StopCapture(1);
  if (mNumRotavs >= 0) {
    StopSTEMfocus();
    return;
  }
  if (mFCindex >= 0)
    mScope->IncDefocus(-mCalDefocus);
  mFCindex = -1;
  mScope->SetBeamTilt(mBaseTiltX, mBaseTiltY);
  mFocusIndex = -1;
  if (mAppliedOffset)
    mScope->IncDefocus(-mAppliedOffset);
  mAppliedOffset = 0.;
  mRequiredBWMean = -1.;
  for (int i = 0; i < 5; i++)
    if (mFocusBuf[i] != NULL) {
      delete [] mFocusBuf[i];
      mFocusBuf[i] = NULL;
    }
  FocusTasksFinished();
}

// Collect all the routine actions here
void CFocusManager::FocusTasksFinished()
{
  mCamera->SetObeyTiltDelay(false);
  mUseOppositeLDArea = false;
  mNextXtiltOffset = 0.;
  mNextYtiltOffset = 0.;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  if (mModeWasContinuous)
    mConSets[mFocusSetNum].mode = CONTINUOUS;
  mModeWasContinuous = false;
}


///////////////////////////////////////////////////////
// CHECKING ACCURACY OF FOCUS CALIBRATION/ABILITY TO AUTOFOCUS

// Check accuracy of autofocus by measuring defocus at plus and minus defocus values
void CFocusManager::CheckAccuracy(int logging)
{
  CString report;
  mCalDelta = fabs((double)mCheckDelta);
  if (mCalDelta < 1.)
    return;
  mCheckLogging = logging;
  mCalDefocus = 0.;
  mFCindex = 0;
  mAccuracyMeasured = false;
  report.Format("Checking accuracy of autofocus by measuring defocus at\r\n"
    "current value and at %.1f microns above and below current defocus", mCalDelta);
  mWinApp->AppendToLog(report, 
    logging == LOG_OPEN_IF_CLOSED ? logging : LOG_SWALLOW_IF_CLOSED);
  AutoFocusStart(-1);
  mWinApp->AddIdleTask(TaskCheckBusy, TaskCheckDone, TaskFocusError, 0, 0);
}

// Process result when a focus measurement is done
void CFocusManager::CheckDone(int param)
{
  double delta = mCalDelta;
  CString report;
  if (mFCindex < 0)
    return;
  
  switch (mFCindex) {

    // First time, save defocus, change in plus direction and start again
  case 0:
    mCheckFirstMeasured = mCurrentDefocus;
    break;

    // Second time, compute plus accuracy, change in minus, and start again
  case 1:
    mPlusAccuracy = (float)((mCurrentDefocus - mCheckFirstMeasured) / mCalDelta);
    delta = -2. * mCalDelta;
    break;

    // Third time, compute minus accuracy, set flags, report result and return;
  case 2:
    mMinusAccuracy = (float)((mCheckFirstMeasured - mCurrentDefocus) / mCalDelta);
    mScope->IncDefocus(mCalDelta);
    mFCindex = -1;
    mAccuracyMeasured = true;
    report.Format("Measured defocus change was:\r\n   %.2f times actual change"
        " of +%.2f"" micron,\r\n   %.2f times actual change of %.2f micron\r\n"
        "Returning to original defocus value", 
        mPlusAccuracy, mCalDelta, mMinusAccuracy, -mCalDelta);
    mWinApp->AppendToLog(report, mCheckLogging);
    return;
  }

  // Change focus and start next measurement
  mScope->IncDefocus(delta);
  mCalDefocus += delta;
  mFCindex++;
  AutoFocusStart(-1);
  mWinApp->AddIdleTask(TaskCheckBusy, TaskCheckDone, TaskFocusError, 0, 0);
}

int CFocusManager::TaskCheckBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mFocusManager->DoingFocus() ? 1 : 0;
}

void CFocusManager::TaskCheckDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mFocusManager->CheckDone(param);
}

// return the last measured accuracy factors and flag for whether they
// were measured
BOOL CFocusManager::GetAccuracyFactors(float &outPlus, float &outMinus)
{
  outPlus = mAccuracyMeasured ? mPlusAccuracy : 1.f;
  outMinus = mAccuracyMeasured ? mMinusAccuracy : 1.f;
  return mAccuracyMeasured;
}


//////////////////////////////////////////////////////////////////////
// ROUTINES TO WORK WITH FOCUS CALIBRATIONS 

// Get the calibration for given mag and camera and currently selected direction
int CFocusManager::GetFocusCal(int inMag, int inCam, int probeMode, int inAlpha,
  FocusTable &focCal)
{
  int iDelMag, iPass, iDir, iMag, iCamTry, focInd, iAct, iCamPass, alphaPass;
  int alphaSteps = (mScope->GetHasNoAlpha() ? 1 : 2);
  ScaleMat refMat, curMat, refInv, delMat;
  int numCam = mWinApp->GetNumReadInCameras();
  int *activeList = mWinApp->GetActiveCameraList();

  if (mCamParams[inCam].STEMcamera)
    return 0;
  if ((focInd = LookupFocusCal(inMag, inCam, mTiltDirection, probeMode, inAlpha, false))
    >= 0) {
      focCal = mFocTab[focInd];
      SEMTrace('C', "Returning direct focus cal for mag %d cam %d a %d", inMag, inCam, 
        inAlpha);
      return 1; // calibrated at this mag
  }

  // Search other mags then other cameras in two passes
  // So the order of search is same camera closest mag,
  // other camera same mag, other camera closest mag
  // first going through these possibilities with both IS calibrated
  // and congruent, then going through without that requirement
  for (iPass = 0; iPass < 2; iPass++) {
    for (alphaPass = 0; alphaPass < alphaSteps; alphaPass++) {
      for (iCamPass = 0; iCamPass < 2; iCamPass++) {
        for (iAct = 0; iAct < numCam; iAct++) {
          iCamTry = activeList[iAct];
          if (mCamParams[iCamTry].STEMcamera)
            continue;
          if ((!iCamPass && iCamTry == inCam) || (iCamPass && iCamTry != inCam)) {
            for (iDelMag = 0; iDelMag < MAX_MAGS; iDelMag++) {
              for (iDir = -1; iDir <= 1; iDir += 2) {
                iMag = inMag + iDelMag * iDir;
                if (iMag < 1 || iMag >= MAX_MAGS)
                  continue;

                // On first pass, insist that both image shifts be calibrated
                if (!iPass && (mMagTab[iMag].matIS[iCamTry].xpx == 0. ||
                  mMagTab[inMag].matIS[inCam].xpx == 0. || 
                  !mShiftManager->CanTransferIS(iMag, inMag)))
                  continue;
                if ((focInd = LookupFocusCal(iMag, iCamTry, mTiltDirection, probeMode, 
                  inAlpha, alphaPass > 0)) < 0)
                  continue;

                // Rotate the vector by the image shift or specimen change
                if (iPass) {
                  refMat = mShiftManager->SpecimenToCamera(iCamTry, iMag);
                  curMat = mShiftManager->SpecimenToCamera(inCam, inMag);
                } else {
                  refMat = mShiftManager->IStoGivenCamera(iMag, iCamTry);
                  curMat = mShiftManager->IStoCamera(inMag);
                }
                refInv = MatInv(refMat);
                delMat = MatMul(refInv, curMat);
                TransformFocusCal(&mFocTab[focInd], &focCal, delMat);
                SEMTrace('C', "Using transformed focus cal on pass %d from mag-cam-a "
                  "%d-%d-%d to %d-%d-%d", iPass, iMag, iCamTry, mFocTab[focInd].alpha, 
                  inMag, inCam, inAlpha);
                return iPass + 2;
              }
            }
          }
        }
      }
    }
  } 
  return 0;
}

// Returns the table index of a calibration matching the given parameters or -1 if none
int CFocusManager::LookupFocusCal(int magInd, int camera, int direction, int probeMode,
  int alpha, bool matchNoAlpha)
{
  for (int ind = 0; ind < mFocTab.GetSize(); ind++) {
    if (mFocTab[ind].magInd == magInd && mFocTab[ind].camera == camera &&
      mFocTab[ind].direction == direction && mFocTab[ind].probeMode == probeMode &&
      ((!matchNoAlpha && (alpha < 0 || alpha == mFocTab[ind].alpha)) || 
      (matchNoAlpha &&  mFocTab[ind].alpha < 0)))
      return ind;
  }
  return -1;
}

// Transform the input focus calibration by the delta scale matrix
void CFocusManager::TransformFocusCal(FocusTable *inCal, FocusTable *outCal, 
                    ScaleMat delMat)
{
  *outCal = *inCal;
  outCal->slopeX = delMat.xpx * inCal->slopeX + delMat.xpy * inCal->slopeY;
  outCal->slopeY = delMat.ypx * inCal->slopeX + delMat.ypy * inCal->slopeY;
  for (int i = 0; i < inCal->numPoints; i++) {
    outCal->shiftX[i] = delMat.xpx * inCal->shiftX[i] + 
      delMat.xpy * inCal->shiftY[i];
    outCal->shiftY[i] = delMat.ypx * inCal->shiftX[i] + 
      delMat.ypy * inCal->shiftY[i];
  } 
  
}

// Change all old (not newly calibrated) focus tables by the same scaling matrix
// for the given camera, and for the given firection or all directions by default
void CFocusManager::ModifyAllCalibrations(ScaleMat delMat, int iCam, int direction)
{
  int focInd;
  for (focInd = 0; focInd < mFocTab.GetSize(); focInd++) {
    if (!mFocTab[focInd].calibrated && mFocTab[focInd].camera == iCam && 
      (direction < 0 || mFocTab[focInd].direction == direction)) {
        TransformFocusCal(&mFocTab[focInd], &mNewCal, delMat);
        mFocTab[focInd] = mNewCal;
    }
  }
}

float CFocusManager::DefocusFromCal(FocusTable *focCal, float inX, float inY, 
                  float &minDist)
{
  float slopeX, slopeY, minDefocus, defocus, df1, df2;
  double lineDist, delX, delY;
  int i, iDir;

  minDist = 1.e10;
  for (i = 0; i < focCal->numPoints - 1; i++) {
    iDir = 1;
    df1 = focCal->defocus[i];
    df2 = focCal->defocus[i + 1];
    if (df2 < df1)
      iDir = -1;

    // Compute defocus value of closest approach to extended line
    slopeX = (focCal->shiftX[i + 1] - focCal->shiftX[i]) / (df2 - df1);
    slopeY = (focCal->shiftY[i + 1] - focCal->shiftY[i]) / (df2 - df1);
    defocus = df1 + (slopeX * (inX - focCal->shiftX[i]) + 
      slopeY * (inY - focCal->shiftY[i])) /
      (slopeX * slopeX + slopeY * slopeY);

    // Limit defocus value unless past ends of data; i.e. measure to segment
    if (i > 0 && iDir * defocus < iDir * df1)
      defocus = df1;
    if (i < focCal->numPoints - 2 && iDir * defocus > iDir * df2)
      defocus = df2;

    // Compute distance to line segment
    delX = focCal->shiftX[i] + slopeX * (defocus - df1) - inX;
    delY = focCal->shiftY[i] + slopeY * (defocus - df1) - inY;
    lineDist = sqrt(delX * delX + delY * delY);

    // maintain defocus at minimum distance 
    if (lineDist < minDist) {
      minDist = (float)lineDist;
      minDefocus = defocus;
    }
  }

  return minDefocus;
}

// Checks for whether the primary peak is at 0,0 and if so, tests whether the second peak
// is closer to target defocus and and strong enough relative to main peak.
// binning can be negative for second beam tilted shot.  Returns index of peak to use
int CFocusManager::CheckZeroPeak(float * xPeak, float * yPeak, float * peakVal, int binning)
{
  // Amount by which 0 peak must be closer to target than second peak
  float peakRatioLimit = 0.2f;   // Limit to how much smaller the second peak can be
  float distance0, distance1, defocus0, defocus1, ratio;
  CString report;

  // But the expected defocus is the cal focus, not the target defocus, when doing either 
  // cal or checking autofocus
  // And it should include the autofocus offset and View defocus offset
  float expected = mTargetDefocus;
  if (mFCindex >= 0)
    expected = (float)mCalDefocus;
  else {
    expected += mDefocusOffset;
    if (mFocusSetNum == VIEW_CONSET)
      expected += mWinApp->mScope->GetLDViewDefocus(VIEW_CONSET);
  }

  if (fabs(xPeak[0]) > 0.5 || fabs(yPeak[0]) > 0.5)
    return 0;

  // The second value returned is the unadjusted defocus derived from the shifts
  if (CurrentDefocusFromShift(binning * xPeak[0], -binning * yPeak[0], ratio, defocus0) || 
    CurrentDefocusFromShift(binning * xPeak[1], -binning * yPeak[1], ratio, defocus1))
    return 0;

  distance0 = (float)fabs(defocus0 - expected);
  distance1 = (float)fabs(defocus1 - expected);
  if (distance1 > distance0 * mMaxPeakDistanceRatio)
    return 0;

  // The allowed ratio between peaks decreases from 1 at the max distance ratio to 
  // smaller ratios (smaller 2nd peaks) at smaller distance ratios
  ratio = distance1 / (distance0 * mMaxPeakDistanceRatio);
  if (ratio < peakRatioLimit)
    ratio = peakRatioLimit;
  if (peakVal[1] > ratio * peakVal[0]) {
    report.Format("Using peak at %.1f,%.1f, rejecting %.2f times higher "
      "peak at %.1f,%.1f", xPeak[1], yPeak[1], peakVal[0] / peakVal[1], xPeak[0], yPeak[0]);
    mWinApp->AppendToLog(report, mVerbose ? LOG_OPEN_IF_CLOSED : LOG_SWALLOW_IF_CLOSED);
    return 1;
  }
  return 0;
}

// Return the drift estimate from the last focus, return value 1 if not available
int CFocusManager::GetLastDrift(double & lastX, double & lastY)
{
  if (!mDriftStored)
    return 1;
  lastX = mLastDriftX;
  lastY = mLastDriftY;
  return 0;
}

// Set the target defocus and update the displayed value
void CFocusManager::SetTargetDefocus(float val)
{
  mTargetDefocus = val;
  mWinApp->mAlignFocusWindow.UpdateSettings();
}

// Set the tilt direction between 0 and 3, set fractions for the beam tilts
void CFocusManager::SetTiltDirection(int inVal)
{
  float frac = (float)sqrt(0.5);
  mTiltDirection = inVal % 4;
  if (mTiltDirection % 2) {
    mFracTiltX = mTiltDirection > 2 ? -frac : frac;
    mFracTiltY = frac;
  } else {
    mFracTiltX = mTiltDirection ? 0.f : 1.f;
    mFracTiltY = mTiltDirection ? 1.f : 0.f;
  }
}

// Gets a rotationally averaged power spectrum and returns background power and
// total power with defined ranges
int CFocusManager::RotationAveragedSpectrum(EMimageBuffer * imBuf, float * rotav, 
                                            float & background, float & totPower)
{
  KImage *image = imBuf->mImage;
  float *fftarray;
  int *ninRing;
  float angle, rotation;
  int ix0, ix1, iy0, iy1, width, nxpad, nypad;
  int nx = image->getWidth();
  int ny = image->getHeight();
  int probeMode = mScope->GetProbeMode();

  // Set up the subarea to use: if there is a normalized slope and the tilt is sizable,
  // get the width that will contain the maximum allowed change in beam size
  // constrain it appropriately and apply it across the tilt axis
  ix0 = iy0 = 0;
  ix1 = nx - 1;
  iy1 = ny - 1;

  if (mSFnormalizedSlope[probeMode] && !imBuf->mDynamicFocused) {
    if (!imBuf->GetTiltAngle(angle))
      angle = (float)mScope->GetTiltAngle();
    if (imBuf->GetAxisAngle(rotation) && fabs((double)angle) > 10.) {
      width = (int)(mSFmaxTiltedSizeRange * mSTEMdefocusToDelZ[probeMode] /
        (mSFnormalizedSlope[probeMode] * tan(DTOR * angle)));
      if (fabs((double)rotation) < 45. || fabs(fabs((double)rotation) - 180.) < 45.) {
        B3DCLAMP(width, (int)(mSFminTiltedFraction * ny), ny);
        iy0 = (ny - width) / 2;
        iy1 = iy0 + width - 1;
      } else {
        B3DCLAMP(width, (int)(mSFminTiltedFraction * nx), nx);
        ix0 = (nx - width) / 2;
        ix1 = ix0 + width - 1;
      }
    }
  }

  nxpad = XCorrNiceFrame(ix1 + 1 - ix0, 2, 19);
  nypad = XCorrNiceFrame(iy1 + 1 - iy0, 2, 19);
  if (!mNumRotavs)
    SEMTrace('f', "Analyzing %d x %d, X %d to %d, Y %d to %d", nxpad, nypad, ix0, ix1, 
    iy0, iy1);

  // Get memory for the real FFT
  NewArray(fftarray, float, nypad * (nxpad + 2));
  NewArray(ninRing, int, mRFTnumPoints);
  if (!fftarray || !ninRing) {
   SEMMessageBox("Failed to get memory for doing rotationally averaged FFT", MB_EXCLAME);
    delete [] ninRing;
    delete [] fftarray;
    return 1;
  }

  // Get transform and rotational average
  image->Lock();
  ProcRotAvFFT(image->getData(), image->getType(), nx, ix0, ix1, iy0, iy1, fftarray, 
    nxpad, nypad, rotav, ninRing, mRFTnumPoints);
  image->UnLock();

  delete [] fftarray;
  delete [] ninRing;
  totalSubtractedPower(rotav, mRFTnumPoints, mRFTbkgdStart, mRFTbkgdEnd, mRFTtotPowStart, 
    mRFTtotPowEnd, &background, &totPower);

  // Get the background and power
  return 0;
}

void CFocusManager::CalSTEMfocus(void)
{
  if (AfxMessageBox(
    "This procedures estimates the spreading of the beam with defocus using"
    " a series of pictures with the " +  mModeNames[1] + " parameter set.\n"
    "Before proceeding, make sure that this parameter set takes\n"
    "fairly good pictures and that you are already well focused.\n\n"
    "Do you want to proceed?", MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;
  if (!KGetOneFloat("Total defocus range to take pictures over:", mSFcalRange, 1))
    return;
  if (!KGetOneInt("Number of focus levels to test:", mSFnumCalSteps))
    return;
  B3DCLAMP(mSFnumCalSteps, 3, MAX_STEMFOC_STEPS);
  mSFbaseFocus = mScope->GetDefocus();
  mCalDelta = mSFcalRange / (mSFnumCalSteps - 1);
  mCalDefocus = -mCalDelta * (mSFnumCalSteps - 1) / 2.;
  mNumRotavs = 0;
  mSFtask = STEM_FOCUS_CAL;
  mModeWasContinuous = mConSets[FOCUS_CONSET].mode == CONTINUOUS;
  mConSets[FOCUS_CONSET].mode = SINGLE_FRAME;
  if (mCamera->CameraBusy()) 
    mCamera->StopCapture(0);
  mWinApp->UpdateBufferWindows();
  StartSTEMfocusShot();
}

void CFocusManager::StartSTEMfocusShot(void)
{
  if (mSFbacklash)
    mScope->SetDefocus(mSFbaseFocus + mCalDefocus - mSFbacklash);
  mScope->SetDefocus(mSFbaseFocus + mCalDefocus);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_STEM_FOCUS, 0, 0);
  mCamera->InitiateCapture(FOCUS_CONSET);
}

void CFocusManager::STEMfocusShot(int param)
{
  CString str;
  float slope1, slope2, intcp1, intcp2, ro1, ro2, meanSlope, bestFocus, paraFocus;
  double step;
  float fitFocus[MAX_STEMFOC_STEPS], sizes[MAX_STEMFOC_STEPS];
  float fitPowers[7], fitBackgrounds[7];
  float *fitRotavs[7];
  float maxUsableSize = 90.;
  int i, ind, fit1Start, fit2End, numpts1, numpts2;
  int bufInd = param > 0 ? param - 1 : 0;
  if (mNumRotavs < 0)
    return;
  if (!mImBufs[bufInd].mImage) {
    if (param) {
      str.Format("No image in buffer %c; be sure to load all required images sequentially"
        , 'A' + param - 1);
      AfxMessageBox(str, MB_EXCLAME);
    }
    StopSTEMfocus();
    return;
  }

  // Get the power spectrum
  NewArray(mRFTrotavs[mNumRotavs], float, mRFTnumPoints);
  if (!mRFTrotavs[mNumRotavs] || RotationAveragedSpectrum(&mImBufs[bufInd], 
    mRFTrotavs[mNumRotavs], mRFTbackgrounds[mNumRotavs], mRFTpowers[mNumRotavs])) {
      SEMMessageBox("Failed to get array for power spectrum in STEM focusing", 
        MB_EXCLAME);
      StopSTEMfocus();
      return;
  }

  // Keep track of the index with maximum power
  if (!mNumRotavs || mRFTpowers[mNumRotavs] > mRFTpowers[mSFmaxPowerInd])
    mSFmaxPowerInd = mNumRotavs;
  mRFTdefocus[mNumRotavs++] = (float)mCalDefocus;
  if (mSFtask == STEM_FOCUS_CAL) {

    // Focus slope calibration
    if (mNumRotavs < mSFnumCalSteps) {
      mCalDefocus += mCalDelta;
      StartSTEMfocusShot();
      return;
    }

    // Finished with calibration shots: make sure there are enough points for fits
    if (mSFmaxPowerInd <= 1 || mSFmaxPowerInd >= mNumRotavs - 2) {
      AfxMessageBox("The highest power is too near the end of the range\n"
        "Focus better or use more focus levels", MB_EXCLAME);
      StopSTEMfocus();
      return;
    }
    if (MakeSincLikeCurve()) {
      StopSTEMfocus();
      return;
    }

    // Find the sizes and keep track of which ones can be used in fits
    fit1Start = 0;
    fit2End = -1;
    mWinApp->AppendToLog("\r\nDefocus  Total power  Fitted size");
    for (i = 0; i < mNumRotavs; i++) {
      if (i == mSFmaxPowerInd) {
        str.Format("%8.2f  %12.2f  (  1.0 assumed)", mRFTdefocus[i] + mSFbaseFocus,
          mRFTpowers[i]);
        mWinApp->AppendToLog(str);
        continue;
      }
      setupDefocusFit(mRFTrotavs[i], mRFTrotavs[mSFmaxPowerInd], mRFTnumPoints,
        mRFTbackgrounds[i], mRFTbackgrounds[mSFmaxPowerInd], mRFTfitStart, mRFTfitEnd,
        mSincCurve, mNumSincPts, mNumSincNodes, (float)fabs(5.*(i - mSFmaxPowerInd)), 1.,
        3, 0, 1.);
      findDefocusSizes(&sizes[i], &ro1, &ro2, 0);
      str.Format("%8.2f  %12.2f  %8.2f", mRFTdefocus[i] + mSFbaseFocus, mRFTpowers[i],
        sizes[i]); 
      mWinApp->AppendToLog(str);
      fitFocus[i] = (float)fabs(mRFTdefocus[i] - mRFTdefocus[mSFmaxPowerInd]);
      if (sizes[i] > maxUsableSize) {
        if (i < mSFmaxPowerInd)
          fit1Start = i + 1;
        else if (fit2End < 0)
          fit2End = i - 1;
      }
    }
    if (fit2End < 0)
      fit2End = mNumRotavs - 1;
    numpts1 = mSFmaxPowerInd - fit1Start;
    numpts2 = fit2End - mSFmaxPowerInd;
    if (numpts1 < 2 || numpts2 < 2) {
      str.Format("There are too few points to fit because sizes\nabove %.f need to be"
        "excluded from the fits.\n\nUse a smaller focus range", maxUsableSize);
      AfxMessageBox(str, MB_EXCLAME);
      StopSTEMfocus();
      return;
    }

    lsFit(&fitFocus[fit1Start], &sizes[fit1Start], numpts1, &slope1, &intcp1, &ro1);
    lsFit(&fitFocus[mSFmaxPowerInd + 1], &sizes[mSFmaxPowerInd + 1], numpts2,
      &slope2, &intcp2, &ro2);
    bestFocus = (intcp1 - intcp2) / (slope1 + slope2) + mRFTdefocus[mSFmaxPowerInd] +
      (float)mSFbaseFocus;
    meanSlope = (numpts1 * slope1 + numpts2 *slope2) / (numpts1 + numpts2);
    ind = mScope->GetProbeMode();
    mSFnormalizedSlope[ind] = meanSlope * mShiftManager->GetPixelSize(&mImBufs[bufInd]);
    str.Format("Slopes of fits below and above are %.2f and %.2f pixel/um (CCs %.4f and "
      "%.4f)\r\nWeighted mean slope is %.2f pixel/um, %f um/um\r\n"
      "Best focus should be at %.2f um", slope1, slope2, ro1, ro2, meanSlope, 
      mSFnormalizedSlope[ind], bestFocus);
    mWinApp->AppendToLog(str);
    if (fit1Start > 0 || fit2End < mNumRotavs - 1) {
      str.Format("WARNING: Some points were excluded because solved sizes were above %f"
        "\r\n   You should rerun this with a defocus range of %.1f or less",
        maxUsableSize, mRFTdefocus[fit2End] - mRFTdefocus[fit1Start]);
      mWinApp->AppendToLog(str);
    }
    if (sizes[0] < 15. && sizes[mNumRotavs-1] < 15.) {
      str.Format("WARNING: The solved sizes did not get very big."
        "\r\n   You should rerun this with a defocus range of about %.0f um",
        60. * mSFcalRange / (sizes[0] + sizes[mNumRotavs-1]));
      mWinApp->AppendToLog(str);
    }
    StopSTEMfocus();
    mWinApp->SetCalibrationsNotSaved(true);
    return;

  } else {

    // AUTOFOCUS
    // If current image is best one, copy its imbuf
    if (mSFbestImBuf && mSFmaxPowerInd == mNumRotavs - 1)
      mBufferManager->CopyImBuf(&mImBufs[bufInd], mSFbestImBuf, false);
    step = 0.;
    if (mSFtask == STEM_FOCUS_COARSE1 || mSFtask == STEM_FOCUS_COARSE2) {
  
      // In coarse phase, take a step in same direction if the current point is maximum
      if (mSFmaxPowerInd == mNumRotavs - 1) {
        step = 2. * mCalDelta;

        // After first point, if step to a new maximum, then we ARE in phase 2, take big
        if (mNumRotavs > 1) {
          mSFtask = STEM_FOCUS_COARSE2;
          step = 4. * mCalDelta;
        }
      } else {

        // Otherwise reverse direction and take big steps for coarse phase 2, or
        // go into fine phase
        mCalDelta = -mCalDelta;
        if (mSFtask == STEM_FOCUS_COARSE1) {
          mSFtask = STEM_FOCUS_COARSE2;
          step = 4. * mCalDelta;
        } else if (!mSFcoarseOnly) {
          mSFtask = STEM_FOCUS_FINE1;
        }
      }

    }

    // In fine phase, take whatever step is needed
    if (mSFtask == STEM_FOCUS_FINE1) {
      step = StepForSTEMFocus(2);
      if (!step)
        step = StepForSTEMFocus(1);
    }

    if (step) {
      SEMTrace('f', "Power %.1f  at %.2f   taking type %d step of %.2f", 
        mRFTpowers[mNumRotavs-1], mCalDefocus, mSFtask, step);

      // Going to another step, make sure limit is not exceeded
      if (((mSFtask == STEM_FOCUS_COARSE1 || mSFtask == STEM_FOCUS_COARSE2) && 
           (mNumRotavs >= MAX_STEMFOC_STEPS - 6 || (mNumRotavs >= mSFstepLimit && 
            mRFTpowers[mNumRotavs-1] / mRFTpowers[mNumRotavs-5] < 1.5))) || 
        mNumRotavs >= MAX_STEMFOC_STEPS) {
          i = IDNO;
          if (mWinApp->DoingTiltSeries() || mWinApp->mMontageController->DoingMontage() ||
            mWinApp->mMacroProcessor->DoingMacro() || 
            mWinApp->mComplexTasks->DoingTasks()) {
              SEMMessageBox("Too many focus steps have been taken\r\n"
                "without finding a maximum power", MB_EXCLAME);
          } else {
            i = AfxMessageBox("Too many focus steps have been taken\r\n"
              "without finding a maximum power\n\n"
              "Do you want to start autofocusing again from the last tested defocus?",
              MB_YESNO | MB_ICONQUESTION);
          }
          StopSTEMfocus();
          if (i == IDYES) {
            mScope->SetDefocus(mSFbaseFocus + mCalDefocus);
            AutoFocusStart(1);
          }
          return;
      }
      mCalDefocus += step;
      if (!param)
        StartSTEMfocusShot();
      return;
    }
      // Or, done, evaluate the focus
    if (MakeSincLikeCurve()) {
      StopSTEMfocus();
      return;
    }

    // Load up to 7 curves for the fit; in reality this will be 5 or rarely 6
    numpts1 = 0;
    for (ind = -3; ind <= 3; ind++) {
      i = RotavIndexForDefocus(mRFTdefocus[mSFmaxPowerInd] + ind * mCalDelta);
      if (i >= 0) {
        fitFocus[numpts1] = mRFTdefocus[i];
        fitBackgrounds[numpts1] = mRFTbackgrounds[i];
        fitPowers[numpts1] = mRFTpowers[i];
        fitRotavs[numpts1++] = mRFTrotavs[i];
      }
    }
    slope1 = mSFnormalizedSlope[mScope->GetProbeMode()] / 
      mShiftManager->GetPixelSize(&mImBufs[bufInd]);
    i = findFocus(fitRotavs, numpts1, mRFTnumPoints, fitFocus, 
      fitPowers, fitBackgrounds, slope1, mRFTfitStart, mRFTfitEnd, 
      mSincCurve, mNumSincPts, mNumSincNodes, 0, 0, 1., &paraFocus, &bestFocus, &slope1);
    if (i > 0) {
      str.Format("findFocus gave an unexpected error %d", i);
      AfxMessageBox(str, MB_EXCLAME);
      StopSTEMfocus();
      return;
    }
    SEMTrace('f', "Power %.1f  at %.2f  n=%d parabolic = %.2f   best = %.2f  slope = %.2f", 
      mRFTpowers[mNumRotavs-1], mCalDefocus, numpts1, paraFocus, bestFocus, slope1);
    mCurrentDefocus = -bestFocus;
    str.Format("Measured defocus = %.2f microns", mCurrentDefocus);
    if (mSFcoarseOnly)
      str += "    (Based on fit to 3 power values after coarse steps)";
    else if (i < 0)
      str += "    (Based on fit to 3 power values due to bad fit to sizes)";
    else if (mSFbestImBuf && mSFmaxPowerInd != mNumRotavs - 1)
      str += "    (Best focused image copied to A)";
    mWinApp->AppendToLog(str, LOG_SWALLOW_IF_CLOSED);
    if (mDoChangeFocus)
      mSFbaseFocus += bestFocus;
    if (mSFbestImBuf && mSFmaxPowerInd != mNumRotavs - 1) {
      for (i = mBufferManager->GetShiftsOnAcquire(); i > 0 ; i--)
        mBufferManager->CopyImageBuffer(i - 1, i);
      mBufferManager->CopyImBuf(mSFbestImBuf, mImBufs);
    }
    mLastFailed = false;
    StopSTEMfocus();
  }
}

void CFocusManager::STEMfocusCleanup(int error)
{
  StopSTEMfocus();
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting focus detection pictures"), MB_EXCLAME);
}

void CFocusManager::StopSTEMfocus(void)
{
  if (mModeWasContinuous)
    mConSets[FOCUS_CONSET].mode = CONTINUOUS;
  mModeWasContinuous = false;
  if (mNumRotavs < 0)
    return;
  for (int i = 0; i < mNumRotavs; i++)
    delete [] mRFTrotavs[i];
  delete mSincCurve;
  mSincCurve = NULL;
  delete mSFbestImBuf;
  mSFbestImBuf = NULL;
  mCamera->RestoreMagAndShift();
  mScope->SetDefocus(mSFbaseFocus);
  mNumRotavs = -1;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

int CFocusManager::MakeSincLikeCurve(void)
{
  NewArray(mSincCurve, float, mNumSincPts);
  if (!mSincCurve)
    return 1;
  sincLikeFunction(mNumSincPts, mNumSincNodes, mSincCurve);
  return 0;
}

double CFocusManager::StepForSTEMFocus(int multiple)
{
  double newFocus;
  int indAbove, indBelow;
  double bestFocus = mRFTdefocus[mSFmaxPowerInd];
  mCalDelta = fabs(mCalDelta);
  double newBelow = bestFocus - multiple * mCalDelta;
  double newAbove = bestFocus + multiple * mCalDelta;
  bool belowDone = RotavIndexForDefocus(newBelow) >= 0;
  bool aboveDone = RotavIndexForDefocus(newAbove) >= 0;

  // Check on either side at this step; if both done, step is 0
  //SEMTrace('f', "mult %d best %.2f  below %.2f done %d above %.2f done %d",
//multiple, bestFocus, newBelow, belowDone?1:0, newAbove, aboveDone?1:0);
  if (belowDone && aboveDone)
    return 0.;

  // If one is done and not the other, go to the undone one
  if (belowDone && !aboveDone) {
    newFocus = newAbove;
  } else if (!belowDone && aboveDone) {
    newFocus = newBelow;
  } else {

    // If both are undone, check on both sides at twice the step and go in the direction
    // where power is already higher
    newFocus = newAbove;
    indBelow = RotavIndexForDefocus(bestFocus - 2 * multiple * mCalDelta);
    indAbove = RotavIndexForDefocus(bestFocus + 2 * multiple * mCalDelta);
    if (indBelow >= 0 && indAbove >= 0 && mRFTpowers[indBelow] > mRFTpowers[indAbove])
      newFocus = newBelow;
  }
  //SEMTrace('f', "new %.2f cal %.2f", newFocus, mCalDefocus);
  return newFocus - mCalDefocus;
}

int CFocusManager::RotavIndexForDefocus(double defocus)
{
  for (int i = 0; i < mNumRotavs; i++)
    if (fabs((mRFTdefocus[i] - defocus) / mCalDelta) < 0.1)
      return i;
  return -1;
}

// Calibrating STEM focus versus Z
void CFocusManager::OnStemFocusVsZ()
{
  double stageX, stageY;
  if (AfxMessageBox("This calibration steps the stage to a series of Z levels and "
    "autofocuses at each one.\n\nThe stage should be at the eucentric height, images"
    " should\nalready be close to focus, and the magnification\nshould be suitable for "
    "measuring focus accurately.\n\nDo you want to proceed?", MB_YESNO | MB_ICONQUESTION)
    == IDNO)
    return;
  if (!KGetOneFloat("Range in Z over which to measure focus:", mSFVZrangeInZ, 1))
    return;
  if (!KGetOneInt("Number of Z levels to measure:", mSFVZnumLevels))
    return;
  B3DCLAMP(mSFVZnumLevels, 3, MAX_FOCUS_Z_LEVELS);
  if (mSFVZrangeInZ < 1.)
    return;
  mScope->GetStagePosition(stageX, stageY, mSFVZbaseZ);
  mSFVZbaseFocus = mScope->GetDefocus();
  SEMTrace('f', "base Z %.2f  defocus %.2f", mSFVZbaseZ, mSFVZbaseFocus);
  mSFVZindex = -2;
  mSFVZtable.numPoints = 0;
  mSFVZtable.probeMode = mScope->GetProbeMode();
  mSFVZtable.spotSize = mScope->GetSpotSize();
  mSFVZdefocusToDeltaZ = mSTEMdefocusToDelZ[mSFVZtable.probeMode];
  mWinApp->SetStatusText(COMPLEX_PANE, "MEASURING FOCUS(Z)");
  mWinApp->UpdateBufferWindows();
  ChangeZandDefocus(-mSFVZinitialDelZ, -mSFVZbacklashZ);
}

void CFocusManager::OnUpdateStemFocusVsZ(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mWinApp->GetSTEMMode() && FocusReady() && !mWinApp->DoingTasks());
}

void CFocusManager::ChangeZandDefocus(double relativeZ, float backlash)
{
  StageMoveInfo moveInfo;
  mScope->GetStagePosition(moveInfo.x, moveInfo.y, moveInfo.z);
  double delZ = mSFVZbaseZ + relativeZ - moveInfo.z;
  mScope->IncDefocus(delZ / mSFVZdefocusToDeltaZ);
  moveInfo.axisBits = axisZ;
  moveInfo.z = mSFVZbaseZ + relativeZ;
  moveInfo.backZ = backlash;
  SEMTrace('f', "ChangeZandDefocus moving stage to %.2f backlash %d", moveInfo.z, backlash!=0.?1:0);
  mScope->MoveStage(moveInfo, backlash != 0.);
  mSFVZmovingStage = true;
  mWinApp->AddIdleTask(TASK_FOCUS_VS_Z, 0, 30000);
}

void CFocusManager::FocusVsZnextTask(int param)
{
  CString report;
  double stageX, stageY, stageZ, defocus;
  int iCen, smoothFit = 3, iFitStr;
  float fitSlope, fitIntcp, minusFac, plusFac, midFac, wholeFac;
  if (mSFVZindex < -2)
    return;
  mScope->GetStagePosition(stageX, stageY, stageZ);
  defocus = mScope->GetDefocus();
  if (mSFVZmovingStage) {
    mSFVZmovingStage = false;
    AutoFocusStart(1);
    mWinApp->AddIdleTask(TASK_FOCUS_VS_Z, 0, 0);
    return;
  }
  if (mSFVZindex == -2) {
    mSFVZtestFocus = defocus;
    mSFVZtestZ = stageZ;
    mSFVZindex++;
    ChangeZandDefocus(0., 0.);
    return;
  }
  if (mSFVZindex == -1) {
    mSFVZbaseFocus = defocus;
    mSFVZbaseZ = stageZ;
    mSFVZdefocusToDeltaZ = (float)((stageZ - mSFVZtestZ) / (defocus - mSFVZtestFocus));
    report.Format("\r\nInitial estimate of defocusToDeltaZ %.4f", mSFVZdefocusToDeltaZ);
    mWinApp->AppendToLog(report);
    mSFVZindex++;
    ChangeZandDefocus(-mSFVZrangeInZ / 2., -mSFVZbacklashZ);
    return;
  }
  mSFVZtable.absFocus[mSFVZindex] = (float)mScope->GetFocus();
  mSFVZtable.defocus[mSFVZindex] = (float)defocus;
  mSFVZtable.stageZ[mSFVZindex++] = (float)stageZ;
  mSFVZtable.numPoints++;
  if (mSFVZindex < mSFVZnumLevels) {
    ChangeZandDefocus(-mSFVZrangeInZ / 2. + mSFVZindex * mSFVZrangeInZ / 
      (mSFVZnumLevels - 1), 0.);
    return;
  }

  // Done: smooth the table
  StopFocusVsZ();
  mWinApp->AppendToLog("\r\n   Z   defocus   Abs Focus   Before smoothing");
  for (iCen = 0; iCen < mSFVZnumLevels; iCen++) {
    report.Format("%2d %8.2f  %8.2f  %8.5f", iCen, mSFVZtable.stageZ[iCen], 
      mSFVZtable.defocus[iCen], mSFVZtable.absFocus[iCen]);
    mWinApp->AppendToLog(report);
    iFitStr = iCen - smoothFit / 2;
    B3DCLAMP(iFitStr, 0, mSFVZnumLevels - smoothFit);
    StatLSFit(&mSFVZtable.stageZ[iFitStr], &mSFVZtable.defocus[iFitStr], smoothFit,
      fitSlope, fitIntcp);
    mSFVZtable.defocus[iCen] = mSFVZtable.stageZ[iCen] * fitSlope + fitIntcp;
    StatLSFit(&mSFVZtable.stageZ[iFitStr], &mSFVZtable.absFocus[iFitStr], smoothFit,
      fitSlope, fitIntcp);
    mSFVZtable.absFocus[iCen] = mSFVZtable.stageZ[iCen] * fitSlope + fitIntcp;
  }
  mWinApp->AppendToLog("\r\n   Z   defocus   Abs Focus   After smoothing");
  for (iCen = 0; iCen < mSFVZnumLevels; iCen++) {
    report.Format("%2d %8.2f  %8.2f  %8.5f", iCen, mSFVZtable.stageZ[iCen], 
      mSFVZtable.defocus[iCen], mSFVZtable.absFocus[iCen]);
    mWinApp->AppendToLog(report);
  }

  // Report fits over whole and segments
  StatLSFit(mSFVZtable.stageZ, mSFVZtable.defocus, mSFVZnumLevels, fitSlope, fitIntcp);
  SEMTrace('f', "Whole line %f %f", fitSlope, fitIntcp);
  wholeFac = 1.f / fitSlope;
  smoothFit = (mSFVZnumLevels + 1 ) / 2;
  StatLSFit(mSFVZtable.stageZ, mSFVZtable.defocus, smoothFit, fitSlope, fitIntcp);
  SEMTrace('f', "minus half curve %f %f", fitSlope, fitIntcp);
  minusFac = 1.f / fitSlope;
  iFitStr = (mSFVZnumLevels - smoothFit) / 2;
  StatLSFit(&mSFVZtable.stageZ[iFitStr], &mSFVZtable.defocus[iFitStr], smoothFit,
      fitSlope, fitIntcp);
  SEMTrace('f', "middle half curve %f %f", fitSlope, fitIntcp);
  midFac = 1.f / fitSlope;
  iFitStr = mSFVZnumLevels - smoothFit;
  StatLSFit(&mSFVZtable.stageZ[iFitStr], &mSFVZtable.defocus[iFitStr], smoothFit,
    fitSlope, fitIntcp);
  SEMTrace('f', "top half curve %f %f", fitSlope, fitIntcp);
  plusFac = 1.f / fitSlope;
  report.Format("STEMDefocusToDeltaZ from fit to whole set of points: %.4f\r\n"
    "   from fit to minus half: %.4f\r\n   from fit to middle half: %.4f\r\n"
    "   from fit to plus half: %.4f", wholeFac, minusFac, midFac, plusFac);
  mWinApp->AppendToLog(report);

  // FEI only for now: Save the table, replacing one of matching spot and probe mode
  if (FEIscope) {
    for (iCen = 0; iCen < mSFfocusZtables.GetSize(); iCen++)
      if (mSFfocusZtables[iCen].spotSize == mSFVZtable.spotSize &&
        mSFfocusZtables[iCen].probeMode == mSFVZtable.probeMode)
        mSFfocusZtables.RemoveAt(iCen--);
    mSFfocusZtables.Add(mSFVZtable);
    mWinApp->SetCalibrationsNotSaved(true);
  }
}

int CFocusManager::FocusVsZBusy(void)
{
  if (mSFVZmovingStage)
    return mScope->StageBusy();
  return mNumRotavs >= 0 ? 1 : 0;
}

void CFocusManager::FocusVsZcleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out in the STEM focus versus Z routine"));
  StopFocusVsZ();
  mWinApp->ErrorOccurred(error);
}

void CFocusManager::StopFocusVsZ(void)
{
  StageMoveInfo smi;
  if (mSFVZindex < -2)
    return;
  mScope->SetDefocus(mSFVZbaseFocus);
  mSFVZindex = -3;
  if (!mScope->WaitForStageReady(10000)) {
    smi.axisBits = axisZ;
    smi.z = mSFVZbaseZ;
    mScope->MoveStage(smi, false);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  }
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  mWinApp->UpdateBufferWindows();
}


// Find the table that best matches the spot size and probe mode
int CFocusManager::FindFocusZTable(int spotSize, int probeMode)
{
  int i, loop, idiff, minDiff, indBest;
  if (!mSFfocusZtables.GetSize())
    return -1;

  // Look for an exact match
  for (i = 0; i < mSFfocusZtables.GetSize(); i++)
    if (mSFfocusZtables[i].spotSize == spotSize &&
        mSFfocusZtables[i].probeMode == probeMode)
        return i;

  // Look first for closest spot size with probe mode match, then without
  for (loop = 0; loop < 2; loop++) {
    minDiff = 100;
    for (i = 0; i < mSFfocusZtables.GetSize(); i++) {
      if (loop || mSFfocusZtables[i].probeMode == probeMode) {
        idiff = mSFfocusZtables[i].spotSize - spotSize;
        if (idiff < 0)
          idiff = -idiff;
        if (idiff < minDiff) {
          minDiff = idiff;
          indBest = i;
        }
      }
    }
    if (minDiff < 100)
      return indBest;
  }
  return -1;
}

// Find a Z value in the given table for the given absolute focus
float CFocusManager::LookupZFromAbsFocus(int tableInd, float absFocus)
{
  float *zvals = &mSFfocusZtables[tableInd].stageZ[0];
  float *absvals = &mSFfocusZtables[tableInd].absFocus[0];
  int i;

  // Find index of first value after the focus but restrict search to allow extrapolation
  for (i = 1; i < mSFfocusZtables[tableInd].numPoints - 1; i++)
    if (absvals[i] > absFocus)
      break;
  return (zvals[i-1] + (absFocus - absvals[i-1]) * (zvals[i] - zvals[i-1]) / 
    (absvals[i] - absvals[i-1]));
}

// Find a relative defocus value in the given table for the given Z value
float CFocusManager::LookupDefocusFromZ(int tableInd, float Z)
{
  float *zvals = &mSFfocusZtables[tableInd].stageZ[0];
  float *defvals = &mSFfocusZtables[tableInd].defocus[0];
  int i;

  // Find index of first value after the focus but restrict search to allow extrapolation
  for (i = 1; i < mSFfocusZtables[tableInd].numPoints - 1; i++)
    if (zvals[i] > Z)
      break;
  return (defvals[i-1] + (Z - zvals[i-1]) * (defvals[i] - defvals[i-1]) / 
    (zvals[i] - zvals[i-1]));
}

float CFocusManager::GetSTEMdefocusToDelZ(int spotSize, int probeMode, double absFocus)
{
  int ind, i, imin, imax;
  double diff, minDiff = 10000.;
  if (probeMode < 0)
    probeMode = mScope->GetProbeMode();
  if (!mSFfocusZtables.GetSize())
    return mSTEMdefocusToDelZ[probeMode];
  if (spotSize < 0)
    spotSize = mScope->FastSpotSize();
  ind = FindFocusZTable(spotSize, probeMode);
  if (ind < 0)
    return mSTEMdefocusToDelZ[probeMode];
  if (absFocus < 0)
    absFocus = mScope->GetFocus();

  // Find nearest table entry
  for (i = 0; i < mSFfocusZtables[ind].numPoints; i++) {
    diff = fabs(absFocus - mSFfocusZtables[ind].absFocus[i]);
    if (diff < minDiff) {
      imin = i;
      minDiff = diff;
    }
  }

  // Go back and forth two entries from that and take slope across there
  imax = B3DMIN(mSFfocusZtables[ind].numPoints - 1, imin + 2);
  imin = B3DMAX(0, imin - 2);
  return (mSFfocusZtables[ind].stageZ[imax] - mSFfocusZtables[ind].stageZ[imin]) /
    (mSFfocusZtables[ind].defocus[imax] - mSFfocusZtables[ind].defocus[imin]);
}

// Compute beam tilt scaling to mrad from focus calibrations
float CFocusManager::EstimatedBeamTiltScaling(void)
{
  float pixel, scaleSum = 0.;
  float numScales = 0;
  int ind, loop, camera, alpha = mScope->GetAlpha();

  // If alpha exists, first look for ones with matching alpha, then take anything if that
  // gave nothing
  for (loop = 0; loop < mScope->GetHasNoAlpha() ? 1 : 2; loop++) {
    for (ind = 0; ind < mFocTab.GetSize(); ind++) {
      camera = mFocTab[ind].camera;
      if (mWinApp->LookupActiveCamera(camera) >= 0 && 
        mFocTab[ind].probeMode == mScope->GetProbeMode() && 
        (loop || alpha < 0 || alpha == mFocTab[ind].alpha)) {
          pixel = mWinApp->mShiftManager->GetPixelSize(camera, mFocTab[ind].magInd);
          scaleSum += (float)sqrt((double)mFocTab[ind].slopeX * mFocTab[ind].slopeX
            + mFocTab[ind].slopeY * mFocTab[ind].slopeY) * 1000.f * pixel / 2.f;
          numScales++;
      }
    }
    if (numScales)
      break;
  }
  return scaleSum / (float)B3DMAX(1, numScales);
}
