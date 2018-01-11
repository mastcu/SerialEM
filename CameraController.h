// CameraController.h: interface for the CCameraController class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAMERACONTROLLER_H__C6BCA005_4648_455D_9AE8_566F127CB989__INCLUDED_)
#define AFX_CAMERACONTROLLER_H__C6BCA005_4648_455D_9AE8_566F127CB989__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// For parallel builds, needed to replace all these imports with includes

// Get the Gatan camera plugin properties
//#import "..\SerialEMCCD\SerialEMCCD.tlb" no_namespace
#include "serialemccd.tlh"

//#import "AmtCamera.tlb" no_namespace
#include "amtcamera.tlh"

//#import "..\FocusRamper\FocusRamper.tlb" no_namespace
#include "focusramper.tlh"

#include "EMbufferManager.h"
#include "EMscope.h"
class DirectElectronCamera;
struct CamPluginFuncs;

#define MAX_DARK_REFS   10
#define MAX_CHANNELS     8
#define MAX_RAMP_STEPS   32
#define MAX_IGNORE_GATAN 10
#define MAX_K2_FILTERS  8
#define MAX_FILTER_NAME_LEN 64
#define DARK_REFERENCE  0
#define GAIN_REFERENCE  1
#define NEW_GAIN_REFERENCE 2
#define RETRACT_BLOCKERS -1
#define RETRACT_ALL -2

// Version that is good enough if there are no K2 cameras
#define SEMCCD_VERSION_OK_NO_K2  104

// Plugin versions with various capabilities
#define PLUGIN_CREATES_DIRECTORY 100
#define PLUGIN_CAN_SUM_FRAMES    101
#define PLUGIN_CAN_ANTIALIAS     101
#define PLUGIN_HAS_LAST_ERROR    101
#define PLUGIN_CAN_GAIN_NORM     102
#define PLUGIN_CAN_MAKE_SUBAREA  104
#define PLUGIN_CAN_CONTROL_BEAM  104
#define PLUGIN_4BIT_101_COUNTING 104
#define PLUGIN_CAN_ALIGN_FRAMES  105
#define PLUGIN_SAVES_TIMES_100   105
#define PLUGIN_UNPROC_LIKE_DS    106
#define PLUGIN_CAN_SET_MRCS_EXT  106
#define PLUGIN_CAN_ALIGN_SUBSET  107

#define CAN_PLUGIN_DO(c, p) CanPluginDo(PLUGIN_##c, p)

// DE Camera flags
#define DE_CAM_CAN_COUNT          0x1
#define DE_CAM_CAN_ALIGN          0x2
#define DE_HAS_TEMP_SET_PT        0x4
#define DE_HAS_READOUT_DELAY      0x8
#define DE_HAS_HARDWARE_BIN       0x10
#define DE_WE_CAN_ALIGN           0x20
#define DE_NORM_IN_SERVER         0x40

#define AMT_VERSION_CAN_NORM     700

enum {INIT_ALL_CAMERAS, INIT_CURRENT_CAMERA, INIT_GIF_CAMERA, INIT_TIETZ_CAMERA};
enum {LINEAR_MODE = 0, COUNTING_MODE, SUPERRES_MODE};
enum DE_CAMERATYPE{LC1100_4k = 1,DE_12 = 2,DE_12_Survey=3,DE_LC1100=4};
enum {EAGLE_TYPE = 1, FALCON2_TYPE, OTHER_FEI_TYPE, FALCON3_TYPE};

#define DEFAULT_FEI_MIN_EXPOSURE 0.011f
#define FCAM_ADVANCED(a) (a->CamFlags & PLUGFEI_USES_ADVANCED)
#define FCAM_CAN_COUNT(a) (a->CamFlags & PLUGFEI_CAM_CAN_COUNT)
#define FCAM_CAN_ALIGN(a) (a->CamFlags & PLUGFEI_CAM_CAN_ALIGN)
#define IS_FALCON2_OR_3(a) (a->FEItype == FALCON2_TYPE || a->FEItype == FALCON3_TYPE)
#define IS_BASIC_FALCON2(a) (a->FEItype == FALCON2_TYPE && !(a->CamFlags & PLUGFEI_USES_ADVANCED))
// This needs to be 108 when that exists
#define PLUGFEI_ALLOWS_ALIGN_HERE 107

struct DarkRef {
  int Left, Right, Top, Bottom;   // binned CCD coordinates of the image
  int SizeX, SizeY;           // size in X and Y
  int Binning;                // Binning
  double Exposure;            // Exposure in sec.
  float Delay;                // Delay after subtracting fixed amount
  int UseCount;               // Count at which it was last used
  int BumpExposure;           // Flag that exposure was incremented
  int Camera;                 // Camera number
  int ByteSize;               // 2 for ints, 4 for floats
  int GainDark;               // flag for which type of reference
  int GainRefBits;            // Scaling factor for integer Gain ref: bits to shift
  double TimeStamp;           // Ticks/1000
  int ConSetUsed;             // Control set this was last associated with
  int xOffset;                // Value of X offset for a binned reference for Tietz block
  int yOffset;                // Value of Y offset for a binned reference for Tietz block
  int Ownership;              // 0 if gainrefmaker owns the array
  void *Array;                // actual reference data
};

