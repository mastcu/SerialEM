// SerialEM.cpp          The main module, has top level routines and
//                          miscellaneous message handlers
//
// Copyright (C) 2003-2019 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include <atlbase.h>
#include <atlcom.h>
#include ".\SerialEM.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ParameterIO.h"
#include "CameraSetupDlg.h"
#include "FileOptions.h"
#include "Image\KStoreMRC.h"
#include "Image\KStoreADOC.h"
#include "Utilities\KGetOne.h"
#include "EMbufferManager.h"
#include "EMscope.h"
#include "CameraController.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "FocusManager.h"
#include "EMmontageController.h"
#include "LogWindow.h"
#include "MacroEditer.h"
#include "MacroProcessor.h"
#include "MacroToolbar.h"
#include "MacroControlDlg.h"
#include "OneLineScript.h"
#include "ComplexTasks.h"
#include "BeamAssessor.h"
#include "ProcessImage.h"
#include "TSController.h"
#include "FilterTasks.h"
#include "DistortionTasks.h"
#include "GainRefMaker.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "NavRotAlignDlg.h"
#include "MultiShotDlg.h"
#include "AutocenSetupDlg.h"
#include "CalibCameraTiming.h"
#include "MultiTSTasks.h"
#include "ParticleTasks.h"
#include "TSViewRange.h"
#include "ReadFileDlg.h"
#include "Mailer.h"
#include "GatanSocket.h"
#include "PluginManager.h"
#include "StateDlg.h"
#include "FalconHelper.h"
#include "StageMoveTool.h"
#include "PiezoAndPPControl.h"
#include "ScreenShotDialog.h"
#include "HoleFinderDlg.h"
#include "MultiCombinerDlg.h"
#include "AutoTuning.h"
#include "ExternalTools.h"
#include "Shared\b3dutil.h"
#include "XFolderDialog/XWinVer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define VERSION_STRING  "SerialEM Version 3.9.0beta"
#define TAG_STRING      "(Tagged SEM_3-8-6, 12/31/20)"

// Offsets for static window inside main frame
#define STATIC_BORDER_TOP      0
#define STATIC_BORDER_LEFT   150
#define STATIC_BORDER_BOTTOM  0
#define STATIC_BORDER_RIGHT   0

// Camera-specific constants
#define CAMERA_NO_DEFAULT  -1
#define CAMERA_MODULO_X 4
#define CAMERA_MODULO_Y 1
#define BUILT_IN_SETTLING  0.25f
#define BEAM_SHUTTER 1
#define INSERT_DELAY 5.0f
#define CAMERA_START_DELAY 0.15f
#define EXTRA_UNBLANK_TIME 0.012f
#define EXTRA_BEAM_TIME 0.075f
#define MINIMUM_DRIFT  0.075f
#define MIN_BLANKED_EXPOSURE 0.3f
#define EXTRA_OPEN_SHUTTER_TIME 0.12f

static LONG WINAPI SEMExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo);
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

// Initial state of tool dialogs
static int  initialDlgState[MAX_TOOL_DLGS] = 
  {3, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Tool dialog colors
static COLORREF dlgBorderColors[] =
{RGB(255,0,0), RGB(0,0,170), RGB(0,255,0), RGB(255,128,0), RGB(255,183,120), 
RGB(0,128,255), RGB(0,140,60), RGB(255,255,0), RGB(255,0,255), RGB(128,64,64), 
RGB(0,255,255), RGB(127,0,192), RGB(150,160,0), RGB(255,170,255), RGB(75,75,0), 
RGB(0,0,0), RGB(255,255,255)};

static UINT sToolDlgIDs[] = 
{IDD_BUFFERSTATUS, IDD_BUFFERCONTROL, IDD_IMAGELEVEL, IDD_SCOPESTATUS, IDD_REMOTE_CONTROL,
IDD_TILTCONTROL, IDD_CAMERA_MACRO, IDD_ALIGNFOCUS, IDD_LOWDOSE, IDD_MONTAGECONTROL,
IDD_STEMCONTROL, IDD_FILTERCONTROL, IDD_DETOOLDLG};

// Static variable for com and other errors to be reported in, watched by OnIdle
static int sThreadError = 0;

// A mutex and static data for trace output to log window
static HANDLE traceMutexHandle;
#define TRACE_MUTEX_WAIT  1000
#define MAX_TRACE_LIST 20
CString sTraceMsg[MAX_TRACE_LIST];
static int sNumTraceMsg = 0;
static double sStartTime;
static CString debugOutput = "";
static DWORD appThreadID;
static CString sBuildDate;
static CString sBuildTime;
static int sBuildDayStamp;
static CString sAboutVersion;
static CString sFunctionCalled = "";
static bool sIgnoreFunctionCalled = false;

CComModule _Module;

/////////////////////////////////////////////////////////////////////////////
// CSerialEMApp

BEGIN_MESSAGE_MAP(CSerialEMApp, CWinApp)
  //{{AFX_MSG_MAP(CSerialEMApp)
  ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_UPDATE_COMMAND_UI(ID_CAMERA_PARAMETERS, OnUpdateCameraParameters)
  ON_COMMAND(ID_CAMERA_SIMULATION, OnCameraSimulation)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SIMULATION, OnUpdateCameraSimulation)
  ON_COMMAND(ID_FILE_OPENLOG, OnFileOpenlog)
  ON_UPDATE_COMMAND_UI(ID_FILE_OPENLOG, OnUpdateFileOpenlog)
  ON_COMMAND(ID_FILE_SAVELOG, OnFileSavelog)
  ON_UPDATE_COMMAND_UI(ID_FILE_SAVELOG, OnUpdateFileSavelog)
  ON_COMMAND(ID_CALIBRATION_ADMINISTRATOR, OnCalibrationAdministrator)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_ADMINISTRATOR, OnUpdateCalibrationAdministrator)
  ON_COMMAND(ID_FILE_SAVELOGAS, OnFileSavelogas)
  ON_BN_CLICKED(ID_HELP, OnHelp)
  ON_COMMAND(ID_HELP_USING, OnHelpUsing)
  ON_COMMAND(ID_RESIZE_MAIN, OnResizeMain)
  ON_COMMAND(ID_CAMERA_PARAMETERS, OnCameraParameters)
  ON_UPDATE_COMMAND_UI(ID_FILE_SAVELOGAS, OnUpdateFileSavelog)
	ON_COMMAND(ID_FILE_READAPPEND, OnFileReadappend)
	//}}AFX_MSG_MAP
  // Standard file based document commands
  ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
  ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
  ON_COMMAND( ID_HELP_INDEX, OnHelpIndex )
  ON_COMMAND(ID_FILE_CONTINUOUSSAVE, OnFileContinuousSave)
  ON_UPDATE_COMMAND_UI(ID_FILE_CONTINUOUSSAVE, OnUpdateFileContinuousSave)
  ON_COMMAND(ID_WINDOW_SHOWSCOPECONTROLPANEL, OnShowScopeControlPanel)
  ON_UPDATE_COMMAND_UI(ID_WINDOW_SHOWSCOPECONTROLPANEL, OnUpdateShowScopeControlPanel)
  ON_COMMAND(ID_WINDOW_RESCUELOGWINDOW, OnWindowRescuelogwindow)
  ON_UPDATE_COMMAND_UI(ID_WINDOW_RESCUELOGWINDOW, OnUpdateWindowRescuelogwindow)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSerialEMApp construction

CSerialEMApp::CSerialEMApp()
{
  // Place all significant initialization in InitInstance
  int i, j, k;
	EnableHtmlHelp();
  mBufferManager = NULL;
  mMacroProcessor = NULL;
  mShiftManager = NULL;
  mShiftCalibrator = NULL;
  mMontageController = NULL;
  mFocusManager = NULL;
  mComplexTasks = NULL;
  mBeamAssessor = NULL;
  mProcessImage = NULL;
  mTSController = NULL;
  mFilterTasks = NULL;
  mDistortionTasks = NULL;
  mMailer = NULL;
  mGatanSocket = NULL;
  mPluginManager = NULL;
  mFalconHelper = NULL;
  mPiezoControl = NULL;
  mParamIO = NULL;
  m_pMainWnd = NULL;
  mNeedStaticWindow = true;
  mNeedFFTWindow = false;
  mMainView = NULL;
  mStackViewImBuf = NULL;
  mViewOpening = false;
  mStackView = NULL;
  mFFTView = NULL;
  mFFTbufIndex = 0;
  mViewClosing = false;
  mMontaging = false;
  mStoreMRC = NULL;
  mScope = NULL;
  mSavingOther = false;
  mLogWindow = NULL;
  mContinuousSaveLog = false;
  mNavigator = NULL;
  mNavHelper = NULL;
  mStageMoveTool = NULL;
  mScreenShotDialog = NULL;
  mAdministrator = false;
  mAdministratorMode = 0;
  mShiftScriptOnlyInAdmin = false;
  mCalNotSaved = false;
  mEFTEMMode = false;
  mSTEMMode = false;
  mAppExiting = false;
  for (i = 0; i < MAX_MACROS; i++) {
    mMacroEditer[i] = NULL;
    mMacroSaveFile[i] = "";
    mMacroFileName[i] = "";
    mMacroName[i] = "";
    mLongMacroName[i] = "";
  }
  mMacroToolbar = NULL;

  // Settings to be overridden by settings file
  for (i = 0; i < MAX_CONSETS; i++) {
    mModeName[i] = "";
    mCamConSets[0][i].right = 0;
  }
  mModeName[0] = "View";
  mModeName[1] = "Focus";
  mModeName[2] = "Trial";
  mModeName[3] = "Record";
  mModeName[4] = "Preview";
  mModeName[5] = "Search";
  mModeName[6] = "Mont-map";
  mModeName[7] = "Track";
  
  mPctLo = 0.1f;
  mPctHi = 0.1f;
  mAreaFrac = 0.8f;
  mTruncDiamOfFFT = 0.004f;
  mBkgdGrayOfFFT = 32;
  mCurrentCamera = 0;
  mCurrentActiveCamera = 0;
  mActiveCameraList[0] = 0;
  mActiveCamListSize = 1;
  mNumAvailCameras = 1;
  mLastCamera = 0;
  mAssumeCamForDummy = -1;
  mInitialCurrentCamera = -1;
  mNonGIFMatchPixel = false;
  mNonGIFMatchIntensity = false;
  mSTEMmatchPixel = true;
  mProcessHere = true;
  for (i = 0; i < MAX_CAMERAS; i++) {
    mCamParams[i].sizeX = CAMERA_NO_DEFAULT;
    mCamParams[i].sizeY = CAMERA_NO_DEFAULT;
    mCamParams[i].unsignedImages = FALSE;
    mCamParams[i].returnsFloats = false;
    mCamParams[i].moduloX = CAMERA_MODULO_X;
    mCamParams[i].moduloY = CAMERA_MODULO_Y;
    mCamParams[i].coordsModulo = false;
    mCamParams[i].squareSubareas = false;
    mCamParams[i].refSizeEvenX = 0;
    mCamParams[i].refSizeEvenY = 0;
    mCamParams[i].leftOffset = 0;
    mCamParams[i].topOffset = 0;
    mCamParams[i].fourPort = false;     // Wasn't initialized
    mCamParams[i].TietzType = 0;
    mCamParams[i].failedToInitialize = false;
    mCamParams[i].AMTtype = 0;
    mCamParams[i].FEItype = 0;
    mCamParams[i].CamFlags = 0;
    mCamParams[i].DE_camType = 0;      // Wasn't init
    mCamParams[i].DE_ImageInvertX = 0;
    mCamParams[i].DE_ImageRot = 0;
    mCamParams[i].DE_FramesPerSec = -1.;
    mCamParams[i].DE_CountingFPS = -1.;
    mCamParams[i].DE_MaxFrameRate = -1.;
    mCamParams[i].alsoInsertCamera = -1;
    mCamParams[i].samePhysicalCamera = -1;
    mCamParams[i].K2Type = 0;
    mCamParams[i].useSocket = false;
    mCamParams[i].STEMcamera = false;
    mCamParams[i].beamShutter = BEAM_SHUTTER;
    mCamParams[i].setAltShutterNC = false;  // Wasn't init
    mCamParams[i].onlyOneShutter = 0;
    mCamParams[i].TietzCanPreExpose = false;
    mCamParams[i].restoreBBmode = -1;
    mCamParams[i].TietzGainIndex = 1;
    mCamParams[i].LowestGainIndex = -1;
    mCamParams[i].TietzImageGeometry = -1;  // So even a property 0 will be sent out
    mCamParams[i].TietzBlocks = 0;
    mCamParams[i].dropFrameFlags = 0;
    mCamParams[i].cropFullSizeImages = -1;
    mCamParams[i].builtInSettling = BUILT_IN_SETTLING;
    mCamParams[i].pixelMicrons = CAMERA_NO_DEFAULT;
    mCamParams[i].magRatio = 1.25f;
    mCamParams[i].extraRotation = 0;
    mCamParams[i].numBinnings = 1;
    mCamParams[i].binnings[0] = 1;
    for (j = 0; j < MAX_BINNINGS; j++) {
      mCamParams[i].gainFactor[j] = 1.;
      mCamParams[i].halfSizeX[j] = 0;
      mCamParams[i].halfSizeY[j] = 0;
      mCamParams[i].quarterSizeX[j] = 0;
      mCamParams[i].quarterSizeY[j] = 0;
      mCamParams[i].minFrameTime[j] = 0.;
      mCamParams[i].frameTimeDivisor[j] = 0.;
    }
    mCamParams[i].autoGainAtBinning = 0;
    mCamParams[i].numExtraGainRefs = 0;
    mCamParams[i].processHere = -1;
    mCamParams[i].retractable = TRUE;
    mCamParams[i].insertDelay = INSERT_DELAY;
    mCamParams[i].retractDelay = INSERT_DELAY;
    mCamParams[i].order = i;
    mCamParams[i].insertingRetracts = -1;
    mCamParams[i].name.Format("Default Camera %d", i + 1);
    mCamParams[i].GIF = FALSE;
    mCamParams[i].hasTVCamera = FALSE;
    mCamParams[i].useTVToUnblank = 0;
    mCamParams[i].checkTemperature = false;
    mCamParams[i].sideMount = false;
    mCamParams[i].canTakeFrames = -1;
    mCamParams[i].useGPUforAlign[0] = mCamParams[i].useGPUforAlign[1] = false;
    mCamParams[i].startupDelay = CAMERA_START_DELAY;
    mCamParams[i].startDelayPerFrame = -0.0001f;
    mCamParams[i].DMbeamShutterOK = TRUE;
    mCamParams[i].DMsettlingOK = TRUE;
    mCamParams[i].DMopenShutterOK = TRUE;
    mCamParams[i].extraUnblankTime = EXTRA_UNBLANK_TIME;
    mCamParams[i].extraBeamTime = EXTRA_BEAM_TIME;
    mCamParams[i].minimumDrift = MINIMUM_DRIFT;
    mCamParams[i].minBlankedExposure = MIN_BLANKED_EXPOSURE;
    mCamParams[i].extraOpenShutterTime = EXTRA_OPEN_SHUTTER_TIME;
    mCamParams[i].deadTime = 0.;
    mCamParams[i].minExposure = 0.001f;
    mCamParams[i].postBlankerDelay = 0.;
    mCamParams[i].defects.wasScaled = 0;
    mCamParams[i].defects.K2Type = 0;
    mCamParams[i].defects.rotationFlip = 0;
    mCamParams[i].defects.usableTop = 0;
    mCamParams[i].defects.usableLeft = 0;
    mCamParams[i].defects.usableBottom = 0;
    mCamParams[i].defects.usableRight = 0;
    mCamParams[i].numHotColumns = 0;
    mCamParams[i].maxXRayDiameter = 3;
    mCamParams[i].hotPixImodCoord = 0;
    mCamParams[i].darkXRayAbsCrit = 0.;
    mCamParams[i].darkXRayNumSDCrit = 0.;
    mCamParams[i].darkXRayBothCrit = 0;
    mCamParams[i].imageXRayAbsCrit = 200.;
    mCamParams[i].imageXRayNumSDCrit = 15.;
    mCamParams[i].imageXRayBothCrit = 1;
    mCamParams[i].showImageXRayBox = 0;
    mCamParams[i].gainRefFrames = 10;
    mCamParams[i].gainRefTarget = 4000;
    mCamParams[i].gainRefBinning = 0;    // This is OK, handled in dialog
    mCamParams[i].gainRefAverageDark = false;
    mCamParams[i].gainRefNumDarkAvg = 5;
    mCamParams[i].TSAverageDark = false;
    mCamParams[i].numBinnedOffsets = 0;
    mCamParams[i].useMinDDDBinning = false;
    mCamParams[i].countsPerElectron = 0.;
    mCamParams[i].corrections = -1;
    mCamParams[i].sizeCheckSwapped = 0;
    mCamParams[i].subareasBad = 0;
    mCamParams[i].centeredOnly = 0;    // Wasn't init
    mCamParams[i].matchFactor = 1.f;
    mCamParams[i].useContinuousMode = -1;
    mCamParams[i].useFastAcquireObject = true;
    mCamParams[i].continuousQuality = 0;
    mCamParams[i].setContinuousReadout = false;
    mCamParams[i].balanceHalves = -1;
    mCamParams[i].ifHorizontalBoundary = -1;
    mCamParams[i].halfBoundary = -1;
    mCamParams[i].shutterLabel1 = "";
    mCamParams[i].shutterLabel2 = "";
    mCamParams[i].shutterLabel3 = "";
    mCamParams[i].cameraNumber = 0;
    mCamParams[i].pluginName = "";
    mCamParams[i].noShutter = false;
    mCamParams[i].pluginCanProcess = 0;
    mCamParams[i].canPreExpose = false;
    mCamParams[i].rotationFlip = 0;
    mCamParams[i].imageRotation = 0;
    mCamParams[i].DMrotationFlip = -1;
    mCamParams[i].setRestoreRotFlip = 2;
    mCamParams[i].rotFlipToRestore = -1;
    mCamParams[i].postActionsOK = -1;     // -1 = Unspecified and OK
    mCamParams[i].addToExposure = -999.;
    mCamParams[i].falcon3ScalePower = 4;
    mCamParams[i].linear2CountingRatio = 8.;
    mCamParams[i].linearOffset = 0.;
    mCamParams[i].K3CDSLinearRatio = 1.5f;
    mCamParams[i].origDefects = NULL;
    mCamParams[i].defNameHasFrameFile = false;
    mCamParams[i].taskTargetSize = 512;
    mCamParams[i].specToCamDoseFac = 0.;
    mCamParams[i].invertFocusRamp = false;
    mCamParams[i].numChannels = 0;
    mCamParams[i].JeolDetectorID = -1;
    for (j = 0; j < MAX_STEM_CHANNELS; j++) {
      mCamParams[i].availableChan[j] = true;
      mCamParams[i].needShotToInsert[j] = false;
    }
    for (k = 0; k < NUMBER_OF_USER_CONSETS; k++) {
      // NEED TO SPLIT OFF NON SIZE_DEPENDENT ITEMS IN PARAMIO AND CALL HERE
      mCamConSets[i][k].useFrameAlign = 1;
      mCamConSets[i][k].faParamSetInd = 0;
      mCamConSets[i][k].lineSync = 0;
      mCamConSets[i][k].dynamicFocus = 0;
      for (j = 0; j < MAX_STEM_CHANNELS; j++)
        mCamConSets[i][k].channelIndex[j] = -1;
      mCamConSets[i][k].channelIndex[0] = 0;
      mCamConSets[i][k].boostMag = 0;
      mCamConSets[i][k].magAllShots = 0;
      mCamConSets[i][k].integration = 1;
      mCamConSets[i][k].correctDrift = -1;
    }
    mCamParams[i].minPixelTime = 0.1f;
    mCamParams[i].maxPixelTime = 0.;
    mCamParams[i].pixelTimeIncrement = 0.;
    mCamParams[i].maxIntegration = 0;
    mCamParams[i].maxScanRate = 0.f;
    mCamParams[i].advisableScanRate = 0.f;
    mCamParams[i].basicFlyback = 50.;
    mCamParams[i].addedFlyback = 0.;
    mCamParams[i].subareaInCorner = false;
  }

  mMontParam.adjustFocus = false;
  mMontParam.focusAfterStage = false;
  mMontParam.repeatFocus = false;
  mMontParam.driftLimit = 2.f;
  mMontParam.correctDrift = true;
  mMontParam.shiftInOverview = false;
  mMontParam.verySloppy = false;
  mMontParam.overviewBinning = 1;
  mMontParam.prescanBinning = 1;
  mMontParam.binning = 1;
  mMontParam.maxOverviewBin = 8;
  mMontParam.maxPrescanBin = 1;
  mMontParam.insideNavItem = -1;
  mMontParam.skipOutsidePoly = false;
  mMontParam.skipCorrelations = false;
  mMontParam.skipRecReblanks = false;
  mMontParam.wasFitToPolygon = false;
  mMontParam.focusBlockSize = 3;
  mMontParam.focusInBlocks = false;
  mMontParam.hqDelayTime = 5.f;
  mMontParam.useHqParams = false;
  mMontParam.minOverlapFactor = 0.1f;
  mMontParam.minMicronsOverlap = 0.5f;
  mMontParam.realignInterval = 64;
  mMontParam.realignToPiece = false;
  mMontParam.maxAlignFrac = 0.25f;
  mMontParam.ISrealign = false;
  mMontParam.maxRealignIS = 10.;
  mMontParam.useAnchorWithIS = false;
  mMontParam.anchorMagInd = -1;
  mMontParam.setupInLowDose = false;
  mMontParam.forFullMontage = false;
  mMontParam.useViewInLowDose = false;
  mMontParam.useSearchInLowDose = false;
  mMontParam.useContinuousMode = false;
  mMontParam.continDelayFactor = 0.5f;
  mMontParam.noDriftCorr = true;
  mMontParam.noHQDriftCorr = false;
  mSelectedConSet = 3;
  mActPostExposure = true;
  mSmallFontsBad = false;
  mDisplayNotTruly120DPI = false;
  mToolTitleHeight = mToolExtraWidth = mToolExtraHeight = 0;
  mFilterParams.autoCamera = true;
  mFilterParams.autoMag = false;
  mFilterParams.energyLoss = 0.;
  mFilterParams.zeroLoss = false;
  mFilterParams.slitIn = false;
  mFilterParams.slitWidth = 20.;
  mFilterParams.firstGIFCamera = -1;
  mFilterParams.firstRegularCamera = -1;
  mFilterParams.matchPixel = true;
  mFilterParams.matchIntensity = true;
  mFilterParams.binFactor = 1.;
  mFilterParams.alignedMagInd = 0;
  mFilterParams.alignedSlitWidth = 0.;
  mFilterParams.doMagShifts = true;
  mFilterParams.currentMagShift = 0.;
  mFilterParams.alignZLPTimeStamp = 0.;
  mFilterParams.refineZLPOffset = 0.;
  mFilterParams.cumulNonRunTime = 0;
  mFilterParams.usedOldAlign = false;
  mRetractOnEFTEM = false;
  mStartupInfo = true;

  // Initialize Mag table
  for (i = 0; i < MAX_MAGS; i++) {
    mFilterParams.magShifts[i] = -999.;
    mMagTab[i].mag = 0;
    mMagTab[i].STEMmag = 0.;
    mMagTab[i].tecnaiRotation = 0.;
    mMagTab[i].offsetISX = 0.;
    mMagTab[i].offsetISY = 0.;
    mMagTab[i].focusTicks = 1;
    for (j = 0; j < 2; j++) {
      mMagTab[i].calOffsetISX[j] = mMagTab[i].calOffsetISY[j] = 0.;
      mMagTab[i].neutralISX[j] = 0.;
      mMagTab[i].neutralISY[j] = 0.;
    }
    for (j = 0; j < MAX_CAMERAS; j++) {
      mMagTab[i].deltaRotation[j] = 999.;
      mMagTab[i].rotation[j] = 999.;
      mMagTab[i].rotDerived[j] = 0;
      mMagTab[i].pixelSize[j] = 0.;
      mMagTab[i].pixDerived[j] = 0;
      mMagTab[i].matIS[j].xpx = 0.;
      mMagTab[i].matIS[j].ypx = 0.;
      mMagTab[i].calibrated[j] = 0;
      mMagTab[i].matStage[j].xpx = 0.;
      mMagTab[i].matStage[j].ypx = 0.;
      mMagTab[i].stageCalibrated[j] = 0;
      mMagTab[i].stageCalFocus[j] = -999.;
    }
  }
  for (i = 0; i < MAX_CAMLENS; i++) {
    mCamLengths[i] = 0;
    mCamLenCalibrated[i] = 0.;
  }

  mIdleArray.SetSize(0,5);
  mCheckThreshold = 1000;
  mMemoryLimit = 0.;

  // Set macro controls
  mMacControl.limitIS = false;
  mMacControl.limitTiltUp = false;
  mMacControl.limitTiltDown = false;
  mMacControl.limitMontError = false;
  mMacControl.limitRuns = false;
  mMacControl.limitScaleMax = false;
  mMacControl.ISlimit = 10.0;
  mMacControl.runLimit = 500;
  mMacControl.tiltUpLimit = 80.;
  mMacControl.tiltDownLimit = -80.;
  mMacControl.scaleMaxLimit = 50;
  mMacControl.montErrorLimit = 1.0;

  InitializeLDParams();
  mNavParams.macroIndex = 1;
  mNavParams.preMacroInd = 1;
  mNavParams.preMacroIndNonTS = 1;
  mNavParams.postMacroInd = 1;
  mNavParams.postMacroIndNonTS = 1;
  mNavParams.nonTSacquireType = 0;
  mNavParams.acqRealign = false;
  mNavParams.acqRoughEucen = false;
  mNavParams.acqFineEucen = false;
  mNavParams.acqAutofocus = false;
  mNavParams.acqCookSpec = false;
  mNavParams.acqCenterBeam = false;
  mNavParams.acqRunPremacro = false;
  mNavParams.acqRunPremacroNonTS = false;
  mNavParams.acqRunPostmacro = false;
  mNavParams.acqRunPostmacroNonTS = false;
  mNavParams.acqSkipInitialMove = false;
  mNavParams.acqSkipZmoves = false;
  mNavParams.acqRestoreOnRealign = true;
  mNavParams.acqSendEmail = false;
  mNavParams.acqSendEmailNonTS = false;
  mNavParams.acqFocusOnceInGroup = false;
  mNavParams.acqCloseValves = false;
  mNavParams.warnedOnSkipMove = false;
  mNavParams.stockFile = "";
  mNavParams.autosaveFile = "";
  mNavParams.numImportXforms = 1;
  mNavParams.importXform[0].xpx = 10.;
  mNavParams.importXform[0].ypx = 0.;
  mNavParams.importXform[0].xpy = 0.;
  mNavParams.importXform[0].ypy = -10.;
  mNavParams.idField[0].tag = 0;
  mNavParams.xField[0].tag = 0;
  mNavParams.yField[0].tag = 0;
  mNavParams.xformName[0] = "None";
  mNavParams.xformID[0] = "";
  mNavParams.importXbase = 0.;
  mNavParams.importYbase = 0.;
  mNavParams.gridInPolyBoxFrac = 0.75f;
  mNavParams.holeInPolyBoxFrac = 1.1f;
  mNavParams.stageBacklash = 0.;
  mNavParams.overlayChannels = "121";
  mNavParams.maxMontageIS = 7.f;
  mNavParams.maxLMMontageIS = 35.f;
  mNavParams.fitMontWithFullFrames = 0.;

  mCookParams.intensity = -1.;
  mCookParams.magIndex = -1;
  mCookParams.spotSize = -1;
  mCookParams.targetDose = 2000;
  mCookParams.trackImage = true;
  mCookParams.cookAtTilt = false;
  mCookParams.tiltAngle = 60.f;
  mCookParams.timeInstead = false;
  mCookParams.minutes = 10.f;
  mCookerDlg = NULL;
  mAutocenDlg = NULL;
  mVPPConditionSetup = NULL;
  for (i = 0; i < 2; i++) {
    mTSRangeParams[i].eucentricity = false;
    mTSRangeParams[i].walkup = false;
    mTSRangeParams[i].autofocus = false;
    mTSRangeParams[i].startAngle = 50.;
    mTSRangeParams[i].endAngle = 60.;
    mTSRangeParams[i].angleInc = 2.;
    mTSRangeParams[i].imageType = 0;
    mTSRangeParams[i].direction = 0;
  }
  mScreenShotParams.imageScaleType = 0;
  mScreenShotParams.imageScaling = 2.;
  mScreenShotParams.ifScaleSizes = 1;
  mScreenShotParams.sizeScaling = 0.;
  mScreenShotParams.fileType = 0;
  mScreenShotParams.compression = 1;
  mScreenShotParams.jpegQuality = 80;
  mScreenShotParams.skipOverlays = 0;
  mAddDPItoSnapshots = 1;
  mHitachiParams.IPaddress = "192.168.10.1";
  mHitachiParams.port = "12050";
  mHitachiParams.beamBlankAxis = 0;
  mHitachiParams.beamBlankDelta = 0.5f;
  mHitachiParams.usePAforHRmodeIS = 0;
  mHitachiParams.imageShiftToUm = 50.;
  mHitachiParams.beamShiftToUm = 100.;
  mHitachiParams.beamTiltScale = 100.;
  mHitachiParams.objectiveToUm[0] = 10000.;
  mHitachiParams.objectiveToUm[1] = 10000.;
  mHitachiParams.objectiveToUm[2] = 10000.;
  mHitachiParams.screenAreaSqCm = 150.;
  mHitachiParams.stageTiltSpeed = 100;
  mHitachiParams.stageXYSpeed = 80000;
  mHitachiParams.flags = 0;
  for (i = 0; i < MAX_MAGS; i++)
    mHitachiParams.baseFocus[i] = HITACHI_LENS_MAX / 2;

  for (i = 0; i < NUM_TSS_PANELS; i++)
    mTssPanelStates[i] = (!i || i == NUM_TSS_PANELS - 1) ? 1 : 0;

  // Initialize dialog panel placements and log window status
  for (i = 0; i < MAX_TOOL_DLGS; i++)
    mDlgPlacements[i].right = 0;
  mLogPlacement.rcNormalPosition.right = 0;
  mNavPlacement.rcNormalPosition.right = 0;
  mCamSetupPlacement.rcNormalPosition.right = 0;
  mScreenShotPlacement.rcNormalPosition.right = 0;
  mRightBorderFrac = 0.;
  mBottomBorderFrac = 0.;
  mMainFFTsplitFrac = 0.5f;
  mRightFrameWidth = 0;
  mBottomFrameWidth = 0;
  mReopenLog =  false;
  mExitWithUnsavedLog = false;
  mReopenMacroToolbar = false;
  for (i = 0; i <= MAX_MACROS; i++)
    mReopenMacroEditor[i] = false;
  mOpenStateWithNav = true;
  mSkipGainRefWarning = false;
  mTestGainFactors = false;
  mDeferBufWinUpdates = false;
  mScopeHasSTEM = false;
  mFirstSTEMcamera = -1;
  mSettingSTEM = false;
  mSettingEFTEM = false;
  mScreenSwitchSTEM = false;
  mRetractToUnblankSTEM = false;
  mBlankBeamInSTEM = false;
  mMustUnblankWithScreen = false;
  mRetractOnSTEM = false;
  mStartingProgram = true;
  mNoExceptionHandler = 0;
  mDummyInstance = false;
  mNoCameras = false;
  mComModuleInited = false;
  mPlugDoingFunc = NULL;
  mPlugStopFunc = NULL;
  mPlugImagingTask = false;
  mAnyDirectDetectors = false;
  mAnySuperResMode = false;
  mFrameAlignMoreOpen = false;
  mShowRemoteControl = true;
  mHasFEIcamera = false;
  mKeepEFTEMstate = true;   // Changed for 3.7.6
  mUseRecordForMontage = false;
  mUseViewForSearch = false;
  mIdleBaseCount = 0;
  mMadeLittleFont = false;
  mAllowCameraInSTEMmode = false;
  mDoseLifetimeHours = 24;
  mBasicMode = false;
  SEMUtilInitialize();
  traceMutexHandle = CreateMutex(0, 0, 0);
  sStartTime = GetTickCount();
  mLastActivityTime = sStartTime;
  appThreadID = GetCurrentThreadId();
  SEMBuildTime(__DATE__, __TIME__);
}

