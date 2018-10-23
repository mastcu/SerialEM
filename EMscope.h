// EMscope.h: interface for the CEMscope class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EMSCOPE_H__3021271B_90D6_48ED_B7BA_5C7F62808186__INCLUDED_)
#define AFX_EMSCOPE_H__3021271B_90D6_48ED_B7BA_5C7F62808186__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#import "..\Packages\adaexp.exe" named_guids
#include "adaexp.tlh"
using namespace adaExp;     // Need this when importing too

#include "Shared\SharedJeolDefines.h"

class CShiftManager;
struct ScopePluginFuncs;

#define TRACE_MUTEX 0

#define STAGE_MIN_X 0
#define STAGE_MAX_X 1
#define STAGE_MIN_Y 2
#define STAGE_MAX_Y 3
#define MAX_ALPHAS  10
#define MAX_GAUGE_WATCH 6
enum {CAL_NTRL_FIND = 0, CAL_NTRL_RESTORE, CAL_NTRL_FOCUS};

#define pnmProjector 11
#define pnmObjective 10
#define pnmAll 12
#define nmAll 6
#define nmCondenser 3
#define nmSpotsize 1
enum {CassetteSlotStatus_Unknown, CassetteSlotStatus_Occupied, CassetteSlotStatus_Empty};
enum {RefrigerantLevel_AutoloaderDewar = 0,
    RefrigerantLevel_ColumnDewar, RefrigerantLevel_HeliumDewar};
enum {gsUnderflow = 1, gsOverflow, gsInvalid};
enum {imNanoProbe = 0, imMicroProbe};
enum {lpRegular = 1, lpEFTEM};
#define pmDiffraction 2
#define pmImaging 1
#define psmLAD 5

#define MAX_LONG_OPERATIONS 11
#define MAX_LONG_THREADS 2
enum {LONG_OP_BUFFER = 0, LONG_OP_REFILL, LONG_OP_INVENTORY, LONG_OP_LOAD_CYCLE,
LONG_OP_MESSAGE_BOX, LONG_OP_HW_DARK_REF, LONG_OP_UNLOAD_CART, LONG_OP_LOAD_CART,
LONG_OP_FILL_STAGE, LONG_OP_FILL_TRANSFER, LONG_OP_FLASH_FEG};

// Standard conversions from signed real to nearest integer for JEOL calls
#define NINT8000(a) (long)floor((a) + 0x8000 + 0.5)
#define LONGNINT(a) (long)floor((a) + 0.5)

// Plugin versions needed for particular versions
#define FEI_PLUGIN_STAGE_SPEED     103
#define FEI_PLUGIN_MANAGE_FAL_CONF 104
#define FEI_PLUGIN_DARK_FIELD      105
#define FEI_PLUGIN_MESSAGE_BOX     105
#define FEI_PLUGIN_DOES_FALCON3    106

struct StageMoveInfo {
  double x;
  double y;
  double z;
  double alpha;
  double speed;
  int axisBits;
  BOOL doBacklash;
  BOOL doRelax;
  BOOL useSpeed;
  BOOL inBackground;
  float backX;
  float backY;
  float backZ;
  float backAlpha;
  float relaxX;
  float relaxY;
  DWORD finishedTick;
  double distanceMoved;    // Must be set for post-action using X/Y moves
  JeolStateData *JeolSD;
  ScopePluginFuncs *plugFuncs;
};

struct StageThreadData {
  StageMoveInfo *info;
};

struct LongThreadData {
  int operations[MAX_LONG_OPERATIONS];
  bool finished[MAX_LONG_OPERATIONS];
  int numOperations;
  JeolStateData *JeolSD;
  ScopePluginFuncs *plugFuncs;
  CString errString;
};

// Globals for scope identity
extern bool JEOLscope, LikeFEIscope, HitachiScope;
extern bool FEIscope;
extern bool UsingScopeMutex;      // Flag that scope mutex is being used for this scope type

