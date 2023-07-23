// MultiCombinerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "NavHelper.h"
#include "MultiHoleCombiner.h"
#include "MultiCombinerDlg.h"

#define MAX_HOLE_THRESH 20

// CMultiCombinerDlg dialog


CMultiCombinerDlg::CMultiCombinerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MULTI_COMBINER, pParent)
  , m_bDisplayMultiShot(FALSE)
  , m_bTurnOffOutside(FALSE)
  , m_strHoleThresh(_T(""))
  , m_bRemovePoints(FALSE)
  , m_bTurnOffCombined(FALSE)
  , m_bSkipAveraging(FALSE)
{
  mNonModal = true;
}

CMultiCombinerDlg::~CMultiCombinerDlg()
{
}

void CMultiCombinerDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_DISPLAY_MULTI, m_bDisplayMultiShot);
  DDX_Control(pDX, IDC_BUT_UNDO_COMBINE, m_butUndoCombine);
  DDX_Radio(pDX, IDC_RON_IMAGE, m_iCombineType);
  DDX_Control(pDX, IDC_BUT_COMBINE_PTS, m_butCombinePts);
  DDX_Control(pDX, IDC_CHECK_REMOVE_OUTSIDE, m_butTurnOffOutside);
  DDX_Check(pDX, IDC_CHECK_REMOVE_OUTSIDE, m_bTurnOffOutside);
  DDX_Text(pDX, IDC_STAT_HOLE_THRESH, m_strHoleThresh);
  DDX_Control(pDX, IDC_SPIN_HOLE_THRESH, m_sbcHoleThresh);
  DDX_Control(pDX, IDC_CHECK_REMOVE_POINTS, m_butRemovePoints);
  DDX_Check(pDX, IDC_CHECK_REMOVE_POINTS, m_bRemovePoints);
  DDX_Control(pDX, IDC_CHECK_TURN_OFF_COMBINED, m_butTurnOffCombined);
  DDX_Check(pDX, IDC_CHECK_TURN_OFF_COMBINED, m_bTurnOffCombined);
  DDX_Check(pDX, IDC_CHECK_SKIP_AVERAGING, m_bSkipAveraging);
}


BEGIN_MESSAGE_MAP(CMultiCombinerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RON_IMAGE, OnCombineType)
  ON_BN_CLICKED(IDC_RIN_POLYGON, OnCombineType)
  ON_BN_CLICKED(IDC_RSAME_GROUP, OnCombineType)
  ON_BN_CLICKED(IDC_BUT_UNDO_COMBINE, OnUndoCombine)
  ON_BN_CLICKED(IDC_CHECK_DISPLAY_MULTI, OnCheckDisplayMulti)
  ON_BN_CLICKED(IDC_BUT_COMBINE_PTS, OnCombinePoints)
  ON_BN_CLICKED(IDC_CHECK_REMOVE_OUTSIDE, OnTurnOffOutside)
  ON_BN_CLICKED(IDC_CHECK_TURN_OFF_COMBINED, OnTurnOffCombined)
  ON_BN_CLICKED(IDC_CHECK_REMOVE_POINTS, OnCheckRemovePoints)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_HOLE_THRESH, OnDeltaposSpinHoleThresh)
  ON_BN_CLICKED(IDC_CHECK_SKIP_AVERAGING, &CMultiCombinerDlg::OnCheckSkipAveraging)
END_MESSAGE_MAP()


// CMultiCombinerDlg message handlers
BOOL CMultiCombinerDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mHelper = mWinApp->mNavHelper;
  mCombiner = mHelper->mCombineHoles;
  m_sbcHoleThresh.SetRange(2, MAX_HOLE_THRESH);
  UpdateSettings();
  UpdateEnables();
  if (!m_bDisplayMultiShot)
    mWinApp->mMainView->DrawImage();
  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

// Store placement and NULL out pointer here
void CMultiCombinerDlg::OnCancel()
{
  mHelper->GetMultiCombinerPlacement();
  mHelper->mMultiCombinerDlg = NULL;
  DestroyWindow();
}

void CMultiCombinerDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

void CMultiCombinerDlg::CloseWindow()
{
  OnCancel();
}

