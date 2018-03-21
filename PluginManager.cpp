#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include ".\PluginManager.h"
#include "ParameterIO.h"
#include "TSController.h"
#include "PiezoAndPPControl.h"

typedef int (*InfoFunc)(const char **, int *, int *);
typedef int (*CallFunc)(int, const char **, int *, int *, int *);
typedef int (*CallFuncEx)(int, const char **, int *, int *, int *, int *, int *, 
                          double *, double *, double *, double *, const char **);
typedef void (*AppPtrFunc)(CSerialEMApp *);

typedef double (*Func1Dbl)(double);
typedef double (*Func2Dbl)(double, double);
typedef double (*Func3Dbl)(double, double, double);
typedef double (*Func4Dbl)(double, double, double, double);
typedef double (*Func5Dbl)(double, double, double, double, double);
typedef double (*Func6Dbl)(double, double, double, double, double, double);
typedef double (*Func1Int)(int);
typedef double (*Func2Int)(int, int);
typedef double (*Func3Int)(int, int, int);
typedef double (*FuncNoarg)(void);
typedef double (*Func1DblStr)(double, char *);
typedef double (*Func2DblStr)(double, double, char *);
typedef double (*Func3DblStr)(double, double, double, char *);
typedef double (*Func4DblStr)(double, double, double, double, char *);
typedef double (*Func5DblStr)(double, double, double, double, double, char *);
typedef double (*Func6DblStr)(double, double, double, double, double, double, char *);
typedef double (*Func1IntStr)(int, char *);
typedef double (*Func2IntStr)(int, int, char *);
typedef double (*Func3IntStr)(int, int, int, char *);
typedef double (*FuncStr)(char *);
typedef double (*Func1DblOut)(double *);
typedef double (*Func2DblOut)(double *, double *);
typedef double (*Func3DblOut)(double *, double *, double *);
#define MAX_CALL_INTS 3
#define MAX_CALL_DBLS 6
#define MAX_CALL_OUTDBLS 3

#define CAMERA_PROC(a,b) cfuncs->b = (a)GetProcAddress(module, #b)
#define DECAM_PROC(a,b) mDEcamFuncs.b = (a)GetProcAddress(module, #b) ; if (!mDEcamFuncs.b) i++;
#define SCOPE_PROC(a,b,c) mScopeFuncs.a = (b)GetProcAddress(module, c) ; if (!mScopeFuncs.a) i++; else j++
#define SCOPE_SAMENAME(a, b) SCOPE_PROC(b, a, #b )
#define GET_ONE_INT(a) SCOPE_PROC(a, ScopeGetInt, #a )
#define GET_ONE_BOOL(a) SCOPE_PROC(a, ScopeGetBool, #a )
#define GET_ONE_DBL(a) SCOPE_PROC(a, ScopeGetDbl, #a )
#define GET_TWO_DBL(a) SCOPE_PROC(a, ScopeGetTwoDbl, #a )
#define SET_ONE_INT(a) SCOPE_PROC(a, ScopeSetInt, #a )
#define SET_ONE_BOOL(a) SCOPE_PROC(a, ScopeSetBool, #a )
#define SET_ONE_DBL(a) SCOPE_PROC(a, ScopeSetDbl, #a )
#define SET_TWO_DBL(a) SCOPE_PROC(a, ScopeSetTwoDbl, #a )
#define CALL_NO_ARGS(a) SCOPE_PROC(a, ScopeNoArg, #a )
#define MATCHING_NAMES_ONLY

CPluginManager::CPluginManager(void)
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mPlugins.SetSize(0, 4);
  mScopePlugIndex = -1;
  mDEplugIndex = -1;
  mLastTiltIndex = -1;
  mAnyTSplugins = false;
}

CPluginManager::~CPluginManager(void)
{
}

