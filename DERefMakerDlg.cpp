//////////////////////////////////////////////////////////////////////
// DERefMakerDlg.cpp:      Acquires dark and gain references to be used in the DE server
//
// Copyright (C) 2017 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "DERefMakerDlg.h"
#include "CameraController.h"

#define MAX_REPEATS 1000

// CDERefMakerDlg dialog

CDERefMakerDlg::CDERefMakerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CDERefMakerDlg::IDD, pParent)
  , m_iProcessingType(FALSE)
  , m_iReferenceType(0)
  , m_bUseHardwareBin(FALSE)
  , m_fExposureTime(0)
  , m_iNumRepeats(0)
  , m_fRefFPS(1.)
  , m_bUseRecHardwareROI(FALSE)
  , m_strRecArea(_T(""))
{

}

CDERefMakerDlg::~CDERefMakerDlg()
{
}

void CDERefMakerDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_RLINEAR_REF, m_iProcessingType);
  DDX_Radio(pDX, IDC_RDARK_REF, m_iReferenceType);
  DDX_Check(pDX, IDC_USE_HARDWARE_BIN, m_bUseHardwareBin);
  DDX_Text(pDX, IDC_EDIT_REF_EXPOSURE, m_fExposureTime);
  DDV_MinMaxFloat(pDX, m_fExposureTime, 0.05f, 50.);
  DDX_Control(pDX, IDC_SPIN_NUM_REPEATS, m_spinNumRepeats);
  DDX_Text(pDX, IDC_EDIT_NUM_REPEATS, m_iNumRepeats);
  DDV_MinMaxInt(pDX, m_iNumRepeats, 1, MAX_REPEATS);
  DDX_Control(pDX, IDC_RDARK_REF, m_butDarkRef);
  DDX_Text(pDX, IDC_EDIT_REF_FPS, m_fRefFPS);
  DDV_MinMaxFloat(pDX, m_fRefFPS, 1., 400.);
  DDX_Control(pDX, IDC_EDIT_REF_FPS, m_editRefFPS);
  DDX_Check(pDX, IDC_USE_REC_HARDWARE_ROI, m_bUseRecHardwareROI);
  DDX_Control(pDX, IDC_USE_REC_HARDWARE_ROI, m_butUseRecHardwareROI);
  DDX_Text(pDX, IDC_STAT_REC_AREA, m_strRecArea);
}


BEGIN_MESSAGE_MAP(CDERefMakerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RLINEAR_REF, OnProcessingType)
  ON_BN_CLICKED(IDC_RPRE_COUNTING, OnProcessingType)
  ON_BN_CLICKED(IDC_RPOST_COUNTING, OnProcessingType)
  ON_BN_CLICKED(IDC_RSUPER_RES_REF, OnProcessingType)
  ON_BN_CLICKED(IDC_RDARK_REF, OnReferenceType)
  ON_BN_CLICKED(IDC_RGAIN_REF, OnReferenceType)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_REPEATS, OnDeltaposSpinNumRepeats)
  ON_EN_KILLFOCUS(IDC_EDIT_REF_FPS, OnKillfocusEditRefFps)
  ON_BN_CLICKED(IDC_USE_HARDWARE_BIN, OnHardwareChange)
  ON_BN_CLICKED(IDC_USE_REC_HARDWARE_ROI, OnHardwareChange)
END_MESSAGE_MAP()


// CDERefMakerDlg message handlers

