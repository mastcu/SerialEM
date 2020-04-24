// MultiCombinerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "NavHelper.h"
#include "MultiHoleCombiner.h"
#include "MultiCombinerDlg.h"


// CMultiCombinerDlg dialog


CMultiCombinerDlg::CMultiCombinerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MULTI_COMBINER, pParent)
  , m_bDisplayMultiShot(FALSE)
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
}


BEGIN_MESSAGE_MAP(CMultiCombinerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RON_IMAGE, OnCombineType)
  ON_BN_CLICKED(IDC_RIN_POLYGON, OnCombineType)
  ON_BN_CLICKED(IDC_RSAME_GROUP, OnCombineType)
  ON_BN_CLICKED(IDC_BUT_UNDO_COMBINE, OnUndoCombine)
  ON_BN_CLICKED(IDC_CHECK_DISPLAY_MULTI, OnCheckDisplayMulti)
  ON_BN_CLICKED(IDC_BUT_COMBINE_PTS, OnCombinePoints)
END_MESSAGE_MAP()


// CMultiCombinerDlg message handlers
BOOL CMultiCombinerDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mHelper = mWinApp->mNavHelper;
  mCombiner = mHelper->mCombineHoles;
  UpdateSettings();
  UpdateEnables();
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
  mWinApp->RestoreViewFocus();
}


void CMultiCombinerDlg::OnUndoCombine()
{
  if (!mCombiner->OKtoUndoCombine()) {
    AfxMessageBox("It is no longer possible to undo the last hole-combining", MB_EXCLAME);
    return;
  }
  mCombiner->UndoCombination();
  UpdateEnables();
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
  int error = mCombiner->CombineItems(m_iCombineType);
  if (error)
    AfxMessageBox("Error trying to combine hole for multiple Records:\n" +
      CString(mWinApp->mNavHelper->mCombineHoles->GetErrorMessage(error)), MB_EXCLAME);
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

void CMultiCombinerDlg::UpdateSettings()
{
  m_iCombineType = mHelper->GetMHCcombineType();
  B3DCLAMP(m_iCombineType, 0, 2);
  m_bDisplayMultiShot = mHelper->GetMHCenableMultiDisplay();
  UpdateData(false);
}

void CMultiCombinerDlg::UpdateEnables()
{
  BOOL busy = mWinApp->DoingTasks();
  m_butCombinePts.EnableWindow(!busy);
  m_butUndoCombine.EnableWindow(!busy && mCombiner->OKtoUndoCombine());
}

