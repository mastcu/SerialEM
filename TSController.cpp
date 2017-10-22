// TSController.cpp:      The tilt series controller
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
#include "SerialEMView.h"
#include ".\TSController.h"
#include "TSSetupDialog.h"
#include "TSResumeDlg.h"
#include "TSBackupDlg.h"
#include "TSvariationsDlg.h"
#include "MultiTSTasks.h"
#include "TSExtraFile.h"
#include "ComplexTasks.h"
#include "ShiftManager.h"
#include "FocusManager.h"
#include "EMMontageController.h"
#include "CameraController.h"
#include "MacroProcessor.h"
#include "ProcessImage.h"
#include "MacroProcessor.h"
#include "BeamAssessor.h"
#include "LogWindow.h"
#include "Mailer.h"
#include "PluginManager.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "FalconHelper.h"
#include "StateDlg.h"
#include "FilterTasks.h"
#include "ThreeChoiceBox.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// The defines for the actions.  Order is arbitrary here, and numbers just have to
// be less than MAX_TS_ACTIONS
enum {LOOP_START_CHECKS = 1, CHECK_RESET_SHIFT, IMPOSE_VARIATIONS, CHECK_REFINE_ZLP, 
      COMPUTE_PREDICTIONS, LOW_MAG_FOR_TRACKING, ALIGN_LOW_MAG_SHOT, TRACKING_SHOT, 
      ALIGN_TRACKING_SHOT, AUTOFOCUS, CHECK_AUTOCENTER,
      POST_FOCUS_TRACKING, ALIGN_POST_FOCUS, RECORD_OR_MONTAGE, SAVE_RECORD_SHOT, 
      TRACK_BEFORE_LOWMAG_REF, ALIGN_LOWMAG_PRETRACK, LOW_MAG_REFERENCE, COPY_LOW_MAG_REF,
      LOWDOSE_PREVIEW, ALIGN_LOWDOSE_PREVIEW, LOWDOSE_TRACK_REF, COPY_LOWDOSE_REF, 
      TILT_IF_NEEDED, SAVE_AUTOFOCUS, ZEROTILT_TRACK_SHOT, ALIGN_ZEROTILT_TRACK, 
      ZEROTILT_PREVIEW, ALIGN_ZEROTILT_PREVIEW, WALK_UP, STARTUP_AUTOFOCUS, 
      SAVE_STARTUP_AUTOFOCUS, STARTUP_PREVIEW, ALIGN_STARTUP_PREVIEW, STARTUP_TRACK_SHOT,
      POST_WALKUP_SHOT, STARTUP_ALIGN_TRACK, REVERSE_TILT, EUCENTRICITY, CHECK_AUTOFOCUS, 
      EVALUATE_AUTOFOCUS, EXTRA_RECORD, SAVE_EXTRA_RECORD, FINISH_STARTUP, 
      BIDIR_ANCHOR_SHOT, BIDIR_INVERT_FILE, BIDIR_RETURN_TO_START, BIDIR_REALIGN_SHOT,
      BIDIR_CHECK_SHIFT, BIDIR_TRACK_SHOT, BIDIR_ALIGN_TRACK, BIDIR_RELOAD_REFS,
      BIDIR_LOWMAG_REF, BIDIR_COPY_LOWMAG_REF, BIDIR_AUTOFOCUS, BIDIR_REVERSE_TILT,
      BIDIR_FINISH_INVERT, GET_DEFERRED_SUM
};

static const char *actionText[] = 
{"LOOP_START_CHECKS", "CHECK_RESET_SHIFT", "IMPOSE_VARIATIONS", "CHECK_REFINE_ZLP", 
"COMPUTE_PREDICTIONS", "LOW_MAG_FOR_TRACKING", "ALIGN_LOW_MAG_SHOT", "TRACKING_SHOT", 
"ALIGN_TRACKING_SHOT", "AUTOFOCUS", "CHECK_AUTOCENTER", 
"POST_FOCUS_TRACKING", "ALIGN_POST_FOCUS", "RECORD_OR_MONTAGE", "SAVE_RECORD_SHOT", 
"TRACK_BEFORE_LOWMAG_REF", "ALIGN_LOWMAG_PRETRACK", "LOW_MAG_REFERENCE", "COPY_LOW_MAG_REF",
"LOWDOSE_PREVIEW", "ALIGN_LOWDOSE_PREVIEW", "LOWDOSE_TRACK_REF", "COPY_LOWDOSE_REF", 
"TILT_IF_NEEDED", "SAVE_AUTOFOCUS", "ZEROTILT_TRACK_SHOT", "ALIGN_ZEROTILT_TRACK", 
"ZEROTILT_PREVIEW", "ALIGN_ZEROTILT_PREVIEW", "WALK_UP", "STARTUP_AUTOFOCUS", 
"SAVE_STARTUP_AUTOFOCUS", "STARTUP_PREVIEW", "ALIGN_STARTUP_PREVIEW", "STARTUP_TRACK_SHOT",
"POST_WALKUP_SHOT", "STARTUP_ALIGN_TRACK", "REVERSE_TILT", "EUCENTRICITY", "CHECK_AUTOFOCUS", 
"EVALUATE_AUTOFOCUS", "EXTRA_RECORD", "SAVE_EXTRA_RECORD", "FINISH_STARTUP", 
"BIDIR_ANCHOR_SHOT", "BIDIR_INVERT_FILE", "BIDIR_RETURN_TO_START", "BIDIR_REALIGN_SHOT",
"BIDIR_CHECK_SHIFT", "BIDIR_TRACK_SHOT", "BIDIR_ALIGN_TRACK", "BIDIR_RELOAD_REFS",
"BIDIR_LOWMAG_REF", "BIDIR_COPY_LOWMAG_REF", "BIDIR_AUTOFOCUS", "BIDIR_REVERSE_TILT",
"BIDIR_FINISH_INVERT", "GET_DEFERRED_SUM"};

#define NO_PREVIOUS_ACTION   -1
#define SHIFTS_ON_ACQUIRE    2
#define BUFFERS_FOR_USER   3

// provisionally:
#define TRACKING_CONSET      2

#define TILT_BACKED_UP       1
#define IMAGE_SHIFT_RESET    2
#define USER_MOVED_STAGE     4
#define USER_STOPPED         8
#define USER_CHANGED_FOCUS  16
#define NEW_REFERENCE       32
#define DRIFTED_AT_STOP     64
#define REFINED_ZLP        128
#define SHIFT_DISTURBANCES (TILT_BACKED_UP | IMAGE_SHIFT_RESET | USER_MOVED_STAGE | NEW_REFERENCE | DRIFTED_AT_STOP)
#define FOCUS_DISTURBANCES (TILT_BACKED_UP | IMAGE_SHIFT_RESET | USER_MOVED_STAGE | NEW_REFERENCE | USER_CHANGED_FOCUS)

#define MAX_BACKUP_LIST_ENTRIES  20

// Here, list the default actions in order, regular loop then startup
// End with a zero
// IF YOU ADD TO THE STARTUP SECTION, BE SURE TO CHANGE mNumActStartup
static int actions[MAX_TS_ACTIONS] = {
  LOOP_START_CHECKS,
  CHECK_RESET_SHIFT,
  IMPOSE_VARIATIONS,
  CHECK_REFINE_ZLP,
  CHECK_AUTOCENTER,
  COMPUTE_PREDICTIONS,
  LOW_MAG_FOR_TRACKING,
  ALIGN_LOW_MAG_SHOT,
  TRACKING_SHOT,
  ALIGN_TRACKING_SHOT,
  AUTOFOCUS,
  SAVE_AUTOFOCUS,
  POST_FOCUS_TRACKING, 
  ALIGN_POST_FOCUS, 
  GET_DEFERRED_SUM,
  RECORD_OR_MONTAGE,
  SAVE_RECORD_SHOT,
  EXTRA_RECORD,
  SAVE_EXTRA_RECORD,
  TRACK_BEFORE_LOWMAG_REF, 
  ALIGN_LOWMAG_PRETRACK,
  LOW_MAG_REFERENCE,
  COPY_LOW_MAG_REF,
  LOWDOSE_PREVIEW,
  ALIGN_LOWDOSE_PREVIEW,
  LOWDOSE_TRACK_REF,
  COPY_LOWDOSE_REF,
  BIDIR_RELOAD_REFS,
  BIDIR_INVERT_FILE,
  BIDIR_RETURN_TO_START,
  BIDIR_REALIGN_SHOT,
  BIDIR_CHECK_SHIFT,
  BIDIR_REVERSE_TILT,
  BIDIR_AUTOFOCUS,
  BIDIR_TRACK_SHOT, 
  BIDIR_ALIGN_TRACK,
  BIDIR_LOWMAG_REF,
  BIDIR_COPY_LOWMAG_REF,
  BIDIR_FINISH_INVERT,
  TILT_IF_NEEDED,
  ZEROTILT_TRACK_SHOT,
  ALIGN_ZEROTILT_TRACK,
  ZEROTILT_PREVIEW,
  ALIGN_ZEROTILT_PREVIEW,
  EUCENTRICITY,
  WALK_UP,
  POST_WALKUP_SHOT,
  STARTUP_AUTOFOCUS,
  SAVE_STARTUP_AUTOFOCUS,
  STARTUP_PREVIEW,
  ALIGN_STARTUP_PREVIEW,
  STARTUP_TRACK_SHOT,
  STARTUP_ALIGN_TRACK,
  REVERSE_TILT,
  CHECK_AUTOFOCUS,
  EVALUATE_AUTOFOCUS,
  BIDIR_ANCHOR_SHOT,
  FINISH_STARTUP,
  0
};


/////////////////////////////////////////////////////////////////////////////
// CTSController
//
// Four steps for a class derived from CCmdTarget:
// 1. Comment out IMPLEMENT_DYNCREATE here
// 2. Comment out DECLARE_DYNCREATE in .h file
// 3. Make the constructor and destructor public in .h
// 4. Add call to OnCmdMsg in CSerialEMApp::OnCmdMsg
//
// IMPLEMENT_DYNCREATE(CTSController, CCmdTarget)

BEGIN_MESSAGE_MAP(CTSController, CCmdTarget)
  //{{AFX_MSG_MAP(CTSController)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTSController construction/destruction

CTSController::CTSController()
{
  int i, j;
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSets = mWinApp->GetConSets();
  mImBufs = mWinApp->GetImBufs();
  mMontParam = mWinApp->GetMontParam();
  mActiveCameraList = mWinApp->GetActiveCameraList();
  mFilterParams = mWinApp->GetFilterParams();
  mLDParam = mWinApp->GetLowDoseParams();
  mModeNames = mWinApp->GetModeNames();
  mTSParam.beamControl = BEAM_INTENSITY_FOR_MEAN;
  mTSParam.trackLowMag = false;
  for (j = 0; j < 3; j++) {
    mTSParam.lowMagIndex[j] = 0;
    mTSParam.magIndex[j] = 0;
    mTSParam.bidirAnchorMagInd[2 * j] = mTSParam.bidirAnchorMagInd[2 * j + 1] = 0;
  }
  mTSParam.refineEucen = true;
  mTSParam.leaveAnchor = true;
  mTSParam.anchorTilt = 45.;
  mTSParam.useAnchor = false;
  mTSParam.refIsInA = true;
  mTSParam.ISlimit = 1.5;
  mTSParam.focusCheckInterval = 5.;
  mTSParam.alwaysFocusHigh = false;
  mTSParam.alwaysFocusAngle = 50.;
  mTSParam.taperAngle = 50.;
  mTSParam.taperCounts = false;
  mTSParam.highCounts = 4000;
  mTSParam.meanCounts = 5000;
  mTSParam.limitIntensity = false;
  mTSParam.cosinePower = 1;
  mTSParam.repeatRecord = true;
  mTSParam.confirmLowDoseRepeat = true;
  mTSParam.maxRecordShift = 0.03f;
  mTSParam.stopOnBigAlignShift = false;
  mTSParam.maxAlignShift = 30.f;
  mTSParam.repeatAutofocus = true;
  mTSParam.maxFocusDiff = 0.25f;
  mTSParam.checkAutofocus = true;
  mTSParam.minFocusAccuracy = 0.25f;
  mTSParam.fitIntervalX = 8.;
  mTSParam.fitIntervalY = 12.;
  mTSParam.fitIntervalZ = 12.;
  mTSParam.minFitXQuadratic = 4;
  mTSParam.minFitYQuadratic = 4;
  mTSParam.maxPredictErrorXY = 0.015f;
  mTSParam.maxPredictErrorZ[0] = 0.20f;
  mTSParam.maxPredictErrorZ[1] = 0.20f;
  mTSParam.maxPredictErrorZ[2] = 1.0f;
  mTSParam.alignTrackOnly = false;
  mTSParam.maxRefDisagreement = 0.05f;
  mTSParam.previewBeforeNewRef = true;
  mTSParam.trackAfterFocus = TRACK_BEFORE_FOCUS;
  mTSParam.skipAutofocus = false;
  mTSParam.limitDefocusChange = false;
  mTSParam.defocusChangeLimit = 20.f;
  mTSParam.anchorBuffer = MAX_BUFFERS - 1;
  mTSParam.lastStartingTilt = 60.;
  mTSParam.refineZLP = false;
  mTSParam.zlpInterval = 15;
  mTSParam.centerBeamFromTrial = false;
  mTSParam.closeValvesAtEnd = false;
  mTSParam.manualTracking = false;
  mTSParam.extraRecordType = NO_TS_EXTRA_RECORD;
  mTSParam.numExtraFilter = 0;
  mTSParam.numExtraFocus = 0;
  mTSParam.numExtraExposures = 0;
  mTSParam.numExtraChannels = 0;
  mTSParam.stackRecords = false;
  mTSParam.stackBinSize = 512;
  mTSParam.extraSetExposure = false;
  mTSParam.extraSetSpot = false;
  mTSParam.extraSpotSize = 0;
  mTSParam.extraDeltaIntensity = 1.;
  mTSParam.extraDrift = 0.;
  mTSParam.extraSetBinning = false;
  mTSParam.extraBinIndex = 0;
  mTSParam.numVaryItems = 0;
  mTSParam.doVariations = false;
  mTSParam.changeRecExposure = false;
  mTSParam.changeAllExposures = false;
  mTSParam.changeSettling = false;
  mTSParam.cenBeamAbove = false;
  mTSParam.cenBeamAngle = 50.;
  mTSParam.cenBeamPeriodically = false;
  mTSParam.cenBeamInterval = 60;
  mTSParam.doBidirectional = false;
  mTSParam.bidirAngle = 0.;
  mTSParam.anchorBidirWithView = true;
  mTSParam.walkBackForBidir = false;
  mTSParam.retainBidirAnchor = false;
  mTSParam.doEarlyReturn = false;
  mTSParam.earlyReturnNumFrames = 1;
  mTSParam.initialized = false;
  mPostponed = false;
  mDoingTS = false;
  mStartedTS = false;
  mVerbose = true;
  mAutosaveLog = false;
  mAutosaveXYZ = false;
  mDebugMode = false;
  mTerminateOnError = 0;
  mTermFromMenu = false;
  mInInitialChecks = false;
  mBackingUpTilt = 0;
  mBackingUpZ = -1;
  mStartAngleCrit1 = 45.;
  mStartAngleCrit2 = 5.;
  mNumActStartup = 18;
  mMaxUsableAngleDiff = 10.;
  mBadShotCrit = 0.2f;
  mBadLowMagCrit = 0.1f;
  mMaxImageFailures = 3;
  mMaxPositionFailures = 3;
  mMaxDropAsFocusDisturbed = 2;
  mMaxDropAsShiftDisturbed = 3;
  mMaxTiltError = 0.1f;
  mMaxDelayAfterTilt = 15;
  mMaxEucentricityAngle = 10.;
  mMaxDisturbValidChange = 0;
  mStageMovedTolerance = 0.1f;
  mUserFocusChangeTol = 0.1f;
  mLowMagFieldFrac = 0.3f;
  mMinFitXAfterDrop = 5;
  mMinFitYAfterDrop = 5;
  mMinFitZAfterDrop = 4;
  mFitDropErrorRatio = 1.3f;
  mFitDropBackoffRatio = 1.1f;
  mTrialCenterMaxRadFrac = 0.45f;
  mBeamShutterError = 0.03f;
  mIntensityPolarity = 0;
  mExpSeriesStep = 1.2f;
  for (i = 0; i < MAX_EXTRA_SAVES; i++) {
    mStoreExtra[i] = false;
    mExtraFiles[i] = 1;
  }
  mAutoTerminatePolicy = 1;
  mPolicyID[0] = IDYES;
  mPolicyID[1] = IDNO;
  mPolicyID[2] = IDCANCEL;
  mLDRecordLossPolicy = 1;
  mDimRetryPolicy = 0;  // Hard to see how this could be anything but 0
  mLDDimRecordPolicy = 1;
  mTiltFailPolicy = -1;   // Defer initialization with default until scope known
  mTermNotAskOnDim = 0;
  mSendEmail = 0;
  mAutoSavePolicy = true;
  mMessageBoxCloseValves = true;
  mMessageBoxValveTime = 30.f;
  mEndOnHighDimImage = true;
  mDimEndsAbsAngle = 55;
  mDimEndsAngleDiff = 5;
  mMinFieldBidirAnchor = 8.;
  mAlarmBidirFieldSize = 6.;
  mStepForBidirReturn = 0.;
  mSkipBeamShiftOnAlign = false;
  mSavedBufForExtra = NULL;
  mEarlyK2RecordReturn = false;
  mCallTSplugins = false;
  mAllowContinuous = false;
  mRunMacroInTS = false;
  mMacroToRun = 0;
  mFrameAlignInIMOD = false;
  mStepAfterMacro = TSMACRO_PRE_RECORD;
  for (i = 0; i < MAX_TSMACRO_STEPS; i++)
    mTableOfSteps[i] = -1;
  mTableOfSteps[TSMACRO_PRE_TRACK1 - 1] = TRACKING_SHOT;
  mTableOfSteps[TSMACRO_PRE_TRACK2 - 1] = POST_FOCUS_TRACKING;
  mTableOfSteps[TSMACRO_PRE_FOCUS - 1] = AUTOFOCUS;
  mTableOfSteps[TSMACRO_PRE_RECORD - 1] = RECORD_OR_MONTAGE;
  mFixedNumFrames = false;
  mTermOnHighExposure = false;
  mMaxExposureIncrease = 3.;
  mTiltIndex = -1;
  mSetupOpenedStamp = GetTickCount();
  FillActOrder(actions);
  ClearActionFlags();
  mRunningMacro = false;
}

CTSController::~CTSController()
{
}

void CTSController::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  mFocusManager = mWinApp->mFocusManager;
  mComplexTasks = mWinApp->mComplexTasks;
  mMultiTSTasks = mWinApp->mMultiTSTasks;
  mMontageController = mWinApp->mMontageController;
  mBeamAssessor = mWinApp->mBeamAssessor;
  mDocWnd = mWinApp->mDocWnd;
  mProcessImage = mWinApp->mProcessImage;
  if (mTiltFailPolicy < 0)
    mTiltFailPolicy = JEOLscope ? 1 : 0;
  if (!mStepForBidirReturn)
    mStepForBidirReturn = JEOLscope ? 10.f : 5.f;
}

// Clear out all the flags for individual actions
void CTSController::ClearActionFlags()
{
  mFindingEucentricity = false;
  mWalkingUp = false;
  mReversingTilt = false;
  mDoingMontage = false;
  mAcquiringImage = false;
  mResettingShift = false;
  mRefiningZLP = false;
  mCenteringBeam = false;
  mTakingBidirAnchor = false;
  mTilting = false;
  mFocusing = false;
  mCheckingAutofocus = false;
  mInvertingFile = false;
  mDoingPlugCall = false;
  mKeepActIndexSame = false;
  mGettingDeferredSum = false;
}

// Turn a list of actions into an array of order numbers for each action
void CTSController::FillActOrder(int *inActions)
{
  // Fill the order array with zeros, then populate with the list of actions
  int i;
  for (i = 0; i < MAX_TS_ACTIONS; i++)
    mActOrder[i] = 0;
  i = 0;
  while (inActions[i]) {
    mActOrder[inActions[i]] = i;
    i++;
  }
  mNumActTotal = i;
  mNumActLoop = i - mNumActStartup;
}

/////////////////////////////////////////////////////////////////////////////
// message handlers moved to MenuTargets: here are supporting calls

BOOL CTSController::IsResumable()
{
  return mStartedTS && !mDoingTS && !mBackingUpTilt;
}

void CTSController::EndLoop()
{
  mStopAtLoopEnd = true;
  mWinApp->SetStatusText(COMPLEX_PANE, "ENDING LOOP");
}

BOOL CTSController::CanBackUp()
{
  return mStartedTS && !mDoingTS && mTiltIndex > 0 && 
    (mActIndex < mActOrder[BIDIR_INVERT_FILE] || mActIndex >= mActOrder[TILT_IF_NEEDED]);
}

/////////////////////////////////////////////////////////////////////////////
// THE MAIN SETUP AND STARTING DIALOG

// Open the dialog, return 1 for error, -1 for cancel
int CTSController::SetupTiltSeries(int future, int futureLDstate, int ldMagIndex) 
{
  CTSSetupDialog tsDialog;
  float fangle, fangl2;
  double angle;
  int iDir, i, index, lowest;
  BOOL montaging, lowNewer, highNewer;
  CString message;
  int *panelStates = mWinApp->GetTssPanelStates();
  mLowDoseMode = mWinApp->LowDoseMode();
  CameraParameters *camParam = mWinApp->GetCamParams();
  ControlSet *camConSets = mWinApp->GetCamConSets();
  if (future && futureLDstate >= 0)
    mLowDoseMode = futureLDstate > 0;

  // Find out if user wants to restore from a state
  i = mWinApp->mNavHelper->GetTypeOfSavedState();
  if (i != STATE_NONE) {
    message.Format("There is %s state currently set\n\nDo you want to restore the prior"
      " state before setting up the tilt series?", i == STATE_IMAGING ? 
      "an imaging" : "a map acquire");
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDYES) {
      if (i == STATE_IMAGING) {
        mWinApp->mNavHelper->RestoreSavedState();
        if (mWinApp->mNavHelper->mStateDlg)
          mWinApp->mNavHelper->mStateDlg->Update();
      } else
        mWinApp->mNavHelper->RestoreFromMapState();
    }
  }

  for (i = 0; i < NUM_TSS_PANELS - 1; i++)
    tsDialog.mPanelOpen[i] = panelStates[i];
  tsDialog.mPanelOpen[NUM_TSS_PANELS - 1] = 1;
  angle = future ? 0. : mScope->GetTiltAngle();

  // First time in (since last tilt series), set the camera and mags; try to set
  // the angles intelligently;
  // Clear flags for limiting to current intensity and increasing it to start
  if (future == 1 || (!future && !mPostponed && !mStartedTS)) {

    // Set camera and mag only if not future montage (as signaled by magLocked)
    if (!(future && mTSParam.magLocked)) {
      mTSParam.cameraIndex = mWinApp->GetCurrentActiveCamera();
      mTSParam.probeMode = mScope->GetProbeMode();
      mSTEMindex = mWinApp->GetSTEMMode() ? 1 + mTSParam.probeMode : 0;
      if (mLowDoseMode) {
        mTSParam.magIndex[mSTEMindex] = mLDParam[RECORD_CONSET].magIndex;
      } else
        mTSParam.magIndex[mSTEMindex] = mScope->GetMagIndex();
    }

    // STEMindex set below, don't use it until then...
    mTSParam.intensitySetAtZero = false;
    mTSParam.limitToCurrent = false;

    // Try to use current angle or old starting angle
    if (fabs(angle) > mStartAngleCrit1) {
      fangle = (float)(0.01 * floor(100. * angle + 0.5));
      mTSParam.startingTilt = fangle;
      mTSParam.endingTilt = -fangle;
    } else {
      iDir = 1;
      if (angle < -mStartAngleCrit2 && mTSParam.lastStartingTilt > 0. ||
        angle > mStartAngleCrit2 && mTSParam.lastStartingTilt < 0.)
        iDir = -1;
      mTSParam.startingTilt = iDir * mTSParam.lastStartingTilt;
      mTSParam.endingTilt = -iDir * mTSParam.lastStartingTilt;
    }
  }
  mSTEMindex = camParam[mActiveCameraList[mTSParam.cameraIndex]].STEMcamera ? 
    1 + mTSParam.probeMode : 0;
  if (future && futureLDstate > 0 && ldMagIndex > 0)
    mTSParam.magIndex[mSTEMindex] = ldMagIndex;

  // First time in OR after a postpone,
  // Try to use the tilt range from sampling if it is new enough and 1-2 angles exist
  // Angles are newer if the interval is small; a negative interval is made very large
  lowNewer = SEMTickInterval(mMultiTSTasks->GetTiltRangeLowStamp(), mSetupOpenedStamp) 
    < 1000000000.;
  highNewer = SEMTickInterval(mMultiTSTasks->GetTiltRangeHighStamp(), mSetupOpenedStamp)
    < 1000000000.;
  fangle = mMultiTSTasks->GetTrUserLowAngle();
  fangl2 = mMultiTSTasks->GetTrUserHighAngle();
  if ((future || !mStartedTS) && 
    (fangle > -900 && lowNewer || fangl2 > -900 && highNewer)) {
      
    // Get each angle, only replace it if it was set
    if (fangle > -900 && lowNewer) {
      fangle = (float)(0.01 * B3DNINT(100. * fangle));
      if (mTSParam.endingTilt > mTSParam.startingTilt)
        mTSParam.startingTilt = fangle;
      else
        mTSParam.endingTilt = fangle;
    }
    if (fangl2 > -900 && highNewer) {
      fangl2 = (float)(0.01 * B3DNINT(100. * fangl2));
      if (mTSParam.endingTilt > mTSParam.startingTilt)
        mTSParam.endingTilt =  fangl2;
      else
        mTSParam.startingTilt = fangl2;
    }
  }

  if (future || (!future && !mPostponed && !mStartedTS))
    tsDialog.InitializePanels(tsDialog.PanelDisplayType());

  // Assume montaging if not started yet and montage is open, or if montaging flag set
  // If montaging, unconditionally set mag to match montage params
  // Allow mag to change if not used yet
  // Nav is responsible for setting up these values if appropriate
  if (!future) {
    montaging = (!mStartedTS && mWinApp->Montaging()) || (mStartedTS && mMontaging);
    if (montaging) {

      // If we are using a montaging mag, may need to modify probe mode and STEM index
      // This HAS to modify the camera to maintain consistency; this is new
      mSTEMindex = MontageMagIntoTSparam(mMontParam, &mTSParam);
    }
    mTSParam.magLocked = montaging && mMontParam->settingsUsed;
    tsDialog.mMontaging = montaging;
  } else
    tsDialog.mMontaging = mTSParam.magLocked;

  // 2/18/04: Don't fiddle with low mag index except to keep it above LM mode
  if (future == 1 || (!future && !mPostponed && !mStartedTS)) {
    lowest = mScope->GetLowestNonLMmag(mSTEMindex);
    if (mTSParam.lowMagIndex[mSTEMindex] < lowest)
      mTSParam.lowMagIndex[mSTEMindex] = lowest;
    index = 2 * mSTEMindex + (mLowDoseMode ? 1 : 0);
    if (mTSParam.bidirAnchorMagInd[index] <= 0)
      mTSParam.bidirAnchorMagInd[index] = 
      mComplexTasks->FindMaxMagInd(mMinFieldBidirAnchor);
    if (mTSParam.bidirAnchorMagInd[index] < lowest)
      mTSParam.bidirAnchorMagInd[index] = lowest;
  }

   // Set up binning from control set of selected camera
  if (!(future && mTSParam.magLocked))
    mTSParam.binning = camConSets[mActiveCameraList[mTSParam.cameraIndex] * MAX_CONSETS
      + RECORD_CONSET].binning;

  // Transfer parameters - this first sets parameters in master record from other places
  mCanFindEucentricity = fabs(angle) <= mMaxEucentricityAngle;
  mTSParam.initialized = true;
  CopyMasterToParam(&tsDialog.mTSParam, future < 2);
  tsDialog.mDoingTS = mStartedTS;
  tsDialog.mBidirStage = mBidirSeriesPhase;
  tsDialog.mLowDoseMode = mLowDoseMode;
  tsDialog.mCanFindEucentricity = mCanFindEucentricity;
  tsDialog.mCurrentAngle = angle;
  tsDialog.mFuture = future;
  tsDialog.mMaxDelayAfterTilt = (float)mMaxDelayAfterTilt;
  if (mPostponed && mWinApp->NavigatorStartedTS())
    tsDialog.mFuture = -1;

  // 2/13/04: Set the mean in A but no longer use as the target intensity
  tsDialog.mMeanInBufferA = (int)mProcessImage->EquivalentRecordMean(0);

  // All actions should return through OK
  mSetupOpenedStamp = GetTickCount();
  iDir = (int)tsDialog.DoModal();
  for (i = 0; i < NUM_TSS_PANELS - 1; i++)
    panelStates[i] = tsDialog.mPanelOpen[i];
  if (iDir != IDOK)
    return -1;

  // Transfer parameters back and set states in program too, including camera
  CopyParamToMaster(&tsDialog.mTSParam, !future);
  mPostponed = !future && tsDialog.mPostpone;
  if (future)
    return 0;
  if (montaging && !mTSParam.magLocked)
    mMontParam->magIndex = mTSParam.magIndex[mSTEMindex];

  StartTiltSeries(tsDialog.mSingleStep, 0);
  return 0;
} 

