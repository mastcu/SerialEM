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

#define MAX_LOOP_DEPTH  40
#define MAX_CALL_DEPTH  (2 * MAX_TOT_MACROS)
#define MAX_MACRO_LABELS 100
#define MAX_MACRO_SKIPS  100
#define MACRO_NO_VALUE  -123456789.
#define MAX_CUSTOM_INTERVAL 20000 
#define MAX_MACRO_TOKENS 60

#define LOOP_LIMIT_FOR_TRY -2146000000
#define LOOP_LIMIT_FOR_IF  -2147000000

enum {VARTYPE_REGULAR, VARTYPE_PERSIST, VARTYPE_INDEX, VARTYPE_REPORT, VARTYPE_LOCAL};
enum {SKIPTO_ENDIF, SKIPTO_ELSE_ENDIF, SKIPTO_ENDLOOP, SKIPTO_CATCH, SKIPTO_ENDTRY};
enum {TXFILE_READ_ONLY, TXFILE_WRITE_ONLY, TXFILE_MUST_EXIST, TXFILE_MUST_NOT_EXIST,
  TXFILE_QUERY_ONLY};

// An enum with indexes to true commands, preceded by special operations that need to be
// processed with a case within the big switch.  Be sure to adjust the starting number
// so VIEW stays at 0
enum {CME_SCRIPTEND = 0, CME_LABEL, CME_SETVARIABLE, CME_SETSTRINGVAR, CME_DOKEYBREAK,
      CME_ZEROLOOPELSEIF, CME_NOTFOUND,
  CME_VIEW, CME_FOCUS, CME_TRIAL, CME_RECORD, CME_PREVIEW,
  CME_A, CME_ALIGNTO, CME_AUTOALIGN, CME_AUTOFOCUS, CME_BREAK, CME_CALL,
  CME_CALLMACRO, CME_CENTERBEAMFROMIMAGE, CME_CHANGEENERGYLOSS, CME_CHANGEFOCUS,
  CME_CHANGEINTENSITYBY, CME_CHANGEMAG, CME_CIRCLEFROMPOINTS, CME_CLEARALIGNMENT,
  CME_CLOSEFILE, CME_CONICALALIGNTO, CME_CONTINUE, CME_COPY, CME_D, CME_DELAY,
  CME_DOMACRO, CME_ECHO, CME_ELSE, CME_ENDIF, CME_ENDLOOP, CME_EUCENTRICITY,
  CME_EXPOSEFILM, CME_F, CME_G, CME_GOTOLOWDOSEAREA, CME_IF, CME_IMAGESHIFTBYMICRONS,
  CME_IMAGESHIFTBYPIXELS, CME_IMAGESHIFTBYUNITS, CME_INCPERCENTC2, CME_L, CME_LOOP,
  CME_M, CME_MACRONAME, CME_LONGNAME, CME_MANUALFILMEXPOSURE, CME_MONTAGE, CME_MOVESTAGE,
  CME_MOVESTAGETO, CME_NEWMAP, CME_OPENNEWFILE, CME_OPENNEWMONTAGE, CME_PAUSE,
  CME_QUADRANTMEANS, CME_R, CME_READFILE, CME_REALIGNTONAVITEM, CME_REALIGNTOOTHERITEM,
  CME_RECORDANDTILTDOWN, CME_RECORDANDTILTUP, CME_REFINEZLP, CME_REPEAT,
  CME_REPORTACCUMSHIFT, CME_REPORTALIGNSHIFT, CME_REPORTBEAMSHIFT, CME_REPORTCLOCK,
  CME_REPORTDEFOCUS, CME_REPORTFOCUS, CME_REPORTMAG, CME_REPORTMAGINDEX,
  CME_REPORTMEANCOUNTS, CME_REPORTNAVITEM, CME_REPORTOTHERITEM, CME_REPORTPERCENTC2,
  CME_REPORTSHIFTDIFFFROM, CME_REPORTSPOTSIZE, CME_REPORTSTAGEXYZ, CME_REPORTTILTANGLE,
  CME_RESETACCUMSHIFT, CME_RESETCLOCK, CME_RESETIMAGESHIFT, CME_RESETSHIFTIFABOVE,
  CME_RESTORECAMERASET, CME_RETRACTCAMERA, CME_REVERSETILT, CME_S, CME_SAVE,
  CME_SCREENDOWN, CME_SCREENUP, CME_SETBEAMBLANK, CME_SETBEAMSHIFT, CME_SETBINNING,
  CME_SETCAMLENINDEX, CME_SETCOLUMNORGUNVALVE, CME_SETDEFOCUS, CME_SETDIRECTORY,
  CME_SETENERGYLOSS, CME_SETEXPOSURE, CME_SETINTENSITYBYLASTTILT,
  CME_SETINTENSITYFORMEAN, CME_SETMAG, CME_SETOBJFOCUS, CME_SETPERCENTC2, CME_SETSLITIN,
  CME_SETSLITWIDTH, CME_SETSPOTSIZE, CME_SHOW, CME_SUBAREAMEAN, CME_SUPPRESSREPORTS,
  CME_SWITCHTOFILE, CME_T, CME_TILTBY, CME_TILTDOWN, CME_TILTTO,
  CME_TILTUP, CME_U, CME_V, CME_WAITFORDOSE, CME_WALKUPTO, CME_RETURN, CME_EXIT,
  CME_MOVETONAVITEM, CME_SKIPPIECESOUTSIDEITEM, CME_INCTARGETDEFOCUS, CME_REPORTFILEZSIZE,
  CME_SPECIALEXPOSEFILM, CME_SETMONTAGEPARAMS, CME_AUTOCENTERBEAM, CME_COOKSPECIMEN,
  CME_IMAGEPROPERTIES, CME_ITEMFORSUPERCOORD, CME_SETNEWFILETYPE,
  CME_SHIFTITEMSBYALIGNMENT, CME_SHIFTIMAGEFORDRIFT, CME_REPORTFOCUSDRIFT,
  CME_CALIBRATEIMAGESHIFT, CME_SETMAGANDINTENSITY, CME_CHANGEMAGANDINTENSITY,
  CME_OPENOLDFILE, CME_VERBOSE, CME_MAILSUBJECT, CME_SENDEMAIL, CME_UPDATEITEMZ,
  CME_UPDATEGROUPZ, CME_REPORTGROUPSTATUS, CME_CROPIMAGE, CME_CLEARPERSISTENTVARS,
  CME_ELSEIF, CME_YESNOBOX, CME_MAKEDIRECTORY, CME_ALLOWFILEOVERWRITE, CME_OPPOSITETRIAL,
  CME_OPPOSITEFOCUS, CME_OPPOSITEAUTOFOCUS, CME_REPORTAUTOFOCUS, CME_REPORTTARGETDEFOCUS,
  CME_PLUGIN, CME_LISTPLUGINCALLS, CME_SETSTANDARDFOCUS, CME_SETCAMERAAREA,
  CME_SETILLUMINATEDAREA, CME_REPORTILLUMINATEDAREA, CME_SELECTCAMERA, CME_SETLOWDOSEMODE,
  CME_TESTSTEMSHIFT, CME_QUICKFLYBACK, CME_NORMALIZELENSES, CME_REPORTSLOTSTATUS,
  CME_LOADCARTRIDGE, CME_UNLOADCARTRIDGE, CME_BACKLASHADJUST, CME_SAVEFOCUS,
  CME_MAKEDATETIMEDIR, CME_ENTERNAMEOPENFILE, CME_FINDPIXELSIZE, CME_SETEUCENTRICFOCUS,
  CME_CAMERAPROPERTIES, CME_SETIMAGESHIFT, CME_REPORTIMAGESHIFT, CME_ENTERONENUMBER,
  CME_SETHELPERPARAMS, CME_STEPFOCUSNEXTSHOT, CME_BEAMTILTDIRECTION, CME_OPENFRAMESUMFILE,
  CME_DEFERSTACKINGNEXTSHOT, CME_SETOBJECTIVESTIGMATOR, CME_REPORTOBJECTIVESTIGMATOR,
  CME_AREDEWARSFILLING, CME_DEWARSREMAININGTIME, CME_REFRIGERANTLEVEL,
  CME_OPENDECAMERACOVER, CME_SMOOTHFOCUSNEXTSHOT, CME_REPORTLOWDOSE, CME_ISPVPRUNNING,
  CME_LONGOPERATION, CME_UPDATEHWDARKREF, CME_RESTOREFOCUS, CME_CALEUCENTRICFOCUS,
  CME_EARLYRETURNNEXTSHOT, CME_SELECTPIEZO, CME_REPORTPIEZOXY, CME_REPORTPIEZOZ,
  CME_MOVEPIEZOXY, CME_MOVEPIEZOZ, CME_KEYBREAK, CME_REPORTALPHA, CME_SETALPHA,
  CME_SETSTEMDETECTORS, CME_SETCENTEREDSIZE, CME_IMAGELOWDOSESET, CME_FORCECENTERREALIGN,
  CME_SETTARGETDEFOCUS, CME_FOCUSCHANGELIMITS, CME_ABSOLUTEFOCUSLIMITS, 
  CME_REPORTCOLUMNMODE, CME_REPORTLENS, CME_REPORTCOIL, CME_REPORTABSOLUTEFOCUS,
  CME_SETABSOLUTEFOCUS, CME_REPORTENERGYFILTER, CME_REPORTBEAMTILT, CME_USERSETDIRECTORY,
  CME_SETJEOLSTEMFLAGS, CME_PROGRAMTIMESTAMPS, CME_SETCONTINUOUS, CME_USECONTINUOUSFRAMES,
  CME_WAITFORNEXTFRAME, CME_STOPCONTINUOUS, CME_REPORTCONTINUOUS, 
  CME_SETLIVESETTLEFRACTION, CME_SKIPTO, CME_FUNCTION, CME_ENDFUNCTION, CME_CALLFUNCTION,
  CME_PRECOOKMONTAGE, CME_REPORTALIGNTRIMMING, CME_NORMALIZEALLLENSES, CME_ADDTOAUTODOC,
  CME_ISVARIABLEDEFINED, CME_ENTERDEFAULTEDNUMBER, CME_SETUPWAFFLEMONTAGE,
  CME_INCMAGIFFOUNDPIXEL, CME_RESETDEFOCUS, CME_WRITEAUTODOC, CME_ELECTRONSTATS,
  CME_ERRORSTOLOG, CME_FLASHDISPLAY, CME_REPORTPROBEMODE, CME_SETPROBEMODE,
  CME_SAVELOGOPENNEW, CME_SETAXISPOSITION, CME_ADDTOFRAMEMDOC, CME_WRITEFRAMEMDOC,
  CME_REPORTFRAMEMDOCOPEN, CME_SHIFTCALSKIPLENSNORM, CME_REPORTLASTAXISOFFSET,
  CME_SETBEAMTILT, CME_CORRECTASTIGMATISM, CME_CORRECTCOMA, CME_SHIFTITEMSBYCURRENTDIFF,
  CME_REPORTCURRENTFILENAME, CME_SUFFIXFOREXTRAFILE, CME_REPORTSCREEN, CME_GETDEFERREDSUM,
  CME_REPORTJEOLGIF, CME_SETJEOLGIF, CME_TILTDURINGRECORD, CME_SETLDCONTINUOUSUPDATE,
  CME_ERRORBOXSENDEMAIL, CME_REPORTLASTFRAMEFILE, CME_TEST, CME_ABORTIFFAILED, 
  CME_PAUSEIFFAILED, CME_READOTHERFILE, CME_REPORTK2FILEPARAMS, CME_SETK2FILEPARAMS,
  CME_SETDOSEFRACPARAMS, CME_SETK2READMODE, CME_SETPROCESSING, CME_SETFRAMETIME,
  CME_REPORTCOUNTSCALING, CME_SETDIVIDEBY2, CME_REPORTNUMFRAMESSAVED, CME_REMOVEFILE,
  CME_REPORTFRAMEALIPARAMS, CME_SETFRAMEALIPARAMS, CME_REQUIRE, CME_ONSTOPCALLFUNC,
  CME_RETRYREADOTHERFILE, CME_DOSCRIPT, CME_CALLSCRIPT, CME_SCRIPTNAME, CME_SETFRAMEALI2,
  CME_REPORTFRAMEALI2, CME_REPORTMINUTETIME, CME_REPORTCOLUMNORGUNVALVE,
  CME_NAVINDEXWITHLABEL, CME_NAVINDEXWITHNOTE, CME_SETDIFFRACTIONFOCUS,
  CME_REPORTDIRECTORY, CME_BACKGROUNDTILT, CME_REPORTSTAGEBUSY, CME_SETEXPOSUREFORMEAN,
  CME_FFT, CME_REPORTNUMNAVACQUIRE, CME_STARTFRAMEWAITTIMER, CME_REPORTTICKTIME,
  CME_READTEXTFILE, CME_RUNINSHELL, CME_ELAPSEDTICKTIME, CME_NOMESSAGEBOXONERROR,
  CME_REPORTAUTOFOCUSOFFSET, CME_SETAUTOFOCUSOFFSET, CME_CHOOSERFORNEWFILE, 
  CME_SKIPACQUIRINGNAVITEM, CME_SHOWMESSAGEONSCOPE, CME_SETUPSCOPEMESSAGE, CME_SEARCH,
  CME_SETPROPERTY, CME_SETMAGINDEX, CME_SETNAVREGISTRATION, CME_LOCALVAR,
  CME_LOCALLOOPINDEXES, CME_ZEMLINTABLEAU, CME_WAITFORMIDNIGHT, CME_REPORTUSERSETTING,
  CME_SETUSERSETTING, CME_CHANGEITEMREGISTRATION, CME_SHIFTITEMSBYMICRONS,
  CME_SETFREELENSCONTROL, CME_SETLENSWITHFLC, CME_SAVETOOTHERFILE, CME_SKIPACQUIRINGGROUP,
  CME_REPORTIMAGEDISTANCEOFFSET, CME_SETIMAGEDISTANCEOFFSET, CME_REPORTCAMERALENGTH,
  CME_SETDECAMFRAMERATE, CME_SKIPMOVEINNAVACQUIRE, CME_TESTRELAXINGSTAGE, CME_RELAXSTAGE,
  CME_SKIPFRAMEALIPARAMCHECK, CME_ISVERSIONATLEAST, CME_SKIPIFVERSIONLESSTHAN,
  CME_RAWELECTRONSTATS, CME_ALIGNWHOLETSONLY, CME_WRITECOMFORTSALIGN, CME_RECORDANDTILTTO,
  CME_AREPOSTACTIONSENABLED, CME_MEASUREBEAMSIZE, CME_MULTIPLERECORDS, 
  CME_MOVEBEAMBYMICRONS, CME_MOVEBEAMBYFIELDFRACTION, CME_NEWDESERVERDARKREF,
  CME_STARTNAVACQUIREATEND, CME_REDUCEIMAGE, CME_REPORTAXISPOSITION, CME_CTFFIND,
  CME_CBASTIGCOMA, CME_FIXASTIGMATISMBYCTF, CME_FIXCOMABYCTF, CME_ECHOEVAL, 
  CME_REPORTFILENUMBER, CME_REPORTCOMATILTNEEDED, CME_REPORTSTIGMATORNEEDED,
  CME_SAVEBEAMTILT, CME_RESTOREBEAMTILT, CME_REPORTCOMAVSISMATRIX,CME_ADJUSTBEAMTILTFORIS,
  CME_LOADNAVMAP, CME_LOADOTHERMAP,CME_REPORTLENSFLCSTATUS, CME_TESTNEXTMULTISHOT,
  CME_ENTERSTRING, CME_COMPARESTRINGS, CME_COMPARENOCASE, CME_REPORTNEXTNAVACQITEM,
  CME_REPORTNUMTABLEITEMS, CME_CHANGEITEMCOLOR, CME_CHANGEITEMLABEL,CME_STRIPENDINGDIGITS,
  CME_MAKEANCHORMAP, CME_STAGESHIFTBYPIXELS, CME_REPORTPROPERTY, CME_SAVENAVIGATOR,
  CME_FRAMETHRESHOLDNEXTSHOT, CME_QUEUEFRAMETILTSERIES, CME_FRAMESERIESFROMVAR,
  CME_WRITEFRAMESERIESANGLES, CME_ECHOREPLACELINE, CME_ECHONOLINEEND, CME_REMOVEAPERTURE,
  CME_REINSERTAPERTURE, CME_PHASEPLATETONEXTPOS, CME_SETSTAGEBAXIS, CME_REPORTSTAGEBAXIS,
  CME_DEFERWRITINGFRAMEMDOC, CME_ADDTONEXTFRAMESTACKMDOC, CME_STARTNEXTFRAMESTACKMDOC,
  CME_REPORTPHASEPLATEPOS, CME_OPENFRAMEMDOC, CME_NEXTPROCESSARGS, CME_CREATEPROCESS,
  CME_RUNEXTERNALTOOL, CME_REPORTSPECIMENSHIFT, CME_REPORTNAVFILE, CME_READNAVFILE,
  CME_MERGENAVFILE, CME_REPORTIFNAVOPEN, CME_NEWARRAY, CME_NEW2DARRAY, CME_APPENDTOARRAY,
  CME_TRUNCATEARRAY, CME_READ2DTEXTFILE, CME_ARRAYSTATISTICS, CME_REPORTFRAMEBASENAME,
  CME_OPENTEXTFILE, CME_WRITELINETOFILE, CME_READLINETOARRAY, CME_READLINETOSTRING,
  CME_CLOSETEXTFILE, CME_FLUSHTEXTFILE, CME_READSTRINGSFROMFILE, CME_ISTEXTFILEOPEN,
  CME_CURRENTSETTINGSTOLDAREA, CME_UPDATELOWDOSEPARAMS, CME_RESTORELOWDOSEPARAMS,
  CME_CALLSTRINGARRAY, CME_STRINGARRAYTOSCRIPT, CME_MAKEVARPERSISTENT,
  CME_SETLDADDEDBEAMBUTTON, CME_KEEPCAMERASETCHANGES, CME_REPORTDATETIME,
  CME_REPORTFILAMENTCURRENT, CME_SETFILAMENTCURRENT, CME_CLOSEFRAMEMDOC,
  CME_DRIFTWAITTASK, CME_GETWAITTASKDRIFT, CME_CLOSELOGOPENNEW, CME_SAVELOG,
  CME_SAVECALIBRATIONS, CME_REPORTCROSSOVERPERCENTC2, CME_REPORTSCREENCURRENT,
  CME_LISTVARS, CME_LISTPERSISTENTVARS, CME_REPORTITEMACQUIRE,
  CME_SETITEMACQUIRE, CME_CHANGEITEMNOTE, CME_CIRCULARSUBAREAMEAN,
  CME_REPORTITEMIMAGECOORDS,
  CME_SETFRAMESERIESPARAMS, CME_SETCUSTOMTIME, CME_REPORTCUSTOMINTERVAL, 
  CME_STAGETOLASTMULTIHOLE, CME_IMAGESHIFTTOLASTMULTIHOLE, CME_NAVINDEXITEMDRAWNON,
  CME_SETMAPACQUIRESTATE, CME_RESTORESTATE, CME_REALIGNTOMAPDRAWNON,
  CME_GETREALIGNTOITEMERROR, CME_DOLOOP, CME_REPORTVACUUMGAUGE, CME_REPORTHIGHVOLTAGE,
  CME_OKBOX, CME_LIMITNEXTAUTOALIGN, CME_SETDOSERATE, CME_CHECKFORBADSTRIPE,
  CME_REPORTK3CDSMODE, CME_SETK3CDSMODE, CME_CONDITIONPHASEPLATE, CME_LINEARFITTOVARS,
  CME_REPORTNUMMONTAGEPIECES, CME_NAVITEMFILETOOPEN, CME_CROPCENTERTOSIZE,
  CME_ACQUIRETOMATCHBUFFER,  CME_REPORTXLENSDEFLECTOR, CME_SETXLENSDEFLECTOR, 
  CME_REPORTXLENSFOCUS, CME_SETXLENSFOCUS, CME_EXTERNALTOOLARGPLACE, 
  CME_READONLYUNLESSADMIN, CME_IMAGESHIFTBYSTAGEDIFF, CME_GETFILEINWATCHEDDIR,
  CME_RUNSCRIPTINWATCHEDDIR, CME_PARSEQUOTEDSTRINGS, CME_SNAPSHOTTOFILE,
  CME_COMBINEHOLESTOMULTI, CME_UNDOHOLECOMBINING, CME_MOVESTAGEWITHSPEED,
  CME_FINDHOLES, CME_MAKENAVPOINTSATHOLES, CME_CLEARHOLEFINDER, CME_REPORTFRAMETILTSUM ,
  CME_MODIFYFRAMETSSHIFTS, CME_REPLACEFRAMETSSHIFTS, CME_REPLACEFRAMETSFOCUS, 
  CME_CAMERATOISMATRIX, CME_ISTOCAMERAMATRIX, CME_CAMERATOSTAGEMATRIX, 
  CME_STAGETOCAMERAMATRIX, CME_CAMERATOSPECIMENMATRIX, CME_SPECIMENTOCAMERAMATRIX, 
  CME_ISTOSPECIMENMATRIX, CME_SPECIMENTOISMATRIX, CME_ISTOSTAGEMATRIX, 
  CME_STAGETOISMATRIX, CME_STAGETOSPECIMENMATRIX, CME_SPECIMENTOSTAGEMATRIX, 
  CME_REPORTISFORBUFFERSHIFT, CME_ALIGNWITHROTATION, CME_TRY, CME_CATCH, CME_ENDTRY, 
  CME_THROW, CME_ALIGNANDTRANSFORMITEMS, CME_ROTATEMULTISHOTPATTERN,
  CME_SETNAVITEMUSERVALUE, CME_REPORTITEMUSERVALUE, CME_FILTERIMAGE,
  // Place for others to add commands
  // End of reserved region
  CME_SETFOLDERFORFRAMES, CME_SETITEMTARGETDEFOCUS, CME_SETITEMSERIESANGLES,
  CME_SUSPENDNAVREDRAW, CME_DEFERLOGUPDATES 
};

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