// Zero out the low dose params
void CSerialEMApp::InitializeLDParams(void)
{
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < MAX_LOWDOSE_SETS; i++) {
      InitializeOneLDParam(mCamLowDoseParams[j][i]);
    }
    mCamLowDoseParams[j][VIEW_CONSET].delayFactor = 0.3f;
    mCamLowDoseParams[j][FOCUS_CONSET].delayFactor = 1.5f;
    mCamLowDoseParams[j][TRIAL_CONSET].delayFactor = 0.6f;
    mCamLowDoseParams[j][RECORD_CONSET].delayFactor = 0.9f;
    mCamLowDoseParams[j][SEARCH_AREA].delayFactor = 0.05f;
  }

}

void CSerialEMApp::InitializeOneLDParam(LowDoseParams &ldParam)
{
  ldParam.magIndex = 0;
  ldParam.camLenIndex = 0;
  ldParam.spotSize = 0;
  ldParam.intensity = 0.0;
  ldParam.probeMode = -1;
  ldParam.ISX = 0.;
  ldParam.ISY = 0.;
  ldParam.axisPosition = 0.;
  ldParam.beamDelX = 0.;
  ldParam.beamDelY = 0.;
  ldParam.axisPosition = 0.;
  ldParam.delayFactor = 1.0f;
  ldParam.slitIn = false;
  ldParam.slitWidth = 0.;
  ldParam.energyLoss = 0.;
  ldParam.zeroLoss = false;
  ldParam.beamAlpha = -999.;
  ldParam.diffFocus = -999.;
  ldParam.beamTiltDX = 0.;
  ldParam.beamTiltDY = 0.;
  ldParam.darkFieldMode = 0;
  ldParam.dfTiltX = 0.;
  ldParam.dfTiltY = 0.;
}

// Clean up everything so we can detect memory leaks
CSerialEMApp::~CSerialEMApp()
{
  for (int i = 0; i < MAX_CAMERAS; i++)
    delete mCamParams[i].origDefects;
  delete mBufferManager;
  delete mMacroProcessor;
  delete mShiftManager;
  delete mShiftCalibrator;
  delete mMontageController;
  delete mFocusManager;
  delete mComplexTasks;
  delete mMultiTSTasks;
  delete mParticleTasks;
  delete mBeamAssessor;
  delete mProcessImage;
  delete mTSController;
  delete mParamIO;
  delete m_pMainWnd;
  delete mFilterTasks;
  delete mDistortionTasks;
  delete mGainRefMaker;
  delete mCalibTiming;
  delete mNavHelper;
  delete mMailer;
  delete mGatanSocket;
  delete mFalconHelper;
  delete mPiezoControl;
  delete mAutoTuning;
  delete mExternalTools;
  CBaseSocket::UninitializeWSA();
  delete mPluginManager;
  for (int i = 0; i < mIdleArray.GetSize(); i++)
    delete mIdleArray[i];
  KStoreADOC::AllDone();
}
/////////////////////////////////////////////////////////////////////////////
// The one and only CSerialEMApp object

CSerialEMApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CSerialEMApp initialization

