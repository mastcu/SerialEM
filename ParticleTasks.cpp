// ParticleTasks.cpp:      Performs tasks primarily for single-particle acquisition
//
// Copyright (C) 2017 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "ParticleTasks.h"
#include "ComplexTasks.h"
#include "EMscope.h"
#include "NavigatorDlg.h"
#include "FocusManager.h"
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "MacroProcessor.h"
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "ZbyGSetupDlg.h"


CParticleTasks::CParticleTasks(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mMSCurIndex = -2;             // Index when it is not doing anything must be < -1
  mMSTestRun = 0;
  mWaitingForDrift = 0;
  mWDDfltParams.measureType = WFD_USE_FOCUS;
  mWDDfltParams.driftRate = 1.;
  mWDDfltParams.useAngstroms = false;
  mWDDfltParams.interval = 10.;
  mWDDfltParams.maxWaitTime = 60.;
  mWDDfltParams.failureAction = 1;
  mWDDfltParams.setTrialParams = false;
  mWDDfltParams.exposure = 0.2f;
  mWDDfltParams.binning = 4;
  mWDDfltParams.usePriorAutofocus = false;
  mWDDfltParams.priorAutofocusRate = 0.5f;
  mMSLastHoleStageX = EXTRA_NO_VALUE;
  mMSLastHoleISX = mMSLastHoleISY = 0.;
  mZBGIterationNum = -1;
  mZBGMaxIterations = 5;
  mZBGIterThreshold = 0.5f;
  mZBGSkipLastThresh = 0.075f;
  mZbyGUseViewInLD = false;
  mZbyGViewSubarea = 0;
  mZBGMaxTotalChange = 100;
  mZBGMaxErrorFactor = 1.67f;
  mZBGMinErrorFactor = 0.4f;
  mZBGMinMoveForErrFac = 2.;
  mZbyGsetupDlg = NULL;
  mZBGCalWithEnteredOffset = false;
  mZBGCalUseBeamTilt = false;
  mZBGSavedTop = -1;
  mATIterationNum = -1;
  mDVDoingDewarVac = false;
}


CParticleTasks::~CParticleTasks(void)
{
}

// Set program pointers
void CParticleTasks::Initialize(void)
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mComplexTasks = mWinApp->mComplexTasks;
  mShiftManager = mWinApp->mShiftManager;
  mFocusManager = mWinApp->mFocusManager;
  mNavHelper = mWinApp->mNavHelper;
  mMSParams = mWinApp->mNavHelper->GetMultiShotParams();
  mZBGCalOffsetToUse = mFocusManager->GetDefocusOffset();
  mZBGCalBeamTiltToUse = mFocusManager->GetBeamTilt();
}

/*
 * External call to start the operation with all the parameters
 * Returns positive for an error or negative of the number of positions being done
 */
int CParticleTasks::StartMultiShot(int numPeripheral, int doCenter, float spokeRad, 
  int numSecondRing, float spokeRad2, float extraDelay, BOOL saveRec, int ifEarlyReturn, 
  int earlyRetFrames, BOOL adjustBT, int inHoleOrMulti)
{
  float pixel, xTiltFac, yTiltFac, minTiltToCompenate = 10.;
  int nextShot, nextHole, testRun, ind, useXholes, useYholes;
  int numXholes = 0, numYholes = 0;
  double delISX, delISY, transISX, transISY, delBTX, delBTY, angle;
  CString str, str2;
  CMapDrawItem *item;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  MontParam *montP = mWinApp->GetMontParam();
  bool multiInHole = (inHoleOrMulti & MULTI_IN_HOLE) != 0;
  bool multiHoles = (inHoleOrMulti & MULTI_HOLES) != 0;
  int testImage = inHoleOrMulti & MULTI_TEST_IMAGE;
  bool testComa = (inHoleOrMulti & MULTI_TEST_COMA) != 0;
  ScaleMat dMat;
  mMSNumPeripheral = multiInHole ? (numPeripheral + numSecondRing) : 0;
  mMSNumInRing[0] = mMSNumPeripheral - numSecondRing;
  mMSNumInRing[1] = numSecondRing;
  mMSDoCenter = multiInHole ? doCenter : 1;
  mMSExtraDelay = extraDelay;
  mMSIfEarlyReturn = ifEarlyReturn;
  mMSEarlyRetFrames = earlyRetFrames;
  mMSAdjustBeamTilt = adjustBT && comaVsIS->magInd > 0;
  mRecConSet = mWinApp->GetConSets() + RECORD_CONSET;
  mSavedAlignFlag = mRecConSet->alignFrames;
  mMSPosIndex.clear();

  // Check conditions, first for test runs
  if (testImage && testComa) {
    SEMMessageBox("You cannot test both multishot image location and coma correction in "
      "one run");
    return 1;
  }
  testRun = testImage;
  if (testImage) {
    if (mWinApp->Montaging() && ((mWinApp->LowDoseMode() && (montP->useViewInLowDose ||
      montP->useSearchInLowDose)) || montP->moveStage)) {
        SEMMessageBox("You cannot test multishot image location with a View or Search "
          "montage or a stage montage");
        return 1;
    }
  }
  if (testComa) {
    testRun =  MULTI_TEST_COMA;
    if (comaVsIS->magInd <= 0 && adjustBT) {
      SEMMessageBox("You cannot test multishot coma correction with\r\n""adjustment for"
        " image shift: there is no calibration of coma versus image shift");
      return 1;
    }
    mMSAdjustBeamTilt = true;
    mMSSaveRecord = false;
    mMSDoCenter = -1;
  }
  mMSAdjustAstig = mMSAdjustBeamTilt && comaVsIS->astigMat.xpx != 0. && 
    !mWinApp->mNavHelper->GetSkipAstigAdjustment();

  // Adjust some parameters for test runs
  if (testRun) {
    mMSIfEarlyReturn = 0;
    if (multiHoles) {
      mMSNumPeripheral = 0;
      multiInHole = false;
      mMSDoCenter = 1;
    } else if (testImage)
      mMSDoCenter = 0;
  }

  mMSSaveRecord = !(multiHoles && mWinApp->Montaging()) && !testComa && saveRec;

  // Then test other conditions
  if (mMSIfEarlyReturn && !camParam->K2Type) {
    SEMMessageBox("The current camera must be a K2/K3 to use early return for multiple "
       "shots");
    return 1;
  }
  if (multiInHole  && (numPeripheral < 2 || numPeripheral > MAX_PERIPHERAL_SHOTS ||
    numSecondRing == 1 || numSecondRing > MAX_PERIPHERAL_SHOTS)) {
    str.Format("To do multiple shots in a hole, the number of shots\n"
        "in one ring around the center must be between 2 and %s", MAX_PERIPHERAL_SHOTS);
    SEMMessageBox(str);
    return 1;
  }
  if (multiHoles && !((mMSParams->useCustomHoles && mMSParams->customHoleX.size() > 0) ||
    mMSParams->holeMagIndex > 0)) {
      SEMMessageBox("Hole positions have not been defined for doing multiple Records");
      return 1;
  }
  if (mMSIfEarlyReturn == 2 && earlyRetFrames == 0 && mMSSaveRecord) {
    SEMMessageBox("You cannot save Record images from multiple\n"
      "shots when doing an early return with 0 frames");
    return 1;
  }
  if (mMSSaveRecord && !mWinApp->mStoreMRC) {
    SEMMessageBox("An image file must be open for saving\n"
      "before starting to acquire multiple Record shots");
    return 1;
  }
  if (mMSSaveRecord && mWinApp->Montaging()) {
    SEMMessageBox("Multiple Records are supposed to be saved but the current image file"
      " is a montage");
    return 1;
  }

  if (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()) {
    if (mWinApp->mNavigator->GetCurrentOrAcquireItem(item) < 0) {
      SEMMessageBox("Could not retrieve the Navigator item currently being acquired");
      return 1;
    }
    numXholes = item->mNumXholes;
    numYholes = item->mNumYholes;
    if (ItemIsEmptyMultishot(item)) {
      PrintfToLog("Item %s has all holes set to be skipped, so it is being skipped", 
        (LPCTSTR)item->mLabel);
      return 0;
    }
  }

  // Set this after all tests and parameter settings, it determines operation of
  // GetHolePositions which is called from SerialEMView
  mMSTestRun = testRun;

  // Go to low dose area before recording any values
  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(RECORD_CONSET);
  mMagIndex = mScope->GetMagIndex();

  // Set up to adjust for defocus if angle is high enough
  angle = mScope->GetTiltAngle();
  if (fabs(angle) < minTiltToCompenate) {
    mMSDefocusTanFac = 0.;
  } else {

    // This is the opposite sign from Reset Image Shift because that is taking away the
    // defocus at the given IS because the stage is sliding up to the 0 IS point, 
    // here we need to impose that defocus for the given IS
    mMSDefocusTanFac = -(float)tan(DTOR * angle) * mShiftManager->GetDefocusZFactor() *
      (mShiftManager->GetStageInvertsZAxis() ? -1.f : 1.f);
    mIStoSpec = mShiftManager->IStoSpecimen(mMagIndex);
    if (!mIStoSpec.xpx) {
      SEMMessageBox("No image shift to specimen matrix available at the\n"
        "current magnification and one is needed to adjust defocus for tilt");
      return 1;
    }
    mMSBaseDefocus = mScope->GetDefocus();
  }

  mMSHoleIndex = 0;
  mMSHoleISX.clear();
  mMSHoleISY.clear();
  if ((inHoleOrMulti & MULTI_HOLES) || (numXholes && numYholes)) {
     mMSNumHoles = GetHolePositions(mMSHoleISX, mMSHoleISY, mMSPosIndex, mMagIndex, 
       mWinApp->GetCurrentCamera(), numXholes, numYholes, (float)angle);
     mMSUseHoleDelay = true;
     useXholes = numXholes ? numXholes : mMSParams->numHoles[0];
     useYholes = numYholes ? numYholes : mMSParams->numHoles[1];
     if (numXholes && numYholes && item->mNumSkipHoles)
       SkipHolesInList(mMSHoleISX, mMSHoleISY, mMSPosIndex, item->mSkipHolePos,
         item->mNumSkipHoles, mMSNumHoles);
     if (!(mMSParams->useCustomHoles && mMSParams->customHoleX.size() > 0) ||
       (numXholes && numYholes)) {

       // Adjust position indexes to be relative to middle, skip 0 for even # of holes
       for (ind = 0; ind < mMSNumHoles; ind++) {
         if (useXholes % 2 != 0 || mMSPosIndex[ind * 2] < useXholes / 2)
           mMSPosIndex[ind * 2] -= useXholes / 2;
         else
           mMSPosIndex[ind * 2] -= useXholes / 2 - 1;
         if (useYholes % 2 != 0 || mMSPosIndex[ind * 2 + 1] < useYholes / 2)
           mMSPosIndex[ind * 2 + 1] -= useYholes / 2;
         else
           mMSPosIndex[ind * 2 + 1] -= useYholes / 2 - 1;
       }
     }
  } else {

    // No holes = 1
    mMSNumHoles = 1;
    mMSHoleISX.push_back(0.);
    mMSHoleISY.push_back(0.);
  }

  if (!testRun) {
    str.Format("Starting multiple Records (%d position%s in %d hole%s) %s beam tilt%s "
      "compensation", mMSNumPeripheral + (mMSDoCenter ? 1 : 0), mMSNumPeripheral + 
      mMSDoCenter > 1 ? "s" : "", mMSNumHoles, mMSNumHoles > 1 ? "s" : "",
      mMSAdjustBeamTilt ? "WITH" : "WITHOUT", mMSAdjustAstig ? "/astigmatism" : "");
    mWinApp->AppendToLog(str);
  }

  // Get the starting image shift and beam tilt, save BT as value to restore and value to
  // increment from when compensating
  mScope->GetImageShift(mBaseISX, mBaseISY);
  if (mMSAdjustBeamTilt) {
    mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
    mCenterBeamTiltX = mBaseBeamTiltX;
    mCenterBeamTiltY = mBaseBeamTiltY;
    if (mMSAdjustAstig) {
      mScope->GetObjectiveStigmator(mBaseAstigX, mBaseAstigY);
      mCenterAstigX = mBaseAstigX;
      mCenterAstigY = mBaseAstigY;
    }

    // If script hasn't already compensated for IS, get the IS to compensate and, compute
    // the beam tilt to apply, and set it as base for compensating position IS's
    if (!(mWinApp->mMacroProcessor->DoingMacro() && 
      mWinApp->mMacroProcessor->GetCompensatedBTforIS())) {
      mScope->GetLDCenteredShift(delISX, delISY);
      mShiftManager->TransferGeneralIS(mMagIndex, delISX, delISY, comaVsIS->magInd,
        transISX, transISY);
      delBTX = comaVsIS->matrix.xpx * transISX + comaVsIS->matrix.xpy * transISY;
      delBTY = comaVsIS->matrix.ypx * transISX + comaVsIS->matrix.ypy * transISY;
      mCenterBeamTiltX = mBaseBeamTiltX + delBTX;
      mCenterBeamTiltY = mBaseBeamTiltY + delBTY;
      mWinApp->mAutoTuning->BacklashedBeamTilt(mCenterBeamTiltX, mCenterBeamTiltY, 
        mScope->GetAdjustForISSkipBacklash() <= 0);
      str.Format("For starting IS  %.3f %.3f  setting BT  %.3f  %.3f",
        delISX, delISY, delBTX, delBTY);
      if (mMSAdjustAstig) {
        delBTX = comaVsIS->astigMat.xpx * transISX + comaVsIS->astigMat.xpy * transISY;
        delBTY = comaVsIS->astigMat.ypx * transISX + comaVsIS->astigMat.ypy * transISY;
        mCenterAstigX = mBaseAstigX + delBTX;
        mCenterAstigY = mBaseAstigY + delBTY;
        mWinApp->mAutoTuning->BacklashedStigmator(mCenterAstigX, mCenterAstigY, 
          mScope->GetAdjustForISSkipBacklash() <= 0);
        str2.Format("  setting stig  %.3f  %.3f", delBTX, delBTY);
        str += str2;
      }
      
      if (GetDebugOutput('1'))
        mWinApp->AppendToLog(str);
    }
  }


  // Set up axis of peripheral shots along tilt axis if tilted enough, otherwise along
  // short axis of camera
  mPeripheralRotation = 0.;
  if (fabs(mScope->GetTiltAngle()) > 20.)
    mPeripheralRotation = (float)mShiftManager->GetImageRotation(
      mWinApp->GetCurrentCamera(), mMagIndex);
  else if (camParam->sizeY < camParam->sizeX)
    mPeripheralRotation = 90.f;

  mActPostExposure = mWinApp->ActPostExposure() && !mMSTestRun;
  mLastISX = mBaseISX;
  mLastISY = mBaseISY;
  pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), mMagIndex);
  mMSRadiusOnCam[0] = spokeRad / pixel;
  mMSRadiusOnCam[1] = spokeRad2 / pixel;
  mCamToIS = mShiftManager->CameraToIS(mMagIndex);

  // Current index runs from -1 to mMSNumPeripheral - 1 for center before
  //                          0 to mMSNumPeripheral - 1 for no center
  //                          0 to mMSNumPeripheral for center after
  mMSCurIndex = mMSDoCenter < 0 ? -1 : 0;
  mMSLastShotIndex = mMSNumPeripheral + (mMSDoCenter > 0 ? 0 : -1);
  mWinApp->UpdateBufferWindows();

  // Need to shift to the first position if doing holes or if center is not being done
  // first
  if (mMSUseHoleDelay || mMSDoCenter >= 0)
    SetUpMultiShotShift(mMSCurIndex, mMSHoleIndex, false);

  // Queue the next position if post-actions and there IS a next position
  if (mActPostExposure && GetNextShotAndHole(nextShot, nextHole))
    SetUpMultiShotShift(nextShot, nextHole, true);

  // Save stage position of the last hole
  mShiftManager->GetStageTiltFactors(xTiltFac, yTiltFac);
  dMat = MatMul(mShiftManager->IStoCamera(mMagIndex),
    MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mMagIndex)));
  mScope->GetStagePosition(mMSLastHoleStageX, mMSLastHoleStageY, delISX);
  mShiftManager->ApplyScaleMatrix(dMat, mMSHoleISX[mMSNumHoles - 1],
    mMSHoleISY[mMSNumHoles - 1], delISX, delISY);
  mMSLastHoleStageX += delISX / xTiltFac;
  mMSLastHoleStageY += delISY / yTiltFac;
  mMSLastHoleISX = mMSHoleISX[mMSNumHoles - 1];
  mMSLastHoleISY = mMSHoleISY[mMSNumHoles - 1];
  mMSLastFailed = true;

  ind = StartOneShotOfMulti();
  if (ind)
    return ind;
  return -(mMSNumHoles * (mMSNumPeripheral + (mMSDoCenter ? 1 : 0)));
}

