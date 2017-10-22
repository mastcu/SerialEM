// MultiShotDlg.cpp:      Dialog for multiple Record parameters
//
// Copyright (C) 2017 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "MultiShotDlg.h"


// CMultiShotDlg dialog

IMPLEMENT_DYNAMIC(CMultiShotDlg, CBaseDlg)

CMultiShotDlg::CMultiShotDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CMultiShotDlg::IDD, pParent)
  , m_strNumShots(_T(""))
  , m_fSpokeDist(0)
  , m_iEarlyFrames(0)
  , m_bSaveRecord(FALSE)
  , m_bUseIllumArea(FALSE)
  , m_fBeamDiam(0)
  , m_iEarlyReturn(0)
  , m_fExtraDelay(0)
{

}

CMultiShotDlg::~CMultiShotDlg()
{
}

void CMultiShotDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_NUM_SHOTS, m_strNumShots);
  DDX_Control(pDX, IDC_SPIN_NUM_SHOTS, m_sbcNumShots);
  DDX_Text(pDX, IDC_EDIT_SPOKE_DIST, m_fSpokeDist);
  DDV_MinMaxFloat(pDX, m_fSpokeDist, 0.05f, 50.);
  DDX_Control(pDX, IDC_EDIT_EARLY_FRAMES, m_editEarlyFrames);
  DDX_Text(pDX, IDC_EDIT_EARLY_FRAMES, m_iEarlyFrames);
  DDV_MinMaxInt(pDX, m_iEarlyFrames, -1, 999);
  DDX_Check(pDX, IDC_CHECK_SAVE_RECORD, m_bSaveRecord);
  DDX_Control(pDX, IDC_CHECK_USE_ILLUM_AREA, m_butUseIllumArea);
  DDX_Check(pDX, IDC_CHECK_USE_ILLUM_AREA, m_bUseIllumArea);
  DDX_Control(pDX, IDC_STAT_BEAM_DIAM, m_statBeamDiam);
  DDX_Control(pDX, IDC_STAT_BEAM_UM, m_statBeamMicrons);
  DDX_Control(pDX, IDC_EDIT_BEAM_DIAM, m_editBeamDiam);
  DDX_Text(pDX, IDC_EDIT_BEAM_DIAM, m_fBeamDiam);
  DDV_MinMaxFloat(pDX, m_fBeamDiam, 0.05f, 50.);
  DDX_Radio(pDX, IDC_RNO_CENTER, m_iCenterShot);
  DDX_Control(pDX, IDC_STAT_NUM_EARLY, m_statNumEarly);
  DDX_Control(pDX, IDC_RNO_EARLY, m_butNoEarly);
  DDX_Radio(pDX, IDC_RNO_EARLY, m_iEarlyReturn);
  DDX_Text(pDX, IDC_EDIT_EXTRA_DELAY, m_fExtraDelay);
  DDV_MinMaxFloat(pDX, m_fExtraDelay, 0., 100.);
  DDX_Control(pDX, IDC_CHECK_SAVE_RECORD, m_butSaveRecord);
}


BEGIN_MESSAGE_MAP(CMultiShotDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_SHOTS, OnDeltaposSpinNumShots)
  ON_BN_CLICKED(IDC_RNO_CENTER, OnRnoCenter)
  ON_BN_CLICKED(IDC_RCENTER_BEFORE, OnRnoCenter)
  ON_BN_CLICKED(IDC_RCENTER_AFTER, OnRnoCenter)
  ON_EN_KILLFOCUS(IDC_EDIT_SPOKE_DIST, OnKillfocusEditSpokeDist)
  ON_EN_KILLFOCUS(IDC_EDIT_BEAM_DIAM, OnKillfocusEditBeamDiam)
  ON_BN_CLICKED(IDC_CHECK_USE_ILLUM_AREA, OnCheckUseIllumArea)
  ON_BN_CLICKED(IDC_RNO_EARLY, OnRnoEarly)
  ON_BN_CLICKED(IDC_RLAST_EARLY, OnRnoEarly)
  ON_BN_CLICKED(IDC_RALL_EARLY, OnRnoEarly)
  ON_EN_KILLFOCUS(IDC_EDIT_EARLY_FRAMES, OnKillfocusEditEarlyFrames)
END_MESSAGE_MAP()


// CMultiShotDlg message handlers