class DLL_IM_EX CEMscope
{
 public:
  void SetMessageWhenClipIS(BOOL inVal);
  BOOL GetMessageWhenClipIS();
  const char *GetC2Name() {return (LPCTSTR)mC2Name;};
  const char *GetC2Units() {return (LPCTSTR)mC2Units;};
  void SetJeolUsePLforIS(BOOL inVal);
  BOOL SetColumnValvesOpen(BOOL state, bool crashing = false);
  int GetColumnValvesOpen();
  BOOL BothLMorNotLM(int mag1, BOOL stem1, int mag2, BOOL stem2);
  void AddShiftBoundary(int inVal) {mShiftBoundaries[mNumShiftBoundaries++] = inVal;};
  void StopCalNeutralIS();
  void CalNeutralNextMag(int magInd);
  BOOL CalibratingNeutralIS() {return mCalNeutralStartMag >= 0;};
  void CalibrateNeutralIS(int calType);
  void SuspendUpdate(int duration);
  GetSetMember(BOOL, ReportsSmallScreen)
  GetSetMember(int, ReportsLargeScreen)
  GetSetMember(double, Jeol_CLA1_to_um)
  GetSetMember(double, Jeol_LMCLA1_to_um)
  double GetLMBeamShiftFactor() {return mJeol_LMCLA1_to_um / mJeol_CLA1_to_um;};
  GetSetMember(double, Jeol_OLfine_to_um)
  GetSetMember(double, Jeol_OM_to_um)
  GetSetMember(double, RoughPLscale)
  GetSetMember(BOOL, UpdateByEvent)
  GetSetMember(BOOL, SpectrumByEvent)
  void SetJeolStageRounding(float inVal);
  void SetJeolPiezoRounding(float inVal);
  float GetJeolStageRounding();
  float GetJeolPiezoRounding();
  GetSetMember(int, MagFixISdelay)
  GetSetMember(int, JeolForceMDSmode)
  GetSetMember(int, InitializeJeolDelay)
  GetSetMember(int, UpdateInterval)
  GetSetMember(int, JeolUpdateSleep)
  GetMember(BOOL, CanControlEFTEM)
  GetSetMember(BOOL, HasOmegaFilter)
  GetSetMember(int, SimulationMode)
  GetSetMember(BOOL, NoColumnValve)
  GetSetMember(BOOL, LDNormalizeBeam)
  GetSetMember(BOOL, LDBeamTiltShifts)
  GetSetMember(int, LDBeamNormDelay)
  float GetLDViewDefocus(int areaOrSet = VIEW_CONSET) {return areaOrSet != VIEW_CONSET ? mSearchDefocus : mLDViewDefocus;};
  void SetLDViewDefocus(float inVal, int area = VIEW_CONSET) {if (area) mSearchDefocus = inVal; else mLDViewDefocus = inVal;};
  GetSetMember(float, SearchDefocus)
  GetSetMember(BOOL, ShiftToTiltAxis)
  GetSetMember(BOOL, NoScope)
  GetMember(BOOL, ApplyISoffset)
  void SetApplyISoffset(BOOL inVal);
  void GetTiltAxisIS(double &ISX, double&ISY);
  GetSetMember(float, TiltAxisOffset)
  void SetStageLimit(int index, float inVal) {mStageLimit[index] = inVal;};
  float GetStageLimit(int index) {return mStageLimit[index];};
  BOOL NormalizeCondenser();
  void SetJeolPostMagDelay(int inVal, int inStage) {mJeolPostMagDelay = inVal; mPostMagStageDelay = inStage;};
  GetMember(int, JeolPostMagDelay);
  GetSetMember(int, JeolExternalMagDelay);
  GetSetMember(int, JeolMagEventWait);
  GetSetMember(int, JeolSTEMPreMagDelay);
  GetSetMember(int, NumSpotSizes)
  GetSetMember(int, MinSpotSize);
  GetSetMember(int, NumAlphas);
  GetMember(int, NumShiftBoundaries)
  int *GetShiftBoundaries() {return &mShiftBoundaries[0];};
  BOOL ScanMagnifications();
  float GetC2IntensityFactor(int probe = -1);
  void SetC2IntensityFactor(int probe, float inVal) {mC2IntensityFactor[probe] = inVal;};
  float GetC2SpotOffset(int spotSize, int probe = -1);
  void SetC2SpotOffset(int spot, int probe, float inVal) {mC2SpotOffset[spot][probe] = inVal;};
  double GetC2Percent(int spot, double intensity, int probe = -1);
  double GetCrossover(int spotSize, int probe = -1);
  void SetCrossover(int spot, int probe, double inVal) {mCrossovers[spot][probe] = inVal;};
  GetSetMember(int, LowestMModeMagInd)
  GetSetMember(int, LowestMicroSTEMmag)
  int GetLowestSTEMnonLMmag(int i) {return mLowestSTEMnonLMmag[i];};
  void SetLowestSTEMnonLMmag(int i, int val) {mLowestSTEMnonLMmag[i] = val;};
  GetSetMember(int, LowestGIFModeMag)
  GetSetMember(BOOL, Jeol1230)
  void HandleNewMag(int inMag);
  BOOL SetEFTEM(BOOL inState);
  BOOL GetEFTEM();
  BOOL SetProbeMode(int micro, BOOL fromLowDose = false);
  CArray<ChannelSet, ChannelSet> *GetBlockedChannels() {return &mBlockedChannels;};

