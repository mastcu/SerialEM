// CameraMacroTools.cpp:  Has buttons for taking pictures, running macros,
//                           and controlling tilt series.  Has general STOP.
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\CameraMacroTools.h"
#include "MacroProcessor.h"
#include "MainFrm.h"
#include "CameraController.h"
#include "ProcessImage.h"
#include "MacroEditer.h"
#include "MacroToolbar.h"
#include "TSController.h"
#include "MultiTSTasks.h"
#include "NavigatorDlg.h"
#include "ShiftCalibrator.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
static int sIdTable[] = {IDC_BUTFLOATDOCK, IDC_STAT_CAMNAME, IDC_BUTOPEN, IDC_STATTOPLINE,
IDC_BUTHELP, PANEL_END, IDC_BUT_SETUP_CAM, IDC_BUT_VIEW, IDC_BUT_FOCUS, 
IDC_BUT_TRIAL, IDC_BUT_RECORD, IDC_BUTMONTAGE, IDC_BUTMACRO1, IDC_BUTMACRO2,
IDC_SPINMACRO2, IDC_SPINMACRO1, IDC_BUTSTOP, IDC_BUTEND, IDC_BUTRESUME,
IDC_BUTMACRO3, IDC_SPINMACRO3, PANEL_END, 
IDC_BUTMACRO4, IDC_BUTMACRO5, IDC_BUTMACRO6, IDC_SPINMACRO4, IDC_SPINMACRO5,
IDC_SPINMACRO6, PANEL_END,
IDC_BUTMACRO7, IDC_BUTMACRO8, IDC_BUTMACRO9, IDC_SPINMACRO7, IDC_SPINMACRO8,
IDC_SPINMACRO9, PANEL_END,
IDC_BUTMACRO10, IDC_BUTMACRO11, IDC_BUTMACRO12, IDC_SPINMACRO10, IDC_SPINMACRO11,
IDC_SPINMACRO12, PANEL_END,
TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

/////////////////////////////////////////////////////////////////////////////
// CCameraMacroTools dialog


CCameraMacroTools::CCameraMacroTools(CWnd* pParent /*=NULL*/)
  : CToolDlg(CCameraMacroTools::IDD, pParent)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CCameraMacroTools)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  for (int ind = 0; ind < NUM_SPINNER_MACROS; ind++)
    mMacroNumber[ind] = ind;
  mDoingTS = false;
  mUserStop = FALSE;
  mDeferredUserStop = false;
}


