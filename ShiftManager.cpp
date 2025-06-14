// ShiftManager.cpp:      Does autoalignment, mouse moving, reset image shift, and has
//                          all the routines for getting coordinate 
//                          transformation matrices
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
#include "MainFrm.h"
#include ".\ShiftManager.h"
#include "ShiftCalibrator.h"
#include "FocusManager.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "Utilities\XCorr.h"
#include "EMscope.h"
#include "MacroProcessor.h"
#include "LogWindow.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "TSController.h"
#include "EMbufferManager.h"
#include "ComplexTasks.h"
#include "MultigridTasks.h"
#include "ParticleTasks.h"
#include "MultiTSTasks.h"
#include "NavHelper.h"
#include "BeamAssessor.h"
#include "ProcessImage.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"
#include "Shared\\cfft.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

static void AlignRightDblClickImage();


#pragma warning ( disable : 4244 )

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CShiftManager::CShiftManager()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mConSets = mWinApp->GetConSets();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mImBufs = mWinApp->GetImBufs();
  mActiveCameraList = mWinApp->GetActiveCameraList();
  mScope = NULL;
  mRegularShiftLimit = 15.0;
  mLowMagShiftLimit = 150.0;
  mRoughISscale = 1.0;
  mLMRoughISscale = 0.0;
  mSTEMRoughISscale = 0.0;
  mTrimDarkBorders = false;
  mErasePeriodicPeaks = false;
  mNoDefaultPeakErasing = 2;
  mDisableAutoTrim = 0;
  SetPeaksToEvaluate(0, 0.);
  mTrimFrac = 0.04f;       // Fraction to trim for correlations
  mTaperFrac = 0.1f;     // Fraction to taper, applied separately to X and Y
  mSigma1 = 0.03f;       // Sigma1 for the filtering
  mSigma2 = 0.05f;
  mRadius2 = 0.25f;
  mMouseShifting = false;
  mMouseEnding = false;
  mShiftingDefineArea = false;
  mNumISdelays = 2;
  mISmoved[0] = 0.;
  mISdelayNeeded[0] = 0.;
  mISmoved[1] = 1.;
  mISdelayNeeded[1] = 1.;
  mDelayPerMagDoubling = 0.25;
  mISdelayScaleFactor = 1.;
  mStartupForISDelays = 0.8f;
  mISTimeOut = GetTickCount();
  mDefocusZFactor = 1.;
  mNormalizationDelay = 2000;
  mLowMagNormDelay = 0;
  mStageMovedDelay = 2000;
  mStageDelayToUse = 2000;
  mResettingIS = false;
  mStartedStageMove = false;
  mTiltDelay = 2.;
  mNumBeamShiftCals = 0;
  mMouseMoveStage = false;
  mShiftPressed = false;
  mMouseStageThresh = 0.7f;
  mMouseStageAbsThresh = 3.0f;
  mInvertStageXAxis = false;
  mNumRelRotations = 0;
  mMinTiltDelay = 200;
  mRotXforms.SetSize(0, 4);
  mStageStretchXform.xpx = 0.;
  mTmplImage = NULL;
  mNextAutoalignLimit = -1.;
  mLastTimeoutWasIS = false;
  mBacklashMouseAndISR = false;
  mStageInvertsZAxis = -1;
  mHitachiStageSpecAngle = -90.;
  mLastFocusForMagCal = 999.;
  mLastAlignXTrimA = mLastAlignXTrimRef = mLastAlignYTrimA = mLastAlignYTrimRef = 0;
  for (int i = 0; i < 5; i++)
    mInterSetShifts.binning[i] = 0;
  mUseSquareShiftLimits = -1;
  mC2SpacingForHighFocus = 0.2f;
  mAcquireWhenShiftDone = -1;
  mRDCthreshFor2ndShot = 0.25f;
}

CShiftManager::~CShiftManager()
{

}

// Further initialization of program component addresses
void CShiftManager::Initialize()
{
  float oldDefaults[14] = {0.1f, 0.f, 0.3f, 0.4f, 0.8f, 0.9f, 1.6f, 1.7f, 3.f, 3.f,
    3.5f, 3.5f, 4.f, 3.7f};
  float newDefaults[8] = {0.1f, 0.f, 0.3f, 0.4f, 0.5f, 0.5f, 1.0f, 0.5f};
  bool stillOldDflt = false, stillNewDflt = false;
  int ind;

  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  if (!mSTEMRoughISscale)
    mSTEMRoughISscale = mRoughISscale;

  // Determine if IS delays are still the old default
  if (fabs(mISdelayScaleFactor - 1.) < 1.e-3) {
    if (mNumISdelays == 7) {
      stillOldDflt = true;
      for (ind = 0; ind < 7; ind++) {
        if (fabs(oldDefaults[2 * ind] - mISmoved[ind]) > 1.e-3 ||
          fabs(oldDefaults[2 * ind + 1] - mISdelayNeeded[ind]) > 1.e-3) {
          stillOldDflt = false;
          break;
        }
      }
    }
    
    // IF not, see if they are the new default
    if (mNumISdelays == 4 && !stillOldDflt) {
      stillNewDflt = true;
      for (ind = 0; ind < 4; ind++) {
        if (fabs(newDefaults[2 * ind] - mISmoved[ind]) > 1.e-3 ||
          fabs(newDefaults[2 * ind + 1] - mISdelayNeeded[ind]) > 1.e-3) {
          stillNewDflt = false;
          break;
        }
      }
    }
  }

  // Identifiably modern scopes with old defaults: make them the new ones, adjust mag
  // dependence
  if (stillOldDflt && (mScope->GetUseIllumAreaForC2() ||
    mScope->GetAdvancedScriptVersion() > 0)) {
    mNumISdelays = 4;
    for (ind = 0; ind < 4; ind++) {
      mISmoved[ind] = newDefaults[2 * ind];
      mISdelayNeeded[ind] = newDefaults[2 * ind + 1];
    }
    ACCUM_MIN(mDelayPerMagDoubling, 0.1f);
    SEMTrace('1', "Image shift delays were at old defaults and are being set to new "
      "recommended values");
    mStartupForISDelays = 0.25f;
  }

  // Scopes with the new default: adjust mag doubling
  if (stillNewDflt && FEIscope) {
    if (mDelayPerMagDoubling > 0.1f)
      SEMTrace('1', "Image shift delay table has new recommended values, "
        "reducing mag dependency factor");
    ACCUM_MIN(mDelayPerMagDoubling, 0.1f);
    mStartupForISDelays = 0.25f;
  }

  // JEOL scopes still need a distance dependent delay but lower scaling
  if (stillOldDflt && JEOLscope) {
    mISdelayScaleFactor = 0.2f;
    SEMTrace('1', "Image shift delays were at old defaults; ISdelayScaleFactor is being"
      " set to new recommended value, %.2f", mISdelayScaleFactor);
    mStartupForISDelays = 0.25f;
  }

  if (mUseSquareShiftLimits < 0) {
    mUseSquareShiftLimits = 0;
    if (!FEIscope || mScope->GetUseIllumAreaForC2() ||
      mScope->GetAdvancedScriptVersion() > 0)
      mUseSquareShiftLimits = 1;
  }
}

// Sets the max # of peaks and minimum peak strength to retain and evaluate by correlation
// Or resets the default defined here, if an argument is 0
void CShiftManager::SetPeaksToEvaluate(int maxPeaks, float minStrength)
{
  if (maxPeaks > 0)
    mMaxNumPeaksToEval = maxPeaks;
  else
    mMaxNumPeaksToEval = 10;
  if (minStrength > 0.)
    mPeakStrengthToEval = minStrength;
  else 
    mPeakStrengthToEval = 0.33f;
}

//////////////////////////////////////////////////////////////////////
// Autoalignment and image alignment
//////////////////////////////////////////////////////////////////////

#define SHIFT_DELAY_FACTOR 0.75f

// Set the shift of an image, and possibly change the image shift as well
int CShiftManager::SetAlignShifts(float newX, float newY, BOOL incremental,
                  EMimageBuffer *imBuf, BOOL doImShift, BOOL imposeOnImage)
{
  float oldX, oldY, delX, delY, defocus = 0.;
  double delISX, delISY, fracX, fracY, angle, intensity = 0.5, xTiltFac, yTiltFac;
  double fieldFrac = 0.;
  double absMove = 0.;
  int limitX, limitY, magInd = 17, camera, spot = 3;
  ScaleMat bMat, bInv;
  int binning;
  MontParam *montParam = mWinApp->GetMontParam();
  int alignBuf = mBufferManager->AutoalignBasicIndex();
  EMimageBuffer *activeImBuf = mWinApp->mActiveView->GetActiveImBuf();

  int shiftedBuf = mBufferManager->MainImBufIndex(imBuf);
  BOOL montOverview = shiftedBuf == 1  && imBuf->IsMontageOverview() &&
    (mWinApp->Montaging() || imBuf->mCaptured == BUFFER_PRESCAN_OVERVIEW);
  BOOL imposeShift = imposeOnImage;

  if (!imBuf->mImage)
    return 1;

  imBuf->mImage->getShifts(oldX, oldY);

  // Get the new shift if incremental; if it is beyond image range, just return
  // silently
  if (incremental) {
    newX += oldX;
    newY += oldY;
    imBuf->mImage->getSize(limitX, limitY);
    limitX -= 4;
    limitY -= 4;
    if (newX < -limitX || newX > limitX || newY < -limitY || newY > limitY)
      return 2;
  }

  // reject the change unless it is for buffer A, montage 
  // overview, the autoalign buffer, or a reset  to 0,0., or an anchor buffer
  if (!(!shiftedBuf || (shiftedBuf == alignBuf)
    || (mWinApp->LowDoseMode() && shiftedBuf == alignBuf + 1) || 
    (newX == 0. && newY == 0.) || montOverview || imBuf->mCaptured == BUFFER_ANCHOR))
    return 1;

  // Apply a change in scope image shift if this is the A buffer or overview
  // of montage with user shift.  Do not apply for processed buffer
  if (doImShift && (!shiftedBuf || (montOverview && !incremental)) && 
    imBuf->mCaptured != BUFFER_PROCESSED) { 
    if (!mShiftPressed)
      mShiftingDefineArea = mWinApp->mLowDoseDlg.ImageAlignmentChange(newX, newY, oldX,
        oldY, mMouseShifting);
    magInd = imBuf->mMagInd ? imBuf->mMagInd : mScope->GetMagIndex();
    binning = imBuf->mBinning;

    // Invert Y at this point; and invert the whole expression; get unbinned pixels
    delX = binning * (newX - (mMouseShifting ? mMouseStartX : oldX));
    delY = binning * ((mMouseShifting ? mMouseStartY : oldY) - newY);

    // Now compute fraction of field shifted if necessary
    camera = mImBufs->mCamera >= 0 ? mImBufs->mCamera : mWinApp->GetCurrentCamera();
    if ((mMouseShifting || mMouseEnding) && !mShiftPressed && mMouseMoveStage) {
      fracX = delX / mCamParams[camera].sizeX;
      fracY = delY / mCamParams[camera].sizeY;
      fieldFrac = sqrt(fracX * fracX + fracY * fracY);
      absMove = sqrt(delX * delX + delY * delY) * GetPixelSize(camera, magInd);
    }

    // If conditions for stage move are not met, continue with image shift logic
    if (!((mMouseShifting || mMouseEnding) && (mShiftPressed || 
         (mMouseMoveStage && (fieldFrac >= mMouseStageThresh || 
           absMove >= mMouseStageAbsThresh)) ||
         (montOverview && montParam->moveStage)))) {
      imposeShift = ImposeImageShiftOnScope(imBuf, delX, delY, magInd, camera, 
        incremental, mMouseShifting) && imposeOnImage;

      // Otherwise, proceed with stage move if at end of shift
    } else if (mMouseEnding && (delX || delY)) {
      if (mScope->StageBusy() > 0) {
        AfxMessageBox(_T("Stage is busy - shift cancelled"));
        imposeShift = false;
        // return 3;
      }

      // Get the stage movement based on nearest calibration, negative needed here
      // as for image shift
      if (imBuf->mLowDoseArea && IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed) && 
        magInd >= mScope->GetLowestNonLMmag() &&
        imBuf->GetSpotSize(spot) && imBuf->GetIntensity(intensity))
        defocus = imBuf->mViewDefocus;
      bMat = FocusAdjustedStageToCamera(camera, magInd, spot, imBuf->mProbeMode,
        intensity, defocus);
      bInv = MatInv(bMat);
      delISX = -(bInv.xpx * delX + bInv.xpy * delY);
      delISY = -(bInv.ypx * delX + bInv.ypy * delY);
      angle = DTOR * mScope->GetTiltAngle();
      xTiltFac = HitachiScope ? cos(angle) : 1.;
      yTiltFac = HitachiScope ? 1. : cos(angle);

      // If transformation exists, try to zero image shift too
      AdjustStageMoveAndClearIS(camera, magInd, delISX, delISY, bInv);
      
      // Get the stage position and change it.  Set a flag and update state
      // to prevent early pictures, use reset shift task handlers
      mScope->GetStagePosition(mMoveInfo.x, mMoveInfo.y, mMoveInfo.z);
      MaintainOrImposeBacklash(&mMoveInfo, delISX / xTiltFac, delISY / yTiltFac,
        mBacklashMouseAndISR && fabs(angle / DTOR) < 10.);
      mMoveInfo.axisBits = axisXY;
      mStartedStageMove = true;
      imBuf->mStageShiftedByMouse = true;
      mWinApp->UpdateBufferWindows();
      mScope->MoveStage(mMoveInfo, mMoveInfo.backX != 0. || mMoveInfo.backY != 0.);
      mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_RESET_SHIFT, 0, 30000);
    }
  }

  // Now make the change in displayed image shift
  if (imposeShift) {
    imBuf->mImage->setShifts(newX, newY);
    mWinApp->mLowDoseDlg.EnableSetViewShiftIfOK();
  }
  imBuf->SetImageChanged(1);
  if (shiftedBuf < 0)
    mWinApp->mActiveView->DrawImage();
  else
    mWinApp->mMainView->DrawImage();
  return imposeShift ? 0 : 3;
}

// If transformation exists, zero image shift and adjust stage move to compensate
void CShiftManager::AdjustStageMoveAndClearIS(int camera, int magInd, double &delStageX, 
  double &delStageY, ScaleMat bInv)
{
  double fracX, fracY, roughScale;
  ScaleMat bMat, aMat;
  aMat = IStoGivenCamera(magInd, camera);
  if (aMat.xpx) {

    if (mWinApp->GetSTEMMode())
      roughScale = mSTEMRoughISscale;
    else
      roughScale = (magInd < mScope->GetLowestNonLMmag(&mCamParams[camera]) && 
      mLMRoughISscale) ? mLMRoughISscale : mRoughISscale;
    mScope->GetLDCenteredShift(fracX, fracY);
    SEMTrace('i', "Base stage shift of %.2f %.2f with IS of %.2f %.2f",
      delStageX, delStageY, fracX, fracY);
    if (sqrt(fracX * fracX + fracY * fracY) * roughScale > 0.01) {
      bMat = MatMul(aMat, bInv);
      delStageX += bMat.xpx * fracX + bMat.xpy * fracY;
      delStageY += bMat.ypx * fracX + bMat.ypy * fracY;
      SEMTrace('i', "Final stage shift of %.2f %.2f", delStageX, delStageY);
      mScope->IncImageShift(-fracX, -fracY);
      SetISTimeOut(GetLastISDelay());
    }
  }
}


// Convert the image shift (already in right-handed coordinates) to a scope image shift
// and impose it if possible
bool CShiftManager::ImposeImageShiftOnScope(EMimageBuffer *imBuf, float delX, float delY,
  int magInd, int camera, BOOL incremental, BOOL mouseShifting)
{
  double delISX, delISY, bufISX, bufISY;
  ScaleMat bMat, bInv;
  CString str;
  int curMag;
  bMat = FocusAdjustedISToCamera(imBuf);
  if (!bMat.xpx)
    bMat = IStoGivenCamera(magInd, camera);
  if (!bMat.xpx) {
    if (!incremental) {
      SEMMessageBox(_T("There is no image shift calibration available\n"
        " for this mag range so images cannot be shifted"));
      return false;
    }
  } else {

    bInv = MatInv(bMat);
    bufISX = -(bInv.xpx * delX + bInv.xpy * delY);
    bufISY = -(bInv.ypx * delX + bInv.ypy * delY);
    curMag = mScope->FastMagIndex();
    TransferGeneralIS(magInd, bufISX, bufISY, curMag, delISX, delISY);
    if (!ImageShiftIsOK(delISX, delISY, true)) {
      // for now, give a message unless this is being done by mouse
      if (!incremental) {
        str.Format(_T("Cannot apply scope image shift of %.4g %.4g units (for shift in "
          "image of %.1f %.1f at mag %d):\nit would go beyond the end of its allowed "
          "range"), delISX, delISY, delX, delY, magInd);
        SEMMessageBox(str);
        return false;
      }
    } else if (!mouseShifting) {

      // Make the change in image shift
      mScope->IncImageShift(delISX, delISY);
      SetISTimeOut(SHIFT_DELAY_FACTOR * GetLastISDelay());
    }
  }

  return true;
}

// When mouse button goes down, set flag and record current shift
void CShiftManager::StartMouseShifting(BOOL shiftPressed, int index)
{
  if (!mImBufs->mImage)
    return;
  mMouseShifting = true;
  mShiftingDefineArea = false;
  mShiftPressed = shiftPressed;
  mImBufs[index].mImage->getShifts(mMouseStartX, mMouseStartY);
}

// When it goes up, make sure flag is set, then get current shift, restore
// starting shift, and call for the move
void CShiftManager::EndMouseShifting(int index)
{
  float newX, newY;
  if (!mMouseShifting)
    return;
  mImBufs[index].mImage->getShifts(newX, newY);
  mImBufs[index].mImage->setShifts(mMouseStartX, mMouseStartY);
  mMouseShifting = false;
  mShiftingDefineArea = false;
  mMouseEnding = true;
  SetAlignShifts(newX, newY, false, &mImBufs[index]);
  mMouseEnding = false;
}

// Call SetAlignShifts with the shift that brings marker to center; set ShiftPressed
// to force stage move
void CShiftManager::AlignmentShiftToMarker(BOOL forceStage)
{
  EMimageBuffer *imBuf = mWinApp->mMainView->GetActiveImBuf();
  if (!(imBuf->mHasUserPt || imBuf->mIllegalUserPt) || !imBuf->mImage || 
    (mWinApp->DoingTasks() && !mWinApp->GetJustNavAcquireOpen()))
    return;
  mMouseEnding = true;
  mShiftPressed = forceStage;
  SetAlignShifts(-(float)(imBuf->mUserPtX - imBuf->mImage->getWidth() / 2.),
    -(float)(imBuf->mUserPtY - imBuf->mImage->getHeight() / 2.), false, imBuf, true);
  mMouseEnding = false;
  mShiftPressed = false;
}

// Higher-level operation: start shift to clicked position then acquire image there
void CShiftManager::AcquireAtRightDoubleClick(int bufInd, float shiftX, 
  float shiftY, BOOL forceStage)
{
  EMimageBuffer *imBuf = &mImBufs[bufInd];
  mRDCclickedBufInd = bufInd;
  mRDCexpectedXshift = (float)(shiftX - imBuf->mImage->getWidth() / 2.);
  mRDCexpectedYshift = (float)(shiftY - imBuf->mImage->getHeight() / 2.);

  // Set up control set to acquire
  mAcquireWhenShiftDone = imBuf->mConSetUsed;
  if (mAcquireWhenShiftDone == RECORD_CONSET && mWinApp->LowDoseMode())
    mAcquireWhenShiftDone = PREVIEW_CONSET;
  mMouseEnding = true;
  mShiftPressed = forceStage;
  SetAlignShifts(-mRDCexpectedXshift, -mRDCexpectedYshift, false, imBuf, true, false);
  mMouseEnding = false;
  mShiftPressed = false;
  mWinApp->SetStatusText(MEDIUM_PANE, "CENTERING CLICKED POINT");

  // If it was not a stage move, do the shot now
  if (!mScope->StageBusy()) {
    mCamera->InitiateCapture(mAcquireWhenShiftDone);
    mAcquireWhenShiftDone = -1 - mAcquireWhenShiftDone;
    mRDCclickedBufInd = -1;
    mCamera->SetNewImageCallback(AlignRightDblClickImage);
  }
}

// Align image if there was a stage move
void CShiftManager::DoAlignDblClickImage()
{
  EMimageBuffer *aliBuf = &mImBufs[mRDCclickedBufInd + 1];
  float xx, yy;
  int nx, ny;
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  if (mRDCclickedBufInd < 0)
    return;

  // The shift is already defined relative to real image area, so it is used directly in
  // this test for being within image
  if (mBufferManager->GetShiftsOnAcquire() < (mRDCclickedBufInd == 1 ? 2 : 1) ||
    !aliBuf->mImage && !aliBuf->mBinning || (aliBuf->mBinning < mImBufs->mBinning &&
    (mImBufs->mBinning % aliBuf->mBinning) != 0) ||
      (aliBuf->mBinning > mImBufs->mBinning && 
    (aliBuf->mBinning % mImBufs->mBinning) != 0) ||
    fabs(mRDCexpectedXshift) > aliBuf->mImage->getWidth() / 2. ||
    fabs(mRDCexpectedYshift) > aliBuf->mImage->getHeight() / 2.)
    return;

  // But the expected shift for autoalign has to include any existing shift
  aliBuf->mImage->getShifts(xx, yy);
  mRDCexpectedXshift = (float)(((mRDCexpectedXshift + xx) * aliBuf->mBinning) /
    mImBufs->mBinning);
  mRDCexpectedYshift = (float)(((mRDCexpectedYshift + yy) * aliBuf->mBinning) /
    mImBufs->mBinning);
  AutoAlign(mRDCclickedBufInd + 1, 0, 1, 0, NULL, mRDCexpectedXshift, mRDCexpectedYshift);
  aliBuf->mImage->setShifts(xx, yy);
  if (mRDCthreshFor2ndShot <= 0.)
    return;
  aliBuf->mImage->getSize(nx, ny);
  if (nx * ny - (nx - fabs(xx)) * (ny - fabs(yy)) < mRDCthreshFor2ndShot * nx * ny)
    return;
  for (nx = 0; nx < mBufferManager->GetShiftsOnAcquire(); nx++)
    mBufferManager->CopyImageBuffer(nx + 1, nx, false);
  mCamera->InitiateCapture(-mAcquireWhenShiftDone - 1);
}

// Callback function
void AlignRightDblClickImage()
{
  ((CSerialEMApp *)AfxGetApp())->mShiftManager->DoAlignDblClickImage();
}


