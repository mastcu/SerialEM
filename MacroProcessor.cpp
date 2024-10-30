// MacroProcessor.cpp:    Runs macros
//
// Copyright (C) 2003-2018 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <direct.h>
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "MainFrm.h"
#include ".\MacroProcessor.h"
#include "MacroEditer.h"
#include "MacroToolbar.h"
#include "FocusManager.h"
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "EMmontageController.h"
#include "LogWindow.h"
#include "EMscope.h"
#include "CameraController.h"
#include "CalibCameraTiming.h"
#include "TSController.h"
#include "ParameterIO.h"
#include "BeamAssessor.h"
#include "ComplexTasks.h"
#include "HoleFinderDlg.h"
#include "AutoContouringDlg.h"
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "ProcessImage.h"
#include "MultiShotDlg.h"
#include "MacroControlDlg.h"
#include "NavigatorDlg.h"
#include "OneLineScript.h"
#include "Mailer.h"
#include "PiezoAndPPControl.h"
#include "MacroSelector.h"
#include "ExternalTools.h"
#include "PythonServer.h"
#include "BaseSocket.h"
#include "Utilities\KGetOne.h"
#include "Shared\iimage.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define CMD_IS(a) (cmdIndex == CME_##a)

#define ABORT_NORET_LINE(a) \
{ \
  mWinApp->AppendToLog((a) + strLine, mLogErrAction);  \
  SEMMessageBox((a) + strLine, MB_EXCLAME); \
  AbortMacro(); \
}

#define FAIL_CHECK_LINE(a) \
{ \
  CString macStr = CString("Script failed initial checking - nothing was executed."  \
  "\r\n\r\n") + (a) + errmess + " on line:\r\n\r\n" + strLine;          \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  return 99; \
}

#define FAIL_CHECK_NOLINE(a) \
{ \
  CString macStr = CString("Script failed initial checking - nothing was executed." \
  "\r\n\r\n") + (a) + errmess;             \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  return 98; \
}

ScriptLangData CMacroProcessor::mScrpLangData;
ScriptLangPlugFuncs *CMacroProcessor::mScrpLangFuncs = NULL;
HANDLE CMacroProcessor::mScrpLangDoneEvent = NULL;
HANDLE CMacroProcessor::mPyProcessHandle = NULL;
CWinThread *CMacroProcessor::mScrpLangThread;
std::set<int> CMacroProcessor::mPythonOnlyCmdSet;

static int sProcessExitStatus;
static int sProcessErrno;

BEGIN_MESSAGE_MAP(CMacroProcessor, CCmdTarget)
  //{{AFX_MSG_MAP(CMacroProcessor)
  ON_COMMAND(ID_MACRO_END, OnMacroEnd)
  ON_COMMAND(ID_MACRO_STOP, OnMacroStop)
  ON_COMMAND(ID_MACRO_RESUME, OnMacroResume)
  ON_UPDATE_COMMAND_UI(ID_MACRO_RESUME, OnUpdateMacroResume)
  ON_COMMAND(ID_MACRO_CONTROLS, OnMacroControls)
  ON_UPDATE_COMMAND_UI(ID_MACRO_STOP, OnUpdateMacroStop)
  //}}AFX_MSG_MAP
  ON_COMMAND_RANGE(ID_MACRO_EDIT1, ID_MACRO_EDIT10, OnMacroEdit)
  ON_UPDATE_COMMAND_UI_RANGE(ID_MACRO_EDIT1, ID_MACRO_EDIT10, OnUpdateMacroEdit)
  ON_COMMAND_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN60, OnMacroRun)
  ON_UPDATE_COMMAND_UI_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN60, OnUpdateMacroRun)
  ON_COMMAND(ID_MACRO_TOOLBAR, OnMacroToolbar)
  ON_UPDATE_COMMAND_UI(ID_MACRO_END, OnUpdateMacroEnd)
  ON_COMMAND(ID_MACRO_SETLENGTH, OnMacroSetlength)
  ON_COMMAND(ID_MACRO_VERBOSE, OnMacroVerbose)
  ON_UPDATE_COMMAND_UI(ID_MACRO_VERBOSE, OnUpdateMacroVerbose)
  ON_COMMAND(ID_MACRO_EDIT15, OnMacroEdit15)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT15, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT20, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT20, OnMacroEdit20)
  ON_COMMAND(ID_MACRO_EDIT25, OnMacroEdit25)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT25, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT30, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT30, OnMacroEdit30)
  ON_COMMAND(ID_MACRO_EDIT35, OnMacroEdit35)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT35, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT40, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT40, OnMacroEdit40)
  ON_COMMAND(ID_MACRO_EDIT45, OnMacroEdit45)
  ON_COMMAND(ID_MACRO_EDIT50, OnMacroEdit50)
  ON_COMMAND(ID_MACRO_EDIT55, OnMacroEdit55)
  ON_COMMAND(ID_MACRO_EDIT60, OnMacroEdit60)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT45, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT50, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT55, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT60, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_READMANY, OnMacroReadMany)
  ON_UPDATE_COMMAND_UI(ID_MACRO_READMANY, OnUpdateMacroReadMany)
  ON_COMMAND(ID_MACRO_LISTFUNCTIONS, OnMacroListFunctions)
  ON_UPDATE_COMMAND_UI(ID_MACRO_LISTFUNCTIONS, OnUpdateNoTasks)
  ON_COMMAND(ID_SCRIPT_SETINDENTSIZE, OnScriptSetIndentSize)
  ON_COMMAND(ID_SCRIPT_LISTPERSISTENTVARS, OnScriptListPersistentVars)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_CLEARPERSISTENTVARS, OnUpdateNoTasks)
  ON_COMMAND(ID_SCRIPT_CLEARPERSISTENTVARS, OnScriptClearPersistentVars)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_CLEARPERSISTENTVARS, OnUpdateClearPersistentVars)
  ON_COMMAND(ID_SCRIPT_RUNONECOMMAND, OnScriptRunOneCommand)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_RUNONECOMMAND, OnUpdateScriptRunOneCommand)
  ON_COMMAND(ID_SCRIPT_OPENEDITORSONSTART, OnOpenEditorsOnStart)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_OPENEDITORSONSTART, OnUpdateOpenEditorsOnStart)
  ON_COMMAND(ID_SCRIPT_LOADNEWPACKAGE, OnScriptLoadNewPackage)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_LOADNEWPACKAGE, OnUpdateMacroReadMany)
  ON_COMMAND(ID_SCRIPT_SAVEPACKAGE, OnScriptSavePackage)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_SAVEPACKAGE, OnUpdateNoTasks)
  ON_COMMAND(ID_SCRIPT_SAVEPACKAGEAS, OnScriptSavePackageAs)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_SAVEPACKAGEAS, OnUpdateNoTasks)
  ON_COMMAND(ID_SCRIPT_RUNONPROGRAMSTART, OnRunOnProgramStart)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_RUNONPROGRAMSTART, OnUpdateRunOnProgramStart)
  ON_COMMAND(ID_SCRIPT_RUNATPROGRAMEND, OnRunAtProgramEnd)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_RUNATPROGRAMEND, OnUpdateRunAtProgramEnd)
  ON_COMMAND(ID_SCRIPT_USEMONOSPACEDFONT, OnUseMonospacedFont)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_USEMONOSPACEDFONT, OnUpdateUseMonospacedFont)
  ON_COMMAND(ID_SCRIPT_SHOWINDENTBUTTONS, OntShowIndentButtons)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_SHOWINDENTBUTTONS, OnUpdateShowIndentButtons)
  ON_COMMAND(ID_SCRIPT_SETPANELROWS, OnScriptSetpanelrows)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_SETPANELROWS, OnUpdateScriptSetpanelrows)
  ON_COMMAND(ID_HELP_RUNSERIALEMSNAPSHOT, OnRunSerialemSnapshot)
  ON_UPDATE_COMMAND_UI(ID_HELP_RUNSERIALEMSNAPSHOT, OnUpdateRunSerialemSnapshot)
  ON_COMMAND(ID_SCRIPT_RUNIFPROGRAMIDLE, OnRunIfProgramIdle)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_RUNIFPROGRAMIDLE, OnUpdateRunIfProgramIdle)
  ON_COMMAND(ID_SCRIPT_SET_NUM_STATUS, OnScriptSetNumStatus)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_SET_NUM_STATUS, OnUpdateScriptSetpanelrows)
  ON_COMMAND(ID_SCRIPT_MONOSPACESTATUSLINES, OnMonospaceStatusLines)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_MONOSPACESTATUSLINES, OnUpdateMonospaceStatusLines)
  ON_COMMAND(ID_SCRIPT_KEEPFOCUSONONELINE, OnKeepFocusOnOneLine)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_KEEPFOCUSONONELINE, OnUpdateKeepFocusOnOneLine)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMacroProcessor::CMacroProcessor()
{
  int i;
  std::string tmpOp1[] = { "SQRT", "COS", "SIN", "TAN", "ATAN", "ABS", "NEARINT",
    "LOG", "LOG10", "EXP" };
  std::string tmpOp2[] = { "ROUND", "POWER", "ATAN2", "MODULO", "DIFFABS",
    "FRACDIFF", "MIN", "MAX", "FORMAT" };
  std::string keywords[] = { "REPEAT", "ENDLOOP", "DOMACRO", "LOOP", "CALLMACRO", "DOLOOP"
    , "ENDIF", "IF", "ELSE", "BREAK", "CONTINUE", "CALL", "EXIT", "RETURN", "KEYBREAK",
    "SKIPTO", "FUNCTION", "CALLFUNCTION", "ENDFUNCTION", "CALLSCRIPT", "DOSCRIPT",
    "TRY", "CATCH", "ENDTRY", "THROW"};
  int pythonOnlyCmds[] = {CME_SETVARIABLE, CME_SETPERSISTENTVAR, CME_GETVARIABLE,
  CME_SETFLOATVARIABLE, CME_PLUGINALLDOUBLES, CME_PLUGINSTRING, CME_PLUGINDOUBLESTRING};
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mMacNames = mWinApp->GetMacroNames();
  mMagTab = mWinApp->GetMagTable();
  mImBufs = mWinApp->GetImBufs();
  mFFTBufs = mWinApp->GetFFTBufs();
  mMacros = mWinApp->GetMacros();
  mControl = mWinApp->GetMacControl();
  mMacroEditer = &(mWinApp->mMacroEditer[0]);
  mConSets = mWinApp->GetConSets();
  mOneLineScript = NULL;
  mFunctionSet1.insert(tmpOp1, tmpOp1 + sizeof(tmpOp1) / sizeof(std::string));
  mFunctionSet2.insert(tmpOp2 , tmpOp2 + sizeof(tmpOp2) / sizeof(std::string));
  mReservedWords.insert(tmpOp1, tmpOp1 + sizeof(tmpOp1) / sizeof(std::string));
  mReservedWords.insert(tmpOp2, tmpOp2 + sizeof(tmpOp2) / sizeof(std::string));
  mReservedWords.insert(keywords, keywords + sizeof(keywords) / sizeof(std::string));
  mPythonOnlyCmdSet.insert(pythonOnlyCmds, pythonOnlyCmds + sizeof(pythonOnlyCmds) / 
    sizeof(int));
  mDocWnd = mWinApp->mDocWnd;
  mMaxCmdToLoopOnIdle = 100;
  mMaxSecToLoopOnIdle = 1.;
  mDoingMacro = false;
  mCurrentMacro = -1;
  mInitialVerbose = false;
  mVarArray.SetSize(0, 5);
  mSleepTime = 0.;
  mDoseTarget = 0.;
  mMovedStage = false;
  mMovedScreen = false;
  mExposedFilm = false;
  mStartedLongOp = false;
  mMovedPiezo = false;
  mMovedAperture = false;
  mRanGatanScript = false;
  mRanCtfplotter = false;
  mRanExtProcess = false;
  mLoadingMap = false;
  mMakingDualMap = false;
  mLastCompleted = false;
  mLastAborted = false;
  mSuspendNavRedraw = false;
  mEnteredName = "";
  mRunningScrpLang = false;
  mCalledFromScrpLang = false;
  mNonMacroDeferring = false;
  mScrpLangData.externalControl = 0;
  mShrMemFile = NULL;
  mToolPlacement.rcNormalPosition.right = NO_PLACEMENT;
  mNumToolButtons = 10;
  mToolButHeight = 0;
  mNumCamMacRows = 1;
  mNumStatusLines = 0;
  for (i = 0; i < NUM_CM_MESSAGE_LINES; i++)
    mHighlightStatus[i] = false;
  mMonospaceStatus = false;
  mKeepOneLineFocus = true;
  mAutoIndentSize = 3;
  mShowIndentButtons = true;
  mUseMonoFont = false;
  mFocusedWndWhenSavedStatus = NULL;
  mSavedMultiShot = NULL;
  mRestoreMacroEditors = true;
  mOneLinePlacement.rcNormalPosition.right = NO_PLACEMENT;
  mFileOptToRestore = NULL;
  mOtherFileOptToRestore = NULL;
  mMailSubject = "Message from SerialEM script";
  for (i = 0; i < MAX_MACROS; i++) {
    mStrNum[i].Format("%d", i + 1);
    mFuncArray[i].SetSize(0, 4);
    mEditerPlacement[i].rcNormalPosition.right = NO_PLACEMENT;
  }
  for (i = 0; i < MAX_TOT_MACROS; i++)
    mReadOnlyStart[i] = -1;
  srand(GetTickCount());
  mProcessThread = NULL;
  mScrpLangThread = NULL;
  mScrpLangDoneEvent = CreateEventA(NULL, FALSE, FALSE, SCRIPT_EVENT_NAME);
  if (!mScrpLangDoneEvent) {
    AfxMessageBox("Cannot run external scripting language; failed to make event"
      " for signaling to it");
  }
}

CMacroProcessor::~CMacroProcessor()
{
  ClearVariables();
  ClearVariables(VARTYPE_PERSIST);
  ClearFunctionArray(-1);
  CloseFileForText(-2);
}

void CMacroProcessor::ShutdownScrpLangScripting()
{
  TerminateScrpLangProcess();
  if (mWinApp->mPythonServer)
    mWinApp->mPythonServer->ShutdownSocketIfOpen(RUN_PYTH_SOCK_ID);
}

void CMacroProcessor::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  mNavHelper = mWinApp->mNavHelper;
  mProcessImage = mWinApp->mProcessImage;
  mFocusManager = mWinApp->mFocusManager;
  mParamIO = mWinApp->mParamIO;
  if (GetDebugOutput('%')) {
    mWinApp->AppendToLog("Commands allowing arithmetic in arguments:");
    for (int i = 0; i < mNumCommands - 1; i++)
      if (mCmdList[i].arithAllowed & 1)
        mWinApp->AppendToLog(mCmdList[i].mixedCase);
  }
  if (mPyModulePath.IsEmpty())
    mPyModulePath = mWinApp->GetExePath() + "\\PythonModules";
  mPyModulePath.Replace("\\", "/");
}

// MESSAGE HANDLERS
void CMacroProcessor::OnMacroEdit(UINT nID)
{
  int index = nID - ID_MACRO_EDIT1;
  if (nID == ID_MACRO_EDIT15)
    index = 14;
  if (nID == ID_MACRO_EDIT20)
    index = 19;
  if (nID == ID_MACRO_EDIT25)
    index = 24;
  if (nID == ID_MACRO_EDIT30)
    index = 29;
  if (nID == ID_MACRO_EDIT35)
    index = 34;
  if (nID == ID_MACRO_EDIT40)
    index = 39;
  if (nID == ID_MACRO_EDIT45)
    index = 44;
  if (nID == ID_MACRO_EDIT50)
    index = 49;
  if (nID == ID_MACRO_EDIT55)
    index = 54;
  if (nID == ID_MACRO_EDIT60)
    index = 59;
  OpenMacroEditor(index);
}

// Actually open an editor if it is not open
void CMacroProcessor::OpenMacroEditor(int index)
{
  if (index < 0 || index >= MAX_MACROS || mMacroEditer[index])
    return;
  mMacroEditer[index] = new CMacroEditer;
  ASSERT(mMacroEditer[index] != NULL);
  mMacroEditer[index]->m_strMacro = mMacros[index];
  mMacroEditer[index]->m_iMacroNumber = index;
  mMacroEditer[index]->Create(IDD_MACRO);
  mMacroEditer[index]->ShowWindow(SW_SHOW);
  mWinApp->mCameraMacroTools.Update();
}

void CMacroProcessor::OnUpdateMacroEdit(CCmdUI* pCmdUI)
{
  int index = pCmdUI->m_nID - ID_MACRO_EDIT1;
  if (pCmdUI->m_nID == ID_MACRO_EDIT15)
    index = 14;
  if (pCmdUI->m_nID == ID_MACRO_EDIT20)
    index = 19;
  if (pCmdUI->m_nID == ID_MACRO_EDIT25)
    index = 24;
  if (pCmdUI->m_nID == ID_MACRO_EDIT30)
    index = 29;
  if (pCmdUI->m_nID == ID_MACRO_EDIT35)
    index = 34;
  if (pCmdUI->m_nID == ID_MACRO_EDIT40)
    index = 39;
  if (pCmdUI->m_nID == ID_MACRO_EDIT45)
    index = 44;
  if (pCmdUI->m_nID == ID_MACRO_EDIT50)
    index = 49;
  if (pCmdUI->m_nID == ID_MACRO_EDIT55)
    index = 54;
  if (pCmdUI->m_nID == ID_MACRO_EDIT60)
    index = 59;
  pCmdUI->Enable(!DoingMacro() && !mMacroEditer[index]);
}

void CMacroProcessor::OnMacroEdit15()
{
  OnMacroEdit(ID_MACRO_EDIT15);
}

void CMacroProcessor::OnMacroEdit20()
{
  OnMacroEdit(ID_MACRO_EDIT20);
}

void CMacroProcessor::OnMacroEdit25()
{
  OnMacroEdit(ID_MACRO_EDIT25);
}

void CMacroProcessor::OnMacroEdit30()
{
  OnMacroEdit(ID_MACRO_EDIT30);
}

void CMacroProcessor::OnMacroEdit35()
{
  OnMacroEdit(ID_MACRO_EDIT35);
}

void CMacroProcessor::OnMacroEdit40()
{
  OnMacroEdit(ID_MACRO_EDIT40);
}

void CMacroProcessor::OnMacroEdit45()
{
  OnMacroEdit(ID_MACRO_EDIT45);
}

void CMacroProcessor::OnMacroEdit50()
{
  OnMacroEdit(ID_MACRO_EDIT50);
}

void CMacroProcessor::OnMacroEdit55()
{
  OnMacroEdit(ID_MACRO_EDIT55);
}

void CMacroProcessor::OnMacroEdit60()
{
  OnMacroEdit(ID_MACRO_EDIT60);
}

void CMacroProcessor::OnMacroToolbar()
{
  if (mWinApp->mMacroToolbar) {
    mWinApp->mMacroToolbar->BringWindowToTop();
    return;
  }
  mWinApp->mMacroToolbar = new CMacroToolbar();
  mWinApp->mMacroToolbar->Create(IDD_MACROTOOLBAR);
  mWinApp->SetPlacementFixSize(mWinApp->mMacroToolbar, &mToolPlacement);
  mWinApp->RestoreViewFocus();
}

void CMacroProcessor::OnMacroSetlength()
{
  CString str;
  int num = mNumToolButtons;
  str.Format("Number of buttons to show in script toolbar (between 5 and %d):",
    MAX_MACROS);
  if (!KGetOneInt(str, num))
    return;
  if (!KGetOneInt("Height of each button in pixels (0 for default):", mToolButHeight))
    return;
  mNumToolButtons = B3DMIN(MAX_MACROS, B3DMAX(5, num));
  if (mWinApp->mMacroToolbar)
    mWinApp->mMacroToolbar->SetLength(mNumToolButtons, mToolButHeight);
}

void CMacroProcessor::OnScriptSetpanelrows()
{
  int val = mNumCamMacRows;
  if (KGetOneInt("Number of rows of script buttons and spinners to show in Camera & "
    "Script control panel (1 - 4):", val))
    SetNumCamMacRows(val);
}

void CMacroProcessor::OnUpdateScriptSetpanelrows(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CMacroProcessor::SetNumCamMacRows(int inVal)
{
  B3DCLAMP(inVal, 1, NUM_CAM_MAC_PANELS - 1);
  mNumCamMacRows = inVal;
  mWinApp->mCameraMacroTools.ManagePanels();
  mWinApp->mCameraMacroTools.SetMacroLabels();
}

void CMacroProcessor::OnScriptSetNumStatus()
{
  int val = mNumStatusLines;
  if (KGetOneInt("Number of lines for status messages to show in Camera & "
    "Script control panel (0 - 6):", val))
    SetNumStatusLines(val);
}

void CMacroProcessor::SetNumStatusLines(int inVal)
{
  B3DCLAMP(inVal, 0, NUM_CM_MESSAGE_LINES);
  mNumStatusLines = inVal;
  mWinApp->mCameraMacroTools.ManagePanels();
}

void CMacroProcessor::OnMonospaceStatusLines()
{
  mMonospaceStatus = !mMonospaceStatus;
  if (mNumStatusLines)
    mWinApp->mCameraMacroTools.Invalidate();
}

void CMacroProcessor::OnUpdateMonospaceStatusLines(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mMonospaceStatus ? 1 : 0);
}


void CMacroProcessor::OnKeepFocusOnOneLine()
{
  mKeepOneLineFocus = !mKeepOneLineFocus;
}


void CMacroProcessor::OnUpdateKeepFocusOnOneLine(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mKeepOneLineFocus ? 1 : 0);
}

void CMacroProcessor::OnScriptListPersistentVars()
{
  ListVariables(VARTYPE_PERSIST);
}

void CMacroProcessor::OnScriptClearPersistentVars()
{
  ClearVariables(VARTYPE_PERSIST);
  mCurrentMacro = -1;
  mWinApp->mCameraMacroTools.Update();
}

void CMacroProcessor::OnUpdateClearPersistentVars(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingMacro() && !mWinApp->DoingTasks());
}

void CMacroProcessor::OnScriptSetIndentSize()
{
  KGetOneInt("Number of spaces for automatic indentation, or 0 to disable:", 
    mAutoIndentSize);
}

void CMacroProcessor::OntShowIndentButtons()
{
  mShowIndentButtons = !mShowIndentButtons;
  AfxMessageBox("Resize open script editor(s) to make this change take effect", 
    MB_ICONINFORMATION | MB_OK);
}

void CMacroProcessor::OnUpdateShowIndentButtons(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mShowIndentButtons ? 1 : 0);
}

void CMacroProcessor::OnUseMonospacedFont()
{
  SetUseMonoFont(!mUseMonoFont);
}

void CMacroProcessor::SetUseMonoFont(BOOL inVal)
{
  mUseMonoFont = inVal;
  for (int ind = 0; ind < MAX_MACROS; ind++)
    if (mMacroEditer[ind])
      mMacroEditer[ind]->m_editMacro.SetFont(mUseMonoFont ? 
        &CMacroEditer::mMonoFont : &CMacroEditer::mDefaultFont);
}

void CMacroProcessor::OnUpdateUseMonospacedFont(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mUseMonoFont > 0 ? 1 : 0);
  pCmdUI->Enable(mUseMonoFont >= 0);
}

void CMacroProcessor::OnOpenEditorsOnStart()
{
  mRestoreMacroEditors = !mRestoreMacroEditors;
}

void CMacroProcessor::OnUpdateOpenEditorsOnStart(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mRestoreMacroEditors ? 1 : 0);
}

void CMacroProcessor::OnMacroVerbose()
{
  mInitialVerbose = !mInitialVerbose;
}

void CMacroProcessor::OnUpdateMacroVerbose(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mInitialVerbose ? 1 : 0);
}

// Read and write many macros
void CMacroProcessor::OnMacroReadMany()
{
  CString filename, direc;
  mDocWnd->DirFromCurrentOrSettingsFile(mDocWnd->GetCurScriptPackPath(),
    direc);
  if (mDocWnd->GetTextFileName(true, true, filename, NULL, &direc))
    return;
  mParamIO->ReadMacrosFromFile(filename, "", MAX_MACROS);
  UpdateAllForNewScripts(false);
}

void CMacroProcessor::OnUpdateMacroReadMany(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!(mWinApp->DoingTasks() || mWinApp->DoingTiltSeries() ||
    (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring())));
}