void CCameraMacroTools::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CCameraMacroTools)
  DDX_Control(pDX, IDC_SPINMACRO3, m_sbcMacro3);
  DDX_Control(pDX, IDC_SPINMACRO2, m_sbcMacro2);
  DDX_Control(pDX, IDC_SPINMACRO1, m_sbcMacro1);
  DDX_Control(pDX, IDC_STATTOPLINE, m_statTopline);
  DDX_Control(pDX, IDC_BUTSTOP, m_butStop);
  DDX_Control(pDX, IDC_BUTMACRO3, m_butMacro3);
  DDX_Control(pDX, IDC_BUTMACRO2, m_butMacro2);
  DDX_Control(pDX, IDC_BUTMACRO1, m_butMacro1);
  DDX_Control(pDX, IDC_BUTRESUME, m_butResume);
  DDX_Control(pDX, IDC_BUTEND, m_butEnd);
  DDX_Control(pDX, IDC_BUT_VIEW, m_butView);
  DDX_Control(pDX, IDC_BUT_TRIAL, m_butTrial);
  DDX_Control(pDX, IDC_BUT_RECORD, m_butRecord);
  DDX_Control(pDX, IDC_BUT_SETUP_CAM, m_butSetup);
  DDX_Control(pDX, IDC_BUTMONTAGE, m_butMontage);
  DDX_Control(pDX, IDC_BUT_FOCUS, m_butFocus);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCameraMacroTools, CToolDlg)
  //{{AFX_MSG_MAP(CCameraMacroTools)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINMACRO1, OnDeltaposSpinmacro1)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINMACRO2, OnDeltaposSpinmacro2)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINMACRO3, OnDeltaposSpinmacro3)
  ON_BN_CLICKED(IDC_BUTMACRO1, OnButmacro1)
  ON_BN_CLICKED(IDC_BUTMACRO2, OnButmacro2)
  ON_BN_CLICKED(IDC_BUTMACRO3, OnButmacro3)
  ON_BN_CLICKED(IDC_BUTSTOP, OnButstop)
  ON_BN_CLICKED(IDC_BUTEND, OnButend)
  ON_BN_CLICKED(IDC_BUTRESUME, OnButresume)
  ON_BN_CLICKED(IDC_BUTMONTAGE, OnButmontage)
  ON_NOTIFY(NM_LDOWN, IDC_BUTMACRO1, OnMacBut1Draw)
  ON_NOTIFY(NM_LDOWN, IDC_BUTMACRO2, OnMacBut2Draw)
  ON_NOTIFY(NM_LDOWN, IDC_BUTMACRO3, OnMacBut3Draw)
  ON_NOTIFY_RANGE(NM_LDOWN, IDC_BUTMACRO4, IDC_BUTMACRO12, OnMacButNDraw)
  ON_NOTIFY_RANGE(UDN_DELTAPOS, IDC_SPINMACRO4, IDC_SPINMACRO12, OnDeltaposSpinmacroN)
  ON_COMMAND_RANGE(IDC_BUTMACRO4, IDC_BUTMACRO12, OnButmacroN)
  ON_WM_PAINT()
  //}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BUT_SETUP_CAM, OnButSetupCam)
  ON_COMMAND_RANGE(IDC_BUT_VIEW, IDC_BUT_RECORD, OnButVFTR)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCameraMacroTools message handlers

BOOL CCameraMacroTools::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  int ind;
  CSpinButtonCtrl *spin;
  CButton *but;
  mMacros = mWinApp->GetMacros();
  mMacProcessor = mWinApp->mMacroProcessor;

  // fix the fonts of the buttons
  m_butView.SetFont(mLittleFont);
  m_butFocus.SetFont(mLittleFont);
  m_butTrial.SetFont(mLittleFont);
  m_butRecord.SetFont(mLittleFont);
  m_butMontage.SetFont(mLittleFont);
  m_butMacro1.SetFont(mLittleFont);
  m_butMacro2.SetFont(mLittleFont);
  m_butMacro3.SetFont(mLittleFont);
  m_butSetup.SetFont(mLittleFont);
  m_butEnd.SetFont(mLittleFont);
  m_butResume.SetFont(mLittleFont);
  m_butStop.mSpecialColor = RGB(255, 64, 64);

  // Set the names of the camera modes
  CString *modeNames = mWinApp->GetModeNames();
  m_butView.SetWindowText(modeNames[0]);
  m_butFocus.SetWindowText(modeNames[1]);
  m_butTrial.SetWindowText(modeNames[2]);
  m_butRecord.SetWindowText(modeNames[3]);

  // Set the spin button controls
  for (ind = 0; ind < 3 * (NUM_CAM_MAC_PANELS - 1); ind++) {
    spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPINMACRO1 + ind);
    if (spin)
      spin->SetRange(1, MAX_MACROS);
    but = (CMyButton *)GetDlgItem(IDC_BUTMACRO1 + ind);
    if (but)
      but->SetFont(mLittleFont);

    // Allow push `buttons to grow if spinners are hidden
    mGrowWidthSet.insert(IDC_BUTMACRO1 + ind);
    mIDsToGrowWidth.push_back(IDC_BUTMACRO1 + ind);
    mIDsTakeWidthFrom.push_back(IDC_SPINMACRO1 + ind);
  }
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 
    sHeightTable);
  mLastRowsShows = mWinApp->mMacroProcessor->GetNumCamMacRows();
  ManagePanels();
  mInitialized = true;
  UpdateSettings();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CCameraMacroTools::OnPaint()
{
  CPaintDC dc(this); // device context for painting

  DrawSideBorders(dc);
}

