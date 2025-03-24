// MultiGridTasks.cpp : For operations involved in handling multiple grids
//
// Copyright (C) 2024 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "MultiGridTasks.h"
#include "ComplexTasks.h"
#include "EMscope.h"
#include "StateDlg.h"
#include "LogWindow.h"
#include "ProcessImage.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "EMbufferManager.h"
#include "NavHelper.h"
#include "ParticleTasks.h"
#include "HoleFinderDlg.h"
#include "MultiShotDlg.h"
#include "AutoContouringDlg.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"
#include "MultiGridDlg.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "MacroProcessor.h"
#include "Shared\b3dutil.h"
#include "Shared\autodoc.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include <algorithm>
#include <direct.h>

static const char *sActionNames[] = {"Remove objective aperture", "Set condenser aperture"
, "Replace objective aperture", "Move to reference position", "Restore condenser aperture"
, "Set grid map state", "Unload grid", "Take reference image", "Move to survey position",
"Take survey image", "Find eucentricity", "Move to center of grid map", "Load grid",
"Take grid map", "Realign to grid map", "Autocontour grid squares", "Set MMM state",
"Take MMMs", "Set state for final acquire", "Do final acquisition", 
"Set up MMM files", "Restore state", "Turn Low Dose on", "Turn Low Dose off", 
"Set Z height", "Check & apply Shift to Marker", "Set Focus area position", "Run script",
"Refine grid alignment", "Grid done"};


CMultiGridTasks::CMultiGridTasks()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mReplaceSpaces = true;
  mAllowOptional = false;
  mUseCaretToReplace = false;
  mDoingMultiGrid = 0;
  mDoingMulGridSeq = false;
  mSuspendedMulGrid = false;
  mRRGcurDirection = -2;
  mRRGMaxRotation = 15.f;
  mRRGmaxCenShift = 50.;
  mRRGShiftLimitForXform = 70.;
  mRRGRotLimitForXform = 17.f;
  mRRGResidLimitForXform = 7.5f;
  mParams.appendNames = false;
  mParams.useSubdirectory = true;
  mParams.LMMstateType = 0;
  mParams.setLMMstate = false;
  mParams.LMMstateNum = 0;
  mParams.removeObjectiveAp = false;
  mParams.setCondenserAp = false;
  mParams.condenserApSize = 150;
  mParams.C1orC2condenserAp = 0;
  mParams.LMMmontType = 0;
  mParams.LMMnumXpieces = 5;
  mParams.LMMnumYpieces = 5;
  mParams.MMMnumXpieces = 2;
  mParams.MMMnumYpieces = 2;
  mParams.LMMoverlapPct = 10;
  mParams.setLMMoverlap = true;
  mParams.autocontour = false;
  mParams.MMMstateType = 0;
  mParams.MMMimageType = 0;
  mParams.acquireLMMs = true;
  mParams.acquireMMMs = false;
  mParams.runFinalAcq = false;
  mParams.runMacroAfterLMM = false;
  mParams.refineAfterRealign = false;
  mParams.refineImageType = 1;
  mParams.refineStateNum = 0;
  mParams.macroToRun = 0;
  for (int i = 0; i < 4; i++) {
    mParams.MMMstateNums[i] = -1;
    mParams.finalStateNums[i] = -1;
    mMontSetupConsetSizes[i] = 0;
  }
  mParams.framesUnderSession = false;
  mParams.msVectorSource = 0;
  mPctStatMidCrit = 0.33f;
  mPctStatRangeCrit = 0.45f;
  mPctRightAwayFrac = 0.5f;
  mMinFracOfPiecesUsable = 0.15f;
  mPctUsableFrac = 0.05f;
  mAppendNames = false;
  mStartedLongOp = false;
  mRestoringOnError = false;
  mNumMMMstateCombos = 2;
  mNumFinalStateCombos = 4;
  mSavedMultiShot = NULL;
  mSavedHoleFinder = NULL;
  mSavedAutoCont = NULL;
  mSavedGeneralParams = NULL;
  mSavedFocusPos = NULL;
  mCamNumForFrameDir = -1;
  InitOrClearSessionValues();
  mAdocChanged = false;
  mSkipGridRealign = false;
  mRRGstore = NULL;
  mRRGdidSaveState = false;
  mUseCustomOrder = false;
  mLastNumSlots = 0;
  mRefineMinField = 8.;
  mShowRefineAfterRealign = false;
  mRefineUsedLDorState = 0;
  mMaxSizeForRefinePolys = 250.;
  mMaxRefineShiftDiff = 5.;
}


CMultiGridTasks::~CMultiGridTasks()
{
}

void CMultiGridTasks::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mShiftManager = mWinApp->mShiftManager;
  mNavHelper = mWinApp->mNavHelper;
  mCartInfo = mScope->GetJeolLoaderInfo();
  mStateArray = mNavHelper->GetStateArray();
  mSingleGridMode = !mScope->GetScopeHasAutoloader();
  if (!mShowRefineAfterRealign && !mSkipGridRealign)
    mParams.refineAfterRealign = false;
  mUseTwoJeolCondAp = JEOLscope && mScope->GetJeolHasExtraApertures() && 
    mScope->GetJeolCondenserApIDtoUse() < 0;
  mSingleCondenserAp = mScope->GetJeolCondenserApIDtoUse() < 0 ? CONDENSER_APERTURE :
    mScope->GetJeolCondenserApIDtoUse();
}

/*
 * Clear out non-text items for a new session 
 */
void CMultiGridTasks::InitOrClearSessionValues()
{
  mInitializedGridMont = false;
  mInitializedMMMmont = false;
  mReferenceCounts = 0.;
  mLMMneedsLowDose = 0;
  mLMMusedStateNum = -1;
  mHaveAutoContGroups = false;
  mLMMmagIndex = 0;
  mSkipMarkerShifts = 0;
  mNamesLocked = 0;
  mLoadedGridIsAligned = 0;
}

/*
 * Function to replace undesirable characters in grid name from scope or user
 */
int CMultiGridTasks::ReplaceBadCharacters(CString &str)
{
  const char *mandatory = "`\'\"$?:*\\/";
  const char *optional = "!#%&(){};|";
  const char *charList = mandatory;
  int indCh, loop, retVal = 0;
  char replacement = B3DCHOICE(mReplaceSpaces, mUseCaretToReplace ? '^' : '@', '_');
  for (loop = 0; loop < (mAllowOptional ? 1 : 2); loop++) {
    for (indCh = 0; indCh < (int)strlen(charList); indCh++)
      if (str.Find(charList[indCh]) >= 0)
        retVal += str.Replace(charList[indCh], replacement);
    charList = optional;
  }
  for (indCh = 0; indCh < str.GetLength(); indCh++) {
    if ((unsigned char)(str.GetAt(indCh)) > 127) {
      str.SetAt(indCh, replacement);
      retVal++;
    }
  }
  if (mReplaceSpaces && str.Find(' ') >= 0)
    retVal += str.Replace(' ', '_');
  return retVal;
}

/*
 * Get the routine in creative ways frm the user name
 */
CString CMultiGridTasks::RootnameFromUsername(JeolCartridgeData &jcd)
{
  CString str, str2;
  int ind1, ind2, lenPref = mPrefix.GetLength();
  int extra = 0, nameLen = jcd.userName.GetLength();
  bool skipC = !mScope->GetScopeHasAutoloader();
  char last;
  str = mPrefix;

  // If name is being appended and contains Car and the right grid #,, do not add C#
  if (mAppendNames && jcd.userName.Find("Car") == 0 && nameLen > 3) {
    str2 = jcd.userName.Mid(3);
    if (atoi((LPCTSTR)str2) == jcd.id)
      skipC = true;
  }
  if (!skipC) {

    // If prefix ends with - or _, take into account when seeing if it already has Car or
    //  grid
    if (lenPref > 1) {
      last = str.GetAt(lenPref - 1);
      if (last == '-' || last == '_')
        extra = 1;
    }

    // See if it ends with car or grid and do not add C
    ind1 = str.Find("Car");
    ind2 = str.Find("car");
    ACCUM_MAX(ind1, ind2);
    if (ind1 >= 0 && (ind1 == lenPref - (3 + extra))) {
      skipC = true;
    } else {
      ind1 = str.Find("Grid");
      ind2 = str.Find("grid");
      ACCUM_MAX(ind1, ind2);
      if (ind1 >= 0 && (ind1 == lenPref - (4 + extra)))
        skipC = true;
    }

    // Compose number and whatever preceeds it, if anything; add to prefix
    if (lenPref > 0 && !extra && mNamesLocked > 1)
      str += "_";
    if (!skipC)
      str += "C";
    str2.Format("%d", jcd.id);
    str += str2;
  }

  // Add name
  if (mAppendNames && !jcd.userName.IsEmpty())
    str += "_" + jcd.userName;
  return str;
}

/*
 * Call to get an inventory
 */
int CMultiGridTasks::GetInventory()
{
  int err, operations = LONG_OP_INVENTORY;
  float hours = 0.;
  err = mScope->StartLongOperation(&operations, &hours, 1);
  if (err) {
    SEMMessageBox("Could not run inventory; " + CString(err == 1 ?
      "the long operation thread is busy" : "program error starting operation"));
    return 1;
  }
  mWinApp->SetStatusText(MEDIUM_PANE, "RUNNING INVENTORY");
  mDoingMultiGrid = MG_INVENTORY;
  mStartedLongOp = true;
  if (mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->UpdateEnables();
  mWinApp->AddIdleTask(TASK_MULTI_GRID, MG_INVENTORY, 0);
  return 0;
}

/*
 * Call to load or unload a grid, used interactively or from sequence
 */
int CMultiGridTasks::LoadOrUnloadGrid(int slot, int taskParam)
{
  int err;
  CString errStr;
  mStartedLongOp = true;
  if (mScope->StageBusy()) {
    SEMTrace('q', "Stage busy before load/unload!!! waiting until done");
    mScope->WaitForStageReady(15000);
  }
  if (slot > 0) {
    SEMTrace('q', "loading slot/index %d", slot);
    err = mScope->LoadCartridge(slot, errStr);
  } else {
    err = mScope->UnloadCartridge(errStr);
  }
  if (JEOLscope && err == 5 && mUnloadErrorOK > 0)
    err = 0;
  if (err) {
    mStartedLongOp = false;
    SEMMessageBox("Could not " + CString(slot > 0 ? "load" : "unload") + " grid: " +
      errStr);
    return err;
  }
  mWinApp->SetStatusText(MEDIUM_PANE, slot > 0 ? "LOADING GRID" : "UNLOADING GRID");
  if (!mDoingMultiGrid) {
    mDoingMultiGrid = taskParam;
    mWinApp->UpdateBufferWindows();
  }
  mWinApp->AddIdleTask(TASK_MULTI_GRID, taskParam, 300000);
  return 0;
}

/*
 *  Multipurpose routine for next task
 */
void CMultiGridTasks::MultiGridNextTask(int param)
{
  int ind, id;
  JeolCartridgeData jcd;
  CString str;
  int curInd = B3DMAX(0, mRRGcurDirection);
  bool forMulGrid = param == MG_RUN_SEQUENCE || param == MG_RESTORING;

  // A tricky test for when the relevant operation is already stopped
  if ((!mDoingMultiGrid && !forMulGrid) || (!mDoingMulGridSeq && forMulGrid))
    return;

  switch (param) {

    // Inventory or internal load or unload, just stop
  case MG_INVENTORY:
  case MG_SEQ_LOAD:
    StopMultiGrid();
    return;

    // After a load, get the cartridge at stage and load the Nav file
  case MG_USER_LOAD:
    ind = mScope->FindCartridgeAtStage(id);
    if (ind >= 0) {
      jcd = mCartInfo->GetAt(ind);
      if (jcd.status & MGSTAT_FLAG_LM_MAPPED) {
        str = FullGridFilePath(-ind - 1, ".nav");
        if (mWinApp->mNavigator)
          mWinApp->mNavigator->SaveAndClearTable();
        mWinApp->mMenuTargets.OpenNavigatorIfClosed();
        mWinApp->mNavigator->LoadNavFile(false, false, &str);
      }
    }
    StopMultiGrid();
    return;

    // Realign moved stage
  case MG_RRG_MOVED_STAGE:
    mWinApp->SetStatusText(SIMPLE_PANE, "");
    if (mBigRotation || mRRGcurDirection >= 0) {
      for (ind = mWinApp->mBufferManager->GetShiftsOnAcquire(); ind > 0; ind--)
        mWinApp->mBufferManager->CopyImageBuffer(ind - 1, ind);

      // Load image and rotate it with current rotation matrix
      if (mWinApp->mBufferManager->ReadFromFile(mRRGstore, mRRGpieceSec[curInd],
        0, true)) {
        StopMultiGrid();
        return;
      }
      if (mNavHelper->RotateForAligning(0, &mRRGrotMat)) {
        StopMultiGrid();
        return;
      }
    }

    mCamera->InitiateCapture(mNavHelper->GetRIstayingInLD() ? 
      mNavHelper->GetRIconSetNum() : TRACK_CONSET);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_RRG_TOOK_IMAGE, 0);
    break;

    // Realign took image
  case MG_RRG_TOOK_IMAGE:
    AlignNewReloadImage();
    break;

    // Refine took image or moved stage
  case MG_REFINE_IMAGE:
  case MG_REFINE_MOVED:
    RefineRealignNextTask(param);
    break;

    // Action in MulGrid sequence:  do next or ignore if doing restore
  case MG_RUN_SEQUENCE:
    if (mRestoringOnError)
      break;
    DoNextSequenceAction(0);
    break;

    // Restoring after stop, return to stop return
  case MG_RESTORING:
    StopMulGridSeq();
    break;
  }
}

/*
 * Busy function for grid load/unload and realign task
 */
int CMultiGridTasks::MultiGridBusy()
{
  if (mStartedLongOp) 
    return mScope->LongOperationBusy();
  return 0;
}

/*
 * Cleanup function
 */
void CMultiGridTasks::CleanupMultiGrid(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out in a multiple grid handling routine"));
  StopMultiGrid();
  mWinApp->ErrorOccurred(error);
}

/*
 * Stop routine for grid load/unload and realign task
 */
void CMultiGridTasks::StopMultiGrid()
{
  if (!mDoingMultiGrid)
    return;

  // Reset things from the reload realign
  if (mDoingMultiGrid == MG_REALIGN_RELOADED) {
    if (mRRGcurStore < 0)
      B3DDELETE(mRRGstore);
    mCamera->SetRequiredRoll(0);
    mNavHelper->RestoreLowDoseConset();
    mNavHelper->RestoreMapOffsets();
    if (mRRGdidSaveState)
      mNavHelper->RestoreFromMapState();
    else if (!mNavHelper->GetRIstayingInLD() && mWinApp->mLowDoseDlg.m_bLowDoseMode ? 1 : 0)
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    mRRGdidSaveState = false;
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }
  RestoreFromRefine();

  mDoingMultiGrid = 0;
  mWinApp->SetBufToggleCount(0);
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
}

/*
 * Busy routine for multigrid sequence
 */
int CMultiGridTasks::MulGridSeqBusy()
{
  if (mStartedLongOp)
    return mScope->LongOperationBusy();
  if ((mMovedAperture && mScope->GetMovingAperture()) || mDoingMultiGrid ||
    (mDoingEucentricity && mWinApp->mComplexTasks->DoingEucentricity()) ||
    (mDoingMontage && mWinApp->mMontageController->DoingMontage()) ||
    (mAutoContouring && mNavHelper->mAutoContouringDlg->DoingAutoContour()) ||
    (mRunningMacro && mWinApp->mMacroProcessor->DoingMacro()) ||
    (mSettingUpNavFiles && mWinApp->mNavigator&& mWinApp->mNavigator->DoingNewFileRange())
    || (mStartedNavAcquire && mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()))
    return 1;
  return 0;
}

/*
 * Error cleanup for multigrid sequence
 */
void CMultiGridTasks::CleanupMulGridSeq(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out in the multiple grid sequence routine"));
  StopMulGridSeq();
  mWinApp->ErrorOccurred(error);
}

/*
 * Stop function for multigrid sequence
 */
void CMultiGridTasks::StopMulGridSeq()
{
  MontParam *montP;
  HoleFinderParams *hfParam = mNavHelper->GetHoleFinderParams();
  if (!mDoingMulGridSeq || mWinApp->GetAppExiting())
    return;

  // restore state and lowdose
  if (mStateWasSet)
    RestoreState();
  if (!BOOL_EQUIV(mWinApp->LowDoseMode(), mSavedLowDose))
    mWinApp->mLowDoseDlg.SetLowDoseMode(mSavedLowDose);
  montP = mWinApp->GetMontParam();
  *montP = mSaveMasterMont;
  RestoreImposedParams();
  ReopenMainLog(-1);
  mCamNumForFrameDir = -1;

  // Start restoring apertures, loop back in until done
  if (mReinsertObjAp || mRestoreCondAp) {
    mMovedAperture = true;
    mRestoringOnError = true;
    if (mReinsertObjAp) {
      mScope->SetApertureSize(OBJECTIVE_APERTURE, mInitialObjApSize);
      mReinsertObjAp = false;
    } else if (mRestoreCondAp > 0) {
      if (mRestoreCondAp > 1 && mUseTwoJeolCondAp)
        mScope->SetApertureSize(JEOL_C1_APERTURE, mInitialC1CondSize);
      else
        mScope->SetApertureSize(mSingleCondenserAp, mInitialCondApSize);
      mRestoreCondAp--;
    }
    mWinApp->SetStatusText(MEDIUM_PANE, "RESTORING APERTURE");
    mWinApp->AddIdleTask(TASK_MULGRID_SEQ, MG_RESTORING, 0);
    return;
  }
  CommonMulGridStop(false);
}

#define RESTORE_PARAMS(grName, svName) \
  if (mSaved##svName) {   \
    GridTo##grName##Settings(*mSaved##svName);  \
    delete mSaved##svName;   \
    mSaved##svName = NULL;   \
  }

/*
 * Restore saved parameters if ones were imposed
 */
void CMultiGridTasks::RestoreImposedParams()
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  RESTORE_PARAMS(MultiShot, MultiShot);
  RESTORE_PARAMS(HoleFinder, HoleFinder);
  RESTORE_PARAMS(AutoCont, AutoCont);
  RESTORE_PARAMS(General, GeneralParams);
  RESTORE_PARAMS(FocusPos, FocusPos);

  // Restore camera settings if saved: this can happen multiple times
  if (mCamNumForFrameDir >= 0) {
    mCamera->SetCameraFrameFolder(&camParams[mCamNumForFrameDir], mSavedDirForFrames);
    if (!mSavedFrameBaseName.IsEmpty())
      mCamera->SetFrameBaseName(mSavedFrameBaseName);
    if (mLastSetFrameNum > -2 && mLastSetFrameNum != mCamera->GetLastUsedFrameNumber() &&
      mCurrentGrid >= 0 && mCurrentGrid < mCartInfo->GetSize()) {
      JeolCartridgeData &jcdEl = mCartInfo->ElementAt(mJcdIndsToRun[mCurrentGrid]);
      jcdEl.lastFrameNumber = mCamera->GetLastUsedFrameNumber();
      mAdocChanged = true;
    }
    mLastSetFrameNum = -2;
    mCamera->SetLastUsedFrameNumber(mSavedLastFrameNum);
    mCamera->SetNumberedFramePrefix(mSavedNumberedPrefix);
    mCamera->SetNumberedFrameFolder(mSavedNumberedFolder);
  }
}

/*
 * If the current log is not the main log, close it and reopen the main one, giving a
 * message about the previous acquire if idVal is not negative.
 */
void CMultiGridTasks::ReopenMainLog(int idVal)
{
  CString str = mWinApp->mLogWindow->GetSaveFile();
  if (mMainLogName.IsEmpty() || !str.CompareNoCase(mMainLogName))
    return;
  mWinApp->mLogWindow->DoSave();
  mWinApp->mLogWindow->CloseLog();
  mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
  if (idVal >= 0)
    PrintfToLog("Navigator Acquire %s for grid # %d", mNavigator->GetAcquireEnded() ?
      "finished" : "failed", idVal);
  else
    PrintfToLog("Multiple grid operations ended");
  SEMAppendToLog("Log for Acquire is in " + str);
  mWinApp->mLogWindow->ReadAndAppend(mMainLogName);

}

/*
 * Externally called suspend routine
 */
void CMultiGridTasks::SuspendMulGridSeq()
{
  if (!mDoingMulGridSeq) 
    return;
  CommonMulGridStop(true);
}

/*
 * Common routine to set flags and status text and update when stopping/suspending
 */
void CMultiGridTasks::CommonMulGridStop(bool suspend)
{
  mSuspendedMulGrid = suspend;
  mDoingMulGridSeq = false;
  mRestoringOnError = false;
  mWinApp->mNavigator->ManageListHeader("");
  mWinApp->SetStatusText(COMPLEX_PANE, suspend ? "MULTIGRID SUSPENDED" : "");
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
  mWinApp->FlashTaskbar();
}

/*
 * End simply sets a flag when running, or completes the stop when suspended
 */
void CMultiGridTasks::EndMulGridSeq()
{ 
  if (mDoingMulGridSeq) {
    mEndWhenGridDone = true;
    mWinApp->SetStatusText(COMPLEX_PANE, "PAUSING MULTIGRID");
  } else if (mSuspendedMulGrid && !mRestoringOnError) {
    mDoingMulGridSeq = true;
    mSuspendedMulGrid = false;
    StopMulGridSeq();
  }
}

/*
 * To resume, restore doing flag, clear suspended flag, and either set a new idle task
 * when resume is secondary to some other routine restarting, or call next action routine
 */
int CMultiGridTasks::ResumeMulGridSeq(int resumeType)
{
  if (!mSuspendedMulGrid || mDoingMulGridSeq)
    return 1;
  mSuspendedMulGrid = false;
  mDoingMulGridSeq = true;
  mStoppedAtGridEnd = false;
  mEndWhenGridDone = false;
  if (resumeType <= 0) {
    if (!resumeType)
      mSeqIndex--;
    mDoingMontage = mStoppedInLMMont;
    mStartedNavAcquire = mStoppedInNavRun;
    mWinApp->AddIdleTask(TASK_MULGRID_SEQ, MG_RUN_SEQUENCE, 0);
    return 0;;
  }

  DoNextSequenceAction(resumeType);
  return 0;
}

/*
 * When the user wants to resume, ask if it should repeat step, do next next, or skip
 * to next grid and call resume function with the choice
 */
void CMultiGridTasks::UserResumeMulGridSeq()
{
  CString mess;
  int resume,answer;
  if (!mSuspendedMulGrid || mDoingMulGridSeq) {
    SEMMessageBox("Program error: the multi-grid operation is not resumable");
    return;
  }
  if (mCurrentGrid < 0 || mCurrentGrid >= (int)mCartInfo->GetSize()) {
    mess.Format("Cannot resume; the current grid number is no longer\nvalid (value %d, "
      "table size %d)\n\nPlease report the steps leading to this problem to the "
      "developers", mCurrentGrid, mCartInfo->GetSize());
    SEMMessageBox(mess);
    return;
  }
  if (mStoppedAtGridEnd) {
    resume = 1;
  } else {
    mess.Format("The last multi-grid operation run was \"%s\"\n"
      "The next operation will be \"%s\".  Select:\n\n"
      "\"Redo Last\" to resume by redoing the last operation (which may be inappropriate)"
      "\n\n\"Do Next\" to start with the next operation",
      sActionNames[mActSequence[B3DMAX(mSeqIndex - 1, 0)]],
      sActionNames[mActSequence[mSeqIndex]]);
    if (!mSingleGridMode)
      mess += "\n\n\"Skip to Next Grid\" to abandon this grid and go to the next one";
    answer = SEMThreeChoiceBox(mess, "Redo Last", "Do Next", 
      mSingleGridMode ? "" : "Skip to Next Grid", mSingleGridMode ?
      MB_QUESTION : MB_YESNOCANCEL);
    if (answer == IDYES)
      resume = 1;
    else if (answer == IDNO)
      resume = 2;
    else
      resume = 3;
  }
  ResumeMulGridSeq(resume);
}

/*
 * Store the error string from a SEMMessageBox call from any external task
 */
void CMultiGridTasks::ExternalTaskError(CString &errStr)
{
  mExternalFailedStr = errStr;
}

/*
 * The other route for errors is through ErrorOccurred but user STOP comes this way too
 */
void CMultiGridTasks::SetExtErrorOccurred(int inVal)
{
  if (mWinApp->mCameraMacroTools.GetUserStop() && !inVal)
    mExtErrorOccurred = 2;   // User stop
  else if (mExtErrorOccurred < 0)
    mExtErrorOccurred = 1;   // Other error
  SEMTrace('q', "Ext error set to %d from inval %d", mExtErrorOccurred, inVal);
}

/*
 * Calls SEMMessageBox for error in these routines, without triggering it to be treated as
 * an external error
 */
void CMultiGridTasks::MGActMessageBox(CString &errStr)
{
  mInternalError = true;
  SEMMessageBox(errStr);
  mInternalError = false;
}

/////////////////////////////////////////////////////////
//  REALIGN TO RELOADED GRID
/////////////////////////////////////////////////////////

/*
 * Start procedure to realign after loading grid
 */
