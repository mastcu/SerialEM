// SerialEM.h : main header file for the SERIALEM application
//

#if !defined(AFX_SERIALEM_H__F084D61C_0A12_4CD1_AC1F_1AC785054FCE__INCLUDED_)
#define AFX_SERIALEM_H__F084D61C_0A12_4CD1_AC1F_1AC785054FCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#define MAX_CAMERAS  6
#define MAX_DLG_CAMERAS  6
#define MAX_MAGS 120      // F20 STEM requires 95, TItan 100, Titan Halo STEM 112
#define NUM_TSS_PANELS 8
#define HITACHI_LENS_MAX 0x3FFC00
#define MAX_CONSETS   9
#define NUMBER_OF_USER_CONSETS  7
#define VIEW_CONSET   0
#define FOCUS_CONSET   1
#define TRIAL_CONSET   2
#define RECORD_CONSET  3
#define PREVIEW_CONSET 4
#define SEARCH_CONSET  5
#define MONT_USER_CONSET 6
#define MONTAGE_CONSET 8
#define ISCAL_CONSET   7
#define TRACK_CONSET   7
#define MAX_LENS_TO_RELAX 6

//#define TASK_TIMER_INTERVAL 50

// Macros for the vast numbers of sets and gets in headers
#define SetMember(a,b) void Set##b(a inVal) {m##b = inVal;};
#define GetMember(a,b) a Get##b() {return m##b;};
#define GetSetMember(a,b) GetMember(a,b)\
  SetMember(a,b)

#include <math.h>
#include <afxtempl.h>
#include <set>
#include <string>
#include "resource.h"       // main symbols
#include "EMimageBuffer.h"  // Added by ClassView
#include "ImageLevelDlg.h"  // Added by ClassView
#include "EMbufferWindow.h" // Added by ClassView
#include "TiltWindow.h" // Added by ClassView
#include "EMstatusWindow.h" // Added by ClassView
#include "AlignFocusWindow.h"   // Added by ClassView
#include "MontageWindow.h"
#include "MontageParam.h"
#include "MagTable.h"
#include "ControlSet.h"
#include "MacroControl.h"
#include "Image\KStoreMRC.h"
#include <comdef.h>
#include "CameraMacroTools.h"
#include "ScopeStatusDlg.h"
#include "LowDoseDlg.h"
#include "FilterControlDlg.h"
#include "MenuTargets.h"
#include "STEMcontrolDlg.h"
#include "RemoteControl.h"
#include "./DirectElectron/DirectElectronToolDlg.h"
#include "Utilities\SEMUtilities.h"

typedef std::set<std::string> StringSet;

enum Tasks {TASK_NAVIGATOR_ACQUIRE, TASK_DISTORTION_STAGEPAIR, TASK_CAL_BEAMSHIFT,
  TASK_REVERSE_TILT, TASK_WALKUP, TASK_EUCENTRICITY, TASK_RESET_REALIGN, 
  TASK_RESET_SHIFT, TASK_CAL_IMAGESHIFT, TASK_MONTAGE, TASK_TILT_SERIES,
  TASK_CAL_CAM_TIMING, TASK_CAL_MAGSHIFT, TASK_REFINE_ZLP, TASK_CCD_CAL_INTENSITY,
  TASK_CAL_IS_NEUTRAL, TASK_CAL_SPOT_INTENSITY, TASK_CAL_DEAD_TIME, TASK_TILT_AFTER_MOVE,
  TASK_FILM_EXPOSURE, TASK_ACQUIRE_RESETUP, TASK_NAV_REALIGN, TASK_DEL_OTHER_STORE,
  TASK_LOAD_MAP, TASK_MACRO_RUN, TASK_MONTAGE_RESTORE, TASK_MONTAGE_FOCUS, TASK_COOKER,
  TASK_TEST_AUTOCEN, TASK_AUTOCEN_BEAM, TASK_TILT_RANGE, TASK_DUAL_MAP, 
  TASK_MONTAGE_REALIGN, TASK_STEM_FOCUS, TASK_FOCUS_VS_Z, TASK_INTERSET_SHIFT,
  TASK_BACKLASH_ADJUST, TASK_ASYNC_SAVE, TASK_LONG_OPERATION, TASK_BIDIR_COPY, 
  TASK_BIDIR_ANCHOR, TASK_STAGE_TOOL, TASK_STACK_FALCON, TASK_MONTAGE_DWELL, 
  TASK_CAL_ASTIG, TASK_FIX_ASTIG, TASK_COMA_FREE, TASK_ZEMLIN, TASK_MULTI_SHOT,
  TASK_GAIN_REF, TASK_ALIGN_DE_FRAMES, TASK_START_NAV_ACQ, TASK_CTF_BASED, 
  TASK_CAL_COMA_VS_IS, TASK_REMOTE_CTRL, TASK_MOVE_APERTURE, TASK_WAIT_FOR_DRIFT,
  TASK_SET_CAMERA_NUM, TASK_CONDITION_VPP, TASK_FIND_HOLES, TASK_REFINE_BS_CAL,
  TASK_CAL_IA_LIMITS, TASK_MACRO_AT_EXIT, TASK_Z_BY_G, TASK_TEMPLATE_ALIGN,
  TASK_DEWARS_VACUUM, TASK_NAV_ACQ_RETRACT
};

enum CalTypes {CAL_DONE_IS = 0, CAL_DONE_STAGE, CAL_DONE_FOCUS, CAL_DONE_BEAM, 
  CAL_DONE_SPOT, NUM_CAL_DONE_TYPES, CAL_DONE_CLEAR_ALL}; 

#define axisX 1
#define axisY 2
#define axisXY 3
#define axisZ 4
#define axisA 8
#define axisB 16
enum {spUnknown = 1, spUp, spDown};
enum JeolScreenPosition 
  {
    spUnknownJeol = 2,
    spUpJeol = 0,
    spDownJeol = 1
  };
struct JeolStateData;
struct JeolParams;