void CCameraMacroTools::OnButSetupCam()
{
  mWinApp->OnCameraParameters();
}

void CCameraMacroTools::OnButVFTR(UINT nID)
{
  mWinApp->UserRequestedCapture(VIEW_CONSET + nID - IDC_BUT_VIEW);
}

void CCameraMacroTools::OnButmontage()
{
  mWinApp->RestoreViewFocus();
  if (mWinApp->Montaging() && !mWinApp->LowDoseMode())
    mWinApp->StartMontageOrTrial(false);
  else
    mWinApp->UserRequestedCapture(PREVIEW_CONSET);
}

void CCameraMacroTools::SetMacroLabels()
{
  int ind;
  if (!mInitialized)
    return;
  if (!mDoingCalISO)
    SetOneMacroLabel(0, IDC_BUTMACRO1);
  SetOneMacroLabel(1, IDC_BUTMACRO2);
  SetOneMacroLabel(2, IDC_BUTMACRO3);
  for (ind = 3; ind < 3 * mWinApp->mMacroProcessor->GetNumCamMacRows(); ind++)
    SetOneMacroLabel(ind, IDC_BUTMACRO4 + ind - 3);
}

void CCameraMacroTools::SetOneMacroLabel(int num, UINT nID)
{
  int navState = GetNavigatorState();
  CString label;
  if (num == 1 && (navState == NAV_SCRIPT_RUNNING || navState == NAV_SCRIPT_STOPPED ||
    navState == NAV_RUNNING_NO_SCRIPT_TS || navState == NAV_PAUSED)) {
    label = "End Nav";
  } else {
    CString *names = mWinApp->GetMacroNames();
    if (names[mMacroNumber[num]].IsEmpty())
      label.Format("Script %d", mMacroNumber[num] + 1);
    else
      label = names[mMacroNumber[num]];
  }
  SetDlgItemText(nID, label);
}

// External notification that a name changed
void CCameraMacroTools::MacroNameChanged(int num)
{
  int ind;
  if (mWinApp->mMacroToolbar) {
    mWinApp->mMacroToolbar->SetOneMacroLabel(num);
    mWinApp->mMacroToolbar->SetLength(mWinApp->mMacroProcessor->GetNumToolButtons(),
      mWinApp->mMacroProcessor->GetToolButHeight());
  }
  if (mDoingTS)
    return;
  if (num == mMacroNumber[0] && !mDoingCalISO)
    SetOneMacroLabel(0, IDC_BUTMACRO1);
  for (ind = 1; ind < 3 * (NUM_CAM_MAC_PANELS - 1); ind++) {
    if (num == mMacroNumber[ind])
      SetOneMacroLabel(ind, IDC_BUTMACRO1 + ind);
  }
}

void CCameraMacroTools::ManagePanels()
{
  BOOL states[NUM_CAM_MAC_PANELS];
  if (!mInitialized)
    return;
  int ind, numRows = mWinApp->mMacroProcessor->GetNumCamMacRows();
  for (ind = 0; ind < NUM_CAM_MAC_PANELS; ind++)
    states[ind] = ind <= numRows && (!ind || (GetState() & TOOL_OPENCLOSED));
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
  if (mLastRowsShows != numRows)
    mWinApp->mMainFrame->SetDialogPositions();
  mLastRowsShows = numRows;
}

void CCameraMacroTools::UpdateSettings()
{
  int ind;
  CSpinButtonCtrl *spin;
  for (ind = 0; ind < 3 * (NUM_CAM_MAC_PANELS - 1); ind++) {
    spin = (CSpinButtonCtrl *)GetDlgItem(IDC_SPINMACRO1 + ind);
    if (spin)
      spin->SetPos(mMacroNumber[ind] + 1);
  }
  if (!mDoingTS)
    SetMacroLabels();
  Update();
}

