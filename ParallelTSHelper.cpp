// ParallelTSHelper.cpp : Helper class for Parallel Tilt Series and task-oriented routines
//
//
// Copyright (C) 2007-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: Leo Crowder
//
#include "stdafx.h"
#include "SerialEM.h"
#include "ParallelTSDlg.h"
#include "NavHelper.h"
#include "EMscope.h"
#include "NavigatorDlg.h"
#include "ParticleTasks.h"
#include "ComplexTasks.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "SerialEMDoc.h"
#include "MultiShotDlg.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "AutoTuning.h"
#include "FocusManager.h"
#include "TSController.h"
#include "SerialEMView.h"
#include "ParallelTSHelper.h"
#include ".\Utilities\SEMUtilities.h"
#include "Shared\b3dutil.h"
#include "Shared\icont.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

CParallelTSHelper::CParallelTSHelper()
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mShiftManager = mWinApp->mShiftManager;
  mScope = NULL;
  mCamera = NULL;
  mNavHelper = NULL;
  mParallelTSDlg = NULL;

  mISTargetIter = -1;
  mActionAtTarget = PARALLELTS_ACTION_AUTOFOCUS;
  mDoingISToTargets = false;
  mLastActionFailed = false;
  mPretilt = 0.f;
  mXpitch = 0.f;
  mStartIndex = 0;
  mAreaMapFileName = "";
  mAreaMapID = 0;
  mTargetMapFileName = "";
  mParTSitem = NULL;
  mSavedTSparamIndex = -1;
  mInitialStateSaved = false;
  mTiltDuringFit = 0.;
  mMagIndex = -1;
  mAreaMapMagInd = -1;
}

CParallelTSHelper::~CParallelTSHelper()
{

}

void CParallelTSHelper::Initialize(void)
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mNavHelper = mWinApp->mNavHelper;

  mParTSopts = mNavHelper->GetParTSOptions();
  mParallelTSDlg = mNavHelper->mParallelTSDlg;

  if (mWinApp->LowDoseMode()) {
    LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mMagIndex = ldp->magIndex;
  } else {
    mMagIndex = mParTSopts->acqMagIndNonLD;
  }
}

// Busy function for task when doing IS
int CParallelTSHelper::ISToTargetsBusy()
{
  return (mCamera->CameraBusy() || mScope->StageBusy() ||
    mNavHelper->GetRealigning() || mWinApp->mFocusManager->DoingFocus()) ? 1 : 0;
}

void CParallelTSHelper::ClearTargets(bool autofocusOrPreview)
{
  mStartIndex = 0;
  mAlignedToFirstISTarget = false;
  mInitialStateSaved = false;
  mISTargetPointIDs.clear();
  mSavedTargetIDs.clear();
  if (autofocusOrPreview) {
    mISTargetSSX.clear();
    mISTargetSSY.clear();
    mISTargetDefocus.clear();
  } else {
    mISTargetISX.clear();
    mISTargetISY.clear();
    mPrevMapShiftX.clear();
    mPrevMapShiftY.clear();
    mPreviewMapIDs.clear();
    mPrevMapSectNums.clear();
  }
}

//Only clear saved data, but keep target map IDs so that refining can be retried
void CParallelTSHelper::ClearSavedTargets()
{
  mStartIndex = 0;
  mAlignedToFirstISTarget = false;
  mInitialStateSaved = false;
  mSavedTargetIDs.clear();
  mPrevMapShiftX.clear();
  mPrevMapShiftY.clear();
  mPreviewMapIDs.clear();
  mPrevMapSectNums.clear();
}

// Begin procedure of image shifting to targets with given map IDs, and either getting the 
// autofocusing or taking Previews at each target
int CParallelTSHelper::StartShiftToTargets(IntVec targetMapIDs, bool autofocusOrPreview)
{
  int err;
  CString mess;
  
  err = AppendNewTargets(targetMapIDs, mess);
  if (err) {
    AfxMessageBox(mess, MB_EXCLAME);
    return err;
  }
  mDoingISToTargets = true;
  mISTargetIter = 0;
  mAtTarget = false;
  mLastActionFailed = false;
  mParTSopts = mNavHelper->GetParTSOptions();

  if (mWinApp->LowDoseMode()) {
    LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mMagIndex = ldp->magIndex;
  } else {
    mMagIndex = mParTSopts->acqMagIndNonLD;
  }

  if (autofocusOrPreview) {
    mActionAtTarget = PARALLELTS_ACTION_AUTOFOCUS;
    mess = "FITTING PLANE";
  } else {
    mActionAtTarget = PARALLELTS_ACTION_PREVIEW;
    mSavedMouseStage = mShiftManager->GetMouseMoveStage();
    mShiftManager->SetMouseMoveStage(false);
    mess = "REFINING TARGETS";
  }

  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, mess);
  mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
  return 0;
}

void CParallelTSHelper::UpdatePlaneParamsInDlg()
{
  if (mParallelTSDlg->IsOpen()) {
    mParallelTSDlg->UpdatePlaneParams(mPretilt, mXpitch);
  }
}

//Pauses the routine, does not restore original state so that more targets can be added.
void CParallelTSHelper::PauseParallelTSShift()
{
  mISTargetIter = -1;
  mDoingISToTargets = false;

  if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
    mParallelTSDlg->FinishRefineTargets(false);
  }
  mWinApp->SetStatusText(COMPLEX_PANE, "");
}

//Stops the routine, and restores original state of the scope.
void CParallelTSHelper::StopParallelTSShift(bool error)
{
  int numPoints;
  CMapDrawItem *item, *mapItem;
  
  mISTargetIter = -1;

  if (mCamera->Acquiring()) {
    mCamera->SetImageShiftToRestore(mBaseISX, mBaseISY);
    if (mParTSopts->adjustBeamTilt)
      mCamera->SetBeamTiltToRestore(mBaseBeamTiltX, mBaseBeamTiltY);
    if (mAdjustBeamTilt)
      mCamera->SetAstigToRestore(mBaseAstigX, mBaseAstigY);
  } else {
    mScope->SetImageShift(mBaseISX, mBaseISY);
    if (mParTSopts->adjustBeamTilt)
      mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
    if (mAdjustBeamTilt)
      mScope->SetObjectiveStigmator(mBaseAstigX, mBaseAstigY);
  }

  mDoingISToTargets = false;
  mAlignedToFirstISTarget = false;
  mDoNextShift = false;
  mInitialStateSaved = false;

  if (mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS) {
    numPoints = (int)mISTargetPointIDs.size();
    if (numPoints > 0) {
      item = mWinApp->mNavigator->FindItemWithMapID(
        mISTargetPointIDs[numPoints - 1], false);
      if (item && item->mDrawnOnMapID) {
        mapItem = mWinApp->mNavigator->FindItemWithMapID(item->mDrawnOnMapID);
        if (mapItem)
          mWinApp->mNavigator->DoLoadMap(true, mapItem, -1);
      }
    }

    if (!GetDebugOutput('N')) {
      if (numPoints == 1)
        mWinApp->mNavigator->DeleteItem();
      else if (numPoints > 1) {
        mWinApp->mNavigator->ExternalDeleteGroup(
          mWinApp->mNavigator->m_bCollapseGroups != 0);
      }
    }
    mParallelTSDlg->FinishFitPlane();
    ClearTargets(true);
  } else if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
    mShiftManager->SetMouseMoveStage(mSavedMouseStage);
    mParallelTSDlg->FinishRefineTargets(error);
    if (error)
      ClearSavedTargets();
  }
  mWinApp->SetStatusText(COMPLEX_PANE, "");
}