struct CameraThreadData {
  SAFEARRAY *psa;
  short int *Array[MAX_CHANNELS];
  int NumChannels;            // Number of channels of data to acquire
  BOOL Simulation;            // Flag for the early simulation mode
  BOOL FauxCamera;            // Using a simulation camera
  int PluginVersion;          // DM Plugin version for this camera
  int Camera;                 // Active Camera number
  int SelectCamera;           // Gatan camera number to select
  int TietzType;              // Or type of Tietz camera
  int FEItype;                // Or flag for FEI camera
  unsigned int CamFlags;      // Flags entry for advanced interface and anything else
  int DE_camType;             // Flag for NCMIR camera
  CString cameraName;         // Camera name for FEI, or mapping name for Tietz (from detector name)
  BOOL checkFEIname;          // Flag to check name of FEI camera
  int STEMcamera;             // Flag for STEM camera
  CString errMess;            // Error message for deferred message boxes
  int errFlag;                // Flag set by testing routines or threads
  long Left, Right, Top, Bottom;     // binned CCD coordinates of the image
  long DMSizeX, DMSizeY;      // size in X and Y
  long CallSizeX, CallSizeY;  // Sizes to use in calls for DE and Tietz
  int fullSizeX, fullSizeY;   // Actual full size of camera
  int restrictedSize;         // Restricted size index
  int ImageType;              // Type of image
  long Binning;                // Binning
  int UseHardwareBinning;     // Hardware binning flag for DE
  double FramesPerSec;        // FPS value when ther is counting mode for DE
  double Exposure;            // Exposure in sec. after forcing bump if any
  long Processing;
  long ProcessingPlus;        // Actual processing value to send to Gatan with flags too
  double DMsettling;           // DM's settling time
  int K2ParamFlags;           // Flags to set in call to set K2Parameters2
  int FEIacquireFlags;        // Flags for advanced FEI acquire
  int UnblankTime;            // delay from firing DM to unblanking, in msec
  int ReblankTime;            // total time for beam to be on, in msec
  int ShutterTime;            // time to hold shutter open before firing DM, in msec
  int MinBlankTime;           // Minimum time occupied by blanking and unblanking
  int ShotDelayTime;          // Time to wait before starting any shot
  long ShutterMode;
  int oneFEIshutter;          // onlyOneShutter value for FEI
  int DMversion;
  BOOL DMbeamShutterOK;
  BOOL DMsettlingOK;
  double PreDarkExposure;     // Exposure time for a pre-dark ref when open shutter broken
  int PreDarkBinning;         // Binning to use for this
  int PreDarkBlankTime;       // Time to blank out in the dark ref
  int NumDRDelete;            // Number of dark refs to delete
  DarkRef *DRdelete;          // Dark refs to delete
  DarkRef *DarkToGet;         // Dark reference to be acquired
  DarkRef *GainToGet;         // Gain reference to be acquired
  long DivideBy2;              // Flag to divide data by 2
  long Corrections;            // Corrections to do, or -1 for default
  int PostActionTime;         // delay until postactions, general flag for actions
  BOOL PostMoveStage;         // Flag to move stage as postaction
  StageMoveInfo MoveInfo;     // Info structure for moving stage
  BOOL PostImageShift;        // Flag to do image shift
  double ISX, ISY;            // Image shift
  DWORD ISTicks;              // Time when change finished
  BOOL PostChangeMag;         // Flag to change mag
  BOOL PostMagFixIS;          // Flag to fix IS after mag change
  int NewMagIndex;            // Index to change to
  int JeolMagChangeDelay;     // Delay time after changing mag
  double ISperMsX, ISperMsY;  // IS change per msec in X and Y
  int DriftISinterval;        // Interval for changes: also flag to do it
  int DynFocusInterval;       // Interval for dynamic focus
  float InitialDriftSteps;    // Number of initial steps to take
  double FocusStep1, FocusStep2; // One or two single focus steps (first is flag to do it)
  int Step2FocusDelay;        // Interval to second focus change if any
  int LowestMModeMag;         // For JEOL to figure out which mag range to use
  DWORD MagTicks;             // Time when normalization finished
  BOOL TietzCanPreexpose;     // Flag to set preexposure
  BOOL RestoreBBmode;         // Flag to restore BB mode after preexposure
  double lastTietzSettling;   // Last settling time sent to shutterbox
  long eagleIndex;            // Index from looking up this camera by name
  long startingFEIshutter;    // FEI wants the UI restored when done
  IAMTCamInterface *amtCam;   // Resident pointer to AMT camera
  DirectElectronCamera *DE_Cam;   // Pointer to DirectElectron 4K camera    
  JeolStateData *JeolSD;     // So that the Jeol scope can be accessed directly
  CamPluginFuncs *plugFuncs;  // Plugin Functions for current camera
  ScopePluginFuncs *scopePlugFuncs;  // Plugin functions for plugin scope
  BOOL GotScopeMutex;         // Flag that the blanker proc got the mutex
  ScaleMat IStoBS;            // Scale to get from image shift to beam shift for JEOL
  DWORD blankerTimeout;       // Timeout interval for the blanker thread after acquisition
  CWinThread *blankerThread;  // blanker thread so main or acquire thread can kill it
  HANDLE blankerMutexHandle;  // Mutex for assessing state of blanker
  bool stopWaitingForBlanker; // Flag that the waiting routine should give up.
  DWORD cameraTimeout;        // Timeout interval for camera acquisition
  int ScanSleep;              // Sleep time between scan moves in ms
  double BeamBaseX, BeamBaseY;    // Base values (start of scan) for beam movement
  double BeamPerMsX, BeamPerMsY;  // Amount to move per ms
  double FocusBase;           // Base value for focus at start of scan (also dyn-focus)
  double JeolOLtoUm;          // JEOL scaling to microns
  double FocusPerMs;          // Amount to change per ms (for dynamic focus too)
  double ScanTime;            // Total time to scan in ms
  int ScanDelay;              // Delay time before scanning, in ms (for dynamic focus too)
  double ScanIntMin, ScanIntMax;  // Min and max intervals in scan
  double ScanIntMean, ScanIntSD;  // Mean and SD intervals
  int TiltDuringDelay;            // Delay time to starting continuous tilt for JEOL, in ms
  CString ChannelName[MAX_CHANNELS];  // STEM channels to acquire
  long ChannelIndex[MAX_CHANNELS];
  int ChanIndOrig[MAX_CHANNELS];  // Original channel index for looking up name after shot
  double STEMrotation;        // Rotation angle to set
  double PixelTime;           // Pixel dwell time
  int LineSyncAndFlags;       // Flag to do line sync for Digiscan: now flags
  int integration;            // Integration for JEOL STEM
  IFocusRamperPtr FocusRamper; // Pointer to focus ramper for FEI STEM
  float rampTable[MAX_RAMP_STEPS];
  double IndexPerMs;           // Table step per ms
  int ContinuousSTEM;
  int PlugSTEMacquireFlags;    // Acquire flags for plugin STEM
  BOOL MakeFEIerrorTimeout;    // Turn FEI COM error into timeout
  int GatanReadMode;           // The read mode of Gatan camera, -1 not to set it
  bool NeedsReadMode;          // Flag that plugin needs to be told the read mode
  double CountScaling;         // Amount to scale counts in counting mode
  long DoseFrac;               // More parameters for the K2 camera
  double FrameTime;
  long AlignFrames;
  long FilterName[MAX_FILTER_NAME_LEN / 4];
  long SaveFrames;
  long rotationFlip;
  long NumFramesSaved;          // Results from saving
  long ErrorFromSave;
  int NumAsyncSumFrames;        // -1 no async, 0 async no image, or # of frames to sum
  BOOL imageReturned;           // Flag that blanker proc can proceed
  bool GetDeferredSum;          // Just get a deferred sum and ignore most other items
  bool UseFrameAlign;
  double FaRawDist, FaSmoothDist, FaResMean, FaMaxResMax, FaMeanRawMax, FaMaxRawMax;
  // FRC values/frequencies times K2FA_FRC_INT_SCALE
  long FaCrossHalf, FaCrossQuarter, FaCrossEighth, FaHalfNyq;
};

struct InsertThreadData {
  int camera;                 // Camera number
  int DMindex;                // Index for DM-type camera
  int DE_camType;             // Flag for DE camera
  int FEItype;                // Flag for FEI camera, advanced only
  CamPluginFuncs *plugFuncs;  // Plugin Functions for current camera
  BOOL insert;                // flag to insert rather than retract
  DWORD delay;                // delay time, msec
  CameraThreadData *td;
};