  static void TaskScreenError(int error);
  static int TaskScreenBusy();
  void StageCleanup(int error);
  static void TaskStageError(int error);
  static int TaskStageBusy();
  static void TaskStageDone(int param);
  void Release();
  void StartUpdate();
  BOOL GetInitialized();
  BOOL SetSpotSize(int inIndex, BOOL normalize = false);
  void SetLowDoseDownArea(int inArea);
  GetMember(int, LowDoseDownArea);
  void GotoLowDoseArea(int inArea);
  void DoISforLowDoseArea(int inArea, int curMag, double &delISX, double &delISY);
  int GetLowDoseArea() {return mLowDoseSetArea;};
  void KillUpdateTimer();
  double GetReversalTilt();
  int GetSpotSize();
  int FastSpotSize();
  static void SetMagChanged(int inIndex);
  static void SetISChanged(double inISX, double inISY);
  BOOL SmallScreenIn();
  double GetIntensity();
  double GetIlluminatedArea();
  double FastIntensity();
  BOOL SetIntensity(double inVal);
  BOOL DelayedSetIntensity(double inVal, DWORD startTime);
  BOOL SetIlluminatedArea(double inVal);
  double GetImageDistanceOffset();
  BOOL SetImageDistanceOffset(double inVal);
  BOOL SetIntensityZoom(BOOL inVal);
  BOOL GetIntensityZoom();
  GetSetMember(float, SmallScreenFactor)
  GetSetMember(float, ScreenCurrentFactor)
  GetSetMember(double, StandardLMFocus)
  double GetStandardLMFocus(int magInd, int probe = -1);
  double *GetLMFocusTable() {return &mLMFocusTable[0][0];};
  void SetFocusToStandardIfLM(int magInd);
  GetSetMember(float, WarnIfKVnotAt)
  void GetLastISChange(double &delX, double &delY)
    {delX = mLastISdelX; delY = mLastISdelY;};
  GetSetMember(DWORD, LastNormalization)
  SetMember(DWORD, LastStageTime)
  SetMember(DWORD, LastTiltTime)
  DWORD GetLastTiltTime();
  DWORD GetLastStageTime();
  GetMember(double, LastTiltChange)
  GetSetMember(float, MaxTiltAngle);
  GetMember(BOOL, MagChanged)
  GetSetMember(int, MainDetectorID)
  GetSetMember(int, PairedDetectorID)
  GetSetMember(int, HasNoAlpha)
  GetSetMember(BOOL, UseIllumAreaForC2)
  GetMember(int, ProbeMode)
  SetMember(int, DiffractOrSTEMwait)
  SetMember(int, ProbeChangeWait)
  GetSetMember(int, InsertDetectorDelay);
  GetSetMember(int, SelectDetectorDelay);
  SetMember(BOOL, UsePLforIS);
  GetSetMember(BOOL, UseCLA2forSTEM);
  GetMember(BOOL, ChangingLDArea);
  BOOL GetClippingIS();
  GetSetMember(double, JeolSTEMdefocusFac);
  GetSetMember(int, JeolSTEMrotation);
  SetMember(int, JeolSwitchSTEMsleep);
  SetMember(int, JeolSwitchTEMsleep);
  SetMember(LowDoseParams *, LdsaParams);
  GetSetMember(BOOL, JeolSTEMunitsX);
  GetSetMember(float, BacklashTolerance);
  GetSetMember(BOOL, MovingStage);
  GetSetMember(BOOL, BkgdMovingStage);
  SetMember(BOOL, SkipNextBeamShift);
  GetMember(bool, DoingLongOperation);
  GetSetMember(BOOL, UsePiezoForLDaxis);
  GetSetMember(bool, FocusCameFromView);
  GetSetMember(int, FalconPostMagDelay);
  GetSetMember(int, LowestSecondaryMag);
  GetSetMember(int, SecondaryMode);
  GetSetMember(int, AlphaChangeDelay);
  GetMember(bool, Disconnected);
  GetSetMember(int, MagChgIntensityDelay);
  GetSetMember(int, HitachiSpotStepDelay);
  GetSetMember(int, HighestLMindexToScan);
  GetSetMember(int, PostProbeDelay);
  GetMember(int, LastNormMagIndex);
  GetSetMember(int, FakeMagIndex);
  GetSetMember(int, FakeScreenPos);
  GetMember(int, PluginVersion);
  GetSetMember(int, PostFocusChgDelay);
  GetSetMember(int, UseJeolGIFmodeCalls);
  GetSetMember(int, PostJeolGIFdelay);
  GetSetMember(int, UpdateBeamBlank);
  GetSetMember(BOOL, BlankTransients);
  SetMember(BOOL, SelectedEFTEM);
  GetMember(double, LastCameraLength);
  GetSetMember(float, StageRelaxation);
  void SetMessageBoxArgs(int type, CString &title, CString &message);
  int GetMessageBoxReturnValue(void);
  GetSetMember(BOOL, UseInvertedMagRange);
  GetMember(double, InternalMagTime);
  GetMember(double, UpdateSawMagTime);
  GetSetMember(BOOL, JeolHasNitrogenClass);
  void SetJeolReadStageForWait(BOOL inVal);
  BOOL GetJeolReadStageForWait();
  GetSetMember(BOOL, SkipAdvancedScripting);