void CMacroProcessor::OnUpdateNoTasks(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

// Load a new file as the package file, wipe out other scripts
void CMacroProcessor::OnScriptLoadNewPackage()
{
  CString filename, path, filen, oldFile;
  bool saveCurrent = false;
  oldFile = mDocWnd->GetCurScriptPackPath();
  mDocWnd->DirFromCurrentOrSettingsFile(oldFile, path);

  // Find out if want to save
  if (!oldFile.IsEmpty())
    saveCurrent = AfxMessageBox("Save current scripts to current file before loading a new"
      " package file?", MB_QUESTION) == IDYES;
  if (mDocWnd->GetTextFileName(true, true, filename, NULL, &path))
    return;
  LoadNewScriptPackage(filename, saveCurrent);
}

// Actually do the saving and loading, callable from elsewhere
int CMacroProcessor::LoadNewScriptPackage(CString & filename, bool saveCurrent)
{
  CString filen, oldFile;
  int retval = 0;
  oldFile = mDocWnd->GetCurScriptPackPath();
  if (saveCurrent) {
    mDocWnd->ManageScriptPackBackup();
    mParamIO->WriteMacrosToFile(oldFile, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
  }
  mWinApp->ClearAllMacros();

  // Try to read, try to revert if fails, and if THAT fails, assign the default name
  if (mParamIO->ReadMacrosFromFile(filename, "", MAX_MACROS + MAX_ONE_LINE_SCRIPTS,
    true)) {
    mWinApp->ClearAllMacros();
    filename = oldFile;
    retval = 1;
    if (mParamIO->ReadMacrosFromFile(filename, "",  MAX_MACROS + MAX_ONE_LINE_SCRIPTS,
      true)) {
      oldFile = mDocWnd->GetCurrentSettingsPath();
      UtilSplitExtension(oldFile, filename, filen);
      filename += "-scripts.txt";
      mWinApp->AppendToLog("Unable to reload last script package; scripts will be saved "
        "to\n" + filename + " unless you do \"Save Package As\" to a different name");
      retval = 2;
    }
  }
  mDocWnd->SetCurScriptPackPath(filename);
  mDocWnd->SetScriptPackBackedUp(false);
  UpdateAllForNewScripts(true);
  mWinApp->AppendToLog("Current script package file is now " + filename);
  return retval;
}

// Take care of all components that need to change when there are potentially new scripts
void CMacroProcessor::UpdateAllForNewScripts(bool oneLinersToo)
{
  if (mWinApp->mMacroToolbar) {
    mWinApp->mMacroToolbar->SetLength(mNumToolButtons, mToolButHeight);
    mWinApp->mMacroToolbar->UpdateSettings();
  }
  mWinApp->mCameraMacroTools.UpdateSettings();
  mWinApp->UpdateAllEditers();
  if (oneLinersToo)
    TransferOneLiners(false);
}

// Save to currently defined package file
void CMacroProcessor::OnScriptSavePackage()
{
  CString filename = mDocWnd->GetCurScriptPackPath();
  if (filename.IsEmpty())
    OnScriptSavePackageAs();
  else
    mParamIO->WriteMacrosToFile(filename, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
}

// Save to new package file
void CMacroProcessor::OnScriptSavePackageAs()
{
  CString filename, path;
  mDocWnd->DirFromCurrentOrSettingsFile(mDocWnd->GetCurScriptPackPath(),
    path);
  if (mDocWnd->GetTextFileName(false, true, filename, NULL, &path, NULL, true))
    return;
  mParamIO->WriteMacrosToFile(filename, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
  mDocWnd->SetCurScriptPackPath(filename);
  mDocWnd->SetScriptPackBackedUp(false);
  mWinApp->AppendToLog("Current script package file is now " + filename);
}

// Run the serialEM Snapshot script from menu
void CMacroProcessor::OnRunSerialemSnapshot()
{
  mMacros[MAX_TOT_MACROS - 1] = "RunSerialEMSnapshot";
  Run(MAX_TOT_MACROS - 1);
}

void CMacroProcessor::OnUpdateRunSerialemSnapshot(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mCamera->CameraBusy());
}

void CMacroProcessor::OpenMacroToolbar(void)
{
  OnMacroToolbar();
}

void CMacroProcessor::ToolbarMacroRun(UINT nID)
{
  OnMacroRun(nID);
}

WINDOWPLACEMENT * CMacroProcessor::GetToolPlacement(void)
{
  if (mWinApp->mMacroToolbar)
    mWinApp->mMacroToolbar->GetWindowPlacement(&mToolPlacement);
  return &mToolPlacement;
}

void CMacroProcessor::ToolbarClosing(void)
{
  mWinApp->mMacroToolbar->GetWindowPlacement(&mToolPlacement);
  mWinApp->mMacroToolbar = NULL;
}

WINDOWPLACEMENT *CMacroProcessor::FindEditerPlacement(int index)
{
  if (mMacroEditer[index])
    mMacroEditer[index]->GetWindowPlacement(&mEditerPlacement[index]);
  return &mEditerPlacement[index];
}

// List all the functions defined in macros
void CMacroProcessor::OnMacroListFunctions()
{
  int index, fun;
  CString title;
  MacroFunction *funcP;
  for (index = 0; index < MAX_MACROS; index++) {
    if (mMacroEditer[index])
      mMacroEditer[index]->TransferMacro(true);
    ScanForName(index, &mMacros[index]);
    if (mFuncArray[index].GetSize()) {
      title.Format("\r\nScript %d", index + 1);
      if (!mMacNames[index].IsEmpty())
        title += ": " + mMacNames[index];
      mWinApp->AppendToLog(title);
      for (fun = 0; fun < (int)mFuncArray[index].GetSize(); fun++) {
        funcP = mFuncArray[index].GetAt(fun);
        PrintfToLog("%s %d %d", (LPCTSTR)funcP->name, funcP->numNumericArgs, 
          funcP->ifStringArg ? 1 : 0);
      }
    }
  }
}

void CMacroProcessor::OnMacroRun(UINT nID)
{
  int index = nID - ID_MACRO_RUN1;
  if (!MacroRunnable(index))
    return;

  // If the editor is open, unload the string and copy to macro
  if (mMacroEditer[index])
    mMacroEditer[index]->TransferMacro(true);
  if (!mMacros[index].IsEmpty())
    Run(index);
}

// Enable a macro if there are no tasks, and it is nonempty or being edited
void CMacroProcessor::OnUpdateMacroRun(CCmdUI* pCmdUI)
{
  int index = pCmdUI->m_nID - ID_MACRO_RUN1;
  pCmdUI->Enable(MacroRunnable(index));
}


void CMacroProcessor::OnScriptRunOneCommand()
{
  if (mOneLineScript) {
    mOneLineScript->BringWindowToTop();
    return;
  }
  mOneLineScript = new COneLineScript();
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    mOneLineScript->m_strOneLine[ind] = mMacros[MAX_MACROS + ind];
    mOneLineScript->m_strOneLine[ind].TrimRight("\r\n");
    mOneLineScript->m_strOneLine[ind].Replace("\r\n", ";");
  }
  mOneLineScript->mMacros = mMacros;
  mOneLineScript->Create(IDD_ONELINESCRIPT);
  if (mOneLinePlacement.rcNormalPosition.right != NO_PLACEMENT)
    mOneLineScript->SetWindowPlacement(&mOneLinePlacement);
  mOneLineScript->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

void CMacroProcessor::OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI)
{
  CString menuText;
  CString *longMacNames = mWinApp->GetLongMacroNames();
  for (int ind = 0; ind < MAX_MACROS; ind++) {
    if (!longMacNames[ind].IsEmpty())
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)longMacNames[ind]);
    else if (mMacNames[ind].IsEmpty())
      menuText.Format("Run %d", ind + 1);
    else
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)mMacNames[ind]);
    UtilModifyMenuItem("Script", ID_MACRO_RUN1 + ind, (LPCTSTR)menuText);
  }
  pCmdUI->Enable(!DoingMacro());
}

WINDOWPLACEMENT *CMacroProcessor::GetOneLinePlacement(void)
{
  if (mOneLineScript)
    mOneLineScript->GetWindowPlacement(&mOneLinePlacement);
  return &mOneLinePlacement;
}

void CMacroProcessor::OneLineClosing(void)
{
  mOneLineScript->GetWindowPlacement(&mOneLinePlacement);
  mOneLineScript = NULL;
}

// Transfer all one-line scripts to or from the dialog if it is open
void CMacroProcessor::TransferOneLiners(bool fromDialog)
{
  if (!mOneLineScript)
    return;
  if (fromDialog)
    mOneLineScript->UpdateData(true);
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    if (fromDialog) {
      mMacros[MAX_MACROS + ind] = mOneLineScript->m_strOneLine[ind];
      mMacros[MAX_MACROS + ind].Replace(";", "\r\n");
    } else {
      mOneLineScript->m_strOneLine[ind] = mMacros[MAX_MACROS + ind];
      mOneLineScript->m_strOneLine[ind].TrimRight("\r\n");
      mOneLineScript->m_strOneLine[ind].Replace("\r\n", ";");
    }
  }
  if (!fromDialog)
    mOneLineScript->UpdateData(false);
}

// This is called on startup or after reading settings
void CMacroProcessor::OpenOrJustCloseOneLiners(bool reopen)
{
  if (reopen && !mOneLineScript) {
    OnScriptRunOneCommand();
  } else if (!reopen && mOneLineScript) {
    mOneLineScript->DestroyWindow();
    mOneLineScript = NULL;
  }
}

// Central place to determine if a macro is theoretically runnable
BOOL CMacroProcessor::MacroRunnable(int index)
{
  return !mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() &&
    !(mWinApp->mNavigator && mWinApp->mNavigator->StartedMacro()) &&
    !(mWinApp->mNavigator && mWinApp->mNavigator->GetPausedAcquire()) &&
    !mWinApp->NavigatorStartedTS() && !mScope->GetMovingStage() &&
    (index >= MAX_MACROS || !mMacros[index].IsEmpty() || mMacroEditer[index]);
}

void CMacroProcessor::OnMacroEnd()
{
  if (DoingMacro())
    Stop(false);
  else
    SetNonResumable();
}

void CMacroProcessor::OnUpdateMacroEnd(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(DoingMacro() ||
    (mWinApp->mNavigator && mWinApp->mNavigator->StartedMacro() && IsResumable()));
}

void CMacroProcessor::OnMacroStop()
{
  Stop(true);
}

// Allow stop through the menu if there is a macro-driven non Navigator tilt series
void CMacroProcessor::OnUpdateMacroStop(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(DoingMacro());
}

void CMacroProcessor::OnMacroResume()
{
  Resume();
}

void CMacroProcessor::OnUpdateMacroResume(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() &&
    !mWinApp->NavigatorStartedTS() && IsResumable());
}

void CMacroProcessor::OnMacroControls()
{
  CMacroControlDlg conDlg;
  conDlg.mControl = *mControl;
  if (conDlg.DoModal() == IDOK)
    *mControl = conDlg.mControl;
}

void CMacroProcessor::OnRunOnProgramStart()
{
  CString str = mWinApp->GetScriptToRunAtStart();
  if (!SelectScriptAtStartEnd(str, "at program startup"))
    mWinApp->SetScriptToRunAtStart(str);
}

void CMacroProcessor::OnUpdateRunOnProgramStart(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mWinApp->GetScriptToRunAtStart().IsEmpty() ? 0 : 1);
}

void CMacroProcessor::OnRunAtProgramEnd()
{
  CString str = mWinApp->GetScriptToRunAtEnd();
  if (!SelectScriptAtStartEnd(str, "at program end"))
    mWinApp->SetScriptToRunAtEnd(str);
}

void CMacroProcessor::OnUpdateRunAtProgramEnd(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mWinApp->GetScriptToRunAtEnd().IsEmpty() ? 0 : 1);
}

void CMacroProcessor::OnRunIfProgramIdle()
{
  CString str = mWinApp->GetScriptToRunOnIdle();
  int interval = mWinApp->GetIdleScriptIntervalSec();
  if (SelectScriptAtStartEnd(str, "periodically when program is idle"))
    return;
  mWinApp->SetScriptToRunOnIdle(str);
  if (KGetOneInt("Interval in seconds at which to run script:", interval))
    mWinApp->SetIdleScriptIntervalSec(interval);
}

void CMacroProcessor::OnUpdateRunIfProgramIdle(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mWinApp->GetScriptToRunOnIdle().IsEmpty() ? 0 : 1 && 
    mWinApp->GetIdleScriptIntervalSec() > 0);
}

// Common routine for opening the macro selector with the current selection and getting
// a new one, returns 1 if canceled
int CMacroProcessor::SelectScriptAtStartEnd(CString &name, const char *when)
{
  CMacroSelector dlg;
  dlg.m_strEntryText = "Select script to run " + CString(when);
  dlg.mMacroIndex = FindMacroByNameOrTextNum(name);
  dlg.mAddNone = true;
  if (dlg.DoModal() == IDCANCEL)
    return 1;
  name = "";
  if (dlg.mMacroIndex >= 0) {
    name = mMacNames[dlg.mMacroIndex];
    if (name.IsEmpty())
      name.Format("%d", dlg.mMacroIndex + 1);
  }
  return 0;
}

void CMacroProcessor::SaveStatusPanes(int macNum)
{
  for (int ind = 0; ind < 3; ind++)
    mWinApp->mMainFrame->GetStatusText(ind + 1, mSavedStatusPanes[ind]);
  if (macNum >= 0 && macNum < MAX_MACROS && mMacroEditer[macNum])
    mMacroEditer[macNum]->TransferMacro(true);
  mFocusedWndWhenSavedStatus = GetFocus();
}

// TASK HANDLERS AND ANCILLARY FUNCTIONS

// Check for conditions that macro may have started
int CMacroProcessor::TaskBusy()
{
  double diff, dose;
  int tbusy;
  EMimageBuffer *imBuf;
  DWORD waitResult;
  CString report;

  // If accumulating dose: do periodic reports, stop when done
  if (mDoingMacro && mDoseTarget > 0.) {
    dose = mWinApp->mScopeStatus.GetFullCumDose() - mDoseStart;

    if (dose < mDoseTarget) {
      if (dose >= mDoseNextReport) {
        diff = SEMTickInterval(mDoseTime);
        diff = (mDoseTarget - dose) * (diff / dose) / 60000.;
        report.Format("Dose so far %.1f electrons/A2, time remaining ~%.1f minutes",
          dose, diff);
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
        mDoseNextReport += mDoseTarget / mNumDoseReports;
      }
      Sleep(10);
      return 1;
    } else {
      mDoseTarget = 0.;
      report.Format("Accumulated dose = %.1f electrons/A2", dose);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      mWinApp->SetStatusText(SIMPLE_PANE, "");
      mWinApp->mScopeStatus.SetWatchDose(false);
      return 0;
    }
  }

  // Check if command is ready from external scripting, or if it finished/errored
  if (mRunningScrpLang && mScrpLangData.waitingForCommand) {

    // But first handle inserting an image in a buffer
    if (CPythonServer::mImArray) {
      mProcessImage->NewProcessedImage(&mImBufs[CPythonServer::mImBaseBuf],
        (short *)CPythonServer::mImArray, CPythonServer::mImType, CPythonServer::mImSizeX,
        CPythonServer::mImSizeY, CPythonServer::mMoreBinning, CPythonServer::mCapFlag,
        false, CPythonServer::mImToBuf);
      CPythonServer::mImArray = NULL;
      SetEvent(mScrpLangDoneEvent);
      SEMTrace('[', "signal function done after NewProcessedImage");
    }
    if (mScrpLangData.externalControl && mScrpLangData.disconnected) {
      AbortMacro();
      return 0;
    }
    if (mScrpLangData.commandReady) {
      return 0;
    }
    if (mScrpLangData.externalControl)
      return 1;
    tbusy = UtilThreadBusy(&mScrpLangThread);
    if (tbusy > 0)
      return 1;
    SEMTrace('[', "thread done %d", tbusy);
    mScrpLangData.threadDone = tbusy ? 1 : -1;
    if (tbusy && mScrpLangData.gotExceptionText)
      EnhancedExceptionToLog(mScrpLangData.strItems[0]);

    // If there was an error and it is not cleared by a successful non-exit, go back to
    // Abort
    if (mScrpLangData.errorOccurred == 1) {
      AbortMacro();
      return 0;
    }

    // If called from a regular macro, drop the call level and switch back to the macro
    if (mCalledFromSEMmacro) {
      LeaveCallLevel(true);
      if (tbusy == 0) {
        mCalledFromSEMmacro = false;
        mRunningScrpLang = false;
      } else {
        AbortMacro();
      }
    }
    return 0;
  }

  // If waiting for marker, evaluate whether a new one is present on the proper image
  if (mWaitingForMarker) {
    imBuf = mWinApp->GetActiveNonStackImBuf();
    if (imBuf->mTimeStamp == mMarkerTimeStamp && 
      imBuf->mCaptured == mMarkerImageCapFlag && imBuf->mHasUserPt && 
      (!mStartedWithMarker || fabs(imBuf->mUserPtX - mOldMarkerX) > 0.01 ||
        fabs(imBuf->mUserPtY - mOldMarkerY) > 0.01)) {
      mWaitingForMarker = false;
      mWinApp->SetStatusText(SIMPLE_PANE, "");
    } else {
      Sleep(10);
      return 1;
    }
  }

  // If sleeping, take little naps to avoid using lots of CPU
  if (mDoingMacro && mSleepTime > 0.) {
    diff = SEMTickInterval(mSleepStart);
    if (diff < mSleepTime) {
      if (mSleepTime - diff > 20.)
        Sleep(10);
      return 1;
    }
    mWinApp->SetStatusText(SIMPLE_PANE, "");
    mSleepTime = 0.;
  }

  // If ran a process, check if it is still running; if not, set the output values and
  // fall through to test other conditions
  if (mProcessThread) {
    if (UtilThreadBusy(&mProcessThread) > 0)
      return 1;
    SetReportedValues(sProcessExitStatus, sProcessErrno);
    report.Format("Shell command exited with status %d, error code %d",
      sProcessExitStatus, sProcessErrno);
    mWinApp->AppendToLog(report, mLogAction);
  }

  // If doing external process, report result and clean it up here
  if (mRanExtProcess) {
    waitResult = WaitForSingleObject(mWinApp->mExternalTools->mExtProcInfo.hProcess, 20);
    if (waitResult != WAIT_OBJECT_0)
      return 1;
    GetExitCodeProcess(mWinApp->mExternalTools->mExtProcInfo.hProcess,
      &mExtProcExitStatus);
    report.Format("%s exited with status %d", (LPCTSTR)mEnteredName, mExtProcExitStatus);
    if (mExtProcExitStatus == 1 && mRanCtfplotter) {
      report += "\r\nSee " + CString(_getcwd(NULL, _MAX_PATH)) +
        "\\ctfplotter.log for error messages";
      mSaveCtfplotGraph = 0;
    }
    SetReportedValues(mExtProcExitStatus);
    if (mExtProcExitStatus || !mRanCtfplotter) {
      mWinApp->AppendToLog(report, mLogAction);
    }
    CleanupExternalProcess();
  }

  return ((mScope->GetInitialized() && ((mMovedStage &&
    mScope->StageBusy() > 0) || (mExposedFilm && mScope->FilmBusy() > 0) ||
    (mMovedScreen && mScope->ScreenBusy()))) ||
    (mStartedLongOp && mScope->LongOperationBusy()) ||
    (mMovedPiezo && mWinApp->mPiezoControl->PiezoBusy() > 0) ||
    (mMovedAperture && mScope->GetMovingAperture()) ||
    (mRanGatanScript &&  mCamera->CameraBusy()) ||
    (mLoadingMap && mWinApp->mNavigator && mWinApp->mNavigator->GetLoadingMap()) ||
    (mMakingDualMap && mNavHelper->GetAcquiringDual()) ||
    (mAutoContouring && mNavHelper->mAutoContouringDlg->DoingAutoContour()) ||
    mWinApp->mShiftCalibrator->CalibratingIS() ||
    (mCamera->GetInitialized() && mCamera->CameraBusy() && 
    (mCamera->GetTaskWaitingForFrame() || 
    !(mUsingContinuous && mCamera->DoingContinuousAcquire()))) ||
    (mWinApp->mMontageController->DoingMontage() &&
      !mWinApp->mMontageController->GetRunningMacro()) ||
    mWinApp->mParticleTasks->GetDVDoingDewarVac() ||
    mFocusManager->DoingFocus() || mWinApp->mAutoTuning->DoingAutoTune() ||
    mShiftManager->ResettingIS() || mWinApp->mCalibTiming->Calibrating() ||
    mWinApp->mFilterTasks->RefiningZLP() || 
    (mSavedMultiShot && CMultiShotDlg::DoingAutoStepAdj()) ||
    mNavHelper->mHoleFinderDlg->DoingMultiMapHoles() ||
    mNavHelper->mHoleFinderDlg->GetFindingHoles() || mNavHelper->GetRealigning() ||
    (mWinApp->mComplexTasks->DoingTasks() && 
      !mWinApp->mParticleTasks->GetMSRunningMacro()) || 
    mWinApp->DoingRegisteredPlugCall()) ? 1 : 0;
}

// Clean up handles from external process, and kill it if it is still running, and close
// shared memory file
void CMacroProcessor::CleanupExternalProcess()
{
  if (mRanExtProcess && mWinApp->mExternalTools->mExtProcInfo.hProcess) {
    if (WaitForSingleObject(mWinApp->mExternalTools->mExtProcInfo.hProcess, 1)
      != WAIT_OBJECT_0)
      TerminateProcess(mWinApp->mExternalTools->mExtProcInfo.hProcess, 1);
    CloseHandle(mWinApp->mExternalTools->mExtProcInfo.hProcess);
    CloseHandle(mWinApp->mExternalTools->mExtProcInfo.hThread);
    mWinApp->mExternalTools->mExtProcInfo.hProcess = NULL;
  }
  if (mShrMemFile)
    iiDelete(mShrMemFile);
  mShrMemFile = NULL;
}

// If the program is not busy, set the flag for running an external script and start it
void CMacroProcessor::CheckAndSetupExternalControl(void)
{
  if (mWinApp->DoingTasks() || (mCamera->CameraBusy() && 
    !mCamera->DoingContinuousAcquire())) {
    mScrpLangData.externalControl = 0;
    return;
  }
  mScrpLangData.externalControl = 1;
  Run(0);
}

// To run a macro: set the current macro with index at start
void CMacroProcessor::Run(int which)
{
  int mac, ind, tryLevel = 0;
  double startTime;
  bool external = mScrpLangData.externalControl > 0;
  MacroFunction *func;
  CString *longMacNames = mWinApp->GetLongMacroNames();
  CString name;
  mRunningScrpLang = false;
  mCalledFromScrpLang = false;
  mCalledFromSEMmacro = false;
  mCallLevel = 0;
  if (!external) {
    if (mMacros[which].IsEmpty())
      return;

    PrepareForMacroChecking(which);

    // First check for an language script and get it composed
    ind = CheckForScriptLanguage(which, false);
    if (ind > 0)
      return;
  } else
    ind = -1;
  mLastAborted = true;
  mLastCompleted = false;
  if (ind < 0) {

    // For language script, make sure camera is stopped then set flag
    if (mCamera->DoingContinuousAcquire()) {
      mCamera->StopCapture(0);
      startTime = GetTickCount();
      while (SEMTickInterval(startTime) < 10000.) {
        if (!mCamera->CameraBusy())
          break;
        Sleep(100);
      }
      if (mCamera->CameraBusy()) {
        SEMMessageBox("Could not stop continuous camera acquisition; cannot proceed with"
          " script");
        mScrpLangData.externalControl = 0;
        return;
      }
    }
    mRunningScrpLang = true;
  }

  // Check the macro(s) for commands, number of arguments, etc.
  if (!mRunningScrpLang && CheckBlockNesting(which, -1, tryLevel))
    return;

  // Clear out wasCalled flags
  for (mac = 0; mac < MAX_TOT_MACROS; mac++) {
    for (ind = 0; ind < mFuncArray[mac].GetSize(); ind++) {
      func = mFuncArray[mac].GetAt(ind);
      func->wasCalled = false;
    }
    if (mac >= MAX_MACROS)
      mMacNames[mac] = "";
  }
  mCurrentMacro = which;
  mBlockLevel = -1;
  mBlockDepths[0] = -1;
  mTryCatchLevel = 0;
  mCallFunction[0] = NULL;
  mCurrentIndex = 0;
  mLastIndex = -1;
  mScriptNumFound = -1;
  mOnStopMacroIndex = -1;
  mExitAtFuncEnd = false;
  mLoopIndsAreLocal = false;
  mStartNavAcqAtEnd = false;
  mUseTempNavParams = false;
  mPackToLoadAtEnd = "";
  mConsetNums.clear();
  mConsetsSaved.clear();
  mChangedConsets.clear();
  mSavedSettingNames.clear();
  mSavedSettingValues.clear();
  mNewSettingValues.clear();
  mLDareasSaved.clear();
  mLDParamsSaved.clear();
  for (mac = 0; mac < MAX_LOWDOSE_SETS; mac++)
    mKeepOrRestoreArea[mac] = 0;
  ClearVariables();
  mUsingContinuous = false;
  mShowedScopeBox = false;
  mRetryReadOther = 0;
  mEmailOnError = "";
  mNoMessageBoxOnError = false;
  mSkipFrameAliCheck = false;
  mAlignWholeTSOnly = false;
  mDisableAlignTrim = false;
  mLastPluginCalled = "";
  mSaveCtfplotGraph = 0;
  mNeedClearTempMacro = -1;
  mBoxOnScopeText = "SerialEM message";
  mBoxOnScopeType = 0;
  mBoxOnScopeInterval = 0.;
  mNumRuns = 0;
  mNextParamSetForMont = 0;
  mRamperStarted = false;
  mBinningForCameraMatrix = 0;
  mTestNextMultiShot = 0;
  mAccumShiftX = 0.;
  mAccumShiftY = 0.;
  mAccumDiff = 0.;
  mMinDeltaFocus = 0;
  mMaxDeltaFocus = 0.;
  mMinAbsFocus = 0.;
  mMaxAbsFocus = 0.;
  mCumulRecordDose = -1.;
  mAreaRecordDoses.resize(1, -1.);
  mAreaForCumulDose = 0;
  mRunToolArgPlacement = 0;
  mNumTempMacros = 0;
  mParseQuotes = false;
  mNoLineWrapInMessageBox = false;
  mMonospacedMessageBox = false;
  mLogAction = LOG_OPEN_IF_CLOSED;
  mLogInfoAction = LOG_OPEN_IF_CLOSED;
  mLogErrAction = LOG_IGNORE;
  mStartClock = GetTickCount();
  mOverwriteOK = false;
  ClearGraphVariables();
  mVerbose = mInitialVerbose ? 1 : 0;
  if (external) {
    name = "External Script";
  } else {
    name = mMacNames[mCurrentMacro];
    if (name.IsEmpty())
      name.Format("Script #%d", mCurrentMacro + 1);
    if (!longMacNames[mCurrentMacro].IsEmpty())
      SetVariable("LONGNAME", longMacNames[mCurrentMacro], VARTYPE_REGULAR, -1, false);
  }
  SetVariable("SCRIPTNAME", name, VARTYPE_REGULAR, -1, false);
  CloseFileForText(-1);

  RunOrResume();
}

// Clear these variables before a script or after starting a graph
void CMacroProcessor::ClearGraphVariables()
{
  mGraphTypeList.clear();
  mGraphColumns.clear();
  mGraphSymbols.clear();
  mGraphAxisLabel = "";
  mGraphKeys.clear();
  mConnectGraph = false;
  mGraphVsOrdinals = 0;
  mGraphColorOpt = -1;
  mGraphXlogSqrt = 0;
  mGraphXlogBase = 0.;
  mGraphYlogSqrt = 0;
  mGraphYlogBase = 0.;
  mGraphXmin = EXTRA_NO_VALUE;
  mGraphXmax = EXTRA_NO_VALUE;
  mGraphYmin = EXTRA_NO_VALUE;
  mGraphYmax = EXTRA_NO_VALUE;
  mGraphSaveName = "";
}

// Do all the common initializations for running or restarting a macro
void CMacroProcessor::RunOrResume()
{
  int ind;
  mDoingMacro = true;
  mOpenDE12Cover = false;
  mStopAtEnd = false;
  mAskRedoOnResume = false;
  mTestScale = false;
  mTestMontError = false;
  mTestTiltAngle = false;
  mLastCompleted = false;
  mLastAborted = false;
  mLastTestResult = true;
  mInInitialSubEval = false;
  mRanCtfplotter = false;
  mCamera->SetTaskWaitingForFrame(false);
  mFrameWaitStart = -1.;
  mWaitingForMarker = false;
  mNumStatesToRestore = 0;
  mFocusToRestore = -999.;
  mFocusOffsetToRestore = -9999.;
  mDEframeRateToRestore = -1.;
  mDEcamIndToRestore = -1;
  mLDSetAddedBeamRestore = -1;
  mK3CDSmodeToRestore = -1;
  mRestoreGridLimits = false;
  mSavedFrameNameFormat = -1;
  mSavedFrameBaseName = "";
  mRestoreConsetAfterShot = false;
  mSuspendNavRedraw = false;
  mDeferLogUpdates = false;
  mDeferSettingsUpdate = false;
  mCropXafterShot = -1;
  mNextProcessArgs = "";
  mInputToNextProcess = "";
  mBeamTiltXtoRestore[0] = mBeamTiltXtoRestore[1] = EXTRA_NO_VALUE;
  mBeamTiltYtoRestore[0] = mBeamTiltYtoRestore[1] = EXTRA_NO_VALUE;
  mAstigXtoRestore[0] = mAstigXtoRestore[1] = EXTRA_NO_VALUE;
  mAstigYtoRestore[0] = mAstigYtoRestore[1] = EXTRA_NO_VALUE;
  B3DDELETE(mFileOptToRestore);
  B3DDELETE(mOtherFileOptToRestore);
  mCompensatedBTforIS = false;
  mKeyPressed = 0;
  if (mChangedConsets.size() > 0 && mCamWithChangedSets == mWinApp->GetCurrentCamera())
    for (ind = 0; ind < (int)B3DMIN(mConsetNums.size(), mChangedConsets.size());ind++)
      mConSets[mConsetNums[ind]] = mChangedConsets[ind];
  if (mSavedSettingNames.size()) {
    for (ind = 0; ind < (int)mSavedSettingNames.size(); ind++)
      mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()),
        mNewSettingValues[ind]);
    mWinApp->UpdateWindowSettings();
  }

  mNumCmdSinceAddIdle = mMaxCmdToLoopOnIdle + 1;
  mScrpLangData.waitingForCommand = 0;
  mScrpLangData.commandReady = 0;
  mScrpLangData.threadDone = 0;
  mScrpLangData.errorOccurred = 0;
  mScrpLangData.exitedFromWrapper = false;
  mLastPythonErrorLine = -1;
  mStoppedContSetNum = mCamera->DoingContinuousAcquire() - 1;
  mWinApp->UpdateBufferWindows();
  SetComplexPane();

  // Start the script language thread unless doing external control,
  // and set up waiting for command
  if (mRunningScrpLang) {
    mWinApp->mMacroProcessor->InitForNextCommand();
    StartRunningScrpLang();
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return;
  }

  if (mStoppedContSetNum >= 0) {
    mCamera->StopCapture(0);
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return;
  }
  mWinApp->mMacroProcessor->NextCommand(true);
}

