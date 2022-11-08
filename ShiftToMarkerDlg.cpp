// ShiftToMarkerDlg.cpp : choose what items to shift and whether to save shifts
//
// Copyright (C) 2021 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "ShiftToMarkerDlg.h"


// CShiftToMarkerDlg dialog


CShiftToMarkerDlg::CShiftToMarkerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_SHIFT_TO_MARKER, pParent)
  , m_strMarkerShift(_T(""))
  , m_strWhatShifts(_T(""))
  , m_iApplyToWhich(FALSE)
  , m_iSaveType(0)
{

}

CShiftToMarkerDlg::~CShiftToMarkerDlg()
{
}

void CShiftToMarkerDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_MARKER_SHIFT, m_strMarkerShift);
  DDX_Text(pDX, IDC_STAT_WHAT_SHIFTS, m_strWhatShifts);
  DDX_Radio(pDX, IDC_RAPPLY_ALL_AT_REG, m_iApplyToWhich);
  DDX_Radio(pDX, IDC_RNO_SAVING, m_iSaveType);
  DDX_Control(pDX, IDC_STAT_MAP_MARKED, m_statMapMarked);
}


BEGIN_MESSAGE_MAP(CShiftToMarkerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RAPPLY_ALL_AT_REG, OnRadioWhichToShift)
  ON_BN_CLICKED(IDC_RAPPLY_ALL_MAPS_AT_MAG, OnRadioWhichToShift)
  ON_BN_CLICKED(IDC_RAPPLY_IN_COHORT, OnRadioWhichToShift)
END_MESSAGE_MAP()


// CShiftToMarkerDlg message handlers
BOOL CShiftToMarkerDlg::OnInitDialog()
{
  CString str;
  int magLow, magHigh, camera = mWinApp->GetCurrentCamera();
  CBaseDlg::OnInitDialog();

  // Set up text of which to apply buttons depending on whether a mag is defined
  if (mFromMag) {
    magLow = MagForCamera(camera, mFromMag);
    str.Format("All maps at %dx and items marked on them", magLow);
    SetDlgItemText(IDC_RAPPLY_ALL_MAPS_AT_MAG, str);
    str.Format("Only maps at %dx and marked items that %s", magLow, mMapWasShifted ?
      "were shifted along with" : "have not been shifted before");
    SetDlgItemText(IDC_RAPPLY_IN_COHORT, str);
    
  } else {
    SetDlgItemText(IDC_RAPPLY_ALL_MAPS_AT_MAG,
      "All maps at one magnification: insufficient information available");
    SetDlgItemText(IDC_RAPPLY_IN_COHORT, mMapWasShifted ? "Only items shifted together"
      " at one magnification: insufficient information available" : "Only items at one "
      "magnification not previously shifted:");
    SetDlgItemText(IDC_STAT_MAP_MARKED, " insufficient information available");
    EnableDlgItem(IDC_RAPPLY_ALL_MAPS_AT_MAG, false);
    EnableDlgItem(IDC_RAPPLY_IN_COHORT, false);
    EnableDlgItem(IDC_STAT_MAP_MARKED, false);
  }
  m_statMapMarked.ShowWindow(mMapWasShifted || !mFromMag ? SW_SHOW : SW_HIDE);

  // And choices for saving depend on having both mags
  if (mFromMag && mToMag) {
    magHigh = MagForCamera(camera, mToMag);
    str.Format("Save shift to apply again from %dx to %dx or other second mag", magLow, 
      magHigh);
    SetDlgItemText(IDC_RSAVE_NEW_SHIFT, str);
    str.Format("Add to saved shift from %dx to %dx%s", magLow,
      mBaseToMag ? MagForCamera(camera, mBaseToMag) : magHigh,
      mBaseToMag && mBaseToMag != mToMag ? " (non-matching second mag)" : "");
    SetDlgItemText(IDC_RADD_TO_SAVED, str);
  } else {
    SetDlgItemText(IDC_RSAVE_NEW_SHIFT, 
      "Save shift to apply again: insufficient information available");
    SetDlgItemText(IDC_RADD_TO_SAVED,
      "Add to saved shift: insufficient information available");
    
  }
  UpdateData(false);
  OnRadioWhichToShift();
  return TRUE;
}

void CShiftToMarkerDlg::OnOK()
{
  UpdateData(true);
  CBaseDlg::OnOK();
}

void CShiftToMarkerDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}


void CShiftToMarkerDlg::OnRadioWhichToShift()
{
  UpdateData(true);
  bool enable = m_iApplyToWhich != 0;
  EnableDlgItem(IDC_RNO_SAVING, enable);
  EnableDlgItem(IDC_RSAVE_NEW_SHIFT, enable);
  EnableDlgItem(IDC_RADD_TO_SAVED, enable && mBaseToMag != 0);
}