class DLL_IM_EX CCameraController  
{
 public:
	 void AcquiredSize(ControlSet *csp, int camera, int &sizeX, int &sizeY);
	 void StartEnsureThread(DWORD timeout);
	 void SetupBeamScan(ControlSet *conSetp);
  static CWinThread *StartBlankerThread(CameraThreadData *td);
  static int WaitForBlankerThread(CameraThreadData * td, DWORD timeout, CString message);
  GetMember(int, DivideBy2)
  void SetDivideBy2(int inVal);
  GetSetMember(int, ScanSleep)
  GetSetMember(int, ScanMargin)
  GetSetMember(float, BeamWidth)
  GetSetMember(int, DriftISinterval);
  GetSetMember(float, InitialDriftSteps);
  float SpecimenBeamExposure(int camera, ControlSet *conSet, BOOL excludeDrift = false);
  float SpecimenBeamExposure(int camera, float exposure, float drift,
    BOOL excludeDrift = false);
  int SwitchTeitzToBeamShutter(int cam);
  int LockInitializeTietz(BOOL firstTime);
  BOOL IsCameraFaux();
  void SetBlankWhenRetracting(BOOL inVal) {mBlankWhenRetracting = inVal;};
  GetSetMember(float, MinBlankingTime)
  GetSetMember(int, ScaledGainRefMax)
  GetSetMember(int, MinGainRefBits)
  SetMember(float, GIFoffsetDelayCrit)
  SetMember(float, GIFoffsetDelayBase1)
  SetMember(float, GIFoffsetDelaySlope1)
  SetMember(float, GIFoffsetDelayBase2)
  SetMember(float, GIFoffsetDelaySlope2)
  SetMember(int, GIFslitInOutDelay)
  SetMember(BOOL, NoSpectrumOffset)
  GetSetMember(BOOL, IgnoreFilterDiffs);
  SetMember(float, DarkAgeLimit)
  SetMember(float, GainTimeLimit)
  SetMember(int, GIFBiggestAperture)
  SetMember(float, TietzFilmSwitchTime)
  SetMember(float, TimeoutFactor)
  SetMember(int, RetryLimit)
  GetMember(BOOL, ShotIncomplete)
  GetSetMember(BOOL, InterpDarkRefs)
  SetMember(int, BlockOffsetSign)
  SetMember(int, DynFocusInterval)
  SetMember(int, StartDynFocusDelay);
  GetSetMember(int, LowerScreenForSTEM);
  SetMember(BOOL, BlankNextShot)
  SetMember(int, DSextraShotDelay);
  SetMember(int, DSshouldFlip);
  SetMember(int, DScontrolBeam);
  SetMember(float, DSglobalRotOffset);
  SetMember(int, SubareaShiftDelay);
  GetSetMember(int, InvertBrightField);
  SetMember(float, InsertDetShotTime);
  SetMember(int, MaxChannelsToGet);
  SetMember(int, RetainMagAndShift);
  SetMember(float, DSsyncMargin);
  GetSetMember(int, NumK2Filters);
  GetSetMember(float, MinK2FrameTime);
  SetMember(float, K2BaseModeScaling);
  SetMember(BOOL, SkipNextReblank);
  SetMember(int, DefaultGIFCamera);
  SetMember(int, DefaultRegularCamera);
  GetMember(float, LastK2BaseTime);
  GetSetMember(float, K2ReadoutInterval);
  CString *GetK2FilterNames() {return &mK2FilterNames[0];};
  GetSetMember(float, FalconReadoutInterval);
  SetMember(int, MaxFalconFrames);
  int GetMaxFalconFrames(CameraParameters *params);
  SetMember(BOOL, FrameSavingEnabled);
  BOOL GetFrameSavingEnabled() {return mFrameSavingEnabled || mCanUseFalconConfig > 0;};
  GetSetMember(CString, FalconFrameConfig);
  GetSetMember(CString, LocalFalconFramePath);
  GetSetMember(CString, FalconConfigFile);
  GetSetMember(int, CanUseFalconConfig);
  int *GetIgnoreDMList(int index) {return &mIgnoreDMList[index][0];};
  void SetNumIgnoreDM(int index, int inVal) {mNumIgnoreDM[index] = inVal;};
  CArray<ChannelSet, ChannelSet> *GetChannelSets() {return &mChannelSets;};
  BOOL GetProcessHere();
  void SetProcessHere(BOOL inVal);
  SetMember(int, RefMemoryLimit)
  void KillUpdateTimer();
  double EasyRunScript(CString command, int selectCamera, int DMindex);
  int CheckFilterSettings();
  void ReleaseCamera(int createFor);
  BOOL CreateCamera(int createFor, bool popupError = true);
  void ShowReference(int type);
  int DeleteReferences(int type, bool onlyDMRefs);
  int AddRefToArray(DarkRef *newRef);
  GetMember(BOOL, DebugMode)
  void SetDebugMode(BOOL inVal);
  BOOL GetInitialized();
  int SetupFilter(BOOL acquiring = false);
  GetMember(BOOL, Halting)
  int ConSetToLDArea(int inConSet);
  SetMember(int, RequiredRoll)
  SetMember(BOOL, ObeyTiltDelay)
  SetMember(BOOL, MakeFEIerrorBeTimeout);
  void QueueMagChange(int inMagInd);
  void QueueImageShift(double inISX, double inISY, int inDelay);
  void QueueStageMove(StageMoveInfo inSmi, int inDelay, bool doBacklash = false);
  void RestartCapture();
  BOOL Settling() {return mSettling >= 0;};
  void ErrorCleanup(int error);
  static int TaskCameraBusy();
  static int TaskGettingFrame();
  void DisplayNewImage(BOOL acquired);
  void AcquireCleanup(int error);
  static void AcquireError(int error);
  static void AcquireDone(int param);
  void EnsureCleanup(int error);
  static void EnsureError(int error);
  static void EnsureDone(int param);
  void StartAcquire();
  void ScreenOrInsertCleanup(int error);
  static void ScreenOrInsertError(int error);
  static UINT InsertProc(LPVOID pParam);
  static UINT EnsureProc(LPVOID pParam);
  static UINT AcquireProc(LPVOID pParam);
  static UINT BlankerProc(LPVOID pParam);
  static UINT HWDarkRefProc(LPVOID pParam);
  int InsertBusy();
  int EnsureBusy();
  int AcquireBusy();
  static void ScreenOrInsertDone(int inSet);
  static int ThreadBusy();
  void SetCurrentCamera(int currentCam, int activeCam);
  int Initialize(int whichCameras);
  BOOL CameraReady();
  void Capture(int inSet, bool retrying = false);
  void StopCapture(int ifNow);
  void SetPending(int whichMode);
  void InitiateCapture(int whichMode);
  BOOL CameraBusy();
  CCameraController();
  virtual ~CCameraController();
  GetSetMember(BOOL, ScreenDownMode)
  GetSetMember(BOOL, SimulationMode)
  GetSetMember(BOOL, OneK2FramePerFile);
  GetSetMember(BOOL, NoK2SaveFolderBrowse);
  GetSetMember(CString, DirForK2Frames);
  GetSetMember(CString, DirForFalconFrames);
  GetSetMember(CString, DirForDEFrames);
  GetSetMember(float, ScalingForK2Counts);
  GetSetMember(int, SaveRawPacked);
  GetSetMember(int, SaveTimes100);
  GetSetMember(int, K2SaveAsTiff);
  GetSetMember(BOOL, NameFramesAsMRCS);
  GetSetMember(BOOL, Use4BitMrcMode);
  GetSetMember(BOOL, SaveUnnormalizedFrames);
  SetMember(CString, SuperResRef);
  SetMember(CString, CountingRef);
  GetSetMember(BOOL, RunCommandAfterSave);
  SetMember(CString, PostSaveCommand);
  SetMember(BOOL, OtherCamerasInTIA);
  GetMember(CString, FrameFilename);
  SetMember(BOOL, DeferStackingFrames);
  SetMember(int, SmoothFocusExtraTime);
  SetMember(BOOL, K2SynchronousSaving);
  SetMember(int, NumZLPAlignChanges);
  SetMember(float, MinZLPAlignInterval);
  SetMember(BOOL, LDwasSetToArea);
  GetSetMember(int, FrameMdocForFalcon);
  GetSetMember(BOOL, SkipK2FrameRotFlip);
  SetMember(int, BaseJeolSTEMflags);
  GetMember(int, RepFlag);
  GetMember(double, LastImageTime);
  GetSetMember(int, WaitingForStacking);
  bool SetNextAsyncSumFrames(int inVal, bool deferSum);
  SetMember(float, K2MaxRamStackGB);
  SetMember(bool, CancelNextContinuous);
  void SetTaskWaitingForFrame(bool inVal) {mTaskFrameWaitStart = inVal ? GetTickCount() : -1.;};
  bool GetTaskWaitingForFrame() {return mTaskFrameWaitStart >= 0.;};
  SetMember(float, ContinuousDelayFrac);
  GetSetMember(int, PreventUserToggle);
  GetSetMember(int, FrameNumberStart); 
  GetSetMember(int, FrameNameFormat); 
  GetSetMember(CString, FrameBaseName);
  GetSetMember(CString, NumberedFramePrefix);
  GetSetMember(CString, NumberedFrameFolder);
  GetSetMember(int, LastFrameNumberStart);
  GetSetMember(int, LastUsedFrameNumber);
  GetSetMember(int, DigitsForNumberedFrame);
  GetSetMember(BOOL, AntialiasBinning);
  GetMember(CString, NewFunctionCalled);
  GetSetMember(BOOL, NoNormOfDSdoseFrac);
  GetSetMember(float, K2MinStartupDelay);
  GetSetMember(CString, AlignFramesComPath);
  GetSetMember(BOOL, ComPathIsFramePath);
  GetSetMember(BOOL, AlignWholeSeriesInIMOD);
  GetSetMember(CString, PathForFrames);
  SetMember(float, DarkMaxSDcrit);
  SetMember(float, DarkMaxMeanCrit);
  SetMember(double, TaskFrameWaitStart);
  GetSetMember(int, NumFrameAliLogLines);
  GetMember(bool, DeferredSumFailed);
  int GetDMversion(int ind) {return mDMversion[ind];};
  SetMember(BOOL, AllowSpectroscopyImages);
  GetMember(bool, AskedDeferredSum);
  GetSetMember(BOOL, ASIgivesGainNormOnly);
  GetMember(bool, StartedFalconAlign);
  GetSetMember(int, Falcon3AlignFraction);
  GetSetMember(int, MinAlignFractionsLinear);
  GetSetMember(int, MinAlignFractionsCounting);
  GetMember(int, NoMessageBoxOnError);
  SetMember(int, DEserverRefNextShot);
  SetMember(float, DEPrevSetNameTimeout);
  int GetServerFramesLeft();