// To start running external script language, clear out unneeded items and start thread
// if not under external control
void CMacroProcessor::StartRunningScrpLang(void)
{
  int ind;
  for (ind = MAX_SCRIPT_LANG_ARGS; ind < MAX_MACRO_TOKENS; ind++) {
    mItemEmpty[ind] = true;
    mStrItems[ind] = "";
    mItemDbl[ind] = 0.;
    mItemFlt[ind] = 0.;
    mItemInt[ind] = 0;
  }

  // Clear flags out for this run.  The function that starts external control is looking
  // for both that and waitingForCommand, so be sure to do that last
  mScrpLangData.commandReady = 0;
  mScrpLangData.disconnected = false;
  mScrpLangData.threadDone = 0;
  mScrpLangData.errorOccurred = 0;
  mScrpLangData.exitedFromWrapper = false;
  mScrpLangData.waitingForCommand = 1;
  if (!mScrpLangData.externalControl) {
    if (mScrpLangThread || mPyProcessHandle) {

      // Handle a lingering process by killing it, then sending the done event to release
      // the socket interface, and consuming that event if it wasn't needed
      TerminateScrpLangProcess();
      SetEvent(mScrpLangDoneEvent);
      SEMTrace('[', "signal done after killing process");
      Sleep(100);
      WaitForSingleObject(mScrpLangDoneEvent, 100);
      mScrpLangData.disconnected = false;
    }
    mScrpLangData.exitStatus = 0;
    mScrpLangThread = AfxBeginThread(RunScriptLangProc,
      (LPVOID)(LPCTSTR)(mMacroForScrpLang.IsEmpty() ? mMacros[mCurrentMacro] :
        mMacroForScrpLang),
      THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    mScrpLangThread->m_bAutoDelete = false;
    mScrpLangThread->ResumeThread();
  }
  mNumCmdSinceAddIdle = 2 * mMaxCmdToLoopOnIdle;
  mLoopInOnIdle = false;
}

// Set the macro name or message in complex pane
void CMacroProcessor::SetComplexPane(void)
{
  if (mScrpLangData.externalControl)
    mMacroPaneText.Format("EXTERNAL CONTROL");
  else if (mMacNames[mCurrentMacro].IsEmpty())
    mMacroPaneText.Format("DOING SCRIPT %d", mCurrentMacro + 1);
  else
    mMacroPaneText.Format("DOING %d: %s", mCurrentMacro + 1, mMacNames[mCurrentMacro]);
  mWinApp->SetStatusText(mWinApp->DoingTiltSeries() &&
    mWinApp->mTSController->GetRunningMacro() ? MEDIUM_PANE : COMPLEX_PANE,
    mMacroPaneText);
}

// A macro is resumable if there is a current one still, and its index is
// not at the end
BOOL CMacroProcessor::IsResumable()
{
  if (mDoingMacro || mCurrentMacro < 0)
    return false;
  return (mCurrentIndex < mMacros[mCurrentMacro].GetLength());
}

// Resume by setting flag and doing next command or repeating last
void CMacroProcessor::Resume()
{
  int lastind = mLastIndex;
  CString stopCom;
  if (!IsResumable())
    return;

  // If there was an error or incomplete action, let user choose whether to redo
  if (mAskRedoOnResume) {
    GetNextLine(&mMacros[mCurrentMacro], lastind, stopCom);
    if (AfxMessageBox("The script stopped on the following line:\n\n" + stopCom +
      "\nDo you want to repeat this command or not?",
      MB_YESNO | MB_ICONQUESTION) == IDYES)
      mCurrentIndex = mLastIndex;
  }
  RunOrResume();
}

// Stop either now or at an ending place
void CMacroProcessor::Stop(BOOL ifNow)
{
  if (ifNow) {
    if (!mWinApp->mCameraMacroTools.GetUserStop() && TestTryLevelAndSkip(NULL))
      return;
    if (!mWinApp->mCameraMacroTools.GetUserStop() && mRunningScrpLang &&
      mNoMessageBoxOnError) {
      SuspendMacro(mScrpLangData.disconnected);
      return;
    }
    if (TestAndStartFuncOnStop())
      return;
    if (mProcessThread && UtilThreadBusy(&mProcessThread) > 0)
      UtilThreadCleanup(&mProcessThread);

    // If we are running Python, kill it now
    if (mScrpLangThread && UtilThreadBusy(&mScrpLangThread) > 0) {
      mScrpLangThread->SuspendThread();
      TerminateScrpLangProcess();
      UtilThreadCleanup(&mScrpLangThread);

    }

    // Mark external as disconnected by set error as user stop so this case is recognized
    if (mRunningScrpLang && mScrpLangData.externalControl) {
      mScrpLangData.disconnected = true;
      mScrpLangData.errorOccurred = SCRIPT_USER_STOP;
    }
      
    if (mDoingMacro && mLastIndex >= 0)
      mAskRedoOnResume = true;
    SuspendMacro(mScrpLangData.disconnected);
  } else
    mStopAtEnd = true;
}

// If there is a stop function registered, set up to run it
int CMacroProcessor::TestAndStartFuncOnStop(void)
{
  if (mOnStopMacroIndex >= 0) {
    mCurrentMacro = mOnStopMacroIndex;
    mCurrentIndex = mOnStopLineIndex;
    mOnStopMacroIndex = -1;
    mExitAtFuncEnd = true;
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return 1;
  }
  return 0;
}

// Tests whether we are in a Try block at any level, and skips to the appropriate catch
int CMacroProcessor::TestTryLevelAndSkip(CString *mess)
{
  CString str;
  int i, numPops, delTryLevel;
  if (mTryCatchLevel > 0) {

    // If running Python, first call abort to get the exit sent
    if (mRunningScrpLang && mCalledFromSEMmacro) {
      AbortMacro();
      return 1;
    }

    // If end of script or function is reached, then drop the call level
    while (SkipToBlockEnd(SKIPTO_CATCH, str, &numPops, &delTryLevel)) {
      mTryCatchLevel += delTryLevel;
      if (mCallLevel > 0) {
        LeaveCallLevel(true);
      } else {
        SEMMessageBox("Terminating script because no CATCH block was found for "
          "processing an error or THROW");
        mTryCatchLevel = 0;
        return 0;
      }
    }

    // If the error is occurring in a statement that starts a block, then raise the 
    // blocklevel before dropping it by number of pops
    if (mInInitialSubEval) {
      if (!mStrItems[0].CompareNoCase("IF") || !mStrItems[0].CompareNoCase("LOOP") ||
        !mStrItems[0].CompareNoCase("DOLOOP") || !mStrItems[0].CompareNoCase("TRY"))
        mBlockLevel++;
    }
    for (i = 0; i < numPops && mBlockLevel >= 0; i++) {
      ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
    mLastIndex = -1;
    mTryCatchLevel += delTryLevel;
    str = "Jumping to \"CATCH\" error handler";
    if (mess && !mess->IsEmpty())
      str += ": " + *mess;
    if (!mNoCatchOutput[B3DMAX(0, mTryCatchLevel)])
      mWinApp->AppendToLog(str);
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return 1;
  }
  mTryCatchLevel = 0;
  return 0;
}

///////////////////////////////////////////////////////

// Abort means end the script and is not pejorative - but call with true when ending
void CMacroProcessor::AbortMacro(bool ending)
{
  SuspendMacro(ending ? -1 : 1);
}

// Suspend or permanently end the script
void CMacroProcessor::SuspendMacro(int abort)
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  int probe, ind;
  FileOptions *fileOpt;
  bool restoreArea = false;
  float *gridLims = mNavHelper->GetGridLimits();
  mLoopInOnIdle = false;
  if (!mDoingMacro)
    return;
  SEMTrace('[', "In abort");

  // Intercept abort when doing external script, set error flag and set wait for command
  // Process user stop like any other exit, not like disconnect happened
  if ((mRunningScrpLang || mCalledFromScrpLang) && 
    (!mScrpLangData.threadDone || mScrpLangData.externalControl)) {
    if (mScrpLangData.disconnected && !(mScrpLangData.errorOccurred == SCRIPT_USER_STOP &&
      mScrpLangData.commandReady)) {
      mScrpLangData.externalControl = 0;
    } else {

      // Clean up from running regular scripts
      if (mCalledFromScrpLang) {
        mCalledFromScrpLang = false;
        mRunningScrpLang = true;
        mCurrentMacro = 0;
        mBlockLevel = -1;
        mBlockDepths[0] = -1;
        mTryCatchLevel = 0;
        for (ind = mCallLevel; ind >= 0; ind--)
          if (mCallFunction[ind])
            mCallFunction[ind]->wasCalled = false;
        mCallLevel = 0;
        mCallFunction[0] = NULL;
        mOnStopMacroIndex = -1;
      }

      // Process true error
      if (mScrpLangData.errorOccurred != SCRIPT_NORMAL_EXIT &&
        mScrpLangData.errorOccurred != SCRIPT_EXIT_NO_EXC &&
        mScrpLangData.errorOccurred != SCRIPT_USER_STOP) {
        mScrpLangData.errorOccurred = 1;
        mScrpLangData.highestReportInd = 0;
        mScrpLangData.reportedStrs[0] = mWinApp->mTSController->GetLastNoBoxMessage();
        mScrpLangData.repValIsString[0] = true;
        mScrpLangData.reportedVals[0] = 0.;
      }
      mScrpLangData.waitingForCommand = 1;
      mScrpLangData.commandReady = 0;
      if (mScrpLangData.disconnected)
        mScrpLangData.externalControl = 0;
      SetEvent(mScrpLangDoneEvent);
      SEMTrace('[', "signal function done after Abort");
      if (mCalledFromSEMmacro && (mScrpLangData.errorOccurred != 1))
        mCalledFromSEMmacro = false;
      if (!mScrpLangData.externalControl || mScrpLangData.errorOccurred == 1) {
        mWinApp->AddIdleTask(TASK_MACRO_RUN, 0, 0);
        return;
      }
    }
  }
  if (mRunningScrpLang && mCalledFromSEMmacro)
    mRunningScrpLang = false;

  if (mRunningScrpLang && (mScrpLangData.threadDone > 0 ||
    mScrpLangData.exitedFromWrapper))
    SEMMessageBox("Error running Python script; see log for information");

  CleanupExternalProcess();
  if (!mWinApp->mCameraMacroTools.GetUserStop() && TestTryLevelAndSkip(NULL))
    return;
  if (abort > 0 && TestAndStartFuncOnStop())
    return;
  if (mNeedClearTempMacro >= 0) {
    mMacros[mNeedClearTempMacro] = "";
    abort = 1;
  }

  // restore user settings
  for (ind = 0; ind < (int)mSavedSettingNames.size(); ind++)
    mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()), 
      mSavedSettingValues[ind]);
  if (mSavedSettingNames.size() || mDeferSettingsUpdate)
    mWinApp->UpdateWindowSettings();
  for (ind = 0; ind < MAX_LOWDOSE_SETS; ind++)
    if (mKeepOrRestoreArea[ind] > 0)
      restoreArea = true;

  // Pop control set that was to be restored after shot before any other restores
  if (mRestoreConsetAfterShot) {
    mRestoreConsetAfterShot = false;
    ind = mConsetNums.back();
    mConSets[ind] = mConsetsSaved.back();
    mConsetNums.pop_back();
    mConsetsSaved.pop_back();
  }

  // Reenable nav updates and log printing
  if (mSuspendNavRedraw && mWinApp->mNavigator) {
    mSuspendNavRedraw = false;
    mWinApp->mNavigator->FillListBox(false, true);
    mWinApp->mNavigator->Redraw();
  }
  if (mDeferLogUpdates && mWinApp->mLogWindow) {
    mDeferLogUpdates = false;
    mWinApp->mLogWindow->FlushDeferredLines();
  }
  mScope->SetDoNextFEGFlashHigh(false);

  // Restore other things and make it non-resumable as they have no mechanism to resume
  if (abort || mNumStatesToRestore > 0 || restoreArea || mFileOptToRestore || 
    mOtherFileOptToRestore) {
    mCurrentMacro = -1;
    mLastAborted = !mLastCompleted;
    if (mNumStatesToRestore) {
      if (mFocusToRestore > -2.)
        mScope->SetFocus(mFocusToRestore);
      if (mFocusOffsetToRestore > -9000.)
        mFocusManager->SetDefocusOffset(mFocusOffsetToRestore);
      if (mDEframeRateToRestore > 0 || mDEcamIndToRestore >= 0) {
        camParams[mDEcamIndToRestore].DE_FramesPerSec = mDEframeRateToRestore;
        mWinApp->mDEToolDlg.UpdateSettings();
      }
      probe = mScope->GetProbeMode();
      if (mBeamTiltXtoRestore[probe] > EXTRA_VALUE_TEST) {
        mScope->SetBeamTilt(mBeamTiltXtoRestore[probe], mBeamTiltYtoRestore[probe]);
        if (mFocusManager->DoingFocus())
          mFocusManager->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
        if (mWinApp->mAutoTuning->DoingZemlin() || 
          mWinApp->mAutoTuning->GetDoingCtfBased())
          mWinApp->mAutoTuning->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
        if (mWinApp->mParticleTasks->DoingMultiShot())
          mWinApp->mParticleTasks->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
      }
      if (mAstigXtoRestore[probe] > EXTRA_VALUE_TEST) {
        mScope->SetObjectiveStigmator(mAstigXtoRestore[probe], mAstigYtoRestore[probe]);
        if (mWinApp->mParticleTasks->DoingMultiShot())
          mWinApp->mParticleTasks->SetBaseAstig(mAstigXtoRestore[probe],
            mAstigYtoRestore[probe]);
      }
      if (mBeamTiltXtoRestore[1 - probe] > EXTRA_VALUE_TEST || 
        mAstigXtoRestore[1 - probe] > EXTRA_VALUE_TEST) {
        mWinApp->AppendToLog("Switching probe modes to restore beam tilt and/or "
          "astigmatism value");
        mScope->SetProbeMode(1 - probe);
        if (mBeamTiltXtoRestore[1 - probe] > EXTRA_VALUE_TEST)
          mScope->SetBeamTilt(mBeamTiltXtoRestore[1 - probe],
            mBeamTiltYtoRestore[1 - probe]);
        if (mAstigXtoRestore[1 - probe] > EXTRA_VALUE_TEST)
          mScope->SetObjectiveStigmator(mAstigXtoRestore[1 - probe], 
            mAstigYtoRestore[1 - probe]);
        mScope->SetProbeMode(probe);
      }
      if (mLDSetAddedBeamRestore >= 0)
        mWinApp->mLowDoseDlg.SetBeamShiftButton(mLDSetAddedBeamRestore > 0);
      if (mK3CDSmodeToRestore >= 0)
        mCamera->SetUseK3CorrDblSamp(mK3CDSmodeToRestore > 0);
      if (mSavedFrameNameFormat >= 0) {
        if (!mSavedFrameBaseName.IsEmpty())
          mCamera->SetFrameBaseName(mSavedFrameBaseName);
        mCamera->SetFrameNameFormat(mSavedFrameNameFormat);
      }
      if (mRestoreGridLimits) {
        for (ind = 0; ind < 4; ind++)
          gridLims[ind] = mGridLimitsToRestore[ind];
      }
    }
    if (mFileOptToRestore) {
      fileOpt = mDocWnd->GetFileOpt();
      *fileOpt = *mFileOptToRestore;
      B3DDELETE(mFileOptToRestore);
    }
    if (mOtherFileOptToRestore) {
      fileOpt = mDocWnd->GetOtherFileOpt();
      *fileOpt = *mOtherFileOptToRestore;
      B3DDELETE(mOtherFileOptToRestore);
    }
    mSavedSettingNames.clear();
    mSavedSettingValues.clear();
    mNewSettingValues.clear();
    if (restoreArea)
      RestoreLowDoseParams(-2);
    mCamera->SetDoseAdjustmentFactor(0.);
  }

  // restore camera sets, clear if non-resumable
  RestoreCameraSet(-1, mCurrentMacro < 0);
  if (mCurrentMacro < 0 && mUsingContinuous)
    mCamera->ChangePreventToggle(-1);
  if (mCurrentMacro < 0)
    CloseFileForText(-1);
  mDoingMacro = false;
  if (mCamera->DoingTiltSums())
    mCamera->CleanUpFromTiltSums();
  if (mStoppedContSetNum >= 0)
    mCamera->InitiateCapture(mStoppedContSetNum);
  if (abort) {
    for (ind = MAX_MACROS + MAX_ONE_LINE_SCRIPTS; ind < MAX_TOT_MACROS; ind++)
      ClearFunctionArray(ind);
  }
  RestoreMultiShotParams();
  mRunningScrpLang = false;
  mCalledFromScrpLang = false;
  ResetEvent(mScrpLangDoneEvent);
  SEMTrace('[', "reset all done ending macro");
  mMacroForScrpLang = "";
  mScrpLangData.externalControl = 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(mWinApp->DoingTiltSeries() &&
    mWinApp->mTSController->GetRunningMacro() ? MEDIUM_PANE : COMPLEX_PANE, 
    (IsResumable() && mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()) ?
    "STOPPED NAV SCRIPT" : "");
  if (!mCamera->CameraBusy())
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  mCamera->StopFrameTSTilting();
  mWinApp->mScopeStatus.SetWatchDose(false);
  mDocWnd->SetDeferWritingFrameMdoc(false);
  if (!mPackToLoadAtEnd.IsEmpty())
    LoadNewScriptPackage(mPackToLoadAtEnd, mSaveCurrentPack);
  for (ind = 0; ind < 3; ind++) {
    if (!mSavedStatusPanes[ind].IsEmpty()) {
      mWinApp->mMainFrame->SetStatusText(ind + 1, mSavedStatusPanes[ind]);
      mSavedStatusPanes[ind] = "";
    }
  }
  if (mFocusedWndWhenSavedStatus) {
    SetFocus(mFocusedWndWhenSavedStatus);
    mFocusedWndWhenSavedStatus = NULL;
  }
}


void CMacroProcessor::SetIntensityFactor(int iDir)
{
  double angle = mScope->GetTiltAngle();
  double increment = iDir * mScope->GetIncrement();
  mIntensityFactor = cos(angle * DTOR) / cos((angle + increment) * DTOR);
}

// Get the next line or multiple lines if they end with backslash
void CMacroProcessor::GetNextLine(CString * macro, int & currentIndex, CString &strLine, 
  bool commentOK)
{
  int index, testInd;
  strLine = "";
  CString temp, trimmed;
  for (;;) {

    // Find next linefeed
    index = macro->Find('\n', currentIndex);
    if (index < 0) {

      // If none, get rest of string, set index past end
      temp = macro->Mid(currentIndex);
      trimmed = temp;
      trimmed.TrimLeft();
      if (!trimmed.GetLength() || trimmed.GetAt(0) != '#' || commentOK)
        strLine += temp;
      currentIndex = macro->GetLength();
      break;
    } else {

      // Set index past end of line then test for space backslash after skipping a \r
      index++;
      temp = macro->Mid(currentIndex, index - currentIndex);
      trimmed = temp;
      trimmed.TrimLeft();
      if (trimmed.GetLength() && trimmed.GetAt(0) == '#' && !commentOK) {
        currentIndex = index;
        if (index >= macro->GetLength())
          break;
      } else {
        testInd = index - 2;
        if (testInd >= 0 && macro->GetAt(testInd) == '\r')
          testInd--;
        if (testInd > 0 && macro->GetAt(testInd) == '\\' &&
          macro->GetAt(testInd - 1) == ' ') {

          // To continue, add the line through the space then set the index to next line
          strLine += macro->Mid(currentIndex, testInd - currentIndex);
          currentIndex = index;
        } else {

          // otherwise get the whole line and break out
          strLine += temp;
          currentIndex = index;
          break;
        }
      }
    }
  }
}

// Scan the macro for a name, set it in master name list, and return 1 if it changed
// Scan the given macro string, or the master macro if NULL
int CMacroProcessor::ScanForName(int macroNumber, CString *macro)
{
  CString newName = "";
  CString longName = "";
  CString strLine, prefix, argName, strItem[MAX_MACRO_TOKENS];
  CString *longMacNames = mWinApp->GetLongMacroNames();
  MacroFunction *funcP;
  mParamIO = mWinApp->mParamIO;
  int scriptLang = 0;
  int currentIndex = 0, lastIndex = 0;
  if ((macroNumber >= MAX_MACROS && macroNumber < MAX_MACROS + MAX_ONE_LINE_SCRIPTS) ||
    (mDoingMacro && mCallLevel > 0))
      return 0;
  if (!macro)
    macro = &mMacros[macroNumber];
  mReadOnlyStart[macroNumber] = -1;
  ClearFunctionArray(macroNumber);
  while (currentIndex < macro->GetLength()) {
    GetNextLine(macro, currentIndex, strLine, true);
    if (!lastIndex && strLine.Find("#!") == 0)
      scriptLang = 1;
    if (!strLine.IsEmpty()) {
      mParamIO->ParseString(strLine, strItem, MAX_MACRO_TOKENS, false, scriptLang);
      if (scriptLang && strItem[0].GetAt(0) == '#')
        prefix = "#";
      if ((strItem[0].CompareNoCase(prefix + "MacroName") == 0 ||
        strItem[0].CompareNoCase(prefix + "ScriptName") == 0) && !strItem[1].IsEmpty()) {
        mParamIO->StripItems(strLine, 1, newName, false, scriptLang);
      } else if (strItem[0].CompareNoCase(prefix + "LongName") == 0 &&
        !strItem[1].IsEmpty()) {
        mParamIO->StripItems(strLine, 1, longName, false, scriptLang);

      // Put all the functions in there that won't be eliminated by minimum argument
      // requirement and let pre-checking complain about details
      } else if (strItem[0].CompareNoCase(prefix + "ReadOnlyUnlessAdmin") == 0) {
        mReadOnlyStart[macroNumber] = lastIndex;
      } else if (strItem[0].CompareNoCase("Function") == 0 && !strItem[1].IsEmpty() && 
        !scriptLang) {
        funcP = new MacroFunction;
        funcP->name = strItem[1];
        funcP->numNumericArgs = strItem[2].IsEmpty() ? 0 : atoi((LPCTSTR)strItem[2]);
        funcP->ifStringArg = !strItem[3].IsEmpty() && strItem[3] != "0";
        funcP->startIndex = currentIndex;  // Trust that this can be used for call
        funcP->wasCalled = false;
        for (int arg = 0; arg < funcP->numNumericArgs + (funcP->ifStringArg ? 1 : 0);
          arg++) {
          if (strItem[4 + arg].IsEmpty())
            argName.Format("ARGVAL%d", arg + 1);
          else
            argName = strItem[4 + arg].MakeUpper();
          funcP->argNames.Add(argName);
        }
        mFuncArray[macroNumber].Add(funcP);
      }
    }
    lastIndex = currentIndex;
  }
  longMacNames[macroNumber] = longName;
  if (newName != mMacNames[macroNumber]) {
    mMacNames[macroNumber] = newName;
    return 1;
  }
  return 0;
}

// Clear out all the functions in a specific macro or all macros
void CMacroProcessor::ClearFunctionArray(int index)
{
  int start = index < 0 ? 0 : index;
  int end = index < 0 ? MAX_TOT_MACROS - 1 : index;
  for (int mac = start; mac <= end; mac++) {
    for (int ind = (int)mFuncArray[mac].GetSize() - 1; ind >= 0; ind--)
      delete mFuncArray[mac].GetAt(ind);
    mFuncArray[mac].RemoveAll();
  }
}