int CMultiGridTasks::RealignReloadedGrid(CMapDrawItem *item, float expectedRot, 
  bool moveInZ, float maxRotation, int transformNav, CString &errStr, int jcdInd)
{
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  int numPieces, numFull, ixMin, iyMin, ixMax, iyMax, midX, midY, delX, delY;
  int minInd[4], bestInd[5], minCenInd[4], pci, dir, indAng, numAngles = 18, bestAngInd;
  int numClose, ind, corn,  err = 0, ixpc, iypc, xTarg, yTarg, binning;
  int meanIXcen, meanIYcen, pctlIXcen, pctlIYcen, numPctl, numAbove;
  float useWidth, useHeight, radDist[4], bestDist[4], maxAxis = 0., ang;
  float minDel, cenAngle, delAng, shortAxis, minAng, maxAng, rad, zStage;
  float meanSXcen, meanSYcen, pctlSXcen, pctlSYcen;
  double wxSum = 0., wySum = 0.;
  float stageXcen = 0., stageYcen = 0.;
  bool indIsDup, bestIsDup, usePctls = true;
  IntVec ixVec, iyVec, pieceSavedAt;
  FloatVec xStage, yStage, meanVec, midFracs, rangeFracs;
  MiniOffsets *mini, myMini;
  MontParam *montP = &mRRGmontParam;
  MontParam *masterMont = mWinApp->GetMontParam();
  float distFrac = 0.75f, meanRatioCrit = 1.5f, pctlRatioCrit = 2.0;
  mNavigator = mWinApp->mNavigator;

  mRRGangleRange = 2.f * maxRotation;
  mRRGtransformNav = B3DMAX(0, B3DMIN(2, transformNav));
  mRRGitem = item;
  mRRGWasAboveXformLimit = false;
  mRRGjcdIndex = jcdInd;

  // Require a montage with autodoc info
  if (!mNavigator) {
    errStr = "The Navigator is not open";
    return 1;
  }
  if (!item || !item->IsMap() || !item->mMapMontage) {
    errStr = "The item to realign to must be a montage map";
    return 1;
  }
  mBigRotation = fabs(expectedRot) > 1.;

  // If not doing big rotation, see if map is already available
  if (!mBigRotation) {
    if (LoadOrReloadMapIfNeeded(item, 2, errStr))
      return 1;
  }

  // Now access the map file regardless and make sure there is an adoc
  if (mNavigator->AccessMapFile(item, mRRGstore, mRRGcurStore, montP, useWidth, 
    useHeight)) {
    errStr = "Error trying to access map file " + item->mMapFile;
    return 1;
  }
  if (mRRGstore->GetAdocIndex() < 0) {
    errStr = "The map has no autodoc information (in .mdoc, .idoc, or .hdf file)";
    if (mRRGcurStore < 0)
      B3DDELETE(mRRGstore);
    return 1;
  }

  // Get piece coords and stage coords
  numPieces = mWinApp->mMontageController->ListMontagePieces(mRRGstore, montP,
    item->mMapSection, pieceSavedAt, &ixVec, &iyVec, &xStage, &yStage, &meanVec, 
    moveInZ ? &zStage : NULL, &midFracs, &rangeFracs);
  if (!numPieces || !xStage.size()) {
    errStr = "Error getting the montage piece coordinates or stage coordinates from "
      "autodoc";
    if (mRRGcurStore < 0)
      B3DDELETE(mRRGstore);
    return 1;
  }
  mScope->SetLDCenteredShift(0., 0.);
  if (!mNavigator->BacklashForItem(item, mRRGbacklashX, mRRGbacklashY) && !mBigRotation) {
    mRRGbacklashX = item->mBacklashX;
    mRRGbacklashY = item->mBacklashY;
  }
  mRRGmapZvalue = moveInZ ? zStage : EXTRA_NO_VALUE;

  // Find the middle coordinate
  numFull = montP->xNframes * montP->yNframes;
  ixMin = VECTOR_MIN(ixVec);
  ixMax = VECTOR_MAX(ixVec) + montP->xFrame;
  iyMin = VECTOR_MIN(iyVec);
  iyMax = VECTOR_MAX(iyVec) + montP->yFrame;
  midX = (ixMin + ixMax) / 2;
  midY = (iyMin + iyMax) / 2;
  mRRGexpectedRot = expectedRot;

  // See if percentiles can be used
  CentroidsFromMeansAndPctls(ixVec, iyVec, xStage, yStage, meanVec, midFracs,
    rangeFracs, mPctUsableFrac, meanIXcen, meanIYcen, meanSXcen, meanSYcen, pctlIXcen,
    pctlIYcen, pctlSXcen, pctlSYcen, numPctl, numAbove);
  usePctls = numPctl == numPieces;
  if (usePctls) {
    midX = pctlIXcen + montP->xFrame / 2;
    midY = pctlIXcen + montP->yFrame / 2;
    stageXcen = pctlSXcen;
    stageYcen = pctlSYcen;
  } else {
    midX = meanIXcen + montP->xFrame / 2;
    midY = meanIXcen + montP->yFrame / 2;
    stageXcen = meanSXcen;
    stageYcen = meanSYcen;
  }

  // Scan angles in 4 directions finding farthest piece intersected by ray
  for (indAng = 0; indAng < numAngles; indAng++) {
    for (dir = 0; dir < 4; dir++) {
      radDist[dir] = 0.;
      minDel = 1.e10f;
      cenAngle = (float)((indAng * 90.) / numAngles + dir * 90.);
      for (pci = 0; pci < numFull; pci++) {
        if (pieceSavedAt[pci] < 0 || (usePctls && midFracs[pci] < mPctUsableFrac))
          continue;

        // Find angles of the 4 corners and their difference from the ray, get the 
        // min and max differences
        minAng = 999.;
        maxAng = -999.;
        for (corn = 0; corn < 4; corn++) {
          delX = ixVec[pci] + montP->xFrame * (corn % 2) - midX;
          delY = iyVec[pci] + montP->yFrame * (corn / 2) - midY;
          ang = (float)(atan2(delY, delX) / DTOR);
          delAng = (float)UtilGoodAngle(ang - cenAngle);
          ACCUM_MIN(minAng, delAng);
          ACCUM_MAX(maxAng, delAng);
        }

        // If the piece intersects the ray, get distance of its middle and accumulate max
        if (fabs(minAng) < 90. && fabs(maxAng) < 90. && minAng < 0. && maxAng > 0.) {
          delX = ixVec[pci] + montP->xFrame / 2 - midX;
          delY = iyVec[pci] + montP->yFrame / 2 - midY;
          rad = (float)sqrt(delX * delX + delY * delY);
          if (rad > radDist[dir]) {
            minInd[dir] = pci;
            radDist[dir] = rad;
          }
        }
      }
    }

    // Keep track of the direction with the longest short axis
    shortAxis = B3DMIN(radDist[0] + radDist[2], radDist[1] + radDist[3]);
    if (shortAxis > maxAxis) {
      maxAxis = shortAxis;
      bestAngInd = indAng;
      for (dir = 0; dir < 4; dir++) {
        bestInd[dir] = minInd[dir];
        bestDist[dir] = radDist[dir];
      }
    }
  }

  // Find the pieces closest to stage center pos
  numClose = 0;
  for (pci = 0; pci < numFull; pci++) {
    if (pieceSavedAt[pci] >= 0)
      MaintainClosestFour(xStage[pci] - stageXcen, yStage[pci] - stageYcen, pci, radDist, 
        minCenInd, numClose);
  }
  bestInd[4] = minCenInd[0];

  // Loop on directions backwards so it is easy to check duplicates
  for (dir = 3; dir >= 0; dir--) {
    cenAngle = (float)((bestAngInd * 90.) / numAngles + dir * 90.);
    xTarg = (int)(distFrac * bestDist[dir] * cos(DTOR * cenAngle)) + midX;
    yTarg = (int)(distFrac * bestDist[dir] * sin(DTOR * cenAngle)) + midY;

    // Find the 4 closest pieces to the target
    numClose = 0;
    for (pci = 0; pci < numFull; pci++) {
      if (pieceSavedAt[pci] >= 0)
        MaintainClosestFour((float)(ixVec[pci] + montP->xFrame / 2 - xTarg),
        (float)(iyVec[pci] + montP->yFrame / 2 - yTarg), pci, radDist, minInd,
          numClose);
    }

    // Pick a different one from closest if it is brighter enough or if best one is
    // a duplicate; but don't pick this one if it is a duplicate
    bestInd[dir] = minInd[0];
    for (ind = 1; ind < numClose; ind++) {
      bestIsDup = IsDuplicatePiece(bestInd[dir], bestInd, dir + 1, 4);
      indIsDup = IsDuplicatePiece(minInd[ind], bestInd, dir + 1, 4);
      if (((!usePctls && meanVec[minInd[ind]] > meanVec[bestInd[dir]] * meanRatioCrit) ||
        (usePctls && midFracs[minInd[ind]] > midFracs[bestInd[dir]] * pctlRatioCrit) ||
        bestIsDup) && !indIsDup)
        bestInd[dir] = minInd[ind];
    }
  }

  // Now pick a different center one if much brighter and not a duplicate
  for (ind = 1; ind < numClose; ind++)
    if ((!usePctls && meanVec[minCenInd[ind]] > meanVec[bestInd[4]] * meanRatioCrit) ||
      (usePctls && midFracs[minCenInd[ind]] > midFracs[bestInd[4]] * pctlRatioCrit) && 
      !IsDuplicatePiece(minCenInd[ind], bestInd, 0, 3))
      bestInd[4] = minCenInd[ind];

  // The fitting equations should work with two unique pieces, make sure there are
  numClose = 0;
  for (dir = 0; dir < 5; dir++)
    if (!IsDuplicatePiece(bestInd[dir], bestInd, dir + 1, 4))
      numClose++;
  if (numClose < 2) {
    errStr = "Only one piece could be found for realigning to";
    return 1;
  }

  // Get image coordinate of center of this piece if aligning to map
  // Use inverted Y piece number as in OffsetMontImagePos
  if (!mBigRotation) {
    ixpc = bestInd[4] / montP->yNframes;
    iypc = (montP->yNframes - 1) - (bestInd[4] % montP->yNframes);
    mini = mImBufs[mMapBuf].mMiniOffsets;
    if (!mini) {
      binning = mImBufs[mMapBuf].mOverviewBin;
      mini = &myMini;
      mWinApp->mMontageController->SetMiniOffsetsParams(myMini, montP->xNframes,
        montP->xFrame / binning, (montP->xFrame - montP->xOverlap) / binning,
        montP->yNframes, montP->yFrame / binning, 
        (montP->yFrame - montP->yOverlap) / binning);
    }
    if (!ixpc)
      mMapCenX = mini->xFrame / 2;
    else if (ixpc == mini->xNframes - 1)
      mMapCenX = mImBufs[mMapBuf].mImage->getWidth() - mini->xFrame / 2;
    else
      mMapCenX = mini->xBase + ixpc * mini->xDelta + mini->xFrame / 2 -
      (mini->xFrame - mini->xDelta) / 2;
    if (!iypc)
      mMapCenY = mini->yFrame / 2;
    else if (iypc == mini->yNframes - 1)
      mMapCenY = mImBufs[mMapBuf].mImage->getHeight() - mini->yFrame / 2;
    else
      mMapCenY = mini->yBase + iypc * mini->yDelta + mini->yFrame / 2 -
      (mini->yFrame - mini->yDelta) / 2;
    if ((int)mini->offsetX.size() > bestInd[4] &&
      mini->offsetX[bestInd[4]] != MINI_NO_PIECE) {
      mMapCenX += mini->offsetX[bestInd[4]];
      mMapCenY += mini->offsetY[bestInd[4]];
    }
    SEMTrace('q', "Align to pc %d %d (y inv), center coord %d %d", ixpc, iypc, mMapCenX,
      mMapCenY);
  }

  // Save section numbers and stage positions of chosen pieces, center in first one
  for (dir = 0; dir < 5; dir++) {
    ind = (dir + 1) % 5;
    mRRGpieceSec[ind] = pieceSavedAt[bestInd[dir]];
    mRRGxStages[ind] = xStage[bestInd[dir]];
    mRRGyStages[ind] = yStage[bestInd[dir]];
  }

  // Compute actual max shift given center position and max rotation
  mRRGmaxInitShift = mRRGmaxCenShift + sinf(mRRGMaxRotation * (float)DTOR) * 
    sqrtf(mRRGxStages[0] * mRRGxStages[0] + mRRGyStages[0] * mRRGyStages[0]);

  if (mScope->GetColumnValvesOpen() < 1)
    mScope->SetColumnValvesOpen(true);

  mRRGdidSaveState = true; //!mWinApp->LowDoseMode();
  if (mRRGdidSaveState) {
    mNavHelper->SetTypeOfSavedState(STATE_NONE);
    mNavHelper->SaveCurrentState(STATE_MAP_ACQUIRE, 0, item->mMapCamera, 0);
  }
  mNavHelper->PrepareToReimageMap(item, montP, conSet, TRIAL_CONSET, (mRRGdidSaveState
    || !(mWinApp->LowDoseMode() && item->mMapLowDoseConSet < 0)) ? 1 : 0, 1);
  if (!mNavHelper->GetRIstayingInLD())
    mNavHelper->SetMapOffsetsIfAny(item);
  mCamera->SetRequiredRoll(1);
  if (!mWinApp->LowDoseMode() && mDoingMulGridSeq)
    mScope->SetFocusToStandardIfLM(item->mMapMagInd);

  // For non-big rotation, Set up to to align to map extract, then regular align to 
  // center piece with index 0
  mRRGshiftX = mRRGshiftY = 0.;
  mRRGcurDirection = mBigRotation ? 0 : -1;
  mDoingMultiGrid = MG_REALIGN_RELOADED;
  mLoadedGridIsAligned = 0;
  if (MoveStageToTargetPiece()) {
    StopMultiGrid();
    return 1;
  }
  mWinApp->SetStatusText(MEDIUM_PANE, "REALIGNING RELOADED GRID");
  mWinApp->UpdateBufferWindows();

  return 0;
}

/*
 * Reload map if needed to have a maximum binning, make sure it is in non-rolling buffer
 */
int CMultiGridTasks::LoadOrReloadMapIfNeeded(CMapDrawItem *item, int maxBin, 
  CString &errStr)
{
  int readBuf = mWinApp->mBufferManager->GetBufToReadInto();
  int rolls = mWinApp->mBufferManager->GetShiftsOnAcquire();
  mMapBuf = mNavigator->FindBufferWithMapID(item->mMapID);

  // If not or if it is binned a lot, reload it with no binning & alignment in overview
  if (mMapBuf < 0 || mImBufs[mMapBuf].mOverviewBin > maxBin) {
    if (ReloadGridMap(item, 1, errStr))
      return 1;
    mMapBuf = readBuf;
  }

  // If buffer is in rolling range, copy it to read buffer or safe buffer
  if (mMapBuf <= rolls) {
    if (readBuf <= rolls)
      readBuf = rolls + (mWinApp->LowDoseMode() ? 3 : 2);
    mWinApp->mBufferManager->CopyImageBuffer(mMapBuf, readBuf);
    mMapBuf = readBuf;
  }
  return 0;
}

/*
 * Use the stage and piece coordinates and mean and percentile fractions to derive
 * weighted mean stage and piece coordinates, beas on either the means or the percentile
 * values (mean of the range and midpoint values) that are above fracThresh
 */
void CMultiGridTasks::CentroidsFromMeansAndPctls(IntVec &ixVec, IntVec &iyVec,
  FloatVec &xStage, FloatVec &yStage, FloatVec &meanVec, FloatVec &midFracs,
  FloatVec &rangeFracs, float fracThresh, int &meanIXcen, int &meanIYcen,
  float &meanSXcen, float &meanSYcen, int &pctlIXcen, int &pctlIYcen, float &pctlSXcen,
  float &pctlSYcen, int &numPcts, int &numAbove)
{
  float meanSum = 0., fracSum = 0., ixMSum = 0., ixPSum = 0., iyMSum = 0., iyPSum = 0.;
  float sxMSum = 0., sxPSum = 0., syMSum = 0., syPSum = 0., mean, frac;
  int ind, numPiece = (int)ixVec.size();
  numPcts = 0;
  numAbove = 0;

  // FLAW: should subtract a min, what about STEM?
  for (ind = 0; ind < numPiece; ind++) {
    mean = meanVec[ind];
    meanSum += mean;
    ixMSum += mean * (float)ixVec[ind];
    iyMSum += mean * (float)iyVec[ind];
    sxMSum += mean * xStage[ind];
    syMSum += mean * yStage[ind];

    // Until something else happens, just collapse these into one value here
    midFracs[ind] = (midFracs[ind] + rangeFracs[ind]) / 2.f;
    frac = midFracs[ind];
    if (frac >= 0.) {
      numPcts++;
      if (frac >= fracThresh) {
        fracSum += frac;
        numAbove++;
        ixPSum += frac * (float)ixVec[ind];
        iyPSum += frac * (float)iyVec[ind];
        sxPSum += frac * xStage[ind];
        syPSum += frac * yStage[ind];
      }
    }
  }
  if (!meanSum)
    meanSum = 0.001f;
  meanIXcen = B3DNINT(ixMSum / meanSum);
  meanIYcen = B3DNINT(iyMSum / meanSum);
  meanSXcen = sxMSum / meanSum;
  meanSYcen = syMSum / meanSum;
  if (numAbove > 0) {
    pctlIXcen = B3DNINT(ixPSum / fracSum);
    pctlIYcen = B3DNINT(iyPSum / fracSum);
    pctlSXcen = sxPSum / fracSum;
    pctlSYcen = syPSum / fracSum;
  }
}

/*
 * Keep a list of the 4 closest pieces to the desired position, delX, delY away
 */
void CMultiGridTasks::MaintainClosestFour(float delX, float delY, int pci, 
  float radDist[4], int minInd[4], int &numClose)
{
  int ind, jnd;
  bool inserted = false;
  float rad = sqrtf(delX * delX + delY * delY);
  for (ind = 0; ind < numClose; ind++) {
    if (rad < radDist[ind]) {
      numClose = B3DMIN(numClose + 1, 4);
      for (jnd = ind + 1; jnd < numClose; jnd++) {
        radDist[jnd] = radDist[jnd - 1];
        minInd[jnd] = minInd[jnd - 1];
      }
      radDist[ind] = rad;
      minInd[ind] = pci;
      inserted = true;
      break;
    }
  }
  if (!inserted && numClose < 4) {
    minInd[numClose] = pci;
    radDist[numClose++] = rad;
  }
}

/* 
 * Checks whether a a piece number in  minInd matches one on the list in bestInd within
 * the specified range of indexes
 */
bool CMultiGridTasks::IsDuplicatePiece(int minInd, int bestInd[5], int startInd, int endInd)
{
  for (int ind = startInd; ind <= endInd; ind++)
    if (minInd == bestInd[ind])
      return true;
  return false;
}

/*
 * Compute a transformed coordinate for the target piece based on current rotation angle
 * and start the stage move there
 */
int CMultiGridTasks::MoveStageToTargetPiece()
{
  float xSign = mShiftManager->GetInvertStageXAxis() ? -1.f : 1.f;
  int curInd = B3DMAX(0, mRRGcurDirection);
  BOOL doBacklash = mRRGbacklashX && mRRGbacklashY && mRRGbacklashX > -100. &&
    mRRGbacklashY > -100.;
  mRRGrotMat = mNavHelper->GetRotationMatrix(mRRGexpectedRot, false);
  mShiftManager->ApplyScaleMatrix(mRRGrotMat, xSign * mRRGxStages[curInd],
    mRRGyStages[curInd], mMoveInfo.x, mMoveInfo.y);
  mMoveInfo.x *= xSign;
  SEMTrace('q', "dir %d pc pos %.2f %.2f  transformed %.2f %.2f -> %.2f %.2f", curInd,
    mRRGxStages[curInd], mRRGyStages[curInd], mMoveInfo.x, mMoveInfo.y, 
    mMoveInfo.x + mRRGshiftX, mMoveInfo.y + mRRGshiftY);
  mMoveInfo.x += mRRGshiftX;
  mMoveInfo.y += mRRGshiftY;
  mMoveInfo.axisBits = axisXY;
  mMoveInfo.backX = mRRGbacklashX;
  mMoveInfo.backY = mRRGbacklashY;
  if (mRRGcurDirection <= 0 && mRRGmapZvalue > EXTRA_VALUE_TEST && 
    (mMoveInfo.axisBits & axisZ) == 0) {
    mMoveInfo.z = mRRGmapZvalue;
    mMoveInfo.axisBits |= axisZ;
    mMoveInfo.backZ = mWinApp->mComplexTasks->GetFEBacklashZ();
  }
  if (!mScope->MoveStage(mMoveInfo, doBacklash))
    return 1;
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_RRG_MOVED_STAGE, 120000);
  mWinApp->SetStatusText(SIMPLE_PANE, "MOVING STAGE");
  return 0;
}

/*
 * Do the alignment and other operations needed when an image is gotten, and finish up
 */
void CMultiGridTasks::AlignNewReloadImage()
{
  int err, ind, oldReg, newReg;
  float xShift, yShift, xSign, rotBest, scaleMax, xTiltFac, yTiltFac, dxy[2], incDxy[2];
  float newXform[6], oldInvXf[6], incXform[6], theta, resid, smag, stretch, phi;
  double xStage, yStage, zStage, transX, transY;
  bool simpleAlign;
  int curInd = B3DMAX(0, mRRGcurDirection);
  CString mess, str;
  ScaleMat bMat, bInv, incMat, rotMat;
  JeolCartridgeData jcd;
  MGridMultiShotParams mgParam;

  mess = "Error in autoalign routine aligning to map piece";
  simpleAlign = (mBigRotation && mRRGcurDirection > 0) ||
    (!mBigRotation && mRRGcurDirection >= 0);

  // Do the appropriate kind of alignment: regular align after the first round sets the 
  // rotation, or align with rotation around the expected rotation for big rotation,
  // or align to extract of map for small rotation.
  if (simpleAlign) {
    mShiftManager->SetNextAutoalignLimit(50.);
    err = mShiftManager->AutoAlign(1, 0, 0, 0, NULL, 0., 0., 0., 0., 0., NULL, NULL,
      false);
  } else if (mBigRotation) {
    err = mNavHelper->AlignWithRotation(1, mRRGexpectedRot, mRRGangleRange, rotBest, 
      xShift, yShift, 0., 0, NULL, -1., 0);
    mess += " with rotation";
    if (err == 1)
      mess.Format("Rotation found when aligning to map piece was at the end of the "
        "search range.\r\nThe angular range for the search needs to be higher than %.1f"
        , mRRGangleRange);
    if (!err)
      err = mShiftManager->AutoAlign(1, 0, false, 0, NULL, 0., 0., 0., 0., rotBest,
        NULL, NULL, false);
  } else {
    err = mWinApp->mProcessImage->AlignBetweenMagnifications(mMapBuf, (float)mMapCenX,
      (float)mMapCenY, mRRGmaxInitShift, 0., mRRGangleRange, false, scaleMax, rotBest, 
      AUTOALIGN_SEARCH_KEEP, mess);
  }
  mImBufs->mImage->getShifts(xShift, yShift);
  // TEMP
  if (mScope->GetSimulationMode() && err < 0) {
    xShift = 0.;
    yShift = 0.;
    err = 0;
  }
  if (err) {
    SEMMessageBox(mess);
    StopMultiGrid();
    return;
  }

  // Compute the stage position at center of shifted image.  The shift is amount that
  // image is shifted in inverted coordinates, so invert for RH camera coordinates
  // The rest is the same as in SetAlignShifts for getting stage move
  yShift = -yShift;
  bMat = mShiftManager->FocusAdjustedStageToCamera(mImBufs);
  bInv = MatInv(bMat);
  mShiftManager->GetStageTiltFactors(xTiltFac, yTiltFac);
  mScope->GetStagePosition(xStage, yStage, zStage);
  xStage -= mImBufs->mBinning * (bInv.xpx * xShift + bInv.xpy * yShift) / xTiltFac;
  yStage -= mImBufs->mBinning * (bInv.ypx * xShift + bInv.ypy * yShift) / yTiltFac;
  mRRGnewXstage[curInd] = (float)xStage;
  mRRGnewYstage[curInd] = (float)yStage;
  SEMTrace('q', "dir %d  shift %.1f %.1f   new stage %.2f %.2f stage diff %.2f %.2f",
    mRRGcurDirection, xShift, yShift, xStage, yStage, xStage - mMoveInfo.x,
    yStage - mMoveInfo.y);
  mWinApp->mMainView->DrawImage();
  mWinApp->ToggleBuffers(1, 300, 8, 16);

  // For first position, set the post-rotation shift and adjust the expected rotation
  if (!simpleAlign) {
    xSign = mShiftManager->GetInvertStageXAxis() ? -1.f : 1.f;
    mRRGexpectedRot -= rotBest;

    // Recompute the transformed position with the new rotation and set the shift that
    // will regenerate the found location
    mRRGrotMat = mNavHelper->GetRotationMatrix(mRRGexpectedRot, false);
    mShiftManager->ApplyScaleMatrix(mRRGrotMat, xSign * mRRGxStages[0],
      mRRGyStages[0], transX, transY);
    transX *= xSign;
    SEMTrace('q', "pc pos %.2f %.2f  transformed %.2f %.2f", mRRGxStages[0],
      mRRGyStages[0], transX, transY);
    mRRGshiftX = (float)(xStage - transX);
    mRRGshiftY = (float)(yStage - transY);
    SEMTrace('q', "shift set to %.2f %.2f", mRRGshiftX, mRRGshiftY);

  } else if (mRRGcurDirection <= 0) {

    // On second shot of center with small rotations, revise the shift
    mRRGshiftX += (float)(xStage - mMoveInfo.x);
    mRRGshiftY += (float)(yStage - mMoveInfo.y);
    SEMTrace('q', "shift revised to %.2f %.2f", mRRGshiftX, mRRGshiftY);
  }

  // Set up to go to next position unless done
  if (mRRGcurDirection < 4) {
    mRRGcurDirection++;
    if (MoveStageToTargetPiece())
      StopMultiGrid();
    return;
  }
  StopMultiGrid();

  // Get transformation
  bMat = FindReloadTransform(dxy, theta, resid);
  if (mRRGtransformNav > 1) {
    mess = "";
    xShift = sqrtf(dxy[0] * dxy[0] + dxy[1] * dxy[1]);
    if (mRRGShiftLimitForXform > 0. && xShift > mRRGShiftLimitForXform)
      mess.Format("Realignment shift of %.1f um is above limit of %1.f", xShift,
        mRRGShiftLimitForXform);
    if (mRRGRotLimitForXform > 0. && fabs(theta) > mRRGRotLimitForXform) {
      str.Format("Realignment rotation of %.1f deg is above limit of %.1f", theta,
        mRRGRotLimitForXform);
      if (!mess.IsEmpty())
        mess += "\r\n";
      mess += str;
    }
    if (mRRGResidLimitForXform > 0. && resid > mRRGResidLimitForXform) {
      str.Format("Mean residual from fit is %.1f um, more than the limit of "
        "%.1f um", resid, mRRGResidLimitForXform);
      if (!mess.IsEmpty())
        mess += "\r\n";
      mess += str;
    }
    if (!mess.IsEmpty()) {
      mess += "\r\nPoints were not transformed";
      mRRGWasAboveXformLimit = true;
      SEMAppendToLog(mess);
    }
  }

  // Transform points
  if (mRRGtransformNav && mWinApp->mNavigator && !mRRGWasAboveXformLimit) {

    // Find new registration #
    oldReg = mNavigator->GetCurrentRegistration();
    newReg = oldReg;
    while (newReg <= MAX_CURRENT_REG) {
      newReg++;
      if (mNavigator->RegistrationUseType(newReg) == NAVREG_UNUSED)
        break;
    }
    if (newReg > MAX_CURRENT_REG) {
      newReg = -1;
      while (newReg < oldReg) {
        newReg++;
        if (mNavigator->RegistrationUseType(newReg) == NAVREG_UNUSED)
          break;
      }
      if (newReg >= oldReg) {
        SEMMessageBox("There are no new unused registration numbers to transform "
          "items to");
      }
    }
    mNavigator->SetCurrentRegistration(newReg);

    // Get the incremental transform to apply if there is a previous one done
    incMat = bMat;
    incDxy[0] = dxy[0];
    incDxy[1] = dxy[1];
    mShiftManager->ScaleMatToIMODxform(bMat, dxy[0], dxy[1], newXform);
    if (mRRGitem->mGridMapXform) {
      xfInvert(mRRGitem->mGridMapXform, oldInvXf, 2);
      xfMult(oldInvXf, newXform, incXform, 2);
      incMat = mShiftManager->IMODxformToScaleMat(incXform, incDxy[0], incDxy[1]);
      SEMTrace('q', "Incremental xform %.4f %.4f %.4f %.4f  %.1f %.1f", incMat.xpx,
        incMat.xpy, incMat.ypx, incMat.ypy, incDxy[0], incDxy[1]);
    }

    // Transform items incrementally, and store the new full xform
    // Pull off the marker shift beforehand and reapply it
    UndoOrRedoMarkerShift(false);
    ind = mNavigator->TransformToCurrentReg(oldReg, incMat, incDxy, 0, 0);
    UndoOrRedoMarkerShift(true);
    PrintfToLog("%d items transformed to new registration", ind);

    // Given a grid index from the dialog, see if the grid has multishot settings with
    // hole vectors and rotate those vector ins specimen space by the angle extracted
    // from the incremental transform
    if (mRRGjcdIndex >= 0) {
      jcd = mCartInfo->GetAt(mRRGjcdIndex);
      if (jcd.multiShotParamIndex >= 0 &&
        jcd.multiShotParamIndex < (int)mMGMShotParamArray.GetSize()) {
        mgParam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
        if (mgParam.values[MS_holeMagIndex] > 0) {
          bMat = mShiftManager->IStoSpecimen((int)mgParam.values[MS_holeMagIndex]);
          amatToRotmagstr(incMat.xpx, incMat.xpy, incMat.ypx, incMat.ypy, &theta, &smag,
            &stretch, &phi);
          rotMat = mNavHelper->GetRotationMatrix(theta, false);
          bInv = MatMul(bMat, rotMat);
          incMat = MatMul(bInv, MatInv(bMat));
          TransformStoredVectors(mgParam, incMat);
          mMGMShotParamArray.SetAt(jcd.multiShotParamIndex, mgParam);
          PrintfToLog("IS vectors in this grid\'s Multiple Record parameters rotated by "
            "%.2f deg", theta);
        }
      }
    }

    if (!mRRGitem->mGridMapXform)
      mRRGitem->mGridMapXform = new float[6];
    for (ind = 0; ind < 6; ind++)
      mRRGitem->mGridMapXform[ind] = newXform[ind];
    mLoadedGridIsAligned = 1;
  }
}

/*
 * Compute transform from results
 */