/*
 * Do the next operations after a shot is done
 */
void CParticleTasks::MultiShotNextTask(int param)
{
  int nextShot, nextHole;
  if (mMSCurIndex < -1)
    return;

  // Save Record
  mRecConSet->alignFrames = mSavedAlignFlag;
  if (mMSSaveRecord && mMSImageReturned &&
    mWinApp->mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC)) {
    StopMultiShot();
    return;
  }

  // Set indices for next shot if there is one, otherwise quit
  if (GetNextShotAndHole(nextShot, nextHole)) {
    mMSCurIndex = nextShot;
    mMSHoleIndex = nextHole;
  } else {
    mMSLastFailed = false;
    StopMultiShot();
    return;
  }

  // Do or queue image shift as needed and take shot
  // For post-action queuing, get the hole and shot index of next position
  if (mActPostExposure) {
    if (GetNextShotAndHole(nextShot, nextHole))
      SetUpMultiShotShift(nextShot, nextHole, true);
  } else {
    SetUpMultiShotShift(mMSCurIndex, mMSHoleIndex, false);
  }
  StartOneShotOfMulti(); 
}

int CParticleTasks::MultiShotBusy(void)
{
  return (DoingMultiShot() && (mWinApp->mCamera->CameraBusy() || 
    mWinApp->mMontageController->DoingMontage() || 
    mWinApp->mAutoTuning->GetDoingCtfBased())) ? 1 : 0;
}

// Cleanup call on error/timeout
void CParticleTasks::MultiShotCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out getting an image with multi-shot routine"));
  StopMultiShot();
  mWinApp->ErrorOccurred(error);
}