int CTSController::StartTiltSeries(BOOL singleStep, int external)
{
  float ratio, derate;
  double angle, curIntensity;
  int error, spot, probe, newBaseZ, i, magToSet = 0;
  CString message, str;
  mLowDoseMode = mWinApp->LowDoseMode();
  ControlSet *camConSets = mWinApp->GetCamConSets();
  mInInitialChecks = true;
  mUserPresent = !external;
  mErrorEmailSent = false;
  mUserStop = false;
  mErrorStop = false;

 // Whether to postpone was set by dialog, but if coming in external, clear it
  if (external) {
    mPostponed = false;

    // If montaging under external call, sync the binning and mag into the TS param now
    if (mWinApp->Montaging()) {
      mTSParam.binning = mMontParam->binning;
      magToSet = mMontParam->magIndex;
      mTSParam.cameraIndex = mMontParam->cameraIndex;
    }

    // Better sync the low dose mag in now also so it is correct in the report
    if (mLowDoseMode)
      magToSet = mLDParam[RECORD_CONSET].magIndex;
  }

  mWinApp->SetActiveCameraNumber(mTSParam.cameraIndex);
  mCamParams = mWinApp->GetCamParams() + mActiveCameraList[mTSParam.cameraIndex];
  mSTEMindex = mCamParams->STEMcamera ? 1 + mTSParam.probeMode : 0;
  if (magToSet)
    mTSParam.magIndex[mSTEMindex] = magToSet;
  mWinApp->UpdateWindowSettings();
  angle = mScope->GetTiltAngle();

   // Return to external control, make sure startup flag is set in case TSMB postpones
  // The external flag indicates "user is not there" and the control flag is the true
  // indicator of being under external control
  mExternalControl = (external || mWinApp->NavigatorStartedTS()) ? 1 : 0;
  mTerminateOnError = mExternalControl ? mAutoTerminatePolicy : 0;
  mAlreadyTerminated = false;
  mLastSucceeded = 0;
  mTiltedToZero = false;
  if (!mStartedTS) {
    mInStartup = true;

    // Set this now so that termination can always restore intensity
    mStartingIntensity = mLowDoseMode ? mLDParam[3].intensity : mScope->GetIntensity();
    mOriginalIntensity = mStartingIntensity;
    mStartingExposure = mConSets[RECORD_CONSET].exposure;
  }

  // Check for binning change and issue warning and offer to abort
  // Need to operate on the camera-specific conset and copy to master consets
  i = RECORD_CONSET + mWinApp->GetCurrentCamera() * MAX_CONSETS;
  if (camConSets[i].binning != mTSParam.binning) {
    ratio = (float)camConSets[i].binning / mTSParam.binning;
    camConSets[i].exposure *= ratio * ratio;
    camConSets[i].binning = mTSParam.binning;
    mWinApp->CopyConSets(mWinApp->GetCurrentCamera());
    message = "You have changed the binning of the " + mModeNames[RECORD_CONSET] +
      " parameter set.\nThe exposure time has been changed to compensate,\n"
      "but you should still check the exposure and drift settling\n"
      "to make sure you are getting good images.";

    // Follow the logic in TSMB if called from elsewhere or postponing anyway,
    // otherwise query and postpone conditionally
    if (mPostponed || external) {
      TSMessageBox(message, MB_EXCLAME, false, 0);
      //mPostponed = !mAlreadyTerminated;
    } else {
      message += "\n\nDo you want to postpone the tilt series in "
        "order to check exposures?";
      if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDYES)
        mPostponed = true;
    }
  }

  // If postponing, (set anything else?) and return
  if (mPostponed) {
    LeaveInitialChecks();
    return 1;
  }

  // Check on intensity calibration any time the user could have selected beam changing
  // check general intensity, or for record area in low dose mode.  Check increase in case
  // intensity is at end of range
  if (mTSParam.beamControl != BEAM_CONSTANT && 
    !(mTSParam.changeRecExposure || mSTEMindex)) { 
    curIntensity = mLowDoseMode ? mLDParam[3].intensity : mScope->GetIntensity();
    spot = mLowDoseMode ? mLDParam[3].spotSize : mScope->GetSpotSize();
    probe = mLowDoseMode ? mLDParam[3].probeMode : mScope->ReadProbeMode();

    // Get the intensity polarity from this call and see if there is an error
    error = mBeamAssessor->OutOfCalibratedRange(curIntensity, spot, probe, 
      mIntensityPolarity);
    if (error) {

      if (error == BEAM_STARTING_OUT_OF_RANGE || error == BEAM_ENDING_OUT_OF_RANGE)
        TSMessageBox("Cannot proceed: Beam intensity is not in the calibrated "
        "range.\n\nYour brightness setting might not be on the same side of\n"
        "crossover as the intensity calibration");
      else
        TSMessageBox("Cannot proceed: Beam intensity is not calibrated for the\n"
        "current spot size and side of crossover or is at the end of its range");
      mPostponed = !mAlreadyTerminated;
      return 1;
    }
  }
    
  // SETUP FOR FIRST STARTING
  if (!mStartedTS) {

    // Set starting angle for series appropriately for bidir series or not
    // check if angles make sense and disallow having data in the file already
    mStartingTilt = mTSParam.startingTilt;
    mEndingTilt = mTSParam.endingTilt;
    mBidirSeriesPhase = BIDIR_NONE;
    if (mTSParam.doBidirectional) {
      mBidirSeriesPhase = BIDIR_FIRST_PART;
      mStartingTilt = mTSParam.bidirAngle;
      mEndingTilt = mTSParam.startingTilt;
      mWalkBackForBidir = mTSParam.walkBackForBidir;
      if (fabs(mStartingTilt - mTSParam.startingTilt) < 1. || 
        fabs(mStartingTilt - mTSParam.endingTilt) < 1.) {
        TSMessageBox("The starting angle for bidirectional tilting is too \n"
          "close to the starting or ending angle");
        mPostponed = !mAlreadyTerminated;
        return 1;
      }

      if (mWinApp->mStoreMRC && mWinApp->mStoreMRC->getDepth() > 0 && mExternalControl) {
        TSMessageBox("Cannot proceed: The image file already has data in it;\n"
          "bidirectional tilt series cannot append to a file.");
        mPostponed = !mAlreadyTerminated;
        return 1;
      }
    }

    // First see if "near" zero and make sure images are in indicated buffers
    // 11/19/06, eliminate test for refisina setting and image in A, because the
    // setting is used below only if there is an image in A
    if (mBufferManager->CheckAsyncSaving())
       return 1;
    
    // Have an anchor if the user indicated it; this will prevent walkup from taking one
    mHaveAnchor = mTSParam.useAnchor && !mExternalControl && !mTSParam.doBidirectional;
    if (mHaveAnchor && !mImBufs[mTSParam.anchorBuffer].mImage) {
      TSMessageBox("There is no image in the indicated buffer to serve as an anchor");
      mPostponed = true;
      return 1;
    }
    
    if (!mExternalControl) {

      // Now check for open file and make sure that is what they want
      if (!mWinApp->Montaging() && mWinApp->mStoreMRC && 
        mWinApp->mStoreMRC->getDepth() > 0) {
        message = mTSParam.doBidirectional ? "which WILL BE OVERWRITTEN if you proceed"
          " with a bidirectional series\n" : "";
        str = mTSParam.doBidirectional ? "\n (data WILL BE OVERWRITTEN if you answer"
          " NO)" : "";
        if (mDocWnd->GetNumStores() == 1) {
          message = "There is an open image file that already has images in it\n"
            + message + "\nDo you want to close this file and start a new one?" + str; 
          if (AfxMessageBox(message, MB_QUESTION) == IDYES)
            mDocWnd->DoCloseFile();
        } else {
          message = "The current image file already has images in it\n" + message +
            "\nDo you want to postpone the series so that you can either\nswitch to a "
            "different open file or open a new file?" + str;
          if (AfxMessageBox(message, MB_QUESTION) == IDYES) {
            mPostponed = true;
            LeaveInitialChecks();
            return 1;
          }
        }
      }

      // Open file if not one open
      if (!mWinApp->mStoreMRC)
        mDocWnd->DoOpenNewFile();
      if (!mWinApp->mStoreMRC) {
        mPostponed = true;
        LeaveInitialChecks();
        return 1;
      }
    } else if (!mWinApp->mStoreMRC) {
      TSMessageBox("There is no open image file");
      mPostponed = !mAlreadyTerminated;
      return 1;
    }
    if (mTSParam.doBidirectional) {
      if (mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC) {
        TSMessageBox("Cannot proceed: Cannot do a bidirectional tilt series with a\r\n"
          "series of numbered TIFF files as output.\r\nOpen a regular image file.");
        mPostponed = !mAlreadyTerminated;
        return 1;
      }
      mBidirAnchorName = mWinApp->mStoreMRC->getFilePath() + ".anchor";
    }

    // Set the current output file, then test the output files and set the protections
    mCurrentStore = mDocWnd->GetCurrentStore();
    EvaluateExtraRecord();
    if (CheckExtraFiles()) {
      mPostponed = !mAlreadyTerminated;
      return 1;
    }
    SetProtections();
    
    mMontaging = mWinApp->Montaging();
    if (mMontaging)
      mMontParam->magIndex = mTSParam.magIndex[mSTEMindex];
    mBaseZ = mMontaging ? mMontParam->zCurrent : mWinApp->mStoreMRC->getDepth();
    
    message = mMontaging ? "The image file already has data in it.\n" : " ";
    if (mWinApp->mStoreMRC->getDepth()) {
      if (external) {
        mWinApp->AppendToLog("WARNING: The image file already has data in it; "
          "proceeding anyway", LOG_OPEN_IF_CLOSED);
      } else if (mTSParam.doBidirectional) {

        // Confirm they want to overwrite a montage (single-frame was confirmed above)
        if (mMontaging && AfxMessageBox(message + "This data WILL BE OVERWRITTEN if you"
          " proceed with a bidirectional series\n\nAre you sure you want to continue?",
          MB_QUESTION) == IDNO) {
          mPostponed = true;
          return 1;
        }
        mBaseZ = 0;
      } else if (AfxMessageBox(message + "Do you want to overwrite some of the data in\n"
        "this file by setting the current Z value?", 
        MB_YESNO | MB_ICONQUESTION) == IDYES) {
        newBaseZ = mBaseZ - 1;
        KGetOneInt("Z value to start overwriting at:", newBaseZ);
        B3DCLAMP(newBaseZ, 0, mBaseZ);
        mBaseZ = newBaseZ;
      }
    }
    mTiltIndex = 0;
    SetFileZ();

    // Doing frame alignment in IMOD, turn off the frame alignment now and set flag
    if (mCamera->IsConSetSaving(&mConSets[RECORD_CONSET], mCamParams, false) && 
      mConSets[RECORD_CONSET].alignFrames && 
      mConSets[RECORD_CONSET].useFrameAlign > 1 && mCamera->GetAlignWholeSeriesInIMOD()) {
        if (mWinApp->mStoreMRC->GetAdocIndex() < 0) {
          TSMessageBox("The output file was not opened with an associated .mdoc\n"
            "file, and this is required with the option you have selected to\n"
            "put out a command file for aligning the whole tilt series in IMOD");
          mPostponed = !mAlreadyTerminated;
          return 1;
       }
       mFrameAlignInIMOD = true;
       mConSets[RECORD_CONSET].alignFrames = 0;
       i = RECORD_CONSET + mWinApp->GetCurrentCamera() * MAX_CONSETS;
       camConSets[i].alignFrames = 0;
    }

    // Put out starting message.  Navigator took care of the log file change
    if (mExternalControl) {
      message.Format("\r\nSTARTING TILT SERIES WITH %s AFTER ERRORS.\r\n", 
        mTerminateOnError ? "AUTOMATIC TERMINATION" : "STOPPING FOR INTERVENTION");
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
    }

    // Save parameters that might vary and initialize zero-degree values for variations
    for (i = 0; i < 5; i++) {
      mSaveExposures[i] = mConSets[i].exposure;
      mSaveDrifts[i] = mConSets[i].drift;
      mSaveFrameTimes[i] = mConSets[i].frameTime;
    }
    mSaveFrameList = mConSets[RECORD_CONSET].summedFrameList;
    mAnyContinuous = false;
    for (i = (mLowDoseMode ? 0 : 1); i < (mLowDoseMode ? 5 : 4); i++) {
      mSaveContinuous[i] = mConSets[i].mode;
      mAnyContinuous |= mSaveContinuous[i] == CONTINUOUS;
    }
    mZeroDegValues[TS_VARY_EXPOSURE] = mConSets[RECORD_CONSET].exposure;
    mZeroDegValues[TS_VARY_DRIFT] = mConSets[RECORD_CONSET].drift;
    mZeroDegValues[TS_VARY_FRAME_TIME] = mConSets[RECORD_CONSET].frameTime;
    mSaveTargetDefocus = mFocusManager->GetTargetDefocus();
    mZeroDegValues[TS_VARY_FOCUS] = mSaveTargetDefocus;
    if (mLowDoseMode) {
      mSaveEnergyLoss = mLDParam[RECORD_CONSET].energyLoss;
      mSaveZeroLoss = mLDParam[RECORD_CONSET].zeroLoss;
      mSaveSlitWidth = mLDParam[RECORD_CONSET].slitWidth;
      mSaveSlitIn = mLDParam[RECORD_CONSET].slitIn;
    } else {
      mSaveEnergyLoss = mFilterParams->energyLoss;
      mSaveZeroLoss = mFilterParams->zeroLoss;
      mSaveSlitWidth = mFilterParams->slitWidth;
      mSaveSlitIn = mFilterParams->slitIn;
    }
    mZeroDegValues[TS_VARY_ENERGY] = mSaveEnergyLoss;
    mZeroDegValues[TS_VARY_SLIT] = mSaveSlitWidth;
    mSaveInterpDarkRef = mCamera->GetInterpDarkRefs();
    CTSVariationsDlg::ListVaryingTypes(mTSParam.varyArray, mTSParam.numVaryItems,
      mTypeVaries, mZeroDegValues);

    // Set flags for whether camera params need to be varied and restored
    mChangeRecExp = (mTSParam.doVariations && mTypeVaries[TS_VARY_EXPOSURE]) || 
      mTSParam.changeRecExposure || mSTEMindex;
    mChangeAllExp = mChangeRecExp && mTSParam.changeAllExposures;
    mChangeRecDrift = ((mTSParam.doVariations && mTypeVaries[TS_VARY_DRIFT]) ||
      (mChangeRecExp && mTSParam.changeSettling)) && !mSTEMindex;
    mChangeAllDrift = mChangeAllExp && mTSParam.changeSettling && !mSTEMindex;
    if (mChangeRecExp)
      mCamera->SetInterpDarkRefs(true);
    mUseExpForIntensity = (mTSParam.changeRecExposure || mSTEMindex) && 
      !(mTSParam.doVariations && mTypeVaries[TS_VARY_EXPOSURE]);
    mVaryFrameTime = mCamParams->K2Type && mTSParam.doVariations && 
      mTypeVaries[TS_VARY_FRAME_TIME];
    mVaryFilter = (mTypeVaries[TS_VARY_ENERGY] || mTypeVaries[TS_VARY_SLIT]) && 
      (mCamParams->GIF || mScope->GetHasOmegaFilter()) && mTSParam.doVariations;

    // Save user's buffer settings and impose our own
    mDocWnd->AutoSaveFiles();
    mSaveShiftsOnAcquire = mBufferManager->GetShiftsOnAcquire();
    mSaveBufToReadInto = mBufferManager->GetBufToReadInto();
    mSaveAlignOnSave = mBufferManager->GetAlignOnSave();
    mSaveCopyOnSave = mBufferManager->GetCopyOnSave();
    mSaveAlignToB = mBufferManager->GetAlignToB();
    mSaveProtectRecord = mBufferManager->GetConfirmBeforeDestroy(3);
    mBufferManager->SetShiftsOnAcquire(SHIFTS_ON_ACQUIRE);
    mAlignBuf = SHIFTS_ON_ACQUIRE + 1;
    mExtraRefBuf = mAlignBuf + 1;
    mReadBuf = mAlignBuf + 2;
    mBufferManager->SetBufToReadInto(mReadBuf);
    mAnchorBuf = MAX_BUFFERS - 1;

    // With new align to B, can just leave copy on save at 0
    mBufferManager->SetCopyOnSave(0);
    mBufferManager->SetAlignOnSave(false);
    mBufferManager->SetAlignToB(false);
    mBufferManager->SetConfirmBeforeDestroy(3, 0);
    mWinApp->mBufferWindow.ProtectBuffer(mAlignBuf, true);    
    mWinApp->mBufferWindow.ProtectBuffer(mExtraRefBuf, true);   
    mWinApp->mBufferWindow.ProtectBuffer(mReadBuf, true);   
    mWinApp->mBufferWindow.ProtectBuffer(mAnchorBuf, true);
    
    // If there is any GIF camera, set autocamera feature to manage EFTEM
    if (mFilterParams->firstGIFCamera >= 0)
      mFilterParams->autoCamera = true;
    
    mStartedTS = true;
    ManageTerminateMenu();
    
    // If there is anything in buffer A, use it to get the required image level going,
    // but derate it considerably, and even more if beam shutter not OK
    // 10/21/08: use this only if it is a reference in A and not running automatically
    mBeamShutterJitters = !mCamParams->DMbeamShutterOK;
    for (i = 0; i < MAX_LOWDOSE_SETS; i++)
      mBWMeanPerSec[i] = -1.;
    if (!mExternalControl && mTSParam.refIsInA &&
      mImBufs->mImageScale && mImBufs->mBinning && mImBufs->mExposure) {
      derate = mBeamShutterJitters ? (float)
        (mImBufs->mExposure / (mImBufs->mExposure + mBeamShutterError)) : 1.f;
      i = mLowDoseMode ? mCamera->ConSetToLDArea(mImBufs->mConSetUsed) : 0;
      mBWMeanPerSec[i] = 0.3f * derate * 
        mProcessImage->UnbinnedSpotMeanPerSec(mImBufs);
    }

    mWarnedNoExtraFilter = false;
    mWarnedNoExtraChannels = false;
    mInExtraRecState = false;
    mFirstIntensity = 0.;
    mFirstExposure = 0.;
    mDidWalkup = false;
    mStartingOut = true;
    mStopAtLoopEnd = false;
    mReachedEndAngle = false;
    mFinishEmailSent = false;
    mPreTilted = false;
    mInLowMag = false;
    mHaveFocusPrediction = false;
    mHaveXYPrediction = false;
    mCheckFocusPrediction = false;
    mUsableLowMagRef = false;
    mHaveLowMagRef = false;
    mHaveRecordRef = false;
    mHaveTrackRef = false;
    mNeedFirstTrackRef = mTSParam.doBidirectional && mLowDoseMode;
    mDidRegularTrack = false;
    mRoughLowMagRef = false;
    mTrackLowMag = mTSParam.trackLowMag && !mLowDoseMode && 
      (mTSParam.lowMagIndex[mSTEMindex] < mTSParam.magIndex[mSTEMindex]);
    mDidResetIS = false;
    mDidTiltAfterMove = false;
    mOriginalAngle = mTSParam.intensitySetAtZero ? 0. : angle;
    mRestoredTrackBuf = 0;
    mLDTrialWasMoved = false;
    mNeedDeferredSum = false;
    mDirection = mStartingTilt < mEndingTilt ? 1 : -1;
    mLastIncrement = mTSParam.tiltIncrement;
    mCurrentTilt = mStartingTilt;
    mTrueStartTilt = mStartingTilt;
    mPartStartTiltIndex = 0;
    mLastBidirReturnTilt = false;
    mWinApp->mCameraMacroTools.DoingTiltSeries(true);
    mWinApp->UpdateWindowSettings();
    mActIndex = mNumActLoop;
    mActLoopEnd = mNumActTotal;
    mHaveStartRef = false;
    mHaveZeroTiltTrack = false;
    mHaveZeroTiltPreview = false;
    mAccumDeltaForMean = 1.;
    mStoppedForAccumDelta = false;
    mMinAccumDeltaForMean = 1.;
    for (i = 0; i < 5; i++)
      mDoseSums[i] = 0.;
    mComplexTasks->GetAndClearDose();
    mOneShotCloseValves = 0;
    mUseNewRefOnResume = false;
    probe = mLowDoseMode ? mLDParam[TRIAL_CONSET].probeMode : mScope->GetProbeMode();
    mAutocenteringBeam = (mTSParam.cenBeamPeriodically || mTSParam.cenBeamAbove) &&
      mMultiTSTasks->AutocenParamExists(mActiveCameraList[mTSParam.cameraIndex], 
      mTSParam.magIndex[mSTEMindex], probe) && !mSTEMindex;
    mAutocenStamp = 0.001 * GetTickCount();
    mDidCrossingAutocen = false;
    if (mTSParam.cenBeamAngle < 0)
      mTSParam.cenBeamAngle = -mTSParam.cenBeamAngle;
    mNumSavedExtras.resize(0);
    mExtraStartingZ.resize(0);

    // If the user says there is an image to align to in buffer A, it is a starting
    // reference if we are within usable range of starting angle: save it in extraref
    if (!mExternalControl && mTSParam.refIsInA && mImBufs->mImage) {
      mHaveStartRef = UsableRefInA(mStartingTilt, mTSParam.magIndex[mSTEMindex]);
      if (mHaveStartRef)
        mBufferManager->CopyImageBuffer(0, mExtraRefBuf);

      // Otherwise, if it is usable from the current angle,
      // save a preview in align buffer, and set flags for preview or track
      else if (UsableRefInA(angle, mTSParam.magIndex[mSTEMindex])) {
        if (mLowDoseMode && mCamera->ConSetToLDArea(mImBufs->mConSetUsed) == 3) {
          mBufferManager->CopyImageBuffer(0, mAlignBuf);
          mHaveZeroTiltPreview = true;
        } else
          mHaveZeroTiltTrack = true;
      }
    }
    
    // If have an anchor, copy to buffer and get angle; if something is wrong,
    // then there is no anchor
    if (mHaveAnchor) {
      if (mTSParam.anchorBuffer != mAnchorBuf)
        mBufferManager->CopyImageBuffer(mTSParam.anchorBuffer, mAnchorBuf);
      mImBufs[mAnchorBuf].mCaptured = BUFFER_ANCHOR;
      mHaveAnchor = mImBufs[mAnchorBuf].GetTiltAngle(mAnchorAngle);
    }

    // If low dose, set up more buffers for tracking stack
    if (mLowDoseMode) {
      mTrackStackBuf = mReadBuf + 1 + BUFFERS_FOR_USER;
      mNumTrackStack = MAX_BUFFERS - mTrackStackBuf - 
        ((mHaveAnchor || mTSParam.leaveAnchor || mTSParam.doBidirectional) ? 1 : 0);
      for (i = mTrackStackBuf; i < mNumTrackStack + mTrackStackBuf; i++) {
        mWinApp->mBufferWindow.ProtectBuffer(i, true);
        mImBufs[i].DeleteImage();
      }
      mLastLDTrackAngle = 0.;

    }

    // } else {

    // Actions if resuming
  }
  return CommonResume(singleStep ? 1 : 0, external);
}
        
// Common routine for starting or resuming the sequence of actions
int CTSController::CommonResume(int inSingle, int external, bool stoppingPart)
{
  double stageX, stageY, stageZ;
  int i, camera, sizeX, sizeY, fileX, fileY;
  CString report;
  camera = mActiveCameraList[mTSParam.cameraIndex];
  ControlSet *csp = mWinApp->GetCamConSets() + camera * MAX_CONSETS + RECORD_CONSET;
  ControlSet *csets = mWinApp->GetCamConSets() + camera * MAX_CONSETS;
  mInInitialChecks = true;
  mUserPresent = !external;
  mErrorEmailSent = false;
  mPostponed = false;
  mFinalReport = "";

  // Get a new matrix in case user recalibrated
  mCmat = mShiftManager->IStoSpecimen(mTSParam.magIndex[mSTEMindex]);
  mCinv = mShiftManager->MatInv(mCmat);

  // Get current value of this setting
  mActPostExposure = mWinApp->ActPostExposure();
  
  // Check record parameters versus file
  mCamera->AcquiredSize(csp, camera, sizeX, sizeY);
  mBigRecord = (mCamParams->sizeX > 2048 || mCamParams->sizeY > 2048) && 
    csp->binning == 1;
  fileX = mWinApp->mStoreMRC->getWidth();
  fileY = mWinApp->mStoreMRC->getHeight();
  if (!mMontaging && fileX && (fileX != sizeX || fileY != sizeY)) {
    report.Format("The Record parameters are not set up to give an image\n"
      " that can be saved in the current image file.\n\n"
      "You need to either:\n1) Set the Record parameters to give a %d by %d image, or\n"
      "2) Terminate the tilt series and restart with a new image file.", fileX, fileY);
    TSMessageBox(report);
    mPostponed = !mAlreadyTerminated;
    return 1;
  }

  // Evaluate whether running a macro, and if so check it
  mDoRunMacro = mRunMacroInTS && mMacroToRun > 0 && mMacroToRun <= MAX_MACROS && 
    mStepAfterMacro > 0 && mStepAfterMacro <= MAX_TSMACRO_STEPS &&
    mTableOfSteps[mStepAfterMacro - 1] >= 0;
  if (mDoRunMacro && mWinApp->mMacroProcessor->EnsureMacroRunnable(mMacroToRun - 1)) {
    mPostponed = !mAlreadyTerminated;
    return 1;
  }

  // Check the extra files and reset the protections, and reset current output file
  EvaluateExtraRecord();
  if (CheckExtraFiles()) {
    mPostponed = !mAlreadyTerminated;
    return 1;
  }
  SetProtections();
  mDocWnd->SetCurrentStore(mCurrentStore);
  mInInitialChecks = false;
  mUserPresent = false;
  mEndingTilt = mTSParam.endingTilt;
  if (mBidirSeriesPhase != BIDIR_NONE)
    mEndingTilt = mBidirSeriesPhase == BIDIR_FIRST_PART ? 
      mTSParam.startingTilt : mTSParam.endingTilt;
  mPoleAngle = mDirection * 90.;

  // Use image in A as new track reference if flag set from manual track or resume dialog
  // Put it in the right place and set flag for track
  if (mUseNewRefOnResume) {
    if (mLowDoseMode) {
      
      // Manage flags for validity of either kind of reference
      if (mCamera->ConSetToLDArea(mImBufs->mConSetUsed) < RECORD_CONSET) {
        mBufferManager->CopyImageBuffer(0, mExtraRefBuf);
        mHaveRecordRef = false;
        mHaveTrackRef = true;
      } else {
        mBufferManager->CopyImageBuffer(0, mAlignBuf);
        mHaveRecordRef = true;
        mHaveTrackRef = false;
      }

    } else
      mBufferManager->CopyImageBuffer(0, mAlignBuf);
    if (mTiltIndex > 0)
      mDisturbance[mTiltIndex - 1] |= NEW_REFERENCE;
    mNeedRegularTrack = true;
  }
  mUseNewRefOnResume = false;

  // This used to be available only in low dose or STEM
  mSkipFocus = mTSParam.skipAutofocus;

  if (!mAllowContinuous) {
    for (i = (mLowDoseMode ? 0 : 1); i < (mLowDoseMode ? 5 : 4); i++) {
       mConSets[i].mode = SINGLE_FRAME;
       csets[i].mode = SINGLE_FRAME;
    }
  }

  // Check for stage movements and focus changes
  if (!mStartingOut && !stoppingPart) {
    mScope->GetStagePosition(stageX, stageY, stageZ);
    if (fabs(stageX - mStopStageX) > mStageMovedTolerance ||
      fabs(stageY - mStopStageY) > mStageMovedTolerance ||
      fabs(stageZ - mStopStageZ) > mStageMovedTolerance) {
      if (mTiltIndex > 0)
        mDisturbance[mTiltIndex - 1] |= USER_MOVED_STAGE;
      mNeedFocus = !mSkipFocus;
      mNeedRegularTrack = true;
      mCheckFocusPrediction = false;
      if (mTrackLowMag) {
        mNeedLowMagTrack = true;
        mNeedRegularTrack = !UsableLowMagRef(mScope->GetTiltAngle());
      }
    }

    // Get target defocus and check for focus changes
    mTSParam.targetDefocus = mFocusManager->GetTargetDefocus();
    if (fabs((mScope->GetUnoffsetDefocus() - (mSTEMindex ? 0. : mTSParam.targetDefocus)) -
      (mStopDefocus - (mSTEMindex ? 0. : mStopTargetDefocus))) > mUserFocusChangeTol) {
      if (mTiltIndex > 0)
        mDisturbance[mTiltIndex - 1] |= USER_CHANGED_FOCUS;
      mCheckFocusPrediction = false;
    }

    // Check for change in low dose area and invalidate the reference; make
    // sure user wants to proceed if there is no reference
    CheckLDTrialMoved();
    if (mLDTrialWasMoved && !mHaveTrackRef) {
      if (AfxMessageBox("You have moved the tracking area but have\n"
        " not supplied a new tracking reference.\n\n"
        "If you proceed, the next Record will be taken\n"
        "at the current position without any tracking.\n\n"
        "Are you sure you want to proceed?", MB_YESNO | MB_ICONQUESTION) != IDYES) {
          mPostponed = true;
          return 1;
        }
    }

    // Recompute the varying values in case anything changes
    if (mTSParam.doVariations)
      ImposeVariations(mScope->GetTiltAngle());
  }
  
  // See if backing up in low dose and need to rearrange the ref stack
  if (mRestoredTrackBuf) {
    for (i = mTrackStackBuf; i < mTrackStackBuf + mNumTrackStack; i++) {
      mRestoredTrackBuf++;
      if (mRestoredTrackBuf < mTrackStackBuf + mNumTrackStack && 
        mImBufs[mRestoredTrackBuf].mImage)
        mBufferManager->CopyImageBuffer(mRestoredTrackBuf, i);
      else
        mImBufs[i].DeleteImage();
    }
  }
  
  // Assess need for next tilt and get stop flag set appropriately
  mStopAtLoopEnd = false;
  mTermAtLoopEnd = false;
  if (!stoppingPart)
    AssessNextTilt(true);

  // Set stop at loop end flag if single loop, unless tilt is next or doing manual track
  mSingleStep = inSingle;
  if (inSingle > 0 && mActIndex != mActOrder[TILT_IF_NEEDED] && !mTSParam.manualTracking)
    mStopAtLoopEnd = true;

  mDoingTS = true;
  mUserStop = false;
  mErrorStop = false;
  mStartupStop = false;
  mStackedLDRef = false;
  mNeedGoodAngle = true;
  //mExtraRefShifted = true;      // Disable shifting of extra reference on any resume
  
  // No longer need to set copy on save now
  mWinApp->mLowDoseDlg.SetContinuousUpdate(false);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "DOING TILT SERIES");
  mImageFailures = 0;
  mPositionFailures = 0;
  mLastStartedIndex = mTiltIndex;
  mLastRedoIndex = mTiltIndex;
  mSwitchedToMontFrom = -1;
  mWinApp->mPluginManager->ResumingTiltSeries(mTiltIndex);

  // If valves haven't been closed ever, set one-shot based on the current state of flag 
  if (mOneShotCloseValves >= 0) 
    mOneShotCloseValves = (mTSParam.closeValvesAtEnd && !mExternalControl) ? 1 : 0;
  mCloseValvesOnStop = false;

  // make sure the camera and mag are right
  mWinApp->SetActiveCameraNumber(mTSParam.cameraIndex);
  if (!mLowDoseMode)
    mScope->SetMagIndex(mTSParam.magIndex[mSTEMindex]);

  // Save user's setting of Record dark reference averaging and impose TS option
  mSaveAverageDarkRef = mConSets[RECORD_CONSET].averageDark;
  mConSets[RECORD_CONSET].averageDark = mSTEMindex ? 0 : mCamParams->TSAverageDark;

  NextAction(NO_PREVIOUS_ACTION);
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// THE MAIN ACTION ROUTINE, WITH INITIAL TESTS FOR THE VALIDITY OF THE LAST
// THEN A LOOP ON THE NEXT ACTIONS IN SEQUENCE