typedef int(CMacCmd::*DispEntry)(void);

struct CmdItem {
  const char *mixedCase;
  int minargs;
  int arithAllowed;
  DispEntry func;
  std::string cmd;
};

class CMacroProcessor : public CCmdTarget
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
  GetMember(int, TryCatchLevel);
  GetMember(bool, SuspendNavRedraw);
  GetMember(bool, DeferLogUpdates);
  GetMember(bool, LoopInOnIdle);
  GetSetMember(BOOL, RestoreMacroEditors);
  int GetReadOnlyStart(int macNum) { return mReadOnlyStart[macNum]; };
  void SetReadOnlyStart(int macNum, int start) { mReadOnlyStart[macNum] = start; };
  std::map<std::string, int> *GetCustomTimeMap() { return &mCustomTimeMap;};
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

protected:
  MacroControl * mControl;
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  CString * mModeNames;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CShiftManager *mShiftManager;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  CNavHelper *mNavHelper;
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

public:
  void GetNextLine(CString * macro, int & currentIndex, CString &strLine);
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
  WINDOWPLACEMENT * GetEditerPlacement(void) {return &mEditerPlacement[0];};
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
  bool ConvertBufferLetter(CString strItem, int emptyDefault, bool checkImage, int &bufIndex, CString & message);
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
  void SendEmailIfNeeded(void);
  int TestAndStartFuncOnStop(void);
  int TestTryLevelAndSkip(CString *mess);
  int CheckForArrayAssignment(CString * strItems, int &firstInd);
  void FindValueAtIndex(CString &value, int arrInd, int & beginInd, int & endInd);
  int ConvertArrayIndex(CString strItem, int leftInd, int rightInd, CString name, int numElements, 
    CString * errMess);
  static UINT RunInShellProc(LPVOID pParam);
  afx_msg void OnScriptSetIndentSize();
  afx_msg void OnScriptListPersistentVars();
  afx_msg void OnScriptClearPersistentVars();
  afx_msg void OnUpdateClearPersistentVars(CCmdUI *pCmdUI);
  afx_msg void OnScriptRunOneCommand();
  afx_msg void OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI);
  int StartNavAvqBusy(void);
  int CheckLegalCommandAndArgNum(CString * strItems, CString strLine, int macroNum);
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
};

#include "MacroCommands.h"

#endif // !defined(AFX_MACROPROCESSOR_H__33178182_58A1_4F3A_B8F4_D41F94866517__INCLUDED_)