// Applies image shift to get to the next target. 
// If returned error code is < 0, the whole procedure should be aborted. If > 0, then just
// the given target should be skippped.
int CParallelTSHelper::ISToNextTarget(int targetIndex, CString &err)
{
  double ISX, ISY, delX, delY, delBTX = 0., delBTY = 0., delAstigX = 0., delAstigY = 0.;
  float delay, transISX, transISY;
  int BTdelay, fromMagInd, area, mapID;
  CMapDrawItem *item = mWinApp->mNavigator->FindItemWithMapID(targetIndex, false);
  ScaleMat st2is, focMat;
  bool doBacklash = mScope->GetAdjustForISSkipBacklash() <= 0;
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  ParallelTSOptions *parTSopt = mNavHelper->GetParTSOptions();
  CString mess;
  
  mapID = item->mDrawnOnMapID;
  CMapDrawItem *mapItem = mWinApp->mNavigator->FindItemWithMapID(mapID);
  if (!mapItem) {
    err.Format("The map on which items were drawn no longer exists");
    return -2;
  }
  area = mapItem->mMapLowDoseConSet;

  fromMagInd = mAreaMapMagInd;
  if (fromMagInd < 0) {
    fromMagInd = mapItem->mMapMagInd;
  }

  st2is = MatMul(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(),
    fromMagInd), mShiftManager->CameraToIS(fromMagInd));
  SEMTrace('N', "stage to IS: mag index %d xpx %.3f xpy %.3f ypx %.3f ypy %.3f",
    fromMagInd, st2is.xpx, st2is.xpy, st2is.ypx, st2is.ypy);
  if (!st2is.xpx) {
    err.Format("There is no calibration to get from stage to IS coordinates at the "
      "given mag");
    return -1;
  }
  ApplyScaleMatrix(st2is, item->mStageX - mCenterStageX,
    item->mStageY - mCenterStageY, ISX, ISY);
  mShiftManager->TransferGeneralIS(fromMagInd, ISX, ISY, mMagIndex, delX, delY);
  focMat = CMultiShotDlg::ISfocusAdjustmentForBufOrArea(NULL, area);
  if (focMat.xpx)
    ApplyScaleMatrix(MatInv(focMat), delX, delY, delX, delY);

  if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW && mParTSopts->applyAdjustingXform) {
    if (CanAdjustISVectors(mAreaMapMagInd, false, mess)) {
      MultiShotParams *params = mNavHelper->GetMultiShotParams();
      SEMTrace('N', "adj xform xpx=%.4f, xpy=%.4f, ypx=%.4f, ypy=%.4f", 
        params->adjustingXform.xpx, params->adjustingXform.xpy, 
        params->adjustingXform.ypx, params->adjustingXform.ypy);
      ApplyScaleMatrix(params->adjustingXform, delX, delY, transISX, transISY);
      SEMTrace('N', "IS transformed from (%.4f, %.4f) to (%.4f, %.4f)",
        delX, delY, transISX, transISY);
      delX = transISX;
      delY = transISY;
    } else {
      SEMTrace('N', "Adjusting transform not applied: %s", mess);
    }
  }

  ISX = mCenterISX + delX;
  ISY = mCenterISY + delY;

  if (!mShiftManager->ImageShiftIsOK(ISX, ISY, FALSE)) {
    err.Format("Point at navigator index %d is beyond image shift limit and will "
      "be skipped", targetIndex);
    PrintfToLog("%s", err);
    return 1;
  }

  mCurISTargetItem = item;

  mScope->GetImageShift(mLastISX, mLastISY);

  // Compute the delay for the amount that IS will change
  delay = mShiftManager->ComputeISDelay(ISX - mLastISX, ISY - mLastISY);
  if (parTSopt->extraDelayFactor > 0.)
    delay *= parTSopt->extraDelayFactor;

  if (mParTSopts->adjustBeamTilt) {
    BTdelay = mWinApp->mAutoTuning->GetBacklashDelay();
    mWinApp->mParticleTasks->GetBTandAstigAdjustment(delX, delY, delBTX, delBTY,
      delAstigX, delAstigY, FALSE, GetDebugOutput('1'));
  }

  mScope->SetImageShift(ISX, ISY);
  mShiftManager->SetISTimeOut(delay);

  if (mParTSopts->adjustBeamTilt) {
    mWinApp->mAutoTuning->BacklashedBeamTilt(mCenterBeamTiltX + delBTX,
      mCenterBeamTiltY + delBTY, doBacklash);
    if (mAdjustBeamTilt)
      mWinApp->mAutoTuning->BacklashedStigmator(mCenterAstigX + delAstigX,
        mCenterAstigY + delAstigY, doBacklash);
  }

  mLastISX = ISX;
  mLastISY = ISY;

  return 0;
}