// Align the image in A against the image in the autoalign buffer or the buffer indicated
// by bufIndex > 0, or to an autocorrelation if bufIndex is -1
// inSmallPad can be 1 for padding by only a small fraction, 0 for default padding by
//            by 0.45, or -1 for full padding by 0.9; a value > 1 indicates template corr
// doImShift (default true) controls whether it does scope image shift
// corrFlags (default 0) can be set 1 to put cross-correlation in A plus 2 to try to 
// eliminate peaks in FFT from periodic structures
// peakVal is returned with the raw correlation peak value if non-NULL 
// expectedXshift, expectedYshift are the expected shifts so that correlation is over the
//                                region of overlap (default 0,0)
// conical non-zero makes it do a conical tilt alignment with the given rotation
// scaling non-zero makes it apply that scaling to A before correlating
// rotation non-zero makes it apply that rotation to A before correlating
// CCC is returned with the correlation coefficient if non-null
// fracPix is returned with the fractional overlap at the chosen peak if non-null
// trimOutput (default true) controls whether the image trimming is reported
// if xShiftOut and yShiftOut are non-NULL, the shift is returned in them without imposing
// on the image or scope or accounting for shift in C
// If probSigma is non-zero, CCC values will also be weighted by a gaussian that depends
// on the image shift of the peak as fraction of area extent, with this sigma
int CShiftManager::AutoAlign(int bufIndex, int inSmallPad, BOOL doImShift, int corrFlags,
                             float *peakVal, float expectedXshift, float expectedYshift,
                             float conical, float scaling, float rotation, float *CCC,
                             float *fracPix, BOOL trimOutput, float *xShiftOut, 
                             float *yShiftOut, float probSigma)
{
  BOOL doTrim, swapStretch;
  int nxUse, nxPad, nyUse, nyPad;
  int needBinA, needBinC;
  int nxTaper, nyTaper, nyTrimCtop;
  int nxTrimA, nyTrimA, nxTrimC, nyTrimC, nxTrimAright, nyTrimAtop, nxTrimCright;
  int targetSize = 512;
  int trimTestSize = 10;
  int trimDelta = 5;
  float trimCenFrac = 0.2f;
  float trimPassRatio = 0.3f;
  float oversizeFrac = 0.1f;
  float freqScale = 1., hiFreqScale = 1.;
  bool centered = true;
  int numPeaks = mMaxNumPeaksToEval;
  int xInd, yInd;
  int iPeak, indMaxPeak, numRealPeaks; 
  float fracPad = 0.45f;    // 11/2/10: Make this "half-padded"
  float trimFrac = mTrimFrac;
  if (mWinApp->mNavHelper->GetRealigning() || mWinApp->mShiftCalibrator->CalibratingIS())
    trimFrac = 0.04f;
  bool tmplCorr = inSmallPad > 1;
  bool transformedA = false;
  bool showCor = (corrFlags & AUTOALIGN_SHOW_CORR) != 0;
  bool fillSpots = (corrFlags & AUTOALIGN_FILL_SPOTS) != 0 || (mErasePeriodicPeaks &&
    !(corrFlags & AUTOALIGN_KEEP_SPOTS));
  BOOL debugTime = GetDebugOutput('T');
  float boostNumSpots = (corrFlags & AUTOALIGN_MORE_SPOTS) ? 2.6f : 0.;
  if (inSmallPad > 0)
    fracPad = 0.05f;
  else if (inSmallPad < 0)
    fracPad = 0.9f;        // And make this really fully padded (maybe thought 0.45 was)
  float delta, frac;
  int toBuf = bufIndex;
  float stretch;
  int typeA, typeC;
  int heightA, widthA, heightC, widthC, nxUseA, nxUseC, nyUseA, nyUseC;
  int extra, baseXshift, ix0A, ix1A, ix0C, ix1C, baseYshift, iy0A, iy1A, iy0C, iy1C;
  //int height, width;
  //int minWidth, minHeight;
  int binA, binC, peakXoffset, peakYoffset, retval = 0;
  float cshiftX, cshiftY;
  void *tempA, *tempC;
  //short int *crray;
  float xPeak[2], yPeak[2];
  float *Xpeaks, *Ypeaks, *peak;
  float tiltA = 0., tiltC = 0., tiltAngles[2] = {0., 0.}, axisAngle = 0.;
  float *fillArray = NULL, *fillBrray = NULL, *fillCrray = NULL;
  bool failed = true;
  int commonBin, size, sizeC, maxBin;
  float stretchAxis, alignLimit = -1.;
  ScaleMat str, strInv;
  float tempX, tempY;
  int numPixel, minPixel;
  float CCCmax, fracMax, wgtCCCmax;
  double singam, cosgam;
  float CCChere, fracHere, CCCbest, fracBest, wgtCCC, bestWgtCCC, distsq, probFac;
  float *CCCptr = CCC, *fracPtr = fracPix;
  double overlapPow = 0.166667;
  BOOL autoCorr = bufIndex < 0;
  bool disableLDtrim = (mDisableAutoTrim & NOTRIM_LOWDOSE_ALL) || 
    ((mDisableAutoTrim & NOTRIM_LOWDOSE_TS) && mWinApp->DoingTiltSeries());
  bool disableTaskTrim = (mDisableAutoTrim & NOTRIM_TASKS_ALL) || 
    ((mDisableAutoTrim & NOTRIM_TASKS_TS) && mWinApp->DoingTiltSeries());
  bool disableItemTrim = (mDisableAutoTrim & NOTRIM_REALIGN_ITEM) != 0;
  double startTime, time1, time2, time3, time4, time5;

  // Set up pointers and flags for cleanup on error or completion
  mArray = NULL;
  mBrray = NULL;
  mCrray = NULL;
  mDeleteA = false;
  mDeleteC = false;
  mPeakHere = NULL;
  mXpeaksHere = NULL;
  mYpeaksHere = NULL;

  // Set the align limit in pre-binning pixels and use up the set value before any returns
  // Shifts are limited in simulation mode!
  if (mNextAutoalignLimit >= 0.) {
    tempX = GetPixelSize(mImBufs);
    if (tempX > 0)
      alignLimit = mNextAutoalignLimit / tempX;
    mNextAutoalignLimit = -1.;
  }

  // Assign the supplied arrays (one way or another) or create arrays for peak values
  if (tmplCorr) {
    Xpeaks = mTmplXpeaks;
    Ypeaks = mTmplYpeaks;
    peak = mTmplPeak;
    if (!peak || !Xpeaks || !Ypeaks)
      return 1;
  } else {
    NewArray(mPeakHere, float, numPeaks);
    NewArray(mXpeaksHere, float, numPeaks);
    NewArray(mYpeaksHere, float, numPeaks);
    if (MemoryError(mPeakHere == NULL || mXpeaksHere == NULL || mYpeaksHere == NULL))
      return 1;
    Xpeaks = mXpeaksHere;
    Ypeaks = mYpeaksHere;
    peak = mPeakHere;
  }

  if (debugTime)
    startTime = wallTime() * 1000.;

  if (!CCCptr)
    CCCptr = &CCChere;
  if (!fracPtr)
    fracPtr = &fracHere;

  // Align to the given buffer, or to the autoalign buffer if none
  if (!toBuf)
    toBuf = mBufferManager->AutoalignBufferIndex();
  if (autoCorr) {
    toBuf = 0;
    targetSize = 2048; //??
  }
  if (fillSpots) {
    targetSize = 768;
    freqScale = 0.75f;
  }

  if (scaling == 1.)
    scaling = 0.;
  if (tmplCorr && mTmplImage) {
    mImA = mTmplImage;
    binA = mTmplBinning;
  } else {
    mImA = mImBufs[0].mImage;
    binA = mImBufs[0].mBinning;
  }
  mImC = mImBufs[toBuf].mImage;
  binC = mImBufs[toBuf].mBinning;
  if (mImA == NULL)
    return 1;
  if (mImC == NULL) {
    mWinApp->mTSController->TSMessageBox(_T("No image to align to in the expected "
      "buffer"));
    return 1;
  }
  typeA = mImA->getType();
  typeC = mImC->getType();
  heightA = mImA->getHeight();
  widthA = mImA->getWidth();
  heightC = mImC->getHeight();
  widthC = mImC->getWidth();

  // Look for a common binning starting at the bigger binning
  maxBin = binA > binC ? binA : binC;
  for (commonBin = maxBin; commonBin < 64; commonBin++) {

    // Evaluate only for common multiples of the two binnings
    if (commonBin % binA || commonBin % binC)
      continue;
    needBinA = commonBin / binA;
    needBinC = commonBin / binC;

    // Find the biggest binned-down dimension of each image but use the smaller image
    // as of 3/14/12
    size = B3DMAX(heightA, widthA) / needBinA;
    sizeC = B3DMAX(heightC, widthC) / needBinC;
    size = B3DMIN(size, sizeC);

    // If the images are less than half the target and this is not the 
    // first try, quit
    if (size < targetSize / 2 && commonBin > maxBin) {
      commonBin = 64;
      break;
    }

  // Stop when size reaches the target
    if (size <= targetSize)
      break;
  }

  if (commonBin == 64) {
    mWinApp->mTSController->TSMessageBox(_T("Image binnings cannot be matched up - "
      "cannot autoalign"));
    return 1;
  }
  
  mWinApp->mMainView->SetFocus();
  //app->CallStatusUpdate(4);

  // Images get unlocked by cleanup routine in case of error
  mImA->Lock();
  mDataA = (void *)mImA->getData();
  mImC->Lock();
  mDataC = (void *)mImC->getData();
  
  if (needBinA > 1) {
    NewArray(tempA, short int, (typeA == kFLOAT ? 2 : 1) * (size_t)heightA * widthA /
      (needBinA * needBinA));
    if (MemoryError(tempA == NULL))
      return 1;

    XCorrBinByN(mDataA, typeA, widthA, heightA, needBinA, (short *)tempA);

    // Replace the types, size, and image pointers
    if (typeA == kUBYTE || typeA == kRGB)
      typeA = kSHORT;
    heightA /= needBinA;
    widthA /= needBinA;
    mDataA = (void *)tempA;
    mDeleteA = true;

    // A negative value here signals that image was scaled up by this amount, so it is
    // appropriate to scale the filters by this divided by the binning here 
    if (mImBufs[0].mEffectiveBin < 0)
      hiFreqScale = B3DMIN(1.f, -1.f / (mImBufs[0].mEffectiveBin / (float)needBinA));
  }
  
  if (needBinC > 1) {
    NewArray(tempC, short int, (typeC == kFLOAT ? 2 : 1) * (size_t)heightC * widthC /
      (needBinC * needBinC));
    if (MemoryError(tempC == NULL)) {
      return 1;
    }
    XCorrBinByN(mDataC, typeC, widthC, heightC, needBinC, (short *)tempC);

    // Replace the types, size, and image pointers
    if (typeC == kUBYTE || typeC == kRGB)
      typeC = kSHORT;
    heightC /= needBinC;
    widthC /= needBinC;
    mDataC = (void *)tempC;
    mDeleteC = true;
    if (mImBufs[toBuf].mEffectiveBin < 0)
      hiFreqScale = B3DMIN(hiFreqScale, -1.f / (mImBufs[toBuf].mEffectiveBin / 
      (float)needBinC));
  }

  if (debugTime)
    time1 = wallTime() * 1000.;

  // Get amount to stretch A to align to C;
  stretch = 1.0;
  if (!tmplCorr) {
    if (mImBufs[0].GetTiltAngle(tiltA) && mImBufs[toBuf].GetTiltAngle(tiltC)) {
      stretch = cos(DTOR * tiltC) / cos(DTOR * tiltA);
    } else if (conical) {
      AfxMessageBox(_T("Tilt angle must be available for\nboth images to do "
        "conical tilt alignment"), MB_EXCLAME);
      return 1;
    }
    if (!mImBufs[0].GetAxisAngle(stretchAxis))
      stretchAxis = (float)GetImageRotation(mWinApp->GetCurrentCamera(),
      mScope->GetMagIndex());
    stretchAxis -= 90.;
  }
  if (mImBufs[0].GetAxisAngle(axisAngle)) {
    tiltAngles[0] = tiltC;
    tiltAngles[1] = tiltA;
  }

  // Initialize str in case of no stretch
  str.xpx = str.ypy = 1.;
  str.xpy = str.ypx = 0.;
  if (scaling || rotation)
    stretch = 1.;
  swapStretch = stretch < 1.0 || conical != 0 || (scaling && scaling < 1.);

  // Set up transform for conical tilt
  if (conical) {
    stretch = 1./cos(DTOR * tiltC);
    cosgam = cos(DTOR * stretchAxis);
    singam = sin(DTOR * stretchAxis);
    str.xpx = cosgam * stretch;
    str.xpy = singam * stretch;
    str.ypx = -singam;
    str.ypy = cosgam;
    strInv.xpx = cos(DTOR * conical);
    strInv.ypx = sin(DTOR * conical);
    strInv.xpy = -strInv.ypx;
    strInv.ypy = strInv.xpx;
    strInv = MatMul(str, strInv);
    stretch = cos(DTOR * tiltA);
    str.xpx = cosgam * stretch;
    str.xpy = -singam;
    str.ypx = singam * stretch;
    str.ypy = cosgam;
    str = MatMul(strInv, str);
  }

  // Now stretch one of the images
  if (scaling > 1. || (stretch > 1.0 && !conical) || (rotation != 0. && !scaling)) {
    // If > 1, stretch A, delete the binned data if it was created
    if (typeA == kUBYTE) {
      NewArray2(tempA, unsigned char, heightA, widthA);
    } else if (typeA == kRGB) {
      NewArray2(tempA, unsigned char, 3 * heightA, widthA);
    } else {
      NewArray2(tempA, short int, (typeA == kFLOAT ? 2 : 1) * heightA, widthA);
    }
    if (MemoryError(tempA == NULL)) {
      return 1;
    }
    if (scaling > 1. || rotation != 0.) {
      if (scaling && !rotation) {
        str.xpx = scaling;
        str.ypy = scaling;
      } else {
        str = StretchCorrectedRotation(mImBufs->mCamera, mImBufs->mMagInd, rotation);
        if (scaling > 1.) {
          str.xpx *= scaling;
          str.xpy *= scaling;
          str.ypx *= scaling;
          str.ypy *= scaling;
        }
      }
      XCorrFastInterp(mDataA, typeA, tempA, widthA, heightA, widthA, heightA, str.xpx,
        str.xpy, str.ypx, str.ypy, widthA / 2.f, heightA / 2.f, 0.f, 0.f);
    } else
      XCorrStretch(mDataA, typeA, widthA, heightA, stretch, stretchAxis,
            tempA, &str.xpx, &str.xpy, &str.ypx, &str.ypy);
    transformedA = true;
    if (mDeleteA)
      delete [] mDataA;
    mDataA = tempA;
    //if (typeA == kUBYTE)
      //typeA = kSHORT;
    mDeleteA = true;
  } else if (swapStretch) {
    // Otherwise, need to invert value, set flag, stretch C, and delete if binned
    if (typeC == kUBYTE) {
      NewArray2(tempC, unsigned char, heightC, widthC);
    } else if (typeC == kRGB) {
      NewArray2(tempC, unsigned char, 3 * heightC, widthC);
    } else {
      NewArray2(tempC, short int, (typeC == kFLOAT ? 2 : 1) * heightC, widthC);
    }
    if (MemoryError(tempC == NULL)) {
      return 1;
    }
    if (scaling && !rotation) {
      str.xpx = 1. / scaling;
      str.ypy = 1. / scaling;
    } else if (rotation) {
      str = StretchCorrectedRotation(mImBufs->mCamera, mImBufs->mMagInd, -rotation);
      str.xpx /= scaling;
      str.xpy /= scaling;
      str.ypx /= scaling;
      str.ypy /= scaling;
    }
    if (conical || scaling) {
      XCorrFastInterp(mDataC, typeC, tempC, widthC, heightC, widthC, heightC, str.xpx,
        str.xpy, str.ypx, str.ypy, widthC / 2.f, heightC / 2.f, 0.f, 0.f);
    } else {
      stretch = 1.f / stretch;
      XCorrStretch(mDataC, typeC, widthC, heightC, stretch, stretchAxis,
            tempC, &str.xpx, &str.xpy, &str.ypx, &str.ypy);
    }
    if (mDeleteC)
      delete [] mDataC;
    mDataC = tempC;
    //if (typeC == kUBYTE)
      //typeC = kSHORT;
    mDeleteC = true;

    
  }
  strInv = MatInv(str);

  // Temporary replacement of imC:
  /*  mImC->UnLock();
    mDeleteC = false;
    mBufferManager->CopyImageBuffer(toBuf, toBuf + 1);
    mBufferManager->ReplaceImage((char *)tempC, typeC, widthC, heightC,
      toBuf, 1, 2);
    mImC = mImBufs[toBuf].mImage;
    mImC->Lock();
  */ 

  // To erase peaks from periodic signals, take the full-sized image with minimal padding
  // and standard tapering
  if (fillSpots) {
    nxUse = B3DMAX(widthA, widthC);
    nyUse = B3DMAX(heightA, heightC);
    nxPad = XCorrNiceFrame((int)(1.05 * nxUse), 2, niceFFTlimit());
    nyPad = XCorrNiceFrame((int)(1.05 * nyUse), 2, niceFFTlimit());
    NewArray2(fillArray, float, nyPad, (nxPad + 2));
    NewArray2(fillBrray, float, nyPad, (nxPad + 2));
    NewArray2(fillCrray, float, nyPad, (nxPad + 2));
    if (fillArray && fillBrray && fillCrray) {
      nxTaper = (int)(mTaperFrac * nxUse);
      nyTaper = (int)(mTaperFrac * nyUse);
      XCorrTaperInPad(mDataA, typeA, widthA, 0, widthA - 1, 0, heightA - 1, fillArray,
        nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);
      XCorrTaperInPad(mDataC, typeC, widthC, 0, widthC - 1, 0, heightC - 1, fillBrray,
        nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);

      // Do the periodic correlation only up to point of erasing.  If it did erase,
      // it returns fillBrray and fillArray erased, otherwise it will just return and
      // we will use original images
      SEMTrace('1', "Erasing peaks if sufficient # found");
      if (XCorrPeriodicCorr(fillBrray, fillArray, fillCrray, nxPad, nyPad, 0., mCTFa,
        tiltAngles, axisAngle, 1, boostNumSpots)) {

        // Repack the images to match the original
        nxTrimA = nxPad / 2 - widthA / 2;
        nyTrimA = nyPad / 2 - heightA / 2;
        repackFloatImage(fillArray, fillArray, nxPad + 2, nxTrimA, nxTrimA + widthA - 1,
          nyTrimA, nyTrimA + heightA - 1);
        nxTrimA = nxPad / 2 - widthC / 2;
        nyTrimA = nyPad / 2 - heightC / 2;
        repackFloatImage(fillBrray, fillBrray, nxPad + 2, nxTrimA, nxTrimA + widthC - 1,
          nyTrimA, nyTrimA + heightC - 1);
        failed = false;
        trimFrac = B3DMAX(trimFrac, 0.1f);
      }
    }
    if (failed) {
      DELETE_ARR(fillArray);
      DELETE_ARR(fillBrray);
      fillSpots = false;
    }
    DELETE_ARR(fillCrray);
  }

  //height = heightA > heightC ? heightA : heightC;
  //width = widthA > widthC ? widthA : widthC;
  //minHeight = heightA < heightC ? heightA : heightC;
  //minWidth = widthA < widthC ? widthA : widthC;
  nxTrimA = nyTrimA = nxTrimC = nyTrimC = (int)(trimFrac * size + 1);
  nxTrimAright = nyTrimAtop = nxTrimCright = nyTrimCtop = nxTrimA;

  // Whether to trim: it is disallowed completely if doing template correlation, 
  // calibrating IS, or script has disabled it
  // Otherwise the choice for realign governs it for realign;
  // the choice for tasks governs it for tasks
  // the choice for low dose governs it in low dose if neither task nor realign
  // Or finally the option governs it
  doTrim = false;
  if (!(tmplCorr || mWinApp->mShiftCalibrator->CalibratingIS() ||
    mWinApp->mParticleTasks->DoingTemplateAlign() == 1 ||
    mWinApp->mMultiGridTasks->GetDoingMultiGrid() ||
    (mWinApp->mMacroProcessor->DoingMacro() && 
    mWinApp->mMacroProcessor->GetDisableAlignTrim()))) {
      if (mWinApp->mNavHelper->GetRealigning() == 1 ||
        mWinApp->mParticleTasks->DoingTemplateAlign() > 1)
        doTrim = !disableItemTrim;
      else if (mWinApp->mComplexTasks->InLowerMag()) 
        doTrim = !disableTaskTrim;
      else if (mWinApp->LowDoseMode())
        doTrim = !disableLDtrim;
      else
        doTrim = mTrimDarkBorders;
  }

  // If rotating, find the trimming needed for rotated image
  if (rotation != 0.) {
    if (fabs(fabs(rotation) - 90.) < 0.05f) {

      // 90 degree rotation: trim the long dimension
      if (widthA > heightA)
        nxTrimA += (widthA - heightA) / 2;
      else
        nyTrimA += (heightA - widthA) / 2;
    } else if (fabs(rotation) > 0.05f && fabs(fabs(rotation) - 180.) > 0.05f) {

      // Other rotations: get the rotated corners
      double xcorn[5], ycorn[5], maxArea, xx, yy, minx, xtmp;
      int j, bestx, besty, ytrim;
      xcorn[0] = xcorn[1] = xcorn[4] = widthA / 2.;
      xcorn[2] = xcorn[3] = - widthA / 2.;
      ycorn[0] = ycorn[3] = ycorn[4] = -heightA / 2.;
      ycorn[1] = ycorn[2] = heightA / 2.;
      for (j = 0; j < 5; j++) {
        xtmp = str.xpx * xcorn[j] + str.xpy * ycorn[j];
        ycorn[j] = str.ypx * xcorn[j] + str.ypy * ycorn[j];
        xcorn[j] = xtmp;
      }

      // Find trimming that fits in rotated box and has largest area: for each possible
      // Y trimming, find minimum intersection of upper line with the rotated box
      maxArea = 0.;
      for (ytrim = nyTrimA; ytrim < heightA / 2 - 2; ytrim++) {
        yy = heightA / 2. - ytrim;
        minx = widthA / 2. - nxTrimA;
        for (j = 0; j < 4; j++) {
          xx = fabs(xcorn[j] + (yy - ycorn[j]) * (xcorn[j+1] - xcorn[j]) / 
            (ycorn[j+1] - ycorn[j]));
          minx = B3DMIN(minx, xx);
        }
        if (yy * minx > maxArea) {
          maxArea = yy * minx;
          besty = ytrim;
          bestx = B3DNINT(widthA / 2. - minx);
        }
      }
      nxTrimA = bestx;
      nyTrimA = besty;
    }
    nxTrimAright = nxTrimA;
    nyTrimAtop = nyTrimA;
  
    // If flag set, low dose mode, or low mag, or first round of realign, 
    // find needed trimming to avoid bad corners
  } else if (doTrim) {
    int nTrim = nxTrimA;
    centered = fabs(stretch - 1.) > 0.0005;
    CString report;
    ProcTrimCircle(mDataA, typeA, widthA, heightA, trimCenFrac, 
      trimTestSize, trimDelta, nxTrimA, nyTrimA, trimPassRatio, centered, nxTrimA, 
      nyTrimA, nxTrimAright, nyTrimAtop);
    ProcTrimCircle(mDataC, typeC, widthC, heightC, trimCenFrac, 
      trimTestSize, trimDelta, nxTrimC, nyTrimC, trimPassRatio, centered, nxTrimC, 
      nyTrimC, nxTrimCright, nyTrimCtop);
    if (nTrim != nxTrimA || nTrim != nxTrimC || nTrim != nxTrimAright || 
      nTrim != nxTrimCright || nTrim != nyTrimA || nTrim != nyTrimC || 
      nTrim != nyTrimAtop || nTrim != nyTrimCtop) {
        if (centered)
          report.Format("Trimming A by %d  %d,  reference by %d  %d", nxTrimA, nyTrimA,
          nxTrimC, nyTrimC);
        else
          report.Format("Trimming A by %d  %d  %d  %d,  reference by %d  %d  %d  %d", 
          nxTrimA, nxTrimAright, nyTrimA, nyTrimAtop, 
          nxTrimC, nxTrimCright, nyTrimC, nyTrimCtop);
        if ((!scaling && trimOutput) || GetDebugOutput('a'))
          mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    }
  }
  mLastAlignXTrimA = (nxTrimA + nxTrimAright) * needBinA;
  mLastAlignYTrimA = (nyTrimA + nyTrimAtop) * needBinA;
  mLastAlignXTrimRef = (nxTrimC + nxTrimCright) * needBinC;
  mLastAlignYTrimRef = (nyTrimC + nyTrimCtop) * needBinC;

  // Get coordinate limits of usable extent
  ix0A = nxTrimA;
  iy0A = nyTrimA;
  ix1A = widthA - nxTrimAright - 1;
  iy1A = heightA - nyTrimAtop - 1;
  ix0C = nxTrimC;
  iy0C = nyTrimC;
  ix1C = widthC - nxTrimCright - 1;
  iy1C = heightC - nyTrimCtop - 1;

  baseXshift = (int)(expectedXshift / needBinA);
  baseYshift = (int)(expectedYshift / needBinA);
  peakXoffset = -baseXshift;
  peakYoffset = -baseYshift;

  if (baseXshift || baseYshift) {
    extra = (int)(oversizeFrac * size);
    ShiftCoordinatesToOverlap(widthA, widthC, extra, baseXshift, ix0A, ix1A, ix0C, ix1C);
    ShiftCoordinatesToOverlap(heightA, heightC, extra, baseYshift, iy0A, iy1A, iy0C, iy1C);
  } else if (!centered) {

    // Set a base shift for non-centered trim if there is not one
    ix1A -= (ix1A + 1 - ix0A) % 2;
    ix1C -= (ix1C + 1 - ix0C) % 2;
    int offA = (widthA - B3DMIN(widthA, widthC)) / 2;
    int offC = (widthC - B3DMIN(widthA, widthC)) / 2;
    baseXshift = ((ix0C + 1 + ix1C) /2 - offC) - ((ix0A + 1 + ix1A) /2 - offA);
    iy1A -= (iy1A + 1 - iy0A) % 2;
    iy1C -= (iy1C + 1 - iy0C) % 2;
    offA = (heightA - B3DMIN(heightA, heightC)) / 2;
    offC = (heightC - B3DMIN(heightA, heightC)) / 2;
    baseYshift = ((iy0C + 1 + iy1C) /2 - offC) - ((iy0A + 1 + iy1A) /2 - offA);
  }

  // The change in the base shift is the amount the peak should be offset from middle
  peakXoffset += baseXshift;
  peakYoffset += baseYshift;

  // Determine padded image size, etc, and get the CTF
  nxUseA = ix1A + 1 - ix0A;
  nxUseC = ix1C + 1 - ix0C;
  nyUseA = iy1A + 1 - iy0A;
  nyUseC = iy1C + 1 - iy0C;

  // Back to the old way: take the larger area
  nxUse = B3DMAX(nxUseA, nxUseC);
  nyUse = B3DMAX(nyUseA, nyUseC);
  nxPad = XCorrNiceFrame((int)((1. + fracPad) * nxUse), 2, niceFFTlimit());
  nyPad = XCorrNiceFrame((int)((1. + fracPad) * nyUse), 2, niceFFTlimit());
  if (autoCorr) {
    nxPad = B3DMAX(nxPad, nyPad);
    nyPad = nxPad;
  }
  XCorrSetCTF(mSigma1 * freqScale, mSigma2 * freqScale * hiFreqScale, 0.f, 
    mRadius2 * freqScale * hiFreqScale, mCTFa, nxPad, nyPad, &delta);
  if (delta)
    for (int jj = 0; jj < 8193; jj++)
      mCTFa[jj] = (float)sqrt((double)mCTFa[jj]);
  
  NewArray2(mArray, float, nyPad, (nxPad + 2));
  NewArray2(mBrray, float, nyPad, (nxPad + 2));
  NewArray2(mCrray, float, nyPad, (nxPad + 2));
  if (MemoryError(mArray == NULL || mBrray == NULL || mCrray == NULL)) {
    return 1;
  }

  if (debugTime)
    time2 = wallTime() * 1000.;

  // Get tapered, padded images and correlate them.
  // It used to be taperFrac * (nxUseA + 2 * nxTrimA; i.e., trim size added back because
  // before that it was based on the the full width regardless of trim
  // hence the adjustment of the fraction here; but it should be a fraction of used width
  frac = mTaperFrac / (1.f - 2.f * trimFrac);
  nxTaper = (int)(frac * nxUseA);
  nyTaper = (int)(frac * nyUseA);
  XCorrTaperInPad(fillArray ? fillArray : mDataA, fillArray ? SLICE_MODE_FLOAT : typeA, 
    widthA, ix0A, ix1A, iy0A, iy1A, mArray, nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);

  if (tmplCorr) {
    nxTaper = 8;
    nyTaper = 8;
  } else {
    nxTaper = (int)(frac * nxUseC);
    nyTaper = (int)(frac * nyUseC);
  }
  XCorrTaperInPad(fillArray ? fillBrray : mDataC, fillArray ? SLICE_MODE_FLOAT: typeC,
    widthC, ix0C, ix1C, iy0C, iy1C, mBrray, nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);

  if (debugTime)
    time3 = wallTime() * 1000.;

  XCorrCrossCorr(mBrray, mArray, nxPad, nyPad, delta, mCTFa, mCrray);
  
  DELETE_ARR(fillArray);
  DELETE_ARR(fillBrray);
  if (debugTime)
    time4 = wallTime() * 1000.;

  if (alignLimit > 0) {
    iPeak = (int)(alignLimit / needBinA) + 2;
    setPeakFindLimits(-iPeak - baseXshift, iPeak - baseXshift, -iPeak - baseYshift,
      iPeak - baseYshift, 1);
  }

  XCorrPeakFindWidth(mBrray, nxPad + 2, nyPad, &Xpeaks[0], &Ypeaks[0], &peak[0], NULL,
    NULL, numPeaks, mPeakStrengthToEval);
  numRealPeaks = 0;

  // For template correlations, convert to coordinates in C and return
  if (tmplCorr) {
    for (iPeak = 0; iPeak < numPeaks; iPeak++) {
      if (peak[iPeak] < -1.e29)
        continue;
      numRealPeaks++;

      // Does Y need inversion here?
      Xpeaks[iPeak] = binC * (Xpeaks[iPeak] + widthC / 2.);
      Ypeaks[iPeak] = binC * (Ypeaks[iPeak] + heightC / 2.);
    }
    AutoalignCleanup();
    return 0;
  }

  // Loop through the peaks, evaluating the CCC at each of the 4 alternative actual shifts
  indMaxPeak = 0;
  minPixel = 0.05 * B3DMIN(nxUseA, nxUseC) * B3DMIN(nyUseA, nyUseC);
  bestWgtCCC = -10.;
  CCCbest = 0.;
  fracBest = 0.;
  for (iPeak = 0; iPeak < numPeaks; iPeak++) {
    if (peak[iPeak] < -1.e29 || peak[iPeak] < peak[0] * mPeakStrengthToEval)
      continue;
    if (mWinApp->mShiftCalibrator->CalibratingIS() && fabs((double)Xpeaks[iPeak]) < 0.25
      && fabs((double)Ypeaks[iPeak]) < 0.25 && (iPeak > 0 || peak[1] > -1.e29)) {
        SEMTrace('c', "Rejecting peak at 0,0, # %d, value %g, next peak %g", iPeak,
          peak[iPeak], peak[B3DMIN(numPeaks - 1, iPeak + 1)]);
        for (xInd = iPeak; xInd < numPeaks - 1; xInd++) {
          peak[xInd] = peak[xInd + 1];
          Xpeaks[xInd] = Xpeaks[xInd + 1];
          Ypeaks[xInd] = Ypeaks[xInd + 1];
        }
        iPeak--;
        continue;
    }
    numRealPeaks++;
    xPeak[0] = Xpeaks[iPeak];
    yPeak[0] = Ypeaks[iPeak];
    xInd = 0;
    yInd = 0;
    wgtCCCmax = -10.;

    // set up shifts of the alternative solutions
    if (xPeak[0] < 0)
      xPeak[1] = xPeak[0] + nxPad;
    else
      xPeak[1] = xPeak[0] - nxPad;
    if (yPeak[0] < 0)
      yPeak[1] = yPeak[0] + nyPad;
    else
      yPeak[1] = yPeak[0] - nyPad;

    for (int iy = 1; iy >= 0; iy--) {
      for (int ix = 1; ix >= 0 ; ix--) {
        if (alignLimit > 0. && sqrt(pow(xPeak[ix] + baseXshift, 2) +
          pow(yPeak[iy] + baseYshift, 2)) * needBinA > alignLimit)
          continue;
        CCChere = CCCoefficientTwoPads(mCrray, mArray,nxPad + 2, nxPad, nyPad,
          xPeak[ix], yPeak[iy], (nxPad - nxUseC) / 2, (nyPad - nyUseC) / 2 + 2,
          (nxPad - nxUseA) / 2 + 2, (nyPad - nyUseA) / 2 + 2 , minPixel, &numPixel);
        if (numPixel >= minPixel) {
          fracHere = (float)numPixel / (B3DMIN(nxUseA, nxUseC) * B3DMIN(nyUseA, nyUseC));
          wgtCCC = CCChere * pow((double)fracHere, overlapPow);
          if (probSigma > 0) {
            distsq = (float)((pow((double)xPeak[ix] + peakXoffset, 2.) + 
              pow((double)yPeak[iy] + peakYoffset, 2.)) * needBinA * needBinA);
            probFac = (float)exp(-0.5 * distsq / (probSigma * probSigma));
            wgtCCC *= probFac;
            SEMTrace('a', "%.1f %.1f Peak %g  CCC %.4f  frac %.2f  prob %.3f, wgtCCC "
              "%.4f%s", xPeak[ix], yPeak[iy], peak[iPeak], CCChere, fracHere, probFac, 
              wgtCCC, wgtCCC > bestWgtCCC ? "*" : "");
          } else {
            SEMTrace('a', "%.1f %.1f Peak %g  CCC %.4f  frac %.2f  wgtCCC %.4f%s", 
              xPeak[ix], yPeak[iy], peak[iPeak], CCChere, fracHere, wgtCCC, 
              wgtCCC > bestWgtCCC ? "*" : "");
          }

          // Find the best of the 4 alternatives
          if (wgtCCC > wgtCCCmax) {
            wgtCCCmax = wgtCCC;
            CCCmax = CCChere;
            fracMax = fracHere;
            xInd = ix;
            yInd = iy;
          }
        }
      }
    }

    // Replace the peak position and keep track of best weighted CCC
    if (wgtCCCmax >= -1.) {
      Xpeaks[iPeak] = xPeak[xInd];
      Ypeaks[iPeak] = yPeak[yInd];
      if (wgtCCCmax > bestWgtCCC) {
        bestWgtCCC = wgtCCCmax;
        indMaxPeak = iPeak;
        CCCbest = CCCmax;
        fracBest = fracMax;
      }
    }
  }

  // Better give distinct error return when no peak is within limits
  if (bestWgtCCC < 0.) {
    AutoalignCleanup();
    return -1;
  }

  *fracPtr = fracBest;
  *CCCptr = CCCbest;
  if (mWinApp->mScope->GetSimulationMode() && nxPad > 128 && nyPad > 128 && 
    (mImBufs->GetSaveCopyFlag() != 0 || mImBufs->mCaptured)) {
    tempX = (float)(0.02 * B3DMIN(nxUseA, nxUseC));
    tempY = (float)(0.02 * B3DMIN(nyUseA, nyUseC));
    B3DCLAMP(Xpeaks[indMaxPeak], -tempX, tempX);
    B3DCLAMP(Ypeaks[indMaxPeak], -tempY, tempY);
  }
  Xpeaks[indMaxPeak] += baseXshift;
  Ypeaks[indMaxPeak] += baseYshift;
  if (mWinApp->mShiftCalibrator->CalibratingIS())
    SEMTrace('c',"Peak # %d has highest CCC, %.4f  %.4f",indMaxPeak, CCCbest, bestWgtCCC);

  // To see the correlation, set it now because mBrray used to get reused
  if (showCor) {
    float corMin, corMax;
    mImA->UnLock();
    if (toBuf < mWinApp->mBufferManager->GetShiftsOnAcquire())
      mImC->UnLock();

    // Images have to be handled differently: get a new array and repack into it
    if (corrFlags & (AUTOALIGN_SHOW_FILTA | AUTOALIGN_SHOW_FILTC)) {
      float *imArr = (corrFlags & AUTOALIGN_SHOW_FILTA) ? mArray : mCrray;
      short *tmpArr;
      NewArray2(tmpArr, short int, nxPad, nyPad);
      if (!tmpArr) {
        AutoalignCleanup();
        return 1;
      }
      for (int iy = 0; iy < nyPad; iy++)
        for (int ix = 0; ix < nxPad; ix++)
          tmpArr[ix + iy * nxPad] = (short)imArr[ix + iy * (nxPad + 2)];
      mWinApp->mProcessImage->NewProcessedImage((corrFlags & AUTOALIGN_SHOW_FILTA) ?
        mImBufs : &mImBufs[bufIndex], tmpArr, kSHORT, nxPad, nyPad, 
        commonBin / mImBufs->mBinning);
    } else {
      mWinApp->mProcessImage->CorrelationToBufferA(mBrray, nxPad, nyPad,
        commonBin / mImBufs->mBinning, corMin, corMax);
    }

    // Reaquire pointers to image now in B
    mImA = mImBufs[1].mImage;
    mImA->Lock();
    if (!mDeleteA)
      mDataA = (void *)mImA->getData();
    if (autoCorr) {
      mImC = mImA;
      AutoalignCleanup();
      return 0;
    }
    if (toBuf < mWinApp->mBufferManager->GetShiftsOnAcquire()) {
      mImC = mImBufs[toBuf + 1].mImage;
      mImC->Lock();
      if (!mDeleteC)
       mDataC = (void *)mImC->getData();
    }
  }

  if (peakVal)
    *peakVal = peak[0];
  if (debugTime)
    time5 = wallTime() * 1000.;

  // This returned the shift to apply to A to align it to C, except that Y is inverted
  // But deStretch the shift if A was stretched; invert Y for this operation
  // This is needed for scaled data too, but not for rotated in original use because
  // NavRotAlign was going to transform image and wanted the shift that applied to it.
  // Now it is required to pass this flag to suppress correction for rotation
  if (transformedA && (rotation == 0. || !(corrFlags &  AUTOALIGN_NO_ROT_ADJUST))) {
    tempX = Xpeaks[indMaxPeak];
    tempY = -Ypeaks[indMaxPeak];
    Xpeaks[indMaxPeak] = strInv.xpx * tempX + strInv.xpy * tempY;
    Ypeaks[indMaxPeak] = -(strInv.ypx * tempX + strInv.ypy * tempY);
  }
  if (needBinA > 1) {
    Xpeaks[indMaxPeak] *= needBinA;
    Ypeaks[indMaxPeak] *= needBinA;
  }

  if (debugTime)
    PrintfToLog("%.1f to bin, %.1f to stretch/erase, %.1f to pad, %.1f to correlate\r\n"
    "%.1f to find %d peaks and get CCCs",
    time1 - startTime, time2 - time1, time3 - time2, time4 - time3,
    time5 - time4, numRealPeaks);

  // If shift values are desired, set them and skip the rest of this, but apply the 
  // adjustement to the expected shift for scaling
  if (xShiftOut && yShiftOut) {
    *xShiftOut = Xpeaks[indMaxPeak];
    *yShiftOut = Ypeaks[indMaxPeak];
    if (scaling) {
      *xShiftOut += (1. - 1. / scaling) * expectedXshift;
      *yShiftOut += (1. - 1. / scaling) * expectedYshift;
    }
   AutoalignCleanup();
    return 0;
  }

  // Get the current shift of C and add it to the peak shifts after adjusting for the
  // difference in binning between A and C
  mImC->getShifts(cshiftX, cshiftY);
  tempX = (cshiftX * binC) / binA;
  tempY = -(cshiftY * binC) / binA;

  // But also need to adjust shift to center the tilt on the center of shifted image
  // and this turns out to be the stretch (or destretch) of C's shift
  // Need to invert Y before and after this operation
  // 1/17/24: I think this applies only to true stretch between different tilts
  if (swapStretch && stretch != 1.0) {
    cshiftX = str.xpx * tempX + str.xpy * tempY;
    cshiftY = -(str.ypx * tempX + str.ypy * tempY);
  } else if (stretch != 1.0) {
    cshiftX = strInv.xpx * tempX + strInv.xpy * tempY;
    cshiftY = -(strInv.ypx * tempX + strInv.ypy * tempY);
  }

  // To prevent memory problem destroying safearray, need to unlock buffers here
  // before other routines access it
  mImA->UnLock();
  mImC->UnLock();

  BOOL lookingAtB = mWinApp->GetImBufIndex() == 1;
  mWinApp->SetCurrentBuffer(0);
  cshiftX += Xpeaks[indMaxPeak];
  cshiftY += Ypeaks[indMaxPeak];

  // If there is expected shift, subtract it from both C and A shift so that A shift
  // and image shift will be just enough to center desired point in A
  if (expectedXshift || expectedYshift) {
    cshiftX -= expectedXshift;
    cshiftY -= expectedYshift;
    mImC->getShifts(tempX, tempY);
    tempX -= (expectedXshift * binA) / binC;
    tempY -= (expectedYshift * binA) / binC;
    mImC->setShifts(tempX, tempY);
    mImBufs[toBuf].SetImageChanged(1);
    if (scaling) {
      cshiftX += (1. - 1. / scaling) * expectedXshift;
      cshiftY += (1. - 1. / scaling) * expectedYshift;
    }
  }

  if (showCor) {
    mWinApp->SetCurrentBuffer(0);

  } else {

    // Have to make sure A is the current buffer : but this doesn't work, so pass
    // the A buffer to SetAlignShifts so it knows who to modify
    retval = SetAlignShifts(cshiftX, cshiftY, false, mImBufs, doImShift && 
      !mScope->GetNoScope());

    // If montaging and this is montage center and overview is in B, shift
    // overview also, 
    if (mWinApp->Montaging() && mImBufs->IsMontageCenter() &&
      mImBufs[1].mImage && mImBufs[1].IsMontageOverview()) {
        binC = mImBufs[1].mBinning;
        mWinApp->SetCurrentBuffer(1);
        retval = SetAlignShifts((cshiftX * binA) / binC, (cshiftY * binA) / binC,
          false, mImBufs + 1);
        // restore display to A unless show overview at end is set and we were still
        // looking at it
        if (!(mWinApp->GetMontParam()->showOverview && lookingAtB))
          mWinApp->SetCurrentBuffer(0);
      }
  }    

  // Something in here (not SetCurrentBuffer or DrawImage) is making the mini-tilt series
  // window lose keyboard focus with no messages detectable: so set focus to the window
  // that should have it
  ((CMainFrame *)(mWinApp->m_pMainWnd))->MDIGetActive(NULL)->SetFocus();
  AutoalignCleanup();
  
  return retval;
}


