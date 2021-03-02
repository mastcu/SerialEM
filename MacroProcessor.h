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
#include <map>
#include <string>

class CMapDrawItem;
class COneLineScript;
struct ScriptLangPlugFuncs;

#define MAX_LOOP_DEPTH  40
#define MAX_CALL_DEPTH  (2 * MAX_TOT_MACROS)
#define MAX_MACRO_LABELS 100
#define MAX_MACRO_SKIPS  100
#define MACRO_NO_VALUE  -123456789.
#define MAX_CUSTOM_INTERVAL 20000 
#define MAX_MACRO_TOKENS 60
#define MAX_SCRIPT_LANG_ARGS  20

#define LOOP_LIMIT_FOR_TRY -2146000000
#define LOOP_LIMIT_FOR_IF  -2147000000

#define SCRIPT_EVENT_NAME  "SEMScriptLangEvent"

enum {VARTYPE_REGULAR, VARTYPE_PERSIST, VARTYPE_INDEX, VARTYPE_REPORT, VARTYPE_LOCAL};
enum {SKIPTO_ENDIF, SKIPTO_ELSE_ENDIF, SKIPTO_ENDLOOP, SKIPTO_CATCH, SKIPTO_ENDTRY};
enum {TXFILE_READ_ONLY, TXFILE_WRITE_ONLY, TXFILE_MUST_EXIST, TXFILE_MUST_NOT_EXIST,
  TXFILE_QUERY_ONLY};

#define MAC_SAME_NAME(nam, req, flg, cme) CME_##cme,
#define MAC_DIFF_NAME(nam, req, flg, fnc, cme) CME_##cme,
#define MAC_SAME_FUNC(nam, req, flg, fnc, cme) CME_##cme,
#include "DefineScriptMacros.h"

// An enum with indexes to true commands, preceded by special operations
enum {
#include "MacroMasterList.h"
};

#define CME_NOTFOUND -1

struct MacroFunction {
  CString name;
  int numNumericArgs;
  bool ifStringArg;
  int startIndex;
  bool wasCalled;
  CArray<CString, CString> argNames;
};

struct ArrayRow {
  CString value;
  int numElements;
};

struct Variable {
  CString name;
  CString value;
  int type;        // Regular, persistent, index, report, or local
  int callLevel;   // call level at which variable defined
  int index;       // index with loop level for index, or for report, or script #
  MacroFunction *definingFunc;   // Function it was defined in
  int numElements;  // Number of elements for a 1D array
  CArray<ArrayRow, ArrayRow> *rowsFor2d;    // Pointer to array of rows for 2D array
};

// For keeping track of open text files
struct FileForText {
  CStdioFile *csFile;   // Pointer to file
  BOOL readOnly;        // If read-only, otherwise write-only
  bool persistent;      // If kept open at end of script
  CString ID;           // Arbitrary ID
};

// For communicating with a scripting language
struct ScriptLangData {
  int functionCode;                        // Command code (index) from plugin
  CString strItems[MAX_SCRIPT_LANG_ARGS];  // String items from plugin (use from 1)
  BOOL itemEmpty[MAX_SCRIPT_LANG_ARGS];    // Flag if item empty, from plugin
  int itemInt[MAX_SCRIPT_LANG_ARGS];       // Integer value by atoi, from plugin
  double itemDbl[MAX_SCRIPT_LANG_ARGS];    // double value from plugin
  int lastNonEmptyInd;                     // Index of last non-empty item from plugin
  CString reportedStrs[6];                 // Reported string values after command
  double reportedVals[6];                  // Reported double values after command
  bool repValIsString[6];                  // Flag for whether reported value is string
  int highestReportInd;                    // Index of highest reported value (from 0)
  int errorOccurred;                       // Flag that an error occurred in the command
  int waitingForCommand;                   // Flag that SerialEM is waiting for command
  int commandReady;                        // Flag set by plugin that command is ready
  int threadDone;                          // Flag that script run thread has exited
  int exitStatus;                          // Exit status of script run thread
};