// Stop call: restore image shift 
void CParticleTasks::StopMultiShot(void)
{
  if (mMSCurIndex < -1)
    return;
  if (mCamera->Acquiring()) {
    mCamera->SetImageShiftToRestore(mBaseISX, mBaseISY);
    if (mMSAdjustBeamTilt)
      mCamera->SetBeamTiltToRestore(mBaseBeamTiltX, mBaseBeamTiltY);
    if (mMSAdjustAstig)
      mCamera->SetAstigToRestore(mBaseAstigX, mBaseAstigY);
    if (mMSDefocusTanFac)
      mCamera->SetDefocusToRestore(mMSBaseDefocus);
  } else {
    mScope->SetImageShift(mBaseISX, mBaseISY);
    if (mMSAdjustBeamTilt)
      mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
    if (mMSAdjustAstig)
      mScope->SetObjectiveStigmator(mBaseAstigX, mBaseAstigY);
    if (mMSDefocusTanFac)
      mScope->SetDefocus(mMSBaseDefocus);
  }

  // Montage does a second restore after it is no longer "Doing", so just set this
  mWinApp->mMontageController->SetBaseISXY(mBaseISX, mBaseISY);
  mRecConSet->alignFrames = mSavedAlignFlag;
  mMSCurIndex = -2;
  mMSTestRun = 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

/*
 * Compute the image shift to the given position and queue it or do it
 */
void CParticleTasks::SetUpMultiShotShift(int shotIndex, int holeIndex, BOOL queueIt)
{
  double ISX, ISY, delBTX, delBTY, delISX = 0, delISY = 0, angle;
  double transISX, transISY, delAstigX = 0., delAstigY = 0.;
  float delay, delFocus;
  int ring = 0, indBase = 0;
  CString str, str2;
  BOOL debug = GetDebugOutput('1');
  bool doBacklash = mScope->GetAdjustForISSkipBacklash() <= 0;
  int BTdelay;
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();

  // Compute IS from center for a peripheral shot: rotate (rad,0) by the angle
  if (shotIndex < mMSNumPeripheral && shotIndex >= 0) {
    if (shotIndex >= mMSNumInRing[0]) {
      ring = 1;
      indBase = mMSNumInRing[0];
    }
    angle = DTOR * ((shotIndex -indBase) * 360. / mMSNumInRing[ring] + 
      mPeripheralRotation);
    mShiftManager->ApplyScaleMatrix(mCamToIS, mMSRadiusOnCam[ring] * (float)cos(angle),
      mMSRadiusOnCam[ring] * (float)sin(angle), delISX, delISY);
  }

  // Get full IS and delta IS and default delay for move from last position
  // Multiply by hole delay factor if doing hole; add extra delay
  delISX += mMSHoleISX[holeIndex];
  delISY += mMSHoleISY[holeIndex];
  ISX = mBaseISX + delISX;
  ISY = mBaseISY + delISY;
  if (debug || mMSTestRun) 
   str.Format("For hole %d shot %d  %s  delIS  %.3f %.3f", holeIndex, shotIndex,
    queueIt ? "Queuing" : "Setting", delISX, delISY);
  
  if (mMSDefocusTanFac != 0.) {
    delFocus = (float)(mIStoSpec.ypx * delISX + mIStoSpec.ypy * delISY) *mMSDefocusTanFac;
    if (debug || mMSTestRun) {
      str2.Format("   delFocus %.3f", delFocus);
      str += str2;
    }
  }
  if (debug || mMSTestRun)
    mWinApp->AppendToLog(str);

  delay = mShiftManager->ComputeISDelay(ISX - mLastISX, ISY - mLastISY);
  if (mMSUseHoleDelay)
    delay *= mMSParams->holeDelayFactor;
  if (mMSExtraDelay > 0.)
    delay += mMSExtraDelay;
  mMSUseHoleDelay = false;

  if (mMSAdjustBeamTilt) {
    BTdelay = mWinApp->mAutoTuning->GetBacklashDelay();
    mShiftManager->TransferGeneralIS(mMagIndex, delISX, delISY, comaVsIS->magInd,
      transISX, transISY);
    delBTX = comaVsIS->matrix.xpx * transISX + comaVsIS->matrix.xpy * transISY;
    delBTY = comaVsIS->matrix.ypx * transISX + comaVsIS->matrix.ypy * transISY;
    if (debug)
      str.Format("%s incremental IS  %.3f %.3f   BT  %.3f  %.3f",
        queueIt ? "Queuing" : "Setting", delISX, delISY, delBTX, delBTY);
    if (mMSAdjustAstig) {
      delAstigX = comaVsIS->astigMat.xpx * transISX + comaVsIS->astigMat.xpy * transISY;
      delAstigY = comaVsIS->astigMat.ypx * transISX + comaVsIS->astigMat.ypy * transISY;
      if (debug) {
        str2.Format("   Astig  %.3f  %.3f", delAstigX, delAstigY);
        str += str2;
      }
    }
    if (debug)
      mWinApp->AppendToLog(str);
  }

  // Queue it or do it
  if (queueIt) {
    mCamera->QueueImageShift(ISX, ISY, B3DNINT(1000. * delay));
    if (mMSAdjustBeamTilt)
      mCamera->QueueBeamTilt(mCenterBeamTiltX + delBTX, mCenterBeamTiltY + delBTY, 
        doBacklash ? BTdelay : 0);
    if (mMSAdjustAstig)
      mCamera->QueueStigmator(mCenterAstigX + delAstigX, mCenterAstigY + delAstigY,
        doBacklash ? BTdelay : 0);
    if (mMSDefocusTanFac)
      mCamera->QueueDefocus(mMSBaseDefocus + delFocus);
  } else {
    mScope->SetImageShift(ISX, ISY);
    mShiftManager->SetISTimeOut(delay);
    if (mMSAdjustBeamTilt)
      mWinApp->mAutoTuning->BacklashedBeamTilt(mCenterBeamTiltX + delBTX,
        mCenterBeamTiltY + delBTY, doBacklash);
    if (mMSAdjustAstig)
      mWinApp->mAutoTuning->BacklashedStigmator(mCenterAstigX + delAstigX,
        mCenterAstigY + delAstigY, doBacklash);
    if (mMSDefocusTanFac)
      mScope->SetDefocus(mMSBaseDefocus + delFocus);
  }
  mLastISX = ISX;
  mLastISY = ISY;
}

// Given current hole and shot index, this will give the next one
bool CParticleTasks::GetNextShotAndHole(int &nextShot, int &nextHole)
{
  nextShot = mMSCurIndex + 1;
  nextHole = mMSHoleIndex;
  if (nextShot > mMSLastShotIndex) {
    nextShot = mMSDoCenter < 0 ? -1 : 0;
    nextHole++;
  }
  return (nextHole < mMSNumHoles);
}

/*
 * Start an acquisition: Set up early return, start shot, set the status pane
 */
int CParticleTasks::StartOneShotOfMulti(void)
{
  CString str;
  int nextShot, nextHole;
  int numInHole = mMSNumPeripheral + (mMSDoCenter ? 1 : 0);
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  bool earlyRet = ((mMSIfEarlyReturn == 1 && !GetNextShotAndHole(nextShot, nextHole)) ||
    mMSIfEarlyReturn == 2 || (mMSIfEarlyReturn > 2 && (mMSHoleIndex > 0 ||
      mMSCurIndex > (mMSDoCenter < 0 ? -1 : 0)))) && camParams->K2Type;
  if (earlyRet && mCamera->SetNextAsyncSumFrames(mMSEarlyRetFrames < 0 ? 65535 :
    mMSEarlyRetFrames, false, GetNextShotAndHole(nextShot, nextHole))) {
    StopMultiShot();
    return 1;
  }
  if (earlyRet && mRecConSet->alignFrames && mRecConSet->useFrameAlign == 1)
    mRecConSet->alignFrames = 0;
  mMSImageReturned = false;
  if (mMSTestRun & MULTI_TEST_COMA) {
    mWinApp->mAutoTuning->CtfBasedAstigmatismComa(1, false, 1, true, false);
  } else if (mMSTestRun && mWinApp->Montaging()) {
    mWinApp->mMontageController->StartMontage(MONT_NOT_TRIAL, false);
  } else {
    mCamera->InitiateCapture(RECORD_CONSET);
    mMSImageReturned = !earlyRet || mMSEarlyRetFrames != 0;
  }

  // Return if error in starting capture
  if (mMSCurIndex < -1)
    return 0;
  mWinApp->AddIdleTask(TASK_MULTI_SHOT, 0, 0);
  str.Format("DOING MULTI-SHOT %d OF %d", mMSHoleIndex * numInHole +
    mMSCurIndex + (mMSDoCenter < 0 ? 2 : 1), numInHole * mMSNumHoles);
  mWinApp->SetStatusText(MEDIUM_PANE, str);
  return 0;
}

/*
 * Get relative image shifts of the holes into the vectors, transferred to the given
 * mag and camera
 */
int CParticleTasks::GetHolePositions(FloatVec &delISX, FloatVec &delISY, IntVec &posIndex,
  int magInd, int camera, int numXholes, int numYholes, float tiltAngle)
{
  int numHoles = 0, ind, ix, iy, direction[2], startInd[2], endInd[2], fromMag, jump[2];
  double xCenISX, yCenISX, xCenISY, yCenISY, transISX, transISY;
  BOOL crossPattern = mMSParams->skipCornersOf3x3;
  bool adjustForTilt = false;
  float holeAngle, specX, specY, cosRatio;
  std::vector<double> fromISX, fromISY;
  ScaleMat IStoSpec, specToIS;

  delISX.clear();
  delISY.clear();
  posIndex.clear();
  if (mMSParams->useCustomHoles && mMSParams->customHoleX.size() > 0 &&
    !(numXholes || numYholes)) {

    // Custom holes are easy, the list is relative to the center position
    numHoles = (int)mMSParams->customHoleX.size();
    for (ind = 0; ind < numHoles; ind++) {
      fromISX.push_back(mMSParams->customHoleX[ind]);
      fromISY.push_back(mMSParams->customHoleY[ind]);
    }
    fromMag = mMSParams->customMagIndex;
    holeAngle = mMSParams->tiltOfCustomHoles;
  } else if (mMSParams->holeMagIndex > 0) {
    if (numXholes && numYholes) {
      crossPattern = numXholes == -3 && numYholes == -3;
      if (crossPattern) {
        numXholes = 3;
        numYholes = 3;
      }
    } else {
      numXholes = mMSParams->numHoles[0];
      numYholes = mMSParams->numHoles[1];
    }
    crossPattern = crossPattern && numXholes == 3 && numYholes == 3;
    holeAngle = mMSParams->tiltOfHoleArray;

    // The hole pattern requires computing position relative to center of pattern
    numHoles = crossPattern ? 5 : (numXholes * numYholes);
    if (mMSTestRun)
      numHoles = B3DMIN(2, numXholes) * B3DMIN(2, numYholes);
    xCenISX = mMSParams->holeISXspacing[0] * 0.5 * (numXholes - 1);
    xCenISY = mMSParams->holeISYspacing[0] * 0.5 * (numXholes - 1);
    yCenISX = mMSParams->holeISXspacing[1] * 0.5 * (numYholes - 1);
    yCenISY = mMSParams->holeISYspacing[1] * 0.5 * (numYholes - 1);
    fromMag = mMSParams->holeMagIndex;

    // Set up to do arbitrary directions in each axis
    direction[0] = direction[1] = 1;
    startInd[0] = startInd[1] = 0;
    endInd[0] = numXholes - 1;
    endInd[1] = numYholes - 1;
    jump[0] = jump[1] = 1;
    if (mMSTestRun && !crossPattern) {
      jump[0] = B3DMAX(2, numXholes) - 1;
      jump[1] = B3DMAX(2, numYholes) - 1;
    }
    for (iy = startInd[1]; (endInd[1] - iy) * direction[1] >= 0; 
      iy += direction[1] * jump[1]) {
      for (ix = startInd[0]; (endInd[0] - ix) * direction[0] >= 0 ; 
        ix += direction[0] * jump[0]) {
        if (!(crossPattern && ((ix % 2 == 0) && (iy % 2 == 0)) ||
          (mMSTestRun && ix == 1 && iy == 1))) {
          fromISX.push_back((ix * mMSParams->holeISXspacing[0] - xCenISX) +
            (iy * mMSParams->holeISXspacing[1] - yCenISX));
          fromISY.push_back((ix * mMSParams->holeISYspacing[0] - xCenISY) +
            (iy * mMSParams->holeISYspacing[1] - yCenISY));
          posIndex.push_back(ix);
          posIndex.push_back(iy);
        }
      }

      // For now, zigzag pattern
      ix = startInd[0];
      startInd[0] = endInd[0];
      endInd[0] = ix;
      direction[0] = -direction[0];
    }
  }

  // Set up to adjust for tilt angle if both angles are defined, the adjust is big enough,
  // and the matrix exists
  if (tiltAngle > -900. && holeAngle > -900.) {
    cosRatio = (float)(cos(DTOR * tiltAngle) / cos(DTOR * holeAngle));
    if (fabs(cosRatio - 1.) > 0.001) {
      IStoSpec = mShiftManager->IStoSpecimen(magInd);
      if (IStoSpec.xpx) {
        adjustForTilt = true;
        specToIS = MatInv(IStoSpec);
      }
    }
  }

  // Transfer the image shifts and add to return vectors
  for (ind = 0; ind < numHoles; ind++) {
    mWinApp->mShiftManager->TransferGeneralIS(fromMag, fromISX[ind], fromISY[ind], magInd,
      transISX, transISY, camera);
    if (adjustForTilt) {
      mShiftManager->ApplyScaleMatrix(IStoSpec, (float)transISX, (float)transISY, specX, 
        specY);
      mShiftManager->ApplyScaleMatrix(specToIS, specX, cosRatio * specY, transISX, 
        transISY);
    }
    delISX.push_back((float)transISX);
    delISY.push_back((float)transISY);
  }
  return numHoles;
}

/*
 * Given the existing vectors and their position indexes for holes, remove the ones
 * listed in the skipIndex list
 */
void CParticleTasks::SkipHolesInList(FloatVec &delISX, FloatVec &delISY, IntVec &posIndex,
  unsigned char *skipIndex, int numSkip, int &numHoles)
{
  int pos, skip;
  for (pos = numHoles - 1; pos >= 0; pos--) {
    for (skip = 0; skip < numSkip; skip++) {
      if (posIndex[2 * pos] == skipIndex[2 * skip] &&
        posIndex[2 * pos + 1] == skipIndex[2 * skip + 1]) {
        delISX.erase(delISX.begin() + pos);
        delISY.erase(delISY.begin() + pos);
        posIndex.erase(posIndex.begin() + 2 * pos);
        posIndex.erase(posIndex.begin() + 2 * pos);
        numHoles--;
        break;
      }
    }
  }
}

// Returns true if the skip list is as big as the number of holes in multishot pattern
bool CParticleTasks::ItemIsEmptyMultishot(CMapDrawItem * item)
{
  int numHoles;
  if (!item->mNumSkipHoles)
    return false;
  numHoles = item->mNumXholes * item->mNumYholes;
  if (item->mNumXholes == -3 && item->mNumYholes == -3)
    numHoles = 5;
  return numHoles == item->mNumSkipHoles;
}

// Returns true and the number of the current hole and peripheral position numbered from
// 1, and 0 for the center position; or returns false if not doing multishot
bool CParticleTasks::CurrentHoleAndPosition(CString &strCurPos)
{
  int curHole, curPos;
  if (mMSCurIndex < -1)
    return false;
  curHole = mMSHoleIndex + 1;
  if (mMSNumPeripheral)
    curPos = (mMSCurIndex + 1) % (mMSNumPeripheral + 1);
  else
    curPos = 0;
  if (mMSPosIndex.size() > 0) {
    strCurPos.Format("X%+dY%+d-%d", mMSPosIndex[2 * mMSHoleIndex], 
      mMSPosIndex[2 * mMSHoleIndex + 1], curPos);
  } else {
    strCurPos.Format("%d-%d", curHole, curPos);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////
// WAITING FOR DRIFT

// The routine to start it with given parameters.  Returns 0 if it starts routine, -1
// if it just goes on because of prior autofocus, there are no error (1) returns yet
int CParticleTasks::WaitForDrift(DriftWaitParams &param, bool useImageInA, 
  float requiredMean, float changeLimit)
{
  ControlSet *conSets = mWinApp->GetConSets();
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  CString str;
  int binInd, realBin, delay, shotType = FOCUS_CONSET;
  UINT tickTime, genTimeOut;
  DWORD lastAcquireTime = mCamera->GetLastAcquireStartTime();
  int maxSecSinceAF = 10;
  double driftX, driftY, stz;
  float imStageX, imStageY, tol;

  mWDParm = param;

  // First check if the last autofocus was good enough
  if (param.measureType != WFD_USE_TRIAL && param.usePriorAutofocus &&
    mFocusManager->GetLastDriftImageTime() == lastAcquireTime &&
    SEMTickInterval(lastAcquireTime) < 1000 * maxSecSinceAF) {
    mScope->GetStagePosition(driftX, driftY, stz);
    tol = mScope->GetBacklashTolerance();
    if (mImBufs->GetStagePosition(imStageX, imStageY) && fabs(imStageX - driftX) < tol &&
      fabs(imStageY - driftY) < tol) {
      mFocusManager->GetLastDrift(driftX, driftY);
      mWDLastDriftRate = (float)sqrt(driftX * driftX + driftY * driftY);
      if (mWDLastDriftRate < param.priorAutofocusRate) {
        str.Format("Skipping wait for drift because drift from last autofocus was %s",
          (LPCTSTR)FormatDrift(mWDLastDriftRate));
        mWDLastFailed = 0;
        mWinApp->AppendToLog(str, LOG_SWALLOW_IF_CLOSED);
        return -1;
      }
    }
  }

  // Set trial parameters
  if (param.measureType == WFD_USE_TRIAL) {
    shotType = TRACK_CONSET;
    conSets[TRACK_CONSET] = conSets[TRIAL_CONSET];
    if (param.setTrialParams && param.exposure > 0.)
      conSets[TRACK_CONSET].exposure = param.exposure;
    if (param.setTrialParams && param.binning > 0) {
      conSets[TRACK_CONSET].binning = param.binning * BinDivisorI(camParam);
      mCamera->FindNearestBinning(camParam, &conSets[TRACK_CONSET], binInd, realBin);
    }
    conSets[TRACK_CONSET].mode = SINGLE_FRAME;
  }

  // Make sure conditions are all met for using  the image in A
  if (useImageInA && !mImBufs->mImage || !mImBufs->mBinning ||
    (shotType == TRACK_CONSET && mImBufs->mConSetUsed != TRIAL_CONSET) ||
    (shotType == FOCUS_CONSET && mImBufs->mConSetUsed != FOCUS_CONSET) ||
    !(param.measureType == WFD_USE_TRIAL || param.measureType == WFD_USE_FOCUS) ||
    ((conSets[shotType].binning > mImBufs->mBinning &&
      conSets[shotType].binning % mImBufs->mBinning != 0) ||
      (mImBufs->mBinning > conSets[shotType].binning &&
        mImBufs->mBinning % conSets[shotType].binning != 0)))
    useImageInA = false;


  // Initialize: save focus parameters
  mWDSavedTripleMode = mFocusManager->GetTripleMode();
  mWDRefocusThreshold = mFocusManager->GetRefocusThreshold();

  // Use time when last acquire actually started, or figure out when shot will start
  if (useImageInA) {
    mWDInitialStartTime = mWDLastStartTime = lastAcquireTime;
  } else {
    if (mWinApp->LowDoseMode())
      mScope->GotoLowDoseArea(param.measureType == WFD_USE_TRIAL ? 
        TRIAL_CONSET : FOCUS_CONSET);
    tickTime = GetTickCount();
    genTimeOut = mShiftManager->GetGeneralTimeOut(shotType);
    delay = (int)SEMTickInterval(genTimeOut, tickTime);
    if (delay > 0)
      tickTime = mShiftManager->AddIntervalToTickTime(tickTime, delay);
    mWDInitialStartTime = mWDLastStartTime = tickTime;
  }
  mWDSleeping = false;
  mWaitingForDrift = 1;
  mWDLastDriftRate = -1.;
  mWDLastFailed = DW_EXTERNAL_STOP;
  mWDUpdateStatus = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "WAITING for Drift");

  // Take image or autofocus
  if (param.measureType == WFD_USE_TRIAL || param.measureType == WFD_USE_FOCUS) {
    mCamera->SetRequiredRoll(1);
    if (!useImageInA)
      mCamera->InitiateCapture(shotType);
  } else {
    mFocusManager->SetTripleMode(true);
    mFocusManager->AutoFocusStart(1);
  }
  mWinApp->AddIdleTask(TASK_WAIT_FOR_DRIFT, 0, 0);
  return 0;
}

// Next task when waiting for drift
void CParticleTasks::WaitForDriftNextTask(int param)
{
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  CString str;
  double driftX, driftY, delta, pixel;
  float shiftX, shiftY;
  double elapsed = 0.001 * SEMTickInterval(mWDInitialStartTime);
  if (!mWaitingForDrift)
    return;
  if (mWDSleeping) {

    // If sleeping, now start the next measurement
    if (mWDParm.measureType == WFD_USE_TRIAL || mWDParm.measureType == WFD_USE_FOCUS)
      mCamera->InitiateCapture(mWDParm.measureType == WFD_USE_TRIAL ? 
        TRACK_CONSET : FOCUS_CONSET);
    else
      mFocusManager->AutoFocusStart(1);
    mWDLastStartTime = GetTickCount();
    mWDSleeping = false;
    mWDUpdateStatus = mWDLastDriftRate > 0;
  } else {

    // Otherwise first check image intensity
    if (mImBufs->mImage && mWDRequiredMean > 0.
      && mWinApp->mProcessImage->ImageTooWeak(mImBufs, mWDRequiredMean)) {
      DriftWaitFailed(DW_FAILED_TOO_DIM, "image is too dim");
      return;
    }

    // Then check for focus failure
    if (mWDParm.measureType == WFD_WITHIN_AUTOFOC &&
      (mFocusManager->GetLastFailed() || mFocusManager->GetLastAborted())) {
      DriftWaitFailed(DW_FAILED_AUTOFOCUS, "autofocus failed");
      return;
    }

    // If this is not the first round, get the drift
    if (mWaitingForDrift > 1 || mWDParm.measureType == WFD_WITHIN_AUTOFOC) {
      if (mWDParm.measureType == WFD_WITHIN_AUTOFOC) {

        // From autofocus itself
        if (mFocusManager->GetLastDrift(driftX, driftY)) {
          DriftWaitFailed(DW_FAILED_NO_INFO, 
            "no drift value available from last autofocus");
          return;
        }

        // If trying to maintain alignment, align to C to take out measured drift
        if (mWDParm.changeIS) {
          if (mShiftManager->AutoAlign(2, 1, true)) {
            DriftWaitFailed(DW_FAILED_AUTOALIGN, "autoalignment failed");
            return;
          }
        }

      } else {

         // Autoalign to B, apply image shift if option set
        if (mShiftManager->AutoAlign(1, 1, mWDParm.changeIS)) {
          DriftWaitFailed(DW_FAILED_AUTOALIGN, "autoalignment failed");
          return;
        }

        // Get the image shift then clear it, and convert to nm/sec
        pixel = mShiftManager->GetPixelSize(mImBufs);
        delta = mImBufs[0].mTimeStamp - mImBufs[2].mTimeStamp;
        if (!pixel || delta <= 0.) {
          DriftWaitFailed(DW_FAILED_NO_INFO, "images are missing needed information");
          return;
        }
        mImBufs->mImage->getShifts(shiftX, shiftY);
        if (!mWDParm.changeIS)
          mImBufs->mImage->setShifts(0., 0.);
        pixel *= 1000. / delta;
        driftX = shiftX * pixel;
        driftY = shiftY * pixel;
      }

      // Drift rate: has it passed the criterion?
      mWDLastDriftRate = (float)sqrt(driftX * driftX + driftY * driftY);
      if (mWDLastDriftRate <= mWDParm.driftRate) {
        str.Format("Drift reached %s after %.1f sec, going on",
          (LPCTSTR)FormatDrift(mWDLastDriftRate), elapsed);
        mWDLastFailed = 0;
        StopWaitForDrift();
        mWinApp->AppendToLog(str, LOG_SWALLOW_IF_CLOSED);
        return;
      }

      // Or is the time up?
      if (elapsed + mWDParm.interval / 3. > mWDParm.maxWaitTime) {
        str.Format("drift is still %s after %.0f sec", 
          (LPCTSTR)FormatDrift(mWDLastDriftRate), elapsed);
        DriftWaitFailed(DW_FAILED_TOO_LONG, str);
        return;
      }
    }

    // If not, then start sleeping
    mWDSleeping = true;
    mWaitingForDrift++;
    str = "";
    if (mWDLastDriftRate > 0) {
      str = FormatDrift(mWDLastDriftRate);
      if (mWinApp->mComplexTasks->GetVerbose() && 
        mWDParm.measureType != WFD_WITHIN_AUTOFOC)
        PrintfToLog("Elapsed time %.1f sec: drift %s", elapsed, (LPCTSTR)str);
    }
    mWinApp->SetStatusText(MEDIUM_PANE, "WAITING for Drift" + str);
  }
  mWinApp->AddIdleTask(TASK_WAIT_FOR_DRIFT, 0, 0);
}

int CParticleTasks::WaitForDriftBusy(void)
{
  CString str;
  if (mWaitingForDrift && mWDSleeping) {
    return (SEMTickInterval(mWDLastStartTime) < 1000. * mWDParm.interval) ? 1 : 0;
  }
  if (mWaitingForDrift && mWDUpdateStatus && SEMTickInterval(mWDLastStartTime) > 1500) {
    str = FormatDrift(mWDLastDriftRate);
    mWinApp->SetStatusText(MEDIUM_PANE, "WAITING for Drift" + str);
    mWDUpdateStatus = false;
  }
  return (mWaitingForDrift && (mWinApp->mCamera->CameraBusy() ||
    (mWDParm.measureType == WFD_WITHIN_AUTOFOC && mFocusManager->DoingFocus()))) ? 1 : 0;
}

// Cleanup call on error/timeout
void CParticleTasks::WaitForDriftCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out getting an image with wait for drift routine"));
  StopWaitForDrift();
  mWinApp->ErrorOccurred(error);
}

// Stop call 
void CParticleTasks::StopWaitForDrift(void)
{
  if (!mWaitingForDrift)
    return;
  mWDLastIterations = mWaitingForDrift;
  mWaitingForDrift = 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mCamera->SetRequiredRoll(0);
  mFocusManager->SetTripleMode(mWDSavedTripleMode);
  mFocusManager->SetRefocusThreshold(mWDRefocusThreshold);
}

// Call to handle failure
void CParticleTasks::DriftWaitFailed(int type, CString reason)
{
  mWDLastFailed = type;
  reason = "Stopping the wait for drift: " + reason;
  StopWaitForDrift();
  if (mWDParm.failureAction)
    SEMMessageBox(reason, MB_EXCLAME, true);  // TODO: consult policy
  else
    mWinApp->AppendToLog(reason + "; going on");
}

// Produce properly formatted string
CString CParticleTasks::FormatDrift(float drift)
{
  CString str;
  if (mWDParm.useAngstroms)
    str.Format(" %.1f A/sec", 10. * drift);
  else
    str.Format(" %.2f nm/sec", drift);
  return str;
}

// Set required mean and change limits if appropriate then start autofocus
void CParticleTasks::StartAutofocus()
{
  if (mWDRequiredMean > EXTRA_VALUE_TEST)
    mFocusManager->SetRequiredBWMean(mWDRequiredMean);
  if (mWDFocusChangeLimit > 0.)
    mFocusManager->NextFocusChangeLimits(-mWDFocusChangeLimit, mWDFocusChangeLimit);
  mFocusManager->AutoFocusStart(1);
}

///////////////////////////////////////////////////////////////////////
//  EUCENTRICITY FROM FOCUS

// Start the operation with flag whether to use view in low dose
int CParticleTasks::EucentricityFromFocus(int useVinLD)
{
  double stageX, stageY, stageZ;
  float backZ;
  StageMoveInfo smi;
  int index, error, paramInd, nearest, area = -1;
  double *focTab = mScope->GetLMFocusTable();
  ZbyGParams *zbgParams;
  BOOL lowDose = mWinApp->LowDoseMode();

  // Get the parameters
  if (mZbyGsetupDlg)
    mZbyGsetupDlg->UnloadControlParams();
  zbgParams = GetZbyGCalAndCheck(useVinLD, index, area, paramInd, nearest, error);
  if (!zbgParams) {
    if (error == 1) {
      SEMMessageBox("Eucentricity by focus cannot be run from diffraction or STEM mode");
    } else if (error == 2) {
      SEMMessageBox(CString("There is no calibration for eucentricity by focus "
        "for the current magnification and camera ") +
        (lowDose ? "and specified Low Dose area" : "outside of Low Dose"));
    } else {
      SEMMessageBox(CString("There is no calibration for eucentricity by focus "
        "for the current spot size and ") + (FEIscope ? "probe mode" : "alpha"));
    }
    return 1;
  }

  // Take care of many state-saving and setting details, then go to the standard focus
  PrepareAutofocusForZbyG(*zbgParams, true);
  mScope->SetFocus(zbgParams->standardFocus);
  mZBGTargetDefocus = zbgParams->targetDefocus;
  mZBGFocusOffset = zbgParams->focusOffset;

  // Initialize flags
  mZBGIterationNum = 0;
  mZBGFinalMove = false;
  mZBGTotalChange = 0.;
  mZBGErrorMess = "";
  mZBGMeasuringFocus = 0;
  mWinApp->UpdateBufferWindows();

  // Find out status of Z backlash and start with a backlash move if necessary, or focus
  mScope->GetStagePosition(stageX, stageY, stageZ);
  if (mScope->GetValidZbacklash(stageZ, backZ) &&
    (backZ * mComplexTasks->GetFEBacklashZ() > 0.)) {
    mZBGMeasuringFocus = 1;
    mFocusManager->AutoFocusStart(-1, mZBGUsingView ? 1 : 0);
  } else {
    smi.z = stageZ;
    smi.backZ = mComplexTasks->GetFEBacklashZ();
    smi.axisBits = axisZ;
    mScope->MoveStage(smi, true);
  }
  mWinApp->AddIdleTask(TASK_Z_BY_G, 0, mZBGMeasuringFocus ? 0 :
    B3DNINT(mComplexTasks->GetStageTimeoutFactor() * 30000));
  mWinApp->SetStatusText(MEDIUM_PANE, "EUCENTRICITY BY FOCUS");
  return 0;
}

// Next operation in Z by G or calibration
void CParticleTasks::ZbyGNextTask(int param)
{
  double stageX, stageY, stageZ;
  float delZ, errFac, scaleDir, scaling, temp, zeroScale, prevFocus, prevScale;
  float oldThresh = 100., oldScale = 0.7f;
  StageMoveInfo smi;
  CString str;
  bool doBacklash;
  int ind, jnd, numScalings = (int)mZBGFocusScalings.size();
  if (mZBGIterationNum < 0)
    return;
  if (mZBGMeasuringFocus) {
    if (mFocusManager->GetLastFailed() || mFocusManager->GetLastAborted()) {
      mZBGErrorMess = "The focus measurement failed when doing Eucentricity by Focus";
    } else {

      // For calibrating, add to sum, save the parameters when done
      if (mZBGMeasuringFocus > 1) {
        mZBGCalParam.targetDefocus += mFocusManager->GetCurrentDefocus() /
          mZBGNumCalIterations;
        mZBGIterationNum++;
        if (mZBGIterationNum >= mZBGNumCalIterations) {
          if (mZBGIndexOfCal < 0)
            mZbyGcalArray.Add(mZBGCalParam);
          else
            mZbyGcalArray[mZBGIndexOfCal] = mZBGCalParam;
          PrintfToLog("Eucentricity by focus calibration done: measured defocus = %.2f"
            " at absolute focus %.6f", mZBGCalParam.targetDefocus, 
            mZBGCalParam.standardFocus);
          StopZbyG();
          if (mZbyGsetupDlg)
            mZbyGsetupDlg->OnButUpdateState();
          return;
        }

        // Or start another iteration
        mFocusManager->AutoFocusStart(-1, mZBGUsingView ? 1 : 0);
        mWinApp->AddIdleTask(TASK_Z_BY_G, 0, 0);
        return;

      } else {

        // Doing Z by G: If focus was measured, get value and determine a scaling
        delZ = -(mFocusManager->GetCurrentDefocus() - mZBGTargetDefocus);
        scaling = 1.;
        if (numScalings > 0) {

          // First work with calibrated scalings: make sure they are in order
          for (ind = 0; ind < numScalings - 2; ind += 2) {
            for (jnd = ind + 2; jnd < numScalings; jnd += 2) {
              if (mZBGFocusScalings[jnd] > mZBGFocusScalings[ind]) {
                B3DSWAP(mZBGFocusScalings[jnd], mZBGFocusScalings[ind], temp);
                B3DSWAP(mZBGFocusScalings[jnd + 1], mZBGFocusScalings[ind + 1], temp);
              }
            }
          }

          // Set up a virtual 1 at 0 if there is no measurement close to set
          zeroScale = 1.;
          prevFocus = 0.;
          prevScale = zeroScale;
          //offset = B3DMIN(0.f, mZBGCalParam.focusOffset);

          // Find index where the offset is between the last focus value and this one,
          // or just break out on last one
          for (ind = 0; ind < numScalings; ind += 2) {
            if ((mZBGFocusOffset <= prevFocus && mZBGFocusOffset > mZBGFocusScalings[ind])
              || ind == numScalings - 2)
              break;
            prevFocus = mZBGFocusScalings[ind];
            prevScale = mZBGFocusScalings[ind + 1];
          }

          // Interpolate
          scaling = (prevScale + (mZBGFocusOffset - prevFocus) *
            (mZBGFocusScalings[ind + 1] - prevScale) /
            (mZBGFocusScalings[ind] - prevFocus)) / zeroScale;
          SEMTrace('1', "Interpolated scaling from reciprocity cal %f", scaling);


        } else {

          // If no calibrations, scale by factor from autofocus curve: the ratio of the 
          // slope where the focus was measured to slope at 0
          if (mFocusManager->GetRatioOfCalSlopes() > 0)
            scaling = mFocusManager->GetRatioOfCalSlopes();
          SEMTrace('1', "Scaling from AF curve %f", scaling);
        }

        // Apply original scaling hack if above default threshold and measurement was past
        // end of curve by a lot (no extended calibration)
        scaleDir = delZ > 0 ? -1.f : 1.f;
        if (fabs(mFocusManager->GetDistPastEndOfCal()) > 0.5 * oldThresh &&
          scaleDir * oldThresh > 0. && scaleDir * (-delZ - oldThresh) > 0.) {
          scaling *= oldScale;
          SEMTrace('1', "Change scale to %f for being past AF calibration", scaling);
        }
        
        delZ *= scaling;

        // compute an error factor
        if (mZBGIterationNum)
          errFac = 1.f - delZ / mZBGLastChange;
        mZBGMeasuringFocus = false;

        // Check for various failures and compose message
        if (fabs(mZBGTotalChange + delZ) > mZBGMaxTotalChange) {
          mZBGErrorMess.Format("Error in Eucentricity by Focus: the last implied Z change"
            "\r\nof %.1f um made the total change be over the limit of %d um",
            delZ, mZBGMaxTotalChange);
        } else if (mZBGIterationNum > 0 && fabs(delZ) > mZBGIterThreshold &&
          fabs(mZBGLastChange) > mZBGMinMoveForErrFac &&
          (errFac > mZBGMaxErrorFactor || errFac < mZBGMinErrorFactor)) {
          mZBGErrorMess.Format("Error in Eucentricity by Focus: the last implied Z change"
            "\r\nof %.1f um was too inconsistent with the previous change of %.1f um", 
            delZ, mZBGLastChange);
        }
      }
    }

    // If error, set up move back to starting position, stop now if no moves done
    if (!mZBGErrorMess.IsEmpty()) {
      delZ = -mZBGTotalChange;
      if (!mZBGIterationNum || mZBGMeasuringFocus > 1) {
        SEMMessageBox(mZBGErrorMess);
        StopZbyG();
        return;
      }
    }

    // Skip another stage move if we are close enough
    if (fabs(delZ) < mZBGSkipLastThresh) {
      PrintfToLog("Eucentricity by focus: skipping last move of %.2f; total Z change %.2f"
        " in %d iterations", delZ, mZBGTotalChange, mZBGIterationNum + 1);
      StopZbyG();
      return;
    }

    // set up the move with or without backlash
    mZBGLastChange = delZ;
    mScope->GetStagePosition(stageX, stageY, stageZ);
    smi.z = stageZ + delZ;
    mZBGTotalChange += delZ;
    smi.backZ = mComplexTasks->GetFEBacklashZ();
    smi.axisBits = axisZ;
    doBacklash = delZ * smi.backZ > 0.;
    if (!doBacklash)
      mScope->CopyZBacklashValid();
    SEMTrace('1', "Moving from %.2f to %.2f (delta %.2f) with%s backlash", stageZ, smi.z,
      delZ, doBacklash ? "" : "out");
    mScope->MoveStage(smi, doBacklash);

    // Bump iteration number and set flag if it is the last move
    mZBGIterationNum++;
    mZBGFinalMove = !mZBGErrorMess.IsEmpty() || fabs(delZ) < mZBGIterThreshold ||
      mZBGIterationNum >= mZBGMaxIterations;
  } else {

    // After last move, output error or summary message
    if (mZBGFinalMove) {
      if (!mZBGErrorMess.IsEmpty()) {
        SEMMessageBox(mZBGErrorMess + "\r\nThe original Z has been restored");
      } else
        PrintfToLog("Eucentricity by focus: total Z change %.2f in %d iterations",
          mZBGTotalChange, mZBGIterationNum);
      StopZbyG();
      return;
    }

    // Or start measuring defocus
    mZBGMeasuringFocus = true;
    mFocusManager->AutoFocusStart(-1, mZBGUsingView ? 1 : 0);
  }
  mWinApp->AddIdleTask(TASK_Z_BY_G, 0, mZBGMeasuringFocus ? 0 :
    B3DNINT(mComplexTasks->GetStageTimeoutFactor() * 30000));
}

// Stop or finish the Z by G operation: restore original focus and other state
void CParticleTasks::StopZbyG()
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  LowDoseParams ldsaParams;
  ControlSet *conSet = mWinApp->GetConSets() + VIEW_CONSET;
  if (mZBGIterationNum < 0)
    return;
  mZBGIterationNum = -1;
  mScope->SetDefocus(mZBGInitialFocus);
  if (mZBGInitialIntensity > -900)
    mScope->SetIntensity(mZBGInitialIntensity);
  if (mZBGLowDoseAreaSaved >= 0) {
    ldsaParams = ldp[mZBGLowDoseAreaSaved];
    mScope->SetLdsaParams(&ldsaParams);
    ldp[mZBGLowDoseAreaSaved] = mZBGSavedLDparam;
    mScope->GotoLowDoseArea(mZBGLowDoseAreaSaved);
  }
  mFocusManager->SetBeamTilt(mZBGSavedBeamTilt, "from StopZbyG");
  mFocusManager->SetDefocusOffset(mZBGSavedOffset);
  if (mZBGUsingView)
    mScope->SetLDViewDefocus(mZBGSavedViewDefocus);
  if (mZBGSavedTop >= 0) {
    conSet->top = mZBGSavedTop;
    conSet->left = mZBGSavedLeft;
    conSet->bottom = mZBGSavedBottom;
    conSet->right = mZBGSavedRight;
  }
  mZBGSavedTop = -1;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Z by G busy function
int CParticleTasks::ZbyGBusy()
{
  if (mZBGMeasuringFocus)
    return mFocusManager->DoingFocus() ? 1 : 0;
  else
    return mScope->StageBusy();
}

// Z by G cleanup function
void CParticleTasks::ZbyGCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out doing eucentricity by measuring focus"));
  StopZbyG();
  mWinApp->ErrorOccurred(error);
}

