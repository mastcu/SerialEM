// CtfAcquireParamDlg.cpp : Sets parameters for images and defocus handling in CTF tuning
//
// Copyright (C) 2025 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "CtfAcquireParamDlg.h"
#include "CameraController.h"


// CCtfAcquireParamDlg dialog


CCtfAcquireParamDlg::CCtfAcquireParamDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_CTF_ACQ_PARAMS, pParent)
  , m_bSetExposure(FALSE)
  , m_bSetDrift(FALSE)
  , m_fExposure(0.1f)
  , m_fDrift(0)
  , m_bSetBinning(FALSE)
  , m_iBinning(1)
  , m_bUseFullField(FALSE)
  , m_bUseFocusInLD(FALSE)
  , m_bAlignFrames(FALSE)
  , m_iNumFrames(3)
  , m_fMinFocus(-0.1f)
  , m_fMinAdd(0.2f)
  , m_bUseAliSet(FALSE)
  , m_iAlignSet(1)
{

}

CCtfAcquireParamDlg::~CCtfAcquireParamDlg()
{
}

void CCtfAcquireParamDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_CTF_EXPOSURE, m_bSetExposure);
  DDX_Text(pDX, IDC_EDIT_CTF_EXPOSURE, m_fExposure);
  DDV_MinMaxFloat(pDX, m_fExposure, 0.1f, 10.f);
  DDX_Check(pDX, IDC_CHECKCTF_DRIFT, m_bSetDrift);
  DDX_Text(pDX, IDC_EDITCTF_DRIFT, m_fDrift);
  DDV_MinMaxFloat(pDX, m_fDrift, 0, 1);
  DDX_Check(pDX, IDC_CHECK_CTF_BINNING, m_bSetBinning);
  DDX_Text(pDX, IDC_EDIT_CTF_BINNING, m_iBinning);
  DDV_MinMaxInt(pDX, m_iBinning, 1, 8);
  DDX_Check(pDX, IDC_CHECK_CTF_FULL_FIELD, m_bUseFullField);
  DDX_Check(pDX, IDC_CHECK_USE_FOCUS_IN_LD, m_bUseFocusInLD);
  DDX_Check(pDX, IDC_CHECKCTF_ALIGN_FRAMES, m_bAlignFrames);
  DDX_Text(pDX, IDC_EDIT_CTF_NUM_FRAMES, m_iNumFrames);
  DDV_MinMaxInt(pDX, m_iNumFrames, 3, 20);
  DDX_Text(pDX, IDC_EDIT_CTF_MIN_FOCUS, m_fMinFocus);
  DDV_MinMaxFloat(pDX, m_fMinFocus, -10.f, -0.1f);
  DDX_Text(pDX, IDC_EDIT_CTF_MIN_ADD, m_fMinAdd);
  DDV_MinMaxFloat(pDX, m_fMinAdd, 0.2f, 10.f);
  DDX_Check(pDX, IDC_CHECK_USE_ALI_SET, m_bUseAliSet);
  DDV_MinMaxInt(pDX, m_iNumFrames, 3, 20);
  DDX_Text(pDX, IDC_EDIT_ALIGN_SET, m_iAlignSet);
  DDV_MinMaxInt(pDX, m_iAlignSet, 1, 
    (int)(mWinApp->mCamera->GetFrameAliParams())->GetSize());
  
}