  int GetNumFramesSaved() {return mTD.NumFramesSaved;};
  BOOL *GetUseGPUforK2Align() {return &mUseGPUforK2Align[0];};
  BOOL GetGpuAvailable(int DMind) {return mGpuMemory[DMind] > 0;};
  float *GetOneViewMinExposure() {return &mOneViewMinExposure[0];};
  float *GetOneViewDeltaExposure() {return &mOneViewDeltaExposure[0];};
  CArray<FrameAliParams, FrameAliParams> *GetFrameAliParams() {return &mFrameAliParams;};
  int GetNumFrameAliParams() {return (int)mFrameAliParams.GetSize() - 1;};
  void ChangePreventToggle(int incr)
  {mPreventUserToggle = (mPreventUserToggle + incr > 0) ? mPreventUserToggle + incr : 0;};
  BOOL Raising() {return mRaisingScreen != 0;};
  BOOL Inserting() {return mInserting;};
  BOOL EnsuringDark() {return mEnsuringDark;};
  BOOL Acquiring() {return mAcquiring;};
  void AdjustSizes(int &DMsizeX, int ccdSizeX, int moduloX, 
                   int &Left, int &Right, int &DMsizeY, int ccdSizeY, int moduloY, 
                   int &Top, int &Bottom, int binning, int camera = -1);
  void CenteredSizes(int &DMsizeX, int ccdSizeX, int moduloX, 
                     int &Left, int &Right, int &DMsizeY, int ccdSizeY, int moduloY, 
                     int &Top, int &Bottom,int binning, int camera = -1);
  void BlockAdjustSizes(int &DMsize, int ccdSize, int sizeMod, int startMod,
                        int &start, int &end, int binning);
  void GetLastCoords(int &top, int &left, int &bottom, int &right) {top = mTop;
    left = mLeft; bottom = mBottom; right = mRight;};

 private:
  void AdjustSizes(int &DMsizeX, int ccdSizeX, int moduloX, 
                   int &Left, int &Right, int binning, int camera = -1);
  CSerialEMApp *mWinApp;
  EMbufferManager *mBufferManager;
  CEMscope  *mScope;
  CShiftManager *mShiftManager;
  CFalconHelper *mFalconHelper;
  EMimageBuffer *mImBufs;
  CWinThread *mInsertThread;
  CWinThread *mAcquireThread;
  CWinThread *mEnsureThread;
  ControlSet *mConSetsp;
  ControlSet *mCamConSets;
  CameraParameters *mParam;
  CameraParameters *mAllParams;
  int *mActiveList;
  IDMCamera *mGatanCamera;    // A member pointer, transient or not
  CamPluginFuncs *mPlugFuncs[MAX_CAMERAS];

  DarkRef mDarkRefs[MAX_CAMERAS][MAX_DARK_REFS];
  DarkRef mDRdelete[MAX_DARK_REFS];
  CArray<DarkRef *, DarkRef *> mRefArray;
  CArray<FrameAliParams, FrameAliParams> mFrameAliParams;

  BOOL mScreenDownMode;   // Flag to leave screen down during capture
  BOOL mSimulationMode;   // Flag to acquire through simulation
  int mRaisingScreen;
  BOOL mInserting;
  BOOL mAcquiring;
  BOOL mHalting;
  BOOL mEnsuringDark;
  BOOL mShotIncomplete;   // Flag that the last acquire sequence was not completed
  BOOL mDiscardImage;     // Flag to discard last image after stopping continuous acquire

