// EMmontageController.cpp:  The montage routine
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
#include ".\EMmontageController.h"
#include "Utilities\XCorr.h"
#include "CameraController.h"
#include "EMbufferManager.h"
#include "Utilities\KGetOne.h"
#include "ShiftManager.h"
#include "MontageWindow.h"
#include "LogWindow.h"
#include "MultiGridTasks.h"
#include "MultiShotDlg.h"
#include "ParticleTasks.h"
#include "MacroProcessor.h"
#include "TSController.h"
#include "ProcessImage.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "FocusManager.h"
#include "ComplexTasks.h"
#include "Shared\b3dutil.h"
#include "Shared\autodoc.h"
#include "Image\KStoreADOC.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#pragma warning ( disable : 4244 )

static char *montMessage[5] = {
  "THIS IS GREAT!", "THIS IS GOOD", 
  "THIS IS NOT VERY GOOD - you might want to fix it in Midas",
  "THIS IS BAD - YOU SHOULD FIX IT IN MIDAS",
  "THIS IS TERRIBLE - YOU MAY NEED TO REDO IT" };

// The actions in exact sequence, to avoid the complexity of TSC
enum {SET_PANE = 0, REALIGN_ANCHOR, ALIGN_EXISTING, MOVE_TO_FOCUS_BLOCK, BLOCK_FOCUS, 
  MOVE_STAGE, CHECK_STAGE, ACQUIRE_ANCHOR, FOCUS_AT_PIECE, CHECK_DRIFT, ALIGN_WITH_IS,
  SET_IMAGE_SHIFT, QUEUE_NEXT, ACQUIRE_PIECE, CHECK_ACQUIRE, DWELL_TO_COOK, RUN_MACRO,
  SAVE_PIECE};
#define NO_PREVIOUS_ACTION   -1

// The realign actions: The SHOT processing must be right after the ACQUIRE
enum {
  EXIST_CHECKPOS_ACQUIRE, EXIST_ALIGN_SHOT, ISALIGN_ACQUIRE, ISALIGN_SHOT,
  ANCHOR_ACQUIRE, ANCHOR_SAVE_SHOT, BACKLASH_CHECK_ACQUIRE, BACKLASH_SHOT,
  ANCHALI_CHECK_ACQUIRE, ANCHALI_ALIGN_SHOT
};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

EMmontageController::EMmontageController()
{
  int i;
  SEMBuildTime(__DATE__, __TIME__);
  mMontaging = false;
  mReadingMontage = false;
  mPieceIndex = -1;
  mRestoringStage = 0;
  mFocusing = false;
  mShootingFilm = false;
  mLastFailed = false;
  mRequiredBWMean = -1.;
  mLowerPad = NULL;
  mUpperPad = NULL;
  mLowerCopy = NULL;
  mBinTemp = NULL;
  mMiniData = NULL;
  mCenterData = NULL;
  for (i = 0; i < 4; i++)
    mCenterImage[i] = NULL;
  mStageBacklash = -1.;   // Defer default initialization until scope is known
  mStopOnStageError = false;
  mMaxStageError = 0.;
  mCountLimit = 0;
  mShootFilmIfDark = false;
  mLimitMontToUsable = false;
  mDuplicateRetryLimit = 3;
  mNumOverwritePieces = 0;
  mAlignToNearestPiece = false;
  mISrealignInterval = 1;
  mUseTrialSetInISrealign = false;
  mDriftRepeatLimit = 5;
  mDriftRepeatDelay = 2.;
  mNumActions = SAVE_PIECE + 1;
  mAutosaveLog = false;
  mDefinedCenterFrames = false;
  mMinOverlapForZigzag = 15.;
  mSloppyRadius1[0] = -0.01f;
  mSloppyRadius1[1] = 0.f;
  mRadius2[0] = mRadius2[1] = 0.35f;
  mSigma1[0] = mSigma1[1] = 0.05f;
  mSigma2[0] = mSigma2[1] = 0.05f;
  mGridMapMinSizeMm = 0.65f;
  mGridMapCutoffInvUm = 0.25f;
  mDivFilterSet1By2 = false;
  mDivFilterSet2By2 = false;
  mUseFilterSet2 = false;
  mUseSet2InLD = false;
  mRedoCorrOnRead = false;
  mMultiShotParams = NULL;
  mXCorrThread = NULL;
  mMacroToRun = 0;
  mRunningMacro = false;
  mAllowHQMontInLD = false;
  mNoMontXCorrThread = false;
  mNoDrawOnRead = false;
  mNextPctlPatchSize = 0;
  mLastSavedAtZ = -1;
}

EMmontageController::~EMmontageController()
{
}

// Initialize the module after all pointers are known
void EMmontageController::Initialize()
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSets = mWinApp->GetConSets();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mImBufs = mWinApp->GetImBufs();
  mBufferManager = mWinApp->mBufferManager;
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mParam = mWinApp->GetMontParam();
  mShiftManager = mWinApp->mShiftManager;
  mTSController = mWinApp->mTSController;
  if (mStageBacklash < 0.) {
    if (JEOLscope)
      mStageBacklash = 10.;
    else if (HitachiScope)
      mStageBacklash = 20.;
    else
      mStageBacklash = 5.;
  }
}

/////////////////////////////////////////////////
//  SETUP ROUTINES
/////////////////////////////////////////////////

// Set up many arrays when opening montaging or changing very sloppy flag
void EMmontageController::SetupOverlapBoxes()
{
  float aspectMax = 2.0f;    //Aspect ratio for overlap boxes
  float padFrac = 0.45f;    // padding fraction
  float radius1 = 0.0;
  float sigma1, radius2, sigma2, ubRadius2;
  float extraWidth = 0.0;
  int indentXC = 2;
  int numSmooth = 6;
  int maxBinning = 3;
  int targetXcorrSize = 1024;
  int i, ix, indentUse;
  int yInv = mParam->yFrame - 1;
  int filtIndex = 0;
  float divFilter = 1.;

  mNumPieces = mParam->xNframes * mParam->yNframes;

  // Get the filter index and division factor
  if (mUseFilterSet2 || (mWinApp->LowDoseMode() && mUseSet2InLD))
    filtIndex = 1;
  if ((!filtIndex && mDivFilterSet1By2) || (filtIndex && mDivFilterSet2By2))
    divFilter = 2.;
  sigma1 = mSigma1[filtIndex] / divFilter;
  sigma2 = mSigma2[filtIndex] / divFilter;
  radius2 = mRadius2[filtIndex] / divFilter;

  // If moving stage, switch to Blendmont's VerySloppyMontage params
  if (mVerySloppy) {
    aspectMax = 5.0f;    // WAS 5
    radius1 = mSloppyRadius1[filtIndex] / divFilter;
    extraWidth = 0.25f;   //0.25f;
  }

  ubRadius2 = TreatAsGridMap();
  mTreatingAsGridMap = ubRadius2 > 0.;
  if (mTreatingAsGridMap) {
    sigma1 = 0.01f;
    radius1 = 0.;
  }

  CLEAR_RESIZE(mUpperShiftX, float, 2 * mNumPieces);
  CLEAR_RESIZE(mUpperShiftY, float, 2 * mNumPieces);
  CLEAR_RESIZE(mUpperFirstX, float, 2 * mNumPieces);
  CLEAR_RESIZE(mUpperFirstY, float, 2 * mNumPieces);
  CLEAR_RESIZE(mPatchCCC, float, 2 * mNumPieces);
  CLEAR_RESIZE(mMaxSDToPieceNum, int, 2 * mNumPieces);
  CLEAR_RESIZE(mMaxSDToIxyOfEdge, int, 2 * mNumPieces);
  CLEAR_RESIZE(mAlternShifts, float, 8 * mNumPieces);
  CLEAR_RESIZE(mBmat, float, 2 * mNumPieces);
  CLEAR_RESIZE(mLowerPatch, float *, 2 * mNumPieces);
  CLEAR_RESIZE(mUpperPatch, float *, 2 * mNumPieces);
  for (i = 0; i < 2 * mNumPieces; i++)
    mLowerPatch[i] = mUpperPatch[i] = NULL;

  // Fix all the vector sizes; mPieceSavedAt is taken care of in ListMontagePieces
  CLEAR_RESIZE(mMontageX, int, mNumPieces+1);
  CLEAR_RESIZE(mMontageY, int, mNumPieces+1);
  CLEAR_RESIZE(mMiniOffsets.offsetX, short int, mNumPieces);
  CLEAR_RESIZE(mMiniOffsets.offsetY, short int, mNumPieces);
  CLEAR_RESIZE(mIndSequence, int, mNumPieces);
  CLEAR_RESIZE(mSkipIndex, int, mNumPieces);
  CLEAR_RESIZE(mPieceToVar, int, mNumPieces);
  CLEAR_RESIZE(mVarToPiece, int, mNumPieces);
  CLEAR_RESIZE(mActualErrorX, float, mNumPieces);
  CLEAR_RESIZE(mActualErrorY, float, mNumPieces);
  CLEAR_RESIZE(mFocusBlockInd, int, mNumPieces);
  CLEAR_RESIZE(mAcquiredStageX, float, mNumPieces);
  CLEAR_RESIZE(mAcquiredStageY, float, mNumPieces);
  CLEAR_RESIZE(mColumnStartY, int, mParam->xNframes);

  // Compute the areas to extract for overlap checking
  mXYpieceSize[0] = mParam->xFrame;
  mXYpieceSize[1] = mParam->yFrame;
  mXYoverlap[0] = mParam->xOverlap;
  mXYoverlap[1] = mParam->yOverlap;
  mCTFp[0] = mCTFx;
  mCTFp[1] = mCTFy;

  mXCorrBinning = montXCFindBinning(maxBinning, targetXcorrSize, indentXC, mXYpieceSize,
    mXYoverlap, aspectMax, extraWidth, padFrac, 19, &mPadPixels, &mBoxPixels[0]);
  if (mTreatingAsGridMap) {
    radius2 = ubRadius2 * mXCorrBinning;
    sigma2 = radius2 / 8.f;
  }
  for (ix = 0; ix < 2; ix++) {
    montXCBasicSizes(ix, mXCorrBinning, indentXC, mXYpieceSize, mXYoverlap, aspectMax, 
      extraWidth, padFrac, 19, &indentUse, &mXYbox[ix][0], &mNumExtra[ix][0], &mXpad[ix], 
      &mYpad[ix], &mMaxLongShift[ix]);

    montXCIndsAndCTF(ix, mXYpieceSize, mXYoverlap, &mXYbox[ix][0], mXCorrBinning, 
      indentUse, &mNumExtra[ix][0], mXpad[ix], mYpad[ix], numSmooth, 
      sigma1, sigma2, radius1, radius2, mVerySloppy ? 1 : 0, &mLowerInd0[ix][0],
      &mLowerInd1[ix][0], &mUpperInd0[ix][ix], &mUpperInd1[ix][ix], &mXsmooth[ix], 
      &mYsmooth[ix], mCTFp[ix], &mDelta[ix]);

    // Copy long direction limits from lower to upper piece and then invert Y, swap limits
    mUpperInd0[ix][1 - ix] = mLowerInd0[ix][1 - ix];
    mUpperInd1[ix][1 - ix] = mLowerInd1[ix][1 - ix];
    i = yInv - mLowerInd0[ix][1];
    mLowerInd0[ix][1] = yInv - mLowerInd1[ix][1];
    mLowerInd1[ix][1] = i;
    i = yInv - mUpperInd0[ix][1];
    mUpperInd0[ix][1] = yInv - mUpperInd1[ix][1];
    mUpperInd1[ix][1] = i;
    mBoxPixels[ix] = mXYbox[ix][0] * mXYbox[ix][1];
  }
  mNeedBoxSetup = false;
}

// Determines whether current parameters qualify montage to be treated as grid map and
// returns the cutoff in unbinned 1/pixel if so, otherwise 0
float EMmontageController::TreatAsGridMap()
{
  int nx = (mParam->xNframes - 1) * (mParam->xFrame - mParam->xOverlap) + mParam->xFrame;
  int ny = (mParam->yNframes - 1) * (mParam->yFrame - mParam->yOverlap) + mParam->yFrame;
  int *activeList = mWinApp->GetActiveCameraList();
  float pixel = mShiftManager->GetPixelSize(activeList[mParam->cameraIndex], 
    mParam->magIndex) * mParam->binning;
  if (mGridMapMinSizeMm <= 0. || 
    sqrt((double)nx * ny) * pixel / 1000. < mGridMapMinSizeMm)
    return 0.;
  return pixel * mGridMapCutoffInvUm;
}

// Turn montaging on or off
void EMmontageController::SetMontaging(BOOL inVal)
{
  int miniTarget = 2048;    // Target size for miniview
  DialogTable *dlgTable = mWinApp->GetDialogTable();
  int dlgState = dlgTable[MONTAGE_DIALOG_INDEX].state;
  int *activeList = mWinApp->GetActiveCameraList();
  CString menuText = "Overwrite Pieces...";

  mParam = mWinApp->GetMontParam();
  int iCam = activeList[mParam->cameraIndex];
  if (mParam->useViewInLowDose)
    mParam->useSearchInLowDose = false;

  if (inVal) {
    mVerySloppy = mParam->verySloppy;
    mUseExpectedShifts = mParam->evalMultiplePeaks;
    SetupOverlapBoxes();
    if (!mMagTab[mParam->magIndex].calibrated[iCam] && 
      mParam->magIndex >= mScope->GetLowestNonLMmag() &&
      (!mParam->moveStage  || CalNeededForISrealign(mParam)) &&
      !mParam->warnedCalOpen)
      mWinApp->AppendToLog("WARNING: The image shifts "
      "for the selected magnification have not been\r\n   calibrated in the last day."
        "  You may want to do this calibration (see Calibration\r\n menu) "
        "if image shift calibration is not very stable on your scope");
    mParam->warnedCalOpen = true;
    dlgState |= 1;  // Open the window

    // Get default (and maximum) binning for trial montages
    mParam->prescanBinning = SetMaxPrescanBinning();

    // Figure out default (and maximum) zoom/bin for overview
    int zoomX = (mParam->xFrame + (mParam->xNframes - 1)* (mParam->xFrame -
        mParam->xOverlap) + miniTarget - 1) / miniTarget;
    int zoomY = (mParam->yFrame + (mParam->yNframes - 1) * (mParam->yFrame -
        mParam->yOverlap) + miniTarget - 1) / miniTarget;
    zoomX = B3DMAX(zoomX, 1);
    mParam->maxOverviewBin = B3DMAX(zoomX, zoomY);
    if (!mParam->overviewBinning)
      mParam->overviewBinning = mParam->maxOverviewBin;
    B3DCLAMP(mParam->overviewBinning, 1, mParam->maxOverviewBin);
    mWinApp->mMontageWindow.UpdateSettings();

  } else if (mMontaging) {
    // Do this only if montaging was on
    CleanPatches();

    if (!mWinApp->mNavHelper->AnyMontageMapsInNavTable())
      dlgState &= ~1; // Close the window
    menuText = "Overwrite";
  }
  mMontaging = inVal;
  mWinApp->mMontageWindow.SetOpenClosed(dlgState);
  mWinApp->DialogChangedState((CToolDlg *)(&mWinApp->mMontageWindow), dlgState);
  mWinApp->UpdateBufferWindows();
  UtilModifyMenuItem("File", ID_FILE_OVERWRITE, (LPCTSTR)menuText);
}

///////////////////////////////////
// START DOING OR READING A MONTAGE
///////////////////////////////////

// Recompose overview by doing fake montage: get a legal section # and proceed
int EMmontageController::ReadMontage(int inSect, MontParam *inParam, 
                                          KImageStore *inStoreMRC, BOOL centerOnly,
                                          BOOL synchronous, int bufToCopyTo)
{
  int *activeList = mWinApp->GetActiveCameraList();
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  int adocInd, nameInd, camNum, ind;

  if (mRunningMacro) {
    SEMMessageBox("You cannot read in a montage while a montage is running a script");
    return 1;
  }
  if (inParam) {
    mParam = inParam;

    // Default to current settings and mark binning as needed
    mParam->cameraIndex = mWinApp->GetCurrentActiveCamera();
    mParam->magIndex = mScope->FastMagIndex();
    mParam->binning = 0;

    // But try to replace/set from adoc
    adocInd = inStoreMRC->GetAdocIndex();
    if (adocInd >= 0) {
      if (!AdocGetMutexSetCurrent(adocInd)) {
        nameInd = inStoreMRC->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;

        // Replace mag index and set binning if they are there
        // If camera number is there, look it up in active list and assign it
        AdocGetInteger(names[nameInd], 0, ADOC_MAGIND, &mParam->magIndex);
        AdocGetInteger(names[nameInd], 0, ADOC_BINNING, &mParam->binning);
        if (!AdocGetInteger(names[nameInd], 0, ADOC_CAMERA, &camNum)) {
          for (ind = 0; ind < mWinApp->GetNumActiveCameras(); ind++) {
            if (camNum == activeList[ind]) {
              mParam->cameraIndex = ind;
              break;
            }
          }
        }
        AdocReleaseMutex();
      }
    }

    // Obey the verysloppy and useExpected settings in the parameter set
    mVerySloppy = mParam->verySloppy;
    mUseExpectedShifts = mParam->evalMultiplePeaks;

    // Set binning based on full field of camera if it didn't come from adoc
    if (!mParam->binning) {
      CameraParameters *camParam = mWinApp->GetCamParams() +
        activeList[mParam->cameraIndex];
      int binX = camParam->sizeX / inStoreMRC->getWidth();
      int binY = camParam->sizeY / inStoreMRC->getHeight();
      mParam->binning = binX < binY ? binX : binY;
      if (!mParam->binning)
        mParam->binning = 1;
    }

    SetupOverlapBoxes();

    mReadStoreMRC = inStoreMRC;
    mReadResetBoxes = mMontaging;
  } else {
    mParam = mWinApp->GetMontParam();
    mReadStoreMRC = mWinApp->mStoreMRC;
  }

  mReadSavedZ = mParam->zCurrent;
  int newZ = mReadSavedZ > 0 ? mReadSavedZ - 1 : 0;
  if (inSect < 0) {
    if (!KGetOneInt("Section number to read: ", newZ))
      return 1;
  } else
    newZ = inSect;
  if (newZ < 0 || newZ > mParam->zMax) {
    SEMMessageBox( "Illegal section number.");
    return 1;
  }
  mExpectingFloats = mReadStoreMRC->getMode() == MRC_MODE_FLOAT;
  mParam->zCurrent = newZ;
  mCenterOnly = centerOnly;
  mSynchronous = synchronous || centerOnly;
  mBufToCopyTo = (bufToCopyTo >= 0 && bufToCopyTo < MAX_BUFFERS) ? 
    bufToCopyTo : mBufferManager->GetBufToReadInto();

  StartMontage(MONT_NOT_TRIAL, true);
  return 0;
}

