// MacroProcessor.cpp:    Runs macros
//
// Copyright (C) 2003-2018 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
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
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "MacroControlDlg.h"
#include "NavigatorDlg.h"
#include "OneLineScript.h"
#include "Mailer.h"
#include "PiezoAndPPControl.h"
#include "Utilities\KGetOne.h"


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
  "\r\n\r\n") + (a) + errmess + ":\r\n\r\n" + strLine;          \
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
  ON_COMMAND_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN40, OnMacroRun)
  ON_UPDATE_COMMAND_UI_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN40, OnUpdateMacroRun)
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
    "FRACDIFF", "MIN", "MAX" };
  std::string keywords[] = { "REPEAT", "ENDLOOP", "DOMACRO", "LOOP", "CALLMACRO", "DOLOOP"
    , "ENDIF", "IF", "ELSE", "BREAK", "CONTINUE", "CALL", "EXIT", "RETURN", "KEYBREAK",
    "SKIPTO", "FUNCTION", "CALLFUNCTION", "ENDFUNCTION", "CALLSCRIPT", "DOSCRIPT",
    "TRY", "CATCH", "ENDTRY", "THROW"};
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mMacNames = mWinApp->GetMacroNames();
  mMagTab = mWinApp->GetMagTable();
  mImBufs = mWinApp->GetImBufs();
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
  mLoadingMap = false;
  mMakingDualMap = false;
  mLastCompleted = false;
  mLastAborted = false;
  mEnteredName = "";
  mToolPlacement.rcNormalPosition.right = 0;
  mNumToolButtons = 10;
  mToolButHeight = 0;
  mAutoIndentSize = 3;
  mRestoreMacroEditors = true;
  mOneLinePlacement.rcNormalPosition.right = 0;
  mMailSubject = "Message from SerialEM script";
  for (i = 0; i < MAX_MACROS; i++) {
    mStrNum[i].Format("%d", i + 1);
    mFuncArray[i].SetSize(0, 4);
    mEditerPlacement[i].rcNormalPosition.right = 0;
  }
  for (i = 0; i < MAX_TOT_MACROS; i++)
    mReadOnlyStart[i] = -1;
  srand(GetTickCount());
  mProcessThread = NULL;
}

CMacroProcessor::~CMacroProcessor()
{
  ClearVariables();
  ClearVariables(VARTYPE_PERSIST);
  ClearFunctionArray(-1);
  CloseFileForText(-2);
}

void CMacroProcessor::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  mNavHelper = mWinApp->mNavHelper;
  if (GetDebugOutput('%')) {
    mWinApp->AppendToLog("Commands allowing arithmetic in arguments:");
    for (int i = 0; i < mNumCommands - 1; i++)
      if (mCmdList[i].arithAllowed & 1)
        mWinApp->AppendToLog(mCmdList[i].mixedCase);
  }
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
  CString menuText;
  CString *longMacNames = mWinApp->GetLongMacroNames();
  pCmdUI->Enable();
  pCmdUI->SetCheck(mInitialVerbose ? 1 : 0);
  for (int ind = 0; ind < MAX_MACROS; ind++) {
    if (!longMacNames[ind].IsEmpty())
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)longMacNames[ind]);
    else if (mMacNames[ind].IsEmpty())
      menuText.Format("Run %d", ind + 1);
    else
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)mMacNames[ind]);
    UtilModifyMenuItem(5, ID_MACRO_RUN1 + ind, (LPCTSTR)menuText);
  }

}