BEGIN_MESSAGE_MAP(CCtfAcquireParamDlg, CDialog)
  ON_BN_CLICKED(IDC_CHECK_CTF_EXPOSURE, OnCheckCtfExposure)
  ON_BN_CLICKED(IDC_CHECKCTF_DRIFT, OnCheckCtfDrift)
  ON_EN_KILLFOCUS(IDC_EDIT_CTF_EXPOSURE, OnEnKillfocusEditCtfExposure)
  ON_EN_KILLFOCUS(IDC_EDITCTF_DRIFT, OnEnKillfocusEditCtfExposure)
  ON_EN_KILLFOCUS(IDC_EDIT_CTF_BINNING, OnEnKillfocusEditCtfExposure)
  ON_EN_KILLFOCUS(IDC_EDIT_CTF_NUM_FRAMES, OnEnKillfocusEditCtfExposure)
  ON_EN_KILLFOCUS(IDC_EDIT_CTF_MIN_FOCUS, OnEnKillfocusEditCtfExposure)
  ON_EN_KILLFOCUS(IDC_EDIT_CTF_MIN_ADD, OnEnKillfocusEditCtfExposure)
  ON_BN_CLICKED(IDC_CHECK_CTF_BINNING, OnCheckCtfBinning)
  ON_BN_CLICKED(IDC_CHECKCTF_ALIGN_FRAMES, OnCheckctfAlignFrames)
  ON_BN_CLICKED(IDC_CHECK_USE_ALI_SET, OnCheckUseAliSet)
  ON_EN_KILLFOCUS(IDC_EDIT_ALIGN_SET, OnEnKillfocusEditAlignSet)
END_MESSAGE_MAP()


// CCtfAcquireParamDlg message handlers
BOOL CCtfAcquireParamDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  // The caller loads and unloads the dialog values
  ManageEnables();
  ShowAlignSetName();
  return TRUE;
}

void CCtfAcquireParamDlg::OnOK()
{
  UpdateData(true);
  CBaseDlg::OnOK();
}

void CCtfAcquireParamDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}

void CCtfAcquireParamDlg::OnCheckCtfExposure()
{
  UpdateData(true);
  ManageEnables();
}


void CCtfAcquireParamDlg::OnCheckCtfDrift()
{
  UpdateData(true);
  ManageEnables();
}


void CCtfAcquireParamDlg::OnEnKillfocusEditCtfExposure()
{
  UpdateData(true);
}


void CCtfAcquireParamDlg::OnCheckCtfBinning()
{
  UpdateData(true);
  ManageEnables();
}


void CCtfAcquireParamDlg::OnCheckctfAlignFrames()
{
  UpdateData(true);
  ManageEnables();
}

void CCtfAcquireParamDlg::OnCheckUseAliSet()
{
  UpdateData(true);
  ManageEnables();
}

void CCtfAcquireParamDlg::OnEnKillfocusEditAlignSet()
{
  UpdateData(true);
  ShowAlignSetName();
}

// Show the name of the alignment parameter set if any
void CCtfAcquireParamDlg::ShowAlignSetName()
{
  CArray<FrameAliParams, FrameAliParams> *faParams =
    mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams param;
  if (!faParams->GetSize()) {
    SetDlgItemText(IDC_STAT_ALI_SET_NAME, "");
    return;
  }
  if (m_iAlignSet > (int)faParams->GetSize() || m_iAlignSet < 1) {
    B3DCLAMP(m_iAlignSet, 1, (int)faParams->GetSize());
    UpdateData(false);
  }
  param = faParams->GetAt(m_iAlignSet - 1);
  SetDlgItemText(IDC_STAT_ALI_SET_NAME, param.name);
}

// Enable items based on on the checkboxes
void CCtfAcquireParamDlg::ManageEnables()
{
  EnableDlgItem(IDC_EDIT_CTF_EXPOSURE, m_bSetExposure);
  EnableDlgItem(IDC_STAT_EXP_SEC, m_bSetExposure);
  EnableDlgItem(IDC_EDITCTF_DRIFT, m_bSetDrift);
  EnableDlgItem(IDC_STAT_DRIFT_SEC, m_bSetDrift);
  EnableDlgItem(IDC_EDIT_CTF_BINNING, m_bSetBinning);
  EnableDlgItem(IDC_EDIT_CTF_NUM_FRAMES, m_bAlignFrames);
  EnableDlgItem(IDC_STAT_FRAMES, m_bAlignFrames);
  EnableDlgItem(IDC_CHECK_USE_ALI_SET, m_bAlignFrames);
  EnableDlgItem(IDC_EDIT_ALIGN_SET, m_bAlignFrames && m_bUseAliSet);
  EnableDlgItem(IDC_STAT_ALI_SET_NAME, m_bAlignFrames && m_bUseAliSet);
}