  static void SetJeolIndForMagMode(int inVal);
  static int GetJeolIndForMagMode();
  static void SetJeolSecondaryModeInd(int inVal);
  static int GetJeolSecondaryModeInd();
  static int SetSecondaryModeForMag(int index);
  void SetIllumAreaLimits(float val1, float val2) {mIllumAreaLowLimit = val1; mIllumAreaHighLimit = val2;};
  void GetIllumAreaLimits(float &val1, float &val2) {val1 = mIllumAreaLowLimit; val2 = mIllumAreaHighLimit;};
  void GetIllumAreaMapTo(float &val1, float &val2) {val1 = mIllumAreaLowMapTo; val2 = mIllumAreaHighMapTo;};
  double *GetSpotBeamShifts(){return &mSpotBeamShifts[0][0][0];};
  float *GetAlphaBeamShifts(){return &mAlphaBeamShifts[0][0];};
  float *GetAlphaBeamTilts(){return &mAlphaBeamTilts[0][0];};
  GetSetMember(int, NumAlphaBeamShifts);
  GetSetMember(int, NumAlphaBeamTilts);
  GetSetMember(BOOL, AdjustFocusForProbe);
  SetMember(double, FirstFocusForProbe);
  GetSetMember(int, NormAllOnMagChange);
  void GetMinMaxBeamShiftSpots(int secondary, int &outMin, int &outMax)
  {outMin = mMinSpotWithBeamShift[secondary], outMax = mMaxSpotWithBeamShift[secondary];};
  void SetMinMaxBeamShiftSpots(int secondary, int inMin, int inMax)
  {mMinSpotWithBeamShift[secondary] = inMin, mMaxSpotWithBeamShift[secondary] = inMax;};
  void CopyBacklashValid() {mBacklashValidAtStart = mXYbacklashValid;};
  void NextLDAreaOpposite() {mNextLDpolarity = -1;};
  BOOL NormalizeProjector();
  BOOL NormalizeObjective();
  void BlankBeam(BOOL inVal);
  void SetBlankWhenDown(BOOL inVal);
  void SetLowDoseMode(BOOL inVal, BOOL hidingOffState = FALSE);
  GetMember(BOOL, LowDoseMode);
  void SetCameraAcquiring(BOOL inVal, float waitTime = 0.);
  GetMember(BOOL, CameraAcquiring);
  void SetShutterlessCamera(int inVal);
  BOOL GetBlankSet() {return mBeamBlankSet;};
  void ScreenCleanup();
  BOOL SetDefocus(double inVal, BOOL incremental = false);
  BOOL IncDefocus(double inVal);
  double GetDefocus();
  double GetUnoffsetDefocus();
  BOOL SetUnoffsetDefocus(double inVal);
  BOOL ResetDefocus(BOOL assumeInit = false);
  BOOL SetMagIndex(int inIndex);
  int FastMagIndex();
  int GetMagIndex(BOOL forceGet = false);
  BOOL SetCamLenIndex(int inIndex);
  int FastCamLenIndex();
  int GetCamLenIndex();
  double GetMagnification();
  double GetHTValue();
  BOOL GetStagePosition(double &X, double &Y, double &Z);
  BOOL GetLDCenteredShift(double &shiftX, double &shiftY);
  BOOL SetLDCenteredShift(double shiftX, double shiftY);
  BOOL GetImageShift(double &shiftX, double &shiftY);
  BOOL FastImageShift(double &shiftX, double &shiftY);
  BOOL SetImageShift(double shiftX, double shiftY);
  BOOL IncImageShift(double shiftX, double shiftY);
  BOOL ChangeImageShift(double shiftX, double shiftY, BOOL bInc);
  BOOL GetBeamShift(double &shiftX, double &shiftY);
  BOOL SetBeamShift(double shiftX, double shiftY);
  BOOL IncBeamShift(double shiftX, double shiftY);
  BOOL ChangeBeamShift(double shiftX, double shiftY, BOOL bInc);
  BOOL GetBeamTilt(double &shiftX, double &shiftY);
  BOOL SetBeamTilt(double shiftX, double shiftY);
  BOOL IncBeamTilt(double shiftX, double shiftY);
  BOOL ChangeBeamTilt(double shiftX, double shiftY, BOOL bInc);
  BOOL GetDarkFieldTilt(int &mode, double &tiltX, double &tiltY);
  BOOL SetDarkFieldTilt(int mode, double tiltX, double tiltY);
  double GetScreenCurrent();
  GetSetMember(int, UseTEMScripting);
  int *GetLastLongOpTimes() {return &mLastLongOpTimes[0];};
  int ScreenBusy();
  int GetScreenPos();
  BOOL SetScreenPos(int inPos);
  int StageBusy(int ignoreOrTrustState = 0);
  BOOL MoveStage(StageMoveInfo info, BOOL doBacklash = false, BOOL useSpeed = false,
    BOOL inBackground = false, BOOL doRelax = false);
  void TiltTo(double inVal);
  void TiltDown();
  void TiltUp();
  double GetTiltAngle(BOOL forceGet = false);
  double FastTiltAngle();
  void SetIncrement(float increment) {mIncrement = increment;};
  float GetIncrement();
  float GetBaseIncrement() {return mIncrement;};
  void SetCosineTilt(BOOL inVal) {m_bCosineTilt = inVal;};
  BOOL GetCosineTilt() {return m_bCosineTilt;};
  int Initialize();
  CEMscope();
  virtual ~CEMscope();

  static BOOL ScopeMutexAcquire(const char *name, BOOL retry);
  static BOOL ScopeMutexRelease(const char *name);
  static float ConvertJEOLStage(double inMove, float &outRem);
  static int LookupMagnification(int magValue, CString units, int mode, CString name,
    int &camLenInd);
  static int TranslateFunctionMode(short mode, CString name, int &newMode);
  static void SetBlankingFlag(BOOL state);
  static BOOL AcquireMagMutex();
  static void ReleaseMagMutex();
  static char *GetInstrumentName() {return mFEIInstrumentName;};
  static void StageMoveKernel(StageThreadData *std, BOOL fromBlanker, BOOL async,
    double &destX, double &destY, double &destZ, double &destAlpha);


  // JEOL functions
  BOOL SetMDSMode(int which);
  int GetMDSMode();
  BOOL SetSpectroscopyMode(int which);
  int GetSpectroscopyMode();
  static void WaitForStageDone(StageMoveInfo *smi, char *procName);

  // General variables that need to be public
  JeolStateData mJeolSD;
  JeolParams mJeolParams;
  int mNumGauges;                         // Number of gauges to watch
  CString mGaugeNames[MAX_GAUGE_WATCH];   // Names (P1, etc)
  double mYellowThresh[MAX_GAUGE_WATCH];  // Threshold for displaying yellow
  double mRedThresh[MAX_GAUGE_WATCH];     // Threshold for displaying red

private:
  static ScopePluginFuncs *mPlugFuncs;
  int *mActiveCamList;