BOOL CSerialEMApp::InitInstance()
{
  int iSet, iCam, iAct, mag, indSpace, indQuote1, indQuote2;
  bool anyFrameSavers = false;
  CString message, dropCameras, settingsFile;
  void *toolDlgs[] = {&mStatusWindow, &mBufferWindow, &mImageLevel, &mScopeStatus,
  &mRemoteControl, &mTiltWindow, &mCameraMacroTools, &mAlignFocusWindow, &mLowDoseDlg,
    &mMontageWindow, &mSTEMcontrol, &mFilterControl, &mDEToolDlg};
  for (mag = 0; mag < sizeof(toolDlgs) / sizeof(void *); mag++)
    mToolDlgs[mag] = (CToolDlg *)toolDlgs[mag];

  AfxEnableControlContainer();

  // Standard initialization
  // If you are not using these features and wish to reduce the size
  //  of your final executable, you should remove from the following
  //  the specific initialization routines you do not need.

  // Got rid of Enable3DControls

  // Parse the command line for arguments and the remainder, a subpath for system path
  mDummyInstance = false;
  if (m_lpCmdLine && strlen(m_lpCmdLine)) {
    mSysSubpath = CString(m_lpCmdLine).Trim();
    while (1) {
      indSpace = mSysSubpath.Find(" ");
      if (mSysSubpath.Find("/DUMMY") == 0) {
        mDummyInstance = true;
      } else if (mSysSubpath.Find("/NoCam") == 0) {
        dropCameras = mSysSubpath;
        if (indSpace > 0)
          dropCameras = dropCameras.Left(indSpace);

      } else if (mSysSubpath.Find("/Settings=") == 0 && (indSpace > 10 || 
        (indSpace < 0 && mSysSubpath.GetLength() > 10))) {
        settingsFile = mSysSubpath;
        if (indSpace > 0)
          settingsFile = settingsFile.Left(indSpace);
        settingsFile = settingsFile.Mid(10);

        // The path could have quotes if it has a space, but it is not required
      } else if (mSysSubpath.Find("/PlugDir=") == 0) {
        indQuote1 = mSysSubpath.Find("\"");
        indQuote2 = mSysSubpath.Find("\"", 10);
        if (indQuote1 == 9 && indQuote2 > 0) {
          mArgPlugDir = mSysSubpath.Left(indQuote2);
          mArgPlugDir = mArgPlugDir.Mid(10);
          indSpace = mSysSubpath.Find(" ", indQuote2);
        } else if (indQuote1 < 0) {
          mArgPlugDir = mSysSubpath;
          if (indSpace > 0)
            mArgPlugDir = mSysSubpath.Left(indSpace);
          mArgPlugDir = mArgPlugDir.Mid(9);
        }
      } else
        break;

      if (indSpace > 0)
        mSysSubpath = mSysSubpath.Mid(indSpace + 1);
      else
        mSysSubpath = "";
    }
    mSysSubpath = mSysSubpath.Trim();
  }

  if (!mDummyInstance) {

    // If there is a previous instance, this will return a good handle with an error
    HANDLE hMutex = CreateMutex(NULL, FALSE, "SerialEMSingleInstance");
    if (hMutex) {
      if (GetLastError() == ERROR_ALREADY_EXISTS)
        return FALSE;
    }
  }

  //if (!AfxOleInit()){
  if (CoInitializeEx(NULL, COINIT_MULTITHREADED) != S_OK) {
    AfxMessageBox(_T("COM Initialization failed!"));
    return FALSE;
  }
  /*if (CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_NONE,
    RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL) != S_OK)
    AfxMessageBox(_T("CoInitializeSecurity failed!")); */

 	_Module.Init(NULL, m_hInstance);
  mComModuleInited = true;

  // No longer get the windows version

  // Change the registry key under which our settings are stored.
  SetRegistryKey(_T("BL3DFS Applications"));

  LoadStdProfileSettings();  // Load standard INI file options (including MRU)

  // Register the application's document templates.  Document templates
  //  serve as the connection between documents, frame windows and views.

  CMultiDocTemplate* pDocTemplate;
  pDocTemplate = new CMultiDocTemplate(
    IDR_SERIALTYPE,
    RUNTIME_CLASS(CSerialEMDoc),
    RUNTIME_CLASS(CChildFrame), // custom MDI child frame
    RUNTIME_CLASS(CSerialEMView));
  AddDocTemplate(pDocTemplate);

  // create main MDI Frame window
  CMainFrame* pMainFrame = new CMainFrame;

  
  if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
    return FALSE;
  m_pMainWnd = pMainFrame;
  mMainFrame = (CMainFrame *)m_pMainWnd;

  // Parse command line for standard shell commands, DDE, file open
  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  // Dispatch commands specified on the command line
  if (!ProcessShellCommand(cmdInfo))
    return FALSE;

  // The main window has been initialized, so show and update it.
  pMainFrame->ShowWindow(m_nCmdShow);
  pMainFrame->UpdateWindow();
  
  
  // Try to find the document 
  mDocWnd = NULL;
  POSITION pos = GetFirstDocTemplatePosition();
  if (pos) {
    CDocTemplate *pDocTemp = GetNextDocTemplate(pos);
    if (pDocTemp) {
      POSITION dPos = pDocTemp->GetFirstDocPosition();
      if(dPos) {
         mDocWnd = (CSerialEMDoc *)pDocTemp->GetNextDoc(dPos);
      }
    }
  }
  if (!mDocWnd) {
    AfxMessageBox("No document class found");
    return FALSE;
  }
  if (!settingsFile.IsEmpty())
    mDocWnd->SetSettingsName(settingsFile);
  
  // Initialize the buffer manager
  mBufferManager = new EMbufferManager(mModeName, mImBufs);
  mMacroProcessor = new CMacCmd();
  
  // Get the various task managers
  // Don't forget to add new ones to the destructor
  mShiftManager = new CShiftManager();
  mShiftCalibrator = new CShiftCalibrator();
  mMontageController = new EMmontageController();
  mFocusManager = new CFocusManager();
  mComplexTasks = new CComplexTasks();
  mMultiTSTasks = new CMultiTSTasks();
  mParticleTasks = new CParticleTasks();
  mBeamAssessor = new CBeamAssessor();
  mProcessImage = new CProcessImage();
  mTSController = new CTSController();
  mFilterTasks = new CFilterTasks();
  mDistortionTasks = new CDistortionTasks();
  mGainRefMaker = new CGainRefMaker();
  mCalibTiming = new CCalibCameraTiming();
  mNavHelper = new CNavHelper();
  mFalconHelper = new CFalconHelper();
  mPiezoControl = new CPiezoAndPPControl();
  mAutoTuning = new CAutoTuning();
  mExternalTools = new CExternalTools();
  mMailer = new CMailer();
  mGatanSocket = new CGatanSocket();
  mPluginManager = new CPluginManager();
  CBaseSocket::InitSocketStatics();
  // Get the scope
  mScope = new CEMscope();

  // Get the camera controller: can do this early now that there are no smart pointers
  // But no earlier than this: the constructor assumes scope pointer etc exist already
  mCamera = new CCameraController();

  // Detect if display is not 120 DPI and initialize accordingly
  HDC screen = GetDC(0);
  mSystemDPI = GetDeviceCaps(screen, LOGPIXELSX);
  ReleaseDC(0, screen);
  mDisplayNotTruly120DPI = mSystemDPI == 120;
  mag = mSystemDPI;
  b3dSetStoreError(1);

  // Initialize the ParameterIO class and read properties and settings; reopen log
  // now in case someone has debugging output to the log
  mParamIO = new CParameterIO;
  mDocWnd->ReadSetPropCalFiles();
  if (mNoExceptionHandler <= 0)
    SetUnhandledExceptionFilter(SEMExceptionFilter);
  if (mScope->GetNoScope() && mNoCameras && mScope->GetSimulationMode())
    mDummyInstance = true;
  if (mDummyInstance) {
    mScope->SetNoScope(true);
    mCamera->SetSimulationMode(true);
    mNoCameras = true;
    if (mAssumeCamForDummy >= 0 && mAssumeCamForDummy < MAX_CAMERAS) {
      if (mCamParams[mAssumeCamForDummy].GIF)
        mEFTEMMode = true;
      else if (mCamParams[mAssumeCamForDummy].STEMcamera)
        mSTEMMode = true;
    }
    mInitialCurrentCamera = mAssumeCamForDummy;
  }
  if (mSystemDPI != 120)
    mDisplayNotTruly120DPI = false;
  if (mReopenLog) {
    if (mLogWindow) {
      if (mLogPlacement.rcNormalPosition.right > 0)
        mLogWindow->SetWindowPlacement(&mLogPlacement);
    } else {
      OnFileOpenlog();
    }
  }
  SEMTrace('1', "Detected system DPI of %d, using DPI %d%s", mag, mSystemDPI,
    mDisplayNotTruly120DPI ? " but not scaling for 120" : "");

  // Set constants based on DPI if not supplied by user
  if (!mToolTitleHeight) {
    mToolTitleHeight = mSystemDPI <= 120 ? 24 : 40;

    // This is based on a post about what is used in Firefox for a resizeable window,
    // where a commenter found that scaling the resizing frame by a dpi ratio worked 
    mag = (int)ceil(GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFIXEDFRAME) * 
      mSystemDPI / 96.);
    mToolTitleHeight = B3DMAX(mToolTitleHeight, mag);
    if (mSystemDPI < 120) {
      mToolExtraWidth = 8;
      mToolExtraHeight = 5;
    } else {
      mToolExtraWidth = 10;
      mToolExtraHeight = 6;
    }
  }

  // Initialize the gain ref now that extra refs are known
  mGainRefMaker->InitializeRefArrays();

  // Trim the active camera list if command line argument says to
  if (!dropCameras.IsEmpty()) {
    iSet = 0;
    for (iAct = 0; iAct < mActiveCamListSize; iAct++) {
      message.Format("%d", mActiveCameraList[iAct]);
      if (dropCameras.Find(message) < 0) 
        mActiveCameraList[iSet++] = mActiveCameraList[iAct];
    }
    mActiveCamListSize = iSet;
  }

  // Find out if STEM interface is to be started and check K2 scaling
  iSet = 0;
  for (iAct = 0; iAct < mActiveCamListSize; iAct++) {
    iCam = mActiveCameraList[iAct];
    if (mCamParams[iCam].STEMcamera)
      mScopeHasSTEM = true;
    else if (mCamParams[iCam].FEItype)
      mHasFEIcamera = true;
    if (mCamParams[iCam].K2Type) {
      iSet++;
      if (mCamParams[iCam].countsPerElectron > 0. && 
        mCamParams[iCam].countsPerElectron < 2.) {
          message.Format("The camera property CountsPerElectron is set to %1.f for camera"
            " %d\n\nFor a K2 camera, it needs to be the number of\nlinear mode counts per"
            " electron, not a value near 1.\nFor a K3, it needs to be 32 and the entry"
            " is not needed", mCamParams[iCam].countsPerElectron, iCam);
          AfxMessageBox(message, MB_EXCLAME);
      }
    }

    // Check for transposed usable area limits
    CameraDefects *def = &mCamParams[iCam].defects;
    bool longInY = def->usableBottom > mCamParams[iCam].sizeY + 4 && 
      def->usableRight < mCamParams[iCam].sizeX - 4;
    if (longInY || (def->usableBottom < mCamParams[iCam].sizeY - 4 && 
      def->usableRight > mCamParams[iCam].sizeX + 4)) {
        message.Format("The camera property UsableArea for camera %d extends beyond\r\n"
          "the camera size in %s and is less than the size in %s\r\n"
          "Should it be UsableArea %d %d %d %d instead?", iCam, longInY ? "Y" : "X",
          longInY ? "X" : "Y", def->usableLeft, def->usableTop, def->usableRight,
          def->usableBottom);
        AfxMessageBox(message, MB_EXCLAME);
        AppendToLog(message);
    }
  }

  if (iSet > 0 && mCamera->GetScalingForK2Counts() > 0. && 
    mCamera->GetScalingForK2Counts() < 2.) {
      message.Format("The general property ScalingForK2Counts is set to %1.f\n\n"
        "It needs to be close to the number of linear mode\ncounts per"
        " electron, not a value near 1.", mCamera->GetScalingForK2Counts());
      AfxMessageBox(message, MB_EXCLAME);
  }
  
  // Double sizes for K2 camera and define the DM and Gatan camera flags
  iAct = 0;
  for (iCam = 0; iCam < MAX_CAMERAS; iCam++) {
    mCamParams[iCam].DMCamera = !(mCamParams[iCam].TietzType ||
      mCamParams[iCam].FEItype || mCamParams[iCam].DE_camType || 
      !mCamParams[iCam].pluginName.IsEmpty());
    mCamParams[iCam].GatanCam = mCamParams[iCam].DMCamera && !mCamParams[iCam].AMTtype;
    if (!mCamParams[iCam].GatanCam)
      mCamParams[iCam].K2Type = 0;
    if (mCamParams[iCam].K2Type)
      AdjustSizesForSuperResolution(iCam);
  }

  // Load plugins now, in case there is a microscope plugin
  mPluginManager->LoadPlugins();

  // Initialize the scope now that properties are set
  if (mScope->Initialize() && mExitOnScopeError)
    return false;

  // Check initialization of parameter sets for first active camera now that we know size
  int numInit = 0;
  int camMScopied, numMontSearchCopy = 0;
  iCam = mActiveCameraList[0];
  for (iSet = 0; iSet < NUMBER_OF_USER_CONSETS; iSet++) {
    if (!mCamConSets[iCam][iSet].right) {
      if (iSet == SEARCH_CONSET || iSet == MONT_USER_CONSET) {
        if (iSet == SEARCH_CONSET)
          mCamConSets[iCam][iSet] = mCamConSets[iCam][VIEW_CONSET];
        else
          mCamConSets[iCam][iSet] = mCamConSets[iCam][RECORD_CONSET];
        numMontSearchCopy = 1;
      } else {
        mParamIO->SetDefaultCameraControls(iSet, &mConSets[iSet], mCamParams[iCam].sizeX, 
          mCamParams[iCam].sizeY);
      }
      numInit++;
    }
  }

  // Finish scope-dependent or other initialization of camera parameters
  for (iCam = 0; iCam < MAX_CAMERAS; iCam++) {
    if (mCamParams[iCam].restoreBBmode < 0)
      mCamParams[iCam].restoreBBmode = JEOLscope ? 1 : 0;
    if (mCamParams[iCam].FEItype && !mCamParams[iCam].STEMcamera && 
      !mCamParams[iCam].minExposure)
        mCamParams[iCam].minExposure = DEFAULT_FEI_MIN_EXPOSURE;
  }

  // Check the rest of the cameras for uninitialized parameter sets and transfer them
  // from camera 0, unless ALL sets were initialized for camera 0
  for (iAct = 0; iAct < mActiveCamListSize; iAct++) {
    iCam = mActiveCameraList[iAct];
    camMScopied = 0;
    if (mCamParams[iCam].sizeX <= 0 || mCamParams[iCam].sizeY <= 0 ||
      mCamParams[iCam].pixelMicrons <= 0) {
      message.Format("%s is not defined for camera %d",
        mCamParams[iCam].pixelMicrons <= 0 ? "Pixel size" : "Camera size in X and Y" , 
        iCam);
      AfxMessageBox(message, MB_EXCLAME);
      return false;
    }

    for (iSet = 0; iSet < NUMBER_OF_USER_CONSETS; iSet++) {
      if (!mCamConSets[iCam][iSet].right) {
        if (iSet == SEARCH_CONSET || iSet == MONT_USER_CONSET) {
          mCamConSets[iCam][iSet] = B3DCHOICE(iSet == SEARCH_CONSET,
            mCamConSets[iCam][VIEW_CONSET], mCamConSets[iCam][RECORD_CONSET]);
          camMScopied = 1;
        } else if (numInit < NUMBER_OF_USER_CONSETS) {
          TransferConSet(iSet, mActiveCameraList[0], iCam);
        } else {
          mParamIO->SetDefaultCameraControls(iSet, &mCamConSets[iCam][iSet],
            mCamParams[iCam].sizeX, mCamParams[iCam].sizeY);
        }
      }

      // If camera has only one shutter, set to beam blank
      if (mCamParams[iCam].onlyOneShutter)
        mCamConSets[iCam][iSet].shuttering = USE_BEAM_BLANK;
      B3DCLAMP(mCamConSets[iCam][iSet].right, 0, mCamParams[iCam].sizeX);
      B3DCLAMP(mCamConSets[iCam][iSet].bottom, 0, mCamParams[iCam].sizeY);

      // Backwards compatibility for Oneview drift correction which was stored in
      // alignFrames: if it is -1, it was not read in, so transfer alignFrames to it
      // and zero out alignFrames
      if (mCamConSets[iCam][iSet].correctDrift < 0 && mCamParams[iCam].OneViewType) {
        mCamConSets[iCam][iSet].correctDrift = mCamConSets[iCam][iSet].alignFrames;
        mCamConSets[iCam][iSet].alignFrames = 0;
      }
    }
    mCamParams[iCam].canBlock = mCamParams[iCam].retractable;
    numMontSearchCopy += camMScopied;

    // 10/1/13: subareas no longer need to be square with rotations; but needs testing
    /*if (mCamParams[iCam].TietzType && mCamParams[iCam].TietzImageGeometry > 0 && 
      (mCamParams[iCam].TietzImageGeometry & 20))
      mCamParams[iCam].squareSubareas = true; */

    // If no individual entry was made, set to global default
    if (mCamParams[iCam].processHere < 0)
      mCamParams[iCam].processHere = mProcessHere ? 1 : 0;
    if (!mCamera->CanProcessHere(&mCamParams[iCam]))
      mCamParams[iCam].processHere = 0;
    if (mCamera->IsDirectDetector(&mCamParams[iCam]))
      mAnyDirectDetectors = true;
    if (mCamParams[iCam].canTakeFrames & FRAMES_CAN_BE_SAVED)
      anyFrameSavers = true;
  }
  if ((mAnyDirectDetectors || anyFrameSavers) && mDocWnd->GetDfltUseMdoc() < 0)
    mDocWnd->SetDfltUseMdoc(1);

  // Copy active list from properties into original active list
  // Check for 16-bit cameras and for GIF
  BOOL anyGIF = false;

  int deCamCount = 0;
  //Max 3 cameras for DE
  int MAX_DE_Cams = 3;
  CString DE_camNames[] = {"","",""};

  for (iCam = 0; iCam < mActiveCamListSize; iCam++) {
    mOriginalActiveList[iCam] = mActiveCameraList[iCam];
    if (mCamParams[mActiveCameraList[iCam]].unsignedImages)
      mAny16BitCameras = true;
    if (mCamParams[mActiveCameraList[iCam]].GIF)
      anyGIF = true;
    if (mCamParams[mActiveCameraList[iCam]].DE_camType)	{
      if (deCamCount < MAX_DE_Cams) {
        DE_camNames[deCamCount] = mCamParams[mActiveCameraList[iCam]].name;
        deCamCount++;
      }
    }
  }

  mScopeHasFilter = anyGIF || mScope->GetHasOmegaFilter();

  // 9/13/12: Moved starting scope update from here to after camera initialization done
  // Further initialization of various task managers now that all pointers exist
  // Moved up here 6/3/13 since panels start updates and call at least shiftmanager
  mShiftManager->Initialize();
  mMenuTargets.Initialize();
  mShiftCalibrator->Initialize();
  mFocusManager->Initialize();
  mMontageController->Initialize();
  mMacroProcessor->Initialize();
  mComplexTasks->Initialize();
  mMultiTSTasks->Initialize();
  mParticleTasks->Initialize();
  mBeamAssessor->Initialize();
  mTSController->Initialize();
  mFilterTasks->Initialize();
  mDistortionTasks->Initialize();
  mNavHelper->Initialize();
  mAutoTuning->Initialize();
  mProcessImage->Initialize();
  mFalconHelper->InitializePointers();
  CNavRotAlignDlg::InitializeStatics();

  // Start the tool windows
  mNumToolDlg = 0;
  for (iSet = 0; iSet < MAX_TOOL_DLGS; iSet++)
    mDlgColorIndex[iSet] = iSet;

#define CREATE_IF_SHOWING(cond, color) \
  if (cond) {  \
    mToolDlgs[color]->Create(sToolDlgIDs[color]);  \
    mDlgColorIndex[mNumToolDlg] = color;  \
    mDialogTable[mNumToolDlg++].pDialog = mToolDlgs[color];  \
  }

  CREATE_IF_SHOWING(!IsIDinHideSet(-10), 0);
  CREATE_IF_SHOWING(!IsIDinHideSet(-11), 1);
  CREATE_IF_SHOWING(!IsIDinHideSet(-12), 2);
  CREATE_IF_SHOWING(!IsIDinHideSet(-13), 3);
  if (IsIDinHideSet(-14))
    mShowRemoteControl = false;
  CREATE_IF_SHOWING(mShowRemoteControl, 4);
  CREATE_IF_SHOWING(!IsIDinHideSet(-15), 5);
  CREATE_IF_SHOWING(!IsIDinHideSet(-16), 6);
  CREATE_IF_SHOWING(!IsIDinHideSet(-17), 7);
  CREATE_IF_SHOWING(!IsIDinHideSet(-18), 8);
  CREATE_IF_SHOWING(!IsIDinHideSet(-19), 9);
  CREATE_IF_SHOWING(mScopeHasSTEM && !IsIDinHideSet(-20), 10);
  CREATE_IF_SHOWING(mScopeHasFilter && !IsIDinHideSet(-21), 11);
  CREATE_IF_SHOWING(deCamCount > 0 && !IsIDinHideSet(-22), 12);

  if (deCamCount > 0)
	  mDEToolDlg.setUpDialogNames(DE_camNames,deCamCount);

  pMainFrame->InitializeDialogTable(mDialogTable, initialDlgState, mNumToolDlg, 
    dlgBorderColors, mDlgColorIndex, mDlgPlacements);
  SetMaxDialogWidth();

  pMainFrame->InitializeStatusBar();

  RestoreViewFocus();
  if (mMainView) 
    DoResizeMain();

  // More initializations
  mDocWnd->SetPointers(mBufferManager, &mBufferWindow);
  mMailer->Initialize();
  mPiezoControl->Initialize();
  if (mReopenMacroToolbar)
    mMacroProcessor->OpenMacroToolbar();
  OpenOrCloseMacroEditors();

  mDocWnd->AppendToProgramLog(true);
  
  // INITIALIZE THE CAMERAS
  if (mCamera->Initialize(INIT_ALL_CAMERAS) < 0)
    return false;
  if (mNoCameras) {
    mNumActiveCameras = mActiveCamListSize;
    mNumAvailCameras = mActiveCamListSize;
    mNumReadInCameras = mActiveCamListSize;
  }

  // Build the calibrations of rotation and pixel size
  mShiftManager->PropagatePixelSizes();
  mShiftManager->PropagateRotations();
  
  // Make sure low dose parameters get copied in
  CopyCameraToCurrentLDP();

  // set default startup camera as non GIF camera if possible
  iCam = mFilterParams.firstRegularCamera >= 0 ? 
    mFilterParams.firstRegularCamera : mFilterParams.firstGIFCamera;
  if (iCam < 0)
    iCam = 0;

  // Then if an initial current camera is entered, try to find that in active list
  if (mInitialCurrentCamera >= 0)
    iCam = LookupActiveCamera(mInitialCurrentCamera);

  // And if not, stay in STEM mode if in STEM
  else if (mScopeHasSTEM && mScope->GetInitialized() && mScope->GetSTEMmode())
    iCam = mFirstSTEMcamera;

  // Or pick the camera that keeps the EFTEM state if flag set
  else if (mScopeHasFilter && mScope->GetInitialized() && mScope->GetCanControlEFTEM()) {
    BOOL inState = mScope->GetEFTEM();
    mScope->SetSelectedEFTEM(inState);
    if (mKeepEFTEMstate && mFilterParams.firstRegularCamera >= 0) {
      iCam = inState ? mFilterParams.firstGIFCamera : mFilterParams.firstRegularCamera;
      mCurrentActiveCamera = iCam;
    }
  }
  if (iCam < 0)
    iCam = 0;

  SetActiveCameraNumber(iCam);
  mInitialCurrentCamera = mActiveCameraList[iCam];

  for (iAct = 0; iAct < mActiveCamListSize; iAct++) {
    iCam = mActiveCameraList[iAct];
    if (mCamParams[iCam].FEItype == FALCON3_TYPE) {
      mCamParams[iCam].unscaledCountsPerElec = mCamParams[iCam].countsPerElectron;
      mCamera->AdjustCountsPerElecForScale(&mCamParams[iCam]);
    }
    if (mCamParams[iCam].K2Type || (mCamParams[iCam].DE_camType && 
      (mCamParams[iCam].CamFlags & DE_CAM_CAN_COUNT)))
      mAnySuperResMode = true;

    // For Falcon 2 or DE that can't align, if align is ON without using framealign, 
    // turn it off
    for (iSet = 0; iSet < NUMBER_OF_USER_CONSETS; iSet++) {
      ControlSet *cs = &mCamConSets[iCam][iSet];
      if (mCamParams[iCam].FEItype == FALCON2_TYPE || (mCamParams[iCam].DE_camType && 
        !(mCamParams[iCam].CamFlags & DE_CAM_CAN_ALIGN))) {
        if (!cs->useFrameAlign && cs->alignFrames > 0)
          cs->alignFrames = 0;
      }
      if (mCamParams[iCam].FEItype == FALCON3_TYPE)
        cs->numSkipBefore = cs->numSkipAfter = 0;

      // Transition DE saving flags to new form
      if (mCamParams[iCam].DE_camType) {
        if ((cs->saveFrames & DE_SAVE_FRAMES) && (cs->saveFrames & DE_SAVE_SUMS))
           cs->saveFrames |= DE_SAVE_MASTER | DE_SAVE_SINGLE;
        else if (cs->saveFrames & DE_SAVE_FRAMES) {
          cs->saveFrames |= DE_SAVE_MASTER;
          cs->DEsumCount = 1;
        } else if (cs->saveFrames & DE_SAVE_SUMS) {
          cs->saveFrames |= DE_SAVE_MASTER;
        }
        cs->saveFrames &= ~(DE_SAVE_FRAMES | DE_SAVE_SUMS) ;
      }

      // Make sure there are valid binnings
      if (!BinningIsValid(cs->binning, iCam, 
        (mCamParams[iCam].K2Type && cs->K2ReadMode == SUPERRES_MODE) || 
        (mCamParams[iCam].K2Type == K3_TYPE && cs->K2ReadMode > LINEAR_MODE))) {
          int newBin = NextValidBinning(cs->binning, -1, iCam, false);
          if (newBin == cs->binning)
            newBin = NextValidBinning(cs->binning, 1, iCam, false);
          cs->binning = newBin;
      }
    }
  }
  CopyConSets(mCurrentCamera);

  // Need to start update after cameras are initialized and active number set this way
  mScope->StartUpdate();

  mScope->ReadProbeMode();
  mAlignFocusWindow.Update();

  // Zero the image shifts
  if (mScope->GetInitialized()) {
    BOOL doReset = !(JEOLscope && mScope->GetSTEMmode());
    if (doReset && JEOLscope) {
      double startTime = GetTickCount();
      while (!mScope->mJeolSD.bDataIsGood && SEMTickInterval(startTime) < 5000) {
        Sleep(100);
      }
      doReset = mScope->mJeolSD.bDataIsGood;
      if (doReset)
        mScope->ScopeUpdate(GetTickCount());
    }
    if (doReset && HitachiScope) {
      iCam = mScope->GetMagIndex();
      doReset = iCam >= mScope->GetLowestMModeMagInd() &&
        iCam < mScope->GetLowestSecondaryMag();
    }
    if (doReset)
      mShiftManager->ResetImageShift(false, false);
    if (!mCamParams[mInitialCurrentCamera].noShutter)
      mScope->BlankBeam(false);
  }
  ManageSTEMBlanking();

  // Initialize continuous aligning and assess GPU for it
  mFalconHelper->Initialize(-3);
  
  pMainFrame->SetWindowText("SerialEM");
  if (!mStartupMessage.IsEmpty())
    AppendToLog(mStartupMessage, LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN);
  if (mStartupInfo) {
    GetStartupMessage();
    AppendToLog(mStartupMessage, LOG_SWALLOW_IF_CLOSED);
    if (mScope->GetNoScope())
      AppendToLog("Running with no microscope connection", LOG_SWALLOW_IF_CLOSED);
  }
  CString *dups = mParamIO->GetDupMessage();
  if (!dups->IsEmpty())
    AppendToLog((LPCTSTR)(*dups), LOG_SWALLOW_IF_CLOSED);
  mBeamAssessor->InitialSetupForAperture();
  if (mScope->GetHasOmegaFilter())
    WarnIfUsingOldFilterAlign();
  mParamIO->ReportSpecialOptions();
  for (iAct = 0; iAct < mActiveCamListSize; iAct++) {
    iCam = mActiveCameraList[iAct];
    if (mCamParams[iCam].K2Type == 1 && mCamParams[iCam].startupDelay >= 1.46 && 
      mCamParams[iCam].startupDelay < mCamera->GetK2MinStartupDelay())
        PrintfToLog("WARNING: Post-exposure actions will be disabled for camera %d "
          "because the\r\n  camera property StartupDelay is too short.  To enable "
          "Post-exposure actions,\r\n"
          "  run Camera Timing in the Calibrate menu and update the StartupDelay.", iCam);
  }
  AppendToLog("Read settings from: " + mDocWnd->GetCurrentSettingsPath(),
    LOG_SWALLOW_IF_CLOSED);
  AppendToLog("Read properties/calibrations from: " + mDocWnd->GetFullSystemDir(),
    LOG_SWALLOW_IF_CLOSED);
  if (mDocWnd->GetReadScriptPack())
    AppendToLog("Read scripts from " + mDocWnd->GetCurScriptPackPath(),
      LOG_SWALLOW_IF_CLOSED);
  if (mLogWindow)
    mLogWindow->SetUnsaved(false);
  if (mDummyInstance)
    SetTitleFile("");
  sAboutVersion = VERSION_STRING;
  if (sAboutVersion.Find("beta") > 0)
    sAboutVersion += " " + sBuildDate;

  // Set memory limit
  if (mMemoryLimit <= 0.) {
    mMemoryLimit = 0.75f * (float)b3dPhysicalMemory();
#ifndef _WIN64
    if (mMemoryLimit > 1.8e9) {
      LPFN_ISWOW64PROCESS fnIsWow64Process;
      BOOL bIsWow64 = false;
      fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
        GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
      if (NULL != fnIsWow64Process)
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
      if (bIsWow64)
        mMemoryLimit = B3DMIN(3.7e9f, mMemoryLimit);
      else if (mMemoryLimit)
        mMemoryLimit = 1.9e9f;
    }
#endif // _WIN64
  }

  mExternalTools->AddMenuItems();
  mMainFrame->RemoveHiddenItemsFromMenus();
  
  mStartingProgram = false;
  DoResizeMain();
  SetTitleFile("");
  iCam = mMacroProcessor->FindMacroByNameOrTextNum(mScriptToRunAtStart);
  if (iCam >= 0)
    mMacroProcessor->Run(iCam);
  return TRUE;
}