int CPluginManager::LoadPlugins(void)
{
  CString path, exePath, plugPath = mWinApp->mDocWnd->GetPluginPath();
  CFileStatus status;
  CString mess, typeStr;
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind = NULL;
  HMODULE module;
  DWORD lastErr;
  InfoFunc getInfo;
  CallFunc getCall;
  CallFuncEx getCallEx;
  AppPtrFunc appCall;
  PluginData *newPlug;
  PluginCall newCall;
  CamPluginFuncs *cfuncs;
  char fullPath[MAX_PATH + 10];
  const char *namep, *actionp;
  int numFuncs = 0, err = 0, flags, i, j, dirLoop = 0;
  double dum1, dum2, dum3, dum4;
  bool twoScopes = false;
  int action = LOG_SWALLOW_IF_CLOSED;

  path = mWinApp->GetArgPlugDir();
  if (!path.IsEmpty()) {
    plugPath = path;
    dirLoop = 1;
  }
  for (; dirLoop < 2; dirLoop++) {
    if (dirLoop) {
      path = plugPath;
      mess = path + "\\*.dll";
    } else {
      i = GetModuleFileName(NULL, fullPath, MAX_PATH + 10);
      if (!i || i >= MAX_PATH + 10) 
        continue;
      UtilSplitPath(CString(fullPath), exePath, mess);
      path = exePath;
      if (path.IsEmpty())
        continue;
      mess = path + "\\*Plugin*.dll";
    }

    // Trim trailing \ since it would make the status fail
    while (path.GetLength() > 1 && path.GetAt(path.GetLength() - 1) == '\\')
      path = path.Left(path.GetLength() - 1);

    // Check that directory exists
    if (!CFile::GetStatus((LPCTSTR)path, status))
      continue;

    hFind = FindFirstFile((LPCTSTR)mess, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
      continue;
    do {
      module = AfxLoadLibrary((LPCTSTR)(path + "\\" + CString(FindFileData.cFileName)));
      if (!module) {
        lastErr = GetLastError();
        mess.Format("An error (%d) occurred loading %s as a plugin library", lastErr,
          FindFileData.cFileName);
        mWinApp->AppendToLog(mess, action);
        continue;
      }
      getInfo = (InfoFunc)GetProcAddress(module, "SEMPluginInfo");
      if (getInfo) {
        err = getInfo(&namep, &flags, &numFuncs);
        if (!err && numFuncs > 0) {
          if (flags & PLUGFLAG_INFOEX) {
            getCallEx = (CallFuncEx)GetProcAddress(module, "SEMCallInfoEx");
            err = getCallEx ? 0 : 1;
          } else {
            getCall = (CallFunc)GetProcAddress(module, "SEMCallInfo");
            err = getCall ? 0 : 1;
          }
        }
      }
      if (!getInfo || err) {
        mess.Format("Tried to load %s as a plugin library but %s",
          FindFileData.cFileName, err ? "there was an error getting info from it" :
          "could not resolve SEMPluginInfo or SEMCallInfo/SEMCallInfoEx");
        mWinApp->AppendToLog(mess, action);
        AfxFreeLibrary(module);
        continue;
      }

      // Forbid more than one scope plugin
      if ((flags & PLUGFLAG_SCOPE) && mScopePlugIndex >= 0) {
        mess.Format("There is more than one scope plugin in\r\n%s%s%s\r\n\r\nThe plugin "
          "%s in %s\r\nwas just loaded in addition to the one shown first in the log\r\n"
          "\r\nDo not run SerialEM.exe in a package folder, only in a SerialEM folder!"
          "\r\nIf that is not the problem, remove whichever plugin is not appropriate", 
          (LPCTSTR)plugPath, !exePath.IsEmpty() ? " and/or " : "", 
          !exePath.IsEmpty() ? (LPCTSTR)exePath : "",
          FindFileData.cFileName, (LPCTSTR)path);
        mWinApp->AppendToLog(mess, action);
        AfxMessageBox(mess, MB_EXCLAME);
        AfxFreeLibrary(module);
        twoScopes = true;
        continue;
      }

      appCall = (AppPtrFunc)GetProcAddress(module, "SetAppPointer");
      if (appCall)
        appCall(mWinApp);

      // Now we are ready for a new library
      newPlug = new PluginData;
      newPlug->handle = module;
      newPlug->shortName = namep;
      newPlug->flags = flags;
      newPlug->calls.SetSize(0, 4);
      cfuncs = &newPlug->camFuncs;

      // But first get camera plugin functions and make sure it has required functions
      if (flags & PLUGFLAG_CAMERA) {
        CAMERA_PROC(CamNoArg, GetNumberOfCameras);
        CAMERA_PROC(CamOneInt, InitializeInterface);
        CAMERA_PROC(CamOneInt, InitializeCamera);
        CAMERA_PROC(CamNoArg, UninitializeCameras);
        CAMERA_PROC(CamOneInt, SelectCamera);
        CAMERA_PROC(CamOneInt, SelectShutter);
        CAMERA_PROC(CamTwoDbl, SetExposure);
        CAMERA_PROC(Cam5Int, SetAcquiredArea);
        CAMERA_PROC(CamTwoInt, PrepareForAcquire);
        CAMERA_PROC(CamAcquire, AcquireImage);
        CAMERA_PROC(CamOneInt, SetDebugMode);
        CAMERA_PROC(CamOneInt, IsCameraInserted);
        CAMERA_PROC(CamTwoInt, SetCameraInsertion);
        CAMERA_PROC(CamTwoInt, SetRotationFlip);
        CAMERA_PROC(CamThreeInt, SetSizeOfCamera);
        CAMERA_PROC(CamNoArg, StopContinuous);
        CAMERA_PROC(CamOneInt, SetAcquireFlags);
        CAMERA_PROC(GetErrStr, GetLastErrorString);
        CAMERA_PROC(CamSTEMAcq, AcquireSTEMImage);
        CAMERA_PROC(CamSTEMProp, GetSTEMProperties);
        CAMERA_PROC(CamNoArg, GetNumberOfGains);
        CAMERA_PROC(CamOneInt, SetGainIndex);
        CAMERA_PROC(CamGetTwoInt, GetCameraSize);
        CAMERA_PROC(CamGetTwoInt, GetPluginVersion);

        if (!cfuncs->AcquireImage || !cfuncs->GetNumberOfCameras || !cfuncs->SetExposure ||
          !cfuncs->SetAcquiredArea) {
            mess.Format("Tried to load %s as a camera plugin but could not resolve some "
              "required functions", FindFileData.cFileName);
            mWinApp->AppendToLog(mess, action);
            delete newPlug;
            AfxFreeLibrary(module);
            continue;
        }
      }

      // Get scope functions for a scope plugin
      if (flags & PLUGFLAG_SCOPE) {
        i = j = 0;
        SCOPE_PROC(GetIsSmallScreenDown, ScopeGetBool, "GetSmallScreenOut");
        SCOPE_PROC(GetPluginVersions, CamGetTwoInt, "GetPluginVersions");
#include "StandardScopeCalls.h"
        mScopePlugIndex = (int)mPlugins.GetSize();
        SEMTrace('1', "%d of %d possible functions resolved in scope plugin", j, i + j);
      }

      // Get Direct Electron plugin functions, forbid two, require all functions
      if (flags & PLUGFLAG_DECAM) {
        if (mDEplugIndex >= 0) {
          i = 1;
          mess.Format("There is more than one DE camera interface plugin in\r\n%s%s%s\r\n"
            "\r\nThe plugin %s in %s will be ignored\r\n",
            (LPCTSTR)plugPath, !exePath.IsEmpty() ? " and/or " : "", 
            !exePath.IsEmpty() ? (LPCTSTR)exePath : "",
            FindFileData.cFileName, (LPCTSTR)path);
        } else {

          i = 0;
          DECAM_PROC(DEconnect, connect);
          DECAM_PROC(DEnoArg, close);
          DECAM_PROC(DEnoArg, isConnected);
          DECAM_PROC(DEtwoChar, setProperty);
          DECAM_PROC(DEoneChar, setCameraName);
          DECAM_PROC(DEgetImage, getImage);
          DECAM_PROC(DEstringVec, getCameraNames);
          DECAM_PROC(DEstringVec, getProperties);
          DECAM_PROC(DEgetFloat, getFloatProperty);
          DECAM_PROC(DEgetInt, getIntProperty);
          DECAM_PROC(DEgetString, getProperty);
          DECAM_PROC(DEsetMode, setLiveMode);
          mDEcamFuncs.getIsInLiveMode = (DEnoArg)GetProcAddress(module, "getIsInLiveMode");
          DECAM_PROC(CamNoArg, getLastErrorCode);
          DECAM_PROC(DEerrString, getLastErrorDescription);

          if (i)
            mess.Format("Tried to load %s as a plugin for the DE camera interface but\r\n"
            "   could not resolve some required functions; this plugin is not up to date",
            FindFileData.cFileName);
          else
            mDEplugIndex = (int)mPlugins.GetSize();
        }
        if (i) {
          mWinApp->AppendToLog(mess, action);
          delete newPlug;
          AfxFreeLibrary(module);
          continue;
        }
      }
      
      // Get piezo plugin functions
      if (flags & PLUGFLAG_PIEZO) {
        PiezoPluginFuncs *pfuncs = &newPlug->piezoFuncs;
        pfuncs->SelectPiezo = (CamOneInt)GetProcAddress(module, "SelectPiezo");
        pfuncs->GetNumberOfPiezos = (CamNoArg)GetProcAddress(module, "GetNumberOfPiezos");
        pfuncs->GetXYPosition = (PiezoGetTwoDbl)GetProcAddress(module, "GetXYPosition");
        pfuncs->GetZPosition = (PiezoGetDbl)GetProcAddress(module, "GetZPosition");
        pfuncs->SetXYPosition = (CamTwoDbl)GetProcAddress(module, "SetXYPosition");
        pfuncs->SetZPosition = (PiezoSetDbl)GetProcAddress(module, "SetZPosition");
        pfuncs->Uninitialize = (ScopeNoArg)GetProcAddress(module, "Uninitialize");
        if (!pfuncs->GetNumberOfPiezos || (!pfuncs->GetXYPosition && !pfuncs->GetZPosition)
          || (!pfuncs->SetXYPosition && !pfuncs->SetZPosition)) {
            mess.Format("Tried to load %s as a piezo plugin but could not resolve some "
              "required functions", FindFileData.cFileName);
            mWinApp->AppendToLog(mess, action);
            delete newPlug;
            AfxFreeLibrary(module);
            continue;
        }
      }

      for (i = 0; i < numFuncs; i++) { 
        if (flags & PLUGFLAG_INFOEX)
          err = getCallEx(i, &namep, &newCall.numInts, &newCall.numDbls, &newCall.ifString,
          &newCall.flags, &j, &dum1, &dum2, &dum3, &dum4, &actionp);
        else
          err = getCall(i, &namep, &newCall.numInts, &newCall.numDbls, &newCall.ifString);
        if (err)
          continue;
        newCall.name = namep;
        newCall.func = (PluginFunction)GetProcAddress(module, (LPCSTR)newCall.name);

        // Look up tilt series action and warn if it doesn't match
        if (newCall.func && (flags & PLUGFLAG_TSCALLS) && 
          (newCall.flags & PLUGCALL_TSACTION)) {
            mAnyTSplugins = true;
            newCall.beforeTSaction = mWinApp->mTSController->LookupActionFromText(actionp);
            if (newCall.beforeTSaction <= 0) {
              mess.Format("WARNING: For plugin %s, function %s:\r\n   the string %s does "
                "not match a TS action", newPlug->shortName, namep, actionp);
              mWinApp->AppendToLog(mess, action);
            } else if (newCall.ifString || newCall.numInts > 1 || newCall.numDbls > 2 || 
              (newCall.numInts && newCall.numDbls)) {
                mess.Format("WARNING: For plugin %s, function %s:\r\n   the number or type "
                  "of arguments does not allow it to be called as a TS action", 
                  newPlug->shortName, namep);
                mWinApp->AppendToLog(mess, action);
                newCall.beforeTSaction = -1; 
            }
        }
        if (newCall.func)
          newPlug->calls.Add(newCall);
      }
      mPlugins.Add(newPlug);
      typeStr = (flags & PLUGFLAG_CAMERA) ? "camera " : "";
      if (flags & PLUGFLAG_SCOPE)
        typeStr = "microscope ";
      if (flags & PLUGFLAG_PIEZO)
        typeStr = "piezo ";
      if (numFuncs)
        mess.Format("Loaded %splugin %s (named %s) and resolved %d of %d script-callable "
        "functions", (LPCTSTR)typeStr, FindFileData.cFileName,
        newPlug->shortName, newPlug->calls.GetSize(), numFuncs);
      else
        mess.Format("Loaded %splugin %s (named %s)%s", (LPCTSTR)typeStr,
        FindFileData.cFileName, newPlug->shortName, 
        (flags & (PLUGFLAG_CAMERA | PLUGFLAG_SCOPE)) ? 
        "" : " with no script-callable functions");
      mWinApp->AppendToLog(mess, action);

    } while (FindNextFile(hFind, &FindFileData) != 0);

    DWORD dwError = GetLastError();
    FindClose(hFind);
    if (dwError != ERROR_NO_MORE_FILES)
      mWinApp->AppendToLog(CString("An error occurred getting filenames in the ") + 
      CString(dirLoop ? "Plugins" : "SerialEM executable") + "directory", action);
  }
  if (twoScopes)
    mScopePlugIndex = -1;
  
  return (int)mPlugins.GetSize();
}
  
CString CPluginManager::GetScopePluginName()
{
  PluginData *plugin;
  if (mScopePlugIndex < 0)
    return CString("");
  plugin = mPlugins[mScopePlugIndex];
  return plugin->shortName;
}

// According to Hans Windhoff (OSIS), doing this from destructors is too late
void CPluginManager::ReleasePlugins(void)
{
  PluginData *plugin;
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if (plugin->flags & PLUGFLAG_CAMERA && plugin->camFuncs.UninitializeCameras)
      plugin->camFuncs.UninitializeCameras();
    if (plugin->flags & PLUGFLAG_SCOPE && mScopeFuncs.UninitializeScope)
      mScopeFuncs.UninitializeScope();
    if (plugin->flags & PLUGFLAG_PIEZO && plugin->piezoFuncs.Uninitialize)
      plugin->piezoFuncs.Uninitialize();
    AfxFreeLibrary(plugin->handle);
    delete plugin;
  }
}

