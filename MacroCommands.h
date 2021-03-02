#ifndef MACROCOMMANDS_H
#define MACROCOMMANDS_H
#include "MacroProcessor.h"
#include "EMscope.h"
class CMacCmd : public CMacroProcessor
{
 public:
  CMacCmd();
  virtual ~CMacCmd();
  int NextCommand(bool startingOut);
  void InitForNextCommand();
  void TaskDone(int param);

 private:

  // Member variables assigned to in NextCommand
  static const char *mLongKeys[MAX_LONG_OPERATIONS];
  static int mLongHasTime[MAX_LONG_OPERATIONS];


  CString mStrLine, mStrCopy, mItem1upper;
  CString *mMacro;

  // Use this only for output followed by break not return, output at end of switch
  CString mLogRpt;
  int mCmdIndex, mLastNonEmptyInd;
  BOOL mKeyBreak;
  int mCurrentCam;
  int mReadOtherSleep;
  FilterParams *mFiltParam;
  int *mActiveList;
  CameraParameters *mCamParams;
  MontParam *mMontP;
  LowDoseParams *mLdParam;
  CNavigatorDlg *mNavigator;

  // "Convenience" member variables that can be replaced with local variables in functions
  CString cReport;
  BOOL cTruth, cDoShift, cDoPause, cDoAbort;
  bool cDoBack;
  ScaleMat cAMat, cBInv;
  EMimageBuffer *cImBuf;
  KImage *cImage;
  CFile *cCfile;
  double cDelISX, cDelISY, cDelX, cDelY, cH1, cV1, cV2, cH2, cH3, cV3, cV4, cH4;
  double cStageX, cStageY, cStageZ, cSpecDist;
  float cFloatX, cFloatY;
  int cIndex, cIndex2, cI, cIx0, cIx1, cIy0, cIy1, cSizeX, cSizeY, cMag;
  int cBinning;
  float cBacklashX, cBacklashY, cBmin, cBmax, cBmean, cBSD, cCpe, cShiftX, cShiftY;
  float cFitErr;
  CMapDrawItem *cNavItem;
  MacroFunction *cFunc;
  Variable *cVar;
  CString *cValPtr;
  int *cNumElemPtr;
  CFileStatus cStatus;
  FileForText *cTxFile;
  StageMoveInfo cSmi;

#define MAC_SAME_NAME(nam, req, flg, cme) int nam(void);
#define MAC_DIFF_NAME(nam, req, flg, fnc, cme)  int fnc(void);
#define MAC_SAME_FUNC(nam, req, flg, fnc, cme) 
#include "DefineScriptMacros.h"

 public:
  // COMMAND FUNCTIONS
#include "MacroMasterList.h"
 };


#endif  // MACROCOMMANDS_H
