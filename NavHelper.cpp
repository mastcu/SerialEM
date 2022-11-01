// NavHelper.cpp : Helper class for Navigator for task-oriented routines
//
//
// Copyright (C) 2007 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include ".\NavHelper.h"
#include "SerialEMDoc.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "EMscope.h"
#include "EMbufferManager.h"
#include "FocusManager.h"
#include "ShiftManager.h"
#include "NavigatorDlg.h"
#include "NavAcquireDlg.h"
#include "MapDrawItem.h"
#include "NavFileTypeDlg.h"
#include "NavRotAlignDlg.h"
#include "MultiShotDlg.h"
#include "MontageSetupDlg.h"
#include "BeamAssessor.h"
#include "StateDlg.h"
#include "TiltSeriesParam.h"
#include "TSController.h"
#include "MultiTSTasks.h"
#include "HoleFinderDlg.h"
#include "holefinder.h"
#include "MultiHoleCombiner.h"
#include "MultiCombinerDlg.h"
#include "Utilities\XCorr.h"
#include "Image\KStoreADOC.h"
#include "Utilities\KGetOne.h"
#include "Shared\autodoc.h"
#include "Shared\b3dutil.h"

enum HelperTasks {TASK_FIRST_MOVE = 1, TASK_FIRST_SHOT, TASK_SECOND_MOVE, 
TASK_SECOND_SHOT, TASK_FINAL_SHOT, TASK_RESET_IS, TASK_RESET_SHOT};

// Default order for task list (actions) in acquire
int CNavHelper::mAcqActDefaultOrder[NAA_MAX_ACTIONS + 1] = {
  NAACT_CHECK_DEWARS,
  NAACT_FLASH_FEG,
  NAACT_COMA_FREE,
  NAACT_ASTIGMATISM,
  NAACT_CONDITION_VPP,
  NAACT_HW_DARK_REF,
  NAACT_REFINE_ZLP,
  NAACT_ROUGH_EUCEN,
  NAACT_EUCEN_BY_FOCUS,
  NAACT_CEN_BEAM,
  NAACT_REALIGN_ITEM,
  NAACT_COOK_SPEC,
  NAACT_FINE_EUCEN,
  NAACT_ALIGN_TEMPLATE,
  NAACT_AUTOFOCUS, 
  NAACT_WAIT_DRIFT,
  NAACT_RUN_PREMACRO, 
  NAACT_HOLE_FINDER,
  NAACT_RUN_POSTMACRO, 
 -1};

#define BEFORE_SETUP_EVERYN (NAA_FLAG_ONLY_BEFORE | NAA_FLAG_HAS_SETUP | NAA_FLAG_EVERYN_ONLY)

CNavHelper::CNavHelper(void)
{
  int i;
  SEMBuildTime(__DATE__, __TIME__);

  // Define local array here and copy to real arrays
  NavAcqAction actions[NAA_MAX_ACTIONS] = {
    {"Rough Eucentricity", 0, 0, 1, 15, 40.},
    {"Autocenter Beam", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Realign to Item", NAA_FLAG_ONLY_BEFORE | NAA_FLAG_HAS_SETUP | NAA_FLAG_HERE_ONLY,
    0, 1, 15, 40.},
    {"Cook Specimen", NAA_FLAG_ONLY_BEFORE | NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Fine Eucentricity", 0, 0, 1, 15, 40.},
    {"Eucentricity by Focus", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Autofocus", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Coma-free Alignment", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Correct Astigmatism", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Condition Phase Plate", NAA_FLAG_HAS_SETUP, 0, 1, 15, 40.},
    {"Update Dark Reference", NAA_FLAG_ANY_SITE_OK, 0, 1, 15, 40.},
    {"Wait for Drift", BEFORE_SETUP_EVERYN, 0, 1, 15, 40.},
    {"Align to Template", BEFORE_SETUP_EVERYN, 0, 1, 15, 40.},
    {"Refine ZLP", 0, 0, 1, 15, 40.},
    {"Run Script before Action", NAA_FLAG_ONLY_BEFORE, 0, 1, 15, 40.},
    {"Run Script after Action", NAA_FLAG_AFTER_ITEM, 0, 1, 15,40.},
    {"Flash FEG", NAA_FLAG_ANY_SITE_OK, 0, 1, 15, 40.},
    {"Manage Dewars/Vacuum", NAA_FLAG_HAS_SETUP | NAA_FLAG_EVERYN_ONLY |
    NAA_FLAG_ANY_SITE_OK, 0, 1, 15, 40.},
    {"Hole Finder && Combiner", NAA_FLAG_AFTER_ITEM | NAA_FLAG_HAS_SETUP | 
    NAA_FLAG_EVERYN_ONLY | NAA_FLAG_HERE_ONLY, 0, 1, 15, 40.}
  };

  mWinApp = (CSerialEMApp *)AfxGetApp();
  mDocWnd = mWinApp->mDocWnd;
	mMagTab = mWinApp->GetMagTable();
	mCamParams = mWinApp->GetCamParams();
	mImBufs = mWinApp->GetImBufs();
	mMontParam = mWinApp->GetMontParam();
	mShiftManager = mWinApp->mShiftManager;
  mBufferManager = mWinApp->mBufferManager;
  mFindHoles = new HoleFinder();
  mCombineHoles = new CMultiHoleCombiner();
  mHoleFinderDlg = new CHoleFinderDlg();

  // Copy the default acquire action structures
  for (mNumAcqActions = 0; mNumAcqActions < NAA_MAX_ACTIONS; mNumAcqActions++) {
    if (mAcqActDefaultOrder[mNumAcqActions] < 0)
      break;
    mAcqActCurrentOrder[0][mNumAcqActions] = mAcqActCurrentOrder[1][mNumAcqActions] = 
      mAcqActDefaultOrder[mNumAcqActions];
    mAllAcqActions[0][mNumAcqActions] = actions[mNumAcqActions];
    mAllAcqActions[1][mNumAcqActions] = actions[mNumAcqActions];
  }
  mRealigning = 0;
  mAcquiringDual = false;
  mMaxMarginNeeded = 10.;
  mMinMarginWanted = 5.;
  mMinMarginNeeded = 1.5f;
  mRImaxLMfield = 15.;
  mRegistrationAtErr = -1;
  mStageErrX = mStageErrY = -999.;
  mRImapID = 0;
  mRIdrawnTargetItem = NULL;
  mRIskipCenTimeCrit = 15;
  mRIskipCenErrorCrit = 0.;
  mRIabsTargetX = mRIabsTargetY = 0.;
  mRImaximumIS = 1.;
  mDistWeightThresh = 5.f;
  mRIweightSigma = 3.f;
  mTypeOfSavedState = STATE_NONE;
  mLastTypeWasMont = false;
  mLastMontFitToPoly = false;
  for (i = 0; i < 4; i++) {
    mGridLimits[i] = 0.;
    mTestParams[i] = 0.;
  }
  mStateArray.SetSize(0, 4);
  mStateDlg = NULL;
  mStatePlacement.rcNormalPosition.right = 0;
  mRotAlignDlg = NULL;
  mRotAlignCenter = 0.f;
  mRotAlignRange = 20.f;
  mSearchRotAlign = true;
  mRotAlignPlace.rcNormalPosition.right = 0;
  mMultiShotDlg = NULL;
  mMultiShotPlace.rcNormalPosition.right = 0;
  mHoleFinderPlace.rcNormalPosition.right = 0;
  mMultiCombinerDlg = NULL;
  mMultiCombinerPlace.rcNormalPosition.right = 0;
  mRIdefocusOffsetSet = 0.;
  mRIbeamShiftSetX = mRIbeamShiftSetY = 0.;
  mRIbeamTiltSetX = mRIbeamTiltSetY = 0.;
  mRIalphaSet = -999;
  mRIuseBeamOffsets = false;
  mRITiltTolerance = 2.;
  mRIdefocusChangeLimit = 0.;
  mContinuousRealign = 0;
  mGridGroupSize = 5.;
  mDivideIntoGroups = false;
  mConvertMaps = true;
  mLoadMapsUnbinned = true;
  mWriteNavAsXML = false;
  mTryRealignScaling = true;
  mPlusMinusRIScaling = false;
  mRealignTestOptions = 3;  // TEMPORARY
  mAutoBacklashMinField = 5.f;
  mAutoBacklashNewMap = 1;  // ASK
  mPointLabelDrawThresh = 15;
  mUseLabelInFilenames = false;
  mEnableMultiShot = 0;
  mMultiShotParams.beamDiam = 0.2f;
  mMultiShotParams.spokeRad[0] = 0.5f;
  mMultiShotParams.numShots[0] = 6;
  mMultiShotParams.spokeRad[1] = 0.25f;
  mMultiShotParams.numShots[1] = 3;
  mMultiShotParams.doSecondRing = false;
  mMultiShotParams.doCenter = 1;
  mMultiShotParams.saveRecord = false;
  mMultiShotParams.doEarlyReturn = 0;
  mMultiShotParams.numEarlyFrames = 0;
  mMultiShotParams.extraDelay = 2.;
  mMultiShotParams.adjustBeamTilt = false;
  mMultiShotParams.useIllumArea = false;
  mMultiShotParams.numHoles[0] = 2;
  mMultiShotParams.numHoles[1] = 2;
  mMultiShotParams.skipCornersOf3x3 = false;
  mMultiShotParams.holeISXspacing[0] = 1.;
  mMultiShotParams.holeISYspacing[0] = 1.;
  mMultiShotParams.holeISXspacing[1] = 1.;
  mMultiShotParams.holeISYspacing[1] = 1.;
  mMultiShotParams.inHoleOrMultiHole = 1;
  mMultiShotParams.useCustomHoles = false;
  mMultiShotParams.holeDelayFactor = 1.5f;
  mMultiShotParams.holeMagIndex = 0;
  mMultiShotParams.customMagIndex = 0;
  mMultiShotParams.tiltOfHoleArray = -999.;
  mMultiShotParams.tiltOfCustomHoles = -999.;
  mMultiShotParams.holeFinderAngle = -999.;
  mSkipAstigAdjustment = false;
  mNavAlignParams.maxAlignShift = 1.;
  mNavAlignParams.resetISthresh = 0.2f;
  mNavAlignParams.maxNumResetIS = 0;
  mNavAlignParams.leaveISatZero = false;
  mNavAlignParams.loadAndKeepBuf = 'S' - 'A';
  float thresholds[] = {2.4f, 3.6f, 4.8f};
  mHoleFinderParams.thresholds.insert(mHoleFinderParams.thresholds.begin(), 
    &thresholds[0], &thresholds[3]);
  float sigmas[] = {1.5f, 2.f, 3.f, -3.f};
  mHoleFinderParams.sigmas.insert(mHoleFinderParams.sigmas.begin(), &sigmas[0], 
    &sigmas[4]);
  mHoleFinderParams.spacing = 2.;
  mHoleFinderParams.diameter = 1.;
  mHoleFinderParams.maxError = 0.05f;
  mHoleFinderParams.useBoundary = false;
  mHoleFinderParams.bracketLast = false;
  mHoleFinderParams.lowerMeanCutoff = EXTRA_NO_VALUE;
  mHoleFinderParams.upperMeanCutoff = EXTRA_NO_VALUE;
  mHoleFinderParams.SDcutoff = EXTRA_NO_VALUE;
  mHoleFinderParams.blackFracCutoff = EXTRA_NO_VALUE;
  mHoleFinderParams.showExcluded = true;
  mHoleFinderParams.layoutType = 0;
  float widths[] = {4, 2, 1.5}, increments[] = {3., 1.5, 1.};
  int numCircles[] = {7, 3, 1};
  mHFwidths.insert(mHFwidths.begin(), &widths[0], &widths[3]);
  mHFincrements.insert(mHFincrements.begin(), &increments[0], &increments[3]);
  mHFnumCircles.insert(mHFnumCircles.begin(), &numCircles[0], &numCircles[3]);
  mHFtargetDiamPix = 50.;
  mHFretainFFTs = 1;
  mHFminNumForTemplate = 5;
  mHFfractionToAverage = 0.5f;
  mHFmaxDiamErrFrac = 0.2f;
  mHFavgOutlieCrit = 4.5f;
  mHFfinalPosOutlieCrit = 4.5f;
  mHFfinalNegOutlieCrit = 9;
  mHFfinalOutlieRadFrac = 0.9f;
  mHFpcToPcSameFrac = 0.5f;
  mHFpcToFullSameFrac = 0.5f;
  mHFsubstOverlapDistFrac = 0.;
  mHFusePieceEdgeDistFrac = 0.25f;
  mHFaddOverlapFrac = 0.75f;

  mRIdidSaveState = false;
  mMHCcombineType = COMBINE_ON_IMAGE;
  mMHCenableMultiDisplay = false;
  mMHCturnOffOutsidePoly = true;
  mEditReminderPrinted = false;
  mCollapseGroups = false;
  mRIstayingInLD = false;
  mRIuseCurrentLDparams = false;
  mNav = NULL;
  mExtDrawnOnID = 0;
  mDoingMultipleFiles = false;
  mCurAcqParamIndex = 0;
  mSettingState = false;
  mOKtoUseHoleVectors = false;
}

CNavHelper::~CNavHelper(void)
{
  ClearStateArray();
  delete mFindHoles;
  delete mCombineHoles;
  delete mHoleFinderDlg;
}


void CNavHelper::ClearStateArray(void)
{
  int i;
  StateParams *param;
  for (i = 0; i < mStateArray.GetSize(); i++) {
    param = mStateArray.GetAt(i);
    delete param;
  }
  mStateArray.RemoveAll();
}

void CNavHelper::Initialize(void)
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  InitAcqActionFlags(false);
}

// Initialization when navigator is opened
void CNavHelper::NavOpeningOrClosing(bool open)
{
  if (open) {
    mNav = mWinApp->mNavigator;
    mItemArray = mNav->GetItemArray();
    mTSparamArray = mNav->GetTSparamArray();
    mFileOptArray = mNav->GetFileOptArray();
    mMontParArray = mNav->GetMontParArray();
    mGroupFiles = mNav->GetScheduledFiles();
    mAcqStateArray = mNav->GetAcqStateArray();
    InitAcqActionFlags(true);
    mNav->SetCurAcqParmActions(mCurAcqParamIndex);
  } else
    mNav = NULL;
}

// Make sure bad flags haven't come in from settings and hide irrelevant actions
void CNavHelper::InitAcqActionFlags(bool opening)
{
  int i;
  bool setFlag;
  NavAcqAction *actions;
  bool canHWDR = mWinApp->GetHasK2OrK3Camera() || mWinApp->GetDEcamCount() > 0;
  mAcqActions = &mAllAcqActions[mCurAcqParamIndex][0];
  for (i = 0; i < 2; i++) {
    actions = &mAllAcqActions[i][0];
    if (opening) {
      setOrClearFlags(&actions[NAACT_HW_DARK_REF].flags, NAA_FLAG_ALWAYS_HIDE,
        canHWDR ? 0 : 1);
      if (!canHWDR)
        setOrClearFlags(&actions[NAACT_HW_DARK_REF].flags, NAA_FLAG_RUN_IT, 0);
      setOrClearFlags(&actions[NAACT_HW_DARK_REF].flags, NAA_FLAG_HAS_SETUP,
        mWinApp->GetDEcamCount() > 0 ? 1 : 0);
    } else {
      if (!FEIscope)
        actions[NAACT_CHECK_DEWARS].name = "Manage Nitrogen";

      // Manage Refine ZLP
      setOrClearFlags(&actions[NAACT_REFINE_ZLP].flags, NAA_FLAG_ALWAYS_HIDE,
        mWinApp->ScopeHasFilter() ? 0 : 1);
      if (!mWinApp->ScopeHasFilter())
        setOrClearFlags(&actions[NAACT_REFINE_ZLP].flags, NAA_FLAG_RUN_IT, 0);

      // Manage Flash FEG
      setFlag = !((FEIscope && mScope->GetAdvancedScriptVersion() >=
        ASI_FILTER_FEG_LOAD_TEMP) || (JEOLscope && mScope->GetJeolHasNitrogenClass())) ||
        mScope->GetScopeCanFlashFEG() <= 0;
      setOrClearFlags(&actions[NAACT_FLASH_FEG].flags, NAA_FLAG_ALWAYS_HIDE,
        setFlag ? 1 : 0);
      if (setFlag)
        setOrClearFlags(&actions[NAACT_FLASH_FEG].flags, NAA_FLAG_RUN_IT, 0);

      // Manage Check dewars
      setFlag = !(FEIscope || (JEOLscope && mScope->GetJeolHasNitrogenClass() && 
        mScope->GetDewarVacCapabilities() > 0) || mScope->GetHasSimpleOrigin());
      setOrClearFlags(&actions[NAACT_CHECK_DEWARS].flags, NAA_FLAG_ALWAYS_HIDE,
        setFlag ? 1 : 0);
      if (setFlag)
        setOrClearFlags(&actions[NAACT_CHECK_DEWARS].flags, NAA_FLAG_RUN_IT, 0);

      // Manage Condition VPP
      setFlag = mScope->GetScopeHasPhasePlate() <= 0;
      setOrClearFlags(&actions[NAACT_CONDITION_VPP].flags, NAA_FLAG_ALWAYS_HIDE,
        setFlag ? 1 : 0);
      if (setFlag)
        setOrClearFlags(&actions[NAACT_CONDITION_VPP].flags, NAA_FLAG_RUN_IT, 0);
    }
  }
}

void CNavHelper::UpdateSettings()
{
  InitAcqActionFlags(false);
  if (mNav) {
    InitAcqActionFlags(true);
    mNav->SetCurAcqParmActions(mCurAcqParamIndex);
  }
}

/////////////////////////////////////////////////
/////  REALIGN TO ITEM
/////////////////////////////////////////////////

// Finds the best map for realigning to the given item and sets all the member variables
// for the realign operation
int CNavHelper::FindMapForRealigning(CMapDrawItem * inItem, BOOL restoreState)
{
  int mapInd, magMax, ix, iy, ixPiece, iyPiece, ixSafer, iySafer, xFrame, yFrame, binning; 
  CMapDrawItem *item;
  int maxAligned, alignedMap, maxDrawn, drawnMap, samePosMap, maxSamePos;
  float toEdge, netViewShiftX, netViewShiftY;
  float incLowX, incLowY, incHighX, incHighY, targetX, targetY, borderDist, borderMax;
  double firstISX, firstISY;
  BOOL betterMap, differentMap, stayInLD;
  ScaleMat aMat, bMat;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  float delX, delY, distMin, distMax, firstDelX, firstDelY;
  int magIndex = mScope->GetMagIndex();

  // Look at all maps that point is inside and find one with most distance from a frame 
  // center to the borders, since we will go to the center point for initial localization
  distMax = 0.;
  maxAligned = 0;
  maxDrawn = 0;
  maxSamePos = 0;
  borderMax = 0.;
  for (mapInd = 0; mapInd < mItemArray->GetSize(); mapInd++) {
    item = mItemArray->GetAt(mapInd);
    if (item->IsNotMap() || item->mRegistration != inItem->mRegistration ||
      item->mImported || item->mColor == NO_REALIGN_COLOR)
      continue;

    // Allow low mag mode if the camera field of view is smaller than the limit, which is
    // determined by objective aperture size
    if (item->mMapMagInd < mScope->GetLowestNonLMmag(&mCamParams[item->mMapCamera]) &&
      mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd) *
      B3DMAX(mCamParams[item->mMapCamera].sizeX, mCamParams[item->mMapCamera].sizeY) >
      mRImaxLMfield)
      continue;

    // Get the target which we need to go to at this mag - this will be different from
    // absolute target if the user does not have apply offsets on and it is clear that
    // we will end up at another mag from this item
    targetX = inItem->mStageX;
    targetY = inItem->mStageY;

    // Accumulate the net stage shift needed to work at this mag relative to the final one
    firstDelX = firstDelY = 0.;
    alignedMap = 0;
    differentMap = inItem->IsMap() && inItem->mMapID != item->mMapID;
    incHighX = incHighY = incLowX = incLowY = 0.;
    if (differentMap || ((restoreState || mWinApp->LowDoseMode()) && 
      !(mWinApp->LowDoseMode() && 
      (item->mNetViewShiftX != 0 || item->mNetViewShiftY != 0)))) {

      // Get potential adjustment for mag change if either the input item is a different
      // map from the current item or we will be restoring state at the end or we are in
      // low dose
      mNav->ConvertIStoStageIncrement(item->mMapMagInd, item->mMapCamera, 0., 0.,
        item->mMapTiltAngle, incLowX, incLowY);
      if (differentMap)
        mNav->ConvertIStoStageIncrement(inItem->mMapMagInd, inItem->mMapCamera, 0., 0.,
          item->mMapTiltAngle, incHighX, incHighY);
      else {
        ix = magIndex;

        // if in low dose, align at the mag of the area we are going back to since that
        // constitutes the restored state
        if (mWinApp->LowDoseMode()) {
          iy = mNav->TargetLDAreaForRealign();
          if (iy >= 0 && ldp[iy].magIndex)
            ix = ldp[iy].magIndex;
        }
        mNav->ConvertIStoStageIncrement(ix, mWinApp->GetCurrentCamera(), 0., 0.,
          item->mMapTiltAngle, incHighX, incHighY);
      }

      firstDelX += incHighX - incLowX;
      firstDelY += incHighY - incLowY;
    }

    // And if there is a recorded error when aligning to this map, set flag
    // Also subtract from target to get back to the original stage position that was
    // aligned to in map and presumably marked.
    if (inItem->IsMap() && inItem->mRealignedID == item->mMapID && 
      inItem->mRealignReg == item->mRegistration) {
      targetX -= inItem->mRealignErrX;
      targetY -= inItem->mRealignErrY;
      alignedMap = 1;
    }
    drawnMap = inItem->mDrawnOnMapID == item->mMapID ? 1 : 0;
    samePosMap = (differentMap && inItem->mAtSamePosID > 0 && 
      inItem->mAtSamePosID == item->mAtSamePosID) ? 1 : 0;

    if (InsideContour(item->mPtX, item->mPtY, item->mNumPoints, targetX, targetY)) {

      // Get minimum distance to the border
      borderDist = 100000000.;
      for (ix = 0; ix < item->mNumPoints - 1; ix++) {
        toEdge = PointSegmentDistance(targetX, targetY, item->mPtX[ix], item->mPtY[ix], 
          item->mPtX[ix+1], item->mPtY[ix+1]);
        borderDist = B3DMIN(borderDist, toEdge);
      }

      // Find piece that target is closest to center of, and possibly safer piece that is
      // farther from edge of a montage; distMin is minimum distance from center of that
      // piece to edge of map
      if (DistAndPiecesForRealign(item, targetX, targetY, inItem->mPieceDrawnOn, mapInd,
        aMat, delX, delY, distMin, ixPiece, iyPiece, ixSafer, iySafer, ix, iy, false, 
        xFrame, yFrame, binning))
        continue;

      // Now can determine net view shift and adjust initial stage delta
      stayInLD = CanStayInLowDose(item, xFrame, yFrame, binning, ix, netViewShiftX, 
        netViewShiftY, false);

      // If realigning to a second map at record mag, need to adjust the target by the
      // calibration shift unless staying in low dose, in which case need to clear out
      // the initial image shift.  Sadly, this is all empirical.
      if (differentMap && ((mWinApp->LowDoseMode() || item->mNetViewShiftX || 
        item->mNetViewShiftY) && (inItem->mMapMagInd == ldp[RECORD_CONSET].magIndex ||
        inItem->mMapMagInd == ldp[VIEW_CONSET].magIndex))) {
          if (stayInLD && (item->mNetViewShiftX || item->mNetViewShiftY)) {
            firstDelX = 0.;
            firstDelY = 0.;
          } else if (!stayInLD) {
            targetX += mRIcalShiftX;
            targetY += mRIcalShiftY;
          }

          // If the user changed the view shift and this map was taken at the same 
          // position (and presumably all under the old shift), revise the target by the
          // change in the view shift.  
          if (samePosMap) {
            targetX += mRIviewShiftChangeX;
            targetY += mRIviewShiftChangeY;
          }
      }
      firstDelX += netViewShiftX;
      firstDelY += netViewShiftY;
      SEMTrace('h', "net shift %.2f %.2f  target %.2f %.2f firstDel %.2f %.2f", 
        netViewShiftX, netViewShiftY, targetX, targetY, firstDelX, firstDelY);

      // Convert the net stage shift to an image shift to apply instead
      bMat = MatMul(ItemStageToCamera(item),
        MatInv(mShiftManager->IStoGivenCamera(item->mMapMagInd, item->mMapCamera)));
      firstISX = firstDelX * bMat.xpx + firstDelY * bMat.xpy;
      firstISY = firstDelX * bMat.ypx + firstDelY * bMat.ypy;
 
      // Treat any edge distance larger than this margin equally
      distMin = B3DMIN(distMin, mMaxMarginNeeded);

      // A map is "better" if has a bigger edge distance, or if it is at a higher mag 
      // for the same effective distance, or if the target is more interior for same
      // effective distance
      betterMap = distMin > distMax || (distMin == distMax && magMax < item->mMapMagInd)
        || (distMin == distMax && borderDist > borderMax);

      // Same pos map trumps all: so if this is a same pos map and previous wasn't, accept
      // it if it is better or at least good enough
      // Then apply all the other conditions only if either previous map wasn't at same
      // pos or it was not quite good enough.  In this case:
      // If both or neither are aligned, take the better map
      // If new one is aligned and old one isn't, take it if it is beyond wanted dist
      // or better.  If new one is better and not aligned, take it unless old one is 
      // aligned and beyond wanted distance or if the target is more interior
      // Treat drawn on map same way as aligned to map - maybe it should count for more
      // but there is no criterion for "enough" border to apply here
      SEMTrace('h',"Map %d: spm %d msp %d betm %d dmin %.2f mmw %.2f alm %d mal %d drm %d"
        " mdr %d dmax %f bdis %.2f bmax %.2f", mapInd, samePosMap, maxSamePos, betterMap,
        distMin, mMinMarginWanted, alignedMap, maxAligned, drawnMap, maxDrawn, distMax,
        borderDist, borderMax);
      if ((samePosMap && !maxSamePos && (betterMap || distMin >= mMinMarginWanted)) ||
        ((!maxSamePos || distMin < mMinMarginWanted) && 
        (((alignedMap == maxAligned || drawnMap == maxDrawn) && betterMap) || 
        (((alignedMap && !maxAligned) || (drawnMap && !maxDrawn)) && 
        (betterMap || distMin >= mMinMarginWanted)) ||
        (betterMap && (!(((!alignedMap && maxAligned) || (!drawnMap && maxDrawn)) && 
        distMax >= mMinMarginWanted) ||
        (distMin == distMax && borderDist > borderMax)))))) {
        SEMTrace('h', "Accepted %d as better", mapInd);
          distMax = distMin;
          borderMax = borderDist;
          maxAligned = alignedMap;
          maxDrawn = drawnMap;
          maxSamePos = samePosMap;
          mRIitemInd = mapInd;
          magMax = item->mMapMagInd;
          mRImat = aMat;
          mRIdelX = delX;
          mRIdelY = delY;
          mRIix = ixPiece;
          mRIiy = iyPiece;
          mRIixSafer = ixSafer;
          mRIiySafer = iySafer;
          mRItargetX = targetX;
          mRItargetY = targetY;
          mRIfirstISX = firstISX;
          mRIfirstISY = firstISY;
      }
    }
  }
  
  if (distMax < mMinMarginNeeded)
    return distMax ? 5 : 4;

  return 0;
}