// Adjust coordinates to be correlated when there is an expected shift so that the
// region correlated is the overlap zone plus some extra amount
void CShiftManager::ShiftCoordinatesToOverlap(int widthA, int widthC, int extra, 
                                              int & baseXshift, int & ix0A, int & ix1A,
                                              int & ix0C, int & ix1C)
{
  int dx0A, dx0C, dx1A, dx1C, dx0, dx1;

  // Get coordinates of each image range centered around the middle of C
  dx0A = ix0A + baseXshift - widthA / 2;
  dx1A = ix1A + baseXshift - widthA / 2;
  dx0C = ix0C - widthC / 2;
  dx1C = ix1C - widthC / 2;

  // Get intersection of this coordinate range
  // (Since we're not padding smaller, no longer try to keep half of C - NOW THAT WE
  // ARE BACK TO PADDING, THAT FEATURE IS GONE, BUT SHIFTED IMAGES ARE ALWAYS SAME SIZE?)
  // increase extra if this leaves no range
  dx0 = B3DMAX(dx0A, dx0C);
  dx1 = B3DMIN(dx1A, dx1C);
  if (dx1 < dx0)
    extra += dx0 - dx1;

  // Expand opposite sides of intersection by the extra amount, but limit amount to
  // the original limits
  if (baseXshift < 0) {
    extra = b3dIMin(3, extra, dx0 - dx0A, dx1C - dx1);
    dx0A = dx0 - extra;
    dx1A = dx1;
    dx0C = dx0;
    dx1C = dx1 + extra;
  } else {
    extra = b3dIMin(3, extra, dx0 - dx0C, dx1A - dx1);
    dx0A = dx0;
    dx1A = dx1 + extra;
    dx0C = dx0 - extra;
    dx1C = dx1;
  }

  // Keep each extent even then get new coordinate values and new base shift
  dx1A = dx0A + 2 * ((dx1A + 1 - dx0A) / 2) - 1;
  dx1C = dx0C + 2 * ((dx1C + 1 - dx0C) / 2) - 1;
  ix0A = dx0A + widthA / 2 - baseXshift;
  ix1A = dx1A + widthA / 2 - baseXshift;
  ix0C = dx0C + widthC / 2;
  ix1C = dx1C + widthC / 2;
  baseXshift = (ix1C + 1 + ix0C - widthC) / 2 - (ix1A + 1 + ix0A - widthA) / 2;
}

BOOL CShiftManager::MemoryError(BOOL inTest)
{
  if (inTest) {
    mWinApp->mTSController->TSMessageBox(_T("Error getting image memory - "
      "cannot autoalign")); 
    AutoalignCleanup();
  }
  return inTest;
}

void CShiftManager::AutoalignCleanup()
{
  mImA->UnLock();
  mImC->UnLock();
  delete [] mArray;
  delete [] mBrray;
  delete [] mCrray;
  delete [] mPeakHere;
  delete [] mXpeaksHere;
  delete [] mYpeaksHere;
  if (mDeleteA)
    delete [] mDataA;
  if (mDeleteC)
    delete [] mDataC;

}