  static int mLastMagIndex;          // Used by timer to detect user mag changes
  static BOOL mMagChanged;           // Flag that mag has changed since normalization
  static int mInternalPrevMag;       // Previous mag to one set internally
  static double mInternalMagTime;    // Time when internal mag was set
  static double mLastISX;
  static double mLastISY;
  static double mPreviousISX;  // IS before the last one seen in update
  static double mPreviousISY;
  static char *mFEIInstrumentName;
  int mUseTEMScripting;
  int mLastRegularMag;        // Keep track of last regular and EFTEM mags
  int mLastEFTEMmag;
  int mLastSTEMmag;
  double mLastSTEMfocus;      // Keep track of last focus values seen too
  double mLastRegularFocus;
  double mUpdateSawMagTime;   // Time of last mag change seen in update routine
  double mLastSeenMagInd;     // Index of last mag seen in update
  BOOL mBlankWhenDown;        // if blanking when screen down
  BOOL mLowDoseMode;          // flag for low dose
  BOOL mCameraAcquiring;      // flag to not blank when screen up in low dose
  int mLowDoseDownArea;       // Area (mode) to display when screen down
  int mLowDoseSetArea;        // Currently displayed area
  BOOL mChangingLDArea;       // Flag that area is changing, to stop double calls/updates
  int mLastLDpolarity;        // Polarity of last low dose area setting
  BOOL mSelectedEFTEM;        // flag that we turned on EFTEM lens settings
  int mSelectedSTEM;          // flag that we turned on STEM in scope; -1 while setting
  FilterParams *mFiltParam;
  MagTable *mMagTab;
  int mUpdateInterval;        // Update interval in ms
  int mJeolUpdateSleep;
  static HANDLE mScopeMutexHandle;
  static char * mScopeMutexOwnerStr;
  static char * mScopeMutexLenderStr;
  static DWORD  mScopeMutexOwnerId;
  static int    mScopeMutexOwnerCount;
  double mJeol_OLfine_to_um;  // Scale factor from OL fine value to microns
  double mJeol_CLA1_to_um;    // Scale factor from CLA1 units to microns, approximate
  double mJeol_LMCLA1_to_um;  // Scale factor for low mag, approximate
  double mJeol_OM_to_um;      // Scale from objective mini to microns
  double mJeolSTEMdefocusFac; // Scale to get defocus to match TEMCON in STEM mode
  double mRoughPLscale;       // Number of mm on camera per "micron" unit of PL
  BOOL mReportsSmallScreen;
  int mReportsLargeScreen;
  BOOL mUpdateByEvent;        // Flag that most updates will occur by event
  int mMagFixISdelay;         // Delay from setting mag to fixing IS values on JEOL
  int mCalNeutralStartMag;    // Starting mag if doing calibrate neutral IS
  int mCalNeutralType;        // Type of operation to do for Hitachi
  int mMagIndSavedIS;         // Mag at which IS was saved for later restore
  double mSavedISX, mSavedISY;  // RAW IS values to be restored at that mag
  int mHandledMag;            // Last mag handled by handle new mag
  int mShutterlessCamera;     // Copy of NoShutter property for current camera
  int mProbeMode;             // last measured mode, 0 for nanoprobe, 1 for microprobe
  int mReturnedProbeMode;     // last mode returned from scope, 0 for nano, 1 for micro
  int mProbeChangeWait;       // milliseconds to wait after seeing probe change
  int mDiffractOrSTEMwait;    // Milliseconds to wait after seeing 0 mag index if STEM
  int mLastSTEMmode;          // Last STEM mode from update loop
  bool mNeedSTEMneutral;      // Flag that STEM neutral hasn't been gotten yet

 private:
  static UINT StageMoveProc(LPVOID pParam);
  static UINT ScreenMoveProc(LPVOID pParam);
  static UINT LongOperationProc(LPVOID pParam);