// START ACQUIRING OR READING IN A MONTAGE
int EMmontageController::StartMontage(int inTrial, BOOL inReadMont, float cookDwellTime,
  int cookSkip, bool skipColumn, float cookSpeed)
{
  int ind, icx1, icx2, icy1, icy2, first, last, j, iDir, notSkipping;
  int ix, iy, i, fullX, fullY, binning, numBlocksX, numBlocksY, frameDelX, frameDelY;
  int left, right, top, bottom, firstUndone, lastDone, firstPiece, area;
  int magInd, lowestM, blockSizeInY;
  double delISX, delISY, baseZ, needed, currentUsage, ISX, ISY, dist, minDist;
  float memoryLimit, stageX, stageY, cornX, cornY, binDiv, xTiltFac, yTiltFac, pixelSize;
  float acqExposure, trialExposure, minContExp, polyRealignErrX, polyRealignErrY;
  BOOL tryForMemory, focusFeasible, external, useHQ, alignable, useVorSinLD;
  int already, borderTry, setNum, useSetNum;
  ScaleMat bMat, aInv;
  bool definedCenter = mDefinedCenterFrames;
  bool preCooking = inTrial == MONT_TRIAL_PRECOOK;
  IntVec p2vOrig;
  CString statText = "MONTAGING";
  CString statPiece, mess;
  CMapDrawItem *navItem;
  KImageStore *storeMRC;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int *activeList = mWinApp->GetActiveCameraList();
  CameraParameters *cam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  mTrialMontage = inTrial;
  mReadingMontage = inReadMont;
  mUsingMultishot = !mReadingMontage && UseMultishotForMontage(mParam);
  if (mUsingMultishot && inTrial == MONT_TRIAL_IMAGE) {
    SEMMessageBox("You cannot do a trial montage when using Multiple Records");
    return 1;
  }
  if (mRunningMacro) {
    SEMMessageBox("You cannot start a montage while a montage is running a script");
    return 1;
  }

  BlockSizesForNearSquare(mParam->xFrame, mParam->yFrame, mParam->xOverlap,
    mParam->yOverlap, mParam->focusBlockSize, mBlockSizeInX, blockSizeInY);
  mDoStageMoves = mParam->moveStage && !mUsingMultishot;

  // Need to figure out image shift blocks early in case it is cancelling the stage move
  if (mDoStageMoves && mParam->imShiftInBlocks && !mReadingMontage) {
    magInd = mParam->magIndex;
    if (mWinApp->LowDoseMode()) {
      setNum = MontageConSetNum(mParam, true);
      area = mCamera->ConSetToLDArea(setNum);
      magInd = ldp[area].magIndex;
    }
    cam = mWinApp->GetCamParams() + activeList[mParam->cameraIndex];
    lowestM = mScope->GetLowestNonLMmag(cam);
    pixelSize = mShiftManager->GetPixelSize(activeList[mParam->cameraIndex], magInd) *
      (float)mParam->binning;
    ImageShiftBlockSizes(mParam->xFrame, mParam->yFrame, mParam->xOverlap,
      mParam->yOverlap, pixelSize,
      magInd < lowestM ? mParam->maxBlockImShiftLM : mParam->maxBlockImShiftNonLM,
      mBlockSizeInX, blockSizeInY);
    if (mBlockSizeInX * blockSizeInY > 1 &&
      mBlockSizeInX >= mParam->xNframes && blockSizeInY >= mParam->yNframes) {
      mWinApp->AppendToLog("Doing montage with image shift because the montage is not\r\n"
        "  larger than the block size for image shifting");
      mDoStageMoves = false;
    } else if (mBlockSizeInX * blockSizeInY == 1)
      SEMTrace('1', "No image shift will be used");
    else
      SEMTrace('1', "Image shifting will be done in %d x %d blocks", mBlockSizeInX, 
        blockSizeInY);
  }
  mImShiftInBlocks = mDoStageMoves && mParam->imShiftInBlocks &&
    mBlockSizeInX * blockSizeInY > 1 && !preCooking && !mReadingMontage;
  useHQ = mDoStageMoves && mParam->useHqParams && (!mWinApp->LowDoseMode() || 
    mAllowHQMontInLD);
  mUsingImageShift = !mDoStageMoves || mImShiftInBlocks;
  mDefinedCenterFrames = false;
  mAddMiniOffsetToCenter = definedCenter;
  mMiniOffsets.subsetLoaded = definedCenter;
  mDefocusForCal = 0.;
  mPctlPatchSize = mNextPctlPatchSize;
  mNextPctlPatchSize = 0;

  mDwellTimeMsec = (int)(1000. * B3DMAX(0., cookDwellTime));
  if (cookDwellTime < 0)
    cookSkip = B3DNINT(-cookDwellTime);
  if (cookSkip == 1)
    cookSkip = 10 * mParam->yNframes;
  mMoveInfo.speed = cookSpeed;
  if (!mReadingMontage) {
    mParam = mWinApp->GetMontParam();
    mReadStoreMRC = mWinApp->mStoreMRC;
    if (mBufferManager->CheckAsyncSaving())
      return 1;

    if (!preCooking && mMacroToRun > 0 && mMacroToRun <= MAX_MACROS) {
      if (mWinApp->mMacroProcessor->DoingMacro()) {
        SEMMessageBox("You cannot run a script from a montage run by a script");
        return 1;
      }
      if (mWinApp->mMacroProcessor->EnsureMacroRunnable(mMacroToRun - 1)) {
        mess.Format("Script # %d is not runnable", mMacroToRun);
        SEMMessageBox(mess);
        return 1;
      }
    }

    if (preCooking && !mDoStageMoves) {
      SEMMessageBox("You cannot use montage pre-cooking with an image shift montage");
      return 1;
    }
    if (preCooking && mWinApp->LowDoseMode()) {
      SEMMessageBox("You cannot use montage pre-cooking in Low Dose mode");
      return 1;
    }

    if ((mWinApp->LowDoseMode() ? 1 : 0) != (mParam->setupInLowDose ? 1 : 0)) {
      useVorSinLD = mParam->useViewInLowDose ||
        (mParam->useSearchInLowDose && ldp[SEARCH_AREA].magIndex);
      if ((mParam->setupInLowDose || mParam->magIndex == ldp[RECORD_CONSET].magIndex) &&
        !useVorSinLD) {

        // If it was set up in low dose and no data taken yet, assign current low dose
        // R mag to the montage; in any event change the flag
        if (mParam->setupInLowDose && mReadStoreMRC->getDepth() == 0)
          mParam->magIndex = ldp[RECORD_CONSET].magIndex;
        mParam->setupInLowDose = mWinApp->LowDoseMode();
      } else {

        // Change is never allowed here if View in LD is on, but can be achieved in 
        // dialog if no data yet
        mess.Format("This montage file was set up in %s mode and cannot be used\n"
          "in %s mode", mParam->setupInLowDose ? "Low Dose" : "regular",
          mParam->setupInLowDose ? "regular" : "Low Dose");
        if (mReadStoreMRC->getDepth() > 0 && useVorSinLD)
          mess += "\n\nYou must either change mode or set up a new montage";
        else
          mess += " without going through the Montage Setup dialog again";
        SEMMessageBox(mess);
        return 1;
      }
    }

    // Switch to defined camera if necessary then get parameters for it
    if (!mWinApp->LowDoseMode() &&
      mWinApp->GetCurrentActiveCamera() != mParam->cameraIndex)
      mWinApp->SetActiveCameraNumber(mParam->cameraIndex);
    cam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
    if (mReadStoreMRC->getDepth() > 0 && CameraNotFeasible()) {
      SEMMessageBox("A montage cannot be saved into this file with the current camera");
      return 1;
    }
    mUseContinuousMode = mParam->useContinuousMode && cam->useContinuousMode > 0;

    if (!mReadingMontage) {
      mExpectingFloats = mCamera->ReturningFloatImages(cam) != 0;
      if (mReadStoreMRC->getDepth() &&
        !BOOL_EQUIV(mExpectingFloats, mReadStoreMRC->getMode() == MRC_MODE_FLOAT)) {
        mess.Format("You cannot take this montage with %sfloat images, the output file is"
          " a %sfloat file", mExpectingFloats ? "" : "non-", 
          mExpectingFloats ? "non-" : "");
        SEMMessageBox(mess);
        return 1;
      }
    }

    // Set mag index now from current low dose params (note this allows a change!)
    setNum = MontageConSetNum(mParam, true);
    useSetNum = MontageConSetNum(mParam, false);
    mWinApp->CopyOptionalSetIfNeeded(setNum);
    if (mWinApp->LowDoseMode()) {
      area = mCamera->ConSetToLDArea(setNum);
      mParam->magIndex = ldp[area].magIndex;
      if (IS_SET_VIEW_OR_SEARCH(setNum) &&
        mParam->magIndex >= mScope->GetLowestNonLMmag())
        mDefocusForCal = mScope->GetLDViewDefocus(area);
    } else if (mConSets[useSetNum].binning != mParam->binning &&
      !mParam->warnedConSetBin) {
      binDiv = BinDivisorF(cam);
      mess.Format("This montage is set to be taken with binning %g,\n"
        "different from the binning (%g) in the %s parameter\n"
        "set.  The exposure time (%g sec) may not be right for\n"
        "binning %g\n\nAre you sure you want to proceed?",
        mParam->binning / binDiv, mConSets[useSetNum].binning / binDiv,
        (LPCTSTR)(*(mWinApp->GetModeNames() + useSetNum)),
        mConSets[useSetNum].exposure, mParam->binning / binDiv);
      if (AfxMessageBox(mess, MB_QUESTION) == IDNO)
        return 1;
      mParam->warnedConSetBin = true;
    }

    /* if (!mMagTab[mParam->magIndex].calibrated[mWinApp->GetCurrentCamera()] &&
      mParam->magIndex >= mScope->GetLowestNonLMmag() &&
      !mParam->warnedCalAcquire && !mTrialMontage &&
      (!mDoStageMoves || CalNeededForISrealign(mParam))) {
      if (SEMMessageBox(
        "You have not calibrated the image shifts for the selected magnification.\n\n"
        "You should do so (see Calibration menu) unless you are just doing "
        "map or test montages.\n\nDo you want to proceed anyway?",
        MB_YESNO | MB_ICONQUESTION, false, IDYES) == IDNO)
        return 1;
      mParam->warnedCalAcquire = true;
    } */

    if (mDoStageMoves && mTrialMontage == MONT_TRIAL_IMAGE) {
      if (AfxMessageBox("This is a stage montage but you started a Prescan.\n\n"
        "Are you sure you want to do a Prescan?",
        MB_YESNO | MB_ICONQUESTION) == IDNO)
        return 1;
    }

    // 4/2/09: Wait for stage ready (can be moved by navigator for starting full 
    // montage) - but not too long since UI is dead.
    if (mDoStageMoves && mScope->WaitForStageReady(5000)) {
      SEMMessageBox("The stage is busy or not ready; montage aborted");
      return 1;
    }

    // Now adjust the mag if it is not at the specified setting
    if (!mWinApp->LowDoseMode()) {
      if (mScope->GetMagIndex() != mParam->magIndex)
        mScope->SetMagIndex(mParam->magIndex);

      // Otherwise, normalize if first time or mag has changed
      else if (!mTrialMontage &&
        (!mParam->settingsUsed || mScope->GetMagChanged())) {
        mScope->NormalizeProjector();
      }

      // If in low mag, go to a standard focus if one is defined
      mScope->SetFocusToStandardIfLM(mParam->magIndex);
    }
  }

  if (inTrial == MONT_TRIAL_IMAGE) {
    // If this is a trial, get parameters into special set, get unbinned full size
    mTrialParam = *mParam;

    // Adjust the parameters to the new binning, making sure they match what the
    // camera will do; make sure maximum and current prescan are OK for current camera
    SetMaxPrescanBinning();
    binning = SetPrescanBinning(mParam->prescanBinning, -1);
    fullX = mParam->xFrame * mParam->binning;
    if (cam->sizeX - fullX < mParam->binning * B3DMAX(1, cam->moduloX))
      fullX = cam->sizeX;
    mTrialParam.xFrame = fullX / binning;
    fullY = mParam->yFrame * mParam->binning;
    if (cam->sizeY - fullY < mParam->binning * B3DMAX(1, cam->moduloY))
      fullY = cam->sizeY;
    mTrialParam.yFrame = fullY / binning;
    mWinApp->mCamera->CenteredSizes(mTrialParam.xFrame, cam->sizeX, cam->moduloX,
      left, right, mTrialParam.yFrame, cam->sizeY, cam->moduloY, top, bottom, binning);

    mTrialParam.xOverlap = (mParam->binning * mParam->xOverlap) / binning;
    mTrialParam.xOverlap += mTrialParam.xOverlap % 2;
    mTrialParam.yOverlap = (mParam->binning * mParam->yOverlap) / binning;
    mTrialParam.yOverlap += mTrialParam.yOverlap % 2;

    mTrialParam.binning = binning;

    // point to the special set
    mParam = &mTrialParam;
    statText = "GETTING TRIAL MONTAGE";
  } else {

    // Need very sloppy parameters if it is set as an option
    // If state not already set, set up boxes again
    if (mNeedBoxSetup || ((!BOOL_EQUIV(mParam->verySloppy, mVerySloppy) ||
      !BOOL_EQUIV(mTreatingAsGridMap, TreatAsGridMap() > 0.)) && !mUsingMultishot)) {
      mVerySloppy = mParam->verySloppy;
      SetupOverlapBoxes();
    }
  }

  // Set up to use expected shifts and set number of Xcorr peaks
  mUseExpectedShifts = mParam->evalMultiplePeaks || mTreatingAsGridMap;
  mNumXcorrPeaks = (mVerySloppy || mUseExpectedShifts) ? 16 : 1;
  if (mTreatingAsGridMap)
    mNumXcorrPeaks = mVerySloppy ? 100 : 50;
  mTrimmedMaxSDs.clear();

  // Fill arrays with the piece coordinates
  frameDelX = mParam->xFrame - mParam->xOverlap;
  frameDelY = mParam->yFrame - mParam->yOverlap;
  for (ix = 0; ix < mParam->xNframes; ix++)
    for (iy = 0; iy < mParam->yNframes; iy++) {
      ind = ix *mParam->yNframes + iy;
      mMontageX[ind] = ix * frameDelX;
      mMontageY[ind] = iy * frameDelY;
      mMiniOffsets.offsetX[ind] = MINI_NO_PIECE;
      mMiniOffsets.offsetY[ind] = MINI_NO_PIECE;
    }

  // Compute starting coordinates of current center virtual piece
  mMontCenterX = mMontageX[mNumPieces - 1] / 2;
  mMontCenterY = mMontageY[mNumPieces - 1] / 2;

  // Set extra piece position to go to center at end
  mMontageX[mNumPieces] = mMontCenterX;
  mMontageY[mNumPieces] = mMontCenterY;

  // Set the center frames if not preset, or revise center if preset
  if (definedCenter && mCenterOnly) {
    mMontCenterX = ((mCenterIndX1 + mCenterIndX2) * frameDelX) / 2;
    mMontCenterY = ((mCenterIndY1 + mCenterIndY2) * frameDelY) / 2;
  } else {
    mCenterIndX1 = (mParam->xNframes - 1) / 2;
    mCenterIndX2 = mParam->xNframes / 2;
    mCenterIndY1 = (mParam->yNframes - 1) / 2;
    mCenterIndY2 = mParam->yNframes / 2;
  }

  // Get starting location
  mScope->GetStagePosition(mBaseStageX, mBaseStageY, baseZ);
  if (mDoStageMoves && mParam->forFullMontage) {
    mBaseStageX = mParam->fullMontStageX;
    mBaseStageY = mParam->fullMontStageY;
  }
  if (mWinApp->LowDoseMode() && !mReadingMontage)
    mScope->GotoLowDoseArea(mCamera->ConSetToLDArea(setNum));
  mScope->GetImageShift(mBaseISX, mBaseISY);
  SEMTrace('S', "Montage base stage %.3f %.3f  IS %.3f %.3f", mBaseStageX, mBaseStageY,
    mBaseISX, mBaseISY);

  // Set up skip list to read in only the center
  mNumToSkip = 0;
  if (mReadingMontage && mCenterOnly) {
    for (ix = 0; ix < mParam->xNframes; ix++)
      for (iy = 0; iy < mParam->yNframes; iy++)
        if (ix < mCenterIndX1 || ix > mCenterIndX2 ||
          iy < mCenterIndY1 || iy > mCenterIndY2)
          mSkipIndex[mNumToSkip++] = ix *mParam->yNframes + iy;
  }

  mAdjustmentScale = 0.;
  if (!mReadingMontage) {

    // Initialize the skip list unless it is being ignored
    mNumToSkip = (mUsingMultishot || mParam->ignoreSkipList) ? 0 : mParam->numToSkip;
    for (ix = 0; ix < mNumToSkip; ix++)
      mSkipIndex[ix] = mParam->skipPieceX[ix] * mParam->yNframes +
      mParam->skipPieceY[ix];

    // Get transformation matrix
    ix = mScope->FastSpotSize();
    iy = mScope->GetProbeMode();
    delISX = mScope->FastIntensity();
    if (mDoStageMoves) {

      // Stage move case - adjust Y by cosine of tilt angle and adjust for defocus
      bMat = mShiftManager->FocusAdjustedStageToCamera(mWinApp->GetCurrentCamera(),
        mParam->magIndex, ix, iy, delISX, mDefocusForCal);
      if (bMat.xpx == 0.) {
        SEMMessageBox("There is no measured or derived"
          " stage shift \ncalibration available at this magnification.\n\n"
          "You must calibrate stage shift to take this montage");
        return 1;
      }
      if (!mShiftManager->GetDefocusMagAndRot(ix, iy, delISX, mDefocusForCal, 
        mAdjustmentScale, mAdjustmentRotation))
        mAdjustmentScale = 0.;

      mBinv = MatInv(bMat);
      mShiftManager->GetStageTiltFactors(xTiltFac, yTiltFac);
      mBinv.xpx /= xTiltFac;
      mBinv.xpy /= xTiltFac;
      mBinv.ypx /= yTiltFac;
      mBinv.ypy /= yTiltFac;

    }

    if (mUsingImageShift) {

      // Normal Image shift case, or image shift in blocks
      bMat = mShiftManager->FocusAdjustedISToCamera(mWinApp->GetCurrentCamera(),
        mParam->magIndex, ix, iy, delISX, mDefocusForCal);
      if (bMat.xpx == 0.) {
        SEMMessageBox("There is no measured or derived image "
          "shift \ncalibration available at this magnification.\n\n"
          "You must calibrate image shift to take this montage");
        return 1;
      }
      if (mImShiftInBlocks)
        mCamToIS = MatInv(bMat);
      else
        mBinv = MatInv(bMat);
    }

    notSkipping = mUsingMultishot;

    // If skipping outside a Nav item, get the item
    if (mParam->skipOutsidePoly && mWinApp->mNavigator && !notSkipping) {
      polyRealignErrX = 0.;
      polyRealignErrY = 0.;
      // Get specific item if there is an index, otherwise get current or acquire item
      // and if it has a polygon ID, get the polygon by that ID
      if (mParam->insideNavItem >= 0)
        navItem = mWinApp->mNavigator->GetOtherNavItem(mParam->insideNavItem);
      else if (mWinApp->mNavigator->GetCurrentOrAcquireItem(navItem) >= 0) {
        if (navItem->mPolygonID) {
          polyRealignErrX = navItem->mRealignErrX;
          polyRealignErrY = navItem->mRealignErrY;
          navItem = mWinApp->mNavigator->FindItemWithMapID(navItem->mPolygonID, false);
          if (!navItem) {
            SEMMessageBox("The Navigator item to skip pieces outside of cannot"
              " be found\n(This is the polygon that was used to define a supermontage"
              " containing this montage)");
            return 1;
          }
        }
      }
      if (!navItem || navItem->IsNotPolygon()) {
        SEMMessageBox(CString("The Navigator item to skip pieces outside of ") +
          (!navItem ? "has a number out of range" : "is not a polygon"));
        return 1;
      }
      mScope->GetImageShift(ISX, ISY);
      bMat = mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mParam->magIndex);
      if (!bMat.xpx) {
        SEMMessageBox("There is no measured or derived stage "
          "shift \ncalibration available at this magnification.\n\n"
          "You must calibrate stage shift to skip pieces outside a polygon");
        return 1;
      }
      aInv = MatInv(bMat);
      notSkipping = 0;
    }

    mActPostExposure = mWinApp->ActPostExposure(&mConSets[setNum]) && !preCooking &&
      !mUseContinuousMode;

    // Check that the image shift/stage move is not out of range
    for (ix = 0; ix < mNumPieces; ix++) {
      fullX = mParam->binning * (mMontageX[ix] - mMontCenterX);
      fullY = mParam->binning * (mMontageY[ix] - mMontCenterY);
      delISX = fullX * mBinv.xpx + fullY * mBinv.xpy;
      delISY = fullX * mBinv.ypx + fullY * mBinv.ypy;
      if (mDoStageMoves) {

        // Add piece to skip list if out of range and turn off post-actions
        if (delISX + mBaseStageX < mScope->GetStageLimit(STAGE_MIN_X) ||
          delISX + mBaseStageX > mScope->GetStageLimit(STAGE_MAX_X) ||
          delISY + mBaseStageY < mScope->GetStageLimit(STAGE_MIN_Y) ||
          delISY + mBaseStageY > mScope->GetStageLimit(STAGE_MAX_Y)) {
          SEMTrace('S', "Adding piece %d at %.2f %.2f to skip list", ix,
            delISX + mBaseStageX, delISY + mBaseStageY);
          if (AddToSkipListIfUnique(ix))
            mActPostExposure = false;
        }

      } else if (!mShiftManager->ImageShiftIsOK(delISX, delISY, true)) {
        SEMMessageBox(
          "The image shift needed is too close to the end of its range.\n\n"
          "Reset the image shift and reposition specimen stage.\n"
          "If that does not work, then the image shift range is just too big");
        return 1;
      }

      // If skipping outside a Nav item, compute adjusted stage position of center
      // Subtract the realign error; that should be equivalent to adding the error to the
      // polygon coordinates
      if (mParam->skipOutsidePoly && mWinApp->mNavigator && !notSkipping &&
        mNumPieces > 1) {
        stageX = mBaseStageX - polyRealignErrX;
        stageY = mBaseStageY - polyRealignErrY;
        if (mDoStageMoves) {
          stageX += delISX;
          delISX = ISX;
          stageY += delISY;
          delISY = ISY;
        } else {
          delISX += ISX;
          delISY += ISY;
        }
        mWinApp->mNavHelper->ConvertIStoStageIncrement(mParam->magIndex,
          mWinApp->GetCurrentCamera(), delISX, delISY, (float)mScope->FastTiltAngle(),
          stageX, stageY);

        // Check the 4 corners of the frame
        iy = 0;
        for (icx1 = -1; icx1 <= 1; icx1 += 2) {
          for (icy1 = -1; icy1 <= 1; icy1 += 2) {
            cornX = stageX + 0.5 * mParam->binning * (aInv.xpx * icx1 * mParam->xFrame
              + aInv.xpy * icy1 * mParam->yFrame);
            cornY = stageY + 0.5 * mParam->binning * (aInv.ypx * icx1 * mParam->xFrame
              + aInv.ypy * icy1 * mParam->yFrame);
            if (InsideContour(navItem->mPtX, navItem->mPtY, navItem->mNumPoints, cornX,
              cornY)) {
              iy = 1;
              break;
            }
          }
        }

        // Skip piece if no corner is inside
        if (!iy)
          AddToSkipListIfUnique(ix);
        else
          notSkipping++;
      }
    }

    if (mParam->skipOutsidePoly && mWinApp->mNavigator && !notSkipping) {
      mWinApp->AppendToLog("\r\nThis montage is being skipped because"
        " all pieces are outside the polygon\r\n", LOG_OPEN_IF_CLOSED);
      return 0;
    }
  }

  if (mNumToSkip >= mNumPieces) {
    SEMMessageBox("This montage cannot be done.\r\n\r\nAll of the pieces are "
      "outside the range of\r\nstage movement defined in the properties file.");
    return 1;
  }

  // Find any sections that are already in the file
  if (!mTrialMontage) {
    already = ListMontagePieces(mReadStoreMRC, mParam, mParam->zCurrent, mPieceSavedAt);
    if (already < 0) {
      SEMMessageBox("Bad parameters passed to ListMontagePieces; see log");
      return 1;
    }

    // For reading in, add missing frames to skip list
    if (mReadingMontage) {
      for (ix = 0; ix < mNumPieces; ix++) {
        if (mPieceSavedAt[ix] < 0)
          AddToSkipListIfUnique(ix);
      }
      if (!already) {
        SEMMessageBox("There are no pieces at this Z value.");
        return 1;
      }
      statText = "READING MONTAGE";
    }
  }

  // Set up index from "variables" to pieces and back
  ind = 0;
  for (ix = 0; ix < mNumPieces; ix++) {
    for (iy = 0; iy < mNumToSkip; iy++) {
      if (mSkipIndex[iy] == ix) {
        mPieceToVar[ix] = -1;
        break;
      }
    }
    if (iy >= mNumToSkip) {
      mPieceToVar[ix] = ind;
      mVarToPiece[ind++] = ix;
    }
    mIndSequence[ix] = ix;
    mFocusBlockInd[ix] = -1;
  }
  p2vOrig = mPieceToVar;

  // For precooking with skipping, loop on columns and first find last actual piece
  if (preCooking && (cookSkip || skipColumn)) {
    for (ix = 0; ix < mParam->xNframes; ix++) {
      last = -1;
      for (iy = 0; iy < mParam->yNframes; iy++)
        if (mPieceToVar[ix * mParam->yNframes + iy] >= 0)
          last = iy;

      // Then retain pieces that are a multiple of the skip amount, or the last one
      icx1 = 0;
      for (iy = 0; iy < mParam->yNframes; iy++) {
        ind = ix * mParam->yNframes + iy;
        if (mPieceToVar[ind] >= 0) {
          if (icx1 && iy != last &&
            ((cookSkip && (ix + iy) % cookSkip != 0) || cookSkip >= mParam->yNframes ||
            (skipColumn && ix % 2 && ix < mParam->xNframes - 1 &&
              p2vOrig[ind + mParam->yNframes] >= 0 &&
              p2vOrig[ind - mParam->yNframes] >= 0))) {
            mPieceToVar[ind] = -1;
            mNumToSkip++;
          }
          icx1++;
        }
      }
    }
  }

  // Check disk space unless doing tilt series
  if (!mTrialMontage && !preCooking && !mReadingMontage && !mWinApp->DoingTiltSeries()) {
    if (UtilCheckDiskFreeSpace((mParam->xFrame * mParam->yFrame * 
      (mExpectingFloats ? 4. : 2.)) * (mNumPieces - (mNumToSkip + already)), "montage"))
      return 1;
  }

  // Set flag for whether doing IS realignment
  mDoISrealign = false;
  if (useHQ && !mReadingMontage && !mTrialMontage && mParam->ISrealign && 
    !mImShiftInBlocks) {
    bMat = mShiftManager->CameraToIS(mParam->magIndex);
    if (!bMat.xpx) {
      SEMMessageBox("There is no measured or derived image shift calibration\n"
        "at this mag to use for the realignment");
      return 1;
    }
    mDoISrealign = true;
  }

  // Anchor and realignment needed if selected and not precooking
  mNeedAnchor = mNeedRealignToAnchor = mUsingAnchor = mNeedColumnBacklash =
    mDoISrealign && mParam->useAnchorWithIS && !preCooking;
  if (mNeedAnchor)
    mAnchorFilename = mWinApp->mStoreMRC->getFilePath() + ".anchor";

  // If doing non-quality stage montage and the overlap in Y is large enough, then go
  // up and down columns: invert order in the odd columns. Also do for precooking
  mDoZigzagStage = mDoStageMoves && !mReadingMontage && mParam->yNframes > 1 &&
    ((!useHQ && mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), mParam->magIndex)
      * mParam->binning * mParam->yOverlap > mMinOverlapForZigzag) || preCooking);
  if (mDoZigzagStage) {
    icx1 = 0;
    for (ix = 0; ix < mParam->xNframes; ix++) {
      if (ix % 2) {
        for (iy = mParam->yNframes - 1; iy >= 0; iy--)
          mIndSequence[ix * mParam->yNframes + iy] = icx1++;
      } else {
        icx1 += mParam->yNframes;
      }
    }
  }

  // For reading montage, set up sequence from center outward so errors at edge don't
  // propagate.  
  // For realign with anchor, use same sequence in X so there is one "seam" in middle
  icx1 = (mParam->xNframes + 1) / 2;
  ind = 0;
  mSecondHalfStartSeq = -1;
  firstPiece = -1;
  if (mReadingMontage || mUsingAnchor) {
    ix = icx1 - 1;
    for (icx2 = -1; icx2 <= 1; icx2 += 2) {
      if (icx2 > 0)
        mSecondHalfStartSeq = ind;
      while (ix >= 0 && ix < mParam->xNframes) {
        icy1 = (mParam->yNframes + 1) / 2;
        if (mUsingAnchor && ix != icx1 - 1) {

          // For IS align on all but first column, find first and last piece in column
          // Sorry, pieces are numbered from 0 here but from 1 in the index routine
          first = -1;
          for (iy = 0; iy < mParam->yNframes; iy++) {
            if (PieceIndexFromXY(ix + 1, iy + 1) >= 0) {
              if (first < 0)
                first = iy;
              last = iy;
            }
          }

          // If found them, now find piece nearest to middle that has a neighbor in 
          // column that was already done and use that to replace the center Y
          for (j = 0; j < mParam->yNframes && first >= 0; j++) {
            for (iDir = 1; iDir >= -1; iDir -= 2) {
              iy = (first + last) / 2 + j * iDir;
              if (PieceIndexFromXY(ix + 1, iy + 1) >= 0 &&
                PieceIndexFromXY(ix + 1 - icx2, iy + 1)) {
                icy1 = iy + 1;
                first = -1;
                break;
              }
            }
          }
        }
        mColumnStartY[ix] = icy1 - 1;

        // From fixed or chosen center point, sequence outward
        iy = icy1 - 1;
        for (icy2 = -1; icy2 <= 1; icy2 += 2) {
          while (iy >= 0 && iy < mParam->yNframes) {
            j = iy + ix * mParam->yNframes;
            mIndSequence[ind++] = j;
            if (firstPiece < 0)
              firstPiece = j;
            iy += icy2;
          }
          iy = icy1;
        }
        ix += icx2;
      }
      ix = icx1;
    }
  }

  // Figure out backlash by move from first to second piece, except for IS align
  mMoveInfo.backX = mMoveInfo.backY = 0.;
  mMoveBackX = mMoveBackY = 0.;
  if (!mReadingMontage && mDoStageMoves) {
    mMoveInfo.backX = mStageBacklash;
    mMoveInfo.backY = mStageBacklash;
    if (mNumPieces > 1) {
      fullX = mMontageX[1] - mMontageX[0];
      fullY = mMontageY[1] - mMontageY[0];
      if (mUsingAnchor) {
        fullX = 0.;
        fullY = -mParam->yFrame;
      }
      if (fullX * mBinv.xpx + fullY * mBinv.xpy > 0.)
        mMoveInfo.backX = -mStageBacklash;
      if (fullX * mBinv.ypx + fullY * mBinv.ypy > 0.)
        mMoveInfo.backY = -mStageBacklash;
    }
    mMoveInfo.axisBits = axisXY;
    mMoveBackX = mMoveInfo.backX;
    mMoveBackY = mMoveInfo.backY;
    mBacklashAdjustX = mBacklashAdjustY = 0.;
  }

  // Set flag for whether to do focus after every move and resize array for focus blocks
  focusFeasible = useHQ && mParam->magIndex >= mScope->GetLowestNonLMmag()
    && !mReadingMontage && !mTrialMontage && !preCooking;
  mFocusAfterStage = focusFeasible && mParam->focusAfterStage;
  mFocusInBlocks = focusFeasible && mBlockSizeInX * blockSizeInY > 1 && 
    mParam->focusInBlocks && mNumPieces > 1 && !mFocusAfterStage && !mDoISrealign;
  icx1 = 1;
  if (mFocusInBlocks || mImShiftInBlocks) {
    numBlocksX = 1 + (mParam->xNframes - 1) / mBlockSizeInX;
    numBlocksY = 1 + (mParam->yNframes - 1) / blockSizeInY;
    icx1 = numBlocksX * numBlocksY;
  }
  CLEAR_RESIZE(mBlockCenX, int, icx1);
  CLEAR_RESIZE(mBlockCenY, int, icx1);

  // Evaluate continuous mode now that focus is known
  if (!mReadingMontage && !mUsingMultishot) {
    acqExposure = mConSets[setNum].exposure;
    trialExposure = mConSets[TRIAL_CONSET].exposure;
    minContExp = acqExposure;
    if (mUseContinuousMode) {
      if (mFocusAfterStage) {
        SEMMessageBox("You cannot use continuous mode for montaging when \nfocusing"
          "at every piece");
        return 1;
      }

      // This MIGHT work but it is flaky and something is wrong with the delay after image
      // shift
      if (mDoISrealign) {
        SEMMessageBox("You cannot use continuous mode for montaging along with\n"
          "realigning with image shift");
        return 1;
      }

      // So this WON't happen
      if (mDoISrealign && (mConSets[TRIAL_CONSET].binning < mConSets[setNum].binning ||
        trialExposure > acqExposure)) {
        SEMMessageBox("You cannot use continuous mode for montaging when \nrealigning"
          "with image shift and Trial " + CString(trialExposure > acqExposure ?
            "exposure is more than the acquisition exposure" :
            "binning is less than the acquisition binning"));
        return 1;
      }

      if (mDoISrealign)
        minContExp = B3DMAX(0.1f, trialExposure);
    }
  }

  // Set up focus blocks by setting up sequence of pieces and indices to focus centers
  if (mFocusInBlocks || mImShiftInBlocks) {
    int ixbst, iybst, numCen, numx, firstInd, ixBlock, iyBlock, yBlockSt, yBlockEnd;
    int iybEnd, yDir;
    float xmin, xmax, ymin, ymax;
    ixbst = 0;
    ind = 0;
    numCen = 0;
    mFocusAfterStage = false;

    // Loop on blocks in X then blocks in Y, get number in block in each direction
    for (ixBlock = 0; ixBlock < numBlocksX; ixBlock++) {
      numx = mParam->xNframes / numBlocksX;
      if (ixBlock < mParam->xNframes % numBlocksX)
        numx++;

      iybst = 0;
      yDir = 1;
      yBlockSt = 0;
      yBlockEnd = numBlocksY - 1;
      if (mDoZigzagStage && ixBlock % 2) {
        B3DSWAP(yBlockSt, yBlockEnd, iy);
        yDir = -1;
      }
      for (iyBlock = yBlockSt; yDir * (iyBlock - yBlockEnd) <= 0; iyBlock += yDir) {
        balancedGroupLimits(mParam->yNframes, numBlocksY, iyBlock, &iybst, &iybEnd);
        
        // Loop on pieces in block in usual order, add to sequence, keep track of index
        // of first included piece and min and max positions of each included piece
        firstInd = -1;
        for (ix = ixbst; ix < ixbst + numx; ix++) {
          for (iy = iybst; iy <= iybEnd; iy++) {
            icx1 = iy + ix * mParam->yNframes;
            mIndSequence[ind++] = icx1;
            if (mPieceToVar[icx1] < 0)
              continue;
            if (firstInd < 0) {
              firstInd = icx1;
              mFocusBlockInd[icx1] = numCen;
              xmin = mMontageX[icx1];
              xmax = mMontageX[icx1];
              ymin = mMontageY[icx1];
              ymax = mMontageY[icx1];
            } else {
              mFocusBlockInd[icx1] = -(2 + numCen);
              xmin = B3DMIN(xmin, mMontageX[icx1]);
              xmax = B3DMAX(xmax, mMontageX[icx1]);
              ymin = B3DMIN(ymin, mMontageY[icx1]);
              ymax = B3DMAX(ymax, mMontageY[icx1]);
            }
          }
        }

        // If there were any included pieces, set index to actual block number and store
        // the center position for focusing
        if (firstInd >= 0) {
          mBlockCenX[numCen] = (xmin + xmax) / 2;
          mBlockCenY[numCen++] = (ymin + ymax) / 2;
        }
      }
      ixbst += numx;
    }
  }

  if (mDoISrealign) {
    CLEAR_RESIZE(mAdjacentForIS, int, mNumPieces);
    FindAdjacentForISrealign();
  } else
    CLEAR_RESIZE(mAdjacentForIS, int, 1);

  lastDone = -1;
  firstUndone = -1;
  mRealignInterval = 0;
  if (!mTrialMontage && !mReadingMontage) {
    external = mWinApp->DoingTiltSeries() || (mWinApp->mNavigator &&
      mWinApp->mNavigator->GetAcquiring());
    
    // Confirm if not under external control and Z is not in order
    if (!already && !external && mParam->zCurrent != mParam->zMax + 1)
      if (AfxMessageBox("The current Z value is not the next Z value for this file.\n\n"
        "Are you sure you want to continue with this Z value?",
        MB_OKCANCEL | MB_ICONQUESTION) == IDCANCEL)
        return 1;      

    // If some pieces exist determine first if it is restartable midway through
    if (already) {

      alignable = mDoISrealign;
      if (mDoStageMoves) {
        mNumDoing = 0;
        for (i = 0; i < mNumPieces; i++) {
          ind = mIndSequence[i];
          if (mPieceToVar[ind] >= 0) {
            if (mPieceSavedAt[ind] >= 0) {
              mNumDoing++;
              lastDone = i;
              if (alignable) {

                // For image shift align, try to get stage position for every piece
                // and also IS although that is optional, and set the acquired positions
                alignable = !mWinApp->mStoreMRC->getStageCoord(mPieceSavedAt[ind], 
                  delISX, delISY);
                if (alignable) {
                  mWinApp->mStoreMRC->getImageShift(mPieceSavedAt[ind], ISX, ISY);
                  SetAcquiredStagePos(ind, delISX, delISY, ISX, ISY);
                }
              }
            } else if (firstUndone < 0)
              firstUndone = i;
          }
        }
      }

      // If it is complete but they want to overwrite, set firstUndone past the end
      if (firstUndone < 0 && mNumOverwritePieces > 0)
        firstUndone = lastDone + 1;

      // If it is still realignable, need to check if the anchor still exists and get
      // the stage position
      if (firstUndone > lastDone && mDoISrealign) {
        if (!alignable) {
          mWinApp->AppendToLog("Cannot resume this montage because no stage coordinates "
            "were saved in the image file");
          firstUndone = -1;
        } else if (mUsingAnchor) {
          storeMRC = UtilOpenOldMRCFile(mAnchorFilename);
          if (!storeMRC) {
            mWinApp->AppendToLog("Cannot resume this montage because the anchor image is"
            " not available");
            firstUndone = -1;
          } else {
            storeMRC->getStageCoord(0, mAnchorStageX, mAnchorStageY);
            delete storeMRC;
            ComputeMoveToPiece(firstPiece, false, icx1, icx2, delISX, delISY);
            delISX = mAnchorStageX - mMoveInfo.x;
            delISY = mAnchorStageY - mMoveInfo.y;
            if (sqrt(delISX * delISX + delISY * delISY) > 0.2 * mShiftManager->GetPixelSize
              (mWinApp->GetCurrentCamera(), mParam->magIndex) * mParam->binning * 
              B3DMIN(mParam->xFrame, mParam->yFrame)) {
              mWinApp->AppendToLog("Cannot resume this montage because there is not an "
                "anchor image taken at the right stage coordinates");
              firstUndone = -1;
            }
          }
        }
      }

      // If it is all done or not resumable, confirm unless under external control
      if (firstUndone <= lastDone) {
        if (!external && AfxMessageBox(
          "There are already pieces in the file at the current Z value.\n\n"
          "Are you sure you want to overwrite these pieces?", 
          MB_OKCANCEL | MB_ICONQUESTION) == IDCANCEL)
          return 1; 
        firstUndone = -1;
      } else {

        // If it is resumable, back off firstUndone to overwrite pieces
        if (mNumOverwritePieces > 0) {
          mNumOverwritePieces = B3DMIN(mNumDoing, mNumOverwritePieces);
          ix = mNumOverwritePieces;
          for (i = firstUndone - 1; i >= 0 && ix > 0; i--) {
            ind = mIndSequence[i];
            if (mPieceToVar[ind] >= 0) {
              mNumDoing--;
              firstUndone = i;
              ix--;
            }
          }
          mess.Format("  Resuming the montage and overwriting the last %d pieces",
            mNumOverwritePieces);
        }

        if (external || mNumOverwritePieces > 0) {
          mWinApp->AppendToLog("The montage at this Z value is partially done and "
            "resumable:");
        } else {
          i = SEMThreeChoiceBox("The montage at this Z value is partially done and "
            "resumable.\n\nPress:\n\"Resume\" to resume the montage where it leaves "
            "off,\n\n\"Redo from Start\" to redo it from the beginning, \n\n\"Cancel\""
            " to stop the montage.", "Resume", "Redo from Start", "Cancel",
            MB_YESNOCANCEL | MB_ICONQUESTION);
          if (i == IDCANCEL)
            return 1;
          if (i == IDNO)
            firstUndone = -1;
        }
        if (firstUndone > 0) {
         if (mNumOverwritePieces > 0)
            mWinApp->AppendToLog(mess);
          else
            mWinApp->AppendToLog("  Resuming the montage from where it left off");
          mWinApp->AppendToLog("  Only a partial overview will be shown and pieces will"
            " not be shifted into register");
          if (mUsingAnchor) {
            mNeedAnchor = false;
            mNeedRealignToAnchor = lastDone < mSecondHalfStartSeq;
          }
        }
      }
    }

    // Determine whether realigning
    if (useHQ && mParam->realignToPiece && mParam->realignInterval >= MIN_Y_MONT_REALIGN
      && mParam->yNframes >= MIN_Y_MONT_REALIGN && !mDoISrealign)
      mRealignInterval = mParam->realignInterval;
  }

  // Need arrays for offsets either doing the IS realign or if reading in anything
  mHaveStageOffsets = mDoISrealign;
  i = (mDoISrealign || mReadingMontage) ? mNumPieces : 1;
  CLEAR_RESIZE(mStageOffsetX, float, i);
  CLEAR_RESIZE(mStageOffsetY, float, i);

  // Read in any offsets if resuming or reading in; set flag based on whether they read
  // in OK, then handle the minioffset arrays
  if (mReadingMontage || (mDoISrealign && firstUndone > 0))
    mHaveStageOffsets = AutodocStageOffsetIO(false, -1) == 0;

  // Build the control set
  if (!mReadingMontage && !preCooking  && !mUsingMultishot) {
    if (cam->K2Type && mConSets[setNum].doseFrac && mCamera->GetPluginVersion(cam) < PLUGIN_CAN_SAVE_SUBAREAS) {
      ix = cam->sizeX / mParam->binning;
      iy = cam->sizeY / mParam->binning;
      mCamera->CenteredSizes(ix, cam->sizeX, cam->moduloX, left, right, 
      iy, cam->sizeY, cam->moduloY, top, bottom, mParam->binning);
      if (mParam->xFrame != ix || mParam->yFrame != iy) {
        mess.Format("Turn off Dose Fractionation in the %s parameters or\r\n"
          "reopen the montage with full-sized frames\r\n(%d x %d at this binning)",
          (LPCTSTR)(*(mWinApp->GetModeNames() + useSetNum)), ix, iy);
        SEMMessageBox(CString("Montage frames cannot be subareas with Dose Fractionation"
          " on unless the SEMCCD plugin is upgraded\n\n") + mess);
        mWinApp->AppendToLog(mess);
        return 1;
      }
    } 
    mConSets[MONTAGE_CONSET] = mConSets[setNum];
    mCamera->CenteredSizes(mParam->xFrame, cam->sizeX, cam->moduloX, left, right, 
      mParam->yFrame, cam->sizeY, cam->moduloY, top, bottom, mParam->binning);
    mConSets[MONTAGE_CONSET].left = left * mParam->binning;
    mConSets[MONTAGE_CONSET].right = right * mParam->binning;
    mConSets[MONTAGE_CONSET].top = top * mParam->binning;
    mConSets[MONTAGE_CONSET].bottom = bottom * mParam->binning;
    mConSets[MONTAGE_CONSET].binning = mParam->binning;
    mConSets[MONTAGE_CONSET].mode = SINGLE_FRAME;
    mNumContinuousAlign = 0;
    if (mTrialMontage || mUseContinuousMode) {
      mConSets[MONTAGE_CONSET].saveFrames = 0;
      mConSets[MONTAGE_CONSET].doseFrac = 0;
    }
    if (mUseContinuousMode) {
      mConSets[MONTAGE_CONSET].mode = CONTINUOUS;
      mNumDropAtContinStart = B3DNINT(mConSets[MONTAGE_CONSET].drift / 
        mConSets[MONTAGE_CONSET].exposure);
      mConSets[MONTAGE_CONSET].drift = 0.;
      mCamera->ChangePreventToggle(1);
      mNumContinuousAlign = B3DNINT(acqExposure / minContExp);
      while (mNumContinuousAlign > 1) {
        mConSets[MONTAGE_CONSET].exposure = acqExposure / mNumContinuousAlign;
        mCamera->ConstrainExposureTime(cam, &mConSets[MONTAGE_CONSET]);
        if (0.8 * minContExp > mConSets[MONTAGE_CONSET].exposure)
          mNumContinuousAlign--;
        else
          break;
      }
      if (mNumContinuousAlign > 1)
        SEMTrace('S', "Continuous: num %d exp %.3f -> %.3f  minExp %.3f",
          mNumContinuousAlign, acqExposure, mConSets[MONTAGE_CONSET].exposure,minContExp);
    }

    if (cam->OneViewType && (useHQ ? mParam->noHQDriftCorr : mParam->noDriftCorr))
      mConSets[MONTAGE_CONSET].alignFrames = 0;
    
    // Adjust exposure, try to do something about reducing drift settling if exposure is
    // now very short.  This is not obsoleted by the binning lock - it is applied in 
    // navigator acquire with new file at item, keep NavHelper AssessAcquireProblems syncd
    // But it does also set the prescan exposure
    float binRatio = (float)mConSets[setNum].binning / mParam->binning;
    if (binRatio < 0.9 && mWinApp->LowDoseMode())
      binRatio *= B3DMIN(2., B3DMAX(1., 1. / binRatio));
    mConSets[MONTAGE_CONSET].exposure *= binRatio * binRatio;
    if (mParam->binning > mConSets[setNum].binning && mConSets[MONTAGE_CONSET].drift > 0.)
    {
      if (mConSets[MONTAGE_CONSET].exposure < 0.1)
        mConSets[MONTAGE_CONSET].drift = 0.;
      else if (mConSets[MONTAGE_CONSET].exposure < 0.25)
        mConSets[MONTAGE_CONSET].drift = cam->builtInSettling / 2.;
    }

    // "Use up" the once dark settings in the control sets, per camera and master
    mConSets[useSetNum].onceDark = 0;
    (mWinApp->GetCamConSets() + useSetNum + mWinApp->GetCurrentCamera() * 
      MAX_CONSETS)->onceDark = 0;
  }

  mDoCorrelations = !(useHQ && mParam->skipCorrelations) && !mUsingMultishot &&
    firstUndone <= 0 && !mTrialMontage;

  // Try to get edge shifts and piece shift solution if reading in and doing correlation
  mAlreadyHaveShifts = false;
  if (mReadingMontage && mDoCorrelations) {
    mAlreadyHaveShifts = !mRedoCorrOnRead && 
      AutodocShiftStorage(false, &mUpperShiftX[0], &mUpperShiftY[0]) == 0;
    if (mAlreadyHaveShifts)
      mAlreadyHaveShifts = FindBestShifts(mNumPieces - mNumToSkip, &mUpperShiftX[0], 
      &mUpperShiftY[0], &mBmat[0], mBefErrMean, mBefErrMax, mAftErrMean, mAftErrMax, 
      mWgtErrMean, mWgtErrMax, false) == 0;
  }

  // Allocate center piece and bail out if that fails
  if (!preCooking) {
    NewArray2(mCenterData, short int, mParam->xFrame, mParam->yFrame * 
      (mExpectingFloats ? 2 : 1));
    if (!mCenterData) {
      SEMMessageBox("Cannot get memory for composing center piece");
      return 1;
    }
  }

  // Allocate correlation arrays if doing them; bail out and clean up if any fail
  if (mDoCorrelations) {
    NewArray(mLowerPad, float, mPadPixels);
    NewArray(mUpperPad, float, mPadPixels);
    NewArray(mLowerCopy, float, mPadPixels / (mNumXcorrPeaks > 1 ? 1 : 2));
    if (mXCorrBinning > 1)
      NewArray(mBinTemp, float, B3DMAX(mBoxPixels[0], mBoxPixels[1]));
    if (!mLowerPad || !mUpperPad || !mLowerCopy || (mXCorrBinning > 1 && !mBinTemp)) {
      SEMMessageBox("Cannot get memory for edge correlation arrays");
      CleanPatches();
      return 1;
    }
  }

  // That was the last return, so start the new log here and trace output
  if (!mReadingMontage && !preCooking && mAutosaveLog && 
    mWinApp->mLogWindow && !mWinApp->DoingTiltSeries() &&
    mWinApp->mLogWindow->SaveFileNotOnStack(mWinApp->mStoreMRC->getName())) {
      mWinApp->mLogWindow->DoSave();
      mWinApp->mLogWindow->CloseLog();
      mess.Format("%.3f: Log restarted %s", SEMSecondsSinceStart(), 
        mWinApp->mDocWnd->DateTimeForTitle());
      mWinApp->AppendToLog(mess);
      mWinApp->AppendToLog(mWinApp->GetStartupMessage(true));
  }
  SEMTrace('M', preCooking ? "Montage Precook Start" : "Montage Start");

  // Set up for mini view - zoom is always one for prescan
  mMiniZoom = mTrialMontage ? 1 : mParam->overviewBinning;
  mConvertMini = mParam->byteMinScale || mParam->byteMaxScale;
  memoryLimit = mWinApp->GetMemoryLimit();
  if (memoryLimit)
    currentUsage = mWinApp->mBufferWindow.MemorySummary();

  borderTry = ((mDoCorrelations || mUsingMultishot) && mParam->shiftInOverview && 
    !mAlreadyHaveShifts && mNumPieces > 1) ? 10 : -1;

  // Enter loop to try to get minizoom with different border strategies and zooms
  mMiniData = NULL;
  while (!mMiniData && !preCooking) {

    // Set up basic sizes for tiling into place
    // Increase X size to multiple of 4 in case it it is converted to a byte image
    // because it has to be a multiple of 4 to be usable for display
    mMiniFrameX = mParam->xFrame / mMiniZoom;
    mMiniDeltaX = mMiniFrameX - mParam->xOverlap / mMiniZoom;
    mMiniSizeX = 4 * ((mMiniFrameX + (mParam->xNframes - 1) * mMiniDeltaX + 3) / 4);
    mMiniFrameY = mParam->yFrame / mMiniZoom;
    mMiniDeltaY = mMiniFrameY - mParam->yOverlap / mMiniZoom;
    mMiniSizeY = mMiniFrameY + (mParam->yNframes - 1) * mMiniDeltaY;
    mMiniArrayX = mMiniSizeX;
    mMiniArrayY = mMiniSizeY;
    mMiniBorderY = 0;

    // Modify if trying to do deferred tiling
    if (borderTry >= 0) {
      mMiniBorderY = (int)((1.2 + borderTry / 10.) * mMiniFrameY);
      mMiniArrayX = mParam->xNframes * mMiniFrameX;
      mMiniArrayY = mParam->yNframes * mMiniFrameY + mMiniBorderY;
    }

    // Go on to next attempt if the array is huge
    if ((double)mMiniArrayX * mMiniArrayY * (mExpectingFloats ? 2 : 1) > 2147000000) {
      NextMiniZoomAndBorder(borderTry);
      continue;
    }
    if (mReadingMontage && mCenterOnly)
      break;

    // Otherwise see if there is a memory limit that needs to be satisfied
    // The needed amount is 2 * size for short array, + size for pixmap
    // unless it is converted to bytes
    if (memoryLimit) {
      needed = (mExpectingFloats ? 5. : 3.) * mMiniArrayX * mMiniArrayY + 
        mMiniSizeX * mMiniSizeY;
      if (mConvertMini)
        needed =  mMiniArrayX * mMiniArrayY; 
      tryForMemory = currentUsage + needed < memoryLimit;
    } else
      tryForMemory = true;

    // Try to get the memory
    if (tryForMemory) {
      if (mConvertMini) {
        NewArray2(mMiniByte, unsigned char, mMiniArrayX, mMiniArrayY);
        mMiniData = (short int *) mMiniByte;
      } else
        NewArray2(mMiniData, short int, mMiniArrayX, mMiniArrayY * 
        (mExpectingFloats ? 2 : 1));
    }
    mMiniUshort = (unsigned short *)mMiniData;
    mMiniFloat = (float *)mMiniData;
    if (!mMiniData) {

      // If failed, try again as long as size is still substantial
      if (!mTrialMontage && mMiniSizeX > 1800 && mMiniSizeY > 1800) {
        NextMiniZoomAndBorder(borderTry);
      } else {
        if (mTrialMontage)
          mWinApp->AppendToLog("Not enough memory for overview image", 
            LOG_OPEN_IF_CLOSED);
        else if (mMiniZoom == mParam->overviewBinning)
          mWinApp->AppendToLog("Not enough memory for overview image with the current "
            "overview binning", LOG_OPEN_IF_CLOSED);
        else
          mWinApp->AppendToLog("Not enough memory for overview image even with a higher "
            "overview binning", LOG_OPEN_IF_CLOSED);
        break;
      }
    }
  }

  if (!mTrialMontage && mMiniData && mMiniZoom > mParam->overviewBinning) {
    mWinApp->AppendToLog("The overview binning needed to be increased to get memory "
      "for the overview image", LOG_OPEN_IF_CLOSED);
    if (!mReadingMontage)
      mWinApp->AppendToLog("You will probably be able to see it at full resolution "
        "by\r\n making it a map and loading the map with conversion to bytes", 
        LOG_OPEN_IF_CLOSED); 
  }

  // Put parameters in the mini offset structure; compute a base and delta for
  // getting boundaries midway in overlap zones
  SetMiniOffsetsParams(mMiniOffsets, mParam->xNframes, mMiniFrameX, mMiniDeltaX,
     mParam->yNframes, mMiniFrameY, mMiniDeltaY);

  // Clear out arrays if pieces are being skipped, or clear overview if shifting
  mNeedToFillMini = mMiniData && (mNumToSkip || mParam->shiftInOverview || 
    firstUndone > 0 || mMiniFrameX + (mParam->xNframes - 1) * mMiniDeltaX < mMiniSizeX);
  mNeedToFillCenter = mNumToSkip > 0;
  mMiniFillVal = 0;
  mPcIndForFillVal = -1;
  if (mMiniBorderY && !mReadingMontage && !mUsingAnchor) {
    for (ix = 1; ix <= mParam->xNframes; ix++) {
      delISX = (mParam->xNframes + 1) / 2. - ix;
      for (iy = 1; iy <= mParam->yNframes; iy++) {
        delISY = (mParam->yNframes + 1) / 2. - iy;
        dist = delISX * delISX + delISY * delISY;
        i = PieceIndexFromXY(ix, iy);
        if ((mPcIndForFillVal < 0 || dist < minDist) && i >= 0) {
          minDist = dist;
          mPcIndForFillVal = i;
        }
      }
    }
  }

  // Fill center in case no pieces occur and there is no mini
  if (mNumToSkip && !preCooking)
    for (ix = 0; ix < mParam->xFrame * mParam->yFrame; ix++)
      mCenterData[ix] = 0;

  // Compute effective binning for mini view that makes it no bigger than camera size

  mMiniEffectiveBin = B3DMAX((float)cam->sizeX / mMiniSizeX, 
    (float)cam->sizeY / mMiniSizeY);
  
  if (mCamera->CameraBusy()) 
    mCamera->StopCapture(0);

  // If adjusting focus, get the matrix to convert from camera coordinates
  // to specimen, and store factors for getting the Y distance as function of
  // montage X and Y coordinates.
  // The minus sign worked when this was done by columns
  // TODO: does this work on inverted X axis scope?
  mBaseFocus = mScope->GetDefocus();
  if (mParam->adjustFocus && !mReadingMontage && mUsingImageShift &&
    !mUsingMultishot) {
    double angle = DTOR * mScope->GetTiltAngle();
    float defocusFac = mShiftManager->GetDefocusZFactor() * 
      (mShiftManager->GetStageInvertsZAxis() ? -1 : 1);
    if (mWinApp->GetSTEMMode())
      defocusFac = -1. / mWinApp->mFocusManager->GetSTEMdefocusToDelZ(-1);
    aInv = mShiftManager->CameraToSpecimen(mParam->magIndex);

    mFocusPitchX = -tan(angle) * aInv.ypx * defocusFac;
    mFocusPitchY = -tan(angle) * aInv.ypy * defocusFac;
  }

  // Start out by letting the next piece index routine find first piece
  mSeqIndex = -1;
  if (firstUndone > 0)
    mSeqIndex = firstUndone - 1;
  else
    mNumDoing = 0;
  mStartTime = GetTickCount();
  mLastNumDoing = -1;
  mInitialNumDoing = mNumDoing;
  SetNextPieceIndexXY();
  mNeedBacklash = true;   // Would it ever not need this?  Needs to be set firstUndone>0
  mFirstResumePiece = firstUndone > 0 ? mPieceIndex : -1;
  mMovingStage = false;
  mPreMovedStage = false;
  mNeedISforRealign = false;
  mErrorSumX = mErrorSumY = 0.;
  mNumErrSum = 0;
  mWinApp->UpdateBufferWindows();
  if (!mUsingMultishot)
    mWinApp->SetStatusText(MEDIUM_PANE, statText);
  mNumCenterSaved = 0;
  mPredictedErrorX = mPredictedErrorY = 0;
  mLastFailed = true;
  mNumTooDim = 0;
  mNumStageErrors = 0;
  mShotFilm = false;
  mLoweredMag = false;
  mNumDoneInColumn = 0;
  mNumSinceRealign = 0;
  mNumOverwritePieces = 0;
  mDriftRepeatCount = 0;
  mDidBlockIS = 0;

  // If multigrid was doing LM montage and it was stopped, tell it to resume
  if (!mReadingMontage && !preCooking && !mTrialMontage &&
    mWinApp->mMultiGridTasks->WasStoppedInLMMont())
    mWinApp->mMultiGridTasks->ResumeMulGridSeq(0);

  // If doing multishot, fetch and set up the parameters
  if (mUsingMultishot) {
    MultiShotParams *masterMSparam = mWinApp->mNavHelper->GetMultiShotParams();;
    mMultiShotParams = new MultiShotParams;
    *mMultiShotParams = *masterMSparam;
    icx1 = mParam->binning * (mParam->xFrame - mParam->xOverlap);
    icy1 = mParam->binning * (mParam->yFrame - mParam->yOverlap);
    mMultiShotParams->holeISXspacing[0] = icx1 * mBinv.xpx;
    mMultiShotParams->holeISYspacing[0] = icx1 * mBinv.ypx;
    mMultiShotParams->holeISXspacing[1] = icy1 * mBinv.xpy;
    mMultiShotParams->holeISYspacing[1] = icy1 * mBinv.ypy;
    mMultiShotParams->numHoles[0] = mParam->xNframes;
    mMultiShotParams->numHoles[1] = mParam->yNframes;
    mMultiShotParams->holeMagIndex[0] = mParam->magIndex;
    mMultiShotParams->tiltOfHoleArray[0] = 0;
    mMultiShotParams->useCustomHoles = false;
    mMultiShotParams->doHexArray = false;
    mWinApp->mParticleTasks->SetNextMSParams(mMultiShotParams);
    mWinApp->AddIdleTask(TASK_MONT_MULTISHOT, 0, 0);
    icx1 = mWinApp->mParticleTasks->StartMultiShot(-1, 0, 0., 0, 0.,
      mMultiShotParams->extraDelay, true, 0, 0, true, MULTI_HOLES);
    return B3DMAX(0, icx1);
  }

  // If testing duplicates and the existing image is a montage, copy C to A so the last
  // record will always be in B
  mDuplicateRetryCount = 0;
  if (!mReadingMontage && !preCooking && mDuplicateRetryLimit >= 0 
    && mCamParams[mWinApp->GetCurrentCamera()].FEItype && mImBufs[2].mImage &&
    mImBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW)
    mBufferManager->CopyImageBuffer(2, 0);

  // Do fake montage by reading in each piece and calling save
  // but first roll the buffers two places
  if (mReadingMontage)
    for (i = mBufferManager->GetShiftsOnAcquire(); i > 1 ; i--)
      mBufferManager->CopyImageBuffer(i - 2, i);

  // If resuming and realign is enabled, start with a realignment
  if (firstUndone > 0 && mRealignInterval > 0 && !RealignToExistingPiece())
    return 0;

  // Start by lowering screen for precook if needed
  if (preCooking && mScope->GetScreenPos() != spDown) {
    if (!mScope->SetScreenPos(spDown))
      return 1;
    mWinApp->AddIdleTask(CEMscope::TaskScreenBusy, TASK_MONTAGE, 0, 60000);
    return 0;
  }

  // Start the first piece and return unless reading synchronously
  if (!mReadingMontage || !mSynchronous) {
    DoNextPiece(NO_PREVIOUS_ACTION);
    return 0;
  }

  // Synchronous: loop on the pieces.  Done routine is called regardless of fate
  while (mPieceIndex >= 0 && mPieceIndex < mNumPieces) {
    int err = DoNextPiece(NO_PREVIOUS_ACTION);
    if (err)
      return err;
  }

  return 0;
}