// Open the setup/calibration dialog
void CParticleTasks::OpenZbyGDialog()
{
  if (mZbyGsetupDlg) {
    mZbyGsetupDlg->BringWindowToTop();
    return;
  }
  mZbyGsetupDlg = new CZbyGSetupDlg();
  mZbyGsetupDlg->m_bUseOffset = mZBGCalWithEnteredOffset;
  mZbyGsetupDlg->m_fFocusOffset = mZBGCalOffsetToUse;
  mZbyGsetupDlg->m_bCalWithBT = mZBGCalUseBeamTilt;
  mZbyGsetupDlg->m_fBeamTilt = mZBGCalBeamTiltToUse;
  mZbyGsetupDlg->Create(IDD_Z_BY_G_SETUP);
  mWinApp->SetPlacementFixSize(mZbyGsetupDlg, &mZbyGPlacement);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT * CParticleTasks::GetZbyGPlacement(void)
{
  if (mZbyGsetupDlg)
    mZbyGsetupDlg->GetWindowPlacement(&mZbyGPlacement);
  return &mZbyGPlacement;
}

// Retrieve some cal parameters when closing
void CParticleTasks::ZbyGSetupClosing()
{
  GetZbyGPlacement();
  mZBGCalWithEnteredOffset = mZbyGsetupDlg->m_bUseOffset;
  mZBGCalOffsetToUse = mZbyGsetupDlg->m_fFocusOffset;
  mZBGCalUseBeamTilt = mZbyGsetupDlg->m_bCalWithBT;
  mZBGCalBeamTiltToUse = mZbyGsetupDlg->m_fBeamTilt;
  mZbyGsetupDlg = NULL;
}

// Look for a Z by G calibration that matches the mag, low dose area, and camera, return
// Null if none, return index of calibration if find it, mag index of nearest mag if not
ZbyGParams *CParticleTasks::FindZbyGCalForMagAndArea(int magInd, int ldArea, int camera, 
  int &paramInd, int &nearest)
{
  int ind;
  nearest = -1;
  paramInd = -1;
  for (ind = 0; ind < (int)mZbyGcalArray.GetSize(); ind++) {
    if (ldArea == mZbyGcalArray[ind].lowDoseArea && mZbyGcalArray[ind].camera == camera) {
      if (magInd == mZbyGcalArray[ind].magIndex) {
        paramInd = ind;
        return &mZbyGcalArray[ind];
      }
      if (nearest < 0 || B3DABS(magInd - mZbyGcalArray[ind].magIndex) <
        B3DABS(magInd - nearest))
        nearest = mZbyGcalArray[ind].magIndex;
    }
  }
  return NULL;
}

// Look for a Z by G calibration that matches the low dose area to be used, mag and camera
// plus require a match to probe/alpha and spot if not in low dose, and return an error
// code along with a NULL for error
ZbyGParams * CParticleTasks::GetZbyGCalAndCheck(int useVinLD, int &magInd, int &ldArea, 
  int &paramInd, int &nearest, int &error)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  ZbyGParams *zbgParams;
  ldArea = -1;
  mZBGUsingView = false;
  if (mWinApp->LowDoseMode()) {
    if (useVinLD < 0)
      mZBGUsingView = mZbyGUseViewInLD;
    else 
      mZBGUsingView = useVinLD > 0;
    ldArea = mZBGUsingView ? VIEW_CONSET : FOCUS_CONSET;
    magInd = ldp[ldArea].magIndex;
  } else
    magInd = mScope->GetMagIndex();

  error = 1;
  if (magInd <= 0)// || mWinApp->GetSTEMMode())  // For Jaap to try
    return NULL;

  zbgParams = FindZbyGCalForMagAndArea(magInd, ldArea, mWinApp->GetCurrentCamera(), 
    paramInd, nearest);
  error = 2;
  if (!zbgParams)
    return NULL;

  error = 3;
  if (!mWinApp->LowDoseMode() && (zbgParams->spotSize != mScope->FastSpotSize() ||
    (FEIscope && zbgParams->probeOrAlpha != mScope->GetProbeMode()) || (JEOLscope &&
      !mScope->GetHasNoAlpha() && zbgParams->probeOrAlpha != mScope->FastAlpha())))
    return NULL;
 
  error = 0;
  return zbgParams;
}