// Process a spin button, given the ID of the macro button
void CCameraMacroTools::DeltaposSpin(NMHDR *pNMHDR, LRESULT *pResult, UINT nID, 
                   int iMacro)
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  mWinApp->RestoreViewFocus();
  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  if (newVal < 1 || newVal > MAX_MACROS) {
    *pResult = 1;
    return;
  }
  mMacroNumber[iMacro] = newVal - 1;
  SetOneMacroLabel(iMacro, nID);
  Update();
  
  *pResult = 0;
}

void CCameraMacroTools::OnDeltaposSpinmacro1(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(pNMHDR, pResult, IDC_BUTMACRO1, 0);
}

void CCameraMacroTools::OnDeltaposSpinmacro2(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(pNMHDR, pResult, IDC_BUTMACRO2, 1);
}

void CCameraMacroTools::OnDeltaposSpinmacro3(NMHDR* pNMHDR, LRESULT* pResult) 
{
  DeltaposSpin(pNMHDR, pResult, IDC_BUTMACRO3, 2);
}

void CCameraMacroTools::OnDeltaposSpinmacroN(UINT nID, NMHDR * pNMHDR, LRESULT * pResult)
{
  int index = nID + 3 - IDC_SPINMACRO4;
  DeltaposSpin(pNMHDR, pResult, index + IDC_BUTMACRO4 - 3, index);
}

// Single routine to run a macro if a button is pushed
void CCameraMacroTools::DoMacro(int inMacro)
{
  mWinApp->RestoreViewFocus();
  if (inMacro >= 0 && inMacro < MAX_MACROS && !mWinApp->DoingTasks()) {

    // Open editor with Ctrl key down
    if (GetAsyncKeyState(VK_CONTROL) / 2 != 0) {
      mMacProcessor->OpenMacroEditor(inMacro);
      return;
    }

    // Otherwise first transfer macro if editor is open, then run if non-empty
    if (mWinApp->mMacroEditer[inMacro])
      mWinApp->mMacroEditer[inMacro]->TransferMacro(true);

    if (!mMacros[inMacro].IsEmpty())
      mMacProcessor->Run(inMacro);
  }
}

// Handlers for individual buttons
void CCameraMacroTools::OnButmacro1() 
{
  if (mDoingTS) {
    mWinApp->RestoreViewFocus();
    if (!mWinApp->DoingTiltSeries())
      mWinApp->mTSController->CommonResume(1, 0);
  } else if (mDoingCalISO) {
    mWinApp->RestoreViewFocus();
    mWinApp->mShiftCalibrator->CalISoffsetNextMag();
  } else
    DoMacro(mMacroNumber[0]);
}

void CCameraMacroTools::OnButmacro2() 
{
  int navState = GetNavigatorState();
  if (mDoingTS) {
    mWinApp->RestoreViewFocus();
    if (!mWinApp->DoingTiltSeries())
      mWinApp->mTSController->CommonResume(-1, 0);
  } else if (navState == NAV_SCRIPT_RUNNING || navState == NAV_SCRIPT_STOPPED || 
    navState == NAV_RUNNING_NO_SCRIPT_TS) {
    mWinApp->RestoreViewFocus();
    mNav->SetAcquireEnded(1);
  } else if (navState == NAV_PAUSED) {
    mWinApp->RestoreViewFocus();
    mNav->EndAcquireWithMessage();
  } else {
    DoMacro(mMacroNumber[1]);
  }
}

void CCameraMacroTools::OnButmacro3() 
{
  if (mDoingTS) {
    mWinApp->RestoreViewFocus();
    if (!mWinApp->DoingTiltSeries())
      mWinApp->mTSController->BackUpSeries();
  } else
    DoMacro(mMacroNumber[2]);
}

void CCameraMacroTools::OnButmacroN(UINT nID)
{
  int index = nID + 3 - IDC_BUTMACRO4;
  DoMacro(mMacroNumber[index]);
}