// Store parameters directly in NavHelper
void CMultiCombinerDlg::OnCombineType()
{
  UpdateData(true);
  mHelper->SetMHCcombineType(m_iCombineType);
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}


void CMultiCombinerDlg::OnUndoCombine()
{
  if (!mCombiner->OKtoUndoCombine()) {
    AfxMessageBox("It is no longer possible to undo the last hole-combining", MB_EXCLAME);
    UpdateEnables();
    return;
  }
  mCombiner->UndoCombination();
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}


void CMultiCombinerDlg::OnTurnOffOutside()
{
  UpdateData(true);
  mHelper->SetMHCturnOffOutsidePoly(m_bTurnOffOutside);
  mWinApp->RestoreViewFocus();
}

void CMultiCombinerDlg::OnCheckDisplayMulti()
{
  UpdateData(true);
  mHelper->SetMHCenableMultiDisplay(m_bDisplayMultiShot);
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}


void CMultiCombinerDlg::OnCombinePoints()
{
  int error = mCombiner->CombineItems(m_iCombineType, m_bTurnOffOutside);
  if (error)
    AfxMessageBox("Error trying to combine hole for multiple Records:\n" +
      CString(mWinApp->mNavHelper->mCombineHoles->GetErrorMessage(error)), MB_EXCLAME);
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

void CMultiCombinerDlg::OnTurnOffCombined()
{
  UpdateData(true);
  if (m_bTurnOffCombined && m_bRemovePoints) {
    m_bRemovePoints = false;
    UpdateData(false);
  }
  mHelper->SetMHCdelOrTurnOffIfFew((m_bRemovePoints ? 1 : 0) +
    (m_bTurnOffCombined ? 2 : 0));
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

void CMultiCombinerDlg::OnCheckRemovePoints()
{
  UpdateData(true);
  if (m_bTurnOffCombined && m_bRemovePoints) {
    m_bTurnOffCombined = false;
    UpdateData(false);
  }
  mHelper->SetMHCdelOrTurnOffIfFew((m_bRemovePoints ? 1 : 0) +
    (m_bTurnOffCombined ? 2 : 0));
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

void CMultiCombinerDlg::OnDeltaposSpinHoleThresh(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  if (NewSpinnerValue(pNMHDR, pResult, 2, MAX_HOLE_THRESH, newVal))
    return;
  mHelper->SetMHCthreshNumHoles(newVal);
  m_strHoleThresh.Format("%d", newVal);
  UpdateData(false);
}

void CMultiCombinerDlg::OnCheckSkipAveraging()
{
  UpdateData(true);
  mHelper->SetMHCskipAveragingPos(m_bSkipAveraging);
}

void CMultiCombinerDlg::UpdateSettings()
{
  m_iCombineType = mHelper->GetMHCcombineType();
  B3DCLAMP(m_iCombineType, 0, 2);
  m_bDisplayMultiShot = mHelper->GetMHCenableMultiDisplay();
  m_bTurnOffOutside = mHelper->GetMHCturnOffOutsidePoly();
  m_bRemovePoints = mHelper->GetMHCdelOrTurnOffIfFew() == 1;
  m_bTurnOffCombined = mHelper->GetMHCdelOrTurnOffIfFew() > 1;
  m_sbcHoleThresh.SetPos(mHelper->GetMHCthreshNumHoles());
  m_strHoleThresh.Format("%d", mHelper->GetMHCthreshNumHoles());
  m_bSkipAveraging = mHelper->GetMHCskipAveragingPos();
  UpdateEnables();
  UpdateData(false);
}

void CMultiCombinerDlg::UpdateEnables()
{
  BOOL busy = mWinApp->DoingTasks();
  m_butCombinePts.EnableWindow(!busy);
  m_butTurnOffOutside.EnableWindow(m_iCombineType < 2);
  SetDlgItemText(IDC_CHECK_REMOVE_OUTSIDE, m_iCombineType ?
    "Turn off group items outside polygon" : "Turn off group items outside image");
  m_butUndoCombine.EnableWindow(!busy && mCombiner->OKtoUndoCombine());
  m_sbcHoleThresh.EnableWindow(m_bRemovePoints || m_bTurnOffCombined);
  EnableDlgItem(IDC_STAT_HOLE_THRESH, m_bRemovePoints || m_bTurnOffCombined);
}