void CTSController::NextAction(int param)
{
  double delFac, ISX, ISY, missingX, missingY, angleError;
  float anchorAngle, imageCrit, shiftX, shiftY, maxError, derate, plusFactor, minusFactor;
  int anchorBuf, nx, ny, delay, error, refBuf, bwIndex, conSet, maxImageFailures;
  int policy, extraSet, magIndex = mTSParam.magIndex[mSTEMindex];
  int curCamera = mWinApp->GetCurrentCamera();
  float binRatio, refShiftX, refShiftY, delX, delY;
  StageMoveInfo smi;
  BOOL needRedo, actionIncomplete, lastOrOnly;
  CString message, message2;
  KImage *image;
  ControlSet *cset;
  double cosRot, sinRot, rotation, fsY0, dose, angle, oldAngle;
  double specErrorX, specErrorY, specSizeX, specSizeY, pixelSize, fracThick;
  double fracErrorX, fracErrorY, shotIntensity;
  CString recordMontage = mMontaging ? "Montage" : "Record";
  CString reason = " because of insufficient counts";
  ScaleMat aMat, aInv;
  EMimageBuffer *refImBuf = mImBufs + mExtraRefBuf;

  DebugDump("Start of NextAction");

  // Get angle well or quickly
  if (mNeedGoodAngle || mActIndex == mActOrder[LOOP_START_CHECKS]) {
    angle = mScope->GetTiltAngle();
    /* message.Format("Got fresh tilt angle %.2f", angle);
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED); */
  } else
    angle = mScope->FastTiltAngle();
  mNeedGoodAngle = false;

  // First check if backing up the tilt; if so, finish the job then set up to 
  // continue without error checking
  if (mBackingUpTilt && mRoughLowMagRef && mAcquiringImage) {

    // If got a rough reference, save it and start backing up for real
    RestoreMag();
    mAcquiringImage = false;
    mBufferManager->CopyImageBuffer(0, mExtraRefBuf);
    mHaveLowMagRef = true;    // Should this be set?
    mLowMagRefTilt = angle;
    StartBackingUp(mBackingUpTilt);
    return;
  
  } else if (mBackingUpTilt == 1) {
    StartBackingUp(2);
    return;
  } else if (mBackingUpTilt == 2) {
    mBackingUpTilt = 0;
    param = NO_PREVIOUS_ACTION;

    // If this happened as part of a backup request, just return
    if (!mDoingTS) {
      mWinApp->UpdateBufferWindows();
      return;
    }
    
    // But if this happened after a user stop, go back to the stop
    // Error stop will be caught below
    if (mUserStop) {
      NormalStop();
      return;
    }
  }

  // Save a deferred sum if last action was to get one
  if (mGettingDeferredSum) {
    mGettingDeferredSum = false;
    error = 0;
    if (mCamera->GetDeferredSumFailed()) {
      SEMMessageBox("An error occurred getting the full summed Record image");
      error = 1;
    } else {
      if (mSecForDeferredSum == mWinApp->mStoreMRC->getDepth())
        error = mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC);
      else
        error = mBufferManager->OverwriteImage(mWinApp->mStoreMRC, mSecForDeferredSum);
    }
    if (mTSParam.stackRecords)
        mBufferManager->AddToStackWindow(0, mTSParam.stackBinSize, mDeferredSumStackSec,
        true);

    // Dispatch to where call started from, or to error stop on error
    if (mWhereDefSumStarted == DEFSUM_TERM_ERROR) {
      TerminateOnError();
      return;
    } else if (error || mWhereDefSumStarted == DEFSUM_ERROR_STOP) {
      ErrorStop();
      return;
    } else if (mWhereDefSumStarted == DEFSUM_NORMAL_STOP) {
      NormalStop();
      return;
    }
  }

  if (!mDoingTS)
    return;

  // If doing a macro, keep action index the same
  if (mRunningMacro)
    mKeepActIndexSame = true;

  // If a plugin call completed, let the manager know but keep the action index the same
  if (mDoingPlugCall) {
    mWinApp->mPluginManager->CompletedTSCall();
    mKeepActIndexSame = true;
  }

  // Check errors if param >= 0
  bwIndex = mLowDoseMode && mImBufs->mImage ? 
    mCamera->ConSetToLDArea(mImBufs->mConSetUsed) : 0;

  // If an error or stop came in during one of the initialization steps, just quit
  // and relinquish control, back to postpone state
  if ((mUserStop || mErrorStop) && mActIndex >= mNumActLoop) {
    EndControl(false);
    mPostponed = true;
    return;
  }

  // Now test for error flag and stop dead: there are no valid images to check
  // for proper results.  Also test for user stop and aborted image or image sequence
  if (mErrorStop || (mUserStop && ((mAcquiringImage && mCamera->GetShotIncomplete()) || 
    (mDoingMontage && mMontageController->GetLastFailed()) ||
    (mFocusing && mFocusManager->GetLastFailed())))) {
    ErrorStop();
    return;
  }

  // Factor by which to derate a BW mean if beam shutter has jitter
  derate = mBeamShutterJitters ? 
    (float)(mImBufs->mExposure / (mImBufs->mExposure + mBeamShutterError)) : 1.f;
  
  // Limit maximum number of failures to 2 for montaging
  maxImageFailures = mMaxImageFailures;
  if (mMontaging && maxImageFailures > 2)
    maxImageFailures = 2;

  // The error checking section on the previous action
  if (param != NO_PREVIOUS_ACTION) {
    
    // First assess whether action needs to be redone because of image failure
    // Use different means for low dose; revise focus area mean if focused OK
    actionIncomplete = (mFocusing && mFocusManager->GetLastFailed()) ||
      (mDoingMontage && mMontageController->GetLastFailed() || (mWalkingUp && 
      mBidirSeriesPhase == BIDIR_RETURN_PHASE && !mComplexTasks->GetLastWalkCompleted()));
    if (mFocusing && !actionIncomplete && mLowDoseMode)
      mProcessImage->UpdateBWCriterion(mImBufs, mBWMeanPerSec[bwIndex], derate);
    needRedo = actionIncomplete && !mUserStop && !mErrorStop;
    if (mLowDoseMode)
      imageCrit = mBWMeanPerSec[bwIndex] * mBadShotCrit;
    else
      imageCrit = mBWMeanPerSec[0] * (
        mImBufs->mMagInd == magIndex ? mBadShotCrit :mBadLowMagCrit);
    if (mAcquiringImage && !mTookExtraRec)
      needRedo = !mSTEMindex && mProcessImage->ImageTooWeak(mImBufs, imageCrit);

    // Set flag to prevent advancing of index if stopped a synchronous copy step
    if (mUserStop && ((mActIndex == mActOrder[BIDIR_INVERT_FILE] && 
      !mBufferManager->GetSaveAsynchronously()) || 
      mActIndex == mActOrder[BIDIR_FINISH_INVERT]))
      mKeepActIndexSame = true;

    // For record image in low dose mode, let the user decide what to do
    if (needRedo && mLowDoseMode && mActIndex == mActOrder[RECORD_OR_MONTAGE] &&
      mImageFailures < maxImageFailures) {
        message.Format("This image does not seem to have sufficient counts\r\n"
          " (%.0f actual vs. %.0f criterion unbinned counts per sec)",
          mProcessImage->UnbinnedSpotMeanPerSec(mImBufs), imageCrit);
        error = EndFirstPartOnDimImage(angle, message, needRedo, false);
        if (error < 0)
          return;
        if (!error) {
          message += 
            "\n\nPress:\n\"Retake\" to acquire the image again,\n\n\"Go On\" to proceed "
            "with the existing image,\n\n\"Stop\" to stop the tilt series";
          policy = mPolicyID[mLDDimRecordPolicy];

          // If not generally terminating on error & flag is set to terminate here, do so
          if (mExternalControl && !mTerminateOnError && mTermNotAskOnDim) {
            policy = IDCANCEL;
            mTerminateOnError = true;
          }
          error = SEMThreeChoiceBox(message, "Retake", "Go On", "Stop", 
            MB_YESNOCANCEL | MB_ICONQUESTION, 0, policy == IDCANCEL, policy);
          if (error == IDNO) {
            needRedo = false;
            mErrorEmailSent = false;
          } else if (error == IDCANCEL) {
            ErrorStop();
            return;
          }
        }
    }

    if (needRedo) {
      mImageFailures++;
      if (mImageFailures >= maxImageFailures) {
        message.Format("There have been %d failures to obtain an image with\r\n"
          "sufficient counts (%.0f actual vs. %.0f criterion unbinned counts per sec)",
          maxImageFailures, mProcessImage->UnbinnedSpotMeanPerSec(mImBufs), imageCrit);
        error = EndFirstPartOnDimImage(angle, message, needRedo, false);
        if (error < 0)
          return;
        if (!error) {
          message +=
            "\n\nSometimes this happens even when the images are good enough.\n\n"
            "Press:\n\"Stop\" to stop the tilt series,\n\n"
            "\"Reset & Go On\" to reset the criterion and go on"; 
          policy = mPolicyID[mDimRetryPolicy];
          if (mExternalControl && !mTerminateOnError && mTermNotAskOnDim) {
            policy = IDYES;
            mTerminateOnError = 1;
          }
          if (SEMThreeChoiceBox(message, "Stop", "Reset && Go On", "", 
            MB_YESNO | MB_ICONQUESTION, 0, policy == IDYES, policy) == IDYES) {
              ErrorStop();
              return;
          } else {
            mErrorEmailSent = false;
            needRedo = false;
            mBWMeanPerSec[bwIndex] = 0.;
          }
        }
      } /*else if (mExternalControl) {

        // TEMPORARY try a long timeout for Helio's problem 4/9/11
        // Note that this gets trimmed as an unreasonable delay!  That would need fixing
        mShiftManager->SetISTimeOut(120.);
        mShiftManager->SetLastTimeoutWasIS(false);
        mWinApp->AppendToLog("Waiting two minutes before retrying...");
        } */
    } 

    // Check for focus aborted and treat like dim image as far as ending part or series
    error = mFocusManager->GetLastAborted();
    if (mFocusing && error) {
      if (error == FOCUS_ABORT_INCONSISTENT)
        message = "Autofocus has failed to behave consistently after several iterations";
      else if (error == FOCUS_ABORT_ABS_LIMIT)
        message = "Autofocus has failed; it would have made focus exceed the specified "
        "absolute limits";
      else
        message = "Autofocus has failed; it would have made focus change by more than "
        "the specified limit";
      error = EndFirstPartOnDimImage(angle, message, needRedo, false);

      // If terminating, return; if not ending first part, give message and stop
      if (error < 0)
        return;
      if (!error) {
        TSMessageBox(message, MB_EXCLAME);
        ErrorStop();
        return;
      }
    }

    // Check for too big an exposure or intensity, return if terminating or go on if
    // ending first part of bidir series, or stop
    if (mTSParam.beamControl == BEAM_INTENSITY_FOR_MEAN && mTermOnHighExposure &&
      mAccumDeltaForMean / mMinAccumDeltaForMean > mMaxExposureIncrease &&
      !mStoppedForAccumDelta) {
        message.Format("Intensity/exposure is now %.2f higher than its minimum", 
          mAccumDeltaForMean / mMinAccumDeltaForMean);
        error = EndFirstPartOnDimImage(angle, message, needRedo, true);
        if (error < 0)
          return;
        if (!error) {
          TSMessageBox(message, MB_EXCLAME);
          mStoppedForAccumDelta = true;
          ErrorStop();
          return;
        }
    }

    if (!needRedo && !mKeepActIndexSame) {

      // No failure, then zero the count
      mImageFailures = 0;
      
      // check for focus being close to prediction, only do this once
      needRedo = mFocusing && mCheckFocusPrediction && 
        fabs(mScope->GetUnoffsetDefocus() - mPredictedFocus) > mTSParam.maxFocusDiff;
      if (needRedo) {
        mCheckFocusPrediction = false;
        reason = " because measured defocus is too far from prediction";
      }

      // Check record image for data area loss - as long as there is a record
      // ref, and either not in low dose, or double align OK, or no track ref
      if (mActIndex == mActOrder[RECORD_OR_MONTAGE] && mHaveRecordRef &&
        (!mLowDoseMode || !mTSParam.alignTrackOnly || !mHaveTrackRef)) {
        
        // This is THE autoalign of the data
        if (AutoAlignAndTest(mAlignBuf, 1, mMontaging ? "Montage Center" : "Record"))
          return;

        // Get image size in X and Y, and track errors in X and Y, and
        // back-rotate these to specimen X and Y
        image = mImBufs->mImage;
        if (mMontaging) {
          nx = mMontParam->xFrame + (mMontParam->xNframes - 1) *
            (mMontParam->xFrame - mMontParam->xOverlap);
          ny = mMontParam->yFrame + (mMontParam->yNframes - 1) *
            (mMontParam->yFrame - mMontParam->yOverlap);
        } else
          image->getSize(nx, ny);
        image->getShifts(shiftX, shiftY);

        if (mLowDoseMode && mVerbose) {
          aInv = mWinApp->mShiftManager->CameraToSpecimen(mImBufs->mMagInd);
          specErrorX = mImBufs->mBinning * (aInv.xpx * shiftX - aInv.xpy * shiftY);
          specErrorY = mImBufs->mBinning * (aInv.ypx * shiftX - aInv.ypy * shiftY);
          message.Format("Record image shifted %.3f, %.3f um", specErrorX, specErrorY);
          mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
        }
 
        // See if need to shift an extra reference by the same amount
        if (!mExtraRefShifted && (mLowDoseMode && mDidRegularTrack ||
          mNeedLowMagTrack && mUsableLowMagRef)) {
          refImBuf->mImage->getShifts(refShiftX, refShiftY);
          aInv = mShiftManager->CameraToIS(magIndex);
          aMat = mShiftManager->MatMul(aInv, 
            mShiftManager->IStoCamera(refImBuf->mMagInd));
          binRatio = (float)mTSParam.binning / (float)refImBuf->mBinning;
          delX = binRatio * (aMat.xpx * shiftX - aMat.xpy * shiftY);
          delY = -binRatio * (aMat.ypx * shiftX - aMat.ypy * shiftY);
          refShiftX += delX;
          refShiftY += delY;
          refImBuf->mImage->setShifts(refShiftX, refShiftY);
          refImBuf->SetImageChanged(1);
          mExtraRefShifted = true;
          aInv = mWinApp->mShiftManager->CameraToSpecimen(magIndex);
          specErrorX = mTSParam.binning * (aInv.xpx * shiftX - aInv.xpy * shiftY);
          specErrorY = mTSParam.binning * (aInv.ypx * shiftX - aInv.ypy * shiftY);
          message.Format("%s reference shifted by %.1f,%.1f pixels, %.3f,%.3f um",
            mLowDoseMode ? "Tracking" : "Low mag", delX, -delY, specErrorX, specErrorY);
          if (mVerbose)
            mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
        }
        
        // Now evaluate error, resolved into components relative to tilt axis
        rotation = mShiftManager->GetImageRotation(curCamera, magIndex);
        cosRot = cos(DTOR * rotation);
        sinRot = sin(DTOR * rotation);

        // Get the image size in each direction by rotating two diagonals and
        // taking the maximum dimension
        specSizeX = B3DMAX(fabs(nx * cosRot + ny * sinRot), 
          fabs(nx * cosRot - ny * sinRot));
        specSizeY = B3DMAX(fabs(-nx * sinRot + ny * cosRot), 
          fabs(-nx * sinRot - ny * cosRot));

        // Invert Y!
        specErrorX = fabs(shiftX * cosRot - shiftY * sinRot);
        specErrorY = fabs(-shiftX * sinRot - shiftY * cosRot);
        
        // Get fractional errors in each direction; X error is amount missing
        fracErrorX = specErrorX / specSizeX;
        fracErrorY = specErrorY / specSizeY;
        missingX = fracErrorX;
    
        // Compute a foreshortened fraction based on the thickness being the
        // minimum of 0.4 micron and 0.3 of the width
        pixelSize = mShiftManager->GetPixelSize(curCamera, magIndex);
        fracThick = 0.4 / (pixelSize * specSizeY * mImBufs->mBinning);
        if (fracThick > 0.3)
          fracThick = 0.3;
        fsY0 = cos(DTOR * mCurrentTilt) + fracThick * 
          fabs(sin(DTOR * mCurrentTilt));
        fsY0 = B3DMAX(0., B3DMIN(1., fsY0));
        missingY = fracErrorY - (1. - fsY0) / 2.;

        // base maximum error on entered value, but make sure it is less
        // than the value for repeating records 
        // 2/26/04: Try not doing that now that reference is shiftable
        maxError = mTSParam.maxRefDisagreement;
        //if (mTSParam.repeatRecord && maxError > 0.9 * mTSParam.maxRecordShift)
        //  maxError = 0.9f * mTSParam.maxRecordShift;

        // First see if need a new tracking reference in low dose
        if (mLowDoseMode && mDidRegularTrack &&
          (fracErrorX > maxError || fracErrorY > maxError)) {
          mHaveTrackRef = false;
          if (mVerbose)
            mWinApp->AppendToLog("Getting a new tracking reference after"
            " Record to reduce reference conflicts", LOG_OPEN_IF_CLOSED);
        }

        // resolve low-mag ref disparity by forcing new reference there
        if (mNeedLowMagTrack && mUsableLowMagRef && !mDidRegularTrack &&
          (fracErrorX > maxError || fracErrorY > maxError)) {
          mRoughLowMagRef = true;
          if (mVerbose)
            mWinApp->AppendToLog("Getting a new low mag reference after"
            " Record to reduce reference conflicts", LOG_OPEN_IF_CLOSED);
        }

        // now check for redo criterion
        needRedo = mTSParam.repeatRecord && (missingX > mTSParam.maxRecordShift ||
          missingY > mTSParam.maxRecordShift);
        if (needRedo) {
          message.Format("This Record image has more than the"
            " specified fraction of data loss\r\n"
            "       (lost fractions %.3f in X, %.3f in Y)", missingX, missingY);
          if (mVerbose)
            mWinApp->AppendToLog(message);
        }
        
        // In low dose, have user confirm taking another picture or allow going on
        if (needRedo && mLowDoseMode && mTSParam.confirmLowDoseRepeat && 
          mPositionFailures < mMaxPositionFailures) {
          message += 
            "\n\nPress:\n\"Retake\" to acquire the image again,\n\n\"Go On\" to proceed "
            "with the existing image,\n\n\"Stop\" to stop the tilt series";
          policy = mPolicyID[mLDRecordLossPolicy];
          error = SEMThreeChoiceBox(message, "Retake", "Go On", "Stop", 
            MB_YESNOCANCEL | MB_ICONQUESTION, 0, policy == IDCANCEL, policy);
          if (error == IDNO) {
            needRedo = false;
            mErrorEmailSent = false;            
          } else if (error == IDCANCEL) {
            ErrorStop();
            return;
          }
        }

        if (needRedo) {
          reason = " because position is too far off";
          mPositionFailures++;
          if (mPositionFailures > mMaxPositionFailures) {
            message.Format("Stopped because of %d failures to obtain a final image\n with"
              " less than the specified fraction of data loss\n"
              "(lost fractions %.3f in X, %.3f in Y)", 
              mMaxPositionFailures, missingX, missingY);
            TSMessageBox(message, MB_EXCLAME);
            ErrorStop();
            return;
          }
          
          // If montaging, need to set Z back if the Z was actually finished
          if (mMontaging) {
            SetFileZ();
            mWinApp->mMontageWindow.Update();
          }
        } else 
          mPositionFailures = 0;
      }
    }

    // Advance index unless redoing something or focus or montage stopped
    if (!needRedo && !actionIncomplete && !mKeepActIndexSame)
      IncActIndex();

    // If user stop, and a record image passes test, want to go forward and stop
    // after saving; otherwise stop now
    // Also stop if starting out with single step and at end of startup
    if (mStartupStop || 
      (mUserStop && (mActIndex != mActOrder[SAVE_RECORD_SHOT] || needRedo))) {
      NormalStop();
      return;
    }

    // BUT, if redoing a record for any reason and the tilt has already happened
    // need to tilt back and reset for focus and tracking
    if (mActIndex == mActOrder[RECORD_OR_MONTAGE] && needRedo)
      mLastRedoIndex = mTiltIndex;
    if (mActIndex == mActOrder[RECORD_OR_MONTAGE] && needRedo && mPreTilted) {
      if (mVerbose)
        mWinApp->AppendToLog("Backing up to redo " + recordMontage + reason,
          LOG_OPEN_IF_CLOSED);

      // Tracking flags will be set properly by this routine
      StartBackingUp(1);
      if (mTrackLowMag)
        mActIndex = mActOrder[LOW_MAG_FOR_TRACKING];
      else
        mActIndex = mActOrder[TRACKING_SHOT];
      return;
    }
    if (needRedo && mVerbose) {
      if (mFocusing)
        recordMontage = "Autofocus";
      else if (mActIndex != mActOrder[RECORD_OR_MONTAGE])
        recordMontage = "image";
      mWinApp->AppendToLog("Redoing " + recordMontage + reason, LOG_OPEN_IF_CLOSED);
    }
  }

  ClearActionFlags();

  // THE ACTION LOOP

  // For each action, pass a 1 to NextActionIs to set the stop flag for single step

  for (;;) {

    //DebugDump("Top of action loop");
    if (!mDoingTS)
      return;

    // RUN A MACRO IF THE STEP HAS BEEN REACHED
    if (!mRunningMacro && mDoRunMacro && 
      mActOrder[mTableOfSteps[mStepAfterMacro - 1]] == mActIndex) {
        if (mVerbose)
          PrintfToLog("Running script # %d", mMacroToRun);
        mRunningMacro = true;
        mWinApp->mMacroProcessor->Run(mMacroToRun - 1);
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        return;
    }
    if (mRunningMacro && !mWinApp->mMacroProcessor->GetLastCompleted()) {
      ErrorStop();
      return;
    }
    mRunningMacro = false;

    // TEST FOR AND RUN A PLUGIN FUNCTION IF ANY EXIST AND THEY ARE ENABLED
    if (mCallTSplugins && mWinApp->mPluginManager->CheckAndRunTSAction(mActIndex, 
      mTiltIndex, mCurrentTilt, error)) {
        if (error < 0) {
          mDoingPlugCall = true;
          mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
          return;
        }
        if (error > 0) {
          message.Format("Stopping because call to plugin returned with error # %d",
            error);
          TSMessageBox(message, MB_EXCLAME);
          ErrorStop();
          return;
        }
        if (mUserStop) {
          NormalStop();
          return;
        }
        continue;

    // CHECK TILT ANGLE AND  OTHER THINGS AT TOP OF LOOP
    } else if (NextActionIs(mActIndex, LOOP_START_CHECKS, 1)) {
      
      // if this is first time, get the true starting angle for purposes of
      // getting regular increments; round to nearest tenth, or 0.05 if
      // increment is small enough
      if (mStartingOut) {
        SetTrueStartTiltIfStarting();
      } else if (mTiltIndex) {
  
        // Check whether our last tilt achieved the desired tilt and decide
        // whether to do a last round or stop now
        // 1/17/06: Questioned why test for error more than half the increment, which
        // could screw up on JEOL with small increments, for no good reason, so made this
        // test less stringent instead of just testing for maxtilterror
        // 11/12/08: added requirement that we are tilting UP and that we didn't tilt 
        // enough rather than too much
        angleError = fabs(angle - mNextTilt);
        if ((angleError > 0.75 * mLastIncrement || angleError > mMaxTiltError) &&
          fabs(mNextTilt) > fabs(mCurrentTilt) && 
          mDirection * (angle - mCurrentTilt) < mLastIncrement) {

          // 5/5/06: Jaap claims he sees the old angle, so wait a while and try again
          Sleep(5000);
          oldAngle = angle;
          angle = mScope->GetTiltAngle();
          angleError = fabs(angle - mNextTilt);
          if (angleError > 0.75 * mLastIncrement || angleError > mMaxTiltError) {
            message.Format("The stage is not at the expected tilt angle\r\n"
              "(at %.2f, expected %.2f, error %.2f).\r\n", angle, mNextTilt, angleError);
            mWinApp->AppendToLog(message, LOG_SWALLOW_IF_CLOSED);
            if (fabs(angle) < 30. || angle * mLastIncrement < 0.)
              message += "This is probably not"; 
            else if (fabs(angle) < 45.)
              message += "This could be"; 
            else
              message += "This is probably"; 
            message += " due to a pole touch.\n\n"
              "Press:\n\"Stop && Set Limit\" to stop and set this as a limiting "
              "('pole touch') angle,\n\n\"Go On\" to try to go on.";
            policy = mPolicyID[mTiltFailPolicy];
            if (SEMThreeChoiceBox(message, "Stop && Set Limit", "Go On", "", 
              MB_YESNO | MB_ICONQUESTION, 0, 
              policy == IDYES && angleError > 0.5 * mLastIncrement, policy) == IDYES) {
                mPoleAngle = angle;
                if (angleError > 0.5 * mLastIncrement) {
                  ErrorStop();
                  return;
                }
            }
          } else
            SEMTrace('1', "Tilt angle wrong (%.2f) initially, correct (%.2f) after delay"
              , oldAngle, angle);
        }
      }

      // Set the current tilt, and compute the next; see if time to stop
      mCurrentTilt = angle;
      AssessNextTilt(false);
      mDidRegularTrack = false;

      // It is time to use the anchor if next tilt is past its angle and the
      // current one is not
      // Put in extra ref buffer if low dose and it is not a record
      if (mHaveAnchor && mDirection * (mNextTilt - mAnchorAngle) >= 0. &&
        mDirection * (mCurrentTilt - mAnchorAngle) < 0.) {
        if (mLowDoseMode) {
          mHaveTrackRef = 
            mCamera->ConSetToLDArea(mImBufs[mAnchorBuf].mConSetUsed) < 3;
          mHaveRecordRef = !mHaveTrackRef;
          if (mHaveTrackRef)
            NewLowDoseRef(mAnchorBuf);
          else
            mBufferManager->CopyImageBuffer(mAnchorBuf, mAlignBuf);
        } else
          mBufferManager->CopyImageBuffer(mAnchorBuf, mAlignBuf);
        if (mTiltIndex > 0)
          mDisturbance[mTiltIndex - 1] |= NEW_REFERENCE;
        if (mVerbose)
          mWinApp->AppendToLog("Replacing reference image with anchor",
            LOG_OPEN_IF_CLOSED);
      }

     // CHECK WHETHER TO RESET IMAGE SHIFTS
    } else if (NextActionIs(mActIndex, CHECK_RESET_SHIFT, 1) ||
      NextActionIs(mActIndex, BIDIR_CHECK_SHIFT, 1)) {
        mScope->GetLDCenteredShift(ISX, ISY);
        if (mShiftManager->RadialShiftOnSpecimen(ISX, ISY, magIndex) > mTSParam.ISlimit) {
          if (mSkipBeamShiftOnAlign)
            mComplexTasks->SetSkipNextBeamShift(true);
          mComplexTasks->ResetShiftRealign();
          mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
          mResettingShift = true;
          mDidResetIS = true;
          if (mTiltIndex > 0) 
            mDisturbance[mTiltIndex - 1] |= IMAGE_SHIFT_RESET;
          if (mVerbose)
            mWinApp->AppendToLog("Resetting image shifts", LOG_OPEN_IF_CLOSED);
          return;
        }

    // VARY EXPOSURE, FOCUS, ETC
    } else if (NextActionIs(mActIndex, IMPOSE_VARIATIONS, 1)) {
      if (ImposeVariations(angle))
        return;
          
    // CHECK FOR AND REFINE THE ZLP
    } else if (NextActionIs(mActIndex, CHECK_REFINE_ZLP, 1)) {
      if ((mFilterParams->slitIn || (mLowDoseMode && 
        (mLDParam[0].slitIn || mLDParam[1].slitIn ||
        mLDParam[2].slitIn || mLDParam[3].slitIn))) && 
        SEMTickInterval(1000. * mFilterParams->alignZLPTimeStamp) > 
        60000. * mTSParam.zlpInterval) {

        // Align the ZLP if it is time, and at least one parameter has slit in
        if (!mWinApp->mFilterTasks->RefineZLP(false)) {
          ErrorStop();
          return;
        }
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mRefiningZLP = true;
        if (mTiltIndex > 0) 
          mDisturbance[mTiltIndex - 1] |= REFINED_ZLP;
        return;
      }

    // CHECK FOR BEAM AUTOCENTERING
    } else if (NextActionIs(mActIndex, CHECK_AUTOCENTER, 1)) {
      if (mMultiTSTasks->AutocenterBeam()) {
        ErrorStop();
        return;
      }
      mAutocenStamp = 0.001 * GetTickCount();
      if (mTiltIndex > 0 && 
        fabs((double)mTiltAngles[mTiltIndex - 1]) < mTSParam.cenBeamAngle &&
        fabs(mCurrentTilt) >= mTSParam.cenBeamAngle)
        mDidCrossingAutocen = true;
      mCenteringBeam = true;
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      return;
      
      // COMPUTE PREDICTIONS
    } else if (NextActionIs(mActIndex, COMPUTE_PREDICTIONS, 1)) {
      ComputePredictions(angle);
        
    // LOW MAG PICTURE FOR TRACKING
    } else if (NextActionIs(mActIndex, LOW_MAG_FOR_TRACKING, 1)) {
      AcquireLowMagImage(true);
      return;
    
    // ALIGN FROM LOW MAG IMAGE, and make it a new reference
    } else if (NextActionIs(mActIndex, ALIGN_LOW_MAG_SHOT, 1)) {
      RestoreMag();
      if (SaveExtraImage(TRACKING_CONSET, "Low mag tracking"))
        return;
      if (mShiftManager->AutoAlign(mExtraRefBuf, 0)) {
        ErrorStop();
        return;
      }
      
      // Make it a new reference unless the previous one was a rough one
      if (!mRoughLowMagRef) {
        mBufferManager->CopyImageBuffer(0, mExtraRefBuf);
        mHaveLowMagRef = true;
        mLowMagRefTilt = angle;
        mExtraRefShifted = false;
      }
    
    // REGULAR TRACKING SHOT
    } else if (NextActionIs(mActIndex, TRACKING_SHOT, 1) || 
      NextActionIs(mActIndex, POST_FOCUS_TRACKING, 1) ||
      NextActionIs(mActIndex, BIDIR_TRACK_SHOT, 1)) {
        if (mShiftManager->ShiftAdjustmentForSet(TRACKING_CONSET, 
          magIndex, shiftX, shiftY))
          mCamera->AdjustForShift(shiftX, shiftY);
        mCamera->InitiateCapture(TRACKING_CONSET);
        AddToDoseSum(TRACKING_CONSET);
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mAcquiringImage = true;
        mDidRegularTrack = true;
        return;

    // ALIGN FROM TRACKING SHOT
    } else if (NextActionIs(mActIndex, ALIGN_TRACKING_SHOT, 1) || 
      NextActionIs(mActIndex, ALIGN_POST_FOCUS, 1)) {
      lastOrOnly = (mActIndex == mActOrder[ALIGN_POST_FOCUS]) || 
        (mTSParam.trackAfterFocus == TRACK_BEFORE_FOCUS);
      if (SaveExtraImage(TRACKING_CONSET, "Tracking"))
        return;
      if (mLowDoseMode) {

        // If this is the first align in regular loop, save track reference
        if (mNeedFirstTrackRef) 
          mBufferManager->CopyImageBuffer(mExtraRefBuf, mAnchorBuf);
        mNeedFirstTrackRef = false;
          
       // If low dose, autoalign to the extra buffer, and if this is the
        // last or only tracking shot, copy buffers
        // so as to roll the stack and make this the next reference
        if (AutoAlignAndTest(mExtraRefBuf, 0, "tracking"))
          return;
        mExtraRefShifted = false;
        if (lastOrOnly) {
          CenterBeamWithTrial();
          NewLowDoseRef(0);
          mLastLDTrackAngle = angle;
        }
      } else
        if (AutoAlignAndTest(mAlignBuf, 0, "Trial"))
          return;

      // Update the BW mean value if low dose or accurate and it is last track
      if ((bwIndex ||!mBeamShutterJitters) && lastOrOnly)
        mProcessImage->UpdateBWCriterion(mImBufs, mBWMeanPerSec[bwIndex], derate);

      // Compute  position error  
      mImBufs->mImage->getShifts(shiftX, shiftY);
      aInv = mWinApp->mShiftManager->CameraToSpecimen(mImBufs->mMagInd);
      specErrorX = mImBufs->mBinning * (aInv.xpx * shiftX - aInv.xpy * shiftY);
      specErrorY = mImBufs->mBinning * (aInv.ypx * shiftX - aInv.ypy * shiftY);
      if (mLowDoseMode && mVerbose) {
        mScope->GetLDCenteredShift(ISX, ISY);
        delX = (float)(mCmat.xpx * ISX + mCmat.xpy * ISY);
        delY = (float)(mCmat.ypx * ISX + mCmat.ypy * ISY);
        message.Format("Tracking image shifted %.3f, %.3f um;  position %.3f, %.3f",
          specErrorX, specErrorY, delX, delY);
        mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
      }

      // Adjust the focus if this was taken after an autofocus
      if (mActIndex == mActOrder[ALIGN_POST_FOCUS] && mNeedFocus && mDidFocus[mTiltIndex])
      {
        float focusFac = mShiftManager->GetDefocusZFactor();
        if (mSTEMindex)
          focusFac = -1.f / mFocusManager->GetSTEMdefocusToDelZ(-1);
        mScope->IncDefocus(specErrorY * tan(DTOR * angle) * focusFac);  
      }

      // If doing intensity for mean, do it now unless low dose or inaccurate
      if (!mLowDoseMode && !mBeamShutterJitters && lastOrOnly && 
        SetIntensityForMean(angle))
        return;

      // If manual tracking, stop now, set flag to take A as reference on resume
      if (lastOrOnly && mTSParam.manualTracking) {
        mUseNewRefOnResume = true;
        mUserStop = true;
      }

    // AUTOFOCUS IF NEEDED    
    } else if (NextActionIs(mActIndex, AUTOFOCUS, 1) || 
      NextActionIs(mActIndex, BIDIR_AUTOFOCUS, 1)) {
      if (!mSTEMindex)
        mFocusManager->SetRequiredBWMean(mBWMeanPerSec[mLowDoseMode ? 1 : 0] * 
        mBadShotCrit);
      if (mTSParam.limitDefocusChange)
        mFocusManager->NextFocusChangeLimits(-mTSParam.defocusChangeLimit, 
          mTSParam.defocusChangeLimit);
      mFocusManager->AutoFocusStart(1);
      mDidFocus[mTiltIndex] = true;
      AddToDoseSum(FOCUS_CONSET);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mFocusing = true;
      return;
    
    // SAVE AUTOFOCUS
    } else if (NextActionIs(mActIndex, SAVE_AUTOFOCUS, 1) ||
      NextActionIs(mActIndex, SAVE_STARTUP_AUTOFOCUS, 0)) {
      if (SaveExtraImage(FOCUS_CONSET, "Autofocus"))
        return;

    // GET A DEFERRED SUM
    } else if (NextActionIs(mActIndex, GET_DEFERRED_SUM, 0)) {
      StartGettingDeferredSum(DEFSUM_LOOP);
      return;

    // TAKE RECORD OR MONTAGE
    } else if (NextActionIs(mActIndex, RECORD_OR_MONTAGE, 0)) {

      // Force one dark reference when beginning
      if (!mTiltIndex && !mSTEMindex)
        mConSets[RECORD_CONSET].onceDark = 1;

      if (mMontaging) {
        if (!mSTEMindex)
          mMontageController->SetRequiredBWMean(mBWMeanPerSec[0] * mBadShotCrit);
        if (mMontageController->StartMontage(MONT_NOT_TRIAL, false)) {
          ErrorStop();
          return;
        }
        mDoingMontage = true;
      } else {

        // Set up for early return and set deferred sum if all conditions fit
        if (mTSParam.doEarlyReturn && mCamParams->K2Type && !mMontaging && 
          !mNumExtraRecords) {
            cset = &mConSets[RECORD_CONSET];
            if (cset->doseFrac && ((cset->alignFrames && cset->useFrameAlign == 1) ||
              cset->saveFrames) && B3DNINT(cset->exposure / cset->frameTime) > 
              mTSParam.earlyReturnNumFrames && !mConSets[FOCUS_CONSET].doseFrac && 
              !mConSets[TRIAL_CONSET].doseFrac && !(mLowDoseMode && 
              (!mConSets[VIEW_CONSET].doseFrac || !mConSets[PREVIEW_CONSET].doseFrac))) {
                mCamera->SetNextAsyncSumFrames(mTSParam.earlyReturnNumFrames, true);
                mNeedDeferredSum = true;
                mSecForDeferredSum = mOverwriteSec < 0 ? mWinApp->mStoreMRC->getDepth() :
                  mOverwriteSec;
            }
        }

        // Good luck maintaining consistency with conditions that cause
        // actions after a record!
        if (mNeedTilt && !mSingleStep && !mStopAtLoopEnd && mActPostExposure &&
          !((mNeedLowMagTrack && !mUsableLowMagRef) || mRoughLowMagRef) &&
          !(mLowDoseMode && !mHaveTrackRef) && !mTSParam.manualTracking &&
          !mNeedTiltAfterMove && mTiltIndex - mLastStartedIndex > 2 &&
          mTiltIndex - mLastRedoIndex > 1 && !mNumExtraRecords) {
          smi.axisBits = axisA;
          smi.alpha = mNextTilt;
          delay = mShiftManager->GetAdjustedTiltDelay(fabs(mNextTilt - mCurrentTilt));
          mCamera->QueueStageMove(smi, delay);
          mPreTilted = true;
          mNeedGoodAngle = true;
        }
        mCamera->SetMaxChannelsToGet(mTSParam.extraRecordType == TS_OTHER_CHANNELS ? 
          3 : 1);
        if (mEarlyK2RecordReturn)
          mCamera->SetFullSumAsyncIfOK(RECORD_CONSET);
        mCamera->InitiateCapture(RECORD_CONSET);
        mAcquiringImage = true;
      }
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mShotAngle = angle;
      return;
    
    // SAVE A RECORD SHOT, also adjust intensity
    } else if (NextActionIs(mActIndex, SAVE_RECORD_SHOT, 1)) {

      // Initialize extra record counter and flag
      mExtraRecIndex = 0;
      mTookExtraRec = false;
      SetTrueStartTiltIfStarting();

      // Update the BW mean value and copy to align buffer
      mProcessImage->UpdateBWCriterion(mImBufs, mBWMeanPerSec[bwIndex], derate);
      if (!mMontaging) {
        if (mOverwriteSec < 0)
          error = mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC);
        else
          error = mBufferManager->OverwriteImage(mWinApp->mStoreMRC, mOverwriteSec);
        if (error) {
          // Messages or queries have already been issued
          //AfxMessageBox("Error writing image to file", 
          //  MB_EXCLAME);
          ErrorStop();
          return;
        }
      }

      mBufferManager->CopyImageBuffer(0, mAlignBuf);
      mHaveRecordRef = true;
      int spotSize = mScope->FastSpotSize();

      // Add record or overview to the stack, return to main buffer so black/white
      // don't become 0/255
      if (mTSParam.stackRecords) {
        mDeferredSumStackSec = mTiltIndex + mBaseZ;
        mBufferManager->AddToStackWindow(mMontaging ? 1 : 0, mTSParam.stackBinSize, 
        mTiltIndex + mBaseZ, true);
        mWinApp->SetCurrentBuffer(mWinApp->GetImBufIndex());
      }

      // Commit values for this tilt to the table
      mTiltAngles[mTiltIndex] = (float)mShotAngle;
      mDisturbance[mTiltIndex] = 0;
      mImBufs->mImage->getShifts(mAlignShiftX[mTiltIndex], 
        mAlignShiftY[mTiltIndex]);
      mTimes[mTiltIndex] = (float)mImBufs->mTimeStamp;

      // SHOULD THIS BE CENTERED
      mScope->GetLDCenteredShift(ISX, ISY);
      mSpecimenX[mTiltIndex] = (float)(mCmat.xpx * ISX + mCmat.xpy * ISY);
      mSpecimenY[mTiltIndex] = (float)(mCmat.ypx * ISX + mCmat.ypy * ISY);
      mSpecimenZ[mTiltIndex++] = mImBufs->mDefocus - 
        (mSTEMindex ? 0.f : mTSParam.targetDefocus);
      
      // One way or another, get the mean
      shotIntensity = mLowDoseMode 
        ? mLDParam[RECORD_CONSET].intensity : mScope->FastIntensity();
      mLastMean = -1000.;
      error = SetIntensityForMean(angle);
      if (mLastMean == -1000.)
        mLastMean = mProcessImage->EquivalentRecordMean(0);

      // Save first intensity after adjusting it
      if (mTiltIndex == 1) {
        mFirstIntensity = mLowDoseMode ? mLDParam[3].intensity : mScope->GetIntensity();
        mFirstExposure = mConSets[RECORD_CONSET].exposure;
      }
      
      message.Format("Saved Z = %d  at %.2f min  Tilt = %.2f  Mean = %.0f  ",
        mBaseZ + mTiltIndex - 1,(mImBufs->mTimeStamp - mTimes[0]) / 60.,
        mShotAngle, mLastMean); 
      if (mLowDoseMode) {
        message2.Format("%s = %.2f%s  ", mScope->GetC2Name(), 
          mScope->GetC2Percent(spotSize, shotIntensity), mScope->GetC2Units());
        message += message2;
      }

      // Add dose in any mode if calibrated
      dose = AddToDoseSum(RECORD_CONSET);
      if (dose) {
        message2.Format("dose = %.2f e/A2  ", dose);
        message += message2;
      }

      message2.Format("X/Y/Z = %7.3f  %7.3f  %7.3f", mSpecimenX[mTiltIndex - 1],
        mSpecimenY[mTiltIndex - 1], mSpecimenZ[mTiltIndex - 1]);
      message += message2;
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
      if (mAutosaveLog || (mExternalControl && mAutoSavePolicy))
        mWinApp->mLogWindow->UpdateSaveFile(true, mWinApp->mStoreMRC->getName());
      mDidFocus[mTiltIndex] = false;
      SetFileZ();
      
      // If intensity gives an error, advance index to avoid re-saving image 
      if (error) {
        mActIndex++;
        return;
      }
      
      // TAKE EXTRA RECORD SHOT
    } else if (NextActionIs(mActIndex, EXTRA_RECORD, 0)) {
      if (SetExtraRecordState())
        return;
      extraSet = mTSParam.extraRecordType == 
        TS_OPPOSITE_TRIAL ? TRIAL_CONSET : RECORD_CONSET;
      if (mSTEMindex)
        mCamera->SetMaxChannelsToGet(1);
      if (mDocWnd->StoreIsMontage(mExtraFiles[RECORD_CONSET])) {

        // For a montage, switch to the file the first time, saving old store # as flag
        if (mSwitchedToMontFrom < 0) {
          mSwitchedToMontFrom = mDocWnd->GetCurrentStore();
          mDocWnd->SetCurrentStore(mExtraFiles[RECORD_CONSET]);
        }
        if (mExtraOverwriteSec < 0)
          mMontParam->zCurrent = mExtraFreeZtoWrite;
        else
          mMontParam->zCurrent = mExtraOverwriteSec;
        mMontageController->SetRequiredBWMean(-1.);
        if (mMontageController->StartMontage(MONT_NOT_TRIAL, false)) {
          ErrorStop();
          return;
        }
        mDoingMontage = true;
      } else {

        // Otherwise, take single image
        if (mEarlyK2RecordReturn)
          mCamera->SetFullSumAsyncIfOK(extraSet);
        mCamera->InitiateCapture(extraSet);
        AddToDoseSum(extraSet);
        mAcquiringImage = true;
      }
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mTookExtraRec = true;
      return;

       // SAVE EXTRA RECORD SHOT
    } else if (NextActionIs(mActIndex, SAVE_EXTRA_RECORD, 1)) {
      if (mTSParam.extraRecordType == TS_OTHER_CHANNELS && mNumSimultaneousChans > 1)
        SetupExtraOverwriteSec();
      extraSet = mTSParam.extraRecordType == 
        TS_OPPOSITE_TRIAL ? TRIAL_CONSET : RECORD_CONSET;
      message.Format("Extra %s #%d", extraSet == RECORD_CONSET ? "Record" : "Trial",
        mExtraRecIndex + 1);
      if (SaveExtraImage(RECORD_CONSET, (char *)((LPCTSTR)message))) {
        if (mTerminateOnError)
          return;
        mTookExtraRec = false;
        if (AfxMessageBox("When you resume, do you want to try to retake and save the\n"
          "last extra image again (Yes) or just go on without saving (No)?", 
          MB_YESNO | MB_ICONQUESTION) == IDYES)
          mActIndex -= 2;
        return;
      }
      mTookExtraRec = false;
      mExtraRecIndex++;

      if (mExtraRecIndex < mNumExtraRecords) {
        mActIndex -= 2;
      } else {
        RestoreFromExtraRec();
      }

      // GET TRACK BEFORE A NEW LOW MAG REF
    } else if (NextActionIs(mActIndex, TRACK_BEFORE_LOWMAG_REF, 1)) {
      if (mShiftManager->ShiftAdjustmentForSet(TRACKING_CONSET, 
        magIndex, shiftX, shiftY))
          mCamera->AdjustForShift(shiftX, shiftY);
      mCamera->InitiateCapture(TRACKING_CONSET);
      AddToDoseSum(TRACKING_CONSET);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mAcquiringImage = true;
      return;

    // ALIGN THE TRACK TO RECORD REFERENCE
    } else if (NextActionIs(mActIndex, ALIGN_LOWMAG_PRETRACK, 1)) {
      if (SaveExtraImage(TRACKING_CONSET, "Tracking"))
        return;
      if (AutoAlignAndTest(mAlignBuf, 1, "Trial"))
        return;

    // GET LOW MAG REFERENCE IF ONE IS NEEDED
    } else if (NextActionIs(mActIndex, LOW_MAG_REFERENCE, 1) ||
      NextActionIs(mActIndex, BIDIR_LOWMAG_REF, 1)) {
      AcquireLowMagImage(false);
      return;

    // COPY THE NEW LOW MAG REFERENCE
    } else if (NextActionIs(mActIndex, COPY_LOW_MAG_REF, 1) ||
      NextActionIs(mActIndex, BIDIR_COPY_LOWMAG_REF, 1)) {
      //RestoreMag();
      if (SaveExtraImage(TRACKING_CONSET, "Low mag reference"))
        return;
      mBufferManager->CopyImageBuffer(0, mExtraRefBuf);
      mHaveLowMagRef = true;
      mLowMagRefTilt = angle;
      mRoughLowMagRef = false;

    // GET PREVIEW IF NEEDED FOR A NEW TRACK REFERENCE OR STARTUP TRACKING
    } else if (NextActionIs(mActIndex, LOWDOSE_PREVIEW, 1) || 
      NextActionIs(mActIndex, STARTUP_PREVIEW, 0) || 
      NextActionIs(mActIndex, ZEROTILT_PREVIEW, 0)) {
        if (mShiftManager->ShiftAdjustmentForSet(PREVIEW_CONSET, 
          magIndex, shiftX, shiftY))
          mCamera->AdjustForShift(shiftX, shiftY);
        mCamera->InitiateCapture(PREVIEW_CONSET);
        AddToDoseSum(PREVIEW_CONSET);
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mAcquiringImage = true;
        return;

        // ALIGN THE PREVIEW TO RECORD REFERENCE
    } else if (NextActionIs(mActIndex, ALIGN_LOWDOSE_PREVIEW, 1) ||
      NextActionIs(mActIndex, ALIGN_STARTUP_PREVIEW, 0) ||
      NextActionIs(mActIndex, ALIGN_ZEROTILT_PREVIEW, 0)) {
      if (SaveExtraImage(PREVIEW_CONSET, "Preview tracking"))
        return;
      if (AutoAlignAndTest(mAlignBuf, 1, "Preview"))
        return;

    // GET LOW DOSE TRACK REFERENCE IF ONE IS NEEDED, OR ZERO-TILT TRACKING SHOT
    // OR A TRACKING SHOT AS QUICK AS POSSIBLE IF DID WALKUP BUT NO USABLE IMAGE
    } else if (NextActionIs(mActIndex, LOWDOSE_TRACK_REF, 1) ||
      NextActionIs(mActIndex, ZEROTILT_TRACK_SHOT, 0) ||
      NextActionIs(mActIndex, POST_WALKUP_SHOT)) {
        if (mShiftManager->ShiftAdjustmentForSet(TRACKING_CONSET, 
          magIndex, shiftX, shiftY))
          mCamera->AdjustForShift(shiftX, shiftY);
        mCamera->InitiateCapture(TRACKING_CONSET);
        AddToDoseSum(TRACKING_CONSET);
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mAcquiringImage = true;
        return;

    // COPY THE NEW LOW DOSE REFERENCE
    } else if (NextActionIs(mActIndex, COPY_LOWDOSE_REF, 1)) {
      if (SaveExtraImage(TRACKING_CONSET, "Low dose reference"))
        return;
      CenterBeamWithTrial();
      NewLowDoseRef(0);
      mHaveTrackRef = true;
      mLastLDTrackAngle = angle;

    // START ACTIONS FOR TURNING AROUND IN BIDIR SERIES
    // LOAD THE STARTING IMAGE AS REFERENCE BEFORE TOUCHING THE FILE
    } else if (NextActionIs(mActIndex, BIDIR_RELOAD_REFS, 1)) {

      // Do one-time adjustment of phase and direction and ending tilt angle
      if (mBidirSeriesPhase == BIDIR_FIRST_PART) {
        mBidirSeriesPhase = BIDIR_RETURN_PHASE;
        ManageTerminateMenu();
        mDirection = -mDirection;
        mEndingTilt = mTSParam.endingTilt;
        mPartStartTiltIndex = mTiltIndex;
        mCurrentTilt = mStartingTilt;
        mPoleAngle = mDirection * 90.;
        mCheckFocusPrediction = false;
        for (nx = mTrackStackBuf; nx < mNumTrackStack + mTrackStackBuf; nx++)
          mImBufs[nx].DeleteImage();
      } 

      if (mMontaging) {
        error = mMontageController->ReadMontage(0, NULL, NULL, false, true);
        if (!error) 
          mBufferManager->CopyImageBuffer(0, mAlignBuf);
      } else {
        error = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, 0);
        if (!error) 
          mBufferManager->CopyImageBuffer(mReadBuf, mAlignBuf);
      }
      if (error) {
        ErrorStop();
        return;
      }

    // INVERT THE FILE OR THE PIECE LIST
    } else if (NextActionIs(mActIndex, BIDIR_INVERT_FILE, 1)) {

      // Start or resume the copying of the file
      error = mMultiTSTasks->InvertFileInZ(mBaseZ + mTiltIndex);
      if (error > 0) {
        if (!mAlreadyTerminated)
          TerminateOnError();
        return;
      }

      // Set a task for synchronous copy with VERY generous timeout
      if (error < 0) {
        mInvertingFile = true;
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, (mBaseZ + mTiltIndex) * (20000 + 
          4 * mWinApp->mStoreMRC->getWidth() *  mWinApp->mStoreMRC->getHeight() / 1000));
        return; 
      }

    // GET BACK TO THE STARTING ANGLE AND POSITION SOMEHOW
    } else if (NextActionIs(mActIndex, BIDIR_RETURN_TO_START, 1)) {

      // Start a walkup or start tilting back by Wim's 5 degree increments
      if (mWalkBackForBidir) {
        mComplexTasks->WalkUp(mStartingTilt, -1, 0.);
        mWalkingUp = true;
      } else {

        // Keep the index the same to come back here unless the last time was last tilt
        mKeepActIndexSame = !mLastBidirReturnTilt;
        mLastBidirReturnTilt = true;
        if (mKeepActIndexSame) {
          if (mDirection * (mStartingTilt - angle) > (1. + mStepForBidirReturn)) {
            smi.alpha = angle + mStepForBidirReturn * mDirection;
            mLastBidirReturnTilt = false;
            smi.doBacklash = false;
          } else {
            smi.alpha = mStartingTilt;
            smi.backAlpha = mDirection * mComplexTasks->GetTiltBacklash();
            smi.doBacklash = true;
          }
          smi.doRelax = false;
          smi.axisBits = axisA;
        } else {

          // Move stage when tilting is done
          mMultiTSTasks->SetAnchorStageXYMove(smi);
        }
        mScope->MoveStage(smi, smi.doBacklash);
        mTilting = true;
        mNeedGoodAngle = true;
      }
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, mWalkBackForBidir ? 0 : 60000);
      return; 

    // REALIGN TO THE ANCHOR IMAGE
    // OR AS A STARTUP ACTION, TAKE AN ANCHOR IMAGE
    } else if (NextActionIs(mActIndex, BIDIR_REALIGN_SHOT, 1) ||
      NextActionIs(mActIndex, BIDIR_ANCHOR_SHOT)) {
        if (mBidirSeriesPhase == BIDIR_RETURN_PHASE) {

          // First restore intensity or exposure time and other conditions
          if (mTSParam.doVariations)
            ImposeVariations(angle);
          if (mUseExpForIntensity) {
            delFac = 0.;
            ChangeExposure(delFac, angle);
          } else if (!mSTEMindex) {
            mScope->SetIntensity(mFirstIntensity);
            if (mLowDoseMode)
              mLDParam[RECORD_CONSET].intensity = mFirstIntensity;
          }


        }
        if (mMultiTSTasks->BidirAnchorImage(
          mTSParam.bidirAnchorMagInd[2 * mSTEMindex + (mLowDoseMode ? 1 : 0)], 
          mTSParam.anchorBidirWithView, mBidirSeriesPhase == BIDIR_RETURN_PHASE,
          !mWalkBackForBidir)) {
            ErrorStop();
            return;
        }
        mTakingBidirAnchor = true;
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        return;

    // REVERSE TILT IF DID NOT DO WALK BACK
    } else if (NextActionIs(mActIndex, BIDIR_REVERSE_TILT, 1)) {
      if (mSkipBeamShiftOnAlign)
        mComplexTasks->SetSkipNextBeamShift(true);
      if (mComplexTasks->ReverseTilt(mDirection)) {
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mReversingTilt = true;
        mNeedGoodAngle = true;
        return;
      }

    // ALIGN TO THE TRACKING SHOT, MANAGE OTHER ITEMS
    } else if (NextActionIs(mActIndex, BIDIR_ALIGN_TRACK, 1)) {
      if (mLowDoseMode) {
        mBufferManager->CopyImageBuffer(mAnchorBuf, mExtraRefBuf);
        NewLowDoseRef(mExtraRefBuf);
        mLastLDTrackAngle = angle;
        if (AutoAlignAndTest(mExtraRefBuf, 0, "tracking"))
          return;
        mExtraRefShifted = false;  // WHAT DOES THIS MEAN?
      } else
        if (AutoAlignAndTest(mAlignBuf, 0, "Trial", true))
          return;

      // Other "final" changes: note that low mag track and finish copy happen after this
      // Reset cumulative intensity change to 1 since we are back to original intensity
      mDisturbance[mTiltIndex - 1] |= IMAGE_SHIFT_RESET;
      mBidirSeriesPhase = BIDIR_SECOND_PART;
      mAccumDeltaForMean = 1.;
      AssessNextTilt(false);
      SetFileZ();
      if (mComplexTasks->GetMinTASMField() > 0.)
        mNeedTiltAfterMove = 1;

    // FINISH INVERTING THE FILE
    } else if (NextActionIs(mActIndex, BIDIR_FINISH_INVERT, 1)) {
      if (mMultiTSTasks->BidirCopyPending()) {
        mMultiTSTasks->StopBidirFileCopy();

        // On failure, it should already have stopped or whatever
        if (mBufferManager->CheckAsyncSaving())
          return;
      }

      // Restart synchronously and set task with big timeout
      if (mMultiTSTasks->BidirCopyPending()) {
        mMultiTSTasks->StartBidirFileCopy(true);
        mInvertingFile = true;
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, (mBaseZ + mTiltIndex) * (20000 + 
          4 * mWinApp->mStoreMRC->getWidth() *  mWinApp->mStoreMRC->getHeight() / 1000));
        return; 
      }

    // TILT IF THAT IS STILL NEEDED: but stop first if single-step
    } else if (NextActionIs(mActIndex, TILT_IF_NEEDED, 1)) {
      if (mStopAtLoopEnd) {
        mUserStop = !mReachedEndAngle;
        if (mTermAtLoopEnd) {

          // Here is normal termination under external control
          mLastSucceeded = 2;
          TerminateOnError();
        } else
          NormalStop();       
        return;
      }
      mStopAtLoopEnd = mSingleStep > 0;

      // Now change intensity if doing cosine
      if (mTSParam.beamControl == BEAM_INVERSE_COSINE) {
        delFac = pow(cos(DTOR * mCurrentTilt) / cos(DTOR * mNextTilt), 
          1. / mTSParam.cosinePower);
        if (SetIntensity(delFac, mNextTilt))
          return;
      }

      if (mNeedTilt) 
        mCurrentTilt = mNextTilt;   // ?? seems right
      if (mNeedTilt && !mPreTilted) {
        if (mNeedTiltAfterMove) {
          if (mSkipBeamShiftOnAlign)
            mComplexTasks->SetSkipNextBeamShift(true);
          mComplexTasks->TiltAfterStageMove((float)mNextTilt, mNeedTiltAfterMove < 0);
        } else
          mScope->TiltTo(mNextTilt);
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, mNeedTiltAfterMove != 0 ? 0 : 60000);
        mDidTiltAfterMove = mNeedTiltAfterMove != 0;
        mNeedTiltAfterMove = 0;
        mTilting = true;
        mNeedGoodAngle = true;
        return;
      }
      mPreTilted = false;

    // INITIALIZATION ACTIONS BELOW HERE

    // ALIGN THE TRACKING SHOT BEFORE STARTING ACTIONS (SHOT TAKEN ABOVE)
    } else if (NextActionIs(mActIndex, ALIGN_ZEROTILT_TRACK, 0)) {
      if (SaveExtraImage(TRACKING_CONSET, "Zero-tilt tracking"))
        return;
      if (mShiftManager->AutoAlign(1, 0)) {   // What should the second arg be?
         ErrorStop();
         return;
      }

    // REFINE EUCENTRICITY if starting near zero
    } else if (NextActionIs(mActIndex, EUCENTRICITY)) {
      mComplexTasks->FindEucentricity(FIND_EUCENTRICITY_FINE | 
        REFINE_EUCENTRICITY_ALIGN);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mFindingEucentricity = true;
      mNeedGoodAngle = true;
      return;

    // WALK UP if not near enough to the starting tilt
    } else if (NextActionIs(mActIndex, WALK_UP) && 
      fabs(angle - mStartingTilt) > 0.1) {

      // Analyze whether to leave an anchor in the walkup:
      // It must be requested and not already provided, and the positive or
      // negative of the angle must fit in range to be tilted through
      anchorBuf = -1;
      anchorAngle = (float)fabs((double)mTSParam.anchorTilt);
      if (mTSParam.leaveAnchor && !mHaveAnchor && !mTSParam.doBidirectional &&
        WalkupCanTakeAnchor(angle, anchorAngle, mStartingTilt)) {
        anchorBuf = mAnchorBuf;
        mHaveAnchor = true;
        if (mStartingTilt < 0.)
          anchorAngle = -anchorAngle;
      }

      mWalkupImageUsable = mComplexTasks->WalkUp(mStartingTilt, anchorBuf, 
        anchorAngle);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mDidWalkup = true;
      mWalkingUp = true;
      mNeedGoodAngle = true;
      return;
      
    // AUTOFOCUS next in starting sequence - need to move existing reference unless
    // it is low dose tracking shot
    } else if (NextActionIs(mActIndex, STARTUP_AUTOFOCUS)) {
      if (mHaveStartRef) { 
        if (!mLowDoseMode || 
          mCamera->ConSetToLDArea(refImBuf->mConSetUsed) == 3) {
          mBufferManager->CopyImageBuffer(mExtraRefBuf, mAlignBuf);
          mHaveRecordRef = true;
        } else
          mHaveTrackRef = true;

      }
      mNeedFocus = mDidWalkup && !mSkipFocus;
      if (mDidWalkup) {

        // If got an anchor in the walkup, get the real anchor angle
        // In STEM, the anchor is not shift-compensated...
        if (mHaveAnchor)
          mHaveAnchor = mImBufs[mAnchorBuf].GetTiltAngle(mAnchorAngle);
        mDidWalkup = false;

        // If there is not a starting reference, take the aligned last image
        // from the walkup, or the post-walkup shot, as reference, unless in STEM;
        // in that case get a new ref that will be properly shifted
        if (!mHaveStartRef && !mSTEMindex) {
          mBufferManager->CopyImageBuffer(0, mLowDoseMode ? mExtraRefBuf : mAlignBuf);
          if (mLowDoseMode)
            mHaveTrackRef = true;
          else
            mHaveRecordRef = true;
          mHaveStartRef = true;
        }

        if (!mSkipFocus) {
          if (!mSTEMindex)
            mFocusManager->SetRequiredBWMean
              (mBWMeanPerSec[mLowDoseMode ? 1 : 0] * mBadShotCrit);
          if (mTSParam.limitDefocusChange)
            mFocusManager->NextFocusChangeLimits(-mTSParam.defocusChangeLimit, 
              mTSParam.defocusChangeLimit);
          mFocusManager->AutoFocusStart(1);
          AddToDoseSum(FOCUS_CONSET);
          mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
          mFocusing = true;
          return;
        }
      }

    // TRACKING SHOT if no reference already or needed for intensity setting
    } else if (NextActionIs(mActIndex, STARTUP_TRACK_SHOT)) {
      
      // If this is being done for intensity control and the beam shutter is
      // not OK, take a record shot instead
      conSet = mTSParam.beamControl == BEAM_INTENSITY_FOR_MEAN && !mLowDoseMode &&
        mBeamShutterJitters ? RECORD_CONSET : TRACKING_CONSET;
      if (conSet == TRACKING_CONSET && mShiftManager->ShiftAdjustmentForSet(
        TRACKING_CONSET, magIndex, shiftX, shiftY))
          mCamera->AdjustForShift(shiftX, shiftY);
      mCamera->InitiateCapture(conSet);
      AddToDoseSum(conSet);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mAcquiringImage = true;
      return;
      
    // PROCESS THE TRACKING SHOT if one was taken
    } else if (NextActionIs(mActIndex, STARTUP_ALIGN_TRACK)) {
      if (SaveExtraImage(TRACKING_CONSET, "Startup tracking"))
        return;

      // Adjust the mean counts per sec unless beam shutter has problem, it's
      // not a Record, and it's a global setting.  But still derate it a lot because
      // there could be a big change in intensity here
      if (bwIndex || !mBeamShutterJitters || mImBufs->mConSetUsed == RECORD_CONSET)
        mProcessImage->UpdateBWCriterion(mImBufs, mBWMeanPerSec[bwIndex], 
        0.3f * derate);

      // If there is an existing reference, align to it; otherwise make this be ref
      refBuf = mLowDoseMode ? mExtraRefBuf : mAlignBuf;
      if (mHaveStartRef && (!mLowDoseMode || mHaveTrackRef)) {
        if (mShiftManager->AutoAlign(refBuf, 0)) {
          ErrorStop();
          return;
        }
      } else
        mBufferManager->CopyImageBuffer(0, refBuf);

      // Set the flag for which kind we have
      if (mLowDoseMode)
        mHaveTrackRef = true;
      else
        mHaveRecordRef = true;

      // If setting intensities, do preliminary setting
      if (!mLowDoseMode && SetIntensityForMean(angle))
        return;
    
    // REVERSE TILT and adjust intensity if doing 1/cosines; base starting intensity
    // on this adjusted value
    } else if (NextActionIs(mActIndex, REVERSE_TILT)) {
      if (mTSParam.beamControl == BEAM_INVERSE_COSINE) {
        delFac = pow(cos(DTOR * mOriginalAngle) / cos(DTOR * angle),
          1. / mTSParam.cosinePower);
        if (SetIntensity(delFac, angle))
          return;
        mStartingIntensity = mLowDoseMode 
          ? mLDParam[RECORD_CONSET].intensity : mScope->GetIntensity();
        mStartingExposure = mConSets[RECORD_CONSET].exposure;
      }

      mCurrentTilt = angle;
      if (mSkipBeamShiftOnAlign)
        mComplexTasks->SetSkipNextBeamShift(true);
      if (mComplexTasks->ReverseTilt(mDirection)) {
        mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
        mReversingTilt = true;
        mNeedGoodAngle = true;
        return;
      }

    // CHECK AUTOFOCUS IF SELECTED
    } else if (NextActionIs(mActIndex, CHECK_AUTOFOCUS)) {
      mFocusManager->SetRequiredBWMean
          (mBWMeanPerSec[mLowDoseMode ? 1 : 0] * mBadShotCrit);
      mFocusManager->CheckAccuracy(LOG_OPEN_IF_CLOSED);
      mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
      mCheckingAutofocus = true;
      return;

    // EVALUATE AUTOFOCUS ACCURACY
    } else if (NextActionIs(mActIndex, EVALUATE_AUTOFOCUS)) {
      mFocusManager->GetAccuracyFactors(plusFactor, minusFactor);
      if (plusFactor > minusFactor)
        plusFactor = minusFactor;
      if (plusFactor < mTSParam.minFocusAccuracy) {
        message.Format("Stopped because Autofocus is able to measure only %.0f%% of\n"
          "actual focus changes at this tilt angle.\n"
          "This may produce inaccurate or runaway focusing\n"
          "at the start of the tilt series.  To avoid this:\n\n"
          "1) Set focus manually to the desired focus level.\n\n"
          "2) Measure defocus (Focus menu) and set\n"
          "the target defocus to the measured value.\n\n"
          "3) Restart the tilt series after disabling the option to check autofocus.\n\n"
          "4) Consider stopping and checking focus accuracy at a lower tilt angle,\n"
          "then adjusting the focus target to the desired level.",
          plusFactor * 100.);
        TSMessageBox(message);
        ErrorStop();
        return;
      }

    // NOTE THAT BIDIRECTIONAL ANCHOR IMAGE IS IN MAIN SECTION ABOVE
    
    // FINAL ACTION, CHECK FOR STOP AT END OF STARTUP AND CLEAR IN STARTUP FLAG
    } else if (mActIndex == mActOrder[FINISH_STARTUP]) {
      mStartupStop = mSingleStep;
      mInStartup = false;
    }

    // Advance loop index when get to bottom, reset to regular loop at end
    IncActIndex();

    // If we did one command with a pending user stop, do it now
    if (mUserStop || mStartupStop) {
      NormalStop();
      return;
    }
  }
}