// Handlers for the right click from each button
void CCameraMacroTools::OnMacBut1Draw(NMHDR *pNotifyStruct, LRESULT *result)
{
  HandleMacroRightClick(&m_butMacro1, 0, !mDoingTS && !mDoingCalISO);
}

void CCameraMacroTools::OnMacBut2Draw(NMHDR *pNotifyStruct, LRESULT *result)
{
  int navState = GetNavigatorState();
  HandleMacroRightClick(&m_butMacro2, 1, !mDoingTS && !(navState == NAV_SCRIPT_RUNNING || 
    navState == NAV_SCRIPT_STOPPED || navState == NAV_RUNNING_NO_SCRIPT_TS || 
    navState == NAV_PAUSED));
}

void CCameraMacroTools::OnMacBut3Draw(NMHDR *pNotifyStruct, LRESULT *result)
{
  HandleMacroRightClick(&m_butMacro3, 2, !mDoingTS);
}

void CCameraMacroTools::OnMacButNDraw(UINT nID, NMHDR *pNotifyStruct, LRESULT *result)
{
  CMyButton *but = (CMyButton *)GetDlgItem(nID);
  if (!but)
    return;
  HandleMacroRightClick(but, nID + 3 - IDC_BUTMACRO4, !mDoingTS);
}

// Common function to open the editor on valid right click
void CCameraMacroTools::HandleMacroRightClick(CMyButton *but, int index, bool openOK)
{
  if (but->m_bRightWasClicked) {
    but->m_bRightWasClicked = false;
    if (openOK)
      mMacProcessor->OpenMacroEditor(mMacroNumber[index]);
  }
  mWinApp->mScope->SetRestoreViewFocusCount(1);
}


// The Stop button calls camera and general error stop
void CCameraMacroTools::OnButstop() 
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
  CString str;
  int navState = GetNavigatorState();
  mWinApp->RestoreViewFocus();
  if (navState == NAV_TS_STOPPED || navState == NAV_PRE_TS_STOPPED)
    mNav->SetAcquireEnded(1);
  else if (navState == NAV_SCRIPT_STOPPED) {
    mMacProcessor->SetNonResumable();
    mNav->EndAcquireWithMessage();
  } else if (mWinApp->mScope->DoingSynchroThread() ||
    mWinApp->mBufferManager->DoingSychroThread()) {
    mDeferredUserStop = true;
    mWinApp->mMainFrame->GetStatusText(MEDIUM_PANE, str);
    mMediumWasEmpty = str.IsEmpty();
    mWinApp->SetStatusText(MEDIUM_PANE, "STOPPING...");
  } else {
    DoUserStop();
  }
}

void CCameraMacroTools::DoUserStop(void)
{
  mWinApp->mCamera->StopCapture(1);
  mUserStop = TRUE;
  mWinApp->ErrorOccurred(0);
  mUserStop = FALSE;
  if (mDeferredUserStop && mMediumWasEmpty)
    mWinApp->SetStatusText(MEDIUM_PANE, "");
  mDeferredUserStop = false;
}

// The End button 
void CCameraMacroTools::OnButend() 
{
  int navState = GetNavigatorState();
  mWinApp->RestoreViewFocus();
  if (mWinApp->NavigatorStartedTS() && mWinApp->mTSController->GetPostponed())
    mWinApp->mTSController->Terminate();
  else if (mDoingTS) 
    mWinApp->mTSController->EndLoop();
  else if (mDoingCalISO)
    mWinApp->mShiftCalibrator->CalISoffsetDone(false);
  else if (mMacProcessor->DoingMacro())
    mMacProcessor->Stop(false);
  else if (mWinApp->mMultiTSTasks->GetAssessingRange())
    mWinApp->mMultiTSTasks->SetEndTiltRange(true);
  else if (mEnabledSearch)
    mWinApp->UserRequestedCapture(SEARCH_CONSET);
  else
    mMacProcessor->SetNonResumable();
}

