// MacroProcessor.h: interface for the CMacroProcessor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_)
#define AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <afxtempl.h>
#include <set>
class COneLineScript;

#define MAX_LOOP_DEPTH  40
#define MAX_CALL_DEPTH  (2 * MAX_MACROS)
#define MAX_MACRO_LABELS 100
#define MAX_MACRO_SKIPS  100
#define MACRO_NO_VALUE  -123456789.

struct MacroFunction {
  CString name;
  int numNumericArgs;
  bool ifStringArg;
  int startIndex;
  bool wasCalled;
  CArray<CString, CString> argNames;
};

struct Variable {
  CString name;
  CString value;
  int type;        // Regular, persistent, index, or report
  int callLevel;   // call level at which variable defined
  int index;       // index with loop level for index, or for report, or script #
  MacroFunction *definingFunc;   // Function it was defined in
  int numElements;
};

struct CmdItem {
  const char *mixedCase;
  int minargs;
  int arithAllowed;
  std::string cmd;
};

class CMacroProcessor : public CCmdTarget
{
 public:
  void SetIntensityFactor(int iDir);
  void RunOrResume();
  void NextCommand();
  void Stop(BOOL ifNow, BOOL fromTSC = FALSE);
  void Resume();
  void Run(int which);
  void TaskDone(int param);
  int TaskBusy();
  void Initialize();
  CMacroProcessor();
  virtual ~CMacroProcessor();
  BOOL DoingMacro() {return mDoingMacro && mCurrentMacro >= 0;};
  BOOL IsResumable();
  void SetNonResumable() {mCurrentMacro = -1;};
  void SetMontageError(float inErr) {mLastMontError = inErr;};
  void SuspendMacro(BOOL abort = false);
  void AbortMacro();
  GetMember(BOOL, LastCompleted)
  GetMember(BOOL, LastAborted)
  GetMember(BOOL, OpenDE12Cover);
  GetSetMember(int, NumToolButtons);
  GetSetMember(int, ToolButHeight);
  GetSetMember(int, AutoIndentSize);
  SetMember(int, KeyPressed);
  GetSetMember(bool, WaitingForFrame);
  GetMember(bool, UsingContinuous);
  GetMember(bool, DisableAlignTrim);
  GetMember(bool, CompensatedBTforIS);
  GetMember(bool, NoMessageBoxOnError);
  bool GetAlignWholeTSOnly() {return DoingMacro() && mAlignWholeTSOnly;};
  bool SkipCheckingFrameAli() {return DoingMacro() && mSkipFrameAliCheck;};
  COneLineScript *mOneLineScript;

protected:

  // Generated message map functions
  //{{AFX_MSG(CMacroProcessor)
  afx_msg void OnMacroEnd();
  afx_msg void OnUpdateMacroStop(CCmdUI* pCmdUI);
  afx_msg void OnMacroStop();
  afx_msg void OnMacroResume();
  afx_msg void OnUpdateMacroResume(CCmdUI* pCmdUI);
  afx_msg void OnMacroControls();
  //}}AFX_MSG
  afx_msg void OnMacroEdit(UINT nID);
  afx_msg void OnUpdateMacroEdit(CCmdUI* pCmdUI);
  afx_msg void OnMacroRun(UINT nID);
  afx_msg void OnUpdateMacroRun(CCmdUI* pCmdUI);
  afx_msg void OnMacroToolbar();
  afx_msg void OnMacroSetlength();


  DECLARE_MESSAGE_MAP()

private:
  MacroControl * mControl;
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  CString * mModeNames;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CShiftManager *mShiftManager;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  ControlSet *mConSets;
  CMacroEditer **mMacroEditer;
  CString mModeCaps[5];
  CString *mMacNames;
  CString mStrNum[MAX_MACROS];
  CArray <MacroFunction *, MacroFunction *> mFuncArray[MAX_MACROS];
  std::set<std::string> mArithAllowed;
  std::set<std::string> mArithDenied;

  CString *mMacros;