// Find macro called on this line; unload and scan names if scanning is true
int CMacroProcessor::FindCalledMacro(CString strLine, bool scanning, CString useName)
{
  CString strCopy;
  int index, index2;
  if (useName.IsEmpty())
    mParamIO->StripItems(strLine, 1, strCopy);
  else
    strCopy = useName;

  // Look for the name
  index = -1;
  for (index2 = 0; index2 < MAX_TOT_MACROS; index2++) {
    if (index >= MAX_MACROS && index < MAX_MACROS + MAX_ONE_LINE_SCRIPTS)
      continue;
    ScanMacroIfNeeded(index2, scanning);
    if (strCopy == mMacNames[index2]) {
      if (index >= 0) {
        SEMMessageBox("Two scripts have a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
        return -1;
      }
      index = index2;
    }
  }
  if (index < 0)
    SEMMessageBox("No script has a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
  return index;
}

// Look up a macro given its name or number in 1-based text form
int CMacroProcessor::FindMacroByNameOrTextNum(CString name)
{
  CString str;
  int num, index;
  if (name.IsEmpty())
    return -1;
  for (index = 0; index < MAX_MACROS; index++)
    ScanMacroIfNeeded(index, true);
  num = atoi((LPCTSTR)name);
  if (num > 0 && num <= MAX_MACROS) {
    str.Format("%d", num);
    if (str == name)
      return num - 1;
  }
  for (index = 0; index < MAX_MACROS; index++)
    if (name == mMacNames[index])
      return index;
  return -1;
}

// Make sure macro is unloaded and name scanned for any macros being edited
void CMacroProcessor::ScanMacroIfNeeded(int index, bool scanning)
{
  if (index < MAX_MACROS && mMacroEditer[index] && scanning) {
    for (int ix0 = 0; ix0 <= mCallLevel; ix0++)
      if (mCallMacro[ix0] == index)
        return;
    mMacroEditer[index]->TransferMacro(true);
    ScanForName(index, &mMacros[index]);
  }
}

// Given the parsed line that calls a function, find the macro it is in and the function
// structure in that macro's array, and index of first argument if any
MacroFunction *CMacroProcessor::FindCalledFunction(CString strLine, bool scanning, 
  int &macroNum, int &argInd, int currentMac)
{
  int colonLineInd = strLine.Find("::");
  int num, mac, ind, loop, colonItemInd, start = 0, end = MAX_TOT_MACROS - 1;
  CString funcName, macName, strItems[MAX_MACRO_TOKENS];
  MacroFunction *func, *retFunc = NULL;
  macroNum = -1;
  if (currentMac < 0)
    currentMac = mCurrentMacro;

  // Reparse the line so everthing is original case
  mParamIO->ParseString(strLine, strItems, MAX_MACRO_TOKENS);

  // No colon, name is first item and need to search all
  if (colonLineInd < 0) {
    funcName = strItems[1];
    argInd = 2;
  } else {

    // colon: first find the item with it (has to be there)
    for (ind = 1; ind < MAX_MACRO_TOKENS; ind++) {
      colonItemInd = strItems[ind].Find("::");
      if (colonItemInd == 0) {
        AfxMessageBox("There must be a script name or number before the :: in this "
          "call:\n\n" + strLine, MB_EXCLAME);
        return NULL;
      }
      if (colonItemInd == strItems[ind].GetLength() - 2) {
        AfxMessageBox("There must be a function name after the :: in this "
          "call:\n\n" + strLine, MB_EXCLAME);
        return NULL;
      }

      // If there is a legal colon and this is the second item, first try to convert the
      // macro name to a number and see if that works
      if (colonItemInd > 0) {
        if (ind == 1) {
          macName = strItems[ind].Left(colonItemInd);
          num = atoi((LPCTSTR)macName);
          funcName.Format("%d", num);
          if (funcName == macName) {
            macroNum = num - 1;
            ScanMacroIfNeeded(macroNum, scanning);
          }
        }

        // If not, go ahead and look up the name
        if (macroNum < 0)
          macroNum = FindCalledMacro(strLine.Left(colonLineInd), scanning);
        if (macroNum < 0)
          return NULL;
        start = macroNum;
        end = macroNum;
        funcName = strItems[ind].Mid(colonItemInd + 2);
        argInd = ind + 1;
        break;
      }
    }
  }

  // Now search one or all function arrays for the function name, make sure only one
  // Search current macro first if there is no colon
  for (loop = (colonLineInd < 0 ? 0 : 1); loop < 2; loop++) {
    for (mac = (loop ? start : currentMac); mac <= (loop ? end : currentMac); mac++)
    {
      if (colonLineInd < 0 || !loop)
        ScanMacroIfNeeded(mac, scanning);
      for (ind = 0; ind < mFuncArray[mac].GetSize(); ind++) {
        func = mFuncArray[mac].GetAt(ind);
        if (func->name == funcName) {
          if (retFunc) {
            if (macroNum == mac)
              AfxMessageBox("Two functions in a script have the same name for this "
              "call:\n\n" + strLine,  MB_EXCLAME);
            else
              AfxMessageBox("Two functions have the same name and you need \nto specify "
              "the script name or number for this call:\n\n" + strLine,  MB_EXCLAME);
            return NULL;
          }
          retFunc = func;
          macroNum = mac;
        }
      }
    }
    if (!loop && retFunc)
      break;
  }
  if (!retFunc) 
    AfxMessageBox("No function has a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
  return retFunc;
}

// Sets a variable of the given name to the value, with the type and index.  Pass index
// of -1 to assign it the current macro index.
// The variable must not exist if mustBeNew is true; returns true for error and fills
// in errStr if it is non-null
bool CMacroProcessor::SetVariable(CString name, CString value, int type, 
  int index, bool mustBeNew, CString *errStr, CArray<ArrayRow, ArrayRow> *rowsFor2d)
{
  int ind, leftInd, rightInd, rightInd2, arrInd, rowInd, numElements;
  int *oldNumElemPtr;
  CString *oldValuePtr;
  CString temp;
  bool isNumeric = type >= VARTYPE_ADD_FOR_NUM;
  if (isNumeric)
    type -= VARTYPE_ADD_FOR_NUM;

  name.MakeUpper();
  if (name[0] == '$' || value[0] == '$') {
    if (errStr)
      errStr->Format("The %s still starts with a $ after substituting variables\r\n"
      "Is it an undefined variable?", name[0] == '$' ? "variable name" : "value");
    return true;
  }
  if (WordIsReserved(name))
    PrintfToLog("WARNING: assigning to a variable %s, which is named the same as a "
      "reserved keyword", (LPCTSTR)name);
  if (index == -1)
    index = mCurrentMacro;

  // Count number of elements in new value
  ind = 0;
  numElements = value.IsEmpty() ? 0 : 1;
  while ((ind = value.Find("\n", ind + 1)) >= 0)
    numElements++;

  // See if this is an assignment to an array element and check legality
  leftInd = name.Find('[');
  rightInd = name.Find(']');
  if (leftInd > 0 && FindAndCheckArrayIndexes(name, leftInd, rightInd, rightInd2, errStr)
    < 0)
      return true;
    
  if ((leftInd < 0 && rightInd >= 0) || leftInd == 0 ||
    (rightInd >= 0 && ((rightInd2 <= 0 && rightInd < name.GetLength() - 1) || 
    (rightInd2 > 0 && rightInd2 < name.GetLength() - 1)))) {
      if (errStr)
        errStr->Format("Illegal use or placement of [ and/or ] in variable name %s", 
          (LPCTSTR)name);
      return true;
  }

  // Get actual variable name to look up, make sure it exists for element assignment
  temp = name;
  if (leftInd > 0)
    temp = name.Left(leftInd);
  Variable *var = LookupVariable(temp, ind);
  if (var && type == VARTYPE_LOCAL && var->type != VARTYPE_LOCAL)
    var = NULL;
  if (leftInd > 0 && !var) {
    if (errStr)
      errStr->Format("Variable %s must be defined to assign to an array element", 
      (LPCTSTR)temp);
    return true;
  }

  // Define a new variable
  if (!var) {
    var = new Variable;
    var->name = name;
    var->value = value;
    var->numElements = numElements;
    var->type = type;
    var->isNumeric = isNumeric;
    var->callLevel = mCallLevel;
    var->index = index;
    var->definingFunc = mCallFunction[mCallLevel];
    var->rowsFor2d = rowsFor2d;
    mVarArray.Add(var);
    return false;
  }

  // Clear out any existing 2D array if not assigning to an array element
  if (leftInd < 0) {
    delete var->rowsFor2d;
    var->rowsFor2d = rowsFor2d;
  }

  // There is a set error if it must be new or if a user variable is using the name
  // of an existing variable of another type
  if (mustBeNew) {
    if (errStr)
      errStr->Format("Variable %s must not already exist and does", (LPCTSTR)name);
    return true;
  }
  if (type != VARTYPE_INDEX && var->type == VARTYPE_INDEX) {
    if (errStr)
      errStr->Format("Variable %s is a loop index variable and cannot be assigned to", 
      (LPCTSTR)name);
    return true;
  }
  if ((type == VARTYPE_PERSIST && var->type != VARTYPE_PERSIST) ||
    (type != VARTYPE_PERSIST && var->type == VARTYPE_PERSIST)) {
      if (errStr)
        errStr->Format("Variable %s is already defined as %spersistent and cannot"
          " be reassigned as %spersistent", (LPCTSTR)name, 
          (type != VARTYPE_PERSIST) ? "" : "non-", type == VARTYPE_PERSIST ? "" : "non-");
    return true;
  }

  var->isNumeric = false;
  if (leftInd > 0) {
    oldNumElemPtr = &var->numElements;
    oldValuePtr = &var->value;

    // For 2D array assignment, get row index, element and value pointers for that row
    if (rightInd2 > 0) {
      rowInd = ConvertArrayIndex(name, leftInd, rightInd, var->name, 
        (int)var->rowsFor2d->GetSize(), errStr);
      if (!rowInd)
        return true;
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(rowInd - 1);
      oldNumElemPtr = &arrRow.numElements;
      oldValuePtr = &arrRow.value;
      leftInd = rightInd + 1;
      rightInd = rightInd2;
    } 

    // If assigning to an array element, get the index value 
    arrInd = ConvertArrayIndex(name, leftInd, rightInd, var->name, 
      (var->rowsFor2d && rightInd2 <= 0) ? (int)var->rowsFor2d->GetSize() : 
      *oldNumElemPtr, errStr);
    if (!arrInd)
      return true;
    if (var->rowsFor2d && rightInd2 <= 0) {

      // If assigning (array) to row of 2D array, just set it into the value
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
      arrRow.numElements = numElements;
      arrRow.value = value;
    } else {

      // For assignment to a single element, get the starting and
      // ending (+1) indexes in the value of that element
      FindValueAtIndex(*oldValuePtr, arrInd, leftInd, rightInd);

      // Substitute the new value for the existing element and adjust numElements
      temp = "";
      if (leftInd)
        temp = oldValuePtr->Left(leftInd);
      temp += value;
      ind = oldValuePtr->GetLength();
      if (rightInd < ind)
        temp += oldValuePtr->Mid(rightInd, ind - rightInd);
      *oldValuePtr = temp;
      *oldNumElemPtr += numElements - 1;
    }
  } else {

    // Regular assignment to variable value
    var->value = value;
    var->numElements = numElements;
    var->isNumeric = isNumeric;
  }

  // If value is empty, remove a persistent variable
  if (type == VARTYPE_PERSIST && value.IsEmpty()) {
    delete var->rowsFor2d;
    delete var;
    mVarArray.RemoveAt(ind);
  }
  return false;
}

// Overloaded function can be passed a double instead of string value
bool CMacroProcessor::SetVariable(CString name, double value, int type,  
  int index, bool mustBeNew, CString *errStr, CArray<ArrayRow, ArrayRow> *rowsFor2d)
{
  CString str;
  str.Format("%f", value);
  UtilTrimTrailingZeros(str);
  return SetVariable(name, str, type, index, mustBeNew, errStr, rowsFor2d);
}

// Globally callable functions; can only be used to set regular or persistent variables.
// Return true for error
bool SEMSetVariableWithStr(CString name, CString value, bool persistent, bool mustBeNew,
  CString *errStr)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!winApp->mMacroProcessor->DoingMacro()) {
    if (errStr)
      *errStr = "A script must be running to define a variable";
    return true;
  }
  return winApp->mMacroProcessor->SetVariable(name, value, 
    persistent ? VARTYPE_PERSIST : VARTYPE_REGULAR, -1, mustBeNew, errStr);
}

bool SEMSetVariableWithDbl(CString name, double value, bool persistent, bool mustBeNew,
  CString *errStr)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!winApp->mMacroProcessor->DoingMacro()) {
    if (errStr)
      *errStr = "A script must be running to define a variable";
    return true;
  }
  return winApp->mMacroProcessor->SetVariable(name, value,
    persistent ? VARTYPE_PERSIST : VARTYPE_REGULAR, -1, mustBeNew, errStr);
}

// Test whether a local variable is already defined as global, issue message and 
// return 1 if it is, return 0 if not defined, -1 if already local
int CMacroProcessor::LocalVarAlreadyDefined(CString & item, CString &strLine)
{
  int index2;
  CString mess;
  Variable *var = LookupVariable(item, index2);
  if (var && var->type != VARTYPE_LOCAL && var->callLevel == mCallLevel &&
    var->definingFunc == mCallFunction[mCallLevel] && (var->index == mCurrentMacro ||
      var->type == VARTYPE_INDEX || var->type == VARTYPE_REPORT)) {
    mess = "Variable " + item + " has already been defined as global"
      "\nin this script/function and cannot be made local in line:\n\n";
    ABORT_NORET_LINE(mess);
    return -1;
  }
  return var ? -1 : 0;
}

// Looks up a variable by name and returns pointer if found, NULL if not, and index in ind
// Looks first for a local variable at the current level and function, then for non-local
Variable *CMacroProcessor::LookupVariable(CString name, int &ind)
{
  Variable *var;
  int global;
  bool localVar;
  name.MakeUpper();

  // Loop twice, look for local variable first, then a global one
  for (global = 0; global < 2; global++) {
    for (ind = 0; ind < mVarArray.GetSize(); ind++) {
      var = mVarArray[ind];
      localVar = var->type == VARTYPE_LOCAL || 
        (mLoopIndsAreLocal && var->type == VARTYPE_INDEX);
      if ((global && !localVar) || (!global && localVar && var->callLevel == mCallLevel && 
        (var->index == mCurrentMacro || var->type == VARTYPE_INDEX) && 
        var->definingFunc == mCallFunction[mCallLevel])) {
          if (var->name == name)
            return var;
      }
    }
  }
  return NULL;
}

// List all variables of the specified type in the log window, or all variables if no type
// is specified
void CMacroProcessor::ListVariables(int type)
{
  int i, j;
  CString s, t, v;
  Variable *var;
  ArrayRow row;
  mWinApp->AppendToLog("\r\n", LOG_OPEN_IF_CLOSED);
  for (i = 0; i < (int)mVarArray.GetSize(); i++)
  {
    var = mVarArray[i];
    if (var->type == type || type < 0) {
      switch (var->type) {
      case VARTYPE_PERSIST:
        t = " (persistent)";
        break;
      case VARTYPE_INDEX:
        t = " (index)";
        break;
      case VARTYPE_REPORT:
        t = " (report)";
        break;
      case VARTYPE_LOCAL:
        t = " (local)";
        break;
      default:
        t = "";
      }
      if (var->rowsFor2d) {
        for (j = 0; j < var->rowsFor2d->GetSize(); j++) {
          if (j == 0)
            s.Format("%s%s = { ", var->name, t);
          else
            s += "  ";
          row = var->rowsFor2d->ElementAt(j);
          v = row.value;
          v.Replace("\n", " ");
          s += "{ " + v + " }" + ((j == var->rowsFor2d->GetSize() - 1) ? " }" : "") + 
            "\r\n";
        }
      }
      else if (var->numElements > 1) {
        v = var->value;
        // TODO DNM Separation with spaces not unambiguous if arrays contain string 
        // values with spaces
        v.Replace("\n", " ");
        s.Format("%s%s = { %s }", var->name, t, v);
      }
      else
        s.Format("%s%s = %s", var->name, t, var->value);
      mWinApp->AppendToLog(s, LOG_OPEN_IF_CLOSED);
    }
  }
}

// Removes all variables of the given type (or any type except persistent by default) AND
// at the given level unless level < 0 (the default) AND with the given index value 
// (or any index by default) 
void CMacroProcessor::ClearVariables(int type, int level, int index)
{
  Variable *var;
  int ind;
  for (ind = (int)mVarArray.GetSize() - 1; ind >= 0; ind--) {
    var = mVarArray[ind];
    if (((type < 0 && var->type != VARTYPE_PERSIST) || var->type == type) &&
      (index < 0 || index == var->index)  && (level < 0 || var->callLevel >= level)) {
      delete var->rowsFor2d;
      delete var;
      mVarArray.RemoveAt(ind);
    }
  }
  if (type == VARTYPE_REPORT) {
    mScrpLangData.highestReportInd = -1;
    if (mRunningScrpLang) {
      for (ind = 0; ind < 6; ind++) {
        mScrpLangData.reportedStrs[ind] = "";
        mScrpLangData.reportedVals[ind] = 0.;
        mScrpLangData.repValIsString[ind] = false;
      }
    }

  }
}

// Given a variable reference that could include an array subscript, look up the variable
// and row of a 2D array return pointers to the value and numElements elements, or return
// NULL for those pointers for a 2D array without subscript.  Returns NULL for the 
// variable for any error condition
Variable * CMacroProcessor::GetVariableValuePointers(CString & name, CString **valPtr, 
  int **numElemPtr, const char *action, CString & errStr)
{
  CString strCopy = name;
  Variable *var;
  int rightInd, right2, index;
  int leftInd = strCopy.Find('[');
  if (leftInd > 0) {
    if (FindAndCheckArrayIndexes(strCopy, leftInd, rightInd, right2, &errStr) < 0)
      return NULL;
    if (right2 > 0) {
      errStr = CString("Cannot ") + action + " to a single element of a 2D array";
      return NULL;
    }
    strCopy = strCopy.Left(leftInd);
  }
  var = LookupVariable(strCopy, right2);
  if (!var) {
    errStr = "Could not find variable " + strCopy;
    return NULL;
  }

  // Get pointers to value and number of elements for all cases
  if (var->rowsFor2d) {
    if (leftInd > 0) {
      index = ConvertArrayIndex(name, leftInd, rightInd, strCopy,
        (int)var->rowsFor2d->GetSize(), &errStr);
      if (!index)
        return NULL;
      ArrayRow& tempRow = var->rowsFor2d->ElementAt(index - 1);
      *valPtr = &tempRow.value;
      *numElemPtr = &tempRow.numElements;
    } else {
      *valPtr = NULL;
      *numElemPtr = NULL;
    }
  } else {
    if (leftInd > 0) {
      errStr = CString("Cannot ") + action + "to a single element of an array";
      return NULL;
    }
    *valPtr = &var->value;
    *numElemPtr = &var->numElements;
  }
  return var;
  }

// Looks for variables and substitutes them in each string item
int CMacroProcessor::SubstituteVariables(CString * strItems, int maxItems, CString line)
{
  Variable *var;
  CString newstr, value;
  int subInd, varInd, maxlen, maxInd, nright, varlen, arrInd, beginInd, endInd, nameInd;
  int itemLen, global, nright2, leftInd, numElements;
  bool localVar, subArrSize;

  // For each item, look for $ until they are used up
  for (int ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    while (1) {

      // Search from the back so array indexes can be substituted first
      subInd = strItems[ind].ReverseFind('$');
      if (subInd < 0)
        break;

      // Set index of actual name and adjust it if this is looking for # of elements
      itemLen = strItems[ind].GetLength();
      subArrSize = subInd < itemLen - 1 && strItems[ind].GetAt(subInd + 1) == '#';
      nameInd = subInd + (subArrSize ? 2 : 1);

      // Now look for the longest matching variable, first looking for local, then global
      for (global = 0; global < 2; global++) {
        maxlen = 0;
        for (varInd = 0; varInd < mVarArray.GetSize(); varInd++) {
          var = mVarArray[varInd];
          localVar = var->type == VARTYPE_LOCAL ||
            (mLoopIndsAreLocal && var->type == VARTYPE_INDEX);
          if ((global && !localVar) || (!global && localVar &&
            var->callLevel == mCallLevel &&
            (var->index == mCurrentMacro || var->type == VARTYPE_INDEX) &&
            var->definingFunc == mCallFunction[mCallLevel])) {
            varlen = var->name.GetLength();
            if (itemLen - nameInd >= varlen) {
              newstr = strItems[ind].Mid(nameInd, varlen);
              if (!newstr.CompareNoCase(var->name) && maxlen < varlen) {
                maxInd = varInd;
                maxlen = varlen;
              }
            }
          }
        }
        if (maxlen)
          break;
      }

      if (!maxlen) {
        SEMMessageBox("Undefined variable in script line:\n\n" +
          line, MB_EXCLAME);
        return 1;
      }
      var = mVarArray[maxInd];

      // If it is a loop index, look up the value and put in value string
      if (var->type == VARTYPE_INDEX) {
        if (var->index < 0 || var->index >= MAX_LOOP_DEPTH) {
          SEMMessageBox("The variable " + var->name + " is apparently a loop index "
            "variable,\nbut the pointer to the loop is out of range in script line:\n\n" +
            line, MB_EXCLAME);
          return 2;
        }
        var->value.Format("%d", mLoopCount[var->index]);
      }

      // If it is an array element, first check usage
      value = var->value;
      leftInd = nameInd + maxlen;
      numElements = var->numElements;
      if (FindAndCheckArrayIndexes(strItems[ind], leftInd, nright, nright2, &newstr) < 0) {
        SEMMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
        return 2;
      }
      if (nright > 0) {
        if (subArrSize && (!var->rowsFor2d || nright2 > 0)) {
          SEMMessageBox("Illegal use of $# with an array element in line:\n\n" + line,
            MB_EXCLAME);
          return 2;
        }

        // 2D array reference: make sure it IS 2D array, then get the index of the row
        if (nright2 > 0) {
          if (!var->rowsFor2d) {
            SEMMessageBox("Reference to 2D array element, but " + var->name + "is not"
              "a 2D array in line:\n\n" + line, MB_EXCLAME);
            return 2;
          }
          arrInd = ConvertArrayIndex(strItems[ind], leftInd, nright, var->name,
            (int)var->rowsFor2d->GetSize(), &newstr);
          if (!arrInd) {
            SEMMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
            return 2;
          }
          ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
          numElements = arrRow.numElements;
          value = arrRow.value;
          leftInd = nright + 1;
          nright = nright2;
        } else if (var->rowsFor2d) {
          numElements = (int)var->rowsFor2d->GetSize();
        }

        // Get the array index
        arrInd = ConvertArrayIndex(strItems[ind], leftInd, nright, var->name,
          numElements, &newstr);
        if (!arrInd) {
          SEMMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
          return 2;
        }

        // The returned value for 1D reference without index or 2D with one index is the
        // raw string including newlines so that it can be assigned to a new array
        // variable.  echo and its variants convert them to two spaces; other output like
        // to a file will need conversion too.
        if (var->rowsFor2d && nright2 <= 0) {
          ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
          value = arrRow.value;
          numElements = arrRow.numElements;
        } else {

          // Get the starting index of the value and of its terminator
          FindValueAtIndex(value, arrInd, beginInd, endInd);
          value = value.Mid(beginInd, endInd - beginInd);
        }
        maxlen += nright - (subInd + maxlen);
      } else if (var->rowsFor2d) {
        if (!subArrSize) {
          SEMMessageBox("Reference to 2D array variable " + var->name +
            " without any subscripts in line:\n\n" + line, MB_EXCLAME);
          return 2;
        }
        numElements = (int)var->rowsFor2d->GetSize();
      }

      // If doing $#, now substitute the number of elements
      if (subArrSize)
        value.Format("%d", numElements);

      // Build up the substituted string
      newstr = "";
      if (subInd)
        newstr = strItems[ind].Left(subInd);

      // Prevent recursion by replacing any $ with temporary string
      value.Replace("$", "&?#{@");
      newstr += value;
      nright = strItems[ind].GetLength() - (nameInd + maxlen);
      if (nright)
        newstr += strItems[ind].Right(nright);
      strItems[ind] = newstr;
    }
    strItems[ind].Replace("&?#{@", "$");
  }
  return 0;
}

//  Get an array of floats from a variable
float *CMacroProcessor::FloatArrayFromVariable(CString name, int &numVals, 
  CString &report)
{
  int index, index2, ix0, ix1, numRows = 1;
  float *fvalues;
  float oneVal;
  char *endPtr;
  Variable *var;
  CString *valPtr;
  CString strCopy;
  int *numElemPtr;
  bool noValPtr;

  numVals = 0;
  name.MakeUpper();
  var = GetVariableValuePointers(name, &valPtr, &numElemPtr, "get float array from",
    report);
  if (!var)
    return NULL;
  noValPtr = !valPtr;
  if (noValPtr) {
    numRows = (int)var->rowsFor2d->GetSize();
    for (index = 0; index < numRows; index++) {
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(index);
      numVals += arrRow.numElements;
    }
  } else
    numVals = *numElemPtr;
  if (!numVals) {
    report = "There are no array elements";
    return NULL;
  }
  fvalues = new float[numVals];
  numVals = 0;
  for (index = 0; index < numRows; index++) {
    if (noValPtr) {
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(index);
      valPtr = &arrRow.value;
      numElemPtr = &arrRow.numElements;
    }

    // This function wants array indexes from 1
    for (index2 = 1; index2 <= *numElemPtr; index2++) {
      FindValueAtIndex(*valPtr, index2, ix0, ix1);
      report = valPtr->Mid(ix0, ix1 - ix0);
      oneVal = (float)strtod((LPCTSTR)report, &endPtr);
      if (endPtr - (LPCTSTR)report < report.GetLength()) {
        strCopy = "";
        if (noValPtr)
          strCopy.Format(" in row %d", index);
        report.Format("Cannot get array statistics: item %s at index %d%s of array %s"
          " has non-numeric characters", (LPCTSTR)report, index2 + 1, (LPCTSTR)strCopy,
          (LPCTSTR)name);
        delete[] fvalues;
        return NULL;
      }

      fvalues[numVals++] = oneVal;
    }
  }
  return fvalues;
}

// Get two float arrays from variables with names at itemInd and itemInd + 1, where they
// are required to have equal numbers of values.
int CMacroProcessor::GetPairedFloatArrays(int itemInd, float **xArray, float **yArray,
  int &numVals, CString &report)
{
  int numY, retVal = 0;
  *xArray = FloatArrayFromVariable(mStrItems[itemInd], numVals, report);
  if (*xArray)
    *yArray = FloatArrayFromVariable(mStrItems[itemInd + 1], numY, report);
  if (!*xArray || !*yArray) {
    retVal = 1;
  } else {
    if (numVals != numY) {
      report = "The two arrays do not have the same number of values";
      retVal = 1;
    }
  }
  return retVal;
}

// Get a float or integer vector from a 1D array
void CMacroProcessor::FillVectorFromArrayVariable(FloatVec *fvec, IntVec *ivec, 
  Variable *var)
{
  int index, ix0, ix1;
  CString valStr;
  float fval;
  if (fvec)
    fvec->clear();
  for (index = 0; index < var->numElements; index++) {
    FindValueAtIndex(var->value, index + 1, ix0, ix1);
    valStr = var->value.Mid(ix0, ix1 - ix0);
    fval = (float)atof((LPCTSTR)valStr);
    if (fvec)
      fvec->push_back(fval);
    if (ivec)
      ivec->push_back(B3DNINT(fval));
  }
}

// Fill an integer vector from all values on command
void CMacroProcessor::SetGraphListVec(IntVec &graphList)
{
  graphList.clear();
  for (int ind = 1; ind < MAX_MACRO_TOKENS; ind++) {
    if (mItemEmpty[ind])
      return;
    graphList.push_back(mItemInt[ind]);
  }
}

// This checks for the array assignment delimiter being at both ends of the set of
// string items starting from firstInd, and strips it from the set of items
// Returns -1 for unbalanced delimiter, 0 for none, 1 for array
int CMacroProcessor::CheckForArrayAssignment(CString *strItems, int &firstInd)
{
  int last, ifStart, ifEnd, lastLen;
  char endSep = '}', startSep = '{';
  for (last = firstInd; last < MAX_MACRO_TOKENS; last++)
    if (strItems[last].IsEmpty())
      break;
  if (last == firstInd)
    return 0;
  ifStart = (strItems[firstInd].GetAt(0) == startSep) ? 1 : 0;
  lastLen = strItems[last - 1].GetLength();
  ifEnd = (strItems[last - 1].GetAt(lastLen - 1) == endSep) ? 1 : 0;
  if (ifStart != ifEnd)
    return -1;
  if (!ifStart)
    return 0;
  if (strItems[firstInd] == startSep)
    firstInd++;
  else
    strItems[firstInd] = strItems[firstInd].Right(strItems[firstInd].GetLength() - 1);
  if (strItems[last - 1] == endSep)
    strItems[last - 1] = "";
  else
    strItems[last - 1] = strItems[last - 1].Left(lastLen - 1);
  return 1;
}

// Finds array indexes, if any, starting at position leftInd in item.  Returns 0 if there
// is no index there or it is beyond end of string; 1 for an index, and -1 for various
// errors.  Returns right1 with the index of the first right bracket or 0 for no index, 
// returns right2 with the index a second right brack or 0 if there is no 2nd index
int CMacroProcessor::FindAndCheckArrayIndexes(CString &item, int leftIndex, int &right1,
  int &right2, CString *errStr)
{
  right1 = right2 = 0;
  if (leftIndex >= item.GetLength() || item.GetAt(leftIndex) != '[')
    return 0;
  right1 = item.Find(']', leftIndex);
  if (right1 < 0) {
    if (errStr)
      *errStr = "Unbalanced array index delimiters: [ without ] (is there a space between [ and ]?)";
    return -1;
  }
  if (item.Left(right1).ReverseFind('[') != leftIndex) {
    if (errStr)
      *errStr = "Unbalanced or extra array delimiters: [ followed by two ]";
    return -1;
  }
  if (right1 == item.GetLength() - 1 || item.GetAt(right1 + 1) != '[')
    return 1;
  right2 = item.Find(']', right1 + 1);
  if (right2 < 0) {
    if (errStr)
      *errStr = "Unbalanced array index delimiters for second dimension: [ without ]";
    return -1;
  }
  if (item.Left(right2).ReverseFind('[') != right1 + 1) {
    if (errStr)
      *errStr = "Unbalanced or extra array delimiters: [ followed by two ]";
    return -1;
  }
  return 1;
}

