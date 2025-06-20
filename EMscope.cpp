// EMscope.cpp           The microscope module, performs all interactions with
//                         microscope except ones by threads that need their
//                         own instance of the Tecnai
//
// Copyright (C) 2003-2021 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <atlbase.h>
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include ".\EMscope.h"
#include "PluginManager.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "CameraController.h"
#include "TSController.h"
#include "FalconHelper.h"
#include "MultiTSTasks.h"
#include "AutocenSetupDlg.h"
#include "StageMoveTool.h"
#include "RemoteControl.h"
#include "NavHelper.h"
#include "MultiShotDlg.h"
#include "MultiGridDlg.h"
#include "MultiGridTasks.h"
#include "ParticleTasks.h"
#include "DoseModulator.h"
#include "ZbyGSetupDlg.h"
#include "BaseSocket.h"
#include <assert.h>
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define AUTOSAVE_INTERVAL_MIN 1
#define MUTEX_TIMEOUT_MS 502
#define DATA_MUTEX_WAIT  100

// This timeout is long enough so that post-action routine should always succeed in
// getting mutex because update routine should finish quickly
#define MAG_MUTEX_WAIT   500

// Scope plugin version that is good enough if there are no FEI cameras, and if there 
// are cameras.  This allows odd features to be added without harrassing other users
#define FEISCOPE_NOCAM_VERSION 115
#define FEISCOPE_CAM_VERSION   115

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

static BOOL sInitialized = false;
static int sSuspendCount = 0;            // Count for update suspension
static double sReversalTilt = 100.;
static double sExtremeTilt = 100.;
static int sTiltDirection = 1;
static float sTiltReversalThresh = 0.1f;    // Set below to 0.2 for JEOL and maxTiltError
static BOOL sScanningMags = false;
static float sJEOLstageRounding = 0.001f;  // Round to this in microns
static float sJEOLpiezoRounding = 0.f;   // Round these in microns, or 0 to skip piezo
static _variant_t *vFalse;
static _variant_t *vTrue;
static BOOL sMessageWhenClipIS = true;
static BOOL sLDcontinuous = false;
static double sCloseValvesInterval = 0.;  // Interval after which to close valves
static double sCloseValvesStart;          // Start of interval
static double sProbeChangedTime = -1.;
static double sDiffSeenTime = -1.;
static BOOL sClippingIS = false;
static BOOL sISwasClipped = false;
static CString sLongOpDescriptions[MAX_LONG_THREADS];
static int sLongThreadMap[MAX_LONG_OPERATIONS];
static int sJeolIndForMagMode = JEOL_MAG1_MODE;  // Index to use in mag mode for JEOL
static int sJeolSecondaryModeInd = JEOL_SAMAG_MODE;  // Index to use for secondary mode
static BOOL sCheckPosOnScreenError = false;
static BOOL sJeolReadStageForWait = false;
static int sJeolRelaxationFlags = 0;      // Whether to wait on spot, mag, and alpha
static int sJeolStartRelaxTimeout = 1000; // Maximum time to wait for start of relaxation
static int sJeolEndRelaxTimeout = 10000;  // Maximum time to wait for end of relaxation
static CString sThreadErrString;   // Error string from thread (stage move)
static CString sMessageBoxTitle;   // Arguments for calling message box function
static CString sMessageBoxText;
static int sMessageBoxType;
static int sMessageBoxReturn;
static int sCartridgeToLoad = -1;
static bool sGettingValuesFast = false;    // Keep track of whether getting values fast
static bool sLastGettingFast = false;
static int sRestoreStageXYdelay;
static int sSimpOrigEndTimeout = 300;
static int sSimpOrigStartTimeout = 15;
static int sSimpleOriginIndex = -1;
static int sRetryStageMove = 0;

static JeolStateData *sJeolSD;


// Parameters controlling whether to wait for stage ready if error on slow movement
static float sStageErrSpeedThresh = 0.15f;  // For speed factors below this
static int sStageErrTimeThresh = 119;       // For a COM error occurring after this # sec
static float sStageErrWaitFactor = 9.;      // Wait for this # sec divided by speed factor

static HANDLE sMagMutexHandle;
static HANDLE sDataMutexHandle;

// Simulation values
static int sSimLoadedCartridge = -1;
static int sSimApertureSize[MAX_APERTURE_NUM + 1] = {0,0,0,0,0,0,0,0,0,0,0, 0};

// Jeol calls for screen pos will all work in terms of spUpJeol, etc
// The rest of the program will use the standard FEI definitions and JEOL positions are
// converted with spJeolToFEI[JoelPosition]
static int spJeolToFEI[] = {spUp, spDown, spUnknown};
static int spFEItoJeol[] = {0, spUnknownJeol, spUpJeol, spDownJeol};

// Class statics
HANDLE CEMscope::mScopeMutexHandle;
char * CEMscope::mScopeMutexOwnerStr;
char * CEMscope::mScopeMutexLenderStr;
DWORD  CEMscope::mScopeMutexOwnerId;
int    CEMscope::mScopeMutexOwnerCount;
int    CEMscope::mLastMagIndex;
double CEMscope::mTiltAngle;
int    CEMscope::mLastEFTEMmode;
BOOL   CEMscope::mMagChanged;
int    CEMscope::mInternalPrevMag;
double CEMscope::mInternalMagTime = 0;
double CEMscope::mLastISX = 0.;
double CEMscope::mLastISY = 0.;
double CEMscope::mPreviousISX;
double CEMscope::mPreviousISY;
BOOL   CEMscope::mBeamBlanked = false;
char * CEMscope::mFEIInstrumentName;
ScopePluginFuncs *CEMscope::mPlugFuncs;

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

static bool SimpleOriginStatus(CRegKey &key, int &numRefills, 
  int &secToNextRefill, int &refilling, int &active, float &sensor, CString &mess);


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEMscope::CEMscope()
{
  int i, j;
  SEMBuildTime(__DATE__, __TIME__);
  sInitialized = false;
  vFalse = new _variant_t(false);
  vTrue = new _variant_t(true);
  mUseTEMScripting = 0;
  mSkipAdvancedScripting = 0;
  mUseUtapiScripting = 0;
  mIncrement = 1.5;
  mStageThread = NULL;
  mScreenThread = NULL;
  mFilmThread = NULL;
  mApertureThread = NULL;
  mSynchronousThread = NULL;
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
  mLastBacklashX = mLastBacklashY = 0.;
  mCosineTilt = false;
  mScreenCurrentFactor = 1.;
  mFilamentCurrentScale = 100.;
  mLastTiltChange = 0.;
  mMaxTiltAngle = -999.;
  mSmallScreenFactor = 1.33f;
  mCameraAcquiring = false;
  mBlankWhenDown = false;
  mLowDoseSetArea = -1;
  mLowDoseDownArea = -1;
  mLowDoseMode = false;
  mLDNormalizeBeam = false;
  mUseNormForLDNormalize = 0;
  mSkipBlankingInLowDose = false;
  mLastSkipLDBlank = false;
  mLDBeamNormDelay = 100;
  mLDViewDefocus = 0.;
  mSearchDefocus = 0.;
  mSelectedEFTEM = false;
  mSelectedSTEM = 0;
  mFeiSTEMprobeModeInLM = 0;
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
  mFiltParam->positiveLossOnly = -1;
  mLowestMModeMagInd = 17;
  mLowestEFTEMNonLMind = -1;
  mLowestSecondaryMag = 0;
  mHighestLMindexToScan = -1;
  mSecondaryMode = 0;
  mLowestGIFModeMag = 0;
  mLowestSTEMnonLMmag[0] = 1;
  mLowestSTEMnonLMmag[1] = 1;
  mLowestMicroSTEMmag = 1;
  mNumShiftBoundaries = 0;
  mNumSpotSizes = -1;
  mNumSTEMSpotSizes = -1;
  mNumAlphas = 3;
  mMinSpotSize = 1;
  for (i = 0; i <= MAX_SPOT_SIZE; i++) {
    mC2SpotOffset[i][0] = mC2SpotOffset[i][1] = 0.; 
    mCrossovers[i][0] = mCrossovers[i][1] = 0.;
    for (j = 0; j < 4; j++)
      mCalLowIllumAreaLim[i][j] = mCalHighIllumAreaLim[i][j] = 0.;
  }
  mNumRegularCamLens = -1;
  mNumLADCamLens = -1;
  for (i = 0; i <= MAX_APERTURE_NUM; i++)
    mSavedApertureSize[i] = -1;
  mNumAlphaBeamShifts = 0;
  mNumAlphaBeamTilts = 0;
  mNumGauges = 0;
  mStageLimit[STAGE_MIN_X] = mStageLimit[STAGE_MIN_Y] = -990.;
  mStageLimit[STAGE_MAX_X] = mStageLimit[STAGE_MAX_Y] = 990.;
  mTiltAxisOffset = 0.;
  mShiftToTiltAxis = false;
  mLastCameraLength = -1.;
  mLastCamLenIndex = -1;
  mLastMagIndex = 0;
  mLastSeenMagInd = -1;
  mLastRegularMag = 0;
  mLastEFTEMmag = 0;
  mLastSTEMmag = 0;
  mLastSTEMmode = -1;
  mSimulationMode = 0;

  // JEOL initializations
  sJeolSD = &mJeolSD;
  mJeolSD.tiltAngle = 0;
  mJeolSD.stageStatus = 1;
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

  mJeolSD.relaxStartTime = mJeolSD.relaxEndTime = GetTickCount();
  mJeolSD.rampupStartTime = -1.;
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
  mJeolSD.StemCL3BaseFocus = 0x8000;
  mJeolParams.StemLMCL3ToUm = 0.;
  mJeolSD.relaxingLenses = false;
  mC2IntensityFactor[0] = mC2IntensityFactor[1] = 1.;
  mAddToRawIntensity = 0.;
  mJeolUpdateSleep = 200;
  mInitializeJeolDelay = 3000;
  mMinInitializeJeolDelay = 5500;
  mUpdateByEvent = true;
  mScreenByEvent = true;
  mSpectrumByEvent = false;
  mJeol_OLfine_to_um = 0.0058;
  mJeol_CLA1_to_um = 0.025;     // was X = 0.0252, Y = 0.0246
  mJeol_LMCLA1_to_um = 0.;
  mJeol_OM_to_um = 0.01;        // measured on 2100
  mUseCLA2forSTEM = false;
  mJeolSTEMdefocusFac = 0.7143; // Measured on Indiana 3200
  mReportsSmallScreen = true;
  mReportsLargeScreen = 1;
  mSkipJeolNeutralCheck = false;
  mStandardLMFocus = -999.;

  // This was never initialized for FEI before...
  for (i = 0; i < MAX_MAGS; i++) {
    mLMFocusTable[i][0] = mLMFocusTable[i][1] = -999.;
  }
  mRoughPLscale = 1.5;        // PL calibrations are ~100 unbinned 15 um pixels/unit

  // General initializations (-1 for these 2 that are scope-dependent)
  mHasNoAlpha = -1;
  mUpdateInterval = 150;
  mMagFixISdelay = 450;    // Was 300: needed to be longer for diffraction
  mJeolForceMDSmode = 0;
  mCalNeutralStartMag = -1;
  mJeolPostMagDelay = 0;   // This is defaulted to 1250 below if events are on
  mJeolSTEMPreMagDelay = 1500;
  mJeolMagEventWait = 5000;  // Actual times as long as 16 have been seen when VME broken
  mPostMagStageDelay = 0;
  mJeolExternalMagDelay = 0;
  mNoColumnValve = false;
  mUpdateBeamBlank = -1;
  mBlankTransients = false;
  mManualExposure = 0.;
  mWarnIfKVnotAt = 0.;
  mMainDetectorID = 13;    // Default for main screen
  mPairedDetectorID = -1;
  mJeol1230 = false;
  mMagIndSavedIS = -1;
  sMagMutexHandle = CreateMutex(0, 0, 0);
  mHandledMag = -1;
  mAdaExists = -1;
  mNoScope = 0;
  mLastLDpolarity = 1;
  mNextLDpolarity = 1;
  mLdsaParams = NULL;
  mChangeAreaAtZeroIS = false;
  mUseIllumAreaForC2 = false;
  mLDBeamTiltShifts = false;
  mShutterlessCamera = 0;
  mProbeMode = 0;
  mReturnedProbeMode = 0;
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
  mJeolHasNitrogenClass = false;
  mJeolHasExtraApertures = false;
  mJeolCondenserApIDtoUse = -1;
  mSequentialLensRelax = false;
  mJeolHasBrightnessZoom = false;
  mJeolRefillTimeout = 2400;
  mJeolFlashFegTimeout = 45;
  mJeolEmissionTimeout = 180;
  mBeamRampupTimeout = 0;
  mGaugeSwitchDelay = 0;
  mAdjustForISSkipBacklash = -1;
  mBacklashTolerance = -1.;
  mXYbacklashValid = false;
  mZbacklashValid = false;
  mStageAtLastPos = false;
  mStageAtLastZPos = false;
  mBacklashValidAtStart = false;
  mZBacklashValidAtStart = false;
  mMinMoveForBacklash = 0.;
  mMinZMoveForBacklash = 0.;
  mStageRelaxation = 0.025f;
  mMovingStage = false;
  mBkgdMovingStage = false;
  mMovingAperture = false;
  mFEIhasApertureSupport = -1;
  mFEIcanGetLoaderNames = -1;
  mLastGaugeStatus = gsInvalid;
  mVacCount = 100;
  mLastSpectroscopy = false;
  mErrCount = 0;
  mLastReport = 0;
  mReportingErr = false;
  mLastLowDose = false;
  mLastScreen = (int)spDown;
  mAutosaveCount = 0;
  mCheckFreeK2RefCount = 0;
  mChangingLDArea = 0;
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
  mInternalMagTime = mUpdateSawMagTime = GetTickCount();
  mISwasNewLastUpdate = false;
  mHitachiMagChgSeeTime = 200;
  mMagChgIntensityDelay = -1;
  mLastMagModeChgTime = -1.;
  mAdjustFocusForProbe = false;
  mNormAllOnMagChange = 0;
  mGoToRecMagEnteringLD = -1;
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
  mHitachiDoesBSforIS = 0;
  mHitachiResetsISinHC = false;
  mLastNormMagIndex = -1;
  mPrevNormMagIndex = -1;
  mFakeMagIndex = 1;
  mFakeScreenPos = spUp;
  mUseInvertedMagRange = false;
  mConstantBrightInNano = false;
  mFalcon3ReadoutInterval = 0.02495f;
  mAddToFalcon3Exposure = 0.013f;
  mFalcon4ReadoutInterval = 0.004021f;
  mFalcon4iReadoutInterval = 0.003133f;
  mMinFalcon4CountAlignFrac = 8;
  mDiffShiftScaling = 10.;
  mXLensModeAvailable = 0;
  mTiltSpeedFactor = 0.;
  mStageXYSpeedFactor = 0.;
  mRestoreStageXYdelay = 100;
  mIdleTimeToCloseValves = 0;
  mIdleTimeToStopEmission = 0;
  mAllowStopEmissionIfIdle = false;
  mUpdateDuringAreaChange = false;
  mDetectorOffsetX = mDetectorOffsetY = 0.;
  mRestoreViewFocusCount = 0;
  mDoNextFEGFlashHigh = false;
  mHasSimpleOrigin = 0;
  mDewarVacCapabilities = -1;
  mScopeCanFlashFEG = -1;
  mScopeHasPhasePlate = -1;
  mDewarVacParams.checkPVP = true;
  mDewarVacParams.runBufferCycle = false;
  mDewarVacParams.bufferTimeMin = 60;
  mDewarVacParams.runAutoloaderCycle = false;
  mDewarVacParams.autoloaderTimeMin = 60;
  mDewarVacParams.refillDewars = false;
  mDewarVacParams.dewarTimeHours = 4.;
  mDewarVacParams.checkDewars = false;
  mDewarVacParams.pauseBeforeMin = 1.;
  mDewarVacParams.startRefill = false;
  mDewarVacParams.startIntervalMin = 15.;
  mDewarVacParams.postFillWaitMin = 5.;
  mDewarVacParams.doChecksBeforeTask = true;
  mChangedLoaderInfo = false;
  mMaxJeolAutoloaderSlots = 17;
  mFegFlashCounter = 0;
  mLastBeamCurrentTime = -1.e9;
  mSkipNormalizations = 0;
  mUtapiConnected = 0;
  mUseFilterInTEMMode = false;
  mScanningMags = 0;
  mUseImageBeamTilt = false;
  mScreenRaiseDelay = 0;
  mScopeHasAutoloader = 1;
  mLDFreeLensDelay = 0;
  mOpenValvesDelay = 0;
  mSubFromPosChgISX = mSubFromPosChgISY = 0.;
  mMonitorC2ApertureSize = -1;
  mInScopeUpdate = false;
  mAdvancedScriptVersion = 0;
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
    free(mScopeMutexLenderStr);
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
  int needVers = FEISCOPE_NOCAM_VERSION;
  static bool firstTime = true;
  double htval;
  int ind, ind2;
  HitachiParams *hitachi = mWinApp->GetHitachiParams();
  int *camLen = mWinApp->GetCamLenTable();
  CString message, str;
  mShiftManager = mWinApp->mShiftManager;
  int *activeList = mWinApp->GetActiveCameraList();
  CameraParameters *camParam = mWinApp->GetCamParams();
  mCamera = mWinApp->mCamera;
  if (mNoScope) {
    mMaxTiltAngle = 70.;
    return 0;
  }
  if (sInitialized) {
    AfxMessageBox(_T("Warning: Call to EMscope::Initialize when"
      " scope is currently initialized"), MB_EXCLAME);
    return 1;
  }
  B3DCLAMP(mHasSimpleOrigin, 0, 2);

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
  mApertureTD.plugFuncs = mPlugFuncs;

  if (firstTime) {

    // Create the mutex here for JEOL, Hitachi, and FEI plugin without separate threads
    if (JEOLscope || HitachiScope) {
      mScopeMutexHandle = CreateMutex(0,0,0);
      mScopeMutexOwnerStr = (char *)calloc(128,1);
      mScopeMutexLenderStr = (char *)calloc(128,1);
      mScopeMutexOwnerCount = 0;
      mScopeMutexOwnerId = 0;
      strcpy(mScopeMutexOwnerStr,"NOBODY");
      UsingScopeMutex = true;
      mMonitorC2ApertureSize = 0;
    }

    // Finish conditional initialization of variables
    if (mShiftManager->GetStageInvertsZAxis() < 0)
      mShiftManager->SetStageInvertsZAxis(HitachiScope ? 1 : 0);
    if (FEIscope) {
      mCanControlEFTEM = true;
      mC2Name = mUseIllumAreaForC2 ? "IA" : "C2";
      mStandardLMFocus = 0.;
      mHasNoAlpha = 1;
      if (mGoToRecMagEnteringLD < 0)
        mGoToRecMagEnteringLD = 0;
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.1f;
      if (mNumSpotSizes <= 0)
        mNumSpotSizes = 11;
      if (mUpdateBeamBlank < 0)
        mUpdateBeamBlank = mWinApp->GetHasFEIcamera() ? 1 : 0;
      if (mNumRegularCamLens < 0)
        mNumRegularCamLens = mUseIllumAreaForC2 ? 21 : 16;
      if (mNumLADCamLens < 0)
        mNumLADCamLens = mUseIllumAreaForC2 ? 22 : 21;
      if (mAdjustForISSkipBacklash < 0)
        mAdjustForISSkipBacklash = 0;
      if (mDewarVacCapabilities < 0)
        mDewarVacCapabilities = mUseIllumAreaForC2 ? 3 : 0;
      if (!mPlugFuncs->GetApertureSize)
        mFEIhasApertureSupport = 0;
      if (!mPlugFuncs->GetCartridgeInfo)
        mFEIcanGetLoaderNames = 0;

    } else if (JEOLscope) {

      // JEOL: Also transfer values to structures before initialization
      sDataMutexHandle = CreateMutex(0,0,0);
      mCanControlEFTEM = mUseJeolGIFmodeCalls > 1;
      mC2Name = "C3";
      mStandardLMFocus = -999.;
      if (mGoToRecMagEnteringLD < 0)
        mGoToRecMagEnteringLD = 1;
      if (mHasNoAlpha < 0) {
        mHasNoAlpha = 0;
         mNumAlphas = B3DMIN(mNumAlphas, MAX_ALPHAS);
      }
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.2f;
      if (mSimulationMode) {
        sJEOLstageRounding = 0.1f;
        mLastRegularFocus = 0.;
        mLastSTEMfocus = 0.;
      }
      if (mScopeHasAutoloader)
        mScopeHasAutoloader = mJeolHasNitrogenClass > 1 ? 1 : 0;

      // Correct the wrong current factor placed in all properties files
      if (fabs(mScreenCurrentFactor - 0.059) < 0.001)
        mScreenCurrentFactor = 1.;

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
      mJeolParams.flags = (mJeolHasNitrogenClass ? JEOL_HAS_NITROGEN_CLASS : 0) |
        (mJeolHasExtraApertures ? JEOL_HAS_EXTRA_APERTURES : 0) |
        (mSequentialLensRelax ? JEOL_SEQUENTIAL_RELAX : 0);
      mJeolParams.flashFegTimeout = mJeolFlashFegTimeout;
      mJeolParams.emissionTimeout = mJeolEmissionTimeout;
      mJeolParams.beamRampupTimeout = mBeamRampupTimeout;
      mJeolParams.gaugeSwitchDelay = mGaugeSwitchDelay;
      mJeolParams.fillNitrogenTimeout = mJeolRefillTimeout;
      mJeolSD.mainDetectorID = mMainDetectorID;
      mJeolSD.pairedDetectorID = mPairedDetectorID;
      if (mJeolParams.StemLMCL3ToUm > 0.)
        mJeolParams.flags |= JEOL_CL3_FOCUS_LM_STEM;

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
      if (mAdjustForISSkipBacklash < 0)
        mAdjustForISSkipBacklash = 1;

      // Set up array to keep track of whether IS neutral was checked
      mCheckedNeutralIS.push_back(1);
      for (ind = 1; ind < MAX_MAGS; ind++) {
        if (!mMagTab[ind].mag)
          break;
        mCheckedNeutralIS.push_back(0);
      }
      if (mDewarVacCapabilities < 0)
        mDewarVacCapabilities = mJeolHasNitrogenClass ? 2 : 0;
      if (mScopeHasPhasePlate < 0)
        mScopeHasPhasePlate = mJeolHasNitrogenClass ? 1 : 0;
      if (mScopeCanFlashFEG < 0)
        mScopeCanFlashFEG = mJeolHasNitrogenClass ? 1 : 0;

    } else {

      // Hitachi
      mCanControlEFTEM = false;
      mStandardLMFocus = -999.;   // ??
      mHasNoAlpha = 1;
	    mC2Name = "C2";
      if (mBacklashTolerance <= 0.)
        mBacklashTolerance = 0.1f;  // WHATEVER
      if (mMaxTiltAngle < -100.)
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
      if (mAdjustForISSkipBacklash < 0)
        mAdjustForISSkipBacklash = 0;

      // Add shift boundaries for HR mode.  LM is pretty good in most of range
      if (mLowestSecondaryMag > 0) {
        for (ind = mLowestSecondaryMag; ind < MAX_MAGS; ind++)
          if (mMagTab[ind].mag)
            AddShiftBoundary(ind);
      }
      mDewarVacCapabilities = 0;
      mScopeHasPhasePlate = 0;
      mScopeCanFlashFEG = 0;
    }

    // More general initialization
    mC2Units = mUseIllumAreaForC2 ? "um" : "%";
    sTiltReversalThresh = B3DMAX(sTiltReversalThresh, 
      mWinApp->mTSController->GetMaxTiltError());
    if (mMaxTiltAngle < -100.)
      mMaxTiltAngle = mUseIllumAreaForC2 ? 69.9f : 79.9f;
    if (mLowestEFTEMNonLMind < 0)
      mLowestEFTEMNonLMind = mLowestMModeMagInd;
    if (mNumSTEMSpotSizes < 0)
      mNumSTEMSpotSizes = mNumSpotSizes;

    // Add a boundary for secondary mag range
    if (mLowestSecondaryMag && !mUsePLforIS && !HitachiScope) {
      for (ind = 0; ind <= mNumShiftBoundaries; ind++) {
        if (ind == mNumShiftBoundaries || mLowestSecondaryMag <= mShiftBoundaries[ind]) {
          if (ind == mNumShiftBoundaries) {
            AddShiftBoundary(mLowestSecondaryMag);
          } else if (mLowestSecondaryMag < mShiftBoundaries[ind]) {
            AddShiftBoundary(0);
            for (ind2 = mNumShiftBoundaries - 1; ind2 > ind; ind2--)
              mShiftBoundaries[ind2] = mShiftBoundaries[ind2 - 1];
            mShiftBoundaries[ind] = mLowestSecondaryMag;
          }
          break;
        }
      }
    }

    // Copy the boundary list to the EFTEM list and change LM boundary to EFTEM one
    mEFTEMShiftBoundaries = mShiftBoundaries;
    for (ind = 0; ind < mNumShiftBoundaries; ind++)
      if (mShiftBoundaries[ind] == mLowestMModeMagInd)
        mEFTEMShiftBoundaries[ind] = mLowestEFTEMNonLMind;
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

  try {
    if (mPlugFuncs->SkipAdvancedScripting)
      mPlugFuncs->SkipAdvancedScripting(mSkipAdvancedScripting);
  }
  catch (_com_error E) {
    if (needVers == FEISCOPE_CAM_VERSION)
      SEMReportCOMError(E, "setting whether to skip advanced scripting ");
  }
  try {
    if (mPlugFuncs->ASIsetVersion && mSkipAdvancedScripting <= 0) {
      str = "getting";
      if (mAdvancedScriptVersion > 0) {
        str = "setting";
        mPlugFuncs->ASIsetVersion(mAdvancedScriptVersion);
      } else
        mAdvancedScriptVersion = mPlugFuncs->ASIgetVersion();
    }
    if (mScopeCanFlashFEG < 0) {
      mScopeCanFlashFEG = 0;
      if (mAdvancedScriptVersion >= ASI_FILTER_FEG_LOAD_TEMP &&
        mPlugFuncs->GetFlashingAdvised) {
        try {
          mScopeCanFlashFEG = mPlugFuncs->GetFlashingAdvised(-1);
        }
        catch (_com_error E) {
        }
      }
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, str + " the advanced scripting version ");
  }
  if (FEIscope && mScopeHasPhasePlate < 0)
    mScopeHasPhasePlate = mAdvancedScriptVersion > 0 ? 1 : 0;
  if (mUtapiConnected) {
    for (ind = 0; ind < (int)mSkipUtapiServices.size(); ind++)
      if (mSkipUtapiServices[ind] >= 0 && mSkipUtapiServices[ind] < UTAPI_SUPPORT_END)
        mUtapiSupportsService[mSkipUtapiServices[ind]] = false;
    if (mMonitorC2ApertureSize < 0 && mUtapiSupportsService[UTSUP_APERTURES])
      mMonitorC2ApertureSize = 2;
  }
  if (!(mUseIllumAreaForC2 && mFEIhasApertureSupport))
    mMonitorC2ApertureSize = 0;
  else if (mMonitorC2ApertureSize < 0)
    mMonitorC2ApertureSize = 1;       // Default changed back to 1, 6/4/25
  if (mMonitorC2ApertureSize > 1) {
    try {
      mPlugFuncs->GetApertureSize(CONDENSER_APERTURE);
    }
    catch (_com_error E) {
      mFEIhasApertureSupport = 0;
      PrintfToLog("There is no support for monitoring C2 aperture on this scope");
    }
  }

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
    if (!mHasNoAlpha)
      mJeolSD.highFlags |= JUPD_ALPHA;
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

      // Would now need to set flags in highFlags for initial update of values coming by
      // event that are not watched ordinarily without events

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
      if (mFiltParam->positiveLossOnly < 0)
        mFiltParam->positiveLossOnly = mJeolHasNitrogenClass > 0 ? 0 : 1;
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

  if (mFiltParam->positiveLossOnly < 0)
   mFiltParam->positiveLossOnly = 0;
  
  if (firstTime) {

    // Check for KV if warning set up, check for MAG/SAMAG mode unconditionally
    if (mWarnIfKVnotAt) {
      htval = GetHTValue();
      if (fabs(htval - mWarnIfKVnotAt) > 0.8) {
        message.Format("WARNING: The scope is currently at %.1f kV\nand the program is "
          "calibrated for %.1f kV", htval, mWarnIfKVnotAt);
        AfxMessageBox(message, MB_EXCLAME);
      }
    }
    if (JEOLscope) {
      ScopeMutexAcquire("Initialize", true);
      try {
        mPlugFuncs->GetStageStatus();
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
              "This is usually accomplished by running INSTALL.bat\n"
              "in a current SerialEM installation package\n"
              "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_4-x-x...)",
              plugVers, FEISCOPE_PLUGIN_VERSION);
            AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
          }
          if (mPlugFuncs->GetPluginVersions && servVers >= 0 && servVers < needVers) {
            if (CBaseSocket::ServerIsRemote(FEI_SOCK_ID))
              message.Format("The scope server on the microscope computer "
                "(FEI-SEMserver.exe)\nis version %d and should be upgraded to the "
                "current version (%d)\n\n"
                "Copy the file from a current SerialEM installation package\n"
                "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_4-x-x...)\n" 
                "to the microscope computer, replacing the version there", 
                servVers, FEISCOPE_PLUGIN_VERSION);
            else
              message.Format("The scope server (FEI-SEMserver.exe) is version %d\n"
                "and should be upgraded to the current version (%d)\n\n"
                "Copy the file from a current SerialEM installation package\n"
                "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_4-x-x...)\n" 
                "and replace the version that you are now running", 
                servVers, FEISCOPE_PLUGIN_VERSION);
            AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
          }
          if (mCamera->GetTakeUnbinnedIfSavingEER() &&
            mPluginVersion < PLUGFEI_CAN_BIN_IMAGE) {
            message.Format("The version needs to be %d to do software binning, so\n"
              "software binning has been disabled and Falcon binning may not work",
              PLUGFEI_CAN_BIN_IMAGE);
            AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
            mCamera->SetTakeUnbinnedIfSavingEER(false);
          }
      }
      try {
        if (mPlugFuncs->GetXLensModeAvailable)
          mXLensModeAvailable = mPlugFuncs->GetXLensModeAvailable();
      }
      catch (_com_error E) {
      }
    }

    if (HitachiScope && GetBeamBlanked()) {
      if (SEMThreeChoiceBox("The deflector used for blanking is very high.\n"
        "Is the beam currently visible and OK, or is it blanked?", "Blanked", "Visible",
        "", MB_YESNO | MB_ICONQUESTION) == IDNO) {
          double ghx, ghy, gtx, gty;
          message = "GH";
          str = "GT";
          if (GetDeflectorByName(message, ghx, ghy) && GetDeflectorByName(str, gtx, gty)){
            ind = 0;
            if (fabs(ghy) < fabs(ghx)) {
              ind = 1;
              ghx = ghy;
            }
            if (fabs(gtx) < fabs(ghx)) {
              ind = 2;
              ghx = gtx;
            }
            if (fabs(gty) < fabs(ghx)) {
              ind = 3;
              ghx = gty;
            }
            message.Format("Switching the beam blank axis from %d to %d for this run.\n"
              "If this works OK, you can change HitachiBeamBlankAxis to %d to avoid"
              " this message", hitachi->beamBlankAxis, ind, ind);
            AfxMessageBox(str, MB_EXCLAME);
            hitachi->beamBlankAxis = ind;
          } else {
            AfxMessageBox("Cannot get values of GH or GT deflectors in order to pick a"
              " better beam blank axis - blanking will not work right!", MB_EXCLAME);
          }
      }
    }
  }

  if (!startErr && firstTime)
    SEMTrace('1', "Microscope Startup succeeded");
  firstTime = false;
  return startErr;
}


