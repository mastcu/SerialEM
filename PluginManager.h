#pragma once
#include <afxtempl.h>

// Definitions for camera functions
typedef double (*PluginFunction)(void);
typedef const char *(*GetErrStr)(void);
typedef int (*CamNoArg)(void);
typedef int (*CamOneInt)(int);
typedef int (*CamTwoDbl)(double, double);
typedef int (*CamTwoInt)(int, int);
typedef int (*CamTwoInt)(int, int);
typedef int (*CamThreeInt)(int, int, int);
typedef int (*Cam5Int)(int, int, int, int, int);
typedef int (*CamAcquire)(short *, int, int, int *, int *);
typedef int (*CamSTEMAcq)(short **, int, int, long *, double, int, int *, int *, int *);
typedef int (*CamSTEMProp)(int, int, double *, double *, double *, double *, double *, int *,int *);
typedef int (*CamGetTwoInt)(int *,int *);

// Camera plugin functions
struct CamPluginFuncs {
  CamNoArg GetNumberOfCameras;
  CamOneInt InitializeInterface;
  CamOneInt InitializeCamera;
  CamNoArg UninitializeCameras;
  CamOneInt SelectCamera;
  CamOneInt SelectShutter;
  CamTwoDbl SetExposure;
  Cam5Int SetAcquiredArea;
  CamAcquire AcquireImage;
  CamTwoInt PrepareForAcquire;
  CamOneInt SetDebugMode;
  CamOneInt IsCameraInserted;
  CamTwoInt SetCameraInsertion;
  CamTwoInt SetRotationFlip;
  CamThreeInt SetSizeOfCamera;
  CamNoArg StopContinuous;
  CamOneInt SetAcquireFlags;
  GetErrStr GetLastErrorString;
  CamSTEMAcq AcquireSTEMImage;
  CamSTEMProp GetSTEMProperties;
  CamNoArg GetNumberOfGains;
  CamOneInt SetGainIndex;
  CamGetTwoInt GetCameraSize;
  CamGetTwoInt GetPluginVersion;
};

// Definitions for Scope functions
typedef void (*ScopeNoArg)(void);
typedef int (*ScopeGetInt)(void);
typedef double (*ScopeGetDbl)(void);
typedef double (*ScopeGetDblSetInt)(int);
typedef BOOL (*ScopeGetBool)(void);
typedef void (*ScopeGetTwoDbl)(double *, double *);
typedef void (*ScopeSetTwoDbl)(double, double);
typedef void (*ScopeSetDbl)(double);
typedef void (*ScopeSetInt)(int);
typedef void (*ScopeSetTwoInt)(int, int);
typedef void (*ScopeSetThreeInt)(int, int, int);
typedef int (*ScopeGetSetInt)(int);
typedef void (*ScopeSetBool)(PLUGIN_BOOL);
typedef void (*ScopeGetThreeDbl)(double *, double *, double *);
typedef void (*ScopeSetStage)(double, double, double, double, int);
typedef void (*ScopeSetStageExtra)(double, double, double, double, double, double, int);
typedef void (*ScopeGetGauge)(const char *, int *, double *);
typedef double (*ScopeGetDblByName)(const char *);
typedef void (*ScopeGetTwoDblByName)(const char *, double *, double *);
typedef void (*ScopeSetTwoIntDbl)(int, int, double);
typedef int (*GetChannelList)(int, int, long *, const char **, BOOL, long *, long *,
                              BOOL *);
typedef int (*LookupCamera)(const char *, BOOL, int, int *, float *, float *);
typedef int (*FEIimage)(void *, int, int, int, double, double, int, int, int, long, long, 
                        long *, const char *, BOOL, int, BOOL, BOOL, long *);
typedef int (*FEIchannels)(short **, const char **, int, long *, int, int, double, 
                           double, int, int, long, long);
typedef int (*FIFinit)(const char *, float);
typedef int (*FIFconfInit)(const char *, long *);
typedef int (*FIFsetup)(const char *, BOOL, int, int, int, long *);
typedef int (*FIFbuildMap)(const char *, long *, long *, long *);
typedef int (*FIFgetNext)(short *, int, int, BOOL);
typedef int (*FIFclean)(BOOL);
typedef int (*FIFclean2)(int, long*);
typedef int (*DoFocRamp)(long, long, long, float *, long, double);
typedef int (*FinishFocRamp)(long, double *, double *, double *, double *);
typedef int (*ShowMB)(int, const char *, const char *);

#define GET_ONE_INT(a) ScopeGetInt a
#define GET_ONE_BOOL(a) ScopeGetBool a
#define GET_ONE_DBL(a) ScopeGetDbl a
#define GET_TWO_DBL(a) ScopeGetTwoDbl a
#define SET_ONE_INT(a) ScopeSetInt a
#define SET_ONE_BOOL(a) ScopeSetBool a
#define SET_ONE_DBL(a) ScopeSetDbl a
#define SET_TWO_DBL(a) ScopeSetTwoDbl a
#define CALL_NO_ARGS(a) ScopeNoArg a
#define MATCHING_NAMES_ONLY
#define SCOPE_SAMENAME(a, b) a b;