  BOOL mDoingMacro;       // Flag for whether running
  int mCurrentMacro;      // Currently running macro
  int mCurrentIndex;      // Index into macro
  int mLoopStart[MAX_LOOP_DEPTH];         // Index of start of loop
  int mLoopLimit[MAX_LOOP_DEPTH];         // Times to do loop
  int mLoopCount[MAX_LOOP_DEPTH];         // Count (index) in loop, from 1
  int mLastIndex;         // Index of last command (the one just run)
  BOOL mStopAtEnd;        // Flag to stop at end of loop or macro
  BOOL mAskRedoOnResume;  // Flag to ask about redoing current command when resume
  int mNumRuns;           // Counter for number of runs
  float mLastMontError;   // Last Montage Error
  BOOL mTestScale;        // flag to test for image intensity
  BOOL mTestMontError;    // flag to test for montage error
  BOOL mTestTiltAngle;    // flag to test tilt angle
  CString mMacroPaneText; // Text to place in status pane
  float mAccumShiftX;     // Accumulated reported shifts
  float mAccumShiftY;
  float mAccumDiff;       // Accumulated difference
  DWORD mStartClock;      // Clock at start of macro
  double mIntensityFactor;  // Cosine factor for last tilt change
  CArray<Variable *, Variable *> mVarArray;     // Array of variable structures
  int mLoopLevel;         // Index for loop level, 0 in top loop or -1 if not in loop
  int mCallLevel;         // Index for call level, 0 in main macro
  int mLoopDepths[MAX_CALL_DEPTH];  // Loop level reached in current macro
  int mCallMacro[MAX_CALL_DEPTH];   // Current macro for given call level
  int mCallIndex[MAX_CALL_DEPTH];   // Current index into macro for given call level
  MacroFunction *mCallFunction[MAX_CALL_DEPTH];  // Pointer to function being called
  BOOL mAlreadyChecked[MAX_MACROS]; // Flag that this macro was already checked
  WINDOWPLACEMENT mToolPlacement;
  int mNumToolButtons;    // Number of tool bottons to show
  int mToolButHeight;     // Height of tool buttons
  WINDOWPLACEMENT mEditerPlacement;
  WINDOWPLACEMENT mOneLinePlacement;
  int mLogErrAction;      // Log argument for error messages
  int mLogAction;         // Parameterized log argument for suppressing reports
  std::vector<ControlSet> mConsetsSaved; // COntrol sets saved from changes
  std::vector<ControlSet> mChangedConsets; // Modified Control sets if suspend
  std::vector<std::string> mSavedSettingNames;   // Names and values of user settings that
  std::vector<double> mSavedSettingValues;       // were changed and need restoring
  std::vector<double> mNewSettingValues;         // and value sthey were changed to
  ShortVec mConsetNums;      // #'s of control sets saved
  int mCamWithChangedSets;   // Number of camera with changed sets on suspend
  double mSleepStart;  // Starting time to sleep from
  double mSleepTime;   // Number of ticks (msec) to sleep
  BOOL mMovedStage;       // Flag that stage should be checked
  BOOL mExposedFilm;   // Flag that film should be checked
  BOOL mMovedScreen;
  BOOL mStartedLongOp; // Flag that long operation was started
  BOOL mMovedPiezo;    // Flag that a piezo movement was started
  BOOL mLoadingMap;    // Flag that an asynchronous map load was started
  double mDoseStart;   // Starting cumulative dose
  double mDoseTarget;  // Target dose to accumulate
  double mDoseTime;    // Time it started
  double mDoseNextReport;  // Dose at which to report next
  int mNumDoseReports;  // Number of reports wanted
  BOOL mLastCompleted;  // Flag that last macro ran to completion
  BOOL mLastAborted;    // Flag that last macro stopped through the Abort call
  BOOL mInitialVerbose; // Initial verbose setting from menu
  int mVerbose;         // Run-time verbose setting
  CString mMailSubject;  // Subject line for email messages
  BOOL mOverwriteOK;      // Flag that overwriting an existing file is allowed
  CString mEnteredName;   // Filename entered from text box
  BOOL mOpenDE12Cover;    // Flag that DE-12 protection cover can be open
  int mNumStatesToRestore; // Number of states to restore when macro stops
  double mFocusToRestore; // Saved focus value;
  float mFocusOffsetToRestore;  // Saved value of autofocus offset to restore
  float mDEframeRateToRestore;  // Saved frame rate of DE camera
  int mDEcamIndToRestore; // Index of that camera
  double mBeamTiltXtoRestore[2];   // Saved beam tilt values to restore for two probe
  double mBeamTiltYtoRestore[2];   // modes
  bool mCompensatedBTforIS; // Flag that beam tilt was compensated for an IS change
  int mKeyPressed;        // Key pressed after macro starts
  int mStoppedContSetNum; // Set number for continuous acquire that was stopped
  float mMinDeltaFocus;   // Defocus change and absolute focus limits for autofocus
  float mMaxDeltaFocus;
  double mMinAbsFocus;
  double mMaxAbsFocus;
  bool mWaitingForFrame;  // Flag that we are waiting for next continuous frame
  bool mUsingContinuous;  // Flag to continue execution during continuous acquire
  CString mEmailOnError;  // Message to send when error message box goes up
  BOOL mLastTestResult;   // result of "Test" command
  int mOnStopMacroIndex;  // Macro number for function to call on stop
  int mOnStopLineIndex;   // And start index for function
  bool mExitAtFuncEnd;    // And flag to end when that function ends
  int mRetryReadOther;    // Number of retries for readOtherFile
  double mFrameWaitStart; // Time from which waiting for next frame starts
  CWinThread *mProcessThread;  // Thread for running a process
  bool mNoMessageBoxOnError;   // Flag for no message box on error
  CString mBoxOnScopeText;  // Text for a scope message box
  float mBoxOnScopeInterval;  // Interval in hours for message box
  int mBoxOnScopeType;
  int mShowedScopeBox;      // Flag so reply can be assigned to variable
  bool mLoopIndsAreLocal;      // Flag that loop indexes are local
  int mAutoIndentSize;         // Number of spaces for autoindent of macro
  bool mSkipFrameAliCheck;   // Flag for camera controller to skip checking frame ali param
  bool mAlignWholeTSOnly;    // Flag for alignment to happen is if in TS with Whole TS
  bool mStartNavAcqAtEnd;    // Flag to start Nav acquire on successful completion 
  int mTestNextMultiShot;    // 1 or 2 to test image area or coma
  bool mDisableAlignTrim;    // Flag to disable trimming in autoalign

public:
  void GetNextLine(CString * macro, int & currentIndex, CString &strLine);
  int ScanForName(int macroNumber, CString *macro = NULL);
  bool SetVariable(CString name, CString value, int type, int index, bool mustBeNew,
    CString *errStr = NULL);
  bool SetVariable(CString name, double value, int type, int index, bool mustBeNew,
    CString *errStr = NULL);
  Variable *LookupVariable(CString name, int &ind);
  void ClearVariables(int type = -1, int level = -1, int index = -1);
  int SubstituteVariables(CString * strItems, int maxItems, CString line);
  int EvaluateExpression(CString * strItems, int maxItems, CString line, int ifArray, 
    int &numItems, int &numOrig);
  int EvaluateArithmeticClause(CString * strItems, int maxItems, CString line, int &numItems);
  void SetReportedValues(double val1 = MACRO_NO_VALUE, double val2 = MACRO_NO_VALUE, 
    double val3 = MACRO_NO_VALUE, double val4 = MACRO_NO_VALUE, 
    double val5 = MACRO_NO_VALUE, double val6 = MACRO_NO_VALUE);
  void SetReportedValues(CString *strItems = NULL, double val1 = MACRO_NO_VALUE, double val2 = MACRO_NO_VALUE, 
    double val3 = MACRO_NO_VALUE, double val4 = MACRO_NO_VALUE, 
    double val5 = MACRO_NO_VALUE, double val6 = MACRO_NO_VALUE);
  void ToolbarMacroRun(UINT nID);
  WINDOWPLACEMENT * GetToolPlacement(void);
  WINDOWPLACEMENT * GetEditerPlacement(void) {return &mEditerPlacement;};
  WINDOWPLACEMENT * GetOneLinePlacement(void);
  void ToolbarClosing(void);
  void OneLineClosing(void);
  int CheckBlockNesting(int macroNum, int startLevel);
  int SkipToBlockEnd(int type, CString line);
  BOOL ItemToDouble(CString str, CString line, double & value);
  int EvaluateComparison(CString * strItems, int maxItems, CString line, BOOL &truth);
  int EvaluateIfStatement(CString * strItems, int maxItems, CString line, BOOL &truth);
  int EvaluateBooleanClause(CString * strItems, int maxItems, CString line, BOOL &truth);
  void ReplaceWithResult(double result, CString * strItems, int index, int & numItems, 
    int numDrop);
  void OpenMacroToolbar(void);
  void SetComplexPane(void);
  WINDOWPLACEMENT *FindEditerPlacement(void);
  BOOL MacroRunnable(int index);
  afx_msg void OnUpdateMacroEnd(CCmdUI *pCmdUI);
  int FindCalledMacro(CString strLine, bool scanning);
  void InsertDomacro(CString * strItem);
  BOOL WordIsReserved(CString str);
  int CheckIntensityChangeReturn(int err);
  int CheckConvertFilename(CString * strItems, CString strLine, int index, CString & strCopy);
  afx_msg void OnMacroVerbose();
  afx_msg void OnUpdateMacroVerbose(CCmdUI *pCmdUI);
  CmdItem * GetCommandList(int & numCommands);
  afx_msg void OnMacroEdit15();
  afx_msg void OnMacroEdit20();
  void PrepareForMacroChecking(int which);
  afx_msg void OnMacroReadMany();
  afx_msg void OnUpdateMacroReadMany(CCmdUI *pCmdUI);
  afx_msg void OnMacroWriteAll();
  afx_msg void OnUpdateMacroWriteAll(CCmdUI *pCmdUI);
  void OpenMacroEditor(int index);
  bool ConvertBufferLetter(CString strItem, int emptyDefault, bool checkImage, int &bufIndex, CString & message);
  bool CheckCameraBinning(double binDblIn, int &binning, CString &message);
  void SaveControlSet(int index);
  bool CheckAndConvertCameraSet(CString &strItem, int &itemInt, int &index, CString &message);
  bool RestoreCameraSet(int index, BOOL erase);
  int SkipToLabel(CString label, CString line, int &numPops);
  int CheckBalancedParentheses(CString * strItems, int maxItems, CString &strLine, CString &errmess);
  int SeparateParentheses(CString * strItems, int maxItems);
  void ClearFunctionArray(int index);
  MacroFunction * FindCalledFunction(CString strLine, bool scanning, int &macroNum, int &argInd, int currentMac = -1);
  void ScanMacroIfNeeded(int index, bool scanning);
  void TrimTrailingZeros(CString & str);
  int PiecesForMinimumSize(float minMicrons, int camSize, float fracOverlap);
  afx_msg void OnMacroListFunctions();
  int EnsureMacroRunnable(int macnum);
  void SendEmailIfNeeded(void);
  int TestAndStartFuncOnStop(void);
  int CheckForArrayAssignment(CString * strItems, int &firstInd);
  void FindValueAtIndex(Variable * var, int arrInd, int & beginInd, int & endInd);
  int ConvertArrayIndex(CString strItem, int leftInd, int rightInd, Variable * var, CString * errMess);
  static UINT RunInShellProc(LPVOID pParam);
  afx_msg void OnScriptSetIndentSize();
  afx_msg void OnScriptClearPersistentVars();
  afx_msg void OnUpdateClearPersistentVars(CCmdUI *pCmdUI);
  afx_msg void OnScriptRunOneCommand();
  afx_msg void OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI);
  int StartNavAvqBusy(void);
  int CheckLegalCommandAndArgNum(CString * strItems, CString strLine, int macroNum);
  bool ArithmeticIsAllowed(CString & str);
  int AdjustBeamTiltIfSelected(double delISX, double delISY, BOOL doAdjust, CString &message);
};

#endif // !defined(AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_)