void CParallelTSHelper::ISToTargetNextTask(int param)
{
  int index, mapID;
  float residual;
  float focusLim = -20.;
  float shiftX, shiftY;
  int sizeX, sizeY;
  float FOVchange, FOVchangeThresh = 0.1f;
  CString curPath = "";
  bool mapSaved, openNewFile = false, openOldFile = false;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  CString mess, str;
  ScaleMat st2ss = MatInv(mShiftManager->SpecimenToStage(1., 1.));
  NavAlignParams *alignParams = mNavHelper->GetNavAlignParams();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  bool canAdjustIS = mShiftManager->GetFocusISCals()->GetSize() > 0 &&
    mShiftManager->GetFocusMagCals()->GetSize() > 0;

  if (!mDoingISToTargets || mISTargetIter < 0)
    return;

  if (mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS) {
    mess = "FITTING PLANE";
  } else if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
    mess = "REFINING TARGETS";
  }
  mWinApp->SetStatusText(COMPLEX_PANE, mess);

  mLastActionFailed = false;
  mapID = mISTargetPointIDs[mStartIndex + mISTargetIter];
  mCurISTargetItem = mWinApp->mNavigator->FindItemWithMapID(mapID, false);

  if (mISTargetIter == 0 && !mAlignedToFirstISTarget) { 
    if (!mCurISTargetItem) {
      StopParallelTSShift();
      return;
    }

    //Realign to first IS Target
    if (mNavHelper->RealignToItem(mCurISTargetItem, 0, 
      alignParams->resetISthresh, alignParams->maxNumResetIS, 0, 0,
      mParTSopts->extractVirtPrevs == 1 ? PREVIEW_CONSET : -1)) {
      AfxMessageBox("Realign to first IS target failed");
      mLastActionFailed = true;
      return;
    }
    mAlignedToFirstISTarget = true;
    mDoNextShift = false;
    mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
    return;
  }

  if (mAtTarget && mCurISTargetItem) {

    if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
      imBufs->mImage->getShifts(shiftX, shiftY);
      imBufs->mImage->getSize(sizeX, sizeY);
      FOVchange = (1 - (sizeX - fabs(shiftX)) * (sizeY - fabs(shiftY)) / 
        (float)(sizeX * sizeY));

      if (FOVchange > FOVchangeThresh) {
        mCamera->InitiateCapture(PREVIEW_CONSET);
        mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
        return;
      }

      if (mParTSopts->extractVirtPrevs == 0){
        if (SaveTargetMap(mess, mapSaved)) {
          AfxMessageBox(mess, MB_EXCLAME);
          StopParallelTSShift();
          return;
        }

        if (!mapSaved) {
          if (!mess.IsEmpty()) {
            str.Format("Map was not saved: %s", mess);
            AfxMessageBox(str, MB_EXCLAME);
          }
          return;
        }
      }
    }

    if (!mInitialStateSaved) {
      SaveInitialState();
    }
 
    bool skipSave = mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS && mISTargetIter == 0;

    if ((mSavedTargetIDs.size() == 0 && mActionAtTarget == PARALLELTS_ACTION_PREVIEW) 
      || skipSave) {
      mCenterStageX = mCurISTargetItem->mStageX;
      mCenterStageY = mCurISTargetItem->mStageY;
      mScope->GetImageShift(mCenterISX, mCenterISY);
      mTiltDuringFit = mScope->GetTiltAngle();
    }
    
    if (!skipSave && SaveTarget(mess)) {
      AfxMessageBox(mess, MB_EXCLAME);
      StopParallelTSShift(true);
      ClearSavedTargets();
      return;
    }

  } else if (mAtTarget && !mCurISTargetItem) {

    //Center item was deleted but none have been saved, cancel refining
    if (mSavedTargetIDs.size() == 0) {
      StopParallelTSShift(true);
      ClearTargets(true);
      return;
    }
    if (!mInitialStateSaved) {
      SaveInitialState();
    }
    mDoNextShift = true;
  }

  if (mDoNextShift) {
    mAtTarget = false;

    if (mStartIndex + mISTargetIter >= (int)mISTargetPointIDs.size() - 1) {
      if (mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS) {

        // Run the least squares fit to get pretilt and X pitch angle
        index = FitPlane(mPretilt, mXpitch, residual, mess);
        if (index) {
          str.Format("Plane fit on points failed: %s", mess);
          AfxMessageBox(str, MB_EXCLAME);
        } else {
          UpdatePlaneParamsInDlg();
        }

        StopParallelTSShift();
        ClearTargets(true);
      } else if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
         PauseParallelTSShift();
      }
      return;
    }

    // Increment the target iteration to get next target
    //If some targets were done on a previous separate run, skip to the first new target
    mISTargetIter++;
    mapID = mISTargetPointIDs[mStartIndex + mISTargetIter];

    // Do the image shift to next target
    index = ISToNextTarget(mapID, mess);

    // If returned error code is < 0, the whole procedure should be aborted. 
    // If > 0, then just the given target should be skippped.
    if (index < 0) {
      AfxMessageBox(mess, MB_EXCLAME);
      StopParallelTSShift(true);
      ClearSavedTargets();
      return;
    } else if (index > 0) {
      mLastActionFailed = true;
    }

    // If failed at this point, skip action and shift to next point if continuing on
    mDoNextShift = mLastActionFailed;
  }

  if (!mDoNextShift) {

    //Queue to save this target and shift to next target on next iteration
    mAtTarget = true;
    mDoNextShift = true;

    //Get Image shift before next iteration
    mScope->GetImageShift(mLastISX, mLastISY);

    if (mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS) {
      if (mISTargetIter)
        mWinApp->mFocusManager->AutoFocusStart(1, -2);
    } else if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {
      mCamera->InitiateCapture(PREVIEW_CONSET);

      // Stop iterations to allow user to refine IS, unless they want to skip that
      if (!(mParTSopts->flags & PTSFLAG_SKIP_REFINE))
        return;
    }
  }

  mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
}

int CParallelTSHelper::SaveAreaMap(CString &err)
{
  int index;
  bool wrongFile, openNewFile = false, openOldFile = false;
  CFile *cfile;
  CMapDrawItem *item;
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();

  // Current image buffer is a loaded map, so map already exists
  item = mWinApp->mNavigator->FindItemWithMapID(imBuf->mMapID);
  if (!item) {

    //If no open file, or open file is unusable, do not use it
    index = mWinApp->mDocWnd->StoreIndexFromName(mTargetMapFileName);
    wrongFile = !mTargetMapFileName.IsEmpty() && (index >= 0 ||
      index == mWinApp->mDocWnd->GetCurrentStore());
    if (!mWinApp->mStoreMRC || wrongFile ||
      !mWinApp->mBufferManager->IsBufferSavable(imBuf, mWinApp->mStoreMRC)) {

      //If Area map is defined and exists, switch to or open it. If not, open new file
      if (!mAreaMapFileName.IsEmpty() && UtilFileExists(mAreaMapFileName) != 0) {
        index = mWinApp->mDocWnd->StoreIndexFromName(mAreaMapFileName);
        if (index >= 0)
          mWinApp->mDocWnd->SetCurrentStore(index);
        else {
          index = mWinApp->mDocWnd->OpenOldMrcCFile(&cfile, mAreaMapFileName, false);
          if (index == MRC_OPEN_NOERR || index == MRC_OPEN_ADOC || index == MRC_OPEN_HDF)
            index = mWinApp->mDocWnd->OpenOldFile(cfile, mAreaMapFileName, index, true);
          if (index != MRC_OPEN_NOERR) {
            err.Format("Error opening old area map file");
            return 1;
          }
        }
      } else {
        if (mWinApp->mDocWnd->DoOpenNewFile()) {
          err.Format("New file was not opened");
          return 2;
        }
      }
    }

    if (imBuf->GetSaveCopyFlag() >= 0) 
      mWinApp->mDocWnd->SaveRegularBuffer();

    if (mWinApp->mNavigator->NewMap()) {
      err.Format("Error making a new area map");
      return 3;
    }

    index = mWinApp->mDocWnd->StoreIndexFromName(mAreaMapFileName);
    if (index >= 0) {
      mWinApp->mDocWnd->SetCurrentStore(index);
      mWinApp->mDocWnd->DoCloseFile();
    }

    item = mWinApp->mNavigator->GetCurrentItem();
  }
  mAreaMapID = item->mMapID;
  mAreaMapMagInd = item->mMapMagInd;
  mAreaMapFileName = item->mMapFile;
  mMappingTilt = item->mMapTiltAngle;
  return 0;
}