typedef int(CMacCmd::*DispEntry)(void);

struct CmdItem {
  const char *mixedCase;
  int minargs;
  int arithAllowed;
  DispEntry func;
  std::string cmd;
};

ScriptLangData DLL_IM_EX *SEMGetScriptLangData();

class DLL_IM_EX CMacroProcessor : public CCmdTarget
{
public:
  void SetIntensityFactor(int iDir);
  void RunOrResume();
  void Stop(BOOL ifNow);
  void Resume();
  void Run(int which);
  int TaskBusy();
  void Initialize();
  CMacroProcessor();
  virtual ~CMacroProcessor();
  BOOL DoingMacro() { return mDoingMacro && mCurrentMacro >= 0; };
  BOOL IsResumable();
  void SetNonResumable() { mCurrentMacro = -1; };
  void SetMontageError(float inErr) { mLastMontError = inErr; };
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
  GetMember(int, TryCatchLevel);
  GetMember(bool, SuspendNavRedraw);
  GetMember(bool, DeferLogUpdates);
  GetMember(bool, LoopInOnIdle);
  GetSetMember(BOOL, RestoreMacroEditors);
  int GetReadOnlyStart(int macNum) { return mReadOnlyStart[macNum]; };
  void SetReadOnlyStart(int macNum, int start) { mReadOnlyStart[macNum] = start; };
  std::map<std::string, int> *GetCustomTimeMap() { return &mCustomTimeMap; };
  bool GetAlignWholeTSOnly() { return DoingMacro() && mAlignWholeTSOnly; };
  bool SkipCheckingFrameAli() { return DoingMacro() && mSkipFrameAliCheck; };
  EMimageBuffer *ImBufForIndex(int ind) { return (ind >= 0 ? &mImBufs[ind] : &mFFTBufs[-1 - ind]); };
  COneLineScript *mOneLineScript;
  static ScriptLangData mScrpLangData;         // Data structure for external scripting
  static ScriptLangPlugFuncs *mScrpLangFuncs;  // The functions of a scripting plugin
  HANDLE mScrpLangDoneEvent;                   // EVent to notify plugin that command done

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

protected:
  MacroControl * mControl;
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  CString * mModeNames;
  EMimageBuffer *mImBufs;
  EMimageBuffer *mFFTBufs;
  CEMscope *mScope;
  CShiftManager *mShiftManager;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  CNavHelper *mNavHelper;
  CProcessImage *mProcessImage;
  CParameterIO *mParamIO;
  ControlSet *mConSets;
  CMacroEditer **mMacroEditer;
  CString mModeCaps[5];
  CString *mMacNames;
  CString mStrNum[MAX_MACROS];
  CArray <MacroFunction *, MacroFunction *> mFuncArray[MAX_TOT_MACROS];
  std::set<std::string> mArithAllowed;
  std::set<std::string> mArithDenied;
  std::map<unsigned int, int> mCmdHashMap;
  std::set<std::string> mFunctionSet1;
  std::set<std::string> mFunctionSet2;
  std::set<std::string> mReservedWords;
  std::map<std::string, int> mCustomTimeMap;
  int mReadOnlyStart[MAX_TOT_MACROS];

  CString *mMacros;
  CmdItem *mCmdList;
  int mNumCommands;
  CString mStrItems[MAX_MACRO_TOKENS];
  BOOL mItemEmpty[MAX_MACRO_TOKENS];
  int mItemInt[MAX_MACRO_TOKENS];
  double mItemDbl[MAX_MACRO_TOKENS];
  float mItemFlt[MAX_MACRO_TOKENS];

