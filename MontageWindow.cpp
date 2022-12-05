// MontageWindow.cpp:     Has controls for montaging
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\MontageWindow.h"
#include "EMmontageController.h"
#include "CameraController.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static UINT sHideableIDs[] = {IDC_BTRIAL};

/////////////////////////////////////////////////////////////////////////////
// CMontageWindow dialog


CMontageWindow::CMontageWindow(CWnd* pParent /*=NULL*/)
  : CToolDlg(CMontageWindow::IDD, pParent)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CMontageWindow)
  m_bAdjustFocus = FALSE;
  m_bCorrectDrift = FALSE;
  m_iCurrentZ = 0;
  m_bShowOverview = FALSE;
	m_strOverviewBin = _T("");
	m_strScanBin = _T("");
	m_bShiftOverview = FALSE;
	m_bVerySloppy = FALSE;
	//}}AFX_DATA_INIT
}


void CMontageWindow::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CMontageWindow)
  DDX_Control(pDX, IDC_VERY_SLOPPY, m_butVerySloppy);
  DDX_Control(pDX, IDC_SHIFT_IN_OVERVIEW, m_butShiftOverview);
  DDX_Control(pDX, IDC_SPINVIEWBIN, m_sbcViewBin);
  DDX_Control(pDX, IDC_SPINSCANBIN, m_sbcScanBin);
  DDX_Control(pDX, IDC_ADJUSTFOCUS, m_butAdjustFocus);
  DDX_Control(pDX, IDC_CORRECTDRIFT, m_butCorrectDrift);
  DDX_Control(pDX, IDC_STATCURRENTZ, m_statCurrentZ);
  DDX_Control(pDX, IDC_SPINZ, m_sbcZ);
  DDX_Control(pDX, IDC_EDITZ, m_editCurrentZ);
  DDX_Control(pDX, IDC_BTRIAL, m_butTrial);
  DDX_Control(pDX, IDC_BSTART, m_butStart);
  DDX_Check(pDX, IDC_ADJUSTFOCUS, m_bAdjustFocus);
  DDX_Check(pDX, IDC_CORRECTDRIFT, m_bCorrectDrift);
  DDX_Text(pDX, IDC_EDITZ, m_iCurrentZ);
  DDV_MinMaxInt(pDX, m_iCurrentZ, 0, 1500);
  DDX_Check(pDX, IDC_SHOW_OVERVIEW, m_bShowOverview);
  DDX_Text(pDX, IDC_STATOVERVIEWBIN, m_strOverviewBin);
  DDX_Text(pDX, IDC_STATSCANBIN, m_strScanBin);
  DDX_Check(pDX, IDC_SHIFT_IN_OVERVIEW, m_bShiftOverview);
  DDX_Check(pDX, IDC_VERY_SLOPPY, m_bVerySloppy);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMontageWindow, CToolDlg)
  //{{AFX_MSG_MAP(CMontageWindow)
  ON_BN_CLICKED(IDC_BSTART, OnBstart)
  ON_BN_CLICKED(IDC_BTRIAL, OnBtrial)
  ON_BN_CLICKED(IDC_ADJUSTFOCUS, OnFocusOrDrift)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINZ, OnDeltaposSpinz)
  ON_EN_KILLFOCUS(IDC_EDITZ, OnKillfocusEditz)
  ON_BN_CLICKED(IDC_SHOW_OVERVIEW, OnShowOverview)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINSCANBIN, OnDeltaposSpinscanbin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINVIEWBIN, OnDeltaposSpinviewbin)
	ON_BN_CLICKED(IDC_SHIFT_IN_OVERVIEW, OnShiftInOverview)
  ON_BN_CLICKED(IDC_CORRECTDRIFT, OnFocusOrDrift)
	ON_BN_CLICKED(IDC_VERY_SLOPPY, OnVerySloppy)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMontageWindow message handlers

void CMontageWindow::OnBstart() 
{
  mWinApp->RestoreViewFocus();
  if (mMontageController->DoingMontage())
    return;
  mWinApp->StartMontageOrTrial(false);
}

void CMontageWindow::OnBtrial() 
{
  mWinApp->RestoreViewFocus();
  if (mMontageController->DoingMontage())
    return;
  mWinApp->StartMontageOrTrial(true);
}

void CMontageWindow::OnBstop() 
{
  mWinApp->RestoreViewFocus();
  if (!mMontageController->DoingMontage())
    return;
  mMontageController->StopMontage();
}

void CMontageWindow::OnFocusOrDrift() 
{
  mWinApp->RestoreViewFocus();
  // If doing a montage, restore previous value
  if (mMontageController->DoingMontage()) {
     UpdateData(false);
     return;
  }
  UpdateData(true);
  mParam->adjustFocus = m_bAdjustFocus;
  mParam->correctDrift = m_bCorrectDrift;
}

void CMontageWindow::OnDeltaposSpinz(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  if (mMontageController->DoingMontage() || newVal < 0 || newVal > 1500) {
    *pResult = 1;
    return;
  }
  mParam->zCurrent = newVal;
  m_iCurrentZ = newVal;
  UpdateData(false);
  *pResult = 0;
}

void CMontageWindow::OnKillfocusEditz() 
{
  mWinApp->RestoreViewFocus();
  if (mMontageController->DoingMontage()) {
    UpdateData(false);
    return;
  }
  UpdateData(true);
  m_sbcZ.SetPos(m_iCurrentZ);
  mParam->zCurrent = m_iCurrentZ;
}