int CParallelTSHelper::SaveTargetMap(CString &err, bool &saved)
{
  int areaStore, tgtStore, store, index, numMaps;
  bool areaFileOpen, tgtFileOpen, fileExists;
  CFile *cfile;
  CMapDrawItem *item;
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  saved = false;

  // check that mag of current buffer image matches acquire mag
  if (imBuf->mMagInd != mMagIndex) {
    err.Format("Target Maps must match the magnification that will be used to "
      "acquire images");
    return 0;
  }

  fileExists = !mTargetMapFileName.IsEmpty() && UtilFileExists(mTargetMapFileName);
  tgtStore = mWinApp->mDocWnd->StoreIndexFromName(mTargetMapFileName);
  numMaps = (int)mPreviewMapIDs.size();

  //If no open file, or open file is unusable, do not use it
  areaStore = mWinApp->mDocWnd->StoreIndexFromName(mAreaMapFileName);
  store = mWinApp->mDocWnd->GetCurrentStore();
  areaFileOpen = (areaStore >= 0 && areaStore == store);
  tgtFileOpen = (tgtStore >= 0 && tgtStore == store);

  if (tgtStore >= 0 && tgtStore != store) {
    mWinApp->mDocWnd->SetCurrentStore(tgtStore);
  } else if (!tgtFileOpen && (!mWinApp->mStoreMRC || areaFileOpen || numMaps > 0 ||
    !mWinApp->mBufferManager->IsBufferSavable(imBuf, mWinApp->mStoreMRC))) {
    if (fileExists) {
      index = mWinApp->mDocWnd->OpenOldMrcCFile(&cfile, mTargetMapFileName, false);
      if (index == MRC_OPEN_NOERR || index == MRC_OPEN_ADOC || index == MRC_OPEN_HDF)
        index = mWinApp->mDocWnd->OpenOldFile(cfile, mTargetMapFileName, index, true);
      if (index != MRC_OPEN_NOERR) {
        err.Format("Error opening map file");
        return 2;
      }
    } else if (numMaps == 0) {
      if (mWinApp->mDocWnd->DoOpenNewFile())
        return 0;
    } else if (numMaps > 0) {
    //If some preview maps were saved but map file was deleted, throw error
      err.Format("The Preview map file was deleted");
      return 3;
    }
  }

  mWinApp->mDocWnd->SaveRegularBuffer();

  if (mWinApp->mNavigator->NewMap()) {
    err.Format("Error making a new Preview map");
    return 0;
  }

  item = mWinApp->mNavigator->GetCurrentItem();
  if (numMaps == 0)
    mTargetMapFileName = item->mMapFile;
  mPreviewMapIDs.push_back(item->mMapID);
  mPrevMapSectNums.push_back(item->mMapSection);

  saved = true;
  return 0;
}

int CParallelTSHelper::SaveInitialState()
{
  int area;
  CString err;
  bool canAdjustIS = mShiftManager->GetFocusISCals()->GetSize() > 0 &&
    mShiftManager->GetFocusMagCals()->GetSize() > 0;
  double stageZ;
  float focusLim = -20.;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();

  area = mScope->GetLowDoseArea();

  // First time, record stage position and check for defocus

  // Go to the desired mag or low dose area before recording any values
  if (mWinApp->LowDoseMode()) {
    mScope->GotoLowDoseArea(RECORD_CONSET);
  } else {
    if (mScope->GetMagIndex() != mMagIndex &&
      !mScope->SetMagIndex(mMagIndex)) {
      //TODO error
      return 1;
    }
  }

  mScope->GetStagePosition(mBaseStageX, mBaseStageY, stageZ);
  if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW &&
    !canAdjustIS && ((imBufs->mLowDoseArea && (imBufs->mConSetUsed == VIEW_CONSET ||
    imBufs->mConSetUsed == SEARCH_CONSET) && imBufs->mViewDefocus < focusLim) ||
    (mWinApp->LowDoseMode() && IS_AREA_VIEW_OR_SEARCH(area) &&
      mScope->GetLDViewDefocus(area) < focusLim))) {
    err.Format("It appears that the scope is in the View Low Dose\n"
      "area with a View defocus offset bigger than %.0f microns.\n\n"
      "This procedure should be run closer to focus.\n"
      "Press Abort to end it, or continue if you know what you are doing.", focusLim);
    AfxMessageBox(err, MB_EXCLAME);
  }

  mScope->GetImageShift(mBaseISX, mBaseISY);

  mAdjustBeamTilt = mParTSopts->adjustBeamTilt && comaVsIS->astigMat.xpx != 0. &&
    mNavHelper->GetSkipAstigAdjustment() <= 0;

  if (mParTSopts->adjustBeamTilt) {
    mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
    mCenterBeamTiltX = mBaseBeamTiltX;
    mCenterBeamTiltY = mBaseBeamTiltY;
    if (mAdjustBeamTilt) {
      mScope->GetObjectiveStigmator(mBaseAstigX, mBaseAstigY);
      mCenterAstigX = mBaseAstigX;
      mCenterAstigY = mBaseAstigY;
    }
  }
  mInitialStateSaved = true;
  return 0;
}

//Save info about current target. For plane fit, specimen coords and defocus. For target
//refinement, image shifts and Preview map info.
int CParallelTSHelper::SaveTarget(CString &err)
{
  int index, index2, mapID;
  int area;
  float defocus, SX, SY;
  float shiftX, shiftY;
  CString mess;
  ScaleMat mat;
  NavAlignParams *alignParams = mNavHelper->GetNavAlignParams();
  bool canAdjustIS = mShiftManager->GetFocusISCals()->GetSize() > 0 &&
    mShiftManager->GetFocusMagCals()->GetSize() > 0;
  double ISX, ISY, stageX, stageY, stageZ, delX, delY;
  float ISlimit = 2.f * mWinApp->mShiftCalibrator->GetCalISOstageLimit();
  float focusLim = -20.;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  ScaleMat focMat;
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();

  area = mScope->GetLowDoseArea();
  mapID = mCurISTargetItem->mMapID;
  
  mScope->GetStagePosition(stageX, stageY, stageZ);
  if (fabs(stageX - mBaseStageX) > ISlimit ||
    fabs(stageY - mBaseStageY) > ISlimit) {
    err.Format("The stage appears to have moved by %.3f in X and %.3f in Y\n"
      "(more than ISoffsetCalStageLimit = %f).\n\n"
      "There must be no stage movement during this procedure.",
      stageX - mBaseStageX, stageY - mBaseStageY, ISlimit);
    return 1;
  }
  
  // If taking Previews, save the image shift at the target
  if (mActionAtTarget == PARALLELTS_ACTION_PREVIEW) {

    // Get the IS and save it
    mScope->GetImageShift(ISX, ISY);
    delX = ISX - mLastISX;
    delY = ISY - mLastISY;
    mLastISX = ISX;
    mLastISY = ISY;

    // But adjust IS for defocus first if possible
    if (canAdjustIS) {
      focMat = CMultiShotDlg::ISfocusAdjustmentForBufOrArea(imBufs, area);
      if (focMat.xpx)
        ApplyScaleMatrix(focMat, ISX, ISY, ISX, ISY);
    }
    
    imBufs->mImage->getShifts(shiftX, shiftY);
    mPrevMapShiftX.push_back(shiftX);
    mPrevMapShiftY.push_back(shiftY);
    mISTargetISX.push_back(ISX);
    mISTargetISY.push_back(ISY);
    mSavedTargetIDs.push_back(mCurISTargetItem->mMapID);

    //If center point, update stage position
    if (mSavedTargetIDs.size() == 1 && (delX != 0 || delY != 0)) {

      //Convert IS change to stage shift
      mat = MatMul(mShiftManager->IStoCamera(mMagIndex), 
        MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mMagIndex)));
      mShiftManager->ApplyScaleMatrix(mat, delX, delY, SX, SY);

      SEMTrace('N', "Shifted stage coords of PTS center pt by %.3f, %.3f", SX, SY);

      mCenterStageX += SX;
      mCenterStageY += SY;
      mCurISTargetItem->mStageX = mCenterStageX;
      mCurISTargetItem->mStageY = mCenterStageY;
      mCurISTargetItem->mPtX[0] = mCenterStageX;
      mCurISTargetItem->mPtY[0] = mCenterStageY;
      mWinApp->mNavigator->FindItemWithMapID(mapID, false);
      mWinApp->mNavigator->UpdateListString(mWinApp->mNavigator->GetFoundItem());
      mWinApp->mNavigator->Redraw();
    }

  } else if (mActionAtTarget == PARALLELTS_ACTION_AUTOFOCUS) {

    // If doing autofocus at targets, get the defocus
    index = mWinApp->mFocusManager->GetLastFailed() ? -1 : 0;
    index2 = mWinApp->mFocusManager->GetLastAborted();
    if (index2)
      index = index2;
    if (index) {
      PrintfToLog("WARNING: Autofocus failed on point at index %d with error type %d",
        mapID, index);
      mLastActionFailed = true;

    } else {
      defocus = (float)mScope->GetDefocus();
      mISTargetDefocus.push_back(-defocus);

      if (mISTargetDefocus.size() == 1) {
        mBaseDefocus = defocus;
      } else {
        mScope->SetDefocus(mBaseDefocus);
      }

      mat = MatInv(mShiftManager->SpecimenToStage(1., 1.));
      mShiftManager->ApplyScaleMatrix(mat, mCurISTargetItem->mStageX,
        mCurISTargetItem->mStageY, SX, SY);
      mISTargetSSX.push_back(SX);
      mISTargetSSY.push_back(SY);
      mSavedTargetIDs.push_back(mCurISTargetItem->mMapID);
    }

    //Get Image shift since we will go straight into shift to next target
    mScope->GetImageShift(mLastISX, mLastISY);
  }
  return 0;
}

