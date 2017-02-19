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
    mWinApp->UserRequestedCapture(4);
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
  CString label;
  CString *names = mWinApp->GetMacroNames();
  if (names[mMacroNumber[num]].IsEmpty())
    label.Format("Script %d", mMacroNumber[num] + 1);
  else
    label = names[mMacroNumber[num]];
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
  if (mDoingTS) {
    mWinApp->RestoreViewFocus();
    if (!mWinApp->DoingTiltSeries())
      mWinApp->mTSController->CommonResume(-1, 0);
  } else
    DoMacro(mMacroNumber[1]);
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
  mWinApp->RestoreViewFocus();
  mWinApp->mCamera->StopCapture(1);
  mUserStop = TRUE;
  mWinApp->ErrorOccurred(0);
  mUserStop = FALSE;
}
void CCameraMacroTools::OnButend() 
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
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
  else if (nav && !nav->StartedMacro() && nav->GetAcquiring())
    nav->SetAcquireEnded(true);
  else if (mEnabledSearch) {
    if (mWinApp->LowDoseMode() && mWinApp->mScope->GetLowDoseArea() != SEARCH_AREA)
      mWinApp->mScope->GotoLowDoseArea(SEARCH_AREA);
    mWinApp->UserRequestedCapture(VIEW_CONSET);
  } else
    mMacProcessor->SetNonResumable();
}

void CCameraMacroTools::OnButresume() 
{
  mWinApp->RestoreViewFocus();
  if (mWinApp->NavigatorStartedTS() && !mWinApp->StartedTiltSeries() && 
    mWinApp->mTSController->GetPostponed())
    mWinApp->mTSController->SetupTiltSeries(0);
  else if (mDoingTS) 
    mWinApp->mTSController->Resume();
  else
    mMacProcessor->Resume();
}

// Set the state of all buttons based on whether a task is being done, etc.
void CCameraMacroTools::Update()
{
  BOOL camBusy = true;
  mEnabledSearch = false;
  CNavigatorDlg *nav = mWinApp->mNavigator;
  if (mWinApp->mCamera)
    camBusy = mWinApp->mCamera->CameraBusy();
  BOOL idle = !mWinApp->DoingTasks() && !(nav && nav->StartedMacro());
  BOOL shotOK = mWinApp->UserAcquireOK();
  BOOL postponed = nav && nav->GetStartedTS() && mWinApp->mTSController->GetPostponed();
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
    m_butResume.EnableWindow(idle && (tsResumable || postponed));
    m_butEnd.EnableWindow(mWinApp->DoingTiltSeries() || postponed);
  } else {
    
    // If no tilt series
    // Enable buttons if the macros are non empty or being edited
    if (mDoingCalISO)
      m_butMacro1.EnableWindow(!camBusy);
    else
      m_butMacro1.EnableWindow(idle && mMacProcessor->MacroRunnable(mMacroNumber[0]));
    m_butMacro2.EnableWindow(idle && mMacProcessor->MacroRunnable(mMacroNumber[1]));
    m_butMacro3.EnableWindow(idle && mMacProcessor->MacroRunnable(mMacroNumber[2]));
    
    m_butResume.EnableWindow(mMacProcessor->IsResumable());
  }

  // Keep STOP enabled during continuous acquires: the press event gets lost in repeated 
  // enable/disables 
  m_butStop.EnableWindow(mWinApp->DoingTasks() || camBusy || 
    mWinApp->mScope->GetMovingStage() || mWinApp->mCamera->DoingContinuousAcquire());
  m_butSetup.EnableWindow((!mWinApp->DoingTasks() || mDoingCalISO) && !camBusy);

  if (mMacProcessor->DoingMacro() || mWinApp->DoingTiltSeries())
    SetDlgItemText(IDC_BUTEND, "End Loop");
  else if (postponed)
    SetDlgItemText(IDC_BUTEND, "End TS");
  else if (nav && nav->StartedMacro() && mMacProcessor->IsResumable())
    SetDlgItemText(IDC_BUTEND, "End Mac");
  else if (nav && !nav->StartedMacro() && nav->GetAcquiring() && !nav->GetStartedTS())
    SetDlgItemText(IDC_BUTEND, "End Nav");
  else if (!mWinApp->mMultiTSTasks->GetAssessingRange() && mWinApp->LowDoseMode()) {
    SetDlgItemText(IDC_BUTEND, "Search");
    mEnabledSearch = true;
  } else 
    SetDlgItemText(IDC_BUTEND, "End");
    
  if (!mWinApp->Montaging() || mWinApp->LowDoseMode()) {
    CString *modeNames = mWinApp->GetModeNames();
    SetDlgItemText(IDC_BUTMONTAGE, modeNames[4]);
  } else
    SetDlgItemText(IDC_BUTMONTAGE, "Montage");
  if (!mDoingTS)
    m_butEnd.EnableWindow(mMacProcessor->DoingMacro() || mDoingCalISO || 
      (mEnabledSearch && idle) ||
      (nav && nav->StartedMacro() && mMacProcessor->IsResumable()) ||
      (nav && !nav->StartedMacro() && nav->GetAcquiring()) || 
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