// Read and write many macros
void CMacroProcessor::OnMacroReadMany()
{
  CString filename;
  if (mWinApp->mDocWnd->GetTextFileName(true, false, filename))
    return;
  mWinApp->mParamIO->ReadMacrosFromFile(filename, "", MAX_MACROS);
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
  oldFile = mWinApp->mDocWnd->GetCurScriptPackPath();

  // Find out if want to save
  if (!oldFile.IsEmpty()) {
    UtilSplitPath(oldFile, path, filen);
    if (AfxMessageBox("Save current scripts to current file before loading a new"
      " package file?", MB_QUESTION) == IDYES) {
      mWinApp->mDocWnd->ManageScriptPackBackup();
      mWinApp->mParamIO->WriteMacrosToFile(oldFile, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
    }
  }
  if (mWinApp->mDocWnd->GetTextFileName(true, false, filename, NULL, &path))
    return;
  mWinApp->ClearAllMacros();

  // Try to read, try to revert if fails, and if THAT fails, assign the default name
  if (mWinApp->mParamIO->ReadMacrosFromFile(filename, "",
    MAX_MACROS + MAX_ONE_LINE_SCRIPTS)) {
    mWinApp->ClearAllMacros();
    filename = oldFile;
    if (mWinApp->mParamIO->ReadMacrosFromFile(filename, "",
      MAX_MACROS + MAX_ONE_LINE_SCRIPTS)) {
      oldFile = mWinApp->mDocWnd->GetCurrentSettingsPath();
      UtilSplitExtension(oldFile, filename, filen);
      filename += "-scripts.txt";
      mWinApp->AppendToLog("Unable to reload last script package; scripts will be saved "
        "to\n" + filename + " unless you do \"Save Package As\" to a different name");
    }
  }
  mWinApp->mDocWnd->SetCurScriptPackPath(filename);
  mWinApp->mDocWnd->SetScriptPackBackedUp(false);
  UpdateAllForNewScripts(true);
  mWinApp->AppendToLog("Current script package file is now " + filename);
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
  CString filename = mWinApp->mDocWnd->GetCurScriptPackPath();
  if (filename.IsEmpty())
    OnScriptSavePackageAs();
  else
    mWinApp->mParamIO->WriteMacrosToFile(filename, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
}

// Save to new package file
void CMacroProcessor::OnScriptSavePackageAs()
{
  CString filename, path, filen, oldFile;
  oldFile = mWinApp->mDocWnd->GetCurScriptPackPath();
  if (!oldFile.IsEmpty())
    UtilSplitPath(oldFile, path, filen);
  if (mWinApp->mDocWnd->GetTextFileName(false, false, filename, NULL, &path))
    return;
  mWinApp->mParamIO->WriteMacrosToFile(filename, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
  mWinApp->mDocWnd->SetCurScriptPackPath(filename);
  mWinApp->mDocWnd->SetScriptPackBackedUp(false);
  mWinApp->AppendToLog("Current script package file is now " + filename);
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
  }
  mOneLineScript->mMacros = mMacros;
  mOneLineScript->Create(IDD_ONELINESCRIPT);
  if (mOneLinePlacement.rcNormalPosition.right > 0)
    mOneLineScript->SetWindowPlacement(&mOneLinePlacement);
  mOneLineScript->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

void CMacroProcessor::OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI)
{
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
    if (fromDialog)
      mMacros[MAX_MACROS + ind] = mOneLineScript->m_strOneLine[ind];
    else
      mOneLineScript->m_strOneLine[ind] = mMacros[MAX_MACROS + ind];
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
    !mWinApp->NavigatorStartedTS() && (index >= MAX_MACROS || !mMacros[index].IsEmpty() || 
    mMacroEditer[index]);
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

// TASK HANDLERS AND ANCILLARY FUNCTIONS

// Check for conditions that macro may have started
int CMacroProcessor::TaskBusy()
{
  double diff, dose;
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

  return ((mScope->GetInitialized() && ((mMovedStage &&
    mScope->StageBusy() > 0) || (mExposedFilm && mScope->FilmBusy() > 0) ||
    (mMovedScreen && mScope->ScreenBusy()))) ||
    (mStartedLongOp && mScope->LongOperationBusy()) ||
    (mMovedPiezo && mWinApp->mPiezoControl->PiezoBusy() > 0) ||
    (mMovedAperture && mScope->GetMovingAperture()) ||
    (mLoadingMap && mWinApp->mNavigator && mWinApp->mNavigator->GetLoadingMap()) ||
    (mMakingDualMap && mNavHelper->GetAcquiringDual()) ||
    mWinApp->mShiftCalibrator->CalibratingIS() ||
    (mCamera->GetInitialized() && mCamera->CameraBusy() && 
    (mCamera->GetTaskWaitingForFrame() || 
    !(mUsingContinuous && mCamera->DoingContinuousAcquire()))) ||
    mWinApp->mMontageController->DoingMontage() || 
    mWinApp->mParticleTasks->DoingMultiShot() || 
    mWinApp->mParticleTasks->GetWaitingForDrift() ||
    mWinApp->mFocusManager->DoingFocus() || mWinApp->mAutoTuning->DoingAutoTune() ||
    mShiftManager->ResettingIS() || mWinApp->mCalibTiming->Calibrating() ||
    mWinApp->mFilterTasks->RefiningZLP() || 
    mNavHelper->mHoleFinderDlg->GetFindingHoles() || mNavHelper->GetRealigning() ||
    mWinApp->mComplexTasks->DoingTasks() || mWinApp->DoingRegisteredPlugCall()) ? 1 : 0;
}

// To run a macro: set the current macro with index at start
void CMacroProcessor::Run(int which)
{
  int mac, ind, tryLevel = 0;
  MacroFunction *func;
  if (mMacros[which].IsEmpty())
    return;

  // Check the macro(s) for commands, number of arguments, etc.
  PrepareForMacroChecking(which);
  mLastAborted = true;
  mLastCompleted = false;
  if (CheckBlockNesting(which, -1, tryLevel))
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
  mOnStopMacroIndex = -1;
  mExitAtFuncEnd = false;
  mLoopIndsAreLocal = false;
  mStartNavAcqAtEnd = false;
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
  mNeedClearTempMacro = -1;
  mBoxOnScopeText = "SerialEM message";
  mBoxOnScopeType = 0;
  mBoxOnScopeInterval = 0.;
  mNumRuns = 0;
  mTestNextMultiShot = 0;
  mAccumShiftX = 0.;
  mAccumShiftY = 0.;
  mAccumDiff = 0.;
  mMinDeltaFocus = 0;
  mMaxDeltaFocus = 0.;
  mMinAbsFocus = 0.;
  mMaxAbsFocus = 0.;
  mRunToolArgPlacement = 0;
  mNumTempMacros = 0;
  mParseQuotes = false;
  mLogAction = LOG_OPEN_IF_CLOSED;
  mLogErrAction = LOG_IGNORE;
  mStartClock = GetTickCount();
  mOverwriteOK = false;
  mVerbose = mInitialVerbose ? 1 : 0;
  CloseFileForText(-1);
  RunOrResume();
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
  mCamera->SetTaskWaitingForFrame(false);
  mFrameWaitStart = -1.;
  mNumStatesToRestore = 0;
  mFocusToRestore = -999.;
  mFocusOffsetToRestore = -9999.;
  mDEframeRateToRestore = -1.;
  mDEcamIndToRestore = -1;
  mLDSetAddedBeamRestore = -1;
  mK3CDSmodeToRestore = -1;
  mRestoreConsetAfterShot = false;
  mSuspendNavRedraw = false;
  mDeferLogUpdates = false;
  mCropXafterShot = -1;
  mNextProcessArgs = "";
  mBeamTiltXtoRestore[0] = mBeamTiltXtoRestore[1] = EXTRA_NO_VALUE;
  mBeamTiltYtoRestore[0] = mBeamTiltYtoRestore[1] = EXTRA_NO_VALUE;
  mAstigXtoRestore[0] = mAstigXtoRestore[1] = EXTRA_NO_VALUE;
  mAstigYtoRestore[0] = mAstigYtoRestore[1] = EXTRA_NO_VALUE;
  mCompensatedBTforIS = false;
  mKeyPressed = 0;
  if (mChangedConsets.size() > 0 && mCamWithChangedSets == mWinApp->GetCurrentCamera())
    for (ind = 0; ind < (int)B3DMIN(mConsetNums.size(), mChangedConsets.size());ind++)
      mConSets[mConsetNums[ind]] = mChangedConsets[ind];
  for (ind = 0 ; ind < (int)mSavedSettingNames.size(); ind++)
    mWinApp->mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()), 
      mNewSettingValues[ind]);
  mStoppedContSetNum = mCamera->DoingContinuousAcquire() - 1;
  mWinApp->UpdateBufferWindows();
  SetComplexPane();
  if (mStoppedContSetNum >= 0) {
    mCamera->StopCapture(0);
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return;
  }
  mWinApp->mMacroProcessor->NextCommand(true);
}

// Set the macro name or message in complex pane
void CMacroProcessor::SetComplexPane(void)
{
  if (mMacNames[mCurrentMacro].IsEmpty())
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
    if (TestAndStartFuncOnStop())
      return;
    if (mProcessThread && UtilThreadBusy(&mProcessThread) > 0)
      UtilThreadCleanup(&mProcessThread);
    if (mDoingMacro && mLastIndex >= 0)
      mAskRedoOnResume = true;
    SuspendMacro();
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
    mWinApp->AppendToLog(str);
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return 1;
  }
  mTryCatchLevel = 0;
  return 0;
}