// The Resume button
void CCameraMacroTools::OnButresume() 
{
  int navState = GetNavigatorState();
  mWinApp->RestoreViewFocus();
  if (mWinApp->NavigatorStartedTS() && !mWinApp->StartedTiltSeries() && 
    mWinApp->mTSController->GetPostponed())
    mWinApp->mTSController->SetupTiltSeries(0);
  else if (navState == NAV_TS_RUNNING || navState == NAV_PRE_TS_RUNNING)
    mNav->SetAcquireEnded(1);
  else if (mDoingTS)
    mWinApp->mTSController->Resume();
  else if (navState == NAV_SCRIPT_RUNNING || navState == NAV_RUNNING_NO_SCRIPT_TS)
    mNav->SetAcquireEnded(-1);
  else if (navState == NAV_PAUSED) {
    mMacProcessor->SetNonResumable();
    mNav->ResumeFromPause();
    mWinApp->SetStatusText(COMPLEX_PANE, "");
    Update();
  } else
    mMacProcessor->Resume();
}

////////////////////////////////////////////////
// Navigator-related button setup:
//
//   Running, non-script, non TS:              PauseN  STOP
//                                             EndNav
//   Paused, non-script, nonTS:                ResumeN
//                                             EndNav
//   TS running:                      EndLoop  EndNav  STOP
//   TS stopped:                      EndTS    ResumeT EndNav
//   preTS script running:            EndLoop  EndNav  STOP
//   preTS script stopped:            EndScr   ResumeS EndNav
//   Script running                   EndLoop  PauseN  STOP
//                                             EndNav
//   Script stopped:                  EndScr   ResumeS EndAll
//                                             EndNav