////////////////////////////////////////////////////////////////////
// RESET IMAGE SHIFT
////////////////////////////////////////////////////////////////////
int CShiftManager::ResetImageShift(BOOL bDoBacklash, BOOL bAdjustScale, int waitTime,
  float relaxation)
{
  double shiftX, shiftY, delX, delY, angle, specX, specY;
  double adjustX, adjustY, roughScale;
  int magInd, area;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  float defocusFac = mDefocusZFactor * (mStageInvertsZAxis ? -1 : 1);
  float xTiltFac, yTiltFac;
  bool setBacklash;
  StageMoveInfo smi;
  ScaleMat aMat, dMat;

  // Wait used to be 2000; it said not to wait long since this is not supposed to be 
  // called unless stage is ready, but that doesn't work from macros.  Default is 5000
  if (mScope->WaitForStageReady(waitTime)) {
    SEMMessageBox(_T("Reset shift aborted - stage not ready"));
    return 1;
  }

  smi.axisBits = axisXY;
  mScope->GetLDCenteredShift(shiftX, shiftY);
  if (shiftX == 0. && shiftY == 0.)
    return 0;
  magInd = mScope->GetMagIndex();

  // Use shift in microns based on calibrated conversion if available, otherwise fall back
  // to rough scale
  delX = RadialShiftOnSpecimen(shiftX, shiftY, magInd);
  if (!delX) {
    if (mWinApp->GetSTEMMode())
      roughScale = mSTEMRoughISscale;
    else
      roughScale = (magInd < mScope->GetLowestNonLMmag() && mLMRoughISscale)
      ? mLMRoughISscale : mRoughISscale;

    delX = sqrt(shiftX * shiftX + shiftY * shiftY) * roughScale;
  }
  if (delX < 0.01)
    return 0;
  SEMTrace('i', "Resetting image shift of %f %f", shiftX, shiftY);

  // Get image shift calibration; if none, just reset and finish
  aMat = IStoSpecimen(magInd);
  if (!aMat.xpx) {
    mScope->IncImageShift(-shiftX, -shiftY);
    SetISTimeOut(GetLastISDelay());
    return 0;
  }
  specX = aMat.xpx * shiftX + aMat.xpy * shiftY;
  specY = aMat.ypx * shiftX + aMat.ypy * shiftY;

  // Compute the stage displacement with no adjustments
  // Use the nearest stage to camera matrix for this, so use IS not specimen coords
  angle = DTOR * GetStageTiltFactors(xTiltFac, yTiltFac);
  if (mWinApp->LowDoseMode() && IS_AREA_VIEW_OR_SEARCH(mScope->GetLowDoseArea())) {

    // If in low dose mode and current area is view or search, use focus-adjusted
    // transformations
    area = mScope->GetLowDoseArea();
    ldp += area;
    dMat = MatMul(FocusAdjustedISToCamera(mWinApp->GetCurrentCamera(), magInd,
      ldp->spotSize, ldp->probeMode, ldp->intensity, mScope->GetLDViewDefocus(area)),
      MatInv(FocusAdjustedStageToCamera(mWinApp->GetCurrentCamera(), magInd,
        ldp->spotSize, ldp->probeMode, ldp->intensity, mScope->GetLDViewDefocus(area))));
  } else {
    dMat = MatMul(IStoCamera(magInd),
      MatInv(StageToCamera(mWinApp->GetCurrentCamera(), magInd)));
  }
  delX = (dMat.xpx * shiftX + dMat.xpy * shiftY) / xTiltFac;
  delY = (dMat.ypx * shiftX + dMat.ypy * shiftY) / yTiltFac;
  adjustX = adjustY = 1.;
  if (bAdjustScale) {
    // Adjust scale  based on last displacement minus amount left
    // Just do Y for now.  Accumulate with last time's adjustment
    if (fabs(mResetStageMoveY) > 0.1)
      adjustY = (mResetStageMoveY - delY) / mResetStageMoveY;
    adjustY *= mResetStageAdjustY;

    // Limit adjustment to move stage as little as half as much (2)
    //  or as much as 1.67 times nominal amount (0.6)
    if (adjustY > 2.)
      adjustY = 2.;
    if (adjustY < 0.6)
      adjustY = 0.6;
    dMat = SpecimenToStage(adjustX, adjustY);
    delX = (dMat.xpx * specX + dMat.xpy * specY) / xTiltFac;
    delY = (dMat.ypx * specX + dMat.ypy * specY) / yTiltFac;
  }

  SEMTrace('i', "spec X,Y = %.2f, %.2f; delX,Y = %.2f, %.2f, adjustY = %.3f",
    specX, specY, delX, delY, adjustY);

  // Save displacements and adjustments
  mResetStageMoveX = delX;
  mResetStageMoveY = delY;
  mResetStageAdjustX = adjustX;
  mResetStageAdjustY = adjustY;

  mScope->GetStagePosition(smi.x, smi.y, smi.z);
  SEMTrace('i', "Stage at %.2f, %.2f  moving to %.2f %.2f", smi.x, smi.y, 
    smi.x + delX, smi.y + delY);
  setBacklash = bDoBacklash && (mBacklashMouseAndISR || 
    mWinApp->mNavHelper->GetRealigning() || mWinApp->mParticleTasks->DoingTemplateAlign())
    && fabs(angle / DTOR) < 10.;
  smi.relaxX = smi.relaxY = 0.;
  MaintainOrImposeBacklash(&smi, delX, delY, setBacklash);

  // relaxation is in the same direction as backlash
  if (setBacklash && relaxation > 0) {
    if (smi.backX)
      smi.relaxX = (smi.backX > 0. ? relaxation : -relaxation);
    if (smi.backY)
      smi.relaxY = (smi.backY > 0. ? relaxation : -relaxation);
  }
  SEMTrace('n', "RIS: backlash %.2f  %.2f  relax %.3f  %.3f", smi.backX, smi.backY, 
    smi.relaxX, smi.relaxY);

  // Do scope actions
  mResettingIS = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "RESETTING SHIFT");
  
  // Adjust focus, but only on first iteration
  // Do all scope actions before moving stage because of single Tecnai problem
  if (mWinApp->GetSTEMMode())
    defocusFac = -1. / mWinApp->mFocusManager->GetSTEMdefocusToDelZ(-1);
  if (!bAdjustScale  && magInd >= mScope->GetLowestNonLMmag())
    mScope->IncDefocus(specY * defocusFac * tan(angle));
  mScope->IncImageShift(-shiftX, -shiftY);
  mScope->MoveStage(smi, smi.backX != 0. || smi.backY != 0., false, 0, 
    smi.relaxX != 0. || smi.relaxY != 0.);
  SetISTimeOut(GetLastISDelay());
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_RESET_SHIFT, 0, 30000);
  return 0;
}

void CShiftManager::ResetISDone()
{
  mResettingIS = false;
  mStartedStageMove = false;
  if (mAcquireWhenShiftDone >= 0) {
    mCamera->InitiateCapture(mAcquireWhenShiftDone);
    mAcquireWhenShiftDone = -1 - mAcquireWhenShiftDone;
    mCamera->SetNewImageCallback(AlignRightDblClickImage);
  } else {
    mWinApp->UpdateBufferWindows();
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }
}

void CShiftManager::ResetISCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mWinApp->mTSController->TSMessageBox(
    _T("Time out trying to move stage while resetting image shift"));
  mAcquireWhenShiftDone = -1;
  ResetISDone();
  mWinApp->ErrorOccurred(error);
}

// Get the current backlash if the position in smi is still valid, or set the backlash
// from montaging at this mag, provided that the user's flag is set to do this
void CShiftManager::MaintainOrImposeBacklash(StageMoveInfo *smi, double delX, double delY,
                                             BOOL doBacklash)
{
  float backX, backY;
  double startX = smi->x, startY = smi->y;
  smi->backX = smi->backY = 0.;
  smi->x += delX;
  smi->y += delY;
  if (!doBacklash)
    return;
  if (mScope->GetValidXYbacklash(startX, startY, backX, backY)) {
    if ((smi->x - startX) * backX < 0 && (smi->y - startY) * backY < 0) {
      mScope->CopyBacklashValid();
      return;
    }
  } else {

    // This could be a mistake... it is not integrated with mapping and backlash moves
    /*backX = backY = mWinApp->mMontageController->GetStageBacklash();
    if (smi->x > startX)
      backX = - backX;
    if (smi->y > startY)
      backY = - backY;*/
    mWinApp->mMontageController->GetColumnBacklash(backX, backY);
  }
  smi->backX = backX;
  smi->backY = backY;
}

//////////////////////////////////////////////////////////////////////
// Routines for getting transformation matrixes
//////////////////////////////////////////////////////////////////////

// There are 5 coordinate systems to consider:
//
// 1) Tecnai specimen coordinates.  The X axis points to the tip of the rod, the
//    Y axis to the front of the microscope, and positive tilt is a CCW rotation around
//    the X axis in this orientation.
// 
// 2) Camera coordinates in unbinned pixels.  The Tecnai rotation angle is a correct
//    estimate of the rotation from the specimen to the camera coordinate system; namely
//    at mid-range mags the X axis is at 77-86 degrees.  The basis for getting from
//    specimen to camera coordinates is a calibration of pixel size and actual rotation
//    of the tilt axis.
//
// 3) Image coordinates in binned pixels.  Y is inverted relative to camera coordinates.
//    The shift in an image display is maintained in image coordinates.
//
// 4) Image shift coordinates.  The IS coordinate of a point is the IS needed to bring
//    the point to the center of the field.  The IS calibration is a true mapping from
//    IS to camera coordinates.
//
// 5) Stage coordinates.  The stage coordinate of a point is the Tecnai stage readout
//    when the point is in the center of the field.  The stage calibration is a true 
//    mapping from stage to camera coordinates.  However, an increase in stage X moves
//    the specimen to its positive X axis, and an increase in stage Y moves it to its
//    positive Y axis, which implies that positive X or Y stage coordinates are near the
//    negative X or Y specimen axis.  Thus the stage coordinates are 180 degrees rotated
//    from the specimen coordinates.

// Get the matrix relating specimen coordinates in microns to
// camera coordinates in pixels
// This is referred to as the A matrix
ScaleMat CShiftManager::SpecimenToCamera(int inCamera, int inMagInd)
{
  ScaleMat mat = {0., 0., 0., 0.};
  double angle;
  float pixel;

  angle = GetImageRotation(inCamera, inMagInd);

  // Compute matrix to get from microns to pixels
  pixel = GetPixelSize(inCamera, inMagInd);
  if (angle > 900 || pixel <= 0.)
    return mat;
  mat.xpx = (float)cos(angle * DTOR) / pixel;
  mat.xpy = -(float)sin(angle * DTOR) / pixel;
  mat.ypy = mat.xpx;
  mat.ypx = -mat.xpy;

  return mat;
}

// Matrix to get from current camera to specimen, which is what most routines need
ScaleMat CShiftManager::CameraToSpecimen(int magInd)
{
  ScaleMat mat = SpecimenToCamera(mWinApp->GetCurrentCamera(), magInd);
  if (!mat.xpx)
    return mat;
  return MatInv(mat);
}


// Get the matrix relating image shift to camera coordinates - this is what gets
// calibrated by image shift calibration
// This is referred to as the B matrix
ScaleMat CShiftManager::IStoCamera(int inMagInd)
{
  mCurrentCamera = mWinApp->GetCurrentCamera();
  return IStoGivenCamera(inMagInd, mCurrentCamera);
}

// Matrix to get from camera to image shift for current camera
ScaleMat CShiftManager::CameraToIS(int magInd)
{
  mCurrentCamera = mWinApp->GetCurrentCamera();
  ScaleMat mat = IStoGivenCamera(magInd, mCurrentCamera);
  if (!mat.xpx)
    return mat;
  return MatInv(mat);
}

ScaleMat CShiftManager::IStoGivenCamera(int inMagInd, int inCamera)
{
  ScaleMat mat;
  mat.xpx = 0.;
  int iCam, iMag, iAct;
  int iDir, delta, limlo, limhi;
  BOOL STEMcamera = mCamParams[inCamera].STEMcamera;
  int EFTEM = mCamParams[inCamera].GIF ? 1 : 0;
  bool debug = GetDebugOutput('c') && (GetDebugOutput('i') || GetDebugOutput('l'));

  MagTable *magtab = &mMagTab[inMagInd];
  float xpx = magtab->matIS[inCamera].xpx;

  // If the IS is calibrated here, return matrix
  if (mMagTab[inMagInd].matIS[inCamera].xpx != 0.)
    return mMagTab[inMagInd].matIS[inCamera];

  // Look for another camera calibrated at this mag, but not a GIF-nonGIF pair and
  // not involving any STEM
  for (iAct = 0; iAct < mWinApp->GetNumReadInCameras(); iAct++) {
    iCam = mActiveCameraList[iAct];
    if (iCam != inCamera && !mCamParams[iCam].STEMcamera && !STEMcamera && 
      (mCamParams[iCam].GIF ? 1 : 0) == EFTEM ) {
        if (mMagTab[inMagInd].matIS[iCam].xpx != 0.) {
          mat = AdjustedISmatrix(iCam, inMagInd, inCamera, inMagInd);
          if (debug)
            SEMTrace('c', "IStoGivenCamera at cam %d mag %d by specimen transfer "
                      "from cam %d at same mag: %.5g %.5g %.5g %.5g", inCamera, inMagInd,
                      iCam, mat.xpx, mat.xpy, mat.ypx, mat.ypy);
          return mat;
        }
    }
  }

  // Now look for a nearby calibrated IS in this camera
  mWinApp->GetMagRangeLimits(inCamera, inMagInd, limlo, limhi);
  for (delta = 1; delta < MAX_MAGS; delta++) {
    for (iDir = -1; iDir <= 1; iDir += 2) {
      iMag = inMagInd + delta * iDir;
      if (iMag < limlo || iMag > limhi)
        continue;
      if (mMagTab[iMag].matIS[inCamera].xpx != 0.) {
        if (!STEMcamera && mScope->GetUsePLforIS() && 
          mScope->BothLMorNotLM(iMag, false, inMagInd, false)) {
            if (debug)
              SEMTrace('c', "IStoGivenCamera at cam %d mag %d by direct copy of PLA cal "
                "from mag %d:", inCamera, inMagInd, iMag);
            return mMagTab[iMag].matIS[inCamera];
        } else if (CanTransferIS(iMag, inMagInd, STEMcamera, EFTEM)) {
          mat = AdjustedISmatrix(inCamera, iMag, inCamera, inMagInd);
          if (debug)
            SEMTrace('c', "IStoGivenCamera at cam %d mag %d by specimen transfer "
                      "from mag %d in same cam: %.5g %.5g %.5g %.5g", inCamera, inMagInd,
                      iMag, mat.xpx, mat.xpy, mat.ypx, mat.ypy);
          return mat;
        }
      }
    }
  }

  if (STEMcamera)
    return mat;

  // Finally, look for the nearest calibrated IS in any non-STEM camera
  for (iAct = 0; iAct < mWinApp->GetNumReadInCameras(); iAct++) {
    iCam = mActiveCameraList[iAct];
    if (iCam == inCamera || mCamParams[iCam].STEMcamera)
      continue;
    for (delta = 1; delta < MAX_MAGS; delta++) {
      for (iDir = -1; iDir <= 1; iDir += 2) {
        iMag = inMagInd + delta * iDir;
        if (iMag < 1 || iMag >= MAX_MAGS)
          continue;
        if (mMagTab[iMag].matIS[iCam].xpx != 0. && CanTransferIS(iMag, inMagInd, false,
          mCamParams[iCam].GIF ? 1 : 0)) {
          mat = AdjustedISmatrix(iCam, iMag, inCamera, inMagInd);
          return mat;
        }
      }
    }
  }

  return mat;
}

// Get the matrix relating Image Shift to specimen coordinates (toCam is optional)
// This is referred to as the C matrix, which is A-inverse * B
ScaleMat CShiftManager::IStoSpecimen(int inMagInd, int toCam)
{
  ScaleMat mat, bMat, aInv;
  mCurrentCamera = mWinApp->GetCurrentCamera();
  if (toCam < 0)
    toCam = mCurrentCamera;
  aInv = SpecimenToCamera(toCam, inMagInd);
  if (!aInv.xpx)
    return aInv;
  aInv = MatInv(aInv);
  bMat = IStoGivenCamera(inMagInd, toCam);

  // If no image shift calibration, return that
  if (bMat.xpx == 0.0)
    return bMat;

  // These traces make it give output during updates
  /*SEMTrace('c', "Mag %d: camera to specimen %.5g %.5g %.5g %.5g\r\n    IS to camera "
  "%.5g %.5g %.5g %.5g", inMagInd, aInv.xpx, aInv.xpy, aInv.ypx, aInv.ypy, bMat.xpx, 
  bMat.xpy, bMat.ypx, bMat.ypy); */
  mat = MatMul(bMat, aInv);
  //SEMTrace('c', "     IS to specimen %.5g %.5g %.5g %.5g", mat.xpx, mat.xpy, mat.ypx, 
  //mat.ypy);
  return mat;
}

// Get matrix relating stage to camera coordinates, based on averaged stage cal (UNUSED)
ScaleMat CShiftManager::AveragedStageToCamera(int inCamera, int inMagInd)
{
  ScaleMat prod;
  prod = MatMul(MatInv(SpecimenToStage(1., 1.)), SpecimenToCamera(inCamera, inMagInd));
  return prod;
}

// Get matrix relating stage to camera coordinates,
// based on a direct calibration if possible or derived from one
ScaleMat CShiftManager::StageToCamera(int inCamera, int inMagInd, int specOnly)
{
  ScaleMat mat, ISthisCam, ISotherCam, ISinv, prod;
  mat.xpx = 0.;
  int iCam, iMag, iAct, limlo, limhi;
  int iDirM, deltaM, iDirC;
  bool doTrace = GetDebugOutput('c') && !mWinApp->mBeamAssessor->CalibratingIntensity();

  MagTable *magtab = &mMagTab[inMagInd];

   // If the stage is calibrated here, return matrix
  if (magtab->matStage[inCamera].xpx != 0. && (specOnly & 2) == 0)
    return magtab->matStage[inCamera];
 
  // Use this image shift to convert from another calibration on first round,
  // only from same camera with transferrable IS
  // On second round use specimen to camera transformation
  ISthisCam = IStoGivenCamera(inMagInd, inCamera);
  for (int source = ((specOnly & 1) ? 1 : 0); source < 2; source++) {
    if (source)
      ISthisCam = SpecimenToCamera(inCamera, inMagInd);
    if (ISthisCam.xpx == 0.)
      continue;

    // Find the closest mag that has a stage calibration
    // Look first in the same camera then in other cameras
    for (iDirC = 0; iDirC < 2; iDirC++) {
      for (iAct = 0; iAct < mWinApp->GetNumReadInCameras(); iAct++) {
        iCam = mActiveCameraList[iAct];
        if ((iDirC && iCam != inCamera && source) || (!iDirC && iCam == inCamera)) {

          // Not between cameras if either is STEM
          if (iCam != inCamera && (mCamParams[iCam].STEMcamera || 
            mCamParams[inCamera].STEMcamera))
            continue;
          mWinApp->GetMagRangeLimits(iCam, inMagInd, limlo, limhi);
          for (deltaM = 1; deltaM < MAX_MAGS; deltaM++) {
            for (iDirM = -1; iDirM <= 1; iDirM += 2) {
              iMag = inMagInd + deltaM * iDirM;
              if (iMag < limlo || iMag > limhi)
                continue;
              if (mMagTab[iMag].matStage[iCam].xpx != 0.) {
                if (!source && CanTransferIS(iMag, inMagInd, mCamParams[iCam].STEMcamera, 
                  mCamParams[iCam].GIF ? 1 : 0)) {
                  
                  // Transform the other stage to camera calibration via the
                  // inverse of the IS cal times this mag/cam IS cal
                  ISotherCam = IStoGivenCamera(iMag, iCam);
                  ISinv = MatInv(ISotherCam);
                  prod = MatMul(mMagTab[iMag].matStage[iCam], ISinv);
                  mat = MatMul(prod, ISthisCam);
                  if (doTrace)
                    SEMTrace('c', "StageToCamera at cam %d mag %d by IS transfer from mag"
                      " %d: %.5g %.5g %.5g %.5g", inCamera, inMagInd, iMag, mat.xpx, 
                      mat.xpy, mat.ypx, mat.ypy);
                  return mat;
                } else if (source) {
                  
                  // Or do the same thing with Specimen to Camera
                  ISotherCam = SpecimenToCamera(iCam, iMag);
                  ISinv = MatInv(ISotherCam);
                  prod = MatMul(mMagTab[iMag].matStage[iCam], ISinv);
                  mat = MatMul(prod, ISthisCam);
                  if (doTrace)
                    SEMTrace('c', "StageToCamera at cam %d mag %d by specimen transfer "
                      "from cam %d mag %d: %.5g %.5g %.5g %.5g", inCamera, inMagInd, iCam,
                      iMag, mat.xpx, mat.xpy, mat.ypx, mat.ypy);
                  return mat;
                }
              }
            }
          }
        }
      }
    }
  }

  // Fallback to inverse of fallback specimen to stage times specimen to camera
  mat = FallbackSpecimenToStage(1., 1.);
  ISinv = MatInv(mat);
  mat = SpecimenToCamera(inCamera, inMagInd);
  prod = MatMul(ISinv, mat);
  if (doTrace)
    SEMTrace('c', "StageToCamera at cam %d mag %d by fallback: %.5g %.5g %.5g %.5g",
      inCamera, inMagInd, mat.xpx, mat.xpy, mat.ypx, mat.ypy);
  return prod;
}

// Modify a stage to camera matrix or its inverse  to work at the given tilt angle
void CShiftManager::AdjustStageToCameraForTilt(ScaleMat &aMat, float angle)
{
  ScaleMat aInv = MatInv(aMat);
  AdjustCameraToStageForTilt(aInv, angle);
  aMat = MatInv(aInv);
}

void CShiftManager::AdjustCameraToStageForTilt(ScaleMat &aInv, float angle)
{
  float cosAng = (float)cos(DTOR * angle);
  aInv.ypx /= cosAng;
  aInv.ypy /= cosAng;
}

// Get the Specimen to Stage transformation, which should be mag and camera
// independent.  So we use the average of available data
ScaleMat CShiftManager::SpecimenToStage(double adjustX, double adjustY)
{
  ScaleMat cum, mat;
  int iCam, iMag, nCum, iAct;
  cum.xpx = cum.xpy = cum.ypx = cum.ypy = 0.;
  nCum = 0;

  // Find all stage calibrations and get product of specimen to camera
  // and inverse of stage to camera.  Accumulate the sum
  for (iAct = 0; iAct < mWinApp->GetNumReadInCameras(); iAct++) {
    iCam = mActiveCameraList[iAct];
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (mMagTab[iMag].matStage[iCam].xpx != 0.) {
        mat = MatMul(SpecimenToCamera(iCam, iMag), MatInv(mMagTab[iMag].matStage[iCam]));
        nCum++;
        cum.xpx += mat.xpx;
        cum.xpy += mat.xpy;
        cum.ypx += mat.ypx;
        cum.ypy += mat.ypy;
      }
    }
  }
  
  // Return the average of this matrix, with the X and Y scaling (?)
  if (nCum) {
    mat.xpx = adjustX * cum.xpx / nCum;
    mat.ypx = adjustX * cum.ypx / nCum;
    mat.xpy = adjustY * cum.xpy / nCum;
    mat.ypy = adjustY * cum.ypy / nCum;
    return mat;
  }

  return FallbackSpecimenToStage(adjustX, adjustY);
}

// This is a fallback for specimen to stage transformation
ScaleMat CShiftManager::FallbackSpecimenToStage(double adjustX, double adjustY)
{
  double thetaX = 0.;
  double thetaY = 0.;
  double scaleX = adjustX * (mInvertStageXAxis ? -1 : 1);
  double scaleY = adjustY;
  ScaleMat dMat;
  if (HitachiScope) {
    thetaX += mHitachiStageSpecAngle;
    thetaY += mHitachiStageSpecAngle;
  }

  // 3/5/04: invert the signs of everything because stage is 180 deg
  // around from specimen
  dMat.xpx = -(float)(scaleX * cos(DTOR * thetaX));
  dMat.ypx = -(float)(scaleX * sin(DTOR * thetaX));
  dMat.xpy = (float)(scaleY * sin(DTOR * thetaY));
  dMat.ypy = -(float)(scaleY * cos(DTOR * thetaY));
  return (MatInv(dMat));
}


// Get pixel size for the current camera at the specified mag index (replaced 11/30/06)
float CShiftManager::GetPixelSize(int inCamera, int inMagIndex)
{
  return mMagTab[inMagIndex].pixelSize[inCamera];
}

// Get the pixel size of an image
float CShiftManager::GetPixelSize(EMimageBuffer *imBuf, float *focusRot)
{
  float scale, rotation, pixel = 0.;
  if (focusRot)
    *focusRot = 0.;
  if (imBuf ) {
    if (imBuf->mPixelSize > 0)
      pixel = imBuf->mPixelSize;
    else if (imBuf->mBinning && imBuf->mCamera >= 0 && imBuf->mMagInd)
      pixel = (float)(imBuf->mBinning * GetPixelSize(imBuf->mCamera, imBuf->mMagInd));
    if (imBuf->mMagInd >= mScope->GetLowestMModeMagInd() &&
      GetScaleAndRotationForFocus(imBuf, scale, rotation)) {
      pixel /= scale;
      if (focusRot)
        *focusRot = rotation;
    }
  }
  return pixel;
}

// Return a refined pixel size in microns if there is one for the camera and mag
float CShiftManager::GetRefinedPixel(EMimageBuffer *imBuf) {
  return GetRefinedPixel(imBuf->mCamera, imBuf->mMagInd, imBuf->mBinning);
}

// Returns the pixel size as unbinned value or binned value if that argument is included
float CShiftManager::GetRefinedPixel(int camera, int magInd, int binning)
{
  std::map<int, float> ::iterator iter;
  if (camera < 0 || !magInd || !binning)
    return 0.0f;
  int index = 10 * magInd + camera;
  if (!mRefinedPixelSizes.count(index))
    return 0.;
  iter = mRefinedPixelSizes.find(index);
  return (float)(binning * 0.001 * iter->second);
}

// Get the image rotation: replaced by new methods 11/30/06
double CShiftManager::GetImageRotation(int inCamera, int inMagInd)
{
  CString warning;
  if (inCamera < 0 || inCamera >= MAX_CAMERAS) {
    warning.Format("WARNING: camera index %d out of range in call to GetImageRotation",
      inCamera);
  } else if (mCamParams[inCamera].STEMcamera) {
    return mCamParams[inCamera].imageRotation;
  } else if (inMagInd < 0 || inMagInd >= MAX_MAGS) {
    warning.Format("WARNING: mag index %d out of range in call to GetImageRotation",
      inMagInd);
  } else {
    return mMagTab[inMagInd].rotation[inCamera];
  }
  if (mScope->GetNoScope())
    return 0.;
  warning += "\r\nPlease send this log with backtrace to David; this is NOT a crash";
  AddBackTraceToMessage(warning);
  mWinApp->AppendToLog(warning);
  return 0.;
}