#define MAX_BUFFERS  20
#define MAX_ROLL_BUFFERS 14
#define MAX_FFT_BUFFERS 8
#define MAX_STORES    8
#define MAX_EXTRA_SAVES 5
#define MAX_EXTRA_RECORDS 30
#define MAX_TS_VARIES  50
#define MAX_VARY_TYPES  6
#define MAX_TOOL_DLGS  16
#define MAX_LOWDOSE_SETS 5
#define LAD_INDEX_BASE  30
#define IDLE_TIMEOUT_ERROR  -99
#define MAX_MACROS 60
#define MAX_ONE_LINE_SCRIPTS 5
#define MAX_TEMP_MACROS 5
#define MAX_TOT_MACROS (MAX_MACROS + MAX_ONE_LINE_SCRIPTS + MAX_TEMP_MACROS)
#define SCOPE_PANEL_INDEX    3
#define REMOTE_PANEL_INDEX   4
#define LOW_DOSE_PANEL_INDEX 8
#define MONTAGE_DIALOG_INDEX 9
#define NO_SUPPLIED_SECTION -99999
#define VALUE_NOT_SET   -999
#define LOG_OPEN_IF_CLOSED                0
#define LOG_SWALLOW_IF_CLOSED             1
#define LOG_MESSAGE_IF_CLOSED             2
#define MESSAGE_ONLY                      3
#define LOG_IF_ADMIN_MESSAGE_IF_NOT       4
#define LOG_MESSAGE_IF_NOT_ADMIN_AND_OPEN 5
#define LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN  6
#define LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN  7
#define MESSAGE_ONLY_IF_ADMIN             8
#define LOG_IF_ADMIN_MESSAGE_IF_CLOSED    9
#define LOG_IF_ADMIN_OPEN_IF_CLOSED      10
#define LOG_IF_DEBUG_OPEN_IF_CLOSED      11
#define LOG_IGNORE                       12

#define SIMPLE_PANE 3
#define MEDIUM_PANE 2
#define COMPLEX_PANE 1
#define BLINK_TIME_ON 500
#define BLINK_TIME_OFF 200
#define JEOL_FAKE_HRESULT  0xabcdef12
#define PLUGIN_FAKE_HRESULT  0xabcdef14
#define SOCKET_FAKE_HRESULT  0xabcdef16
#define NOFUNC_FAKE_HRESULT  0xabcdef18

#define BUFFER_PROCESSED          -1
#define BUFFER_MONTAGE_OVERVIEW   -2
#define BUFFER_MONTAGE_CENTER     -3
#define BUFFER_CALIBRATION        -4
#define BUFFER_TRACKING           -5
#define BUFFER_MONTAGE_PRESCAN    -6
#define BUFFER_ANCHOR             -7
#define BUFFER_PRESCAN_OVERVIEW   -8
#define BUFFER_PRESCAN_CENTER     -9
#define BUFFER_STACK_IMAGE       -10
#define BUFFER_UNUSED            -11
#define BUFFER_FFT               -12
#define BUFFER_LIVE_FFT          -13
#define BUFFER_MONTAGE_PIECE     -14
#define BUFFER_AUTOCOR_OVERVIEW  -15
#define BUFFER_CROPPED           -16

#define DTOR 0.01745329252

// Flags and definitions for plugins
#define PLUGFLAG_CAMERA            1
#define PLUGFLAG_SCOPE             (1 << 1)
#define PLUGFLAG_INFOEX            (1 << 2)
#define PLUGFLAG_TSCALLS           (1 << 3)
#define PLUGFLAG_PIEZO             (1 << 4)
#define PLUGFLAG_DECAM             (1 << 5)
#define PLUGFLAG_RETURNS_FLOATS    (1 << 6)
#define PLUGFLAG_FLOATS_BY_FLAG    (1 << 7)
#define PLUGFLAG_CAN_DIV_MORE      (1 << 8)
#define PLUGFLAG_SCRIPT_LANG       (1 << 9)
#define PLUGFLAG_NO_DIV_BY_2       (1 << 10)
#define PLUGCALL_TSACTION          1
#define PLUGCAM_DIVIDE_BY2         1
#define PLUGCAM_CONTINUOUS         2
// See SEMCCDDefines.h for bits 17-21
// These are copied to FeiScope.h
#define PLUGFEI_MAKE_CAMERA        2
#define PLUGFEI_MAKE_STAGE         4
#define PLUGFEI_MAKE_ILLUM         8
#define PLUGFEI_MAKE_PROJECT      0x10
#define PLUGFEI_MAKE_VACUUM       0x20
#define PLUGFEI_MAKE_ALL          0x3E
#define PLUGFEI_MAKE_ADVANCED     0x40
#define PLUGFEI_MAKE_NOBASIC      0x80
#define PLUGFEI_USES_ADVANCED     0x100
#define PLUGFEI_CAN_DOSE_FRAC     0x200
#define PLUGFEI_CAM_CAN_COUNT     0x400
#define PLUGFEI_CAM_CAN_ALIGN     0x800
#define PLUGFEI_CAM_CONTIN_SAVE   0x1000
#define PLUGFEI_NO_RECORD_LIMIT   1000000000
#define PLUGFEI_INDEX_MASK        0xFF
#define PLUGFEI_MAX_FRAC_SHIFT    16
#define PLUGFEI_WAIT_FOR_FRAMES   1
#define PLUGFEI_APPLY_PIX2COUNT   2
#define PLUGFEI_UNBIN_PIX2COUNT   4
#define PLUGFEI_CALL_EER_MODE     8
#define PLUGFEI_USE_EER_MODE      0x10   

typedef _variant_t PLUGIN_BOOL;
typedef void (*PlugStopFunc)(int);
typedef bool (*PlugDoingFunc)(void);


// Definitions for frame-saving filename format
#define FRAME_FOLDER_ROOT            (1)
#define FRAME_FOLDER_SAVEFILE    (1 << 1)
#define FRAME_FOLDER_NAVLABEL    (1 << 2)
#define FRAME_FILE_ROOT          (1 << 3)
#define FRAME_FILE_SAVEFILE      (1 << 4)
#define FRAME_FILE_NAVLABEL      (1 << 5)
#define FRAME_FILE_NUMBER        (1 << 6)
#define FRAME_FILE_MONTHDAY      (1 << 7)
#define FRAME_FILE_HOUR_MIN_SEC  (1 << 8)
#define FRAME_LABEL_IF_ACQUIRE   (1 << 9)
#define FRAME_FILE_TILT_ANGLE    (1 << 10)
#define FRAME_FILE_DATE_PREFIX   (1 << 11)
#define FRAME_FILE_HOLE_AND_POS  (1 << 12)

// Definitions for Tietz errors and flags.  Keep flags compatible with PLUGCAM flags
// defined in SEMCCDDefines.h
#define TIETZ_NO_LOCK      1
#define TIETZ_NO_CAM_INIT  2
#define TIETZ_NO_MAP_NAME  3
#define TIETZ_NO_SOCKET        (1)
#define TIETZ_NO_CAMC4        (1 << 1)
#define TIETZ_NO_SHUTTERBOX   (1 << 2)
#define TIETZ_USE_SHUTTERBOX (1)
#define TIETZ_IS_GPU_CAMERA  (1 << 2)
#define TIETZ_GET_DARK_REF    4
#define TIETZ_RESTORE_BBMODE (1 << 1)
#define TIETZ_SET_READ_MODE  (1 << 2)
#define TIETZ_CROP_SUBAREA    (1 << 3)
#define TIETZ_SET_BURST_MODE  (1 << 4)
#define TIETZ_DROP_FRAME_BITS   2
#define TIETZ_DROP_FRAME_MASK   3