ScaleMat CMultiGridTasks::FindReloadTransform(float dxyOut[2], float &thetaOut,
  float &residOut)
{
  ScaleMat aM[6], strMat, usMat, usInv;
  const int MSIZE = 5, XSIZE = 5;
  float x[5][XSIZE], sx[MSIZE], xm[MSIZE], sd[MSIZE];
  float ss[MSIZE][MSIZE], ssd[MSIZE][MSIZE], d[MSIZE][MSIZE], r[MSIZE][MSIZE];
  float dxy[6][2], smagMean, str, sinth, costh, devAvg, devSD, tmp;
  double thetad, alpha, theta[6], lvoMax, dmaxMin, dsumMin, sdMin;
  double devMax[6], devSum[6], devSumSq[6], devX, devY, devPnt, xx, yy, leaveOut[5];
  int ind, skip, nPairs, i, nCol = 3, ptMax[6];
  int use, ptLvoMax = -1, ptMaxMin = -1, ptSumMin = -1, ptSDmin = -1;
  float threshForStretch = 10.;
  float leaveCrit = 3.f;
  float minLeaveOutForElim = 2.f;

  strMat.xpx = 0.;
  if (fabs(mRRGexpectedRot) > threshForStretch) {

    strMat = mShiftManager->GetStageStretchXform();
    if (strMat.xpx) {
      SEMAppendToLog("getting stretch matrix");
      usMat = mShiftManager->UnderlyingStretch(strMat, smagMean, thetad, str, alpha);
      usInv = MatInv(usMat);
    }
  }

  for (skip = 0; skip < 6; skip++) {

    // Load the data matrix
    nPairs = 0;
    for (ind = 0; ind < 5; ind++) {
      if (ind == skip)
        continue;
      i = nPairs;
      x[nPairs][0] = mRRGxStages[ind];
      x[nPairs][1] = mRRGyStages[ind];
      x[nPairs][2] = 0.;
      x[nPairs][3] = mRRGnewXstage[ind];
      x[nPairs++][4] = mRRGnewYstage[ind];
      if (strMat.xpx) {
        mShiftManager->ApplyScaleMatrix(usInv, x[i][0], x[i][1], x[i][0], x[i][1]);
        mShiftManager->ApplyScaleMatrix(usInv, x[i][3], x[i][4], x[i][3], x[i][4]);
      }
      if (skip == 5)
        SEMTrace('q', "%d %.3f %.3f %.3f %.3f", ind, x[i][0], x[i][1], x[i][3], x[i][4]);
    }

    StatCorrel(&x[0][0], XSIZE, 5, MSIZE, &nPairs, sx, &ss[0][0], &ssd[0][0], &d[0][0],
      &r[0][0], xm, sd, &nCol);

    theta[skip] = atan2(-(ssd[1][3] - ssd[0][4]), (ssd[0][3] + ssd[1][4]));
    sinth = (float)sin(theta[skip]);
    costh = (float)cos(theta[skip]);
    aM[skip].xpx = costh;
    aM[skip].xpy = -sinth;
    aM[skip].ypx = sinth;
    aM[skip].ypy = costh;
    dxy[skip][0] = xm[3] - xm[0] * costh + xm[1] * sinth;
    dxy[skip][1] = xm[4] - xm[0] * sinth - xm[1] * costh;

    // Get summed and maximum deviation
    devMax[skip] = -1.;
    devSum[skip] = 0.;
    devSumSq[skip] = 0.;
    for (i = 0; i < nPairs; i++) {
      mShiftManager->ApplyScaleMatrix(aM[skip], x[i][0], x[i][1], xx, yy);
      devX = xx + dxy[skip][0] - x[i][3];
      devY = yy + dxy[skip][1] - x[i][4];
      devPnt = sqrt(devX * devX + devY * devY);
      devSum[skip] += devPnt;
      devSumSq[skip] += devPnt * devPnt;
      if (devMax[skip] < devPnt) {
        devMax[skip] = devPnt;
        ptMax[skip] = i < skip ? i : i + 1;
      }
    }

    // Get the leave-out error for a skipped point
    if (skip < 5) {
      mShiftManager->ApplyScaleMatrix(aM[skip], mRRGxStages[skip], mRRGyStages[skip],
        xx, yy);
      devX = xx + dxy[skip][0] - mRRGnewXstage[skip];
      devY = yy + dxy[skip][1] - mRRGnewYstage[skip];
      leaveOut[skip] = sqrt(devX * devX + devY * devY);
    }

    SEMTrace('q', "skip %d theta %.2f  dev mean %.2f max %.2f leave-out %.2f", skip,
      theta[skip] / DTOR, devSum[skip] / nPairs, devMax[skip], 
      skip < 5 ? leaveOut[skip] : 0.);
  }

  // Find point with lowest max, lowest mean/sum, and highest leave-out
  dmaxMin = 1.e10;
  dsumMin = 1.e10;
  lvoMax = -1.;
  sdMin = 1.e10;
  for (skip = 0; skip < 5; skip++) {
    if (ptMaxMin < 0 || dmaxMin > devMax[skip]) {
      dmaxMin = devMax[skip];
      ptMaxMin = skip;
    }
    if (ptSumMin < 0 || dsumMin > devSum[skip]) {
      dsumMin = devSum[skip];
      ptSumMin = skip;
    }
    if (ptLvoMax < 0 || lvoMax < leaveOut[skip]) {
      lvoMax = leaveOut[skip];
      ptLvoMax = skip;
    }
    sumsToAvgSDdbl(devSum[skip], devSumSq[skip], 4, 1, &devAvg, &devSD);
    if (ptSDmin < 0 || sdMin > devSD) {
      sdMin = devSD;
      ptSDmin = skip;
    }
  }
  SEMTrace('q', "max min %d  mean min %d  SD min %d leave max %d", ptSumMin, ptMaxMin,
    ptSDmin, ptLvoMax);

  // They should all match; if so make sure that one has the highest error in
  // the other solution and that its leave-out error is statistically
  // above the deviations of the rest
  use = 5;
  if (ptMaxMin == ptSumMin && ptMaxMin == ptLvoMax && ptMaxMin == ptSDmin &&
    lvoMax > minLeaveOutForElim) {
    ind = 0;
    for (skip = 0; skip < 5; skip++)
      if (skip != ptLvoMax && ptMax[skip] == ptLvoMax)
        ind++;
    sumsToAvgSDdbl(devSum[ptLvoMax], devSumSq[ptLvoMax], 4, 1, &devAvg, &devSD);
    if (ind == 4 && fabs(lvoMax - devAvg) > leaveCrit * devSD)
      use = ptLvoMax;
    SEMTrace('q', "dev mean %.2f SD %.2f  # SD %.2f  use %d", devAvg, devSD,
      fabs(lvoMax - devAvg) / devSD, use);
  }

  // Adjust for the stretch by multiplying us inverse * matrix * us
  if (strMat.xpx) {
    tmp = dxy[use][0] * usMat.xpx + dxy[use][1] * usMat.xpy;
    dxy[use][1] = dxy[use][0] * usMat.ypx + dxy[use][1] * usMat.ypy;
    dxy[use][0] = tmp;
    aM[use] = MatMul(MatMul(usInv, aM[use]), usMat);
  }
  dxyOut[0] = dxy[use][0];
  dxyOut[1] = dxy[use][1];
  thetaOut = (float)(theta[use] / DTOR);
  residOut = (float)(devSum[use] / (use < 5 ? 4 : 5));

  PrintfToLog("Grid rotation %.2f  shift %.1f %.1f  mean deviation %.2f um", 
    thetaOut, dxyOut[0], dxyOut[1], residOut);

  return aM[use];
}

/*
 * Take off or put back the marker shift for every item so transformation is based on
 * coordinates that apply to the original grid map
 */
void CMultiGridTasks::UndoOrRedoMarkerShift(bool redo)
{
  MapItemArray *itemArr = mNavigator->GetItemArray();
  int ind;
  float shiftX, shiftY;
  CMapDrawItem *item;
  for (ind = 0; ind < (int)itemArr->GetSize(); ind++) {
    item = itemArr->GetAt(ind);
    if (item->mShiftCohortID && item->mMarkerShiftX > EXTRA_VALUE_TEST) {
      shiftX = (redo ? 1.f : -1.f) * item->mMarkerShiftX;
      shiftY = (redo ? 1.f : -1.f) * item->mMarkerShiftY;
      item->mStageX += shiftX;
      item->mStageY += shiftY;
      mNavigator->ShiftItemPoints(item, shiftX, shiftY);
    }
  }
}

////////////////////////////////////////////////////////
// REFINING GRID ALIGNMENT WITH MEDIUM_MAG IMAGES
////////////////////////////////////////////////////////

/*
 * Main routine for aligning to grid map from higher mag: takes the control set to use,
  * but imaging conditions must already be set up.  
 */
int CMultiGridTasks::AlignGridMapAcrossMags(CMapDrawItem *mapItem, int setNum,
  float findNearX, float findNearY, float maxDiff, CString &errStr)
{
  FloatVec polySizes, tempSizes, fitX, fitY, fitDist, angles, pcCenDists;
  IntVec candidates;
  MapItemArray *itemArray;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int ind, ldArea, magInd, numPoly, bestInd = -1, camSize, cen, jnd, dir, lastJnd;
  int numFit, cornInd[3], numSteps, nxIm, nyIm, type, boxLen, boxWidth, knd, indMinCen;
  float pctCrit = 0.9f, perimFrac = 0.1f;
  float cenOffset = 0.1f, edgeTestLen = 5., edgeCritFrac = 0.33f;
  float xSum = 0., ySum = 0., size, dist, FOV;
  float perim, minLength, intcp, slope, ro, vecX[2], vecY[2], frac;
  float intcp2, slope2, ro2, delX, delY, cenXdel, cenYdel;
  float nearXstage, nearYstage, farXstage, farYstage, nearXim, nearYim, farXim, farYim;
  float xCorn, yCorn;
  float maxCenDist = -1.e30f, minCenDist = 1.e30f, bestTargDist;
  ScaleMat aMat;
  ControlSet *conSets = mWinApp->GetConSets();
  CMapDrawItem *item;
  const char *badInfo = "Image buffer does not have enough information to convert "
    "stage coordinates";
  mNavigator = mWinApp->mNavigator;
  mMaxRefineDiff = maxDiff;

  // Initial sanity checks
  if (!mNavigator) {
    errStr = "Navigator must be open to refine grid map alignment";
    return 1; 
  }
  if (!mapItem->mMapID || !mapItem->IsMap()) {
    errStr = "The item passed to AlignGridMapAcrossMags must be a map";
    return 1;
  }

  // Get mag, field size and pixel size, make sure FOV is big enough
  if (mWinApp->LowDoseMode()) {
    ldArea = mCamera->ConSetToLDArea(setNum);
    magInd = ldp[ldArea].magIndex;
  } else {
    magInd = mScope->GetMagIndex();
  }
  camSize = B3DMIN(conSets[setNum].right - conSets[setNum].left,
    conSets[setNum].bottom - conSets[setNum].top);
  FOV = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), magInd) * (float)camSize;
  if (FOV < mRefineMinField) {
    errStr.Format("The minimum dimension of the camera field is only %.1f um, less than "
      "the %.1f required to refine grid map alignment", FOV, mRefineMinField);
    return 1;
  }

  // Get the map unbinned into a stable buffer
  if (LoadOrReloadMapIfNeeded(mapItem, 1, errStr))
    return 1;

  // Prepare to get image coordinates
  if (!mNavigator->BufferStageToImage(&mImBufs[mMapBuf], aMat, delX, delY)) {
    errStr = badInfo;
    return 1;
  }

  // Get candidates
  itemArray = mNavigator->GetItemArray();
  for (ind = 0; ind < (int)itemArray->GetSize(); ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsPolygon() && item->mDrawnOnMapID == mapItem->mMapID) {
      size = sqrtf(mNavigator->ContourArea(item->mPtX, item->mPtY, item->mNumPoints));
      if (item->mGroupID > 0 || size < mMaxSizeForRefinePolys) {
        polySizes.push_back(size);
        candidates.push_back(ind);
        xSum += item->mStageX;
        ySum += item->mStageY;
        mWinApp->mMainView->ExternalStageToImage(&mImBufs[mMapBuf], aMat, delX,
          delY, item->mStageX, item->mStageY, slope, slope2, &dist);
        ACCUM_MAX(maxCenDist, dist);
        if (dist < minCenDist) {
          minCenDist = dist;
          indMinCen = (int)pcCenDists.size();
        }
        pcCenDists.push_back(dist);
      }
    }
  }

  // Make sure there are some
  numPoly = (int)polySizes.size();
  if (!numPoly) {
    errStr.Format("To refine grid map alignment, there must be polygons either from "
      "autocontouring or smaller than %.0f microns in size", mMaxSizeForRefinePolys);
    return 1;
  }

  // If center for selection not specified, use center of candidate polygons
  if (findNearX < EXTRA_VALUE_TEST || findNearY < EXTRA_VALUE_TEST) {
    findNearX = xSum / numPoly;
    findNearY = ySum / numPoly;
  }
  item = itemArray->GetAt(candidates[indMinCen]);
  bestTargDist = sqrtf(powf(item->mStageX - findNearX, 2.f) + 
    powf(item->mStageY - findNearY, 2.f));
  bestInd = indMinCen;
  SEMTrace('q', "Initial polygon %s at %.1f, %.1f  %.1f from piece cen  %.1f from target",
    (LPCTSTR)item->mLabel, item->mStageX, item->mStageY, pcCenDists[bestInd],
    bestTargDist);

  // Look for nearly as good polygons closer to specified center
  for (ind = 0; ind < numPoly; ind++) {
    if (pcCenDists[ind] - minCenDist < 0.05 * maxCenDist || 
      pcCenDists[ind] < 0.1 * maxCenDist) {
      item = itemArray->GetAt(candidates[ind]);
      dist = sqrtf(powf(item->mStageX - findNearX, 2.f) + powf(item->mStageY - findNearY,
        2.f));
      if (dist < bestTargDist && (bestTargDist - dist > 0.1 * maxCenDist || 
        minCenDist > pcCenDists[ind] / 2.f)) {
        bestTargDist = dist;
        bestInd = ind;
      }
    }
  }

  // Get perimeter
  item = itemArray->GetAt(candidates[bestInd]);
  SEMTrace('q', "Chose polygon %s at %.1f, %.1f  %.1f from piece cen  %.1f from target", 
    (LPCTSTR)item->mLabel, item->mStageX, item->mStageY, pcCenDists[bestInd],
    bestTargDist);
  perim = 0.;
  for (ind = 1; ind < item->mNumPoints; ind++)
    perim += sqrtf(powf(item->mPtX[ind] - item->mPtX[ind - 1], 2.f) + 
      powf(item->mPtY[ind] - item->mPtY[ind - 1], 2.f));
  minLength = perimFrac * perim;

  // Walk around, fit lines in two directions from each point out to 1/10 of perimeter
  candidates.clear();
  for (cen = 0; cen < item->mNumPoints - 1; cen++) {
    for (dir = -1; dir <= 1; dir += 2) {
      dist = 0.;
      lastJnd = cen;
      fitX.clear();
      fitY.clear();
      fitDist.clear();
      fitX.push_back(item->mPtX[cen]);
      fitY.push_back(item->mPtY[cen]);
      fitDist.push_back(0.);
      for (ind = (!cen && dir < 0) ? 2 : 1; ind < item->mNumPoints; ind++) {
        jnd = (ind * dir + cen + item->mNumPoints) % item->mNumPoints;
        dist += sqrtf(powf(item->mPtX[jnd] - item->mPtX[lastJnd], 2.f) +
          powf(item->mPtY[jnd] - item->mPtY[lastJnd], 2.f));
        fitX.push_back(item->mPtX[jnd]);
        fitY.push_back(item->mPtY[jnd]);
        fitDist.push_back(dist);
        lastJnd = jnd;
        if (dist > minLength)
          break;
      }

      // Get X and Y as function of distance, and vector
      numFit = (int)fitX.size();
      lsFit(&fitDist[0], &fitX[0], numFit, &slope, &intcp, &ro);
      vecX[(dir + 1) / 2] = slope * fitDist[numFit - 1];
      lsFit(&fitDist[0], &fitY[0], numFit, &slope2, &intcp2, &ro2);
      vecY[(dir + 1) / 2] = slope2 * fitDist[numFit - 1];
      //PrintfToLog("n %d X int %.3f slo %.3f ro %.3f Y int %.3f slo %.3f ro %.3f  vec %.1f %.1f",
        //numFit, intcp, slope, ro, intcp2, slope2, ro2, vecX[(dir + 1) / 2], vecY[(dir + 1) / 2]);

    }

    // Get angle
    angles.push_back((float)(acos((vecX[0] * vecX[1] + vecY[0] * vecY[1]) /
      sqrtf((vecX[0] * vecX[0] + vecY[0] * vecY[0]) *
      (vecX[1] * vecX[1] + vecY[1] * vecY[1]))) / DTOR));
    candidates.push_back(cen);
    //SEMTrace('q', "pt %d  at %.1f %.1f  angle %.1f", cen, item->mPtX[cen],
      //item->mPtY[cen], angles[cen]);
  }

  if (!mNavigator->BufferStageToImage(&mImBufs[mMapBuf], aMat, delX, delY)) {
    errStr = badInfo;
    return 1;
  }
  type = mImBufs[mMapBuf].mImage->getType();
  mImBufs[mMapBuf].mImage->getSize(nxIm, nyIm);

  // Sort them to find smallest angles and find points that are well-separated
  rsSortIndexedFloats(&angles[0], &candidates[0], item->mNumPoints - 1);

  jnd = 0;
  while (jnd < 3 && minLength > 1.) {
    for (cen = 0; cen < item->mNumPoints - 1; cen++) {
      if ((jnd && candidates[cen] == cornInd[0]) ||
        (jnd > 1 && candidates[cen] == cornInd[1]))
        continue;
      dir = 0;
      for (ind = 0; ind < jnd; ind++) {
        if (sqrtf(powf(item->mPtX[candidates[cen]] - item->mPtX[cornInd[ind]], 2.f) +
          powf(item->mPtY[candidates[cen]] - item->mPtY[cornInd[ind]], 2.f)) < minLength)
        {
          dir = 1;
          break;
        }
      }
      if (!dir) {

        // The next candidate is far enough from the other accepted points.  Try to look
        // in image for the actual edge/corner
        ind = candidates[cen];

        // Get stage positions closer and farther from center along the sample line
        xCorn = item->mPtX[ind];
        yCorn = item->mPtY[ind];
        cenXdel = xCorn - item->mStageX;
        cenYdel = yCorn - item->mStageY;
        dist = sqrtf(powf(cenXdel, 2.f) + powf(cenYdel, 2.f));
        nearXstage = xCorn - 0.5f * cenXdel * edgeTestLen / dist;
        nearYstage = yCorn - 0.5f * cenYdel * edgeTestLen / dist;
        farXstage = xCorn + 0.5f * cenXdel * edgeTestLen / dist;
        farYstage = yCorn + 0.5f * cenYdel * edgeTestLen / dist;

        // Convert to image coords
        mWinApp->mMainView->ExternalStageToImage(&mImBufs[mMapBuf], aMat, delX,
          delY, nearXstage, nearYstage, nearXim, nearYim);
        mWinApp->mMainView->ExternalStageToImage(&mImBufs[mMapBuf], aMat, delX,
          delY, farXstage, farYstage, farXim, farYim);

        // Set up for maximum of 100 step sample
        dist = sqrtf(powf(farXim - nearXim, 2.f) + powf(farYim - nearYim, 2.f));
        numSteps = B3DMIN(100, B3DNINT(dist));
        if (nearXim > 2 && nearXim < nxIm - 3 && nearYim > 2 && nearYim < nyIm - 3 &&
          farXim > 2 && farXim < nxIm - 3 && farYim > 2 && farYim < nyIm - 3 &&
          numSteps > 20) {

          // If both ends are inside the image and there are at least 20 steps, set a box
          // size of at least 25 pixels, use square since it is going into a corner
          //SEMTrace('q', "near st %.2f %.2f  im %.0f %.0f  far st %.2f %.2f  im %.0f %.0f",
            //nearXstage, nearYstage, nearXim, nearYim, farXstage, farYstage, farXim, farYim);
          frac = dist / (float)numSteps;
          boxLen = B3DNINT(5. * frac);
          boxWidth = B3DNINT(5. * frac);
          fitX.resize(numSteps + 1);
          fitY.resize(numSteps + 1);
          fitDist.resize(6);
          for (knd = 0; knd < 5; knd++)
            fitDist[knd] = (float)(3 - knd);

          // Get means and near and get average garident by fits to 5 points
          ProcBoxMeansAlongLine(mImBufs[mMapBuf].mImage->getData(), type, nxIm, nyIm, 2,
            boxLen, boxWidth, B3DNINT(nearXim), B3DNINT(nearYim), B3DNINT(farXim),
            B3DNINT(farYim), numSteps, &fitX[0]);

          for (knd = 2; knd < numSteps - 2; knd++) {
            lsFit(&fitDist[0], &fitX[knd - 2], 5, &fitY[knd], &intcp, &ro);
            //SEMTrace('q', "%d  %.2f  %.3f", knd, fitY[knd], ro);
          }

          float firstPeak = -1.e10, secondPeak = -1.e10;
          int firstInd = 0, secondInd = 0;

          // Find two highest peaks with monotonic fall of two boxes each side
          for (knd = 4; knd < numSteps - 4; knd++) {
            if (fitY[knd] > fitY[knd - 1] && fitY[knd] > fitY[knd + 1] &&
              fitY[knd - 1] > fitY[knd - 2] && fitY[knd + 1] > fitY[knd + 2]) {
              if (fitY[knd] > firstPeak) {
                secondPeak = firstPeak;
                secondInd = firstInd;
                firstPeak = fitY[knd];
                firstInd = knd;
              } else if (fitY[knd] > secondPeak) {
                secondPeak = fitY[knd];
                secondInd = knd;
              }
            }
          }
          SEMTrace('q', "first peak %.3f  at %d  second %.3f at %d", firstPeak, firstInd,
            secondPeak, secondInd);

          knd = 0;
          if (firstInd > 0) {
            if (!secondInd) {
              knd = firstInd;
            } else if (firstInd > secondInd) {
              knd = firstInd;
            } else if (secondPeak > 0.5 * firstPeak ||
              (secondPeak > numSteps / 4 - 2 && firstPeak < numSteps / 4)) {
              knd = secondInd;
            } else {
              knd = firstInd;
            }
          }
          if (knd) {
            if (knd < numSteps / 4) {
              SEMTrace('q', "Gradient peak at %d of %d: inside polygon", knd + 1,
                numSteps);
            } else {

              // If all is good, get fractional way out from polygon point and revise
              frac = 2.f * (float)knd / (float)numSteps - 1.f;
              SEMTrace('q', "Revising corner by %.2f from %.2f, %.2f to %.2f, %.2f",
                frac * edgeTestLen / 2.f, xCorn, yCorn, xCorn + frac *
                (farXstage - xCorn), yCorn + frac * (farYstage - yCorn));
              xCorn += frac * (farXstage - xCorn);
              yCorn += frac * (farYstage - yCorn);
            }
          }
        }

        // Now compute position that offsets the corner from the center of field
        frac = cenOffset * FOV / dist;
        mRRGxStages[jnd] = frac * item->mStageX + (1.f - frac) * xCorn;
        mRRGyStages[jnd] = frac * item->mStageY + (1.f - frac) * yCorn;
        cornInd[jnd++] = ind;
        if (jnd >= 3)
          break;
      }
    }
    minLength /= 2.f;
  }
  if (jnd < 3) {
    errStr = "Cannot find three well-separated corners of the chosen polygon";
    return 1;
  }
  SEMTrace('q', "Corner points %d  %d  %d", cornInd[0], cornInd[1], cornInd[2]);

  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(ldArea);

  // Prepare to proceed with task
  mRefineStepIndex = 0;
  mRefineItem = mapItem;
  mRefineSetNum = setNum;
  mDoingMultiGrid = MG_REFINE_REALIGNED;
  mWinApp->SetStatusText(MEDIUM_PANE, "REFINING GRID ALIGNMENT");
  mWinApp->UpdateBufferWindows();
  MoveStageForRefineMapAlign();
  return 0;
}

// Make a standard Navigator move to the current position to evaluate
void CMultiGridTasks::MoveStageForRefineMapAlign()
{
  float backX, backY, montErrX, montErrY;
  mNavigator->MontErrForItem(mRefineItem, montErrX, montErrY);
  mNavigator->BacklashForItem(mRefineItem, backX, backY);
  mNavigator->AdjustAndMoveStage(mRRGxStages[mRefineStepIndex] + montErrX, 
    mRRGyStages[mRefineStepIndex] + montErrY, mRefineItem->mStageZ,
    axisX | axisY | (mRefineStepIndex ? 0 : axisZ), backX, backY);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_REFINE_MOVED, 300000);
}

// Next task for refining alignment
void CMultiGridTasks::RefineRealignNextTask(int param)
{
  ScaleMat aMat;
  float delX, delY, imageX, imageY, scale, rot, xShift, yShift, xDeltas[3], yDeltas[3];
  float delDiffs[3], minDiff = 1.e10, maxDiff = -1.e10;
  float badDiffCrit = 1., useTwoCritFac = 2.f, useTwoAbsCrit = 0.5f;
  int ind, twoInd, maxInd, midInd, drop = -1;
  double xStage, yStage;
  CString errStr;

  // Take image
  if (param == MG_REFINE_MOVED) {
    mWinApp->mCamera->InitiateCapture(mRefineSetNum);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_REFINE_IMAGE, 0);
  } else {

    // Or align to image: get the image position
    mNavigator->BufferStageToImage(&mImBufs[mMapBuf], aMat, delX, delY);
    mWinApp->mMainView->ExternalStageToImage(&mImBufs[mMapBuf], aMat, delX,
      delY, mRRGxStages[mRefineStepIndex], mRRGyStages[mRefineStepIndex], imageX, imageY);

    // Align
    if (mWinApp->mProcessImage->AlignBetweenMagnifications(mMapBuf, imageX, imageY, -0.8f,
      mNavHelper->GetScaledAliDfltPctChg() / 100.f,
      2.f * mRRGMaxRotation, false, scale, rot, 0, errStr)) {
      StopMultiGrid();
      SEMMessageBox("Error aligning to grid map: " + errStr);
      return;
    }
    mWinApp->ToggleBuffers(1, 300, 6, 10);

    // Get shift and equivalent stage position
    mImBufs->mImage->getShifts(xShift, yShift);
    ScaleMat bMat, bInv;
    float xTiltFac, yTiltFac;

    // The shift is amount that
    // image is shifted in inverted coordinates, so invert for RH camera coordinates
    // The rest is the same as in SetAlignShifts for getting stage move
    yShift = -yShift;
    bMat = mShiftManager->FocusAdjustedStageToCamera(mImBufs);
    bInv = MatInv(bMat);
    mShiftManager->GetStageTiltFactors(xTiltFac, yTiltFac);
    xStage  = mRRGnewXstage[mRefineStepIndex] = mRRGxStages[mRefineStepIndex] - 
      mImBufs->mBinning * (bInv.xpx * xShift + bInv.xpy * yShift) / xTiltFac;
    yStage  = mRRGnewYstage[mRefineStepIndex] = mRRGyStages[mRefineStepIndex] - 
      mImBufs->mBinning * (bInv.ypx * xShift + bInv.ypy * yShift) / yTiltFac;
    SEMTrace('q', "corner %d shift: %.3f  %.3f", mRefineStepIndex, 
      xStage - mRRGxStages[mRefineStepIndex], yStage - mRRGyStages[mRefineStepIndex]);

    // Move to next
    if (mRefineStepIndex < 2) {
      mRefineStepIndex++;
      MoveStageForRefineMapAlign();
      return;
    }

    // Or finish up with result
    // Get the shift amounts
    for (ind = 0; ind < 3; ind++) {
      xDeltas[ind] = mRRGnewXstage[ind] - mRRGxStages[ind];
      yDeltas[ind] = mRRGnewYstage[ind] - mRRGyStages[ind];
    }

    // Compute difference between each pair of shifts and keep track of pair with minimum
    for (ind = 0; ind < 3; ind++) {
      delDiffs[ind] = sqrtf(powf(xDeltas[(ind + 1) % 3] - xDeltas[ind], 2.f) +
        powf(yDeltas[(ind + 1) % 3] - yDeltas[ind], 2.f));
      if (!ind || delDiffs[ind] < minDiff) {
        minDiff = delDiffs[ind];
        twoInd = ind;
      }
      if (!ind || delDiffs[ind] > maxDiff) {
        maxDiff = delDiffs[ind];
        maxInd = ind;
      }
    }
    for (ind = 0; ind < 3; ind++)
      if (ind != maxInd && ind != twoInd)
        midInd = ind;
    SEMTrace('q', "Differences between shifts: %.3f  %.3f  %.3f", minDiff,
      delDiffs[midInd], maxDiff);
    StopMultiGrid();

    // If applying and it exceeds limit, report that as error
    if (mMaxRefineDiff > 0)
      badDiffCrit = mMaxRefineDiff;
    if (mMaxRefineDiff >= 0. && minDiff > badDiffCrit) {
      errStr.Format("Differences between all three shifts exceed %.1f microns, items "
            "are not being shifted", minDiff);
      SEMMessageBox(errStr);
    } else {

      // Otherwise average two or three positions
      if (delDiffs[midInd] > useTwoCritFac * minDiff && maxDiff > useTwoAbsCrit)
        drop = (twoInd + 2) % 3;
      xShift = yShift = 0.;
      for (ind = 0; ind < 3; ind++) {
        if (ind != drop) {
          xShift += xDeltas[ind];
          yShift += yDeltas[ind];
        }
      }
      xShift /= (drop < 0 ? 3.f : 2.f);
      yShift /= (drop < 0 ? 3.f : 2.f);

      // Shift or just report
      if (mMaxRefineDiff >= 0.) {
        ind = mNavigator->ShiftItemsAtRegistration(xShift, yShift, 
          mNavigator->GetCurrentRegistration(), 0);
        PrintfToLog("%d items shifted by %.3f  %.3f microns from average of %d positions",
          ind, xShift, yShift, drop < 0 ? 3 : 2);
      } else {
        PrintfToLog("Average shift from %d positions was %.3f  %.3f microns", 
          drop < 0 ? 3 : 2, xShift, yShift);
      }
    }
  }
}

/////////////////////////////////////////////////////////
//  MULTIPLE GRID RUNS
/////////////////////////////////////////////////////////

/*
 * Start a series of runs given the parameters
 */