// A centralized place for testing whether to perform actions in the loop
BOOL CTSController::NextActionIs(int nextIndex, int action, int testStep)
{
  BOOL retval = NextActionIsReally(nextIndex, action, testStep);
  if (retval) {
    DebugDump(actionText[B3DMAX(0, action - 1)]);
    if (testStep > 0 && mSingleStep < 0) {
      if (!mDebugMode)
        PrintfToLog("Executing step: %s", actionText[B3DMAX(0, action - 1)]);
      mUserStop = true;
    }
  }
  return retval;
}

BOOL CTSController::NextActionIsReally(int nextIndex, int action, int testStep)
{
  if (nextIndex != mActOrder[action])
    return false;

  switch (action) {
  case LOOP_START_CHECKS:
  case CHECK_RESET_SHIFT:
  case COMPUTE_PREDICTIONS:
    return true;

  case IMPOSE_VARIATIONS:
    return mTSParam.doVariations;

  // CHECK FOR NEED TO REFINE THE ZLP IF USER PARAMETERS ARE SET
  case CHECK_REFINE_ZLP:
    return (mCamParams->GIF || mScope->GetHasOmegaFilter()) && mTSParam.refineZLP && 
      mTSParam.zlpInterval > 0 && !mSTEMindex;

  // CHECK FOR NEED TO AUTOCENTER BEAM IF ONE OR ANOTHER CRITERION MET
  case CHECK_AUTOCENTER:
    if (!mAutocenteringBeam)
      return false;
    if (mTSParam.cenBeamPeriodically && 
      0.001 * GetTickCount() - mAutocenStamp > mTSParam.cenBeamInterval * 60.)
      return true;
    return (mTSParam.cenBeamAbove && mTiltIndex > 0 && !mDidCrossingAutocen &&
      fabs((double)mTiltAngles[mTiltIndex - 1]) < mTSParam.cenBeamAngle &&
      fabs(mCurrentTilt) >= fabs((double)mTSParam.cenBeamAngle));

  // LOW MAG PICTURE FOR TRACKING IF NEEDED
  case LOW_MAG_FOR_TRACKING:
  case ALIGN_LOW_MAG_SHOT:
    return ((mNeedLowMagTrack && mUsableLowMagRef) || mRoughLowMagRef);

  // REGULAR TRACKING SHOT IF NEEDED AND REFERENCE EXISTS
  case TRACKING_SHOT:
  case ALIGN_TRACKING_SHOT:
    return (mNeedRegularTrack && (!mLowDoseMode || mHaveTrackRef) && 
      mTSParam.trackAfterFocus != TRACK_AFTER_FOCUS);

  case SAVE_STARTUP_AUTOFOCUS:
  case SAVE_AUTOFOCUS:
  case AUTOFOCUS:
    return mNeedFocus;

  // SECOND TRACKING SHOT IF NEEDED AND REFERENCE EXISTS
  case POST_FOCUS_TRACKING:
  case ALIGN_POST_FOCUS:
    return (mNeedRegularTrack && (!mLowDoseMode || mHaveTrackRef) && 
      (mTSParam.trackAfterFocus == TRACK_AFTER_FOCUS || 
      (mTSParam.trackAfterFocus == TRACK_BEFORE_AND_AFTER && mNeedFocus)));

  case GET_DEFERRED_SUM:
    return mNeedDeferredSum;

  case RECORD_OR_MONTAGE:
  case SAVE_RECORD_SHOT:
    return true;

  case EXTRA_RECORD:
    return (mNumExtraRecords && mExtraRecIndex < mNumExtraRecords &&
      !(mTSParam.extraRecordType == TS_OTHER_CHANNELS && 
      mExtraRecIndex < mNumSimultaneousChans - 1));

  case SAVE_EXTRA_RECORD:
    return mTookExtraRec || (mTSParam.extraRecordType == TS_OTHER_CHANNELS && 
      mExtraRecIndex < mNumSimultaneousChans - 1);

  // GET AND COPY LOW MAG REFERENCE IF ONE IS NEEDED, DOING TRACK FIRST
  case TRACK_BEFORE_LOWMAG_REF:
  case ALIGN_LOWMAG_PRETRACK:
  case LOW_MAG_REFERENCE:
  case COPY_LOW_MAG_REF:
    return ((mNeedLowMagTrack && !mUsableLowMagRef) || mRoughLowMagRef);

  // GET AND ALIGN A PREVIEW BEFORE GETTING A LOW DOSE TRACK REFERENCE
  case LOWDOSE_PREVIEW:
  case ALIGN_LOWDOSE_PREVIEW:
    return (mLowDoseMode && !mHaveTrackRef && mTSParam.previewBeforeNewRef);

  // GET AND COPY LOW DOSE TRACK REFERENCE IF ONE IS NEEDED
  case LOWDOSE_TRACK_REF:
  case COPY_LOWDOSE_REF:
    return (mLowDoseMode && !mHaveTrackRef);
  
  // BIDIRECTIONAL RETURN PHASE ACTIONS
  case BIDIR_RELOAD_REFS:
    return ((mBidirSeriesPhase == BIDIR_FIRST_PART || 
      mBidirSeriesPhase ==  BIDIR_RETURN_PHASE) && !mNeedTilt);

  case BIDIR_INVERT_FILE:
  case BIDIR_RETURN_TO_START: 
  case BIDIR_REALIGN_SHOT:
  case BIDIR_CHECK_SHIFT:
  case BIDIR_TRACK_SHOT:
  case BIDIR_ALIGN_TRACK:
    return mBidirSeriesPhase == BIDIR_RETURN_PHASE;

  case BIDIR_REVERSE_TILT:
    return mBidirSeriesPhase == BIDIR_RETURN_PHASE && !mWalkBackForBidir;

  case BIDIR_AUTOFOCUS:
    return mBidirSeriesPhase == BIDIR_RETURN_PHASE && !mTSParam.skipAutofocus;

  case BIDIR_LOWMAG_REF:
  case BIDIR_COPY_LOWMAG_REF:
    return mBidirSeriesPhase == BIDIR_SECOND_PART && mTSParam.trackLowMag && 
      mTiltIndex == mPartStartTiltIndex;

  case BIDIR_FINISH_INVERT:
    return mBidirSeriesPhase == BIDIR_SECOND_PART && mTiltIndex == mPartStartTiltIndex;

  // TILT AT END OF LOOP
  case TILT_IF_NEEDED:
    return true;

  // INITIALIZATION ACTIONS BELOW HERE
    
  // Do initial alignment to image in A
  case ZEROTILT_TRACK_SHOT:
  case ALIGN_ZEROTILT_TRACK:
    return mHaveZeroTiltTrack;

  case ZEROTILT_PREVIEW:
  case ALIGN_ZEROTILT_PREVIEW:
    return mHaveZeroTiltPreview;

  // REFINE EUCENTRICITY if starting near zero
  case EUCENTRICITY:
    return (mCanFindEucentricity && mTSParam.refineEucen && !HitachiScope);
  
  case WALK_UP:
  case STARTUP_AUTOFOCUS:
    return true;
  
  // TAKE SHOT RIGHT ATER WALKUP IF NO START REF AND LAST IMAGE NOT USABLE
  case POST_WALKUP_SHOT:
    return (mDidWalkup && !mHaveStartRef && !mWalkupImageUsable);
    
  // PREVIEW IN LOW DOSE BEFORE TAKING A TRACK REFERENCE
  case STARTUP_PREVIEW:
  case ALIGN_STARTUP_PREVIEW:
    return (mLowDoseMode && mHaveRecordRef && !mHaveTrackRef && 
      mTSParam.previewBeforeNewRef);
      
  // TRACKING SHOT if no reference already or needed for intensity setting
  case STARTUP_TRACK_SHOT:
  case STARTUP_ALIGN_TRACK:
    return (!mHaveStartRef ||
      (!mLowDoseMode && mTSParam.beamControl == BEAM_INTENSITY_FOR_MEAN) ||
      (mLowDoseMode && mHaveRecordRef && !mHaveTrackRef));;
    // 2/20/04: removed && mTSParam.previewBeforeNewRef

  case REVERSE_TILT:
    return true;

  // CHECK AND EVALUATE AUTOFOCUS IF SELECTED
  case CHECK_AUTOFOCUS:
  case EVALUATE_AUTOFOCUS:
    return mTSParam.checkAutofocus && !mSTEMindex;

  // BIDIRECTIONAL ANCHOR SHOT
  case BIDIR_ANCHOR_SHOT:
    return mBidirSeriesPhase != BIDIR_NONE;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// HELPER ROUTINES FOR PERFORMING ACTIONS

// Go to low mag, adjust intensity and exposure time, and acquire track shot
void CTSController::AcquireLowMagImage(BOOL restoreMag)
{
  //double newIntensity, delBeam, newDelta;
  int camera = mWinApp->GetCurrentCamera();
  //int error;

  if (!mInLowMag)
    mComplexTasks->LowerMagIfNeeded(mTSParam.lowMagIndex[mSTEMindex], 0.8f, 0.5f, 
    TRACKING_CONSET);

  // If indicated, restore the mag after the shot
  if (restoreMag && mActPostExposure)
    mCamera->QueueMagChange(mTSParam.magIndex[mSTEMindex]);
  
  mCamera->InitiateCapture(TRACKING_CONSET);
  AddToDoseSum(TRACKING_CONSET);
  mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
  mAcquiringImage = true;
  mInLowMag = true;
}

// Restore mag, intensity and exposure time if in low mag mode
void CTSController::RestoreMag()
{
  if (!mInLowMag)
    return;
  mComplexTasks->RestoreMagIfNeeded();
  mInLowMag = false;
}

// Check if doing beam intensity by mean counts, set it, act on errors
BOOL CTSController::SetIntensityForMean(double angle)
{
  float termAngle;
  double delFac, target, frac;
  BOOL retval;

  if (mTSParam.beamControl != BEAM_INTENSITY_FOR_MEAN)
    return false;

  mLastMean = mProcessImage->EquivalentRecordMean(0);
  target = mTSParam.meanCounts;
  
  // If the tapering is active and we are above the taper angle, figure out ramp
  if (mTSParam.taperCounts && fabs(angle) > mTSParam.taperAngle) {
    frac = 0.;
    
    // At the starting side, use real start angle if available
    if (-mDirection * angle > mTSParam.taperAngle) {
      termAngle = mStartingOut ? mStartingTilt : mTrueStartTilt;
      if (-mDirection * termAngle > mTSParam.taperAngle)
        frac = (-mDirection * angle - mTSParam.taperAngle) / 
          (-mDirection * termAngle - mTSParam.taperAngle);
  
    // At the ending side
    } else if (mDirection * mTSParam.endingTilt > mTSParam.taperAngle)
      frac = (mDirection * angle - mTSParam.taperAngle) / 
          (mDirection * mTSParam.endingTilt - mTSParam.taperAngle);
    if (frac > 1.)
      frac = 1.;
    if (frac < 0.)
      frac = 0.;
    target = (1.f - frac) * mTSParam.meanCounts + frac * mTSParam.highCounts;
  }

  delFac = target / mLastMean;

  //CString report;
  //report.Format("lastMean %.0f  target %.0f  delFac %f", mLastMean, target, delFac);
  //mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  retval = SetIntensity(delFac, angle);
  if (!retval) {
    mAccumDeltaForMean = (float)(mAccumDeltaForMean * delFac);
    mMinAccumDeltaForMean = B3DMIN(mMinAccumDeltaForMean, mAccumDeltaForMean);
  }
  return retval;
}

// Common path for setting intensity
BOOL CTSController::SetIntensity(double delFac, double angle)
{
  double newIntensity, newFactor, limit, expFac = 0.;
  int error;
  CString mess;
  bool limitIntensity = !mTSParam.doBidirectional && (mTSParam.limitToCurrent || 
      (mTSParam.limitIntensity && mFirstExposure > 0.));

  SEMTrace('1', "SetIntensity called with factor %.4f", delFac);
  // If exposure is to be changed instead of intensity, do that
  if (mUseExpForIntensity) {
    limit = 1.e10;
    if (limitIntensity)
      limit = mTSParam.limitToCurrent ? mStartingExposure : mFirstExposure;
    ChangeExposure(delFac, angle, limit);
    return false;
  }

  // If exposure is changing, change it and modify delFac
  if (mTSParam.doVariations && mTypeVaries[TS_VARY_EXPOSURE]) {
    ChangeExposure(expFac, angle);
    delFac /= expFac;
    if (mSTEMindex)
      return false;

    // In STEM, one or the other or neither of those exposure happened, but it's done
  } else if (mSTEMindex) {
    return false;

  // If intensity is to be limited, first assess and just set to limit if necessary
  } else if (limitIntensity) {
    limit = mTSParam.limitToCurrent ? mStartingIntensity : mFirstIntensity;
    error = mBeamAssessor->AssessBeamChange(delFac, newIntensity, newFactor, 
      mLowDoseMode ? 3 : -1);
    if ((!error || error == BEAM_ENDING_OUT_OF_RANGE) &&
      mIntensityPolarity * (newIntensity - limit) > 0.) {
      mScope->SetIntensity(limit);
      if (mLowDoseMode)
        mLDParam[3].intensity = limit;
      return false;
    }
  }

  // Now do change for real and respond to errors
    SEMTrace('1', "Changing beam intensity by %.4f", delFac);
  error = mBeamAssessor->ChangeBeamStrength(delFac, mLowDoseMode ? 3 : -1);
  if (error && error != BEAM_ENDING_OUT_OF_RANGE) {
    mess.Format("Error %d setting beam intensity", error);
    TSMessageBox(mess);
    ErrorStop();
    return true;
  }
  if (error)
    mWinApp->AppendToLog("Warning: attempting to set beam intensity"
    " outside calibrated range", LOG_OPEN_IF_CLOSED);
  return false;       
}

// Change Record exposure by the given factor; set or change drift settling for Record
// as appropriate and change other exposures and settlings if appropriate too
void CTSController::ChangeExposure(double &delFac, double angle, double limit)
{
  float setValues[MAX_VARY_TYPES];
  float deadTime = mCamParams->K2Type ? 0.f : mCamParams->deadTime;
  float frameTime, constraint, minExposure = mCamParams->minExposure;
  float newExp;
  float diffThresh = 0.05f;
  bool oneFrame;
  int numFrames, newNum;
  bool restoreFirst = delFac == 0. && mBidirSeriesPhase == BIDIR_RETURN_PHASE;
  ControlSet *csets = mWinApp->GetCamConSets() + 
    MAX_CONSETS * mActiveCameraList[mTSParam.cameraIndex];
  ControlSet *recSet = &csets[RECORD_CONSET];
 
  if (mTSParam.doVariations) {
    CTSVariationsDlg::FindValuesToSet(mTSParam.varyArray, mTSParam.numVaryItems, 
      mZeroDegValues, (float)angle, mTypeVaries, setValues);
    if (delFac <= 0. && !restoreFirst) {
      delFac = 1.;
      if (fabs(recSet->exposure - setValues[TS_VARY_EXPOSURE]) < 1.e-6)
        return;
      delFac = B3DMAX(setValues[TS_VARY_EXPOSURE] - deadTime, 0.001) / 
        B3DMAX(recSet->exposure - deadTime, 0.001);
      if (mTypeVaries[TS_VARY_EXPOSURE] > 0)
        PrintfToLog("Discrete exposure time change to %.3f at %.1f deg",
          setValues[TS_VARY_EXPOSURE], angle);
    }
  }

  if (restoreFirst)
    delFac = (mFirstExposure - deadTime) / (recSet->exposure - deadTime);
  if (delFac <= 0.)
    return;

  // Change the Record exposure by the factor or limit the change and modify the factor
  newExp = (float)(delFac * (recSet->exposure - deadTime) + deadTime);
  if (newExp > limit) {
    delFac = B3DMAX(limit - deadTime, 0.001) / B3DMAX(recSet->exposure - deadTime, 0.001);
    newExp = (float)limit;
  }

  numFrames = B3DNINT(recSet->exposure / B3DMAX(0.001, recSet->frameTime));
  oneFrame = mCamParams->K2Type && recSet->doseFrac && !mVaryFrameTime &&
    fabs(recSet->frameTime - recSet->exposure) < 1.e-5;
  if (oneFrame)
    recSet->frameTime = newExp;
  SEMTrace('1', "Changing (ideal) Record exposure by %.4f to %.4f", delFac, newExp);
  recSet->exposure = newExp;

  // For a fixed # of frames, get the ideal frame time for new exposure, then constrain 
  // it to actual frame time, then get the actual new exposure
  if (!oneFrame && mCamParams->K2Type && recSet->doseFrac && !mVaryFrameTime && 
    mFixedNumFrames) {
      recSet->frameTime = (float)(newExp / numFrames);
      mCamera->ConstrainFrameTime(recSet->frameTime);
      newExp = (float)(recSet->frameTime * numFrames);
      SEMTrace('1', "Changing frame time to %.4f, actual exposure to %.4f", 
        recSet->frameTime, newExp);

      // But if somehow this doesn't give right # of frames, change exact exposure by
      // least amount so it will get constrained correctly
      newNum = B3DNINT(recSet->exposure / recSet->frameTime);
      if (newNum != numFrames) {
        if (newNum < numFrames)
          recSet->exposure = (float)(numFrames - 0.49) * recSet->frameTime;
        else 
          recSet->exposure = (float)(numFrames + 0.49) * recSet->frameTime;
        PrintfToLog("WARNING: Had to change ideal exposure to %.4f to get %d frames", 
          recSet->exposure, numFrames);
      }
  }

  // Figure out how much the exposure time will differ from the ideal time and report it
  frameTime = mVaryFrameTime ? setValues[TS_VARY_FRAME_TIME] : recSet->frameTime;
  if (mCamParams->K2Type)
    SEMTrace('1', "Frame time for constraint is %.4f", frameTime);
  mCamera->ConstrainExposureTime(mCamParams, recSet->doseFrac, recSet->K2ReadMode, 
    recSet->binning, recSet->alignFrames && !recSet->useFrameAlign, newExp, frameTime);
  newExp = mWinApp->mFalconHelper->AdjustSumsForExposure(mCamParams, recSet, newExp);
  if (oneFrame)
    constraint = mCamera->GetK2ReadoutInterval();
  else 
    constraint = mCamera->GetLastK2BaseTime();
  if (mCamParams->K2Type)
    SEMTrace('1', "Will be constrained to %.4f  (frame %.4f, constraint %.4f)", newExp, 
      frameTime, constraint);
  else if (fabs(newExp - recSet->exposure) > 1.e-5)
    SEMTrace('1', "Will be constrained to %.4f", newExp);

  // Keep storing the desired exact exposure; it will be constrained to newExp in acquire
  if (fabs((double)(newExp - recSet->exposure)) / recSet->exposure > diffThresh) {
    CString mess;
    mess.Format("WARNING: Desired exposure time (%.3f) differs from actual one (%.3f) by"
      " more than %d%%", recSet->exposure, newExp, B3DNINT(100. * diffThresh));
    mWinApp->AppendToLog(mess);
  }

  // Change the drift settling
  if (mTSParam.changeSettling && !mSTEMindex && 
    !(mTSParam.doVariations && mTypeVaries[TS_VARY_DRIFT]))
    recSet->drift *= (float)delFac;

  // If changing other exposures, now scale those too, and drift if requested
  if (mChangeAllExp) {
    for (int i = 0; i < 5; i++) {
      if (i == RECORD_CONSET)
        continue;
      oneFrame = mCamParams->K2Type && csets[i].doseFrac && 
        fabs(csets[i].frameTime - csets[i].exposure) < 1.e-5;
      csets[i].exposure = (float)delFac * (csets[i].exposure - deadTime) + deadTime;
      if (oneFrame)
        csets[i].frameTime = csets[i].exposure;
      if (mChangeAllDrift)
        csets[i].drift *= (float)delFac;
    }
  }
  mWinApp->CopyConSets(mActiveCameraList[mTSParam.cameraIndex]);
}

// Handle all the variations that are not done during intensity changes
int CTSController::ImposeVariations(double angle)
{
  CString message;
  float setValues[MAX_VARY_TYPES];
  float oldTarget;
  double tryLoss, netLoss, delFac = 0.;
  bool changed = false;
  LowDoseParams *ldp = &mLDParam[RECORD_CONSET];
  ControlSet *csets = mWinApp->GetCamConSets() + 
    MAX_CONSETS * mActiveCameraList[mTSParam.cameraIndex];

  CTSVariationsDlg::FindValuesToSet(mTSParam.varyArray, mTSParam.numVaryItems, 
    mZeroDegValues, (float)angle, mTypeVaries, setValues);

  // Change exposure here if it is not being changed elsewhere
  if ((mTSParam.beamControl == BEAM_CONSTANT || mBidirSeriesPhase == BIDIR_RETURN_PHASE)
    && mTypeVaries[TS_VARY_EXPOSURE])
    ChangeExposure(delFac, angle);

  // Change drift here; it wasn't changed elsewhere if being changed directly
  if (mTypeVaries[TS_VARY_DRIFT]) {
    if (mTypeVaries[TS_VARY_DRIFT] > 0 && 
      mConSets[RECORD_CONSET].drift != setValues[TS_VARY_DRIFT])
      PrintfToLog("Discrete drift settling change to %.3f at %.1f deg", 
        setValues[TS_VARY_DRIFT], angle);
    mConSets[RECORD_CONSET].drift = setValues[TS_VARY_DRIFT];
    csets[RECORD_CONSET].drift = setValues[TS_VARY_DRIFT];
  }

  // Change frame time
  if (mVaryFrameTime) {
    if (mTypeVaries[TS_VARY_FRAME_TIME] > 0 && 
      mConSets[RECORD_CONSET].frameTime != setValues[TS_VARY_FRAME_TIME])
      PrintfToLog("Discrete frame time change to %.3f at %.1f deg", 
        setValues[TS_VARY_FRAME_TIME], angle);
    mConSets[RECORD_CONSET].frameTime = setValues[TS_VARY_FRAME_TIME];
    csets[RECORD_CONSET].frameTime = setValues[TS_VARY_FRAME_TIME];
  }
  
  // Change defocus target and defocus if change is big enough
  if (mTypeVaries[TS_VARY_FOCUS]) {
    oldTarget = mFocusManager->GetTargetDefocus();
    if (fabs((double)oldTarget - setValues[TS_VARY_FOCUS]) > 5.e-4) {
      if (mTypeVaries[TS_VARY_FOCUS] > 0)
        PrintfToLog("Discrete defocus target change to %.2f at %.1f deg", 
          setValues[TS_VARY_FOCUS], angle);
      mFocusManager->SetTargetDefocus(setValues[TS_VARY_FOCUS]);
      if (!mScope->IncDefocus((double)setValues[TS_VARY_FOCUS] - oldTarget))
        return 1;

      // If this made focus go outside limits, try to force an autofocus
      if (CheckAndLimitAbsFocus()) {
        mCheckFocusPrediction = false;
        mDisturbance[mTiltIndex - 1] |= FOCUS_DISTURBANCES;
      }
    }
  }

  // Handle energy loss after checking legality
  if (mVaryFilter) {
    tryLoss = setValues[TS_VARY_ENERGY];
    if (mTypeVaries[TS_VARY_ENERGY] && fabs(tryLoss - mFilterParams->energyLoss) > 0.01){
      if (mWinApp->mFilterControl.LossOutOfRange(tryLoss, netLoss)) {
        message.Format("You specified an energy loss of %.1f for this tilt angle."
          "\nThis requires a net %s of %.1f with current adjustments\nand is beyond "
          "the allowed range for the filter.",  mScope->GetHasOmegaFilter() ? 
          "shift" : "offset", tryLoss, netLoss);
        TSMessageBox(message);
        ErrorStop();
        return 1;
      }

      if (mTypeVaries[TS_VARY_ENERGY] > 0)
        PrintfToLog("Discrete energy loss change to %.0f at %.1f deg",
          setValues[TS_VARY_ENERGY], angle);
      changed = true;
      mFilterParams->energyLoss = (float)tryLoss;
      mFilterParams->zeroLoss = false;
    }

    // Slit width was checked during variation input
    if (mTypeVaries[TS_VARY_SLIT] &&
      fabs((double)setValues[TS_VARY_SLIT] - mFilterParams->slitWidth) > 0.1) {
      changed = true;
      mFilterParams->slitWidth = setValues[TS_VARY_SLIT];
      if (mTypeVaries[TS_VARY_SLIT] > 0)
        PrintfToLog("Discrete slit width change to %.0f at %.1f deg", 
          setValues[TS_VARY_SLIT], angle);
    }
    if (changed) {
      mFilterParams->slitIn = true;
      if (mLowDoseMode)
        mWinApp->mLowDoseDlg.SyncFilterSettings(RECORD_CONSET);
      mCamera->SetIgnoreFilterDiffs(true);
      mWinApp->mFilterControl.UpdateSettings();
    }
  }
  return 0;
}


// Do the first or second step of backing up tilt, and set flags
void CTSController::StartBackingUp(int inStep)
{
  // First evaluate need for an emergency low mag reference
  if (mTrackLowMag && !mRoughLowMagRef) {
    
    // Get rough ref if the current ref is not good enough at the target tilt
    mRoughLowMagRef = !UsableLowMagRef(mCurrentTilt);
    if (mRoughLowMagRef) {
      AcquireLowMagImage(true);
      mBackingUpTilt = inStep;
      return;
    } else
      mNeedLowMagTrack = true;
  }

  // Set the flags to enforce tracking
  mNeedRegularTrack = !mTrackLowMag || mRoughLowMagRef;
  mNeedFocus = !mSkipFocus;
  mCheckFocusPrediction = false;
  mNeedGoodAngle = true;

  // Need to wait for stage to be free from pretilt
  if (mScope->WaitForStageReady(10000)) {
    TSMessageBox("Timeout waiting for stage to be ready when backing up");
    mPreTilted = false;
    ErrorStop();
    return;
  }
  mBackingUpTilt = inStep;
  mScope->TiltTo(mCurrentTilt - (2 - inStep) * mDirection * 
    mComplexTasks->GetTiltBacklash());
  mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 25000);
  mPreTilted = false;
  if (mTiltIndex > 0) 
    mDisturbance[mTiltIndex - 1] |= TILT_BACKED_UP;
}

// Set the current Z if montaging, or set up to overwrite if necessary for regular file
void CTSController::SetFileZ()
{
  int newZ = mTiltIndex + mBaseZ;
  if (mMontaging)
    mMontParam->zCurrent = newZ;
  else {
    if (newZ != mWinApp->mStoreMRC->getDepth())
      mOverwriteSec = newZ;
    else
      mOverwriteSec = -1;
  }
}

// Get the next tilt angle and assess whether we should tilt there
void CTSController::AssessNextTilt(BOOL preTest)
{
  double overshoot;

  // in computing next tilt, adjust tiltIndex up by 1 if before the
  // place in loop where it is incremented
  int addIndex = mActIndex <= mActOrder[SAVE_RECORD_SHOT] ? 1 : 0;
  double currentTilt = mCurrentTilt;

  // If this is before the test for whether we achieved nextTilt, and if this
  // is past the first tilt and at start of loop, use the last tilt angle as
  // the current tilt and drop the index by one
  if (preTest && mTiltIndex && mActIndex == mActOrder[LOOP_START_CHECKS]) {
    if (mTiltIndex != mPartStartTiltIndex)
      currentTilt = mTiltAngles[mTiltIndex - 1];
    else
      currentTilt = mTiltAngles[0];
    addIndex--;
  }

  // For cosine tilt, get the next tilt implied by the current angle;
  // for regular tilt set based on increment from regular starting tilt
  if (mTSParam.cosineTilt) {
    mLastIncrement = (float)(mTSParam.tiltIncrement * cos(DTOR * mCurrentTilt));
    mNextTilt = currentTilt + mDirection * mLastIncrement;
  } else
    mNextTilt = mTrueStartTilt + mDirection * (mTiltIndex + addIndex - 
      (mPartStartTiltIndex ? mPartStartTiltIndex - 1 : 0)) * mTSParam.tiltIncrement;
  
  // Need a next tilt if it does not go past the pole angle and either it is not past
  // the ending angle or it is time to tilt (could require a single step/loop request)
  // Let it overshoot by up to half the tilt increment but not more than a fixed amount
  overshoot = mDirection * (mNextTilt - mEndingTilt);
  mNeedTilt = ((overshoot < 0.5 * mLastIncrement && overshoot < 0.25)
    || (mActIndex == mActOrder[TILT_IF_NEEDED])) &&
    mDirection * (mNextTilt - mPoleAngle) < 0.05;
  
  // Set stop at "loop end" if not tilting and set up for closing valves
  if (!mNeedTilt && (mBidirSeriesPhase == BIDIR_NONE || 
    mBidirSeriesPhase == BIDIR_SECOND_PART)) {
    mStopAtLoopEnd = true;
    mTermAtLoopEnd = mExternalControl != 0;
    mCloseValvesOnStop = mOneShotCloseValves > 0;
    if (mOneShotCloseValves > 0)
      mOneShotCloseValves = -1;
    mReachedEndAngle = true;
  }
}

void CTSController::IncActIndex()
{
  mActIndex++;
  if (mActIndex >= mActLoopEnd) {
    mActIndex = 0;
    mActLoopEnd = mNumActLoop;
  }
}

// Make a buffer be a new low dose reference
void CTSController::NewLowDoseRef(int inBuf)
{
  // Copy the stack down, then copy current reference to stack, then copy new ref
  for (int i = mTrackStackBuf + mNumTrackStack - 1; i > mTrackStackBuf; i--)
    mBufferManager->CopyImageBuffer(i - 1, i);
  mBufferManager->CopyImageBuffer(mExtraRefBuf, mTrackStackBuf);
  if (inBuf != mExtraRefBuf)
    mBufferManager->CopyImageBuffer(inBuf, mExtraRefBuf);
}

// Set and return the value of mUsableLowMagRef based on existence and angle of ref
BOOL CTSController::UsableLowMagRef(double angle)
{
  mUsableLowMagRef = mHaveLowMagRef && !mRoughLowMagRef && 
    (fabs(angle - mLowMagRefTilt) < 1.5 * mLastIncrement ||
    fabs(angle - mLowMagRefTilt) < mMaxUsableAngleDiff * cos(DTOR * angle));
  return mUsableLowMagRef;
}

// Do autoalignment and test for whether it exceeds criterion; return true if so
BOOL CTSController::AutoAlignAndTest(int bufNum, int smallPad, char *shotName,
                                     bool scaling)
{
  int nx, ny;
  float shiftX, shiftY;
  double pctShift;
  CString message;

  if (mSkipBeamShiftOnAlign)
    mScope->SetSkipNextBeamShift(true);

  if (scaling) 
    nx = mMultiTSTasks->AlignWithScaling(bufNum, true, shiftX);
  else
    nx = mShiftManager->AutoAlign(bufNum, smallPad);
  mScope->SetSkipNextBeamShift(false);
  if (nx) {
    ErrorStop();
    return true;
  }
  if (!mTSParam.stopOnBigAlignShift)
    return false;

  mImBufs->mImage->getSize(nx, ny);
  mImBufs->mImage->getShifts(shiftX, shiftY);
  shiftX /= nx;
  shiftY /= ny;
  pctShift = 100. * sqrt((double)(shiftX * shiftX + shiftY * shiftY));
  if (pctShift < mTSParam.maxAlignShift)
    return false;

  // Stop first to record stop time, in case it is a long time before user sees this
  if (!mTerminateOnError)
    ErrorStop();
  message.Format("The alignment shift of the %s image is %.1f%% of the image size,\n"
    "which exceeds your criterion of %.1f%%.\n\n"
    "If this alignment is bad, undo the alignment shift with the Clear Alignment button\n"
    "on the Image Alignment and Focus control panel, then check the specimen position.\n\n"
    "If the alignment is correct, use Resume to continue the tilt series.", shotName,
    pctShift, mTSParam.maxAlignShift);
  TSMessageBox(message);
  return true;
}

// Center beam if flag set, require fitted radius to be 90% of half-diagonal
void CTSController::CenterBeamWithTrial()
{
  int nx, ny;
  double minRadius;
  if (!mTSParam.centerBeamFromTrial)
    return;
  mImBufs->mImage->getSize(nx, ny);
  minRadius = mTrialCenterMaxRadFrac * sqrt((double)(nx *nx + ny * ny));
  if (mWinApp->GetImBufIndex())
    mWinApp->SetCurrentBuffer(0);

  // Also require error to be a small fraction of the radius
  mProcessImage->CenterBeamFromActiveImage(minRadius, 0.03f);
}

// This makes sure that the true rounded start tilt angle gets set once and that the 
// starting flag gets cleared
void CTSController::SetTrueStartTiltIfStarting(void)
{
  double angle, roundFac;
  if (mStartingOut) {
    angle = mScope->GetTiltAngle(true);
    roundFac = mTSParam.tiltIncrement > 0.5 ? 0.1 : 0.05;
    mTrueStartTilt = (float)(roundFac * floor(0.5 + (fabs(angle - mStartingTilt) < 
      0.1 ? (double)mStartingTilt : angle) / roundFac));
    mStartingOut = false;
  }
}

// If absolute focus limits are being applied, test if the current focus is outside the
// limit and if so, bring it just inside
int CTSController::CheckAndLimitAbsFocus(void)
{
  double focus, minFocus, maxFocus;
  if (!mSTEMindex && mTSParam.applyAbsFocusLimits) {
    minFocus = mFocusManager->GetEucenMinAbsFocus();
    maxFocus = mFocusManager->GetEucenMaxAbsFocus();
    focus = mScope->GetFocus();
    if (focus < minFocus) {
      mScope->SetFocus(minFocus + 1.e-5 * (maxFocus - minFocus));
      return 1;
    }
    if (focus > maxFocus) {
      mScope->SetFocus(maxFocus - 1.e-5 * (maxFocus - minFocus));
      return 1;
    }
  }
  return 0;
}

// Common routine to start getting a deferred sum.  Return 0 if nothing started, 1 for
// normal start, and 2 for an error
int CTSController::StartGettingDeferredSum(int fromWhere)
{
  if (!mNeedDeferredSum)
    return 0;

  // Clear the need flag unconditionally, messages have been issued on error
  mNeedDeferredSum = false;
  if (mCamera->GetDeferredSum()) {
    ErrorStop();
    return 2;
  }
  mGettingDeferredSum = true;
  mWhereDefSumStarted = fromWhere;
  mWinApp->AddIdleTask(TASK_TILT_SERIES, 0, 0);
  return 1;
}


/////////////////////////////////////////////////////////////////////////////
// TASK, ERROR, AND STOPPING ROUTINES

// Error: register an error stop and call for next action to process it
void CTSController::TiltSeriesError(int error)
{
  CString str;
  ExternalStop(1);
  if (error == IDLE_TIMEOUT_ERROR) {
    str.Format("A timeout occurred in the last %s, action index %d", 
      mAcquiringImage ? "image acquisition" : "action", mActIndex);
    TSMessageBox(str);
    BOOL debugSave = mDebugMode;
    mDebugMode = true;
    DebugDump(str + "\r\n");
    mDebugMode = debugSave;
  }
  if (mTerminateOnError)
    TerminateOnError();
  else
    NextAction(error ? error : 1);
}

// Check all of the things that may have been started
int CTSController::TiltSeriesBusy()
{
  if (mInvertingFile)
    return (mMultiTSTasks->DoingBidirCopy() ? 1 : 0);
  if (mAcquiringImage || mGettingDeferredSum)
    return mCamera->TaskCameraBusy();
  if (mWalkingUp || mReversingTilt || mFindingEucentricity || mResettingShift || 
    (mTilting && mDidTiltAfterMove))
    return (mComplexTasks->DoingTasks() ? 1 : 0);
  if (mDoingMontage)
    return (mMontageController->DoingMontage() ? 1 : 0);
  if (mFocusing)
    return (mFocusManager->DoingFocus() ? 1 : 0);
  if (mTilting || mBackingUpTilt)
    return (mScope->StageBusy());
  if (mCheckingAutofocus)
    return (mFocusManager->CheckingAccuracy() ? 1 : 0);
  if (mRefiningZLP)
    return (mWinApp->mFilterTasks->RefiningZLP() ? 1 : 0);
  if (mCenteringBeam)
    return (mMultiTSTasks->GetAutoCentering() ? 1 : 0);
  if (mTakingBidirAnchor)
    return (mMultiTSTasks->DoingAnchorImage() ? 1 : 0);
  if (mRunningMacro)
    return (mWinApp->mMacroProcessor->DoingMacro() ? 1 : 0);
  if (mDoingPlugCall)
    return (mWinApp->DoingRegisteredPlugCall() ? 1 : 0);
  return 0;
}

// Perform a normal stop due to user request
void CTSController::NormalStop()
{
  // If need a deferred sum, get that and leave flags as they are, otherwise convert
  // to an error stop and fall through
  int defErr = StartGettingDeferredSum(DEFSUM_NORMAL_STOP);
  if (defErr == 1)
    return;
  if (defErr) {
    mUserStop = false;
    mErrorStop = true;
  }

  // If a pretilt has occurred, find out if we should go back
  if (mPreTilted && AfxMessageBox("The stage has already tilted after the last Record "
    "image.\nIt would have to be tilted back to be at the tilt angle of that image.\n"
    "Do not do this unless you need to, because it may require\n"
    "additional focusing and tracking shots for several tilts.\n\n"
    "Do you want to tilt back?", MB_YESNO, MB_ICONQUESTION) == IDYES) {
    StartBackingUp(1);
    return;
  }

  if (mCloseValvesOnStop)
    if (!mScope->SetColumnValvesOpen(false))
      AfxMessageBox("An error occurred closing the gun valve at the end of the series.",
         MB_EXCLAME);
  if (mErrorStop)
    ErrorStop();
  else
    CommonStop(false);
}

// Perform an error stop if there was an internal or external error
void CTSController::ErrorStop()
{
  // If this flag is set
  if (mAlreadyTerminated)
    return;
  // If it happened in startup, end the controller just like a user stop
  if (mActIndex >= mNumActLoop)
    EndControl(false);
  else {

    // If need a deferred sum, get that
    if (StartGettingDeferredSum(DEFSUM_ERROR_STOP) == 1)
      return;

    // If pretilted, need to back up; set flags to come back through here
    if (mPreTilted) {
      StartBackingUp(1);
      mUserStop = false;
      mErrorStop = true;
      return;
    }

    CommonStop(false);
  }
}

// Common things to do for either kind of stop
void CTSController::CommonStop(BOOL terminating)
{
  RestoreMag();
  RestoreFromExtraRec();
  mHighestIndex = mTiltIndex;
  mStopAtLoopEnd = false;
  mRestoredTrackBuf = 0;
  mLDTrialWasMoved = false;
  mScope->GetStagePosition(mStopStageX, mStopStageY, mStopStageZ);
  mStopDefocus = mScope->GetUnoffsetDefocus();
  mStopTargetDefocus = mFocusManager->GetTargetDefocus();
  mStopLDTrialDelX = mLDParam[2].ISX - mLDParam[0].ISX;
  mStopLDTrialDelY = mLDParam[2].ISY - mLDParam[0].ISY;

  mConSets[RECORD_CONSET].averageDark = mSaveAverageDarkRef;

  // No longer need to fix copy on save
  mDoingTS = false;
  mPostponed = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "STOPPED TILT SERIES");
  SendEmailIfNeeded(terminating);
}