// Convert an array index in a string item given the index of left and right delimiters
// Fill in an error message and return 0 if there is a problem
int CMacroProcessor::ConvertArrayIndex(CString strItem, int leftInd, int rightInd,
  CString name, int numElements, CString *errMess)
{
  int arrInd;
  double dblInd;
  CString temp;
  char *endPtr;
  if (rightInd == leftInd + 1) {
    errMess->Format("Empty array index for variable %s", name);
    return 0;
  }
  temp = strItem.Mid(leftInd + 1, rightInd - (leftInd + 1));
  if (temp.FindOneOf("()-+*/") >= 0 && EvalExpressionInIndex(temp)) {
    *errMess = "Error doing arithmetic in array index " + temp;
    return 0;
  }
  dblInd = strtod((LPCTSTR)temp, &endPtr);
  if (endPtr - (LPCTSTR)temp < temp.GetLength()) {
    errMess->Format("Illegal character in array index %s", (LPCTSTR)temp);
    return 0;
  }
  arrInd = B3DNINT(dblInd);
  if (fabs(arrInd - dblInd) > 0.001) {
    errMess->Format("Array index %s is not close enough to an integer", (LPCTSTR)temp);
    return 0;
  }
  if (arrInd < 1 || arrInd > numElements) {
    errMess->Format("Array index evaluates to %d and is out of range for variable %s",
      arrInd, (LPCTSTR)name);
    return 0;
  }
  return arrInd;
}

// For a variable with possible multiple elements, find element at index arrInd
// (numbered from 1, must be legal) and return starting index and index of terminator
// which is null or \n
void CMacroProcessor::FindValueAtIndex(CString &value, int arrInd, int &beginInd, 
  int &endInd)
{
  endInd = -1;
  for (int valInd = 0; valInd < arrInd; valInd++) {
    beginInd = endInd + 1;
    endInd = value.Find('\n', beginInd);
    if (endInd < 0) {
      endInd = value.GetLength();
      return;
    }
  }
}

// Evaluate an arithmetic expression inside array index delimiters, which cannot contain
// spaces, but expanding all parenthese and operators into separate tokens so that the
// regular arithmetic function can be used.
int CMacroProcessor::EvalExpressionInIndex(CString &indStr)
{
  const int maxItems = 40;
  CString strItems[maxItems];
  CString strLeft, temp = indStr.Mid(1);
  int numItems, opInd, splitInd = -1;
  char opChar;
  bool atStartOfClause = true;
  bool lastWasOp = false;
  if ((indStr.GetAt(0) == '-' || indStr.GetAt(0) == '+') && temp.FindOneOf("()+-*/") < 0)
    return 0;
  strLeft = indStr;
  while (1) {
    opInd = strLeft.FindOneOf("()+-*/");

    // Process operator at start first: if it is + or -, see if last was true operator
    // or we are at start of clause and next is a digit;
    // if so keep it together with next and find operator after that
    if (opInd == 0) {
      opChar = strLeft.GetAt(0);
      if (opChar == '-' || opChar == '+') {
        if (lastWasOp || (atStartOfClause && strLeft.GetLength() > 1 &&
          (char)strLeft.GetAt(1) >= '0' && (char)strLeft.GetAt(1) <= '9')) {
          temp = strLeft.Mid(1);
          opInd = temp.FindOneOf("()+-*/");
          if (opInd > 0)
            opInd++;
        }
      }
    }

    // If no more operators, assign the remaining string
    if (opInd < 0) {
      if (strLeft.GetLength() > 0) {
        splitInd++;
        if (splitInd >= maxItems)
          break;
        strItems[splitInd] = strLeft;
      }
      break;
    }

    // Split to the left of the operator off if any
    if (opInd > 0) {
      splitInd++;
      if (splitInd >= maxItems)
        break;
      strItems[splitInd] = strLeft.Left(opInd);
      lastWasOp = false;
      strLeft = strLeft.Mid(opInd);
      opInd = 0;
    }

    // Record if this was an operator or start of parentheses and split it off
    opChar = strLeft.GetAt(0);
    lastWasOp = opChar == '+' || opChar == '-' || opChar == '*' || opChar == '/';
    atStartOfClause = opChar == '(';
    splitInd++;
    if (splitInd >= maxItems)
      break;
    strItems[splitInd] = opChar;
    strLeft = strLeft.Mid(1);
  }

  if (splitInd >= maxItems) {
    SEMMessageBox("Too many components to evaluate expression in array index");
    return 1;
  }
  if (EvaluateExpression(strItems, splitInd + 1, "", 0, numItems, opInd))
    return 1;
  if (numItems > 1) {
    SEMMessageBox("Array index still has multiple components after doing arithmetic");
    return 1;
  }
  indStr = strItems[0];
  return 0;
}

// Evaluates an arithmetic expression in the array of items
int CMacroProcessor::EvaluateExpression(CString *strItems, int maxItems, CString line,
                                        int ifArray, int &numItems, int &numOrig)
{
  int ind, left, right, maxLevel, level;
  CString cstr;
  std::string str;
  bool noLine = line.GetLength() == 0;

  // Count original number of items
  numItems = 0;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;
  numOrig = numItems;

  // Loop on parenthesis pairs
  for (;;) {

    // Find the highest level and the index of ( and ) for the first occurrence of it
    maxLevel = level = 0;
    left = right = -1;
    for (ind = 0; ind < numItems; ind++) {
      if (strItems[ind] == '(') {
        level++;
        if (level > maxLevel) {
          maxLevel = level;
          left = ind;
          right = -1;
        }
      }
      if (strItems[ind] == ')') {
        if (level == maxLevel && right < 0)
          right = ind;
        level--;
      }
    }

    if (level) {
      SEMMessageBox("Unbalanced parentheses while evaluating an expression in script" +
        CString(noLine ? "" : " line:\n\n") + line, MB_EXCLAME);
        return 1;
    }

    // When no parentheses, do whatever remains directly or return with it
    if (!maxLevel && ifArray) {
      strItems[numItems] = "";
      return 0;
    }
    if (!maxLevel)
      return EvaluateArithmeticClause(strItems, numItems, line, numItems);

    // Otherwise evaluate inside the parentheses; but first move a function inside
    cstr = strItems[left - 1];
    cstr.MakeUpper();
    str = std::string((LPCTSTR)cstr);
    if (left > 0 && (mFunctionSet1.count(str) > 0 || mFunctionSet2.count(str) > 0)) {
      strItems[left] = strItems[left - 1];
      left--;
      strItems[left] = "(";
    }
    if (EvaluateArithmeticClause(&strItems[left + 1], right - left - 1, line, ind))
      return 1;

    // Get rid of the ( and ) and pack down the rest of the items
    strItems[left] = strItems[left + 1];
    for (ind = 0; ind < numItems - right; ind++)
      strItems[left + 1 + ind] = strItems[right + 1 + ind];
    numItems -= right - left;
  }
  return 0;  
}

// Evaluates one arithmetic clause possibly inside parentheses
int CMacroProcessor::EvaluateArithmeticClause(CString * strItems, int maxItems, 
                                              CString line, int &numItems)
{
  int ind, dig, loop;
  double left, right, result = 0.;
  CString str, *strp;
  std::string stdstr;
  bool isFormat, gotDec = false;
  CString format = "";
  char conv;
  CString allowedConv = "d f g G e E x X";
  numItems = 0;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;

  // Loop on * and / then on + and -
  for (loop = 0; loop < 2; loop++) {
    for (ind = 0; ind < numItems; ind++) {
      strp = &strItems[ind];
      if ((!loop && (*strp == "*" || *strp == "/")) ||
        (loop && (*strp == "+" || *strp == "-"))) {

        // Process minus at start, reject other operators at start/end
        if (!ind && *strp == "-") {
          if (ItemToDouble(strItems[1], line, right))
            return 1;
          result = -right;

          // Replace minus sign and shift strings down by 1
          ReplaceWithResult(result, strItems, 0, numItems, 1);
          ind--;

        }  else if (!ind || ind == numItems - 1) {
           SEMMessageBox("Arithmetic operator at beginning or end of script line or "
             "clause:\n\n" + line, MB_EXCLAME);
           return 1;
        } else {

          // Get the values on either side
          if (ItemToDouble(strItems[ind - 1], line, left) ||
            ItemToDouble(strItems[ind + 1], line, right))
            return 1;

          // Compute value
          if (*strp == "*")
            result = left * right;
          else if (*strp == "+")
            result = left + right;
          else if (*strp == "-")
            result = left - right;
          else {
            if (fabs(right) < 1.e-30 * fabs(left)) {
              SEMMessageBox("Division by 0 or very small number in script line:\n\n" +
                line, MB_EXCLAME);
              return 1;
            }
            result = left / right;
          }

          // Replace left value and shift strings down by 2
          ReplaceWithResult(result, strItems, ind - 1, numItems, 2);
          ind--;
        }
      }
    }
  }

  // Evaluate functions if possible - will really only work at start
  for (ind = 0; ind < numItems; ind++) {
    str = strItems[ind];
    str.MakeUpper();
    isFormat = str == "FORMAT";

    // First process items without arguments
    if (str == "RAND") {
      result = rand() / (float)RAND_MAX;
      ReplaceWithResult(result, strItems, ind, numItems, 0);
    }
    if (ind == numItems - 1)
      break;
    stdstr = std::string((LPCTSTR)str);
    if (mFunctionSet1.count(stdstr) > 0) {
      if (ItemToDouble(strItems[ind + 1], line, right))
        return 1;
      if (str == "SQRT") {
        if (right < 0.) {
          SEMMessageBox("Taking square root of negative number in script line:\n\n" +
              line, MB_EXCLAME);
          return 1;
        }
        result = sqrt(right);
      } else if (str == "COS")
        result = cos(DTOR * right);
      else if (str == "SIN")
        result = sin(DTOR * right);
      else if (str == "TAN")
        result = tan(DTOR * right);
      else if (str == "ATAN")
        result = atan(right) / DTOR;
      else if (str == "ABS")
        result = fabs(right);
      else if (str == "NEARINT")
        result = B3DNINT(right);
      else if (str == "LOG" || str == "LOG10") {
        if (right <= 0.) {
          SEMMessageBox("Taking logarithm of non-positive number in script line:\n\n" +
              line, MB_EXCLAME);
          return 1;
        }
        result = str == "LOG" ? log(right) : log10(right);
      } else if (str == "EXP")
        result = exp(right);
      ReplaceWithResult(result, strItems, ind, numItems, 1);
    } else if (mFunctionSet2.count(stdstr) > 0) {
      if (isFormat && strItems[ind + 2].GetLength() > 2 && strItems[ind + 2].Find("0x") == 0) {
        sscanf(((LPCTSTR)strItems[ind + 2]) + 2, "%x", &dig);
        right = dig;
      } else if ((!isFormat && ItemToDouble(strItems[ind + 1], line, left)) ||
        ItemToDouble(strItems[ind + 2], line, right))
        return 1;
      if (str == "ATAN2")
        result = atan2(left, right) / DTOR;
      else if (str == "MODULO") {
        if (B3DNINT(right) > 0)
          result = B3DNINT(left) % B3DNINT(right);
        else
          result = 0;
      } else if (str == "FRACDIFF") {
        result = B3DMAX(fabs(left), fabs(right));
        if (result < 1.e-37)
          result = 0.;
        else
          result = fabs(left - right) / result;
      } else if (str == "DIFFABS")
        result = fabs(left - right);
      else if (str == "ROUND") {
        result = pow(10., B3DNINT(right));
        result = B3DNINT(left * result) / result;
      } else if (str == "POWER")
        result = pow(left, right);
      else if (str == "MIN")
        result = B3DMIN(left, right);
      else if (str == "MIN")
        result = B3DMIN(left, right);
      else if (str == "MAX")
        result = B3DMAX(left, right);
      else if (isFormat) {
        format = strItems[ind + 1];
        conv = format.GetAt(format.GetLength() - 1);
        if (allowedConv.Find(conv) < 0) {
          SEMMessageBox("The format string " + format + " does not end in one of: " + 
            allowedConv);
          return 1;
        }
        for (dig = 0; dig < format.GetLength() - 1; dig++) {
          char let = format.GetAt(dig);
          if (let == '.') {
            if (gotDec) {
              SEMMessageBox("The format string " + format +
                " has more than one decimal point");
              return 1;
            }
            if (conv == 'x' || conv == 'X' || conv == 'd') {
              SEMMessageBox("The format string " + format +
                " for integer conversion should not have a decimal point");
              return 1;
            }
            gotDec = true;

          } else if (let < '0' || let > '9') {
            SEMMessageBox("The format string " + format + 
              " has a non-numeric character before the end");
            return 1;
          }
        }
        format = "%" + format;
        result = right;
      }
      ReplaceWithResult(result, strItems, ind, numItems, 2, format);
    }
  }
  return 0;
}

// Evaluates a potentially compound IF statement
int CMacroProcessor::EvaluateIfStatement(CString *strItems, int maxItems, CString line,
                                         BOOL &truth)
{
  int ind, left, right, maxLevel, level, numItems = 0;
  int leftInds[MAX_LOOP_DEPTH];

  if (SeparateParentheses(strItems, maxItems, mParseQuotes)) {
    AfxMessageBox("Too many items on line after separating out parentheses in script"
      " line:\n\n" + line, MB_EXCLAME);
    return 1;
  }

  // Count original number of items
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;

  // Loop on parenthesis pairs containing one or more booleans
  for (;;) {

    // Find the highest level with a boolean and the index of ( and ) for the first 
    // occurrence of it.  Keep track of open paren index by level for this
    maxLevel = level = 0;
    left = right = -1;
    for (ind = 0; ind < numItems; ind++) {
      if (strItems[ind] == '(') {
        level++;
        leftInds[level] = ind;
      }
      if (strItems[ind] == ')') {
        if (level == maxLevel && right < 0)
          right = ind;
        level--;
      }
      if ((!strItems[ind].CompareNoCase("AND") || !strItems[ind].CompareNoCase("OR")) && 
        level > maxLevel) {
        maxLevel = level;
        left = leftInds[level];
        right = -1;
      }
    }

    if (level) {
      AfxMessageBox("Unbalanced parentheses while evaluating an expression in script"
        " line:\n\n" + line, MB_EXCLAME);
        return 1;
    }

    // When no parentheses, do whatever remains directly
    if (!maxLevel)
      return EvaluateBooleanClause(strItems, numItems, line, truth);

    // Otherwise evaluate inside the parentheses
    if (EvaluateBooleanClause(&strItems[left + 1], right - left - 1, line, truth))
      return 1;

    // Replace the ( ) with a true or false statement and pack down the rest
    strItems[left] = "1";
    strItems[left + 1] = "==";
    strItems[left + 2] = truth ? "1" : "0";
    for (ind = 0; ind < numItems - right; ind++)
      strItems[left + 3 + ind] = strItems[right + 1 + ind];
    numItems -= right - left - 2;
  }
  return 1; 
}

// Evaluates a unit of an IF statement containing one or more boolean operators at one
// parenthetical level
int CMacroProcessor::EvaluateBooleanClause(CString * strItems, int maxItems, CString line,
                                           BOOL &truth)
{
  int ind, numItems, opind, indstr = 0;
  BOOL clauseVal, lastAnd;

  // Loop on logical operators
  for (;;) {
    opind = -1;
    numItems = 0;

    // Find next logical operator if any
    for (ind = indstr; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
      if (!strItems[ind].CompareNoCase("OR") || !strItems[ind].CompareNoCase("AND")) {
        opind = ind;
        break;
      }
      numItems++;
    }

    // Check for one end or other of clause/line
    if (opind == indstr || opind == maxItems - 1 || strItems[opind + 1].IsEmpty()) {
      AfxMessageBox("AND/OR operator at start of line or clause or at end of line "
        "in script line:\n\n" + line, MB_EXCLAME);
      return 1;
    }

    // get the value of the clause and assign for first clause, or AND or OR with truth
    if (EvaluateComparison(&strItems[indstr], numItems, line, clauseVal))
      return 1;
    if (!indstr)
      truth = clauseVal;
    else if (lastAnd)
      truth = truth && clauseVal;
    else
      truth = truth || clauseVal;

    // Done if no more operators, otherwise set up for next clause
    if (opind < 0)
      break;
    lastAnd = !strItems[opind].CompareNoCase("AND");
    indstr = opind + 1;
  }
  return 0;
}

// Evaluates an IF clause for a comparison operator and arithmetic expressions on
// each side, returns value in truth
int CMacroProcessor::EvaluateComparison(CString * strItems, int maxItems, CString line,
                                         BOOL &truth)
{
  int ind, numItems = 0, opind = -1, numLeft = 1, numRight = 1;
  double left, right;
  CString *str;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    numItems++;
    str = &strItems[ind];
    if (*str == "==" || *str == "!=" || *str == ">=" || *str == "<=" || *str == "<" ||
      *str == ">") {
      if (opind >= 0) {
        AfxMessageBox("More than one comparison operator in clause in script line:\n\n" +
           line, MB_EXCLAME);
         return 1;
      }
      opind = ind;
    }
  }
  if (opind < 0) {
    AfxMessageBox("No comparison operator for IF statement clause in script line:\n\n" +
      line, MB_EXCLAME);
    return 1;
  }

  // Strip parentheses around the entire clause by adjusting pointer, index and count
  if (strItems[0] == "(" && strItems[numItems - 1] == ")") {
    strItems++;
    opind--;
    numItems -= 2;
  }
  if (!opind || opind == numItems - 1) {
    AfxMessageBox("Comparison operator at beginning or end of IF statement/clause:\n\n" +
      line, MB_EXCLAME);
    return 1;
  }

  // If there is more than one item, evaluate expressions, make sure there is only one
  if (opind > 1 && EvaluateExpression(strItems, opind, line, 0, numLeft, ind))
    return 1;
  if (opind < numItems - 2 && EvaluateExpression(&strItems[opind + 1],
    numItems - 1 - opind, line, 0, numRight, ind))
    return 1;
  if (numLeft > 1 || numRight > 1) {
    AfxMessageBox("Items being compared do not reduce to a single number in script line:"
      "\n\n" + line, MB_EXCLAME);
    return 1;
  }

  // Convert the single strings to values and evaluate
  if (ItemToDouble(strItems[0], line, left))
    return 1;
  if (ItemToDouble(strItems[opind + 1], line, right))
    return 1;
  str = &strItems[opind];
  if (*str == ">")
    truth = left > right;
  else if (*str == "<")
    truth = left < right;
  else if (*str == ">=")
    truth = left >= right;
  else if (*str == "<=")
    truth = left <= right;
  else if (*str == "==")
    truth = fabs(left - right) <= 1.e-10 * B3DMAX(fabs(left), fabs(right));
  else if (*str == "!=")
    truth = fabs(left - right) > 1.e-10 * B3DMAX(fabs(left), fabs(right));

  return 0;
}

// Convert an item being used for arithmetic to a double
BOOL CMacroProcessor::ItemToDouble(CString str, CString line, double & value)
{
  char *strptr = (char *)(LPCTSTR)str;
  char *invalid;
  value = strtod(strptr, &invalid);
  if (invalid - strptr != str.GetLength()) {
    SEMMessageBox("Invalid character in " +  str +
      " which you are trying to do arithmetic with in script line:\n\n" +
      line, MB_EXCLAME);
    return true;
  }
  return false;
}

// Replace the item at index in the array of items with text for the result value
// and shift the remainder of the items down as needed (by 1 or 2)
void CMacroProcessor::ReplaceWithResult(double result, CString * strItems, int index,
                                        int & numItems, int numDrop, CString format)
{
  char conv;
  if (format.IsEmpty()) {
    strItems[index].Format("%f", result);
    UtilTrimTrailingZeros(strItems[index]);
  } else {
    conv = format.GetAt(format.GetLength() - 1);
    if (conv == 'x' || conv == 'X' || conv == 'd')
      strItems[index].Format(format, B3DNINT(result));
    else
      strItems[index].Format(format, result);
  }
  numItems -= numDrop;
  for (int lr = index + 1; lr < numItems; lr++)
    strItems[lr] = strItems[lr + numDrop];
}

// Saves reported values in variables
void CMacroProcessor::SetReportedValues(double val1, double val2, double val3,
                                       double val4, double val5, double val6)
{
  SetReportedValues(NULL, val1, val2, val3, val4, val5, val6);
}

void CMacroProcessor::SetReportedValues(CString *strItems, double val1, double val2, double val3,
  double val4, double val5, double val6)
{
  if (val1 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val1, 1);
  if (val2 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val2, 2);
  if (val3 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val3, 3);
  if (val4 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val4, 4);
  if (val5 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val5, 5);
  if (val6 > EXTRA_VALUE_TEST)
    SetOneReportedValue(strItems, val6, 6);
}

void CMacroProcessor::SetRepValsAndVars(int firstInd, double val1, double val2, 
  double val3, double val4, double val5, double val6)
{
  SetReportedValues(&mStrItems[firstInd], val1, val2, val3, val4, val5, val6);
}

// Sets one reported value with a numeric or string value, also sets an optional variable
// Pass the address of the FIRST optional variable
// Clears all report variables when index is 1 and then requires report variables to
// be new, so call with 1 first
void CMacroProcessor::SetOneReportedValue(CString *strItems, CString *valStr, 
  double value, int index)
{
  CString num, str;
  int typeAdd = valStr ? 0 : VARTYPE_ADD_FOR_NUM;
  if (index < 1 || index > 6)
    return;
  num.Format("%d", index);
  if (valStr) {
    str = *valStr;
    mScrpLangData.repValIsString[index - 1] = true;
  } else {
    str.Format("%f", value);
    UtilTrimTrailingZeros(str);
    mScrpLangData.reportedVals[index - 1] = value;
  }
  if (index == 1 && !mRunningScrpLang)
    ClearVariables(VARTYPE_REPORT);
  mScrpLangData.reportedStrs[index - 1] = str;
  ACCUM_MAX(mScrpLangData.highestReportInd, index - 1);
  SetVariable("REPORTEDVALUE" + num, str, VARTYPE_REPORT + typeAdd, index, true);
  SetVariable("REPVAL" + num, str, VARTYPE_REPORT + typeAdd, index, true);
  if (strItems && !strItems[index - 1].IsEmpty())
    SetVariable(strItems[index - 1], str, VARTYPE_REGULAR + typeAdd, -1, false);
}

// Overloads for convenience
void CMacroProcessor::SetOneReportedValue(CString &valStr, int index)
{
  SetOneReportedValue(NULL, &valStr, 0., index);
}
void CMacroProcessor::SetOneReportedValue(double value, int index)
{
  SetOneReportedValue(NULL, NULL, value, index);
}
void CMacroProcessor::SetOneReportedValue(CString *strItem, CString &valStr, int index)
{
  SetOneReportedValue(strItem, &valStr, 0., index);
}
void CMacroProcessor::SetOneReportedValue(CString *strItem, double value, int index)
{
  SetOneReportedValue(strItem, NULL, value, index);
}

