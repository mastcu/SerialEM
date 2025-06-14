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
struct MultiShotParams;

#define MAX_LOOP_DEPTH  40
#define MAX_CALL_DEPTH  80
#define MAX_PY_TRY_DEPTH 20
#define MAX_MACRO_LABELS 100
#define MAX_MACRO_SKIPS  100
#define MACRO_NO_VALUE  -123456789.
#define MAX_CUSTOM_INTERVAL 20000 
#define MAX_MACRO_TOKENS 60
#define MAX_SCRIPT_LANG_ARGS  20

#define LOOP_LIMIT_FOR_TRY -2146000000
#define LOOP_LIMIT_FOR_IF  -2147000000

#define SCRIPT_EVENT_NAME  "SEMScriptLangEvent"
#define SCRIPT_NORMAL_EXIT  -123456
#define SCRIPT_EXIT_NO_EXC  -654321
#define SCRIPT_USER_STOP    -234561
#define VARTYPE_ADD_FOR_NUM  10

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
CME_ENUM_LENGTH
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
  bool isNumeric;  // True if the value was originally numeric
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
  int commandReady;                        // Flag set by plugin that command is ready
  bool gotExceptionText;                   // Flag that strItems has exception text

  // Variables used only by SerialEM
  int waitingForCommand;                   // Flag that SerialEM is waiting for command
  int threadDone;                          // Flag that script run thread has exited
  int exitStatus;                          // Exit status of script run thread
  bool exitedFromWrapper;                  // Flag that exit was called from exception
  int externalControl;                     // -1 to request it, 0 if not, 1 if under it
  bool disconnected;                       // Flag that the socket disconnected
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
  void StartRunningScrpLang(void);
  void Stop(BOOL ifNow);
  void Resume();
  void Run(int which);
  void ClearGraphVariables();
  int TaskBusy();
  void CleanupExternalProcess();
  void CheckAndSetupExternalControl(void);
  void Initialize();
  CMacroProcessor();
  virtual ~CMacroProcessor();
  void ShutdownScrpLangScripting();
  BOOL DoingMacro() { return mDoingMacro && mCurrentMacro >= 0; };
  BOOL IsResumable();
  void SetNonResumable() { mCurrentMacro = -1; };
  void SetMontageError(float inErr) { mLastMontError = inErr; };
  void SuspendMacro(int abort = 0);
  void AbortMacro(bool ending = false);
  GetMember(BOOL, LastCompleted)
    GetMember(BOOL, LastAborted)
    GetMember(BOOL, OpenDE12Cover);
  GetSetMember(int, NumToolButtons);
  GetSetMember(int, ToolButHeight);
  GetMember(int, NumCamMacRows);
  GetMember(int, NumStatusLines);
  GetSetMember(int, AutoIndentSize);
  GetMember(BOOL, UseMonoFont);
  void SetUseMonoFont(BOOL inVal);
  GetSetMember(BOOL, ShowIndentButtons);
  GetSetMember(CString, MonoFontName);
  CString *GetStatusLines() {return &mStatusLines[0]; };
  bool *GetHighlightStatus() { return &mHighlightStatus[0]; };
  GetSetMember(BOOL, MonospaceStatus);
  SetMember(int, KeyPressed);
  GetSetMember(bool, WaitingForFrame);
  GetMember(bool, UsingContinuous);
  GetMember(bool, DisableAlignTrim);
  GetMember(bool, CompensatedBTforIS);
  GetMember(bool, NoMessageBoxOnError);
  GetMember(int, TryCatchLevel);
  GetMember(int, PythonTryLevel);
  GetMember(bool, SuspendNavRedraw);
  GetMember(bool, DeferLogUpdates);
  GetMember(bool, NonMacroDeferring);
  GetMember(float, CumulRecordDose);
  GetMember(bool, C2ApForScalingWasSet);
  void SetNonMacroDeferLog(bool inVal) { mDeferLogUpdates = inVal; mNonMacroDeferring = inVal; };

  GetMember(bool, LoopInOnIdle);
  GetMember(bool, RunningScrpLang);
  GetMember(int, LastPythonErrorLine);
  GetSetMember(BOOL, RestoreMacroEditors);
  SetMember(CString, PyModulePath);
  GetMember(CString, EnteredName);
  SetMember(HWND, FocusedWndWhenSavedStatus);
  GetSetMember(BOOL, KeepOneLineFocus);
  GetSetMember(CString, PyIncludePath);
  GetSetMember(int, NextParamSetForMont);
  GetMember(CString, ScriptWindowTitle);
  GetMember(CString *, FKeyMapping);
  bool *GetNoCatchOutput() { return &mNoCatchOutput[0]; };
  bool *GetNoPyTryOutput() { return &mNoPyTryOutput[0]; };
  std::vector<std::string> *GetVersionsOfPython() { return &mVersionsOfPython; };
  int GetReadOnlyStart(int macNum) { return mReadOnlyStart[macNum]; };
  void SetReadOnlyStart(int macNum, int start) { mReadOnlyStart[macNum] = start; };
  std::map<std::string, int> *GetCustomTimeMap() { return &mCustomTimeMap; };
  bool GetAlignWholeTSOnly() { return DoingMacro() && mAlignWholeTSOnly; };
  bool SkipCheckingFrameAli() { return DoingMacro() && mSkipFrameAliCheck; };
  EMimageBuffer *ImBufForIndex(int ind) { return (ind >= 0 ? &mImBufs[ind] : &mFFTBufs[-1 - ind]); };
  IntVec *GetStoredGraphTypes() { return &mStoredGraphTypes; };
  std::vector<FloatVec> *GetStoredGraphValues() { return &mStoredGraphValues; };
  COneLineScript *mOneLineScript;
  static ScriptLangData mScrpLangData;         // Data structure for external scripting
  static ScriptLangPlugFuncs *mScrpLangFuncs;  // The functions of a scripting plugin
  static HANDLE mScrpLangDoneEvent;            // Event to notify server thread command done
  static std::set<int> mPythonOnlyCmdSet;      // Set of commands available only from Python
  static HANDLE mPyProcessHandle;              // Process handle so we can kill it

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
  afx_msg void OnMacroFKeyRun(UINT nID);
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
  CFocusManager *mFocusManager;
  CParameterIO *mParamIO;
  CSerialEMDoc *mDocWnd;
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
  int mPythonTryLevel;     // Level for StartTry/EndTry from other script language
  bool mInInitialSubEval;  // Flag to test if in a block-raising statement if error
  bool mNoCatchOutput[MAX_LOOP_DEPTH];  // Flag to suppress output, index 0 for level 1
  bool mNoPyTryOutput[MAX_PY_TRY_DEPTH];  // Block level reached in current call level
  int mBlockDepths[MAX_CALL_DEPTH];  // Block level reached in current call level
  int mCallMacro[MAX_CALL_DEPTH];   // Current macro for given call level
  int mCallIndex[MAX_CALL_DEPTH];   // Current index into macro for given call level
  MacroFunction *mCallFunction[MAX_CALL_DEPTH];  // Pointer to function being called
  BOOL mAlreadyChecked[MAX_TOT_MACROS]; // Flag that this macro was already checked
  WINDOWPLACEMENT mToolPlacement;
  int mNumToolButtons;    // Number of tool bottons to show
  int mToolButHeight;     // Height of tool buttons
  int mNumCamMacRows;     // Number of rows of buttons in camera-macro tool panel
  int mNumStatusLines;   // Number of message lines in camera-macro tool panel
  WINDOWPLACEMENT mEditerPlacement[MAX_MACROS];
  WINDOWPLACEMENT mOneLinePlacement;
  int mLogErrAction;      // Log argument for error messages
  int mLogAction;         // Parameterized log argument for suppressing reports
  int mLogInfoAction;     // Log argument for info messages
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
  bool mOpenValvesAfterSleep; // Flag to reopen valves after sleep expires
  bool mWaitingForMarker; //  Flag that we are waiting for a new marker point
  BOOL mStartedWithMarker;  // Flag that there was a marker on that image already
  float mOldMarkerX, mOldMarkerY;   // Starting value if any
  double mMarkerTimeStamp;  // Time stamp of image
  int mMarkerImageCapFlag;  // mCaptured value of image too
  BOOL mMovedStage;       // Flag that stage should be checked
  BOOL mExposedFilm;   // Flag that film should be checked
  BOOL mMovedScreen;
  BOOL mStartedLongOp; // Flag that long operation was started
  BOOL mRanGatanScript; // Flag that a DM script was started
  BOOL mRanCtfplotter;  // Flag that ctfplotter was run
  BOOL mRanExtProcess;  // Flag that 
  BOOL mMovedPiezo;    // Flag that a piezo movement was started
  BOOL mMovedAperture; // Flag that an aperture/phase plate movement was started
  BOOL mLoadingMap;    // Flag that an asynchronous map load was started
  BOOL mMakingDualMap; // Flag that anchor map has been started
  BOOL mAutoContouring; // Flag that autocontouring was started
  int mStartedRefineZLP; // Flag that refine ZLP was started: 1 normal, -1 allow failure
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
  float mGridLimitsToRestore[4];   // User grid limits to restore at end
  bool mRestoreGridLimits;
  FileOptions *mFileOptToRestore;   // Main file options to restore at the end
  FileOptions *mOtherFileOptToRestore;  // Other file options to restore at the end
  int mSavedFrameNameFormat;   // set >=0 if this needs to be restored
  CString mSavedFrameBaseName; // It needs to be restored also if non-empty
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
  BOOL mUseMonoFont;         // setting to use monospaced font if possible
  CString mMonoFontName;     // Priority font to use
  BOOL mShowIndentButtons;   // setting for whether editor shows indent buttons
  bool mSkipFrameAliCheck;   // Flag for camera controller to skip checking frame ali param
  bool mAlignWholeTSOnly;    // Flag for alignment to happen is if in TS with Whole TS
  bool mStartNavAcqAtEnd;    // Flag to start Nav acquire on successful completion 
  bool mUseTempNavParams;    // Flag to do that with temp params at index 2;
  CString mPackToLoadAtEnd;  // Name of script package to load at end if successful
  bool mSaveCurrentPack;     // Flag to save current package first
  int mTestNextMultiShot;    // 1 or 2 to test image area or coma
  bool mDisableAlignTrim;    // Flag to disable trimming in autoalign
  BOOL mRestoreMacroEditors; // Flag to reopen the editor windows on startup/settings
  CString mNextProcessArgs;  // Argument string for next process to create
  int mRunToolArgPlacement;  // Whether to replace (0), prepend (-1), or append (1)
  int mNumTempMacros;        // Number of temporary macros assigned from script
  int mNeedClearTempMacro;   // Flag that the current temporary needs to be cleared
  int mClearTempAtCallLevel; // Call level at which to clear temp macro
  bool mParseQuotes;         // Flag that strings are parsed with quoting
  bool mCheckingParseQuotes; // Flag it is happening during checking
  bool mSuspendNavRedraw;    // Flag to save redrawing of Nav table and display to end
  bool mDeferLogUpdates;     // Flag for log window to defer its updates and accumulate
  bool mNonMacroDeferring;   // Flag that some other module is deferring
  bool mDeferSettingsUpdate; // Flag not to update windows after SetUserSetting
  bool mLoopInOnIdle;        // Flag for OnIdle to keep calling TaskDone
  int mNumCmdSinceAddIdle;   // Number of commands run with looping in OnIdle
  double mLastAddIdleTime;   // Time of last call to AddIdleTask
  int mMaxCmdToLoopOnIdle;   // Maximum number of commands before returning to event loop
  float mMaxSecToLoopOnIdle; // Maximum number of seconds before doing so
  ControlSet *mCamSet;       // Control set, set by call to CheckAndConvertCameraSet
  bool mNoLineWrapInMessageBox;  // Flag for messages boxes without line wrap
  bool mMonospacedMessageBox; // Flag to use monospaced font

  bool mRunningScrpLang;     // Flag that external interpreter is running the script
  bool mCalledFromScrpLang;  // Flag that regular script called from language script
  bool mCalledFromSEMmacro;  // Flag that language script called from regular one
  CString mMacroForScrpLang; // String actually passed with other scripts included
  static CWinThread *mScrpLangThread;
  IntVec mLineInSrcMacro;      // Map from line in composite script to line in source
  IntVec mIndexOfSrcLine;      // And the character index of that line in the source
  IntVec mMacNumAtScrpLine;    // Macro number for each block of composite script
  IntVec mMacStartLineInScrp;  // And the starting line of that block in the composite
  IntVec mFirstRealLineInPart; // Macro line number of first non-blank line in a block
  int mLastPythonErrorLine;    // Last line number and error happened on
  std::vector<std::string> mIncludedFiles;
  std::vector<std::string> mPathsToPython;
  std::vector<std::string> mVersionsOfPython;
  CString mPyModulePath;       // Module path set by property or set as default
  int mScriptNumFound;         // Last script number (0-based) for FindScriptByName
  float mCumulRecordDose;      // Cumulative Record dose for tilt series
  FloatVec mAreaRecordDoses;   // Record doses for separate areas
  int mAreaForCumulDose;       // Current area index
  ImodImageFile *mShrMemFile;  // Shared memory file for a process
  DWORD mExtProcExitStatus;    // Exit status from external process run sychronously
  IntVec mGraphTypeList;       // Entered type list for graphs
  IntVec mGraphColumns;        // Columns to graph
  IntVec mGraphSymbols;        // Symbol numbers
  CString mGraphAxisLabel;     // Bottom axis  label
  std::vector<std::string> mGraphKeys;   // Key labels
  bool mConnectGraph;          // Flag to connect
  int mGraphVsOrdinals;        // Flag to do ordinals
  int mGraphColorOpt;          // Use colors, except black for given group #
  int mGraphXlogSqrt;          // 1 or 2 for log/sqrt for X
  float mGraphXlogBase;        // Base to add for log/sqrt
  int mGraphYlogSqrt;
  float mGraphYlogBase;
  float mGraphXmin;            // MIn and max in X and Y
  float mGraphXmax;
  float mGraphYmin;
  float mGraphYmax;
  CString mGraphSaveName;       // Name of file to save and exit
  bool mGraphSaveTiff;         // Whether to save tiff instead of png
  IntVec mStoredGraphTypes;    // Stored type data
  std::vector<FloatVec> mStoredGraphValues;    // Stored values from last graph or others
  CString mInputToNextProcess; // Input to pipe in when running next process
  CString mVarForProcOutputPipe;  // Name of variable to place output in
  int mGotPipedOutputOrErr;    // 1 if got output, -1 for impossible error, 2 exitcode
  CString mPipeOutput;         // A string just for this
  HWND mFocusedWndWhenSavedStatus;
  CString mSavedStatusPanes[3];
  CString mStatusLines[NUM_CM_MESSAGE_LINES];
  bool mHighlightStatus[NUM_CM_MESSAGE_LINES];
  BOOL mMonospaceStatus;
  BOOL mKeepOneLineFocus;
  CString mPyIncludePath;
  int mSaveCtfplotGraph;       // 1 for png, 2 for tiff to save next ctfplotter graph
  int mBufForCtfplotGraph;     // Buffer to load into when gotten (-1 not to)
  CString mCtfplotGraphName;   // FIlename to save next ctfplotter graph to
  CString mLastPluginCalled;   // Keep track so it is easier to call again with ":"
  int mNextParamSetForMont;    // Parameter set to use for next montage opened
  bool mRamperStarted;         // Flag for focus ramper started
  CString mScriptWindowTitle;  // Title for window opened by script
  float mBinningForCameraMatrix; // Adjust camera matrices to this user binning
  MultiShotParams *mSavedMultiShot;  // Saved parameters for running step & adjust
  bool mC2ApForScalingWasSet;  // Flag that C2 aperture size was set
  CString mFKeyMapping[10];    // Custom mapping to actual macros