  int mFilmShutter;       // Code to use film shutter
  int mSettling;          // Has set number if delaying for some time

  CameraThreadData mTD;   // Parameters to pass to the threads
  InsertThreadData mITD;
  int mBinning;
  int mDMsizeX, mDMsizeY;
  double mExposure;
  int   mTop;
  int   mLeft;
  int   mBottom;
  int   mRight;
  float   mDelay;
  BOOL mForce;
  BOOL mPreDarkBump;      // Flag that pre-exposure dark ref exposure was bumped
  int mLastConSet;        // Conset used for parameters
  int mRepFlag;           // Set number for continuous capture
  int mPending;           // Set number for a pending capture
  int mNumRetries;        // Number of retries after timeout
  int mRetryLimit;        // Limit on number of retries (default 0 to disable)
    
  DarkRef *mDarkp;        // pointer to current dark ref
  DarkRef *mGainp;        // Pointer to gain reference
  DarkRef *mDarkBelow;    // Points to dark references for interpolation
  DarkRef *mDarkAbove;
  int mUseCount;          // Counter for use of dark refs
  int mNumDRdelete;       // Number of dark refs to delete in DM
  int mRefMemoryLimit;    // Limit on memory usage for references
  int mConSetRefLimit;    // Limit on number of refs for a control set
  float mDarkAgeLimit;    // Age limit for dark refs in seconds
  float mGainTimeLimit;   // Time since last camera access after which gain refs expire
  double mLastImageTime;  // Time stamp of last image acquire, for expiring gain refs
  BOOL mProcessHere;      // Flag to process data here
  int mDivideBy2;         // Flag to divide 16-bit data by 2
  int mDivBy2ForImBuf;    // Whether division by 2 into unsigned, to be stored in imbuf
  int mDivBy2ForExtra;    // Actual # of divisions by 2 value to be stored in extra
  float mMinBlankingTime; // Minimum time that it takes to blank/unblank beam