// send an email after a stop, or after a postpone if called by Navigator
void CTSController::SendEmailIfNeeded(BOOL terminating)
{
  CString mess;
  CString title = "SerialEM tilt series ";
  if (mSendEmail && !mUserStop && !mFinishEmailSent && !mErrorEmailSent && 
    (!mStopAtLoopEnd || mReachedEndAngle)) {
    mess.Format("The tilt series being saved to file %s\r\n", mWinApp->m_strTitle);
    if (mReachedEndAngle) {
      mFinishEmailSent = true;
      title += "completed successfully";
      mess += "successfully reached the ending tilt angle";
      if (!mFinalReport.IsEmpty())
        mess += "\r\n" + mFinalReport;
    } else if (mTermFromMenu) {
      title += "terminated by user";
      mess += "was terminated from the menu or with the End button";
      if (!mFinalReport.IsEmpty())
        mess += "\r\n" + mFinalReport;
   } else {
      title += CString((terminating ? "terminated" : "stopped")) + " due to error";
      if (mInStartup)
        mess += CString((terminating ? "terminated" : "stopped in the postpone state")) +
          " because of an error during initial actions";
      else 
        mess += CString(terminating ? "terminated" : "stopped") + " because of an error";
    }
    mWinApp->mMailer->SendMail(title, mess);
    mErrorEmailSent = true;
  }
}