int CMultiGridTasks::StartGridRuns(int LMneedsLD, int MMMneedsLD, int finalNeedsLD,
  int finalCamera, bool undoneOnly)
{
  int err, grid, ind, apSize = 0, numAcq, numTS, acqType, numFail = 0, numDark = 0;
  int magInd, camera, ldArea, numPoly, jnd, numMSparam = 0, maxStamp, minStamp;
  int numXformed = 0, paramXformable = 0, curSetXformable = 0, backXformable = 0;
  int backXfStamp, subXfStamp;
  float halfx, halfy, size;
  double dists[3], avgDist, angle;
  FloatVec msDists, msAngles;
  ScaleMat cam2st;
  CFileStatus status;
  JeolCartridgeData jcd;
  CString str, directory, fname;
  NavAcqParams *mapParams = mWinApp->GetNavAcqParams(0);
  NavAcqParams *finalParams = mWinApp->GetNavAcqParams(1);
  NavAcqAction *mapActions = mNavHelper->GetAcqActions(0);
  NavAcqAction *finalActions = mNavHelper->GetAcqActions(1);
  NavAcqParams *testAcqParams;
  NavAcqAction *testActions;
  MultiShotParams testMSparams;
  MultiShotParams *curMSparams;
  MGridMultiShotParams mgMSparam;
  ZbyGParams *zbygParam;
  StateParams *state;
  int *stateNums;
  IntVec dlgIndToJcdInd, gridsDoingMulShot;
  CameraParameters *camParams = mWinApp->GetCamParams() + finalCamera;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CMapDrawItem *item, *mapItem;
  MapItemArray *itemArray;
  BOOL LDforZbyG, usingView;
  bool noFrameSubs, movable, doRefine, doPolyMonts, fpMultiShot, sameXform;
  bool apForLMM = false;
  bool stateForLMM = false;
  bool stateForMMM = false;
  bool stateForAcq = false;
  bool needForget = false;
  BOOL mapLacksMarkerShift = mParams.acquireLMMs;
  bool didLMMforGrid = false, didMMMforGrid = false, didAcqForGrid = false;
  bool lastMMMforGrid = false, lastAcqForGrid = false;
  bool setupLMMimaging = false, skipRealign = false;
  mSkipEucentricity = mWinApp->mComplexTasks->GetFEMaxTilt() >
    mScope->GetMaxTiltAngle() + 1.;
  mNavigator = mWinApp->mNavigator;
  mSingleGridMode = !mScope->GetScopeHasAutoloader();
  doRefine = (mShowRefineAfterRealign || mSkipGridRealign) &&
    mParams.refineAfterRealign && !mSingleGridMode;
  if (doRefine)
    itemArray = mNavigator->GetItemArray();

  mMGdlg = mNavHelper->mMultiGridDlg;
  dlgIndToJcdInd = mMGdlg->GetDlgIndToJCDindex();
  mReinsertObjAp = false;
  mRestoreCondAp = 0;
  mNoMarkerShifts = false;
  mSavedLowDose = mWinApp->LowDoseMode();

  // Get the various setup parameters and lock them in
  if (!mNamesLocked) {
    mPrefix = mMGdlg->m_strPrefix;
    mAppendNames = mParams.appendNames;
    mUseSubdirs = mParams.useSubdirectory;
    mWorkingDir = mWinApp->mDocWnd->GetInitialDir();
    mNamesLocked = 2;
    mMGdlg->UpdateEnables();
    mMGdlg->UpdateCurrentDir();
  }

  // Assign these items as what will be used if doing LMMs, otherwise use existing values
  if (mParams.acquireLMMs) {
    mLMMneedsLowDose = LMneedsLD;
    mLMMusedStateNum = mParams.setLMMstate ? mParams.LMMstateNum : -1;
    mLMMusedStateName = mParams.LMMstateName;
    mLMMmagIndex = mGridMontParam.magIndex;
  }

  // Get the grid lists
  mNumGridsToRun = mMGdlg->GetListOfGridsToRun(mJcdIndsToRun, mSlotNumsToRun);
  if (!mNumGridsToRun)
    return 0;

  mActiveCameraList = mWinApp->GetActiveCameraList();

  // Get state of apertures if they are to be changed and error out if bad
  if (mParams.removeObjectiveAp) {
    apSize = mScope->GetApertureSize(OBJECTIVE_APERTURE);
    mInitialObjApSize = apSize;

  }
  if (apSize >= 0 && mParams.setCondenserAp) {
    apSize = mScope->GetApertureSize(mSingleCondenserAp);
    mInitialCondApSize = apSize;
    if (apSize && mUseTwoJeolCondAp)
      mInitialC1CondSize = mScope->GetApertureSize(JEOL_C1_APERTURE);
  }
  if (apSize < 0) {
    SEMMessageBox("Aperture control appears not to work on this scope");
    return 1;
  }
  if (mParams.setCondenserAp && JEOLscope) {
    ind = mScope->FindApertureIndexFromSize(
      (mUseTwoJeolCondAp && !mParams.C1orC2condenserAp) ?
      JEOL_C1_APERTURE : mSingleCondenserAp, mParams.condenserApSize, str);
    if ((mParams.condenserApSize && !ind) || ind < 0) {
      SEMMessageBox("There is a problem with condenser aperture size: \r\n" + str);
      return 1;
    }
  }

  if (mSingleGridMode && !mReferenceCounts && mParams.acquireLMMs) {
    if (!KGetOneFloat("Enter the reference counts in a low mag mapping image with no beam"
      , mReferenceCounts, 0) || !mReferenceCounts)
      return 1;
  }

  // If not doing LM maps or reference counts exist and grid on stage is to be done first, 
  // make sure grid is aligned if it might be, and set to skip realign if it is
  if (mMGdlg->GetOnStageDlgIndex() >= 0 &&
    (!mParams.acquireLMMs || mReferenceCounts > 0.)) {
    ind = dlgIndToJcdInd[mMGdlg->GetOnStageDlgIndex()];
    
    if (ind == mJcdIndsToRun[0] && mLoadedGridIsAligned < 0 && !mSkipGridRealign &&
      (mParams.acquireMMMs || mParams.runFinalAcq)) {
      jcd = mCartInfo->GetAt(ind);
      str.Format("Is the grid on the stage, # %d, still aligned to the grid"
        " map or has it been disturbed since it was aligned?   Press:\n\n"
        "\"Aligned\" if it is still aligned\n"
        "\"Disturbed\" if the realign to grid map routine needs to be run", jcd.id);

      if (SEMThreeChoiceBox(str, "Aligned", "Disturbed", "", MB_QUESTION) == IDYES)
        mLoadedGridIsAligned = 1;
      else
        mLoadedGridIsAligned = 0;
    }
    skipRealign = ind == mJcdIndsToRun[0] && mLoadedGridIsAligned > 0;
  }

  // Check for dark and failed and make sure they want to proceed with failed
  for (grid = 0; grid < mNumGridsToRun; grid++) {
    jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
    if (jcd.status & MGSTAT_FLAG_FAILED)
      numFail++;
    if (jcd.status & MGSTAT_FLAG_TOO_DARK)
      numDark++;
  }
  if (numDark) {
    SEMMessageBox("Grids that were found to be too dark cannot be included because "
      "they can not be realigned");
    return 1;
  }
  if (numFail) {
    str.Format("The selected grids include %d grids where something failed "
      "previously\n\nAre you sure you want to proceed with them?", numFail);
    if (AfxMessageBox(str, MB_QUESTION) == IDNO)
      return 1;
    for (grid = 0; grid < mNumGridsToRun; grid++)
      ChangeStatusFlag(grid, MGSTAT_FLAG_FAILED, 0);
  }

  // Make sure there are groups if autocontouring
  if (mParams.acquireLMMs && mParams.autocontour && !mHaveAutoContGroups) {
    mNavHelper->mAutoContouringDlg->SyncToMasterParams();
    if (mNavHelper->mAutoContouringDlg->IsOpen())
      CopyAutoContGroups();
    err = 0;
    if (mHaveAutoContGroups)
      for (ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++)
        err += mAutoContGroups[ind];
    if (!err) {
      SEMMessageBox("You have selected to do autocontouring but have\nnot yet selected"
        " any contour groups to be converted to polygons.\n\nUse the Setup button"
        " to open the Autocontouring dialog for selecting groups");
      return 1;
    }
  }

  // Make sure script is runnable
  if (mParams.acquireLMMs && mParams.runMacroAfterLMM) {
    if (mWinApp->mMacroProcessor->EnsureMacroRunnable(mParams.macroToRun))
      return 1;
  }

  // Do final check for states
  if (mParams.acquireLMMs && CheckStatesInRange(&mParams.LMMstateNum,
    &mParams.LMMstateName, 1, "grid mapping"))
    return 1;
  if (mParams.acquireMMMs && CheckStatesInRange(mParams.MMMstateNums,
    mParams.MMMstateNames, 4, "medium-mag mapping"))
    return 1;
  if (mParams.runFinalAcq) {
    if (CheckStatesInRange(mParams.finalStateNums,
      mParams.finalStateNames, 4, "final acquisition global state"))
      return 1;
    for (grid = 0; grid < mNumGridsToRun; grid++) {
      jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
      str.Format("final acquisition state for grid %d", jcd.id);
      if (CheckStatesInRange(jcd.acqStateNums, jcd.acqStateNames, 4, str))
        return 1;
    }
  }

  // If starting with MMMs and fitting polygons, check all nav files for files to open
  if ((mParams.acquireMMMs || mParams.runFinalAcq) && !mParams.acquireLMMs) {
    mNavigator->SaveAndClearTable();
    for (grid = 0; grid < mNumGridsToRun; grid++) {
      fname = FullGridFilePath(grid, ".nav");
      jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
      if (mNavigator->LoadNavFile(false, false, &fname)) {
        str.Format("Error reopening Navigator file for grid # %d:\n %s", jcd.id,
          (LPCTSTR)fname);
        SEMMessageBox(str);
        return 1;
      }
      mNavHelper->CountAcquireItems(0, -1, numAcq, numTS);
      if ((mParams.acquireMMMs && !numAcq) ||
        (!mParams.acquireMMMs && !numAcq && !numTS)) {
        str.Format("There are no items set for Acquire%s in this Navigator file for "
          "grid # %d", mParams.acquireMMMs ? "" : " or tilt series", jcd.id);
        SEMMessageBox(str);
        return 1;
      }

      // Check if map has a marker shift
      if (!PrepareAlignToGridMap(mJcdIndsToRun[grid], false, &mapItem, str) &&
        (mapItem->mMarkerShiftX < EXTRA_VALUE_TEST || !mapItem->mShiftCohortID))
        mapLacksMarkerShift = true;

      // Make sure polygons exist for refine operation or for polygon montage
      doPolyMonts = mParams.acquireMMMs && mParams.MMMimageType == 1;
      if (doRefine || doPolyMonts) {
        numAcq = 0;
        numPoly = 0;
        itemArray = mNavigator->GetItemArray();
        for (ind = 0; ind < (int)itemArray->GetSize(); ind++) {
          item = itemArray->GetAt(ind);
          if (item->IsPolygon() && item->mAcquire)
            numPoly++;
          if (item->IsPolygon() && item->mDrawnOnMapID == mapItem->mMapID) {
            if (item->mGroupID > 0) {
              numAcq = 1;
              if (!doPolyMonts)
                break;
            }
            size = sqrtf(mNavigator->ContourArea(item->mPtX, item->mPtY, item->mNumPoints));
            if (size < mMaxSizeForRefinePolys) {
              numAcq = 1;
              if (!doPolyMonts)
                break;
            }
          }
        }
        if (doRefine && !numAcq) {
          str.Format("There are no polygons suitable for refining grid alignment in the"
            " Navigator file for grid # %d", jcd.id);
          SEMMessageBox(str);
          return 1;
        }
        if (doPolyMonts && !numPoly) {
          str.Format("You have selected to acquire polygon montages but\n"
            "there are no polygons marked for acquisition in the"
            " Navigator file for grid # %d", jcd.id);
          SEMMessageBox(str);
          return 1;
        }
      }

      if (mParams.acquireMMMs && mParams.MMMimageType > 0 &&
        mNavHelper->AreAnyFilesSetToOpen()) {
        if (mParams.MMMimageType == 1 && mNavHelper->AreOnlyReusableMontagesSetToOpen()) {
          mNavHelper->EndAllNewFilesSetToOpen();
          PrintfToLog("There were files set to open for polygon montages for grid # %d;"
            "\r\n  these have been cleared out so that new files can be set up", jcd.id);
          mNavigator->DoSave(false);
        } else {
          str.Format("There are files set to open in this Navigator file for grid # %d;"
            " this is not allowed when saving montages", jcd.id);
          SEMMessageBox(str);
          return 1;
        }
      }

      // Determine nav and multishot settings and run the acquire check
      testAcqParams = mParams.acquireMMMs ? mapParams : finalParams;
      testActions = mParams.acquireMMMs ? mapActions : finalActions;
      if (!mParams.acquireMMMs && jcd.finalDataParamIndex >= 0) {
        MGridAcqItemsParams acqParam =
          mMGAcqItemsParamArray.GetAt(jcd.finalDataParamIndex);
        testAcqParams = &acqParam.params;
        testActions = acqParam.actions;
      }
      acqType = testAcqParams->acquireType;
      testAcqParams->acquireType = testAcqParams->nonTSacquireType;
      if (testAcqParams->acquireType == ACQUIRE_MULTISHOT)
        testAcqParams->useMapHoleVectors = mParams.msVectorSource == 0;
      if (jcd.multiShotParamIndex >= 0) {
        MGridMultiShotParams mgParam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
        GridToMultiShotSettings(mgParam, &testMSparams);
      }
      str.Format("FOR GRID # %d:\r\n", jcd.id);
      err = mNavHelper->AssessAcquireForParams(testAcqParams, testActions,
        (jcd.multiShotParamIndex >= 0) ? testMSparams :
        *(mNavHelper->GetMultiShotParams()), 0, -1, str);
      testAcqParams->acquireType = acqType;
      if (err)
        return 1;

      // try to assess Z by G cal
      if ((testActions[NAACT_EUCEN_BY_FOCUS].flags & NAA_FLAG_RUN_IT) != 0) {
        magInd = mScope->GetMagIndex();
        camera = mWinApp->GetCurrentCamera();
        if (mParams.acquireMMMs) {
          LDforZbyG = MMMneedsLD;
          stateNums = mParams.MMMstateNums;
        } else {
          LDforZbyG = finalNeedsLD;
          stateNums = mParams.finalStateNums;
          if (jcd.acqStateNums[0] >= 0 || jcd.acqStateNums[1] >= 0 || 
            jcd.acqStateNums[2] >= 0 || jcd.acqStateNums[3] >= 0)
            stateNums = &jcd.acqStateNums[0];
        }

        // Check states for different camera or low-dose value
        state = NULL;
        for (ind = 0; stateNums && ind < 4; ind++) {
          if (stateNums[ind] >= 0) {
            state = mStateArray->GetAt(stateNums[ind]);
            camera = state->camIndex;
            if (state->lowDose != 0)
              LDforZbyG = true;
          }
        }
        ldArea = mWinApp->mParticleTasks->GetLDAreaForZbyG(LDforZbyG, -1, usingView);

        // If in low dose, get mag from currenr area or matching state
        if (ldArea >= 0) {
          magInd = ldp[ldArea].magIndex;
          for (ind = 0; stateNums && ind < 4; ind++) {
            if (stateNums[ind] >= 0) {
              state = mStateArray->GetAt(stateNums[ind]);
              if (mNavHelper->AreaFromStateLowDoseValue(state, &ind) == ldArea) {
                magInd = state->ldParams.magIndex;
                break;
              }
            }
          }

          // Outside of low dose, it could come from state
        } else if (state) {
          magInd = state->magIndex;
        }
        zbygParam = mWinApp->mParticleTasks->FindZbyGCalForMagAndArea(magInd, ldArea,
          camera, ind, err);

        // Outside of low dose, spot and probe/alpha have to match
        if (zbygParam && ldArea < 0) {
          if (zbygParam->spotSize != (state ? state->spotSize : mScope->FastSpotSize()) ||
            (FEIscope && zbygParam->probeOrAlpha !=
            (state ? state->probeMode : mScope->GetProbeMode())) ||
              (JEOLscope && !mScope->GetHasNoAlpha() && zbygParam->probeOrAlpha !=
            (state ? state->beamAlpha : mScope->FastAlpha())))
            zbygParam = NULL;
        }

        if (!zbygParam) {
          str.Format("Eucentricity by Focus is selected to be run for grid # %d"
            " but there is no calibration available for the acquisition conditions being"
            " used", jcd.id);
          SEMMessageBox(str);
          return 1;
        }
      }
    }
  }

  // A couple of sanity checks
  if (mParams.acquireMMMs || mParams.runFinalAcq) {

    // Make sure refine will work
    if (doRefine && mMGdlg->CheckRefineOptions(size, grid, ind, &state, str)) {
      SEMMessageBox(str);
      return 1;
    }

    // Check for shift offsets and marker shifts unless user has said to skip the latter
    if (!mSkipMarkerShifts) {
      err = CheckShiftsForLMmode(mLMMmagIndex);
      if (err < 0)
        return 1;

      // There has to be/have been something!
      if (err == 0) {
        str.Format("To use grid map locations at higher magnification,\n"
          "you must acquire maps with Low Dose View or Search with a\n"
          "shift offset defined, or have a marker shift stored for magnification %d",
          MagForCamera(mWinApp->GetCurrentCamera(), mLMMmagIndex));
        SEMMessageBox(str);
        return 1;
      }

      // No marker shifts but offsets, so don't try for shifts
      if (err == 2)
        mNoMarkerShifts = true;

      // Both and they have not been applied yet: ask what to do and record answer
      if (err == 3 && mapLacksMarkerShift) {
        str = "There is both a shift offset for the Low Dose area\n"
          "used for the grid map, and a marker shift stored for the\n"
          "grid map magnification";
        if (!mParams.acquireLMMs)
          str += " that has not been applied yto some maps yet";
        str += ".\n\nDo you want to apply the marker shift?\n"
          "(Answer No if the LD shift offset supercedes the marker shift;\n"
          "or Yes if the marker shift is needed even with the LD shift offset.)";
        if (AfxMessageBox(str, MB_QUESTION) == IDNO)
          mSkipMarkerShifts = 1;
        else
          mSkipMarkerShifts = -1;
      }
    }

    // Set run-time flag to skip if session flag is now set
    if (mSkipMarkerShifts > 0)
      mNoMarkerShifts = true;
  }

  if (mParams.acquireLMMs && mParams.acquireMMMs && !mParams.autocontour) {
    SEMMessageBox("To do both grid maps and medium mag maps in the same run,"
      " you need to select Autocontouring so that there are items to acquire");
    return 1;
  }
  mUsingHoleFinder = mParams.acquireMMMs &&
    (mapActions[NAACT_HOLE_FINDER].flags & NAA_FLAG_RUN_IT) != 0;
  if (mParams.acquireMMMs && mParams.runFinalAcq) {
    if (!mUsingHoleFinder) {
      SEMMessageBox("To do both medium mag maps and final acquisition in the same run,"
        " you need to select hole finding as a mapping task");
      return 1;
    }
    if (!mapParams->runHoleCombiner && finalParams->nonTSacquireType == ACQUIRE_MULTISHOT
      && AfxMessageBox("You selected Multiple Records for final\nacquisition but did not"
        " set the option to use the hole combiner.\n\nDo you really want to proceed?", 
        MB_QUESTION) == IDNO)
      return 1;
    /*if (!finalParams->useMapHoleVectors && 
      finalParams->nonTSacquireType == ACQUIRE_MULTISHOT && mNumGridsToRun > 1) {
      AfxMessageBox("You are trying to do Multiple Records on \n"
        "more than one grid and did not select to use map hole vectors for shifts", 
        MB_EXCLAME);
      return 1;
    }*/
  }

  // Set flags for parameter swaps needed
  // Testing nonTSacquireType because regular acquireType is still 0 for read-in
  // parameters and/or comes through as 0 in what is tested
  mUsingAutoContour = mParams.acquireLMMs && mParams.autocontour;
  if (mParams.runFinalAcq) {

    // Make list of grids using multishot and number with params in case vectors
    // come from settings
    fpMultiShot = finalParams->nonTSacquireType == ACQUIRE_MULTISHOT;
    mUsingMultishot = fpMultiShot;
    if (mMGMShotParamArray.GetSize() > 0) {
      for (grid = 0; grid < mNumGridsToRun; grid++) {
        jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
        if (jcd.finalDataParamIndex >= 0) {
          MGridAcqItemsParams acqParam =
            mMGAcqItemsParamArray.GetAt(jcd.finalDataParamIndex);
          if (acqParam.params.nonTSacquireType == ACQUIRE_MULTISHOT) {
            mUsingMultishot = true;
            gridsDoingMulShot.push_back(grid);
            if (jcd.multiShotParamIndex >= 0)
              numMSparam++;
          }
        } else if (fpMultiShot) {
          gridsDoingMulShot.push_back(grid);
          if (jcd.multiShotParamIndex >= 0)
            numMSparam++;
        }
      }
    }

    // If more than one grid has multishot and using settings, check that each has them
    if ((int)gridsDoingMulShot.size() > 1 && mParams.msVectorSource) {
      if (numMSparam < (int)gridsDoingMulShot.size()) {
        str.Format("You set up to do Multiple Records on %d grids and \n"
          "selected for image shifts vectors to come from settings\n"
          "but there are saved settings for only %d of the grids",
          gridsDoingMulShot.size(), numMSparam);
        AfxMessageBox(str, MB_EXCLAME);
        return 1;
      }

      // Make sure they have unique vectors: get distance and angle for each
      mNavHelper->UpdateMultishotIfOpen();
      curMSparams = mNavHelper->GetMultiShotParams();
      sameXform = true;
      for (ind = 0; ind < numMSparam; ind++) {
        grid = gridsDoingMulShot[ind];
        jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
        mgMSparam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
        GridToMultiShotSettings(mgMSparam, &testMSparams);
        mNavHelper->GetMultishotDistAndAngles(&testMSparams, testMSparams.doHexArray,
          dists, avgDist, angle);
        msDists.push_back((float)avgDist);
        msAngles.push_back((float)angle);

        // Keep track of number xformable etc, and if the xforms are the same
        magInd = (int)mgMSparam.values[MS_origMagOfArray];
        if (magInd > 0) {
          numXformed++;
          if (testMSparams.xformFromMag == magInd)
            backXformable++;
        } else {
          if (testMSparams.xformFromMag == -magInd)
            paramXformable++;
        }
        if (curMSparams->xformFromMag == B3DABS(magInd))
          curSetXformable++;
        if (!ind) {
          minStamp = maxStamp = testMSparams.xformMinuteTime;
        } else {
          ACCUM_MIN(minStamp, testMSparams.xformMinuteTime);
          ACCUM_MAX(maxStamp, testMSparams.xformMinuteTime);
        }
      }

      // Compare distances and angles
      for (ind = 0; ind < numMSparam - 1; ind++) {
        for (jnd = ind + 1; jnd < numMSparam; jnd++) {
          if (fabs(msDists[ind] - msDists[jnd]) < 1.e-4 &&
            fabs(msAngles[ind] - msAngles[jnd]) < 1.e-3) {
            grid = gridsDoingMulShot[ind];
            jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
            err = jcd.id;
            grid = gridsDoingMulShot[jnd];
            jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
            str.Format("The Multiple Record settings for grids %d and %d have the same"
              " vectors", err, jcd.id);
            AfxMessageBox(str, MB_EXCLAME);
            return 1;
          }
        }
      }

      // Determine whether to use new transforms for any and whether to back-transform
      backXfStamp = -2;
      subXfStamp = -2;
      if (curSetXformable &&
        ((minStamp == maxStamp && minStamp != curMSparams->xformMinuteTime) ||
        (minStamp != maxStamp && maxStamp == curMSparams->xformMinuteTime))) {

        // All the same and a new one was done, or the current doesn't match the earliest
        str = "You have made a new refining transform for vectors since the";
        if (minStamp != maxStamp)
          str += " earliest";
        str += " Multiple Record settings were stored.  Do you want to use it";
        if (minStamp != maxStamp)
          str += " in the earliest settings";
        str += "?   Press:\n\n";
        str += backXformable ? "\"Use New for Raw\"" : "\"Use New\"";
        str += " to apply the new transform to vectors that have not been "
          "transformed";
        str += minStamp != maxStamp ? " in the earliest settings\n" : "\n";
        if (backXformable) {
          str += "\"Use New for All\" to use the new transform for all vectors";
          if (minStamp != maxStamp)
            str += " in the earliest settings";
          str += ", refining ones that have already been transformed\n";
        }
        str += "\"Use Old\" to go on with the transformations stored in the settings";
        ind = SEMThreeChoiceBox(str, backXformable ? "Use New for Raw" : "Use New",
          backXformable ? "Use New for All" : "Use Old",
          backXformable ? "Use Old" : "", backXformable ? MB_YESNOCANCEL : MB_QUESTION);
        if (ind == IDYES)
          subXfStamp = minStamp;
        if (backXformable && ind == IDNO) {
          subXfStamp = minStamp;
          backXfStamp = minStamp;
        }

      } else if (curSetXformable && minStamp != curMSparams->xformMinuteTime) {

        // Otherwise, multiple stored one and a new current one!
        str = "You have made a new refining transform for vectors since some"
          " Multiple Record settings were stored.  Do you want to use it?   Press:\n\n"
          "\"Use for Earliest\" to apply the new transform only to the settings with the"
          " oldest stored transforms\n"
          "\"Use for All\" to apply the new transform instead in all stored settings\n"
          "\"Use Old\" to go on with the transformations stored in the settings";
        ind = SEMThreeChoiceBox(str, "Use for Earliest", "Use for All", "Use Old",
          MB_YESNOCANCEL);
        if (ind == IDYES)
          subXfStamp = minStamp;
        if (ind == IDNO)
          subXfStamp = -1;
        if (ind != IDCANCEL && backXformable) {
          if (AfxMessageBox("Do you want to use the new transform to refine\n"
            " vectors that have already been transformed?", MB_QUESTION) == IDYES) {
            backXfStamp = ind == IDYES ? minStamp : -1;
          }
        }
      }

      // Loop on grids doing appropriate actions ending in transformed vectors if possible
      for (ind = 0; ind < numMSparam; ind++) {
        grid = gridsDoingMulShot[ind];
        jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
        mgMSparam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
        GridToMultiShotSettings(mgMSparam, &testMSparams);
        magInd = (int)mgMSparam.values[MS_origMagOfArray];

        // Backtransform if appropriate
        if ((backXfStamp == -1 || backXfStamp == testMSparams.xformMinuteTime) && 
          magInd > 0 && magInd == testMSparams.xformFromMag) {
          TransformStoredVectors(mgMSparam, MatInv(testMSparams.adjustingXform));
          magInd = -magInd;
          mgMSparam.values[MS_origMagOfArray] = (float)magInd;
        }

        // Substitute current transform if appropriate
        if ((subXfStamp == -1 || subXfStamp == testMSparams.xformMinuteTime) && magInd < 0
          && curMSparams->xformFromMag == -magInd) {
          testMSparams.adjustingXform = curMSparams->adjustingXform;
          mgMSparam.values[MS_xformXpx] = curMSparams->adjustingXform.xpx;
          mgMSparam.values[MS_xformXpy] = curMSparams->adjustingXform.xpy;
          mgMSparam.values[MS_xformYpx] = curMSparams->adjustingXform.ypx;
          mgMSparam.values[MS_xformYpy] = curMSparams->adjustingXform.ypy;
          mgMSparam.values[MS_xformMinuteTime] = (float)curMSparams->xformMinuteTime;
        }

        // Transform unconditionally now
        if (magInd < 0 && -magInd == testMSparams.xformFromMag) {
          TransformStoredVectors(mgMSparam, testMSparams.adjustingXform);
          mgMSparam.values[MS_origMagOfArray] = -(float)magInd;
          PrintfToLog("Applied adjusting transform to IS vectors in multiple record"
            " settings for grid %d", jcd.id);
        }
        mMGMShotParamArray.SetAt(jcd.multiShotParamIndex, mgMSparam);
      }
    }
  }

  // Make the directories if they don't exist yet.
  if (mUseSubdirs) {
    for (grid = 0; grid < mNumGridsToRun; grid++) {
      jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);
      directory = mWorkingDir + "\\" + RootnameFromUsername(jcd);
      if (!CFile::GetStatus((LPCTSTR)directory, status)) {
        if (_mkdir((LPCTSTR)directory)) {
          err = GetLastError();
          str.Format("Error (%d) trying to create directory %s", err, (LPCTSTR)directory);
          SEMMessageBox(str);
          return 1;
        }
      }
    }
  }

  // Save directory for frame saving if doing final and camera permits it
  if (mParams.runFinalAcq && ((camParams->canTakeFrames & 1) ||
    mCamera->IsDirectDetector(camParams))) {
    mCamNumForFrameDir = finalCamera;
    mSavedDirForFrames = mCamera->GetCameraFrameFolder(camParams, noFrameSubs, movable);
    mSavedLastFrameNum = mCamera->GetLastUsedFrameNumber();
    mSavedFrameBaseName = mCamera->GetFrameBaseName();
    mSavedNumberedFolder = mCamera->GetNumberedFrameFolder();
    mSavedNumberedPrefix = mCamera->GetNumberedFramePrefix();
  }

  // Determine 4 positions around center for eucentricity
  if (mParams.acquireLMMs && !mSkipEucentricity) {
    cam2st = MatInv(mShiftManager->StageToCamera(
      mActiveCameraList[mGridMontParam.cameraIndex], mGridMontParam.magIndex));
    halfx = (float)(mGridMontParam.xFrame * mGridMontParam.binning / 2);
    halfy = (float)(mGridMontParam.yFrame * mGridMontParam.binning / 2);
    mShiftManager->ApplyScaleMatrix(cam2st, halfx, halfy, mEucenXtrials[0],
      mEucenYtrials[0]);
    mShiftManager->ApplyScaleMatrix(cam2st, -halfx, halfy, mEucenXtrials[1],
      mEucenYtrials[1]);
    mShiftManager->ApplyScaleMatrix(cam2st, -halfx, -halfy, mEucenXtrials[2],
      mEucenYtrials[2]);
    mShiftManager->ApplyScaleMatrix(cam2st, halfx, -halfy, mEucenXtrials[3],
      mEucenYtrials[3]);
  }

  mActSequence.clear();
  for (grid = 0; grid < mNumGridsToRun; grid++) {
    jcd = mCartInfo->GetAt(mJcdIndsToRun[grid]);

    // Low mag map operations
    if (mParams.acquireLMMs && (!undoneOnly || (jcd.status & MGSTAT_FLAG_LM_MAPPED) == 0))
    {
      if (!setupLMMimaging || lastMMMforGrid || lastAcqForGrid) {
        AddToSeqForLMimaging(apForLMM, stateForLMM, mLMMneedsLowDose);
        setupLMMimaging = true;
      }
 
      // Get reference counts on first grid
      if (!mReferenceCounts && !grid && !mSingleGridMode) {
        mActSequence.push_back(MGACT_UNLOAD_GRID);
        mActSequence.push_back(MGACT_REF_MOVE);
        mActSequence.push_back(MGACT_REF_IMAGE);
      }

      // Load grid and take up to 4 survey images
      if (!mSingleGridMode)
        mActSequence.push_back(MGACT_LOAD_GRID);
      if (!mSkipEucentricity) {
        mActSequence.push_back(MGACT_SURVEY_STAGE_MOVE);
        mActSequence.push_back(MGACT_SURVEY_IMAGE);

        // Last one will move to best place if needed, after which eucentricity, or not
        mActSequence.push_back(MGACT_SURVEY_STAGE_MOVE);
        mActSequence.push_back(MGACT_EUCENTRICITY);
      }

      // Go to center and take montage, allow extra actions if eucentricity failed
      mActSequence.push_back(MGACT_LMM_CENTER_MOVE);
      mActSequence.push_back(MGACT_TAKE_LMM);
      if (!mSkipEucentricity) {
        mActSequence.push_back(MGACT_SURVEY_STAGE_MOVE);
        mActSequence.push_back(MGACT_EUCENTRICITY);
      }

      // Autocontour
      if (mParams.autocontour)
        mActSequence.push_back(MGACT_AUTOCONTOUR);

      // Script
      if (mParams.runMacroAfterLMM)
        mActSequence.push_back(MGACT_RUN_MACRO);
      didLMMforGrid = true;
    }

    // For medium mag maps
    if (mParams.acquireMMMs && (!undoneOnly || (jcd.status & MGSTAT_FLAG_MMM_DONE) == 0))
    {
      mActSequence.push_back(MGACT_MARKER_SHIFT);

      // If LMM wasn't just done, set state for LM, load grid, and realign to grid
      if (!didLMMforGrid && !mSingleGridMode && !skipRealign) {
        AddToSeqForReloadRealign(apForLMM, stateForLMM);
      }

      // Restore from LM imaging if set
      AddToSeqForRestoreFromLM(apForLMM, stateForLMM);

      // Refine alignment if grid was reloaded or if show refine property is set
      if (doRefine && (!didLMMforGrid || mShowRefineAfterRealign)) {
        mActSequence.push_back(MGACT_REFINE_ALIGN);
      }

      // Manage low dose if needed
      if (MMMneedsLD)
        mActSequence.push_back(MMMneedsLD > 0 ? MGACT_LOW_DOSE_ON : MGACT_LOW_DOSE_OFF);

      // Set state for MMM
      if ((mParams.MMMstateNums[0] >= 0 || mParams.MMMstateNums[1] >= 0 ||
        mParams.MMMstateNums[2] >= 0 || mParams.MMMstateNums[3] >= 0) && !stateForMMM) {
        mActSequence.push_back(MGACT_MMM_STATE);
        stateForMMM = true;
      }

      // Set up files to open or open files
      mActSequence.push_back(MGACT_SETUP_MMM_FILES);

      // Acquire
      mActSequence.push_back(MGACT_TAKE_MMM);
      didMMMforGrid = true;
    }

    // For final acquisition
    if (mParams.runFinalAcq && (!undoneOnly || (jcd.status & MGSTAT_FLAG_ACQ_DONE) == 0))
    {
      mActSequence.push_back(MGACT_MARKER_SHIFT);

      // Reload and realign unless it was just done
      if (!didLMMforGrid && !didMMMforGrid && !mSingleGridMode && !skipRealign) {
        AddToSeqForReloadRealign(apForLMM, stateForLMM);
      } 

      AddToSeqForRestoreFromLM(apForLMM, stateForLMM);

      // Refine alignment
      if (doRefine && !didMMMforGrid) {
        mActSequence.push_back(MGACT_REFINE_ALIGN);
      }

      // Manage low dose if needed
      if (finalNeedsLD)
        mActSequence.push_back(finalNeedsLD > 0 ? MGACT_LOW_DOSE_ON : MGACT_LOW_DOSE_OFF);
      
      // Set state for final acq
      if ((mParams.finalStateNums[0] >= 0 || mParams.finalStateNums[1] >= 0 ||
        mParams.finalStateNums[2] >= 0 || mParams.finalStateNums[3] >= 0) && 
        !stateForAcq) {
        mActSequence.push_back(MGACT_FINAL_STATE);
        stateForAcq = true;
      }

      // Set Focus position
      if (jcd.focusPosParamIndex >= 0)
        mActSequence.push_back(MGACT_FOCUS_POS);

      // Acquire
      mActSequence.push_back(MGACT_FINAL_ACQ);
      didAcqForGrid = true;
    }

    // Restore all states
    if (apForLMM || stateForMMM || stateForAcq)
      needForget = true;
    if (mParams.acquireMMMs || mParams.runFinalAcq || grid == mNumGridsToRun - 1)
      AddToSeqForRestoreFromLM(apForLMM, stateForLMM);
    if (stateForMMM || stateForAcq)
      mActSequence.push_back(MGACT_RESTORE_STATE);
    stateForMMM = stateForAcq = false;
    mActSequence.push_back(MGACT_GRID_DONE);
    lastMMMforGrid = didMMMforGrid;
    lastAcqForGrid = didAcqForGrid;
    didLMMforGrid = false;
    didMMMforGrid = false;
    didAcqForGrid = false;
    skipRealign = false;
  }

  if (GetDebugOutput('q')) {
    for (ind = 0; ind < (int)mActSequence.size(); ind++) {
      if (mActSequence[ind] < sizeof(sActionNames) / sizeof(char *))
        PrintfToLog("%d  %d  %s", ind, mActSequence[ind], 
          sActionNames[mActSequence[ind]]);
    }
  }

  if (needForget && mNavHelper->GetTypeOfSavedState() != STATE_NONE) {
    if (mNavHelper->mStateDlg)
      mNavHelper->mStateDlg->OnButForgetState();
    else
      mNavHelper->ForgetSavedState();
  }

  if (!mWinApp->mLogWindow)
    mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
  if (!mSingleGridMode) {
    mMainLogName = mWinApp->mLogWindow->GetSaveFile();
    if (mMainLogName.IsEmpty()) {
      mMainLogName = mSessionFilename;
      mWinApp->mLogWindow->UpdateSaveFile(true, mMainLogName);
      mMainLogName = mWinApp->mLogWindow->GetSaveFile();
    }
  }

  // Clear out the navigator and initialize all flags then start
  mNavigator->SaveAndClearTable();
  mSeqIndex = -1;
  mCurrentGrid = 0;
  mStateWasSet = false;
  mMovedToFirstTrial = false;
  mSurveyIndex = 0;
  mInternalError = false;
  mStartedLongOp = false;
  mDoingEucentricity = false;
  mMovedAperture = false;
  mAutoContouring = false;
  mSettingUpNavFiles = false;
  mStartedNavAcquire = false;
  mUnloadErrorOK = -1;
  mRestoringOnError = false;
  mFailureRetry = 0;
  mExternalFailedStr = "";
  mExtErrorOccurred = -1;
  mSuspendedMulGrid = false;
  mEndWhenGridDone = false;
  mStoppedAtGridEnd = false;
  mPctPatchSize = -1;
  mSaveMasterMont = *(mWinApp->GetMontParam());
  mDoingMulGridSeq = true;
  mWinApp->UpdateBufferWindows();
  mRunStartTime = GetTickCount();
  mRemainingTime = -1.;
  OutputGridStartLine();
  UpdateGridStatusText();
  MultiGridNextTask(MG_RUN_SEQUENCE);
  return 0;
}

