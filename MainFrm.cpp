// MainFrm.cpp:           Standard MFC MDI component, manages the main program 
//                          window and has code for program shutdown, control
//                          panel management and status line entries
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ChildFrm.h"
#include "LogWindow.h"
#include "EMscope.h"
#include "shiftManager.h"
#include "TSController.h"
#include "CameraController.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "FalconHelper.h"
#include "ProcessImage.h"
#include "XFolderDialog\XWinVer.h"

#include ".\MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DIALOG_TOP_OFFSET 46
#define DIALOG_LEFT_OFFSET 4
#define WIN10_LEFT_OFFSET 10
#define DIALOG_CLOSED_HEIGHT 19
#define MIN_WIDTH_FOR_EXIT 800

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
  //{{AFX_MSG_MAP(CMainFrame)
  ON_WM_CREATE()
  ON_WM_KEYDOWN()
  ON_WM_MOVE()
  ON_WM_CLOSE()
  ON_WM_SIZE()
  ON_WM_TIMER()
  //}}AFX_MSG_MAP
	ON_COMMAND(ID_WINDOW_NEW, OnWindowNew)
  ON_COMMAND(ID_HELP_FINDER, CMDIFrameWnd::OnHelpFinder)
  ON_COMMAND(ID_HELP, CMDIFrameWnd::OnHelp)
  ON_COMMAND(ID_CONTEXT_HELP, CMDIFrameWnd::OnContextHelp)
  ON_COMMAND(ID_DEFAULT_HELP, CMDIFrameWnd::OnHelpFinder)
END_MESSAGE_MAP()