// Find distance to edge of map; for a montage, find the best piece for initial and
// target alignments
int CNavHelper::DistAndPiecesForRealign(CMapDrawItem *item, float targetX, float targetY,
  int pcDrawnOn, int mapInd, ScaleMat &aMat, float &delX, float &delY, float &distMin, 
  int &ixPiece, int &iyPiece, int &ixSafer, int &iySafer, int &imageX, int &imageY,
  bool keepStore, int &xFrame, int &yFrame, int &binning)
{
  KImageStore *imageStore;
  int curStore, ix, iy, xst, yst, imX, imY, idx, idy, ind, midDist;
  int xFrameDel, yFrameDel, finalMidMin, saferMidMin;
  float useWidth, useHeight, loadWidth, loadHeight, edgeDist, finalEdgeDist;
  float saferEdgeMin;
  MontParam *montP;
  float pixel, halfX, halfY;
  bool drawnOnPiece, finalDrawnOn = false, saferDrawnOn = false;

  // Get dimensions.  For montage, get pointers to file so that frame sizes,
  // etc are available
  if (item->mMapMontage) {
    montP = &mMapMontParam;
    if (mNav->AccessMapFile(item, imageStore, curStore, montP, useWidth, useHeight))
      return 1;
    xFrameDel = (montP->xFrame - montP->xOverlap);
    loadWidth = (float)(montP->xFrame + (montP->xNframes - 1) * xFrameDel);
    yFrameDel =  (montP->yFrame - montP->yOverlap);
    loadHeight = (float)(montP->yFrame + (montP->yNframes - 1) * yFrameDel);
    mWinApp->mMontageController->ListMontagePieces(imageStore, montP, 
      item->mMapSection, mPieceSavedAt);
    xFrame = montP->xFrame;
    yFrame = montP->yFrame;
    if (keepStore) {
      mMapMontP = montP;
      mMapStore = imageStore;
      mCurStoreInd = curStore;
    } else {
      if (curStore < 0)
        delete imageStore;
    }
    binning = item->mMontBinning ? item->mMontBinning : montP->binning;
  } else {
    xFrame = item->mMapWidth;
    yFrame = item->mMapHeight;
    loadWidth = useWidth = (float)item->mMapWidth;
    loadHeight = useHeight = (float)item->mMapHeight;
    binning = item->mMapBinning;
  }

  // Get transform to the unrotated full-sized image
  aMat = item->mMapScaleMat;
  aMat.xpx *= loadWidth / useWidth;
  aMat.xpy *= loadWidth / useWidth;
  aMat.ypx *= loadHeight / useHeight;
  aMat.ypy *= loadHeight / useHeight;
  delX = (item->mMapWidth / useWidth) * loadWidth / 2.f - 
    aMat.xpx * item->mStageX - aMat.xpy * item->mStageY;
  delY = (2.f - item->mMapHeight / useHeight) * loadHeight / 2.f - 
    aMat.ypx * item->mStageX - aMat.ypy * item->mStageY;
  pixel = mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd) * 
    binning;

  // For single frame, distance is just half the minimum frame size
  if (!item->mMapMontage) {
    halfX = 0.5f * pixel * loadWidth;
    halfY = 0.5f * pixel * loadHeight;
    distMin = B3DMIN(halfX, halfY);
  } else {

    // For montage, need to figure out the frame that the point is in.  Do this in
    // right-handed coordinates.  Find piece with center closest to point but far
    // enough away from edge
    imX = (int)(aMat.xpx * targetX + aMat.xpy * targetY + delX);
    imY = (int)(loadHeight - (aMat.ypx * targetX + 
      aMat.ypy * targetY + delY));
    if (imX < 0 || imY < 0 || imX > loadWidth || imY > loadHeight)
      return 2;

    finalMidMin = 1000000000;
    saferMidMin = 1000000000;
    saferEdgeMin = -1.;
    for (ix = 0; ix < montP->xNframes; ix++) {
      for (iy = 0; iy < montP->yNframes; iy++) {
        xst = ix * xFrameDel;
        yst = iy * yFrameDel;
        ind = iy + ix * montP->yNframes;
        if (mPieceSavedAt[ind] >= 0) {
          idx = imX - (xst + montP->xFrame / 2);
          idy = imY - (yst + montP->yFrame / 2);

          // Get distance of center from target, and minimum distance to edge
          midDist = idx * idx + idy * idy;
          edgeDist = PieceToEdgeDistance(montP, ix, iy) * pixel;
          drawnOnPiece = ix * montP->yNframes + iy == pcDrawnOn;

          // Take this as a new safer piece either if we haven't gotten far enough
          // from the edge yet and this piece is farther from edge; or if this
          // piece is either beyond the desired margin or as good as the last piece
          // and it is closer to the target than the last piece, as long as the last piece
          // wasn't the one drawn on; or if the edge distance is adequate and this is
          // the piece drawn on
          if ((saferEdgeMin < mMinMarginWanted && edgeDist > saferEdgeMin) ||
            ((edgeDist >= mMinMarginWanted || edgeDist == saferEdgeMin) &&
            midDist < saferMidMin && !saferDrawnOn) ||
            (edgeDist >= mMinMarginWanted && drawnOnPiece)) {
              saferEdgeMin = edgeDist;
              saferMidMin = midDist;
              saferDrawnOn = edgeDist >= mMinMarginWanted && drawnOnPiece;
              ixSafer = ix;
              iySafer = iy;
          }

          if ((imX >= xst && imX <= xst + montP->xFrame &&
            imY >= yst && imY <= yst + montP->yFrame) || drawnOnPiece) {

              // Take this as a new final piece if point is in it and it is
              // closer to the target, or if it is the piece drawn on
              if ((midDist < finalMidMin && !finalDrawnOn) || drawnOnPiece) {
                finalMidMin = midDist;
                finalDrawnOn = drawnOnPiece;
                ixPiece = ix;
                iyPiece = iy;
                finalEdgeDist = edgeDist;
                imageX = imX;
                imageY = imY;
              }
          }
        }
      }
    }

    // Skip if no pieces found containing point
    if (finalMidMin == 1000000000)
      return 2;

    // If the final piece is good enough or somehow no safer piece was identified,
    // use the final one as the safer one
    SEMTrace('h', "Map %d : saferMidMin %.2f  saferEdgeMin %.2f  safer ix,iy %d"
      " %d", mapInd, pixel * sqrt((double)saferMidMin), saferEdgeMin, ixSafer, iySafer);
    SEMTrace('h', "  finalMidMin %.2f drawnOn %s finalEdgeDist %.2f  final ix,iy %d"
      " %d", pixel * sqrt((double)finalMidMin), finalDrawnOn ? "Y" : "N", finalEdgeDist,
      ixPiece, iyPiece);
    if (finalEdgeDist >= mMinMarginWanted || saferEdgeMin < 0) {
      ixSafer = ixPiece;
      iySafer = iyPiece;
      saferEdgeMin = finalEdgeDist;
    }
    distMin = saferEdgeMin;
  }
  return 0;
}

// Align to the given item with image shift after realign to the map it is drawn on,
// which must be a second-round map
int CNavHelper::RealignToDrawnOnMap(CMapDrawItem * item, BOOL restoreState)
{
  int err;
  CMapDrawItem *mapItem = mNav->FindItemWithMapID(item->mDrawnOnMapID, true);
  if (!mapItem)
    return item->mDrawnOnMapID ? 9 : 8;
  mRIdrawnTargetItem = item;
  err = RealignToItem(mapItem, restoreState, 0., 0, 0, false);
  if (err)
    mRIdrawnTargetItem = NULL;
  return err;
}

// Realign to the coordinates in the given item by correlating with maps,
int CNavHelper::RealignToItem(CMapDrawItem *inItem, BOOL restoreState, 
  float resetISalignCrit, int maxNumResetAlign, int leaveZeroIS, BOOL justMoveIfSkipCen)
{
  int i, ix, iy, ind, axes, action; 
  CMapDrawItem *item;
  KImageStore *imageStore;
  float useWidth, useHeight, montErrX, montErrY, itemBackX, itemBackY;
  MontParam *montP;
  ScaleMat aMat;
  NavAcqParams *navParams = mWinApp->GetNavAcqParams(mCurAcqParamIndex);
  CenterSkipData cenSkip;
  float finalX, finalY, stageX, stageY, firstDelX, firstDelY, mapAngle, angle;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  i = FindMapForRealigning(inItem, restoreState);
  mRIContinuousMode = mContinuousRealign;
  mContinuousRealign = 0;
  if (i)
    return i;

  // Save the absolute target stage coordinate that gets adjusted on mag-dependent basis
  mRIabsTargetX = inItem->mStageX;
  mRIabsTargetY = inItem->mStageY;

  item = mItemArray->GetAt(mRIitemInd);
  mSecondRoundID = 0;
  if (inItem->IsMap() && item->mMapID != inItem->mMapID && 
    inItem->mColor != NO_REALIGN_COLOR)
    mSecondRoundID = inItem->mMapID;
  else if (mRIdrawnTargetItem)
    return 10;
  SEMTrace('1', "(Initial) alignment to map %d (%s)", mRIitemInd, (LPCTSTR)item->mLabel);

  // Use the passed-in values for resetting IS and leaving IS at 0 if no second round
  mRIresetISCritForAlign = mSecondRoundID ? 0.f : resetISalignCrit;
  mRIresetISmaxNumAlign = mSecondRoundID ? 0 : maxNumResetAlign;
  mRIresetISleaveZero = mSecondRoundID ? 0 : leaveZeroIS;
  mRIresetISnumDone = 0;
  
  // Get access to map file again
  mMapMontP = &mMapMontParam;
  i = mNav->AccessMapFile(item, imageStore, mCurStoreInd, mMapMontP, useWidth, useHeight);
  if (i)
    return i;
  montP = mMapMontP;
  mMapStore = imageStore;

  // Get rotation matrix of map images due to transformations only
  aMat = ItemStageToCamera(item);
  mRIrotAngle = mNav->RotationFromStageMatrices(aMat, mRImat, mRIinverted);
  mRIrMat = mNav->GetRotationMatrix(mRIrotAngle, mRIinverted);

  // Set up to get target montage error in adjustment coming next
  montErrX = montErrY = 0.;
  mUseMontStageError = false;
  if (item->mMapMontage) {
    mUseMontStageError = (mRealignTestOptions & 2) != 0 &&
      item->mRegistration == item->mOriginalReg;
    mWinApp->mMontageController->ListMontagePieces(mMapStore, mMapMontP, 
      item->mMapSection, mPieceSavedAt);
  }

  // Set the initial component of previous error so it can now be applied to frame 
  // positions, and adjust the target position to be in same shifted coordinates
  // But previous error only works if we are in the same registration...
  mPreviousErrX = mPreviousErrY = 0.;
  mPrevLocalErrX = mPrevLocalErrY = 0.;
  mLocalErrX = mLocalErrY = 0.;
  if (inItem->mRealignedID == item->mMapID && inItem->mRealignReg == item->mRegistration){
    if (mUseMontStageError)
      InterpMontStageOffset(mMapStore, mMapMontP, ItemStageToCamera(item), mPieceSavedAt, 
        mRItargetX - item->mStageX, mRItargetY - item->mStageY, montErrX, montErrY);
    mPreviousErrX = inItem->mRealignErrX - inItem->mLocalRealiErrX - montErrX;
    mPreviousErrY = inItem->mRealignErrY - inItem->mLocalRealiErrY - montErrY;
    mRItargetX += mPreviousErrX;
    mRItargetY += mPreviousErrY;
    mPrevLocalErrX = inItem->mLocalRealiErrX;
    mPrevLocalErrY = inItem->mLocalRealiErrY;
  }
  SEMTrace('1', "Target X, Y: %.3f %.3f", mRItargetX, mRItargetY);

  montErrX = montErrY = 0.;
  stageX = item->mStageX + mPreviousErrX;
  stageY = item->mStageY + mPreviousErrY;
  if (item->mMapMontage) {

    // Get stage position of final piece
    StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIix, mRIiy, finalX, 
      finalY, montErrX, montErrY);

    // Test for whether a montage center exists in its entirety
    ix = (mMapMontP->xNframes - 1) / 2;
    iy = (mMapMontP->yNframes - 1) / 2;
    ind = iy + ix * mMapMontP->yNframes;
    mUseMontCenter = false;
    if (mPieceSavedAt[ind] >= 0 && 
      ((!(montP->xNframes % 2) && !(montP->yNframes % 2) && mPieceSavedAt[ind + 1] >= 0 &&
      mPieceSavedAt[ind + montP->yNframes] >= 0 && 
      mPieceSavedAt[ind + montP->yNframes + 1] >= 0) ||
      ((montP->xNframes % 2) && !(montP->yNframes % 2) && mPieceSavedAt[ind + 1] >= 0) ||
      (!(montP->xNframes % 2) && (montP->yNframes % 2) && 
      mPieceSavedAt[ind + montP->yNframes] >= 0))) {

        // Then see if the target is closer to the montage center than to piece center
        mUseMontCenter = (pow(finalX - mRItargetX, 2) + pow(finalY - mRItargetY, 2)) >
          (pow(stageX - mRItargetX, 2) + pow(stageY - mRItargetY, 2));
    }

    // TODO: DECIDE WHAT TO DO ABOUT THIS
    mUseMontCenter = false;

    // Get stage position of safer piece
    StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIixSafer, mRIiySafer, 
      stageX, stageY, montErrX, montErrY);
    SEMTrace('1', "First round alignment to piece %d %d at %.2f %.2f, montErr %.2f %.2f",
      mRIixSafer, mRIiySafer, stageX, stageY, montErrX, montErrY);
  }

  // Better make sure stage is ready
  if (mScope->WaitForStageReady(5000)) {
    SEMMessageBox("The stage is busy or not ready trying to realign to an item");
    if (mCurStoreInd < 0)
      delete mMapStore;
    return 6;
  }

  // Set up to use scaling in first if option selected and not low dose
  mRItryScaling = mTryRealignScaling && !mWinApp->LowDoseMode();

  // Save state if desired; which will only work by forgetting prior state
  mRIdidSaveState = false;
  if (restoreState) {
    mRIdidSaveState = true;
    ForgetSavedState();
    SaveCurrentState(STATE_MAP_ACQUIRE, 0, item->mMapCamera, 0);
  }

  // This sets mRInetViewShiftX/Y, mRIconSetNum, and mRIstayingInLD, and mrIcalShiftX/Y
  // Here is where low dose gets turned off if so
  PrepareToReimageMap(item, mMapMontP, conSet, TRIAL_CONSET, 
    (restoreState || !(mWinApp->LowDoseMode() && item->mMapLowDoseConSet < 0)) ? 1 : 0);
  if (mRIstayingInLD)
    mRIContinuousMode = 0;   // To avoid this, need to set/restore mode in LD conset
  if (mRIContinuousMode) {
    conSet->mode = CONTINUOUS;
    mCamera->ChangePreventToggle(1);
  }
  if (!mRIstayingInLD)
    SetMapOffsetsIfAny(item);
  mNav->BacklashForItem(item, itemBackX, itemBackY);
  mRIfirstStageX = stageX + montErrX;
  mRIfirstStageY = stageY + montErrY;
  mRItargMontErrX = 0.;
  mRItargMontErrY = 0.;
  if (mUseMontStageError)
    InterpMontStageOffset(mMapStore, mMapMontP, ItemStageToCamera(item), mPieceSavedAt, 
      mRItargetX - (item->mStageX + mPreviousErrX), 
      mRItargetY - (item->mStageY + mPreviousErrY), mRItargMontErrX, mRItargMontErrY);

  firstDelX = firstDelY = 0.;
  action = TASK_FIRST_MOVE;

  // If the mapID doesn't match, clear the center skip list; otherwise look up this
  // position in the list, and if time is within limit, skip first round
  if (mRImapID != item->mMapID)
    mCenterSkipArray.RemoveAll();
  mCenSkipIndex = -1;
  mRIJustMoving = false;
  for (ind = 0; ind < mCenterSkipArray.GetSize(); ind++) {
    if (mRIfirstStageX == mCenterSkipArray[ind].firstStageX && 
      mRIfirstStageY == mCenterSkipArray[ind].firstStageY && 
      mRIfirstISX == mCenterSkipArray[ind].firstISX && 
      mRIfirstISY == mCenterSkipArray[ind].firstISY) {
        mCenSkipIndex = ind;
        if (mWinApp->MinuteTimeStamp() - mCenterSkipArray[ind].timeStamp <=
          mRIskipCenTimeCrit) {
          firstDelX = mCenterSkipArray[ind].baseDelX + mRItargetX + mRItargMontErrX;
          firstDelY = mCenterSkipArray[ind].baseDelY + mRItargetY + mRItargMontErrY;
          action = TASK_SECOND_MOVE;
          if (justMoveIfSkipCen && !mSecondRoundID) {
            mRIJustMoving = true;
            mWinApp->AppendToLog("Just moving to target after skipping first"
              " round alignment to center");
          } else {
            if (LoadForAlignAtTarget(item))
              return 7;
            mWinApp->AppendToLog("Skipping first round alignment to center of map frame");
          }
        }
        break;
    }
  }

  // If there was no match, add this operation to the array with a zero time stamp since
  // it is not completely usable yet
  if (mCenSkipIndex < 0) {
    cenSkip.firstStageX = mRIfirstStageX;
    cenSkip.firstStageY = mRIfirstStageY;
    cenSkip.firstISX = mRIfirstISX;
    cenSkip.firstISY = mRIfirstISY;
    cenSkip.timeStamp = 0;
    mCenterSkipArray.Add(cenSkip);
    mCenSkipIndex = (int)mCenterSkipArray.GetSize() - 1;
  }

  // Go to Z position of item unless this is after doing eucentricity in acquire
  axes = axisXY;
  if (!(mNav->GetAcquiring() && (mNav->GetDidEucentricity() || navParams->skipZmoves)))
    axes |= axisZ;

  // Go to map tilt angle or 0 if current angle differs by more than tolerance
  mapAngle = (float)B3DCHOICE(item->mMapTiltAngle > -1000., item->mMapTiltAngle, 0.);
  angle = (float)mScope->GetTiltAngle();
  if (fabs(angle - mapAngle) > B3DMAX(cos(DTOR * angle), 0.5) * mRITiltTolerance)
    axes |= axisA;

  // Adjust the net view shift down by the amount that will be added in AdjustAndMove so
  // that just it will be applied as an offset to the stage position
  mRInetViewShiftX -= mRIcalShiftX;
  mRInetViewShiftY -= mRIcalShiftY;
  SEMTrace('h', "Subtract %.2f %.2f from net shift", mRIcalShiftX, mRIcalShiftY);

  // 9/14/12: We have to use the aligning-to item's Z here to end up at the right Z
  // Add the view shift just to the coordinates being moved to, all the evaluations
  // of error below depend on comparisons to original first stage pos without it
  SEMTrace('h', "first stage %.2f %.2f  net view shift %.2f %.2f  firstdel %.2f %.2f"
    " VS change %.2f %.2f firstIS %.2f %.2f", mRIfirstStageX ,mRIfirstStageY, 
    mRIviewShiftChangeY, mRIfirstISX, mRIfirstISY);
  mRealigning = 1;
  mNav->AdjustAndMoveStage(mRIfirstStageX + mRInetViewShiftX + firstDelX - 
    mRIviewShiftChangeX, mRIfirstStageY + mRInetViewShiftY + firstDelY - 
    mRIviewShiftChangeY, inItem->mStageZ, axes, itemBackX, itemBackY, 
    mRIfirstISX + mRIleaveISX, mRIfirstISY + mRIleaveISY, mapAngle);

  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_NAV_REALIGN, action, 30000);
  mWinApp->SetStatusText(MEDIUM_PANE, "REALIGNING TO ITEM");
  mCamera->SetRequiredRoll(1);
  mRInumRounds = 0;
  mWinApp->UpdateBufferWindows();
  return 0;
}

// Perform the next task in the sequence
void CNavHelper::RealignNextTask(int param)
{
  CMapDrawItem *item = mItemArray->GetAt(mRIitemInd);
  std::vector<BOOL> pieceDone;
  ScaleMat bMat = ItemStageToCamera(item);
  ScaleMat bInv = MatInv(bMat);
  float stageDelX, stageDelY, peakMax, peakVal, shiftX, shiftY, stageX, stageY, scaling;
  float cenMontErrX, cenMontErrY, itemCenX, itemCenY, imagePixelSize;
  float CCC = 0.;
  float bestDelX, bestDelY, pcDelX, pcDelY, pcShiftX, pcShiftY, pcErrX, pcErrY, overlapMax;
  float pcStageX, pcStageY, pcMontErrX, pcMontErrY, wgtPeakMax, wgtPeakVal, overlap;
  double delISX, delISY;
  float samePosCrit = 0.35f;
  float closeCCCcrit = 0.8f;
  double overlapPow = 0.16667;
  bool betterPeak, setShiftInImage;
  int i, ixNew, iyNew, ixCen, iyCen, ix, iy, ind, indBest;
  CString report;
  if (!mRealigning)
    return;
  if (mRIJustMoving) {
    StopRealigning();
    return;
  }
  if (item->mMapMontage)
    pieceDone.resize(mMapMontP->xNframes * mMapMontP->yNframes);
  itemCenX = item->mStageX + mPreviousErrX;
  itemCenY = item->mStageY + mPreviousErrY;
  cenMontErrX = cenMontErrY = 0.;

  switch (param) {
    case TASK_FIRST_MOVE:
      StartRealignCapture(mRIContinuousMode != 0, TASK_FIRST_SHOT);
      return;
  
    case TASK_FIRST_SHOT:
      scaling = 0.;
      if (item->mMapMontage) {
        for (i = 0; i < mMapMontP->xNframes * mMapMontP->yNframes; i++)
          pieceDone[i] = false;
        ixNew = mRIixSafer;
        iyNew = mRIiySafer;
        ixCen = -1;
        wgtPeakMax = peakMax = -1.e20f;
        bestDelX = bestDelY = overlapMax = 0;

        // Evaluate up to 9 pieces around the central piece
        while (ixNew != ixCen || iyNew != iyCen) {
          ixCen = ixNew;
          iyCen = iyNew;
          for (ix = ixCen - 1; ix <= ixCen + 1; ix++) {
            for (iy = iyCen - 1; iy <= iyCen + 1; iy++) {
              if (ix < 0 || ix >= mMapMontP->xNframes || iy < 0 || 
                iy >= mMapMontP->yNframes)
                continue;
              ind = iy + ix * mMapMontP->yNframes;

              // For a valid piece not done yet, read, rotate and align
              if (!pieceDone[ind] && mPieceSavedAt[ind] >= 0) {
                mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[ind], 1, true);
                mImBufs[1].mBinning = item->mMontBinning;
                if (RotateForAligning(1)) {
                  RealignCleanup(1);
                  return;
                }

                // Align without shifting image
                mShiftManager->AutoAlign(1, -1, false, false, &peakVal, 0., 0., 0., 0., 
                  0., &CCC, &overlap, GetDebugOutput('1'), &pcShiftX, &pcShiftY);
                pieceDone[ind] = true;

                // Compute the change in position that this correlation implies and
                // disparity from original stage position for this move
                StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, ix, iy, 
                  pcStageX, pcStageY, pcMontErrX, pcMontErrY);
                pcErrX = -mImBufs->mBinning * (bInv.xpx * pcShiftX - bInv.xpy * pcShiftY);
                pcErrY = -mImBufs->mBinning * (bInv.ypx * pcShiftX - bInv.ypy * pcShiftY);
                pcDelX = pcErrX + mPrevLocalErrX + mRItargetX + mRItargMontErrX - 
                  (pcStageX + pcMontErrX);
                pcDelY = pcErrY + mPrevLocalErrY + mRItargetY + mRItargMontErrY - 
                  (pcStageY + pcMontErrY);
                pcErrX += mRIfirstStageX - (pcStageX + pcMontErrX), 
                pcErrY += mRIfirstStageY - (pcStageY + pcMontErrY);
                pcErrX = (float)sqrt(pcErrX * pcErrX + pcErrY * pcErrY);
                wgtPeakVal = (float)(CCC * pow((double)overlap, overlapPow)) *
                  mDistWeightThresh / B3DMAX(mDistWeightThresh, pcErrX);

                // If this position is essentially the same as the best one, take it if
                // the CCC is sufficiently higher, reject if it is sufficiently lower, 
                // and otherwise pick the one with the biggest overlap
                if (fabs((double)(pcDelX - bestDelX)) < samePosCrit && 
                  fabs((double)(pcDelY - bestDelY)) < samePosCrit) {
                  if (peakMax <= 1.e-20 || peakMax / CCC < closeCCCcrit)
                    betterPeak = true;
                  else if (CCC / peakMax < closeCCCcrit)
                    betterPeak = false;
                  else
                    betterPeak = overlap > overlapMax;
                } else {

                  // But for different positions, use the weighted CCC to decide
                  betterPeak = wgtPeakVal > wgtPeakMax;
                }
                SEMTrace('n', "Align to %d %d, peak= %.6g   CCC= %.4f  frac= "
                  "%.2f  delXY= %.2f, %.2f  err= %.2f  wgtCCC= %.4f%s", ix, iy, peakVal, 
                  CCC, overlap, pcDelX, pcDelY, pcErrX, wgtPeakVal, betterPeak? "*" : "");

                // If a new maximum is found, record position of peak and get the 
                // stage position and alignment shifts for this piece
                if (betterPeak) {
                  peakMax = CCC;
                  wgtPeakMax = wgtPeakVal;
                  overlapMax = overlap;
                  ixNew = ix;
                  iyNew = iy;
                  indBest = ind;
                  bestDelX = pcDelX;
                  bestDelY = pcDelY;
                  StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, ix, iy, 
                    stageX, stageY, cenMontErrX, cenMontErrY);
                  shiftX = pcShiftX;
                  shiftY = pcShiftY;
                }
              }
            }
          }
        }

        // Restore best shift to A and image in B
        setShiftInImage = true;
        if (ind != indBest || mRItryScaling) {
          mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[indBest], 1, true);
          mImBufs[1].mBinning = item->mMontBinning;
          RotateForAligning(1);
          if (mRItryScaling)
            AlignWithScaling(shiftX, shiftY, scaling);

          // Scaling will be 0 if it failed or wasn't done; either way, restore shifts
          setShiftInImage = scaling == 0.;
        }
        if (setShiftInImage) {
          mImBufs->mImage->setShifts(shiftX, shiftY);
          mImBufs->SetImageChanged(1);
          mNav->Redraw();
        }

      } else {

        // If single frame, read into B, rotate, align and set stage and shift values
        mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 1);
        mImBufs[1].mBinning = item->mMapBinning;
        if (RotateForAligning(1)) {
          RealignCleanup(1);
          return;
        }
        if (mRItryScaling)
          AlignWithScaling(shiftX, shiftY, scaling);
        if (!scaling) {
          mShiftManager->AutoAlign(1, -1, false, false, &peakVal,  0., 0., 0., 0., 0., 
            NULL, NULL, GetDebugOutput('1'));
          mImBufs->mImage->getShifts(shiftX, shiftY);
        }
        stageX = itemCenX;
        stageY = itemCenY;
      }

      // Now get the montage offset interpolated to the target position instead
      if (mUseMontStageError) {
        SEMTrace('1', "Change in montage error from center to target %.2f %.2f",
          mRItargMontErrX - cenMontErrX, mRItargMontErrY - cenMontErrY);
      }

      // Now we have a center stage position and the image alignment shift needed to get
      // there, so determine stage shift needed to get to target
      // Incorporate the change in montage error into this move instead of adding it to
      // the stage position in the move as was done in the initial move
      mStageErrX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      mStageErrY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      mRegistrationAtErr = mNav->GetCurrentRegistration();
      mRImapID = item->mMapID;
      report.Format("Disparity in stage position after aligning to center of map frame: "
        "%.2f %.2f", mStageErrX + mRIfirstStageX - (stageX + cenMontErrX), 
        mStageErrY + mRIfirstStageY - (stageY + cenMontErrY));
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      mCenterSkipArray[mCenSkipIndex].baseDelX = mStageErrX + mPrevLocalErrX - 
        (stageX + cenMontErrX);
      mCenterSkipArray[mCenSkipIndex].baseDelY = mStageErrY + mPrevLocalErrY - 
        (stageY + cenMontErrY);
      stageDelX = mCenterSkipArray[mCenSkipIndex].baseDelX + mRItargetX + mRItargMontErrX;
      stageDelY = mCenterSkipArray[mCenSkipIndex].baseDelY + mRItargetY + mRItargMontErrY;
      mCenterSkipArray[mCenSkipIndex].timeStamp = mWinApp->MinuteTimeStamp();

      // If there is scaling, the distance from center to target has shrunk from the
      // original image, so adjust by the change in this distance
      // Sign verified on 9% shrunk map, + gets to target, - doesn't
      if (scaling) {
        stageDelX += (1.f / scaling - 1.f) * (mRItargetX - stageX);
        stageDelY += (1.f / scaling - 1.f) * (mRItargetY - stageY);
      }
      mRInumRounds++;

      // If shift can be done with image shift, do that then go to second round
      if (sqrt(stageDelX * stageDelX + stageDelY * stageDelY) < mRImaximumIS && 
        !(mRIresetISleaveZero > 0 || mRIresetISmaxNumAlign > 0)) {
          bInv = MatMul(bMat, mShiftManager->CameraToIS(item->mMapMagInd));

          // Subtract the local error which is assumed to be from stage moves
          stageDelX -= mPrevLocalErrX;
          stageDelY -= mPrevLocalErrY;
          delISX = bInv.xpx * stageDelX + bInv.xpy * stageDelY;
          delISY = bInv.ypx * stageDelX + bInv.ypy * stageDelY;
          mScope->IncImageShift(delISX, delISY);
          mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
          report.Format("Aligning to target with image shift equivalent to stage shift:"
            " %.2f %.2f", stageDelX, stageDelY);
          mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

          // Shift the image as well (will need an override for debugging)
          //mImBufs->mImage->getShifts(shiftX, shiftY);
          bMat = mShiftManager->IStoCamera(item->mMapMagInd);
          shiftX = -(float)((bMat.xpx * delISX + bMat.xpy * delISY) / mImBufs->mBinning);
          shiftY = (float)((bMat.ypx * delISX + bMat.ypy * delISY) / mImBufs->mBinning);
          mImBufs->mImage->setShifts(shiftX , shiftY);
          mImBufs->SetImageChanged(1);
          mNav->Redraw();

          // Revise error based on current position and original adjusted target position
          mNav->GetAdjustedStagePos(stageX, stageY, shiftX);
          mStageErrX = (stageX - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftX, 
            mRInetViewShiftX)) - (mRItargetX - mPreviousErrX);
          mStageErrY = (stageY - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftY, 
            mRInetViewShiftY)) - (mRItargetY - mPreviousErrY);
          StartSecondRound();
          return;
      }

      // Otherwise first load images for aligning the next shot, get stage pos of center
      SEMTrace('1', "Moving to target with stage shift: %.2f %.2f",
        stageDelX, stageDelY);
      mRIscaling = scaling;

      if (LoadForAlignAtTarget(item))
        return;

      // Finally, move the stage, relative to the first stage move, adding the view shift
      // because it is not in the adjustment applied to the stage position
      mNav->BacklashForItem(item, shiftX, shiftY);
      mNav->AdjustAndMoveStage(mRIfirstStageX + mRInetViewShiftX + stageDelX - 
        mRIviewShiftChangeX, mRIfirstStageY + mRInetViewShiftY + stageDelY - 
        mRIviewShiftChangeY, 0., axisXY, shiftX, shiftY,
        mRIfirstISX + mRIleaveISX, mRIfirstISY + mRIleaveISY);
      mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_NAV_REALIGN, TASK_SECOND_MOVE,
        30000);
      return;

    case TASK_SECOND_MOVE:
      if (mRIresetISleaveZero > 0 && mRIresetISmaxNumAlign <= 0) 
        StartSecondRound();
      else
        StartRealignCapture(mRIContinuousMode != 0, TASK_SECOND_SHOT);
      return;

    case TASK_SECOND_SHOT:
      imagePixelSize = (float)mImBufs->mBinning *
        mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd);
      mShiftManager->AutoAlign(1, -1, true, false, &peakVal, mExpectedXshift, 
        mExpectedYshift, 0., mRIscaling, 0., NULL, NULL, GetDebugOutput('1'), NULL, NULL, 
        mRIweightSigma / imagePixelSize);
      mImBufs[1].mImage->setShifts(-mExpectedXshift, -mExpectedYshift);
      mImBufs[1].SetImageChanged(1);
      mImBufs->mImage->getShifts(shiftX, shiftY);
      mLocalErrX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      mLocalErrY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      SEMTrace('1', "Aligned to target with image shift of %.1f %.1f pixels\r\n"
        "    local stage err %.2f %.2f, implied total err %.2f %.2f", shiftX, shiftY, 
        mLocalErrX, mLocalErrY, mLocalErrX + mStageErrX, mLocalErrY + mStageErrY);
      mRInumRounds++;

      // Get new stage position and revise and report error, based on original adjusted
      // target position.  The net shift adjustments are sadly empirical
      mNav->GetAdjustedStagePos(stageX, stageY, shiftX);
      mStageErrX = (stageX - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftX, 
        mRInetViewShiftX)) - (mRItargetX - mPreviousErrX);
      mStageErrY = (stageY - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftY, 
        mRInetViewShiftY)) - (mRItargetY - mPreviousErrY);
      report.Format("Disparity in stage position after aligning to target position: "
        "%.2f %.2f  (2nd move error %.2f %.2f)", mStageErrX, mStageErrY,
        mLocalErrX, mLocalErrY);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

      // Evaluate whether center alignment can be re-used
      // Cancel re-use if error is bigger than a fraction of field size or if an error
      // crit is set and the error is above that; 
      // otherwise if an error crit is set, renew the time stamp
      shiftX = (float)sqrt(mLocalErrX * mLocalErrX + mLocalErrY * mLocalErrY);
      if (shiftX > 0.17 * imagePixelSize * 
        B3DMIN(mImBufs->mImage->getWidth(), mImBufs->mImage->getHeight()) ||
        (mRIskipCenErrorCrit > 0. && shiftX > mRIskipCenErrorCrit)) {
          mCenterSkipArray[mCenSkipIndex].timeStamp = 0;
      } else if (mRIskipCenErrorCrit > 0.) {
        mCenterSkipArray[mCenSkipIndex].timeStamp = mWinApp->MinuteTimeStamp();
      }
      if (mRIresetISmaxNumAlign > 0) {
        StartResetISorFinish(item->mMapMagInd);
        return;
      }
      StartSecondRound();
      return;

    case TASK_FINAL_SHOT:
      mShiftManager->AutoAlign(1, 0, 1, false, NULL, 0., 0., 0., 0., 0., NULL, NULL,
        GetDebugOutput('1'));
      mImBufs->mImage->getShifts(shiftX, shiftY);
      SEMTrace('1', "Realigned to map item itself with image shift of %.1f %.1f pixels",
        shiftX, shiftY);
      mRInumRounds++;

      // Get new stage position and revise and report error, based on target position of
      // map itself.  Do this before changing mag to get correct estimate
      mNav->GetAdjustedStagePos(stageX, stageY, cenMontErrX);
      mStageErrX = stageX - mRIabsTargetX;
      mStageErrY = stageY - mRIabsTargetY;
      item = mNav->FindItemWithMapID(mSecondRoundID);
      bMat = ItemStageToCamera(item);
      bInv = MatInv(bMat);
      stageX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      stageY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      report.Format("Disparity in stage position after aligning to map item itself: "
        "%.2f %.2f  (align shift %.2f %.2f)", mStageErrX, mStageErrY, stageX, stageY);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      report.Format("Finished aligning to item in %d rounds", mRInumRounds);

      // If centering on a marked point on this map, now shift to the point and shift the
      // image to match
      if (mRIdrawnTargetItem) {
        stageDelX = mRIdrawnTargetItem->mStageX - item->mStageX;
        stageDelY = mRIdrawnTargetItem->mStageY - item->mStageY;
        bInv = MatMul(bMat, mShiftManager->CameraToIS(item->mMapMagInd));
        mShiftManager->ApplyScaleMatrix(bInv, stageDelX, stageDelY, delISX, delISY);
        mScope->IncImageShift(delISX, delISY);
        mShiftManager->ApplyScaleMatrix(bMat, -stageDelX / mImBufs->mBinning,
          -stageDelY / mImBufs->mBinning, stageX, stageY);
        mImBufs->mImage->setShifts(shiftX + stageX, shiftY - stageY);
        report.Format("Aligned to marked target with image shift (%.1f  %1.f pixels)",
          stageX, stageY);
        mImBufs->SetImageChanged(1);
        mWinApp->mMainView->DrawImage();
      }
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      StopRealigning();
      return;

      // After a reset, take an image if realign needed, or finish up
    case TASK_RESET_IS:
      mRIresetISnumDone++;
      if (mRIresetISneedsAlign)
        StartRealignCapture(mRIContinuousMode != 0, TASK_RESET_SHOT);
      else
        StartSecondRound();
      return;

      // Realign the shot after a reset, then do another operation or finish up
    case TASK_RESET_SHOT:
      mShiftManager->AutoAlign(1, 0);
      StartResetISorFinish(item->mMapMagInd);
      return;
  }
}