// A macro for getting a new array and ending up with null if it fails
#define NewArray(arr,typ,siz) try { \
  arr = new typ[siz]; \
} \
  catch (CMemoryException *e) { \
  e->Delete(); \
  arr = NULL; \
  }

#define NewArray2(arr,typ,siz1, siz2) try { \
  arr = new typ[(size_t)(siz1) * (size_t)(siz2)]; \
} \
  catch (CMemoryException *e) { \
  e->Delete(); \
  arr = NULL; \
  }

#define CLEAR_RESIZE(a,b,c) { a.clear(); \
  a.swap(std::vector<b>(a)); \
  a.resize(c); }

#define IS_SET_VIEW_OR_SEARCH(a) (a == VIEW_CONSET || a == SEARCH_CONSET)
#define IS_AREA_VIEW_OR_SEARCH(a) (a == VIEW_CONSET || a == SEARCH_AREA)

#define B3DMIN(a,b) ((a) < (b) ? (a) : (b))
#define B3DMAX(a,b) ((a) > (b) ? (a) : (b))
#define B3DCLAMP(a,b,c) a = B3DMAX((b), B3DMIN((c), (a)))
#define B3DNINT(a) (int)floor((a) + 0.5)
#define B3DCHOICE(a,b,c) ((a) ? (b) : (c))
#define B3DABS(a) ((a) >= 0 ? (a) : -(a))
#define B3DSWAP(a,b,c) {c = (a); a = (b); b = c;}
#define ACCUM_MAX(maxv, val) maxv = ((maxv) > (val) ? (maxv) : (val))
#define ACCUM_MIN(minv, val) minv = ((minv) < (val) ? (minv) : (val))
#define B3DDELETE(a) {delete a ; a = NULL;}
#define BOOL_EQUIV(a, b) (((a) ? 1 : 0) == ((b) ? 1 : 0))
#define MB_EXCLAME (MB_OK | MB_ICONEXCLAMATION)
#define MB_QUESTION (MB_YESNO | MB_ICONQUESTION)

// Forward definitions of classes so pointers can be defined
class CSerialEMDoc;
class CSerialEMView;
class EMbufferManager;
class CEMscope;
class CCameraController;
class CParameterIO;
class CShiftManager;
class CShiftCalibrator;
class EMmontageController;
class CLogWindow;
class CFocusManager;
class CMacCmd;
class CMacroEditer;
class CMacroToolbar;
class CComplexTasks;
class CBeamAssessor;
class CProcessImage;
class CTSController;
class CFilterTasks;
class CDistortionTasks;
class CGainRefMaker;
class CNavigatorDlg;
class CNavHelper;
class CCalibCameraTiming;
class CCookerSetupDlg;
class CAutocenSetupDlg;
class CVPPConditionSetup;
class CMultiTSTasks;
class CParticleTasks;
class CMailer;
class CPluginManager;
class CGatanSocket;
class CFalconHelper;
class CPiezoAndPPControl;
class CStageMoveTool;
class CAutoTuning;
class CExternalTools;
class CScreenShotDialog;
class CMainFrame;
class CPythonServer;

/////////////////////////////////////////////////////////////////////////////
// CSerialEMApp:
// See SerialEM.cpp for the implementation of this class
//

struct DialogTable {
  CToolDlg *pDialog;
  int  state;     // 0 for closed, 1 for full open, -1 for midway open
  int  width;     // width of dialog when created
  int  fullHeight;  // full height when created
  int  midHeight;   // height when midway open
};

struct IdleCallBack {
  int (__cdecl *busyFunc)(void);    // Function to call to find out if still busy
  void (__cdecl *nextFunc)(int);    // Function to call when done successfully
  void (__cdecl *errorFunc)(int);   // Function to call if error
  int source;                       // Code for the source of the task
  int param;                        // One parameter to pass
  DWORD timeOut;                    // if not = 0, timeout in millisec
  BOOL extendTimeOut;               // Flag to extend timeouts after long intervals
};

struct LensRelaxData {
  short normIndex;                     // One of nm... defines
  short numLens;                       // Number of lenses to do together
  short numSteps;                      // Number of steps in full direction
  short delay;                           // msec delay after all lenses changed
  short lensTypes[MAX_LENS_TO_RELAX];  // Type numbers in the FLC calls
  float amplitudes[MAX_LENS_TO_RELAX]; // Change in the full direction
};

// GLOBAL error reporting and other functions; some for plugin access defined in EMscope
void DLL_IM_EX SEMReportCOMError(_com_error E, CString inString, CString *outStr = NULL,
  bool skipErr = false);
BOOL DLL_IM_EX SEMTestHResult(HRESULT hr, CString inString, CString *outStr = NULL, 
                    int *errFlag = NULL, bool skipErr = false);
int DLL_IM_EX SEMStageCameraBusy();
void DLL_IM_EX SEMTrace(char key, char *fmt, ...);
void VarArgToCString(CString &str, char *fmt, va_list args);
void DLL_IM_EX PrintfToLog(char *fmt, ...);
BOOL DLL_IM_EX GetDebugOutput(char key);
void DLL_IM_EX SEMErrorOccurred(int error);
void SEMBuildTime(char *dateStr, char *timeStr);
double DLL_IM_EX SEMTickInterval(double now, double then);
double DLL_IM_EX SEMTickInterval(UINT now, UINT then);
double DLL_IM_EX SEMTickInterval(double then);
int DLL_IM_EX SEMMessageBox(CString message, UINT type = MB_OK | MB_ICONEXCLAMATION, 
    BOOL terminate = true, int retval = 0);
int DLL_IM_EX SEMThreeChoiceBox(CString message, CString yesText, CString noText, 
  CString cancelText, UINT type = MB_YESNO | MB_ICONQUESTION, int setDefault = 0, 
    BOOL terminate = true, int retval = 0, bool noLineWrap = false);
int DLL_IM_EX SEMInitializeWinsock(void);
double DLL_IM_EX SEMWallTime();
int DLL_IM_EX SEMUseTEMScripting();
int DLL_IM_EX SEMNumFEIChannels();
HitachiParams DLL_IM_EX *SEMGetHitachiParams();
int DLL_IM_EX *SEMGetLastHitachiMember();
int DLL_IM_EX SEMSetSecondaryModeForMag(int index);
BOOL DLL_IM_EX SEMAcquireScopeMutex(char *name, BOOL retry);
BOOL DLL_IM_EX SEMReleaseScopeMutex(char *name); 
void DLL_IM_EX SEMAcquireJeolDataMutex();
void DLL_IM_EX SEMReleaseJeolDataMutex();
BOOL DLL_IM_EX SEMScopeHasFilter();
MagTable DLL_IM_EX *SEMGetMagTable();
int DLL_IM_EX *SEMGetCamLenTable();
void DLL_IM_EX SEMClippingJeolIS(long &jShiftX, long &jShiftY);
BOOL DLL_IM_EX SEMCheckStageTimeout();
void DLL_IM_EX SEMSetStageTimeout();
void DLL_IM_EX SEMGetJeolStructures(JeolStateData **jsd, int **lastState, 
  JeolParams **params, int **lastParam);
