// EMscope.cpp           The microscope module, performs all interactions with
//                         microscope except ones by threads that need their
//                         own instance of the Tecnai
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
#include ".\EMscope.h"
#include "PluginManager.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "CameraController.h"
#include "TSController.h"
#include "MultiTSTasks.h"
#include "AutocenSetupDlg.h"
#include "StageMoveTool.h"
#include "RemoteControl.h"
#include "BaseSocket.h"
#include <assert.h>
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define AUTOSAVE_INTERVAL_MIN 5
#define MUTEX_TIMEOUT_MS 502
#define DATA_MUTEX_WAIT  100

// This timeout is long enough so that post-action routine should always succeed in
// getting mutex because update routine should finish quickly
#define MAG_MUTEX_WAIT   500

// Scope plugin version that is good enough if there are no FEI cameras, and if there 
// are cameras.  This allows odd features to be added without harrassing other users
#define FEISCOPE_NOCAM_VERSION 105
#define FEISCOPE_CAM_VERSION   107

// Global variables for scope identity
bool JEOLscope = false;
bool LikeFEIscope = false;
bool FEIscope = false;
bool HitachiScope = false;
bool UsingScopeMutex = false;

static VOID CALLBACK UpdateProc(
  HWND hwnd,     // handle of window for timer messages
  UINT uMsg,     // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime   // current system time
);

static void debugCallback(char *msg);
static void ManageTiltReversalVars();

static double sTiltAngle;
static BOOL sInitialized = false;
static int sSuspendCount = 0;            // Count for update suspension
static double sReversalTilt = 100.;
static double sExtremeTilt = 100.;
static int sTiltDirection = 1;
static float sTiltReversalThresh = 0.1f;    // Set below to 0.2 for JEOL and maxTiltError
static BOOL sScanningMags = false;
static float sJEOLstageRounding = 0.001f;  // Round to this in microns
static float sJEOLpiezoRounding = 0.f;   // Round these in microns, or 0 to skip piezo
static BOOL sBeamBlanked = false;        // To keep track of actual blank state
static _variant_t *vFalse;
static _variant_t *vTrue;
static BOOL sMessageWhenClipIS = true;
static BOOL sLDcontinuous = false;
static double sCloseValvesInterval = 0.;  // Interval after which to close valves
static double sCloseValvesStart;          // Start of interval
static double sProbeChangedTime = -1.;
static double sDiffSeenTime = -1.;
static BOOL sClippingIS = false;
static CString sLongOpDescriptions[MAX_LONG_THREADS];
static int sLongThreadMap[MAX_LONG_OPERATIONS];
static int sJeolIndForMagMode = JEOL_MAG1_MODE;  // Index to use in mag mode for JEOL
static int sJeolSecondaryModeInd = JEOL_SAMAG_MODE;  // Index to use for secondary mode
static BOOL sCheckPosOnScreenError = false;
CString sMessageBoxTitle;   // Arguments for calling message box function
CString sMessageBoxText;
int sMessageBoxType;
int sMessageBoxReturn;
int sCartridgeToLoad = -1;

// Parameters controlling whether to wait for stage ready if error on slow movement
static float sStageErrSpeedThresh = 0.15f;  // For speed factors below this
static int sStageErrTimeThresh = 119;       // For a COM error occurring after this # sec
static float sStageErrWaitFactor = 9.;      // Wait for this # sec divided by speed factor

static HANDLE sMagMutexHandle;
static HANDLE sDataMutexHandle;

// Jeol calls for screen pos will all work in terms of spUpJeol, etc
// The rest of the program will use the standard FEI definitions and JEOL positions are
// converted with spJeolToFEI[JoelPosition]
static int spJeolToFEI[] = {spUp, spDown, spUnknown};
static int spFEItoJeol[] = {0, spUnknownJeol, spUpJeol, spDownJeol};

#define VAR_BOOL(a) ((a) ? *vTrue : *vFalse)

#define AUTONORMALIZE_SET(v) { \
  if (mPlugFuncs->SetAutoNormEnabled) \
    mPlugFuncs->SetAutoNormEnabled(v); \
}

#define USE_DATA_MUTEX(a) { \
  SEMAcquireJeolDataMutex(); \
  a; \
  SEMReleaseJeolDataMutex(); \
}

#define FAST_GET(t, f) \
{ GetValuesFast(1); \
  t retval = f(); \
  GetValuesFast(0); \
  return retval; \
}

#define ALL_INSTRUMENT_LENSES 99

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

HANDLE CEMscope::mScopeMutexHandle; 
char * CEMscope::mScopeMutexOwnerStr;
DWORD  CEMscope::mScopeMutexOwnerId;
int    CEMscope::mScopeMutexOwnerCount;
int    CEMscope::mLastMagIndex;
BOOL   CEMscope::mMagChanged;
int    CEMscope::mInternalPrevMag;
double CEMscope::mInternalMagTime;
double CEMscope::mLastISX = 0.;
double CEMscope::mLastISY = 0.;
double CEMscope::mPreviousISX;
double CEMscope::mPreviousISY;
char * CEMscope::mFEIInstrumentName;
ScopePluginFuncs *CEMscope::mPlugFuncs;

CEMscope::CEMscope()
{
  int i;
  SEMBuildTime(__DATE__, __TIME__);
  sInitialized = false;
  vFalse = new _variant_t(false);
  vTrue = new _variant_t(true);
  mUseTEMScripting = 0;
  mIncrement = 1.5;
  mStageThread = NULL;
  mScreenThread = NULL;
  mFilmThread = NULL;
  for (i = 0; i < MAX_LONG_THREADS; i++)
    mLongOpThreads[i] = NULL;
  for (i = 0; i < MAX_LONG_OPERATIONS; i++) {
    mLastLongOpTimes[i] = 0;
    sLongThreadMap[i] = 0;
  }
  sLongThreadMap[LONG_OP_HW_DARK_REF] = 1;
  for (i = 0; i < MAX_GAUGE_WATCH; i++)
    mGaugeIndex[i] = -1;
  mDoingLongOperation = false;
  mBeamBlankSet = false;
  mMagChanged = true;
  mLastNormalization = GetTickCount();
  mLastStageCheckTime = mLastNormalization;
  mLastTiltTime = mLastNormalization;
  mLastStageTime = mLastNormalization;
  m_bCosineTilt = false;
  mScreenCurrentFactor = 1.;
  mLastTiltChange = 0.;
  mMaxTiltAngle = 79.9f;
  mSmallScreenFactor = 1.33f;
  mCameraAcquiring = false;
  mBlankWhenDown = false;
  mLowDoseSetArea = -1;
  mLowDoseDownArea = 0;
  mLowDoseMode = false;
  mLDNormalizeBeam = false;
  mLDBeamNormDelay = 100;
  mLDViewDefocus = 0.;
  mSearchDefocus = 0.;
  mSelectedEFTEM = false;
  mSelectedSTEM = 0;
  mHasOmegaFilter = false;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mMagTab = mWinApp->GetMagTable();
  mActiveCamList = mWinApp->GetActiveCameraList();
  mFromPixelMatchMag = 0;
  mToPixelMatchMag = 0;
  mSTEMfromMatchMag = 0;
  mSTEMtoMatchMag = 0;
  mNonGIFfromMatchMag = 0;
  mNonGIFtoMatchMag = 0;
  mFiltParam = mWinApp->GetFilterParams();
  mFiltParam->minLoss = MINIMUM_ENERGY_LOSS;
  mFiltParam->maxLoss = MAXIMUM_ENERGY_LOSS;
  mFiltParam->minWidth = MINIMUM_SLIT_WIDTH;
  mFiltParam->maxWidth = MAXIMUM_SLIT_WIDTH;
  mFiltParam->adjustForSlitWidth = true;      // Need to adjust on GIF
  mFiltParam->positiveLossOnly = false;
  mLowestMModeMagInd = 17;
  mLowestSecondaryMag = 0;
  mHighestLMindexToScan = -1;
  mSecondaryMode = 0;
  mLowestGIFModeMag = 0;
  mLowestSTEMnonLMmag[0] = 1;
  mLowestSTEMnonLMmag[1] = 1;
  mLowestMicroSTEMmag = 1;
  mNumShiftBoundaries = 0;
  mNumSpotSizes = -1;
  for (i = 0; i <= MAX_SPOT_SIZE; i++) {
    mC2SpotOffset[i][0] = mC2SpotOffset[i][1] = 0.; 
    mCrossovers[i][0] = mCrossovers[i][1] = 0.;
  }
  mNumAlphaBeamShifts = 0;
  mNumAlphaBeamTilts = 0;
  mNumGauges = 0;
  mStageLimit[STAGE_MIN_X] = mStageLimit[STAGE_MIN_Y] = -990.;
  mStageLimit[STAGE_MAX_X] = mStageLimit[STAGE_MAX_Y] = 990.;
  mTiltAxisOffset = 0.;
  mShiftToTiltAxis = false;
  mLastMagIndex = 0;
  mLastRegularMag = 0;
  mLastEFTEMmag = 0;
  mLastSTEMmag = 0;
  mLastSTEMmode = -1;
  mSimulationMode = false;

  // JEOL initializations
  mJeolSD.tiltAngle = 0;
  mJeolSD.stageReady = false;
  mJeolSD.magIndex = 0;
  mJeolSD.spotSize = 1;
  mJeolSD.alpha = -999;
  mJeolSD.objective = 0;
  mJeolSD.objectiveFine = 0;
  mJeolSD.current = 0;
  mJeolSD.rawIntensity = 0;
  mJeolSD.stageX = 0;
  mJeolSD.stageY = 0;
  mJeolSD.ISX = 0.;
  mJeolSD.ISY = 0.;
  mJeolSD.numDetectors = 0;
  mJeolSD.energyShift = 0.;
  mJeolSD.shiftOn = false;
  mJeolSD.slitWidth = 0;
  mJeolSD.slitIn = false;
  mJeolSD.spectroscopy = false;
  mJeolSD.JeolEFTEM = -1;
  mJeolSD.JeolSTEM = false;
  mJeolSD.valveOrFilament = -1;
  mJeolSD.STEMchanging = false;
  mJeolSD.suspendUpdate = false;
  mJeolSD.magInvalid = false;
  mJeolSD.terminate = 0;
  mJeolSD.numSequence = 31;
  mJeolSD.lowSequence = 0;
  mJeolSD.lowerSequence = 0;
  mJeolSD.highSequence = 0;
  mJeolSD.lowHigh = 0;
  mJeolSD.doneHigh = 0;
  mJeolSD.doneLow = 0;
  mJeolSD.doLower = false;
  mJeolSD.usePLforIS = false;
  mJeolSD.useCLA2forSTEM = false;

  mJeolSD.internalMagTime = -1.;
  mJeolSD.changedMagTime = -1.;
  mJeolSD.skipMagEventTime = 700;
  mJeolSD.internalIStime = -1.;
  mJeolSD.skipISeventTime = 1500;
  mJeolSD.ISRingIndex = 0;
  mJeolSD.magRingIndex = 0;
  for (int j = 0; j < IS_RING_MAX; j++)
    mJeolSD.ringMagTime[j] = mJeolSD.ringISTime[j] = -1.;
  
  mJeolSD.bDataIsGood = 0;
  mJeolSD.updateByEvent = false;
  mJeolSD.comReady = false;
  
  mJeolSD.baseFocus = 32 * 0x8000 + 0x8000;
  mJeolSD.coarseBaseFocus = 0x8000;
  mJeolSD.fineBaseFocus = 0x8000;
  mJeolSD.miniBaseFocus = 0x8000;
  mC2IntensityFactor[0] = mC2IntensityFactor[1] = 1.;
  mJeolUpdateSleep = 200;
  mInitializeJeolDelay = 3000;
  mUpdateByEvent = true;
  mScreenByEvent = true;
  mSpectrumByEvent = false;
  mJeol_OLfine_to_um = 0.0058;
  mJeol_CLA1_to_um = 0.025;     // was X = 0.0252, Y = 0.0246
  mJeol_LMCLA1_to_um = 0.;
  mJeol_OM_to_um = 0.01;        // measured on 2100
  mJeolSTEMdefocusFac = 0.7143; // Measured on Indiana 3200
  mReportsSmallScreen = true;
  mReportsLargeScreen = 1;
  mStandardLMFocus = -999.;

  // This was never initialized for FEI before...
  for (i = 0; i < MAX_MAGS; i++) {
    mLMFocusTable[i][0] = mLMFocusTable[i][1] = -999.;
  }
  mRoughPLscale = 1.5;        // PL calibrations are ~100 unbinned 15 um pixels/unit

  // General initializations (-1 for these 2 that are scope-dependent)
  mHasNoAlpha = -1;
  mUpdateInterval = 100;
  mMagFixISdelay = 450;    // Was 300: needed to be longer for diffraction
  mJeolForceMDSmode = 0;
  mCalNeutralStartMag = -1;
  mJeolPostMagDelay = 0;   // This is defaulted to 1250 below if events are on
  mJeolSTEMPreMagDelay = 1500;
  mJeolMagEventWait = 5000;  // Actual times as long as 16 have been seen when VME broken
  mPostMagStageDelay = 0;
  mNoColumnValve = false;
  mUpdateBeamBlank = -1;
  mBlankTransients = false;
  mManualExposure = 0.;
  mWarnIfKVnotAt = 0.;
  mMainDetectorID = 13;    // Default for main screen
  mJeol1230 = false;
  mMagIndSavedIS = -1;
  sMagMutexHandle = CreateMutex(0, 0, 0);
  mHandledMag = -1;
  mAdaExists = -1;
  mNoScope = 0;
  mLastLDpolarity = 1;
  mNextLDpolarity = 1;
  mLdsaParams = NULL;
  mUseIllumAreaForC2 = false;
  mLDBeamTiltShifts = false;
  mShutterlessCamera = 0;
  mProbeMode = 0;
  mProbeChangeWait = 1500;
  mDiffractOrSTEMwait = 2000;
  mSTEMswitchDelay = 2000;
  mLastSTEMfocus = -1.e10;
  mLastRegularFocus = -1.e10;
  mNeedSTEMneutral = true;
  mSTEMneutralISX = 0.;
  mSTEMneutralISY = 0.;
  mInsertDetectorDelay = 700;
  mSelectDetectorDelay = 700;
  mJeolSTEMrotation = -999;
  mJeolSwitchSTEMsleep = 1000;
  mJeolSwitchTEMsleep = 0;
  mPostFocusChgDelay = 0;
  mPostJeolGIFdelay = 3000;
  mUseJeolGIFmodeCalls = 0;
  mJeolSTEMunitsX = false;
  mUsePLforIS = false;
  mBacklashTolerance = -1.;
  mXYbacklashValid = false;
  mStageAtLastPos = false;
  mBacklashValidAtStart = false;
  mMinMoveForBacklash = 0.;
  mStageRelaxation = 0.025f;
  mMovingStage = false;
  mBkgdMovingStage = false;
  mLastGaugeStatus = gsInvalid;
  mVacCount = 0;
  mLastSpectroscopy = false;
  mErrCount = 0;
  mLastReport = 0;
  mReportingErr = false;
  mLastLowDose = false;
  mLastScreen = (int)spDown;
  mAutosaveCount = 0;
  mCheckFreeK2RefCount = 0;
  mChangingLDArea = false;
  mIllumAreaLowLimit = 0.;
  mIllumAreaHighLimit = 0.;
  mIllumAreaLowMapTo = 0.1f;
  mIllumAreaHighMapTo = 0.9f;
  mNeutralIndex = 0;
  mSkipNextBeamShift = false;
  mUsePiezoForLDaxis = false;
  mFocusCameFromView = false;
  mFalconPostMagDelay = 10;
  mAlphaChangeDelay = 500;
  mHitachiMagFocusDelay = 250;   // Times up to 125 seen on a local scope
  mHitachiMagISDelay = 150;      // Shows up within one check of focus and within 125 ms
  mInternalMagTime = -1.;
  mISwasNewLastUpdate = false;
  mHitachiMagChgSeeTime = 200;
  mMagChgIntensityDelay = -1;
  mLastMagModeChgTime = -1.;
  mAdjustFocusForProbe = false;
  mNormAllOnMagChange = 0;
  mFirstFocusForProbe = EXTRA_NO_VALUE;
  mPostProbeDelay = 3000;
  mFirstBeamXforProbe = EXTRA_NO_VALUE;
  mHitachiModeChgDelay = 400;
  mDisconnected= false;
  mMinSpotWithBeamShift[0] = 0;
  mMaxSpotWithBeamShift[0] = 0;
  mMinSpotWithBeamShift[1] = 0;
  mMaxSpotWithBeamShift[1] = 0;
  mHitachiSpotStepDelay = 300;
  mHitachiSpotBeamWait = 120;
  mLastNormMagIndex = -1;
  mFakeMagIndex = 1;
  mFakeScreenPos = spUp;
  mUseInvertedMagRange = false;
  mGettingValuesFast = false;
  mLastGettingFast = false;
  mPluginVersion = 0;
  mPlugFuncs = NULL;
  mScopeMutexHandle = NULL;
}

CEMscope::~CEMscope()
{
  if (mUpdateID)
    KillUpdateTimer();

  if (mScopeMutexHandle) {
    CloseHandle(mScopeMutexHandle);
    free(mScopeMutexOwnerStr);
  }

  sInitialized = false;
  delete vTrue;
  delete vFalse;
}

// Kill the timer so it doesn't try to update dying windows
void CEMscope::KillUpdateTimer()
{
  if (mUpdateID) 
    ::KillTimer(NULL, mUpdateID);

  // scope uninitialize is called by pluginManager from ExitInstance which is late...
  mUpdateID = 0;
  sInitialized = false;
}

// 1/24/14: Eliminated code related to mSingleTecnai.  The scope mutex with socket server
// may serve the same function; 5/9/16: removed comments about it

int CEMscope::Initialize()
{
  int startErr = 0, startCall = 0;
  static bool firstTime = true;
  double htval;
  int ind, ind2;
  HitachiParams *hitachi = mWinApp->GetHitachiParams();
  int *camLen = mWinApp->GetCamLenTable();
  CString message;
  mShiftManager = mWinApp->mShiftManager;
  int *activeList = mWinApp->GetActiveCameraList();
  CameraParameters *camParam = mWinApp->GetCamParams();
  if (mNoScope)
    return 0;
  if (sInitialized) {
    AfxMessageBox(_T("Warning: Call to EMscope::Initialize when"
      " scope is currently initialized"), MB_EXCLAME);
    return 1;
  }

  // If scope is not defined as JEOL, look for a plugin scope
  message = mWinApp->mPluginManager->GetScopePluginName();
  if (message.IsEmpty()) {
    AfxMessageBox("No microscope plugin was loaded and one is required", MB_EXCLAME);
    return 1;
  }
  if (message == "ScopePlugin") {
    AfxMessageBox("A sample microscope plugin was loaded, and a real one is "
      "required instead", MB_EXCLAME);
    return 1;
  }

  mPlugFuncs = mWinApp->mPluginManager->GetScopeFuncs();
  if (message == "HitachiScope") {
    HitachiScope = true;
  } else if (message == "FEIScope") {
    if (!mPlugFuncs->BeginThreadAccess) {
      AfxMessageBox("An FEI scope plugin was loaded but it is too old\n"
        "to use with this version of SerialEM", MB_EXCLAME);
      return 1;
    }
    FEIscope = true;
  } else if (message == "JEOLScope") {
    JEOLscope = true;
  } else {
    AfxMessageBox("An unrecognized scope plugin was loaded, and a real one is "
      "required instead", MB_EXCLAME);
    return 1;
  }
  LikeFEIscope = FEIscope || HitachiScope;

  // Set these here, they need to be reset if mMoveInfo is copied to but are otherwise
  // reliably set for use
  mMoveInfo.JeolSD = &mJeolSD;
  mMoveInfo.plugFuncs = mPlugFuncs;

  if (firstTime) {

    // Create the mutex here for JEOL, Hitachi, and FEI plugin without separate threads
    if (JEOLscope || HitachiScope) {
      mScopeMutexHandle = CreateMutex(0,0,0);
      mScopeMutexOwnerStr = (char *)calloc(1024,1);
      mScopeMutexOwnerCount = 0;
      mScopeMutexOwnerId = 0;
      strcpy(mScopeMutexOwnerStr,"NOBODY");
      UsingScopeMutex = true;
    }

    // Finish conditional initialization of variables
    if (FEIscope) {
      mCanControlEFTEM = true;
      mC2Name = mUseIllumAreaForC2 ? "IA" : "C2";
      mStandardLMFocus = 0.;
      mHasNoAlpha = 1;
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.1f;
      if (mNumSpotSizes <= 0)
        mNumSpotSizes = 11;
      if (mUpdateBeamBlank < 0)
        mUpdateBeamBlank = mWinApp->GetHasFEIcamera() ? 1 : 0;
    } else if (JEOLscope) {

      // JEOL: Also transfer values to structures before initialization
      sDataMutexHandle = CreateMutex(0,0,0);
      mCanControlEFTEM = mUseJeolGIFmodeCalls > 1;
      mC2Name = "C3";
      mStandardLMFocus = -999.;
      if (mHasNoAlpha < 0)
        mHasNoAlpha = 0;
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.2f;
      if (mSimulationMode) {
        sJEOLstageRounding = 0.1f;
        mLastRegularFocus = 0.;
        mLastSTEMfocus = 0.;
      }
      mJeolSD.CLA1_to_um = mJeol_CLA1_to_um;
      mJeolSD.doMiniInLowMag = mJeol_OM_to_um > 0.;
      mJeolSD.postMagStageDelay = mPostMagStageDelay;
      mJeolSD.lowestNonLMmag = mLowestMModeMagInd;
      SetJeolUsePLforIS(mUsePLforIS);
      mJeolSD.useCLA2forSTEM = mUseCLA2forSTEM;
      mJeolParams.STEMunitsX = mJeolSTEMunitsX;
      mJeolParams.scanningMags = false;
      mJeolParams.indForMagMode = sJeolIndForMagMode;
      mJeolParams.secondaryModeInd = sJeolSecondaryModeInd;
      mJeolParams.lowestSecondaryMag = mLowestSecondaryMag;
      mJeolParams.CLA1_to_um = mJeol_CLA1_to_um;
      mJeolParams.OLfine_to_um = mJeol_OLfine_to_um;
      mJeolParams.OM_to_um = mJeol_OM_to_um;
      mJeolParams.noColumnValve = mNoColumnValve;
      mJeolParams.stageRounding = sJEOLstageRounding;
      mJeolParams.reportsLargeScreen = mReportsLargeScreen > 0;
      mJeolParams.simulationMode = mSimulationMode;
      mJeolParams.postMagDelay = mJeolPostMagDelay;
      mJeolParams.magEventWait = mJeolMagEventWait;
      mJeolParams.STEMdefocusFac = mJeolSTEMdefocusFac;
      mJeolParams.STEMrotation = mJeolSTEMrotation;
      mJeolParams.scopeHasSTEM = mWinApp->ScopeHasSTEM();
      mJeolParams.hasOmegaFilter = mHasOmegaFilter;
      mJeolParams.initializeJeolDelay = mInitializeJeolDelay;
      mJeolParams.useGIFmodeCalls = mUseJeolGIFmodeCalls;
      mJeolSD.mainDetectorID = mMainDetectorID;

      // Event stuff is subject to revision if it fails, including spectrum by event
      mScreenByEvent = mUpdateByEvent;  // This hasn't needed to be separate
      mJeolParams.screenByEvent = mScreenByEvent;
      mJeolSD.setUpdateByEvent = mUpdateByEvent;

      mJeolSD.lowestSTEMnonLMmag = mLowestSTEMnonLMmag[0];
      mJeolSD.Jeol1230 = mJeol1230;
      if (mJeol1230)
        mHasNoAlpha = 1;
      
      sTiltReversalThresh = 0.2f;
      if (mNumSpotSizes <= 0)
        mNumSpotSizes = 5;
      if (mMagChgIntensityDelay < 0)
        mMagChgIntensityDelay = 200;

    } else {

      // Hitachi
      mCanControlEFTEM = false;
      mStandardLMFocus = -999.;   // ??
      mHasNoAlpha = 1;
	    mC2Name = "C2";
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.1f;  // WHATEVER
      mMaxTiltAngle = B3DMIN(mMaxTiltAngle, 60.f);
      for (ind = 0; ind < MAX_MAGS; ind++)
        hitachi->magTable[ind] = mMagTab[ind].mag;
      for (ind = 0; ind < MAX_CAMLENS; ind++)
        hitachi->camLenTable[ind] = camLen[ind];
      hitachi->lowestNonLMmag = mLowestMModeMagInd;
      hitachi->lowestSecondaryMag = mLowestSecondaryMag;
      if (mNumSpotSizes <= 0)
        mNumSpotSizes = 5;
      if (mMagChgIntensityDelay < 0)
        mMagChgIntensityDelay = 30;

      // Add shift boundaries for HR mode.  LM is pretty good in most of range
      if (mLowestSecondaryMag > 0) {
        for (ind = mLowestSecondaryMag; ind < MAX_MAGS; ind++)
          if (mNumShiftBoundaries < MAX_MAGS - 1 && mMagTab[ind].mag)
            mShiftBoundaries[mNumShiftBoundaries++] = ind;
      }
    }

    // More general initialization
    mC2Units = mUseIllumAreaForC2 ? "um" : "%";
    sTiltReversalThresh = B3DMAX(sTiltReversalThresh, 
      mWinApp->mTSController->GetMaxTiltError());

    // Add a boundary for secondary mag range
    if (mLowestSecondaryMag && !mUsePLforIS && !HitachiScope) {
      for (ind = 0; ind <= mNumShiftBoundaries; ind++) {
        if (ind == mNumShiftBoundaries || mLowestSecondaryMag <= mShiftBoundaries[ind]) {
          if (ind == mNumShiftBoundaries) {
            mShiftBoundaries[mNumShiftBoundaries++] = mLowestSecondaryMag;
          } else if (mLowestSecondaryMag < mShiftBoundaries[ind]) {
            for (ind2 = mNumShiftBoundaries; ind2 > ind; ind2--)
              mShiftBoundaries[ind2] = mShiftBoundaries[ind2 - 1];
            mShiftBoundaries[ind] = mLowestSecondaryMag;
            mNumShiftBoundaries++;
          }
          break;
        }
      }
    }
  }

  // Get the various pointers for the scope objects
  try {
    if (mPlugFuncs->InitializeScope)
      mPlugFuncs->InitializeScope();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, "initializing scope plugin ");
    return 1;
  }
  sInitialized = true;

  // Now set up a lot of things for TEMCON JEOL scope
  if (JEOLscope) {
    mPlugFuncs->GetNumStartupErrors(&startErr, &startCall);

    // Revise updating by event based on results of connecting to sinks
    mUpdateByEvent = mScreenByEvent = mJeolParams.screenByEvent =
      mJeolSD.setUpdateByEvent;
    if (!mUpdateByEvent)
      mSpectrumByEvent = false;

    // Reset the defocus means measure the starting focus and set as zero value
    // The base focus is tracked in the plugin now
    if (GetStandardLMFocus(mLowestMModeMagInd / 2) > -900.)
      mJeolSD.miniBaseFocus = (int)(65535. * GetStandardLMFocus(mLowestMModeMagInd / 2)
      + 0.5);
    if (!ResetDefocus(true))
      startErr++;
    startCall++;

    // Set the flags for initial update call and default for following calls
    mJeolSD.highFlags = JUPD_STAGE_POS | JUPD_STAGE_STAT | JUPD_STAGE_STAT 
      | JUPD_MAG | JUPD_CURRENT | JUPD_INTENSITY | JUPD_SPOT | JUPD_IS | JUPD_FOCUS;
    if (mReportsLargeScreen > 0 && !mJeol1230)
      mJeolSD.highFlags |= JUPD_SCREEN;
    else {
      mJeolSD.screenPos = mReportsLargeScreen < 0 ? spUnknownJeol : spDownJeol;
      mReportsLargeScreen = 0;
    }
    if (mReportsSmallScreen && !mJeol1230)
      mJeolSD.highFlags |= JUPD_SMALL_SCREEN;
    else
      mJeolSD.smallScreen = 0;
    if (mJeol1230)
      mJeolSD.highFlags &= ~JUPD_SPOT;
    if (mWinApp->GetShowRemoteControl())
      mJeolSD.highFlags |= JUPD_BEAM_STATE;
    if (mHasOmegaFilter)
      mJeolSD.highFlags |= JUPD_ENERGY | JUPD_ENERGY_ON | JUPD_SLIT_IN | 
      JUPD_SLIT_WIDTH | JUPD_SPECTRUM;
    if (mJeolSD.numDetectors)
      mJeolSD.highFlags |= JUPD_DET_INSERTED | JUPD_DET_SELECTED;
    if (mUseJeolGIFmodeCalls)
      mJeolSD.highFlags |= JUPD_GIF_MODE;
    mJeolSD.lowFlags = 0;
    mJeolSD.lowerFlags = 0;
    mJeolSD.highLowRatio = 2;
    mJeolSD.lowFlagsNew = 0;
    mJeolSD.highFlagsNew = mJeolSD.highFlags;

    // If updates are coming by event, set flags for high priority and low priority
    // updates depending on what is not available by event
    if (mUpdateByEvent) {
      mJeolSD.highFlagsNew = JUPD_CURRENT | JUPD_INTENSITY | JUPD_FOCUS;
      if (mJeolSD.usePLforIS )
        mJeolSD.highFlagsNew |= JUPD_IS;
      if (!mScreenByEvent && mReportsLargeScreen) {
        mJeolSD.lowFlagsNew = JUPD_SCREEN;
        if (mReportsSmallScreen)
          mJeolSD.lowFlagsNew |= JUPD_SMALL_SCREEN;
      }
      if (mHasOmegaFilter && !mSpectrumByEvent)
        mJeolSD.lowFlagsNew |= JUPD_SPECTRUM;
      if (mWinApp->GetShowRemoteControl())
        mJeolSD.lowFlagsNew |= JUPD_BEAM_STATE;
      if (mUseJeolGIFmodeCalls)
        mJeolSD.highFlagsNew |= JUPD_GIF_MODE;

      // Set lower flags as all the current flags minus all the new flags
      // The items coming by event will be checked once per full cycle of the rest
      mJeolSD.lowerFlags = mJeolSD.highFlags & 
        ~(mJeolSD.highFlagsNew | mJeolSD.lowFlagsNew);

      // Also set flags for initial update of values coming by event that are not
      // watched ordinarily without events
      if (!mHasNoAlpha)
        mJeolSD.highFlags |= JUPD_ALPHA;

      // Events are slow so we need to allow time to process the avalanche of events
      // on a mag change
      if (!mJeolPostMagDelay) {
        mJeolPostMagDelay = 1250;
        mJeolParams.postMagDelay = mJeolPostMagDelay;
      }
    }

    // Get the max loss and slit width if there is a filter
    if (mHasOmegaFilter) {
      double loss, minw, maxw;
      try {
        mPlugFuncs->GetFilterRanges(&loss, &minw, &maxw);
        mFiltParam->maxLoss = (float)loss;
        mFiltParam->minWidth = (float)minw;
        mFiltParam->maxWidth = (float)maxw;
      }
      catch (_com_error E) {
        startErr += 1;
      }
      startCall++;
      mFiltParam->matchIntensity = false;
      mFiltParam->matchPixel = false;
      mFiltParam->autoMag = false;
      mFiltParam->autoCamera = false;
      mFiltParam->adjustForSlitWidth = false;
      mFiltParam->positiveLossOnly = true;       // May need to be a property
    }

    if (startErr && startErr >= startCall - 1) {
      AfxMessageBox("Too many errors occurred on initial calls to microscope - "
        "it will not be available", MB_EXCLAME);
      sInitialized = false;
      mPlugFuncs->UninitializeScope();
      return 1;
    }

    if (startErr)
      AfxMessageBox("Errors occurred on initial calls to microscope", MB_EXCLAME);
    
    // No longer put it in Photo MDS mode if requested, just turn MDS off
    if (mJeolForceMDSmode < 0) {
      if (GetMDSMode() != JEOL_MDS_OFF) {
        SetMDSMode(JEOL_MDS_OFF);
        AfxMessageBox("MDS mode has been turned off to put the scope\n into a standard "
          "state for working with SerialEM.", MB_EXCLAME);
      }
    }
  }

  if (firstTime) {

    // Check for KV if warning set up, check for MAG/SAMAG mode unconditionally
    if (mWarnIfKVnotAt) {
      htval = GetHTValue();
      if (fabs(htval - mWarnIfKVnotAt) > 1.) {
        message.Format("WARNING: The scope is currently at %.0f kV\nand the program is "
          "calibrated for %.0f kV", htval, mWarnIfKVnotAt);
        AfxMessageBox(message, MB_EXCLAME);
      }
    }
    if (JEOLscope) {
      ScopeMutexAcquire("Initialize", true);
      try {
        if (!mPlugFuncs->GetSTEMMode()) {
          int mode = mPlugFuncs->GetImagingMode();
          if (((mode == JEOL_SAMAG_MODE && sJeolIndForMagMode == JEOL_MAG1_MODE) ||
            (mode == JEOL_MAG1_MODE && sJeolIndForMagMode == JEOL_SAMAG_MODE)) &&
            !mLowestSecondaryMag) {
              message.Format("WARNING: The scope is currently in %sMAG mode and the "
                "program properties are set up for %sMAG mode", mode == JEOL_MAG1_MODE ?
                "" : "SA", mode == JEOL_MAG1_MODE ? "SA" : "");
              AfxMessageBox(message, MB_EXCLAME);
          }
        }
      }
      catch (_com_error E) {
      }
      ScopeMutexRelease("Initialize");
    }

    if (FEIscope) {
      int plugVers = 0;
      int servVers = CBaseSocket::LookupTypeID(FEI_SOCK_ID) >= 0 ? 0 : -1;
      int needVers = FEISCOPE_NOCAM_VERSION;
      for (ind = 0; ind < mWinApp->GetActiveCamListSize(); ind++)
        if (camParam[activeList[ind]].FEItype)
          needVers = FEISCOPE_CAM_VERSION;
      if (!mPlugFuncs->GetPluginVersions || 
        !mPlugFuncs->GetPluginVersions(&plugVers, &servVers)) {
          mPluginVersion = plugVers;
          if (servVers >= 0 && servVers < plugVers)
            mPluginVersion = servVers;
          if (plugVers < FEISCOPE_PLUGIN_VERSION) {
            message.Format("The FEI scope plugin (FeiScopePlugin.dll) is version %d\n"
              "and should be upgraded to the current version (%d)\n\n"
              "This is usually accomplished by running install.bat\n"
              "in a current SerialEM installation package\n"
              "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_3-x-x...)",
              plugVers, FEISCOPE_PLUGIN_VERSION);
            AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
          }
          if (mPlugFuncs->GetPluginVersions && servVers >= 0 && servVers < needVers) {
            if (CBaseSocket::ServerIsRemote(FEI_SOCK_ID))
              message.Format("The scope server on the microscope computer "
                "(FEI-SEMserver.exe)\nis version %d and should be upgraded to the "
                "current version (%d)\n\n"
                "Copy the file from a current SerialEM installation package\n"
                "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_3-x-x...)\n" 
                "to the microscope computer, replacing the version there", 
                servVers, FEISCOPE_PLUGIN_VERSION);
            else
              message.Format("The scope server (FEI-SEMserver.exe) is version %d\n"
                "and should be upgraded to the current version (%d)\n\n"
                "Copy the file from a current SerialEM installation package\n"
                "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_3-x-x...)\n" 
                "and replace the version that you are now running", 
                servVers, FEISCOPE_PLUGIN_VERSION);
            AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
          }
      }
    }
  }

  if (!startErr && firstTime)
    SEMTrace('1', "Microscope Startup succeeded");
  firstTime = false;
  return startErr;
}