  CShiftManager *mShiftManager;
  CWinThread * mStageThread;
  CWinThread * mScreenThread;
  CWinThread * mFilmThread;
  CWinThread *mLongOpThreads[MAX_LONG_THREADS];
  LongThreadData mLongOpData[MAX_LONG_THREADS];
  int mLastLongOpTimes[MAX_LONG_OPERATIONS];
  UINT_PTR mUpdateID;         // ID of timer for updates
  float mIncrement;           // Tilt increment
  BOOL m_bCosineTilt;         // Flag for cosine tilt
  StageMoveInfo mMoveInfo;
  BOOL mBeamBlankSet;         // Keeps track of requested blank setting
  double mLastISdelX;         // Last change in image shift
  double mLastISdelY;
  DWORD mLastNormalization;   // Time of last normalization
  float mScreenCurrentFactor; // Correction factor for screen current
  DWORD mLastTiltTime;        // Time of last tilt
  DWORD mLastStageTime;       // Time of last stage move
  double mLastTiltChange;     // Amount tilted by
  float mSmallScreenFactor;   // Amount to change current if small screen in
  CSerialEMApp *mWinApp;
  int mFromPixelMatchMag;     // Mag last changed from in EFTEM pixel matching
  int mToPixelMatchMag;       // Mag last changed to
  int mSTEMfromMatchMag;      // Mag last changed from in STEM pixel matching
  int mSTEMtoMatchMag;
  int mNonGIFfromMatchMag;    // Mag last changed from non nonGIF pixel matching
  int mNonGIFtoMatchMag;      // Mag last changed to
  int mLowestMModeMagInd;     // Index of the lowest mag before low mag mode
  int mLowestGIFModeMag;      // Index to lowest mag that should be used in GIF mode
  int mLowestSTEMnonLMmag[2]; // Index to lowest non LM mag in STEM mode
  int mLowestMicroSTEMmag;    // Index to start of microprobe mags
  int mShiftBoundaries[MAX_MAGS + 10];  // Mags that start a new image or beam shift regime
  int mNumShiftBoundaries;    // Number of boundaries
  float mC2IntensityFactor[2];   // Factor to scale intensity to C2 reading, each probe mode
  float mC2SpotOffset[MAX_SPOT_SIZE + 1][2];    // Offset for each spot size, each probe mode
  int mNumSpotSizes;          // Number of spot size
  int mMinSpotSize;           // Minimum usable spot size
  int mNumAlphas;             // Number of alphas
  double mCrossovers[MAX_SPOT_SIZE + 1][2];        // Intensity at crossover
  float mAlphaBeamShifts[MAX_ALPHAS][2];
  int mNumAlphaBeamShifts;    // Number of alphas with recorded beam shifts
  float mAlphaBeamTilts[MAX_ALPHAS][2];
  int mNumAlphaBeamTilts;     // Number of alphas with recorded beam tilt
  double mSpotBeamShifts[2][MAX_SPOT_SIZE + 1][2];
  int mMinSpotWithBeamShift[2];  // Min and max spot sizes with recorded beam shifts
  int mMaxSpotWithBeamShift[2];  // For primary and secondary mags
  float mStageLimit[4];       // Limits for moving the stage
  float mMaxTiltAngle;        // Limit for tilt angles
  float mTiltAxisOffset;      // Offset from optical to tilt axis
  BOOL mShiftToTiltAxis;      // Flag to maintain center of image shift on tilt axis
  BOOL mApplyISoffset;        // Flag to apply image shift offsets
  int mInitializeJeolDelay;   // Delay after initializing JEOL, in ms
  BOOL mHasOmegaFilter;
  BOOL mScreenByEvent;        // Flag that screen updates occur by event
  BOOL mSpectrumByEvent;      // Flag that spectroscopy mode update is by event
  BOOL mLDNormalizeBeam;      // Flag to go through view on changes between LD areas
  BOOL mLDBeamTiltShifts;     // Flag to record beam tilt shifts and adjust for them
  int mLDBeamNormDelay;       // Sleep time after going to view intensity
  float mLDViewDefocus;       // Defocus offset going to View in low dose
  float mSearchDefocus;       // Defocus offset going to Search in low dose
  LowDoseParams *mLdsaParams; // Parameters of the current set low dose area
  int mJeolForceMDSmode;      // +1 to turn on MDS mode to avoid IS resets, -1 to turn off
  BOOL mCanControlEFTEM;      // Flag that EFTEM can be turned on/off (false on JEOL)
  int mSimulationMode;        // Flag that a simulator is being run; if > 1, it sets KV
  int mJeolPostMagDelay;      // Sleep delay after changing mag
  int mJeolMagEventWait;      // Full wait time for a mag event to show up after change
  int mJeolSTEMPreMagDelay;   // Delay between a JEOL STEM image and STEM mag change
  int mPostMagStageDelay;     // Delay after changing mag for accessing stage only
  int mJeolExternalMagDelay;  // Delay after an external mag change for fixing IS
  BOOL mNoColumnValve;        // Flag for scope (JEOL) with no column/beam valve control
  CString mC2Name;            // Name of C2 lens
  CString mC2Units;           // Units to put out for intensity
  double mManualExposure;     // Time for manual exposure, 0 for auto
  double mStandardLMFocus;     // Focus to use in low mag for montages, etc
  float mWarnIfKVnotAt;         // KV for warning on startup
  double mLMFocusTable[MAX_MAGS][2];  // Table of individual standard focus values
  int mMainDetectorID;        // Detector ID for main screen or equiv. on JEOL
  int mPairedDetectorID;      // Detector that moves complementary to main screen
  BOOL mJeol1230;             // Flag for a 1230
  int mHasNoAlpha;            // Flag that scope has no alpha setting
  int mAdaExists;             // Flag that adaexp has been found
  int mNoScope;               // Flag that there is no scope to connect to: has voltage
  int mFakeMagIndex;          // A settable, returnable mag index if there is no scope
  int mFakeScreenPos;         // And a fake screen position so the right mag can be shown
  int mNextLDpolarity;        // Polarity of image shift for next low dose area setting
  BOOL mUseIllumAreaForC2;    // Use illuminated area for intensity
  double mLastStageCheckTime; // Last time when stage was checked for real on JEOL
  int mSTEMswitchDelay;       // ms timeout for acquisition after switching STEM
  float mSTEMneutralISX;      // Neutral values for IS (CLA1) on JEOL in STEM
  float mSTEMneutralISY;
  int mInsertDetectorDelay;   // Delays after inserting or selecting JEOL detectors
  int mSelectDetectorDelay;
  int mJeolSTEMrotation;      // Value that JEOL should have for STEM rotation
  int mJeolSwitchSTEMsleep;   // Msec to just plain sleep after switching STEM
  int mJeolSwitchTEMsleep;    // Msec to sleep after switching back to TEM, if different
  BOOL mJeolSTEMunitsX;       // Flag that STEM mag units come through as X not ABC
  CArray<ChannelSet, ChannelSet> mBlockedChannels;
  BOOL mUsePLforIS;           // Flag to use PL when initialize scope
  BOOL mUseCLA2forSTEM;       // Flag to use CLA2 for JEOL STEM
  bool mXYbacklashValid;      // Flag that backlash values are still valid for a position
  float mLastBacklashX;       // Last backlash values
  float mLastBacklashY;
  double mPosForBacklashX;    // And position at which backlash was recorded
  double mPosForBacklashY;
  float mBacklashTolerance;   // Maximum change in position for considering position valid
  float mRequestedStageX;     // Position requested in last MoveStage call
  float mRequestedStageY;
  bool mStageAtLastPos;       // Flag that stage is still at last position
  bool mBacklashValidAtStart; // Flag that backlash was valid before stage move
  float mMinMoveForBacklash;  // One-shot threshold for the next move to set a backlash
  double mStartForMinMoveX;   // Starting position for a move to be evaluated
  double mStartForMinMoveY;   // for setting backlash
  BOOL mMovingStage;          // Flag that we started a stage move
  BOOL mBkgdMovingStage;      // Flag that stage move was started on background channel
  float mIllumAreaLowLimit;   // Limits for illuminated area
  float mIllumAreaHighLimit;
  float mIllumAreaLowMapTo;   // Values that lower and upper limits map to
  float mIllumAreaHighMapTo;
  int mNeutralIndex;          // Index to neutral values (0 = nonGIF, 1 = GIF)
  BOOL mSkipNextBeamShift;    // Flag to skip shifting beam on next IS change
  bool mDoingLongOperation;   // Flag that any long operation is active
  BOOL mUsePiezoForLDaxis;    // Flag to use a piezo for low dose axis shift
  bool mFocusCameFromView;    // Flag that focus was reached from View mode
  int mFalconPostMagDelay;    // Timeout interval after mag change for taking Falcon image
  int mLowestSecondaryMag;    // Index for bottom of a secondary mag range
  int mSecondaryMode;         // 1 if in secondary mode; can be flag or index
  int mGaugeIndex[MAX_GAUGE_WATCH];
  int mAlphaChangeDelay;      // Delay after changing alpha
  bool mDisconnected;         // Flag that we have disconnected from scope
  BOOL mAdjustFocusForProbe;  // Flag to adjust focus when probe mode changes
  double mFirstFocusForProbe; // First focus value seen in a probe mode
  double mLastFocusInUpdate;  // Last focus value seen in the update routine
  double mFirstBeamXforProbe; // First beam shift value seen in a probe mode
  double mFirstBeamYforProbe;
  int mPostProbeDelay;        // Msec delay after changing probe mode
  int mHitachiMagFocusDelay;  // Maximum time to wait for focus change after mag change
  int mHitachiMagISDelay;     // Maximum time to wait for IS change after mag change
  int mMagChgIntensityDelay;  // Delay from changing mag to changing intensity
  int mHitachiMagChgSeeTime;  // max time for internal mag change to show up in update
  bool mISwasNewLastUpdate;   // Flag that new IS was seen on last update
  double mLastMagModeChgTime; // Time of last LM/M whatever boundary crossing
  int mHitachiModeChgDelay;   // Amount of time to wait before managing IS
  int mHitachiSpotStepDelay;  // Msec between stepping up in spot sizes
  int mHitachiSpotBeamWait;
  int mHighestLMindexToScan;  // Last index of enabled LM mags, if any are disabled
  int mCheckFreeK2RefCount;   // Counter for checking K2 reference freeing
  int mLastNormMagIndex;      // Mag of last normalization
  bool mGettingValuesFast;    // Keep track of whether getting values fast
  bool mLastGettingFast;
  int mPostFocusChgDelay;     // Delay after changing focus
  int mUseJeolGIFmodeCalls;   // 1 to rely on state from calls, 2 to change EFTEM with it
  BOOL mJeolHasNitrogenClass; // Flag to create the nitrogen class
  int mJeolRefillTimeout;     // Timeout for refilling
  int mJeolFlashFegTimeout;   // Timeout for flashing FEG
  int mJeolEmissionTimeout;   // Timeout for turning emission off or on
  int mPostJeolGIFdelay;      // Delay time after setting GIF mode
  BOOL mUseInvertedMagRange;  // Flag to step up in Titan inverted range to find mag
  int mUpdateBeamBlank;       // Flag to update beam blanker
  BOOL mBlankTransients;      // Flag to blank for mag/spot size changes & normalizations
  double mLastCameraLength;   // Camera length in last update: valid only in diff mode
  int mNormAllOnMagChange;    // Norm all lenses when in LM if 1, or always if 2
  float mStageRelaxation;     // Default distance to relax stage from backlash
  int mDoingStoppableRefill;  // Sum of bits for refill types
  float mFalcon3ReadoutInterval; // Frame interval for Falcon 3 camera
  float mAddToFalcon3Exposure; // Default to set addToExposure for Falcon 3
  BOOL mSkipAdvancedScripting; // To make cameras connect by old scripting
  std::vector<short int> mCheckedNeutralIS;
  int mPluginVersion;         // Version of plugin or server