////////////////////////////////////////
// MAIN ROUTINES FOR MONTAGE ACQUISITION
////////////////////////////////////////

#define MONTAGE_DELAY_FACTOR 1.33f

// THE MAIN LOOP FOR DOING SUCCESSIVE ACTIONS
int EMmontageController::DoNextPiece(int param)
{
  double ISX, ISY, postISX, postISY, sterr, stageX, stageY;
  int timeOut = 120000;
  float delayFactor = MONTAGE_DELAY_FACTOR;
  CString report, str;
  bool doBacklash, invertBacklash, nextPieceBlockCen, gotStageXY = false;
  bool checkStats = false;
  bool precooking = mTrialMontage == MONT_TRIAL_PRECOOK;
  BOOL moveStage = mDoStageMoves;
  double adjISX, adjISY;
  float alignISDelayFac = 0.25f;
  int ix, type, delay, iDelX, iDelY, nextPiece, startsCol, adocInd, nameInd;
  KImage *image;
  void *data;
  float sDmin, denmin;
  float amin, amax, amean, expMin, expMax, expMean, lastMin, lastMax, lastMean;
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};

  if (mTrialMontage)
    delayFactor /= 4.f;
  //DWORD shiftDelay = mShiftManager->GetShiftDelay();

  if (mRunningMacro && !mWinApp->mMacroProcessor->GetLastCompleted()) {
    mRunningMacro = false;
    StopMontage();
    return 1;
  }
  mRunningMacro = false;

  if (mRestoringStage > 0)
    return 0;

  if (mUsingMultishot) {
    StopMontage();
    return 0;
  }

  // If already stopped, redo the image shift in case camera did it after stop
  // Or do the stage move in case it was blocked before or camera was busy 
  // (dubious if run from macro etc)
  if (mPieceIndex < 0) {
    if (mReadingMontage)
      return 0;
    if (mDoStageMoves) {
      if (!mScope->StageBusy())
        StartStageRestore();
    } 
    if (mUsingImageShift) {
      mScope->SetImageShift(mBaseISX, mBaseISY);
      mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
    }
    return 0;
  }

  // If reading in, set pane and get image, then either call save or pass through idle
  if (mReadingMontage) {
    report.Format("PIECE %d of %d", mNumDoing, mNumPieces - mNumToSkip);
    mWinApp->SetStatusText(SIMPLE_PANE, report);

    // For the first read-in image, if saving was the last action, need to check for it
    // reading in the wrong data - get the min/max/mean for the images in question
    if (!mSeqIndex && mLastSavedAtZ >= 0) {
      adocInd = mReadStoreMRC->GetAdocIndex();
      nameInd = mReadStoreMRC->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
      if (adocInd >= 0 && !AdocGetMutexSetCurrent(adocInd)) {
        if (!AdocGetThreeFloats(names[nameInd], mPieceSavedAt[mPieceIndex], 
          ADOC_MINMAXMEAN, &expMin, &expMax, &expMean) && 
          !AdocGetThreeFloats(names[nameInd], mLastSavedAtZ,
            ADOC_MINMAXMEAN, &lastMin, &lastMax, &lastMean))
          checkStats = true;
        AdocReleaseMutex();
      }
    }

    for (int readLoop = 0; readLoop < (checkStats ? 3 : 1); readLoop++) {
      int err = mBufferManager->ReadFromFile(mReadStoreMRC, mPieceSavedAt[mPieceIndex], 0,
        true);
      if (err) {
        StopMontage();
        return err;
      }
      if (checkStats) {

        // If checking, get the MMM and see if everything matches the last written image
        // better than the current image
        mImBufs->mImage->getSize(iDelX, iDelY);
        fullArrayMinMaxMean(mImBufs->mImage->getData(), mImBufs->mImage->getType(), iDelX,
          iDelY, &amin, &amax, &amean);
        if (fabs(lastMin - amin) < fabs(expMin - amin) && 
          fabs(lastMax - amax) < fabs(expMax - amax) &&
          fabs(lastMean - amean) < fabs(expMean - amean)) {
          if (readLoop < 2) {
            SEMAppendToLog("WARNING: First read-in image matches last saved one better"
              " than expected image, reading it again");
            mWinApp->mStoreMRC->Flush();
            Sleep(1000);
          } else {
            SEMAppendToLog("WARNING: It still matches; giving up");
          }
        } else {
          SEMTrace('1', "Image stats match expected values");
          break;
        }
      }
    }

    // If anything else gets read, clear out last saved Z to avoid checks on just reading
    if (mSeqIndex)
      mLastSavedAtZ = -1;
    SavePiece();
    if (!mSynchronous)
      mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_MONTAGE, 0, 0);
    return 0;
  }

  // The action index comes in as last action, needs to be set or incremented
  if (param == NO_PREVIOUS_ACTION)
    mAction = SET_PANE;
  else
    mAction = (mAction + 1) % mNumActions;

  if (mFocusing) {
    report.Format("%s, %d/%d", mNumPieces - mNumToSkip > 1000 ? "MONTAGE" : "MONTAGING",
      mNumDoing, mNumPieces - mNumToSkip);
    AddRemainingTime(report);
    mWinApp->SetStatusText(MEDIUM_PANE, report);
  }    

  mMovingStage = false;
  mFocusing = false;

  // THE ACTION LOOP
  for (;;) {

    nextPiece = NextPieceIndex();
    ComputeMoveToPiece(mPieceIndex, false, iDelX, iDelY, adjISX, adjISY);
    nextPieceBlockCen = nextPiece < mNumPieces && mFocusBlockInd[nextPiece] >= 0;
    if (mAction == SET_PANE) {
      report.Format("%s, %d/%d", B3DCHOICE(precooking, "PRECOOKING", 
        mNumPieces - mNumToSkip > 1000 ? "MONTAGE" : "MONTAGING"),
        mNumDoing, mNumPieces - mNumToSkip);
      AddRemainingTime(report);
      mWinApp->SetStatusText(MEDIUM_PANE, report);

    } else if (mAction == REALIGN_ANCHOR && mNeedRealignToAnchor && 
      mSeqIndex >= mSecondHalfStartSeq) {
      return RealignToAnchor();

    } else if (mAction == ALIGN_EXISTING && PieceNeedsRealigning(mPieceIndex, false)) {
      if (!RealignToExistingPiece())
        return 0;
      mNumSinceRealign = 0;
      if (PieceStartsColumn(mPieceIndex))
        mNumDoneInColumn = 0;
    
    } else if (mAction == MOVE_TO_FOCUS_BLOCK && moveStage && 
      (mFocusBlockInd[mPieceIndex] >= 0 || mPieceIndex == mFirstResumePiece)) {
      if (mUseContinuousMode && mFocusInBlocks)
        mCamera->StopCapture(-1);
      if (!mPreMovedStage) {

        // For block focusing, reset the image shift for hybrid montage, 
        // then start the stage move and return
        if (mImShiftInBlocks)
          mScope->SetImageShift(mBaseISX, mBaseISY);
        ComputeMoveToPiece(mPieceIndex, mFocusInBlocks || mImShiftInBlocks, iDelX, iDelY,
          adjISX, adjISY);
        mScope->MoveStage(mMoveInfo, mImShiftInBlocks);
        SEMTrace('S', "DoNextPiece moving stage to %.3f %.3f for %s block",
          mMoveInfo.x, mMoveInfo.y, mImShiftInBlocks ? "image shift" : "focus");
        mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE, 0, timeOut);
        mMovingStage = true;
        mNeedBacklash = true;
        return 0;
      }

    } else if (((mAction == FOCUS_AT_PIECE && (mFocusAfterStage || mDriftRepeatCount)) ||
      (mAction == BLOCK_FOCUS && mFocusInBlocks && 
      (mFocusBlockInd[mPieceIndex] >= 0 || mPieceIndex == mFirstResumePiece)))) {
        
      // Do the focus next, make it respect the extra delay
      delay = 1000. * mParam->hqDelayTime;
      if (mDriftRepeatCount)
        mShiftManager->SetGeneralTimeOut(GetTickCount(), 
        (int)(1000. * mDriftRepeatDelay));
      else if (delay > mShiftManager->GetStageMovedDelay())
        mShiftManager->SetStageDelayToUse(delay);
      mWinApp->mFocusManager->AutoFocusStart(1);
      mWinApp->AddIdleTask(NULL, TASK_MONTAGE_FOCUS, 0, 0);
      mFocusing = true;
      return 0;

    } else if (mAction == CHECK_DRIFT && mParam->repeatFocus && (mFocusAfterStage || 
      (mFocusInBlocks && mFocusBlockInd[mPieceIndex] >= 0 && mImShiftInBlocks))) {
      mWinApp->mFocusManager->GetLastDrift(ISX, ISY);
      ISX = sqrt(ISX * ISX + ISY * ISY);
      if (ISX > mParam->driftLimit) {
        if (mDriftRepeatCount < mDriftRepeatLimit) {
          mDriftRepeatCount++;
          mAction = FOCUS_AT_PIECE - 1;
        } else {
          mDriftRepeatCount = 0;
          report.Format("Drift is still %.2f nm/sec after %d retries, just continuing",
            ISX, mDriftRepeatLimit);
          mWinApp->AppendToLog(report);
        }
      } else
        mDriftRepeatCount = 0;

    } else if (mAction == MOVE_STAGE && moveStage && !mPreMovedStage && 
      !mImShiftInBlocks) {

      // Stage was not moved: need to move it.  Do image shift first
      if (mDoISrealign && mAdjacentForIS[mPieceIndex] >= 0) {
        startsCol = PieceStartsColumn(mPieceIndex);
        if (!mFocusAfterStage && !mNeedColumnBacklash && (startsCol > 0 ||
          mISrealignInterval < 2 ||
          (startsCol == 0 && (mNumDoneInColumn % mISrealignInterval == 0)))) {
          if (SetImageShiftTestClip(adjISX, adjISY, alignISDelayFac))
            return 1;
          mNeedISforRealign = false;
        } else
          mNeedISforRealign = true;
      }
        
      // Invert the backlash for zigzag for an even X index (numbered from 1)
      mNeedBacklash = mNeedBacklash && !precooking;
      invertBacklash = mNeedBacklash && mDoZigzagStage && !(mPieceX % 2);
      if (invertBacklash) {
        mMoveInfo.backX *= -1.;
        mMoveInfo.backY *= -1.;
      }

      mScope->MoveStage(mMoveInfo, mNeedBacklash, precooking && mMoveInfo.speed > 0.);
      if (invertBacklash) {
        mMoveInfo.backX *= -1.;
        mMoveInfo.backY *= -1.;
      }
      SEMTrace('S', "DoNextPiece moving stage to %.3f %.3f%s", mMoveInfo.x, mMoveInfo.y,
        mNeedBacklash ? (invertBacklash ? " with inverted backlash" : " with backlash") :
        "");
      if (precooking)
        SEMTrace('M', "DoNextPiece Starting stage move to %.3f %.3f", mMoveInfo.x,
          mMoveInfo.y);
      else
        SEMTrace('M', "DoNextPiece Starting stage move");
      mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE, 0, timeOut);
      mMovingStage = true;
      return 0;
    
    } else if (mAction == CHECK_STAGE && moveStage && !precooking) {
      if (mDidBlockIS > 0) {
        mDidBlockIS = 0;
      } else {

        // For HQ use, set the extra delay time if this wasn't done in premove
        delay = 1000. * mParam->hqDelayTime;
        if (!mPreMovedStage && mParam->useHqParams &&
          delay > mShiftManager->GetStageMovedDelay())
          mShiftManager->SetStageDelayToUse(delay);

        mPreMovedStage = false;
        if (GetDebugOutput('S')) {
          mScope->FastStagePosition(stageX, stageY, sterr);
          gotStageXY = true;
          SEMTrace('S', "Piece %d %d %d at actual stage coords %.3f %.3f", mPieceX,
            mPieceY, mParam->zCurrent, stageX, stageY);
        }

        // If stage did not move far enough try a few times
        if (mMaxStageError > 0.) {
          if (!gotStageXY)
            mScope->FastStagePosition(stageX, stageY, sterr);
          gotStageXY = true;
          ix = TestStageError(stageX, stageY, sterr);
          if (ix > 0) {
            report.Format("Stage is %.2f microns from intended position; %s", sterr,
              mNumStageErrors >= 3 ? (mStopOnStageError ? "giving up and stopping" :
                "giving up and going on") : "trying again");
            mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
            if (mNumStageErrors < 3) {
              mMovingStage = true;
              mAction = MOVE_STAGE;
              mScope->MoveStage(mMoveInfo, mNeedBacklash);
              mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE, 0, timeOut);
              return 0;
            }
            if (mStopOnStageError) {
              mMovingStage = false;
              StopMontage();
              return 1;
            }
          }
        }
      }
      mNumStageErrors = 0;

    } else if (mAction == ACQUIRE_ANCHOR && mNeedColumnBacklash) {
      RealignNextTask(ANCHOR_ACQUIRE);
      return 0;

    } else if (mAction == ALIGN_WITH_IS && moveStage && mDoISrealign && 
      mAdjacentForIS[mPieceIndex] >= 0) {
      startsCol = PieceStartsColumn(mPieceIndex);
      if (startsCol > 0 || mISrealignInterval < 2 || (startsCol == 0 &&
        mNumDoneInColumn % mISrealignInterval == 0)) {

        // Set the image shift if it was not queued and was deferred above
        if (mNeedISforRealign || mFocusAfterStage) {
          if (SetImageShiftTestClip(adjISX, adjISY, alignISDelayFac))
            return 1;
        }
        mNeedISforRealign = false;
        if (!RealignToExistingPiece())
          return 0;
      }

    } else if (mAction == SET_IMAGE_SHIFT && (!moveStage || mImShiftInBlocks)) {

      // Do shift unconditionally because the predicted error may have changed
      ISX = mImShiftInBlocks ? mBlockISMoveInfo.x : mMoveInfo.x;
      ISY = mImShiftInBlocks ? mBlockISMoveInfo.y : mMoveInfo.y;
      mScope->SetImageShift(ISX, ISY);
      mShiftManager->SetISTimeOut(delayFactor * mShiftManager->GetLastISDelay());
      SEMTrace('S', "Piece %d, shifting to %.4f, %.4f with delay %.3f", mPieceIndex,
        ISX, ISY, MONTAGE_DELAY_FACTOR * mShiftManager->GetLastISDelay());
      mDidBlockIS = mImShiftInBlocks ? 1 : 0;

      // Change focus if called for
      if (mParam->adjustFocus && !mReadingMontage)
        mScope->SetDefocus(mBaseFocus + mFocusPitchX * iDelX + mFocusPitchY * iDelY);

    } else if (mAction == QUEUE_NEXT && mActPostExposure) {
      if (!moveStage || mImShiftInBlocks) {

        // Set up next image shift if allowed
        if (mImShiftInBlocks && ((nextPieceBlockCen && mFocusInBlocks) || 
          nextPiece >= mNumPieces)) {
          postISX = mBaseISX;
          postISY = mBaseISY;
        } else {
          ComputeMoveToPiece(nextPiece, false, iDelX, iDelY, adjISX, adjISY);
          postISX = mImShiftInBlocks ? mBlockISMoveInfo.x : mMoveInfo.x;
          postISY = mImShiftInBlocks ? mBlockISMoveInfo.y : mMoveInfo.y;
        }
        delay = (int)(1000. * delayFactor *
          mShiftManager->ComputeISDelay(postISX - ISX, postISY - ISY));
        mCamera->QueueImageShift(postISX, postISY, delay);
        SEMTrace('S', "Piece %d, queueing IS to %.4f, %.4f with delay %.3f",
          mPieceIndex, postISX, postISY, 0.001 * delay);
        mDidBlockIS = mImShiftInBlocks ? -1 : 0;

      }


      // Or Queue stage move if these conditions are met
      if (moveStage && (!mImShiftInBlocks || (mImShiftInBlocks && nextPieceBlockCen)) &&
        !(PieceNeedsRealigning(nextPiece, true) || 
        (nextPieceBlockCen && !mImShiftInBlocks)
          || (mNeedRealignToAnchor && mNextSeqInd >= mSecondHalfStartSeq))) {

        // First set the acquired position of the current piece
        if (mDoISrealign) {
          if (!gotStageXY)
            mScope->FastStagePosition(stageX, stageY, sterr);
          mScope->FastImageShift(ISX, ISY);
          SetAcquiredStagePos(mPieceIndex, stageX, stageY, ISX, ISY);
        }
        ComputeMoveToPiece(nextPiece, nextPieceBlockCen, iDelX, iDelY, adjISX, adjISY);
        doBacklash = nextPiece < mNumPieces && 
          mParam->yNframes > 1 && mPieceX != nextPiece / mParam->yNframes + 1;
        startsCol = PieceStartsColumn(nextPiece);
        if (mUsingAnchor && nextPiece < mNumPieces)
          doBacklash = startsCol != 0;
        if (mDoISrealign && !mFocusAfterStage && (startsCol > 0 || 
          mISrealignInterval < 2 || 
          (startsCol == 0 && (mNumDoneInColumn + 1) % mISrealignInterval == 0))) {
          delay = (int)(1000. * alignISDelayFac * 
            mShiftManager->ComputeISDelay(adjISX - ISX, adjISY - ISY)); 
          mCamera->QueueImageShift(adjISX, adjISY, delay);
          SEMTrace('S', "Queueing image shift for realign of %.3f, %.3f", adjISX, adjISY);
        }

        delay = mShiftManager->GetStageMovedDelay();
        if (mParam->useHqParams)
          delay = B3DMAX(delay, (int)(1000. * mParam->hqDelayTime));

        // Invert the backlash for NEXT column for an odd current X index
       invertBacklash = doBacklash && mDoZigzagStage && (mPieceX % 2);
        if (invertBacklash) {
          mMoveInfo.backX *= -1.;
          mMoveInfo.backY *= -1.;
        }
        mCamera->QueueStageMove(mMoveInfo, delay, doBacklash);
        if (invertBacklash) {
          mMoveInfo.backX *= -1.;
          mMoveInfo.backY *= -1.;
        }
        mPreMovedStage = true;
        SEMTrace('S', "Piece %d, queueing stage move to %.3f, %.3f%s", 
          mPieceIndex, mMoveInfo.x, mMoveInfo.y, doBacklash ? 
          (invertBacklash ? " with inverted backlash" : " with backlash") : "");
      }

    } else if (mAction == ACQUIRE_PIECE && !precooking) {
      if (mCamera->CameraBusy())
        timeOut += 60000;
      SEMTrace('M', "DoNextPiece Starting capture%s", B3DCHOICE(mPreMovedStage,  
        mDidBlockIS ? " with image shift" : " with stage move", ""));
      if (mWinApp->GetSTEMMode() && !(mMacroToRun > 0 && mMacroToRun <= MAX_MACROS))
        mCamera->SetMaxChannelsToGet(1);
      if (mParam->useHqParams && mParam->skipRecReblanks && !mUseContinuousMode)
        mCamera->SetSkipNextReblank(true);
      if (mDidBlockIS < 0)
        mDidBlockIS = 1;
      if (!mUseContinuousMode || !mCamera->DoingContinuousAcquire()) {
        if (mUseContinuousMode) {
          mCamera->SetContinuousDelayFrac(mParam->continDelayFactor);
          mCamera->SetNumDropAtContinStart(mNumDropAtContinStart);
        }
        mCamera->InitiateCapture(MONTAGE_CONSET);
      }
      if (mUseContinuousMode) {
        if (mNumContinuousAlign > 1)
          mCamera->AlignContinuousFrames(mNumContinuousAlign, false);
        mCamera->SetTaskWaitingForFrame(true);
        mWinApp->AddIdleTask(CCameraController::TaskGettingFrame, TASK_MONTAGE, 0, 0);
      } else {
        mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_MONTAGE, 0, 0);
      }
      return 0;

    } else if (mAction == CHECK_ACQUIRE && !precooking) {

      if (mTrialMontage)
        mImBufs->mCaptured = BUFFER_MONTAGE_PRESCAN;

      /* //For testing what happens with a dim image 
      if (mWinApp->mStoreMRC->getDepth() > 4) {
      KImage *image = mImBufs->mImage;
      int nx = image->getWidth();
      int ny = image->getHeight();
      image->Lock();

      short int *data = (short int *)image->getData();
      for (int i = 0; i < nx * ny; i++)
      data[i] /= 100;
      image->UnLock();
      } */
      

      // If a required level is registered, test the image scale against it
      // and fail if more than half of the pieces are too dim
      if (mRequiredBWMean > 0. && 
        mWinApp->mProcessImage->ImageTooWeak(mImBufs, mRequiredBWMean)) {
        mNumTooDim++;
        if (mNumTooDim > (mNumPieces - mNumToSkip) / 2) {
          StopMontage();  
          return 1;
        }
      }

      // Now test against the user limit for piece too dim
      image = mImBufs[0].mImage;
      type = image->getType();
      if (!mTrialMontage && mCountLimit > 0) {
        image->Lock();
        data = image->getData();
        denmin = (float)(mCountLimit + 1);
        ProcSampleMeanSD(data, type, image->getWidth(), image->getHeight(), &denmin, &sDmin);
        if (denmin < mCountLimit) {
          report.Format("Mean of image is %.0f, below the limit of %d", denmin, mCountLimit);
          mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
          if (mShootFilmIfDark && !mShotFilm) {
            mCamera->RetractAllCameras();
            Sleep(10000);
            mScope->SetManualExposure(1.);
            mScope->TakeFilmExposure(false, 0., 0., false);
            mShotFilm = true;
            mShootingFilm = true;
            mAction = ACQUIRE_PIECE - 1;
            mWinApp->AddIdleTask(CEMscope::TaskFilmBusy, TASK_MONTAGE, 0, 100000);
            mWinApp->AppendToLog("Taking film exposure to try to unstick the shutter", 
              LOG_OPEN_IF_CLOSED);
            return 0;
          }
      
          if (mShootFilmIfDark) 
            mScope->SetColumnValvesOpen(false);
          report.Format("The mean counts of this image are below your minimum value.\n\n"
            "Press:\n"
            "\"Retry\" to try again with this piece,\n\n\"Save && Go On\" to save this"
            " image and go on,\n\n\"Cancel\" to stop the montage.%s", 
            mShootFilmIfDark ? "\n\nBe sure to turn the beam on again first!" : "");
          ix = SEMThreeChoiceBox(report, "Retry", "Save && Go On", "Cancel", 
            MB_YESNOCANCEL | MB_ICONQUESTION);
          if (ix != IDNO) {
            if (ix == IDYES) {
              mAction = SET_IMAGE_SHIFT - 1;
              if (moveStage && mPreMovedStage) {
                mPreMovedStage = false;
                mNeedBacklash = true;
                mAction = MOVE_STAGE - 1;
              }
            } else {
              StopMontage();
              return 1;
            }
          }
        }
      }
      mShotFilm = false;

      // Now test for duplicate image from Eagle camera
      if (mDuplicateRetryLimit >= 0 && mCamParams[mWinApp->GetCurrentCamera()].FEItype) {
        if (mImBufs->IsImageDuplicate(&mImBufs[1])) {

          // If count is less than limit, retry after some delay; otherwise ask what to do
          if (mDuplicateRetryCount < mDuplicateRetryLimit) {
            mWinApp->AppendToLog("Duplicate image received from Eagle camera - retrying");
            mDuplicateRetryCount++;
            mShiftManager->SetISTimeOut(10.);
            mShiftManager->SetLastTimeoutWasIS(false);
            mAction = SET_IMAGE_SHIFT - 1;
            if (moveStage && mPreMovedStage) {
              mPreMovedStage = false;
              mNeedBacklash = true;
              mAction = MOVE_STAGE - 1;
            }
          } else {
            report.Format("A duplicate image was received from the Eagle camera after %d "
              "retries.\n\nPress:\n\"Save && Go On\" to save the image and go on,\n\n"
              "\"Stop\" to stop the montage.", mDuplicateRetryCount);
            if (SEMThreeChoiceBox(report, "Save && Go On", "Stop", "",
              MB_YESNO | MB_ICONQUESTION, 1, false, IDYES) == IDNO) {
              mWinApp->mCameraMacroTools.SetUserStop(true);
              mWinApp->ErrorOccurred(0);
              mWinApp->mCameraMacroTools.SetUserStop(false);
              return 1;
            }

            // If going on, reset the email sent on error flag in TSC
            if (mWinApp->DoingTiltSeries())
              mWinApp->mTSController->SetErrorEmailSent(false);
            mDuplicateRetryCount = 0;
          }
        } else
          mDuplicateRetryCount = 0;
      }
        
    } else if (mAction == DWELL_TO_COOK && precooking) {
      mDwellStartTime = GetTickCount();
      mWinApp->AddIdleTask(TASK_MONTAGE_DWELL, 0, 1000 + 3 * mDwellTimeMsec);
      return 0;

    } else if (mAction == RUN_MACRO && mMacroToRun > 0 && mMacroToRun <= MAX_MACROS &&
      !precooking) {
      mRunningMacro = true;
      mWinApp->mMacroProcessor->Run(mMacroToRun - 1);
      mWinApp->AddIdleTask(TASK_MONT_MACRO, 0, 0);
      return 0;

    } else if (mAction == SAVE_PIECE) {

      // Now we are safely done with going back to getting piece, so adjust some flags
      mNumSinceRealign++;
      mNumDoneInColumn++;
      if (PieceStartsColumn(mPieceIndex))
        mNumDoneInColumn = 1;

      if (!precooking) {
        SavePiece();
      } else {
        SetNextPieceIndexXY();
        if (mPieceX > mParam->xNframes) {
          mMoveInfo.x = mBaseStageX;
          mMoveInfo.y = mBaseStageY;
          StartStageRestore();
          mLastFailed = false;
          CleanPatches();
        }          
      }
      if (mPieceIndex < 0 || mRestoringStage)
        return 0;
    }

    // Increment action at end of loop
    mAction = (mAction + 1) % mNumActions;
  }

  // There is no escape from that loop so it's an error to get here!
  return 1;
}