// An external stop is called from the ErrorOccurred function - record which type
void CTSController::ExternalStop(int error)
{
  // A user stop can apparently generate some error stop calls but not vice versa, so
  // set user stop absolutely and clear error stop, set error stop only if user not set
  if (!error) {
    mUserStop = true;
    mErrorStop = false;
  } else if (!mUserStop)
    mErrorStop = true;
  /*CString message;
  message.Format("ext stop %d  user %d error %d", error, mUserStop?1:0, mErrorStop?1:0);
  mWinApp->AppendToLog(message,LOG_OPEN_IF_CLOSED); */
}

// End the controller's modifications
void CTSController::EndControl(BOOL terminating)
{
  float slitWidth = 0.;
  float energyLoss = 0.;
  MontParam mp = *mMontParam;
  int iCam = mActiveCameraList[mTSParam.cameraIndex];
  ControlSet *csets = mWinApp->GetCamConSets() + MAX_CONSETS * iCam;
  CString report, str, xyzName;
  float pixelSize;
  int sizeX, sizeY, mbSaved, filmMag, i;
  CTime ctdt = CTime::GetCurrentTime();

  if (!mBidirAnchorName.IsEmpty()) {
    if (mTSParam.retainBidirAnchor)
      mWinApp->AppendToLog("Left anchor images in " + mBidirAnchorName);
    else
      UtilRemoveFile(mBidirAnchorName);
  }
  report = mWinApp->mStoreMRC->getFilePath();
  UtilSplitExtension(report, xyzName, str);
  xyzName += "_xyz.txt";
  if (mStartedTS && mBidirSeriesPhase > BIDIR_FIRST_PART && 
    mMultiTSTasks->BidirCopyPending())
    mMultiTSTasks->CleanupBidirFileCopy();

  // Prepare report before CommonStop so it can go in email
  if (!mMontaging) {
    mp.xNframes = 1;
    mp.yNframes = 1;
    mp.xOverlap = 0;
    mp.yOverlap = 0;
    mp.xFrame = mWinApp->mStoreMRC->getWidth();
    mp.yFrame = mWinApp->mStoreMRC->getHeight();
  }
  sizeX = mp.xFrame + (mp.xNframes - 1) * (mp.xFrame - mp.xOverlap);  
  sizeY = mp.yFrame + (mp.yNframes - 1) * (mp.yFrame - mp.yOverlap);  
  mbSaved = (int)((mBaseZ + mTiltIndex) * 1.e-6 * mp.xFrame * mp.yFrame *
    mp.xNframes * mp.yNframes * 2);
  pixelSize = 1000.f * mTSParam.binning * 
    mShiftManager->GetPixelSize(iCam, mTSParam.magIndex[mSTEMindex]);
  filmMag = MagForCamera(mCamParams, mTSParam.magIndex[mSTEMindex]);
  if ((mCamParams->GIF || mScope->GetHasOmegaFilter()) && 
    (mLowDoseMode ? mLDParam[3].slitIn : mFilterParams->slitIn)) {
    slitWidth = mLowDoseMode ? mLDParam[3].slitWidth : mFilterParams->slitWidth;
    if (!(mLowDoseMode ? mLDParam[3].zeroLoss : mFilterParams->zeroLoss))
      energyLoss =  mLowDoseMode ? mLDParam[3].energyLoss : mFilterParams->energyLoss;
  }

  report.Format("%4d/%02d/%02d %02d:%02d\t%s\t%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f\t%.2f\t%s"
    "\t%.1f\t%dx%d\t%s\t%d\t%d\t%.2f\t%s\t%.1f\t%.1f\n",
    ctdt.GetYear(), ctdt.GetMonth(), ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(),
    (LPCTSTR)mWinApp->mStoreMRC->getFilePath(), mBaseZ, mBaseZ + mTiltIndex, sizeX,
    sizeY, mbSaved, mTiltAngles[0], mTiltAngles[mTiltIndex - 1], mTSParam.tiltIncrement, 
    mTSParam.cosineTilt ? "COS" : "REG", (mTimes[mTiltIndex - 1] - mTimes[0]) / 60., 
    mp.xNframes, mp.yNframes, (LPCTSTR)mCamParams->name, filmMag, 
    mTSParam.binning, pixelSize, mLowDoseMode ? "LD" : "ND", slitWidth, energyLoss);
  mFinalReport = report;
  mFinalReport.Replace("\t", "  ");
  mFinalReport.Replace("\n", "");

  CommonStop(terminating);
  mPostponed = mInStartup && (!mTerminateOnError || mUserStop) && !terminating;
  mWinApp->SetStatusText(COMPLEX_PANE, mExternalControl && mPostponed ? 
    "POSTPONED T-SERIES" : "");
  mStartedTS = false;
  ManageTerminateMenu();
  mTermFromMenu = false;
  mTerminateOnError = 0;
  mBufferManager->SetShiftsOnAcquire(mSaveShiftsOnAcquire);
  mBufferManager->SetBufToReadInto(mSaveBufToReadInto);
  mBufferManager->SetAlignOnSave(mSaveAlignOnSave);
  mBufferManager->SetCopyOnSave(mSaveCopyOnSave);
  mBufferManager->SetAlignToB(mSaveAlignToB);
  mBufferManager->SetConfirmBeforeDestroy(3, mSaveProtectRecord);
  mWinApp->mBufferWindow.ProtectBuffer(mAlignBuf, false);   
  mWinApp->mBufferWindow.ProtectBuffer(mExtraRefBuf, false);    
  mWinApp->mBufferWindow.ProtectBuffer(mReadBuf, false);    
  mWinApp->mBufferWindow.ProtectBuffer(mAnchorBuf, false);
  if (mLowDoseMode)
    for (int i = mTrackStackBuf; i < mNumTrackStack + mTrackStackBuf; i++)
      mWinApp->mBufferWindow.ProtectBuffer(i, false);

  if (mTSParam.doVariations && mTypeVaries[TS_VARY_FOCUS]) {
    //mWinApp->AppendToLog("Resetting target");
    mFocusManager->SetTargetDefocus(mSaveTargetDefocus);
  }

  if (mVaryFilter) {
    if (mTSParam.doVariations && mTypeVaries[TS_VARY_ENERGY]) {
      mFilterParams->energyLoss = mSaveEnergyLoss;
      mFilterParams->zeroLoss = mSaveZeroLoss;
    }
    if (mTSParam.doVariations && mTypeVaries[TS_VARY_SLIT])
      mFilterParams->slitWidth = mSaveSlitWidth;
    mFilterParams->slitIn = mSaveSlitIn;
    if (mLowDoseMode)
      mWinApp->mLowDoseDlg.SyncFilterSettings(RECORD_CONSET);
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
  }

  // Restore exposure times
  if (mChangeRecExp) {
    mCamera->SetInterpDarkRefs(mSaveInterpDarkRef);
    csets[RECORD_CONSET].exposure = mSaveExposures[RECORD_CONSET];
    csets[RECORD_CONSET].frameTime = mSaveFrameTimes[RECORD_CONSET];
    csets[RECORD_CONSET].summedFrameList = mSaveFrameList;
    if (mChangeAllExp) {
      for (i = 0; i < 5; i++) {
        csets[i].exposure = mSaveExposures[i];
        csets[i].frameTime = mSaveFrameTimes[i];
      }
    }
  }

  // Restore drift
  if (mChangeRecDrift)
    csets[RECORD_CONSET].drift = mSaveDrifts[RECORD_CONSET];
  if (mChangeAllDrift)
    for (i = 0; i < 5; i++)
      csets[i].drift = mSaveDrifts[i];

  // Restore frame time
  if (mVaryFrameTime)
    csets[RECORD_CONSET].frameTime = mSaveFrameTimes[RECORD_CONSET];

  // Restore continuous mode
  if (mAnyContinuous) {
    for (i = (mLowDoseMode ? 0 : 1); i < (mLowDoseMode ? 5 : 4); i++)
      csets[i].mode = mSaveContinuous[i];
  }

  // If doing deferred frame alignment restore align flag first
  if (mFrameAlignInIMOD)
    csets[RECORD_CONSET].alignFrames = 1;

  // And if any of those got restored, copy the consets
  if (mChangeRecExp || mChangeRecDrift || mChangeAllDrift || mVaryFrameTime || 
    mAnyContinuous || mFrameAlignInIMOD)
    mWinApp->CopyConSets(iCam);
  if (mFrameAlignInIMOD && mTiltIndex)
    mCamera->MakeMdocFrameAlignCom();
  mFrameAlignInIMOD = false;
  mWinApp->mCameraMacroTools.DoingTiltSeries(mExternalControl && mPostponed);
  mExternalControl = 0;
  mWinApp->UpdateWindowSettings();
  mTSParam.lastStartingTilt = mTSParam.startingTilt;
  mDocWnd->SetCurrentStore(mCurrentStore);
  mDocWnd->EndStoreProtection();

  // Dump logbook entry if anything is saved
  if (!mTiltIndex)
    return;
  CString title = "Date\tFile\tZstart\tNZ\tNX\tNY\tMBytes\tStart\tEnd\tInc\tCosine\tMinutes"
    "\tFrames\tCamera\tMag\tBin\tPixel\tLowdose\tSlit\tLoss\n";
  mDocWnd->AppendToLogBook(report, title);
  mWinApp->AppendToLog(mFinalReport, LOG_SWALLOW_IF_CLOSED);
  
  // Report total dose in various activities
  AddToDoseSum(0);
  if (mDoseSums[RECORD_CONSET] > 0.) {
    report.Format("Total dose in electrons/square Angstrom:\r\n"
      "Recording:  %.1f\r\nTracking:    %.1f\r\nFocusing:    %.1f\r\nTasks:       %.2f",
      mDoseSums[RECORD_CONSET], mDoseSums[TRACKING_CONSET], mDoseSums[FOCUS_CONSET], 
      mDoseSums[0]);
    if (mLowDoseMode) {
      str.Format("\r\nPreviews:   %.1f", mDoseSums[PREVIEW_CONSET]);
      report += str;
    }
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
  }

  // Autosave xyz file
  if (mAutosaveXYZ)
    WriteTiltXYZFile(&xyzName);
}

// Termination from the menu
void CTSController::Terminate()
{
  CString str;
  int i, minIndex, err, answer;
  double minAngle = 200;
  mErrorEmailSent = false;

  // If running from external control, still close the file etc;
  if (mWinApp->NavigatorStartedTS()) {
    if (!mPostponed || mStartedTS)
      mLastSucceeded = 1;
    mTermFromMenu = true;
    TerminateOnError();
    return;
  }

  if (mStartedTS && mBidirSeriesPhase == BIDIR_FIRST_PART) {
    answer = SEMThreeChoiceBox(_T("Do you want to go on to the second part of the \ntilt "
      "series instead of terminating it?\n\n"
      "Press:\n\"Second Part\" to go on to the second part,\n\n"
      "\"Terminate\" to terminate the whole series,\n\n"
      "\"Cancel\" to avoid either action."), "Second Part", "Terminate", "Cancel", 
      MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL)
      return;
    if (answer == IDYES) {
      mActIndex = mActOrder[BIDIR_RELOAD_REFS];
      mNeedTilt = false;
      CommonResume(0, 0, true);
      return;
    }
  }

  if (mStartedTS && mBidirSeriesPhase > BIDIR_FIRST_PART && 
    mMultiTSTasks->BidirCopyPending()) {
      if (AfxMessageBox("The inversion of the file from the first\n"
        "part of the series is not complete.\n\n"
        "Are you sure you want to terminate the tilt series?", MB_QUESTION) != IDYES)
          return;
  }

  answer = SEMThreeChoiceBox(_T("Do you want the image file to be closed when\n"
    "the tilt series controller is terminated?\n\n"
    "Press:\n\"Close File\" to terminate and close the file,\n\n"
    "\"Leave Open\" to terminate and leave the file open,\n\n"
    "\"Cancel\" to avoid terminating the controller."), "Close File", "Leave Open",
    "Cancel", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (answer == IDCANCEL)
    return;

  // Option to read in zero-tilt view and rotate it 90 degrees
  if (mTiltIndex > 0 && ((mTiltAngles[0] >= 0. && mTiltAngles[mTiltIndex - 1] <= 0. ||
    mTiltAngles[0] <= 0. && mTiltAngles[mTiltIndex - 1] >= 0.) ||
    (mTSParam.doBidirectional && mBidirSeriesPhase != BIDIR_FIRST_PART)) && 
    !mLowDoseMode) {
      str = "Do you want to leave 90-degree rotated copies\nof the zero tilt view in "
        "buffers " + CString((char)('A' + mReadBuf + 1)) + " and " + 
        CString((char)('A' + mReadBuf + 2)) + "?";
    if (AfxMessageBox(str, MB_YESNO, MB_ICONQUESTION) == IDYES) {
      
      // Find minimum tilt
      for (i = 0; i < mTiltIndex; i++) {
        if (minAngle > fabs((double)mTiltAngles[i])) {
          minAngle = fabs((double)mTiltAngles[i]);
          minIndex = i;
        }
      }
      if (mTSParam.doBidirectional && mBidirSeriesPhase != BIDIR_FIRST_PART && 
        minIndex < mPartStartTiltIndex)
        minIndex = mPartStartTiltIndex - 1 - minIndex;

      // Read into read buffer
      if (mMontaging)
        err = mMontageController->ReadMontage(minIndex, NULL, NULL, false, true);
      else
        err = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, minIndex);
      if (err) {
        AfxMessageBox("Error reading image back from file", MB_EXCLAME);
      } else {

          // Rotate it both ways and put in buffers.  Montage uses overview in B
          short int *brray;
          i = mMontaging ? 1 : mReadBuf;
          KImage *image = mImBufs[i].mImage;
          int nx = image->getWidth();
          int ny = image->getHeight();
          int type = image->getType();
          image->Lock();
          for (i = 0; i < 2; i++) {
            NewArray(brray, short int, nx * ny);
            if (!brray) {
              AfxMessageBox("Failed to get memory for rotated image", MB_EXCLAME);
              break;
            }
            
            if (i)
              ProcRotateLeft(image->getData(), type, nx, ny, brray);
            else
              ProcRotateRight(image->getData(), type, nx, ny, brray);
            
            // Replace the image, cleaning up on failure
            if (mBufferManager->ReplaceImage((char *)brray, type, ny, nx, 
              mReadBuf + 1 + i, BUFFER_PROCESSED, RECORD_CONSET) ) {
              delete [] brray;
              break;
            }
            mImBufs[mReadBuf + 1 + i].mBinning = mTSParam.binning;

          }
          image->UnLock();
      }
    }
  }
  mTermFromMenu = true;
  EndControl(true);
  if (answer == IDYES)
    mDocWnd->DoCloseFile();
  mWinApp->DetachStackView();
}

// Termination on exit
BOOL CTSController::TerminateOnExit()
{
  bool inverting = mBidirSeriesPhase > BIDIR_FIRST_PART && 
    mMultiTSTasks->BidirCopyPending();
  CString str;
  if (!mStartedTS)
    return false;
  if (mNeedDeferredSum || inverting) {
    if (mNeedDeferredSum)
      str = "The full summed image from the last Record still\n"
        "needs to be retrieved and saved.\n\n";
    if (inverting)
      str += "The inversion of the file from the first\n"
      "part of the series is not complete.\n\n";
  }
  if (AfxMessageBox(str + "Are you sure you want to terminate the\n"
    "tilt series and exit the program?", MB_YESNO, MB_ICONQUESTION) != IDYES)
      return true;
  
  EndControl(true);
  return false;
}

// Terminate on error or normal completion when being controlled from elsewhere
// So far this can also be used for real termination from elsewhere
void CTSController::TerminateOnError(void)
{
  if (StartGettingDeferredSum(DEFSUM_TERM_ERROR) == 1)
    return;
  mTerminateOnError = 0;
  mPostponed = false;
  mInStartup = false;
  mWinApp->mCameraMacroTools.DoingTiltSeries(false);
  if (mStartedTS)
    EndControl(true);
  if (mLowDoseMode)
    mLDParam[RECORD_CONSET].intensity = mOriginalIntensity;
  else
    mScope->SetIntensity(mOriginalIntensity);
  mScope->TiltTo(0.);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);

  // Navigator tests this flag to know when tilt is done
  mTiltedToZero = true;
  mDocWnd->DoCloseFile();
  if (mWinApp->mStackView)
    mWinApp->mStackView->CloseFrame();
}

// Set complex pane if  doing initial checks under external
// control, and clear the flags that should be cleared when leaving from initial checks
void CTSController::LeaveInitialChecks(void)
{
  if (mInInitialChecks && mExternalControl)
    mWinApp->SetStatusText(COMPLEX_PANE, "POSTPONED T-SERIES");
  mUserPresent = false;
  mInInitialChecks = false;
}

int CTSController::TSMessageBox(CString message, UINT type, BOOL terminate, int retval)
{
  int index = 0;
  CString valve;

  // Intercept error from macros if the flag is set, make sure there is a \r before at
  // least one \n in a row, print message and return
  if (mWinApp->mCamera->GetNoMessageBoxOnError() < 0 || (mWinApp->mMacroProcessor->DoingMacro() && 
    mWinApp->mMacroProcessor->GetNoMessageBoxOnError())) {
      for (int ind = 0; ind < message.GetLength(); ind++) {
        if (message.GetAt(ind) == '\n' && (!ind || (message.GetAt(ind - 1) != '\r' &&
          message.GetAt(ind - 1) != '\n'))) {
          message.Insert(ind, '\r');
          ind++;
        }
      }
      message = CString("\r\nSCRIPT STOPPING WITH THIS MESSAGE:\r\n") + message + 
      CString("\r\n* * * * * * * * * * * * * * * * * *\r\n");    
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
    return retval;
 }

  // Always give message box if not terminating, if it was a user stop, if currently
  // postponed, or if the user is present before going into the real actions.
  // Send an email if the user seems not to be around, we are not postponed, and
  // it is clearly in the tilt series somewhere
  // If we are not in tilt series but in navigator acquire, let it send email
  if (((!mTerminateOnError || mPostponed) && !mWinApp->mCamera->GetNoMessageBoxOnError())
    || mUserStop || mUserPresent) {
    if (mInInitialChecks || mStartedTS || mDoingTS) {
      if (!(mUserStop || mPostponed || mUserPresent))
        SendEmailIfNeeded(false);
    } else {
      if (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring())
        mWinApp->mNavigator->SendEmailIfNeeded();
      if (mWinApp->mMacroProcessor->DoingMacro())
        mWinApp->mMacroProcessor->SendEmailIfNeeded();
    }
    LeaveInitialChecks();
    if (mMessageBoxCloseValves && mMessageBoxValveTime > 0.)
      mScope->CloseValvesAfterInterval(60000. * mMessageBoxValveTime);
    SEMTrace('m', "TSMessageBox: %s", (LPCTSTR)message);

    // Call three-choice box if the type and button texts are adequate
    if (mCallFromThreeChoice && !mTCBoxYesText.IsEmpty() && !mTCBoxNoText.IsEmpty() && 
      ((type & MB_YESNO) != 0 || 
      ((type & MB_YESNOCANCEL) != 0 && !mTCBoxCancelText.IsEmpty()))) {
        mCallFromThreeChoice = false;
        CThreeChoiceBox tcb;
        tcb.mDlgType = type;
        tcb.mYesText = mTCBoxYesText;
        tcb.mNoText = mTCBoxNoText;
        tcb.mCancelText = mTCBoxCancelText;
        tcb.mSetDefault = mTCBoxDefault;
        tcb.m_strMessage = message;
        tcb.DoModal();
        retval = tcb.mChoice;
    } else {
      mCallFromThreeChoice = false;
      retval = ::MessageBox(mWinApp->m_pMainWnd->m_hWnd, (LPCTSTR)message, NULL, type);
    }
    if (mMessageBoxCloseValves && mMessageBoxValveTime > 0. && 
      mScope->CloseValvesAfterInterval(-1.) == 0.) {
      valve = JEOLscope ? "The gun valve was closed":"The column valves were closed";
      if (mScope->GetNoColumnValve())
        valve = "The filament was turned off";
      ::MessageBox(mWinApp->m_pMainWnd->m_hWnd, (LPCTSTR)(valve + " while waiting for " +
        "you to close the last message box!"), "SerialEM Warning", MB_EXCLAME);
    }
    return retval;
  }

  // Clear these flags to save the many return points from doing so
  mUserPresent = false;
  mInInitialChecks = false;
  while ((index = message.Find('\n', index)) >= 0) {
    if (!index || message.GetAt(index - 1) != '\r')
      message.Insert(index++, '\r');
    index++;
  }
  if (terminate || mWinApp->mCamera->GetNoMessageBoxOnError())
    message = CString("\r\n* * * * * * * * * * * * * * * * * *\r\n"
      "TILT SERIES TERMINATING WITH THIS"
      " MESSAGE:\r\n") + message + CString("\r\n* * * * * * * * * * * * * * * * * *");
  else
    message = CString("\r\nTILT SERIES CONTINUING WITH THIS ERROR MESSAGE:\r\n") + 
      message;
  mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  mWinApp->AppendToLog("", LOG_OPEN_IF_CLOSED);
  if (terminate) {
    TerminateOnError();
    mAlreadyTerminated = true;
  }
  return retval;
}

// Tests whether a dim image should end the first part of a bidirectional series
int CTSController::EndFirstPartOnDimImage(double angle, CString message, BOOL &needRedo, 
  bool highExp)
{
  // The test that rules out ending on either kind of call
  if (mBidirSeriesPhase == BIDIR_RETURN_PHASE || 
    (fabs(angle) < mDimEndsAbsAngle && fabs(mEndingTilt - angle) > mDimEndsAngleDiff))
    return 0;

  // The test for not ending on a dim image/bad autofocus
  if (!highExp && (!mEndOnHighDimImage || !(mBidirSeriesPhase == BIDIR_FIRST_PART || 
    (mExternalControl && (mTerminateOnError || mTermNotAskOnDim)))))
    return 0;

  // The test for not ending with a high exposure - it already tested if we are supposed 
  // to end on high exposures and if we already stopped
  if (highExp && !(mBidirSeriesPhase == BIDIR_FIRST_PART || mExternalControl))
    return 0;

  mWinApp->AppendToLog(message);
  if (mBidirSeriesPhase == BIDIR_FIRST_PART) {
    mWinApp->AppendToLog("\r\nTHE FIRST PART OF THE SERIES IS BEING ENDED;\r\nGOING ON TO"
      " SECOND PART BASED ON TILT SERIES POLICY SETTINGS\r\n");

    // Set the index for return; stop thinking a redo is needed, but hijack the partial
    // return flag to avoid other checks and incrementing index
    // Also cancel a walkup since tracking is compromised
    mActIndex = mActOrder[BIDIR_RELOAD_REFS];
    needRedo = false;
    mKeepActIndexSame = true;
    mNeedTilt = false;
    if (mWalkBackForBidir)
      mWinApp->AppendToLog("RETURNING TO STARTING ANGLE WITH TILT STEPS INSTEAD "
      "OF WALKUP\r\n");
    mWalkBackForBidir = false;
    return 1;
  }
  
  mWinApp->AppendToLog("\r\nTHE SERIES IS CONSIDERED SUCCESSFULLY COMPLETED\r\n"
    " BASED ON TILT SERIES POLICY SETTINGS\r\n");
  mReachedEndAngle = true;
  mTermAtLoopEnd = true;
  mLastSucceeded = 2;
  TerminateOnError();
  return -1;
}


/////////////////////////////////////////////////////////////////////////////
// DIALOGS FOR RESUMING AND BACKING UP
//
// Bring up the dialog to resume the series, then act on user's wishes
void CTSController::Resume()
{
  CTSResumeDlg resumeDlg;
  resumeDlg.m_strStatus = "The next regular action will be to ";
  int nextIndex = mActIndex;
  float bufAngle;
  bool skipOK = mActIndex < mActOrder[BIDIR_INVERT_FILE] || 
    mActIndex >= mActOrder[TILT_IF_NEEDED];

  // Invalidate the reference if trial area moved
  CheckLDTrialMoved();

  for (;;) {

    if (NextActionIs(nextIndex, CHECK_RESET_SHIFT) || 
      NextActionIs(nextIndex, LOOP_START_CHECKS)) {
      resumeDlg.m_strStatus += "check image shift, then track.";
      break;
    } else if (NextActionIs(nextIndex, IMPOSE_VARIATIONS)) {
      resumeDlg.m_strStatus += "change parameters based on angle, then track.";
      break;
    } else if (NextActionIs(nextIndex, CHECK_REFINE_ZLP)) {
      resumeDlg.m_strStatus += "check for refining ZLP, then track.";
      break;
    } else if (NextActionIs(nextIndex, CHECK_AUTOCENTER)) {
      resumeDlg.m_strStatus += "check for autocentering beam , then track.";
      break;
    } else if (NextActionIs(nextIndex, COMPUTE_PREDICTIONS)) {
      resumeDlg.m_strStatus += "compute predictions, then track.";
      break;
    } else if (NextActionIs(nextIndex, LOW_MAG_FOR_TRACKING)) {
      resumeDlg.m_strStatus += "get low mag tracking shot.";
      break;
    } else if (NextActionIs(nextIndex, ALIGN_LOW_MAG_SHOT)) {
      resumeDlg.m_strStatus += "align low mag tracking shot.";
      break;
    } else if (NextActionIs(nextIndex, TRACKING_SHOT)) {
      resumeDlg.m_strStatus += "get regular tracking shot.";
      break;
    } else if (NextActionIs(nextIndex, ALIGN_TRACKING_SHOT)) {
      resumeDlg.m_strStatus += "align regular tracking shot.";
      break;
    } else if (NextActionIs(nextIndex, AUTOFOCUS)) {
      resumeDlg.m_strStatus += "autofocus.";
      break;
    } else if (NextActionIs(nextIndex, POST_FOCUS_TRACKING)) {
      resumeDlg.m_strStatus += "get post-focus tracking shot.";
      break;
    } else if (NextActionIs(nextIndex, ALIGN_POST_FOCUS)) {
      resumeDlg.m_strStatus += "align post-focus tracking shot.";
      break;
   } else if (NextActionIs(nextIndex, RECORD_OR_MONTAGE)) {
      if (mMontaging)
        resumeDlg.m_strStatus += "take montage.";
      else
        resumeDlg.m_strStatus += "take " + mModeNames[RECORD_CONSET] + " image.";
      break;
    } else if (NextActionIs(nextIndex, SAVE_RECORD_SHOT)) {
      if (mMontaging)
        resumeDlg.m_strStatus += "make montage overview be the new reference.";
      else
        resumeDlg.m_strStatus += "save " + mModeNames[RECORD_CONSET] + 
        " image and make it be the new reference.";
      break;
    } else if (NextActionIs(nextIndex, EXTRA_RECORD) || 
      NextActionIs(nextIndex, SAVE_EXTRA_RECORD)) {
        resumeDlg.m_strStatus.Format("The next regular action will be to %s extra %s "
          "image #%d", NextActionIs(nextIndex, EXTRA_RECORD) ? "acquire" : "save", 
          (LPCTSTR)mModeNames[RECORD_CONSET], mExtraRecIndex + 1);
        break;
    } else if (NextActionIs(nextIndex, TRACK_BEFORE_LOWMAG_REF)) {
      resumeDlg.m_strStatus += "take a Trial before getting a new low mag reference.";
      break;
    } else if (NextActionIs(nextIndex, ALIGN_LOWMAG_PRETRACK)) {
      resumeDlg.m_strStatus += "align the Trial to Record reference.";
      break;
    } else if (NextActionIs(nextIndex, LOW_MAG_REFERENCE)) {
      resumeDlg.m_strStatus += "get reference for low mag tracking.";
      break;
    } else if (NextActionIs(nextIndex, COPY_LOW_MAG_REF)) {
      resumeDlg.m_strStatus += "copy low mag reference to its buffer.";
      break;
    } else if (NextActionIs(nextIndex, LOWDOSE_PREVIEW)) {
      resumeDlg.m_strStatus += "take a Preview before getting a new tracking reference.";
      break;
    } else if (NextActionIs(nextIndex, ALIGN_LOWDOSE_PREVIEW)) {
      resumeDlg.m_strStatus += "align the Preview to Record reference.";
      break;
    } else if (NextActionIs(nextIndex, LOWDOSE_TRACK_REF)) {
      resumeDlg.m_strStatus += "get new tracking area reference.";
      break;
    } else if (NextActionIs(nextIndex, COPY_LOWDOSE_REF)) {
      resumeDlg.m_strStatus += "copy tracking reference to its buffer.";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_INVERT_FILE)) {
      resumeDlg.m_strStatus += "start copying file and/or inverting Z.";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_RETURN_TO_START)) {
      resumeDlg.m_strStatus += "return to the starting tilt angle";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_REALIGN_SHOT)) {
      resumeDlg.m_strStatus += "realign to bidirectional anchor image";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_CHECK_SHIFT)) {
      resumeDlg.m_strStatus += "check for resetting image shift";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_REVERSE_TILT)) {
      resumeDlg.m_strStatus += "reverse tilt for other direction";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_RELOAD_REFS)) {
      resumeDlg.m_strStatus += "reload reference image(s)";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_TRACK_SHOT)) {
      resumeDlg.m_strStatus += "take a tracking shot";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_AUTOFOCUS)) {
      resumeDlg.m_strStatus += "autofocus before starting second part of series";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_LOWMAG_REF)) {
      resumeDlg.m_strStatus += "take a new low mag reference";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_COPY_LOWMAG_REF)) {
      resumeDlg.m_strStatus += "save the low mag reference";
      break;
    } else if (NextActionIs(nextIndex, BIDIR_FINISH_INVERT)) {
      resumeDlg.m_strStatus += "finish copying file to invert Z";
      break;
    } else if (NextActionIs(nextIndex, GET_DEFERRED_SUM)) {
      resumeDlg.m_strStatus += "get the full summed Record image";
      break;
    } else if (NextActionIs(nextIndex, TILT_IF_NEEDED)) {
      if (mPreTilted)
        resumeDlg.m_strStatus += "check image shifts at the new tilt."
          "  Cancel and select Back Up to repeat actions at the last tilt.";
      else
        resumeDlg.m_strStatus += "go to next tilt.";
      break;
    }
    nextIndex++;
    if (nextIndex >= mActLoopEnd)
      nextIndex = 0;
  }

  // Set other flags and do dialog
  resumeDlg.m_bMontaging = mMontaging;
  resumeDlg.m_bRecordInA = mImBufs->mCaptured == 1 && 
    mImBufs->mConSetUsed == RECORD_CONSET && mImBufs->GetSaveCopyFlag() > 0;
  resumeDlg.m_strRecordName = mModeNames[RECORD_CONSET];
  resumeDlg.m_iResumeOption = 0;
  resumeDlg.m_bUseNewRef = mUseNewRefOnResume;
  resumeDlg.m_bChangeOK = skipOK;

  // Going on to next tilt is an option if we are past the save or if we have
  // been beyond it at some point
  resumeDlg.m_bTiltable = (mActIndex > mActOrder[SAVE_RECORD_SHOT] ||
    mHighestIndex > mTiltIndex) && skipOK &&
    !(nextIndex == mActOrder[TILT_IF_NEEDED] && mPreTilted);

  // Low mag ref is available if ref is usable or rough ref exists
  resumeDlg.m_bLowMagAvailable = mTrackLowMag && 
    (UsableLowMagRef(mScope->GetTiltAngle()) || mRoughLowMagRef);
  if (resumeDlg.DoModal() != IDOK)
    return;

  // Adjust option up by one for montaging - NO MORE
  //if (mMontaging && resumeDlg.m_iResumeOption > 1)
  //  resumeDlg.m_iResumeOption++;

  switch (resumeDlg.m_iResumeOption) {
  case 1:  // Go on to next tilt
    
    // if the action is before the place where tiltindex is incremented,
    // need to increment it and adjust  the overwrites
    if (mActIndex <= mActOrder[SAVE_RECORD_SHOT])
      mTiltIndex++;
    mActIndex = mActOrder[TILT_IF_NEEDED];
    break;

  case 2:  // save user's record image

    // This time, if action is beyond the save, we need to decrement
    // the index and cause an overwrite
    if (mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted)
      mTiltIndex--;
    mActIndex = mActOrder[SAVE_RECORD_SHOT];
    mHaveXYPrediction = false;
    if (mImBufs->GetTiltAngle(bufAngle))
      mShotAngle = bufAngle;

    // Need to autoalign if all regular conditions are met, unless user says
    // this is a new reference
    if (mHaveRecordRef && !resumeDlg.m_bUseNewRef &&
      (!mLowDoseMode || !mTSParam.alignTrackOnly || !mHaveTrackRef)) {
      if (mShiftManager->AutoAlign(mAlignBuf, 1))
        return;
    }
    break;

  case 3:   // Do record/montage

    // Again, if we are beyond save, we need to decrement
    if (mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted)
      mTiltIndex--;
    mActIndex = mActOrder[RECORD_OR_MONTAGE];
    mDidRegularTrack = false;
    break;

  case 4:   // do autofocus

    // Similarly, and need to set flags to get focus and prevent tests
    if (mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted)
      mTiltIndex--;
    mActIndex = mActOrder[AUTOFOCUS];
    mNeedFocus = true;      // Really let the user select this even in skip mode
    mCheckFocusPrediction = false;
    mDidRegularTrack = false;

    // Set flag to make sure a track after focus occurs
    if (mTSParam.trackAfterFocus != TRACK_BEFORE_FOCUS)
      mNeedRegularTrack = true;
    break;

  case 5:   // do regular track
    if (mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted)
      mTiltIndex--;
    mActIndex = mActOrder[TRACKING_SHOT];
    mNeedFocus = !mSkipFocus;   // But don't override skip flag here
    mCheckFocusPrediction = false;
    mNeedRegularTrack = true;

    // If a reference exists but has been invalidated, try to reinstate it
    if (mLowDoseMode && !mHaveTrackRef && !mLDTrialWasMoved &&
      mImBufs[mExtraRefBuf].GetTiltAngle(bufAngle))
      mHaveTrackRef = fabs(mScope->GetTiltAngle() - bufAngle) < 
        mMaxUsableAngleDiff * cos(DTOR * bufAngle);
    break;

  case 6:   // do low mag track
    if (mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted)
      mTiltIndex--;
    mActIndex = mActOrder[LOW_MAG_FOR_TRACKING];
    mNeedLowMagTrack = true;
    mNeedFocus = !mSkipFocus;
    mCheckFocusPrediction = false;
    mNeedRegularTrack = mRoughLowMagRef;
    break;
  }

  // If any option except just go on, set up for overwriting if necessary and recompute
  // next tilt angle
  if (resumeDlg.m_iResumeOption) {
    SetFileZ();

    // Fix the current tilt
    if (mPreTilted && mActIndex != mActOrder[TILT_IF_NEEDED]) {
      mCurrentTilt = mNextTilt;
      mPreTilted = false;
    }
    // mStopAtLoopEnd = false;
    // AssessNextTilt();
  }

  mUseNewRefOnResume = resumeDlg.m_bUseNewRef;

  CommonResume(resumeDlg.m_iSingleStep, 0);
}