  BOOL mStageQueued;            // flag that stage move queued for postaction
  StageMoveInfo mSmiToDo;       // Stage move to do
  int mStageDelay;              // delay to impose
  BOOL mISQueued;               // Image shift queued
  double mISToDoX, mISToDoY;    // image shift to do
  int mISDelay;                 // delay for image shift
  BOOL mMagQueued;              // mag change queued
  int mMagIndToDo;              // mag to change to;
  int mTiltDuringShotDelay;     // Delay time to starting tilt during shot (< 0 for none)
  float mFocusInterval1, mFocusInterval2;    // First and second interval to focus steps
  double mFocusStepToDo1, mFocusStepToDo2;  // The focus steps to do, or range for smooth
  int mSmoothFocusExtraTime;    // Extra time for focus ramp in SmoothFocus operation
  BOOL mDriftISQueued;		 	    // Flag that drift compensation by IS movement is queued
  float mDriftXnmPerSec;		    // Nm per second in X and Y queued
  float mDriftYnmPerSec;
  int mDriftISinterval;         // Interval between moves
  float mInitialDriftSteps;     // Number of steps to take immediately
  int mDynFocusInterval;        // Minimum interval for dynamic focus in STEM
  int mStartDynFocusDelay;      // Time to wait after setting focus at start of ramp
  DWORD mStartTime;             // for reporting timing
  int mContinuousCount;         // Count of frames in continuous mode
  double mContinStartTime;      // Time of second frame
  float mTiltBefore;            // Tilt angle acquired before shot
  int mMagBefore;               // Mag index before shot
  double mStageXbefore;         // Stage position before move
  double mStageYbefore;
  double mStageZbefore;
  int mRequiredRoll;            // override to standard buffer roll requested by tasks
  BOOL mObeyTiltDelay;          // flag to enforce delay after tilt
  BOOL mDebugMode;
  int mGIFBiggestAperture;      // Number of aperture for imaging
  BOOL mCOMBusy;                // flag that there is a camera connection active
  UINT_PTR mFilterUpdateID;     // ID of timer for filter updates
  BOOL mWasSpectroscopy;        // flag that GIF was in spectroscopy on last update
  DWORD mBackToImagingTime;     // Time when switched from spectroscopy to imaging
  float mGIFoffsetDelayCrit;    // Criterion offset for switching between:
  float mGIFoffsetDelayBase1;   // base delay in DM ticks, small offsets
  float mGIFoffsetDelaySlope1;  // increase per EV of offset
  float mGIFoffsetDelayBase2;   // base delay in DM ticks, large offsets
  float mGIFoffsetDelaySlope2;  // increase per EV of offset
  int mGIFslitInOutDelay;       // Ticks of delay when slit in/out
  int mNumZLPAlignChanges;      // minimum number of energy shift changes
  float mMinZLPAlignInterval;   // in minimum time interval
  double mLastEnergyShift;
  int mShiftTimeIndex;          // Index at which to place next shift time
  BOOL mNoSpectrumOffset;       // Flag that there is no spectrum offset on system
  BOOL mIgnoreFilterDiffs;      // Flag to prevent state from changing filter param
  int mScaledGainRefMax;        // Maximum value for integer gain ref
  int mMinGainRefBits;          // Minimum bit shift for scaling of integer gain ref
  BOOL mBlankWhenRetracting;    // Flag to blank whenever retracting a camera
  int mNumDMCameras[3];         // Number of DM cameras of COM, socket, or AMT
  int mNumTietzCameras;         // Number of Tietz cameras
  int mNumDECameras;	        	//Number of NCMIR developed cameras
  BOOL mDEInitialized;		      //Flag that a NCMIR cam was connected.
  BOOL mDMInitialized[3];       // Flag that different kinds of DM interfaces contacted
  BOOL mTietzInitialized;       // Flag that Tietz camera connection was made
  BOOL mFEIinitialized;         // Flag that FEI camera instance was created
  BOOL mPlugInitialized;        // Flag that some plugin cameras were initialized
  float mTietzFilmSwitchTime;   // Pre-exposure time needed if switching to film shutter
  int mCameraWithBeamOn;        // Camera # of camera that was in Tietz film shutter mode
  BOOL mTietzInstances;         // Flag that instances of Tietz modules were created
  BOOL mShutterInstance;        // Flag that shutterbox instance was created
  BOOL mAMTactive;              // Flag that active camera is an AMT
  double mStartingISX;          // Image shift before shot started
  double mStartingISY;
  int mNumAverageDark;          // Number of dark references to average
  int mAverageDarkCount;        // Counter for averaging dark references
  int *mDarkSum;                // Array for summing dark refs
  BOOL mInterpDarkRefs;         // Flag to interpolate between two dark references
  float mInterpRefInterval;     // Interval between exposure times when using two refs
  float mMinInterpRefExp;       // Minimum exposure time 
  float mTimeoutFactor;         // Factor to increase timeout values by
  float mBeamWidth;             // Width for scanning with a slit (unbinned pixels)
  int mScanMargin;              // Margin beyond image frame on each side (unbinned pix)
  int mScanSleep;               // Sleep time between beam movements (ms)
  double mCenterFocus;          // Defocus at center, initial value to restore at end
  double mCenterBeamX, mCenterBeamY;  // Beam X and Y values at center
  int mBlockOffsetSign;         // Sign for Tietz block gain offset
  BOOL mOppositeAreaNextShot;   // Flag to go to opposite LD area on next shot
  BOOL mLDwasSetToArea;         // External flag that LD area already set
  int mNumIgnoreDM[3];          // Number of cameras to ignore on each interface
  int mIgnoreDMList[3][MAX_IGNORE_GATAN];
  BOOL mNeedToSelectDM;         // A DM camera was switched to and must be selected
  float mDefaultCountScaling;   // Scaling to use if counts per electron not set
  bool mNeedsReadMode[3];       // Flag that read mode needs to be set for interface
  double mDSlineFreq;           // Digiscan line frequency
  bool mStartedScanning;        // Flag that scanning-type changed need to be restored
  int mScreenUpSTEMdelay;       // Time (ms) to wait after screen raise switches to STEM
  int mLowerScreenForSTEM;      // 1 = lower when taking shot; 2 = lower when switch
  BOOL mRamperInstance;         // Flag that FocusRamper was created
  BOOL mBlankNextShot;          // Flag to blank beam during next shot
  int mDEserverRefNextShot;     // # of repeats to do a reference in DE server next shot
  int mDSextraShotDelay;        // Extra delay time after nominal time to acquire image
  int mDSshouldFlip;            // Flipping value that DS should have
  float mDSglobalRotOffset;     // Rotation offset that it should have
  BOOL mShiftedISforSTEM;       // Flag that shift was applied to acquire shifted subarea
  int mSubareaShiftDelay;       // Delay after shifting for subarea
  int mAdjustmentShiftDelay;    // Delay after shifting for image adjustment
  int mTotalSTEMShiftDelay;     // Delay actually applied
  int mPostSTEMmagChgDelay;     // Delay after changing STEM mag
  double mIStoRestoreX;         // The shift values to restore
  double mIStoRestoreY;
  float mDSsyncMargin;          // Extra microseconds per line to allow when line sync on
  int mDScontrolBeam;           // 1 to move to safe position, 2 to move to fixed position
  int mInvertBrightField;       // 1 invert if invert on, -1 invert if off, 0 do nothing
  BOOL mFEIdetectorInserted[MAX_STEM_CHANNELS];
  float mInsertDetShotTime;     // Exposure time for inserting detector
  int mNeedShotToInsert;        // set # when doing a shot, or -1 if not
  int mMaxChannelsToGet;        // Maximum number of channels on next shot
  int mMagToRestore;            // Mag index to restore
  int mRetainMagAndShift;       // set # to retain mag and shift change after, -1 not to
  float mAdjustShiftX, mAdjustShiftY;   // Unbinned pixels to adjust position for STEM
  CArray<ChannelSet, ChannelSet> mChannelSets;
  bool mFoundCombo;
  BOOL mMakeFEIerrorBeTimeout;  // Flag to convert an FEI error to a timeout for retries
  CString mK2FilterNames[MAX_K2_FILTERS];
  int mNumK2Filters;
  float mMinK2FrameTime;        // Minimum frame time allowed in dose fractionation
  float mK2ReadoutInterval;       // Actual readout interval, base for all times
  int mZoomFilterType;          // Type for antialias filter
  BOOL mOneK2FramePerFile;      // Flag to save one file per frame
  CString mDirForK2Frames;      // Directory to save them in
  CString mPathForFrames;       // Name actually used on this acquisition
  float mScalingForK2Counts;    // Scaling if different from counts per electron
  float mK2BaseModeScaling;     // Linear scaling factor for K2 Base camera
  BOOL mAntialiasBinning;       // Flag to use antialiasing in plugin for non SR/dosefrac
  float mOneViewDeltaExposure[MAX_BINNINGS];   // Exposure time increments for OneView
  float mOneViewMinExposure[MAX_BINNINGS];    // Minimum exposure times
  int mMaxFalconFrames;         // Maximum number of intermediate Falcon frames
  float mFalconReadoutInterval; // Frame interval for Falcon camera
  BOOL mFrameSavingEnabled;     // Flag that frame-saving is enabled in the FEI dialog
  CString mFalconFrameConfig;   // Name of file for controlling frame-saving
  CString mLocalFalconFramePath; // Place that frames can be written by FEI
  CString mFalconConfigFile;    // Path/Name of FalconConfig.xml
  int mCanUseFalconConfig;      // -1 not to, 0 read-only, 1 read/write
  bool mSavingFalconFrames;     // per-shot flag if it is happening
  bool mStartedFalconAlign;     // Flag that DisplayNewImage started alignment and left
  bool mAligningFalconFrames;   // Flag that Falcon frames are to be aligned here
  bool mRemoveFEIalignedFrames; // Flag to remove frames aligned by FEI
  bool mRestoreFalconConfig;    // Flag that config file needs to be restored after shot
  int mFalcon3AlignFraction;    // Number of frames in an alignment fraction for Falcon 3
  int mMinAlignFractionsLinear; // Minimum # of alignment fractions for aligning linear
  int mMinAlignFractionsCounting; // Minimum # of alignment fractions for aligning counting
  BOOL mDeferStackingFrames;    // One-shot flag to defer stacking on next shot
  BOOL mStackingWasDeferred;    // Flag that it happened, so setup should be skipped
  int mFrameMdocForFalcon;      // 1 to save in mdoc for Records, 2 for all
  BOOL mFalconAsyncStacking;    // Flag to stack frames asynchronously
  int mWaitingForStacking;      // Flag that we are waiting for Falcon stacking
  CString mDirForFalconFrames;  // Directory or subfolder to save Falcon frames in
  BOOL mOtherCamerasInTIA;      // Flag that FEI name needs to be checked before acquiring
  BOOL mSkipNextReblank;        // Flag to not blank readout in next shot
  int mDefaultGIFCamera;        // Active camera number of "first GIF camera"
  int mDefaultRegularCamera;    // Active camera number of "first regular camera"
  float mBaseK2CountingTime;    // Counting and super-res need to be multiples of these
  float mBaseK2SuperResTime;
  float mLastK2BaseTime;        // Base time last time that exposure time was constrained
  BOOL mNoK2SaveFolderBrowse;   // Flag not to show the Browse button when picking folder
  int mSaveRawPacked;           // Pack flags: 1 bytes->4bits/ints->bytes + 2 ints->4bits
  int mSaveTimes100;            // Flag to save times 100 in 16 bits
  int mK2SaveAsTiff;            // 1 for lzw compression, 2 for zip compression
  BOOL mUse4BitMrcMode;         // Flag to use 4-bit mode 101
  BOOL mSaveUnnormalizedFrames; // Flag to save frames as unnormalized
  BOOL mSkipK2FrameRotFlip;     // Flag to have plugin skip reorienting frames
  BOOL mNameFramesAsMRCS;       // Flag to have plugin use extension .mrcs for frame stack
  BOOL mK2SynchronousSaving;    // Flag to save the old way, synchronously
  CString mSuperResRef;         // Name of super-res reference to save with packed bytes
  CString mCountingRef;         // Name of counting mode reference file
  BOOL mRunCommandAfterSave;    // Flag to run a command after saving frames
  CString mPostSaveCommand;     // Command to run after saving (future)
  bool mInDisplayNewImage;      // Flag for proper testing of continuous mode
  int mNoMessageBoxOnError;     // -1 for macro processor no MB set, 1 for TS with term
  int mDMversion[3];
  int mPluginVersion[3];
  int mNextAsyncSumFrames;      // 0 for async next shot, > 0 to return partial sum
  DWORD mLastAsyncTimeout;      // timeout for last async shot
  double mAsyncTickStart;       // time it started
  float mLastAsyncExpTime;      // Just the exposure time of last async shot
  float mK2MaxRamStackGB;       // Maximum GB to allow for a RAM stack/grab stack
  float mK2MinFrameForRAM;      // Minimum frame time for using RAM stack
  int mBaseJeolSTEMflags;       // Basic flags to which div by 2 and continuous are added
  bool mCancelNextContinuous;   // Flag to cancel continuous mode on next shot
  int mSingleContModeUsed;      // Value of mode to use on this shot/used on last shot
  double mTaskFrameWaitStart;   // Start time for when a task waits for frame
  float mContinuousDelayFrac;   // Fraction of normal delay to apply in continous mode
  int mPreventUserToggle;       // Flag not to let user toggle continuous mode
  bool mStoppedContinuous;      // Set when stop to prevent settling timeout from capture
  CString mDirForDEFrames;      // Single directory name for a subfolder of main location
  float mDEPrevSetNameTimeout;  // Sec of timeout when fetching set name
  float mDESetNameTimeoutUsed;  // Actual value used for timeout
  int mFrameNameFormat;         // Set of flags for components of folder/filename 
  int mFrameNumberStart;        // Starting number for sequential numbers
  CString mFrameBaseName;       // User's base name
  CString mFrameFilename;       // Filename component for frame to save
  CString mFrameFolder;         // Path for frame to save
  CString mNumberedFramePrefix; // Prefix before number/date/time in last numbered frame
  CString mNumberedFrameFolder; // Last folder used when numbering
  int mLastFrameNumberStart;    // The starting number last time frame was saved
  int mLastUsedFrameNumber;     // The number actually used last time
  int mDigitsForNumberedFrame;  // Number of digits for sequential numbers
  CString mNewFunctionCalled;   // Description of new function being called in plugin
  int mK2GainRefTimeStamp[2][2]; // Time when a K2 gain reference last used; -1 not loaded
  int mK2GainRefLifetime;       // minutes before purging
  BOOL mNoNormOfDSdoseFrac;     // Flag not to do normalization
  float mK2MinStartupDelay;     // Minimum startup delay to allow post-actions for K2
  double mGpuMemory[2];         // GPU memory for K2 frame alignment, each channel
  BOOL mUseGPUforK2Align[3];    // Channel-specific flag to use GPU; third one for IMOD
  CString mAlignFramesComPath;  // Path where com files should be written
  BOOL mComPathIsFramePath;     // Flag to put the com file in the frame folder
  BOOL mAlignWholeSeriesInIMOD;  // Flag to do IMOD alignment only for whole tilt series
  bool mGettingFRC;             // Flag that an FRC is being gotten
  int mTypeOfAlignError;         // -1 for no errors, 0 for unweighted, 1 for robust
  bool mDeferSumOnNextAsync;     // Make a deferred sum on early return shot
  bool mSetDeferredSize;         // Flag to keep track of need to save between entires
  bool mAskedDeferredSum;        // Flag that deferred sum was requested
  bool mStartingDeferredSum;     // Flag that this acquire starts a deferred sum
  int mXdeferredSize;            // Size expected for image
  int mYdeferredSize;
  int mXdeferredSizeTD;          // Size stored in TD
  int mYdeferredSizeTD;
  int mReadModeDeferred;          // Other items to get deferred sum properly processed
  int mBinDeferred;
  int mBinDeferredTD;
  int mSavedInDeferred;
  bool mAlignedDeferred;
  int mLeftDeferred, mRightDeferred;
  int mTopDeferred, mBotDeferred;
  int mLastDeferredConSet;
  ControlSet *mConsDeferred;
  EMimageBuffer *mBufDeferred;
  EMimageExtra *mExtraDeferred;
  bool mStartedExtraForDEalign;  // Flag that an extra header was made to start DE align
  int mDoingDEframeAlign;        // Flag that align is being done here or IMOD
  bool mRemoveAlignedDEframes;   // Flag to remove DE frames if aligning only
  float mDarkMaxSDcrit;          // Sd for testing dark reference against if > 0
  float mDarkMaxMeanCrit;        // Mean for testing dark reference against if > 0
  int mBadDarkNumRetries;        // Number of retries if there is a bad dark reference
  int mBadDarkCount;             // Current count of bad dark refs
  int mNumFrameAliLogLines;            // Number of frame align log lines
  bool mDeferredSumFailed;       // Flag that getting deferred sum failed
  BOOL mAllowSpectroscopyImages; // Flag to allow images to be taken in spectroscopy
  BOOL mASIgivesGainNormOnly;    // Flag that advanced scripting interface only does norm
  float mPriorRecordDose;        // Cumulative dose prior to shot if doing tilt series
  int mNumSubsetAligned;         // Number of frames aligned and returned in sum, 0 if all
  int mAlignStart, mAlignEnd;    // Starting and ending frames numbered from 1 as in UI

public:
  void SetNonGatanPostActionTime(void);
  static int GetArrayForImage(CameraThreadData * td, long &arrsize, int index = 0);
  static int GetArrayForReference(CameraThreadData * td, DarkRef * ref, long &arrSize, CString strGainDark);
  static int CopyFEIimage(CameraThreadData *td, SAFEARRAY * psa, void * array, 
    int sizeX, int sizeY, int imageType, int divideBy2);
  static int GetSizeCopyFEIimage(CameraThreadData *td, SAFEARRAY * psa, void * array, 
    int imageType, int divideBy2, int sizeX, int sizeY, int messInd);
  static int AcquireFEIimage(CameraThreadData * td, void * array, int correction, 
    double settling, int sizeX, int sizeY, int messInd);
  static int AcquireFEIchannels(CameraThreadData * td, int sizeX, int sizeY);
  static void AcquirePluginImage(CameraThreadData * td, void **array, int arrSize, 
    int processing, double settling, bool blanker, int &sizeX, int &sizeY, int &retval,
    int &numAcquired);
  static void AddStatsAndSleep(CameraThreadData * td, DWORD &curTime, DWORD &lastTime, 
    int &numScan, double &intervalSum, double &intervalSumSq, int stepInterval);