int EMmontageController::CookingDwellBusy(void)
{
  return SEMTickInterval(mDwellStartTime) < mDwellTimeMsec ? 1 : 0;
}

// SAVE THE PIECE IMAGE AND PROCESS FOR OVERLAPS AND TO COMPOSE OVERVIEW
int EMmontageController::SavePiece()
{
  KImage *image;
  void *data;
  int type, lower, idir, ixy, i, nVar, upper, nSum, cenType, sectInd, dataSize, adocInd;
  float xPeak, yPeak, lowMean, highMean, midFrac, rangeFrac;
  CString report;
  ScaleMat camToSpec, specToCam;
  int centerTaper = 12;
  int numIter = 4 + (mXCorrBinning > 1 ? 1 : 0); // Search here is on binned data, do more
  int limStep = 10;
  const int debugLen = 100 * MONTXC_MAX_DEBUG_LINE;
  char debugStr[debugLen];
  int isave, startX, startY, endX, endY, iy, iny, outy, indin, indout, ix;
  int iDirX, iDirY, shiftX, shiftY, debugLevel;
  float angle, adjBaseX, adjBaseY, specX, specY;
  float *centerFloat = (float *)mCenterData;
  unsigned short int *uCenter = (unsigned short int *)mCenterData;
  double aMaxCos;
  EMimageExtra *extra0 = NULL, *extra1;
  BOOL doingTasks;

  debugLevel = GetDebugOutput('a') ? 2 : 0;
  mImBufs->GetTiltAngle(angle);
  image = mImBufs[0].mImage;
  type = image->getType();
  dataSizeForMode(type, &dataSize, &i);

  // 5/27/25: Removed debug output on piece min/max/mean
   if (!BOOL_EQUIV(mExpectingFloats, dataSize == 4)) {
    report.Format("SavePiece in montaging got a %s image when expecting %s",
      mExpectingFloats ? "integer" : "float", mExpectingFloats ? "floats" : "integers");
    SEMMessageBox(report);
    StopMontage();
    return 1;
  }

  // Save the image
  if (!mReadingMontage && !mTrialMontage) {
    extra0 = mImBufs->mImage->GetUserData();
    if (mImShiftInBlocks) {
      if (extra0) {
        extra0->mNominalStageX = mNominalStageX;
        extra0->mNominalStageY = mNominalStageY;
      }
    }
    isave = mPieceSavedAt[mPieceIndex];
    if (isave < 0) {
      if (mBufferManager->CheckAsyncSaving())
        return 1;
      isave = mWinApp->mStoreMRC->getDepth();
    }

    // First image: save the spacing to global
    if (!mWinApp->mStoreMRC->getDepth()) {
      adocInd = mWinApp->mStoreMRC->GetAdocIndex();
      if (adocInd >= 0 && !AdocGetMutexSetCurrent(adocInd)) {
        AdocSetTwoIntegers(ADOC_GLOBAL, 0, ADOC_PSPACE, mParam->xFrame - mParam->xOverlap,
          mParam->yFrame - mParam->yOverlap);
        AdocReleaseMutex();
      }
    }

    SEMTrace('M', "SaveImage Saving image at %d %d,  file Z = %d", mPieceX, mPieceY,
      isave);
    if (mPieceSavedAt[mPieceIndex] < 0) {
      ix = mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC);
      if (!ix)
        mPieceSavedAt[mPieceIndex] = isave;
    } else {
      ix = mBufferManager->OverwriteImage(mWinApp->mStoreMRC, mPieceSavedAt[mPieceIndex]);
    }
    if (ix) {
      StopMontage();
      return 1;
    }
    mLastSavedAtZ = isave;

    // Maintain Zmax every time a piece is saved
    if (mParam->zCurrent > mParam->zMax)
      mParam->zMax = mParam->zCurrent;

    // Mark that mag and binning parameters were used
    mParam->settingsUsed = true;

    // Get and store percentile patch statistics if requested; error are non-fatal
    if (mPctlPatchSize > 0) {
      if (mWinApp->mProcessImage->PatchPercentileStats(mImBufs, mLowPercentile,
        mHighPercentile, mPctlMidCrit, mPctlRangeCrit, mPctlPatchSize, lowMean, highMean,
        midFrac, rangeFrac, report)) {
        SEMAppendToLog("WARNING: Error getting patch statistics: " + report);
      } else {
        ix = StorePercentileStats(mPieceIndex, highMean, midFrac, rangeFrac);
        if (ix)
          PrintfToLog("WARNING: Error %d saving patch statistics in mdoc", ix);
      }
    }

    // Get actual stage X,Y adjusting for image shift; 
    // get stage offset and add to autodoc if they are being used
    extra0 = (EMimageExtra *)mImBufs[0].mImage->GetUserData();
    SetAcquiredStagePos(mPieceIndex, extra0->mStageX, extra0->mStageY, extra0->mISX,
      extra0->mISY);
    if (mHaveStageOffsets) {
      ix = mParam->binning * (mMontageX[mPieceIndex] - mMontCenterX);
      iy = mParam->binning * (mMontageY[mPieceIndex] - mMontCenterY);
      mStageOffsetX[mPieceIndex] = mAcquiredStageX[mPieceIndex] -
        (float)(mBinv.xpx * ix + mBinv.xpy * iy + mBaseStageX);
      mStageOffsetY[mPieceIndex] = mAcquiredStageY[mPieceIndex] -
        (float)(mBinv.ypx * ix + mBinv.ypy * iy + mBaseStageY);
      AutodocStageOffsetIO(true, mPieceIndex);
    }
  }
    
  image->Lock();
  data = image->getData();
  mActualErrorX[mPieceIndex]  = 0.;
  mActualErrorY[mPieceIndex]  = 0.;
  nSum = 0;

  // Compute the expected shift for multishot
  if (mUsingMultishot) {
    camToSpec = mShiftManager->CameraToSpecimen(mParam->magIndex);
    for (int ixy = 0; ixy < 2; ixy++) {
      lower = LowerIndex(mPieceIndex, ixy);
      idir = 0;
      if (lower >= 0) {
        idir = 1;
        upper = mPieceIndex;
      } 
      xPeak = yPeak = 0.;
      if (idir && camToSpec.xpx) {
        specToCam = MatInv(camToSpec);
        ix = mParam->binning * (mMontageX[upper] - mMontageX[lower]);
        iy = mParam->binning * (mMontageY[upper] - mMontageY[lower]);
        ApplyScaleMatrix(camToSpec, (float)ix, (float)iy, specX, specY);
        ApplyScaleMatrix(specToCam, specX, specY * cos(DTOR *angle), xPeak,
          yPeak);

        // This is the amount that the image area moved and thus the amount to shift
        // into alignment.  We need amount upper edge IS displaced from alignment so
        // take the negative.  Make yPeak negative to use below
        xPeak = (float)((xPeak - ix) / mParam->binning);
        yPeak = (float)((iy - yPeak) / mParam->binning);
        mActualErrorX[mPieceIndex] += mActualErrorX[lower] - idir * xPeak;
        mActualErrorY[mPieceIndex] += mActualErrorY[lower] + idir * yPeak;
        nSum++;
        mAdjustedOverlaps[ixy] = mParam->yOverlap + B3DNINT(yPeak);
        mExpectedShiftsX[ixy] = idir * xPeak;
        mExpectedShiftsY[ixy] = -idir * yPeak;
      }
      if (idir) {
        mUpperShiftX[2 * lower + ixy] = -xPeak;
        mUpperShiftY[2 * lower + ixy] = yPeak;
      }
    }

  } else if (mDoCorrelations) {
    SEMTrace('M', "SaveImage Processing");

    if (mXCorrThread) {
      if (WaitForXCorrProc(20000)) {
        SEMMessageBox("Piece correlation thread timed out");
        StopMontage();
        return 1;
      }
      ProcessXCorrResult();
      nSum = AddToActualErrors();
      MaintainErrorSums(nSum, mXCTD.pieceIndex);
    }

    // Extract the patches in the edges and correlate whatever is 
    // available to get displacements from neighboring pieces
    for (int ixy = 0; ixy < 2; ixy++) {
      double wallstart = wallTime();
      // get patch in upper edge if there is another piece above this one; it is a lower
      mXCTD.upper[ixy] = UpperIndex(mPieceIndex, ixy);
      if (mXCTD.upper[ixy] >= 0) {
        if (mAlreadyHaveShifts) {

          // If there are already shifts, need to set this non-NULL to use tests below
          mLowerPatch[2 * mPieceIndex + ixy] = &mBefErrMax;
        } else {
          extractPatch(data, type, ixy, &mLowerInd0[ixy][0], &mLowerInd1[ixy][0],
            &mLowerPatch[2 * mPieceIndex + ixy]);
        }
      }

      // Get patch in lower edge if it is needed
      mXCTD.lower[ixy] = LowerIndex(mPieceIndex, ixy);
      if (mXCTD.lower[ixy] >= 0) {
        if (mAlreadyHaveShifts) {
          mUpperPatch[2 * mPieceIndex + ixy] = &mBefErrMax;
        } else {
          extractPatch(data, type, ixy, &mUpperInd0[ixy][0], &mUpperInd1[ixy][0],
            &mUpperPatch[2 * mPieceIndex + ixy]);
        }
      }

      // Correlate with the patch from the lower piece if it exists, or with upper
      mXCTD.idir[ixy] = 0;
      if (mXCTD.lower[ixy] >= 0 && mUpperPatch[2 * mPieceIndex + ixy] &&
        mLowerPatch[2 * mXCTD.lower[ixy] + ixy]) {
        mXCTD.idir[ixy] = 1;
        mXCTD.upper[ixy] = mPieceIndex;
      } else if (mXCTD.upper[ixy] >= 0 && mUpperPatch[2 * mXCTD.upper[ixy] + ixy] &&
        mLowerPatch[2 * mPieceIndex + ixy]) {
        mXCTD.idir[ixy] = -1;
        mXCTD.lower[ixy] = mPieceIndex;
      }

      // Clean these up right away, crashes occurred when nulled in a clause below
      if (mAlreadyHaveShifts && mXCTD.upper[ixy] >= 0)
        mLowerPatch[2 * mPieceIndex + ixy] = NULL;
      if (mAlreadyHaveShifts && mXCTD.lower[ixy] >= 0)
        mUpperPatch[2 * mPieceIndex + ixy] = NULL;
    }

    if ((mXCTD.idir[0] || mXCTD.idir[1]) && !mAlreadyHaveShifts) {

      // Load the thread data structure with all the values needed
      for (ixy = 0; ixy < 2; ixy++) {
        if (mXCTD.idir[ixy]) {
          mXCTD.lowerPatch[ixy] = mLowerPatch[2 * mXCTD.lower[ixy] + ixy];
          mXCTD.upperPatch[ixy] = mUpperPatch[2 * mXCTD.upper[ixy] + ixy];
          mXCTD.XYbox[ixy] = &mXYbox[ixy][0];
          mXCTD.numExtra[ixy] = &mNumExtra[ixy][0];
          mXCTD.CTFp[ixy] = mCTFp[ixy];
        }
      }
      mXCTD.XYpieceSize = &mXYpieceSize[0];
      mXCTD.XYoverlap = &mXYoverlap[0];
      mXCTD.Xsmooth = &mXsmooth[0];
      mXCTD.Ysmooth = &mYsmooth[0];
      mXCTD.Xpad = &mXpad[0];
      mXCTD.Ypad = &mYpad[0];
      mXCTD.lowerPad = mLowerPad;
      mXCTD.upperPad = mUpperPad;
      mXCTD.lowerCopy = mLowerCopy;
      mXCTD.numXcorrPeaks = mNumXcorrPeaks;
      mXCTD.delta = &mDelta[0];
      mXCTD.XCorrBinning = mXCorrBinning;
      mXCTD.maxLongShift = &mMaxLongShift[0];
      mXCTD.debugStr = debugStr;
      mXCTD.debugLen = debugLen;
      mXCTD.debugLevel = debugLevel;
      mXCTD.pieceIndex = mPieceIndex;

      if (debugLevel > 1) {
        mWinApp->mMacroProcessor->SetNonMacroDeferLog(true);
      }
      if (mMiniBorderY && !mNoMontXCorrThread && 
        !(mParam->correctDrift && !mDoStageMoves && !mUsingMultishot &&
        mMagTab[mParam->magIndex].calibrated[mWinApp->GetCurrentCamera()]) && !debugLevel
        && NextPieceIndex() / mParam->yNframes < mParam->xNframes) {

        // Start thread if deferred tiling, no drift correction, not debugging and not the
        // last frame
        mXCorrThread = AfxBeginThread(XCorrProc, &mXCTD, THREAD_PRIORITY_BELOW_NORMAL, 0,
          CREATE_SUSPENDED);
        mXCorrThread->m_bAutoDelete = false;
        mXCorrThread->ResumeThread();

      } else {

        // Otherwise run directly and process
        XCorrProc(&mXCTD);
        if (debugLevel > 1) {
          mWinApp->mMacroProcessor->SetNonMacroDeferLog(false);
          if (mWinApp->mLogWindow)
            mWinApp->mLogWindow->FlushDeferredLines();
        }
        ProcessXCorrResult();
      }
    }

    // If already have shifts, set the peak values for use below
    if (mAlreadyHaveShifts) {
      for (ixy = 0; ixy < 2; ixy++) {
        if (mXCTD.idir[ixy]) {
          mXCTD.xPeak[ixy] = -mUpperShiftX[2 * mXCTD.lower[ixy] + ixy];
          mXCTD.yPeak[ixy] = mUpperShiftY[2 * mXCTD.lower[ixy] + ixy];
        }
      }
    }

    if (!mXCorrThread)
      nSum = AddToActualErrors();
  }

  if (!mXCorrThread)
    MaintainErrorSums(nSum, mPieceIndex);

  // Convert the buffer to bytes now if required and it is not
  if (mMiniData && mConvertMini && type != kUBYTE) {
    image->UnLock();
    if (!mImBufs[0].mPixMap)
      mImBufs[0].mPixMap = new KPixMap();
    if (!mImBufs[0].ConvertToByte(mParam->byteMinScale, mParam->byteMaxScale)) {
      SEMMessageBox("Error converting piece image to bytes");
      StopMontage();
      return 1;
    }
    image = mImBufs[0].mImage;
    type = image->getType();
    if (type == kUBYTE)
      dataSize = 1;
    image->Lock();
    data = image->getData();
  }

  // Add image to mini view
  if (mMiniData) {
    short int *sShort = (short int *)data;
    unsigned short int *uShort = (unsigned short int *)data;
    unsigned char *byte = (unsigned char *)data;
    float *fData = (float *)data;
    int outx, subZoomX = 0, subZoomY = 0, xtrim, ytrim, keepByte = 0;
    int xstrt = 0, xend = mMiniFrameX, ystrt = 0, yend = mMiniFrameY;
    int xofset = 0, yofset = 0;
    int miniBaseX = (mPieceX - 1) * mMiniDeltaX;
    int miniBaseY = (mParam->yNframes - mPieceY) * mMiniDeltaY;
    bool needSample = image->getRowBytes() % 2 && type != kUBYTE;

    // First time in, get mean of this image and fill the whole array
    if (mNeedToFillMini && (mPcIndForFillVal < 0 || mPcIndForFillVal == mPieceIndex)) {
      mMiniFillVal = ProcImageMean(data, type, mParam->xFrame, mParam->yFrame, 0,
        mParam->xFrame - 1, 0, mParam->yFrame - 1);
      if (!mMiniBorderY) {
        if (mConvertMini) {
          for (outx = 0; outx < mMiniSizeX * mMiniSizeY; outx++)
            mMiniByte[outx] = (unsigned char)mMiniFillVal;
        } else if (mExpectingFloats) {
          for (outx = 0; outx < mMiniSizeX * mMiniSizeY; outx++)
            mMiniFloat[outx] = mMiniFillVal;
        } else if (type == kUSHORT) {
          for (outx = 0; outx < mMiniSizeX * mMiniSizeY; outx++)
            mMiniUshort[outx] = (unsigned short)mMiniFillVal;
        } else {
          for (outx = 0; outx < mMiniSizeX * mMiniSizeY; outx++)
            mMiniData[outx] = (short)mMiniFillVal;
        }
      }
      mNeedToFillMini = false;

      // Fill center now that this mean is known
      if (mNeedToFillCenter) {
        if (mExpectingFloats) {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            centerFloat[i] = mMiniFillVal;
        } else if (type == kUSHORT) {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            uCenter[i] = (unsigned short)mMiniFillVal;
        } else {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            mCenterData[i] = (short)mMiniFillVal;
        }
        mNeedToFillCenter = false;
      }
    }

    // If doing deferred tiling, adjust the bases
    if (mMiniBorderY) {
      subZoomX = (mParam->xFrame % mMiniZoom) / 2;
      subZoomY = (mParam->yFrame % mMiniZoom) / 2;
      miniBaseX = (mPieceX - 1) * mMiniFrameX;
      miniBaseY = (mParam->yNframes - mPieceY) * mMiniFrameY + mMiniBorderY;
    } else {

      // Otherwise, proceed as usual for placing images directly into the array
      // Trim edges of images in overlap regions
      xtrim = mParam->xOverlap / (10 * mMiniZoom);
      ytrim = mParam->yOverlap / (10 * mMiniZoom);
      if (mPieceX > 1)
        xstrt = xtrim;
      if (mPieceX < mParam->xNframes)
        xend -= xtrim;
      if (mPieceY > 1)
        yend -= ytrim;
      if (mPieceY < mParam->yNframes)
        ystrt = ytrim;


      if (mParam->shiftInOverview) {

        // If shifting, define limits and offsets for copying the data
        // The error terms were displacements so we need the negative to get back
        // to the amount to move the piece; but then we negate Y again for operating
        // in upside-down image
        // And if we already have piece shifts, the offsets are the +X and -Y shifts
        xofset = -B3DNINT(mActualErrorX[mPieceIndex] / mMiniZoom);
        yofset = B3DNINT(mActualErrorY[mPieceIndex] / mMiniZoom);
        if (mAlreadyHaveShifts) {
          xofset = B3DNINT(mBmat[2 * mPieceToVar[mPieceIndex]] / mMiniZoom);
          yofset = -B3DNINT(mBmat[2 * mPieceToVar[mPieceIndex] + 1] / mMiniZoom);
        }
        miniBaseX += xofset;
        miniBaseY += yofset;
        mMiniOffsets.offsetX[mPieceIndex] = xofset;
        mMiniOffsets.offsetY[mPieceIndex] = yofset;

        // If a base is negative, limit the start of the copy
        // If a base is beyond the limit for the composed montage, limit end of copy
        if (miniBaseX < 0)
          xstrt = -miniBaseX;
        if (miniBaseX > mMiniSizeX - mMiniFrameX)
          xend = mMiniSizeX - miniBaseX;
        if (miniBaseY < 0)
          ystrt = -miniBaseY;
        if (miniBaseY > mMiniSizeY - mMiniFrameY)
          yend = mMiniSizeY - miniBaseY;
      }
    }

    // Bin if possible
    if (!needSample && xstrt < xend && ystrt < yend) {
      if (type == kUBYTE)
        keepByte = mConvertMini ? 1 : -1;
      outx = extractAndBinIntoArray(data, type, 
        image->getRowBytes() / dataSize, 
        xstrt * mMiniZoom + subZoomX, xend * mMiniZoom + subZoomX - 1,
        ystrt * mMiniZoom + subZoomY, yend * mMiniZoom + subZoomY - 1, mMiniZoom, 
        mMiniData, mMiniArrayX, miniBaseX + xstrt, miniBaseY + ystrt, keepByte, &outx, 
        &outy);
      if (outx) {
        needSample = true;
        PrintfToLog("WARNING: Error %d from call to bin into overview array", outx);
      }
    }

    // Or copy the subsampled image into place
    if (needSample && xstrt < xend && ystrt < yend) {
      for (int outy = ystrt; outy < yend; outy++) {
        size_t indout = (miniBaseY + (size_t)outy) * mMiniArrayX + miniBaseX + xstrt;
        int indin = xstrt * mMiniZoom + subZoomX;
        switch (image->getType()) {
        case kSHORT:
          sShort = (short *)image->getRowData(outy * mMiniZoom + subZoomY);
          for (outx = xstrt; outx < xend; outx++) {
            mMiniData[indout++] = sShort[indin];
            indin += mMiniZoom;
          }
          break;

        case kUSHORT:
          uShort = (unsigned short *)image->getRowData(outy * mMiniZoom + subZoomY);
          for (outx = xstrt; outx < xend; outx++) {
            mMiniUshort[indout++] = uShort[indin];
            indin += mMiniZoom;
          }
          break;

        case kFLOAT:
          fData = (float *)image->getRowData(outy * mMiniZoom + subZoomY);
          for (outx = xstrt; outx < xend; outx++) {
            mMiniFloat[indout++] = fData[indin];
            indin += mMiniZoom;
          }
          break;

        case kUBYTE:
          byte = (unsigned char *)image->getRowData(outy * mMiniZoom + subZoomY);
          if (mConvertMini) 
            for (outx = xstrt; outx < xend; outx++) {
              mMiniByte[indout++] = byte[indin];
              indin += mMiniZoom;
            }
          else
            for (outx = xstrt; outx < xend; outx++) {
              mMiniData[indout++] = byte[indin];
              indin += mMiniZoom;
            }
            break;
        }
      }
    }
  }
  image->UnLock();
  cenType = image->getType();

  // Save center pieces
  if (mPieceX - 1 >= mCenterIndX1 && mPieceX - 1 <= mCenterIndX2 &&
    mPieceY - 1 >= mCenterIndY1 && mPieceY - 1 <= mCenterIndY2) {
      mCenterCoordX[mNumCenterSaved] = mMontageX[mPieceIndex];
      mCenterCoordY[mNumCenterSaved] = mMontageY[mPieceIndex];
      mCenterPiece[mNumCenterSaved] = mPieceIndex;
      mCenterImage[mNumCenterSaved] = new KImage(image);
      mCenterShift[mNumCenterSaved][0] = 0;
      mCenterShift[mNumCenterSaved][1] = 0;
      mNumCenterSaved++;
      if (mNeedToFillCenter) {

        // Fill center in case there was no minidata
        mMiniFillVal = ProcImageMean(data, type, mParam->xFrame, mParam->yFrame, 0,
          mParam->xFrame - 1, 0, mParam->yFrame - 1);
        if (mExpectingFloats) {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            centerFloat[i] = mMiniFillVal;
        } else if (type == kUSHORT) {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            uCenter[i] = (unsigned short)mMiniFillVal;
        } else {
          for (i = 0; i < mParam->xFrame * mParam->yFrame; i++)
            mCenterData[i] = (short)mMiniFillVal;
        }
        mNeedToFillCenter = false;
      }
  }

  // Increment piece position
  SetNextPieceIndexXY();
  if (!mReadingMontage && mAutosaveLog)
    mWinApp->mLogWindow->UpdateSaveFile(true, mWinApp->mStoreMRC->getName());
  if (mPieceX <= mParam->xNframes)
    return 0;

  // IF FINISHED

  if (!mReadingMontage) {
    if (mUseContinuousMode) {
      mCamera->StopCapture(-1);
      mCamera->ChangePreventToggle(-1);
    }

    // Restore shifts to center plus any accumulated error 
    int iDelX = mDoISrealign ? 0 : (mParam->binning * mPredictedErrorX);
    int iDelY = mDoISrealign ? 0 : (mParam->binning * mPredictedErrorY);
    if (mDoStageMoves) {
      mMoveInfo.x = mBaseStageX + mBinv.xpx * iDelX + mBinv.xpy * iDelY;
      mMoveInfo.y = mBaseStageY + mBinv.ypx * iDelX + mBinv.ypy * iDelY;
      StartStageRestore();
      if (mUsingAnchor)
          UtilRemoveFile(mAnchorFilename);

    } 
    
    if (mUsingImageShift && !mUsingMultishot) {
      if (mImShiftInBlocks)
        mScope->SetImageShift(mBaseISX, mBaseISY);
      else
        mScope->SetImageShift(mBaseISX + mBinv.xpx * iDelX + mBinv.xpy * iDelY,
          mBaseISY + mBinv.ypx * iDelX + mBinv.ypy * iDelY);
      mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());

      if (mParam->adjustFocus)
        mScope->SetDefocus(mBaseFocus);
    }
  }


  nVar = mNumPieces - mNumToSkip;
  if (nVar > 1 && (mDoCorrelations || mUsingMultishot)) {

    // Compute the best shift for the pieces
    float aMeanFirst, aMaxFirst, wMeanFirst, wMaxFirst;
    FloatVec bMat1;
    bMat1.resize(2 * mNumPieces);
    float *upperShiftX = &mUpperShiftX[0];
    float *upperShiftY = &mUpperShiftY[0];
    if (!mAlreadyHaveShifts) {
      if (mUsingMultishot)
        isave = 0;
      else
        isave = FindBestShifts(nVar, &mUpperFirstX[0], &mUpperFirstY[0], &bMat1[0],
          mBefErrMean, mBefErrMax, aMeanFirst, aMaxFirst, wMeanFirst, wMaxFirst, 
          mNumXcorrPeaks > 1 && nVar > 3);

      if (!isave) 
        isave = FindBestShifts(nVar, &mUpperShiftX[0], &mUpperShiftY[0], &mBmat[0], 
          mBefErrMean, mBefErrMax, mAftErrMean, mAftErrMax, mWgtErrMean, mWgtErrMax, 
          false);
    }

    if (mAlreadyHaveShifts || !isave) {

      // Take the method that makes the product of mean and max be a minimum
      if (!mAlreadyHaveShifts && !mUsingMultishot && 
        mWgtErrMax * mWgtErrMean > wMaxFirst * wMeanFirst) {
        mAftErrMax = aMaxFirst;
        mAftErrMean = aMeanFirst;
        mWgtErrMax = wMaxFirst;
        mWgtErrMean = wMeanFirst;
        upperShiftX = &mUpperFirstX[0];
        upperShiftY = &mUpperFirstY[0];
        for (i = 0; i < 2 * nVar; i++)
          mBmat[i] = bMat1[i];
      }

      // Save aligned piece coordinates for external use
      if (!mAlreadyHaveShifts) {
        i = StoreAlignedCoordsInAdoc();
        if (i > 0)
          PrintfToLog("WARNING: Error %d saving aligned piece coordinates into autodoc", 
          i);
      }

      // Save the center shifts
      for (isave = 0; isave < mNumCenterSaved; isave++)
        for (ixy = 0; ixy < 2; ixy++)
          mCenterShift[isave][ixy] = 
          (int)floor(mBmat[ixy + 2 * mPieceToVar[mCenterPiece[isave]]] + 0.5);

      // Derate the maximum error by the cos of the tilt angle for tests and messages
      aMaxCos = mWgtErrMax * pow(cos((double)(DTOR * angle)), 2.);
      mWinApp->mMacroProcessor->SetMontageError(aMaxCos);     
      int mess = 0;
      if (aMaxCos > 1.)
        mess = 1;
      if (aMaxCos > 2.)
        mess = 2;
      if (aMaxCos > 4.0)
        mess = 3;
      if (aMaxCos > 12.0)
        mess = 4;

      CString strZ = "";
      CString strRtn = "\r\n";
      CString badMess = "";
      if ((!mReadingMontage || mWinApp->GetAdministrator()) && 
        !CMultiShotDlg::DoingAutoStepAdj()) {
        int logWhere = LOG_MESSAGE_IF_CLOSED;
        if (mWinApp->mMacroProcessor->DoingMacro() || mWinApp->DoingTiltSeries() ||
          (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring())) {
          strZ.Format("Z = %d: ", mParam->zCurrent);
          logWhere = LOG_OPEN_IF_CLOSED;
          strRtn = " ";
        }
        if (mUsingMultishot) {
          report = "Pieces are being shifted by their expected displacements";
        } else if (mParam->xNframes > 1 && mParam->yNframes > 1) {
          report.Format("The original displacements in the overlap zones have%s"
            "mean   %.2f      maximum    %.2f    pixels.\r\n"
            "After shifting pieces into register, the displacements "
            "have%smean   %.2f      maximum    %.2f    pixels.",
            //"SD adjusted %.2f  %.2f\r\r%s",
            strRtn, mBefErrMean, mBefErrMax, strRtn, mWgtErrMean, mWgtErrMax);
          if (!mDoStageMoves)
            badMess.Format("\r\n%s\r\n", montMessage[mess]);
        } else {
          report.Format("The original displacements in the overlap zones have%s"
            "mean   %.2f      maximum    %.2f    pixels.\r\n"
            "The pieces can be shifted so that displacements are zero.",
            strRtn, mBefErrMean, mBefErrMax);
        }
        report = strZ + report + badMess;
        mWinApp->AppendToLog(report, logWhere);
      }

      // Save best shifts into autodoc if there is one.  Async file saving error will
      // show up here, so need to stop if that occurs
      if (!mAlreadyHaveShifts && AutodocShiftStorage(true, upperShiftX, upperShiftY) > 0){
        StopMontage();
        return 1;
      }
    }
  }

  // Adjust and write the stage offsets
  adjBaseX = 0.;
  adjBaseY = 0.;
  nSum = 0;
  if (mHaveStageOffsets && !mReadingMontage) {
    for (i = 0; i < mNumPieces; i++) {
      if (mPieceSavedAt[i] >= 0) {
        adjBaseX += mStageOffsetX[i];
        adjBaseY += mStageOffsetY[i];
        nSum++;
      }
    }
    if (nSum) {
      adjBaseX /= nSum;
      adjBaseY /= nSum;
    }

    // Both the base stage position and all the offsets get their mean subtracted off
    for (i = 0; i < mNumPieces; i++) {
      if (mPieceSavedAt[i] >= 0) {
        mStageOffsetX[i] -= adjBaseX;
        mStageOffsetY[i] -= adjBaseY;
      }
    }
    AutodocStageOffsetIO(true, -1);
  }

  // Move the mini data into buffer B
  int miniType = 
    mConvertMini ? kUBYTE : (image->getType() == kUSHORT ? kUSHORT : kSHORT);
  if (!mConvertMini && mExpectingFloats)
    miniType = kFLOAT;
  if (mMiniData) {

    // Do the deferred tiling
    if (mMiniBorderY) {
      RetilePieces(miniType);

    // Or if pieces were shifted, find average shift and recenter image
    } else if (mParam->shiftInOverview && mNumErrSum > 1 && !mAlreadyHaveShifts) {
      shiftX = B3DNINT((mErrorSumX / mNumErrSum) / mMiniZoom);
      shiftY = -B3DNINT((mErrorSumY / mNumErrSum) / mMiniZoom);
      ProcShiftInPlace(mMiniData, miniType, mMiniSizeX, mMiniSizeY, 
        shiftX, shiftY, mMiniFillVal);
      for (i = 0; i < mNumPieces; i++) {
        if (mMiniOffsets.offsetX[i] != MINI_NO_PIECE) {
          mMiniOffsets.offsetX[i] += shiftX;
          mMiniOffsets.offsetY[i] += shiftY;
        }
      }
    }

    if (mBufferManager->ReplaceImage((char *)mMiniData, miniType, mMiniSizeX, mMiniSizeY,
      1, mTrialMontage ? BUFFER_PRESCAN_OVERVIEW : BUFFER_MONTAGE_OVERVIEW, 
      MONTAGE_CONSET, B3DCHOICE(mReadingMontage && mImBufs->mBinning > 0,
        mImBufs->mBinning, mParam->binning) * mMiniZoom, false, !mNoDrawOnRead)) {
      delete [] mMiniData;
    } else {

      // At this point buffer A still has the last read-in image, so get these parameters
      // from it if possible
      int bufCam = mImBufs->mCamera;
      mImBufs[1].mMagInd = B3DCHOICE(mReadingMontage && mImBufs->mMagInd > 0, 
        mImBufs->mMagInd, mParam->magIndex);
      mImBufs[1].mCamera = B3DCHOICE(mReadingMontage && bufCam >= 0,
        bufCam, mWinApp->GetCurrentCamera());
      mImBufs[1].mEffectiveBin = mMiniEffectiveBin;
      mImBufs[1].mDivideBinToShow = mImBufs->mDivideBinToShow;
      if (mReadingMontage && bufCam >= 0)
        mImBufs[1].mEffectiveBin = B3DMAX((float)mCamParams[bufCam].sizeX / mMiniSizeX, 
          (float)mCamParams[bufCam].sizeY / mMiniSizeY);
      mImBufs[1].mSecNumber = mParam->zCurrent;
      if (mParam->shiftInOverview) {
        mImBufs[1].mMiniOffsets = new MiniOffsets;
        *(mImBufs[1].mMiniOffsets) = mMiniOffsets;
      }
      if (mReadStoreMRC == mWinApp->mStoreMRC) {
        mImBufs[1].mCurStoreChecksum = mWinApp->mStoreMRC->getChecksum();
      } else {
        mImBufs[1].mSecNumber = -mParam->zCurrent - 1;
        mImBufs[1].mCurStoreChecksum = 0;
      }
      mImBufs[1].mProbeMode = mImBufs[0].mProbeMode;
      mImBufs[1].mChangeWhenSaved = mImBufs[0].mChangeWhenSaved;
      mImBufs[1].mOverviewBin = mMiniZoom;
      mImBufs[1].mPixelSize = mImBufs[0].mPixelSize * mMiniZoom;

      // Navigator is going to use this vs the setting below to distinguish if read-in
      mImBufs[1].mConSetUsed = MONTAGE_CONSET;
      mImBufs[1].mLowDoseArea = false;
      mImBufs[1].mMapID = 0;

      sectInd = -1;
      if (mReadingMontage) {
        sectInd = AccessMontSectInAdoc(mReadStoreMRC, mParam->zCurrent);
        mImBufs[1].mMapID = mWinApp->mNavHelper->FindMapIDforReadInImage(
          mReadStoreMRC->getFilePath(), mParam->zCurrent);
      }

      if (!mReadingMontage || sectInd >= 0) {
        extra1 = (EMimageExtra *)mImBufs[1].mImage->GetUserData();
        if (!extra1) {
          extra1 = new EMimageExtra;
          mImBufs[1].mImage->SetUserData(extra1);
        } 
      }

      // If this is a captured montage, copy extra data from buffer A if it exists
      // and insert current stage position and center image shift
      if (!mReadingMontage) {
        extra0 = (EMimageExtra *)mImBufs[0].mImage->GetUserData();
        if (extra0)
          *extra1 = *extra0;
        extra1->mStageX = mBaseStageX + adjBaseX;
        extra1->mStageY = mBaseStageY + adjBaseY;
        extra1->mISX = B3DCHOICE(mDoStageMoves && !mHaveStageOffsets && !mImShiftInBlocks,
          mImBufs[0].mISX, mBaseISX);
        extra1->mISY = B3DCHOICE(mDoStageMoves && !mHaveStageOffsets && !mImShiftInBlocks, 
          mImBufs[0].mISY, mBaseISY);
        extra1->ValuesIntoShorts();
        mImBufs[1].mISX = mHaveStageOffsets ? 0 : extra1->mISX;
        mImBufs[1].mISY = mHaveStageOffsets ? 0 : extra1->mISY;
        mImBufs[1].mBacklashX = mImBufs[1].mBacklashY = 0.;
        if (!mDoZigzagStage)
          GetLastBacklash(mImBufs[1].mBacklashX, mImBufs[1].mBacklashY);
        if (!mDoStageMoves && !mImBufs[1].mBacklashX && !mImBufs[1].mBacklashY)
          mScope->GetValidXYbacklash(mBaseStageX, mBaseStageY, mImBufs[1].mBacklashX,
            mImBufs[1].mBacklashY);
        mImBufs[1].mConSetUsed = MontageConSetNum(mParam, true);
        mImBufs[1].mLowDoseArea = mWinApp->LowDoseMode();
        if (IS_SET_VIEW_OR_SEARCH(mImBufs[1].mConSetUsed) && mImBufs[1].mLowDoseArea)
          mImBufs[1].mViewDefocus = mScope->GetLDViewDefocus(mImBufs[1].mConSetUsed);
        if (MapParamsToAutodoc() > 0)
          PrintfToLog("WARNING: Errors occurred writing parameters for defining a map to"
            " autodoc");
      } else if (sectInd >= 0) {
        MapParamsToOverview(sectInd);
        AdocReleaseMutex();
      }

      // Display if appropriate: and if reading, copy to read buffer first
      if (mReadingMontage || mTrialMontage || mParam->showOverview) {
        i = 1;
        if (mReadingMontage) {
          i = mBufToCopyTo;
          if (i == 1 || mBufferManager->CopyImageBuffer(1, i, !mNoDrawOnRead))
            i = 1;
        }
        if (!mNoDrawOnRead)
          mWinApp->SetCurrentBuffer(i);
      }
    }
    mMiniData = NULL;
  }

  // Only do special operations on a center pair if there are two and supposed to be 2
  BOOL goodCenterPair = mNumCenterSaved == 2 &&
    (mCenterIndX2 + 1 - mCenterIndX1) * (mCenterIndY2 + 1 - mCenterIndY1) == 2;

  // Compose the center piece
  // Adjust coordinates, but do not shift singleton
  // Signs are trial-and-error as usual
  if (mNumCenterSaved > 1) {
    for (isave = 0; isave < mNumCenterSaved; isave++) {
      mCenterCoordX[isave] += mCenterShift[isave][0];
      mCenterCoordY[isave] += mCenterShift[isave][1];
    }

    if (mAddMiniOffsetToCenter) {

      // Set up mini offsets for a 2 x 2
      mMiniOffsets.offsetX.clear();
      mMiniOffsets.offsetY.clear();
      mMiniOffsets.xBase = mParam->xOverlap / 2;
      mMiniOffsets.xDelta = (mParam->xFrame - mParam->xOverlap) / 2;
      mMiniOffsets.xFrame = (mParam->xFrame + mParam->xOverlap) / 2;
      mMiniOffsets.xNframes = 2;
      mMiniOffsets.yBase = mParam->yOverlap / 2;
      mMiniOffsets.yDelta = (mParam->yFrame - mParam->yOverlap) / 2;
      mMiniOffsets.yFrame = (mParam->yFrame + mParam->yOverlap) / 2;
      mMiniOffsets.yNframes = 2;

      // Find the center offset for each piece in order and put into mini offset
      for (ix = mCenterIndX1; ix <= mCenterIndX2; ix++) {
        for (iy = mCenterIndY1; iy <= mCenterIndY2; iy++) {
          for (isave = 0; isave < mNumCenterSaved; isave++) {
            if (mCenterPiece[isave] == iy + ix * mParam->yNframes) {
              mMiniOffsets.offsetX.push_back(mCenterShift[isave][0]);
              mMiniOffsets.offsetY.push_back(-mCenterShift[isave][1]);
              break;
            }
          }
        }
      }
    }
    
    if (goodCenterPair) {

      // For two pieces in Y, center first piece in X, shift second same
      if (mCenterIndY2 > mCenterIndY1) {
        mCenterCoordX[0] -= mCenterShift[0][0];
        mCenterCoordX[1] -= mCenterShift[0][0];
        mCenterShift[1][0] -= mCenterShift[0][0];
        mCenterShift[0][0] = 0;
        if (mAddMiniOffsetToCenter) {
          mMiniOffsets.xBase = 0;
          mMiniOffsets.xFrame = mMiniOffsets.xDelta = mParam->xFrame;
          mMiniOffsets.xNframes = 1;
        }
      } else {

        // For two pieces in X, center first one in Y
        mCenterCoordY[0] -= mCenterShift[0][1];
        mCenterCoordY[1] -= mCenterShift[0][1];
        mCenterShift[1][1] -= mCenterShift[0][1];
        mCenterShift[0][1] = 0;
        if (mAddMiniOffsetToCenter) {
          mMiniOffsets.yBase = 0;
          mMiniOffsets.yFrame = mMiniOffsets.yDelta = mParam->yFrame;
          mMiniOffsets.yNframes = 1;
        }
      }
    }
  }

  double dSum = 0.;
  nSum = 0;
  int tSum;
  float fSum;
  for (isave = 0; isave < mNumCenterSaved; isave++) {

    // Find limits of copy, in montage coordinates
    startX = B3DMAX(mMontCenterX, mCenterCoordX[isave]);
    endX = B3DMIN(mMontCenterX, mCenterCoordX[isave]) + mParam->xFrame - 1;
    startY = B3DMAX(mMontCenterY, mCenterCoordY[isave]);
    endY = B3DMIN(mMontCenterY, mCenterCoordY[isave]) + mParam->yFrame - 1;
    
    image = mCenterImage[isave];
    image->Lock();
    short int *sShort = (short int *)image->getData();
    unsigned char *byte = (unsigned char *)sShort;
    unsigned short int *uShort = (unsigned short int *)sShort;
    float *dFloat = (float *)sShort;

    // Copy the data, getting cumulative sum
    for (iy = endY; iy >= startY; iy--) {
      iny = (mParam->yFrame - 1) - (iy - mCenterCoordY[isave]);
      outy = (mParam->yFrame - 1) - (iy - mMontCenterY);
      indin = iny * mParam->xFrame + startX - mCenterCoordX[isave];
      indout = outy * mParam->xFrame + startX - mMontCenterX;
      
      tSum = 0; 
      fSum = 0.;
      switch (cenType) {
      case kSHORT:
        for (ix = indin; ix <= indin + endX - startX; ix++) {
          mCenterData[indout++] = sShort[ix];
          tSum += sShort[ix];
        }
        break;

      case kUSHORT:
        for (ix = indin; ix <= indin + endX - startX; ix++) {
          uCenter[indout++] = (short int)(uShort[ix]);
          tSum += uShort[ix];
        }
        break;

      case kFLOAT:
        for (ix = indin; ix <= indin + endX - startX; ix++) {
          centerFloat[indout++] = dFloat[ix];
          fSum += dFloat[ix];
        }
        break;

      case kUBYTE:
        for (ix = indin; ix <= indin + endX - startX; ix++) {
          mCenterData[indout++] = byte[ix];
          tSum += byte[ix];
        }
        break;
      }
      if (cenType == kFLOAT)
        dSum += fSum;
      else
        dSum += tSum;
      nSum += endX + 1 - startX;
    }

    // Done with the data - should be able to delete it
    image->UnLock();
    delete image;
    mCenterImage[isave] = NULL;
  }


  // Now if there are two pieces in the center, need to fill and taper corners
  if (goodCenterPair) {
    float dMean = (float)(dSum / nSum);
    if (mCenterIndY2 > mCenterIndY1) {

      // the two frames are in Y
      startY = mParam->yFrame + mCenterCoordY[0] - mMontCenterY;
      endY = mParam->yFrame - 1;
      iDirY = -1;
      if (mCenterCoordX[1] > mMontCenterX) {
        // gap is on the left side of upper piece
        iDirX = 1;
        startX = 0;
        endX = mCenterCoordX[1] - mMontCenterX - 1;
      } else {
        // gap is to right side
        iDirX = -1;
        endX = mParam->xFrame - 1;
        startX = endX - (mMontCenterX - mCenterCoordX[1] - 1);
      }
    } else {

      // the two frames are in X
      startX = mParam->xFrame + mCenterCoordX[0] - mMontCenterX;
      endX = mParam->xFrame - 1;
      iDirX = -1;
      if (mCenterCoordY[1] > mMontCenterY) {
        // gap is below rightmost piece
        iDirY = 1;
        startY = 0;
        endY = mCenterCoordY[1] - mMontCenterY - 1;
      } else {
        // gap is above it
        iDirY = -1;
        endY = mParam->yFrame - 1;
        startY = endY - (mMontCenterY - mCenterCoordY[1] - 1);
      }
    }
    XCorrFillTaperGap(mCenterData, cenType, mParam->xFrame, mParam->yFrame, dMean,
            startX, endX, iDirX, startY, endY, iDirY, centerTaper);
  }

  // Get a copy of extra data from current buffer if it exists, install after replacement
  extra0 = (EMimageExtra *)mImBufs[0].mImage->GetUserData();
  if (extra0) {
    extra1 = new EMimageExtra;
    *extra1 = *extra0;
  } 

  if (mBufferManager->ReplaceImage((char *)mCenterData, 
    cenType == kUBYTE ? kSHORT : cenType,  mParam->xFrame, mParam->yFrame, 0,
    mTrialMontage ? BUFFER_PRESCAN_CENTER : BUFFER_MONTAGE_CENTER, 
    mImBufs[0].mConSetUsed, mReadingMontage ? 1 : mParam->binning, // Why not set in read
    false, !mNoDrawOnRead)) {
      delete [] mCenterData;
      if (extra0)
        delete extra1;
  } else {
    if (extra0)
      mImBufs->mImage->SetUserData(extra1);
    if (!mReadingMontage) {
      mImBufs->mMagInd = mParam->magIndex;
      mImBufs->mEffectiveBin = mParam->binning;
      mImBufs->mDefocus = mBaseFocus;
    }
  }
  mCenterData = NULL;
  if (mAddMiniOffsetToCenter) {
    mImBufs->mMiniOffsets = new MiniOffsets;
    *(mImBufs->mMiniOffsets) = mMiniOffsets;
  }

  if (!mReadingMontage && !mTrialMontage) {

    // Autoalign if function is selected 
    if (mBufferManager->GetAlignOnSave() && mParam->zCurrent > 0)
      mShiftManager->AutoAlign(0, 1);

    if (mBufferManager->GetCopyOnSave())
      mBufferManager->CopyImageBuffer(0, mBufferManager->GetCopyOnSave());

    // Increment Z value
    mParam->zCurrent++;
  }

  mLastFailed = false;
  mNoDrawOnRead = false;
  mRequiredBWMean = -1.;  // A required mean must be registered each time
  CleanPatches();
  if (!mRestoringStage)
    StageRestoreDone();
  if (mReadingMontage)
    ReadingDone();

  // Find out whether to make a map; fake out the task test
  iDirX = mPieceIndex;
  mPieceIndex = -1;
  doingTasks = mWinApp->DoingTasks() && !mWinApp->GetJustMontRestoringStage();
  mPieceIndex = iDirX;

  if (!mReadingMontage && mParam->makeNewMap && !mWinApp->DoingTiltSeries() &&
    !doingTasks && !mUsingMultishot && mWinApp->mNavigator &&
    !mWinApp->mNavigator->GetAcquiring()) {
    mWinApp->mNavigator->NewMap();
  }
  if (!mReadingMontage && mParam->closeFileWhenDone && !mWinApp->DoingTiltSeries() &&
    !doingTasks && !mUsingMultishot && (!mWinApp->mNavigator ||
    !mWinApp->mNavigator->GetAcquiring())) {
    mWinApp->mDocWnd->DoCloseFile();
  }
  SEMTrace('M', "Montage Done processing");
  return 0;
}