// Checks the nesting of IF and LOOP blocks in the given macro, as well
// as the appropriateness of ELSE, BREAK, CONTINUE
int CMacroProcessor::CheckBlockNesting(int macroNum, int startLevel, int &tryLevel)
{
  int blockType[MAX_LOOP_DEPTH];
  bool elseFound[MAX_LOOP_DEPTH], catchFound[MAX_LOOP_DEPTH];
  CString strLabels[MAX_MACRO_LABELS], strSkips[MAX_MACRO_SKIPS];
  int labelBlockLevel[MAX_MACRO_LABELS], labelSubBlock[MAX_MACRO_LABELS];
  int skipBlockLevel[MAX_MACRO_SKIPS], skipCurIndex[MAX_MACRO_SKIPS];
  int labelCurIndex[MAX_MACRO_LABELS];
  short skipSubBlocks[MAX_MACRO_SKIPS][MAX_LOOP_DEPTH + 1];
  int blockLevel = startLevel;
  bool stringAssign, isIF, isELSE, isELSEIF, isTRY, inFunc = false, inComment = false;
  bool useFound;
  char *endPtr;
  MacroFunction *func;
  int subBlockNum[MAX_LOOP_DEPTH + 1];
  CString *macro = &mMacros[macroNum];
  int tooMany, numLabels = 0, numSkips = 0;
  int i, inloop, needVers, index, skipInd, labInd, length, cmdIndex, currentIndex = 0;
  int lastInd, newCurrentInd, startLine, lastFoundScript = -1;
  int currentVersion = 204;
  CString strLine, strItems[MAX_MACRO_TOKENS], errmess, intCheck;
  const char *features[] = {"variable1", "arrays", "keepcase", "zeroloop", "evalargs"};
  int numFeatures = sizeof(features) / sizeof(char *);

  if (startLevel < 0)
    mCheckingParseQuotes = false;
  mAlreadyChecked[macroNum] = true;
  for (i = 0; i <= MAX_LOOP_DEPTH; i++)
    subBlockNum[i] = 0;

  errmess.Format(" in script #%d", macroNum + 1);
  while (currentIndex < macro->GetLength()) {
    GetNextLine(macro, currentIndex, strLine);
    if (inComment) {
      if (strLine.Find("/*") >= 0) {
        strLine.TrimLeft();
        if (strLine.Find("/*") == 0)
          FAIL_CHECK_LINE("Starting a comment with /* inside a /*...*/ comment");
      }
      if (strLine.Find("*/") >= 0)
        inComment = false;
      continue;
    }
    tooMany = mParamIO->ParseString(strLine, strItems, MAX_MACRO_TOKENS,
      mCheckingParseQuotes);
    if (strItems[0].Find("/*") == 0) {
      inComment = strLine.Find("*/") < 0;
      continue;
    }

    if (!strItems[0].IsEmpty()) {
      strItems[0].MakeUpper();
      strtod((LPCTSTR)strItems[0], &endPtr);
      if (endPtr - (LPCTSTR)strItems[0] == strItems[0].GetLength())
        FAIL_CHECK_LINE("A command cannot start with a number");
      cmdIndex = LookupCommandIndex(strItems[0]);

      // Check validity of variable assignment then skip further checks
      stringAssign = strItems[1] == "@=" || strItems[1] == ":@=";
      if (!stringAssign && tooMany)
        FAIL_CHECK_LINE("Too many items on line");
      if (strItems[1] == "=" || strItems[1] == ":=" || stringAssign) {
        index = 2;
        if (!stringAssign) {
          i = CheckForArrayAssignment(strItems, index);
          if (i < 0)
            FAIL_CHECK_LINE("Array delimiters \"{}\" are not at start and end of values");
        }
        if (strItems[1] == "=" && strItems[index].IsEmpty())
          FAIL_CHECK_LINE("Empty assignment");
        if (WordIsReserved(strItems[0]))
          FAIL_CHECK_LINE("Assignment to reserved command name");
        if (strItems[0].GetAt(0) == '$')
          FAIL_CHECK_LINE("You cannot assign to a name that is a variable");
        if (!stringAssign &&
          CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine, errmess,
            mCheckingParseQuotes))
          return 99;
        continue;
      }

      // If the command is a variable, skip checks except to make sure it is not a label
      if (strItems[0].GetAt(0) == '$') {
        if (strItems[0].GetAt(strItems[0].GetLength() - 1) == ':')
          FAIL_CHECK_LINE("Label cannot be a variable");
        continue;
      }
      if (CMD_IS(PARSEQUOTEDSTRINGS))
        mCheckingParseQuotes = strItems[1].IsEmpty() || atoi((LPCTSTR)strItems[1]) != 0;

      // See if a feature is required
      if (CMD_IS(REQUIRE)) {
        for (i = 1; i < MAX_MACRO_TOKENS; i++) {
          if (strItems[i].IsEmpty())
            break;
          inloop = 0;
          for (index = 0; index < numFeatures; index++)
            if (!strItems[i].CompareNoCase(features[index]))
              inloop = 1;
          if (!inloop) {
            needVers = atoi(strItems[1]);
            if (needVers > 0) {
              if (needVers <= currentVersion) {
                inloop = 1;
              } else {
                 intCheck.Format("Required scripting version %d is not provided by this\n"
                   "version of SerialEM, which has scripting version %d.\n"
                   "If this number is right, then you need a later version."
                   "\nThe Require command is", needVers, currentVersion);
                 FAIL_CHECK_NOLINE(intCheck);
              }
            }
          }
          if (!inloop) {
            intCheck.Format("Required feature \"%s\" is not available in this version\n"
              "of SerialEM.  If this is spelled right, then you need a later version\n\n"
              "Features available in this version are:\n", (LPCTSTR)strItems[1]);
            for (index = 0; index < numFeatures; index++)
              intCheck += CString(" ") + features[index];
            intCheck += "\nThe Require command is";
            FAIL_CHECK_NOLINE(intCheck);
          }
        }
      }

      // Validate a label and record its level and sub-block #
      length = strItems[0].GetLength();
      if (strItems[0].GetAt(length - 1) == ':') {
        if (!strItems[1].IsEmpty())
          FAIL_CHECK_LINE("Label must not be followed by anything");
        if (length == 1)
          FAIL_CHECK_LINE("Empty label in line");
        if (numLabels >= MAX_MACRO_LABELS)
          FAIL_CHECK_NOLINE("Too many labels in script");
        strLabels[numLabels] = strItems[0].Left(length - 1);
        for (i = 0; i < numLabels; i++) {
          if (strLabels[i] == strLabels[numLabels])
            FAIL_CHECK_LINE("Duplicate of label already used")
        }
        labelCurIndex[numLabels] = currentIndex;
        labelBlockLevel[numLabels] = blockLevel;
        labelSubBlock[numLabels++] = subBlockNum[1 + blockLevel];
        continue;
      }
        
      // Repeat and DoMacro may not be called from inside any nest or called macro
      if ((CMD_IS(REPEAT) || CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT)) && 
        (blockLevel >= 0 || mCallLevel))
        FAIL_CHECK_LINE("Repeat, DoMacro, or DoScript statement cannot be used\n"
        "inside a called script, an IF block, or a loop,");

      isIF = CMD_IS(IF);
      isELSE = CMD_IS(ELSE);
      isELSEIF = CMD_IS(ELSEIF);
      isTRY = CMD_IS(TRY);
      if (CMD_IS(REPEAT)) {
        break;

        // Check macro call if it has an argument - leave to general command check if not
      } else if ((CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT) || CMD_IS(CALLMACRO) ||
        CMD_IS(CALLSCRIPT) || CMD_IS(CALL) || CMD_IS(CALLFUNCTION)) &&
        !strItems[1].IsEmpty()) {
        func = NULL;
        useFound = false;
        if (strItems[1].GetAt(0) == '$')
          FAIL_CHECK_LINE("The script number or script or function name cannot be a"
            " variable,");
        if ((CMD_IS(CALLMACRO) || CMD_IS(CALLSCRIPT)) && strItems[1] == "-1") {
          if (lastFoundScript <= 0)
            FAIL_CHECK_LINE("Calling with a script number of -1 must be preceded by"
              " FindScriptByName,");
          useFound = true;
        }
        if (CMD_IS(CALL)) {
          index = FindCalledMacro(strLine, true);
          if (index < 0)
            return 12;
        } else if (CMD_IS(CALLFUNCTION)) {
          func = FindCalledFunction(strLine, true, index, i, macroNum);
          if (!func)
            return 12;
        } else {
          index = useFound ? lastFoundScript : atoi(strItems[1]) - 1;

          // If this is an unchecked macro, better unload the macro if editor open
          if (index >= 0 && index < MAX_MACROS && !mAlreadyChecked[index] &&
            mMacroEditer[index])
            mMacroEditer[index]->TransferMacro(true);

          if (index < 0 || index >= MAX_MACROS || mMacros[index].IsEmpty())
            FAIL_CHECK_LINE("Trying to do illegal script number or empty script,");
        }

        // For calling a macro, check that it is not too deep and that it's not circular
        // There is no good way to check circular function calls, so this relies on 
        // run-time testing
        if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
          mCallLevel++;
          if (mCallLevel >= MAX_CALL_DEPTH)
            FAIL_CHECK_LINE("Too many nested script calls");
          if (!CMD_IS(CALLFUNCTION)) {
            for (i = 0; i < mCallLevel; i++) {
              if (mCallMacro[i] == index && mCallFunction[i] == func)
                FAIL_CHECK_LINE("Trying to call a script that is already calling a "
                  "script");
            }
          }

          mCallMacro[mCallLevel] = index;
          mCallFunction[mCallLevel] = func;
        }

        // Now call this function reentrantly to check macro, unless it is a DoMacro
        // and already checked
        if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT) || CMD_IS(CALLFUNCTION)) ||
          !mAlreadyChecked[index]) {
          mAlreadyChecked[index] = true;
          i = CheckForScriptLanguage(index, true);
          if (i) {
            if (i > 0)
              FAIL_CHECK_LINE("You cannot run any Python scripts");
            if (mCalledFromScrpLang)
              FAIL_CHECK_LINE("You cannot call a Python script from a regular script"
                " called by a Python script");
          } else if (CheckBlockNesting(index, blockLevel, tryLevel))
            return 14;
        }

        // Skip out if doing macro; otherwise pop stack again
        if (CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))
          break;
        mCallLevel--;

        // Check finding a script and record the found number
      } else if (CMD_IS(FINDSCRIPTBYNAME)) {
          index = FindCalledMacro(strLine, true);
          if (index < 0)
            return 12;
          lastFoundScript = index + 1;

      // Check embedded python
      } else if (CMD_IS(PYTHONSCRIPT)) {
        if (mCalledFromScrpLang)
          FAIL_CHECK_LINE("You cannot run Python embedded in a regular script"
            " called by a Python script");

        if (!IsEmbeddedPythonOK(macroNum, currentIndex, lastInd, newCurrentInd))
          FAIL_CHECK_NOLINE("PythonScript line with no matching EndPythonScript line");
        startLine = CountLinesToCurIndex(macroNum, currentIndex);
        i = CheckForScriptLanguage(macroNum, true, 0, currentIndex, lastInd, startLine);
        if (!i)
          FAIL_CHECK_NOLINE("PythonScript line not followed by a #!Python line");
        if (i > 0)
          FAIL_CHECK_NOLINE("Cannot use PythonScript: you cannot run any Python scripts");
        currentIndex = newCurrentInd;

      } else if (CMD_IS(ENDPYTHONSCRIPT)) {
        FAIL_CHECK_NOLINE("EndPythonScript line with no preceding PythonScript line");

      // Examine loops
      } else if ((CMD_IS(LOOP) && !strItems[1].IsEmpty()) || 
        (CMD_IS(DOLOOP) && !strItems[3].IsEmpty()) || 
        (isIF && !strItems[3].IsEmpty()) || isTRY) {
        blockLevel++;
        subBlockNum[1 + blockLevel]++;
        if (blockLevel >= MAX_LOOP_DEPTH)
          FAIL_CHECK_NOLINE("Too many nested LOOP/DOLOOP, IF, and TRY statements");
        blockType[blockLevel] = B3DCHOICE(isTRY, 2, isIF ? 1 : 0);
        elseFound[blockLevel] = false;
        catchFound[blockLevel] = false;
        if (isTRY)
          tryLevel++;
        if (isIF && CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine, errmess,
          mCheckingParseQuotes))
          return 99;

      } else if (isELSE || isELSEIF) {
        subBlockNum[1 + blockLevel]++;
        if (blockLevel <= startLevel || blockType[blockLevel] != 1)
          FAIL_CHECK_NOLINE("ELSE or ELSEIF statement not in an IF block");
        if (elseFound[blockLevel])
          FAIL_CHECK_NOLINE("ELSEIF or ELSE following an ELSE statement in an IF block");
        if (isELSE)
          elseFound[blockLevel] = true;
        if (isELSE && strItems[1].MakeUpper() == "IF")
          FAIL_CHECK_NOLINE("ELSE IF must be ELSEIF without a space");
        if (isELSE && !strItems[1].IsEmpty())
          FAIL_CHECK_NOLINE("ELSE line with extraneous entries after the ELSE");
        if (isELSEIF && CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine,
          errmess, mCheckingParseQuotes))
          return 99;

      } else if (CMD_IS(CATCH)) {
        subBlockNum[1 + blockLevel]++;
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("CATCH statement not in a TRY block");
        if (blockType[blockLevel] != 2)
          FAIL_CHECK_NOLINE("CATCH contained in an LOOP, DOLOOP, or IF  block");
        if (catchFound[blockLevel])
          FAIL_CHECK_NOLINE("Second CATCH found in a TRY block");
        catchFound[blockLevel] = true;
        tryLevel--;

      } else if (CMD_IS(ENDLOOP)) {
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("ENDLOOP without corresponding LOOP or DOLOOP statement");
        if (blockType[blockLevel])
          FAIL_CHECK_NOLINE("ENDLOOP contained in an IF or TRY  block");
        blockLevel--;

      } else if (CMD_IS(ENDIF)) {
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("ENDIF without corresponding IF statement");
        if (blockType[blockLevel] != 1)
          FAIL_CHECK_NOLINE("ENDIF contained in a LOOP, DOLOOP, or TRY block");
        blockLevel--;

      } else if (CMD_IS(ENDTRY)) {
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("ENDTRY without corresponding TRY statement");
        if (blockType[blockLevel] != 2)
          FAIL_CHECK_NOLINE("ENDTRY contained in an LOOP, DOLOOP, or IF  block");
        if (!catchFound[blockLevel])
          FAIL_CHECK_NOLINE("NO CATCH was found in a TRY - ENDTRY block");
        blockLevel--;

      } else if (CMD_IS(BREAK) || CMD_IS(KEYBREAK) || CMD_IS(CONTINUE)) {
        inloop = 0;
        for (i = startLevel + 1; i <= blockLevel; i++) {
          if (!blockType[i])
            inloop = 1;
        }
        if (!inloop)
          FAIL_CHECK_NOLINE("BREAK, KEYBREAK, or CONTINUE not contained in a LOOP or "
            "DOLOOP block");

      } else if (CMD_IS(THROW) && tryLevel <= 0) {
        FAIL_CHECK_LINE("THROW seems not to be contained in any TRY block");

      } else if (CMD_IS(SKIPTO) && !strItems[1].IsEmpty()) {

        // For a SkipTo, record the location and level and subblock at all lower levels
        if (numSkips >= MAX_MACRO_SKIPS)
          FAIL_CHECK_NOLINE("Too many SkipTo commands in script");
        strSkips[numSkips] = strItems[1].MakeUpper();
        skipCurIndex[numSkips] = currentIndex;
        for (i = startLevel; i <= blockLevel; i++)
          skipSubBlocks[numSkips][1 + i] = subBlockNum[1 + i];
        skipBlockLevel[numSkips++] = blockLevel;

      } else if (CMD_IS(FUNCTION) && !strItems[1].IsEmpty()) {

        // For a function, the loop level must be back to base, must not be in a function,
        // and the 3rd and 4th items must be integers
        if (inFunc)
          FAIL_CHECK_LINE("Starting a new function without ending previous one on line");
        inFunc = true;
        if (blockLevel > startLevel)
          FAIL_CHECK_LINE("Unclosed IF, LOOP, DOLOOP or TRY block in script before "
            "function");
        for (i = 2; i < 4; i++) {
          intCheck = "";
          if (!strItems[i].IsEmpty()) {
            index = atoi((LPCTSTR)strItems[i]);
            intCheck.Format("%d", index);
          }
          if (intCheck != strItems[i])
            FAIL_CHECK_LINE("Function name must be in one word without spaces, followed"
              " by\nnumber of arguments and integer flag for whether there is a string "
              "argument");
        }

      } else if (CMD_IS(ENDFUNCTION)) {
        if (!inFunc)
          FAIL_CHECK_NOLINE("An EndFunction command occurred when not in a function");
        inFunc = false;
        if (blockLevel > startLevel)
          FAIL_CHECK_NOLINE("Unclosed IF, LOOP, DOLOOP or TRY block inside function");

      } else {

        i = CheckLegalCommandAndArgNum(&strItems[0], cmdIndex, strLine, macroNum);
        if (i == 17)
          return 17;
        if (i)
          FAIL_CHECK_LINE("Unrecognized command");
      }

      // Check commands where arithmetic is allowed
      if (ArithmeticIsAllowed(strItems[0]) && 
        CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine, errmess,
          mCheckingParseQuotes))
        return 99;
      
      if (CMD_IS(SKIPIFVERSIONLESSTHAN) && currentIndex < macro->GetLength())
        GetNextLine(macro, currentIndex, strLine);
    }
  }
  if (blockLevel > startLevel)
    FAIL_CHECK_NOLINE("Unclosed IF, LOOP, or DOLOOP block");
  if (inComment)
    FAIL_CHECK_NOLINE("A comment started with /* was not closed with */");
  if (inFunc)
    FAIL_CHECK_NOLINE("The last function was missing an EndFunction command");

  if (!numSkips)
    return 0;

  // Validate the labels if there are any skips
  for (skipInd = 0; skipInd < numSkips; skipInd++) {
    for (labInd = 0; labInd < numLabels; labInd++) {
      if (strSkips[skipInd] == strLabels[labInd]) {
        if (labelCurIndex[labInd] < skipCurIndex[skipInd])
          FAIL_CHECK_NOLINE(CString("You cannot skip backwards; label occurs before "
          "SkipTo ") + strSkips[skipInd]);
        if (labelBlockLevel[labInd] > skipBlockLevel[skipInd])
          FAIL_CHECK_NOLINE(CString("Trying to skip into a higher level LOOP, DOLOOP, or"
            " IF block with SkipTo ") + strSkips[skipInd]);
        if (labelSubBlock[labInd] != skipSubBlocks[skipInd][1 + labelBlockLevel[labInd]])
          FAIL_CHECK_NOLINE(CString("Trying to skip into a different LOOP/DOLOOP or "
            "TRY/CATCH block or section of IF block with SkipTo ") + strSkips[skipInd]);
        break;
      }
    }
    if (labInd == numLabels)
      FAIL_CHECK_NOLINE(CString("Label not found for SkipTo ") + strSkips[skipInd]);
  }
  return 0;
}


// Check for whether the command is legal and has enough arguments
// Returns 17 if there are not enough arguments, 1 if not legal command
int CMacroProcessor::CheckLegalCommandAndArgNum(CString * strItems, int cmdIndex,
  CString strLine, int macroNum)
{
  int i = cmdIndex;
  CString errmess;

  if (cmdIndex == CME_NOTFOUND || (cmdIndex >= 0 && cmdIndex < CME_EXIT) ||
    mPythonOnlyCmdSet.count(cmdIndex))
    return 1;

  if (strItems[mCmdList[i].minargs].IsEmpty()) {
    errmess.Format("The command must be followed by at least %d entries\n"
      " on this line in script #%d:\n\n", mCmdList[i].minargs, macroNum + 1);
    SEMMessageBox(errmess + strLine, MB_EXCLAME);
    return 17;
  }
  return 0;
}

// Check a line for balanced parentheses and no empty clauses
int CMacroProcessor::CheckBalancedParentheses(CString *strItems, int maxItems, 
  CString &strLine, CString &errmess, bool useQuotes)
{
  int ind, level = 0;
  if (SeparateParentheses(strItems, maxItems, useQuotes))
    FAIL_CHECK_LINE("Too many items in line after separating out parentheses")
    
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    if (strItems[ind] == "(") {
      level++;
    }
    if (strItems[ind] == ")") {
      level--;
      if (level < 0)
        FAIL_CHECK_LINE("Unbalanced parentheses");
      if (strItems[ind - 1] == "(")
        FAIL_CHECK_LINE("Nothing inside parentheses");
    }
  }
  if (level != 0)
    FAIL_CHECK_LINE("Unbalanced parentheses");
  return 0;
}

// Expand the line to put parentheses in separate items, return error if too many items
int CMacroProcessor::SeparateParentheses(CString *strItems, int maxItems, bool useQuotes)
{
  int icopy, length, ind = maxItems - 1;
  int numItems = -1;
  while (ind >= 0) {

    // Move from the back end, skip empty items, and initialize number of items
    if (strItems[ind].IsEmpty()) {
      ind--;
      continue;
    }
    if (numItems < 0)
      numItems = ind + 1;
    length = strItems[ind].GetLength();

    // Expand an item with more than one character
    if (length > 1 && (strItems[ind].GetAt(0) == '(' ||
      (strItems[ind].GetAt(length - 1) == ')' && 
        !(useQuotes && strItems[ind].Find('(') > 0)))) {
        if (numItems >= maxItems)
          return 1;
        for (icopy = numItems; icopy > ind + 1; icopy--)
          strItems[icopy] = strItems[icopy - 1];
        numItems++;
        if (strItems[ind].GetAt(0) == '(') {
          strItems[ind + 1] = strItems[ind].Right(length - 1);
          strItems[ind] = "(";
          ind++;
        } else {
          strItems[ind] = strItems[ind].Left(length - 1);
          strItems[ind + 1] = ")";
        }
    } else {
      ind--;
    }
  }
  return 0;
}

void CMacroProcessor::PrepareForMacroChecking(int which)
{
  for (int i = 0; i < MAX_TOT_MACROS; i++)
    mAlreadyChecked[i] = false;
  mCallLevel = 0;
  mCallMacro[0] = which;
}

// Skips to the end of a block or to a catch or else statement, leaves current index
// at start of statement to be processed (the end statement, or past an else or catch)
int CMacroProcessor::SkipToBlockEnd(int type, CString line, int *numPops, 
  int *delTryLevel)
{
  CString *macro = &mMacros[mCurrentMacro];
  CString strLine, strItems[4];
  int ifLevel = 0, loopLevel = 0, tryLevel = 0, popTry = 0, funcLevel = 0;
  int nextIndex = mCurrentIndex, cmdIndex;
  bool isCATCH;
  if (numPops)
    *numPops = 0;
  if (delTryLevel)
    *delTryLevel = 0;
  while (nextIndex < macro->GetLength()) {
    mCurrentIndex = nextIndex;
    GetNextLine(macro, nextIndex, strLine);
    if (!strLine.IsEmpty()) {
      mParamIO->ParseString(strLine, strItems, 4);
      strItems[0].MakeUpper();
      cmdIndex = LookupCommandIndex(strItems[0]);
      isCATCH = CMD_IS(CATCH);

      // mTryCatchLevel is decremented on the catch because you are out of try at that
      // point, so we need to keep track of changes that need to be applied to that var
      // separate from the number of try levels that are popping, which is popTry and
      // is incremented on try and decremented on endtry
      if (isCATCH)
        tryLevel--;

      // Keep track if in function or leaving a function; if leave a function after
      // starting in  one, that is the end of the call level and need to quit; otherwise
      // if in function, just skip to next line
      if (CMD_IS(FUNCTION))
        funcLevel++;
      if (CMD_IS(ENDFUNCTION))
        funcLevel--;
      if (funcLevel < 0)
        break;
      if (funcLevel > 0)
        continue;

      // If we're at same block level as we started and we find end, return
      // Go to next statement for catch and and else, otherwise run the statement we
      // skipped to
      if ((!ifLevel && ((type == SKIPTO_ELSE_ENDIF && (CMD_IS(ELSE) || CMD_IS(ELSEIF)))
        || ((type == SKIPTO_ENDIF || type == SKIPTO_ELSE_ENDIF) && CMD_IS(ENDIF))) ||
        (!loopLevel && type == SKIPTO_ENDLOOP && CMD_IS(ENDLOOP))) ||
        (!popTry && type == SKIPTO_CATCH && isCATCH) || 
        (!popTry && type == SKIPTO_ENDTRY && CMD_IS(ENDTRY))) {
        if (CMD_IS(ELSE) || isCATCH)
          mCurrentIndex = nextIndex;
        if (numPops)
          *numPops = -(ifLevel + loopLevel + popTry);
        if (delTryLevel)
          *delTryLevel = tryLevel;
        return 0;
      }

      // Otherwise keep track of the block level as it goes up and down
      if (CMD_IS(LOOP))
        loopLevel++;
      if (CMD_IS(IF))
        ifLevel++;
      if (CMD_IS(TRY)) {
        tryLevel++;
        popTry++;
      }
      if (CMD_IS(ENDLOOP))
        loopLevel--;
      if (CMD_IS(ENDIF))
        ifLevel--;
      if (CMD_IS(ENDTRY))
        popTry--;
    }
  }
  if (!line.IsEmpty())
    AfxMessageBox("No appropriate statement found to skip forward to from script "
      "line:\n\n" + line, MB_EXCLAME);
  return 1;
}

// Skip to a label and return the number of blocks levels to descend
int CMacroProcessor::SkipToLabel(CString label, CString line, int &numPops, 
  int &delTryLevel)
{
  CString *macro = &mMacros[mCurrentMacro];
  CString strLine, strItems[4];
  int nextIndex = mCurrentIndex;
  int cmdIndex;
  label += ":";
  numPops = 0;
  delTryLevel = 0;
  while (nextIndex < macro->GetLength()) {
    mCurrentIndex = nextIndex;
    GetNextLine(macro, nextIndex, strLine);
    if (!strLine.IsEmpty()) {
      mParamIO->ParseString(strLine, strItems, 4);
      strItems[0].MakeUpper();
      cmdIndex = LookupCommandIndex(strItems[0]);

      // For a match, make sure there is not a negative number of pops
      if (strItems[0] == label) {
        if (numPops >= 0)
          return 0;
        AfxMessageBox("Trying to skip into a higher block level in script line:\n\n" +
          line, MB_EXCLAME);
        return 1;
      }

      // Otherwise keep track of the block level as it goes up and down
      if (CMD_IS(LOOP) || CMD_IS(IF) || CMD_IS(TRY))
        numPops--;
      if (CMD_IS(ENDLOOP) || CMD_IS(ENDIF) || CMD_IS(ENDTRY))
        numPops++;
      if (CMD_IS(TRY))
        delTryLevel++;
      if (CMD_IS(CATCH))
        delTryLevel--;
    }
  }
  AfxMessageBox("No label found to skip forward to from script line:\n\n" + line, 
    MB_EXCLAME);
  return 1;
}