// Set low dose, state, and aperture settings for imaging grid map
void CMultiGridTasks::AddToSeqForLMimaging(bool &apForLMM, bool &stateForLMM, int needsLD)
{
  if (needsLD)
    mActSequence.push_back(needsLD > 0 ? MGACT_LOW_DOSE_ON : MGACT_LOW_DOSE_OFF);
  if (mParams.setLMMstate && !stateForLMM) {
    mActSequence.push_back(MGACT_LMM_STATE);
    stateForLMM = true;
  }

  // Set aperture for LM imaging
  if (mParams.removeObjectiveAp && !apForLMM)
    mActSequence.push_back(MGACT_REMOVE_OBJ_AP);
  if (mParams.setCondenserAp && !apForLMM)
    mActSequence.push_back(MGACT_SET_CONDENSER_AP);
  apForLMM = mParams.removeObjectiveAp || mParams.setCondenserAp;
}

// Add to sequence for loadinga grid and realigning if appropriate
// Include an unload in case it has to iterate
void CMultiGridTasks::AddToSeqForReloadRealign(bool &apForLMM, bool &stateForLMM)
{
  if (!mSkipGridRealign)
    AddToSeqForLMimaging(apForLMM, stateForLMM, mLMMneedsLowDose);
  mActSequence.push_back(MGACT_LOAD_GRID);
  mActSequence.push_back(MGACT_SET_ZHEIGHT);
  if (!mSkipGridRealign) {
    mActSequence.push_back(MGACT_REALIGN_TO_LMM);
    mActSequence.push_back(MGACT_UNLOAD_GRID);
  }
}

// If aperture was set for LM, set up to restore it
void CMultiGridTasks::AddToSeqForRestoreFromLM(bool &apForLMM, bool &stateForLMM)
{
  if (stateForLMM) {
    mActSequence.push_back(MGACT_RESTORE_STATE);
    stateForLMM = false;
  }

  if (apForLMM) {
    if (mParams.removeObjectiveAp)
      mActSequence.push_back(MGACT_REPLACE_OBJ_AP);
    if (mParams.setCondenserAp) {
      if (mUseTwoJeolCondAp)
        mActSequence.push_back(MGACT_RESTORE_C1_AP);
      mActSequence.push_back(MGACT_RESTORE_COND_AP);
    }
    apForLMM = false;
  }
}
// Check if a state number is out of range and issue message
int CMultiGridTasks::CheckStatesInRange(int *stateNums, CString *names, int numStates,
  CString mess)
{
  CString str;
  for (int ind = 0; ind < numStates; ind++) {
    if (stateNums[ind] >= (int)mStateArray->GetSize()) {
      str.Format("State # %d (%s) for %s is no longer in the array of states",
        stateNums[ind], (LPCTSTR)names[ind], (LPCTSTR)mess);
      AfxMessageBox(str, MB_EXCLAME);
      return 1;
    }
  }
  return 0;
}

/*
 * DO THE NEXT OPERATION IN THE SEQUENCE AFTER POST_PROCESSING PREVIOUS ONE
 */
void CMultiGridTasks::DoNextSequenceAction(int resume)
{
  int err, ind, nx, ny, setNum, fileType, timeout = 0, numPieces, meanIXcen, meanIYcen;
  int pctlIXcen, pctlIYcen, numPctl, numAbove, bestInd, firstInd, lastInd, groupInd;
  int action, size, addIdleTask = 1;
  float lowMean, highMean, zStage, meanSXcen, meanSYcen, pctlSXcen, pctlSYcen;
  float dist, bestDist, avgFrac;
  CString errStr, str, label, str2, str3;
  int *stateNums = &mParams.MMMstateNums[0];
  StageMoveInfo moveInfo;
  JeolCartridgeData jcd = mCartInfo->ElementAt(mJcdIndsToRun[B3DMIN(mCurrentGrid,
    (int)mJcdIndsToRun.size() - 1)]);
  JeolCartridgeData &jcdEl = mCartInfo->ElementAt(mJcdIndsToRun[B3DMIN(mCurrentGrid,
    (int)mJcdIndsToRun.size() - 1)]);
  AutoContourParams *acParam;
  MapItemArray *itemArr;
  CMapDrawItem *item;
  BaseMarkerShift baseShift;
  MGridHoleFinderParams hfParam;
  MGridAutoContParams acParm;
  MGridMultiShotParams msParam;
  MGridGeneralParams genParam;
  MGridAcqItemsParams fdParam;
  MGridFocusPosParams fpParam;
  NavAcqParams *acqParams;
  CameraParameters *camParams = mWinApp->GetCamParams();
  FileOptions fileOpt;
  IntVec ixVec, iyVec, pieceSavedAt;
  FloatVec xStage, yStage, meanVec, midFracs, rangeFracs;
  bool skipEucen = false, skipGrid = false, fileAtFirst = false;
  bool movable, noFrameSubs;
  mNavigator = mWinApp->mNavigator;

  if (!mDoingMulGridSeq)
    return;

  if (mParams.acquireLMMs)
    setNum = MontageConSetNum(&mGridMontParam, true);

  // First check for error or stop in the last action
  if (mSeqIndex >= 0 && !resume) {
    action = mActSequence[mSeqIndex];

    if (!mExternalFailedStr.IsEmpty() || mExtErrorOccurred >= 0) {
      SEMTrace('q', "NextAction  ext %d", mExtErrorOccurred);
      mStoppedInLMMont = mStoppedInNavRun = false;
      if (mExtErrorOccurred > 1) {
        mStoppedInLMMont = action == MGACT_TAKE_LMM;
        mStoppedInNavRun = action == MGACT_TAKE_MMM || action == MGACT_FINAL_ACQ;
        SuspendMulGridSeq();
        if (mStoppedInLMMont || mStoppedInNavRun)
          return;
        action = -1;
      }
      switch (action) {

        // Abandon hope
      case MGACT_LMM_STATE:
      case MGACT_UNLOAD_GRID:
      case MGACT_MMM_STATE:
      case MGACT_FINAL_STATE:
      case MGACT_FOCUS_POS:
      case MGACT_SETUP_MMM_FILES:
      case MGACT_RESTORE_STATE:
      case MGACT_LOW_DOSE_ON:
      case MGACT_LOW_DOSE_OFF:
        errStr = "Stopping multiple grid operations";
        if (mExternalFailedStr.IsEmpty())
          str.Format(" because of an error in the current operation (action %s)",
            sActionNames[action]);
        else
          str = " after this error:\n\n" + mExternalFailedStr;
        MGActMessageBox(errStr + str);
        StopMulGridSeq();
        return;

        // Skip to next grid
      case MGACT_LOAD_GRID:
      case MGACT_TAKE_LMM:
      case MGACT_REALIGN_TO_LMM:
      case MGACT_REFINE_ALIGN:
      case MGACT_AUTOCONTOUR:
      case MGACT_RUN_MACRO:
      case MGACT_TAKE_MMM:
      case MGACT_FINAL_ACQ:
        errStr.Format("Skipping to next grid and marking grid %d as failed because of ",
          jcd.id);
        if (mExternalFailedStr.IsEmpty())
          str.Format("an error in the current operation (action %s)",
            sActionNames[action]);
        else 
          str = "this error:\n" + mExternalFailedStr;
        ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
        SaveSessionFileWarnIfError();
        if (SkipToNextGrid(errStr + str))
          return;
        action = -1;
        break;

        // Retry then give up
      case MGACT_REMOVE_OBJ_AP:
      case MGACT_SET_CONDENSER_AP:
      case MGACT_REPLACE_OBJ_AP:
      case MGACT_REF_MOVE:
      case MGACT_RESTORE_COND_AP:
      case MGACT_RESTORE_C1_AP:
      case MGACT_SURVEY_STAGE_MOVE:
      case MGACT_REF_IMAGE:
      case MGACT_SURVEY_IMAGE:
      case MGACT_LMM_CENTER_MOVE:
      case MGACT_SET_ZHEIGHT:
        if (mFailureRetry) {
          errStr = "Stopping multiple grid operations after retrying because of ";
          if (mExternalFailedStr.IsEmpty())
            str.Format("an error in the current operation (action %s)",
              sActionNames[action]);
          else
            str = "this error:\n\n" + mExternalFailedStr;
          MGActMessageBox(errStr + str);
          StopMulGridSeq();
          return;
        }
        mFailureRetry++;
        errStr.Format("Retrying the current operation (action %s) because of ",
          sActionNames[action]);
        if (mExternalFailedStr.IsEmpty())
          errStr += "an error";
        else
          errStr += "this error:\n" + mExternalFailedStr;
        SEMAppendToLog(errStr);
        mSeqIndex--;
        action = -1;
        break;

      default:
        break;
      }
    }

    if (resume == 3) {
      errStr.Format("Skipping further action with grid %d because you selected to resume"
        " this way", jcd.id);
      if (SkipToNextGrid(errStr))
        return;
    } else if (mSuspendedMulGrid)
      action = mActSequence[mSeqIndex];

    if (action >= 0)
      mFailureRetry = 0;

    if (resume)
      action = -1;

    /*
     * Do operations required when an action is done
     */
    switch (action) {

      // After reference image, record the image mean as the reference counts
    case MGACT_REF_IMAGE:
      mReferenceCounts = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
      SEMTrace('q', "Reference counts: %.2f", mReferenceCounts);
      break;

      // After a survey image for eucentricity
    case MGACT_SURVEY_IMAGE:
      mDidEucentricity = false;
      mImBufs->mImage->getSize(nx, ny);
      mPctPatchSize = B3DNINT(1.6 * pow((double)nx * ny, 0.25));
      B3DCLAMP(mPctPatchSize, 50, 100);

      // Get percentile stats, if they are good enough proceed on this position
      if (mWinApp->mProcessImage->PatchPercentileStats(mImBufs, 1., 99.,
        mPctStatMidCrit * mReferenceCounts, mPctStatRangeCrit * mReferenceCounts,
        mPctPatchSize, lowMean, highMean, mPctMidFracs[mSurveyIndex],
        mPctRangeFracs[mSurveyIndex], errStr)) {
        str.Format("WARNING: Error finding percentile statistics from survey image "
          "for grid %d: ", jcd.id);
        SEMAppendToLog(str + errStr);
        skipEucen = true;
      } else {
        avgFrac = 0.5f * (mPctMidFracs[mSurveyIndex] + mPctRangeFracs[mSurveyIndex]);
        SEMTrace('q', "Pctl stats at %.2f,%.2f: low %.1f  high %.1f  midFrac %.3f "
          " rangeFrac %.3f  avg %.3f", mEucenXtrials[mSurveyIndex], 
          mEucenXtrials[mSurveyIndex], lowMean, highMean, mPctMidFracs[mSurveyIndex],
          mPctRangeFracs[mSurveyIndex], avgFrac);
        if (avgFrac >= mPctRightAwayFrac) {

          // In order to do it right here, skip stage move
          mSeqIndex++;
        } else if (mSurveyIndex < 3) {

          // Otherwise back 2 to to do next trial
          mSeqIndex -= 2;
          mSurveyIndex++;
        } else {

          // Trials done, find the best of the possibly bad lot
          mSurveyIndex = 0;
          for (ind = 1; ind < 4; ind++) {
            if (mPctMidFracs[mSurveyIndex] + mPctRangeFracs[mSurveyIndex] <
              mPctMidFracs[ind] + mPctRangeFracs[ind])
              mSurveyIndex = ind;
          }
          SEMTrace('q', "Best one %d, avg frac %.3f", mSurveyIndex, 0.5 *
            (mPctMidFracs[mSurveyIndex] + mPctRangeFracs[mSurveyIndex]));

          // If no good, skip it; otherwise 
          if (0.5 * (mPctMidFracs[mSurveyIndex] + mPctRangeFracs[mSurveyIndex]) <
            mPctUsableFrac) {
            skipEucen = true;
          } else {
            mMovedToFirstTrial = false;
          }
        }
      }

      // Skip over stage move and eucentricity
      if (skipEucen) {
        PrintfToLog("Skipping eucentricity before map for grid %d; no sampled areas are"
          " bright enough", jcd.id);
        mSeqIndex += 2;
      }

      break;

      // After eucentricity, see if failed or not
    case MGACT_EUCENTRICITY:
      mDoingEucentricity = false;
      mDidEucentricity = !mWinApp->mComplexTasks->GetFELastCoarseFailed();
      if (mSurveyIndex == 4) {

        // If this was done after LM map, it's a failure or need to update the Z
        if (!mDidEucentricity) {
          errStr.Format("Could not find eucentricity for grid # %d even in a usable "
            "place; marking it as failed", jcdEl.id);
          ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
          skipGrid = true;
        } else if (mNavigator->DoUpdateZ(0, 0)) {
          errStr.Format("Inexplicable error updating Z for grid # %d;"
            " marking it as failed", jcdEl.id);
          ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
          skipGrid = true;
        } else {

          // Record the Z value to change to upon reload
          item = mNavigator->FindItemWithMapID(jcdEl.LMmapID);
          if (!item)
            item = mNavigator->GetOtherNavItem(0);
          jcdEl.zStage = item->mStageZ;
          mAdocChanged = true;
        }
      }
      SaveSessionFileWarnIfError();
      if (skipGrid && SkipToNextGrid(errStr))
        return;

      break;

      // After the grid montage, make it a map, record Z
    case MGACT_TAKE_LMM:
      mDoingMontage = false;
      mNavigator->NewMap(true, 1);
      mNavigator->SetReloadTableOnNextAdd(10);
      item = mNavigator->GetCurrentItem();
      ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_LM_MAPPED, 1);
      jcdEl.LMmapID = item->mMapID;
      numPieces = mWinApp->mMontageController->ListMontagePieces(mWinApp->mStoreMRC,
        &mGridMontParam, 0, pieceSavedAt, &ixVec, &iyVec, &xStage, &yStage, &meanVec,
        &zStage, &midFracs, &rangeFracs);
      jcdEl.zStage = zStage;
      mWinApp->mDocWnd->DoCloseFile();
      mNavigator->DoSave(false);
      mLoadedGridIsAligned = true;
      mAdocChanged = true;

      // Copy to read buffer in case eucentricity has to happen
      mWinApp->mBufferManager->CopyImageBuffer(1,
        mWinApp->mBufferManager->GetBufToReadInto(), false);
      if (!numPieces || !xStage.size()) {
        ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
        errStr.Format("Error getting piece or stage coordinates from the LM map"
          "for grid %d; marking it as failed", jcdEl.id);
        skipGrid = true;
      } else {

        // Get percentile stats and see if grid has enough usable pieces
        CentroidsFromMeansAndPctls(ixVec, iyVec, xStage, yStage, meanVec, midFracs,
          rangeFracs, mPctUsableFrac, meanIXcen, meanIYcen, meanSXcen, meanSYcen, 
          pctlIXcen, pctlIYcen, pctlSXcen, pctlSYcen, numPctl, numAbove);
        if (numPctl < 0.75 * numPieces) {
          ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
          PrintfToLog("Not enough percentile statistics for grid %d to know if"
            " it is OK; marking it as failed", jcdEl.id);
          skipGrid = true;
        } else if (numAbove < mMinFracOfPiecesUsable * numPctl) {
          ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_TOO_DARK, 1);
          errStr.Format("Only %d of %d pieces are bright enough to be usable for grid %d;"
            " marking it as too dark", numAbove, numPieces, jcdEl.id);
          skipGrid = true;
        }
      }
      SaveSessionFileWarnIfError();

      if (skipGrid) {
        if (SkipToNextGrid(errStr))
          return;

      } else if (!mDidEucentricity && !mSkipEucentricity) {

        // If eucentricity not done yet, find best place, close to center if possible
        bestInd = -1;
        for (ind = 0; ind < numPieces; ind++) {
          if (midFracs[ind] > mPctUsableFrac) {
            dist = B3DMAX(100.f, powf(xStage[ind] * xStage[ind] +
              yStage[ind] * yStage[ind], 0.5f));
            if (bestInd < 0 || midFracs[ind] / dist * dist < midFracs[ind] / bestDist) {
              bestDist = dist * dist;
              bestInd = ind;
            }
          }
        }

        // Stick that in position 5
        mSurveyIndex = 4;
        SEMTrace('q', "Doing eucentricity at %.1f, %.1f", xStage[bestInd], 
          yStage[bestInd]);
        mEucenXtrials[4] = xStage[bestInd];
        mEucenYtrials[4] = yStage[bestInd];

      } else if (!mSkipEucentricity) {

        // Skip eucentricity if not needed
        mSeqIndex += 2;
      }
      break;

      // After autocontouring
    case MGACT_AUTOCONTOUR:
      mAutoContouring = false;

      // Create polygons
      if (mNavHelper->mAutoContouringDlg->ExternalCreatePolys(-1., -1., -1., -1., -1.,
        -1., errStr)) {
        errStr += "\r\nMarking grid as failed due to this failure in autocontouring";
        ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
        SaveSessionFileWarnIfError();
        if (SkipToNextGrid(errStr))
          return;
      }
      itemArr = mNavigator->GetItemArray();
      for (ind = 1; ind < (int)itemArr->GetSize(); ind++) {
        item = itemArr->GetAt(ind);
        if (item->IsPolygon())
          item->mAcquire = true;
      }

      // Set them all to acquire
      mNavigator->FillListBox();
      mNavigator->SetChanged(true);
      mNavigator->ManageCurrentControls();
      mNavigator->Redraw();
      break;

      // After script:
    case MGACT_RUN_MACRO:
      mRunningMacro = false;
      if (mWinApp->mMacroProcessor->GetLastAborted()) {
        ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
        errStr.Format("Marking grid %d as failed because script did not complete", 
          jcd.id);
        if (SkipToNextGrid(errStr))
          return;
      }
      break;

      // After setting up files for MMM's, clear flag
    case MGACT_SETUP_MMM_FILES:
      mSettingUpNavFiles = false;
      break;

      // After either navigator acquire, close files, save Nav and record status
    case MGACT_TAKE_MMM:
    case MGACT_FINAL_ACQ:
      mStartedNavAcquire = false;
      mWinApp->mDocWnd->OnCloseAllFiles();
      mNavigator->DoSave(false);
      if (mNavigator->GetAcquireEnded())
        ChangeStatusFlag(mCurrentGrid, mActSequence[mSeqIndex] == MGACT_TAKE_MMM ?
          MGSTAT_FLAG_MMM_DONE : MGSTAT_FLAG_ACQ_DONE, 1);
      else
        ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
      ReopenMainLog(jcd.id);
      SaveSessionFileWarnIfError();
      break;

      // After grid load/unload, clear flags
    case MGACT_UNLOAD_GRID:
      if (mRealignIteration)
        mSeqIndex -= 4;
    case MGACT_LOAD_GRID:
      mStartedLongOp = false;
      mUnloadErrorOK = 0;
      mSurveyIndex = 0;
      break;

      // After realign, see if transform was within limits
    case MGACT_REALIGN_TO_LMM:
      if (mRRGWasAboveXformLimit) {
        if (mRealignIteration) {
          ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
          errStr.Format("Marking grid %d as failed because realign transformation "
            "exceeded limits", jcd.id);
          if (SkipToNextGrid(errStr))
            return;

        } else {

          // Let it go on to unload, it will have to back up by 4 after the unload
          mWinApp->SetNextLogColorStyle(0, 1);
          PrintfToLog("Realign transformation exceeded limits; reloading grid %d to "
            "try again", jcd.id);
          mRealignIteration++;
        }


      } else {

        // Skip the unload and iterating
        mSeqIndex++;
        mRealignIteration = 0;
      }
    default:
      break;
    }
  }
  
  if (resume > 1) {
    errStr = "";
    if (SkipToNextGrid(errStr))
      return;
  }
  
  // Advance sequence index: if playing games with that, set it to one below needed value
  // Need to back up 1 for resume 1, not increment for resume 2, increment otherwise
  // including resume with skip to next grid, consistent with other skips
  if (resume == 1)
    mSeqIndex--;
  else if (!resume || resume == 3)
    mSeqIndex++;
  mExternalFailedStr = "";
  mExtErrorOccurred = -1;
  if (mSeqIndex >= (int)mActSequence.size()) {
    StopMulGridSeq();
    return;
  }

  // Suspend after index incremented, it needs to be dropped 1 to resume
  if (mSuspendedMulGrid)
    return;

  // Renew the jcd in case it skipped to next grid; it will need a new jcdEl if it used
  // below
  jcd = mCartInfo->ElementAt(mJcdIndsToRun[B3DMIN(mCurrentGrid,
    (int)mJcdIndsToRun.size() - 1)]);
  action = mActSequence[mSeqIndex];
  if (mSeqIndex >= 0 && action < sizeof(sActionNames) / sizeof(char *))
    SEMTrace('q', "Index %d action %d %s", mSeqIndex, action,
      sActionNames[action]);

  if (action != MGACT_LOAD_GRID && action != MGACT_UNLOAD_GRID && 
    action != MGACT_GRID_DONE && action != MGACT_RESTORE_COND_AP && 
    action != MGACT_RESTORE_C1_AP && action != MGACT_RESTORE_STATE && 
    action != MGACT_REPLACE_OBJ_AP && mScope->GetColumnValvesOpen() < 1)
    mScope->SetColumnValvesOpen(true);

  /*
   * NOW START THE NEXT ACTION
   */
  switch (action) {

    // Start removing objective aperture
  case MGACT_REMOVE_OBJ_AP:
    mScope->SetApertureSize(OBJECTIVE_APERTURE, 0);
    mMovedAperture = true;
    mReinsertObjAp = true;
    break;

    // Start changing the condenser aperture
  case MGACT_SET_CONDENSER_AP:
    ind = (mUseTwoJeolCondAp && !mParams.C1orC2condenserAp) ?
      JEOL_C1_APERTURE : mSingleCondenserAp;
    size = mScope->FindApertureIndexFromSize(ind, mParams.condenserApSize, str);
    mScope->SetApertureSize(ind, size);
    mMovedAperture = true;
    mRestoreCondAp = mUseTwoJeolCondAp ? 2 : 1;
    break;

    // Start putting objective aperture back in
  case MGACT_REPLACE_OBJ_AP:
    mScope->SetApertureSize(OBJECTIVE_APERTURE, mInitialObjApSize);
    mMovedAperture = true;
    mReinsertObjAp = false;
    break;

    // Start getting back to original condenser aperture(s)
    // restore C1 first so a restore value of 1 always means restore condenser
  case MGACT_RESTORE_C1_AP:
    mScope->SetApertureSize(JEOL_C1_APERTURE, mInitialC1CondSize);
    mMovedAperture = true;
    mRestoreCondAp--;
    break;

  case MGACT_RESTORE_COND_AP:
    mScope->SetApertureSize(mSingleCondenserAp, mInitialCondApSize);
    mMovedAperture = true;
    mRestoreCondAp--;
    break;

    // Start unloading grid, allow for error if no grid loaded first time (?)
  case MGACT_UNLOAD_GRID:
    if (!mRealignIteration)
      SEMAppendToLog("Removing loaded grid to take blank reference image");
    if (mUnloadErrorOK < 0)
      mUnloadErrorOK = 1;
    LoadOrUnloadGrid(-1, MG_SEQ_LOAD);
    break;

    // Start loading grid, allow for error if grid is known to be on stage
  case MGACT_LOAD_GRID:
    if (jcd.station == JAL_STATION_STAGE)
      mUnloadErrorOK = 1;
    LoadOrUnloadGrid(mSlotNumsToRun[mCurrentGrid], MG_SEQ_LOAD);
    break;

    // Start moving stage to Z height if known
  case MGACT_SET_ZHEIGHT:
    if (jcd.zStage < -900.) 
      break;
    moveInfo.z = jcd.zStage;
    moveInfo.axisBits = axisZ;
    moveInfo.backZ = mWinApp->mComplexTasks->GetFEBacklashZ();
    mScope->MoveStage(moveInfo, true);
    timeout = 60000;
    addIdleTask = 2;
    break;

    // Start routine to realign to grid map
  case MGACT_REALIGN_TO_LMM:
    if (RealignToGridMap(mJcdIndsToRun[mCurrentGrid], false, true, errStr)) {
      errStr += "\r\nMarking grid as failed due to this failure to realign to map";
      ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
      SaveSessionFileWarnIfError();
      if (SkipToNextGrid(errStr))
        return;
    }
    break;

    // Start routine to refine alignment to grid map
  case MGACT_REFINE_ALIGN:
    if (RefineGridMapAlignment(mJcdIndsToRun[mCurrentGrid], mParams.refineStateNum, true,
      errStr)) {
      errStr += "\r\nMarking grid as failed due to this failure to refine alignment to"
        " map";
      ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
      SaveSessionFileWarnIfError();
      if (SkipToNextGrid(errStr))
        return;
    }
    break;


    // Turn on low dose if off
  case MGACT_LOW_DOSE_ON:
    if (!mWinApp->LowDoseMode())
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    break;

    // Turn off low dose if on
  case MGACT_LOW_DOSE_OFF:
    if (mWinApp->LowDoseMode())
      mWinApp->mLowDoseDlg.SetLowDoseMode(false);
    break;

    // Set state(s) for MM mapping
  case MGACT_LMM_STATE:
    if (CStateDlg::DoSetImState(mParams.LMMstateNum, errStr)) {
      SEMMessageBox("Error trying to set LM state: " + errStr);
      StopMulGridSeq();
      return;
    }
    mStateWasSet = true;
    break;

    // Set state(s) for final acquire
  case MGACT_FINAL_STATE:
    stateNums = &mParams.finalStateNums[0];
    if (jcd.acqStateNums[0] >= 0 || jcd.acqStateNums[1] >= 0 || jcd.acqStateNums[2] >= 0
      || jcd.acqStateNums[3] >= 0)
      stateNums = &jcd.acqStateNums[0];
  case MGACT_MMM_STATE:   // Fall through to states for MMM
    err = 0;
    for (ind = 0; ind < 4; ind++) {
      if (stateNums[ind] >= 0) {
        err = CStateDlg::DoSetImState(stateNums[ind], errStr);
        if (!err)
          mStateWasSet = true;
        else
          break;
      }
    }
    if (err) {
      str.Format("Error trying to set state # %d %s\n", stateNums[ind] + 1,
        action == MGACT_MMM_STATE ? "for medium mag maps:" :
        "for final acquisition");
      SEMMessageBox(str + errStr);
      StopMulGridSeq();
      return;
    }
    break;

  case MGACT_FOCUS_POS:
    if (jcd.focusPosParamIndex >= 0 && jcd.focusPosParamIndex <
      (int)mMGFocusPosParamArray.GetSize()) {
      fpParam = mMGFocusPosParamArray.GetAt(jcd.focusPosParamIndex);
      GridToFocusPosSettings(fpParam);
    }
    break;

    // Restore state
  case MGACT_RESTORE_STATE:
    RestoreState();
    break;

    // Start taking reference or survey image
  case MGACT_REF_IMAGE:
  case MGACT_SURVEY_IMAGE:
    if (!mSurveyIndex && !mWinApp->LowDoseMode())
      mScope->SetFocusToStandardIfLM(mLMMmagIndex);
    mCamera->InitiateCapture(setNum);
    addIdleTask = 2;
    break;

    // Move the stage for ref or survey image, skip move if already there for ref
  case MGACT_SURVEY_STAGE_MOVE:
    if (!mSurveyIndex && mMovedToFirstTrial)
      break;
  case MGACT_REF_MOVE:
    moveInfo.x = mEucenXtrials[mSurveyIndex];
    moveInfo.y = mEucenYtrials[mSurveyIndex];
    moveInfo.axisBits = axisXY;
    mScope->MoveStage(moveInfo);
    timeout = 60000;
    mMovedToFirstTrial = true;
    addIdleTask = 2;
    break;

    // Start eucentricity with the LM mapping parameter set
  case MGACT_EUCENTRICITY:
    mWinApp->mComplexTasks->SetFENextCoarseConSet(setNum);
    mWinApp->mComplexTasks->FindEucentricity(FIND_EUCENTRICITY_COARSE);
    mDoingEucentricity = true;
    break;

    // Start moving to center of montage
  case MGACT_LMM_CENTER_MOVE:
    moveInfo.x = mGridMontParam.fullMontStageX;
    moveInfo.y = mGridMontParam.fullMontStageY;
    moveInfo.axisBits = axisXY;
    mScope->MoveStage(moveInfo);
    timeout = 60000;
    addIdleTask = 2;
    break;

    // Start taking Grid map
  case MGACT_TAKE_LMM:

    // Open new Nav file, i.e. save old, clear out nav, and define new name
    str = FullGridFilePath(mCurrentGrid, ".nav");
    if (UtilFileExists(str)) {
      str2 = FullGridFilePath(mCurrentGrid, "-1.nav");
      NextAutoFilenameIfNeeded(str2);
      UtilRenameFile(str, str2);
      UtilSplitPath(str2, label, str3);
      UtilSplitPath(str, label, str2);
      PrintfToLog("Renamed existing Navigator file %s to %s", (LPCTSTR)str2,
        (LPCTSTR)str3);
    }
    mNavigator->SaveAndClearTable();
    mNavigator->SetCurrentNavFile(str);

    // Prepare to open image file and guarantee an mdoc
    str = FullGridFilePath(mCurrentGrid, "_LMM_1.");
    if (OpenNewMontageFile(mGridMontParam, str))
      return;

    // start it
    SEMAppendToLog("Opened new file for grid map: " + str);
    if (mPctPatchSize < 0) {
      mPctPatchSize = B3DNINT(1.6 * pow((double)mGridMontParam.xFrame * 
        mGridMontParam.yFrame, 0.25));
      B3DCLAMP(mPctPatchSize, 50, 100);
    }
    mWinApp->mMontageController->SetPercentileStatParams(mPctPatchSize, 1., 99., 
      mPctStatMidCrit * mReferenceCounts, mPctStatRangeCrit * mReferenceCounts);
    mWinApp->mMontageController->StartMontage(0, false);
    mDoingMontage = true;
    break;

    // Start autocontouring
  case MGACT_AUTOCONTOUR:
    mNavHelper->mAutoContouringDlg->SyncToMasterParams();
    acParam = mNavHelper->GetAutocontourParams();
    if (mNavHelper->mAutoContouringDlg->IsOpen())
      CopyAutoContGroups();
    if (mHaveAutoContGroups) {
      mNavHelper->mAutoContouringDlg->ExternalSetGroups(acParam->numGroups, 
        acParam->groupByMean, mAutoContGroups, MAX_AUTOCONT_GROUPS);
    }
    ind = mWinApp->mBufferManager->GetBufToReadInto();
    mNavHelper->mAutoContouringDlg->AutoContourImage(&mImBufs[ind], 
      acParam->usePixSize ? acParam->targetPixSizeUm : acParam->targetSizePixels, 
      acParam->minSize, acParam->maxSize, acParam->useAbsThresh ? 0.f : 
      acParam->relThreshold, acParam->useAbsThresh ? acParam->absThreshold : 0.f,
      false);
    mAutoContouring = true;
    break;

    // Run script
  case MGACT_RUN_MACRO:
    mWinApp->mMacroProcessor->Run(mParams.macroToRun);
    mRunningMacro = true;
    break;

    // Shift to marker if necessary and grid startup stuff
  case MGACT_MARKER_SHIFT:

     // Take care of hole, multishot, autocontour parameters if needed
    if (mUsingHoleFinder && jcd.holeFinderParamIndex >= 0 &&
      jcd.holeFinderParamIndex < (int)mMGHoleParamArray.GetSize()) {
      if (!mSavedHoleFinder) {
        mSavedHoleFinder = new MGridHoleFinderParams;
        HoleFinderToGridSettings(*mSavedHoleFinder);
      }
      hfParam = mMGHoleParamArray.GetAt(jcd.holeFinderParamIndex);
      GridToHoleFinderSettings(hfParam);
    }

    if (mUsingMultishot && jcd.multiShotParamIndex >= 0 &&
      jcd.multiShotParamIndex < (int)mMGMShotParamArray.GetSize()) {
      if (!mSavedMultiShot) {
        mSavedMultiShot = new MGridMultiShotParams;
        MultiShotToGridSettings(*mSavedMultiShot);
      }
      msParam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
      GridToMultiShotSettings(msParam);
      if (mParams.msVectorSource)
        PrintfToLog("Using IS vectors from this grid\'s multiple Record settings");
    }

    if (mUsingAutoContour && jcd.autoContParamIndex >= 0 &&
      jcd.autoContParamIndex < (int)mMGAutoContParamArray.GetSize()) {
      if (!mSavedAutoCont) {
        mSavedAutoCont = new MGridAutoContParams;
        AutoContToGridSettings(*mSavedAutoCont);
      }
      acParm = mMGAutoContParamArray.GetAt(jcd.autoContParamIndex);
      GridToAutoContSettings(acParm);
    }

    // Do general params unconditionally
    if (jcd.generalParamIndex >= 0 && jcd.generalParamIndex <
      (int)mMGGeneralParamArray.GetSize()) {
      if (!mSavedGeneralParams) {
        mSavedGeneralParams = new MGridGeneralParams;
        GeneralToGridSettings(*mSavedGeneralParams);
      }
      genParam = mMGGeneralParamArray.GetAt(jcd.generalParamIndex);
      GridToGeneralSettings(genParam);
    }

    // Just save focus position here, set it after state
    if (jcd.focusPosParamIndex >= 0 && jcd.focusPosParamIndex <
      (int)mMGFocusPosParamArray.GetSize() && !mSavedFocusPos) {
      mSavedFocusPos = new MGridFocusPosParams;
      FocusPosToGridSettings(*mSavedFocusPos);
    }

    mRealignIteration = 0;
    err = OpenNavFileIfNeeded(jcd);
    if (err > 1)
      return;
    if (err)
      break;
    item = mNavigator->FindItemWithMapID(jcd.LMmapID);
    if (!item) {
      errStr.Format("Skipping grid %d because of failure to find grid map", jcd.id);
      if (SkipToNextGrid(errStr))
        return;
      break;
    }
    if (!mNoMarkerShifts && (!item->mShiftCohortID || 
      item->mMarkerShiftX < EXTRA_VALUE_TEST)) {

      // It wasn't shifted: find shift and do it
      if (MarkerShiftIndexForMag(item->mMapMagInd, baseShift) < 0) {
        errStr.Format("Skipping grid %d because no Shift to Marker was done on the "
          "grid map yet\r\n  and there is no saved shift for magnification %d", jcd.id,
          MagForCamera(item->mMapCamera, item->mMapMagInd));
        if (SkipToNextGrid(errStr))
          return;
        break;
      }

      mNavigator->ShiftItemsAtRegistration(baseShift.shiftX, baseShift.shiftY,
        item->mRegistration, 1);
      PrintfToLog("Applied marker shift of %.2f, %.2f to Navigator items for grid %d",
        baseShift.shiftX, baseShift.shiftY, jcd.id);
    }
    break;

    // Start setting up MM map file(s)
  case MGACT_SETUP_MMM_FILES:
    err = OpenNavFileIfNeeded(jcd);
    if (err > 1)
      return;
    if (err)
      break;
    itemArr = mNavigator->GetItemArray();

    // Find out if there is a file at first acquire item
    firstInd = -1;
    for (ind = 0; ind < (int)itemArr->GetSize(); ind++) {
      item = itemArr->GetAt(ind);
      if (item->mAcquire) {
        if (firstInd < 0) {
          firstInd = ind;
          label = item->mLabel;
          groupInd = mNavigator->GroupScheduledIndex(item->mGroupID);
          if (item->mFilePropIndex >= 0 || groupInd >= 0)
            fileAtFirst = true;
        }
        lastInd = ind;
      }
    }

    // For polygon montage, start process of setting up files
    fileOpt = *(mWinApp->mDocWnd->GetFileOpt());
    if (mParams.MMMimageType == 1) {
      fileType = fileOpt.separateForMont ? fileOpt.montFileType : fileOpt.fileType;
      str = FullGridFilePath(mCurrentGrid, "_MMM_" + label);
      str += fileType == STORE_TYPE_HDF ? ".hdf" : ".mrc";
      NextAutoFilenameIfNeeded(str);
      mNavHelper->SetFirstMontFilename(str);
      mSettingUpNavFiles = true;
      mNavigator->ToggleNewFileOverRange(firstInd, lastInd, 1);
    } else if (!fileAtFirst) {

      // Otherwise, open a file if nothing already set up (how could it be?)
      str = FullGridFilePath(mCurrentGrid, "_MMM_1.");
      if (mParams.MMMimageType) {
        if (OpenNewMontageFile(mMMMmontParam, str))
          return;
      } else {
        str += fileOpt.fileType == STORE_TYPE_HDF ? "hdf" : "mrc";
        NextAutoFilenameIfNeeded(str);
        if (mWinApp->mDocWnd->DoOpenNewFile(str)) {
          StopMulGridSeq();
          return;
        }
      }
      SEMAppendToLog("Opened new file for medium-mag maps: " + str);
    }
    break;

    // Start taking medium mag maps
  case MGACT_TAKE_MMM:
    mNavHelper->CopyAcqParamsAndActionsToTemp(0);
    acqParams = mWinApp->GetNavAcqParams(2);
    if (acqParams->acquireType == ACQUIRE_MULTISHOT)
      acqParams->useMapHoleVectors = mParams.msVectorSource == 0;
    acqParams->noMBoxOnError = true;
    if (mParams.MMMimageType > 0) {
      acqParams->acquireType = ACQUIRE_TAKE_MAP;
    } else {
      acqParams->mapWithViewSearch = 2 - mParams.MMMstateType;
    }
    mNavigator->SetCurAcqParmActions(2);
    CloseMainLogOpenForGrid("_MMM.log");
    mNavigator->AcquireAreas(NAVACQ_SRC_MG_RUN_MMM, false, true);
    mStartedNavAcquire = true;
    break;

    // Start final acquisition
  case MGACT_FINAL_ACQ:
    err = OpenNavFileIfNeeded(jcd);
    if (err > 1)
      return;
    if (err)
      break;
    if (jcd.finalDataParamIndex >= 0) {
      if (jcd.finalDataParamIndex >= (int)mMGAcqItemsParamArray.GetSize()) {
        str.Format("Skipping grid %d because index of Final Data parameters stored for "
          "it is out of range (%d >= %d)\r\n",
          jcd.finalDataParamIndex, mMGAcqItemsParamArray.GetSize());
        SkipToNextGrid(str);
        break;
      }
      fdParam = mMGAcqItemsParamArray.GetAt(jcd.finalDataParamIndex);
      mNavHelper->CopyAcqParamsAndActionsToTemp(&fdParam.actions[0], &fdParam.params,
        &fdParam.actOrder[0]);
    } else
      mNavHelper->CopyAcqParamsAndActionsToTemp(1);
    acqParams = mWinApp->GetNavAcqParams(2);
    if (acqParams->acquireType == ACQUIRE_MULTISHOT)
      acqParams->useMapHoleVectors = mParams.msVectorSource == 0;
    acqParams->noMBoxOnError = true;
    CloseMainLogOpenForGrid("_final.log");
    mNavigator->SetCurAcqParmActions(2);
    mOpeningForFinalFailed = false;

    // Take care of frames directory
    mLastSetFrameNum = -2;
    if (mCamNumForFrameDir >= 0) {
      mCamera->GetCameraFrameFolder(&camParams[mCamNumForFrameDir], noFrameSubs, movable);
      str = RootnameFromUsername(jcd);
      if (!noFrameSubs) {
        if (!movable || !mParams.framesUnderSession)
          str = mSavedDirForFrames + "\\" + str;
        else if (mUseSubdirs)
          str = mWorkingDir + "\\" + str + "\\frames";
        else
          str = mWorkingDir + "\\" + str + "_frames";
      }
      mCamera->SetCameraFrameFolder(&camParams[mCamNumForFrameDir], str);
      if (!mSavedFrameBaseName.IsEmpty()) {
        str = mSavedFrameBaseName + "_" + jcd.userName;
        mCamera->SetFrameBaseName(str);
      }
      mLastSetFrameNum = mCamera->GetLastUsedFrameNumber();
      if (jcd.lastFrameNumber >= 0) {
        mLastSetFrameNum = jcd.lastFrameNumber;
        mCamera->SetKeepLastUsedFrameNum(true);
        mCamera->SetLastUsedFrameNumber(jcd.lastFrameNumber);
      }
    }

    mNavigator->AcquireAreas(NAVACQ_SRC_MG_RUN_ACQ, false, true);
    if (mOpeningForFinalFailed) {
      StopMulGridSeq();
      return;
    }
    mStartedNavAcquire = true;
    break;

    // Grid done: increment the grid number.  Operations here need to be replicated in the
    // skip function
  case MGACT_GRID_DONE:
    if (mEndWhenGridDone) {
      if (mCurrentGrid == mNumGridsToRun - 1) {
        StopMulGridSeq();
      } else {
        SuspendMulGridSeq();
        mStoppedAtGridEnd = true;
      }
      return;
    }
    FinishWithGrid();
    break;

  default:
    break;
  }

  // Add idle task with specific busy check or generic stage/camera check
  if (addIdleTask == 1) {
    mWinApp->AddIdleTask(TASK_MULGRID_SEQ, MG_RUN_SEQUENCE, timeout);
  } else if (addIdleTask == 2) {
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULGRID_SEQ, MG_RUN_SEQUENCE, timeout);
  }
}