// Do operations after an edge correlation is done
void EMmontageController::ProcessXCorrResult()
{
  int ix, iy, ixy;
  int lower, upper;

  for (ixy = 0; ixy < 2; ixy++) {
    if (!mXCTD.idir[ixy])
      continue;

    lower = mXCTD.lower[ixy];
    upper = mXCTD.upper[ixy];


    // Get the max SD and alternative shifts; apply same operation to them as to 
    // xfirst, yfirst and hope its right
    mMaxSDToIxyOfEdge[mTrimmedMaxSDs.size()] = ixy;
    mMaxSDToPieceNum[mTrimmedMaxSDs.size()] = lower;
    mTrimmedMaxSDs.push_back(mXCTD.trimmedMaxSD[ixy]);
    if (mNumXcorrPeaks > 1) {
      iy = 4 * (2 * lower + ixy);
      for (ix = 0; ix < 2; ix++) {
        if (mXCTD.alternShifts[ixy][2 * ix] > -1.e20) {
          mAlternShifts[iy + 2 * ix] = -mXCTD.alternShifts[ixy][2 * ix];
          mAlternShifts[iy + 2 * ix + 1] = mXCTD.alternShifts[ixy][2 * ix + 1];
        }
      }
    }

    // This returned the amount to shift the upper edge to align it to the
    // lower, except that Y is inverted.  We need the amount that upper
    // edge is displaced from the lower one, so take -X and +Y.
    mUpperShiftX[2 * lower + ixy] = -mXCTD.xPeak[ixy];
    mUpperShiftY[2 * lower + ixy] = mXCTD.yPeak[ixy];
    mUpperFirstX[2 * lower + ixy] = -mXCTD.xFirst[ixy];
    mUpperFirstY[2 * lower + ixy] = mXCTD.yFirst[ixy];
    mPatchCCC[2 * lower + ixy] = mXCTD.CCCmax[ixy];



    // Now get rid of the patches
    delete[] mUpperPatch[2 * upper + ixy];
    delete[] mLowerPatch[2 * lower + ixy];
    mUpperPatch[2 * upper + ixy] = NULL;
    mLowerPatch[2 * lower + ixy] = NULL;
  }
}

// Add correlation peak positions to the mActualErrorX/Y
int EMmontageController::AddToActualErrors()
{
  int idir, i, ixy, nSum = 0;
  for (ixy = 0; ixy < 2; ixy++) {
    idir = mXCTD.idir[ixy];
    if (idir) {
      // Compute contribution to actual error in position of this piece
      i = idir > 0 ? mXCTD.lower[ixy] : mXCTD.upper[ixy];
      mActualErrorX[mXCTD.pieceIndex] += mActualErrorX[i] - idir * mXCTD.xPeak[ixy];
      mActualErrorY[mXCTD.pieceIndex] += mActualErrorY[i] + idir * mXCTD.yPeak[ixy];
      nSum++;
      /*SEMTrace('1', "Piece %d %d  %s %s error %.0f %.0f plus %.0f %.0f", mPieceX,
      mPieceY, idir > 0 ? "lower" : "upper", ixy ? "Y" : "X", mActualErrorX[i],
      mActualErrorY[i], - idir * xPeak, idir * yPeak);*/
    }
  }
  return nSum;
}
// Finish actual error computation, add to error sum and predicted error if needed
void EMmontageController::MaintainErrorSums(int nSum, int pieceIndex)
{

  // In either case, compute actualError, needed for aligning when not deferring tiling
  if (mDoCorrelations || mUsingMultishot) {
    if (nSum > 1) {
      mActualErrorX[pieceIndex] /= 2.;
      mActualErrorY[pieceIndex] /= 2.;
    }
    mErrorSumX += mActualErrorX[pieceIndex];
    mErrorSumY += mActualErrorY[pieceIndex];
    mNumErrSum++;

    if (mParam->correctDrift && !mDoStageMoves && !mUsingMultishot &&
      mMagTab[mParam->magIndex].calibrated[mWinApp->GetCurrentCamera()]) {
      mPredictedErrorX += mActualErrorX[pieceIndex];
      mPredictedErrorY += mActualErrorY[pieceIndex];
    }
  }
}

