// CameraController.cpp:  Runs the camera and energy filter via SerialEMCCD
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
#include ".\CameraController.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "FocusManager.h"
#include "EMmontageController.h"
#include "TSController.h"
#include "ComplexTasks.h"
#include "ProcessImage.h"
#include "GainRefMaker.h"
#include "NavigatorDlg.h"
#include "BeamAssessor.h"
#include "MacroProcessor.h"
#include "FalconHelper.h"
#include "PluginManager.h"
#include "CalibCameraTiming.h"
#include "Utilities\XCorr.h"
#include "DirectElectron\DirectElectronCamera.h"
#include "DirectElectron\DirectElectronCamPanel.h"
#include "GatanSocket.h"
#include <mmsystem.h>
#include "Shared\b3dutil.h"
#include "Shared\SEMCCDDefines.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define FORCING_INCREMENT  0.0002
#define MAX_SCOPE_SETTLING 60000
#define DARK_REF_MEMORY 100000000
#define DM_ONE_DARK_REF_PER_BINNING 360
#define DM_K2_API_CHANGED_LOTS 40301
#define DM_RETURNS_DEFECT_LIST 40301
#define DM_OVW_DRIFT_CORRECT_OK 50001
#define DM_DS_CONTROLS_BEAM   40200
#define MAX_ESHIFT_TIMES  10
#define BLANKER_MUTEX_WAIT 50

// Indices to the three kinds of DM camera interfaces
#define COM_IND 0
#define SOCK_IND 1
#define AMT_IND 2

// Flags for specifying which camera interface to create
enum {CREATE_FOR_CURRENT, CREATE_FOR_GIF, CREATE_FOR_ANY};

// Static variables related to DM camera interfaces, used by macros and from threads
static int sSocketCurrent;         // Flag for whether current camera is socket
static int sAmtCurrent;            // Flag for whether current camera is AMT
static int sCreateFor = CREATE_FOR_CURRENT;  // Type of camera to create for
static int sNumCOMGatanCam = 0;
static int sGIFisSocket;             // 0 or 1 if GIF is on socket interface

// The test for whether to create a COM interface to Gatan camera
#define NEED_COM_DM_CAMERA (sCreateFor == CREATE_FOR_ANY && sNumCOMGatanCam) || \
  (sCreateFor == CREATE_FOR_CURRENT && !(sAmtCurrent || sSocketCurrent)) || \
  (sCreateFor == CREATE_FOR_GIF && !sGIFisSocket)

// Be careful to wrap any macro after an if statement in { }

// This macro can be used to create the camera interface from main routine or threads
// It assumes hr is already declared
#define CreateDMCamera(a) if (NEED_COM_DM_CAMERA) \
    hr = CoCreateInstance(__uuidof(DMCamera), 0, CLSCTX_LOCAL_SERVER, \
      __uuidof(IDMCamera), (void **)&a); \
  else \
    hr = S_OK;

// This is the corresponding macro for releasing the camera interface
#define ReleaseDMCamera(a) if (NEED_COM_DM_CAMERA) \
  a->Release();

// Macro for using the socket interface to call a function and throwing exception on error
#define CALL_SOCKET(c) if (CGatanSocket::##c) \
    throw(_com_error((HRESULT)SOCKET_FAKE_HRESULT, NULL, true))

// CallDMCamera: to call the current camera with supplied pointers
#define CallDMCamera(a,b,c) \
  if (sAmtCurrent) \
    b->c; \
  else if (sSocketCurrent) { \
    CALL_SOCKET(c); \
  } else \
    a->c;

// CallDMIndCamera: to call a camera on a specific interface with supplied pointers
#define CallDMIndCamera(a,b,c,d) \
  if (a == AMT_IND) \
    c->d; \
  else if (a == SOCK_IND) { \
    CALL_SOCKET(d); \
  } else \
    b->d;

// CallGatanCamera: to call a current Gatan camera using the supplied pointer
#define CallGatanCamera(a,c) \
  if (sSocketCurrent) { \
    CALL_SOCKET(c); \
  } else \
    a->c;

// CallDMIndGatan: to call a Gatan camera on a specific interface with supplied pointer
#define CallDMIndGatan(a,b,d) \
  if (a == SOCK_IND) { \
    CALL_SOCKET(d); \
  } else \
    b->d;

// MainCallDMCamera: to call the current camera from main
#define MainCallDMCamera(c) CallDMCamera(mGatanCamera, mTD.amtCam, c)

// MainCallDMIndCamera: to call a camera on a specific interface from main
#define MainCallDMIndCamera(a,c) CallDMIndCamera(a, mGatanCamera, mTD.amtCam, c)

// MainCallGatanCamera: to call the current Gatan camera from main
#define MainCallGatanCamera(c) CallGatanCamera(mGatanCamera, c)

// This gives the DM-type index for a given camera parameter set
#define CAMP_DM_INDEX(a) ((a)->AMTtype ? AMT_IND : ((a)->useSocket ? SOCK_IND : COM_IND))

#define FALCON_DIR_UNSET "NotSetByUser"

static int restrictedSizeIndex;       // Index to last selected restricted size

static const char *sGetCamObjText = "Object manager = CM_GetCameraManager()\n"
    "Object cameraList = CM_GetCameras(manager)\n"
    "Object camera = ObjectAt(cameraList,";


static VOID CALLBACK FilterUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, 
                                      DWORD dwTime);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCameraController::CCameraController()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mBufferManager = mWinApp->mBufferManager;
  mScope = mWinApp->mScope;
  mShiftManager = mWinApp->mShiftManager;
  mFalconHelper = mWinApp->mFalconHelper;
  mTD.JeolSD = &mScope->mJeolSD;
  mTD.scopePlugFuncs = NULL;
  mITD.td = &mTD;
  mImBufs = mWinApp->GetImBufs();
  mConSetsp = mWinApp->GetConSets();
  mAllParams = mWinApp->GetCamParams();
  mParam = mAllParams;
  mCamConSets = mWinApp->GetCamConSets();
  mActiveList = mWinApp->GetActiveCameraList();
  mScreenDownMode = false;
  mSimulationMode = false;
  mRaisingScreen = 0;
  mAcquiring = false;
  mInserting = false;
  mEnsuringDark = false;
  mHalting = false;
  mShotIncomplete = false;
  mDiscardImage = false;
  mNoMessageBoxOnError = 0;
  mStageQueued = false;
  mMagQueued = false;
  mISQueued = false;
  mDriftISQueued = false;
  mInsertThread = NULL;
  mAcquireThread = NULL;
  mEnsureThread = NULL;
  mLastConSet = -1;
  mUseCount = 1;
  mPending = -1;
  mRepFlag = -1;
  mSettling = -1;
  mPreDarkBump = false;
  for (int j = 0; j < MAX_CAMERAS; j++) {
    mPlugFuncs[j] = NULL;
    for (int i = 0; i < MAX_DARK_REFS; i++)
      mDarkRefs[j][i].UseCount = 0;
  }
  mRefArray.SetSize(0,5);
  OutOfSTEMUpdate();

  //mExtraBeamTime = 0.25f; // Amount of extra unblanked time
  mRequiredRoll = 0;
  mObeyTiltDelay = false;
  mDebugMode = false;
  mDivideBy2 = -1;    // -1 means not set by user settings, so property can have effect
  mRefMemoryLimit = DARK_REF_MEMORY;
  mConSetRefLimit = 5;
  mDarkAgeLimit = 3600.;
  mGainTimeLimit = 60.;
  mLastImageTime = 0.;
  mProcessHere = true;
  mScaledGainRefMax = 60000;
  mMinGainRefBits = 12;
  mMinBlankingTime = 0.;
  mGIFBiggestAperture = 4;
  mGIFslitInOutDelay = 18;
  mGIFoffsetDelayCrit = 40.;
  mGIFoffsetDelayBase1 = 10.;
  mGIFoffsetDelayBase2 = 30.;
  mGIFoffsetDelaySlope1 = 0.5f;
  mGIFoffsetDelaySlope2 = 0.25f;
  mNoSpectrumOffset = false;
  mIgnoreFilterDiffs = false;
  mCOMBusy = false;
  mFilterUpdateID = 0;
  mWasSpectroscopy = false;
  mBackToImagingTime = 0;
  mTD.Camera = -1;
  mTD.lastTietzSettling = 0.;
  mTD.blankerMutexHandle = CreateMutex(0, 0, 0);
  mTD.blankerThread = NULL;
  mTD.startingFEIshutter = VALUE_NOT_SET;
  mNumZLPAlignChanges = 3;
  mMinZLPAlignInterval = 6.;   // 3/13/17: Quantum K2 on Krios had some times just above 5
  mShiftTimeIndex = -1;
  mBlankWhenRetracting = false;
  for (int k = 0; k < 3; k++) {
    mNumDMCameras[k] = 0;
    mDMInitialized[k] = false;
  }
  mNumTietzCameras = 0;
  mNumDECameras = 0;
  mTietzInitialized = false;
  mFEIinitialized = false;
	mDEInitialized = false;
  mPlugInitialized = false;
  mOtherCamerasInTIA = false;
  mTD.DE_Cam = NULL;
  mTD.errFlag = 0;
  mTD.NumChannels = 1;
  mTietzFilmSwitchTime = 2.0;
  mCameraWithBeamOn = 0;
  mTietzInstances = false;
  mShutterInstance = false;
  mAMTactive = false;
  mBeamWidth = 0.;
  mScanSleep = 10;
  mScanMargin = 100;
  mNumAverageDark = 0;
  mDarkSum = NULL;
  mTimeoutFactor = 1.;
  mDriftISinterval = 100;
  mInitialDriftSteps = 2.0;
  // Disable retries: menu actions and message boxes can mess it up
  // Would need to institute a mutexed flag to avoid timeouts in such cases
  mRetryLimit = 0; 
  mInterpDarkRefs = false;
  mMinInterpRefExp = 0.01f;
  mInterpRefInterval = 0.2f;
  mBlockOffsetSign = -1;      // Don't understand why, but this sign is needed for F416
  mOppositeAreaNextShot = false;
  mDynFocusInterval = -1;
  mStartDynFocusDelay = -1;
  mTiltDuringShotDelay = -1;
  mStartedScanning = false;
  mScreenUpSTEMdelay = 7000;
  mLowerScreenForSTEM = 1;
  mRamperInstance = false;
  mBlankNextShot = false;
  mDEserverRefNextShot = 0;
  mTD.ContinuousSTEM = 0;
  mDSextraShotDelay = 100;
  mDSshouldFlip = -1;         // Will be 0 or 1 if read in
  mDSglobalRotOffset = 0.;    // Won't be tested unless flip is read in too
  mShiftedISforSTEM = false;
  mSubareaShiftDelay = 200;
  mAdjustmentShiftDelay = 20;
  mPostSTEMmagChgDelay = 300;
  mDSsyncMargin = 10.;
  mDScontrolBeam = 0;
  mInvertBrightField = 1;    // Default is to do what the button says
  mInsertDetShotTime = 3.;
  mMaxChannelsToGet = MAX_STEM_CHANNELS;
  mMagToRestore = 0;
  mRetainMagAndShift = -1;
  mAdjustShiftX = mAdjustShiftY = 0.;
  mMakeFEIerrorBeTimeout = false;
  mNumK2Filters = 1;
  mK2FilterNames[0] = "None";
  mAntialiasBinning = true;

  // These are initialized below in Initialize() after determining if base or summit
  mMinK2FrameTime = -1.;
  mK2ReadoutInterval = -1.;
  mK2BaseModeScaling = -1.;

  // Set these to values in simple DM user interface except for first incrment
  mOneViewMinExposure[0] = 0.04f;
  mOneViewDeltaExposure[0] = 0.01f;
  mOneViewDeltaExposure[1] = mOneViewMinExposure[1] = 0.01f;
  mOneViewDeltaExposure[2] = mOneViewMinExposure[2] = 0.005f;
  mOneViewDeltaExposure[3] = mOneViewMinExposure[3] = 0.005f;
  for (int l = 4; l < MAX_BINNINGS; l++) {
    mOneViewDeltaExposure[l] = 0.;
    mOneViewMinExposure[l] = 0.;
  }
  mFalconReadoutInterval = 0.055771f;
  mMaxFalconFrames = 7;
  mFalcon3AlignFraction = 6;
  mMinAlignFractionsLinear = 8;
  mMinAlignFractionsCounting = 32;
  mFrameSavingEnabled = false;
  mCanUseFalconConfig = -1;
  mRestoreFalconConfig = false;
  mDeferStackingFrames = false;
  mStackingWasDeferred = false;
  mStartedFalconAlign = false;
  mStartedExtraForDEalign = false;
  mFrameMdocForFalcon = 0;
  mZoomFilterType = 5;
  mOneK2FramePerFile = false;
  mDefaultCountScaling = 30.f;
  mScalingForK2Counts = 0.;
  mDirForK2Frames = "";
  mDirForFalconFrames = FALCON_DIR_UNSET;
  mDESetNameTimeoutUsed = 0.;
  mDEPrevSetNameTimeout = 5.;
  mSkipNextReblank = false;
  mDefaultGIFCamera = -1;
  mDefaultRegularCamera = -1;
  mBaseK2CountingTime = 0.1f;
  mBaseK2SuperResTime = 0.5f;
  mNoK2SaveFolderBrowse = false;
  mRunCommandAfterSave = false;
  mSaveRawPacked = -1;
  mSaveTimes100 = 0;
  mSaveUnnormalizedFrames = false;
  mK2SaveAsTiff = 0;
  mSkipK2FrameRotFlip = false;
  mNameFramesAsMRCS = false;
  mK2SynchronousSaving = false;
  mFocusStepToDo1 = 0.;
  mFocusStepToDo2 = 0.;
  mLDwasSetToArea = -1;
  mSmoothFocusExtraTime = 400;
  mInDisplayNewImage = false;
  mNeedToSelectDM = false;
  mNextAsyncSumFrames = -1;
  mLastAsyncTimeout = 0;
  mK2MaxRamStackGB = 12.;
  mK2MinFrameForRAM = 0.05f;
  mBaseJeolSTEMflags = 0;
  mFalconAsyncStacking = true;
  mWaitingForStacking = 0;
  mCancelNextContinuous = false;
  mSingleContModeUsed = SINGLE_FRAME;
  mTaskFrameWaitStart = -1.;
  mPreventUserToggle = 0;
  mContinuousDelayFrac = 0.;
  mStoppedContinuous = false;
  mFrameNameFormat = FRAME_FILE_MONTHDAY | FRAME_FILE_HOUR_MIN_SEC;
  mFrameNumberStart = 0;
  mLastUsedFrameNumber = -1;
  mLastFrameNumberStart = -1;
  mDigitsForNumberedFrame = 3;
  mK2GainRefTimeStamp[0][0] = mK2GainRefTimeStamp[0][1] = -1;
  mK2GainRefTimeStamp[1][0] = mK2GainRefTimeStamp[1][1] = -1;
  mK2GainRefLifetime = 10;
  mNoNormOfDSdoseFrac = false;
  mK2MinStartupDelay = 3.0;
  mDeferSumOnNextAsync = false;
  mAskedDeferredSum = false;
  mStartingDeferredSum = false;
  mConsDeferred = NULL;
  mBufDeferred = NULL;
  mExtraDeferred = NULL;
  mUseGPUforK2Align[0] = mUseGPUforK2Align[1] = false;
  mAlignWholeSeriesInIMOD = false;
  mNumFrameAliLogLines = 2;
  mDarkMaxMeanCrit = 0.;
  mDarkMaxSDcrit = 0.;
  mBadDarkNumRetries = 2;
  mAllowSpectroscopyImages = false;
  mASIgivesGainNormOnly = true;
}

// Set a frame align param to default values
void CCameraController::SetFrameAliDefaults(FrameAliParams &faParam, const char *name,
  int binning, float filt1, int sizeRestrict)
{
  faParam.aliBinning = binning;
  faParam.name = name;
  faParam.targetSize = 1000;
  faParam.binToTarget = 1;
  faParam.strategy = FRAMEALI_PAIRWISE_NUM;
  faParam.numAllVsAll = 7;
  faParam.rad2Filt1 = filt1;
  faParam.rad2Filt2 = 0.f;
  faParam.rad2Filt3 = 0.;
  faParam.rad2Filt4 = 0.;
  faParam.hybridShifts = false;
  faParam.sigmaRatio = 1.f / 7.f;
  faParam.refRadius2 = 0.;
  faParam.doRefine = false;
  faParam.refineIter = 5;
  faParam.useGroups = false;
  faParam.groupSize = 2;
  faParam.doSmooth = false;
  faParam.smoothThresh = 15;
  faParam.shiftLimit = 20;
  faParam.truncate = false;
  faParam.truncLimit = 0.;
  faParam.groupRefine = false;
  faParam.stopIterBelow = 0.1f;
  faParam.antialiasType = 4;
  faParam.keepPrecision = false;
  faParam.outputFloatSums = false;
  faParam.alignSubset = false;
  faParam.subsetStart = 1;
  faParam.subsetEnd = 20;
  faParam.sizeRestriction = sizeRestrict;
  faParam.whereRestriction = 0;
}

CCameraController::~CCameraController()
{
  if (mDMInitialized[AMT_IND])
    mTD.amtCam->Release();
  DeleteReferences(DARK_REFERENCE, false);
  DeleteReferences(GAIN_REFERENCE, false);
  if (mFilterUpdateID)
    ::KillTimer(NULL, mFilterUpdateID);
  for (int i = 0; i < 3; i++)
  mDMInitialized[i] = false;
  mTietzInitialized = false;
  mFEIinitialized = false;
  if (mRamperInstance)
    mTD.FocusRamper = NULL;
  
  delete mConsDeferred;
  delete mBufDeferred;
  delete mExtraDeferred;
}

// Shut down timer to prevent updates
void CCameraController::KillUpdateTimer()
{
  if (mFilterUpdateID)
    ::KillTimer(NULL, mFilterUpdateID);
  mFilterUpdateID = 0;

  //Uninit any DE cameras that were initialized to begin with.
  // DNM: Let the destructor uninitialize it once
  // Now that it is a plugin, it needs to uninitialize here before the destructors
  if (mTD.DE_Cam)
    delete mTD.DE_Cam;
  mTD.DE_Cam = NULL;
	mDEInitialized = false;
}

///////////////////////////////////////////////////////
//  Initialization
///////////////////////////////////////////////////////

// This routine gets an instance of the Gatan camera COM interface if called for by the
// argument specifying type to create for
// Returns true if there is an error, sets a flag that there is a connection if not
// It should be called only for a DM camera type, from main thread
BOOL CCameraController::CreateCamera(int createFor, bool popupError)
{
  BOOL bRtn;
  HRESULT hr;
  sCreateFor = createFor;
  CreateDMCamera(mGatanCamera);
  if (popupError)
    bRtn = SEMTestHResult(hr, "getting instance of camera ");
  else 
    bRtn = !SUCCEEDED(hr);
  mCOMBusy = !bRtn;
  sCreateFor = CREATE_FOR_CURRENT;
  return bRtn;
}

// This routine releases the instance if one was created
void CCameraController::ReleaseCamera(int createFor)
{
  sCreateFor = createFor;
  ReleaseDMCamera(mGatanCamera);
  sCreateFor = CREATE_FOR_CURRENT;
  mCOMBusy = false;
}

// This routine makes the initial contact with the camera(s).
// It calls a separate routine for each type of camera
// It returns 1 if there is not a successful instance creation or -1 for fatal error
int CCameraController::Initialize(int whichCameras)
{
  CString report;
  int *originalList = mWinApp->GetOriginalActiveList();
  int numOrig = mWinApp->GetActiveCamListSize();
  int indPresent = 0, FEIstem = 0;
  int indMissing = numOrig - 1;
  int numGatanListed = 0, numTietzListed = 0, numFEIlisted = 0, numPlugListed = 0;
  int numDMListed[3], digiscan[3], numAdd[3];
  int numDEListed = 0;
  double addedFlyback;

  int i, ind, err, DMind;
  long num;
  BOOL anyGIF = false;
  BOOL anyPreExp = false;
  int anyK2Type = 0;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  if (mWinApp->GetNoCameras())
    return 1;
  if (mFalconFrameConfig.IsEmpty())
    mMaxFalconFrames = 0;
  
  CameraParameters *camP = &mAllParams[mWinApp->GetCurrentCamera()];
  if (mDynFocusInterval < 0)
    mDynFocusInterval = FEIscope ? 40 : 80;
  if (mStartDynFocusDelay < 0)
    mStartDynFocusDelay = FEIscope ? 100 : 250;
  if (mDivideBy2 < 0)
    mDivideBy2 = 0;
  if (mSaveRawPacked < 0)
    mSaveRawPacked = 0;
  for (i = 0; i < 3; i++) {
    digiscan[i] = 0;
    numDMListed[i] = 0;
    numAdd[i] = 0;
    mNeedsReadMode[i] = false;
  }

  // If we are coming in here because something about Tietz cameras is not ready,
  // then try to relock and reinitialize if necessary, and in any case return if the
  // desired camera failed to initialize now or earlier
  if (whichCameras == INIT_CURRENT_CAMERA && camP->TietzType && mTietzInitialized) {
    if (LockInitializeTietz(false))
      return 1;
    if (camP->failedToInitialize)
      return 1;
    return 0;
  }
  
  // Count up cameras of each type listed, clear retractability of Tietz cameras
  // FEI retractability now cleared in initialize function
  for (i = 0; i < numOrig; i++) {
    ind = originalList[i];
    if (mAllParams[ind].TietzType) {
      numTietzListed++;
      if (mAllParams[ind].TietzCanPreExpose)
        anyPreExp = true;
      mAllParams[ind].retractable = false;
      mAllParams[ind].pluginName = "";
    } else if (mAllParams[ind].FEItype) {
      numFEIlisted++;
      if (mAllParams[ind].STEMcamera)
        FEIstem = 1;
    } else if (mAllParams[ind].DE_camType)
	  	numDEListed++;
    else if (!mAllParams[ind].pluginName.IsEmpty())
      numPlugListed++;
    else if (mAllParams[ind].AMTtype)
      numDMListed[AMT_IND]++;
    else {
      DMind = mAllParams[ind].useSocket ? SOCK_IND : COM_IND;
      numDMListed[DMind]++;
      if (mAllParams[ind].STEMcamera) {
        mAllParams[ind].retractable = false;
        digiscan[DMind] = 1;
        addedFlyback = mAllParams[ind].addedFlyback;
      }
      if (mAllParams[ind].GIF)
        sGIFisSocket = DMind;
      if (mAllParams[ind].K2Type) {
        mNeedsReadMode[DMind] = true;
        anyK2Type = mAllParams[ind].K2Type;
      }
      if (mAllParams[ind].OneViewType)
        mNeedsReadMode[DMind] = true;
    }
    if (mAllParams[ind].GIF)
      anyGIF = true;
  }
  if (numFEIlisted - FEIstem > 1)
    mOtherCamerasInTIA = true;

  // Delayed initialization of K2 parameters depending on base or summit if not entered
  if (mMinK2FrameTime < 0) 
    mMinK2FrameTime = anyK2Type > 1 ? 0.1138f : 0.025f;
  if (mK2ReadoutInterval < 0)
    mK2ReadoutInterval = anyK2Type > 1 ? 0.1138f : 0.0025f;
  if (mK2BaseModeScaling < 0)
    mK2BaseModeScaling = 0.25f;


  // Try to initialize DM type cameras if any are listed and not initialized yet
  for (DMind = 0; DMind < 3; DMind++) {
    if (numDMListed[DMind] && !mDMInitialized[DMind] && (whichCameras == INIT_ALL_CAMERAS 
      || (whichCameras == INIT_GIF_CAMERA && DMind == sGIFisSocket) || 
      (whichCameras == INIT_CURRENT_CAMERA && camP->DMCamera &&
      DMind == CAMP_DM_INDEX(camP) ))) {
        InitializeDMcameras(DMind, numDMListed, originalList, numOrig, anyGIF, digiscan,
          addedFlyback);
    }
  }
  sNumCOMGatanCam = mNumDMCameras[COM_IND];

  // For omega filter, do filter setup here
  if (mScope->GetHasOmegaFilter() && mScope->GetInitialized() && SetupFilter())
     AfxMessageBox("An error occurred trying to initialize filter parameters.",
       MB_EXCLAME);
  
  // Create Tietz interface if necessary
  if (numTietzListed && !mTietzInitialized && (whichCameras == INIT_ALL_CAMERAS ||
    whichCameras == INIT_TIETZ_CAMERA || 
    (whichCameras == INIT_CURRENT_CAMERA && camP->TietzType))) {
      err = InitializeTietz(whichCameras, originalList, numOrig, anyPreExp);
      if (err && whichCameras != INIT_ALL_CAMERAS)
        return err;
  }
    
  // Create FEI interface if necessary
  if (numFEIlisted && !mFEIinitialized && (whichCameras == INIT_ALL_CAMERAS ||
    (whichCameras == INIT_CURRENT_CAMERA && camP->FEItype)))
    InitializeFEIcameras(numFEIlisted, originalList, numOrig);

  // Initialize DE cameras
  if (numDEListed && !mDEInitialized && (whichCameras == INIT_ALL_CAMERAS ||
    (whichCameras == INIT_CURRENT_CAMERA && camP->DE_camType)))
    InitializeDirectElectron(originalList, numOrig);

  // Initialize plugin cameras
  if (numPlugListed && !mPlugInitialized && (whichCameras == INIT_ALL_CAMERAS ||
    (whichCameras == INIT_CURRENT_CAMERA && !camP->pluginName.IsEmpty())))
    InitializePluginCameras(numPlugListed, originalList, numOrig);

  // Build the active camera list
  for (ind = 0; ind < numOrig; ind++) {
    i = originalList[ind];
    if (mAllParams[i].DMCamera && !mAllParams[i].STEMcamera) {
      DMind = CAMP_DM_INDEX(&mAllParams[i]);
      if (numAdd[DMind] < mNumDMCameras[DMind] - digiscan[DMind]) {
        mActiveList[indPresent++] = i;
        numAdd[DMind]++;
      } else
        mActiveList[indMissing--] = i;
    } else {
      if (!(mAllParams[i].failedToInitialize || 
        (mAllParams[i].TietzType && !mTietzInstances)))
        mActiveList[indPresent++] = i;
      else
        mActiveList[indMissing--] = i;
    }
  }
  
  // Find index of first regular and GIF cameras in new list
  filtParam->firstGIFCamera = -1;
  filtParam->firstRegularCamera = -1;
  for (ind = 0; ind < numOrig; ind++) {
    if (mAllParams[mActiveList[ind]].GIF && (filtParam->firstGIFCamera < 0 || 
      ind == mDefaultGIFCamera))
      filtParam->firstGIFCamera = ind;
    if (!mAllParams[mActiveList[ind]].GIF && !mAllParams[mActiveList[ind]].STEMcamera &&
      (filtParam->firstRegularCamera < 0 || ind == mDefaultRegularCamera))
      filtParam->firstRegularCamera = ind;
    if (mAllParams[mActiveList[ind]].STEMcamera && mWinApp->GetFirstSTEMcamera() < 0)
      mWinApp->SetFirstSTEMcamera(ind);
  }
  
  // Put any other cameras read in on the end of the active list past missing cameras
  for (ind = 0; ind < MAX_CAMERAS; ind++) {
    if (mAllParams[ind].name.Find("Default Camera") >= 0)
      continue;
    i = -1;
    for (num = 0; num < numOrig; num++) {
      if (mActiveList[num] == ind)
        i = num;
    }
    if (i < 0)
      mActiveList[numOrig++] = ind;
  }

  // Finally set number of active cameras
  err = mNumDMCameras[0] + mNumDMCameras[1] + mNumDMCameras[2]
    + mNumTietzCameras + numFEIlisted + mNumDECameras + numPlugListed;
  if (!err) {
    mWinApp->SetNoCameras(true);
    mSimulationMode = true;
  }
  mWinApp->SetNumberOfActiveCameras(err, numOrig);
  return 0;
}

// Initialize DMcamera-type interface for index DMind
void CCameraController::InitializeDMcameras(int DMind, int *numDMListed, 
                                            int *originalList, int numOrig, BOOL anyGIF,
                                            int *digiscan, double addedFlyback)
{
  HRESULT hr;
  CString report;
  double flyback, rotOffset;
  int i, ind, div, sel, nlist, needVers = SEMCCD_VERSION_OK_NO_K2;
  long num, allnum, doFlip, available;
  int gatanSelectList[2 * MAX_IGNORE_GATAN];
  bool anyK2 = false, gotDefectList;

  if (DMind == AMT_IND) {
    hr = CoCreateInstance(__uuidof(AMTCamInterface), 0, CLSCTX_INPROC_SERVER,
      __uuidof(IAMTCamInterface), (void **)&mTD.amtCam);
    report = "while connecting to AMT camera.";
  } else if (DMind == COM_IND) {
    CreateDMCamera(mGatanCamera);
    report = "while connecting to Gatan Camera.\nIs DigitalMicrograph running?";
  } else {
    hr = mWinApp->mGatanSocket->Initialize() ? E_FAIL : S_OK;
    report = "while connecting to Gatan camera via socket.\nIs DigitalMicrograph "
      "running on the server?";
  }

  if(!SEMTestHResult(hr, report)) {
    try {
      long version, plugVersion;
      long canSelectShutter, canSetSettling, openShutterWorks;
      mPluginVersion[DMind] = 0;
      MainCallDMIndCamera(DMind, GetNumberOfCameras(&allnum));
      MainCallDMIndCamera(DMind, SetDebugMode(mDebugMode));

      // The plugin knows the DM version, so get it from there.  And AMT has a version too
      MainCallDMIndCamera(DMind, GetDMVersion(&version));
      mDMversion[DMind] = version;

      if (DMind != AMT_IND) {

        // See if latest version is required instead of what is OK if no K2
        for (ind = 0, num = 0; ind < numOrig; ind++) {
          i = originalList[ind];
          if (mAllParams[i].GatanCam && mAllParams[i].K2Type && 
            DMind == CAMP_DM_INDEX(&mAllParams[i]))
            needVers = SEMCCD_PLUGIN_VERSION;
        }

        mNewFunctionCalled = "a new function to get the plugin version";
        try {
          CallDMIndGatan(DMind, mGatanCamera, GetPluginVersion(&plugVersion));
          mPluginVersion[DMind] = plugVersion;
          if (plugVersion < needVers) {
            if (DMind == SOCK_IND && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID))
              report.Format("The SerialEMCCD plugin to DigitalMicrograph on the other "
              "computer\nis version %d and should be upgraded to the current version (%d)"
              "\n\nCopy the appropriate file from a current SerialEM installation package"
              "\n(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_3-x-x...)\n" 
              "to the Gatan Plugins folder on the other computer and replace\n"
              "the version that is currently there (exit DM first)",
              plugVersion, SEMCCD_PLUGIN_VERSION);
            else
              report.Format("The SerialEMCCD plugin to DigitalMicrograph on this computer"
              "\nis version %d and should be upgraded to the current version (%d)\n\n"
              "This is usually accomplished by exiting DM and running\n"
              "install.bat in a current SerialEM installation package\n"
              "(normally in a folder C:\\Program Files\\SerialEM\\SerialEM_3-x-x...)", 
              plugVersion, SEMCCD_PLUGIN_VERSION);
            AfxMessageBox(report, MB_ICONINFORMATION | MB_OK);
          }
        }
        catch (...) {
        }
        mNewFunctionCalled = "";

        if (digiscan[DMind]) {
          CallDMIndGatan(DMind, mGatanCamera, GetDSProperties(mDSextraShotDelay, 
            addedFlyback, mDSsyncMargin, &flyback, &mDSlineFreq, &rotOffset, &doFlip));
          if (mDSshouldFlip >= 0) {
            if ((mDSshouldFlip ? 1 : 0) != doFlip) {
              report.Format("Digiscan is%s set up to Flip Horizontal Scan and it is%s "
                "supposed to.\n\nYou should fix this in the DigiScan Setup Advanced "
                "(Configuration) dialog.", doFlip ? "" : " not", doFlip ? " not" : "");
              AfxMessageBox(report, MB_EXCLAME);
            }
            if (fabs(mDSglobalRotOffset - rotOffset) > 0.01) {
              report.Format("DigiScan is set up with a Rotation Offset of %.2f and it "
                "is supposed to be %.2f.\n\nYou should fix this in the DigiScan Setup"
                " Advanced (Configuration) dialog.", rotOffset, mDSglobalRotOffset);
              AfxMessageBox(report, MB_EXCLAME);
            }
          }
          allnum++;
        }

        // Get the shuttering capabilities and AND this with the current properties
        // for all cameras on this interface
        MainCallDMIndCamera(DMind, GetDMCapabilities(&canSelectShutter, &canSetSettling, 
          &openShutterWorks));
        for (i = 0; i < MAX_CAMERAS; i++) {
          if (mAllParams[i].GatanCam && DMind == CAMP_DM_INDEX(&mAllParams[i])) {
            mAllParams[i].DMbeamShutterOK = 
              mAllParams[i].DMbeamShutterOK && canSelectShutter;
            mAllParams[i].DMsettlingOK = 
              mAllParams[i].DMsettlingOK && canSetSettling;
            mAllParams[i].DMopenShutterOK = 
              mAllParams[i].DMopenShutterOK && openShutterWorks;
          }
        }
      }

      // Count number not to ignore and build mapping to actual camera number, don't
      // let them ignore the last one if its DigiScan
      num = 0;
      for (i = 0; i < allnum; i++)
        if (!numberInList(i, mIgnoreDMList[DMind], mNumIgnoreDM[DMind], 0) || 
          i == allnum - digiscan[DMind])
          gatanSelectList[num++] = i;

      if (num < 1)
        gatanSelectList[num++] = 0;

      if (num != numDMListed[DMind]) {
        if (num > numDMListed[DMind])
          report.Format("There are %d %s%s cameras available but only %d specified"
          "\nin the ActiveCameraList from the SerialEMproperties file.\n\n"
          "You will not be able to access the last %d cameras.", 
          num, DMind == SOCK_IND ? "socket server " : "", 
          DMind == AMT_IND ? "AMT" : "Gatan", numDMListed[DMind], 
          num - numDMListed[DMind]);
        else if (DMind == AMT_IND)
          report.Format("There are only %d AMT camera(s) available while %d are "
          "specified\nin the ActiveCameraList from the SerialEMproperties file.",
          num, numDMListed[DMind]);
        else
          report.Format("There are only %d %sGatan camera(s) available while %d are"
          " specified\nin the ActiveCameraList from the SerialEMproperties file."
          "\n\nIf a camera is turned off, you should turn it on,\n"
          "then restart DigitalMicrograph and SerialEM.\n\n"
          "If a camera is unavailable, you should edit the properties\n"
          "file to list just the available cameras, then restart SerialEM.\n\n",
          num, DMind == SOCK_IND ? "socket server " : "", numDMListed[DMind]);
        AfxMessageBox(report, MB_EXCLAME);
      }
      nlist= B3DMIN(num, numDMListed[DMind]);
      mNumDMCameras[DMind] = nlist;

      // Go through the cameras on the interface in order by the active list and
      // assign camera numbers from this select list
      // Set shutter configuration for Gatan if flag set for this
      // It's possible for nothing but DigiScan to be left so stop treating them as
      // regular cameras when num reaches 1 less than the limit
      for (ind = 0, num = 0; ind < numOrig && num < nlist; ind++) {
        i = originalList[ind];
        if (mAllParams[i].DMCamera && !mAllParams[i].STEMcamera && 
          DMind == CAMP_DM_INDEX(&mAllParams[i]) && num < nlist - digiscan[DMind]) {
            sel = gatanSelectList[num];
            mAllParams[i].cameraNumber = sel;
            if (mAllParams[i].GatanCam) {
              if (mAllParams[i].setAltShutterNC)
                MainCallDMIndCamera(DMind, SetShutterNormallyClosed(sel, 
                (long)mAllParams[i].beamShutter));

              // Do not try to set or clear DM settling for Faux camera
              if (mAllParams[i].name.Find("Faux") >= 0)
                MainCallDMIndCamera(DMind, SetNoDMSettling(sel));

              // Set up defaults for continuous mode before checking size so boundary
              // direction can be gotten in size check if needed
              if (version < 40000)
                mAllParams[i].useFastAcquireObject = false;
              if (mAllParams[i].useContinuousMode < 0) {
                if (mAllParams[i].K2Type || mAllParams[i].OneViewType) {
                  mAllParams[i].useContinuousMode = 1;
                  mAllParams[i].setContinuousReadout = true;
                } else if (mAllParams[i].DMRefName.Find("US4000") >= 0) {
                  mAllParams[i].useContinuousMode = 1;
                } else if (mAllParams[i].DMRefName.Find("US1000") >= 0) {
                  mAllParams[i].useContinuousMode = 1;
                } else if (mAllParams[i].DMRefName.Find("Orius") >= 0) {
                  mAllParams[i].useContinuousMode = 1;
                  mAllParams[i].setContinuousReadout = true;
                  mAllParams[i].continuousQuality = 1;
                  mAllParams[i].balanceHalves = 1;
                  mAllParams[i].fourPort = true;
                }
              }

              // Default post-actions disabled for OneView
              if (mAllParams[i].OneViewType && mAllParams[i].postActionsOK < 0)
                mAllParams[i].postActionsOK = 0;

              // Check the size
              CheckGatanSize(sel, i);

              // Set up default boundary for Orius if needed
              if (mAllParams[i].useContinuousMode > 0 && mAllParams[i].balanceHalves > 0
                && mAllParams[i].halfBoundary < 0) {
                  if (mAllParams[i].ifHorizontalBoundary)
                    mAllParams[i].halfBoundary = mAllParams[i].sizeY / 2;
                  else
                    mAllParams[i].halfBoundary = mAllParams[i].sizeX / 2;
              }

              // For a K2 and GMS 2.31, get the defect list
              gotDefectList = false;
              if (mAllParams[i].K2Type && !mAllParams[i].defects.wasScaled && 
                version >= DM_RETURNS_DEFECT_LIST) {
                MainCallDMIndCamera(DMind, selectCamera(sel));
                GetMergeK2DefectList(DMind, &mAllParams[i], false);
                gotDefectList = true;
                mWinApp->mGainRefMaker->IsDMReferenceNew(i);
              }

              if (mAllParams[i].K2Type && mAllParams[i].countingRefForK2.IsEmpty()) {
                if (!mCountingRef.IsEmpty())
                  mAllParams[i].countingRefForK2 = mCountingRef;
                else
                  mAllParams[i].countingRefForK2 = MakeFullDMRefName(&mAllParams[i], 
                    ".m2.");
              }
              if (mAllParams[i].K2Type && mAllParams[i].superResRefForK2.IsEmpty()) {
                if (!mSuperResRef.IsEmpty())
                  mAllParams[i].superResRefForK2 = mSuperResRef;
                else
                  mAllParams[i].superResRefForK2 = MakeFullDMRefName(&mAllParams[i], 
                    ".m3.");
              }

              if (mAllParams[i].OneViewType && 
                mPluginVersion[DMind] < PLUGIN_CAN_MAKE_SUBAREA) {
                  mAllParams[i].subareasBad = 2;
                  mAllParams[i].moduloX = -2;
              }

              // For all Gatan cameras, identify bad pixels that touch rows/columns
              if (!gotDefectList) {
                div = BinDivisorI(&mAllParams[i]);
                CorDefFindTouchingPixels(mAllParams[i].defects, mAllParams[i].sizeX / div,
                  mAllParams[i].sizeY / div, 0);

                // For K2, scale the defects up and mark as K2
                if (mAllParams[i].K2Type && !mAllParams[i].defects.wasScaled) {
                  CorDefScaleDefectsForK2(&mAllParams[i].defects, false);
                  mAllParams[i].defects.K2Type = 1;
                }
              }

              if (mAllParams[i].K2Type)
                anyK2 = true;
            }
            num++;
        }
        if (mAllParams[i].GatanCam && mAllParams[i].STEMcamera) {
          mAllParams[i].basicFlyback = (float)flyback;
          mAllParams[i].flyback = (float)flyback + mAllParams[i].addedFlyback;
        }
      }

      if (anyK2 && mPluginVersion[DMind] >= PLUGIN_CAN_ALIGN_FRAMES) {
        CallDMIndGatan(DMind, mGatanCamera, IsGpuAvailable(0, &available, 
          &mGpuMemory[DMind]));
        SEMTrace('1', "GPU %s available for K2 aligning", available ? "IS" : "IS NOT");
      }


    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T(DMind == AMT_IND ? "getting initial information "
        "about AMT cameras " : "getting initial information about Gatan cameras "));
    }

    mDMInitialized[DMind] = true;
    if (DMind == COM_IND)
      mGatanCamera->Release();

    // If there is a GIF camera, first enforce the program state on the filter
    // then start an update timer to collect the state as it gets changed by other
    // interfaces
    if (anyGIF && DMind == sGIFisSocket) {
      if (SetupFilter())
        AfxMessageBox("An error occurred trying to initialize filter parameters.\n\n"
        "You probably need to start Filter Control and restart Digital Micrograph\n"
        "if you want to use the energy filter", MB_EXCLAME);

      mFilterUpdateID = ::SetTimer(NULL, 1, 500, FilterUpdateProc);
    }
  }

  // Even if there was a total failure, we need to say one was on the active list
  // so that reinitialization can occur
  if (!mNumDMCameras[DMind])
    mNumDMCameras[DMind] = 1;
}

// Initialize Tietz camera interface
int CCameraController::InitializeTietz(int whichCameras, int *originalList, int numOrig, 
                                        BOOL anyPreExp)
{
  int i, ind;
  CString mess;
  CamPluginFuncs *funcs;

  // Create an instance just once, even if later lock fails
  if (!mTietzInstances) {

    funcs = mWinApp->mPluginManager->GetCameraFuncs("TietzPlugin");
    if (!funcs) {
      AfxMessageBox("The TietzPlugin was not loaded; no Tietz camera will be available",
        MB_EXCLAME);
      return 1;
    }
    if (!funcs->GetCameraSize || !funcs->InitializeInterface || !funcs->InitializeCamera
      || !funcs->UninitializeCameras || !funcs->PrepareForAcquire || 
      !funcs->SelectCamera || !funcs->SelectShutter || !funcs->SetRotationFlip ||
      !funcs->SetGainIndex || !funcs->GetNumberOfGains || !funcs->GetPluginVersion) {
        AfxMessageBox("The TietzPlugin is missing at least one required function; no "
          "Tietz camera will be available", MB_EXCLAME);
      return 1;
    }

    ind = funcs->InitializeInterface(anyPreExp ? TIETZ_USE_SHUTTERBOX : 0);
    if ((ind & ~TIETZ_NO_SHUTTERBOX) == 0) {
      mTietzInstances = true;
      mShutterInstance = anyPreExp && !ind;

      // Get the functions into array, cancel property if shutterbox creation failed
      for (i = 0; i < numOrig; i++) {
        ind = originalList[i];
        if (mAllParams[ind].TietzType) {
          if (anyPreExp && !mShutterInstance)
            mAllParams[ind].TietzCanPreExpose = false;
          mPlugFuncs[ind] = funcs;
          mAllParams[ind].cameraNumber = mAllParams[ind].TietzType;
        }
      }
    } else {
      mess = "Failed to connect to Tietz camera interface.\n"
        "No Tietz camera will be available";
      if (ind == TIETZ_NO_SOCKET) 
        mess += "\n\nIs the Tietz-SEMServer running on the Tietz computer?\n"
        "Set property DebugOutput to K to diagnose connection to the server";
      AfxMessageBox(mess, MB_EXCLAME);
    }
  }

  // If interfaces created, proceed to lock and initialize
  if (mTietzInstances) {

    // If the lock fails, mark as one camera, with later attempt
    // to reinitialize
    if (LockInitializeTietz(true)) {
      mNumTietzCameras = 1;
      if (whichCameras == INIT_CURRENT_CAMERA)
        return 1;
    } else {

      // If initialization succeeds, count up number of cameras
      mTietzInitialized = true;
      mNumTietzCameras = 0;
      for (ind = 0; ind < numOrig; ind++) {
        i = originalList[ind];
        if (mAllParams[i].TietzType && !mAllParams[i].failedToInitialize)
          mNumTietzCameras++;
      }
    }
  }
  return 0;
}

// Initialize FEI camera interface
void CCameraController::InitializeFEIcameras(int &numFEIlisted, int *originalList, 
                                             int numOrig)
{
  int i, ind, err, bin, base, power;
  bool needsConfig = false, oldFalcon2, anyOldFalcon2 = false;

  // Check that all listed cameras are accessible (???)
  numFEIlisted = 0;
  for (ind = 0; ind < numOrig; ind++) {
    i = originalList[ind];
    if (mAllParams[i].FEItype) {
      oldFalcon2 = mAllParams[i].FEItype == FALCON2_TYPE;
      err = mScope->LookupScriptingCamera(&mAllParams[i], false);
      if (err) {
        //mAllParams[i].failedToInitialize = true;
        if (err == 1) {
          if (mOtherCamerasInTIA)
            mWinApp->AppendToLog("The FEI camera named " + mAllParams[i].name + 
            " was not found among the available cameras\r\nTIA must be running and the "
            " camera must be selected in the microscope user interface CCD panel\r\n"
            "for the camera to be available");
          else
            AfxMessageBox("The FEI camera named " + mAllParams[i].name + 
            " was not found among the available cameras\n\nIs TIA running?", MB_EXCLAME);
        }
        if (err == 2) {
          AfxMessageBox("FEI camera cannot be assessed because microscope object is "
            "not initialized", MB_EXCLAME);
          break;
        }
      } else {
        if (mAllParams[i].CamFlags & PLUGFEI_USES_ADVANCED)
          oldFalcon2 = false;
      }
      mFEIinitialized = true;
      if (oldFalcon2)
        anyOldFalcon2 = true;
      numFEIlisted++;
      mAllParams[i].flyback = mAllParams[i].basicFlyback + mAllParams[i].addedFlyback;
    }
  }

  // Initialize helper now that it is known whether configs should be ignored
  // Also clear retractability here if not advanced interface
  mFalconHelper->Initialize(mFEIinitialized && !anyOldFalcon2 ? 1 : 0);
  for (ind = 0; ind < numOrig; ind++) {
    i = originalList[ind];
    if (mAllParams[i].FEItype) {
      if (mAllParams[i].CamFlags & PLUGFEI_USES_ADVANCED) {
        if (mAllParams[i].autoGainAtBinning > 0) {
          for (bin = 0; bin < mAllParams[i].numBinnings; bin++) {
            if (mAllParams[i].gainFactor[bin] != 1.) {
              mAllParams[i].autoGainAtBinning = 0;
              AfxMessageBox("The FEI camera named " + mAllParams[i].name + " has "
                "both RelativeGainFactors and AutoGainFactors property entries.\n\n"
                "The AutoGainFactors entry will be ignored.", MB_EXCLAME);
            }
          }
        }
        if (mAllParams[i].autoGainAtBinning >= mAllParams[i].numBinnings) {
          mAllParams[i].autoGainAtBinning = 0;
          AfxMessageBox("The starting binning index in the AutoGainFactors property entry"
            " is too high for the\nFEI camera named " + mAllParams[i].name + 
            " and this entry will have no effect", MB_EXCLAME);

        }
        base = mAllParams[i].autoGainAtBinning;
        if (base > 0) {
          for (bin = base; bin < mAllParams[i].numBinnings; bin++) {
            power = B3DNINT(2. * log((double)mAllParams[i].binnings[bin] / 
              mAllParams[i].binnings[base - 1]) / log(2.));
            mAllParams[i].gainFactor[bin] = (float)pow(0.5, (double)power);
          }
        }
      } else {
        mAllParams[i].retractable = false;
        mAllParams[i].autoGainAtBinning = 0;
      }
    }
  }
}

// Initialize Direct Electron cameras
void CCameraController::InitializeDirectElectron(int *originalList, int numOrig)
{
  int ind, i;

  mNumDECameras = 0;
  for (ind = 0; ind < numOrig; ind++) {
    i = originalList[ind];
    if (mAllParams[i].DE_camType) {
      if (mAllParams[i].DE_camType){
        if(!mTD.DE_Cam)
          mTD.DE_Cam = new DirectElectronCamera(mAllParams[i].DE_camType,i);
        // DNM: need to test for + return value
        if (mTD.DE_Cam->initialize(mAllParams[i].name, i) > 0) {
          SEMTrace('D', "successfully initialized the camera: %s", mAllParams[i].name);
          mNumDECameras = mNumDECameras + 1;	
          mDEInitialized = true;
          if (mAllParams[i].useContinuousMode < 0 && mAllParams[i].DE_camType >= 2) {
            mAllParams[i].useContinuousMode = 1;
            mAllParams[i].setContinuousReadout = 1;
          }
          if (mAllParams[i].name.Find("Survey") > 0)
            mAllParams[i].CamFlags &= ~(DE_CAM_CAN_COUNT);
          if (mAllParams[i].CamFlags & DE_WE_CAN_ALIGN)
            mFalconHelper->Initialize(-1);
        }	else {
          AfxMessageBox("FAILURE in Initializing Direct Electron camera", MB_EXCLAME);
          mAllParams[i].failedToInitialize = true;
        }
      } else
        mAllParams[i].failedToInitialize = true;
    }
  }

  // DNM: keep it consistent, delete object and NULL the pointer now if none initialized
  if (!mDEInitialized) {
    delete mTD.DE_Cam;
    mTD.DE_Cam = NULL;
  }
}

// Initialize plugin cameras of all kinds
void CCameraController::InitializePluginCameras(int &numPlugListed, int *originalList, 
                                                int numOrig)
{
  int ind, i, err, num, idum;
  double minPixel, rotInc, ddum;
  CString report;

  numPlugListed = 0;
  for (ind = 0; ind < numOrig; ind++) {
    err = 0;
    i = originalList[ind];
    if (!mAllParams[i].pluginName.IsEmpty()) {
      mPlugFuncs[i] = mWinApp->mPluginManager->GetCameraFuncs
        (mAllParams[i].pluginName);
      if (!mPlugFuncs[i]) {
        AfxMessageBox("The camera plugin named " + mAllParams[i].pluginName + 
          " did not load so that camera will be unavailable", MB_EXCLAME);
        err = 1;
      } else {
        num = mPlugFuncs[i]->GetNumberOfCameras();
        if (num <= 0) {
          AfxMessageBox("Error getting number of cameras from the camera plugin "
            "named " + mAllParams[i].pluginName, MB_EXCLAME);
          err = 1;
        }
      }
      if (!err && num > 1 && !mPlugFuncs[i]->SelectCamera) {
        AfxMessageBox("The camera plugin named " + mAllParams[i].pluginName +
          "reported more than one camera but has no CameraSelect function", 
          MB_EXCLAME);
        err = 1;
      }
      if (!err && mAllParams[i].cameraNumber >= num) {
        report.Format("The CameraNumber property for a plugin camera is %d but the "
          "plugin named %s only reports %d cameras", 
          mAllParams[i].cameraNumber, (LPCTSTR)mAllParams[i].pluginName, num);
        AfxMessageBox(report, MB_EXCLAME);
        err = 1;
      }

      // Test for shutter call if there is a second label and only one shutter not set
      if (!err && !mAllParams[i].onlyOneShutter && 
        !mAllParams[i].shutterLabel2.IsEmpty() && !mPlugFuncs[i]->SelectShutter) {
          AfxMessageBox("The properties for a plugin camera had a second shutter label "
            "but the plugin named " + mAllParams[i].pluginName + 
            "has no CameraShutter function", MB_EXCLAME);
          err = 1;
      }

      // Test for insertion functions if retractable
      if (!err && mAllParams[i].retractable && (!mPlugFuncs[i]->IsCameraInserted ||
        !mPlugFuncs[i]->SetCameraInsertion)) {
          AfxMessageBox("The \"Retractable\" property is set for a plugin camera but "
            "the plugin camera named " + mAllParams[i].pluginName + 
            " has no insertion functions", MB_EXCLAME);
          err = 1;
      }
      if (!err && mAllParams[i].rotationFlip && !mPlugFuncs[i]->SetRotationFlip) {
        AfxMessageBox("The \"RotationAndFlip\" property is set for a plugin camera "
          "but the plugin camera named " + mAllParams[i].pluginName + 
          " has no SetRotationFlip function", MB_EXCLAME);
        err = 1;
      }

      // For a STEM camera, send the camera number plus the number of additional channels
      num = mAllParams[i].cameraNumber;
      if (mAllParams[i].STEMcamera) 
        num += mAllParams[i].numChannels - 1;
      if (!err && mPlugFuncs[i]->InitializeCamera && 
        mPlugFuncs[i]->InitializeCamera(num) != 0) {
          report.Format("Failed to initialize camera %d %sin the plugin named %s", 
            mAllParams[i].cameraNumber, mAllParams[i].STEMcamera ? "(STEM) " : "",
            (LPCTSTR)mAllParams[i].pluginName);
          AfxMessageBox(report, MB_EXCLAME);
          err = 1;
      }
      if (!err && mPlugFuncs[i]->SetRotationFlip)
        mPlugFuncs[i]->SetRotationFlip(mAllParams[i].cameraNumber, 
        mAllParams[i].rotationFlip);
      if (!err && mPlugFuncs[i]->SetSizeOfCamera)
        mPlugFuncs[i]->SetSizeOfCamera(mAllParams[i].cameraNumber, mAllParams[i].sizeX,
        mAllParams[i].sizeY);

      // Send the STEM interface our desired size too so it doesn't have to rely on the
      // previous call being first
      if (!err && mAllParams[i].STEMcamera && mPlugFuncs[i]->GetSTEMProperties) {
        mPlugFuncs[i]->GetSTEMProperties(mAllParams[i].sizeX, mAllParams[i].sizeY,
          &minPixel, &mAllParams[i].maxPixelTime, &mAllParams[i].pixelTimeIncrement, 
          &rotInc, &ddum, &mAllParams[i].maxIntegration, &idum);
        if (minPixel > 0)
          mAllParams[i].minPixelTime = (float)minPixel;
      }
      mAllParams[i].failedToInitialize = err != 0;
      if (!err)
        numPlugListed++;
    }
  }
  mPlugInitialized = numPlugListed > 0;
}


static VOID CALLBACK FilterUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, 
                                      DWORD dwTime)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->CheckFilterSettings();
}
///////////////////////////////////////////////////////////////////////
// Parameter-setting routines
///////////////////////////////////////////////////////////////////////

// Set the debug mode for output to Results window (Gatan) or turn on DE debug mode
void CCameraController::SetDebugMode(BOOL inVal)
{
  // Do not initialize the camera from this call, which could be from reading properties
  mDebugMode = inVal;

	if (mDEInitialized && mTD.DE_camType == LC1100_4k) 
		mTD.DE_Cam->setDebugMode();

  if ((mPlugInitialized || mTietzInitialized) && mTD.plugFuncs && 
    mTD.plugFuncs->SetDebugMode)
    mTD.plugFuncs->SetDebugMode(inVal ? 1 : 0);

  if (!mDMInitialized[0] && !mDMInitialized[1] && !mDMInitialized[2])
    return;
  if (CreateCamera(CREATE_FOR_ANY))
    return;
  for (int DMind = 0; DMind < 3; DMind++)
    if (mDMInitialized[DMind])
      MainCallDMIndCamera(DMind, SetDebugMode(mDebugMode));
  ReleaseCamera(CREATE_FOR_ANY);
}

// Set camera parameters
void CCameraController::SetCurrentCamera(int currentCam, int activeCam)
{
  mParam = &mAllParams[currentCam];
  mFilmShutter = 1 - mParam->beamShutter;
  mTD.Camera = activeCam;
  mTD.SelectCamera = mParam->cameraNumber;  // See how simple this is (if it works)?
  mProcessHere = mParam->processHere > 0;
  mTD.plugFuncs = mPlugFuncs[currentCam];
  sSocketCurrent = (mParam->GatanCam && mParam->useSocket) ? 1 : 0;
  sAmtCurrent = mParam->AMTtype ? 1 : 0;
  mNeedToSelectDM = mParam->DMCamera;

  // If switching away from camera in film shutter mode, switch it to beam shutter
  if (mCameraWithBeamOn && mCameraWithBeamOn != mParam->TietzType)
    SwitchTeitzToBeamShutter(currentCam);

  // Inform scope about the shutterless status
  mScope->SetShutterlessCamera(mParam->noShutter);
   
  //Had to add this for DE camera switching TM.
  //3_28_11 for DE12
  if (mParam->DE_camType) {
	  if(mTD.DE_Cam)
		  mTD.DE_Cam->setCameraName(mParam->name);
  }
  UtilModifyMenuItem(2, ID_CAMERA_ACQUIREGAINREF, (mParam->DE_camType && 
    !CanProcessHere(mParam)) ? "Ac&quire Ref in Server" : "Ac&quire Gain Ref");

  // When switching to an FEI camera and there is more than one, invalidate its index
  // so the scope will be refreshed to look it up again
  if (mParam->FEItype && mOtherCamerasInTIA && !FCAM_ADVANCED(mParam))
    mParam->eagleIndex = -1;

  FixDirForFalconFrames(mParam);

  // Manage blanking of AMT camera
  if (!mAMTactive && mParam->AMTtype)
    SetAMTblanking();
  else if (mAMTactive && !mParam->AMTtype && mDMInitialized[AMT_IND]) {
    SEMTrace('1', "SetCurrentCamera turning off AMT blanking");
    try {
      mTD.amtCam->SetShutterNormallyClosed(mTD.SelectCamera, 0);
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("turning off blanking of AMT camera "));
    }
  }
  mAMTactive = mParam->AMTtype != 0;
}

// Set flag to divide 16-bit data by 2
void CCameraController::SetDivideBy2(int inVal)
{
  mDivideBy2 = inVal;

  // Need to delete dark references for all 16-bit cameras
  for (int i = 0; i < mRefArray.GetSize(); i++) {
    DarkRef *ref = mRefArray[i];
    if (ref->GainDark == DARK_REFERENCE && 
      mAllParams[mActiveList[ref->Camera]].unsignedImages) {
      
      // Delete array only if we own it
      if (ref->Array && ref->Ownership)
        delete ref->Array;
      delete ref;
      mRefArray.RemoveAt(i--);
    }
  }
}


///////////////////////////////////////////////////////
//  External control functions (starting, stopping, monitoring, getting deferred sum,
//                              making align com file)
///////////////////////////////////////////////////////

// InitiateCapture is to be called by all outside routines to request a capture
void CCameraController::InitiateCapture(int inSet)
{
/*  BOOL cal = mShiftManager->CalibratingIS() || 
    mWinApp->mFocusManager->DoingFocus() || 
    mWinApp->mComplexTasks->DoingResetShiftRealign(); */
  if (mWinApp->GetDummyInstance())
    return;
  if (CameraBusy()) {
    SetPending(inSet);
    return;
  }

  // If the last shot was a single frame, do the auto-shift to the extent specified
  if (mSingleContModeUsed == SINGLE_FRAME || mRequiredRoll) {
    int nRoll = mBufferManager->GetShiftsOnAcquire();
    if (nRoll < mRequiredRoll)
      nRoll = mRequiredRoll;

    // If current buffer is not A and its image is rolled, switch to the new buffer
    // Defer updates: they will be done by Capture or by ErrorCleanup if it bails out
    mWinApp->SetDeferBufWinUpdates(true);
    RollBuffers(nRoll, mWinApp->GetImBufIndex());
    mWinApp->SetDeferBufWinUpdates(false);
  }
  mLastConSet = inSet;
  Capture(inSet);

}

// Roll the buffers by the given amount, keeping the buffer displayed indicated by
// keepIndexCurrent displayed if it is not 0 or less.
void CCameraController::RollBuffers(int nRoll, int keepIndexCurrent)
{
  for (int i = nRoll; i > 0 ; i--) {
    mBufferManager->CopyImageBuffer(i - 1, i);
    if (keepIndexCurrent > 0 && keepIndexCurrent == i - 1)
      mWinApp->SetCurrentBuffer(keepIndexCurrent + 1);
  }
}

// Called when space bar is hit to restart last capture mode
void CCameraController::RestartCapture()
{
  if (mLastConSet >= 0 && mLastConSet < NUMBER_OF_USER_CONSETS)
    InitiateCapture(mLastConSet);
}

// To set up a pending shot, simply set the mode into the flag
void CCameraController::SetPending(int whichMode)
{
  mPending = whichMode;
}

// Stop capture: stop repeated captures regardless, use ifNow > 0 to set halting flag
// if the stop is to be immediate, use ifNow < 0 in continuous mode to discard the
// final image that comes in
void CCameraController::StopCapture(int ifNow)
{
  // Set to discard final image if in continuous mode
  if (ifNow < 0 && mRepFlag >= 0)
    mDiscardImage = true;

  // Clear the repeat flag
  TurnOffRepeatFlag();
  if (!CameraBusy()) {
    StopContinuousSTEM();
    return;
  }
  if (ifNow > 0) {
    mHalting = true;
  }
  
  // DNM: call the DE camera only if it is the current camera	
  if (mParam->DE_camType  && mParam->DE_camType < 2 && mTD.DE_Cam) {
    mWinApp->AppendToLog("Going to try to stop acquisition of camera.");
    mTD.DE_Cam->StopAcquistion();
    mHalting = true;
  }
}

// If there is a pending shot, initiate it and return true
bool CCameraController::InitiateIfPending(void)
{
  if (mPending >= 0) {
    int temp = mPending;
    mPending = -1;
    InitiateCapture(temp);
    return true;
  }
  return false;
}

// Test for camera ready: is it initialized and not busy
BOOL CCameraController::CameraReady()
{
  return ((GetInitialized() || mSimulationMode) && !CameraBusy());
}

BOOL CCameraController::CameraBusy()
{
  return mRaisingScreen || mInserting || mSettling >= 0 || mAcquiring || mEnsuringDark ||
   mWaitingForStacking > 0 || mStartedFalconAlign || mStartedExtraForDEalign ||
   mScope->LongOperationBusy(LONG_OP_HW_DARK_REF);
}

int CCameraController::GetServerFramesLeft()
{
  return mTD.DE_Cam ? mTD.DE_Cam->GetNumLeftServerRef() : 0;
};

// Retract all cameras: set all retractable cameras as "canBlock" so that it will test
// and retract them if in
void CCameraController::RetractAllCameras(void)
{
  for (int ind = 0; ind < mWinApp->GetNumActiveCameras(); ind++)
    mAllParams[mActiveList[ind]].canBlock = mAllParams[mActiveList[ind]].retractable;
  Capture(RETRACT_ALL);
}

// Test for whether doing a continuos acquire, return parameter set PLUS ONE if so
int CCameraController::DoingContinuousAcquire(void)
{
  if ((CameraBusy() || mInDisplayNewImage) && 
    (mRepFlag >= 0 || (mSingleContModeUsed == CONTINUOUS && !mStoppedContinuous))) {
      if (mRepFlag >= 0)
        return (mRepFlag + 1);
      if (mLastConSet >= 0)
        return (mLastConSet + 1);
      return 1;
  }
  return 0;
}

// test for whether the current camera type is initialized and available
BOOL CCameraController::GetInitialized()
{
  CameraParameters *camP = &mAllParams[mWinApp->GetCurrentCamera()];
  if (camP->TietzType) 
    return mTietzInitialized && !camP->failedToInitialize;

  if (camP->DE_camType)
  	return mDEInitialized;
  if (camP->FEItype)
    return mFEIinitialized;
  if (!camP->pluginName.IsEmpty())
    return mPlugInitialized;
  return mDMInitialized[CAMP_DM_INDEX(camP)];
}

// Return whether processing is being done here
BOOL CCameraController::GetProcessHere()
{
  return (mProcessHere || mParam->TietzType || 
    (mParam->AMTtype && mDMversion[AMT_IND] < AMT_VERSION_CAN_NORM) ||
    mParam->DE_camType == 1 || 
    (mTD.plugFuncs && !mParam->canDoProcessing)) && CanProcessHere(mParam);
}

// Set whether processing here from menu
void CCameraController::SetProcessHere(BOOL inVal)
{
  mProcessHere = inVal;
  mParam->processHere = inVal ? 1 : 0;
}

// Return whether post-actions can be taken safely
BOOL CCameraController::PostActionsOK(void)
{
  return (!mParam->FEItype && !((mParam->K2Type > 1 && mParam->startupDelay < 1.46) ||
    (mParam->K2Type == 1 && mParam->startupDelay < mK2MinStartupDelay)) && 
    mParam->postActionsOK && mParam->DE_camType < 2 && 
    !mParam->STEMcamera && mParam->noShutter != 1);
}

// Return whether the given camera can pre-expose with the given shuttering
bool CCameraController::CanPreExpose(CameraParameters *param, int shuttering)
{
  return (!param->STEMcamera && ((param->GatanCam && (mParam->noShutter || 
    (!param->onlyOneShutter && (!param->OneViewType || shuttering == USE_FILM_SHUTTER) && 
    (param->DMsettlingOK || shuttering == USE_DUAL_SHUTTER)))) ||
    param->K2Type || (param->OneViewType && param->onlyOneShutter) || 
    param->FEItype == EAGLE_TYPE || 
    (param->TietzType && param->TietzCanPreExpose && shuttering == USE_BEAM_BLANK) ||
    param->AMTtype || (param->DE_camType && !param->onlyOneShutter) || 
    (!param->pluginName.IsEmpty() && (param->noShutter || param->canPreExpose))));
}

// Return whether processing here is allowed for the given camera
bool CCameraController::CanProcessHere(CameraParameters *param)
{
  return !((param->FEItype && FCAM_ADVANCED(param) && mASIgivesGainNormOnly) ||
    param->K2Type || param->OneViewType || (param->DE_camType && mTD.DE_Cam &&
    mTD.DE_Cam->GetNormAllInServer()));
}

// Returns true if camera is a OneView and the DM version can do drift correction
bool CCameraController::OneViewDriftCorrectOK(CameraParameters *param)
{
  return param->OneViewType && 
    mDMversion[CAMP_DM_INDEX(param)] >= DM_OVW_DRIFT_CORRECT_OK;
}

bool CCameraController::HasNewK2API(CameraParameters * param)
{
  return param->K2Type && mDMversion[CAMP_DM_INDEX(param)] >= DM_K2_API_CHANGED_LOTS;
}

// Returns member variable for falcon frames, or value obtained from advanced scripting
int CCameraController::GetMaxFalconFrames(CameraParameters *params)
{
  if (FCAM_ADVANCED(params))
    return ((params->CamFlags >> PLUGFEI_MAX_FRAC_SHIFT) & 0xFFFF);
  return mMaxFalconFrames;
}

// Return number of intervals if dynamic focusing can be used
int CCameraController::DynamicFocusOK(float exposure, int sizeY, float flyback,
                                      int &interval, double &msPerLine)
{
  double fullexp = 1000. * exposure + sizeY * flyback / 1000.;
  if (mDynFocusInterval <= 0)
    return 0;
  int numSteps = (int)(fullexp / mDynFocusInterval);
  if (numSteps < 5)
    return 0;
  interval = (int)(fullexp / numSteps);
  msPerLine = fullexp / sizeY;
  return numSteps;
}

// Sets up to go to an opposite low dose area on next shot; return true for error
bool CCameraController::OppositeLDAreaNextShot(void)
{
  if (mWinApp->mLowDoseDlg.ShiftsBalanced())
    return true;
  mOppositeAreaNextShot = true;
  return false;
}

// Sets up for early return on next K2 shot, return true for error
bool CCameraController::SetNextAsyncSumFrames(int inVal, bool deferSum)
{
  if (!mParam->K2Type) {
    SEMMessageBox("Early return works only with a K2 camera");
    return true;
  }
  if (inVal == 65535 && mDMversion[CAMP_DM_INDEX(mParam)] < DM_K2_API_CHANGED_LOTS) {
    SEMMessageBox("Early return with a full sum is not possible before GMS 2.3.1");
    return true;
  }
  if (mK2SynchronousSaving && mDMversion[CAMP_DM_INDEX(mParam)] >= 
    DM_K2_API_CHANGED_LOTS) {
    SEMMessageBox("Early return with synchronous saving is not possible after GMS 2.3.0");
    return true;
  }
  mNextAsyncSumFrames = inVal;
  mDeferSumOnNextAsync = deferSum;
  return false;
}

/*
 * Fetch the deferred sum from the plugin if it exists
 */
int CCameraController::GetDeferredSum(void)
{
  if (!mAskedDeferredSum) {
    SEMMessageBox("There is no deferred sum available");
    return 1;
  }
  if (!mParam->K2Type) {
    SEMMessageBox("A deferred sum can only be obtained with a K2 camera as current"
      " camera");
    return 2;
  }
  if (CameraBusy()) {
    SEMMessageBox("Cannot get deferred sum back while camera is busy");
    return 3;
  }
  if (!mConsDeferred || !mBufDeferred || !mExtraDeferred) {
    SEMMessageBox("Cannot process a deferred sum correctly;\n"
      "some of the data that was supposed to be saved is not available");
    return 4;
  }
  mTD.GetDeferredSum = true;
  RollBuffers(mBufferManager->GetShiftsOnAcquire(), mWinApp->GetImBufIndex());

  // Set all the items needed for acquire call and processing to occur
  mTD.ShotDelayTime = 0;
  mDMsizeX = mXdeferredSize;
  mDMsizeY = mYdeferredSize;
  mTD.DMSizeX = mXdeferredSizeTD;
  mTD.DMSizeY = mYdeferredSizeTD;
  mTD.NumAsyncSumFrames = -1;
  mTD.GatanReadMode = mReadModeDeferred;
  mBinning = mBinDeferred;
  mTD.Binning = mBinDeferredTD;
  mLeft = mLeftDeferred;
  mRight = mRightDeferred;
  mTop = mTopDeferred;
  mBottom = mBotDeferred;

  mTD.SaveFrames = mSavedInDeferred;
  mTD.UseFrameAlign = mAlignedDeferred;
  mTD.DoseFrac = true;
  mLastConSet = mLastDeferredConSet;
  mAskedDeferredSum = false;
  mDeferredSumFailed = false;

  // Do the essentials for starting the thread
  mAcquiring = true;
  mAcquireThread = AfxBeginThread(AcquireProc, &mTD, THREAD_PRIORITY_HIGHEST, 0, 
    CREATE_SUSPENDED);
  TRACE("AcquireProc created with ID 0x%0x\n",mAcquireThread->m_nThreadID);
  mAcquireThread->m_bAutoDelete = false;
  mWinApp->SetStatusText(SIMPLE_PANE, "GETTING DEFERRED SUM");
  mWinApp->UpdateBufferWindows();
  mAcquireThread->ResumeThread();
  mWinApp->AddIdleTask(ThreadBusy, AcquireDone, AcquireError, 0, 0);
  return 0;
}

/*
 * Make plugin write an align com file to work with an mdoc at the end of a tilt series
 */
int CCameraController::MakeMdocFrameAlignCom(void)
{
  CString mdocPath, mdocName, tempStr, comRoot;
  int nameLen, textLen, stringSize, frameX, frameY;
  long retVal = 0;
  UINT numRead;
  CFile *file = NULL;
  CFileStatus status;
  char *strings;
  ControlSet *conSet = &mConSetsp[RECORD_CONSET];
  bool remote = mParam->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID);

  // Check parameters
  if (!mParam->K2Type && !IS_FALCON2_OR_3(mParam)) {
    SEMMessageBox("Cannot make a com file for aligning tilt series frames\n"
      "unless the K2 or Falcon camera is still selected");
    return 1;
  }
  if (mParam->K2Type && GetPluginVersion(mParam) < PLUGIN_CAN_ALIGN_FRAMES) {
    SEMMessageBox("The current version of the plugin cannot make\n"
      "a com file for aligning frames in IMOD");
    return 1;
  }
  if (mParam->K2Type && !(IsConSetSaving(conSet, RECORD_CONSET, mParam, true) && 
    conSet->alignFrames && conSet->useFrameAlign > 1)) {
      SEMMessageBox("The Record parameters must be set for frame saving and aligning\n"
        "in IMOD in order to make a com file for aligning frames");
      return 1;
  }
  if (!mWinApp->mStoreMRC || mWinApp->mStoreMRC->GetAdocIndex() < 0) {
    SEMMessageBox("There needs to be a current open file with an associated mdoc file\n"
      "to make a com file for aligning tilt series frames");
    return 1;
  }

  // Set path and start working with mdoc file
  if (mAlignFramesComPath.IsEmpty())
    mAlignFramesComPath = mDirForK2Frames;
  mdocPath = mWinApp->mStoreMRC->getAdocName();
  UtilSplitPath(mdocPath, tempStr, mdocName);
  UtilSplitExtension(mdocName, comRoot, tempStr);
  if (!mParam->K2Type) {

    // For Falcon, just call routine and leave
    mFalconHelper->GetSavedFrameSizes(mParam, conSet, frameX, frameY);
    retVal = mFalconHelper->WriteAlignComFile(mdocPath, mAlignFramesComPath + 
      '\\' + comRoot + ".pcm", conSet->faParamSetInd, 
      mFalconHelper->GetUseGpuForAlign(1), true, frameX, frameY);
    if (retVal) {
      tempStr = "The com file for aligning frames was not written:\r\n   ";
      if (retVal < 0)
        tempStr += "An error occurred copying the mdoc file to the frame location";
      else
        tempStr += SEMCCDErrorMessage(retVal);
      SEMMessageBox(tempStr);
      return 1;
    }
    return 0;
  }

  nameLen = mdocName.GetLength() + 1;
  if (remote) {

    // If it is remote, we need to send the mdoc, so open and get string length
    try {
      file = new CFile(mdocPath, CFile::modeReadWrite | CFile::shareDenyWrite);
      if (!file->GetStatus(status)) {
        SEMMessageBox("Could not get status and length of mdoc file " + mdocPath);
        delete file;
        return 1;
      }
      textLen = (int)status.m_size;
    }
    catch (CFileException *err) {
      err->Delete();
      SEMMessageBox("Could not open mdoc file " + mdocPath);
      return 1;
    }
  } else {

    // Otherwise we need to send filename
    textLen = mdocPath.GetLength() + 1;
  }

  // Get string buffer and start with mdoc name
  stringSize = (4 + nameLen + textLen) / 4;
  strings = new char[4 * stringSize];
  sprintf(strings, "%s", (LPCTSTR)mdocName);
  if (remote) {

    // Read mdoc into the string buffer after name
    try {
      numRead = file->Read(strings + nameLen, stringSize * 4 - nameLen - 1);
      strings[nameLen + numRead] = 0x00;
    }
    catch (CFileException *err) {
      err->Delete();
      SEMMessageBox("Error reading in mdoc file " + mdocPath);
      retVal = 1;
    }
    delete file;
  } else {

    // Or put the full path there
    sprintf(strings + nameLen, "%s", (LPCTSTR)mdocPath);
  }
  if (!retVal && !mParam->useSocket && CreateCamera(CREATE_FOR_CURRENT, false)) {
    SEMMessageBox("Error connecting with camera for making tilt series align com file");
    retVal = 1;
  }

  // Call the setup routine with appropriate flags and the com file name
  if (!retVal && SetupK2SavingAligning(mConSetsp[RECORD_CONSET], RECORD_CONSET, false, 
    true, &comRoot))
    retVal = 1;

  // Get the plugin to make the file
  if (!retVal) {
    try {
      MainCallGatanCamera(MakeAlignComFile(remote ? K2FA_WRITE_MDOC_TEXT : 0, 0, 0., 0.,
        stringSize, (long *)strings, &retVal));
      if (retVal) {
        tempStr.Format("Error %d trying to create com file for aligning tilt series "
          "frames:\n%s", retVal, SEMCCDErrorMessage(retVal));
        SEMMessageBox(tempStr);
      }
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("trying to make a com file for aligning tilt series "));
      retVal = 1;
    }
  }
  delete [] strings;
  if (!mParam->useSocket)
    ReleaseCamera(CREATE_FOR_CURRENT);
  return retVal;
}

// Take the frame file path for Falcon or DE, and tell FalconHelper to write the com file
// for alignframes
void CCameraController::MakeOneFrameAlignCom(CString &localFramePath, ControlSet *conSet)
{
  int err, frameX, frameY;
  CString dirPath, filename, root, ext;
  int paramInd = conSet->faParamSetInd;
  
  // Extract folder and make sure that
  // helper has this as the last frame folder, and compose name
  UtilSplitPath(localFramePath, dirPath, filename);
  mFalconHelper->SetLastFrameDir(dirPath);
  UtilSplitExtension(filename, root, ext);
  if (mComPathIsFramePath)
    root = dirPath + '\\' + root + ".pcm";
  else
    root = mAlignFramesComPath + '\\' + root + ".pcm";
  mFalconHelper->GetSavedFrameSizes(mParam, conSet, frameX, frameY);
  err = mFalconHelper->WriteAlignComFile(filename, root, 
    paramInd, mFalconHelper->GetUseGpuForAlign(1),
    false, frameX, frameY);
  if (err)
    PrintfToLog("WARNING: The com file for aligning was not saved: %s",
    SEMCCDErrorMessage(err));
}

// Set the next shot for early return with full sum if possible and appropriate
void CCameraController::SetFullSumAsyncIfOK(int inSet)
{
  if (!mParam->K2Type || mDMversion[CAMP_DM_INDEX(mParam)] < DM_K2_API_CHANGED_LOTS ||
    mK2SynchronousSaving || !mConSetsp[inSet].doseFrac || !mConSetsp[inSet].saveFrames)
    return;
  mNextAsyncSumFrames = 65535;
  mAskedDeferredSum = false;
}

// OnIdle can call this static member to find out if camera busy
int CCameraController::TaskCameraBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return (winApp->mCamera->CameraBusy() ? 1 : 0);
}

// Static member to test whether to keep waiting for continuous frame
int CCameraController::TaskGettingFrame()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return ((winApp->mCamera->DoingContinuousAcquire() && 
    winApp->mCamera->GetTaskWaitingForFrame()) ? 1 : 0);
}

// OnIdle checks here - tell it the status of the current operation
int CCameraController::ThreadBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  CCameraController *cam = winApp->mCamera;
  if (cam->Raising())
    return winApp->mScope->ScreenBusy();
  if (cam->Inserting())
    return cam->InsertBusy();
  if (cam->Settling())
    return cam->GetHalting() ? -1 : 1;
  if (cam->EnsuringDark())
    return cam->EnsureBusy();
  if (cam->Acquiring())
    return cam->AcquireBusy();
  if (cam->GetWaitingForStacking() > 0)
    return winApp->mFalconHelper->GetStackingFrames() ? 1 : 0;

  return 0;
}

// Routine to display the last used gain or dark reference, or a newly made one
void CCameraController::ShowReference(int type)
{
  int maxUse = 0;
  DarkRef *ref;
  DarkRef *lastRef = NULL;
  int i, size, binning;
  float factor;
  float *fdata;
  unsigned short *usdata;
  void *array;
  int gainRefBits, byteSize, ownership;
  
  if (type == GAIN_REFERENCE || type == DARK_REFERENCE) {

    // Find the reference
    for (i = 0; i < mRefArray.GetSize(); i++) {
      ref = mRefArray[i];
      if (ref->GainDark == type && ref->UseCount > maxUse) {
        maxUse = ref->UseCount;
        lastRef = ref;
      }
    }

    if (!lastRef)
      return;
    mTD.DMSizeX = lastRef->SizeX;
    mTD.DMSizeY = lastRef->SizeY;
    array = lastRef->Array;
    byteSize = lastRef->ByteSize;
    gainRefBits = lastRef->GainRefBits;

    // Or get the reference from the maker who just called in
  } else {
    binning = type - NEW_GAIN_REFERENCE;
    if (mWinApp->mGainRefMaker->GetReference(binning, array, byteSize, gainRefBits, 
        ownership, 0, 0))
      return;
    mTD.DMSizeX = mParam->sizeX / binning;
    mTD.DMSizeY = mParam->sizeY / binning;
    if (mParam->fourPort || mParam->refSizeEvenX)
      mTD.DMSizeX = (mTD.DMSizeX / 2) * 2;
    if (mParam->fourPort || mParam->refSizeEvenY)
      mTD.DMSizeY = (mTD.DMSizeY / 2) * 2;
  }

  // Get an array and copy data; take gain ref times 1000
  size = mTD.DMSizeX * mTD.DMSizeY;
  if (type == DARK_REFERENCE) {
    NewArray(mTD.Array[0],short int,size);
    mTD.ImageType = kSHORT;
  } else {
    NewArray(fdata,float,size);
    mTD.Array[0] = (short *)fdata;
    mTD.ImageType = kFLOAT;
  }

  if (!mTD.Array[0]) {
    AfxMessageBox("Failed to get memory for displaying reference image", MB_EXCLAME);
    return;
  }
  if (type == DARK_REFERENCE)
    memcpy(mTD.Array[0], array, size * 2);
  else {
    if (byteSize == 4) {
      memcpy(mTD.Array[0], array, size * 4);
    } else {
      usdata = (unsigned short *)array;
      factor = 1.f / (float)(1 << gainRefBits);
      for (i = 0; i < size; i++)
        fdata[i] = usdata[i] * factor;
    }
  }
  
  // Roll buffers, call display final with flag that image is not acquired  
  mWinApp->SetDeferBufWinUpdates(true);
  if (mSingleContModeUsed == SINGLE_FRAME)
    RollBuffers(mBufferManager->GetShiftsOnAcquire(), 0);
  mWinApp->SetDeferBufWinUpdates(false);
  DisplayNewImage(false);
}

// Return the memory usage of all references
double CCameraController::RefMemoryUsage(void)
{
  double size, sum = 0.;
  DarkRef *ref;
  for (int i = 0; i < mRefArray.GetSize(); i++) {
    ref = mRefArray[i];
    if (ref->Array && ref->Ownership) {
      size = ref->ByteSize * ref->SizeX * ref->SizeY;
      sum += size;
      //SEMTrace('1', "CamCon: i %d type %d size %.2f", i, ref->GainDark, size / 1.e6);
    }
  }
  sum += mWinApp->mGainRefMaker->MemoryUsage();
  return sum;
}

///////////////////////////////////////////////////////
//  Routines to queue actions post-exposure
///////////////////////////////////////////////////////

// queue a tilt or stage move with a given delay
void CCameraController::QueueStageMove(StageMoveInfo inSmi, int inDelay, bool doBacklash)
{
  mStageQueued = true;
  mStageDelay = inDelay;
  mSmiToDo = inSmi;
  mSmiToDo.doBacklash = doBacklash;
  mSmiToDo.doRelax = false;
  mSmiToDo.useSpeed = false;
  if (!doBacklash)
    mSmiToDo.backX = mSmiToDo.backY = mSmiToDo.backZ = mSmiToDo.backAlpha = 0.;
}

// Queue an image shift with a given delay - add tilt axis offset now
void CCameraController::QueueImageShift(double inISX, double inISY, int inDelay)
{
  double taxisX, taxisY;
  mScope->GetTiltAxisIS(taxisX, taxisY);
  mISQueued = true;
  mISToDoX = inISX + taxisX;
  mISToDoY = inISY + taxisY;
  mISDelay = inDelay;
}

// Queue a mag change and normalization
void CCameraController::QueueMagChange(int inMagInd)
{
  mMagQueued = true;
  mMagIndToDo = inMagInd;
}

// Queue up one or two focus steps during exposure
void CCameraController::QueueFocusSteps(float interval1, double focus1, float interval2, 
                                        double focus2)
{
  mFocusInterval1 = interval1;
  mFocusStepToDo1 = focus1;
  mFocusInterval2 = interval2;
  mFocusStepToDo2 = focus2;
}

// Prepare for tilting during a shot
int CCameraController::QueueTiltDuringShot(double angle, int delayToStart, double speed)
{
  mTiltDuringShotDelay = delayToStart;
  mSmiToDo.alpha = angle;
  mSmiToDo.axisBits = axisA;
  mSmiToDo.doBacklash = false;
  mSmiToDo.doRelax = false;
  if (speed > 0. && (!FEIscope || mScope->GetPluginVersion() < FEI_PLUGIN_STAGE_SPEED))
    return 1;
  mSmiToDo.useSpeed = speed > 0.;
  mSmiToDo.speed = speed;
  return 0;
}

///////////////////////////////////////////////////////
//  The main Capture routine: analyzes control set, sets up actions
///////////////////////////////////////////////////////

void CCameraController::Capture(int inSet, bool retrying)
{
  int ind, error, setState, binInd, sumCount;
  BOOL bEnsureDark = false;
  CString logmess;
  int numActive = mWinApp->GetNumActiveCameras();
  int gainXoffset, gainYoffset;
  double exposure, megaVoxel, megaVoxPerSec = 0.15;
  bool superRes, falconHasFrames, weCanAlignFalcon, aligningOnly;
  BOOL retracting = inSet == RETRACT_BLOCKERS || inSet == RETRACT_ALL;
  mWinApp->CopyOptionalSetIfNeeded(inSet);
  ControlSet conSet = mConSetsp[retracting ? 0 : inSet]; // Copy the control set for ease
  mStartTime = GetTickCount();
  mShotIncomplete = true;     // Set flag for incomplete shot, cleared only on image
  mNeedShotToInsert = -1;
  mSingleContModeUsed = mCancelNextContinuous ? SINGLE_FRAME : conSet.mode;
  mStoppedContinuous = false;
  if (mWinApp->GetNoCameras())
    return;
  
  // Reset retry count if this is the first time in here
  if (!mInserting && !mRaisingScreen && !retrying)
    mNumRetries = 0;

  // If we were retracting and the blank flag is set, unblank now
  if (mInserting && !mITD.insert && mBlankWhenRetracting && !mParam->noShutter) {
    mScope->BlankBeam(false);
    mScope->SetCameraAcquiring(false);
  }
  
  // If we came in here, we must be done with screen and insertion
  // Don't clear settling time until after testing for low dose
  if (mInserting || mRaisingScreen) {
    SEMTrace('R', "Capture clearing raise signal");
    if (mParam->AMTtype)
      SetAMTblanking(true);
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }

  // If we just raised the screen to switch into STEM, set a timeout
  if (mRaisingScreen > 0 && mParam->STEMcamera && mWinApp->DoSwitchSTEMwithScreen())
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mScreenUpSTEMdelay);
  mRaisingScreen = 0;
  mInserting = false;
  
  // If the halt flag is set, clear it and exit
  if (mHalting) {
    mHalting = false;
    StopContinuousSTEM();
    ErrorCleanup(1);
    return;
  }

  if (mWaitingForStacking) {
    mWaitingForStacking = 0;
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }

  if (mParam->FEItype == FALCON2_TYPE && mFrameSavingEnabled && conSet.saveFrames && 
    mFalconHelper->GetStackingFrames()) {
      mWaitingForStacking = 1;
      mWinApp->AddIdleTask(ThreadBusy, ScreenOrInsertDone, ScreenOrInsertError, inSet, 0);
      mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR STACKING");    
      mWinApp->UpdateBufferWindows();
      return;
  }

  // If not initialized and not in simulation mode, try to initialize
  if (!GetInitialized() && !mSimulationMode) {
    if (Initialize(INIT_CURRENT_CAMERA)) {
      ErrorCleanup(1);
      return;
    }
  }
  // Trap door to crash the program!  It proves that the crash address matches the map
  /*if ((mWinApp->GetDebugKeys()).Find('}') >= 0) {
    CanPreExpose(NULL, 1);
  }*/
  mSmiToDo.plugFuncs = mTD.scopePlugFuncs = mScope->GetPlugFuncs();
  mTD.errFlag = 0;
  if (mParam->GatanCam)
    mTD.DMversion = mDMversion[CAMP_DM_INDEX(mParam)];

  // If we are retaining shifts/mag change for a set and it is not appropriate, cancel
  if ((mMagToRestore || mShiftedISforSTEM) && 
    !(inSet == mRetainMagAndShift && mParam->STEMcamera))
    RestoreMagAndShift();

  // For STEM camera, figure out detectors now in case it affects screen
  // Clear low dose area flag if returning just to be safe
  if (CapSetupSTEMChannelsDetectors(conSet, inSet, retracting)) {
    mLDwasSetToArea = -1;
    return;
  }

  // Now manage the screen, raise or lower if necessary
  if (CapManageScreen(inSet, retracting, numActive))
    return;

  // If FEI camera has bad index, look it up to make sure it's there
  if (mParam->FEItype && mParam->eagleIndex < 0) {
    ind = mScope->LookupScriptingCamera(mParam, true);
    if (ind) {
      if (ind == 2)
        SEMMessageBox("The microscope communication got screwed up trying to contact "
        "this camera");
      else if (ind == 1)
        SEMMessageBox("Cannot access this camera.\n\n"
        "TIA must be running and the camera must be selected\nin the CCD panel of the "
        "microscope user interface.", MB_EXCLAME);
      ErrorCleanup(1);
      return;
    }
  }

  // Enforce constraints on exposure time in the COPY of the control set, do not modify
  // the original.  This is so that tilt series can keep working on exposure time in
  // main control set, but it means exposure and dose have to be taken from this exposure
  // variable at the end
  ConstrainExposureTime(mParam, &conSet);
  mExposure = conSet.exposure;

  // Make sure binning is legal too
  if (!retracting && FindNearestBinning(mParam, &conSet, binInd, mBinning)) {
    logmess.Format("WARNING: Parameter set has an illegal binning (%d), using binning %d",
      conSet.binning, mBinning);
    mWinApp->AppendToLog(logmess);
    conSet.binning = mBinning;
  }

  // For JEOL, need a reliable value of tilt.  Tilt is needed before frame saving setups
  mTiltBefore = (float)mScope->GetTiltAngle();

  // First turn on the save flag for EVERYBODY if it is supposed to align frames in IMOD
  if (IsConSetSaving(&conSet, inSet, mParam, false)) {
    if (mParam->DE_camType)
      conSet.saveFrames |= DE_SAVE_MASTER;
    else if (!conSet.saveFrames)
      conSet.saveFrames = 1;
  }

  // Next manage K2 switching to dark-subtracted if saving frames and they are
  // supposed to be unnormalized.
  if (mParam->K2Type && conSet.doseFrac) {
    if (IsConSetSaving(&conSet, inSet, mParam, true) && !conSet.saveFrames)
      conSet.saveFrames = 1;
    if (conSet.saveFrames && conSet.processing == GAIN_NORMALIZED && 
        mSaveUnnormalizedFrames && CAN_PLUGIN_DO(CAN_GAIN_NORM, mParam))
        conSet.processing = DARK_SUBTRACTED;
  }

  // Make sure camera is inserted, blocking cameras are retracted, check temperature, and
  // set up saving from K2 camera;  Again clear low dose area flag to be safe
  // Only do this once, not if come back in from settling
  if (mSettling < 0) {
    mTD.NumAsyncSumFrames = -1;
    mNumSubsetAligned = 0;
    if (CapManageInsertTempK2Saving(conSet, inSet, retracting, numActive)) {
        mLDwasSetToArea = -1;
        return;
    }
  }

  // If just retracting, finish up, initiate a pending shot and return
  if (retracting) {
    ErrorCleanup(0);
    InitiateIfPending();
    return;
  }

  // Set the low dose area, setup the energy filter, and check for timeouts and settling
  if (CapSetLDAreaFilterSettling(inSet))
    return;

  // Check the config file time at least before every Falcon shot
  if (IS_BASIC_FALCON2(mParam) && mCanUseFalconConfig >= 0) {
    setState = -1;
    if (mCanUseFalconConfig > 0)
      setState = (conSet.saveFrames || conSet.alignFrames) ? 1 : 0;
    error = mFalconHelper->CheckFalconConfig(setState, ind,
      "Giving up on accessing the config file;\n"
      "You will need to use the stupid checkbox in the Camera Setup dialog to indicate "
      "if intermediate frame saving is selected in the FEI dialog");
    if (error) {
      ErrorCleanup(1);
      return;
    }
    mFrameSavingEnabled = ind > 0;
    mRestoreFalconConfig = setState >= 0;
  }

  // Finally the one-shot flag can be cleared since we are done leaving and coming back
  mNextAsyncSumFrames = -1;
  mDeferSumOnNextAsync = false;
   
  // Set up Falcon saving
  mTD.FEIacquireFlags = 0;
  falconHasFrames = IS_FALCON2_OR_3(mParam) && (FCAM_ADVANCED(mParam) || 
    mFrameSavingEnabled);

  // We can align HERE if the plugin version is sufficient and it is not a Falcon 3
  // with no local path to server
  weCanAlignFalcon = falconHasFrames && 
    mWinApp->mScope->GetPluginVersion() >= PLUGFEI_ALLOWS_ALIGN_HERE &&
    !(mParam->FEItype == FALCON3_TYPE && mLocalFalconFramePath.IsEmpty());

  mSavingFalconFrames = falconHasFrames && conSet.saveFrames;

  // WE are aligning here if we can and alignment is selected for here
  mAligningFalconFrames = weCanAlignFalcon && conSet.alignFrames && 
    conSet.useFrameAlign == 1;

  // Just aligning somewhere, and removing frames, if no save is selected and we CAN align
  // here, meaning there is a local path to Falcon 3 frames
  aligningOnly = conSet.alignFrames && !conSet.saveFrames && weCanAlignFalcon;

  // Set flag for immediate wait if aligning here or in IMOD
  if (FCAM_ADVANCED(mParam) && conSet.alignFrames && weCanAlignFalcon && 
    conSet.useFrameAlign > 0)
    mTD.FEIacquireFlags |= PLUGFEI_WAIT_FOR_FRAMES;

  // Check for inability to write a com file for Falcon 2
  if (FCAM_ADVANCED(mParam) && mParam->FEItype == FALCON2_TYPE && conSet.alignFrames &&
    conSet.useFrameAlign > 1 && CBaseSocket::ServerIsRemote(FEI_SOCK_ID) &&
    mLocalFalconFramePath.IsEmpty()) {
      SEMMessageBox("The program cannot write a command file for alignment with IMOD"
        " when the microscope computer is remote unless the property LocalFalconFramePath"
        " is defined for direct access to files on the microscope.");
      ErrorCleanup(1);
      return;
  }

  // Call the config setup function in any case of aligning/saving as well as to avoid
  // frames for basic Falcon 2
  if ((IS_BASIC_FALCON2(mParam) && mFrameSavingEnabled) || mSavingFalconFrames ||
    mAligningFalconFrames || aligningOnly) {
      if (FCAM_ADVANCED(mParam) || mAligningFalconFrames)
        mDeferStackingFrames = false;

      // Setup routine used to check that the frame folder is not an empty string when not
      // saving, so pass it the top directory in that case, otherwise get path/name for
      // real.  For advanced scripting, this sends the optional folder and it is turned
      // into the full path
      if (conSet.saveFrames || (FCAM_ADVANCED(mParam) && 
        (mAligningFalconFrames || aligningOnly)))
          ComposeFramePathAndName(aligningOnly);
      else
        mFrameFolder = mDirForFalconFrames;
      if (mFalconHelper->SetupConfigFile(conSet, mLocalFalconFramePath, 
        mFrameFolder, mFrameFilename, mFalconFrameConfig, mStackingWasDeferred, mParam,
        mTD.NumFramesSaved)) {
          ErrorCleanup(1);
          return;
      }
  }

  // Now that the fact that we need to align without saving is recorded, turn on the
  // save flag for all align cases: but set flag if we need to remove stack aligned by FEI
  mRemoveFEIalignedFrames = false;
  if (aligningOnly) {
    conSet.saveFrames = 1;
    mRemoveFEIalignedFrames = !conSet.useFrameAlign;
    if (mRemoveFEIalignedFrames)
      mTD.FEIacquireFlags |= PLUGFEI_WAIT_FOR_FRAMES;
  }

  // Set up DE12 saving
  mRemoveAlignedDEframes = false;
  if (mWinApp->mDEToolDlg.CanSaveFrames(mParam)) {
    setState = (conSet.saveFrames & DE_SAVE_FINAL) ? DE_SAVE_FINAL : 0;
    sumCount = conSet.K2ReadMode > 0 ? conSet.sumK2Frames : conSet.DEsumCount;
    sumCount = B3DMAX(1, sumCount);

    // Turn on only the save master flag if aligning only and set flag to remove frames
    if (conSet.alignFrames && !(conSet.saveFrames & DE_SAVE_MASTER)) {
      conSet.saveFrames = DE_SAVE_MASTER;
      mRemoveAlignedDEframes = true;
    }

    if (conSet.saveFrames & DE_SAVE_MASTER) {

      // Set up the traditional frame/sum/final flags
      if (mTD.DE_Cam->GetNormAllInServer()) {

        // New server: always set DE_SAVE_SUMS, set DE_SAVE_FRAMES for saving raw/single
        // also if summing
        setState |= DE_SAVE_SUMS;
        if ((sumCount > 1 || conSet.K2ReadMode > 0) && 
          (conSet.saveFrames & DE_SAVE_SINGLE))
            setState |= DE_SAVE_FRAMES;
      } else {

        // Old server: set DE_SAVE_SUMS only for sums > 1; set DE_SAVE_FRAMES for sums = 1
        // or to save single also
        if (conSet.DEsumCount <= 1 || (conSet.saveFrames & DE_SAVE_SINGLE))
          setState |= DE_SAVE_FRAMES;
        if (conSet.DEsumCount > 1)
          setState |= DE_SAVE_SUMS;
      }

      // Set up frame folder before getting full folder, make sure it is OK
      mFrameFolder = mParam->DE_AutosaveDir;
      if (!mFrameFolder.IsEmpty() && !mDirForDEFrames.IsEmpty()) {
        mFrameFolder += "\\" + mDirForDEFrames;
        if (CreateFrameDirIfNeeded(mFrameFolder, &logmess, 'D')) {
          SEMMessageBox(logmess);
          ErrorCleanup(1);
          return;
        }
      }

      // Get full folder and suffix, using temporary name if aligning only
      // again make sure folder is OK
      ComposeFramePathAndName(mRemoveAlignedDEframes);
      if (!mFrameFolder.IsEmpty() && CreateFrameDirIfNeeded(mFrameFolder, &logmess, 'D')){
        SEMMessageBox(logmess);
        ErrorCleanup(1);
        return;
      }
    }
    if (mTD.DE_Cam->SetAllAutoSaves(setState, sumCount, mFrameFilename, mFrameFolder, 
      mParam->CamFlags & DE_CAM_CAN_COUNT)) {
        ErrorCleanup(1);
        return;
    }
  }
  
  if (!mWinApp->mBufferManager->OKtoDestroy(0, "Capturing an image")) {
    ErrorCleanup(1);
    return;
  }
  
  // Convert from coordinates in conset to unbinned coordinates, adjusting as needed
  CapManageCoordinates(conSet, gainXoffset, gainYoffset);

  // Now that image size is known, check allocation against buffer usage
  if (!UtilOKtoAllocate(2 * mTD.DMSizeX * mTD.DMSizeY)) {
    SEMMessageBox("Cannot allocate array for image without exceeding memory limit\n"
      "You need to delete some image buffers (in Buffer Control panel, open Options)");
    ErrorCleanup(1);
    return;
  }

  // load some thread data
  mTD.ImageType = (mParam->unsignedImages && mDivideBy2 <= 0) ? kUSHORT : kSHORT;
  mTD.DivideBy2 = (mParam->unsignedImages && mDivideBy2 > 0) ? mDivideBy2 : 0;

  /*logmess.Format("Thread data divide by 2: %d", mTD.DivideBy2);
  if (mDebugMode)
    mWinApp->AppendToLog(logmess, LOG_OPEN_IF_CLOSED); */
  mTD.Corrections = mParam->corrections;
  mTD.PreDarkBinning = 0;
  mTD.LowestMModeMag = mScope->GetLowestMModeMagInd();
  mTD.JeolMagChangeDelay = mScope->GetJeolPostMagDelay();
  mTD.RestoreBBmode = mParam->restoreBBmode != 0;

  // Set up all the shuttering and timing parameters
  CapSetupShutteringTiming(conSet, inSet, bEnsureDark);

  // Copy flags and targets, clear the flags
  mTD.TietzType = mParam->TietzType;
  mTD.FEItype = mParam->FEItype;
  mTD.CamFlags = mParam->CamFlags;
  mTD.DE_camType = mParam->DE_camType;

  // DNM: this was added for NCMIR camera
  mWinApp->SetStatusText(SIMPLE_PANE, "");

  mTD.cameraName = mParam->name;
  if ((mTD.FEItype && !mTD.STEMcamera && !mParam->detectorName[0].IsEmpty()) || 
    mParam->TietzType)
      mTD.cameraName = mParam->detectorName[0];
  mTD.MakeFEIerrorTimeout = mMakeFEIerrorBeTimeout;
  mTD.checkFEIname = mOtherCamerasInTIA;
  mTD.oneFEIshutter = mParam->onlyOneShutter;
  mTD.fullSizeX = mParam->sizeX / BinDivisorI(mParam);
  mTD.fullSizeY = mParam->sizeY / BinDivisorI(mParam);
  mTD.PostMoveStage = mStageQueued;
  mTD.imageReturned = false;
  mTD.GetDeferredSum = false;
  mDeferredSumFailed = false;
  mTD.MoveInfo = mSmiToDo;
  mTD.MoveInfo.JeolSD = mTD.JeolSD;
  mTD.PostImageShift = mISQueued;
  mTD.ISX = mISToDoX;
  mTD.ISY = mISToDoY;
  mTD.PostChangeMag = mMagQueued;
  mTD.NewMagIndex = mMagIndToDo;
  mTD.TiltDuringDelay = B3DCHOICE(!FEIscope && mTiltDuringShotDelay >= 0, 
    B3DMAX(1, mTiltDuringShotDelay), 0);
  mTD.Processing = conSet.processing;

  // Set up so that an error message in the post-action script, which is not issued until 
  // final cleanup, will not result in a message box if script or TS is set to avoid them
  mNoMessageBoxOnError = 0;
  if (mWinApp->mMacroProcessor->DoingMacro() &&
    mWinApp->mMacroProcessor->GetNoMessageBoxOnError())
    mNoMessageBoxOnError = -1;
  if (mWinApp->DoingTiltSeries() && mWinApp->mTSController->GetTerminateOnError())
    mNoMessageBoxOnError = 1;

  // Set up scaling for K2 camera.  Enforce read mode 0 for base camera
  // Set read mode -2 or -3 for OneView so it is distinct from K2
  // Send a scaling of 1 for summit linear mode, or the scaling parameter for base camera
  mTD.NeedsReadMode = mNeedsReadMode[CAMP_DM_INDEX(mParam)];
  if (mParam->K2Type == 2)
    conSet.K2ReadMode = 0;
  mTD.GatanReadMode = B3DCHOICE(mParam->K2Type > 0 || 
    (mParam->DE_camType && (mParam->CamFlags & DE_CAM_CAN_COUNT)) ||
    (mParam->FEItype && FCAM_CAN_COUNT(mParam)), conSet.K2ReadMode, -1);
  if (mParam->OneViewType)
    mTD.GatanReadMode = conSet.K2ReadMode != 0 ? -2 : -3;
  mTD.CountScaling = GetCountScaling(mParam);
  if (mTD.GatanReadMode == 0)
    mTD.CountScaling = mParam->K2Type == 2 ? mK2BaseModeScaling : 1.;

  // DE cameras: Handle numerous items
  mTD.AlignFrames = conSet.alignFrames;
  mTD.UseFrameAlign = false;
  mDoingDEframeAlign = 0;
  if (mParam->DE_camType) {

    // Set the frames per second and fix count scaling for no counting
    mTD.FramesPerSec = mParam->DE_FramesPerSec;
    if ((mParam->CamFlags & DE_CAM_CAN_COUNT) && mTD.GatanReadMode > 0)
      mTD.FramesPerSec = mParam->DE_CountingFPS;
    else
      mTD.CountScaling = 1.;

    // set AlignFrames if aligning in server
    mTD.AlignFrames = -1;
    if (mParam->CamFlags & DE_CAM_CAN_ALIGN)
      mTD.AlignFrames = (conSet.useFrameAlign && (conSet.saveFrames & DE_SAVE_MASTER)) ? 
        0 : conSet.alignFrames;

    // Set up the flags for our own frame alignment
    if (conSet.alignFrames && (mParam->CamFlags & DE_WE_CAN_ALIGN)) {
      mDoingDEframeAlign = conSet.useFrameAlign;
      if (conSet.useFrameAlign > 1 && mAlignWholeSeriesInIMOD)
        mDoingDEframeAlign = 0;
    }

    // Give error if TIFF file saving is selected for incompatible actions
    if (!mWinApp->mDEToolDlg.GetFormatForAutoSave() && 
      conSet.alignFrames && (mParam->CamFlags & DE_WE_CAN_ALIGN) && conSet.useFrameAlign){
        SEMMessageBox("You must have saving to MRC files selected to align frames");
        ErrorCleanup(1);
        return;
    }

    // Set hardware binning if available and needed
    mTD.UseHardwareBinning = -1;
    if (mParam->CamFlags & DE_HAS_HARDWARE_BIN)
      mTD.UseHardwareBinning = (conSet.binning > 1 && conSet.boostMag > 0) ? 1 : 0;
    mTD.UseHardwareROI = -1;
    if (mParam->CamFlags & DE_HAS_HARDWARE_ROI)
      mTD.UseHardwareROI = conSet.magAllShots > 0 ? 1 : 0;

    // If doing gain ref in server, Send the repeat count and the mode to set
    // Adjust the mode to linear for the pre-counting items now that FPS is set
    mTD.DE_Cam->SetupServerReference(mDEserverRefNextShot, 
      conSet.processing == UNPROCESSED ? DE_DARK_IMAGE : DE_GAIN_IMAGE);
    if (mDEserverRefNextShot > 0 && mWinApp->mGainRefMaker->GetDEcurProcessType() == 1)
      mTD.GatanReadMode = 0;

    // If last saved frame name was predicted, try to verify it now; then set the timeout
    // for getting name this time by multiplying basic timeout by 2 or 3 ???
    mTD.DE_Cam->VerifyLastSetNameIfPredicted(
      B3DMAX(mDEPrevSetNameTimeout, mDESetNameTimeoutUsed));
    mDESetNameTimeoutUsed = mDEPrevSetNameTimeout * (1.f + 
      (mTD.GatanReadMode > 0 ? 1.f : 0.f) + (mTD.AlignFrames ? 1.f : 0.f));
  }

  mTD.DoseFrac = conSet.doseFrac;
  B3DCLAMP(conSet.frameTime, mMinK2FrameTime, 10.f);
  mTD.FrameTime = conSet.frameTime;
  if ((conSet.doseFrac || IS_FALCON2_OR_3(mParam)) && conSet.alignFrames && 
    conSet.useFrameAlign) {
    mTD.AlignFrames = 0; 
    if (conSet.useFrameAlign == 1) {
      mTD.K2ParamFlags |= K2_USE_FRAMEALIGN;
      mTD.UseFrameAlign = true;
    } else if (!mAlignWholeSeriesInIMOD) {
      mTD.K2ParamFlags |= K2_MAKE_ALIGN_COM;   // Used for Falcon also
    }
  }
  mTD.SaveFrames = conSet.saveFrames && (!mParam->K2Type || conSet.doseFrac);
  mTD.rotationFlip = mParam->rotationFlip;

  B3DCLAMP(conSet.filterType, 0, mNumK2Filters - 1);
  if (mK2FilterNames[conSet.filterType].GetLength() >= MAX_FILTER_NAME_LEN)
    mK2FilterNames[conSet.filterType].Truncate(MAX_FILTER_NAME_LEN - 1);
  sprintf((char *)mTD.FilterName, "%s", (LPCTSTR)mK2FilterNames[conSet.filterType]);
  mStageQueued = false;
  mMagQueued = false;
  mISQueued = false;
  mNumDRdelete = 0;
  mPriorRecordDose = (float)B3DCHOICE(mWinApp->DoingTiltSeries(),
    mWinApp->mTSController->GetCumulativeDose(), -1.);

  // Adjust or set scaling for FEI cameras in advanced interface
  mDivBy2ForExtra = mDivBy2ForImBuf = mTD.DivideBy2;
  if (mParam->FEItype && FCAM_ADVANCED(mParam)) { 
    if (mParam->autoGainAtBinning > 0) {
      mTD.DivideBy2 += B3DNINT(log((double)mParam->gainFactor[binInd]) / log(0.5));
      SEMTrace('E', "Divide by 2 set to %d for binning %d (factor %f)", mTD.DivideBy2, 
        mBinning, mParam->gainFactor[binInd]);
    }
    mDivBy2ForExtra = mTD.DivideBy2;
    mTD.CountScaling = 0.;
    if (mParam->FEItype == FALCON3_TYPE && mParam->falcon3ScalePower > -10) {
      if (conSet.K2ReadMode) {
        if (mParam->falcon3ScalePower <= 0)
          mTD.DivideBy2 -= mParam->falcon3ScalePower;
        else
          mTD.CountScaling = mParam->linear2CountingRatio / 
            (float)pow(2., mTD.DivideBy2 + mParam->falcon3ScalePower);
      } else {
        mTD.FEIacquireFlags |= (PLUGFEI_APPLY_PIX2COUNT | PLUGFEI_UNBIN_PIX2COUNT);
        if (mParam->falcon3ScalePower <= 0)
          mTD.CountScaling = 1.f / (mParam->linear2CountingRatio * 
            (float)pow(2., mTD.DivideBy2 - mParam->falcon3ScalePower));
        else
          mTD.DivideBy2 += mParam->falcon3ScalePower;
      }
      SEMTrace('E', "Falcon 3 scaling: divideBy2 %d float scale %f  flags %x", 
        mTD.DivideBy2, mTD.CountScaling, mTD.FEIacquireFlags);
    }
  }

  // Set timeout for camera acquires from exposure and readout components
  megaVoxel = (mDMsizeX * mDMsizeY / 1.e6) / (mParam->fourPort ? 4. : 1.);
  if (mParam->DE_camType == DE_12 || IS_FALCON2_OR_3(mParam))
    megaVoxel = (mParam->sizeX * mParam->sizeY) / 1.e6;
  exposure = mExposure;
  if (mDEserverRefNextShot > 0) {
    exposure = mDEserverRefNextShot * ((conSet.K2ReadMode * 10 + 1) * exposure + 3.);
    if (mParam->CamFlags & DE_CAM_CAN_COUNT)
      exposure *= mTD.FramesPerSec / 20.;
  }
  else if (mParam->DE_camType && conSet.K2ReadMode > 0)
    exposure *= 3 * conSet.K2ReadMode;
  if (mParam->STEMcamera && mParam->GatanCam && conSet.lineSync)
    exposure += mDMsizeY * mDSsyncMargin / 1.e6;
  mTD.cameraTimeout = (DWORD)(mTimeoutFactor * 1000. * (5. + mTD.DMsettling + exposure +
    megaVoxel / megaVoxPerSec));
  if (mParam->K2Type) {
    double asyncUsedUp = 0.;
    if (mLastAsyncTimeout)
      asyncUsedUp = SEMTickInterval(mAsyncTickStart);

    // K2 has a basic fixed time that depends on mode, but linear mode can have dark ref
    superRes = conSet.K2ReadMode == SUPERRES_MODE;
    mTD.cameraTimeout = (DWORD)((conSet.K2ReadMode == LINEAR_MODE ? 2. : 1.) * 
      mTimeoutFactor * 1000. * (mExposure + (superRes ? 20. : 15.)));
    if (mTD.DoseFrac) {

      // For K2 dose frac mode, add time per frame, plus time if aligning
      ind = (int)ceil(mExposure / conSet.frameTime);
      mTD.cameraTimeout += (DWORD)(mTimeoutFactor * 1000. * ind * (superRes ? 6. : 1.5));
      if (mTD.AlignFrames)
        mTD.cameraTimeout += (DWORD)(mTimeoutFactor * 1000. * ind * 
        (superRes ? 20. : 5.));

      // Allow time for saving too, at 10 MB/sec.  Superres is bytes, others are ints
      if (mTD.SaveFrames)
        mTD.cameraTimeout += (DWORD)(mTimeoutFactor * 1000. * ind * 
        (superRes ? 7. : 3.5));

      // If there is a leftover timeout from async shot, use it up for any dose frac shot
      DWORD saveTimeout = mTD.cameraTimeout;
      if (mLastAsyncTimeout && asyncUsedUp < mLastAsyncTimeout)
          mTD.cameraTimeout += mLastAsyncTimeout - (DWORD)asyncUsedUp;

      // If saving these frames async, then save the timeout and start time
      if (mTD.NumAsyncSumFrames >= 0) {
        mLastAsyncTimeout = saveTimeout;
        mAsyncTickStart = GetTickCount();
        mLastAsyncExpTime = (float)mExposure;
      }

      // For non dose-frac shot, we could add previous exposure time minus amount used up
      // to allow for the plugin waiting until previous capture is over for GMS < 2.31,
      // but keep it simple and just extend the timeout for every shot
    } else if (mLastAsyncTimeout && asyncUsedUp < mLastAsyncTimeout) {
      mTD.cameraTimeout += mLastAsyncTimeout - (DWORD)asyncUsedUp;
    }
    if (mDebugMode) {
      logmess.Format("K2 timeout %d  readmode %d  dosefrac %d  align %d", 
        mTD.cameraTimeout, conSet.K2ReadMode, mTD.DoseFrac?1:0, mTD.AlignFrames?1:0);
      mWinApp->AppendToLog(logmess, LOG_OPEN_IF_CLOSED);
    }
  }

  // For Falcon saving, allow ~8 MB/sec, data are saved as 4 byte ints
  if (mSavingFalconFrames)
    mTD.cameraTimeout += (DWORD)(mTimeoutFactor * 1000. * (mDMsizeX * mDMsizeY / 2.e6) *
    mFalconHelper->GetFrameTotals(conSet.summedFrameList, ind));

  // Add 4 minutes if tilting in blanker thread
  if (mTD.TiltDuringDelay)
    mTD.cameraTimeout += 240000;

  // Save stage and mag before starting, setup drift with IS and dynamic focus
  if (CapSaveStageMagSetupDynFocus(conSet, inSet))
    return;

  // Set timeout for blanker to basic value plus an amount per micron for stage move
  mTD.blankerTimeout = 0;
  if ((mTD.PreDarkBinning && mTD.PreDarkBlankTime) || mTD.UnblankTime || mTD.ReblankTime 
    || mTD.PostActionTime || mTD.ScanTime > 0. || mTD.FocusStep1)
    mTD.blankerTimeout = 15000;
  if (mTD.PostMoveStage && (mTD.MoveInfo.axisBits & (axisX | axisY)))
    mTD.blankerTimeout += (DWORD)(((JEOLscope && mScope->GetSimulationMode()) ? 
    1.0f : 0.25f) * mTD.MoveInfo.distanceMoved * 1000.);
  else if (mTD.PostMoveStage && (mTD.MoveInfo.axisBits && axisA))
    mTD.blankerTimeout += (DWORD)((JEOLscope ? 1000. : 200.) * 
      (2. + fabs(mTD.MoveInfo.alpha - mTiltBefore)));
  if (mTD.TiltDuringDelay)
    mTD.blankerTimeout += 240000;

  // Manage the dark and gain reference usage
  if (CapManageDarkGainRefs(conSet, inSet, bEnsureDark, gainXoffset, gainYoffset))
    return;

  mTD.Exposure = mExposure;
  if (mParam->FEItype == FALCON3_TYPE && FCAM_CAN_COUNT(mParam) && conSet.K2ReadMode > 0){
    ind = B3DNINT(mTD.Exposure / mFalconReadoutInterval);
    mTD.Exposure = mFalconReadoutInterval * (ind + ind / 32);
    SEMTrace('E', "Adjusted exposure time for lost frames in counting from %.3f to %.3f",
      mExposure, mTD.Exposure);
  }
  if (mParam->addToExposure > -1. && 
    mParam->addToExposure + mExposure > mParam->minExposure)
      mTD.Exposure += mParam->addToExposure;
  mTD.psa = NULL;
  mTD.Array[0] = NULL;
  mTD.Simulation = mSimulationMode;
  mTD.FauxCamera = IsCameraFaux();
  mTD.NumDRDelete = mNumDRdelete;
  mTD.DRdelete = mDRdelete;
  mTD.DMbeamShutterOK = mParam->DMbeamShutterOK;
  mTD.DMsettlingOK = mParam->DMsettlingOK;
  mTD.ProcessingPlus = mTD.Processing;
  mTD.PlugSTEMacquireFlags = (mBaseJeolSTEMflags & ~(0x3)) + 
    (mTD.DivideBy2 ? PLUGCAM_DIVIDE_BY2 : 0);
  mTD.integration = conSet.integration;
  mTD.PluginVersion = GetPluginVersion(mParam);
  
  if (mSingleContModeUsed == CONTINUOUS && mNeedShotToInsert < 0) {
    if (mParam->STEMcamera && mParam->GatanCam)
      mTD.ContinuousSTEM = mTD.ContinuousSTEM ? -1 : 1;
    else {
      mTD.ContinuousSTEM = 0;

      // For continuous mode in the plugin, set all the flags in the processing variable
      // This variable will serve as the flag that the mode is on
      if (((mParam->GatanCam && !mParam->STEMcamera && !conSet.doseFrac) ||
        (mTD.plugFuncs && mTD.plugFuncs->StopContinuous) || 
        (mParam->DE_camType >= 2 && !conSet.saveFrames)) &&
        mParam->useContinuousMode > 0) {
          mTD.ProcessingPlus += CONTINUOUS_USE_THREAD + 
            (mParam->setContinuousReadout ? CONTINUOUS_SET_MODE : 0) +
            (mParam->useFastAcquireObject ? CONTINUOUS_ACQUIS_OBJ : 0) +
            (mParam->continuousQuality << QUALITY_BITS_SHIFT);

          // Make sure the camera timeout is appropriately longer than the return time
          // Plus, if continuous has already started, the return time governs completely
          mTD.cameraTimeout = B3DMAX(mTD.cameraTimeout, 2 * CONTINUOUS_RETURN_TIMEOUT);
          if (mRepFlag >= 0) {
            mTD.cameraTimeout = 2 * CONTINUOUS_RETURN_TIMEOUT;
            mTD.NeedsReadMode = false;
            bEnsureDark = false;
          }
          mTD.PlugSTEMacquireFlags |= PLUGCAM_CONTINUOUS;
      }
    }

    // Start a counter and get start time for FPS output
    if (mRepFlag < 0)
      mContinuousCount = 0;
    else if (mContinuousCount == 1)
      mContinStartTime = GetTickCount();
    mRepFlag = inSet;
  } else {
    mRepFlag = -1;
    mTD.ContinuousSTEM = 0;
  }

  // Drift correction actually seems to work in continuous mode so allow it
  // Property files went out without BasicCorrections so we need to supply the 49 here
  if (OneViewDriftCorrectOK(mParam) && conSet.alignFrames && 
    mTD.Processing == GAIN_NORMALIZED) {
      if (mTD.Corrections <= 0)
        mTD.Corrections = 49;     
      mTD.Corrections |= OVW_DRIFT_CORR_FLAG;
  }
 
  // Record that a deferred sum is being started, or cancel its existence if doing dose 
  // frac or continuous shot
  if (mSetDeferredSize) {
    mXdeferredSizeTD = mTD.DMSizeX;
    mYdeferredSizeTD = mTD.DMSizeY;
    mXdeferredSize = mDMsizeX;
    mYdeferredSize = mDMsizeY;
    mReadModeDeferred = mTD.GatanReadMode;
    mBinDeferred = mBinning;
    mBinDeferredTD = mTD.Binning;
    mLeftDeferred = mLeft;
    mRightDeferred = mRight;
    mTopDeferred = mTop;
    mBotDeferred = mBottom;
    mAskedDeferredSum = true;
    mSavedInDeferred = mTD.SaveFrames;
    mAlignedDeferred = mTD.UseFrameAlign;
    mLastDeferredConSet = inSet;
    if (!mConsDeferred)
      mConsDeferred = new ControlSet;
    *mConsDeferred = conSet;
    mStartingDeferredSum = true;
  } else if (mTD.DoseFrac || (mTD.ProcessingPlus & CONTINUOUS_USE_THREAD)) {
    mAskedDeferredSum = false;
  }
 
  // Inform scope so it can unblank beam if in low dose mode; when noshutter = 1 this also
  // unblanks the beam and reblanks it in the call at the end
  // Don't do this if about to blank for dark ref
  if (!mScope->GetCameraAcquiring())
    mScope->SetCameraAcquiring(true, 
      B3DCHOICE(bEnsureDark && !mSimulationMode, 0.f, mParam->postBlankerDelay));
  
  if (bEnsureDark && !mSimulationMode) {
    CString message;
    if (mDebugMode) {
      DWORD ticks = GetTickCount();
      message.Format("%.3f start getting dark, %u elapsed", 
        0.001 * (ticks % 3600000), ticks - mStartTime);
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
    }

    // Set flag and start thread for ensuring dark
    mEnsuringDark = true;
    mAverageDarkCount = 0;
    mBadDarkCount = 0;
    
    // Need to blank beam during this operation
    if (!mScope->GetBlankSet()) {
      mScope->BlankBeam(true);
      if (mParam->postBlankerDelay >= 0.001)
        Sleep(B3DNINT(1000. * mParam->postBlankerDelay));
    }
    StartEnsureThread(mTD.cameraTimeout);
    mWinApp->UpdateBufferWindows();
    return;
  }
  
  StartAcquire();
}

///////////////////////////////////////////////////////
//  Support routines for Capture 
///////////////////////////////////////////////////////

// Load the selected channels into TD, and manage selection of JEOL or setup insertion for
// FEI detectors
int CCameraController::CapSetupSTEMChannelsDetectors(ControlSet &conSet, int inSet, 
                                                     BOOL retracting)
{
  int block, ind, chan, i;
  mTD.NumChannels = 1;
  if (mParam->STEMcamera && !retracting) {

    // get the channel index and names into the thread data
    mTD.NumChannels = 0;
    block = GetMaxChannels(mParam);
    block = B3DMIN(block, mMaxChannelsToGet);
    if (mRequiredRoll > 0 || (mSingleContModeUsed == CONTINUOUS && !mParam->FEItype))
      block = 1;
    for (chan = 0; chan < mParam->numChannels && mTD.NumChannels < block; chan++) {
      ind = conSet.channelIndex[chan];
      for (i = 0; i < mTD.NumChannels && ind >= 0; i++)
        if (MutuallyExclusiveChannels(mTD.ChannelIndex[i], ind))
          ind = -1;
      if (ind >= 0 && ind < MAX_STEM_CHANNELS && mParam->availableChan[ind]) {
        mTD.ChannelIndex[mTD.NumChannels] = ind;
        mTD.ChanIndOrig[mTD.NumChannels] = ind;
        mTD.Array[mTD.NumChannels] = NULL;
        mTD.ChannelName[mTD.NumChannels++] = mParam->detectorName[ind];
      }
    }
    if (!mTD.NumChannels) {
      SEMMessageBox("No available STEM channels were selected in this parameter set",
        MB_EXCLAME);
      ErrorCleanup(1);
      return 1;
    }

    // Select the detectors for JEOL
    if (JEOLscope) {
      if (!mScope->SelectJeolDetectors((int *)mTD.ChannelIndex, mTD.NumChannels)) {
        ErrorCleanup(1);
        return 1;
      }

      // Remap the channel index array into DS channels
      if (mParam->GatanCam)
        for (chan = 0; chan < mTD.NumChannels; chan++)
          mTD.ChannelIndex[chan] = ChannelMappedTo(mTD.ChannelIndex[chan]);

    } else {

      // Check for need to insert FEI detectors.  This seems to apply to any detector
      // selected in TUI, so need to check all available ones
      for (ind = 0; ind < mParam->numChannels; ind++) {
        if (mParam->availableChan[ind] && !mFEIdetectorInserted[ind] && 
          mParam->needShotToInsert[ind])
          mNeedShotToInsert = inSet;
      }
    }
  }
  return 0;
}

// Take care of raising or lowering screen
int CCameraController::CapManageScreen(int inSet, BOOL retracting, int numActive)
{
  // If not in screen down mode, and not just retracting, manage screen
  // Raise the screen for STEM camera if screen switches STEM
  if (!mScreenDownMode && !retracting && /*!IsCameraFaux() &&*/ !mScope->GetNoScope() && 
    (!mParam->STEMcamera || mWinApp->DoSwitchSTEMwithScreen() || mLowerScreenForSTEM < 0
    || mWinApp->DoDropScreenForSTEM())) {
      int pos = mScope->FastScreenPos();
      int newpos = -999;

      // First manage STEM mode and the screen down flag for case where screen has
      // to be down.  Turn off the flag then make sure STEM is on
      if (mParam->STEMcamera && mWinApp->DoDropScreenForSTEM() && 
        mWinApp->DoSwitchSTEMwithScreen()) {
          mWinApp->SetScreenSwitchSTEM(false);
          if (pos != spUp)
            mScope->SetSTEM(true);
          mWinApp->mSTEMcontrol.UpdateSettings();
          mWinApp->mLowDoseDlg.Update();
      }

      // If is a STEM and screen needs to be down, lower it
      if (mParam->STEMcamera && mWinApp->DoDropScreenForSTEM()) {
        if (pos != spDown) {
          SEMTrace('R', "Lowering screen for STEM %d", pos);
          newpos = spDown;
        }

        // If is a STEM and screen needs to be down, lower it
      } else if (mParam->STEMcamera && mLowerScreenForSTEM < 0) {
        if (pos != spUp) {
          SEMTrace('R', "Raising screen for STEM %d", pos);
          newpos = spUp;
        }

        // Otherwise if it is not a side-mount, raise the screen
      } else if (!mParam->sideMount && pos != spUp) {
        SEMTrace('R', "Raising screen for regular %d", pos);
        newpos = spUp;
      } else if (mParam->sideMount && pos != spDown) {

        // Otherwise see if a lower camera is thought to be inserted or is not 
        // retractable and lower the screen
        for (int iCam = 0; iCam < numActive; iCam++) {
          CameraParameters *camP = &mAllParams[mActiveList[iCam]];
          if (camP != mParam && camP->order > mParam->order && 
            (camP->canBlock || !camP->retractable)) 
            newpos = spDown;
        }
      }
      SEMTrace('R', "Checking screen: sidemount %d  pos %d newpos %d", 
        mParam->sideMount?1:0, pos, newpos);
      if (newpos > -999) {
        mScope->SetScreenPos(newpos);
        mRaisingScreen = (newpos == spUp) ? 1 : -1;
        mWinApp->AddIdleTask(ThreadBusy, ScreenOrInsertDone,ScreenOrInsertError,
          inSet, 0);
        mWinApp->SetStatusText(SIMPLE_PANE, newpos == spUp ? 
          "RAISING SCREEN" : "LOWERING SCREEN");
        mWinApp->UpdateBufferWindows();
        return 1;
      }
  }
  return 0;
}

// Take care of insertion and retract of other cameras, checking temperature, and setting
// the saving of frames from K2.  These are the things that need a camera instance
int CCameraController::CapManageInsertTempK2Saving(const ControlSet &conSet, int inSet, 
                                                   BOOL retracting, int numActive)
{
  BOOL blockable, STEMretract, insertingOther;
  bool aligning;
  int iCam, camInd;
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  int curCam = mWinApp->GetCurrentCamera();
  mSetDeferredSize = false;

  // If not in simulation mode or screen down, make sure camera is inserted
  // and any cameras above are retracted
  // First see if any can block current camera
  blockable = false;
  STEMretract = mParam->STEMcamera && !mWinApp->GetMustUnblankWithScreen() && 
    mWinApp->GetRetractToUnblankSTEM();
  for (iCam = 0; iCam < numActive; iCam++) {
    camInd = mActiveList[iCam];
    CameraParameters *camP = &mAllParams[camInd];
    if (camP != mParam && curCam != camP->samePhysicalCamera && 
      camInd != mParam->samePhysicalCamera && camInd != mParam->insertingRetracts &&
      (camP->order <= mParam->order || STEMretract) && camP->canBlock) 
      blockable = true;
  }

  // Go into this insertion state checking if we need to check temperature, if the camera
  // could be blocked, if it is retractable, if we are retracting all.  It used to test
  // if Gatan/AMT camera instead of testing retractable; this test should now be redundant
  // but at least don't check a GIF camera that can't be blocked
  if ((!mScreenDownMode || mParam->checkTemperature) && !mSimulationMode &&
    (mParam->checkTemperature || blockable || (mParam->DMCamera && !mParam->GIF) || 
    mParam->retractable || retracting || mNeedToSelectDM || mParam->alsoInsertCamera >= 0
    || (mParam->K2Type && conSet.doseFrac && conSet.saveFrames))) {
      if (mNumDMCameras[COM_IND] > 0 && CreateCamera(CREATE_FOR_ANY)) {
        ErrorCleanup(1);
      return 1;
      }
      BOOL justReturn = false;
      try {
        long inserted;
        // Try having this be the only place this is called except for select before
        // complex shots.  It is needed in case a new DM is started
        if (mParam->DMCamera) {
          MainCallDMCamera(SetCurrentCamera(mTD.SelectCamera));
        }
        mNeedToSelectDM = false;

        // first scan active cameras for ones that can block and are not same physical
        // camera
        for (iCam = 0; iCam < numActive; iCam++) {
          camInd = mActiveList[iCam];
          CameraParameters *camP = &mAllParams[camInd];
          insertingOther = camInd == mParam->alsoInsertCamera && !retracting;

          if ((((camP != mParam && curCam != camP->samePhysicalCamera && 
            camInd != mParam->samePhysicalCamera && camInd != mParam->insertingRetracts &&
            (camP->order <= mParam->order || STEMretract)) || inSet == RETRACT_ALL) && 
            camP->canBlock) || insertingOther) {
              inserted = insertingOther ? 1 : 0;
              TestCameraInserted(iCam, inserted);

              // Retract if it is known to be inserted
              // We used to do it if it was last thought to be in and we are just retracting
              if (inserted != (insertingOther ? 1 : 0)) {//|| inSet == RETRACT_BLOCKERS) {

                // Set flag and setup thread parameters.  Blank if necessary
                SEMTrace('R', "Camera # %d is %s it", iCam, 
                  insertingOther ? "retracted; inserting" : "inserted; retracting");
                mInserting = true;
                mITD.camera = camP->cameraNumber;
                mITD.DMindex = CAMP_DM_INDEX(camP);
                mITD.DE_camType = camP->DE_camType;
                mITD.FEItype = FCAM_ADVANCED(camP) ? camP->FEItype : 0;
                if (mITD.FEItype)
                  mITD.camera = camP->eagleIndex;
                mITD.plugFuncs = mPlugFuncs[mActiveList[iCam]];
                mITD.delay = (int)(1000. * camP->retractDelay);
                mITD.insert = insertingOther;
                if (mBlankWhenRetracting && !mParam->noShutter && !insertingOther) {
                  mScope->SetCameraAcquiring(true);
                  mScope->BlankBeam(true);
                }

                // If just retracting, better clear the block flag now to avoid a loop
                if (retracting)
                  camP->canBlock = false;
                mWinApp->SetStatusText(SIMPLE_PANE, 
                  mBlankWhenRetracting ? "BLANKED REMOVAL"
                  : (insertingOther ? "INSERTING CAMERA" : "REMOVING CAMERA"));
                break;
              } else 

                // clear the block flag when it is seen not to block
                camP->canBlock = insertingOther;
          }

          // Now if there is a GIF that can blank beam, insert TV camera, set flag
          // negative to indicate it is taken care of
          if (iCam != mTD.Camera && camP->GIF && camP->useTVToUnblank > 0) {
            CString strTVIn = "if(IFCGetTVIn() == 0)\n  IFCSetTVIn(1)";
            try {
              EasyRunScript(strTVIn, 0, sGIFisSocket);
            }
            catch (_com_error E) {
              SEMMessageBox("A COM error occurred trying to put TV camera in.\n\n"
                "If this is because Filter Control is not running,\n"
                "just proceed and see if you get a good image.\n\n"
                "If Filter Control is not running and you still have\n"
                "problems, close DigitalMicrograph, then start\n"
                "Filter Control and restart DigitalMicrograph", MB_EXCLAME);
            }
            camP->useTVToUnblank = -1;
          }
        }

        // Now if not retracting, check for insertion of current camera
        // Skip if continuous mode has already started; that falg is set after this
        // function on the first shot of continuous
        if (!mInserting && mParam->retractable && !retracting && mRepFlag < 0) {

          // Either we are going to insert it or it already is, so set block flag
          mAllParams[mWinApp->GetCurrentCamera()].canBlock = true;

          if (mDebugMode) {
            CString message;
            message.Format("%.3f start asking if inserted", 
              0.001 * (GetTickCount() % 3600000));
            mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
          }

          // If it is not inserted, set flag and thread parameters
          inserted = 1;
          TestCameraInserted(mTD.Camera, inserted);
          if (!inserted) {
            mInserting = true;
            mITD.camera = mTD.SelectCamera;
            mITD.DMindex = CAMP_DM_INDEX(mParam);
            mITD.DE_camType = mParam->DE_camType;
            mITD.FEItype = mParam->FEItype;
            mITD.plugFuncs = mTD.plugFuncs;
            if (mTD.plugFuncs)
              mITD.camera = mParam->cameraNumber;
            if (mParam->FEItype)
              mITD.camera = mParam->eagleIndex;
            mITD.delay = (int)(1000. * mParam->insertDelay);
            mITD.insert = true;
            mWinApp->SetStatusText(SIMPLE_PANE, "INSERTING CAMERA");
          }
        }

        // Start thread if needed
        if (mInserting) {
          mInsertThread = AfxBeginThread(InsertProc, &mITD,
            THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
          SEMTrace('R', "InsertProc created with ID 0x%0x",mInsertThread->m_nThreadID);
          mInsertThread->m_bAutoDelete = false;
          mInsertThread->ResumeThread();
          mWinApp->AddIdleTask(ThreadBusy, ScreenOrInsertDone,
            ScreenOrInsertError, inSet, 0);
          mWinApp->UpdateBufferWindows();
          justReturn = true;
        }
      }
      catch (_com_error E) {
        SEMReportCOMError(E, _T("getting camera insertion position "));
        justReturn = true;
        ErrorCleanup(1);
      }

      // Now check temperature if required
      if (!justReturn && mParam->checkTemperature) {
        try {
          CString command;
          command.Format("%s %d)\n"
            "Number target\n"
            "Number stable = CM_IsTemperatureStable(camera, target)\n"
            "Exit(stable)", sGetCamObjText, mTD.SelectCamera);
          if (B3DNINT(EasyRunScript(command, 0, mParam->useSocket ? SOCK_IND : COM_IND))){
            mParam->checkTemperature = false;
          } else {
            AfxMessageBox("The camera temperature is not yet stable", MB_EXCLAME);
            justReturn = true;
            ErrorCleanup(1);
          }
        } 
        catch (_com_error E) {
          SEMReportCOMError(E, _T("checking whether camera temperature is stable "));
          justReturn = true;
          mParam->checkTemperature = false;
          ErrorCleanup(1);
        }
      }

      // Now set up for K2 frame saving or aligning
      aligning = mParam->K2Type && conSet.doseFrac && conSet.alignFrames && 
        (conSet.useFrameAlign == 1 || 
        (conSet.useFrameAlign && conSet.saveFrames && !mAlignWholeSeriesInIMOD)) &&
        CAN_PLUGIN_DO(CAN_ALIGN_FRAMES, mParam);
      if (!justReturn && mParam->K2Type && conSet.doseFrac && 
        (conSet.saveFrames || aligning)) {
          if (SetupK2SavingAligning(conSet, inSet, conSet.saveFrames != 0, aligning,
            NULL)) {
              justReturn = true;
              ErrorCleanup(1);
          }
      }

      if (mNumDMCameras[COM_IND] > 0)
        ReleaseCamera(CREATE_FOR_ANY);
      if (justReturn)
        return 1;
  }
  return 0;
}

// Routine that sets up K2 saving or aligning or writing of a COM file after saving;
// it must be called after creating a camera object
int CCameraController::SetupK2SavingAligning(const ControlSet &conSet, int inSet, 
  bool saving, bool aligning, CString *aliComRoot)
{
  int ldArea, magIndex, sizeX, sizeY, numAllVsAll, refineIter, groupSize, doSpline;
  int notOK, newIndex, faInd, numFilt = 1, deferGpuSum = 0, flags = 0;
  int frameSizeX, frameSizeY, numAliFrames, frameStartEnd = 0;
  int DMind = CAMP_DM_INDEX(mParam);
  double maxMemory = pow(1024., 3.) * mK2MaxRamStackGB;
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  FrameAliParams faParam;
  bool alignSubset = false;
  bool isSuperRes = conSet.K2ReadMode == SUPERRES_MODE;
  bool trulyAligning = aligning && conSet.useFrameAlign == 1;
  CString mess;
  if (aligning) {
    faInd = conSet.faParamSetInd;
    B3DCLAMP(faInd, 0, (int)mFrameAliParams.GetSize() - 1);
    mess = "WARNING: ";
    if (!mWinApp->mMacroProcessor->SkipCheckingFrameAli()) {
      notOK = UtilFindValidFrameAliParams(conSet.K2ReadMode, conSet.useFrameAlign,
        faInd, newIndex, &mess);
      if (notOK || newIndex != faInd) {
        mWinApp->AppendToLog(mess);
        if (notOK > 0)
          mWinApp->AppendToLog("Using the frame alignment parameter set anyway");
        else
          PrintfToLog("Using the %s suitable frame alignment parameter set, %s",
          notOK < 0 ? "first" : "one", (LPCTSTR)mFrameAliParams[newIndex].name);
        faInd = newIndex;
      }
    }
    faParam = mFrameAliParams[faInd];
  }

  // Get pixel size from mag we are going to be in after changing low dose area
  if (mScope->GetLowDoseMode()) {
    ldArea = ConSetToLDArea(inSet);
    magIndex = ldParam[ldArea].magIndex;
  } else
    magIndex = mScope->FastMagIndex();
  double pixel = 10000. * mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), 
    magIndex) * (isSuperRes ? 1 : 2);
  if (aligning && conSet.useFrameAlign > 1 && mOneK2FramePerFile) {
    SEMMessageBox("You cannot save a com file for running\n"
      "alignframes when saving one frame per file");
    return 1;
  } else if (conSet.saveFrames && mDirForK2Frames.IsEmpty()) {
    SEMMessageBox("You must specify a directory for saving the frame images");
    return 1;
  }

  int numFrames = B3DNINT(conSet.exposure / conSet.frameTime);
  double numAsyncSum = 0.;
  CString sdir, root;
  char defName[50];

  if (saving) {

    // Set up the file and directory names.
    ComposeFramePathAndName(false);
    mPathForFrames.Format("%s\\%s", (LPCTSTR)mFrameFolder, 
      (LPCTSTR)mFrameFilename);
    if (mOneK2FramePerFile) {
      sdir = mPathForFrames;
      root = "K";
      mPathForFrames += "\\";
    } else {
      sdir = mFrameFolder;
      root = mFrameFilename;
      if (mK2SaveAsTiff)
        mPathForFrames += ".tif";
      else if (mNameFramesAsMRCS && CAN_PLUGIN_DO(CAN_SET_MRCS_EXT, mParam)) {
        mPathForFrames += ".mrcs";
        flags |= K2_MRCS_EXTENSION;
      } else
        mPathForFrames += ".mrc";
    }
  }

  // Set various flags and get lengths of string pieces
  int sdlen = sdir.GetLength() + 1;
  int rootlen = root.GetLength() + 1;
  int nameSize = sdlen + rootlen + 4;
  int stringSize = 4;
  int reflen = 0, comlen = 0, defectLen = 0, sumLen = 0;
  int alignFlags = 0, gpuFlags = 0, aliRefLen = 0, aliComLen;
  CString refFile, sumList, tmpStr, aliComName;
  if (isSuperRes)
    refFile = mParam->superResRefForK2;
  else if (conSet.K2ReadMode == COUNTING_MODE)
    refFile = mParam->countingRefForK2;
  if (mSaveRawPacked & 1)
    flags |= K2_SAVE_RAW_PACKED;
  if ((mSaveRawPacked & 1) && (mSaveRawPacked & 2) && 
      CAN_PLUGIN_DO(4BIT_101_COUNTING, mParam))
    flags |= K2_RAW_COUNTING_4BIT;
  if ((mSaveRawPacked & 1) && mUse4BitMrcMode && !mK2SaveAsTiff &&
      CAN_PLUGIN_DO(4BIT_101_COUNTING, mParam))
    flags |= K2_SAVE_4BIT_MRC_MODE;
  if (mSaveTimes100 && conSet.processing == GAIN_NORMALIZED && 
      CAN_PLUGIN_DO(SAVES_TIMES_100, mParam))
    flags |= K2_SAVE_TIMES_100;
  if (!refFile.IsEmpty()) {
    flags |= K2_COPY_GAIN_REF;
    reflen = refFile.GetLength() + 1;
    nameSize += reflen;

    // Older plugin copied if flag set regardless of DS versus unproc
    if (conSet.processing != GAIN_NORMALIZED) {
      alignFlags |= K2_COPY_GAIN_REF;
      stringSize += reflen;
      aliRefLen = reflen;
    }
  }
  if (mK2SaveAsTiff == 1)
    flags |= K2_SAVE_LZW_TIFF;
  else if (mK2SaveAsTiff == 2)
    flags |= K2_SAVE_ZIP_TIFF;
  if (mK2SynchronousSaving) {
    flags |= K2_SAVE_SYNCHRON;
    alignFlags |= K2_SAVE_SYNCHRON;
  }
  if (mSkipK2FrameRotFlip && mParam->rotationFlip)
    flags |= K2_SKIP_FRAME_ROTFLIP;
  if ((saving || aligning) && conSet.processing != GAIN_NORMALIZED && 
    mDMversion[DMind] >= DM_RETURNS_DEFECT_LIST) {
      flags |= K2_SAVE_DEFECTS;
      alignFlags |= K2_SAVE_DEFECTS;

      // Check for a new reference if enough idle time has passed and if it is new,
      // fetch a new defect list and merge it into original list
      if (SEMTickInterval(1000. * mLastImageTime) / 1000. > mGainTimeLimit &&
        mWinApp->mGainRefMaker->IsDMReferenceNew(mWinApp->GetCurrentCamera())) {
          SEMTrace('r', "Getting new defect list");
          GetMergeK2DefectList(CAMP_DM_INDEX(mParam), mParam, true);
          mParam->defectString.clear();
      }

      // Save defects for new GMS, non-normalized images
      // Make up a defect string if it is empty or if the current rotation/flip 
      // for frames does not match that used when making the string, or if one was made
      // up just for aligning and now one is needed with a name for saving
      if (mParam->defectString.empty() || (!mParam->defNameHasFrameFile && 
        !mFrameFilename.IsEmpty())|| (mSkipK2FrameRotFlip ? 0 : mParam->rotationFlip) != 
        mParam->defStringRotFlip) {
          sprintf(defName, "#defects_%s.txt\n", (LPCTSTR)mFrameFilename);
          mParam->defectString = defName;
          SEMTrace('r', "Made new defect string with name %s", defName);
          mParam->defNameHasFrameFile = !mFrameFilename.IsEmpty();
          if (flags & K2_SKIP_FRAME_ROTFLIP) {
            CameraDefects defects = mParam->defects;
            sizeX = (mParam->rotationFlip % 2) ? mParam->sizeY : mParam->sizeX;
            sizeY = (mParam->rotationFlip % 2) ? mParam->sizeX : mParam->sizeY;
            CorDefRotateFlipDefects(defects, 0, sizeX, sizeY);
            CorDefDefectsToString(defects, mParam->defectString, sizeX, sizeY);
            mParam->defStringRotFlip = 0;
          } else {
            CorDefDefectsToString(mParam->defects, mParam->defectString,
              mParam->sizeX, mParam->sizeY);
            mParam->defStringRotFlip = mParam->rotationFlip;
          }
      }
      defectLen = (int)mParam->defectString.length() + 1;
      nameSize += defectLen;
      stringSize += defectLen;
  }
  if (mRunCommandAfterSave) {
    flags |= K2_RUN_COMMAND;
    comlen = mPostSaveCommand.GetLength() + 1;
    nameSize += comlen;
  }
  if (saving && conSet.sumK2Frames && conSet.summedFrameList.size() >
      0 && CAN_PLUGIN_DO(CAN_SUM_FRAMES, mParam)) {
      flags |= K2_SAVE_SUMMED_FRAMES;
      for (int frm = 0; frm < (int)conSet.summedFrameList.size() / 2; frm++) {
        tmpStr.Format("%d %d", conSet.summedFrameList[frm *2], 
          conSet.summedFrameList[frm * 2 + 1]);
        if (frm)
          sumList += " ";
        sumList += tmpStr;
      }
      sumLen = sumList.GetLength() + 1;
      nameSize += sumLen;
  }

  // Set up for align com file if one is needed
  if (aligning && conSet.useFrameAlign > 1) {
    if (aliComRoot)
      aliComName = B3DCHOICE(mComPathIsFramePath, mFrameFolder, mAlignFramesComPath) + 
      "\\" + *aliComRoot + ".pcm";
    else
      aliComName.Format("%s\\%s.pcm", B3DCHOICE(mComPathIsFramePath || 
      mAlignFramesComPath.IsEmpty(), (LPCTSTR)mFrameFolder, (LPCTSTR)mAlignFramesComPath),
      (LPCTSTR)mFrameFilename);
    aliComLen = aliComName.GetLength() + 1;
    stringSize += aliComLen;
    alignFlags |= K2_MAKE_ALIGN_COM;
  }

  // Get array for saving and pack strings into it
  char *names = NULL;
  char *strings = NULL;
  long setupErr;
  float radius2[4], totAliMem = 0.;
  if (saving) {
    nameSize /= 4;
    names = new char[4 * nameSize];
    sprintf(names, "%s", (LPCTSTR)sdir);
    sprintf(names + sdlen, "%s", (LPCTSTR)root);
    if (reflen)
      sprintf(names + sdlen + rootlen, "%s", (LPCTSTR)refFile);
    if (defectLen)
      sprintf(names + sdlen + rootlen + reflen, "%s", 
      mParam->defectString.c_str());
    if (comlen)
      sprintf(names + sdlen + rootlen + reflen + defectLen, "%s", 
      (LPCTSTR)mPostSaveCommand);
    if (sumLen)
      sprintf(names + sdlen + rootlen + reflen + defectLen + comlen, "%s", 
      (LPCTSTR)sumList);
  }

  mGettingFRC = false;
  if (aligning) {
    numAliFrames = numFrames;
    if (saving && faParam.alignSubset && CAN_PLUGIN_DO(CAN_ALIGN_SUBSET, mParam)) {
      mAlignStart = B3DMAX(1, B3DMIN(faParam.subsetStart, K2FA_SUB_START_MASK));
      mAlignEnd = B3DMIN(numFrames, faParam.subsetEnd);
      if (mAlignEnd - mAlignStart > 0) {
        frameStartEnd = mAlignStart + (mAlignEnd << K2FA_SUB_END_SHIFT);
        numAliFrames = mAlignEnd + 1 - mAlignStart;
        alignSubset = true;
        if (trulyAligning)
          mNumSubsetAligned = numAliFrames;
      }
    }

    // Get array for aligning and pack strings into it
    stringSize /= 4;
    strings = new char[4 * stringSize];
    if (aliRefLen)
      sprintf(strings, "%s", (LPCTSTR)refFile);
    if (defectLen)
      sprintf(strings + aliRefLen, "%s", mParam->defectString.c_str());
    if (aliComLen)
      sprintf(strings + aliRefLen + defectLen, "%s", (LPCTSTR)aliComName);

    // Set variables for the call
    numAllVsAll = NumAllVsAllFromFAparam(faParam, numAliFrames, groupSize, refineIter, 
      doSpline, numFilt, radius2);
    mFalconHelper->GetSavedFrameSizes(mParam, &conSet, frameSizeX, frameSizeY);

     // Set some align flags
    if (faParam.hybridShifts && numFilt > 1)
      alignFlags |= K2FA_USE_HYBRID_SHIFTS;
    if (faParam.groupRefine && groupSize > 1 && refineIter > 0)
      alignFlags |= K2FA_GROUP_REFINE;
    if (doSpline)
      alignFlags |= K2FA_SMOOTH_SHIFTS;
    if (alignSubset)
      alignFlags |= K2FA_ALIGN_SUBSET;

    // Do things when truly aligning
    if (trulyAligning) {

      // Let plugin figure out whether it needs to do anything with this
      if (faParam.keepPrecision)
        alignFlags |= K2FA_KEEP_PRECISION;

      // Evaluate all memory needs
      totAliMem = UtilEvaluateGpuCapability(frameSizeX, 
        frameSizeY, conSet.binning / (isSuperRes ? 1 : 2), 
        faParam, numAllVsAll, numAliFrames, refineIter, groupSize, numFilt, doSpline, 
        mUseGPUforK2Align[DMind] ? mGpuMemory[DMind] : 0., maxMemory, gpuFlags, 
        deferGpuSum, mGettingFRC);
      if (totAliMem > maxMemory)
        PrintfToLog("WARNING: With current parameters, memory needed for aligning "
        "(%.1f GB) exceeds allowed\r\n  memory usage (%.1f GB), which is controlled by"
        " the K2MaxRamStackGB property", totAliMem, mK2MaxRamStackGB);

    } else {

      // If writing a com file, just send any flag
      if (mUseGPUforK2Align[DMind])
        gpuFlags = GPU_FOR_SUMMING;
      if (faParam.outputFloatSums)
        alignFlags |= K2FA_KEEP_PRECISION;
    }
    
    if (mGettingFRC)
      alignFlags |= K2FA_MAKE_EVEN_ODD;
    if (deferGpuSum)
      alignFlags |= K2FA_DEFER_GPU_SUM;
  }

  // Manage asynchronous acquire variables for early return
  if (mNextAsyncSumFrames >= 0) {
    mTD.NumAsyncSumFrames = mNextAsyncSumFrames;
    numAsyncSum = mNextAsyncSumFrames;
    flags |= K2_EARLY_RETURN;
    alignFlags |= K2_EARLY_RETURN;
    if (mDeferSumOnNextAsync)
      flags |= K2_MAKE_DEFERRED_SUM;
    if (mDeferSumOnNextAsync || aligning) {
      mSetDeferredSize = true;
    }
  }

  // Manage flag for gain normalizing a non-norm dose frac shot
  if (((mDMversion[DMind] >= PLUGIN_CAN_GAIN_NORM && conSet.processing == DARK_SUBTRACTED)
    || (conSet.processing == UNPROCESSED && mDMversion[DMind] >= PLUGIN_UNPROC_LIKE_DS))&&
    conSet.K2ReadMode > 0 && mNextAsyncSumFrames != 0 && reflen && !mNoNormOfDSdoseFrac) {
      flags |= K2_GAIN_NORM_SUM;
      mK2GainRefTimeStamp[DMind][isSuperRes ? 1 : 0] = 
        mWinApp->MinuteTimeStamp();
  }

  // For new K2 API, determine whether to do asynchronous to RAM and how many
  // frames to grab into stack
  if (mDMversion[DMind] >= DM_K2_API_CHANGED_LOTS && !mK2SynchronousSaving) {
    int maxInRAM, numGrab;
    int numSuperPix = (conSet.right - conSet.left) * (conSet.bottom - conSet.top);
    double frameInRAM = numSuperPix / 2.;
    double frameInGrab = numSuperPix / (isSuperRes ? 1. : 2.);

    // Reduce memory by amount needed for alignment
    // If there is a grab stack and we are keeping precision in alignment, need to double
    // both; but if saving times 100, that is sufficient and it doubles only superres
    maxMemory = B3DMAX(0., maxMemory - totAliMem * pow(1024., 3.));
    if (conSet.processing == GAIN_NORMALIZED) {
      if (trulyAligning && faParam.keepPrecision && !(saving && mSaveTimes100))
        frameInGrab *= 2;
      if (saving && mSaveTimes100 && isSuperRes)
        frameInGrab *= 2;
    }

    // If forming a full sum, there is no grab stack and usage is limited by 
    // the original RAM stack; otherwise (early return) it is limited by the 
    // size of the grabbed frames
    if (mNextAsyncSumFrames < 0)
      maxInRAM = (int)(maxMemory / frameInRAM);
    else
      maxInRAM = (int)B3DMAX(1., (maxMemory / frameInGrab));

    // Do asynchronous to RAM if it fits and if the frame time meets the 
    // minimum
    if (conSet.frameTime >= mK2MinFrameForRAM - 0.001 && numFrames <= maxInRAM) {
      flags |= K2_ASYNC_IN_RAM;
      alignFlags |= K2_ASYNC_IN_RAM;
    } else
      frameInRAM = 0.;
    if (mNextAsyncSumFrames >= 0) {
      numGrab = B3DMIN(numFrames, maxInRAM);
      numAsyncSum += 65536. * numGrab;
    }
  }

  try {
    if (saving)
      MainCallGatanCamera(SetupFileSaving2(mParam->rotationFlip, mOneK2FramePerFile, 
      pixel, flags, numAsyncSum, 0., 0., 0., nameSize, (long *)names, &setupErr));
    if (aligning) {
      MainCallGatanCamera(SetupFrameAligning(
        mFalconHelper->GetFrameAlignBinning(faParam, frameSizeX, frameSizeY),
        faParam.rad2Filt1, numAllVsAll ? faParam.rad2Filt2 : 0., 
        numAllVsAll ? faParam.rad2Filt3 : 0., faParam.sigmaRatio, 
        faParam.truncate ? faParam.truncLimit : 0., alignFlags, gpuFlags, numAllVsAll,
        groupSize, faParam.shiftLimit, faParam.antialiasType, refineIter,
        faParam.stopIterBelow, faParam.refRadius2, (long)(numAsyncSum + 0.1),
        frameStartEnd, 0, 0., stringSize, (long *)strings, &setupErr));
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("trying to set up frame saving or aligning "));
    setupErr = -1;
  }
  if (setupErr > 0) {
    sdir.Format("Error %d trying to set up frame saving or aligning:\n%s", 
      setupErr, SEMCCDErrorMessage(setupErr));
    SEMMessageBox(sdir);
  }
  delete [] names;
  delete [] strings;
  return setupErr != 0 ? 1 : 0;
}

// Compute the numAllVsAll variable for aligning based on expected # of frames and
// sort and handle the filters, set a couple of other flags
int CCameraController::NumAllVsAllFromFAparam(FrameAliParams &faParam, int numAliFrames, 
  int &groupSize, int &refineIter, int &doSpline, int &numFilters, float *radius2)
{
  int numGroups, ndata, ind;
  int numAllVsAll = faParam.numAllVsAll;
  if (faParam.strategy == FRAMEALI_HALF_PAIRWISE)
    numAllVsAll = B3DMAX(7, numAliFrames / 2);
  else if (faParam.strategy == FRAMEALI_ALL_PAIRWISE)
    numAllVsAll = numAliFrames;
  else if (faParam.strategy == FRAMEALI_ACCUM_REF)
    numAllVsAll = 0;
  groupSize = (faParam.useGroups && numAllVsAll) ? faParam.groupSize : 1;
  if (groupSize > 1) {
    numGroups = numAliFrames + 1 - groupSize;
    if ((numGroups + 1 - groupSize) * (numGroups - groupSize) / 2 < numGroups)
      groupSize = 1;
    numAllVsAll += groupSize - 1;
  }
  mTypeOfAlignError = -1;
  if (numAllVsAll) {
    ndata = B3DMIN(numAllVsAll, numAliFrames) + 1 - groupSize;
    mTypeOfAlignError = B3DCHOICE(((ndata + 1 - groupSize) * (ndata - groupSize)) / 2 >= 
      2 * ndata, 1, 0);
  }

  refineIter = faParam.doRefine ? faParam.refineIter : 0;
  doSpline = (faParam.doSmooth && numAliFrames >= faParam.smoothThresh) ? 1 : 0;
  numFilters = 1;
  radius2[0] = faParam.rad2Filt1;
  radius2[1] = faParam.rad2Filt2;
  radius2[2] = faParam.rad2Filt3;
  radius2[3] = faParam.rad2Filt4;
  if (!radius2[0])
    radius2[0] = 0.06f;
  for (ind = 1; ind < 4; ind++) {
    if (radius2[ind] <= 0)
      break;
    numFilters++;
  }
  if (numFilters > 1)
    rsSortFloats(radius2, numFilters);

  return numAllVsAll;
}

// Set the low dose area, setup the energy filter, and check for timeouts and settling
int CCameraController::CapSetLDAreaFilterSettling(int inSet)
{
  int ldArea, filtErr;
  CString logmess;

  // If in low dose and this is not the settling re-entry, set the acquisition area
  // unless it matches what we were just told about
  if (mSettling < 0 && mScope->GetLowDoseMode()) {

    // translate sets appropriately
    ldArea = ConSetToLDArea(inSet);
    if (mOppositeAreaNextShot)
      mScope->NextLDAreaOpposite();
    if (ldArea != mLDwasSetToArea)
        mScope->GotoLowDoseArea(ldArea);
    mOppositeAreaNextShot = false;
    mLDwasSetToArea = -1;
  }
  mSettling = -1;
  
  // If not in simulation mode and this is a GIF, make sure parameters are set
  // (User might have changed through other programs)
  if (!mSimulationMode && !(mRepFlag >= 0 && mContinuousCount > 0) && 
    (mParam->GIF || mScope->GetHasOmegaFilter())) {
    if ((filtErr = SetupFilter(true)) != 0 && !(filtErr > 0 && mAllowSpectroscopyImages)){
      if (filtErr > 0)
        AfxMessageBox("The filter is in spectroscopy mode\n\n"
          "You must go back to imaging mode before you can take pictures.", MB_EXCLAME);
      else
        SEMMessageBox("An error occurred while trying to set the state of"
          " the filter", MB_EXCLAME);
      ErrorCleanup(1);
      return 1;
    }
  }
  mIgnoreFilterDiffs = false;
 
  // If the image shift/stage time out is still active, just wait for it to be over
  // Where did that 0.8 come from?  It was introduced to reproduce what we always used to
  // get for an additional delay with a 795 camera
  int timeSet = mObeyTiltDelay ? RECORD_CONSET : inSet;
  UINT genTimeOut = mShiftManager->GetGeneralTimeOut(timeSet);
  UINT tickCount = GetTickCount();
  float startUp = 0.;

  // Skip the additional delay for image shift and a STEM camera
  if (!(mParam->STEMcamera && mShiftManager->GetLastTimeoutWasIS())) {
    startUp = 0.8f - mParam->startupDelay;
    if (mParam->K2Type)
      startUp = 0.8f - B3DMIN(1.4f, mParam->startupDelay);
  }
  if (mParam->GatanCam && !mParam->onlyOneShutter && !mParam->STEMcamera)
    startUp -= mParam->builtInSettling;
  genTimeOut = mShiftManager->AddIntervalToTickTime(genTimeOut, (int)(1000. * startUp));
  float delay = (float)SEMTickInterval(genTimeOut, tickCount);
  if (mSingleContModeUsed == CONTINUOUS) {
    if (GetTaskWaitingForFrame()) {
      genTimeOut = mShiftManager->GetGeneralTimeOut(timeSet);
      delay = (float)(mContinuousDelayFrac * SEMTickInterval(genTimeOut, tickCount));
      if (delay > 0)
        mShiftManager->ResetAllTimeouts();
    } else
      delay = 0.;
  }
  if (delay > 0) {
    UINT ISdelay = (UINT)delay;

    // 3/21/13: just compare against this constant regardless of user's tilt delay
    UINT delayLim = MAX_SCOPE_SETTLING;
    logmess.Format("delay %u  ticks %u  time out %u", ISdelay, tickCount, genTimeOut);
    // If delay is too long, cut it back and change the general time out too
    // Could give error message
    if (ISdelay > delayLim) {
      ISdelay = delayLim;
      mWinApp->AppendToLog("Warning: settling time had unreasonable value:",
        LOG_OPEN_IF_CLOSED);
      mWinApp->AppendToLog(logmess, LOG_OPEN_IF_CLOSED);
      mShiftManager->ResetAllTimeouts();
      mShiftManager->SetGeneralTimeOut(tickCount, ISdelay);
    } else if (mDebugMode)
      mWinApp->AppendToLog(logmess, LOG_OPEN_IF_CLOSED);
    mSettling = inSet;
    mWinApp->AddIdleTask(ThreadBusy, ScreenOrInsertDone,
      ScreenOrInsertError, inSet, -(int)ISdelay);
    mWinApp->SetStatusText(SIMPLE_PANE, "SETTLING");    
    mWinApp->UpdateBufferWindows();
    return 1;
  }
  return 0;
}

// Do everything required to get from the coordinates in the conset to usable binned
// coordinates 
void CCameraController::CapManageCoordinates(ControlSet & conSet, int &gainXoffset, 
                                             int &gainYoffset)
{
  int csizeX, csizeY, block, operation = 0;
  int tLeft, tTop, tBot, tRight, tsizeX, tsizeY, reducedSizeX, reducedSizeY;
  BOOL unbinnedK2, doseFrac, superRes, antialiasInPlugin, swapXYinAcquire = false;
  bool reduceAligned;

  // Get the CCD coordinates after binning. First make sure binning is right for K2
  // and set various flags about taking images unbinned and antialiasing in plugin
  superRes = conSet.K2ReadMode == SUPERRES_MODE;
  doseFrac = mParam->K2Type && conSet.doseFrac;
  if (mParam->K2Type && !superRes && conSet.binning < 2)
    conSet.binning = 2;
  antialiasInPlugin = mParam->K2Type && CAN_PLUGIN_DO(CAN_ANTIALIAS, mParam)  
    && conSet.mode == SINGLE_FRAME && (doseFrac || superRes || mAntialiasBinning) &&
    conSet.binning > (superRes ? 1 : 2) && mTD.NumAsyncSumFrames != 0;
  reduceAligned = doseFrac && conSet.alignFrames && conSet.useFrameAlign == 1;
  unbinnedK2 = mParam->K2Type && (superRes || doseFrac || antialiasInPlugin);
  mBinning = conSet.binning;
  mLeft = conSet.left / mBinning;
  mTop = conSet.top / mBinning;
  mRight = conSet.right / mBinning;
  mBottom = conSet.bottom / mBinning;
  mDMsizeX = mRight - mLeft;
  mDMsizeY = mBottom - mTop;
  reducedSizeX = mDMsizeX;
  reducedSizeY = mDMsizeY;
  
  // Adjust them to be multiples of the modulo values or satisfy other restrictions
  // But first get the final reduced size if antialiasing in plugin
  if (antialiasInPlugin || reduceAligned) {
    tLeft = mLeft;
    tRight = mRight;
    tTop = mTop;
    tBot = mBottom;
    AdjustSizes(reducedSizeX, mParam->sizeX, mParam->moduloX, tLeft, tRight, 
      reducedSizeY, mParam->sizeY, mParam->moduloY, tTop, tBot, mBinning);
  }
  AdjustSizes(mDMsizeX, mParam->sizeX, doseFrac ? -2 : mParam->moduloX, mLeft, mRight, 
    mDMsizeY, mParam->sizeY, doseFrac ? -2 : mParam->moduloY, mTop, mBottom, mBinning);
  
  // Fix coordinates for Tietz geometry
  tsizeX = mDMsizeX;
  tsizeY = mDMsizeY;
  tLeft = mLeft;
  tRight = mRight;
  tTop = mTop;
  tBot = mBottom;
  gainXoffset = 0;
  gainYoffset = 0;
  csizeX = mParam->sizeX;
  csizeY = mParam->sizeY;
  if (mParam->TietzType && mParam->TietzImageGeometry > 0) {
    UserToTietzCCD(mParam->TietzImageGeometry, mBinning, csizeX, csizeY, tsizeX,
      tsizeY, tTop, tLeft, tBot, tRight);
    swapXYinAcquire = (mParam->TietzImageGeometry & 20) != 0;
  }
  if (mParam->DE_camType >= 2)
    operation = mTD.DE_Cam->OperationForRotateFlip(mParam->DE_ImageRot, 
      mParam->DE_ImageInvertX);
  if (operation) {
    CorDefUserToRotFlipCCD(operation, mBinning, csizeX, csizeY, tsizeX, tsizeY, tTop, tLeft,
      tBot, tRight);
    swapXYinAcquire = operation % 2 ? 1 : 0;
  }

  if (mParam->TietzType && mParam->TietzBlocks) {

    // Supposedly only the F416 needs this.  Compute X gain offset in CCD coordinates
    // Then convert back to user coordinates to get offset in X or Y for gain reference
    // But new F816 is supposed to be "like F416" so let us see if this works.
    if (mParam->TietzType == 11 || mParam->TietzType == 13) {
      gainXoffset = mBlockOffsetSign * ((tLeft * mBinning) % (mParam->TietzBlocks + 1));
      if (mParam->TietzImageGeometry > 0) {
        int t2sizeX = csizeX / mBinning;
        int t2sizeY = csizeY / mBinning;
        int t2Right = t2sizeX + gainXoffset;
        int t2Bot = t2sizeY;
        TietzCCDtoUser(mParam->TietzImageGeometry, mBinning, csizeX, csizeY, t2sizeX,
          t2sizeY, gainYoffset, gainXoffset, t2Bot, t2Right);
      }
    }

    // Adjust the coordinates sent to camera to be multiple of block
    block = mParam->TietzBlocks / mBinning;
    tLeft = block * (tLeft / block);
    tTop = block * (tTop / block);
  }

  // Fix for K2 superresolution mode or dose fractionation
  // Set coordinates for binning 2 (actually unbinned) and set sizes to actual acquired
  // size, 4 times bigger for superres
  if (unbinnedK2) {
    tLeft = tLeft * mBinning / 2;
    tRight = tRight * mBinning / 2;
    tTop = tTop * mBinning / 2;
    tBot = tBot * mBinning / 2;
    if (doseFrac) {
      tLeft = 0;
      tRight = csizeX / 2;
      tTop = 0;
      tBot = csizeY / 2;
    }
    tsizeX = tRight - tLeft;
    tsizeY = tBot - tTop;
    if (superRes) {
      tsizeX *= 2;
      tsizeY *= 2;
    }

    // Set these variables when there is a reduction in the plugin, or when aligning,
    // because an unbinned alignment needs them to trim to the final size
    if (antialiasInPlugin || reduceAligned) {
      tsizeX = reducedSizeX;
      tsizeY = reducedSizeY;
      swapXYinAcquire = doseFrac && mParam->rotationFlip % 2;
    }
  }

  SEMTrace('T', "Size & LRTB, user: %d %d %d %d %d %d\r\n     mTD: %d %d %d %d %d %d   "
    "gain offset %d %d", mDMsizeX, mDMsizeY, mLeft, mRight, mTop, mBottom, tsizeX, tsizeY,
    tLeft, tRight, tTop, tBot, gainXoffset, gainYoffset);

  // load some thread data
  // If sizes have been swapped in the tsize variables, unswap them here and save sizes
  // for the calls for Tietz and DE.  Or, provide swapped sizes for the K2 dose frac case
  // with antialiasing in the plugin, where the plugin needs to know sizes to reduce 
  // unrotated image to
  mTD.DMSizeX = swapXYinAcquire ? tsizeY : tsizeX;
  mTD.DMSizeY = swapXYinAcquire ? tsizeX : tsizeY;
  mTD.CallSizeX = tsizeX;
  mTD.CallSizeY = tsizeY;
  mTD.Binning = mBinning;
  B3DCLAMP(mZoomFilterType, 0, 5);
  mTD.K2ParamFlags = B3DCHOICE(antialiasInPlugin, mZoomFilterType + 1, 0);
  if (mParam->K2Type) {
    if ((superRes || doseFrac) && !(antialiasInPlugin || reduceAligned))
      mTD.Binning = 1;
    else if (!superRes)
      mTD.Binning /= 2;
  }

  mTD.Left = tLeft;
  mTD.Top = tTop;
  mTD.Right = tRight;
  mTD.Bottom = tBot;

  // For oneView, throw the flag to take a subarea if plugin can do this and it is indeed
  // a subarea
  if (mParam->OneViewType && !mParam->subareasBad && 
    (tsizeX < csizeX / mBinning || tsizeY < csizeY / mBinning))
    mTD.K2ParamFlags |= K2_OVW_MAKE_SUBAREA;

}

void CCameraController::CapSetupShutteringTiming(ControlSet & conSet, int inSet, 
                                                 BOOL &bEnsureDark)
{
  CString logmess;
  double scanRate;

  int iShuttering = conSet.shuttering;
  int currentCam = mWinApp->GetCurrentCamera();
  if (!CanPreExpose(mParam, iShuttering)) {
    mConSetsp[inSet].drift = 0.;
    mCamConSets[inSet + currentCam * MAX_CONSETS].drift = 0.;
  }

  // Manage open beam shutter of Tietz camera before working on new shuttering mode
  if (mCameraWithBeamOn && (mCameraWithBeamOn != mParam->TietzType || 
    iShuttering != USE_FILM_SHUTTER))
    SwitchTeitzToBeamShutter(currentCam);

  mDelay = 0.;
  mTD.UnblankTime = 0;
  mTD.ReblankTime = 0;
  mTD.ShutterTime = 0;
  mTD.DynFocusInterval = 0;
  mTD.ScanTime = 0;
  mTD.ShotDelayTime = 0;
  mTD.MinBlankTime = (int)(1000. * mMinBlankingTime);
  mTD.STEMcamera = mParam->STEMcamera;

  if (FEIscope && mParam->DMCamera && mBeamWidth > 0. && mTD.IStoBS.xpx != 0.) {
    SetupBeamScan(&conSet);
    bEnsureDark = conSet.processing != UNPROCESSED;

  } else if (mParam->STEMcamera) {

    // STEM CAMERA

    float tmpExp = mNeedShotToInsert >= 0 ? mInsertDetShotTime : conSet.exposure;
    ComputePixelTime(mParam, mTD.DMSizeX, mTD.DMSizeY, 
      mParam->FEItype || mNeedShotToInsert >= 0 ? 0 : conSet.lineSync, 0., 0., tmpExp,
      mTD.PixelTime, scanRate);
    mExposure = tmpExp;
    if (mNeedShotToInsert < 0)
      conSet.exposure = tmpExp;
    conSet.processing = UNPROCESSED;
    if (mParam->FEItype) {
      if (restrictedSizeIndex <= 0)
        mTD.restrictedSize = 0;
      else
        mTD.restrictedSize = restrictedSizeIndex;
    } else if (mParam->GatanCam) {

       // Set up line sync variable, which is now a set of flags for plugin high enough
      mTD.LineSyncAndFlags = B3DCHOICE(conSet.lineSync && mNeedShotToInsert < 0, 
        DS_LINE_SYNC, 0);
      if (CAN_PLUGIN_DO(CAN_CONTROL_BEAM, mParam) && 
        mDMversion[CAMP_DM_INDEX(mParam)] > DM_DS_CONTROLS_BEAM && 
        conSet.mode == SINGLE_FRAME) {
          if (mDScontrolBeam > 0)
            mTD.LineSyncAndFlags |= (mDScontrolBeam << 1);
      }
    }
    SetNonGatanPostActionTime();
    mTD.DMsettling = 0.;

  } else if (mParam->GatanCam) {

    // GATAN CAMERA: Set the timing depending on the shutter mode
    // Fancy shutter with no drift is the same as beam shutter only
    if (iShuttering == USE_DUAL_SHUTTER && 
      (conSet.drift == 0.0 || mParam->K2Type || mParam->OneViewType))
        iShuttering = USE_BEAM_BLANK;

    // If drift settling is not OK in DM, or if beam blanking is requested but not
    // available, go for dual shutter
    if ((!mParam->DMsettlingOK && conSet.drift > 0.0) || 
      (!mParam->DMbeamShutterOK && iShuttering == USE_BEAM_BLANK))
      iShuttering = USE_DUAL_SHUTTER;

    switch (iShuttering) {

      // Film shutter: if there is delay, subtract off built-in amount
    case USE_FILM_SHUTTER:
      if (conSet.drift) {
        mDelay = conSet.drift - mParam->builtInSettling;
        if (mDelay < 0.01)
          mDelay = 0.01f;

        // OneView just takes the drift directly and seems to work
        if (mParam->OneViewType)
          mDelay = conSet.drift;
      }
      mTD.ShutterMode = mFilmShutter;
      break;

      // Beam blanking by DM: set the delay time, subtracting built-in amount
      // It is ineffective for OneView with two shutters currently
    case USE_BEAM_BLANK:
      if (conSet.drift && !(mParam->OneViewType && !mParam->onlyOneShutter)) {
        mDelay = conSet.drift - mParam->builtInSettling;
        if (mDelay < 0.01)
          mDelay = 0.01f;
        if (mParam->K2Type)
          mDelay = (float)(mK2ReadoutInterval * B3DNINT(conSet.drift / 
          mK2ReadoutInterval));
      }
      mTD.ShutterMode = mParam->beamShutter;
      if (mParam->extraBeamTime < 0.)
        mTD.UnblankTime = (int)(1000. * mParam->startupDelay);
      break;

      // Dual shutter needs to use internal blanking, either sleep between
      // unblanking the beam and firing DM, or fire DM then unblank the beam
    case USE_DUAL_SHUTTER:
      int msDrift = (int)(1000. * conSet.drift);
      if (conSet.drift < mParam->minimumDrift)
        msDrift = (int)(1000. * mParam->minimumDrift);
      int msBuiltIn = (int)(1000. * mParam->builtInSettling);
      int msStartup = (int)(1000. * mParam->startupDelay);
      if (conSet.drift == 0.) {

        // The desperate case of no beam blanking through DM
        // Extend exposure by the "extra time" on both sides, unblank by extra time
        // after exposure starts, reblank after exposure time
        mTD.ReblankTime = (int)(1000. * (mExposure - mParam->extraUnblankTime) + 0.5);
        //mTD.ReblankTime = (int)(1000. * (mExposure) + 0.5);
        if (mTD.ReblankTime < 1)
          mTD.ReblankTime = 1;
        mExposure += mParam->minimumDrift + mParam->extraBeamTime;

        // Set up for a minimum exposure time so short exposures can share dark refs
        if (mExposure < mParam->minBlankedExposure)
          mExposure = mParam->minBlankedExposure;
        mTD.UnblankTime = msBuiltIn + msStartup + (int)(1000. * mParam->minimumDrift);
      } else {

        // Regular dual shuttering, need to truncate the preexposure
        if (msDrift <= msBuiltIn) {
          mTD.UnblankTime = msBuiltIn + msStartup - msDrift;
        } else {

          // regular shuttering, need to extend preexposure
          // subtract startup delay IFF shutter can be left open
          mTD.ShutterTime = msDrift - msBuiltIn;
          if (mTD.ShutterTime <= 0) {
            mTD.ShutterTime = 0;
          } else if (mParam->DMopenShutterOK) {

            // If we can use openShutter commands, then subtract the extra
            // time
            mTD.ShutterTime -= (int)(1000. * mParam->extraOpenShutterTime);
            if (mTD.ShutterTime < 10)
              mTD.ShutterTime = 10;
          } else {

            // But if we can't use shutter, set up dark reference exposure
            mTD.PreDarkBlankTime = msBuiltIn - mTD.ShutterTime;
            if (mTD.PreDarkBlankTime < 10)
              mTD.PreDarkBlankTime = 0;
            mTD.PreDarkExposure = 0.001 * (mTD.ShutterTime - msBuiltIn);
            if (mTD.PreDarkExposure < 0.01)
              mTD.PreDarkExposure = 0.01;
            if (mPreDarkBump)
              mTD.PreDarkExposure += FORCING_INCREMENT;
            mPreDarkBump = !mPreDarkBump;
            mTD.PreDarkBinning = mParam->binnings[mParam->numBinnings - 1];
            mTD.ShutterTime = 0;
          }

          mTD.UnblankTime = 0;
        }

        // Add the shutter time since it is done by camera plugin now
        if (!mSkipNextReblank)
          mTD.ReblankTime = (int)(1000. * (mExposure + mParam->builtInSettling + 
            mParam->startupDelay + mParam->extraBeamTime)) + mTD.ShutterTime -
            mTD.UnblankTime;
        mSkipNextReblank = false;
      }
      mTD.ShutterMode = mFilmShutter;
      bEnsureDark = conSet.processing != UNPROCESSED && !mParam->K2Type;
      break;
    }

    mTD.DMsettling = mDelay;

    // If can't set beam shutter, send a negative shutter number to prevent trying
    // to set shutter at all
    if (!mParam->DMbeamShutterOK)
      mTD.ShutterMode = -1;

    // Set up post actions
    mTD.PostActionTime = 0;
    if (mStageQueued || mISQueued || mMagQueued || (mDriftISQueued && mBeamWidth <= 0)) {

      // Use reblanktime as a flag, or set up time to end of exposure regardless
      if (mTD.ReblankTime)
        mTD.PostActionTime = mTD.ReblankTime;
      else
        mTD.PostActionTime = (int)(1000. * (mExposure + mParam->builtInSettling + 
        mParam->startupDelay + mDelay + mParam->extraBeamTime));

      // Make it ensure dark reference
      bEnsureDark = conSet.processing != UNPROCESSED && !mParam->K2Type;
    }
  } else if (mParam->TietzType) {

    // TIETZ CAMERA SHUTTERING - for film shutter, may need to open it and wait
    mTD.DMsettling = 0.;
    mTD.TietzCanPreexpose = mParam->TietzCanPreExpose;
    mTD.ShutterMode = iShuttering;
    if (iShuttering == USE_FILM_SHUTTER) {
      if (mCameraWithBeamOn != mTD.Camera)
        mTD.ShutterTime = (int)(1000. * mTietzFilmSwitchTime);
      mCameraWithBeamOn = mParam->TietzType;    // THIS SHOULD BE SET LATER
    } else if (conSet.drift > 0 && mParam->TietzCanPreExpose) {

      // For beam shutter, if there is drift settling, and if
      // the pre-exposure shuttering exists, use that
      mTD.DMsettling = conSet.drift;
      mTD.ShutterMode = USE_DUAL_SHUTTER;
    }

    SetNonGatanPostActionTime();

    // No need to set ensure dark flag, it is set if processed image below

    if (mDebugMode) {
      logmess.Format("shutter mode %d  shutter time %d  settling %f  posttime %d"
        "  beam on %d",
        mTD.ShutterMode, mTD.ShutterTime, mTD.DMsettling, mTD.PostActionTime,
        mCameraWithBeamOn);
      mWinApp->AppendToLog(logmess, LOG_OPEN_IF_CLOSED);
    }
  } else if (mParam->AMTtype) {

    // AMT camera shuttering
    mTD.ShutterMode = 0;
    ConstrainDriftSettling(conSet.drift);
    SetNonGatanPostActionTime();

    // Here and for FEI there is no way to "ensure" dark reference exists if not
    // processing here

  } else if (mParam->DE_camType) {

	  // Direct Electron camera for handling shuttering
	  // Ensure we have a proper Settling time
	  ConstrainDriftSettling(conSet.drift);
	  SetNonGatanPostActionTime();
	 
  } else if (mParam->FEItype) {

    // FEI camera shuttering - set size also
    ConstrainDriftSettling(conSet.drift);
    if ((!mParam->processHere || (mFrameSavingEnabled && IS_BASIC_FALCON2(mParam) &&
      !mWinApp->mGainRefMaker->GetPreparingGainRef())) && 
      conSet.processing == DARK_SUBTRACTED)
      conSet.processing = GAIN_NORMALIZED;
    if (FCAM_ADVANCED(mParam) && !mASIgivesGainNormOnly)
      conSet.processing = GAIN_NORMALIZED;
    SetNonGatanPostActionTime();
    mTD.eagleIndex = mParam->eagleIndex;
    if (restrictedSizeIndex <= 0)
      mTD.restrictedSize = 0;
    else
      mTD.restrictedSize = restrictedSizeIndex;

  } else if (mTD.plugFuncs) {

    // PLUGIN camera
    // Pass on values; the acquire routine will test the existence of functions
    mTD.SelectCamera = mParam->cameraNumber;
    mTD.ShutterMode = iShuttering;
    ConstrainDriftSettling(conSet.drift);
    if (!mParam->canPreExpose)
      mTD.DMsettling = 0.;

    // Fix processing requests in case setup dialog didn't
    if (!mParam->processHere && conSet.processing == DARK_SUBTRACTED && 
      mParam->canDoProcessing && !(mParam->canDoProcessing & DARK_SUBTRACTED))
      conSet.processing = GAIN_NORMALIZED;
    if (!mParam->processHere && conSet.processing == GAIN_NORMALIZED && 
      mParam->canDoProcessing && !(mParam->canDoProcessing & GAIN_NORMALIZED))
      conSet.processing = DARK_SUBTRACTED;
    SetNonGatanPostActionTime();
  }

  // If there is no shutter, redo all of this!
  if (mParam->noShutter) {
    mTD.DMsettling = 0.;
    mTD.UnblankTime = 0;
    mTD.ReblankTime = 0;
    mTD.ShutterTime = 0;
    mTD.PostActionTime = 0;
    mTD.ShutterMode = USE_BEAM_BLANK;
    if (conSet.drift > mParam->startupDelay)
      mTD.ShotDelayTime = (int)(1000. * (conSet.drift - mParam->startupDelay));
    
    // If no reliable timing, simply allow more pre-exposure
    if (mParam->noShutter == 1) {
      if (mParam->extraBeamTime < 0.)
        mTD.UnblankTime = (int)(1000. * mParam->startupDelay);
    } else {

      // Otherwise set up for unblanking and reblanking
      if (conSet.drift <= mParam->startupDelay) 
        mTD.UnblankTime = (int)(1000. * (mParam->startupDelay - conSet.drift));
      mTD.ReblankTime = (int)(1000 * (mParam->startupDelay + mExposure + 
        mParam->extraBeamTime)) - mTD.UnblankTime;
      if (mStageQueued || mISQueued || mMagQueued || (mDriftISQueued && mBeamWidth <= 0))
         mTD.PostActionTime = mTD.ReblankTime;
    }
    bEnsureDark |= (conSet.processing != UNPROCESSED && mParam->GatanCam && 
      !mParam->K2Type);
  }
}

// Take care of saving stage position and mag, setting up boosted mag, and setting up
// dynamic focus
int CCameraController::CapSaveStageMagSetupDynFocus(ControlSet & conSet, int inSet)
{
  double msPerLine;
  CString mess;
  float flyback = mParam->flyback, startupDelay = mParam->startupDelay;
  MagTable *magTab = mWinApp->GetMagTable();
  BOOL boostMag;
  int ind;

  // Determine if doing a focus picture that needs its mag boosted
  boostMag = mParam->STEMcamera && conSet.boostMag && inSet == FOCUS_CONSET && 
    (conSet.magAllShots || mWinApp->mFocusManager->DoingFocus()) && !mMagToRestore &&
    !mWinApp->LowDoseMode();
  
  // Get the tilt angle and mag now in case there is a post-action
  // Get beam shift matrix for current mag
  mScope->FastStagePosition(mStageXbefore, mStageYbefore, mStageZbefore);
  if (mTD.PostChangeMag || boostMag) {
    mMagBefore = mScope->GetMagIndex(true);
    if (!boostMag)
      mTD.PostMagFixIS = mScope->AssessMagISchange(mMagBefore, mTD.NewMagIndex, false,
        mTD.ISX, mTD.ISY);
  } else
    mMagBefore = mScope->FastMagIndex();
  mTD.IStoBS = mShiftManager->GetBeamShiftCal(mMagBefore);
  
  // Now ready to set up parameters for drift compensation, or dynamic focus, and to set
  // the rotation angle for STEM
  SetupDriftWithIS();
  if (mParam->STEMcamera) {

    // Boost the mag and save the old mag; but the mag for the image has to be the
    // boosted mag.  This variable was not set if a boost was retained
    if (boostMag) {
      mMagToRestore = mMagBefore;
      mMagBefore = mShiftManager->FindBoostedMagIndex(mMagBefore, conSet.boostMag);
      mScope->SetMagIndex(mMagBefore);
      Sleep(mPostSTEMmagChgDelay);
    }

    for (ind = 0; ind < mParam->numBinnings - 1; ind++)
      if (mBinning == mParam->binnings[ind])
        break;
    mTD.STEMrotation = mWinApp->GetAddedSTEMrotation() -
      (magTab[mMagBefore].rotation[mWinApp->GetCurrentCamera()] + mParam->extraRotation);
    if (mShiftManager->GetInvertStageXAxis())
      mTD.STEMrotation = -mTD.STEMrotation;
    if (conSet.dynamicFocus && mSingleContModeUsed == SINGLE_FRAME && 
      mNeedShotToInsert < 0) {
        if (mParam->FEItype) {
          ind = mWinApp->mCalibTiming->FlybackTimeFromTable(mBinning, mTD.DMSizeX, 
            mMagBefore, conSet.exposure, flyback, startupDelay, &mess);
          if (ind != FLYBACK_EXACT)
            mWinApp->AppendToLog("WARNING: " + mess);
        }
        ind = DynamicFocusOK(conSet.exposure, mTD.DMSizeY, flyback, 
          mTD.DynFocusInterval, msPerLine);
        if (ind > 0)
          SetupDynamicFocus(ind, msPerLine, flyback, startupDelay);
    }

    // Create focus ramper if needed and make sure it's still there and responds
    if (FEIscope && (mTD.UnblankTime != 0 || 
      (mTD.DynFocusInterval && mTD.PostActionTime))) {

        // Plugin scope
        if (FEIscope && mTD.scopePlugFuncs->FocusRamperInitialize) {
          if (mTD.scopePlugFuncs->FocusRamperInitialize()) {
            SEMMessageBox(CString(mTD.scopePlugFuncs->GetLastErrorString()));
            ErrorCleanup(1);
            return 1;
          }
        } else {

          // Local scope
          for (ind = 0; ind < 2; ind++) {
            if (!mRamperInstance && CreateFocusRamper())
              break;
            try {
              mTD.FocusRamper->Initialize(_bstr_t(mScope->GetInstrumentName()));
              break;
            } 
            catch (_com_error E) {
              if (ind)
                SEMReportCOMError(E, _T("Trying to initialize focus ramper "));
              mTD.FocusRamper = NULL;
              mRamperInstance = false;
            }
          }
          if (!mRamperInstance) {
            ErrorCleanup(1);
            return 1;
          }
        }
    }
  }

  // Set up for focus steps for Gatan or Tietz
  mTD.FocusStep1 = 0.;
  if ((mParam->GatanCam || mParam->TietzType || IS_FALCON2_OR_3(mParam)) && 
    !mParam->STEMcamera && !mTD.DynFocusInterval && !mTD.DriftISinterval && 
    (mFocusStepToDo1 || mFocusStepToDo2) && mFocusInterval1 < conSet.exposure) {
      mCenterFocus = mScope->GetDefocus();
      if (mFocusInterval1 > 0.) {

        // If there is an interval, set up step shots
        mTD.FocusStep1 = mFocusStepToDo1;
        mTD.FocusStep2 = 0.;
        mTD.Step2FocusDelay = 0;
        if (mFocusStepToDo2 && mFocusInterval2 && mFocusInterval2 < conSet.exposure) {
          mTD.FocusStep2 = mFocusStepToDo2;
          mTD.Step2FocusDelay = B3DNINT(1000. * (mFocusInterval2 - mFocusInterval1));
        }

        // The delay is startup time to exposure plus step interval, minus unblank time
        // Adjust reblank and post-action times down by the sum of the two delays
        mTD.ScanDelay = (int)(1000. * (mParam->builtInSettling + 
          mParam->startupDelay + mTD.DMsettling + mFocusInterval1)) - mTD.UnblankTime;
        if (mTD.ReblankTime)
          mTD.ReblankTime -= mTD.ScanDelay + mTD.Step2FocusDelay;
        if (mTD.PostActionTime)
          mTD.PostActionTime -= mTD.ScanDelay + mTD.Step2FocusDelay;
      } else {

        // Otherwise set up dynamic focus
        mTD.FocusPerMs = 0.001 * (mFocusStepToDo2 - mFocusStepToDo1) / mExposure;
        mTD.FocusBase = mCenterFocus + mFocusStepToDo1 - 
          mTD.FocusPerMs * mSmoothFocusExtraTime / 2.;
        mTD.JeolOLtoUm = mScope->GetJeol_OLfine_to_um();
        mTD.DynFocusInterval = mDynFocusInterval;
        mTD.ScanDelay = (int)(1000. * (mParam->builtInSettling + mParam->startupDelay + 
          mTD.DMsettling)) - mTD.UnblankTime - mSmoothFocusExtraTime / 2;
        mTD.ScanDelay = B3DMAX(1, mTD.ScanDelay);
        mTD.PostActionTime = (int)(1000. * mExposure) + mSmoothFocusExtraTime;
        mTD.ReblankTime = 0;
      }
  }

  // It is a one-shot: zero this out
  mFocusStepToDo1 = 0.;
  mFocusStepToDo2 = 0.;

  return 0;
}

// If a dark or gain reference is required, find one or prepare to get one if
// doing processing here
int CCameraController::CapManageDarkGainRefs(ControlSet & conSet, int inSet, 
                                             BOOL &bEnsureDark, int gainXoffset, 
                                             int gainYoffset)
{
  int refErr;

  // If dark ref once only selected, then set forcedark flag and clear in the conset
  if (mConSetsp[inSet].onceDark) {
    mConSetsp[inSet].onceDark = 0;
    mCamConSets[inSet + mWinApp->GetCurrentCamera() * MAX_CONSETS].onceDark = 0;
    conSet.forceDark = 1;
  }

  mTD.DarkToGet = NULL;
  mTD.GainToGet = NULL;
  mDarkBelow = NULL;
  mDarkAbove = NULL;
  if (conSet.processing != UNPROCESSED) {
    mUseCount++;
    mDarkp = NULL;
    mGainp = NULL;

    if (GetProcessHere() && !(mFrameSavingEnabled && IS_BASIC_FALCON2(mParam) &&
      !mWinApp->mGainRefMaker->GetPreparingGainRef())) {
      double currentTicks = GetTickCount();

      // Check for KV change if necessary (it is done after an interval)
      mWinApp->mGainRefMaker->CheckChangedKV();

      // See if gain references could have been replaced in DM
      // Set up to check for new DM gain references, and now that this is possible
      // need to delete all gain refs unconditionally
      if (SEMTickInterval(currentTicks, 1000. * mLastImageTime) / 1000. > mGainTimeLimit){
        mWinApp->mGainRefMaker->DMRefsNeedChecking();
        if (DeleteReferences(GAIN_REFERENCE, false))
          SEMTrace('r', "Removing gain references due to inactivity");
      }

      float minRefExp = B3DMAX(mParam->minExposure, mMinInterpRefExp);

      // Look through array of references for ones that match
      for (int i = 0; i < mRefArray.GetSize(); i++) {
        DarkRef *ref = mRefArray[i];
        if (conSet.processing == GAIN_NORMALIZED && 
          ref->GainDark == GAIN_REFERENCE && gainXoffset == ref->xOffset &&
          gainYoffset == ref->yOffset && mTD.Camera == ref->Camera && 
          mBinning == ref->Binning) {
          mGainp = ref;
          ref->UseCount = mUseCount;
        }
        if (ref->GainDark == DARK_REFERENCE && mTD.Camera == ref->Camera && 
          mBinning == ref->Binning && mDelay == ref->Delay && mTop == ref->Top &&
          mBottom == ref->Bottom && mLeft == ref->Left && mRight == ref->Right) {
          if (mExposure == ref->Exposure) {
            mDarkp = ref;
            ref->UseCount = mUseCount;
          } else if (mInterpDarkRefs && !conSet.forceDark) {

            // If interpolating, find nearest dark reference above and below, where
            // one at minimum exposure counts as being below
            if (mExposure < ref->Exposure && ref->Exposure > minRefExp && 
              ref->Exposure - mExposure <= mInterpRefInterval) {
              if (!mDarkAbove || ref->Exposure < mDarkAbove->Exposure)
                mDarkAbove = ref;
             
            } else if ((mExposure > ref->Exposure && 
              mExposure - ref->Exposure <= mInterpRefInterval) || 
              (mExposure < minRefExp && ref->Exposure == minRefExp)) {
              if (!mDarkBelow || ref->Exposure > mDarkBelow->Exposure)
                mDarkBelow = ref;
            }
          }
        }
      }

      // If no gain reference found, make a new one
      // Allocate memory after clearing out old refs if necessary
      if (conSet.processing == GAIN_NORMALIZED && !mGainp) {
        int evenCrit = 0;
        mGainp = new DarkRef;
        mGainp->GainDark = GAIN_REFERENCE;
        mGainp->Binning = mBinning;
        mGainp->Camera = mTD.Camera;
        mGainp->UseCount = mUseCount;
        mGainp->xOffset = gainXoffset;
        mGainp->yOffset = gainYoffset;

        // Try to get gain reference internally; if not set up for DM reference or error
        // out if it is not a Gatan camera
        if ((refErr = mWinApp->mGainRefMaker->GetReference(mBinning, mGainp->Array, 
          mGainp->ByteSize, mGainp->GainRefBits, mGainp->Ownership, gainXoffset, 
          gainYoffset))) {
          if (!mParam->GatanCam || refErr < 0) {
            delete mGainp;
            SEMMessageBox("There is no gain reference for this camera at this"
              " binning" + CString(refErr < 0 ? " and KV level" : ""), MB_EXCLAME);
            ErrorCleanup(1);
            return 1;
          }

          SEMTrace('r', "Need gain reference from DM");
          mGainp->ByteSize = 4;
          mGainp->Array = NULL;
          mGainp->Ownership = -1;
          evenCrit = 1;

        }

        // Set the expected image size based on the even flags and source of ref
        mGainp->SizeX = mParam->sizeX / mBinning;
        mGainp->SizeY = mParam->sizeY / mBinning;
        if (mParam->fourPort || mParam->refSizeEvenX > evenCrit)
          mGainp->SizeX = 2 * (mGainp->SizeX / 2);
        if (mParam->fourPort || mParam->refSizeEvenY > evenCrit)
          mGainp->SizeY = 2 * (mGainp->SizeY / 2);
        AddRefToArray(mGainp);
      }

      // Analyze refs found for interpolating
      if (mInterpDarkRefs) {

        // If there is an exact match or neither below or above found, proceed as usual
        if (mDarkp) {
          mDarkBelow = mDarkAbove = NULL;
          SEMTrace('r', "Using exact match to dark ref");
        } else if (mDarkBelow || mDarkAbove) {

          // Found below but not above, or interval is too big and above is farther away
          if ((mDarkBelow && !mDarkAbove) || (mDarkBelow && mDarkAbove &&
            mDarkAbove->Exposure - mDarkBelow->Exposure > mInterpRefInterval && 
            mExposure - mDarkBelow->Exposure < mDarkAbove->Exposure - mExposure)) {

            // If it is too old, replace it with one at current exposure
            if (SEMTickInterval(currentTicks, 1000. * mDarkBelow->TimeStamp) / 1000. > 
              mDarkAgeLimit) {
                SEMTrace('r', "Ref below at %.3f usable but expired, replacing with "
                  "this exp", mDarkBelow->Exposure);
                mDarkp = mDarkBelow;
                mDarkBelow = NULL;
                mDarkp->Exposure = mExposure;
                mDarkp->UseCount = mUseCount;
            } else {

              // Otherwise set up a new one
              SetupNewDarkRef(inSet, mDarkBelow->Exposure + mInterpRefInterval);
              mDarkAbove = mDarkp;
              SEMTrace('r', "Ref only below at %.3f, making new one at %.3f",
                mDarkBelow->Exposure, mDarkp->Exposure);
            }

            // Found above but not below, or interval too big and below is farther away
          } else if ((mDarkAbove && !mDarkBelow) || (mDarkBelow && mDarkAbove &&
            mDarkAbove->Exposure - mDarkBelow->Exposure > mInterpRefInterval && 
            mExposure - mDarkBelow->Exposure >= mDarkAbove->Exposure - mExposure)) {
            if (SEMTickInterval(currentTicks, 1000. * mDarkAbove->TimeStamp) / 1000. > 
              mDarkAgeLimit) {
                SEMTrace('r', "Ref above at %.3f usable but expired, replacing with "
                  "this exp", mDarkAbove->Exposure);
                mDarkp = mDarkAbove;
                mDarkAbove = NULL;
                mDarkp->Exposure = mExposure;
                mDarkp->UseCount = mUseCount;
            } else {
              SetupNewDarkRef(inSet, 
                B3DMAX(minRefExp, mDarkAbove->Exposure - mInterpRefInterval));
              mDarkBelow = mDarkp;
              SEMTrace('r', "Ref only above at %.3f, making new one at %.3f",
                mDarkAbove->Exposure, mDarkp->Exposure);
            }
          } else {

            // Both exist AND their interval is usable: assign the older one to mDarkp
            // so that will be replaced if it is expired
            if (mDarkAbove->TimeStamp > mDarkBelow->TimeStamp)
              mDarkp = mDarkBelow;
            else
              mDarkp = mDarkAbove;
            SEMTrace('r', "Ref below at %.3f, above at %.3f", mDarkBelow->Exposure,
              mDarkAbove->Exposure);
            mDarkAbove->UseCount = mUseCount;
            mDarkBelow->UseCount = mUseCount;
          }
        }
      }

      // Get a new dark reference if needed
      if (!mDarkp) {
        SetupNewDarkRef(inSet, mExposure);
      }
      
      // Set up to get a gain ref if the ref has a NULL array
      mTD.GainToGet = (mGainp && !mGainp->Array) ? mGainp : NULL;

      // Set up to get a dark ref if the ref has a NULL array, if we are forcing a
      // dark reference or if this reference is too old
      BOOL needDarkRef = !mDarkp->Array || conSet.forceDark;
      if (!needDarkRef && SEMTickInterval(currentTicks, 1000. * mDarkp->TimeStamp)/1000. >
        mDarkAgeLimit) {
          needDarkRef = true;
          SEMTrace('r', "Replacing dark reference due to its age");
      }
      mTD.DarkToGet = needDarkRef ? mDarkp : NULL;
      bEnsureDark = mTD.GainToGet || mTD.DarkToGet;
      mTD.Processing = UNPROCESSED;
      
      // Set up dark reference averaging if once or every time flag set; clear once flag
      mNumAverageDark = 0;
      if (needDarkRef) {
        if (conSet.averageOnce) {
          conSet.averageDark = 1;
          mConSetsp[inSet].averageOnce = 0;
          mCamConSets[inSet + mWinApp->GetCurrentCamera() * MAX_CONSETS].averageOnce = 0;
        }
        if (conSet.averageDark && conSet.numAverage > 1)
          mNumAverageDark = conSet.numAverage;
      }

    } else if (mParam->GatanCam) {

      // If processing in DM, need to analyze whether to force a new dark ref
      // Search for a matching dark ref in our list
      while (mDarkp == NULL) {
        int minUse = mUseCount + 1;
        int oldest;
        int freeRef = -1;
        int sizetot = 0;
        for (int i = 0; i < MAX_DARK_REFS; i++) {
          DarkRef *ref = &mDarkRefs[mTD.Camera][i];
          if (ref->UseCount) {
            // If it is a valid dark ref entry, add to size total
            sizetot += 2 * ref->SizeX * ref->SizeY;
            
            // Find oldest used one
            if (ref->UseCount < minUse) {
              minUse = ref->UseCount;
              oldest = i;
            }
            
            // See if size and binning match (DM will reuse that dark ref)
            // Or, binning matches only for newer DM
            if (((mDMsizeX == ref->SizeX && mDMsizeY == ref->SizeY) ||
              mTD.DMversion >= DM_ONE_DARK_REF_PER_BINNING) && 
              mBinning == ref->Binning) 
              mDarkp = ref;
            
            // keep track of first free one too
          } else if (freeRef < 0)
            freeRef = i;
        }
        
        // if there is no match, DM will make a new dark ref.  
        if (mDarkp == NULL) {
          // If there is no room in our table, or if this new will make total size 
          // too big, delete the oldest
          if (freeRef < 0 || sizetot + mDMsizeX * mDMsizeY > mRefMemoryLimit) {
            mDarkp = &mDarkRefs[mTD.Camera][oldest];
            mDRdelete[mNumDRdelete++] = *mDarkp;
            mDarkp->UseCount = 0;
          } else {
            // otherwise, we can go ahead and assign a new entry in table
            mDarkp = &mDarkRefs[mTD.Camera][freeRef];
            mDarkp->SizeX = mDMsizeX;
            mDarkp->SizeY = mDMsizeY;
            mDarkp->Binning = mBinning;
            mDarkp->Exposure = 0.0;
            mDarkp->BumpExposure = 0;
          }
        }
      }
      
      mForce = mDarkp->Top != mTop || mDarkp->Left != mLeft || 
        mDarkp->Delay != mDelay || conSet.forceDark != 0;
      mDarkp->Top = mTop;
      mDarkp->Bottom = mBottom;
      mDarkp->Left = mLeft;
      mDarkp->Right = mRight;
      mDarkp->Delay = mDelay;
      mDarkp->UseCount = mUseCount;
      
      // Get an exposure that is bumped if this dark ref has bump flag set
      double exposure = mExposure + 
        ((mDarkp->BumpExposure && !mParam->K2Type) ? FORCING_INCREMENT : 0);
      
      // If forcing a new one, and this exposure matches the selected dark ref,
      // then toggle the flag and bump the real exposure if flag now set
      if (mForce && !mParam->K2Type) {
        if (exposure == mDarkp->Exposure) 
          mDarkp->BumpExposure = 1 - mDarkp->BumpExposure;
        if (mDarkp->BumpExposure)
          mExposure += FORCING_INCREMENT;
      } else {
        // If not forcing, just take this possibly bumped time
        mExposure = exposure;
      }
      
      // Make this exact time be the one stored with this dark ref
      mDarkp->Exposure = mExposure;
    
    }
  }
  return 0;
}


////////////////////////////////////////////////////////////////////////////////////
// END OF CAPTURE SUPPORT ROUTINES
////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//  Size adjustment and reference management routines
///////////////////////////////////////////////////////

// Return coordinates for a legal centered frame given the starting sizes
void CCameraController::CenteredSizes(int &DMsizeX, int ccdSizeX, int moduloX, int &Left,
                                      int &Right, int &DMsizeY, int ccdSizeY, 
                                      int moduloY, int &Top, int &Bottom,int binning, 
                                      int camera)
{
  CameraParameters *camParam = (camera >= 0) ? &mAllParams[camera] : mParam;
  Left = (ccdSizeX / binning - DMsizeX) / 2;
  Right = Left + DMsizeX;
  Top = (ccdSizeY / binning - DMsizeY) / 2;
  Bottom = Top + DMsizeY;
  AdjustSizes(DMsizeX, ccdSizeX, moduloX, Left, Right, DMsizeY, ccdSizeY, moduloY, Top, 
    Bottom, binning, camera);

  // If Tietz blocked camera is now miscentered, increase by one block
  if (camParam->TietzBlocks) {
    if (Left + Right - ccdSizeX / binning > ccdSizeX % camParam->TietzBlocks + 2) {
      Left -= camParam->TietzBlocks;
      DMsizeX += camParam->TietzBlocks;
    } else if (ccdSizeX / binning - (Left + Right) > 
      ccdSizeX % camParam->TietzBlocks + 2) {
      Right += camParam->TietzBlocks;
      DMsizeX += camParam->TietzBlocks;
    }
    if (Top + Bottom - ccdSizeY / binning > ccdSizeY % camParam->TietzBlocks + 2) {
      Top -= camParam->TietzBlocks;
      DMsizeY += camParam->TietzBlocks;
    } else if (ccdSizeX / binning - (Top + Bottom) > 
      ccdSizeY % camParam->TietzBlocks + 2) {
      Bottom += camParam->TietzBlocks;
      DMsizeY += camParam->TietzBlocks;
    }
    AdjustSizes(DMsizeX, ccdSizeX, moduloX, Left, Right, DMsizeY, ccdSizeY, moduloY, Top, 
      Bottom, binning, camera);
  }
}
 
// Return just the size that a control set would acquire from the given camera
void CCameraController::AcquiredSize(ControlSet *csp, int camera, int &sizeX, int &sizeY)
{
  int binning = csp->binning;

  // Promote the binning to 2 if K2 not in SuperRes mode
  if (mAllParams[camera].K2Type && csp->K2ReadMode != SUPERRES_MODE)
    binning = B3DMAX(2, binning);
  int left = csp->left / binning;
  int right = csp->right / binning;
  int top = csp->top / binning;
  int bottom = csp->bottom / binning;
  sizeX = right - left;
  sizeY = bottom - top;
  AdjustSizes(sizeX, mAllParams[camera].sizeX, mAllParams[camera].moduloX, left, 
    right, sizeY, mAllParams[camera].sizeY, mAllParams[camera].moduloY, top, 
    bottom, binning, camera);
}

// Adjust sizes and positions for both axes: all external calls are through this
// version and not the one-axis version
void CCameraController::AdjustSizes(int &DMsizeX, int ccdSizeX, int moduloX, int &Left,
                                    int &Right, int &DMsizeY, int ccdSizeY, int moduloY, 
                                    int &Top, int &Bottom, int binning, int camera)
{
  int diff;
  CameraParameters *camParam = (camera >= 0) ? &mAllParams[camera] : mParam;
  int operation = 0;
  if (camParam->DE_camType >= 2)
    operation = mTD.DE_Cam->OperationForRotateFlip(camParam->DE_ImageRot, 
    camParam->DE_ImageInvertX);
  if (camParam->TietzType && camParam->TietzImageGeometry > 0)
    UserToTietzCCD(camParam->TietzImageGeometry, binning, ccdSizeX, ccdSizeY, DMsizeX, 
      DMsizeY, Top, Left, Bottom, Right);
  if (operation)
    CorDefUserToRotFlipCCD(operation, binning, ccdSizeX, ccdSizeY, DMsizeX, 
      DMsizeY, Top, Left, Bottom, Right);

  if (camParam->TietzBlocks)
    BlockAdjustSizes(DMsizeX, ccdSizeX, camParam->TietzBlocks, camParam->TietzBlocks + 1,
      Left, Right, binning);
  else
    AdjustSizes(DMsizeX, ccdSizeX, moduloX, Left, Right, binning, camera);
  if (camParam->squareSubareas) {
    diff = DMsizeY - DMsizeX;
    Top = B3DMAX(0, Top + diff / 2);
    Bottom = B3DMIN(ccdSizeY / binning, Top + DMsizeX);
    Top = B3DMAX(0, Bottom - DMsizeX);
    DMsizeY = DMsizeX;
  }
  if (camParam->TietzBlocks)
    BlockAdjustSizes(DMsizeY, ccdSizeY, camParam->TietzBlocks, camParam->TietzBlocks,
      Top, Bottom, binning);
  else
    AdjustSizes(DMsizeY, ccdSizeY, moduloY, Top, Bottom, binning, camera);
  if (camParam->TietzType && camParam->TietzImageGeometry > 0)
    TietzCCDtoUser(camParam->TietzImageGeometry, binning, ccdSizeX, ccdSizeY, DMsizeX, 
      DMsizeY, Top, Left, Bottom, Right);
  if (operation)
    CorDefRotFlipCCDtoUser(operation, binning, ccdSizeX, ccdSizeY, DMsizeX, 
      DMsizeY, Top, Left, Bottom, Right);
}

// Adjust sizes and positions so that dimensions are a multiple of modulo number
// And impose other restrictions encoded in moduloX
// DMsizeX, Left, and Right are in binned pixels
void CCameraController::AdjustSizes(int &DMsizeX, int ccdSizeX, int moduloX, 
                  int &Left, int &Right, int binning, int camera)
{
  static int lastRestricted = -1;
  int moduloMap[] = {8,16,32,64};
  int ind, binInd = -1, mapBase = -5;
  bool mapping = moduloX <= -5 && moduloX >= -8;
  int xMax = ccdSizeX/binning;
  CameraParameters *camParam = (camera >= 0) ? &mAllParams[camera] : mParam;
  if (!DMsizeX)
    DMsizeX++;
  if (Right <= Left) {
    Left = xMax / 2 - 2;
    Right = xMax / 2 + 2;
    DMsizeX = 4;
  }

  // First check for restricted sizes using size requested or the last restricted size
  // The last restricted size is used to enforce Y having same index after a call for X
  if ((moduloX < 0 && lastRestricted < 0) || lastRestricted >= 0) {
    for (ind = 0; ind < camParam->numBinnings; ind++) {
      if (camParam->binnings[ind] == binning) {
        binInd = ind;
        break;
      }
    }

    if ((DMsizeX > (xMax + 1) / 2 && lastRestricted < 0) || lastRestricted == 0 || 
      moduloX == -2 || mapping) {
      DMsizeX = xMax;
      Left = 0;
      restrictedSizeIndex = 0;
    } else if ((DMsizeX > (xMax + 3) / 4 && lastRestricted < 0) || lastRestricted == 1) {
      DMsizeX = xMax / 2;
      Left = xMax / 4;
      restrictedSizeIndex = 1;
      if (lastRestricted < 0 && binInd >= 0 && camParam->halfSizeX[binInd])
        DMsizeX = camParam->halfSizeX[binInd];
      if (lastRestricted == 1 && binInd >= 0 && camParam->halfSizeY[binInd])
        DMsizeX = camParam->halfSizeY[binInd];
    } else {
      DMsizeX = xMax / 4;
      Left = (3 * xMax) / 8;
      restrictedSizeIndex = 2;
      if (lastRestricted < 0 && binInd >= 0 && camParam->quarterSizeX[binInd])
        DMsizeX = camParam->quarterSizeX[binInd];
      if (lastRestricted > 0 && binInd >= 0 && camParam->quarterSizeY[binInd])
        DMsizeX = camParam->quarterSizeY[binInd];
    }

    // Toggle index stored here between -1 and the size index
    lastRestricted = lastRestricted < 0 ? restrictedSizeIndex : -1;
    Right = Left + DMsizeX;

    if (!mapping)
      return;
  }
  if (mapping)
    moduloX = moduloMap[mapBase - moduloX];

  // Next take care of centered coordinates - just use the size
  if (camParam->centeredOnly) {
    Left = (xMax - DMsizeX) / 2;
    Right = Left + DMsizeX;
  }

  // Take care of each coordinate being required to be a multiple of modulo
  if (camParam->coordsModulo) {

    // Expand to left and right, drop back if too big
    Left = moduloX * (Left / moduloX);
    Right = moduloX * ((Right + moduloX - 1) / moduloX);
    if (Right > xMax)
      Right -= moduloX;
    DMsizeX = Right - Left;
    return;
  }

  // Now take care of size being a multiple of modulo
  int extra = DMsizeX % moduloX;
  if (extra) {
    
    int bigSize = moduloX * ((DMsizeX + moduloX - 1) / moduloX);
    if (bigSize > xMax) {
      // Contract if too big
      Left += extra/2;
      Right -= (extra - extra/2);
    } else {
      // Otherwise expand
      extra = moduloX - extra;
      Left -= extra/2; 
      Right += (extra - extra/2);
      if (Left < 0) {
        Right -= Left;
        Left = 0;
      }
      if (Right > xMax) {
        Left -= (Right - xMax);
        Right = xMax;
      }
    }
    DMsizeX = Right - Left;
  }
}

// Adjust sizes for crazy Tietz block readouts
void CCameraController::BlockAdjustSizes(int &DMsize, int ccdSize, int sizeMod, 
                                         int startMod, int &start, int &end, int binning)
{
  // Convert to unbinned coordinates
  int ubSize = DMsize * binning;
  int ubStart = start * binning;
  int ubEnd = end * binning;

  // round size to multiple of block size
  int nblocks = B3DNINT((double)ubSize / sizeMod);
  nblocks = B3DMAX(1, B3DMIN(ccdSize / sizeMod, nblocks));
  int newSize = nblocks * sizeMod;
  DMsize = newSize / binning;

  // expand or contract the start and end
  ubStart = B3DMAX(0, ubStart + (ubSize - newSize) / 2);
  ubEnd = B3DMIN(ccdSize, ubStart + newSize);
  ubStart = ubEnd - newSize;

  // shift to the nearest allowed starting point
  ubStart =  startMod * B3DNINT((double)ubStart / startMod);

  // Return to binned coordinates, and round UP if it is not evenly divisible
  start = ubStart / binning;
  if (ubStart % binning)
    start++;
  end = start + DMsize;
}

// Apply any known constraints to the exposure time and K2 frame time
bool CCameraController::ConstrainExposureTime(CameraParameters *camP, ControlSet *consP) 
{
  return ConstrainExposureTime(camP, consP->doseFrac > 0, consP->K2ReadMode, 
    consP->binning, consP->alignFrames && !consP->useFrameAlign, 
    DESumCountForConstraints(camP, consP), consP->exposure, consP->frameTime);
}

int CCameraController::DESumCountForConstraints(CameraParameters *camP, ControlSet *consP) 
{
 if (mWinApp->mDEToolDlg.HasFrameTime(camP) && ((consP->saveFrames & DE_SAVE_MASTER) ||
   (consP->alignFrames && consP->useFrameAlign > 1)))
     return (consP->K2ReadMode > 0 ? consP->sumK2Frames : consP->DEsumCount);
 return 1;
}

// The underlying constraint routine.  doseFrac is relevant only for K2, alignInCamera
// only for Falcon, sumCount only for DE
bool CCameraController::ConstrainExposureTime(CameraParameters *camP, BOOL doseFrac,
  int readMode, int binning, bool alignInCamera, int sumCount, float &exposure, 
  float &frameTime)
{
  bool retval = false;
  float ftime, baseTime, minExp;
  double fps = 0., epsilon = 0.;
  int num, minFracs;

  // For all cameras, enforce the minimum exposure
  if (exposure < camP->minExposure) {
    exposure = camP->minExposure;
    retval = true;
  }
  if (!camP->K2Type && !IS_FALCON2_OR_3(camP) && !mWinApp->mDEToolDlg.HasFrameTime(camP)
    && !camP->OneViewType) 
    return retval;
  
  // Both K2 and Falcon round to nearest number of frames for a given exposure time;
  // when exposure is exactly midway between for K2, it rounds up
  // DE12 uses Num = floor(exp_time * fps) + 1  according to Liang
  if (camP->K2Type) {

    // For K2, first fix the frame time if it is used and set it as base time for 
    // exposure, otherwise set base time based on mode
    if (doseFrac) {
      if (ConstrainFrameTime(frameTime))
        retval = true;
      baseTime = frameTime;

    } else if (readMode == COUNTING_MODE) {
      baseTime = mBaseK2CountingTime;
    } else if (readMode == SUPERRES_MODE) {
      baseTime = mBaseK2SuperResTime;
    } else {
      baseTime = mBaseK2CountingTime;
    }
    mLastK2BaseTime = baseTime;
  } else if (camP->OneViewType) {

    // OneView 
    baseTime = B3DMAX(0.0001f, mOneViewDeltaExposure[0]);
    minExp = B3DMAX(0.0001f, mOneViewMinExposure[0]);
    for (num = 0; num < camP->numBinnings; num++) {
      if (!mOneViewMinExposure[num] || !mOneViewDeltaExposure[num])
        break;
      baseTime = mOneViewDeltaExposure[num];
      minExp = mOneViewMinExposure[num];
      if (camP->binnings[num] == binning)
        break;
    }
    if (exposure < minExp) {
      exposure = minExp;
      retval = true;
    }

  } else if (camP->FEItype) {

    // Falcon for now
    baseTime = mFalconReadoutInterval;
    if (camP->FEItype == FALCON3_TYPE && alignInCamera) {
      baseTime *= mFalcon3AlignFraction;
      minFracs = readMode > 0 ? mMinAlignFractionsCounting : mMinAlignFractionsLinear;
      if (exposure < baseTime * minFracs) {
        exposure = baseTime * minFracs;
        retval = true;
      }
    }
  } else {

    // DE12
    fps = readMode > 0 ? camP->DE_CountingFPS : camP->DE_FramesPerSec;
    if (fps <= 0.)
      fps = mWinApp->mDEToolDlg.GetFramesPerSecond();
    if (fps <= 0.)
      return retval;
    epsilon = -0.0005f;
    baseTime = (float)(sumCount / fps);
    frameTime = baseTime;
  }

  // Make exposure a multiple of base time: round up from a boundary by adding a bit (K2)
  num = B3DNINT(exposure / baseTime + 1.e-5);
  ftime = (float)(B3DMAX(1, num) * baseTime + epsilon);
  if (fabs(exposure - ftime) > 1.e-5)
    retval = true;
  exposure = ftime;
  return retval;
}

// Return a factor for rounding a constrained exposure time if appropriate
float CCameraController::ExposureRoundingFactor(CameraParameters *camP)
{
  if (camP->K2Type == 2 || IS_FALCON2_OR_3(camP) ||mWinApp->mDEToolDlg.HasFrameTime(camP))
    return 200.f;
  if (camP->OneViewType)
    return 1000.f;
  return 0;
}

bool CCameraController::IsDirectDetector(CameraParameters *camP)
{
  return camP->K2Type || IS_FALCON2_OR_3(camP) || camP->DE_camType == DE_12;
}

// Constrain a frame time for the K2 camera and return true if changed
bool CCameraController::ConstrainFrameTime(float &frameTime)
{
  float ftime = frameTime;
  int num = B3DNINT(frameTime / mK2ReadoutInterval);
  frameTime = (float)(num * mK2ReadoutInterval);
  B3DCLAMP(frameTime, mMinK2FrameTime, 10.f);
  return fabs(frameTime - ftime) > 1.e-5;
}

// Return the count scaling being used for a K2 camera, or countsPereElectron otherwise
float CCameraController::GetCountScaling(CameraParameters *camParam)
{
  float retVal = camParam->countsPerElectron;
  if (camParam->K2Type) {
    retVal = mScalingForK2Counts > 0. ? mScalingForK2Counts : mParam->countsPerElectron;
    if (!retVal)
      retVal = mDefaultCountScaling;
  } else if (!retVal)
    retVal = camParam->DE_camType ? mDefaultCountScaling : 1.f;
  return retVal;
}

// Adjust the working value of counts per electron for a Falcon for the chosen scaling
void CCameraController::AdjustCountsPerElecForScale(CameraParameters *param)
{
  if (param->FEItype != FALCON3_TYPE)
    return;
  param->countsPerElectron = param->unscaledCountsPerElec;
  if (param->falcon3ScalePower < 0)
    param->countsPerElectron /= (float)pow(2., -param->falcon3ScalePower);
  else if (param->falcon3ScalePower > 0)
    param->countsPerElectron *= 
      param->linear2CountingRatio / (float)pow(2., param->falcon3ScalePower);
}

// Return the target size for tasks for the given camera, or the current on if camaParam
// is NULL (the default)
int CCameraController::TargetSizeForTasks(CameraParameters *camParam)
{
  if (!camParam)
    camParam = mParam;
  return (camParam && camParam->taskTargetSize > 0) ? camParam->taskTargetSize : 512;
}

// Return plugin version for a Gatan camera
int CCameraController::GetPluginVersion(CameraParameters *camP)
{
  if (camP->GatanCam)
    return mPluginVersion[camP->useSocket ? SOCK_IND : COM_IND];
  return 0;
}

// Convenience function for testing capability
bool CCameraController::CanPluginDo(int minVersion, CameraParameters *param)
{
  return GetPluginVersion(param) >= minVersion;
}

// For cameras with no special handling, fix and transfer the drift settling entry
void CCameraController::ConstrainDriftSettling(float drift)
{
  mTD.DMsettling = drift > 0.01 ? B3DMAX(mParam->minimumDrift, drift) : 0.;
  if (mParam->maximumDrift > 0.)
    mTD.DMsettling = B3DMIN(mParam->maximumDrift, mTD.DMsettling);
}

// Finds the nearest actual binning to the on in the conset, returns its index in binInd,
// the real value in realBin, and true if it does not match
bool CCameraController::FindNearestBinning(CameraParameters *camParam, 
                                           ControlSet *conSet, int &binInd, int &realBin)
{
  int i, diff, minDiff = 100;
  for (i = 0; i < camParam->numBinnings; i++) {
    diff = conSet->binning - camParam->binnings[i];
    if (diff < 0)
      diff = -diff;
    if (minDiff > diff) {
      minDiff = diff;
      binInd = i;
    }
  }
  if (camParam->K2Type && conSet->K2ReadMode != SUPERRES_MODE && !binInd)
    binInd = 1;
  realBin = camParam->binnings[binInd];
  return realBin != conSet->binning;
}

// Add a reference to the array, computing total memory occupied and deleting any
// older references if needed to keep memory within bounds
int CCameraController::AddRefToArray(DarkRef *newRef)
{
  int i, j, minUse, oldest, maxSame;
  DarkRef *ref;
  int numOlder = 0;
  int memTot = 0;
  int numDarks[MAX_CAMERAS][MAX_CONSETS];
  CString report;
  for (i = 0; i < MAX_CAMERAS; i++)
    for (j = 0; j < MAX_CONSETS; j++)
      numDarks[i][j] = 0;

  // Add the new reference now so it is included in totals
  mRefArray.Add(newRef);

  // Get total memory used, number of older references, and number for each conset
  // Simply skip ones we don't own
  for (i = 0; i < mRefArray.GetSize(); i++) {
    ref = mRefArray[i];
    if (!ref->Ownership)
      continue;
    memTot += ref->SizeX * ref->SizeY * ref->ByteSize;
    if (ref->UseCount < newRef->UseCount)
      numOlder++;
    if (ref->GainDark == DARK_REFERENCE)
      numDarks[ref->Camera][ref->ConSetUsed]++;
  }

  SEMTrace('r', "%d refs, total mem %d; %d older", i, memTot, numOlder);

  // Loop until memory is below limit or there are no older ones left
  while (numOlder && memTot > mRefMemoryLimit) {

    // Find maximum number of dark refs for any control set
    maxSame = 0;
    for (i = 0; i < MAX_CAMERAS; i++)
      for (j = 0; j < MAX_CONSETS; j++)
        if (maxSame < numDarks[i][j])
          maxSame = numDarks[i][j];

    // Find oldest ref, or oldest of any control set if that is above the limit
    minUse = newRef->UseCount + 2;
    for (i = 0; i < mRefArray.GetSize(); i++) {
      ref = mRefArray[i];
      if (!ref->Ownership)
        continue;
      if (maxSame <= mConSetRefLimit || (ref->GainDark == DARK_REFERENCE &&
        numDarks[ref->Camera][ref->ConSetUsed] > mConSetRefLimit)) {
        if (ref->UseCount < minUse) {
          minUse = ref->UseCount;
          oldest = i;
        }
      }
    }

    // Delete the oldest, subtract its memory, adjust count for control set
    ref = mRefArray[oldest];

    if (ref->GainDark == DARK_REFERENCE)
      SEMTrace('r', "Deleting dark ref, camera %d, ConSet %d, binning %d,\r\n"
        "(%d %d %d %d), exposure %g, settling %g", ref->Camera, ref->ConSetUsed,
        ref->Binning, ref->Top, ref->Left, ref->Bottom, ref->Right, ref->Exposure, 
        ref->Delay);
    else
      SEMTrace('r', "Deleting gain ref, camera %d, binning %d", ref->Camera, 
        ref->Binning);

    memTot -= ref->SizeX * ref->SizeY * ref->ByteSize;
    if (ref->GainDark == DARK_REFERENCE)
      numDarks[ref->Camera][ref->ConSetUsed]--;
    if (ref->Array)
      delete ref->Array;
    delete ref;
    mRefArray.RemoveAt(oldest);
    numOlder--;
  }

  return 0;
}

// Remove all references of one type (dark or gain) from the reference array
int CCameraController::DeleteReferences(int type, bool onlyDMRefs)
{
  int total = 0;
  for (int i = 0; i < mRefArray.GetSize(); i++) {
    DarkRef *ref = mRefArray[i];
    if (ref->GainDark == type) {
      
      // If removing only DM refs, skip unless ownership is -1
      if (onlyDMRefs && ref->Ownership >= 0)
        continue;

      // Delete array only if we own it
      if (ref->Array && ref->Ownership)
        delete ref->Array;
      delete ref;
      mRefArray.RemoveAt(i--);
      total++;
    }
  }
  return total;
}

///////////////////////////////////////////////////////
//  Dark reference ensuring or acquiring routines
///////////////////////////////////////////////////////

// Start the thread for getting dark reference
void CCameraController::StartEnsureThread(DWORD timeout)
{
  CString paneText = "GETTING DARK REF";
  mEnsureThread = AfxBeginThread(EnsureProc, &mTD,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  TRACE("EnsureProc created with ID 0x%0x\n",mEnsureThread->m_nThreadID);
  mEnsureThread->m_bAutoDelete = false;
  mEnsureThread->ResumeThread();
  mWinApp->AddIdleTask(ThreadBusy, EnsureDone, EnsureError, 0, timeout);
  if (mNumAverageDark > 1)
    paneText.Format("DARK REF #%d", mAverageDarkCount + 1);
  mWinApp->SetStatusText(SIMPLE_PANE, paneText);
}

// Routine to ensure or obtain gain and/or dark reference
UINT CCameraController::EnsureProc(LPVOID pParam)
{
  char CamCommand[2048];
  int camPlace = 0;
  CamCommand[0] = 0x00;
  long arrSize, height, width;
  CameraThreadData *td = (CameraThreadData *)pParam;
  int sizeX, sizeY, numDum, retval = 0;
  long binning = td->Binning;
  double expSave = td->Exposure;
  CString message, str;
  CString strGainDark = "Dark";
  DarkRef *ref;
  IDMCamera *pGatan;
  HRESULT hr;

  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  if (!(td->TietzType || td->FEItype || td->DE_camType || td->plugFuncs)) {

    // GATAN and AMT CAMERAS
    CreateDMCamera(pGatan);
    if (CCTestHResult(td, hr, "getting instance of Camera ")) {
      retval = 1;
    } else if (td->GainToGet || td->DarkToGet) {
     
      // PROCESSING HERE: get gain ref
      ref = td->GainToGet;
      strGainDark = "Gain";
      for (int iGainDark = 0; iGainDark < 2 && !retval; iGainDark++) {
        if (ref) {
          
          // Need to get a reference.  First get an array if needed
          retval = GetArrayForReference(td, ref, arrSize, strGainDark);
          
          if (!retval) {
            try {
              width = ref->SizeX;
              height = ref->SizeY;
              
              // Get the reference, give a warning if size is wrong
              if (iGainDark) {
                if (td->NeedsReadMode) {
                  if (td->GatanReadMode < -1) {
                    CallGatanCamera(pGatan, SetK2Parameters2(td->GatanReadMode, 
                      td->CountScaling, 0, 0, 0., 0, 0, 0, td->K2ParamFlags,
                      td->DMSizeX + K2_REDUCED_Y_SCALE * td->DMSizeY, 
                      td->fullSizeX + K2_REDUCED_Y_SCALE * td->fullSizeY, 
                      0., 0., MAX_FILTER_NAME_LEN / 4, td->FilterName));
                  } else {
                    CallGatanCamera(pGatan, SetReadMode(td->GatanReadMode, 
                      td->CountScaling));
                  }
                }
                CallDMCamera(pGatan, td->amtCam, GetDarkReference((short *)ref->Array, 
                  &arrSize, &width, &height, ref->Exposure, binning, td->Top, 
                  td->Left, td->Bottom, td->Right, td->ShutterMode, td->DMsettling,
                  td->DivideBy2, td->Corrections));
              } else {
                CallDMCamera(pGatan, td->amtCam, GetGainReference((float *)ref->Array, 
                  &arrSize, &width, &height, binning));
              }
              
              if (width != ref->SizeX || height != ref->SizeY) {
                message.Format("%s reference is %d by %d, not the expected size of"
                  " %d by %d", strGainDark, width, height, ref->SizeX, ref->SizeY);
                DeferMessage(td, message);
                retval = 1;
              } 
              ref->TimeStamp = 0.001 * GetTickCount();
            }
            catch (_com_error E) {
              long errCode = -1;
              try {
                if (td->PluginVersion >= PLUGIN_HAS_LAST_ERROR)
                  CallGatanCamera(pGatan, GetLastError(&errCode));
              }
              catch (...) {
              }
              retval = 1;
              if (td->MakeFEIerrorTimeout) {

                // Turn into a timeout
                delete [] ref->Array;
                ref->Array = NULL;
                SEMTrace('1', "Error obtaining %s reference, turning into a "
                  "timeout by sleeping %d", iGainDark ? "dark" : "gain",
                  td->cameraTimeout + td->blankerTimeout +3000);
                Sleep(td->cameraTimeout + td->blankerTimeout + 3000);
              }
              if (iGainDark)
                message.Format("obtaining dark reference - %s", 
                  errCode > 0 ? SEMCCDErrorMessage(errCode) : "");
              else
                message.Format("obtaining gain reference - %s\n\n"
                  "Is there a gain reference?  Check Digital Micrograph "
                  "for error messages.\n\nDescription: ", 
                  errCode > 0 ? SEMCCDErrorMessage(errCode) : "");
              CCReportCOMError(td, E, (LPCTSTR)message);
            }
            if (retval) {
              delete [] ref->Array;
              ref->Array = NULL;
            }
          }
        }
        ref = td->DarkToGet;
        strGainDark = "Dark";
      }
      ReleaseDMCamera(pGatan);
    } else {
      
      // Just ensuring the references before timed blanking
      // Take care of deleting any dark refs first
      for (int i = 0; i < td->NumDRDelete; i++) {
        sprintf(CamCommand + camPlace, 
          "image dark%d := SSCGetDarkReference(%g, %d, %d, %d, %d, %d )\r"
          "DeleteImage(dark%d)\r", i,
          td->DRdelete[i].Exposure,
          td->DRdelete[i].Binning,
          td->DRdelete[i].Top,
          td->DRdelete[i].Left,
          td->DRdelete[i].Bottom,
          td->DRdelete[i].Right, i);
        camPlace = (int)strlen(CamCommand);
      }
      td->NumDRDelete = 0;
      
      // This plugin handles the settling and shutter
      //pGatan->SetCurrentCamera(td->Camera);
      camPlace = (int)strlen(CamCommand);
      
      // Need to get the gain reference too because that will delay the shot
      sprintf(CamCommand + camPlace, 
        "image tmp := SSCGetDarkReference(%g, %d, %d, %d, %d, %d )\r",
        td->Exposure, binning, td->Top, td->Left, td->Bottom, td->Right);
      if (td->Processing == GAIN_NORMALIZED) {
        camPlace = (int)strlen(CamCommand);
        sprintf(CamCommand + camPlace, "image tmp2 := SSCGetGainReference(%d)\r",
          binning);
      }
      
      try {
        long strSize = (int)strlen(CamCommand) / 4 + 1;
        double scriptRet;
        CallGatanCamera(pGatan, 
          ExecuteScript(strSize, (long *)CamCommand, 1, &scriptRet));
      }
      catch (_com_error E) {
        retval = 1;
        CCReportCOMError(td, E, _T("executing script to ensure dark ref "));
      }
      ReleaseDMCamera(pGatan);
    }
 
  } else if (td->DarkToGet && td->DE_camType) {

    // DE camera getting a dark reference
    // DNM: removed code for GainToGet which will never happen
    if (td->DarkToGet) {
      td->DE_Cam->SetLastErrorString("");
      ref = td->DarkToGet;
      retval = GetArrayForReference(td, ref, arrSize, strGainDark);
      if (retval != 1) {
        retval = td->DE_Cam->setPreExposureTime((long) (1000 * td->DMsettling));

        // Calculate the proper ROI and set the binning
        if (!retval)
          retval = td->DE_Cam->setROI(td->Left * binning, td->Top * binning, 
            td->CallSizeX * binning, td->CallSizeY * binning, -1);
        if (!retval)
          retval = td->DE_Cam->setBinning(binning, binning, td->CallSizeX, td->CallSizeY,
          -1);
        if (!retval)
          retval = td->DE_Cam->AcquireDarkImage((float)ref->Exposure);	
        bool imageFinished = false;
        while (!imageFinished && !retval) {  
          //Here because of the way it works it will
          //take "small naps" and wait until it is finished
          // DE12 etc do not work that way: this is the call that acquires image
          if(td->DE_Cam->isImageAcquistionDone()) {
            imageFinished = true;
            retval = td->DE_Cam->copyImageData((unsigned short*) ref->Array, 
              td->CallSizeX, td->CallSizeY, td->DivideBy2);
          } else
            ::Sleep(100);

          //need this for time stamp
          ref->TimeStamp = 0.001 * GetTickCount();
        }
        if (retval) {
          delete [] ref->Array;
          ref->Array = NULL;
          if (retval > 0) {
            str = td->DE_Cam->GetLastErrorString();
            message.Format("Error %d setting parameters or getting dark reference from DE"
              " camera; %s", retval, B3DCHOICE(str.IsEmpty(), GetDebugOutput('D') ? 
              "see debug output" : "Set Debug Output to D for details", (LPCTSTR)str));
            DeferMessage(td, message);
          }
        }
      }
    }

  } else if (td->DarkToGet && td->FEItype) {

    // FEI Camera
    ref = td->DarkToGet;
    retval = GetArrayForReference(td, ref, arrSize, strGainDark);
    td->Exposure = ref->Exposure;

    if (retval != 1)
       retval = AcquireFEIimage(td, ref->Array, UNPROCESSED, 0., ref->SizeX, ref->SizeY, 
        1);
    td->Exposure = expSave;
    ref->TimeStamp = 0.001 * GetTickCount();
    if (retval && ref->Array) {
      delete [] ref->Array;
      ref->Array = NULL;
    }
  } else if (td->DarkToGet && td->plugFuncs) {

    // Plugin camera including Tietz, which needs a signal that a dark reference is
    // being taken
    ref = td->DarkToGet;
    retval = GetArrayForReference(td, ref, arrSize, strGainDark);
    if (!retval) {
      AcquirePluginImage(td, &ref->Array, arrSize, 
        td->TietzType ? TIETZ_GET_DARK_REF : UNPROCESSED, 0., false, sizeX,
        sizeY, retval, numDum);
      if (retval) {
        message.Format("Error %d setting parameters or getting dark reference from plugin"
          " camera", retval);
        if (td->plugFuncs->GetLastErrorString)
          message += ":\n" + CString(td->plugFuncs->GetLastErrorString());
        DeferMessage(td, message);
      } else if (sizeX != ref->SizeX || sizeY != ref->SizeY) {
        message.Format("Dark reference is %d by %d, not the expected size of"
          " %d by %d", sizeX, sizeY, ref->SizeX, ref->SizeY);
        DeferMessage(td, message);
        retval = 1;
      } 
      ref->TimeStamp = 0.001 * GetTickCount();
    }
    if (retval && ref->Array) {
      delete [] ref->Array;
      ref->Array = NULL;
    }

  }
  CoUninitialize();
  return retval;
}
/* Timing test code:
  srand( (GetTickCount() << 6) & 0xFFFF );
  DWORD delay = (DWORD)(2. * td->cameraTimeout * (float)rand() / RAND_MAX);
  SEMTrace('1', "Dark ensuring thread sleeping for %d", delay);
  Sleep(delay); */

// Get image or dark reference from plugin
void CCameraController::AcquirePluginImage(CameraThreadData *td, void **array, 
                                           int arrSize, int processing, double settling, 
                                           bool blanker, int &sizeX, int &sizeY, 
                                           int &retval, int &numAcquired)
{
  bool tietzDark = td->TietzType && processing == TIETZ_GET_DARK_REF;
  bool tietzImage = td->TietzType && !tietzDark;
  int flags = td->DivideBy2 ? PLUGCAM_DIVIDE_BY2 : 0;
  if (tietzImage && td->RestoreBBmode)
    flags |= TIETZ_RESTORE_BBMODE;

  // Do the selection for Tietz dark reference but not image
  if (!retval && td->plugFuncs->SelectCamera && !tietzImage)
    retval = td->plugFuncs->SelectCamera(td->SelectCamera);

  // Shutter selection for Tietz is handled by the Prepare call
  if (!retval && td->plugFuncs->SelectShutter && !td->STEMcamera && !td->TietzType)
    retval = td->plugFuncs->SelectShutter(td->ShutterMode);

  // For some reason the "CallSize" is passed for Tietz dark reference but not image
  if (!retval)
    retval = td->plugFuncs->SetAcquiredArea(td->Top * td->Binning, td->Left * 
    td->Binning, (tietzDark ? td->CallSizeX : td->DMSizeX) * td->Binning, 
    (tietzDark ? td->CallSizeY : td->DMSizeY) * td->Binning, td->Binning);
  if (!retval)
    retval = td->plugFuncs->SetExposure(td->STEMcamera ? td->PixelTime : td->Exposure, 
    settling);
  if (!retval && td->plugFuncs->SetAcquireFlags) {
    if (td->STEMcamera)
      td->plugFuncs->SetAcquireFlags(td->PlugSTEMacquireFlags);
    else
      td->plugFuncs->SetAcquireFlags(flags); 
  }

  // Do preliminary steps for Tietz that were always before starting blanker
  if (!retval && tietzImage)
    retval = td->plugFuncs->PrepareForAcquire(td->TietzType, td->ShutterMode);
  if (!retval && blanker)
    StartBlankerThread(td);

  // Get the image
  if (!retval) {
    numAcquired = 1;
    if (td->STEMcamera)
      retval = td->plugFuncs->AcquireSTEMImage((short **)array, arrSize, td->NumChannels,
      td->ChannelIndex, td->STEMrotation, td->integration, &sizeX, &sizeY, &numAcquired);
    else
      retval = td->plugFuncs->AcquireImage(((short **)array)[0], arrSize, processing, 
        &sizeX, &sizeY);
  }
  if (retval && (processing & CONTINUOUS_USE_THREAD))
    td->plugFuncs->StopContinuous();
}

// Check whether ensuring dark ref is still going on
int CCameraController::EnsureBusy()
{
  return UtilThreadBusy(&mEnsureThread);
}

// When the ensure dark is done, do the real acquire
void CCameraController::EnsureDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->StartAcquire();
}

void CCameraController::EnsureError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->EnsureCleanup(error);
}

void CCameraController::EnsureCleanup(int error)
{
  CString report;
  if (error == IDLE_TIMEOUT_ERROR) {
    // If there was a timeout, and ensure thread still running, suspend 
    // and delete it
    if (mEnsureThread) {
      UtilThreadCleanup(&mEnsureThread);
      if (mParam->GatanCam && mParam->useSocket)
        CBaseSocket::CloseBeforeNextUse(GATAN_SOCK_ID);
    }
  }
  mEnsuringDark = false;
  if (!mParam->noShutter)
    mScope->BlankBeam(false);
  if (error == IDLE_TIMEOUT_ERROR) {

    // Restart whole capture two more times on timeout
    report.Format("Timeout occurred acquiring/ensuring dark reference for %s image",
      (LPCTSTR)CurrentSetName());
    if (mNumRetries < mRetryLimit && !mHalting) {
      mNumRetries++;
      ErrorCleanup(0);
      mWinApp->AppendToLog(report + ", retrying", LOG_SWALLOW_IF_CLOSED);
      Capture(mLastConSet, true);
      return;
    }
    mWinApp->mTSController->TSMessageBox(report);
  }
  mHalting = false;
  ErrorCleanup(error);
}

///////////////////////////////////////////////////////
//  Image acquisition routines
///////////////////////////////////////////////////////

// Start the image acquisition or another dark reference
void CCameraController::StartAcquire()
{
  CString setText = CurrentSetName();
  setText.MakeUpper();
  CString message;
  DWORD sleepTime = 1000;
  DWORD sleepIncrement = 100;
  DWORD sleepBite;
  DWORD retrySleep = 2000;
  int busy, arrSize, i, half;
  double delISX, delISY, delX, delY;
  ScaleMat aMat;
  DarkRef *ref;
  unsigned short *usdata;
  short *sdata;
  float refMean, refSD;
  float darkCrit = mParam->darkXRayAbsCrit;
  StageThreadData stData;
  bool moreDarkRefs, redoBadRef = false;
  DWORD ticks = GetTickCount();
  if (mParam->unsignedImages && mDivideBy2)
    darkCrit /= 2.;
  if (setText == "TRACK")
    setText = "IN TASK";

  // In continuous mode, reduce increment to be able to catch the thread ending quicker
  // And if a macro is using it, stop watching thread and add idle task much sooner so
  // the macro can proceed
  if (mTD.ContinuousSTEM || (mTD.ProcessingPlus & CONTINUOUS_USE_THREAD)) {
    sleepIncrement = 10;
    if (mWinApp->mMacroProcessor->DoingMacro() && 
      mWinApp->mMacroProcessor->GetUsingContinuous())
      sleepTime = 50;
  }
  
  // Clear flag from previous operation; abort if halting
  if (mEnsuringDark) {

    // Try to convert a gain reference
    if (mTD.GainToGet && mScaledGainRefMax) {
      ref = mTD.GainToGet;
      arrSize = ref->SizeX * ref->SizeY;

      // Try to convert to a scaled integer gain ref.  First correct
      // defects to prevent high values in bad areas there from
      // ruining conversion. If it fails, just forget it
      NewArray(usdata,unsigned short, arrSize);
      CorDefCorrectDefects(&mParam->defects, ref->Array, kFLOAT, ref->Binning,
          0, 0, ref->SizeY, ref->SizeX);
      if (ProcConvertGainRef((float *)ref->Array, usdata, arrSize,
        mScaledGainRefMax, mMinGainRefBits, &ref->GainRefBits)) {
        if (usdata)
          delete [] usdata;
      } else {
        
        // Success: replace float array
        delete [] ref->Array;
        ref->Array = usdata;
        ref->ByteSize = 2;
      }
    }
    mTD.GainToGet = NULL;

    // Unblank beam if no more dark references (i.e., leave it in same state that it was
    // set before starting dark references) then process dark reference if any
    moreDarkRefs = !mHalting && mTD.DarkToGet && mAverageDarkCount + 1 < mNumAverageDark;
    if (!moreDarkRefs && mParam->noShutter < 2) {
      mScope->BlankBeam(false);
      if (mParam->postBlankerDelay >= 0.001)
        Sleep(B3DNINT(1000. * mParam->postBlankerDelay));
    }

    if (mTD.DarkToGet) {

      // remove X rays from a dark reference
      ref = mTD.DarkToGet;
      mWinApp->mProcessImage->RemoveXRays(mParam, ref->Array, mTD.ImageType, ref->Binning,
        ref->Top, ref->Left, ref->Bottom, ref->Right, darkCrit,
        mParam->darkXRayNumSDCrit, mParam->darkXRayBothCrit, mDebugMode);
      arrSize = ref->SizeX * ref->SizeY;
      sdata = (short *)ref->Array;
      if (mDarkMaxMeanCrit > 0. || mDarkMaxSDcrit > 0. && arrSize > 100) {

        // If testing either mean or SD, get central 80% mean/SD
        CorDefSampleMeanSD(sdata, mTD.ImageType, ref->SizeX, ref->SizeX, ref->SizeY, 
          ref->SizeX / 10, ref->SizeY / 10, 4 * ref->SizeX / 5, 4 * ref->SizeY / 5, 
          &refMean, &refSD);
        redoBadRef = (mDarkMaxMeanCrit > 0. && refMean > mDarkMaxMeanCrit) ||
          (mDarkMaxSDcrit > 0. && refSD > mDarkMaxSDcrit);
        if (redoBadRef) {
          mBadDarkCount++;
          message.Format("Bad dark reference obtained with mean of %.1f and/or SD"
            " of %.1f\r\nThis is above the criterion - %s", refMean, refSD, 
            mBadDarkCount > mBadDarkNumRetries ? "giving up" : "retrying");

          // If run out of trials, delete arrays and abort the operation
          if (mBadDarkCount > mBadDarkNumRetries) {
            SEMMessageBox(message);
            if (mAverageDarkCount > 0) {
              delete [] mDarkSum;
              mDarkSum = NULL;
            }
            delete [] ref->Array;
            ref->Array = NULL;
            mEnsuringDark = false;
            ErrorCleanup(1);
            mHalting = false;
            return;
          }

          // Otherwise toggle the blanker after some delays
          mWinApp->AppendToLog(message);
          mScope->BlankBeam(false);
          Sleep(retrySleep);
          mScope->BlankBeam(true);
          Sleep(retrySleep);
        }
      }
      if (!redoBadRef)
        mAverageDarkCount++;

      // If this is the first dark ref and more are to be taken, get array for sum
      if (!redoBadRef && moreDarkRefs && mAverageDarkCount < 2) {
        NewArray(mDarkSum, int, arrSize);
        if (!mDarkSum) {
          mWinApp->AppendToLog("FAILED TO GET MEMORY FOR AVERAGING DARK REFERENCE!", 
            LOG_MESSAGE_IF_CLOSED);
          moreDarkRefs = false;
        } else {
          for (i = 0; i < arrSize; i++)
            mDarkSum[i] = sdata[i];
        }
      }

      // If this is a later dark reference, add the array in
      if (!redoBadRef && mAverageDarkCount > 1) {
        for (i = 0; i < arrSize; i++)
          mDarkSum[i] += sdata[i];
      }

      // If there are more to get, start thread and return
      if (moreDarkRefs || redoBadRef) {
        StartEnsureThread(mTD.cameraTimeout);
        if (mDebugMode) {
          message.Format("%.3f getting next dark ref, %u elapsed", 0.001 * 
            (ticks % 3600000), ticks - mStartTime);
          mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
        }
        return;

        // Or, If this is the last dark reference of series, put average back in array
      } else if (mAverageDarkCount > 1) {
        half = mAverageDarkCount / 2;
        for (i = 0; i < arrSize; i++)
          sdata[i] = (mDarkSum[i] + half) / mAverageDarkCount;
        delete [] mDarkSum;
        mDarkSum = NULL;
      }  
      TestGainFactor(sdata, ref->SizeX, ref->SizeY, ref->Binning);
    }
  }

  mEnsuringDark = false;
  if (mHalting) {
    StopContinuousSTEM();
    ErrorCleanup(1);
    mHalting = false;
    return;
  }

  if (mDebugMode) {
    DWORD ticks = GetTickCount();
    message.Format("%.3f start acquiring, %u elapsed", 0.001 * (ticks % 3600000),
      ticks - mStartTime);
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  }

  // Assign images the original IS if a shift is retained to avoid confusing TSC
  if (mShiftedISforSTEM) {
    mStartingISX = mIStoRestoreX;
    mStartingISY = mIStoRestoreY;
  } else
    mScope->FastImageShift(mStartingISX, mStartingISY);

  // Shift for subarea in STEM
  if (mParam->STEMcamera && !mShiftedISforSTEM) {
    mTotalSTEMShiftDelay = 0;
    delX = delY = 0.;
    if (mParam->subareaInCorner) {

      // Get coordinate of center point of scan relative to center of image
      // This is taken positive because it is a coordinate and will be transformed to a
      // a coordinate by the matrix
      delX = (mTD.Binning * mTD.DMSizeX - mParam->sizeX) / 2;
      delY = (mParam->sizeY - mTD.Binning * mTD.DMSizeY) / 2;
      if (delX || delY) {
        if (mParam->rotationFlip) {
          delISY = (mParam->rotationFlip % 4) * 90. * DTOR;
          delISX = delX * cos(delISY) - delY * sin(delISY);
          delY = delX * sin(delISY) + delY * cos(delISY);
          delX = (mParam->rotationFlip / 4) ? -delISX : delISX;
        }
        mTotalSTEMShiftDelay = mSubareaShiftDelay;
      }
    }

    // If adjusting shifts, take the negative because we are given image shifts and
    // we always need the negative when converting a shift in the image instead of
    // coordinates to an IS value with the matrix
    if (mAdjustShiftX || mAdjustShiftY) {
      SEMTrace('s', "Applying shift %.2f %.2f", mAdjustShiftX, mAdjustShiftY);
      delX -= mAdjustShiftX;
      delY -= mAdjustShiftY;
      mAdjustShiftX = mAdjustShiftY = 0.;
      mTotalSTEMShiftDelay += mAdjustmentShiftDelay;
    }
    if (delX || delY) {
      aMat = mShiftManager->CameraToIS(mMagBefore);
      if (aMat.xpx) {
        SEMTrace('s', "Total shift %.2f %.2f", delX, delY);
        delISX = delX * aMat.xpx + delY * aMat.xpy;
        delISY = delX * aMat.ypx + delY * aMat.ypy;
        mScope->SetImageShift(mStartingISX + delISX, mStartingISY + delISY);
        mShiftedISforSTEM = true;
        mIStoRestoreX = mStartingISX;
        mIStoRestoreY = mStartingISY;
        Sleep(mTotalSTEMShiftDelay);
      } else
        mWinApp->AppendToLog("WARNING: There is no IS calibration at this mag to correct "
        "a subarea or binning/exposure shift");
    }
  }

  // Set up beam for scan or dynamic focus now
  if (mTD.ScanTime > 0.) {
    mScope->SetDefocus(mTD.FocusBase);
    mScope->SetBeamShift(mTD.BeamBaseX, mTD.BeamBaseY);
  } else if (mTD.DynFocusInterval > 0) {
    double extra = mTD.FocusBase > mCenterFocus ? 1. : -1.;
    mScope->SetDefocus(mTD.FocusBase + extra);
    mScope->SetDefocus(mTD.FocusBase);
    Sleep(mStartDynFocusDelay);
  }
  mStartedScanning = true;

  // Unblank now for noShutter = 2 camera with known timing; it will get reblanked in
  // the blanker thread
  if (mParam->noShutter == 2 && mTD.UnblankTime <= mTD.MinBlankTime)
    mScope->BlankBeam(false);
  if (mBlankNextShot)
    mScope->BlankBeam(true);

  // Now set flag and start thread for actual acquisition
  mAcquiring = true;
  mAcquireThread = AfxBeginThread(AcquireProc, &mTD, THREAD_PRIORITY_HIGHEST, 0, 
    CREATE_SUSPENDED);
  TRACE("AcquireProc created with ID 0x%0x\n",mAcquireThread->m_nThreadID);
  mAcquireThread->m_bAutoDelete = false;
  if (mTD.ScanTime)
    mScope->SuspendUpdate((int)mTD.ScanTime + mTD.ScanDelay);
  else if (mTD.DynFocusInterval > 0)
    mScope->SuspendUpdate((int)mTD.PostActionTime + mTD.ScanDelay);

  // Take care of our window stuff, then sleep right after starting thread
  // to keep from delaying shot too much
  mWinApp->SetStatusText(SIMPLE_PANE, "ACQUIRING " + setText);
  if (mRepFlag >= 0 && !mPreventUserToggle)
    mWinApp->SetStatusText(B3DCHOICE(mWinApp->mProcessImage->GetLiveFFT(), COMPLEX_PANE, 
    MEDIUM_PANE), "Esc or Space to STOP");
  mWinApp->UpdateBufferWindows();
  mAcquireThread->ResumeThread();
  if (FEIscope && mTiltDuringShotDelay >= 0) {
    PrintfToLog("FREEZING THE USER INTERFACE UNTIL TILTING IS DONE");
    if (mTiltDuringShotDelay > 0)
      Sleep(mTiltDuringShotDelay);
    try {
      stData.info = &mTD.MoveInfo;
      CEMscope::StageMoveKernel(&stData, false, false, delISX, delISY, delX, delY);
    }
    catch (_com_error E) {
      SEMReportCOMError(E, "Doing continuous stage tilt during shot ");
    }
  } else if (mTD.ShutterTime) {
    sleepTime += mTD.ShutterTime + (DWORD)(1000. * mParam->startupDelay);
  } else if (mTD.PreDarkBinning) {
    sleepTime += (DWORD)(1000. * (mTD.PreDarkExposure + mParam->startupDelay +
      mParam->minimumDrift));
  }
  
  // Take little naps, check for thread completion
  while (sleepTime > 0) {
    sleepBite = sleepTime < sleepIncrement ? sleepTime : sleepIncrement;
    ::Sleep(sleepBite);
    sleepTime -= sleepBite;
    busy = ThreadBusy();
    if (!busy) {
      mWinApp->SetIdleBaseCount(102);
      break;
    }

    // If error, call error function and return;
    if (busy < 0) {
      AcquireError(busy);
      return;
    }
  }
  // If fall through, put task on list
  mWinApp->AddIdleTask(ThreadBusy, AcquireDone,
    AcquireError, 0, mTD.cameraTimeout + mTD.blankerTimeout);
}

// Routine for the acquire thread
UINT CCameraController::AcquireProc(LPVOID pParam)
{
  char CamCommand[2048];
  int camPlace = 0;
  long arrSize, ldum1;
  double ddum1, ddum2;
  HRESULT hr;
  bool imageOK = false, askedForContinuous = false, rampStarted = false;
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CameraThreadData *td = (CameraThreadData *)pParam;
  CameraThreadData pdTD;
  bool startBlanker = td->PostActionTime || td->UnblankTime || td->FocusStep1 || 
    td->TiltDuringDelay;
  int sizeX, sizeY, chan, numChan, retval = 0, numCopied = 0;
  CString report, str;
  long binning = td->Binning;

  // For any camera, wait to start if this is set
  if (td->ShotDelayTime)
    Sleep(td->ShotDelayTime);

  // For GATAN or AMT
  if (!td->Simulation && !(td->TietzType || td->FEItype || td->DE_camType || 
    td->plugFuncs)) {
    td->psa = NULL;
    td->Array[0] = NULL;
    IDMCamera *pGatan;
    HRESULT hr;
    CreateDMCamera(pGatan);
    if(CCTestHResult(td, hr, "getting instance of Camera ")) {
      retval = 1;
    } else if (td->GetDeferredSum) {

      // Get a deferred sum
      arrSize = td->DMSizeX * td->DMSizeY;
      retval = GetArrayForImage(td, arrSize);
      if (!retval) {
        try {
          CallGatanCamera(pGatan, ReturnDeferredSum(td->Array[0], &arrSize, 
                &td->DMSizeX, &td->DMSizeY));
          imageOK = true;
          if (td->SaveFrames) {
             CallGatanCamera(pGatan, GetFileSaveResult(&td->NumFramesSaved, 
                &td->ErrorFromSave));
          }
          if (td->UseFrameAlign) {
            CallGatanCamera(pGatan, FrameAlignResults(&td->FaRawDist, &td->FaSmoothDist,
              &td->FaResMean, &td->FaMaxResMax, &td->FaMeanRawMax, &td->FaMaxRawMax,
              &td->FaCrossHalf, &td->FaCrossQuarter, &td->FaCrossEighth, &td->FaHalfNyq,
              &ldum1, &ddum1, &ddum2));
          }

        }
        catch (_com_error E) {
          if (imageOK) {
            td->ErrorFromSave = -1;
          } else {
            long errCode = -1;
            try {
              CallGatanCamera(pGatan, GetLastError(&errCode));
            }
            catch (...) {
            }
            report.Format("Getting deferred sum from DigitalMicrograph - %s\n\n"
              "Description: ", errCode > 0 ? SEMCCDErrorMessage(errCode) : "");
            CCReportCOMError(td, E, report);
            retval = 1;
            delete [] td->Array[0];
            td->Array[0] = NULL;
          }
        }
      }
    } else {
      if (td->ReblankTime || td->PostActionTime) {
        try {
          CallDMCamera(pGatan, td->amtCam, selectCamera(td->SelectCamera));
        }
        catch (_com_error E) {
          retval = 1;
          CCReportCOMError(td, E, _T("Selecting camera "));
        }
      }
      
      // Alternatively, may need to do a pre-exposure dark reference
      if (!retval && td->PreDarkBinning) {
        // If we need to blank part of it, fire off blanker thread
        if (td->PreDarkBlankTime) {
          pdTD = *td;
          pdTD.UnblankTime = td->PreDarkBlankTime;
          pdTD.ReblankTime = pdTD.PostActionTime = pdTD.DriftISinterval = 0;
          pdTD.PostChangeMag = pdTD.PostImageShift = pdTD.PostMoveStage = false;
          td->blankerThread = StartBlankerThread(&pdTD);
        }
        try {
          sprintf(CamCommand + camPlace, 
            "image tmp := SSCGetDarkReference(%g, %d, 0, 0, 8, 8)\r",
            td->PreDarkExposure, td->PreDarkBinning);
          double scriptRet;
          CallGatanCamera(pGatan, 
            ExecuteScript((int)strlen(CamCommand) / 4 + 1, (long *)CamCommand, 1,
            &scriptRet));
      //    pGatan->QueueScript(strlen(CamCommand) / 4 + 1, (long *)CamCommand);
        }
        catch (_com_error E) {
          retval = 1;
          CCReportCOMError(td, E, _T("Getting pre-exposure dark reference "));
        }
        
        // clean up blanker thread
        if (td->PreDarkBlankTime) {
          retval = WaitForBlankerThread(td, 15000, 
            _T("Blanking thread did not end properly in pre-exposure dark reference"));
        }
      }
      
      if (!retval) {
        
        // Start dynamic focus ramper for FEI scope
        if (FEIscope && td->STEMcamera && (td->UnblankTime != 0 || 
          (td->DynFocusInterval && td->PostActionTime))) {
            retval = StartFocusRamp(td, rampStarted);

        // Or start thread to do extra blanking
        } else if (startBlanker || td->ReblankTime || td->ScanTime > 0.) {
          StartBlankerThread(td);
        }
      }
        
      if (!retval) {
        if (td->STEMcamera) {

          // For STEM, get one array, acquire the images, then get additional arrays
          // and the stored images.
          retval = GetArrayForImage(td, arrSize);
          if (!retval) {
            try {
              DWORD startSTEM = GetTickCount();
              SEMTrace('s', "Start AcquireImage");
              chan = 0;
              CallGatanCamera(pGatan, AcquireDSImage(td->Array[0], &arrSize, &td->DMSizeX, 
                &td->DMSizeY, td->STEMrotation, td->PixelTime, td->LineSyncAndFlags, 
                td->ContinuousSTEM, td->NumChannels, td->ChannelIndex, td->DivideBy2));
              numCopied = 1;
              SEMTrace('s', "Returned from AcquireImage  took %d", GetTickCount() - startSTEM);
              for (chan = 1; chan < td->NumChannels; chan++) {
                retval = GetArrayForImage(td, arrSize, chan);
                if (retval)
                  break;
                CallGatanCamera(pGatan, ReturnDSChannel(td->Array[chan], &arrSize, 
                  &td->DMSizeX, &td->DMSizeY, td->ChannelIndex[chan], td->DivideBy2));
                numCopied++;
              }
            }
            catch (_com_error E) {
              if (numCopied)
                CCReportCOMError(td, E, "Getting additional channels of DigiScan Data");
              else
                CCReportCOMError(td, E, "Acquiring an image with DigiScan");
              retval = 1;
              delete [] td->Array[chan];
              td->Array[chan] = NULL;
            }
          }
 
          // Get statistics back from focus ramp, make sure it ends
          if (FEIscope && td->DynFocusInterval && td->PostActionTime && 
            td->STEMcamera) {
              FinishFocusRamp(td, rampStarted);
          }

          // If one channel is returned OK, cancel the error and set channel count
          if (retval && numCopied) {
            retval = 0;
            td->NumChannels = numCopied;  
          }
        } else {

          // Take care of deleting any dark refs first (already done if fancy timing)
          for (int i = 0; i < td->NumDRDelete; i++) {
            sprintf(CamCommand + camPlace, 
              "image dark%d := SSCGetDarkReference(%g, %d, %d, %d, %d, %d )\r"
              "DeleteImage(dark%d)\r", i,
              td->DRdelete[i].Exposure,
              td->DRdelete[i].Binning,
              td->DRdelete[i].Top,
              td->DRdelete[i].Left,
              td->DRdelete[i].Bottom,
              td->DRdelete[i].Right, i);
            camPlace = (int)strlen(CamCommand);
          }

          try {
            long shutterDMticks = (int)(0.06 * td->ShutterTime);
            if (td->ShutterTime && !shutterDMticks)
              shutterDMticks = 1;
            if (camPlace) {
              CallGatanCamera(pGatan, QueueScript(camPlace / 4 + 1, (long *)CamCommand));
            }
            retval = GetArrayForImage(td, arrSize);
            if (!retval) {
              td->ErrorFromSave = 0;
              if (td->NeedsReadMode) {
                if (td->GatanReadMode == -1) {
                  CallGatanCamera(pGatan, SetReadMode(td->GatanReadMode, 
                    td->CountScaling));
                } else if (td->GatanReadMode < 0) {
                  CallGatanCamera(pGatan, SetK2Parameters2(td->GatanReadMode, 
                    td->CountScaling, 0, 0, 0., 0, 0, 0, td->K2ParamFlags,
                    td->DMSizeX + K2_REDUCED_Y_SCALE * td->DMSizeY, 
                    td->fullSizeX + K2_REDUCED_Y_SCALE * td->fullSizeY, 
                    0., 0., MAX_FILTER_NAME_LEN / 4, td->FilterName));
                } else {
                  CallGatanCamera(pGatan, SetK2Parameters2(td->GatanReadMode, 
                    td->CountScaling, td->FauxCamera ? 0 : 6, td->DoseFrac, td->FrameTime,
                    td->AlignFrames, td->SaveFrames, td->rotationFlip, td->K2ParamFlags,
                    td->DMSizeX + K2_REDUCED_Y_SCALE * td->DMSizeY, 
                    td->fullSizeX + K2_REDUCED_Y_SCALE * td->fullSizeY, 0., 0.,  
                    MAX_FILTER_NAME_LEN / 4, td->FilterName));
                }
              }
              //SEMTrace('1', "Calling GetAcquireImage with divideby2 %d", td->DivideBy2);
              askedForContinuous = (td->ProcessingPlus & CONTINUOUS_USE_THREAD) != 0;
              CallDMCamera(pGatan, td->amtCam, GetAcquiredImage(td->Array[0], &arrSize, 
                &td->DMSizeX, &td->DMSizeY, td->ProcessingPlus, td->Exposure, td->Binning, 
                td->Top, td->Left, td->Bottom, td->Right, td->ShutterMode,
                td->DMsettling, shutterDMticks, td->DivideBy2, td->Corrections));
              if (td->NeedsReadMode && td->GatanReadMode >= 0 && td->DoseFrac && 
                (td->SaveFrames || td->UseFrameAlign)) {
                  imageOK = true;
                  if (td->SaveFrames) {
                    CallGatanCamera(pGatan, GetFileSaveResult(&td->NumFramesSaved, 
                      &td->ErrorFromSave));
                  }
                  if (td->UseFrameAlign) {
                    CallGatanCamera(pGatan, FrameAlignResults(&td->FaRawDist, 
                      &td->FaSmoothDist, &td->FaResMean, &td->FaMaxResMax, 
                      &td->FaMeanRawMax, &td->FaMaxRawMax, &td->FaCrossHalf, 
                      &td->FaCrossQuarter, &td->FaCrossEighth, &td->FaHalfNyq,
                      &ldum1, &ddum1, &ddum2));
                  }
              }
            }
          }
          catch (_com_error E) {
            if (imageOK) {
              td->ErrorFromSave = -1;
            } else {
              long errCode = -1;
              try {
                if (td->PluginVersion >= PLUGIN_HAS_LAST_ERROR)
                  CallGatanCamera(pGatan, GetLastError(&errCode));
              }
              catch (...) {
              }
              if (askedForContinuous) {
                try {
                  CallGatanCamera(pGatan, StopContinuousCamera());
                }
                catch (...) {
                }
              }
              retval = 1;
              delete [] td->Array[0];
              td->Array[0] = NULL;
              if (td->MakeFEIerrorTimeout) {

                // Turn into a timeout
                SEMTrace('1', "Error getting image from camera, turning into a "
                  "timeout by sleeping %d", td->cameraTimeout + td->blankerTimeout +3000);
                Sleep(td->cameraTimeout + td->blankerTimeout + 3000);
              }
              report.Format("Getting image from camera - %s\n\n%sDescription: ",
                errCode > 0 ? SEMCCDErrorMessage(errCode) : "", sAmtCurrent ? "" : 
                "Check Digital Micrograph for error messages.\n\n");
              CCReportCOMError(td, E, (LPCTSTR)report);
            }
          }
        }
      }
      ReleaseDMCamera(pGatan);
    } 
 
  } else if (td->FEItype) {

    // FEI camera
    retval = GetArrayForImage(td, arrSize);
    if (!retval) {

      // Start dynamic focus
      if (td->STEMcamera && (td->UnblankTime != 0 || 
        (td->DynFocusInterval && td->PostActionTime))) {
          retval = StartFocusRamp(td, rampStarted);

      // Or start thread to do post actions
      } else if (startBlanker)
        StartBlankerThread(td);

      if (!retval && td->STEMcamera)
        retval = AcquireFEIchannels(td, td->DMSizeX, td->DMSizeY);
      else if (!retval)
        retval = AcquireFEIimage(td, td->Array[0], td->Processing, td->DMsettling,
          td->DMSizeX, td->DMSizeY, 0);
      if (retval) {
        delete [] td->Array[0];
        td->Array[0] = NULL;
      }

      // Get statistics back from focus ramp, make sure it ends
      if (td->DynFocusInterval && td->PostActionTime && td->STEMcamera) {
        FinishFocusRamp(td, rampStarted);
      }
      
    }
  } else if (td->DE_camType) {
    td->DE_Cam->SetLastErrorString("");

    // IMAGE from Direct Electron camera
    retval = GetArrayForImage(td, arrSize);

    // DNM: skip if there is a memory problem
    if (!retval) {

      // Make sure live mode is off before setting up shot
      if (td->DE_camType >= 2 && !(td->ProcessingPlus & CONTINUOUS_USE_THREAD))
        retval = td->DE_Cam->SetLiveMode(0); 

      // Set the processing
      if (!retval && td->DE_camType >= 2) 
        retval = td->DE_Cam->setCorrectionMode(td->ProcessingPlus & 3, td->GatanReadMode);

      // Set alignment in server if relevant
      if (!retval && td->AlignFrames >= 0)
        retval = td->DE_Cam->SetAlignInServer(td->AlignFrames);

      // Set the binning
      if (!retval) 
        retval = td->DE_Cam->setBinning(td->Binning, td->Binning, td->CallSizeX, 
          td->CallSizeY, td->UseHardwareBinning);

      // Set the preexposure time in milliseconds
      if (!retval)
        retval = td->DE_Cam->setPreExposureTime(1000. * td->DMsettling);

      // Calculate the proper ROI and set it. TM to support DE12 3_28_11
      if (!retval)
        retval = td->DE_Cam->setROI(td->Left * td->Binning, td->Top * td->Binning, 
          td->CallSizeX * td->Binning, td->CallSizeY * td->Binning, td->UseHardwareROI);

      // Set the operating mode if relevant and the FPS
      if (!retval)
        retval = td->DE_Cam->SetCountingParams(td->GatanReadMode, td->CountScaling, 
          td->FramesPerSec);

      // Start thread to do post actions
      if (!retval && startBlanker) {
        StartBlankerThread(td);
      }

      if (!retval)
        retval = td->DE_Cam->AcquireImage((float)td->Exposure);	

      // Turn on live mode if flag set
      if (!retval && td->DE_camType >= 2 && 
        (td->ProcessingPlus & CONTINUOUS_USE_THREAD))
        retval = td->DE_Cam->SetLiveMode(1 + 
          B3DCHOICE(td->ProcessingPlus & CONTINUOUS_SET_MODE, 1 , 0));

  	  bool imageFinished = false;
      while (!imageFinished && !retval) {

        //Here because of the way it works it will
        //take "small naps" and wait until it is finished
        // Actually the DE-12 will not take the image until the get_image call inside
        // CopyImageData, but this costs nothing to test first in this loop
        if (td->DE_Cam->isImageAcquistionDone()) {
          imageFinished = true;
          retval = td->DE_Cam->copyImageData((unsigned short*)td->Array[0], td->CallSizeX, 
            td->CallSizeY, td->DivideBy2);
        }	else
          ::Sleep(100);
      }
      if (retval) {
        if (retval > 0) {
          str = td->DE_Cam->GetLastErrorString();
          report.Format("Error setting parameters or getting image from DE camera; %s",
            B3DCHOICE(str.IsEmpty(), GetDebugOutput('D') ? "see debug output" : 
            "Set Debug Output to D for details", (LPCTSTR)str));
          DeferMessage(td, report);
        }
        delete [] td->Array[0];
        td->Array[0] = NULL;
      }
    }

  } else if (td->plugFuncs) {

    // PLUGIN camera or STEM: Get one or more arrays
    numChan = (td->STEMcamera ? td->NumChannels : 1);
    for (chan = 0; chan < numChan && !retval; chan++)
      retval = GetArrayForImage(td, arrSize, chan);

    if (!retval) {
      AcquirePluginImage(td, (void **)td->Array, arrSize, td->ProcessingPlus, 
        td->DMsettling, startBlanker, sizeX,
        sizeY, retval, numCopied);
      if (retval) {
        report.Format("Error %d setting parameters or getting image from plugin"
          " camera", retval);
        if (td->plugFuncs->GetLastErrorString)
          report += ":\n" + CString(td->plugFuncs->GetLastErrorString());
        DeferMessage(td, report);
      }
    }

    // Clean up unused arrays for error or partial return
    if (retval) 
      numCopied = 0;
    for (chan = numCopied; chan < numChan; chan++) {
      delete [] td->Array[chan];
      td->Array[chan] = NULL;
    }

  } else {
    
    // Simulation image
    if (td->ShutterTime)
      ::Sleep(td->ShutterTime);
    retval = 1;
    int height = td->DMSizeY;
    int width = td->DMSizeX;
    int iStart = 64;
    int i, j, iVal;
    int iDir = 1;
    int iBlockY = (height + 127)/ 128;
    int iBlockX = (width + 63) / 64;
    short int *cPixels;
    unsigned short int *usPixels;
    double factor = td->Exposure * td->Binning * td->Binning;
    SAFEARRAY *psa;
    SAFEARRAYBOUND rgsabound[2];
    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = width;
    rgsabound[1].lLbound = 0;
    rgsabound[1].cElements = height;
    
    psa = SafeArrayCreate(VT_I2, 2, rgsabound);
    if (psa) {
      
      // If creation successful, try to access data
      hr = SafeArrayAccessData(psa, (void **)&cPixels);
      if (hr == S_OK) {
        //  for (int rep = 0; rep < 50; rep++) {
        for (j = 0; j < height; j++)  {
          iVal = iStart;
          for (i = 0; i < width; i++) {
            cPixels[j * width + i] = 16*iVal;
            if (i % iBlockX == 0) 
              iVal += iDir; 
            if (iVal < -5)
              iVal = -5;
            if (iVal > 300)
              iVal = 300;
          }
          if (j % iBlockY == 0)
            iStart++;
          // iStart += j % 2;
          iDir = -iDir;
          //}
        }

        if (td->ImageType == kSHORT) {
          if (factor >= 7.9)
            factor = 7.9;
          for (i = 0; i < width * height; i++)
            cPixels[i] = (short)(factor * cPixels[i]);
        } else {
          if (factor >= 15.9)
            factor = 15.9;
          usPixels = (unsigned short *)cPixels;
          for (i = 0; i < width * height; i++)
            usPixels[i] = (short)(factor * usPixels[i]);

        }
        hr = SafeArrayUnaccessData(psa);
        
        // If everything OK, pass the array on; otherwise destroy it
        if (hr == S_OK) {
          td->psa = psa;
          //retval = 0;
          retval = GetArrayForImage(td, arrSize);
          if (!retval) {
            td->psa = NULL;
            retval = CopyFEIimage(td, psa, td->Array[0], width, height, td->ImageType, 
              td->DivideBy2);
            SafeArrayDestroy(psa);
          }

        } else
          SafeArrayDestroy(psa);
      }
    }
    if (td->PostChangeMag)
      td->MagTicks = ::GetTickCount();
    if (td->PostImageShift)
      td->ISTicks = ::GetTickCount();
    if (td->PostMoveStage)
      td->MoveInfo.finishedTick = ::GetTickCount(); 
  }

  // Deal with the blanking thread properly
  td->imageReturned = true;
  if (td->blankerThread) {
    retval = WaitForBlankerThread(td, td->blankerTimeout, 
      _T("Blanking/post action thread did not end properly after the exposure"));
  }
  CoUninitialize();
  return retval;
}

// Common place to start blanker proc and wait for mutex
CWinThread *CCameraController::StartBlankerThread(CameraThreadData *td)
{
  DWORD startTime;
  td->GotScopeMutex = false;
  td->stopWaitingForBlanker = false;
  td->blankerThread = AfxBeginThread(BlankerProc, td, THREAD_PRIORITY_HIGHEST, 0,
    CREATE_SUSPENDED);
  TRACE("BlankerProc created with ID 0x%0x\n",td->blankerThread->m_nThreadID);
  td->blankerThread->m_bAutoDelete = false;
  td->blankerThread->ResumeThread();

  // Watch for mutex (if we are using them at all)
  startTime = GetTickCount();
  while (UsingScopeMutex && !td->GotScopeMutex && 
    SEMTickInterval(startTime) < 10000) {
    Sleep(10);
  }
  //TRACE("It took %d ms to get the mutex\n", GetTickCount() - startTime);
  return td->blankerThread;
}

// Waits for blanker thread to be done then kills it if necessary
int CCameraController::WaitForBlankerThread(CameraThreadData * td, DWORD timeout,
                                            CString message)
{
  DWORD beginTime = GetTickCount();
  DWORD exitCode;
  int retval;
  bool firstTime = true;
  do {
    if (!firstTime) {
      ReleaseMutex(td->blankerMutexHandle);
      Sleep(50);
    }
    firstTime = false;
    if (td->stopWaitingForBlanker)
      return 1;
    WaitForSingleObject(td->blankerMutexHandle, BLANKER_MUTEX_WAIT);
    GetExitCodeThread(td->blankerThread->m_hThread, &exitCode);
    if (exitCode != STILL_ACTIVE)
      break;
  } while (SEMTickInterval(beginTime) < timeout);

  // Suspend it if still active, then delete
  // Yes, it is supposedly bad to call TerminateThread!  But without this, the scope mutex
  // is not released and others getting the mutex hang.  With it, the mutex IS released
  retval = exitCode;
  if (exitCode == STILL_ACTIVE) {
    retval = 1;
    DeferMessage(td, message);
    td->blankerThread->SuspendThread();
    TerminateThread(td->blankerThread->m_hThread, 0);
  }
  delete td->blankerThread;
  td->blankerThread = NULL;
  ReleaseMutex(td->blankerMutexHandle);
  return retval;
}

// Routine to handle extra blanking or post actions
UINT CCameraController::BlankerProc(LPVOID pParam)
{
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  CameraThreadData *td = (CameraThreadData *)pParam;
  StageMoveInfo *info = &td->MoveInfo;
  StageThreadData stData;
  int retval = 0;
  DWORD startTime, curTime, lastTime;
  double interval, elapsed, baseISX, baseISY, newISX, newISY, intervalSum, intervalSumSq;
  double destX, destY, destZ, destAlpha;
  TIMECAPS tc;
  BOOL periodSet = false;
  int  numScan;
  HRESULT hr = S_OK;
  stData.info = info;
  _variant_t *vFalse = new _variant_t(false);
  _variant_t *vTrue = new _variant_t(true);

  CEMscope::ScopeMutexAcquire("BlankerProc", true);
  td->GotScopeMutex = true;
  if (FEIscope && td->scopePlugFuncs->BeginThreadAccess(1, 
    PLUGFEI_MAKE_ILLUM | PLUGFEI_MAKE_PROJECT | PLUGFEI_MAKE_STAGE)) {
    DeferMessage(td, "Error creating second instance of microscope object\n"
      "for blanking beam or doing post-exposure actions");
    SEMErrorOccurred(1);
    retval = 1;
  }
  if (!retval) {
    try {

      // UnblankTime indicates an optional initial blanking
      if (td->UnblankTime) {
        if (td->UnblankTime > td->MinBlankTime) {
          CEMscope::SetBlankingFlag(true);
          td->scopePlugFuncs->SetBeamBlank(*vTrue);
          ::Sleep(td->UnblankTime - td->MinBlankTime);
          td->scopePlugFuncs->SetBeamBlank(*vFalse);
          CEMscope::SetBlankingFlag(false);
        } else

          // If it is too short, skip blanking and just sleep
          ::Sleep(td->UnblankTime);
      }

      // TiltDuringDelay indicates to start a stage tilt
      if (td->TiltDuringDelay) {
        Sleep(td->TiltDuringDelay);
        CEMscope::StageMoveKernel(&stData, true, true, destX, destY, destZ, destAlpha);
      }

      // set up precision timer for scan, dynamic focus, or drift
      if (td->ScanTime || td->DriftISinterval || td->DynFocusInterval) {
        if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
          periodSet = true;
          timeBeginPeriod(tc.wPeriodMin);
        }
        td->ScanIntMin = 100000.;
        td->ScanIntMax = 0.;
        intervalSum = 0.;
        numScan = -1;
        intervalSumSq = 0.;
      }

      // Scan time indicates do scanning
      // This is the only one that works only for FEI direct
      if (FEIscope && td->ScanTime) {
        double focus;

        ::Sleep(td->ScanDelay);
        startTime = lastTime = timeGetTime();

        while(1) {

          // Get the elapsed time and set shift and focus from base
          curTime = timeGetTime();
          elapsed = SEMTickInterval((double)curTime, (double)startTime);
          if (elapsed > td->ScanTime)
            break;
          //lastTime = timeGetTime();
          td->scopePlugFuncs->SetBeamShift(elapsed * td->BeamPerMsX + td->BeamBaseX,
            elapsed * td->BeamPerMsY + td->BeamBaseY);
          interval = (double)timeGetTime() - (double)curTime;
          if (interval < 33.) {
            focus = 1.e-6 * (elapsed * td->FocusPerMs + td->FocusBase);
            td->scopePlugFuncs->SetDefocus(focus);
          }

          AddStatsAndSleep(td, curTime, lastTime, numScan, intervalSum, intervalSumSq, 
            td->ScanSleep);
        }

      } else if (td->DriftISinterval) {

          // For drift compensation, get the base IS
        
        td->scopePlugFuncs->GetImageShift(&baseISX, &baseISY);
        startTime = lastTime = timeGetTime();
        while(1) {

          // Get the elapsed time
          curTime = timeGetTime();
          elapsed = SEMTickInterval((double)curTime, (double)startTime);
          if (elapsed > td->PostActionTime)
            break;

          // Add time corresponding to desired initial steps, compute IS and set it
          elapsed += td->InitialDriftSteps * td->DriftISinterval;
          newISX = elapsed * td->ISperMsX + baseISX;
          newISY = elapsed * td->ISperMsY + baseISY;
          td->scopePlugFuncs->SetImageShift(newISX, newISY);

          AddStatsAndSleep(td, curTime, lastTime, numScan, intervalSum, intervalSumSq, 
            td->DriftISinterval);
        }
          
        // Reset image shift to base value
        td->scopePlugFuncs->SetImageShift(baseISX, baseISY);

        // Dynamic focus or 1-2 focus steps
      } else if (td->DynFocusInterval || td->FocusStep1) {
        double focus, focusBase;
        long last_coarse, fineBase, coarseBase;
        SEMTrace('1', "Starting %s", td->DynFocusInterval ? "Dynamic focus" :
          "focus steps");

        // Get starting value in defocus units not microns
        if (!JEOLscope) {
          focusBase = 1.e6 * td->scopePlugFuncs->GetDefocus();
        } else {
          fineBase = td->JeolSD->objectiveFine;
          coarseBase = td->JeolSD->objective;
          last_coarse = coarseBase;
        }

        ::Sleep(td->ScanDelay);
        if (td->DynFocusInterval) {

          startTime = lastTime = timeGetTime();
          //SEMTrace('1', "Starting scan at base focus %.2f", focusBase);

          while(1) {

            // Get the elapsed time and set focus from base
            curTime = timeGetTime();
            elapsed = SEMTickInterval((double)curTime, (double)startTime);
            if (elapsed > td->PostActionTime)
              break;
            focus = elapsed * td->FocusPerMs;    // This is already in defocus units
            ChangeDynFocus(td, focusBase, focus, fineBase, coarseBase, 
              last_coarse);
            
            //if (numScan % 10 == 0) SEMTrace('1', "Elapsed %.0f  at focus %.2f  %.2f  %.2f  %f", elapsed, focus + focusBase, focusBase, elapsed * td->FocusPerMs, td->FocusPerMs);
            AddStatsAndSleep(td, curTime, lastTime, numScan, intervalSum, intervalSumSq, 
              td->DynFocusInterval);
          }

        } else {

          // One or two focus steps
          ChangeDynFocus(td, focusBase, td->FocusStep1, fineBase, coarseBase, 
            last_coarse);
          if (td->FocusStep2 && td->Step2FocusDelay) {
            ::Sleep(td->Step2FocusDelay);
            ChangeDynFocus(td, focusBase, td->FocusStep2, fineBase, 
              coarseBase, last_coarse);
          }
        }
        SEMTrace('1', "Done with %s", td->DynFocusInterval ? "Dynamic focus" :
          "focus steps");
      }

      // Done with precision timer; get statistics
      if (td->ScanTime || td->DriftISinterval || td->DynFocusInterval) {
        if (periodSet)
          timeEndPeriod(tc.wPeriodMin);
        td->ScanIntSD = 0.;
        td->ScanIntMean = 0.;
        if (numScan > 0) 
          td->ScanIntMean = intervalSum / numScan;
        if (numScan > 1)
          td->ScanIntSD = sqrt((intervalSumSq - numScan * td->ScanIntMean * 
          td->ScanIntMean) / (numScan - 1.));
      }

      // Now wait ReblankTime, and reblank the beam
      if (td->ReblankTime) {
        if (!(td->DriftISinterval || td->DynFocusInterval))
          ::Sleep(td->ReblankTime);
        td->scopePlugFuncs->SetBeamBlank(*vTrue);

        CEMscope::SetBlankingFlag(true);

        // Or wait the post-action time or until image returned
      } else if (td->PostActionTime && !(td->DriftISinterval || td->DynFocusInterval)) {
        double startTime = GetTickCount();
        while (!td->imageReturned) {
          int elapsed = (int)SEMTickInterval(startTime);
          if (elapsed >= td->PostActionTime)
            break;
          ::Sleep(B3DMIN(50, td->PostActionTime - elapsed));
        }
      }

      // START POST-ACTIONS HERE, FIRST MAG
      if (td->PostChangeMag) {
        CEMscope::AcquireMagMutex();
        if (td->PostMagFixIS)
          SEMTrace('i', "PostMag setting raw IS %.3f %.3f", td->ISX, td->ISY);

        if (JEOLscope) {  // JEOL
          td->scopePlugFuncs->SetMagnificationIndex(td->NewMagIndex);
          SEMSetJeolStateMag(td->NewMagIndex, true);
          if (td->PostMagFixIS) {
            td->scopePlugFuncs->UseMagInNextSetISXY(td->NewMagIndex);
            td->scopePlugFuncs->SetImageShift(td->ISX, td->ISY);
          }
       
        } else {   // Plugin
          if (td->scopePlugFuncs->SetAutoNormEnabled)
            td->scopePlugFuncs->SetAutoNormEnabled(*vFalse);
          td->scopePlugFuncs->SetMagnificationIndex(td->NewMagIndex);
          if (!td->STEMcamera)
            CEMscope::SetSecondaryModeForMag(td->NewMagIndex);
          if (td->scopePlugFuncs->NormalizeLens)
            td->scopePlugFuncs->NormalizeLens(pnmProjector);
          if (td->scopePlugFuncs->SetAutoNormEnabled)
            td->scopePlugFuncs->SetAutoNormEnabled(*vTrue);
          if (td->PostMagFixIS)
            td->scopePlugFuncs->SetImageShift(td->ISX, td->ISY);
        }
          
        td->MagTicks = ::GetTickCount();
        CEMscope::SetMagChanged(td->NewMagIndex);
        CEMscope::ReleaseMagMutex();
      }

      // NEXT IMAGE SHIFT - handle beam for Hitachi and JEOL
      if (td->PostImageShift) {
        double shiftX, shiftY, ISdelX, ISdelY;
        if (td->IStoBS.xpx && (HitachiScope || JEOLscope))
          td->scopePlugFuncs->GetImageShift(&shiftX, &shiftY);
        td->scopePlugFuncs->SetImageShift(td->ISX, td->ISY);  // And here in microns
        CEMscope::SetISChanged(td->ISX, td->ISY);
        if (td->IStoBS.xpx && (HitachiScope || JEOLscope)) {
          ISdelX = td->ISX - shiftX;
          ISdelY = td->ISY - shiftY;
          td->scopePlugFuncs->GetBeamShift(&shiftX, &shiftY);
          shiftX += td->IStoBS.xpx * ISdelX + td->IStoBS.xpy * ISdelY;
          shiftY += td->IStoBS.ypx * ISdelX + td->IStoBS.ypy * ISdelY;
          td->scopePlugFuncs->SetBeamShift(shiftX, shiftY);
        }
      }
      td->ISTicks = ::GetTickCount();

      if (td->TiltDuringDelay)
        CEMscope::WaitForStageDone(stData.info, "BlankerProc");

      if (td->PostMoveStage) {

        // Wait for stage ready for FEI at least, where it is simple
        if (FEIscope) {
          startTime = GetTickCount();
          while (SEMTickInterval(startTime) < 5000) {
            if ((retval = td->scopePlugFuncs->GetStageStatus()))
              Sleep(100);
            else
              break;
          }
        }
        if (retval)
          DeferMessage(td, "Stage not ready for doing post-exposure movement");
        else
          CEMscope::StageMoveKernel(&stData, true, false, destX, destY, destZ, destAlpha);
        SEMTrace('M', "BlankerProc finished stage move");
      }
    }
    catch (_com_error E) {
      CCReportCOMError(td, E, 
        _T("blanking/unblanking beam or doing post-exposure action "));
      retval = 1;
    }
  }
  if (FEIscope) {
    td->scopePlugFuncs->EndThreadAccess(1);
  }
  CoUninitialize();
  CEMscope::ScopeMutexRelease("BlankerProc");
  delete vTrue;
  delete vFalse;
  return retval;
}

// Keep track of scan intervals for statistics and sleep until next step
void CCameraController::AddStatsAndSleep(CameraThreadData *td, DWORD &curTime, 
                                       DWORD &lastTime, int &numScan, double &intervalSum,
                                         double &intervalSumSq, int stepInterval)
{
  double interval = SEMTickInterval((double)curTime, (double)lastTime);
  lastTime = curTime;
  numScan++;
  if (numScan > 0) {
    td->ScanIntMin = B3DMIN(td->ScanIntMin, interval);
    td->ScanIntMax = B3DMAX(td->ScanIntMax, interval);
    intervalSum += interval;
    intervalSumSq += interval * interval;
  }

  // Compute sleep interval
  interval = SEMTickInterval((double)timeGetTime(), (double)curTime);
  if (interval < stepInterval) {
    ::Sleep((DWORD)(stepInterval - interval));
  }
}

// Sets the focus for a change of "focus" from base fine/coarse value and maintains
// the last coarse value to minimize setting of coarse
void CCameraController::ChangeDynFocus(CameraThreadData *td, double focusBase,
  double focus, long fineBase, long coarseBase, long &last_coarse)
{
  if (!JEOLscope) {
    td->scopePlugFuncs->SetDefocus(1.e-6 * (focus + focusBase));
  } else {  // JEOL
    int focusSteps = B3DNINT(focus / td->JeolOLtoUm);
    int focus_fine = fineBase + focusSteps;
    int focus_coarse = coarseBase;
    while (focus_fine > 0xFFFF) {
      focus_fine -= 32;
      focus_coarse++;
    }
    while (focus_fine < 0) {
      focus_fine += 32;
      focus_coarse--;
    }
    if (focus_coarse != last_coarse)
      td->scopePlugFuncs->SetCoarseFocus(focus_coarse);
    td->scopePlugFuncs->SetFineFocus(focus_fine);
    last_coarse = focus_coarse;
  }
}

// Start the external focus ramper for an FEI scope
int CCameraController::StartFocusRamp(CameraThreadData * td, bool & rampStarted)
{
  int chan, retval = 0;
  try {
    if (!td->DynFocusInterval)
      td->ScanDelay = td->UnblankTime;

    // If startup delay is negative, it means start the ramper and then wait to 
    // start the shot
    chan = 0;
    if (td->ScanDelay < 0) {
      chan = 10 - td->ScanDelay;
      td->ScanDelay = 10;
    }
    if (FEIscope) {
      if (td->scopePlugFuncs->BeginThreadAccess(1, 0)) {
        DeferMessage(td,"Error getting access to thread for starting focus ramper");
        SEMErrorOccurred(1);
        retval = 1;
      } else {
        if (td->scopePlugFuncs->DoFocusRamp(td->ScanDelay, td->PostActionTime, 
          td->DynFocusInterval, td->rampTable, MAX_RAMP_STEPS, td->IndexPerMs)) {
            DeferMessage(td, CString(td->scopePlugFuncs->GetLastErrorString()));
            SEMErrorOccurred(1);
            retval = 1;
        } else
          rampStarted = true;
      }
    } else {
      td->FocusRamper->DoRamp(td->ScanDelay, td->PostActionTime, 
        td->DynFocusInterval, td->rampTable, td->IndexPerMs);
    }
    if (chan && !retval)
      Sleep(chan);
  }
  catch (_com_error E) {
    retval = 1;
    CCReportCOMError(td, E, _T("Calling FocusRamper->DoRamp() "));
  }
  return retval;
}

// Collect statistics from it and make sure it ended
void CCameraController::FinishFocusRamp(CameraThreadData * td, bool rampStarted)
{
  CString report;
  int chan;
  try {
    td->ScanIntMin = 0;
    if (FEIscope) {
      if (rampStarted && td->scopePlugFuncs->FinishFocusRamp(3000, &td->ScanIntMin, 
        &td->ScanIntMax, &td->ScanIntMean, &td->ScanIntSD))
        DeferMessage(td, CString(td->scopePlugFuncs->GetLastErrorString()));
      td->scopePlugFuncs->EndThreadAccess(1);
    } else {
      td->FocusRamper->FinishRamp(3000, &td->ScanIntMin, &td->ScanIntMax, 
        &td->ScanIntMean, &td->ScanIntSD);
    }
  }
  catch (_com_error E) {
    chan = -B3DNINT(td->ScanIntMin);
    report.Format("Calling FocusRamper->FinishRamp(), error code %d ", chan);
    if (chan == 2) {
      _bstr_t message = "";
      try {
        message = td->FocusRamper->GetLastComErrorString();
        report += CString((char *)message) + " ";
      }
      catch (_com_error E2) {
      }
    }
    CCReportCOMError(td, E, report);
  }
}

// Check whether acquire is still going on
int CCameraController::AcquireBusy()
{
  int retval = UtilThreadBusy(&mAcquireThread);
  return retval;
}

// When the acquisition is done, display the image
void CCameraController::AcquireDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->DisplayNewImage(true);
}

void CCameraController::AcquireError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->AcquireCleanup(error);
}

// Cleanup if there is an error
void CCameraController::AcquireCleanup(int error)
{
  CString report;
  if (error == IDLE_TIMEOUT_ERROR) {
    // If there was a timeout, and acquire thread still running, suspend 
    // and delete it, but first deal with blanker thread
    if (mAcquireThread) {
      WaitForSingleObject(mTD.blankerMutexHandle, BLANKER_MUTEX_WAIT);
      if (mTD.blankerThread) {
        //SEMTrace('1', "Timeout: Killing blanker thread");  
        // Give the acquire thread a chance to not wait on mutex and tell it to give up
        mTD.stopWaitingForBlanker = true;
        ReleaseMutex(mTD.blankerMutexHandle);
        Sleep(50);
        WaitForSingleObject(mTD.blankerMutexHandle, BLANKER_MUTEX_WAIT);
        UtilThreadCleanup(&mTD.blankerThread);
      }
      //SEMTrace('1', "Timeout: Killing acquire thread");  
      UtilThreadCleanup(&mAcquireThread);
      ReleaseMutex(mTD.blankerMutexHandle);
      if (mParam->GatanCam && mParam->useSocket)
        CBaseSocket::CloseBeforeNextUse(GATAN_SOCK_ID);

      // If the thread had to be killed, we don't know if any data are good, so flush them
      for (int chan = 0; chan < mTD.NumChannels; chan++) {
        delete mTD.Array[chan];
        mTD.Array[chan] = NULL;
      }
    }
  }
  mAcquiring = false;

  // If an image was actually gotten, better display it
  // Otherwise, make sure blanking is cleared
  if (mTD.psa || mTD.Array[0]) {
    TurnOffRepeatFlag();
    DisplayNewImage(true);
  } else {
    if (mTD.ReblankTime && !mParam->noShutter)
      mScope->BlankBeam(false);

    // Restart capture up to 2 times on timeout
    if (error == IDLE_TIMEOUT_ERROR) {
      report.Format("Timeout occurred acquiring %s image", (LPCTSTR)CurrentSetName());
      if ((mNumRetries < mRetryLimit || ((mTD.ProcessingPlus & CONTINUOUS_USE_THREAD) && 
        !mNumRetries)) && !mHalting && !mTD.GetDeferredSum) {
          mNumRetries++;
          //StopContinuousSTEM();
          ErrorCleanup(0);
          //if (!(mTD.ProcessingPlus & CONTINUOUS_USE_THREAD) || mNumRetries)
            mWinApp->AppendToLog(report + ", retrying", LOG_SWALLOW_IF_CLOSED);
          Capture(mLastConSet, true);
          return;
      } else {
        mWinApp->mTSController->TSMessageBox(report);
      }
    }
  }
  mHalting = false;
  mDeferredSumFailed = mTD.GetDeferredSum;
  ErrorCleanup(error);
}


///////////////////////////////////////////////////////
//  Screen raising and camera insertion routines
///////////////////////////////////////////////////////

// OnIdle calls here when raise or insertion done: just start capture again
void CCameraController::ScreenOrInsertDone(int inSet)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  SEMTrace('R', "ScreenOrInsertDone calling capture");
  if (winApp->mCamera->GetWaitingForStacking() >= 0)
    winApp->mCamera->Capture(inSet);
}

// If there is an error or timeout return from raising or inserting
void CCameraController::ScreenOrInsertError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mCamera->ScreenOrInsertCleanup(error);
}

// Non-static function to do the cleanup if there is an error
void CCameraController::ScreenOrInsertCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR) {
    // If we were delaying, now go start capture again unless continuous was stopped
    if (mSettling >= 0) {
      if (mStoppedContinuous)
        ErrorCleanup(0);
      else
        Capture(mSettling);
      return;
    }
    
    // If there was a timeout, and insert thread still running, suspend 
    // and delete it
    if (mInsertThread) {
      UtilThreadCleanup(&mInsertThread);
    } else if (mRaisingScreen)
      mScope->ScreenCleanup();
    mWinApp->mTSController->TSMessageBox(_T("Timeout raising screen or inserting camera"));
  }

  // Unblank beam if it was blanked
  if (mInserting && !mITD.insert && mBlankWhenRetracting && !mParam->noShutter)
    mScope->BlankBeam(false);
  mRaisingScreen = 0;
  mInserting = false;
  mHalting = false;
  if (mWaitingForStacking > 0)  
    mWaitingForStacking = -1;
  ErrorCleanup(error);
}

// Check whether insertion is still going on
int CCameraController::InsertBusy()
{
  return UtilThreadBusy(&mInsertThread);
}

// The procedure for actually inserting the camera
UINT CCameraController::InsertProc(LPVOID pParam)
{
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  int retval = 0;
  InsertThreadData *itd = (InsertThreadData *)pParam;
  IDMCamera *pGatan;
  HRESULT hr = S_OK;
  CString message;
  if (itd->FEItype) {
    if (itd->td->scopePlugFuncs->BeginThreadAccess(2, PLUGFEI_MAKE_NOBASIC)) {
      DeferMessage(itd->td, "Error setting up thread access for inserting/retracting");
      retval = 1;
    }

  } else if (!itd->plugFuncs && !itd->DE_camType && itd->DMindex == COM_IND) {
    sCreateFor = CREATE_FOR_ANY;
    CreateDMCamera(pGatan);
    sCreateFor = CREATE_FOR_CURRENT;
  }
  if(FAILED(hr)) {
    message.Format(_T("0x%x, Error getting second instance of Camera "), hr);
    DeferMessage(itd->td, message);
    retval = 1;
  } else if (!retval) {
    try {
      SEMTrace('R', "InsertProc: Setting insertion state of camera %d to %d", 
        itd->camera, itd->insert);
      if (itd->plugFuncs) {

        // For OSIS, selection retracts any camera, so we need to select before inserting
        if (itd->insert && itd->plugFuncs->SelectCamera)
          retval = itd->plugFuncs->SelectCamera(itd->camera);
        if (!retval)
          retval = itd->plugFuncs->SetCameraInsertion(itd->camera, itd->insert ? 1 : 0);
      } else if (itd->DE_camType) {
        if (itd->insert)
          hr = itd->td->DE_Cam->insertCamera();
        else
          hr = itd->td->DE_Cam->retractCamera();
        if (!SUCCEEDED(hr))
          retval = 1;
      } else if (itd->FEItype) {
          retval = itd->td->scopePlugFuncs->ASIsetCameraInsertion(itd->camera, itd->insert ? 1 : 0);
          if (retval) {
            message = itd->td->scopePlugFuncs->GetLastErrorString();
            DeferMessage(itd->td, message);
          }

      } else {
        CallDMIndCamera(itd->DMindex, pGatan, itd->td->amtCam, 
          InsertCamera(itd->camera, itd->insert ? 1 : 0));
      }
      SEMTrace('R', "InsertProc: Returned from insertion call, sleeping %d", 
        itd->delay);
      Sleep(itd->delay);
    }
    catch (_com_error E) {
      CCReportCOMError(itd->td, E, _T("Error inserting or retracting camera "));
      retval = 1;
    }
    if (!itd->plugFuncs && !itd->DE_camType  && !itd->FEItype&& itd->DMindex == COM_IND)
      pGatan->Release();
  }
  if (itd->FEItype)
    itd->td->scopePlugFuncs->EndThreadAccess(2);
  CoUninitialize();
  SEMTrace('R', "InsertProc: exiting thread after sleep");
  return retval;
}


///////////////////////////////////////////////////////
//  Final display and cleanup
///////////////////////////////////////////////////////

// This macro will either assign b to the imBuf element a, or copy the element a from 
// mBufDeferred to imBuf
#define CUR_OR_DEFD_TO_BUF(a, b) imBuf->##a = mTD.GetDeferredSum ? mBufDeferred->##a : (b)

// Finally, display the image when a capture is done
void CCameraController::DisplayNewImage(BOOL acquired)
{
  double stX, stZ, ISX, ISY;
  int spotSize, chan, i, err, ix, iy, invertCon, operation, ixoff, iyoff, divideBy;
  int alignErr, sumCount, camFrames, typext = 0, ldSet = 0;
  BOOL lowDoseMode, hasUserPtSave = false;
  bool nameConfirmed, readLocally = false;
  float axoff, ayoff, specRate, camRate;
  double delay;
  CString message, str, root, ext, localFramePath;
  CString *comFolder = NULL;
  KImage *image;
  EMimageBuffer *imBuf;
  EMimageExtra *extra;
  CMapDrawItem *navItem;
  ShortVec summedList;
  float frameTimeForDose;
  ControlSet *lastConSetp = mTD.GetDeferredSum ? mConsDeferred : &mConSetsp[mLastConSet];
  DWORD ticks;
  float partialExposure = (float)mExposure;
  short int *parray, *imin, *imout;
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  int curCam = mWinApp->GetCurrentCamera();
  float pixelSize = (float)(mBinning * 10000. * 
    mShiftManager->GetPixelSize(curCam, mMagBefore));

  mAcquiring = false;
  mShotIncomplete = false;

  // If this was just a shot to insert detector, delete arrays and take real shot
  if (mNeedShotToInsert >= 0) {
    for (chan = mTD.NumChannels - 1; chan >= 0; chan--) {
      delete [] mTD.Array[chan];
      mTD.Array[chan] = NULL;
    }
    for (chan = 0; chan < mParam->numChannels; chan++)
      if (mParam->availableChan[chan])
        mFEIdetectorInserted[chan] = true;

    ErrorCleanup(0);

    // If halting was set, this will handle that fairly soon
    Capture(mNeedShotToInsert);
    return;
  }
  mInDisplayNewImage = true;

  // Process all the channels of data; do one-time actions on first channel to process
  // and only on first trip into here when aligning falcon frames
  for (chan = mTD.NumChannels - 1; chan >= 0; chan--) {
    if (acquired && chan == mTD.NumChannels - 1 && !mStartedFalconAlign && 
      !mStartedExtraForDEalign) {

      if (mTD.ReblankTime && !mParam->noShutter)
        mScope->BlankBeam(false);

      // Take care of delay times from postactions and record XY backlash
      if (mTD.PostChangeMag)
        mScope->SetLastNormalization(mTD.MagTicks);
      if (mTD.PostImageShift)
        mShiftManager->SetGeneralTimeOut(mTD.ISTicks, mISDelay);
      if (mTD.PostMoveStage) {
        mShiftManager->SetGeneralTimeOut(mTD.MoveInfo.finishedTick, mStageDelay);
        mScope->SetValidXYbacklash(&mTD.MoveInfo);
      }

      if (mDebugMode) {
        ticks = GetTickCount();
        message.Format("%.3f got image, %u elapsed", 0.001 * (ticks % 3600000),
          ticks - mStartTime);
        mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
      }
      //if (mParam->STEMcamera && !mParam->FEItype && !mTD.ContinuousSTEM)
       // mShiftManager->SetGeneralTimeOut(GetTickCount(), -450);
    }
    if (!mTD.Array[chan] || (!(mTD.DMSizeX * mTD.DMSizeY) && mTD.NumAsyncSumFrames != 0))
      continue;
    if (mDiscardImage) {
      delete [] mTD.Array[chan];
      mTD.Array[chan] = NULL;
      continue;      
    }

    // Stack the Falcon frames or at least add them to the file map
    if ((mSavingFalconFrames || mAligningFalconFrames) && !mStartedFalconAlign) {
      if (mDeferStackingFrames) {
        err = mFalconHelper->BuildFileMap(mLocalFalconFramePath,mDirForK2Frames);
        mStackingWasDeferred = true;
        if (err > 0) {
          message.Format("Error %d occurred when adding to list of frames to stack:  "
            "%s\r\n", err, mFalconHelper->GetErrorString(err));
          mWinApp->AppendToLog(message);
        }

      } else {

        // The frame path and name were composed in the setup step
        mPathForFrames.Format("%s%s%s.mrc", (LPCTSTR)mFrameFolder, 
          mFrameFolder.IsEmpty() ? "" : "\\", (LPCTSTR)mFrameFilename);
        localFramePath = mLocalFalconFramePath;

        // Adjust to full path for advanced scripting, but if there is a local path,
        // compose full path with that and set to read locally
        if (FCAM_ADVANCED(mParam)) {
          message = mTD.scopePlugFuncs->ASIgetLastStoragePath();
          SEMTrace('E', "%s   %s  local path: %s", (LPCTSTR)message,
            (LPCTSTR)mPathForFrames, (LPCTSTR)localFramePath);
          message.Replace("\\\\127.0.0.1", "C:");
          if (!localFramePath.IsEmpty()) {
            if (localFramePath.GetAt(localFramePath.GetLength() - 1) != '\\')
              localFramePath += '\\';
            localFramePath += mPathForFrames;
            readLocally = true;
          }
          if (!message.IsEmpty()) {
            if (message.GetAt(message.GetLength() - 1) != '\\')
              message += '\\';
            mPathForFrames = message + mPathForFrames;
          }
          if (localFramePath.IsEmpty())
            localFramePath = mPathForFrames;
        }

        // Call StackFrames to do stacking and/or aligning or writing com file
        ix = -1;
        if (IS_BASIC_FALCON2(mParam) && (mTD.K2ParamFlags & K2_MAKE_ALIGN_COM))
          ix = lastConSetp->faParamSetInd;
        divideBy = mTD.DivideBy2;
        if (IS_BASIC_FALCON2(mParam))
          divideBy = mTD.DivideBy2 ? 2 : 1;
        if (IS_BASIC_FALCON2(mParam) || mAligningFalconFrames)
          mTD.ErrorFromSave = mFalconHelper->StackFrames(localFramePath, mFrameFolder, 
            mFrameFilename, mDivBy2ForImBuf, divideBy, (float)mTD.CountScaling, 
            mParam->rotationFlip, 
            mParam->DMrotationFlip < 0 ? mParam->rotationFlip : mParam->DMrotationFlip,
            pixelSize, mFalconAsyncStacking, readLocally, ix, mTD.NumFramesSaved, &mTD);
        mStackingWasDeferred = false;
        mStartedFalconAlign = mAligningFalconFrames && mFalconAsyncStacking &&
          !mTD.ErrorFromSave;

        // return now, when the stacking/aligning is done it will come back in
        if (mStartedFalconAlign) {
          mWinApp->SetStatusText(SIMPLE_PANE, "ALIGNING " + CurrentSetName().MakeUpper());
          mInDisplayNewImage = false;
          return;
        }
      }
    }

    // Do similar operation for DE align
    // Get the extra data structure now so that name can be stored in it
    if (mDoingDEframeAlign == 1 && !mStartedExtraForDEalign) {
      mExtraDeferred = new EMimageExtra;
      mTD.DE_Cam->SetImageExtraData(mExtraDeferred, mDESetNameTimeoutUsed, false, 
        nameConfirmed);

      // If name was availble, proceed to start the align
      if (nameConfirmed) {
        mStartedExtraForDEalign = !mFalconHelper->AlignFramesFromFile(
          mExtraDeferred->mSubFramePath, mConSetsp[mLastConSet], mParam->rotationFlip, 
          mTD.DivideBy2, (float)mTD.CountScaling, ix, &mTD);
        if (mStartedExtraForDEalign) {
          mWinApp->SetStatusText(SIMPLE_PANE, "ALIGNING " + CurrentSetName().MakeUpper());
          mInDisplayNewImage = false;
          return;
        }
      } else {
        SEMMessageBox("The name of the stack of frames to be aligned was not available in"
          " the expected time");
      }
      delete mExtraDeferred;
      mExtraDeferred = NULL;
    }

    // Remove the frame file if only alignment in FEI was selected in the interface
    if (mRemoveFEIalignedFrames) {
      str = mLocalFalconFramePath;
      if (str.GetAt(str.GetLength() - 1) != '\\')
        str += '\\';
      str += mFrameFolder + (mFrameFolder.IsEmpty() ? "" : "\\") + mFrameFilename;
      SEMTrace('E', "Removing %s and .xml after alignment", (LPCTSTR)(str +".mrc"));
      UtilRemoveFile(str + ".mrc");
      UtilRemoveFile(str + ".xml");
    }
    if ((mAligningFalconFrames || mDoingDEframeAlign == 1) && 
      mFalconHelper->GetAlignSubset()) {
        mNumSubsetAligned = mFalconHelper->GetNumAligned();
        mAlignStart = mFalconHelper->GetAlignStart();
        mAlignEnd = mFalconHelper->GetAlignEnd();
    }

    if (acquired) {
      TestGainFactor((short *)mTD.Array[chan], mTD.DMSizeX, mTD.DMSizeY, mTD.Binning);

      if (mTD.STEMcamera) {

        // For STEM, see if images need to be reoriented or contrast inverted
        operation = mWinApp->mCalibTiming->Calibrating() ? 0 : mParam->rotationFlip;
        invertCon = mWinApp->GetInvertSTEMimages() ? 1 : 0;
        if (mInvertBrightField <= 0 && 
          mParam->channelName[mTD.ChanIndOrig[chan]].Find("Bright") == 0) {
            if (mInvertBrightField < 0)
              invertCon = 1 - invertCon;
            else
              invertCon = 0;
        }
        if (mWinApp->mCalibTiming->Calibrating())
          invertCon = 0;
        if (operation || invertCon) {
          if (RotateAndReplaceArray(chan, operation, invertCon))
            SEMMessageBox("Failed to get memory for processing STEM image", MB_EXCLAME);
        }
        
      } else if (mParam->K2Type &&  mTD.NumAsyncSumFrames == 0) {
      } else if (mParam->K2Type) {

        // Get the adjusted binning of the data
        if (mTD.GatanReadMode != SUPERRES_MODE)
          mTD.Binning *= 2;
        if (mTD.DoseFrac) {
          
          // Need to rotate dose frac image if it is rotated - but check if the rotation
          // is not right
          ix = ((mParam->rotationFlip % 2) ? mDMsizeY : mDMsizeX) * mBinning /mTD.Binning;
          iy = ((mParam->rotationFlip % 2) ? mDMsizeX : mDMsizeY) * mBinning /mTD.Binning;
          if (fabs((double)mTD.DMSizeX - ix) > 20. ||fabs((double)mTD.DMSizeY - iy) > 20.)
          {
            message.Format("The returned image does not have the expected size\r\n"
              "(received %d x %d, expected %d x %d)\r\n\r\n"
              "Check that the RotationAndFlip property setting\r\nmatches the operations"
              " done in DigitalMicrograph", mTD.DMSizeX, mTD.DMSizeY, ix, iy);
            SEMMessageBox( message, MB_EXCLAME);
            mDMsizeX = mTD.DMSizeX * mTD.Binning / mBinning;
            mDMsizeY = mTD.DMSizeY * mTD.Binning / mBinning;
          } else if (mParam->rotationFlip && 
            RotateAndReplaceArray(chan, mParam->rotationFlip,0))
              SEMMessageBox("Failed to get memory for rotating K2 image", MB_EXCLAME);

          // need to adjust sizes again without the full frame constraint to get size the
          // same as it would be without dose frac on
          AdjustSizes(mDMsizeX, mParam->sizeX, mParam->moduloX, mLeft, mRight, 
            mDMsizeY, mParam->sizeY, mParam->moduloY, mTop, mBottom, mBinning);
        }

        // Compare adjusted binning to the desired binning
        if (mTD.Binning < mBinning) {

          // Antialias reduction is needed if binning does not yet match
          double filtScale = (double)mTD.Binning / mBinning;
          unsigned char **linePtrs = makeLinePointers(mTD.Array[chan], mTD.DMSizeX,
            mTD.DMSizeY, 2);

          // Need to make the image the same size as a regular full-sized acquire
          axoff = (float)(B3DMAX(0, mTD.DMSizeX - mDMsizeX * mBinning / mTD.Binning) /2.);
          ayoff = (float)(B3DMAX(0, mTD.DMSizeY - mDMsizeY * mBinning / mTD.Binning) /2.);
          NewArray(parray, short int, mDMsizeX * mDMsizeY);
          B3DCLAMP(mZoomFilterType, 0, 5);
          err = selectZoomFilter(mZoomFilterType, filtScale, &i);
          if (!err && parray && linePtrs) {
            setZoomValueScaling((float)(1. / (filtScale * filtScale)));
            err = zoomWithFilter(linePtrs, mTD.DMSizeX, mTD.DMSizeY, axoff, ayoff, 
              mDMsizeX, mDMsizeY, mDMsizeX, 0, 
              mTD.ImageType == kUSHORT ? SLICE_MODE_USHORT : SLICE_MODE_SHORT,
              parray, NULL, NULL);
            if (!err) {
              delete [] mTD.Array[chan];
              mTD.Array[chan] = parray;
              mTD.DMSizeX = mDMsizeX;
              mTD.DMSizeY = mDMsizeY;
            } else {
              delete parray;
              message.Format("Error %d trying to reduce image with zoomWithFilter", err);
              SEMMessageBox(message);
            }
          } else {
            if (err)
              message.Format("Error %d setting up image reduction with selectZoomFilter",
              err);
            else
              message = "Failed to allocate memory for image reduction";
            SEMMessageBox(message);
            delete parray;
            err = 1;
          }
          B3DFREE(linePtrs);
          if (err && mRepFlag >= 0) {
            TurnOffRepeatFlag();
            StopContinuousSTEM();
          }
        } else if (mTD.DoseFrac && (mTD.DMSizeX > mDMsizeX || mTD.DMSizeY > mDMsizeY)) {

          // Or simple trimming is needed for unbinned dose frac data
          ixoff = (mTD.DMSizeX - mDMsizeX) / 2;
          iyoff = (mTD.DMSizeY - mDMsizeY) / 2;
          for (iy = 0; iy < mDMsizeY; iy++) {
            imout = mTD.Array[chan] + iy * mDMsizeX;
            imin = mTD.Array[chan] + (iy + iyoff) * mTD.DMSizeX + ixoff;
            for (ix = 0; ix < mDMsizeX; ix++)
              *imout++ = *imin++;
          }
          mTD.DMSizeX = mDMsizeX;
          mTD.DMSizeY = mDMsizeY;
        }

      } else if (!mSimulationMode && mTD.Processing != lastConSetp->processing){

        // If processing needed, first dark subtract or gain correct, then correct defects
        if (mTD.DMSizeX == mDarkp->SizeX && mTD.DMSizeY == mDarkp->SizeY) {
          short *array2 = NULL;
          double exp2 = 0.;
          double wallstart = wallTime();
          if (mDarkAbove) {
            mDarkp = mDarkBelow;
            array2 = (short int *)mDarkAbove->Array;
            exp2 = mDarkAbove->Exposure;
          }
          if (lastConSetp->processing == DARK_SUBTRACTED) {
            ProcDarkSubtract(mTD.Array[chan], mTD.ImageType, mTD.DMSizeX, mTD.DMSizeY, 
              (short *)mDarkp->Array, mDarkp->Exposure, array2, exp2, mExposure);
          } else {
            ProcGainNormalize(mTD.Array[chan], mTD.ImageType, mGainp->SizeX, mTop, mLeft,
              mBottom, mRight, (short *)mDarkp->Array, mDarkp->Exposure, array2, exp2, 
              mExposure, mGainp->Array, mGainp->ByteSize, mGainp->GainRefBits);
          }

          // Keep this processing here in case there is defect correction right at the
          // boundary
          if ((mTD.ProcessingPlus & CONTINUOUS_USE_THREAD) && mParam->balanceHalves) {
            ix = mParam->ifHorizontalBoundary ? mParam->sizeY : mParam->sizeX;
            ixoff = (mParam->halfBoundary - ix / 2) / mBinning + (ix / 2) / mBinning;
            ProcBalanceHalves(mTD.Array[chan], mTD.ImageType, mTD.DMSizeX, mTD.DMSizeY,
              mTop, mLeft, ixoff, mParam->ifHorizontalBoundary);
          }
          CorDefCorrectDefects(&mParam->defects, mTD.Array[chan], mTD.ImageType, 
            mBinning, mTop, mLeft, mBottom, mRight);
          if (lastConSetp->removeXrays)
            mWinApp->mProcessImage->RemoveXRays(mParam, mTD.Array[chan], mTD.ImageType, 
            mBinning, mTop, mLeft, mBottom, mRight, mParam->imageXRayAbsCrit, 
            mParam->imageXRayNumSDCrit, mParam->imageXRayBothCrit, mDebugMode);

        } else 
          SEMMessageBox("The image is not the expected size so\nit has not been"
          "processed", MB_EXCLAME);
      } else if ((mTD.ProcessingPlus & CONTINUOUS_USE_THREAD) && mParam->balanceHalves) {

        // But also balance halves if needed and it normalized in DM
        ix = mParam->ifHorizontalBoundary ? mParam->sizeY : mParam->sizeX;
        ixoff = (mParam->halfBoundary - ix / 2) / mBinning + (ix / 2) / mBinning;
        ProcBalanceHalves(mTD.Array[chan], mTD.ImageType, mTD.DMSizeX, mTD.DMSizeY,
          mTop, mLeft, ixoff, mParam->ifHorizontalBoundary);
      } else if (mTD.Processing == GAIN_NORMALIZED && mParam->DE_camType >= 2) {

        // And correct defects for DE camera with normalized images
        CorDefCorrectDefects(&mParam->defects, mTD.Array[chan], mTD.ImageType, 
          mBinning, mTop, mLeft, mBottom, mRight);
      }
      if (mParam->OneViewType && mTD.Corrections > 0 && 
        (mTD.Corrections & OVW_DRIFT_CORR_FLAG) != 0) {
          int analyzeLen = B3DMAX(1024, B3DMAX(mTD.DMSizeY, mTD.DMSizeX) / 4);
          int width = mBinning > 1 ? 30 : 60;
          float critMADN = 9.f;
          int relTop, relBottom, relLeft, relRight;
          if (mLeft * mBinning < width || mRight * mBinning > mParam->sizeX - width ||
            mTop * mBinning < width || mBottom * mBinning > mParam->sizeY - width) {
              if (CorDefFindDriftCorrEdges(mTD.Array[chan], mTD.ImageType, mTD.DMSizeX, 
                mTD.DMSizeY, analyzeLen, width, critMADN, relLeft, relRight, relTop, 
                relBottom)) {
                  SEMMessageBox("Error getting memory for finding dark edges of drift"
                    " corrected image");
              } else {

                // Modify the usable area limits for any edge that is not at the limit
                CameraDefects defCopy = mParam->defects;
                if (relLeft > 0)
                  defCopy.usableLeft = B3DMAX(defCopy.usableLeft, 
                  mBinning * (mLeft + relLeft));
                if (relRight < mTD.DMSizeX - 1) {
                  defCopy.usableRight = ((mRight - 1) - (mTD.DMSizeX - 1 - relRight)) *
                    mBinning;
                  if (mParam->defects.usableRight > 0)
                    defCopy.usableRight = B3DMIN(defCopy.usableRight, 
                    mParam->defects.usableRight);
                }
                if (relTop > 0)
                  defCopy.usableTop = B3DMAX(defCopy.usableTop, 
                  mBinning * (mTop + relTop));
                if (relBottom < mTD.DMSizeY - 1) {
                  defCopy.usableBottom = ((mBottom - 1) - (mTD.DMSizeY - 1 - relBottom)) *
                    mBinning;
                  if (mParam->defects.usableBottom > 0)
                    defCopy.usableBottom = B3DMIN(defCopy.usableBottom, 
                    mParam->defects.usableBottom);
                }
                /*SEMTrace('1', "CDFDCE: size %d %d; returned %d %d %d %d; usable area"
                " %d %d %d %d", mTD.DMSizeX, mTD.DMSizeY, relLeft, relRight, relTop, 
                  relBottom, defCopy.usableLeft, defCopy.usableRight, defCopy.usableTop,
                  defCopy.usableBottom);*/
                CorDefCorrectDefects(&defCopy, mTD.Array[chan], mTD.ImageType, 
                  mBinning, mTop, mLeft, mBottom, mRight);
              }
          }
      }
      if (mDebugMode) {
        ticks = GetTickCount();
        message.Format("%.3f processed, %u elapsed", 0.001 * (ticks % 3600000),
          ticks - mStartTime);
        mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
      }
    }

    if (mTD.ImageType == kFLOAT)
      image = new KImageFloat();
    else
      image = new KImageShort();
    if (mTD.ImageType == kUSHORT)
      image->setType(kUSHORT);
    if (mTD.psa && acquired)
      image->useData(mTD.psa);
    else
      image->useData((char *)mTD.Array[chan], mTD.DMSizeX, mTD.DMSizeY);

    // Adjust the IS for a view offset in low dose by the net offset
    lowDoseMode = mTD.GetDeferredSum ? mBufDeferred->mLowDoseArea :mWinApp->LowDoseMode();
    if (lowDoseMode) {
      ldSet = ConSetToLDArea(mLastConSet);
      if (ldSet == VIEW_CONSET || ldSet == SEARCH_AREA) {
        mWinApp->mLowDoseDlg.GetNetViewShift(ISX, ISY, ldSet);
        mStartingISX -= ISX;
        mStartingISY -= ISY;
      }
    }

    mLastImageTime = 0.001 * GetTickCount();
    float defocus = (float)
      ((mTD.ScanTime || mTD.DynFocusInterval) ? mCenterFocus : mScope->FastDefocus());

    // Get a partial exposure time that applies to this image for early return or aligned
    // subset
    if (mTD.NumAsyncSumFrames > 0 || mNumSubsetAligned > 0) {
      i = B3DNINT(lastConSetp->exposure / lastConSetp->frameTime);
      if (mTD.NumAsyncSumFrames > 0)
        partialExposure = (float)(mExposure * B3DMIN(mTD.NumAsyncSumFrames, i) / 
          (float)B3DMAX(1, i));
      else if (mNumSubsetAligned > 0 && mParam->K2Type && !lastConSetp->sumK2Frames)
        partialExposure = (float)(mExposure * B3DMIN(mNumSubsetAligned, i) / 
          (float)B3DMAX(1, i));
      else if (mNumSubsetAligned > 0 && mParam->DE_camType)
        partialExposure = (float)((mExposure * mNumSubsetAligned) / 
          mFalconHelper->GetNumFiles());
      else 
        partialExposure = mFalconHelper->AlignedSubsetExposure(
          lastConSetp->summedFrameList, mParam->K2Type ? 
          lastConSetp->frameTime : mFalconReadoutInterval, mAlignStart, mAlignEnd);
    }

    // Modify mImBufs only if there is actually an image, but save info when starting
    // deferred sum
    if (mTD.NumAsyncSumFrames != 0 || mStartingDeferredSum) {
      imBuf = mImBufs;
      if (!mTD.NumAsyncSumFrames) {
        if (!mBufDeferred)
          mBufDeferred = new EMimageBuffer;
        imBuf = mBufDeferred;
      } else {

        // Transfer the shift from an existing continuous mode image to the new one if 
        // this is this frame comes in while mouse shifting
        if (mRepFlag >= 0 && mShiftManager->GetMouseShifting() && mImBufs->mImage) {
          mImBufs->mImage->getShifts(axoff, ayoff);
          image->setShifts(axoff, ayoff);
        }

        // If a task is waiting for next continuous frame, make sure it is unique frame 
        // and an exposure time has elapsed, plus any general timeout that was set and not
        // turned into a delay before capture, and clear the task flag
        if (mRepFlag >= 0 && mTaskFrameWaitStart >= 0. && 
          !mImBufs->IsImageDuplicate(image)) {
            ticks = mShiftManager->GetGeneralTimeOut(mObeyTiltDelay ? RECORD_CONSET : 
              mLastConSet);
            delay = SEMTickInterval((double)ticks, (double)GetTickCount());
            if (SEMTickInterval(mTaskFrameWaitStart) >= 1000. * mExposure + 
              mContinuousDelayFrac * B3DMAX(delay, 0.)) {
                if (delay > 0)
                  mShiftManager->ResetAllTimeouts();
                mTaskFrameWaitStart = -1.;
            }
        }
        mImBufs->DeleteImage();
        mImBufs->DeleteOffsets();
        mImBufs->mImage = image;
        mImBufs->mCaptured = 1;
        mWinApp->mBufferManager->FindScaling(mImBufs);
      }

      imBuf->mBinning = mBinning;
      imBuf->mEffectiveBin = (float)mBinning;
      CUR_OR_DEFD_TO_BUF(mDivideBinToShow, BinDivisorI(mParam));
      CUR_OR_DEFD_TO_BUF(mDividedBy2, mDivBy2ForImBuf > 0);
      CUR_OR_DEFD_TO_BUF(mCamera, curCam);
      if (mMagToRestore)
        CUR_OR_DEFD_TO_BUF(mEffectiveBin, imBuf->mEffectiveBin * 
          mShiftManager->GetPixelSize(curCam, mMagBefore) /
          mShiftManager->GetPixelSize(curCam, mMagToRestore));
      imBuf->mConSetUsed = mLastConSet;
      CUR_OR_DEFD_TO_BUF(mDynamicFocused, mTD.STEMcamera && mTD.DynFocusInterval > 0);
      CUR_OR_DEFD_TO_BUF(mMagInd, mMagBefore);
      CUR_OR_DEFD_TO_BUF(mISX, mStartingISX);
      CUR_OR_DEFD_TO_BUF(mISY, mStartingISY);
      CUR_OR_DEFD_TO_BUF(mExposure, (float)mExposure);
      CUR_OR_DEFD_TO_BUF(mK2ReadMode, mTD.GatanReadMode);
      mScope->GetValidXYbacklash(mStageXbefore, mStageYbefore, imBuf->mBacklashX, 
          imBuf->mBacklashY);
      if (mTD.GetDeferredSum) {
        imBuf->mBacklashX = mBufDeferred->mBacklashX;
        imBuf->mBacklashY = mBufDeferred->mBacklashY;
      }
      if (mTD.NumAsyncSumFrames != 0)
        CorDefSampleMeanSD(image->getRowData(), image->getType(), image->getWidth(),
          image->getHeight(), &imBuf->mSampleMean, &imBuf->mSampleSD);
      else
        imBuf->mSampleMean = EXTRA_NO_VALUE;
      CUR_OR_DEFD_TO_BUF(mTimeStamp, mLastImageTime);
      imBuf->mStageShiftedByMouse = false;
      hasUserPtSave = imBuf->mHasUserPt;
      imBuf->mHasUserPt = false;
      imBuf->mHasUserLine = false;
      imBuf->mCtfFocus1 = 0.;
      imBuf->mMapID = 0;
      imBuf->mChangeWhenSaved = 0;
      imBuf->mStage2ImMat.xpx = 0.;
      imBuf->mRegistration = 0;
      imBuf->mZoom = 0.;
      CUR_OR_DEFD_TO_BUF(mDefocus, defocus);
      imBuf->mLowDoseArea = lowDoseMode;
      imBuf->mViewDefocus = 0.;
      imBuf->mCurStoreChecksum = 0;
      CUR_OR_DEFD_TO_BUF(mProbeMode, mScope->GetProbeMode());
      if (mWinApp->mNavigator)
        CUR_OR_DEFD_TO_BUF(mRegistration, mWinApp->mNavigator->GetCurrentRegistration());

      // If its a single frame, set up a save-copy flag and set to 1
      if (mSingleContModeUsed == SINGLE_FRAME && mTD.NumAsyncSumFrames != 0)
        mImBufs->IncrementSaveCopy();

      // Copy the imbuf and NULL out pointers if this was a real early return sum
      if (mTD.NumAsyncSumFrames != 0 && mStartingDeferredSum) {
        mBufDeferred = new EMimageBuffer;
        *mBufDeferred = *mImBufs;
        mBufDeferred->ClearForDeletion();
      }
    }

    // Get the extra header data - get JEOL data from last update to save time
    // Adopt the stored extra if getting deferred sum or have extra from starting DE align
    if (mTD.GetDeferredSum || mStartedExtraForDEalign) {
      extra = mExtraDeferred;
      image->SetUserData(extra);
      mExtraDeferred = NULL;
    }
    
    if (!mTD.GetDeferredSum) {
      if (!mStartedExtraForDEalign) {
        extra = new EMimageExtra;
        image->SetUserData(extra);
      }
      extra->m_fTilt = mTiltBefore;
      extra->mDefocus = defocus;
      if (!mParam->STEMcamera)
        extra->mTargetDefocus = mWinApp->mFocusManager->GetTargetDefocus();
      stZ = mScope->FastIntensity();
      extra->m_fIntensity = (float)stZ;
      extra->mStageX = (float)mStageXbefore;
      extra->mStageY = (float)mStageYbefore;
      extra->mStageZ = (float)mStageZbefore;
      extra->mDividedBy2 = mDivBy2ForExtra;
      extra->mDateTime = mWinApp->mDocWnd->DateTimeForTitle();
      if (mParam->STEMcamera)
        extra->mChannelName = mParam->channelName[mTD.ChannelIndex[chan]];
      if (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring() && 
        !mWinApp->DoingTiltSeries()) {
          ix = mWinApp->mNavigator->GetCurrentOrAcquireItem(navItem);
          extra->mNavLabel = navItem->mLabel;
      }
      extra->m_iMag = MagOrEFTEMmag(mWinApp->GetEFTEMMode(), mMagBefore, 
        mParam->STEMcamera);
      extra->mMagIndex = mMagBefore;
      typext = TILT_MASK | VOLT_XY_MASK | INTENSITY_MASK | MAG100_MASK | DOSE_MASK;
      if (mWinApp->mMontageController->DoingMontage() && 
        !mWinApp->mMontageController->GetFocusing()) {
          int X, Y, Z;
          mWinApp->mMontageController->GetCurrentCoords(X, Y, Z);
          extra->m_iMontageX = X;
          extra->m_iMontageY = Y;
          extra->m_iMontageZ = Z;
          if (mWinApp->mNavigator)
            mWinApp->mNavigator->GetSuperCoords(extra->mSuperMontX, extra->mSuperMontY);
          if (extra->mSuperMontX >= 0) {
            extra->mSuperMontX += X;
            extra->mSuperMontY += Y;
          }
          typext |= MONTAGE_MASK;
      }

      // Store the exposure dose, adjust for difference between requested and actual time
      if (lowDoseMode) {
        ldSet = ConSetToLDArea(mLastConSet);
        spotSize = ldParam[ldSet].spotSize;
        if ((ldSet == VIEW_CONSET || ldSet == SEARCH_AREA) && 
          mLastConSet == MONTAGE_CONSET)
          imBuf->mConSetUsed = ldSet == VIEW_CONSET ? VIEW_CONSET : SEARCH_CONSET;
        if (ldSet == VIEW_CONSET || ldSet == SEARCH_AREA)
          mImBufs->mViewDefocus = mScope->GetLDViewDefocus(ldSet);
      } else {
        spotSize = mScope->FastSpotSize();
      }
      extra->m_fDose = (float)mWinApp->mBeamAssessor->GetElectronDose(spotSize, stZ, 
        SpecimenBeamExposure(curCam, lastConSetp, true));
      extra->m_fDose *= (float)mExposure / lastConSetp->exposure;
      extra->mPriorRecordDose = mPriorRecordDose;

      // Store other no-cost stuff and then convert header items to shorts
      extra->mSpotSize = spotSize;
      extra->mISX = (float)mStartingISX;
      extra->mISY = (float)mStartingISY;
      extra->mExposure = (float)mExposure;
      extra->mBinning = (float)mBinning / BinDivisorF(mParam);
      extra->mCamera = curCam;
      extra->mPixel = pixelSize;
      if (mTD.NumAsyncSumFrames != 0 && imBuf->mSampleMean > EXTRA_VALUE_TEST && 
        IsDirectDetector(mParam) && extra->m_fDose > 0 &&
        !mWinApp->mProcessImage->DoseRateFromMean(imBuf, imBuf->mSampleMean, camRate)) {
          specRate = extra->m_fDose * 
            (float)pow((double)extra->mPixel / extra->mBinning, 2) / partialExposure;
          mParam->specToCamDoseFac = camRate / specRate;
      }
      if (mParam->STEMcamera)
        extra->mAxisAngle = mParam->imageRotation + mWinApp->GetAddedSTEMrotation();
      else
        extra->mAxisAngle = (float)mShiftManager->GetImageRotation(curCam, 
        mMagBefore);
      extra->ValuesIntoShorts();
    }

    // Now start using current values to update extra for a deferred sum

    // Report the fate of frame-saving for K2 and Falcon
    if (IsConSetSaving(lastConSetp, mLastConSet, mParam, true) || 
      (mSavingFalconFrames && !mDeferStackingFrames)) {
        if (mTD.NumAsyncSumFrames >= 0) {
          mTD.NumFramesSaved = B3DNINT(mExposure / mTD.FrameTime);
          mTD.ErrorFromSave = 0;
          if (lastConSetp->sumK2Frames && lastConSetp->summedFrameList.size() > 0 &&
            CAN_PLUGIN_DO(CAN_SUM_FRAMES, mParam))
            mTD.NumFramesSaved = mFalconHelper->GetFrameTotals(
              lastConSetp->summedFrameList, i, mTD.NumFramesSaved);
        }

        if (mTD.ErrorFromSave < 0) {
          message = "An error occurred trying to find out how many frames were saved";
        } else {
          message = "";
          if (mTD.ErrorFromSave > 0)
            message.Format("Error %d occurred when saving frames:  %s\r\n", 
            mTD.ErrorFromSave, mParam->K2Type ? SEMCCDErrorMessage(mTD.ErrorFromSave) : 
            mFalconHelper->GetErrorString(mTD.ErrorFromSave));
          if (mTD.NumFramesSaved)
            str.Format(" %d frames %s saved to %s", mTD.NumFramesSaved, 
            (mTD.NumAsyncSumFrames < 0 && (!(mSavingFalconFrames && mFalconAsyncStacking)
            || mAligningFalconFrames)) ? "were" : "are being", (LPCTSTR)mPathForFrames);
          else
            str = "No frames were saved";
          message += str;

          // Save in extra
          if (mTD.NumFramesSaved) {
            extra->mNumSubFrames = mTD.NumFramesSaved;
            extra->mSubFramePath = mPathForFrames;

            // Write com file for Falcon aligning if flag is set in advanced case:
            // Here local frame path is required
            if (mParam->FEItype && FCAM_ADVANCED(mParam) && 
              (mTD.K2ParamFlags & K2_MAKE_ALIGN_COM)) {
                MakeOneFrameAlignCom(localFramePath, lastConSetp);
            }
          }
        }
        mWinApp->AppendToLog(message);
    }

    // Get special data for DE camera and report frame-saving there
    if (mParam->DE_camType >= 2 && !(mTD.ProcessingPlus & CONTINUOUS_USE_THREAD)) {
      if (!mStartedExtraForDEalign)
        mTD.DE_Cam->SetImageExtraData(extra, mDESetNameTimeoutUsed, 
          mDoingDEframeAlign < 2, nameConfirmed);
      else
        nameConfirmed = true;
      alignErr = 0;
      if (IsConSetSaving(lastConSetp, mLastConSet, mParam, false)) {

          // Get frame time, number of camera frames and # in summed frames
          // Counting mode has no readout on  frames saved, so we have to fill that in
          frameTimeForDose = (float)(1.f / mTD.FramesPerSec);
          camFrames = B3DNINT(mExposure / frameTimeForDose);
          sumCount = B3DMAX(1, lastConSetp->DEsumCount);

          // Just compute number of frames for counting mode or if saving is in progress
          if (lastConSetp->K2ReadMode && (mParam->CamFlags & DE_CAM_CAN_COUNT)) {
            sumCount = B3DMAX(1, lastConSetp->sumK2Frames);
            extra->mNumSubFrames = camFrames / sumCount;
          }
          if (!nameConfirmed || !extra->mNumSubFrames) {
            extra->mNumSubFrames = camFrames / sumCount;
            nameConfirmed = false;
          }
          if (extra->mNumSubFrames > 0) {
            message.Format(" %d %sframes %s saved to %s", extra->mNumSubFrames, 
              sumCount > 1 ? "" : "summed ", nameConfirmed ? "were" : "are being", 
              (LPCTSTR)extra->mSubFramePath);
          } else  {
            message.Format("Saving of %d %sframes was requested but the server says no "
              "frames were saved", camFrames / sumCount, sumCount > 1 ? "" : "summed ");
          }

          // Set up summed frame list for the dose estimate below
          // There shouldn't be any remainder any more, but leave it here
          summedList.push_back((short)camFrames / sumCount);
          summedList.push_back(sumCount);
          if (camFrames % sumCount) {
            summedList.push_back(1);
            summedList.push_back((short)(camFrames % sumCount));
          }
          if (!extra->mNumSubFrames && mScope->GetSimulationMode())
            extra->mNumSubFrames = (camFrames + sumCount - 1) / sumCount;
          mWinApp->AppendToLog(message);
      }

      // Write com file for IMOD alignment
      if (mDoingDEframeAlign == 2) {
        mFalconHelper->SetRotFlipForComFile(mParam->rotationFlip);
        MakeOneFrameAlignCom(extra->mSubFramePath, lastConSetp);
      }
    }

    // Report on frame aligning if not deferred
    if ((mTD.UseFrameAlign && mTD.NumAsyncSumFrames < 0) || 
      (mDoingDEframeAlign == 1 && mStartedExtraForDEalign)) {
      if (!mParam->K2Type && ((alignErr = mFalconHelper->GetAlignError()) != 0 || 
        !(ix = mFalconHelper->GetNumAligned()))) {
          if (!ix && !alignErr && !mTD.ErrorFromSave) {
            message = "An unknown error occurred when aligning frames";
          } else {
            if (!ix && !alignErr)
              alignErr = mTD.ErrorFromSave;
            message.Format("Error %d occurred when aligning frames:  %s\r\n", 
              alignErr, mFalconHelper->GetErrorString(alignErr));
          }
          mWinApp->AppendToLog(message);
      } else if (mNumFrameAliLogLines > 0) {
 
        if (mNumFrameAliLogLines > 1) {
          PrintfToLog("Frame alignment results:          distance raw = %.1f smoothed"
            " = %.1f", mTD.FaRawDist, mTD.FaSmoothDist);
          if (mTypeOfAlignError == 0)
            PrintfToLog(" Residual mean = %.2f  max max = %.2f", mTD.FaResMean, 
            mTD.FaMaxResMax);
          if (mTypeOfAlignError > 0)
            PrintfToLog(" Weighted resid mean = %.2f  max max = %.2f  Mean unweighted max"
            " = %.2f", mTD.FaResMean, mTD.FaMaxResMax, mTD.FaMaxRawMax);
        } else {
          PrintfToLog("Frame alignment: %sesid mean = %.2f  max max = %.2f  "
            "raw dist = %.1f", mTypeOfAlignError > 0 ? "Weighted r" : "R", mTD.FaResMean,
            mTD.FaMaxResMax, mTD.FaRawDist);
        }
        if ((mParam->K2Type && mGettingFRC) || 
          (!mParam->K2Type && mFalconHelper->GetGettingFRC()))
            PrintfToLog(" FRC crossings 0.5: %.4f  0.25: %.4f  0.125: %.4f  is %.4f at "
              "0.25/pix", mTD.FaCrossHalf / K2FA_FRC_INT_SCALE, 
              mTD.FaCrossQuarter / K2FA_FRC_INT_SCALE, 
              mTD.FaCrossEighth / K2FA_FRC_INT_SCALE, mTD.FaHalfNyq / K2FA_FRC_INT_SCALE);
      }
    }
    
    // Remove DE frames if flag set and no align error
    if (mRemoveAlignedDEframes && !alignErr) {
      UtilRemoveFile(extra->mSubFramePath);
      UtilRemoveFile(mTD.DE_Cam->GetPathAndDatasetName() + "_info.txt");
      if (lastConSetp->K2ReadMode > 0)
        UtilRemoveFile(mTD.DE_Cam->GetPathAndDatasetName() + ".log");
    }

    // Get dose per frame and output list of doses and # of frames
    if (extra->mNumSubFrames > 0) {
      if (IS_FALCON2_OR_3(mParam)) {
        summedList = lastConSetp->summedFrameList;
        frameTimeForDose = mFalconReadoutInterval;
      } else if (mParam->K2Type) {

        // Set up dummy summed frame list or use the one that was used
        frameTimeForDose = lastConSetp->frameTime;
        summedList.push_back((short)mTD.NumFramesSaved);
        summedList.push_back(1);
        if (lastConSetp->sumK2Frames && lastConSetp->summedFrameList.size() > 0 &&
          CAN_PLUGIN_DO(CAN_SUM_FRAMES, mParam))
          summedList = lastConSetp->summedFrameList;
      }
      extra->mFrameDosesCounts = "";
      for (i = 0; i < (int)summedList.size() / 2; i++) {
        str.Format("%.5g %d", summedList[i * 2 + 1] * frameTimeForDose * extra->m_fDose /
          mExposure, summedList[i * 2]);
        if (!extra->mFrameDosesCounts.IsEmpty())
          extra->mFrameDosesCounts += "  ";
        extra->mFrameDosesCounts += str;
      }
    }

    // Get counts per electron from imBuf if not early return, or else compute laboriously
    if (mTD.NumAsyncSumFrames != 0) {
      extra->mCountsPerElectron= mWinApp->mProcessImage->CountsPerElectronForImBuf(imBuf);
    } else if (mParam->countsPerElectron && mParam->K2Type) {
      extra->mCountsPerElectron = mParam->countsPerElectron;
      if (lastConSetp->K2ReadMode > 0)
        extra->mCountsPerElectron = GetCountScaling(mParam);
      if (mTD.DivideBy2)
        extra->mCountsPerElectron /= 2.;
    }

    // Save to autodoc file if one is designated
    if (extra->mNumSubFrames > 0 || (IS_FALCON2_OR_3(mParam) && (mFrameMdocForFalcon > 1 
      || (mFrameMdocForFalcon && mLastConSet == RECORD_CONSET)))) {
        if (mTD.GetDeferredSum)
          i = mWinApp->mDocWnd->UpdateLastMdocFrame(image);
        else  
          i = mWinApp->mDocWnd->SaveFrameDataInMdoc(image);
        if (i) {
          message.Format("Error trying to save frame metadata to frame .mdoc file:\n"
            "Error %s", i == 1 ? "setting current autodoc index" : 
            (i == 2 ? "storing data in autodoc structure" : "writing the .mdoc file"));
          SEMMessageBox(message);
        }
    }

    // If making a deferred sum, now is the time to copy the extra
    if (mStartingDeferredSum) {
      mExtraDeferred = new EMimageExtra;
      *mExtraDeferred = *extra;
    }
  
    // If it is partial sum, adjust exposure time in extra and imbuf now that mdoc saved
    if (mTD.NumAsyncSumFrames > 0 || mNumSubsetAligned > 0) {
      mImBufs->mExposure = extra->mExposure = partialExposure;
    }

    // Now if this was deferred sum, we are done with imbuf and conSet
    if (mTD.GetDeferredSum) {
      delete mBufDeferred;
      mBufDeferred = NULL;
      delete mConsDeferred;
      mConsDeferred = NULL;
    }

    // If there are more channels, roll the buffers
    if (chan)
      RollBuffers(mBufferManager->GetShiftsOnAcquire(), 0);

    // Delete image if it is a no-return shot
    if (mTD.NumAsyncSumFrames == 0)
      delete image;
  }

  // Add dose for dose meter unless LD Trial/Focus areas
  if (mWinApp->mScopeStatus.WatchingDose()) {
    if (ldSet != 1 && ldSet != 2) {
      stX = mWinApp->mBeamAssessor->GetElectronDose(spotSize, stZ, 
        SpecimenBeamExposure(curCam, lastConSetp));
      stX *= mExposure / lastConSetp->exposure;
      mWinApp->mScopeStatus.AddToDose(stX);
    }
  }

  if (mTD.NumAsyncSumFrames != 0 && !mDiscardImage) {

    // Defer updates by any actions below - the update will be done on ErrorCleanup
    mWinApp->SetDeferBufWinUpdates(true);
    mWinApp->mLowDoseDlg.AddDefineAreaPoint();
    mImBufs->SetImageChanged(1);

    // For live FFT, or a regular FFT if side-by-side is open and this is enabled, 
    // do an FFT instead of setting current buffer
    // But set the buffer if not 0 so the FFT is of the right buffer
    if ((mSingleContModeUsed == CONTINUOUS && mWinApp->mProcessImage->GetLiveFFT()) ||
      (mSingleContModeUsed != CONTINUOUS && mWinApp->mProcessImage->GetSideBySideFFT() &&
      mWinApp->mProcessImage->GetAutoSingleFFT() && mWinApp->mFFTView)) {
      if (mWinApp->GetImBufIndex())
        mWinApp->SetCurrentBuffer(0);
      if (!mWinApp->mProcessImage->GetSideBySideFFT())
        mImBufs->mHasUserPt = hasUserPtSave;
      mWinApp->mProcessImage->GetFFT(1, 
        mSingleContModeUsed == CONTINUOUS ? BUFFER_LIVE_FFT : BUFFER_FFT);
      if (mWinApp->mProcessImage->GetSideBySideFFT())
        mWinApp->SetCurrentBuffer(0);
    } else
      mWinApp->SetCurrentBuffer(0);
    mWinApp->SetDeferBufWinUpdates(false);
  }
  ErrorCleanup(0);
  mInDisplayNewImage = false;
  if (mRepFlag >= 0) {
    axoff = (float)(1000. * mContinuousCount / SEMTickInterval(mContinStartTime));
    mContinuousCount++;
    SEMTrace('t', "%u elapsed,  %.2f FPS", GetTickCount() - mStartTime, axoff);
  }
  
  // Update filter settings if there is a timer that may have been waiting
  if (mFilterUpdateID && !mTD.ContinuousSTEM && 
    !(mTD.ProcessingPlus & CONTINUOUS_USE_THREAD))
    CheckFilterSettings();
  
  // If halting, stop now and clear pending
  if (mHalting) {
    StopContinuousSTEM();
    mHalting = false;
    mPending = -1;
    return;
  }
  if (mRepFlag < 0)
    StopContinuousSTEM();
  
  // Check for whether to start another capture
  if (InitiateIfPending())
    return;

  // Check if doing repeated captures
  if (mRepFlag >= 0)
    Capture(mRepFlag);
}


// Common place to clean up when there is an error, or when done
void CCameraController::ErrorCleanup(int error)
{
  int ind; 

  // Make sure nothing is queued any more
  mStageQueued = false;
  mMagQueued = false;
  mISQueued = false;
  mFocusStepToDo1 = 0.;
  mFocusStepToDo2 = 0.;
  mSettling = -1;
  mLDwasSetToArea = -1;
  mNextAsyncSumFrames = -1;
  mTD.NumAsyncSumFrames = -1;
  mDeferSumOnNextAsync = false;
  mStartingDeferredSum = false;
  if (mStartedScanning) {
    if (mTD.ScanTime || mTD.DynFocusInterval || mTD.FocusStep1)
      mScope->SetDefocus(mCenterFocus);
    if (mTD.ScanTime)
      mScope->SetBeamShift(mCenterBeamX, mCenterBeamY);
    if (!error && (mTD.ScanTime || mTD.DriftISinterval || mTD.DynFocusInterval)) {
      SEMTrace('1', "%s interval min %.0f  max %.0f  mean %.1f  Sd %.1f", 
        mTD.DynFocusInterval ? "Focus" : (mTD.ScanTime ? "Scan" : "Shift"), 
        mTD.ScanIntMin, mTD.ScanIntMax, mTD.ScanIntMean, mTD.ScanIntSD);
    }
  }
  mStartedScanning = false;
  mTiltDuringShotDelay = -1;

  if ((mShiftedISforSTEM || mMagToRestore) && (mRetainMagAndShift < 0 || error))
    RestoreMagAndShift();
  mAdjustShiftX = mAdjustShiftY = 0.;

  if (mBlankNextShot)
    mScope->BlankBeam(false);
  mBlankNextShot = false;
  mDEserverRefNextShot = 0;
  mBlankWhenRetracting = false;
  mDeferStackingFrames = false;
  mCancelNextContinuous = false;
  mStartedFalconAlign = false;
  mStartedExtraForDEalign = false;
  mDiscardImage = false;
  mMaxChannelsToGet = MAX_STEM_CHANNELS;
  if (error || mRepFlag < 0 || mHalting || mPending >= 0 ||
    (!mTD.ContinuousSTEM && !(mTD.ProcessingPlus & CONTINUOUS_USE_THREAD)))
    mScope->SetCameraAcquiring(false);
  if (mParam->FEItype && mTD.eagleIndex < 0)
    mParam->eagleIndex = mTD.eagleIndex;
  if (mRestoreFalconConfig) {
    if (!mFalconHelper->CheckFalconConfig(-2, ind, "Failed to restore initial "
      "state of Intermediate frame saving; check the FEI dialog"))
      mFrameSavingEnabled = ind > 0;
    mRestoreFalconConfig = false;
  }
  if (error) {
    TurnOffRepeatFlag();
    StopContinuousSTEM();
    mWinApp->ErrorOccurred(error);
  }
  if (mTD.errFlag)
    SEMMessageBox(mTD.errMess, MB_EXCLAME);
  mTD.errFlag = 0;
  mNoMessageBoxOnError = 0;
  if (error || mRepFlag < 0 || mHalting || mPending >= 0)
    mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  if (mRepFlag >= 0 && !mPreventUserToggle)
    mWinApp->SetStatusText(B3DCHOICE(mWinApp->mProcessImage->GetLiveFFT(), COMPLEX_PANE, 
    MEDIUM_PANE), "");
}

// Restore any mag change and image shift adjustments done for STEM and clear retain flag
void CCameraController::RestoreMagAndShift(void)
{
  if (mShiftedISforSTEM) {
    SEMTrace('s', "Restoring shifts");
    mScope->SetImageShift(mIStoRestoreX, mIStoRestoreY);
    mShiftManager->SetISTimeOut((float)(mTotalSTEMShiftDelay / 1000.));
  }
  mShiftedISforSTEM = false;
  if (mMagToRestore) {
    mScope->SetMagIndex(mMagToRestore);
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostSTEMmagChgDelay);
  }
  mMagToRestore = 0;
  mRetainMagAndShift = -1;
}

int CCameraController::ConSetToLDArea(int inConSet)
{
  int ldArea = inConSet;
  if (inConSet == MONTAGE_CONSET) {
    ldArea = RECORD_CONSET;
    if (mWinApp->LowDoseMode()) {
      MontParam *montp = mWinApp->GetMontParam();
      if (montp->useViewInLowDose)
        ldArea = VIEW_CONSET;
      else if (montp->useSearchInLowDose)
        ldArea = SEARCH_AREA;
    }
  } else if (inConSet == PREVIEW_CONSET)
    ldArea = RECORD_CONSET;
  else if (inConSet == SEARCH_CONSET)
    ldArea = SEARCH_AREA;
  else if (inConSet > 3)
    ldArea = TRIAL_CONSET;
  return ldArea;
}

  
////////////////////////////////////////////////////////////////////////
// FILTER_RELATED ROUTINES
////////////////////////////////////////////////////////////////////////

// This issues all of the commands needed to make sure the GIF is in the
// right state in general and to set slit state, slit width and energy loss
// If in spectroscopy, it will not do anything, and return 1.
// Other errors return -1
int CCameraController::SetupFilter(BOOL acquiring)
{
  FilterParams *filtParam = mWinApp->GetFilterParams();
  CString command;
  CString setOffsetOn = mNoSpectrumOffset ? 
    "" : "if (IFCGetOffsetOn() == 0)\n   IFCSetOffsetOn(1)\n";
  CString offsetFunc = mNoSpectrumOffset ? "EnergyShift" : "SpectrumOffset";
  int retval;
  static float offsetWarned = -100.0;

  // Delays for setting offset; Cut the delay if not acquiring
  float delayBase1 = acquiring ? mGIFoffsetDelayBase1 : 1.f;
  float delayBase2 = acquiring ? mGIFoffsetDelayBase2 : 1.f;
  float delaySlope1 = acquiring ? mGIFoffsetDelaySlope1 : 0.f;
  float delaySlope2 = acquiring ? mGIFoffsetDelaySlope2 : 0.f;

  // Loss must be inverted if driving energy shift instead of spectrum offset
  double loss = mWinApp->mFilterControl.LossToApply() * (mNoSpectrumOffset ? -1. : 1.);
  
  mIgnoreFilterDiffs = false;
  if (mSimulationMode)
    return 0;
  if (JEOLscope && mScope->GetHasOmegaFilter() && mScope->GetInitialized()) {
    SEMAcquireJeolDataMutex();
    if (mTD.JeolSD->spectroscopy) {
      SEMReleaseJeolDataMutex();
      return 1;
    }
    if (!mTD.scopePlugFuncs)
      mTD.scopePlugFuncs = mScope->GetPlugFuncs();
    BOOL needSlit = ((filtParam->slitIn ? 1 : 0) != (mTD.JeolSD->slitIn ? 1 : 0));
    double delOffset = fabs(mTD.JeolSD->energyShift - loss);
    BOOL needShift = ((loss ? 1 : 0) != (mTD.JeolSD->shiftOn ? 1 : 0));
    double delWidth = fabs(mTD.JeolSD->slitWidth - filtParam->slitWidth);
    SEMReleaseJeolDataMutex();

    // Let's replicate the GIF situation until we learn otherwise
    // Test if anything needs changing and return if not
    if (!needShift && delOffset < 0.1 && delWidth < 0.1 && !needSlit)
      return 0;

    mScope->ScopeMutexAcquire("SetupFilter", true);
    try {
      
      // Set shift on if loss is non-zero (per Jaap)
      if (needShift)
        mTD.scopePlugFuncs->EnableEnergyShift(loss ? 1 : 0);

      // Set slit in/out
      // Note that these delays are assumed to be milliseconds not DM ticks
      if (needSlit) {
        mTD.scopePlugFuncs->SetSlitIn(filtParam->slitIn ? 1 : 0);
        Sleep((DWORD)mGIFslitInOutDelay);
      }

      // Set slit width
      if (delWidth > 0.1)
        mTD.scopePlugFuncs->SetSlitWidth((double)filtParam->slitWidth);

      // Set energy shift
      if (delOffset > 0.1) {
        if (filtParam->positiveLossOnly && loss < 0) {
          command.Format("Attempting to set filter to a negative net loss (%.1f)\r\n"
            "Use \"Adjust Slit Offset\" to align the slit at an offset more\r\npositive"
            " than the current offset, which is %.1f", loss, filtParam->refineZLPOffset);
          loss = 0;
          if (filtParam->refineZLPOffset != offsetWarned)
            SEMMessageBox(command, MB_EXCLAME);
          else
            mWinApp->AppendToLog(command, LOG_OPEN_IF_CLOSED);
          offsetWarned = filtParam->refineZLPOffset;
        }
        
        mTD.scopePlugFuncs->SetEnergyShift(loss);
        if (delOffset < mGIFoffsetDelayCrit)
          Sleep((DWORD)(delayBase1 + delOffset * delaySlope1));
        else
          Sleep((DWORD)(delayBase2 + delOffset * delaySlope2));
      }
      retval = 0;
    }

    catch (_com_error E) {
      SEMReportCOMError(E, _T("setting filter parameters "));
      retval = -1;
    }
    mScope->ScopeMutexRelease("SetupFilter");
    return retval;
  }
 
  if (!mDMInitialized[sGIFisSocket] && Initialize(INIT_GIF_CAMERA))
    return -1;
  if (CreateCamera(CREATE_FOR_GIF))
    return -1;
  if (mDebugMode) {
    command.Format("SetupFilter %.2f, slit %d", 0.001 * (GetTickCount() % 3600000),
      filtParam->slitIn);
    mWinApp->AppendToLog(command, LOG_OPEN_IF_CLOSED);
  }

  // Exit with error if not in imaging mode
  command.Format("if (IFCGetFilterMode() == 0)\n"
          "   Exit(1)\n"
          //"   IFCSetFilterMode(1)\n"
          "%s"
          //"if (IFCGetOffsetOn() == 0)\n"
          //"   IFCSetOffsetOn(1)\n"
          "number ap = IFCGetAperture()\n"
          "if (ap > 0 && ap != %d) {\n"
          "   IFCSetAperture(%d)\n"
          "   Delay(20)\n"                  // 5 was enough
          "}\n"
          "if (IFCGetSlitIn() != %d){\n"
          "   IFCSetSlitIn(%d)\n"
          "   Delay(%d)\n"
          "}\n"
          "if (abs(IFCGetSlitWidth() - %f) > 0.1)\n"
          "   IFCSetSlitWidth(%f)\n"
          "number delOffset = abs(IFCGet%s() - (%f))\n"
          "if (delOffset > 0.1) {\n"
          "   IFCSet%s(%f)\n"
          "   if (delOffset < %f)\n"
          "      Delay(%f + delOffset * %f)\n"
          "   else\n"
          "      Delay(%f + delOffset * %f)\n"
          "}\n",
          (LPCTSTR)setOffsetOn,
          mGIFBiggestAperture, mGIFBiggestAperture,
          filtParam->slitIn ? 1 : 0, filtParam->slitIn ? 1 : 0,
          mGIFslitInOutDelay,
          filtParam->slitWidth, filtParam->slitWidth, 
          (LPCTSTR)offsetFunc, loss, (LPCTSTR)offsetFunc, loss,
          mGIFoffsetDelayCrit,
          delayBase1, delaySlope1, delayBase2, delaySlope2);

  if (mParam->hasTVCamera && mTD.Camera >= 0) {
    command += "if (IFCGetTVIn() > 0)\n   IFCSetTVIn(0)\n";
    if (mAllParams[mActiveList[mTD.Camera]].useTVToUnblank)
      mAllParams[mActiveList[mTD.Camera]].useTVToUnblank = 1;
  }
  command += "Exit(0)\n";
  try {
    retval = (int)EasyRunScript(command, 0, sGIFisSocket);
  }
  catch (_com_error E) {
    if (mFilterUpdateID)
      SEMReportCOMError(E, _T("executing script to set filter parameters "));
    retval = -1;
  }
  ReleaseCamera(CREATE_FOR_GIF);
  return retval;
}

int CCameraController::CheckFilterSettings()
{
  static DWORD eShiftTimes[MAX_ESHIFT_TIMES];
  BOOL changed = false;
  double offset, width, eShift, timeDiff;
  int states, aperture;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  CameraParameters *camParam = &mAllParams[mActiveList[filtParam->firstGIFCamera]];
  CString command;
  BOOL slitIn;
  BOOL tvIn;
  BOOL imageMode;
  int retval = 0;
  CString report;
  DWORD curTime = GetTickCount();
  BOOL timingOut = curTime < mBackToImagingTime + 7000;

  // Initialize the energy shift time table
  if (mShiftTimeIndex < 0) {
    mShiftTimeIndex = 0;
    for (int j = 0; j < MAX_ESHIFT_TIMES; j++)
      eShiftTimes[j] = 0;
    if (mNumZLPAlignChanges > MAX_ESHIFT_TIMES)
      mNumZLPAlignChanges = MAX_ESHIFT_TIMES;
  }

  if (!mDMInitialized[sGIFisSocket] || CameraBusy() || mCOMBusy) 
    return 1;
  if (CreateCamera(CREATE_FOR_GIF, false))
    return 2;
  try {
    if (mDebugMode) {
      MainCallDMIndCamera(sGIFisSocket, SetDebugMode(false));
    }
    if (!mNoSpectrumOffset) {
      command = "Number offset = IFCGetSpectrumOffset()\nExit(offset)";
      offset = EasyRunScript(command, 0, sGIFisSocket); 
    }
    command = "Number retval = IFCGetSlitWidth() + 1000 * (IFCGetAperture() + "
      "8 * IFCGetTVIn() + 16 *IFCGetSlitIn() + 32 * IFCGetFilterMode())\nExit(retval)";
    width = EasyRunScript(command, 0, sGIFisSocket);
    command = "Number shift = IFCGetEnergyShift()\nExit(shift)";
    eShift = EasyRunScript(command, 0, sGIFisSocket);
    if (mDebugMode) {
      MainCallDMIndCamera(sGIFisSocket, SetDebugMode(true));
    }
    states = (int)floor(width / 1000. + 0.1);
    width -= 1000. * states;
    aperture = states & 7;
    tvIn = (states & 8) > 0;
    slitIn = (states & 16) > 0;
    imageMode = (states & 32) > 0;

    // Change slit width unless in spectroscopy mode or just came out of it
    if (!mWasSpectroscopy && imageMode && fabs(width - filtParam->slitWidth) > 0.1) {
      changed = !mIgnoreFilterDiffs;
      if (!timingOut && !mIgnoreFilterDiffs)
        filtParam->slitWidth = (float)width;
    }

    // change slit in unless in spectroscopy mode or just came out of it
    if (!mWasSpectroscopy && imageMode && 
      (slitIn && !filtParam->slitIn || !slitIn && filtParam->slitIn)) {
      changed = !mIgnoreFilterDiffs;
      if (!timingOut && !mIgnoreFilterDiffs)
        filtParam->slitIn = slitIn;
    }
    
    // If energy shift changed, record time and see if enough changes have happened
    if (imageMode && fabs(eShift - mLastEnergyShift) > 0.1) {
      mLastEnergyShift = eShift;
      eShiftTimes[mShiftTimeIndex++] = curTime;
      mShiftTimeIndex %= MAX_ESHIFT_TIMES;
      timeDiff = 0.001 * curTime - 0.001 * eShiftTimes[(mShiftTimeIndex + 
        MAX_ESHIFT_TIMES - mNumZLPAlignChanges) % MAX_ESHIFT_TIMES];
      if (mDebugMode) {
        report.Format("Energy shift change to %.1f at %.3f, %d-change time %.3f", 
          eShift, 0.001 * (curTime % 3600000), mNumZLPAlignChanges, timeDiff);
      if (timeDiff > 0. && timeDiff <= mMinZLPAlignInterval)
        mWinApp->mFilterControl.AdjustForZLPAlign();
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      }
    }

    // Do not touch energy loss - just let it be asserted by imaging
    // Change energy loss if not in spectroscopy and TV is out
    /*if (imageMode && !tvIn && fabs(offset - filtParam->energyLoss) > 0.1) {
      changed = true;
      filtParam->energyLoss = (float)offset;
    } */
    
    // If the TV camera has been retracted, make sure flag is set to put it back in
    // for using the other camera
    if (camParam->hasTVCamera && camParam->useTVToUnblank && !tvIn)
      camParam->useTVToUnblank = 1;
  }
  catch (_com_error E) {
    retval = 3;
  }
  if (changed && !timingOut) {
    mWinApp->mFilterControl.UpdateSettings();
    if (mDebugMode) {
      report.Format("Settings changed %.2f", 0.001 * (curTime % 3600000));
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
  }
  
  ReleaseCamera(CREATE_FOR_GIF);

  // The first time back from spectroscopy, reassert the filter params for slit etc
  if (!retval && imageMode && (mWasSpectroscopy || (timingOut && changed))) {
    SetupFilter();
    if (mDebugMode) {
      report.Format("Reasserting settings %.2f", 0.001 * (curTime % 3600000));
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
  }
  if (mWasSpectroscopy && imageMode)
    mBackToImagingTime = curTime;
  mWasSpectroscopy = !imageMode;
  
  return retval;
}

// This is meant to be called from inside a try block
double CCameraController::EasyRunScript(CString command, int selectCamera, int DMindex)
{
  double scriptRet;
  long strSize = command.GetLength() / 4 + 1;
  long *strTemp = new long[strSize];
  sprintf((char *)strTemp, "%s", command);
  try {
    MainCallDMIndCamera(DMindex, ExecuteScript(strSize, strTemp, selectCamera, 
      &scriptRet));
  }
  // This is need to avoid leaking the strTemp when there is no filter control
  catch (_com_error E) {
    _com_error E2 = E;
    delete strTemp;
    throw E2;
  }
  delete strTemp;
  return scriptRet;
}

BOOL CCameraController::IsCameraFaux()
{
  return(mParam->name.Find("Faux") >= 0 || mParam->name.Find("Simulation") >= 0 || 
    mParam->name.Find("None") >= 0 || mScope->GetSimulationMode());
}

////////////////////////////////////////////////////////////////////////
// TIETZ CAMERA RELATED ROUTINES
////////////////////////////////////////////////////////////////////////

// Lock Tietz camera and initialize all that have not failed previously
int CCameraController::LockInitializeTietz(BOOL firstTime)
{
  int nlist = mWinApp->GetActiveCamListSize();
  int i, ind, err;
  int numGain, xsize, ysize;
  CString report;
  CameraParameters *camP;
     
  // Try to initialize every camera in active list, record failures
  for (i = 0; i < nlist; i++) {
    ind = mActiveList[i];
    camP = &mAllParams[ind];
    if (camP->TietzType && !camP->failedToInitialize) {
      err = mPlugFuncs[ind]->InitializeCamera(camP->TietzType);
      if (err) {
        report = mPlugFuncs[ind]->GetLastErrorString();

        if (err != TIETZ_NO_LOCK) {
          if (firstTime)
            report += "\n\nTietz camera " + camP->name + " is in the active camera list\n"
            "but could not be initialized, so it will be unavailable.";
          else
            report += "\n\nTietz camera " + camP->name + " could not be reinitialized\n"
            "after getting the camera lock back\n\nDo not try to use it!";
        }
        AfxMessageBox(report, MB_EXCLAME);
        if (err == TIETZ_NO_LOCK)
          return 1;
        camP->failedToInitialize = true;
      } else {
        numGain = mPlugFuncs[ind]->GetNumberOfGains();
        if (numGain > 0) {
          B3DCLAMP(camP->TietzGainIndex, 1, numGain);
          err = mPlugFuncs[ind]->SetGainIndex(camP->TietzGainIndex);
        }
        if (numGain < 0 || err)
          AfxMessageBox(mPlugFuncs[ind]->GetLastErrorString(), MB_EXCLAME);
        if (camP->TietzImageGeometry >= 0 &&
          mPlugFuncs[ind]->SetRotationFlip(camP->TietzType, camP->TietzImageGeometry))
          AfxMessageBox(mPlugFuncs[ind]->GetLastErrorString(), MB_EXCLAME);
        if (!camP->TietzBlocks) {
          if (mPlugFuncs[ind]->GetCameraSize(&xsize, &ysize))
            ReportOnSizeCheck(mActiveList[i], -1, -1);
          else 
            ReportOnSizeCheck(mActiveList[i], xsize, ysize);
        }
      }
    }
  }
  return 0;
}

// Switch the camera that left the beam on back to beam shutter
int CCameraController::SwitchTeitzToBeamShutter(int cam)
{
  if (mPlugFuncs[cam]->SelectCamera(mCameraWithBeamOn)) {
    AfxMessageBox("Error selecting last active Tietz camera in order to close film "
      "shutter", MB_EXCLAME);
    return 1;
  }
  if (mPlugFuncs[cam]->SelectShutter(USE_BEAM_BLANK)) {
    AfxMessageBox(_T("Error closing film shutter of last Tietz camera "), MB_EXCLAME);
    return 1;
  }
  mCameraWithBeamOn = 0;
  if (mDebugMode)
    mWinApp->AppendToLog("Closed film shutter of last Tietz camera");
  return 0;
}

// Compute the actual time of beam on specimen for common cases, or just the exposure
// time excluding drift if optional excludeDrift is true.
float CCameraController::SpecimenBeamExposure(int camera, ControlSet *conSet, 
                                              BOOL excludeDrift)
{
  return SpecimenBeamExposure(camera, conSet->exposure, conSet->drift, excludeDrift);
}

float CCameraController::SpecimenBeamExposure(int camera, float exposure, float drift,
    BOOL excludeDrift)
{
  CameraParameters *camP = mAllParams + camera;
  drift = excludeDrift ? 0 : drift;
  
  // For older Gatan camera, enforce minimum drift settling and add extra time
  if (drift && camP->GatanCam && !camP->K2Type && !camP->OneViewType) {
    if (drift < camP->minimumDrift)
      drift = camP->minimumDrift;
    drift += camP->extraBeamTime;
  }

  // Subtract dead time but do not return negative number 
  drift += exposure - camP->deadTime;
  if (drift <= 0.)
    drift = 0.0001f;
  return drift;
}

void CCameraController::SetupBeamScan(ControlSet *conSetp)
{
  float specY, minY, maxY, totalY;
  double umPerMs;
  int top, left, bottom, right;
  CString message;
  int camera = mWinApp->GetCurrentCamera();
  int magInd = mScope->GetMagIndex();
  float pixel = mShiftManager->GetPixelSize(camera, magInd);
  double angle = mScope->GetTiltAngle();
  double tanAng = tan(DTOR * angle);
  ScaleMat ca2sp = mShiftManager->CameraToSpecimen(magInd);
  ScaleMat IS2sp = mShiftManager->IStoSpecimen(magInd);
  ScaleMat sp2bs = mShiftManager->MatMul(mShiftManager->MatInv(IS2sp), mTD.IStoBS);
  mCenterFocus = mScope->GetDefocus();
  mScope->GetBeamShift(mCenterBeamX, mCenterBeamY);

  // Convert frame to unbinned pixels relative to center
  top = mTop * mBinning - mParam->sizeY / 2;
  bottom = mBottom * mBinning - mParam->sizeY / 2;
  left = mLeft * mBinning - mParam->sizeX / 2;
  right = mRight * mBinning - mParam->sizeX / 2;

  // Find Y specimen displacement of corners of frame and thus the min and max Y
  minY = 10000.;
  maxY = -10000.;
  specY = ca2sp.ypx * left + ca2sp.ypy * top;
  minY = minY < specY ? minY : specY;
  maxY = maxY > specY ? maxY : specY;
  specY = ca2sp.ypx * left + ca2sp.ypy * bottom;
  minY = minY < specY ? minY : specY;
  maxY = maxY > specY ? maxY : specY;
  specY = ca2sp.ypx * right + ca2sp.ypy * top;
  minY = minY < specY ? minY : specY;
  maxY = maxY > specY ? maxY : specY;
  specY = ca2sp.ypx * right + ca2sp.ypy * bottom;
  minY = minY < specY ? minY : specY;
  maxY = maxY > specY ? maxY : specY;

  // get starting Y and total Y extent of scan and rate of movement in micron/ms
  specY = minY - (float)(pixel * (mScanMargin + mBeamWidth / 2.));
  totalY = maxY + (float)(pixel * (mScanMargin + mBeamWidth / 2.)) - specY;
  umPerMs = 0.001 * pixel * mBeamWidth / mExposure;

  // Set up the scan time and focus and beam at start and per ms
  mTD.ScanTime = totalY / umPerMs;
  mTD.FocusBase = mCenterFocus - minY * tanAng;
  mTD.BeamBaseX = mCenterBeamX + minY * sp2bs.xpy;
  mTD.BeamBaseY = mCenterBeamY + minY * sp2bs.ypy;
  mTD.FocusPerMs = -umPerMs * tanAng;
  mTD.BeamPerMsX = umPerMs * sp2bs.xpy;
  mTD.BeamPerMsY = umPerMs * sp2bs.ypy;
  mTD.ScanSleep = mScanSleep;

  // Set up start delay and true exposure time
  mTD.ScanDelay = (int)(1000. * (mParam->startupDelay + mParam->builtInSettling + 
    conSetp->drift + mParam->extraBeamTime));
  mExposure = 0.001 * mTD.ScanTime + conSetp->drift + 2 * mParam->extraBeamTime;
  mTD.ShutterMode = mParam->beamShutter;
  mTD.PostActionTime = 0;
  if (mStageQueued || mISQueued || mMagQueued)
    mTD.PostActionTime = (int)(1000. * mParam->extraBeamTime);
  if (mDebugMode) {
    message.Format("Start Y %.3f  Total Y %.3f  Delay %d  Total Time %.1f  Exposure %.3f"
      , specY, totalY, mTD.ScanDelay, mTD.ScanTime, mExposure);
    mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  }
}

// Set up parameters for doing dynamic focus
void CCameraController::SetupDynamicFocus(int numIntervals, double msPerLine, 
                                          float flyback, float startup)
{
  double startY, startZ;
  float refZ, refFocus, rampZ, startFocus;
  CString message;
  int i, tableInd;
  int camera = mWinApp->GetCurrentCamera();
  float pixel = mShiftManager->GetPixelSize(camera, mMagBefore);
  float defocusToDelZ = mWinApp->mFocusManager->GetSTEMdefocusToDelZ(-1);

  // Get the rotation angle of the native scan before any operations on image,
  // and invert the tangent of the angle if it is 180.  This will adjust both the
  // starting Z and the change in Z during the scan
  double nativeRotation = mParam->imageRotation - (mParam->rotationFlip % 4) * 90.;
  double tanAng = tan(DTOR * mTiltBefore) * (mParam->invertFocusRamp ? -1. : 1.);
  if (fabs(UtilGoodAngle(nativeRotation)) > 90.)
    tanAng = -tanAng;
  mCenterFocus = mScope->GetDefocus();

  // The specimen Y at the start of the scan in an unrotated image; and the true 
  // starting Z, adjusted for possible 180 degree rotation
  startY = 0.5 * mTD.DMSizeY * mBinning * pixel * (numIntervals + 1.) / numIntervals;
  startZ = startY * tanAng;
  mTD.FocusBase = mCenterFocus + startZ / defocusToDelZ;
  mTD.JeolOLtoUm = mScope->GetJeol_OLfine_to_um() * mScope->GetJeolSTEMdefocusFac();

  mTD.FocusPerMs = -mBinning * pixel * tanAng / (msPerLine * defocusToDelZ);
  mTD.PostActionTime = (int)(1000. * mExposure + mTD.DMSizeY * flyback / 1000.);
  mTD.ScanDelay = (int)(1000. * startup);
  mTD.IndexPerMs = MAX_RAMP_STEPS / (1.07 * mTD.PostActionTime);
  if (FEIscope) {

    // For FEI, setup the default ramp, then look for a calibration table
    for (i = 0; i < MAX_RAMP_STEPS; i++)
      mTD.rampTable[i] = (float)((i / mTD.IndexPerMs) * mTD.FocusPerMs);
    tableInd = mWinApp->mFocusManager->FindFocusZTable(mScope->GetSpotSize(), 
      mScope->GetProbeMode());
    if (tableInd >= 0) {

      // These are Z and defocus in the table corresponding to the center position
      refZ = mWinApp->mFocusManager->LookupZFromAbsFocus(tableInd, 
        (float)mScope->GetFocus());
      refFocus = mWinApp->mFocusManager->LookupDefocusFromZ(tableInd, refZ);

      // Revise the base focus: change in focus is the focus in the table at the starting
      // Z (plus reference Z) minus the focus at the reference Z
      startFocus = mWinApp->mFocusManager->LookupDefocusFromZ(tableInd, refZ + 
        (float)startZ);
      mTD.FocusBase = mCenterFocus + startFocus - refFocus;

      // For each step of ramp, get the Z to look up in the table, and get the change in
      // focus from the start 
      for (i = 0; i < MAX_RAMP_STEPS; i++) {
        rampZ = (float)(refZ + startZ - (i / mTD.IndexPerMs) * mBinning * pixel * tanAng /
          msPerLine);
        mTD.rampTable[i] = mWinApp->mFocusManager->LookupDefocusFromZ(tableInd, rampZ) - 
          startFocus;
      }
    }
  }
  SEMTrace('s', "Center focus %.2f  base focus %.2f  numInt %d  focusPerMs %f  indexPerMs"
    " %f", mCenterFocus, mTD.FocusBase, numIntervals, mTD.FocusPerMs, mTD.IndexPerMs);
}


BOOL CCameraController::CreateFocusRamper(void)
{
  HRESULT hr = CoCreateInstance(__uuidof(CoFocusRamper), NULL, CLSCTX_LOCAL_SERVER,
    __uuidof(IFocusRamper), (LPVOID *)&mTD.FocusRamper);
  mRamperInstance = SUCCEEDED(hr);
  return SEMTestHResult(hr, "Creating instance of FocusRamper");
}

// Set time for post-exposure actions; set up test blanking if extra beam time negative
void CCameraController::SetNonGatanPostActionTime(void)
{
  mTD.PostActionTime = 0;
  if (mParam->extraBeamTime < 0.) {
    mTD.UnblankTime = B3DNINT(1000. * mParam->startupDelay);
    if (!mTD.UnblankTime)
      mTD.UnblankTime = 1;
  }
  if (mStageQueued || mISQueued || mMagQueued || (mDriftISQueued && mBeamWidth <= 0.))
    mTD.PostActionTime = (int)(1000. * (mParam->startupDelay + mExposure + 
    mTD.DMsettling + mParam->extraBeamTime));
}

int CCameraController::GetArrayForImage(CameraThreadData * td, long & arrSize, int index)
{
  arrSize = (td->DMSizeX + 4) * (td->DMSizeY + 4);
  NewArray(td->Array[index],short int,arrSize);
  if (!td->Array[index]) {
    DeferMessage(td, "Failed to get memory for image!");
    return 1;
  }
  return 0;
}

int CCameraController::GetArrayForReference(CameraThreadData *td, DarkRef *ref, 
                                            long &arrSize, CString strGainDark)
{
  char *arrtmp;
  arrSize = (ref->SizeX + 4) * (ref->SizeY + 4);
  if (!ref->Array) {
    NewArray(arrtmp, char, arrSize * ref->ByteSize);
    ref->Array = (void *)arrtmp;
    if (!ref->Array) {
      DeferMessage(td, "Cannot get memory for " + strGainDark + " reference");
      return 1;
    }
  }
  return 0;
}

int CCameraController::GetSizeCopyFEIimage(CameraThreadData *td, SAFEARRAY * psa, 
                                           void * array, int imageType, int divideBy2, 
                                           int sizeX, int sizeY, int messInd)
{
  long width, height, retval = 0;
  char *messText[] = {"Image", "Dark reference", "STEM channel"};
  CString message;
  HRESULT hr = SafeArrayGetUBound(psa, 1, &width);
  if (CCTestHResult(td, hr, "getting SafeArray X upper bound"))
    return 1;
  hr = SafeArrayGetUBound(psa, 2, &height);
  if (CCTestHResult(td, hr, "getting SafeArray Y upper bound"))
    return 1;
  width++;
  height++;
  SEMTrace('E', "SA width %d height %d", width, height);

  // Check the size, try to copy the image.  
  if (width != sizeX || height != sizeY) {
    B3DCLAMP(messInd, 0, 2);
    message.Format("%s is %d by %d, not the expected size of %d by %d", messText[messInd],
      width, height, sizeX, sizeY);
    DeferMessage(td, message);
    retval = 1;
  } else 
    retval = CopyFEIimage(td, psa, array, width, height, td->ImageType, td->DivideBy2);
  hr = SafeArrayDestroy(psa);
  CCTestHResult(td, hr, "destroying SAFEARRAY from FEI camera");
  return retval;
}

int CCameraController::CopyFEIimage(CameraThreadData *td, SAFEARRAY * psa, void * array, 
                                    int sizeX, int sizeY, int imageType, int divideBy2)
{
  // I2, UI2, and I4 are allowed
  // I2 and UI2 without divide is a straight copy if type matches
  // I4 needs a check
  // divide always goes to signed data, not divide can be signed or unsigned
  VARTYPE pvt;
  void HUGEP *pdata;
  short *sin, *sout;
  unsigned short *usin, *usout;
  int *intin;
  short sval;
  unsigned short usval;
  int ival, i, j;
  if (!psa) {
    DeferMessage(td, "Got a null safearray from FEI camera");
    return 4;
  }
  HRESULT hr = SafeArrayGetVartype(psa, &pvt);
  if (CCTestHResult(td, hr, _T("Failed to get type of safe array from FEI camera ")))
    return 1;
  if (pvt != VT_I2 && pvt != VT_UI2 && pvt != VT_I4) {
    DeferMessage(td, "FEI camera returned unsupported type of data");
    return 2;
  }

  hr = SafeArrayAccessData(psa, &pdata);
  if (CCTestHResult(td, hr, _T("Failed to get access to safe array from FEI camera ")))
    return 3;
 
  if (!pdata)
    return 4;

  // Get all possible pointers for input and output
  sout = (short *)array;
  usout = (unsigned short *)array;
  SEMTrace('E', "going to process %x %x %d %d %d %d", pdata, array, pvt, VT_I2, VT_UI2, 
    VT_I4);

  for (j = 0; j < sizeY; j++) {
    if (divideBy2) {

      // Divide by 2 output is always signed
      switch (pvt) {
      case VT_I2:
        sin = (short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++)
          *sout++ = *sin++ / 2;
        break;
      case VT_UI2:
        usin = (unsigned short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++)
          *sout++ = (short)(*usin++ / 2);
        break;
      case VT_I4:
        intin = (int *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++) {
          ival = (*intin++) / 2;
          ival = B3DMIN(32767, ival);
          *sout++ = B3DMAX(-32768, ival);
        }
        break;
      }

    } else if (imageType == kSHORT) {

      // Otherwise, also signed output
      switch (pvt) {
      case VT_I2:
        sin = (short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++)
          *sout++ = *sin++;
        break;
      case VT_UI2:
        usin = (unsigned short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++) {
          usval = *usin++;
          *sout++ = (short)B3DMIN(32767, usval);
        }
        break;
      case VT_I4:
        intin = (int *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++) {
          ival = *intin++;
          ival = B3DMIN(32767, ival);
          *sout++ = B3DMAX(-32768, ival);
        }
        break;
      }
    } else {

      // And unsigned output
      switch (pvt) {
      case VT_I2:
        sin = (short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++) {
          sval = *sin++;
          *usout++ = (unsigned short)B3DMAX(0, sval);
        }
        break;
      case VT_UI2:
        usin = (unsigned short *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++)
          *usout++ = *usin++;
        break;
      case VT_I4:
        intin = (int *)pdata + sizeX * j;
        for (i = 0; i < sizeX; i++) {
          ival = *intin++;
          ival = B3DMIN(65535, ival);
          *usout++ = B3DMAX(0, ival);
        }
        break;
      }
    }
  }
  hr = SafeArrayUnaccessData(psa);
  return 0;
}

// Acquire an image from FEI camera with given settling and corrections
int CCameraController::AcquireFEIimage(CameraThreadData *td, void *array, int correction,
                                       double settling, int sizeX, int sizeY, int messInd)
{
  if (!FEIscope)
    return 0;
  CString message;
  long retval = 0, index = 0;
  static bool needFalconShutter = true;
  bool advanced = (td->CamFlags & PLUGFEI_USES_ADVANCED) != 0;
  DWORD ticks = GetTickCount();

  if (td->scopePlugFuncs->BeginThreadAccess(2, advanced ? PLUGFEI_MAKE_NOBASIC : 0)) {
    DeferMessage(td, "Error creating second instance of microscope object\n"
      " for acquiring image");
    SEMErrorOccurred(1);
    return 1;
  }
  if (advanced) {
    SEMTrace('E', "Calling ASIacquireFromcamera %p %d %d %f %d %d %d %d %d %d %d %d %d "
      "%d %f", array, sizeX, sizeY,  td->Exposure,  messInd, td->Binning, 
      td->restrictedSize, td->ImageType, td->DivideBy2, td->eagleIndex, 
      (td->CamFlags & PLUGFEI_CAN_DOSE_FRAC) ? td->SaveFrames : -1, td->GatanReadMode,
      (td->CamFlags & PLUGFEI_CAM_CAN_ALIGN) ? td->AlignFrames : -1, td->FEIacquireFlags,
      td->CountScaling);
    retval = td->scopePlugFuncs->ASIacquireFromCamera(array, sizeX, sizeY,  
      td->Exposure,  messInd, td->Binning, td->restrictedSize, td->ImageType,
      td->DivideBy2, td->eagleIndex, 
      (td->CamFlags & PLUGFEI_CAN_DOSE_FRAC) ? td->SaveFrames : -1, td->GatanReadMode,
      (td->CamFlags & PLUGFEI_CAM_CAN_ALIGN) ? td->AlignFrames : -1, td->FEIacquireFlags,
      0, 0, td->CountScaling, 0.);

  } else
    retval = td->scopePlugFuncs->AcquireFEIimage(array, sizeX, sizeY, correction, 
      td->Exposure, settling, messInd, td->Binning, td->restrictedSize, td->ImageType,
      td->DivideBy2, &td->eagleIndex, (LPCTSTR)td->cameraName, td->checkFEIname,
      td->FEItype, td->oneFEIshutter, td->FauxCamera, &td->startingFEIshutter);
  if (retval)
    message = td->scopePlugFuncs->GetLastErrorString();
  td->scopePlugFuncs->EndThreadAccess(2);
  if (retval == 2 && td->MakeFEIerrorTimeout) {

    // Turn into a timeout
    SEMTrace('1', "COM Error from FEI Camera, turning into a timeout by sleeping %d", 
      td->cameraTimeout + td->blankerTimeout + 3000);
    delete [] td->Array[0];
    td->Array[0] = NULL;
    Sleep(td->cameraTimeout + td->blankerTimeout + 3000);
  }
  if (retval) {
    DeferMessage(td, message);
    SEMErrorOccurred(retval);
  }
  return retval;
}

// Acquire a set of STEM images from FEI 
int CCameraController::AcquireFEIchannels(CameraThreadData * td, int sizeX, int sizeY)
{
  if (JEOLscope)
    return 0;
  CString message;
  long chan, arrSize, numAlloc, retval = 0, numAdded = 0, numCopied = 0;
  DWORD ticks = GetTickCount();
  const char *chanNames[MAX_STEM_CHANNELS];
  SEMTrace('E', "Entering AcquireFEIchannels");

  if (td->scopePlugFuncs->BeginThreadAccess(2, 0)) {
    DeferMessage(td, "Error creating second instance of microscope object\n"
      " for acquiring image");
    SEMErrorOccurred(1);
    return 1;
  }
  for (chan = 0; chan < td->NumChannels; chan++) {
    chanNames[chan] = (const char *)(LPCTSTR)td->ChannelName[chan];
    if (chan && GetArrayForImage(td, arrSize, chan)) {
      SEMErrorOccurred(1);
      retval = 1;
      break;
    }
  }
  numAlloc = chan;
  if (!retval) {
    retval = td->scopePlugFuncs->AcquireFEIchannels(&td->Array[0], &chanNames[0], 
      td->NumChannels, &numCopied, sizeX, sizeY, td->Exposure, td->STEMrotation * DTOR, 
      td->Binning, td->restrictedSize, td->ImageType, td->DivideBy2);
    if (retval)
      DeferMessage(td, CString(td->scopePlugFuncs->GetLastErrorString()));
  }
  td->scopePlugFuncs->EndThreadAccess(2);

  // Return value 2 means numCopied is correct
  if (retval && retval != 2)
    numCopied = 0;

  // Clean up unused arrays
  for (chan = numCopied; chan < numAlloc; chan++) {
    delete td->Array[chan];
    td->Array[chan] = NULL;
  }

  if (!numCopied)
    return 1;

  // Issue a message about not enough channels if there has been no other message
  if (!retval && numCopied < td->NumChannels) {
    message.Format("STEM data were acquired from only %d detectors, not the %d requested",
      numCopied, td->NumChannels);
    DeferMessage(td, message);
  }
  td->NumChannels = numCopied;
  return 0;
}

// Make a "best effort" to restore an original shutter setting of Eagle camera
void CCameraController::RestoreFEIshutter(void)
{
  if (mTD.startingFEIshutter == VALUE_NOT_SET)
    return;

  //  Loop on the cameras to find the Eagle or other shuttering camera and try to reset
  // the shutter with it
  for (int cam = 0; cam < mWinApp->GetNumActiveCameras(); cam++) {
    CameraParameters *camP = &mAllParams[mActiveList[cam]];
    if (camP->FEItype == EAGLE_TYPE && !camP->STEMcamera &&
      !mScope->LookupScriptingCamera(camP, false, mTD.startingFEIshutter)) {
        mTD.startingFEIshutter = VALUE_NOT_SET;
        return;
    }
  }
}

void CCameraController::TestGainFactor(short * array, int sizeX, int sizeY, int binning)
{
  if (!mWinApp->GetTestGainFactors())
    return;
  float factor = mWinApp->GetGainFactor(mWinApp->GetCurrentCamera(), binning);
  if (factor == 1.)
    return;
  for (int i = 0; i < sizeX * sizeY; i++)
    array[i] = (short)(factor * array[i]);
}

// AMT camera specific managing of beam blanking
void CCameraController::SetAMTblanking()
{
  if (mDMInitialized[AMT_IND] && mParam->AMTtype)
    SetAMTblanking(mScope->GetScreenPos() == spUp);
}

void CCameraController::SetAMTblanking(bool blank)
{
  try {
    if (mDMInitialized[AMT_IND] && mParam->AMTtype) {
      SEMTrace('1', "SetAMTblanking, blank = %d, screendownmode = %d, sending %d", 
        blank ? 1 : 0, mScreenDownMode ? 1 : 0, (blank || mScreenDownMode) ? -1 : 0);
      mTD.amtCam->SetShutterNormallyClosed(mTD.SelectCamera, 
        (blank || mScreenDownMode) ? -1 : 0);
    }
  }
  catch (_com_error E) {
    SEMReportCOMError(E, _T("setting blanking state of AMT camera "));
  }
}

CString CCameraController::CurrentSetName(void)
{
  int nameInd = mLastConSet == MONTAGE_CONSET ? RECORD_CONSET : mLastConSet;
  if (mLastConSet == MONTAGE_CONSET && mWinApp->LowDoseMode()) {
    nameInd = ConSetToLDArea(mLastConSet);
    if (nameInd == SEARCH_AREA)
      nameInd = SEARCH_CONSET;
  }
  CString *modeNamep = mWinApp->GetModeNames() + nameInd;
  return *modeNamep;
}

// Queue drift compensation by image shift with a drift rate in nm/s or unbinned pixel/s
void CCameraController::QueueDriftRate(double driftX, double driftY, bool convertPixels)
{
  int magInd, camera;
  double pixel;
  if (convertPixels) {

    // Convert pixels to nanometers at the current mag
    magInd = mScope->FastMagIndex();
    camera = mWinApp->GetCurrentCamera();
    pixel = 1000. * mShiftManager->GetPixelSize(camera, magInd);
    driftX *= pixel;
    driftY *= pixel;
  }
  mDriftXnmPerSec = (float)driftX;
  mDriftYnmPerSec = (float)driftY;
  mDriftISQueued = true;
}

// Set up drift compensation with IS, or handle all the flags if not doing it
void CCameraController::SetupDriftWithIS(void)
{
  ScaleMat aMat, aInv;
  double pixel;
  mTD.DriftISinterval = 0;
  if (mDriftISQueued && mBeamWidth <= 0.) {

    // 1000 converts micron pixel to nanometers, times 1000 convert drift per sec to msec
    pixel = 1.e6 * mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), mMagBefore);
    aMat = mShiftManager->IStoCamera(mMagBefore);

    // If the scaling exists, convert nm back to pixels and set flag and properties
    if (aMat.xpx && pixel) {
      aInv = MatInv(aMat);
      mTD.ISperMsX = (mDriftXnmPerSec * aInv.xpx + mDriftYnmPerSec * aInv.xpy) / pixel;
      mTD.ISperMsY = (mDriftXnmPerSec * aInv.ypx + mDriftYnmPerSec * aInv.ypy) / pixel;
      mTD.DriftISinterval = mDriftISinterval;
    }
  }
  mDriftISQueued = false;
}

// Call SEMReportCOMError but save the error message and set flag for later message box
void CCameraController::CCReportCOMError(CameraThreadData *td, _com_error E, CString inString)
{
  if (td->errFlag)
    SEMReportCOMError(E, inString);
  else
    SEMReportCOMError(E, inString, &td->errMess);
  td->errFlag++;
}

// Call SEMTestHResult but save the error message and set flag
BOOL CCameraController::CCTestHResult(CameraThreadData *td, HRESULT hr, CString inString)
{
  return SEMTestHResult(hr, inString,  &td->errMess, &td->errFlag);
}

// Save a message to be put up later
void CCameraController::DeferMessage(CameraThreadData *td, CString inString)
{
  if (!td->errFlag)
    td->errMess = inString;
  td->errFlag++;
}


// Convert program/user coordinates to coordinates on Tietz CCD chip
void CCameraController::UserToTietzCCD(int geometry, int binning, int &camSizeX, 
                                       int &camSizeY, int &imSizeX, int &imSizeY, 
                                       int &top, int &left, int &bottom, int &right)
{
  if (geometry & 16)
    CorDefRotateCoordsCCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, 
    bottom, right);
  if (geometry & 8) {
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
    CorDefMirrorCoords(binning, camSizeX, left, right);
  }
  if (geometry & 4)
    CorDefRotateCoordsCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, bottom, 
    right);
  if (geometry & 2)
    CorDefMirrorCoords(binning, camSizeX, left, right);
  if (geometry & 1) 
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
}

// COnvert coordinates on Tietz chip back to user coordinates
void CCameraController::TietzCCDtoUser(int geometry, int binning, int &camSizeX, 
                                       int &camSizeY, int &imSizeX, int &imSizeY, 
                                       int &top, int &left, int &bottom, int &right)
{
  if (geometry & 1) 
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
  if (geometry & 2)
    CorDefMirrorCoords(binning, camSizeX, left, right);
  if (geometry & 4)
    CorDefRotateCoordsCCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left,
    bottom, right);
  if (geometry & 8) {
    CorDefMirrorCoords(binning, camSizeY, top, bottom);
    CorDefMirrorCoords(binning, camSizeX, left, right);
  }
  if (geometry & 16)
    CorDefRotateCoordsCW(binning, camSizeX, camSizeY, imSizeX, imSizeY, top, left, bottom, 
    right);
}

////////////////////////////////////////////////////////////////////
// Various Support Routines
////////////////////////////////////////////////////////////////////

void CCameraController::SetupNewDarkRef(int inSet, double exposure)
{
  mDarkp = new DarkRef;
  mDarkp->ByteSize = 2;
  mDarkp->GainDark = DARK_REFERENCE;
  mDarkp->Binning = mBinning;
  mDarkp->Camera = mTD.Camera;
  mDarkp->SizeX = mDMsizeX;
  mDarkp->SizeY = mDMsizeY;
  mDarkp->Top = mTop;
  mDarkp->Left = mLeft;
  mDarkp->Bottom = mBottom;
  mDarkp->Right = mRight;
  mDarkp->Exposure = exposure;
  mDarkp->Delay = mDelay;
  mDarkp->UseCount = mUseCount;
  mDarkp->Array = NULL;
  mDarkp->ConSetUsed = inSet;
  mDarkp->xOffset = 0;
  mDarkp->yOffset = 0;
  mDarkp->Ownership = 1;
  AddRefToArray(mDarkp);
}

// Obtain the size from a Gatan camera and issue warnings
void CCameraController::CheckGatanSize(int camNum, int paramInd)
{
  CString mess, mess2;
  int xsize, ysize, DMind;
  int transposeMap[8] = {0, 258, 3, 257, 1, 256, 2, 259};
  CameraParameters *camP = &mAllParams[paramInd];
  bool getBoundaryDir = camP->useContinuousMode && camP->balanceHalves && 
    camP->ifHorizontalBoundary < 0;
  int setRotFlip = camP->setRestoreRotFlip;
  bool doOrient = true, setRF = camP->setRestoreRotFlip > 0;
  DMind = CAMP_DM_INDEX(camP);

  // First check the rotation and flip settings
  xsize = camP->DMrotationFlip;
  if ((xsize < 0 && !getBoundaryDir) || camP->STEMcamera)
    doOrient = false;
  if (xsize > 7) {
    mess.Format("DMrotationAndFlip property for camera %d is %d and must be between 0 "
      "and 7", camNum, xsize);
    AfxMessageBox(mess, MB_EXCLAME);
    doOrient = false;
  }
  if (doOrient) {
    mess.Format("%s %d)\n"
      "Number xx = CM_Config_GetDefaultTranspose(camera)\n"
      "Exit(xx)\n", sGetCamObjText, camNum);
    try {
      ysize = B3DNINT(EasyRunScript(mess, 0, DMind));
      if (xsize >= 0 && ysize != transposeMap[xsize]) {
        if (setRotFlip > 0) {
           mess2.Format("WARNING: The camera configuration in DigitalMicrograph was\r\n"
             "        incorrect for camera # %d, %s\r\n        "
             "It has now been set to rotate by %d, %sflip around Y\r\n",
            paramInd, (LPCTSTR)camP->name, 90 * (xsize % 4), xsize < 4 ? "NO " : "");
          if (setRotFlip > 1) {
            camP->rotFlipToRestore = ysize;
            mess2 += "        The original setting will be restored when the program "
              "exits";
          }
        } else {
          mess.Format("The camera configuration in DigitalMicrograph is incorrect for "
            "camera # %d, %s\nIt should be set to rotate by %d, %sflip around Y\n\n"
            "Do you want SerialEM to fix this configuration?", paramInd, 
            (LPCTSTR)camP->name, 90 * (xsize % 4), xsize < 4 ? "NO " : "");
          setRF = AfxMessageBox(mess, MB_QUESTION) == IDYES;
        }
        if (setRF) {
          mess.Format("%s %d)\n"
            "CM_Config_SetDefaultTranspose(camera, %d)\n", sGetCamObjText, camNum, 
            transposeMap[xsize]);
          EasyRunScript(mess, 0, DMind);
          if (setRotFlip > 0)
            mWinApp->AppendToLog(mess2);
        }
      }

      // To get boundary, find rotation/flip if needed then see if there is a +/-90 rotation
      if (getBoundaryDir) {
        if (xsize < 0) {
          for (xsize = 0; xsize < 8; xsize++)
            if (transposeMap[xsize] == ysize)
              break;
        }
        camP->ifHorizontalBoundary = xsize % 2;
      }
    }
    catch (_com_error E) {
      SEMReportCOMError(E, "Running script to check rotation and flip");
      return;
    }
  }

  // Now do the size after fixing the config
  mess.Format("%s %d)\n"
    "Number xx, yy\n"
    "Number trans = CM_Config_GetDefaultTranspose(camera)\n"
    "if (trans > 255)\n"
    "  CM_CCD_GetSize(camera, yy, xx)\n"
    "else\n"
    "  CM_CCD_GetSize(camera, xx, yy)\n", sGetCamObjText, camNum);
  try {
    xsize = B3DNINT(EasyRunScript(mess + "Exit(xx + 100000 * yy)", 0, DMind));
    ysize = xsize / 100000;
    xsize = xsize % 100000;

    // No longer need to swap the size check numbers with above script
    camP->sizeCheckSwapped = B3DMIN(0, camP->sizeCheckSwapped);
    ReportOnSizeCheck(paramInd, xsize, ysize);
  }
  catch (_com_error E) {
    if (IsCameraFaux())
      SEMReportCOMError(E, "Running size script");
    ReportOnSizeCheck(paramInd, -1, -1);
    return;
  }
}

// Issue warnings for a camera size not the same as in properties
void CCameraController::ReportOnSizeCheck(int paramInd, int xsize, int ysize)
{
  CameraParameters *camP = &mAllParams[paramInd];
  CString mess;
  int propX = camP->sizeX;
  int propY = camP->sizeY;
  if (camP->sizeCheckSwapped < 0)
    return;
  if (camP->sizeCheckSwapped > 0) {
    int temp = xsize;
    xsize = ysize;
    ysize = temp;
  }
  if (camP->K2Type) {
    propX /= 2;
    propY /= 2;
  }
  if (xsize < 0 && ysize < 0) {
    mess.Format("Warning: Could not check the size of camera # %d, %s", paramInd,
      (LPCTSTR)camP->name);
    mWinApp->AppendToLog(mess);
  } else if (xsize < propX || ysize < propY) {
    mess.Format("WARNING: Camera # %d, %s, reports a smaller size (%d x %d)\n"
      " than the size in the properties file (%d x %d)\n\n"
      "This may make the program crash or misbehave", paramInd, 
      (LPCTSTR)camP->name, xsize, ysize, propX, propY);
    AfxMessageBox(mess, MB_EXCLAME);
  } else if (xsize > propX || ysize > propY) {
    mess.Format("Warning: Camera # %d, %s, reports a bigger size (%d x %d)\r\n"
      "       than the size in the properties file (%d x %d)", paramInd,
      (LPCTSTR)camP->name, xsize, ysize, propX, propY);
    mWinApp->AppendToLog(mess);
  }
}

// Starting call for updating the K2 hardware dark reference if it has not been done in
// a time longer than the interval, or unconditionally if the time is 0, or simply to 
// set the time that it was done if the argument is < 0
int CCameraController::UpdateK2HWDarkRef(float hoursSinceLast)
{
  int oper = LONG_OP_HW_DARK_REF;
  if (mParam->K2Type != 1) {
    SEMMessageBox("Cannot update K2 hardware dark reference, the current camera is not"
      " a K2");
    return 2;
  }
  return(mScope->StartLongOperation(&oper, &hoursSinceLast, 1));
}

// Starts the thread for the long operation routine
CWinThread *CCameraController::StartHWDarkRefThread(void)
{
  mITD.DMindex = CAMP_DM_INDEX(mParam);
  return (AfxBeginThread(HWDarkRefProc, &mITD, THREAD_PRIORITY_NORMAL, 0, 
    CREATE_SUSPENDED));
}

// The thread proc that makes the call
UINT CCameraController::HWDarkRefProc(LPVOID pParam)
{
  int retval = 0;
  InsertThreadData *itd = (InsertThreadData *)pParam;
  IDMCamera *pGatan;
  HRESULT hr = S_OK;
  double scriptRet;
  long strTemp[12];
  sprintf((char *)strTemp, "K2_UpdateHardwareDarkReference(camera)\n");
  if (itd->DMindex == COM_IND) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CreateDMCamera(pGatan);
    if (SEMTestHResult(hr, " getting second instance of camera for updating hardware"
      " dark ref"))
      retval = 1;
  }
  if (!retval){
    try {
      CallDMIndCamera(itd->DMindex, pGatan, itd->td->amtCam,
        ExecuteScript(12, strTemp, 1, &scriptRet));
    }
    catch (_com_error E) {
      SEMReportCOMError(E, _T("Error running script to update hardware dark reference "));
      retval = 1;
    }
    if (itd->DMindex == COM_IND)
      pGatan->Release();
  }
  if (itd->DMindex == COM_IND)
    CoUninitialize();
  return retval;
}

// If K2 gain references have been loaded into DM, check if it is time to free them
// or free them unconditionally on program exit
void CCameraController::CheckAndFreeK2References(bool unconditionally)
{
  int which = 0, now, dmInd;
  HRESULT hr = S_OK;
  if (CameraBusy() || (mK2GainRefTimeStamp[0][0] < 0 && mK2GainRefTimeStamp[0][1] < 0 &&
    mK2GainRefTimeStamp[1][0] < 0 && mK2GainRefTimeStamp[1][1] < 0))
    return;
  now = mWinApp->MinuteTimeStamp();

  // Check both camera interfaces
  for (dmInd = 0; dmInd < 2; dmInd++) {
    which = 0;
    if (mK2GainRefTimeStamp[dmInd][0] > 0 && (now - mK2GainRefTimeStamp[dmInd][0] > 
      mK2GainRefLifetime || unconditionally))
        which = 1;
    if (mK2GainRefTimeStamp[dmInd][1] > 0 && (now - mK2GainRefTimeStamp[dmInd][1] > 
      mK2GainRefLifetime || unconditionally))
        which += 2;
    if (which) {
      if (dmInd == COM_IND && CreateCamera(CREATE_FOR_ANY, false))
        continue;
      try {
        CallDMIndGatan(dmInd, mGatanCamera, FreeK2GainReference(which));
        if (which & 1)
          mK2GainRefTimeStamp[dmInd][0] = -1;
        if (which & 2)
          mK2GainRefTimeStamp[dmInd][1] = -1;
      }
      catch (_com_error E) {
      }
      if (dmInd == COM_IND)
        mGatanCamera->Release();
    }
  }
}

// Acquire the defect list for a K2 camera using a current main thread connection
void CCameraController::GetMergeK2DefectList(int DMind, CameraParameters *param, 
  bool errToLog)
{
  long pairSize, numPoints, numTotal;
  short *xyPairs = NULL;
  CString str;
  pairSize = 500000;

  // First time, duplicate the defects from properties; after that, copy that to defects
  if (!param->origDefects) {
    param->origDefects = new CameraDefects;
    *param->origDefects = param->defects;
  } else {
    param->defects = *param->origDefects;
  }

  NewArray(xyPairs, short int, pairSize);
  if (!xyPairs) {
    str = "Failed to allocate array for getting K2 camera defects";
    if (errToLog)
      mWinApp->AppendToLog(CString("WARNING: ") + str);
    else
      AfxMessageBox(str, MB_EXCLAME);
  } else {
    numPoints = numTotal = 0;
    CallDMIndGatan(DMind, mGatanCamera, GetDefectList(xyPairs, &pairSize, &numPoints, 
      &numTotal));
    if (numPoints < numTotal) {
      str.Format("WARNING: The array for getting defect points is not large enough:\n"
        "   it only fit %d of %d points", numPoints, numTotal);
      if (errToLog)
        mWinApp->AppendToLog(str);
      else
        AfxMessageBox(str, MB_EXCLAME);
    }
    if (IsCameraFaux()) {
      xyPairs[2 * numPoints] = GetTickCount() % 1024;
      xyPairs[2 * numPoints + 1] = GetTickCount() % 1967;
      numPoints++;
    }

    // Merge with the existing list; this encodes the rotation/flip value
    CorDefMergeDefectLists(param->defects, (unsigned short *)xyPairs, numPoints, 
      param->sizeX / 2, param->sizeY / 2, param->rotationFlip);
    CorDefFindTouchingPixels(param->defects, param->sizeX / 2, param->sizeY / 2, 0);

    // Scale the defects up and mark as K2
    if (!param->defects.wasScaled)
      CorDefScaleDefectsForK2(&param->defects, false);
    param->defects.K2Type = 1;
  }
  delete [] xyPairs;
}

// Restore Gatan camera orientations on exit if necessary
void CCameraController::RestoreGatanOrientations(void)
{
  int dmInd;
  CString mess;
  for (int ind = 0; ind < mWinApp->GetNumActiveCameras(); ind++) {
    CameraParameters *camP = &mAllParams[mActiveList[ind]];
    if (camP->GatanCam && camP->rotFlipToRestore >= 0) {
      dmInd = CAMP_DM_INDEX(camP);
      if (!mDMInitialized[dmInd] || 
        (dmInd == COM_IND && CreateCamera(CREATE_FOR_ANY, false)))
        continue;
      mess.Format("%s %d)\n"
        "CM_Config_SetDefaultTranspose(camera, %d)\n", sGetCamObjText, camP->cameraNumber, 
        camP->rotFlipToRestore);
      try {
        EasyRunScript(mess, 0, dmInd);
      }
      catch (_com_error E) {
      }
      if (dmInd == COM_IND)
        mGatanCamera->Release();
    }
  }
}

// Test if a camera is inserted, return 1 if it is, 0 not, or leave inserted unchanged
void CCameraController::TestCameraInserted(int actIndex, long &inserted)
{
  int camIndex = mActiveList[actIndex];
  CameraParameters *camP = &mAllParams[camIndex];
  if (camP->DMCamera) {
    int DMind = CAMP_DM_INDEX(camP);
    MainCallDMIndCamera(DMind, IsCameraInserted(camP->cameraNumber, &inserted));
  } else if (camP->DE_camType) {
    inserted = mTD.DE_Cam->IsCameraInserted();
  } else if (!camP->pluginName.IsEmpty()) {
    inserted = mPlugFuncs[camIndex]->IsCameraInserted(camP->cameraNumber);
  } else if (camP->FEItype && FCAM_ADVANCED(camP)) {
    inserted = mTD.scopePlugFuncs->ASIisCameraInserted(camP->eagleIndex);
  }
}

// Let's see if this is usable...
int CCameraController::RotateAndReplaceArray(int chan, int operation, int invertCon)
{
  short int *parray;
  int nxout, nyout;
  NewArray(parray, short int, mTD.DMSizeX * mTD.DMSizeY);
  if (!parray) {

    // If it fails, better swap the sizes or bad things happen
    if (operation % 2) {
      nxout = mTD.DMSizeY;
      mTD.DMSizeY = mTD.DMSizeX;
      mTD.DMSizeX = nxout;
    }
    return 1;
  }
  ProcRotateFlip((short *)mTD.Array[chan], mTD.ImageType, mTD.DMSizeX,
    mTD.DMSizeY, operation, invertCon, parray, &nxout, &nyout);
  delete [] mTD.Array[chan];
  mTD.Array[chan] = parray;
  mTD.DMSizeX = nxout;
  mTD.DMSizeY = nyout;
  return 0;
}

// Given current settings from the Set File Options dialog, make up the directory and 
// filename for saving the next set of frames
void CCameraController::ComposeFramePathAndName(bool temporary)
{
  CString date, time, path, filename, savefile, label;
  char numFormat[6];
  int trimCount;
  CMapDrawItem *item;
  B3DCLAMP(mDigitsForNumberedFrame, 3, 5);
  sprintf(numFormat, "%%0%dd", mDigitsForNumberedFrame);

  // Get common components
  if ((mFrameNameFormat & (FRAME_FOLDER_SAVEFILE | FRAME_FILE_SAVEFILE)) && 
    mWinApp->mStoreMRC) {
    savefile = mWinApp->mStoreMRC->getName();
    if (!savefile.IsEmpty()) {
      trimCount = savefile.GetLength() - (savefile.ReverseFind('\\') + 1);
      time = savefile.Right(trimCount);
      UtilSplitExtension(time, savefile, date);
    }
  }
  if ((mFrameNameFormat & (FRAME_FOLDER_NAVLABEL | FRAME_FILE_NAVLABEL)) && 
    mWinApp->mNavigator && (!(mFrameNameFormat & FRAME_LABEL_IF_ACQUIRE) || 
    mWinApp->mNavigator->GetAcquiring()) && 
    mWinApp->mNavigator->GetCurrentOrAcquireItem(item) >= 0)
    label = item->mLabel;

  // Set up folder
  if (!mParam->DE_camType || !mParam->DE_AutosaveDir.IsEmpty()) {
     if ((mFrameNameFormat & FRAME_FOLDER_ROOT) && !mFrameBaseName.IsEmpty())
      path = mFrameBaseName;
    if ((mFrameNameFormat & FRAME_FOLDER_SAVEFILE) && !savefile.IsEmpty())
      UtilAppendWithSeparator(path, savefile, "_");
    if ((mFrameNameFormat & FRAME_FOLDER_NAVLABEL) && !label.IsEmpty())
      UtilAppendWithSeparator(path, label, "_");

    // Ignore that path for advanced falcon if there is overall folder set, or use it
    if (IS_FALCON2_OR_3(mParam) && FCAM_ADVANCED(mParam)) {
      if (mDirForFalconFrames.IsEmpty())
        mFrameFolder = path;
      else
        mFrameFolder = mDirForFalconFrames;
    } else {

      // Otherwise append the folder.  For DE, FrameFolder is set up by caller
      if (!mParam->DE_camType)
        mFrameFolder = B3DCHOICE(IS_BASIC_FALCON2(mParam), mDirForFalconFrames,
          mDirForK2Frames);
      if (!path.IsEmpty())
        mFrameFolder += CString("\\") + path;
    }
  }

  // Set up prefix of filename if any
  if ((mFrameNameFormat & FRAME_FILE_ROOT) && !mFrameBaseName.IsEmpty())
    filename = mFrameBaseName;
  if ((mFrameNameFormat & FRAME_FILE_SAVEFILE) && !savefile.IsEmpty())
    UtilAppendWithSeparator(filename, savefile, "_");
  if ((mFrameNameFormat & FRAME_FILE_NAVLABEL) && !label.IsEmpty()) 
    UtilAppendWithSeparator(filename, label, "_");

  // If doing numbers, now evaluate whether the number needs to be reset and record
  // current prefix and filename
  // For temporary names (files to be deleted), leave off the number and add both 
  // date and time below
  if ((mFrameNameFormat & FRAME_FILE_NUMBER) && !temporary) {
    if (mLastUsedFrameNumber < mFrameNumberStart || mNumberedFrameFolder != mFrameFolder
      || mNumberedFramePrefix != filename)
      mLastUsedFrameNumber = mFrameNumberStart - 1;
    mNumberedFrameFolder = mFrameFolder;
    mNumberedFramePrefix = filename;
    mLastUsedFrameNumber++;
    date.Format(numFormat, mLastUsedFrameNumber);
    UtilAppendWithSeparator(filename, date, "_");
    mLastFrameNumberStart = mFrameNumberStart;
  }

  // Get final components
  if (mFrameNameFormat & FRAME_FILE_TILT_ANGLE) {
    date.Format("%.1f", mTiltBefore);
    if (mParam->DE_camType && mTD.DE_Cam->GetServerVersion() < DE_SUFFIX_KEEPS_PT)
      date.Replace('.', 'p');
    UtilAppendWithSeparator(filename, date, "_");
  }
  if (!mParam->DE_camType) {
    if ((mFrameNameFormat & (FRAME_FILE_MONTHDAY | FRAME_FILE_HOUR_MIN_SEC)) || temporary)
      mWinApp->mDocWnd->DateTimeComponents(date, time);
    if ((mFrameNameFormat & FRAME_FILE_MONTHDAY) || temporary)
      UtilAppendWithSeparator(filename, date, "_");
    if ((mFrameNameFormat & FRAME_FILE_HOUR_MIN_SEC) || temporary)
      UtilAppendWithSeparator(filename, time, "_");
  }
  mFrameFilename = filename;
}

// Compose the complete path of a DM reference with the given suffix to replace two
// "extensions" in the standard DM reference name.
CString CCameraController::MakeFullDMRefName(CameraParameters *camP, const char *mode)
{
  CString ref, root, ext;
  if (!camP->DMRefName.IsEmpty()) {
    ref = mWinApp->mGainRefMaker->GetDMRefPath();
    ext = mWinApp->mGainRefMaker->GetRemoteRefPath();
    if (camP->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID) && !ext.IsEmpty()) 
      ref = ext;
    if (!ref.IsEmpty()) {
      ref += "\\" + camP->DMRefName;
      for (int ind = 0; ind < 4; ind++) {
        ext.Format(".m%d.", ind);
        if (ref.Find(ext) > 0) {
          ref.Replace(ext, mode);
          break;
        }
      }
    }
  }
  return ref;
}

// Returns true if all conditions are set for frame-saving from K2 or permanent 
// frame-saving from Falcon or DE based on camera and control set
bool CCameraController::IsConSetSaving(ControlSet *conSet, int setNum,
  CameraParameters *param, bool K2only)
{
  bool falconCanSave = (IS_BASIC_FALCON2(param) && GetMaxFalconFrames(param) && 
    mFrameSavingEnabled) || 
    (IS_FALCON2_OR_3(param) && (param->CamFlags & PLUGFEI_CAN_DOSE_FRAC));
  bool weCanAlignFalcon = falconCanSave && 
    mWinApp->mScope->GetPluginVersion() >= PLUGFEI_ALLOWS_ALIGN_HERE &&
    !(param->FEItype == FALCON3_TYPE && mLocalFalconFramePath.IsEmpty());
  BOOL DEcanSave = mWinApp->mDEToolDlg.CanSaveFrames(param);
  bool weCanAlignDE = DEcanSave && (param->CamFlags & DE_WE_CAN_ALIGN);

  // Align is turned off during TS/script with these set, but need it for Record
  bool alignAnyway = setNum == RECORD_CONSET && 
    (mWinApp->mTSController->GetFrameAlignInIMOD() ||
    mWinApp->mMacroProcessor->GetAlignWholeTSOnly());

  return (((param->K2Type && conSet->doseFrac) ||   // Saving possible for camera type
           ((falconCanSave || DEcanSave) && !K2only)) &&
          (((!DEcanSave && conSet->saveFrames) ||   // Ordinary saving selected
            (DEcanSave && (conSet->saveFrames & DE_SAVE_MASTER))) ||
           (conSet->useFrameAlign > 1 &&            // Or forced saving is called for
            ((param->K2Type && CAN_PLUGIN_DO(CAN_ALIGN_FRAMES, param)) || 
             (!K2only && (weCanAlignFalcon || weCanAlignDE))) &&
            (conSet->alignFrames || alignAnyway)))); // and align should be done
}

// One-time management of directory for Falcon frames
void CCameraController::FixDirForFalconFrames(CameraParameters *param)
{
  if (!IS_FALCON2_OR_3(param))
    return;
  if (FCAM_ADVANCED(param)) {

    // For advanced scripting, if it is not set yet, the default is blank; if
    // it somehow has a path in it, set it to blank
    if (mDirForFalconFrames == FALCON_DIR_UNSET)
      mDirForFalconFrames = "";
    int slashInd = mDirForFalconFrames.FindOneOf("/\\");
    if (mDirForFalconFrames.Find(':') >= 0 || 
      (slashInd >= 0 && slashInd < mDirForFalconFrames.GetLength() - 1))
      mDirForFalconFrames = "";
  } else if (mDirForFalconFrames == FALCON_DIR_UNSET) {

    // For old scripting the default is to inherit from K2 path
    mDirForFalconFrames = mDirForK2Frames;
  }    
}

////////////////////////////////////////////////////////////////////
// STEM Routines
////////////////////////////////////////////////////////////////////

// Get the pixel time and possibly revise the exposure time 
void CCameraController::ComputePixelTime(CameraParameters *camParams, int sizeX, 
                                         int sizeY, int lineSync, float pixelSize, 
                                         float maxScanRate, float &exposure, 
                                         double &pixelTime, double &scanRate)
{
  double cycle;
  int nmult;
  float minPixel = camParams->minPixelTime;
  float flyback = camParams->flyback;
  double increment = camParams->pixelTimeIncrement;

  // If minimum scan rate and a pixel size are defined, make sure the exposure is long
  // enough
  if (pixelSize > 0) {
    scanRate = 1.e-3 * sizeX * sizeY * pixelSize / exposure;
    if (maxScanRate > 0 && scanRate > maxScanRate)
      exposure = (float)((1.e-3 * sizeX * sizeY * pixelSize) / maxScanRate);
  }

  // Get the pixel time then adjust if line sync is on or if there is an increment
  pixelTime = exposure * 1.e6 / (sizeX * sizeY);
  pixelTime = B3DMAX(minPixel, pixelTime);
  if (camParams->maxPixelTime > 0.)
    pixelTime = B3DMIN(camParams->maxPixelTime, pixelTime);
  if (increment > 0.)
    pixelTime = minPixel + increment * B3DNINT((pixelTime - minPixel) / increment);
  if (lineSync) {
    cycle = 1.e6 / mDSlineFreq;
    nmult = B3DNINT((pixelTime * sizeX + flyback + mDSsyncMargin) / cycle);
    pixelTime = (cycle * nmult - flyback - mDSsyncMargin) / sizeX;
    while (pixelTime < minPixel) {
      nmult++;
      pixelTime = (cycle * nmult - flyback - mDSsyncMargin) / sizeX;
    }
  }

  // Recompute exposure and 
  exposure = (float)(pixelTime * sizeX * sizeY / 1.e6);
  if (pixelSize > 0)
    scanRate = 1.e-3 * sizeX * sizeY * pixelSize / exposure;
 }

// Return the maximum channels at once
int CCameraController::GetMaxChannels(CameraParameters * param)
{
  int comboSize;
  int list[MAX_CHANNELS];
  if (param->maxChannels)
    return param->maxChannels;
  if (!mChannelSets.GetSize() || param->numChannels < 2) {
    param->maxChannels = param->numChannels;
    return param->maxChannels;
  }

  // Loop on increasing combination size and call function that recursively tests
  // ALL combination of that size.  The last one to return that one is OK sets the size
  param->maxChannels = 1;
  for (comboSize = 2; comboSize <= param->numChannels; comboSize++) {
    mFoundCombo = false;
    BuildEvalChannelList(param->numChannels, comboSize, 0, list);
    if (!mFoundCombo)
      break;
    param->maxChannels = comboSize;
  }
  return param->maxChannels;
}

// Recursive function to build a list of channel numbers and evaluate it when it is
// big enough
void CCameraController::BuildEvalChannelList(int numChan, int comboSize, int ninList, 
                                             int *list)
{
  int i, j, k;
  for (i = 0; i < numChan && !mFoundCombo; i++) {
    if (!numberInList(i, list, ninList, 0)) {
      list[ninList] = i;
      if (ninList + 1 < comboSize) {
        BuildEvalChannelList(numChan, comboSize, ninList + 1, list);
      } else {
        mFoundCombo = true;
        for (j = 0; j < ninList && mFoundCombo; j++)
          for (k = j + 1; k <= ninList && mFoundCombo; k++) 
            if (MutuallyExclusiveChannels(list[j], list[k]))
              mFoundCombo = false;
      }
    }
  }
}

// Return true if two channels are mutually exclusive or the same number
bool CCameraController::MutuallyExclusiveChannels(int chan1, int chan2)
{
  if (chan1 == chan2)
    return true;
  for (int set = 0; set < mChannelSets.GetSize(); set++)
    if (numberInList(chan1, &mChannelSets[set].channels[0], mChannelSets[set].numChans, 
      0) &&
      numberInList(chan2, &mChannelSets[set].channels[0], mChannelSets[set].numChans, 0))
      return true;
  return false;
}

// Return the DS channel that a detector is mapped to, if any
int CCameraController::ChannelMappedTo(int chan) 
{
  for (int set = 0; set < mChannelSets.GetSize(); set++)
    if (numberInList(chan, &mChannelSets[set].channels[0], mChannelSets[set].numChans, 0)
      && mChannelSets[set].mappedTo >= 0)
      return mChannelSets[set].mappedTo;
  return chan;
}

// Stop continuous STEM OR continuous regular camera
void CCameraController::StopContinuousSTEM(void)
{
  HRESULT hr;
  mContinuousDelayFrac = 0.;
  if (!mTD.ContinuousSTEM && !(mTD.ProcessingPlus & CONTINUOUS_USE_THREAD))
    return;
  if (mScope->GetCameraAcquiring())
    mScope->SetCameraAcquiring(false);

  // Plugin camera
  if (mTD.plugFuncs && mTD.plugFuncs->StopContinuous) {
    mTD.plugFuncs->StopContinuous();
    return;
  }

  // DE camera
  if (mParam->DE_camType >= 2) {
    mTD.DE_Cam->SetLiveMode(0);
    return;
  }

  // Gatan camera
  CreateDMCamera(mGatanCamera);
  if (FAILED(hr))
    return;
  try {
    if (mTD.ContinuousSTEM) {
      CallGatanCamera(mGatanCamera, StopDSAcquisition());
      mTD.ContinuousSTEM = 0;
    } else {
      CallGatanCamera(mGatanCamera, StopContinuousCamera());
      mTD.ProcessingPlus = mTD.Processing;
    }
  }
  catch (_com_error E) {
  }
  ReleaseDMCamera(mGatanCamera);
}

void CCameraController::TurnOffRepeatFlag(void)
{
  if (mRepFlag >= 0) {
    mStoppedContinuous = true;
    mWinApp->SetStatusText(B3DCHOICE(mWinApp->mProcessImage->GetLiveFFT(), COMPLEX_PANE, 
    MEDIUM_PANE), "");
  }
  mRepFlag = -1;
  mContinuousDelayFrac = 0.;
}

void CCameraController::OutOfSTEMUpdate(void)
{
  for (int i = 0; i < MAX_STEM_CHANNELS; i++)
    mFEIdetectorInserted[i] = false;
}

void CCameraController::CountSimultaneousChannels(CameraParameters *camParams, 
                                                  int *simultaneous, int maxSimul, 
                                                  int & numSimul, int & numAvail)
{
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  int maxChan = mWinApp->mCamera->GetMaxChannels(camParams);
  maxChan = B3DMIN(maxSimul, maxChan);
  int i, ind;
  numSimul = 0;
  numAvail = 0;
  for (i = 0; i < camParams->numChannels; i++) {
    if (camParams->availableChan[i])
      numAvail++;
    ind = conSet->channelIndex[i];
    if (ind >= 0 && camParams->availableChan[ind] && numSimul < maxChan)
      simultaneous[numSimul++] = ind + 1;
  }
}

void CCameraController::AdjustForShift(float adjustX, float adjustY)
{
  mAdjustShiftX = adjustX;
  mAdjustShiftY = adjustY;
}