// Evaluate whether a realignment or just a reset IS is to be done and start a reset
// in either case, or else finish up
void CNavHelper::StartResetISorFinish(int magInd)
{
  double delISX, delISY, stz;
  float backlashX, backlashY;
  mScope->GetLDCenteredShift(delISX, delISY);
  delISX = mShiftManager->RadialShiftOnSpecimen(delISX, delISY, magInd);
  mRIresetISneedsAlign = delISX >= mRIresetISCritForAlign && mRIresetISnumDone < 
    mRIresetISmaxNumAlign;
  if (mRIresetISneedsAlign || mRIresetISleaveZero > 0) {
    SEMTrace('1', "Starting Reset IS%s, IS dist = %.2f", 
      mRIresetISneedsAlign ? " and realign" : "", delISX);
    mScope->GetStagePosition(delISX, delISY, stz);
    mShiftManager->ResetImageShift(mScope->GetValidXYbacklash(delISX, delISY, backlashX,
      backlashY), false);
    mWinApp->AddIdleTask(TaskRealignBusy, TASK_NAV_REALIGN, TASK_RESET_IS, 0);
    return;
  }
  SEMTrace('1', "Leaving final IS dist = %.2f", delISX); 
  StartSecondRound();
}


// Load the image needed for second round alignment, rotate if needed, and compute 
// expected shift for aligning
int CNavHelper::LoadForAlignAtTarget(CMapDrawItem *item)
{
  float shiftX = item->mStageX + mPreviousErrX;
  float shiftY = item->mStageY + mPreviousErrY;
  int ind, i, retval = 0;
  float tmpX, tmpY;
  ScaleMat bMat = ItemStageToCamera(item);

  for (i = mBufferManager->GetShiftsOnAcquire(); i > 0 ; i--)
    mBufferManager->CopyImageBuffer(i - 1, i);
  if (item->mMapMontage) {
    if (mUseMontCenter) {
      SEMTrace('1', "Second round alignment to montage center");
      retval = mWinApp->mMontageController->ReadMontage(item->mMapSection, mMapMontP,
        mMapStore, true);
    } else {
      ind = mRIiy + mRIix * mMapMontP->yNframes;
      retval = mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[ind], 0, true);
      StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIix, mRIiy, shiftX,
        shiftY, tmpX, tmpY);
      SEMTrace('1', "Second round alignment to piece %d %d at %.2f %.2f", mRIix, 
        mRIiy,  shiftX, shiftY);
    }

  } else {
    retval = mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 0);
  }
  mImBufs[0].mBinning = item->mMapMontage ? item->mMontBinning : item->mMapBinning;

  // Rotate image then get expected shift in aligning to the given image
  if (!retval)
    retval = RotateForAligning(0);
  if (retval) {
    RealignCleanup(1);
    return 1;
  }
  shiftX = mRItargetX - shiftX;
  shiftY = mRItargetY - shiftY;
  mExpectedXshift = (bMat.xpx * shiftX + bMat.xpy * shiftY) / mImBufs->mBinning;
  mExpectedYshift = -(bMat.ypx * shiftX + bMat.ypy * shiftY) / mImBufs->mBinning;
  if (mUseMontCenter)
    mWinApp->mMontageController->AdjustShiftInCenter(mMapMontP, mExpectedXshift,
    mExpectedYshift);
  mWinApp->SetCurrentBuffer(0);
  return 0;
}

// This task is used for checking external operations still busy (just Reset IS so far)
int CNavHelper::TaskRealignBusy(void)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mShiftManager->ResettingIS() ? 1 : 0;
}

void CNavHelper::RealignCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out realigning to Navigator position"));
  StopRealigning();
  mWinApp->ErrorOccurred(error);
}

// To end the realigning and restore conditions
void CNavHelper::StopRealigning(void)
{
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  if (!mRealigning)
    return;

  mRealigning = 0;

  // Close file if not part of open sets
  if (mCurStoreInd < 0 && mMapStore)
    delete mMapStore;

  // Restore state if saved
  RestoreLowDoseConset();
  RestoreMapOffsets();
  if (mRIdidSaveState)
    RestoreSavedState();
  else if (!mRIstayingInLD && mWinApp->mLowDoseDlg.m_bLowDoseMode ? 1 : 0) {
    mSettingState = true;
    mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    mSettingState = false;
  }
  mRIdidSaveState = false;
  mCamera->ChangePreventToggle(-1);
  if (mRIContinuousMode == 1)
    mCamera->StopCapture(0);
  if (mRIContinuousMode > 1)
    conSet->mode = CONTINUOUS;
 
  mCamera->SetRequiredRoll(0);
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mRIdrawnTargetItem = NULL;
  mWinApp->UpdateBufferWindows();
}

// If aligning to a map item after a first round at lower mag, set up conditions
void CNavHelper::StartSecondRound(void)
{
  float useWidth, useHeight;

  CString report;
  ScaleMat aMat;
  KImageStore *imageStore;
  CMapDrawItem *item = mNav->FindItemWithMapID(mSecondRoundID);

  // Undo the image shift imposed in the first round if any
  if (mRIfirstISX || mRIfirstISY)
    mScope->IncImageShift(-mRIfirstISX, -mRIfirstISY);
  if (!item) {
    StopRealigning();
    report.Format("Finished aligning to item in %d rounds", mRInumRounds);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    return;
  }
  if (mCurStoreInd < 0)
    delete mMapStore;
  mMapStore = NULL;
  if (mNav->AccessMapFile(item, imageStore, mCurStoreInd, mMapMontP, useWidth, 
    useHeight)) {
    SEMMessageBox("Failed to access map file of item being aligned to");
    RealignCleanup(1);
    return;
  }
  mMapStore = imageStore;
  mRealigning = 2;

  // Get a new rotation matrix for this item
  aMat = ItemStageToCamera(item);
  mRIrotAngle = mNav->RotationFromStageMatrices(aMat, item->mMapScaleMat, mRIinverted);
  mRIrMat = mNav->GetRotationMatrix(mRIrotAngle, mRIinverted);

  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  RestoreLowDoseConset();
  PrepareToReimageMap(item, mMapMontP, conSet, TRIAL_CONSET, 1);
  if (mRIstayingInLD)
    RestoreMapOffsets();
  else
    SetMapOffsetsIfAny(item);
  if (mRIContinuousMode) {
    mCamera->StopCapture(0);
    if (mRIContinuousMode > 1)
      conSet->mode = CONTINUOUS;
  }

  // Load the image to align to and rotate as usual
  for (int i = mBufferManager->GetShiftsOnAcquire(); i > 0 ; i--)
    mBufferManager->CopyImageBuffer(i - 1, i);
  if (item->mMapMontage) {
    mWinApp->mMontageController->ReadMontage(item->mMapSection, mMapMontP,
      mMapStore, true);
  } else {
     mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 0);
  }
  mImBufs[0].mBinning = item->mMapMontage ? item->mMontBinning : item->mMapBinning;
  if (RotateForAligning(0)) {
    RealignCleanup(1);
    return;
  }

  // Fire off the shot - AGAIN TIMEOUT WAS 300000
  StartRealignCapture(mRIContinuousMode > 1, TASK_FINAL_SHOT);
}

// Start a capture for Realign if not using continuous mode or startContinuous is true,
// and set up the idel task appropriately
void CNavHelper::StartRealignCapture(bool useContinuous, int nextTask)
{
  if (!useContinuous || !mCamera->DoingContinuousAcquire())
    mCamera->InitiateCapture(mRIconSetNum);
  if (useContinuous) {
    mCamera->SetTaskWaitingForFrame(true);
    mWinApp->AddIdleTask(CCameraController::TaskGettingFrame, TASK_NAV_REALIGN, nextTask,
      0);
  } else {
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_NAV_REALIGN, nextTask, 0);
  }
}

// Set scope and filter parameters and set up a control set imaging a map area
void CNavHelper::PrepareToReimageMap(CMapDrawItem *item, MontParam *param, 
                                     ControlSet *conSet, int baseNum, int hideLDoff) 
{
  ControlSet *conSets = mWinApp->GetConSets();
  int  binning, xFrame, yFrame, area, top, left, bottom, right;
  float defocus;
  StateParams stateParam;
  LowDoseParams *ldp;
  CString *names = mWinApp->GetModeNames();
  CameraParameters *camP = mWinApp->GetCamParams() + item->mMapCamera;
  bool filtered = (camP->GIF || mScope->GetHasOmegaFilter()) && item->mMapSlitWidth > 0.;
  StoreMapStateInParam(item, param, baseNum, &stateParam);

  // If coming from Realign to Item, see if we can STAY in low dose instead of hiding it
  mRIstayingInLD = false;
  mRIleaveISX = mRIleaveISY = 0.;
  if (hideLDoff) {
    if (item->mMapMontage) {
      xFrame = param->xFrame;
      yFrame = param->yFrame;
      binning = item->mMontBinning ? item->mMontBinning : param->binning;
    } else {
      xFrame = item->mMapWidth;
      yFrame = item->mMapHeight;
      binning = item->mMapBinning;
    }
    mRIstayingInLD = CanStayInLowDose(item, xFrame, yFrame, binning, mRIconSetNum,
      mRInetViewShiftX, mRInetViewShiftY, true);

    // If staying in low dose, go there now, so that the stage move will be adjusted at
    // the same mag as the anticipated adjustment was computed
    if (mRIstayingInLD) {
      if (mRIuseCurrentLDparams)
        SEMTrace('1', "Map matches low dose %s; staying in low dose with current "
          "parameters but binning %g", (LPCTSTR)names[mRIconSetNum],
          binning / BinDivisorF(camP));
      else
        SEMTrace('1', "Map matches low dose %s; staying in low dose but with exp %.3f "
          "intensity %.5f bin %g", (LPCTSTR)names[mRIconSetNum], item->mMapExposure, 
          item->mMapIntensity, binning / BinDivisorF(camP));

      // Save the conSet and modify it to the map parameters
      mSavedConset = conSets[mRIconSetNum];
      /*StateCameraCoords(item->mMapCamera, xFrame, yFrame, binning, 
        conSets[mRIconSetNum].left, conSets[mRIconSetNum].right, 
        conSets[mRIconSetNum].top, conSets[mRIconSetNum].bottom);*/
      top = mSavedConset.top / binning;
      left = mSavedConset.left / binning;
      bottom = mSavedConset.bottom / binning;
      right = mSavedConset.right / binning;
      mCamera->AdjustSizes(xFrame, camP->sizeX, camP->moduloX, left, right, yFrame,
        camP->sizeY, camP->moduloY, top, bottom, binning, item->mMapCamera);
      conSets[mRIconSetNum].top = top * binning;
      conSets[mRIconSetNum].left = left * binning;
      conSets[mRIconSetNum].bottom = bottom * binning;
      conSets[mRIconSetNum].right = right * binning;
      conSets[mRIconSetNum].binning = binning;

      if (!mRIuseCurrentLDparams) {
        conSets[mRIconSetNum].exposure = item->mMapExposure;
        conSets[mRIconSetNum].saveFrames = 0;
        conSets[mRIconSetNum].mode = SINGLE_FRAME;

        // Also save the low dose params and use map beam and slit conditions
        area = mCamera->ConSetToLDArea(item->mMapLowDoseConSet);
        ldp = mWinApp->GetLowDoseParams() + area;
        mRIsavedLDparam = *ldp;
        ldp->intensity = item->mMapIntensity;
        ldp->spotSize = item->mMapSpotSize;
        if (filtered) {
          ldp->slitIn = item->mMapSlitIn;
          ldp->slitWidth = item->mMapSlitWidth;
        }
      }
      mScope->GotoLowDoseArea(mCamera->ConSetToLDArea(mRIconSetNum));
      mRIareaDefocusChange = 0.;
      if (!mRIuseCurrentLDparams && IS_SET_VIEW_OR_SEARCH(mRIconSetNum)) {
        defocus = mScope->GetLDViewDefocus(area);
        if (fabs(item->mDefocusOffset - defocus) > 0.1) {
          mRIareaDefocusChange = item->mDefocusOffset - defocus;
          mScope->SetLDViewDefocus(item->mDefocusOffset, area);
          mScope->IncDefocus(mRIareaDefocusChange);
        }
      }
      return;
    }

    // If leaving LD, set the base IS to leave on stage moves
    ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mRIleaveISX = ldp->ISX;
    mRIleaveISY = ldp->ISY;
  }
  SEMTrace('I', "PrepareToReimageMap set intensity to %.5f  %.3f%%", stateParam.intensity
    , mScope->GetC2Percent(stateParam.spotSize, stateParam.intensity, 
      stateParam.probeMode));
  SetStateFromParam(&stateParam, conSet, baseNum, hideLDoff);
}

// Determine if the realign routine can stay in low dose
bool CNavHelper::CanStayInLowDose(CMapDrawItem *item, int xFrame, int yFrame, int binning,
  int &setNum, float &retShiftX, float &retShiftY, bool forReal)
{
  int mapLDconSet = item->mMapLowDoseConSet;
  int area = mCamera->ConSetToLDArea(mapLDconSet);
  double netX, netY, fullX, fullY;
  bool match = true;
  LowDoseParams *ldp;
  CameraParameters *camP = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  bool hasAlpha = JEOLscope && !mScope->GetHasNoAlpha();
  setNum = TRACK_CONSET;

  // Start out with the item's net view/search shifts in these, but if they are non-zero,
  // replace them with the current net shift (as a stage move) and keep track of how much
  // the view shift changed
  retShiftX = item->mNetViewShiftX;
  retShiftY = item->mNetViewShiftY;
  mRIviewShiftChangeX = mRIviewShiftChangeY = 0;
  if (retShiftX || retShiftY) {
    mWinApp->mLowDoseDlg.GetNetViewShift(netX, netY, area);
    SimpleIStoStage(item, netX, netY, retShiftX, retShiftY);
    mRIviewShiftChangeX = retShiftX - item->mNetViewShiftX;
    mRIviewShiftChangeY = retShiftY - item->mNetViewShiftY;
  }

  // Compute the amount of mag offset calibration shift that the requested stage position
  // will be adjusted by so that it can be added onto the requested position, since the
  // view shifts applied when going to low dose area incorporate the calibration as well
  mRIcalShiftX = mRIcalShiftY = 0.;
  if ((retShiftX || retShiftY) && !mScope->GetApplyISoffset()) {
    mWinApp->mLowDoseDlg.GetNetViewShift(netX, netY, area);
    mWinApp->mLowDoseDlg.GetFullViewShift(fullX, fullY, area);
    fullX -= netX;
    fullY -= netY;
    SimpleIStoStage(item, fullX, fullY, mRIcalShiftX, mRIcalShiftY);
  }

  // Now if in low dose, look for matching set
  if (mWinApp->LowDoseMode() && mapLDconSet >= 0 && mapLDconSet <= SEARCH_CONSET) {
    ldp = mWinApp->GetLowDoseParams() + area;

    // Check mag/spot/alpha and filter
    if (item->mMapCamera != mWinApp->GetCurrentCamera() || 
      item->mMapMagInd != ldp->magIndex || 
      (item->mMapProbeMode >= 0 && item->mMapProbeMode != ldp->probeMode) || 
      (hasAlpha && (item->mMapAlpha >= 0 || ldp->beamAlpha >= 0) &&  
      item->mMapAlpha != ldp->beamAlpha) || item->mMapBinning <= 0) {
        if (forReal)
          SEMTrace('1', "Leaving LD, map bin %d map/LD cam %d/%d mag %d/%d"
            " probe %d/%d alpha %d/%.0f", 
            item->mMapMontage ? item->mMontBinning : item->mMapBinning, item->mMapCamera, 
            mWinApp->GetCurrentCamera(), item->mMapMagInd, ldp->magIndex, 
            item->mMapProbeMode >= 0 ? item->mMapProbeMode : -1, 
            item->mMapProbeMode >= 0 ? ldp->probeMode : -1, 
            hasAlpha ? item->mMapAlpha : -1, hasAlpha ? ldp->beamAlpha : -1.);
        match = false;
    }

    // Check for defocus offset change if a limit is set as a trap-door
    if (match && IS_SET_VIEW_OR_SEARCH(item->mMapLowDoseConSet) && 
      mRIdefocusChangeLimit > 0. && fabs(item->mDefocusOffset - 
      mScope->GetLDViewDefocus(area)) > mRIdefocusChangeLimit) {
        if (forReal)
          SEMTrace('1', "Leaving LD, defocus offset map %.1f  LD %.1f",
            item->mDefocusOffset, mScope->GetLDViewDefocus(area));
        match = false;
    }

    // Found a match!  Zero out the returned shifts
    if (match) {
      setNum = item->mMapLowDoseConSet;
      retShiftX = 0.;
      retShiftY = 0.;
      return true;
    }
  }

  // But if not staying in low dose, need to adjust the returned shift to be a full view
  // shift by adding the calibration back on
  retShiftX += mRIcalShiftX;
  retShiftY += mRIcalShiftY;
  return false;
}

// Set some special items if appropriate for the given item: defocus offset, alpha change,
// and beam shift and tilt if enable by properties.
void CNavHelper::SetMapOffsetsIfAny(CMapDrawItem *item)
{

  // First restore existing offsets
  RestoreMapOffsets();

  // Handle defocus offset
  mRIdefocusOffsetSet = item->mDefocusOffset;
  if (item->mDefocusOffset)
    mScope->IncDefocus(item->mDefocusOffset);

  // Handle alpha change
  if (item->mMapAlpha >= 0) {
    mRIalphaSaved = mScope->GetAlpha();
    if (mRIalphaSaved >= 0) {
      mScope->ChangeAlphaAndBeam(mRIalphaSaved, item->mMapAlpha);
      mRIalphaSet = item->mMapAlpha;
    }
  }

  // Handle a beam shift/tilt after the alpha change, as in low dose
  if (mRIuseBeamOffsets) {
    mRIbeamShiftSetX = item->mViewBeamShiftX;
    mRIbeamShiftSetY = item->mViewBeamShiftY;
    if (mRIbeamShiftSetX || mRIbeamShiftSetY)
      mScope->IncBeamShift(mRIbeamShiftSetX, mRIbeamShiftSetY);
    mRIbeamTiltSetX = item->mViewBeamTiltX;
    mRIbeamTiltSetY = item->mViewBeamTiltY;
    if (mRIbeamTiltSetX || mRIbeamTiltSetY)
      mScope->IncBeamTilt(mRIbeamTiltSetX, mRIbeamTiltSetY);
  }
}

// Restore any special offsets or changes set by SetMapOffsetsIfAny
void CNavHelper::RestoreMapOffsets()
{

  // Restore defocus offset
  if (mRIdefocusOffsetSet)
    mScope->IncDefocus(-mRIdefocusOffsetSet);
  mRIdefocusOffsetSet = 0.;

  // Then restore alpha.  Do alpha and other beam changes in the same order as when
  // changing low dose areas, since that is what this mimics
  SEMTrace('b', "RestoreMapOffsets restore alpha %d to %d",mRIalphaSet + 1, 
      mRIalphaSaved + 1);
  if (mRIalphaSet >= 0 && mRIalphaSaved >= 0)
    mScope->ChangeAlphaAndBeam(mRIalphaSet, mRIalphaSaved);
  mRIalphaSet = -999;

  // Restore beam shift/tilt
  SEMTrace('b', "RestoreMapOffsets restore bs %f %f  bt %f %f", mRIbeamShiftSetX, 
    mRIbeamShiftSetY, mRIbeamTiltSetX, mRIbeamTiltSetY);
  if (mRIbeamShiftSetX || mRIbeamShiftSetY)
    mScope->IncBeamShift(-mRIbeamShiftSetX, -mRIbeamShiftSetY);
  if (mRIbeamTiltSetX || mRIbeamTiltSetY)
    mScope->IncBeamTilt(-mRIbeamTiltSetX, -mRIbeamTiltSetY);
  mRIbeamShiftSetX = mRIbeamShiftSetY = 0.;
  mRIbeamTiltSetX = mRIbeamTiltSetY = 0.;
}

/////////////////////////////////////////////////
/////  STATE SAVING, SETTING, RESTORING
/////////////////////////////////////////////////

// Restore state if it was saved
void CNavHelper::RestoreSavedState(void)
{
  int area, destCam, ind, setNum, curCam = mWinApp->GetCurrentCamera();
  ControlSet *conSets;
  LowDoseParams *ldp;
  LowDoseParams ldsaParams;
  if (mTypeOfSavedState == STATE_NONE)
    return;
  for (ind = (int)mSavedStates.GetSize() - 1; ind >= 0; ind--) {
    area = AreaFromStateLowDoseValue(&mSavedStates[ind], &setNum);
    destCam = mSavedStates[ind].camForParams < 0 ?
      mSavedStates[ind].camIndex : mSavedStates[ind].camForParams;
    if (ind) {

      // For added LD states:  Restore the LD Params for THIS camera
      // First set the existing params if those are master and in this area
      ldp = mWinApp->GetLDParamsForCamera(destCam);
      if (mWinApp->LowDoseMode() && mScope->GetLowDoseArea() == area && 
        ldp == mWinApp->GetLowDoseParams()) {
        ldsaParams = ldp[area];
        mScope->SetLdsaParams(&ldsaParams);
        mScope->GotoLowDoseArea(area);

        // But if this is trial or focus and the axis position is changing, have to go
        // to Record to avoid changing the position while in the area 
        if ((area == FOCUS_CONSET || area == TRIAL_CONSET) &&
          (fabs(ldp[area].ISX - mSavedStates[ind].ldParams.ISX) > 1.e-4 ||
            fabs(ldp[area].ISY - mSavedStates[ind].ldParams.ISY) > 1.e-4))
          mScope->GotoLowDoseArea(RECORD_CONSET);
      }
      ldp[area] = mSavedStates[ind].ldParams;
      if (mSavedStates[ind].ldDefocusOffset > -9990.) {
        mScope->SetLDViewDefocus(mSavedStates[ind].ldDefocusOffset, area);
        mWinApp->mLowDoseDlg.UpdateSettings();
      }

      // Restore the consets for current cam or master sets; the routine will copy to
      // master if it is current cam
      if (destCam == mWinApp->GetCurrentCamera())
        conSets = mWinApp->GetConSets();
      else
        conSets = mWinApp->GetCamConSets() + destCam * MAX_CONSETS;
      SetConsetsFromParam(&mSavedStates[ind], conSets + setNum, setNum);
    } else {

      // For the original state: it will switch to the camera before the restore
      if (destCam == mSavedStates[ind].camIndex)
        conSets = mWinApp->GetConSets();
      else
        conSets = mWinApp->GetCamConSets() + destCam * MAX_CONSETS;
      SetStateFromParam(&mSavedStates[0], conSets + setNum, setNum);
      if (mSavedStates[0].lowDose && mSavedLowDoseArea >= 0)
        mScope->GotoLowDoseArea(mSavedLowDoseArea);
    }
  }
  SEMTrace('I', "RestoreSavedState restored intensity to %.5f", mSavedStates[0].lowDose ?
    mSavedStates[0].ldParams.intensity : mSavedStates[0].intensity);
  ForgetSavedState();
}