// The edge correlation procedure
UINT EMmontageController::XCorrProc(LPVOID param)
{
  int ixy, useExtra[2];
  XCorrThreadData *td = (XCorrThreadData *)param;
  float sDmin, denmin;
  // Search here is on binned data, do more
  int numIter = 4 + (td->XCorrBinning > 1 ? 1 : 0);
  int limStep = 10;

  for (ixy = 0; ixy < 2; ixy++) {
    if (!td->idir[ixy])
      continue;
    // If we ever pass expected shifts through these, they need to account for Y
    // inversion.  Pass -Y extra so it adjusts correctly for inversion
    td->xFirst[ixy] = td->yFirst[ixy] = 0.;
    useExtra[0] = td->numExtra[ixy][0];
    useExtra[1] = -td->numExtra[ixy][1];
    if (td->debugLevel) {
      SEMTrace('a', "box %d %d pc %d %d ov %d %d smth %d %d pad %d %d", td->XYbox[ixy][0],
        td->XYbox[ixy][1], td->XYpieceSize[0], td->XYpieceSize[1], td->XYoverlap[0],
        td->XYoverlap[1], td->Xsmooth[ixy], td->Ysmooth[ixy], td->Xpad[ixy], td->Ypad[ixy]);
      SEMTrace('a', "numpk %d CTF  %f %f delta %f extra %d %d bin %d ixy %d mls %d",
        td->numXcorrPeaks, td->CTFp[ixy][200], td->CTFp[ixy][500], td->delta[ixy],
        useExtra[0], useExtra[1], td->XCorrBinning, ixy,
        td->maxLongShift[ixy]);
    }
    montXCorrEdge(td->lowerPatch[ixy], td->upperPatch[ixy], td->XYbox[ixy], 
      td->XYpieceSize, td->XYoverlap, td->Xsmooth[ixy], td->Ysmooth[ixy], td->Xpad[ixy],
      td->Ypad[ixy], td->lowerPad, td->upperPad, td->lowerCopy, td->numXcorrPeaks, 0,
      td->CTFp[ixy], td->delta[ixy], &useExtra[0], td->XCorrBinning, ixy,
      td->maxLongShift[ixy], td->numXcorrPeaks > 1 ? 1 : 0, &td->xFirst[ixy],
      &td->yFirst[ixy], &td->CCCmax[ixy], twoDfft, NULL, td->debugStr, td->debugLen,
      td->debugLevel);

    if (td->debugLevel) {
      char *lineEnd, *curDebug = &td->debugStr[0];

      while ((lineEnd = strchr(curDebug, '\n')) != NULL) {
        *lineEnd = 0x00;
        SEMTrace('a', "pc %d low %d up %d dir %d xy %d  %s", td->pieceIndex, td->lower[ixy],
          td->upper[ixy], td->idir[ixy], ixy, curDebug);
        curDebug = lineEnd + 1;
      }
    }
    td->trimmedMaxSD[ixy] = montXCGetLastTrimmedMaxSD();
    if (td->numXcorrPeaks > 1)
      montXCGetLastRunnersUp(&td->alternShifts[ixy][0], 2);

    // First get back to the correlation peak position in these images by undoing what
    // montXcorrEdge did. Take the negative before and after as in Blendmont,
    // so function can be called as lower, upper
    // No longer need to adjust Y peak by 2 * extraWidth;
    td->xPeak[ixy] = -B3DNINT(td->xFirst[ixy] / td->XCorrBinning + td->numExtra[ixy][0]);
    td->yPeak[ixy] = -B3DNINT((td->yFirst[ixy] / td->XCorrBinning -td->numExtra[ixy][1]));

    montBigSearch(td->lowerPatch[ixy], td->upperPatch[ixy],
      td->XYbox[ixy][0], td->XYbox[ixy][1], 0, 0, td->XYbox[ixy][0] - 1, td->XYbox[ixy][1] - 1,
      &td->xPeak[ixy], &td->yPeak[ixy], &sDmin, &denmin, numIter, limStep);

    // Use negative peak value, apply adjustment by extra
    td->xPeak[ixy] = td->XCorrBinning * (-td->xPeak[ixy] - td->numExtra[ixy][0]);
    td->yPeak[ixy] = td->XCorrBinning * (-td->yPeak[ixy] + td->numExtra[ixy][1]);
  }
  return 0;
}

// Wait for the XCorrProc thread to finish and return 0, 
// if it doesn't finish, kill it and clean up, return 1
int EMmontageController::WaitForXCorrProc(int timeout)
{
  double startTime = GetTickCount();
  int busy;
  while (SEMTickInterval(startTime) < timeout) {
    busy = UtilThreadBusy(&mXCorrThread);
    if (busy <= 0)
      return 0;
    Sleep(25);
  }
  UtilThreadCleanup(&mXCorrThread);
  return 1;
}

///////////////////////////////////////
// TERMINATION ROUTINES
///////////////////////////////////////

void EMmontageController::PieceCleanup(int error)
{
  StopMontage(error);
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout doing montage"));
  mWinApp->ErrorOccurred(error);
}

// For stopping a montage - not called on normal finish
void EMmontageController::StopMontage(int error)
{
  if (mPieceIndex < 0)
    return;

  // If already restoring stage, clear it out only if this is a user stop
  if (mRestoringStage > 0) {
    if (!error)
      StageRestoreDone();
    return;
  }
  if (mXCorrThread)
    WaitForXCorrProc(5000);
  
  // Just return to starting position of image shifts or stage
  if (!mReadingMontage)
    mCamera->StopCapture(1);
  mCamera->ChangePreventToggle(-1);
  if (mLoweredMag)
    mWinApp->mComplexTasks->RestoreMagIfNeeded();
  if (!mReadingMontage) {
    if (mDoStageMoves) {
      mMoveInfo.x = mBaseStageX;
      mMoveInfo.y = mBaseStageY;

      // Let the camera or stage move finish and let DoNextPiece start the restore
      if (!mMovingStage && !mCamera->CameraBusy() && !mScope->WaitForStageReady(10000))
        StartStageRestore();
      else
        StageRestoreDone(-1);
    } 
    if (mUsingImageShift) {
      if (mParam->adjustFocus)
        mScope->SetDefocus(mBaseFocus);
      mScope->SetImageShift(mBaseISX, mBaseISY);
      mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
      if (!mDoStageMoves)
        StageRestoreDone();
    }
  } else {
    ReadingDone();
    StageRestoreDone();
  }

  delete [] mMiniData;
  mMiniData = NULL;
  mRunningMacro = false;
  CleanPatches();
  delete mMultiShotParams;
  mMultiShotParams = NULL;
  mNoDrawOnRead = false;

  for (int i = 0; i < 4; i++) {
    delete mCenterImage[i];
    mCenterImage[i] = NULL;
  }
}

// Restore stage at end
void EMmontageController::StartStageRestore(void)
{
  SEMTrace('M', "Stage Restore Start");
  if (mDoISrealign)
    mScope->SetImageShift(0., 0.);
  mScope->MoveStage(mMoveInfo, false);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE_RESTORE, 0, 120000);
  mRestoringStage = 1;
}

// External/internal call when stage restore is done: now sole place where really done
void EMmontageController::StageRestoreDone(int restoreVal)
{
  if (!restoreVal)
    SEMTrace('M', "Stage Restore Done");
  mRestoringStage = restoreVal;
  mPieceIndex = -1;
  mMovingStage = false;
  mFocusing = false;
  mShootingFilm = false;
  mWinApp->UpdateBufferWindows();
  if (!mUsingMultishot)
    mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Reset the Z value and set up overlap boxes again when done reading
void EMmontageController::ReadingDone(void)
{
  mParam->zCurrent = mReadSavedZ;
  mWinApp->UpdateBufferWindows();
  if (mReadResetBoxes) {
    mParam = mWinApp->GetMontParam();
    mVerySloppy = mParam->verySloppy;
    SetupOverlapBoxes();
  }
  mWinApp->SetStatusText(SIMPLE_PANE, "");
}

//////////////////////////////////////////////
// GENERAL SUPPORT ROUTINES
//////////////////////////////////////////////

// Return index of piece that is lower in given direction 0 (X, 1 Y) or -1 if none
int EMmontageController::LowerIndex(int inIndex, int inDir)
{
  int ix = inIndex / mParam->yNframes;
  int iy =  inIndex % mParam->yNframes;
  
  if (!inDir)
    ix--;
  else
    iy--;
  if (ix < 0 || iy < 0)
    return -1;
  int index = iy + ix * mParam->yNframes;
  if (mPieceToVar[index] < 0)
    return -1;
  return index;
}

// Return index of piece that is upper in given direction 0 (X, 1 Y) or -1 if none
int EMmontageController::UpperIndex(int inIndex, int inDir)
{
  int ix = inIndex / mParam->yNframes;
  int iy =  inIndex % mParam->yNframes;
  if (!inDir)
    ix++;
  else
    iy++;
  if (ix >= mParam->xNframes || iy >= mParam->yNframes)
    return -1;
  int index = iy + ix * mParam->yNframes;
  if (mPieceToVar[index] < 0)
    return -1;
  return index;
}

void EMmontageController::CleanPatches()
{
  delete [] mCenterData;
  mCenterData = NULL;
  delete [] mLowerPad;
  mLowerPad = NULL;
  delete [] mUpperPad;
  mUpperPad = NULL;
  delete [] mLowerCopy;
  mLowerCopy = NULL;
  delete [] mBinTemp;
  mBinTemp = NULL;
  for (int i = 0; i < mNumPieces; i++)
    for (int ixy = 0; ixy < 2; ixy++) {
      delete [] mLowerPatch[2 * i + ixy];
      mLowerPatch[2 * i + ixy] = NULL;
      delete [] mUpperPatch[2 * i + ixy];
      mUpperPatch[2 * i + ixy] = NULL;
    }
}

// Extract overlap data from image into a patch
void EMmontageController::extractPatch(void *data, int type, int ixy, int *ind0, 
                                       int *ind1, float **patch)
{
  int ix, iy, nx = mXYbox[ixy][0];
  int ny = mXYbox[ixy][1];
  NewArray(*patch, float, mBoxPixels[ixy]);
  if (*patch) {
    if (mXCorrBinning > 1) {
      extractWithBinning(data, type, mParam->xFrame, ind0[0], ind1[0], ind0[1], ind1[1], 
        mXCorrBinning, mBinTemp, 1, &ix, &iy);
      XCorrTaperInPad(mBinTemp, type, nx, 0, nx - 1, 0, ny - 1, *patch, nx, nx, ny, 0, 0);
    } else {
      XCorrTaperInPad(data, type, mParam->xFrame, ind0[0], ind1[0], ind0[1], ind1[1],
        *patch, nx, nx, ny, 0, 0);
    }
  } else
    mWinApp->AppendToLog("Error getting memory for overlap patches",
    LOG_OPEN_IF_CLOSED);
}

// Get the index of next piece that is not to be skipped, and save the sequence index
int EMmontageController::NextPieceIndex()
{
  mNextSeqInd = mSeqIndex;
  while (++mNextSeqInd < mNumPieces) {
    if (mPieceToVar[mIndSequence[mNextSeqInd]] >= 0)
      return mIndSequence[mNextSeqInd];
  }
  mNextSeqInd--;
  return mNumPieces;
}

// Advance the piece numbers from current piece to next available piece and set flag if
// stage backlash would be needed
void EMmontageController::SetNextPieceIndexXY()
{
  mPieceIndex = NextPieceIndex();
  mNeedBacklash = (mParam->yNframes > 1 && mPieceX != mPieceIndex / mParam->yNframes + 1
    && mPieceIndex < mNumPieces);
  mNeedBacklash =  mSeqIndex < 0 || (mNeedBacklash && !mUsingAnchor) || 
    (mUsingAnchor && PieceStartsColumn(mPieceIndex));
  mSeqIndex = mNextSeqInd;
  mPieceX = mPieceIndex / mParam->yNframes + 1;
  mPieceY = mPieceIndex % mParam->yNframes + 1;
  if (mNumDoing == mInitialNumDoing + 1)
    mStartTime = GetTickCount();
  mNumDoing++;
}

// Add a piece to the skip list if it is not already on it
bool EMmontageController::AddToSkipListIfUnique(int ix)
{
  bool add = true;
  for (int iy = 0; iy < mNumToSkip; iy++) {
    if (mSkipIndex[iy] == ix) {
      add = false;
      break;
    }
  }
  if (add)
    mSkipIndex[mNumToSkip++] = ix;
  return add;
}

// Return true if montaging is on and the current camera cannot be used with these params
bool EMmontageController::CameraNotFeasible(MontParam *param)
{
  int top, bot, left, right, xFrame, yFrame, setNum;
  if (!mMontaging)
    return false;
  if (!param)
    param = mWinApp->GetMontParam();
  int camera = mWinApp->GetCurrentCamera();
  setNum = MontageConSetNum(param, false);
  if (param->binning * param->xFrame > mCamParams[camera].sizeX || 
    param->binning * param->yFrame > mCamParams[camera].sizeY)
    return true;
  xFrame = param->xFrame;
  yFrame = param->yFrame;
  mCamera->CenteredSizes(xFrame, mCamParams[camera].sizeX, mCamParams[camera].moduloX, 
    left, right, yFrame, mCamParams[camera].sizeY, mCamParams[camera].moduloY, top, bot,
    param->binning);
  return xFrame != param->xFrame || yFrame != param->yFrame;
}

// Get the active camera number depending on whether low dose is on or not
int EMmontageController::GetMontageActiveCamera(MontParam *param)
{
  if (mWinApp->LowDoseMode())
    return mWinApp->GetCurrentActiveCamera();
  return param->cameraIndex;
}

// Return the backlash values from the last run unless it used anchor
void EMmontageController::GetLastBacklash(float &outX, float &outY)
{
  outX = (float)(mUsingAnchor ? 0. : mMoveBackX);
  outY = (float)(mUsingAnchor ? 0. : mMoveBackY);
}

// Return the backlash that would be used for going up columns at current conditions
void EMmontageController::GetColumnBacklash(float & backlashX, float & backlashY)
{
  ScaleMat bInv = MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), 
    mScope->FastMagIndex()));
  backlashX = backlashY = mStageBacklash;
  if (bInv.xpy > 0)
    backlashX = -backlashX;
  if (bInv.ypy > 0)
    backlashY = -backlashY;
}

// Figure out maximum binning for trial montages
int EMmontageController::SetMaxPrescanBinning()
{
  int trialTarget = 1024;   // Target size for trial montage
  int fullX, fullY, binning, i;
  int *activeList = mWinApp->GetActiveCameraList();

  int iCam = activeList[GetMontageActiveCamera(mParam)];
  CameraParameters *cam = mWinApp->GetCamParams() + iCam;

  // For faster readout cameras (e.g., CMOS), limit the added binning to 2 and make target
  // bigger
  bool limitBinning = false;
  if (mCamera->IsDirectDetector(cam) || cam->OneViewType || cam->FEItype == 3) {
    trialTarget = 2048;
    limitBinning = true;
  }

  fullX = mParam->binning * (mParam->xFrame * mParam->xNframes -
    mParam->xOverlap * (mParam->xNframes - 1));
  fullY = mParam->binning * (mParam->yFrame * mParam->yNframes -
    mParam->yOverlap * (mParam->yNframes - 1));
  
  // Find the binning that reduces it below target
  for (i = 0; i < cam->numBinnings; i++) {
    binning = cam->binnings[i];
    if (binning < mParam->binning)
      continue;
    if (fullX / binning <= trialTarget && fullY / binning <= trialTarget ||
      (limitBinning && binning >= 2 * mParam->binning))
      break;
  }
  mParam->maxPrescanBin = binning;
  return binning;
}

// Try to set the prescan binning to the given value, but keep it to legal
// binning within montage parameter limits, change by dir (+/-1) if needed
int EMmontageController::SetPrescanBinning(int tryVal, int dir)
{
  int *activeList = mWinApp->GetActiveCameraList();
  int iCam = activeList[GetMontageActiveCamera(mParam)];

  // First clamp the binning to the limits of the montage and the maximum
  if (tryVal < mParam->binning)
    tryVal = mParam->binning;
  if (tryVal > mParam->maxPrescanBin)
    tryVal = mParam->maxPrescanBin;
  
  // Change the binning in the given direction until it is valid for the camera
  while (!mWinApp->BinningIsValid(iCam, tryVal, false) && tryVal > mParam->binning && 
    tryVal < mParam->maxPrescanBin)
    tryVal += dir;
  mParam->prescanBinning = tryVal;
  return tryVal;
}

// Returns coordinates of current piece for camera controller to put in image extra data
// For a trial montage, this will be coords in an unbinned montage so that the buffer
// display will give the right frame number
void EMmontageController::GetCurrentCoords(int &outX, int &outY, int &outZ)
{
  MontParam *master = mWinApp->GetMontParam();
  outX = (mPieceX - 1) * (master->xFrame - master->xOverlap);
  outY = (mPieceY - 1) * (master->yFrame - master->yOverlap);
  outZ = mParam->zCurrent;
}

// Make a list of the pieces in the file at the given z value, return the section #
// for each piece indexed in Y then X order in pieceSavedAt; return value # of pieces
// and also clear and reallocate the vector pieceSavedAt
// Optionally return piece coordinates if ixVec, iyVec non-NULL, and stage coordinates
// if there is an autodoc and xStage, yStage are non-NULL
int EMmontageController::ListMontagePieces(KImageStore *storeMRC, MontParam *param, 
  int zValue, IntVec &pieceSavedAt, IntVec *ixVec, IntVec *iyVec, FloatVec *xStage,
  FloatVec *yStage, FloatVec *meanVec, float *zStage, FloatVec *midFrac, 
  FloatVec *rangeFrac)
{
  int already, nsec, ind, ix, iy, iz, fullNum = param->xNframes * param->yNframes;
  float amin, amax;
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  bool gotMutex = storeMRC->GetAdocIndex() >= 0 &&
    (storeMRC->getStoreType() != STORE_TYPE_MRC || storeMRC->montCoordsInAdoc());
  int nameInd = storeMRC->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
  if (param->xNframes <= 0 || param->yNframes <= 0 || param->xFrame - param->xOverlap <= 0
    || param->yFrame - param->yOverlap <= 0 || param->xNframes >= 10000 ||
    param->yNframes >= 10000 || param->xFrame > 17000 || param->yFrame > 17000) {
    PrintfToLog("BAD PARAMETERS PASSED TO ListMontagePieces: xynf %d %d xyf %d %d ov %d "
      "%d", param->xNframes, param->yNframes, param->xFrame, param->yFrame, 
      param->xOverlap, param->yOverlap);
    return -1;
  }
  CLEAR_RESIZE(pieceSavedAt, int, fullNum);
  if (ixVec && iyVec) {
    ixVec->resize(fullNum);
    iyVec->resize(fullNum);
  }
  mBufferManager->CheckAsyncSaving();
  for (ind = 0; ind < param->xNframes * param->yNframes; ind++)
    pieceSavedAt[ind] = -1;
  already = 0;
  nsec = storeMRC->getDepth();
  if (xStage && yStage)
    gotMutex = true;
  if (gotMutex && AdocGetMutexSetCurrent(storeMRC->GetAdocIndex()) < 0)
    return 0;
  if (xStage && yStage && gotMutex) {
    yStage->resize(fullNum);
    xStage->resize(fullNum);
    if (meanVec)
      meanVec->resize(fullNum);
    if (midFrac && rangeFrac) {
      midFrac->resize(fullNum, -1.);
      rangeFrac->resize(fullNum, -1.);
    }
  }
  if (zStage)
    *zStage = EXTRA_NO_VALUE;
  for (int isec = 0; isec <  nsec ; isec++) {
    if (!storeMRC->getPcoord(isec, ix, iy, iz, gotMutex) && iz == zValue) {
      ind = (ix / (param->xFrame - param->xOverlap) ) * 
        param->yNframes + iy / (param->yFrame - param->yOverlap);
      pieceSavedAt[ind] = isec;
      if (ixVec && iyVec) {
        ixVec->at(ind) = ix;
        iyVec->at(ind) = iy;
      }
      if (xStage && yStage && gotMutex) {
        if (AdocGetTwoFloats(names[nameInd], isec, ADOC_STAGE, &xStage->at(ind),
          &yStage->at(ind))) {
          already = 0;
          break;
        }

        // Replace with nominal stage coordinates for hybrid montage
        if (!AdocGetTwoFloats(names[nameInd], isec, ADOC_NOMINAL_STAGE, &amin, &amax)) {
          xStage->at(ind) = amin;
          yStage->at(ind) = amax;
        }

        if (meanVec && AdocGetThreeFloats(names[nameInd], isec, ADOC_MINMAXMEAN, &amin,
          &amax, &meanVec->at(ind))) {
          already = 0;
          break;
        }
        if (zStage && *zStage < EXTRA_VALUE_TEST && AdocGetFloat(names[nameInd], isec, 
          ADOC_STAGEZ, zStage)) {
          already = 0;
          break;
        }

        if (midFrac && rangeFrac && AdocGetThreeFloats(names[nameInd], isec, 
          ADOC_PCTL_STATS, &amax, &midFrac->at(ind), &rangeFrac->at(ind)) < 0) {
          already = 0;
          break;
        }
      }
      already++;
    }
  }
  if (gotMutex)
    AdocReleaseMutex();
  return already;
}

// Compute and save the effective stage position at which piece was acquired, adjusting
// for image shift
void EMmontageController::SetAcquiredStagePos(int piece, double stageX, double stageY, 
                                              double ISX, double ISY)
{
  ScaleMat dMat = MatMul(mShiftManager->IStoCamera(mParam->magIndex), MatInv(
    mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mParam->magIndex)));
  mAcquiredStageX[piece] = stageX + dMat.xpx * ISX + dMat.xpy * ISY;
  mAcquiredStageY[piece] = stageY + dMat.ypx * ISX + dMat.ypy * ISY;
  SEMTrace('S',"SetAcquiredStagePos for %d: %.3f,%.3f from stage %.3f,%.3f  IS %.3f,%.3f"
    , piece, mAcquiredStageX[piece], mAcquiredStageY[piece], stageX, stageY, ISX, ISY);
}

/// Get the piece index from X and Y frame numbers numbered from 1, or -1 if none
int EMmontageController::PieceIndexFromXY(int ix, int iy)
{
  int ind = (ix - 1) * mParam->yNframes + iy - 1;
  if (ix > 0 && ix <= mParam->xNframes && iy > 0 && iy <= mParam->yNframes && 
    mPieceToVar[ind] >= 0)
    return ind;
  return -1;
}

// Return 0 if this piece is not in a new column, 1 if it is the first in a column,
// -1 if there is no previous piece
int EMmontageController::PieceStartsColumn(int index)
{
  if (mPieceIndex >= mNumPieces || index >= mNumPieces)
    return 0;

  // For IS realign with anchor, directly test whether the piece is at the column start
  if (mUsingAnchor) {
    if (index == mPieceIndex && mSeqIndex <= 0)
      return -1;
    return (index % mParam->yNframes == mColumnStartY[index / mParam->yNframes]) ? 1 : 0;
  }

  // Otherwise Look back from this piece to previous existing piece
  for (int i = index - 1; i >= 0; i--)
    if (mPieceToVar[i] >= 0)
      return (i / mParam->yNframes < index / mParam->yNframes &&
        (!mFocusInBlocks || mFocusBlockInd[index] >= 0)) ? 1 : 0;
  return -1;
}

// Return true if it is time to realign to a piece: either the start of a column and
// the number done in previous column was at least a third of a column or higher than the
// criterion; or number since last realign is above criterion and there are no focus
// blocks or it is start of a focus block
bool EMmontageController::PieceNeedsRealigning(int index, bool nextPiece)
{
  int ifcol,  np = 0, numDone = mNumDoneInColumn;
  if (!mDoStageMoves || !mRealignInterval)
    return false;
  ifcol = PieceStartsColumn(index);
  if (ifcol < 0)
    return false;
  if (nextPiece) {
    np = 1;
    numDone++;
    if (PieceStartsColumn(mPieceIndex))
      numDone = 1;
  }

  return (ifcol > 0 && (numDone >= mRealignInterval || 
    numDone > mParam->yNframes / 3)) ||
    (!ifcol && mNumSinceRealign + np >= mRealignInterval && 
    (!mFocusInBlocks || mFocusBlockInd[index] >= 0));
}

// Limit the X and Y sizes if necessary to be within usable area
void EMmontageController::LimitSizesToUsable(CameraParameters *cam, int camNum, 
  int magInd, int &xsize, int &ysize, int binning)
{
  int ind, div;
  if (mLimitMontToUsable && cam->defects.usableRight)
    LimitOneSizeToUsable(xsize, cam->sizeX, cam->defects.usableLeft, 
    cam->defects.usableRight, binning, cam->moduloX);
  if (mLimitMontToUsable && cam->defects.usableBottom)
    LimitOneSizeToUsable(ysize, cam->sizeY, cam->defects.usableTop, 
    cam->defects.usableBottom, binning, cam->moduloY);
  for (ind = 0; ind < (int)mMontageLimits.GetSize(); ind++) {
    if (mMontageLimits[ind].camera == camNum && 
      (mMontageLimits[ind].magInd == magInd || !mMontageLimits[ind].magInd)) {
      div = BinDivisorI(cam);
      if (mMontageLimits[ind].right)
        LimitOneSizeToUsable(xsize, cam->sizeX, div * mMontageLimits[ind].left, 
          div * mMontageLimits[ind].right, binning, cam->moduloX);
      if (mMontageLimits[ind].bottom)
        LimitOneSizeToUsable(ysize, cam->sizeY, div * mMontageLimits[ind].top, 
         div *  mMontageLimits[ind].bottom, binning, cam->moduloY);
      break;
    }
  }
}

// Compute centered size that is within usable area in one dimension
void EMmontageController::LimitOneSizeToUsable(int &size, int camSize, int start, int end, 
                                         int binning, int modulo)
{
  int border = B3DMAX(start, camSize - 1 - end);
  int limit = (camSize - 2 * border) / binning;
  limit = modulo * (limit / modulo);
  size = B3DMIN(size, limit);
}

// Set the frames for loading a "center" from selected pieces
void EMmontageController::SetFramesForCenterOnly(int indX1, int indX2, int indY1, 
                                                 int indY2)
{
  mDefinedCenterFrames = true;
  mCenterIndX1 = indX1;
  mCenterIndX2 = indX2;
  mCenterIndY1 = indY1;
  mCenterIndY2 = indY2;
}

// Set the values of the given minioffset structure
void EMmontageController::SetMiniOffsetsParams(MiniOffsets &mini, int xNframes, 
  int xFrame, int xDelta, int yNframes, int yFrame, int yDelta)
{
  mini.xBase = mini.yBase = 0;
  mini.xNframes = xNframes;
  mini.xFrame = xFrame;
  mini.xDelta = xDelta;
  if (xNframes == 1)
    mini.xDelta = xFrame;
  else if (xNframes == 2)
    mini.xDelta = (xFrame + xDelta) / 2;
  else
    mini.xBase = (xFrame - xDelta) / 2;
  mini.yNframes = yNframes;
  mini.yFrame = yFrame;
  mini.yDelta = yDelta;
  if (yNframes == 1)
    mini.yDelta = yFrame;
  else if (yNframes == 2)
    mini.yDelta = (yFrame + yDelta) / 2;
  else
    mini.yBase = (yFrame - yDelta) / 2;
}

// Add minutes or seconds of remaining time if more than 3 pieces done
void EMmontageController::AddRemainingTime(CString & report)
{
  CString str;
  int remaining = B3DNINT(GetRemainingTime() / 1000);
  if (remaining > 0) {
    if (remaining < 180)
      str.Format(", %d sec", remaining);
    else
      str.Format(", %d min", (remaining + 30) / 60);
    report += str;
  }
}

// Compute number of pieces in a block in X and Y that gives closest to a square size
void EMmontageController::BlockSizesForNearSquare(int xSize, int ySize, int xOverlap, 
  int yOverlap, int blockSize, int &numInX, int &numInY)
{
  int xFull = blockSize * xSize - (blockSize - 1) * xOverlap;
  int yFull = blockSize * ySize - (blockSize - 1) * yOverlap;
  if (xFull < yFull) {
    numInX = blockSize;
    numInY = B3DNINT((xFull - yOverlap) / (float)(ySize - yOverlap));
  } else {
    numInY = blockSize;
    numInX = B3DNINT((yFull - xOverlap) / (float)(xSize - xOverlap));
  }
}

// Get "blockSize" number and number of pieces in X and Y with less than the allowed IS
void EMmontageController::ImageShiftBlockSizes(int sizeX, int sizeY, int xOverlap, 
  int yOverlap, float pixelSize, float maxISallowed, int &numInX, int &numInY)
{
  int xExtent, yExtent, xNum, yNum;
  double montIS;
  for (int num = 1; num < 100; num++) {
    BlockSizesForNearSquare(sizeX, sizeY, xOverlap, yOverlap, num, xNum, yNum);
    xExtent = (sizeX - xOverlap) * (xNum - 1);
    yExtent = (sizeY - yOverlap) * (yNum - 1);
    montIS = sqrt((double)xExtent * xExtent + yExtent * yExtent) * pixelSize / 2.;
    if (montIS > maxISallowed)
      break;
    numInX = xNum;
    numInY = yNum;
  }
}

// Retrun information on piece to script run during montage
int EMmontageController::GetCurrentPieceInfo(bool next, int &xPc, int &yPc, int &ixPc, 
  int &iyPc)
{
  int ind = mPieceIndex;
  if (mPieceIndex < 0)
    return 1;
  
  if (next)
    ind = NextPieceIndex();
  xPc = ind / mParam->yNframes;
  yPc = ind % mParam->yNframes;
  ixPc = (mParam->xFrame - mParam->xOverlap) * xPc;
  iyPc = (mParam->yFrame - mParam->yOverlap) * yPc;
  return 0;
}

//////////////////////////////////////////////
// ROUTINES TO HELP NAVIGATOR OPERATIONS
//////////////////////////////////////////////

// When Navigator Realigns to item, this adjusts a shift for correlating to
// the composed center??
void EMmontageController::AdjustShiftInCenter(MontParam *param, float &shiftX, 
                                              float &shiftY)
{

  int xTest, yTest, isave, minDist, minInd, xst, xnd, yst, ynd, xDist, yDist;

  // Convert shift to a position in montage coordinates
  xTest = mMontCenterX + (int)(param->xFrame / 2. + shiftX);
  yTest = mMontCenterY + (int)(param->yFrame / 2. - shiftY);

  // Loop on the center pieces finding the closest one
  minDist = 100000000;
  for (isave = 0; isave < mNumCenterSaved; isave++) {
    
    // Find limits of copy, in montage coordinates
    xst = B3DMAX(mMontCenterX, mCenterCoordX[isave]);
    xnd = B3DMIN(mMontCenterX, mCenterCoordX[isave]) + param->xFrame - 1;
    yst = B3DMAX(mMontCenterY, mCenterCoordY[isave]);
    ynd = B3DMIN(mMontCenterY, mCenterCoordY[isave]) + param->yFrame - 1;
 
    // Get the "distance" from point to piece, negative if inside
    if (xTest < xst)
      xDist = xst - xTest;
    else if (xTest > xnd)
      xDist = xTest - xnd;
    else
      xDist = -B3DMIN(xTest - xst, xnd - xTest);
    if (yTest < yst)
      yDist = yst - yTest;
    else if (yTest > ynd)
      yDist = yTest - ynd;
    else
      yDist = -B3DMIN(yTest - yst, ynd - yTest);
    xDist = B3DMAX(xDist, yDist);
    
    // Keep track of closest piece
    if (xDist < minDist) {
      minDist = xDist;
      minInd = isave;
    }
  }
  if (minDist >= 10000000)
    return;

  // Adjust shifts, the Y shift is in image not montage coords so subtract
  shiftX += mCenterShift[minInd][0];
  shiftY -= mCenterShift[minInd][1];
}