void CEMscope::StartUpdate()
{
  mUpdateID = ::SetTimer(NULL, 1, mUpdateInterval, UpdateProc);
  if (JEOLscope && sInitialized) {
    if (!mHasOmegaFilter && mWinApp->ScopeHasFilter())
      mJeolSD.JeolEFTEM = 0;
    mPlugFuncs->StartUpdates(mJeolUpdateSleep);
  }
}

void CEMscope::SuspendUpdate(int duration)
{
  sSuspendCount = (duration + mUpdateInterval - 1) / mUpdateInterval;
}

// Release the member pointers to refresh scope? Seems not to happen any more
void CEMscope::Release()
{
  if (!sInitialized)
    return;

  if (mPlugFuncs->UninitializeScope) {
    mPlugFuncs->UninitializeScope();
  }
  sInitialized = false;
}

BOOL CEMscope::GetInitialized()
{
  return sInitialized;
}

// Notification from low dose that continuous update is on
void CEMscope::SetLDContinuousUpdate(BOOL state)
{
  sLDcontinuous = state;
  if (JEOLscope) {
    if (!mUpdateByEvent && !mHasNoAlpha) {
      if (state)
        mJeolSD.highFlags |= JUPD_ALPHA;
      else
       mJeolSD.highFlags &= ~JUPD_ALPHA;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
//  THE UPDATE PROCEDURE
//  Which consists of a main routine and a set of Updatexxx sub-routines for pieces
//  that could be isolated, leaving mostly the heart of darkness mag/image shift handling
//  in the main routine
////////////////////////////////////////////////////////////////////////////////////
static VOID CALLBACK UpdateProc(
  HWND hwnd,     // handle of window for timer messages
  UINT uMsg,     // WM_TIMER message
  UINT_PTR idEvent,  // timer identifier
  DWORD dwTime   // current system time
)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mScope->ScopeUpdate(dwTime);
}
  
void CEMscope::ScopeUpdate(DWORD dwTime)
{
  int screenPos;
  int magIndex, camLenIndex, indOldIS, oldOffCalInd, newOffCalInd;
  int spotSize, probe, tmpMag, toCam, oldNeutralInd, subMode;
  double intensity, objective, rawIntensity, ISX, ISY, current, defocus;
  BOOL EFTEM, bReady, smallScreen, manageISonMagChg, gotoArea, blanked, restoreIS = false;
  int STEMmode, gunState = -1;
  bool newISseen = false;
  bool handleMagChange, temProbeChanged = false;
  int vacStatus = -1;
  double stageX, stageY, stageZ, delISX, delISY, axisISX, axisISY, tranISX, tranISY;
  double rawISX, rawISY, curMag, minDiff, minDiff2;
  MagTable *oldTab, *newTab;
  CString checkpoint = "start";
  BOOL needBlank, changedSTEM, changedJeolEFTEM = false;

  CShiftManager *shiftManager = mWinApp->mShiftManager;
  int lastMag;
  float alpha = -999.;
  double diffFocus = -999.;
  double updateStart = GetTickCount();
  //double wallStart = wallTime();

  if (mNoScope) {
    mWinApp->mScopeStatus.Update(0., mFakeMagIndex, 0., 0., 0., 0., 0., 0., 
      mFakeScreenPos == spUp, false, false, false, 0, 1, 0., 0., 0., 0, 0., 1, -1);
    return;
  }

  if (!sInitialized || mSelectedSTEM < 0)
    return;
  if (sSuspendCount > 0) {
    sSuspendCount--;
    return;
  }

  // Routine makes no direct calls to JEOL so doesn't need scope mutex, but does
  // need the data mutex to access thread data
  if (JEOLscope) {

    if (!mJeolSD.bDataIsGood)   // prevent a race condition
      return;
    SEMAcquireJeolDataMutex();
  } else { //FEI-like

    // Bail out if scope mutex can't be acquired
    if (!ScopeMutexAcquire("UpdateProc", false))
      return;
  }
  if (!AcquireMagMutex())
    return;
  if (mPlugFuncs->DoingUpdate) {
    mPlugFuncs->DoingUpdate(1);
    mGettingValuesFast = true;
  }

  try {

    // Get stage position and readiness
    UpdateStage(stageX, stageY, stageZ, bReady);
    checkpoint = "stage";

    // Get the mag and determine if it has changed
    lastMag = mLastMagIndex;
    if (JEOLscope) { 
      if (mJeolSD.magInvalid)
        magIndex = GetMagIndex();
      else
        magIndex = mJeolSD.magIndex;
      mLastCameraLength = mJeolSD.camLenValue;
      camLenIndex = mJeolSD.camLenIndex;
      STEMmode = mJeolSD.JeolSTEM ? 1 : 0;
      if (STEMmode && mNeedSTEMneutral && mLastSTEMmag)
        ResetSTEMneutral();
      mProbeMode = STEMmode ? 0 : 1;

    } else { // FEI-like

      // Get the image shift immediately before the mag and it will be known to be the
      // image shift for the mag that we then read
      mPlugFuncs->GetImageShift(&rawISX, &rawISY);
      STEMmode = (mWinApp->ScopeHasSTEM() && mPlugFuncs->GetSTEMMode() != 0) ? 1 : 0;
      if (mUpdateBeamBlank > 0 && mPlugFuncs->GetBeamBlank) {
        blanked = mPlugFuncs->GetBeamBlank();
        if ((blanked ? 1 : 0) != (sBeamBlanked ? 1 : 0))
          SEMTrace('B', "Update saw beam blank %s", blanked ? "ON" : "OFF");
        sBeamBlanked = blanked;
      }

      // Get probe mode or assign proper value based on STEM or not
      if (FEIscope) {
        PLUGSCOPE_GET(ProbeMode, probe, 1);
        probe = probe ? 1 : 0;
      } else {
        probe = STEMmode ? 0 : 1;
      }
      if (STEMmode) {
        PLUGSCOPE_GET(STEMMagnification, curMag, 1);

        // The scope changes from nano to microprobe before leaving STEM
        // Thus, need to wait for over a second after first seeing a probe mode change
        if (probe != mProbeMode) {
          if (sProbeChangedTime < 0.) {
            SEMTrace('g', "ScopeUpdate: probe mode change seen, wait for a bit");
            sProbeChangedTime = GetTickCount();
          } else if (SEMTickInterval(sProbeChangedTime) > mProbeChangeWait) {
            SEMTrace('g', "ScopeUpdate: probe mode change %d to %d accepted", 
              mProbeMode, probe);
            mProbeMode = probe;
            sProbeChangedTime = -1.;
          }
        } else {
          sProbeChangedTime = -1.;
        }

        magIndex = LookupSTEMmagFEI(curMag, mProbeMode, minDiff);

        // But if it has changed, see if the mag is unique to the new mode
        if (sProbeChangedTime >= 0.) {
          tmpMag = LookupSTEMmagFEI(curMag, probe, minDiff2);
          if (minDiff > 0. && minDiff2 == 0.) {
            SEMTrace('g', "ScopeUpdate: probe mode change %d to %d accepted due to "
              "unique mag", mProbeMode, probe);
            mProbeMode = probe;
            sProbeChangedTime = -1.;
            magIndex = tmpMag;
          } else {

            // If it is not a confirmed changed, better keep the old mag and IS
            magIndex = lastMag;
            rawISX = mLastISX;
            rawISY = mLastISY;
          }
        }
        STEMmode += mProbeMode;
        mWinApp->mSTEMcontrol.UpdateSTEMstate(mProbeMode);
      } else {

        // TEM mode.  If the probe mode changed, set flag and just restore the raw IS 
        // values
        PLUGSCOPE_GET(MagnificationIndex, magIndex, 1);
        if (probe != mProbeMode) {
          temProbeChanged = true;
          rawISX = mLastISX;
          rawISY = mLastISY;
          mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostProbeDelay);
        }
        mProbeMode = probe;
        sProbeChangedTime = -1.;
      }
      if (lastMag && !magIndex && mWinApp->ScopeHasSTEM()) {
        if (sDiffSeenTime < 0.) {
          SEMTrace('g', "ScopeUpdate: 0 mag index seen, wait for a bit");
          sDiffSeenTime = GetTickCount();
        } else if (SEMTickInterval(sDiffSeenTime) > mDiffractOrSTEMwait) {
          sDiffSeenTime = -1.;
        }
        if (sDiffSeenTime >= 0) {
          magIndex = lastMag;
          rawISX = mLastISX;
          rawISY = mLastISY;
        }
      }

      // For Hitachi, the old mag can show up in update after an internal mag change, so
      // if the old mag matches and the change was recent enough, switch to new mag that
      // was set as last mag index
      if (HitachiScope && lastMag && magIndex && lastMag != magIndex && 
        magIndex == mInternalPrevMag && mInternalMagTime >= 0. && 
        SEMTickInterval(mInternalMagTime) < mHitachiMagChgSeeTime) {
          SEMTrace('g', "Update: Still see mag %d after %.0f msec, switch to mag %d",
            magIndex, SEMTickInterval(mInternalMagTime), lastMag);
          magIndex = lastMag;
      }

      if (magIndex != lastMag || rawISX != mLastISX || rawISY != mLastISY)
        SEMTrace('i', "Update: lastMag %d new mag %d last IS %.3f %.3f  rawIS %.3f %.3f",
        lastMag, magIndex, mLastISX, mLastISY, rawISX, rawISY);

      // This flag is solely to indicate on the next round that previous rather than
      // last IS should be used for transfers; but do not do that when crossing boundary
      if (BothLMorNotLM(lastMag, mLastSTEMmode != 0, magIndex, STEMmode != 0) &&
        !(HitachiScope && mInternalMagTime >= 0. && 
        SEMTickInterval(mInternalMagTime) < 2 * mHitachiMagChgSeeTime && 
        !BothLMorNotLM(mLastMagIndex, false, mInternalPrevMag, false)))
        newISseen = rawISX != mLastISX || rawISY != mLastISY;
      else 
        mLastMagModeChgTime = GetTickCount();

      // Diffraction
      if (!magIndex) {
        PLUGSCOPE_GET(CameraLength, mLastCameraLength, 1);
        PLUGSCOPE_GET(CameraLengthIndex, camLenIndex, 1);
        if (FEIscope) {
          PLUGSCOPE_GET(SubMode, subMode, 1);
          if (subMode == psmLAD)
             camLenIndex += LAD_INDEX_BASE;
        }
        if (sLDcontinuous)
          PLUGSCOPE_GET(AbsFocus, diffFocus, 1.);
      }
    }
    checkpoint = "mag";

    // Is it JEOL EFTEM with a change of state?  Get the neutral index fixed now, and set
    // up old and new indices and IS offset indices, and camera we are going to
    oldNeutralInd = mNeutralIndex;
    oldOffCalInd = newOffCalInd = (mWinApp->GetEFTEMMode() ? 1 : 0);
    toCam = mWinApp->GetCurrentCamera();
    if (JEOLscope && !mHasOmegaFilter && mJeolSD.JeolEFTEM >= 0) {
      updateEFTEMSpectroscopy(EFTEM);
      changedJeolEFTEM = mNeutralIndex != (mWinApp->GetEFTEMMode() ? 1 : 0);
      newOffCalInd = mNeutralIndex;
      if (changedJeolEFTEM)
        toCam = mActiveCamList[B3DMAX(0, mNeutralIndex ? mFiltParam->firstGIFCamera : 
        mFiltParam->firstRegularCamera)];
    }

    delISX = 0.;
    delISY = 0.;
    handleMagChange = (magIndex != lastMag || changedJeolEFTEM) && 
      mCalNeutralStartMag < 0 && !sScanningMags;
    if (handleMagChange) {
      mMagChanged = true;

      // Record time of user change to prevent images from being taken too soon
      SetLastNormalization(GetTickCount());
      HandleNewMag(magIndex);
      indOldIS = 0;

      // See if an image shift adjustment is needed
      newTab = mWinApp->GetMagTable() + magIndex;
      oldTab = mWinApp->GetMagTable() + lastMag;
      delISX = newTab->neutralISX[mNeutralIndex] - oldTab->neutralISX[oldNeutralInd];
      delISY = newTab->neutralISY[mNeutralIndex] - oldTab->neutralISY[oldNeutralInd];
      if (mApplyISoffset) {
        delISX += newTab->calOffsetISX[newOffCalInd] - oldTab->calOffsetISX[oldOffCalInd];
        delISY += newTab->calOffsetISY[newOffCalInd] - oldTab->calOffsetISY[oldOffCalInd];
      }
    }

    if (handleMagChange || temProbeChanged) {

      // Manage IS across a mag change either if staying in LM or M, or if the offsets
      // are turned on by user
      manageISonMagChg = !STEMmode && lastMag && magIndex && 
        (!HitachiScope || mLastMagModeChgTime < 0 || temProbeChanged ||
        SEMTickInterval(mLastMagModeChgTime) > mHitachiModeChgDelay) &&
        (BothLMorNotLM(lastMag, mLastSTEMmode != 0, magIndex, STEMmode != 0) || 
        GetApplyISoffset() || changedJeolEFTEM);

      if (JEOLscope) {  
        SEMSetStageTimeout();

        // First see if need to save the image shift for the old mag for later restore
        if (!changedJeolEFTEM && SaveOrRestoreIS(lastMag, magIndex)) {
          indOldIS = LookupRingIS(lastMag, changedJeolEFTEM);
          if (indOldIS >= 0) {
            mSavedISX = mJeolSD.ringISX[indOldIS];
            mSavedISY = mJeolSD.ringISY[indOldIS];
            mMagIndSavedIS = lastMag;
            SEMTrace('i', "Update saved raw IS %.3f %.3f, going from %d to %d", 
              mSavedISX, mSavedISY, lastMag, magIndex);
          }
        }

        // For JEOL, need to find the old image shift before trying to translate it
        // across a non-congruent boundary
        if (manageISonMagChg) {
          ISX = mJeolSD.ISX;
          ISY = mJeolSD.ISY;
          indOldIS = LookupRingIS(lastMag, changedJeolEFTEM);
          // However it is gotten, adjust the IS stored on the ring
          if (indOldIS >= 0) {
            ISX = mJeolSD.ringISX[indOldIS] - oldTab->neutralISX[oldNeutralInd];
            ISY = mJeolSD.ringISY[indOldIS] - oldTab->neutralISY[oldNeutralInd]; 
            if (mApplyISoffset) {
              ISX -= oldTab->calOffsetISX[oldOffCalInd];
              ISY -= oldTab->calOffsetISY[oldOffCalInd];
            }
          } 
        }
      } else {  // FEI-like

        if (HitachiScope && mISwasNewLastUpdate) {
          SEMTrace('i', "Using previous IS because last was new on last update");
          mLastISX = mPreviousISX;
          mLastISY = mPreviousISY;
        }

        // Tecnai: save IS here if appropriate
        // TODO: IS THIS A BUG?  SHOULDN'T IT SAVE LAST VALUES?
        if (!STEMmode && SaveOrRestoreIS(lastMag, magIndex)) {
          mSavedISX = rawISX;
          mSavedISY = rawISY;
          mMagIndSavedIS = lastMag;
          SEMTrace('i', "Update saved raw IS %.3f %.3f, going from %d to %d", 
            mSavedISX, mSavedISY, lastMag, magIndex);
        }
      }  

      // Test for whether restoring, get the raw IS back for this mag
      restoreIS = temProbeChanged || (!STEMmode && !changedJeolEFTEM &&
        AssessRestoreIS(lastMag, magIndex, rawISX,rawISY));

      // If the mag change is across boundary between non-congruent shifts,
      // Get the real image shift including axis offset, excluding neutral and offset
      if (manageISonMagChg && (changedJeolEFTEM ||
        !shiftManager->CanTransferIS(lastMag, magIndex, STEMmode != 0))) {
        if (!JEOLscope) {
          ISX = mLastISX - (oldTab->neutralISX[mNeutralIndex] + oldTab->offsetISX);
          ISY = mLastISY - (oldTab->neutralISY[mNeutralIndex] + oldTab->offsetISY);
        }

        // convert to specimen coords and back to new IS coords at new mag and modify del
        shiftManager->TransferGeneralIS(lastMag, ISX, ISY, magIndex, tranISX, tranISY,
          toCam);
        delISX += tranISX - ISX;
        delISY += tranISY - ISY;
        SEMTrace('i', "Update: transfer IS %.3f %.3f  to %.3f %.3f  total del %.3f %.3f"
          "  toCam %d from %d" ,
          ISX, ISY, tranISX, tranISY, delISX, delISY, toCam, mWinApp->GetCurrentCamera());
      }

      // Do it if adjustment is needed or mag changes reset IS, as long as
      // we should be managing across this change and an IS was found
      if ((delISX || delISY || MagChgResetsIS(magIndex) || temProbeChanged) && 
        ((manageISonMagChg && indOldIS >= 0) || restoreIS)) {
        if (JEOLscope) {
          if (!restoreIS) {
            rawISX = mJeolSD.ringISX[indOldIS] + delISX;
            rawISY = mJeolSD.ringISY[indOldIS] + delISY;
          }

          // Post the image shift; modify all entries in ring at this mag to
          // point to this image shift
          ScopeMutexAcquire("UpdateProc", true);
          mPlugFuncs->UseMagInNextSetISXY(magIndex);
          mPlugFuncs->SetImageShift(rawISX, rawISY);
          ScopeMutexRelease("UpdateProc");

        } else { // FEI-like

          // Tecnai: change the raw value and use it to set the shift
          // We can't read back the value because it may not be right for current mag
          if (!restoreIS) {
            rawISX = mLastISX + delISX;
            rawISY = mLastISY + delISY;
          }

            
          // Probably need to do beam shift too, so update the mage variable and get the
          // new tilt axis offset and call the regular IS routine
          mLastMagIndex = magIndex;
          GetTiltAxisIS(axisISX, axisISY);
          SetImageShift(rawISX - axisISX, rawISY - axisISY);
          SEMTrace('i',"Update asserted last %.3f %.3f + del %.3f %.3f new raw %.3f %.3f",
            mLastISX, mLastISY, delISX, delISY, rawISX, rawISY);
        }
      }
    }

    // Change the last mag index then get the new tilt axis etc offset
    changedSTEM = STEMmode != mLastSTEMmode;
    if (changedSTEM && !STEMmode)
      mWinApp->mCamera->OutOfSTEMUpdate();
    mLastMagIndex = magIndex;
    mLastSTEMmode = STEMmode;
    GetTiltAxisIS(axisISX, axisISY);

    // Call this routine in case post-action tilt changed mag
    if (mHandledMag != magIndex)
       HandleNewMag(magIndex);
    ReleaseMagMutex();
    checkpoint = "image shift/mag handling";

    if (JEOLscope) {
      ISX = mJeolSD.ISX - axisISX;
      ISY = mJeolSD.ISY - axisISY;
    } else { //FEI
      ISX = rawISX - axisISX;
      ISY = rawISY - axisISY;
      mPreviousISX = mLastISX;
      mPreviousISY = mLastISY;
      mLastISX = rawISX;
      mLastISY = rawISY;
    }
    mISwasNewLastUpdate = newISseen;
    
    // Get current, screen pos, focus
    UpdateScreenBeamFocus(STEMmode, screenPos, smallScreen, spotSize, rawIntensity, 
      current, defocus, objective, alpha);
    checkpoint = "defocus";

    // Handle adjusting focus on a probe mode change, record first focus of mode
    if (temProbeChanged) {
      if (mAdjustFocusForProbe && mFirstFocusForProbe > EXTRA_VALUE_TEST) {
        defocus += mLastFocusInUpdate - mFirstFocusForProbe;
        PLUGSCOPE_SET(Defocus, defocus * 1.e-6);
      }
      mFirstFocusForProbe = defocus;
      if (FEIscope)
        mPlugFuncs->GetBeamShift(&mFirstBeamXforProbe, &mFirstBeamYforProbe);
    }
    mLastFocusInUpdate = defocus;

    // Determine reversals of tilting
    // First initialize the values
    if (sReversalTilt > 90.) {
      sReversalTilt = sTiltAngle;
      sExtremeTilt = sTiltAngle;
    }
    ManageTiltReversalVars();

    // Do vacuum gauge check on FEI
    UpdateGauges(vacStatus);
    checkpoint = "vacuum";

    // Take care of screen-dependent changes in low dose and update low dose panel
    needBlank = NeedBeamBlanking(screenPos, STEMmode != 0, gotoArea);
    UpdateLowDose(screenPos, needBlank, gotoArea, magIndex, camLenIndex, diffFocus, 
      alpha, spotSize, rawIntensity, ISX, ISY);

    checkpoint = "low dose";

    // If screen changed, tell an AMT camera to change its blanking
    // If screen changed or STEM changed, manage blanking for shutterless camera when not
    // in low dose and STEM-dependent blanking
    if (mLastScreen != screenPos || changedSTEM) {
      if (mLastScreen != screenPos)
        mWinApp->mCamera->SetAMTblanking(screenPos == spUp);
      if (!mLowDoseMode || changedSTEM)
        BlankBeam(needBlank);
    }

    intensity = GetC2Percent(spotSize, rawIntensity);

    // Determine EFTEM mode and check spectroscopy on JEOL
    updateEFTEMSpectroscopy(EFTEM);

    if (mWinApp->GetShowRemoteControl()) {
      if (JEOLscope)
        gunState = mJeolSD.valveOrFilament;
      else if (mPlugFuncs->GetGunValve)
        gunState = mPlugFuncs->GetGunValve();
      mWinApp->mRemoteControl.Update(magIndex, spotSize, rawIntensity, mProbeMode, 
        gunState, STEMmode);
    }
    mWinApp->mScopeStatus.Update(current, magIndex, defocus, ISX, ISY, stageX, stageY,
      stageZ, screenPos == spUp, smallScreen != 0, sBeamBlanked, EFTEM, STEMmode,spotSize, 
      rawIntensity, intensity, objective, vacStatus, mLastCameraLength, mProbeMode, 
      gunState);

    if (mWinApp->mStageMoveTool)
      mWinApp->mStageMoveTool->UpdateStage(stageX, stageY);
    checkpoint = "status update";

    // Update the last mags/focus seen in a mode and change mode if indicated
    UpdateLastMagEftemStem(magIndex, defocus, screenPos, EFTEM, STEMmode);    

    mLastLowDose = mLowDoseMode;
    mLastScreen = screenPos;
    mErrCount = 0;
    mDisconnected = false;
  }
  catch (_com_error E) {
     CString message;

     // Disable trying to get vaccuum if it fails once
     if (checkpoint == "defocus")
       mNumGauges = 0;
    
    // Report first error and then multiple errors every 10 seconds
    if (dwTime  > 10000 + mLastReport && !mReportingErr) {
      mReportingErr = true;
      if (mPlugFuncs->ScopeIsDisconnected && 
        mPlugFuncs->ScopeIsDisconnected()) {
          if (!mDisconnected) {
            message = "Connection to microscope has been lost";
            if (FEIscope)
              message += "\n\nIt can probably be restored if you restart FEI-SEMserver";
            SEMMessageBox(message);
          }
          mDisconnected = true;

      } else if (!mErrCount) {
        message = "getting scope update after " + checkpoint + " ";
        SEMReportCOMError(E, message);
      } else {
        message.Format("getting scope update %d times after %s ", mErrCount, 
          LPCTSTR(checkpoint));
        SEMReportCOMError(E, message);
        mErrCount = 0;
      }
      mReportingErr = false;
      mLastReport = dwTime;
    } else
      mErrCount++;
  }

  // When count is up, check for autosaving of settings, navigator
  if (mAutosaveCount++ > AUTOSAVE_INTERVAL_MIN * 60000 / mUpdateInterval) {
    mAutosaveCount = 0;
    mWinApp->mDocWnd->AutoSaveFiles();
  }

  if (mCheckFreeK2RefCount++ > 30000 / mUpdateInterval) {
    mCheckFreeK2RefCount = 0;
    mWinApp->mCamera->CheckAndFreeK2References(false);
  }

  // Check if valves should be closed
  if (sCloseValvesInterval > 0. && 
    SEMTickInterval(sCloseValvesStart) > sCloseValvesInterval) {
    SetColumnValvesOpen(false);
    sCloseValvesInterval = 0.;
  }

  if (mPlugFuncs->DoingUpdate) {
     mPlugFuncs->DoingUpdate(0);
     mGettingValuesFast = false;
  }
  if (JEOLscope) 
    SEMReleaseJeolDataMutex();
  else
    ScopeMutexRelease("UpdateProc"); 
  //SEMTrace('H', "Update time %f (ticks)  %.1f (wall)", SEMTickInterval(updateStart),
    //1000.*(wallTime() - wallStart));
}

// UPDATE SUB-ROUTINES CALLED ONLY FROM INSIDE ScopeUpdate

// Get tilt angle and stage position and readiness, update panels for blanking
void CEMscope::UpdateStage(double &stageX, double &stageY, double &stageZ, BOOL &bReady)
{
  float backX, backY;
  if (JEOLscope) {
    sTiltAngle = mJeolSD.tiltAngle;
    stageX = mJeolSD.stageX;
    stageY = mJeolSD.stageY;
    stageZ = mJeolSD.stageZ;
    bReady = mJeolSD.stageReady;

  } else {  // Plugin
    mPlugFuncs->GetStagePosition(&stageX, &stageY, &stageZ);
    stageX *= 1.e6;
    stageY *= 1.e6;
    stageZ *= 1.e6;
    sTiltAngle = mPlugFuncs->GetTiltAngle() / DTOR;
    bReady = mPlugFuncs->GetStageStatus() == 0;
  }

  // 12/15/04: just use internal blanked flag instead of getting from scope
  GetValidXYbacklash(stageX, stageY, backX, backY);
  mWinApp->mTiltWindow.TiltUpdate(sTiltAngle, bReady);
  mWinApp->mLowDoseDlg.BlankingUpdate(sBeamBlanked);
  if (mWinApp->ScopeHasSTEM())
    mWinApp->mSTEMcontrol.BlankingUpdate(sBeamBlanked);
}

// Get the other items for the scope status window
// Screen position, intensity, spotsize, image beam shift, and current
void CEMscope::UpdateScreenBeamFocus(int STEMmode, int &screenPos, int &smallScreen, 
                                     int &spotSize, double &rawIntensity, double &current, 
                                     double &defocus, double &objective, float &alpha)
{
  if (JEOLscope) {
    screenPos = spJeolToFEI[mJeolSD.screenPos];
    rawIntensity = mJeolSD.rawIntensity;
    spotSize = mJeolSD.spotSize;
    current = mJeolSD.current;
    smallScreen = mJeolSD.smallScreen;
    if (sLDcontinuous)
      alpha = (float)mJeolSD.alpha;

  } else {  // FEI-like
    screenPos = GetScreenPos();
    if (mUseIllumAreaForC2) {
      PLUGSCOPE_GET(IlluminatedArea, rawIntensity, 1.e4);
      rawIntensity = IllumAreaToIntensity(rawIntensity);
    } else
      PLUGSCOPE_GET(Intensity, rawIntensity, 1.);
    PLUGSCOPE_GET(SpotSize, spotSize, 1);
    // TEMP FOR BAD VM!
    if (!sCheckPosOnScreenError)
      PLUGSCOPE_GET(ScreenCurrent, current, 1.e9);
    smallScreen = SmallScreenIn();
  }
  if (smallScreen)
    current *= mSmallScreenFactor;
  current *= mScreenCurrentFactor;

  // Get the defocus and objective excitation
  PLUGSCOPE_GET(Defocus, defocus, 1.e6);
  PLUGSCOPE_GET(ObjectiveStrength, objective, 100.);
}

// Determine vacuum status for gauges for FEI
void CEMscope::UpdateGauges(int &vacStatus)
{
  if (mPlugFuncs->GetGaugePressure) {
    int gaugeStatus, gauge, statIndex;
    double gaugePressure;

    if (mNumGauges) {

      for (gauge = 0; gauge < mNumGauges; gauge++) {

        // Get status of one gauge on the list by its index
        mPlugFuncs->GetGaugePressure((LPCTSTR)mGaugeNames[gauge], &gaugeStatus, 
          &gaugePressure);
        mLastGaugeStatus = gaugeStatus;
        mLastPressure = gaugePressure;
        statIndex = 0;
        if (gaugeStatus == gsInvalid || gaugeStatus == gsOverflow || 
          gaugePressure > mRedThresh[gauge])
          statIndex = 2;
        else if (gaugeStatus != gsUnderflow && 
          gaugePressure > mYellowThresh[gauge])
          statIndex = 1;

        if (mVacCount == 0)
          SEMTrace('V', "Gauge %d, status %d,  pressure %f", gauge, gaugeStatus, 
          gaugePressure);

        // Report maximum of all status values
        if (vacStatus < statIndex)
          vacStatus = statIndex;
      }
      if (mVacCount++ >= 1000 / mUpdateInterval)
        mVacCount = 0;
}
  }
}

// Manage things when screen changes in low dose, update low dose panel
void CEMscope::UpdateLowDose(int screenPos, BOOL needBlank, BOOL gotoArea, int magIndex,
                             int camLenIndex, double diffFocus, float alpha, 
                             int &spotSize, double &rawIntensity, double &ISX,double &ISY)
{
  double delISX, delISY;
  if (mLowDoseMode) {

    // If low dose mode or screen position has changed:
    if (mLastLowDose != mLowDoseMode || mLastScreen != screenPos) {

      // First apply a blank if needed, regardless of screen            
      if (needBlank)
        BlankBeam(true);

      // If screen is down, next set the desired area, then unblank if needed
      if (screenPos == spDown) {
        if (gotoArea)
          GotoLowDoseArea(mLowDoseDownArea);
        if (!needBlank)
          BlankBeam(false);

        // For omega filter, assert the filter settings
        if (GetHasOmegaFilter())
          mWinApp->mCamera->SetupFilter();
      }

      // Refresh the spot size and intensity
      spotSize = FastSpotSize();
      rawIntensity = FastIntensity();
    }

    // After those potential changes, send update to low dose window
    mWinApp->mLowDoseDlg.ScopeUpdate(magIndex, spotSize, rawIntensity, ISX, ISY,
      screenPos == spDown, mLowDoseSetArea, camLenIndex, diffFocus, alpha, mProbeMode);

    // Subtract the current image shift from the IS sent to scope window
    if (mLowDoseSetArea >= 0 && (!mUsePiezoForLDaxis || !mLowDoseSetArea)) {
      LowDoseParams *ldParams = mWinApp->GetLowDoseParams() + mLowDoseSetArea;
      delISX = ldParams->ISX;
      delISY = ldParams->ISY;
      if (ldParams->magIndex != magIndex)
        mWinApp->mShiftManager->TransferGeneralIS(ldParams->magIndex, delISX, delISY, 
        magIndex, delISX, delISY);
      ISX -= mLastLDpolarity * delISX;
      ISY -= mLastLDpolarity * delISY;
    }
  }
}

// Update filter settings in parameter and panel if not in spectroscopy
void CEMscope::updateEFTEMSpectroscopy(BOOL &EFTEM)
{
  EFTEM = false;
  if (JEOLscope) {
    if (mHasOmegaFilter) {
      if (!mJeolSD.spectroscopy) {

        // Reassert filter setting when coming out of spectroscopy
        if (mLastSpectroscopy) {
          mWinApp->mCamera->SetupFilter();

        } else if (!mWinApp->mCamera->GetIgnoreFilterDiffs() && 
          (fabs(mFiltParam->slitWidth - mJeolSD.slitWidth) > 0.1 ||
          (mFiltParam->slitIn ? 1 : 0) != (mJeolSD.slitIn ? 1 : 0))) {
            mFiltParam->slitIn = mJeolSD.slitIn;
            mFiltParam->slitWidth = mJeolSD.slitWidth;
            mWinApp->mFilterControl.UpdateSettings();
        }
      }
      mLastSpectroscopy = mJeolSD.spectroscopy;
      EFTEM = false;
    } else if (mJeolSD.JeolEFTEM >= 0) {
      EFTEM = mJeolSD.JeolEFTEM > 0;
      mNeutralIndex = EFTEM ? 1 : 0;
    }

  } else { // FEI-like
    int lvalue;
    PLUGSCOPE_GET(EFTEMMode, lvalue, 1);
    EFTEM = (lvalue == lpEFTEM);
  }
}

void CEMscope::UpdateLastMagEftemStem(int magIndex, double defocus, int screenPos, 
                                      BOOL EFTEM, int STEMmode)
{
  BOOL needed;

  // Record last real mag seen in a mode as long as mode matches internal state
  if (EFTEM && mWinApp->GetEFTEMMode() && magIndex) {
    if (mLastEFTEMmag != magIndex)
      SEMTrace('g', "ScopeUpdate: in EFTEM, changing last EFTEM mag %d to %d",
      mLastEFTEMmag, magIndex);
    mLastEFTEMmag = magIndex;
    mLastRegularFocus = defocus;

  } else if (!EFTEM && 
    (!mWinApp->GetEFTEMMode() || (screenPos == spDown && mFiltParam->autoMag))
    && !STEMmode &&
    (!mWinApp->GetSTEMMode() || (screenPos == spDown && mWinApp->GetScreenSwitchSTEM())) 
    && magIndex) {
      if (mLastRegularMag != magIndex)
        SEMTrace('g', "ScopeUpdate: not in EFTEM or STEM, changing last regular mag %d "
        "to %d", mLastRegularMag, magIndex);
      mLastRegularMag = magIndex;
      mLastRegularFocus = defocus;
  } else if (STEMmode && mWinApp->GetSTEMMode() && magIndex) {
    if (mLastSTEMmag != magIndex)
      SEMTrace('g', "ScopeUpdate: in STEM, changing last STEM mag %d to %d",
      mLastSTEMmag, magIndex);
    mLastSTEMmag = magIndex;
    mLastSTEMfocus = defocus;
  }

  // Deal with EFTEM and STEM properly as long as screen is not busy
  if (ScreenBusy() <= 0) { // && GetCanControlEFTEM()) {

    // If not in mode and somehow user gets into EFTEM, better set the mode
    if (EFTEM && !mWinApp->GetEFTEMMode())
      mWinApp->SetEFTEMMode(true);

    // If user turned off mode through Tecnai, turn it off internally
    else if (!EFTEM && mSelectedEFTEM && mWinApp->GetEFTEMMode())
      mWinApp->SetEFTEMMode(false);

    // otherwise determine needed state of lenses: if screen is up or screen is
    // down and auto mag change disabled, it needs to be on
    else if (mWinApp->GetEFTEMMode() && mCanControlEFTEM) {
      needed = (screenPos == spUp || !mFiltParam->autoMag ||
        (mWinApp->mAutocenDlg && mWinApp->mAutocenDlg->m_bSetState));
      if (needed && !EFTEM || !needed && EFTEM) 
        SetEFTEM(needed);
    }

    // If user turns on STEM, set it internally
    if (STEMmode && !mWinApp->GetSTEMMode()) {
      SEMTrace('g', "ScopeUpdate: scope in STEM, program not, setting program mode");
      mWinApp->SetSTEMMode(true);
    }

    // If user turns it off and it is on in all respects, turn in off
    else if (!STEMmode && mSelectedSTEM > 0 && mWinApp->GetSTEMMode()) {
      SEMTrace('g', "ScopeUpdate: scope not in STEM, program is, setting program mode");
      mWinApp->SetSTEMMode(false);
    }

    // otherwise if the mode is on, set the scope depending on the screen
    else if (mWinApp->GetSTEMMode() && mSelectedSTEM >= 0) {
      needed = screenPos == spUp || !mWinApp->GetScreenSwitchSTEM();
      if (needed && !STEMmode || !needed && STEMmode ) {
        SEMTrace('g', "ScopeUpdate: program in STEM, setting scope %d to match screen; pos %d (unk %d)",
          needed ? 1 : 0, screenPos, spUnknown);
        SetSTEM(needed);
      }
    }
  }
}

// END OF UPDATE SUB-FUNCTIONS
//////////////////////////////////////

// Takes care of the reversal, extreme and direction variables given current tilt angle
static void ManageTiltReversalVars()
{
  if (sTiltDirection * (sTiltAngle - sExtremeTilt) > 0) 

    // Moving in the same direction: replace the extreme value
    sExtremeTilt = sTiltAngle;
  else if (sTiltDirection * (sTiltAngle - sExtremeTilt) < -sTiltReversalThresh) {

    // Moving in opposite direction by more than threshold: reverse,
    // make the reversal be the last extreme, and replace the extreme
    sTiltDirection = -sTiltDirection;
    sReversalTilt = sExtremeTilt;
    sExtremeTilt = sTiltAngle;
  }
}

// Return tilt angle
double CEMscope::GetTiltAngle(BOOL forceGet)
{
  int getFast = 0;
  bool wasGettingFast = mGettingValuesFast;
  if (!sInitialized)
    return 0.0;

  // Plugin uses state value only if getting fast is set
  if (JEOLscope) {
    if ((mJeolSD.eventDataIsGood && !forceGet) || mGettingValuesFast) {
      if (!wasGettingFast)
        GetValuesFast(1);
      getFast = 1;
    }
    while (SEMCheckStageTimeout())
      Sleep(200);
  }

  if (!getFast)
    ScopeMutexAcquire("GetTiltAngle",true);

  try {
    sTiltAngle = mPlugFuncs->GetTiltAngle() / DTOR;
    if (getFast && !wasGettingFast)
      GetValuesFast(-1);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting tilt angle "));    
  }
  if (!getFast)
    ScopeMutexRelease("GetTiltAngle");
  return sTiltAngle;
}

// Used to get tilt angle fast for JEOL by using update value, but setting fast won't be
// undone by the above
double CEMscope::FastTiltAngle()
{
  FAST_GET(double, GetTiltAngle);
}

double CEMscope::GetReversalTilt()
{
  return sReversalTilt;
}


float CEMscope::GetIncrement()
{
  if (!m_bCosineTilt)
    return mIncrement;
  return (float)cos(GetTiltAngle() * DTOR) * mIncrement;
}


void CEMscope::TiltUp()
{
  TiltTo(GetTiltAngle() + GetIncrement());
}

void CEMscope::TiltDown()
{
  TiltTo(GetTiltAngle() - GetIncrement());
}

void CEMscope::TiltTo(double inVal)
{
  StageMoveInfo info;
  info.alpha = inVal;
  info.axisBits = axisA;
  MoveStage(info, false);
}

DWORD CEMscope::GetLastTiltTime() 
{
  // Make sure the time is unloaded after the movement but trust JEOL state
  StageBusy(-1);
  return mLastTiltTime;
}

DWORD CEMscope::GetLastStageTime() 
{
  // Make sure the time is unloaded after the movement
  StageBusy(-1);
  return mLastStageTime;
}

BOOL CEMscope::MoveStage(StageMoveInfo info, BOOL doBacklash, BOOL useSpeed, 
  BOOL inBackground, BOOL doRelax)
{
  // This call is not supposed to happen unless stage is ready, so don't wait long
  // 5/17/11: Hah.  Nothing is too long for a JEOL...  increased from 4000 to 12000
  DWORD waitTime = 12000;
  float signedLimit = (info.alpha >= 0. ? 1.f : -1.f) * mMaxTiltAngle;
  if (JEOLscope) {
    if (SEMCheckStageTimeout())
      waitTime += mJeolSD.postMagStageDelay;
  }
  if (WaitForStageReady(waitTime)) {
    SEMMessageBox(_T("Stage not ready"));
    return false;
  }
  if (inBackground && FEIscope && SEMNumFEIChannels() < 4) {
    SEMMessageBox("The stage cannot be moved in the background.\n"
      "You must have the property BackgroundSocketToFEI set to open\n"
      "the socket connection for background stage movement when\nthe program starts.");
    return false;
  }
  
  mMoveInfo = info;
  mMoveInfo.JeolSD = &mJeolSD;
  mMoveInfo.plugFuncs = mPlugFuncs;
  mMoveInfo.doBacklash = doBacklash;
  mMoveInfo.doRelax = doRelax && (mMoveInfo.relaxX != 0. || mMoveInfo.relaxY != 0.);
  if (!doBacklash)
    mMoveInfo.backX = mMoveInfo.backY = mMoveInfo.backZ = mMoveInfo.backAlpha = 0.;
  if (!doRelax)
    mMoveInfo.relaxX = mMoveInfo.relaxY = 0.;
  mMoveInfo.useSpeed = useSpeed && mPlugFuncs->SetStagePositionExtra;
  if (!useSpeed)
    mMoveInfo.speed = 1.;
  mMoveInfo.inBackground = inBackground;
  mRequestedStageX = (float)info.x;
  mRequestedStageY = (float)info.y;
  mStageAtLastPos = false;

  if (info.axisBits & axisA) {

    // Limit both the final angle and the backlash angle
    if (fabs(info.alpha) > mMaxTiltAngle)
      mMoveInfo.alpha = signedLimit;
    if (fabs(mMoveInfo.alpha + mMoveInfo.backAlpha) > mMaxTiltAngle)
      mMoveInfo.backAlpha = (float)(signedLimit - mMoveInfo.alpha);

    // Capture tilt change here
    mLastTiltChange = fabs(mMoveInfo.alpha - GetTiltAngle());
  }

  if (JEOLscope) {
    //TEMCON selection
    // Update is too slow to disable the buttons so do it explicitly
    // Also set ready flag false since thread may hang on timeout
    USE_DATA_MUTEX(mJeolSD.stageReady = false);
    mWinApp->mTiltWindow.TiltUpdate(mJeolSD.tiltAngle, false);
  }

  mLastStageCheckTime = GetTickCount();
  mStageThread = AfxBeginThread(StageMoveProc, &mMoveInfo,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("StageMoveProc created with ID 0x%0x\n",mStageThread->m_nThreadID);
  mStageThread->m_bAutoDelete = false;
  mStageThread->ResumeThread();
  mMovingStage = true;
  mBkgdMovingStage = inBackground;
  mWinApp->mCameraMacroTools.Update();
  mWinApp->mMontageWindow.Update();
  mWinApp->mTiltWindow.UpdateEnables();
  mWinApp->mAlignFocusWindow.Update();
  mWinApp->AddIdleTask(TaskStageBusy, TaskStageDone, TaskStageError, 0, 0);
  return true;
}

int CEMscope::TaskStageBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mScope->StageBusy();
}

void CEMscope::TaskStageError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mScope->StageCleanup(error);
  CEMscope::TaskStageDone(0);
}