// Disconnect and reconnect to JEOL to try to cure errors
int CEMscope::RenewJeolConnection()
{
  int startErr = 0, startCall = 0, numTry = 5, trial, delay = 5000;
  double startTime;
  if (!JEOLscope)
    return -1;
  KillUpdateTimer();
  sInitialized = false;
  startTime = GetTickCount();
  while (SEMTickInterval(startTime) < 5000)
    SleepMsg(10);
  for (trial = 0; trial < numTry; trial++) {
    startErr = 0; startCall = 0;
    mPlugFuncs->UninitializeScope();
    startTime = GetTickCount();
    while (SEMTickInterval(startTime) < delay)
      SleepMsg(10);
    mPlugFuncs->InitializeScope();
    mPlugFuncs->GetNumStartupErrors(&startErr, &startCall);
    if (!ResetDefocus(true))
      startErr++;
    startCall++;
    if (!startErr || startErr < startCall - 1)
      break;
    if (trial < numTry - 1) {
      PrintfToLog("Trial #%d reconnecting to JEOL scope had too many startup errors",
        trial + 1);
      delay += 3000;
    } else {
      PrintfToLog("Failed to reconnect to JEOL scope");
      return 1;
    }
  }
  sInitialized = true;
  StartUpdate();

  return 0;
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

#define CHECK_TIME(a) if (reportTime) \
  {newWall = wallTime(); \
   cumWall += newWall - wallStart;  \
   wallTimes[a] = newWall - wallStart;  \
   wallStart = newWall;\
 }
//SEMTrace('u', "%.3f", 1000 * wallTimes[a]);\

void CEMscope::ScopeUpdate(DWORD dwTime)
{
  int screenPos;
  int magIndex, indOldIS, oldOffCalInd, newOffCalInd;
  int spotSize, probe, returnedProbe, tmpMag, toCam, oldNeutralInd, subMode;
  double intensity, objective, rawIntensity, ISX, ISY, current, defocus;
  BOOL EFTEM, bReady, smallScreen, manageISonMagChg, gotoArea, blanked, restoreIS = false;
  int STEMmode, gunState = -1, apSize;
  bool newISseen = false;
  bool handleMagChange, deferISchange, temProbeChanged = false;
  int vacStatus = -1;
  double stageX, stageY, stageZ, delISX, delISY, axisISX, axisISY, tranISX, tranISY;
  double rawISX, rawISY, curMag, minDiff, minDiff2;
  MagTable *oldTab, *newTab;
  CString checkpoint = "start";
  BOOL needBlank, changedSTEM, changedJeolEFTEM = false;

  CShiftManager *shiftManager = mWinApp->mShiftManager;
  int lastMag;
  float alpha = -999.;
  double diffFocus = -999., newWall, cumWall = 0.;
  double wallStart, wallTimes[12] = {0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.};
  bool reportTime = GetDebugOutput('u') && (mAutosaveCount % 10 == 0);
  static int firstTime = 1;
  static int apertureUpdateCount = 0, numApertureFailures = 0;

  //if (reportTime)
    wallStart = wallTime();

  // If a counter was set to restore focus, decrement and act on it when count expires
  if (mRestoreViewFocusCount > 0) {
    mRestoreViewFocusCount--;
    if (!mRestoreViewFocusCount)
      mWinApp->RestoreViewFocus();
  }

  if (mNoScope) {
    mWinApp->mScopeStatus.Update(0., mFakeMagIndex, 0., 0., 0., 0., 0., 0., 
      mFakeScreenPos == spUp, false, false, false, 0, 1, 0., 0., 0., 0, 0., 1, -1, -999);
    return;
  }

  mAutosaveCount++;
  if (!sInitialized || mSelectedSTEM < 0 || mInScopeUpdate)
    return;
  mWinApp->ManageBlinkingPane(GetTickCount());

  if (sSuspendCount > 0) {
    sSuspendCount--;
    return;
  }
  if ((mChangingLDArea && !mUpdateDuringAreaChange) || mSynchronousThread)
    return;

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
    sGettingValuesFast = true;
  }

  mInScopeUpdate = true;
  try {

    // Get stage position and readiness
    UpdateStage(stageX, stageY, stageZ, bReady);
    checkpoint = "stage";
    CHECK_TIME(0);

    if (firstTime && mPlugFuncs->GetBeamBlank)
      mBeamBlanked = mPlugFuncs->GetBeamBlank();
    firstTime = 0;

    // Get the mag and determine if it has changed
    lastMag = mLastMagIndex;
    if (JEOLscope) { 
      if (mJeolSD.magInvalid)
        magIndex = GetMagIndex();
      else
        magIndex = mJeolSD.magIndex;
      mLastCameraLength = mJeolSD.camLenValue;
      mLastCamLenIndex = mJeolSD.camLenIndex;
      STEMmode = mJeolSD.JeolSTEM ? 1 : 0;
      if (STEMmode && mNeedSTEMneutral && mLastSTEMmag)
        ResetSTEMneutral();
      mProbeMode = STEMmode ? 0 : 1;
      if (magIndex != mLastSeenMagInd) {
        mUpdateSawMagTime = GetTickCount();
        SEMTrace('i', "Update saw new mag index %d, last seen %d, last handled %d",
          magIndex, mLastSeenMagInd, mLastMagIndex);
      }

    } else { // FEI-like

      // Get the image shift immediately before the mag and it will be known to be the
      // image shift for the mag that we then read
      mPlugFuncs->GetImageShift(&rawISX, &rawISY);
      CHECK_TIME(1);
      STEMmode = (mWinApp->ScopeHasSTEM() && mPlugFuncs->GetSTEMMode() != 0) ? 1 : 0;
      CHECK_TIME(2);
      if (mUpdateBeamBlank > 0 && mPlugFuncs->GetBeamBlank) {
        blanked = mPlugFuncs->GetBeamBlank();
        if ((blanked ? 1 : 0) != (mBeamBlanked ? 1 : 0))
          SEMTrace('B', "Update saw beam blank %s", blanked ? "ON" : "OFF");
        mBeamBlanked = blanked;
      }

      // Get probe mode or assign proper value based on STEM or not
      if (FEIscope) {
        PLUGSCOPE_GET(ProbeMode, probe, 1);
        returnedProbe = probe = probe ? 1 : 0;
      } else {
        probe = STEMmode ? 0 : 1;
      }
      CHECK_TIME(3);

      if (STEMmode) {

        PLUGSCOPE_GET(STEMMagnification, curMag, 1);

        // Hijack the probe logic if in LM and assert it when property set
        if (mFeiSTEMprobeModeInLM > 0) {
          tmpMag = LookupSTEMmagFEI(curMag, probe, minDiff2);
          if (minDiff2 > 0. && MagIsInFeiLMSTEM(tmpMag)) {
            probe = B3DMIN(1, mFeiSTEMprobeModeInLM - 1);
            if (probe != mProbeMode) {
              SEMTrace('g', "ScopeUpdate: setting probe mode to %d in LM for "
                "FeiSTEMprobeModeInLM = %d", probe, mFeiSTEMprobeModeInLM);
              mProbeMode = probe;
            }
          }
        }

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
        mWinApp->mSTEMcontrol.UpdateSTEMstate(mProbeMode, magIndex);
      } else {

        // TEM mode.  Need mag index and diffraction values here
        // If the probe mode changed, set flag and just restore the raw IS values
        PLUGSCOPE_GET(MagnificationIndex, magIndex, 1);

        // Diffraction
        if (!magIndex) {
          PLUGSCOPE_GET(CameraLength, mLastCameraLength, 1);
          PLUGSCOPE_GET(CameraLengthIndex, mLastCamLenIndex, 1);
          if (FEIscope) {
            PLUGSCOPE_GET(SubMode, subMode, 1);
            if (subMode == psmLAD)
              mLastCamLenIndex += LAD_INDEX_BASE;
          }
          if (sLDcontinuous)
            diffFocus = GetDiffractionFocus();
        }
        CHECK_TIME(4);

        if (FEIscope && magIndex < GetLowestMModeMagInd() && 
          (magIndex > 0 || subMode == psmLAD))
          probe = 1;
        if (returnedProbe != mReturnedProbeMode) {
          temProbeChanged = true;
          rawISX = mLastISX;
          rawISY = mLastISY;
          mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostProbeDelay);
        }
        mProbeMode = probe;
        mReturnedProbeMode = returnedProbe;
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
      if (magIndex != lastMag)
        mUpdateSawMagTime = GetTickCount();

      // This flag is solely to indicate on the next round that previous rather than
      // last IS should be used for transfers; but do not do that when crossing boundary
      if (BothLMorNotLM(lastMag, mLastSTEMmode != 0, magIndex, STEMmode != 0) &&
        !(HitachiScope && mInternalMagTime >= 0. && 
        SEMTickInterval(mInternalMagTime) < 2 * mHitachiMagChgSeeTime && 
        !BothLMorNotLM(mLastMagIndex, false, mInternalPrevMag, false)))
        newISseen = rawISX != mLastISX || rawISY != mLastISY;
      else 
        mLastMagModeChgTime = GetTickCount();
    }
    checkpoint = "mag";
    CHECK_TIME(5);

    // Is it JEOL EFTEM with a change of state?  Get the neutral index fixed now, and set
    // up old and new indices and IS offset indices, and camera we are going to
    oldNeutralInd = mNeutralIndex;
    oldOffCalInd = newOffCalInd = (mWinApp->GetEFTEMMode() ? 1 : 0);
    toCam = mWinApp->GetCurrentCamera();
    if (JEOLscope && !mHasOmegaFilter && mJeolSD.JeolEFTEM >= 0 && !mUseFilterInTEMMode) {
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
    deferISchange = JEOLscope && mJeolExternalMagDelay > 0 && 
      SEMTickInterval(mUpdateSawMagTime) < mJeolExternalMagDelay;
    if (handleMagChange && !deferISchange) {
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

    if ((handleMagChange  && !deferISchange) || temProbeChanged) {

      // Manage IS across a mag change either if staying in LM or M, or if the offsets
      // are turned on by user
      manageISonMagChg = !STEMmode && lastMag && magIndex && 
        (!HitachiScope || mLastMagModeChgTime < 0 || temProbeChanged ||
        SEMTickInterval(mLastMagModeChgTime) > mHitachiModeChgDelay) &&
        (BothLMorNotLM(lastMag, mLastSTEMmode != 0, magIndex, STEMmode != 0) || 
        GetApplyISoffset() || changedJeolEFTEM);

      if (JEOLscope) {  
        SEMSetStageTimeout();
        BOOL crossedLM = BothLMorNotLM(lastMag, mLastSTEMmode != 0, magIndex, 
          STEMmode != 0);

        // First see if need to save the image shift for the old mag for later restore
        if (!changedJeolEFTEM && SaveOrRestoreIS(lastMag, magIndex)) {
          indOldIS = LookupRingIS(lastMag, changedJeolEFTEM, crossedLM);
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
          indOldIS = LookupRingIS(lastMag, changedJeolEFTEM, crossedLM);
          // However it is gotten, adjust the IS stored on the ring
          if (indOldIS >= 0) {
            ISX = mJeolSD.ringISX[indOldIS] - (oldTab->neutralISX[oldNeutralInd] + 
              mDetectorOffsetX);
            ISY = mJeolSD.ringISY[indOldIS] - (oldTab->neutralISY[oldNeutralInd] + 
              mDetectorOffsetY); 
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
        !shiftManager->CanTransferIS(lastMag, magIndex, STEMmode != 0, EFTEM ? 1 : 0))) {

        // I think this is for non-JEOL because it was already done for JEOL
        if (!JEOLscope) {
          ISX = mLastISX - (oldTab->neutralISX[mNeutralIndex] + oldTab->offsetISX + 
            mDetectorOffsetX);
          ISY = mLastISY - (oldTab->neutralISY[mNeutralIndex] + oldTab->offsetISY + 
            mDetectorOffsetY);
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
      mCamera->OutOfSTEMUpdate();
    if (!deferISchange)
      mLastMagIndex = magIndex;
    mLastSeenMagInd = magIndex;
    mLastSTEMmode = STEMmode;
    GetTiltAxisIS(axisISX, axisISY);

    // Call this routine in case post-action tilt changed mag
    if (mHandledMag != magIndex)
       HandleNewMag(magIndex);
    ReleaseMagMutex();
    checkpoint = "image shift/mag handling";
    CHECK_TIME(6);

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
    CHECK_TIME(7);

    // Handle adjusting focus on a probe mode change, record first focus of mode
    if (temProbeChanged) {
      if (mAdjustFocusForProbe && mFirstFocusForProbe > EXTRA_VALUE_TEST && 
        mLastFocusInUpdate != mFirstFocusForProbe) {
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
      sReversalTilt = mTiltAngle;
      sExtremeTilt = mTiltAngle;
    }
    ManageTiltReversalVars();

    // Do vacuum gauge check on FEI
    UpdateGauges(vacStatus);
    checkpoint = "vacuum";
    CHECK_TIME(8);
    if (mMonitorC2ApertureSize > 1 && mUpdateInterval * apertureUpdateCount++ > 5000 &&
      !mMovingAperture) {
      apertureUpdateCount = 0;
      try {
        apSize = mPlugFuncs->GetApertureSize(CONDENSER_APERTURE);
        numApertureFailures = 0;
        mWinApp->mBeamAssessor->ScaleIntensitiesIfApChanged(apSize, true);
      }
      catch (_com_error E) {
        numApertureFailures++;
        if (numApertureFailures > 4) {
          mMonitorC2ApertureSize = 0;
          PrintfToLog("WARNING: Getting C2 aperture size has failed 5 times in a row;\r\n"
            "   it will no longer be monitored");
        }
      }
    }

    // Take care of screen-dependent changes in low dose and update low dose panel
    needBlank = NeedBeamBlanking(screenPos, STEMmode != 0, gotoArea);
    UpdateLowDose(screenPos, needBlank, gotoArea, magIndex, diffFocus, 
      alpha, spotSize, rawIntensity, ISX, ISY, bReady);

    checkpoint = "low dose";
    CHECK_TIME(9);

    // If screen changed, tell an AMT camera to change its blanking
    // If screen changed or STEM changed, manage blanking for shutterless camera when not
    // in low dose and STEM-dependent blanking
    if (mLastScreen != screenPos || changedSTEM || 
      mLastSkipLDBlank != mSkipBlankingInLowDose) {
      if (mLastScreen != screenPos)
        mCamera->SetAMTblanking(screenPos == spUp);
      if (!mLowDoseMode || changedSTEM || mLastSkipLDBlank != mSkipBlankingInLowDose)
        BlankBeam(needBlank, "ScopeUpdate");
      mLastSkipLDBlank = mSkipBlankingInLowDose;
    }

    intensity = GetC2Percent(spotSize, rawIntensity);

    // Determine EFTEM mode and check spectroscopy on JEOL
    updateEFTEMSpectroscopy(EFTEM);
    mLastEFTEMmode = EFTEM;

    if (mWinApp->GetShowRemoteControl()) {
      if (JEOLscope)
        gunState = mJeolSD.valveOrFilament;
      else if (mPlugFuncs->GetGunValve)
        gunState = mPlugFuncs->GetGunValve();
      mWinApp->mRemoteControl.Update(magIndex, mLastCamLenIndex, spotSize, rawIntensity,
        mProbeMode, gunState, STEMmode, (int)alpha, screenPos, mBeamBlanked, bReady);
    }
    mWinApp->mScopeStatus.Update(current, magIndex, defocus, ISX, ISY, stageX, stageY,
      stageZ, screenPos == spUp, smallScreen != 0, mBeamBlanked, EFTEM, STEMmode,spotSize, 
      rawIntensity, intensity, objective, vacStatus, mLastCameraLength, mProbeMode, 
      gunState, (int)alpha);

    if (mWinApp->mStageMoveTool)
      mWinApp->mStageMoveTool->UpdateStage(stageX, stageY);
    if (mWinApp->mNavHelper->mMultiShotDlg)
      mWinApp->mNavHelper->mMultiShotDlg->UpdateMultiDisplay(magIndex, rawIntensity);

    checkpoint = "status update";
    CHECK_TIME(10);

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
    if ((dwTime  > 10000 + mLastReport && !mReportingErr) || (mDisconnected && 
      mPlugFuncs->ScopeIsDisconnected && mPlugFuncs->ScopeIsDisconnected())) {
      mReportingErr = true;
      if (mPlugFuncs->ScopeIsDisconnected && 
        mPlugFuncs->ScopeIsDisconnected()) {
        probe = (int)(1000. * (wallTime() - wallStart));
        B3DCLAMP(probe, 2000, 5000);
        SuspendUpdate(probe);
          if (!mDisconnected) {
            mDisconnected = true;
            message = "Connection to microscope has been lost";
            if (FEIscope)
              message += "\n\nIt can probably be restored if you restart FEI-SEMserver";
            SEMMessageBox(message);
          }
 
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
  if (mAutosaveCount > AUTOSAVE_INTERVAL_MIN * 60000 / mUpdateInterval) {
    mAutosaveCount = 0;
    mWinApp->mDocWnd->AutoSaveFiles();
  }

  if (mCheckFreeK2RefCount++ > 30000 / mUpdateInterval) {
    mCheckFreeK2RefCount = 0;
    mCamera->CheckAndFreeK2References(false);
    mWinApp->mFalconHelper->ManageFalconReference(false, false, "");
  }

  // Check if valves should be closed due to message box up
  if (sCloseValvesInterval > 0. && 
    SEMTickInterval(sCloseValvesStart) > sCloseValvesInterval) {
    if (GetColumnValvesOpen() > 0) {
      SetColumnValvesOpen(false);
      sCloseValvesInterval = 0.;
    } else
      sCloseValvesInterval = -1.;
  }

  // Or if the option is set to close after inactivity
  if (mIdleTimeToCloseValves > 0 && SEMTickInterval(mWinApp->GetLastActivityTime()) >
    60000. * mIdleTimeToCloseValves) {
    if (!mClosedValvesAfterIdle && GetColumnValvesOpen() > 0) {
      SetColumnValvesOpen(false, true);
      CTime ctdt = CTime::GetCurrentTime();
      if (mNoColumnValve)
        PrintfToLog("%02d:%02d:%02d: Turned off beam after %d minutes of inactivity",
          ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(), mIdleTimeToCloseValves);
      else
        PrintfToLog("%02d:%02d:%02d: Closed valve%s after %d minutes of inactivity",
          ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(), JEOLscope ? "" : "s", 
          mIdleTimeToCloseValves);
    }
    mClosedValvesAfterIdle = true;
  } else {
    mClosedValvesAfterIdle = false;
  }
  if (JEOLscope && mAllowStopEmissionIfIdle && mIdleTimeToStopEmission > 0 &&
    SEMTickInterval(mWinApp->GetLastActivityTime()) > 60000. * mIdleTimeToStopEmission) {
    if (!mStoppedEmissionOnIdle && GetEmissionState(toCam) && toCam) {
      SetEmissionState(0);
      CTime ctdt = CTime::GetCurrentTime();
      PrintfToLog("%02d:%02d:%02d: Turned off emission after %d minutes of inactivity",
          ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(), mIdleTimeToStopEmission);
    }
    mStoppedEmissionOnIdle = true;
  } else {
    mStoppedEmissionOnIdle = false;
  }

  if (mPlugFuncs->DoingUpdate) {
     mPlugFuncs->DoingUpdate(0);
     sGettingValuesFast = false;
  }
  if (JEOLscope) 
    SEMReleaseJeolDataMutex();
  else
    ScopeMutexRelease("UpdateProc"); 
  if (reportTime) {
    /*
    0: stage pos & tilt & readiness
    1: image shift
    2: STEM mode if STEM
    3: beam blank & probe mode
    4: FEI only: Mag index.  If diff: cam length, cam len index, submode, diff
    Focus if LD continuous
    5: Mag index generally
    6: image shift/mag handling if changed
    7: Screen pos main and small, intensity/IA, spot size, screen current,
    defocus, objective strength, alpha
    8: vacuum (sampled intermittently)
    9: Updating low dose, maybe blanking
    10: EFTEM mode, column valve
    */
    PrintfToLog("Update time %.1f msec", 1000. * cumWall);
    PrintfToLog("Components %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f", 
      1000.*wallTimes[0],1000.*wallTimes[1], 1000.*wallTimes[2],1000.*wallTimes[3],
      1000.*wallTimes[4],1000.*wallTimes[5],1000.*wallTimes[6],
      1000.*wallTimes[7],1000.*wallTimes[8],1000.*wallTimes[9],1000.*wallTimes[10]);
  }
  mInScopeUpdate = false;
}

// UPDATE SUB-ROUTINES CALLED ONLY FROM INSIDE ScopeUpdate

// Get tilt angle and stage position and readiness, update panels for blanking
void CEMscope::UpdateStage(double &stageX, double &stageY, double &stageZ, BOOL &bReady)
{
  float backX, backY;
  if (JEOLscope) {
    mTiltAngle = mJeolSD.tiltAngle;
    stageX = mJeolSD.stageX;
    stageY = mJeolSD.stageY;
    stageZ = mJeolSD.stageZ;
    bReady = mJeolSD.stageStatus == 0;

  } else {  // Plugin
    mPlugFuncs->GetStagePosition(&stageX, &stageY, &stageZ);
    stageX *= 1.e6;
    stageY *= 1.e6;
    stageZ *= 1.e6;
    mTiltAngle = mPlugFuncs->GetTiltAngle() / DTOR;
    bReady = mPlugFuncs->GetStageStatus() == 0;
  }

  // 12/15/04: just use internal blanked flag instead of getting from scope
  GetValidXYbacklash(stageX, stageY, backX, backY);
  GetValidZbacklash(stageZ, backY);
  mWinApp->mTiltWindow.TiltUpdate(mTiltAngle, bReady);
  mWinApp->mLowDoseDlg.BlankingUpdate(mBeamBlanked);
  if (mWinApp->ScopeHasSTEM())
    mWinApp->mSTEMcontrol.BlankingUpdate(mBeamBlanked);
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
    alpha = (float)mJeolSD.alpha;

  } else {  // FEI-like
    screenPos = GetScreenPos();
    if (mUseIllumAreaForC2) {
      PLUGSCOPE_GET(IlluminatedArea, rawIntensity, 1.e4);
      rawIntensity = IllumAreaToIntensity(rawIntensity);
    } else {
      PLUGSCOPE_GET(Intensity, rawIntensity, 1.);
      rawIntensity += mAddToRawIntensity;
    }
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
  static int lastVacStatus = -1;
  if (mVacCount++ >= (JEOLscope ? 5000 : 1700) / mUpdateInterval ||
    (GetDebugOutput('u') && (mAutosaveCount % 50 == 0))) {
      mVacCount = 0;
      if (mPlugFuncs->GetGaugePressure) {
        int gaugeStatus, gauge, statIndex;
        double gaugePressure, startTime;

        if (mNumGauges) {

          for (gauge = 0; gauge < mNumGauges; gauge++) {
            if (JEOLscope && mGaugeNames[gauge] == "P4")
              continue;

            // Get status of one gauge on the list by its index
            startTime = GetTickCount();
            mPlugFuncs->GetGaugePressure((LPCTSTR)mGaugeNames[gauge], &gaugeStatus,
              &gaugePressure);
            if (JEOLscope && (gaugeStatus == 0 || gaugeStatus == 3))
              gaugeStatus = 3 - gaugeStatus;
            mLastGaugeStatus = gaugeStatus;
            mLastPressure = gaugePressure;
            statIndex = 0;
            if (gaugeStatus == gsInvalid || gaugeStatus == gsOverflow || 
              gaugePressure > mRedThresh[gauge])
              statIndex = 2;
            else if (gaugeStatus != gsUnderflow && 
              gaugePressure > mYellowThresh[gauge])
              statIndex = 1;

            SEMTrace('V', "Gauge %d, status %d,  pressure %f  SEMstatus %d  took %.0f", 
              gauge, gaugeStatus, gaugePressure, statIndex, SEMTickInterval(startTime));

            // Report maximum of all status values
            if (vacStatus < statIndex)
              vacStatus = statIndex;
          }
          lastVacStatus = vacStatus;
        }
      }
  } else if (mPlugFuncs->GetGaugePressure)
    vacStatus = lastVacStatus;
}

// Manage things when screen changes in low dose, update low dose panel
void CEMscope::UpdateLowDose(int screenPos, BOOL needBlank, BOOL gotoArea, int magIndex,
  double diffFocus, float alpha, int &spotSize, double &rawIntensity, double &ISX,
  double &ISY, BOOL stageReady)
{
  double delISX, delISY;
  if (mLowDoseMode) {

    // If low dose mode or screen position has changed:
    if (mLastLowDose != mLowDoseMode || mLastScreen != screenPos) {

      // First apply a blank if needed, regardless of screen            
      if (needBlank)
        BlankBeam(true, "UpdateLowDose");

      // If screen is down, next set the desired area, then unblank if needed
      if (screenPos == spDown) {
        if (gotoArea && mLowDoseDownArea >= 0) {
          GotoLowDoseArea(mLowDoseDownArea);
          if (mWinApp->mMultiTSTasks->AutocenMatchingIntensity()) {
            delISX = mWinApp->mAutocenDlg->GetParamIntensity();
            if (delISX > 0.)
              SetIntensity(delISX);
          }
        }
        if (!needBlank)
          BlankBeam(false), "UpdateLowDose";

        // For omega filter, assert the filter settings
        if (GetHasOmegaFilter())
          mCamera->SetupFilter();
      }

      // Refresh the spot size and intensity
      spotSize = FastSpotSize();
      rawIntensity = FastIntensity();
    }

    // After those potential changes, send update to low dose window
    mWinApp->mLowDoseDlg.ScopeUpdate(magIndex, spotSize, rawIntensity, ISX, ISY,
      screenPos == spDown, mLowDoseSetArea, mLastCamLenIndex, diffFocus, alpha, 
      mProbeMode, stageReady);

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
          mCamera->SetupFilter();

        } else if (!mCamera->GetIgnoreFilterDiffs() && 
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
    double wallStart = wallTime();
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
    else if (mWinApp->GetEFTEMMode() && mCanControlEFTEM && !mUseFilterInTEMMode) {
      needed = (screenPos == spUp || !mFiltParam->autoMag ||
        mWinApp->mMultiTSTasks->AutocenTrackingState());
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
void CEMscope::ManageTiltReversalVars()
{
  if (sTiltDirection * (mTiltAngle - sExtremeTilt) > 0) 

    // Moving in the same direction: replace the extreme value
    sExtremeTilt = mTiltAngle;
  else if (sTiltDirection * (mTiltAngle - sExtremeTilt) < -sTiltReversalThresh) {

    // Moving in opposite direction by more than threshold: reverse,
    // make the reversal be the last extreme, and replace the extreme
    sTiltDirection = -sTiltDirection;
    sReversalTilt = sExtremeTilt;
    sExtremeTilt = mTiltAngle;
  }
}

// Return tilt angle
double CEMscope::GetTiltAngle(BOOL forceGet)
{
  int getFast = 0;
  bool wasGettingFast = sGettingValuesFast;
  if (!sInitialized)
    return 0.0;

  // Plugin uses state value only if getting fast is set
  if (JEOLscope) {
    if ((mJeolSD.eventDataIsGood && !forceGet) || sGettingValuesFast) {
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
    mTiltAngle = mPlugFuncs->GetTiltAngle() / DTOR;
    if (getFast && !wasGettingFast)
      GetValuesFast(-1);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting tilt angle "));    
  }
  if (!getFast)
    ScopeMutexRelease("GetTiltAngle");
  return mTiltAngle;
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
  if (!mCosineTilt)
    return mIncrement;
  return (float)cos(GetTiltAngle() * DTOR) * mIncrement;
}


void CEMscope::TiltUp(double restoreX, double restoreY)
{
  TiltTo(GetTiltAngle() + GetIncrement());
}

void CEMscope::TiltDown(double restoreX, double restoreY)
{
  TiltTo(GetTiltAngle() - GetIncrement());
}

void CEMscope::TiltTo(double inVal, double restoreX, double restoreY)
{
  StageMoveInfo info;
  BOOL restore = false;
  info.alpha = inVal;
  info.axisBits = axisA;
  if (restoreX > EXTRA_VALUE_TEST && restoreY > EXTRA_VALUE_TEST) {
    info.x = restoreX;
    info.y = restoreY;
    restore = true;
  }
  MoveStage(info, false, false, false, false, restore);
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

// Get the B axis of an FEI stage, which could be Krios flip-flop, assume it is radians
double CEMscope::GetStageBAxis(void)
{
  double retval = 0.;
  if (!sInitialized || !mPlugFuncs->GetStageBAxis)
    return 0.0;

  ScopeMutexAcquire("GetStageBAxis",true);

  try {
    retval = mPlugFuncs->GetStageBAxis() / DTOR;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting B axis of stage "));    
  }
  ScopeMutexRelease("GetStageBAxis");
  return retval;
}

// Set the B axis value, leave as degrees in the structure
BOOL CEMscope::SetStageBAxis(double inVal)
{
  StageMoveInfo info;
  info.beta = inVal;
  info.axisBits = axisB;
  return MoveStage(info, false);
}

// External call to move stage
BOOL CEMscope::MoveStage(StageMoveInfo info, BOOL doBacklash, BOOL useSpeed, 
  int inBackground, BOOL doRelax, BOOL doRestore)
{
  // This call is not supposed to happen unless stage is ready, so don't wait long
  // 5/17/11: Hah.  Nothing is too long for a JEOL...  increased from 4000 to 12000
  DWORD waitTime = 12000;
  float signedLimit = (info.alpha >= 0. ? 1.f : -1.f) * mMaxTiltAngle;
  if (JEOLscope) {
    if (SEMCheckStageTimeout())
      waitTime += mJeolSD.postMagStageDelay;
  }
  if (inBackground >= 0 && WaitForStageReady(waitTime)) {
    SEMMessageBox(_T("Stage not ready"));
    return false;
  }
  if (inBackground > 0 && FEIscope && CBaseSocket::LookupTypeID(FEI_SOCK_ID) >= 0 
    && SEMNumFEIChannels() < 4) {
    SEMMessageBox("The stage cannot be moved in the background.\n"
      "You must have the property BackgroundSocketToFEI set to\n"
      "open the socket connection for background stage movement\n"
      "when the program starts.");
    return false;
  }

  if (((mTiltSpeedFactor > 0. && info.axisBits == axisA) || (mStageXYSpeedFactor > 0 && 
    (info.axisBits & axisXY) != 0 && !(info.axisBits & ~axisXY))) && !useSpeed) {
    useSpeed = true;
    info.speed = B3DCHOICE(info.axisBits == axisA, mTiltSpeedFactor, mStageXYSpeedFactor);
  }

  if (((info.axisBits & axisB) || useSpeed) && !mPlugFuncs->SetStagePositionExtra) {
    SEMMessageBox("Current scope plugin version does not support setting speed or B "
      "axis");
    return false;
  }
  mMoveInfo = info;
  mMoveInfo.JeolSD = &mJeolSD;
  mMoveInfo.plugFuncs = mPlugFuncs;
  mMoveInfo.doBacklash = doBacklash;
  mMoveInfo.doRelax = doRelax && (mMoveInfo.relaxX != 0. || mMoveInfo.relaxY != 0.);
  mMoveInfo.doRestoreXY = doRestore && !(info.axisBits & axisXY);
  if (!doBacklash)
    mMoveInfo.backAlpha = 0.;
  if (mMoveInfo.doRestoreXY || !doBacklash)
    mMoveInfo.backX = mMoveInfo.backY = mMoveInfo.backZ = 0.;
  if (!doRelax)
    mMoveInfo.relaxX = mMoveInfo.relaxY = 0.;
  mMoveInfo.useSpeed = useSpeed && mPlugFuncs->SetStagePositionExtra;

  mMoveInfo.inBackground = B3DMAX(0, inBackground);
  mRequestedStageX = (float)info.x;
  mRequestedStageY = (float)info.y;
  mRequestedStageZ = (float)info.z;
  mStageAtLastPos = false;
  mStageAtLastZPos = false;
  sRestoreStageXYdelay = mRestoreStageXYdelay;

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
    USE_DATA_MUTEX(mJeolSD.stageStatus = ((info.axisBits & axisX) ? 1 : 0) + 
      ((info.axisBits & axisY) ? 10 : 0) + ((info.axisBits & axisZ) ? 100 : 0) +
      ((info.axisBits & axisA) ? 1000 : 0));
    mWinApp->mTiltWindow.TiltUpdate(mJeolSD.tiltAngle, false);
  }
  sThreadErrString = "";

  mLastStageCheckTime = GetTickCount();
  mStageThread = AfxBeginThread(StageMoveProc, &mMoveInfo,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("StageMoveProc created with ID 0x%0x\n",mStageThread->m_nThreadID);
  mStageThread->m_bAutoDelete = false;
  mStageThread->ResumeThread();
  mMovingStage = true;
  mBkgdMovingStage = inBackground > 0;
  UpdateWindowsForStage();
  if (!mWinApp->GetAppExiting())
    mWinApp->AddIdleTask(TaskStageBusy, TaskStageDone, TaskStageError, 0, 0);
  return true;
}

void CEMscope::UpdateWindowsForStage()
{
  mWinApp->mCameraMacroTools.Update();
  mWinApp->UpdateAllEditers();
  mWinApp->UpdateMacroButtons();
  mWinApp->mMontageWindow.Update();
  mWinApp->mTiltWindow.UpdateEnables();
  mWinApp->mAlignFocusWindow.Update();
  mWinApp->mLowDoseDlg.Update();
  if (mWinApp->GetShowRemoteControl())
    mWinApp->mRemoteControl.UpdateEnables();
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
  winApp->mScope->UpdateWindowsForStage();
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
      SetValidZbacklash(&mMoveInfo);
      if (!sThreadErrString.IsEmpty())
        SEMMessageBox(sThreadErrString);
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
    if ((mJeolSD.eventDataIsGood && ignoreOrTrustState <= 0) || ignoreOrTrustState < -1)
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
      if (JEOLscope) {
        if (!sGettingValuesFast)
          SEMTrace('w', "StageBusy: read status %d at %.3f", retval,
            SEMSecondsSinceStart());
        GetValuesFast(-1);
      }
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
  CString toStr;
  stData.info = info;

  // Checking this in the main thread hangs the interface, so wait until we get to the
  // stage thread to wait for this to expire
  while (JEOLscope && SEMCheckStageTimeout())
    Sleep(100);
  
  ScopeMutexAcquire("StageMoveProc", true);
  if (FEIscope) {
    if (info->plugFuncs->BeginThreadAccess(info->inBackground ? 3 : 1, 
      PLUGFEI_MAKE_STAGE)) {
      sThreadErrString = "Error creating second instance of microscope object for"
        " moving stage";
      SEMErrorOccurred(1);
      retval = 1;
    }
  }
  if (!retval) {
    try {
      StageMoveKernel(&stData, false, false, destX, destY, destZ, destAlpha);
    }
    catch (_com_error E) {
      StageMovingToMessage(destX, destY, destZ, destAlpha, info->axisBits, toStr);
      if (fabs(destX) > xyLimit || fabs(destY) > xyLimit) {
        SEMReportCOMError(E, _T(toStr + CString(".\r\n\r\n"
          "You are very close to the end of the stage range")), &sThreadErrString);
      } else {
        SEMReportCOMError(E, _T(toStr), &sThreadErrString);
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

// Common function for getting the string for the stage move
void CEMscope::StageMovingToMessage(double destX, double destY, double destZ, 
  double destAlpha, int axisBits, CString &toStr)
{
  CString str;
  toStr = "moving stage to";
  if (axisBits & (axisX | axisY)) {
    str.Format(" X %.2f  Y %.2f", destX * 1.e6, destY * 1.e6);
    toStr += str;
  }
  if (axisBits & axisZ) {
    str.Format("  Z %.2f", destZ * 1.e6);
    toStr += str;
  }
  if (axisBits & axisA) {
    str.Format("  tilt %.1f", destAlpha);
    toStr += str;
  }
}

// Macro to do a call in a retry loop
#define RETRY_LOOP(call) \
  for (retry = (sRetryStageMove ? 0 : 1); retry < 2; retry++) {    \
    try {     \
      info->plugFuncs->call;    \
      break;     \
    }     \
    catch (_com_error E) {    \
      if (retry > 0)     \
        throw E;     \
      StageMovingToMessage(destX, destY, destZ, destAlpha, info->axisBits, str);    \
      SEMReportCOMError(E, str, &errStr, true);     \
      SEMTrace('0', "WARNING: %s\r\nRetrying...", (LPCTSTR)errStr);     \
    }     \
  }

// Underlying stage move procedure, callable in try block from scope or blanker threads
void CEMscope::StageMoveKernel(StageThreadData *std, BOOL fromBlanker, BOOL async, 
  double &destX, double &destY, double &destZ, double &destAlpha)
{
  StageMoveInfo *info = std->info;
  bool xyBacklash = info->doBacklash && (info->axisBits & axisXY);
  double xpos = 0., ypos = 0., zpos = 0., alpha, timeout, beta;
  double startTime = GetTickCount();
  int step, back, relax, restore, axisBitsUse, retry;
  CString str, errStr;
  destX = 0.;
  destY = 0.;
  destZ = 0.;

  // Make sure this is initialized so timeouts are sensible
  info->finishedTick = GetTickCount();

  // If doing backlash, set up a first move with backlash added
  // Only change axes that are specified in axisBits
  // First do a backlash move if specified, then do the move to an overshoot if relaxing
  // and finally the move to the target
  for (step = 0; step < 4; step++) {
    back = step == 0 ? 1 : 0;
    relax = step == 1 ? 1 : 0;
    restore = step == 3 ? 1 : 0;
    axisBitsUse = restore ? axisXY : info->axisBits;
    if ((back && !info->doBacklash) || (relax && !info->doRelax) || 
      (restore && !info->doRestoreXY))
      continue;
    if (restore)
      Sleep(sRestoreStageXYdelay);

    // Adding backX/Y makes the second move be opposite to the direction of backX/Y
    // Subtracting relax makes the final move be in the direction of relaxX/Y
    if (axisBitsUse & (axisX | axisY)) {
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
    if (info->doRelax || info->doBacklash || info->doRestoreXY)
      if (axisBitsUse & (axisA | axisZ))
        SEMTrace('n', "Step %d, %s move to %.3f", step + 1, 
        (axisBitsUse & axisA) ? "A" : "Z", (axisBitsUse & axisA) ? alpha : zpos);
      else
        SEMTrace('n', "Step %d, move to %.3f %.3f", step + 1, xpos, ypos);
     
    // destX, destY are used to decide on "stage near end of range" message upon error
    // They are supposed to be in meters; leave at 0 to disable 
    if ((info->useSpeed || (info->axisBits & axisB)) && 
      info->plugFuncs->SetStagePositionExtra) {
      beta = (info->axisBits & axisB) ? info->beta : 0.;
      if (info->useSpeed) {
        try {
          info->plugFuncs->SetStagePositionExtra(destX, destY, destZ, alpha * DTOR,
            beta * DTOR, info->speed, axisBitsUse);
        }
        catch (_com_error E) {

          // If there is an error with the speed setting and conditions are right, wait
          // for a time that depends on the speed before throwing the error on
          if (info->speed > sStageErrSpeedThresh || SEMTickInterval(startTime) <
            1000. * sStageErrTimeThresh) {
            SEMTrace('S', "Stage error occurred at %.1f sec with speed factor %.3f",
              SEMTickInterval(startTime) / 1000., info->speed);
            throw E;
          }
          timeout = 1000. * sStageErrWaitFactor / info->speed;
          SEMTrace('S', "Caught stage error at %.1f, waiting up to %.f sec for stage ready",
            SEMTickInterval(startTime) / 1000., timeout / 1000.);
          startTime = GetTickCount();
          while (SEMTickInterval(startTime) < timeout) {
            if (!info->plugFuncs->GetStageStatus())
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
        RETRY_LOOP(SetStagePositionExtra(destX, destY, destZ, alpha * DTOR,
          beta * DTOR, 1., axisBitsUse));
      }

    } else {
      RETRY_LOOP(SetStagePosition(destX, destY, destZ, alpha * DTOR, axisBitsUse));
    }
    if (async && (JEOLscope || FEIscope))
      return;
    if (HitachiScope || JEOLscope)
      WaitForStageDone(info, fromBlanker ? "BlankerProc" : "StageMoveProc");         
    mTiltAngle = info->plugFuncs->GetTiltAngle() / DTOR;
    ManageTiltReversalVars();
    if ((step == 2 && !info->doRestoreXY) || (restore && step == 3)) {
      info->plugFuncs->GetStagePosition(&xpos, &ypos, &zpos);
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
    needMutex = !(mJeolSD.eventDataIsGood || sGettingValuesFast);
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

// Get X/Y poxition of piezo drives if they exist
BOOL CEMscope::GetPiezoXYPosition(double &X, double &Y)
{
  BOOL success = true;
  if (!sInitialized || !mPlugFuncs->GetPiezoXYPosition)
    return false;
  ScopeMutexAcquire("GetPiezoXYPosition", true);

  try {
    mPlugFuncs->GetPiezoXYPosition(&X, &Y);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Piezo Position "));
    success = false;
  }
  ScopeMutexRelease("GetPiezoXYPosition");
  return success;
}

// Set X/Y position of piezos
BOOL CEMscope::SetPiezoXYPosition(double X, double Y, bool incremental)
{
  BOOL success = true;
  double curX, curY;
  if (!sInitialized || !mPlugFuncs->GetPiezoXYPosition || !mPlugFuncs->SetPiezoXYPosition)
    return false;
  ScopeMutexAcquire("SetPiezoXYPosition", true);

  try {
    if (incremental) {
      mPlugFuncs->GetPiezoXYPosition(&curX, &curY);
      X += curX;
      Y += curY;
    }
    mPlugFuncs->SetPiezoXYPosition(X, Y);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Piezo Position "));
    success = false;
  }
  ScopeMutexRelease("SetPiezoXYPosition");
  return success;
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
    sGettingValuesFast));
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
  double axisISX, axisISY, plugISX, plugISY, neutISX, neutISY;
  const double Jeol_IS1X_to_um =  0.00508;
  const double Jeol_IS1Y_to_um =  0.00434;
  bool needBeamShift = false;
  int magIndex, ind, neutXnew, neutYnew, neutXold, neutYold;
  short ID = mJeolSD.usePLforIS ? 11 : 8;
  mLastISdelX = shiftX;
  mLastISdelY = shiftY;
  ScaleMat IStoBS;
  if (!sInitialized || mDisconnected)
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

    // Check the neutral image shift one time at this mag
    if (JEOLscope && !mLastSTEMmode && mLastMagIndex > 0 && !mSkipJeolNeutralCheck &&
      mLastMagIndex < (int)mCheckedNeutralIS.size() && !mCheckedNeutralIS[mLastMagIndex]){
        magIndex = mPlugFuncs->GetMagnificationIndex();
        if (!mCheckedNeutralIS[magIndex]) {
          mPlugFuncs->SetToNeutral(ID);
          //Sleep(500);  Should no longer be necessary 2/9/22
          mPlugFuncs->GetImageShift(&neutISX, &neutISY);
          mCheckedNeutralIS[magIndex] = 1;
          neutXold = NINT8000(mMagTab[magIndex].neutralISX[mNeutralIndex] / 
            Jeol_IS1X_to_um);
          neutYold = NINT8000(mMagTab[magIndex].neutralISY[mNeutralIndex] / 
            Jeol_IS1Y_to_um);
          neutXnew = NINT8000(neutISX / Jeol_IS1X_to_um);
          neutYnew = NINT8000(neutISY / Jeol_IS1Y_to_um);
          if (B3DABS(neutXnew - neutXold) > 1 || B3DABS(neutYnew - neutYold) > 1 ) {
            CString mess;
            mess.Format("The neutral image shift calibration appears to be off at "
              "this magnification.\r\n(Stored value 0x%x  0x%x,  value now 0x%x, 0x%x)"
              "\r\n\r\nYou should run \"Neutral IS Values\" in the "
                "Calibration menu and save calibrations", neutXold, neutYold, neutXnew, 
                neutYnew);
            mWinApp->AppendToLog(mess);
            SEMMessageBox(mess);
            for (ind = 0; ind < (int)mCheckedNeutralIS.size(); ind++)
              mCheckedNeutralIS[ind] = 1;
          }
        }
    }

    // Make the movements; get mag if not STEM mode and need beam shift
    sISwasClipped = false;
    mPlugFuncs->SetImageShift(shiftX, shiftY);
    if (JEOLscope)
      GetValuesFast(1);
    if (sISwasClipped) {
      mPlugFuncs->GetImageShift(&plugISX, &plugISY);
      mLastISdelX += (plugISX - shiftX);
      mLastISdelY += (plugISY - shiftY);
      shiftX = plugISX;
      shiftY = plugISY;
    }
    magIndex = -1;
    needBeamShift = (JEOLscope || (HitachiNeedsBSforIS(magIndex))) && 
      !(mWinApp->ScopeHasSTEM() && mPlugFuncs->GetSTEMMode && mPlugFuncs->GetSTEMMode());
    if (needBeamShift && magIndex < 0)
      magIndex = mPlugFuncs->GetMagnificationIndex();
    if (JEOLscope)
      GetValuesFast(-1);
    SetISChanged(shiftX, shiftY);

    // Need to apply beam shift if it is calibrated for some scopes, but not when it
    // is being accumulated during low dose change
    if (needBeamShift && !mSkipNextBeamShift && !(JEOLscope && mChangingLDArea > 0)) {
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

// Returns true if a Hitachi scope needs the beam shift in the current mode; returns
// mag index if it comes is as -1.  returns false for other scopes
bool CEMscope::HitachiNeedsBSforIS(int &magIndex)
{
  if (!mHitachiDoesBSforIS)
    return HitachiScope;
  if (magIndex < 0)
    magIndex = mPlugFuncs->GetMagnificationIndex();
  if (magIndex < mLowestMModeMagInd)
    return (mHitachiDoesBSforIS & 4) == 0;
  if (magIndex >= mLowestSecondaryMag)
    return (mHitachiDoesBSforIS & 2) == 0;
  return (mHitachiDoesBSforIS & 1) == 0;
}

// Get the image shift offsets needed to center on the tilt axis, if mode selected
// In any event, add the neutral value and user offset
void CEMscope::GetTiltAxisIS(double &ISX, double &ISY, int magInd)
{
  static BOOL firstTime = true;
  int STEMmode = mLastSTEMmode;
  int toCam = -1;

  bool jeolEFTEM = JEOLscope && !mHasOmegaFilter && mJeolSD.JeolEFTEM >= 0;
  ISX = 0.;
  ISY = 0.;
  if (!mLastMagIndex && firstTime)
    mLastMagIndex = GetMagIndex();
  if (magInd < 0)
    magInd = mLastMagIndex;
  firstTime = false;
  if (!magInd || magInd >= MAX_MAGS)
    return;

  // Set image shift value to the sum of neutral and offset values
  if (STEMmode < 0 && mWinApp->ScopeHasSTEM())
    STEMmode = GetSTEMmode() ? 1 : 0;
  if (STEMmode <= 0) {
    if (jeolEFTEM) {

      // When JEOL EFTEM is switching, the neutral index has the new state, needed here
      ISX = mMagTab[magInd].neutralISX[mNeutralIndex] + mDetectorOffsetX +
        (mApplyISoffset ? mMagTab[magInd].calOffsetISX[mNeutralIndex] : 0.);
      ISY = mMagTab[magInd].neutralISY[mNeutralIndex] + mDetectorOffsetY +
        (mApplyISoffset ? mMagTab[magInd].calOffsetISY[mNeutralIndex] : 0.);
    } else {
      ISX = mMagTab[magInd].neutralISX[mNeutralIndex] + mDetectorOffsetX +
        mMagTab[magInd].offsetISX;
      ISY = mMagTab[magInd].neutralISY[mNeutralIndex] + mDetectorOffsetY +
        mMagTab[magInd].offsetISY;
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
  ScaleMat aMat = mShiftManager->IStoSpecimen(magInd, toCam);
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
    if (FEIscope && mUseImageBeamTilt)
      mPlugFuncs->GetImageBeamTilt(&tiltX, &tiltY);
    else
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
      if (FEIscope && mUseImageBeamTilt)
        mPlugFuncs->GetImageBeamTilt(&plugX, &plugY);
      else
        mPlugFuncs->GetBeamTilt(&plugX, &plugY);
      tiltX += plugX;
      tiltY += plugY;
    }
    if (FEIscope && mUseImageBeamTilt)
      mPlugFuncs->SetImageBeamTilt(tiltX, tiltY);
    else
      mPlugFuncs->SetBeamTilt(tiltX, tiltY);
    if (!JEOLscope)
      SEMTrace('b', "SetBeamTilt %.3f %.3f", tiltX, tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Beam Tilt "));
    success = false;
  } 
  ScopeMutexRelease("ChangeBeamTilt");
  return success;
}

BOOL CEMscope::GetImageBeamTilt(double & tiltX, double & tiltY)
{
  BOOL success = true;

  if (!sInitialized || !mPlugFuncs->GetImageBeamTilt)
    return false;

  ScopeMutexAcquire("GetImageBeamTilt", true);

  try {
    mPlugFuncs->GetImageBeamTilt(&tiltX, &tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Image-Beam Tilt "));
    success = false;
  }
  ScopeMutexRelease("GetImageBeamTilt");
  return success;
}

BOOL CEMscope::SetImageBeamTilt(double tiltX, double tiltY)
{
  BOOL success = true;
  if (!sInitialized || !mPlugFuncs->SetImageBeamTilt)
    return false;
  ScopeMutexAcquire("SetImageBeamTilt", true);

  try {
    mPlugFuncs->SetImageBeamTilt(tiltX, tiltY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Image-Beam Tilt "));
    success = false;
  }
  ScopeMutexRelease("SetImageBeamTilt");
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

// Diffraction shift
BOOL CEMscope::GetDiffractionShift(double & shiftX, double & shiftY)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->GetDiffractionShift)
    return false;

  ScopeMutexAcquire("GetDiffractionShift", true);

  try {
    mPlugFuncs->GetDiffractionShift(&shiftX, &shiftY);
    shiftX /= mDiffShiftScaling;
    shiftY /= mDiffShiftScaling;
  }
  catch (_com_error E) {
    success = false;
    SEMReportCOMError(E, _T("getting Diffraction Shift "));
  }
  ScopeMutexRelease("GetDiffractionShift");
  return success;
}


BOOL CEMscope::SetDiffractionShift(double shiftX, double shiftY)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->SetDiffractionShift)
    return false;

  ScopeMutexAcquire("SetDiffractionShift", true);

  try {
    mPlugFuncs->SetDiffractionShift(shiftX * mDiffShiftScaling, 
      shiftY * mDiffShiftScaling);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Diffraction Shift "));
    success = false;
  }
  ScopeMutexRelease("SetDiffractionShift");
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

// Get the condenser stigmator, range -1 to 1
bool CEMscope::GetCondenserStigmator(double & stigX, double & stigY)
{
  bool success = true;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("GetCondenserStigmator", true);

  try {
    mPlugFuncs->GetCondenserStigmator(&stigX, &stigY);
  }
  catch (_com_error E) {
    success = false;
    SEMReportCOMError(E, _T("getting Condenser Stigmator "));
  }
  ScopeMutexRelease("GetCondenserStigmator");
  return success;
}

// Set the condenser stigmator
bool CEMscope::SetCondenserStigmator(double stigX, double stigY)
{
  bool success = true;
  if (!sInitialized)
    return false;

  ScopeMutexAcquire("SetCondenserStigmator", true);

  try {
    mPlugFuncs->SetCondenserStigmator(stigX, stigY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Condenser Stigmator "));
    success = false;
  }
  ScopeMutexRelease("SetCondenserStigmator");
  return success;
}

// Get the diffraction stigmator, range -1 to 1
bool CEMscope::GetDiffractionStigmator(double & stigX, double & stigY)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->GetDiffractionStigmator)
    return false;

  ScopeMutexAcquire("GetDiffractionStigmator", true);

  try {
    mPlugFuncs->GetDiffractionStigmator(&stigX, &stigY);
  }
  catch (_com_error E) {
    success = false;
    SEMReportCOMError(E, _T("getting Diffraction Stigmator "));
  }
  ScopeMutexRelease("GetDiffractionStigmator");
  return success;
}

// Set the diffraction stigmator
bool CEMscope::SetDiffractionStigmator(double stigX, double stigY)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->SetDiffractionStigmator)
    return false;

  ScopeMutexAcquire("SetDiffractionStigmator", true);

  try {
    mPlugFuncs->SetDiffractionStigmator(stigX, stigY);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting Diffraction Stigmator "));
    success = false;
  }
  ScopeMutexRelease("SetDiffractionStigmator");
  return success;
}

// X LENS FUNCTIONS
#define XLENS_ERROR_STARTUP(a) \
  int retval = 0; \
  if (!sInitialized)  \
    return 1;  \
  if(!mPlugFuncs->a)  \
    return 2;  \
  if (mXLensModeAvailable <= 0) \
    return 5 - mXLensModeAvailable;

#define XLENS_GETSETXY(a, x, y) \
int CEMscope::a##Deflector(int which, double x, double y) \
{  \
  CString mess;  \
  if (which < 1 || which > 3)  \
    return 3;  \
  XLENS_ERROR_STARTUP(a##Shift); \
  ScopeMutexAcquire(#a"Deflector", true);  \
  try {   \
    if (which == 1) { \
      mess = "Shift";  \
      mPlugFuncs->a##Shift(x, y); \
    } else if (which == 2) { \
      mess = "Tilt";  \
      mPlugFuncs->a##Tilt(x, y); \
    } else { \
      mess = "Stigmator";  \
      mPlugFuncs->a##Stigmator(x, y); \
    } \
  }  \
  catch (_com_error E) {  \
    retval = 4;  \
    SEMReportCOMError(E, _T("calling "#a + mess + " "));  \
  }  \
  ScopeMutexRelease(#a"Deflector");  \
  return retval;  \
}

XLENS_GETSETXY(GetXLens, &outX, &outY);
XLENS_GETSETXY(SetXLens, inX, inY);

#define XLENS_GETSETFOCUS(a, typ, x, c) \
int CEMscope::a(typ x)  \
{  \
  XLENS_ERROR_STARTUP(a);  \
  ScopeMutexAcquire(#a, true);  \
  try {  \
    c; \
  }  \
  catch (_com_error E) {  \
    retval = 4;  \
    SEMReportCOMError(E, _T("calling "#a" "));  \
  }  \
  ScopeMutexRelease(#a);  \
  return retval;  \
}

XLENS_GETSETFOCUS(GetXLensFocus, double, &outX, outX = mPlugFuncs->GetXLensFocus());
XLENS_GETSETFOCUS(SetXLensFocus, double, inX, mPlugFuncs->SetXLensFocus(inX));
XLENS_GETSETFOCUS(GetXLensMode, int, &outX, outX = mPlugFuncs->GetXLensMode());
XLENS_GETSETFOCUS(SetXLensMode, BOOL, inX, mPlugFuncs->SetXLensMode(inX));

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
    sGettingValuesFast));
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
  if (!sInitialized || mDisconnected)
    return spUnknown;

  // Plugin will use state value if update by event or getting fast
  bool needMutex = !(JEOLscope && ((mScreenByEvent && mJeolSD.eventDataIsGood) || 
    sGettingValuesFast || !mReportsLargeScreen));
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
    SEMMessageBox(_T("Screen is already moving"));
    return false;
  }

  if (mWinApp->GetEFTEMMode() && mCanControlEFTEM) {
    BOOL needed = (inPos == spUp || !mFiltParam->autoMag ||
      mWinApp->mMultiTSTasks->AutocenTrackingState());
    SetEFTEM(needed);
  }
  
  mMoveInfo.axisBits = inPos;
  mMoveInfo.finishedTick = (inPos == spUp) ? mScreenRaiseDelay : 0;
  mScreenThread = AfxBeginThread(ScreenMoveProc, &mMoveInfo,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("ScreenMoveProc thread created with ID 0x%0x\n",mScreenThread->m_nThreadID);
  mScreenThread->m_bAutoDelete = false;
  mScreenThread->ResumeThread();
  // This used to be done for "singleTecnai"
  //mWinApp->AddIdleTask(TaskScreenBusy, NULL, TaskScreenError, 0, 60000);
  return true;
}

BOOL CEMscope::SynchronousScreenPos(int inPos)
{
  double startTime = GetTickCount();
  if (!SetScreenPos(inPos))
    return false;
  while (SEMTickInterval(startTime) < 20000) {
    if (!ScreenBusy())
      return true;
    SleepMsg(50);
  }
  SEMMessageBox("Timeout setting screen position");
  return false;
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
  if (!retval && info->finishedTick > 0)
    Sleep(info->finishedTick);
  CoUninitialize();
  ScopeMutexRelease("ScreenMoveProc");
  return retval;
}

// Function to set static
void CEMscope::SetCheckPosOnScreenError(BOOL inVal)
{
  sCheckPosOnScreenError = inVal;
}
BOOL CEMscope::GetCheckPosOnScreenError() {return sCheckPosOnScreenError;};

// Filament current: so far on JEOL only
double CEMscope::GetFilamentCurrent()
{
  double result = -1.;
  if (!sInitialized || !mPlugFuncs->GetFilamentCurrent)
    return -1.;
  ScopeMutexAcquire("GetFilamentCurrent", true);
  try {
    result = mPlugFuncs->GetFilamentCurrent() * mFilamentCurrentScale;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting filament current "));
  }
  ScopeMutexRelease("GetFilamentCurrent");
  return result;
}

BOOL CEMscope::SetFilamentCurrent(double current)
{
  BOOL success = true;
  if (!sInitialized || !mPlugFuncs->SetFilamentCurrent)
    return false;
  ScopeMutexAcquire("SetFilamentCurrent", true);
  try {
    mPlugFuncs->SetFilamentCurrent(current / mFilamentCurrentScale);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting filament current "));
    success = false;
  }
  ScopeMutexRelease("SetFilamentCurrent");
  return success;
}

// Emission state: read/write on JEOL, read-only on FEI ASI
BOOL CEMscope::GetEmissionState(int & state)
{
  BOOL success = true;
  state = -1;
  if (!sInitialized || !mPlugFuncs->GetEmissionState)
    return false;
  ScopeMutexAcquire("GetEmissionState", true);
  try {
    state = mPlugFuncs->GetEmissionState();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting FEG emission state "));
  }
  ScopeMutexRelease("GetEmissionState");
  return success;
}

BOOL CEMscope::SetEmissionState(int state)
{
  BOOL success = true;
  if (!sInitialized || !mPlugFuncs->SetEmissionState)
    return false;
  ScopeMutexAcquire("SetEmissionState", true);
  try {
    mPlugFuncs->SetEmissionState(state);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting FEG emission state "));
  }
  ScopeMutexRelease("SetEmissionState");
  return success;
}

// MAGNIFICATION AND PROJECTOR NORMALIZATION

// Get the magnification value
double CEMscope::GetMagnification()
{
  double result;
  if (!sInitialized)
    return 0.;

  // Plugin will use state value if update by event or getting fast, and not scanning
  bool needMutex = !(JEOLscope && (mJeolSD.eventDataIsGood || sGettingValuesFast) && 
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
  if (!sInitialized || mDisconnected)
    return -1;

  // Plugin will use state value only if getting fast and mag is valid
  if (JEOLscope && (mJeolSD.eventDataIsGood || sGettingValuesFast) && !forceGet && 
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
        if (minDiff >= 1. && !mScanningMags)
          SEMTrace('1', "WARNING: GetMagIndex did not find exact match to %.0f in STEM"
          " mag table (closest differs by %f)", curMag, minDiff);
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

  // If we are using only one STEM LM mag range, transpose to other range
  if (mFeiSTEMprobeModeInLM == 1 && result >= mLowestMicroSTEMmag && 
    result < mLowestSTEMnonLMmag[1])
    result -= mLowestMicroSTEMmag - 1;
  else if (mFeiSTEMprobeModeInLM > 1 && result < mLowestSTEMnonLMmag[0] && 
    mLowestSTEMnonLMmag[1] > mLowestMicroSTEMmag)
    result += mLowestMicroSTEMmag - 1;
  return result;
}

// Set the magnification by its index
BOOL CEMscope::SetMagIndex(int inIndex)
{
  double tranISX, tranISY, curISX, curISY;
  double axisISX, axisISY;
  BOOL result = true;
  BOOL convertIS, restoreIS, ifSTEM;
  bool unblankAfter;
  int lowestM, ticks, newLowestM;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  const char *routine = "SetMagIndex";
  ScaleMat bsmFrom, bsmTo;
  double newPCX, newPCY, delBSX = 0., delBSY = 0.;
  double posChangingISX, posChangingISY;

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
  lowestM = ifSTEM ? mLowestSTEMnonLMmag[mProbeMode] : GetLowestMModeMagInd();
  if (ifSTEM) {
    newLowestM = lowestM;
    if (!BOOL_EQUIV(inIndex < mLowestMicroSTEMmag, currentIndex < mLowestMicroSTEMmag))
      newLowestM = mLowestSTEMnonLMmag[1 - mProbeMode];
  }
      

  // There is no way to set the projection submode (which is psmLAD in STEM low mag and 
  // psmD in nonLM) so we cannot go between LM and nonLM
  if (FEIscope && ifSTEM && !BOOL_EQUIV(inIndex < newLowestM, currentIndex < lowestM)) {
    if (!UtapiSupportsService(UTSUP_MAGNIFICATION))
      return false;
    lowestM = inIndex < newLowestM ? -2 : -1;
  }

  if (GetDebugOutput('i')) {
    GetTiltAxisIS(axisISX, axisISY);
    GetImageShift(curISX, curISY);
    PrintfToLog("SetMagIndex: IS at start raw %.3f %.3f  net %.3f %.3f", curISX + axisISX,
      curISY + axisISY, curISX, curISY);
  }

  // Compute beam shift change that happens due to change in beam shift cal on JEOL
  if (JEOLscope && !ifSTEM && currentIndex >= lowestM && inIndex >= lowestM) {
    bsmFrom = mShiftManager->GetBeamShiftCal(currentIndex);
    bsmTo = mShiftManager->GetBeamShiftCal(inIndex);
    if (bsmFrom.xpx != bsmTo.xpx && bsmFrom.xpx && bsmTo.xpx) {
      GetImageShift(curISX, curISY);
      PositionChangingPartOfIS(curISX, curISY, posChangingISX, posChangingISY);
      mShiftManager->TransferGeneralIS(currentIndex, posChangingISX, posChangingISY,
        inIndex, newPCX, newPCY);
      delBSX = bsmTo.xpx * newPCX + bsmTo.xpy * newPCY -
        (bsmFrom.xpx * posChangingISX + bsmFrom.xpy * posChangingISY);
      delBSY = bsmTo.ypx * newPCX + bsmTo.ypy * newPCY -
        (bsmFrom.ypx * posChangingISX + bsmFrom.ypy * posChangingISY);
      /*PrintfToLog("PCIS %.3f %.3f  newPC %.3f %.3f delBS %.5g %.5g", posChangingISX,
      posChangingISY, newPCX, newPCY, delBSX, delBSY);*/
    }
  }

  convertIS = AssessMagISchange(currentIndex, inIndex, ifSTEM, tranISX, tranISY);
  ScopeMutexAcquire(routine, true);

  SaveISifNeeded(currentIndex, inIndex);
  if (!ifSTEM)
    restoreIS = AssessRestoreIS(currentIndex, inIndex, tranISX, tranISY);

  unblankAfter = BlankTransientIfNeeded(routine);
  mSynchroTD.initialSleep = 0;
  mSynchroTD.ifSTEM = ifSTEM;
  mSynchroTD.lowestM = lowestM;
  mSynchroTD.normalize = (mSkipNormalizations & 1) ? 0 : 1;
  mSynchroTD.newProbeMode = mProbeMode;

  // JEOL STEM dies if mag is changed too soon after last image; 
  if (JEOLscope && ifSTEM && !camParam->pluginName.IsEmpty()) {
    ticks = (int)SEMTickInterval(1000. * mCamera->GetLastImageTime());
    mSynchroTD.initialSleep = B3DMAX(0, mJeolSTEMPreMagDelay - ticks);
  }
  if (ifSTEM)
    mSynchroTD.STEMmag = mMagTab[inIndex].STEMmag;

  result = RunSynchronousThread(SYNCHRO_DO_MAG, inIndex, currentIndex, routine);
  UnblankAfterTransient(unblankAfter, routine);
  if (result) {
    if (!ifSTEM) {
      mLastNormalization = mSynchroTD.lastNormalizationTime;
      mLastNormMagIndex = inIndex;
      mPrevNormMagIndex = currentIndex;
    } else
      mProbeMode = mSynchroTD.newProbeMode;

    // set so that this is not detected as a user change
    SetMagChanged(inIndex);

    // Mode used to be handled here too.
    // Plugin changes the mode after the delay, but not the mag.
    if (JEOLscope) {
      SEMSetJeolStateMag(inIndex, true);
      IncOrAccumulateBeamShift(delBSX, delBSY, "SetMagIndex");
    }
    if (!ifSTEM)
      SetSecondaryModeForMag(inIndex);

    // Falcon Dose Protector may respond to transient conditions after a mag change,
    // such as a temporary setting of objective strength, so there needs to be a delay
    if (IS_FALCON2_3_4(camParam))
      mShiftManager->SetGeneralTimeOut(mLastNormalization, mFalconPostMagDelay);
    HandleNewMag(inIndex);
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
      GetTiltAxisIS(axisISX, axisISY);
      if (GetDebugOutput('i')) {
        GetImageShift(curISX, curISY);
        PrintfToLog("SetMagIndex: IS after raw %.3f %.3f  net %.3f %.3f  setting to "
          "r %.3f %.3f  n %.3f %.3f", curISX + axisISX, curISY + axisISY, curISX, curISY,
          tranISX, tranISY, tranISX - axisISX, tranISY - axisISY);
      }
      SetImageShift(tranISX - axisISX, tranISY - axisISY);
    }
  }
  ScopeMutexRelease(routine);
  return result;
}

BOOL CEMscope::SetMagKernel(SynchroThreadData *sytd)
{
  DWORD ticks;
  double focusStart, startTime, startISX, startISY, curISX, curISY;
  bool focusChanged = false, ISchanged, normAll;
  int mode;
  int currentIndex = sytd->curIndex, inIndex = sytd->newIndex;
  int relaxFlag = RELAX_FOR_MAG;
  if (!BOOL_EQUIV(sytd->curIndex < sytd->lowestM, sytd->newIndex < sytd->lowestM) &&
    !(RELAX_FOR_MAG & sJeolRelaxationFlags))
    relaxFlag = RELAX_FOR_LM_M;
  try {
    if (JEOLscope) {

      if (sytd->initialSleep > 0)
        Sleep(sytd->initialSleep);

      ticks = GetTickCount();
      PLUGSCOPE_SET(MagnificationIndex, inIndex);
      WaitForLensRelaxation(relaxFlag, ticks);

    } else {  // FEIlike

      if (HitachiScope && !sytd->ifSTEM) {
        focusStart = mPlugFuncs->GetAbsFocus();
        mPlugFuncs->GetImageShift(&startISX, &startISY);
      }

      // Tecnai: if in diffraction mode, set back to imaging
      if (!currentIndex) {
        if (mPlugFuncs->SetImagingMode)
          mPlugFuncs->SetImagingMode(pmImaging);
      }

      if (sytd->ifSTEM) {
        PLUGSCOPE_GET(ProbeMode, mode, 1);
        mode = (mode == imMicroProbe ? 1 : 0);
        sytd->newProbeMode = inIndex >= sytd->lowestMicroSTEMmag ? 1 : 0;
        if (mode != sytd->newProbeMode) {
          PLUGSCOPE_SET(ProbeMode, (sytd->newProbeMode ? imMicroProbe : imNanoProbe));
        }
        if (sytd->lowestM < 0) {
          PLUGSCOPE_SET(ObjectiveMode, -sytd->lowestM - 1);
          Sleep(2000);
        }
        PLUGSCOPE_SET(STEMMagnification, sytd->STEMmag);
      } else {

        // Disable autonormalization, change mag, normalize and reenable
        AUTONORMALIZE_SET(*vFalse);
        PLUGSCOPE_SET(MagnificationIndex, inIndex);

        // Hitachi needs to poll and make sure associated changes are done
        if (HitachiScope) {
          startTime = GetTickCount();
          ISchanged = inIndex >= sytd->lowestMModeMagInd && 
            inIndex < sytd->lowestSecondaryMag;
          while (1) {
            if (SEMTickInterval(startTime) > sytd->HitachiMagFocusDelay ||
              (focusChanged && SEMTickInterval(startTime) > sytd->HitachiMagISDelay)) {
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
      }
    }

    // Normalization: has to be outside of the non-JEOL section now
    if (!sytd->ifSTEM && mPlugFuncs->NormalizeLens && sytd->normalize > 0) {

      // Normalize all lenses if going in or out of LM, or if in LM and option is 1,
      // or always if option is 2.  This used to do condenser in addition, but now
      // it does "all", which adds objective
      normAll = (!BOOL_EQUIV(sytd->curIndex < sytd->lowestM, 
        sytd->newIndex < sytd->lowestM) ||
        (inIndex < sytd->lowestM && sytd->normAllOnMagChange > 0) || 
        sytd->normAllOnMagChange > 1);
      if (normAll && FEIscope) {
        mPlugFuncs->NormalizeLens(ALL_INSTRUMENT_LENSES);
      } else {
        if (!JEOLscope || SEMLookupJeolRelaxData(pnmProjector)) {
          mPlugFuncs->NormalizeLens(pnmProjector);
        } if (normAll) {
          if (!JEOLscope || SEMLookupJeolRelaxData(nmCondenser))
            mPlugFuncs->NormalizeLens(nmCondenser);
          if (!JEOLscope || SEMLookupJeolRelaxData(pnmObjective))
            mPlugFuncs->NormalizeLens(pnmObjective);
        }
      }
    }
    sytd->lastNormalizationTime = GetTickCount();

    if (!JEOLscope)
      AUTONORMALIZE_SET(*vTrue);

  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting magnification "));
    return false;
  }
  return true;
}

// Set mag index directly or via the current low dose area
BOOL CEMscope::SetMagOrAdjustLDArea(int inIndex)
{
  if (AdjustLDAreaForItem(0, inIndex))
    return true;
  return SetMagIndex(inIndex);
}

// Set a parameter (mag or probe mode) by changing low dose area and going to the area if
// continuous mode is on and crossing LM boundary or changing probe mode for View or 
// Search, returning true. Otherwise return false.  Allows defocus offset to be managed
bool CEMscope::AdjustLDAreaForItem(int which, int inItem)
{
  LowDoseParams *ldp;
  LowDoseParams ldpOld;
  int lowestM;
  bool retval = false;
  if (mLowDoseMode && IS_AREA_VIEW_OR_SEARCH(mLowDoseSetArea)
    && !mWinApp->GetSTEMMode() &&
    mWinApp->mLowDoseDlg.DoingContinuousUpdate(mLowDoseSetArea)) {
    ldp = mWinApp->GetLowDoseParams() + mLowDoseSetArea;
    if (!which) {
      lowestM = GetLowestMModeMagInd();
      if (!BOOL_EQUIV(ldp->magIndex < lowestM, inItem < lowestM)) {
        ldpOld = *ldp;
        mWinApp->mLowDoseDlg.TransferISonAxis(ldp->magIndex, ldp->ISX, ldp->ISY, inItem,
          ldp->ISX, ldp->ISY);
        ldp->magIndex = inItem;
        retval = true;
      }
    } else {
      if (ldp->probeMode != inItem) {
        ldpOld = *ldp;
        ldp->probeMode = inItem;
        retval = true;
      }
    }
    if (retval) {
      SetLdsaParams(&ldpOld);
      GotoLowDoseArea(mLowDoseSetArea);
    }
  }
  return retval;
}

bool CEMscope::SetSTEMMagnification(double magVal)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->SetSTEMMagnification)
    return false;
  ScopeMutexAcquire("SetSTEMMagnification", true);
  
  try {
    mPlugFuncs->SetSTEMMagnification(magVal);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting STEM magnification "));
    success = false;
  }
  ScopeMutexRelease("SetSTEMMagnification");
  return success;

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
      realISX = oldISX + axisISX - (mMagTab[fromInd].neutralISX[mNeutralIndex] +
        mDetectorOffsetX) - mMagTab[fromInd].offsetISX;
      realISY = oldISY + axisISY - (mMagTab[fromInd].neutralISY[mNeutralIndex] +
        mDetectorOffsetY) - mMagTab[fromInd].offsetISY;
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
  realISX = mSavedISX - (mMagTab[mMagIndSavedIS].neutralISX[mNeutralIndex] +
    mDetectorOffsetX) - mMagTab[mMagIndSavedIS].offsetISX;
  realISY = mSavedISY - (mMagTab[mMagIndSavedIS].neutralISY[mNeutralIndex] + 
    mDetectorOffsetY) - mMagTab[mMagIndSavedIS].offsetISY;
  mShiftManager->TransferGeneralIS(mMagIndSavedIS, realISX, realISY, toInd, newISX, 
    newISY);
  newISX += mMagTab[toInd].neutralISX[mNeutralIndex] + mDetectorOffsetX + 
    mMagTab[toInd].offsetISX;
  newISY += mMagTab[toInd].neutralISY[mNeutralIndex] + mDetectorOffsetY + 
    mMagTab[toInd].offsetISY;
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
  int lastMag, baseIndex = 0, lowestNonLM[2] = {0, 0};
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

      report = "In STEM mode, you need to go to the lowest STEM\r\nmagnification then "
        "step through the mags with the magnification knob.\r\nYou will step through the "
        "mags in nanoprobe mode then in microprobe mode;\r\npress the \"End\" button when"
        " you reach the highest mag in each mode.\r\nBe sure to enable LMscan in "
        "the microscope interface in order to start at the lowest LM mag.\r\n"
        "Do not try to step to the next mag until the previous step shows up in the "
        "log window!\r\n\r\nNow go to the lowest mag in LM.";
      mWinApp->AppendToLog(report);
      AfxMessageBox(report, MB_EXCLAME);

      // Loop on each mode
      magInd = 1;
      mScanningMags = 1;
      mWinApp->UpdateBufferWindows();
      for (curMode = 0; curMode < 2; curMode++) {
        baseIndex = magInd;
        if (curMode) {
          PLUGSCOPE_SET(ProbeMode, imMicroProbe);
          AfxMessageBox("Go to the lowest LM magnification again for microprobe mode", 
            MB_EXCLAME);
        }

        // Loop within the mode until they say it is done
        for (; magInd < 2 * MAX_MAGS && mScanningMags > 0; magInd++) {
          report = "Go to the next higher magnification\n\n";
          while (mScanningMags > 0) {
            PLUGSCOPE_GET(STEMMagnification, curSMag, 1.);
            if (curSMag < lastSMag && magInd > baseIndex) {
              if (!lowestNonLM[curMode])
                lowestNonLM[curMode] = magInd;
              else
                lowestNonLM[curMode] = -1;
            }
            if (curSMag != lastSMag)
              break;
            SleepMsg(50);
          }
          if (curSMag != lastSMag) {
            str.Format("%d %.1f", magInd, curSMag);
            mWinApp->AppendToLog(str);
          } else
            magInd--;
          lastSMag = curSMag;
        }
        if (!mScanningMags)
          break;
        if (!curMode)
          mScanningMags = 1;
      }
      mScanningMags = 0;
      mWinApp->UpdateBufferWindows();
      report.Format("Set:\r\n LowestMicroSTEMmag %d\r\n", baseIndex);  
      if (lowestNonLM[0] > 0 && lowestNonLM[1] > 0)
        str.Format(" LowestSTEMnonLMmag  %d %d\r\n", lowestNonLM[0], lowestNonLM[1]);
      else
        str = "Also note the two indexes where"
        " non LM mags start,\r\n  and put both in a LowestSTEMnonLMmag property entry";
      mWinApp->AppendToLog(report + str);
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
          if (mSimulationMode && UtapiSupportsService(UTSUP_MAGNIFICATION))
            curSMag = 0.;
          else
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
      if (FEIscope && GetEFTEM()) {
        for (; magInd < MAX_MAGS; magInd++) {
          if (!mMagTab[magInd].mag)
            break;
          PrintfToLog("%d %8d %5d %8d %8d %5d", magInd, mMagTab[magInd].mag, 
            B3DNINT(mMagTab[magInd].tecnaiRotation), mMagTab[magInd].screenMag, 0, 0);
        }
      }
      if (!mode || (numModes == 4 && mode == 1)) {
        if (!mode)
          firstBase = magInd;
        baseIndex += magInd - 1;
      } else if (STEMmode || mode == numModes - 2) {
        if (STEMmode)
          report.Format("Set:\r\n LowestSTEMnonLMmag %d",  baseIndex + 1);
        else if (numModes == 4) 
          report.Format("Set:\r\n LowestMModeMagIndex %d\r\nSet:\r\n LowestSecondaryMag"
          "%d\r\nCameraLengthTable:", firstBase, baseIndex + 1);
        else
          report.Format("Set:\r\n LowestMModeMagIndex %d\r\n\r\nCameraLengthTable:", 
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
  ScopeMutexRelease("ScanMagnifications");
  if (startingMag > 0)
    SetMagIndex(startingMag);
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
    mLastCamLenIndex = result;
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
  int indCalled = inIndex;

  if (!sInitialized)
    return false;
  
  int magInd, lowestM, currentIndex = GetCamLenIndex();
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
        lowestM = GetLowestMModeMagInd();

        // Go to a mag in the right range if necessary then go to diffraction
        // 7/25/19: diffraction shift may be bad if you come from M, come from SA instead
        if (magInd < lowestM && inIndex < LAD_INDEX_BASE) {
          PLUGSCOPE_SET(MagnificationIndex, lowestM + 5);
        } else if (magInd >= lowestM && inIndex > LAD_INDEX_BASE) {
          PLUGSCOPE_SET(MagnificationIndex, lowestM - 1);
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
    mLastCamLenIndex = indCalled;
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
    startMag = GetLowestMModeMagInd();
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
    if (mPlugFuncs->NormalizeLens && (!JEOLscope || SEMLookupJeolRelaxData(pnmProjector)))
    {
      unblankAfter = BlankTransientIfNeeded(routine);
      mPlugFuncs->NormalizeLens(pnmProjector);
      UnblankAfterTransient(unblankAfter, routine);
    }
    mMagChanged = false;
    mLastNormalization = GetTickCount();
    mLastNormMagIndex = FastMagIndex();
    mPrevNormMagIndex = mLastNormMagIndex;
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
    if (mPlugFuncs->NormalizeLens && (!JEOLscope || SEMLookupJeolRelaxData(nmCondenser))){
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
    if (mPlugFuncs->NormalizeLens && (!JEOLscope || SEMLookupJeolRelaxData(pnmObjective)))
    {
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
  CString str;
  const char *routine = "NormalizeAll";

  if (!sInitialized)
    return false;
  if (!(FEIscope || (JEOLscope && !((illumProj & 1) && !SEMLookupJeolRelaxData(nmAll)) &&
    !((illumProj & 2) && !SEMLookupJeolRelaxData(pnmAll))))) {
    if (JEOLscope) {
      str.Format("The NormalizeAll function is available for JEOL\n"
        "only with one or two of the lens groups defined\n"
        " with the JeolLensRelaxProgram property with indexes %d and %d", pnmAll, nmAll);
        SEMMessageBox(str);
    } else
      SEMMessageBox("The NormalizeAll function is not available for this scope");
    return false;
  }

  if (mPlugFuncs->NormalizeLens) {
    unblankAfter = BlankTransientIfNeeded(routine);
    if (!illumProj) {
      success = RunSynchronousThread(SYNCHRO_DO_NORM, ALL_INSTRUMENT_LENSES, 0, NULL);
    } else {
      if (illumProj & 1)
        success = RunSynchronousThread(SYNCHRO_DO_NORM, nmAll, 0, NULL);
      if (success && (illumProj & 2))
        success = RunSynchronousThread(SYNCHRO_DO_NORM, pnmAll, 0, NULL);
    }
    UnblankAfterTransient(unblankAfter, routine);
  }
  return success;
}

BOOL CEMscope::NormalizeKernel(SynchroThreadData *sytd)
{
  try {
    mPlugFuncs->NormalizeLens(sytd->newIndex);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("normalizing lenses "));
    return false;
  }
  return true;
}

// Look through the array of lens programs for one matching the normalization index
LensRelaxData *SEMLookupJeolRelaxData(int normInd)
{
  CEMscope *scope = ((CSerialEMApp *)AfxGetApp())->mScope;
  LensRelaxData *ret;
  CArray<LensRelaxData, LensRelaxData> *relaxProgs = scope->GetLensRelaxProgs();
  int ind;
  for (ind = 0; ind < (int)relaxProgs->GetSize(); ind++) {
    LensRelaxData &relax = relaxProgs->ElementAt(ind);
    if (relax.normIndex == normInd) {
      ret = relaxProgs->GetData();
      return &ret[ind];
    }
  }
  return NULL;
}

/* Return Defocus in Microns */
double CEMscope::GetDefocus()
{ 
  double result;

  if (!sInitialized)
    return 0.;

  // 11/30/10: There is no focus value by event! Eliminate forceread, use of stored value
  // Plugin recomputes it when getting fast
  bool needMutex = !(JEOLscope && sGettingValuesFast);
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

// Return the objective strength
double CEMscope::GetObjectiveStrength(void)
{
  double result;
  if (!sInitialized)
    return 0.;

  ScopeMutexAcquire("GetObjectiveStrength", true);
  try {
    PLUGSCOPE_GET(ObjectiveStrength, result, 1.);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting objective strength "));
    result = 0.;
  }
  ScopeMutexRelease("GetObjectiveStrength");
  return result;
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
  
  // USE THIS SO EFTEM FINDS A VALUE SET AT HIGHEST LM FOR TEM
  // SWITCH BACK TO GET FUNCTION IF SEPARATE EFTEM VALUES GET STORED
  int lowestM = mLowestMModeMagInd;
  double focus = magInd >= lowestM ? -999 : mStandardLMFocus;
  HitachiParams *hParams = mWinApp->GetHitachiParams();
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  for (i = 1; i < MAX_MAGS; i++) {
    if (mLMFocusTable[i][probe] > -900. &&
      (magInd >= lowestM ? 1 : 0) == (i >= lowestM ? 1 : 0)) {
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
  if (!mInScopeUpdate)
    SEMTrace('c', "GetStandardLMFocus: mag %d, nearest mag %d, focus %f", magInd, nearInd,
      focus);
  return focus;
}

// Set the focus the the standard LM focus if this is LM and standard focus exists
void CEMscope::SetFocusToStandardIfLM(int magInd)
{
  if (mWinApp->GetSTEMMode() || magInd >= GetLowestMModeMagInd())
    return;
  double focus = GetStandardLMFocus(magInd);
  if (focus > -900.)
    SetFocus(focus);
}

// Set up or end blanking mode when screen down in low dose
void CEMscope::SetBlankWhenDown(BOOL inVal)
{
  mBlankWhenDown = inVal;
  BlankBeam(NeedBeamBlanking(GetScreenPos(), GetSTEMmode()), "SetBlankWhenDown");
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
    (inVal && mBeamBlanked && B3DABS(mShutterlessCamera) < 2)) {
      BlankBeam(!inVal, "SetCameraAcquiring");
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
  BlankBeam(NeedBeamBlanking(GetScreenPos(), camParam->STEMcamera), 
    "SetShutterlessCamera");
}

// Set mode, let Update figure out what to do
void CEMscope::SetLowDoseMode(BOOL inVal, BOOL hidingOffState)
{
  // If mode is being turned off, return to record parameters, unblank here
  if (mLowDoseMode && !inVal) {
    int screenPos = FastScreenPos();
    if (hidingOffState && screenPos == spDown)
      BlankBeam(true, "SetLowDoseMode");
    GotoLowDoseArea(3);
    RestoreFromFreeLens(3, -1);
    if (!hidingOffState && !NeedBeamBlanking(GetScreenPos(), FastSTEMmode()))
      BlankBeam(false, "SetLowDoseMode");
    mLowDoseSetArea = -1;
  }
  mLowDoseMode = inVal;
}

// Blank or unblank the beam; keep track of requested setting
void CEMscope::BlankBeam(BOOL inVal, const char *fromWhere)
{
  if (!sInitialized || mJeol1230 || mDisconnected)
    return;
  ScopeMutexAcquire("BlankBeam", true);
  try {
    PLUGSCOPE_SET(BeamBlank, VAR_BOOL(inVal));
    SEMTrace('B', "%s Set beam blank %s", fromWhere ? fromWhere : "", 
      inVal ? "ON" : "OFF");
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting beam blank "));
  }
  mBeamBlankSet = inVal;
  mBeamBlanked = inVal;
  ScopeMutexRelease("BlankBeam");
}

BOOL CEMscope::GetBeamBlanked()
{
  BOOL retval = false;
  if (!sInitialized || mJeol1230 || !mPlugFuncs->GetBeamBlank)
    return false;
  ScopeMutexAcquire("GetBeamBlanked", true);
  try {
    retval = mPlugFuncs->GetBeamBlank();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting beam blank "));
  }
  ScopeMutexRelease("GetBeamBlanked");
  return retval;
}

// Blanks beam for a routine creating transients if the flag is set, returns true if it
// did.  This and the unblank function no longer need to be called from a try block
bool CEMscope::BlankTransientIfNeeded(const char *routine)
{   
  if ((mBlankTransients || mCamera->DoingContinuousAcquire()) && !mBeamBlanked &&
    mPlugFuncs->SetBeamBlank != NULL) {
    BlankBeam(true, routine);
    return true;
  }
  return false;
}

// Unlbank after procedure if it was blanked
void CEMscope::UnblankAfterTransient(bool needUnblank, const char *routine)
{
  if (needUnblank)
    BlankBeam(false, routine);
}

// Change to a new low dose area
void CEMscope::GotoLowDoseArea(int newArea)
{
  double delISX, delISY, beamDelX, beamDelY, startBeamX, startBeamY;
  double curISX, curISY, newISX, newISY, centeredISX = 0., centeredISY = 0., intensity;
  int curAlpha, curMagInd;
  DWORD magTime;
  CString errStr;
  CString *setNames = mWinApp->GetModeNames();
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();
  LowDoseParams *ldArea = ldParams + newArea;
  BOOL filterChanged = false;
  BOOL alphaDone, ISdone = false;
  BOOL STEMmode = mWinApp->GetSTEMMode();
  bool fromFocTrial = mLowDoseSetArea == FOCUS_CONSET || mLowDoseSetArea == TRIAL_CONSET;
  bool toFocTrial = newArea == FOCUS_CONSET || newArea == TRIAL_CONSET;
  bool fromSearch = mLowDoseSetArea == SEARCH_AREA;
  bool toSearch = newArea == SEARCH_AREA;
  bool fromView = mLowDoseSetArea == VIEW_CONSET;
  bool toView = newArea == VIEW_CONSET;
  bool toViewOK, fromViewOK, toSearchOK, fromSearchOK, removeAddDefocus, adjForFocus;
  bool splitBeamShift, leavingLowMag, enteringLowMag, deferJEOLspot, manage = false;
  bool probeDone = true, changingAtZeroIS, sameIntensity, needCondenserNorm = false;
  BOOL bDebug = GetDebugOutput('b');
  BOOL lDebug = GetDebugOutput('l');
  int setNum, oldArea = mLowDoseSetArea;
  int lowestM = GetLowestMModeMagInd();
  if (!sInitialized || mChangingLDArea || sClippingIS || mDisconnected)
    return;

  mChangingLDArea = -1;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "CHANGING LD AREA");
  mWinApp->mLowDoseDlg.DeselectGoToButtons(newArea);

  // Use designated params if set by nav helper, otherwise use set area params
  // Leave it at NULL for now, it is protected by tests for oldArea >= 0 below and needs
  // be tested for with mLowDoseSetArea in DoISforLowDoseArea
  if (!mLdsaParams && oldArea >= 0)
    mLdsaParams = ldParams + (oldArea >= 0 ? oldArea : RECORD_CONSET);

  if (oldArea < 0)
    curMagInd = GetMagIndex();

  // Get some useful flags about what is changing and what to do
  leavingLowMag = !STEMmode && ldArea->magIndex >= lowestM &&
    ((oldArea >= 0 && mLdsaParams->magIndex && mLdsaParams->magIndex < lowestM)
      || (oldArea < 0 && curMagInd < lowestM));
  splitBeamShift = !STEMmode && ((oldArea >= 0 && leavingLowMag) ||
    ((oldArea < 0 || mLdsaParams->magIndex >= lowestM) &&
      ldArea->magIndex && ldArea->magIndex < lowestM));
  changingAtZeroIS = !STEMmode &&  mChangeAreaAtZeroIS &&
    oldArea >= 0 && mLdsaParams->magIndex !=  ldArea->magIndex;
  enteringLowMag = !STEMmode && ((oldArea < 0 && curMagInd >= lowestM) ||
    (oldArea >= 0 && mLdsaParams->magIndex >= lowestM)) && ldArea->magIndex &&
    ldArea->magIndex < lowestM;
  removeAddDefocus = leavingLowMag || enteringLowMag || 
    (ldArea->probeMode >= 0 && ldArea->probeMode != mProbeMode);
  deferJEOLspot = !mJeol1230 && JEOLscope && (leavingLowMag || enteringLowMag);
  toViewOK = toView && ldArea->magIndex > 0;
  toSearchOK = toSearch && ldArea->magIndex > 0;
  fromViewOK = fromView && mLdsaParams->magIndex > 0;
  fromSearchOK = fromSearch && mLdsaParams->magIndex > 0;
  adjForFocus = 
    (((mLowDoseSetArea == VIEW_CONSET || newArea == VIEW_CONSET) && mLDViewDefocus) ||
    ((mLowDoseSetArea == SEARCH_AREA || newArea == SEARCH_AREA) && mSearchDefocus)) &&
    mLowDoseSetArea != newArea && mShiftManager->GetFocusISCals()->GetSize() > 0 &&
    mShiftManager->GetFocusMagCals()->GetSize();

  if (oldArea < 0 && ldParams[RECORD_CONSET].magIndex > 0 && (mGoToRecMagEnteringLD > 1 ||
    (mGoToRecMagEnteringLD == 1 && leavingLowMag)))
    SetMagIndex(ldParams[RECORD_CONSET].magIndex);

  if (GetDebugOutput('L'))
    GetImageShift(curISX, curISY);
  if (bDebug || lDebug)
    PrintfToLog("\r\nGotoLowDoseArea: %d: focus at start %.2f  update count %d", newArea, 
      GetDefocus(), mAutosaveCount);
  if (bDebug && lDebug && !STEMmode) {
    GetBeamShift(startBeamX, startBeamY);
    GetBeamTilt(curISX, curISY);
  } else if (JEOLscope && !STEMmode) {
    GetBeamShift(startBeamX, startBeamY);
  }
  mLDChangeCumulBeamX = mLDChangeCumulBeamY = 0.;
  mSetAlphaBeamTiltX = EXTRA_NO_VALUE;

  // Notify dialog so it can finish setting beam shift
  if (newArea != oldArea)
    mWinApp->mLowDoseDlg.AreaChanging(newArea);

  // Keep track of whether Focus is being reached after View
  if (mWinApp->mLowDoseDlg.SameAsFocusArea(newArea) && 
    !mWinApp->mLowDoseDlg.SameAsFocusArea(oldArea))
    mFocusCameFromView = fromView;

  // Undo free lens changes before anything else
  RestoreFromFreeLens(oldArea, newArea);

  // If normalizing beam, do it unless going to View or Search.
  // Do it differently depending on the property
  if (!STEMmode && mLDNormalizeBeam && !toView && !toSearch) {
    sameIntensity = oldArea >= 0 && ldArea->spotSize == mLdsaParams->spotSize &&
      ldArea->intensity == mLdsaParams->intensity &&
      ldArea->probeMode == mLdsaParams->probeMode && 
      ldArea->beamAlpha == mLdsaParams->beamAlpha;
    if (mUseNormForLDNormalize) {

      // If using condenser normalization, do it if not going to view or search and not
      // going between tied focus/trial
      needCondenserNorm = !toView && !toSearch && !sameIntensity;
    } else {

      // Classic: set beam for view area if not going to View or Search, not coming from
      // view anyway, and intensity changes
      if (oldArea > 0 && !sameIntensity && ldArea->spotSize && ldParams[0].spotSize &&
        ldArea->intensity && ldParams[0].intensity) {

        SetSpotSize(ldParams[0].spotSize);
        SetIntensity(ldParams[0].intensity, ldParams[0].spotSize, ldParams[0].probeMode);
        if (mLDBeamNormDelay)
          Sleep(mLDBeamNormDelay);
      }
    }
  }

  // If we are not at the mag of the current area, just go back so the image shift gets
  // set appropriately before the coming changes
  if (oldArea >= 0 && FastMagIndex() != mLdsaParams->magIndex && 
    mLdsaParams->magIndex > 0)
    SetMagIndex(mLdsaParams->magIndex);
  
  // If changing area at zero IS, get the current centered shift and reset it, and
  // reinitialize the cumulative change, since IS is put back after the cumulative change
  if (changingAtZeroIS || adjForFocus)
    GetLDCenteredShift(centeredISX, centeredISY);
  if (changingAtZeroIS) {
    SetLDCenteredShift(0., 0.);
    if (JEOLscope && !STEMmode)
      GetBeamShift(startBeamX, startBeamY);
    mLDChangeCumulBeamX = mLDChangeCumulBeamY = 0.;
  }

  // Set up for this area to be used for shifts when changing mag or alpha
  mLDChangeCurArea = mLowDoseSetArea;
  mChangingLDArea = 1;

  // Do image shift at higher mag for consistency in beam shifts, so do it now if
  // leaving a higher mag
  // If mags are equal and alpha differs, do IS now when going to a higher alpha so the
  // beam shift will be done at the same alpha
  // However, when using PLA, do it at the mag of a trial/focus area when either leaving
  // or going to a trial/focus area.  The problem is that the PLA needed for area 
  // separation may be out of range at a higher mag where a bigger projector shift is 
  // needed for the given IS offset
  if (GetUsePLforIS(ldArea->magIndex) && ((fromFocTrial && !toFocTrial) ||
    (!fromFocTrial && toFocTrial && mLdsaParams->magIndex) || 
    fromSearchOK || toSearch))
    ISdone = oldArea >= 0 && (fromFocTrial || fromSearch || 
    (!ldArea->magIndex && ldArea->camLenIndex));
  else
    ISdone = oldArea >= 0 && (((ldArea->magIndex || ldArea->camLenIndex) &&
    ((ldArea->magIndex < mLdsaParams->magIndex && !toSearch)) || 
      fromSearchOK) ||
    (ldArea->magIndex == mLdsaParams->magIndex && !mHasNoAlpha &&
    mLdsaParams->beamAlpha >= 0 && ldArea->beamAlpha > mLdsaParams->beamAlpha));
  if (ISdone)
    DoISforLowDoseArea(newArea, mLdsaParams->magIndex, delISX, delISY, 
      oldArea >= 0 && mLdsaParams->magIndex == ldArea->magIndex, adjForFocus, 
      centeredISX, centeredISY);

  // If leaving view or search area, set defocus back first
  if (!STEMmode && mLDViewDefocus && fromViewOK && 
    (!toView || (toViewOK && removeAddDefocus)))
      IncDefocus(-(double)mLDViewDefocus);
  if (!STEMmode && mSearchDefocus && fromSearchOK && 
    (!toSearch || (toSearchOK && removeAddDefocus)))
      IncDefocus(-(double)mSearchDefocus);

  // Pull off the beam shift if leaving an area and going between LM and nonLM
  if (splitBeamShift && oldArea >= 0 &&
    (mLdsaParams->beamDelX || mLdsaParams->beamDelY))
      IncOrAccumulateBeamShift(-mLdsaParams->beamDelX, -mLdsaParams->beamDelY, 
        "Go2LD pull off BS");

  // Set probe mode if any.  This will simply save and restore IS across the change,
  // and impose any change in focus that has occurred in this mode unconditionally
  // and impose accumulated beam shifts from this mode after translating to new mode
  // First back off a beam shift; it didn't work to include that in the converted shift
  // But then defer the probe change if going out of LM
  Sleep(2);
  if (ldArea->probeMode >= 0) {
    if (mProbeMode != ldArea->probeMode || (leavingLowMag && oldArea < 0)) {
      if (oldArea >= 0 && (mLdsaParams->beamDelX || mLdsaParams->beamDelY) && 
        mProbeMode == mLdsaParams->probeMode && !splitBeamShift)
        IncOrAccumulateBeamShift(-mLdsaParams->beamDelX, -mLdsaParams->beamDelY,
          "Go2LD pull off BS");
      probeDone = !leavingLowMag;
      if (probeDone)
        SetProbeMode(ldArea->probeMode, true);
    }
  } else
    ldArea->probeMode = mProbeMode;
  SleepMsg(2);

  // Mag and spot size will be set only if they change.  Do intensity unconditionally
  // If something is not initialized, set it up with current value
  // Don't set spot size on 1230 since it can't be read
  // Set spot before mag since JEOL mag change generates spot event
  if (!mJeol1230) {
    if (ldArea->spotSize) {
      if (!deferJEOLspot)
        SetSpotSize(ldArea->spotSize);
    } else
      ldArea->spotSize = GetSpotSize();
  }
  SleepMsg(2);

  // Set alpha before mag if it is changing to a lower mag since scope can impose a beam
  // shift in the mag change that may depend on alpha  The reason for doing THIS at a 
  // higher mag is that there is hopefully only one mode at a lower mag, so the change
  // of alpha will always be able to happen in the upper mag range
  curAlpha = GetAlpha();
  alphaDone = curAlpha >= 0 && ldArea->beamAlpha >= 0. && !STEMmode && 
    ((oldArea >= 0 && (ldArea->magIndex || ldArea->camLenIndex) &&
    ldArea->magIndex < mLdsaParams->magIndex) || enteringLowMag);
  if (!mHasNoAlpha)
    GetBeamTilt(mBaseForAlphaBTX, mBaseForAlphaBTY);
  if (alphaDone && !mHasNoAlpha)
    ChangeAlphaAndBeam(curAlpha, (int)ldArea->beamAlpha,
      (oldArea >= 0 && mLdsaParams->magIndex > 0) ? mLdsaParams->magIndex : -1,
      ldArea->magIndex);

  if (ldArea->magIndex)
    SetMagIndex(ldArea->magIndex);
  else if (!ldArea->camLenIndex)
    ldArea->magIndex = GetMagIndex();
 
  if (!ldArea->magIndex) {
    if (ldArea->camLenIndex)
      SetCamLenIndex(ldArea->camLenIndex);
    else 
      ldArea->camLenIndex = GetCamLenIndex();

    if (ldArea->diffFocus > -990.)
      SetDiffractionFocus(ldArea->diffFocus);
    else
      ldArea->diffFocus = GetDiffractionFocus();
  }
  magTime = GetTickCount();
  SleepMsg(2);

  // Now that mag is done, do the probe change if coming out of LM
  if (!probeDone)
    SetProbeMode(ldArea->probeMode, true);
  SleepMsg(2);

  // For FEI, set spot size (if it isn't right) again because mag range change can set 
  // spot size; afraid to just move spot size change down here because of note above
  if (!JEOLscope || deferJEOLspot)
    SetSpotSize(ldArea->spotSize);

  if (mCamera->HasDoseModulator() && ldArea->EDMPercent > 0) {
    setNum = toSearch ? SEARCH_CONSET : newArea;
    if (mCamera->mDoseModulator->SetDutyPercent(ldArea->EDMPercent, errStr) > 0)
      PrintfToLog("WARNING: Failed to set dose modulator to %.1f%% for %s area:\r\n %s",
        ldArea->EDMPercent, setNames[setNum], (LPCSTR)errStr);
  }

  // For alpha change, need to restore the beam shift to what it was for consistent
  // changes; and if there is a calibrated beam shift change, apply that
  if (ldArea->beamAlpha >= 0. && !STEMmode && !alphaDone && !mHasNoAlpha) {
    ChangeAlphaAndBeam(curAlpha, (int)ldArea->beamAlpha,
      (oldArea >= 0 && mLdsaParams->magIndex > 0) ? mLdsaParams->magIndex : -1, 
      ldArea->magIndex);
  } else if (!STEMmode && !alphaDone)
    ldArea->beamAlpha = (float)curAlpha;

  // If going to view or search area, set defocus offset incrementally
  if (!STEMmode && mLDViewDefocus && toViewOK && 
    (!fromView || (fromViewOK  && removeAddDefocus))) {
    IncDefocus((double)mLDViewDefocus);
    mCurDefocusOffset = mLDViewDefocus;
  }
  if (!STEMmode && mSearchDefocus && toSearchOK && 
    (!fromSearch || (fromSearchOK && removeAddDefocus))) {
    IncDefocus((double)mSearchDefocus);
    mCurDefocusOffset = mSearchDefocus;
  }

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
    if (filterChanged || (JEOLscope && (enteringLowMag || leavingLowMag))) {
      mCamera->SetIgnoreFilterDiffs(true);
      if (JEOLscope)
        mCamera->SetupFilter();
      mWinApp->mFilterControl.UpdateSettings();
    }
  }
  
  // Now set intensity after all the changes JEOL might have imposed from spot or filter
  intensity = ldArea->intensity;
  if (!STEMmode && intensity) {
    if (newArea == TRIAL_CONSET && mWinApp->mMultiTSTasks->AutocenMatchingIntensity(
      ACTRACK_TO_TRIAL)) {
      intensity = mWinApp->mAutocenDlg->GetParamIntensity();
      manage = true;
    }
    if (intensity)
      DelayedSetIntensity(intensity, magTime, ldArea->spotSize, ldArea->probeMode);
    if (needCondenserNorm) {
      if (mUseNormForLDNormalize == 1)
        NormalizeCondenser();
      else
        NormalizeAll(1);
      if (mLDBeamNormDelay)
        Sleep(mLDBeamNormDelay);
    }

    if (mWinApp->mAutocenDlg)
      mWinApp->mAutocenDlg->ManageLDtrackText(manage);
  } else if (!STEMmode)
    ldArea->intensity = GetIntensity();
  SleepMsg(2);

  // Shift image now after potential mag change
  if (!ISdone)
    DoISforLowDoseArea(newArea, ldArea->magIndex, delISX, delISY, true, adjForFocus,
      centeredISX, centeredISY);

  // Do incremental beam shift if any; but if area is undefined, assume coming from Record
  if (!STEMmode) {
    if (oldArea < 0)
      mLdsaParams = ldParams + RECORD_CONSET;
    beamDelX = ldArea->beamDelX - mLdsaParams->beamDelX;
    beamDelY = ldArea->beamDelY - mLdsaParams->beamDelY;
    if (splitBeamShift || mLdsaParams->probeMode != ldArea->probeMode) {
      beamDelX = ldArea->beamDelX;
      beamDelY = ldArea->beamDelY;
    }
    IncOrAccumulateBeamShift(beamDelX, beamDelY, "Go2LD put on new BS");

    // Do incremental beam tilt changes if enabled or reassert alpha beam tilt
    if (mLDBeamTiltShifts || mSetAlphaBeamTiltX > EXTRA_VALUE_TEST) {
      beamDelX = beamDelY = 0.;
      if (mLDBeamTiltShifts) {
        beamDelX = ldArea->beamTiltDX - mLdsaParams->beamTiltDX;
        beamDelY = ldArea->beamTiltDY - mLdsaParams->beamTiltDY;
      }
      if (mSetAlphaBeamTiltX > EXTRA_VALUE_TEST)
        SetBeamTilt(mSetAlphaBeamTiltX + beamDelX, mSetAlphaBeamTiltY + beamDelY);
      else if (beamDelX || beamDelY)
        IncBeamTilt(beamDelX, beamDelY);
    }

    // Do absolute dark field beam tilt if it differs from current value
    if (ldArea->darkFieldMode != mLdsaParams->darkFieldMode || 
      fabs(ldArea->dfTiltX - mLdsaParams->dfTiltX) > 1.e-6 ||
      fabs(ldArea->dfTiltY - mLdsaParams->dfTiltY) > 1.e-6)
      SetDarkFieldTilt(ldArea->darkFieldMode, ldArea->dfTiltX, ldArea->dfTiltY);
  }

  // Set the area, save the polarity of this setting, and reset polarity
  mLowDoseSetArea = newArea;
  mLastLDpolarity = mNextLDpolarity;
  mNextLDpolarity = 1;

  // Do full setting of beam shift for JEOL in one step
  if (JEOLscope && !STEMmode)
    SetBeamShift(startBeamX + mLDChangeCumulBeamX, startBeamY + mLDChangeCumulBeamY);
  mChangingLDArea = -1;

  // Finally, put the centered IS back at the new mag and area
  if (changingAtZeroIS && mLdsaParams) {
    mShiftManager->TransferGeneralIS(mLdsaParams->magIndex, centeredISX, centeredISY,
      ldArea->magIndex, newISX, newISY);
    SetLDCenteredShift(newISX, newISY);
    SEMTrace('l', "LD centered IS before: %.3f, %.3f; after: %.3f, %.3f", centeredISX, 
      centeredISY, newISX, newISY);
  }

  // Set Free lens after all changes
  SetFreeLensForArea(newArea);

  if (GetDebugOutput('L')) {
    GetImageShift(newISX, newISY);
    if (oldArea != newArea)
      SEMTrace('L', "LD: from %d to %d  was %.3f, %.3f  inc by %.3f, %.3f  now %.3f, %.3f"
        , oldArea, newArea, curISX, curISY, delISX, delISY, newISX, newISY);
  }
  if (GetDebugOutput('l')) {
    SEMTrace('l', "GotoLowDoseArea: focus at end %.2f update count %d\r\n", GetDefocus(),
      mAutosaveCount);
    if (GetDebugOutput('b') && !STEMmode) {
      GetBeamShift(curISX, curISY);
      GetBeamTilt(curISX, curISY);
    }
  }

  mLdsaParams = NULL;
  mChangingLDArea = 0;
  mWinApp->mLowDoseDlg.SelectGoToButton(newArea);
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  mWinApp->UpdateBufferWindows();
  mWinApp->mAlignFocusWindow.UpdateAutofocus(ldArea->magIndex);
  if (mWinApp->mParticleTasks->mZbyGsetupDlg && 
    !BOOL_EQUIV(oldArea == VIEW_CONSET, newArea == VIEW_CONSET))
    mWinApp->mParticleTasks->mZbyGsetupDlg->OnButUpdateState();

}

// Get change in image shift; set delay; tell low dose dialog to ignore this shift
void CEMscope::DoISforLowDoseArea(int inArea, int curMag, double &delISX, double &delISY,
  bool registerMagIS, bool adjForFocus, double cenISX, double cenISY)
{
  double oldISX, oldISY, useISX = 0., useISY = 0., posChgX = 0., posChgY = 0.;
  double vsXshift, vsYshift, curISX, curISY, adjISX, adjISY, transISX, transISY;
  float delay, scale, rotation, defocus;
  ScaleMat bsMat, focMat, sclMat;
  bool doAdj;
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();

  // IF using piezo for axis shift, do the movement
  delISX = delISY = 0.;
  mSubFromPosChgISX = mSubFromPosChgISY = 0.;
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
    posChgX = delISX;
    posChgY = delISY;

    // Get position again, taking off the view/search offset for JEOL
    if (JEOLscope && IS_AREA_VIEW_OR_SEARCH(inArea)) {
      mWinApp->mLowDoseDlg.GetEitherViewShift(vsXshift, vsYshift, inArea);
      mShiftManager->TransferGeneralIS(ldParams[inArea].magIndex,
        mNextLDpolarity * (useISX - vsXshift), mNextLDpolarity * (useISY - vsYshift),
        curMag, posChgX, posChgY);
    }
    /*PrintfToLog("useIS %.3f %.3f  posChg %.3f  %.3f del %.3f %.3f", useISX, useISY, 
    posChgX, posChgY, delISX, delISY);*/
  }
  if (mLowDoseSetArea >= 0 && (!mLowDoseSetArea || !mUsePiezoForLDaxis)) {

    // Also convert the existing image shift to the current mag (which is one or the 
    // other), but also only for View if using piezo
    mShiftManager->TransferGeneralIS(mLdsaParams->magIndex,
      mLastLDpolarity * mLdsaParams->ISX, 
      mLastLDpolarity * mLdsaParams->ISY, curMag, oldISX, oldISY);
    delISX -= oldISX;
    delISY -= oldISY;

    // And get the change for JEOL with view/search offset excluded
    if (JEOLscope) { 
      vsXshift = vsYshift = 0.;
      if (IS_AREA_VIEW_OR_SEARCH(mLowDoseSetArea))
        mWinApp->mLowDoseDlg.GetEitherViewShift(vsXshift, vsYshift, mLowDoseSetArea);
      mShiftManager->TransferGeneralIS(mLdsaParams->magIndex,
        mLastLDpolarity * (mLdsaParams->ISX - vsXshift),
        mLastLDpolarity * (mLdsaParams->ISY - vsYshift), 
        curMag, oldISX, oldISY);
      posChgX -= oldISX;
      posChgY -= oldISY;
      /*PrintfToLog("oldIS %.3f %.3f  posChg %.3f  %.3f del %.3f %.3f", oldISX, oldISY, 
      posChgX, posChgY, delISX, delISY);*/
    }
  }
  mLDChangeCurArea = inArea;

  // If going in or out of View or Search with defocus offset, try to correct an
  // existing image shift for the defocus : need both kinds of calibrations
  if (adjForFocus) { 

    // Use  centered IS to avoid being fooled by offsets, and only do it if it is nonzero
    doAdj = fabs(cenISX) > 0.01 || fabs(cenISY) > 0.01;
    if (doAdj && ((mLowDoseSetArea == VIEW_CONSET && mLDViewDefocus) ||
      (mLowDoseSetArea == SEARCH_AREA  && mSearchDefocus))) {

      // For leaving an area, convert defocused IS to camera coords and back scale/rotate
      // it, then convert back to unfocused IS
      defocus = mLowDoseSetArea == SEARCH_AREA ? mSearchDefocus : mLDViewDefocus;
      mShiftManager->GetDefocusMagAndRot(mLdsaParams->spotSize, mLdsaParams->probeMode,
        mLdsaParams->intensity, defocus, scale, rotation);
      sclMat = mShiftManager->MatScaleRotate(mShiftManager->FocusAdjustedISToCamera(
        mWinApp->GetCurrentCamera(), mLdsaParams->magIndex, mLdsaParams->spotSize,
        mLdsaParams->probeMode, mLdsaParams->intensity, defocus), 1.f / scale, -rotation);
      focMat =  MatMul(sclMat, mShiftManager->CameraToIS(mLdsaParams->magIndex));
      ApplyScaleMatrix(focMat, cenISX, cenISY, adjISX, adjISY);

      // If the mag has already changed, transfer the adjustment to the new mag
      if (curMag == ldParams[inArea].magIndex) {
        mShiftManager->TransferGeneralIS(mLdsaParams->magIndex, adjISX - cenISX, 
          adjISY - cenISY, ldParams[inArea].magIndex, transISX, transISY);
       } else {
        transISX = adjISX - cenISX;
        transISY = adjISY - cenISY;;
      }
      /*PrintfToLog("Leaving V/S, curmag %d cen %.2f %.2f  adj %.2f %.2f, adjust %.4f "
        "%.4f -> %.4f %.4f", curMag, cenISX, cenISY, adjISX, adjISY, adjISX - cenISX,
        adjISY - cenISY, transISX, transISY);*/

      // Add the change in image shift and set it as the current shift at the starting mag
      delISX += transISX;;
      delISY += transISY;
      cenISX = adjISX;
      cenISY = adjISY;
    }
    if (doAdj && mLowDoseSetArea >= 0 && ((inArea == VIEW_CONSET && mLDViewDefocus) ||
      (inArea == SEARCH_AREA  && mSearchDefocus))) {

      // Entering either area, transfer the image shift, transform to camera, rotate and
      // scale for defocus, tranform back to IS, again add the change
      defocus = inArea == SEARCH_AREA ? mSearchDefocus : mLDViewDefocus;
        mShiftManager->TransferGeneralIS(mLdsaParams->magIndex, cenISX, cenISY, 
          ldParams[inArea].magIndex, curISX, curISY);
      mShiftManager->GetDefocusMagAndRot(ldParams[inArea].spotSize,
        ldParams[inArea].probeMode, ldParams[inArea].intensity, defocus, scale, rotation);
      sclMat = mShiftManager->MatScaleRotate(mShiftManager->IStoCamera(
        ldParams[inArea].magIndex), scale, rotation);
      focMat = MatMul(sclMat, MatInv(mShiftManager->FocusAdjustedISToCamera(
        mWinApp->GetCurrentCamera(), ldParams[inArea].magIndex, ldParams[inArea].spotSize,
          ldParams[inArea].probeMode, ldParams[inArea].intensity, defocus)));
      ApplyScaleMatrix(focMat, curISX, curISY, adjISX, adjISY);

      // If the mag hasn't changed yet, transfer the adjustment to the old mag
      if (curMag != ldParams[inArea].magIndex) {
        mShiftManager->TransferGeneralIS(ldParams[inArea].magIndex, adjISX - curISX,
          adjISY - curISY, mLdsaParams->magIndex, transISX, transISY);
      } else {
        transISX = adjISX - curISX;
        transISY = adjISY - curISY;
      }
      /*PrintfToLog("Entering V/S, curmag %d cen %.2f %.2f  trans %.2f %.2f  adj %.2f "
        "%.2f adjust %.4f %.4f -> %.4f %.4f", curMag, cenISX, cenISY, curISX, curISY,
        adjISX, adjISY, adjISX - curISX, adjISY - curISY, transISX, transISY);*/
      delISX += transISX;
      delISY += transISY;

      // Store as current part of IS that doesn't represent a position change
      mSubFromPosChgISX = transISX;
      mSubFromPosChgISY = transISY;
    }
  }

  if (!delISX && !delISY)
    return;
  SEMTrace('l', "Changing IS for LD area change by %.3f, %.3f", delISX, delISY);
  if (JEOLscope && registerMagIS)
    mPlugFuncs->UseMagInNextSetISXY(curMag);
  IncImageShift(delISX, delISY);
  delay = ldParams[inArea].delayFactor * mShiftManager->GetLastISDelay();
  mShiftManager->SetISTimeOut(delay);
  mWinApp->mLowDoseDlg.SetIgnoreIS();

  // Compute the beam shift needed at the current mag to reflect the true position change
  if (JEOLscope) {
    bsMat = mShiftManager->GetBeamShiftCal(curMag);
    if (bsMat.xpx) {
      adjISX = bsMat.xpx * posChgX + bsMat.xpy * posChgY;
      adjISY = bsMat.ypx * posChgX + bsMat.ypy * posChgY;
      mLDChangeCumulBeamX += adjISX;
      mLDChangeCumulBeamY += adjISY;
      SEMTrace('b', "posChg %.3f %.3f -> change accumBS at mag %d by %.5g %.5g to %.5g "
        "%.5g ", posChgX, posChgY, curMag, adjISX, adjISY, mLDChangeCumulBeamX,
        mLDChangeCumulBeamY);
    }
  }
}

// Restore lenses set with FLC when leaving oldArea, except ones to be set in newArea,
// which can be -1 to restore all
void CEMscope::RestoreFromFreeLens(int oldArea, int newArea)
{
  FreeLensSequence seq, newSeq;
  int ind, jnd, lens, setIndex;
  bool haveNew = false, retain;
  if (oldArea < 0 || !JEOLscope || !mFLCSequences.GetSize())
    return;
  setIndex = mWinApp->GetLDSetIndexForCamera(mWinApp->GetCurrentCamera());

  // If there is a new area, get its sequence if any
  if (newArea >= 0) {
    for (ind = 0; ind < (int)mFLCSequences.GetSize(); ind++) {
      newSeq = mFLCSequences.GetAt(ind);
      if (newSeq.ldArea == newArea && newSeq.setIndex == setIndex) {
        haveNew = true;
        break;
      }
    }
  }

  // Get sequence for old area if any
  for (ind = 0; ind < (int)mFLCSequences.GetSize(); ind++) {
    seq = mFLCSequences.GetAt(ind);
    if (seq.ldArea == oldArea && seq.setIndex == setIndex) {

      // Loop on lenses, see if retaining it for new area
      for (lens = 0; lens < seq.numLens; lens++) {
        retain = false;
        if (haveNew) {
          for (jnd = 0; jnd < newSeq.numLens; jnd++) {
            if (newSeq.lens[jnd] == seq.lens[lens]) {
              retain = true;
              break;
            }
          }
        }

        // If not, turn off FLC
        if (!retain)
          SetFreeLensControl(seq.lens[lens], 0);
      }
    }
  }
}

// Set one or more lenses with free lens control when entering a low dose area
void CEMscope::SetFreeLensForArea(int newArea)
{
  int ind, lens, setIndex;
  FreeLensSequence seq;
  if (!JEOLscope || !mFLCSequences.GetSize())
    return;
  setIndex = mWinApp->GetLDSetIndexForCamera(mWinApp->GetCurrentCamera());

  for (ind = 0; ind < (int)mFLCSequences.GetSize(); ind++) {
    seq = mFLCSequences.GetAt(ind);
    if (seq.ldArea == newArea && seq.setIndex == setIndex) {
      for (lens = 0; lens < seq.numLens; lens++)
        SetLensWithFLC(seq.lens[lens], seq.value[lens], false);
      if (mLDFreeLensDelay > 0)
        Sleep(mLDFreeLensDelay);
      break;
    }
  }
  return;
}

// Change alpha from old to new if it is different and change beam shift and tilt if
// the calibrations exist
void CEMscope::ChangeAlphaAndBeam(int oldAlpha, int newAlpha, int oldMag, int newMag)
{
  double beamDelX = 0., beamDelY = 0., beamX, beamY;
  double tiltDelX = 0., tiltDelY = 0., tiltX, tiltY;
  ScaleMat bsmOld, bsmNew;
  double curISX, curISY;
  double posChangingISX, posChangingISY, pcNewISX, pcNewISY, pcOldISX, pcOldISY;
  int magInd = 0;
  bool changingLM;
  mSetAlphaBeamTiltX = EXTRA_NO_VALUE;

  // This routine needs to run to assert beam tilt changes at least when going in or out
  // of LM, so figure out the mag if needed and return if there is nothing to do
  if (oldMag < 0 || newMag < 0) {
    magInd = FastMagIndex();
    if (oldMag < 0)
      oldMag = magInd;
    if (newMag < 0)
      newMag = magInd;
  }
  changingLM = !BothLMorNotLM(oldMag, false, newMag, false);
  if (oldAlpha == newAlpha && !changingLM)
    return;

  if (oldAlpha < mNumAlphaBeamShifts && newAlpha < mNumAlphaBeamShifts) {
    beamDelX = mAlphaBeamShifts[newAlpha][0] - mAlphaBeamShifts[oldAlpha][0];
    beamDelY = mAlphaBeamShifts[newAlpha][1] - mAlphaBeamShifts[oldAlpha][1];
  }
  if (oldAlpha < mNumAlphaBeamTilts && newAlpha < mNumAlphaBeamTilts) {
    tiltDelX = mAlphaBeamTilts[newAlpha][0] - mAlphaBeamTilts[oldAlpha][0];
    tiltDelY = mAlphaBeamTilts[newAlpha][1] - mAlphaBeamTilts[oldAlpha][1];
  }
  GetBeamShift(beamX, beamY);

  // Use base beam tilt when changing low dose area, this could be after a mag change
  // that changed the tilt
  if (mChangingLDArea) {
    tiltX = mBaseForAlphaBTX;
    tiltY = mBaseForAlphaBTY;
  } else {
    GetBeamTilt(tiltX, tiltY);
  }

  if (oldAlpha != newAlpha) {

    // If beam calibrations differ (and they should), translate the beam shift needed for
    // position-changing part of IS, but do it with IS appropriate for each mag if there 
    // are two
    bsmOld = mShiftManager->GetBeamShiftCal(oldMag, oldAlpha);
    bsmNew = mShiftManager->GetBeamShiftCal(newMag, newAlpha);
    if (bsmOld.xpx != bsmNew.xpx && bsmOld.xpx && bsmNew.xpx) {
      if (!magInd)
        magInd = FastMagIndex();
      GetImageShift(curISX, curISY);
      PositionChangingPartOfIS(curISX, curISY, posChangingISX, posChangingISY);
      pcOldISX = pcNewISX = posChangingISX;
      pcOldISY = pcNewISY = posChangingISY;
      /*PrintfToLog("cur %d old %d new %d  posChg %.3f %.3f", magInd, oldMag, newMag, 
      pcOldISX, pcOldISY);*/
      if (oldMag != magInd)
        mShiftManager->TransferGeneralIS(newMag, pcNewISX, pcNewISY, oldMag, pcOldISX,
          pcOldISY);
      if (newMag != magInd)
        mShiftManager->TransferGeneralIS(oldMag, pcOldISX, pcOldISY, newMag, pcNewISX,
          pcNewISY);
      /*PrintfToLog("old posChg old %.3f %.3f  new %.3f %.3f", pcOldISX, pcOldISY,
      pcNewISX, pcNewISY);*/
      beamDelX += bsmNew.xpx * pcNewISX + bsmNew.xpy * pcNewISY -
        (bsmOld.xpx * pcOldISX + bsmOld.xpy * pcOldISY);
      beamDelY += bsmNew.ypx * pcNewISX + bsmNew.ypy * pcNewISY -
        (bsmOld.ypx * pcOldISX + bsmOld.ypy * pcOldISY);
    }

    if (GetDebugOutput('l') || GetDebugOutput('b'))
      PrintfToLog("Changing alpha from %d to %d and beam shift %.3f %.3f  to %.3f %.3f",
        oldAlpha + 1, newAlpha + 1, beamX, beamY, beamX + beamDelX, beamY + beamDelY);
    SetAlpha(newAlpha);
  }

  // Now take care of beam shift/tilt
  if (mChangingLDArea) {
    IncOrAccumulateBeamShift(beamDelX, beamDelY, "ChangeAlpha");
  } else {
    SetBeamShift(beamX + beamDelX, beamY + beamDelY);
  }
  SetBeamTilt(tiltX + tiltDelX, tiltY + tiltDelY);
  mSetAlphaBeamTiltX = tiltX + tiltDelX;
  mSetAlphaBeamTiltY = tiltY + tiltDelY;
  if (oldAlpha != newAlpha)
    Sleep(mAlphaChangeDelay);
}

// Set the area that should be shown when screen down; go there if low dose on
void CEMscope::SetLowDoseDownArea(int inArea)
{
  BOOL needBlank, gotoArea;
  needBlank = NeedBeamBlanking(FastScreenPos(), FastSTEMmode(), gotoArea);
  mLowDoseDownArea = inArea;
  if (gotoArea && mLowDoseDownArea >= 0) {
    GotoLowDoseArea(inArea);
    if (!mWinApp->GetSTEMMode() && mHasOmegaFilter)
      mCamera->SetupFilter();
  }
}

// Compute the component of image shift that represent displacement from axial position
// rather than being used to keep the axis aligned
void CEMscope::PositionChangingPartOfIS(double curISX, double curISY,
  double &posChangingISX, double &posChangingISY)
{
  double offsetsX, offsetsY, vsXshift = 0., vsYshift = 0., axisISX, axisISY;
  double vsXtmp, vsYtmp;
  int area;
  BOOL shiftSave = mShiftToTiltAxis;
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();

  GetTiltAxisIS(axisISX, axisISY);
  mShiftToTiltAxis = false;
  GetTiltAxisIS(offsetsX, offsetsY);
  mShiftToTiltAxis = shiftSave;
  if (mLowDoseMode) {
    area = mChangingLDArea > 0 ? mLDChangeCurArea : mLowDoseSetArea;
    if (IS_AREA_VIEW_OR_SEARCH(area)) {
      mWinApp->mLowDoseDlg.GetEitherViewShift(vsXtmp, vsYtmp, area);
      mShiftManager->TransferGeneralIS(ldParams[area].magIndex, vsXtmp, vsYtmp,
        mLastMagIndex, vsXshift, vsYshift);

      // Take off the defocus adjustment for image shift if one was made
      vsXshift += mSubFromPosChgISX;
      vsYshift += mSubFromPosChgISY;
    }
  }
  posChangingISX = curISX + axisISX - offsetsX - vsXshift;
  posChangingISY = curISY + axisISY - offsetsY - vsYshift;
}

// Accumulate beam shift changes on JEOL when changing low dose are; otherwise apply it
void CEMscope::IncOrAccumulateBeamShift(double beamDelX, double beamDelY,
  const char *descrip)
{
  if (JEOLscope && mChangingLDArea > 0) {
    mLDChangeCumulBeamX += beamDelX;
    mLDChangeCumulBeamY += beamDelY;
    SEMTrace('b', "Accum for %s: %.5g %.5g  sum %.5g %.5g", descrip, beamDelX, beamDelY,
      mLDChangeCumulBeamX, mLDChangeCumulBeamY);
  } else if (beamDelX || beamDelY) {
    IncBeamShift(beamDelX, beamDelY);
  }
}

BOOL CEMscope::GetIntensityZoom()
{
  BOOL result;

  // IntensityZoom (MagLink?) is not implemented for JEOL; setting will return false
  if (!sInitialized || (JEOLscope && !mJeolHasBrightnessZoom) || HitachiScope)
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
    if ((JEOLscope && !mJeolHasBrightnessZoom) || HitachiScope) {
      TRACE("SetIntensityZoom is not implemented for Hitachi microscopes.\n");
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
  bool needMutex = !(JEOLscope && sGettingValuesFast);
  if (needMutex)
    ScopeMutexAcquire("GetIntensity", true);

  try {
    PLUGSCOPE_GET(Intensity, result, 1.);
    result += mAddToRawIntensity;
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

// Set the intensity (C2) value.  Spot and probe are optional and needed for scaling
// illuminated if the limits are calibrated: pass spot if possible to avoid a Get
BOOL CEMscope::SetIntensity(double inVal, int spot, int probe)
{
  BOOL result = true;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();

  if (!sInitialized)
    return false;

  // Falcon 3 can have a problem with initial blanking after an intensity change, so this
  // allows a timeout before the next image
  if (camParam->postIntensityTimeout > 0.) {
    if (fabs(GetIntensity() - inVal) < 1.e-6)
      return true;
    mShiftManager->SetGeneralTimeOut(GetTickCount(), 
      (int)(1000. * camParam->postIntensityTimeout));
  }

  if (mUseIllumAreaForC2)
    return SetIlluminatedArea(IntensityToIllumArea(inVal, spot, probe));

  ScopeMutexAcquire("SetIntensity", true);

  try {
    PLUGSCOPE_SET(Intensity, inVal - mAddToRawIntensity);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting beam intensity "));
    result = false;
  }
  ScopeMutexRelease("SetIntensity");
  return result;
}

BOOL CEMscope::DelayedSetIntensity(double inVal, DWORD startTime, int spot, int probe)
{
  double elapsed = SEMTickInterval((double)startTime);
  if (elapsed < mMagChgIntensityDelay - 1.)
    Sleep((DWORD)(mMagChgIntensityDelay - elapsed));
  return SetIntensity(inVal, spot, probe);
}

double CEMscope::GetC2Percent(int spot, double intensity, int probe)
{
  if (probe < 0 || probe > 1)
    probe = mProbeMode;
  if (mUseIllumAreaForC2)
    return 100. * IntensityToIllumArea(intensity, spot, probe);
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
  static int numOut = 0;
  static CString outVals = "BUG: GetCrossover values out of range in startup:\r\n";
  static int numWarn = 0;
  CString str;
  if (probe < 0 || probe > 1)
    probe = mProbeMode;

  // Ridiculous stuff to find bug
  if (!(mWinApp->GetStartingProgram() || mWinApp->GetAppExiting())) {
    if (numOut && !outVals.IsEmpty()) {
      mWinApp->AppendToLog(outVals);
      numWarn++;
    }
    outVals = "";
  }
  if (probe < 0 || probe > 1 || spotSize < 0 || spotSize > MAX_SPOT_SIZE) {
    if (mWinApp->GetStartingProgram() || mWinApp->GetAppExiting()) {
      str.Format(" %d %d ", probe, spotSize);
      outVals += str;
      numOut++;
    } else {
      if (numWarn < 5) {
        PrintfToLog("BUG: GetCrossover values out of range: %d %d", probe, spotSize);
        numWarn++;
      }
      numOut++;
    }
    B3DCLAMP(probe, 0, 1);
    B3DCLAMP(spotSize, 0, MAX_SPOT_SIZE);
  }
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
double CEMscope::IllumAreaToIntensity(double illum, int spot, int probe)
{
  float lowLimit, highLimit;
  if (GetAnyIllumAreaLimits(spot, probe, lowLimit, highLimit))
    return (illum - lowLimit) * (mIllumAreaHighMapTo - mIllumAreaLowMapTo) / (
      highLimit - lowLimit) + mIllumAreaLowMapTo;
  return illum;
}

double CEMscope::IntensityToIllumArea(double intensity, int spot, int probe)
{
  float lowLimit, highLimit;
  if (GetAnyIllumAreaLimits(spot, probe, lowLimit, highLimit))
    return (intensity - mIllumAreaLowMapTo) * (highLimit - lowLimit) /
      (mIllumAreaHighMapTo - mIllumAreaLowMapTo) + lowLimit;
  return intensity;
}

// Return illuminated area limits either from the calibration at the given spot and
// probe or from the property, return false if none
bool CEMscope::GetAnyIllumAreaLimits(int spot, int probe, float &lowLimit, 
  float &highLimit)
{
  if (mCalHighIllumAreaLim[1][1] > mCalLowIllumAreaLim[1][1]) {
    if (spot < 0)
      spot = GetSpotSize();
    if (probe < 0)
      probe = mProbeMode;
    if (mCalHighIllumAreaLim[spot][probe] > mCalLowIllumAreaLim[spot][probe]) {
      lowLimit = mCalLowIllumAreaLim[spot][probe];
      highLimit = mCalHighIllumAreaLim[spot][probe];
      return true;
    }
  }
  if (mIllumAreaHighLimit > mIllumAreaLowLimit) {
    lowLimit = mIllumAreaLowLimit;
    highLimit = mIllumAreaHighLimit;
    return true;
  }
  return false;
}

// Convert an intensity if there is a known change in aperture
double CEMscope::IntensityAfterApertureChange(double intensity, int oldAper, int newAper,
  int spot, int probe)
{
  if (!mUseIllumAreaForC2 || !oldAper || !newAper || oldAper == newAper)
    return intensity;
  return IllumAreaToIntensity((newAper * IntensityToIllumArea(intensity, spot, probe)) / 
    oldAper, spot, probe);
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
    sGettingValuesFast));
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

// Set the spot size with optional normalization (> 0) or preventation of 
// autonormalization (< 0)
BOOL CEMscope::SetSpotSize(int inIndex, int normalize)
{
  BOOL result = true;
  int curSpot = -1;
  bool fixBeam, unblankAfter;
  double beamXorig, beamYorig, delX, delY;
  const char *routine = "SetSpotSize";

  if (!sInitialized || inIndex < mMinSpotSize)
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
  if (fixBeam && !(JEOLscope && mChangingLDArea))
    GetBeamShift(beamXorig, beamYorig);

  unblankAfter = BlankTransientIfNeeded(routine);

  // Change the spot size.  
  if (mSkipNormalizations & 2)
    normalize = 0;
  mSynchroTD.normalize = normalize;
  result = RunSynchronousThread(SYNCHRO_DO_SPOT, inIndex, curSpot, NULL);

  UnblankAfterTransient(unblankAfter, routine);
  if (normalize > 0)
    mLastNormalization = mSynchroTD.lastNormalizationTime;

  // Finally, call the routine to set the fixed beam shift
  if (result && fixBeam) {
    delX = mSpotBeamShifts[mSecondaryMode][inIndex][0] -
      mSpotBeamShifts[mSecondaryMode][curSpot][0];
    delY = mSpotBeamShifts[mSecondaryMode][inIndex][1] -
      mSpotBeamShifts[mSecondaryMode][curSpot][1];
    if (JEOLscope && mChangingLDArea) {
      IncOrAccumulateBeamShift(delX, delY, "Spot beam shift");
    } else {
      beamXorig += delX;
      beamYorig += delY;
      SEMTrace('b', "Fixing BS for spot by %.5g %.5g to %.5g %.5g", delX, delY, beamXorig,
        beamYorig);
      SetBeamShift(beamXorig, beamYorig);
    }
  }
  return result;
}

// Does the real work, is called from the synchronous thread
BOOL CEMscope::SetSpotKernel(SynchroThreadData *sytd)
{
  double startTime, curBSX, curBSY, startBSX, startBSY;

  try {

    // If normalization requested(or forbidden), disable autonorm,
    // then later normalize, and set normalization time to give some delay 
    if (sytd->normalize)
      AUTONORMALIZE_SET(*vFalse);
    
    // Hitachi hysteresis is ferocious.  Things are somewhat stable by always going up
    // from spot 1 instead of down.
    if (HitachiScope) {
      if (sytd->newIndex < sytd->curIndex) {
        for (int i = 1; i < sytd->newIndex; i++) {
          mPlugFuncs->SetSpotSize(i);
          Sleep(sytd->HitachiSpotStepDelay);
        }
      }

      // Get beam shift before last spot change so we can look for change
      mPlugFuncs->GetBeamShift(&startBSX, &startBSY);
    }

    // Make final change to spot size
    startTime = GetTickCount();
    PLUGSCOPE_SET(SpotSize, sytd->newIndex);
    WaitForLensRelaxation(RELAX_FOR_SPOT, startTime);

    // Wait for a beam shift to show up on Hitachi
    if (HitachiScope) {
      startTime = GetTickCount();
      while (1) {
        if (SEMTickInterval(startTime) > sytd->HitachiSpotBeamWait) {
          SEMTrace('b', "Timeout waiting for beam shift change after spot change");
          break;
        }
        mPlugFuncs->GetBeamShift(&curBSX, &curBSY);
        if (curBSX != startBSX || curBSY != startBSY) {
          SEMTrace('b', "BS change seen in %.0f to %.5g %.5g", SEMTickInterval(startTime),
            curBSX, curBSX);
          break;
        }
        Sleep(30);
      }
    }

    // Then normalize if selected
    if (sytd->normalize > 0) {
      if (mPlugFuncs->NormalizeLens && (!JEOLscope || SEMLookupJeolRelaxData(nmSpotsize)))
        mPlugFuncs->NormalizeLens(nmSpotsize);
      sytd->lastNormalizationTime = GetTickCount();
    }

    // Re-enable autonormalization
    if (sytd->normalize)
      AUTONORMALIZE_SET(*vTrue);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting spot size "));
    return false;
  }

  return true;
}

// Get alpha value on JEOL, or -999
int CEMscope::GetAlpha(void)
{
  int result = -999;
  if (!sInitialized || mHasNoAlpha)
    return -999;

  // Plugin will use state value if updating by event or getting fast
  BOOL needMutex = !(mJeolSD.eventDataIsGood || sGettingValuesFast);
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
  if (!sInitialized || mHasNoAlpha) {
    inIndex = -999;
    return false;
  }

  return RunSynchronousThread(SYNCHRO_DO_ALPHA, inIndex, 0, NULL);
}

BOOL CEMscope::SetAlphaKernel(SynchroThreadData *sytd)
{
  double startTime = GetTickCount();

  // It will check the current alpha
  try {
    mPlugFuncs->SetAlpha(sytd->newIndex);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting alpha value "));
    return false;
  }
  WaitForLensRelaxation(RELAX_FOR_ALPHA, startTime);
  return true;
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

// Set any lens by name on Hitachi
BOOL CEMscope::SetLensByName(CString & name, double value)
{
  BOOL result = true;
  if (!sInitialized || !mPlugFuncs->SetLensByName)
    return false;
  ScopeMutexAcquire("setLensByName", true);
  try {
    value = mPlugFuncs->SetLensByName((LPCTSTR)name, value);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting lens by name "));
    result = false;
  }
  ScopeMutexRelease("SetLensByName");
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

// Set any recognized coil by name in Hitachi
BOOL CEMscope::SetDeflectorByName(CString & name, double valueX, double valueY, int incr)
{
  BOOL result = true;
  if (!sInitialized || !HitachiScope || !mPlugFuncs->SetDeflectorByName)
    return false;
  ScopeMutexAcquire("SetDeflectorByName", true);
  try {
    mPlugFuncs->SetDeflectorByName((LPCTSTR)name, valueX, valueY, incr);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting deflector by name "));
    result = false;
  }
  ScopeMutexRelease("SetDeflectorByName");
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

BOOL CEMscope::GetLensFLCStatus(int lens, int &state, double &lensVal)
{
  BOOL result = true;
  if (!sInitialized || !mPlugFuncs->GetLensFLCStatus)
    return false;
  ScopeMutexAcquire("GetLensFLCStatus", true);
  try {
    mPlugFuncs->GetLensFLCStatus(lens, &state, &lensVal);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting status of lens with free lens control "));
    result = false;
  }
  ScopeMutexRelease("GetLensFLCStatus");
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

int CEMscope::FastColumnValvesOpen()
{
  FAST_GET(int, GetColumnValvesOpen);
}

// Manipulate column valves
BOOL CEMscope::SetColumnValvesOpen(BOOL state, bool crashing)
{
  BOOL result = true;
  if (!sInitialized)
    return false;
  ScopeMutexAcquire("SetBeamValve", true);
  try {
    mPlugFuncs->SetGunValve(VAR_BOOL(state));
  }
  catch (_com_error E) {
    if (!crashing)
      SEMReportCOMError(E, _T("setting column valves "));
    result = false;
  }
  ScopeMutexRelease("SetBeamValve");
  if (mOpenValvesDelay && state)
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mOpenValvesDelay);
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

// Find out if flashing of a given type is advised
BOOL CEMscope::GetIsFlashingAdvised(int high, int &answer)
{
  BOOL result = true;
  if (!sInitialized || !FEIscope || !mPlugFuncs->GetFlashingAdvised || 
    mAdvancedScriptVersion < ASI_FILTER_FEG_LOAD_TEMP)
    return false;
  ScopeMutexAcquire("GetIsFlashingAdvised", true);
  try {
    answer = mPlugFuncs->GetFlashingAdvised(high);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("determining if FEG flashing is advised "));
    result = false;
  }
  ScopeMutexRelease("GetIsFlashingAdvised");
  return result;
}

// Get the beam current from the FEG
BOOL CEMscope::GetFEGBeamCurrent(double &current, bool skipErr)
{
  BOOL result = true;
  if (!sInitialized || !FEIscope || !mPlugFuncs->GetFEGBeamCurrent ||
    mAdvancedScriptVersion < ASI_FILTER_FEG_LOAD_TEMP)
    return false;

  ScopeMutexAcquire("GetFEGBeamCurrent", true);
  try {
    current = mPlugFuncs->GetFEGBeamCurrent();
    mFEGBeamCurrent = current;
    mLastBeamCurrentTime = GetTickCount();
  }
  catch (_com_error E) {
    if (!skipErr)
      SEMReportCOMError(E, _T("getting FEG beam current "));
    result = false;
  }
  ScopeMutexRelease("GetFEGBeamCurrent");
  return result;
}

// Get beam current or read a fresh value every 5 minutes
BOOL CEMscope::FastFEGBeamCurrent(double &current)
{
  float minutes = 5.;
  if (SEMTickInterval(mLastBeamCurrentTime) < minutes * 60000) {
    current = mFEGBeamCurrent;
    return true;
  }
  return GetFEGBeamCurrent(current, true);
}

// Get the accelerating voltage in KV
double CEMscope::GetHTValue()
{
  double result;
  if (mNoScope)
    return mNoScope;
  if (B3DABS(mSimulationMode) > 1)
    return B3DABS(mSimulationMode);
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

/*
 * Functions for dealing with FEI or JEOL autoloader
 */
// Get the status of a slot; slots are apparently numbered from 1
BOOL CEMscope::CassetteSlotStatus(int slot, int &status, CString &names, int *numSlotsPtr)
{
  BOOL success = false;
  bool gettingName = false;
  int numSlots = -999;
  int csstat, dum1, dum2, dum3, dum4, dum5;
  status = -2;
  names = "";
  if (!sInitialized || !FEIscope)
    return false;
  try {
    numSlots = mPlugFuncs->NumberOfLoaderSlots();
    SEMTrace('W', "Number of slots is %d", numSlots);
    if (numSlotsPtr)
      *numSlotsPtr = numSlots;
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
    if (mFEIcanGetLoaderNames && !numSlotsPtr && (status >= 0 || !slot)) {
      success = true;
      if (!slot)
        status = 0;
      names = mPlugFuncs->GetCartridgeInfo(slot, &dum1, &dum2, &dum3, &dum4, &dum5);
      if (!names.Compare("!ERROR!")) {
        success = false;
        names = "An error occurred getting names from the autoloader panel: \r\n";
        names += mPlugFuncs->GetLastErrorString();
        names += "\r\n";
      }
    }
  }
  catch (_com_error E) {
    if (success) {
      mFEIcanGetLoaderNames = 0;
    } else if (numSlots == -999)
      SEMReportCOMError(E, _T("getting number of autoloader cassette slots "));
    else
      SEMReportCOMError(E, _T("getting status of autoloader cassette slot "));
  }
  SEMTrace('W', "CassetteSlotStatus returning %s, status %d", 
    success ? "success" : "failure", status);
  return success;
}

// Load a cartridge to the stage: slot # for FEI (from 1) or index + 1 in JEOL catalogue
int CEMscope::LoadCartridge(int slot, CString &errStr)
{
  int ind, ID, err, oper = LONG_OP_LOAD_CART;
  JeolCartridgeData jcd;
  float sinceLast = 0.;
  if (!sInitialized || (!FEIscope && mJeolHasNitrogenClass < 2)) {
    errStr = "Autoloader operations are not supported by this microscope";
    return 3;
  }
  if (slot <= 0)
    return 4;
  if (FEIscope) {
    sCartridgeToLoad = slot;
    mUnloadedCartridge = FindCartridgeAtStage(ID);
    mLoadedCartridge = -1;
    for (ind = 0; ind < (int)mJeolLoaderInfo.GetSize();ind++) {
      jcd = mJeolLoaderInfo.GetAt(ind);
      if (jcd.id == slot) {
        mLoadedCartridge = ind;   // A jcd index
        break;
      }
    }
  } else {
    if (!mJeolLoaderInfo.GetSize())
      return 6;
    if (slot > mJeolLoaderInfo.GetSize()) {
      errStr = "Slot number or index is out of range";
      return 4;
    }
    jcd = mJeolLoaderInfo.GetAt(slot - 1);
    if (jcd.station == JAL_STATION_STAGE) {
      errStr = "The cartridge is already in the stage according to the current inventory";
      return 5;
    }
    sCartridgeToLoad = jcd.id;
    mLoadedCartridge = slot - 1;  // A jcd index
    mUnloadedCartridge = FindCartridgeAtStage(ID);
  }
  err = StartLongOperation(&oper, &sinceLast, 1);
  if (err)
    errStr = (err == 1) ? "The thread is already busy for a long operation" :
    "There was an error trying to load a cartridge";
  return err;
}

// Unload a cartridge from the stage
int CEMscope::UnloadCartridge(CString &errStr)
{
  int err, oper = LONG_OP_UNLOAD_CART;
  float sinceLast = 0.;
  JeolCartridgeData jcd;
  if (!sInitialized || (!FEIscope && mJeolHasNitrogenClass < 2)) {
    errStr = "Autoloader operations are not supported by this microscope";
    return 3;
  }
  if (JEOLscope && !mJeolLoaderInfo.GetSize()) {
    errStr = "You need to do a cassette inventory; There is no cartridge "
      "information";
    return 6;
  }
  mLoadedCartridge = -1;
  mUnloadedCartridge = FindCartridgeAtStage(sCartridgeToLoad);
  if (JEOLscope && mUnloadedCartridge < 0) {
    errStr = "There is no cartridge in the stage according to the current inventory";
    return 5;
  }
  err = StartLongOperation(&oper, &sinceLast, 1);
  if (err)
    errStr = (err == 1) ? "The thread is already busy for a long operation" :
    "There was an error trying to unload a cartridge";
  return err;
}

// Lookup which cartridge is in the stage in the JEOL table, return the ID (slot # for 
// FEI), return value index in jcd table
int CEMscope::FindCartridgeAtStage(int &id)
{
  int ind;
  JeolCartridgeData jcd;
  id = -1;
  for (ind = 0; ind < mJeolLoaderInfo.GetSize(); ind++) {
    jcd = mJeolLoaderInfo.GetAt(ind);
    if (jcd.station == JAL_STATION_STAGE) {
      id = jcd.id;
      return ind;
    }
  }
  return -1;
}

// Look up a slot number by ID
int CEMscope::FindCartridgeWithID(int ID, CString &errStr)
{
  int ind;
  JeolCartridgeData jcd;
  if (!JEOLscope) {
    errStr = "ID values are available only for JEOL autoloaders";
    return -1;
  }
  if (mJeolHasNitrogenClass < 2) {
    errStr = "Autoloader support is not available unless property JeolHasNitrogenClass "
      "is set to >= 2";
    return -1;
  }
  if (!mJeolLoaderInfo.GetSize()) {
    "An inventory must be run first to get autoloader information";
    return -1;
  }
  for (ind = 0; ind < mJeolLoaderInfo.GetSize(); ind++) {
    jcd = mJeolLoaderInfo.GetAt(ind);
    if (jcd.id == ID)
      return ind + 1;
  }
  errStr.Format("ID %d was not found in the inventory", ID);
  return -1;
}

// Functions for dealing with temperature control

// Dewars busy
bool CEMscope::AreDewarsFilling(int &busy)
{
  int time, ibusy, ibusy2, active;
  double level;
  BOOL bBusy;
  bool bRet = true;
  if (HitachiScope || (JEOLscope && !mPlugFuncs->GetNitrogenStatus && !mHasSimpleOrigin))
    return false;
  ScopeMutexAcquire("AreDewarsFilling", true);
  try {
    if (mHasSimpleOrigin) {
      bRet = GetSimpleOriginStatus(ibusy2, time, ibusy, active);
      busy = ibusy > 0;
    } else if (FEIscope) {
      bRet = GetTemperatureInfo(0, bBusy, time, 0, level);
      busy = bBusy ? 1 : 0;
    } else {
      ibusy = mPlugFuncs->GetNitrogenStatus(1);
      ibusy2 = mPlugFuncs->GetNitrogenStatus(2);
      if (ibusy < 0 || ibusy2 < 0)
        bRet = false;
      else
        busy = (ibusy ? 1 : 0) + (ibusy2 ? 2 : 0);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Nitrogen status "));
    bRet = false;
  }
  ScopeMutexRelease("AreDewarsFilling");
  return bRet;
}

// Remaining time - seconds
bool CEMscope::GetDewarsRemainingTime(int which, int &time)
{
  BOOL busy;
  bool bRet;
  int ibusy, remaining, active;
  double level;
  if (JEOLscope && mJeolHasNitrogenClass > 1 && mPlugFuncs->GetNitrogenInfo) {
    return GetNitrogenInfo(which, time, level);
  }
  if (mHasSimpleOrigin) {
    bRet = GetSimpleOriginStatus(remaining, time, ibusy, active);
    if (!remaining)
      time = -1;
    if (!active)
      time = -2;
    return bRet;
  }
  return GetTemperatureInfo(1, busy, time, 0, level);
}

// refrigerant level - units?
bool CEMscope::GetRefrigerantLevel(int which, double &level)
{
  BOOL busy;
  int time;
  if (JEOLscope && mJeolHasNitrogenClass > 1 && mPlugFuncs->GetNitrogenInfo) {
    return GetNitrogenInfo(which, time, level);
  } 
  return GetTemperatureInfo(2, busy, time, which, level);
}

bool CEMscope::GetNitrogenInfo(int which, int & time, double & level)
{
  try {
    mPlugFuncs->GetNitrogenInfo(which, &time, &level);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting Nitrogen information "));
    return false;
  }
  return true;
}

// Read the registry to access information on SimpleOrigin system
bool CEMscope::GetSimpleOriginStatus(int & numRefills, int & secToNextRefill, 
  int &filling, int &active, float *sensor)
{
  CRegKey key;
  CString mess;
  float temp;
  if (!mHasSimpleOrigin) {
    SEMMessageBox("The property for a SimpleOrigin system is not set");
    return false;
  }
  if (!SimpleOriginStatus(key, numRefills, secToNextRefill, filling, active, temp, 
    mess)) {
    SEMMessageBox(mess);
    return false;
  }
  if (sensor)
    *sensor = temp;

  return true;
}

// Static function to be called from long op as well as GetSimpleOriginStatus
static bool SimpleOriginStatus(CRegKey &key, int &numRefills, int &secToNextRefill, 
  int &filling, int &active, float &sensor, CString &mess)
{
  LONG nResult;
  DWORD value;
  int ind = -1;
  const int bufSize = 32;
  char buffer[bufSize];
  ULONG pnChars = bufSize;
  const char *keys[] = {"NUMBER_OF_REMAINING_REFILLS", "TIME_TO_NEXT_REFILL_SEC",
    "IS_SYSTEM_REFILLING", "SYSTEM_STATUS", "SENSOR_TEMPERATURE"};
  nResult = key.Open(HKEY_CURRENT_USER, _T("SimpleOrigin"));
  if (nResult == ERROR_SUCCESS) {
    nResult = key.QueryDWORDValue(keys[++ind], value);
    numRefills = value;
  }
  if (nResult == ERROR_SUCCESS) {
    nResult = key.QueryDWORDValue(keys[++ind], value);
    secToNextRefill = value;
  }
  if (nResult == ERROR_SUCCESS) {
    nResult = key.QueryDWORDValue(keys[++ind], value);
    filling = value;
  }
  if (nResult == ERROR_SUCCESS) {
    nResult = key.QueryDWORDValue(keys[++ind], value);
    active = value;
  }
  if (nResult == ERROR_SUCCESS) {
    nResult = key.QueryStringValue(keys[++ind], buffer, &pnChars);
    sensor = 0.;
    if (pnChars && nResult == ERROR_SUCCESS)
      sensor = (float)atof(buffer);
  }
  if (nResult != ERROR_SUCCESS) {
    mess.Format("Error %d %s%s for SimpleOrigin system", nResult, 
      ind < 0 ? "opening top registry key" : "getting ", ind < 0 ? "" : keys[ind]);
    return false;
  }

  return true;
}

// Function called from long op thread to start the refill and wait until done
int CEMscope::RefillSimpleOrigin(CString &errString)
{
  CRegKey key;
  DWORD value;
  float sensor;
  LONG nResult;
  int remaining, secToNext, active, filling;
  double startTicks;

  if (!SimpleOriginStatus(key, remaining, secToNext, filling, active, sensor, errString))
    return 1;

  if (!active) {
    errString = "The SimpleOrigin system is not available/active";
    return 1;
  }
  if (!remaining) {
    errString = "There are no refills remaining in the SimpleOrigin system";
    return 1;
  }

  // If not already filling, start it
  if (secToNext) {
    nResult = key.SetDWORDValue("REMOTE_FILL_NOW_START", 1);

    if (nResult != ERROR_SUCCESS) {
      errString.Format("Error %d setting REMOTE_FILL_NOW_START for SimpleOrigin", 
        nResult);
      return 1;
    }
  }

  // Wait to see it start
  startTicks = GetTickCount();
  for (;;) {
    if (key.QueryDWORDValue("IS_SYSTEM_REFILLING", value) == ERROR_SUCCESS &&
      value)
      break;
    if (SEMTickInterval(startTicks) < 1000. * sSimpOrigStartTimeout)
      Sleep(50);
    else {
      errString = "Timeout waiting for SimpleOrigin filling to start";
      return 3;
    }
  }

  // Wait to see it stop
  startTicks = GetTickCount();
  for (;;) {
    if (key.QueryDWORDValue("IS_SYSTEM_REFILLING", value) == ERROR_SUCCESS &&
      !value)
      break;
    if (SEMTickInterval(startTicks) < 1000. * sSimpOrigEndTimeout)
      Sleep(100);
    else {
      errString = "Timeout waiting for SimpleOrigin filling to end";
      return 3;
    }
  }
  return 0;
}

// Make the system active or inactive
bool CEMscope::SetSimpleOriginActive(int active, CString & errString)
{
  CRegKey key;
  LONG nResult;
  float sensor;
  int remaining, secToNext, isActive, filling;
  if (!SimpleOriginStatus(key, remaining, secToNext, filling, isActive, sensor, 
    errString))
    return false;
  if (BOOL_EQUIV(isActive != 0, active != 0))
    return true;

  // Still don't know if this a correct way to do it but it should cover the bases
  key.SetDWORDValue(active ? "SOFTWARE_STOP" : "SOFTWARE_START", 0);
  nResult = key.SetDWORDValue(active ? "SOFTWARE_START" : "SOFTWARE_STOP", 1);
  if (nResult != ERROR_SUCCESS) {
    errString.Format("Error %d setting SOFTWARE_START/STOP for SimpleOrigin", nResult);
    return false;
  }
  return true;
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

// Get arbitrary gauge information
bool CEMscope::GetGaugePressure(const char *name, int &outStat, double &outPress)
{
  bool success = true;
  if (!sInitialized || !mPlugFuncs->GetGaugePressure)
    return false;
  ScopeMutexAcquire("GetGaugePressure", true);
  try {
    mPlugFuncs->GetGaugePressure(name, &outStat, &outPress);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting vacuum gauge pressure "));
    success = false;
  }
  ScopeMutexRelease("GetGaugePressure");
  return success;
}

/*
 * Aperture functions
 */

// Function to check for support and whether kind value is within limits
int CEMscope::CheckApertureKind(int kind)
{
  CString str;
  if (!sInitialized)
    return 1;
  if (HitachiScope && kind != 2) {
    SEMMessageBox("Aperture kind must be 2 for a Hitachi scope");
    return 1;
  }
  if (FEIscope) {
    if (!mPlugFuncs->GetApertureSize)
      mFEIhasApertureSupport = 0;
    if (mSimulationMode < 0)
      mFEIhasApertureSupport = 1;
    else if (mFEIhasApertureSupport < 0 && mPlugFuncs->GetApertureSize) {
      try {

        // This function will throw only if there is no automation, so it doesn't matter
        // if it fails to work
        mPlugFuncs->GetApertureSize(OBJECTIVE_APERTURE);
        mFEIhasApertureSupport = 1;
      }
      catch (_com_error E) {
        mFEIhasApertureSupport = 0;
      }
    }
    if (!mFEIhasApertureSupport) {
      SEMMessageBox("Aperture movement is not available on this scope");
      return 1;
    }
    if (kind < 0 || kind > 4) {
      SEMMessageBox("Aperture number must be between 0 and 4 for FEI scope");
      return 1;
    }
  }
  if (JEOLscope && (kind < 0 || kind > 11 || (!mJeolHasExtraApertures && 
    (kind < 1 || kind > 6)))) {
    str.Format("Aperture number %d is out of range, it must be between %d and %d", kind,
      mJeolHasExtraApertures ? 0 : 1, mJeolHasExtraApertures ? 11 : 6);
    SEMMessageBox(str);
    return 1;
  }
  return 0;
}

// Get size of given aperture
int CEMscope::GetApertureSize(int kind)
{
  CString mess = "getting size number of aperture: ";
  int result = -1;
  if (CheckApertureKind(kind) || !mPlugFuncs->GetApertureSize)
    return -1;
  if (mSimulationMode < 0)
    return sSimApertureSize[kind];
  ScopeMutexAcquire("GetApertureSize", true);
  try {
    result = mPlugFuncs->GetApertureSize(kind);
    if (result < 0) {
      mess = "Error " + mess;
      if (FEIscope && mPlugFuncs->GetLastErrorString)
        mess += mPlugFuncs->GetLastErrorString();
      SEMMessageBox(mess);
    }
  }
  catch (_com_error E) {
    if (FEIscope && mPlugFuncs->GetLastErrorString)
      mess += mPlugFuncs->GetLastErrorString();
    SEMReportCOMError(E, _T(mess));
  }
  ScopeMutexRelease("GetApertureSize");
  return result;
}

// Set size of given aperture, which may retract it
// This starts a thread
bool CEMscope::SetApertureSize(int kind, int size)
{
  if (CheckApertureKind(kind) || !mPlugFuncs->SetApertureSize)
    return false;
  if (mSimulationMode < 0) {
    sSimApertureSize[kind] = size;
    return true;
  }
  mApertureTD.actionFlags = APERTURE_SET_SIZE;
  mApertureTD.apIndex = kind;
  mApertureTD.sizeOrIndex = size;
  if (StartApertureThread("setting size number of aperture "))
    return false;
  return true;
}

// Get X/Y position of given aperture, a value between -1 and 1
bool CEMscope::GetAperturePosition(int kind, float &posX, float &posY)
{
  bool result = true;
  double xpos, ypos;
  if (CheckApertureKind(kind) || !mPlugFuncs->GetAperturePosition)
    return false;
  ScopeMutexAcquire("GetAperturePosition", true);
  try {
    mPlugFuncs->GetAperturePosition(kind, &xpos, &ypos);
    posX = (float)xpos;
    posY = (float)ypos;
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting X/Y position of aperture "));
  }
  ScopeMutexRelease("GetAperturePosition");
  return result;
}

// Set X/Y position of given aperture.  Starts a thread
bool CEMscope::SetAperturePosition(int kind, float posX, float posY)
{
  if (CheckApertureKind(kind) || !mPlugFuncs->SetAperturePosition)
    return false;
  mApertureTD.actionFlags = APERTURE_SET_POS;
  mApertureTD.apIndex = kind;
  mApertureTD.posX = posX;
  mApertureTD.posY = posY;
  if (StartApertureThread("setting position of aperture "))
    return false;
  return true;
}

// Higher level function to take out an aperture and save its position
// This starts a thread too, through SetApertureSize
int CEMscope::RemoveAperture(int kind)
{
  int size = GetApertureSize(kind);
  if (size < 0)
    return 1;
  if (JEOLscope) {
    if (!GetAperturePosition(kind, mSavedAperturePosX[kind],
      mSavedAperturePosY[kind]))
      return 2;
    mSavedApertureSize[kind] = size;
    SEMTrace('1', "RemoveAperture %d: size %d, at %f  %f", kind, size,
      mSavedAperturePosX[kind], mSavedAperturePosY[kind]);
  }
  if (!SetApertureSize(kind, 0))
    return 1;
  return 0;
}

// Higher level function to reinsert a removed aperture and restore its position
int CEMscope::ReInsertAperture(int kind)
{
  CString str;
  int retval = 0;
  if (CheckApertureKind(kind))
    return 1;
  if (!mPlugFuncs->SetApertureSize || (JEOLscope && !mPlugFuncs->SetAperturePosition))
    return 3;
  if (!JEOLscope) {
    mApertureTD.actionFlags = APERTURE_SET_SIZE;
    mApertureTD.sizeOrIndex = 1;
  } else {
    if (mSavedApertureSize[kind] < 0) {
      str.Format("No size and position has been saved for aperture number %d by the "
        "function to remove an aperture", kind);
      SEMMessageBox(str);
      return 2;
    }
    mApertureTD.actionFlags = APERTURE_SET_SIZE | APERTURE_SET_POS;
    mApertureTD.sizeOrIndex = mSavedApertureSize[kind];
    mApertureTD.posX = mSavedAperturePosX[kind];
    mApertureTD.posY = mSavedAperturePosY[kind];
  }
  mApertureTD.apIndex = kind;
  if (StartApertureThread("reinserting aperture "))
    retval = 4;
  mSavedApertureSize[kind] = -1;
  return retval;
}

// Go to the next phase plate position
bool CEMscope::MovePhasePlateToNextPos()
{
  if (!sInitialized || !mPlugFuncs->GoToNextPhasePlatePos)
    return false;
  if (FEIscope && mWinApp->FilterIsSelectris())
    mCamera->SetSuspendFilterUpdates(true);
  mApertureTD.actionFlags = APERTURE_NEXT_PP_POS;
  if (StartApertureThread("moving phase plate to next position "))
    return false;
  return true;
}

// Looks up an aperture size for the given aperture from its index, NUMBERED FROM 1
int CEMscope::FindApertureSizeFromIndex(int apInd, int sizeInd)
{
  int ap;
  for (ap = 0; ap < (int)mApertureSizes.size(); ap++) {
    if (mApertureSizes[ap][0] == apInd) {
      if (sizeInd <= 0 || sizeInd >= (int)mApertureSizes[ap].size())
        return 0;
      return mApertureSizes[ap][sizeInd];
    }
  }
  return 0;
}

// Looks up the aperture index, NUMBERED FROM 1, with the given size for a given aperture
// Returns 0 if there IS a list and the size is not in it, or -1 if there is no list
int CEMscope::FindApertureIndexFromSize(int apInd, int size)
{
  int ap, ind;
  for (ap = 0; ap < (int)mApertureSizes.size(); ap++) {
    if (mApertureSizes[ap][0] == apInd) {
      for (ind = 1; ind < (int)mApertureSizes[ap].size(); ind++)
        if (mApertureSizes[ap][ind] == size)
          return ind;
      return 0;
    }
  }
  return -1;
}

// Can be called for both scopes, produces useful error messages when the lookup fails
int CEMscope::FindApertureIndexFromSize(int apInd, int size, CString &errStr)
{
  int index = size;
  if (size > 0 && JEOLscope) {
    index = FindApertureIndexFromSize(apInd, size);
    if (!index) {
      errStr.Format("Size %d is not in the list from the AperturesSizes property for "
        "aperture %d in line:\n\n", size, apInd);
      
    } else if (index < 0 && size > 4) {
      errStr.Format("There is no ApertureSizes property with a size list for"
        " aperture %d; \r\n %d is probably an incorrect entry for the position index",
        apInd, size);
      index = -size;
    }
  }
  return index;
}

// Return the current position of the phase plate
int CEMscope::GetCurrentPhasePlatePos(void)
{
  CString mess;
  int pos = -1;
  if (!sInitialized || !mPlugFuncs->GetCurPhasePlatePos)
    return -1;
  ScopeMutexAcquire("GetCurrentPhasePlatePos", true);
  try {
    pos = mPlugFuncs->GetCurPhasePlatePos();
    if (pos < 0) {
      mess = "Error getting current phase plate position";
      if (mPlugFuncs->GetLastErrorString)
        mess += mPlugFuncs->GetLastErrorString();
      SEMMessageBox(mess);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting current phase plate position "));
  }
  ScopeMutexRelease("GetCurrentPhasePlatePos");
  return pos;
}

// Get position of beam stop
int CEMscope::GetBeamStopPos()
{
  int pos = -2;
  if (!sInitialized || !mPlugFuncs->GetBeamStopper)
    return -2;
  ScopeMutexAcquire("GetBeamStopPos", true);
  try {
    pos = mPlugFuncs->GetBeamStopper();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting beam stop position "));
  }
  return pos;
}

// Set beam stop position through aperture thread
bool CEMscope::SetBeamStopPos(int newPos)
{
  if (!sInitialized || !mPlugFuncs->SetBeamStopper)
    return false;
  mApertureTD.actionFlags = APERTURE_BEAM_STOP;
  mApertureTD.apIndex = newPos;
  if (StartApertureThread("setting beam stop position "))
    return false;
  return true;
}

// Start thread for aperture or phase plate movement
int CEMscope::StartApertureThread(const char *descrip)
{
  mApertureTD.description = descrip;
  mApertureThread = AfxBeginThread(ApertureMoveProc, &mApertureTD,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  mApertureThread->m_bAutoDelete = false;
  mApertureThread->ResumeThread();
  mWinApp->AddIdleTask(TASK_MOVE_APERTURE, 0, 30000);
  mMovingAperture = true;
  return 0;
}

// Is the Aperture busy?
int CEMscope::ApertureBusy()
{
  int retval = UtilThreadBusy(&mApertureThread);
  mMovingAperture = retval > 0;
  if (retval <= 0)
    mCamera->SetSuspendFilterUpdates(false);
  if (!retval && mMonitorC2ApertureSize > 0 && mApertureTD.apIndex == 1)
    mWinApp->mBeamAssessor->ScaleIntensitiesIfApChanged(mApertureTD.sizeOrIndex, true);
  return retval;
}

int CEMscope::TaskApertureBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mScope->ApertureBusy();
}

// Delete the thread for moving an aperture, and if there is an error, issue message
void CEMscope::ApertureCleanup(int error)
{
  UtilThreadCleanup(&mApertureThread);
  if (error)
    SEMMessageBox(_T(error == IDLE_TIMEOUT_ERROR ? "Time out " : "An error occurred ") + 
      mApertureTD.description);
  mWinApp->ErrorOccurred(error);
  mMovingAperture = false;
  mCamera->SetSuspendFilterUpdates(false);
}

// The procedure for aperture or phase plate movement
UINT CEMscope::ApertureMoveProc(LPVOID pParam)
{
  ApertureThreadData *td = (ApertureThreadData *)pParam;
  int retval = 0;
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  ScopeMutexAcquire("ApertureMoveProc", true);

  if (FEIscope) {
    if (td->plugFuncs->BeginThreadAccess(1, 0)) {
      sThreadErrString.Format("Error creating second instance of microscope object for"
        " moving %s", (td->actionFlags & APERTURE_NEXT_PP_POS) ? "phase plate" :
        "aperture");
      SEMErrorOccurred(1);
      retval = 1;
    }
  }
  if (!retval) {
    try {
      if (td->actionFlags & APERTURE_SET_SIZE) {
        td->plugFuncs->SetApertureSize(td->apIndex, td->sizeOrIndex);
      }
      if (td->actionFlags & APERTURE_SET_POS) {
        td->plugFuncs->SetAperturePosition(td->apIndex, (double)td->posX, 
          (double)td->posY);
      }
      if (td->actionFlags & APERTURE_NEXT_PP_POS) {
        retval = td->plugFuncs->GoToNextPhasePlatePos(0);
        if (retval && td->plugFuncs->GetLastErrorString)
          td->description += td->plugFuncs->GetLastErrorString();
      }
      if (td->actionFlags & APERTURE_BEAM_STOP) {
        td->plugFuncs->SetBeamStopper(td->apIndex);
      }
    }
    catch (_com_error E) {

      // This makes it add the error string to the description and leave the message for
      // later
      SEMReportCOMError(E, td->description, &td->description);
      retval = 1;
    }
  }

  if (FEIscope && !(td->actionFlags & APERTURE_BEAM_STOP)) {
    td->plugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  ScopeMutexRelease("ApertureMoveProc");
  return retval;
}

// Set microprobe/nanoprobe on FEI
BOOL CEMscope::SetProbeMode(int micro, BOOL fromLowDose)
{
  BOOL success = true, ifSTEM;
  double ISX, ISY, beamX, beamY, toBeamX, toBeamY, delFocus;
  bool adjustFocus = mAdjustFocusForProbe && mFirstFocusForProbe > EXTRA_VALUE_TEST;
  bool adjustShift = false;
  int magInd;
  ScaleMat fromMat, toMat;

  if (!sInitialized)
    return false;

  if (ReadProbeMode() == (micro ? 1 : 0))
    return true;

  if (fromLowDose && mFirstBeamXforProbe > EXTRA_VALUE_TEST && FEIscope) {
    magInd = GetMagIndex();
    fromMat = mShiftManager->GetBeamShiftCal(magInd, -3333, micro ? 0 : 1);
    toMat = mShiftManager->GetBeamShiftCal(magInd, -3333, micro ? 1 : 0);
    adjustShift = toMat.xpx != 0. && fromMat.xpx != 0.;
  }

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
    if (!RunSynchronousThread(SYNCHRO_DO_PROBE, (micro ? imMicroProbe : imNanoProbe),
      0, NULL))
      return false;
    mReturnedProbeMode = mProbeMode = micro ? 1 : 0;
    if (!ifSTEM) {
      mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostProbeDelay);
      if (success && !SetImageShift(ISX, ISY))
        success = false;
      if (adjustFocus && delFocus) {
        IncDefocus(delFocus);
      }
      if (success && adjustShift) {
        if (GetBeamShift(beamX, beamY)) {
          SEMTrace('b', "After, beam %.3f %.3f  change to %.3f %.3f", beamX, beamY,
            beamX + toBeamX, beamY + toBeamY);
          if (!SetBeamShift(beamX + toBeamX, beamY + toBeamY))
            success = false;
        } else {
          success = false;
        }
      }
      mFirstFocusForProbe = GetDefocus();
      if (!GetBeamShift(mFirstBeamXforProbe, mFirstBeamYforProbe))
        success = false;
    }
  }
  return success;
}

BOOL CEMscope::SetProbeKernel(SynchroThreadData *sytd)
{
  try {
    mPlugFuncs->SetProbeMode(sytd->newIndex);
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting probe mode "));
    return false;
  }
  return true;
}

// set probe mode directly or by changing low dose parameters
BOOL CEMscope::SetProbeOrAdjustLDArea(int inProbe)
{
  if (AdjustLDAreaForItem(1, inProbe))
    return true;
  return SetProbeMode(inProbe);
}

// Get microprobe/nanoprobe on FEI or return the default value for TEM/STEM on other scope
int CEMscope::ReadProbeMode(void)
{
  int subMode, retval = 1;
  bool inSTEM;
  if (!sInitialized)
    return 0;
  ScopeMutexAcquire("ReadProbeMode", true);
  if (FEIscope) {
    try {
      if (!mLastMagIndex)
        PLUGSCOPE_GET(SubMode, subMode, 1);
      inSTEM = !(!mWinApp->ScopeHasSTEM() || !mPlugFuncs->GetSTEMMode());
      if (!inSTEM && mLastMagIndex < GetLowestMModeMagInd() &&
        (mLastMagIndex > 0 || subMode == psmLAD)) {
        mProbeMode = retval = 1;
      } else if (inSTEM && MagIsInFeiLMSTEM(mLastMagIndex)) {

        // Keep the current probe mode if option not set, or return and set mode to
        // option value minus 1
        if (!mFeiSTEMprobeModeInLM)
          retval = mProbeMode;
        else
          mProbeMode = retval = B3DMIN(1, mFeiSTEMprobeModeInLM - 1);
      } else {
        PLUGSCOPE_GET(ProbeMode, retval, 1);
        mReturnedProbeMode = mProbeMode = retval;
      }
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

// Simply test if the mag is in either LM range
bool CEMscope::MagIsInFeiLMSTEM(int inMag)
{
  return (inMag < mLowestSTEMnonLMmag[0] ||
    (inMag >= mLowestMicroSTEMmag && inMag < mLowestSTEMnonLMmag[1]));
}

// Directly determine whether scope is in STEM mode
BOOL CEMscope::GetSTEMmode(void)
{
  BOOL result;
  if (!sInitialized || !mWinApp->ScopeHasSTEM())
    return false;

  // Plugin will use state value if getting fast
  bool needMutex = !(JEOLscope && sGettingValuesFast);
  if (needMutex)
    ScopeMutexAcquire("GetSTEMmode", true);
  SEMSetFunctionCalled("GetSTEMmode");
  try {
    result = mPlugFuncs->GetSTEMMode();
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("getting TEM vs. STEM mode "));
    result = false;
  }
  SEMSetFunctionCalled("", "");
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
  double ratio, factor, newIntensity, delISX, delISY;
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
          if (oldMag != newMag && mApplyISoffset &&
            AssessMagISchange(oldMag, newMag, false, delISX, delISY))
            mPlugFuncs->SetImageShift(delISX, delISY);
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
  const char *useName = name;
  if (!mScopeMutexHandle)
    return true;

  if (mScopeMutexOwnerId == GetCurrentThreadId()) {
  //  if (TRACE_MUTEX)
  //    TRACE("This thread already had the mutex.\n");
    mScopeMutexOwnerCount ++;
    return true;
  }

  // If the lender is reacquiring it, use the lender string
  if (!strcmp(name, "MutexLender"))
    useName = mScopeMutexLenderStr;

  if (TRACE_MUTEX) SEMTrace('1', "%s waiting for Mutex",useName);
  
  // This wait does not seem to allow enough time for threads to run properly, so set
  // the timeout low when not retrying.  It should loop and sleep instead...
  while ((mutex_status = WaitForSingleObject(mScopeMutexHandle, 
    retry ? MUTEX_TIMEOUT_MS : 20)) == WAIT_TIMEOUT) {
    if (retry) {
      if (TRACE_MUTEX) 
        SEMTrace('1', "%s timed out getting mutex, currently held by %s.. trying again.",
        useName, mScopeMutexOwnerStr);
      continue;
    } else {
      if (TRACE_MUTEX) 
        SEMTrace('1', "%s failed to get the mutex, currently held by %s. giving up!", 
        useName, mScopeMutexOwnerStr);
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

  strcpy(mScopeMutexOwnerStr,useName);
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

  // If the owner is lending it, copy its name to the lender string
  if (!strcmp(mScopeMutexOwnerStr, "MutexLender")) {
    strcpy(mScopeMutexLenderStr, mScopeMutexOwnerStr);
  } else {

    if ( TRACE_MUTEX && (0 != strcmp(mScopeMutexOwnerStr,name))) {
      SEMTrace('1', "%s tried to release the mutex, but the mutex is held by %s!",name,mScopeMutexOwnerStr);
    }
    assert(0 == strcmp(mScopeMutexOwnerStr, name));
  }
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
float CEMscope::GetJeolStageRounding() {return sJEOLstageRounding;};
float CEMscope::GetJeolPiezoRounding() {return sJEOLpiezoRounding;}

void CEMscope::SetSimCartridgeLoaded(int inVal)
{
  sSimLoadedCartridge = inVal;
}

// Central place to change the offset and make sure that scope defocus is changed in 
// tandem and maintain the currently set offset value
void CEMscope::SetLDViewDefocus(float inVal, int area)
{
  if (area) 
    mSearchDefocus = inVal; 
  else 
    mLDViewDefocus = inVal;
  if (mWinApp->mLowDoseDlg.GetTrulyLowDose() && 
    mLowDoseSetArea == (area ? SEARCH_AREA : VIEW_CONSET) &&
    fabs(mCurDefocusOffset - inVal) > 1.e-3) {
    IncDefocus(inVal - mCurDefocusOffset);
    mCurDefocusOffset = inVal;
  }
}

void CEMscope::SetBlankingFlag(BOOL state)
{
  mBeamBlanked = state;
}


void CEMscope::WaitForStageDone(StageMoveInfo *smi, char *procName)
{
  double sx, sy, sz, errx, erry;
  int i, status, ntry = 10, ntryReady = 300;
  int waitTime = 100;
  JeolStateData *jsd = smi->JeolSD;
  bool useEvents = jsd->eventDataIsGood && !sJeolReadStageForWait;

  if (FEIscope)
    return;

  // If updating by event, first wait to see it go non-ready, using the ready state flag
  if (JEOLscope) {
    for (i = 0; i < ntry; i++) {
      if (!sInitialized)
        return;
      if (useEvents)
        GetValuesFast(1);
      status = smi->plugFuncs->GetStageStatus();
      if (useEvents)
        GetValuesFast(-1);
      if (!useEvents && !status)
        SEMTrace('w', "WaitForStageDone: wait for nonready, status %d at %.3f", status,
          SEMSecondsSinceStart());
      if (status)
        break;
      if (i < ntry - 1) {
        ScopeMutexRelease(procName);
        Sleep(waitTime);
        ScopeMutexAcquire(procName, true);
      }
    }

    if (jsd->eventDataIsGood) {

      // If not ready was seen, wait on ready flag to avoid load on system,
      // then fall back to getting status for safety's sake
      if (i < ntry) {
        SEMTrace('w', "WaitForStageDone: Not ready %s seen (status %d) in %d ms at %.3f",
          sJeolReadStageForWait ? "reading" : "event", status, i * waitTime,
          SEMSecondsSinceStart());
        for (i = 0; i < ntryReady; i++) {
          if (!sInitialized)
            return;
          if (useEvents)
            GetValuesFast(1);
          status = smi->plugFuncs->GetStageStatus();
          if (useEvents)
            GetValuesFast(-1);
          if (!useEvents && status)
            SEMTrace('w', "WaitForStageDone: wait for ready, status %d at %.3f", status,
              SEMSecondsSinceStart());
          if (!status)
            break;
          ScopeMutexRelease(procName);
          Sleep(waitTime);
          ScopeMutexAcquire(procName, true);
        }
        SEMTrace('w', "WaitForStageDone: Ready %s %sseen in %d ms at %.3f",
          sJeolReadStageForWait ? "reading" : "event", i == ntryReady ? "NOT " : "",
          i * waitTime, SEMSecondsSinceStart());

        if (sJeolReadStageForWait)
          Sleep(waitTime);

      } else
        SEMTrace('w', "WaitForStageDone: Not ready %s NOT seen in %d ms at %.3f",
          sJeolReadStageForWait ? "reading" : "event", (ntry - 1) * waitTime,
          SEMSecondsSinceStart());
    }
  }

  // If no event update or didn't see busy, or events never came through,
  // check status until it clears; otherwise just do a direct status read to make sure
  while (1) {
    if (!sInitialized)
      return;
    try {
      if (!SEMCheckStageTimeout()) {
        status = smi->plugFuncs->GetStageStatus();
        if (!status)
          break;
        SEMTrace('w', "WaitForStageDone: checking ready, status %d at %.3f", status,
          SEMSecondsSinceStart());
      }
    }
    catch (_com_error E) {

    }
    ScopeMutexRelease(procName);
    Sleep((JEOLscope ? 1 : 3) * waitTime);
    ScopeMutexAcquire(procName, true);
  }

  // Get actual position now and update state
  if (JEOLscope  && sInitialized) {
    try {
      smi->plugFuncs->GetStagePosition(&sx, &sy, &sz);
      if (smi->axisBits & axisXY) {
        errx = 1.e6 * sx - smi->x;
        erry = 1.e6 * sy - smi->y;
        SEMTrace('w', "WaitForStageDone: move to %.3f %.3f  error %.3f %.3f  at %.3f",
          smi->x, smi->y, errx, erry, SEMSecondsSinceStart());
        if (fabs(errx) > 1. || fabs(erry) > 1.) {
          Sleep(600);
          smi->plugFuncs->GetStagePosition(&sx, &sy, &sz);
          SEMTrace('w', "WaitForStageDone:     error %.3f %.3f  at %.3f",
            1.e6 * sx - smi->x, erry = 1.e6 * sy - smi->y, SEMSecondsSinceStart());
        }
      }
    }
    catch (_com_error E) {
    }
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

// Waits for the lens relaxation events to come in that it has started and finished
// on a JEOL after changing the given type of lens
int CEMscope::WaitForLensRelaxation(int type, double earliestTime)
{
  double startTime;
  BOOL state, eventTime;
  const char *names[] = {"mag", "spot", "alpha", "LM-nonLM mag"};
  if (!JEOLscope || !(type & sJeolRelaxationFlags) || sJeolSD->JeolSTEM)
    return 0;
  startTime = GetTickCount();
  while (SEMTickInterval(startTime) < sJeolStartRelaxTimeout) {
    SEMAcquireJeolDataMutex();
    state = sJeolSD->relaxingLenses;
    eventTime = sJeolSD->relaxStartTime;
    SEMReleaseJeolDataMutex();
    if (SEMTickInterval(eventTime, earliestTime) > 0.)
      state = 1;
    if (state)
      break;
    Sleep(50);
  }
  if (!state) {
    SEMTrace('0', "%.3f: WARNING: Lens relaxation event on %s not detected within %d msec"
      " timeout, going on", SEMSecondsSinceStart(), names[B3DMIN(2, type - 1)], 
      sJeolStartRelaxTimeout);
    return 1;
  }
  while (SEMTickInterval(startTime) < sJeolEndRelaxTimeout) {
    SEMAcquireJeolDataMutex();
    state = sJeolSD->relaxingLenses;
    eventTime = sJeolSD->relaxEndTime;
    SEMReleaseJeolDataMutex();
    if (SEMTickInterval(eventTime, earliestTime) > 0.)
      state = 0;
    if (!state)
      break;
    Sleep(50);
  }
  if (state) {
    SEMTrace('0', "%.3f: WARNING: End of lens relaxation not detected within %d msec, "
      "going on", SEMSecondsSinceStart(), sJeolEndRelaxTimeout);
    return 1;
  }
  return 0;
}

// Get the maximum image shift allowed by deflectors at the given mag with current offsets
float CEMscope::GetMaxJeolImageShift(int magInd)
{
  double axisISX, axisISY, maxISX, maxISY;
  const double Jeol_IS1X_to_um = 0.00508;
  const double Jeol_IS1Y_to_um = 0.00434;
  if (!JEOLscope)
    return 0.0f;

  // Get neutral and other relevant values at this mag
  GetTiltAxisIS(axisISX, axisISY, magInd);
  SEMAcquireJeolDataMutex();

  // Convert maximum deflector value just as in plugin, subtract axis component
  maxISX = 0x3FFF * (sJeolSD->JeolSTEM ? mJeolParams.CLA1_to_um : Jeol_IS1X_to_um) - 
    fabs(axisISX);
  maxISY = 0x3FFF * (sJeolSD->JeolSTEM ? mJeolParams.CLA1_to_um : Jeol_IS1Y_to_um) - 
    fabs(axisISY);
  SEMReleaseJeolDataMutex();

  // Retrun minimum of the two shifts
  return (float)B3DMIN(mShiftManager->RadialShiftOnSpecimen(maxISX, 0., magInd),
    mShiftManager->RadialShiftOnSpecimen(0., maxISY, magInd));
}

void CEMscope::SetRetryStageMove(int inVal)
{
  sRetryStageMove = inVal;
}

int CEMscope::GetRetryStageMove()
{
  return sRetryStageMove;
}

// Gets and sets for the properties
void CEMscope::SetJeolRelaxationFlags(int inVal) {
  sJeolRelaxationFlags = inVal;
}
int CEMscope::GetJeolRelaxationFlags() {
  return sJeolRelaxationFlags;
}
void CEMscope::SetJeolStartRelaxTimeout(int inVal) {
  sJeolStartRelaxTimeout = inVal;
}
int CEMscope::GetJeolStartRelaxTimeout() {
  return sJeolStartRelaxTimeout;
}
void CEMscope::SetJeolEndRelaxTimeout(int inVal) {
  sJeolEndRelaxTimeout = inVal;
}
int CEMscope::GetJeolEndRelaxTimeout() {
  return sJeolEndRelaxTimeout;
}


BOOL CEMscope::GetClippingIS()
{
  return sClippingIS;
}

// Return whether IS was clipped: this is valid only if SetISwasClipped is called
// with false before setting IS
BOOL CEMscope::GetISwasClipped()
{
  return sISwasClipped;
}

void CEMscope::SetISwasClipped(BOOL val) 
{
  sISwasClipped = val;
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
    SEMMessageBox(message, MB_EXCLAME);
    sClippingIS = false;
    SEMErrorOccurred(1);
  }

  SEMTrace('1', "Image shift 0x%X  0x%X out of range, being clipped to limits",
    jShiftX, jShiftY);
  sISwasClipped = true;
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
    mShiftBoundaries.clear();
    mNumShiftBoundaries = 0;
    for (int i = 2; i < MAX_MAGS; i++)
      AddShiftBoundary(i);
  }
}

void CEMscope::SetJeolStemLMCL3toUm(float inVal)
{
  mJeolParams.StemLMCL3ToUm = inVal;
}

float CEMscope::GetJeolStemLMCL3toUm()
{
  return mJeolParams.StemLMCL3ToUm;
}

void CEMscope::SetMessageWhenClipIS(BOOL inVal)
{
  sMessageWhenClipIS = inVal;
}
BOOL CEMscope::GetMessageWhenClipIS() {return sMessageWhenClipIS;};

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

BOOL SEMScanningMags()
{
  return sScanningMags;
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
  BOOL startingBlank = mBeamBlanked;

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
        mBeamBlanked = true;

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
          mBeamBlanked = false;
          Sleep(preTicks);
        }

        // Open film shutter
        err = myAda->GetOpenShutter();
        if (err < 0)
          throw 3;
        FILM_WAIT_LOOP(10, myAda->GetShutterStatus(), shutterOpen, 7);

        // Open beam shutter if not opened before, but let film shutter effect settle
        if (mBeamBlanked) {
          Sleep(preUnblankDelay);
          info->plugFuncs->SetBeamBlank(*vFalse);
          mBeamBlanked = false;
        }

        // Wait for the exposure time
        ticks = (int)(1000. * info->x);
        if (ticks > 0)
          Sleep(ticks);

        // Close the beam shutter then the film shutter
        info->plugFuncs->SetBeamBlank(*vTrue);
        mBeamBlanked = true;
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
        mBeamBlanked = startingBlank;
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
  int lowest1 = GetLowestMModeMagInd();
  int lowest2 = lowest1;
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
  return GetLowestMModeMagInd();
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
      (saveMag >= lowestM && otherMag < lowestM && !mApplyISoffset && !mLowDoseMode));
  else
    return (saveMag && !otherMag && mApplyISoffset);
}

// Returns true if IS is/may be reset on a mag change: JEOL or Hitachi in LM/HR
bool CEMscope::MagChgResetsIS(int toInd)
{
  return JEOLscope || (HitachiScope && (toInd < mLowestMModeMagInd || 
    (mLowestSecondaryMag > 0 && toInd >= mLowestSecondaryMag) || (mHitachiResetsISinHC &&
     toInd >= mLowestMModeMagInd && toInd < mLowestSecondaryMag)));
}

int CEMscope::LookupRingIS(int lastMag, BOOL changedEFTEM, BOOL crossedLM)
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
    else if (crossedLM)
      delay += 250;
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
  static bool doMessage = true;
  int i, err;
  double minDrift, maxDrift;
  float readout, f4readout;
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
  if (restoreShutter < -900 && mSkipAdvancedScripting > 0)
    restoreShutter = -950;
  err = mPlugFuncs->LookupScriptingCamera((LPCTSTR)CCDname, refresh, restoreShutter,
    &params->eagleIndex, &minDrift, &maxDrift);

  if (err == 3)
    SEMMessageBox(CString(mPlugFuncs->GetLastErrorString()));
  if (!err) {
    params->maximumDrift = (float)maxDrift;
    if (mPluginVersion >= FEI_PLUGIN_DOES_FALCON3) {

      // Entries for testing the interface for advanced scripting cameras
      if ((params->FEItype == 10 + FALCON3_TYPE || params->FEItype == 10 + FALCON4_TYPE) 
        && mSimulationMode) {
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
      params->CamFlags = (params->eagleIndex & ~PLUGFEI_INDEX_MASK);
      SEMTrace('E', "index ret %x  flags %x  mind %f  maxd %f", params->eagleIndex, 
        params->CamFlags, minDrift, maxDrift);
      if (params->CamFlags & PLUGFEI_USES_ADVANCED)
        mCamera->SetOtherCamerasInTIA(false);
      if (doMessage) {
        mWinApp->AppendToLog(CString("Connected to ") +
          B3DCHOICE(params->CamFlags & PLUGFEI_USES_ADVANCED, 
            UtapiSupportsService(UTSUP_CAM_SINGLE) ? "UTAPI" : "Advanced", "Standard") +
          " Scripting interface for FEI camera access" + B3DCHOICE(
          mSkipAdvancedScripting > 0, " because property SkipAdvancedScripting is set!",
            ""));
        doMessage = false;
      }
      if (params->CamFlags & PLUGFEI_USES_ADVANCED) {
        if (fabs(params->minExposure - DEFAULT_FEI_MIN_EXPOSURE) < 1.e-5)
          params->minExposure = (float)minDrift;
        else
          params->minExposure = (float)B3DMAX(params->minExposure, minDrift);
        params->maxExposure = params->maximumDrift;
        params->maximumDrift = 0.;
      } else {
        params->minimumDrift = (float)B3DMAX(params->minimumDrift, minDrift);
      }

      // Promote a Falcon 2 to 3 automatically, and adjust the frame time if low
      if (params->FEItype == FALCON2_TYPE && (params->CamFlags & PLUGFEI_CAM_CAN_COUNT) && 
        (params->CamFlags & PLUGFEI_CAM_CAN_ALIGN))
        params->FEItype = FALCON3_TYPE;

      // For Falcon 3 or 4, set the general readout interval if it is a Falcon 2 value,
      // and set this camera's interval from that value if not set already, provided it
      // doesn't match the value set for the other kind of camera.  This is going to get
      // out of hand with more timings
      f4readout = params->falconVariant == FALCON4I_VARIANT ? mFalcon4iReadoutInterval :
        mFalcon4ReadoutInterval;
      if (params->FEItype == FALCON3_TYPE && params->ReadoutInterval <= 0.) {
        if (mCamera->GetFalconReadoutInterval() > 0.05)
          mCamera->SetFalconReadoutInterval(mFalcon3ReadoutInterval);
        readout = mCamera->GetFalconReadoutInterval();
        if (fabs(readout - mFalcon4ReadoutInterval) < 0.01 * mFalcon4ReadoutInterval ||
          fabs(readout - mFalcon4iReadoutInterval) < 0.01 * mFalcon4iReadoutInterval)
          params->ReadoutInterval = mFalcon3ReadoutInterval;
        else
          params->ReadoutInterval = readout;
      }
      if (params->FEItype == FALCON4_TYPE  && params->ReadoutInterval <= 0.) {
        if (mCamera->GetFalconReadoutInterval() > 0.05)
          mCamera->SetFalconReadoutInterval(f4readout);
        readout = mCamera->GetFalconReadoutInterval();
        if (fabs(readout - (mFalcon3ReadoutInterval)) < 0.01 * mFalcon3ReadoutInterval ||
          fabs((params->falconVariant == FALCON4I_VARIANT ? mFalcon4ReadoutInterval :
            mFalcon4iReadoutInterval) - readout) < 0.01 * mFalcon4ReadoutInterval)
          params->ReadoutInterval = f4readout;
        else
          params->ReadoutInterval = readout;
      }
      if (params->FEItype == FALCON4_TYPE && mPluginVersion >= PLUGFEI_CAM_SAVES_EER &&
        mCamera->GetCanSaveEERformat() < 0)
        mCamera->SetCanSaveEERformat(1);
      if (params->FEItype == FALCON4_TYPE)
        mCamera->SetMinAlignFractionsCounting(mMinFalcon4CountAlignFrac);
      if (params->FEItype == FALCON3_TYPE && params->addToExposure < -1.)
        params->addToExposure = mAddToFalcon3Exposure;
      if (params->FEItype == FALCON4_TYPE && params->addToExposure < -1.)
        params->addToExposure = f4readout / 2.f;
      if (params->canTakeFrames) {
        if (params->FEItype == OTHER_FEI_TYPE &&mPluginVersion >= PLUGFEI_CONTINUOUS_SAVE)
          params->CamFlags |= PLUGFEI_CAM_CONTIN_SAVE;
        else
          params->canTakeFrames = 0;
      }
    } else {
      params->minimumDrift = (float)B3DMAX(params->minimumDrift, minDrift);
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
  static bool firstTime = true;

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
  else if (firstTime) {
    mWinApp->AppendToLog(mPlugFuncs->GetLastErrorString());
    firstTime = false;
  }
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

void CEMscope::SetJeolReadStageForWait(BOOL inVal)
{
  sJeolReadStageForWait = inVal;
}
BOOL CEMscope::GetJeolReadStageForWait() {return sJeolReadStageForWait;};

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
  bool allIn, retval = true, needScreenUp = false;
  int i, j, k, timeout = 5000;
  double startTime, insertedTime = -1., selectedTime = -1.;
  int inserted, detectorID;
  int detOrScreen = mCamera->GetScreenInIfDetectorOut();
  if (!sInitialized || !JEOLscope)
    return false;
  if (!mJeolSD.numDetectors)
    return true;
  if (detOrScreen >= 0) {
    if (numberInList(detOrScreen, detInd, numDet, 0))
      needScreenUp = true;
    else {
      SEMTrace('R', "Lowering screen before unselecting/retracting detector %d",
        mJeolSD.detectorIDs[detOrScreen]);
      if (!SynchronousScreenPos(spDown))
        return false;
    }
  }

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
          detectorID = mJeolSD.detectorIDs[i];
          SEMTrace('R', "Retracting detector %d", detectorID);
          mPlugFuncs->SetDetectorPosition(detectorID, 0);
          startTime = GetTickCount();
          while (mJeolSD.detInserted[i] && SEMTickInterval(startTime) < timeout) {
            inserted = mPlugFuncs->GetDetectorPosition(detectorID);
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
      if (detInd[i] < mJeolSD.numDetectors && !mJeolSD.detInserted[detInd[i]]) {
        SEMTrace('R', "Inserting detector %d", mJeolSD.detectorIDs[detInd[i]]);
        mPlugFuncs->SetDetectorPosition(mJeolSD.detectorIDs[detInd[i]], 1);
      }
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
        SleepMsg(50);
    }

    // Now change selections of ALL detectors if needed
    for (i = 0; i < mJeolSD.numDetectors; i++) {
      detectorID = mJeolSD.detectorIDs[i];
      inserted = (short)numberInList(i, detInd, numDet, 0);
      if (mJeolSD.detSelected[i] != inserted) {
        SEMTrace('R', "%selecting detector %d", inserted ? "S" : "Des", detectorID);
        mPlugFuncs->SetDetectorSelected(detectorID, inserted);
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
  if (needScreenUp) {
    SEMTrace('R', "Raising screen for detector %d", mJeolSD.detectorIDs[detOrScreen]);
    if (!SynchronousScreenPos(spUp))
      retval = false;
  }
  return retval;
}

bool CEMscope::GetSTEMBrightnessContrast(const char * name, double & bright, double & contrast)
{
  int error = 0;
  if (!sInitialized || !mPlugFuncs->GetDetectorBrightContrast)
    return false;

  ScopeMutexAcquire("GetSTEMBrightnessContrast", true);
  error = mPlugFuncs->GetDetectorBrightContrast(name, &bright, &contrast);
  if (error)
    SEMMessageBox(error == 1 ? "STEM detector not in currently available list for getting"
      " brightness/contrast" : "Error getting STEM detector brightness/contrast");
  ScopeMutexRelease("GetSTEMBrightnessContrast");
  return error == 0;
}

bool CEMscope::SetSTEMBrightnessContrast(const char * name, double bright, double contrast)
{
  int error = 0;
  if (!sInitialized || !mPlugFuncs->SetDetectorBrightContrast)
    return false;

  ScopeMutexAcquire("SetSTEMBrightnessContrast", true);
  error = mPlugFuncs->SetDetectorBrightContrast(name, bright, contrast);
  if (error)
    SEMMessageBox(error == 1 ? "STEM detector not in currently available list for setting"
      " brightness/contrast" : "Error setting STEM detector brightness/contrast");
  ScopeMutexRelease("SetSTEMBrightnessContrast");
  return error == 0;
}

// A single place to evaluate the appropriate beam blank state and whether a low dose area
// needs to be switched to
BOOL CEMscope::NeedBeamBlanking(int screenPos, BOOL STEMmode, BOOL &goToLDarea)
{
  bool mustBeDown = screenPos == spDown && mWinApp->DoDropScreenForSTEM() != 0;
  goToLDarea = false;

  // Blanking during a dark reference takes priority, in case screen signals are screwed
  if (mCamera->EnsuringDark())
    return true;

  // Don't blank if camera is acquiring unless it is being managed by the camera
  // If screen is up blank in low dose, while pretending to be in LD, or for shutterless
  if (mCameraAcquiring && B3DABS(mShutterlessCamera) < 2)
    return false;
  if (screenPos == spUp)
    return mShutterlessCamera || (!mSkipBlankingInLowDose && 
    (mLowDoseMode || mWinApp->mLowDoseDlg.m_bLowDoseMode));
   
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
  return (mShutterlessCamera < 0 && !mLowDoseMode) || (mLowDoseMode && mBlankWhenDown) || 
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
    sGettingValuesFast = sLastGettingFast;
    sLastGettingFast = false;
  } else {
    sLastGettingFast = sGettingValuesFast;
    sGettingValuesFast = enable > 0;
  }
}

bool CEMscope::GetGettingValuesFast()
{
  return sGettingValuesFast;
}


// Process a moveInfo after a stage move to record a valid X/Y backlash or set as invalid
// or, if one-shot flag is set that it was valid at start, check if move was in same
// direction and clear the flag.
void CEMscope::SetValidXYbacklash(StageMoveInfo *info)
{
  bool old = mXYbacklashValid;
  bool didBacklash = info->doBacklash && (info->axisBits & axisXY) && 
    (info->backX || info->backY);

  // tests below result in retaining X/Y backlash state for a Z-only move 
  bool zOnly = (info->axisBits & ~axisZ) == 0;
  double movedFromPosX = info->x - mPosForBacklashX;
  double movedFromPosY = info->y - mPosForBacklashY;
  bool kept = (zOnly && old) || (mBacklashValidAtStart && 
    ((movedFromPosX * mLastBacklashX <= 0. && movedFromPosY * mLastBacklashY <= 0.) || 
    (fabs(movedFromPosX) < mBacklashTolerance &&
      fabs(movedFromPosY) < mBacklashTolerance)));
  bool established = mMinMoveForBacklash > 0. && 
    fabs(info->x - mStartForMinMoveX) >= mMinMoveForBacklash &&
    fabs(info->y - mStartForMinMoveY) >= mMinMoveForBacklash;
  mXYbacklashValid = didBacklash || kept || established;
  if (!zOnly) {
    mPosForBacklashX = info->x;
    mPosForBacklashY = info->y;
    mStageAtLastPos = true;
  }
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

// Do similar operation for Z, allowing an X/Y move only to retain the Z backlash state
void CEMscope::SetValidZbacklash(StageMoveInfo * info)
{
  bool old = mZbacklashValid;
  bool didBacklash = info->doBacklash && (info->axisBits & axisZ) && info->backZ;
  bool xyOnly = (info->axisBits & ~(axisX | axisY)) == 0;
  double movedFromPosZ = info->z - mPosForBacklashZ;
  bool kept = (xyOnly && old) || (mZBacklashValidAtStart && 
    (movedFromPosZ * mLastBacklashZ <= 0 || fabs(movedFromPosZ) < mBacklashTolerance));
  bool established = mMinZMoveForBacklash > 0. &&
    fabs(info->z - mStartForMinMoveZ) >= mMinZMoveForBacklash;
  mZbacklashValid = didBacklash || kept || established;
  if (!xyOnly) {
    mPosForBacklashZ = info->z;
    mStageAtLastZPos = true;
  }
  mZBacklashValidAtStart = false;
  if (!mZbacklashValid) {
    if (old)
      SEMTrace('n', "This stage move invalidated Z backlash data");
    mMinZMoveForBacklash = 0.;
    return;
  }
  if (didBacklash) {
    mLastBacklashZ = (float)info->backZ;
    SEMTrace('n', "Backlash data recorded for this Z stage move, pos %.3f  bl %.1f",
     mPosForBacklashZ, mLastBacklashZ);
  } else if (established) {
    mLastBacklashZ = (info->z > mStartForMinMoveZ ? -1.f : 1.f) * mMinZMoveForBacklash;
    SEMTrace('n', "Z Backlash state established by this stage move");
  } else
    SEMTrace('n', "Z Backlash state retained by this stage move");
  mMinZMoveForBacklash = 0.;
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

// Return true and return backlash values if Z backlash is still valid for given Z
bool CEMscope::GetValidZbacklash(double stageZ, float & backZ)
{
  bool atSamePos = fabs(stageZ - mPosForBacklashZ) < mBacklashTolerance;
  backZ = 0;
  mStageAtLastZPos = mStageAtLastZPos && atSamePos;
  if (!mZbacklashValid)
    return false;
  mZbacklashValid = atSamePos;
  if (!mZbacklashValid) {
    SEMTrace('n', "Z backlash data has become invalid, the stage has moved (%.2f to %.2f)"
      , mPosForBacklashZ, stageZ);
    return false;
  }
  backZ = mLastBacklashZ;
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

// Same for Z
bool CEMscope::StageIsAtSameZPos(double stageZ, float requestedZ)
{
  return mStageAtLastZPos && fabs(stageZ - mPosForBacklashZ) < mBacklashTolerance &&
    fabs((double)(requestedZ - mRequestedStageZ)) < 1.e-5;
}

// Descriptions of the long-running operations
static char *longOpDescription[MAX_LONG_OPERATIONS] = 
{"running buffer cycle", "refilling refrigerant",
  "getting cassette inventory", "running autoloader buffer cycle", "showing message box",
  "updating hardware dark reference", "unloading a cartridge", "loading a cartridge",
  "refilling stage", "refilling transfer tank", "flashing FEG", 
  "refilling stage and transfer tank"};

// Start a thread for a long-running operation: returns 1 if thread busy, 
// 2 if inappropriate in some other way, -1 if nothing was started
int CEMscope::StartLongOperation(int *operations, float *hoursSinceLast, int numOps)
{
  int ind, thread, longOp, scopeOps[MAX_LONG_OPERATIONS], numScopeOps = 0;
  float sinceLast;
  bool needHWDR = false, startedThread = false, suspendFilter = false;
  int now = mWinApp->MinuteTimeStamp();
  // Change second one back to 0 to enable simpleorigin
  short scopeType[MAX_LONG_OPERATIONS] = {1, 0, 1, 1, 1, 0, 1, 1, 2, 2, 0, 2};
  short blocksFilter[MAX_LONG_OPERATIONS] = {0, 4, 4, 4, 0, 0, 4, 4, 0, 0, 4, 0};
  mDoingStoppableRefill = 0;
  mChangedLoaderInfo = false;
  mLongOpErrorToReport = 0;
  if (mJeolHasNitrogenClass > 1) {
    scopeType[LONG_OP_INVENTORY] = 0;
    scopeType[LONG_OP_LOAD_CART] = 0;
    scopeType[LONG_OP_UNLOAD_CART] = 0;
  }

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
          if ((scopeType[longOp] == 1 && !FEIscope) || 
            (scopeType[longOp] == 2 && !JEOLscope))
            return 2;
          if (JEOLscope && !mPlugFuncs->FlashFEG)
            return 3;
        }
        if (longOp == LONG_OP_FILL_STAGE || longOp == LONG_OP_FILL_BOTH)
          mDoingStoppableRefill |= 1;
        if (longOp == LONG_OP_FILL_TRANSFER || longOp == LONG_OP_FILL_BOTH)
          mDoingStoppableRefill |= 2;
        if (longOp == LONG_OP_REFILL) {
          if (!FEIscope && !mHasSimpleOrigin)
            return 2;
        }
        if (FEIscope && blocksFilter[longOp] > 0 && mWinApp->FilterIsSelectris() &&
          mAdvancedScriptVersion >= blocksFilter[longOp])
          suspendFilter = true;
    }
    if (sinceLast < -1.5) {
      hoursSinceLast[ind] = (float)(now - mLastLongOpTimes[longOp]) / 60.f;
      return 0;
    } else if (sinceLast < 0) {
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
      mLongOpThreads[thread] = mCamera->StartHWDarkRefThread();
      sLongOpDescriptions[thread] = longOpDescription[LONG_OP_HW_DARK_REF];
    } else if (thread == 0 && numScopeOps) {
      sLongOpDescriptions[thread] = "";
      mLongOpData[thread].flags = (mDoNextFEGFlashHigh ? LONGOP_FLASH_HIGH : 0) |
        (mHasSimpleOrigin ? LONGOP_SIMPLE_ORIGIN : 0);
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
       mDoNextFEGFlashHigh = false;
      mLongOpData[thread].cartInfo = &mJeolLoaderInfo;
      mLongOpData[thread].maxLoaderSlots = mMaxJeolAutoloaderSlots;
      mLongOpThreads[thread] = AfxBeginThread(LongOperationProc, &mLongOpData[thread],
        THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    } else {
      continue;
    }
    mDoingLongOperation = true;
    mLongOpThreads[thread]->m_bAutoDelete = false;
    mLongOpThreads[thread]->ResumeThread();
    if (!mWinApp->GetSuppressSomeMessages())
      mWinApp->AppendToLog("Started a long running operation: " +
        sLongOpDescriptions[thread]);
    startedThread = true;
  }
  if (startedThread) {
    if (mWinApp->mParticleTasks->GetDVDoingDewarVac())
      mWinApp->SetStatusText(SIMPLE_PANE, "LONG OPERATION");
    else
      mWinApp->SetStatusText(MEDIUM_PANE, "DOING LONG OPERATION");
    mWinApp->UpdateBufferWindows();
    mWinApp->AddIdleTask(TASK_LONG_OPERATION, 0, 0);
    if (suspendFilter)
      mCamera->SetSuspendFilterUpdates(true);
    return 0;
  }
  return -1;
}

// Thread proc for microscope long operations
UINT CEMscope::LongOperationProc(LPVOID pParam)
{
  LongThreadData *lod = (LongThreadData *)pParam;
  HRESULT hr = S_OK;
  CString descrip;
  int retval = 0, longOp, ind, error, idAtZero = -1, fillVal = 0;
  bool fillTransfer, loadCart, valid;
  double startTime;
  const char *name;
  int slot, station, rotation, cartType;
  JeolCartridgeData jcData;
  const char *jeolFEGmess[5] = {
    ": Timeout waiting for the scope to be able to start flashing the FEG",
    ": Scope error flashing the FEG",
    ": Timeout time reached before FEG flashing done; check property JeolFlashFegTimeout",
    ": Timeout waiting for the scope to be ready to turn on emission after flashing",
    ": Timeout before emission reached ON state; check property JeolEmissionTimeout"};

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CEMscope::ScopeMutexAcquire("LongOperationProc", true);

  if (FEIscope && lod->plugFuncs->BeginThreadAccess(1, PLUGFEI_MAKE_VACUUM)) {
    SEMMessageBox("Error creating second instance of microscope object for" +
    sLongOpDescriptions[0]);
    SEMErrorOccurred(1);
    retval = 1;
  }
  if (!retval) {

    // Do the operations in the entered sequence
    for (ind = 0; ind < lod->numOperations; ind++) {
      longOp = lod->operations[ind];
      fillTransfer = longOp == LONG_OP_FILL_TRANSFER || longOp == LONG_OP_FILL_BOTH;
      loadCart = longOp == LONG_OP_LOAD_CART;
      error = 0;
      try {

        // Run buffer cycle
        if (longOp == LONG_OP_BUFFER) {
          lod->plugFuncs->RunBufferCycle();
        }
        // Refill refrigerant
        if (longOp == LONG_OP_REFILL) {
          if (lod->flags & LONGOP_SIMPLE_ORIGIN)
            retval = RefillSimpleOrigin(lod->errString);
          else 
            lod->plugFuncs->ForceRefill();
        }
        // Do the cassette inventory
        if (longOp == LONG_OP_INVENTORY) {
          if (JEOLscope) {
            jcData.Init();
            lod->cartInfo->RemoveAll();
            name = lod->plugFuncs->GetCartridgeInfo(0, &idAtZero, &station, &slot,
              &rotation, &cartType);
            if (idAtZero > 0) {
              jcData.id = idAtZero;
              valid = name != NULL;
              jcData.name = valid ? name : "";
              jcData.slot = valid ? slot : 0;
              jcData.station = valid ? station: 0;
              jcData.rotation = valid ? rotation : 0;
              jcData.type = valid ? cartType : 0;
              lod->cartInfo->Add(jcData);
            }

            // For JEOL, get as much info as possible;
            for (ind = 1; ind <= lod->maxLoaderSlots; ind++) {
              try {
                name = mPlugFuncs->GetCartridgeInfo(ind, &jcData.id, &station, &slot,
                  &rotation, &cartType);
                if (name && name[0] != 0x00 && jcData.id > 0) {
                  jcData.station = station;
                  jcData.slot = slot;
                  jcData.rotation = rotation;
                  jcData.type = cartType;
                  jcData.name = name;
                  if (jcData.id == idAtZero)
                    lod->cartInfo->SetAt(0, jcData);
                  else
                    lod->cartInfo->Add(jcData);
                }
              }
              catch (_com_error E) {
              }
            }
          } else {
            lod->plugFuncs->DoCassetteInventory();
          }
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

        // Unload or load a cartridge
        if (longOp == LONG_OP_UNLOAD_CART || loadCart) {
          if (JEOLscope) {
            startTime = GetTickCount();
            while (SEMTickInterval(startTime) < 120000) {
              error = lod->plugFuncs->MoveCartridge(sCartridgeToLoad,
                loadCart ? JAL_STATION_STAGE : JAL_STATION_STORAGE, 0);
              if (error != 1)
                break;
              Sleep(5000);
            }
          } else {
            if (SEMGetSimulationMode() < 0) {
              if ((loadCart && sCartridgeToLoad == sSimLoadedCartridge) ||
                (!loadCart && sSimLoadedCartridge < 0))
                throw(_com_error((HRESULT)PLUGIN_FAKE_HRESULT, NULL, true));
              if (loadCart)
                sSimLoadedCartridge = sCartridgeToLoad;
              else
                sSimLoadedCartridge = -1;
              lod->plugFuncs->SetGunValve(false);

            } else {
              if (loadCart) {
                SEMTrace('W', "Calling to load cartridge %d", sCartridgeToLoad);
                lod->plugFuncs->LoadCartridge(sCartridgeToLoad);
              } else
                lod->plugFuncs->UnloadCartridge();
            }
          }
        }

        // Fill JEOL stage tank or transfer tank
        if (longOp == LONG_OP_FILL_STAGE || fillTransfer) {
          if (fillTransfer)
            fillVal = 2;
          if (longOp == LONG_OP_FILL_STAGE || longOp == LONG_OP_FILL_BOTH)
            fillVal++;
          error = lod->plugFuncs->RefillNitrogen(fillVal, 1);
          if (error == 2) {
            error = lod->plugFuncs->RefillNitrogen(fillVal, 0);
            lod->errString = "Timeout ";
            lod->errString += longOpDescription[longOp];
            lod->errString += " - stopped the filling";
          } else if (error > 2) {
            lod->errString.Format("Error starting refill for %s tank%s",
              B3DCHOICE(error == 5, "both", error == 3 ? "stage" : "transfer"),
              B3DCHOICE(error == 5, "s", 
                longOp == LONG_OP_FILL_BOTH ? " (the other one filled OK)" : ""));
            if (lod->plugFuncs->GetLastErrorString) {
              lod->errString += ": ";
              lod->errString += lod->plugFuncs->GetLastErrorString();
            }
          }
          if (error > 1) {
            error = 0;
            retval = 1;
          }
        }

        if (longOp == LONG_OP_FLASH_FEG) {
          error = lod->plugFuncs->FlashFEG((lod->flags & LONGOP_FLASH_HIGH) ? 1 : 0);
        }

        if (error) {
          hr = 0x80000000 | (B3DABS(error));
          descrip = longOpDescription[longOp];
          if (JEOLscope && longOp == LONG_OP_FLASH_FEG) {
            if (error > 1 && error < 7)
              descrip += jeolFEGmess[error - 2];
          } else if(lod->plugFuncs->GetLastErrorString) {
            descrip += ": ";
            descrip += lod->plugFuncs->GetLastErrorString();
          }
          SEMTestHResult(hr,  _T(descrip), &lod->errString, &error, true);
          retval = 1;
        } else
          lod->finished[ind] = (longOp != LONG_OP_REFILL && longOp != LONG_OP_FILL_BOTH) 
          || retval == 0;
      }

      // Save an error in the error string and retval but continue in loop
      catch (_com_error E) {
        descrip == ": ";
        if (lod->plugFuncs->GetLastErrorString && SEMGetSimulationMode() >= 0)
          descrip += lod->plugFuncs->GetLastErrorString();
        SEMReportCOMError(E, _T(longOpDescription[longOp] + descrip), &lod->errString,
          true);
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
  int errorOK[MAX_LONG_OPERATIONS] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const char *errStringsOK[MAX_LONG_OPERATIONS] = {"", "Cannot force refill", "", "", "",
    "", "", "", "", "", "", ""};
  JeolCartridgeData jcData;
  bool throwErr = false, didError;
  CString excuse;
  indStart = B3DCHOICE(index < 0, 0, sLongThreadMap[index]);
  indEnd = B3DCHOICE(index < 0, MAX_LONG_THREADS - 1, sLongThreadMap[index]);
  if (!mDoingLongOperation)
    return 0;
  for (thread = indStart; thread <= indEnd; thread++) {
    if (!mLongOpThreads[thread])
      continue;
    busy = UtilThreadBusy(&mLongOpThreads[thread]);
    if (busy <= 0) {
      for (op = 0; op < mLongOpData[thread].numOperations; op++) {
        longOp = mLongOpData[thread].operations[op];

        // First convert an error to success if the error is allowed and the error string
        // matches the allowed one; issue completion messages in either case
        if (busy < 0 && errorOK[longOp] && errStringsOK[longOp][0] != 0x00 &&
          mLongOpData[thread].errString.Find(errStringsOK[longOp]) >= 0) {
            mWinApp->AppendToLog("Call for " + CString(longOpDescription[longOp]) + 
              " probably finished successfully with misleading error message:\r\n     " + 
              mLongOpData[thread].errString);
            mLongOpData[thread].finished[op] = true;
            busy = 0;
        } else {
          if ((longOp == LONG_OP_REFILL || longOp == LONG_OP_FILL_STAGE || 
            longOp == LONG_OP_FILL_TRANSFER || longOp == LONG_OP_FILL_BOTH) && 
            !mLongOpData[thread].errString.IsEmpty()) 
          {
            mWinApp->AppendToLog("WARNING: " + mLongOpData[thread].errString);
            if (mLongOpData[thread].errString.Find("imeout") > 0)
              mLongOpErrorToReport = 1;
            if (mLongOpData[thread].errString.Find("starting") > 0) {
              if (mLongOpData[thread].errString.Find("both") > 0)
                mLongOpErrorToReport = 5;
              if (mLongOpData[thread].errString.Find("stage") > 0)
                mLongOpErrorToReport = 3;
              else if (mLongOpData[thread].errString.Find("transfer") > 0)
                mLongOpErrorToReport = 4;
            } else
              mLongOpErrorToReport = 2;
          }
          if (longOp == LONG_OP_INVENTORY && JEOLscope) {
            mChangedLoaderInfo = mJeolLoaderInfo.GetSize() ? -1 : 0;
          }

          // If moved a cartridge, change the table
          if (longOp == LONG_OP_LOAD_CART || longOp == LONG_OP_UNLOAD_CART) {
            if (busy && !mLongOpData[thread].finished[op] &&
              mWinApp->mMultiGridTasks->GetUnloadErrorOK() > 0)
              excuse = (longOp == LONG_OP_LOAD_CART) ?
              " (But cartridge is assumed to be on stage already)" :
              " (But it is assumed there was no cartridge loaded)";
            if (!busy || !excuse.IsEmpty()) {
              if (mUnloadedCartridge >= 0 && mUnloadedCartridge != mLoadedCartridge &&
                mUnloadedCartridge < (int)mJeolLoaderInfo.GetSize()) {
                jcData = mJeolLoaderInfo.GetAt(mUnloadedCartridge);
                jcData.station = JAL_STATION_STORAGE;
                mJeolLoaderInfo.SetAt(mUnloadedCartridge, jcData);
              }
              if (mLoadedCartridge >= 0 && 
                mLoadedCartridge < (int)mJeolLoaderInfo.GetSize()) {
                jcData = mJeolLoaderInfo.GetAt(mLoadedCartridge);
                jcData.station = JAL_STATION_STAGE;
                mJeolLoaderInfo.SetAt(mLoadedCartridge, jcData);
              }
              mChangedLoaderInfo = 1;
              if (mWinApp->mNavHelper->mMultiGridDlg)
                mWinApp->mNavHelper->mMultiGridDlg->NewGridOnStage(mLoadedCartridge);
              mWinApp->mMultiGridTasks->SetLoadedGridIsAligned(0);
            }
          }
          didError = busy && (!mLongOpData[thread].finished[op] || thread == 1);
          if (didError || mLongOpErrorToReport || !mWinApp->GetSuppressSomeMessages())
            mWinApp->AppendToLog("Call for " + CString(longOpDescription[longOp]) +
            B3DCHOICE(didError || mLongOpErrorToReport, " ended with error" + excuse, 
              " finished successfully"));
          if (!excuse.IsEmpty())
            busy = 0;
        }

        // Record time if completed, throw error if not and error not OK
        if (mLongOpData[thread].finished[op]) {
          if (longOp == LONG_OP_FLASH_FEG)
            mFegFlashCounter++;
          mLastLongOpTimes[longOp] = now;
          mWinApp->mDocWnd->SetShortTermNotSaved();
        } else if (busy < 0 && !errorOK[longOp] && !mLongOpErrorToReport)
          throwErr = true;
      }
    }
    if (busy > 0)
      retval = 1;
    if (busy < 0 && !retval)
      retval = -1;
    if (busy < 0)
      mWinApp->AppendToLog(mLongOpData[thread].errString);
  }

  // It looks fishy to do this after one thread errors if there is another still running,
  // but the system stays disabled anyway because the operation as whole is still busy
  if (throwErr)
    SEMErrorOccurred(1);

  // If all were checked and none are busy any more, clear flag and reenable interface
  // First rescan them all to make sure they are finished and give final error return
  if (index < 0 && retval <= 0) {
    if (!retval) {
      for (thread = indStart; thread <= indEnd; thread++) {
        if (mLongOpThreads[thread]) {
          for (op = 0; op < mLongOpData[thread].numOperations; op++) {
            longOp = mLongOpData[thread].operations[op];
            if (!mLongOpData[thread].finished[op] && !errorOK[longOp])
              retval = -1;
          }
        }
      }
    }

    mDoingLongOperation = false;
    mCamera->SetSuspendFilterUpdates(false);
    mWinApp->SetStatusText(mWinApp->mParticleTasks->GetDVDoingDewarVac() ?
      SIMPLE_PANE : MEDIUM_PANE, "");
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
  if (mDoingLongOperation && mDoingStoppableRefill && !exiting) {
    if (mDoingStoppableRefill & 1)
      mPlugFuncs->RefillNitrogen(1, 0);
    if (mDoingStoppableRefill & 2)
      mPlugFuncs->RefillNitrogen(2, 0);
    SEMMessageBox("If nitrogen was filling, it has now been stopped");
    return 0;
  }
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

// Starts a thread and waits for it complete, allowing messages to be processed
BOOL CEMscope::RunSynchronousThread(int action, int newIndex, int curIndex,
  const char *routine)
{
  int retval;
  double startTime;
  const char *statusText[] = {"CHANGING MAG", "CHANGING SPOT", "CHANGING PROBE",
    "CHANGING ALPHA", "NORMALIZING"};
  if (mSynchronousThread)
    return false;

  // Release the mutex if caller claimed it
  if (routine)
    CEMscope::ScopeMutexRelease(routine);

  // Fill the thread data.  Callers are responsible for setting normalize, ifSTEM, 
  // STEMmag, initialSleep and lowestM is relevant, and setting 
  // mLastNormalization and mProbeMode afterwards when appropriate
  mSynchroTD.actionType = action;
  mSynchroTD.newIndex = newIndex;
  mSynchroTD.curIndex = curIndex;
  mSynchroTD.normAllOnMagChange = mNormAllOnMagChange;
  mSynchroTD.lowestMModeMagInd = GetLowestMModeMagInd();
  mSynchroTD.lowestSecondaryMag = mLowestSecondaryMag;
  mSynchroTD.lowestMicroSTEMmag = mLowestMicroSTEMmag;
  mSynchroTD.HitachiMagFocusDelay = mHitachiMagFocusDelay;
  mSynchroTD.HitachiMagISDelay = mHitachiMagISDelay;
  mSynchroTD.HitachiSpotBeamWait = mHitachiSpotBeamWait;
  mSynchroTD.HitachiSpotStepDelay = mHitachiSpotStepDelay;

  startTime = GetTickCount();
  while (StageBusy() && SEMTickInterval(startTime) < 10000) {
    mWinApp->ManageBlinkingPane(GetTickCount());
    SleepMsg(100);
  }

  // Start the thread
  startTime = wallTime();
  int startCount = mAutosaveCount;
  mSynchronousThread = AfxBeginThread(SynchronousProc, &mSynchroTD,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  mSynchronousThread->m_bAutoDelete = false;
  mSynchronousThread->ResumeThread();

  // Unless already handled, set an action and update windows
  if (!mChangingLDArea) {
    mWinApp->UpdateBufferWindows();
    mWinApp->SetStatusText(SIMPLE_PANE, statusText[action]);
  }

  // Wait until done
  while (1) {
    retval = UtilThreadBusy(&mSynchronousThread);
    if (retval <= 0)
      break;
    mWinApp->ManageBlinkingPane(GetTickCount());
    SleepMsg(20);
  }

  // Restore windows
  if (!mChangingLDArea) {
    mWinApp->UpdateBufferWindows();
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }

  // Get mutex back if needed.  Return TRUE on success like other routines
  if (routine)
    CEMscope::ScopeMutexAcquire(routine, true);
  if (mWinApp->mCameraMacroTools.GetDeferredUserStop())
    mWinApp->mCameraMacroTools.DoUserStop();
  return retval == 0;
}

// The thread procedure for synchronous threads
// Get mutex, set up to do operations in thread, and dispatch to proper function
UINT CEMscope::SynchronousProc(LPVOID pParam)
{
  SynchroThreadData *sytd = (SynchroThreadData *)pParam;
  BOOL retval = true;
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CEMscope::ScopeMutexAcquire("SynchronousProc", true);

  if (FEIscope && mPlugFuncs->BeginThreadAccess(1, 
    PLUGFEI_MAKE_ILLUM | PLUGFEI_MAKE_PROJECT)) {
    SEMMessageBox("Error creating second instance of microscope object for synchronous "
      "thread");
    SEMErrorOccurred(1);
    retval = FALSE;
  }
  if (retval) {
    switch (sytd->actionType) {
    case SYNCHRO_DO_MAG:
      retval = SetMagKernel(sytd);
      break;
    case SYNCHRO_DO_SPOT:
      retval = SetSpotKernel(sytd);
      break;
    case SYNCHRO_DO_PROBE:
      retval = SetProbeKernel(sytd);
      break;
    case SYNCHRO_DO_ALPHA:
      retval = SetAlphaKernel(sytd);
      break;
    case SYNCHRO_DO_NORM:
      retval = NormalizeKernel(sytd);
      break;
    }
  }

  if (FEIscope) {
    mPlugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  CEMscope::ScopeMutexRelease("SynchronousProc");
  return retval ? 0 : 1;
}

// Message box on scope functions
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
  if (JEOLscope) {
    setOrClearFlags(mUpdateByEvent ? &mJeolSD.lowFlags : &mJeolSD.highFlags,
      JUPD_BEAM_STATE, newState ? 1 : 0);
  }
}

void CEMscope::SetHasSimpleOrigin(int inVal)
{
  B3DCLAMP(inVal, 0, 2);
  mHasSimpleOrigin = inVal;
  sSimpleOriginIndex = mHasSimpleOrigin - 1;
}

// Overloaded function returns number of spots in current mode for < 0, or for given mode
int CEMscope::GetNumSpotSizes(int ifSTEM)
{
  if (ifSTEM < 0)
    ifSTEM = mWinApp->GetSTEMMode() ? 1 : 0;
  return ifSTEM ? mNumSTEMSpotSizes : mNumSpotSizes;
}