// Sets up mMoveInfo with X/Y (image shift or stage value) to move to for a given piece
// or for a focus block, also returns unbinned pixel distance from center of frame
void EMmontageController::ComputeMoveToPiece(int pieceInd, BOOL focusBlock, int &iDelX,
  int &iDelY, double &adjISX, double &adjISY)
{
  double postISX, postISY, stageAdjX, stageAdjY, regularX, regularY;
  int ubOffX, ubOffY, block;
  StageMoveInfo *moveInfo = &mMoveInfo;
  stageAdjX = stageAdjY = adjISX = adjISY = 0.;
  if (mImShiftInBlocks || focusBlock) {
    block = mFocusBlockInd[pieceInd];
    if (block < 0)
      block = -block - 2;
  }
  if (mImShiftInBlocks && !focusBlock) {
    iDelX = mParam->binning * ((mMontageX[pieceInd] - mBlockCenX[block]) +
      mPredictedErrorX);
    iDelY = mParam->binning * ((mMontageY[pieceInd] - mBlockCenY[block]) +
      mPredictedErrorY);
    postISX = mCamToIS.xpx * iDelX + mCamToIS.xpy * iDelY + mBaseISX;
    postISY = mCamToIS.ypx * iDelX + mCamToIS.ypy * iDelY + mBaseISY;
    moveInfo = &mBlockISMoveInfo;
  }

  if (mDoISrealign && !focusBlock) {
    GetAdjustmentsForISrealign(pieceInd, stageAdjX, stageAdjY, adjISX, adjISY,
      ubOffX, ubOffY);
    iDelX = mParam->binning * (mMontageX[pieceInd] - mMontCenterX);
    iDelY = mParam->binning * (mMontageY[pieceInd] - mMontCenterY);
  } else {
    iDelX = mParam->binning * (((focusBlock ? mBlockCenX[block] :
      mMontageX[pieceInd]) - mMontCenterX) + mPredictedErrorX);
    iDelY = mParam->binning * (((focusBlock ? mBlockCenY[block] :
      mMontageY[pieceInd]) - mMontCenterY) + mPredictedErrorY);
  }
  regularX = mBinv.xpx * iDelX + mBinv.xpy * iDelY +
    (mDoStageMoves ? mBaseStageX : mBaseISX) + stageAdjX;
  regularY = mBinv.ypx * iDelX + mBinv.ypy * iDelY +
    (mDoStageMoves ? mBaseStageY : mBaseISY) + stageAdjY;
  if (mImShiftInBlocks && !focusBlock) {
    mNominalStageX = regularX;
    mNominalStageY = regularY;
  } else {
    postISX = regularX;
    postISY = regularY;
  }

  // This is relevant only for post-action moves
  moveInfo->distanceMoved = B3DMAX(fabs(postISX - moveInfo->x), 
    fabs(postISY - moveInfo->y));
  moveInfo->x = postISX;
  moveInfo->y = postISY;
}

// Compute the adjustment to nominal stage position when doing an IS realign, the image
// shift for doing the alignment to an adjacent piece, and the expected image offset
// for autoalign to adjacent piece, in unbinned pixels
void EMmontageController::GetAdjustmentsForISrealign(int piece, double & stageAdjX, 
                                                     double & stageAdjY, double & adjISX,
                                                     double & adjISY, int & ubpixOffX,
                                                     int &ubpixOffY)
{
  int adj, shiftX, shiftY;
  ScaleMat bInv;
  float pixel;
  bool pcAbove, refAbove;
  double iDelX, iDelY, offTot;
  stageAdjX = stageAdjY = adjISX = adjISY = 0.;
  if (!mDoISrealign || piece >= mNumPieces)
    return;

  adj = mAdjacentForIS[piece];
  mAddedSubbedBacklash = 0;
  if (adj < 0)
    return;

  // Add the backlash adjustment if the piece is above the starting point of column
  // but the piece being aligned to is not; subtract it if opposite is the case
  if (mUsingAnchor) {
    pcAbove = piece % mParam->yNframes > mColumnStartY[piece / mParam->yNframes];
    refAbove = adj % mParam->yNframes > mColumnStartY[adj / mParam->yNframes];
    if (pcAbove && !refAbove)
      mAddedSubbedBacklash = 1;
    else if (!pcAbove && refAbove)
      mAddedSubbedBacklash = -1;
    stageAdjX = mAddedSubbedBacklash * mBacklashAdjustX;
    stageAdjY = mAddedSubbedBacklash * mBacklashAdjustY;
  }
  
  // Get nominal stage position of adjacent piece; adjustment is actual position minus
  // that.
  iDelX = mParam->binning * (mMontageX[adj] - mMontCenterX);
  iDelY = mParam->binning * (mMontageY[adj] - mMontCenterY);
  stageAdjX += mAcquiredStageX[adj] - 
    (mBinv.xpx * iDelX + mBinv.xpy * iDelY + mBaseStageX);
  stageAdjY += mAcquiredStageY[adj] - 
    (mBinv.ypx * iDelX + mBinv.ypy * iDelY + mBaseStageY);

  // Determine pixel distance between this piece and adjacent one, and total
  bInv = mShiftManager->CameraToIS(mParam->magIndex);
  shiftX = mParam->binning * (mMontageX[adj] - mMontageX[piece]);
  shiftY = mParam->binning * (mMontageY[adj] - mMontageY[piece]);
  ubpixOffX = ubpixOffY = 0;
  pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), mParam->magIndex);
  offTot = pixel * sqrt((double)shiftX * shiftX + shiftY * shiftY);

  // If this distance is too big, reduce the offset for image shift and set
  // expected image offset for autoalign
  if (offTot > mParam->maxRealignIS) {
    adj = (int)(shiftX * mParam->maxRealignIS / offTot);
    ubpixOffX = adj - shiftX;
    shiftX = adj;
    adj = (int)(shiftY * mParam->maxRealignIS / offTot);
    ubpixOffY = adj - shiftY;
    shiftY = adj;
  }

  // Get the image shift needed to get to adjacent position
  adjISX = bInv.xpx * shiftX + bInv.xpy * shiftY;
  adjISY = bInv.ypx * shiftX + bInv.ypy * shiftY;
}

/////////////////////////////////////////////////
// REALIGNING TO EXISTING PIECES OR ANCHORS
////////////////////////////////////////////////

// Start the process of realigning to an existing piece
int EMmontageController::RealignToExistingPiece(void)
{
  CString mess;
  double adjISX, adjISY;
  int iDelX, iDelY, timeout = 120000;

  // Get the piece to realign to
  if (mBufferManager->CheckAsyncSaving())
    return 1;
  if (mDoISrealign) {
    mRealignInd = mAdjacentForIS[mPieceIndex];
    if (mAddedSubbedBacklash)
      SEMTrace('S', "%s backlash to position adjustment", 
      mAddedSubbedBacklash > 0 ? "Added" : "Subtracted");
  } else
    mRealignInd = FindPieceForRealigning(mPieceX, mPieceY);

  if (mRealignInd < 0) {
    mess.Format("Cannot find appropriate piece for realigning to before piece"
      " %d, %d; just continuing", mPieceX, mPieceY);
    mWinApp->AppendToLog(mess);
    return 1;
  }

  // Read it in if it is not the previous piece already in A
  if (!(mImBufs->GetSaveCopyFlag() < 0 && mImBufs->mCaptured && 
    mImBufs->mConSetUsed == MONTAGE_CONSET && 
    mImBufs->mSecNumber == mPieceSavedAt[mRealignInd])) {
    if (mBufferManager->ReadFromFile(mWinApp->mStoreMRC, mPieceSavedAt[mRealignInd], 0, 
      true)) {
      mess.Format("Failed to read piece selected for realigning to before piece"
        " %d, %d; just continuing", mPieceX, mPieceY);
      mWinApp->AppendToLog(mess);
      return 1;
    }
  }

  if (mDoISrealign) {
    RealignNextTask(ISALIGN_ACQUIRE);
    return 0;
  }

  // Go to the location with backlash
  ComputeMoveToPiece(mRealignInd, false, iDelX, iDelY, adjISX, adjISY);
  mScope->MoveStage(mMoveInfo, true);
  SEMTrace('S', "RealignToPiece moving stage to %.3f %.3f", mMoveInfo.x, mMoveInfo.y);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE_REALIGN, 
    EXIST_CHECKPOS_ACQUIRE, timeout);
  return 0;
}

// Start the realignment to an anchor
int EMmontageController::RealignToAnchor(void)
{
  int timeout = 120000;

  // Reread the image
  if (UtilOpenFileReadImage(mAnchorFilename, "anchor")) {
    StopMontage();
    return 1;
  }

  // Move stage to anchor position
  mMoveInfo.x = mAnchorStageX;
  mMoveInfo.y = mAnchorStageY;
  mScope->MoveStage(mMoveInfo, true);
  SEMTrace('S', "RealignToAnchor moving stage to %.3f %.3f", mMoveInfo.x, mMoveInfo.y);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE_REALIGN, 
    ANCHALI_CHECK_ACQUIRE, timeout);
  return 0;
}

// Do the next task when realigning to a piece
void EMmontageController::RealignNextTask(int param)
{
  int targetSize = B3DMAX(1024, mCamera->TargetSizeForTasks());
  float shiftX, shiftY;
  int i, extraWait = 200, expectX = 0, expectY = 0; 
  double ISX, ISY, sterr, stageAdjX, stageAdjY, adjISX, adjISY;
  int timeOut = 120000;
  bool stageErr = false;
  ScaleMat bInv;
  CFileStatus status;
  CString mess;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  int consNum = (param == ISALIGN_ACQUIRE && mUseTrialSetInISrealign) ? 
    TRIAL_CONSET : TRACK_CONSET;

  if (mPieceIndex < 0) {
    DoNextPiece(0);
    return;
  }

  if ((param == EXIST_CHECKPOS_ACQUIRE || param == ANCHALI_CHECK_ACQUIRE ||
    param == BACKLASH_CHECK_ACQUIRE) && mMaxStageError > 0.) {
    mScope->FastStagePosition(ISX, ISY, sterr);
    i = TestStageError(ISX, ISY, sterr);
    if (i > 0) {
      mess.Format("Stage is %.2f microns from intended position; %s", sterr,
        mNumStageErrors >= 3 ? "giving up on realigning" : "trying again");
      mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
      if (mNumStageErrors < 3) {
        mScope->MoveStage(mMoveInfo, true);
        mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE_REALIGN, param,
          timeOut);
        return;
      } else {
        stageErr = true;
        if (param != EXIST_CHECKPOS_ACQUIRE) {
          SEMMessageBox("Failed to move stage to proper position for anchor image");
          StopMontage();
          return;
        }
      }
    }
  }
  mNumStageErrors = 0;

  // Do all kinds of shots as long as there is not a stage error
  if ((param == EXIST_CHECKPOS_ACQUIRE || param == ISALIGN_ACQUIRE || 
    param == ANCHOR_ACQUIRE || param == BACKLASH_CHECK_ACQUIRE || 
    param == ANCHALI_CHECK_ACQUIRE) && !stageErr) {
      if (mUseContinuousMode && param != ISALIGN_ACQUIRE)
        mCamera->StopCapture(-1);

      // Take a shot based on Record area
      // 9/5/11: This was ANCHALI_CHECK_ACQUIRE but couldn't figure out why - backlash
      // shot does immediately follow the ANCHOR_ACQUIRE and surely doesn't need a new 
      // conset
      if (param != BACKLASH_CHECK_ACQUIRE)
        mWinApp->mComplexTasks->MakeTrackingConSet(conSet, targetSize, RECORD_CONSET);
      if (param == ANCHOR_ACQUIRE || param == ANCHALI_CHECK_ACQUIRE) {
        mWinApp->mComplexTasks->LowerMagIfNeeded(mParam->anchorMagInd, 0.75f, 0.5f, 
          TRACK_CONSET);
        mLoweredMag = true;
      }

      // This won't happen.  Even with extra wait of 4000 it gave big shifts: needs work
      if (mUseContinuousMode && param == ISALIGN_ACQUIRE) {
        if (mConSets[MONTAGE_CONSET].correctDrift)
          extraWait += (int)(750. * mConSets[MONTAGE_CONSET].exposure);
        mCamera->SetTaskFrameWaitStart(mShiftManager->AddIntervalToTickTime(
          GetTickCount(), extraWait));
        mWinApp->AddIdleTask(CCameraController::TaskGettingFrame, TASK_MONTAGE_REALIGN,
          param + 1, 0);
      } else {
        mCamera->InitiateCapture(consNum);
        mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MONTAGE_REALIGN, param + 1, 0);
      }
      return;    
  }

  // Process the alignment to existing piece with or without IS if there is no stage
  // error, if there is an error fall through and return to the normal flow
  if (param == EXIST_CHECKPOS_ACQUIRE || param == EXIST_ALIGN_SHOT || 
    param == ISALIGN_SHOT) {
    if (!stageErr) {
      mImBufs->mCaptured = BUFFER_TRACKING;
      if (param == ISALIGN_SHOT) {
        GetAdjustmentsForISrealign(mPieceIndex, stageAdjX, stageAdjY, adjISX, adjISY, 
          expectX, expectY);
        expectX /= mImBufs->mBinning;
        expectY /= -mImBufs->mBinning;
      }
      mShiftManager->AutoAlign(1, 0, false, 0, NULL, expectX, expectY);
      /*for (int jj = 0; jj < 3; jj++) {
      Sleep(300);
      mWinApp->SetCurrentBuffer(1);
      Sleep(300);
      mWinApp->SetCurrentBuffer(0);
      }*/

      // The shifts are stored peak positions, and above error was computed as negative
      // of a peak position, so take the negative but invert Y
      mImBufs->mImage->getShifts(shiftX, shiftY);
      shiftX *= ((float)mImBufs->mBinning) / mParam->binning;
      shiftY *= ((float)mImBufs->mBinning) / mParam->binning;
      if (fabs((double)shiftX) > mParam->maxAlignFrac * mParam->xFrame || 
        fabs((double)shiftY) > mParam->maxAlignFrac * mParam->yFrame) {
          mess.Format("Realignment shift of %.0f,%.0f is greater than maximum allowed "
            "shift;\r\n the position adjustment is not being changed", shiftX, shiftY);
          if (param == ISALIGN_SHOT) {
            mScope->SetImageShift(0., 0.);
            mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
          }
        } else if (param != ISALIGN_SHOT) {
          mPredictedErrorX -= B3DNINT(shiftX);
          mPredictedErrorY += B3DNINT(shiftY);
          mess.Format("The position adjustment is being changed by a realignment shift "
            "of %.0f,%.0f", shiftX, shiftY);
        } else {
          bInv = mShiftManager->CameraToIS(mParam->magIndex);    
          adjISX = -mParam->binning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
          adjISY = -mParam->binning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
          mScope->SetImageShift(adjISX, adjISY);
          mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
          mess.Format("Shift realigning %d %d to %d %d is %.0f,%.0f",
            mPieceX, mPieceY, mAdjacentForIS[mPieceIndex] / mParam->yNframes + 1, 
            mAdjacentForIS[mPieceIndex] % mParam->yNframes + 1, shiftX, shiftY);
        }
        mWinApp->AppendToLog(mess);
    }
    if (param == EXIST_CHECKPOS_ACQUIRE) {
      mPreMovedStage = false;
      mNeedBacklash = true;
    }
    mNumSinceRealign = 0;
    if (PieceStartsColumn(mPieceIndex))
      mNumDoneInColumn = 0;
    DoNextPiece(0);
    return;
  }

  // GOT AN ANCHOR IMAGE
  if (param == ANCHOR_SAVE_SHOT) {

    // Get the stage position
    mScope->GetStagePosition(mMoveInfo.x, mMoveInfo.y, ISX);

    // Save the anchor image to a file; save stage pos for anchor
    if (mNeedAnchor) {
      mAnchorStageX = mMoveInfo.x;
      mAnchorStageY = mMoveInfo.y;
      if (UtilSaveSingleImage(mAnchorFilename, "anchor", false)) {
        StopMontage();
        return;
      }
    }

    // Set up for move down column and back up
    mMoveInfo.backX = -3. * mBinv.xpy * mParam->binning * 
      (mParam->yFrame - mParam->yOverlap);
    mMoveInfo.backY = -3. * mBinv.ypy * mParam->binning * 
      (mParam->yFrame - mParam->yOverlap);
    mScope->MoveStage(mMoveInfo, true);
    SEMTrace('S', "RealignNext doing backlash of %.3f %.3f", mMoveInfo.backX, 
      mMoveInfo.backY);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_MONTAGE_REALIGN, 
      BACKLASH_CHECK_ACQUIRE, timeOut);
    return;
  }

  // GOT BACKLASH IMAGE OR REALIGN IMAGE AT ANCHOR POSITION
  if (param == BACKLASH_SHOT || param == ANCHALI_ALIGN_SHOT) {
    mImBufs->mCaptured = BUFFER_TRACKING;

    // The binnings need to be equalized because a read-in image gets record binning
    mImBufs[1].mBinning = mImBufs->mBinning;
    mShiftManager->AutoAlign(1, 0, false);
    bInv = MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), 
      mParam->anchorMagInd));

    // Compute the stage move required to realign from this shift (invert Y)
    mImBufs->mImage->getShifts(shiftX, shiftY);
    adjISX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
    adjISY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
    mWinApp->mComplexTasks->RestoreMagIfNeeded();
    mLoweredMag = false;
    mMoveInfo.backX = mMoveBackX;
    mMoveInfo.backY = mMoveBackY;

    // For backlash shot, save the adjustment and go back to moving stage to piece
    if (param == BACKLASH_SHOT) {
      mBacklashAdjustX = adjISX;
      mBacklashAdjustY = adjISY;

      // If this is a resume and we only need backlash, then we need to set the
      // backlash flag for moving to a piece in lower have but not in upper
      if (!mNeedAnchor)
        mNeedBacklash = mPieceIndex % mParam->yNframes <= 
        mColumnStartY[mPieceIndex / mParam->yNframes];
      mNeedColumnBacklash = false;
      mNeedAnchor = false;
      mAction = B3DMIN(mAction, MOVE_STAGE - 1);
      SEMTrace('1', "RealignNext: backlash adjustment is %.3f %.3f", adjISX, adjISY); 
    } else {

      // For anchor realignment, need to adjust the acquired locations
      SEMTrace('1', "RealignNext: Adjusting acquired by %.3f %.3f", adjISX, adjISY); 
      for (i = 0; i < mNumPieces; i++) {
        if (mPieceSavedAt[i] >= 0) {
          mAcquiredStageX[i] += adjISX;
          mAcquiredStageY[i] += adjISY;
        }
      }
      mNeedRealignToAnchor = false;
    }
    DoNextPiece(0);
    return;
  }

}

// Evaluate whether to remind about image shift calibrations if doing IS realign
bool EMmontageController::CalNeededForISrealign(MontParam * param)
{
  float shiftX, shiftY, pixel;
  float overlapTol = 0.1f;  // Tolerance as fraction of overlap zone
  float maxISerr = 0.025f;    // Maximum expected IS cal error
  int *activeList = mWinApp->GetActiveCameraList();
  if (!param->moveStage || !param->useHqParams || !param->ISrealign || 
    param->setupInLowDose)
    return false;
  pixel = mShiftManager->GetPixelSize(activeList[param->cameraIndex], 
    param->magIndex) * param->binning;
  shiftX = B3DMIN(pixel * param->xFrame, param->maxRealignIS);
  shiftY = B3DMIN(pixel * param->yFrame, param->maxRealignIS);
  return (pixel * param->xOverlap * overlapTol < shiftX * maxISerr ||
    pixel * param->yOverlap * overlapTol < shiftY * maxISerr);
}

// Find an appropriate piece for realigning to
int EMmontageController::FindPieceForRealigning(int pieceX, int pieceY)
{
  int ynf = mParam->yNframes;
  int numBlocksX, ixbst, numx, ixBlock;
  int ix, iy, iyt, first, last, iDir, i, ind, xorig = pieceX;

  // Adjust to left side of block if focusing in blocks : find limits of block it is in
  if (mFocusInBlocks) {
    numBlocksX = 1 + (mParam->xNframes - 1) / mParam->focusBlockSize;
    ixbst = 1;
    for (ixBlock = 0; ixBlock < numBlocksX; ixBlock++) {
      numx = mParam->xNframes / numBlocksX;
      if (ixBlock < mParam->xNframes % numBlocksX)
        numx++;
      if (pieceX >= ixbst && pieceX < ixbst + numx) {
        pieceX = ixbst;
        break;
      }
      ixbst += numx;
    }
  }

  if (mAlignToNearestPiece) {

    // First try to find nearest piece in adjacent column
    ix = -1;
    for (i = 0; i < ynf - 1 && ix < 0; i++) {
      for (iDir = -1; iDir <= 1; iDir += 2) {
        iy = pieceY + iDir * i;
        ind = PieceIndexFromXY(pieceX - 1, iy);
        if (ind >= 0 && mPieceSavedAt[ind] >= 0) {
          ix = pieceX - 1;
          break;
        }
      }
    }

    // If that fails take the previous piece in this column
    if (ix < 0) {
      ind = PieceIndexFromXY(pieceX, pieceY - 1);
      return ((ind >= 0 && mPieceSavedAt[ind] >= 0) ? ind : -1);
    }

    // Otherwise back off from the edge of the montage if possible
    for (i = 1; i <= 2; i++) {
      for (iDir = -1; iDir <= 1; iDir += 2) {
        ind = PieceIndexFromXY(ix, iy + iDir);

        // If piece exists 2 or 3 away in one direction (i+1) and does not exist 1 or 2
        // away in the other direction, move in that direction as long as piece exists
        if (PieceIndexFromXY(ix, iy + (i + 1) * iDir) >= 0 &&
          ind >= 0 && mPieceSavedAt[ind] >= 0 && PieceIndexFromXY(ix, iy - i * iDir) < 0)
          iy += iDir;
      }
    }
  } else {

    // Align to middle of a previous column: try 3 columns back
    // Look for first and last piece in column
    for (ix = pieceX - 3; ix < pieceX; ix++) {
      first = 0;
      for (iy = 1; iy <= ynf; iy++) {
        ind = PieceIndexFromXY(ix, iy);
        if (ind >= 0 && mPieceSavedAt[ind] >= 0) {
          if (!first)
            first = iy;
          last = iy;
        }
      }

      // If found anything, find piece nearest to middle of column
      iy = -1;
      if (first) {
        for (i = 0; i < ynf - 1 && iy < 0; i++) {
          for (iDir = 1; iDir >= -1; iDir -= 2) {
            iyt = (first + last) / 2 + iDir * i;
            ind = PieceIndexFromXY(ix, iyt);
            if (ind >= 0 && mPieceSavedAt[ind] >= 0) {
              iy = iyt;
              break;
            }
          }
        }

        if (ix == pieceX - 1)
          break;

        // If there is a piece 2 to left, as well as 2 below and above (sort of), 
        // this column is OK; otherwise go try next.
        if (iy > 0 && iy - first > 1 && last - iy > 1) {
          ind = PieceIndexFromXY(ix - 2, iy);
          if (ind >= 0 && mPieceSavedAt[ind] >= 0)
            break;
        }
      }
    }
  }
  if (iy < 0)
    return -1;
  SEMTrace('1', "Realigning to piece %d %d for piece %d %d (adjusted X %d)", ix, iy, 
    xorig, pieceY, pieceX);
  return PieceIndexFromXY(ix, iy);
}

// Find the adjacent pieces to be used during IS realign, for all pieces
void EMmontageController::FindAdjacentForISrealign(void)
{
  int ind, ipc, pcx, pcy, apcx, apcy, look, del, dlook, seq;
  int ixdel[4] = {-1, 1, 0, 0};
  int iydel[4] = {0, 0, -1, 1};
  int toEdge[2], adjPiece[2], maxLook = 3;
  std::vector<int> seqnum;
  seqnum.resize(mNumPieces);

  // Build list of sequence numbers for pieces
  for (ind = 0; ind < mNumPieces; ind++)
    seqnum[mIndSequence[ind]] = ind;

  for (seq = 0; seq < mNumPieces; seq++) {
    ipc = mIndSequence[seq];
    mAdjacentForIS[ipc] = -1;
    if (mPieceToVar[ipc] < 0)
      continue;

    toEdge[0] = toEdge[1] = 0;
    pcx = ipc / mParam->yNframes + 1;
    pcy = ipc % mParam->yNframes + 1;

    // Look in the 4 directions for an adjacent piece with lower sequence number
    for (del = 0; del < 4; del++) {
      apcx = pcx + ixdel[del];
      apcy = pcy + iydel[del];
      ind = PieceIndexFromXY(apcx, apcy);
      if (ind >= 0 && mPieceToVar[ind] >= 0 && seqnum[ind] < seqnum[ipc]) {

        // Now look in each direction for how far it is to edge
        adjPiece[del / 2] = ind;
        toEdge[del/2] = maxLook + 1;
        for (dlook = 0; dlook < 4; dlook++) {
          for (look = 1; look <= maxLook; look++) {
            ind = PieceIndexFromXY(apcx + look*ixdel[dlook], apcy + look*iydel[dlook]);
            if (ind < 0) {
              toEdge[del/2] = B3DMIN(toEdge[del/2], look);
              break;
            }
          }
        }
      }
    }

    // Take the direction that is farther to an edge
    // If they are equal, alternate between X and Y in column and stagger this
    // between columns
    if (toEdge[0] || toEdge[1]) {
      if (toEdge[0] > toEdge[1])
        mAdjacentForIS[ipc] = adjPiece[0];
      else if (toEdge[1] > toEdge[0])
        mAdjacentForIS[ipc] = adjPiece[1];
      else
        mAdjacentForIS[ipc] = adjPiece[(pcx + pcy) % 2];
      //SEMTrace('1', "For %d %d  align to %d %d", pcx, pcy, mAdjacentForIS[ipc] / 
        //mParam->yNframes + 1, mAdjacentForIS[ipc] % mParam->yNframes + 1);
    } //else
      //SEMTrace('1', "No realign for %d %d", pcx, pcy);
  }
}


//////////////////////////////////////////////////////////////
// FINDING BEST SHIFTS, RETILING, AND AUTODOC RELATED ROUTINES
//////////////////////////////////////////////////////////////