// When the task is done or killed by stop, just set the flag false, do not kill thread
void CEMscope::TaskStageDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mScope->SetMovingStage(false);
  winApp->mScope->SetBkgdMovingStage(false);
  winApp->mCameraMacroTools.Update();
  winApp->mMontageWindow.Update();
  winApp->mTiltWindow.UpdateEnables();
  winApp->mAlignFocusWindow.Update();
}

int CEMscope::StageBusy(int ignoreOrTrustState)
{
  int retval = 0, getFast = 0;

  // First assess the state of the thread, if any
  if (mStageThread) {
    retval = UtilThreadBusy(&mStageThread);
    if (retval <= 0) {

      // If it is no longer active, unload time and set backlash variables
      // Also set the stage move delay to the default; routines wanting a longer delay
      // need to set it after the move is done
      if (mMoveInfo.axisBits & axisA)
        mLastTiltTime = mMoveInfo.finishedTick;
      if (mMoveInfo.axisBits & axisXY) {
        mLastStageTime = mMoveInfo.finishedTick;
        mShiftManager->SetStageDelayToUse(mShiftManager->GetStageMovedDelay());
      }
      SetValidXYbacklash(&mMoveInfo);
    }
    return retval;
  }

  // What to return if not initialized for some other reason?
  if (!sInitialized)
    return 1;

  if (JEOLscope) {

    // If updating by event, trust the internal data except every 4 seconds
    // Plugin uses state only when getting fast is set
    if (ignoreOrTrustState >= 0 && SEMTickInterval(mLastStageCheckTime) > 4000)
      ignoreOrTrustState = 1;
    if (mJeolSD.eventDataIsGood && ignoreOrTrustState <= 0)
      getFast = 1;
    mLastStageCheckTime = GetTickCount();
  }

  if (!getFast)
    ScopeMutexAcquire("StageBusy", true);

  try {

    // This blocks the UI but then so would an error
    while (JEOLscope && SEMCheckStageTimeout())
      Sleep(100);

    if (mPlugFuncs->GetStageStatus) {
      if (JEOLscope)
        GetValuesFast(getFast);
      retval = mPlugFuncs->GetStageStatus();
      if (JEOLscope)
        GetValuesFast(-1);
    }
  }
  catch (_com_error E) {
    // SEMReportCOMError(E, _T("getting Stage Status "));
    retval = -1;
  } 
  if (!getFast)
    ScopeMutexRelease("StageBusy");
  return retval;
}

void CEMscope::StageCleanup(int error)
{
  UtilThreadCleanup(&mStageThread);
}

// Thread procedure for moving the stage
UINT CEMscope::StageMoveProc(LPVOID pParam)
{
  StageMoveInfo *info = (StageMoveInfo *)pParam;
  StageThreadData stData;
  double destX = 0., destY = 0., destZ = 0., destAlpha = 0.;
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  int retval = 0;
  double xyLimit = 0.95e-3 * (JEOLscope ? 1.2 : 1.0);
  CString str, toStr;
  stData.info = info;

  // Checking this in the main thread hangs the interface, so wait until we get to the
  // stage thread to wait for this to expire
  while (JEOLscope && SEMCheckStageTimeout())
    Sleep(100);
  
  ScopeMutexAcquire("StageMoveProc", true);
  if (FEIscope) {
    if (info->plugFuncs->BeginThreadAccess(info->inBackground ? 3 : 1, 
      PLUGFEI_MAKE_STAGE)) {
      SEMMessageBox("Error creating second instance of microscope object for"
        " moving stage");
      SEMErrorOccurred(1);
      retval = 1;
    }
  }
  if (!retval) {
    try {
      StageMoveKernel(&stData, false, false, destX, destY, destZ, destAlpha);
    }
    catch (_com_error E) {
      toStr = "moving stage to";
      if (info->axisBits & (axisX | axisY)) {
        str.Format(" X %.2f  Y %.2f", destX * 1.e6, destY * 1.e6);
        toStr += str;
      }
      if (info->axisBits & axisZ) {
        str.Format("  Z %.2f", destZ * 1.e6);
        toStr += str;
      }
      if (info->axisBits & axisA) {
        str.Format("  tilt %.1f", destAlpha);
        toStr += str;
      }
      if (fabs(destX) > xyLimit || fabs(destY) > xyLimit) {
        SEMReportCOMError(E, _T(toStr + CString(".\r\n\r\n"
          "You are very close to the end of the stage range") + 
          CString(FEIscope ? "\r\n\r\nCheck Tecnai for pole touch and end"
          " of range messages.\r\n" : "")));
      } else {
        SEMReportCOMError(E, _T(toStr +
          CString(FEIscope ? "\r\n\r\nCheck Tecnai for pole touch and end"
          " of range messages.\r\n" : "")));
      }
      retval = 1;
    }
  }
  if (FEIscope) {
    info->plugFuncs->EndThreadAccess(info->inBackground ? 3 : 1);
  }
  CoUninitialize();
  ScopeMutexRelease("StageMoveProc");
  return retval;
}

// Underlying stage move procedure, callable in try block from scope or blanker threads
void CEMscope::StageMoveKernel(StageThreadData *std, BOOL fromBlanker, BOOL async, 
  double &destX, double &destY, double &destZ, double &destAlpha)
{
  StageMoveInfo *info = std->info;
  bool xyBacklash = info->doBacklash && (info->axisBits & axisXY);
  double xpos = 0., ypos = 0., zpos = 0., alpha, timeout;
  double startTime = GetTickCount();
  int step, back, relax;
  destX = 0.;
  destY = 0.;
  destZ = 0.;

  // If doing backlash, set up a first move with backlash added
  // Only change axes that are specified in axisBits
  // First do a backlash move if specified, then do the move to an overshoot if relaxing
  // and finally the move to the target
  for (step = 0; step < 3; step++) {
    back = step == 0 ? 1 : 0;
    relax = step == 1 ? 1 : 0;
    if ((back && !info->doBacklash) || (relax && !info->doRelax))
      continue;

    // Adding backX/Y makes the second move be opposite to the direction of backX/Y
    // Subtracting relax makes the final move be in the direction of relaxX/Y
    if (info->axisBits & (axisX | axisY)) {
      xpos = info->x + back * info->backX - relax * info->relaxX;
      ypos = info->y + back * info->backY - relax * info->relaxY;
      destX = xpos * 1.e-6;
      destY = ypos * 1.e-6;
    }
    zpos = info->z + back * info->backZ;
    alpha = info->alpha + back * info->backAlpha;
    destZ = zpos * 1.e-6;
    destAlpha = alpha;
    if (fromBlanker && (info->axisBits & (axisX | axisY)))
      SEMTrace('S', "BlankerProc stage move to %.2f %.2f", xpos, ypos);
    if (info->doRelax || info->doBacklash)
      SEMTrace('n', "Step %d, move to %.3f %.3f", step + 1, xpos, ypos);
     
    // destX, destY are used to decide on "stage near end of range" message upon error
    // They are supposed to be in meters; leave at 0 to disable 
    if (std->info->useSpeed && std->info->plugFuncs->SetStagePosition) {
      try {
        std->info->plugFuncs->SetStagePositionExtra(destX, destY, destZ, alpha * DTOR, 0.,
          std->info->speed, info->axisBits);
      }
      catch (_com_error E) {

        // If there is an error with the speed setting and conditions are right, wait
        // for a time that depends on the speed before throwing the error on
        if (std->info->speed > sStageErrSpeedThresh || SEMTickInterval(startTime) <
          1000. * sStageErrTimeThresh) {
            SEMTrace('S', "Stage error occurred at %.1f sec with speed factor %.3f",
              SEMTickInterval(startTime) / 1000., std->info->speed);
            throw E;
        }
        timeout = 1000. * sStageErrWaitFactor / std->info->speed;
        SEMTrace('S', "Caught stage error at %.1f, waiting up to %.f sec for stage ready", 
          SEMTickInterval(startTime) / 1000., timeout / 1000.);
        startTime = GetTickCount();
        while (SEMTickInterval(startTime) < timeout) {
          if (!std->info->plugFuncs->GetStageStatus())
            break;
          Sleep(100);
        }
        if (SEMTickInterval(startTime) > timeout) {
          SEMTrace('S', "Timeout expired at %.1f, passing on error", 
            SEMTickInterval(startTime) / 1000.);
          throw E;
        }
      }

    } else {
      std->info->plugFuncs->SetStagePosition(destX, destY, destZ, alpha * DTOR,
        info->axisBits);
    }
    if (async && (JEOLscope || FEIscope))
      return;
    if (HitachiScope || JEOLscope)
      WaitForStageDone(info, fromBlanker ? "BlankerProc" : "StageMoveProc");         
    sTiltAngle = std->info->plugFuncs->GetTiltAngle() / DTOR;
    ManageTiltReversalVars();
    if (step == 2) {
      std->info->plugFuncs->GetStagePosition(&xpos, &ypos, &zpos);
      info->x = xpos * 1.e6;
      info->y = ypos * 1.e6;
    }
  }
  info->finishedTick = GetTickCount();
}

// Waits the given milliseconds for the stage to be ready, returns true if it fails
BOOL CEMscope::WaitForStageReady(int msecs)
{
  double startTime = GetTickCount();
  double delta;
  while (StageBusy() > 0) {
    delta = SEMTickInterval(startTime);

    // Try one last time, forcing a read from scope on JEOL
    if (delta > msecs) {
      if (StageBusy(1) > 0)
        return true;
      break;
    }
    Sleep(100);
  }
  return false;
}

// Get X, Y, Z in microns
BOOL CEMscope::GetStagePosition(double &X, double &Y, double &Z)
{
  BOOL success = true, needMutex = false;
  if (!sInitialized)
    return false;

  // Plugin will use state value if update by event or getting fast
  if (JEOLscope) {
    needMutex = !(mJeolSD.eventDataIsGood || mGettingValuesFast);
    while (SEMCheckStageTimeout())
      Sleep(200);
  }

  if (needMutex)
    ScopeMutexAcquire("GetStagePosition", true);

  try {
    mPlugFuncs->GetStagePosition(&X, &Y, &Z);
    X *= 1.e6;
    Y *= 1.e6;
    Z *= 1.e6;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting stage position "));
    success = false;
  } 
  if (needMutex)
    ScopeMutexRelease("GetStagePosition");
  return success;
}

BOOL CEMscope::FastStagePosition(double & X, double & Y, double & Z)
{
  GetValuesFast(1);
  BOOL retval = GetStagePosition(X, Y, Z);
  GetValuesFast(0);
  return retval;
}

// Return image shift, possibly adjusted to center shift if low dose is on
BOOL CEMscope::GetLDCenteredShift(double &shiftX, double &shiftY)
{
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();
  if (!GetImageShift(shiftX, shiftY))
    return false;
  if (mLowDoseMode && mLowDoseSetArea >= 0 && (!mUsePiezoForLDaxis || !mLowDoseSetArea)) {
    shiftX -= mLastLDpolarity * ldParams[mLowDoseSetArea].ISX;
    shiftY -= mLastLDpolarity * ldParams[mLowDoseSetArea].ISY;
  }
  return true;
}

// Set image shift, adjusted to center shift if low dose is on
BOOL CEMscope::SetLDCenteredShift(double shiftX, double shiftY)
{
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();
  if (mLowDoseMode && mLowDoseSetArea >= 0) {
    shiftX += mLastLDpolarity * ldParams[mLowDoseSetArea].ISX;
    shiftY += mLastLDpolarity * ldParams[mLowDoseSetArea].ISY;
  }
  return SetImageShift(shiftX, shiftY);
}

// Get image shift in microns
BOOL CEMscope::GetImageShift(double &shiftX, double &shiftY)
{
  BOOL success = true;
  double axisISX, axisISY;

  if (!sInitialized)
    return false;

  GetTiltAxisIS(axisISX, axisISY);

  // Plugin will return from state if it is true IS and updating by event, or getting fast
  bool needMutex = !(JEOLscope && ((mJeolSD.eventDataIsGood && !mJeolSD.usePLforIS) ||
    mGettingValuesFast));
  if (needMutex)
    ScopeMutexAcquire("GetImageShift", true);
  
  try {
    mPlugFuncs->GetImageShift(&shiftX, &shiftY);
    shiftX -= axisISX;
    shiftY -= axisISY;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Image Shift "));
    success = false;
  } 
  if (needMutex)
    ScopeMutexRelease("GetImageShift");
  return success;
}

// For testing IS within range in mouse shifting
BOOL CEMscope::FastImageShift(double &shiftX, double &shiftY)
{
  GetValuesFast(1);
  BOOL retval = GetImageShift(shiftX, shiftY);
  GetValuesFast(0);
  return retval;
}

// Set it in microns
BOOL CEMscope::SetImageShift(double shiftX, double shiftY)
{
  return ChangeImageShift(shiftX, shiftY, false);
}

BOOL CEMscope::IncImageShift(double shiftX, double shiftY)
{
  return ChangeImageShift(shiftX, shiftY, true);
}

BOOL CEMscope::ChangeImageShift(double shiftX, double shiftY, BOOL bInc)
{
  BOOL success;
  double axisISX, axisISY, plugISX, plugISY;
  bool needBeamShift = false;
  int magIndex;
  mLastISdelX = shiftX;
  mLastISdelY = shiftY;
  ScaleMat IStoBS;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("ChangeImageShift", true);

  try {

    // Get the current position; it is needed to keep track of changes even when not
    // doing incremental change
    mPlugFuncs->GetImageShift(&plugISX, &plugISY);

    // Set the final value for relative changes
    if (bInc) {
      shiftX += plugISX;
      shiftY += plugISY;
    } else { 
      
      // Absolute change, get the delta from last position
      // absolute change needs to account for tilt axis offset
      GetTiltAxisIS(axisISX, axisISY);
      mLastISdelX -= plugISX - axisISX;
      mLastISdelY -= plugISY - axisISY;
      shiftX += axisISX;
      shiftY += axisISY;
    }
  
    // Make the movements; get mag if not STEM mode and need beam shift
    mPlugFuncs->SetImageShift(shiftX, shiftY);
    if (JEOLscope)
      GetValuesFast(1);
    needBeamShift = (JEOLscope || HitachiScope) && !(mWinApp->ScopeHasSTEM() && 
      mPlugFuncs->GetSTEMMode && mPlugFuncs->GetSTEMMode());
    if (needBeamShift)
      magIndex = mPlugFuncs->GetMagnificationIndex();
    if (JEOLscope)
      GetValuesFast(-1);
    SetISChanged(shiftX, shiftY);

    // Need to apply beam shift if it is calibrated for some scopes
    if (needBeamShift && !mSkipNextBeamShift) {
      IStoBS = mShiftManager->GetBeamShiftCal(magIndex);
      if (IStoBS.xpx) {
        shiftX = IStoBS.xpx * mLastISdelX + IStoBS.xpy * mLastISdelY;
        shiftY = IStoBS.ypx * mLastISdelX + IStoBS.ypy * mLastISdelY;
        mPlugFuncs->GetBeamShift(&plugISX, &plugISY);
        mPlugFuncs->SetBeamShift(plugISX + shiftX, plugISY + shiftY);
      }
    }
    mSkipNextBeamShift = false;

    success = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Image Shift "));
    success = false;
  }
  ScopeMutexRelease("ChangeImageShift");
  return success;
}

// Get the image shift offsets needed to center on the tilt axis, if mode selected
// In any event, add the neutral value and user offset
void CEMscope::GetTiltAxisIS(double &ISX, double &ISY)
{
  static BOOL firstTime = true;
  int STEMmode = mLastSTEMmode;
  int toCam = -1;

  bool jeolEFTEM = JEOLscope && !mHasOmegaFilter && mJeolSD.JeolEFTEM >= 0;
  ISX = 0.;
  ISY = 0.;
  if (!mLastMagIndex && firstTime)
    mLastMagIndex = GetMagIndex();
  firstTime = false;
  if (!mLastMagIndex || mLastMagIndex >= MAX_MAGS)
    return;

  // Set image shift value to the sum of neutral and offset values
  if (STEMmode < 0 && mWinApp->ScopeHasSTEM())
    STEMmode = GetSTEMmode() ? 1 : 0;
  if (STEMmode <= 0) {
    if (jeolEFTEM) {

      // When JEOL EFTEM is switching, the neutral index has the new state, needed here
      ISX = mMagTab[mLastMagIndex].neutralISX[mNeutralIndex] + 
        (mApplyISoffset ? mMagTab[mLastMagIndex].calOffsetISX[mNeutralIndex] : 0.);
      ISY = mMagTab[mLastMagIndex].neutralISY[mNeutralIndex] + 
        (mApplyISoffset ? mMagTab[mLastMagIndex].calOffsetISY[mNeutralIndex] : 0.);
    } else {
      ISX = mMagTab[mLastMagIndex].neutralISX[mNeutralIndex] + 
        mMagTab[mLastMagIndex].offsetISX;
      ISY = mMagTab[mLastMagIndex].neutralISY[mNeutralIndex] + 
        mMagTab[mLastMagIndex].offsetISY;
    }
  } else if (JEOLscope) {
    ISX = mSTEMneutralISX;
    ISY = mSTEMneutralISY;
  }
  
  // 12/24/06, shift to axis in low mag too, for consistency in maps
  if (!mShiftToTiltAxis || !mTiltAxisOffset)
    return;

  // If shifting to axis, now convert axis offset to image shift, but set camera if 
  // JEOL EFTEM is changing
  if (jeolEFTEM && (mWinApp->GetEFTEMMode() ? 1 : 0) != mNeutralIndex)
    toCam = mActiveCamList[B3DMAX(0, mNeutralIndex ? mFiltParam->firstGIFCamera : 
        mFiltParam->firstRegularCamera)];
  ScaleMat aMat = mShiftManager->IStoSpecimen(mLastMagIndex, toCam);
  if (!aMat.xpx)
    return;
  aMat = mShiftManager->MatInv(aMat);
  ISX += aMat.xpy * mTiltAxisOffset;
  ISY += aMat.ypy * mTiltAxisOffset;
}

// Set flag for whether to apply extra offsets: either copy values in or zero them out
void CEMscope::SetApplyISoffset(BOOL inVal)
{
  int i;
  int ind = mWinApp->GetEFTEMMode() ? 1 : 0;
  if (mLowDoseMode)
    mWinApp->mLowDoseDlg.ConvertAxisPosition(false);
  if (inVal && !GetSTEMmode()) {
    for (i = 1; i < MAX_MAGS; i++) {
      mMagTab[i].offsetISX = mMagTab[i].calOffsetISX[ind];
      mMagTab[i].offsetISY = mMagTab[i].calOffsetISY[ind];
    }
  } else {
    for (i = 1; i < MAX_MAGS; i++)
      mMagTab[i].offsetISX = mMagTab[i].offsetISY = 0.;
  }
  mApplyISoffset = inVal;
  if (mLowDoseMode)
    mWinApp->mLowDoseDlg.ConvertAxisPosition(true);
}

// Reset the neutral value for STEM on a JEOL
void CEMscope::ResetSTEMneutral(void)
{
  double ISX, ISY;
  if (!JEOLscope || !GetSTEMmode())
    return;
  GetImageShift(ISX, ISY);
  SEMTrace('1', "Change STEM neutral from %f %f to %f %f", mSTEMneutralISX, 
    mSTEMneutralISY, mSTEMneutralISX+ISX, mSTEMneutralISY+ISY);
  mSTEMneutralISX = (float)(mSTEMneutralISX + ISX);
  mSTEMneutralISY = (float)(mSTEMneutralISY + ISY);
  mNeedSTEMneutral = false;
}