// Initialize dialog
BOOL CMultiShotDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  // Save original parameters and transfer them to interface elements
  mActiveParams = mWinApp->mNavHelper->GetMultiShotParams();
  mSavedParams = *mActiveParams;
  m_iEarlyReturn = mActiveParams->doEarlyReturn;
  m_iEarlyFrames = mActiveParams->numEarlyFrames;
  m_iCenterShot = mActiveParams->doCenter < 0 ? 2 : mActiveParams->doCenter;
  m_fBeamDiam = mActiveParams->beamDiam;
  m_fSpokeDist = mActiveParams->spokeRad;
  m_bSaveRecord = mActiveParams->saveRecord;
  m_fExtraDelay = mActiveParams->extraDelay;
  m_bUseIllumArea = mActiveParams->useIllumArea;
  m_sbcNumShots.SetRange(0, 100);
  m_sbcNumShots.SetPos(50);

  // Hide Early return items if no K2 in system
  ShowDlgItem(IDC_STAT_EARLY_GROUP, mCanReturnEarly);
  ShowDlgItem(IDC_RNO_EARLY, mCanReturnEarly);
  ShowDlgItem(IDC_RLAST_EARLY, mCanReturnEarly);
  ShowDlgItem(IDC_RALL_EARLY, mCanReturnEarly);
  ShowDlgItem(IDC_STAT_NUM_EARLY, mCanReturnEarly);
  ShowDlgItem(IDC_EDIT_EARLY_FRAMES, mCanReturnEarly);
  ManageEnables();

  // Hide "Use illuminated area" if not present
  m_butUseIllumArea.ShowWindow(mHasIlluminatedArea ? SW_SHOW : SW_HIDE);
  m_strNumShots.Format("%d", mActiveParams->numShots);
  UpdateData(false);
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// OK, Cancel, handling returns in text fields
void CMultiShotDlg::OnOK() 
{
  UpdateData(true);
  UpdateAndUseMSparams();
  CBaseDlg::OnOK();
}

void CMultiShotDlg::OnCancel() 
{
  *mActiveParams = mSavedParams;
  mWinApp->mMainView->DrawImage();
  CBaseDlg::OnCancel();
}

BOOL CMultiShotDlg::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Number of shots spinner
void CMultiShotDlg::OnDeltaposSpinNumShots(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mActiveParams->numShots, 2, 12, 
    mActiveParams->numShots))
    return;
  UpdateData(true);
  m_strNumShots.Format("%d", mActiveParams->numShots);
  UpdateData(false);
  UpdateAndUseMSparams();
  *pResult = 0;
}

// Center shot radio button
void CMultiShotDlg::OnRnoCenter()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Off-center distance
void CMultiShotDlg::OnKillfocusEditSpokeDist()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// New beam diameter
void CMultiShotDlg::OnKillfocusEditBeamDiam()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Whether to use illuminated area
void CMultiShotDlg::OnCheckUseIllumArea()
{
  UpdateData(true);
  ManageEnables();
  UpdateAndUseMSparams();
}

// Early return radio button
void CMultiShotDlg::OnRnoEarly()
{
  UpdateData(true);
  ManageEnables();
}

// Early return frames: manage 
void CMultiShotDlg::OnKillfocusEditEarlyFrames()
{
  UpdateData(true);
  ManageEnables();
}

// Unload dialog parameters into the structure and redraw
void CMultiShotDlg::UpdateAndUseMSparams(void)
{
  mActiveParams->doEarlyReturn = m_iEarlyReturn;
  mActiveParams->numEarlyFrames = m_iEarlyFrames;
  mActiveParams->doCenter = m_iCenterShot > 1 ? -1 : m_iCenterShot;
  mActiveParams->beamDiam = m_fBeamDiam;
  mActiveParams->spokeRad = m_fSpokeDist;
  mActiveParams->saveRecord = m_bSaveRecord;
  mActiveParams->extraDelay = m_fExtraDelay;
  mActiveParams->useIllumArea = m_bUseIllumArea;
  mWinApp->mMainView->DrawImage();
}

// Getting lazy: one function for multiple cases
void CMultiShotDlg::ManageEnables(void)
{
  bool enable = !(mHasIlluminatedArea && m_bUseIllumArea && mWinApp->LowDoseMode());
  m_statBeamDiam.EnableWindow(enable);
  m_statBeamMicrons.EnableWindow(enable);
  m_editBeamDiam.EnableWindow(enable);
  m_statNumEarly.EnableWindow(m_iEarlyReturn > 0);
  m_editEarlyFrames.EnableWindow(m_iEarlyReturn > 0);
  m_butSaveRecord.EnableWindow(m_iEarlyReturn == 0 || m_iEarlyFrames != 0);
}