double CPluginManager::ExecuteCommand(CString strLine, int *itemInt, double *itemDbl,
                                      BOOL *itemEmpty, CString &report, double &outD1, 
                                      double &outD2, double &outD3, int &numOut,int &err)
{
  CString mess;
  CString strItems[4];
  PluginData *plugin;
  PluginCall call;
  int plug, ind, args;
  double retval;
  err = 1;
  numOut = 0;
  
  // Lookup the plugin name
  mWinApp->mParamIO->ParseString(strLine, strItems, 4);
  for (plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if (plugin->shortName == strItems[1])
      break;
  }
  if (plug >= mPlugins.GetSize()) {
    mess.Format("There is no plugin loaded named %s, in statement:\n\n", strItems[1]);
    AfxMessageBox(mess + strLine, MB_OK);
    return 0.;
  }
    
  // Lookup the function
  for (ind = 0; ind < plugin->calls.GetSize(); ind++) {
    call = plugin->calls[ind];
    if (call.name == strItems[2])
      break;
  }
  if (ind >= plugin->calls.GetSize()) {
    mess.Format("There is no function named %s in this plugin, in statement:\n\n",
      strItems[2]);
    AfxMessageBox(mess + strLine, MB_OK);
    return 0.;
  }
  if (call.ifString >= 0 && (call.numInts > MAX_CALL_INTS || call.numDbls > MAX_CALL_DBLS
    || (call.numInts > 0 && call.numDbls > 0)) || call.ifString < 0 &&
    (call.numInts > 0 || call.numDbls < 1 || call.numDbls > MAX_CALL_OUTDBLS)) {
    mess.Format("The plugin function %s requires a list of "
      "arguments that the program cannot send, in statement:\n\n", strItems[2]);
    AfxMessageBox(mess + strLine, MB_OK);
    return 0.;
  }
  args = B3DMAX(0, B3DMAX(call.numInts, call.numDbls)) + (call.ifString ? 1 : 0);
  if (call.ifString < 0)
    args = 0;
  if (itemEmpty[2 + args]) {
    mess.Format("The plugin function %s needs %d values and you did not enter enough"
      " values in statement:\n\n", strItems[2], args);
    AfxMessageBox(mess + strLine, MB_OK);
    return 0.;
  }
  err = 0;
  if (call.ifString)
    mWinApp->mParamIO->StripItems(strLine, 2 + args, mess);

  if (call.ifString >= 0)
    args = 100 * (call.ifString ? 1 : 0) + 10 * B3DMAX(0,call.numInts) + 
      B3DMAX(0,call.numDbls);
  else {
    args = -call.numDbls;
    numOut = call.numDbls;
  }
  switch (args) {
    case 0:
      retval = ((FuncNoarg)call.func)();
      break;
    case 1:
      retval = ((Func1Dbl)call.func)(itemDbl[3]);
      break;
    case 2:
      retval = ((Func2Dbl)call.func)(itemDbl[3], itemDbl[4]);
      break;
    case 3:
      retval = ((Func3Dbl)call.func)(itemDbl[3], itemDbl[4], itemDbl[5]);
      break;
    case 4:
      retval = ((Func4Dbl)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6]);
      break;
    case 5:
      retval = ((Func5Dbl)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6],
        itemDbl[7]);
      break;
    case 6:
      retval = ((Func6Dbl)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6],
        itemDbl[7], itemDbl[8]);
      break;
    case 10:
      retval = ((Func1Int)call.func)(itemInt[3]);
      break;
    case 20:
      retval = ((Func2Int)call.func)(itemInt[3], itemInt[4]);
      break;
    case 30:
      retval = ((Func3Int)call.func)(itemInt[3], itemInt[4], itemInt[5]);
      break;
    case 100:
      retval = ((FuncStr)call.func)((char *)(LPCTSTR)mess);
      break;
    case 101:
      retval = ((Func1DblStr)call.func)(itemDbl[3], (char *)(LPCTSTR)mess);
      break;
    case 102:
      retval = ((Func2DblStr)call.func)(itemDbl[3], itemDbl[4], (char *)(LPCTSTR)mess);
      break;
    case 103:
      retval = ((Func3DblStr)call.func)(itemDbl[3], itemDbl[4], itemDbl[5],
        (char *)(LPCTSTR)mess);
      break;
    case 104:
      retval = ((Func4DblStr)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6],
        (char *)(LPCTSTR)mess);
      break;
    case 105:
      retval = ((Func5DblStr)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6],
        itemDbl[7], (char *)(LPCTSTR)mess);
      break;
    case 106:
      retval = ((Func6DblStr)call.func)(itemDbl[3], itemDbl[4], itemDbl[5], itemDbl[6],
        itemDbl[7], itemDbl[8], (char *)(LPCTSTR)mess);
      break;
    case 110:
      retval = ((Func1IntStr)call.func)(itemInt[3], (char *)(LPCTSTR)mess);
      break;
    case 120:
      retval = ((Func2IntStr)call.func)(itemInt[3], itemInt[4], (char *)(LPCTSTR)mess);
      break;
    case 130:
      retval = ((Func3IntStr)call.func)(itemInt[3], itemInt[4], itemInt[5],
        (char *)(LPCTSTR)mess);
      break;
    case -1:
      retval = ((Func1DblOut)call.func)(&outD1);
      break;
    case -2:
      retval = ((Func2DblOut)call.func)(&outD1, &outD2);
      break;
    case -3:
      retval = ((Func3DblOut)call.func)(&outD1, &outD2, &outD3);
      break;
  }

  report.Format("The return value from %s was %6g", call.name, retval);
  return retval;
}