// Start a Z hy Z calibration with the given number of iterations, calInd is index if
// using a calibration or -1 if using current conditions
void CParticleTasks::DoZbyGCalibration(ZbyGParams &param, int calInd, int iterations)
{
  mZBGCalParam = param;
  mZBGIndexOfCal = calInd;
  mZBGNumCalIterations = iterations;
  PrepareAutofocusForZbyG(param, calInd >= 0);
  mZBGUsingView = param.lowDoseArea == VIEW_CONSET;

  mZBGCalParam.standardFocus = mScope->GetFocus();
  mZBGIterationNum = 0;
  mZBGMeasuringFocus = 2;
  mZBGCalParam.targetDefocus = 0.;
  mFocusManager->AutoFocusStart(-1, param.lowDoseArea == VIEW_CONSET ? 1 : 0);
  mWinApp->AddIdleTask(TASK_Z_BY_G, 0, 0);
}

// Do common items to get prepared for running autofocus operation for either a Z by
// G calibration or the operation itself
void CParticleTasks::PrepareAutofocusForZbyG(ZbyGParams &param, bool saveAndSetLD)
{
  LowDoseParams *ldp;
  ControlSet *conSet = mWinApp->GetConSets() + VIEW_CONSET;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  int sizeX, sizeY, left, top, bottom, right;
  mZBGInitialIntensity = -999.;
  mZBGSavedLDparam.magIndex = 0;
  mZBGLowDoseAreaSaved = -1;

  // Save and set up needed beam tilt and defocus offset
  mZBGSavedBeamTilt = mFocusManager->GetBeamTilt();
  mFocusManager->SetBeamTilt(param.beamTilt, "from PrepareAutofocusForZbyG");
  mZBGSavedOffset = mFocusManager->GetDefocusOffset();

  // For View area, make the offset value be the view defocus 
  // zero the offset for all cases; it will applied below on one-time basis
  if (param.lowDoseArea == VIEW_CONSET) {
    mZBGSavedViewDefocus = mScope->GetLDViewDefocus();
    mScope->SetLDViewDefocus(param.focusOffset);
  }
  mFocusManager->SetDefocusOffset(0.);

  if (param.lowDoseArea >= 0) {

    // If flag is set (i.e., if using a calibration), save the low dose params for the
    // area and set the values in the Z by G param into that
    if (saveAndSetLD) {
      mZBGLowDoseAreaSaved = param.lowDoseArea;
      ldp = mWinApp->GetLowDoseParams() + param.lowDoseArea;
      mZBGSavedLDparam = *ldp;
      if (mScope->GetLowDoseArea() == param.lowDoseArea)
        mScope->SetLdsaParams(&mZBGSavedLDparam);
      ldp->intensity = param.intensity;
      ldp->spotSize = param.spotSize;
      if (JEOLscope && !mScope->GetHasNoAlpha())
        ldp->beamAlpha = (float)param.probeOrAlpha;
      else if (FEIscope)
        ldp->probeMode = param.probeOrAlpha;
    }

    // Go to area so initial focus is correct
    mScope->GotoLowDoseArea(param.lowDoseArea);

    if (param.lowDoseArea == VIEW_CONSET && mZbyGViewSubarea > 0) {
      mZBGSavedTop = conSet->top;
      mZBGSavedLeft = conSet->left;
      mZBGSavedBottom = conSet->bottom;
      mZBGSavedRight = conSet->right;
      if (mZbyGViewSubarea == 1) {
        sizeX = (camParam->sizeX / 4) / conSet->binning;
        sizeY = (camParam->sizeY / 4) / conSet->binning;
      } else if (mZbyGViewSubarea == 2) {
        sizeX = ((3 * camParam->sizeX) / 8) / conSet->binning;
        sizeY = ((3 * camParam->sizeY) / 8) / conSet->binning;
      } else {
        sizeX = (camParam->sizeX / 2) / conSet->binning;
        sizeY = (camParam->sizeY / 2) / conSet->binning;
      }
      mCamera->CenteredSizes(sizeX, camParam->sizeX, camParam->moduloX, left, right,
        sizeY, camParam->sizeY, camParam->moduloY, top, bottom, conSet->binning);
      conSet->top = conSet->binning * top;
      conSet->left = conSet->binning * left;
      conSet->bottom= conSet->binning * bottom;
      conSet->right = conSet->binning * right;
    }
    mWinApp->mLowDoseDlg.SetContinuousUpdate(false);
  } else {

    // For non LD, just set the intensity
    mZBGInitialIntensity = mScope->GetIntensity();
    mScope->SetIntensity(param.intensity);
  }

  // Save the focus to restore and then do the defocus offset here
  mZBGInitialFocus = mScope->GetDefocus();
  if (param.lowDoseArea != VIEW_CONSET)
    mScope->IncDefocus(param.focusOffset);
}