///////////////////////////////////////////////////////

void CMacroProcessor::AbortMacro()
{
  SuspendMacro(true);
}

void CMacroProcessor::SuspendMacro(BOOL abort)
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  int probe, ind;
  bool restoreArea = false;
  mLoopInOnIdle = false;
  if (!mDoingMacro)
    return;
  if (!mWinApp->mCameraMacroTools.GetUserStop() && TestTryLevelAndSkip(NULL))
    return;
  if (TestAndStartFuncOnStop())
    return;
  if (mNeedClearTempMacro >= 0) {
    mMacros[mNeedClearTempMacro] = "";
    abort = true;
  }

  // restore user settings
  for (ind = 0; ind < (int)mSavedSettingNames.size(); ind++)
    mWinApp->mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()), 
      mSavedSettingValues[ind]);
  if (mSavedSettingNames.size())
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

  // Restore other things and make it non-resumable as they have no mechanism to resume
  if (abort || mNumStatesToRestore > 0 || restoreArea) {
    mCurrentMacro = -1;
    mLastAborted = !mLastCompleted;
    if (mNumStatesToRestore) {
      if (mFocusToRestore > -2.)
        mScope->SetFocus(mFocusToRestore);
      if (mFocusOffsetToRestore > -9000.)
        mWinApp->mFocusManager->SetDefocusOffset(mFocusOffsetToRestore);
      if (mDEframeRateToRestore > 0 || mDEcamIndToRestore >= 0) {
        camParams[mDEcamIndToRestore].DE_FramesPerSec = mDEframeRateToRestore;
        mWinApp->mDEToolDlg.UpdateSettings();
      }
      probe = mScope->GetProbeMode();
      if (mBeamTiltXtoRestore[probe] > EXTRA_VALUE_TEST) {
        mScope->SetBeamTilt(mBeamTiltXtoRestore[probe], mBeamTiltYtoRestore[probe]);
        if (mWinApp->mFocusManager->DoingFocus())
          mWinApp->mFocusManager->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
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
    }
    mSavedSettingNames.clear();
    mSavedSettingValues.clear();
    mNewSettingValues.clear();
    if (restoreArea)
      RestoreLowDoseParams(-2);
  }

  // restore camera sets, clear if non-resumable
  RestoreCameraSet(-1, mCurrentMacro < 0);
  if (mCurrentMacro < 0 && mUsingContinuous)
    mCamera->ChangePreventToggle(-1);
  if (mCurrentMacro < 0)
    CloseFileForText(-1);
  mDoingMacro = false;
  if (mStoppedContSetNum >= 0)
    mCamera->InitiateCapture(mStoppedContSetNum);
  if (abort) {
    for (ind = MAX_MACROS + MAX_ONE_LINE_SCRIPTS; ind < MAX_TOT_MACROS; ind++)
      ClearFunctionArray(ind);
  }
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(mWinApp->DoingTiltSeries() &&
    mWinApp->mTSController->GetRunningMacro() ? MEDIUM_PANE : COMPLEX_PANE, 
    (IsResumable() && mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()) ?
    "STOPPED NAV SCRIPT" : "");
  if (!mCamera->CameraBusy())
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  mWinApp->mScopeStatus.SetWatchDose(false);
  mWinApp->mDocWnd->SetDeferWritingFrameMdoc(false);
}