// Return the camera function array for a plugin
CamPluginFuncs *CPluginManager::GetCameraFuncs(CString name)
{
  PluginData *plugin;
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if ((plugin->flags & PLUGFLAG_CAMERA) && name == plugin->shortName)
      return &plugin->camFuncs;
  }
  return NULL;
}

// Fill an array with all piezo plugin functions and return number of plugins
int CPluginManager::GetPiezoPlugins(PiezoPluginFuncs **plugFuncs, CString **names)
{
  PluginData *plugin;
  int num = 0;
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if (plugin->flags & PLUGFLAG_PIEZO) {
      if (num >= MAX_PIEZO_PLUGS) {
        PrintfToLog("WARNING: There are more piezo plugins than the arrays allow; only "
          "%d will be available", MAX_PIEZO_PLUGS);
        break;
      }
      plugFuncs[num] = &plugin->piezoFuncs;
      names[num++] = &plugin->shortName;
    }
  }
  return num;
}

// List function prototypes for all functions in all plugins
void CPluginManager::ListCalls(void)
{
  PluginData *plugin;
  PluginCall call;
  CString mess;
  int args, i, j;
  if (!mPlugins.GetSize()) {
    mWinApp->AppendToLog("No plugins were loaded");
    return;
  }
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    if (plug)
      mWinApp->AppendToLog(" ");
    plugin = mPlugins[plug];
    mess.Format("Functions for plugin %s:", plugin->shortName);
    mWinApp->AppendToLog(mess);
    for (i = 0; i < plugin->calls.GetSize(); i++) {
      args = 0;
      call = plugin->calls[i];
      mess.Format("%s(", call.name);
      for (j = 0; j < call.numInts; j++) {
        if (args)
          mess += ", ";
        mess += "int";
        args++;
      }
      for (j = 0; j < call.numDbls; j++) {
        if (args)
          mess += ", ";
        if (call.ifString < 0)
          mess += "[out]";
        mess += "double";
        args++;
      }
      if (call.ifString > 0) {
        if (args)
          mess += ", ";
        mess += "string";
      }
      mess += ")";
      mWinApp->AppendToLog(mess);
    }
  }
}