// Multiply sizes and binning and divide pixel sizes from properties by 2 for a camera
// with super-resolution mode; or undo this operation (not used for DE after all!)
void CSerialEMApp::AdjustSizesForSuperResolution(int iCam)
{
  int iSet, mag;
  float pixelDiv = 2.f;
  if (mCamParams[iCam].DE_camType && !(mCamParams[iCam].CamFlags & DE_CAM_CAN_COUNT)) {
    pixelDiv = 0.5f;
    for (iSet = 1 ; iSet < mCamParams[iCam].numBinnings; iSet++)
      mCamParams[iCam].binnings[iSet - 1] = mCamParams[iCam].binnings[iSet] / 2;
    mCamParams[iCam].numBinnings--;
  } else {
    for (iSet = B3DMIN(mCamParams[iCam].numBinnings - 1, MAX_BINNINGS - 2); iSet >= 0;
      iSet--)
      mCamParams[iCam].binnings[iSet + 1] = mCamParams[iCam].binnings[iSet] * 2;
    mCamParams[iCam].binnings[0] = 1;
    mCamParams[iCam].numBinnings = B3DMIN(mCamParams[iCam].numBinnings + 1, 
      MAX_BINNINGS);
  }
  mCamParams[iCam].sizeX = B3DNINT(mCamParams[iCam].sizeX * pixelDiv);
  mCamParams[iCam].sizeY = B3DNINT(mCamParams[iCam].sizeY * pixelDiv);

  // Divide the pixel sizes by 2; they are based on native pixels
  mCamParams[iCam].pixelMicrons /= pixelDiv;
  for (mag = 1; mag < MAX_MAGS; mag++)
    mMagTab[mag].pixelSize[iCam] /= pixelDiv;
}