// Return beam shift in microns too
BOOL CEMscope::GetBeamShift(double &shiftX, double &shiftY)
{
  BOOL success = true;
  if (!sInitialized)
    return false;
  ScopeMutexAcquire("GetBeamShift", true);
  try {
    mPlugFuncs->GetBeamShift(&shiftX, &shiftY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Beam Shift "));
    success = false;
  } 
  ScopeMutexRelease("GetBeamShift");
  return success;
}

BOOL CEMscope::SetBeamShift(double shiftX, double shiftY)
{
  return ChangeBeamShift(shiftX, shiftY, false);
}

BOOL CEMscope::IncBeamShift(double shiftX, double shiftY)
{
  return ChangeBeamShift(shiftX, shiftY, true);
}

BOOL CEMscope::ChangeBeamShift(double shiftX, double shiftY, BOOL bInc)
{
  BOOL success = true;
  double oldX, oldY;

  if (!sInitialized)
    return false;
  
  ScopeMutexAcquire("ChangeBeamShift", true);

  try {
    if (bInc) {  // incremental
      mPlugFuncs->GetBeamShift(&oldX, &oldY);
      shiftX += oldX;
      shiftY += oldY;
    }
    mPlugFuncs->SetBeamShift(shiftX, shiftY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Beam Shift "));
    success = false;
  } 
  ScopeMutexRelease("ChangeBeamShift");
  return success;
}

// Return beam tilt in milliradians or percent of full scale
BOOL CEMscope::GetBeamTilt(double &tiltX, double &tiltY)
{
  BOOL success = true;

  if (!sInitialized)
    return false;

  ScopeMutexAcquire("GetBeamTilt", true);
  
  try {
    mPlugFuncs->GetBeamTilt(&tiltX, &tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Beam Tilt "));
    success = false;
  }
  ScopeMutexRelease("GetBeamTilt");
  return success;
}

BOOL CEMscope::SetBeamTilt(double tiltX, double tiltY)
{
  return ChangeBeamTilt(tiltX, tiltY, false);
}

BOOL CEMscope::IncBeamTilt(double tiltX, double tiltY)
{
  return ChangeBeamTilt(tiltX, tiltY, true);
}

BOOL CEMscope::ChangeBeamTilt(double tiltX, double tiltY, BOOL bInc)
{
  BOOL success = true;
  double plugX, plugY;

  if (!sInitialized)
    return false;

  ScopeMutexAcquire("ChangeBeamTilt", true);

  try {
    if (bInc) {
      mPlugFuncs->GetBeamTilt(&plugX, &plugY);
      tiltX += plugX;
      tiltY += plugY;
    }
    mPlugFuncs->SetBeamTilt(tiltX, tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Beam Tilt "));
    success = false;
  } 
  ScopeMutexRelease("ChangeBeamTilt");
  return success;
}

// Get the dark field mode and tilt, if mode is non-zero, return 0,0 tilt if mode 0
BOOL CEMscope::GetDarkFieldTilt(int &mode, double &tiltX, double &tiltY)
{
  BOOL success = true;

  if (!sInitialized)
    return false;

  tiltX = tiltY = 0.;
  mode = 0;
  if (!mPlugFuncs->GetDarkFieldTilt || !mPlugFuncs->GetDarkFieldMode || 
    mPluginVersion < FEI_PLUGIN_DARK_FIELD)
    return true;

  ScopeMutexAcquire("GetDarkFieldTilt", true);
  
  try {
    mode = mPlugFuncs->GetDarkFieldMode();
    if (mode)
      mPlugFuncs->GetDarkFieldTilt(&tiltX, &tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Dark Field Mode or Tilt "));
    success = false;
  }
  ScopeMutexRelease("GetDarkFieldTilt");
  return success;
}

// Set the dark field mode and tilt; if tilt is 0,0, set the mode to 0
BOOL CEMscope::SetDarkFieldTilt(int mode, double tiltX, double tiltY)
{
  BOOL success = true;
  int curMode;

  if (!sInitialized)
    return false;

  if (!mPlugFuncs->SetDarkFieldTilt || !mPlugFuncs->GetDarkFieldMode || 
    !mPlugFuncs->GetDarkFieldMode || mPluginVersion < FEI_PLUGIN_DARK_FIELD)
    return true;

  ScopeMutexAcquire("SetDarkFieldTilt", true);
  if (tiltX == 0. && tiltY == 0.)
    mode = 0;
  
  try {
    curMode = mPlugFuncs->GetDarkFieldMode();
    if (mode != curMode)
      mPlugFuncs->SetDarkFieldMode(mode);
    if (mode)
      mPlugFuncs->SetDarkFieldTilt(tiltX, tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Dark Field Mode or Tilt "));
    success = false;
  }
  ScopeMutexRelease("SetDarkFieldTilt");
  return success;
}

// Get the objective stigmator, range -1 to 1
bool CEMscope::GetObjectiveStigmator(double & stigX, double & stigY)
{
  bool success = true;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("GetObjectiveStigmator", true);
  
  try {
    mPlugFuncs->GetObjectiveStigmator(&stigX, &stigY);
  }
  catch (_com_error E) {
    success = false;
    SEMReportCOMError(E, _T("getting Objective Stigmator "));
  }
  ScopeMutexRelease("GetObjectiveStigmator");
  return success;
}

// Set the objective stigmator
bool CEMscope::SetObjectiveStigmator(double stigX, double stigY)
{
  bool success = true;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetObjectiveStigmator", true);
  
  try {
    mPlugFuncs->SetObjectiveStigmator(stigX, stigY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Objective Stigmator "));
    success = false;
  }
  ScopeMutexRelease("SetObjectiveStigmator");
  return success;
}


// SCREEN POSITION AND CURRENT FUNCTIONS
//
// Screen current
double CEMscope::GetScreenCurrent()
{
  double result;

  if (!sInitialized)
    return 0.;
  
  ScopeMutexAcquire("GetScreenCurrent",true);

  try {
    PLUGSCOPE_GET(ScreenCurrent, result, 1.e9);
    result *= mScreenCurrentFactor;
    if (SmallScreenIn())
      result *= mSmallScreenFactor;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting screen current "));
    result = 0.;
  }
  ScopeMutexRelease("GetScreenCurrent");
  return result;
}

// Small screen state
BOOL CEMscope::SmallScreenIn()
{
  BOOL result = false;

  // Plugin will use state value if update by event or getting fast
  bool needMutex = !(JEOLscope && ((mScreenByEvent && mJeolSD.eventDataIsGood) || 
    mGettingValuesFast));
  if (!sInitialized)
    return false;
  
  if (!mReportsSmallScreen)
    return false;

  if (needMutex)
    ScopeMutexAcquire("SmallScreenIn", true);

  try {
    if (mPlugFuncs->GetIsSmallScreenDown) {  // Plugin
      result = mPlugFuncs->GetIsSmallScreenDown();
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting small screen position "));
  }
  if (needMutex)
    ScopeMutexRelease("SmallScreenIn");
  return result;
}

int CEMscope::GetScreenPos()
{
  int result;
  if (mNoScope)
    return mFakeScreenPos;
  if (!sInitialized)
    return spUnknown;

  // Plugin will use state value if update by event or getting fast
  bool needMutex = !(JEOLscope && ((mScreenByEvent && mJeolSD.eventDataIsGood) || 
    mGettingValuesFast || !mReportsLargeScreen));
  if (needMutex)
    ScopeMutexAcquire("GetScreenPos", true);
  try {
    PLUGSCOPE_GET(MainScreen, result, 1);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting screen position "));
    result = spUnknown;
  }
  if (needMutex)
    ScopeMutexRelease("GetScreenPos");
  return result;
}


int CEMscope::FastScreenPos(void)
{
  if (mNoScope) {
    return mFakeScreenPos;
  } else {
    FAST_GET(int, GetScreenPos);
  }
}

static int newScreenPos;

// Set the screen position; make sure EFTEM is right for the position change
BOOL CEMscope::SetScreenPos(int inPos)
{
  if (mNoScope) {
    if (inPos != spUp && inPos != spDown)
      return false;
    if (inPos != mFakeScreenPos)
      mWinApp->mDocWnd->SetShortTermNotSaved();
    mFakeScreenPos = inPos;
    return true;
  }
  if (!sInitialized)
    return false;
  if (ScreenBusy() > 0) {
    AfxMessageBox(_T("Screen is already moving"));
    return false;
  }

  if (mWinApp->GetEFTEMMode() && mCanControlEFTEM) {
    BOOL needed = (inPos == spUp || !mFiltParam->autoMag ||
      (mWinApp->mAutocenDlg && mWinApp->mAutocenDlg->m_bSetState));
    SetEFTEM(needed);
  }
  
  mMoveInfo.axisBits = inPos;
  mScreenThread = AfxBeginThread(ScreenMoveProc, &mMoveInfo,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("ScreenMoveProc thread created with ID 0x%0x\n",mScreenThread->m_nThreadID);
  mScreenThread->m_bAutoDelete = false;
  mScreenThread->ResumeThread();
  // This used to be done for "singleTecnai"
  //mWinApp->AddIdleTask(TaskScreenBusy, NULL, TaskScreenError, 0, 60000);
  return true;
}

int CEMscope::TaskScreenBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mScope->ScreenBusy();
}

void CEMscope::TaskScreenError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mScope->ScreenCleanup();
}

// Is the screen busy?  Suitable for calling from OnIdle busy function
int CEMscope::ScreenBusy()
{
  return UtilThreadBusy(&mScreenThread);
}

// Delete the thread for raising the screen if there is a timeout
// But where does that leave COM?
void CEMscope::ScreenCleanup()
{
  UtilThreadCleanup(&mScreenThread);
}

// Thread Procedure for moving the screen
UINT CEMscope::ScreenMoveProc(LPVOID pParam)
{
  // DNM: Added the mutex acquire and release
  StageMoveInfo *info = (StageMoveInfo *)pParam;
  int mainPos, newPos = info->axisBits;
  double timeout = 15000., startTime = GetTickCount();

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ScopeMutexAcquire("ScreenMoveProc", true);
  int retval = 0;

  if (FEIscope && info->plugFuncs->BeginThreadAccess(1, PLUGFEI_MAKE_CAMERA)) {
    SEMMessageBox("Error creating second instance of microscope object for"
      "moving screen");
    SEMErrorOccurred(1);
    retval = 1;
  }
  if (!retval) {
    try {

      // JEOL waits for position to change to desired one
      INFOPLUG_SET(MainScreen, (enum ScreenPosition)newPos);
      while (!JEOLscope && SEMTickInterval(startTime) < timeout) {
        INFOPLUG_GET(MainScreen, mainPos, 1);
        if (mainPos == newPos)
          break;
        Sleep(50);
      }
    }
    catch (_com_error E) {

      // Workaround to Titan problem; check if the screen ends up as desired
      bool isErr = true;
      if (FEIscope && sCheckPosOnScreenError) {
        while (SEMTickInterval(startTime) < timeout) {
          int mainPos;
          try {
            INFOPLUG_GET(MainScreen, mainPos, 1);
            if (mainPos == newPos) {
              isErr = false;
              break;
            }
          }
          catch (_com_error E) {
          }
          Sleep(50);
        }
      }
      if (isErr) {
        SEMReportCOMError(E, _T("moving screen "));
        retval = 1;
      }
    }
  }
  if (FEIscope) {
    info->plugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  ScopeMutexRelease("ScreenMoveProc");
  return retval;
}

// Function to set static
void CEMscope::SetCheckPosOnScreenError(BOOL inVal)
{
  sCheckPosOnScreenError = inVal;
}

// MAGNIFICATION AND PROJECTOR NORMALIZATION

// Get the magnification value
double CEMscope::GetMagnification()
{
  double result;
  if (!sInitialized)
    return 0.;

  // Plugin will use state value if update by event or getting fast, and not scanning
  bool needMutex = !(JEOLscope && (mJeolSD.eventDataIsGood || mGettingValuesFast) && 
    !sScanningMags);
  if (needMutex)
    ScopeMutexAcquire("GetMagnification", true);
  try {
    if (GetSTEMmode()) {
      PLUGSCOPE_GET(STEMMagnification, result, 1.);
    } else {
      PLUGSCOPE_GET(Magnification, result, 1.);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting magnification "));
    result = 0.;
  }
  if (needMutex)
    ScopeMutexRelease("GetMagnification");
  return result;
}

// Quickly get a mag index using state value on JEOL
int CEMscope::FastMagIndex()
{
  if (mNoScope) {
    return mFakeMagIndex;
  } else {
    FAST_GET(int, GetMagIndex);
  }
}

// Get the mag index for the current mag; returns zero in diffraction
int CEMscope::GetMagIndex(BOOL forceGet)
{
  int result, gettingFast = 0;
  if (mNoScope)
    return mFakeMagIndex;
  if (!sInitialized)
    return -1;

  // Plugin will use state value only if getting fast and mag is valid
  if (JEOLscope && (mJeolSD.eventDataIsGood || mGettingValuesFast) && !forceGet && 
    !mJeolSD.magInvalid)
    gettingFast = 1;

  // When doing a long operation, the mag index seems to be the Achille's heel, so only
  // try for the mutex once and return last mag index when it fails
  if (!gettingFast && !ScopeMutexAcquire("GetMagIndex", !mDoingLongOperation))
    return mLastMagIndex;

  try {

    if (JEOLscope) {

      // Jeol takes care of setting secondary mode
      GetValuesFast(gettingFast);
      PLUGSCOPE_GET(MagnificationIndex, result, 1);
      GetValuesFast(-1);
    
    } else {  // FEI-like
      if (GetSTEMmode()) {     // STEM mode
        double minDiff, curMag;
        PLUGSCOPE_GET(STEMMagnification, curMag, 1.);
        PLUGSCOPE_GET(ProbeMode, mProbeMode, 1);
        mProbeMode = (mProbeMode == imMicroProbe) ? 1 : 0;
        result = LookupSTEMmagFEI(curMag, mProbeMode, minDiff);
        if (minDiff >= 1.)
          SEMTrace('1', "WARNING: GetMagIndex did not find exact match to %.0f in STEM"
          " mag table", curMag);
      } else {

        // Regular mode
        PLUGSCOPE_GET(MagnificationIndex, result, 1);
        SetSecondaryModeForMag(result);
      }
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting mag index "));
    result = -1;
  }
  if (!gettingFast)
    ScopeMutexRelease("GetMagIndex");
  return result;
}


// Find closest mag in STEM mag table for an FEI
int CEMscope::LookupSTEMmagFEI(double curMag, int curMode, double & minDiff)
{
  int result, lowlim, hilim;
  double diff;
  minDiff = 1.e20;
  lowlim = curMode ? mLowestMicroSTEMmag : 1;
  hilim = curMode ? MAX_MAGS : mLowestMicroSTEMmag;
  for (int i = hilim - 1; i >= lowlim; i--) {
    diff = fabs(mMagTab[i].STEMmag - curMag);
    if (diff < minDiff) {
      minDiff = diff;
      result = i;
    }
  }
  return result;
}

// Set the magnification by its index
BOOL CEMscope::SetMagIndex(int inIndex)
{
  double tranISX, tranISY, focusStart, startTime, startISX, startISY, curISX, curISY;
  BOOL result = true;
  BOOL convertIS, restoreIS, ifSTEM, normAll;
  bool focusChanged = false, ISchanged, unblankAfter;
  int mode, newmode, lowestM, ticks;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  const char *routine = "SetMagIndex";

  if (mNoScope) {
    if (inIndex < 1 || inIndex > MAX_MAGS || !mMagTab[inIndex].mag)
      return false;
    if (inIndex != mFakeMagIndex)
      mWinApp->mDocWnd->SetShortTermNotSaved();
    mFakeMagIndex = inIndex;
    return true;
  }
  if (!sInitialized)
    return false;
  
  int currentIndex = GetMagIndex(true);
  if (currentIndex == inIndex)
    return true;

  // First see if need to save the current IS and then whether to restore it
  ifSTEM = GetSTEMmode();
  lowestM = ifSTEM ? mLowestSTEMnonLMmag[mProbeMode] : mLowestMModeMagInd;
  if (FEIscope && ifSTEM && inIndex < lowestM)
    return false;
  convertIS = AssessMagISchange(currentIndex, inIndex, ifSTEM, tranISX, tranISY);
  ScopeMutexAcquire(routine, true);
  SaveISifNeeded(currentIndex, inIndex);
  if (!ifSTEM)
    restoreIS = AssessRestoreIS(currentIndex, inIndex, tranISX, tranISY);
  try {

    unblankAfter = BlankTransientIfNeeded(routine);
    
    if (JEOLscope) { 

      // JEOL STEM dies if mag is changed too soon after last image; 
      if (ifSTEM && !camParam->pluginName.IsEmpty()) {
        ticks = (int)SEMTickInterval(1000. * mWinApp->mCamera->GetLastImageTime());
        if (ticks < mJeolSTEMPreMagDelay)
          Sleep(mJeolSTEMPreMagDelay - ticks);
      }
      PLUGSCOPE_SET(MagnificationIndex, inIndex);
    
    } else {  // FEIlike

      if (HitachiScope && !ifSTEM) {
        focusStart = mPlugFuncs->GetAbsFocus();
        mPlugFuncs->GetImageShift(&startISX, &startISY);
      }
    
      // Tecnai: if in diffraction mode, set back to imaging
      if (!currentIndex) {
        if (mPlugFuncs->SetImagingMode)
          mPlugFuncs->SetImagingMode(pmImaging);
      }

      if (ifSTEM) {
        PLUGSCOPE_GET(ProbeMode, mode, 1);
        mode = (mode == imMicroProbe ? 1 : 0);
        newmode = inIndex >= mLowestMicroSTEMmag ? 1 : 0;
        if (mode != newmode) {
          PLUGSCOPE_SET(ProbeMode, (newmode ? imMicroProbe : imNanoProbe));
          mProbeMode = newmode;
        }
        PLUGSCOPE_SET(STEMMagnification, mMagTab[inIndex].STEMmag);
      } else {

        // Disable autonormalization, change mag, normalize and reenable
        AUTONORMALIZE_SET(*vFalse);
        PLUGSCOPE_SET(MagnificationIndex, inIndex);

        // Hitachi needs to poll and make sure associated changes are done
        if (HitachiScope) {
          startTime = GetTickCount();
          ISchanged = inIndex >= mLowestMModeMagInd && inIndex < mLowestSecondaryMag;
          while (1) {
            if (SEMTickInterval(startTime) > mHitachiMagFocusDelay ||
              (focusChanged && SEMTickInterval(startTime) > mHitachiMagISDelay)) {
              SEMTrace('i', "Timeout waiting for %s %s %s change", 
                focusChanged ? "" : "focus", ISchanged || focusChanged ? "" : "and",
                ISchanged ? "" : "IS");
              break;
            }
            if (!focusChanged && focusStart != mPlugFuncs->GetAbsFocus()) {
              focusChanged = true;
              SEMTrace('i', "Focus change seen in %.0f", SEMTickInterval(startTime));
            }
            if (!ISchanged) {
              mPlugFuncs->GetImageShift(&curISX, &curISY);
              ISchanged = curISX != startISX || curISY != startISY;
              if (ISchanged)
              SEMTrace('i', "IS change seen in %.0f", SEMTickInterval(startTime));
            }
            if (focusChanged && ISchanged)
              break;
            Sleep(30);
          }
        }

        if (mPlugFuncs->NormalizeLens && mCalNeutralStartMag < 0) {

          // Normalize all lenses if going in or out of LM, or if in LM and option is 1,
          // or always if option is 2.  This used to do condenser in addition, but now
          // it does "all", which adds objective
          normAll = !ifSTEM && (!BOOL_EQUIV(currentIndex < lowestM, inIndex < lowestM) ||
            (inIndex < lowestM && mNormAllOnMagChange > 0) || mNormAllOnMagChange > 1);
          if (normAll && FEIscope) {
            mPlugFuncs->NormalizeLens(ALL_INSTRUMENT_LENSES);
          } else {
            mPlugFuncs->NormalizeLens(pnmProjector);
            if (normAll) {
              mPlugFuncs->NormalizeLens(nmCondenser);
              mPlugFuncs->NormalizeLens(pnmObjective);
            }
          }
        }
        mLastNormalization = GetTickCount();
        mLastNormMagIndex = inIndex;

        AUTONORMALIZE_SET(*vTrue);

      }
    }
    UnblankAfterTransient(unblankAfter, routine);
    
    // set so that this is not detected as a user change
    SetMagChanged(inIndex);
    mLastNormalization = GetTickCount();

    // Mode used to be handled here too.
    // Plugin changes the mode after the delay, but not the mag.
    if (JEOLscope) {
      SEMSetJeolStateMag(inIndex, true);
    }
    if (!ifSTEM)
      SetSecondaryModeForMag(inIndex);

    // Falcon Dose Protector may respond to transient conditions after a mag change,
    // such as a temporary setting of objective strength, so there needs to be a delay
    if (IS_FALCON2_OR_3(camParam))
      mShiftManager->SetGeneralTimeOut(mLastNormalization, mFalconPostMagDelay);
    HandleNewMag(inIndex);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting magnification "));
    result = false;
  }

  // If there is change in base image shift or mag change resets it, then put out the
  // absolute image shift without moving beam.  Do not do this if calibrating neutral
  // or if crossing LM-M boundary with offsets turned off
  if (result && !ifSTEM && (convertIS || restoreIS) && mCalNeutralStartMag < 0) {
    if (JEOLscope) {
      try {
        mPlugFuncs->UseMagInNextSetISXY(inIndex);
        mPlugFuncs->SetImageShift(tranISX, tranISY);
      }
      catch (_com_error E) {
        SEMReportCOMError(E, _T("adjusting image shift for mag change "));
        result = false;
      }

    } else {  // FEIlike
      double axisISX, axisISY;
      GetTiltAxisIS(axisISX, axisISY);
      SEMTrace('i', "SetMagIndex: converting to new net IS %.3f %.3f",
        tranISX - axisISX, tranISY - axisISY);
      SetImageShift(tranISX - axisISX, tranISY - axisISY);
    }
  }
  ScopeMutexRelease(routine);
  return result;
}

// Assesses the IS change for a change in mag from fromInd to toInd, returns new RAW IS
// (including the axis, etc, offsets) and the axis offsets for the current mag.  
BOOL CEMscope::AssessMagISchange(int fromInd, int toInd, BOOL STEMmode, double &newISX, 
                                 double &newISY)
{
  double delISX, delISY, oldISX, oldISY, realISX, realISY, tranISX, tranISY;
  double axisISX, axisISY;
  BOOL result;
  BOOL convertIS;
  BOOL bothLMnonLM = BothLMorNotLM(fromInd, STEMmode, toInd, STEMmode);

  if (!fromInd || !toInd || STEMmode)
    return false;
  delISX = mMagTab[toInd].neutralISX[mNeutralIndex] + mMagTab[toInd].offsetISX -
    (mMagTab[fromInd].neutralISX[mNeutralIndex] + mMagTab[fromInd].offsetISX);
  delISY = mMagTab[toInd].neutralISY[mNeutralIndex] + mMagTab[toInd].offsetISY -
    (mMagTab[fromInd].neutralISY[mNeutralIndex] + mMagTab[fromInd].offsetISY);
  convertIS = (bothLMnonLM || mApplyISoffset || mLowDoseMode) && 
    !mShiftManager->CanTransferIS(fromInd, toInd, STEMmode);
  if (delISX || delISY || MagChgResetsIS(toInd) || convertIS) {
    GetImageShift(oldISX, oldISY);
    GetTiltAxisIS(axisISX, axisISY);

    // If image shifts are not congruent, then get real shift including tilt axis offset
    // and convert to specimen coordinates at old mag then IS coords at new mag
    if (convertIS) {
      realISX = oldISX + axisISX - mMagTab[fromInd].neutralISX[mNeutralIndex] - 
        mMagTab[fromInd].offsetISX;
      realISY = oldISY + axisISY - mMagTab[fromInd].neutralISY[mNeutralIndex] - 
        mMagTab[fromInd].offsetISY;
      mShiftManager->TransferGeneralIS(fromInd, realISX, realISY, toInd,
        tranISX, tranISY);
      delISX += tranISX - realISX;
      delISY += tranISY - realISY;
    }
  }

  result = (delISX || delISY || MagChgResetsIS(toInd)) && mCalNeutralStartMag < 0 &&
    (bothLMnonLM || mApplyISoffset || mLowDoseMode);
  if (result) {
    newISX = oldISX + delISX + axisISX;
    newISY = oldISY + delISY + axisISY;
  }

  // Tecnai used to be done separately with an incorrect call to transferGeneral that
  // transferred the entire IS not just the real shift
  return result;
}

// Test whether IS should be restored going from fromInd to toInd and compute new raw IS
BOOL CEMscope::AssessRestoreIS(int fromInd, int toInd, double &newISX, double &newISY)
{
  double realISX, realISY;
  if (!SaveOrRestoreIS(toInd, fromInd) || mMagIndSavedIS < 0 || mCalNeutralStartMag >= 0)
    return false;
  realISX = mSavedISX - mMagTab[mMagIndSavedIS].neutralISX[mNeutralIndex] - 
    mMagTab[mMagIndSavedIS].offsetISX;
  realISY = mSavedISY - mMagTab[mMagIndSavedIS].neutralISY[mNeutralIndex] - 
    mMagTab[mMagIndSavedIS].offsetISY;
  mShiftManager->TransferGeneralIS(mMagIndSavedIS, realISX, realISY, toInd, newISX, 
    newISY);
  newISX += mMagTab[toInd].neutralISX[mNeutralIndex] + mMagTab[toInd].offsetISX;
  newISY += mMagTab[toInd].neutralISY[mNeutralIndex] + mMagTab[toInd].offsetISY;
  mMagIndSavedIS = -1;
  SEMTrace('i', "Restoring raw IS %.3f %.3f, going from %d to %d", newISX, newISY,
    fromInd, toInd);
  return true;
}

// Test whether IS should be save on this mag change, and if so save it
void CEMscope::SaveISifNeeded(int fromInd, int toInd)
{
  double axisISX, axisISY;
  if (SaveOrRestoreIS(fromInd, toInd)) {
    mMagIndSavedIS = fromInd;
    GetImageShift(mSavedISX, mSavedISY);
    GetTiltAxisIS(axisISX, axisISY);
    mSavedISX += axisISX;
    mSavedISY += axisISY;
    SEMTrace('i', "Saved raw IS %.3f %.3f, going from %d to %d", mSavedISX, mSavedISY,
      fromInd, toInd);
  }
}

// Routine to scan mags and produce mag table and camera length table
BOOL CEMscope::ScanMagnifications()
{
  long magVal;
  int rot, magInd, magSet, curMode, startingMode, firstBase;
  double startingSMag, curSMag, lastSMag;
  int lastMag, baseIndex = 0;
  BOOL retval = true;
  CString report, str;
  int functionModes[] = {JEOL_LOWMAG_MODE, JEOL_MAG1_MODE, JEOL_DIFF_MODE, 1, 3};
  int numModes = 3;
  int startingMag = 0;
  BOOL JeolGIF = false;
  BOOL STEMmode = GetSTEMmode();
  if (!STEMmode) {
    functionModes[1] = sJeolIndForMagMode;
    if (mLowestSecondaryMag) {
      numModes = 4;
      functionModes[3] = functionModes[2];
      functionModes[2] = sJeolSecondaryModeInd;
    }
  }

  if (!sInitialized || HitachiScope)
    return false;

  if (FEIscope && GetEFTEM() && GetScreenPos() != spUp) {
    AfxMessageBox("You must raise the viewing screen to get valid"
      " rotation values in EFTEM mode", MB_EXCLAME);
    return false;
  }

  // For JEOL, check for GIF mode and set up for STEM mode
  if (JEOLscope) {
    if (mJeolSD.JeolEFTEM > 0) {
      JeolGIF = true;
      numModes = 1;
      functionModes[0] = mPlugFuncs->GetImagingMode();
      baseIndex = mLowestMModeMagInd - 1;
      report.Format("EFTEM mode detected.  You should run this routine in EFTEM mode only"
        "\r\nafter setting LowestMModeMagIndex correctly in the property file.\r\n"
        "Output below assumes that the value of %d is correct", mLowestMModeMagInd);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
    if (STEMmode) {
      numModes = 2;
      functionModes[0] = JEOL_STEM_LOWMAG;
      functionModes[1] = JEOL_STEM_MAGMODE;
    }
  }

  // For FEI STEM mode, do completely different procedure with user stepping mags
  if (LikeFEIscope && STEMmode) {
    try {
      PLUGSCOPE_GET(STEMMagnification, startingSMag, 1.);
      PLUGSCOPE_GET(ProbeMode, startingMode, 1);
      startingMode = (startingMode == imMicroProbe) ? 1 : 0;
      if (startingMode)
        PLUGSCOPE_SET(ProbeMode, imNanoProbe);

      report = "In STEM mode, you need to go to the lowest STEM magnification then\r\n"
        "step through the mags with the magnification knob.\r\nYou will step through the "
        "mags in nanoprobe mode then in microprobe mode.\r\nBe sure to enable LMscan in "
        "the microscope interface and start at the lowest LM mag,\r\n"
        "unless NO ONE is EVER going to use the LM mags.\r\n\r\nYou will close a "
        "confirmation dialog box after each mag step.\r\nMake sure that turning the knob "
        "does not take two mag steps instead of one";
      mWinApp->AppendToLog(report);
      AfxMessageBox(report, MB_EXCLAME);

      // Loop on each mode
      magInd = 1;
      for (curMode = 0; curMode < 2; curMode++) {
        baseIndex = magInd;
        report = "Go to the lowest magnification in ";
        if (curMode) {
          report += "microprobe mode\n\n";
          PLUGSCOPE_SET(ProbeMode, imMicroProbe);
        } else
          report += "nanoprobe mode\n\n";

        // Loop within the mode until they say it is done
        for (; magInd < 2 * MAX_MAGS; magInd++) {
          report += "Press OK or Enter when the magnification is set\n\n"
            "Press Cancel when the magnification will not go any higher";
          if (AfxMessageBox(report, MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
            break;
          report = "Go to the next higher magnification\n\n";
          PLUGSCOPE_GET(STEMMagnification, curSMag, 1.);
          if (magInd > 1 && curSMag == lastSMag) {
            if (AfxMessageBox("The mag is the same as last time\nWas there really a mag "
              "step to record?", MB_YESNO | MB_ICONQUESTION) == IDNO) {
                magInd--;
                continue;
            }
          }
          str.Format("%d %.1f", magInd, curSMag);
          mWinApp->AppendToLog(str);
          lastSMag = curSMag;
        }
      }
      report.Format("Set LowestMicroSTEMmag %d\r\n  Also note the two indexes where"
        " non LM mags start,\r\n  and put both in a LowestSTEMnonLMmag property entry", 
        baseIndex);
      mWinApp->AppendToLog(report);
      if (!startingMode)
        PLUGSCOPE_SET(ProbeMode, imNanoProbe);
      PLUGSCOPE_SET(STEMMagnification, startingSMag);
      if (magInd >= MAX_MAGS) {
        report.Format("You need a new version of the program with MAX_MAGS set to %d\n\n"
          "Tell David!", magInd + 2);
        AfxMessageBox(report, MB_EXCLAME);
      }
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("accessing STEM magnifications "));
      retval = false;
    }
    return retval;
  }


  if (!JeolGIF) {
    startingMag = GetMagIndex(); 

    if (startingMag < 0) {
      TRACE("Error: ScanMagnifications: Couldn't get initial magnification!\n");
     // return false;
    }
  }

  ScopeMutexAcquire("ScanMagnifications", true);
  sScanningMags = true;
  mJeolParams.scanningMags = true;

  try {

    // Disable autonormalization
    if (LikeFEIscope) {
      AUTONORMALIZE_SET(*vFalse);
      numModes = 1;
    }

    for (int mode = 0; mode < numModes; mode++) {
      if (JEOLscope) {
        mPlugFuncs->SetImagingMode(functionModes[mode]);
      }
      lastMag = -1;
      for (magInd = 1; magInd < MAX_MAGS; magInd++) {
        if (JEOLscope) {
          if (magInd == mHighestLMindexToScan + 1 && 
            functionModes[mode] == JEOL_LOWMAG_MODE)
            break;

          // For JEOL, set the mag then get the mag directly from scope
          // It used to not change the mag past the index limit, then it started giving
          // an error after errors were being processed correctly.
          // This is a special call that will limit the retries just for this call.
          // When the call fails, it returns 1 if index is > 4, otherwise throws an error
          if (mPlugFuncs->CheckMagSelector(magInd - 1))
            break;
          magVal = (long)mPlugFuncs->GetMagnification();
          rot = 0;

          // MIT 2200 bug: reported 100 for 80, so it then stopped on real 100, index 4
          if (magVal == lastMag && magInd != 4)
            break;
          lastMag = magVal;

        } else { // FEI

          PLUGSCOPE_SET(MagnificationIndex, magInd);
          PLUGSCOPE_GET(MagnificationIndex, magSet, 1);
          if (magSet != magInd)
            break;
          PLUGSCOPE_GET(Magnification, curSMag, 1.);
          magVal = B3DNINT(curSMag);
          PLUGSCOPE_GET(ImageRotation, curSMag, 1.);
          rot = B3DNINT(curSMag / DTOR);
        }
        if (STEMmode)
          report.Format("%d %8d", magInd + baseIndex, magVal);
        else if (!JeolGIF && mode && mode == numModes - 1)
          report.Format("%d %8d", magInd, magVal);
        else if (FEIscope && GetEFTEM())
          report.Format("%d %8d %5d %8d %8d %5d", magInd, mMagTab[magInd].mag, 
           B3DNINT(mMagTab[magInd].tecnaiRotation), mMagTab[magInd].screenMag, magVal, rot);
        else
          report.Format("%d %8d %5d", magInd + baseIndex, magVal, rot);
         mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      }
      if (!mode || (numModes == 4 && mode == 1)) {
        if (!mode)
          firstBase = magInd;
        baseIndex += magInd - 1;
      } else if (STEMmode || mode == numModes - 2) {
        if (STEMmode)
          report.Format("Set LowestSTEMnonLMmag to %d",  baseIndex + 1);
        else if (numModes == 4) 
          report.Format("Set LowestMModeMagIndex to %d\r\nSet LowestSecondaryMag to %d"
          "\r\nCameraLengthTable:", firstBase, baseIndex + 1);
        else
          report.Format("Set LowestMModeMagIndex to %d\r\n\r\nCameraLengthTable:", 
            baseIndex + 1);
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      }
    }
    if (LikeFEIscope)
      AUTONORMALIZE_SET(*vTrue);
  }

  catch (_com_error E) {
    SEMReportCOMError(E, _T("scanning magnifications "));
    retval = false;
  }
  if (startingMag > 0)
    SetMagIndex(startingMag);
  ScopeMutexRelease("ScanMagnifications");
  sScanningMags = false;
  mJeolParams.scanningMags = false;
  return retval;
}

// Camera length fast and regular get routines
int CEMscope::FastCamLenIndex()
{
  FAST_GET(int, GetCamLenIndex);
}

int CEMscope::GetCamLenIndex()
{
  int result, subMode;
  if (!sInitialized)
    return -1;

  // Just call the mag index routine to get the length set in the state.  It will call
  // it as fast if possible.  Then the cam length index routine will return computed value
  if (JEOLscope)
    GetMagIndex();
  else
    ScopeMutexAcquire("GetCamLenIndex", true);
  try {
    PLUGSCOPE_GET(CameraLengthIndex, result, 1);
    if (FEIscope) {
      PLUGSCOPE_GET(SubMode, subMode, 1);
      if (subMode == psmLAD)
        result += LAD_INDEX_BASE;
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting camera length index "));
    result = -1;
  }
  if (!JEOLscope)
    ScopeMutexRelease("GetCamLenIndex");
  return result;
}

// Set the camera length index in diffraction mode
BOOL CEMscope::SetCamLenIndex(int inIndex)
{
  BOOL result = true;

  if (!sInitialized)
    return false;
  
  int magInd, currentIndex = GetCamLenIndex();
  if (currentIndex == inIndex)
    return true;
  ScopeMutexAcquire("SetCamLenIndex", true);
  if (!currentIndex)
    SaveISifNeeded(GetMagIndex(), 0);

  try {
    
    if (FEIscope) {  // FEI

      // If we are already in diffraction but in wrong mode, need to go to imaging mode
      if (currentIndex && (currentIndex - LAD_INDEX_BASE) * (inIndex - LAD_INDEX_BASE) 
        < 0) {
          if (mPlugFuncs->SetImagingMode)
            mPlugFuncs->SetImagingMode(pmImaging);
          currentIndex = 0;
      }

      // If in imaging mode, get mag index and see if it's the right mode
      if (!currentIndex) {
        PLUGSCOPE_GET(MagnificationIndex, magInd, 1);

        // Go to a mag in the right range if necessary then go to diffraction
        if (magInd < mLowestMModeMagInd && inIndex < LAD_INDEX_BASE) {
          PLUGSCOPE_SET(MagnificationIndex, mLowestMModeMagInd);
        } else if (magInd >= mLowestMModeMagInd && inIndex > LAD_INDEX_BASE) {
          PLUGSCOPE_SET(MagnificationIndex, mLowestMModeMagInd - 1);
        }
        if (mPlugFuncs->SetImagingMode)
          mPlugFuncs->SetImagingMode(pmDiffraction);
      }

      // Get right index for mode and set it
      if (inIndex > LAD_INDEX_BASE)
        inIndex -= LAD_INDEX_BASE;
      PLUGSCOPE_SET(CameraLengthIndex, inIndex);

    } else {   // Other Plugin
      if (!currentIndex && mPlugFuncs->SetImagingMode && !JEOLscope)
        mPlugFuncs->SetImagingMode(pmDiffraction);
      mPlugFuncs->SetCameraLengthIndex(inIndex);
    }
    PLUGSCOPE_GET(CameraLength, mLastCameraLength, 1.);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting camera length index "));
    result = false;
  }

  // Keep update from operating on the change
  SetMagChanged(0);
  HandleNewMag(0);
  ScopeMutexRelease("SetCamLenIndex");
  return result;
}

// Start calibration of neutral IS
void CEMscope::CalibrateNeutralIS(int calType)
{
  int startMag = 1;
  CString pane = "GETTING NEUTRAL IS";
  if (calType != CAL_NTRL_FIND)
    pane = (calType == CAL_NTRL_RESTORE) ? "RESTORING NEUTRAL IS" : "GETTING BASE FOCUS";
  mCalNeutralType = calType;
  mNeutralIndex = 0;
  if (!HitachiScope && mJeolSD.JeolEFTEM > 0) {
    startMag = mLowestMModeMagInd;
    mNeutralIndex = 1;
  }
  mCalNeutralStartMag = GetMagIndex();
  if (mCalNeutralStartMag < 0)
    return;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, pane);
  CalNeutralNextMag(startMag);
}

// Do one mag at a time so that the calibration can be stopped
void CEMscope::CalNeutralNextMag(int magInd)
{
  short ID = mJeolSD.usePLforIS ? 11 : 8;
  double oldISX, oldISY, newISX, newISY;
  HitachiParams *hParams = mWinApp->GetHitachiParams();

  if (mCalNeutralStartMag < 0)
    return;
  if (mMagTab[magInd].mag) {
    if (SetMagIndex(magInd)) {
      ScopeMutexAcquire("CalibrateNeutralIS", true);
      try {

        if (HitachiScope) {
          if (mCalNeutralType == CAL_NTRL_FOCUS) {
            hParams->baseFocus[magInd] = 
              B3DNINT(mPlugFuncs->GetAbsFocus() * HITACHI_LENS_MAX);

          } else if (mCalNeutralType == CAL_NTRL_RESTORE) {
            mPlugFuncs->SetImageShift(mMagTab[magInd].neutralISX[mNeutralIndex], 
              mMagTab[magInd].neutralISY[mNeutralIndex]);

          } else {
            mPlugFuncs->GetImageShift(&oldISX, &oldISY);
            SEMTrace('1', "%d Old values  %f %f   New values  %f %f", magInd,
            mMagTab[magInd].neutralISX[mNeutralIndex], 
            mMagTab[magInd].neutralISY[mNeutralIndex], oldISX, oldISY);
            mMagTab[magInd].neutralISX[mNeutralIndex] = (float)oldISX;
            mMagTab[magInd].neutralISY[mNeutralIndex] = (float)oldISY;
          }

        } else {

          // JEOL: Set to neutral and measure the IS
          if (!mJeol1230) {
            mPlugFuncs->SetToNeutral(ID);
          }

          mPlugFuncs->GetImageShift(&newISX, &newISY);

          oldISX = mMagTab[magInd].neutralISX[mNeutralIndex];
          oldISY = mMagTab[magInd].neutralISY[mNeutralIndex];
          mMagTab[magInd].neutralISX[mNeutralIndex] = (float)newISX;
          mMagTab[magInd].neutralISY[mNeutralIndex] = (float)newISY;
          SEMTrace('1', "%d Old values  %f %f   New values  %f %f", magInd,
            oldISX, oldISY, newISX, newISY);
        }
        if (mCalNeutralType != CAL_NTRL_RESTORE)
          mWinApp->SetCalibrationsNotSaved(true);
      }
      catch (_com_error E) {
        SEMReportCOMError(E, _T(HitachiScope ? "Calibrating base focus values" : 
          "calibrating neutral image shift "));
        magInd = MAX_MAGS;
      }
      ScopeMutexRelease("CalibrateNeutralIS");
    } else {
      magInd = MAX_MAGS;
    }
  } else {
    for (; magInd < MAX_MAGS; magInd++)
      if (mMagTab[magInd].mag)
        break;
  }

  // For Hitachi, skip over HC mags except when doing base focus
  if (HitachiScope && mCalNeutralType != CAL_NTRL_FOCUS && 
    magInd == mLowestMModeMagInd - 1 && mLowestSecondaryMag > 0)
    magInd = mLowestSecondaryMag - 1;

  // If not at end or error, set up for next mag
  if (magInd + 1 < MAX_MAGS) {
    mWinApp->AddIdleTask(NULL, TASK_CAL_IS_NEUTRAL, magInd + 1, 0);
    return;
  }

  // Visit lowest mag in HR mode so the scope will go to that mag when entering HR mode
  if (HitachiScope)
    SetMagIndex(mLowestSecondaryMag);
  StopCalNeutralIS();  
}

// Stop calibration of Neutral IS and reset windows
void CEMscope::StopCalNeutralIS()
{ 
  static bool stopping = false;
  if (mCalNeutralStartMag < 0 || stopping)
    return;
  stopping = true;
  SetMagIndex(mCalNeutralStartMag);
  mCalNeutralStartMag = -1;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  stopping = false;
}

// Call this from other routines that change the mag, to avoid having it
// be interpreted as a user change
void CEMscope::SetMagChanged(int inIndex)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  JeolStateData *JSDp = &winApp->mScope->mJeolSD;
  mMagChanged = false;
  mInternalPrevMag = mLastMagIndex;
  mLastMagIndex = inIndex;
  mInternalMagTime = GetTickCount();
  if (JEOLscope) {
    SEMAcquireJeolDataMutex();
    if (!JSDp->usePLforIS && !JSDp->JeolSTEM)
      JSDp->internalMagTime = mInternalMagTime;
    SEMReleaseJeolDataMutex();
  }
}

void CEMscope::SetISChanged(double inISX, double inISY)
{
  mLastISX = mPreviousISX = inISX;
  mLastISY = mPreviousISY = inISY;
}

/*
 * NORMALIZATION FUNCTIONS: first projector
 */
BOOL CEMscope::NormalizeProjector()
{
  BOOL result = true;
  bool unblankAfter;
  const char *routine = "NormalizeProjector";

  if (!sInitialized)
    return false;

  ScopeMutexAcquire(routine, true);

  try {
    // What to do for JEOL? Keep the normalization time in case whatever evoked
    // this call needs some settling anyway
    if (mPlugFuncs->NormalizeLens) {
      unblankAfter = BlankTransientIfNeeded(routine);
      mPlugFuncs->NormalizeLens(pnmProjector);
      UnblankAfterTransient(unblankAfter, routine);
    }
    mMagChanged = false;
    mLastNormalization = GetTickCount();
    mLastNormMagIndex = FastMagIndex();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("normalizing projection lenses "));
    result = false;
  }

  ScopeMutexRelease(routine);
  return result; 
}

// Normalize the condenser lenses
BOOL CEMscope::NormalizeCondenser()
{
  BOOL success = true;
  bool unblankAfter;
  const char *routine = "NormalizeCondenser";

  if (!sInitialized)
    return false;

  ScopeMutexAcquire(routine, true);

  try {
    // This is not available on the JEOL
    if (mPlugFuncs->NormalizeLens) {
      unblankAfter = BlankTransientIfNeeded(routine);
      mPlugFuncs->NormalizeLens(nmCondenser);
      UnblankAfterTransient(unblankAfter, routine);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("normalizing condenser lenses "));
    success = false;
  }
  ScopeMutexRelease(routine);
  return success;
}

// Normalize the Objective lens
BOOL CEMscope::NormalizeObjective()
{
  BOOL success = true;
  bool unblankAfter;
  const char *routine = "NormalizeObjective";

  if (!sInitialized)
    return false;

  ScopeMutexAcquire(routine, true);

  try {
    if (mPlugFuncs->NormalizeLens) {
      unblankAfter = BlankTransientIfNeeded(routine);
      mPlugFuncs->NormalizeLens(pnmObjective);
      UnblankAfterTransient(unblankAfter, routine);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("normalizing Objective lens "));
    success = false;
  }
  ScopeMutexRelease(routine);
  return success;
}

// Normalize all lenses; for FEI only
BOOL CEMscope::NormalizeAll(int illumProj)
{
  BOOL success = true;
  bool unblankAfter;
  const char *routine = "NormalizeAll";

  if (!sInitialized)
    return false;
  if (!FEIscope) {
    SEMMessageBox("The NormalizeAll function is available only for FEI scopes");
    return false;
  }

  ScopeMutexAcquire(routine, true);
  try {
    if (mPlugFuncs->NormalizeLens) {
      unblankAfter = BlankTransientIfNeeded(routine);
      if (!illumProj) {
        mPlugFuncs->NormalizeLens(ALL_INSTRUMENT_LENSES);
      } else {
        if (illumProj & 1)
          mPlugFuncs->NormalizeLens(nmAll);
        if (illumProj & 2)
          mPlugFuncs->NormalizeLens(pnmAll);
      }
      UnblankAfterTransient(unblankAfter, routine);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("normalizing all lenses "));
    success = false;
  }
  ScopeMutexRelease(routine);
  return success;
}

/* Return Defocus in Microns */
double CEMscope::GetDefocus()
{ 
  double result;

  if (!sInitialized)
    return 0.;

  // 11/30/10: There is no focus value by event! Eliminate forceread, use of stored value
  // Plugin recomputes it when getting fast
  bool needMutex = !(JEOLscope && mGettingValuesFast);
  if (needMutex)
    ScopeMutexAcquire("GetDefocus", true);

  try {
    PLUGSCOPE_GET(Defocus, result, 1.e6);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting defocus "));
    result = 0.;
  }
  if (needMutex)
    ScopeMutexRelease("GetDefocus");
  return result;
}

double CEMscope::FastDefocus(void)
{
  FAST_GET(double, GetDefocus);
}

BOOL CEMscope::IncDefocus(double inVal)
{
  return SetDefocus(GetDefocus() + inVal, true);
}

BOOL CEMscope::SetDefocus(double inVal, BOOL incremental)
{
  BOOL success = true;

  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetDefocus", true);

  try {
    PLUGSCOPE_SET(Defocus, inVal * 1.e-6);
    if (mPostFocusChgDelay > 0)
      Sleep(mPostFocusChgDelay);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting defocus "));
    success = false;
  }
  ScopeMutexRelease("SetDefocus");
  return success;
}

// Return the defocus, adjusted for an offset in low dose view mode
double CEMscope::GetUnoffsetDefocus()
{
  double defocus = GetDefocus();
  if (mLowDoseMode && mLowDoseSetArea == VIEW_CONSET && !mWinApp->GetSTEMMode())
    defocus -= mLDViewDefocus;
  else if (mLowDoseMode && mLowDoseSetArea == SEARCH_AREA && !mWinApp->GetSTEMMode())
    defocus -= mSearchDefocus;
  return defocus;
}

// Set defocus with a value that does not include an offset if in low dose view mode
BOOL CEMscope::SetUnoffsetDefocus(double inVal)
{
  if (mLowDoseMode && mLowDoseSetArea == VIEW_CONSET && !mWinApp->GetSTEMMode())
    inVal += mLDViewDefocus;
  else if (mLowDoseMode && mLowDoseSetArea == SEARCH_AREA && !mWinApp->GetSTEMMode())
    inVal += mSearchDefocus;
  return SetDefocus(inVal);
}

BOOL CEMscope::ResetDefocus(BOOL assumeInit) {

  BOOL success = true;

  if (!assumeInit && !sInitialized) 
    return false;

  ScopeMutexAcquire("ResetDefocus", true);

  if (FEIscope && mAdjustFocusForProbe && mFirstFocusForProbe > EXTRA_NO_VALUE)
    mFirstFocusForProbe -= GetDefocus();

  try {
    mPlugFuncs->ResetDefocus();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("resetting defocus "));
    success = false;
  }

  ScopeMutexRelease("ResetDefocus");
  return success;
}

// Return an absolute focus value appropriate for the mag mode
double CEMscope::GetFocus(void)
{
  double result;
  if (!sInitialized)
    return 0.;

  ScopeMutexAcquire("GetFocus", true);

  // Tecnai value between -1 and 1, varies in low mag while objective does not
  // Plugin originally defined as between 0 and 1, but JEOL gives number from 2-2050
  try {
    PLUGSCOPE_GET(AbsFocus, result, 1.);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Focus "));
    result = 0.;
  }
  ScopeMutexRelease("GetFocus");
  return result;
}

// Set an absolute focus value : put out a value that was gotten before
BOOL CEMscope::SetFocus(double inVal)
{
  BOOL success = true;

  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetFocus", true);

  try {
    PLUGSCOPE_SET(AbsFocus, inVal);
    if (mPostFocusChgDelay > 0)
      Sleep(mPostFocusChgDelay);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting focus "));
    success = false;
  }
  ScopeMutexRelease("SetFocus");
  return success;
}

BOOL CEMscope::SetObjFocus(int step)
{
  BOOL success = true;

  if (LikeFEIscope || !sInitialized)
    return false;

  ScopeMutexAcquire("SetObjFocus", true);

  try {
    mPlugFuncs->SetObjFocus(step);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting objective focus "));
    success = false;
  }
  ScopeMutexRelease("SetObjFocus");
  return success;
}

// Get a diffraction focus value
// POSSIBLY TEMPORARY: get IL1 for JEOL
double CEMscope::GetDiffractionFocus(void)
{
  double val;
  if (JEOLscope) {
    if (GetLensByName(CString("IL1"), val))
      return val;
    return 0.;
  } else
    return GetFocus();
}

// Set a diffraction focus value
BOOL CEMscope::SetDiffractionFocus(double inVal)
{
  BOOL success = false;
  if (!sInitialized)
    return false;

  if (!JEOLscope)
    return SetFocus(inVal);
  ScopeMutexAcquire("SetDiffractionFocus", true);
  try {
    if (mPlugFuncs->SetDiffractionFocus) {
      mPlugFuncs->SetDiffractionFocus(inVal);
      success = true;
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting diffraction focus "));
  }
  ScopeMutexRelease("SetDiffractionFocus");
  return success;
  
}

// Return a standard LM focus appropriate for an LM mag, and a focus in nonLM if in nonLM
double CEMscope::GetStandardLMFocus(int magInd, int probe)
{
  int i, dist, distmin = 1000, nearInd = -1;
  double focus = magInd >= mLowestMModeMagInd ? -999 : mStandardLMFocus;
  HitachiParams *hParams = mWinApp->GetHitachiParams();
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  for (i = 1; i < MAX_MAGS; i++) {
    if (mLMFocusTable[i][probe] > -900. &&
      (magInd >= mLowestMModeMagInd ? 1 : 0) == (i >= mLowestMModeMagInd ? 1 : 0)) {
      dist = i >= magInd ? i - magInd : magInd - i;
      if (dist < distmin) {
        distmin = dist;
        nearInd = i;
        focus = mLMFocusTable[nearInd][probe];
      }
    }
  }
  if (HitachiScope && focus != -999. && nearInd > 0)
    focus += (hParams->baseFocus[magInd] - hParams->baseFocus[nearInd]) / 
    (double)HITACHI_LENS_MAX;
  SEMTrace('c', "GetStandardLMFocus: mag %d, nearest mag %d, focus %f", magInd, nearInd,
    focus);
  return focus;
}

// Set the focus the the standard LM focus if this is LM and standard focus exists
void CEMscope::SetFocusToStandardIfLM(int magInd)
{
  if (mWinApp->GetSTEMMode() || magInd >= mLowestMModeMagInd)
    return;
  double focus = GetStandardLMFocus(magInd);
  if (focus > -900.)
    SetFocus(focus);
}

// Set up or end blanking mode when screen down in low dose
void CEMscope::SetBlankWhenDown(BOOL inVal)
{
  mBlankWhenDown = inVal;
  BlankBeam(NeedBeamBlanking(GetScreenPos(), GetSTEMmode()));
}

// Unblank or blank beam depending on camera acquire if in low dose mode and camera module
// is not handling the shutter (shutterless not 2) or if there is no shutter but camera
// module is just letting this function take care of shuttering (shutterless = 1)
void CEMscope::SetCameraAcquiring(BOOL inVal, float waitTime)
{
  int screenPos = FastScreenPos();
  int blankBefore = NeedBeamBlanking(screenPos, mWinApp->GetSTEMMode()) ? 1 : 0;
  mCameraAcquiring = inVal;
  if (blankBefore != (NeedBeamBlanking(screenPos, mWinApp->GetSTEMMode()) ? 1 : 0) ||
    inVal && sBeamBlanked) {
      BlankBeam(!inVal);
      if (waitTime >= 0.001)
        Sleep(B3DNINT(1000. * waitTime));
  }
}

// Set flag to maintain blanking for a shutterless camera
// Get the STEM argument to blanking routine from the camera properties since it may
// not be switched into STEM yet
void CEMscope::SetShutterlessCamera(int inVal)
{
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  mShutterlessCamera = inVal;
  BlankBeam(NeedBeamBlanking(GetScreenPos(), camParam->STEMcamera));
}

// Set mode, let Update figure out what to do
void CEMscope::SetLowDoseMode(BOOL inVal, BOOL hidingOffState)
{
  // If mode is being turned off, return to record parameters, unblank here
  if (mLowDoseMode && !inVal) {
    int screenPos = FastScreenPos();
    if (hidingOffState && screenPos == spDown)
      BlankBeam(true);
    GotoLowDoseArea(3);
    if (!hidingOffState && !NeedBeamBlanking(GetScreenPos(), FastSTEMmode()))
      BlankBeam(false);
    mLowDoseSetArea = -1;
  }
  mLowDoseMode = inVal;
}

// Blank or unblank the beam; keep track of requested setting
void CEMscope::BlankBeam(BOOL inVal)
{
  if (!sInitialized || mJeol1230)
    return;
  ScopeMutexAcquire("BlankBeam", true);
  try {
    PLUGSCOPE_SET(BeamBlank, VAR_BOOL(inVal));
    SEMTrace('B', "Set beam blank %s", inVal ? "ON" : "OFF");
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting beam blank "));
  }
  mBeamBlankSet = inVal;
  sBeamBlanked = inVal;
  ScopeMutexRelease("BlankBeam");
}

// Blanks beam for a routine creating transients if the flag is set, returns if it did
// This and the unblank function are to be called from with a try block
bool CEMscope::BlankTransientIfNeeded(const char *routine)
{   
  if (mBlankTransients && !sBeamBlanked && mPlugFuncs->SetBeamBlank != NULL) {
    mPlugFuncs->SetBeamBlank(*vTrue);
    sBeamBlanked = true;
    SEMTrace('B', "%s set beam blank ON", routine);
    return true;
  }
  return false;
}

// Unlbank after procedure if it was blanked
void CEMscope::UnblankAfterTransient(bool needUnblank, const char *routine)
{
  if (!needUnblank)
    return;
  mPlugFuncs->SetBeamBlank(*vFalse);
  sBeamBlanked = false;
  SEMTrace('B', "%s set beam blank OFF", routine);
}

// Change to a new low dose area
void CEMscope::GotoLowDoseArea(int inArea)
{
  double delISX, delISY, beamDelX, beamDelY;
  double curISX, curISY, newISX, newISY;
  int curAlpha;
  DWORD magTime;
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();
  LowDoseParams *ldArea = ldParams + inArea;
  BOOL filterChanged = false;
  BOOL alphaDone, ISdone = false;
  BOOL STEMmode = mWinApp->GetSTEMMode();
  bool fromFocTrial = mLowDoseSetArea == FOCUS_CONSET || mLowDoseSetArea == TRIAL_CONSET;
  bool toFocTrial = inArea == FOCUS_CONSET || inArea == TRIAL_CONSET;
  bool splitBeamShift;
  if (!sInitialized || mChangingLDArea || sClippingIS)
    return;

  mChangingLDArea = true;

  // Use designated params if set by nav helper, otherwise use set area params
  if (!mLdsaParams)
    mLdsaParams = ldParams + mLowDoseSetArea;
  splitBeamShift = !STEMmode && ((mLowDoseSetArea >= 0 && mLdsaParams->magIndex && 
    mLdsaParams->magIndex < mLowestMModeMagInd && ldArea->magIndex >= mLowestMModeMagInd)
    || ((mLowDoseSetArea < 0 || mLdsaParams->magIndex >= mLowestMModeMagInd) &&
    ldArea->magIndex && ldArea->magIndex < mLowestMModeMagInd));

  if (GetDebugOutput('L'))
    GetImageShift(curISX, curISY);
  if (GetDebugOutput('l')) {
    SEMTrace('l', "GotoLowDoseArea: %d: focus at start %.2f", inArea, GetDefocus());
    if (GetDebugOutput('b')) {
      GetBeamShift(curISX, curISY);
      GetBeamTilt(curISX, curISY);
    }
  }

  // Notify dialog so it can finish setting beam shift
  if (inArea != mLowDoseSetArea)
    mWinApp->mLowDoseDlg.AreaChanging(inArea);

  // Keep track of whether Focus is being reached after View
  if (mWinApp->mLowDoseDlg.SameAsFocusArea(inArea) && 
    !mWinApp->mLowDoseDlg.SameAsFocusArea(mLowDoseSetArea))
    mFocusCameFromView = mLowDoseSetArea == VIEW_CONSET;

  // If normalizing beam and we are not going to or from view area and intensity is 
  // changing and everything is defined, set beam for view area
  if (!STEMmode && mLDNormalizeBeam && inArea && mLowDoseSetArea > 0 && 
    (ldArea->spotSize != mLdsaParams->spotSize ||
    ldArea->intensity != mLdsaParams->intensity) &&
    ldArea->spotSize && ldParams[0].spotSize &&
    ldArea->intensity && ldParams[0].intensity) {

    SetSpotSize(ldParams[0].spotSize);
    SetIntensity(ldParams[0].intensity);
    if (mLDBeamNormDelay)
      Sleep(mLDBeamNormDelay);
  }

  // If we are not at the mag of the current area, just go back so the image shift gets
  // set appropriately before the coming changes
  if (mLowDoseSetArea >= 0 && FastMagIndex() != mLdsaParams->magIndex)
    SetMagIndex(mLdsaParams->magIndex);

  // Do image shift at higher mag for consistency in beam shifts, so do it now if
  // leaving a higher mag
  // However, when using PLA, do it at the mag of a trial/focus area when either leaving
  // or going to a trial/focus area.  The problem is that the PLA needed for area 
  // separation may be out of range at a higher mag where a bigger projector shift is 
  // needed for the given IS offset
  if (GetUsePLforIS(ldArea->magIndex) && ((fromFocTrial && !toFocTrial) || 
    (!fromFocTrial && toFocTrial)))
    ISdone = mLowDoseSetArea >= 0 && fromFocTrial;
  else
    ISdone = mLowDoseSetArea >= 0 && (ldArea->magIndex || ldArea->camLenIndex) &&
      ldArea->magIndex < mLdsaParams->magIndex;
  if (ISdone)
    DoISforLowDoseArea(inArea, mLdsaParams->magIndex, delISX, delISY);

  // If leaving view or search area, set defocus back first
  if (!STEMmode && mLDViewDefocus && mLowDoseSetArea == VIEW_CONSET && 
    inArea != VIEW_CONSET)
      IncDefocus(-(double)mLDViewDefocus);
  if (!STEMmode && mSearchDefocus && mLowDoseSetArea == SEARCH_AREA && 
    inArea != SEARCH_AREA)
      IncDefocus(-(double)mSearchDefocus);

  // Pull off the beam shift if leaving an area and going between LM and nonLM
  if (splitBeamShift && mLowDoseSetArea >= 0 && 
    (mLdsaParams->beamDelX || mLdsaParams->beamDelY))
      IncBeamShift(-mLdsaParams->beamDelX, -mLdsaParams->beamDelY);

  // Set probe mode if any.  This will simply save and restore IS across the change,
  // and impose any change in focus that has occurred in this mode unconditionally
  // and impose accumulated beam shifts from this mode after translating to new mode
  // First back off a beam shift; it didn't work to include that in the converted shift
  if (ldArea->probeMode >= 0) {
    if (mProbeMode != ldArea->probeMode) {
      if ((mLdsaParams->beamDelX || mLdsaParams->beamDelY) && 
        mProbeMode == mLdsaParams->probeMode && !splitBeamShift)
        IncBeamShift(-mLdsaParams->beamDelX, -mLdsaParams->beamDelY);
      SetProbeMode(ldArea->probeMode, true);
    }
  } else
    ldArea->probeMode = mProbeMode;

  // Mag and spot size will be set only if they change.  Do intensity unconditionally
  // If something is not initialized, set it up with current value
  // Don't set spot size on 1230 since it can't be read
  // Set spot before mag since JEOL mag change generates spot event
  if (ldArea->spotSize && !mJeol1230)
    SetSpotSize(ldArea->spotSize);
  else
    ldArea->spotSize = GetSpotSize();

  // Set alpha before mag if it is changing to a lower mag since scope can impose a beam
  // shift in the mag change that may depend on alpha  The reason for doing THIS at a 
  // higher mag is that there is hopefully only one mode at a lower mag, so the change
  // of alpha will always be able to happen in the upper mag range
  curAlpha = GetAlpha();
  alphaDone = curAlpha >= 0 && ldArea->beamAlpha >= 0. && !STEMmode && 
    mLowDoseSetArea >= 0 && (ldArea->magIndex || ldArea->camLenIndex) &&
    ldArea->magIndex < mLdsaParams->magIndex;
  if (alphaDone)
    ChangeAlphaAndBeam(curAlpha, (int)ldArea->beamAlpha);
  
  if (ldArea->magIndex)
    SetMagIndex(ldArea->magIndex);
  else if (ldArea->camLenIndex) {
    SetCamLenIndex(ldArea->camLenIndex);

    if (ldArea->diffFocus > -990.)
      SetDiffractionFocus(ldArea->diffFocus);
    else
      ldArea->diffFocus = GetDiffractionFocus();
  } else
    ldArea->magIndex = GetMagIndex();
  magTime = GetTickCount();

  // For FEI, set spot size (if it isn't right) again because mag range change can set 
  // spot size; afraid to just move spot size change down here because of note above
  if (!JEOLscope)
    SetSpotSize(ldArea->spotSize);

  // For alpha change, need to restore the beam shift to what it was for consistent
  // changes; and if there is a calibrated beam shift change, apply that
  if (ldArea->beamAlpha >= 0. && !STEMmode && !alphaDone) {
    ChangeAlphaAndBeam(curAlpha, (int)ldArea->beamAlpha);
  } else if (!STEMmode && !alphaDone)
    ldArea->beamAlpha = (float)curAlpha;

  // If going to view or search area, set defocus offset
  if (!STEMmode && mLDViewDefocus && mLowDoseSetArea != VIEW_CONSET && 
    inArea == VIEW_CONSET)
      IncDefocus((double)mLDViewDefocus);
  if (!STEMmode && mSearchDefocus && mLowDoseSetArea != SEARCH_AREA && 
    inArea == SEARCH_AREA)
      IncDefocus((double)mSearchDefocus);

  // If in EFTEM mode, synchronize to the filter parameters
  if (!STEMmode && mWinApp->GetFilterMode()) {
    if (ldArea->slitWidth) {
      filterChanged = ldArea->slitWidth != mFiltParam->slitWidth;
      mFiltParam->slitWidth = ldArea->slitWidth;
    } else
      ldArea->slitWidth = mFiltParam->slitWidth;

    if (mFiltParam->slitIn && !ldArea->slitIn ||
      !mFiltParam->slitIn && ldArea->slitIn)
      filterChanged = true;
    mFiltParam->slitIn = ldArea->slitIn;

    if (ldArea->energyLoss != mFiltParam->energyLoss)
      filterChanged = true;
    mFiltParam->energyLoss = ldArea->energyLoss;

    if (ldArea->zeroLoss && !mFiltParam->zeroLoss || 
      !ldArea->zeroLoss && mFiltParam->zeroLoss)
      filterChanged = true;
    mFiltParam->zeroLoss = ldArea->zeroLoss;
      
    // Send out values if any changed - need to set actual filter on the JEOL because
    // with an energy offset, it adjusts intensity
    // Yes, let camera set filter and this will put the proper delays in
    if (filterChanged) {
      mWinApp->mCamera->SetIgnoreFilterDiffs(true);
      if (JEOLscope)
        mWinApp->mCamera->SetupFilter();
      mWinApp->mFilterControl.UpdateSettings();
    }
  }
  
  // Now set intensity after all the changes JEOL might have imposed from spot or filter
  if (!STEMmode && ldArea->intensity) {
    if (!mWinApp->mMultiTSTasks->GetAcSetupDlgOpen())
      DelayedSetIntensity(ldArea->intensity, magTime);
  } else if (!STEMmode)
    ldArea->intensity = GetIntensity();

  // Shift image now after potential mag change
  if (!ISdone)
    DoISforLowDoseArea(inArea, ldArea->magIndex, delISX, delISY);

  // Do incremental beam shift if any; but if area is undefined, assume coming from Record
  if (!STEMmode) {
    if (mLowDoseSetArea < 0)
      mLdsaParams = ldParams + RECORD_CONSET;
    beamDelX = ldArea->beamDelX - mLdsaParams->beamDelX;
    beamDelY = ldArea->beamDelY - mLdsaParams->beamDelY;
    if (splitBeamShift || mLdsaParams->probeMode != ldArea->probeMode) {
      beamDelX = ldArea->beamDelX;
      beamDelY = ldArea->beamDelY;
    }
    if (beamDelX || beamDelY)
      IncBeamShift(beamDelX, beamDelY);

    // Do incremental beam tilt changes if enabled
    if (mLDBeamTiltShifts) {
      beamDelX = ldArea->beamTiltDX - mLdsaParams->beamTiltDX;
      beamDelY = ldArea->beamTiltDY - mLdsaParams->beamTiltDY;
      if (beamDelX || beamDelY)
        IncBeamTilt(beamDelX, beamDelY);
    }

    // Do absolute dark field beam tilt if it differs from current value
    if (ldArea->darkFieldMode != mLdsaParams->darkFieldMode || 
      fabs(ldArea->dfTiltX - mLdsaParams->dfTiltX) > 1.e-6 ||
      fabs(ldArea->dfTiltY - mLdsaParams->dfTiltY) > 1.e-6)
      SetDarkFieldTilt(ldArea->darkFieldMode, ldArea->dfTiltX, ldArea->dfTiltY);
  }

  if (GetDebugOutput('L'))
    GetImageShift(newISX, newISY);
  if (mLowDoseSetArea != inArea)
    SEMTrace('L', "LD: from %d to %d  was %.3f, %.3f  inc by %.3f, %.3f  now %.3f, %.3f",
      mLowDoseSetArea, inArea, curISX, curISY, delISX, delISY, newISX, newISY);
  if (GetDebugOutput('l')) {
    SEMTrace('l', "GotoLowDoseArea: focus at end %.2f", GetDefocus());
    if (GetDebugOutput('b')) {
      GetBeamShift(curISX, curISY);
      GetBeamTilt(curISX, curISY);
    }
  }

  // Set the area, save the polarity of this setting, and reset polarity
  mLowDoseSetArea = inArea;
  mLastLDpolarity = mNextLDpolarity;
  mNextLDpolarity = 1;
  mLdsaParams = NULL;
  mChangingLDArea = false;
}

// Get change in image shift; set delay; tell low dose dialog to ignore this shift
void CEMscope::DoISforLowDoseArea(int inArea, int curMag, double &delISX, double &delISY)
{
  double oldISX, oldISY, useISX = 0., useISY = 0.;
  float delay;
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();

  // IF using piezo for axis shift, do the movement
  delISX = delISY = 0.;
  if (mUsePiezoForLDaxis) {
    mWinApp->mLowDoseDlg.GoToPiezoPosForLDarea(inArea);
  }

  // Convert the image shift at the new area mag to the current mag unconditionally, 
  // or only for View area if using piezo
  if (!inArea || !mUsePiezoForLDaxis) {
    useISX = ldParams[inArea].ISX;
    useISY = ldParams[inArea].ISY;

    // If current area undefined, subtract off the Record IS to stay looking at same spot
    if (mLowDoseSetArea < 0) {
      useISX -= ldParams[RECORD_CONSET].ISX;
      useISY -= ldParams[RECORD_CONSET].ISY;
    }
    mShiftManager->TransferGeneralIS(ldParams[inArea].magIndex, mNextLDpolarity * useISX,
      mNextLDpolarity * useISY, curMag, delISX, delISY);
  }
  if (mLowDoseSetArea >= 0 && (!mLowDoseSetArea || !mUsePiezoForLDaxis)) {

    // Also convert the existing image shift to the current mag (which is one or the 
    // other), but also only for View if using piezo
    mShiftManager->TransferGeneralIS(mLdsaParams->magIndex,
      mLastLDpolarity * mLdsaParams->ISX, 
      mLastLDpolarity * mLdsaParams->ISY, curMag, oldISX, oldISY);
    delISX -= oldISX;
    delISY -= oldISY;
  }
  if (!delISX && !delISY)
    return;
  SEMTrace('l', "Changing IS for LD area change by %.3f, %.3f", delISX, delISY);
  IncImageShift(delISX, delISY);
  delay = ldParams[inArea].delayFactor * mShiftManager->GetLastISDelay();
  mShiftManager->SetISTimeOut(delay);
  mWinApp->mLowDoseDlg.SetIgnoreIS();
}

// Change alpha from old to new if it is different and change beam shift and tilt if
// the calibrations exist
void CEMscope::ChangeAlphaAndBeam(int oldAlpha, int newAlpha)
{
  double beamDelX = 0., beamDelY = 0., beamX, beamY;
  double tiltDelX = 0., tiltDelY = 0., tiltX, tiltY;
  if (oldAlpha < mNumAlphaBeamShifts && newAlpha < mNumAlphaBeamShifts) {
    beamDelX = mAlphaBeamShifts[newAlpha][0] - mAlphaBeamShifts[oldAlpha][0];
    beamDelY = mAlphaBeamShifts[newAlpha][1] - mAlphaBeamShifts[oldAlpha][1];
  }
  if (oldAlpha < mNumAlphaBeamTilts && newAlpha < mNumAlphaBeamTilts) {
    tiltDelX = mAlphaBeamTilts[newAlpha][0] - mAlphaBeamTilts[oldAlpha][0];
    tiltDelY = mAlphaBeamTilts[newAlpha][1] - mAlphaBeamTilts[oldAlpha][1];
  }
  if (newAlpha != oldAlpha) {
    GetBeamShift(beamX, beamY);
    GetBeamTilt(tiltX, tiltY);
    SEMTrace('l', "Changing alpha from %d to %d and setting beam", oldAlpha + 1, 
      newAlpha + 1);
    SetAlpha(newAlpha);
    SetBeamShift(beamX + beamDelX, beamY + beamDelY);
    SetBeamTilt(tiltX + tiltDelX, tiltY + tiltDelY);
    Sleep(mAlphaChangeDelay);
  }
}

// Set the area that should be shown when screen down; go there if low dose on
void CEMscope::SetLowDoseDownArea(int inArea)
{
  BOOL needBlank, gotoArea;
  needBlank = NeedBeamBlanking(FastScreenPos(), FastSTEMmode(), gotoArea);
  mLowDoseDownArea = inArea;
  if (gotoArea) {
    GotoLowDoseArea(inArea);
    if (!mWinApp->GetSTEMMode() && mHasOmegaFilter)
      mWinApp->mCamera->SetupFilter();
  }
}

BOOL CEMscope::GetIntensityZoom()
{
  BOOL result;

  // IntensityZoom (MagLink?) is not implemented for JEOL; setting will return false
  if (!sInitialized || JEOLscope || HitachiScope)
    return false;

  ScopeMutexAcquire("GetIntensityZoom", true);

  try {
    PLUGSCOPE_GET(IntensityZoom, result, 1);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting state of Intensity Zoom "));
    result = false;
  }
  ScopeMutexRelease("GetIntensityZoom");
  return result;
}

BOOL CEMscope::SetIntensityZoom(BOOL inVal)
{
  BOOL result;

  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetIntensityZoom", true);

  try {
    if (JEOLscope || HitachiScope) {
      TRACE("SetIntensityZoom is not implemented for JEOL or Hitachi microscopes.\n");
      //  assert(false);
      result = false;      // This should be detected where necessary
    } else {
      PLUGSCOPE_SET(IntensityZoom, inVal);
      result = true;
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting state of Intensity Zoom "));
    result = false;
  }
  ScopeMutexRelease("SetIntensityZoom");
  return result;
}

// Get the intensity (C2) value, return 0 for error which is not ideal
double CEMscope::GetIntensity()
{
  double result;

  if (!sInitialized)
    return 0.;
  if (mUseIllumAreaForC2) {
    result = GetIlluminatedArea();
    if (result < -998.)
      return 0.;
    return IllumAreaToIntensity(result);
  }

  //Plugin  uses state value if getting fast
  bool needMutex = !(JEOLscope && mGettingValuesFast);
  if (needMutex)
    ScopeMutexAcquire("GetIntensity", true);

  try {
    PLUGSCOPE_GET(Intensity, result, 1.);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting beam intensity "));
    result = 0.;
  }
  if (needMutex)
    ScopeMutexRelease("GetIntensity");
  return result;
}

// Get a fast intensity using update value from JEOL
double CEMscope::FastIntensity()
{
  FAST_GET(double, GetIntensity);
}

// Set the intensity (C2) value
BOOL CEMscope::SetIntensity(double inVal)
{
  BOOL result = true;

  if (!sInitialized)
    return false;

  if (mUseIllumAreaForC2)
    return SetIlluminatedArea(IntensityToIllumArea(inVal));

  ScopeMutexAcquire("SetIntensity", true);

  try {
    PLUGSCOPE_SET(Intensity, inVal);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting beam intensity "));
    result = false;
  }
  ScopeMutexRelease("SetIntensity");
  return result;
}

BOOL CEMscope::DelayedSetIntensity(double inVal, DWORD startTime)
{
  double elapsed = SEMTickInterval((double)startTime);
  if (elapsed < mMagChgIntensityDelay - 1.)
    Sleep((DWORD)(mMagChgIntensityDelay - elapsed));
  return SetIntensity(inVal);
}

double CEMscope::GetC2Percent(int spot, double intensity, int probe)
{
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  if (mUseIllumAreaForC2)
    return 100. * IntensityToIllumArea(intensity);
  return 100. * (intensity * mC2IntensityFactor[probe] + mC2SpotOffset[spot][probe]);
}

// Get the scaling factor and offsets for C2 values; probe defaults to -1 meaning use
// mProbeMode
float CEMscope::GetC2IntensityFactor(int probe)
{
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  return mC2IntensityFactor[probe];
}


float CEMscope::GetC2SpotOffset(int spotSize, int probe)
{
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  return mC2SpotOffset[spotSize][probe];
}

double CEMscope::GetCrossover(int spotSize, int probe) 
{
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  return mCrossovers[spotSize][probe];
}

// Get the illuminated area on Titan, return 0 for error which is not ideal
double CEMscope::GetIlluminatedArea()
{
  double result = -999.;
  if (!sInitialized || JEOLscope)
    return 0.;
  try {
    PLUGSCOPE_GET(IlluminatedArea, result, 1.e4);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting illuminated area "));
    result = 0.;
  }
  return result;
}

// Set the illuminated area on a Titan
BOOL CEMscope::SetIlluminatedArea(double inVal)
{
  BOOL result = false;
  if (!sInitialized || JEOLscope)
    return false;
  try {
    PLUGSCOPE_SET(IlluminatedArea, 1.e-4 * inVal);
    result = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting illuminated area "));
    result = false;
  }
  return result;
}

// Functions to get back and forth between illuminated area and internal intensity
double CEMscope::IllumAreaToIntensity(double illum)
{
  if (mIllumAreaHighLimit > mIllumAreaLowLimit)
    return (illum - mIllumAreaLowLimit) * (mIllumAreaHighMapTo - 
    mIllumAreaLowMapTo) / (mIllumAreaHighLimit - mIllumAreaLowLimit) + 
    mIllumAreaLowMapTo;
  return illum;
}

double CEMscope::IntensityToIllumArea(double intensity)
{
  if (mIllumAreaHighLimit > mIllumAreaLowLimit)
    return (intensity - mIllumAreaLowMapTo) * (mIllumAreaHighLimit - mIllumAreaLowLimit) /
      (mIllumAreaHighMapTo - mIllumAreaLowMapTo) + mIllumAreaLowLimit;
  return intensity;
}

// Convert an intensity if there is a known change in aperture
double CEMscope::IntensityAfterApertureChange(double intensity, int oldAper, int newAper)
{
  if (!mUseIllumAreaForC2 || !oldAper || !newAper || oldAper == newAper)
    return intensity;
  return IllumAreaToIntensity((newAper * IntensityToIllumArea(intensity)) / oldAper);
}

// Get the C3 - Image plane distance offset on Titan, return -999 for error
double CEMscope::GetImageDistanceOffset()
{
  double result = -999.;
  if (!sInitialized || !FEIscope)
    return -999.;
  try {
    PLUGSCOPE_GET(ImageDistanceOffset, result, 1.e4);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting image distance offset "));
    result = -999.;
  }
  return result;
}

// Set the C3 - Image plane distance offset on a Titan
BOOL CEMscope::SetImageDistanceOffset(double inVal)
{
  BOOL result = false;
  if (!sInitialized || !FEIscope)
    return false;
  try {
    PLUGSCOPE_SET(ImageDistanceOffset, 1.e-4 * inVal);
    result = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting image distance offset "));
    result = false;
  }
  return result;
}


// Get the spot size index
int CEMscope::GetSpotSize()
{
  int result;

  if (!sInitialized)
    return 0;

  // Plugin will use state value if update by event, getting fast, or Jeol 1230
  bool needMutex = !(JEOLscope && (mJeolSD.eventDataIsGood || mJeol1230 || 
    mGettingValuesFast));
  if (needMutex)
    ScopeMutexAcquire("GetSpotSize", true);

  try {
    PLUGSCOPE_GET(SpotSize, result, 1);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting spot size "));
    result = 0;
  }
  if (needMutex)
    ScopeMutexRelease("GetSpotSize");
  return result;
}

// Get a spot size quickly from update value on JEOL
int CEMscope::FastSpotSize()
{
  FAST_GET(int, GetSpotSize);
}

// Set the spot size with optional normalization
BOOL CEMscope::SetSpotSize(int inIndex, BOOL normalize)
{
  BOOL result = true;
  int curSpot = -1;
  bool fixBeam, unblankAfter;
  double startTime, beamXorig, beamYorig, curBSX, curBSY, startBSX, startBSY;
  const char *routine = "SetSpotSize";

  if (!sInitialized)
    return false;

  if (!mJeol1230)
    curSpot = GetSpotSize();
  if (curSpot == inIndex)
    return true;

  // If we have calibrated shift, get starting beam shift as basis for fix
  fixBeam = curSpot >= mMinSpotWithBeamShift[mSecondaryMode] && 
    curSpot <= mMaxSpotWithBeamShift[mSecondaryMode] &&
    inIndex >= mMinSpotWithBeamShift[mSecondaryMode] && 
    inIndex <= mMaxSpotWithBeamShift[mSecondaryMode];
  if (fixBeam)
    GetBeamShift(beamXorig, beamYorig);

  ScopeMutexAcquire(routine, true);

  try {
    unblankAfter = BlankTransientIfNeeded(routine);
     
    // Change the spot size.  If normalization requested, disable autonorm,
    // then later normalize, and set normalization time to give some delay
    if (normalize)
      AUTONORMALIZE_SET(*vFalse);

    // Hitachi hysteresis is ferocious.  Things are somewhat stable by always going up
    // from spot 1 instead of down.
    if (HitachiScope) {
      if (inIndex < curSpot) {
        for (int i = 1; i < inIndex; i++) {
          mPlugFuncs->SetSpotSize(i);
          Sleep(mHitachiSpotStepDelay);
        }
      }

      // Get beam shift before last spot change so we can look for change
      mPlugFuncs->GetBeamShift(&startBSX, &startBSY);
    }

    // Make final change to spot size
    PLUGSCOPE_SET(SpotSize, inIndex);

    // Wait for a beam shift to show up on Hitachi
    if (HitachiScope) {
      startTime = GetTickCount();
      while (1) {
        if (SEMTickInterval(startTime) > mHitachiSpotBeamWait) {
          SEMTrace('b', "Timeout waiting for beam shift change after spot change");
          break;
        }
        mPlugFuncs->GetBeamShift(&curBSX, &curBSY);
        if (curBSX != startBSX || curBSY != startBSY) {
          SEMTrace('b', "BS change seen in %.0f to %f %f", SEMTickInterval(startTime),
            curBSX, curBSX);
          break;
        }
        Sleep(30);
      }
    }

    // Then normalize if selected
    if (normalize) {
      if (mPlugFuncs->NormalizeLens)
        mPlugFuncs->NormalizeLens(nmSpotsize);
      AUTONORMALIZE_SET(*vTrue);
      mLastNormalization = GetTickCount();
    }
    UnblankAfterTransient(unblankAfter, routine);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting spot size "));
    result = false;
  }
  ScopeMutexRelease(routine);

  // Finally, call the routine to set the fixed beam shift
  if (result && fixBeam) {
    beamXorig += mSpotBeamShifts[mSecondaryMode][inIndex][0] - 
      mSpotBeamShifts[mSecondaryMode][curSpot][0];
    beamYorig += mSpotBeamShifts[mSecondaryMode][inIndex][1] - 
      mSpotBeamShifts[mSecondaryMode][curSpot][1];
    SEMTrace('b', "Fixing BS to %f %f", beamXorig, beamYorig);
    SetBeamShift(beamXorig, beamYorig);
  }
  return result;
}

// Get alpha value on JEOL, or -999
int CEMscope::GetAlpha(void)
{
  int result = -999;
  if (!sInitialized || mHasNoAlpha)
    return -999;

  // Plugin will use state value if updating by event or getting fast
  BOOL needMutex = mJeolSD.eventDataIsGood || mGettingValuesFast;
  if (needMutex)
    ScopeMutexAcquire("GetAlpha", true);

  try {
    result = mPlugFuncs->GetAlpha();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting alpha value "));
  }
  if (needMutex)
    ScopeMutexRelease("GetAlpha");
  return result;
}

// And a fast version
int CEMscope::FastAlpha(void)
{
  FAST_GET(int, GetAlpha);
}

// Set alpha value on JEOL
BOOL CEMscope::SetAlpha(int inIndex)
{
  BOOL result = true;
  if (!sInitialized || mHasNoAlpha)
    return false;

  ScopeMutexAcquire("SetAlpha", true);

  // It will check the current alpha
  try {
    mPlugFuncs->SetAlpha(inIndex);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting alpha value "));
    result = false;
  }
  ScopeMutexRelease("SetAlpha");
  return result;
}

// Get GIF mode on JEOL
int CEMscope::GetJeolGIF(void)
{
  int result = -1;
  if (!sInitialized || !JEOLscope)
    return -1;

  ScopeMutexAcquire("GetJeolGIF", true);

  try {
    result = mPlugFuncs->GetEFTEMMode() - 1;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting GIF mode "));
  }
  ScopeMutexRelease("GetJeolGIF");
  return result;
}

// Set GIF mode on JEOL
BOOL CEMscope::SetJeolGIF(int inIndex)
{
  BOOL result = true;
  if (!sInitialized || !JEOLscope)
    return false;

  ScopeMutexAcquire("SetJeolGIF", true);

  try {
    if (mPlugFuncs->GetEFTEMMode() != inIndex) {
      mPlugFuncs->SetEFTEMMode(inIndex + 1);
      if (mPostJeolGIFdelay > 0)
        Sleep(mPostJeolGIFdelay);
      if (mCanControlEFTEM) {
        SEMAcquireJeolDataMutex();
        mJeolSD.JeolEFTEM = inIndex;
        SEMReleaseJeolDataMutex();
      }
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting GIF mode "));
    result = false;
  }
  ScopeMutexRelease("SetJeolGIF");
  return result;
}

// Get Hitachi column mode
BOOL CEMscope::GetColumnMode(int &mode, int &subMode)
{
  BOOL result = true;
  int both;
  if (!sInitialized || !HitachiScope || !mPlugFuncs->GetSubMode)
    return false;
  ScopeMutexAcquire("GetColumnMode", true);
  try {
    both = mPlugFuncs->GetSubMode();
    mode = both >> 8;
    subMode = both & 0xFF;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting column mode "));
    result = false;
  }
  ScopeMutexRelease("GetColumnMode");
  return result;
}

// Get any lens by name from Hitachi or JEOL
BOOL CEMscope::GetLensByName(CString &name, double &value)
{
  BOOL result = true;
  if (!sInitialized || !mPlugFuncs->GetLensByName)
    return false;
  ScopeMutexAcquire("GetLensByName", true);
  try {
    value = mPlugFuncs->GetLensByName((LPCTSTR)name);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting lens by name "));
    result = false;
  }
  ScopeMutexRelease("GetLensByName");
  return result;
}

// Get any deflector coil by name from Hitachi
BOOL CEMscope::GetDeflectorByName(CString &name, double &valueX, double &valueY)
{
  BOOL result = true;
  if (!sInitialized || !HitachiScope || !mPlugFuncs->GetDeflectorByName)
    return false;
  ScopeMutexAcquire("GetDeflectorByName", true);
  try {
    mPlugFuncs->GetDeflectorByName((LPCTSTR)name, &valueX, &valueY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting deflector by name "));
    result = false;
  }
  ScopeMutexRelease("GetDeflectorByName");
  return result;
}

// Set free lens control on or off on JEOL for one or all lenses
BOOL CEMscope::SetFreeLensControl(int lens, int arg)
{
  BOOL result = true;
  if (!sInitialized || !mPlugFuncs->SetFreeLensControl)
    return false;
  ScopeMutexAcquire("SetFreeLensControl", true);
  try {
    mPlugFuncs->SetFreeLensControl(lens, arg);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting state/value of free lens control "));
    result = false;
  }
  ScopeMutexRelease("SetFreeLensControl");
  return result;
}

// Set one lens that has had free lens control turned on
BOOL CEMscope::SetLensWithFLC(int lens, double inVal, bool relative)
{
  BOOL result = true;
  if (!sInitialized || !mPlugFuncs->SetLensWithFLC)
    return false;
  ScopeMutexAcquire("SetLensWithFLC", true);
  try {
    mPlugFuncs->SetLensWithFLC(lens, relative ? 1 : 0, inVal);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting a lens under free lens control "));
    result = false;
  }
  ScopeMutexRelease("SetLensWithFLC");
  return result;
}

// Returns state of column valves or beam; 1 open/on, 0 closed, -1 for indeterminate
// and -2 for error or not supported
int CEMscope::GetColumnValvesOpen()
{
  int result = -2;
  if (!sInitialized || !mPlugFuncs->GetGunValve)
    return -2;
  ScopeMutexAcquire("GetBeamValve", true);

  try {
    result = mPlugFuncs->GetGunValve();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting column valves/gun state "));
  }
  ScopeMutexRelease("GetBeamValve");
  return result;
}


// Manipulate column valves
BOOL CEMscope::SetColumnValvesOpen(BOOL state)
{
  BOOL result = true;
  if (!sInitialized || (mNoColumnValve && state))
    return false;
  ScopeMutexAcquire("SetBeamValve", true);

  try {
    mPlugFuncs->SetGunValve(VAR_BOOL(state));
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting column valves "));
    result = false;
  }
  ScopeMutexRelease("SetBeamValve");
  return result;
}

// Determine if PVP is running on FEI
BOOL CEMscope::IsPVPRunning(BOOL & state)
{
  BOOL result = true;
  if (!sInitialized || !FEIscope)
    return false;
  ScopeMutexAcquire("IsPVPRunning", true);
  try {
    state = mPlugFuncs->GetPVPRunning();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("determining if PVP is running "));
    result = false;
  }
  ScopeMutexRelease("IsPVPRunning");
  return result;
}

// Get the accelerating voltage in KV
double CEMscope::GetHTValue()
{
  double result;
  if (mNoScope)
    return mNoScope;
  if (!sInitialized)
    return -1.;

  ScopeMutexAcquire("GetHTValue", true);

  try {
    result = mPlugFuncs->GetHighVoltage() / 1000.;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting accelerating voltage "));
    result = -1.;
  }
  ScopeMutexRelease("GetHTValue");
  return result;
}

// Functions for dealing with FEI autoloader
// Get the status of a slot; slots are apparently numbered from 1
BOOL CEMscope::CassetteSlotStatus(int slot, int &status)
{
  BOOL success = false;
  int numSlots = -999;
  int csstat; 
  status = -2;
  if (!sInitialized || !FEIscope)
    return false;
  try {
    numSlots = mPlugFuncs->NumberOfLoaderSlots();
    SEMTrace('W', "Number of slots is %d", numSlots);
    if (slot > 0 && slot <= numSlots) {
      csstat = mPlugFuncs->LoaderSlotStatus(slot);
      if (csstat == CassetteSlotStatus_Empty)
        status = 0;
      if (csstat == CassetteSlotStatus_Occupied)
        status = 1;
      if (csstat == CassetteSlotStatus_Unknown)
        status = -1;
      success = true;
    }
  }
  catch (_com_error E) {
    if (numSlots == -999)
      SEMReportCOMError(E, _T("getting number of autoloader cassette slots "));
    else
      SEMReportCOMError(E, _T("getting status of autoloader cassette slot "));
  }
  SEMTrace('W', "CassetteSlotStatus returning %s, status %d", 
    success ? "success" : "failure", status);
  return success;
}

int CEMscope::LoadCartridge(int slot)
{
  int oper = LONG_OP_LOAD_CART;
  float sinceLast = 0.;
  if (!sInitialized || !FEIscope)
    return 2;
  if (slot <= 0)
    return 2;
  sCartridgeToLoad = slot;
  return (StartLongOperation(&oper, &sinceLast, 1));
}

int CEMscope::UnloadCartridge(void)
{
  int oper = LONG_OP_UNLOAD_CART;
  float sinceLast = 0.;
  if (!sInitialized || !FEIscope)
    return 2;
  return (StartLongOperation(&oper, &sinceLast, 1));
}

// Functions for dealing with temperature control

// Dewars busy
bool CEMscope::AreDewarsFilling(BOOL &busy)
{
  int time;
  double level;
  return GetTemperatureInfo(0, busy, time, 0, level);
}

// Remaining time - units?
bool CEMscope::GetDewarsRemainingTime(int &time)
{
  BOOL busy;
  double level;
  return GetTemperatureInfo(1, busy, time, 0, level);
}

// refrigerant level - units?
bool CEMscope::GetRefrigerantLevel(int which, double &level)
{
  BOOL busy;
  int time;
  return GetTemperatureInfo(2, busy, time, which, level);
}

// Common function to reduce boilerplate
bool CEMscope::GetTemperatureInfo(int type, BOOL &busy, int &time, int which, 
                                  double &level)
{
  int levelNames[3] = {RefrigerantLevel_AutoloaderDewar, 
    RefrigerantLevel_ColumnDewar, RefrigerantLevel_HeliumDewar};
  const char *messages[3] = {"determining if dewars are busy ", 
    "determining remaining dewar time ", "determining refrigerant level "};
  bool success = false;
  if (!sInitialized || !FEIscope || type < 0 || type > 2)
    return false;
  try {
    if (!mPlugFuncs->TempControlAvailable()) {
      SEMMessageBox("Temperature control is not available on this system");
      return false;
    }
    if (type == 0) {
      busy = mPlugFuncs->DewarsAreBusyFilling();
    } else if (type == 1) {
      time = mPlugFuncs->DewarsRemainingTime();
    } else {
      B3DCLAMP(which, 1, 3);
      level = mPlugFuncs->GetRefrigerantLevel(levelNames[which - 1]);
    }
    success = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T(messages[type]));
  }
  return success;
}

// Set microprobe/nanoprobe on FEI
BOOL CEMscope::SetProbeMode(int micro, BOOL fromLowDose)
{
  BOOL success = true, ifSTEM;
  double ISX, ISY, beamX, beamY, toBeamX, toBeamY, delFocus;
  bool adjustFocus = (mAdjustFocusForProbe || fromLowDose) && 
          mFirstFocusForProbe > EXTRA_VALUE_TEST;
  bool adjustShift = false;
  int magInd;
  ScaleMat fromMat, toMat;

  if (!sInitialized)
    return false;

  if (ReadProbeMode() == (micro ? 1 : 0))
    return true;

  ScopeMutexAcquire("SetProbeMode", true);
  if (fromLowDose && mFirstBeamXforProbe > EXTRA_VALUE_TEST && FEIscope) {
    magInd = GetMagIndex();
    fromMat = mShiftManager->GetBeamShiftCal(magInd, -3333, micro ? 0 : 1);
    toMat = mShiftManager->GetBeamShiftCal(magInd, -3333, micro ? 1 : 0);
    adjustShift = toMat.xpx != 0. && fromMat.xpx != 0.;
  }

  try {
    // This is not available on the JEOL
    // In TEM mode, save and restore image shift and adjust focus if option is on
    if (FEIscope) {
      ifSTEM = GetSTEMmode();
      if (!ifSTEM) { 
        if (!GetImageShift(ISX, ISY))
          success = false;
        if (adjustFocus)
          delFocus = GetDefocus() - mFirstFocusForProbe;
        if (adjustShift) {
          if (GetBeamShift(beamX, beamY)) {
            beamX -= mFirstBeamXforProbe;
            beamY -= mFirstBeamYforProbe;
            toMat = MatMul(MatInv(fromMat), toMat);
            toBeamX = beamX * toMat.xpx + beamY * toMat.xpy;
            toBeamY = beamX * toMat.ypx + beamY * toMat.ypy;
            SEMTrace('b', "Probe change  first beam %.3f %.3f  net change %.3f %.3f  "
              "converted %.3f %.3f",
              mFirstBeamXforProbe, mFirstBeamYforProbe, beamX, beamY, toBeamX, toBeamY);
          } else
            success = false;
        }
        
      }
      PLUGSCOPE_SET(ProbeMode, (micro ? imMicroProbe : imNanoProbe));
      mProbeMode = micro ? 1 : 0;
      if (!ifSTEM) {
        mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostProbeDelay);
        if (success && !SetImageShift(ISX, ISY))
          success = false;
        if (adjustFocus)
          IncDefocus(delFocus);
        if (success && adjustShift && GetBeamShift(beamX, beamY)) {
          SEMTrace('b', "After, beam %.3f %.3f  change to %.3f %.3f", beamX, beamY, 
            beamX + toBeamX, beamY + toBeamY);
          if (!SetBeamShift(beamX + toBeamX, beamY + toBeamY))
            success = false;
        } else {
          success = false;
        }
        mFirstFocusForProbe = GetDefocus();
        if (!GetBeamShift(mFirstBeamXforProbe, mFirstBeamYforProbe))
          success = false;
      }
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting probe mode "));
    success = false;
  }
  ScopeMutexRelease("SetProbeMode");
  return success;
}

// Get microprobe/nanoprobe on FEI or return the default value for TEM/STEM on other scope
int CEMscope::ReadProbeMode(void)
{
  int retval = 1;
  if (!sInitialized)
    return 0;
  ScopeMutexAcquire("ReadProbeMode", true);
  if (FEIscope) {
    try {
      PLUGSCOPE_GET(ProbeMode, retval, 1);
      mProbeMode = retval;
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("getting probe mode "));
    }
  } else {
    retval = FastSTEMmode() ? 0 : 1;
    mProbeMode = retval;
  }
  ScopeMutexRelease("ReadProbeMode");
  return retval;
}

// Directly determine whether scope is in STEM mode
BOOL CEMscope::GetSTEMmode(void)
{
  BOOL result;
  if (!sInitialized || !mWinApp->ScopeHasSTEM())
    return false;

  // Plugin will use state value if getting fast
  bool needMutex = !(JEOLscope && mGettingValuesFast);
  if (needMutex)
    ScopeMutexAcquire("GetSTEMmode", true);
  try {
    result = mPlugFuncs->GetSTEMMode();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting TEM vs. STEM mode "));
    result = false;
  }
  if (needMutex)
    ScopeMutexRelease("GetSTEMmode");
  return result;
}

BOOL CEMscope::FastSTEMmode(void)
{
  FAST_GET(int, GetSTEMmode);
}

// Actually switch scope into or out of STEM mode and change mags as needed
bool CEMscope::SetSTEM(BOOL inState)
{
  bool result = true;
  int newMag = 0;
  int oldMag, curMag, toCam, fromCam;
  float toFactor, fromPixel, toPixel;
  double ISX, ISY;
  CameraParameters *camP = mWinApp->GetCamParams();
  short mode = inState ? 1 : 0;
  int regCam = mFiltParam->firstRegularCamera;
  if (regCam < 0)
    regCam = mFiltParam->firstGIFCamera;
  if (!sInitialized || !mWinApp->ScopeHasSTEM())
    return false;
  int curMode = GetSTEMmode() ? 1 : 0;
  int jeolSleep = mJeolSwitchSTEMsleep;
  if (!inState && mJeolSwitchTEMsleep)
    jeolSleep = mJeolSwitchTEMsleep;
  SEMTrace('g', "SetSTEM: requested %d, scope %d, mSelected %d", mode, 
    curMode, mSelectedSTEM ? 1 : 0);
  if (mode == curMode && mode == mSelectedSTEM)
    return true;

  // Continue if mSelected doesn't match to get probe mode set for FEI and to get
  // camera switching
  curMag = GetMagIndex();
  ScopeMutexAcquire("SetSTEM", true);

  try {
    mSelectedSTEM = -1;
    double defocus = inState ? mLastSTEMfocus : mLastRegularFocus;
    if (JEOLscope && !mWinApp->GetStartingProgram()) {
      SuspendUpdate(jeolSleep);
      USE_DATA_MUTEX(mJeolSD.suspendUpdate = true);
    }
    if (mode != curMode) {
      mPlugFuncs->SetSTEMMode(inState);
    }
    ScopeMutexRelease("SetSTEM");
    if (JEOLscope && !mWinApp->GetStartingProgram()) {
      SuspendUpdate(jeolSleep);
      Sleep(jeolSleep);
      USE_DATA_MUTEX(mJeolSD.suspendUpdate = false);
      GetImageShift(ISX, ISY);
    }

    // Wait until the mode actually shows up
    double startTime = GetTickCount();
    while (curMode != mode && SEMTickInterval(startTime) < 5000) {
      Sleep(100);
      curMode = (GetSTEMmode() ? 1 : 0);
    }

    // Set the probe mode after it is confirmed to be in the mode
    if (LikeFEIscope)
      PLUGSCOPE_SET(ProbeMode, ((!inState || mProbeMode) ? imMicroProbe :imNanoProbe));
 
    // Now analyze for pixel matching
    fromCam = inState ? regCam : mWinApp->GetFirstSTEMcamera();
    toCam = !inState ? regCam : mWinApp->GetFirstSTEMcamera();
    if (mWinApp->GetSTEMmatchPixel() && !mLowDoseMode && fromCam >= 0 && toCam >= 0) {
      fromCam = mActiveCamList[fromCam];
      toCam = mActiveCamList[toCam];
      oldMag = inState ? mLastRegularMag : mLastSTEMmag;
      toFactor = camP[toCam].matchFactor / camP[fromCam].matchFactor;
      FindMatchingMag(fromCam, toCam, toFactor, oldMag, curMag, newMag, fromPixel, 
          toPixel, mSTEMfromMatchMag, mSTEMtoMatchMag, true);
      SEMTrace('g', "Match from %d  %f  to %d  %f", oldMag, fromPixel, newMag, toPixel);
      if (newMag)
        SetMagIndex(newMag);
      mLastMagIndex = newMag;   // Hope this doesn't screw something up
    }

    // Tecnai needs focus reasserted only going to STEM, JEOL simulater needs both ways
    // but plugin takes care of that
    if (defocus > -1.e9)
      SetDefocus(defocus);

    mSelectedSTEM = inState ? 1 : 0;
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mSTEMswitchDelay);
    SEMTrace('g', "SetSTEM: mode is set to %d", mSelectedSTEM);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting STEM/TEM mode "));
    ScopeMutexRelease("SetSTEM");
    result = false;
  }
  return result;
}

// This is unused 12/19/05 so no problem.  Still unused 10/27/11
BOOL CEMscope::GetEFTEM()
{
  BOOL result;
  int lvalue;
  if (JEOLscope)
    return mJeolSD.JeolEFTEM > 0;
  
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("GetEFTEM", true);
  
  try {
    PLUGSCOPE_GET(EFTEMMode, lvalue , 1);
    result = (lvalue == lpEFTEM);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting lens program "));
    result = false;
  }
  ScopeMutexRelease("GetEFTEM");
  return result;
}

// Actually switch to scope into or out of EFTEM mode and change mags as needed
BOOL CEMscope::SetEFTEM(BOOL inState)
{
  BOOL result;
  int newMag = 0;
  int oldMag, curMag, toCam, fromCam, enumVal, regCam, lvalue;
  float toFactor, fromPixel, toPixel;
  double ratio, factor, newIntensity;
  BOOL savedIntensityZoom = false;
  BOOL regIsSTEM = false, mismatch = false;

  if (!sInitialized)
    return false;

  oldMag = GetMagIndex();
  if (oldMag < 0)
    return false;
  curMag = oldMag;
  
  ScopeMutexAcquire("SetEFTEM", true);

  try {

    if (JEOLscope) {
      // For JEOL, use the last recorded mag in the old state when switching states
      int mag = inState ? mLastRegularMag : mLastEFTEMmag;
      if (mag)
        oldMag = mag;
    } else {  // FEI-like
      enumVal = inState ? lpEFTEM : lpRegular;
      PLUGSCOPE_GET(EFTEMMode, lvalue , 1);
      mismatch = lvalue != enumVal;
    }

    SEMTrace('g', "SetEFTEM: lens program mismatch %d, previous state %d, new state %d",
      mismatch ? 1 : 0, mSelectedEFTEM ? 1 : 0, inState ? 1 : 0);
    // Evaluate the following changes if the lens program doesn't match or if
    // the current state of the program does not match the desired state
    if (mismatch || (!inState && mSelectedEFTEM) || (inState && !mSelectedEFTEM)) {

      // If both kinds of cameras exist and either pixel matching or intensity
      // matching are needed, get pixel sizes
      regCam = mFiltParam->firstRegularCamera;
      if (regCam < 0) {
        regCam = mWinApp->GetFirstSTEMcamera();
        regIsSTEM = true;
        if (inState)
          oldMag = mLastSTEMmag;
      }
      fromCam = inState ? regCam : mFiltParam->firstGIFCamera;
      toCam = !inState ? regCam : mFiltParam->firstGIFCamera;
      if (((mFiltParam->matchPixel || mFiltParam->matchIntensity) && !mLowDoseMode) &&
        fromCam >= 0 && toCam >= 0) {
        fromCam = mActiveCamList[fromCam];
        toCam = mActiveCamList[toCam];
        toFactor = inState ? mFiltParam->binFactor : 1.f / mFiltParam->binFactor;
        FindMatchingMag(fromCam, toCam, toFactor, oldMag, curMag, newMag, fromPixel, 
          toPixel, mFromPixelMatchMag, mToPixelMatchMag, mFiltParam->matchPixel);
        if (mFiltParam->matchPixel)
          SEMTrace('g', "SetEFTEM: from old mag %d, cur mag %d, to mag %d", oldMag, 
            curMag, newMag);

        // Now change intensity if that option is selected and it is not a STEM camera
        if (mFiltParam->matchIntensity && !mLowDoseMode && !regIsSTEM) {
          ratio = (fromPixel * fromPixel) / (toPixel * toPixel);
          if (!mWinApp->mBeamAssessor->AssessBeamChange(ratio, newIntensity, 
            factor, -1)) {
              savedIntensityZoom = GetIntensityZoom();
              if (savedIntensityZoom)
                SetIntensityZoom(false);
              SetIntensity(newIntensity);
              SEMTrace('g', "SetEFTEM: Changing intensity to %.5f", newIntensity);
          }
        }
      }

      if (JEOLscope) {
        if (mCanControlEFTEM)
          SetJeolGIF(inState ? 1 : 0);
        if (newMag) {
          SetMagIndex(newMag);
          mLastMagIndex = newMag;
        }
        if (!mCanControlEFTEM) {
          SEMAcquireJeolDataMutex();
          mJeolSD.JeolEFTEM = inState ? 1 : 0;
          SEMReleaseJeolDataMutex();
        }

      } else {
        if (mCanControlEFTEM) {

          // Disable autonormalization, change mode, normalize and reenable
          AUTONORMALIZE_SET(*vFalse);
          PLUGSCOPE_SET(EFTEMMode, enumVal);
          if (newMag) {
            PLUGSCOPE_SET(MagnificationIndex, newMag);
            mLastMagIndex = newMag;   // avoid thinking its a user change
          }
          if (mPlugFuncs->NormalizeLens)
            mPlugFuncs->NormalizeLens(pnmProjector);
          AUTONORMALIZE_SET(*vTrue);
        }
      }

      // If intensity zoom was disabled, reenable it
      if (savedIntensityZoom)
        SetIntensityZoom(true);
      
      // set so that normalizations will not be repeated
      mMagChanged = false;
      mLastNormalization = GetTickCount();
    }

    // Inform filter control about the new state and possible mag
    HandleNewMag(newMag ? newMag : curMag);
    mSelectedEFTEM = inState;
    result = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting lens program "));
    result = false;
  }
  ScopeMutexRelease("SetEFTEM");
  return result;
}

// Find matching mag if doMatchPixel is true, or in any case find the from and to pixel
// sizes.  Supply fromCam, toCam, toFactor, oldMag, and curMag.  fromMatchMag and 
// toMatchMag should be member variables for going back to the same mag.
void CEMscope::FindMatchingMag(int fromCam, int toCam, float toFactor, int oldMag, 
                               int curMag, int &newMag, float &fromPixel, float &toPixel, 
                               int &fromMatchMag, int &toMatchMag, BOOL doMatchPixel)
{
  int mag, iDir, limlo, limhi, lowestFrom, lowestTo;
  float pixel;
  double diff, diffMin;
  CameraParameters *camP = mWinApp->GetCamParams();
  BOOL toSTEM = camP[toCam].STEMcamera;

  // Start with a mag at the same place relative to the LM-M boundary and keep it
  // within range for the new camera
  lowestFrom = GetLowestNonLMmag(&camP[fromCam]);
  lowestTo = GetLowestNonLMmag(&camP[toCam]);
  mWinApp->GetMagRangeLimits(toCam, lowestTo, limlo, limhi);
  newMag = curMag + lowestTo - lowestFrom;
  B3DCLAMP(newMag, limlo, limhi);
  fromPixel = mShiftManager->GetPixelSize(fromCam, oldMag);
  toPixel = toFactor * mShiftManager->GetPixelSize(toCam, newMag);
  diffMin = fabs((double)(fromPixel - toPixel));
  if (doMatchPixel) {

    // If we switched to this mag, switch back to one we came from
    if (toMatchMag == oldMag) {
      newMag  = fromMatchMag;
      toPixel = toFactor * mShiftManager->GetPixelSize(toCam, newMag);
    } else {

      // otherwise search nearby mags for closest pixel size
      curMag = newMag;

      for (iDir = -1; iDir <= 1; iDir += 2) {
        // Loop in one direction, until a bigger difference is found
        // Need to start from curMag since that is where first diff evaluated
        for (mag = curMag + iDir; mag >= limlo && mag <= limhi; mag += iDir) {

          // Skip if no mag is defined or if candidate is not on same side
          // of LM-M boundary
          if ((!toSTEM && !mMagTab[mag].mag) || (toSTEM && !mMagTab[mag].STEMmag) || 
            !BothLMorNotLM(oldMag, camP[fromCam].STEMcamera, mag, toSTEM))
            continue;

          pixel = toFactor * mShiftManager->GetPixelSize(toCam, mag);
          diff = fabs((double)(fromPixel - pixel));
          if (diff < diffMin) {
            toPixel = pixel;
            newMag = mag;
            diffMin = diff;
          } else
            break;
        }
      }
    }
    toMatchMag = newMag;
    fromMatchMag = oldMag;
  }
}

// Handle changing mag and intensity when matching between non-GIF cameras
void CEMscope::MatchPixelAndIntensity(int fromCam, int toCam, float toFactor,
                                      BOOL matchPixel, BOOL matchIntensity)
{
  int oldMag, newMag;
  float fromPixel, toPixel;
  double ratio, factor, newIntensity;
  oldMag = GetMagIndex();
  FindMatchingMag(fromCam, toCam, toFactor, oldMag, oldMag, newMag, fromPixel, 
    toPixel, mNonGIFfromMatchMag, mNonGIFtoMatchMag, matchPixel);
  if (!newMag)
    return;
  if (matchPixel)
    SetMagIndex(newMag);
  if (matchIntensity) {
     ratio = (fromPixel * fromPixel) / (toPixel * toPixel);
     if (!mWinApp->mBeamAssessor->AssessBeamChange(ratio, newIntensity, factor, -1))
       SetIntensity(newIntensity);
  }
}

// This routine is the common pathway when changing to a new mag or when the mag
// is discovered to be changed.  It informs filter control of the change and lens state
void CEMscope::HandleNewMag(int inMag)
{
  BOOL lensProg;
  int lvalue;
  try {
    if (JEOLscope) 
      lensProg = mHasOmegaFilter || mJeolSD.JeolEFTEM > 0; //mWinApp->GetEFTEMMode();
    else {  // FEI-like
      PLUGSCOPE_GET(EFTEMMode, lvalue, 1);
      lensProg = lvalue == lpEFTEM;
    }
    if (mWinApp->mFilterControl)
      mWinApp->mFilterControl.NewMagUpdate(inMag, lensProg);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting lens program "));
  }
  mHandledMag = inMag;
}


/* If we attempt to acquire a mutex
   that is already owned by this thread, then the acquisition will succeed
   immediately. In
   contrast, I think a POSIX mutex would deadlock if you tried to acquire it 
   twice.  
   
   The following ScopeMutexAcquire and ScopeMutexRelease functions 
   implement a sort of semaphore.   A given thread is allowed to acquire the
   mutex multiple times.  To release the mutex, the release function must be
   called as many times as the Acquire function was called. 

   (It turns out that this is basically the inherent behavior of mutexes in
    Win32, but I was thinking that mutex acquisition was idempotent.. so, much
    of the following code is unnecessary. oh well.)

   For more information, see:

     http://groups.google.com/groups?th=ee6b8570b2dd8f34

   The issue this is designed to correct is described here:

     http://www.livejournal.com/users/nibot_lab/20966.html

         It turns out that was only a problem because I was checking the name
         of the function that acquired the mutex.  Without that, it would have
         been fine.

   Resolution:

     http://www.livejournal.com/users/nibot_lab/21300.html
*/

BOOL CEMscope::ScopeMutexAcquire(const char *name, BOOL retry) {
  DWORD mutex_status;
  
  if (!mScopeMutexHandle)
    return true;

  if (mScopeMutexOwnerId == GetCurrentThreadId()) {
  //  if (TRACE_MUTEX)
  //    TRACE("This thread already had the mutex.\n");
    mScopeMutexOwnerCount ++;
    return true;
  }

  if (TRACE_MUTEX) SEMTrace('1', "%s waiting for Mutex",name);
  
  // This wait does not seem to allow enough time for threads to run properly, so set
  // the timeout low when not retrying.  It should loop and sleep instead...
  while ((mutex_status = WaitForSingleObject(mScopeMutexHandle, 
    retry ? MUTEX_TIMEOUT_MS : 20)) == WAIT_TIMEOUT) {
    if (retry) {
      if (TRACE_MUTEX) 
        SEMTrace('1', "%s timed out getting mutex, currently held by %s.. trying again.",
        name, mScopeMutexOwnerStr);
      continue;
    } else {
      if (TRACE_MUTEX) 
        SEMTrace('1', "%s failed to get the mutex, currently held by %s. giving up!", 
        name, mScopeMutexOwnerStr);
      return false;
    }
    assert(false);
  }

  /* Make sure it really did work. */

  if (mutex_status == WAIT_FAILED) {
    SEMTrace('1', "Mutex error: %s",GetLastError());
  }
  assert(mutex_status == WAIT_OBJECT_0 || mutex_status == WAIT_ABANDONED);

  /* Consistency Check */

  assert(mScopeMutexOwnerCount == 0 || mutex_status == WAIT_ABANDONED);
  mScopeMutexOwnerCount = 1;

  /* Now we're sure that we have the mutex. */

  strcpy(mScopeMutexOwnerStr,name);
  mScopeMutexOwnerId = GetCurrentThreadId();
  if (TRACE_MUTEX) SEMTrace('1', "%s got Mutex",mScopeMutexOwnerStr);   

  return true;
}


BOOL CEMscope::ScopeMutexRelease(const char *name) {
  
  if (!mScopeMutexHandle)
    return true;

  /* Make sure this thread really owns the Mutex */

  assert(mScopeMutexOwnerId == GetCurrentThreadId());

  mScopeMutexOwnerCount --;

  if (mScopeMutexOwnerCount) {
    //if (TRACE_MUTEX) 
    //  SEMTrace('1', "%s released the mutex, but it is not the primary owner\n",name);
    return true;
  } 

  if ( TRACE_MUTEX && (0 != strcmp(mScopeMutexOwnerStr,name))) {
    SEMTrace('1', "%s tried to release the mutex, but the mutex is held by %s!",name,mScopeMutexOwnerStr);
  }
  assert(0 == strcmp(mScopeMutexOwnerStr, name));
  if (TRACE_MUTEX) SEMTrace('1', "%s released the Mutex.",name);

  strcpy(mScopeMutexOwnerStr,"NOBODY"); 
  mScopeMutexOwnerId = 0;

  ReleaseMutex(mScopeMutexHandle);
  return true;
}

// Returns FALSE when the mag mutex could not be acquired
BOOL CEMscope::AcquireMagMutex()
{
  return WaitForSingleObject(sMagMutexHandle, MAG_MUTEX_WAIT) != WAIT_TIMEOUT;
}
void CEMscope::ReleaseMagMutex()
{
  ReleaseMutex(sMagMutexHandle);
}

// Round a stage coordinate in microns to the given precision, convert to nanometers,
// and return rounded value plus remainder in nanometers
float CEMscope::ConvertJEOLStage(double inMove, float &outRem)
{
  float rounded = sJEOLstageRounding * (int)floor(inMove / sJEOLstageRounding + 0.5);
  if (sJEOLpiezoRounding)
    outRem = 1.e3f * sJEOLpiezoRounding * 
      (int)floor((inMove - rounded) / sJEOLpiezoRounding + 0.5);
  else
    outRem = 0.;
  return 1.e3f * rounded;
}

void CEMscope::SetJeolStageRounding(float inVal)
{
  sJEOLstageRounding = inVal;
}
void CEMscope::SetJeolPiezoRounding(float inVal)
{
  sJEOLpiezoRounding = inVal;
}

void CEMscope::SetBlankingFlag(BOOL state)
{
  sBeamBlanked = state;
}


void CEMscope::WaitForStageDone(StageMoveInfo *smi, char *procName)
{
  double sx, sy, sz;
  int i, ntry = 10, ntryReady = 300;
  int waitTime = 100;
  JeolStateData *jsd = smi->JeolSD;

  if (FEIscope)
    return;

  // If updating by event, first wait to see it go non-ready, using the ready state flag
  if (JEOLscope && jsd->eventDataIsGood) {
    smi->plugFuncs->GetValuesFast(1);
    for (i = 0; i < ntry; i++) {
      if (smi->plugFuncs->GetStageStatus())
        break;
      if (i < ntry - 1) {
        ScopeMutexRelease(procName);
        Sleep(waitTime);
        ScopeMutexAcquire(procName, true);
      }
    }

    // If not ready was seen, wait on ready flag to avoid load on system,
    // then fall back to getting status for safety's sake
    if (i < ntry) {
      for (i = 0; i < ntryReady; i++) {
        if (!smi->plugFuncs->GetStageStatus())
          break;
        ScopeMutexRelease(procName);
        Sleep(waitTime);
        ScopeMutexAcquire(procName, true);
      }
      if (i == ntryReady)
        SEMTrace('S', "WaitForStageDone: Ready event not seen in %d ms", 
          ntryReady * waitTime);

    } else
      SEMTrace('S', "WaitForStageDone: Not ready event not seen in %d ms", 
        (ntry - 1) * waitTime);
    smi->plugFuncs->GetValuesFast(0);
  }

  // If no event update or didn't see busy, or events never came through,
  // check status until it clears
  while (1) {
    try {
      if (!SEMCheckStageTimeout() && !smi->plugFuncs->GetStageStatus())
        break;
    }
    catch (_com_error E) {

    }
    ScopeMutexRelease(procName);
    Sleep((JEOLscope ? 1 : 3) * waitTime);
    ScopeMutexAcquire(procName, true);
  }

  // Get actual position now and update state
  if (JEOLscope) {
    try {
      smi->plugFuncs->GetStagePosition(&sx, &sy, &sz);
    }
    catch (_com_error E) {
    }
    USE_DATA_MUTEX(jsd->stageReady = true);
  }
}

BOOL CEMscope::SetMDSMode(int which)
{
  BOOL retval = true;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetMDSMode", true);
  try {
    mPlugFuncs->SetMDSMode(which);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("Setting MDS mode "));
    retval = false;
  }
  
  ScopeMutexRelease("SetMDSMode");
  return retval;
}

int CEMscope::GetMDSMode()
{
  int mode;
  if (!sInitialized)
    return -2;

  ScopeMutexAcquire("GetMDSMode", true);
  try {
    mode = mPlugFuncs->GetMDSMode();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("Getting MDS mode "));
    mode = -1;
  }
  ScopeMutexRelease("GetMDSMode");
  return mode;
}

BOOL CEMscope::SetSpectroscopyMode(int which)
{
  BOOL retval = false;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetSpectroscopyMode", true);
  try {
    mPlugFuncs->SetSpectroscopyMode(which ? 1 : 0);
    retval = true;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("Setting spectroscopy mode "));
  }
  ScopeMutexRelease("SetSpectroscopyMode");
  return retval;
}

int CEMscope::GetSpectroscopyMode()
{
  short mode;
  if (!sInitialized)
    return -2;

  ScopeMutexAcquire("GetSpectroscopyMode", true);
  try {
    mode = mPlugFuncs->GetSpectroscopyMode();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("Getting Spectroscopy mode "));
    mode = -1;
  }
  ScopeMutexRelease("GetSpectroscopyMode");
  return mode;
}


int CEMscope::LimitJeolImageShift(long &jShiftX, long &jShiftY)
{
  CString message;
  int lowLim = 0;
  int hiLim = 0xffff;
  // For testing, use Tietz camera to get progressive shifts
  //int lowLim = 0x5000;
  //int hiLim = 0xB000;
  
  if (jShiftX >= lowLim && jShiftX <= hiLim && jShiftY >= lowLim && jShiftY <= hiLim)
    return 0;
  if (sMessageWhenClipIS) {
    sClippingIS = true;
    message.Format("The image shift values, %d  %d, are out of range and are being "
      "clipped\n\nTracking may be incorrect - you should reset image shift and check "
      "image position", jShiftX, jShiftY);
    AfxMessageBox(message, MB_EXCLAME);
    sClippingIS = false;
    SEMErrorOccurred(1);
  }

  SEMTrace('1', "Image shift 0x%X  0x%X out of range, being clipped to limits",
    jShiftX, jShiftY);

  B3DCLAMP(jShiftX, lowLim, hiLim);
  B3DCLAMP(jShiftY, lowLim, hiLim);
  return 1;
}

BOOL CEMscope::GetClippingIS()
{
  return sClippingIS;
}

// This replaces LimitJeolImageShift - the plugin calls this when it happens
void SEMClippingJeolIS(long &jShiftX, long &jShiftY)
{
  CString message;
  if (sMessageWhenClipIS) {
    sClippingIS = true;
    message.Format("The image shift values, %d  %d, are out of range and are being "
      "clipped\n\nTracking may be incorrect - you should reset image shift and check "
      "image position", jShiftX, jShiftY);
    AfxMessageBox(message, MB_EXCLAME);
    sClippingIS = false;
    SEMErrorOccurred(1);
  }

  SEMTrace('1', "Image shift 0x%X  0x%X out of range, being clipped to limits",
    jShiftX, jShiftY);
}

// Set the stage timeout after a mag change if needed
void SEMSetStageTimeout()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  JeolStateData *JSDp = &winApp->mScope->mJeolSD;
  if (!JSDp->postMagStageDelay)
    return;
  SEMAcquireJeolDataMutex();
  JSDp->blockStage = true;
  JSDp->stageTimeout = (double)GetTickCount();
  SEMReleaseJeolDataMutex();
}

// Check whether the stage timeout has expired and release it
BOOL SEMCheckStageTimeout()
{
  BOOL retval;
  double diff;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  JeolStateData *JSDp = &winApp->mScope->mJeolSD;
  if (!JEOLscope)
    return false;
  SEMAcquireJeolDataMutex();
  if (JSDp->blockStage) {
    diff = SEMTickInterval(JSDp->stageTimeout);
    if (diff >= JSDp->postMagStageDelay)
      JSDp->blockStage = false;
  }
  retval = JSDp->blockStage;
  SEMReleaseJeolDataMutex();
  return retval;
}

// Manage the JEOL data mutex
void SEMAcquireJeolDataMutex()
{
  WaitForSingleObject(sDataMutexHandle, DATA_MUTEX_WAIT);
}
void SEMReleaseJeolDataMutex()
{
  ReleaseMutex(sDataMutexHandle);
}

// Give the plugin the addresses of the structures and of their last members here
void SEMGetJeolStructures(JeolStateData **jsd, int **lastState, JeolParams **params, 
  int **lastParam)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  *jsd = &winApp->mScope->mJeolSD;
  *lastState = &(*jsd)->lastMember;
  *params = &winApp->mScope->mJeolParams;
  *lastParam = &(*params)->lastMember;
}

// Set the mag state variable and manage the ring entries
void SEMSetJeolStateMag(int magIndex, BOOL needMutex)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  JeolStateData *jsd = &winApp->mScope->mJeolSD;
  if (needMutex)
    SEMAcquireJeolDataMutex();
  int i = jsd->magRingIndex;
  jsd->magIndex = magIndex;
  jsd->ringMag[i] = magIndex;
  jsd->ringMagTime[i] = GetTickCount();
  jsd->ringCrossInd[i] = -1;
  jsd->magRingIndex = (i + 1) % IS_RING_MAX;
  if (needMutex)
    SEMReleaseJeolDataMutex();
}

BOOL CEMscope::GetUsePLforIS(int magInd)
{
  return (JEOLscope && mJeolSD.usePLforIS) || (HitachiScope && 
    magInd >= mLowestSecondaryMag);
}

BOOL CEMscope::GetUsePLforIS(void)
{
  return JEOLscope && mJeolSD.usePLforIS;
}

void CEMscope::SetJeolUsePLforIS(BOOL inVal)
{
  if (JEOLscope) {
    mJeolSD.usePLforIS = inVal;
    if (!inVal)
      return;
    mNumShiftBoundaries = MAX_MAGS - 2;
    for (int i = 2; i < MAX_MAGS; i++)
      mShiftBoundaries[i - 2] = i;
  }
}

void CEMscope::SetMessageWhenClipIS(BOOL inVal)
{
  sMessageWhenClipIS = inVal;
}

void CEMscope::SetJeolIndForMagMode(int inVal)
{
  sJeolIndForMagMode = inVal;
}
int CEMscope::GetJeolIndForMagMode()
{
   return sJeolIndForMagMode;
}
void CEMscope::SetJeolSecondaryModeInd(int inVal)
{
  sJeolSecondaryModeInd = inVal;
}
int CEMscope::GetJeolSecondaryModeInd()
{
  return sJeolSecondaryModeInd;
}

// Set the secondary mode if appropriate: 0 if in main mag range, 1 if in secondary
// Do not call this if in STEM mode; OK to call in LM or diffraction
// Returns -1 if it was not set, or 0 or 1 if it was set
int CEMscope::SetSecondaryModeForMag(int index)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  int secondary = winApp->mScope->GetLowestSecondaryMag();
  int lowestM = winApp->mScope->GetLowestMModeMagInd();
  if (!secondary || index < lowestM)
    return -1;
  winApp->mScope->SetSecondaryMode(index >= secondary ? 1 : 0);
  return index >= secondary ? 1 : 0;
}

// FILM EXPOSURE ROUTINES
// Set the exposure time or 0 for auto exposure
void CEMscope::SetManualExposure(double time)
{
  mManualExposure = time;
}

// Initiate the exposure
BOOL CEMscope::TakeFilmExposure(BOOL useAda, double loadDelay, double preExpose, 
                                BOOL screenOn)
{
  if (FilmBusy() > 0) {
    AfxMessageBox(_T("Film picture already in progress"));
    return false;
  }
  if (JEOLscope) {
    if (useAda) {
      AfxMessageBox(_T("Special exposure sequence not available on JEOL"));
      return false;
    }
  
  } else if (FEIscope) {  // FEI
    if (useAda) {
      if (mAdaExists < 0) {
        ITAdaExpPtr myAda;
        HRESULT hr = CoCreateInstance(__uuidof(TAdaExp), 0, CLSCTX_ALL, 
          __uuidof(ITAdaExp), (void **)&myAda);
        mAdaExists = SUCCEEDED(hr) ? 1 : 0;
      }
      if (mAdaExists <= 0) {
        AfxMessageBox(_T("Cannot connect to adaexp server - is it installed?"));
        return false;
      }
    }
    try {
      if (mPlugFuncs->GetFilmStock) {
        if (!mPlugFuncs->GetFilmStock()) {
          AfxMessageBox(_T("The film stock is apparently zero; cannot take picture"));
          return false;
        }
      } else {
        PrintfToLog("FEIScopePlugin.dll needs to be upgraded to test film stock and dim"
          " screen");
      }
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("getting the film stock "));
      return false;
    }
  }
  if (useAda)
    mCameraAcquiring = true;
  else
    SetCameraAcquiring(true);
  
  mMoveInfo.x = mManualExposure;
  mMoveInfo.y = loadDelay;
  mMoveInfo.z = preExpose;
  mMoveInfo.doBacklash = useAda;
  mMoveInfo.axisBits = screenOn ? 1 : 0;
  mFilmThread = AfxBeginThread(FilmExposeProc, &mMoveInfo,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("FilmExposeProc thread created with ID 0x%0x\n",mFilmThread->m_nThreadID);
  mFilmThread->m_bAutoDelete = false;
  mFilmThread->ResumeThread();

  // Start idle task unconditionally because need to reset blanking when done
  mWinApp->AddIdleTask(TaskFilmBusy, TASK_FILM_EXPOSURE, 0, 120000);
  return true;
}

// Busy and cleanup boilerplate
int CEMscope::TaskFilmBusy(void)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mScope->FilmBusy();
}