/*
 * Skip to a selected action, used only ot skip ot next grid
 */
int CMultiGridTasks::SkipToAction(int mgAct)
{
  int ind = mSeqIndex;
  for (; ind < (int)mActSequence.size(); ind++) {
    if (mActSequence[ind] == mgAct) {
      mSeqIndex = ind;
      return 0;
    }
  }
  return 1;
}

/*
 * Skip to next grid after printing the errStr in red; stpo if error or end flag set
 */
int CMultiGridTasks::SkipToNextGrid(CString &errStr)
{
  mWinApp->SetNextLogColorStyle(WARNING_COLOR_IND, 0);
  SEMAppendToLog(errStr);
  if (SkipToAction(MGACT_GRID_DONE)) {
    SEMMessageBox("Program error: could not skip to end of actions for current "
      "grid");
    StopMulGridSeq();
    return 1;
  }
  if (mEndWhenGridDone) {
    StopMulGridSeq();
    return 1;
  }

  // Increment the current grid here because the seqindex will always be incremented 
  // before the case is hit
  FinishWithGrid();
  return 0;
}

/*
 * Common tasks when done with grid
 */
void CMultiGridTasks::FinishWithGrid()
{
  double timePerGrid;
  CString str;
  RestoreImposedParams();
  mCurrentGrid++;
  timePerGrid = SEMTickInterval(mRunStartTime) / mCurrentGrid;
  if (mCurrentGrid < mNumGridsToRun) {
    mRemainingTime = timePerGrid * (mNumGridsToRun - 1 - mCurrentGrid);
    str.Format("Estimated time left after current grid (%d/%d): ", mCurrentGrid + 1,
      mNumGridsToRun);
    CTimeSpan ts(B3DNINT(mRemainingTime / 1000.));
    if (ts.GetDays() > 0)
      str += ts.Format(" %D da %H:%M:%S");
    else
      str += ts.Format(" %H:%M:%S");
    mWinApp->mNavigator->ManageListHeader(str);
  }
  OutputGridStartLine();
  UpdateGridStatusText();
}

////////////////////////////////////////////
//  Support routines
////////////////////////////////////////////
/*
 * Set status text with grid #
 */
void CMultiGridTasks::UpdateGridStatusText()
{
  if (mCurrentGrid < 0 || mCurrentGrid >= mNumGridsToRun)
    return;
  JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndsToRun[mCurrentGrid]);
  CString str;
  str.Format("DOING MULTIGRID %d", jcd.id);
  mWinApp->SetStatusText(COMPLEX_PANE, str);
}

/*
 * Do standard separator and output when starting a grid
 */
void CMultiGridTasks::OutputGridStartLine()
{
  if (mCurrentGrid >= mNumGridsToRun)
    return;
  JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndsToRun[mCurrentGrid]);
  mWinApp->SetNextLogColorStyle(0, 1);
  PrintfToLog("-----------------------------------------------------------------"
    "---------------\r\nStarting grid # %d\r\n", jcd.id);
}

/*
 * Return full path to grid file and rootname, optionally appending a suffix
 */
CString CMultiGridTasks::FullGridFilePath(int gridInd, CString suffix)
{
  CString str, str2;
  JeolCartridgeData jcd;
  int jcdInd;
  if (gridInd < 0)
    jcdInd = -(gridInd + 1);
  else
    jcdInd = mJcdIndsToRun[gridInd];
  jcd = mCartInfo->GetAt(jcdInd);
  str = mWorkingDir;
  str2 = RootnameFromUsername(jcd);
  if (mUseSubdirs)
    str += "\\" + str2;
  str += "\\" + str2;
  return str + suffix;
}

/*
 * Determine which grid the current navigator file belongs to
 */
int CMultiGridTasks::GridIndexOfCurrentNavFile()
{
  CString nav, str;
  if (!mWinApp->mNavigator || mSingleGridMode)
    return -1;
  nav = mWinApp->mNavigator->GetCurrentNavFile();
  for (int ind = 0; ind < (int)mCartInfo->GetSize(); ind++) {
    str = FullGridFilePath(-ind - 1, ".nav");
    if (str == nav)
      return ind;
  }
  return -1;
}

/*
 * Calls for next autonumbered file if th given file exists
 */
void CMultiGridTasks::NextAutoFilenameIfNeeded(CString &str)
{
  CFileStatus status;
  while (CFile::GetStatus(str, status))
    str = mNavHelper->NextAutoFilename(str);
}

/*
 * Close the main log and open the grid-specific one
 */
void CMultiGridTasks::CloseMainLogOpenForGrid(const char *suffix)
{
  if (!mSingleGridMode) {
    mWinApp->mLogWindow->DoSave();
    mWinApp->mLogWindow->CloseLog();
    mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
    mWinApp->mLogWindow->UpdateSaveFile(true, FullGridFilePath(mCurrentGrid, suffix));
  }
}

/*
 * Opens the montage file for grid map or MM map
 */
int CMultiGridTasks::OpenNewMontageFile(MontParam &montP, CString &str)
{
  FileOptions fileOpt = *(mWinApp->mDocWnd->GetFileOpt());
  int fileType = fileOpt.separateForMont ? fileOpt.montFileType : fileOpt.fileType;

  // Prepare to open image file and guarantee an mdoc
  fileOpt.TIFFallowed = false;
  if (fileType == STORE_TYPE_HDF) {
    fileOpt.montageInMdoc = true;
    str += "hdf";
  } else {
    fileOpt.montageInMdoc = false;
    fileOpt.montUseMdoc = fileOpt.useMdoc = true;
    fileOpt.typext |= MONTAGE_MASK | VOLT_XY_MASK;
    fileOpt.fileType = fileOpt.montFileType = STORE_TYPE_MRC;
    str += "mrc";
  }
  montP.zCurrent = 0;
  montP.zMax = -1;
  NextAutoFilenameIfNeeded(str);

  // Leave current file (it replaces the mast mont), copy to the master
  mWinApp->mDocWnd->LeaveCurrentFile();
  *(mWinApp->GetMontParam()) = montP;
  mWinApp->mStoreMRC = mWinApp->mDocWnd->OpenNewFileByName(str, &fileOpt);
  if (!mWinApp->mStoreMRC) {
    mWinApp->mDocWnd->RestoreCurrentFile();
    SEMMessageBox("An error occurred attempting to open a new file:\n" + str);
    StopMulGridSeq();
    return 1;
  }

  // set up montaging 
  mWinApp->SetMontaging(true);
  mWinApp->mDocWnd->AddCurrentStore();
  return 0;
}

// If Nav discovers that a file is needed
int CMultiGridTasks::OpenFileForFinalAcq()
{
  FileOptions *fileOpt = mWinApp->mDocWnd->GetFileOpt();
  CString name = FullGridFilePath(mCurrentGrid, "_Final_1.");
  name += fileOpt->fileType == STORE_TYPE_HDF ? "hdf" : "mrc";
  NextAutoFilenameIfNeeded(name);
  if (mWinApp->mDocWnd->DoOpenNewFile(name)) {
    mOpeningForFinalFailed = true;
    return 1;
  }
  SEMAppendToLog("Opened new file for final data: " + name);
  return 0;
}

/*
 * Finds grid map for given item in catalogue, reloading nav file if necessary,
 * and starts realign routine which will reload map if needed
 */
int CMultiGridTasks::RealignToGridMap(int jcdInd, bool askIfSave, bool applyLimits, 
  CString &errStr)
{
  CMapDrawItem *item;
  if (PrepareAlignToGridMap(jcdInd, askIfSave, &item, errStr))
    return 1;
  if (RealignReloadedGrid(item, 0., true, mRRGMaxRotation, applyLimits ? 2 : 1, errStr,
    jcdInd))
    return 1;
  return 0;
}

/*
 * Finds grid map for given item in catalogue, reloading nav file if necessary,
 */
int CMultiGridTasks::PrepareAlignToGridMap(int jcdInd, bool askIfSave, 
  CMapDrawItem **itemP, CString &errStr)
{
  JeolCartridgeData jcd = mCartInfo->GetAt(jcdInd);
  CMapDrawItem *item = NULL;
  CString str;
  mNavigator = mWinApp->mNavigator;
  if (jcd.LMmapID) {
    item = mNavigator->FindItemWithMapID(jcd.LMmapID);

    // Reopen nav file if can't find it in current  one
    if (!item) {
      if (mNavigator->SaveAndClearTable(askIfSave) < 0)
        return -1;
      str = FullGridFilePath(-jcdInd - 1, ".nav");
      if (mNavigator->LoadNavFile(false, false, &str)) {
        errStr.Format("Error reloading Navigator file for grid # %: %s", jcd.id,
          (LPCTSTR)str);
        return 1;
      }
      item = mNavigator->FindItemWithMapID(jcd.LMmapID);
    }
  }
  if (!item) {
    if (!mNavigator->GetNumberOfItems()) {
      errStr = "Cannot find grid map entry; the Navigator table is empty";
      return 1;
    }

    // Fallback to getting first item!
    item = mNavigator->GetOtherNavItem(0);
  }
  *itemP = item;
  return 0;
}

/*
 * Top-level routine for starting refinement of grid map alignment for operations here
 */
int CMultiGridTasks::RefineGridMapAlignment(int jcdInd, int stateNum, bool askIfSave, 
  CString &errStr)
{
  CMapDrawItem *item;
  StateParams *state;
  BOOL inLD = mWinApp->LowDoseMode();
  int ldArea, consNum = RECORD_CONSET;
  if (PrepareAlignToGridMap(jcdInd, askIfSave, &item, errStr))
    return 1;
  if (mParams.refineImageType < 2) {
    consNum = mParams.refineImageType ? VIEW_CONSET : SEARCH_CONSET;
    if (!inLD) {
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);
      mRefineUsedLDorState = 1;
    }

  } else {
    state = mStateArray->GetAt(stateNum);
    ldArea = mNavHelper->AreaFromStateLowDoseValue(state, &consNum);
    if (ldArea >= 0 && ldArea != VIEW_CONSET && ldArea != SEARCH_AREA) {
      errStr = "The state for refining grid map alignment must be View, Search, or "
        "a non Low Dose state";
      return 1;
    }

    if (CStateDlg::DoSetImState(stateNum, errStr)) {
      errStr = "Error trying to set state for refining grid map alignment:\r\n" + errStr;
      return 1;
    }
    mRefineUsedLDorState = -1;
  }

  // Start the routine, with the property value as maximum difference
  if (AlignGridMapAcrossMags(item, consNum, EXTRA_NO_VALUE, EXTRA_NO_VALUE, 
    mMaxRefineShiftDiff, errStr)) {
    RestoreFromRefine();
    return 1;
  }
  return 0;
}

// Load the map for the given item in way appropriate for realign
int CMultiGridTasks::ReloadGridMap(CMapDrawItem *item, int useBin, CString &errStr)
{
  BOOL saveShift, saveRot;
  int ind, err;
  MontParam *masterMont = mWinApp->GetMontParam();
  saveRot = item->mRotOnLoad;
  ind = masterMont->overviewBinning;
  saveShift = masterMont->shiftInOverview;
  item->mRotOnLoad = false;
  masterMont->shiftInOverview = true;
  masterMont->overviewBinning = useBin;
  err = mNavigator->DoLoadMap(true, item, -1, false);
  item->mRotOnLoad = saveRot;
  masterMont->overviewBinning = ind;
  masterMont->shiftInOverview = saveShift;
  return err;
}

// Open nav file for the current grid if it is not the current open file
int CMultiGridTasks::OpenNavFileIfNeeded(JeolCartridgeData &jcd)
{
  CString str = FullGridFilePath(mCurrentGrid, ".nav");
  CString errStr;
  
  if (!str.Compare(mNavigator->GetCurrentNavFile()))
    return 0;
  mNavigator->SaveAndClearTable();
  if (mNavigator->LoadNavFile(false, false, &str)) {
    errStr.Format("Error reloading Navigator file for grid %d; marking grid as"
      " failed", jcd.id);
    ChangeStatusFlag(mCurrentGrid, MGSTAT_FLAG_FAILED, 1);
    if (SkipToNextGrid(errStr))
      return 2;
    return 1;
  }
  return 0;
}