// Measure z-height at navigator items in the specified group, and perform a least 
// squares fit to a plane to determine the pretilt and X pitch angle.
int CParallelTSHelper::FitPlane(float &pretilt, float &xPitch, float &residual,
  CString &mess)
{
  int numPoints, numToDrop;
  FloatVec residuals;
  float alpha;
  float dev;
  float xCoef, yCoef, zIntercept;
  int i;

  if ((int)mISTargetDefocus.size() < MIN_NUM_POINTS_TO_FIT_PLANE) {
    mess.Format("Defocus was measured at %d out of %d points, "
      "but %d are required to fit to plane",
      (int)mISTargetDefocus.size(), (int)mISTargetPointIDs.size(), 
      MIN_NUM_POINTS_TO_FIT_PLANE);
    return 3;
  }

  //Run least squares fit on vectors of x,y,z coords
  numPoints = (int)mISTargetDefocus.size();
  lsFit2(&mISTargetSSX[0], &mISTargetSSY[0], &mISTargetDefocus[0], numPoints, &xCoef,
    &yCoef, &zIntercept);

  for (i = 0; i < numPoints; i++) {
    dev = mISTargetDefocus[i] -
      (mISTargetSSX[i] * xCoef + mISTargetSSY[i] * yCoef + zIntercept);
    residuals.push_back(B3DABS(dev));
    if (GetDebugOutput('N')) {
      PrintfToLog("%d  %.4f  %.4f  %.4f  %.4f", i, mISTargetSSX[i], mISTargetSSY[i],
        mISTargetDefocus[i], dev);
    }
  }

  if (numPoints > MIN_NUM_POINTS_TO_FIT_PLANE) {
    FloatVec outliers, outlierResid;
    float elimMin = 0.01f; //TODO what should this be?
    mWinApp->mTSController->FindOutliersInResultList(residuals, elimMin, 1, outliers);
    IntVec dropInd;
    for (i = 0; i < numPoints; i++) {
      if (outliers[i] > 0) {
        outlierResid.push_back(residuals[i]);
        dropInd.push_back(i);
      }
    }

    //Remove outliers and redo the fit
    numToDrop = B3DMIN((int)dropInd.size(), numPoints - MIN_NUM_POINTS_TO_FIT_PLANE);

    if (numToDrop > 0) {
      rsSortIndexedFloats(&outlierResid[0], &dropInd[0], numToDrop);
      dropInd.resize(numToDrop);

      //Resort into increasing index order to easily drop elements starting from the end
      std::sort(dropInd.begin(), dropInd.end());
      for (i = numToDrop - 1; i >= 0; i--) {
        mISTargetSSX.erase(mISTargetSSX.begin() + dropInd[i]);
        mISTargetSSY.erase(mISTargetSSY.begin() + dropInd[i]);
        mISTargetDefocus.erase(mISTargetDefocus.begin() + dropInd[i]);
      }

      if (AssessPtsToFitPlane(mISTargetSSX, mISTargetSSY,
        mISTargetDefocus, mess)) {
        return 2;
      }

      lsFit2(&mISTargetSSX[0], &mISTargetSSY[0], &mISTargetDefocus[0],
        (int)mISTargetDefocus.size(), &xCoef, &yCoef, &zIntercept);
    }

  }

  //Measured angle from fit, which is current stage tilt plus specimen tilt
  alpha = atan(yCoef); 

  //Pretilt is the tilt angle that will make the specimen level
  pretilt = (float)mTiltDuringFit - alpha / (float)RADIANS_PER_DEGREE; 
  xPitch = -atan(xCoef / (cos(alpha) - yCoef * sin(alpha))) / (float)RADIANS_PER_DEGREE;

  if (GetDebugOutput('N')) {
    PrintfToLog("xcoef =  %.4f, ycoef =  %.4f, zintercept =  %.4f", xCoef, yCoef, 
      zIntercept);
    PrintfToLog("alpha =  %.4f, pretilt = %.4f, xPitch = %.4f", alpha, pretilt, xPitch);
  }

  return 0;
}

// Find the long axis and aspect ratio of the convex hull of a set of points. 
// This is used to determine if a set of points sufficiently span 2d space.
void CParallelTSHelper::ConvexHullLongAxis(FloatVec ptsX, FloatVec ptsY, 
  float *aspectRatio, float *longAxis, float anglePrecision)
{
  int numPoints = B3DMIN((int)ptsX.size(), (int)ptsY.size());
  FloatVec xHull, yHull;
  int nHull;
  float xcen, ycen;
  Icont *cont;

  xHull.resize(numPoints);
  yHull.resize(numPoints);
  convexBound(&ptsX[0], &ptsY[0], numPoints, 0., 0., &xHull[0], &yHull[0], &nHull, &xcen,
    &ycen, numPoints);

  cont = imodContourNew();
  cont->pts = B3DMALLOC(Ipoint, nHull);
  if (cont->pts) {
    cont->psize = nHull;
    for (int ind = 0; ind < nHull; ind++) {
      cont->pts[ind].x = xHull[ind];
      cont->pts[ind].y = yHull[ind];
      cont->pts[ind].z = 0.;
    }
  }
  imodContourLongAxis(cont, anglePrecision, aspectRatio, longAxis);
  imodContourDelete(cont);
}