// Compute a pixel size for all mags by propagating any calibration information,
// from direct pixel sizes and image shift calibrations, and falling back to scope mag
void CShiftManager::PropagatePixelSizes(void)
{
  int iAct, iCam, iMag, regCam, mag2, iDir, calMag, delta, derived, iAct2, iCam2;
  int limlo, limhi, numRanges, range, lowestMicro;
  int midFilmMag = 15000;
  int numCam = mWinApp->GetNumReadInCameras();
  int lowestMmag = mScope->GetLowestMModeMagInd();
  float ratio;
  CameraParameters *camP = mCamParams;
  MagTable *magT = mMagTab;
  BOOL anyCal = false;
  mWinApp->mMacroProcessor->SetNonMacroDeferLog(true);

  // Clear out existing derived calibrations; see if there are any calibrated pixels
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (magT[iMag].pixelSize[iCam] && !magT[iMag].pixDerived[iCam])
        anyCal = true;
      else {
        magT[iMag].pixelSize[iCam] = 0.;
        magT[iMag].pixDerived[iCam] = 1000000;   // To make comparisons easier
      }
    }
  }

  // If there are no direct calibrations, pick a camera and mag to set the fallback in
  derived = 1;
  if (!anyCal) {
    PickMagForFallback(-1, calMag, regCam);
    magT[calMag].pixelSize[regCam] = camP[regCam].pixelMicrons / 
      (MagForCamera(regCam, calMag) * camP[regCam].magRatio);
    magT[calMag].pixDerived[regCam] = derived++;
    SEMTrace('c', "Set initial pixel fallback %f for mag %d camera %d", 
      magT[calMag].pixelSize[regCam] * 1000., calMag, regCam);
  }
  mAnyCalPixelSizes = anyCal;

  anyCal = true;
  while (anyCal) {
    anyCal = false;

    // Propagate calibrations between mags with image shifts
    for (iAct = 0; iAct < numCam; iAct++) {
      iCam = mActiveCameraList[iAct];
      for (iMag = 1; iMag < MAX_MAGS; iMag++) {
        if (!magT[iMag].matIS[iCam].xpx)
          continue;
        mWinApp->GetMagRangeLimits(iCam, iMag, limlo, limhi);
        for (delta = 1; delta < MAX_MAGS && !magT[iMag].pixelSize[iCam]; delta++) {
          for (iDir = -1; iDir <= 1; iDir += 2) {
            mag2 = iMag + delta * iDir;

            // If the other mag has a calibrated pixel at a lower level of derivation
            // and it has an image shift cal that can transfer, get the pixel size
            if (mag2 >= limlo && mag2 <= limhi && 
              magT[mag2].pixDerived[iCam] < derived &&
              magT[mag2].matIS[iCam].xpx && 
              CanTransferIS(mag2, iMag, camP[iCam].STEMcamera, camP[iCam].GIF ? 1 : 0)) {
                magT[iMag].pixelSize[iCam] = TransferPixelSize(
                  mag2, iCam, iMag, iCam);
                magT[iMag].pixDerived[iCam] = derived;
                anyCal = true;
                SEMTrace('c', "Mag %d Cam %d  Pixel = %.3f by IS transfer from mag %d"
                  ", derived %d", iMag, iCam, magT[iMag].pixelSize[iCam] * 1000.,
                  mag2, derived);
                break;
            }
          }
        }
      }
    }
    derived++;

    // Propagate calibrations between cameras at same mag by image shift cal
    for (iAct = 0; iAct < numCam; iAct++) {
      iCam = mActiveCameraList[iAct];
      if (camP[iCam].STEMcamera)
        continue;
      for (iMag = 1; iMag < MAX_MAGS; iMag++) {
        if (!magT[iMag].matIS[iCam].xpx || magT[iMag].pixelSize[iCam])
          continue;
        for (iAct2 = 0; iAct2 < numCam; iAct2++) {
          iCam2 = mActiveCameraList[iAct2];
          if (camP[iCam2].STEMcamera)
            continue;

          // Require calibrated size at a lower level of derivation than what was
          // just done, and IS cal
          if (iCam2 != iCam && magT[iMag].pixDerived[iCam2] < derived - 1 &&
            magT[iMag].matIS[iCam2].xpx) {
              magT[iMag].pixelSize[iCam] = TransferPixelSize(iMag, iCam2, iMag, iCam);
              magT[iMag].pixDerived[iCam] = derived;
              anyCal = true;
              SEMTrace('c', "Mag %d Cam %d  Pixel = %.3f by IS transfer from Cam %d"
                ", derived %d", iMag, iCam, magT[iMag].pixelSize[iCam] * 1000.,
                iCam2, derived);
              break;
            }
        }
      }
    }
    derived++;
  }

  // For a camera with any calibrations, try to use pixel size ratio from another similar
  // camera then complete the calibrations using nearby film to camera ratios
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
    for (range = 0; range < numRanges; range++) {
      mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
      for (iMag = limlo; iMag <= limhi; iMag++)
        if (magT[iMag].pixelSize[iCam])
          break;
      if (iMag <= limhi) {
        for (iMag = limlo; iMag <= limhi; iMag++) {
          if (!magT[iMag].pixelSize[iCam] && MagForCamera(iCam, iMag)) {

            // Find closest previously calibrated mag
            anyCal = false;
            for (delta = 1; delta < MAX_MAGS && !anyCal && !camP[iCam].STEMcamera; 
              delta++) {
                for (iDir = -1; iDir <= 1; iDir += 2) {
                  mag2 = iMag + delta * iDir;
                  if (mag2 >= limlo && mag2 <= limhi && 
                    magT[mag2].pixDerived[iCam] < derived) {

                      // Look for a camera with both mags calibrated
                      for (iAct2 = 0; iAct2 < numCam; iAct2++) {
                        iCam2 = mActiveCameraList[iAct2];

                        if (iCam2 != iCam && magT[iMag].pixDerived[iCam2] < derived &&
                          magT[mag2].pixDerived[iCam2] < derived && 
                          !camP[iCam2].STEMcamera &&
                          (camP[iCam].GIF ? 1 : 0) == (camP[iCam2].GIF ? 1 : 0)) {
                            magT[iMag].pixelSize[iCam] = magT[mag2].pixelSize[iCam] *
                              magT[iMag].pixelSize[iCam2] / magT[mag2].pixelSize[iCam2];
                            magT[iMag].pixDerived[iCam] = derived;
                            SEMTrace('c', "Mag %d Cam %d  Pixel = %.3f by ratio to mag %d"
                              " in cam %d", iMag, iCam, 
                              magT[iMag].pixelSize[iCam] * 1000., mag2, iCam2);
                            break;
                        }
                      }
                      anyCal = true;
                      break;
                  }
                }
            }

            // Now fallback using mean of nearby film to camera ratios
            if (!magT[iMag].pixelSize[iCam]) {
              ratio = NearbyFilmCameraRatio(iMag, iCam, derived);
              magT[iMag].pixelSize[iCam] = camP[iCam].pixelMicrons / 
                (MagForCamera(iCam, iMag) * ratio);
              magT[iMag].pixDerived[iCam] = derived + 1;
              SEMTrace('c', "Mag %d Cam %d  Pixel = %.4f by fallback with nearby ratio %f"
                , iMag, iCam, magT[iMag].pixelSize[iCam] * 1000., ratio);
            }
          }
        }
      }
    }
  }
  derived += 2;

  // Now find uncalibrated cameras and set up complete fallback
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (!magT[iMag].pixelSize[iCam] && MagForCamera(iCam, iMag)) {
         magT[iMag].pixelSize[iCam] = camP[iCam].pixelMicrons / 
            (MagForCamera(iCam, iMag) * camP[iCam].magRatio);
         magT[iMag].pixDerived[iCam] = derived;
         SEMTrace('c', "Mag %d Cam %d  Pixel = %.3f by fallback",
           iMag, iCam, magT[iMag].pixelSize[iCam] * 1000.);
      }
    }
  }
  mWinApp->mMacroProcessor->SetNonMacroDeferLog(false);
  if (mWinApp->mLogWindow)
    mWinApp->mLogWindow->FlushDeferredLines();
}

// Transfer a pixel size from one mag and camera to another via image shift calibrations
double CShiftManager::TransferPixelSize(int fromMag, int fromCam, int toMag, int toCam)
{
  ScaleMat aInv, aProd;
  aInv = MatInv(mMagTab[toMag].matIS[toCam]);
  aProd = MatMul(aInv, mMagTab[fromMag].matIS[fromCam]);
  return mMagTab[fromMag].pixelSize[fromCam] * 0.5 * 
    (sqrt(aProd.xpx * aProd.xpx + aProd.xpy * aProd.xpy) + 
    sqrt(aProd.ypx * aProd.ypx + aProd.ypy * aProd.ypy));
}

// Compute a film to camera ratio from the nearest mags whose calibrations are
// less derived than the given value
float CShiftManager::NearbyFilmCameraRatio(int inMag, int inCam, int derived)
{
  int delta, iDir, mag2, limlo, limhi;
  int numAvg = 0;
  int numMax = 5;
  float ratSum = 0.;
  mWinApp->GetMagRangeLimits(inCam, inMag, limlo, limhi);
  for (delta = 0; delta < MAX_MAGS && numAvg < numMax; delta++) {
    for (iDir = -1; iDir <= 1 && numAvg < numMax; iDir += 2) {
      if (!delta && iDir > 0)
        continue;
      mag2 = inMag + delta * iDir;
      if (mag2 >= limlo && mag2 <= limhi && mMagTab[mag2].pixDerived[inCam] < derived) {
        ratSum += mCamParams[inCam].pixelMicrons / (mMagTab[mag2].pixelSize[inCam] *
        MagForCamera(inCam, mag2));
        numAvg++;
      }
    }
  }
  if (numAvg)
    return (ratSum / numAvg);
  return mCamParams[inCam].magRatio;
}

// Compute a rotation angles for all mags by propagating any calibration information,
// from absolute and relative rotations and image shift calibrations, and falling back to
// changes in scope rotation angles
void CShiftManager::PropagateRotations(void)
{
  int iAct, iCam, iMag, regCam, calMag, derived, iAct2, iCam2;
  int numRanges, range, limlo, limhi, lowestMicro, midFilmMag = 15000;
  int numCam = mWinApp->GetNumReadInCameras();
  int lowestMmag = mScope->GetLowestMModeMagInd();
  bool anyCal = false, anyOK;
  CameraParameters *camP = mCamParams;
  MagTable *magT = mMagTab;
  int camFallbackDerived[MAX_CAMERAS];
  mAnyAbsRotCal = false;
  mAnyAbsSTEMrotCal = false;
  mWinApp->mMacroProcessor->SetNonMacroDeferLog(true);

  // Clear out existing derived calibrations; see if there are any calibrated rotations
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (magT[iMag].rotation[iCam] < 900. && !magT[iMag].rotDerived[iCam]) {
        if (!camP[iCam].STEMcamera) {
          if (!mAnyAbsRotCal)
            SEMTrace('c', "First calibrated absolute rotation of %.2f found at mag %d "
            "for camera %d", magT[iMag].rotation[iCam], iMag, iCam);
          mAnyAbsRotCal = true;
        } else {
          if (!mAnyAbsSTEMrotCal)
            SEMTrace('c', "First calibrated absolute STEM rotation of %.2f found at mag"
              " %d for camera %d", magT[iMag].rotation[iCam], iMag, iCam);
          mAnyAbsSTEMrotCal = true;
        }
      } else {
        magT[iMag].rotation[iCam] = 999.;
        magT[iMag].rotDerived[iCam] = 1000000;   // To make comparisons easier
      }
    }
  }

  // If there are no direct calibrations, pick a camera and mag to set the fallback in
  derived = 1;
  if (!mAnyAbsRotCal) {
    PickMagForFallback(-1, calMag, regCam);
    if (camP[regCam].STEMcamera)
      magT[calMag].rotation[regCam] = 0.;
    else
      magT[calMag].rotation[regCam] = GoodAngle((camP[regCam].GIF ?
        magT[calMag].EFTEMtecnaiRotation : magT[calMag].tecnaiRotation) + 
        mGlobalExtraRotation + camP[regCam].extraRotation);
    magT[calMag].rotDerived[regCam] = derived++;
    SEMTrace('c', "Set initial rotation fallback %f for mag %d camera %d", 
      magT[calMag].rotation[regCam], calMag, regCam);
  }

  PropagateCalibratedRotations(-1, derived);

  // For a camera with no calibrations yet, take one angle from another camera by
  // difference in extra rotation
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
    for (range = 0; range < numRanges; range++) {
      mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
      anyCal = false;
      for (iMag = limlo; iMag <= limhi; iMag++) {
        if (magT[iMag].rotation[iCam] < 900.) {
          anyCal = true;
          break;
        }
      }
      if (anyCal)
        continue;
      if (camP[iCam].STEMcamera) {

        // Uncalibrated STEM camera: just set to zero; extraRotation is added to this
        for (iMag = limlo; iMag <= limhi; iMag++) {
          if (magT[iMag].STEMmag) {
            magT[iMag].rotation[iCam] = 0;
            magT[iMag].rotDerived[iCam] = derived;
          }
        }

        // regular camera: look for another non-STEM camera
      } else {
        for (iAct2 = 0; iAct2 < numCam && !anyCal; iAct2++) {
          iCam2 = mActiveCameraList[iAct2];
          if (iCam2 == iCam || camP[iCam2].STEMcamera)
            continue;
          for (iMag = B3DMAX(lowestMmag, limlo); iMag <= limhi; iMag++) {
            if (magT[iMag].rotation[iCam2] < 900.) {
              magT[iMag].rotation[iCam] = GoodAngle(magT[iMag].rotation[iCam2] +
                camP[iCam].extraRotation - camP[iCam2].extraRotation);
              anyCal = true;
              magT[iMag].rotDerived[iCam] = derived + 1;
              SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f assigned from Cam %d by diff"
                " in ExtraRotations", iMag, iCam, magT[iMag].rotation[iCam], iCam2);
              break;
            }
          }
        }
      }
    }
  }
  derived++;
  for (iCam = 0; iCam < MAX_CAMERAS; iCam++)
    camFallbackDerived[iCam] = derived;
  derived++;

  // Now look at each uncalibrated one and find nearest calibrated mag
  FallbackToRotationDifferences(-1, derived);
  derived += 2;

  // Now find uncalibrated cameras and pick an individual fallback
  mCamWithRotFallback.clear();
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    anyCal = false;
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (magT[iMag].rotation[iCam] < 900. && MagForCamera(iCam, iMag)) {
        anyCal = true;
        break;
      }
    }
    if (!anyCal) {
      mCamWithRotFallback.insert(iCam);
      PickMagForFallback(iAct, calMag, regCam);
      if (camP[regCam].STEMcamera)
        magT[calMag].rotation[regCam] = 0.;
      else
        magT[calMag].rotation[regCam] = GoodAngle((camP[regCam].GIF ?
          magT[calMag].EFTEMtecnaiRotation : magT[calMag].tecnaiRotation) +
          mGlobalExtraRotation + camP[regCam].extraRotation);
      magT[calMag].rotDerived[regCam] = derived++;
      SEMTrace('c', "Set rotation fallback %f for mag %d in camera %d",
        magT[calMag].rotation[regCam], calMag, regCam);

      // Rerun the propagations - the fallback should cover it completely
      PropagateCalibratedRotations(iAct, derived);
      camFallbackDerived[iCam] = derived;
      FallbackToRotationDifferences(iAct, derived);
      derived++;
    }

  }
  
  // Make lists of mags relying on nominal rotation differences for other cameras
  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    mMagsWithRotFallback[iCam].clear();
    anyOK = false;
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (!magT[iMag].pixDerived[iCam] || magT[iMag].matIS[iCam].xpx != 0.) {
        if (magT[iMag].rotDerived[iCam] >= camFallbackDerived[iCam])
          mMagsWithRotFallback[iCam].push_back(MagForCamera(iCam, iMag));
        else
          anyOK = true;
      }
    }
    if (mMagsWithRotFallback[iCam].size() > 0 && !anyOK) {
      mCamWithRotFallback.insert(iCam);
      mMagsWithRotFallback[iCam].clear();
    }
  }
  mWinApp->mMacroProcessor->SetNonMacroDeferLog(false);
  if (mWinApp->mLogWindow)
    mWinApp->mLogWindow->FlushDeferredLines();
}

// Spreads rotation information via calibrated relative rotation or by IS transfer
void CShiftManager::PropagateCalibratedRotations(int actCamToDo, int & derived)
{
  int iAct, iCam, iMag, mag2, delInd, iDir, delta, iAct2, iCam2;
  int fromMag, toMag, numRanges, range, limlo, limhi, lowestMicro;
  double rotation;
  int numCam = mWinApp->GetNumReadInCameras();
  bool anyProp, anyCal = false;
  CameraParameters *camP = mCamParams;
  MagTable *magT = mMagTab;
  int actStart = actCamToDo < 0 ? 0 : actCamToDo;
  int actEnd = actCamToDo < 0 ? numCam - 1 : actCamToDo;

  anyCal = true;
  while (anyCal) {
    anyCal = false;

    // Propagate calibrated rotations by relative rotations within camera
    for (iAct = actStart; iAct <= actEnd; iAct++) {
      iCam = mActiveCameraList[iAct];
      mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
      for (range = 0; range < numRanges; range++) {
        mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
        SEMTrace('c', "cam  %d range %d lo %d hi %d", iCam, range, limlo, limhi);
        anyProp = true;
        while (anyProp) {
          anyProp = false;
          for (iMag = limlo; iMag <= limhi; iMag++) {
            if (magT[iMag].rotation[iCam] < 900.) {

              // Go one step forward then backward
              // Don't do anything if a mag is calibrated already or there is no delta
              for (delInd = 0; delInd < 2; delInd++) {
                iDir = 1 - 2 * delInd;
                mag2 = iMag + iDir;
                if (mag2 < limlo || mag2 + delInd > limhi || magT[mag2].rotation[iCam] <
                  900. || magT[mag2 + delInd].deltaRotation[iCam] > 900.)
                  continue;
                magT[mag2].rotation[iCam] = GoodAngle(magT[mag2 - iDir].rotation[iCam]
                  + iDir * magT[mag2 + delInd].deltaRotation[iCam]);
                magT[mag2].rotDerived[iCam] = derived;
                anyCal = true;
                anyProp = true;
                SEMTrace('c', "Mag %d Cam %d  Rotation by delta = %.1f, derived %d",
                  mag2, iCam, magT[mag2].rotation[iCam], derived);

                // Skip the mag we just assigned to to avoid propagating it forward
                if (iDir > 0)
                  iMag++;
              }
            }
          }
        }

        // Now search for and satisfy any relative rotations in the special table
        for (iMag = limlo; iMag <= limhi; iMag++) {
          if (magT[iMag].rotation[iCam] < 900.) {
            for (iDir = 0; iDir < mNumRelRotations; iDir++) {
              if (mRelRotations[iDir].camera == iCam) {
                fromMag = mRelRotations[iDir].fromMag;
                toMag = mRelRotations[iDir].toMag;
                mag2 = 0;
                if (fromMag == iMag && magT[toMag].rotation[iCam] > 900.) {
                  mag2 = toMag;
                  rotation = mRelRotations[iDir].rotation;
                }
                if (toMag == iMag && magT[fromMag].rotation[iCam] > 900.) {
                  mag2 = fromMag;
                  rotation = -mRelRotations[iDir].rotation;
                }
                if (mag2) {
                  magT[mag2].rotation[iCam] = GoodAngle(magT[iMag].rotation[iCam]
                    + rotation);
                  magT[mag2].rotDerived[iCam] = derived;
                  anyCal = true;
                  SEMTrace('c', "Mag %d Cam %d  Rotation by special = %.1f, derived %d",
                    mag2, iCam, magT[mag2].rotation[iCam], derived);
                }
              }
            }
          }
        }
      }
    }
    derived++;

    // Propagate calibrations between mags with image shifts
    // Skip STEM camera because 1) image shift needs to be calibrated after relative 
    // angles are stabilized; 2) not sure of the sign
    for (iAct = actStart; iAct <= actEnd; iAct++) {
      iCam = mActiveCameraList[iAct];
      if (camP[iCam].STEMcamera)
        continue;
      for (iMag = 1; iMag < MAX_MAGS; iMag++) {
        if (!magT[iMag].matIS[iCam].xpx)
          continue;
        for (delta = 1; delta < MAX_MAGS && magT[iMag].rotation[iCam] > 900.;
          delta++) {
          for (iDir = -1; iDir <= 1; iDir += 2) {
            mag2 = iMag + delta * iDir;

            // If the other mag has a calibrated rotation at a lower level of 
            // derivation and it has an image shift cal that can transfer, get the 
            // rotation
            if (mag2 >= 1 && mag2 < MAX_MAGS && magT[mag2].rotDerived[iCam] <
              derived && magT[mag2].matIS[iCam].xpx && CanTransferIS(mag2, iMag, false,
                camP[iCam].GIF ? 1 : 0)) {
              magT[iMag].rotation[iCam] = TransferImageRotation(
                magT[mag2].rotation[iCam], iCam, mag2, iCam, iMag);
              magT[iMag].rotDerived[iCam] = derived;
              anyCal = true;
              SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f by IS transfer from mag"
                " %d, derived %d", iMag, iCam, magT[iMag].rotation[iCam], mag2,
                derived);
              break;
            }
          }
        }
      }
    }
    derived++;

    // Propagate calibrations between cameras at same mag by image shift cal
    // Skip STEM cameras for this
    if (actCamToDo < 0) {
      for (iAct = 0; iAct < numCam; iAct++) {
        iCam = mActiveCameraList[iAct];
        if (camP[iCam].STEMcamera)
          continue;
        for (iMag = 1; iMag < MAX_MAGS; iMag++) {
          if (!magT[iMag].matIS[iCam].xpx || magT[iMag].rotation[iCam] < 900.)
            continue;
          for (iAct2 = 0; iAct2 < numCam; iAct2++) {
            iCam2 = mActiveCameraList[iAct2];

            // Require calibrated rotation at a lower level of derivation than the
            // preceding IS transfer, and require IS cal
            if (iCam2 != iCam && magT[iMag].rotDerived[iCam2] < derived - 1 &&
              magT[iMag].matIS[iCam2].xpx && !camP[iCam2].STEMcamera &&
              (camP[iCam].GIF == camP[iCam2].GIF ||
                BOOL_EQUIV(iMag < mScope->GetLowestMModeMagInd(camP[iCam].GIF != 0),
                  iMag < mScope->GetLowestMModeMagInd(camP[iCam2].GIF) != 0))) {
              magT[iMag].rotation[iCam] = TransferImageRotation(
                magT[iMag].rotation[iCam2], iCam2, iMag, iCam, iMag);
              magT[iMag].rotDerived[iCam] = derived;
              anyCal = true;
              SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f by IS transfer from Cam %d"
                ", derived %d", iMag, iCam, magT[iMag].rotation[iCam], iCam2,
                derived);
              break;
            }
          }
        }
      }
    }
    derived++;
  }
}

