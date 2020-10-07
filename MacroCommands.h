#ifndef MACROCOMMANDS_H
#define MACROCOMMANDS_H
#include "MacroProcessor.h"
#include "EMscope.h"
class CMacCmd : public CMacroProcessor
{
 public:
  CMacCmd();
  virtual ~CMacCmd();
  void NextCommand(bool startingOut);
  void TaskDone(int param);

 private:
  static const char *cLongKeys[MAX_LONG_OPERATIONS];
  static int cLongHasTime[MAX_LONG_OPERATIONS];

  CString cStrLine, cStrCopy, cItem1upper;
  CString cStrItems[MAX_MACRO_TOKENS];
  CString cReport;

  // Use this only for output followed by break not return, output at end of switch
  CString cLogRpt;
  BOOL cItemEmpty[MAX_MACRO_TOKENS];
  int cItemInt[MAX_MACRO_TOKENS];
  double cItemDbl[MAX_MACRO_TOKENS];
  float cItemFlt[MAX_MACRO_TOKENS];
  BOOL cTruth, cDoShift, cKeyBreak, cDoPause, cDoAbort;
  bool cDoBack;
  ScaleMat cAMat, cBInv;
  EMimageBuffer *cImBuf;
  KImage *cImage;
  CFile *cCfile;
  double cDelISX, cDelISY, cDelX, cDelY, cH1, cV1, cV2, cH2, cH3, cV3, cV4, cH4;
  double cStageX, cStageY, cStageZ, cSpecDist;
  float cFloatX, cFloatY;
  int cIndex, cIndex2, cI, cIx0, cIx1, cIy0, cIy1, cSizeX, cSizeY, cMag;
  int cBinning, cLastNonEmptyInd;
  int cCmdIndex;
  float cBacklashX, cBacklashY, cBmin, cBmax, cBmean, cBSD, cCpe, cShiftX, cShiftY;
  float cFitErr;
  int cCurrentCam;
  FilterParams *cFiltParam;
  int *cActiveList;
  CameraParameters *cCamParams;
  MontParam *cMontP;
  LowDoseParams *cLdParam;
  CNavigatorDlg *cNavigator;
  CMapDrawItem *cNavItem;
  MacroFunction *cFunc;
  Variable *cVar;
  CString *cValPtr;
  int *cNumElemPtr;
  CFileStatus cStatus;
  FileForText *cTxFile;
  StageMoveInfo cSmi;
  int cReadOtherSleep;

};

#endif  // MACROCOMMANDS_H