int CEMscope::FilmBusy(void)
{
  return UtilThreadBusy(&mFilmThread);
}

void CEMscope::FilmCleanup(int error)
{
  UtilThreadCleanup(&mFilmThread);
  SetCameraAcquiring(false);
}

#define FILM_WAIT_LOOP(a,b,c,d)  \
  sleepMsec = a; \
  numTicks = timeOut / sleepMsec; \
  for (ticks = 0; ticks < numTicks && err >= 0; ticks++) { \
    err = b; \
    if (err == c) \
      break; \
    Sleep(sleepMsec); \
  } \
  if (ticks >= numTicks) \
    throw d;

// The thread procedure to do the film exposure
UINT CEMscope::FilmExposeProc(LPVOID pParam)
{
  StageMoveInfo *info = (StageMoveInfo *)pParam;
  double exposure = info->x;
  long err = 0, shutterStatus = 0, externalStatus = 0;
  ITAdaExpPtr myAda;
  HRESULT hr;
  BOOL startingBlank = sBeamBlanked;

  // 4/5/11: user must have up to date adaexp because the shutter signals are switched
  // in a few versions of Tecnai 4.0
  // Also note Max says ExternalShutterStatus will only be valid when screen is up
  long shutterOpen = 0, shutterClosed = 1, externalEnabled = 1, externalDisabled = 0;
  long plateLoaded = 1, plateUnloaded = 0;
  bool loadedPlate = false;
  int ticks, preTicks;
  int preUnblankDelay = 1000;
  long _result;

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ScopeMutexAcquire("FilmExposeProc", true);
  int retval = 0;
  if (FEIscope) {

    if (info->plugFuncs->BeginThreadAccess(1, PLUGFEI_MAKE_CAMERA | PLUGFEI_MAKE_ILLUM)) {
      SEMMessageBox("Error creating second instance of microscope object for"
        "film exposure");
      SEMErrorOccurred(1);
      retval = 1;
    }
    if (!retval && info->doBacklash) {
      hr = CoCreateInstance(__uuidof(TAdaExp), 0, CLSCTX_ALL, __uuidof(ITAdaExp), 
        ( void **)&myAda);
      if(SEMTestHResult(hr, " creating instance of adaexp server for"
        "film exposure")) {
        retval = 1;
      }
    }
  }
  if (!retval) {
    try {

      if (info->doBacklash) {
        int numTicks, sleepMsec, timeOut = 30000;
        Sleep(800);
        shutterStatus = myAda->GetShutterStatus();
        externalStatus = myAda->GetExternalShutterStatus();
        if (shutterStatus < 0 || externalStatus < 0)
          throw 1;

        if (!info->axisBits && info->plugFuncs->SetScreenDim)
          info->plugFuncs->SetScreenDim(*vTrue);

        // Blank the beam
        info->plugFuncs->SetBeamBlank(*vTrue);
        sBeamBlanked = true;

        // Close the film shutter internally
        if (shutterStatus == shutterOpen) {
          err = myAda->GetCloseShutter();
          FILM_WAIT_LOOP(10, myAda->GetShutterStatus(), shutterClosed, 4);
        }

        // Disconnect external control
        if (externalStatus == externalEnabled) {
          err = myAda->GetDisconnectExternalShutter();
          FILM_WAIT_LOOP(10, myAda->GetExternalShutterStatus(), externalDisabled, 5);
        }

        // Load the plate
        err = myAda->GetLoadPlate();
        loadedPlate = true;
        if (err < 0)
          throw 2;
        FILM_WAIT_LOOP(50, myAda->GetPlateLoadStatus(), plateLoaded, 6);

        // Wait for Chen's delay; subtract off delay before unblanking if no pre-exp
        ticks = (int)(1000. * info->y);
        preTicks = (int)(1000. * info->z);
        if (preTicks <= 0)
          ticks -= preUnblankDelay;
        if (ticks > 0)
          Sleep(ticks);

        // Expose the number; according to Chen this advances it
        myAda->GetExposePlateLabel();
        //myAda->GetUpdateExposureNumber();

        // If there is a pre-exposure, open the beam shutter and wait for the delay
        if (preTicks > 0) {
          info->plugFuncs->SetBeamBlank(*vFalse);
          sBeamBlanked = false;
          Sleep(preTicks);
        }

        // Open film shutter
        err = myAda->GetOpenShutter();
        if (err < 0)
          throw 3;
        FILM_WAIT_LOOP(10, myAda->GetShutterStatus(), shutterOpen, 7);

        // Open beam shutter if not opened before, but let film shutter effect settle
        if (sBeamBlanked) {
          Sleep(preUnblankDelay);
          info->plugFuncs->SetBeamBlank(*vFalse);
          sBeamBlanked = false;
        }

        // Wait for the exposure time
        ticks = (int)(1000. * info->x);
        if (ticks > 0)
          Sleep(ticks);

        // Close the beam shutter then the film shutter
        info->plugFuncs->SetBeamBlank(*vTrue);
        sBeamBlanked = true;
        err = myAda->GetCloseShutter();
        FILM_WAIT_LOOP(10, myAda->GetShutterStatus(), shutterClosed, 8);

        // Unload plate
        err = myAda->GetUnloadPlate();
        FILM_WAIT_LOOP(50, myAda->GetPlateLoadStatus(), plateUnloaded, 9);
        loadedPlate = false;

        // Let the common end restore original state

      } else {

        // The plain old Tecnai/JEOL exposure.  JEOL waits until status clears
        INFOPLUG_SET(ManualExposureMode, VAR_BOOL(exposure > 0.));
        if (exposure > 0.)
          INFOPLUG_SET(ManualExposureTime, exposure);
        info->plugFuncs->TakeExposure();
      }
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("exposing film "));
      retval = 1;
    }
    catch (int errnum) {
      CString message;
      message.Format("Error %d doing special film exposure (err %d shStat %d exStat %d)", 
        errnum, err, shutterStatus, externalStatus);
      AfxMessageBox(message, MB_EXCLAME);
      SEMTrace('1', "%s", (LPCTSTR)message);
      retval = 1;
    }

    // Restore shutter states for normal or error termination, call raw methods and
    // ignore any errors
    if (FEIscope && info->doBacklash) {
      if (externalStatus == externalEnabled)
        myAda->get_ConnectExternalShutter(&_result);
      if (shutterStatus == shutterOpen)
        myAda->get_OpenShutter(&_result);
      if (loadedPlate)
        myAda->get_UnloadPlate(&_result);
      try {
        if (info->plugFuncs->SetScreenDim)
          info->plugFuncs->SetScreenDim(*vFalse);
        info->plugFuncs->SetBeamBlank(VAR_BOOL(startingBlank));
        sBeamBlanked = startingBlank;
      }
      catch (_com_error E) {
      }
    }
  }
  if (FEIscope) {
    info->plugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  ScopeMutexRelease("FilmExposeProc");
  return retval;
}