  BOOL mDoingMacro;       // Flag for whether running
  int mCurrentMacro;      // Currently running macro
  int mCurrentIndex;      // Index into macro
  int mLoopStart[MAX_LOOP_DEPTH];         // Index of start of loop
  int mLoopLimit[MAX_LOOP_DEPTH];         // Times to do loop or special values for IF/TRY
  int mLoopCount[MAX_LOOP_DEPTH];         // Count (index) in loop, from 1
  int mLoopIncrement[MAX_LOOP_DEPTH];     // Increment to add to index (negative OK)
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
  CArray<FileForText *, FileForText *> mTextFileArray;   // Array of open text files
  int mBlockLevel;         // Index for block level, 0 in top block or -1 if not in block
  int mCallLevel;          // Index for call level, 0 in main macro
  int mTryCatchLevel;      // Level of try blocks, raised at Try, dropped at Catch
  int mBlockDepths[MAX_CALL_DEPTH];  // Block level reached in current call level
  int mCallMacro[MAX_CALL_DEPTH];   // Current macro for given call level
  int mCallIndex[MAX_CALL_DEPTH];   // Current index into macro for given call level
  MacroFunction *mCallFunction[MAX_CALL_DEPTH];  // Pointer to function being called
  BOOL mAlreadyChecked[MAX_TOT_MACROS]; // Flag that this macro was already checked
  WINDOWPLACEMENT mToolPlacement;
  int mNumToolButtons;    // Number of tool bottons to show
  int mToolButHeight;     // Height of tool buttons
  WINDOWPLACEMENT mEditerPlacement[MAX_MACROS];
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
  bool mRestoreConsetAfterShot;   // Flag to restore a control set after a shot
  int mCropXafterShot, mCropYafterShot;   // Size to crop to after a shot
  std::vector<LowDoseParams> mLDParamsSaved;    // Saved low dose sets
  ShortVec mLDareasSaved;    // Numbers of areas saved
  short int mKeepOrRestoreArea[MAX_LOWDOSE_SETS];    // -1 to keep or 1 to restore an area
  double mSleepStart;  // Starting time to sleep from
  double mSleepTime;   // Number of ticks (msec) to sleep
  BOOL mMovedStage;       // Flag that stage should be checked
  BOOL mExposedFilm;   // Flag that film should be checked
  BOOL mMovedScreen;
  BOOL mStartedLongOp; // Flag that long operation was started
  BOOL mMovedPiezo;    // Flag that a piezo movement was started
  BOOL mMovedAperture; // Flag that an aperture/phase plate movement was started
  BOOL mLoadingMap;    // Flag that an asynchronous map load was started
  BOOL mMakingDualMap; // Flag that anchor map has been started
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
  int mLDSetAddedBeamRestore;   // Saved state of low dose SET function for beam shift
  double mBeamTiltXtoRestore[2];   // Saved beam tilt values to restore for two probe
  double mBeamTiltYtoRestore[2];   // modes
  double mAstigXtoRestore[2];   // Saved stigamtor values to restore for two probe
  double mAstigYtoRestore[2];   // modes
  int mK3CDSmodeToRestore;  // -1 or value of mode to restore at end
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
  BOOL mRestoreMacroEditors; // Flag to reopen the editor windows on startup/settings
  CString mNextProcessArgs;  // Argument string for next process to create
  int mRunToolArgPlacement;  // Whether to replace (0), prepend (-1), or append (1)
  int mNumTempMacros;        // Number of temporary macros assigned from script
  int mNeedClearTempMacro;   // Flag that the current temporary needs to be cleared
  bool mParseQuotes;         // Flag that strings are parsed with quoting 
  bool mSuspendNavRedraw;    // Flag to save redrawing of Nav table and display to end
  bool mDeferLogUpdates;     // Flag for log window to defer its updates and accumulate
  bool mLoopInOnIdle;        // Flag for OnIdle to keep calling TaskDone
  int mNumCmdSinceAddIdle;   // Number of commands run with looping in OnIdle
  double mLastAddIdleTime;   // Time of last call to AddIdleTask
  int mMaxCmdToLoopOnIdle;   // Maximum number of commands before returning to event loop
  float mMaxSecToLoopOnIdle; // Maximum number of seconds before doing so
  ControlSet *mCamSet;       // Control set, set by call to CheckAndConvertCameraSet