CString CSerialEMApp::GetStartupMessage(bool original)
{
  if (!original) {
    CTime ctdt = CTime::GetCurrentTime();
    int iCam = 32;
#ifdef _WIN64
    iCam = 64;
#endif
    mStartupMessage.Format("%s %d-bit,  built %s  %s\r\n%s  %d/%d/%d  %02d:%02d:%02d",
      VERSION_STRING, iCam, (LPCTSTR)sBuildDate, (LPCTSTR)sBuildTime,
      mStartingProgram ? "Started" : "Current date", ctdt.GetMonth(),
      ctdt.GetDay(), ctdt.GetYear(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
  }
  return mStartupMessage;
}

// Return an integer of the form yyyymmdd
int CSerialEMApp::GetBuildDayStamp(void)
{
  return sBuildDayStamp;
}

// Return the version in the form 10000 * generation + 100 * major + minor
int CSerialEMApp::GetIntegerVersion(void)
{
  CString verStr = VERSION_STRING;
  int major, minor, gen, start = verStr.Find("ion ") + 4;
  verStr = verStr.Mid(start);
  sscanf((LPCTSTR)verStr, "%d.%d.%d", &gen, &major, &minor);
  return 10000 * gen + 100 * major + minor;
}

int CSerialEMApp::ExitInstance() 
{
  // Destructors have already been called for document...
  // Put checks for whether to exit in MainFrm OnClose
  // It already set this flag but do it in case we get here another way
  mAppExiting = true;
  if (mPluginManager)
    mPluginManager->ReleasePlugins();
  if (mScope)
    delete mScope;
  if (mCamera)
    delete mCamera;
  
  if (mComModuleInited)
  	_Module.Term();
  return CWinApp::ExitInstance();
}

static LONG WINAPI SEMExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
  static bool first = true;
  CString name, func;
  CString message;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  int startInd = winApp->mStartupMessage.Find('\r');

  // It comes in twice; returning this only the second time allows the application error
  // box to come up once instead of twice
  if (!first)
    return (winApp->GetNoExceptionHandler() ? EXCEPTION_EXECUTE_HANDLER : 
      EXCEPTION_CONTINUE_SEARCH);
  try {
    func = winApp->mCamera->GetNewFunctionCalled();
    if (!func.IsEmpty()) {
      message = "SerialEM is crashing trying to call\r\n" + func +
        "\r\nin its plugin to DigitalMicrograph.\r\n\r\n"
        "You need to upgrade the plugin to a version with this function,\r\n"
        "as well as one or two \"proxy-stub\" dlls (with \"ps\" in their names)";
    } else if (!sFunctionCalled.IsEmpty()) {
      message = "SerialEM is crashing trying to call\r\n" + sFunctionCalled +
      "\r\nwhich may mean that function does not exist in\r\n"
      "the version of the object being called.\r\n\r\n";

    } else {
      message.Format("%s is crashing due to an unhandled exception\r\n(Exception "
        "code 0x%x, address 0x%x, SEMTrace is 0x%x)", startInd > 0 ?
        (LPCTSTR)winApp->mStartupMessage.Left(startInd) : "SerialEM",
        ExceptionInfo->ExceptionRecord->ExceptionCode, 
        ExceptionInfo->ExceptionRecord->ExceptionAddress, SEMTrace);
    
      // Tried adding a backtrace but it was short and useless
    }

    // First try to close the valves immediately
    if (winApp->mTSController->GetMessageBoxCloseValves() && 
      winApp->mTSController->GetMessageBoxValveTime() > 0. && 
      !winApp->mScope->GetNoColumnValve()) {
        winApp->mScope->SetColumnValvesOpen(false, true);
        message += "\r\n\r\nValves have been closed";
    }
  }
  catch (...) {
  }
  try {

    // Then try to save the log somewhere
    if (winApp->mLogWindow) {
      winApp->AppendToLog(message);
      if ((winApp->mLogWindow->GetSaveFile()).IsEmpty()) {
        if (winApp->mStoreMRC)
          name = winApp->mStoreMRC->getName();
        if (name.IsEmpty())
          name = "SerialEMcrash";
        winApp->mLogWindow->UpdateSaveFile(true, name);
      } else {
        winApp->mLogWindow->DoSave();
      }
      if (!(winApp->mLogWindow->GetLastFilePath()).IsEmpty())
        message += "\n\nThe log has been saved to:\n" +
        winApp->mLogWindow->GetLastFilePath();
    }

    // Try to close files and rescue HDF
    winApp->mDocWnd->CloseAllStores();
  }
  catch (...) {
  }
  if (winApp->LowDoseMode())
    message += "\r\n\r\nWill try to leave low dose mode before closing...";
  AfxMessageBox(message);
  if (winApp->LowDoseMode())
    winApp->mLowDoseDlg.SetLowDoseMode(false);
  first = false;
  return EXCEPTION_EXECUTE_HANDLER;
}

// Winsock initialization is invoked by Mailer and by any socket class initialization
// and happens only once.  The program uninitializes once at end with the function in
// BaseSocket
// Insist on at least Winsock v1.1
const unsigned int VERSION_MAJOR = 1;
const unsigned int VERSION_MINOR = 1;

int SEMInitializeWinsock(void)
{
  WSADATA WSData;
  CString report;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (CBaseSocket::GetWSAinitialized())
    return 0;

  // Attempt to initialize WinSock (1.1 or later). 
  if (WSAStartup(MAKEWORD(VERSION_MAJOR, VERSION_MINOR), &WSData)) {
    report.Format("SEMInitializeWinsock: Cannot find Winsock v%d.%d or later!", 
      VERSION_MAJOR, VERSION_MINOR);
    winApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    return 1;
  } 
  CBaseSocket::SetWSAinitialized(true);
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
  CAboutDlg();

// Dialog Data
  //{{AFX_DATA(CAboutDlg)
  enum { IDD = IDD_ABOUTBOX };
  //}}AFX_DATA

  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CAboutDlg)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

// Implementation
protected:
  //{{AFX_MSG(CAboutDlg)
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
public:
  CString m_strVersion;
  CString m_strTag;
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
, m_strVersion(_T(sAboutVersion))
, m_strTag(_T(TAG_STRING))
{
  //{{AFX_DATA_INIT(CAboutDlg)
  //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAboutDlg)
  //}}AFX_DATA_MAP
  DDX_Text(pDX, IDC_STATVERSION, m_strVersion);
  DDX_Text(pDX, IDC_STATTAG, m_strTag);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
  //{{AFX_MSG_MAP(CAboutDlg)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CSerialEMApp::OnAppAbout()
{
  CAboutDlg aboutDlg;
  aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// FUNCTIONS RELATED TO INTERACTIONS WITH CSERIALEMVIEW

void CSerialEMApp::SetCurrentBuffer(int index, bool fftBuf)
{
  if (fftBuf) {
    if (mFFTView) {
      mFFTView->SetCurrentBuffer(index);
    } else {
      mNeedFFTWindow = true;
      DoResizeMain();
      ViewOpening();
      mMainFrame->OnWindowNew();
    }
  } else
    mMainView->SetCurrentBuffer(index);
}

void CSerialEMApp::SetImBufIndex(int inIndex, bool fftBuf)
{
  if (fftBuf)
    mFFTbufIndex = inIndex;
  else
    mImBufIndex = inIndex;
}


// Tell a new view about its size and image buffer to use
int CSerialEMApp::GetNewViewProperties(CSerialEMView *inView, int &iBordLeft, 
                                       int &iBordTop, int &iBordRight, int &iBordBottom,
                                       EMimageBuffer *&imBufs, int &iImBufNumber, 
                                       int &iImBufIndex)
{
  CRect rect;
  int userRight, userBot, fullWidth, needStatic = mNeedStaticWindow ? 1 : 0;
  float dpiScale = GetScalingForDPI();
  int nonStatAddBot = 25;
  iBordLeft = STATIC_BORDER_LEFT;
  iBordRight = STATIC_BORDER_RIGHT;
  iBordTop = STATIC_BORDER_TOP;
  iBordBottom = STATIC_BORDER_BOTTOM;
  
  // If this is the static window, send border sizes
  // If it is not the static window, iBordRight and iBordBottom are used as sizes
  if (needStatic) {
    imBufs = &mImBufs[0];
    iImBufNumber = MAX_BUFFERS;
    iImBufIndex = mImBufIndex;
    mNeedStaticWindow = FALSE;
  } else if (mNeedFFTWindow) {
    m_pMainWnd->GetWindowRect(rect);
    fullWidth = rect.Width() - mMaxDialogWidth - mRightFrameWidth;
    if (CountOpenViews() > 1)
      iBordRight += 30;
    userRight = B3DNINT(mRightBorderFrac * fullWidth);
    iBordRight = B3DMAX(iBordRight, userRight);
    iBordLeft = mMaxDialogWidth + B3DNINT(mMainFFTsplitFrac * (fullWidth - iBordRight));
    userBot = B3DNINT(mBottomBorderFrac * (rect.Height() - mBottomFrameWidth));
    iBordBottom = B3DMAX(iBordBottom, userBot);
    imBufs = &mFFTBufs[0];
    iImBufNumber = MAX_FFT_BUFFERS;
    iImBufIndex = 0;
    needStatic = 3;
    mFFTView = inView;
    mNeedFFTWindow = false;
  } else if (mStackViewImBuf) {

    // If we need a stack view window, send the imbuf and send size / 2
    // Here, store the view pointer; main view pointer was deferred until active signal
    imBufs = mStackViewImBuf;
    iBordLeft = mMaxDialogWidth;
    if (mMultiTSTasks->GetAssessingRange()) {
      iBordRight = B3DNINT(256 * dpiScale);
      iBordBottom = B3DNINT((256 + nonStatAddBot) * dpiScale);
    } else {
      iBordRight = B3DNINT(dpiScale * imBufs->mImage->getWidth() / 2);
      iBordBottom = B3DNINT(dpiScale * imBufs->mImage->getHeight() / 2 + nonStatAddBot);
    }
    iImBufNumber = 1;
    iImBufIndex = 0;
    needStatic = 2;
    mStackViewImBuf = NULL;
    mStackView = inView;
  } else {

    // Otherwise copy active buffer and send a size
    iBordLeft = mMaxDialogWidth;
    iBordRight = B3DNINT(512 * dpiScale);
    iBordBottom = B3DNINT(512 * dpiScale + nonStatAddBot);
    imBufs = new EMimageBuffer;
    EMimageBuffer *fromBuf = mActiveView->GetActiveImBuf();
    mBufferManager->CopyImBuf(fromBuf, imBufs);
    if (fromBuf->mImage) {
      int width = B3DNINT(dpiScale * fromBuf->mImage->getWidth());
      int height = B3DNINT(dpiScale * fromBuf->mImage->getHeight() + nonStatAddBot);
      if (width < iBordRight)
        iBordRight = width;
      if (height < iBordBottom)
        iBordBottom = height;
    }
    iImBufNumber = 1;
    iImBufIndex = 0;
  }
  return needStatic;
}

void CSerialEMApp::RestoreViewFocus()
{
  if (mActiveView)
    mActiveView->SetFocus();
}

// Retain a pointer to the active view, and the first time, store
// the pointer to the main view
void CSerialEMApp::SetActiveView(CSerialEMView *inActiveView)
{
  
  mActiveView = inActiveView;
  if (mMainView == NULL) {
    mMainView = inActiveView;
    mMainFrame->SetDialogOffset(mMainView);   
  }
}

// Return the rectange and dialog offset of the main frame for positioning clients
void CSerialEMApp::GetMainRect(CRect * rect, int & dialogOffset)
{
  dialogOffset = mMaxDialogWidth;
  mMainFrame->GetWindowRect(rect);
}

// Add a buffer to the stack view; first time, set pointer and open window
int CSerialEMApp::AddToStackView(EMimageBuffer * imBuf, int angleOrder)
{
  if (mStackView) {
    return mStackView->AddBufferToStack(imBuf, angleOrder);
  }
  mStackViewImBuf = imBuf;
  ViewOpening();
  mMainFrame->OnWindowNew();
  return 0;
}

// When a view is closing, if it is stack view clear out pointer, set flag, resize
void CSerialEMApp::ViewClosing(BOOL stackView, BOOL FFTview)
{
  int nview;
  if (stackView) {
    if (mStackView && mMultiTSTasks->mRangeViewer) {
      mMultiTSTasks->mRangeViewer->DestroyWindow();
      mMultiTSTasks->mRangeViewer = NULL;
    }
    mStackView = NULL;
  }
  if (FFTview) {
    mFFTView = NULL;
    for (nview = 0; nview < MAX_FFT_BUFFERS; nview++) {
      mFFTBufs[nview].DeleteImage();
      mFFTBufs[nview].DeleteOffsets();
      delete mFFTBufs[nview].mPixMap;
      mFFTBufs[nview].mPixMap = NULL;
    }
    mBufferWindow.UpdateSaveCopy();
  }
  mViewClosing = true;
  nview = CountOpenViews();
  if (mMainView && (nview < 2 + B3DCHOICE(mFFTView, 1, 0) || FFTview) && !mAppExiting)
    DoResizeMain();
  mViewClosing = false;
}

// Stop keeping track of a stack view, tell it so
void CSerialEMApp::DetachStackView(void)
{
  if (mStackView)
    mStackView->StopBeingActiveStack();
  mStackView = NULL;
}

// To resize the stack view with given borders from top and left
void CSerialEMApp::ResizeStackView(int extraBordLeft, int bordTop)
{
  if (mStackView)
    mStackView->ResizeToFit(mMaxDialogWidth + extraBordLeft, STATIC_BORDER_TOP + bordTop,
      STATIC_BORDER_RIGHT, STATIC_BORDER_BOTTOM, 0);
}

// This is the handler for the resize menu entry/shortcut, which resets the border
// fractions to resize to the full area
void CSerialEMApp::OnResizeMain() 
{
  mRightBorderFrac = 0.;
  mBottomBorderFrac = 0.;
  DoResizeMain();
}

// Tell the main window to resize itself to fill space; come in a bit if other windows
// whichBuf is 0 to do both, -1 for just main, 1 for just FFT
void CSerialEMApp::DoResizeMain(int whichBuf) 
{
  // ResizeToFit crashes on very small screens (<= 800x600) on some systems so skip during
  // startup and do at very end of startup
  if (mStartingProgram || mMainFrame->GetClosingProgram())
    return;
  CRect rect;
  m_pMainWnd->GetWindowRect(rect);
  int nview = CountOpenViews();
  int ifFFT = B3DCHOICE(mFFTView || mNeedFFTWindow, 1, 0);
  int right = STATIC_BORDER_RIGHT + B3DCHOICE(nview > 1 + ifFFT, 30, 0);
  int userRight = B3DNINT(mRightBorderFrac * (rect.Width() - mMaxDialogWidth - 
    mRightFrameWidth));
  int userBottom = B3DNINT(mBottomBorderFrac * (rect.Height() - mBottomFrameWidth));
  userBottom = B3DMAX(userBottom, STATIC_BORDER_BOTTOM);
  right = B3DMAX(userRight, right);
  if (whichBuf <= 0)
    mMainView->ResizeToFit(mMaxDialogWidth, STATIC_BORDER_TOP, right, userBottom, -ifFFT);
  if (mFFTView && whichBuf >= 0)
    mFFTView->ResizeToFit(mMaxDialogWidth, STATIC_BORDER_TOP, right, userBottom, 1);
}

// Either the main window or FFT window is resizing to the given rectangle: adjust border
// fractions and possible the split between the two windows
void CSerialEMApp::MainViewResizing(CRect &winRect, bool FFTwin)
{
  CRect rect;
  float usableWidth;
  m_pMainWnd->GetWindowRect(rect);
  if (mStartingProgram) {
    mRightFrameWidth = B3DMIN(30, rect.right - winRect.right);
    mBottomFrameWidth = B3DMIN(100, rect.bottom - winRect.bottom);
  } else {
    usableWidth = (float)(rect.Width() - mMaxDialogWidth);

    // If FFT open, adjust right fraction only if this is FFT resizing
    if (!mFFTView || FFTwin)
      mRightBorderFrac = (float)(rect.right - mRightFrameWidth - winRect.right) / 
        usableWidth;
    mBottomBorderFrac = (float)(rect.bottom - mBottomFrameWidth - winRect.bottom) / 
      (float)rect.Height();
    B3DCLAMP(mRightBorderFrac, 0.f, 0.9f);
    B3DCLAMP(mBottomBorderFrac, 0.f, 0.9f);

    // If FFT open, adjust the split between main and FFT and resize windows
    if (mFFTView) {
      mMainFFTsplitFrac = (float)((B3DCHOICE(FFTwin, winRect.left, winRect.right) - 
        (rect.left + mMaxDialogWidth)) / ((1. - mRightBorderFrac) * usableWidth));
      B3DCLAMP(mMainFFTsplitFrac, 0.1f, 0.9f);

      // This resizes the currently resizing window, but seems to work and results in
      // both windows having their zooms adjusted instead of just one
      DoResizeMain(0);
    }
  }
}

int CSerialEMApp::CountOpenViews(void)
{
  int nview = (mViewOpening ? 1 : 0) - (mViewClosing ? 1 : 0);
  if (mDocWnd) {
    POSITION pos = mDocWnd->GetFirstViewPosition();
    while (pos != NULL) {
      mDocWnd->GetNextView(pos);
        nview++;
    }
  }
  return nview;
}

// Function to call resize when window is opening
void CSerialEMApp::ViewOpening(void)
{
  mViewOpening = true;
  DoResizeMain();
  mViewOpening = false;
}

// Return active image buffer, but if active window is the stack view, use main view
EMimageBuffer *CSerialEMApp::GetActiveNonStackImBuf(void)
{
  if (mStackView == mActiveView || mFFTView == mActiveView)
    return mMainView->GetActiveImBuf();
  return mActiveView->GetActiveImBuf();
}

/////////////////////////////////////////////////////////////////////////////
// IDLE TASK MANAGER

// Add a task to the list for OnIdle to watch
void CSerialEMApp::AddIdleTask(int (__cdecl *busyFunc)(void), void (__cdecl *nextFunc)(int),
                 void (__cdecl *errorFunc)(int), int param, int timeOut)
{
  AddIdleTask(busyFunc, nextFunc, errorFunc, 0, param, timeOut);
}

void CSerialEMApp::AddIdleTask(int (__cdecl *busyFunc)(void), int source, int param, 
                               int timeOut)
{
  AddIdleTask(busyFunc, NULL, NULL, source, param, timeOut);
}

void CSerialEMApp::AddIdleTask(int source, int param, int timeOut)
{
  AddIdleTask(NULL, NULL, NULL, source, param, timeOut);
}

void CSerialEMApp::AddIdleTask(int (__cdecl *busyFunc)(void), void (__cdecl *nextFunc)(int),
                 void (__cdecl *errorFunc)(int), int source, int param, int timeOut)
{
  IdleCallBack *idc = new IdleCallBack;
  idc->busyFunc = busyFunc;
  idc->nextFunc = nextFunc;
  idc->errorFunc = errorFunc;
  idc->source = source;
  idc->param = param;
  idc->extendTimeOut = timeOut > 0;
  mLastCheckTime = GetTickCount();
  if (timeOut)
    idc->timeOut = mShiftManager->AddIntervalToTickTime(mLastCheckTime, B3DABS(timeOut));
  else
    idc->timeOut = 0;
  mIdleArray.Add(idc);

  // Start the timer if necessary if not using the idle processing
#ifdef TASK_TIMER_INTERVAL
  if (!mMainFrame->NewTask())
    AfxMessageBox("Failed to start timer to monitor completion of tasks",
      MB_EXCLAME);
#endif
}

// Remove an idle task from the list
void CSerialEMApp::RemoveIdleTask(int source)
{
  IdleCallBack *idc;
  for (int i = 0; i < mIdleArray.GetSize(); i++) {
    idc = mIdleArray[i];
    if (idc->source == source) {
      mIdleArray.RemoveAt(i);
      delete idc;
      i--;
    }
  }
}


// Check the idle tasks, return true if any are still running
BOOL CSerialEMApp::CheckIdleTasks()
{
  BOOL bRtn = FALSE;
  int i, busy = 0;
  IdleCallBack *idc;
  DWORD time = GetTickCount();
  static DWORD maxInt = 0;
  DWORD interval = 0;

  // If there is anything in array, get the interval since the last check
  // If the interval is greater than a threshold, add it to the timeouts
  if (mIdleArray.GetSize()) {
    interval = (DWORD)SEMTickInterval((UINT)time, (UINT)mLastCheckTime);
    if (interval > maxInt)
      maxInt = interval;
    mLastCheckTime = time;
    if (interval < mCheckThreshold)
      interval = 0;
  }

  // First check for com errors and have program act on them
  if (sThreadError) {
    ErrorOccurred(sThreadError);
    sThreadError = 0;
  }

  if (mIdleArray.GetSize())
    mLastActivityTime = time;
  
  // Look through the list of tasks if any
  for (i = 0; i < mIdleArray.GetSize(); i++) {
    // Return TRUE if there is anything on list
    bRtn = TRUE;
    idc = mIdleArray[i];
    busy = 0;
    if (idc->busyFunc)
      busy = (*(idc->busyFunc))();
    else if (idc->source == TASK_TILT_SERIES)
      busy = mTSController->TiltSeriesBusy();
    else if (idc->source == TASK_MACRO_RUN)
      busy = mMacroProcessor->TaskBusy();
    else if (idc->source == TASK_MONTAGE_FOCUS || idc->source == TASK_CAL_ASTIG ||
      idc->source == TASK_FIX_ASTIG || idc->source == TASK_COMA_FREE ||
      idc->source == TASK_CTF_BASED)
      busy = mFocusManager->DoingFocus() ? 1 : 0;
    else if (idc->source == TASK_CAL_COMA_VS_IS)
      busy = mAutoTuning->GetDoingCtfBased() ? 1 : 0;
    else if (idc->source == TASK_MONTAGE_DWELL)
      busy = mMontageController->CookingDwellBusy();
    else if (idc->source == TASK_DEL_OTHER_STORE || idc->source == TASK_LOAD_MAP)
      busy = mMontageController->DoingMontage();
    else if (idc->source == TASK_COOKER)
      busy = mMultiTSTasks->TaskCookerBusy();
    else if (idc->source == TASK_TILT_RANGE)
      busy = mMultiTSTasks->TiltRangeBusy();
    else if (idc->source == TASK_CONDITION_VPP)
      busy = mMultiTSTasks->VppConditionBusy();
    else if (idc->source == TASK_EUCENTRICITY)
      busy = mComplexTasks->EucentricityBusy();
    else if (idc->source == TASK_FOCUS_VS_Z)
      busy = mFocusManager->FocusVsZBusy();
    else if (idc->source == TASK_DUAL_MAP)
      busy = mNavHelper->DualMapBusy();
    else if (idc->source == TASK_NAVIGATOR_ACQUIRE && mNavigator)
      busy = mNavigator->TaskAcquireBusy();
    else if (idc->source == TASK_ASYNC_SAVE)
      busy = mBufferManager->AsyncSaveBusy();
    else if (idc->source == TASK_GAIN_REF)
      busy = mGainRefMaker->GainRefBusy();
    else if (idc->source == TASK_LONG_OPERATION)
      busy = mScope->LongOperationBusy();
    else if (idc->source == TASK_STACK_FALCON)
      busy = mFalconHelper->StackingWaiting();
    else if (idc->source == TASK_START_NAV_ACQ)
      busy = mMacroProcessor->StartNavAvqBusy();
    else if (idc->source == TASK_MULTI_SHOT)
      busy = mParticleTasks->MultiShotBusy();
    else if (idc->source == TASK_MOVE_APERTURE)
      busy = mScope->ApertureBusy();
    else if (idc->source == TASK_WAIT_FOR_DRIFT)
      busy = mParticleTasks->WaitForDriftBusy();
    else if (idc->source == TASK_MACRO_AT_EXIT)
      busy = mMacroProcessor->DoingMacro() ? 1 : 0;

    // Increase timeouts after long intervals
    if (idc->extendTimeOut)
      idc->timeOut = mShiftManager->AddIntervalToTickTime(idc->timeOut, interval);

    // If not busy or the time is up
    if (busy <= 0 || (idc->timeOut && SEMTickInterval(idc->timeOut, (UINT)time) < 0)) {

        // Delete task from array now, to prevent multiple deletion
        mIdleArray.RemoveAt(i);

      // Otherwise, if OK, call next func with parameter, 
      // or if error, call error function with return code if it exists
      // or if timeout, call error function with that code
      if (!busy) {
        if (idc->nextFunc != NULL)
          (*(idc->nextFunc))(idc->param);
        else if (idc->source == TASK_NAVIGATOR_ACQUIRE && mNavigator)
          mNavigator->AcquireNextTask(idc->param);
        else if (idc->source == TASK_LOAD_MAP && mNavigator)
          mNavigator->FinishLoadMap();
        else if (idc->source == TASK_NAV_REALIGN)
          mNavHelper->RealignNextTask(idc->param);
        else if (idc->source == TASK_DUAL_MAP)
          mNavHelper->DualMapDone(idc->param);
        else if (idc->source == TASK_DISTORTION_STAGEPAIR)
          mDistortionTasks->SPNextTask(idc->param);
        else if (idc->source == TASK_CAL_BEAMSHIFT)
          mBeamAssessor->ShiftCalImage();
        else if (idc->source == TASK_REFINE_BS_CAL)
          mBeamAssessor->RefineShiftCalImage();
        else if (idc->source == TASK_CCD_CAL_INTENSITY)
          mBeamAssessor->CalIntensityImage(idc->param);
        else if (idc->source == TASK_CAL_SPOT_INTENSITY)
          mBeamAssessor->SpotCalImage(idc->param);
        else if (idc->source == TASK_CAL_IA_LIMITS)
          mBeamAssessor->CalIllumAreaNextTask();
        else if (idc->source == TASK_RESET_REALIGN)
          mComplexTasks->RSRANextTask(idc->param);
        else if (idc->source == TASK_WALKUP)
          mComplexTasks->WalkUpNextTask(idc->param);
        else if (idc->source == TASK_REVERSE_TILT)
          mComplexTasks->ReverseTiltNextTask(idc->param);
        else if (idc->source == TASK_EUCENTRICITY)
          mComplexTasks->EucentricityNextTask(idc->param);
        else if (idc->source == TASK_TILT_AFTER_MOVE)
          mComplexTasks->TASMNextTask(idc->param);
        else if (idc->source == TASK_BACKLASH_ADJUST)
          mComplexTasks->BASPNextTask(idc->param);
        else if (idc->source == TASK_COOKER)
          mMultiTSTasks->CookerNextTask(idc->param);
        else if (idc->source == TASK_TILT_RANGE)
          mMultiTSTasks->TiltRangeNextTask(idc->param);
        else if (idc->source == TASK_TEST_AUTOCEN)
          mMultiTSTasks->AutocenTestAcquireDone();
        else if (idc->source == TASK_AUTOCEN_BEAM)
          mMultiTSTasks->AutocenNextTask(idc->param);
        else if (idc->source == TASK_CONDITION_VPP)
          mMultiTSTasks->VppConditionNextTask(idc->param);
        else if (idc->source == TASK_REMOTE_CTRL)
          mRemoteControl.TaskDone(idc->param);
        else if (idc->source == TASK_MULTI_SHOT)
          mParticleTasks->MultiShotNextTask(idc->param);
        else if (idc->source == TASK_WAIT_FOR_DRIFT)
          mParticleTasks->WaitForDriftNextTask(idc->param);
        else if (idc->source == TASK_RESET_SHIFT)
          mShiftManager->ResetISDone();
        else if (idc->source == TASK_CAL_IMAGESHIFT)
          mShiftCalibrator->ShiftDone();
        else if (idc->source == TASK_CAL_CAM_TIMING)
          mCalibTiming->CalTimeNextTask();
        else if (idc->source == TASK_CAL_DEAD_TIME)
          mCalibTiming->CalDeadNextTask();
        else if (idc->source == TASK_CAL_MAGSHIFT)
          mFilterTasks->CalMagShiftNextTask();
        else if (idc->source == TASK_REFINE_ZLP)
          mFilterTasks->RefineZLPNextTask();
        else if (idc->source == TASK_CAL_IS_NEUTRAL)
          mScope->CalNeutralNextMag(idc->param);
        else if (idc->source == TASK_MONTAGE || idc->source == TASK_MONTAGE_FOCUS ||
          idc->source == TASK_MONTAGE_DWELL)
          mMontageController->DoNextPiece(idc->param);
        else if (idc->source == TASK_MONTAGE_RESTORE)
          mMontageController->StageRestoreDone();
        else if (idc->source == TASK_MONTAGE_REALIGN)
          mMontageController->RealignNextTask(idc->param);
        else if (idc->source == TASK_TILT_SERIES)
          mTSController->NextAction(idc->param);
        else if (idc->source == TASK_MACRO_RUN) {
          do {
            mMacroProcessor->TaskDone(idc->param);
          } while (mMacroProcessor->GetLoopInOnIdle());
        }  else if (idc->source == TASK_ACQUIRE_RESETUP)
          ReopenCameraSetup();
        else if (idc->source == TASK_SET_CAMERA_NUM)
          SetActiveCameraNumber(idc->param);
        else if (idc->source == TASK_DEL_OTHER_STORE)
          mBufferManager->DeleteOtherStore();
        else if (idc->source == TASK_FILM_EXPOSURE)
          mScope->FilmCleanup(0);
        else if (idc->source == TASK_STEM_FOCUS)
          mFocusManager->STEMfocusShot(idc->param);
        else if (idc->source == TASK_FOCUS_VS_Z)
          mFocusManager->FocusVsZnextTask(idc->param);
        else if (idc->source == TASK_INTERSET_SHIFT)
          mShiftCalibrator->InterSetShiftNextTask(idc->param);
        else if (idc->source == TASK_ASYNC_SAVE)
          mBufferManager->AsyncSaveDone();
        else if (idc->source == TASK_BIDIR_COPY)
          mMultiTSTasks->StartBidirFileCopy(true);
        else if (idc->source == TASK_BIDIR_ANCHOR)
          mMultiTSTasks->BidirAnchorNextTask(idc->param);
        else if (idc->source == TASK_STAGE_TOOL && mStageMoveTool)
          mStageMoveTool->StageToolNextTask();
        else if (idc->source == TASK_STACK_FALCON)
          mFalconHelper->StackNextTask(idc->param);
        else if (idc->source == TASK_ALIGN_DE_FRAMES)
          mFalconHelper->AlignNextFrameTask(idc->param);
        else if (idc->source == TASK_CAL_ASTIG)
          mAutoTuning->AstigCalNextTask(idc->param);
        else if (idc->source == TASK_FIX_ASTIG)
          mAutoTuning->FixAstigNextTask(idc->param);
        else if (idc->source == TASK_COMA_FREE)
          mAutoTuning->ComaFreeNextTask(idc->param);
        else if (idc->source == TASK_ZEMLIN)
          mAutoTuning->ZemlinNextTask(idc->param);
        else if (idc->source == TASK_CTF_BASED)
          mAutoTuning->CtfBasedNextTask(idc->param);
        else if (idc->source == TASK_CAL_COMA_VS_IS)
          mAutoTuning->ComaVsISNextTask(idc->param);
        else if (idc->source == TASK_GAIN_REF)
          mGainRefMaker->AcquiringRefNextTask(idc->param);
        else if (idc->source == TASK_FIND_HOLES)
          mNavHelper->mHoleFinderDlg->ScanningNextTask(idc->param);
        else if (idc->source == TASK_MACRO_AT_EXIT)
          mMainFrame->DoClose(true);

      } else {
        if (busy > 0 && idc->timeOut && (idc->timeOut <= time))
          busy = IDLE_TIMEOUT_ERROR;
        if (idc->errorFunc != NULL)
          (*(idc->errorFunc))(busy);
        else if (idc->source == TASK_NAVIGATOR_ACQUIRE && mNavigator)
          mNavigator->AcquireCleanup(busy);
        else if (idc->source == TASK_NAV_REALIGN)
          mNavHelper->RealignCleanup(busy);
        else if (idc->source == TASK_DUAL_MAP)
          mNavHelper->DualMapCleanup(busy);
        else if (idc->source == TASK_DISTORTION_STAGEPAIR)
          mDistortionTasks->SPCleanUp(busy);
        else if (idc->source == TASK_CAL_BEAMSHIFT || idc->source == TASK_REFINE_BS_CAL)
          mBeamAssessor->ShiftCalCleanup(busy);
        else if (idc->source == TASK_CCD_CAL_INTENSITY)
          mBeamAssessor->CalIntensityCCDCleanup(busy);
        else if (idc->source == TASK_CAL_SPOT_INTENSITY)
          mBeamAssessor->SpotCalCleanup(busy);
        else if (idc->source == TASK_RESET_REALIGN)
          mComplexTasks->RSRACleanup(busy);
        else if (idc->source == TASK_WALKUP)
          mComplexTasks->WalkUpCleanup(busy);
        else if (idc->source == TASK_REVERSE_TILT)
          mComplexTasks->ReverseTiltCleanup(busy);
        else if (idc->source == TASK_EUCENTRICITY)
          mComplexTasks->EucentricityCleanup(busy);
        else if (idc->source == TASK_TILT_AFTER_MOVE)
          mComplexTasks->TASMCleanup(busy);
        else if (idc->source == TASK_BACKLASH_ADJUST)
          mComplexTasks->BASPCleanup(busy);
        else if (idc->source == TASK_COOKER)
          mMultiTSTasks->CookerCleanup(busy);
        else if (idc->source == TASK_TILT_RANGE)
          mMultiTSTasks->TiltRangeCleanup(busy);
        else if (idc->source == TASK_AUTOCEN_BEAM)
          mMultiTSTasks->AutocenCleanup(busy);
        else if (idc->source == TASK_CONDITION_VPP)
          mMultiTSTasks->VppConditionCleanup(busy);
        else if (idc->source == TASK_REMOTE_CTRL)
          mRemoteControl.TaskCleanup(busy);
        else if (idc->source == TASK_MULTI_SHOT)
          mParticleTasks->MultiShotCleanup(busy);
        else if (idc->source == TASK_WAIT_FOR_DRIFT)
          mParticleTasks->WaitForDriftCleanup(busy);
        else if (idc->source == TASK_RESET_SHIFT)
          mShiftManager->ResetISCleanup(busy);
        else if (idc->source == TASK_CAL_IMAGESHIFT)
          mShiftCalibrator->ShiftCleanup(busy);
        else if (idc->source == TASK_CAL_CAM_TIMING)
          mCalibTiming->CalTimeCleanup(busy);
        else if (idc->source == TASK_CAL_DEAD_TIME)
          mCalibTiming->CalDeadCleanup(busy);
        else if (idc->source == TASK_CAL_MAGSHIFT)
          mFilterTasks->CalMagShiftCleanup(busy);
        else if (idc->source == TASK_REFINE_ZLP)
          mFilterTasks->RefineZLPCleanup(busy);
        else if (idc->source == TASK_MONTAGE || idc->source == TASK_MONTAGE_RESTORE ||
          idc->source == TASK_MONTAGE_FOCUS || idc->source == TASK_MONTAGE_REALIGN || 
          idc->source == TASK_MONTAGE_DWELL)
          mMontageController->PieceCleanup(busy);
        else if (idc->source == TASK_TILT_SERIES)
          mTSController->TiltSeriesError(busy);
        else if (idc->source == TASK_FILM_EXPOSURE)
          mScope->FilmCleanup(busy);
        else if (idc->source == TASK_STEM_FOCUS)
          mFocusManager->STEMfocusCleanup(busy);
        else if (idc->source == TASK_FOCUS_VS_Z)
          mFocusManager->FocusVsZcleanup(busy);
        else if (idc->source == TASK_INTERSET_SHIFT)
          mShiftCalibrator->InterSetShiftCleanup(busy);
        else if (idc->source == TASK_ASYNC_SAVE)
          mBufferManager->AsyncSaveCleanup(busy);
        else if (idc->source == TASK_BIDIR_ANCHOR)
          mMultiTSTasks->BidirAnchorCleanup(busy);
        else if (idc->source == TASK_CAL_ASTIG)
          mAutoTuning->AstigCalCleanup(idc->param);
        else if (idc->source == TASK_FIX_ASTIG)
          mAutoTuning->FixAstigCleanup(idc->param);
        else if (idc->source == TASK_COMA_FREE)
          mAutoTuning->ComaFreeCleanup(idc->param);
        else if (idc->source == TASK_ZEMLIN)
          mAutoTuning->ZemlinCleanup(idc->param);
        else if (idc->source == TASK_CTF_BASED)
          mAutoTuning->CtfBasedCleanup(idc->param);
        else if (idc->source == TASK_GAIN_REF)
          mGainRefMaker->ErrorCleanup(busy);
        else if (idc->source == TASK_MOVE_APERTURE)
          mScope->ApertureCleanup(busy);
        else if (idc->source == TASK_FIND_HOLES)
          mNavHelper->mHoleFinderDlg->StopScanning();
     }

      // Delete task from memory and drop index
      delete idc;
      i--;

      // Try only completing one task each time through
      break;
    }
  }

  ManageBlinkingPane(time);

  // Dump debug output to log window
  if (debugOutput && 
    (WaitForSingleObject(traceMutexHandle, TRACE_MUTEX_WAIT) == WAIT_OBJECT_0)) {
    for (i = 0; i < sNumTraceMsg; i++)
      AppendToLog(sTraceMsg[i], LOG_OPEN_IF_CLOSED);
    sNumTraceMsg = 0;
    ReleaseMutex(traceMutexHandle);
  }

  return bRtn;
}

// On idle entry if timer not being used
BOOL CSerialEMApp::OnIdle(LONG lCount) 
{
  BOOL bIdle = CWinApp::OnIdle(lCount);
#ifdef TASK_TIMER_INTERVAL
  return bIdle;
#else
  // Let the base class finish its tasks, as recommended usage
  // Except occasionally, sneak a cycle 
  // (but when camera acquire is already done, it sets the count high to go right away)
  mIdleBaseCount++;
  if (bIdle && mIdleBaseCount < 100)
    return TRUE;
  mIdleBaseCount = 0;

  // Sleeping 1 is enough to keep it from eating lots of CPU during acquires, the average
  // tick count change during the sleep is only 1 ms longer than the sleep time
  Sleep(2);
  return CheckIdleTasks() || bIdle;
#endif
}

// A global convenience routine to check if stage or camera are busy
int SEMStageCameraBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  int stage = winApp->mScope->StageBusy();
  if (stage < 0)
    return -1;
  return (stage > 0 || winApp->mCamera->CameraBusy());
}

///////////////////////////////////////////////////////////////////////////
// ERROR REPORTING AND HANDLING

// Central reporting location for error, so that complex tasks can be stopped
void CSerialEMApp::ErrorOccurred(int error)
{
  if (mMacroProcessor->DoingMacro())
    mMacroProcessor->Stop(true);
  if (mShiftCalibrator->CalibratingIS())
    mShiftCalibrator->StopCalibrating();
  if (mShiftCalibrator->CalibratingOffset())
    mShiftCalibrator->CalISoffsetDone(true);
  if (mShiftCalibrator->DoingInterSetShift())
    mShiftCalibrator->StopInterSetShift();
  if (mFocusManager->DoingFocus())
    mFocusManager->StopFocusing();
  if (mFocusManager->DoingSTEMfocus())
    mFocusManager->StopSTEMfocus();
  if (mFocusManager->DoingSTEMfocusVsZ())
    mFocusManager->StopFocusVsZ();
  if (mMontageController->DoingMontage())
    mMontageController->StopMontage(error);
  if (mComplexTasks->DoingResetShiftRealign())
    mComplexTasks->StopResetShiftRealign();
  if (mComplexTasks->DoingWalkUp())
    mComplexTasks->StopWalkUp();
  if (mComplexTasks->ReversingTilt())
    mComplexTasks->StopReverseTilt();
  if (mComplexTasks->DoingEucentricity())
    mComplexTasks->StopEucentricity();
  if (mComplexTasks->DoingTiltAfterMove())
    mComplexTasks->StopTiltAfterMove();
  if (mComplexTasks->DoingBacklashAdjust())
    mComplexTasks->StopBacklashAdjust();
  if (mMultiTSTasks->GetCooking())
    mMultiTSTasks->StopCooker();
  if (mMultiTSTasks->GetAssessingRange())
    mMultiTSTasks->StopTiltRange();
  if (mMultiTSTasks->GetAutoCentering())
    mMultiTSTasks->StopAutocen();
  if (mMultiTSTasks->GetConditioningVPP())
    mMultiTSTasks->StopVppCondition();
  if (mShowRemoteControl && mRemoteControl.GetDoingTask())
    mRemoteControl.TaskDone(0);
  if (mBeamAssessor->CalibratingIntensity())
    mBeamAssessor->StopCalibratingIntensity();
  if (mBeamAssessor->CalibratingBeamShift())
    mBeamAssessor->StopShiftCalibration();
  if (mBeamAssessor->CalibratingSpotIntensity())
    mBeamAssessor->StopSpotCalibration();
  if (mBeamAssessor->CalibratingIAlimits())
    mBeamAssessor->StopCalIllumAreaLimits();
  if (mFilterTasks->CalibratingMagShift())
    mFilterTasks->StopCalMagShift();
  if (mFilterTasks->RefiningZLP())
    mFilterTasks->StopRefineZLP();
  if (mGainRefMaker->AcquiringGainRef())
    mGainRefMaker->StopAcquiringRef();
  if (mDistortionTasks->DoingStagePairs())
    mDistortionTasks->StopStagePairs();
  if (mNavigator && mNavigator->GetAcquiring())
    mNavigator->StopAcquiring(true);
  if (mNavHelper->GetRealigning())
    mNavHelper->StopRealigning();
  if (mNavHelper->GetAcquiringDual())
    mNavHelper->StopDualMap();
  if (mCalibTiming->Calibrating())
    mCalibTiming->StopCalTiming();
  if (mCalibTiming->DoingDeadTime())
    mCalibTiming->StopCalDead();
  if (mScope->CalibratingNeutralIS())
    mScope->StopCalNeutralIS();
  if (DoingTiltSeries())
    mTSController->ExternalStop(error);
  if (mScope->GetMovingStage())
    CEMscope::TaskStageDone(0);
  if ((mMultiTSTasks->DoingBidirCopy() < 0 && !mCameraMacroTools.GetUserStop()) ||
    mMultiTSTasks->DoingBidirCopy() > 0)
    mMultiTSTasks->StopBidirFileCopy();
  if (mMultiTSTasks->DoingAnchorImage())
    mMultiTSTasks->StopAnchorImage();
  if (mParticleTasks->DoingMultiShot())
    mParticleTasks->StopMultiShot();
  if (mParticleTasks->GetWaitingForDrift())
    mParticleTasks->StopWaitForDrift();
  if (mStageMoveTool && mStageMoveTool->GetGoingToAcquire())
    mStageMoveTool->StopNextAcquire();
  if (mNavHelper->mHoleFinderDlg->GetFindingHoles())
    mNavHelper->mHoleFinderDlg->StopScanning();
  if (mCamera->GetWaitingForStacking())
    mCamera->SetWaitingForStacking(-1);
  if (mScope->GetDoingLongOperation() && mCameraMacroTools.GetUserStop())
    mScope->StopLongOperation(false);
  if (mAutoTuning->GetDoingCalAstig())
    mAutoTuning->StopAstigCal();
  if (mAutoTuning->GetDoingFixAstig())
    mAutoTuning->StopFixAstig();
  if (mAutoTuning->GetDoingComaFree())
    mAutoTuning->StopComaFree();
  if (mAutoTuning->DoingZemlin())
    mAutoTuning->StopZemlin();
  if (mAutoTuning->GetDoingCtfBased())
    mAutoTuning->StopCtfBased();
  if (mAutoTuning->DoingComaVsIS())
    mAutoTuning->StopComaVsISCal();
  if (mPlugStopFunc && mPlugDoingFunc && mPlugDoingFunc())
    mPlugStopFunc(error);
  mCamera->SetPreventUserToggle(0);
}

// Global function to call ErrorOccurred only if the call is from the main thread
// otherwise leave the error for OnIdle to pick up
void SEMErrorOccurred(int error)
{
  if (GetCurrentThreadId() == appThreadID)
    ((CSerialEMApp *)AfxGetApp())->ErrorOccurred(error);
  else
    sThreadError = error;
}

void SEMReportCOMError(_com_error E, CString inString, CString *outStr, bool skipErr)
{
  CString sDescription;
  ScopePluginFuncs *plugFuncs;
  if (E.Error() == JEOL_FAKE_HRESULT || E.Error() == PLUGIN_FAKE_HRESULT || 
    E.Error() == NOFUNC_FAKE_HRESULT) {
      if (E.Error() == JEOL_FAKE_HRESULT)
        sDescription = _T("ERROR in call to JEOL ") + inString;
      else if (E.Error() == NOFUNC_FAKE_HRESULT)
        sDescription = _T("ERROR ") + inString + 
        _T(": function not defined in microscope plugin");
      else {
        sDescription = _T("ERROR in call to microscope ") + inString;
        plugFuncs = CEMscope::GetPlugFuncs();
        if (plugFuncs && plugFuncs->GetLastErrorString) {
          if (CEMscope::ScopeMutexAcquire("SEMReportCOMError", false)) {
            sDescription += ": " + CString(plugFuncs->GetLastErrorString());
            CEMscope::ScopeMutexRelease("SEMReportCOMError");
          }
        }
      }
      if (outStr)
        *outStr = sDescription;
      else if (E.Error() == PLUGIN_FAKE_HRESULT || E.Error() == NOFUNC_FAKE_HRESULT ||
        (debugOutput.IsEmpty() || debugOutput == "0"))
        SEMMessageBox(sDescription);
      else
        SEMTrace('1', (char *)((LPCTSTR)sDescription));

  } else if (E.Error() == SOCKET_FAKE_HRESULT) {
    sDescription = _T("ERROR calling socket server Gatan camera: ") + inString;
    if (outStr)
      *outStr = sDescription;
    else
      SEMMessageBox(sDescription);

  } else {
    if (E.Description().length() > 0){
      sDescription.Format(E.Description());
    } else if (E.ErrorMessage() != NULL){
      sDescription.Format(E.ErrorMessage());
    }
    sDescription = _T("COM error ") + inString + _T(": ") + sDescription; 
    if (outStr)
      *outStr = sDescription;
    else
      SEMMessageBox(sDescription);
  }
  if (!skipErr)
    SEMErrorOccurred(1);
}

BOOL SEMTestHResult(HRESULT hr, CString inString, CString *outStr, int *errFlag, 
  bool skipErr)
{
  if (SUCCEEDED(hr))
    return false;

  if (inString) {
    CString message;
    message.Format(_T("Error 0x%x %s"), hr, inString);
    if (outStr && !(errFlag && *errFlag))
      *outStr = message;
    else
      SEMMessageBox(message);
    if (errFlag)
      (*errFlag)++;
  }
  if (!skipErr)
    SEMErrorOccurred(1);
  return true;
}

// Global function to record latest build date of calling modules
void SEMBuildTime(char *dateStr, char *timeStr)
{
  int month, year, day, sec, hour, min;
  static CTime latestDate = CTime(2000,1,1,1,1,1);
  char monStr[10];
  char stdMonth[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  sscanf(dateStr, "%s %d %d", &monStr[0], &day, &year);
  sscanf(timeStr, "%d:%d:%d", &hour, &min, &sec);
  char *monPtr = strstr(stdMonth, monStr);
  if (!monPtr)
    return;
  month = (int)(monPtr - stdMonth) / 3 + 1;
  CTime thisDate = CTime(year, month, day, hour, min, sec);
  if (thisDate < latestDate) 
    return;
  latestDate = thisDate;
  sBuildDate = dateStr;
  sBuildTime = timeStr;
  sBuildDayStamp = year * 10000 + month * 100 + day;
}

// Global function to get interval between two tick counts or from previous time to now
// It will also return negative intervals; basically if the interval exceeds +/-28 days it
// will be wrapped to the other sign
double SEMTickInterval(double now, double then)
{
  double retval = now - then;
  if (retval < -2147483648.)
    retval += 4294967296.;
  else if (retval > 2147483648.)
    retval -= 4294967296.;
  return retval;
}
double SEMTickInterval(double then)
{
  return SEMTickInterval((double)GetTickCount(), then);
}

double SEMTickInterval(UINT now, UINT then)
{
  return SEMTickInterval((double)now, (double)then);
}

// Returns time in seconds since program start
double DLL_IM_EX SEMSecondsSinceStart()
{
  return SEMTickInterval((double)GetTickCount(), sStartTime) / 1000.;
}

double SEMWallTime()
{
  return wallTime();
}

int SEMUseTEMScripting()
{
  return ((CSerialEMApp *)AfxGetApp())->mScope->GetUseTEMScripting();
}
HitachiParams *SEMGetHitachiParams()
{
  return &((CSerialEMApp *)AfxGetApp())->mHitachiParams;
}
int *SEMGetLastHitachiMember()
{
  return &((CSerialEMApp *)AfxGetApp())->mHitachiParams.lastMember;
}

// Global convenience function for accessing TSMessageBox
int SEMMessageBox(CString message, UINT type, BOOL terminate, int retval)
{
  return ((CSerialEMApp *)AfxGetApp())->mTSController->TSMessageBox
    (message, type, terminate, retval);
}

// And global function for accessing ThreeChoiceBox through TSMessageBox
int SEMThreeChoiceBox(CString message, CString yesText, CString noText, 
  CString cancelText, UINT type, int setDefault, BOOL terminate, int retval)
{
  CTSController *tsc = ((CSerialEMApp *)AfxGetApp())->mTSController;
  tsc->SetTCBoxYesText(yesText);
  tsc->SetTCBoxNoText(noText);
  tsc->SetTCBoxCancelText(cancelText);
  tsc->SetCallFromThreeChoice(true);
  tsc->SetTCBoxDefault(setDefault);
  return tsc->TSMessageBox(message, type, terminate, retval);
}

// Register the name of a function being called in case of a crash
void SEMSetFunctionCalled(const char *name, const char *descrip)
{
  if (sIgnoreFunctionCalled)
    return;
  sFunctionCalled = name;
  if (!sFunctionCalled.IsEmpty() && descrip)
    sFunctionCalled = sFunctionCalled + " " + descrip;
}

// Set flag to ignore the function called calls (for JEOL update thread)
void DLL_IM_EX SEMIgnoreFunctionCalled(bool ignore)
{
  sIgnoreFunctionCalled = ignore;
}


// Handy trace routine to log window, can be called from threads
// Definitions:
//   1 for general
//   A to autosave log on debug output
//   B for beam blank
//   C for focus calibrations
//   D for Direct Electron camera
//   E for Eagle camera
//   F for filter (refine ZLP)
//   G for verbose GIF and JEOL mag lookups
//   H for Hitachi
//   I for intensity changes during state changes and autocentering
//   J for all JEOL calls
//   K for socket interface
//   L for verbose low dose
//   M for Moran/montage time reports
//   P Reserved for other plugins
//   R for retraction
//   S for stage and montage info
//   T for Tietz setup/reset time, camera coords, auotoalign time
//   V for vacuum gauge status and output
//   W for Winkler's typing into axis pos bug or autoloader or leave DE inserted on exit
//   X for EMSIS and Dectris cameras
//   a for alignment correlation coefficients; GPU frame alignment choices/memory usage
//   b for beam shift
//   c for calibrations
//   d for electron dose
//   e for JEOL events
//   f for STEM Focus and JEOL objective focus
//   g for GIF and STEM state tracking functions
//   h for NavHelper
//   i for image shift
//   j for JEOL camera
//   k for key strokes
//   l for low dose
//   m to output all SEMMessageBox messages to log
//   n for Navigator
//   p for AlignWithScaling and FindBeamCenter output
//   r for references
//   s for STEM in general
//   t for exposure time/intensity changes in tasks and continuous timing
//   u for updates
//   w for JEOL stage wait
//   y for background save start/end reports
//   % list script commands allowing arithmetic
//   } Crash program on next image acquire
void SEMTrace(char key, char *fmt, ...)
{
  va_list args;
  CString str;
  if (debugOutput.IsEmpty() || debugOutput == "0" || 
    (key != '1' && debugOutput.Find(key) < 0))
    return;
  if (WaitForSingleObject(traceMutexHandle, TRACE_MUTEX_WAIT) != WAIT_OBJECT_0)
    return;

  if (sNumTraceMsg >= MAX_TRACE_LIST)
    sNumTraceMsg = MAX_TRACE_LIST - 1;
  
  sTraceMsg[sNumTraceMsg].Format("%.3f: ", SEMSecondsSinceStart());
  va_start(args, fmt);
  VarArgToCString(str, fmt, args);
  va_end(args);
  sTraceMsg[sNumTraceMsg++] += str;

  // If this is the main thread, dump the output immediately
  if (GetCurrentThreadId() == appThreadID) {
    for (int len = 0; len < sNumTraceMsg; len++)
      ((CSerialEMApp *)AfxGetApp())->AppendToLog(sTraceMsg[len], LOG_OPEN_IF_CLOSED);
    sNumTraceMsg = 0;
    if (debugOutput.Find('A') >= 0)
      ((CSerialEMApp *)AfxGetApp())->mLogWindow->UpdateSaveFile(false);
  }

  ReleaseMutex(traceMutexHandle);
}

// Safely print a message into a buffer and return it in the CString argumemt
void VarArgToCString(CString &str, char *fmt, va_list args)
{
#define TRACE_BUF_SIZE 256
  char buf[TRACE_BUF_SIZE];
  char *useBuf = &buf[0];
  char *bigBuf = NULL;
  
  int len;
  len = _vscprintf(fmt, args);
  if (len >= TRACE_BUF_SIZE) {
    bigBuf = (char *)malloc(len + 10);
    useBuf = bigBuf;
  }
  if (useBuf) {
    vsprintf(useBuf, fmt, args);
    str = useBuf;
  }
  B3DFREE(bigBuf);
}

void CSerialEMApp::SetDebugOutput(CString keys)
{
  debugOutput = keys;
}

BOOL GetDebugOutput(char key)
{
  return (!(debugOutput.IsEmpty() || debugOutput == "0") &&
    (key == '1' || debugOutput.Find(key) >= 0));
}

CString CSerialEMApp::GetDebugKeys(void)
{
  return debugOutput;
}


////////////////////////////////////////////////////////////////////////
// FUNCTIONS FOR INTERACTIONS WITH CONTROL PANELS, MAIN FRAME ETC

// A dialog box says it is changing size
// Pass the state change on to manager in MainFrm
void CSerialEMApp::DialogChangedState(CToolDlg *inDialog, int inState)
{
  mMainFrame->DialogChangedState(inDialog, inState);
}
BOOL CSerialEMApp::GetWindowPlacement(WINDOWPLACEMENT *winPlace)
{
  return m_pMainWnd->GetWindowPlacement(winPlace);
}

BOOL CSerialEMApp::SetWindowPlacement(WINDOWPLACEMENT *winPlace)
{
  return m_pMainWnd->SetWindowPlacement(winPlace);
}

void CSerialEMApp::SetStatusText(int iPane, CString strText)
{
  mMainFrame->SetStatusText(iPane, strText);
  if (iPane != SIMPLE_PANE)
    return;
  mBlinkText = strText;
  if (strText.IsEmpty())
    return;
  mBlinkOn = true;
  mNextBlinkTime = GetTickCount() + BLINK_TIME_ON;
}

// Manage the blinking pane
void CSerialEMApp::ManageBlinkingPane(DWORD time)
{
  if (!mBlinkText.IsEmpty() && time > mNextBlinkTime && !mAppExiting) {
    if (mBlinkOn) {
      mNextBlinkTime += BLINK_TIME_OFF;
      mMainFrame->SetStatusText(SIMPLE_PANE, mBlinkText);
    } else {
      mNextBlinkTime += BLINK_TIME_ON;
      mMainFrame->SetStatusText(SIMPLE_PANE, "");
    }
    mBlinkOn = !mBlinkOn;
  }
}

void CSerialEMApp::SetTitleFile(CString fileName)
{
  CString progName = mDummyInstance ? "DUMMY SerialEM" : "SerialEM";
  if (!mProgramTitleText.IsEmpty())
    progName += " - " + mProgramTitleText;
  if (fileName.IsEmpty())
    mMainFrame->SetWindowText(progName);
  else
    mMainFrame->SetWindowText(progName + "  -  " + fileName);
}

// Central call to update control panel dialogs when program state changes
void CSerialEMApp::UpdateBufferWindows()
{
  if (!mMaxDialogWidth || mDeferBufWinUpdates)
    return;
  mBufferWindow.UpdateSaveCopy();
  mStatusWindow.Update();
  mTiltWindow.UpdateEnables();
  mAlignFocusWindow.Update();
  mLowDoseDlg.Update();
  mMontageWindow.Update();
  mDEToolDlg.Update();
  if (mFilterParams.firstGIFCamera >= 0 || mScope->GetHasOmegaFilter())
    mFilterControl.Update();
  if (mShowRemoteControl)
    mRemoteControl.UpdateEnables();
  if (mScopeHasSTEM)
    mSTEMcontrol.UpdateEnables();
  if (mNavigator)
    mNavigator->Update();
  else
    mNavHelper->UpdateStateDlg();
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->ManagePanels(true);
  if (mDocWnd->mReadFileDlg)
    mDocWnd->mReadFileDlg->Update();
  if (mStageMoveTool)
    mStageMoveTool->Update();
  if (mAutocenDlg)
    mAutocenDlg->UpdateEnables();
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->ManageEnables();
  if (mNavHelper->mMultiCombinerDlg)
    mNavHelper->mMultiCombinerDlg->UpdateEnables();
  UpdateAllEditers();
  UpdateMacroButtons();
}

// Central call to update all macro editers
void CSerialEMApp::UpdateAllEditers(void)
{
  for (int i = 0; i < MAX_MACROS; i++)
    if (mMacroEditer[i])
      mMacroEditer[i]->UpdateButtons();
  if (mMacroProcessor->mOneLineScript)
    mMacroProcessor->mOneLineScript->Update();
}

// Central call to update macro buttons
void CSerialEMApp::UpdateMacroButtons(void)
{
  mCameraMacroTools.Update();
  if (mMacroToolbar)
    mMacroToolbar->Update();
}

// Central call to update control panel when settings might change
void CSerialEMApp::UpdateWindowSettings()
{
  mBufferWindow.UpdateSettings();
  mImageLevel.UpdateSettings();
  mTiltWindow.UpdateSettings();
  mCameraMacroTools.UpdateSettings();
  mAlignFocusWindow.UpdateSettings();
  mMontageWindow.UpdateSettings();
  mLowDoseDlg.UpdateSettings();
  if (mShowRemoteControl)
    mRemoteControl.UpdateSettings();
  if (mFilterParams.firstGIFCamera >= 0 || mScope->GetHasOmegaFilter())
    mFilterControl.UpdateSettings();
  if (ScopeHasSTEM())
    mSTEMcontrol.UpdateSettings();
  mMacroProcessor->UpdateAllForNewScripts(true);
  if (mAutocenDlg)
    mAutocenDlg->UpdateSettings();
  if (mScreenShotDialog)
    mScreenShotDialog->UpdateSettings();
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->UpdateSettings();
  if (mNavHelper->mMultiCombinerDlg)
    mNavHelper->mMultiCombinerDlg->UpdateSettings();
}


////////////////////////////////////////////////////////////////////
//  INFORMATION CALLS

BOOL CSerialEMApp::DoingImagingTasks()
{
  return (mShiftCalibrator->CalibratingIS() || 
    mShiftCalibrator->CalibratingOffset() || mShiftCalibrator->DoingInterSetShift() ||
    mMontageController->DoingMontage() ||
    mFocusManager->DoingFocus() || mFocusManager->DoingSTEMfocus() ||
    mFocusManager->DoingSTEMfocusVsZ() ||
    mComplexTasks->DoingTasks() ||   // Reports on MultiTSTasks too
    mBeamAssessor->CalibratingIntensity() ||
    mBeamAssessor->CalibratingBeamShift() ||
    mBeamAssessor->CalibratingSpotIntensity() ||
    mFilterTasks->CalibratingMagShift() ||
    mFilterTasks->RefiningZLP() ||
    mGainRefMaker->AcquiringGainRef() ||
    mDistortionTasks->DoingStagePairs() ||
    mCalibTiming->Calibrating() ||
    mCalibTiming->DoingDeadTime() ||
    mParticleTasks->DoingMultiShot() || mParticleTasks->GetWaitingForDrift() ||
    (mNavigator && ((mNavigator->GetAcquiring() && !mNavigator->GetStartedTS() && 
    !mNavigator->StartedMacro() && !mNavigator->GetPausedAcquire()) ||
    mNavHelper->GetRealigning() ||
    mNavHelper->GetAcquiringDual())) || mAutoTuning->DoingAutoTune() ||
    (mStageMoveTool && mStageMoveTool->GetGoingToAcquire()) ||
    DoingTiltSeries() || (mPlugImagingTask && mPlugDoingFunc && mPlugDoingFunc()));
}

BOOL CSerialEMApp::DoingTasks()
{
  bool trulyBusy = DoingImagingTasks()|| 
    mMacroProcessor->DoingMacro() ||
    mShiftManager->ResettingIS() ||
    mScope->CalibratingNeutralIS() || mBeamAssessor->CalibratingIAlimits() ||
    mScope->GetDoingLongOperation() || mMultiTSTasks->DoingBidirCopy() > 0 ||
    (mNavigator && mNavigator->GetLoadingMap()) ||
    (mShowRemoteControl && mRemoteControl.GetDoingTask()) ||
    (mNavHelper->mHoleFinderDlg && mNavHelper->mHoleFinderDlg->GetFindingHoles()) ||
    (mPlugDoingFunc && mPlugDoingFunc());
  mJustChangingLDarea = !trulyBusy && mScope->GetChangingLDArea() != 0;
  mJustDoingSynchro = !trulyBusy && mScope->DoingSynchroThread();
  return trulyBusy || mJustChangingLDarea || mJustDoingSynchro;
}

// Register a stop function of form "void stopFunc(int error)" to be called by 
// ErrorOccurred, with error value 0 for a user stop.  Returns previous function if any;
// call with NULL or previous function to stop being called
PlugStopFunc CSerialEMApp::RegisterPlugStopFunc(PlugStopFunc func)
{
  PlugStopFunc temp = mPlugStopFunc;
  mPlugStopFunc = func;
  return temp;
}

// Registers a function of form "bool doingFunc(void)" to be called by DoingTasks(), or
// by DoingImagingTasks() if imaging is true.  Returns previous function if any and 
// whether it was imaging function; call with NULL or previous function to stop being 
// called
PlugDoingFunc CSerialEMApp::RegisterPlugDoingFunc(PlugDoingFunc func, bool imaging, 
                                                  bool &wasImaging)
{
  PlugDoingFunc temp = mPlugDoingFunc;
  mPlugDoingFunc = func;
  wasImaging = mPlugImagingTask;
  mPlugImagingTask = imaging;
  return temp;
}

// For tilt series or macro to test if call is done
bool CSerialEMApp::DoingRegisteredPlugCall(void)
{
  return (mPlugDoingFunc && mPlugDoingFunc());
}

BOOL CSerialEMApp::UserAcquireOK(void)
{
  return (!DoingTasks() || mShiftCalibrator->CalibratingOffset()) && 
    (!mScope->GetMovingStage() || mScope->GetBkgdMovingStage());
}


// Complex tasks are ones where parameters shouldn't be changed or that use complex pane
BOOL CSerialEMApp::DoingComplexTasks()
{
  return mMacroProcessor->DoingMacro() || mNavHelper->GetAcquiringDual() ||
    mComplexTasks->DoingComplexTasks() || StartedTiltSeries() || 
    (mNavigator && mNavigator->GetAcquiring() && !mNavigator->StartedMacro());
}

int *CSerialEMApp::GetInitialDlgState()
{
  return &initialDlgState[0];
}

BOOL CSerialEMApp::DoingTiltSeries()
{
  return mTSController->DoingTiltSeries();
}

BOOL CSerialEMApp::StartedTiltSeries()
{
  return mTSController->StartedTiltSeries();
}

BOOL CSerialEMApp::NavigatorStartedTS()
{
  return mNavigator && mNavigator->GetStartedTS();
};

BOOL CSerialEMApp::LowDoseMode()
{
  return mScope->GetLowDoseMode();
}

BOOL CSerialEMApp::ActPostExposure(ControlSet *conSet, bool alignHereOK)
{
  return mActPostExposure && mCamera->PostActionsOK(conSet, alignHereOK);
};


/////////////////////////////////////////////////////////////////////////////
// CSERIALEMAPP MESSAGE HANDLERS

// 2/12/04: moved most message handlers to MenuTargets, retaining only the ones
// that deal with member variables of this module.

// Pass the command to any command targets that we have created, excluding ones
// where we moved the target functions to MenuTargets
BOOL CSerialEMApp::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
  if (mMenuTargets.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;
  if (mProcessImage && mProcessImage->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;
  if (mFocusManager && mFocusManager->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;
  if (mComplexTasks && mComplexTasks->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;
  if (mMacroProcessor && mMacroProcessor->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
    return TRUE;
  
  return CWinApp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CSerialEMApp::OnUpdateNoTasks(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!DoingTasks());  
}

void CSerialEMApp::OnCameraParameters() 
{
  CCameraSetupDlg camDlg;
  if (mNoCameras)
    return;

  // Do not allow parameters or camera to change if stacking frames - come back when done
  if (mCamera->CheckFrameStacking(false, true)) {
    AddIdleTask(CFalconHelper::TaskStackingBusy, TASK_ACQUIRE_RESETUP, 0, 0);
    return;
  }
  camDlg.mCamParam = &mCamParams[0];
  camDlg.mCurrentSet = mSelectedConSet;
  camDlg.mCurrentCamera = mCurrentActiveCamera;
  camDlg.mNumCameras = mNumActiveCameras;
  camDlg.mActiveCameraList = &mActiveCameraList[0];
  camDlg.mModeNames = mModeName;
  camDlg.mConSets = &mCamConSets[0][0];
  camDlg.mMontaging = mMontaging;
  camDlg.mStartedTS = StartedTiltSeries();
  if (mNumActiveCameras > 1 && mLastCamera == mCurrentActiveCamera)
    mLastCamera = 
      mCurrentActiveCamera ? mCurrentActiveCamera - 1 : mCurrentActiveCamera + 1;
  camDlg.mLastCamera = mLastCamera;
  camDlg.mAcquireAndReopen = false;
  camDlg.mPlacement = mCamSetupPlacement;
  camDlg.m_bMatchPixel = mNonGIFMatchPixel;
  camDlg.m_bMatchIntensity = mNonGIFMatchIntensity;
  camDlg.m_bKeepPixel = mKeepPixelTime;
  if (camDlg.DoModal() == IDOK) {

    // Do not change camera parameters if user hits Cancel, but do copy control sets
    mLastCamera = camDlg.mLastCamera;
    mNonGIFMatchPixel = camDlg.m_bMatchPixel;
    mNonGIFMatchIntensity = camDlg.m_bMatchIntensity;
    mKeepPixelTime = camDlg.m_bKeepPixel;
    SetActiveCameraNumber(camDlg.mCurrentCamera);
  } else
    CopyConSets(mCurrentCamera);
  mSelectedConSet = camDlg.mCurrentSet;   // Should this change if Cancel?
  mCamSetupPlacement = camDlg.mPlacement;
  RestoreViewFocus();
  UpdateBufferWindows();
  if (camDlg.mAcquireAndReopen) {
    if (LowDoseMode() && mUseViewForSearch && mScope->GetLowDoseArea() == SEARCH_AREA &&
      mSelectedConSet == VIEW_CONSET)
      mCamera->InitiateCapture(SEARCH_CONSET);
    else
      mCamera->InitiateCapture(mSelectedConSet);
    AddIdleTask(CCameraController::TaskCameraBusy, TASK_ACQUIRE_RESETUP, 0, 0);
  }
}

void CSerialEMApp::OnUpdateCameraParameters(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(UserAcquireOK() && !mCamera->CameraBusy());  
	
}

void CSerialEMApp::ReopenCameraSetup(void)
{
  mProcessImage->OnProcessMinmaxmean();
  OnCameraParameters();
}

void CSerialEMApp::UserRequestedCapture(int whichMode)
{
  RestoreViewFocus();
  if (!UserAcquireOK() )
    return;
  
  // If busy, set a pending capture; otherwise initiate a capture
  if (mCamera->CameraBusy()) {
    mCamera->StopCapture(0);
    mCamera->SetPending(whichMode);
  } else
    mCamera->InitiateCapture(whichMode); 
}

// Simulation mode: inform camera, allow all camera with properties if in simulation
void CSerialEMApp::OnCameraSimulation() 
{
  mCamera->SetSimulationMode(!mCamera->GetSimulationMode());
  SetNumberOfActiveCameras(mNumAvailCameras, mNumReadInCameras);
}

void CSerialEMApp::OnUpdateCameraSimulation(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!StartedTiltSeries());
  pCmdUI->SetCheck(mCamera->GetSimulationMode() ? 1 : 0);
}


void CSerialEMApp::SetMontaging(BOOL inVal)
{
  mMontaging = inVal;
  mMontageController->SetMontaging(inVal);
}

// Common path for starting montage or trial montage
void CSerialEMApp::StartMontageOrTrial(BOOL inTrial)
{
  if (!Montaging())
    if (mDocWnd->GetMontageParamsAndFile(false))
      return;
  if (!mMontageController->DoingMontage())
    mMontageController->StartMontage(inTrial ? MONT_TRIAL_IMAGE : MONT_NOT_TRIAL, false);
}

void CSerialEMApp::SetAdministratorMode(int inVal)
{
  if (mAdministratorMode < 0)
    return;
  mAdministrator = inVal > 0;
  mAdministratorMode = inVal;
}

void CSerialEMApp::OnCalibrationAdministrator() 
{
  mAdministrator = !mAdministrator; 
  mAdministratorMode = mAdministrator ? 1 : 0;
  if (mFilterParams.firstGIFCamera >= 0)
    mFilterControl.Update();
  UpdateAllEditers();
}

void CSerialEMApp::OnUpdateCalibrationAdministrator(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mAdministratorMode >= 0);
  pCmdUI->SetCheck(mAdministrator ? 1 : 0);
}

void CSerialEMApp::OnShowScopeControlPanel()
{
  if (!mShowRemoteControl) {
    mRemoteControl.Create(IDD_REMOTE_CONTROL);
    mMainFrame->InsertOneDialog((CToolDlg *)(&mRemoteControl), 
      REMOTE_PANEL_INDEX, dlgBorderColors);
  } else {
    if (mMainFrame->RemoveOneDialog(REMOTE_PANEL_INDEX))
      return;
    mRemoteControl.mInitialized = false;
  }
  mShowRemoteControl = !mShowRemoteControl;
  mScope->RemoteControlChanged(mShowRemoteControl);
}

void CSerialEMApp::OnUpdateShowScopeControlPanel(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mShowRemoteControl ? 1 : 0);
}

void CSerialEMApp::OnHelp() 
{
  CWinApp::OnHelp();  
}

void CSerialEMApp::OnHelpUsing() 
{
  if (m_eHelpType == afxHTMLHelp)
    HtmlHelp(0x10000 + ID_HELP_USING);
  else
    WinHelp(0x10000 + ID_HELP_USING);
}

/////////////////////////////////////////////////////
//  LOG WINDOW FUNCTIONS
/////////////////////////////////////////////////////

// Open the log window
void CSerialEMApp::OnFileOpenlog() 
{
  mLogWindow = new CLogWindow();
  mLogWindow->Create(IDD_LOGWINDOW);
  mLogPlacement.showCmd = SW_SHOWNORMAL;
  if (mLogPlacement.rcNormalPosition.right > 0)
    mLogWindow->SetWindowPlacement(&mLogPlacement);
  RestoreViewFocus();
}

void CSerialEMApp::OnUpdateFileOpenlog(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mLogWindow == NULL); 
}

// Reset the position and size of the log window
void CSerialEMApp::OnWindowRescuelogwindow()
{
  int sizeX, sizeY, offset = 50;
  mLogWindow->GetWindowPlacement(&mLogPlacement);
  mLogPlacement.showCmd = SW_SHOWNORMAL;
  sizeX = B3DMAX(ScaleValueForDPI(500),
    mLogPlacement.rcNormalPosition.right - mLogPlacement.rcNormalPosition.left);
  sizeY = B3DMAX(ScaleValueForDPI(300),
    mLogPlacement.rcNormalPosition.bottom - mLogPlacement.rcNormalPosition.top);
  mLogPlacement.rcNormalPosition.right = offset + sizeX;
  mLogPlacement.rcNormalPosition.bottom = offset + sizeY;
  mLogPlacement.rcNormalPosition.left = mLogPlacement.rcNormalPosition.top = offset;
  mLogWindow->SetWindowPlacement(&mLogPlacement);
}

void CSerialEMApp::OnUpdateWindowRescuelogwindow(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mLogWindow != NULL);
}

// Append a string to the log window, with 3 alternatives if it is not open
// yet: open window, put in message box, and swallow it
// Lineflags can include 1 suppress the line ending, and 2 to remove back to the last line
// ending
void CSerialEMApp::AppendToLog(CString inString, int inAction, int lineFlags)
{
  CString str;
  if (mAppExiting)
    return;

  // Process complex administrative-dependent flags
  switch (inAction) {
  case LOG_IF_ADMIN_MESSAGE_IF_NOT:
    if (mAdministrator)
      inAction = LOG_OPEN_IF_CLOSED;
    else
      inAction = MESSAGE_ONLY;
    break;

  case LOG_MESSAGE_IF_NOT_ADMIN_AND_OPEN:
    if (mAdministrator)
      inAction = LOG_MESSAGE_IF_CLOSED;
    else
      inAction = MESSAGE_ONLY;
    break;

  case LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN:
    if (mAdministrator)
      inAction = LOG_OPEN_IF_CLOSED;
    else
      inAction = LOG_MESSAGE_IF_CLOSED;
    break;

  case LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN:
    if (mAdministrator)
      inAction = LOG_OPEN_IF_CLOSED;
    else
      inAction = LOG_SWALLOW_IF_CLOSED;
    break;

  case MESSAGE_ONLY_IF_ADMIN:
    if (mAdministrator)
      inAction = MESSAGE_ONLY;
    else
      return;
    break;

  case LOG_IF_ADMIN_MESSAGE_IF_CLOSED:
    if (mAdministrator)
      inAction = LOG_MESSAGE_IF_CLOSED;
    else
      return;
    break;

  case LOG_IF_ADMIN_OPEN_IF_CLOSED:
    if (mAdministrator)
      inAction = LOG_OPEN_IF_CLOSED;
    else
      return;
    break;

  case LOG_IF_DEBUG_OPEN_IF_CLOSED:
    if (!debugOutput)
      return;
    inAction = LOG_OPEN_IF_CLOSED;
    if (mLogWindow) {
      str.Format("%.3f: ", SEMSecondsSinceStart());
      mLogWindow->Append(str, 0);
    } 
    break;

  case LOG_IGNORE:
    return;

  default:
    break;
  }

  
  if (inAction == MESSAGE_ONLY ||
    (!mLogWindow && inAction == LOG_MESSAGE_IF_CLOSED)) {
      AfxMessageBox(inString, MB_EXCLAME);
      return;
  }
  
  if (!mLogWindow) {
    if (inAction == LOG_SWALLOW_IF_CLOSED)
      return;
    OnFileOpenlog();
  }

  if (inString.IsEmpty() || inString.GetAt(inString.GetLength() - 1) != '\n') {
    if (lineFlags & 1)
      inString += " ";
    else
      inString += "\r\n";
  }
  mLogWindow->Append(inString, lineFlags);
}

// Handy formatted output to log with variable arguments
void PrintfToLog(char *format, ...)
{
  va_list args;
  int len;
  char *buffer;

  va_start(args, format);
  len = _vscprintf(format, args) + 4;
  NewArray(buffer, char, len);
  vsprintf_s(buffer, len, format, args);
  if (!buffer)
    return;
  ((CSerialEMApp *)AfxGetApp())->AppendToLog(CString(buffer));
  delete [] buffer;
}

// To save the log file: save to new or existing file
void CSerialEMApp::OnFileSavelog() 
{
  mLogWindow->Save();
}

// Save to a new file even if one has been defined
void CSerialEMApp::OnFileSavelogas() 
{
  mLogWindow->SaveAs();
}

void CSerialEMApp::OnUpdateFileSavelog(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mLogWindow != NULL); 
}

// Read in existing log and set up to append to it
void CSerialEMApp::OnFileReadappend() 
{
  if (!mLogWindow)
    OnFileOpenlog();
  mLogWindow->ReadAndAppend();
}

// Toggle continuous save of log
void CSerialEMApp::OnFileContinuousSave()
{
  mContinuousSaveLog = !mContinuousSaveLog;
}

void CSerialEMApp::OnUpdateFileContinuousSave(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mContinuousSaveLog ? 1 : 0);
}