/*
 * Look for a marker shift at the given mag
 */
int CMultiGridTasks::MarkerShiftIndexForMag(int magInd, BaseMarkerShift &baseShift)
{
  int ind;
  CArray<BaseMarkerShift, BaseMarkerShift> *shiftArray =
    mNavHelper->GetMarkerShiftArray();
  for (ind = 0; ind < (int)shiftArray->GetSize(); ind++) {
    baseShift = shiftArray->GetAt(ind);
    if (baseShift.fromMagInd == magInd)
      return ind;
  }
  return -1;
}

/* 
 * Look for marker shift AND View/Search shift offsets for grid map
 */
int CMultiGridTasks::CheckShiftsForLMmode(int magInd)
{
  int retVal = 0, ldArea = -1;
  BaseMarkerShift baseShift;
  StateParams *state;
  double shiftX, shiftY;
  if (MarkerShiftIndexForMag(magInd, baseShift) >= 0)
    retVal = 1;
  if (mParams.LMMstateType < 2) {
    ldArea = mParams.LMMstateType ? VIEW_CONSET : SEARCH_AREA;
    mWinApp->mLowDoseDlg.GetFullViewShift(shiftX, shiftY, ldArea);
    if (shiftX || shiftY)
      retVal += 2;
  } else {

    // The dialog should be open when this check is called, but...
    if (mMGdlg) {
      if (!mMGdlg->GetOrRefindState(mParams.LMMstateNum, mParams.LMMstateName,
        "state for grid map", mMGdlg->m_comboLMMstate))
        return -1;
    } else if (mParams.LMMstateNum < 0 || mParams.LMMstateNum >= mStateArray->GetSize()) {
      AfxMessageBox("The state number for grid mapping is out of range", MB_EXCLAME);
      return -1;
    }
    state = mStateArray->GetAt(mParams.LMMstateNum);
    ldArea = mNavHelper->AreaFromStateLowDoseValue(state, NULL);
    if ((ldArea == VIEW_CONSET || ldArea == SEARCH_AREA) && state->ldShiftOffsetX > 
      -9000.)
      retVal += 2;
  }
  return retVal;
}

/*
 * Restore state and update dialog
 */
void CMultiGridTasks::RestoreState()
{
  mNavHelper->RestoreSavedState();
  if (mNavHelper->mStateDlg) {
    mNavHelper->mStateDlg->Update();
    mNavHelper->mStateDlg->DisableUpdateButton();
    mNavHelper->mStateDlg->SetCamOfSetState(-1);
  }
  mStateWasSet = false;
}

void CMultiGridTasks::RestoreFromRefine()
{
  if (mRefineUsedLDorState > 0)
    mWinApp->mLowDoseDlg.SetLowDoseMode(false);
  else if (mRefineUsedLDorState < 0)
    RestoreState();
  mRefineUsedLDorState = 0;
}

/*
 * Return grid montage param, copying from master the first time if not read in
 */
MontParam *CMultiGridTasks::GetGridMontParam()
{
  if (!mInitializedGridMont) {
    mGridMontParam = *(mWinApp->GetMontParam());
    mWinApp->mDocWnd->InitMontParamsForDialog(&mGridMontParam, 0);
    mInitializedGridMont = true;
  }
  return &mGridMontParam;
}


/*
 * Return MMM montage param, copying from master the first time if not read in
 */
MontParam *CMultiGridTasks::GetMMMmontParam()
{
  if (!mInitializedMMMmont) {
    mMMMmontParam = *(mWinApp->GetMontParam());
    mWinApp->mDocWnd->InitMontParamsForDialog(&mMMMmontParam, 0);
    mInitializedMMMmont = true;
  }
  return &mMMMmontParam;
}

/*
 * Set or clear a bit in the status flags
 */
void CMultiGridTasks::ChangeStatusFlag(int gridInd, b3dUInt32 flag, int state)
{
  int jcdInd = mJcdIndsToRun[gridInd];
  JeolCartridgeData &jcd = mCartInfo->ElementAt(jcdInd);
  setOrClearFlags((b3dUInt32 *)(&jcd.status), flag, state);
  if (mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->SetStatusText(jcdInd);
  mAdocChanged = true;
}

/* 
 * Get the autocontouring groups so they can be set properly
 */
void CMultiGridTasks::CopyAutoContGroups()
{
  int *showGroup = mNavHelper->mAutoContouringDlg->GetShowGroup();
  for (int ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++)
    mAutoContGroups[ind] = showGroup[ind];
  mHaveAutoContGroups = true;
}

/*
 * Macro and function for copying grid multishot settings to main settings
 */
#define COPY_MEMBER(mem) msParam->mem = B3DNINT(mgParam.values[MS_##mem]);
void CMultiGridTasks::GridToMultiShotSettings(MGridMultiShotParams &mgParam,
  MultiShotParams *msParam)
{
  msParam->numShots[0] = B3DNINT(mgParam.values[MS_numShots0]);
  msParam->numShots[1] = B3DNINT(mgParam.values[MS_numShots1]);
  msParam->numHoles[0] = B3DNINT(mgParam.values[MS_numHoles0]);
  msParam->numHoles[1] = B3DNINT(mgParam.values[MS_numHoles1]);
  msParam->spokeRad[0] = mgParam.values[MS_spokeRad0];
  msParam->spokeRad[1] = mgParam.values[MS_spokeRad1];
  COPY_MEMBER(doSecondRing);
  COPY_MEMBER(doCenter);
  COPY_MEMBER(inHoleOrMultiHole);
  COPY_MEMBER(skipCornersOf3x3);
  msParam->doHexArray = mgParam.values[MS_numHexRings] != 0;
  if (msParam->doHexArray)
    COPY_MEMBER(numHexRings);
  msParam->useCustomHoles = false;
  if (msParam->doHexArray) {
    msParam->holeMagIndex[1] = B3DNINT(mgParam.values[MS_holeMagIndex]);
    msParam->origMagOfArray[1] = B3DNINT(mgParam.values[MS_origMagOfArray]);
    msParam->hexISXspacing[0] = mgParam.values[MS_ISXspacing0];
    msParam->hexISYspacing[0] = mgParam.values[MS_ISYspacing0];
    msParam->hexISXspacing[1] = mgParam.values[MS_ISXspacing1];
    msParam->hexISYspacing[1] = mgParam.values[MS_ISYspacing1];
    msParam->hexISXspacing[2] = mgParam.values[MS_ISXspacing2];
    msParam->hexISYspacing[2] = mgParam.values[MS_ISYspacing2];
  } else {
    msParam->holeMagIndex[0] = B3DNINT(mgParam.values[MS_holeMagIndex]);
    msParam->origMagOfArray[0] = B3DNINT(mgParam.values[MS_origMagOfArray]);
    msParam->holeISXspacing[0] = mgParam.values[MS_ISXspacing0];
    msParam->holeISYspacing[0] = mgParam.values[MS_ISYspacing0];
    msParam->holeISXspacing[1] = mgParam.values[MS_ISXspacing1];
    msParam->holeISYspacing[1] = mgParam.values[MS_ISYspacing1];
  }
  msParam->xformFromMag = B3DNINT(mgParam.values[MS_xformFromMag]);
  msParam->xformToMag = B3DNINT(mgParam.values[MS_xformToMag]);
  msParam->adjustingXform.xpx = mgParam.values[MS_xformXpx];
  msParam->adjustingXform.xpy = mgParam.values[MS_xformXpy];
  msParam->adjustingXform.ypx = mgParam.values[MS_xformYpx];
  msParam->adjustingXform.ypy = mgParam.values[MS_xformYpy];
  msParam->xformMinuteTime = B3DNINT(mgParam.values[MS_xformMinuteTime]);
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateSettings();
}
#undef COPY_MEMBER

// Macro-callable version
void CMultiGridTasks::GridToMultiShotSettings(MGridMultiShotParams &mgParam)
{
  GridToMultiShotSettings(mgParam, mNavHelper->GetMultiShotParams());
}

/*
* Macro and function for copying main multishot settings to grid settings
*/
#define COPY_MEMBER(mem) mgParam.values[MS_##mem] = (float)msParam->mem;
void CMultiGridTasks::MultiShotToGridSettings(MGridMultiShotParams &mgParam)
{
  MultiShotParams *msParam = mNavHelper->GetMultiShotParams();
  mNavHelper->UpdateMultishotIfOpen();
  mgParam.values[MS_spokeRad0] = msParam->spokeRad[0];
  mgParam.values[MS_spokeRad1] = msParam->spokeRad[1];
  mgParam.values[MS_numShots0] = (float)msParam->numShots[0];
  mgParam.values[MS_numShots1] = (float)msParam->numShots[1];
  mgParam.values[MS_numHoles0] = (float)msParam->numHoles[0];
  mgParam.values[MS_numHoles1] = (float)msParam->numHoles[1];
  COPY_MEMBER(doSecondRing);
  COPY_MEMBER(doCenter);
  COPY_MEMBER(inHoleOrMultiHole);
  COPY_MEMBER(skipCornersOf3x3);
  mgParam.values[MS_numHexRings] = (float)
    (msParam->doHexArray ? msParam->numHexRings : 0);
  mgParam.values[MS_origMagOfArray] = (float)
    msParam->origMagOfArray[msParam->doHexArray ? 1 : 0];
  mgParam.values[MS_holeMagIndex] = (float)
    msParam->holeMagIndex[msParam->doHexArray ? 1 : 0];
  mgParam.values[MS_ISXspacing0] = (float)(msParam->doHexArray ?
    msParam->hexISXspacing[0] : msParam->holeISXspacing[0]);
  mgParam.values[MS_ISYspacing0] = (float)(msParam->doHexArray ?
    msParam->hexISYspacing[0] : msParam->holeISYspacing[0]);
  mgParam.values[MS_ISXspacing1] = (float)(msParam->doHexArray ?
    msParam->hexISXspacing[1] : msParam->holeISXspacing[1]);
  mgParam.values[MS_ISYspacing1] = (float)(msParam->doHexArray ?
    msParam->hexISYspacing[1] : msParam->holeISYspacing[1]);
  mgParam.values[MS_ISXspacing2] = (float)(msParam->doHexArray ?
    msParam->hexISXspacing[2] : 0.);
  mgParam.values[MS_ISYspacing2] = (float)(msParam->doHexArray ?
    msParam->hexISYspacing[2] : 0.);
  mgParam.values[MS_xformFromMag] = (float)msParam->xformFromMag;
  mgParam.values[MS_xformToMag] = (float)msParam->xformToMag;
  mgParam.values[MS_xformXpx] = msParam->adjustingXform.xpx;
  mgParam.values[MS_xformXpy] = msParam->adjustingXform.xpy;
  mgParam.values[MS_xformYpx] = msParam->adjustingXform.ypx;
  mgParam.values[MS_xformYpy] = msParam->adjustingXform.ypy;
  mgParam.values[MS_xformMinuteTime] = (float)msParam->xformMinuteTime;
}
#undef COPY_MEMBER

/*
 * Macro and function for copying grid hole finder settings to main settings
 */
#define COPY_MEMBER(mem) hfParam->mem = mgParam.values[HF_##mem];
void CMultiGridTasks::GridToHoleFinderSettings(MGridHoleFinderParams &mgParam)
{
  HoleFinderParams *hfParam = mNavHelper->GetHoleFinderParams();
  hfParam->thresholds = mgParam.thresholds;
  hfParam->sigmas = mgParam.sigmas;
  COPY_MEMBER(lowerMeanCutoff);
  COPY_MEMBER(upperMeanCutoff);
  COPY_MEMBER(SDcutoff);
  COPY_MEMBER(blackFracCutoff);
  COPY_MEMBER(edgeDistCutoff);
  hfParam->hexagonalArray = mgParam.values[HF_hexagonalArray] != 0;
  if (hfParam->hexagonalArray) {
    hfParam->hexSpacing = mgParam.values[HF_spacing];
    hfParam->hexDiameter = mgParam.values[HF_diameter];
  } else {
    COPY_MEMBER(spacing);
    COPY_MEMBER(diameter);
  }
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->UpdateSettings();
}
#undef COPY_MEMBER

/*
 * Macro and function for copying main hole finder settings to grid settings 
 */
#define COPY_MEMBER(mem) mgParam.values[HF_##mem] = hfParam->mem;
void CMultiGridTasks::HoleFinderToGridSettings(MGridHoleFinderParams &mgParam)
{
  HoleFinderParams *hfParam = mNavHelper->GetHoleFinderParams();
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->SyncToMasterParams();
  mgParam.thresholds = hfParam->thresholds;
  mgParam.sigmas = hfParam->sigmas;
  COPY_MEMBER(lowerMeanCutoff);
  COPY_MEMBER(upperMeanCutoff);
  COPY_MEMBER(SDcutoff);
  COPY_MEMBER(blackFracCutoff);
  COPY_MEMBER(edgeDistCutoff);
  mgParam.values[HF_hexagonalArray] = hfParam->hexagonalArray ? 1.f : 0.f;
  mgParam.values[HF_spacing] = hfParam->hexagonalArray ? hfParam->hexSpacing : 
    hfParam->spacing;
  mgParam.values[HF_diameter] = hfParam->hexagonalArray ? hfParam->hexDiameter :
    hfParam->diameter;
}
#undef COPY_MEMBER

/*
 * Macro and function for copying grid autocontouring settings to main settings
 */
#define COPY_MEMBER(mem) acParam->mem = mgParam.values[AC_##mem];
void CMultiGridTasks::GridToAutoContSettings(MGridAutoContParams &mgParam)
{
  AutoContourParams *acParam = mWinApp->mNavHelper->GetAutocontourParams();
  COPY_MEMBER(minSize);
  COPY_MEMBER(maxSize);
  COPY_MEMBER(relThreshold);
  COPY_MEMBER(lowerMeanCutoff);
  COPY_MEMBER(upperMeanCutoff);
  COPY_MEMBER(borderDistCutoff);
  if (mNavHelper->mAutoContouringDlg->IsOpen())
    mNavHelper->mAutoContouringDlg->UpdateSettings();
}
#undef COPY_MEMBER

/*
 * Macro and function for copying main autocontouring settings to grid settings
 */
#define COPY_MEMBER(mem) mgParam.values[AC_##mem] = acParam->mem;
void CMultiGridTasks::AutoContToGridSettings(MGridAutoContParams &mgParam)
{
  AutoContourParams *acParam = mWinApp->mNavHelper->GetAutocontourParams();
  if (mNavHelper->mAutoContouringDlg->IsOpen())
    mNavHelper->mAutoContouringDlg->SyncToMasterParams();
  COPY_MEMBER(minSize);
  COPY_MEMBER(maxSize);
  COPY_MEMBER(relThreshold);
  COPY_MEMBER(lowerMeanCutoff);
  COPY_MEMBER(upperMeanCutoff);
  COPY_MEMBER(borderDistCutoff);
}
#undef COPY_MEMBER

/*
 * Functions for copying general settings back and forth
 */
void CMultiGridTasks::GridToGeneralSettings(MGridGeneralParams &mgParam)
{
  mShiftManager->SetDisableAutoTrim(mgParam.values[GP_disableAutoTrim]);
  mShiftManager->SetErasePeriodicPeaks(mgParam.values[GP_erasePeriodicPeaks]);
  mNavHelper->SetRIErasePeriodicPeaks(mgParam.values[GP_RIErasePeriodicPeaks]);
  mWinApp->mAlignFocusWindow.UpdateSettings();
}

void CMultiGridTasks::GeneralToGridSettings(MGridGeneralParams &mgParam)
{
  mgParam.values[GP_disableAutoTrim] = mShiftManager->GetDisableAutoTrim();
  mgParam.values[GP_erasePeriodicPeaks] = mShiftManager->GetErasePeriodicPeaks();
  mgParam.values[GP_RIErasePeriodicPeaks] = mNavHelper->GetRIErasePeriodicPeaks();
}

/*
* Functions for copying focus position back and forth
*/
void CMultiGridTasks::GridToFocusPosSettings(MGridFocusPosParams &fpParam)
{
  mNavHelper->SetLDFocusPosition(mWinApp->GetCurrentCamera(),
    fpParam.values[FP_focusAxisPos], B3DNINT(fpParam.values[FP_rotateAxis]),
    B3DNINT(fpParam.values[FP_axisRotation]), B3DNINT(fpParam.values[FP_focusXoffset]),
    B3DNINT(fpParam.values[FP_focusYoffset]), "grid settings", false);
}

void CMultiGridTasks::FocusPosToGridSettings(MGridFocusPosParams &fpParam)
{
  int rotateAxis, axisRot, xOffset, yOffset;
  mNavHelper->SaveLDFocusPosition(1, fpParam.values[FP_focusAxisPos], rotateAxis, axisRot,
    xOffset, yOffset, BOOL_EQUIV(GetDebugOutput('q'), true));
  fpParam.values[FP_rotateAxis] = (float)rotateAxis;
  fpParam.values[FP_axisRotation] = (float)axisRot;
  fpParam.values[FP_focusXoffset] = (float)xOffset;
  fpParam.values[FP_focusYoffset] = (float)yOffset;
}

void CMultiGridTasks::TransformStoredVectors(MGridMultiShotParams &mgParam, ScaleMat mat)
{
  ApplyScaleMatrix(mat, mgParam.values[MS_ISXspacing0], mgParam.values[MS_ISYspacing0],
    mgParam.values[MS_ISXspacing0], mgParam.values[MS_ISYspacing0]);
  ApplyScaleMatrix(mat, mgParam.values[MS_ISXspacing1], mgParam.values[MS_ISYspacing1],
    mgParam.values[MS_ISXspacing1], mgParam.values[MS_ISYspacing1]);
  ApplyScaleMatrix(mat, mgParam.values[MS_ISXspacing2], mgParam.values[MS_ISYspacing2],
    mgParam.values[MS_ISXspacing2], mgParam.values[MS_ISYspacing2]);
}

/*
 * What Chen really wanted: apply just the vectors when loading a Nav file and settings
 * are stored for the grid
 */
void CMultiGridTasks::ApplyVectorsIfSavedAndOK(int jcdInd)
{
  JeolCartridgeData jcd = mCartInfo->ElementAt(jcdInd);
  MGridMultiShotParams mgParam;
  MultiShotParams tempParam;
  MultiShotParams *master = mNavHelper->GetMultiShotParams();
  int hexInd, dir;
  if (jcd.multiShotParamIndex < 0)
    return;
  mgParam = mMGMShotParamArray.GetAt(jcd.multiShotParamIndex);
  if (!mgParam.values[MS_holeMagIndex])
    return;
  mNavHelper->UpdateMultishotIfOpen();
  GridToMultiShotSettings(mgParam, &tempParam);
  hexInd = tempParam.doHexArray ? 1 : 0;
  if (mNavHelper->ConfirmReplacingShiftVectors(4, hexInd))
    return;
  double *xOut = hexInd ? &master->hexISXspacing[0] : &master->holeISXspacing[0];
  double *yOut = hexInd ? &master->hexISYspacing[0] : &master->holeISYspacing[0];
  double *xIn = hexInd ? &tempParam.hexISXspacing[0] : &tempParam.holeISXspacing[0];
  double *yIn = hexInd ? &tempParam.hexISYspacing[0] : &tempParam.holeISYspacing[0];
  for (dir = 0; dir < 2 + hexInd; dir++) {
    xOut[dir] = xIn[dir];
    yOut[dir] = yIn[dir];
  }
  master->origMagOfArray[hexInd] = tempParam.origMagOfArray[hexInd];
  master->holeMagIndex[hexInd] = tempParam.holeMagIndex[hexInd];
  master->doHexArray = hexInd > 0;
  master->canUndoRectOrHex = 0;
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->UpdateSettings();
  if (mWinApp->mNavigator) {
    mWinApp->mNavigator->Redraw();
    mWinApp->mNavigator->SetGridIndexOfMap(jcdInd);
  }
}

/*
 * Loads all of the grid maps from the session, or if there is no session or no LM maps
 * done yet, tries to load them from the current nav file, with startBuf as preferred
 * starting buffer if possible, or -1 to start past the read buffer
 */
int CMultiGridTasks::LoadAllGridMaps(int startBuf, CString &errStr)
{
  int ind, jcdInd, numMaps = 0, numLoaded = 0, lowestMag = 999, saveRolls;
  float pixel, biggestMap, size;
  float minGridMapSize = 400.;
  IntVec mapOrJcdInds;
  CString str;
  std::string sstr;
  JeolCartridgeData jcd;
  MapItemArray *itemArray;
  CMapDrawItem *item;
  int maxLoad = MAX_BUFFERS - 2;
  mNavigator = mWinApp->mNavigator;

  // If there is grid info AND there has been any mapping, count up maps from flags/IDs
  if (mCartInfo->GetSize() && mLMMmagIndex > 0) {
    if (!mNavigator)
      mWinApp->mMenuTargets.OpenNavigatorIfClosed();
    mNavigator = mWinApp->mNavigator;
    for (jcdInd = 0; jcdInd < (int)mCartInfo->GetSize(); jcdInd++) {
      jcd = mCartInfo->GetAt(jcdInd);
      if ((jcd.status & MGSTAT_FLAG_LM_MAPPED) && jcd.LMmapID > 0) {
        numMaps++;
        mapOrJcdInds.push_back(jcdInd);
      }
    }
    if (!numMaps) {
      errStr = "No grids in the session are marked as having a grid map";
      return 2;
    }

  } else {

    // Otherwise require nav open, look for the lowest mag maps
    if (!mNavigator) {
      errStr = "The Navigator must be open to look for multiple maps in a Navigator "
        "table";
      return 1;
    }
    itemArray = mNavigator->GetItemArray();
    for (ind = 0; ind < (int)itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->IsMap()) {
        if (item->mMapMagInd < lowestMag) {
          lowestMag = item->mMapMagInd;
          biggestMap = 0.;
          numMaps = 0;
          mapOrJcdInds.clear();
        }

        // At the lowest mag, keep track of biggest size
        if (item->mMapMagInd == lowestMag) {
          pixel = (float)(mShiftManager->GetPixelSize(item->mMapCamera, lowestMag) *
            item->mMapBinning);
          size = (float)(pixel * (item->mMapWidth + item->mMapHeight) / 2.);
          ACCUM_MAX(biggestMap, size);
          numMaps++;
          mapOrJcdInds.push_back(ind);
        }
      }
    }
    if (!numMaps) {
      errStr = "No maps were found in the Navigator table";
      return 2;
    }
    if (lowestMag >= mScope->GetLowestNonLMmag() || biggestMap < minGridMapSize) {
      errStr = "No LM maps covering a large enough area were found in the Navigator "
        "table";
      return 3;
    }
  }

  // Set up where it loads and make sure rolling doesn't occur there while loading
  if (startBuf < 0)
    startBuf = mWinApp->mBufferManager->GetBufToReadInto() + 1;
  ACCUM_MIN(numMaps, maxLoad);
  B3DCLAMP(startBuf, 2, maxLoad - numMaps);
  saveRolls = mWinApp->mBufferManager->GetShiftsOnAcquire();
  mWinApp->mBufferManager->SetShiftsOnAcquire(B3DMIN(saveRolls, startBuf - 1));
  mMultiLoadLabelMap.clear();

  // Loop on maps to load
  for (jcdInd = 0; jcdInd < (int)mapOrJcdInds.size(); jcdInd++) {
    if (mCartInfo->GetSize() && mLMMmagIndex > 0) {

      // For session map, need to load nav, find map
      jcd = mCartInfo->GetAt(jcdInd);
      str = FullGridFilePath(-jcdInd - 1, ".nav");
      if (mNavigator->LoadNavFile(false, false, &str)) {
        SEMAppendToLog("Error loading Navigator file " + str);
        continue;
      }

      item = mNavigator->FindItemWithMapID(jcd.LMmapID);
      if (!item) {
        PrintfToLog("Could not find map with ID %d in file %s", item->mMapID,
          (LPCTSTR)str);
        continue;
      }
    } else {

      // Otherwise get the item
      item = itemArray->GetAt(jcdInd);
    }

    // Load and report
    if (mNavigator->DoLoadMap(true, item, startBuf + numLoaded, false)) {
      PrintfToLog("Error loading map with label %s from file %s", (LPCTSTR)item->mLabel,
        (LPCTSTR)str);
      continue;
    }
    sstr = (LPCTSTR)(item->mLabel + "  -  " + item->mNote);
    mMultiLoadLabelMap.insert(std::pair<int, std::string>(item->mMapID, sstr));
    PrintfToLog("%c: %s  -  %s", (char)('A' + startBuf + numLoaded), 
      (LPCTSTR)item->mLabel, (LPCTSTR)item->mNote);
    numLoaded++;
  }
  mWinApp->SetCurrentBuffer(startBuf);
  mWinApp->mBufferManager->SetShiftsOnAcquire(saveRolls);
  return 0;
}

bool CMultiGridTasks::GetGridMapLabel(int mapID, CString &value)
{
  std::map<int, std::string>::iterator iter;
  if (!mMultiLoadLabelMap.count(mapID))
    return false;
  iter = mMultiLoadLabelMap.find(mapID);
  value = iter->second.c_str();
  return !value.IsEmpty();
}

int CMultiGridTasks::GetCurrentGridID()
{
  JeolCartridgeData jcd;
  if (!(mDoingMulGridSeq || mSuspendedMulGrid) || mCurrentGrid < 0 || 
    mCurrentGrid >= mNumGridsToRun)
    return -1;
  jcd = mCartInfo->GetAt(mJcdIndsToRun[mCurrentGrid]);
  return jcd.id;
}

//////////////////////////////////////////////////////////////////
// SAVING AND READING THE SESSION FILE
//////////////////////////////////////////////////////////////////

// Names and macros
#define MGDOC_APPEND "AppendNames"
#define MGDOC_PREFIX "NamePrefix"
#define MGDOC_TOPDIR "WorkingDir"
#define MGDOC_LOCKED "NamesLocked"
#define MGDOC_SUBDIRS "UseSubDirs"
#define MGDOC_GRID "Grid"
#define MGDOC_ID  "ID"
#define MGDOC_NAME  "Name"
#define MGDOC_STATION  "Station"
#define MGDOC_SLOT  "Slot"
#define MGDOC_TYPE  "Type"
#define MGDOC_ROTATION  "Rotation"
#define MGDOC_ZSTAGE  "Zstage"
#define MGDOC_USERNAME  "UserName"
#define MGDOC_STATUS  "Status"
#define MGDOC_REFCOUNT  "ReferenceCounts"
#define MGDOC_LMMAPID  "LMmapID"
#define MGDOC_LMMINLD  "LMneedsLD"
#define MGDOC_LMM_STNUM  "LMMstateNum"
#define MGDOC_LMM_STNAME  "LMMstateName"
#define MGDOC_CONT_GROUP  "AutoContGroups"
#define MGDOC_LMM_MAGIND  "LMMmagIndex"
#define MGDOC_SKIP_MARKER "SkipMarkerShift"
#define MGDOC_HOLE_PARAM  "HoleFinderParams"
#define MGDOC_HOLE_THRESH  "HoleThresholds"
#define MGDOC_HOLE_FILTER  "HoleFilters"
#define MGDOC_MS_PARAM  "MultiShotParams"
#define MGDOC_AC_PARAM  "AutoContourParams"
#define MGDOC_GEN_PARAM  "GeneralParams"
#define MGDOC_FINAL_PARAM "FinalDataParamInd"
#define MGDOC_ACQ_ST_NUM  "AcqStateNum"
#define MGDOC_ACQ_ST_NAME  "AcqStateName"
#define MGDOC_SEP_ACQ_ST  "SeparateAcqState"
#define MGDOC_IS_ALIGNED  "LoadedIsAligned"
#define MGDOC_FRAME_NUM   "LastFrameNumber"
#define MGDOC_FP_PARAM  "FocusPosParams"
#define MGDOC_NOTE "Note"
#define MGDOC_ORDER "RunOrder"
#define MGDOC_LMM_CONSXY "LMMConsetSizes"
#define MGDOC_MMM_CONSXY "MMMConsetSizes"

#define MGNAV_ACQ_SUFFIX "_navAcq.txt"

#define ADOC_PUT(a) err += AdocSet##a;
#define ADOC_ARG "MontParam",sectInd
#define NAV_MONT_PARAMS
#define BOOL_SETT_ASSIGN(a, b) ADOC_PUT(Integer(ADOC_ARG, a, b ? 1 : 0)); 
#define INT_SETT_ASSIGN(a, b) ADOC_PUT(Integer(ADOC_ARG, a, b));
#define FLOAT_SETT_ASSIGN(a, b) ADOC_PUT(Float(ADOC_ARG, a, b));