public:
  void SetNumCamMacRows(int inVal);
  void SetNumStatusLines(int inVal);
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
  void SetReportedValues(double val1 = EXTRA_NO_VALUE, double val2 = EXTRA_NO_VALUE,
    double val3 = EXTRA_NO_VALUE, double val4 = EXTRA_NO_VALUE,
    double val5 = EXTRA_NO_VALUE, double val6 = EXTRA_NO_VALUE);
  void SetReportedValues(CString *strItems = NULL, double val1 = EXTRA_NO_VALUE, double val2 = EXTRA_NO_VALUE,
    double val3 = EXTRA_NO_VALUE, double val4 = EXTRA_NO_VALUE,
    double val5 = EXTRA_NO_VALUE, double val6 = EXTRA_NO_VALUE);
  void SetRepValsAndVars(int firstInd, double val1 = EXTRA_NO_VALUE, double val2 = EXTRA_NO_VALUE,
    double val3 = EXTRA_NO_VALUE, double val4 = EXTRA_NO_VALUE,
    double val5 = EXTRA_NO_VALUE, double val6 = EXTRA_NO_VALUE);
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
    int numDrop, CString format = "");
  void OpenMacroToolbar(void);
  void SetComplexPane(void);
  WINDOWPLACEMENT *FindEditerPlacement(int index);
  BOOL MacroRunnable(int index);
  afx_msg void OnUpdateMacroEnd(CCmdUI *pCmdUI);
  int FindCalledMacro(CString strLine, bool scanning, CString useName = "");
  int FindMacroByNameOrTextNum(CString name);
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
  afx_msg void OnMacroEdit45();
  afx_msg void OnMacroEdit50();
  afx_msg void OnMacroEdit55();
  afx_msg void OnMacroEdit60();
  void PrepareForMacroChecking(int which);
  afx_msg void OnMacroReadMany();
  afx_msg void OnUpdateMacroReadMany(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNoTasks(CCmdUI *pCmdUI);
  void OpenMacroEditor(int index);
  bool ConvertBufferLetter(CString strItem, int emptyDefault, bool checkImage, int &bufIndex,
    CString & message, bool fftAllowed = false);
  void ConvertApertureNameToNum();
  bool CheckCameraBinning(double binDblIn, int &binning, CString &message);
  void SaveControlSet(int index);
  bool CheckAndConvertCameraSet(CString &strItem, int &itemInt, int &index, CString &message);
  bool RestoreCameraSet(int index, BOOL erase);
  void FormatCartridgeInfo(int index, int &id, int &station, int &slot, int &type, int &rotation,
    CString &name, CString &report, CString &rowVal);
  int SkipToLabel(CString label, CString line, int &numPops, int &delTryLevel);
  void LeaveCallLevel(bool popBlocks);
  int CheckBalancedParentheses(CString * strItems, int maxItems, CString &strLine, 
    CString &errmess, bool useQuotes);
  int SeparateParentheses(CString * strItems, int maxItems, bool useQuotes);
  void ClearFunctionArray(int index);
  MacroFunction * FindCalledFunction(CString strLine, bool scanning, int &macroNum, int &argInd, int currentMac = -1);
  void ScanMacroIfNeeded(int index, bool scanning);
  int PiecesForMinimumSize(float minMicrons, int camSize, float fracOverlap);
  afx_msg void OnMacroListFunctions();
  int EnsureMacroRunnable(int macnum);
  int CheckForScriptLanguage(int macNum, bool justCheckStart, int argInd = 0, int currentInd = 0, int lastInd = -1,
    int startLine = 0);
  void IndentAndAppendToScript(CString &source, CString &copy, CString &indentStr, bool isPython);
  void DoReplacementsInPythonLine(CString &line);
  bool IsEmbeddedPythonOK(int macNum, int currentInd, int &lastInd, int &newCurrentInd);
  int CountLinesToCurIndex(int macNum, int curIndex);
  void EnhancedExceptionToLog(CString &str);
  void SetPathToPython(CString &version, CString &path);
  void SendEmailIfNeeded(void);
  int TestAndStartFuncOnStop(void);
  int TestTryLevelAndSkip(CString *mess);
  int CheckForArrayAssignment(CString * strItems, int &firstInd);
  void FindValueAtIndex(CString &value, int arrInd, int & beginInd, int & endInd);
  int ConvertArrayIndex(CString strItem, int leftInd, int rightInd, CString name, int numElements,
    CString * errMess);
  void FillVectorFromArrayVariable(FloatVec *fvec, IntVec *ivec, Variable *var);
  static UINT RunInShellProc(LPVOID pParam);
  static UINT RunScriptLangProc(LPVOID pParam);
  static int CreateOnePipe(HANDLE *childRd, HANDLE *childWr, SECURITY_ATTRIBUTES *saAttr, bool setForWrite,
    const char *descrip);
  static UINT StdoutToLogProc(LPVOID pParam);
  static void TerminateScrpLangProcess(void);
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
  bool AdjustBeamTiltAndAstig(double delISX, double delISY, double &BTX, double &BTY, double &astigX, double &astigY);
  int AdjustBTApplyISSetDelay(double delISX, double delISY, BOOL doAdjust, BOOL setDelay, double scale, CString &message);
  int TestIncrementalImageShift(double delISX, double delISY);
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
  int RunScriptFromFile(CString &filename, bool deleteFile, CString &mess);
  void SubstituteLineStripItems(CString & strLine, int numStrip, CString & strCopy);
  void JustStripItems(CString & strLine, int numStrip, CString & strCopy, bool allowComment = false);
  int CheckAndConvertLDAreaLetter(CString & item, int needOnOrOff, int & index, CString &strLine);
  void RestoreLowDoseParams(int index);
  void RestoreMultiShotParams();
  bool IsLowDoseAreaSaved(int which);
  void UpdateLDAreaIfSaved();
  void GetAutocontourParams(int fi, float &target, float &minSize,
    float &maxSize, float &relThresh, float &absThresh, BOOL &usePoly);
  int MakeNewTempMacro(CString &strVar, CString &strIndex, bool tempOnly, CString &strLine);
  bool SetupStageRestoreAfterTilt(CString * strItems, double &stageX, double &stageY);
  int DoStageRelaxation(double delx);
  CMapDrawItem *CurrentOrIndexedNavItem(int &index, CString &strLine);
  float * FloatArrayFromVariable(CString name, int &numVals, CString & report);
  int GetPairedFloatArrays(int itemInd, float **xArray, float **yArray, int &numVals, CString & report);
  void SetGraphListVec(IntVec &graphList);
  afx_msg void OnScriptLoadNewPackage();
  int LoadNewScriptPackage(CString &filename, bool saveCurrent);
  void UpdateAllForNewScripts(bool oneLinersToo);
  afx_msg void OnScriptSavePackage();
  afx_msg void OnScriptSavePackageAs();
  afx_msg void OnRunOnProgramStart();
  afx_msg void OnUpdateRunOnProgramStart(CCmdUI *pCmdUI);
  afx_msg void OnRunAtProgramEnd();
  afx_msg void OnUpdateRunAtProgramEnd(CCmdUI *pCmdUI);
  int SelectScriptAtStartEnd(CString &name, const char *when);
  void SaveStatusPanes(int macNum);
  afx_msg void OnUseMonospacedFont();
  afx_msg void OnUpdateUseMonospacedFont(CCmdUI *pCmdUI);
  afx_msg void OntShowIndentButtons();
  afx_msg void OnUpdateShowIndentButtons(CCmdUI *pCmdUI);
  afx_msg void OnScriptSetpanelrows();
  afx_msg void OnUpdateScriptSetpanelrows(CCmdUI *pCmdUI);
  afx_msg void OnRunSerialemSnapshot();
  afx_msg void OnUpdateRunSerialemSnapshot(CCmdUI *pCmdUI);
  afx_msg void OnRunIfProgramIdle();
  afx_msg void OnUpdateRunIfProgramIdle(CCmdUI *pCmdUI);
  afx_msg void OnScriptSetNumStatus();
  afx_msg void OnMonospaceStatusLines();
  afx_msg void OnUpdateMonospaceStatusLines(CCmdUI *pCmdUI);
  afx_msg void OnKeepFocusOnOneLine();
  afx_msg void OnUpdateKeepFocusOnOneLine(CCmdUI *pCmdUI);
  afx_msg void OnScriptMapFunctionKey();
  afx_msg void OnScriptListFKeyMappings();
};

#include "MacroCommands.h"

#endif // !defined(AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_)