// Assess if the provided group of points are sufficient for a 2D least squares fit
int CParallelTSHelper::AssessPtsToFitPlane(FloatVec &ptsX, FloatVec &ptsY, FloatVec &ptsZ, 
  CString &mess)
{
  int numPoints, centerPtInd;
  FloatVec finalPtsX, finalPtsY, finalPtsZ;
  IntVec indexVec, sortedIndexVec;
  float dist, cenX = 0.f, cenY = 0.f;
  float ratio, longAxis, distToCen;

  float minSpanOfPlanePts = 1.f; //TODO get this from parameters or something
  float ISlimit = mShiftManager->GetRegularShiftLimit();

  numPoints = (int)ptsZ.size();
  if (numPoints < MIN_NUM_POINTS_TO_FIT_PLANE) {
    mess.Format("Only %d point(s) added but %d are required to fit to plane",
      numPoints, MIN_NUM_POINTS_TO_FIT_PLANE);
    return -1;
  }

  cenX = (VECTOR_MIN(ptsX) + VECTOR_MAX(ptsX)) / 2.f;
  cenY = (VECTOR_MIN(ptsY) + VECTOR_MAX(ptsY)) / 2.f;

  distToCen = 1.e10;
  for (int i = 0; i < numPoints; i++) {
    indexVec.push_back(i);
    dist = (cenX - ptsX[i]) * (cenX - ptsX[i]) + (cenY - ptsY[i]) * (cenY - ptsY[i]);
    if (i == 0 || dist < distToCen) {
      centerPtInd = i;
      distToCen = dist;
    }
  }

  //Place center point first in final vectors
  sortedIndexVec.resize(1, indexVec[centerPtInd]);
  finalPtsX.resize(1, ptsX[centerPtInd]);
  finalPtsY.resize(1, ptsY[centerPtInd]);

  // Remove points beyond image shift limit
  for (int i = 0; i < numPoints; i++) {
    if (i != centerPtInd) {
      if (fabs(ptsX[i] - ptsX[centerPtInd]) <= ISlimit &&
        fabs(ptsY[i] - ptsY[centerPtInd]) <= ISlimit) {
        sortedIndexVec.push_back(indexVec[i]);
        finalPtsX.push_back(ptsX[i]);
        finalPtsY.push_back(ptsY[i]);
        finalPtsZ.push_back(ptsZ[i]);
      }
    }
  }

  //Check that there are still enough remaining points
  numPoints = (int)sortedIndexVec.size();
  if (numPoints < MIN_NUM_POINTS_TO_FIT_PLANE) {
    mess.Format("Only %d point(s) within IS limits but %d are required to fit to plane",
      numPoints, MIN_NUM_POINTS_TO_FIT_PLANE);
    return -2;
  }

  //Check that points sufficiently span
  ConvexHullLongAxis(finalPtsX, finalPtsY, &ratio, &longAxis);
  if (longAxis / ratio < minSpanOfPlanePts) {
    mess.Format("Given points are not suitable for a least squares fit. "
      "Points should span at least %.1f microns in perpendicular directions",
      minSpanOfPlanePts);
    return 1;
  }

  ptsX = finalPtsX;
  ptsY = finalPtsY;
  ptsZ = finalPtsZ;

  return 0;
}

// Given a set of points, determine if image shifts from the center point to the other 
// points are within IS limits. By default the first item is the starting point, unless 
// sortedIndexVec is given in which case the center-most point will be the starting point.
int CParallelTSHelper::AssessISTargetShiftLimit(IntVec indexVec, CString &mess, 
  IntVec *sortedIndexVec)
{
  int numPoints, newInd;
  FloatVec ptsX, ptsY;
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  float stageZ, cenX, cenY;
  float ISlimit = mShiftManager->GetRegularShiftLimit();

  numPoints = (int)indexVec.size();

  for (int i = 0; i < numPoints; i++) {
    item = itemArr->GetAt(indexVec[i]);
    ptsX.push_back(item->mStageX);
    ptsY.push_back(item->mStageY);
    if (i == 0)
      stageZ = item->mStageZ;
  }

  if (sortedIndexVec) {

    cenX = (VECTOR_MIN(ptsX) + VECTOR_MAX(ptsX)) / 2.f;
    cenY = (VECTOR_MIN(ptsY) + VECTOR_MAX(ptsY)) / 2.f;

    mWinApp->mNavigator->AddItemFromStagePositions(&cenX, &cenY, 1, stageZ,
      mParallelTSDlg->GetFitPlaneGroupID());
    itemArr = mWinApp->mNavigator->GetItemArray();
    newInd = (int)itemArr->GetSize() - 1;
    item = itemArr->GetAt(newInd);
    item->mNote = "[Temp Center Pt]";
    mWinApp->mNavigator->UpdateListString(newInd);
    mWinApp->mNavigator->Redraw();

    //Place center point first in vectors
    sortedIndexVec->resize(1, newInd);
    indexVec.insert(indexVec.begin(), newInd);
    ptsX.insert(ptsX.begin(), cenX);
    ptsY.insert(ptsY.begin(), cenY);
  } else {
    cenX = ptsX[0];
    cenY = ptsY[0];
  }

  // Remove points beyond image shift limit
  for (int i = 1; i < (int)indexVec.size(); i++) {
    if (fabs(ptsX[i] - cenX) <= ISlimit && fabs(ptsY[i] - cenY) <= ISlimit) {
      if (sortedIndexVec)
        sortedIndexVec->push_back(indexVec[i]);
    } else {
      PrintfToLog("WARNING: Point at navigator index %d is beyond image shift limit\n"
        " and therefore unusable.", indexVec[i] + 1);
    }
  }
  return 0;
}

// Assess if the provided points are sufficient for a 2D least squares fit
int CParallelTSHelper::AssessPtsToFitPlane(IntVec indexVec, IntVec &sortedIndexVec, 
  CString &mess)
{
  int numPoints, err;
  FloatVec ptsX, ptsY;
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  float ratio, longAxis;

  float minSpanOfPlanePts = 1.f; //TODO get this from parameters or something
  float ISlimit = mShiftManager->GetRegularShiftLimit();

  numPoints = (int)indexVec.size();
  if (numPoints < MIN_NUM_POINTS_TO_FIT_PLANE) {
    mess.Format("Only %d point(s) added but %d are required to fit to plane",
      numPoints, MIN_NUM_POINTS_TO_FIT_PLANE);
    return -1;
  }

  err = AssessISTargetShiftLimit(indexVec, mess, &sortedIndexVec);
  if (err) {
    return err;
  }

  //Check that there are still enough remaining points
  numPoints = (int)sortedIndexVec.size();
  if (numPoints < MIN_NUM_POINTS_TO_FIT_PLANE) {
    mess.Format("Only %d point(s) within IS limits but %d are required to fit to plane",
      numPoints, MIN_NUM_POINTS_TO_FIT_PLANE);
    return -2;
  }

  for (int i = 0; i < (int)sortedIndexVec.size(); i++) {
    item = itemArr->GetAt(sortedIndexVec[i]);
    ptsX.push_back(item->mStageX);
    ptsY.push_back(item->mStageY);
  }

  //Check that points sufficiently span
  //These should be in specimen coords I think
  ConvexHullLongAxis(ptsX, ptsY, &ratio, &longAxis);
  if (longAxis / ratio < minSpanOfPlanePts) {
    mess.Format("Given points are not suitable for a least squares fit. "
      "Points should span at least %.1f microns in perpendicular directions",
      minSpanOfPlanePts);
    return 1;
  }

  return 0;
}