// Set the state of all buttons based on whether a task is being done, etc.
void CCameraMacroTools::Update()
{
  int navState = GetNavigatorState();
  BOOL camBusy = true;
  BOOL continuous = false;
  CString name;
  CameraParameters *camParams = mWinApp->GetCamParams();
  mEnabledSearch = false;
  bool stopEnabled;
  if (mWinApp->mCamera) {
    camBusy = mWinApp->mCamera->CameraBusy();
    continuous = mWinApp->mCamera->DoingContinuousAcquire();
  }
  BOOL idle = (!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen()) && 
    !(mNav && mNav->StartedMacro());
  BOOL shotOK = mWinApp->UserAcquireOK();
  BOOL postponed = mNav && mNav->GetStartedTS() && mWinApp->mTSController->GetPostponed();
  BOOL tsResumable = mWinApp->mTSController->IsResumable();
  m_butView.EnableWindow(shotOK);
  m_butTrial.EnableWindow(shotOK);
  m_butFocus.EnableWindow(shotOK);
  m_butRecord.EnableWindow(shotOK);
  if (!mWinApp->Montaging())
    m_butMontage.EnableWindow(shotOK);
  else
    m_butMontage.EnableWindow(idle && !mDoingTS && !postponed && !camBusy);
  
  if (mDoingTS) {
    
    // If doing a tilt series, enable single step and backup buttons if idle
    // check for resumability, and enable End if actually active
    m_butMacro1.EnableWindow(idle && tsResumable);
    m_butMacro2.EnableWindow(idle && tsResumable);
    m_butMacro3.EnableWindow(idle && tsResumable && mWinApp->mTSController->CanBackUp());
    m_butResume.EnableWindow((idle && (tsResumable || postponed)) || 
      navState == NAV_TS_RUNNING);
    m_butEnd.EnableWindow(mWinApp->DoingTiltSeries() || postponed);
  } else {
    
    // If no tilt series
    // Enable buttons if the macros are non empty or being edited
    if (mDoingCalISO)
      m_butMacro1.EnableWindow(!camBusy || continuous);
    else
      m_butMacro1.EnableWindow(idle && mMacProcessor->MacroRunnable(mMacroNumber[0]));
    m_butMacro2.EnableWindow((idle && mMacProcessor->MacroRunnable(mMacroNumber[1])) ||
      navState == NAV_SCRIPT_STOPPED || navState == NAV_SCRIPT_RUNNING || 
      navState == NAV_RUNNING_NO_SCRIPT_TS || navState == NAV_PAUSED);
    SetOneMacroLabel(1, IDC_BUTMACRO2);
    m_butMacro3.EnableWindow(idle && mMacProcessor->MacroRunnable(mMacroNumber[2]));
    
    m_butResume.EnableWindow(mMacProcessor->IsResumable() || (navState == NAV_PAUSED && 
      idle) || navState == NAV_SCRIPT_RUNNING || navState == NAV_RUNNING_NO_SCRIPT_TS);
  }

  // Keep STOP enabled during continuous acquires: the press event gets lost in repeated 
  // enable/disables 
  stopEnabled = (mWinApp->DoingTasks() && !mWinApp->GetJustChangingLDarea() &&
    !mWinApp->GetJustDoingSynchro() && !mWinApp->GetJustNavAcquireOpen() && 
    !mWinApp->mScope->DoingSynchroThread() && !mWinApp->mProcessImage->DoingAutoContour())
    || camBusy || mWinApp->mScope->GetMovingStage() || continuous ||
    navState == NAV_TS_STOPPED || navState == NAV_PRE_TS_STOPPED || 
    navState == NAV_SCRIPT_STOPPED;
  m_butStop.EnableWindow(stopEnabled);
  m_butStop.m_bShowSpecial = stopEnabled;
  m_butSetup.EnableWindow((!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen() || 
    mDoingCalISO) && (!camBusy || continuous));

  // Set the End button
  if (mMacProcessor->DoingMacro() || mWinApp->DoingTiltSeries())
    SetDlgItemText(IDC_BUTEND, "End Loop");
  else if (postponed)
    SetDlgItemText(IDC_BUTEND, "End TS");
  else if (mNav && mNav->StartedMacro() && mMacProcessor->IsResumable())
    SetDlgItemText(IDC_BUTEND, "End Script");
  else if (!mWinApp->mMultiTSTasks->GetAssessingRange() && !mDoingCalISO &&
    (mWinApp->LowDoseMode() || !mWinApp->GetUseViewForSearch())) {
      SetDlgItemText(IDC_BUTEND, "Search");
      mEnabledSearch = true;
  } else 
    SetDlgItemText(IDC_BUTEND, "End");

  // Set the Resume button
  if (navState == NAV_RUNNING_NO_SCRIPT_TS || navState == NAV_SCRIPT_RUNNING)
    SetDlgItemText(IDC_BUTRESUME, "PauseN");
  else if (navState == NAV_PAUSED && idle)
    SetDlgItemText(IDC_BUTRESUME, "ResumeN");
  else if (navState == NAV_TS_STOPPED)
    SetDlgItemText(IDC_BUTRESUME, "ResumeT");
  else if (navState == NAV_SCRIPT_STOPPED || navState == NAV_PRE_TS_STOPPED)
    SetDlgItemText(IDC_BUTRESUME, "ResumeS");
  else if (navState != NO_NAV_RUNNING)
    SetDlgItemText(IDC_BUTRESUME, "End Nav");
  else
    SetDlgItemText(IDC_BUTRESUME, "Resume");

  // Set the STOP button
  if (navState == NAV_TS_STOPPED || navState == NAV_PRE_TS_STOPPED)
    SetDlgItemText(IDC_BUTSTOP, "EndNav");
  else if (navState == NAV_SCRIPT_STOPPED)
    SetDlgItemText(IDC_BUTSTOP, "End All");
  else
    SetDlgItemText(IDC_BUTSTOP, "STOP");
    
  if (!mWinApp->Montaging() || mWinApp->LowDoseMode()) {
    CString *modeNames = mWinApp->GetModeNames();
    SetDlgItemText(IDC_BUTMONTAGE, modeNames[4]);
  } else
    SetDlgItemText(IDC_BUTMONTAGE, "Montage");
  if (!mDoingTS)
    m_butEnd.EnableWindow((mMacProcessor->DoingMacro() && 
      !mMacProcessor->GetRunningScrpLang()) || mDoingCalISO || 
      (mEnabledSearch && shotOK) || (navState != NO_NAV_RUNNING && 
      navState != NAV_RUNNING_NO_SCRIPT_TS && !(navState == NAV_PAUSED && idle)) || 
      mWinApp->mMultiTSTasks->GetAssessingRange());

  // Manage camera name
  if (!mWinApp->GetNoCameras()) {
    name = camParams[mWinApp->GetCurrentCamera()].name;
    SetDlgItemText(IDC_STAT_CAMNAME, name);
  }
}