BOOL CEMscope::BothLMorNotLM(int mag1, BOOL stem1, int mag2, BOOL stem2) 
{
  int lowest1 = mLowestMModeMagInd;
  int lowest2 = mLowestMModeMagInd;
  if (stem1)
    lowest1 = mLowestSTEMnonLMmag[(FEIscope && mag1 >= mLowestMicroSTEMmag) ? 1 : 0];
  if (stem2)
    lowest2 = mLowestSTEMnonLMmag[(FEIscope && mag2 >= mLowestMicroSTEMmag) ? 1 : 0];
  if (!mag1 || !mag2 || ((stem1 ? 1 : 0) != (stem2 ? 1 : 0)) ||
    !((mag1 < lowest1 && mag2 < lowest2) || 
    (mag1 >= lowest1 && mag2 >= lowest2)))
      return false;
  return (stem1 || stem2 || mLowestSecondaryMag == 0 || 
    ((mag1 < mLowestSecondaryMag && mag2 < mLowestSecondaryMag) || 
    (mag1 >= mLowestSecondaryMag && mag2 >= mLowestSecondaryMag)));
}

// Return lowest mag above LM for regular mag mode if index = 0, STEM JEOL or FEInanoprobe
// for index = 1 and FEI microprobe for index = 2, STEM in current mode for index = -1,
// and index based on current modes for index=-2 (default)
int CEMscope::GetLowestNonLMmag(int index)
{
  if (index > 0)
    return mLowestSTEMnonLMmag[index - 1];
  if (index == -1 || (index < -1 && mWinApp->GetSTEMMode()))
    return mLowestSTEMnonLMmag[mProbeMode];
  if (mLowestSecondaryMag && mSecondaryMode)
    return mLowestSecondaryMag;
  return mLowestMModeMagInd;
}