// Performs all changes needed when leaving a call level (script or function) and 
// returning to the caller
void CMacroProcessor::LeaveCallLevel(bool popBlocks)
{
  // For a return or an exception from a try, pop any loops, clear index variables
  if (popBlocks) {
    while (mBlockDepths[mCallLevel] >= 0) {
      ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
  }
  ClearVariables(VARTYPE_LOCAL, mCallLevel);
  if (mCallFunction[mCallLevel])
    mCallFunction[mCallLevel]->wasCalled = false;
  if (mCurrentMacro == mNeedClearTempMacro && mClearTempAtCallLevel == mCallLevel) {
    mMacros[mNeedClearTempMacro] = "";
    mNumTempMacros--;
    mNeedClearTempMacro = -1;
  }
  mCallLevel--;
  mCurrentMacro = mCallMacro[mCallLevel];
  mCurrentIndex = mCallIndex[mCallLevel];
  mLastIndex = -1;
  if (!mCallLevel && mCalledFromScrpLang) {
    mCalledFromScrpLang = false;
    mRunningScrpLang = true;
  }
}

// Common place to check for reserved control commands
BOOL CMacroProcessor::WordIsReserved(CString str)
{
  std::string stdStr = (LPCTSTR)str;
  return mReservedWords.count(stdStr) > 0;
}

// Common place to check if arithmetic is allowed for a command
bool CMacroProcessor::ArithmeticIsAllowed(CString &str)
{
  std::string sstr = (LPCTSTR)str;
  return ((str.Find("SET") == 0 && mArithDenied.count(sstr) <= 0) || 
    mArithAllowed.count(sstr) > 0);
}

// Check for the return values from an intensity change and issue warning or message
int CMacroProcessor::CheckIntensityChangeReturn(int err)
{
  CString report;
  if (err) {
    if (err == BEAM_STARTING_OUT_OF_RANGE || err == BEAM_ENDING_OUT_OF_RANGE) {
      mWinApp->AppendToLog("Warning: attempting to set beam strength beyond"
        " calibrated range", LOG_OPEN_IF_CLOSED);
    } else {
      report = "Error trying to change beam strength";
      if (err == BEAM_STRENGTH_NOT_CAL || err == BEAM_STRENGTH_WRONG_SPOT)
        report += "\nBeam strength is not calibrated for this spot size";
      SEMMessageBox(report, MB_EXCLAME);
      AbortMacro();
      return 1;
    }
  }
  return 0;
}

// Make sure a filename is not empty and try to convert to a full path
int CMacroProcessor::CheckConvertFilename(CString * strItems, CString strLine, int index,
                                          CString & strCopy)
{
  char absPath[_MAX_PATH];
  char *fullp;
  if (strItems[index].IsEmpty()) {
    SEMMessageBox("Missing filename in statement:\n\n" + strLine, MB_EXCLAME);
    AbortMacro();
    return 1;
  }

  // Get the filename in original case
  SubstituteVariables(&strLine, 1, strLine);
  JustStripItems(strLine, index, strCopy);
  fullp = _fullpath(absPath, (LPCTSTR)strCopy, _MAX_PATH);
  if (!fullp) {
    SEMMessageBox("The filename cannot be converted to an absolute path in statement:"
      "\n\n" + strLine, MB_EXCLAME);
    AbortMacro();
    return 1;
  }
  strCopy = fullp;
  return 0;
}

CmdItem *CMacroProcessor::GetCommandList(int & numCommands)
{
  numCommands = mNumCommands - 1;
  return &mCmdList[0];
}

// Converts a buffer letter in the item to a buffer index, supplies emptyDefault as the
// result for an empty and returns false if that is >= 0, true otherwise; checks legality
// of letter and also of image in buffer if checkImage true; composes message on error
bool CMacroProcessor::ConvertBufferLetter(CString strItem, int emptyDefault,
  bool checkImage, int &bufIndex, CString &message, bool fftAllowed)
{
  EMimageBuffer *imBuf;
  strItem.MakeUpper();
  if (strItem.IsEmpty()) {
    bufIndex = emptyDefault;
    if (bufIndex < 0) {
      message = "No buffer letter is present in statement: \r\n\r\n";
      return true;
    }
  } else {
    bufIndex = atoi((LPCTSTR)strItem);
    if (!bufIndex) {
      bufIndex = (int)strItem.GetAt(0) - (int)'A';
      if (strItem.GetLength() == 2 && strItem.GetAt(1) == 'F' && fftAllowed) {
        if (bufIndex < 0 || bufIndex >= MAX_FFT_BUFFERS) {
          message = "Improper FFT buffer letter " + strItem + " in statement: \r\n\r\n";
          return true;
        }
        if (!mWinApp->mFFTView) {
          message = "The FFT window is not open and no buffers are available for "
            "statement: \r\n\r\n";
          return true;
        }
        bufIndex = -1 - bufIndex;
      } else {
        if (bufIndex < 0 || bufIndex >= MAX_BUFFERS || strItem.GetLength() > 1) {
          message = "Improper buffer letter " + strItem + " in statement: \r\n\r\n";
          return true;
        }
      }
    } else {
      if (bufIndex < 1 || bufIndex > MAX_BUFFERS || strItem.GetLength() >
        1 + (bufIndex / 10)) {
        message = "Improper buffer number " + strItem + " in statement: \r\n\r\n";
        return true;
      }
      bufIndex--;
    }
  }
  imBuf = ImBufForIndex(bufIndex);
  if (checkImage && !imBuf->mImage) {
    message = "There is no image in buffer " + strItem + " in statement: \r\n\r\n";
    return true;
  }
  return false;
}

// Convert anything starting with C, O, or S to value for condenser (2), objective,
// and selected area
void CMacroProcessor::ConvertApertureNameToNum()
{
  char firstLet = mStrItems[1].GetAt(0);
  if (firstLet == 'c' || firstLet == 'C')
    mItemInt[1] = 1;
  if (firstLet == 'o' || firstLet == 'O')
    mItemInt[1] = 2;
  if (firstLet == 's' || firstLet == 'S')
    mItemInt[1] = 4;
}

// Takes incoming  binning and scales binning
// for K2, and checks that binning exists.  Returns true with message on error
bool CMacroProcessor::CheckCameraBinning(double binDblIn, int &binning, CString &message)
{
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  binning = B3DNINT(BinDivisorF(camParams) * binDblIn);
  for (int ix0 = 0; ix0 < camParams->numBinnings; ix0++)
    if (binning == camParams->binnings[ix0])
      return false;
  message = "Not a valid binning for camera in: \n\n";
  return true;
}

// Takes incoming set number or letter in strItem and itemInt, checks its validity, and 
// puts a set number converted from a letter in itemInt and copy the number to index
// Returns true with message on error.  Also assigns conset pointer to mCamSet
bool CMacroProcessor::CheckAndConvertCameraSet(CString &strItem, int &itemInt, int &index,
                                             CString &message)
{
  char letter;
  itemInt = -1;

  // To allow strings from a reported value, if it starts with a digit, strip trailing 0's
  // and the decimal place if that is all that is after the digit
  letter = (char)strItem.GetAt(0);
  if (letter >= '0' && letter <= '9') {
    while (strItem.Find('.') >= 0 && strItem.Right(1) == "0")
      strItem.Delete(strItem.GetLength() - 1);
    if (strItem.GetLength() == 2 || strItem.Right(1) == ".")
      strItem.Delete(1);
  }
  if (strItem.GetLength() == 1) {
    letter = (char)strItem.GetAt(0);
    if (!strItem.CompareNoCase("V"))
      itemInt = 0;
    else if (!strItem.CompareNoCase("F"))
      itemInt = 1;
    else if (!strItem.CompareNoCase("T"))
      itemInt = 2;
    else if (!strItem.CompareNoCase("R"))
      itemInt = 3;
    else if (!strItem.CompareNoCase("P"))
      itemInt = 4;
    else if (!strItem.CompareNoCase("S"))
      itemInt = 5;
    else if (!strItem.CompareNoCase("M"))
      itemInt = 6;
    else if (letter >= '0' && letter <= '9')
      itemInt = atoi((LPCTSTR)strItem);
    if (itemInt > NUMBER_OF_USER_CONSETS)
      itemInt = -1;
    index = itemInt;
    if (itemInt >= 0) {
      mCamSet = &mConSets[index];
      return false;
    }
  }
  message = "Inappropriate parameter set letter/number in: \n\n";
  return true;
}

// If adjustment of beam tilt for given image shift is requested and available, do it
int CMacroProcessor::AdjustBeamTiltIfSelected(double delISX, double delISY, BOOL doAdjust,
  CString &message)
{
  double astigX, astigY, BTX, BTY;
  int probe = mScope->GetProbeMode();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  if (!doAdjust)
    return 0;
  if (comaVsIS->magInd <= 0) {
    message = "There is no calibration of beam tilt needed to compensate image shift "
      "in:\n\n";
    return 1;
  }
  if (AdjustBeamTiltAndAstig(delISX, delISY, BTX, BTY, astigX, astigY)) {
    if (mAstigXtoRestore[probe] < EXTRA_VALUE_TEST) {
      mAstigXtoRestore[probe] = astigX;
      mAstigYtoRestore[probe] = astigY;
      mNumStatesToRestore++;
    }
  }
  if (mBeamTiltXtoRestore[probe] < EXTRA_VALUE_TEST) {
    mBeamTiltXtoRestore[probe] = BTX;
    mBeamTiltYtoRestore[probe] = BTY;
    mNumStatesToRestore++;
  }
  mCompensatedBTforIS = true;
  return 0;
}

// Actually do the adjustment, returning starting BT and astig and true if did astig also
bool CMacroProcessor::AdjustBeamTiltAndAstig(double delISX, double delISY, double &BTX,
  double &BTY, double &astigX, double &astigY)
{
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  double delBTX, delBTY, transISX, transISY;
  mScope->GetBeamTilt(BTX, BTY);
  mShiftManager->TransferGeneralIS(mScope->FastMagIndex(), delISX, delISY,
    comaVsIS->magInd, transISX, transISY);
  if (mNavHelper->GetSkipAstigAdjustment() >= 0) {
    delBTX = comaVsIS->matrix.xpx * transISX + comaVsIS->matrix.xpy * transISY;
    delBTY = comaVsIS->matrix.ypx * transISX + comaVsIS->matrix.ypy * transISY;
    mWinApp->mAutoTuning->BacklashedBeamTilt(BTX + delBTX, BTY + delBTY,
      mScope->GetAdjustForISSkipBacklash() <= 0);
  }
  mCompensatedBTforIS = true;
  if (comaVsIS->astigMat.xpx && mNavHelper->GetSkipAstigAdjustment() <= 0) {
    mScope->GetObjectiveStigmator(astigX, astigY);
    delBTX = comaVsIS->astigMat.xpx * transISX + comaVsIS->astigMat.xpy * transISY;
    delBTY = comaVsIS->astigMat.ypx * transISX + comaVsIS->astigMat.ypy * transISY;
    mWinApp->mAutoTuning->BacklashedStigmator(astigX + delBTX, astigY + delBTY,
      mScope->GetAdjustForISSkipBacklash() <= 0);
    return true;
  }
  return false;
}

int CMacroProcessor::AdjustBTApplyISSetDelay(double delISX, double delISY, BOOL doAdjust,
  BOOL setDelay, double scale, CString &message)
{
  if (AdjustBeamTiltIfSelected(delISX, delISY, doAdjust, message))
    return 1;

  // Make the change in image shift
  mScope->IncImageShift(delISX, delISY);
  if (setDelay && scale > 0)
    mShiftManager->SetISTimeOut((float)scale * mShiftManager->GetLastISDelay());
  return 0;
}

// Tests whether a prospective image shift is within limits
int CMacroProcessor::TestIncrementalImageShift(double delX, double delY)
{
  CString str;
  if (!mShiftManager->ImageShiftIsOK(delX, delY, true)) {
    str.Format("Image shift by %.3f %.3f would exceed the limit and is not being done", 
      delX, delY);
    mWinApp->AppendToLog(str, mLogAction);
    SetReportedValues(1.);
    return 1;
  }
  SetReportedValues(0.);
  return 0;
}

// Saves a control set in the stack if it has not been saved already
void CMacroProcessor::SaveControlSet(int index)
{
  for (int ind = 0; ind < (int)mConsetNums.size(); ind++)
    if (index == mConsetNums[ind])
      return;
  mConsetNums.push_back(index);
  mConsetsSaved.push_back(mConSets[index]);
}

// Restores a single control set given by index, or all control sets if index is < 0
// Erases the corresponding saved set if erase is true; otherwise (and if index is < 0)
// 
bool CMacroProcessor::RestoreCameraSet(int index, BOOL erase)
{
  bool retval = true;
  if (index < 0 && !erase) {
    mChangedConsets.clear();
    mCamWithChangedSets = mWinApp->GetCurrentCamera();
  }
  for (int ind = (int)mConsetNums.size() - 1; ind >= 0; ind--) {
    if (mConsetNums[ind] == index || index < 0) {
      if (index < 0 && !erase)
        mChangedConsets.push_back(mConSets[mConsetNums[ind]]);
      mConSets[mConsetNums[ind]] = mConsetsSaved[ind];
      if (erase) {
        mConsetsSaved.erase(mConsetsSaved.begin() + ind);
        mConsetNums.erase(mConsetNums.begin() + ind);
      }
      retval = false;
    }
  }
  return retval;
}

// Given info for a cartridge, format both output for the log table and a row for an array
void CMacroProcessor::FormatCartridgeInfo(int index, int &id, int &station, int &slot,
  int &cartType, int &rotation, CString &name, CString &report, CString &rowVal)
{
  const char *stations[4] = {"Unknown", "Magazine", "Storage ", "Stage   "};
  JeolCartridgeData jcd;
  CArray<JeolCartridgeData, JeolCartridgeData> *loaderInfo = mScope->GetJeolLoaderInfo();
  id = -1;
  if (index < 0 || index >= (int)loaderInfo->GetSize())
    return;
  jcd = loaderInfo->GetAt(index);
  id = jcd.id;
  station = jcd.station;
  slot = jcd.slot;
  cartType = jcd.type;
  rotation = jcd.rotation;
  name = jcd.name;
  report.Format("%2d %7d  %s  %d    %d    %d    %s", index + 1, id, stations[station],
    slot, cartType, rotation, (LPCTSTR)name);
  rowVal.Format("%d\n%d\n%d\n%d\n%d\n", id, station, slot, cartType, rotation);
  rowVal += name;
}

// Returns a navigator item specified by a positive index, a negative distance from end
// of table (-1 = last item), or 0 for current or acquire item.  Updates the value of
// index with the true 0-based value.  Issues message, aborts and returns NULL on failures
CMapDrawItem *CMacroProcessor::CurrentOrIndexedNavItem(int &index, CString &strLine)
{
  CMapDrawItem *item;
  if (!mWinApp->mNavigator) {
    ABORT_NORET_LINE("The Navigator must be open to execute line:\n\n");
    return NULL;
  }
  if (index > 0)
    index--;
  else if (index < 0)
    index = mWinApp->mNavigator->GetNumNavItems() + index;
  else {
    index = mWinApp->mNavigator->GetCurrentOrAcquireItem(item);
    if (index < 0) {
      ABORT_NORET_LINE("There is no current Navigator item for line:\n\n");
      return NULL;
    }
  }
  item = mWinApp->mNavigator->GetOtherNavItem(index);
  if (!item) {
    ABORT_NORET_LINE("Index is out of range in statement:\n\n");
    return NULL;
  }
  return item;
}

// Computes number of montage pieces needed to achieve the minimum size in microns given
// the camera number of pixels in camSize and fractional overlap
int CMacroProcessor::PiecesForMinimumSize(float minMicrons, int camSize, 
  float fracOverlap)
{
  int binning = mConSets[RECORD_CONSET].binning;
  int magInd = mScope->GetMagIndex();
  float pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), magInd) * 
    binning;
  int minPixels = (int)(minMicrons / pixel);
  camSize /= binning;
  return (int)ceil(1. + (minPixels - camSize) / ((1. - fracOverlap) * camSize));
}

// Transfer macro if open and give error if macro is empty (1) or fails (2)
int CMacroProcessor::EnsureMacroRunnable(int macnum)
{
  int err, tryLevel = 0;
  if (mMacroEditer[macnum])
    mMacroEditer[macnum]->TransferMacro(true);
  if (mMacros[macnum].IsEmpty()) {
    SEMMessageBox("The script you selected to run is empty", MB_EXCLAME);
    return 1;
  } 
  PrepareForMacroChecking(macnum);
  err = CheckForScriptLanguage(macnum, false);
  mMacroForScrpLang = "";
  if (err > 0 || (!err && CheckBlockNesting(macnum, -1, tryLevel)))
    return 2;
 return 0;
}

// Test for whether the script starts with a #! for external script language and then
// try to get the functions, then look for #include and concatenate all included scripts
int CMacroProcessor::CheckForScriptLanguage(int macNum, bool justCheckStart, int argInd,
  int currentInd, int lastInd, int startLine)
{
  bool isPython;
  int ind, fail, size, cumulLine = 0, indent = 0, lineNum = 0, firstRealLine = -1;
  int pyVersion = 0;
  CFile *file = NULL;
  unsigned char *buffer = NULL;
  CString indentStr, fileName, fileStr, errStr, modulePath = mPyModulePath;
  CString includePath, tryPath;
  CFileStatus status;

  // Test for scripting language
  int includeInd = -1, count;
  CString name, line;
  if (lastInd < 0)
    lastInd = mMacros[macNum].GetLength();
  while (currentInd < lastInd) {
    GetNextLine(&mMacros[macNum], currentInd, line, true);
    if (line.Find("#!") == 0)
      break;
    name = line;
    name.Replace("#", "");
    name = name.Trim();
    if (!name.IsEmpty())
      break;
  }
  if (line.Find("#!") != 0)
    return 0;
  if (!mScrpLangDoneEvent) {
    if (!justCheckStart)
      mWinApp->AppendToLog("Cannot run external scripting language without object"
        " for communicating with it");
    return 1;
  }
  if (justCheckStart)
    return -1;

  count = line.GetLength();
  while (count > 1 && (line.GetAt(count - 1) == '\n' || line.GetAt(count - 1) == '\r' ||
    line.GetAt(count - 1) == ' '))
    count--;
  name = line.Mid(2, count - 2);

  // determine if Python and either start server or check for plugin functions
  line = name;
  line.MakeLower();
  isPython = line.Find("pyth") == 0;
  mScrpLangData.strItems[0] = "";
  if (isPython) {
    if (mWinApp->mPythonServer->StartServerIfNeeded(RUN_PYTH_SOCK_ID))
      return 1;

    // If there is a full "python" see if there is one matching the version number given
    // after it, or take the first entry if there is no version number.
    if (line.Find("python") == 0) {
      line = line.Mid(6).Trim();
      for (ind = 0; ind < (int)mPathsToPython.size(); ind++) {
        if ((line.IsEmpty() && !ind) || (!line.IsEmpty() && 
          mVersionsOfPython[ind].find((LPCTSTR)line) == 0)) {
          mScrpLangData.strItems[0] = mPathsToPython[ind].c_str();
          mScrpLangData.strItems[0] += "\\";
          pyVersion = 100 * atoi(mVersionsOfPython[ind].c_str()) +
            atoi(mVersionsOfPython[ind].c_str() + 2);
          if (pyVersion >= 207 && pyVersion <= 304)
            modulePath = mPyModulePath + "/" + mVersionsOfPython[ind].c_str();
        }
      }

      // Nothing found is bad if they specified one
      if (mScrpLangData.strItems[0].IsEmpty() && !line.IsEmpty()) {
        SEMMessageBox("Could not find a PathToPython entry with a version"
          " matching " + name);
        return 1;
      }
      mScrpLangData.strItems[0] = "\"" + mScrpLangData.strItems[0] + "python.exe\"";
    }
  } else {
    mScrpLangFuncs = mWinApp->mPluginManager->GetScriptLangFuncs(name);
    if (!mScrpLangFuncs) {
      SEMMessageBox("There is no scripting language plugin loaded whose\n"
        "name matches the one at the start of this script, " + name);
      return 1;
    }
  }

  // Determine if python and start wrapper
  mMacroForScrpLang = "";
  mLineInSrcMacro.clear();
  mIndexOfSrcLine.clear();
  mMacNumAtScrpLine.clear();
  mMacStartLineInScrp.clear();
  mIncludedFiles.clear();
  if (isPython) {
    startLine += 27;
    name.Format("sys.path.insert(0, \"%s\")\r\n", modulePath);
    line.Format("ConnectToSEM(%d)\r\n", CBaseServer::GetPortForSocketIndex(0));
    mMacroForScrpLang = "import sys\r\n" +
      name +
      "from serialem import SEMexited, SEMerror, ConnectToSEM\r\n"
      "from serialem import Echo as SEMecho\r\n"
      "from serialem import ScriptIsInitialized as SEMscriptIsInitialized\r\n"
      "SEMscriptIsInitialized()\r\n" +
      line +
      "def SEMarrayToFloats(var):\r\n"
      "  if not isinstance(var, str):\r\n"
      "    return [float(var)]\r\n"
      "  ret = []\r\n"
      "  for item in var.split('\\n'):\r\n"
      "    ret.append(float(item))\r\n"
      "  return ret\r\n"
      "def SEMarrayToInts(var):\r\n"
      "  ret = []\r\n"
      "  for item in var.split('\\n'):\r\n"
      "    ret.append(int(item))\r\n"
      "  return ret\r\n"
      "def listToSEMarray(var):\r\n"
      "  ret = ''\r\n"
      "  for val in var:\r\n"
      "    ret += str(val) + '\\n'\r\n"
      "  return ret[:-1]\r\n"
      "SEMargStrings = [";
    if (argInd > 0) {
      for (ind = argInd; ind < MAX_MACRO_TOKENS; ind++) {
        if (mItemEmpty[ind])
          break;
        if (mStrItems[ind].Replace("\\", "\\\\"))
          mStrItems[ind].Replace("|\\\\", "\\");
        if (ind > argInd)
          mMacroForScrpLang += ", ";
        mMacroForScrpLang += "\"\"\"" + mStrItems[ind] + "\"\"\"";
      }
    }
    mMacroForScrpLang += "]\r\n";
    if (pyVersion < 1 || pyVersion >= 300) {
      mMacroForScrpLang += "def SEMprint(*args):\r\n"
        "  print(*args, flush = True)\r\n"
        "SEMflush = sys.stdout.flush\r\n";
      startLine += 2;
    }
    mMacroForScrpLang +=
      "try:\r\n";
    for (ind = 0; ind < startLine; ind++) {
      mLineInSrcMacro.push_back(-1);
      mIndexOfSrcLine.push_back(-1);
    }
    for (ind = 0; ind < 2; ind++)
      indentStr += ' ';
  }

  // Setup first block of actual script and add the first line
  mMacNumAtScrpLine.push_back(macNum);
  mMacStartLineInScrp.push_back((int)mLineInSrcMacro.size());

  mIndexOfSrcLine.push_back(0);
  mLineInSrcMacro.push_back(lineNum++);
  mIncludedFiles.push_back("");
  mMacroForScrpLang += indentStr + line;

  // Now build up the string, indenting for python and inserting any #include scripts in
  // place
  while (currentInd < lastInd) {
    mIndexOfSrcLine.push_back(currentInd);
    mLineInSrcMacro.push_back(lineNum++);
    GetNextLine(&mMacros[macNum], currentInd, line, true);

    // Escape the backslashes to prevent them from being interpreted as escape code
    // but restore intended escape codes prefixed by |
    if (isPython)
      DoReplacementsInPythonLine(line);
    mMacroForScrpLang += indentStr + line;

    // Keep track of the first non-blank line so that the "maybe" statements can be
    // added to tracebacks
    name = line.Trim(" \n\r");
    if (firstRealLine < 0 && !name.IsEmpty() && name.GetAt(0) != '#')
      firstRealLine = (int)mLineInSrcMacro.size() - 1;

    if (name.IsEmpty())
      continue;
    if (name.Find("#include") != 0)
      continue;

    // Process an #includeFile entry
    if (name.Find("#includeFile") == 0 || name.Find("#includefile") == 0) {
      fail = 0;
      includePath = mPyIncludePath;

      // get the file name
      fileName = name.Mid((int)strlen("#includefile"));
      while (fileName.GetLength() > 0 && fileName.GetAt(0) == ' ')
        fileName = fileName.Mid(1);
      if (fileName.IsEmpty()) {
        SEMMessageBox("A filename must be included after #includeFile");
        return 1;
      }

      // If there is no include path or it looks like an absolute path, use it as is
      if (includePath.IsEmpty() || (fileName.GetAt(0) == '\\' || fileName.GetAt(0) == '/' ||
        (fileName.GetLength() > 3 && fileName.GetAt(1) == ':' &&
        (fileName.GetAt(2) == '\\' || fileName.GetAt(2) == '/')))) {
        if (!CFile::GetStatus((LPCTSTR)fileName, status)) {
          SEMMessageBox("File to be included does not exist: " + fileName);
          return 1;
        }
      } else {

        // Otherwise put current directory on front of path then extract each path entry
        includePath = ".;" + includePath;
        for (;;) {
          ind = includePath.Find(";");
          if (ind > 0) {
            tryPath = includePath.Left(ind);

            // Leave shorter path or set index -1 if at end
            if (includePath.GetLength() > ind + 1)
              includePath = includePath.Mid(ind + 1);
            else
              ind = -1;
          } else
            tryPath = includePath;

          // If file is there, great, make full name and proceed
          if (CFile::GetStatus((LPCTSTR)(tryPath + "\\" + fileName), status)) {
            fileName = tryPath + "\\" + fileName;
            break;
          }

          // If index is no good, it is all over, can't find it
          if (ind <= 0) {
            SEMMessageBox("Cannot find " + fileName +
              " in the current path or on the PythonIncludePath");
            return 1;
          }
        }
      }

      // Get the text just as when loading a script in editor
      try {
        errStr = "Opening included file " + fileName;
        file = new CFile(fileName, CFile::modeRead | CFile::shareDenyWrite);
        size = (int)file->GetLength();
        NewArray(buffer, unsigned char, size + 5);
        if (buffer) {
          errStr = "Reading included file " + fileName;
          ind = file->Read((void *)buffer, size);
          B3DCLAMP(ind, 0, size);
          buffer[ind] = 0x00;
          fileStr = buffer;
        } else {
          errStr = "Allocating memory for reading included file " + fileName;
          fail = 1;
        }
      }
      catch (CFileException *err) {
        err->Delete();
        fail = 1;
      }

      // Clean up, error out if error
      delete file;
      delete buffer;
      if (fail) {
        SEMMessageBox(errStr);
        return 1;
      }

    } else {

      // regular include of other script
      includeInd = FindCalledMacro(name.Mid(1), true);
      if (includeInd < 0)
        return 1;
    }

    // Finish the previous block, set up new block for include and add it in
    mFirstRealLineInPart.push_back(firstRealLine);
    mMacNumAtScrpLine.push_back(includeInd);
    mMacStartLineInScrp.push_back((int)mLineInSrcMacro.size());
    mIncludedFiles.push_back(includeInd < 0 ? (LPCTSTR)fileName : "");
    IndentAndAppendToScript(includeInd < 0 ? fileStr : mMacros[includeInd], 
      mMacroForScrpLang, indentStr, isPython);

    // Start new block of main script
    mMacNumAtScrpLine.push_back(macNum);
    mMacStartLineInScrp.push_back((int)mLineInSrcMacro.size());
    mIncludedFiles.push_back("");
    firstRealLine = -1;
  }
  mFirstRealLineInPart.push_back(firstRealLine);

  // Catch exceptions at end
  if (isPython) {
    mMacroForScrpLang +=
      "\r\nexcept SystemExit:\r\n"
      "  import serialem\r\n"
      "  serialem.Exit(1)\r\n"
      "except SEMerror:\r\n"
      "  pass\r\n"
      "except SEMexited:\r\n"
      "  pass\r\n"
      "except Exception:\r\n"
      "  import serialem, traceback\r\n"
      "  line = \"\\r\\nError in Python script:\\r\\n\"\r\n"
      "  lines = traceback.format_exc().splitlines()\r\n"
      "  for ind in range(len(lines)):\r\n"
      "    line += lines[ind] + \"\\r\\n\"\r\n"
      "  serialem.Exit(-1, line)\r\n";

  }
  if (GetDebugOutput(']'))
    mWinApp->AppendToLog(mMacroForScrpLang);
  return -1;
}

// Copy lines from source to copy, indenting by indentStr and keeping track of first real
// line
void CMacroProcessor::IndentAndAppendToScript(CString &source, CString &copy, 
  CString &indentStr, bool isPython)
{
  int currentInd = 0, lineNum = 0, length = source.GetLength();
  CString line, trim;
  int firstRealLine = -1;
  while (currentInd < length) {
    mIndexOfSrcLine.push_back(currentInd);
    mLineInSrcMacro.push_back(lineNum++);
    GetNextLine(&source, currentInd, line, true);
    if (isPython)
      DoReplacementsInPythonLine(line);
    copy += indentStr + line;
    if (firstRealLine < 0) {
      line.Trim(" \r\n");
      if (!line.IsEmpty() && line.GetAt(0) != '#')
        firstRealLine = (int)mLineInSrcMacro.size() - 1;
    }
  }
  mFirstRealLineInPart.push_back(firstRealLine);
}

void CMacroProcessor::DoReplacementsInPythonLine(CString & line)
{
  int index = 0;

  // Escape the backslashes to prevent them from being interpreted as escape code
  // but restore intended escape codes prefixed by |
  // For convenience, preserve clear cases of \n and \r\n
  if (line.Find("\\n") >= 0) {
    if (line.Find("EchoBreakLine(") >= 0)
      line.Replace("\\n", "|\\n");
    else
      line.Replace("'\\n'", "'|\\n'");
    line.Replace("'\\r\\n'", "'|\\r|\\n'");
    line.Replace("\"\\n\"", "\"|\\n\"");
    line.Replace("\"\\r\\n\"", "\"|\\r|\\n\"");
  }
  if (line.Replace("\\", "\\\\"))
    line.Replace("|\\\\", "\\");

  // Be nice and replace print with SEMprint
  while ((index = line.Find("print(", index)) >= 0) {
    if (!index || line.GetAt(index - 1) == ' ' || line.GetAt(index - 1) == '\r' ||
      line.GetAt(index - 1) == '\n')
      line.Insert(index, "SEM");
    index += 5;
  }
}

// Test whether an embedded script can be run by looking for the End line, and return the index of that line
// plus the index past that line
bool CMacroProcessor::IsEmbeddedPythonOK(int macNum, int currentInd, int &lastInd, int &newCurrentInd)
{
  CString line, strItems[2];
  int cmdIndex, length = mMacros[macNum].GetLength();
  lastInd = newCurrentInd = currentInd;
  while (newCurrentInd < length) {
    GetNextLine(&mMacros[macNum], newCurrentInd, line);
    mParamIO->ParseString(line, &strItems[0], 2);
    strItems[0].MakeUpper();
    cmdIndex = LookupCommandIndex(strItems[0]);
    if (cmdIndex == CME_ENDPYTHONSCRIPT)
      return true;
    lastInd = newCurrentInd;
  }
  return false;
}