int CParallelTSHelper::AppendNewTargets(IntVec targetMapIDs, CString &mess)
{
  int ind, jnd, size;
  bool append;

  size = (int)mISTargetPointIDs.size();

  if (size == 0) {
    mStartIndex = 0;
    mISTargetPointIDs = targetMapIDs;
    mAlignedToFirstISTarget = false;
    return 0;
  } 
  mStartIndex = size > 0 ? size - 1 : 0;

  for (ind = 0; ind < (int)targetMapIDs.size(); ind++) {
    append = true;
    for (jnd = 0; jnd < size; jnd++) {
      if (mISTargetPointIDs[jnd] == targetMapIDs[ind]) {
        append = false;
        break;
      }
    }
    if (append) {
      mISTargetPointIDs.push_back(targetMapIDs[ind]);
    }
  }

  return 0;
}

int CParallelTSHelper::GetSavedTargetsInNav(IntVec *navInd, IntVec *indices)
{
  int ind, jnd, numPoints, numSaved;
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  if (indices)
    indices->clear();
  navInd->clear();
  numSaved = (int)mSavedTargetIDs.size();
  for (ind = 0; ind < numSaved; ind++) {
    if (mWinApp->mNavigator->FindItemWithMapID(mSavedTargetIDs[ind], false)) {
      navInd->push_back(mWinApp->mNavigator->GetFoundItem());
    }
  }
  numPoints = (int)navInd->size();
  
  //Sort saved targets by their order in the navigator
  std::sort(navInd->begin(), navInd->end());

  if (indices) {
    for (ind = 0; ind < numPoints; ind++) {
      item = itemArr->GetAt(navInd->at(ind));
      for (jnd = 0; jnd < numSaved; jnd++) {
        if (item->mMapID == mSavedTargetIDs[jnd]) {
          indices->push_back(jnd);
          break;
        }
      }
    }
  }

  return numPoints;
}

// Converts saved IS targets or the given item to a parallel TS item. 
int CParallelTSHelper::ConvertToParTSItem(CString &err, CMapDrawItem *item)
{
  int ind, jnd;
  CMapDrawItem *mapItem;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  IntVec navInd, indices;
  FloatVec delISX, delISY;
  int numPoints, numX, numY, numDef;
  float ptX, ptY;
  EMimageBuffer *imBuf;
  double ISXcen, ISYcen;

  mapItem = mWinApp->mNavigator->FindItemWithMapID(mAreaMapID);
  if (!mapItem) {
    err.Format("The Parallel Tilt Series area map was deleted");
    return -1;
  }

  mParTSParam.prevSectNums.clear();
  mParTSParam.xCoordInArea.clear();
  mParTSParam.yCoordInArea.clear();
  mParTSParam.xShiftInImage.clear();
  mParTSParam.yShiftInImage.clear();

  //Handle a regular pattern set up from multishot params
  if (item) {
    //parTSitemInd = mWinApp->mNavigator->GetCurrentIndex(); //TODO
    if (item->IsPoint()) {
      MultiShotParams *msPars = mNavHelper->GetMultiShotParams();
      mNavHelper->GetNumHolesFromParam(numX, numY, numDef);
      numPoints = mNavHelper->GetNumHolesForItem(item, numDef);
      if (item->mNumXholes > 0)
        numX = item->mNumXholes;
      if (item->mNumYholes > 0)
        numY = item->mNumYholes;
      mWinApp->mParticleTasks->GetHolePositions(delISX, delISY, indices, mMagIndex,
        mWinApp->GetCurrentCamera(), numX, numY, mMappingTilt, item);

      mParTSitem = item;
      mParTSitem->mNumIStargets = (short)numPoints;
      mParTSitem->mIStargetsXY = new float[numPoints * 2];
      mParTSitem->mMagOfIStargets = msPars->holeMagIndex[msPars->doHexArray ? 1 : 0];

      for (ind = 0; ind < numPoints; ind++) {
        mParTSitem->mIStargetsXY[2 * ind] = delISX[ind];
        mParTSitem->mIStargetsXY[2 * ind + 1] = delISY[ind];
      }
    
    //TODO when code it written to make regular pattern with polygon, add here
    //} else if (item->IsPolygon()) {

    } else {
      err.Format("The item must be a point to finalize the area");
      return 1;
    }

  } else {

    if (mParTSopts->flags & PTSFLAG_SKIP_REFINE) {
      if (GetISVectors(mParallelTSDlg->GetTargetGroupID(), err)) {
        return 2;
      }
    } 

    //Handle custom targets places in arbitrary positions
    numPoints = GetSavedTargetsInNav(&navInd, &indices);
    if (numPoints < 2) {
      err.Format("At least two targets are required to create a parallel tilt series item");
      return 3;
    }

    //Center point information
    mParTSitem = itemArr->GetAt(navInd[0]);
    mParTSitem->mNumIStargets = (short)numPoints;
    mParTSitem->mIStargetsXY = new float[numPoints * 2];
    mParTSitem->mMagOfIStargets = (mParTSopts->flags & PTSFLAG_SKIP_REFINE) ? 
      mAreaMapMagInd : mMagIndex;

    // Get IS and Preview map for first item
    if (mParTSopts->extractVirtPrevs == 0)
      mParTSParam.firstPrevMapID = mPreviewMapIDs[indices[0]];
    else
      mParTSParam.firstPrevMapID = 0;
    ISXcen = mISTargetISX[indices[0]];
    ISYcen = mISTargetISY[indices[0]];

    // If using extracts, load the area map
    if (mParTSopts->extractVirtPrevs == 1) {
      mWinApp->mNavigator->DoLoadMap(true, mapItem, -1);
      imBuf = &mWinApp->GetImBufs()[mWinApp->mBufferManager->GetBufToReadInto()];
    }

    for (ind = 0; ind < numPoints; ind++) {
      jnd = indices[ind];
      mParTSitem->mIStargetsXY[2 * ind] = (float)(mISTargetISX[jnd] - ISXcen);
      mParTSitem->mIStargetsXY[2 * ind + 1] = (float)(mISTargetISY[jnd] - ISYcen);
      
      if (mParTSopts->extractVirtPrevs == 0) {
        mParTSParam.prevSectNums.push_back(mPrevMapSectNums[jnd]);
        mParTSParam.xShiftInImage.push_back(mPrevMapShiftX[jnd]);
        mParTSParam.yShiftInImage.push_back(mPrevMapShiftY[jnd]);
      } else if (mParTSopts->extractVirtPrevs == 1) {
        item = itemArr->GetAt(navInd[ind]);
        mWinApp->mMainView->GetItemImageCoords(imBuf, item, ptX, ptY);
        mParTSParam.xCoordInArea.push_back(ptX);
        mParTSParam.yCoordInArea.push_back(ptY);
      }
    }

    //Delete the extra nav items now that all parameters are stored in one item
    mWinApp->mNavigator->m_bCollapseGroups = false;
    for (ind = numPoints - 1; ind > 0; ind--) {
      if (mParTSopts->extractVirtPrevs == 0) {
        item = mWinApp->mNavigator->FindItemWithMapID(mPreviewMapIDs[indices[ind]]);
        if (item) {
          jnd = mWinApp->mNavigator->GetFoundItem();
          mWinApp->mNavigator->ExternalDeleteItem(item, jnd);
        }
      }
      item = mWinApp->mNavigator->FindItemWithMapID(mSavedTargetIDs[indices[ind]], false);
      if (item) {
        jnd = mWinApp->mNavigator->GetFoundItem();
        mWinApp->mNavigator->ExternalDeleteItem(item, jnd);
      }
    }

    ClearTargets(false);

    //Close preview map file if open
    ind = mWinApp->mDocWnd->StoreIndexFromName(mTargetMapFileName);
    if (ind >= 0) {
      mWinApp->mDocWnd->SetCurrentStore(ind);
      mWinApp->mDocWnd->DoCloseFile();
    }

    item = mParTSitem;
  }

  mParTSParam.navID = mWinApp->mNavigator->MakeUniqueID();
  mParTSParam.preTilt = mPretilt;
  mParTSParam.xPitchAngle = mXpitch;
  mParTSParam.mappingTilt = mMappingTilt;

  ParallelTSParam *parTSParam = new ParallelTSParam;
  *parTSParam = mParTSParam;
  mParTSitem->mParallelTSIndex = (int)mWinApp->mNavigator->GetParallelTSArray()->Add(parTSParam);

  mWinApp->mNavigator->FindItemWithMapID(mParTSitem->mMapID, false);
  ind = mWinApp->mNavigator->GetFoundItem();
  mParTSitem->mNote.Format("%d TS targets", mParTSitem->mNumIStargets);
  mWinApp->mNavigator->UpdateListString(ind);
  mWinApp->mNavigator->SetSelectedItem(ind);

  //Check the TS box. If already checked for some reason, uncheck it first.
  if (mWinApp->mNavigator->m_bTiltSeries) {
    mWinApp->mNavigator->m_bTiltSeries = false;
    mWinApp->mNavigator->UpdateData(false);
    mWinApp->mNavigator->OnCheckTiltSeries();
  }
  mWinApp->mNavigator->m_bTiltSeries = true;
  mWinApp->mNavigator->UpdateData(false);
  mWinApp->mNavigator->OnCheckTiltSeries();

  mSavedTSparamIndex = mParTSitem->mTSparamIndex;
  return 0;
}