  BOOL PostActionsOK(void);
  double RefMemoryUsage(void);
  void TestGainFactor(short * array, int sizeX, int sizeY, int binning);
  void SetAMTblanking();
  void SetAMTblanking(bool blank);
  CString CurrentSetName(void);
  void QueueDriftRate(double driftX, double driftY, bool convertPixels);
  void SetupDriftWithIS(void);
  static void CCReportCOMError(CameraThreadData *td, _com_error E, CString inString);
  static BOOL CCTestHResult(CameraThreadData *td, HRESULT hr, CString inString);
  static void DeferMessage(CameraThreadData *td, CString inString);
  void UserToTietzCCD(int geometry, int binning, int &camSizeX, int &camSizeY, int &imSizeX,
    int &imSizeY, int &top, int &left, int &bottom, int &right);
  void TietzCCDtoUser(int geometry, int binning, int &camSizeX, int &camSizeY, int &imSizeX,
    int &imSizeY, int &top, int &left, int &bottom, int &right);
  void SetupNewDarkRef(int inSet, double exposure);
  void CheckGatanSize(int camNum, int paramInd);
  void ReportOnSizeCheck(int paramInd, int xsize, int ysize);
  bool OppositeLDAreaNextShot(void);
  bool InitiateIfPending(void);
  void TestCameraInserted(int actIndex, long &inserted);
  void ComputePixelTime(CameraParameters *camParams, int sizeX, int sizeY, int lineSync,
    float pixelSize, float maxScanRate, float &exposure, double &pixelTime, 
    double &scanRate);
  void ConstrainDriftSettling(float drift);
  int DynamicFocusOK(float exposure, int sizeY, float flyback, int &interval, 
    double &msPerLine);
  void SetupDynamicFocus(int numIntervals, double msPerLine, float flyback, float startup);
  BOOL CreateFocusRamper(void);
  int GetMaxChannels(CameraParameters * param);
  void StopContinuousSTEM(void);
  void TurnOffRepeatFlag(void);
  void OutOfSTEMUpdate(void);
  void CountSimultaneousChannels(CameraParameters * camParams, int * simultaneous, int maxSimul, int & numSimul, int & numAvail);
  void RestoreMagAndShift(void);
  void AdjustForShift(float adjustX, float adjustY);
  bool CanPreExpose(CameraParameters * param, int shuttering);
  bool MutuallyExclusiveChannels(int chan1, int chan2);
  int ChannelMappedTo(int chan);
  void BuildEvalChannelList(int numChan, int comboSize, int ninList, int * list);
  void InitializeDMcameras(int DMind, int *numDMListed, int *originalList, 
    int numOrig, BOOL anyGIF, int *digiscan, double addedFlyback);
  int InitializeTietz(int whichCameras, int *originalList, int numOrig, BOOL anyPreExp);
  void InitializeFEIcameras(int &numFEIlisted, int *originalList, int numOrig);
  void InitializeDirectElectron(int *originalList, int numOrig);
  void InitializePluginCameras(int &numPlugListed, int *originalList, int numOrig);
  int RotateAndReplaceArray(int chan, int operation, int invertCon);
  int CapSetupSTEMChannelsDetectors(ControlSet & conSet, int inSet, BOOL retracting);
  int CapManageScreen(int inSet, BOOL retracting, int numActive);