  // Old static variables from UpdateProc
  int mErrCount;
  DWORD mLastReport;
  BOOL mReportingErr;
  BOOL mLastLowDose;
  int mLastScreen;
  int mAutosaveCount;
  BOOL mLastSpectroscopy;
  double mLastPressure;
  int mLastGaugeStatus, mVacCount;

public:
  void SetManualExposure(double time);
  BOOL TakeFilmExposure(BOOL useAda, double loadDelay, double preExpose, BOOL screenOn);
  static int TaskFilmBusy(void);
  int FilmBusy(void);
  void FilmCleanup(int error);
  static UINT FilmExposeProc(LPVOID pParam);
  double GetFocus(void);
  BOOL SetFocus(double inVal);
  BOOL WaitForStageReady(int msecs);
  BOOL GetUsePLforIS(void);
  BOOL GetUsePLforIS(int magInd);
  BOOL SetAlpha(int alpha);
  BOOL SetJeolGIF(int alpha);
  void SetLDContinuousUpdate(BOOL state);
  int GetAlpha(void);
  int GetJeolGIF(void);
  double GetDiffractionFocus(void);
  BOOL SetDiffractionFocus(double inVal);
  BOOL AssessMagISchange(int fromInd, int toInd, BOOL STEMmode, double &newISX,
    double &newISY);
  BOOL SaveOrRestoreIS(int saveMag, int otherMag);
  BOOL AssessRestoreIS(int fromInd, int toInd, double &newISX, double &newISY);
  void SaveISifNeeded(int fromInd, int toInd);
  int LookupRingIS(int lastMag, BOOL changedEFTEM);
  BOOL SetObjFocus(int step);
  int LookupScriptingCamera(CameraParameters * params, bool refresh,
    int restoreShutter = VALUE_NOT_SET);
  double FastDefocus(void);
  BOOL FastStagePosition(double & X, double & Y, double & Z);
  int FastScreenPos(void);
  double CloseValvesAfterInterval(double interval);
  void FindMatchingMag(int fromCam, int toCam, float toFactor, int oldMag, int curMag,
    int &newMag, float &fromPixel, float &toPixel, int &fromMatchMag, int &toMatchMag,
    BOOL doMatchPixel);
  void MatchPixelAndIntensity(int fromCam, int toCam, float toFactor, BOOL matchPixel,
    BOOL matchIntensity);
  int GetFEIChannelList(CameraParameters * params, bool release);
  BOOL GetSTEMmode(void);
  int LookupSTEMmagFEI(double curMag, int curMode, double & minDiff);
  bool SetSTEM(BOOL inState);
  BOOL FastSTEMmode(void);
  int GetLowestNonLMmag(int index = -2);
  int GetLowestNonLMmag(CameraParameters *camParam);
  void ResetSTEMneutral(void);
  bool TestSTEMshift(int type, int delx, int dely);
  void AddImageDetector(int id);
  bool SelectJeolDetectors(int * detInd, int numDet);
  BOOL NeedBeamBlanking(int screenPos, BOOL STEMmode, BOOL &goToLDarea);
  BOOL NeedBeamBlanking(int screenPos, BOOL STEMmode);
  void ScopeUpdate(DWORD dwTime);
  void UpdateStage(double &stageX, double &stageY, double &stageZ, BOOL &bReady);
  void UpdateLastMagEftemStem(int magIndex, double defocus, int screenPos, BOOL EFTEM,
    int STEMmode);
  void updateEFTEMSpectroscopy(BOOL &EFTEM);
  void UpdateLowDose(int screenPos, BOOL needBlank, BOOL gotoArea, int magIndex,
    int camLenIndex, double diffFocus, float alpha, int &spotSize,
    double &rawIntensity, double &ISX, double &ISY);
  void UpdateGauges(int &vacStatus);
  void UpdateScreenBeamFocus(int STEMmode, int &screenPos, int &smallScreen, int &spotSize,
                                     double &rawIntensity, double &current,
                                     double &defocus, double &objective, float &alpha);
  BOOL CassetteSlotStatus(int slot, int &status);
  int LoadCartridge(int slot);
  int UnloadCartridge(void);
  static ScopePluginFuncs *GetPlugFuncs() {return mPlugFuncs;};
  void SetValidXYbacklash(StageMoveInfo * info);
  bool GetValidXYbacklash(double stageX, double stageY, float & backX, float & backY);
  bool StageIsAtSamePos(double stageX, double stageY, float requestedX, float requestedY);
  double IllumAreaToIntensity(double illum);
  double IntensityToIllumArea(double intensity);
  double IntensityAfterApertureChange(double intensity, int oldAper, int newAper);
  bool AreDewarsFilling(int & busy);
  bool GetDewarsRemainingTime(int & time);
  bool GetRefrigerantLevel(int which, double & level);
  bool GetObjectiveStigmator(double & stigX, double & stigY);
  bool SetObjectiveStigmator(double stigX, double stigY);
  bool GetTemperatureInfo(int type, BOOL & busy, int & time, int which, double & level);
  BOOL IsPVPRunning(BOOL & state);
  int StartLongOperation(int *operations, float *hoursSinceLast, int numOps);
  int LongOperationBusy(int index = -1);
  int StopLongOperation(bool exiting, int index = -1);
  void ChangeAlphaAndBeam(int oldAlpha, int newAlpha);
  int FastAlpha(void);
  BOOL GetColumnMode(int &mode, int &subMode);
  BOOL GetLensByName(CString &name, double &value);
  BOOL GetDeflectorByName(CString &name, double &valueX, double &valueY);
  bool MagChgResetsIS(int toInd);
  BOOL NormalizeAll(int illumProj);
  int ReadProbeMode(void);
  void SetCheckPosOnScreenError(BOOL inVal);
  BOOL GetCheckPosOnScreenError();
  void GetValuesFast(int enable);
  void RemoteControlChanged(BOOL newState);
  void SetBacklashFromNextMove(double startX, double startY, float minMove) {
    mStartForMinMoveX = startX; mStartForMinMoveY = startY; mMinMoveForBacklash = minMove;};
  bool BlankTransientIfNeeded(const char *routine);
  void UnblankAfterTransient(bool needUnblank, const char *routine);
  BOOL SetFreeLensControl(int lens, int arg);
  BOOL SetLensWithFLC(int lens, double inVal, bool relative);
  BOOL GetLensFLCStatus(int lens, int &state, double &lensVal);
};


#endif // !defined(AFX_EMSCOPE_H__3021271B_90D6_48ED_B7BA_5C7F62808186__INCLUDED_)