// For remaining mags, uses nominal differences in mag table as fallback
void CShiftManager::FallbackToRotationDifferences(int actCamToDo, int & derived)
{
  int iAct, iCam, iMag, mag2, iDir, delta, iAct2, iCam2;
  int numRanges, range, limlo, limhi, lowestMicro;
  int numCam = mWinApp->GetNumReadInCameras();
  CameraParameters *camP = mCamParams;
  MagTable *magT = mMagTab;
  int actStart = actCamToDo < 0 ? 0 : actCamToDo;
  int actEnd = actCamToDo < 0 ? numCam - 1 : actCamToDo;

  for (iAct = 0; iAct < numCam; iAct++) {
    iCam = mActiveCameraList[iAct];
    mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
    for (range = 0; range < numRanges; range++) {
      mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
      for (iMag = limlo; iMag <= limhi; iMag++) {
        if (magT[iMag].rotation[iCam] < 900. || !MagForCamera(iCam, iMag))
          continue;
        for (delta = 1; delta < MAX_MAGS && magT[iMag].rotation[iCam] > 900.; delta++) {
          for (iDir = -1; iDir <= 1; iDir += 2) {
            mag2 = iMag + delta * iDir;
            if (mag2 >= limlo && mag2 <= limhi && magT[mag2].rotDerived[iCam] < derived) {

              // See if another camera has calibrations at these two mags and if so
              // take use the difference in rotations to adjust rotation at mag2
              // But this will not work between GIF and nonGIF
              for (iAct2 = 0; iAct2 < numCam && !camP[iCam].STEMcamera; iAct2++) {
                iCam2 = mActiveCameraList[iAct2];
                if (iCam2 != iCam && !camP[iCam2].STEMcamera &&
                  (camP[iCam].GIF ? 1 : 0) == (camP[iCam2].GIF ? 1 : 0) &&
                  magT[mag2].rotDerived[iCam2] < derived &&
                  magT[iMag].rotDerived[iCam2] < derived) {
                  magT[iMag].rotation[iCam] = GoodAngle(magT[mag2].rotation[iCam] +
                    magT[iMag].rotation[iCam2] - magT[mag2].rotation[iCam2]);
                  magT[iMag].rotDerived[iCam] = derived;
                  SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f by rotation difference"
                    " to mag %d in cam %d", iMag, iCam, magT[iMag].rotation[iCam], mag2,
                    iCam2);
                  break;
                }
              }

              // Otherwise, fall back to difference in nominal rotations
              if (magT[iMag].rotation[iCam] > 900.) {
                if (camP[iCam].STEMcamera)
                  magT[iMag].rotation[iCam] = magT[mag2].rotation[iCam];
                else
                  magT[iMag].rotation[iCam] = GoodAngle(magT[mag2].rotation[iCam] +
                  (camP[iCam].GIF ?
                    (magT[iMag].EFTEMtecnaiRotation - magT[mag2].EFTEMtecnaiRotation)
                    : (magT[iMag].tecnaiRotation - magT[mag2].tecnaiRotation)));
                magT[iMag].rotDerived[iCam] = derived + 1;
                SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f by nominal diff from mag %d"
                  , iMag, iCam, magT[iMag].rotation[iCam], mag2);

              }
              break;
            }
          }
        }
      }

      // Pick up from nearest mag if nothing was in the mag range
      for (iMag = limlo; iMag <= limhi; iMag++) {
        if (magT[iMag].rotation[iCam] < 900. || !MagForCamera(iCam, iMag))
          continue;
        for (delta = 1; delta < MAX_MAGS && magT[iMag].rotation[iCam] > 900.; delta++) {
          for (iDir = -1; iDir <= 1; iDir += 2) {
            mag2 = iMag + delta * iDir;
            if (magT[mag2].rotDerived[iCam] < derived && MagForCamera(iCam, mag2)) {
              if (camP[iCam].STEMcamera)
                magT[iMag].rotation[iCam] = magT[mag2].rotation[iCam];
              else
                magT[iMag].rotation[iCam] = GoodAngle(magT[mag2].rotation[iCam] +
                (camP[iCam].GIF ?
                  (magT[iMag].EFTEMtecnaiRotation - magT[mag2].EFTEMtecnaiRotation)
                  : (magT[iMag].tecnaiRotation - magT[mag2].tecnaiRotation)));
              magT[iMag].rotDerived[iCam] = derived + 1;
              SEMTrace('c', "Mag %d Cam %d  Rotation = %.1f by nominal diff from mag %d"
                , iMag, iCam, magT[iMag].rotation[iCam], mag2);

            }
            break;
          }
        }
      }
    }
  }
}


// Finds a mag for assigning a fallback pixel size or rotation angle to that is
// in the middle of the image shift calibrated range (in M mode) for a nonGIF
// camera if possible.
void CShiftManager::PickMagForFallback(int actCamToDo, int & calMag, int & regCam)
{
  int iAct, iCam, iMag, gifCam, midMag, minWithIS, maxWithIS, gifMin = 0, gifMax = 0;
  int regMax, stemMin, stemMax, limlo, limhi, numRanges, lowestMicro, range, stemCam = -1;
  int regMin, midFilmMag = 15000;
  int numCam = mWinApp->GetNumReadInCameras();
  int lowestMmag = mScope->GetLowestMModeMagInd();
  BOOL anyCal = false;
  int actStart = actCamToDo < 0 ? 0 : actCamToDo;
  int actEnd = actCamToDo < 0 ? numCam - 1 : actCamToDo;
  gifCam = -1;
  regCam = -1;
  for (iAct = actStart; iAct <= actEnd; iAct++) {
    iCam = mActiveCameraList[iAct];

    // Find min and max mag with IS cal and closest mag to mid-range mag

    if (!mCamParams[iCam].STEMcamera) {
      minWithIS = 200;
      maxWithIS = 0;
      midMag = lowestMmag;
      for (iMag = lowestMmag; iMag < MAX_MAGS; iMag++) {
        if (mMagTab[iMag].matIS[iCam].xpx) {
          minWithIS = B3DMIN(minWithIS, iMag);
          maxWithIS = B3DMAX(maxWithIS, iMag);
        }
        if (fabs((double)(MagForCamera(iCam, iMag) - midFilmMag))
          < fabs((double)(MagForCamera(iCam, midMag) - midFilmMag)))
          midMag = iMag;
      }

      // Default it to mid-range mag if none; keep track of whether any regular
      if (minWithIS > maxWithIS)
        minWithIS = maxWithIS = midMag;
      else if (!mCamParams[iCam].GIF)
        anyCal = true;

      // Keep track of camera with biggest range for GIF and nonGIF
      if (mCamParams[iCam].GIF) {
        if (gifCam < 0 || maxWithIS - minWithIS > gifMax - gifMin) {
          gifCam = iCam;
          gifMin = minWithIS;
          gifMax = maxWithIS;
        }
      } else if (regCam < 0 || maxWithIS - minWithIS > regMax - regMin) {
        regCam = iCam;
        regMin = minWithIS;
        regMax = maxWithIS;
      }

    } else {

      // STEM camera
      mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
      for (range = 0; range < numRanges; range++) {
        minWithIS = 200;
        maxWithIS = 0;
        mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
        midMag = mScope->GetLowestSTEMnonLMmag(range);
        for (iMag = midMag; iMag < limhi; iMag++) {
          if (mMagTab[iMag].matIS[iCam].xpx) {
            minWithIS = B3DMIN(minWithIS, iMag);
            maxWithIS = B3DMAX(maxWithIS, iMag);
          }
        }

        if (minWithIS > maxWithIS)
          minWithIS = maxWithIS = midMag;
        if (stemCam < 0 || maxWithIS - minWithIS > stemMax - stemMin) {
          stemCam = iCam;
          stemMin = minWithIS;
          stemMax = maxWithIS;
       }
      }
    }
  }

  // Use GIF camera if it is the only one or if it is the only one with IS cals
  if (regCam < 0 || (gifCam >= 0 && !anyCal && gifMax > gifMin)) {
    regCam = gifCam;
    regMin = gifMin;
    regMax = gifMax;
  }

  if (regCam < 0 && stemCam >= 0) {
    regCam = stemCam;
    regMin = stemMin;
    regMax = stemMax;
  }

  // Get mag in midpoint of this big IS range
  iMag = (regMin + regMax) / 2;
  calMag = NearestIScalMag(iMag, regCam, true);
  if (calMag < 0)
    calMag = iMag;
}

// Prints the warnings about problems with rotations
int CShiftManager::ReportFallbackRotations(int onlyAbs)
{
  int iAct, iCam, ind = 0, retval = 0;
  CString str, str2;
  if (!mAnyAbsRotCal || mCamWithRotFallback.size() > 0) {
    str2 = "WARNING: There is no calibrated absolute rotation information for ";
    if (mAnyAbsSTEMrotCal)
      str2 += "any non-STEM camera";
    else if (!mAnyAbsRotCal)
      str2 += "any camera";
    else {
      ind = 0;
      for (iAct = 0; iAct < mWinApp->GetNumActiveCameras(); iAct++) {
        iCam = mActiveCameraList[iAct];
        if (mCamWithRotFallback.count(iCam)) {
          if (ind++)
            str2 += " or ";
          str2 += mCamParams[iCam].name;
        }
      }
    }
    if (!mAnyAbsRotCal || ind > 0) {
      if (mAnyAbsSTEMrotCal)
        str2 += ".\r\n  All non-STEM";
      else
        str2 += ".\r\n  All";
      mWinApp->AppendToLog(str2 + " rotations will rely on GlobalExtraRotation "
        "and ExtraRotation properties", LOG_SWALLOW_IF_CLOSED);
      retval = 1;
    }
  }
  if (onlyAbs)
    return retval;
  for (iAct = 0; iAct < mWinApp->GetNumActiveCameras(); iAct++) {
    iCam = mActiveCameraList[iAct];
    if (mMagsWithRotFallback[iCam].size() > 0) {
      str = "   ";
      mWinApp->AppendToLog("WARNING: camera " + mCamParams[iCam].name + " is missing some"
        " information on calibrated relative rotations.\r\n  Relative rotations will rely"
        " on differences between nominal mag table rotations for mags:",
        LOG_SWALLOW_IF_CLOSED);
      for (ind = 0; ind < (int)mMagsWithRotFallback[iCam].size(); ind++) {
        str2.Format(" %d", mMagsWithRotFallback[iCam][ind]);
        str += str2;
        if (str.GetLength() > 90) {
          mWinApp->AppendToLog(str, LOG_SWALLOW_IF_CLOSED);
          str = "   ";
        }
      }
      if (str.GetLength() > 4)
        mWinApp->AppendToLog(str, LOG_SWALLOW_IF_CLOSED);
      retval = 1;
    }
  }
  return retval;
}

// Compares stage cal matrices derived multippe ways and reports discrepancies
int CShiftManager::CheckStageToCamConsistency(float rotCrit, float magCrit, bool debug)
{
  int iAct, iCam, ind, direct, loop, retval = 0;
  float theta, smag, phi, strtch;
  bool anyForCam;
  CameraParameters *camParams = mWinApp->GetCamParams();
  CString str, saveKeys = mWinApp->GetDebugKeys();
  ScaleMat fromIS, fromSpec, invProd;
  for (iAct = 0; iAct < mWinApp->GetNumActiveCameras(); iAct++) {
    iCam = mActiveCameraList[iAct];
    for (direct = 0; direct < 2; direct++) {
      anyForCam = false;
      for (ind = 0; ind < MAX_MAGS; ind++) {

        // Check any mag where there is either a defined pixel size, or absolute or relative
        // rotation value
        if (mMagTab[ind].mag && BOOL_EQUIV(mMagTab[ind].matStage[iCam].xpx != 0., direct >
          0) && (!mMagTab[ind].pixDerived[iCam] ||
            mMagTab[ind].rotDerived[iCam] <= (mAnyAbsRotCal ? 1 : 2))) {
          fromIS = StageToCamera(iCam, ind, 2 * direct);
          fromSpec = StageToCamera(iCam, ind, 1 + 2 * direct);
          if (fromIS.xpx && fromSpec.xpx) {
            for (loop = 0; loop <= direct; loop++) {
              if (direct) {
                if (!loop && fromIS.xpx == fromSpec.xpx && fromIS.xpy == fromSpec.xpy &&
                  fromIS.ypx == fromSpec.ypx && fromIS.ypy == fromSpec.ypy)
                  continue;
                invProd = MatMul(loop ? fromSpec : fromIS,
                  MatInv(mMagTab[ind].matStage[iCam]));
              } else
                invProd = MatMul(fromIS, MatInv(fromSpec));
              amatToRotmagstr(invProd.xpx, invProd.xpy, invProd.ypx, invProd.ypy, &theta,
                &smag, &strtch, &phi);
              if (fabs(theta) > rotCrit || fabs(smag - 1.) > magCrit ||
                fabs(strtch - 1.) > magCrit || strtch < 0) {
                if (!anyForCam) {
                  if (direct)
                    mWinApp->AppendToLog("\r\nThere are inconsistencies between the stage"
                      " calibration done at a mag and one derived\r\n from another mag "
                      "using image shift or rotation and pixel values for camera " +
                      mCamParams[iCam].name);
                  else
                    mWinApp->AppendToLog("\r\nThere are inconsistencies between stage to "
                      "camera matrices derived from image shift cals\r\n and ones derived"
                      " from rotation and pixel information for camera " + 
                      mCamParams[iCam].name);
                  mWinApp->AppendToLog("Index Mag   rotation  scaling   stretch  between "
                    "matrices");
                  anyForCam = true;
                }
                str.Format("%3d %8d  %7.1f  %6.2f  %6.2f", ind, MagForCamera(iCam, ind),
                  theta, smag, strtch);
                if (direct)
                  str += loop ? "  (derived from rotation/pixel)" : 
                  "  (derived from IS cals)";
                mWinApp->AppendToLog(str);
                if (debug) {
                  mWinApp->SetDebugOutput("c");
                  if (direct) {
                    PrintfToLog("Calibrated matrix  %.5g  %.5g  %.5g  %.5g", 
                      mMagTab[ind].matStage[iCam].xpx, mMagTab[ind].matStage[iCam].xpy,
                      mMagTab[ind].matStage[iCam].ypx, mMagTab[ind].matStage[iCam].ypy);
                      StageToCamera(iCam, ind, loop + 2);
                  } else {
                    StageToCamera(iCam, ind, 0);
                    StageToCamera(iCam, ind, 1);
                  }
                  mWinApp->SetDebugOutput(saveKeys);
                }
                retval++;
              }
            }
          }
        }
      }
    }
  }
  return retval;
}

// Use image shift matrices to transfer rotation angle from one mag to another
double CShiftManager::TransferImageRotation(double fromAngle, int fromCam, int fromMag,
                      int toCam, int toMag)
{
  ScaleMat aInv = MatInv(mMagTab[fromMag].matIS[fromCam]);
  ScaleMat aProd = MatMul(aInv, mMagTab[toMag].matIS[toCam]);
  double xtheta = atan2(aProd.ypx, aProd.xpx) / DTOR;
  double ytheta = atan2(-aProd.xpy, aProd.ypy) / DTOR;
  double ydtheta = GoodAngle(ytheta - xtheta);
  
  xtheta = GoodAngle(fromAngle + (xtheta + 0.5 * ydtheta));
  return xtheta;
}

// Get an image rotation based on calibrated information of some kind
// Note the treatment of relative angles is different from the propagation method
// Here it assigns an angle from the closest linked absolute angle
float CShiftManager::GetCalibratedImageRotation(int inCamera, int inMagIndex)
{
  float backAngle = 999.f;
  float forAngle = 999.f;
  float cumAngle = 0.f;
  int iBack, iFor, limlo, limhi;

  mWinApp->GetMagRangeLimits(inCamera, inMagIndex, limlo, limhi);

  // If a measured absolute angle was entered, return it
  if (mMagTab[inMagIndex].rotation[inCamera] < 900. &&
    !mMagTab[inMagIndex].rotDerived[inCamera])
    return mMagTab[inMagIndex].rotation[inCamera];

  // Search back for a link to a calibrated angle
  for (iBack = inMagIndex - 1; iBack >= limlo; iBack--) {

    // Accumulate delta angles to the current mag, but stop if none
    if (mMagTab[iBack + 1].deltaRotation[inCamera] < 900.)
      cumAngle += mMagTab[iBack + 1].deltaRotation[inCamera];
    else
      break;

    // If find a measured angle, compute backAngle and stop
    if (mMagTab[iBack].rotation[inCamera] < 900. &&
      !mMagTab[iBack].rotDerived[inCamera]) {
      backAngle = mMagTab[iBack].rotation[inCamera] + cumAngle;
      break;
    }
  }

  // Search forward similarly
  cumAngle = 0.f;
  for (iFor = inMagIndex + 1; iFor <= limhi; iFor++) {
    if (mMagTab[iFor].deltaRotation[inCamera] < 900.)
      cumAngle += mMagTab[iFor].deltaRotation[inCamera];
    else
      break;

    if (mMagTab[iFor].rotDerived[inCamera] < 900. && 
      !mMagTab[iFor].rotDerived[inCamera]) {
      backAngle = mMagTab[iFor].rotation[inCamera] - cumAngle;
      break;
    }
  }

  // Take closest result if both exist
  if (backAngle < 900.) {
    if (forAngle < 900.) {
      if (inMagIndex - iBack > iFor - inMagIndex)
        return forAngle;
      else
        return backAngle;
    } else
      return backAngle;
  } else if (forAngle < 900.)
    return forAngle;

  return 999.f;
}

// Adjust the image shift matrix from one mag/camera combination to the desired one
ScaleMat CShiftManager::AdjustedISmatrix(int iCamCal, int iMagCal, int iCamWant, 
                     int iMagWant)
{
  ScaleMat bCal, aCal, aInv, aWant, prod1, prod2;
  bCal = mMagTab[iMagCal].matIS[iCamCal];
  aCal = SpecimenToCamera(iCamCal, iMagCal);
  aInv = MatInv(aCal);
  prod1 = MatMul(bCal, aInv);
  aWant = SpecimenToCamera(iCamWant, iMagWant);
  prod2 = MatMul(prod1, aWant);
  return prod2;
}

// Returns whether IS values are consistent between the two mags
BOOL CShiftManager::CanTransferIS(int magFrom, int magTo, BOOL STEMcamera, int GIFcamera)
{
  int i, mag;
  if (!mScope)
  	return false;
  if (GIFcamera < 0)
    GIFcamera = mCamParams[mWinApp->GetCurrentCamera()].GIF ? 1 : 0;
  int *boundaries = mScope->GetShiftBoundaries(GIFcamera);

  // For a STEM camera, check each LM-M boundary  (JEOL????)
  if (STEMcamera) {
    for (i = 0; i < (FEIscope ? 2 : 1) ; i++) {
      mag = mScope->GetLowestSTEMnonLMmag(i);
      if ((magFrom < mag && magTo >= mag) || (magFrom >= mag && magTo < mag))
        return false;
    }
    return true;
  }

  // Check each boundary and return false if mags cross a boundary
  for (i = 0; i < mScope->GetNumShiftBoundaries(); i++) {
    mag = boundaries[i];
    if ((magFrom < mag && magTo >= mag) || (magFrom >= mag && magTo < mag))
      return false;
  }
  return true;
}

// Returns whether two mags are separated by a beam shift boundary
bool CShiftManager::CrossesBeamShiftBoundary(int mag1, int mag2)
{
  int ind;
  for (ind = 0; ind < (int)mBeamShiftBoundaries.size(); ind++)
    if ((mag1 < mBeamShiftBoundaries[ind] && mag2 >= mBeamShiftBoundaries[ind]) ||
      (mag1 >= mBeamShiftBoundaries[ind] && mag2 < mBeamShiftBoundaries[ind]))
      return true;
  return false;
}

// Transfer a general image shift from one mag to another; toCam is optional
void CShiftManager::TransferGeneralIS(int fromMag, double fromX, double fromY, int toMag,
                                    double &toX, double &toY, int toCam)
{
  ScaleMat cMat, cTo, cProd;
  static int lastFrom = 0, lastTo = 0;
  static double lastX = 0., lastY = 0.;
  bool diffFromLast = fromMag != lastFrom || toMag != lastTo || fromX != lastX || 
    fromY != lastY;
  int iCam = mWinApp->GetCurrentCamera();
  toX = toY = 0;
  if (!fromMag || !toMag)
    return;
  lastX = fromX;
  lastY = fromY;
  lastFrom = fromMag;
  lastTo = toMag;
  toX = fromX;
  toY = fromY;
  if (toMag == fromMag || CanTransferIS(toMag, fromMag, mCamParams[iCam].STEMcamera,
    mCamParams[iCam].GIF ? 1 : 0)) {
    if (diffFromLast)
      SEMTrace('l', "TransferGeneralIS %d to %d direct copy", fromMag, toMag);
    return;
  } else {
    cMat = IStoSpecimen(fromMag);
    cTo = IStoSpecimen(toMag, toCam);
    if (!cTo.xpx || !cMat.xpx)
      return;
    cProd = MatMul(cMat, MatInv(cTo));
    toX = cProd.xpx * fromX + cProd.xpy * fromY;
    toY = cProd.ypx * fromX + cProd.ypy * fromY;
    if (diffFromLast)
      SEMTrace('l', "TransferGeneralIS %d to %d (cam %d to %d): %f %f  to %f %f", fromMag,
      toMag, iCam, toCam,  fromX, fromY, toX, toY);
  }
}

// Find the nearest mag with calibrated image shift; look across image shift
// boundaries if crossBorders is true
int CShiftManager::NearestIScalMag(int inMag, int iCam, BOOL crossBorders)
{
  int iDir, delta, iMag, limlo, limhi;
  mWinApp->GetMagRangeLimits(iCam, inMag, limlo, limhi);
  if (mMagTab[inMag].matIS[iCam].xpx)
    return inMag;
  for (delta = 1; delta < MAX_MAGS; delta++) {
    for (iDir = -1; iDir <= 1; iDir += 2) {
      iMag = inMag + delta * iDir;
      if (iMag < limlo || iMag > limhi)
        continue;
      if (mMagTab[iMag].matIS[iCam].xpx && 
        (CanTransferIS(iMag, inMag, mCamParams[iCam].STEMcamera, 
          mCamParams[iCam].GIF ? 1 : 0) || crossBorders))
        return iMag;
    }
  }
  return -1;
}

// Add a beam shift calibration at the given mag; pass a negative mag for JEOL EFTEM mode
void CShiftManager::SetBeamShiftCal(ScaleMat inMat, int inMag, int inAlpha, int inProbe,
  int retain)
{
  int i, j, mag, ind, calMag, sign;
  int *boundaries = mWinApp->mScope->GetShiftBoundaries();
  int numBound = mWinApp->mScope->GetNumShiftBoundaries();
  ind = -1;

  // Treat negative mags as a separate set of mags (JEOL EFTEM mode)
  sign = inMag < 0 ? -1 : 1;

  // Look at each existing beam shift cal and see if it is on same side of all
  // boundaries as the new mag
  for (j = 0; j < mNumBeamShiftCals; j++) {
    calMag = mBeamCalMagInd[j];
    if ((calMag < 0 ? -1 : 1) != sign)
      continue;
    if (mBeamCalProbe[j] != inProbe)
      continue;

    // Alpha test: replace only if old one has no alpha or the alphas match
    // This is somewhat flawed as it may arbitrarily pick one of several to replace
    if (!(mBeamCalAlpha[j] < 0 || mBeamCalAlpha[j] == inAlpha))
      continue;
    if (mBeamCalRetain[j] && calMag != inMag)
      continue;
    if (CrossesBeamShiftBoundary(sign * calMag, sign * inMag))
      continue;
    for (i = 0; i < numBound; i++) {
      mag = boundaries[i];
      if ((sign * calMag < mag && sign * inMag >= mag) || 
        (sign * calMag >= mag && sign * inMag < mag))
        break;
    }

    // If got through loop, then on same side as new mag; replace this calibration
    // in this case or if there was no mag index at all
    if (!calMag || i == numBound) {
      ind = j;
      break;
    }
  }

  if (ind < 0) {
    ind = mNumBeamShiftCals++;
    mBeamCalMagInd.push_back(inMag);
    mBeamCalAlpha.push_back(inAlpha);
    mBeamCalProbe.push_back(inProbe);
    mBeamCalRetain.push_back(retain);
  } else {
    mBeamCalMagInd[ind] = inMag;
    mBeamCalAlpha[ind] = inAlpha;
    mBeamCalProbe[ind] = inProbe;
    mBeamCalRetain[ind] = retain;
  }
  mIStoBS[ind] = inMat;
}