// Call the IMOD library routine to find piece shifts, return various error measures
int EMmontageController::FindBestShifts(int nvar, float *upperShiftX, float *upperShiftY,
                                        float *bMat, float &bMean, float &bMax, 
                                        float &aMean, float &aMax, float &wMean, 
                                        float &wMax, bool tryAltern)
{
  int *ixpclist, *iypclist, *pieceLower, *pieceUpper, *edgeLower, *edgeUpper;
  int *ifSkipEdge, *ivarpc, *indvar, *usedAltern;
  float *dxedge, *dyedge, *ccc, *outlier, *work, *alternShifts = NULL;
  int i, j, inde, ixy, lower, upper, upVar, maxEdges, nSum, nedge[2], retval = -1;
  int numIter, numEdge, altIxy;
  float dx, dy, bDist, aDist, aSum, bSum, median;
  float outlierCrit = 2.5f, sdOutlierCrit = 2.24f, sdMedianRatio = 0.25f;
  float lowerMedRatio = 0.1f, upperMedRatio = 0.66f;  // 9/17/14: lowered from 0.8

                          //  Values are from midas ; blendmont
  int maxIter =  100 + nvar * 10;     // 20 + nvar; 100 + nvar * 10
  int numAvgForTest = 10; // 7;  10
  int intervalForTest = 100; // 50; 100
  float critMaxMove = 1.e-4f;  // 5.e-4f;  1.e-4
  float critMoveDiff = 1.e-6f;  // 5.e-6f; 1.e-6
  float robustCrit = nvar > 10 ? 1. : 0.;

  // Count edges
  for (ixy = 0; ixy < 2; ixy++) {
    nedge[ixy] = 0;
    for (i = 0; i < nvar; i++)
      if (UpperIndex(mVarToPiece[i], ixy) >= 0)
        nedge[ixy]++;
  }
  maxEdges = B3DMAX(nedge[0], nedge[1]);

  // Allocate arrays
  NewArray(ixpclist, int, mNumPieces);
  NewArray(iypclist, int, mNumPieces);
  NewArray(indvar, int, mNumPieces);
  NewArray(ivarpc, int, nvar);
  NewArray(dxedge, float, maxEdges * 2);
  NewArray(dyedge, float, maxEdges * 2);
  NewArray(pieceLower, int, maxEdges * 2);
  NewArray(pieceUpper, int, maxEdges * 2);
  NewArray(ifSkipEdge, int, maxEdges * 2);
  NewArray(edgeLower, int, 2 * mNumPieces);
  NewArray(edgeUpper, int, 2 * mNumPieces);
  NewArray(work, float, 21 * nvar + 1);
  if (tryAltern)
    NewArray(alternShifts, float, maxEdges * 8);
  if (ixpclist && iypclist && dxedge && dyedge && pieceLower && pieceUpper &&
    edgeLower && edgeUpper && work && ifSkipEdge && indvar && ivarpc &&
    (!tryAltern || alternShifts)) {

    // Fill the arrays
    for (i = 0; i < mNumPieces; i++) {
      ixpclist[i] = mMontageX[i];
      iypclist[i] = mMontageY[i];
      indvar[i] = mPieceToVar[i];
      edgeLower[2*i] = edgeLower[2*i+1] = -1;
      edgeUpper[2*i] = edgeUpper[2*i+1] = -1;
    }
    for (i = 0; i < nvar; i++) {
      ivarpc[i] = mVarToPiece[i];
    }
    for (i = 0; i < 2 * maxEdges; i++) {
      ifSkipEdge[i] = 0;
      pieceLower[i] = pieceUpper[i] = -1;
    }
    numEdge = 0;
    altIxy = 4 * maxEdges;
    ccc = (float *)work;
    outlier = (float *)(work + 4 * nvar);
    for (ixy = 0; ixy < 2; ixy++) {
      inde = 0;
      for (i = 0; i < nvar; i++) {
        lower = mVarToPiece[i];
        upper = UpperIndex(lower, ixy);
        if (upper >= 0) {
          edgeLower[2 * upper + ixy] = inde;
          edgeUpper[2 * lower + ixy] = inde;
          pieceLower[2 * inde + ixy] = lower;
          pieceUpper[2 * inde + ixy] = upper;
          dxedge[2 * inde + ixy] = upperShiftX[2 * lower + ixy];
          dyedge[2 * inde + ixy] = upperShiftY[2 * lower + ixy];
          if (tryAltern) {
            for (j = 0; j < 4; j++)
              alternShifts[4 * inde + altIxy * ixy + j] = 
              mAlternShifts[4 * (2 * lower + ixy) + j];
          }
          inde++;
          if (mNumXcorrPeaks > 1)
            ccc[numEdge++] = mPatchCCC[2 * lower + ixy];
        }
      }
    }

    // Get the median max SD and analyze for outliers
    if (mTrimmedMaxSDs.size() > 5) {
      rsMedian(&mTrimmedMaxSDs[0], (int)mTrimmedMaxSDs.size(), outlier, &median);
      rsMadMedianOutliers(&mTrimmedMaxSDs[0], (int)mTrimmedMaxSDs.size(), sdOutlierCrit, 
        outlier);
      for (i = 0; i < (int)mTrimmedMaxSDs.size(); i++) {
        if (outlier[i] < 0. && mTrimmedMaxSDs[i] <= sdMedianRatio * median) {
          ixy = mMaxSDToIxyOfEdge[i];
          inde = edgeUpper[2 * mMaxSDToPieceNum[i] + ixy];
          ifSkipEdge[2 * inde + ixy] = 1;
          SEMTrace('a', "Skipping edge with SD %f: %d pc %d ixy %d", mTrimmedMaxSDs[i], 
            inde, mMaxSDToPieceNum[i], ixy);
        }
      }
    }

    // Get the median CCC and analyze for outliers if very sloppy
    if (mNumXcorrPeaks > 1 && nvar > 10) {
      rsMedian(ccc, numEdge, outlier, &median);
      rsMadMedianOutliers(ccc, numEdge, outlierCrit, outlier);
      numEdge = 0;
      for (ixy = 0; ixy < 2; ixy++) {
        inde = 0;
        for (i = 0; i < nvar; i++) {
          lower = mVarToPiece[i];
          upper = UpperIndex(lower, ixy);
          if (upper >= 0) {

            // Skip an edge if it is an outlier or if its CCC is very low
            if (!ifSkipEdge[2 * inde + ixy] && ((outlier[numEdge] < 0. && ccc[numEdge] < 
              upperMedRatio * median) || ccc[numEdge] < 0.01 + lowerMedRatio * median)) {
              SEMTrace('a', "Skipping %d, %d %c edge, outlier %.0f  ccc %.4f  medcrit "
                "%.4f", lower / mParam->yNframes + 1, lower % mParam->yNframes + 1, 'X'+
                ixy, outlier[numEdge], ccc[numEdge], 0.01 + lowerMedRatio * median);
              ifSkipEdge[2 * inde + ixy] = 1;
            }
            numEdge++;
            inde++;
          }
        }
      }
    }

    // Try to substitute alternate shifts
    if (tryAltern) {
      usedAltern = (int *)work;
      j = 0;
      pickAlternativeShifts(ivarpc, nvar, indvar, dxedge, dyedge, pieceLower, pieceUpper,
        ifSkipEdge, 1, edgeLower, edgeUpper, 1, 0, alternShifts, 2, altIxy, 3., 0.33f,
        15., usedAltern, &j);
    }

    // Do the fit
    retval = findPieceShifts(ivarpc, nvar, indvar, ixpclist, iypclist, dxedge, dyedge,
      1, pieceLower, pieceUpper, ifSkipEdge, 1, bMat, 1, edgeLower, edgeUpper, 1, work,
      0, -1, 1, robustCrit, critMaxMove, critMoveDiff, maxIter, numAvgForTest, 
      intervalForTest, &numIter, &wMean, &wMax);
    if (!retval) {

      // Compute error before and unweighted error after
      bSum = 0.;
      bMax = 0.;
      aSum = 0.;
      aMax = 0.;
      nSum = 0;
      for (ixy = 0; ixy < 2; ixy++) {
        for (i = 0; i < nvar; i++) {
          lower = mVarToPiece[i];
          upper = UpperIndex(lower, ixy);
          if (upper >= 0) {
            upVar = mPieceToVar[upper];
            dx = upperShiftX[2*lower+ixy];
            dy = upperShiftY[2*lower+ixy];
            bDist = (float)sqrt((double)(dx * dx + dy * dy));
            bSum += bDist;
            if (bMax < bDist)
              bMax = bDist;
            dx += bMat[2 * upVar] - bMat[2 * i];
            dy += bMat[1 + 2 * upVar] - bMat[1 + 2 * i];
            aDist = (float)sqrt((double)(dx * dx + dy * dy));
            aSum += aDist;
            if (aMax < aDist)
              aMax = aDist;
            nSum++;
          }
        }
      }
      bMean = bSum / B3DMAX(1, nSum);
      aMean = aSum / B3DMAX(1, nSum);

    }
  }

  delete [] indvar;
  delete [] ivarpc;
  delete [] ifSkipEdge;
  delete [] ixpclist;
  delete [] iypclist;
  delete [] dxedge;
  delete [] dyedge;
  delete [] pieceLower;
  delete [] pieceUpper;
  delete [] edgeLower;
  delete [] edgeUpper;
  delete [] work;
  delete [] alternShifts;
  return retval;
}

// Read or write the edge shifts from/to an autodoc file if one exists
int EMmontageController::AutodocShiftStorage(bool write, float * upperShiftX, 
                                             float * upperShiftY)
{
  char *keys[8] = {ADOC_XEDGE, ADOC_YEDGE, ADOC_XEDGEVS, ADOC_YEDGEVS, ADOC_EDGEX_MAXSD,
  ADOC_EDGEY_MAXSD, ADOC_EDGEXVS_MAXSD, ADOC_EDGEYVS_MAXSD};
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  KImageStore *store = mReadingMontage ? mReadStoreMRC : mWinApp->mStoreMRC;
  int ipc, ixy, keyInd, nameInd, retval = 0, iz, ind;
  float sdVal;
  int adocInd = store->GetAdocIndex();
  if (adocInd < 0)
    return -1;
  if (AdocGetMutexSetCurrent(adocInd) < 0)
    return 1;
  nameInd = store->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
  for (ipc = 0; ipc < mNumPieces; ipc++) {
    iz = mPieceSavedAt[ipc];
    if (iz < 0)
      continue;
    for (ixy = 0; ixy < 2; ixy++) {
      keyInd = ixy + (mVerySloppy ? 2 : 0);
      if (UpperIndex(ipc, ixy) >= 0) {
        if (mNumXcorrPeaks > 1) {
          if (write)
            retval = AdocSetThreeFloats(names[nameInd], iz, keys[keyInd], 
            upperShiftX[2*ipc + ixy], upperShiftY[2*ipc + ixy], mPatchCCC[2 * ipc + ixy]);
          else
            retval = AdocGetThreeFloats(names[nameInd], iz, keys[keyInd], 
            &upperShiftX[2*ipc + ixy], &upperShiftY[2*ipc + ixy], 
              &mPatchCCC[2 * ipc + ixy]);

        } else {
          if (write)
            retval = AdocSetTwoFloats(names[nameInd], iz, keys[keyInd], 
            upperShiftX[2*ipc + ixy], upperShiftY[2*ipc + ixy]);
          else
            retval = AdocGetTwoFloats(names[nameInd], iz, keys[keyInd], 
            &upperShiftX[2*ipc + ixy], &upperShiftY[2*ipc + ixy]);
        }
        if (retval) {
          AdocReleaseMutex();
          return 2;
        }
        keyInd += 4;
        if (write) {

          // Find a trimmed max SD for this edge and write it
          for (ind = 0; ind < (int)mTrimmedMaxSDs.size(); ind++) {
            if (mMaxSDToIxyOfEdge[ind] == ixy && mMaxSDToPieceNum[ind] == ipc) {
              if (AdocSetFloat(names[nameInd], iz, keys[keyInd], mTrimmedMaxSDs[ind])) {
                AdocReleaseMutex();
                return 2;
              }
              break;
            }
          }
        } else {

          // Or read one in and store it and piece # and ixy in vectors
          if (!AdocGetFloat(names[nameInd], iz, keys[keyInd], &sdVal)) {
            mMaxSDToIxyOfEdge[mTrimmedMaxSDs.size()] = ixy;
            mMaxSDToPieceNum[mTrimmedMaxSDs.size()] = ipc;
            mTrimmedMaxSDs.push_back(sdVal);
          }
        }
      }
    }
  }

  if (write) {

    // Release the mutex before checking for async saving if writing
    AdocReleaseMutex();
    if (mBufferManager->CheckAsyncSaving())
      return 3;
    if (AdocGetMutexSetCurrent(adocInd) < 0)
      return 1;
    if (store->getStoreType() != STORE_TYPE_HDF && 
      AdocWrite((char *)(LPCTSTR)store->getAdocName()) < 0)
      retval = 3;
  }
  AdocReleaseMutex();
  return retval;
}

// Read or write stage offsets for one or all pieces
int EMmontageController::AutodocStageOffsetIO(bool write, int pieceInd)
{
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  KImageStore *store = mReadingMontage ? mReadStoreMRC : mWinApp->mStoreMRC;
  int ipc, nameInd, retval = 0, iz, ipcStr, ipcEnd;
  int adocInd = store->GetAdocIndex();
  if (adocInd < 0)
    return -1;
  if (AdocGetMutexSetCurrent(adocInd) < 0)
    return 1;
  nameInd = store->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
  ipcStr = pieceInd < 0 ? 0 : pieceInd;
  ipcEnd = pieceInd < 0 ? mNumPieces : pieceInd + 1;
  for (ipc = ipcStr; ipc < ipcEnd; ipc++) {
    iz = mPieceSavedAt[ipc];
    if (iz < 0)
      continue;
    if (write)
      retval = AdocSetTwoFloats(names[nameInd], iz, ADOC_STAGEOFF, mStageOffsetX[ipc],
        mStageOffsetY[ipc]);
    else
      retval = AdocGetTwoFloats(names[nameInd], iz, ADOC_STAGEOFF, &mStageOffsetX[ipc],
        &mStageOffsetY[ipc]);
    if (retval) {
      AdocReleaseMutex();
      return 2;
    }
  }
  if (write && pieceInd < 0) {

    // This may now be unneeded
    if (mBufferManager->CheckAsyncSaving())
      return 3;
    if (store->getStoreType() != STORE_TYPE_HDF && 
      AdocWrite((char *)(LPCTSTR)store->getAdocName()) < 0)
      retval = 3;
  }
  AdocReleaseMutex();
  return retval;
}

// Save aligned coordinates in the autodoc
int EMmontageController::StoreAlignedCoordsInAdoc(void)
{
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  char *keys[2] = {ADOC_ALI_COORD, ADOC_ALI_COORDVS};
  KImageStore *store = mReadingMontage ? mReadStoreMRC : mWinApp->mStoreMRC;
  int ipc, keyInd, nameInd, retval = 0, iz, pcX, pcY, pcZ, varInd;
  int adocInd = store->GetAdocIndex();
  if (adocInd < 0)
    return -1;

  // Have to finish the I/O or last section won't be there yet
  if (mBufferManager->CheckAsyncSaving())
    return 3;
  if (AdocGetMutexSetCurrent(adocInd) < 0)
    return 1;
  nameInd = store->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
  keyInd = mVerySloppy ? 1 : 0;
  for (ipc = 0; ipc < mNumPieces; ipc++) {
    iz = mPieceSavedAt[ipc];
    if (iz < 0)
      continue;
    varInd = mPieceToVar[ipc] * 2;
    retval = AdocGetThreeIntegers(names[nameInd], iz, ADOC_PCOORD, &pcX, &pcY, &pcZ);
    if (!retval)
      retval = AdocSetThreeIntegers(names[nameInd], iz, keys[keyInd], 
      pcX + B3DNINT(mBmat[varInd]), pcY + B3DNINT(mBmat[varInd + 1]), pcZ);
    if (retval) {
      AdocReleaseMutex();
      return 2;
    }
  }

  if (store->getStoreType() != STORE_TYPE_HDF && 
    AdocWrite((char *)(LPCTSTR)store->getAdocName()) < 0)
    retval = 3;
  AdocReleaseMutex();
  return retval;
}

// Store percentile statistics for multigrid to analyze
int EMmontageController::StorePercentileStats(int pieceInd, float meanHigh, float midFrac,
  float rangeFrac)
{
  char *names[2] = {ADOC_ZVALUE, ADOC_IMAGE};
  int nameInd, retval = 0;
  int adocInd = mWinApp->mStoreMRC->GetAdocIndex();
  if (adocInd < 0)
    return -1;

  // Have to finish the I/O or last section won't be there yet
  if (mBufferManager->CheckAsyncSaving())
    return 3;
  if (AdocGetMutexSetCurrent(adocInd) < 0)
    return 1;
  nameInd = mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC ? 1 : 0;
  retval = AdocSetThreeFloats(names[nameInd], mPieceSavedAt[pieceInd], 
    ADOC_PCTL_STATS, meanHigh, midFrac, rangeFrac);
  AdocReleaseMutex();
  return retval;
}

// Save the extra data and all the values needed to define a map in a MontSection of the
// autodoc
int EMmontageController::MapParamsToAutodoc(void)
{
  int errSum = 0, index;
  ControlSet *conSet = &mConSets[mImBufs[1].mConSetUsed];
  FilterParams *filtParam = mWinApp->GetFilterParams();
  float backX = 0, backY = 0;
  CString str;
  float netShiftX, netShiftY, beamShiftX, beamShiftY, beamTiltX, beamTiltY;
  bool filtering = mCamParams[mImBufs[0].mCamera].GIF || mScope->GetHasOmegaFilter();
  int adocInd = mWinApp->mStoreMRC->GetAdocIndex();
  if (adocInd < 0)
    return -1;
  if (AdocGetMutexSetCurrent(adocInd) < 0)
    return 1;

  // Lookup or create the section
  index = AdocLookupByNameValue(ADOC_MONT_SECT, mImBufs[1].mSecNumber);
  if (index < 0) {
    str.Format("%d", mImBufs[1].mSecNumber);
    index = AdocAddSection(ADOC_MONT_SECT, (LPCTSTR)str);
  }
  if (index < 0) {
    AdocReleaseMutex();
    return 2;
  }

  errSum = KStoreADOC::SetValuesFromExtra(mImBufs[1].mImage, ADOC_MONT_SECT, index);
  AdocDeleteKeyValue(ADOC_MONT_SECT, index, ADOC_PCOORD);
  AdocDeleteKeyValue(ADOC_MONT_SECT, index, ADOC_NOMINAL_STAGE);
  errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_MONT_SIZE,
    mParam->xNframes * mParam->xFrame - (mParam->xNframes - 1) * mParam->xOverlap,
    mParam->yNframes * mParam->yFrame - (mParam->yNframes - 1) * mParam->yOverlap);
  errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_BUF_ISXY, mImBufs[1].mISX,
    mImBufs[1].mISY);
  errSum -= AdocSetInteger(ADOC_MONT_SECT, index, ADOC_PROBEMODE, mImBufs[1].mProbeMode);
  errSum -= AdocSetInteger(ADOC_MONT_SECT, index, ADOC_STAGE_MONT, 
    mDoStageMoves ? 1 : 0);
  errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_CONSET_USED, 
    mImBufs[1].mConSetUsed, mImBufs[1].mLowDoseArea ? 1 : 0);
  GetLastBacklash(backX, backY);
  errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_MONT_BACKLASH, backX, backY);
  errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_VALID_BACKLASH, 
    mImBufs[1].mBacklashX, mImBufs[1].mBacklashY);
  errSum -= AdocSetFloat(ADOC_MONT_SECT, index, ADOC_DRIFT, conSet->drift);
  errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_CAM_MODES, conSet->shuttering,
    conSet->K2ReadMode);
  if (mImBufs[1].mLowDoseArea && IS_SET_VIEW_OR_SEARCH(mImBufs[1].mConSetUsed)) {
    CMapDrawItem item;
    item.mMapMagInd = mImBufs[1].mMagInd;
    item.mMapCamera = mImBufs[1].mCamera;
    errSum -= AdocSetFloat(ADOC_MONT_SECT, index, ADOC_FOCUS_OFFSET, 
      mImBufs[1].mViewDefocus);
    mWinApp->mNavHelper->GetViewOffsets(&item, netShiftX, netShiftY, beamShiftX, 
      beamShiftY, beamTiltX, beamTiltY, mCamera->ConSetToLDArea(mImBufs[1].mConSetUsed));
    errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_NET_VIEW_SHIFT, netShiftX, 
      netShiftY);
    errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_VIEW_BEAM_SHIFT, beamShiftX, 
      beamShiftY);
    errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_VIEW_BEAM_TILT, beamTiltX, 
      beamTiltY);
    errSum -= AdocSetFloat(ADOC_MONT_SECT, index, ADOC_VIEW_DEFOCUS, 
      mImBufs[1].mViewDefocus);
  }
  errSum -= AdocSetInteger(ADOC_MONT_SECT, index, ADOC_ALPHA, mScope->GetAlpha());
  errSum -= AdocSetTwoFloats(ADOC_MONT_SECT, index, ADOC_FILTER,
    (filtering && filtParam->slitIn) ? 1 : 0, filtering ? filtParam->slitWidth : 0.);
  errSum -= AdocSetInteger(ADOC_MONT_SECT, index, ADOC_FIT_POLY_ID,
    mParam->fitToPolygonID);
  errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_MONT_FRAMES, mParam->xNframes,
    mParam->yNframes);
  if (mAdjustmentScale) {
    errSum -= AdocSetFloat(ADOC_MONT_SECT, index, ADOC_ADJ_PIXEL,
     10000. * mImBufs[0].mPixelSize * mAdjustmentScale);
    errSum -= AdocSetFloat(ADOC_MONT_SECT, index, ADOC_ADJ_ROTATION,
      (mImBufs[1].mImage->GetUserData())->mAxisAngle + mAdjustmentRotation);
  }
  if (mUsingMultishot) {
    errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_ADJ_OVERLAPS,
      mAdjustedOverlaps[0], mAdjustedOverlaps[1]);
    errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_XEDGE_EXPECT,
      mExpectedShiftsX[0], mExpectedShiftsY[0]);
    errSum -= AdocSetTwoIntegers(ADOC_MONT_SECT, index, ADOC_YEDGE_EXPECT,
      mExpectedShiftsX[1], mExpectedShiftsY[1]);
  }

  // SetValue returns 1 for error, all the Adoc sets return -1

  if (mWinApp->mStoreMRC->getStoreType() != STORE_TYPE_HDF && 
    AdocWrite((char *)(LPCTSTR)mWinApp->mStoreMRC->getAdocName()) < 0)
    errSum -= 1000;
  AdocReleaseMutex();
  return errSum;
}

// If the store has an adoc, acquires the adoc mutex, looks up montage section, and
// makes sure it has keys; returns section number or -1 if any of that fails
int EMmontageController::AccessMontSectInAdoc(KImageStore *store, int secNum)
{
  int sectInd = -1;
  if (store->GetAdocIndex() >= 0) {
    if (!AdocGetMutexSetCurrent(store->GetAdocIndex()))
      sectInd = AdocLookupByNameValue(ADOC_MONT_SECT, secNum);
    if (sectInd >= 0 && !AdocGetNumberOfKeys(ADOC_MONT_SECT, sectInd))
      sectInd = -1;
    if (sectInd < 0)
      AdocReleaseMutex();
  }
  return sectInd;
}

// Get any parameters from the MontSection data that can go into the imBuf
void EMmontageController::MapParamsToOverview(int sectInd)
{ 
  int typext;
  float ISX, ISY;
  KStoreADOC::LoadExtraFromValues(mImBufs[1].mImage->GetUserData(), typext, 
    ADOC_MONT_SECT, sectInd);
  AdocGetTwoFloats(ADOC_MONT_SECT, sectInd, ADOC_BUF_ISXY, &ISX, &ISY);
  mImBufs[1].mISX = ISX;
  mImBufs[1].mISY = ISY;
  AdocGetInteger(ADOC_MONT_SECT, sectInd, ADOC_PROBEMODE, &mImBufs[1].mProbeMode);
  AdocGetTwoIntegers(ADOC_MONT_SECT, sectInd, ADOC_CONSET_USED, 
    &mImBufs[1].mConSetUsed, &typext);
  mImBufs[1].mLowDoseArea = typext != 0;
  AdocGetTwoFloats(ADOC_MONT_SECT, sectInd, ADOC_VALID_BACKLASH, 
    &mImBufs[1].mBacklashX, &mImBufs[1].mBacklashY);
  AdocGetFloat(ADOC_MONT_SECT, sectInd, ADOC_VIEW_DEFOCUS, &mImBufs[1].mViewDefocus);
}

// Adjust zoom or border/retiling strategy when get overview array
void EMmontageController::NextMiniZoomAndBorder(int &borderTry)
{
  // Drop the border size if any, or try for no border at all if zoom is > 1
  // I.e., when zoom is 1 we increase the zoom instead of using no deferred tiling
  if (borderTry > 0 || (mMiniZoom > 1 && borderTry == 0)) {
    borderTry--;
    return;
  }

  // Otherwise, increase the zoom and reinitialize the border trials
  mMiniZoom++;
  if (mParam->shiftInOverview && !mAlreadyHaveShifts)
    borderTry = 10;
}

// Recompose the overview image from the stored sampled pieces
void EMmontageController::RetilePieces(int miniType)
{
  int shiftY, outx, xstrt, xend, ystrt, yend, xtile, ytile, top, bottom, ix, iy;
  int ipc, ivar, unfilled, miniBaseX, miniBaseY, xofset, yofset;
  size_t outy;
  int xtrim = mParam->xOverlap / (10 * mMiniZoom);
  int ytrim = mParam->yOverlap / (10 * mMiniZoom);
  float useMat;

  // Evaluate the need for a Y shift
  shiftY = 0;
  for (iy = mParam->yNframes; iy > 0; iy--) {

    //Convert the bottom of this row of stored tiles into a line in final image
    bottom = (int)((((mParam->yNframes - iy) * (size_t)mMiniFrameY + mMiniBorderY) *
      mMiniArrayX) / mMiniSizeX);
    for (ix = 0; ix < mParam->xNframes; ix++) {
      ipc = ix * mParam->yNframes + iy - 1;
      ivar = mPieceToVar[ipc];
      if (ivar >= 0) {

        // Get top line of this frame, trim, limit to top of final image
        useMat = (mDoCorrelations || mUsingMultishot) ? mBmat[2 * ivar + 1] : 0.;
        top = (mParam->yNframes + 1 - iy) * mMiniFrameY - B3DNINT(useMat / mMiniZoom);
        if (iy > 1)
          top -= ytrim;
        top = B3DMIN(top, mMiniSizeY);

        // The shift is what is needed to keep the top below the bottom
        shiftY = B3DMAX(shiftY, top - bottom);
      }
    }
  }

  // Loop on rows
  unfilled = 0;
  for (iy = mParam->yNframes; iy > 0; iy--) {

    // Fill from the last line not filled to the bottom of this row of stored tiles
    bottom = (int)((((mParam->yNframes - iy) * (size_t)mMiniFrameY + mMiniBorderY) * 
      mMiniArrayX) / mMiniSizeX);
    bottom = B3DMIN(bottom, mMiniSizeY);
    for (outy = unfilled; outy < (size_t)bottom; outy++) {
      if (mConvertMini) {
        for (outx = 0; outx < mMiniSizeX; outx++)
          mMiniByte[outx + outy * mMiniSizeX] = (unsigned char)mMiniFillVal;
      } else if (miniType == kUSHORT) {
        for (outx = 0; outx < mMiniSizeX; outx++)
          mMiniUshort[outx + outy * mMiniSizeX] = (unsigned short)mMiniFillVal;
      } else if (miniType == kFLOAT) {
        for (outx = 0; outx < mMiniSizeX; outx++)
          mMiniFloat[outx + outy * mMiniSizeX] = mMiniFillVal;
      } else {
        for (outx = 0; outx < mMiniSizeX; outx++)
          mMiniData[outx + outy * mMiniSizeX] = (short)mMiniFillVal;
      }
    }
    unfilled = bottom;

    // loop across the row
    for (ix = 0; ix < mParam->xNframes; ix++) {
      ipc = ix * mParam->yNframes + iy - 1;
      ivar = mPieceToVar[ipc];
      if (ivar >= 0 && mPieceSavedAt[ipc] >= 0) {
        xtile = ix * mMiniFrameX;
        ytile = (mParam->yNframes - iy) * mMiniFrameY + mMiniBorderY;
        xstrt = 0;
        xend = xstrt + mMiniFrameX;
        ystrt = 0;
        yend = ystrt + mMiniFrameY;
        if (ix > 0)
          xstrt += xtrim;
        if (ix < mParam->xNframes - 1)
          xend -= xtrim;
        if (iy > 1)
          yend -= ytrim;
        if (iy < mParam->yNframes)
          ystrt += ytrim;
        miniBaseX = ix * mMiniDeltaX;
        miniBaseY = (mParam->yNframes - iy) * mMiniDeltaY; 

        // Get the final offset, set the minioffsets, then adjust for shift
        useMat = (mDoCorrelations || mUsingMultishot) ? mBmat[2 * ivar] : 0.;
        xofset = B3DNINT(useMat / mMiniZoom);
        useMat = (mDoCorrelations || mUsingMultishot) ? mBmat[2 * ivar + 1] : 0.;
        yofset = -B3DNINT(useMat / mMiniZoom);
        mMiniOffsets.offsetX[ipc] = xofset;
        mMiniOffsets.offsetY[ipc] = yofset;
        yofset -= shiftY;
        miniBaseX += xofset;
        miniBaseY += yofset;
 
        // If a base is negative, limit the start of the copy
        // If a base is beyond the limit for the composed montage, limit end of copy
        if (miniBaseX < 0)
          xstrt = -miniBaseX;
        if (miniBaseX > mMiniSizeX - mMiniFrameX)
          xend = mMiniSizeX - miniBaseX;
        if (miniBaseY < 0)
          ystrt = -miniBaseY;
        if (miniBaseY > mMiniSizeY - mMiniFrameY)
          yend = mMiniSizeY - miniBaseY;

        // Copy lines
        if (xstrt < xend && ystrt < yend) {
          for (outy = ystrt; outy < (size_t)yend; outy++) {
            size_t indout = (miniBaseY + outy) * mMiniSizeX + miniBaseX + xstrt;
            size_t indin = xstrt + xtile + (ytile + outy) * mMiniArrayX;
            if (mConvertMini) {
              for (outx = xstrt; outx < xend; outx++)
                mMiniByte[indout++] = mMiniByte[indin++];
            } else if (miniType == kFLOAT) {
              for (outx = xstrt; outx < xend; outx++)
                mMiniFloat[indout++] = mMiniFloat[indin++];
            } else {
              for (outx = xstrt; outx < xend; outx++)
                mMiniData[indout++] = mMiniData[indin++];
            }
          }
        }
      }
    }
  }

  // Shift to recenter if needed
  if (shiftY)
    ProcShiftInPlace(mMiniData, mConvertMini ? kUBYTE : miniType, mMiniSizeX, mMiniSizeY, 
       0, shiftY, mMiniFillVal);
}


void EMmontageController::AdjustFilter(int index)
{
  float value = mSloppyRadius1[index];
  if (!KGetOneFloat("NEGATIVE of radius at which low frequency filter starts to rise"
    " from 0:", value, 3))
    return;
  B3DCLAMP(value, -0.5, 0.);
  SetNeedBoxSetup(value != mSloppyRadius1[index]);
  mSloppyRadius1[index] = value;
  value = mSloppyRadius1[index];
  value = mRadius2[index];
  if (!KGetOneFloat("Radius at which high frequency filter starts to fall off:", value, 
    2))
    return;
  B3DCLAMP(value, 0.01, 0.5);
  SetNeedBoxSetup(value != mRadius2[index]);
  mRadius2[index] = value;
  value = mSigma1[index];
  if (!KGetOneFloat("Sigma for rise of low frequency filter from 0:", value, 3))
    return;
  B3DCLAMP(value, 0., 0.5);
  SetNeedBoxSetup(value != mSigma1[index]);
  mSigma1[index] = value;
  value = mSigma2[index];
  if (!KGetOneFloat("Sigma for falloff of high frequency filter:", value, 3))
    return;
  B3DCLAMP(value, 0., 0.5);
  SetNeedBoxSetup(value != mSigma2[index]);
  mSigma2[index] = value;
}

// Estimate the time remaining in the montage if there are at least 3 pieces done
double EMmontageController::GetRemainingTime()
{
  double interval;
  int numDone = mNumDoing - mInitialNumDoing - 1;
  int numTot = mNumPieces - mNumToSkip - mInitialNumDoing;
  if (mPieceIndex < 0 || numDone < 3 || numTot <= 0)
    return -1000.;

  // Return the same results as last time if working on same piece
  if (mNumDoing == mLastNumDoing)
    interval = mLastElapsed;
  else
    interval = SEMTickInterval(mStartTime);
  mLastElapsed = interval;
  mLastNumDoing = mNumDoing;
  return  interval * (numTot / (numDone - 1.) - 1.);
}

// Test for whether the stage position is sufficiently close to the target; if not 
// increment error count then try getting the position again after a wait; put out message
// and return -1 if that succeeds, or return 1 if that fails or count reaches 3
int EMmontageController::TestStageError(double ISX, double ISY, double &sterr)
{
  ISX -= mMoveInfo.x;
  ISY -= mMoveInfo.y;
  sterr = sqrt(ISX * ISX + ISY * ISY);
  if (sterr <= mMaxStageError)
    return 0;
  mNumStageErrors++;
  if (mNumStageErrors < 3) {
    Sleep(1000);    // In case done signal is in too soon
    mScope->WaitForStageReady(10000);
    mScope->GetStagePosition(ISX, ISY, sterr);
    ISX -= mMoveInfo.x;
    ISY -= mMoveInfo.y;
    sterr = sqrt(ISX * ISX + ISY * ISY);
    if (sterr <= mMaxStageError) {
      mWinApp->AppendToLog("WARNING: Initial stage position incorrect; discrepancy"
        " resolved by waiting and reading position again");
      return -1;
    }
  }
  return 1;
}

// Sets an image shift and tests whether it was clipped, stops montage unconditionally
int EMmontageController::SetImageShiftTestClip(double adjISX, double adjISY,
  float delayFac)
{
  SEMTrace('S', "Setting image shift for realign of %.3f, %.3f", adjISX, adjISY);
  mScope->SetImageShift(adjISX, adjISY);
  if (mScope->GetISwasClipped()) {
    mWinApp->AppendToLog("Montage stopped due to image shift being clipped.  The"
    " image shift for\r\n   realigning may be too large with current neutral "
      "values and IS offsets");
    if (!mScope->GetMessageWhenClipIS())
      StopMontage();
    return 1;
  }
  mShiftManager->SetISTimeOut(delayFac * mShiftManager->GetLastISDelay());
  return 0;
}

// Implements a change in what camera set to use with a value that is + to turn an option
// on or - to turn it off, 1 for MontMap, 2 for View in LD, 3 for Search in LD
void EMmontageController::ChangeParamSetToUse(MontParam *montP, int changeType)
{
  int index = B3DABS(changeType);
  if (index == 1)
    montP->useMontMapParams = changeType > 0;
  if (index == 2) {
    montP->useViewInLowDose = changeType > 0;
    if (montP->useViewInLowDose) {
      montP->useSearchInLowDose = false;
      montP->usePrevInLowDose = false;
    }
  }
  if (index == 3) {
    montP->useSearchInLowDose = changeType > 0;
    if (montP->useSearchInLowDose) {
      montP->useViewInLowDose = false;
      montP->usePrevInLowDose = false;
    }
  }
  if (index == 4) {
    montP->usePrevInLowDose = changeType > 0;
    if (montP->usePrevInLowDose) {
      montP->useSearchInLowDose = false;
      montP->useViewInLowDose = false;
    }
  }
}