// See if there is a call that is appropriate at this point in series, run it and return
// its value
bool CPluginManager::CheckAndRunTSAction(int actionIndex, int tiltIndex, double tiltAngle,
                                         int &err)
{
  PluginData *plugin;
  PluginCall* callp;
  int args;
  double retval = 0.;
  err = 0;
  if (!mAnyTSplugins)
    return false;
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if (!(plugin->flags & PLUGFLAG_TSCALLS))
      continue;
    for (int i = 0; i < plugin->calls.GetSize(); i++) {
      callp = &plugin->calls[i];

      // It is time to do the call if its action index matches and it has not been done
      // on this tilt index
      if ((callp->flags & PLUGCALL_TSACTION) && actionIndex == callp->beforeTSaction &&
        tiltIndex > callp->tiltIndexDone) {
          args = 10 * B3DMAX(0,callp->numInts) + B3DMAX(0,callp->numDbls);
          switch (args) {
            case 0:
              retval = ((FuncNoarg)callp->func)();
              break;
            case 1:
              retval = ((Func1Dbl)callp->func)(tiltAngle);
              break;
            case 2:
              retval = ((Func2Dbl)callp->func)((double)tiltIndex, tiltAngle);
              break;
            case 10:
              retval = ((Func1Int)callp->func)(tiltIndex);
              break;
          }

          // Asynchronous call: save the indices so completion can be marked
          if (retval < 0.) {
            err = -1;
            mLastTiltIndex = tiltIndex;
            mLastTSplugInd = plug;
            mLastTScallInd = i;
          }
          if (retval > 0.)
            err = B3DNINT(B3DMAX(1., retval));

          // If direct successful return, make as done on this index
          if (err == 0) {
            callp->tiltIndexDone = tiltIndex;
            mLastTiltIndex = -1;
          }
          return true;
      }
    }
  }
  return false;
}

