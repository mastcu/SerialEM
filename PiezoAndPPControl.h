// PiezoAndPPControl.h: declarations for PiezoAndPPControl.cpp
#pragma once
#include "PluginManager.h"

#define MAX_PIEZO_PLUGS 4

class CWinThread;

struct PiezoThreadData 
{
  double pos1, pos2;
  bool setZ;
  PiezoPluginFuncs *plugFuncs;
  int errCode;
};

struct PiezoScaling
{
  int plugNum;
  int piezoNum;
  int zAxis;
  float minPos;
  float maxPos;
  float unitsPerMicron;
};

class CPiezoAndPPControl
{
public:
  CPiezoAndPPControl(void);
  ~CPiezoAndPPControl(void);
  void Initialize(void);

private:
  CSerialEMApp *mWinApp;
  CEMscope *mScope;
  PiezoPluginFuncs *mPlugFuncs[MAX_PIEZO_PLUGS];
  int mNumPlugins;
  int mNumPiezos[MAX_PIEZO_PLUGS];
  CString *mPlugNames[MAX_PIEZO_PLUGS];
  int mSelectedPiezo;
  int mSelectedPlug;
  static UINT PiezoMoveProc(LPVOID pParam);
  CWinThread *mMoveThread;
  PiezoThreadData mPTD;
  CArray<PiezoScaling, PiezoScaling> mScalings;

public:
  bool SelectPiezo(int plugNum, int piezoNum);
  bool GetXYPosition(double &px, double &py);  
  bool GetZPosition(double &pz);
  bool SetXYPosition(double xpos, double ypos, bool incremental = false);
  bool SetZPosition(double zpos, bool incremental = false);
  bool CheckSelection(const char *action);
  void ReportNoAxis(const char * action);
  void ReportAxisError(int err, const char * action);
  CString DrivenByMess(int plugNum);
  void StartMovementThread(double pos1, double pos2, bool setZ);
  static int TaskPiezoBusy(void);
  int PiezoBusy(void);
  void PiezoCleanup(int error);

  int WaitForPiezoReady(int timeOut = 5000);
  int GetLastMovementError() {return mPTD.errCode;};
  CArray<PiezoScaling, PiezoScaling> *GetScalings() {return &mScalings;};
  void ScalePosition(bool zAxis, bool unitsToUm, double & xpos, double & ypos);
  bool LookupScaling(int plugNum, int piezoNum, bool zAxis, PiezoScaling & scaling);
};