int CParallelTSHelper::GetTSparamItem(CMapDrawItem *&item)
{
  item = NULL;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  int ind, navInd = -1;

  if (mSavedTSparamIndex >= 0) {
    for (ind = 0; ind < itemArr->GetSize(); ind++) {
      item = itemArr->GetAt(ind);
      if (item->mTSparamIndex == mSavedTSparamIndex) {
        navInd = ind;
        break;
      }
    }
  }
  return navInd;
}

void CParallelTSHelper::UpdateTSParams()
{
  CMapDrawItem *item;
  int ind;
  
  ind = GetTSparamItem(item);
  if (ind < 0) {
    return;
  }
  mWinApp->mNavigator->SetSelectedItem(ind, false);
  mWinApp->mNavigator->OnButTsparams();
  mSavedTSparamIndex = item->mTSparamIndex;
}

// Checks preconditions and applies the adjusting transform to a given set of multishot
// vectors
bool CParallelTSHelper::CanAdjustISVectors(int fromMag, bool multiShot, CString &mess)
{
  int camera = mWinApp->GetCurrentCamera();
  MultiShotParams *params = mNavHelper->GetMultiShotParams();

  if (!params->xformFromMag || !params->adjustingXform.xpx) {
    mess = "No adjustment transform available";
    return false;
  }

  if (multiShot) {
    return mNavHelper->AdjustMultiShotVectors(params,
      B3DCHOICE(params->useCustomHoles, 1, params->doHexArray ? -1 : 0), true, mess) == 0;
  } else {
    mess.Format("Adjustment transform available from %dx to %dx", MagForCamera(camera,
      params->xformFromMag), MagForCamera(camera, params->xformToMag));
    return params->xformFromMag == fromMag;
  }
}

int CParallelTSHelper::GetCenterPtID()
{
  if (mSavedTargetIDs.size() > 0)
    return mSavedTargetIDs[0];
  else
    return -1;
}

// Just compute the image shift vectors without any refinement
int CParallelTSHelper::GetISVectors(int groupID, CString &err)
{
  int ind, numPoints, numAcq;
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  float cenStageX, cenStageY;
  float delX, delY;
  double ISX, ISY;
  CString label, mess;
  IntVec indexVec;
  ScaleMat st2is;
  bool canAdjustIS = mShiftManager->GetFocusISCals()->GetSize() > 0 &&
    mShiftManager->GetFocusMagCals()->GetSize() > 0;

  numPoints = mWinApp->mNavigator->CountItemsInGroup(groupID, label, mess, numAcq, 
    &indexVec);
  if (numPoints < 2) {
    err.Format("At least two targets are required to create a parallel tilt series item");
    return -1;
  }

  st2is = MatMul(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mAreaMapMagInd), 
    mShiftManager->CameraToIS(mAreaMapMagInd));
  if (!st2is.xpx) {
    err.Format("There is no calibration to get from stage to IS coordinates at the "
    "given mag");
    return 1;
  }

  for (ind = 0; ind < numPoints; ind++) {
    item = itemArr->GetAt(indexVec[ind]);
    if (ind == 0) {
      cenStageX = item->mStageX;
      cenStageY = item->mStageY;
      ISX = ISY = 0.f;
    } else {

      // Get IS from center point to the next target
      mShiftManager->ApplyScaleMatrix(st2is, item->mStageX - cenStageX,
        item->mStageY - cenStageY, ISX, ISY);

      //TODO TransferGeneralIS??? I need this also in the regular routine I think???
      /*mWinApp->mShiftManager->TransferGeneralIS(mAreaMapMagInd, delX, delY,
        magInd, ISX, ISY, mWinApp->GetCurrentCamera());*/

      if (mParTSopts->applyAdjustingXform) {
        if (CanAdjustISVectors(mAreaMapMagInd, false, mess)) {
          MultiShotParams *params = mNavHelper->GetMultiShotParams();
          SEMTrace('N', "adj xform xpx=%.4f, xpy=%.4f, ypx=%.4f, ypy=%.4f",
            params->adjustingXform.xpx, params->adjustingXform.xpy,
            params->adjustingXform.ypx, params->adjustingXform.ypy);
          ApplyScaleMatrix(params->adjustingXform, ISX, ISY, delX, delY);
          SEMTrace('N', "IS transformed from (%.4f, %.4f) to (%.4f, %.4f)",
            ISX, ISY, delX, delY);
          ISX = delX;
          ISY = delY;
        }
        else {
          SEMTrace('N', "Adjusting transform not applied: %s", mess);
        }
      }

      if (!mShiftManager->ImageShiftIsOK(ISX, ISY, FALSE)) {
        PrintfToLog("Point at navigator index %d is beyond image shift limit and will "
          "be skipped", indexVec[ind]);
        continue;
      }
    }

    mISTargetISX.push_back(ISX);
    mISTargetISY.push_back(ISY);
    mSavedTargetIDs.push_back(item->mMapID);
  }

  return 0;
}