// Log window informs us if it is closing - it takes care of its own destruction
void CSerialEMApp::LogClosing()
{
  mLogWindow->GetWindowPlacement(&mLogPlacement);
  mLogWindow = NULL;
}

///////////////////////////////
// Other non-modal window items

// Navigator closing
void CSerialEMApp::NavigatorClosing()
{
  mNavigator->GetWindowPlacement(&mNavPlacement);
  mNavigator = NULL;
  mMenuTargets.mNavigator = NULL;
  mNavHelper->NavOpeningOrClosing(false);
  mOpenStateWithNav = mNavHelper->mStateDlg != NULL;
  if (mNavHelper->mHoleFinderDlg->IsOpen())
    mNavHelper->mHoleFinderDlg->CloseWindow();
  if (mNavHelper->mMultiCombinerDlg)
    mNavHelper->mMultiCombinerDlg->CloseWindow();
}

// Stage move tool
void CSerialEMApp::OpenStageMoveTool()
{
  if (mStageMoveTool) {
    mStageMoveTool->BringWindowToTop();
    return;
  }
  mStageMoveTool = new CStageMoveTool();
  mStageMoveTool->m_bImageAfterMove = mImageWithStageToolMove;
  mStageMoveTool->Create(IDD_STAGEMOVETOOL);
  if (mStageToolPlacement.rcNormalPosition.right > 0)
    SetPlacementFixSize(mStageMoveTool, &mStageToolPlacement);
  else
    mStageMoveTool->SetWindowPos(&CWnd::wndTopMost, 500, 400, 100, 100, 
      SWP_NOSIZE | SWP_SHOWWINDOW);
}

