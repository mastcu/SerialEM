// DistortionTasks.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "SerialEMDoc.h"
#include "DistortionTasks.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "Utilities\KGetOne.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define USER_CONSET 3
#define SHOT_CONSET 3

enum {SP_BACKLASH_MOVE, SP_FIRST_MOVE, SP_SECOND_MOVE, SP_FIRST_SHOT, SP_SECOND_SHOT};

/////////////////////////////////////////////////////////////////////////////
// CDistortionTasks

//IMPLEMENT_DYNCREATE(CDistortionTasks, CCmdTarget)

CDistortionTasks::CDistortionTasks()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSets = mWinApp->GetConSets();
  mImBufs = mWinApp->GetImBufs();
  mMagTab = mWinApp->GetMagTable();

  mIterCount = 0;
  mIterLimit = 16;
  mMajorDimMinOverlap = 0.48f;
  mMajorDimMaxOverlap = 0.60f;
  mMinorDimMinOverlap = 0.94f;
  mDiagonalMinOverlap = 0.65f;
  mDiagonalMaxOverlap = 0.75f;
  mDirectionIndex = 0;
  mStageBacklash = 5.;
  mStageDelay = 0;   // Use default
  mMaxMoveToCheck = 0.;
}

CDistortionTasks::~CDistortionTasks()
{
}


BEGIN_MESSAGE_MAP(CDistortionTasks, CCmdTarget)
	//{{AFX_MSG_MAP(CDistortionTasks)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDistortionTasks message handlers

void CDistortionTasks::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mShiftManager = mWinApp->mShiftManager;

}

void CDistortionTasks::SetOverlaps()
{
  if (!KGetOneFloat("Minimum overlap along dimension that separates side-by-side pairs:",
    mMajorDimMinOverlap, 2))
    return;
  if (!KGetOneFloat("Maximum overlap along dimension that separates side-by-side pairs:",
    mMajorDimMaxOverlap, 2))
    return;
  if (!KGetOneFloat("Minimum overlap along dimension that lines up in side-by-side pairs:"
    , mMinorDimMinOverlap, 2))
    return;
  if (!KGetOneFloat("Minimum overlap in either dimension in diagonal pairs:",
    mDiagonalMinOverlap, 2))
    return;
  KGetOneFloat("Maximum overlap in either dimension in diagonal pairs:",
    mDiagonalMaxOverlap, 2);
}