int CEMscope::GetLowestNonLMmag(CameraParameters *camParam)
{
  return GetLowestNonLMmag(camParam->STEMcamera ? -1 : 0);
}

// Test for whether an IS get saved leaving, or restored going back to saveMag
BOOL CEMscope::SaveOrRestoreIS(int saveMag, int otherMag)
{
  int lowestM = GetLowestNonLMmag();   // NO IDEA IF THIS IS APPROPRIATE
  if (JEOLscope)
    return ((saveMag >= lowestM && !otherMag) ||
      (saveMag < lowestM && !otherMag && mApplyISoffset) ||
      (saveMag >= lowestM && otherMag < lowestM && !mApplyISoffset));
  else
    return (saveMag && !otherMag && mApplyISoffset);
}

// Returns true if IS is/may be reset on a mag change: JEOL or Hitachi in LM/HR
bool CEMscope::MagChgResetsIS(int toInd)
{
  return JEOLscope || (HitachiScope && (toInd < mLowestMModeMagInd || 
    (mLowestSecondaryMag > 0 && toInd >= mLowestSecondaryMag)));
}

int CEMscope::LookupRingIS(int lastMag, BOOL changedEFTEM)
{
  int ind, indNewMag, delay, indOldIS;
  double timeRef, timeDiff;
  if (!JEOLscope)
    return 0;

  indNewMag  = -1;
  indOldIS = -1;
  ind = mJeolSD.magRingIndex ? mJeolSD.magRingIndex - 1 : IS_RING_MAX - 1;
  SEMTrace('i', "Searching for mag in list");
  if (mUpdateByEvent) {

    // If update by event, find the first mag change away from the old mag by
    // looking back until old mag and saving index for every mag but the old mag
    while (ind != mJeolSD.magRingIndex && mJeolSD.ringMagTime[ind] >= 0.) {
      SEMTrace('i', "%.3f mag %i  crossInd %d", mJeolSD.ringMagTime[ind] / 1000., 
        mJeolSD.ringMag[ind], mJeolSD.ringCrossInd[ind]);

      if (mJeolSD.ringMag[ind] == lastMag) {
        if (mJeolSD.ringCrossInd[ind] >= 0)
          indOldIS = mJeolSD.ringCrossInd[ind];
        break;
      }
      indNewMag = ind;
      ind = ind ? ind - 1 : IS_RING_MAX - 1;
    }
    delay = mMagFixISdelay;
    if (changedEFTEM)
      delay += 500;
  } else {

    // If update by poll, look back and find the old mag in the list
    while (ind != mJeolSD.magRingIndex && mJeolSD.ringMagTime[ind] >= 0.) {
      SEMTrace('i', "%.3f mag %i  crossInd %d", mJeolSD.ringMagTime[ind] / 1000., 
        mJeolSD.ringMag[ind], mJeolSD.ringCrossInd[ind]);

      if (mJeolSD.ringMag[ind] == lastMag) {
        if (mJeolSD.ringCrossInd[ind] >= 0)
          indOldIS = mJeolSD.ringCrossInd[ind];
        indNewMag = ind;
        break;
      }
      ind = ind ? ind - 1 : IS_RING_MAX - 1;
    }
    delay = 0;
  }

  if (indNewMag >= 0 && indOldIS < 0) {

    // If an index is found for an appropriate mag entry, then get the reference
    // time before which an IS measurement must exist, and then search back for
    // an IS value that is before or the same as that time
    SEMTrace('i', "Searching for shift in list");

    timeRef = SEMTickInterval(mJeolSD.ringMagTime[indNewMag], delay);
    ind = mJeolSD.ISRingIndex ? mJeolSD.ISRingIndex - 1 : IS_RING_MAX - 1;
    while (ind != mJeolSD.ISRingIndex && mJeolSD.ringISTime[ind] >= 0.) {
      timeDiff = timeRef - mJeolSD.ringISTime[ind];

      SEMTrace('i', "%.3f diff %.3f  IS %.3f %.3f", 
        mJeolSD.ringISTime[ind] / 1000., timeDiff, mJeolSD.ringISX[ind], 
        mJeolSD.ringISY[ind]);

      if (timeDiff >= 0. || timeDiff < -2000000000) {
        indOldIS = ind;
        break;
      }
      ind = ind ? ind - 1 : IS_RING_MAX - 1;
    }
  }
  return indOldIS;
}