// Return the number of lines through the current index 
int CMacroProcessor::CountLinesToCurIndex(int macNum, int curIndex)
{
  int currentInd = 0, lineNum = 0;
  while (currentInd >= 0 && currentInd < curIndex) {
    currentInd = mMacros[macNum].Find("\r\n", currentInd) + 1;
    lineNum++;
  }
  return lineNum;
}

// Take a Python exception from the plugin and translate the line numbers to be correct,
// provide script names, and print the line in question if necessary
void CMacroProcessor::EnhancedExceptionToLog(CString &str)
{
  CString line, numStr, name;
  int indLine, indComma, lineNum, ind, lineInSrc, indInSrc, macNum = -1, indMod;
  int currentInd = 0, length = str.GetLength();
  CString attribErr;

  if (length && mScrpLangData.exitStatus < 0) {
    mWinApp->AppendToLog("Error trying to run Python to execute script:\r\n   " + str);
    return;
  }

  // Loop on error message
  while (currentInd < length) {
    GetNextLine(&str, currentInd, line);
    line.TrimRight(" \r\n");
    ind = line.Find("module 'serialem' has no attribute");
    if (ind > 0) {
      ind = line.Find("'", ind + 20);
      if (ind > 0) {
        indLine = line.ReverseFind('\'');
        name = line.Mid(ind + 1, indLine - 1 - ind);
        name.MakeUpper();
        if (LookupCommandIndex(name) == CME_NOTFOUND)
          attribErr = "That is not a recognized SerialEM script command";
        else
          attribErr = "You need to upgrade your SerialEM module with the latest"
          " PythonModules package";
      }
    }
    indLine = line.Find("line ");
    if (indLine < 0) {
      mWinApp->AppendToLog(line);
      continue;
    }

    // Syntax errors have no comma and do not need the text line added
    // Errors withount <string> belong to other modules
    indComma = line.Find(",", indLine);
    if (indComma > 0 && line.Find("File \"<stdin>") < 0) {
      mWinApp->AppendToLog(line);
      continue;
    }

    // Get the line number
    if (indComma < 0)
      numStr = line.Mid(indLine + 5);
    else
      numStr = line.Mid(indLine + 5, indComma - (indLine + 5));
    lineNum = atoi((LPCTSTR)numStr) - 1;

    // Print and skip if not legal
    if (lineNum >= (int)mLineInSrcMacro.size() || lineNum < mMacStartLineInScrp[0]) {
      mWinApp->AppendToLog(line);
      continue;
    }

    // Search the vectors for which script block this occurs in and 
    for (ind = (int)mMacStartLineInScrp.size() - 1; ind >= 0; ind--) {
      if (lineNum >= mMacStartLineInScrp[ind]) {
        lineInSrc = mLineInSrcMacro[lineNum];
        indInSrc = mIndexOfSrcLine[lineNum];
        macNum = mMacNumAtScrpLine[ind];
        if (macNum < 0) {
          name = mIncludedFiles[ind].c_str();
        } else {
          name = mMacNames[macNum];
          if (name.IsEmpty())
            name.Format("Script #%d", macNum + 1);
        }
        numStr.Format("line %d, in %s", lineInSrc + 1, (LPCTSTR)name);
        mLastPythonErrorLine = lineInSrc + 1;

        // If it has function instead of <module>, add it after the script name
        indMod = line.Find("<module>", indLine);
        if (indComma > 0 && indMod < 0) {
          indMod = line.Find("in ", indLine + 6);
          if (indMod > 0)
            numStr += " - " + line.Mid(indMod + 3);
        }

        // Equivocate about errors right before or after an include
        line = line.Left(indLine) + numStr;
        if (indComma < 0 && ind > 0 && lineNum == mFirstRealLineInPart[ind]) {
          if (mMacNumAtScrpLine[ind - 1] == mMacNumAtScrpLine[0])
            line += "   (or maybe just before #include " + name + ")";
          else
            line += "   (or maybe right at the end of " + 
            (mMacNumAtScrpLine[ind - 1] < 0 ? CString(mIncludedFiles[ind - 1].c_str()) : 
              mMacNames[mMacNumAtScrpLine[ind - 1]]) + ")";
        }
        mWinApp->AppendToLog(line);

        // Output line itself
        if (indComma >= 0 && macNum >= 0) {
          GetNextLine(&mMacros[macNum], indInSrc, line);
          line.TrimRight(" \r\n");
          mWinApp->AppendToLog(line);
        }
        break;
      }
    }
  }

  if (!attribErr.IsEmpty())
    mWinApp->AppendToLog(attribErr);

  // Try to show error
  if (macNum >= 0 && mLastPythonErrorLine > 0 && mMacroEditer[macNum]) 
    mMacroEditer[macNum]->SelectAndShowLine(mLastPythonErrorLine);
}

void CMacroProcessor::SetPathToPython(CString & version, CString & path)
{
  mVersionsOfPython.push_back((LPCTSTR)version);
  mPathsToPython.push_back((LPCTSTR)path);
}

// Send an email from a message box error if command defined the text
void CMacroProcessor::SendEmailIfNeeded(void)
{
  if (!mEmailOnError.IsEmpty())
    mWinApp->mMailer->SendMail(mMailSubject, mEmailOnError);
}

// Thread Procedure for running a process
UINT CMacroProcessor::RunInShellProc(LPVOID pParam)
{
  CString *strCopy = (CString *)pParam;
  std::string command = (LPCTSTR)(*strCopy);
  _flushall();
  sProcessErrno = 0;
  sProcessExitStatus = system(command.c_str());
  if (sProcessExitStatus == -1) {
    switch (errno) {
    case E2BIG: sProcessErrno = 1; break;
    case ENOENT: sProcessErrno = 2; break;
    case ENOEXEC: sProcessErrno = 3; break;
    case ENOMEM: sProcessErrno = 4; break;
    default: sProcessErrno = 5;
    }
    return 1;
  }
  return 0;
}

// Thread procedure for starting a script in external language
UINT CMacroProcessor::RunScriptLangProc(LPVOID pParam)
{
  SECURITY_ATTRIBUTES saAttr;
  HANDLE hChildStd_IN_Rd = NULL;
  HANDLE hChildStd_IN_Wr = NULL;
  HANDLE hChildStd_OUT_Rd = NULL;
  HANDLE hChildStd_OUT_Wr = NULL;
  HANDLE hChildStd_ERR_Rd = NULL;
  HANDLE hChildStd_ERR_Wr = NULL;
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;
  BOOL bSuccess = FALSE;
  char *cmdLine = NULL;
  const char *script = (const char *)pParam;
  DWORD dwWrite, dwWritten, exitCode, dwRead;
  const int bufLen = 256;
  char buffer[bufLen];

  if (mScrpLangData.strItems[0].IsEmpty()) {
    mScrpLangData.exitStatus = mScrpLangFuncs->RunScript(script);
  } else {
    mScrpLangData.exitStatus = -1;
    mScrpLangData.gotExceptionText = true;

    // This is based heavily on a Microsoft post

    // Set the bInheritHandle flag so pipe handles are inherited.
    // This inherits all inheritable handles;  If you need to explicitly list ones to
    // inherit in CreateProcess (Vista onwards), see
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (CreateOnePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, false, 
      "stdout output"))
      return 1;

    // Create a pipe for the child process's STDERR.
    if (CreateOnePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, false, 
      "stderr output"))
      return 1;

    // Create a pipe for the child process's STDIN.
    if (CreateOnePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, true, "input"))
      return 1;

    // Set up members of the PROCESS_INFORMATION structure.
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure.
    // This structure specifies the STDIN and STDOUT handles for redirection.
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStd_ERR_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.hStdInput = hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    cmdLine = _strdup((LPCTSTR)mScrpLangData.strItems[0]);
    if (!cmdLine) {
      mScrpLangData.strItems[0] = "Failed to duplicate the command line for running"
        " Python";
      return 1;
    }

    // Create the child process.
    bSuccess = CreateProcessA(NULL,
      cmdLine,       // command line
      &saAttr,          // process security attributes
      &saAttr,          // primary thread security attributes
      TRUE,          // handles are inherited
      CREATE_NO_WINDOW,             // creation flags
      NULL,          // use parent's environment
      NULL,          // use parent's current directory
      &siStartInfo,  // STARTUPINFO pointer
      &piProcInfo);  // receives PROCESS_INFORMATION

    // Close handles to the stdin and stdout pipes no longer needed by the child process
    // If they are not explicitly closed, there is no way to recognize that the child 
    // process has ended.
    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_ERR_Wr);
    CloseHandle(hChildStd_IN_Rd);

    if (!bSuccess) {
      mScrpLangData.strItems[0] = "Error starting Python with this command: ";
      mScrpLangData.strItems[0] += cmdLine;
      CloseHandle(hChildStd_IN_Wr);
      CloseHandle(hChildStd_ERR_Rd);
      CloseHandle(hChildStd_OUT_Rd);
      free(cmdLine);
      return 1;
    }
    free(cmdLine);
    mScrpLangData.exitStatus = 0;

    // Close handle to the child process primary thread, keep the other handle
    CloseHandle(piProcInfo.hThread);
    mPyProcessHandle = piProcInfo.hProcess;

     // People are getting error 5, access denied.  Tried: CREATE_SUSPENDED: it hung
    //        BREAKAWAY_FROM_JOB  it couldn't run python
    //        &saAttr instead of NULL for the security attributes: no difference
    //
    // Assign the process to the job object so that if SerialEM dies, it will be killed
    if (CPythonServer::mJobObject) {
      if (!AssignProcessToJobObject(CPythonServer::mJobObject, mPyProcessHandle))
        SEMTrace('0', "WARNING: Error %d occurred assigning python process to job object", 
          GetLastError());
    }

    dwWrite = (DWORD)strlen(script);
    bSuccess = WriteFile(hChildStd_IN_Wr, script, dwWrite, &dwWritten, NULL);

    // Close the pipe handle so the child process stops reading.
    CloseHandle(hChildStd_IN_Wr);

    // Give up if there was an error writing to pipe
    if (!bSuccess) {
      TerminateScrpLangProcess();
      mScrpLangData.strItems[0] = "An error occurred writing the script to the pipe into"
        " Python";
      mScrpLangData.exitStatus = -1;
      return 1;
    }

    // Start thread to read from stdout and send to log
    AfxBeginThread(StdoutToLogProc, (LPVOID)(&hChildStd_OUT_Rd), THREAD_PRIORITY_NORMAL);

    // Read stderr from pipe
    mScrpLangData.strItems[0] = "";
    for (;;) {
      bSuccess = ReadFile(hChildStd_ERR_Rd, buffer, bufLen - 1, &dwRead, NULL);
      if (!bSuccess || dwRead == 0)
        break;
      buffer[dwRead] = 0x00;
      mScrpLangData.strItems[0] += buffer;
    }
    CloseHandle(hChildStd_ERR_Rd);

    // Waiting now may be gratuitous...
    WaitForSingleObject(mPyProcessHandle, INFINITE);

    // Get the exit status and close handles
    GetExitCodeProcess(mPyProcessHandle, &exitCode);
    CloseHandle(mPyProcessHandle);
    mPyProcessHandle = NULL;
    mScrpLangData.exitStatus = (int)exitCode;
    mScrpLangData.gotExceptionText = !mScrpLangData.strItems[0].IsEmpty();
  }
  SEMTrace('[', "RunScriptLangProc exit stat %d", mScrpLangData.exitStatus);
  return mScrpLangData.exitStatus ? 1 : 0;
}

// Function for the boilerplate in creating a pipe
int CMacroProcessor::CreateOnePipe(HANDLE * childRd, HANDLE * childWr, 
  SECURITY_ATTRIBUTES * saAttr, bool setForWrite, const char *descrip)
{
  // Create a pipe for the child process's STDOUT.
  if (!CreatePipe(childRd, childWr, saAttr, 0)) {
    mScrpLangData.strItems[0].Format("Error creating %s pipe for Python process", 
      descrip);
    return 1;
  }

  // Ensure the read handle to the pipe for STDOUT is not inherited.
  if (!SetHandleInformation(setForWrite ? *childWr : *childRd, HANDLE_FLAG_INHERIT, 0)) {
    mScrpLangData.strItems[0].Format("Error in SetHandleInformation for %s pipe of Python"
      " process", descrip);
    return 1;
  }
  return 0;
}

// Thread procedure to pump the standard out into the log
UINT CMacroProcessor::StdoutToLogProc(LPVOID pParam)
{
  const int bufLen = 1024;
  char buffer[bufLen];
  BOOL bSuccess;
  DWORD dwRead;
  HANDLE *outRd = (HANDLE *)pParam;

  for (;;) {
    bSuccess = ReadFile(*outRd, buffer, bufLen - 1, &dwRead, NULL);
    if (!bSuccess || dwRead == 0)
      break;
    buffer[B3DMIN(bufLen - 1, dwRead)] = 0x00;
    SEMTrace('0', buffer);
  }
  CloseHandle(*outRd);
  return 0;
}

// Common call to terminate the process and to let the thread exit before closing handle
void CMacroProcessor::TerminateScrpLangProcess(void)
{
  if (mPyProcessHandle) {
    TerminateProcess(mPyProcessHandle, 1);
    for (int ind = 0; ind < 100; ind++) {
      if (!mScrpLangThread || UtilThreadBusy(&mScrpLangThread) <= 0)
        break;
      Sleep(10);
    }
    if (mPyProcessHandle)
      CloseHandle(mPyProcessHandle);
    mPyProcessHandle = NULL;
  }
}

// Busy function for the task to start the Navigator Acquire
int CMacroProcessor::StartNavAvqBusy(void)
{
  if (DoingMacro())
    return 1;
  if (mLastCompleted && mWinApp->mNavigator) {
    mWinApp->mNavigator->SetCurAcqParmActions(mUseTempNavParams ? 2 :
      mNavHelper->GetCurAcqParamIndex());
    mWinApp->mNavigator->AcquireAreas(NAVACQ_SRC_MACRO, false, mUseTempNavParams);
  }
  return 0;
}

// Create a simple hash value with the djb2 alogorithm, mask bit before the multiply
unsigned int CMacroProcessor::StringHashValue(const char *str)
{
  unsigned int hash = 5381;
  int c;
  while ((c = (unsigned int)(*str++)) != 0)
    hash = (hash & 0x3FFFFFF) * 33 + c;
  return hash;
}

// Lookup a command or other candidate for command by first trying to find the hash value
// in the map, then falling back to matching the string
// The lookup has been tested with hash values masked to 0 - 63
int CMacroProcessor::LookupCommandIndex(CString & item)
{
  unsigned int hash = StringHashValue((LPCTSTR)item);
  int ind;
  std::map<unsigned int, int>::iterator mapit;

  if (!mCmdHashMap.count(hash))
    return CME_NOTFOUND;
  mapit = mCmdHashMap.find(hash);
  ind = mapit->second;

  // Return the map value as long as the string matches: we need to exclude the universe
  // of variable names that might collide with a command having unique hash
  if (ind && item == mCmdList[ind].cmd.c_str())
    return ind;

  // Fallback to looking up command by string
  for (ind = 0; ind < mNumCommands - 1; ind++) {
    if (item == mCmdList[ind].cmd.c_str())
      return ind;
  }
  return CME_NOTFOUND;
}

// Find a text file given the ID, performing a check on its existent specified by 
// checkType and returning its index in the array
FileForText *CMacroProcessor::LookupFileForText(CString &ID, int checkType, 
  CString &strLine, int &index)
{
  FileForText *tfile;
  CString errStr;
  for (index = 0; index < (int)mTextFileArray.GetSize(); index++) {
    tfile = mTextFileArray.GetAt(index);
    if (tfile->ID == ID) {
      if (checkType == TXFILE_MUST_NOT_EXIST)
        errStr = "There is already an open file with identifier " + ID;
      if (checkType == TXFILE_READ_ONLY && !tfile->readOnly)
        errStr = "The file with identifier " + ID + " cannot be read from";
      if (checkType == TXFILE_WRITE_ONLY && tfile->readOnly)
        errStr = "The file with identifier " + ID + " cannot be written to";
      if (!errStr.IsEmpty())
        break;
      return tfile;
    }
  }
  if (errStr.IsEmpty() && checkType != TXFILE_MUST_NOT_EXIST && 
    checkType != TXFILE_QUERY_ONLY)
      errStr = "There is no open file with identifier " + ID;
  if (!errStr.IsEmpty())
    ABORT_NORET_LINE(errStr + " for line:\n\n");
  return NULL;
}

// Close a text file given its index
void CMacroProcessor::CloseFileForText(int index)
{
  FileForText *txFile;
  int startInd = index, endInd = index, ind;
  if (index >= mTextFileArray.GetSize())
    return;
  if (index < 0) {
    startInd = 0;
    endInd = (int)mTextFileArray.GetSize() - 1;
  }
  for (ind = endInd; ind >= startInd; ind--) {
    txFile = mTextFileArray.GetAt(ind);
    if (index != -1 || !txFile->persistent) {
      txFile->csFile->Close();
      delete txFile->csFile;
      delete txFile;
      mTextFileArray.RemoveAt(ind);
    }
  }
}

// Read in the given file to the next temporary script position, optionally delete it,
// and run the script as a "call"
int CMacroProcessor::RunScriptFromFile(CString &filename, bool deleteFile, CString &mess)
{
  // Run a script: read it in
  int index2 = 0;
  int index = MAX_MACROS + MAX_ONE_LINE_SCRIPTS + mNumTempMacros++;
  CString line, action;
  CStdioFile *sFile = NULL;
  try {
    action = "opening file";
    sFile = new CStdioFile(filename, CFile::modeRead | CFile::shareDenyWrite |
      CFile::modeNoInherit);
    action = "reading file";
    mMacros[index] = "";
    while (sFile->ReadString(line)) {
      if (!mMacros[index].IsEmpty())
        mMacros[index] += "\r\n";
      mMacros[index] += line;
    }
  }
  catch (CFileException * perr) {
    index2 = perr->m_lOsError;
    perr->Delete();
  }
  delete sFile;

  // Delete the file
  if (deleteFile) {
    try {
      CFile::Remove(filename);
    }
    catch (CFileException * pEx) {
      if (!index2)
        action = "removing";
      index2 = pEx->m_lOsError;
      pEx->Delete();
    }
  }
  if (index2) {
    mess.Format("Error %d %s %s", index2, (LPCTSTR)action, (LPCTSTR)filename);
    return 1;
  }
  if (mMacros[index].IsEmpty())
    return -1;

  ScanForName(index, &mMacros[index]);

  index2 = CheckForScriptLanguage(index, false, 0);
  if (index2 > 0) {
    AbortMacro();
    mess = "";
    return 1;
  }
  if (index2 < 0) {
    mRunningScrpLang = true;
    mCalledFromSEMmacro = true;
    StartRunningScrpLang();
  }
  
  // Set it up like callScript
  mCallIndex[mCallLevel++] = mCurrentIndex;
  mCurrentMacro = index;
  mCallMacro[mCallLevel] = mCurrentMacro;
  mBlockDepths[mCallLevel] = -1;
  mCallFunction[mCallLevel] = NULL;
  mCurrentIndex = 0;
  mLastIndex = -1;
  mNeedClearTempMacro = mCurrentMacro;
  mClearTempAtCallLevel = mCallLevel;
  return 0;
}

// Extract part of entered line after substituting variables
// No longer a convenience!  You must use these instead of StripItems to get the final
// string argument for external scripting
void CMacroProcessor::SubstituteLineStripItems(CString & strLine, int numStrip, 
  CString &strCopy)
{
  if (!mRunningScrpLang)
    SubstituteVariables(&strLine, 1, strLine);
  JustStripItems(strLine, numStrip, strCopy);
}

// Or just extract part of entered line
void CMacroProcessor::JustStripItems(CString &strLine, int numStrip, CString &strCopy,
  bool allowComment)
{
  if (mRunningScrpLang)
    strCopy = mScrpLangData.strItems[mScrpLangData.lastNonEmptyInd];
  else
    mParamIO->StripItems(strLine, numStrip, strCopy, allowComment);
}

// Convert letter after command to LD area # or abort if not legal.  Tests for whether
// Low dose is on or off if needOnOrOff >0 or <0.  Returns index -1 and no error if item
// is empty
int CMacroProcessor::CheckAndConvertLDAreaLetter(CString &item, int needOnOrOff,
  int &index, CString &strLine)
{
  if (needOnOrOff && !BOOL_EQUIV(mWinApp->LowDoseMode(), needOnOrOff > 0)) {
    ABORT_NORET_LINE("You must" + CString(needOnOrOff > 0 ? "" : " NOT") + 
      " be in low dose mode to use this command:\n\n");
    return 1;
  }
  if (item.IsEmpty()) {
    index = -1;
    return 0;
  }
  index = CString("VFTRS").Find(item.Left(1));
  if (index < 0)
    ABORT_NORET_LINE("Command must be followed by one of V, F, T, R, or S in line:\n\n");
  return index < 0 ? 1 : 0;
}

// Restore saved low dose parameters for the set given by index, or for all sets if index
// is -1, or for ones marked to be restored if index is < -1
void CMacroProcessor::RestoreLowDoseParams(int index)
{
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  LowDoseParams ldsaParams;
  int ind, set;
  int curArea = mWinApp->LowDoseMode() ? mScope->GetLowDoseArea() : -1;
  for (ind = 0; ind < (int)mLDareasSaved.size(); ind++) {
    set = mLDareasSaved[ind];
    if (set == index || index == -1 || (index < -1 && mKeepOrRestoreArea[set] > 0)) {

      // When going to the current area, save the current params in the local variable
      // and tell LD that is the current set of parameters to use, then set in new
      // parameters and go to the area to get them handled right
      if (set == curArea) {
        ldsaParams = ldParam[set];
        mScope->SetLdsaParams(&ldsaParams);
      }
      ldParam[set] = mLDParamsSaved[ind];
      if (set == curArea)
        mScope->GotoLowDoseArea(curArea);
      if (index >= 0) {
        mLDareasSaved.erase(mLDareasSaved.begin() + ind);
        mLDParamsSaved.erase(mLDParamsSaved.begin() + ind);
        return;
      }
    }
  }
  if (index < 0) {
    mLDareasSaved.clear();
    mLDParamsSaved.clear();
  }
}

// restore multishot params if they were saved to do step & adjust IS
void CMacroProcessor::RestoreMultiShotParams()
{
  if (mSavedMultiShot) {
    MultiShotParams *params = mNavHelper->GetMultiShotParams();
    *params = *mSavedMultiShot;
    B3DDELETE(mSavedMultiShot);
  }
}

// External call to find out if a low dose area is saved/free to modify
bool CMacroProcessor::IsLowDoseAreaSaved(int which)
{
  return mKeepOrRestoreArea[which] != 0;
}

// Call to update the current Low dose area if it was saved
void CMacroProcessor::UpdateLDAreaIfSaved()
{
  BOOL saveContinuous = mWinApp->mLowDoseDlg.GetContinuousUpdate();
  int curArea = mWinApp->LowDoseMode() ? mScope->GetLowDoseArea() : -1;
  if (curArea < 0 || !mKeepOrRestoreArea[curArea])
    return;
  if (!saveContinuous)
    mWinApp->mLowDoseDlg.SetContinuousUpdate(true);
  mScope->ScopeUpdate(GetTickCount());
  if (!saveContinuous)
    mWinApp->mLowDoseDlg.SetContinuousUpdate(false);
}

// Store string variable into an extra script
int CMacroProcessor::MakeNewTempMacro(CString &strVar, CString &strIndex, bool tempOnly,
  CString &strLine)
{
  int ix0, index, endInd, lastEnd, nextEnd, length;
  int lowLim = tempOnly ? 0 : -MAX_MACROS;
  Variable *var = LookupVariable(strVar.MakeUpper(), ix0);
  if (!var) {
    ABORT_NORET_LINE("The variable " + strVar + " is not defined in line:\n\n");
    return 0;
  }
  if (var->rowsFor2d) {
    ABORT_NORET_LINE("The variable " + strVar + " is a 2D array and cannot be run "
      "as a script in line:\n\n");
    return 0;
  }
  index = mNumTempMacros;
  if (!strIndex.IsEmpty()) {
    index = atoi((LPCTSTR)strIndex);
    if (!index || index > MAX_TEMP_MACROS || index < lowLim) {
      ABORT_NORET_LINE("The specified extra script number is out of range in line:\n\n");
      return 0;
    }
    if (index < 0) {
      index = -index - 1;
    } else {
      index--;
      if (index > mNumTempMacros) {
        ABORT_NORET_LINE("The script number must specify the number of an existing "
          "\r\nextra script or the next unused number in line:\n\n");
        return 1;
      }
      mNumTempMacros = B3DMAX(mNumTempMacros, index + 1);
      index += MAX_MACROS + MAX_ONE_LINE_SCRIPTS;
    }
  } else {
    if (mNumTempMacros == MAX_TEMP_MACROS) {
      ABORT_NORET_LINE("Too many extra scripts are defined to use another in line:\n\n");
      return 0;
    }
    mNumTempMacros++;
    index += MAX_MACROS + MAX_ONE_LINE_SCRIPTS;
  }
  mMacros[index] = "";
  lastEnd = 0;
  length = var->value.GetLength();
  for (;;) {
    endInd = var->value.Find("\n", lastEnd);
    if (endInd < 0)
      endInd = length;
    nextEnd = endInd + 1;
    if (endInd > 0 && var->value.GetAt(endInd - 1) == '\r')
      endInd--;
    mMacros[index] += var->value.Mid(lastEnd, endInd - lastEnd) + "\r\n";
    lastEnd = nextEnd;
    if (lastEnd >= length)
      break;
  }

  ScanForName(index, &mMacros[index]);
  return index;
}

// Test whether a stage restore is specified in the first passed item, and use the current
// stage position if the next passed items do not have one
bool CMacroProcessor::SetupStageRestoreAfterTilt(CString *strItems, double &stageX, 
  double &stageY)
{
  double stageZ;
  stageX = stageY = EXTRA_NO_VALUE;
  if (strItems[0].IsEmpty() || !atoi((LPCTSTR)strItems[0])) {
    return false;
  } else if (strItems[1].IsEmpty() || strItems[2].IsEmpty()) {
    mScope->GetStagePosition(stageX, stageY, stageZ);
  } else {
    stageX = atof((LPCTSTR)strItems[1]);
    stageY = atof((LPCTSTR)strItems[2]);
  }
  return true;
}

// DO stage relaxation if backlash is valid.  Returns 1 for failure to get stage 
// position, 0 if a move was started, -1 if backlash not valid
int CMacroProcessor::DoStageRelaxation(double delX)
{
  float backlashX, backlashY;
  StageMoveInfo smi;
  if (!mScope->GetStagePosition(smi.x, smi.y, smi.z))
    return 1;
  if (mScope->GetValidXYbacklash(smi.x, smi.y, backlashX, backlashY)) {
    mScope->CopyBacklashValid();

    // Move in direction of the backlash, which is opposite to direction of stage move
    smi.x += (backlashX > 0. ? delX : -delX);
    smi.y += (backlashY > 0. ? delX : -delX);
    smi.z = 0.;
    smi.alpha = 0.;
    smi.axisBits = axisXY;
    mScope->MoveStage(smi);
    return 0;
  }
  return -1;
}

ScriptLangData *SEMGetScriptLangData() 
{
  return &CMacroProcessor::mScrpLangData; 
}