  bool mRunningScrpLang;     // Flag that external interpreter is running the script
  CString mMacroForScrpLang; // String actually passed with other scripts included
  CWinThread *mScrpLangThread;

public:
  void GetNextLine(CString * macro, int & currentIndex, CString &strLine, bool commentOK = false);
  int ScanForName(int macroNumber, CString *macro = NULL);
  bool SetVariable(CString name, CString value, int type, int index, bool mustBeNew,
    CString *errStr = NULL, CArray<ArrayRow, ArrayRow> *rowsFor2d = NULL);
  bool SetVariable(CString name, double value, int type, int index, bool mustBeNew,
    CString *errStr = NULL, CArray<ArrayRow, ArrayRow> *rowsFor2d = NULL);
  Variable *LookupVariable(CString name, int &ind);
  void ListVariables(int type = -1);
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
  WINDOWPLACEMENT * GetEditerPlacement(void) { return &mEditerPlacement[0]; };
  WINDOWPLACEMENT * GetOneLinePlacement(void);
  void ToolbarClosing(void);
  void OneLineClosing(void);
  int CheckBlockNesting(int macroNum, int startLevel, int &tryLevel);
  int SkipToBlockEnd(int type, CString line, int *numPops = NULL, int *delTryLevel = NULL);
  BOOL ItemToDouble(CString str, CString line, double & value);
  int EvaluateComparison(CString * strItems, int maxItems, CString line, BOOL &truth);
  int EvaluateIfStatement(CString * strItems, int maxItems, CString line, BOOL &truth);
  int EvaluateBooleanClause(CString * strItems, int maxItems, CString line, BOOL &truth);
  void ReplaceWithResult(double result, CString * strItems, int index, int & numItems,
    int numDrop);
  void OpenMacroToolbar(void);
  void SetComplexPane(void);
  WINDOWPLACEMENT *FindEditerPlacement(int index);
  BOOL MacroRunnable(int index);
  afx_msg void OnUpdateMacroEnd(CCmdUI *pCmdUI);
  int FindCalledMacro(CString strLine, bool scanning);
  int FindMacroByNameOrTextNum(CString name);
  void InsertDomacro(CString * strItem);
  BOOL WordIsReserved(CString str);
  int CheckIntensityChangeReturn(int err);
  int CheckConvertFilename(CString * strItems, CString strLine, int index, CString & strCopy);
  afx_msg void OnMacroVerbose();
  afx_msg void OnUpdateMacroVerbose(CCmdUI *pCmdUI);
  CmdItem * GetCommandList(int & numCommands);
  afx_msg void OnMacroEdit15();
  afx_msg void OnMacroEdit20();
  afx_msg void OnMacroEdit25();
  afx_msg void OnMacroEdit30();
  afx_msg void OnMacroEdit35();
  afx_msg void OnMacroEdit40();
  void PrepareForMacroChecking(int which);
  afx_msg void OnMacroReadMany();
  afx_msg void OnUpdateMacroReadMany(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNoTasks(CCmdUI *pCmdUI);
  void OpenMacroEditor(int index);
  bool ConvertBufferLetter(CString strItem, int emptyDefault, bool checkImage, int &bufIndex,
    CString & message, bool fftAllowed = false);
  bool CheckCameraBinning(double binDblIn, int &binning, CString &message);
  void SaveControlSet(int index);
  bool CheckAndConvertCameraSet(CString &strItem, int &itemInt, int &index, CString &message);
  bool RestoreCameraSet(int index, BOOL erase);
  int SkipToLabel(CString label, CString line, int &numPops, int &delTryLevel);
  void LeaveCallLevel(bool popBlocks);
  int CheckBalancedParentheses(CString * strItems, int maxItems, CString &strLine, CString &errmess);
  int SeparateParentheses(CString * strItems, int maxItems);
  void ClearFunctionArray(int index);
  MacroFunction * FindCalledFunction(CString strLine, bool scanning, int &macroNum, int &argInd, int currentMac = -1);
  void ScanMacroIfNeeded(int index, bool scanning);
  int PiecesForMinimumSize(float minMicrons, int camSize, float fracOverlap);
  afx_msg void OnMacroListFunctions();
  int EnsureMacroRunnable(int macnum);
  int CheckForScriptLanguage(int macNum);
  void SendEmailIfNeeded(void);
  int TestAndStartFuncOnStop(void);
  int TestTryLevelAndSkip(CString *mess);
  int CheckForArrayAssignment(CString * strItems, int &firstInd);
  void FindValueAtIndex(CString &value, int arrInd, int & beginInd, int & endInd);
  int ConvertArrayIndex(CString strItem, int leftInd, int rightInd, CString name, int numElements,
    CString * errMess);
  static UINT RunInShellProc(LPVOID pParam);
  static UINT RunScriptLangProc(LPVOID pParam);
  afx_msg void OnScriptSetIndentSize();
  afx_msg void OnScriptListPersistentVars();
  afx_msg void OnScriptClearPersistentVars();
  afx_msg void OnUpdateClearPersistentVars(CCmdUI *pCmdUI);
  afx_msg void OnScriptRunOneCommand();
  afx_msg void OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI);
  int StartNavAvqBusy(void);
  int CheckLegalCommandAndArgNum(CString * strItems, int cmdIndex, CString strLine, int macroNum);
  bool ArithmeticIsAllowed(CString & str);
  int AdjustBeamTiltIfSelected(double delISX, double delISY, BOOL doAdjust, CString &message);
  int AdjustBTApplyISSetDelay(double delISX, double delISY, BOOL doAdjust, BOOL setDelay, double scale, CString &message);
  afx_msg void OnOpenEditorsOnStart();
  afx_msg void OnUpdateOpenEditorsOnStart(CCmdUI *pCmdUI);
  void TransferOneLiners(bool fromDialog);
  void OpenOrJustCloseOneLiners(bool reopen);
  unsigned int StringHashValue(const char * str);
  int LookupCommandIndex(CString & item);
  int LocalVarAlreadyDefined(CString & item, CString &strLine);
  int FindAndCheckArrayIndexes(CString & item, int leftIndex, int & right1, int & right2,
    CString *errStr);
  int EvalExpressionInIndex(CString & indStr);
  Variable * GetVariableValuePointers(CString & name, CString ** valPtr, int ** numElemPtr,
    const char *action, CString & errStr);
  void SetOneReportedValue(CString * strItem, CString * ValtStr, double value, int index);
  void SetOneReportedValue(CString &valStr, int index);
  void SetOneReportedValue(double value, int index);
  void SetOneReportedValue(CString *strItem, CString &valStr, int index);
  void SetOneReportedValue(CString *strItem, double value, int index);
  FileForText *LookupFileForText(CString & ID, int checkReadOrWrite, CString &strLine, int &index);
  void CloseFileForText(int index);
  void SubstituteLineStripItems(CString & strLine, int numStrip, CString & strCopy);
  int CheckAndConvertLDAreaLetter(CString & item, int needOnOrOff, int & index, CString &strLine);
  void RestoreLowDoseParams(int index);
  bool IsLowDoseAreaSaved(int which);
  void UpdateLDAreaIfSaved();
  int MakeNewTempMacro(CString &strVar, CString &strIndex, bool tempOnly, CString &strLine);
  bool SetupStageRestoreAfterTilt(CString * strItems, double &stageX, double &stageY);
  CMapDrawItem *CurrentOrIndexedNavItem(int &index, CString &strLine);
  float * FloatArrayFromVariable(CString name, int &numVals, CString & report);
  afx_msg void OnScriptLoadNewPackage();
  void UpdateAllForNewScripts(bool oneLinersToo);
  afx_msg void OnScriptSavePackage();
  afx_msg void OnScriptSavePackageAs();
  afx_msg void OnRunOnProgramStart();
  afx_msg void OnUpdateRunOnProgramStart(CCmdUI *pCmdUI);
  afx_msg void OnRunAtProgramEnd();
  afx_msg void OnUpdateRunAtProgramEnd(CCmdUI *pCmdUI);
  int SelectScriptAtStartEnd(CString &name, const char *when);
};

#include "MacroCommands.h"

#endif // !defined(AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_)