// Set the scope and camera parameter state to that used to acquire a map item
int CNavHelper::SetToMapImagingState(CMapDrawItem * item, bool setCurFile, int hideLDoff)
{
  int camera, curStore, err, retval = 0;
  float width, height;
  ControlSet tmpSet;
  ControlSet *masterSets = mWinApp->GetCamConSets();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  KImageStore *imageStore;
  MontParam *masterMont = mWinApp->GetMontParam();

  if (item->IsNotMap() || item->mImported)
    return 1;
  mMapMontP = NULL;
  SaveCurrentState(STATE_MAP_ACQUIRE, 0, item->mMapCamera, 0);
  mMapStateItemID = item->mMapID;
  mSavedStoreName = "";
  mNewStoreName = "";
  if (setCurFile) {
    mMapMontP = &mMapMontParam;

    // Open the file or get the store # if it is currently open
    err = mNav->AccessMapFile(item, imageStore, curStore, mMapMontP, width, height, 
      setCurFile);
    if (err) {
      SEMMessageBox("The file for that map item can no longer be accessed", MB_EXCLAME);
      mMapMontP = NULL;
      curStore = -1;
      retval = 1;
    } else {

      // Save current name if we are going to a new file
      if (mWinApp->mStoreMRC && (curStore >= 0 || mDocWnd->GetNumStores() < MAX_STORES))
        mSavedStoreName = mWinApp->mStoreMRC->getName();

      // Change to the file if it is already open
      if (curStore >= 0 && mDocWnd->GetCurrentStore() != curStore)
        mDocWnd->SetCurrentStore(curStore);
    
      // Otherwise define it as new open store if there is enough space
      if (curStore < 0) {
        if (mDocWnd->GetNumStores() < MAX_STORES) {
          mDocWnd->LeaveCurrentFile();
          mWinApp->mStoreMRC = imageStore;
          mNewStoreName = imageStore->getName();
          if (item->mMapMontage) {
            *masterMont = *mMapMontP;
            mWinApp->SetMontaging(true);
          }
          mDocWnd->AddCurrentStore();
        } else {
          SEMMessageBox("The file for that map item cannot be opened for saving.\n"
            "There are too many other files open right now", MB_EXCLAME);
          delete imageStore;
          retval = 1;
        }
      }
    }
  }

  tmpSet = *conSet;
  PrepareToReimageMap(item, mMapMontP, &tmpSet, RECORD_CONSET, hideLDoff);
  camera = mWinApp->GetCurrentCamera();
  /*if (conSet->binning != tmpSet.binning || conSet->exposure != tmpSet.exposure ||
    conSet->drift != tmpSet.drift)
    mWinApp->AppendToLog("\r\nCamera parameters were changed in setting to map item "
    "state", LOG_OPEN_IF_CLOSED); */
  *conSet = tmpSet;
  masterSets[camera * MAX_CONSETS + RECORD_CONSET] = tmpSet;
  UpdateStateDlg();
  return retval;
}

// Restore state from setting to a map state
int CNavHelper::RestoreFromMapState(void)
{
  ControlSet *masterSets = mWinApp->GetCamConSets();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  int camera, store;
  RestoreLowDoseConset();
  if (mTypeOfSavedState != STATE_MAP_ACQUIRE)
    return 1;
  RestoreSavedState();
  camera = mWinApp->GetCurrentCamera();
  masterSets[camera * MAX_CONSETS + RECORD_CONSET] = *conSet;

  // Close new file if it was opened just for this purpose
  if (!mNewStoreName.IsEmpty()) {
    store = mDocWnd->StoreIndexFromName(mNewStoreName);
    if (store >= 0) {
      mDocWnd->SetCurrentStore(store);
      mDocWnd->DoCloseFile();
    }
  }

  // Restore original file if it is still open
  if (!mSavedStoreName.IsEmpty()) {
    store = mDocWnd->StoreIndexFromName(mSavedStoreName);
    if (store >= 0)
      mDocWnd->SetCurrentStore(store);
  }
  UpdateStateDlg();
  return 0;
}

// Store the current scope state or a set of low dose params into the state param
// 0 for non-lowdose, 1 for current state, or < 0 for specific state
void CNavHelper::StoreCurrentStateInParam(StateParams *param, int lowdose, 
  int saveLDfocusPos, int camNum, int saveTargOffs)
{
  LowDoseParams *ldp;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  ControlSet *conSets;
  double shiftX, shiftY;
  int curCam = mWinApp->GetCurrentCamera();
  int ldInd = RECORD_CONSET;
  int montInd = MONT_USER_CONSET;
  if (camNum < 0)
    camNum = curCam;
  if (lowdose < 0 && !param->montMapConSet) {
    ldInd = -1 - lowdose;
  } else if (lowdose > 0) {
    if (mScope->GetLowDoseArea() >= 0 && !param->montMapConSet)
      ldInd = mScope->GetLowDoseArea();
    lowdose = -1 - ldInd;
  }

  // Get the right LD params for the camera, and the right consets too
  ldp = mWinApp->GetLDParamsForCamera(camNum) + ldInd;
  if (ldInd == SEARCH_AREA)
    ldInd = mWinApp->GetUseViewForSearch() ? VIEW_CONSET : SEARCH_CONSET;
  if (ldInd == RECORD_CONSET && param->montMapConSet) {
    ldInd = MONT_USER_CONSET;
    montInd = RECORD_CONSET;
  }

  // Should it do this or just access the master?  Map state accesses the master
  if (camNum == curCam)
    conSets = mWinApp->GetConSets();
  else
    conSets = mWinApp->GetCamConSets() + camNum * MAX_CONSETS;
  param->camForParams = camNum;
  param->lowDose = lowdose;
  if (lowdose) {
    param->ldParams = *ldp;
    param->probeMode = ldp->probeMode;
    if (IS_AREA_VIEW_OR_SEARCH(ldInd)) {
      mWinApp->mLowDoseDlg.GetFullViewShift(shiftX, shiftY, ldInd);
      param->ldShiftOffsetX = (float)shiftX;
      param->ldShiftOffsetY = (float)shiftY;
    }
  } else {
    param->magIndex = mScope->GetMagIndex();
    param->intensity = mScope->GetIntensity();
    param->spotSize = mScope->GetSpotSize();
    param->probeMode = mScope->ReadProbeMode();
    param->beamAlpha = mScope->GetAlpha();

    // Store filter parameters unconditionally here; they will be set conditionally
    param->slitIn = filtParam->slitIn;
    param->slitWidth = filtParam->slitWidth;
    param->energyLoss = filtParam->energyLoss;
    param->zeroLoss = filtParam->zeroLoss;
  }

  // Save either the defocus target or the LD defocus offset - the caller knows what kind
  // of state has this in it
  if (saveTargOffs < 0)
    param->targetDefocus = mWinApp->mFocusManager->GetTargetDefocus();
  if (saveTargOffs > 0 && lowdose)
    param->ldDefocusOffset = mScope->GetLDViewDefocus(saveTargOffs - 1);
  param->camIndex = mWinApp->GetCurrentCamera();
  param->binning = conSets[ldInd].binning;
  param->exposure = conSets[ldInd].exposure;
  param->drift = conSets[ldInd].drift;
  param->shuttering = conSets[ldInd].shuttering;
  param->K2ReadMode = conSets[ldInd].K2ReadMode;
  param->singleContMode = conSets[ldInd].mode;
  param->saveFrames = conSets[ldInd].saveFrames;
  param->doseFrac = conSets[ldInd].doseFrac;
  param->frameTime = conSets[ldInd].frameTime;
  param->processing = conSets[ldInd].processing;
  param->alignFrames = conSets[ldInd].alignFrames;
  param->useFrameAlign = conSets[ldInd].useFrameAlign;
  param->faParamSetInd = conSets[ldInd].faParamSetInd;

  param->xFrame = (conSets[ldInd].right - conSets[ldInd].left) / param->binning;
  param->yFrame = (conSets[ldInd].bottom - conSets[ldInd].top) / param->binning;

  SaveLDFocusPosition(lowdose ? saveLDfocusPos : 0, param->focusAxisPos, param->rotateAxis
    , param->axisRotation, param->focusXoffset, param->focusYoffset, true);
  param->readModeView = lowdose ? -1 : conSets[VIEW_CONSET].K2ReadMode;
  param->readModeFocus = lowdose ? -1 : conSets[FOCUS_CONSET].K2ReadMode;
  param->readModeTrial = lowdose ? -1 : conSets[TRIAL_CONSET].K2ReadMode;
  param->readModePrev = lowdose ? -1 : conSets[PREVIEW_CONSET].K2ReadMode;
  param->readModeSrch = (lowdose || mWinApp->GetUseViewForSearch()) ? -1 : 
    conSets[SEARCH_CONSET].K2ReadMode;
  param->readModeMont = (lowdose || mWinApp->GetUseRecordForMontage()) ? -1 : 
    conSets[montInd].K2ReadMode;
}

// Save the low dose focus area for the current camera in the given parameters
// The returned axisPos is the full R to F offset, not the net offset stored in low dose
// parameters
void CNavHelper::SaveLDFocusPosition(int saveIt, float &axisPos, BOOL &rotateAxis, 
  int &axisRotation, int &xOffset, int &yOffset, bool traceIt)
{
  ControlSet *conSet = mWinApp->GetConSets() + FOCUS_CONSET;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + 
    (saveIt > 1 ? TRIAL_CONSET : FOCUS_CONSET);
  LowDoseParams *ldrec = mWinApp->GetLowDoseParams() + RECORD_CONSET;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  xOffset = yOffset = 0;
  axisPos = EXTRA_NO_VALUE;
  rotateAxis = false;
  axisRotation = 0;
  if (saveIt) {

    // Make sure axis position is up to date in ld params
    if (mWinApp->LowDoseMode()) {
      if (ldp->magIndex)
        ldp->axisPosition = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(ldp->magIndex,
          ldp->ISX, ldp->ISY);
      if (mWinApp->mLowDoseDlg.ShiftsBalanced() && ldrec->magIndex)
        ldrec->axisPosition = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(ldrec->magIndex,
          ldrec->ISX, ldrec->ISY);
    }

    // Return the full offset from R to F if R not at 0
    xOffset = (conSet->right + conSet->left) / 2 - camParam->sizeX / 2;
    yOffset = (conSet->bottom + conSet->top) / 2 - camParam->sizeY / 2;
    axisPos = (float)(ldp->axisPosition - B3DCHOICE(fabs(ldrec->axisPosition) > 1.e-5,
      ldrec->axisPosition, 0.));
    rotateAxis = mWinApp->mLowDoseDlg.m_bRotateAxis;
    axisRotation = rotateAxis ? mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
    if (traceIt)
      SEMTrace('1', "Saved focus in state: axis pos %.2f rot %d offsets %d %d",
        axisPos, axisRotation, xOffset, yOffset);
  }

}

// Store the acquire state defined in a map item into a state param, accessing the 
// montage param if non-NULL, or the camera conset specified by baseNum for some values
void CNavHelper::StoreMapStateInParam(CMapDrawItem *item, MontParam *montP, int baseNum,
                                      StateParams *param)
{
  ControlSet *camSets = mWinApp->GetCamConSets();
  ControlSet *conSet;

  // Start by filling with the current state in case some map items are not there
  StoreCurrentStateInParam(param, 0, 0, mWinApp->GetCurrentCamera(), 0);
  param->magIndex = item->mMapMagInd;
  if (item->mMapIntensity)
    param->intensity = item->mMapIntensity;
  if (item->mMapSpotSize)
    param->spotSize = item->mMapSpotSize;
  param->probeMode = item->mMapProbeMode;
  param->beamAlpha = item->mMapAlpha;
  if (item->mMapCamera >= 0) {
    param->camIndex = item->mMapCamera;
    param->camForParams = item->mMapCamera;
  }
  param->slitWidth = 0.;
  if ((mCamParams[param->camIndex].GIF || mWinApp->ScopeHasFilter()) && 
    item->mMapSlitWidth) {
    param->slitIn = item->mMapSlitIn;
    param->slitWidth = item->mMapSlitWidth;
    param->energyLoss = 0.;
    param->zeroLoss = true;
  }
  if (item->mMapBinning > 0)
    param->binning = item->mMapBinning;
  if (item->mShutterMode >= 0) {
    param->exposure = item->mMapExposure;
    param->drift = item->mMapSettling;
    param->shuttering = item->mShutterMode;
    param->K2ReadMode = item->mK2ReadMode;
  }
  param->singleContMode = SINGLE_FRAME;
  param->saveFrames = 0;
  param->doseFrac = 0;
  param->processing = GAIN_NORMALIZED;

  if (item->mMapMontage) {
    if (montP) {
      param->xFrame = montP->xFrame;
      param->yFrame = montP->yFrame;
      param->binning = item->mMontBinning ? item->mMontBinning : montP->binning;
    } else {
      if (baseNum == SEARCH_CONSET && mWinApp->GetUseViewForSearch())
        baseNum = VIEW_CONSET;

      // If no param is entered, take current frame size, adjust for binning 
      // Use the indicated conset from camera sets as the base for this info
      conSet = &camSets[param->camIndex * MAX_CONSETS + baseNum];
      param->binning = item->mMontBinning ? item->mMontBinning : conSet->binning;
      param->xFrame = (conSet->right - conSet->left) / param->binning;
      param->yFrame = (conSet->bottom - conSet->top) / param->binning;
    }
  } else {
    param->xFrame = item->mMapWidth;
    param->yFrame = item->mMapHeight;
    param->binning = item->mMapBinning;
  }
}

// Set scope state and control set state from the given state param
void CNavHelper::SetStateFromParam(StateParams *param, ControlSet *conSet, int baseNum,
                                   int hideLDoff)
{
  int i, alpha, ldArea;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  CameraParameters *camP = &mCamParams[param->camIndex];
  int *activeList = mWinApp->GetActiveCameraList();
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  LowDoseParams ldsaParams;
  bool gotoArea = false;
  mSettingState = true;

  // Set camera first in case there is an automatic mag change and EFTEM change
  for (i = 0; i < mWinApp->GetNumActiveCameras(); i++) {
    if (activeList[i] == param->camIndex) {
      if (param->camIndex != mWinApp->GetCurrentCamera())
        mWinApp->SetActiveCameraNumber(i);
      break;
    }
  }

  if (param->lowDose) {
    ldArea = AreaFromStateLowDoseValue(param, NULL);
    ldp = mWinApp->GetLDParamsForCamera(
      param->camForParams < 0 ? param->camIndex : param->camForParams) + ldArea;

    // If it is already in low dose and in this area, need to save the params and tell it
    // to use those as current params; then need to go to area after changing params
    if (mWinApp->LowDoseMode() && mScope->GetLowDoseArea() == ldArea) {
      ldsaParams = *ldp;
      mScope->SetLdsaParams(&ldsaParams);
      gotoArea = true;
    }
    if (mWinApp->LowDoseMode())
      ldp->axisPosition = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(
        ldp->magIndex, ldp->ISX, ldp->ISY);
    param->ldParams.axisPosition = ldp->axisPosition;
    *ldp = param->ldParams;
    if (IS_AREA_VIEW_OR_SEARCH(ldArea)) {
      if (param->ldShiftOffsetX > -9990.) {
        i = ldArea == SEARCH_AREA ? 1 : 0;
        mWinApp->mLowDoseDlg.mViewShiftX[i] = param->ldShiftOffsetX;
        mWinApp->mLowDoseDlg.mViewShiftY[i] = param->ldShiftOffsetY;
      }
      mWinApp->mLowDoseDlg.ConvertAxisPosition(false);
      mWinApp->mLowDoseDlg.ConvertAxisPosition(true);
    } else
      mWinApp->mLowDoseDlg.ConvertOneAxisToIS(ldp->magIndex, ldp->axisPosition, ldp->ISX,
      ldp->ISY);
    if (gotoArea)
      mScope->GotoLowDoseArea(ldArea);
    else
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);

  } else {
    mWinApp->mLowDoseDlg.SetLowDoseMode(false, hideLDoff > 0);
    mScope->SetMagIndex(param->magIndex);
    if (param->probeMode >= 0)
      mScope->SetProbeMode(param->probeMode, true);

    if (param->beamAlpha >= 0) {
      alpha = mScope->GetAlpha();
      if (alpha >= 0)
        mScope->ChangeAlphaAndBeam(alpha, param->beamAlpha);
    }

    // Set the spot size before intensity to make sure the intensity is legal for this
    // spot size on a FEI, and maybe to handle spot-size dependent changes elsewhere
    mScope->SetSpotSize(param->spotSize);
    if (!camP->STEMcamera)
      mScope->DelayedSetIntensity(param->intensity, GetTickCount(), param->spotSize, 
        param->probeMode);

    // Modify filter parameters if they are present: but don't update unless in filter mode
    if (param->slitWidth > 0.) {
      filtParam->slitIn = param->slitIn;
      filtParam->slitWidth = param->slitWidth;
      filtParam->zeroLoss = param->zeroLoss;
      filtParam->energyLoss = param->energyLoss;
      if (mWinApp->GetFilterMode()) {
        mCamera->SetIgnoreFilterDiffs(true);        
        mWinApp->mFilterControl.UpdateSettings();
      }
    }
  }
  if (param->targetDefocus > -9990.) {
    mScope->IncDefocus(param->targetDefocus -
      mWinApp->mFocusManager->GetTargetDefocus());
    mWinApp->mFocusManager->SetTargetDefocus(param->targetDefocus);
    mWinApp->mAlignFocusWindow.UpdateSettings();
  }
  if (param->ldDefocusOffset > -9990.) {
    mScope->SetLDViewDefocus(param->ldDefocusOffset, ldArea);
    mWinApp->mLowDoseDlg.UpdateSettings();
  }
  SetConsetsFromParam(param, conSet, baseNum);
  mSettingState = false;
}

// Set control sets as appropriate for the given param, modifying the passed one
void CNavHelper::SetConsetsFromParam(StateParams *param, ControlSet *conSet, int baseNum)
{
  int i, curCam = mWinApp->GetCurrentCamera();
  CameraParameters *camP = &mCamParams[param->camIndex];
  ControlSet *workSets;
  ControlSet *camSets = mWinApp->GetCamConSets();
  int destCam = param->camForParams < 0 ? param->camIndex : param->camForParams;
  int montInd = baseNum == MONT_USER_CONSET ? RECORD_CONSET : MONT_USER_CONSET;

  // Set the working sets to either the current ones or ones in the master.  The caller
  // was responsible for passing in conSet as current or master depending on whether 
  // the camera was the current one
  if (destCam == curCam)
    workSets = mWinApp->GetConSets();
  else
    workSets = camSets + destCam * MAX_CONSETS;

  // Copy control set in case need to fill in missing stuff (camera switch
  // copied the camera conset in)
  if (baseNum == SEARCH_CONSET && mWinApp->GetUseViewForSearch())
    baseNum = VIEW_CONSET;
  *conSet = *(workSets + baseNum);

  StateCameraCoords(destCam, param->xFrame, param->yFrame, param->binning,
    conSet->left, conSet->right, conSet->top, conSet->bottom);
  conSet->binning = param->binning;
  conSet->mode = param->singleContMode;
  conSet->exposure = param->exposure;
  if (!camP->STEMcamera)
    conSet->drift = param->drift;
  conSet->shuttering = param->shuttering;
  conSet->K2ReadMode = param->K2ReadMode;
  conSet->doseFrac = param->doseFrac;
  conSet->saveFrames = param->saveFrames;
  if (param->frameTime > 0)
    conSet->frameTime = param->frameTime;
  if (param->processing >= 0)
    conSet->processing = param->processing;
  if (param->alignFrames >= 0)
    conSet->alignFrames = param->alignFrames;
  if (param->useFrameAlign >= 0)
    conSet->useFrameAlign = param->useFrameAlign;
  if (param->faParamSetInd >= 0) {
    conSet->faParamSetInd = param->faParamSetInd;
    B3DCLAMP(conSet->faParamSetInd, 0, mCamera->GetNumFrameAliParams());
  }

  // Set axis position and subarea of focus area if it was stored in param
  if (param->lowDose && param->focusAxisPos > EXTRA_VALUE_TEST) {
    SetLDFocusPosition(param->camIndex, param->focusAxisPos, param->rotateAxis,
      param->axisRotation, param->focusXoffset, param->focusYoffset, "state", 
      param->lowDose == -1 - TRIAL_CONSET);
  }

  if (param->readModeView >= 0)
    workSets[VIEW_CONSET].K2ReadMode = param->readModeView;
  if (param->readModeFocus >= 0)
    workSets[FOCUS_CONSET].K2ReadMode = param->readModeFocus;
  if (param->readModeTrial >= 0)
    workSets[TRIAL_CONSET].K2ReadMode = param->readModeTrial;
  if (param->readModePrev >= 0)
    workSets[PREVIEW_CONSET].K2ReadMode = param->readModePrev;
  if (param->readModeSrch >= 0 && !mWinApp->GetUseViewForSearch())
    workSets[SEARCH_CONSET].K2ReadMode = param->readModeSrch;
  if (param->readModeMont >= 0 && !mWinApp->GetUseRecordForMontage())
    workSets[montInd].K2ReadMode = param->readModeMont;

  // Copy back to the camera sets IF this is the current camera
  if (destCam == curCam)
    for (i = 0; i < MAX_CONSETS; i++)
      camSets[destCam * MAX_CONSETS + i] = workSets[i];
}

// Set the low dose axis position and Focus area offset from given parameters
void CNavHelper::SetLDFocusPosition(int camIndex, float axisPos, BOOL rotateAxis, 
  int axisRotation, int xOffset, int yOffset, const char *descrip, bool forTrial)
{
  int area = forTrial ? TRIAL_CONSET : FOCUS_CONSET;
  int otherArea = forTrial ? FOCUS_CONSET : TRIAL_CONSET;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + area;
  LowDoseParams *ldRec = mWinApp->GetLowDoseParams() + RECORD_CONSET;
  ControlSet *camSets = mWinApp->GetCamConSets();
  ControlSet *focusSet = mWinApp->GetConSets() + area;
  CameraParameters *camP = &mCamParams[camIndex];
  int oldRotation = mWinApp->mLowDoseDlg.m_bRotateAxis ?
    mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
  bool changed = fabs(ldp->axisPosition - axisPos) > 1.e-3 || oldRotation != axisRotation;
  bool needConversions = mWinApp->LowDoseMode() && ldp->magIndex;
  bool subChanged;
  int curArea = mScope->GetLowDoseArea();
  bool needGotoRec = mWinApp->LowDoseMode() && (curArea == area ||
    (curArea == otherArea && (mWinApp->mLowDoseDlg.m_bTieFocusTrial ||
      mWinApp->mLowDoseDlg.GetTieFocusTrialPos())));
  if (needGotoRec)
    mScope->GotoLowDoseArea(RECORD_CONSET);
  if (oldRotation != axisRotation && needConversions)
    mWinApp->mLowDoseDlg.ConvertAxisPosition(false);
  mWinApp->mLowDoseDlg.m_bRotateAxis = rotateAxis;
  if (rotateAxis)
    mWinApp->mLowDoseDlg.m_iAxisAngle = axisRotation;
  if (oldRotation != axisRotation && needConversions)
    mWinApp->mLowDoseDlg.ConvertAxisPosition(true);

  // The passed-in position is the full offset from R to F, but the axis position stored
  // in low-dose params is the net offset from the center
  ldp->axisPosition = axisPos + B3DCHOICE(fabs(ldRec->axisPosition) > 1.e-5, 
    ldRec->axisPosition, 0.);
  if (needConversions)
    mWinApp->mLowDoseDlg.ConvertOneAxisToIS(ldp->magIndex, ldp->axisPosition, ldp->ISX,
      ldp->ISY);
  if (mWinApp->mLowDoseDlg.m_bTieFocusTrial) {
    ldp = mWinApp->GetLowDoseParams();
    ldp[otherArea] = ldp[area];
  } else if (mWinApp->mLowDoseDlg.GetTieFocusTrialPos()) {
    mWinApp->mLowDoseDlg.SyncPosOfFocusAndTrial(area);
  }
  mWinApp->mLowDoseDlg.ManageAxisPosition();
  subChanged = ModifySubareaForOffset(camIndex, xOffset, yOffset, focusSet->left,
    focusSet->top, focusSet->right, focusSet->bottom);
  if (subChanged || changed)
    PrintfToLog("%s area changed from stored %s, axis position %.2f angle %d, subarea"
      " offset %d %d", forTrial ? "Trial" : "Focus", descrip, axisPos, rotateAxis ? axisRotation : 0, 
      xOffset / focusSet->binning, yOffset / focusSet->binning);
  camSets[camIndex * MAX_CONSETS + area] = *focusSet;

}

// Save the current state if it is not already saved
void CNavHelper::SaveCurrentState(int type, int saveLDfocusPos, int camNum,
  int saveTargOffs, BOOL montMap)
{
  StateParams state;
  if (mTypeOfSavedState != STATE_NONE)
    return;
  state.montMapConSet = montMap;
  mTypeOfSavedState = type;
  mPriorState.montMapConSet = montMap;
  StoreCurrentStateInParam(&mPriorState, mWinApp->LowDoseMode() ? 1 : 0, 0, 
    mWinApp->GetCurrentCamera(), 0);
  StoreCurrentStateInParam(&state, mWinApp->LowDoseMode() ? 1 : 0, saveLDfocusPos, 
    camNum, saveTargOffs);
  SEMTrace('I', "SaveCurrentState saved intensity %.5f", mWinApp->LowDoseMode() ? 
    state.ldParams.intensity : state.intensity);
  mSavedStates.Add(state);
  mSavedLowDoseArea = mScope->GetLowDoseArea();
}

// Adds additional state to the array of saved states for other area - the point is
// to save the state that can be modified
void CNavHelper::SaveLowDoseAreaForState(int area, int camNum, bool saveTargOffs,
  BOOL montMap)
{
  int savedArea, ind;
  StateParams state;
  LowDoseParams *param = mWinApp->GetLowDoseParams() + area;
  if (mTypeOfSavedState == STATE_NONE || area < 0)
    return;
  for (ind = 0; ind < mSavedStates.GetSize(); ind++) {
    state = mSavedStates.GetAt(ind);
    savedArea = AreaFromStateLowDoseValue(&state, NULL);
    if (savedArea == area && BOOL_EQUIV(montMap, state.montMapConSet))
      return;
  }

  state.montMapConSet = montMap;
  StoreCurrentStateInParam(&state, -1 - area, 0, camNum, saveTargOffs ? area + 1 : 0);
  mSavedStates.Add(state);
}

// Return the low dose area (-1 for none) and optionally the control set number from the 
// lowDose value in a state
int CNavHelper::AreaFromStateLowDoseValue(StateParams *param, int *setNum)
{
  int area = -1;
  if (param->lowDose) {
    area = RECORD_CONSET;
    if (param->lowDose < 0)
      area = -1 - param->lowDose;
  }
  if (setNum) {
    if (area >= 0)
      *setNum = area;
    else
      *setNum = RECORD_CONSET;
    if (area == SEARCH_AREA)
      *setNum = SEARCH_CONSET;
    if (*setNum == RECORD_CONSET && param->montMapConSet)
      *setNum = MONT_USER_CONSET;
  }
  return area;
}

// Set type of state saved to None and clear out the array of saved states
void CNavHelper::ForgetSavedState(void)
{
  mTypeOfSavedState = STATE_NONE;
  mSavedStates.RemoveAll();
}

// Returns a copy of the first saved state if any and true, or returns false if none
bool CNavHelper::GetSavedPriorState(StateParams &state)
{
  if (!mSavedStates.GetSize())
    return false;
  state = mPriorState;
  return true;
}

// If stayed in Low Dose for a map imaging instead of switching to a map state, restore
// the saved conSet
void CNavHelper::RestoreLowDoseConset(void)
{
  int area;
  float defocus;
  LowDoseParams *ldp;
  ControlSet *workSets = mWinApp->GetConSets();
  ControlSet *camSets = mWinApp->GetCamConSets();
  if (!mRIstayingInLD)
    return;
  workSets[mRIconSetNum] = mSavedConset;
  camSets[mWinApp->GetCurrentCamera() * MAX_CONSETS + mRIconSetNum] = mSavedConset;
  if (!mRIuseCurrentLDparams) {
    area = mCamera->ConSetToLDArea(mRIconSetNum);
    ldp = mWinApp->GetLowDoseParams() + area;
    *ldp = mRIsavedLDparam;
    if (mRIareaDefocusChange) {
      defocus = mScope->GetLDViewDefocus(area);
      mScope->SetLDViewDefocus(defocus - mRIareaDefocusChange, area);
      mScope->IncDefocus(-mRIareaDefocusChange);
    }
  }

  // And be sure to turn flag off in case this is called from other situations
  mRIstayingInLD = false;
}

/////////////////////////////////////////////////
/////  VARIOUS UTILITIES
/////////////////////////////////////////////////

// Get the stage position of the center of a montage piece, adding previously found error
void CNavHelper::StagePositionOfPiece(MontParam * param, ScaleMat aMat, float delX,
                                      float delY, int ix, int iy, float &stageX, 
                                      float & stageY, float &montErrX, float &montErrY)
{
  int retval;
  int imX = ix * (param->xFrame - param->xOverlap) + param->xFrame / 2;
  int imY = (param->yNframes - 1 - iy) * (param->yFrame - param->yOverlap) + 
    param->yFrame / 2;
  ScaleMat aInv = MatInv(aMat);
  stageX = aInv.xpx * (imX - delX) + aInv.xpy * (imY - delY) + mPreviousErrX;
  stageY = aInv.ypx * (imX - delX) + aInv.ypy * (imY - delY) + mPreviousErrY;
  montErrX = montErrY = 0.;

  // Return stage error when acquiring montage if one was obtained and flag is set
  if (mUseMontStageError) {
    int adocInd = mMapStore->GetAdocIndex();
    if (adocInd < 0 || AdocGetMutexSetCurrent(adocInd) < 0)
      return;
    retval = LookupMontStageOffset(mMapStore, mMapMontP, ix, iy, mPieceSavedAt, montErrX,
      montErrY);
    AdocReleaseMutex();
    if (retval)
      return;
  }
}

// Get the montage stage offset for the given piece; assumes the autodoc index has been
// set successfully and mutex is held
int CNavHelper::LookupMontStageOffset(KImageStore *storeMRC, MontParam *param, int ix, 
                                      int iy, std::vector<int> &pieceSavedAt, 
                                      float &montErrX, float &montErrY)
{
  int iz = pieceSavedAt[iy + ix * param->yNframes];
  if (iz < 0)
    return 1;
  if (AdocGetTwoFloats(storeMRC->getStoreType() == STORE_TYPE_ADOC ? ADOC_IMAGE : 
    ADOC_ZVALUE, iz, ADOC_STAGEOFF, &montErrX, &montErrY))
    return 1;
  return 0;
}