// Look up a camera in the FEI acquisition object interface, optionally refreshing the
// microscope object first; or restore shutter to an original stage
int CEMscope::LookupScriptingCamera(CameraParameters *params, bool refresh, 
                                    int restoreShutter)
{
  CString CCDname = params->name;
  int i, err;
  float minDrift;
  if (!FEIscope)
    return 2;
  if (!sInitialized)
    return 2;
  if (params->STEMcamera) {

    // For a STEM camera, save the binnings in original array, fill in detector names,
    // and call to get the channels
    params->numOrigBinnings = params->numBinnings;
    for (i = 0; i < params->numBinnings; i++)
      params->origBinnings[i] = params->binnings[i];
    for (i = 0; i < params->numChannels; i++)
      if (params->detectorName[i].IsEmpty())
        params->detectorName[i] = params->channelName[i];
    return GetFEIChannelList(params, false);
  }
  if (!params->detectorName[0].IsEmpty())
    CCDname = params->detectorName[0];

  // It outputs a message only for error 3, COM error
  err = mPlugFuncs->LookupScriptingCamera((LPCTSTR)CCDname, refresh, restoreShutter,
    &params->eagleIndex, &minDrift, &params->maximumDrift);
  if (err == 3)
    SEMMessageBox(CString(mPlugFuncs->GetLastErrorString()));
  if (!err) {
    if (mPluginVersion >= FEI_PLUGIN_DOES_FALCON3) {

      // Entries for testing the interface for advanced scripting cameras
      if (params->FEItype == 10 + FALCON3_TYPE && mSimulationMode) {
        params->eagleIndex |= (PLUGFEI_USES_ADVANCED | PLUGFEI_CAN_DOSE_FRAC |
          PLUGFEI_CAM_CAN_ALIGN | PLUGFEI_CAM_CAN_COUNT | 
          (40000 << PLUGFEI_MAX_FRAC_SHIFT));
        params->FEItype -= 10;
      }
      if (params->FEItype == 10 + FALCON2_TYPE && mSimulationMode) {
        params->eagleIndex |= (PLUGFEI_USES_ADVANCED | PLUGFEI_CAN_DOSE_FRAC |
          (40 << PLUGFEI_MAX_FRAC_SHIFT));
        params->FEItype -= 10;
      }
      params->FEIflags = params->eagleIndex & ~PLUGFEI_INDEX_MASK;
      SEMTrace('E', "index ret %x  flags %x", params->eagleIndex, params->FEIflags);
      if (params->FEIflags & PLUGFEI_USES_ADVANCED) {
        if (fabs(params->minExposure - DEFAULT_FEI_MIN_EXPOSURE) < 1.e-5)
          params->minExposure = minDrift;
        else
          params->minExposure = B3DMAX(params->minExposure, minDrift);
      } else {
        params->minimumDrift = B3DMAX(params->minimumDrift, minDrift);
      }

      // Promote a Falcon 2 to 3 automatically, and adjust the frame time if low
      if (params->FEItype == FALCON2_TYPE && (params->FEIflags & PLUGFEI_CAM_CAN_COUNT) && 
        (params->FEIflags & PLUGFEI_CAM_CAN_ALIGN))
        params->FEItype = FALCON3_TYPE;
      if (params->FEItype == FALCON3_TYPE && mWinApp->mCamera->GetFalconReadoutInterval()
        > 0.05)
          mWinApp->mCamera->SetFalconReadoutInterval(0.025f);
    } else {
      params->minimumDrift = B3DMAX(params->minimumDrift, minDrift);
    }
    params->eagleIndex &= PLUGFEI_INDEX_MASK;
  }
  return err;
 }

// Get the current list of detectors and check against the names in the camera properties;
// also make sure the binning list is valid for all detectors.  Release and reinitialize
// microscope pointers when "refresh" is true, for calling before camera setup
int CEMscope::GetFEIChannelList(CameraParameters * params, bool refresh)
{
  int chan, err;
  char *detNames[MAX_STEM_CHANNELS];

  for (chan = 0; chan < params->numChannels; chan++)
    detNames[chan] = _strdup((LPCTSTR)params->detectorName[chan]);
  err = mPlugFuncs->GetFEIChannelList(params->numChannels, params->numOrigBinnings,
    (long *)(&params->origBinnings[0]), (const char **)&detNames[0], (BOOL)refresh, 
    (long *)(&params->numBinnings), (long *)(&params->binnings[0]),
    params->availableChan);
  for (chan = 0; chan < params->numChannels; chan++)
    B3DFREE(detNames[chan]);
  if (err == 3)
    SEMMessageBox(CString(mPlugFuncs->GetLastErrorString()));
  return err;
}

// Set an interval after which valves should be closed and return previous value
double CEMscope::CloseValvesAfterInterval(double interval)
{
  double prior = sCloseValvesInterval;
  if (!sInitialized)
    return -1;
  sCloseValvesInterval = interval;
  sCloseValvesStart = GetTickCount();
  return prior;
}

bool CEMscope::TestSTEMshift(int type, int delx, int dely)
{
  bool success = true;

  if (!sInitialized || !JEOLscope|| type < 1 || type > 5)
    return false;
  
  ScopeMutexAcquire("TestSTEMshift", true);

  try {
    mPlugFuncs->TestSTEMShift(type, delx, dely);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting weird Shift "));
    success = false;
  } 
  ScopeMutexRelease("TestSTEMshift");
  return success;
}

void CEMscope::AddImageDetector(int id)
{
  int num = mJeolSD.numDetectors;
  if (num >= MAX_DETECTORS || id < 0 || id > 24)
    return;
  for (int i = 0; i < num; i++)
    if (mJeolSD.detectorIDs[i] == id)
      return;
  mJeolSD.detectorIDs[num] = id;
  mJeolSD.detInserted[num] = 0;
  mJeolSD.detSelected[num] = 0;
  mJeolSD.numDetectors++;
}

bool CEMscope::SelectJeolDetectors(int *detInd, int numDet)
{
  bool allIn, retval = true;
  int i, j, k, timeout = 5000;
  double startTime, insertedTime = -1., selectedTime = -1.;
  int inserted;
  if (!sInitialized || !JEOLscope)
    return false;
  if (!mJeolSD.numDetectors)
    return true;

  ScopeMutexAcquire("SelectJeolDetectors", true);

  try {

    // First look for inserted ones that could block a needed one
    for (i = 0; i < mJeolSD.numDetectors; i++) {
      if (mJeolSD.detInserted[i] && !numberInList(i, detInd, numDet, 0)) {
        allIn = false;
        for (j = 0; j < mBlockedChannels.GetSize(); j++) {
          if (mBlockedChannels[j].mappedTo != i)
            continue;
          for (k = 0; k < numDet; k++) {
            if (numberInList(detInd[k], mBlockedChannels[j].channels, 
              mBlockedChannels[j].numChans, 0))
              allIn = true;
          }
        }

        // A block was found - retract the detector and wait until out
        if (allIn) {
          mPlugFuncs->SetDetectorPosition(mJeolSD.detectorIDs[i], 0);
          startTime = GetTickCount();
          while (mJeolSD.detInserted[i] && SEMTickInterval(startTime) < timeout) {
            inserted = mPlugFuncs->GetDetectorPosition(mJeolSD.detectorIDs[i]);
            if (!inserted) {
              USE_DATA_MUTEX(mJeolSD.detInserted[i] = 0);
            } else {
              Sleep(50);
            }
          }
        }
      }
    }

    // Insert ones that are not inserted
    for (i = 0; i < numDet; i++) {
      if (detInd[i] < mJeolSD.numDetectors && !mJeolSD.detInserted[detInd[i]])
        mPlugFuncs->SetDetectorPosition(mJeolSD.detectorIDs[detInd[i]], 1);
    }

    // Loop until they are all in desired state
    startTime = GetTickCount();
    allIn = false;
    while (!allIn && SEMTickInterval(startTime) < timeout) {
      allIn = true;
      for (i = 0; i < numDet; i++) {
        if (detInd[i] < mJeolSD.numDetectors && !mJeolSD.detInserted[detInd[i]]) {
          inserted = mPlugFuncs->GetDetectorPosition(mJeolSD.detectorIDs[detInd[i]]);
          if (inserted) {
            USE_DATA_MUTEX(mJeolSD.detInserted[detInd[i]] = 1);
            insertedTime = GetTickCount();
          } else
            allIn = false;
        }
      }
      if (!allIn)
        Sleep(50);
    }

    // Now change selections of ALL detectors if needed
    for (i = 0; i < mJeolSD.numDetectors; i++) {
      inserted = (short)numberInList(i, detInd, numDet, 0);
      if (mJeolSD.detSelected[i] != inserted) {
        mPlugFuncs->SetDetectorSelected(mJeolSD.detectorIDs[i], inserted);
        USE_DATA_MUTEX(mJeolSD.detSelected[i] = inserted);
        selectedTime = GetTickCount();
      }
    }

    // Wait for delay after either insertion or selection change
    timeout = 0;
    if (insertedTime >= 0) {
      i = (int)(mInsertDetectorDelay - SEMTickInterval(insertedTime));
      timeout = B3DMAX(timeout, i);
    }
    if (selectedTime >= 0) {
      i = (int)(mSelectDetectorDelay - SEMTickInterval(selectedTime));
      timeout = B3DMAX(timeout, i);
    }
    if (timeout)
      Sleep(timeout);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("selecting/inserting detector(s) "));
    retval = false;
  } 
  ScopeMutexRelease("SelectJeolDetectors");
  return retval;
}

// A single place to evaluate the appropriate beam blank state and whether a low dose area
// needs to be switched to
BOOL CEMscope::NeedBeamBlanking(int screenPos, BOOL STEMmode, BOOL &goToLDarea)
{
  bool mustBeDown = screenPos == spDown && mWinApp->DoDropScreenForSTEM() != 0;
  goToLDarea = false;

  // Don't blank if camera is acquiring unless it is being managed by the camera
  // If screen is up blank in low dose, while pretending to be in LD, or for shutterless
  if (mCameraAcquiring && mShutterlessCamera < 2)
    return false;
  if (screenPos == spUp)
    return mLowDoseMode || mWinApp->mLowDoseDlg.m_bLowDoseMode || mShutterlessCamera;
   
  // In Stem mode, when the screen is down, low dose rules the blanking and whether to
  // go to a low dose area unless the screen has to be down, in which case the
  // shutterless flag also means it should blank
  if (mWinApp->GetSTEMMode() || STEMmode) {
    if (mLowDoseMode) {
      if (mustBeDown && STEMmode) {
        return mBlankWhenDown || mShutterlessCamera;
      } else {
        goToLDarea = true;
        return mBlankWhenDown;
      }
    } else {

      // When not in low dose, shutterless mean blank if screen has to be down
      return mShutterlessCamera && mustBeDown;
    }
  }

  // TEM mode for sure: low dose governs, blank if flag set to blank when down; but if
  // in the start of Realign to Item where hiding low dose off, blank unconditionally
  goToLDarea = mLowDoseMode;
  return (mLowDoseMode && mBlankWhenDown) || 
    (!mLowDoseMode && mWinApp->mLowDoseDlg.m_bLowDoseMode);
}

BOOL CEMscope::NeedBeamBlanking(int screenPos, BOOL STEMmode)
{
  BOOL gotoArea;
  return NeedBeamBlanking(screenPos, STEMmode, gotoArea);
}

// Way to call plugin to get values fast and keep track of whether doing so
void CEMscope::GetValuesFast(int enable)
{
  if (!sInitialized || !mPlugFuncs->GetValuesFast)
    return;
  mPlugFuncs->GetValuesFast(enable);
  if (enable < 0) {
    mGettingValuesFast = mLastGettingFast;
    mLastGettingFast = false;
  } else {
    mLastGettingFast = mGettingValuesFast;
    mGettingValuesFast = enable > 0;
  }
}

// Process a moveInfo after a stage move to record a valid X/Y backlash or set as invalid
// or, if one-shot flag is set that it was valid at start, check if move was in same
// direction and clear the flag.
void CEMscope::SetValidXYbacklash(StageMoveInfo *info)
{
  bool old = mXYbacklashValid;
  bool didBacklash = info->doBacklash && (info->axisBits & axisXY) && 
    (info->backX || info->backY);
  double movedFromPosX = info->x - mPosForBacklashX;
  double movedFromPosY = info->y - mPosForBacklashY;
  bool kept = mBacklashValidAtStart && ((movedFromPosX * mLastBacklashX <= 0. &&
    movedFromPosY * mLastBacklashY <= 0.) || (fabs(movedFromPosX) < mBacklashTolerance &&
    fabs(movedFromPosY) < mBacklashTolerance));
  bool established = mMinMoveForBacklash > 0. && 
    fabs(info->x - mStartForMinMoveX) >= mMinMoveForBacklash &&
    fabs(info->y - mStartForMinMoveY) >= mMinMoveForBacklash;
  mXYbacklashValid = didBacklash || kept || established;
  mPosForBacklashX = info->x;
  mPosForBacklashY = info->y;
  mStageAtLastPos = true;
  mBacklashValidAtStart = false;
  if (!mXYbacklashValid) {
    if (old)
      SEMTrace('n', "This stage move invalidated backlash data");
    mMinMoveForBacklash = 0.;
    return;
  }
  if (didBacklash) {
    mLastBacklashX = (float)info->backX;
    mLastBacklashY = (float)info->backY;
    SEMTrace('n', "Backlash data recorded for this stage move, pos %.3f  %.3f  bl %.1f "
      "%.1f", mPosForBacklashX, mPosForBacklashY, mLastBacklashX, mLastBacklashY);
  } else if (established) {
    mLastBacklashX = (info->x > mStartForMinMoveX ? -1.f : 1.f) * mMinMoveForBacklash;
    mLastBacklashY = (info->y > mStartForMinMoveY ? -1.f : 1.f) * mMinMoveForBacklash;
    SEMTrace('n', "Backlash state established by this stage move");
  } else
    SEMTrace('n', "Backlash state retained by this stage move");
  mMinMoveForBacklash = 0.;
}

// Return true and return backlash values if backlash is still valid for given position
bool CEMscope::GetValidXYbacklash(double stageX, double stageY, float &backX, 
                                  float &backY)
{
  bool atSamePos = fabs(stageX - mPosForBacklashX) < mBacklashTolerance &&
    fabs(stageY - mPosForBacklashY) < mBacklashTolerance;
  backX = backY = 0;
  mStageAtLastPos = mStageAtLastPos && atSamePos;
  if (!mXYbacklashValid)
    return false;
  mXYbacklashValid = atSamePos;
  if (!mXYbacklashValid) {
    SEMTrace('n', "Backlash data has become invalid, the stage has moved");
    return false;
  }
  backX = mLastBacklashX;
  backY = mLastBacklashY;
  return true;
}

// Return true if stage is still at given position within tolerance and if the request
// matches the last requested move
bool CEMscope::StageIsAtSamePos(double stageX, double stageY, float requestedX, 
                                float requestedY)
{
  return mStageAtLastPos && fabs(stageX - mPosForBacklashX) < mBacklashTolerance &&
    fabs(stageY - mPosForBacklashY) < mBacklashTolerance &&
    fabs((double)(requestedX - mRequestedStageX)) < 1.e-5 && 
    fabs((double)(requestedY - mRequestedStageY)) < 1.e-5;
}

// Descriptions of the long-running operations
static char *longOpDescription[MAX_LONG_OPERATIONS] = 
{"running buffer cycle", "refilling refrigerant",
  "getting cassette inventory", "running autoloader buffer cycle", "showing message box",
  "updating hardware dark reference", "unloading a cartridge", "loading a cartridge"};

// Start a thread for a long-running operation: returns 1 if thread busy, 
// 2 if inappropriate in some other way, -1 if nothing was started
int CEMscope::StartLongOperation(int *operations, float *hoursSinceLast, int numOps)
{
  int ind, thread, longOp, scopeOps[MAX_LONG_OPERATIONS], numScopeOps = 0;
  float sinceLast;
  bool needHWDR = false, startedThread = false;
  int now = mWinApp->MinuteTimeStamp();

  // Loop through the operations, test if it is time to do each and add to list to do
  for (ind = 0; ind < numOps; ind++) {
    longOp = operations[ind];
    sinceLast = hoursSinceLast[ind];
    if (longOp == LONG_OP_MESSAGE_BOX && (!FEIscope || 
      mPluginVersion < FEI_PLUGIN_MESSAGE_BOX || sMessageBoxTitle.IsEmpty() || 
      sMessageBoxText.IsEmpty()))
      return 2;
    if (sinceLast >= 0. && !(mLastLongOpTimes[longOp] > 0 && sinceLast > 0 && 
      now - mLastLongOpTimes[longOp] < 60. * sinceLast)) {
        if (UtilThreadBusy(&mLongOpThreads[sLongThreadMap[longOp]]) > 0)
          return 1;
        if (longOp == LONG_OP_HW_DARK_REF) {
          needHWDR = true;
        } else {
          scopeOps[numScopeOps++] = longOp;
          if (!FEIscope)
            return 2;
        }
    }
    if (sinceLast < 0) {
      mLastLongOpTimes[longOp] = now;
      mWinApp->mDocWnd->SetShortTermNotSaved();
    }
  }

  // Start thread for scope or K2 if needed
  for (thread = 0; thread < MAX_LONG_THREADS; thread++) {
    if (thread == 1 && needHWDR) {
      mLongOpData[thread].numOperations = 1;
      mLongOpData[thread].operations[0] = LONG_OP_HW_DARK_REF;
      mLongOpData[thread].finished[0] = true;
      mLongOpThreads[thread] = mWinApp->mCamera->StartHWDarkRefThread();
      sLongOpDescriptions[thread] = longOpDescription[LONG_OP_HW_DARK_REF];
    } else if (thread == 0 && numScopeOps) {
      sLongOpDescriptions[thread] = "";
      for (ind = 0; ind < numScopeOps; ind++) {
        if (ind)
          sLongOpDescriptions[thread] += ", ";
        sLongOpDescriptions[thread] += CString(longOpDescription[scopeOps[ind]]);
        mLongOpData[thread].operations[ind] = scopeOps[ind];
        mLongOpData[thread].finished[ind] = false;
      }
      mLongOpData[thread].numOperations = numScopeOps;
      mLongOpData[thread].JeolSD = &mJeolSD;
      mLongOpData[thread].plugFuncs = mPlugFuncs;
      mLongOpThreads[thread] = AfxBeginThread(LongOperationProc, &mLongOpData[thread],
        THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    } else {
      continue;
    }
    mDoingLongOperation = true;
    mLongOpThreads[thread]->m_bAutoDelete = false;
    mLongOpThreads[thread]->ResumeThread();
    mWinApp->AppendToLog("Started a long running operation: " + 
      sLongOpDescriptions[thread]);
    startedThread = true;
  }
  if (startedThread) {
    mWinApp->SetStatusText(MEDIUM_PANE, "DOING LONG OPERATION");
    mWinApp->UpdateBufferWindows();
    mWinApp->AddIdleTask(TASK_LONG_OPERATION, 0, 0);
    return 0;
  }
  return -1;
}

// Thread proc for microscope long operations
UINT CEMscope::LongOperationProc(LPVOID pParam)
{
  LongThreadData *lod = (LongThreadData *)pParam;
  HRESULT hr = S_OK;
  int retval = 0, longOp, ind;

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CEMscope::ScopeMutexAcquire("LongOperationProc", true);

  if (lod->plugFuncs->BeginThreadAccess(1, PLUGFEI_MAKE_VACUUM)) {
    SEMMessageBox("Error creating second instance of microscope object for" +
    sLongOpDescriptions[0]);
    SEMErrorOccurred(1);
    retval = 1;
  }
  if (!retval) {

    // Do the operations in the entered sequence
    for (ind = 0; ind < lod->numOperations; ind++) {
      longOp = lod->operations[ind];
      try {

        // Run buffer cycle
        if (longOp == LONG_OP_BUFFER) {
          lod->plugFuncs->RunBufferCycle();
        }
        // Refill refrigerant
        if (longOp == LONG_OP_REFILL) {
          lod->plugFuncs->ForceRefill();
        }
        // Do the cassette inventory
        if (longOp == LONG_OP_INVENTORY) {
          lod->plugFuncs->DoCassetteInventory();
        }

        // Run autoloader buffer cycle
        if (longOp == LONG_OP_LOAD_CYCLE) {
          lod->plugFuncs->LoaderBufferCycle();
        }

        // Put up message box
        if (longOp == LONG_OP_MESSAGE_BOX) {
          sMessageBoxReturn = lod->plugFuncs->ShowMessageBox(sMessageBoxType, 
            (LPCTSTR)sMessageBoxTitle, (LPCTSTR)sMessageBoxText);
        }

        // Unload a cartridge
        if (longOp == LONG_OP_UNLOAD_CART) {
          lod->plugFuncs->UnloadCartridge();
        }

        // Load a cartridge
        if (longOp == LONG_OP_LOAD_CART) {
          lod->plugFuncs->LoadCartridge(sCartridgeToLoad);
        }

        lod->finished[ind] = true;
      }

      // Save an error in the error string and retval but continue in loop
      catch (_com_error E) {
        SEMReportCOMError(E, _T(longOpDescription[longOp]), &lod->errString, true);
        retval = 1;
      }
    }
  }
  if (FEIscope) {
    lod->plugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  CEMscope::ScopeMutexRelease("LongOperationProc");
  return retval;
}

// Test if one or any long operation is busy; return 1 if any is busy regardless of
// whether others have errors, or if there is an error on one and none other are busy
int CEMscope::LongOperationBusy(int index)
{ 
  int thread, op, longOp, busy, indStart, indEnd, retval = 0;
  int now = mWinApp->MinuteTimeStamp();
  int errorOK[MAX_LONG_OPERATIONS] = {0, 1, 0, 0, 0, 0, 0, 0};
  bool throwErr = false;
  indStart = B3DCHOICE(index < 0, 0, sLongThreadMap[index]);
  indEnd = B3DCHOICE(index < 0, MAX_LONG_THREADS - 1, sLongThreadMap[index]);
  if (!mDoingLongOperation)
    return 0;
  for (thread = indStart; thread <= indEnd; thread++) {
    if (!mLongOpThreads[thread])
      continue;
    busy = UtilThreadBusy(&mLongOpThreads[thread]);
    if (busy > 0)
      retval = 1;
    if (busy < 0 && !retval)
      retval = -1;
    if (busy <= 0) {
      mWinApp->AppendToLog("Call for " + sLongOpDescriptions[thread] + 
        (busy ? " ended with error" : " finished successfully"));
      for (op = 0; op < mLongOpData[thread].numOperations; op++) {
        longOp = mLongOpData[thread].operations[op];
        if (mLongOpData[thread].finished[op]) {
          mLastLongOpTimes[longOp] = now;
          mWinApp->mDocWnd->SetShortTermNotSaved();
        } else if (busy < 0 && !errorOK[longOp])
          throwErr = true;
      }
    }
    if (busy < 0)
      mWinApp->AppendToLog(mLongOpData[thread].errString);
  }
  if (throwErr)
    SEMErrorOccurred(1);

  // If all were checked and none are busy any more, clear flag and reenable interface
  if (index < 0 && retval <= 0) {
    mDoingLongOperation = false;
    mWinApp->SetStatusText(MEDIUM_PANE, "");
    mWinApp->UpdateBufferWindows();
  }
  return retval;
}

int CEMscope::StopLongOperation(bool exiting, int index)
{
  int op, ok, indStart, indEnd, retval = 0;
  CString mess;
  indStart = B3DCHOICE(index < 0, 0, sLongThreadMap[index]);
  indEnd = B3DCHOICE(index < 0, MAX_LONG_THREADS - 1, sLongThreadMap[index]);
  for (op = indStart; op <= indEnd; op++) {
    if (UtilThreadBusy(&mLongOpThreads[op]) > 0) {
      mess.Format("There is a thread running for %s\n\nAre you sure you want to terminate"
        " this thread in the middle of that operation%s?", sLongOpDescriptions[op], 
        exiting ? " and exit the program" : "");
      ok = SEMMessageBox(mess, exiting ? MB_OKCANCEL : MB_YESNO);
      if (ok == IDOK || ok == IDYES) {
        UtilThreadCleanup(&mLongOpThreads[op]);
      } else {
        retval = 1;
        if (exiting)
          return 1;
      }
    }
  }
  return retval;
}

void CEMscope::SetMessageBoxArgs(int type, CString &title, CString &message) 
{
  sMessageBoxType = type;
  sMessageBoxTitle = title;
  sMessageBoxText = message;
}

int CEMscope::GetMessageBoxReturnValue(void)
{
  return sMessageBoxReturn;
}

// Change what JEOL update routine looks for if remote control was added or taken away
void CEMscope::RemoteControlChanged(BOOL newState)
{
  if (JEOLscope)
    setOrClearFlags(mUpdateByEvent ? &mJeolSD.lowFlags : &mJeolSD.highFlags,
    JUPD_BEAM_STATE, newState ? 1 : 0);
}