///////////////////////////////////////////////////////////////////////
// ALIGN TO TEMPLATE
//
// Start the task
int CParticleTasks::AlignToTemplate(NavAlignParams & aliParams)
{
  CMapDrawItem *map;
  EMimageBuffer *imBuf = &mImBufs[aliParams.loadAndKeepBuf];
  MontParam montParm;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  mATParams = aliParams;

  // Find map and check it
  map = mWinApp->mNavigator->FindItemWithString(mATParams.templateLabel, false, true);
  if (!map) {
    SEMMessageBox("Cannot find the template map in Navigator table for aligning to "
      "template");
    return 1;
  }
  if (!map->IsMap()) {
    SEMMessageBox("The Navigator item designated as the template to align, with label " +
      mATParams.templateLabel + ", is not a map");
    return 1;
  }
  if (map->mMapMontage) {
    SEMMessageBox("The Navigator item designated as the template to align, with label " +
      mATParams.templateLabel + ", ia a montage map");
    return 1;
  }

  // See if it is in the buffer and load if not.  Load gives messages on error
  if (!imBuf->mImage || imBuf->mMapID != map->mMapID) {
    if (mWinApp->mNavigator->DoLoadMap(true, map, mATParams.loadAndKeepBuf))
      return 1;
  }

  mATDidSaveState = !mWinApp->LowDoseMode();
  if (mATDidSaveState) {
    mNavHelper->SetTypeOfSavedState(STATE_NONE);
    mNavHelper->SaveCurrentState(STATE_MAP_ACQUIRE, false, map->mMapCamera, 0);
  }
  mNavHelper->PrepareToReimageMap(map, &montParm, conSet, TRIAL_CONSET, (mATDidSaveState 
    || !(mWinApp->LowDoseMode() && map->mMapLowDoseConSet < 0)) ? 1 : 0);
  if (!mNavHelper->GetRIstayingInLD())
    mNavHelper->SetMapOffsetsIfAny(map);

  mATResettingShift = false;
  mATIterationNum = 0;
  mATLastFailed = false;
  mMagIndex = mScope->GetMagIndex();
  mATSavedShiftsOnAcq = mWinApp->mBufferManager->GetShiftsOnAcquire();
  mWinApp->mBufferManager->SetShiftsOnAcquire(B3DMAX(1, mATSavedShiftsOnAcq));
  mCamera->SetCancelNextContinuous(true);
  mWinApp->mCamera->InitiateCapture(
    mNavHelper->GetRIstayingInLD() ? mNavHelper->GetRIconSetNum() : TRACK_CONSET);

  mCamera->SetRequiredRoll(1);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "ALIGNING TO TEMPLATE");
  mWinApp->AddIdleTask(TASK_TEMPLATE_ALIGN, 0, 0);

  return 0;
}