// Get a beam shift calibration for the given mag and alpha (default -3333, means use
// FastAlpha value)
ScaleMat CShiftManager::GetBeamShiftCal(int magInd, int inAlpha, int inProbe)
{
  int j, dir, diff, calMag, loop, skipBound, numBoundLoop, numLoop, calSign = 1;
  int bestJ = -1, bestMag;
  ScaleMat mat, mat2, currIStoSpec, calSpecToIS;
  BOOL EFTEM = mWinApp->GetEFTEMMode();
  bool EFTEMnonLM = FEIscope && EFTEM && magInd >= mScope->GetLowestMModeMagInd(true);
  mat.xpx = 0.;
  if (!mNumBeamShiftCals)
    return mat;

  currIStoSpec = IStoSpecimen(magInd);
  if (inAlpha < -1000)
    inAlpha = mScope->FastAlpha();
  if (inProbe < 0)
    inProbe = mScope->GetProbeMode();
  numLoop = inAlpha < 0 ? 1 : 2;
  numBoundLoop = mBeamShiftBoundaries.size() > 0 ? 2 : 1;

  // Access only ones with negative mag index for JEOL EFTEM mode
  if (JEOLscope && !mScope->GetHasOmegaFilter() && EFTEM)
    calSign = -1;

  // Loop through this twice if alpha; first try to match alpha
  // First look through calibrations, and if there is one that can transfer IS, use its
  // matrix directly; then fall back to specimen transformations 
  for (loop = 1; loop <= numLoop; loop++) {

    // And loop through this twice if there are boundaries: first obey the boundaries,
    // then ignore them
    for (skipBound = 0; skipBound < numBoundLoop; skipBound++) {
      for (j = 0; j < mNumBeamShiftCals; j++) {
        calMag = calSign * mBeamCalMagInd[j];
        if (calMag < 0)
          continue;
        if (inProbe != mBeamCalProbe[j])
          continue;

        // If it not the last time through the loop, require a matching alpha
        if (loop < numLoop && mBeamCalAlpha[j] != inAlpha)
          continue;

        // Assume old calibration is from M mode range
        if (!calMag)
          calMag = mScope->GetLowestMModeMagInd();
        if (EFTEMnonLM && calMag < mScope->GetLowestMModeMagInd(false))
          continue;
        if (CanTransferIS(calMag, magInd) &&
          (skipBound || !CrossesBeamShiftBoundary(calMag, magInd))) {
          if (bestJ < 0 || B3DABS(calMag - magInd) < B3DABS(bestMag - magInd)) {
            bestJ = j;
            bestMag = calMag;
            if (bestMag == magInd)
              break;
          }
        }
      }
      if (bestJ >= 0) {
        if (JEOLscope)
          SEMTrace('c', "For mag %d alpha %d returning BS cal from mag %d alpha %d",
            magInd, inAlpha + 1, bestMag, mBeamCalAlpha[bestJ] + 1);
        else
          SEMTrace('c', "For mag %d %s returning BS cal from mag %d %s",
            magInd, inProbe ? "micro" : "nano", bestMag, mBeamCalProbe[bestJ] ?
            "micro" : "nano");
        return mIStoBS[bestJ];
      }

      // Must have a IS to specimen transformation for this mag for next loops
      if (!currIStoSpec.xpx)
        continue;

      // Look at calibrations, from closest to farther mags, and find one with an
      // IS to specimen transformation and use it to transform the calibrated BS matrix
      // But loop twice if alpha; first try to match alpha
      for (diff = 1; diff < MAX_MAGS; diff++) {
        for (dir = 1; dir >= -1; dir -= 2) {
          for (j = 0; j < mNumBeamShiftCals; j++) {
            calMag = calSign * mBeamCalMagInd[j];
            if (calMag < 0)
              continue;
            if (inProbe != mBeamCalProbe[j])
              continue;
            if (loop < numLoop && mBeamCalAlpha[j] != inAlpha)
              continue;
            if (!calMag)
              calMag = mScope->GetLowestMModeMagInd();
            if (!skipBound && CrossesBeamShiftBoundary(calMag, magInd))
              continue;

            // Check a mag only if it is on the same side of LM-M as given mag
            if (EFTEMnonLM && calMag < mScope->GetLowestMModeMagInd(false))
              continue;
            if (magInd + dir * diff == calMag &&
              mScope->BothLMorNotLM(calMag, false, magInd, false)) {
              mat2 = IStoSpecimen(calMag);
              if (mat2.xpx) {
                calSpecToIS = MatInv(mat2);
                mat2 = MatMul(calSpecToIS, mIStoBS[j]);
                mat = MatMul(currIStoSpec, mat2);
                SEMTrace('c', "For mag %d  alpha %d using specimen transfer of BS cal"
                    " from mag %d alpha %d", magInd, inAlpha + 1, calMag, 
                  mBeamCalAlpha[j] + 1);
                return mat;
              }
            }
          }
        }
      }
    }
  }
  
  return mat;
}

// List the beam shift calibrations
void CShiftManager::ListBeamShiftCals()
{
  int ind, jnd, iCam = mWinApp->GetCurrentCamera(), useMag;
  CString str1, str2;
  ScaleMat prod, specToIS;
  IntVec calInds;
  bool hasAlpha = JEOLscope && !mScope->GetHasNoAlpha();
  if (!mNumBeamShiftCals)
    return;

  // Sort on alpha/probe then on mag within alpha/probe
  for (ind = 0; ind < mNumBeamShiftCals; ind++)
    calInds.push_back(ind);
  for (ind = 0; ind < mNumBeamShiftCals - 1; ind++) {
    for (jnd = ind + 1; jnd < mNumBeamShiftCals; jnd++) {
      if ((hasAlpha &&
        mBeamCalAlpha[calInds[ind]] > mBeamCalAlpha[calInds[jnd]]) ||
        (FEIscope && mBeamCalProbe[calInds[ind]] > mBeamCalProbe[calInds[jnd]]))
        B3DSWAP(calInds[ind], calInds[jnd], useMag);
    }
  }
  for (ind = 0; ind < mNumBeamShiftCals - 1; ind++) {
    for (jnd = ind + 1; jnd < mNumBeamShiftCals; jnd++) {
      if ((HitachiScope || (JEOLscope && (!hasAlpha ||
        mBeamCalAlpha[calInds[ind]] == mBeamCalAlpha[calInds[jnd]])) ||
        (FEIscope && mBeamCalProbe[calInds[ind]] == mBeamCalProbe[calInds[jnd]])) &&
        mBeamCalMagInd[calInds[ind]] > mBeamCalMagInd[calInds[jnd]]) {
        B3DSWAP(calInds[ind], calInds[jnd], useMag);
      }
    }
  }

  mWinApp->SetNextLogColorStyle(0, 1);
  mWinApp->AppendToLog("\r\nBeam shift calibrations as specimen to beam shift matrices:");
  str1 = "Ind ";
  if (FEIscope)
    str1 += "Probe";
  else if (hasAlpha)
    str1 += "Alpha";
  mWinApp->SetNextLogColorStyle(0, 4);
  mWinApp->AppendToLog(str1 + "    Matrix                                        Mag"
    + CString(JEOLscope ? "  Refined" : ""));
  for (jnd = 0; jnd < mNumBeamShiftCals; jnd++) {
    ind = calInds[jnd];
    str1.Format("%2d  ", mBeamCalMagInd[ind]);
    str2 = "";
    if (FEIscope)
      str2 = mBeamCalProbe[ind] ? "   uP   " : "   nP   ";
    else if (hasAlpha)
      str2.Format("    %d   ", mBeamCalAlpha[ind] + 1);
    str1 += str2;

    // JEOL GIF calibrations are at negative mag indexes
    useMag = B3DABS(mBeamCalMagInd[ind]);
    specToIS = MatInv(IStoSpecimen(useMag));
    if (specToIS.xpx) {
      prod = MatMul(specToIS, mIStoBS[ind]);
      str2.Format(" %9.3f %9.3f %9.3f %9.3f  %12d", prod.xpx, prod.xpy, prod.ypx, 
        prod.ypy, mMagTab[useMag].mag);
      if (JEOLscope)
        str2 += mBeamCalRetain[ind] ? "   1" : "   0";
    } else {
      str2.Format(" No image shift to specimen matrix available  %12d",
        MagForCamera(iCam, useMag));
    }
    str1 += str2;
    mWinApp->AppendToLog(str1);
  }
}

// Convert a beam shift at the given mag to a specimen shift, return true if possible
bool CShiftManager::BeamShiftToSpecimenShift(ScaleMat & IStoBS, int magInd,
  double beamDelX, double beamDelY, float & specX, float & specY)
{
  ScaleMat BStoIS, IStoSpec, BStoSpec;
  specX = specY = 0;
  if (IStoBS.xpx && magInd) {
    BStoIS = MatInv(IStoBS);
    IStoSpec = IStoSpecimen(magInd);
    if (IStoSpec.xpx) {
      BStoSpec = MatMul(BStoIS, IStoSpec);
      ApplyScaleMatrix(BStoSpec, beamDelX, beamDelY, specX, specY);
      return true;
    }
  }
  return false;
}

// Returns factors needed to adjust a camera or IS to stage matrix for stage tilt, the
// return value is the tilt angle
double CShiftManager::GetStageTiltFactors(float & xTiltFac, float & yTiltFac)
{
  double angle = mScope->GetTiltAngle();
  float cosa = cos(DTOR * angle);
  xTiltFac = (HitachiScope ? cosa : 1.f);
  yTiltFac = (HitachiScope ? 1.f : cosa);
  return angle;
}

// Given any angle, returns a value between -180 and 180.
double CShiftManager::GoodAngle(double angle)
{
  return UtilGoodAngle(angle);
}

// Multiply two matrices: aa is matrix applied first
ScaleMat MatMul(ScaleMat aa, ScaleMat bb)
{
  ScaleMat cc = {0., 0., 0., 0.};;
  if (!aa.xpx || !bb.xpx)
    return cc;
  cc.xpx = bb.xpx * aa.xpx + bb.xpy * aa.ypx;
  cc.xpy = bb.xpx * aa.xpy + bb.xpy * aa.ypy;
  cc.ypx = bb.ypx * aa.xpx + bb.ypy * aa.ypx;
  cc.ypy = bb.ypx * aa.xpy + bb.ypy * aa.ypy;
  return cc;
}

// Inverts a matrix, with optional parameter to do it for a matrix that operates between
// left-handed and right-handed coordinates
ScaleMat MatInv(ScaleMat aa, bool yInverted)
{
  ScaleMat inv;
  float ySign = yInverted ? -1.f : 1.f;
  float det = ySign * (aa.xpx * aa.ypy - aa.xpy * aa.ypx);
  inv.xpx = 0.;
  if (!aa.xpx)
    return inv;
  inv.xpx = aa.ypy * ySign / det;
  inv.xpy = -aa.xpy / det;
  inv.ypx = -aa.ypx / det;
  inv.ypy = ySign * aa.xpx / det;
  return inv;
}

ScaleMat CShiftManager::MatMul(ScaleMat aa, ScaleMat bb)
{
  return ::MatMul(aa, bb);
}

ScaleMat CShiftManager::MatInv(ScaleMat aa)
{
  return ::MatInv(aa);
}

// Apply a scale matrix to x and Y values, adding to existing values if incremental is set
// (default false).  Set testXpx to false for simple 90-degree matrix!
// Double/float to float must have separate output from input
void ApplyScaleMatrix(ScaleMat &mat, double xFrom, double yFrom,
  float &xTo, float &yTo, bool incremental, bool testXpx)
{
  if (!incremental)
    xTo = yTo = 0.;
  if (testXpx && !mat.xpx)
    return;
  xTo += (float)(mat.xpx * xFrom + mat.xpy * yFrom);
  yTo += (float)(mat.ypx * xFrom + mat.ypy * yFrom);
}

// Double/float to double can have the output the same as the input
void ApplyScaleMatrix(ScaleMat &mat, double xFrom, double yFrom,
  double &xTo, double &yTo, bool incremental, bool testXpx)
{
  float fxTo = (float)xTo;
  float fyTo = (float)yTo;
  ApplyScaleMatrix(mat, xFrom, yFrom, fxTo, fyTo, incremental, testXpx);
  xTo = fxTo;
  yTo = fyTo;
}

void CShiftManager::ApplyScaleMatrix(ScaleMat &mat, double xFrom, double yFrom,
  float &xTo, float &yTo, bool incremental, bool testXpx)
{
  ::ApplyScaleMatrix(mat, xFrom, yFrom, xTo, yTo, incremental, testXpx);
}

void CShiftManager::ApplyScaleMatrix(ScaleMat &mat, double xFrom, double yFrom,
  double &xTo, double &yTo, bool incremental, bool testXpx)
{
  ::ApplyScaleMatrix(mat, xFrom, yFrom, xTo, yTo, incremental, testXpx);
}

// Scales and rotates a matrix
ScaleMat CShiftManager::MatScaleRotate(ScaleMat aMat, float scale, float rotation)
{
  ScaleMat rotMat;
  rotMat.xpx = rotMat.ypy = cos(rotation * DTOR) * scale;
  rotMat.xpy = -sin(rotation * DTOR) * scale;
  rotMat.ypx = -rotMat.xpy;
  return MatMul(aMat, rotMat);
}

// Sets up an IMOD-style transform qith scale, rotation, and shift
void CShiftManager::MakeScaleRotTransXform(float xf[6], float scale, float rot, 
  float dx, float dy)
{
  xf[0] = scale * (float)cos(rot * DTOR);
  xf[2] = -scale * (float)sin(rot * DTOR);
  xf[1] = -xf[2];
  xf[3] = xf[0];
  xf[4] = dx;
  xf[5] = dy;
}

// Sets up an IMOD-style transform from a ScaleMat and delta values.
void CShiftManager::ScaleMatToIMODxform(ScaleMat mat, float delx, float dely, float xf[6])
{
  xf[0] = mat.xpx;
  xf[2] = mat.xpy;
  xf[1] = mat.ypx;
  xf[3] = mat.ypy;
  xf[4] = delx;
  xf[5] = dely;
}

// Returns a ScaleMat and delta values from an IMOD-style transform
ScaleMat CShiftManager::IMODxformToScaleMat(float xf[6], float &delx, float &dely)
{
  ScaleMat mat;
  mat.xpx = xf[0];
  mat.xpy = xf[2];
  mat.ypx = xf[1];
  mat.ypy = xf[3];
  delx = xf[4];
  dely = xf[5];
  return mat;
}

////////////////////////////////////////////////////////////////////
// Image shift checking and timing routines
////////////////////////////////////////////////////////////////////

// Verify that image shift is OK given current limits
BOOL CShiftManager::ImageShiftIsOK(double newX, double newY, BOOL incremental)
{
  double oldX, oldY;
  ScaleMat cMat;
  float specX, specY;
  int magInd = mScope->FastMagIndex();
  float limit = magInd >= mScope->GetLowestNonLMmag() 
    ? mRegularShiftLimit : mLowMagShiftLimit;
  
  if (incremental) {
    mScope->FastImageShift(oldX, oldY);
    newX += oldX;
    newY += oldY;
  }
  
  if (mUseSquareShiftLimits) {
    cMat = IStoSpecimen(magInd);
    if (cMat.xpx == 0.)
      return true;

    // Test specimen distance on each axis separately
    specX = newX * cMat.xpx;
    specY = newX * cMat.ypx;
    if (sqrt(specX * specX + specY * specY) >= limit)
      return false;
    specX = newY * cMat.xpy;
    specY = newY * cMat.ypy;
    return (sqrt(specX * specX + specY * specY) < limit);
  }

  // Compute distance in specimen plane and compare to its limit
  return (RadialShiftOnSpecimen(newX, newY, magInd) < limit);
}

// Compute the radial distance on specimen corresponding to an image shift
double CShiftManager::RadialShiftOnSpecimen(double inISX, double inISY, int inMagInd)
{
  ScaleMat cMat = IStoSpecimen(inMagInd);
  if (cMat.xpx == 0.)
    return 0.;
  double specX = inISX * cMat.xpx + inISY * cMat.xpy;
  double specY = inISX * cMat.ypx + inISY * cMat.ypy;
  return sqrt(specX * specX + specY * specY);
}

// Get the delay needed for an anticipated image shift
float CShiftManager::PredictISDelay(double ISX, double ISY)
{
  double delX, delY;
  mScope->GetImageShift(delX, delY);
  delX = ISX - delX;
  delY = ISY - delY;
  return ComputeISDelay(delX, delY);
}

// Get the delay needed for the last image shift
float CShiftManager::GetLastISDelay()
{
  double delX, delY;
  mScope->GetLastISChange(delX, delY);
  return ComputeISDelay(delX, delY);
}

// Get the delay needed for the given incremental image shift
float CShiftManager::ComputeISDelay(double delX, double delY)
{
  int i;
  int ind = mScope->FastMagIndex();
  float delay, frac;
  if (!delX && !delY)
    return 0.;

  // Convert each component to distance on the specimen separately
  ScaleMat aMat = IStoSpecimen(ind);
  if (aMat.xpx == 0.)
    return 2.5f;
  float delSpecX = (float)sqrt(pow(delX * aMat.xpx, 2) + pow(delX * aMat.ypx, 2));
  float delSpecY = (float)sqrt(pow(delY * aMat.xpy, 2) + pow(delY * aMat.ypy, 2));
  if (delSpecY > delSpecX)
    delSpecX = delSpecY;
  
  // In low mag mode the effective distance is much less because lower coil setting
  // is needed for a given movement
  if (ind < mScope->GetLowestMModeMagInd())
    delSpecX /= 10.;

  // Find first entry in table above this value
  for (i = 0; i < mNumISdelays - 2; i++) {
    if (delSpecX < mISmoved[i + 1])
      break;
  }

  // Interpolate/extrapolate from this segment
  if (mNumISdelays < 2) {
    delay = mISdelayNeeded[0];
  } else {
    frac = (delSpecX - mISmoved[i]) / (mISmoved[i + 1] - mISmoved[i]);
    delay = (1.f - frac) * mISdelayNeeded[i] + frac * mISdelayNeeded[i + 1];
  }
  if (delay < 0.)
    delay = 0.;

  // Increase delay logarithmically for smaller Record pixel sizes than 1 nm
  if (mDelayPerMagDoubling) {
    float pixel = GetPixelSize(mWinApp->GetCurrentCamera(), ind) * 
      mConSets[RECORD_CONSET].binning;
    delSpecX = 0.001 / pixel;
    if (delSpecX > 1.)
      delay += mDelayPerMagDoubling * log(delSpecX) / log(2.);
  }

  // For STEM, the delay just needs to be less.  This is about right when calibrating
  // IS on a Tecnai
  if (mWinApp->GetSTEMMode())
    delay /= 5.;
  return mISdelayScaleFactor * delay;

}

static void CheckTimeout(UINT value, const char * descrip)
{
  float crit = 300000.;

  // This gives interval from current time to value
  double interval = SEMTickInterval(value);
  if (interval < -crit)
    PrintfToLog("WARNING (please report): Timeout for %s is too long (%.2f sec, timeout"
      " %u, ticks %u)", descrip, -0.001 * interval, value, GetTickCount());
}

// Set the time out time to be present time plus the delay, but do not make
// it smaller than a current value.  If it is later than the current value, mark that it
// came from a true IS timeout
void CShiftManager::SetISTimeOut(float inDelay)
{
  UINT newTime = AddIntervalToTickTime(GetTickCount(), (int)(1000. * inDelay)) ;
  CheckTimeout(newTime, "IS Timeout");
  if (SEMTickInterval(newTime, mISTimeOut) > 0.) {
    mISTimeOut = newTime;
    mLastTimeoutWasIS = true;
  }
}

// Store a requested timeout time in the IS timeout variable but mark it as not an IS
void CShiftManager::SetGeneralTimeOut(UINT inTime)
{
  CheckTimeout(inTime, "general timeout");
  if (SEMTickInterval(mISTimeOut, inTime) < 0)
    mISTimeOut = inTime;
  mLastTimeoutWasIS = false;
}

void CShiftManager::SetGeneralTimeOut(UINT inTicks, int interval)
{
  CheckTimeout(inTicks, "general timeout before adding interval");
  SetGeneralTimeOut(AddIntervalToTickTime(inTicks, interval));
}

UINT CShiftManager::GetAdjustedTiltDelay(double tiltChange)
{
  double scaleFac = 1.;
  double baseInc = mScope->GetBaseIncrement();

  // Get the basic tilt delay from TS if doing one and not doing a task
  float tiltDelay = mTiltDelay;
  TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
  if (mWinApp->DoingTiltSeries() && !mWinApp->mComplexTasks->DoingTasks())
    tiltDelay = tsParam->tiltDelay;
  if (tiltChange > 0. && baseInc > 0)
    scaleFac = sqrt(tiltChange / baseInc);
  scaleFac = B3DMAX(0.33, B3DMIN(2., scaleFac));

  // 6/12/10: limit scale factor to 1 by a tilt delay of 8 (fix stupid bug from 4/09)
  if (scaleFac > 1.)
    scaleFac = 1. + B3DMIN(6., B3DMAX(0., (8. - tiltDelay))) * (scaleFac - 1.) / 6.;
  return (UINT)(1000. * scaleFac * tiltDelay);
}

UINT CShiftManager::GetGeneralTimeOut(int whichSet)
{
  // Calculate a tilt timeout for certain defined functions: Record shots, and
  // autofocusing and ???
  UINT tiltTimeOut = 0;
  if (whichSet != TRACK_CONSET &&
    (whichSet > 2 || mWinApp->mFocusManager->DoingFocus())) {
    tiltTimeOut = AddIntervalToTickTime(mScope->GetLastTiltTime(),
      GetAdjustedTiltDelay(mScope->GetLastTiltChange()));
    CheckTimeout(tiltTimeOut, "Tilt timeout with adjusted tilt delay");
  } else {
    tiltTimeOut = AddIntervalToTickTime(mScope->GetLastTiltTime(), mMinTiltDelay);
    CheckTimeout(tiltTimeOut, "Tilt timeout with min tilt delay");
  }

  // Get timeout since last normalization and take max
  UINT normTimeOut = GetNormalizationTimeOut(false);
  if (SEMTickInterval(normTimeOut, tiltTimeOut) < 0)
    normTimeOut = tiltTimeOut;

  // Compute a timeout since the last stage move and take max, based on a delay time that
  // Was reset to default at end of stage move and possibly changed by routine after that
  tiltTimeOut = AddIntervalToTickTime(mScope->GetLastStageTime(), mStageDelayToUse);
  CheckTimeout(tiltTimeOut, "Stage move timeout");
  if (SEMTickInterval(normTimeOut, tiltTimeOut) < 0)
    normTimeOut = tiltTimeOut;

  // Compare with the IS timeout and if it is later, mark as not being true IS timeout
  if (SEMTickInterval(normTimeOut, mISTimeOut) > 0)
    mLastTimeoutWasIS = false;
  return (SEMTickInterval(normTimeOut, mISTimeOut) > 0 ? normTimeOut : mISTimeOut);
}

// Compute a timeout since last normalization
UINT CShiftManager::GetNormalizationTimeOut(bool leavingLMalso)
{
  CameraParameters *camP = mWinApp->GetActiveCamParam();
  int normDelay = mNormalizationDelay;
  if (mLowMagNormDelay > 0 && mScope->GetLastNormMagIndex() > 0 &&
    (mScope->GetLastNormMagIndex() < mScope->GetLowestNonLMmag(camP) || 
      (leavingLMalso && mScope->GetPrevNormMagIndex() > 0 && 
        mScope->GetPrevNormMagIndex() < mScope->GetLowestNonLMmag(camP))))
    normDelay = mLowMagNormDelay;
  UINT normTimeOut = AddIntervalToTickTime(mScope->GetLastNormalization(), normDelay);
  CheckTimeout(normTimeOut, "normalization delay");
  return normTimeOut;
}

void CShiftManager::ResetAllTimeouts()
{
  DWORD reset = AddIntervalToTickTime(GetTickCount(), -60000);
  mISTimeOut = reset;
  mScope->SetLastNormalization(reset);
  mScope->SetLastTiltTime(reset);
  mScope->SetLastStageTime(reset);
}

// Add a millisecond interval to a tick count and get a valid tick count with wraparound
UINT CShiftManager::AddIntervalToTickTime(UINT ticks, int interval)
{
  double newTicks = (double)ticks + interval;
  while (newTicks < 0)
    newTicks += 4294967296.;
  while (newTicks >= 4294967296.)
    newTicks -= 4294967296.;
  return (UINT)newTicks;
}