// Finds a value for stage offset for the given position delX, delY relative to the
// center of the montage by interpolating from 4 surrounding piece centers
void CNavHelper::InterpMontStageOffset(KImageStore *imageStore, MontParam *montP,
                                       ScaleMat aMat,std::vector<int> &pieceSavedAt, 
                                       float delX, float delY, float &montErrX, 
                                       float &montErrY)
{
  int adocInd, pcxlo, pcxhi, pcylo, pcyhi;
  float pcX, pcY, cenX, cenY, fx, fy;
  float errX00, errX01, errX10, errX11, errY00, errY01, errY10, errY11;
  montErrX = montErrY = 0.;
  cenX = (aMat.xpx * delX + aMat.xpy * delY) / montP->binning;
  cenY = (aMat.ypx * delX + aMat.ypy * delY) / montP->binning;

  // Get fractional piece position, where 0,0 is middle of lower left piece
  pcX = (float)(cenX / (montP->xFrame - montP->xOverlap) + (montP->xNframes - 1.) / 2.);
  pcY = (float)(cenY / (montP->yFrame - montP->yOverlap) + (montP->yNframes - 1.) / 2.);

  // Determine lower and upper piece index and interpolation fraction
  fx = 0.;
  if (pcX < 0.) {
    pcxlo = pcxhi = 0;
  } else if (pcX >= montP->xNframes - 1.) {
    pcxlo = pcxhi = montP->xNframes - 1;
  } else {
    pcxlo = (int)pcX;
    pcxhi = pcxlo + 1;
    fx = pcX - pcxlo;
  }
  fy = 0.;
  if (pcY < 0.) {
    pcylo = pcyhi = 0;
  } else if (pcY >= montP->yNframes - 1.) {
    pcylo = pcyhi = montP->yNframes - 1;
  } else {
    pcylo = (int)pcY;
    pcyhi = pcylo + 1;
    fy = pcY - pcylo;
  }

  // Try to get the 4 mont errors and interpolate
  adocInd = imageStore->GetAdocIndex();
  if (adocInd >= 0 && AdocGetMutexSetCurrent(adocInd) >= 0) {
    if (!LookupMontStageOffset(imageStore, montP, pcxlo, pcylo, pieceSavedAt, errX00, 
      errY00) && !LookupMontStageOffset(imageStore, montP, pcxhi, pcylo, pieceSavedAt, 
      errX10, errY10) &&
      !LookupMontStageOffset(imageStore, montP, pcxlo, pcyhi, pieceSavedAt, errX01, errY01)
      && !LookupMontStageOffset(imageStore, montP, pcxhi, pcyhi, pieceSavedAt, errX11,
      errY11)) {
        montErrX = (1.f - fy) * ((1.f - fx) * errX00 + fx * errX10) + 
          fy * ((1.f - fx) * errX01 + fx * errX11);
        montErrY = (1.f - fy) * ((1.f - fx) * errY00 + fx * errY10) + 
          fy * ((1.f - fx) * errY01 + fx * errY11);
    }
    AdocReleaseMutex();
  }
}

// Rotating the image in the given buffer by current rotation matrix, or skip if identity 
int CNavHelper::RotateForAligning(int bufNum)
{
  int width, height, err;
  float sizingFrac = 0.5f;
  EMimageBuffer *imBuf = &mImBufs[bufNum];
  if (fabs(mRIrMat.xpx - 1.) < 1.e-6 && fabs(mRIrMat.xpy) < 1.e-6 && 
    fabs(mRIrMat.ypx) < 1.e-6 && fabs(mRIrMat.ypy - 1.) < 1.e-6)
    return 0;

  width = imBuf->mImage->getWidth();
  height = imBuf->mImage->getHeight();
  err = TransformBuffer(imBuf, mRIrMat, width, height, sizingFrac, mRIrMat);
  if (err)
    AfxMessageBox("Insufficient memory to rotate the image for aligning", MB_EXCLAME);
  return err;
}

// Transforms the image in imBuf by the matrix in rMat.  Determines the size of the new
// image by applying the matrix in sizingMat to the sizes in sizingWidth and sizingHeight
// and then allowing the fraction sizingFrac of the implied increase in size
int CNavHelper::TransformBuffer(EMimageBuffer * imBuf, ScaleMat sizingMat, 
                                int sizingWidth, int sizingHeight, float sizingFrac,
                                ScaleMat rMat)
{
  int width, height, sizeX, sizeY, tmpS, type;
  EMimageExtra *extra;
  void *array;

  sizeX = (int)fabs(sizingMat.xpx * sizingWidth + sizingMat.xpy * sizingHeight);
  tmpS = (int)fabs(sizingMat.xpx * sizingWidth - sizingMat.xpy * sizingHeight);
  sizeX = (int)(sizingFrac * (B3DMAX(sizeX, tmpS) - sizingWidth) + sizingWidth);
  sizeY = (int)fabs(sizingMat.ypx * sizingWidth + sizingMat.ypy * sizingHeight);
  tmpS = (int)fabs(sizingMat.ypx * sizingWidth - sizingMat.ypy * sizingHeight);
  sizeY = (int)(sizingFrac * (B3DMAX(sizeY, tmpS) - sizingHeight) + sizingHeight);
  sizeX = 4 * (sizeX / 4);
  sizeY = 2 * (sizeY / 2);

  // Get the array for the rotation
  type = imBuf->mImage->getType();
  if (type == kUBYTE) {
    NewArray2(array, unsigned char, sizeX, sizeY);
  } else if (type == kRGB) {
    NewArray2(array, unsigned char, 3 * sizeX, sizeY);
  } else if (type == kFLOAT) {
    NewArray2(array, float, sizeX, sizeY);
  } else {
    NewArray2(array, short int, sizeX, sizeY);
  }
  if (!array)
    return 2;

  // Get the real rotation matrix and do rotation
  imBuf->mImage->Lock();
  width = imBuf->mImage->getWidth();
  height = imBuf->mImage->getHeight();
  XCorrFastInterp(imBuf->mImage->getData(), type, array, width, height, sizeX, sizeY,
    rMat.xpx, rMat.xpy, rMat.ypx, rMat.ypy, width / 2.f, height / 2.f, 0., 0.);
  imBuf->mImage->UnLock();

  // This buffer could be anywhere, so let's do all the replacement steps here
  extra = imBuf->mImage->GetUserData();
  imBuf->mImage->SetUserData(NULL);
  imBuf->DeleteImage();
  if (type == kUBYTE)
    imBuf->mImage = new KImage;
  else if (type == kRGB)
    imBuf->mImage = new KImageRGB;
  else if (type == kFLOAT)
    imBuf->mImage = new KImageFloat;
  else {
    imBuf->mImage = new KImageShort;
    imBuf->mImage->setType(type);
  }
  imBuf->mImage->useData((char *)array, sizeX, sizeY);
  if (extra)
    imBuf->mImage->SetUserData(extra);
  imBuf->SetImageChanged(1);
  return 0;
}

// Returns distance from ptX,ptY to line segment between x1,y1 and x2,y2
float CNavHelper::PointSegmentDistance(float ptX, float ptY, float x1, float y1, 
                                          float x2, float y2)
{
  float tmin, distsq, dx, dy;
  tmin=((ptX-x1)*(x2-x1)+(ptY-y1)*(y2-y1))/((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
  tmin=B3DMAX(0.f,B3DMIN(1.f,tmin));
  dx = (x1+tmin*(x2-x1)-ptX);
  dy = (y1+tmin*(y2-y1)-ptY);
  distsq = dx * dx + dy * dy;
  if (distsq < 0)
    return 0.;
  return (float)sqrt(distsq);
}

// Compute the minimum distance to any edge of a montage from the center of the given
// piece.  Requires that mPieceSavedAt already be filled in for the section
float CNavHelper::PieceToEdgeDistance(MontParam * montP, int ixPiece, int iyPiece)
{
  int ind, j, xFrameDel, yFrameDel, ix, iy; 
  float xpcen, ypcen, xCen, yCen, x1, x2, y1, halfX, halfY, dist, distMin, y2;
  int ixDir[] = {-1, 0, 1, 0};
  int iyDir[] = {0, -1, 0, 1};

  xFrameDel = (montP->xFrame - montP->xOverlap);
  yFrameDel =  (montP->yFrame - montP->yOverlap);
  halfX = montP->xFrame / 2.f;
  halfY = montP->yFrame / 2.f;
  xCen = ixPiece * xFrameDel + halfX;
  yCen = iyPiece * yFrameDel + halfY;
  distMin = 100000000.;
  for (ix = 0; ix < montP->xNframes; ix++) {
    for (iy = 0; iy < montP->yNframes; iy++) {
      ind = iy + ix * montP->yNframes;
      if (mPieceSavedAt[ind] < 0)
        continue;
      xpcen = ix * xFrameDel + halfX;
      ypcen = iy * yFrameDel + halfY;
      for (j = 0; j < 4; j++) {

        // Loop over the 4 directions; if there is no piece in a direction,
        // compose the coordinates of the edge and get distance to edge segment
        ind = iy + iyDir[j] + (ix + ixDir[j]) * montP->yNframes;
        if (ix + ixDir[j] < 0 || ix + ixDir[j] >= montP->xNframes || 
          iy + iyDir[j] < 0 || iy + iyDir[j] >= montP->yNframes ||
          mPieceSavedAt[ind] < 0) {
            if (ixDir[j]) {
              x1 = x2 = xpcen + ixDir[j] * halfX;
            } else {
              x1 = xpcen - halfX;
              x2 = xpcen + halfX;
            }
            if (iyDir[j]) {
              y1 = y2 = ypcen + iyDir[j] * halfY;
            } else {
              y1 = ypcen - halfY;
              y2 = ypcen + halfY;
            }
            dist = PointSegmentDistance(xCen, yCen, x1, y1, x2, y2);
            distMin = B3DMIN(dist, distMin);
          }
      }
    }
  }
  return distMin;
}

// Return the last stage error and the registration it was taken at
int CNavHelper::GetLastStageError(float &stageErrX, float &stageErrY, float &localErrX,
    float &localErrY)
{
  stageErrX = mStageErrX;
  stageErrY = mStageErrY;
  localErrX = mLocalErrX;
  localErrY = mLocalErrY;
  return mRegistrationAtErr;
}

// Return the target position where last stage error was found, and ID of map used
int CNavHelper::GetLastErrorTarget(float & targetX, float & targetY)
{
  targetX = mRIabsTargetX;
  targetY = mRIabsTargetY;
  return mRImapID;
}

// Function to change registrations of all buffers matching the mapID
void CNavHelper::ChangeAllBufferRegistrations(int mapID, int fromReg, int toReg)
{
  POSITION pos = mWinApp->mDocWnd->GetFirstViewPosition();
  while (pos != NULL) {
    CSerialEMView *pView = (CSerialEMView *)mWinApp->mDocWnd->GetNextView(pos);
    pView->ChangeAllRegistrations(mapID, fromReg, toReg);
  }
}

// Get the image and beam shift and beam tilt for View or search from current parameters
void CNavHelper::GetViewOffsets(CMapDrawItem * item, float &netShiftX, 
  float &netShiftY, float &beamShiftX, float & beamShiftY, float &beamTiltX, 
  float &beamTiltY, int area)
{
  double shiftX, shiftY;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  beamTiltX = beamTiltY = 0.;
  mWinApp->mLowDoseDlg.GetNetViewShift(shiftX, shiftY, area);
  SimpleIStoStage(item, shiftX, shiftY, netShiftX, netShiftY);

  // Store incremental beam shift and tilt if any
  beamShiftX = (float)(ldParm[area].beamDelX - ldParm[RECORD_CONSET].beamDelX);
  beamShiftY = (float)(ldParm[area].beamDelY - ldParm[RECORD_CONSET].beamDelY);
  if (mScope->GetLDBeamTiltShifts()) {
    beamTiltX = (float)(ldParm[area].beamTiltDX - 
      ldParm[RECORD_CONSET].beamTiltDX);
    beamTiltY = (float)(ldParm[area].beamTiltDY - 
      ldParm[RECORD_CONSET].beamTiltDY);
  }
}

// Convert IS to a stage position: for the callers to this, it works better to use 0-focus
// stage calibration instead of defocused one
void CNavHelper::SimpleIStoStage(CMapDrawItem * item, double ISX, double ISY, 
  float &stageX, float &stageY)
{
  ScaleMat aMat, bMat;
  stageX = stageY = 0.;
  aMat = mShiftManager->IStoGivenCamera(item->mMapMagInd, item->mMapCamera);
  if (aMat.xpx && (ISX || ISY)) {
    //aMat = MatMul(aMat, MatInv(ItemStageToCamera(item)));
    bMat = MatInv(mShiftManager->StageToCamera(item->mMapCamera, item->mMapMagInd));
    if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
      mShiftManager->AdjustCameraToStageForTilt(bMat, item->mMapTiltAngle);
    aMat = MatMul(aMat, bMat);
    stageX = (float)(aMat.xpx * ISX + aMat.xpy * ISY);
    stageY = (float)(aMat.ypx * ISX + aMat.ypy * ISY);
  }
}

// Get the stage to camera matrix for an item, adjusting for defocus
ScaleMat CNavHelper::ItemStageToCamera(CMapDrawItem * item)
{
  ScaleMat aMat;
  if (IS_SET_VIEW_OR_SEARCH(item->mMapLowDoseConSet))
    aMat = mShiftManager->FocusAdjustedStageToCamera(item->mMapCamera, item->mMapMagInd,
      item->mMapSpotSize, item->mMapProbeMode, item->mMapIntensity, item->mDefocusOffset);
  else
    aMat = mShiftManager->StageToCamera(item->mMapCamera, item->mMapMagInd);
  if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
    mShiftManager->AdjustStageToCameraForTilt(aMat, item->mMapTiltAngle);
  return aMat;
}

// Get the coordinates for a camera set from the state parameters
void CNavHelper::StateCameraCoords(int camIndex, int xFrame, int yFrame, int binning,
  int &left, int &right, int &top, int &bottom)
{
  CameraParameters *camP = mWinApp->GetCamParams() + camIndex;
  mCamera->CenteredSizes(xFrame, camP->sizeX, camP->moduloX, left, right, 
    yFrame, camP->sizeY, camP->moduloY, top, bottom, binning, camIndex);
  left *= binning;
  right *= binning;
  top *= binning;
  bottom *= binning;
}

// Get number of holes defined in multi-shot params, or 0,0 for a custom pattern
// Return false if custom or not defined (0, 0)
bool CNavHelper::GetNumHolesFromParam(int &xnum, int &ynum, int &numTotal)
{
  xnum = 0;
  ynum = 0;
  if (mMultiShotParams.useCustomHoles && mMultiShotParams.customHoleX.size() > 0) {
    numTotal = (int)mMultiShotParams.customHoleX.size();
  } else {
    xnum = mMultiShotParams.numHoles[0];
    ynum = mMultiShotParams.numHoles[1];
    numTotal = xnum * ynum;
    if (mMultiShotParams.skipCornersOf3x3 && xnum == 3 && ynum == 3) {
      xnum = ynum = -3;
      numTotal = 5;
    }
  }
  if (!MultipleHolesAreSelected() || !xnum || !ynum)
    numTotal = 1;
  return xnum != 0 && ynum != 0;
}

// Return the number of holes that would be acquired for the current item, or the given
// default if no items 
int CNavHelper::GetNumHolesForItem(CMapDrawItem *item, int numDefault)
{
  int numForItem = numDefault;
  if (item->mNumXholes && item->mNumYholes) {
    if (item->mNumXholes == -3 && item->mNumYholes == -3)
      numForItem = 5;
    else
      numForItem = item->mNumXholes * item->mNumYholes;
    numForItem -= item->mNumSkipHoles;
  }
  return numForItem;
}

/////////////////////////////////////////////////////
////    MARKER SHIFT FUNCTIONS
/////////////////////////////////////////////////////

// Clear out the saved shifts before saving any new ones for a new operation
void CNavHelper::ClearSavedMapMarkerShifts()
{
  mSavedMaShMapIDs.clear();
  mSavedMaShCohortIDs.clear();
  mSavedMaShXshift.clear();
  mSavedMaShXshift.clear();
}

// Save existing cohort ID and marker shifts for a map and apply new ones
void CNavHelper::SaveMapMarkerShiftToLists(CMapDrawItem * item, int cohortID, 
  float newXshift, float newYshift)
{
  if (item->IsNotMap())
    return;
  mSavedMaShMapIDs.push_back(item->mMapID);
  mSavedMaShCohortIDs.push_back(item->mShiftCohortID);
  mSavedMaShXshift.push_back(item->mMarkerShiftX);
  mSavedMaShYshift.push_back(item->mMarkerShiftY);
  item->mShiftCohortID = cohortID;
  item->mMarkerShiftX = newXshift;
  item->mMarkerShiftY = newYshift;
}

// Find the saved marker shift for an item and restore it
void CNavHelper::RestoreMapMarkerShift(CMapDrawItem * item)
{
  int ind;
  if (item->IsNotMap())
    return;
  for (ind = 0; ind < (int)mSavedMaShMapIDs.size(); ind++) {
    if (mSavedMaShMapIDs[ind] == item->mMapID) {
      item->mShiftCohortID = mSavedMaShCohortIDs[ind];
      item->mMarkerShiftX = mSavedMaShXshift[ind];
      item->mMarkerShiftY = mSavedMaShYshift[ind];
      break;
    }
  }
}

// Find the base marker shift that matches the fromMag and is nearest to toMag
BaseMarkerShift *CNavHelper::FindNearestBaseShift(int fromMag, int toMag)
{
  int ind, magDist, minDist = 1000, minInd = -1;
  if (!fromMag || !toMag)
    return NULL;
  for (ind = 0; ind < (int)mMarkerShiftArray.GetSize(); ind++) {
    if (mMarkerShiftArray[ind].fromMagInd == fromMag) {
      magDist = B3DABS(mMarkerShiftArray[ind].toMagInd - toMag);
      if (magDist < minDist) {
        minInd = ind;
        minDist = magDist;
      }
    }
  }
  if (minInd < 0)
    return NULL;
  return &mMarkerShiftArray[minInd];
}

// Test if it is possible to apply store marker shift: there must be at least on marker 
// shift for which an unshifted map at the fromMag exists
bool CNavHelper::OKtoApplyBaseMarkerShift()
{
  int shInd, itInd, reg;
  CMapDrawItem *item;
  if (!mNav)
    return false;
  reg = mNav->GetCurrentRegistration();
  for (shInd = 0; shInd < (int)mMarkerShiftArray.GetSize(); shInd++) {
    for (itInd = 0; itInd < (int)mItemArray->GetSize(); itInd++) {
      item = mItemArray->GetAt(itInd);
      if (item->IsMap() && item->mRegistration == reg &&
        item->mMapMagInd == mMarkerShiftArray[shInd].fromMagInd && 
        (!item->mShiftCohortID || item->mMarkerShiftX < EXTRA_VALUE_TEST))
        return true;
    }
  }
  return false;
}

// Apply a base marker shift: determine parameters and query user in order to call
// shift routine in Nav
void CNavHelper::ApplyBaseMarkerShift()
{
  int ind, magVal, magInd, nearMag = -20, itemMag = 0, curMag = mScope->GetMagIndex();
  int camera = mWinApp->GetCurrentCamera();
  int useInd, toMag;
  IntVec shiftInds, shiftToMags;
  CString matchText1, matchText, str;
  bool itemMatch = false;
  CMapDrawItem *item = mNav->GetCurrentItem();

  // Set up a default mag based either on the mag of the current item if it is a map,
  // or the mag nearest to the current mag
  if (item && item->IsMap())
    itemMag = item->mMapMagInd;
  for (ind = 0; ind < (int)mMarkerShiftArray.GetSize(); ind++) {
    if (mMarkerShiftArray[ind].fromMagInd == itemMag)
      itemMatch = true;
    if (B3DABS(nearMag - mMarkerShiftArray[ind].fromMagInd) >
      B3DABS(curMag - mMarkerShiftArray[ind].fromMagInd))
      nearMag = mMarkerShiftArray[ind].fromMagInd;
  }

  // Find out a mag from the user and get index for it
  magVal = MagForCamera(camera, itemMatch ? itemMag : nearMag);
  if (!KGetOneInt("Shifts will be applied to unshifted maps and items marked on them.",
    "Magnification of maps to apply stored shift to:", magVal))
    return;
  magInd = FindIndexForMagValue(magVal, camera);
  magVal = MagForCamera(camera, magInd);

  // See how many shifts match that
  for (ind = 0; ind < (int)mMarkerShiftArray.GetSize(); ind++) {
    if (mMarkerShiftArray[ind].fromMagInd == magInd) {
      shiftInds.push_back(ind);
      shiftToMags.push_back(MagForCamera(camera, mMarkerShiftArray[ind].toMagInd));
    }
  }
  
  // If none, bail out
  if (!shiftInds.size()) {
    str.Format("There are no stored shifts at magnification %dx", magVal);
    AfxMessageBox(str, MB_EXCLAME);
    return;
  }

  // If multiple, get the one to use, looping to get a useful answer if needed
  useInd = shiftInds.size() > 1 ? -1 : shiftInds[0];
  while (useInd < 0) {
    matchText = "";
    matchText1.Format("Values have been saved for Shift to Marker from a point "
      "at %dx to images at", magVal);
    for (ind = 0; ind < (int)shiftInds.size(); ind++) {
      if (ind && (int)shiftInds.size() > 2)
        matchText += ", ";
      if (ind == (int)shiftInds.size() - 1)
        matchText += " and";
      str.Format(" %dx", shiftToMags[ind]);
      matchText += str;
    }
    matchText += ".  Enter the image magnification of the one to use:";
    toMag = shiftToMags[0];
    if (!KGetOneInt(matchText1, matchText, toMag))
      return;
    for (ind = 0; ind < (int)shiftInds.size(); ind++)
      if (shiftToMags[ind] == toMag)
        useInd = ind;
  }

  // Final info to user and confirmation
  str.Format("A shift of %.2f, %.2f will be applied to all maps at\n %dx that have not "
    "been shifted before, and items marked on them.\n\nDo you want to proceed?",
    mMarkerShiftArray[useInd].shiftX, mMarkerShiftArray[useInd].shiftY, magVal);
  if (AfxMessageBox(str, MB_QUESTION) == IDNO)
    return;

  // Do the shift; it saves parameters for undo
  mNav->ShiftCohortOfItems(mMarkerShiftArray[useInd].shiftX,
    mMarkerShiftArray[useInd].shiftY, mNav->GetCurrentRegistration(), magInd, 
    mNav->MakeUniqueID(), false, false, 1);
}


/////////////////////////////////////////////////////
/////  SETTING FILES, TS PARAMS, STATES FOR ACQUIRES
///////////////////////////////////////////////////// 

// Generate the next filename in a series given a filename
// The rules are that digits before an extension or at the end of the filename,
// or digits followed by one non-digit before the extension or at the end,
// will be incremented by 1 and output with the same number of digits or more if needed
// If there are no digits, a "2" is added before the extension or at the end of the name
CString CNavHelper::NextAutoFilename(CString inStr, CString oldLabel, CString newLabel)
{
  int curnum, numdig;
  CString str, format, extra, ext = "";
  if (mUseLabelInFilenames && !oldLabel.IsEmpty() && oldLabel != newLabel) {
    UtilSplitPath(inStr, str, format);
    if (format.Find(oldLabel) >= 0) {
      curnum = format.GetLength() + 1 - oldLabel.GetLength();
      while (curnum >= 0 && format.Find(oldLabel, curnum) < 0)
        curnum--;
      if (curnum >= 0) {
        extra = format.Mid(curnum);
        if (curnum)
          format = format.Left(curnum);
        else
          format = "";
        extra.Replace(oldLabel, newLabel);
        inStr = str + "\\" + format + extra;
        return inStr;
      }
    }
  }
  inStr = DecomposeNumberedName(inStr, ext, curnum, numdig, extra);
  if (numdig) {
    format.Format("%%0%dd", numdig);
    str.Format((LPCTSTR)format, curnum + 1);
    str = inStr + str + extra + ext;
  } else {
    str = inStr + "2" + ext;
  }
  return str;
}

// Splits a name into an unnumbered root, a number if any, an optional character between 
// the number and extension, and the extension including its dot, if any.  Also returns
// the number of digits, or 0 if there is no number.
CString CNavHelper::DecomposeNumberedName(CString inStr, CString &ext, int &curnum, 
  int &numdig, CString &extra)
{
  int dirInd, extInd, numroot, digInd;
  CString str;
  ext = "";
  extra = "";
  curnum = 0;
  numdig = 0;
  dirInd = inStr.ReverseFind('\\');
  extInd = inStr.ReverseFind('.');
  if (extInd > dirInd && extInd > 0) {
    ext = inStr.Right(inStr.GetLength() - extInd);
    inStr = inStr.Left(extInd);
  }
  str = inStr;
  str = str.MakeReverse();
  digInd = str.FindOneOf("0123456789");
  if (digInd == 0 || digInd == 1) {
    if (digInd == 1) {
      extra = str.Left(1);
      str = str.Right(str.GetLength() - 1);
    }
    str = str.SpanIncluding("0123456789");
    str = str.MakeReverse();
    curnum = atoi((LPCTSTR)str);
    numdig = str.GetLength();
    numroot = inStr.GetLength() - numdig - digInd;
    if (numroot)
      str = inStr.Left(numroot);
    return str;
  } else {
    return inStr;
  }
}

// Checks a given filename for whether its unnumbered root, extension, and extra character
// match the given ones
void CNavHelper::CheckForSameRootAndNumber(CString &root, CString &ext, 
  CString &extra, CString name, int &maxNum, int &numDig)
{
  CString ext2, extra2;
  int num2, numDig2;
  name.MakeUpper();
  if (!name.IsEmpty() && name.Find(root) == 0) {
    DecomposeNumberedName(name, ext2, num2, numDig2, extra2);
    if (!ext2.CompareNoCase(ext) && !extra2.CompareNoCase(extra) && num2 > maxNum) {
      maxNum = num2;
      ACCUM_MAX(numDig, numDig2);
    }
  }
}

// Check whether the name is already used in the lists of ones to open
bool CNavHelper::NameToOpenUsed(CString name)
{
  CMapDrawItem *item;
  ScheduledFile *sched;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (!item->mFileToOpen.IsEmpty() && item->mFileToOpen == name)
      return true;
  }
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched = mGroupFiles->GetAt(i);
    if (sched->filename == name)
      return true;
  }
  return false;
}

// Set up a new file to open for an item or group.  Returns -1 if user cancels, or >0
// for other errors; cleans up references in either case 
int CNavHelper::NewAcquireFile(int itemNum, int listType, ScheduledFile *sched)
{
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  ScheduledFile *sched2;
  StateParams *state;
  MontParam *montp;
  int i, index;
  bool breakForFit = false;
  int *propIndexp = &item->mFilePropIndex;
  int *montIndexp = &item->mMontParamIndex;
  int *stateIndexp = &item->mStateIndex;
  CString autoName = "";
  CString *namep = &item->mFileToOpen;
  if (listType == NAVFILE_GROUP) {
    propIndexp = &sched->filePropIndex;
    montIndexp = &sched->montParamIndex;
    stateIndexp = &sched->stateIndex;
    namep = &sched->filename;
  }
  *propIndexp = *montIndexp = *stateIndexp = -1;

  // First check for low dose state mismatch for tilt series
  if (listType == NAVFILE_TS) {
    for (i = itemNum - 1; i >= 0; i--) {
      item2 = mItemArray->GetAt(i);
      if (item2->mTSparamIndex >= 0 && item2->mStateIndex >= 0) {
        state = mAcqStateArray->GetAt(item2->mStateIndex);
        if (state->lowDose && !mWinApp->LowDoseMode() || 
          !state->lowDose && mWinApp->LowDoseMode()) {
          autoName.Format("Low dose mode is currently %s but it is %s in the imaging "
            "state\nparameters of a previous item selected for a tilt series.\n\n"
            "If you do want to change modes between series, you should set the imaging\n"
            "state for this series and also set the tilt series parameters.",
            state->lowDose ? "OFF" : "ON", state->lowDose ? "ON" : "OFF");
          AfxMessageBox(autoName, MB_OK | MB_ICONINFORMATION);
        }
        break;
      }
    }
  }

  // First look for a prior item of the same kind to get names/references from
  for (i = itemNum - 1; i >= 0; i--) {
    item2 = mItemArray->GetAt(i);
    if (listType == NAVFILE_TS) {
      if (item2->mTSparamIndex >= 0) {
        item->mTSparamIndex = item2->mTSparamIndex;
        *propIndexp = item2->mFilePropIndex;
        *montIndexp = item2->mMontParamIndex;
        autoName = NextAutoFilename(item2->mFileToOpen, item2->mLabel, item->mLabel);
        ChangeRefCount(NAVARRAY_TSPARAM, item->mTSparamIndex, 1);
      }
    } else if (item2->mTSparamIndex < 0) {
      if (item2->mFilePropIndex >= 0) {
        
        // Force a new parameter set if last one is fitting to polygon and this is a 
        // polygon item - otherwise let it inherit the fit
        autoName = NextAutoFilename(item2->mFileToOpen, item2->mLabel, item->mLabel);
        if (item2->mMontParamIndex >= 0) {
          montp = mMontParArray->GetAt(item2->mMontParamIndex);
          breakForFit = montp->wasFitToPolygon && item->IsPolygon();
        }
        *propIndexp = item2->mFilePropIndex;
        *montIndexp = item2->mMontParamIndex;
      } else {
        index = mNav->GroupScheduledIndex(item2->mGroupID);
        if (index >= 0) {
          sched2 = mGroupFiles->GetAt(index);
          *propIndexp = sched2->filePropIndex;
          *montIndexp = sched2->montParamIndex;
          autoName = NextAutoFilename(sched2->filename, item2->mLabel, item->mLabel);
        }
      }
    }

    // If succeeded, increment the remaining reference counts
    if (*propIndexp >= 0) {
      ChangeRefCount(NAVARRAY_FILEOPT, *propIndexp, 1);
      if (*montIndexp >= 0) 
        ChangeRefCount(NAVARRAY_MONTPARAM, *montIndexp, 1);

      // Check for uniqueness of filenames
      if (breakForFit)
        break;
      while (NameToOpenUsed(autoName) || mDocWnd->StoreIndexFromName(autoName) >= 0)
        autoName = NextAutoFilename(autoName);
      *namep = autoName;
      return 0;
    }
  }

  // If nothing found, need to create file options and filename
  index = SetFileProperties(itemNum, listType, sched, false, 
    breakForFit && mSkipMontFitDlgs);
  if (index) {
    *propIndexp = *montIndexp = -1;
    RecomputeArrayRefs();
    return index;
  }

  // If there is a problem with the name, abort the whole thing
  if (autoName.IsEmpty()) {
    index = SetOrChangeFilename(itemNum, listType, sched);
    if (index) {
      *propIndexp = *montIndexp = -1;
      RecomputeArrayRefs();
      return index;
    }
  } else {

    // But use the name if one was inherited for a new poly fit
    while (NameToOpenUsed(autoName) || mDocWnd->StoreIndexFromName(autoName) >= 0)
      autoName = NextAutoFilename(autoName);
    *namep = autoName;
  }

  // Do TS params last so the montaging is already known
  if (listType == NAVFILE_TS) {
    index = SetTSParams(itemNum);
    if (index) {
      *propIndexp = *montIndexp = -1;
      *namep = "";
      RecomputeArrayRefs();
    }
  }

  return index;
}