// When tilt series starts/resumes, mark all functions as not called on current tilt index
void CPluginManager::ResumingTiltSeries(int tiltIndex)
{
  PluginData *plugin;
  PluginCall *callp;
  for (int plug = 0; plug < mPlugins.GetSize(); plug++) {
    plugin = mPlugins[plug];
    if (plugin->flags & PLUGFLAG_TSCALLS) {
      for (int i = 0; i < plugin->calls.GetSize(); i++) {
        callp = &plugin->calls[i];
        if (callp->flags & PLUGCALL_TSACTION)
          callp->tiltIndexDone = tiltIndex - 1;
      }
    }
  }
  mLastTiltIndex = -1;
}

// When an asynchronous call completes, mark it as done
void CPluginManager::CompletedTSCall(void)
{
  PluginData *plugin;
  PluginCall* callp;
  if (mLastTiltIndex < 0 || mLastTSplugInd < 0 || mLastTSplugInd >= mPlugins.GetSize())
    return;
  plugin = mPlugins[mLastTSplugInd];
  if (mLastTScallInd < 0 || mLastTScallInd >= plugin->calls.GetSize())
    return;
  callp = &plugin->calls[mLastTScallInd];
  callp->tiltIndexDone = mLastTiltIndex;
  mLastTiltIndex = -1;
}
