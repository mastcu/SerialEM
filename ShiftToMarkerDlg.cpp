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
#include "NavHelper.h"
#include "Utilities\KGetOne.h"


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
  DDX_Control(pDX, IDC_LIST_SAVED_SHIFTS, m_listSavedShifts);
}


BEGIN_MESSAGE_MAP(CShiftToMarkerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RAPPLY_ALL_AT_REG, OnRadioWhichToShift)
  ON_BN_CLICKED(IDC_RAPPLY_ALL_MAPS_AT_MAG, OnRadioWhichToShift)
  ON_BN_CLICKED(IDC_RAPPLY_IN_COHORT, OnRadioWhichToShift)
  ON_BN_CLICKED(IDC_BUT_REMOVE_SHIFT, OnButRemoveShift)
  ON_LBN_SELCHANGE(IDC_LIST_SAVED_SHIFTS, OnSelchangeListSavedShifts)
END_MESSAGE_MAP()


// CShiftToMarkerDlg message handlers
BOOL CShiftToMarkerDlg::OnInitDialog()
{
  CString str;
  int magLow, magHigh, camera = mWinApp->GetCurrentCamera();
  CBaseDlg::OnInitDialog();
  mMarkerShiftArr = mWinApp->mNavHelper->GetMarkerShiftArray();
  int tabs[3] = {6, 38, 42};
  m_listSavedShifts.SetTabStops(3, tabs);
  FillListBox();

  if (mOKtoShift) {
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
      m_iApplyToWhich = 0;
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
    if (m_iSaveType > 1 && !mBaseToMag)
      m_iSaveType = 1;
  } else {
    EnableDlgItem(IDC_RAPPLY_ALL_AT_REG, false);
    EnableDlgItem(IDC_RAPPLY_ALL_MAPS_AT_MAG, false);
    EnableDlgItem(IDC_RAPPLY_IN_COHORT, false);
    EnableDlgItem(IDC_STAT_MAP_MARKED, false);
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
  bool enable = m_iApplyToWhich != 0 && mOKtoShift && mFromMag != 0;
  EnableDlgItem(IDC_RNO_SAVING, enable);
  EnableDlgItem(IDC_RSAVE_NEW_SHIFT, enable);
  EnableDlgItem(IDC_RADD_TO_SAVED, enable && mBaseToMag != 0);
}

void CShiftToMarkerDlg::OnButRemoveShift()
{
  int ind = m_listSavedShifts.GetCurSel(), cam = mWinApp->GetCurrentCamera();
  BaseMarkerShift markShft;
  CString str;
  if (ind < 0 || ind >= mMarkerShiftArr->GetSize())
    return;
  markShft = mMarkerShiftArr->GetAt(ind);
  str.Format("Are you sure you want to remove shift of %.2f, %.2f from %dx to %dx?",
    markShft.shiftX, markShft.shiftY, MagForCamera(cam, markShft.fromMagInd),
    MagForCamera(cam, markShft.toMagInd));
  if (AfxMessageBox(str, MB_QUESTION) == IDYES) {
    mMarkerShiftArr->RemoveAt(ind);
    FillListBox();
  }
}


void CShiftToMarkerDlg::OnSelchangeListSavedShifts()
{
  EnableDlgItem(IDC_BUT_REMOVE_SHIFT, true);
}

void CShiftToMarkerDlg::FillListBox()
{
  int ind, cam = mWinApp->GetCurrentCamera();
  BaseMarkerShift markShft;
  CString str;
  m_listSavedShifts.ResetContent();
  for (ind = 0; ind < mMarkerShiftArr->GetSize(); ind++) {
    markShft = mMarkerShiftArr->GetAt(ind);
    str.Format("\t%d\t%d\t%.2f, %.2f", MagForCamera(cam, markShft.fromMagInd),
      MagForCamera(cam, markShft.toMagInd),
      markShft.shiftX, markShft.shiftY);
    m_listSavedShifts.AddString(str);
  }
  m_listSavedShifts.SetCurSel(-1);
  EnableDlgItem(IDC_BUT_REMOVE_SHIFT, false);
}