void DLL_IM_EX SEMSetJeolStateMag(int magIndex, BOOL needMutex);
void SetNumFEIChannels(int inval);
double DLL_IM_EX SEMSecondsSinceStart();
void DLL_IM_EX SEMSetFunctionCalled(const char *name, const char *descrip = NULL);
void DLL_IM_EX SEMIgnoreFunctionCalled(bool ignore);
LensRelaxData DLL_IM_EX *SEMLookupJeolRelaxData(int normInd);
double DLL_IM_EX SEMRecentVoltage();
CString DLL_IM_EX SEMLastNoBoxMessage();
void DLL_IM_EX *SEMGetBufferImage(int bufInd, int ifFFT, int &imType, int &rowBytes, int &sizeX,
  int &sizeY);
bool DLL_IM_EX SEMIsBufferImageValid(void *array, int imType, int rowBytes, int sizeX, int sizeY,
  int &bufInd, int &ifFFT);
  
class DLL_IM_EX CSerialEMApp : public CWinApp
{
public:
  void WarnIfUsingOldFilterAlign();
  SetMember(CString, StartupMessage);
  CString GetStartupMessage(bool original = false);
  CString GetVersionString();
  SetMember(BOOL, StartupInfo);
  SetMember(BOOL, ExitOnScopeError);
  void SetAdministratorMode(int inVal);
  int GetAdministratorMode() { return mAdministratorMode; };
  GetMember(BOOL, Administrator);
  void SetDebugOutput(CString keys);
  BOOL ScopeHasFilter() { return mScopeHasFilter; };
  BOOL ScopeHasSTEM() { return mScopeHasSTEM; };
  void NavigatorClosing();
  BOOL CalibrationsNotSaved() { return mCalNotSaved; };
  void SetCalibrationsNotSaved(BOOL inVal) { mCalNotSaved = inVal; };
  GetMember(BOOL, Any16BitCameras);
  void CopyCameraToCurrentLDP();
  LowDoseParams *GetLDParamsForCamera(int camNUm);
  void CopyCurrentToCameraLDP();
  BOOL CheckIdleTasks();
  FilterParams *GetFilterParams() { return &mFilterParams; };
  GetMember(BOOL, EFTEMMode)
    BOOL GetFilterMode();
  void SetEFTEMMode(BOOL inState);
  GetMember(BOOL, STEMMode);
  GetSetMember(int, ActiveCamListSize)
    GetMember(int, NumActiveCameras)
    GetMember(int, NumReadInCameras)
    int *GetActiveCameraList() { return &mActiveCameraList[0]; };
  int *GetOriginalActiveList() { return &mOriginalActiveList[0]; };
  SetMember(int, InitialCurrentCamera)
    void TransferConSet(int inSet, int fromCam, int toCam);
  void CopyConSets(int inCam);
  void SetActiveCameraNumber(int inNum);
  void SetNumberOfActiveCameras(int inNum, int readIn);
  GetSetMember(BOOL, RetractOnEFTEM)
    afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM lParam);
  void SetTitleFile(CString fileName);
  BOOL LowDoseMode();
  void UpdateWindowSettings();
  BOOL StartedTiltSeries();
  BOOL DoingTiltSeries();
  GetSetMember(BOOL, ActPostExposure)
    BOOL ActPostExposure(ControlSet *conSet = NULL, bool alignHereOK = false);
  BOOL DoingComplexTasks();
  GetSetMember(BOOL, SmallFontsBad)
    GetSetMember(BOOL, DisplayNotTruly120DPI)
    void SetStatusText(int iPane, CString strText, bool skipBlink = false);
  void ManageBlinkingPane(DWORD time);
  BOOL SetWindowPlacement(WINDOWPLACEMENT *winPlace);
  BOOL GetWindowPlacement(WINDOWPLACEMENT *winPlace);
  void ErrorOccurred(int error);
  void AppendToLog(CString inString, int inAction = LOG_OPEN_IF_CLOSED, int lineFlags = 0);
  void LogClosing();
  WINDOWPLACEMENT *GetLogPlacement() { return &mLogPlacement; };
  WINDOWPLACEMENT *GetNavPlacement() { return &mNavPlacement; };
  WINDOWPLACEMENT *GetStageToolPlacement();
  void OpenStageMoveTool();
  void OpenScreenShotDlg();
  WINDOWPLACEMENT *GetScreenShotPlacement();
  SetMember(bool, ReopenLog);
  void SetReopenMacroEditor(int index, bool open) { mReopenMacroEditor[index] = open; };
  void StartMontageOrTrial(BOOL inTrial);
  void SetMontaging(BOOL inVal);
  void UserRequestedCapture(int whichMode);
  void AddIdleTask(int(__cdecl *busyFunc)(void), void(__cdecl *nextFunc)(int),
    void(__cdecl *errorFunc)(int), int source, int param, int timeOut);
  void AddIdleTask(int(__cdecl *busyFunc)(void), void(__cdecl *nextFunc)(int),
    void(__cdecl *errorFunc)(int), int param, int timeOut);
  void AddIdleTask(int(__cdecl *busyFunc)(void), int source, int param, int timeOut);
  void AddIdleTask(int source, int param, int timeOut);
  void UpdateBufferWindows();
  void OnCameraParameters();
  void OnResizeMain();
  void DoResizeMain(int whichBuf = 0);
  GetSetMember(int, SelectedConSet)
    GetMember(int, CurrentCamera)
    GetMember(int, CurrentActiveCamera)
    GetMember(CameraParameters *, CamParams)
    GetSetMember(float, MemoryLimit)
    CameraParameters *GetActiveCamParam(int index = -1);
  void UpdateStatusWindow();
  void DialogChangedState(CToolDlg *inDialog, int inState);
  void RestoreViewFocus();
  int GetNewViewProperties(CSerialEMView *inView, int &iBordLeft, int &iBordTop, int &iBordRight,
    int &iBordBottom, EMimageBuffer *&imBufs, int &iImBufNumber, int &iImBufIndex);
  void SetImBufIndex(int inIndex, bool fftBuf = false);
  int GetImBufIndex(bool fftBuf = false) { return fftBuf ? mFFTbufIndex : mImBufIndex; };
  void SetCurrentBuffer(int index, bool fftBuf = false);
  CSerialEMApp();
  ~CSerialEMApp();
  void SetActiveView(CSerialEMView *inActiveView);
  void SetDisplayTruncation(float inPctLo, float inPctHi) { mPctLo = inPctLo; mPctHi = inPctHi; };
  void GetDisplayTruncation(float &outPctLo, float &outPctHi) { outPctLo = mPctLo; outPctHi = mPctHi; };
  void SetPctAreaFraction(float inAreaFrac) { mAreaFrac = inAreaFrac; };
  float GetPctAreaFraction() { return mAreaFrac; };
  BOOL Montaging() { return mMontaging; };
  BOOL SavingOther() { return mSavingOther; };
  SetMember(bool, SavingOther)
    BOOL DoingTasks();
  BOOL DoingImagingTasks();
  GetMember(LowDoseParams *, LowDoseParams)
    LowDoseParams *GetCamLowDoseParams() { return &mCamLowDoseParams[0][0]; };
  MontParam *GetMontParam() { return &mMontParam; };
  GetMember(EMimageBuffer *, ImBufs);
  GetMember(EMimageBuffer *, FFTBufs);
  GetMember(ControlSet *, ConSets);
  ControlSet *GetCamConSets() { return &mCamConSets[0][0]; };
  CString *GetModeNames() { return mModeName; };
  GetMember(CString *, Macros)
    CString *GetMacroSaveFiles() { return mMacroSaveFile; };
  CString *GetMacroFileNames() { return mMacroFileName; };
  CString *GetMacroNames() { return mMacroName; };
  CString *GetLongMacroNames() { return mLongMacroName; };
  MagTable *GetMagTable() { return mMagTab; };
  int *GetCamLenTable() { return &mCamLengths[0]; };
  float *GetCamLenCalibrated() { return &mCamLenCalibrated[0]; };
  MacroControl *GetMacControl() { return &mMacControl; };
  int *GetInitialDlgState();
  GetMember(DialogTable *, DialogTable)
    GetMember(int, NumToolDlg)
    GetMember(RECT *, DlgPlacements)
    GetSetMember(BOOL, ExitWithUnsavedLog)
    GetSetMember(BOOL, TestGainFactors)
    GetSetMember(BOOL, SkipGainRefWarning)
    SetMember(BOOL, ProcessHere)
    SetMember(BOOL, ReopenMacroToolbar)
    NavParams *GetNavParams() { return &mNavParams; };
  NavAcqParams *GetNavAcqParams(int which) { return &mNavAcqParams[which]; };
  CookParams *GetCookParams() { return &mCookParams; };
  SetMember(BOOL, DeferBufWinUpdates)
    GetSetMember(BOOL, ContinuousSaveLog)
    GetSetMember(BOOL, OpenStateWithNav)
    GetSetMember(BOOL, NonGIFMatchIntensity)
    GetSetMember(BOOL, NonGIFMatchPixel)
    GetSetMember(int, FirstSTEMcamera)
    GetSetMember(BOOL, STEMmatchPixel)
    GetSetMember(BOOL, ScreenSwitchSTEM)
    GetSetMember(BOOL, InvertSTEMimages)
    GetSetMember(float, AddedSTEMrotation)
    GetSetMember(BOOL, KeepPixelTime)
    GetSetMember(BOOL, RetractToUnblankSTEM);
  GetSetMember(BOOL, BlankBeamInSTEM);
  GetSetMember(BOOL, MustUnblankWithScreen);
  GetSetMember(BOOL, RetractOnSTEM);
  GetMember(BOOL, StartingProgram);
  GetMember(CString, SysSubpath);
  GetMember(CString, ArgPlugDir);
  GetSetMember(int, NoExceptionHandler);
  BOOL GetDummyInstance();
  GetSetMember(BOOL, NoCameras);
  GetSetMember(BOOL, ImageWithStageToolMove);
  GetSetMember(bool, AppExiting);
  SetMember(int, IdleBaseCount);
  GetMember(bool, AnyDirectDetectors);
  GetMember(bool, AnySuperResMode);
  GetMember(bool, AnyRetractableCams);
  GetSetMember(BOOL, FrameAlignMoreOpen);
  GetSetMember(int, BkgdGrayOfFFT);
  GetSetMember(float, TruncDiamOfFFT);
  GetSetMember(int, SystemDPI);
  GetSetMember(int, LastSystemDPI);
  GetSetMember(int, ToolExtraWidth);
  GetSetMember(int, ToolExtraHeight);
  GetSetMember(int, ToolTitleHeight);
  GetSetMember(BOOL, ShowRemoteControl);
  GetMember(bool, HasFEIcamera);
  GetSetMember(BOOL, KeepEFTEMstate);
  GetSetMember(BOOL, UseRecordForMontage);
  GetSetMember(BOOL, UseViewForSearch);
  GetSetMember(float, RightBorderFrac);
  GetSetMember(float, BottomBorderFrac);
  GetSetMember(float, MainFFTsplitFrac);
  GetSetMember(int, AssumeCamForDummy);
  int *GetDlgColorIndex() { return &mDlgColorIndex[0]; };
  GetSetMember(bool, AbsoluteDlgIndex);
  GetSetMember(BOOL, AllowCameraInSTEMmode);
  GetSetMember(int, DoseLifetimeHours);
  GetSetMember(int, AddDPItoSnapshots);
  GetSetMember(BOOL, SettingsFixedForIACal);
  GetSetMember(BOOL, ShiftScriptOnlyInAdmin);
  GetSetMember(CString, ScriptToRunAtStart);
  GetSetMember(CString, ScriptToRunAtEnd);
  GetSetMember(CString, ProgramTitleText);
  GetSetMember(double, LastActivityTime);
  GetMember(bool, JustChangingLDarea);
  GetMember(bool, JustDoingSynchro);
  GetMember(bool, JustNavAcquireOpen);
  GetMember(bool, InUpdateWindows);
  GetMember(BOOL, EnableExternalPython);
  GetMember(bool, HasK2OrK3Camera);
  GetMember(int, DEcamCount);
  GetSetMember(int, NavOrLogHadFocus);
  GetSetMember(BOOL, MonospacedLog);
  void SetEnableExternalPython(BOOL inVal);
  std::set<int> *GetIDsToHide() { return &mIDsToHide; };
  std::set<int>  *GetLineHideIDs() { return &mLineHideIDs; };
  std::set<int>  *GetIDsToDisable() { return &mIDsToDisable; };
  std::set<int>  *GetBasicIDsToHide() { return &mBasicIDsToHide; };
  std::set<int>  *GetBasicLineHideIDs() { return &mBasicLineHideIDs; };
  std::set<int>  *GetBasicIDsToDisable() { return &mBasicIDsToDisable; };
  StringSet *GetHideStrings() { return &mHideStrings; };
  StringSet *GetBasicHideStrings() { return &mBasicHideStrings; };
  bool IsIDinHideSet(UINT nID) { return mIDsToHide.count(nID) > 0 ||
      (mBasicMode && mBasicIDsToHide.count(nID)); };
  bool IsIDinLineHides(UINT nID) { return mLineHideIDs.count(nID) > 0 ||
      (mBasicMode && mBasicLineHideIDs.count(nID)); };
  bool IsIDinDisableSet(UINT nID) { return mIDsToDisable.count(nID) ||
    (mBasicMode && mBasicIDsToDisable.count(nID)); };
  bool IsStringInHideSet(std::string &str) { return mHideStrings.count(str) ||
    (mBasicMode && mBasicHideStrings.count(str)); };

  HitachiParams *GetHitachiParams() {return &mHitachiParams;};

  RangeFinderParams *GetTSRangeParams() {return &mTSRangeParams[0];};
  ScreenShotParams *GetScreenShotParams() { return &mScreenShotParams; };
  int *GetTssPanelStates() {return &mTssPanelStates[0];};
  BOOL NavigatorStartedTS();

  // Convenience pointers to program components
  CSerialEMView *mActiveView;        // The active view window
  CSerialEMView *mMainView;          // The main view window
  CSerialEMView *mStackView;          // The stack view window
  CSerialEMView *mFFTView;           // View for side-by-side FFT
  CMenuTargets mMenuTargets;
  CImageLevelDlg mImageLevel;
  CEMbufferWindow mBufferWindow;
  CTiltWindow mTiltWindow;
  EMstatusWindow mStatusWindow;
  CAlignFocusWindow mAlignFocusWindow;
  CMontageWindow mMontageWindow;
  CCameraMacroTools mCameraMacroTools;
  CScopeStatusDlg mScopeStatus;
  CLowDoseDlg mLowDoseDlg;
  CRemoteControl mRemoteControl;
  CFilterControlDlg mFilterControl;
  CSTEMcontrolDlg mSTEMcontrol;
  DirectElectronToolDlg mDEToolDlg;
  CMainFrame *mMainFrame;

  EMbufferManager *mBufferManager;
  KImageStore  *mStoreMRC;
  CSerialEMDoc * mDocWnd;
  CEMscope  *mScope;
  CCameraController *mCamera;
  CParameterIO *mParamIO;
  CShiftManager *mShiftManager;
  CShiftCalibrator *mShiftCalibrator;
  EMmontageController *mMontageController;
  CLogWindow *mLogWindow;
  CFocusManager *mFocusManager;
  CMacCmd *mMacroProcessor;
  CMacroEditer *mMacroEditer[MAX_MACROS];
  CMacroToolbar *mMacroToolbar;
  CComplexTasks *mComplexTasks;
  CBeamAssessor *mBeamAssessor;
  CProcessImage *mProcessImage;
  CTSController *mTSController;
  CFilterTasks *mFilterTasks;
  CDistortionTasks *mDistortionTasks;
  CGainRefMaker *mGainRefMaker;
  CNavigatorDlg *mNavigator;
  CNavHelper *mNavHelper;
  CFalconHelper *mFalconHelper;
  CCalibCameraTiming *mCalibTiming;
  CCookerSetupDlg *mCookerDlg;
  CAutocenSetupDlg *mAutocenDlg;
  CVPPConditionSetup *mVPPConditionSetup;
  CScreenShotDialog *mScreenShotDialog;
  CMultiTSTasks *mMultiTSTasks;
  CParticleTasks *mParticleTasks;
  CMailer *mMailer;
  CGatanSocket *mGatanSocket;
  CPluginManager *mPluginManager;
  CPiezoAndPPControl *mPiezoControl;
  CStageMoveTool *mStageMoveTool;
  CAutoTuning *mAutoTuning;
  CExternalTools *mExternalTools;
  CPythonServer *mPythonServer;
  CString  m_strTitle;
  HitachiParams mHitachiParams;
  CString mStartupMessage;     // Message to display in box or log window on startup

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSerialEMApp)
public:
  virtual BOOL InitInstance();
  virtual BOOL OnIdle(LONG lCount);
  virtual int ExitInstance();
  virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
  //}}AFX_VIRTUAL

  // Implementation
  //{{AFX_MSG(CSerialEMApp)
  afx_msg void OnAppAbout();
  afx_msg void OnZoomdown();
  afx_msg void OnUpdateNoTasks(CCmdUI* pCmdUI);
  afx_msg void OnUpdateCameraParameters(CCmdUI* pCmdUI);
  afx_msg void OnCameraSimulation();
  afx_msg void OnUpdateCameraSimulation(CCmdUI* pCmdUI);
  afx_msg void OnFileOpenlog();
  afx_msg void OnUpdateFileOpenlog(CCmdUI* pCmdUI);
  afx_msg void OnFileSavelog();
  afx_msg void OnUpdateFileSavelog(CCmdUI* pCmdUI);
  afx_msg void OnCalibrationAdministrator();
  afx_msg void OnUpdateCalibrationAdministrator(CCmdUI* pCmdUI);
  afx_msg void OnFileSavelogas();
  afx_msg void OnHelp();
  afx_msg void OnHelpUsing();
  afx_msg void OnFileReadappend();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