WINDOWPLACEMENT *CSerialEMApp::GetStageToolPlacement()
{
  if (mStageMoveTool) {
    mStageMoveTool->GetWindowPlacement(&mStageToolPlacement);
    mImageWithStageToolMove = mStageMoveTool->m_bImageAfterMove;
  }
  return &mStageToolPlacement;
};

// Image snapshot dialog
void CSerialEMApp::OpenScreenShotDlg()
{
  if (mScreenShotDialog) {
    mScreenShotDialog->BringWindowToTop();
    return;
  }
  mScreenShotDialog = new CScreenShotDialog();
  mScreenShotDialog->Create(IDD_SCREENSHOT);
  if (mScreenShotPlacement.rcNormalPosition.right > 0)
    SetPlacementFixSize(mScreenShotDialog, &mScreenShotPlacement);
  else
    mScreenShotDialog->SetWindowPos(&CWnd::wndTopMost, 500, 400, 100, 100,
      SWP_NOSIZE | SWP_SHOWWINDOW);
  RestoreViewFocus();
}

WINDOWPLACEMENT * CSerialEMApp::GetScreenShotPlacement()
{
  if (mScreenShotDialog)
    mScreenShotDialog->GetWindowPlacement(&mScreenShotPlacement);
  return &mScreenShotPlacement;
}


////////////////////////////////////////////////////////////////////////
// FUNCTIONS FOR CAMERA AND EFTEM MODE SWITCHING

// Set the number of cameras actually on system, and the number usable
void CSerialEMApp::SetNumberOfActiveCameras(int inNum, int readIn)
{
  mNumAvailCameras = inNum;
  mNumActiveCameras = mActiveCamListSize;
  mNumReadInCameras = readIn;

  // limit number of usable cameras to ones present, unless in simulation mode
  if (inNum < mNumActiveCameras && !mCamera->GetSimulationMode())
    mNumActiveCameras = inNum;

  if (mCurrentActiveCamera >= mNumActiveCameras)
    SetActiveCameraNumber(mNumActiveCameras - 1);
}

// Give a camera index into arrays, finds the active camera number or returns -1 if none
int CSerialEMApp::LookupActiveCamera(int camInd)
{
  for (int i = 0; i < mNumActiveCameras; i++)
    if (mActiveCameraList[i] == camInd)
      return i;
  return -1;
}

// Set the active & current camera number, copy control sets, and inform controller
void CSerialEMApp::SetActiveCameraNumber(int inNum)
{
  int oldCam = mActiveCameraList[mCurrentActiveCamera];
  float toFactor;
  bool eitherSTEM, matchPixel, matchIntensity;
  if (inNum >= mNumActiveCameras)
    inNum = mNumActiveCameras - 1;
  if (mAutocenDlg && mCurrentActiveCamera != inNum && !mAppExiting) {
    SEMMessageBox("You cannot change the current camera while\n"
      "the Beam Autocentering Setup Dialog is open");
    ErrorOccurred(1);
    return;
  }

  // Do not switch cameras while stacking frames, come back here when done
  if (mCamera->CheckFrameStacking(true, true)) {
    AddIdleTask(CFalconHelper::TaskStackingBusy, TASK_SET_CAMERA_NUM, inNum, 0);
    return;
  }

  if (mNoCameras) {
    mCurrentActiveCamera = inNum;
    mCurrentCamera = mActiveCameraList[mCurrentActiveCamera];
    return;
  }
  if (mCamera->CameraBusy())
    mCamera->StopCapture(0);
  CopyCurrentToCameraLDP();
  mCurrentActiveCamera = inNum;
  mCurrentCamera = mActiveCameraList[mCurrentActiveCamera];
  CopyConSets(mCurrentCamera);
  mCamera->SetCurrentCamera(mCurrentCamera, mCurrentActiveCamera);
  eitherSTEM = mCamParams[oldCam].STEMcamera || mCamParams[mCurrentCamera].STEMcamera;

  // if auto-switching is on, set up EFTEM mode appropriately, also STEM
  if (mEFTEMMode && mCamParams[mCurrentCamera].STEMcamera)
    SetEFTEMMode(false);
  if (mSTEMMode && mCamParams[mCurrentCamera].GIF)
    SetSTEMMode(false);
  if (!mSettingEFTEM && !mSettingSTEM && 
    (mCamParams[mCurrentCamera].STEMcamera || !mAllowCameraInSTEMmode))
    SetSTEMMode(mCamParams[mCurrentCamera].STEMcamera);
  if ((mFilterParams.autoCamera || eitherSTEM) && !mSettingEFTEM && !mSettingSTEM) {
    SetEFTEMMode(mCamParams[mCurrentCamera].GIF);
    if (mEFTEMMode)
      mCamera->SetupFilter();
  }
  matchPixel = (!eitherSTEM && mNonGIFMatchPixel);
  matchIntensity = !eitherSTEM && mNonGIFMatchIntensity;
  if (!LowDoseMode() && (matchPixel || matchIntensity) &&
    !mCamParams[oldCam].GIF && !mCamParams[mCurrentCamera].GIF && 
    oldCam != mCurrentCamera) {
    toFactor = mCamParams[mCurrentCamera].matchFactor / mCamParams[oldCam].matchFactor;
    mScope->MatchPixelAndIntensity(oldCam, mCurrentCamera, toFactor, matchPixel,
      matchIntensity);
  }
}

// If pixel matching is on and this is a non-GIF camera different from the initial
// camera on startup, switch back to startup camera so mag will be right when restarting
void CSerialEMApp::RestoreCameraForExit(void)
{
  CameraParameters *initP = &mCamParams[mInitialCurrentCamera];
  CameraParameters *curP = &mCamParams[mCurrentCamera];
  int iCam = LookupActiveCamera(mInitialCurrentCamera);
  if (mNonGIFMatchPixel && !curP->GIF && !curP->STEMcamera && !initP->GIF && 
    !initP->STEMcamera && mInitialCurrentCamera != mCurrentCamera && iCam >= 0)
    SetActiveCameraNumber(iCam);
}