// Set or adjust tilt series params
int CNavHelper::SetTSParams(int itemNum)
{
  TiltSeriesParam *tsp, *tspSrc;
  MontParam *montp;
  int err, i;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  bool needNew = false;
  int future = 1, futureLDstate = -1, ldMagInd = 0, overrideFlags = 0;
  int prevIndex = item->mTSparamIndex;
  CString inheritors;

  // If parameters already exist, copy them to the master; otherwise just edit the master
  if (prevIndex >= 0) {
    tspSrc = mTSparamArray->GetAt(prevIndex);
    mWinApp->mTSController->CopyParamToMaster(tspSrc, false);
    future = 2;
  }

  // Set some parameters if montaging: magLocked indicates montaging
  tsp = mWinApp->mTSController->GetTiltSeriesParam();
  tsp->magLocked = item->mMontParamIndex >= 0;
  if (tsp->magLocked) {
    montp = mMontParArray->GetAt(item->mMontParamIndex);
    mWinApp->mTSController->MontageMagIntoTSparam(montp, tsp);
    tsp->binning = montp->binning;
  }

  // Find the last state set for a TS param and set the future LD state from it
  for (i = itemNum; i >= 0; i--) {
    item2 = mItemArray->GetAt(i);
    if (item2->mTSparamIndex >= 0 && item2->mStateIndex >= 0) {
      StateParams *state = mAcqStateArray->GetAt(item2->mStateIndex);
      futureLDstate = state->lowDose ? 1 : 0;
      if (state->lowDose)
        ldMagInd = state->ldParams.magIndex;
      break;
    }
  }

  // Set flags for special parameters set in the Nav item
  if (item->mTSstartAngle > EXTRA_VALUE_TEST) {
    overrideFlags |= NAV_OVERRIDE_TILT;
    if (item->mTSbidirAngle > EXTRA_VALUE_TEST)
      overrideFlags |= NAV_OVERRIDE_BIDIR;
  }
  if (item->mTargetDefocus > EXTRA_VALUE_TEST)
    overrideFlags |= NAV_OVERRIDE_DEF_TARG;

  err = mWinApp->mTSController->SetupTiltSeries(future, futureLDstate, ldMagInd,
    overrideFlags);
  if (err)
    return err;

  // Now that we have success, either copy it back to source or make a new one
  if (item->mTSparamIndex < 0)
    needNew = true;
  else {

    // See if this is a reference shared with an earlier item, if so split it off
    for (i = 0; i < itemNum && !needNew; i++) {
      item2 = mItemArray->GetAt(i);
      needNew = item2->mTSparamIndex == prevIndex;
      if (needNew)
        NoLongerInheritMessage(itemNum, i, "tilt series parameters");
    }
  }

  if (prevIndex >= 0)
    AddInheritingItems(itemNum, prevIndex, NAVARRAY_TSPARAM, inheritors);

  if (needNew) {
    tsp = new TiltSeriesParam;
    mWinApp->mTSController->CopyMasterToParam(tsp, false);
    mTSparamArray->Add(tsp);
    tsp->refCount = 0;
    tsp->navID = mNav->MakeUniqueID();
 
    // For this item and all following ones with the same original index, assign to the
    // new index and adjust the reference counts
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);
      if (item2->mTSparamIndex == prevIndex && (i == itemNum || prevIndex >= 0)) {
        if (prevIndex >= 0)
          ChangeRefCount(NAVARRAY_TSPARAM, prevIndex, -1);
        item2->mTSparamIndex = (int)mTSparamArray->GetSize() - 1;
        tsp->refCount++;
      }
    }
  } else
    mWinApp->mTSController->CopyMasterToParam(tspSrc, false);

  BeInheritedByMessage(itemNum, inheritors, "Changes in tilt series parameters");
  return 0;
}

// Set the file properties for a file to be opened for item or group, either by editing
// existing properties for that item or by cloning shared properties and editing those
int CNavHelper::SetFileProperties(int itemNum, int listType, ScheduledFile *sched,
  bool fromFilePropButton, bool skipFitDlgs)
{
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  ScheduledFile *sched2;
  CNavFileTypeDlg typeDlg;
  MontParam *montpSrc = mWinApp->GetMontParam();
  MontParam *newMontp;
  FileOptions *fileOptSrc = mDocWnd->GetFileOpt();
  FileOptions *newFileOpt;
  CString inheritors;
  bool madeNewOpt = false, gotThisSched = false;
  int movingStage, prevIndex, i, j, sharedInd;
  int *propIndexp = &item->mFilePropIndex;
  int *montIndexp = &item->mMontParamIndex;
  prevIndex = *montIndexp;
  if (listType == NAVFILE_GROUP) {
    propIndexp = &sched->filePropIndex;
    montIndexp = &sched->montParamIndex;
  }
  if (prevIndex >= 0)
    montpSrc = mMontParArray->GetAt(prevIndex);
  if (*propIndexp >= 0)
    fileOptSrc = mFileOptArray->GetAt(*propIndexp);
  typeDlg.m_iSingleMont = mLastTypeWasMont ? 1 : 0;
  if (*propIndexp >= 0)
    typeDlg.m_iSingleMont = prevIndex >= 0 ? 1 : 0;
  typeDlg.mPolyFitOK = item->IsPolygon() && listType == NAVFILE_ITEM;
  typeDlg.m_bFitPoly = mLastMontFitToPoly;
  typeDlg.m_bSkipDlgs = mSkipMontFitDlgs;
  if (prevIndex >= 0 && typeDlg.mPolyFitOK)
    typeDlg.m_bFitPoly = montpSrc->wasFitToPolygon;
  if (!skipFitDlgs && typeDlg.DoModal() != IDOK)
    return -1;
  mLastTypeWasMont = typeDlg.m_iSingleMont != 0;
  mLastMontFitToPoly = typeDlg.m_bFitPoly;
  mSkipMontFitDlgs = typeDlg.m_bSkipDlgs;

  AddInheritingItems(itemNum, prevIndex, NAVARRAY_MONTPARAM, inheritors);

  // If it is now supposed to be a montage, get parameters from existing param or
  // master ones, do dialog, then save params
  if (typeDlg.m_iSingleMont) {
    CMontageSetupDlg montDlg;
    montDlg.mParam = *montpSrc;
    if (prevIndex < 0)
      mDocWnd->InitMontParamsForDialog(&montDlg.mParam, false);

    montDlg.mSizeLocked = false;
    montDlg.mConstrainSize = false;
    if (listType == NAVFILE_TS)
      montDlg.mParam.moveStage = false;
    if (typeDlg.m_bFitPoly && typeDlg.mPolyFitOK) {

      // This assumes that it is the current item
      if (mNav->PolygonMontage(&montDlg, skipFitDlgs))
        return -1;
      montDlg.mParam.wasFitToPolygon = true;
    } else if (montDlg.DoModal() != IDOK)
      return -1;

    // Set some params after the dialog: note makeNewMap is irrelevant in Nav acquire
    // 5/11/18: give up on setting very sloppy for stage
    montDlg.mParam.warnedCalOpen = true;
    montDlg.mParam.warnedCalAcquire = true;
    movingStage = montDlg.mParam.moveStage ? 1 : 0;

    // Need to make a new param if there is not one in array, or if the reference count
    // was greater than 1 and it is shared with earlier ones
    if (prevIndex < 0 || (montDlg.mParam.refCount > 1 && 
      EarlierItemSharesRef(itemNum, prevIndex, NAVARRAY_MONTPARAM, sharedInd))) {
          newMontp = new MontParam;
        *newMontp = montDlg.mParam;
        mMontParArray->Add(newMontp);
        newMontp->refCount = 0;
        newMontp->navID = mNav->MakeUniqueID();
        if (prevIndex >= 0 && sharedInd >= 0)
          NoLongerInheritMessage(itemNum, sharedInd, "montage parameters");
        for (i = itemNum; i < mItemArray->GetSize(); i++) {
          item2 = mItemArray->GetAt(i);
          
          // Adjust indices of inheriting items and item itself if not doing a group file
          if (item2->mMontParamIndex == prevIndex && (prevIndex >= 0 ||
            (i == itemNum && listType != NAVFILE_GROUP))) {
              if (prevIndex >= 0)
                montpSrc->refCount--;
              item2->mMontParamIndex = (int)mMontParArray->GetSize() - 1;
              newMontp->refCount++;
          }

          // Adjust indices of group files
          j = mNav->GroupScheduledIndex(item2->mGroupID);
          if (j >= 0) {
            sched2 = mGroupFiles->GetAt(j);
            if (sched2->montParamIndex == prevIndex && (prevIndex >= 0 ||
              (sched2 == sched && listType == NAVFILE_GROUP))) {
                if (prevIndex >= 0)
                  montpSrc->refCount--;
                sched2->montParamIndex = (int)mMontParArray->GetSize() - 1;
                newMontp->refCount++;
                if (sched2 == sched)
                  gotThisSched = true;
            }
          }
        }
        if (listType == NAVFILE_GROUP && !gotThisSched) {
          sched->montParamIndex = (int)mMontParArray->GetSize() - 1;
          newMontp->refCount++;
        }
    } else
      *montpSrc = montDlg.mParam;
    BeInheritedByMessage(itemNum, inheritors, "Changes in montage parameters");

  } else if (prevIndex >= 0) {

    // If it used to be a montage and is no more, reduce reference count and set index to
    // -1 for each item
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);
      if (item2->mMontParamIndex == prevIndex) {
        ChangeRefCount(NAVARRAY_MONTPARAM, prevIndex, -1);
        item2->mMontParamIndex = -1;
      }
    }
    BeInheritedByMessage(itemNum, inheritors, "Changing from montage to single frame");
    movingStage = -1;
  }
  inheritors = "";
  sharedInd = -1;

  // Now do file options dialog but create a new copy first if needed
  newFileOpt = fileOptSrc;
  prevIndex = *propIndexp;
  if (prevIndex < 0 || (fileOptSrc->refCount > 1 && 
    EarlierItemSharesRef(itemNum, prevIndex, NAVARRAY_FILEOPT, sharedInd))) {
    newFileOpt = new FileOptions;
    mDocWnd->SetFileOptsForSTEM();
    *newFileOpt = *fileOptSrc;
    mDocWnd->RestoreFileOptsFromSTEM();
    newFileOpt->refCount = 0;
    madeNewOpt = true;
  }

  // Prepare the file options as appropriate for single frame or montage
  newFileOpt->TIFFallowed = 0;
  newFileOpt->montageInMdoc = false;
  newFileOpt->typext &= ~MONTAGE_MASK;
  if (*montIndexp < 0) {
    if (movingStage < 0)
      newFileOpt->typext &= ~VOLT_XY_MASK;
  } else {
    newFileOpt->mode = B3DMAX(newFileOpt->mode, 1);
    newMontp = mMontParArray->GetAt(*montIndexp);
    if ((newMontp->xNframes - 1) * (newMontp->xFrame - newMontp->xOverlap) > 65535 ||
      (newMontp->yNframes - 1) * (newMontp->yFrame - newMontp->yOverlap) > 65535)
      newFileOpt->montageInMdoc = true;
    else
      newFileOpt->typext |= MONTAGE_MASK;
    if (movingStage > 0)
      newFileOpt->typext |= VOLT_XY_MASK;
  }

  if (mDocWnd->FilePropForSaveFile(newFileOpt, fromFilePropButton)) {
    if (madeNewOpt)
      delete newFileOpt;
    return -1;
  }

  if (prevIndex >= 0 && sharedInd >= 0)
    NoLongerInheritMessage(itemNum, sharedInd, "file properties");
  AddInheritingItems(itemNum, prevIndex, NAVARRAY_FILEOPT, inheritors);

  // For a new file option, add it to the array and dereference this item and any 
  // following ones that shared it from the old options and point them to the new one
  if (madeNewOpt) {
    newFileOpt->navID = mNav->MakeUniqueID();
    mFileOptArray->Add(newFileOpt);
    gotThisSched = false;
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);

      // Do the item itself only if not doing a file at group
      if (item2->mFilePropIndex == prevIndex && (prevIndex >= 0 ||
        (i == itemNum && listType != NAVFILE_GROUP))) {
          if (prevIndex >= 0)
            fileOptSrc->refCount--;
          item2->mFilePropIndex = (int)mFileOptArray->GetSize() - 1;
          newFileOpt->refCount++;
      }

      // Check and switch any scheduled groups too, keep track if do the passed schedule
      // A new one is not yet in array
      j = mNav->GroupScheduledIndex(item2->mGroupID);
      if (j >= 0) {
        sched2 = mGroupFiles->GetAt(j);
        if (sched2->filePropIndex == prevIndex && (prevIndex >= 0 ||
          (sched2 == sched && listType == NAVFILE_GROUP))) {
            if (prevIndex >= 0)
              fileOptSrc->refCount--;
            sched2->filePropIndex = (int)mFileOptArray->GetSize() - 1;
            newFileOpt->refCount++;
            if (sched2 == sched)
              gotThisSched = true;
        }
      }
    }
    if (listType == NAVFILE_GROUP && !gotThisSched) {
      sched->filePropIndex = (int)mFileOptArray->GetSize() - 1;
      newFileOpt->refCount++;
    }
  }
  BeInheritedByMessage(itemNum, inheritors, "Changes in file properties");

  // A successful pass through the dialog should (?) update the program master 
  mDocWnd->CopyMasterFileOpts(newFileOpt, COPY_TO_MASTER);
  return 0;
}

// Get a new filename for the item or group, defaulting the dialog to an existing one
// Return -1 if user cancels the dialog, or 1 if file already open, 2 if it is scheduled
// for a group, or 3 if it is scheduled for an item
int CNavHelper::SetOrChangeFilename(int itemNum, int listType, ScheduledFile *sched)
{
  int i, numacq, fileType = STORE_TYPE_MRC;
  CString filename, label, lastlab;
  ScheduledFile *sched2;
  CMapDrawItem *item2;
  LPCTSTR lpszFileName = NULL;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CString *namep = &item->mFileToOpen;
  FileOptions *fileOptp = NULL;
  i = item->mFilePropIndex;
  if (listType == NAVFILE_GROUP) {
    namep = &sched->filename;
    i = sched->filePropIndex;
  }
  if (i >= 0) {
    fileOptp = mFileOptArray->GetAt(i);
    fileType = fileOptp->useMont() ? fileOptp->montFileType : fileOptp->fileType;
  }
  filename = *namep;
  if (!namep->IsEmpty())
    lpszFileName = (LPCTSTR)filename;
  else if (mUseLabelInFilenames) {
    filename = item->mLabel;
    if (fileType == STORE_TYPE_MRC)
      filename += ".mrc";
    if (fileType == STORE_TYPE_TIFF)
      filename += ".tif";
    lpszFileName = (LPCTSTR)filename;
  }
  if (mDocWnd->FilenameForSaveFile(fileType, lpszFileName, filename))
    return -1;

    // Check for file already open
  i = mDocWnd->StoreIndexFromName(filename);
  if (i >= 0) {
    label.Format("This file is already open (file #%d).\n\nPick another file name,"
      " or close the file (in which case it will be overwritten)", i + 1);
    AfxMessageBox(label, MB_EXCLAME);
    return 1;
  }

  // Check for duplication with group files
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched2 = mGroupFiles->GetAt(i);
    if (listType == NAVFILE_GROUP && sched2->groupID == sched->groupID)
      continue;
    if (sched2->filename == filename) {
      mNav->CountItemsInGroup(sched2->groupID, label, lastlab, numacq);
      AfxMessageBox("This filename is already set up for the group\n"
        "with labels from " + label + " to " + lastlab + "\n\nPick another name.",
        MB_EXCLAME);
      return 2;
    }
  }

  // Check for duplication with other defined files for items
  for (i = 0; i < mItemArray->GetSize(); i++) {
    if (i == itemNum)
      continue;
    item2 = mItemArray->GetAt(i);
    if (item2->mFilePropIndex >= 0 && item2->mFileToOpen == filename) {
      label.Format("This filename is already set up to be opened for item #%d with"
        " label %s\r\nPick another name.", i, (LPCTSTR)item2->mLabel);
      AfxMessageBox(label, MB_EXCLAME);
      return 3;
    }
  }

  // Copy name and return for success
  *namep = filename;
  return 0;
}

void CNavHelper::NoLongerInheritMessage(int itemNum, int sharedInd, char * typeText)
{
  CString str;
  CMapDrawItem *item1 = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2 = mItemArray->GetAt(sharedInd);
  if (!mDoingMultipleFiles)
    return;
  str.Format("Item # %d (%s) no longer inherits changes in %s from item # %d (%s)", 
    itemNum + 1, (LPCTSTR)item1->mLabel, typeText, sharedInd + 1, (LPCTSTR)item2->mLabel);
  mWinApp->AppendToLog(str);
}

void CNavHelper::AddInheritingItems(int num, int prevIndex, int type, CString & listStr)
{
  CString str;
  CMapDrawItem *item;
  if (prevIndex < 0)
    return;
  for (int i = num + 1; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if ((type == NAVARRAY_FILEOPT && item->mFilePropIndex == prevIndex) ||
      (type == NAVARRAY_MONTPARAM && item->mMontParamIndex == prevIndex) ||
      (type == NAVARRAY_TSPARAM && item->mTSparamIndex == prevIndex)) {
        str.Format("%d (%s)", i + 1, (LPCTSTR)item->mLabel);
        if (!listStr.IsEmpty())
          listStr += ", ";
        listStr += str;
    }
  }
}

void CNavHelper::BeInheritedByMessage(int itemNum, CString & listStr, char * typeText)
{
  CString str;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  if (listStr.IsEmpty())
    return;
  str.Format("%s for item # %d (%s) are being inherited by these items:", 
    typeText, itemNum + 1, (LPCTSTR)item->mLabel);
  mWinApp->AppendToLog(str);
  mWinApp->AppendToLog("     " + listStr);
}

// Remove a param from one of the arrays and delete it, and decrease any references to 
// ones above it
void CNavHelper::RemoveFromArray(int which, int index)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (which == NAVARRAY_FILEOPT && item->mFilePropIndex > index)
      item->mFilePropIndex--;
    if (which == NAVARRAY_MONTPARAM && item->mMontParamIndex > index)
      item->mMontParamIndex--;
    if (which == NAVARRAY_TSPARAM && item->mTSparamIndex > index)
      item->mTSparamIndex--;
    if (which == NAVARRAY_STATE && item->mStateIndex > index)
      item->mStateIndex--;
  }
  for (i = 0; i < mGroupFiles->GetSize() && which != NAVARRAY_TSPARAM; i++) {
    sched = mGroupFiles->GetAt(i);
    if (which == NAVARRAY_FILEOPT && sched->filePropIndex > index)
      sched->filePropIndex--;
    if (which == NAVARRAY_MONTPARAM && sched->montParamIndex > index)
      sched->montParamIndex--;
    if (which == NAVARRAY_STATE && sched->stateIndex > index)
      sched->stateIndex--;
  }
  if (which == NAVARRAY_FILEOPT) {
    FileOptions *opt = mFileOptArray->GetAt(index);
    delete opt;
    mFileOptArray->RemoveAt(index);
  } else if (which == NAVARRAY_MONTPARAM) {
    MontParam *montp = mMontParArray->GetAt(index);
    delete montp;
    mMontParArray->RemoveAt(index);
  } else if (which == NAVARRAY_TSPARAM) {
    TiltSeriesParam *tsp = mTSparamArray->GetAt(index);
    delete tsp;
    mTSparamArray->RemoveAt(index);
  } else if (which == NAVARRAY_STATE) {
    StateParams *state = mAcqStateArray->GetAt(index);
    delete state;
    mAcqStateArray->RemoveAt(index);
  }
}

// Reduce the reference count of the given param, and remove it if it is zero
void CNavHelper::ChangeRefCount(int which, int index, int dir)
{
  int newCount;
  if (which == NAVARRAY_FILEOPT) {
    FileOptions *opt = mFileOptArray->GetAt(index);
    opt->refCount += dir;
    newCount = opt->refCount;
  } else if (which == NAVARRAY_MONTPARAM) {
    MontParam *montp = mMontParArray->GetAt(index);
    montp->refCount += dir;
    newCount = montp->refCount;
  } else if (which == NAVARRAY_TSPARAM) {
    TiltSeriesParam *tsp = mTSparamArray->GetAt(index);
    tsp->refCount += dir;
    newCount = tsp->refCount;
  }
  if (!newCount)
    RemoveFromArray(which, index);
}

// A garbage collection routine to handle cancels that leave things in strange states
void CNavHelper::RecomputeArrayRefs(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  FileOptions *opt;
  MontParam *montp;
  TiltSeriesParam *tsp;
  int i, loop, limit, fileInd, montInd, tsInd;

  // First zero all the references
  for (i = 0; i < mFileOptArray->GetSize(); i++) {
    opt = mFileOptArray->GetAt(i);
    opt->refCount = 0;
  }
  for (i = 0; i < mMontParArray->GetSize(); i++) {
    montp = mMontParArray->GetAt(i);
    montp->refCount = 0;
  }
  for (i = 0; i < mTSparamArray->GetSize(); i++) {
    tsp = mTSparamArray->GetAt(i);
    tsp->refCount = 0;
  }

  // Add up the references again
  limit = (int)mItemArray->GetSize();
  for (loop = 0; loop < 2; loop++) {
    for (i = 0; i < limit; i++) {
      if (loop) {
        sched = mGroupFiles->GetAt(i);
        fileInd = sched->filePropIndex;
        montInd = sched->montParamIndex;
        tsInd = -1;
      } else {
        item = mItemArray->GetAt(i);
        fileInd = item->mFilePropIndex;
        montInd = item->mMontParamIndex;
        tsInd = item->mTSparamIndex;
      }

      if (fileInd >= 0) {
        opt = mFileOptArray->GetAt(fileInd);
        opt->refCount++;
      }
      if (montInd >= 0) {
        montp = mMontParArray->GetAt(montInd);
        montp->refCount++;
      }
      if (tsInd >= 0) {
        tsp = mTSparamArray->GetAt(tsInd);
        tsp->refCount++;
      }
    }
    limit = (int)mGroupFiles->GetSize();
  }

  // Remove items with no references from top down
  for (i = (int)mFileOptArray->GetSize() - 1; i >= 0; i--) {
    opt = mFileOptArray->GetAt(i);
    if (!opt->refCount)
      RemoveFromArray(NAVARRAY_FILEOPT, i);
  }
  for (i = (int)mMontParArray->GetSize() - 1; i >= 0; i--) {
    montp = mMontParArray->GetAt(i);
    if (!montp->refCount)
      RemoveFromArray(NAVARRAY_MONTPARAM, i);
  }
  for (i = (int)mTSparamArray->GetSize() - 1; i >= 0; i--) {
    tsp = mTSparamArray->GetAt(i);
    if (!tsp->refCount)
      RemoveFromArray(NAVARRAY_TSPARAM, i);
  }
}

// See if an earlier item shares a reference to the given type of index
bool CNavHelper::EarlierItemSharesRef(int itemNum, int refInd, int which, int &shareInd)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  int i, index;
  shareInd = -1;
  for (i = 0; i < itemNum; i++) {
    item = mItemArray->GetAt(i);
    if (which == NAVARRAY_FILEOPT && item->mFilePropIndex == refInd ||
      which == NAVARRAY_MONTPARAM && item->mMontParamIndex == refInd) {
        shareInd = i;
        return true;
    }
    index = mNav->GroupScheduledIndex(item->mGroupID);
    if (index >= 0) {
      sched = mGroupFiles->GetAt(index);
      if (which == NAVARRAY_FILEOPT && sched->filePropIndex == refInd ||
        which == NAVARRAY_MONTPARAM && sched->montParamIndex == refInd)
        return true;
    }
  }
  return false;
}

// Takes care of removing references and resetting indexes when either acquire is
// turned off or tilt series or new file at item is. Acquire should already be set false
// if appropriate
void CNavHelper::EndAcquireOrNewFile(CMapDrawItem * item, bool endGroupFile)
{
  ScheduledFile *sched;
  CMapDrawItem *item2;
  int i, index;
  bool active = false;
  if (item->mTSparamIndex >= 0) {
    ChangeRefCount(NAVARRAY_TSPARAM, item->mTSparamIndex, -1);
    item->mTSparamIndex = -1;
  }
  if (!mWinApp->GetDummyInstance()) {
    if (!item->mAcquire && item->mTSparamIndex < 0) {
      item->mFocusAxisPos = EXTRA_NO_VALUE;
      item->mTargetDefocus = EXTRA_NO_VALUE;
    }
    if (item->mTSparamIndex < 0) {
      item->mTSstartAngle = EXTRA_NO_VALUE;
      item->mTSbidirAngle = EXTRA_NO_VALUE;
    }
  }
  if (item->mFilePropIndex >= 0) {
    ChangeRefCount(NAVARRAY_FILEOPT, item->mFilePropIndex, -1);
    item->mFilePropIndex = -1;
    if (item->mMontParamIndex >= 0)
      ChangeRefCount(NAVARRAY_MONTPARAM, item->mMontParamIndex, -1);
    item->mMontParamIndex = -1;
    item->mFileToOpen = "";
    if (item->mStateIndex >= 0)
      RemoveFromArray(NAVARRAY_STATE, item->mStateIndex);
    item->mStateIndex = -1;
  } else {
    index = mNav->GroupScheduledIndex(item->mGroupID);
    if (index >= 0) {
      
      // Are there other acquire items with the same group ID?
      for (i = 0; i < mItemArray->GetSize(); i++) {
        item2 = mItemArray->GetAt(i);
        if (item2->mGroupID == item->mGroupID && item2->mAcquire) {
          active = true;
          break;
        }
      }
      if (!active || endGroupFile) {
        sched = mGroupFiles->GetAt(index);
        ChangeRefCount(NAVARRAY_FILEOPT, sched->filePropIndex, -1);
        if (sched->montParamIndex >= 0)
          ChangeRefCount(NAVARRAY_MONTPARAM, sched->montParamIndex, -1);
        if (sched->stateIndex >= 0)
          RemoveFromArray(NAVARRAY_STATE, sched->stateIndex);
        delete sched;
        mGroupFiles->RemoveAt(index);
      }
    }
  }
}

// Gets the file type and returns the scheduled file object if any for an item
ScheduledFile * CNavHelper::GetFileTypeAndSchedule(CMapDrawItem * item, int & fileType)
{
  ScheduledFile *sched = NULL;
  int index = mNav->GroupScheduledIndex(item->mGroupID);
  fileType = -1;
  if (item->mTSparamIndex >= 0) {
    fileType = NAVFILE_TS;
  } else if (index >= 0) {
    sched = mGroupFiles->GetAt(index);
    fileType = NAVFILE_GROUP;
  } else if (item->mFilePropIndex >= 0)
    fileType = NAVFILE_ITEM;
  return sched;
}

// Clean up all the Navigator arrays for closing or reading new file
void CNavHelper::DeleteArrays(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  FileOptions *opt;
  MontParam *montp;
  TiltSeriesParam *tsp;
  StateParams *state;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    delete item;
  }   
  mItemArray->RemoveAll();
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched = mGroupFiles->GetAt(i);
    delete sched;
  }   
  mGroupFiles->RemoveAll();
  for (i = 0; i < mFileOptArray->GetSize(); i++) {
    opt = mFileOptArray->GetAt(i);
    delete opt;
  }   
  mFileOptArray->RemoveAll();
  for (i = 0; i < mMontParArray->GetSize(); i++) {
    montp = mMontParArray->GetAt(i);
    delete montp;
  }   
  mMontParArray->RemoveAll();
  for (i = 0; i < mTSparamArray->GetSize(); i++) {
    tsp = mTSparamArray->GetAt(i);
    delete tsp;
  }   
  mTSparamArray->RemoveAll();
  for (i = 0; i < mAcqStateArray->GetSize(); i++) {
    state = mAcqStateArray->GetAt(i);
    delete state;
  }   
  mAcqStateArray->RemoveAll();
}