  // ControlSet is const here because the routine is called only the first time when
  // there is settling, so changes would be lost in that case
  int CapManageInsertTempK2Saving(const ControlSet &conSet, int inSet, BOOL retracting, 
    int numActive);
  int SetupK2SavingAligning(const ControlSet & conSet, int inSet, bool saving, bool aligning,
    CString *aliComRoot);
  int CapSetLDAreaFilterSettling(int inSet);
  void CapManageCoordinates(ControlSet &conSet, int &gainXoffset, int &gainYoffset);
  void CapSetupShutteringTiming(ControlSet & conSet, int inSet, BOOL &bEnsureDark);
  int CapManageDarkGainRefs(ControlSet & conSet, int inSet, BOOL &bEnsureDark, 
    int gainXoffset, int gainYoffset);
  int CapSaveStageMagSetupDynFocus(ControlSet & conSet, int inSet);
  bool ConstrainExposureTime(CameraParameters *camP, ControlSet *consP);
  bool ConstrainExposureTime(CameraParameters *camP, BOOL doseFrac, int readMode, 
    int binning, bool alignInCamera, int sumCount, float &exposure, float &frameTime);
  bool ConstrainFrameTime(float &frameTime);
  void RestoreFEIshutter(void);
  void QueueFocusSteps(float interval1, double focus1, float interval2, double focus2);
  static void ChangeDynFocus(CameraThreadData *td, double focus, double focusBase, 
    long fineBase, long coarseBase, long &last_coarse);
  float ExposureRoundingFactor(CameraParameters * camP);
  bool IsDirectDetector(CameraParameters * camP);
  int DoingContinuousAcquire(void);
  int UpdateK2HWDarkRef(float hoursSinceLast);
  CWinThread * StartHWDarkRefThread(void);
  void RotFlipOneCCDcoord(int operation, int camSizeX, int camSizeY, int &xx, int &yy);
  void SetFullSumAsyncIfOK(int inSet);
  bool FindNearestBinning(CameraParameters *camParam, ControlSet *conSet, int &binInd,
    int &realBin);
  void ComposeFramePathAndName(bool temporary);
  int GetPluginVersion(CameraParameters *camP);
  static int StartFocusRamp(CameraThreadData * td, bool & rampStarted);
  static void FinishFocusRamp(CameraThreadData *td, bool rampStarted);
  void CheckAndFreeK2References(bool unconditionally);
  CString MakeFullDMRefName(CameraParameters * camP, const char *suffix);
int GetDeferredSum(void);
bool OneViewDriftCorrectOK(CameraParameters * param);
void RollBuffers(int nRoll, int keepIndexCurrent);
bool HasNewK2API(CameraParameters * param);
void SetFrameAliDefaults(FrameAliParams & faParam, const char *name,
  int binning, float filt1, int sizeRestrict);
int MakeMdocFrameAlignCom(void);
int QueueTiltDuringShot(double angle, int delayToStart, double speed);
void RetractAllCameras(void);
float GetCountScaling(CameraParameters * camParam);
int TargetSizeForTasks(CameraParameters *camParam = NULL);
void RestoreGatanOrientations(void);
void GetMergeK2DefectList(int DMind, CameraParameters *param, bool errToLog);
bool IsConSetSaving(ControlSet *conSet, int setNum, CameraParameters *param, bool K2only);
bool CanProcessHere(CameraParameters *param);
void FixDirForFalconFrames(CameraParameters * param);
bool CanPluginDo(int minVersion, CameraParameters * param);
int NumAllVsAllFromFAparam(FrameAliParams &faParam, int numAliFrames, int &groupSize, 
  int &refineIter, int &doSpline, int &numFilters, float *radius2);
void AdjustCountsPerElecForScale(CameraParameters * param);
int DESumCountForConstraints(CameraParameters *camP, ControlSet *consP);
void MakeOneFrameAlignCom(CString & localFramePath, ControlSet *conSet);
};


#endif // !defined(AFX_CAMERACONTROLLER_H__C6BCA005_4648_455D_9AE8_566F127CB989__INCLUDED_)