private:
  BOOL mNeedStaticWindow;
  bool mNeedFFTWindow;
  BOOL mViewClosing;
  BOOL mViewOpening;
  EMimageBuffer *mStackViewImBuf;     // Buffer to pass to it on creation
  EMimageBuffer mImBufs[MAX_BUFFERS];
  EMimageBuffer mFFTBufs[MAX_FFT_BUFFERS];
  ControlSet   mConSets[MAX_CONSETS];
  ControlSet   mCamConSets[MAX_CAMERAS][MAX_CONSETS]; // order so can step through modes
  CameraParameters  mCamParams[MAX_CAMERAS];
  CString mModeName[MAX_CONSETS];
  MagTable mMagTab[MAX_MAGS];
  int mCamLengths[MAX_CAMLENS];
  float mCamLenCalibrated[MAX_CAMLENS];
  LowDoseParams mLowDoseParams[MAX_LOWDOSE_SETS];
  LowDoseParams mCamLowDoseParams[3][MAX_LOWDOSE_SETS];
  NavParams mNavParams;
  NavAcqParams mNavAcqParams[2];
  CookParams mCookParams;
  RangeFinderParams mTSRangeParams[2];
  CString mMacroName[MAX_TOT_MACROS];
  CString mLongMacroName[MAX_TOT_MACROS];
  CString mMacroFileName[MAX_MACROS];
  CString mMacroSaveFile[MAX_MACROS];
  int mSelectedConSet;
  BOOL mAdministrator;       // The boolean for whether in Admin mode or not
  int mAdministratorMode;    // And a value that can be negative to forbid turning it 0
  BOOL mCalNotSaved;
  BOOL mComModuleInited;
  BOOL mExitOnScopeError;
  BOOL mScopeHasFilter;
  BOOL mScopeHasSTEM;
  int mFirstSTEMcamera;                // Index of first STEM camera or -1 if none
  BOOL mOpenStateWithNav;              // Flag to open state dialog with the Navigator

  int mImBufIndex;
  int mFFTbufIndex;
  float mPctLo, mPctHi;
  float mAreaFrac;
  int mBkgdGrayOfFFT;                  // Mean gray level for background of FFT
  float mTruncDiamOfFFT;                 // Diameter of FFT area to truncate as frac of size
  DialogTable mDialogTable[MAX_TOOL_DLGS];
  int mMaxDialogWidth;
  int mNumToolDlg;
  RECT mDlgPlacements[MAX_TOOL_DLGS];
  int mDlgColorIndex[MAX_TOOL_DLGS];
  bool mAbsoluteDlgIndex;
  CToolDlg *mToolDlgs[MAX_TOOL_DLGS];
  WINDOWPLACEMENT mLogPlacement;
  WINDOWPLACEMENT mNavPlacement;
  WINDOWPLACEMENT mCamSetupPlacement;
  WINDOWPLACEMENT mStageToolPlacement;
  WINDOWPLACEMENT mScreenShotPlacement; 
  BOOL mReopenLog;
  ScreenShotParams mScreenShotParams;

  MontParam  mMontParam;
  BOOL mMontaging;
  BOOL mSavingOther;                   // Flag that saving to other file
  int mNumAvailCameras;                   // Number of cameras initialized
  int mActiveCamListSize;              // Number of active cameras read from properties
  int mNumActiveCameras;               // Number of usable active cameras
  int mNumReadInCameras;           // Number of active plus read in cameras in active list
  int mCurrentCamera;                  // Number of camera in full list
  int mCurrentActiveCamera;            // Index of camera in active list
  int mActiveCameraList[MAX_CAMERAS];  // List of indexes to fixed camera number
  int mOriginalActiveList[MAX_CAMERAS];  // Original list from properties file
  int mInitialCurrentCamera;            // Camera to set as initial current camera
  int mLastCamera;                     // Last camera available for copy from in setup dialog
  BOOL mAny16BitCameras;               // Are any cameras 16 bit?
  BOOL mRetractOnEFTEM;                // Retract current camera when entering EFTEM
  BOOL mProcessHere;                   // Global default flag for processing here

  CArray<IdleCallBack *, IdleCallBack *> mIdleArray;
  CString mMacros[MAX_TOT_MACROS];
  MacroControl mMacControl;

  float mMemoryLimit;                  // Limit on memory available for buffers
  CString mBlinkText;          // Text for simple (rightmost) pane, to blink
  DWORD mNextBlinkTime;        // Time for next change
  BOOL mBlinkOn;               // Flag that it is on
  DWORD mLastCheckTime;        // Last time idle loop was checked
  DWORD mCheckThreshold;       // Threshold interval for adding time to timeouts
  BOOL mActPostExposure;       // Flag to live dangerously
  BOOL mSmallFontsBad;         // Flag to use MS Sans Serif instead (laptop?)
  BOOL mDisplayNotTruly120DPI;      // Flag that dialogs need to set 120 DPI
  BOOL mEFTEMMode;             // State flag for EFTEM mode
  BOOL mSTEMMode;              // Flag for STEM mode
  BOOL mSettingSTEM;           // Flags to prevent double calls into set routines
  BOOL mSettingEFTEM;
  FilterParams mFilterParams;
  BOOL mStartupInfo;           // Give startup info if log open
  BOOL mExitWithUnsavedLog;    // Developer's flag to exit without having to say no
  BOOL mTestGainFactors;       // Flag to multiply images by gain factors
  BOOL mSkipGainRefWarning;    // Flag to skip warning about normalized spot
  BOOL mReopenMacroToolbar;    // Flag to reopen macro toolbar on startup
  bool mReopenMacroEditor[MAX_MACROS + 1];   // Flags for reopening macro editors
  BOOL mDeferBufWinUpdates;    // Ignore calls to UpdateBufferWindows
  BOOL mContinuousSaveLog;     // Do continuous save of log file
  int mTssPanelStates[NUM_TSS_PANELS];   // States of tilt series dialog panels
  BOOL mNonGIFMatchPixel;      // Flags to match pixel size/intensity between nonGIF cameras
  BOOL mNonGIFMatchIntensity;
  BOOL mSTEMmatchPixel;
  BOOL mScreenSwitchSTEM;      // Flag to turn off STEM mode when screen down
  BOOL mInvertSTEMimages;      // Flag to invert contrast of acquired STEM images
  float mAddedSTEMrotation;    // Additional rotation over that defined in properties
  float mArrowBeamIncrement;   // Increment for moving beam with keys
  float mArrowBrightIncrement;   // Increment for changing intensity with keys
  BOOL mKeepPixelTime;         // Keep STEM pixel time the same when change binning
  BOOL mRetractToUnblankSTEM;  // Retract cameras to unblank STEM
  BOOL mBlankBeamInSTEM;       // State variable for blanking in STEM
  BOOL mMustUnblankWithScreen; // Disable retracting cameras as option
  BOOL mRetractOnSTEM;         // Flag to retract cameras with blanking when entering STEM
  BOOL mStartingProgram;
  int mNoExceptionHandler;    // Flag to not set up exception handler or not allow MS box
  BOOL mDummyInstance;         // Flag for second dummy instance
  CString mSysSubpath;         // Remainder of command line arguments
  CString mArgPlugDir;         // Plugin directory by command line argument
  BOOL mNoCameras;             // Flag for no cameras
  BOOL mImageWithStageToolMove; // Flag to take image after each move with stage move tool
  PlugStopFunc mPlugStopFunc;  // Stop function in plugin, called with error value
  PlugDoingFunc mPlugDoingFunc; // Function for a plugin to indicate doing a task
  bool mPlugImagingTask;        // Flag that plugin task is tested as an imaging task
  bool mAnyDirectDetectors;     // Flag for whther any cameras are direct detectors
  bool mAnySuperResMode;        // Flag if any camera has a super-res mode
  bool mAnyRetractableCams;     // Flag if any camera is retractable
  BOOL mFrameAlignMoreOpen;     // Flag that bottom panel is open in frame align dialog
  bool mAppExiting;             // Flag that program is exiting
  int mIdleBaseCount;           // Count for letting base class finish idle tasks
  int mSystemDPI;               // DPI detected from system or passed by properties
  int mLastSystemDPI;           // DPI saved in a settings file
  int mToolExtraWidth;          // DPI-dependent parameters for tool panel 
  int mToolExtraHeight;
  int mToolTitleHeight;
  BOOL mShowRemoteControl;      // Flag to show remote control dialog
  CFont mLittleFont;            // Central place to get the right font
  bool mMadeLittleFont;         // Flag that it was made already
  CFont mBoldFont;              // Central place to get a bold font for standard text
  bool mMadeBoldFont;         // Flag that it was made already
  bool mHasFEIcamera;           // Flag that there is an FEI camera
  BOOL mKeepEFTEMstate;         // Flag to stay in or out of EFTEM on startup/shutdown
  BOOL mUseRecordForMontage;    // Flag to use Record parameters for Montage
  BOOL mUseViewForSearch;       // Flag to use View parameters for Search
  float mRightBorderFrac;       // User's setting for right border of main as frac of area
  float mBottomBorderFrac;      // User's setting for bottom border of main as frac
  float mMainFFTsplitFrac;      // Fraction of right-left area taken by Main with FFT open
  int mRightFrameWidth;         // difference between main and frame right edge at startup
  int mBottomFrameWidth;        // difference between main and frame bottom edge at start
  int mAssumeCamForDummy;       // Camera to assume for dummy instance
  BOOL mAllowCameraInSTEMmode;  // Allow a true camera to be used in STEM mode
  int mDoseLifetimeHours;       // Validity of electron dose calibration in hours
  int mAddDPItoSnapshots;       // 0 not to, 1 to use true pixel, > 1 for value to use
  BOOL mSettingsFixedForIACal;  // Flag that settings have been converted for IA limit cal
  BOOL mShiftScriptOnlyInAdmin; // Flag that scripts can be shifted up/down only in Admin
  std::set<int>  mIDsToHide;
  std::set<int>  mLineHideIDs;
  std::set<int>  mIDsToDisable;
  std::set<int>  mBasicIDsToHide;
  std::set<int>  mBasicLineHideIDs;
  std::set<int>  mBasicIDsToDisable;
  StringSet mHideStrings;
  StringSet mBasicHideStrings;
  BOOL mBasicMode;
  CString mScriptToRunAtStart;
  CString mScriptToRunAtEnd;
  CString mProgramTitleText;
  double mLastActivityTime;     // Tick time of last redisplay or something in idle array
  bool mJustChangingLDarea;     // Flag that "DOingTasks" was true because of LD change
  bool mJustDoingSynchro;       // Flag that "DoingTasks" true because of synchro thread
  bool mJustNavAcquireOpen;     // Flag that "DoingTasks" true because of nav acquire
  bool mInUpdateWindows;        // Flag that update call is from UpdateBufferWindows
  BOOL mEnableExternalPython;   // Flag to open socket for external python
  bool mHasK2OrK3Camera;        // Flag for whether there is K2 or K3
  int mDEcamCount;              // Number of DE cameras
  int mNavOrLogHadFocus;        // 1 if nav last got focus, -1 if log did
  BOOL mMonospacedLog;          // Flag that log window should use monospaced font

