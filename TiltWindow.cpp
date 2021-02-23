// TiltWindow.cpp:        Has controls for tilting and setting increments
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TiltWindow.h"
#include "Utilities\KGetOne.h"
#include "EMscope.h"
#include "CameraController.h"
#include "ShiftManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTiltWindow dialog


CTiltWindow::CTiltWindow(CWnd* pParent /*=NULL*/)
  : CToolDlg(CTiltWindow::IDD, pParent)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CTiltWindow)
  m_bCosineTilt = FALSE;
  m_sTiltText = _T("");
  //}}AFX_DATA_INIT
  mStageReady = true;
  mProgramReady = true;
  mInitialized = false;
}


void CTiltWindow::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CTiltWindow)
  DDX_Control(pDX, IDC_TILT_DELAY, m_butSetDelay);
  DDX_Control(pDX, IDC_COSINE_TILT, m_butCosine);
  DDX_Control(pDX, IDC_TILT_INCREMENT, m_butSetIncrement);
  DDX_Control(pDX, IDC_TILT_UP, m_butUp);
  DDX_Control(pDX, IDC_TILT_TO, m_butTo);
  DDX_Control(pDX, IDC_TILT_DOWN, m_butDown);
  DDX_Check(pDX, IDC_COSINE_TILT, m_bCosineTilt);
  DDX_Text(pDX, IDC_TILT_TEXT, m_sTiltText);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTiltWindow, CToolDlg)
  //{{AFX_MSG_MAP(CTiltWindow)
  ON_BN_CLICKED(IDC_TILT_UP, OnTiltUp)
  ON_BN_CLICKED(IDC_TILT_DOWN, OnTiltDown)
  ON_BN_CLICKED(IDC_TILT_TO, OnTiltTo)
  ON_BN_CLICKED(IDC_TILT_INCREMENT, OnTiltIncrement)
  ON_BN_CLICKED(IDC_COSINE_TILT, OnCosineTilt)
  ON_BN_CLICKED(IDC_TILT_DELAY, OnTiltDelay)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTiltWindow message handlers

void CTiltWindow::OnTiltUp() 
{
  mWinApp->mScope->TiltUp();
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  mWinApp->RestoreViewFocus();  
}

void CTiltWindow::OnTiltDown() 
{
  mWinApp->mScope->TiltDown();
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  mWinApp->RestoreViewFocus();  
}

void CTiltWindow::OnTiltTo() 
{
  float angle = 0.;
  if (KGetOneFloat("Tilt to: ", angle, 2)) {
    mWinApp->mScope->TiltTo((double)angle);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  }
  mWinApp->RestoreViewFocus();  
}

void CTiltWindow::OnTiltIncrement() 
{
  float increment = mWinApp->mScope->GetBaseIncrement();
  if (KGetOneFloat("Tilt increment (degrees):", increment, 2))
    mWinApp->mScope->SetIncrement(increment);
  mWinApp->RestoreViewFocus();  
}

void CTiltWindow::OnTiltDelay() 
{
  float delay = mWinApp->mShiftManager->GetTiltDelay();
  if (KGetOneFloat("Delay time after tilting by the basic increment (seconds): ", delay, 2))
    mWinApp->mShiftManager->SetTiltDelay(delay);
  mWinApp->RestoreViewFocus();  
}

void CTiltWindow::OnCosineTilt() 
{
  UpdateData(TRUE);
  mWinApp->mScope->SetCosineTilt(m_bCosineTilt);
  mWinApp->RestoreViewFocus();  
}

// Update the tilt angle display with the given value,
// enable the buttons if the stage is ready
void CTiltWindow::TiltUpdate(double inTilt, BOOL bReady)
{
  if (!mInitialized)
    return;

  if (inTilt != mTiltAngle) {
    m_sTiltText.Format("%.2f", inTilt);
    UpdateData(false);
    mTiltAngle = inTilt;
  }
  if (bReady && !mStageReady || !bReady && mStageReady) {
    mStageReady = bReady;
    EnableTiltButtons();
  }
}

BOOL CTiltWindow::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  
    // Set the font for the angle display
  CRect rect;
  CStatic *statTilt = (CStatic *)GetDlgItem(IDC_TILT_TEXT);
  statTilt->GetWindowRect(&rect);
  mFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  statTilt->SetFont(&mFont);
  mInitialized = true;
  TiltUpdate(-68.88, false);
  UpdateSettings();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CTiltWindow::UpdateEnables()
{
  mProgramReady = !(mWinApp->DoingTasks() || mWinApp->StartedTiltSeries());
  if (mWinApp->mCamera && mWinApp->mCamera->CameraBusy())
    mProgramReady = false;
  EnableTiltButtons();
  
  // Allow changes of increment and cosine if no complex task underway
  m_butSetIncrement.EnableWindow(!mWinApp->DoingComplexTasks());
  m_butSetDelay.EnableWindow(!mWinApp->DoingTasks());
  m_butCosine.EnableWindow(!mWinApp->DoingComplexTasks());
}

// Enable the tilt buttons if the stage is ready and the program is not doing
// any tasks
void CTiltWindow::EnableTiltButtons()
{
  BOOL bReady = mStageReady && mProgramReady;
  m_butUp.EnableWindow(bReady);
  m_butDown.EnableWindow(bReady);
  m_butTo.EnableWindow(bReady);
}

void CTiltWindow::UpdateSettings()
{
  m_bCosineTilt = mWinApp->mScope->GetCosineTilt();
  UpdateData(false);
  UpdateEnables();
}