// External call to back up the tilt series
void CTSController::BackUpSeries()
{
  CTSBackupDlg backupDlg;
  float bufAngle;
  double limitAngle, delFac;
  int newIndex, thisZ, lastError, thisError, i;
  int numEntries = MAX_BACKUP_LIST_ENTRIES;
  double minDiff;

  CheckLDTrialMoved();

  // set starting index in list based on maximum allowed for display,
  // or on whatever is in reach of existing tracking images for low dose
  if (mLowDoseMode && !mLDTrialWasMoved) {
    
    // First copy existing reference to stack if it is not already there
    if (!mStackedLDRef)
      NewLowDoseRef(mExtraRefBuf);
    mStackedLDRef = true;

    // then find oldest angle in stack
    for (i = mTrackStackBuf; i < mNumTrackStack + mTrackStackBuf; i++) {
      if (mImBufs[i].GetTiltAngle(bufAngle))
        limitAngle = bufAngle;
      else
        break;
    }
    // Extend the angle to usable limit
    limitAngle -= mDirection * mMaxUsableAngleDiff * cos(DTOR * limitAngle);

    // Now look back for last angle that falls within limit
    for (i = 1; i < mHighestIndex && i <= numEntries; i++)
      if (mDirection * (mTiltAngles[mHighestIndex - i - 1] - limitAngle) <= 0.)
        break;
    numEntries = i;

    // If trial was moved, issue a warning and allow full range of angles
  } else if (mLowDoseMode) {
    AfxMessageBox("You will be able to back up, but there are no tracking \nreferences"
      " for any previous tilt angles\nbecause you have changed the location of the "
       "tracking area", MB_EXCLAME);
  }
  
  int startIndex = mHighestIndex - numEntries;
  if (startIndex < mPartStartTiltIndex)
    startIndex = mPartStartTiltIndex;

  // Set the base Z value based on this, and provide pointers to tilt angles
  // and set the selection to the current index, not the highest
  backupDlg.m_iBaseZ = mBaseZ + startIndex;
  backupDlg.m_fpTilts = &mTiltAngles[startIndex];
  backupDlg.m_iNumEntries = mHighestIndex - startIndex;

  // Have to back up the pointer if and only if the tilt index is at the
  // highest index, because there is no tilt angle at that index
  backupDlg.m_iSelection = mTiltIndex - startIndex - 
    (mTiltIndex == mHighestIndex ? 1 : 0);
  if (backupDlg.DoModal() != IDOK)
    return;

  newIndex = startIndex + backupDlg.m_iSelection;

  // Get the previous Z and put in align buffer
  mBackingUpZ = newIndex + mBaseZ - 1;
  if (mBackingUpZ < 0)
    mBackingUpZ = 0;
  thisZ = newIndex + mBaseZ;

  if (mMontaging) {
    lastError = mMontageController->ReadMontage(mBackingUpZ, NULL, NULL, false, 
      true);
    if (!lastError) {
      mImBufs->mImage->setShifts(mAlignShiftX[mBackingUpZ - mBaseZ], 
        mAlignShiftY[mBackingUpZ - mBaseZ]);
      mBufferManager->CopyImageBuffer(0, mAlignBuf);
    }
    
    // Get the new Z if it is not the same
    if (thisZ != mBackingUpZ) {
      thisError = mMontageController->ReadMontage(thisZ, NULL, NULL, false, true);
      if (!thisError) 
        mImBufs->mImage->setShifts(mAlignShiftX[thisZ - mBaseZ], 
          mAlignShiftY[thisZ - mBaseZ]);
    }
  } else {

    // single frame
    lastError = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, mBackingUpZ, mAlignBuf);
    if (!lastError) {
      mImBufs[mAlignBuf].mImage->setShifts(mAlignShiftX[mBackingUpZ - mBaseZ], 
        mAlignShiftY[mBackingUpZ - mBaseZ]);
    }

    thisError = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, thisZ, 0);
    if (!thisError) {
      mImBufs->mImage->setShifts(mAlignShiftX[thisZ - mBaseZ], 
        mAlignShiftY[thisZ - mBaseZ]);
    }
  }

  if (lastError)
    AfxMessageBox("Error reading previous tilt for reference", MB_EXCLAME);
  if (thisError)
    AfxMessageBox("Error reading image for this tilt", MB_EXCLAME);

  // Change beam intensity or exposure back to where it was at new angle
  // and restore any other variations
  if (mTSParam.beamControl == BEAM_INVERSE_COSINE) {
    delFac = pow(cos(DTOR * mCurrentTilt) / cos(DTOR * mTiltAngles[newIndex]), 
      1. / mTSParam.cosinePower);
    SetIntensity(delFac, mTiltAngles[newIndex]);
  }
  if (mTSParam.doVariations)
    ImposeVariations(mTiltAngles[newIndex]);

  // This is needed for the backing up calls
  mCurrentTilt = mTiltAngles[newIndex];

  // For low dose, find the closest reference in stack
  if (mLowDoseMode && !mLDTrialWasMoved) {
    minDiff = 10000.;
    for (i = mTrackStackBuf; i < mNumTrackStack + mTrackStackBuf; i++) {
      if (mImBufs[i].GetTiltAngle(bufAngle)) {
        if (fabs(mCurrentTilt - bufAngle) < minDiff) {
          minDiff = fabs(mCurrentTilt - bufAngle);
          mRestoredTrackBuf = i;
        }
      } else
        break;
    }
    mBufferManager->CopyImageBuffer(mRestoredTrackBuf, mExtraRefBuf);
    mHaveTrackRef = true;
  }

  // Need to change tilt angle unless there is no change in index, or
  // the index is dropping by one simply due to moving the action index back
  if (!((newIndex == mTiltIndex) || (newIndex == mTiltIndex - 1 &&
    mActIndex > mActOrder[SAVE_RECORD_SHOT] && !mPreTilted))) {

    // If index is less than the last, do full backup with backlash,
    // otherwise just tilt once forward 
    if (newIndex < mTiltIndex)
      StartBackingUp(1);
    else
      StartBackingUp(2);
  }

  // Set new index, set the file writing position, set to start at the
  // beginning of the loop, get the next tilt value, and disable predictions
  mTiltIndex = newIndex;
  SetFileZ();
  mActIndex = mActOrder[LOOP_START_CHECKS];

  mHaveFocusPrediction = false;
  mHaveXYPrediction = false;
}

///////////////////////////////////////////////////////////////////////////////////////
// EXTRA OUTPUT HANDLING

// Open the dialog controlling extra output
void CTSController::SetExtraOutput()
{
  CTSExtraFile extraDlg;
  int i;

  // If tilt series started, reload the files array with actual file numbers
  if (mStartedTS && LookupProtectedFiles())
    return;

  extraDlg.m_bStackRecord = mTSParam.stackRecords;
  extraDlg.mMaxStackSizeXY = mTSParam.stackBinSize;
  for (i = 0; i < MAX_EXTRA_SAVES; i++) {
    extraDlg.mSaveOn[i] = mStoreExtra[i];
    extraDlg.mFileIndex[i] = mExtraFiles[i];
  }
  if (!mTSParam.numExtraExposures) {
    mTSParam.numExtraExposures = 1;
    mTSParam.extraExposures[0] = 0.;
    mTSParam.extraDrift = mConSets[RECORD_CONSET].drift;
  }
  extraDlg.m_fNewDrift = mTSParam.extraDrift;
  extraDlg.m_iWhichRecord = mTSParam.extraRecordType;
  extraDlg.mNumExtraFocus = mTSParam.numExtraFocus;
  extraDlg.mNumExtraExposures = mTSParam.numExtraExposures;
  extraDlg.mNumExtraFilter = mTSParam.numExtraFilter;
  extraDlg.mNumExtraChannels = mTSParam.numExtraChannels;
  for (i = 0; i < mTSParam.numExtraFocus; i++)
    extraDlg.mExtraFocus[i] = mTSParam.extraFocus[i];
  for (i = 0; i < mTSParam.numExtraExposures; i++)
    extraDlg.mExtraExposures[i] = mTSParam.extraExposures[i];
  for (i = 0; i < mTSParam.numExtraFilter; i++) {
    extraDlg.mExtraLosses[i] = mTSParam.extraLosses[i];
    extraDlg.mExtraSlits[i] = mTSParam.extraSlits[i];
  }
  for (i = 0; i < mTSParam.numExtraChannels; i++)
    extraDlg.mExtraChannels[i] = mTSParam.extraChannels[i];
  extraDlg.m_fDeltaC2 = mTSParam.extraDeltaIntensity;
  extraDlg.m_bSetSpot = mTSParam.extraSetSpot;
  extraDlg.m_bSetExposure = mTSParam.extraSetExposure;
  if (!mTSParam.extraSpotSize)
    mTSParam.extraSpotSize = mScope->GetSpotSize();
  extraDlg.mSpotSize = mTSParam.extraSpotSize;
  extraDlg.mBinIndex = mTSParam.extraBinIndex;
  extraDlg.m_bTrialBin = mTSParam.extraSetBinning;
  extraDlg.m_bKeepBidirAnchor = mTSParam.retainBidirAnchor;

  if (extraDlg.DoModal() != IDOK)
    return;
  mTSParam.stackRecords = extraDlg.m_bStackRecord;
  mTSParam.stackBinSize = extraDlg.mMaxStackSizeXY;
  for (i = 0; i < MAX_EXTRA_SAVES; i++) {
    mStoreExtra[i] = extraDlg.mSaveOn[i];
    mExtraFiles[i] = extraDlg.mFileIndex[i];
  }
  mTSParam.extraRecordType = extraDlg.m_iWhichRecord;
  mTSParam.numExtraFocus = extraDlg.mNumExtraFocus;
  mTSParam.numExtraExposures = extraDlg.mNumExtraExposures;
  mTSParam.numExtraFilter = extraDlg.mNumExtraFilter;
  mTSParam.numExtraChannels = extraDlg.mNumExtraChannels;
  for (i = 0; i < mTSParam.numExtraFocus; i++)
    mTSParam.extraFocus[i] = extraDlg.mExtraFocus[i];
  for (i = 0; i < mTSParam.numExtraExposures; i++)
    mTSParam.extraExposures[i] = extraDlg.mExtraExposures[i];
  for (i = 0; i < mTSParam.numExtraFilter; i++) {
    mTSParam.extraLosses[i] = extraDlg.mExtraLosses[i];
    mTSParam.extraSlits[i] = extraDlg.mExtraSlits[i];
  }
  for (i = 0; i < mTSParam.numExtraChannels; i++)
    mTSParam.extraChannels[i] = extraDlg.mExtraChannels[i];

  mTSParam.extraDeltaIntensity = extraDlg.m_fDeltaC2;
  mTSParam.extraSetSpot = extraDlg.m_bSetSpot;
  mTSParam.extraSetExposure = extraDlg.m_bSetExposure;
  mTSParam.extraSpotSize = extraDlg.mSpotSize;
  mTSParam.extraDrift = extraDlg.m_fNewDrift;
  if (mWinApp->LowDoseMode()) {
    mTSParam.extraSetBinning = extraDlg.m_bTrialBin;
    mTSParam.extraBinIndex = extraDlg.mBinIndex;
  }
  mTSParam.retainBidirAnchor = extraDlg.m_bKeepBidirAnchor;
  
  // Renew the protections with the potentially new file numbers
  if (mStartedTS) {
    EvaluateExtraRecord();
    SetProtections();
  }
}

// For each extra store, look up protected file and complain if fails
int CTSController::LookupProtectedFiles()
{
  int actual = 0;
  for (int i = 0; i < MAX_EXTRA_SAVES; i++) {
    if (mStoreExtra[i]) {
      actual = mDocWnd->LookupProtectedStore(mProtectedFiles[i]);
      if (actual < 0) 
        break;
      mExtraFiles[i] = actual;
    }
  }
  if (actual >= 0) {
    actual = mDocWnd->LookupProtectedStore(mProtectedStore);
    if (actual >= 0)
      mCurrentStore = actual;
  }
  if (actual < 0) {
    TSMessageBox("Something has gone wrong keeping track of output files"
      "\n\nYou probably need to terminate the tilt series and start again.");
    return 1;
  }
  return 0;
}

// Set the output file protections for main and extra outputs
void CTSController::SetProtections()
{
  mDocWnd->EndStoreProtection();
  mDocWnd->ProtectStore(mCurrentStore);
  mProtectedStore = mCurrentStore;
  for (int i = 0; i < MAX_EXTRA_SAVES; i++) {
    if (mStoreExtra[i]) {
      mDocWnd->ProtectStore(mExtraFiles[i]);
      mProtectedFiles[i] = mExtraFiles[i];
    }
  }
}

// Check the extra output files for existence and ability to save indicated images
int CTSController::CheckExtraFiles()
{
  CString typestr, message;
  KImageStore *store;
  int fileX[MAX_STORES], fileY[MAX_STORES];
  int i, numStores, sizeX, sizeY, actual, binning;
  int camera = mActiveCameraList[mTSParam.cameraIndex];
  ControlSet *csp;

  for (i = 0; i < MAX_STORES; i++)
    fileX[i] = fileY[i] = 0;

  if (mStartedTS && LookupProtectedFiles())
    return 1;

  numStores = mDocWnd->GetNumStores();
  for (i = 0; i < MAX_EXTRA_SAVES; i++) {
    if (mStoreExtra[i]) {
      if (i < NUMBER_OF_USER_CONSETS)
        typestr.Format("extra %s output", (LPCTSTR)mModeNames[i]);
      else
        typestr.Format("extra output type #%d", i + 1);
      if (mExtraFiles[i] >= numStores) {
        message.Format("File #%d for %s does not exist; only %d files are open", 
          mExtraFiles[i] + 1, (LPCTSTR)typestr, numStores);
        TSMessageBox(message);
        return 1;
      }
      if (mExtraFiles[i] == mCurrentStore) {
        message.Format("File #%d for %s is selected as\nthe current file for output\n\n"
          "Use the \"To #\" spinner in the Buffer Controls panel to make\n"
          "the desired file for regular %ss be the current file", 
          mExtraFiles[i] + 1, (LPCTSTR)typestr, (LPCTSTR)mModeNames[RECORD_CONSET]);
        TSMessageBox(message);
        return 1;
      }
      store = mDocWnd->GetStoreMRC(mExtraFiles[i]);

      // For a montage file, make sure it extra Record and not STEM mode or opposite trial
      if (mDocWnd->StoreIsMontage(mExtraFiles[i])) {
        if (i != RECORD_CONSET) {
          message.Format("File #%d for %s is a montage and cannot be saved to except for "
            "extra Records", mExtraFiles[i] + 1, (LPCTSTR)typestr);
          TSMessageBox(message);
          return 1;
        }
        if (mTSParam.extraRecordType == TS_OPPOSITE_TRIAL || mCamParams->STEMcamera) {
          message.Format("File #%d for %s is a montage and cannot be used for %s",
            mExtraFiles[i] + 1, (LPCTSTR)typestr,
            mCamParams->STEMcamera ? "extra Records in STEM mode" : "opposite Trials");
          TSMessageBox(message);
          return 1;
        }
        MontParam *montp = mDocWnd->GetStoreMontParam(mExtraFiles[i]);
        if (!mStartedTS && montp)
          mExtraFreeZtoWrite = montp->zCurrent;
      }


      if (i >= NUMBER_OF_USER_CONSETS)
        continue;

      actual = i;
      csp = mWinApp->GetCamConSets() + camera * MAX_CONSETS + i;
      binning = csp->binning;
      if (i == RECORD_CONSET && mTSParam.extraRecordType == TS_OPPOSITE_TRIAL) {
        actual = TRIAL_CONSET;
        csp = mWinApp->GetCamConSets() + camera * MAX_CONSETS + TRIAL_CONSET;
        binning = csp->binning;
        if (mTSParam.extraSetBinning)
          csp->binning = mCamParams->binnings
            [B3DMIN(mTSParam.extraBinIndex, mCamParams->numBinnings - 1)];
      }
      
      // Check sizes except for a montage file
      if (!mDocWnd->StoreIsMontage(mExtraFiles[i])) {
        mCamera->AcquiredSize(csp, camera, sizeX, sizeY);
        csp->binning = binning;
        if (!mStartedTS)
          mExtraFreeZtoWrite = store->getDepth();

        // If file already has a size, make sure it matches; otherwise make sure
        // it matches size if file was previously assigned; otherwise save size
        if (store->getWidth()) {
          if (store->getWidth() != sizeX || store->getHeight() != sizeY) {
            message.Format("The current %s parameters will produce a %d x %d image,\n"
              "the wrong size for file #%d (%d x %d)", (LPCTSTR)mModeNames[actual], sizeX, 
              sizeY, mExtraFiles[i] + 1, store->getWidth(), store->getHeight());
            TSMessageBox(message);
            return 1;
          }

        } else if (fileX[mExtraFiles[i]]) {
          if (fileX[mExtraFiles[i]] != sizeX || fileY[mExtraFiles[i]] != sizeY) {
            message.Format("The current %s parameters will produce a %d x %d image,\n"
              "but some other selected output to file #%d  will produce a %d x %d image",
              (LPCTSTR)mModeNames[actual], sizeX, sizeY, mExtraFiles[i] + 1, 
              fileX[mExtraFiles[i]], fileY[mExtraFiles[i]]);
            TSMessageBox(message);
            return 1;
          }
        } else {
          fileX[mExtraFiles[i]] = sizeX;
          fileY[mExtraFiles[i]] = sizeY;
        }
      }
    }
  }
  return 0;
}

// Check whether an image of the given index should be saved and save it
int CTSController::SaveExtraImage(int index, char *message)
{
  int i, err, lastSave = 0, sectNum;
  CString report;
  EMimageBuffer saveBuf;
  if (!mStoreExtra[index])
    return 0;

  // This should be pointless...  but could user close a file while running?
  int fileNum = mDocWnd->LookupProtectedStore(mProtectedFiles[index]);
  int numSave = 1;

  // Report montage saving and return
  if (mSwitchedToMontFrom >= 0) {
    report.Format("%s montage saved to file #%d, Z = %d", message, fileNum + 1, 
      mMontParam->zCurrent - 1);
    if (mVerbose)
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    ManageZafterNewExtraRec();
    return 0;
  }

  // Otherwise set up and save image(s)
  if (index == FOCUS_CONSET)
    numSave = (mFocusManager->GetTripleMode() || mSTEMindex) ? 3 : 2;
  if (index == RECORD_CONSET && mTSParam.extraRecordType == TS_OTHER_CHANNELS &&
    mExtraRecIndex < mNumSimultaneousChans - 1) {
      lastSave = mExtraRecIndex + 1;
  }

  for (i = numSave + lastSave - 1; i >= lastSave; i--) {

    // Set up to overwrite a record section, otherwise it appends
    sectNum = -1;
    if (index == RECORD_CONSET && mExtraOverwriteSec >= 0)
      sectNum = mExtraOverwriteSec;
    if ((err = mDocWnd->SaveBufferToFile(i, fileNum, sectNum))) {
      report.Format("Error saving %s image to file #%d", message, fileNum + 1);
      TSMessageBox(report);
      ErrorStop();
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      return err;
    }
    report.Format("%s image saved to file #%d, Z = %d", message, fileNum + 1, 
      mImBufs[i].mSecNumber);
    if (mVerbose)
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    if (index == RECORD_CONSET)
      ManageZafterNewExtraRec();
  }
  return 0;
}

// Manage the overwriting and list of section Z values after writing one section
void CTSController::ManageZafterNewExtraRec(void)
{
  int trueTiltInd = mTiltIndex - 1;
  if (trueTiltInd >= (int)mExtraStartingZ.size()) {
    mExtraStartingZ.push_back(mExtraFreeZtoWrite);
    mNumSavedExtras.push_back(0);
  } 
  if (trueTiltInd == (int)mExtraStartingZ.size() - 1) 
    mNumSavedExtras[trueTiltInd] += 1;

  if (mExtraOverwriteSec < 0)
    mExtraFreeZtoWrite++;
  else {
    mExtraOverwriteSec++;
    mExtraFreeZtoWrite = B3DMAX(mExtraFreeZtoWrite, mExtraOverwriteSec);
  }
}

// Determine whether extra records will be taken and set number
void CTSController::EvaluateExtraRecord()
{
  CString str = "";
  int ind, numex, numAvail, chan;

  mNumExtraRecords = 0;
  mNumSimultaneousChans = 1;
  switch (mTSParam.extraRecordType) {
    case NO_TS_EXTRA_RECORD:
      break;

    case TS_FOCUS_SERIES:
      mNumExtraRecords = mTSParam.numExtraFocus;
      break;

    case TS_FILTER_SERIES:
      if (mTSParam.numExtraFilter) {
        if (mCamParams->GIF || mScope->GetHasOmegaFilter())
          mNumExtraRecords = mTSParam.numExtraFilter;
        else if (!mWarnedNoExtraFilter) {
          str = "No extra " + mModeNames[RECORD_CONSET] + "s will be taken "
          "with different filter settings\r\n"
          "because the selected camera is not on the GIF";
          mWarnedNoExtraFilter = true;
        }
      }
      break;

    case TS_OPPOSITE_TRIAL:
      mNumExtraRecords = mWinApp->LowDoseMode() ? 2 : 0;
      if (mWinApp->mLowDoseDlg.ShiftsBalanced())
        str = "The tilt series will fail because\r\n"
          "opposite trials cannot be taken with Balance Shifts on";
      break;

    case TS_OTHER_CHANNELS:
      if (!mCamParams->STEMcamera) {
        if (!mWarnedNoExtraChannels) {
          str = "No extra " + mModeNames[RECORD_CONSET] + "s will be saved "
            "from other detectors\r\n"
            "because you are not in STEM mode";
          mWarnedNoExtraChannels = true;
        }
        break;
      }
      if (mCamParams->FEItype)
        mScope->GetFEIChannelList(mCamParams, true);
      mCamera->CountSimultaneousChannels(mCamParams, mSimultaneousChans, 3, 
        mNumSimultaneousChans, numAvail);
      if (!mNumSimultaneousChans) {
        str = "The tilt series will fail because the " + mModeNames[RECORD_CONSET] + 
          " parameter set\r\ndoes not specify an available STEM detector";
        break;
      }
      mNumExtraRecords = mNumSimultaneousChans - 1;

      // For each channel in the extra list not in the simultaneous channels and
      // not duplicated later in the kist, increment the record count
      numex = mTSParam.numExtraChannels;
      for (ind = 0; ind < numex; ind++) {
        chan = mTSParam.extraChannels[ind];
        if (!numberInList(chan, mSimultaneousChans, mNumSimultaneousChans, 0) && 
          (ind == numex - 1 || 
          !numberInList(chan, &mTSParam.extraChannels[ind+1], numex - ind - 1, 0)))
            mNumExtraRecords++;
      }
      if (!mNumExtraRecords && !mWarnedNoExtraChannels) {
        str = "No extra " + mModeNames[RECORD_CONSET] + "s will be saved "
          "because no other available detectors are specified";
        mWarnedNoExtraChannels = true;
      }
      break;
  }
  mStoreExtra[RECORD_CONSET] = mNumExtraRecords != 0;
  if (!str.IsEmpty()) {
    if (mUserPresent)
      AfxMessageBox(str, MB_EXCLAME);
    else
      mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  }
}

// Set up the state for the current extra record shot, saving existing state if needed
int CTSController::SetExtraRecordState(void)
{
  CString message;
  int error, numex, ind, extind, chan;
  double tryLoss, netLoss;
  int extraSet = RECORD_CONSET;

  SetupExtraOverwriteSec();

  // Do all the state saves first, then the beam/exposure changes
  if (!mInExtraRecState) {
    switch (mTSParam.extraRecordType) {
      case TS_FOCUS_SERIES:
        mDefocusBase = mScope->GetUnoffsetDefocus();
        break;

      case TS_FILTER_SERIES:

        // Save the state from low dose params or direct from filter settings
        if (mLowDoseMode) {
          mSlitStateBase = mLDParam[RECORD_CONSET].slitIn;
          mSlitWidthBase = mLDParam[RECORD_CONSET].slitWidth;
          mZeroLossBase = mLDParam[RECORD_CONSET].zeroLoss;
          mEnergyLossBase = mLDParam[RECORD_CONSET].energyLoss;
        } else {
          mSlitStateBase = mFilterParams->slitIn;
          mSlitWidthBase = mFilterParams->slitWidth;
          mZeroLossBase = mFilterParams->zeroLoss;
          mEnergyLossBase = mFilterParams->energyLoss;
        }
        break;

      case TS_OPPOSITE_TRIAL:
        extraSet = TRIAL_CONSET;
        break;

      case TS_OTHER_CHANNELS:
        mChannelIndBase = mConSets[RECORD_CONSET].channelIndex[0];
        break;

    }

    // set flag now; stop will restore everything correctly
    // Save a copy of the A before so it can be restored at end of extra Records
    mInExtraRecState = true;
    mSavedBufForExtra = new EMimageBuffer;
    mBufferManager->CopyImBuf(mImBufs, mSavedBufForExtra, false);
    mExposureBase = mConSets[extraSet].exposure;
    mDriftBase = mConSets[extraSet].drift;
    if (mTSParam.extraSetExposure && 
      mCamera->CanPreExpose(mCamParams, mConSets[extraSet].shuttering)) {
        mConSets[extraSet].drift = mTSParam.extraDrift;
    }
    if (mTSParam.extraSetBinning && mTSParam.extraRecordType == TS_OPPOSITE_TRIAL) {
      mBinningBase = mConSets[extraSet].binning;
      mConSets[extraSet].binning = mCamParams->binnings
        [B3DMIN(mTSParam.extraBinIndex, mCamParams->numBinnings - 1)];
    }

    if (mTSParam.extraSetSpot) {
      if (mLowDoseMode) {
        mSpotSizeBase = mLDParam[extraSet].spotSize;
        mLDParam[extraSet].spotSize = mTSParam.extraSpotSize;
      } else {
        mSpotSizeBase = mScope->GetSpotSize();
        mScope->SetSpotSize(mTSParam.extraSpotSize);
      }
    }
    if (mTSParam.extraDeltaIntensity != 1. && !mSTEMindex) {
      if (mLowDoseMode)
        mIntensityBase = mLDParam[extraSet].intensity;
      else
        mIntensityBase = mScope->GetIntensity();
      error = mBeamAssessor->ChangeBeamStrength(mTSParam.extraDeltaIntensity, 
        mLowDoseMode ? extraSet : -1);
      if (error && error != BEAM_ENDING_OUT_OF_RANGE) {
        TSMessageBox("Error setting beam intensity for extra Record image");
        ErrorStop();
        return 1;
      }
      if (error)
        mWinApp->AppendToLog("Warning: attempting to set beam intensity"
        " outside calibrated range for extra Record image", LOG_OPEN_IF_CLOSED);
    }
  }

  // Set the exposure time if a non-zero one exists for this index
  if (mTSParam.extraSetExposure && mTSParam.numExtraExposures > mExtraRecIndex &&
    mTSParam.extraExposures[mExtraRecIndex] > 0.)
      mConSets[extraSet].exposure = (float)mTSParam.extraExposures[mExtraRecIndex];

  // Now set the series-specific state items
  switch (mTSParam.extraRecordType) {
    case TS_FOCUS_SERIES:
      mScope->SetUnoffsetDefocus(mDefocusBase + mTSParam.extraFocus[mExtraRecIndex]);

      // Going beyond limits is fatal here
      if (CheckAndLimitAbsFocus()) {
        message.Format("Extra %s #%d would make focus go beyond the specified absolute "
          "limits.", (LPCTSTR)mModeNames[RECORD_CONSET], mExtraRecIndex + 1); 
        TSMessageBox(message);
        ErrorStop();
        return 1;
      }
      break;

    case TS_FILTER_SERIES:
  
      // Negative slit width means no filtering
      if (mTSParam.extraSlits[mExtraRecIndex] < 0) {
        mFilterParams->slitIn = false;
      } else {

        // Positive width is a width to use, 0 means use the base width
        mFilterParams->slitIn = true;
        if (mTSParam.extraSlits[mExtraRecIndex]) {
          if (mTSParam.extraSlits[mExtraRecIndex] > mFilterParams->maxWidth) {
            message.Format("Extra %s #%d specifies a slit width of %d.\nThis "
              "is beyond the allowed "
              "range for the filter.", (LPCTSTR)mModeNames[RECORD_CONSET], 
              mExtraRecIndex + 1, mTSParam.extraSlits[mExtraRecIndex]); 
            TSMessageBox(message);
            ErrorStop();
            return 1;
          }
          mFilterParams->slitWidth = (float)mTSParam.extraSlits[mExtraRecIndex];
        } else
          mFilterParams->slitWidth = mSlitWidthBase;

        // non-zero energy loss means an incremental loss, zero mean use base loss
        if (mTSParam.extraLosses[mExtraRecIndex]) {
          tryLoss = (mZeroLossBase ? 0. : mEnergyLossBase) + 
            mTSParam.extraLosses[mExtraRecIndex];
          if (mWinApp->mFilterControl.LossOutOfRange(tryLoss, netLoss)) {
            message.Format("Extra %s #%d specifies an energy loss of %.1f\nThis requires"
              "a net %s of %.1f with current adjustments\nand is beyond the allowed "
              "range for the filter.", (LPCTSTR)mModeNames[RECORD_CONSET], 
              mExtraRecIndex + 1, mScope->GetHasOmegaFilter() ? "shift" : "offset", 
              tryLoss, netLoss);
            TSMessageBox(message);
            ErrorStop();
            return 1;
          }
          mFilterParams->zeroLoss = false;
          mFilterParams->energyLoss = (float)tryLoss;
        } else {
          mFilterParams->zeroLoss = mZeroLossBase;
          mFilterParams->energyLoss = mEnergyLossBase;
        }
 
      }
      if (mLowDoseMode)
        mWinApp->mLowDoseDlg.SyncFilterSettings(RECORD_CONSET);
      mCamera->SetIgnoreFilterDiffs(true);
      mWinApp->mFilterControl.UpdateSettings();
      break;

    case TS_OPPOSITE_TRIAL:
      if (!mExtraRecIndex && mCamera->OppositeLDAreaNextShot()) {
        TSMessageBox("Opposite trials cannot be taken with Balance Shifts on");
        ErrorStop();
        return 1;
      }
      break;

    case TS_OTHER_CHANNELS:
      numex = mTSParam.numExtraChannels;
      extind = 0;

      // Just as above, count each channel that is not in simultaneous set or duplicated
      // and when the right count is reached, set the channel number
      for (ind = 0; ind < numex; ind++) {
        chan = mTSParam.extraChannels[ind];
        if (!numberInList(chan, mSimultaneousChans, mNumSimultaneousChans, 0) && 
          (ind == numex - 1 || 
          !numberInList(chan, &mTSParam.extraChannels[ind+1], numex - ind - 1, 0))) {
            if (extind + mNumSimultaneousChans - 1 == mExtraRecIndex) {
              mConSets[RECORD_CONSET].channelIndex[0] = chan - 1;
              break;
            }
            extind++;
        }
      }
      break;

  }

  return 0;
}