void CMacroProcessor::SetIntensityFactor(int iDir)
{
  double angle = mScope->GetTiltAngle();
  double increment = iDir * mScope->GetIncrement();
  mIntensityFactor = cos(angle * DTOR) / cos((angle + increment) * DTOR);
}

// Get the next line or multiple lines if they end with backslash
void CMacroProcessor::GetNextLine(CString * macro, int & currentIndex, CString &strLine)
{
  int index, testInd;
  strLine = "";
  CString temp;
  for (;;) {

    // Find next linefeed
    index = macro->Find('\n', currentIndex);
    if (index < 0) {

      // If none, get rest of string, set index past end
      temp = macro->Mid(currentIndex);
      if (!temp.TrimLeft().GetLength() || temp.TrimLeft().GetAt(0) != '#')
        strLine += temp;
      currentIndex = macro->GetLength();
      break;
    } else {

      // Set index past end of line then test for space backslash after skipping a \r
      index++;
      temp = macro->Mid(currentIndex, index - currentIndex);
      if (temp.TrimLeft().GetLength() && temp.TrimLeft().GetAt(0) == '#') {
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
  CString strLine, argName, strItem[MAX_MACRO_TOKENS];
  CString *longMacNames = mWinApp->GetLongMacroNames();
  MacroFunction *funcP;
  int currentIndex = 0, lastIndex = 0;
  if ((macroNumber >= MAX_MACROS && macroNumber < MAX_MACROS + MAX_ONE_LINE_SCRIPTS) ||
    (mDoingMacro && mCallLevel > 0))
      return 0;
  if (!macro)
    macro = &mMacros[macroNumber];
  mReadOnlyStart[macroNumber] = -1;
  ClearFunctionArray(macroNumber);
  while (currentIndex < macro->GetLength()) {
    GetNextLine(macro, currentIndex, strLine);
    if (!strLine.IsEmpty()) {
      mWinApp->mParamIO->ParseString(strLine, strItem, MAX_MACRO_TOKENS);
      if ((strItem[0].CompareNoCase("MacroName") == 0 ||
        strItem[0].CompareNoCase("ScriptName") == 0) && !strItem[1].IsEmpty()) {
        mWinApp->mParamIO->StripItems(strLine, 1, newName);
      } else if (strItem[0].CompareNoCase("LongName") == 0 && !strItem[1].IsEmpty()) {
        mWinApp->mParamIO->StripItems(strLine, 1, longName);

      // Put all the functions in there that won't be eliminated by minimum argument
      // requirement and let pre-checking complain about details
      } else if (strItem[0].CompareNoCase("ReadOnlyUnlessAdmin") == 0) {
        mReadOnlyStart[macroNumber] = lastIndex;
      } else if (strItem[0].CompareNoCase("Function") == 0 && !strItem[1].IsEmpty()) {
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
int CMacroProcessor::FindCalledMacro(CString strLine, bool scanning)
{
  CString strCopy;
  int index, index2;
  mWinApp->mParamIO->StripItems(strLine, 1, strCopy);

  // Look for the name
  index = -1;
  for (index2 = 0; index2 < MAX_TOT_MACROS; index2++) {
    if (index >= MAX_MACROS && index < MAX_MACROS + MAX_ONE_LINE_SCRIPTS)
      continue;
    ScanMacroIfNeeded(index2, scanning);
    if (strCopy == mMacNames[index2]) {
      if (index >= 0) {
        AfxMessageBox("Two scripts have a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
        return -1;
      }
      index = index2;
    }
  }
  if (index < 0)
    AfxMessageBox("No script has a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
  return index;
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
  mWinApp->mParamIO->ParseString(strLine, strItems, MAX_MACRO_TOKENS);

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

// Sets a variable of the given name to the value, with the type and index.
// The variable must not exist if mustBeNew is true; returns true for error and fills
// in errStr if it is non-null
bool CMacroProcessor::SetVariable(CString name, CString value, int type, 
  int index, bool mustBeNew, CString *errStr, CArray<ArrayRow, ArrayRow> *rowsFor2d)
{
  int ind, leftInd, rightInd, rightInd2, arrInd, rowInd, numElements = 1;
  int *oldNumElemPtr;
  CString *oldValuePtr;
  CString temp;
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

  // For each item, look for $ repeatedly
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
      newstr += value;
      nright = strItems[ind].GetLength() - (nameInd + maxlen);
      if (nright)
        newstr += strItems[ind].Right(nright);
      strItems[ind] = newstr;
    }
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
  int ind, loop;
  double left, right, result = 0.;
  CString str, *strp;
  std::string stdstr;
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
      if (ItemToDouble(strItems[ind + 1], line, left) ||
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
      ReplaceWithResult(result, strItems, ind, numItems, 2);
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

  if (SeparateParentheses(strItems, maxItems)) {
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
                                        int & numItems, int numDrop)
{
  strItems[index].Format("%f", result);
  UtilTrimTrailingZeros(strItems[index]);
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
  if (val1 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val1, 1);
  if (val2 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val2, 2);
  if (val3 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val3, 3);
  if (val4 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val4, 4);
  if (val5 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val5, 5);
  if (val6 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val6, 6);
}

// Sets one reported value with a numeric or string value, also sets an optional variable
// Pass the address of the FIRST optional variable
// Clears all report variables when index is 1 and then requires report variables to
// be new, so call with 1 first
void CMacroProcessor::SetOneReportedValue(CString *strItems, CString *valStr, 
  double value, int index)
{
  CString num, str;
  if (index < 1 || index > 6)
    return;
  num.Format("%d", index);
  if (valStr)
    str = *valStr;
  else {
    str.Format("%f", value);
    UtilTrimTrailingZeros(str);
  }
  if (index == 1)
    ClearVariables(VARTYPE_REPORT);
  SetVariable("REPORTEDVALUE" + num, str, VARTYPE_REPORT, index, true);
  SetVariable("REPVAL" + num, str, VARTYPE_REPORT, index, true);
  if (strItems && !strItems[index - 1].IsEmpty())
    SetVariable(strItems[index - 1], str, VARTYPE_REGULAR, -1, false);
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
  MacroFunction *func;
  int subBlockNum[MAX_LOOP_DEPTH + 1];
  CString *macro = &mMacros[macroNum];
  int numLabels = 0, numSkips = 0;
  int i, inloop, needVers, index, skipInd, labInd, length, cmdIndex, currentIndex = 0;
  int currentVersion = 202;
  CString strLine, strItems[MAX_MACRO_TOKENS], errmess, intCheck;
  const char *features[] = {"variable1", "arrays", "keepcase", "zeroloop", "evalargs"};
  int numFeatures = sizeof(features) / sizeof(char *);

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
    if (mWinApp->mParamIO->ParseString(strLine, strItems, MAX_MACRO_TOKENS))
      FAIL_CHECK_LINE("Too many items on line");
    if (strItems[0].Find("/*") == 0) {
      inComment = true;
      continue;
    }

    if (!strItems[0].IsEmpty()) {
      strItems[0].MakeUpper();
      InsertDomacro(&strItems[0]);
      cmdIndex = LookupCommandIndex(strItems[0]);

      // Check validity of variable assignment then skip further checks
      stringAssign = strItems[1] == "@=" || strItems[1] == ":@=";
      if (strItems[1] == "=" || strItems[1] == ":=" || stringAssign) {
        index = 2;
        if (!stringAssign) {
          i = CheckForArrayAssignment(strItems, index);
          if (i < 0)
            FAIL_CHECK_LINE("Array delimiters \"{}\" are not at start and end of values");
        }
        if (strItems[1] == "=" && strItems[index].IsEmpty())
          FAIL_CHECK_LINE("Empty assignment in line");
        if (WordIsReserved(strItems[0]))
          FAIL_CHECK_LINE("Assignment to reserved command name in line");
        if (strItems[0].GetAt(0) == '$')
          FAIL_CHECK_LINE("You cannot assign to a name that is a variable in line");
        if (!stringAssign &&
          CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine, errmess))
          return 99;
        continue;
      }

      // If the command is a variable, skip checks except to make sure it is not a label
      if (strItems[0].GetAt(0) == '$') {
        if (strItems[0].GetAt(strItems[0].GetLength() - 1) == ':')
          FAIL_CHECK_LINE("Label cannot be a variable in line");
        continue;
      }

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
          FAIL_CHECK_LINE("Label must not be followed by anything in line");
        if (length == 1)
          FAIL_CHECK_LINE("Empty label in line");
        if (numLabels >= MAX_MACRO_LABELS)
          FAIL_CHECK_NOLINE("Too many labels in script");
        strLabels[numLabels] = strItems[0].Left(length - 1);
        for (i = 0; i < numLabels; i++) {
          if (strLabels[i] == strLabels[numLabels])
            FAIL_CHECK_LINE("Duplicate of label already used in line")
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
      if (CMD_IS(REPEAT))
        break;

      // Check macro call if it has an argument - leave to general command check if not
      else if ((CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT) || CMD_IS(CALLMACRO) || 
        CMD_IS(CALLSCRIPT) || CMD_IS(CALL) || CMD_IS(CALLFUNCTION)) && 
        !strItems[1].IsEmpty()) {
          func = NULL;

          if (strItems[1].GetAt(0) == '$')
            FAIL_CHECK_LINE("The script number or name cannot be a variable,");
          if (CMD_IS(CALL)) {
            index = FindCalledMacro(strLine, true);
            if (index < 0)
              return 12;
          } else if (CMD_IS(CALLFUNCTION)) {
            func = FindCalledFunction(strLine, true, index, i, macroNum);
            if (!func)
              return 12;
          } else {
            index = atoi(strItems[1]) - 1;

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
            if (CheckBlockNesting(index, blockLevel, tryLevel))
              return 14;
          }

          // Skip out if doing macro; otherwise pop stack again
          if (CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))
            break;
          mCallLevel--;

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
        if (isIF && CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine,errmess))
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
          errmess))
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
        FAIL_CHECK_LINE("THROW seems not to be contained in any TRY block in line");

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

        i = CheckLegalCommandAndArgNum(&strItems[0], strLine, macroNum);
        if (i == 17)
          return 17;
        if (i)
          FAIL_CHECK_LINE("Unrecognized command in line");
      }

      // Check commands where arithmetic is allowed
      if (ArithmeticIsAllowed(strItems[0]) && 
        CheckBalancedParentheses(strItems, MAX_MACRO_TOKENS, strLine, errmess))
        return 99;
      
      if (CMD_IS(SKIPIFVERSIONLESSTHAN) && currentIndex < macro->GetLength())
        GetNextLine(macro, currentIndex, strLine);
    }
  }
  if (blockLevel > startLevel)
    FAIL_CHECK_NOLINE("Unclosed IF, LOOP, or DOLOOP block");
  if (inComment)
    FAIL_CHECK_NOLINE("A comment started with /* was not closed with */");

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
int CMacroProcessor::CheckLegalCommandAndArgNum(CString * strItems, CString strLine, 
  int macroNum)
{
  int i = 0;
  CString errmess;

  // Loop on the commands
  while (mCmdList[i].cmd.length()) {
    if (strItems[0] == mCmdList[i].cmd.c_str()) {
      if (strItems[mCmdList[i].minargs].IsEmpty()) {
        errmess.Format("The command must be followed by at least %d entries\n"
          " on this line in script #%d:\n\n", mCmdList[i].minargs, macroNum + 1);
        SEMMessageBox(errmess + strLine, MB_EXCLAME);
        return 17;
      }
      return 0;
    }
    i++;
  }
  return 1;
}

// Check a line for balanced parentheses and no empty clauses
int CMacroProcessor::CheckBalancedParentheses(CString *strItems, int maxItems, 
                                              CString &strLine, CString &errmess)
{
  int ind, level = 0;
  if (SeparateParentheses(strItems, maxItems))
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
int CMacroProcessor::SeparateParentheses(CString *strItems, int maxItems)
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
      strItems[ind].GetAt(length - 1) == ')')) {
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
      mWinApp->mParamIO->ParseString(strLine, strItems, 4);
      strItems[0].MakeUpper();
      cmdIndex = LookupCommandIndex(strItems[0]);
      isCATCH = CMD_IS(CATCH);
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
      if ((!ifLevel && ((type == SKIPTO_ELSE_ENDIF && (CMD_IS(ELSE) || CMD_IS(ELSEIF)))
        || ((type == SKIPTO_ENDIF || type == SKIPTO_ELSE_ENDIF) && CMD_IS(ENDIF))) ||
        (!loopLevel && type == SKIPTO_ENDLOOP && CMD_IS(ENDLOOP))) ||
        (!popTry && type == SKIPTO_CATCH && isCATCH) || 
        (!tryLevel && type == SKIPTO_ENDTRY && CMD_IS(ENDTRY))) {
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
      mWinApp->mParamIO->ParseString(strLine, strItems, 4);
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
  mCallLevel--;
  if (mCurrentMacro == mNeedClearTempMacro) {
    mMacros[mNeedClearTempMacro] = "";
    mNumTempMacros--;
    mNeedClearTempMacro = -1;
  }
  mCurrentMacro = mCallMacro[mCallLevel];
  mCurrentIndex = mCallIndex[mCallLevel];
  mLastIndex = -1;
}

// Convert a single number to a DOMACRO
void CMacroProcessor::InsertDomacro(CString * strItems)
{
  if (strItems[1].IsEmpty()) {
    for (int i = 0; i < MAX_MACROS; i++) {
      if (mStrNum[i] == strItems[0]) {
        strItems[0] = "DOMACRO";
        strItems[1] = mStrNum[i];
        break;
      }
    }
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
  mWinApp->mParamIO->StripItems(strLine, index, strCopy);
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
                                          bool checkImage, int &bufIndex,
                                          CString &message)
{
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
      if (bufIndex < 0 || bufIndex >= MAX_BUFFERS || strItem.GetLength() > 1) {
        message = "Improper buffer letter " + strItem + " in statement: \r\n\r\n";
        return true;
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
  if (checkImage && !mImBufs[bufIndex].mImage) {
    message = "There is no image in buffer " + strItem + " in statement: \r\n\r\n";
    return true;
  }
  return false;
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
// Returns tre with message on error
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
    if (itemInt >= 0)
      return false;
  }
  message = "Inappropriate parameter set letter/number in: \n\n";
  return true;
}

// If adjustment of beam tilt for given image shift is requested and available, do it
int CMacroProcessor::AdjustBeamTiltIfSelected(double delISX, double delISY, BOOL doAdjust,
  CString &message)
{
  double delBTX, delBTY, transISX, transISY, astigX, astigY, BTX, BTY;
  int probe = mScope->GetProbeMode();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  if (!doAdjust)
    return 0;
  if (comaVsIS->magInd <= 0) {
    message = "There is no calibration of beam tilt needed to compensate image shift "
      "in:\n\n";
    return 1;
  }
  mScope->GetBeamTilt(BTX, BTY);
  if (mBeamTiltXtoRestore[probe] < EXTRA_VALUE_TEST) {
    mBeamTiltXtoRestore[probe] = BTX;
    mBeamTiltYtoRestore[probe] = BTY;
    mNumStatesToRestore++;
  }
  mShiftManager->TransferGeneralIS(mScope->FastMagIndex(), delISX, delISY, 
    comaVsIS->magInd, transISX, transISY);
  delBTX = comaVsIS->matrix.xpx * transISX + comaVsIS->matrix.xpy * transISY;
  delBTY = comaVsIS->matrix.ypx * transISX + comaVsIS->matrix.ypy * transISY;
  mWinApp->mAutoTuning->BacklashedBeamTilt(BTX + delBTX, BTY + delBTY, 
    mScope->GetAdjustForISSkipBacklash() <= 0);
  mCompensatedBTforIS = true;
  if (comaVsIS->astigMat.xpx && !mNavHelper->GetSkipAstigAdjustment()) {
    mScope->GetObjectiveStigmator(astigX, astigY);
    if (mAstigXtoRestore[probe] < EXTRA_VALUE_TEST) {
      mAstigXtoRestore[probe] = astigX;
      mAstigYtoRestore[probe] = astigY;
      mNumStatesToRestore++;
    }
    delBTX = comaVsIS->astigMat.xpx * transISX + comaVsIS->astigMat.xpy * transISY;
    delBTY = comaVsIS->astigMat.ypx * transISX + comaVsIS->astigMat.ypy * transISY;
    mWinApp->mAutoTuning->BacklashedStigmator(astigX + delBTX, astigY + delBTY, 
      mScope->GetAdjustForISSkipBacklash() <= 0);
  }
  return 0;
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
      ABORT_NORET_LINE("There is no current Navigator item for line:\n\n.");
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
  int tryLevel = 0;
  if (mMacroEditer[macnum])
    mMacroEditer[macnum]->TransferMacro(true);
  if (mMacros[macnum].IsEmpty()) {
    SEMMessageBox("The script you selected to run is empty", MB_EXCLAME);
    return 1;
  } 
  PrepareForMacroChecking(macnum);
  if (CheckBlockNesting(macnum, -1, tryLevel))
    return 2;
 return 0;
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

// Busy function for the task to start the Navigator Acquire
int CMacroProcessor::StartNavAvqBusy(void)
{
  if (DoingMacro())
    return 1;
  if (mLastCompleted && mWinApp->mNavigator) 
    mWinApp->mNavigator->AcquireAreas(false);
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

// Convenience function to extract part of entered line after substituting variables
void CMacroProcessor::SubstituteLineStripItems(CString & strLine, int numStrip, 
  CString &strCopy)
{
  SubstituteVariables(&strLine, 1, strLine);
  mWinApp->mParamIO->StripItems(strLine, numStrip, strCopy);
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