// Make a new state param and add it to states here, or in navigator
StateParams *CNavHelper::NewStateParam(bool navAcquire)
{
  StateParams *param = new StateParams;

  if (navAcquire)
    mAcqStateArray->Add(param);
  else
    mStateArray.Add(param);
  return param;
}

// Return a state param by index from either states here or in navigator
StateParams *CNavHelper::ExistingStateParam(int index, bool navAcquire)
{
  if (index < 0 || (navAcquire && index >= mAcqStateArray->GetSize()) ||
    (!navAcquire && index >= mStateArray.GetSize()))
    return NULL;
  if (navAcquire)
    return mAcqStateArray->GetAt(index);
  else
    return mStateArray.GetAt(index);
}

void CNavHelper::OpenStateDialog(void)
{
  if (mStateDlg) {
    mStateDlg->BringWindowToTop();
    return;
  }
  mStateDlg = new CStateDlg();
  mStateDlg->Create(IDD_STATEDLG);
  if (mStatePlacement.rcNormalPosition.right > 0)
    mStateDlg->SetWindowPlacement(&mStatePlacement);
  mStateDlg->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetStatePlacement(void)
{
  if (mStateDlg)
    mStateDlg->GetWindowPlacement(&mStatePlacement);
  return &mStatePlacement;
}

// Update the dialogs we are responsible for
void CNavHelper::UpdateStateDlg(void)
{
  if (mStateDlg)
    mStateDlg->Update();
  if (mRotAlignDlg)
    mRotAlignDlg->Update();
}

/////////////////////////////////////////////////
/////  DUAL MAP  (THE ORIGINAL NAME)  NOW ANCHOR MAP
/////////////////////////////////////////////////

int CNavHelper::MakeDualMap(CMapDrawItem *item)
{
  EMimageBuffer *imBuf;
  KImageStore *store;
  int err, ind, curStore, numAvailable = 0, indAvail;
  int bufInd = mWinApp->Montaging() && mImBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW ?
    1 : 0;
  mIndexAfterDual = mNav->GetCurListSel();
  if (!item) {
    SEMMessageBox("Cannot find the item that was to be used for the map acquire state",
      MB_EXCLAME);
    return 1;
  }
  
  // Make a map if it is not already one and it is suitable
  imBuf = &mImBufs[bufInd];
  if (!imBuf->mMapID && imBuf->mMagInd > item->mMapMagInd) {
    err = mNav->NewMap(true);
    if (err > 0)
      return 2;
    if (!err) {
      mWinApp->AppendToLog("Made a new map from the existing image before getting"
      " anchor map");
      mIndexAfterDual = mNav->GetCurListSel();
    } else if (!bufInd) {

      // If this fails for buffer A, look for another non-montage file it can work in
      curStore = mDocWnd->GetCurrentStore();
      for (ind = 0; ind < mDocWnd->GetNumStores(); ind++) {
        if (ind == curStore || mDocWnd->GetStoreMontParam(ind))
          continue;
        store = mDocWnd->GetStoreMRC(ind);
        if (store && mBufferManager->IsBufferSavable(imBuf, store)) {
          numAvailable++;
          indAvail = ind;
        }
      }

      // If there is only one, switch to it, make map, switch back
      if (numAvailable == 1) {
        mDocWnd->SetCurrentStore(indAvail);
        err = mNav->NewMap(true);
        mDocWnd->SetCurrentStore(curStore);
        if (err > 0)
          return 2;
        if (!err) {
          mWinApp->AppendToLog("Saved existing image in only suitable file and made it"
            " a new map before getting anchor map");
          mIndexAfterDual = mNav->GetCurListSel();
        }

        // Otherwise give message or warning
      } else if (numAvailable) {
        if (SEMMessageBox("The existing image in buffer A cannot be made into a\n"
          "map because it could be saved into more than one open file but not the current"
          " file.\n\nDo you want to stop making an anchor map and\nswitch to the "
          "appropriate file for saving the existing image first?", MB_QUESTION) == IDYES)
          return 1;
      } else {
        mWinApp->AppendToLog("WARNING: could not make a map from the existing image in "
          "buffer A\r\n""   because there is no open file into which it can be saved");
      }
    }
  }

  // If already in a map acquire state, save the ID of that and restore before new state
  // But if in any other set state, forget the prior state so we return to this state
  mSavedMapStateID = 0;
  if (mTypeOfSavedState == STATE_MAP_ACQUIRE) {
    mSavedMapStateID = mMapStateItemID;
    RestoreFromMapState();
  } else {
    ForgetSavedState();
  }

  // Go to imaging state, apply any defocus offset
  if (SetToMapImagingState(item, true, 1)) {
    RestoreFromMapState();
    return 3;
  }
  if (!mRIstayingInLD)
    SetMapOffsetsIfAny(item);

  mAcquiringDual = true;
  mWinApp->UpdateBufferWindows();
  if (item->mMapMontage) {
    mWinApp->StartMontageOrTrial(false);
  } else {
    mCamera->SetCancelNextContinuous(true);
    mWinApp->mCamera->InitiateCapture(mRIstayingInLD ? mRIconSetNum : RECORD_CONSET);
  }

  mWinApp->AddIdleTask(TASK_DUAL_MAP, 0, 0);
  mWinApp->SetStatusText(COMPLEX_PANE, "MAKING ANCHOR MAP");
  return 0;
}

int CNavHelper::DualMapBusy(void)
{
  int err = mCamera->TaskCameraBusy();
  if (err < 0)
    return err;
  return (err || mWinApp->mMontageController->DoingMontage()) ? 1 : 0;
}

void CNavHelper::DualMapDone(int param)
{
  CMapDrawItem *item;
  int savedIndex = -1;
  if (!mAcquiringDual)
    return;
  if (!(mWinApp->Montaging() && mWinApp->mMontageController->GetLastFailed())) {
    if (!mWinApp->Montaging()) {
      if (!mBufferManager->IsBufferSavable(mImBufs)) {
        AfxMessageBox("The file into which the anchor map was to be saved has changed\n"
          "and this image cannot be saved there", MB_EXCLAME);
        StopDualMap();
        return;
      }
      mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC);
    }

    if (!mScope->GetSimulationMode())
      mNav->SetSkipBacklashType(2);
    if (!mNav->NewMap()) {
      item = mNav->GetCurrentItem();
      if (item) {

        // restore nav current item; adjust note and copy the defocus offset
        savedIndex = mNav->GetCurrentIndex();
        mNav->SetCurListSel(mIndexAfterDual);
        item->mNote = "Anchor - " + item->mNote;
        mNav->UpdateListString((int)mItemArray->GetSize() - 1);
        if (!mRIstayingInLD) {
          item->mDefocusOffset = (float)mRIdefocusOffsetSet;
          item->mMapAlpha = mRIalphaSet;
        }
      }
    }
  }
  StopDualMap();
  if (savedIndex >= 0)
    mNav->AdjustBacklash(savedIndex, true);
}

void CNavHelper::DualMapCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out acquiring anchor map"), MB_EXCLAME);
  StopDualMap();
  mWinApp->ErrorOccurred(error);
}

void CNavHelper::StopDualMap(void)
{
  CMapDrawItem *item;
  if (!mAcquiringDual)
    return;
  mAcquiringDual = false;
  RestoreMapOffsets();
  RestoreFromMapState();

  // Restore the prior map imaging state if any and if it still exists
  if (mSavedMapStateID) {
    item = mNav->FindItemWithMapID(mSavedMapStateID);
    if (item)
      SetToMapImagingState(item, true);
  }
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  mWinApp->UpdateBufferWindows();
}

/////////////////////////////////////////////////
/////  ACQUIRE-RELATED ROUTINES, CHECKING AND LISTING
/////////////////////////////////////////////////

// Check for problems in the set of items/files/etc to acquire
int CNavHelper::AssessAcquireProblems(int startInd, int endInd)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  MontParam *montp;
  StateParams *state;
  TiltSeriesParam *tsp;
  CString mess, mess2, label;
  CString *names = mWinApp->GetModeNames();
  int montParInd, stateInd, camMismatch, binMismatch, numNoMap, numAtEdge, err;
  int *activeList = mWinApp->GetActiveCameraList();
  ControlSet *masterSets = mWinApp->GetCamConSets();
  NavAcqParams *navParam = mWinApp->GetNavAcqParams(mCurAcqParamIndex);
  mAcqActions = mAllAcqActions[mCurAcqParamIndex];
  int lastBin[MAX_CAMERAS];
  int cam, bin, i, j, k, ind, stateCam, numBroke, numGroups, curGroup, fileOptInd, setNum;
  bool seen;
  BOOL savingMulti = navParam->acquireType == ACQUIRE_MULTISHOT && IsMultishotSaving();
  int *seenGroups;

  camMismatch = 0;
  binMismatch = 0;
  numNoMap = numAtEdge = numBroke = numGroups = 0;
  for (i = 0; i < MAX_CAMERAS; i++)
    lastBin[i] = -1;

  // Check the user actions
  for (ind = 0; ind < mNumAcqActions; ind++) {
    if (DOING_ACTION(ind) && (mAcqActions[ind].flags &NAA_FLAG_OTHER_SITE)) {
      if (mAcqActions[ind].labelOrNote.IsEmpty()) {
        SEMMessageBox("There is no text for the " +
          CString((mAcqActions[ind].flags & NAA_FLAG_MATCH_NOTE) ? "note" : "label") +
          " for doing " + mAcqActions[ind].name + " at the nearest matching item");
        return 1;
      }
      if (FindNearestItemMatchingText(0., 0., mAcqActions[ind].labelOrNote,
        (mAcqActions[ind].flags & NAA_FLAG_MATCH_NOTE) != 0) < 0) {
        SEMMessageBox("There is no item whose " +
          CString((mAcqActions[ind].flags & NAA_FLAG_MATCH_NOTE) ? "note" : "label") +
          " starts with " + mAcqActions[ind].labelOrNote + " for doing " +
          mAcqActions[ind].name + " at the nearest matching item");
        return 1;
      }
    }
  }
  if (DOING_ACTION(NAACT_ALIGN_TEMPLATE)) {
    item = mNav->FindItemWithString(mNavAlignParams.templateLabel, false, true);
    if (!item)
      mess = "Cannot find the template map in Navigator table for aligning to template";
    else if (item->IsNotMap())
      mess = "The item matching the label for aligning to template is not a map";
    else if (item->mMapMontage)
      mess = "The item matching the label for aligning to template is a montage";
    if (!mess.IsEmpty()) {
      SEMMessageBox(mess);
      return 1;
    }
  }
  if (FEIscope && DOING_ACTION(NAACT_FLASH_FEG) &&
    mScope->GetAdvancedScriptVersion() < ASI_FILTER_FEG_LOAD_TEMP) {
    SEMMessageBox("The version of advanced scripting has not been identified as high "
      "enough to support FEG flashing", MB_EXCLAME);
    return 1;
  }

  seenGroups = new int[mItemArray->GetSize()];
  for (i = startInd; i <= endInd; i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration() && 
      ((item->mAcquire && navParam->acquireType != ACQUIRE_DO_TS) || 
      (item->mTSparamIndex >=0 && navParam->acquireType == ACQUIRE_DO_TS))) {

      // Get parameter index for item or group, ignore current group
      montParInd = item->mMontParamIndex;
      fileOptInd = item->mFilePropIndex;
      stateInd = item->mStateIndex;
      j = mNav->GroupScheduledIndex(item->mGroupID);
      if (j >= 0 && item->mGroupID != curGroup) {
        sched = mGroupFiles->GetAt(j);

        // Check for file open
        k = mDocWnd->StoreIndexFromName(sched->filename);
        if (k >= 0) {
          mNav->CountItemsInGroup(sched->groupID, label, mess2, j);
          mess.Format("The file set to be opened for the group with\n"
            "labels from %s to %s is already open."
            "\n(file # %d, %s)\n\nYou should set a different name for the file to open,"
            "\nor close the file (in which case it will be overwritten)", (LPCTSTR)label,
            (LPCTSTR)mess2, k + 1, sched->filename);
          AfxMessageBox(mess, MB_EXCLAME);
          delete[] seenGroups;
          return 1;
        }

        // First see if group has been seen before
        seen = false;
        for (k = 0; k< numGroups; k++) {
          if (item->mGroupID == seenGroups[k]) {
            seen = true;
            break;
          }
        }

        // If seen, it is bad; if not get parameter indices and add to seen list
        // Either way, make it current group so further items in this group are ignored
        curGroup = item->mGroupID;
        if (seen)
          numBroke++;
        else {
          montParInd = sched->montParamIndex;
          fileOptInd = sched->filePropIndex;
          stateInd = sched->stateIndex;
          seenGroups[numGroups++] = curGroup;
          if (savingMulti && montParInd >= 0) {
            mNav->CountItemsInGroup(sched->groupID, label, mess2, j);
            mess.Format("The file set to be opened for the group with"
              " labels from %s to %s is set up for montaging.\n\n"
              "This will not work when taking Multiple Records.\n"
              "You must set all files to open to be single-frame.",
              (LPCTSTR)label, (LPCTSTR)mess2);
            AfxMessageBox(mess, MB_EXCLAME);
            delete[] seenGroups;
            return 1;
          }
        }
      }

      // Check file already open
      if (item->mFilePropIndex >= 0) {
        k = mDocWnd->StoreIndexFromName(item->mFileToOpen);
        if (k >= 0) {
          mess.Format("The file set to be opened for item # %d, label %s is already open"
            "\n(file # %d, %s)\n\nYou should set a different name for the file to open,"
            "\nor close the file (in which case it will be overwritten)", i + 1, 
            item->mLabel, k + 1, item->mFileToOpen);
          AfxMessageBox(mess, MB_EXCLAME);
          delete[] seenGroups;
          return 1;
        }
        if (item->mMontParamIndex >= 0 && savingMulti) {
          mess.Format("The file set to be opened for item # %d, label %s is set up for"
            " montaging.\n\n" "This will not work when taking Multiple Records.\n"
            "You must set all files to open to be single-frame.", i + 1, item->mLabel);
          AfxMessageBox(mess, MB_EXCLAME);
          delete[] seenGroups;
          return 1;
        }
      }

      // If there is new file for item not group, cancel current group
      if (j < 0 && item->mFilePropIndex >= 0)
        curGroup = 0;

      // Check that realign to map can work if requested now that file indices are known
      if (DOING_ACTION(NAACT_REALIGN_ITEM)) {
        mNav->SetNextFileIndices(fileOptInd, montParInd);
        err = FindMapForRealigning(item, navParam->restoreOnRealign || 
          navParam->acquireType != ACQUIRE_RUN_MACRO);
        if (err == 4)
          numNoMap++;
        if (err == 5)
          numAtEdge++;
      }

      // Get camera and binning from state first 
      stateCam = -1;
      if (stateInd >= 0) {
        state = mAcqStateArray->GetAt(stateInd);
        for (j = 0; j < mWinApp->GetNumActiveCameras(); j++) {
          if (activeList[j] == state->camIndex) {
            stateCam = j;
            lastBin[j] = state->binning;
            break;
          }
        }
      }

      // Get defined camera and binning from the parameter sets
      setNum = RECORD_CONSET;
      if (montParInd >= 0) {
        montp = mMontParArray->GetAt(montParInd);
        cam = montp->cameraIndex;
        bin = montp->binning;
        setNum = MontageConSetNum(montp, true);
      } else if (item->mTSparamIndex >= 0) {
        tsp = mTSparamArray->GetAt(item->mTSparamIndex);
        cam = tsp->cameraIndex;
        bin = tsp->binning;
      } else
        continue;

      // If the camera does not match the one defined in this item's state, it's mismatch
      if (stateCam >= 0 && cam != stateCam)
        camMismatch++;

      // No binning defined by state defined yet: get the binning from the camera conset
      if (lastBin[cam] < 0)
        lastBin[cam] = masterSets[activeList[cam] * MAX_CONSETS + setNum].binning;

      // See "Adjust exposure..." in StartMontage for this exception
      if (lastBin[cam] != bin && !(montParInd >= 0 && mWinApp->LowDoseMode() && 
        bin > lastBin[cam] && bin <= 2 * lastBin[cam]))
        binMismatch++;

      // This param binning becomes the binning for next time unless there is a state
      lastBin[cam] = bin;
    }
  }

  delete [] seenGroups;

  if (numNoMap || numAtEdge) {
    mess = "You selected 'Realign to Item' but:\n";
    if (numNoMap) {
      mess2.Format("    %d items are not located within a map that can be used for"
        " realigning\n", numNoMap);
      mess += mess2;
    }
    if (numAtEdge) {
      mess2.Format("    %d items are located too close to the edge of a map for "
        "realigning to be reliable\n", numAtEdge);
      mess += mess2;
    }
    mess += "\nDo you want to proceed anyway and just go to the\n"
      "nominal stage coordinates for those items?";
    if (AfxMessageBox(mess, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return 1;
  }

  if (numBroke) {
    mess.Format("There is a new file opened for another item in the middle of a group "
      "with a designated file, so some of the items in that group would be "
      "acquired into the new file.  This happens %d times.\n\nPress:\n"
      "\"Stop && Fix\" to stop and try to fix this,\n\"Go On\" to just go on.", numBroke);
    if (SEMThreeChoiceBox(mess, "Stop && Fix", "Go On", "", MB_YESNO | MB_ICONQUESTION) ==
      IDYES)
      return 1;
  }

  if (camMismatch || binMismatch) {
    mess = "";
    if (camMismatch)
      mess.Format("There are %d mismatches between the camera specified in the montage "
      "or tilt series parameters for an item and the camera in the imaging state "
      "to be set for that item.  This is odd.\n\n", camMismatch);
    if (binMismatch) {
      mess2.Format("The binning specified in the montage or tilt series parameters for "
        "an item differs from the existing binning for the "
        "particular camera and parameter set. "
        "This happens %d times.\nThe exposure time would be changed to compensate, but"
      " this is probably undesirable\n\n", binMismatch);
      mess += mess2;
    }
    mess += "Press:\n\"Stop && Fix\" to stop and fix these problems by resetting state"
      " parameters,\n\n"
      "\"Go On\" to go on regardless of these problems.";
    if (SEMThreeChoiceBox(mess, "Stop && Fix", "Go On", "", MB_YESNO | MB_ICONQUESTION) ==
      IDYES)
      return 1;
  }
  return 0;
}

// Test if multishot would save with current params and set allZero if it is because
// it is set for all empty early returns 
BOOL CNavHelper::IsMultishotSaving(bool *allZeroER)
{
  if (allZeroER)
    *allZeroER = false;
  UpdateMultishotIfOpen(false);
  if (mWinApp->GetHasK2OrK3Camera() && mMultiShotParams.doEarlyReturn == 2 &&
    !mMultiShotParams.numEarlyFrames) {
    if (allZeroER)
      *allZeroER = true;
    return false;
  }
  return mMultiShotParams.saveRecord;
}

// Find the item whose note or label starts with the given text
int CNavHelper::FindNearestItemMatchingText(float stageX, float stageY, CString &text,
  bool matchNote)
{
  int ind, nearPosInd = -1;
  double dist, minDist = 1.e30;
  CMapDrawItem *item;
  CString *strPtr;
  for (ind = 0; ind < mItemArray->GetSize(); ind++) {
    item = mItemArray->GetAt(ind);
    strPtr = matchNote ? &item->mNote : &item->mLabel;
    if (strPtr->Find(text) == 0) {
      dist = sqrt(pow(stageX - item->mStageX, 2.f) +
        pow(stageY - item->mStageY, 2.f));
      if (dist < minDist) {
        nearPosInd = ind;
        minDist = dist;
      }
    }
  }
  return nearPosInd;
}

// Return whether we are acquiring with no MB on error set
BOOL CNavHelper::GetNoMessageBoxOnError()
{
  NavAcqParams *params;
  if (!mNav || !mNav->GetAcquiring())
    return false;
  params = mWinApp->GetNavAcqParams(mCurAcqParamIndex);
  return params->noMBoxOnError;
}

// List files, tilt series, montages, and states
void CNavHelper::ListFilesToOpen(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  MontParam *montp;
  StateParams *state;
  TiltSeriesParam *tsp;
  CString mess, mess2, label, lastlab;
  CString *namep;
  int mag, active, magInd, spot, probe;
  double intensity;
  int *activeList = mWinApp->GetActiveCameraList();
  MagTable *magTab = mWinApp->GetMagTable();
  CameraParameters *camp;
  int montParInd, stateInd, num, numacq;
  int i, j, k, numGroups = 0;
  float starting, ending, bidir;
  ControlSet *focusSet = mWinApp->GetConSets() + FOCUS_CONSET;
  bool seen, single = false;

  if (!mItemArray->GetSize())
    return;
  int *seenGroups = new int[mItemArray->GetSize()];
  mWinApp->AppendToLog("\r\nListing of files to open and states to be set", LOG_OPEN_IF_CLOSED);
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration() && 
      (item->mAcquire || item->mTSparamIndex >= 0)) {

      namep = NULL;
      if (item->mFilePropIndex >= 0) {
        namep = &item->mFileToOpen;
        montParInd = item->mMontParamIndex;
        stateInd = item->mStateIndex;
      }
      j = mNav->GroupScheduledIndex(item->mGroupID);
      if (j >= 0) {

        // First see if group has been seen before
        seen = false;
        for (k = 0; k< numGroups; k++) {
          if (item->mGroupID == seenGroups[k]) {
            seen = true;
            break;
          }
        }
        if (!seen) {
          sched = mGroupFiles->GetAt(j);
          montParInd = sched->montParamIndex;
          stateInd = sched->stateIndex;
          namep = &sched->filename;
          seenGroups[numGroups++] = item->mGroupID;
        }
      }

      if (namep) {
        if (j >= 0) {
          num = mNav->CountItemsInGroup(sched->groupID, label, lastlab, numacq);
          mess.Format("Group of %d items (%d set to Acquire), labels from %s to %s",
            num, numacq, (LPCTSTR)label, (LPCTSTR)lastlab);
        } else if (item->mTSparamIndex >= 0) {
          tsp = mTSparamArray->GetAt(item->mTSparamIndex);
          camp = mWinApp->GetActiveCamParam(tsp->cameraIndex);
          starting = tsp->startingTilt;
          ending = tsp->endingTilt;
          bidir = tsp->bidirAngle;
          if (item->mTSstartAngle > EXTRA_VALUE_TEST) {
            starting = item->mTSstartAngle;
            ending = item->mTSendAngle;
            if (item->mTSbidirAngle > EXTRA_VALUE_TEST)
              bidir = item->mTSbidirAngle;
          }
          mess2.Format("%.1f to %.1f at %.2f", starting, ending, tsp->tiltIncrement);
          if (tsp->doBidirectional) {
            mess.Format(", start %.1f", bidir);
            mess2 += mess;
          }
          mess.Format("Tilt series with camera %d,   binning %d,   %s "
            "deg%s,  item # %d,  label %s", tsp->cameraIndex + 1,
            tsp->binning / BinDivisorI(camp), (LPCTSTR)mess2, 
            item->mTSstartAngle > EXTRA_VALUE_TEST ? "*" : "", i, (LPCTSTR)item->mLabel);
          single = true;
        } else {
          mess.Format("Single item # %d,  label %s", i, (LPCTSTR)item->mLabel);
          single = true;
        }
        mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
        mess = CString("   ") + *namep;
        if (montParInd >= 0) {
          montp = mMontParArray->GetAt(montParInd);
          mess2.Format("     %d x %d montage", montp->xNframes, montp->yNframes);
          mess += mess2;
          if (item->mTSparamIndex < 0) {
            camp = mWinApp->GetActiveCamParam(montp->cameraIndex);
            mess2.Format("   with camera %d   binning %d", montp->cameraIndex + 1,
              montp->binning / BinDivisorI(camp));
            mess += mess2;
          }
        }
        mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
        if (stateInd >= 0) {
          state = mAcqStateArray->GetAt(stateInd);
          magInd = state->lowDose ? state->ldParams.magIndex : state->magIndex;
          camp = mWinApp->GetCamParams() + state->camIndex;
          mag = MagForCamera(camp, magInd);
          active = mWinApp->LookupActiveCamera(state->camIndex) + 1;
          spot = state->lowDose ? state->ldParams.spotSize : state->spotSize;
          intensity = state->lowDose ? state->ldParams.intensity : state->intensity;
          probe = state->lowDose ? state->ldParams.probeMode : state->probeMode;

          mess.Format("   New state:  %s   cam %d   mag %d   spot %d   %s %.2f%s   exp "
            "%.3f   bin %d   %dx%d", state->lowDose ? "LD" : "", active, mag, spot,
            mScope->GetC2Name(), mScope->GetC2Percent(spot, intensity, probe),
            mScope->GetC2Units(), state->exposure,
            state->binning / BinDivisorI(camp), state->xFrame, state->yFrame);
          if (state->lowDose) {
            mess2.Format("  F pos %.2f angle %d off %d %d", state->focusAxisPos,
              state->axisRotation, state->focusXoffset / focusSet->binning,
              state->focusYoffset / focusSet->binning);
            mess += mess2;
          }
          mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
        }
      }
      if (item->mFocusAxisPos > EXTRA_VALUE_TEST || 
        item->mTargetDefocus > EXTRA_VALUE_TEST) {
        mess = "";
        if (!single)
          mess.Format("Item # %d,  label %s", i, (LPCTSTR)item->mLabel);
        if (item->mFocusAxisPos > EXTRA_VALUE_TEST) {
          mess2.Format("  F pos %.2f angle %d off %d %d", item->mFocusAxisPos,
            item->mFocusAxisAngle, item->mFocusXoffset / focusSet->binning,
            item->mFocusYoffset / focusSet->binning);
          mess += mess2;
        }
        if (item->mTargetDefocus > EXTRA_VALUE_TEST) {
          mess2.Format("  F targ %.2f", item->mTargetDefocus);
          mess += mess2;
        }
        mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
      }
    }
  }
  delete [] seenGroups;

}

// return count of items marked for acquire and tilt series, or -1 if Nav not open
void CNavHelper::CountAcquireItems(int startInd, int endInd, int & numAcquire, int & numTS)
{
  CMapDrawItem *item;
  if (!mNav) {
    numAcquire = -1;
    numTS = -1;
    return;
  }
  if (endInd < 0)
    endInd = (int)mItemArray->GetSize() - 1;
  numAcquire = 0;
  numTS = 0;
  for (int i = startInd; i <= endInd; i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration()) {
      if (item->mAcquire)
        numAcquire++;
      if (item->mTSparamIndex >= 0)
        numTS++;
    }
  }
}

// Return number of positions and number of holes to be acquired with current or item
// multishot parameters, excluding ones that would acquire fewer than minHoles.
void CNavHelper::CountHoleAcquires(int startInd, int endInd, int minHoles, 
  int &numCenters, int &numHoles, int &numRecs)
{
  CMapDrawItem *item;
  int numDefault, numForItem, xt, yt;
  if (!mNav) {
    numCenters = -1;
    numHoles = -1;
    return;
  }
  if (endInd < 0 || endInd >= (int)mItemArray->GetSize())
    endInd = (int)mItemArray->GetSize() - 1;
  startInd = B3DMAX(0, startInd);
  numCenters = 0;
  numHoles = 0;

  // Get the default from parameters
  if (mMultiShotDlg)
    mMultiShotDlg->UpdateAndUseMSparams(false);
  GetNumHolesFromParam(xt, yt, numDefault);

  // Get value for each item to be acquired in range
  for (int i = startInd; i <= endInd; i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration() && item->mAcquire) {
      numForItem = GetNumHolesForItem(item, numDefault);
      if (numForItem >= minHoles) {
        numCenters++;
        numHoles += numForItem;
      }
    }
  }
  xt = 1;
  if (mMultiShotParams.inHoleOrMultiHole & 1)
    xt = (mMultiShotParams.doCenter ? 1 : 0) + mMultiShotParams.numShots[0] + 
    (mMultiShotParams.doSecondRing ? mMultiShotParams.numShots[1] : 0);
  numRecs = numHoles * xt;
}

// Return true if there are any montage maps 
bool CNavHelper::AnyMontageMapsInNavTable()
{
  CMapDrawItem *item;
  if (!mNav)
    return false;
  for (int ind = 0; ind < (int)mItemArray->GetSize(); ind++) {
    item = mItemArray->GetAt(ind);
    if (item->IsMap() && item->mMapMontage)
      return true;
  }
  return false;
}

// See if a read-in image is actually a map and return the map ID, except when a map is
// being loaded
int CNavHelper::FindMapIDforReadInImage(CString filename, int secNum, int ignoreLoad)
{
  CString navDir, navPath, str;
  CMapDrawItem *item;
  CFileStatus status;
  if (!mNav || (!ignoreLoad && mNav->GetLoadingMap()))
    return 0;
  mNav->GetCurrentNavDir(navDir);
  while (navDir.Replace("\\\\", "\\")) {};
  while (filename.Replace("\\\\", "\\")) {};
  for (int ind = 0; ind < (int)mItemArray->GetSize(); ind++) {
    item = mItemArray->GetAt(ind);
    if (item->IsMap() && secNum == item->mMapSection) {
      str = item->mMapFile;
      while (str.Replace("\\\\", "\\")) {};
      if (str == filename)
        return item->mMapID;
      navPath = navDir + item->mTrimmedName;
      if (navPath == filename && !CFile::GetStatus((LPCTSTR)item->mMapFile, status))
        return item->mMapID;
    }
  }
  return 0;
}

