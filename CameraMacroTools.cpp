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
#include "CameraController.h"
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

enum {NO_NAV_RUNNING = 0, NAV_RUNNING_NO_SCRIPT_TS, NAV_PAUSED, NAV_TS_RUNNING,
  NAV_TS_STOPPED, NAV_PRE_TS_RUNNING, NAV_PRE_TS_STOPPED, NAV_SCRIPT_RUNNING,
  NAV_SCRIPT_STOPPED};

/////////////////////////////////////////////////////////////////////////////
// CCameraMacroTools dialog


CCameraMacroTools::CCameraMacroTools(CWnd* pParent /*=NULL*/)
  : CToolDlg(CCameraMacroTools::IDD, pParent)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CCameraMacroTools)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  mMacroNumber[0] = 0;
  mMacroNumber[1] = 1;
  mMacroNumber[2] = 2;
  mDoingTS = false;
  mUserStop = FALSE;
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
  DDX_Control(pDX, ID_CAMERA_VIEW, m_butView);
  DDX_Control(pDX, ID_CAMERA_TRIAL, m_butTrial);
  DDX_Control(pDX, ID_CAMERA_RECORD, m_butRecord);
  DDX_Control(pDX, ID_CAMERA_PARAMETERS, m_butSetup);
  DDX_Control(pDX, IDC_BUTMONTAGE, m_butMontage);
  DDX_Control(pDX, ID_CAMERA_FOCUS, m_butFocus);
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
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCameraMacroTools message handlers

BOOL CCameraMacroTools::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
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

  // Set the names of the camera modes
  CString *modeNames = mWinApp->GetModeNames();
  m_butView.SetWindowText(modeNames[0]);
  m_butFocus.SetWindowText(modeNames[1]);
  m_butTrial.SetWindowText(modeNames[2]);
  m_butRecord.SetWindowText(modeNames[3]);

  // Set the spin button controls
  m_sbcMacro1.SetRange(1, MAX_MACROS);
  m_sbcMacro2.SetRange(1, MAX_MACROS);
  m_sbcMacro3.SetRange(1, MAX_MACROS);
  UpdateSettings();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
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
  if (!mDoingCalISO)
    SetOneMacroLabel(0, IDC_BUTMACRO1);
  SetOneMacroLabel(1, IDC_BUTMACRO2);
  SetOneMacroLabel(2, IDC_BUTMACRO3);
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
  if (mWinApp->mMacroToolbar) {
    mWinApp->mMacroToolbar->SetOneMacroLabel(num);
    mWinApp->mMacroToolbar->SetLength(mWinApp->mMacroProcessor->GetNumToolButtons(),
      mWinApp->mMacroProcessor->GetToolButHeight());
  }
  if (mDoingTS)
    return;
  if (num == mMacroNumber[0] && !mDoingCalISO)
    SetOneMacroLabel(0, IDC_BUTMACRO1);
  if (num == mMacroNumber[1])
    SetOneMacroLabel(1, IDC_BUTMACRO2);
  if (num == mMacroNumber[2])
    SetOneMacroLabel(2, IDC_BUTMACRO3);
}

void CCameraMacroTools::UpdateSettings()
{
  m_sbcMacro1.SetPos(mMacroNumber[0] + 1);
  m_sbcMacro2.SetPos(mMacroNumber[1] + 1);
  m_sbcMacro3.SetPos(mMacroNumber[2] + 1);
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

// The Stop button calls camera and general error stop
void CCameraMacroTools::OnButstop() 
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
  int navState = GetNavigatorState();
  mWinApp->RestoreViewFocus();
  if (navState == NAV_TS_STOPPED || navState == NAV_PRE_TS_STOPPED)
    mNav->SetAcquireEnded(1);
  else if (navState == NAV_SCRIPT_STOPPED) {
    mMacProcessor->SetNonResumable();
    mNav->EndAcquireWithMessage();
  } else {
    mWinApp->mCamera->StopCapture(1);
    mUserStop = TRUE;
    mWinApp->ErrorOccurred(0);
    mUserStop = FALSE;
  }
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
  else if (mDoingTS) 
    mWinApp->mTSController->Resume();
  else if (navState == NAV_TS_RUNNING || navState == NAV_PRE_TS_RUNNING)
    mNav->SetAcquireEnded(1);
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
  mEnabledSearch = false;
  if (mWinApp->mCamera)
    camBusy = mWinApp->mCamera->CameraBusy();
  BOOL idle = !mWinApp->DoingTasks() && !(mNav && mNav->StartedMacro());
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
      m_butMacro1.EnableWindow(!camBusy);
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
  m_butStop.EnableWindow(mWinApp->DoingTasks() || camBusy || 
    mWinApp->mScope->GetMovingStage() || mWinApp->mCamera->DoingContinuousAcquire() ||
    navState == NAV_TS_STOPPED || navState == NAV_PRE_TS_STOPPED || 
    navState == NAV_SCRIPT_STOPPED);
  m_butSetup.EnableWindow((!mWinApp->DoingTasks() || mDoingCalISO) && !camBusy);

  // Set the End button
  if (mMacProcessor->DoingMacro() || mWinApp->DoingTiltSeries())
    SetDlgItemText(IDC_BUTEND, "End Loop");
  else if (postponed)
    SetDlgItemText(IDC_BUTEND, "End TS");
  else if (mNav && mNav->StartedMacro() && mMacProcessor->IsResumable())
    SetDlgItemText(IDC_BUTEND, "End Script");
  else if (!mWinApp->mMultiTSTasks->GetAssessingRange() && 
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
    m_butEnd.EnableWindow(mMacProcessor->DoingMacro() || mDoingCalISO || 
      (mEnabledSearch && idle) || (navState != NO_NAV_RUNNING && 
      navState != NAV_RUNNING_NO_SCRIPT_TS && !(navState == NAV_PAUSED && idle)) || 
      mWinApp->mMultiTSTasks->GetAssessingRange());
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
    SetDlgItemText(IDC_STATTOPLINE, "Camera && Tilt Series Controls");
    m_butMacro1.SetWindowText("1 Loop");
    m_butMacro2.SetWindowText("1 Step");
    m_butMacro3.SetWindowText("Backup");

  } else {
    SetDlgItemText(IDC_STATTOPLINE, "Camera && Script Controls");
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
  NavParams *param = mWinApp->GetNavParams();
  if (!mNav || !mNav->GetAcquiring())
    return NO_NAV_RUNNING;
  if (mNav->GetPausedAcquire())
    return NAV_PAUSED;
  if (!mNav->StartedMacro() && !mNav->GetStartedTS() && !idle)
    return NAV_RUNNING_NO_SCRIPT_TS;
  if (idle && !mNav->GetStartedTS() && (!mNav->StartedMacro() || 
    !mMacProcessor->IsResumable()))
    return NAV_PAUSED;
  if (mNav->GetStartedTS())
    return idle ? NAV_TS_STOPPED : NAV_TS_RUNNING;  // Should this test if postponed?
  if (param->acquireType == ACQUIRE_DO_TS && mMacProcessor->DoingMacro())
    return NAV_PRE_TS_RUNNING;
  if (idle && param->acquireType == ACQUIRE_DO_TS && mNav->StartedMacro())
    return mMacProcessor->IsResumable() ? NAV_PRE_TS_STOPPED : NAV_PAUSED;
  if (mNav->StartedMacro())
    return idle ? NAV_SCRIPT_STOPPED : NAV_SCRIPT_RUNNING;
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