static UINT indicators[] =
{
  ID_SEPARATOR,           // status line indicator
  0, //ID_INDICATOR_CAPS,
  0, //ID_INDICATOR_NUM,
  0, //ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
  SEMBuildTime(__DATE__, __TIME__);
  mDialogTable = NULL;
  m_nTimer = 0;
  mDialogOffset = 0;
  mClosingProgram = false;
  mLeftDialogOffset = IsVersion(10, VER_GREATER_EQUAL, 0, VER_GREATER_EQUAL) ?
    WIN10_LEFT_OFFSET : DIALOG_LEFT_OFFSET;
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  // Can't delete this without handling more things...
  if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
    | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
    !m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
  {
    TRACE0("Failed to create toolbar\n");
    return -1;      // fail to create
  }

  if (!m_wndStatusBar.Create(this) ||
    !m_wndStatusBar.SetIndicators(indicators,
      sizeof(indicators)/sizeof(UINT)))
  {
    TRACE0("Failed to create status bar\n");
    return -1;      // fail to create
  }

  // TODO: Delete these three lines if you don't want the toolbar to
  //  be dockable
  m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
  EnableDocking(CBRS_ALIGN_ANY);
  DockControlBar(&m_wndToolBar);
  m_wndToolBar.ShowWindow(SW_HIDE);


  return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
  if( !CMDIFrameWnd::PreCreateWindow(cs) )
    return FALSE;
  // TODO: Modify the Window class or styles here by modifying
  //  the CREATESTRUCT cs
  // Prevent child titles from changing main window title
  cs.style &= ~FWS_ADDTOTITLE;
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
  CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
  CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

void CMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  char cChar = char(nChar);
  char text[32];
  sprintf(text, "Code %u, char %c", nChar, cChar);
  MessageBox(CString(text));
  
  CMDIFrameWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CMainFrame::DialogChangedState(CToolDlg *inDialog, int inState)
{
  int i;
  for (i = 0; i < mNumDialogs; i++) {
    if (inDialog == mDialogTable[i].pDialog) {
      if (inState != mDialogTable[i].state) {
        mDialogTable[i].state = inState;
        SetDialogPositions();
        break;
      }
    }
  }
}


// Finish initializing dialogs and set up the dialog table by finding out 
// the initial full size of each one
void CMainFrame::InitializeDialogTable(DialogTable *inTable, int *initialState, 
                     int numDialog, COLORREF *borderColors, int *colorIndex,
                     RECT *dlgPlacements)
{
  CRect rect;
  mDialogTable = inTable;
  mNumDialogs = numDialog;
  for (int i = 0; i < numDialog; i++)
    InitializeOneDialog(inTable[i], colorIndex[i], borderColors);
  InitializeDialogPositions(initialState, dlgPlacements, colorIndex);
}

// Do operations to finish initializing one dialog and its table entry
void CMainFrame::InitializeOneDialog(DialogTable &inTable, int colorIndex, 
  COLORREF *borderColors)
{
  CRect rect;
  CToolDlg *dlg = inTable.pDialog;
  dlg->SetBorderColor(borderColors[colorIndex]);
  dlg->ShowWindow(SW_SHOW);
  dlg->GetWindowRect(&rect);
  inTable.width = rect.Width();
  inTable.fullHeight = rect.Height();
  /*if (colorIndex == SCOPE_PANEL_INDEX && !mWinApp->GetAnyDirectDetectors())
    inTable.fullHeight -= ((CScopeStatusDlg *)dlg)->DoseHeightAdjustment();*/
  if (mWinApp->GetDisplayNot120DPI()) {
    inTable.width += mWinApp->GetToolExtraWidth();
    inTable.fullHeight += mWinApp->GetToolExtraHeight();
  }
  inTable.midHeight = dlg->GetMidHeight();
}

// Remove a dialog with given color index from the tool panel array, returns 1 if that
// index cannot be found
int CMainFrame::RemoveOneDialog(int colorInd)
{
  int *dlgColorIndex = mWinApp->GetDlgColorIndex();
  int ind;
  int dlgInd = mWinApp->LookupToolDlgIndex(colorInd);
  if (dlgInd < 0)
    return 1;
  CToolDlg *dlg = mDialogTable[dlgInd].pDialog;
  for (ind = dlgInd; ind < mNumDialogs - 1; ind++) {
    mDialogTable[ind] = mDialogTable[ind + 1];
    dlgColorIndex[ind] = dlgColorIndex[ind + 1];
  }
  mNumDialogs--;
  dlg->DestroyWindow();
  mWinApp->SetMaxDialogWidth();
  SetDialogPositions();
  return 0;
}

// Insert a dialog with the given color index into the tool panel array and initialize it
void CMainFrame::InsertOneDialog(CToolDlg *dlg, int colorInd, COLORREF *borderColors)
{
  int *dlgColorIndex = mWinApp->GetDlgColorIndex();
  int ind, insertAt;
  for (insertAt = 0; insertAt < mNumDialogs; insertAt++)
    if (dlgColorIndex[insertAt] > colorInd)
      break;
  for (ind = mNumDialogs - 1; ind >= insertAt; ind--) {
    mDialogTable[ind + 1] = mDialogTable[ind];
    dlgColorIndex[ind + 1] = dlgColorIndex[ind];
  }
  mNumDialogs++;
  mDialogTable[insertAt].pDialog = dlg;
  dlgColorIndex[insertAt] = colorInd;
  InitializeOneDialog(mDialogTable[insertAt], colorInd, borderColors);
  mWinApp->SetMaxDialogWidth();
  SetDialogPositions();
}

// Set up initial dialog positions
void CMainFrame::InitializeDialogPositions(int *initialState, RECT *dlgPlacements, 
  int *colorIndex)
{
  CToolDlg *dlg;
  WINDOWPLACEMENT winPlace;
  int useInd, state;
  for (int i = 0; i < mNumDialogs; i++) {

    // Get index of state/placement to use: by default it is dialog index, but
    // if using absolute indexes, it is the color index; and if showing remote control
    // (for the first time) set it closed and shift indexes down for the rest
    useInd = i;
    if (mWinApp->GetAbsoluteDlgIndex()) {
      useInd = colorIndex[i];
    } else if (mWinApp->GetShowRemoteControl()) {
      if (colorIndex[i] > REMOTE_PANEL_INDEX)
        useInd = i - 1;
      else if (colorIndex[i] == REMOTE_PANEL_INDEX)
        useInd = -1;
    }
    state = useInd < 0 ? 0 : initialState[useInd];
    mDialogTable[i].state = state;
    dlg = mDialogTable[i].pDialog;
    dlg->SetOpenClosed(state);

    // If the window is floating and there is a placement rectangle, set the position
    if ((state & TOOL_FLOATDOCK) && 
      dlgPlacements[useInd].right > 0 && dlgPlacements[useInd].bottom > 0) {
      dlg->GetWindowPlacement(&winPlace);
      winPlace.rcNormalPosition.top = dlgPlacements[useInd].top;
      winPlace.rcNormalPosition.left = dlgPlacements[useInd].left;
      winPlace.rcNormalPosition.bottom = dlgPlacements[useInd].bottom;
      winPlace.rcNormalPosition.right = dlgPlacements[useInd].right;
      dlg->SetWindowPlacement(&winPlace);
    }
  }
  SetDialogPositions();
}

// Set the dialog positions.  Go down from the top, setting the height of
// each based on its state
void CMainFrame::SetDialogPositions()
{
  int i, height, xoff, yoff;
  CRect rect;

  if (!mDialogTable || mClosingProgram)
    return;

  GetWindowRect(&rect);
  xoff = rect.left + mLeftDialogOffset;
  yoff = rect.top + (mDialogOffset ? mDialogOffset : DIALOG_TOP_OFFSET);
  
  // If the toolbar is on, add its height
  if (m_wndToolBar.IsWindowVisible()) {
    m_wndToolBar.GetWindowRect(&rect);
    yoff += rect.Height();
  }

  for (i = 0; i < mNumDialogs; i++) {
    if (mDialogTable[i].state & TOOL_OPENCLOSED && 
      !(mDialogTable[i].state & TOOL_FULLOPEN) &&
      mDialogTable[i].midHeight != 0)
      height = mDialogTable[i].midHeight;
    else if (mDialogTable[i].state & TOOL_OPENCLOSED)
      height = mDialogTable[i].fullHeight;
    else
      height = DIALOG_CLOSED_HEIGHT;

    if (mDialogTable[i].state & TOOL_FLOATDOCK) {
      height += mWinApp->GetToolTitleHeight();
      mDialogTable[i].pDialog->SetWindowPos(NULL, xoff, yoff, mDialogTable[i].width,
                          height, SWP_NOMOVE);

    } else {
      mDialogTable[i].pDialog->SetWindowPos(NULL, xoff, yoff, mDialogTable[i].width,
                          height, SWP_NOZORDER);
      yoff += height;
    }
  }

  // Do second loop to bring floaters to top
  for (i = 0; i < mNumDialogs; i++) {
    if (mDialogTable[i].state & TOOL_FLOATDOCK)
      mDialogTable[i].pDialog->BringWindowToTop();
  }

  mWinApp->RestoreViewFocus();    
}

// The first time the main window appears, set up the dialog offset
void CMainFrame::SetDialogOffset(CSerialEMView *mainView)
{
  CRect mainRect, childRect;
  CChildFrame *childFrame = (CChildFrame *)mainView->GetParent();
  childFrame->GetWindowRect(childRect);
  GetWindowRect(mainRect);
  mDialogOffset = childRect.top - mainRect.top;
//  SetDialogPositions();
}

void CMainFrame::OnMove(int x, int y) 
{
  CMDIFrameWnd::OnMove(x, y);
  
  SetDialogPositions(); 
}

// The old View Menu had Toolbar generating ID_VIEW_TOOLBAR
// and Status Bar generating ID_VIEW_STATUS_BAR

//void CMainFrame::OnViewToolbar() 
//{
//  CMDIFrameWnd::OnViewToolbar();
/*  if (m_wndToolBar.IsWindowVisible())
    MessageBox("Is Vis");
  else */
//    MessageBox("Is not");

//}


void CMainFrame::OnClose() 
{
  WINDOWPLACEMENT winPlace;
  int magInd;
  bool skipReset = false;

  if (  mClosingProgram)
    return;
    mClosingProgram = true;

  if (mWinApp->mTSController->TerminateOnExit()) {
      mClosingProgram = false;
    return;
  }

  if (mWinApp->mFalconHelper->GetStackingFrames()) {
    if (AfxMessageBox("It SEEMS that frames from the Falcon are still being stacked\n"
      "Do you really want to exit?", MB_QUESTION) != IDYES) {
        mClosingProgram = false;
      return;
    }
  }


  if (mWinApp->mNavigator)
    mWinApp->SetOpenStateWithNav(mWinApp->mNavHelper->mStateDlg != NULL);

  // Auto save files, may save some inquiries
  mWinApp->mDocWnd->AutoSaveFiles();

  // Want to shut off low dose mode now so that parameters return
  // to their normal states before settings are saved
  mWinApp->mLowDoseDlg.SetLowDoseMode(false);

  if (mWinApp->mScope->GetDoingLongOperation() && 
    mWinApp->mScope->StopLongOperation(true)) {
        mClosingProgram = false;
      return;
  }
  
  if (mWinApp->mDocWnd->SaveSettingsOnExit() ||
    (!mWinApp->GetExitWithUnsavedLog() && 
    mWinApp->mLogWindow && mWinApp->mLogWindow->AskIfSave("exiting?")) ||
    mWinApp->mNavigator && mWinApp->mNavigator->AskIfSave("exiting?")) {
      mClosingProgram = false;
    return;
  }

  if (mWinApp->mBufferManager->CheckAsyncSaving()) {
    if (AfxMessageBox("Do want to save that image somewhere before exiting?", 
      MB_QUESTION) == IDYES) {
        mClosingProgram = false;
      return;
    }
  }

  mWinApp->SetAppExiting(true);
  mWinApp->mCamera->CheckAndFreeK2References(true);

  // If this is closed automatically, there are activation context errors on exit
  if (mWinApp->mProcessImage->GetSideBySideFFT() && mWinApp->mFFTView)
    mWinApp->mFFTView->CloseFrame();

  // If scope is already disconnected, kill the timer now to uninitialize it
  if (mWinApp->mScope->GetDisconnected())
    mWinApp->mScope->KillUpdateTimer();

  // Turn off frame saving if possible, or tell user to do so
  if (mWinApp->mCamera->GetFrameSavingEnabled()) {
    if (mWinApp->mCamera->GetCanUseFalconConfig() > 0)
      mWinApp->mFalconHelper->CheckFalconConfig(-3, magInd, "Failed to restore initial "
      "state of Intermediate frame saving; check the FEI dialog");
    else if (!mWinApp->mCamera->IsCameraFaux())
      AfxMessageBox("Remember to turn off Intermediate frame saving in the FEI dialog", 
        MB_EXCLAME);
  }

  // Be bold and reset shifts now, without a backlash if one is programmed
  // then sit on stage busy
  if (mWinApp->mScope->GetInitialized()) {
    mWinApp->mScope->BlankBeam(false);
    mWinApp->mScope->SetTiltAxisOffset(0.);
    if (HitachiScope) {
      magInd = mWinApp->mScope->GetMagIndex();
      skipReset = magInd < mWinApp->mScope->GetLowestMModeMagInd() || 
        magInd >= mWinApp->mScope->GetLowestSecondaryMag();
    }
    if (!mWinApp->mScope->StageBusy() && !(JEOLscope && mWinApp->mScope->GetSTEMmode())
      && !skipReset) {
      if (mWinApp->mShiftManager->ResetImageShift(false, false) ||
        mWinApp->mScope->WaitForStageReady(10000))
        AfxMessageBox("Timeout trying to reset image shifts while exiting", MB_EXCLAME);
    }
    if (mWinApp->GetEFTEMMode() && !mWinApp->GetKeepEFTEMstate())
      mWinApp->SetEFTEMMode(false);
    else
      mWinApp->RestoreCameraForExit();
  }
  mWinApp->mScope->KillUpdateTimer();
  mWinApp->mCamera->KillUpdateTimer();
  mWinApp->mDEToolDlg.KillUpdateTimer();
  mWinApp->mCamera->RestoreGatanOrientations();
  mWinApp->mCamera->RestoreFEIshutter();

  // If frame width is too narrow, make it wider to avoid crash bug when
  // exiting (i.e. if menu line is wrapped)
  GetWindowPlacement(&winPlace);
  if (winPlace.rcNormalPosition.right - winPlace.rcNormalPosition.left < 
    MIN_WIDTH_FOR_EXIT) {
    winPlace.rcNormalPosition.left = 10;
    winPlace.rcNormalPosition.right = 10 + MIN_WIDTH_FOR_EXIT;
    SetWindowPlacement(&winPlace);
  }

  mWinApp->mDocWnd->CloseAllStores();
  mWinApp->mDocWnd->AppendToProgramLog(false);
  CMDIFrameWnd::OnClose();
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
  CMDIFrameWnd::OnSize(nType, cx, cy);
  if (mClosingProgram)
    return;
  mWinApp->DoResizeMain();
  mWinApp->RestoreViewFocus();    
}

void CMainFrame::SetStatusText(int iPane, CString strText)
{
  m_wndStatusBar.SetPaneText(iPane, strText);
}

void CMainFrame::InitializeStatusBar()
{
  // Set the sizes of the the status bar panes
  UINT uID, uStyle;
  int nWidth;
  CRect rectArea;
  CFont font;
  m_wndStatusBar.GetPaneInfo(1, uID, uStyle, nWidth);
  CDC *pDC = m_wndStatusBar.GetDC();
  m_wndStatusBar.GetClientRect(&rectArea);
  CStatusBarCtrl &sbCtrl =  m_wndStatusBar.GetStatusBarCtrl();
  sbCtrl.GetRect(1,&rectArea);  // IS two pixels smaller
  mFont.CreateFont((rectArea.Height()), 0, 0, 0, FW_SEMIBOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  m_wndStatusBar.SetFont(&mFont);
  pDC->SelectObject(m_wndStatusBar.GetFont());
  
  // Use the longest expected text
  pDC->DrawText(_T("STOPPED TILT SERIES  "), -1, rectArea, 
    DT_SINGLELINE | DT_CALCRECT);
  m_wndStatusBar.SetPaneInfo(1, uID, uStyle, rectArea.Width());
  pDC->DrawText(_T("CORRECTING TILT BACKLASH  "), -1, rectArea, 
    DT_SINGLELINE | DT_CALCRECT);
  m_wndStatusBar.SetPaneInfo(2, uID, uStyle, rectArea.Width());
  pDC->DrawText(_T("INSERTING CAMERA  "), -1, rectArea, 
    DT_SINGLELINE | DT_CALCRECT);
  m_wndStatusBar.SetPaneInfo(3, uID, uStyle, rectArea.Width());
  m_wndStatusBar.ReleaseDC(pDC);
  // SetStatusText(2, "DOING NOTHING");

}

// Functions to start and check a timer if an interval is set
#ifdef TASK_TIMER_INTERVAL
BOOL CMainFrame::NewTask()
{
  if (!m_nTimer)
    m_nTimer = SetTimer(ID_TASK_TIMER, TASK_TIMER_INTERVAL, NULL);
  return m_nTimer != 0;
}

void CMainFrame::OnTimer(UINT nIDEvent) 
{
  // Chek the idle tasks, kill timer if there are no more
  if (!mWinApp->CheckIdleTasks()) {
    KillTimer(ID_TASK_TIMER);
    m_nTimer = 0;
  }
  CMDIFrameWnd::OnTimer(nIDEvent);
}
#endif

void CMainFrame::OnWindowNew()
{
  CMDIFrameWnd::OnWindowNew();
}