// Initialize dialog
BOOL CDERefMakerDlg::OnInitDialog()
{
  mRecSet = mWinApp->GetConSets() + RECORD_CONSET;
  CBaseDlg::OnInitDialog();
  bool hasHardwareROI = (mCamParams->CamFlags & DE_HAS_HARDWARE_ROI) != 0;
  ShowDlgItem(IDC_USE_HARDWARE_BIN, (mCamParams->CamFlags & DE_HAS_HARDWARE_BIN) != 0);
  ShowDlgItem(IDC_USE_REC_HARDWARE_ROI, hasHardwareROI);
  mEnableROI = hasHardwareROI && (mRecSet->left > 0 || mRecSet->right < mCamParams->sizeX 
    || mRecSet->top > 0 || mRecSet->bottom < mCamParams->sizeY);
  m_butUseRecHardwareROI.EnableWindow(mEnableROI);
  if (!mEnableROI && hasHardwareROI)
    m_strRecArea = "(Select a Record subarea first for hardware ROI)";
  B3DCLAMP(m_iProcessingType, 0, 3);
  B3DCLAMP(m_iReferenceType, 0, 1);
  if (!(mCamParams->CamFlags & DE_CAM_CAN_COUNT)) {
    ShowDlgItem(IDC_RLINEAR_REF, false);
    ShowDlgItem(IDC_RPRE_COUNTING, false);
    ShowDlgItem(IDC_RPOST_COUNTING, false);
    ShowDlgItem(IDC_RSUPER_RES_REF, false);
    ShowDlgItem(IDC_STAT_PROC_TYPE, false);
    m_iProcessingType = 0;
  }
  m_spinNumRepeats.SetRange(0, 10000);
  m_spinNumRepeats.SetPos(5000);
  LoadListItemsToDialog();
  OnHardwareChange();

  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// Constrain the reference type for post-counting gain and move array variables into the
// dialog elements
void CDERefMakerDlg::LoadListItemsToDialog(void)
{
  if (m_iProcessingType >= 2)
    m_iReferenceType = 1;
  m_butDarkRef.EnableWindow(m_iProcessingType < 2);
  mCurListInd = 2 * m_iProcessingType + (1 - m_iReferenceType);
  m_fExposureTime = mExposureList[mCurListInd];
  m_iNumRepeats = mRepeatList[mCurListInd];
  B3DCLAMP(m_fExposureTime, 0.05f, 50.f);
  B3DCLAMP(m_iNumRepeats, 1, MAX_REPEATS);
  if (m_iProcessingType && mCamParams->DE_CountingFPS <= 0)
    mCamParams->DE_CountingFPS = mCamParams->DE_FramesPerSec;
  m_fRefFPS = m_iProcessingType ?mCamParams->DE_CountingFPS : mCamParams->DE_FramesPerSec;
  UpdateData(false);
}

// When starting, only the exposure and repeats need to be moved into the arrays
void CDERefMakerDlg::OnOK() 
{
  UpdateData(true);
  mExposureList[mCurListInd] = m_fExposureTime;
  mRepeatList[mCurListInd] = m_iNumRepeats;
  if (m_iProcessingType)
    mCamParams->DE_CountingFPS = m_fRefFPS;
  else
    mCamParams->DE_FramesPerSec = m_fRefFPS;
  CBaseDlg::OnOK();
}

// Not needed...
void CDERefMakerDlg::OnCancel() 
{
  CBaseDlg::OnCancel();
}

// Both radio buttons currently do the same thing: unload exposure/repeats into arrays
// and load values for new selection
void CDERefMakerDlg::OnProcessingType()
{
  OnReferenceType();
}

void CDERefMakerDlg::OnReferenceType()
{
  UpdateData(true);
  mExposureList[mCurListInd] = m_fExposureTime;
  mRepeatList[mCurListInd] = m_iNumRepeats;
  LoadListItemsToDialog();
}

// Change the number of repeats
void CDERefMakerDlg::OnDeltaposSpinNumRepeats(NMHDR *pNMHDR, LRESULT *pResult)
{
  UpdateData(true);
  if (NewSpinnerValue(pNMHDR, pResult, m_iNumRepeats, 1, MAX_REPEATS, m_iNumRepeats))
    return;
  UpdateData(false);
}

void CDERefMakerDlg::OnKillfocusEditRefFps()
{
  int lastProcessing = m_iProcessingType;
  UpdateData(true);
  if (lastProcessing)
    mCamParams->DE_CountingFPS = m_fRefFPS;
  else
    mCamParams->DE_FramesPerSec = m_fRefFPS;
}

// Format the line showing size of hardware ROI reference
void CDERefMakerDlg::OnHardwareChange()
{
  UpdateData(true);
  if (!mEnableROI)
    return;
  int bin = m_bUseHardwareBin ? 2 : 1;
  if (!m_bUseRecHardwareROI) {
    m_strRecArea = "";
  } else {
    m_strRecArea.Format("(%d x %d%s)", (mRecSet->right - mRecSet->left) / bin, 
      (mRecSet->bottom - mRecSet->top) / bin, m_bUseHardwareBin ? " binned pixels" : "");
  }
  UpdateData(false);
}