// Set the program into or out of EFTEM mode
void CSerialEMApp::SetEFTEMMode(BOOL inState)
{
  BOOL needed;
  int ldArea;
  int toCamera = mFilterParams.firstRegularCamera;
  if (toCamera < 0)
    toCamera = mFirstSTEMcamera;
  bool fromSTEM = inState && (mSTEMMode || mCamParams[mCurrentCamera].STEMcamera);
  bool toSTEM = !inState && (mCamParams[mCurrentCamera].STEMcamera || mSettingSTEM ||
    (toCamera >= 0 && toCamera == mFirstSTEMcamera));

  if (mFilterParams.firstGIFCamera < 0 || mSettingEFTEM)
    return;

  WarnIfUsingOldFilterAlign();

  SEMTrace('g', "SetEFTEMMode: current state %d, desired state %d", mEFTEMMode ? 1 : 0,
    inState ? 1 : 0);

  mSettingEFTEM = true;
  if (!mSettingSTEM) {

    // Switch camera if necessary - this takes care of copying out low dose parameters
    if (mSTEMMode && inState)
      SetSTEMMode(false);

    if (mFilterParams.autoCamera || fromSTEM || toSTEM) {
      if (inState && !mCamParams[mCurrentCamera].GIF)
        SetActiveCameraNumber(mFilterParams.firstGIFCamera);
      else if (!inState && toCamera >= 0 && mCamParams[mCurrentCamera].GIF && 
        !(toSTEM && JEOLscope))
        SetActiveCameraNumber(toCamera);
    }
  }

  // Make some changes only if there is a change in mode
  if (inState && !mEFTEMMode || !inState && mEFTEMMode) {

    // If entering EFTEM and Retract flag is set, set blanking flag and retract upper
    // camera with blanking
    if (inState && mRetractOnEFTEM && mCamera->CameraReady()) {
      mCamera->SetBlankWhenRetracting(true);
      mCamera->Capture(RETRACT_BLOCKERS);
    }
    
    // If in low dose mode, turn off defining area and set the current parameters as the
    // "set area" params, since parameters get changed before the call to set the area
    if (LowDoseMode()) {
      if (mLowDoseDlg.GetDefining())
        mLowDoseDlg.TurnOffDefine();
      ldArea = mScope->GetLowDoseArea();
      if (ldArea >= 0) {
        mScope->GotoLowDoseArea(ldArea);
        mScope->SetLdsaParams(&mCamLowDoseParams[mEFTEMMode ? 1 : 0][ldArea]);
      }
      // 1/18/12 No longer call ChangeAllShifts!
    }

    // Change state and copy the proper LDP back in if not going to STEM
    mEFTEMMode = inState;

    // If entering EFTEM mode, take slit out
    if (mEFTEMMode)
      mFilterParams.slitIn = false;

    // Restore low dose params, go to the area officially
    if (!toSTEM) {
      CopyCameraToCurrentLDP();
      if (LowDoseMode()) {
        if (ldArea >= 0)
          mScope->GotoLowDoseArea(ldArea);
        mLowDoseDlg.UpdateSettings();
      }

      // Synchronize the window and set filter to settings
      mFilterControl.UpdateSettings();
      if (mEFTEMMode && mCamera->CameraReady())
        mCamera->SetupFilter();
    }
  }

  // Switch EFTEM lens mode if needed
  needed = mEFTEMMode && (!mScope->GetCanControlEFTEM() ||
    (mScope->GetScreenPos() == spUp || !mFilterParams.autoMag));
  mScope->SetEFTEM(needed);
  mFilterControl.UpdateSettings();
  mSettingEFTEM = false;
}

// Set the program into or out of STEM mode
void CSerialEMApp::SetSTEMMode(BOOL inState)
{
  int toCamera = mFilterParams.firstRegularCamera;
  if (toCamera < 0)
    toCamera = mFilterParams.firstGIFCamera;
  bool toEFTEM = !inState && (mSettingSTEM || mCamParams[mCurrentCamera].GIF ||
    (toCamera >= 0 && toCamera == mFilterParams.firstGIFCamera));
  if (!mScopeHasSTEM || mSettingSTEM)
    return;

  SEMTrace('g', "SetSTEMMode to state %d", inState ?1:0);
  mSettingSTEM = true;

  // Turn off LD mode before switching camera
  if (LowDoseMode() && (mSTEMMode ? 1 : 0) != (inState ? 1 : 0))
    mLowDoseDlg.SetLowDoseMode(false);

  if (!mSettingEFTEM) {
      
    // Switch camera if necessary.  If came here from a camera switch, it is already right
    if (inState && mEFTEMMode)
      SetEFTEMMode(false);
    if (inState && !mCamParams[mCurrentCamera].STEMcamera)
      SetActiveCameraNumber(mFirstSTEMcamera);
    else if (!inState && toCamera >= 0 && mCamParams[mCurrentCamera].STEMcamera)
      SetActiveCameraNumber(toCamera);
  }

  // If entering STEM and Retract flag is set, set blanking flag and retract upper
  // camera with blanking
  if (inState && !mSTEMMode && mRetractOnSTEM && mCamera->CameraReady()) {
    mCamera->SetBlankWhenRetracting(true);
    mCamera->RetractAllCameras();
  }

  // Change state, copy new LD params in
  mSTEMMode = inState;
  if (!toEFTEM) {
    CopyCameraToCurrentLDP();
    mLowDoseDlg.Update();
    if (mScopeHasFilter)
      mFilterControl.Update();
  }
  mScope->SetSTEM(mSTEMMode && 
    (mScope->GetScreenPos() == spUp || !DoSwitchSTEMwithScreen()));
  mSTEMcontrol.UpdateSTEMstate(mScope->GetProbeMode());
  if (mSTEMMode) {
    if (DoDropScreenForSTEM() > 1)
      mScope->SetScreenPos(spDown);
    else if (mCamera->GetLowerScreenForSTEM() < -1)
      mScope->SetScreenPos(spUp);
  }
  mSettingSTEM = false;
}

// Return the gain factor for the given camera at the given binning
float CSerialEMApp::GetGainFactor(int camera, int binning)
{
  int i, binInd = -1;
  if (binning <= 0 ||  camera < 0 || camera >= MAX_CAMERAS)
    return 1.;
  for (i = 0; i < mCamParams[camera].numBinnings; i++)
    if (mCamParams[camera].binnings[i] == binning)
      return mCamParams[camera].gainFactor[i];
  return 1.;
}

// Copy control sets for a camera to the master control sets
void CSerialEMApp::CopyConSets(int inCam)
{
  for (int i = 0; i < MAX_CONSETS; i++)
    mConSets[i] = mCamConSets[inCam][i];
}

// Synchronize the Search or Montage control set from View or Record if option selected
void CSerialEMApp::CopyOptionalSetIfNeeded(int inSet, int inCam)
{
  if (inCam < 0)
    inCam = mCurrentCamera;
  if (inSet == SEARCH_CONSET && mUseViewForSearch) {
    mCamConSets[inCam][SEARCH_CONSET] = mCamConSets[inCam][VIEW_CONSET];
    mConSets[SEARCH_CONSET] = mConSets[VIEW_CONSET];
  }
  if (inSet == MONT_USER_CONSET && mUseRecordForMontage) {
    mCamConSets[inCam][MONT_USER_CONSET] = mCamConSets[inCam][RECORD_CONSET];
    mConSets[MONT_USER_CONSET] = mConSets[RECORD_CONSET];
  }
}

// Transfer a control set from one camera to another, scaling the coordinates
// for the change in size and adjusting binning as needed
void CSerialEMApp::TransferConSet(int inSet, int fromCam, int toCam)
{
  ControlSet *tocs = &mCamConSets[toCam][inSet];
  ControlSet *fromcs = &mCamConSets[fromCam][inSet];
  *tocs = *fromcs;
  tocs->left = (fromcs->left * mCamParams[toCam].sizeX) / mCamParams[fromCam].sizeX;
  tocs->right = (fromcs->right * mCamParams[toCam].sizeX) / mCamParams[fromCam].sizeX;
  tocs->top = (fromcs->top * mCamParams[toCam].sizeY) / mCamParams[fromCam].sizeY;
  tocs->bottom = (fromcs->bottom * mCamParams[toCam].sizeY) / mCamParams[fromCam].sizeY;
  tocs->binning = BinDivisorI(&mCamParams[toCam]) * fromcs->binning /
    BinDivisorI(&mCamParams[fromCam]);
  if (!BinningIsValid(tocs->binning, toCam, false)) {
    int newbin = NextValidBinning(tocs->binning, -1, toCam, false);
    if (newbin == tocs->binning)
      newbin = NextValidBinning(tocs->binning, 1, toCam, false);
    tocs->binning = newbin;
  }
}

// Provide a flag for being in EFTEM mode or having an omega filter
BOOL CSerialEMApp::GetFilterMode()
{
  return mEFTEMMode || (mScope->GetHasOmegaFilter() && !mSTEMMode);
}

//Copy the current low dose parameters to the camera-specific ones
void CSerialEMApp::CopyCurrentToCameraLDP()
{
  int dest = mEFTEMMode ? 1 : 0;
  if (mSTEMMode)
    dest = 2;
  if (LowDoseMode())
    mLowDoseDlg.ConvertAxisPosition(false);
  for (int i = 0; i < MAX_LOWDOSE_SETS; i++)
    mCamLowDoseParams[dest][i] = mLowDoseParams[i];
}

//Copy the camera-specific low dose parameters to the master parameter set
void CSerialEMApp::CopyCameraToCurrentLDP()
{
  int src = mEFTEMMode ? 1 : 0;
  if (mSTEMMode)
    src = 2;
  for (int i = 0; i < MAX_LOWDOSE_SETS; i++)
    mLowDoseParams[i] = mCamLowDoseParams[src][i];
  if (LowDoseMode())
    mLowDoseDlg.ConvertAxisPosition(true);
}

// Provide a message if filter alignment is being used from old session
void CSerialEMApp::WarnIfUsingOldFilterAlign()
{
  CString str;
  if (!mFilterParams.usedOldAlign)
    return;
  str.Format("Using filter alignment from previous session (refinement offset %.1f)",
    mFilterParams.refineZLPOffset);
  AppendToLog(str, LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN);
  mFilterParams.usedOldAlign = false;
}

// Return a time stamp for the current time in minutes
int CSerialEMApp::MinuteTimeStamp(void)
{
  CTime time = CTime::GetCurrentTime();
  return ((int)(time.GetTime() / 60));
}

// Draws an image just read in
void CSerialEMApp::DrawReadInImage(void)
{
  mMainView->DrawImage();
  SetImBufIndex(mBufferManager->GetBufToReadInto());
  mMainView->SetCurrentBuffer(mBufferManager->GetBufToReadInto());
  UpdateBufferWindows(); 
}

// Return a format for printing pixel size in nm
CString CSerialEMApp::PixelFormat(float pixelInNm, float pixScale)
{
  CString format;
  int ndec  = (int)floor(log10((pixScale < 1. ? 10000. : 1000.) / (pixelInNm *pixScale)));
  ndec = B3DMIN(3, B3DMAX(0, ndec));
  format.Format("%%.%df %s", ndec, pixScale < 1. ? "um" : "nm");
  return format;
}

// Set the placement for a fixed-size dialog and adjust size to current dialog size
void CSerialEMApp::SetPlacementFixSize(CWnd *window, WINDOWPLACEMENT *lastPlacement)
{
  WINDOWPLACEMENT origPlace;
  RECT *orig = &origPlace.rcNormalPosition;
  RECT *setting = &lastPlacement->rcNormalPosition;

  window->GetWindowPlacement(&origPlace);
  if (setting->right > 0){
    setting->right = setting->left + orig->right - orig->left;
    setting->bottom = setting->top + orig->bottom - orig->top;
    lastPlacement->showCmd = 1;
    window->SetWindowPlacement(lastPlacement);
  }
  window->ShowWindow(SW_SHOW);
}

// Returns the number of mag ranges to consider for a camera and the index of the second
// mag range for STEM microprobe in FEI
void CSerialEMApp::GetNumMagRanges(int camera, int &numRanges, int &lowestMicro)
{
  lowestMicro = 1;
  numRanges = 1;
  if (FEIscope && mCamParams[camera].STEMcamera) {
    numRanges = 2;
    lowestMicro = mScope->GetLowestMicroSTEMmag();
    return;
  }
  if (mCamParams[camera].STEMcamera || !mScope->GetLowestSecondaryMag())
    return;
  numRanges = 2;
  lowestMicro = mScope->GetLowestSecondaryMag();
}

// Returns the lower and upper limits of the mag range including the given mag index
// for the given camera.  The upper limit IS included in the mag range; loop with <=
void CSerialEMApp::GetMagRangeLimits(int camera, int magInd, int &lowerLimit, 
                                     int &upperLimit)
{
  int lowest;
  lowerLimit = 1;
  upperLimit = MAX_MAGS - 1;
  if (FEIscope && mCamParams[camera].STEMcamera)
    lowest = mScope->GetLowestMicroSTEMmag();
  else if (!mCamParams[camera].STEMcamera && mScope->GetLowestSecondaryMag())
    lowest = mScope->GetLowestSecondaryMag();
  else
    return;
  if (magInd < lowest)
    upperLimit = lowest - 1;
  else
    lowerLimit = lowest;
}

// Step to the next available mag for the given camera with increment delta; stay out
// of LM if noLM is true (default is false)
int CSerialEMApp::FindNextMagForCamera(int camera, int magInd, int delta, BOOL noLM)
{
  int limlo, limhi, index, numChange;
  int secondary = mScope->GetLowestSecondaryMag();
  int lowestM = mScope->GetLowestMModeMagInd();
  BOOL ifSTEM = mCamParams[mCurrentCamera].STEMcamera;
  bool inSecondary = !ifSTEM && secondary && magInd >= secondary;
  bool inDualLM = !ifSTEM && secondary && magInd < lowestM && mScope->GetSecondaryMode();
  GetMagRangeLimits(mCurrentCamera, magInd, limlo, limhi);
  if (mScope->GetUseInvertedMagRange() && 
    UtilMagInInvertedRange(magInd, mCamParams[mCurrentCamera].GIF))
    delta = -delta;
  numChange = delta > 0 ? delta : -delta;
  if (noLM) {
    index = ifSTEM ? 1 : 0;
    if (ifSTEM && FEIscope && magInd >= mScope->GetLowestMicroSTEMmag())
      index = 2;
    if (ifSTEM || !secondary || magInd < secondary)
      limlo = mScope->GetLowestNonLMmag(index);
  }
  index = 0;
  while (index < numChange) {
    magInd += delta > 0 ? 1 : -1;
    if (inSecondary && magInd < limlo) {
      if (noLM)
        return -1;
      magInd = lowestM - 1;
      limlo = 1;
      inSecondary = false;
    }
    if (inDualLM && magInd >= lowestM) {
      magInd = secondary + magInd - lowestM;
      limhi = MAX_MAGS;
      inDualLM = false;
    }
    if (magInd < limlo || magInd > limhi)
      return -1;
    if ((!ifSTEM && mMagTab[magInd].mag > 0) || (ifSTEM && mMagTab[magInd].STEMmag > 0))
      index++;
  }
  return magInd;
}

// Take care of changing noShutter property of STEM cameras and setting scope flag 
// if in STEM mode
void CSerialEMApp::ManageSTEMBlanking(void)
{
  int act, cam;
  for (act = 0; act < mNumActiveCameras; act++) {
    cam = mActiveCameraList[act];
    if (mCamParams[cam].STEMcamera)
      mCamParams[cam].noShutter = mBlankBeamInSTEM ? 1 : 0;
  }
  if (mSTEMMode)
    mScope->SetShutterlessCamera(mBlankBeamInSTEM ? 1 : 0);
}

// Return flag for whether to lower screen for STEM, based on confusing variables
int CSerialEMApp::DoDropScreenForSTEM(void)
{
  if (mCamera->GetLowerScreenForSTEM() < 0)
    return 0;
  return mCamera->GetLowerScreenForSTEM() * 
    ((!mRetractToUnblankSTEM || mMustUnblankWithScreen) ? 1 : 0);
}

BOOL CSerialEMApp::DoSwitchSTEMwithScreen(void)
{ 
  return mScreenSwitchSTEM && !LowDoseMode();
}

// Return the next valid binning in the given direction from the current one for the
// given camera and given whether 1 is allowed for K2 camera
int CSerialEMApp::NextValidBinning(int curBinning, int direction, int camera, 
                                   bool allowFractional)
{
  CameraParameters *cam = &mCamParams[camera];
  int newVal = curBinning;
  int minInd = BinningIsValid(cam->binnings[0], camera, allowFractional) ? 0 : 1;

  for (;;) {
    newVal += direction;
    if (newVal < cam->binnings[minInd] || newVal > cam->binnings[cam->numBinnings - 1])
      return curBinning;
    if (BinningIsValid(newVal, camera, allowFractional))
      break;
  }
  return newVal;
}

// Returns whether the binning is valid for the given camera, which depends on whether
// binning 1 (displayed as 0.5) is allowed for a K2
bool CSerialEMApp::BinningIsValid(int binning, int camera, bool allowFractional)
{
  CameraParameters *cam = &mCamParams[camera];
  for (int i = 0; i < cam->numBinnings; i++) {
    if (binning == cam->binnings[i] && (i > 0 || !cam->K2Type || (allowFractional &&
      (mCamConSets[camera][RECORD_CONSET].K2ReadMode == SUPERRES_MODE ||
      (cam->K2Type == K3_TYPE && mCamConSets[camera][RECORD_CONSET].K2ReadMode > 
      LINEAR_MODE)))))
      return true;
  }
  return false;
}

// Return string for the binning, divided by 2 if necessary
CString CSerialEMApp::BinningText(int binning, int divideBinToShow)
{
  CString str;
  float trueBin = (float)binning / (float)B3DMAX(1, divideBinToShow);
  int intBin = B3DNINT(trueBin);
  if (fabs(trueBin - intBin) < 1.e-5)
    str.Format("%d", intBin);
  else
    str.Format("%.1f", trueBin);
  return str;
}

CString CSerialEMApp::BinningText(int binning, CameraParameters *camParam)
{
  return BinningText(binning, BinDivisorI(camParam));
}

CameraParameters *CSerialEMApp::GetActiveCamParam(int index)
{
  if (index < 0)
    index = mCurrentActiveCamera;
  return &mCamParams[mActiveCameraList[index]];
}

// Global functions for JEOL plugin
int SEMSetSecondaryModeForMag(int index)
{
  return CEMscope::SetSecondaryModeForMag(index);
}

BOOL SEMAcquireScopeMutex(char *name, BOOL retry)
{
  return CEMscope::ScopeMutexAcquire(name, retry);
}

BOOL SEMReleaseScopeMutex(char *name)
{
  return CEMscope::ScopeMutexRelease(name);
}

BOOL SEMScopeHasFilter()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->ScopeHasFilter();
}

MagTable *SEMGetMagTable()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->GetMagTable();
}

int *SEMGetCamLenTable()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->GetCamLenTable();
}

static int sNumFEIChannels = 3;
void SetNumFEIChannels(int inval)
{
  B3DCLAMP(inval, 3, 4);
  sNumFEIChannels = inval;
}

int SEMNumFEIChannels()
{
  return sNumFEIChannels;
}

double CSerialEMApp::ProgramStartTime(void)
{
  return sStartTime;
}

// Toggle basic mode, managing tool dlgs and calling various things to manage menu/dialogs
void CSerialEMApp::SetBasicMode(BOOL inVal)
{
  int ind;
  mBasicMode = inVal;
  if (mStartingProgram)
    return;

  for (ind = 0; ind < MAX_TOOL_DLGS; ind++) {
    if (mBasicIDsToHide.count(-10 - ind)) {
      if (ind == REMOTE_PANEL_INDEX) {
        if (BOOL_EQUIV(mShowRemoteControl, inVal))
          OnShowScopeControlPanel();
      } else if (inVal) {
        mToolDlgs[ind]->mInitialized = false;
        mMainFrame->RemoveOneDialog(ind);
      } else {
        mToolDlgs[ind]->Create(sToolDlgIDs[ind]);
        mMainFrame->InsertOneDialog(mToolDlgs[ind], ind, dlgBorderColors);
      }
    }
  }
  mMainFrame->RemoveHiddenItemsFromMenus();
  UpdateBufferWindows();
  UpdateWindowSettings();
}

// Given a color index for a tool panel, find its index in the dialog table
int CSerialEMApp::LookupToolDlgIndex(int colorInd)
{
  for (int ind = 0; ind < mNumToolDlg; ind++)
    if (mDlgColorIndex[ind] == colorInd)
      return ind;
  return -1;
}

// Compute the maximum width of tool panels
void CSerialEMApp::SetMaxDialogWidth(void)
{
  mMaxDialogWidth = 0;
  for (int ind = 0; ind < mNumToolDlg; ind++)
    ACCUM_MAX(mMaxDialogWidth, mDialogTable[ind].width);
}

// To be called on startup or after reading settings: opens ones set to be opened and
// closes ones that are open if needed
void CSerialEMApp::OpenOrCloseMacroEditors(void)
{
  if (!mMacroProcessor->GetRestoreMacroEditors())
    return;
  for (int ind = 0; ind < MAX_MACROS; ind++) {
    if (mReopenMacroEditor[ind] && !mMacroEditer[ind]) 
      mMacroProcessor->OpenMacroEditor(ind);
    else if (!mReopenMacroEditor[ind] && mMacroEditer[ind])
      mMacroEditer[ind]->JustCloseWindow();
  }
  mMacroProcessor->OpenOrJustCloseOneLiners(mReopenMacroEditor[MAX_MACROS]);
  UpdateBufferWindows();
  RestoreViewFocus();
}

// Sets all macros empty and updates open editors
void CSerialEMApp::ClearAllMacros(void)
{
  int set;
  for (set = 0; set < MAX_MACROS + MAX_ONE_LINE_SCRIPTS; set++) {
    mMacros[set] = "";
    if (set < MAX_MACROS) {
      if (mMacroEditer[set])
        mMacroEditer[set]->TransferMacro(false);
      else
        mMacroProcessor->ScanForName(set);
    } else if (mMacroProcessor->mOneLineScript)
      mMacroProcessor->mOneLineScript->m_strOneLine[set - MAX_MACROS] = "";
  }
  mMacroProcessor->TransferOneLiners(false);
}

// Returns the little font based on the size of a regaulr font static text box
// creates it the first time it is asked for
CFont * CSerialEMApp::GetLittleFont(CWnd *stat)
{
  CRect rect;
  int height = 8;

  // On UTMB 2200 XP system, 96 DPI, height of stat was 16 and a size of 12 was bad,
  // scaling 0.715 just gets it down to 11.  But this scaling made MSSF too small, so
  // keep the 0.77 worked out before for that.  Just revert to doing 80 MSSF for 96 DPI!
  if (mMadeLittleFont)
    return &mLittleFont;
  CString fontName = "Small Fonts";
  if (mSmallFontsBad || (mSystemDPI >= 120 && !mDisplayNotTruly120DPI))
    fontName = "Microsoft Sans Serif";
  height = ScaleValueForDPI(height);
  if (stat) {
    stat->GetWindowRect(&rect);
    height = rect.Height();
  }
  if (fontName == "Small Fonts" && mSystemDPI == 96)
    mLittleFont.CreatePointFont(80, "Microsoft Sans Serif");
  else
    mLittleFont.CreateFont(B3DNINT(0.77 * height), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, fontName);
  mMadeLittleFont = true;
  return &mLittleFont;
}

// Returns the scaling that is being applied for DPI
float CSerialEMApp::GetScalingForDPI()
{
  return (float)B3DCHOICE(mDisplayNotTruly120DPI, 1., mSystemDPI / 96.);
}

// Scale one integer for DPI
int CSerialEMApp::ScaleValueForDPI(double value)
{
  return B3DNINT(value * GetScalingForDPI());
}