void CDistortionTasks::CalibrateDistortion() 
{
  ControlSet *conSet = mConSets + USER_CONSET;
  float sizeX, sizeY;
  int which;
  float targetShiftX, targetShiftY;
  double ISX, ISY, stageX, stageY, stageZ;
  CString report;
  double angle = DTOR * mWinApp->mScope->GetTiltAngle();
  float xTiltFac = (float)(HitachiScope ? cos(angle) : 1.);
  float yTiltFac = (float)(HitachiScope ? 1. : cos(angle));

  ScaleMat aMat = mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), 
    mScope->GetMagIndex());

  mBinning = conSet->binning;
  mAInv = mShiftManager->MatInv(aMat);
  mAInv.xpx /= xTiltFac;
  mAInv.xpy /= xTiltFac;
  mAInv.ypx /= yTiltFac;
  mAInv.ypy /= yTiltFac;

  if (!KGetOneInt("Direction to move (0-7; 0=right, 1=45 deg, 2=up, etc):", 
    mDirectionIndex))
    return;
  if (mDirectionIndex < 0)
    mDirectionIndex = 0;
  if (mDirectionIndex > 7)
    mDirectionIndex = 7;

  // Get image size
  sizeX = (float)((conSet->right - conSet->left) / mBinning);
  sizeY = (float)((conSet->bottom - conSet->top) / mBinning);

  // Get min and max allowed shifts as function of direction
  switch (mDirectionIndex) {
  case 0:
    mMaxShiftY = sizeY * (1.f - mMinorDimMinOverlap);
    mMinShiftY = - mMaxShiftY;
    mMinShiftX = sizeX * (1.f - mMajorDimMaxOverlap);
    mMaxShiftX = sizeX * (1.f - mMajorDimMinOverlap);
    break;
    
  case 4:
    mMaxShiftY = sizeY * (1.f - mMinorDimMinOverlap);
    mMinShiftY = - mMaxShiftY;
    mMinShiftX = -sizeX * (1.f - mMajorDimMinOverlap);
    mMaxShiftX = -sizeX * (1.f - mMajorDimMaxOverlap);
    break;
    
  case 2:
    mMaxShiftX = sizeX * (1.f - mMinorDimMinOverlap);
    mMinShiftX = - mMaxShiftX;
    mMinShiftY = sizeY * (1.f - mMajorDimMaxOverlap);
    mMaxShiftY = sizeY * (1.f - mMajorDimMinOverlap);
    break;
  case 6:
    mMaxShiftX = sizeX * (1.f - mMinorDimMinOverlap);
    mMinShiftX = - mMaxShiftX;
    mMinShiftY = -sizeY * (1.f - mMajorDimMinOverlap);
    mMaxShiftY = -sizeY * (1.f - mMajorDimMaxOverlap);
    break;
    
  case 1:
    mMinShiftX = sizeX * (1.f - mDiagonalMaxOverlap);
    mMaxShiftX = sizeX * (1.f - mDiagonalMinOverlap);
    mMinShiftY = sizeY * (1.f - mDiagonalMaxOverlap);
    mMaxShiftY = sizeY * (1.f - mDiagonalMinOverlap);
    break;

  case 3:
    mMinShiftX = -sizeX * (1.f - mDiagonalMinOverlap);
    mMaxShiftX = -sizeX * (1.f - mDiagonalMaxOverlap);
    mMinShiftY = sizeY * (1.f - mDiagonalMaxOverlap);
    mMaxShiftY = sizeY * (1.f - mDiagonalMinOverlap);
    break;
  case 5:
    mMinShiftX = -sizeX * (1.f - mDiagonalMinOverlap);
    mMaxShiftX = -sizeX * (1.f - mDiagonalMaxOverlap);
    mMinShiftY = -sizeY * (1.f - mDiagonalMinOverlap);
    mMaxShiftY = -sizeY * (1.f - mDiagonalMaxOverlap);
    break;
  case 7:
    mMinShiftX = sizeX * (1.f - mDiagonalMaxOverlap);
    mMaxShiftX = sizeX * (1.f - mDiagonalMinOverlap);
    mMinShiftY = -sizeY * (1.f - mDiagonalMinOverlap);
    mMaxShiftY = -sizeY * (1.f - mDiagonalMaxOverlap);
    break;
  }


  // Make sure image shift is where the user wants it
  mScope->GetImageShift(ISX, ISY);
  if (fabs(ISX) > 0.05 || fabs(ISY) > 0.05) {
    which = SEMThreeChoiceBox("Image shift is not near zero; this could affect the "
      "estimate of the distortion field\n\n"
      "Press:\n\"Reset IS\" to reset the image shift to zero and move stage to "
      "compensate,\n\n"
      "\"Leave IS Nonzero\" to leave image shift where it is, or\n\n\"Cancel\" to abort"
      " the procedure", "Reset IS", "Leave IS Nonzero", "Cancel", 
      MB_YESNOCANCEL | MB_ICONQUESTION);
    if (which == IDCANCEL)
      return;
    if (which == IDYES) {
      if (mShiftManager->ResetImageShift(false, false) ||
        mScope->WaitForStageReady(10000)) {
        AfxMessageBox("Timeout trying to reset image shifts", MB_EXCLAME);
        return;
      }
    }
  }

  if (mScope->WaitForStageReady(1000)) {
    AfxMessageBox("Stage is not ready to use, cannot proceed", MB_EXCLAME);
    return;
  }

  report.Format("Allowed range for shift: %.0f to %.0f in X; %.0f to %.0f in Y",
    mMinShiftX, mMaxShiftX, mMinShiftY, mMaxShiftY); 
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);

  // Get target image shift as center of ranges, and target stage move
  targetShiftX = (mMinShiftX + mMaxShiftX) / 2.f;
  targetShiftY = (mMinShiftY + mMaxShiftY) / 2.f;
  mTargetMoveX = mBinning * (mAInv.xpx * targetShiftX + mAInv.xpy * targetShiftY);
  mTargetMoveY = mBinning * (mAInv.ypx * targetShiftX + mAInv.ypy * targetShiftY);

  report.Format("Target stage move: %.2f  %.2f", mTargetMoveX, mTargetMoveY);
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);

  mScope->GetStagePosition(stageX, stageY, stageZ);
  mCenterInfo.x = stageX;
  mCenterInfo.y = stageY;
  mCenterInfo.z = stageZ;
  mCenterInfo.axisBits = axisXY;

  // Define backlash from sign of stage move
  mCenterInfo.backX = mTargetMoveX > 0 ? -mStageBacklash : mStageBacklash;
  mCenterInfo.backY = mTargetMoveY > 0 ? -mStageBacklash : mStageBacklash;

  mCumMoveDiffX = 0;
  mCumMoveDiffY = 0;
  mStageMoveX = mTargetMoveX;
  mStageMoveY = mTargetMoveY;
  mIterCount = 1;
  mPredictedMoveX.clear();
  mPredictedMoveY.clear();
  mFinishing = false;
  mSaveAlignOnSave = mWinApp->mBufferManager->GetAlignOnSave();
  mWinApp->mBufferManager->SetAlignOnSave(false);
  mCamera->SetRequiredRoll(1);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "GETTING STAGE PAIRS");
  SPDoFirstMove();
}