// Return a matrix for rotating an image that is corrected for underlying stretch if one
// has been entered with properties
ScaleMat CShiftManager::StretchCorrectedRotation(int camera, int magInd, float rotation)
{
  ScaleMat mat, raMat, bb;
  float smagMean, str;
  double thetad, alpha;
  int dist, minDist = 1000;
  int lowestM = mScope->GetLowestNonLMmag(&mCamParams[camera]);
  RotStretchXform rotXform;

  // Get rotation matrix to start with
  mat.xpx = mat.ypy = cos(DTOR * rotation);
  mat.ypx = sin(DTOR * rotation);
  mat.xpy = -mat.ypx;
  if (camera < 0 || magInd <= 0)
    return mat;

  // Find nearest transform in the same mag range and camera
  for (int i = 0; i < mRotXforms.GetSize(); i++) {
    rotXform = mRotXforms[i];
    if (camera == rotXform.camera && ((magInd < lowestM && rotXform.magInd < lowestM) ||
      (magInd >= lowestM && rotXform.magInd >= lowestM))) {
      
      dist = magInd - rotXform.magInd;
      if (dist < 0)
        dist = -dist;
      if (dist < minDist) {
        minDist = dist;
        raMat = rotXform.mat;
      }
    }
  }
  if (minDist == 1000)
    return mat;

  bb = UnderlyingStretch(raMat, smagMean, thetad, str, alpha);

  // Modify rotation matrix by destretching, rotating, and restretching
  mat = MatMul(MatMul(MatInv(bb), mat), bb);
  return mat;
}

// Return a matrix for rotating stage coordinates that is corrected for underlying 
// stretch if a transformation matrix has been saved as a calibration
ScaleMat CShiftManager::StretchCorrectedRotation(float rotation)
{
  ScaleMat mat, bb;
  float smagMean, str;
  double thetad, alpha;

  // Get rotation matrix to start with
  mat.xpx = mat.ypy = cos(DTOR * rotation);
  mat.ypx = sin(DTOR * rotation);
  mat.xpy = -mat.ypx;

  if (mStageStretchXform.xpx == 0)
    return mat;
  bb = UnderlyingStretch(mStageStretchXform, smagMean, thetad, str, alpha);

  // Modify rotation matrix by destretching, rotating, and restretching
  mat = MatMul(MatMul(MatInv(bb), mat), bb);
  return mat;
}

// Given a transformation between rotated images or coordinates, return the underlying 
// no-mag stretch transformation, mean mag, actual rotation, stretch and axis of stretch
ScaleMat CShiftManager::UnderlyingStretch(ScaleMat raMat, float &smagMean, double &thetad
                                          , float &str, double &alpha)
{
  ScaleMat bb;
  float theta, smag, phi;
  double bfac, ssqr, sinsqa;

  // This is translated from finddistort.f
  // From transform between rotated images, determine rot-mag-stretch
  // take out the net mag change and get the no-mag transform
  amatToRotmagstr(raMat.xpx, raMat.xpy, raMat.ypx, raMat.ypy, &theta, &smag, &str, &phi);
  smagMean = (float)(smag * sqrt((double)str));
  rotmagstrToAmat(theta, smag / smagMean, str, phi, &bb.xpx, &bb.xpy, &bb.ypx, &bb.ypy);

  // Get underlying rotation angle and derive underlying stretch
  thetad = acos(0.5 * (bb.xpx + bb.ypy)) * (bb.xpy - bb.ypx >= 0 ? 1. : -1.);
  bfac = (bb.ypx - bb.xpy) / sin(thetad);
  if (fabs(bfac) < 2.) 
    ssqr = -bfac / 2.;
  else
    ssqr = (-bfac + sqrt(bfac * bfac - 4.)) / 2.;
  str = (float)sqrt(ssqr);

  sinsqa = (bb.xpy / sin(thetad) - ssqr) / (1. / ssqr - ssqr);
  alpha = 0.;
  if (sinsqa >= 0.) 
    alpha = asin(sqrt(sinsqa)) * 
      ((cos(thetad) - bb.ypy) / (sin(thetad) * (1. / ssqr - ssqr)) >= 0. ? 1. : -1.);

  // convert str and alpha to a transformation matrix
  bb.xpx = (float)(str + (1. / str - str) * sin(alpha) * sin(alpha));
  bb.ypy = (float)(str + (1. / str - str) * cos(alpha) * cos(alpha));
  bb.xpy = (float)((str - 1. / str) * cos(alpha) * sin(alpha));
  bb.ypx = bb.xpy;

  return bb;
}

// Find the mag index needed for autofocusing with boosted mag, starting at given magInd
int CShiftManager::FindBoostedMagIndex(int magInd, int boostMag)
{ 
  int lowerMag, upperMag, ind, curcam = mWinApp->GetCurrentCamera();
  int retval = magInd;
  double pixDiff, wantPixel, minPdiff = 1.e30;
  if (!boostMag)
    return magInd;
  wantPixel = GetPixelSize(curcam, magInd) / boostMag;
  if (boostMag == 1)
    wantPixel *= 2;
  mWinApp->GetMagRangeLimits(curcam, magInd, lowerMag, upperMag);
  for (ind = lowerMag; ind <= upperMag; ind++) {
    if (!MagForCamera(curcam, ind))
      continue;
    pixDiff = fabs(GetPixelSize(curcam, ind) - wantPixel);
    if (pixDiff < minPdiff) {
      minPdiff = pixDiff;
      retval = ind;
    }
  }
  return retval;
}

// Find the shift adjustment for a given set conSet at the mag magInd, provided that
// it is appropriate and the shift measurement matches
bool CShiftManager::ShiftAdjustmentForSet(int conSet, int magInd, float &shiftX, 
                                          float &shiftY, int camera, int recBin)
{ 
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int setMag = magInd;
  int recMag = magInd;
  int recSet = RECORD_CONSET;
  int conArea = mCamera->ConSetToLDArea(conSet);
  double exprat;
  ControlSet *camSets = mConSets;
  if (camera >= 0)
    camSets = mWinApp->GetCamConSets() + camera * MAX_CONSETS;
  if (recBin < 0)
    recBin = camSets[recSet].binning;
  shiftX = shiftY = 0.;

  // Require STEM mode and measurements for both sets
  if (!mWinApp->GetSTEMMode() || conSet > 4 || !mInterSetShifts.binning[conSet] || 
    !mInterSetShifts.binning[recSet])
    return false;

  // If low dose, replace mags and require no shift in axis position
  if (mWinApp->LowDoseMode()) {
    setMag = ldp[conArea].magIndex;
    recMag = ldp[recSet].magIndex;
    if (fabs(ldp[conArea].axisPosition - ldp[recSet].axisPosition) > 0.01)
      return false;
  }

  // For focus, find the higher mag if appropriate
  if (conSet == FOCUS_CONSET && !mWinApp->LowDoseMode() && camSets[conSet].boostMag)
    setMag = FindBoostedMagIndex(magInd, camSets[conSet].boostMag);
  if (mInterSetShifts.binning[conSet] != camSets[conSet].binning ||
    mInterSetShifts.binning[recSet] != recBin ||
    mInterSetShifts.magInd[conSet] != setMag || mInterSetShifts.magInd[recSet] != recMag
    || fabs((double)(mInterSetShifts.sizeX[conSet] * camSets[conSet].binning - 
    (camSets[conSet].right - camSets[conSet].left))) > 10. ||
    fabs((double)(mInterSetShifts.sizeY[conSet] * camSets[conSet].binning - 
    (camSets[conSet].bottom - camSets[conSet].top))) > 10. ||
    fabs((double)(mInterSetShifts.sizeX[recSet] * recBin - 
    (camSets[recSet].right - camSets[recSet].left))) > 10. ||
    fabs((double)(mInterSetShifts.sizeY[recSet] * recBin - 
    (camSets[recSet].bottom - camSets[recSet].top))) > 10.) {
      SEMTrace('s', "%d %d %d %d %d %d %d %d %.5g %.5g %.5g %.5g", 
        mInterSetShifts.binning[conSet], camSets[conSet].binning,
        mInterSetShifts.binning[recSet], recBin,
        mInterSetShifts.magInd[conSet], setMag , mInterSetShifts.magInd[recSet] , recMag,
        fabs((double)(mInterSetShifts.sizeX[conSet] * camSets[conSet].binning - 
        (camSets[conSet].right - camSets[conSet].left))), 
        fabs((double)(mInterSetShifts.sizeY[conSet] * camSets[conSet].binning - 
        (camSets[conSet].bottom - camSets[conSet].top))) ,
        fabs((double)(mInterSetShifts.sizeX[recSet] * recBin - 
        (camSets[recSet].right - camSets[recSet].left))) ,
        fabs((double)(mInterSetShifts.sizeY[recSet] * recBin - 
        (camSets[recSet].bottom - camSets[recSet].top))));
      return false;
  }
  exprat = mInterSetShifts.exposure[conSet] / camSets[conSet].exposure;
  if (exprat < 0.65 || exprat > 1.55)
    return false;
  shiftX = mInterSetShifts.shiftX[conSet];
  shiftY = mInterSetShifts.shiftY[conSet];
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// HIGH-DEFOCUS MAGNIFICATION AND ROTATION CALIBRATIONS
///////////////////////////////////////////////////////////////////////////////

// Get interpolated values for scaling and rotation at the given conditions from the
// nearest calibrations.  Returns number  if interpolated values are provided
int CShiftManager::GetDefocusMagAndRot(int spot, int probeMode, double intensity, 
  float defocus, float &scale, float &rotation, float &nearestFocus,
  double nearC2Dist[2], int nearC2ind[2], int &numNearC2, int magIndForIS)
{
  bool anyMode0 = false, anyMode1 = false;
  int ind, dirFoc, dirInt, indFoc, indInt, nearIntInd[2][2], numNearInt[2], nearestInd;
  int minInd[2][2] = {-1, -1, -1, -1};
  int spotDiff, minSpotDiff = 100, magDiff, minMagDiff = 1000;
  bool aboveTol, closerIntFoc;
  double focDiff, minFocDiff, intDiff, minIntDiff, bestInt[2][2], focTol = 5.;
  float bestFocus[2][2] = {999., 999., 999., 999.};
  double dist[2], distInt, nearIntDist[2][2];
  float oneScale[2], oneRot[2], frac, oneFocus[2] = {9999., 9999.};
  int aboveCross = mWinApp->mBeamAssessor->GetAboveCrossover(spot, intensity, probeMode) ?
    1 : 0;
  double crossover = mScope->GetCrossover(spot, probeMode);
  HighFocusCalArray *focusCals = magIndForIS ? &mFocusISCals : &mFocusMagCals;
  scale = 1.;
  rotation = 0.;
  if (!focusCals->GetSize())
    return 0;

  // See if there are any calibrations in nanoprobe or microprobe
  for (ind = 0; ind < focusCals->GetSize(); ind++) {
    HighFocusMagCal& cal = focusCals->ElementAt(ind);
    if (cal.probeMode)
      anyMode1 = true;
    else
      anyMode0 = true;
  }

  // If probe mode is not specified and there is only one kind of calibration, use it
  // (This is probably never invoked)
  if (probeMode < 0 && !anyMode1)
    probeMode = 0;
  else if (probeMode < 0 && !anyMode0)
    probeMode = 1;

  // Otherwise insist on a probe mode match
  if (!((probeMode == 0 && anyMode0) || (probeMode == 1 && anyMode1)))
    return 0;

  // Find the nearest calibration in each direction for focus and intensity
  for (dirFoc = -1; dirFoc <= 1; dirFoc += 2) {
    indFoc = (dirFoc + 1) / 2;
    for (dirInt = -1; dirInt <= 1; dirInt += 2) {
      indInt = (dirInt + 1) / 2;
      minFocDiff = minIntDiff = 1.e10;
      for (ind = 0; ind < focusCals->GetSize(); ind++) {
        HighFocusMagCal& cal = focusCals->ElementAt(ind);
        if (cal.probeMode == probeMode && aboveCross ==
          (mWinApp->mBeamAssessor->GetAboveCrossover(cal.spot, cal.intensity, 
          cal.probeMode) ? 1 : 0)) {
            intDiff = cal.intensity - intensity;
            focDiff = cal.defocus - defocus;
            spotDiff = B3DABS(cal.spot - spot);
            magDiff = B3DABS(cal.magIndex - magIndForIS);
            aboveTol = fabs(bestFocus[indFoc][indInt] - cal.defocus) >= focTol;

            // A calibration is better if it is from a closer spot size, or they are
            // at about the same focus and at a closer intensity, or the focus is closer
            closerIntFoc = (fabs(intDiff) < minIntDiff && !aboveTol) ||
              (fabs(focDiff) < minFocDiff && aboveTol);
            if (dirInt * intDiff >= 0. && dirFoc * focDiff >= 0 &&
              (spotDiff < minSpotDiff || (spotDiff == minSpotDiff && closerIntFoc))) {

                // Keep track of focus and intensity at best cal
                bestInt[indFoc][indInt] = cal.intensity;
                minIntDiff = fabs(intDiff);
                bestFocus[indFoc][indInt] = cal.defocus;
                minFocDiff = fabs(focDiff);
                minInd[indFoc][indInt] = ind;
                minSpotDiff = spotDiff;
                minMagDiff = magDiff;
            }
        }
      }
    }
  }

  // First process each focus level
  for (dirFoc = -1; dirFoc <= 1; dirFoc += 2) {

    // Eliminate farther one out in defocus
    indFoc = (dirFoc + 1) / 2;
    if (minInd[indFoc][0] >= 0 && minInd[indFoc][1] >= 0) {
      if (dirFoc * (bestFocus[indFoc][1] - bestFocus[indFoc][0]) > focTol)
        minInd[indFoc][1] = -1;
      else if (dirFoc * (bestFocus[indFoc][0] - bestFocus[indFoc][1]) > focTol)
        minInd[indFoc][0] = -1;
    }

    // compute inverse distances from crossover
    numNearInt[indFoc] = 0;
    distInt = intensity;
    if (crossover > 0)
      distInt = 1. / fabs(intensity - crossover);
    for (ind = 0; ind < 2; ind++) {
      if (minInd[indFoc][ind] >= 0) {
        HighFocusMagCal& cal = focusCals->ElementAt(minInd[indFoc][ind]);
        dist[ind] = cal.intensity;
        if (crossover > 0)
          dist[ind] = 1. / fabs(dist[ind] - crossover);
        nearIntInd[indFoc][numNearInt[indFoc]] = minInd[indFoc][ind];
        nearIntDist[indFoc][numNearInt[indFoc]] = fabs(dist[ind] - distInt);
        numNearInt[indFoc]++;
      }
    }

    // If still two left, interpolate 
    if (minInd[indFoc][0] >= 0 && minInd[indFoc][1] >= 0) {
      HighFocusMagCal& cal0 = focusCals->ElementAt(minInd[indFoc][0]);
      HighFocusMagCal& cal1 = focusCals->ElementAt(minInd[indFoc][1]);
      if (dist[1] < dist[0])
        frac = (float)((distInt - dist[0]) / B3DMIN(-1.e-6, dist[1] - dist[0]));
      else
        frac = (float)((distInt - dist[0]) / B3DMAX(1.e-6, dist[1] - dist[0]));
      oneScale[indFoc] = (1.f - frac) * cal0.scale + frac * cal1.scale;
      oneRot[indFoc] = (1.f - frac) * cal0.rotation + frac * cal1.rotation;
      oneFocus[indFoc] = cal0.defocus;
    } else if (minInd[indFoc][0] >= 0) {

      // Or assign from one or the other
      HighFocusMagCal& cal0 = focusCals->ElementAt(minInd[indFoc][0]);
      oneScale[indFoc] = cal0.scale;
      oneRot[indFoc] = cal0.rotation;
      oneFocus[indFoc] = cal0.defocus;
    } else if (minInd[indFoc][1] >= 0) {
      HighFocusMagCal& cal1 = focusCals->ElementAt(minInd[indFoc][1]);
      oneScale[indFoc] = cal1.scale;
      oneRot[indFoc] = cal1.rotation;
      oneFocus[indFoc] = cal1.defocus;
   }
  }

  // Assign or interpolate between focuses
  if (oneFocus[0] > 9000. && oneFocus[1] > 9000.) {
    return 0;
  } else if (oneFocus[0] > 9000.) {
    rotation = oneRot[1];
    scale = oneScale[1];
    nearestInd = 1;
  } else if (oneFocus[1] > 9000.) {
    frac = defocus / B3DMIN(-1., oneFocus[0]);
    rotation = frac * oneRot[0];
    scale = 1. + frac * (oneScale[0] - 1.);
    nearestInd = 0;
  } else {
    frac = (defocus - oneFocus[0]) / B3DMAX(1., oneFocus[1] - oneFocus[0]);
    scale = (1.f - frac) * oneScale[0] + frac * oneScale[1];
    rotation = (1.f - frac) * oneRot[0] + frac * oneRot[1];
    nearestInd = B3DCHOICE(oneFocus[1] - defocus > defocus - oneFocus[0], 0, 1);
  }

  // Return the nearest focus, and the two intensities around it
  nearestFocus = oneFocus[nearestInd];
  numNearC2 = numNearInt[nearestInd];
  for (ind = 0; ind < numNearInt[nearestInd]; ind++) {
    nearC2ind[ind] = nearIntInd[nearestInd][ind];
    nearC2Dist[ind] = nearIntDist[nearestInd][ind];
  }
  SEMTrace('c', "For focus %.2f intensity %.5f  %s scale = %.4f rotation = %.2f", defocus,
    intensity, magIndForIS ? " IS" : "stage", scale, rotation);

  return (oneFocus[0] > 9000. || oneFocus[1] > 9000.) ? 1 : 2;
}

// Version for those who don't care about details
int CShiftManager::GetDefocusMagAndRot(int spot, int probeMode, double intensity, 
  float defocus, float &scale, float &rotation)
{
  double nearC2dist[2];
  float nearestFocus;
  int nearC2Ind[2], numNearC2;
  return GetDefocusMagAndRot(spot, probeMode, intensity, defocus, scale, rotation,
    nearestFocus, nearC2dist, nearC2Ind, numNearC2);
}

// Add one calibration to the array
void CShiftManager::AddHighFocusMagCal(int spot, int probeMode, double intensity, 
  float defocus, float scale, float rotation, int magIndForIS)
{
  double focTol = 5.;
  double distC2tol = mC2SpacingForHighFocus;
  double nearC2dist[2];
  int ind, nearC2Ind[2], numNearC2, numRemove = 0, remove[2], newAp = 0;
  float nearScale, nearRot, nearFoc;
  HighFocusCalArray *focusCals = magIndForIS ? &mFocusISCals : &mFocusMagCals;
  HighFocusMagCal cal;
  GetDefocusMagAndRot(spot, probeMode, intensity, defocus, nearScale,
      nearRot, nearFoc, nearC2dist, nearC2Ind, numNearC2, magIndForIS);
  cal.crossover = mScope->GetCrossover(spot, probeMode);
  if (cal.crossover <= 0.)
    distC2tol /= 4.f;

  // Remove existing one(s) if inverse intensity is close enough
  if (focusCals->GetSize() && fabs((double)defocus - nearFoc) < focTol) {
    for (ind = 0; ind < numNearC2; ind++)
      if (nearC2dist[ind] < distC2tol)
        remove[numRemove++] = nearC2Ind[ind];
    if (numRemove == 2 && remove[0] == remove[1])
      numRemove = 1;
    for (ind = 0; ind < numRemove; ind++) {
      HighFocusMagCal& cal = focusCals->ElementAt(remove[ind]);
      if (magIndForIS && magIndForIS == cal.magIndex && spot == cal.spot) {
        PrintfToLog("Removing nearby calibration at defocus %.2f  intensity % .3f%s",
          cal.defocus, mScope->GetC2Percent(spot, cal.intensity, probeMode),
          mScope->GetC2Units());
        focusCals->RemoveAt(remove[ind]);
      }
    }
  }

  // Manage aperture dependence when defocus changes
  if (mScope->GetUseIllumAreaForC2()) {
    if (fabs((double)defocus - mLastFocusForMagCal) < 5.) {
      newAp = mLastApertureForMagCal;
    } else if (mWinApp->mMacroProcessor->DoingMacro() &&
      mWinApp->mMacroProcessor->GetC2ApForScalingWasSet()) {
      newAp = mWinApp->mBeamAssessor->GetCurrentAperture();
    } else {
      newAp = mWinApp->mBeamAssessor->RequestApertureSize();
      if (newAp) {
        mWinApp->mBeamAssessor->ScaleTablesForAperture(newAp, false);
      } else
        mWinApp->AppendToLog("No aperture size is being recorded for this calibration");
    }
  }

  // Store the calibrations
  mLastApertureForMagCal = newAp;
  mLastFocusForMagCal = defocus;
  cal.measuredAperture = newAp;
  cal.defocus = defocus;
  cal.intensity = intensity;
  cal.probeMode = probeMode;
  cal.rotation = rotation;
  cal.scale = scale;
  cal.spot = spot;
  cal.magIndex = magIndForIS;
  focusCals->Add(cal);
  mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_HIGH_FOCUS);
}

// Return a stage to camera matrix adjusted for the given conditions
ScaleMat CShiftManager::FocusAdjustedStageToCamera(int inCamera, int inMagInd, int spot, 
  int probe, double intensity, float defocus, bool forIS)
{
  float scale, rotation, nearFoc; 
  double nearC2dist[2];
  int nearC2Ind[2], numNearC2;
  ScaleMat aMat = forIS ? IStoGivenCamera(inMagInd, inCamera) :
    StageToCamera(inCamera, inMagInd);
  if (defocus >= 0. || inMagInd < mScope->GetLowestMModeMagInd() || 
    !GetDefocusMagAndRot(spot, probe, intensity, defocus, scale, 
    rotation, nearFoc, nearC2dist, nearC2Ind, numNearC2, forIS ? inMagInd : 0))
    return aMat;
  aMat = MatScaleRotate(aMat, scale, rotation);
  SEMTrace('c', "Adjusted %s cal %.5g %.5g %.5g %.5g", forIS ? "IS" : "stage", 
    aMat.xpx, aMat.xpy, aMat.ypx, aMat.ypy);
  return aMat;
}

// Variant for using an image buffer
ScaleMat CShiftManager::FocusAdjustedStageToCamera(EMimageBuffer *imBuf, bool forIS)
{
  float defocus = 0.;
  int spot = 1;
  double intensity = 0.5;
  ScaleMat aMat = {0., 0., 0., 1.};
  if (imBuf->mCamera < 0 || !imBuf->mMagInd)
    return aMat;
  if (imBuf->mLowDoseArea && imBuf->mMagInd >= mScope->GetLowestNonLMmag() && 
    IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed)) {
      defocus = imBuf->mViewDefocus;
      if (!imBuf->GetSpotSize(spot) || !imBuf->GetIntensity(intensity))
        defocus = 0.;
  }

  return FocusAdjustedStageToCamera(imBuf->mCamera, imBuf->mMagInd, spot,
    imBuf->mProbeMode, intensity, defocus, forIS);
}

// Return scale and rotation values for an image buffer, returns number of focus cals
// involved if any returned
int CShiftManager::GetScaleAndRotationForFocus(EMimageBuffer *imBuf, float &scale, 
  float &rotation)
{
  double intensity;
  int spot;
  scale = 1.;
  rotation = 0.;
  if (imBuf->mLowDoseArea && IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed) && 
      imBuf->GetSpotSize(spot) && imBuf->GetIntensity(intensity))
     return GetDefocusMagAndRot(spot, imBuf->mProbeMode, intensity, imBuf->mViewDefocus,
          scale, rotation);
  return false;
}

// Return a stage to camera matrix adjusted for the given conditions
ScaleMat CShiftManager::FocusAdjustedISToCamera(int inCamera, int inMagInd, int spot,
  int probe, double intensity, float defocus)
{
  return FocusAdjustedStageToCamera(inCamera, inMagInd, spot, probe, intensity, defocus,
    true);
}

ScaleMat CShiftManager::FocusAdjustedISToCamera(EMimageBuffer *imBuf)
{
  return FocusAdjustedStageToCamera(imBuf, true);
}