public:
  void UpdateAllEditers(void);
  void InitializeLDParams(void);
  void InitializeOneLDParam(LowDoseParams &ldParam);
  CString GetDebugKeys(void);
  int AddToStackView(EMimageBuffer * imBuf, int angleOrder);
  void ViewClosing(BOOL stackView, BOOL FFTview);
  void DetachStackView(void);
  BOOL UserAcquireOK(void);
  float GetGainFactor(int camera, int binning);
  int MinuteTimeStamp(void);
  void UpdateMacroButtons(void);
  void ViewOpening(void);
  afx_msg void OnFileContinuousSave();
  afx_msg void OnUpdateFileContinuousSave(CCmdUI *pCmdUI);
  void DrawReadInImage(void);
  void ResizeStackView(int extraBordLeft, int bordTop);
  void GetMainRect(CRect * rect, int & dialogOffset);
  int LookupActiveCamera(int camInd);
  CString PixelFormat(float pixelInNm, float pixScale = 1.);
  int CountOpenViews(void);
  void SetPlacementFixSize(CWnd * window, WINDOWPLACEMENT * lastPlacement);
  void FixInitialPlacements(void);
  void ReopenCameraSetup(void);
  void SetSTEMMode(BOOL inState);
  void GetNumMagRanges(int camera, int &numRanges, int &lowestMicro);
  void GetMagRangeLimits(int camera, int magInd, int &lowerLimit, int &upperLimit);
  int FindNextMagForCamera(int camera, int magInd, int delta, BOOL noLM = false);
  void ManageSTEMBlanking(void);
  int DoDropScreenForSTEM(void);
  BOOL DoSwitchSTEMwithScreen(void);
  int NextValidBinning(int curBinning, int direction, int camera, bool allowFractional);
  bool BinningIsValid(int binning, int camera, bool allowFractional);
  CString BinningText(int binning, int divideBinToShow);
  CString BinningText(int binning, CameraParameters *camParam);
  void RestoreCameraForExit(void);
  void RemoveIdleTask(int source);
  EMimageBuffer * GetActiveNonStackImBuf(void);
  PlugStopFunc RegisterPlugStopFunc(PlugStopFunc func);
  PlugDoingFunc RegisterPlugDoingFunc(PlugDoingFunc func, bool imaging, bool &wasImaging);
  bool DoingRegisteredPlugCall(void);
  double ProgramStartTime(void);
  GetMember(BOOL, BasicMode);
  void SetBasicMode(BOOL inVal);
  void ManageDialogOptionsHiding(void);
  int LookupToolDlgIndex(int colorInd);
 afx_msg void OnUpdateShowScopeControlPanel(CCmdUI *pCmdUI);
afx_msg void OnShowScopeControlPanel();
void SetMaxDialogWidth(void);
void CopyOptionalSetIfNeeded(int inSet, int inCam = -1);
int GetBuildDayStamp(void);
int GetIntegerVersion(void);
void AdjustSizesForSuperResolution(int iCam);
void MainViewResizing(CRect &winRect, bool FFTwin);
void OpenOrCloseMacroEditors(void);
void ClearAllMacros(void);
CFont * GetLittleFont(CWnd *stat);
CFont * GetBoldFont(CWnd *stat);
float GetScalingForDPI();
int ScaleValueForDPI(double value);
afx_msg void OnWindowRescuelogwindow();
afx_msg void OnUpdateWindowRescuelogwindow(CCmdUI *pCmdUI);
afx_msg void OnFileUseMonospacedFont();
afx_msg void OnUpdateFileUseMonospacedFont(CCmdUI *pCmdUI);
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERIALEM_H__F084D61C_0A12_4CD1_AC1F_1AC785054FCE__INCLUDED_)