void CCameraMacroTools::UpdateHiding(void)
{
  ManagePanels();
}

// Change macro tools to tilt series tools or vice versa
void CCameraMacroTools::DoingTiltSeries(BOOL ifTS)
{
  m_sbcMacro1.ShowWindow(ifTS ? SW_HIDE : SW_SHOW);
  m_sbcMacro2.ShowWindow(ifTS ? SW_HIDE : SW_SHOW);
  m_sbcMacro3.ShowWindow(ifTS ? SW_HIDE : SW_SHOW);
  m_sbcMacro1.EnableWindow(!ifTS);
  m_sbcMacro2.EnableWindow(!ifTS);
  m_sbcMacro3.EnableWindow(!ifTS);
  if (ifTS) {
    SetDlgItemText(IDC_STATTOPLINE, "Camera && TS");
    m_butMacro1.SetWindowText("1 Loop");
    m_butMacro2.SetWindowText("1 Step");
    m_butMacro3.SetWindowText("Backup");

  } else {
    SetDlgItemText(IDC_STATTOPLINE, "Camera && Script");
    SetMacroLabels();
  }
  mDoingTS = ifTS;
  Update();
}

// Test all the variables in the right order to determine navigator acquire state
int CCameraMacroTools::GetNavigatorState(void)
{
  mNav = mWinApp->mNavigator;
  BOOL idle = !mWinApp->DoingTasks();
  NavAcqParams *param = mWinApp->GetNavAcqParams(
    mWinApp->mNavHelper->GetCurAcqParamIndex());
  if (!mNav || !mNav->GetAcquiring())
    return NO_NAV_RUNNING;
  if (mNav->GetPausedAcquire())
    return NAV_PAUSED;
  if (!mNav->StartedMacro() && !mNav->GetStartedTS() && !idle)
    return NAV_RUNNING_NO_SCRIPT_TS;
  if (idle && !mNav->GetStartedTS() && (!mNav->StartedMacro() || 
    !mMacProcessor->IsResumable()))
    return NAV_PAUSED;

  // Should this test if postponed instead of idle?
  if (mNav->GetStartedTS())
    return (idle ? NAV_TS_STOPPED : NAV_TS_RUNNING);  
  if (param->acquireType == ACQUIRE_DO_TS && mMacProcessor->DoingMacro())
    return NAV_PRE_TS_RUNNING;
  if (idle && param->acquireType == ACQUIRE_DO_TS && mNav->StartedMacro())
    return mMacProcessor->IsResumable() ? NAV_PRE_TS_STOPPED : NAV_PAUSED;
  if (mNav->StartedMacro())
    return (idle ? NAV_SCRIPT_STOPPED : NAV_SCRIPT_RUNNING);
  mWinApp->AppendToLog("Warning: GetNavigatorState fell into an unexcluded muddle");
  return 0;  //??
}

// Change tools a little bit for calibrating IS offset
void CCameraMacroTools::CalibratingISoffset(bool ifCal)
{
  m_sbcMacro1.ShowWindow(ifCal ? SW_HIDE : SW_SHOW);
  m_sbcMacro1.EnableWindow(!ifCal);
  mDoingCalISO = ifCal;
  if (ifCal)
    m_butMacro1.SetWindowText("NextMag");
  else
    SetMacroLabels();
  Update();
}