// Next operation in align to template
void CParticleTasks::TemplateAlignNextTask(int param)
{
  int alignErr;
  double ISX, ISY, radialUM, stz;
  float backlashX, backlashY, shiftX, shiftY;
  ScaleMat mat;
  if (mATIterationNum < 0)
    return;
  if (mATResettingShift) {

    // Image shift was reset: stop if leaving at zero and IS was below threshold or this
    // was the last iteration
    mATResettingShift = false;
    mATIterationNum++;
    if (mATParams.leaveISatZero &&
      (mATFinishing || mATIterationNum >= mATParams.maxNumResetIS)) {
      if (mATFinishing)
        PrintfToLog("Align to Template finished after %d IS resets with IS left at zero",
          mATIterationNum);
      else
        PrintfToLog("Align to Template stopped at limit of %d iterations, with IS left"
          " at 0", mATIterationNum);
      StopTemplateAlign();
      return;
    }

    // Otherwise new image for align
    mWinApp->mCamera->InitiateCapture(
      mNavHelper->GetRIstayingInLD() ? mNavHelper->GetRIconSetNum() : TRACK_CONSET);


  } else {

    // Got an image, align it
    mWinApp->mShiftManager->SetNextAutoalignLimit(mATParams.maxAlignShift);
    alignErr = mWinApp->mShiftManager->AutoAlign(mATIterationNum ?
      1 : mATParams.loadAndKeepBuf, 1, 1, false, NULL, 0., 0., 0., 0., 0., NULL, NULL,
      GetDebugOutput('1'));
    if (alignErr) {
      SEMMessageBox(alignErr < 0 ? "Template autoalignment failed to find a peak within"
        " the distance limit" : "Template autoalignment had a memory or other error");
      mATLastFailed = true;
      StopTemplateAlign();
      return;
    }

    // Assess shift for iterating or final message
    mScope->GetLDCenteredShift(ISX, ISY);
    if (!mATIterationNum) {
      mat = MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mMagIndex));
      if (mat.xpx) {
        mImBufs->mImage->getShifts(shiftX, shiftY);
        shiftX *= mImBufs->mBinning;
        shiftY *= -mImBufs->mBinning;
        mShiftManager->ApplyScaleMatrix(mat, shiftX, shiftY, backlashX, backlashY);
        PrintfToLog("Shift in Align to Template is %.2f, %.2f microns",
          backlashX, backlashY);
      }
    }
    radialUM = (float)mWinApp->mShiftManager->RadialShiftOnSpecimen(ISX, ISY, mMagIndex);

    // Done if iteration limit reached
    if (mATIterationNum >= mATParams.maxNumResetIS) {
      if (mATParams.maxNumResetIS > 0)
        PrintfToLog("Align to Template stopped at limit of %d iterations, with residual "
          "IS of %.2f um", mATIterationNum, radialUM);
      else
        PrintfToLog("Align to Template finished with residual IS of %.2f um", radialUM);
      StopTemplateAlign();
      return;
    }

    // Otherwise assess shift, start a reset if above limit, or on final round if below
    // and leaving IS at zero
    mATFinishing = radialUM < mATParams.resetISthresh;
    if (mATFinishing && !mATParams.leaveISatZero) {
      PrintfToLog("Align to Template finished after %d IS resets with residual IS of "
        "%.2f, below threshold", mATIterationNum, radialUM);
      StopTemplateAlign();
      return;
    }
    SEMTrace('1', "Resetting shift of  %.2f um, iteration %d", radialUM,
      mATIterationNum);
    mScope->GetStagePosition(ISX, ISY, stz);
    mWinApp->mShiftManager->ResetImageShift(mScope->GetValidXYbacklash(ISX, ISY,
      backlashX, backlashY), false);
    mATResettingShift = true;
  }
  mWinApp->AddIdleTask(TASK_TEMPLATE_ALIGN, 0, 0);
}