#define WRITE_GRID_PARAMS(namel, arr, BREV, brev, typ) \
     if (jcd.namel##ParamIndex >= 0 &&   \
      jcd.namel##ParamIndex < mMG##arr##ParamArray.GetSize()) {    \
       brev##Param = mMG##arr##ParamArray.GetAt(jcd.namel##ParamIndex);    \
       if (AdocSet##typ##Array(MGDOC_GRID, sectInd, MGDOC_##BREV##_PARAM,  \
         brev##Param.values, BREV##_noParam))  \
         err++;   \
     }

/*
 * Save session information to current or new file
 */
int CMultiGridTasks::SaveSessionFile(CString &errStr)
{
  CString date, time, str, dir, name;
  int adocInd, grid, mont, ind, sectInd, err = 0;
  JeolCartridgeData jcd;
  MontParam *montParam = &mGridMontParam;
  char keyBuf[32];
  MGridHoleFinderParams hfParam;
  MGridAutoContParams acParam;
  MGridMultiShotParams msParam;
  MGridGeneralParams genParam;
  MGridAcqItemsParams acqParams;
  MGridFocusPosParams fpParam;

  // Get current parameters if names have not been locked
  if (!mNamesLocked) {
    if (mNavHelper->mMultiGridDlg)
      mNavHelper->mMultiGridDlg->SyncToMasterParams();
    mAppendNames = mParams.appendNames;
    mUseSubdirs = mParams.useSubdirectory;
    mWorkingDir = mWinApp->mDocWnd->GetInitialDir();
  }

  // Make up a unique filename if none
  if (mSessionFilename.IsEmpty()) {
    mWinApp->mDocWnd->DateTimeComponents(date, time, false, false);
    mSessionFilename = mWorkingDir + "\\multiGrid_" + date + "_1.adoc";
    NextAutoFilenameIfNeeded(mSessionFilename);
    mBackedUpSessionFile = true;
    mBackedUpAcqItemsFile = true;
  } else {

    // Otherwise check if it has moved because of new directory being chosen
    UtilSplitPath(mSessionFilename, dir, name);
    dir.TrimRight("/\\");
    mWorkingDir.TrimRight("/\\");
    if (dir.CompareNoCase(mWorkingDir)) {
      if (UtilRenameFile(mSessionFilename, mWorkingDir + "\\" + name, &date, true))
        SEMAppendToLog(date + "; new file will be written in new location");

      // Manage acquire param file too
      UtilSplitExtension(mSessionFilename, str, time);
      str += MGNAV_ACQ_SUFFIX;
      mSessionFilename = mWorkingDir + "\\" + name;
      if (UtilFileExists(str)) {
        UtilSplitExtension(mSessionFilename, name, time);
        name += MGNAV_ACQ_SUFFIX;
        if (UtilRenameFile(str, name, &date, true))
          SEMAppendToLog(date + "; new file will be written in new location");
      }
    }
  }
  mWinApp->mDocWnd->ManageBackupFile(mSessionFilename, mBackedUpSessionFile);
  UpdateDialogForSessionFile();

  // Write any items in the final data acquire param array
  if (mMGAcqItemsParamArray.GetSize() > 0) {
    UtilSplitExtension(mSessionFilename, str, time);
    str += MGNAV_ACQ_SUFFIX;
    if (UtilFileExists(str))
      mWinApp->mDocWnd->ManageBackupFile(str, mBackedUpAcqItemsFile);
    if (mWinApp->mParamIO->WriteMulGridAcqParams(str, errStr))
      return 1;
  }

  // Get the autodoc
  if (!AdocAcquireMutex()) {
    errStr = "Failed to acquire autodoc mutex for saving session file";
    return 1;
  }
  adocInd = AdocNew();
  if (adocInd < 0) {
    errStr = "Failed to create new autodoc structure";
    err = 1;
  }

  // Save global values
  if (!err) {
    if (!mPrefix.IsEmpty())
      err += AdocSetKeyValue(ADOC_GLOBAL_NAME, 0, MGDOC_PREFIX, (LPCTSTR)mPrefix);
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_APPEND, mAppendNames ? 1 : 0);
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LOCKED, mNamesLocked);
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_SUBDIRS, mUseSubdirs ? 1 : 0);
    if (mReferenceCounts > 0.)
      err += AdocSetFloat(ADOC_GLOBAL_NAME, 0, MGDOC_REFCOUNT, mReferenceCounts);
    if (!mWorkingDir.IsEmpty())
      err += AdocSetKeyValue(ADOC_GLOBAL_NAME, 0, MGDOC_TOPDIR, (LPCTSTR)mWorkingDir);
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMMINLD, mLMMneedsLowDose);
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_SKIP_MARKER, mSkipMarkerShifts);
    if (mLMMusedStateNum >= 0) {
      err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_STNUM, mLMMusedStateNum);
      err += AdocSetKeyValue(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_STNAME,
        (LPCTSTR)mLMMusedStateName);
    }
    err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_IS_ALIGNED, mLoadedGridIsAligned);
    if (mHaveAutoContGroups)
      err += AdocSetIntegerArray(ADOC_GLOBAL_NAME, 0, MGDOC_CONT_GROUP, mAutoContGroups, 
        MAX_AUTOCONT_GROUPS);
    if (mLMMmagIndex)
      err += AdocSetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_MAGIND, mLMMmagIndex);
    if (mUseCustomOrder)
      err += AdocSetIntegerArray(ADOC_GLOBAL_NAME, 0, MGDOC_ORDER, &mCustomRunDlgInds[0],
        (int)mCustomRunDlgInds.size());
    if (mInitializedGridMont)
      err += AdocSetTwoIntegers(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_CONSXY,
        mMontSetupConsetSizes[0], mMontSetupConsetSizes[1]);
    if (mInitializedMMMmont)
      err += AdocSetTwoIntegers(ADOC_GLOBAL_NAME, 0, MGDOC_MMM_CONSXY,
        mMontSetupConsetSizes[2], mMontSetupConsetSizes[3]);
  }

  // Save grid values
  for (grid = 0; grid < (int)mCartInfo->GetSize() && !err; grid++) {
    date.Format("%d", grid);
    sectInd = AdocAddSection(MGDOC_GRID, date);
    if (sectInd < 0) {
      errStr = "Error adding autodoc section for grid";
    } else {
      jcd = mCartInfo->GetAt(grid);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_ID, jcd.id);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_STATION, jcd.station);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_SLOT, jcd.slot);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_ROTATION, jcd.rotation);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_TYPE, jcd.type);
      err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_STATUS, jcd.status);
      err += AdocSetFloat(MGDOC_GRID, sectInd, MGDOC_ZSTAGE, jcd.zStage);
      err += AdocSetKeyValue(MGDOC_GRID, sectInd, MGDOC_NAME, (LPCTSTR)jcd.name);
      err += AdocSetKeyValue(MGDOC_GRID, sectInd, MGDOC_USERNAME, (LPCTSTR)jcd.userName);
      err += AdocSetKeyValue(MGDOC_GRID, sectInd, MGDOC_NOTE, (LPCTSTR)jcd.note);
      if (jcd.LMmapID)
        err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_LMMAPID, jcd.LMmapID);
      if (jcd.finalDataParamIndex >= 0)
        err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_FINAL_PARAM,
          jcd.finalDataParamIndex);
      if (jcd.lastFrameNumber >= 0)
        err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_FRAME_NUM, jcd.lastFrameNumber);

      // Write states
      if (jcd.separateState) {
        err += AdocSetInteger(MGDOC_GRID, sectInd, MGDOC_SEP_ACQ_ST, jcd.separateState);
        for (ind = 0; ind < 4; ind++) {
          if (jcd.acqStateNums[ind] >= 0) {
            snprintf(keyBuf, 31, "%s%d", MGDOC_ACQ_ST_NUM, ind);
            err += AdocSetInteger(MGDOC_GRID, sectInd, keyBuf, jcd.acqStateNums[ind]);
          }
          if (!jcd.acqStateNames[ind].IsEmpty()) {
            snprintf(keyBuf, 31, "%s%d", MGDOC_ACQ_ST_NAME, ind);
            err += AdocSetKeyValue(MGDOC_GRID, sectInd, keyBuf,
              (LPCTSTR)jcd.acqStateNames[ind]);
          }
        }
      }

      // Write hole finder params
      if (jcd.holeFinderParamIndex >= 0 &&
        jcd.holeFinderParamIndex < mMGHoleParamArray.GetSize()) {
        hfParam = mMGHoleParamArray.GetAt(jcd.holeFinderParamIndex);
        if (AdocSetFloatArray(MGDOC_GRID, sectInd, MGDOC_HOLE_PARAM, hfParam.values,
          HF_noParam))
          err++;
        if (hfParam.thresholds.size() > 0 && AdocSetFloatArray(MGDOC_GRID, sectInd,
          MGDOC_HOLE_THRESH, &hfParam.thresholds[0],(int) hfParam.thresholds.size()))
          err++;
        if (hfParam.sigmas.size() > 0 && AdocSetFloatArray(MGDOC_GRID, sectInd,
          MGDOC_HOLE_FILTER, &hfParam.sigmas[0], (int)hfParam.sigmas.size()))
          err++;
      }

      // Get multishot, autocontour, general, focusPos params
      WRITE_GRID_PARAMS(multiShot, MShot, MS, ms, Float);
      WRITE_GRID_PARAMS(autoCont, AutoCont, AC, ac, Float);
      WRITE_GRID_PARAMS(general, General, GEN, gen, Integer);
      WRITE_GRID_PARAMS(focusPos, FocusPos, FP, fp, Float);
      
      if (err) {
        errStr = "Errors writing data to a grid section of the autodoc";
      }
    }
  }

  // Save montage params if they have been initialized
  for (mont = 0; mont < 2 && !err; mont++) {
    if ((mont ? mInitializedMMMmont : mInitializedGridMont)) {
      date.Format("%d", mont);
      sectInd = AdocAddSection("MontParam", date);
      if (sectInd < 0) {
        err++;
        errStr = "Error creating new section for MontParam in session autodoc";
      } else {
#include "NavAdocParams.h"
      }
      err += AdocSetFloat("MontParam", mont, "MinUmOverlap", 
        montParam->minMicronsOverlap);
      err += AdocSetFloat("MontParam", mont, "MinOverlapFac", 
        montParam->minOverlapFactor);
      if (err)
        errStr = "Error writing MontParam section of session autodoc";
    }
    montParam = &mMMMmontParam;
  }
  if (!err) {
    err += AdocWrite((LPCTSTR)mSessionFilename);
    if (err)
      errStr = "Error writing autodoc to file: " + mSessionFilename;
  }
  AdocClear(adocInd);
  AdocReleaseMutex();
  if (!err)
    mAdocChanged = false;
  return err;
}

/*
 * Saves and gives a standard warning if there is an error
 */
void CMultiGridTasks::SaveSessionFileWarnIfError()
{
  CString errStr;
  if (SaveSessionFile(errStr))
    SEMAppendToLog("WARNING: Could not write session file - " + errStr);
  if (mCurrentGrid >= 0 && mCurrentGrid < (int)mJcdIndsToRun.size() &&
    mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->SetStatusText(mJcdIndsToRun[mCurrentGrid]);
}
#undef NAV_MONT_PARAMS
#undef ADOC_ARG
#undef BOOL_SETT_ASSIGN
#undef INT_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN

/*
 * Clear out everything that might be read in from a file and other items for fresh start
 */
void CMultiGridTasks::ClearSession(bool keepAcqParams)
{
  mCartInfo->RemoveAll();
  InitOrClearSessionValues();
  mPrefix = "";
  mWorkingDir = "";
  mNamesLocked = 0;
  mUseCustomOrder = false;
  mLastNumSlots = 0;
  mLMMusedStateName = "";
  mSessionFilename = "";
  mMGAutoContParamArray.RemoveAll();
  mMGHoleParamArray.RemoveAll();
  mMGMShotParamArray.RemoveAll();
  mMGGeneralParamArray.RemoveAll();
  mMGFocusPosParamArray.RemoveAll();
  for (int i = 0; i < 4; i++)
    mMontSetupConsetSizes[i] = 0;
  mInitializedGridMont = false;
  mInitializedMMMmont = false;
  if (!keepAcqParams)
    mMGAcqItemsParamArray.RemoveAll();
  if (mNavHelper->mMultiGridDlg) {
    mNavHelper->mMultiGridDlg->m_strPrefix = "";
    mNavHelper->mMultiGridDlg->SetRootname();
    mNavHelper->mMultiGridDlg->UpdateData(false);
    if (!mScope->GetScopeHasAutoloader()) {
      mNavHelper->mMultiGridDlg->InitForSingleGrid();
    } else {
      mNavHelper->mMultiGridDlg->SetNumUsedSlots(0);
      mNavHelper->mMultiGridDlg->ReloadTable(0, 0);
    }
    mNavHelper->mMultiGridDlg->CheckIfAnyUndoneToRun();
    mNavHelper->mMultiGridDlg->UpdateEnables();
    mNavHelper->mMultiGridDlg->UpdateSettings();
    mNavHelper->mMultiGridDlg->UpdateCurrentDir();
  }
  mAdocChanged = false;
}

/*
 * Turning off subset is separate because it would not get restored by reading in session
 */
void CMultiGridTasks::TurnOffSubset()
{
  NavAcqParams *navParams = mWinApp->GetNavAcqParams(0);
  navParams->multiGridSubset = -B3DABS(navParams->multiGridSubset);
  navParams = mWinApp->GetNavAcqParams(1);
  navParams->multiGridSubset = -B3DABS(navParams->multiGridSubset);
}

/*
 * Macros for reading a file: montage params etc
 */
#define ADOC_REQUIRED(a) \
  retval = a; \
  if (retval) \
    err++;

#define ADOC_BOOL_ASSIGN(a)  \
  if (!retval)  \
    a = index != 0;

#define INT_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetInteger(SECT_NAME, ind1, a, &b));

#define BOOL_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetInteger(SECT_NAME, ind1, a, &index));\
       ADOC_BOOL_ASSIGN(b);

#define FLOAT_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetFloat(SECT_NAME, ind1, a, &b));

#define NAV_MONT_PARAMS
#define SECT_NAME "MontParam"
#define SKIP_ADOC_PUTS

#define GET_STRING(scname, scnum, key, var) \
  retval = AdocGetString(scname, scnum, key, &string);   \
if (!retval) {    \
  var = string;   \
  free(string);   \
} else if (retval < 0) {  \
  err++;     \
}

#define GET_GRID_PARAMS(NameC, namel, arr, BREV, brev, typ) \
    NameC##ToGridSettings(brev##Param);   \
    ind1 = 0;   \
    val = AdocGet##typ##Array(MGDOC_GRID, grid, MGDOC_##BREV##_PARAM, brev##Param.values,\
      &ind1, BREV##_noParam);  \
    if (val < 0)   \
      err++;   \
    if (!val) {   \
      jcd.namel##ParamIndex = (int)mMG##arr##ParamArray.GetSize();   \
      mMG##arr##ParamArray.Add(brev##Param);   \
    }

 /*
 * Reading a session file
 */
int CMultiGridTasks::LoadSessionFile(bool useLast, CString &errStr)
{
  int adocInd, numSect, grid, err = 0, val, retval, ind1, index = IDOK, stageInd = -1;
  int stageID = -1, acqParamErr = 0;
  bool haveHF;
  char *string;
  static char szFilter[] = "Autodoc files (*.adoc)|*.adoc|All files (*.*)|*.*||";
  char keyBuf[32];
  JeolCartridgeData jcd;
  MontParam *montParam;
  LPCTSTR lpszFileName = NULL;
  CString useName = mSessionFilename;
  CString filename, direc = mWinApp->mDocWnd->GetInitialDir(), acqParmName, ext;
  MGridHoleFinderParams hfParam;
  MGridAutoContParams acParam;
  MGridMultiShotParams msParam;
  MGridGeneralParams genParam;
  MGridFocusPosParams fpParam;
  float values[MS_noParam];
  ShortVec inRunList;
  int intVals[50];
  int isAligned = 0;

  if (useName.IsEmpty())
    useName = mLastSessionFile;
  if (!useLast || useName.IsEmpty()) {
                                    
    // Start in the directory of the current file and get name
    if (mWorkingDir)
      direc = mWorkingDir;
    if (!useName.IsEmpty()) {
      UtilSplitPath(useName, direc, filename);
      lpszFileName = useName;
    }

    // Use file dialog directly since it allows filling in previous name
    MyFileDialog fileDlg(TRUE, ".adoc", lpszFileName, OFN_HIDEREADONLY, szFilter, NULL,
      !direc.IsEmpty());
    if (!direc.IsEmpty())
      fileDlg.mfdTD.lpstrInitialDir = direc;
    index = fileDlg.DoModal();
    if (index == IDOK)
      mSessionFilename = fileDlg.GetPathName();
  }
  mWinApp->RestoreViewFocus();
  if (index != IDOK)
    return -1;

  mBackedUpSessionFile = false;
  mBackedUpAcqItemsFile = false;
  UpdateDialogForSessionFile();

  // Read in file
  if (!AdocAcquireMutex()) {
    errStr = "Failed to acquire autodoc mutex";
    return 1;
  }
  adocInd = AdocRead(mSessionFilename);
  if (adocInd < 0) {
    errStr = "Error reading session autodoc file";
    AdocReleaseMutex();
    return 1;
  }
  mLastSessionFile = mSessionFilename;

  // Clear out and process global entries
  ClearSession();
  mSessionFilename = mLastSessionFile;

  // Look for nav acquire params
  UtilSplitExtension(mSessionFilename, acqParmName, ext);
  acqParmName += MGNAV_ACQ_SUFFIX;
  if (UtilFileExists(acqParmName)) {
    if (mWinApp->mParamIO->ReadAcqParamsFromFile(NULL, NULL, NULL, acqParmName, errStr)) {
      AdocReleaseMutex();
      return 1;
    }
    if (!errStr.IsEmpty())
      SEMAppendToLog("WARNING: " + errStr);
  }

  numSect = AdocGetNumberOfSections(MGDOC_GRID);
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_APPEND, &val))
    err = 1;
  mAppendNames = val != 0;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LOCKED, &mNamesLocked) < 0)
    err++;
  val = 0;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_SUBDIRS, &val) < 0)
    err++;
  mUseSubdirs = val != 0;

  if (AdocGetFloat(ADOC_GLOBAL_NAME, 0, MGDOC_REFCOUNT, &mReferenceCounts) < 0)
    err++;
  GET_STRING(ADOC_GLOBAL_NAME, 0, MGDOC_PREFIX, mPrefix);
  GET_STRING(ADOC_GLOBAL_NAME, 0, MGDOC_TOPDIR, mWorkingDir);
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMMINLD, &mLMMneedsLowDose) < 0)
    err++;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_STNUM, &mLMMusedStateNum) < 0)
    err++;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_MAGIND, &mLMMmagIndex) < 0)
    err++;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_IS_ALIGNED, &isAligned) < 0)
    err++;
  if (AdocGetInteger(ADOC_GLOBAL_NAME, 0, MGDOC_SKIP_MARKER, &mSkipMarkerShifts) < 0)
    err++;
  GET_STRING(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_STNAME, mLMMusedStateName);
  ind1 = MAX_AUTOCONT_GROUPS;
  
  val = AdocGetIntegerArray(ADOC_GLOBAL_NAME, 0, MGDOC_CONT_GROUP, mAutoContGroups, &ind1,
    MAX_AUTOCONT_GROUPS);
  if (val < 0)
    err++;
  mHaveAutoContGroups = val == 0;

  ind1 = 0;
  val = AdocGetIntegerArray(ADOC_GLOBAL_NAME, 0, MGDOC_ORDER, intVals, &ind1, 50);
  if (val < 0)
    err++;
  if (val == 0) {
    mUseCustomOrder = true;
    mCustomRunDlgInds.clear();
    inRunList.resize(numSect);
    for (val = 0; val < ind1; val++) {
      if (intVals[val] >= 0 && intVals[val] < numSect && !inRunList[intVals[val]]) {
        mCustomRunDlgInds.push_back(intVals[val]);
        inRunList[intVals[val]] = 1;
      }
    }
  }
  if (AdocGetTwoIntegers(ADOC_GLOBAL_NAME, 0, MGDOC_LMM_CONSXY,
    &mMontSetupConsetSizes[0], &mMontSetupConsetSizes[1]) < 0)
    err++;
  if (AdocGetTwoIntegers(ADOC_GLOBAL_NAME, 0, MGDOC_MMM_CONSXY,
    &mMontSetupConsetSizes[1], &mMontSetupConsetSizes[3]) < 0)
    err++;

  if (err)
    errStr = "Error reading values from global section of autodoc";
  
  if (!mScope->GetScopeHasAutoloader())
    mCartInfo->RemoveAll();

  // Process grid entries
  acqParmName = "";
  for (grid = 0; grid < numSect && !err; grid++) {
    jcd.Init();
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_ID, &jcd.id))
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_STATION, &jcd.station) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_SLOT, &jcd.slot))
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_ROTATION, &jcd.rotation) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_TYPE, &jcd.type) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_LMMAPID, &jcd.LMmapID) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_STATUS, &jcd.status) < 0)
      err++;
    if (AdocGetFloat(MGDOC_GRID, grid, MGDOC_ZSTAGE, &jcd.zStage) < 0)
      err++;
    GET_STRING(MGDOC_GRID, grid, MGDOC_NAME, jcd.name);
    GET_STRING(MGDOC_GRID, grid, MGDOC_USERNAME, jcd.userName);
    GET_STRING(MGDOC_GRID, grid, MGDOC_NOTE, jcd.note);
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_STATUS, &jcd.status) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_SEP_ACQ_ST, &jcd.separateState) < 0)
      err++;
    if (AdocGetInteger(MGDOC_GRID, grid, MGDOC_FRAME_NUM, &jcd.lastFrameNumber) < 0)
      err++;
    if (jcd.separateState) {
      for (ind1 = 0; ind1 < 4; ind1++) {
        snprintf(keyBuf, 31, "%s%d", MGDOC_ACQ_ST_NUM, ind1);
        if (AdocGetInteger(MGDOC_GRID, grid, keyBuf, &jcd.acqStateNums[ind1]) < 0)
          err++;
        snprintf(keyBuf, 31, "%s%d", MGDOC_ACQ_ST_NAME, ind1);
        GET_STRING(MGDOC_GRID, grid, keyBuf, jcd.acqStateNames[ind1]);
      }
    }
    if (jcd.LMmapID && !(jcd.status & MGSTAT_FLAG_LM_MAPPED)) {
      jcd.status |= MGSTAT_FLAG_LM_MAPPED;
      ext.Format("Grid %d had an LMmapID entry but the status entry did not\r\n"
        "have the LM mapped flag set; the flag is now being set", jcd.id);
      if (!acqParmName.IsEmpty())
        acqParmName += "\r\n";
      acqParmName += ext;
    }

    // Get the final data parameter index and check if out of range
    val = AdocGetInteger(MGDOC_GRID, grid, MGDOC_FINAL_PARAM, &jcd.finalDataParamIndex);
    if (val < 0)
      err++;
    if (!val && jcd.finalDataParamIndex >= (int)mMGAcqItemsParamArray.GetSize()) {
      ext.Format("Index of Final Data parameters stored for grid %d is out of range"
        " (%d >= %d)\r\n  No parameters are now assigned for that grid", jcd.id,
        jcd.finalDataParamIndex, mMGAcqItemsParamArray.GetSize());
      if (!acqParmName.IsEmpty())
        acqParmName += "\r\n";
      acqParmName += ext;
      jcd.finalDataParamIndex = -1;
    }

    // Get hole finder params, thresholds, sigmas
    HoleFinderToGridSettings(hfParam);
    haveHF = false;
    ind1 = 0;
    val = AdocGetFloatArray(MGDOC_GRID, grid, MGDOC_HOLE_PARAM, hfParam.values, &ind1,
      HF_noParam);
    if (val < 0)
      err++;
    if (!val)
      haveHF = true;

    ind1 = 0;
    val = AdocGetFloatArray(MGDOC_GRID, grid, MGDOC_HOLE_THRESH, values, &ind1, 
      MS_noParam);
    if (val < 0)
      err++;
    if (!val) {
      haveHF = true;
      hfParam.thresholds.resize(ind1);
      for (val = 0; val < ind1; val++)
        hfParam.thresholds[val] = values[val];
    }

    ind1 = 0;
    val = AdocGetFloatArray(MGDOC_GRID, grid, MGDOC_HOLE_FILTER, values, &ind1,
      MS_noParam);
    if (val < 0)
      err++;
    if (!val) {
      haveHF = true;
      hfParam.sigmas.resize(ind1);
      for (val = 0; val < ind1; val++)
        hfParam.sigmas[val] = values[val];
    }
    if (haveHF) {
      jcd.holeFinderParamIndex = (int)mMGHoleParamArray.GetSize();
      mMGHoleParamArray.Add(hfParam);
    }

    // Get multishot, autocontour, general, focusPos params
    GET_GRID_PARAMS(MultiShot, multiShot, MShot, MS, ms, Float);
    GET_GRID_PARAMS(AutoCont, autoCont, AutoCont, AC, ac, Float);
    GET_GRID_PARAMS(General, general, General, GEN, gen, Integer);
    GET_GRID_PARAMS(FocusPos, focusPos, FocusPos, FP, fp, Float);

    if (!err && jcd.station == JAL_STATION_STAGE) {
      stageInd = grid;
      stageID = jcd.id;
    }
    mCartInfo->Add(jcd);

    if (err)
      errStr = "Error reading values from a grid section of the autodoc";
  }

  // Get montage params and mark as initialized
  numSect = AdocGetNumberOfSections(SECT_NAME);
  for (ind1 = 0; ind1 < numSect && !err; ind1++) {
    if (AdocGetSectionName(SECT_NAME, ind1, &string) != 0) {
      err++;
    } else {
      if (string[0] == '0') {
        mInitializedGridMont = true;
        montParam = &mGridMontParam;
      } else {
        mInitializedMMMmont = true;
        montParam = &mMMMmontParam;
      }
      free(string);
#include "NavAdocParams.h"
      mWinApp->mNavigator->FinishMontParamLoad(montParam, ind1, err);
      montParam->warnedConSetBin = true;
      if (AdocGetFloat(SECT_NAME, ind1, "MinUmOverlap", &montParam->minMicronsOverlap))
        err++;
      if (AdocGetFloat(SECT_NAME, ind1, "MinOverlapFac", &montParam->minOverlapFactor))
        err++;
      if (err)
        errStr = "Error reading MontParam section of autodoc";
    }
  }
  AdocClear(adocInd);

  // Adjust dialog and other parameters
  if (err) {
    ClearSession();
    errStr = acqParmName + errStr;
  } else {
    if (mNavHelper->mMultiGridDlg) {
      mNavHelper->mMultiGridDlg->SetNumUsedSlots((int)mCartInfo->GetSize());
      mNavHelper->mMultiGridDlg->m_strPrefix = mPrefix;
      mNavHelper->mMultiGridDlg->SetRootname();
      mNavHelper->mMultiGridDlg->UpdateData(false);
      mNavHelper->mMultiGridDlg->ReloadTable(1, 1 + (mUseCustomOrder ? 1 : 0));
      mNavHelper->mMultiGridDlg->CheckIfAnyUndoneToRun();
      mNavHelper->mMultiGridDlg->UpdateCurrentDir();
    }
    IdentifyGridOnStage(stageID, stageInd);
    if (stageInd >= 0)
      mLoadedGridIsAligned = isAligned ? -1 : 0;
    mParams.appendNames = mAppendNames;
    mParams.useSubdirectory = mUseSubdirs;
    if (!mWorkingDir.IsEmpty() && !mNamesLocked) {
      if (!_chdir((LPCTSTR)mWorkingDir))
        mWinApp->mDocWnd->SetInitialDirToCurrentDir();
    }

    if (mNavHelper->mMultiGridDlg) {
       mNavHelper->mMultiGridDlg->UpdateSettings();
      mNavHelper->mMultiGridDlg->UpdateEnables();
    }
    if (!acqParmName.IsEmpty()) {
      errStr = acqParmName;
      err = -1;
    }
  }

  AdocReleaseMutex();
  mAdocChanged = false;

  return err;
}

// Maintain the filename on the dialog title bar
void CMultiGridTasks::UpdateDialogForSessionFile()
{
  CString direc, filename;
  if (!mNavHelper->mMultiGridDlg)
    return;
  UtilSplitPath(mSessionFilename, direc, filename);
  mNavHelper->mMultiGridDlg->SetWindowText("Multiple Grid Operations: " + filename);
}

#undef ADOC_REQUIRED
#undef ADOC_BOOL_ASSIGN
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN
#undef SECT_NAME
#undef NAV_MONT_PARAMS
#undef SKIP_ADOC_PUTS
#undef GET_STRING

/*
 * Function to let the user specify the grid on the stage; the ID and index are of what
 * is thought to be on the stage if anything
 */
void CMultiGridTasks::IdentifyGridOnStage(int stageID, int &stageInd)
{
  CString direc;
  int ind1, newID = stageID;
  if (!mScope->GetScopeHasAutoloader()) {
    stageInd = 0;
    return;
  }
  direc.Format("Enter %s of grid on stage or -1 if none:", JEOLscope ? "ID" : "slot #");
  if (KGetOneInt(direc, newID)) {
    stageInd = -1;

    // Make sure at most one grid is on the stage, fix any goofs no matter they happened
    for (ind1 = 0; ind1 < (int)mCartInfo->GetSize(); ind1++) {
      JeolCartridgeData &jcdEl = mCartInfo->ElementAt(ind1);
      if (jcdEl.id == newID) {
        stageInd = ind1;
        if (jcdEl.station != JAL_STATION_STAGE)
          mAdocChanged = true;
        jcdEl.station = JAL_STATION_STAGE;
      } else if (jcdEl.station == JAL_STATION_STAGE) {
        jcdEl.station = JAL_STATION_MAGAZINE;
        mAdocChanged = true;
      }
    }
    if (stageInd < 0 && newID >= 0)
      AfxMessageBox("That number is not in the inventory", MB_EXCLAME);
  }
  if (mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->NewGridOnStage(stageInd);
  mScope->SetSimCartridgeLoaded(newID);
  mWinApp->RestoreViewFocus();
}

/*static float RRGfunk(float *y, float *err)
{
  int ind;
  float dx, dy;
  float cosRot = (float)cos(y[0] * DTOR);
  float sinRot = (float)sin(y[0] * DTOR);
  *err = 0.;
  for (ind = 0; ind < 5; ind++) {
    if (ind == sSkipInd)
      continue;
    dx = sOrigXstage[ind] * (cosRot + y[1]) - sOrigYstage[ind] * (sinRot - y[2]) + y[3] -
      sNewXstage[ind];
    dy = sOrigXstage[ind] * (sinRot + y[2]) + sOrigYstage[ind] * (cosRot - y[1]) + y[4] -
      sNewYstage[ind];
    *err += dx * dx + dy * dy;
  }
}*/