void CDistortionTasks::SPNextTask(int param)
{
  float shiftX, shiftY, bShiftX, bShiftY, actualMoveX, actualMoveY;
  int numAvg, ind, maxAvg = 3;
  CString report;

  if (!mIterCount)
    return;
  
  switch (param) {

  case SP_FIRST_MOVE:
    // Test if terminating
    if (mFinishing) {
      StopStagePairs();
      return;
    }
    if (mStageDelay > 0)
      mWinApp->mShiftManager->SetStageDelayToUse(mStageDelay);
    mCamera->InitiateCapture(SHOT_CONSET);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_DISTORTION_STAGEPAIR, SP_FIRST_SHOT, 
      0);
    return;

  case SP_FIRST_SHOT:
    mMoveInfo = mCenterInfo;
    mMoveInfo.x += mStageMoveX;
    mMoveInfo.y += mStageMoveY;
    mScope->MoveStage(mMoveInfo, false);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_DISTORTION_STAGEPAIR, 
        SP_SECOND_MOVE, 60000);
    return;

  case SP_SECOND_MOVE:
    if (mStageDelay > 0)
      mWinApp->mShiftManager->SetStageDelayToUse(mStageDelay);
    mCamera->InitiateCapture(SHOT_CONSET);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_DISTORTION_STAGEPAIR, 
        SP_SECOND_SHOT, 0);
    return;

  case SP_SECOND_SHOT:
    // 11/30/11: Call with argument not to shift so it doesn't need to be restored
    mShiftManager->AutoAlign(1, 0, false, false, NULL, (mMinShiftX + mMaxShiftX) / 2.f,
      -(mMinShiftY + mMaxShiftY) / 2.f);
    mImBufs->mImage->getShifts(shiftX, shiftY);
    mImBufs[1].mImage->getShifts(bShiftX, bShiftY);
    shiftX -= bShiftX;
    shiftY -= bShiftY;
    mImBufs[1].mImage->setShifts(0., 0.);
    mImBufs->mImage->setShifts(shiftX, shiftY);
    mImBufs->SetImageChanged(1);
    mWinApp->mMainView->DrawImage();
    shiftY = -shiftY;

    report.Format("Trial %d: shift between images is %.0f in X, %.0f in Y", 
      mIterCount, shiftX, shiftY);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);

    if ((mMaxMoveToCheck > 0. && sqrt(pow(mTargetMoveX, 2.f) + pow(mTargetMoveY, 2.f)) >
      mMaxMoveToCheck) || (shiftX > mMinShiftX && shiftX < mMaxShiftX &&
      shiftY > mMinShiftY && shiftY < mMaxShiftY)) {
      // Save images
      mWinApp->SetCurrentBuffer(1);
      mWinApp->mDocWnd->SaveActiveBuffer();
      mWinApp->SetCurrentBuffer(0);
      mWinApp->mDocWnd->SaveRegularBuffer();
      mWinApp->AppendToLog("Shifts acceptable, pair saved to file", LOG_OPEN_IF_CLOSED);
      mFinishing = true;
      SPDoFirstMove();
      return;
    }

    if (mIterCount >= mIterLimit) {
      mWinApp->AppendToLog("Too many iterations, giving up", LOG_OPEN_IF_CLOSED);
      mFinishing = true;
      SPDoFirstMove();
      return;
    }

    // Keep track of adjusted move implied by this move's error and then average the
    // most recent adjusted moves.  This seems better than old way that averaged the
    // prediction over all moves, which would not move fast enough to solution
    actualMoveX = mBinning * (mAInv.xpx * shiftX + mAInv.xpy * shiftY);
    actualMoveY = mBinning * (mAInv.ypx * shiftX + mAInv.ypy * shiftY);
    mPredictedMoveX.push_back(mStageMoveX + mTargetMoveX - actualMoveX);
    mPredictedMoveY.push_back(mStageMoveY + mTargetMoveY - actualMoveY);
    mStageMoveX = 0.;
    mStageMoveY = 0.;
    numAvg = B3DMIN((int)mPredictedMoveX.size(), maxAvg);
    for (ind = 0; ind < numAvg; ind++) {
      mStageMoveX += mPredictedMoveX[mPredictedMoveX.size() - 1 - ind] / numAvg;
      mStageMoveY += mPredictedMoveY[mPredictedMoveY.size() - 1 - ind] / numAvg;
    }
    report.Format("Adjusted stage move: %.2f  %.2f", mStageMoveX, mStageMoveY);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    mIterCount++;
    SPDoFirstMove();
    return;
  }

}

void CDistortionTasks::SPCleanUp(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out in stage-move pairs routine"), MB_EXCLAME);
  StopStagePairs();
  mWinApp->ErrorOccurred(error);
}

void CDistortionTasks::StopStagePairs()
{
  if (!mIterCount)
    return;
  if (!mFinishing) {
    mScope->MoveStage(mCenterInfo, true);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  }
  mIterCount = 0;
  mWinApp->mBufferManager->SetAlignOnSave(mSaveAlignOnSave);
  mCamera->SetRequiredRoll(0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

void CDistortionTasks::SPDoFirstMove()
{
  mScope->MoveStage(mCenterInfo, true);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_DISTORTION_STAGEPAIR, SP_FIRST_MOVE, 
    60000);
}

