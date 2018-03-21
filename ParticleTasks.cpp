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
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "CameraController.h"


CParticleTasks::CParticleTasks(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mMSCurIndex = -2;             // Index when it is not doing anything must be < -1
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
}

/*
 * External call to start the operation with all the parameters
 */
int CParticleTasks::StartMultiShot(int numPeripheral, int doCenter, float spokeRad, 
  float extraDelay, BOOL saveRec, int ifEarlyReturn, int earlyRetFrames, BOOL adjustBT)
{
  float pixel;
  int magInd;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  mMSNumPeripheral = numPeripheral;
  mMSDoCenter = doCenter;
  mMSExtraDelay = extraDelay;
  mMSIfEarlyReturn = ifEarlyReturn;
  mMSEarlyRetFrames = earlyRetFrames;
  mMSSaveRecord = saveRec;
  mMSAdjustBeamTilt = adjustBT && comaVsIS->magInd > 0;

  // Check conditions
  if (ifEarlyReturn && !camParam->K2Type) {
    SEMMessageBox("The current camera must be a K2 to use early return for multiple "
      "shots");
    return 1;
  }
  if (numPeripheral < 2 || numPeripheral > 12) {
    SEMMessageBox("To do multiple shots, the number of shots\n"
      "around the center must be between 2 and 12");
    return 1;
  }
  if (ifEarlyReturn && earlyRetFrames <= 0 && saveRec) {
    SEMMessageBox("You cannot save Record images from multiple\n"
      "shots when doing an early return with 0 frames");
    return 1;
  }
  if (saveRec && !mWinApp->mStoreMRC) {
    SEMMessageBox("An image file must be open for saving\n"
      "before starting to acquire multiple Record shots");
    return 1;
  }

  // Go to low dose are before recording any values
  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(RECORD_CONSET);
  magInd = mScope->GetMagIndex();
  mScope->GetImageShift(mBaseISX, mBaseISY);
  if (mMSAdjustBeamTilt)
    mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  mActPostExposure = mWinApp->ActPostExposure();
  mLastISX = mBaseISX;
  mLastISY = mBaseISY;
  pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), magInd);
  mMSRadiusOnCam = spokeRad / pixel;
  mCamToIS = mShiftManager->CameraToIS(magInd);
  mMSCurIndex = mMSDoCenter < 0 ? -1 : 0;
  mWinApp->UpdateBufferWindows();

  // Do initial image shift or queue it depending on what is being shot
  // Go to position 0 immediately if no center shot, or queue it if doing center
  // And if postactions and no center shot now, queue movement to position 1
  if (mMSDoCenter >= 0 || mActPostExposure)
    SetUpMultiShotShift(0, mMSDoCenter < 0);
  if (mMSDoCenter >= 0 && mActPostExposure)
    SetUpMultiShotShift(1, true);

  return StartOneShotOfMulti();
}

/*
 * Do the next operations after a shot is done
 */
void CParticleTasks::MultiShotNextTask(int param)
{
  int lastIndex = mMSNumPeripheral + (mMSDoCenter > 0 ? 0 : -1);
  if (mMSCurIndex < -1)
    return;

  // Save Record
  if (mMSSaveRecord && mWinApp->mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC)) {
    StopMultiShot();
    return;
  }

  // Stop at end
  mMSCurIndex++;
  if (mMSCurIndex > lastIndex) {
    StopMultiShot();
    return;
  }

  // Do or queue image shift as needed and take shot
  if (mMSCurIndex < lastIndex || !mActPostExposure)
    SetUpMultiShotShift(mMSCurIndex + (mActPostExposure ? 1 : 0), mActPostExposure);
  StartOneShotOfMulti(); 
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
  mScope->SetImageShift(mBaseISX, mBaseISY);
    if (mMSAdjustBeamTilt)
      mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  mMSCurIndex = -2;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

/*
 * Compute the image shift to the given position and queue it or do it
 */
void CParticleTasks::SetUpMultiShotShift(int shotIndex, BOOL queueIt)
{
  double ISX, ISY, delBTX, delBTY, delISX = 0, delISY = 0, angle, cosAng, sinAng;
  float delay;
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();

  // Compute IS from center for a peripheral shot
  if (shotIndex < mMSNumPeripheral) {
    angle = DTOR * shotIndex * 360. / mMSNumPeripheral;
    cosAng = cos(angle);
    sinAng = sin(angle);
    delISX = mMSRadiusOnCam * (mCamToIS.xpx * cosAng + mCamToIS.xpy * sinAng);
    delISY = mMSRadiusOnCam * (mCamToIS.ypx * cosAng + mCamToIS.ypy * sinAng);
  }

  // Get full IS and default delay for move from last position; plus extra delay
  ISX = mBaseISX + delISX;
  ISY = mBaseISY + delISY;
  delay = mShiftManager->ComputeISDelay(ISX - mLastISX, ISY - mLastISY);
  if (mMSExtraDelay > 0.)
    delay += mMSExtraDelay;

  if (mMSAdjustBeamTilt) {
    delBTX = comaVsIS->matrix.xpx * delISX + comaVsIS->matrix.xpy * delISY;
    delBTY = comaVsIS->matrix.ypx * delISX + comaVsIS->matrix.ypy * delISY;
    SEMTrace('1', "%s incremental IS  %.3f %.3f   BT  %.3f  %.3f", 
      queueIt ? "Queuing" : "Setting", delISX, delISY, delBTX, delBTY);
  }

  // Queue it or do it
  if (queueIt) {
    mCamera->QueueImageShift(ISX, ISY, B3DNINT(1000. * delay));
    if (mMSAdjustBeamTilt)
      mCamera->QueueBeamTilt(mBaseBeamTiltX + delBTX, mBaseBeamTiltY + delBTY);
  } else {
    mScope->SetImageShift(ISX, ISY);
    mShiftManager->SetISTimeOut(delay);
    if (mMSAdjustBeamTilt)
      mScope->SetBeamTilt(mBaseBeamTiltX + delBTX, mBaseBeamTiltY + delBTY);
  }
  mLastISX = ISX;
  mLastISY = ISY;
}

/*
 * Start an acquisition: Set up early return, start shot, set the status pane
 */
int CParticleTasks::StartOneShotOfMulti(void)
{
  CString str;
  if (mMSIfEarlyReturn && mCamera->SetNextAsyncSumFrames(mMSEarlyRetFrames < 0 ? 65535 : 
    mMSEarlyRetFrames, false)) {
    StopMultiShot();
    return 1;
  }
  mCamera->InitiateCapture(RECORD_CONSET);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_SHOT, 0, 0);
  str.Format("DOING MULTI-SHOT %d OF %d", mMSCurIndex + (mMSDoCenter < 0 ? 2 : 1), 
    mMSNumPeripheral + (mMSDoCenter ? 1 : 0));
  mWinApp->SetStatusText(MEDIUM_PANE, str);
  return 0;
}