// Stop - restore state
void CParticleTasks::StopTemplateAlign()
{
  if (mATIterationNum < 0)
    return;
  mATIterationNum = -1;
  mWinApp->mBufferManager->SetShiftsOnAcquire(mATSavedShiftsOnAcq);
  mNavHelper->RestoreLowDoseConset();
  mNavHelper->RestoreMapOffsets();
  if (mATDidSaveState)
    mNavHelper->RestoreFromMapState();
  else if (!mNavHelper->GetRIstayingInLD() && mWinApp->mLowDoseDlg.m_bLowDoseMode ? 1 : 0)
    mWinApp->mLowDoseDlg.SetLowDoseMode(true);
  mATDidSaveState = false;

  mCamera->SetRequiredRoll(0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

int CParticleTasks::TemplateAlignBusy()
{
  if (mATResettingShift)
    return mWinApp->mShiftManager->ResettingIS() ? 1 : 0;
  return mCamera->TaskCameraBusy();
}

void CParticleTasks::TemplateAlignCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out aligning to template"));
  StopTemplateAlign();
  mWinApp->ErrorOccurred(error);
}

///////////////////////////////////////////////////////////////////////
//  MANAGE DEWARS AND VACUUM
//
// Start the process
int CParticleTasks::ManageDewarsVacuum(DewarVacParams & params, int skipOrDoChecks)
{
  int longOps[5];
  float hours[5];
  int err = 0, busy, secToStart, numLongOps = 0;
  int capabilities = mScope->GetDewarVacCapabilities();
  bool canGetTimeToFill = (FEIscope && (capabilities & 2)) ||
    mScope->GetHasSimpleOrigin();
  bool doChecks = skipOrDoChecks >= 0;
  bool doNonChecks = skipOrDoChecks <= 0;
  mDVAlreadyFilling = false;
  mDVWaitForFilling = 0;
  mDVWaitAfterFilled = 0.;
  mDVStartedRefill = false;
  mDVStartedCycle = false;
  mDVParams = params;
  if ((capabilities & 2) == 0 && !mScope->GetHasSimpleOrigin()) {
    mDVParams.checkDewars = false;
    mDVParams.refillDewars = false;
  }

  // Just handle nitrogen checking/filling here and leave other operations to next task
  // Check if dewars are already filling if it is either set to check or possibly
  // going to start it
  if (((doNonChecks && mDVParams.refillDewars) || (doChecks && mDVParams.checkDewars)) &&
    mScope->AreDewarsFilling(busy) && busy) {
    mDVAlreadyFilling = true;

    // If not already filling and we can get time to next filling, do so
  } else if (((doNonChecks && mDVParams.refillDewars) || doChecks) && 
    mDVParams.checkDewars && canGetTimeToFill) {
    if (mScope->GetDewarsRemainingTime(0, secToStart)) {

      // If it is within the pause interval, just set up to wait for it to start
      if (secToStart <= params.pauseBeforeMin * 60.) {
        mDVWaitForFilling = secToStart;
        mDVStartTime = GetTickCount();

        // Or if it is within interval where we should trigger it, go ahead and set up
        // for immediate start
      } else if (params.startRefill && secToStart <= params.startIntervalMin * 60.) {
        SetupRefillLongOp(longOps, hours, numLongOps, 0.);
      }

    }
  }

  // If none of that resulted in waiting or starting, set up to do long operation if it
  // is time for it.
  if ((doNonChecks && mDVParams.refillDewars) && !mDVAlreadyFilling && !mDVWaitForFilling &&
    !numLongOps) {
    SetupRefillLongOp(longOps, hours, numLongOps, params.dewarTimeHours);
  }

  // Set flags if buffer cycle operations should be done
  mDVNeedVacRuns = doNonChecks && FEIscope && (params.runBufferCycle ||
    (params.runAutoloaderCycle && (capabilities & 1)));
  mDVNeedPVPCheck = doChecks && FEIscope && params.checkPVP;
  mDVDoingDewarVac = true;

  // Start long operation or not
  if (numLongOps) {
    err = mScope->StartLongOperation(longOps, hours, numLongOps);
    if (err > 0) {
      SEMMessageBox("Could not start a long operation for filling nitrogen;\n" +
        CString(err == 1 ? "The thread for long operations is already running" :
          "inappropriate parameters were sent to the long operation routine"));
      mDVDoingDewarVac = false;
      return 1;
    }
    mDVStartedRefill = !err;
  }
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "MANAGING SCOPE");

  // Set up to go through busy if waiting or long op, or just go to next task now
  if (mDVAlreadyFilling || mDVWaitForFilling || mDVStartedRefill) {
    if (!mDVStartedRefill)
      mWinApp->SetStatusText(SIMPLE_PANE, mDVAlreadyFilling ? "WAIT UNTIL FILLED" :
        "WAIT for FILL START");
    if (mDVAlreadyFilling) {

      // If it fills on its own, record the time that it was found to be filling
      numLongOps = 0;
      SetupRefillLongOp(longOps, hours, numLongOps, -1.);
      mScope->StartLongOperation(longOps, hours, numLongOps);
    }
    mWinApp->AddIdleTask(TASK_DEWARS_VACUUM, 0, 2000 * mDVWaitForFilling);
  } else {
    DewarsVacNextTask(0);
  }
  return 0;
}

// Set the operation entries for a refill appropriate for the scope type with the given 
// value for the hours entry
void CParticleTasks::SetupRefillLongOp(int *longOps, float *hours, int &numLongOps,
  float hourVal)
{
  if (JEOLscope && mScope->GetJeolHasNitrogenClass()) {
    longOps[numLongOps] = LONG_OP_FILL_STAGE;
    hours[numLongOps++] = hourVal;
    longOps[numLongOps] = LONG_OP_FILL_TRANSFER;
    hours[numLongOps++] = hourVal;
  } else if ((FEIscope && (mScope->GetDewarVacCapabilities() & 2)) ||
    mScope->GetHasSimpleOrigin()) {
    longOps[numLongOps] = LONG_OP_REFILL;
    hours[numLongOps++] = hourVal;
  }
}

// Do next task in the series
void CParticleTasks::DewarsVacNextTask(int param)
{
  int longOps[5];
  float hours[5];
  int err, numLongOps = 0;
  BOOL state;
  if (!mDVDoingDewarVac)
    return;

  mDVWaitAfterFilled = 0.;

  // After waiting to start filling, set that it is filling
  if (mDVWaitForFilling) {
    mDVWaitForFilling = 0;
    mDVAlreadyFilling = true;
    SetupRefillLongOp(longOps, hours, numLongOps, -1.);
    mScope->StartLongOperation(longOps, hours, numLongOps);
    mWinApp->SetStatusText(SIMPLE_PANE, "WAIT UNTIL FILLED");
  } else if (mDVAlreadyFilling || mDVStartedRefill) {

    // If filling was doneby testing, or the long operation finished, set up for post-wait
    mDVAlreadyFilling = false;
    mDVStartedRefill = false;
    mDVWaitAfterFilled = mDVParams.postFillWaitMin;
    mDVStartTime = GetTickCount();
    mWinApp->SetStatusText(SIMPLE_PANE, "WAIT AFTER FILL");
  }

  // Put this idle task on if there is a something to do (0 wait = nothing to do)
  if (mDVAlreadyFilling || mDVWaitAfterFilled) {
    mWinApp->AddIdleTask(TASK_DEWARS_VACUUM, 0, 0);
    return;
  }

  // If buffer cycles done or PVP done, clear those flags
  mDVStartedCycle = false;
  mDVWaitForPVP = false;

  // Now check the buffer cycles with a long op call, clear their flag
  if (mDVNeedVacRuns && mDVParams.runBufferCycle) {
    longOps[numLongOps] = LONG_OP_BUFFER;
    hours[numLongOps++] = (float)(mDVParams.bufferTimeMin / 60.);
  }
  if (mDVNeedVacRuns && mDVParams.runAutoloaderCycle &&
    (mScope->GetDewarVacCapabilities() & 1)) {
    longOps[numLongOps] = LONG_OP_LOAD_CYCLE;
    hours[numLongOps++] = (float)(mDVParams.autoloaderTimeMin / 60.);
  }
  mDVNeedVacRuns = false;


  // Start long operation if needed
  if (numLongOps) {
    err = mScope->StartLongOperation(longOps, hours, numLongOps);
    if (err > 0) {
      SEMMessageBox("Could not start a long operation for doing buffer cycle;\n" +
        CString(err == 1 ? "The thread for long operations is already running" :
          "inappropriate parameters were sent to the long operation routine"));
      StopDewarsVac();
      return;
    }
    mDVStartedCycle = !err;
  }

  // Check for PVP if not doing any buffer cycles
  if (!mDVStartedCycle && mDVNeedPVPCheck && mScope->IsPVPRunning(state)) {
    mDVWaitForPVP = state;
    mDVNeedPVPCheck = false;
    mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR PVP");
  }

  // Start task if anything to do, or stop
  if (mDVStartedCycle || mDVWaitForPVP)
    mWinApp->AddIdleTask(TASK_DEWARS_VACUUM, 0, 0);
  else
    StopDewarsVac();
}

// A simple stop function
void CParticleTasks::StopDewarsVac()
{
  mDVDoingDewarVac = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->SetStatusText(SIMPLE_PANE, "");
}

// Testing for busy in the various cases
int CParticleTasks::DewarsVacBusy()
{
  int busy;
  BOOL state;

  // Long operation for refill or buffer cycle
  if (mDVStartedRefill || mDVStartedCycle)
    return mScope->LongOperationBusy();

  // Testing for filling to start or end
  if (mDVWaitForFilling || mDVAlreadyFilling) {
    if (mScope->AreDewarsFilling(busy))
      return ((mDVWaitForFilling && !busy) || (mDVAlreadyFilling && busy)) ? 1 : 0;
    return -1;
  }

  // Waiting a time after fill
  if (mDVWaitAfterFilled)
    return (SEMTickInterval(mDVStartTime) < 60000. * mDVWaitAfterFilled) ? 1 : 0;

  // Waiting for PVP to stop
  if (mDVWaitForPVP) {
    if (mScope->IsPVPRunning(state))
      return state ? 1 : 0;
    return -1;
  }
  return 0;
}

void CParticleTasks::DewarsVacCleanup(int error)
{
}