void CMontageWindow::OnShowOverview() 
{
  UpdateData(true);
  mParam->showOverview = m_bShowOverview;
  mWinApp->RestoreViewFocus();
}

void CMontageWindow::OnShiftInOverview() 
{
  UpdateData(true);
  mParam->shiftInOverview = m_bShiftOverview;
  mWinApp->RestoreViewFocus();
}

void CMontageWindow::OnVerySloppy() 
{
  UpdateData(true);
  mParam->verySloppy = m_bVerySloppy;
  mWinApp->RestoreViewFocus();
}

void CMontageWindow::OnDeltaposSpinscanbin(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int oldVal = mParam->prescanBinning;
  int newVal = mParam->prescanBinning + pNMUpDown->iDelta;
  newVal = mMontageController->SetPrescanBinning(newVal, pNMUpDown->iDelta);
  if (newVal == oldVal) {
    *pResult = 1;
    return;
  }
  ManageBinning();
	*pResult = 0;
}

void CMontageWindow::OnDeltaposSpinviewbin(NMHDR* pNMHDR, LRESULT* pResult) 
{
  mWinApp->RestoreViewFocus();
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  if (newVal < 1 || newVal > mParam->maxOverviewBin) {
    *pResult = 1;
    return;
  }
  mParam->overviewBinning = newVal;
	
  ManageBinning();
	*pResult = 0;
}

// Returns come through to here
void CMontageWindow::OnOK() 
{
  m_butAdjustFocus.SetFocus();
  OnKillfocusEditz();
}

BOOL CMontageWindow::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  
  mMontageController = mWinApp->mMontageController;
  mParam = mWinApp->GetMontParam();
  m_sbcZ.SetRange(0,1500);
  m_bAdjustFocus = mParam->adjustFocus;
  m_bCorrectDrift = mParam->correctDrift;
  m_bShowOverview = mParam->showOverview;
  CRect rect;
  m_editCurrentZ.GetWindowRect(&rect);
  if (!mFont.m_hObject)
    mFont.CreateFont((rect.Height() - 4), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  m_editCurrentZ.SetFont(&mFont);

  UpdateSettings();
  ManageHideableItems(sHideableIDs, sizeof(sHideableIDs) / sizeof(UINT));
  mInitialized = true;

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CMontageWindow::UpdateSettings()
{
  if (!mInitialized)
    return;
  m_bAdjustFocus = mParam->adjustFocus;
  m_bCorrectDrift = mParam->correctDrift;
  m_bShowOverview = mParam->showOverview;
  m_bShiftOverview = mParam->shiftInOverview;
  m_bVerySloppy = mParam->verySloppy;
  m_sbcViewBin.SetRange(1, mParam->maxOverviewBin);
  m_sbcViewBin.SetPos(mParam->overviewBinning);
  Update();
}

void CMontageWindow::Update()
{
  if (!mInitialized)
    return;
  m_iCurrentZ = mParam->zCurrent;
  m_sbcZ.SetPos(m_iCurrentZ);
  BOOL bEnable = mWinApp->Montaging() && !mMontageController->DoingMontage() &&
    !mWinApp->DoingComplexTasks();
  BOOL noShift = mWinApp->Montaging() && mParam->skipCorrelations && mParam->useHqParams;
  m_statCurrentZ.EnableWindow(bEnable);
  m_editCurrentZ.EnableWindow(bEnable);
  m_sbcZ.EnableWindow(bEnable);
  
  bEnable = (mWinApp->Montaging() || !mWinApp->mStoreMRC) && !mWinApp->DoingTasks() &&
    (!mWinApp->mScope || !mWinApp->mScope->GetMovingStage());
  m_butStart.EnableWindow(bEnable && !mWinApp->StartedTiltSeries() &&
    !mWinApp->mCamera->CameraBusy());
  m_butTrial.EnableWindow(bEnable && !mWinApp->mCamera->CameraBusy());
  
  bEnable = !mMontageController->DoingMontage() && !mWinApp->DoingComplexTasks();
  m_butShiftOverview.EnableWindow(bEnable && !noShift);
  m_butVerySloppy.EnableWindow(bEnable && !noShift);
  m_butCorrectDrift.EnableWindow(bEnable && !mParam->moveStage && !noShift);
  m_butAdjustFocus.EnableWindow(bEnable && !mParam->moveStage);

  // If montaging is on, fix the prescan maximum and actual values
  if (mWinApp->Montaging()) {
    mMontageController->SetMaxPrescanBinning();
    mMontageController->SetPrescanBinning(mParam->prescanBinning, -1);
  }
  m_sbcScanBin.SetRange(mParam->binning, mParam->maxPrescanBin);
  m_sbcScanBin.SetPos(mParam->prescanBinning);
  ManageBinning();
}

void CMontageWindow::UpdateHiding(void)
{
  ManageHideableItems(sHideableIDs, sizeof(sHideableIDs) / sizeof(UINT));
}

void CMontageWindow::ManageBinning()
{
  int divideBin = 1;
  if (mWinApp->Montaging()) {
    int *activeList = mWinApp->GetActiveCameraList();
    int iCam = activeList[mMontageController->GetMontageActiveCamera(mParam)];
    CameraParameters *cam = mWinApp->GetCamParams() + iCam;
    divideBin = BinDivisorI(cam);
  }
  m_strScanBin.Format("Prescan %s", (LPCTSTR)mWinApp->BinningText(
    mParam->prescanBinning, divideBin));
  m_strOverviewBin.Format("Bin: Overview %d", mParam->overviewBinning);
  UpdateData(false);
}
