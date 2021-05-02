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

#define MAC_SAME_NAME(nam, req, flg, cme) int nam(void);
#define MAC_DIFF_NAME(nam, req, flg, fnc, cme)  int fnc(void);
#define MAC_SAME_FUNC(nam, req, flg, fnc, cme) 
#include "DefineScriptMacros.h"

 public:
  // COMMAND FUNCTIONS
#include "MacroMasterList.h"
 };


#endif  // MACROCOMMANDS_H