// Check the validity of tilt series angles entered in script call or when about to be
// used and return error message
int CNavHelper::CheckTiltSeriesAngles(int paramInd, float start, float end, float bidir,
  CString &errMess)
{
  float minAng = B3DMIN(start, end);
  float maxAng = B3DMAX(start, end);
  float minSep = 6.;
  TiltSeriesParam *tsp = mTSparamArray->GetAt(paramInd);
  if (fabs(start) > mScope->GetMaxTiltAngle() || fabs(end) > mScope->GetMaxTiltAngle()) {
    errMess.Format("The starting or ending tilt angle (%.1f and %.1f) stored in the "
      "Navigator item is beyond the maximum angle (%.1f)", start, end,
      mScope->GetMaxTiltAngle());
    return 1;
  }
  if (maxAng < minAng + 2.) {
    errMess.Format("The starting or ending tilt angle (%.1f and %.1f) stored in the "
      "Navigator item do not have enough range", start, end);
    return 1;
  }
  if (bidir > EXTRA_VALUE_TEST && tsp->doBidirectional && 
    (bidir < minAng + minSep || bidir > maxAng - minSep)) {
    errMess.Format("The bidirectional starting angle (%.1f) stored in the "
      "Navigator item is too close to one end of the range (%.1f to %.1f)", bidir, start,
      end);
    return 1;
  }
  return 0;
}

// Set a user value in the map: returns 1 for number out of range
int CNavHelper::SetUserValue(CMapDrawItem *item, int number, CString &value)
{
  if (number < 1 || number > MAX_NAV_USER_VALUES)
    return 1;
  std::string sstr = (LPCTSTR)value;
  std::map<int, std::string>::iterator iter;
  if (item->mUserValueMap.count(number)) {
    iter = item->mUserValueMap.find(number);
    iter->second = sstr;
  } else
    item->mUserValueMap.insert(std::pair<int, std::string>(number, sstr));
  return 0;
}

// Get a user value from the map; returns 1 for number out of range and -1 if not found
int CNavHelper::GetUserValue(CMapDrawItem *item, int number, CString &value)
{
  std::map<int, std::string>::iterator iter;
  if (number < 1 || number > MAX_NAV_USER_VALUES)
    return 1;
  if (item->mUserValueMap.count(number)) {
    iter = item->mUserValueMap.find(number);
    value = iter->second.c_str();
    return value.IsEmpty() ? -1 : 0;
  }
  return -1;
}

// Align an image, searching for best rotation, with possible scaling as well
int CNavHelper::AlignWithRotation(int buffer, float centerAngle, float angleRange, 
                                  float &rotBest, float &shiftXbest, float &shiftYbest,
                                  float scaling)
{
  float step = 1.5f;
  int numSteps = B3DNINT(angleRange / step) + 1;
  int ist, idir, istMax, numStepCuts = 5;
  float peakmax = -1.e30f;
  float rotation, curMax, peak, shiftX, shiftY, CCC, fracPix;
  float peakBelow = -1.e30f, peakAbove = -1.e30f;
  float *CCCp = &CCC, *fracPixP = &fracPix;
  double overlapPow = 0.166667;
  FloatVec vecRot, vecPeak;
  CString report;

  if (!scaling) {
    CCCp = NULL;
    fracPixP = NULL;
  }

  step = 0.;
  if (numSteps > 1)
    step = angleRange / (numSteps - 1);

  // Scan for the number of steps
  for (ist = 0; ist < numSteps; ist++) {
    rotation = centerAngle - 0.5f * angleRange + (float)ist * step;
    if (mShiftManager->AutoAlign(buffer, 0, false, false, &peak, 0., 0., 0., scaling, 
      rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
      return 1;
    if (CCCp)
      peak = (float)(CCC * pow((double)fracPix, overlapPow));
    vecRot.push_back(rotation);
    vecPeak.push_back(peak);
    if (peak > peakmax) {
      peakmax = peak;
      rotBest = rotation;
      istMax = ist;
      shiftXbest = shiftX;
      shiftYbest = shiftY;
    }
    SEMTrace('1', "Rotation %.2f  peak  %g  shift %.1f %.1f", rotation, peak,
      shiftX, shiftY);
  }

  if (numSteps > 2 && (istMax == 0 || istMax == numSteps - 1)) {
    mWinApp->AppendToLog("The best correlation was found at the end of the range tested;"
    "\r\nyou should redo this with a different angular range");
    if (CCCp)
      return 1;
  }

  // Cut the step and look on either side of the peak 
  if (numSteps > 2 && !(istMax == 0 || istMax == numSteps - 1)) {
    for (ist = 0; ist < numStepCuts; ist++) {
      step /= 2.f;
      curMax = rotBest;
      for (idir = -1; idir <= 1; idir += 2) {
        rotation = curMax + idir * step;
        if (mShiftManager->AutoAlign(buffer, 0, false, false, &peak, 0., 0., 0., scaling,
          rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
          return 1;
        if (CCCp)
          peak = (float)(CCC * pow((double)fracPix, overlapPow));

        // When autoalign is called with shift pointers, it does not put the shift in the
        // image, so it needs to come from the call not the image
        vecRot.push_back(rotation);
        vecPeak.push_back(peak);
        if (peak > peakmax) {
          peakmax = peak;
          rotBest = rotation;
          shiftXbest = shiftX;
          shiftYbest = shiftY;
        }
        SEMTrace('1', "Rotation %.2f  peak  %g  shift %.1f %.1f", rotation, 
          peak, shiftX, shiftY);
      }
    }
  }

  // Get interpolated value
  UtilInterpolatePeak(vecRot, vecPeak, step, peakmax, rotBest);

  mWinApp->SetCurrentBuffer(0);
  report.Format("The two images correlate most strongly with a rotation of %.2f",
    rotBest);
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  return 0;
}

int CNavHelper::OKtoAlignWithRotation(void)
{
  ScaleMat aMat;
  float delX, delY;
  int readBuf = mWinApp->mBufferManager->GetBufToReadInto();
  int registration, topBuf;
  topBuf = BufferForRotAlign(registration);
  if (!mImBufs[topBuf].mImage || !mNav->BufferStageToImage(&mImBufs[topBuf], aMat, delX,
    delY) || mImBufs[topBuf].mCaptured == BUFFER_PROCESSED ||
    mImBufs[topBuf].mCaptured == BUFFER_FFT)
    return 0;
  if (!mImBufs[readBuf].mImage || !mImBufs[readBuf].mMapID || 
    mImBufs[readBuf].mRegistration == registration || 
    !mNav->FindItemWithMapID(mImBufs[readBuf].mMapID))
    return 0;
  return 1;
}

void CNavHelper::OpenRotAlignDlg(void)
{
  if (mRotAlignDlg) {
    mRotAlignDlg->BringWindowToTop();
    return;
  }
  mRotAlignDlg = new CNavRotAlignDlg();
  mRotAlignDlg->Create(IDD_NAVROTALIGN);
  mWinApp->SetPlacementFixSize(mRotAlignDlg, &mRotAlignPlace);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetRotAlignPlacement(void)
{
  if (mRotAlignDlg) {
    mRotAlignDlg->GetWindowPlacement(&mRotAlignPlace);
    mRotAlignDlg->UnloadData();
  }
  return &mRotAlignPlace;
}

// Returns the buffer number for rotation align, and the registration of it
int CNavHelper::BufferForRotAlign(int &registration)
{
  int topBuf = 0;
  if (mImBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW &&
    mImBufs[0].mCaptured == BUFFER_MONTAGE_CENTER)
    topBuf = 1;
  registration = mImBufs[topBuf].mRegistration ? mImBufs[topBuf].mRegistration :
    mNav->GetCurrentRegistration();
  return topBuf;
}

int CNavHelper::AlignWithScaling(float & shiftX, float & shiftY, float & scaling)
{
  if (mWinApp->mMultiTSTasks->AlignWithScaling(1, false, scaling, 
    mPlusMinusRIScaling ? -1.f : 0.f)) {
    scaling = 0;
    return 1;
  }
  mImBufs->mImage->getShifts(shiftX, shiftY);
  return 0;
}

// Opens the multishot dialog or raises it, manages position
void CNavHelper::OpenMultishotDlg(void)
{
  int *activeList = mWinApp->GetActiveCameraList();
  if (mMultiShotDlg) {
    mMultiShotDlg->BringWindowToTop();
    return;
  }
  mMultiShotDlg = new CMultiShotDlg();
  mMultiShotDlg->mHasIlluminatedArea = mScope->GetUseIllumAreaForC2();
  mMultiShotDlg->mCanReturnEarly = false;
  for (int ind = 0; ind < mWinApp->GetActiveCamListSize(); ind++)
    if (mCamParams[activeList[ind]].K2Type)
      mMultiShotDlg->mCanReturnEarly = true;
  mMultiShotDlg->Create(IDD_MULTI_SHOT_SETUP);
  mWinApp->SetPlacementFixSize(mMultiShotDlg, &mMultiShotPlace);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetMultiShotPlacement(bool update)
{
  if (mMultiShotDlg) {
    mMultiShotDlg->GetWindowPlacement(&mMultiShotPlace);
    if (update)
      mMultiShotDlg->UpdateAndUseMSparams(false);
  }
  return &mMultiShotPlace;
}

// Convenience function so others don't need to test
void CNavHelper::UpdateMultishotIfOpen(bool draw)
{
  if (mMultiShotDlg)
    mMultiShotDlg->UpdateAndUseMSparams(draw);
}

// Rotate either the regular or custom pattern in the given parameters by the given angle
int CNavHelper::RotateMultiShotVectors(MultiShotParams *params, float angle, bool custom)
{
  ScaleMat aMat, aProd, rotMat;
  int ind, numHoles = 2;
  float transISX, transISY;
  int magInd = custom ? params->customMagIndex : params->holeMagIndex;
  if (!magInd)
    return 1;

  // Need to transform IS to specimen, rotate, then specimen to IS
  aMat = mShiftManager->IStoSpecimen(magInd);
  if (!aMat.xpx)
    return 2;
  rotMat.xpx = rotMat.ypy = (float)cos(angle * DTOR);
  rotMat.ypx = (float)sin(angle * DTOR);
  rotMat.xpy = -rotMat.ypx;
  aProd = MatMul(MatMul(aMat, rotMat), MatInv(aMat));

  // Transform hole positions
  if (custom) {
    for (ind = 0; ind < (int)params->customHoleX.size(); ind++) {
      mWinApp->mShiftManager->ApplyScaleMatrix(aProd, params->customHoleX[ind],
        params->customHoleY[ind], transISX, transISY);
      params->customHoleX[ind] = transISX;
      params->customHoleY[ind] = transISY;
    }
  } else {
    for (ind = 0; ind < 2; ind++) {
      mWinApp->mShiftManager->ApplyScaleMatrix(aProd, (float)params->holeISXspacing[ind],
        (float)params->holeISYspacing[ind], transISX, transISY);
      params->holeISXspacing[ind] = transISX;
      params->holeISYspacing[ind] = transISY;
    }
  }
  return 0;
}

void CNavHelper::OpenHoleFinder(void)
{
  if (mHoleFinderDlg->IsOpen()) {
    mHoleFinderDlg->BringWindowToTop();
    return;
  }
  //mHoleFinderDlg = new CHoleFinderDlg();
  mHoleFinderDlg->Create(IDD_HOLE_FINDER);
  mWinApp->SetPlacementFixSize(mHoleFinderDlg, &mHoleFinderPlace);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT * CNavHelper::GetHoleFinderPlacement(void)
{
  if (mHoleFinderDlg->IsOpen()) {
    mHoleFinderDlg->GetWindowPlacement(&mHoleFinderPlace);
  }
  return &mHoleFinderPlace;
}

void CNavHelper::OpenMultiCombiner(void)
{
  if (mMultiCombinerDlg) {
    mMultiCombinerDlg->BringWindowToTop();
    return;
  }
  mMultiCombinerDlg = new CMultiCombinerDlg();
  mMultiCombinerDlg->Create(IDD_MULTI_COMBINER);
  mWinApp->SetPlacementFixSize(mMultiCombinerDlg, &mMultiCombinerPlace);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetMultiCombinerPlacement()
{
  if (mMultiCombinerDlg) {
    mMultiCombinerDlg->GetWindowPlacement(&mMultiCombinerPlace);
  }
  return &mMultiCombinerPlace;
}

WINDOWPLACEMENT *CNavHelper::GetAcquireDlgPlacement(bool fromDlg)
{
  if (fromDlg && mNav && mNav->mNavAcquireDlg) {
    mNav->mNavAcquireDlg->GetWindowPlacement(&mAcquireDlgPlace);
  }
  return &mAcquireDlgPlace;
}

// This is called on file changes to update the acquire dialog
void CNavHelper::UpdateAcquireDlgForFileChanges()
{
  if (mNav && mNav->mNavAcquireDlg) {
    mNav->EvaluateAcquiresForDlg(mNav->mNavAcquireDlg);
    mNav->mNavAcquireDlg->ManageOutputFile();
  }
}

// Loads a piece or synthesizes piece containing the given item
void CNavHelper::LoadPieceContainingPoint(CMapDrawItem *ptItem, int mapIndex)
{
  int cenX[2], cenY[2], xFrame, yFrame, xOverlap, yOverlap, err, yNframes, binning;
  int imageX, imageY, ix, iy;
  ScaleMat aMat;
  float distMin, delX, delY, stageX = 0., stageY = 0., montErrX, montErrY, tmpX, tmpY;
  float useShiftedRatio = 1.25;
  CMapDrawItem *map = mItemArray->GetAt(mapIndex);
  mWinApp->RestoreViewFocus();
  err = DistAndPiecesForRealign(map, ptItem->mStageX, ptItem->mStageY, 
    ptItem->mPieceDrawnOn, mapIndex, aMat, delX, delY, distMin, cenX[0], cenY[0], cenX[1],
    cenY[1], imageX, imageY, true, xFrame, yFrame, binning);
  if (err > 1 && mCurStoreInd < 0)
    delete mMapStore;
  if (err)
    return;
  cenX[1] = cenX[0];
  cenY[1] = cenY[0];
  xOverlap = mMapMontP->xOverlap;
  yOverlap = mMapMontP->yOverlap;
  yNframes = mMapMontP->yNframes;
  
  // Adjust coordinate relative to edge of frame then shift to using two frames
  // if it is possible and the distance to the edge in shifted frame is enough bigger 
  // than the distance in the real frame
  imageX -= cenX[0] * (xFrame - xOverlap);
  if (cenX[0] > 0 && (xFrame + xOverlap) / 2. - imageX > useShiftedRatio * imageX)
    cenX[0]--;
  else if (cenX[0] < mMapMontP->xNframes - 1 && 
    imageX - (xFrame - xOverlap) / 2. > useShiftedRatio * (xFrame - imageX))
    cenX[1]++;
  imageY -= cenY[0] * (yFrame - yOverlap);
  if (cenY[0] > 0 && (yFrame + yOverlap) / 2. - imageY > useShiftedRatio * imageY)
    cenY[0]--;
  else if (cenY[0] < yNframes - 1 && 
    imageY - (yFrame - yOverlap) / 2. > useShiftedRatio * (yFrame - imageY))
    cenY[1]++;

  // But make sure the added piece exists, or at least one of two added ones, and if
  // they don't, then revert the index back to the main piece
  if (cenX[0] < cenX[1] && mPieceSavedAt[cenY[0] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[0] * yNframes] < 0)
    cenX[0]++;
  if (cenX[0] < cenX[1] && mPieceSavedAt[cenY[0] + cenX[1] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[1] * yNframes] < 0)
    cenX[1]--;
  if (cenY[0] < cenY[1] && mPieceSavedAt[cenY[0] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[0] + cenX[1] * yNframes] < 0)
    cenY[0]++;
  if (cenY[0] < cenX[1] && mPieceSavedAt[cenY[1] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[1] * yNframes] < 0)
    cenY[1]--;

  // Set the center frames and read the montage
  mWinApp->mMontageController->SetFramesForCenterOnly(cenX[0], cenX[1], cenY[0], cenY[1]);
  if (mWinApp->mMontageController->ReadMontage(map->mMapSection, mMapMontP,
    mMapStore, true)) {
      if (mCurStoreInd < 0)
        delete mMapStore;
      return;
  }

  // Get the stage position of this piece by averaging all contributing pieces
  mPreviousErrX = mPreviousErrY = 0.;
  mUseMontStageError = false;
  for (ix = 0; ix < 2; ix++) {
    for (iy = 0; iy < 2; iy++) {
      StagePositionOfPiece(mMapMontP, aMat, delX, delY, cenX[ix], cenY[iy], tmpX, 
        tmpY, montErrX, montErrY);
      stageX += tmpX / 4.f;
      stageY += tmpY / 4.f;
    }
  }

  // Adjust buffer properties
  mImBufs->mStage2ImMat = aMat;
  mImBufs->mStage2ImDelX = mImBufs->mImage->getWidth() / 2.f - 
    aMat.xpx * stageX - aMat.xpy * stageY;
  mImBufs->mStage2ImDelY = mImBufs->mImage->getHeight() / 2.f - 
    aMat.ypx * stageX - aMat.ypy * stageY;
  mImBufs->mLoadWidth = mImBufs->mImage->getWidth();
  mImBufs->mLoadHeight = mImBufs->mImage->getHeight();
  mImBufs->mISX = mImBufs->mISY = 0.;
  mImBufs->mRotAngle = 0.;
  mImBufs->mInverted = false;
  mImBufs->mCamera = map->mMapCamera;
  mImBufs->mCaptured = BUFFER_MONTAGE_PIECE;

  // Rotate buffer just like map
  if (map->mRotOnLoad && !map->mImported)
    mNav->RotateMap(mImBufs, false);

  if (mCurStoreInd < 0)
    delete mMapStore;
  mWinApp->SetCurrentBuffer(0);
}

//////////////////////////////////////////////////////
// EXTERNAL ITEM PROCESSING
//////////////////////////////////////////////////////

// DO complete checks and conversions for the given item
int CNavHelper::ProcessExternalItem(CMapDrawItem *item, int extType)
{
  int ind, retval, drawnOn, adocSave;
  ScaleMat aMat;
  mMapMontP = &mMapMontParam;
  CMapDrawItem *mapItem;
  if (!item->mDrawnOnMapID)
    return NEXERR_NO_DRAWN_ON;
  mapItem = mNav->FindItemWithMapID(item->mDrawnOnMapID, true);
  if (!mapItem)
    return NEXERR_NO_MAP_WITH_ID;
  if (extType != NAVEXT_ON_MAP && !mapItem->mMapMontage)
    return NEXERR_MAP_NOT_MONT;
  if (item->mDrawnOnMapID != mExtDrawnOnID) {

    // If the ID does not match, first handle previous file and ID
    CleanupFromExternalFileAccess();

    // Then access a montage or just get parameters from single-frame map
    mCurStoreInd = 0;
    if (mapItem->mMapMontage) {
      adocSave = AdocGetCurrentIndex();
      if (mNav->AccessMapFile(mapItem, mMapStore, mCurStoreInd, mMapMontP, 
        mExtUseWidth, mExtUseHeight))
        return NEXERR_ACCESS_FILE;
      mExtDrawnOnID = item->mDrawnOnMapID;
      mExtXframe = mMapMontP->xFrame;
      mExtYframe = mMapMontP->yFrame;
      mExtXspacing = mExtXframe - mMapMontP->xOverlap;
      mExtYspacing = mExtYframe - mMapMontP->yOverlap;
      mExtLoadWidth = (float)(4 * ((mExtXframe + (mMapMontP->xNframes - 1) * mExtXspacing
        + 3) / 4));
      mExtLoadHeight = (float)(mExtYframe + (mMapMontP->yNframes - 1) * mExtYspacing);
      mExtTypeOfOffsets = -1;
      if (adocSave >= 0)
        AdocSetCurrent(adocSave);
    } else {
      mExtXframe = mapItem->mMapWidth;
      mExtYframe = mapItem->mMapHeight;
      mExtLoadWidth = mExtUseWidth = (float)mapItem->mMapWidth;
      mExtLoadHeight = mExtUseHeight = (float)mapItem->mMapHeight;
    }
    mExtDrawnOnID = item->mDrawnOnMapID;

    // Get transformation from image to stage coordinates
    aMat = mapItem->mMapScaleMat;
    aMat.xpx *= mExtLoadWidth / mExtUseWidth;
    aMat.xpy *= mExtLoadWidth / mExtUseWidth;
    aMat.ypx *= -mExtLoadHeight / mExtUseHeight;
    aMat.ypy *= -mExtLoadHeight / mExtUseHeight;
    mExtDelX = (mExtLoadWidth * mapItem->mMapWidth / mExtUseWidth) / 2.f - 
      (aMat.xpx * mapItem->mStageX + aMat.xpy * mapItem->mStageY);
    mExtDelY = (mExtLoadHeight * mapItem->mMapHeight / mExtUseHeight) / 2.f - 
      (aMat.ypx * mapItem->mStageX + aMat.ypy * mapItem->mStageY);
    mExtInv = MatInv(aMat);
  }

  // Transform the center stage position and all the points
  retval = TransformExternalCoords(item, extType, mapItem, item->mStageX, item->mStageY,
    item->mPieceDrawnOn);
  if (retval)
    return retval;
  for (ind = 0; ind < item->mNumPoints; ind++) {
    retval = TransformExternalCoords(item, extType, mapItem, item->mPtX[ind], 
      item->mPtY[ind], drawnOn);
    if (retval)
      return retval;
  }

  return 0;
}

// Convert the pixel coordinates in fx, fy to stage coordinates for the item of the given
// type on the given map; returns pieceDrawnOn for cases of aligned montages
int CNavHelper::TransformExternalCoords(CMapDrawItem *item, int extType, 
  CMapDrawItem *mapItem, float &fx, float &fy, int &pieceDrawnOn)
{
  int pcX, pcY, adocInd, adocSave, numPieces, xPiece, yPiece, ipc, nameInd, iz;
  int pcZ, retval, adjX, adjY, adjZ;
  float tempx, xInPc, yInPc;
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  char *keys[2] = {ADOC_ALI_COORD, ADOC_ALI_COORDVS};

  // Convert piece coordinate to coordinate in full image
  if (extType == NAVEXT_ON_PIECE) {
    if (item->mPieceDrawnOn < 0)
      return EXTERR_NO_PIECE_ON;
    xPiece = item->mPieceDrawnOn / mMapMontP->yNframes;
    yPiece = item->mPieceDrawnOn % mMapMontP->yNframes;
    fx += (float)(xPiece * mExtXspacing);
    fy += (float)(yPiece * mExtYspacing);

  } else if (extType == NAVEXT_ON_ALIMONT || extType == NAVEXT_ON_VSMONT) {

    numPieces = mMapMontP->xNframes * mMapMontP->yNframes;

    // Need new offsets if they don't match
    if (extType != mExtTypeOfOffsets) {

      // First make sure there's an mdoc
      adocInd = mMapStore->GetAdocIndex();
      if (adocInd < 0)
        return EXTERR_NO_MDOC;

      // Get pieceSavedAt of montage if not loaded yet
      if (mExtTypeOfOffsets < 0) {
        mWinApp->mMontageController->ListMontagePieces(mMapStore, mMapMontP, 
          mapItem->mMapSection, mPieceSavedAt);
      }

      // Get access to mdoc and get the offsets sized
      adocSave = AdocGetCurrentIndex();
      if (AdocSetCurrent(adocInd) < 0)
        return EXTERR_BAD_MDOC_IND;
      mExtTypeOfOffsets = 0;
      CLEAR_RESIZE(mExtOffsets.offsetX, short int, numPieces);
      CLEAR_RESIZE(mExtOffsets.offsetY, short int, numPieces);
      nameInd = mMapStore->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;

      // loop on pieces
      for (ipc = 0; ipc < numPieces; ipc++) {
        iz = mPieceSavedAt[ipc];
        if (iz < 0) {
          mExtOffsets.offsetX[ipc] = MINI_NO_PIECE;
          mExtOffsets.offsetY[ipc] = MINI_NO_PIECE;
          continue;
        }

        // Get piece coordinate then adjusted coordinate, difference is offset but
        // Y is inverted
        retval = 0;
        if (AdocGetThreeIntegers(names[nameInd], iz, ADOC_PCOORD, &pcX, &pcY, &pcZ))
          retval = EXTERR_NO_PC_COORD;

        if (!retval && AdocGetThreeIntegers(names[nameInd], iz, keys[extType - 1], 
          &adjX, &adjY, &adjZ))
          retval = EXTERR_NO_ALI_COORDS;
        if (retval) 
          break;
        mExtOffsets.offsetX[ipc] = (short)(adjX - pcX);
        mExtOffsets.offsetY[ipc] = (short)(pcY - adjY);
      }

      // Restore to nav autodoc
      if (adocSave >= 0)
        AdocSetCurrent(adocSave);
      if (retval)
        return retval;

      // Finish setting up the offsets
      mExtTypeOfOffsets = extType;
      mExtOffsets.subsetLoaded = false;
      mWinApp->mMontageController->SetMiniOffsetsParams(mExtOffsets, mMapMontP->xNframes,
        mExtXframe, mExtXspacing, mMapMontP->yNframes, mExtYframe, mExtYspacing);
    }

    // Adjust the Y-inverted position and get piece it is on
    fy = mExtLoadHeight - fy;
    mNav->OffsetMontImagePos(&mExtOffsets, 0, mMapMontP->xNframes - 1, 0, 
      mMapMontP->yNframes - 1, fx, fy, pieceDrawnOn, xInPc, yInPc);
    fy = mExtLoadHeight - fy;
  }

  // Transform full image coordinate to stage
  tempx = (float)((fx - mExtDelX) * mExtInv.xpx + (fy - mExtDelY) * mExtInv.xpy);
  fy = (float)((fx - mExtDelX) * mExtInv.ypx + (fy - mExtDelY) * mExtInv.ypy);
  fx = tempx;
  return 0;
}

// Remove an open store if necessary
void CNavHelper::CleanupFromExternalFileAccess()
{
  if (mExtDrawnOnID) {
    if (mCurStoreInd < 0)
      delete mMapStore;
    mExtDrawnOnID = 0;
  }
}

// Shift the subarea defined by the given coordinates by the offsets and keep it within
// legal bounds
bool CNavHelper::ModifySubareaForOffset(int camera, int xOffset, int yOffset, int &left,
  int &top, int &right, int &bottom)
{
  bool retval;
  CameraParameters *camP = &mCamParams[camera];
  int xFrame, yFrame, tleft, tright, ttop, tbottom;
  xFrame = right - left;
  yFrame = bottom - top;
  tleft = B3DMAX(0, (camP->sizeX - xFrame) / 2 + xOffset);
  tright = B3DMIN(camP->sizeX, tleft + xFrame);
  tleft = tright - xFrame;
  ttop = B3DMAX(0, (camP->sizeY - yFrame) / 2 + yOffset);
  tbottom = B3DMIN(camP->sizeY, ttop + yFrame);
  ttop = tbottom - yFrame;
  retval = (top != ttop || bottom != tbottom || left != tleft || right != tright);
  left = tleft;
  right = tright;
  top = ttop;
  bottom = tbottom;
  return retval;
}

// Get the focus position for the current item based on its state or the focus position
// stored in it, or the state or stored focus position for a previous acquire item of the
// type.  If there is no current or prior setting, or Nav is not open or there is no 
// current item, it returns the defined Low Dose focus position
void CNavHelper::FindFocusPosForCurrentItem(StateParams &state, bool justLDstate,
  int registration, int curInd)
{
  CMapDrawItem *item;
  StateParams *statePtr;
  int ind, firstInd;
  bool tilts;

  // initialize to current state
  state.camIndex = mWinApp->GetCurrentCamera();
  SaveLDFocusPosition(1, state.focusAxisPos, state.rotateAxis, state.axisRotation,
    state.focusXoffset, state.focusYoffset, false);

  if (!mNav || justLDstate)
    return;
  if (curInd < 0)
    curInd = mNav->GetCurrentIndex();

  if (curInd < 0 || curInd >= (int)mItemArray->GetSize())
    return;
  item = mItemArray->GetAt(curInd);
  tilts = item->mTSparamIndex >= 0;
  
  // Look back for last state if any
  for (ind = curInd; ind >= 0; ind--) {
    item = mItemArray->GetAt(ind);
    if (item->mRegistration == registration && (BOOL_EQUIV(item->mAcquire, !tilts) || 
      BOOL_EQUIV(item->mTSparamIndex >= 0, tilts))) {
      if (item->mFilePropIndex >= 0 && item->mStateIndex >= 0) {
        statePtr = mAcqStateArray->GetAt(item->mStateIndex);
        state = *statePtr;
        break;
      }
    }
  }

  // Now look back for item focus set
  firstInd = B3DMAX(0, ind);
  for (ind = curInd; ind >= firstInd; ind--) {
    item = mItemArray->GetAt(ind);
    if (item->mRegistration == registration && (BOOL_EQUIV(item->mAcquire, !tilts) ||
        BOOL_EQUIV(item->mTSparamIndex >= 0, tilts)) &&
      item->mFocusAxisPos > EXTRA_VALUE_TEST) {
      state.focusAxisPos = item->mFocusAxisPos;
      state.rotateAxis = item->mRotateFocusAxis;
      state.axisRotation = item->mFocusAxisAngle;
      state.focusXoffset = item->mFocusXoffset;
      state.focusYoffset = item->mFocusYoffset;
      break;
    }
  }
}