// On the first extra record, determine if overwriting
void CTSController::SetupExtraOverwriteSec(void)
{
  int trueTiltInd = mTiltIndex - 1;
  if (!mInExtraRecState && !mExtraRecIndex) {
    mExtraOverwriteSec = -1;
    if (trueTiltInd < (int)mExtraStartingZ.size()) {
      if (mNumSavedExtras[trueTiltInd] != mNumExtraRecords && 
        trueTiltInd != mExtraStartingZ.size() - 1) {
          mWinApp->AppendToLog("Warning: Number of extra Records to save does not "
            "match number previously saved at this tilt,\r\n  so new images are being"
            " appended to the file instead of overwriting existing images");
      } else {
        mExtraOverwriteSec = mExtraStartingZ[trueTiltInd];
        if (trueTiltInd == mExtraStartingZ.size() - 1)
          mNumSavedExtras[trueTiltInd] = 0;
      }
    }
  }
}

// Restore state from extra records if any
void CTSController::RestoreFromExtraRec(void)
{
  int extraSet = mTSParam.extraRecordType == 
    TS_OPPOSITE_TRIAL ? TRIAL_CONSET : RECORD_CONSET;

  if (mSwitchedToMontFrom >= 0) {
    mDocWnd->SetCurrentStore(mSwitchedToMontFrom);
    mSwitchedToMontFrom = -1;
  }

  if (!mInExtraRecState)
    return;

  // Restore beam/exposure settings
  mConSets[extraSet].exposure = mExposureBase;
  mConSets[extraSet].drift = mDriftBase;
  if (mTSParam.extraDeltaIntensity != 1. && !mSTEMindex) {
    if (mLowDoseMode)
      mLDParam[extraSet].intensity = mIntensityBase;
    else
      mScope->SetIntensity(mIntensityBase);
  }
  if (mTSParam.extraSetSpot) {
    if (mLowDoseMode)
      mLDParam[extraSet].spotSize = mSpotSizeBase;
    else
      mScope->SetSpotSize(mSpotSizeBase);
  }
  if (mTSParam.extraSetBinning && mTSParam.extraRecordType == TS_OPPOSITE_TRIAL)
    mConSets[extraSet].binning = mBinningBase;

  // Restore from filter or focus series
  switch (mTSParam.extraRecordType) {
    case TS_FOCUS_SERIES:
      mScope->SetUnoffsetDefocus(mDefocusBase);
      break;

    case TS_FILTER_SERIES:
      mFilterParams->slitIn = mSlitStateBase;
      mFilterParams->slitWidth = mSlitWidthBase;
      mFilterParams->zeroLoss = mZeroLossBase;
      mFilterParams->energyLoss = mEnergyLossBase;
      if (mLowDoseMode)
        mWinApp->mLowDoseDlg.SyncFilterSettings(RECORD_CONSET);

      // This takes care of the mag shift with current settings, set up filter since 
      // there is not an image necessarily coming
      mWinApp->mFilterControl.UpdateSettings();
      mCamera->SetupFilter();
      break;

    case TS_OTHER_CHANNELS:
      mConSets[RECORD_CONSET].channelIndex[0] = mChannelIndBase;
      break;
  }
  mInExtraRecState = false;

  // If there is a saved copy of A, roll A and B into B and C then copy it to A
  if (mSavedBufForExtra) {
    Sleep(1000);
    mBufferManager->CopyImageBuffer(1, 2);
    mBufferManager->CopyImageBuffer(0, 1);
    mBufferManager->CopyImBuf(mSavedBufForExtra, mImBufs, false);
    delete mSavedBufForExtra;
    mSavedBufForExtra = NULL;
  }
}


/////////////////////////////////////////////////////////////////////////////
// PREDICTION LOGIC

#define MAX_PRED_FITS 100

// Compute predictions and evaluate need for track and focus shots
void CTSController::ComputePredictions(double angle)
{
  int i, numDisturbed, nFitX, nFitY, nFitZ, nDropX, nDropY, nDropZ;
  int ixStart, iyStart, izStart, izFirst, izLast, firstUsable, toArea;
  float trackSize, trackCrit;
  int nUsable = 0;
  BOOL foundValid = false;
  float lastValidX, lastValidY, lowMagTolerance, stdErrorX, stdErrorY, stdErrorZ;
  float lastErrorX, lastErrorY, lastErrorZ, delX, delY, slope, slope2, intcp, roCorr;
  double delISX, delISY;
  float x1fit[MAX_PRED_FITS], x2fit[MAX_PRED_FITS], yfit[MAX_PRED_FITS];
  CString message, messadd;
  BOOL recentlyReversed = mDirection * (angle - mScope->GetReversalTilt()) < 
    mComplexTasks->GetTiltBacklash() - 3. * mMaxTiltError;
  double usableAngleDiff = mMaxUsableAngleDiff * cos(DTOR * angle);
    
  if (recentlyReversed)
    SEMTrace('1', "recentlyReversed: dir %d ang %f revtilt %f back %f 3mte %f",
      mDirection,  angle, mScope->GetReversalTilt(), mComplexTasks->GetTiltBacklash(),
      3. * mMaxTiltError);
  ixStart = mConSets[TRACKING_CONSET].right - mConSets[TRACKING_CONSET].left;
  iyStart = mConSets[TRACKING_CONSET].bottom - mConSets[TRACKING_CONSET].top;
  trackSize = (ixStart < iyStart ? ixStart : iyStart) * 
    mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), mTSParam.magIndex[mSTEMindex]);
  trackCrit = mTSParam.maxPredictErrorXY * trackSize;

  message = "";
  // Compute errors from last predictions unless disturbed
  if (mHaveXYPrediction) {
    lastErrorX = mSpecimenX[mTiltIndex - 1] - mPredictedX;
    lastErrorY = mSpecimenY[mTiltIndex - 1] - mPredictedY;
    message.Format("Errors in X/Y Prediction  %7.3f  %7.3f um,  %4.1f  %4.1f %%   ",
      lastErrorX, lastErrorY, fabs(100. * lastErrorX / trackSize), 
      fabs(100. * lastErrorY / trackSize));
  }
  if (!mHaveXYPrediction || (mDisturbance[mTiltIndex - 1] & SHIFT_DISTURBANCES)) {
    lastErrorX = 0.;
    lastErrorY = 0.;
  }

  // Get error from last focus prediction, or 0. if no focus was done
  if (mHaveFocusPrediction && mDidFocus[mTiltIndex - 1]) {
    lastErrorZ = mSpecimenZ[mTiltIndex - 1] - mPredictedZ;
    messadd.Format("Error in Z Prediction  %7.3f", lastErrorZ);
    message += messadd;
  } else
    lastErrorZ = 0.;

  if (!message.IsEmpty() && mVerbose)
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);

  if (mTiltIndex) {

    // If this is the first round after a user stop, mark a stopping disturbance
    // if the error was large enough
    if ((mDisturbance[mTiltIndex - 1] & USER_STOPPED) &&
      (fabs((double)lastErrorX) > 0.5 * trackCrit || 
      fabs((double)lastErrorY) > 0.5 * trackCrit))
      mDisturbance[mTiltIndex - 1] |= DRIFTED_AT_STOP;
    
    // Count how much data are available since last disturbance
    for (i = mTiltIndex - 1; i >= 0; i--) {
      if (mDisturbance[i] & SHIFT_DISTURBANCES)
        break;
      else
        nUsable++;
    }

    // Look for last valid change in X/Y: an interval over which there was no
    // disturbance.  Do not look past too many disturbances
    numDisturbed = B3DCHOICE(mDisturbance[mTiltIndex - 1] & SHIFT_DISTURBANCES, 1, 0);
    for (i = mTiltIndex - 1; i > 0 ; i--) {
      if (numDisturbed > mMaxDisturbValidChange)
        break;
      if (mDisturbance[i - 1] & SHIFT_DISTURBANCES) {
        numDisturbed++;
      } else {
        foundValid = true;
        lastValidX = mSpecimenX[i] - mSpecimenX[i - 1];
        lastValidY = mSpecimenY[i] - mSpecimenY[i - 1];
        break;
      }
    }
  }

  if (nUsable > MAX_PRED_FITS)
    nUsable = MAX_PRED_FITS;

  // DETERMINE NEED FOR TRACK AND FOCUS, AND MAKE PREDICTIONS
  // Less than two usable points: do track
  if (nUsable < 2) {
    mNeedRegularTrack = true;
    mHaveXYPrediction = false;

  // 2 or more points: do linear or quadratic fit separately to X and Y
  } else {

    // Drop points right after disturbance, as long as at least 3 are left
    if (nUsable >= 3) {
      nUsable -= mMaxDropAsShiftDisturbed;
      if (nUsable < 3)
        nUsable = 3;
    }

    // Do X first.  Look back beyond second point for first one outside the
    // selected fitting interval
    for (ixStart = mTiltIndex - 2; ixStart > mTiltIndex - nUsable; ixStart--)
      if (fabs(angle - mTiltAngles[ixStart - 1]) > mTSParam.fitIntervalX)
        break;
    nFitX = mTiltIndex - ixStart;

    // Fill arrays
    for (i = 0; i < nFitX; i++) {
      x1fit[i] = (float)i;
      x2fit[i] = (float)(i * i);
      yfit[i] = mSpecimenX[i + ixStart];
    }
  
    // get the prediction
    BestPredFit(x1fit, x2fit, yfit, nFitX, nDropX, mMinFitXAfterDrop, 
      mTSParam.minFitXQuadratic, slope, slope2, intcp, roCorr, (float)nFitX,
      mPredictedX, stdErrorX);

    // Now do the same for Y, with a potentially different interval
    for (iyStart = mTiltIndex - 2; iyStart > mTiltIndex - nUsable; iyStart--)
      if (fabs(angle - mTiltAngles[iyStart - 1]) > mTSParam.fitIntervalY)
        break;
    nFitY = mTiltIndex - iyStart;
    for (i = 0; i < nFitY; i++) {
      x1fit[i] = (float)i;
      x2fit[i] = (float)(i * i);
      yfit[i] = mSpecimenY[i + iyStart];
    }
  
    BestPredFit(x1fit, x2fit, yfit, nFitY, nDropY, mMinFitYAfterDrop, 
      mTSParam.minFitYQuadratic, slope, slope2, intcp, roCorr, (float)nFitY,
      mPredictedY, stdErrorY);

    // Track if only 2 points, if recent tilt reversal, if manual tracking,
    // if either X or Y last error is bigger
    // than the criterion, or if the standard error as a vector is bigger

    mNeedRegularTrack = nUsable == 2 || recentlyReversed || mTSParam.manualTracking ||
      fabs((double)lastErrorX) > trackCrit || 
      fabs((double)lastErrorY) > trackCrit ||
      sqrt((double)(stdErrorX * stdErrorX + stdErrorY * stdErrorY)) > trackCrit;
    mHaveXYPrediction = true;

    message.Format("Predicted X = %7.3f  StdErr = %7.3f  nFit = %d  nDrop = %d", 
      mPredictedX, stdErrorX, nFitX, nDropX);
    if (mVerbose)
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
    message.Format("Predicted Y = %7.3f  StdErr = %7.3f  nFit = %d  nDrop = %d", 
      mPredictedY, stdErrorY, nFitY, nDropY);
    if (mVerbose)
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  }
  
  // Now do Z, which may have gaps in data
  // Count how much data are available since last disturbance
  nUsable = 0;
  for (i = mTiltIndex - 1; i >= 0; i--) {
    if (mDisturbance[i] & FOCUS_DISTURBANCES)
      break;
    else if (mDidFocus[i]) {
      nUsable++;
      firstUsable = i;
    }
  }

  if (nUsable > MAX_PRED_FITS)
    nUsable = MAX_PRED_FITS;

  // Less than two usable points: do focus
  if (nUsable < 2 || mSkipFocus) {
    mNeedFocus = !mSkipFocus;
    mHaveFocusPrediction = false;

  // 2 or more points: do linear fit to Z's
  } else {

    // Drop points right after disturbance, as long as at least 3 are left
    if (nUsable >= 3) {
      nUsable -= mMaxDropAsFocusDisturbed;
      if (nUsable < 3)
        nUsable = 3;
    }

    // Get the number of points in the fitting interval for Z, but insist on at
    // least three before enforcing interval.  
    // Keep track of the first and last indexes where focus measured
    nFitZ = 0;
    izLast = -1;
    for (izStart = mTiltIndex -1 ; izStart >= firstUsable; izStart--) {
      if (nFitZ >= 3 && (fabs((double)(mTiltAngles[izLast] - 
        mTiltAngles[izStart])) > mTSParam.fitIntervalZ || nFitZ >= nUsable))
        break;
      if (mDidFocus[izStart]) {
        nFitZ++;
        izFirst = izStart;
        if (izLast < 0)
          izLast = izStart;
      }
    }
  
    // Fill arrays
    nFitZ = 0;
    for (i = izFirst; i < mTiltIndex; i++) {
      if (mDidFocus[i]) {
        x1fit[nFitZ] = (float)(i - izFirst);
        yfit[nFitZ++] = mSpecimenZ[i];
      }
    }
  
    // get the prediction
    BestPredFit(x1fit, x2fit, yfit, nFitZ, nDropZ, mMinFitZAfterDrop, 
      10000, slope, slope2, intcp, roCorr,
      (float)(mTiltIndex - izFirst), mPredictedZ, stdErrorZ);

    // Focus if last Error is big or twice standard error is big, or
    // if the span since the last focus is bigger than the span of the predictions
    // or if the span is greater  than the defined interval
    // or if we are above the forcing angle and that option is selected
    mNeedFocus = nUsable == 2 || 
      fabs((double)lastErrorZ) > mTSParam.maxPredictErrorZ[mSTEMindex] ||
      2. * stdErrorZ > mTSParam.maxPredictErrorZ[mSTEMindex] ||
      fabs(angle - mTiltAngles[izLast]) > 
      fabs((double)(mTiltAngles[izLast] - mTiltAngles[izFirst + nDropZ])) ||
      fabs(angle - mTiltAngles[izLast]) >= mTSParam.focusCheckInterval ||
      (mTSParam.alwaysFocusHigh && fabs(angle) > mTSParam.alwaysFocusAngle);

    mHaveFocusPrediction = true;

    message.Format("Predicted Z = %7.3f  StdErr = %7.3f  nFit = %d  nDrop = %d", 
      mPredictedZ, stdErrorZ, nFitZ, nDropZ);
    if (mVerbose)
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  }

  // force tracking if user stopped, because of unknown time delay
  // Also track after a refine ZLP
  // On next round we will evaluate whether to mark user stop as a real disturbance
  if (mTiltIndex > 0 && (mDisturbance[mTiltIndex - 1] & (USER_STOPPED | REFINED_ZLP)))
    mNeedRegularTrack = true;

  if (mLowDoseMode && mTSParam.alignTrackOnly)
    mNeedRegularTrack = true;
  
  // If low dose, force a new track reference at regular intervals, unless we were
  // getting a regular track anyway
  if (mLowDoseMode && fabs(angle - mLastLDTrackAngle) > 1.5 * mLastIncrement &&
    fabs(angle - mLastLDTrackAngle) > usableAngleDiff && !mNeedRegularTrack)
    mHaveTrackRef = false;
  

  // IMPLEMENT THE CHANGES
  mCheckFocusPrediction = false;
  if (mHaveFocusPrediction) {
    mPredictedFocus = mPredictedZ + (mSTEMindex ? 0.f : mTSParam.targetDefocus);
    mCheckFocusPrediction = mTSParam.repeatAutofocus;

    // If the scope might be changing the focus going in and out of View mode, then
    // we have to get out of View.  Pick the best area to go to and go there
    if (JEOLscope && mScope->GetLowDoseArea() == VIEW_CONSET) {
      toArea = RECORD_CONSET;
      if (mNeedFocus)
        toArea = FOCUS_CONSET;
      if (mNeedRegularTrack && mTSParam.trackAfterFocus != TRACK_AFTER_FOCUS)
        toArea = TRIAL_CONSET;
      SEMTrace('t', "In View, going to area %d before focus change", toArea);
      mScope->GotoLowDoseArea(toArea);
    }

    // 11/30/10: Switch from incDefocus to set call, avoid two defocus reads
    // If it goes outside absolute limits, let it be limited and force autofocusing to
    // give it a chance to come out within limits
    mScope->SetUnoffsetDefocus(mPredictedFocus);
    if (CheckAndLimitAbsFocus())
      mNeedFocus = true;
  }

  if (mHaveXYPrediction) {
    delX = mPredictedX - mSpecimenX[mTiltIndex - 1];
    delY = mPredictedY - mSpecimenY[mTiltIndex - 1];
    delISX = mCinv.xpx * delX + mCinv.xpy * delY;
    delISY = mCinv.ypx * delX + mCinv.ypy * delY;
    if (mSkipBeamShiftOnAlign)
      mScope->SetSkipNextBeamShift(true);
    mScope->IncImageShift(delISX, delISY);
    mScope->SetSkipNextBeamShift(false);
    mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
    //message.Format("Predict  X = %7.3f  Y = %7.3f  Z = %7.3f  Focus %7.2f", 
    //  mPredictedX, mPredictedY, mPredictedZ, mPredictedFocus);
    //mWinApp->AppendToLog(message, LOG_IF_ADMIN_OPEN_IF_CLOSED);
  }

  // DETERMINE IF LOW MAG ALIGNMENT IS NEEDED:
  // Compute tolerance as fraction of minimum size of tracking field
  lowMagTolerance = mLowMagFieldFrac * trackSize;
  mNeedLowMagTrack = false;
  mNeedTiltAfterMove = 0;

  // If there were no predictions (no last errors) do it unconditionally
  // (4/11/12, deleted code based on last valid changes)
  if (!lastErrorX && !lastErrorY)
    mNeedLowMagTrack = true;

  // If there was an error computed, do it if error was > tolerance
  else
    mNeedLowMagTrack = lowMagTolerance <
    sqrt((double)(lastErrorX * lastErrorX + lastErrorY * lastErrorY));

  // But if low mag track was not selected by user or we are in low dose or we just did
  // a TASM, do not use low mag track per se (which is at start of cycle)
  // Use tilt after move routine if recently reversed, every time until reversal is done
  if (!mTrackLowMag || mDidTiltAfterMove || recentlyReversed) {
    mNeedLowMagTrack = false;
    if (recentlyReversed)
      mNeedTiltAfterMove = -1;
  }

  // determine if current low mag ref is usable
  UsableLowMagRef(angle);

  // Need to call tilt after move routine if did a reset or stage was moved: but if 
  // already set to use due to tilt reversal, set up to use the bigger field size
  if (mComplexTasks->GetMinTASMField() > 0. && 
    (mDidResetIS || (mTiltIndex > 0 && 
    (mDisturbance[mTiltIndex - 1] & (IMAGE_SHIFT_RESET | USER_MOVED_STAGE)))) &&
    (!mNeedTiltAfterMove || 
    mComplexTasks->GetMinTASMField() > mComplexTasks->GetMinRTField()))
      mNeedTiltAfterMove = 1;
  mDidResetIS = false;
  mDidTiltAfterMove = false;

  // Cancel the regular track if there is a low mag tracking set up
  if (mNeedLowMagTrack && mUsableLowMagRef)
    mNeedRegularTrack = false;

  // restore mag if needed at this point if no low mag track needed
  if (!mNeedLowMagTrack && !mRoughLowMagRef)
    RestoreMag();

  message = "";
  if (mNeedLowMagTrack || mRoughLowMagRef)
    message += "Need low mag tracking  -";
  if (mNeedRegularTrack)
    message += "  Need regular tracking  -";
  if (mLowDoseMode && !mHaveTrackRef)
    message += "  Need new track reference  -";
  if (mNeedTiltAfterMove)
    message += "  Need tilt with tracking  - ";
  if (mNeedFocus)
    message += "  Need autofocus";
  if (!message.IsEmpty() && mVerbose)
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
}

void CTSController::BestPredFit(float *x1fit, float *x2fit, float *yfit, int &nFit, 
                int &nDrop, int minFit, int minQuadratic, float &slope,
                float &slope2, float &intcp, float &roCorr, 
                float predX, float &predY, float &stdError)
{
  int iDrop, maxDrop, bestDrop;
  float errors[MAX_PRED_FITS];
  float firstError, minError;
  maxDrop = nFit - minFit > 0 ? nFit - minFit : 0;
  minError = 1.e20f;
  nDrop = 0;

  // Compute a prediction with each candidate number of points dropped, find
  // the one with the minimum standard error
  for (iDrop = 0; iDrop <= maxDrop; iDrop++) {
    if (nFit - iDrop < minQuadratic)
      StatLSFitPred(&x1fit[iDrop], &yfit[iDrop], nFit - iDrop, &slope, &intcp,
        &roCorr, predX, &predY, &stdError);
    else
      StatLSFit2Pred(&x1fit[iDrop], &x2fit[iDrop], &yfit[iDrop], nFit - iDrop,
        &slope, &slope2, &intcp, predX, predX * predX, &predY, &stdError);
    errors[iDrop] = stdError;
    if (!iDrop)
      firstError = stdError;
    if (minError > stdError) {
      minError = stdError;
      bestDrop = iDrop;
    }
  }

  // If lowest error is less than the first by a large enough factor, search back
  // for last one that isn't much bigger
  if (minError * mFitDropErrorRatio < firstError) {
    for (iDrop = 0; iDrop <= bestDrop; iDrop++) {
      if (errors[iDrop] < minError * mFitDropBackoffRatio)
        break;
    }
    nDrop = iDrop;
  }
  nFit -= nDrop;

  // Do final fit
  if (nFit < minQuadratic)
    StatLSFitPred(&x1fit[nDrop], &yfit[nDrop], nFit, &slope, &intcp,
    &roCorr, predX, &predY, &stdError);
  else
    StatLSFit2Pred(&x1fit[nDrop], &x2fit[nDrop], &yfit[nDrop], nFit,
    &slope, &slope2, &intcp, predX, predX * predX, &predY, &stdError);
}

// Check if low dose trial area was moved, return true if so
BOOL CTSController::CheckLDTrialMoved()
{
  // Not moved if not low dose or no detectable change in delta
  if (!mLowDoseMode || 
    (fabs(mLDParam[2].ISX - mLDParam[0].ISX - mStopLDTrialDelX) <= 0.001 &&
      fabs(mLDParam[2].ISY - mLDParam[0].ISY - mStopLDTrialDelY) <= 0.001))
      return false;

  // If moved, invalidate the current track reference, set flag, clear out the 
  // reference stack, and reset the stopping delta values
  mHaveTrackRef = false;
  mLDTrialWasMoved = true;
  mStopLDTrialDelX = mLDParam[2].ISX - mLDParam[0].ISX;
  mStopLDTrialDelY = mLDParam[2].ISY - mLDParam[0].ISY;
  for (int i = mTrackStackBuf; i < mTrackStackBuf + mNumTrackStack; i++)
    mImBufs[i].DeleteImage();
  return true;
}

// Test for whether buffer A has an image that can be used as a reference to the
// given angle at given mag.  Test mag if not in low dose, test low dose areas otherwise
BOOL CTSController::UsableRefInA(double angle, int magIndex)
{
  float imAngle;
  if (!mImBufs->mImage || !mImBufs->GetTiltAngle(imAngle))
    return false;
  if ((!mWinApp->LowDoseMode() && mImBufs->mMagInd != magIndex) || 
    (mWinApp->LowDoseMode() && (!mImBufs->mLowDoseArea || 
    (mImBufs->mConSetUsed != RECORD_CONSET && mImBufs->mConSetUsed != TRIAL_CONSET && 
    mImBufs->mConSetUsed != PREVIEW_CONSET))))
    return false;
  return fabs(angle - imAngle) < mMaxUsableAngleDiff * cos(DTOR * angle);
}

// Test for whether walkup will be asked to take an anchor
BOOL CTSController::WalkupCanTakeAnchor(double curAngle, float anchorAngle,
                                        float targetAngle)
{  anchorAngle = (float)fabs(double(anchorAngle));
  return (targetAngle >= 0. && targetAngle >= anchorAngle && 
    anchorAngle >= curAngle + 2.) ||
    (targetAngle <= 0. && targetAngle <= -anchorAngle && -anchorAngle <= curAngle - 2.);
}

void CTSController::DebugDump(CString message)
{
  CString report;
  if (!mDebugMode)
    return;
  double angle = mScope->GetTiltAngle();
  report.Format("\r\n%s:  ActIndex %d  TiltIndex %d  CurrentTilt %.2f actual angle %.2f\r\n"
    "NextTilt %.2f  NeedTilt %d  PreTilted %d  Tilting %d  BackingUpTilt %d\r\n"
    "SingleStep %d  StopAtLoopEnd %d  UserStop %d  ErrorStop %d  HaveTrackRef %d\r\n"
    "HaveRecordRef %d  AcquiringImage %d  DoingMontage %d  Focusing %d  DidRegularTrack %d\r\n"
    "BWMeanPerSec[0] %.1f  BWmean in A %.1f\r\n",
    (LPCTSTR)message, mActIndex, mTiltIndex, mCurrentTilt, angle, mNextTilt, mNeedTilt ? 1 : 0,
    mPreTilted ? 1 : 0, mTilting ? 1 : 0, mBackingUpTilt ? 1 : 0, mSingleStep, 
    mStopAtLoopEnd ? 1 : 0, mUserStop ? 1 : 0, mErrorStop ? 1 : 0, mHaveTrackRef ? 1 : 0,
    mHaveRecordRef ? 1 : 0, mAcquiringImage ? 1 : 0, mDoingMontage ? 1 : 0,
    mFocusing ? 1 : 0, mDidRegularTrack ? 1 : 0,  mBWMeanPerSec[0], 
    mProcessImage->UnbinnedSpotMeanPerSec(mImBufs));
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
}

// Accumulate dose for a task, focus, or acquisition
double CTSController::AddToDoseSum(int which)
{
  int spotSize, set, probe;
  double intensity, dose;
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  double focusFac = 1.;
  float exposure;

  if (mSTEMindex)
    return 0.;

  // For task, just get task accumulated dose
  if (!which) {
    dose = mComplexTasks->GetAndClearDose();
    mDoseSums[0] += dose;
    return dose;
  }

  // For focus, multiple by 2 or 3
  if (which == FOCUS_CONSET)
    focusFac = mFocusManager->GetTripleMode() ? 3. : 2.;

  // Get intensity and spot from scope or low dose set
  if (mLowDoseMode) {
    set = mCamera->ConSetToLDArea(which);
    spotSize = ldParam[set].spotSize;
    intensity = ldParam[set].intensity;
    probe = ldParam[set].probeMode;
  } else {
    spotSize = mScope->FastSpotSize();
    intensity = mScope->FastIntensity();
    probe = mScope->GetProbeMode();
  }
  exposure = mCamera->SpecimenBeamExposure(mActiveCameraList[mTSParam.cameraIndex], 
    &mConSets[which]);
  dose = focusFac * mBeamAssessor->GetElectronDose(spotSize, intensity, 
    exposure, probe);
  mDoseSums[which] += dose;
  return dose;
}

// Return the Record area dose to this point, include Record, Preview, and on-axis tasks
double CTSController::GetCumulativeDose()
{
  return mDoseSums[RECORD_CONSET] + mDoseSums[PREVIEW_CONSET] + 
    mComplexTasks->GetOnAxisDose();
}

// Modify parameters in master control record from other interfaces
// Unconditionally get beam tilt, defocus target, tilt increment, cosine, delay
void CTSController::SyncOtherModulesToParam(void)
{
  mTSParam.beamTilt = mFocusManager->GetBeamTilt();
  mTSParam.targetDefocus = (float)(0.01 * floor(100. * 
    mFocusManager->GetTargetDefocus() + 0.5));
  mTSParam.autofocusOffset = mFocusManager->GetDefocusOffset();
  mTSParam.applyAbsFocusLimits = mFocusManager->GetUseEucenAbsLimits();
  mTSParam.cosineTilt = mScope->GetCosineTilt();
  mTSParam.tiltIncrement = mScope->GetBaseIncrement();
  mTSParam.tiltDelay = mShiftManager->GetTiltDelay();
}

// Take parameters in param and place back in other interfaces
void CTSController::SyncParamToOtherModules(void)
{
  mFocusManager->SetBeamTilt(mTSParam.beamTilt);
  mFocusManager->SetTargetDefocus((float)mTSParam.targetDefocus);
  mFocusManager->SetDefocusOffset(mTSParam.autofocusOffset);
  mFocusManager->SetUseEucenAbsLimits(mTSParam.applyAbsFocusLimits);
  mScope->SetCosineTilt(mTSParam.cosineTilt);
  mScope->SetIncrement(mTSParam.tiltIncrement);
  mShiftManager->SetTiltDelay(mTSParam.tiltDelay);
}

// Copy a TSParam - it could get more complicated, but not easily!
void CTSController::CopyTSParam(TiltSeriesParam *fromParam, TiltSeriesParam *toParam)
{
  *toParam = *fromParam;
}

// Copying a param into the master requires setting other program states too
void CTSController::CopyParamToMaster(TiltSeriesParam *param, bool sync)
{
  CopyTSParam(param, &mTSParam);
  if (sync)
    SyncParamToOtherModules();
}

// Copying the current master into a param requires gathering other states first
void CTSController::CopyMasterToParam(TiltSeriesParam *param, bool sync)
{
  if (sync)
    SyncOtherModulesToParam();
  CopyTSParam(&mTSParam, param);
}

// Copies the mag index and camera index from a montage param into a TS param, adjusting
// the probeMode and returning a STEMindex that indexes the magIndex
int CTSController::MontageMagIntoTSparam(MontParam *montParam, TiltSeriesParam *tsParam)
{
  int STEMindex;
  tsParam->cameraIndex = montParam->cameraIndex;
  CameraParameters *camParam = mWinApp->GetCamParams() + 
    mActiveCameraList[mTSParam.cameraIndex];
  mTSParam.probeMode = 0;
  if (camParam->STEMcamera && FEIscope)
    mTSParam.probeMode = montParam->magIndex >= mScope->GetLowestMicroSTEMmag() ? 1 : 0;
  STEMindex = camParam->STEMcamera ? 1 + mTSParam.probeMode : 0;
  mTSParam.magIndex[STEMindex] = montParam->magIndex;
  return STEMindex;
}

void CTSController::ManageTerminateMenu(void)
{
  CMenu *menu;
  CString menuText = mStartedTS && mBidirSeriesPhase == BIDIR_FIRST_PART ?
    "Ter&minate Part/Whole" : "Ter&minate";
  menu = mWinApp->m_pMainWnd->GetMenu()->GetSubMenu(7);
  menu->ModifyMenu(ID_TILTSERIES_TERMINATE, MF_BYCOMMAND | MF_STRING, 
    ID_TILTSERIES_TERMINATE, (LPCTSTR)menuText);
  mWinApp->m_pMainWnd->DrawMenuBar();
}

// Looks up the name of an action from and return its corresponding action index
int CTSController::LookupActionFromText(const char *actionName)
{
  for (int i = 0; i < sizeof(actionText) / sizeof(const char *); i++)
    if (!strcmp(actionText[i], actionName))
      return mActOrder[i + 1];
  return -1;
}

// Write simple text file with X/Y/Z positions and tilt angles
void CTSController::WriteTiltXYZFile(CString *inFile)
{
  CString filename;
  CStdioFile *cFile = NULL;
  if (mTiltIndex <= 0)
    return;
  if (inFile)
    filename = *inFile;
  else if (mWinApp->mDocWnd->GetTextFileName(false, false, filename))
    return;
  try {
    cFile = new CStdioFile(filename, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);
    for (int ind = 0; ind < mTiltIndex; ind++) {
      filename.Format("%6.2f  %7.3f  %7.3f  %7.3f\n", mTiltAngles[ind], mSpecimenX[ind],
        mSpecimenY[ind], mSpecimenZ[ind]);
      cFile->WriteString(filename);
    }
    cFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    AfxMessageBox("Error writing position data to file", MB_EXCLAME);
  } 
  if (cFile)
    delete cFile;
}