// Scope plugin functions
struct ScopePluginFuncs {
  ScopeGetBool GetIsSmallScreenDown;
  CamGetTwoInt GetPluginVersions;
#include "StandardScopeCalls.h"

};

#undef MATCHING_NAMES_ONLY

// A few definitions still needed for piezo functions
typedef int (*PiezoGetDbl)(double *);
typedef int (*PiezoGetTwoDbl)(double *, double *);
typedef int (*PiezoSetDbl)(double);

// Piezo plugin functions
struct PiezoPluginFuncs {
  CamOneInt SelectPiezo;
  CamNoArg GetNumberOfPiezos;
  PiezoGetTwoDbl GetXYPosition;
  PiezoGetDbl GetZPosition;
  CamTwoDbl SetXYPosition;
  PiezoSetDbl SetZPosition;
  ScopeNoArg Uninitialize;
};

// Definitions for DEcamPlugin
typedef bool (*DEconnect)(const char*, int, int);
typedef bool (*DEnoArg)();
typedef bool (*DEtwoChar)(const char *, const char *);
typedef bool (*DEoneChar)(const char *);
typedef bool (*DEgetImage)(void *, unsigned int);
typedef bool (*DEstringVec)(std::vector<std::string> *);
typedef bool (*DEgetFloat)(std::string, float *);
typedef bool (*DEgetInt)(std::string, int *);
typedef bool (*DEgetString)(std::string, std::string *);
typedef bool (*DEsetMode)(bool);
typedef void (*DEerrString)(std::string *);

// DE camera functions
struct DEPluginFuncs {
  DEconnect connect;
  DEnoArg close;
  DEnoArg isConnected;
  DEtwoChar setProperty;
  DEoneChar setCameraName;
  DEgetImage getImage;
  DEstringVec getCameraNames;
  DEstringVec getProperties;
  DEgetFloat getFloatProperty;
  DEgetInt getIntProperty;
  DEgetString getProperty;
  DEsetMode setLiveMode;
  DEnoArg getIsInLiveMode;
  CamNoArg getLastErrorCode;
  DEerrString getLastErrorDescription;
};

// Information structure for a single plugin function call
struct PluginCall {
  PluginFunction func;
  CString name;
  int numInts;
  int numDbls;
  int ifString;
  int flags;
  int beforeTSaction;
  int tiltIndexDone;
};

// Information about one plugin including arrays of functions
struct PluginData {
  HMODULE handle;
  CString shortName;
  int flags;
  CArray<PluginCall, PluginCall> calls;
  CamPluginFuncs camFuncs;
  PiezoPluginFuncs piezoFuncs;
};

// The plugin manager class
class CPluginManager
{
public:
  CPluginManager(void);
  ~CPluginManager(void);

private:
  CArray<PluginData *, PluginData *> mPlugins;
  CSerialEMApp *mWinApp;
  ScopePluginFuncs mScopeFuncs;
  int mScopePlugIndex;
  DEPluginFuncs mDEcamFuncs;
  int mDEplugIndex;
  int mLastTSplugInd;
  int mLastTScallInd;
  int mLastTiltIndex;
  bool mAnyTSplugins;
public:
  int LoadPlugins(void);
  double ExecuteCommand(CString strLine, int *itemInt, double *itemDbl, BOOL *itemEmpty,
    CString &report, double &outD1, double &outD2, double &outD3, int &numOut, int &err);
  void ListCalls(void);
  CamPluginFuncs *GetCameraFuncs(CString name);
  CString GetScopePluginName();
  void ReleasePlugins(void);
  ScopePluginFuncs *GetScopeFuncs() {return &mScopeFuncs;};
  bool CheckAndRunTSAction(int actionIndex, int tiltIndex, double tiltAngle, int &err);
  void ResumingTiltSeries(int tiltIndex);
  void CompletedTSCall(void);
  GetMember(bool, AnyTSplugins);
  int GetPiezoPlugins(PiezoPluginFuncs **plugFuncs, CString **names);
  DEPluginFuncs *GetDEcamFuncs() {return &mDEcamFuncs;};
  GetMember(int, DEplugIndex);

};

#define THROW_NOFUNC_ERROR throw(_com_error((HRESULT)NOFUNC_FAKE_HRESULT, NULL, true))

// Macros to be used in EMscope, could be useful elsewhere
#define PLUGSCOPE_SET(f, v) \
{ if (!mPlugFuncs->Set##f) { \
  THROW_NOFUNC_ERROR; \
} else { \
  mPlugFuncs->Set##f(v); \
} }

#define PLUGSCOPE_GET(f, v, s) \
{ if (!mPlugFuncs->Get##f) { \
    THROW_NOFUNC_ERROR; \
} else { \
  v = (s) * mPlugFuncs->Get##f(); \
} }


// These two are called from threads where there is MyCamera and info->plugfuncs
#define INFOPLUG_SET(f, v) \
{ if (!info->plugFuncs->Set##f) { \
  THROW_NOFUNC_ERROR; \
} else { \
  info->plugFuncs->Set##f(v); \
} }

#define INFOPLUG_GET(f, v, s) \
{ if (!info->plugFuncs->Get##f) { \
    THROW_NOFUNC_ERROR; \
} else { \
  v = (s) * info->plugFuncs->Get##f(); \
} }
  
