// CMultiGridDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "MultiGridDlg.h"
#include "NavHelper.h"

static int idTable[] = {IDC_STAT_RUN, IDC_STAT_NAME,
  IDC_CHECK_MULGRID_RUN1, IDC_CHECK_MULGRID_RUN2, IDC_CHECK_MULGRID_RUN3,
  IDC_CHECK_MULGRID_RUN4, IDC_CHECK_MULGRID_RUN5, IDC_CHECK_MULGRID_RUN6,
  IDC_CHECK_MULGRID_RUN7, IDC_CHECK_MULGRID_RUN8, IDC_CHECK_MULGRID_RUN9,
  IDC_CHECK_MULGRID_RUN10, IDC_CHECK_MULGRID_RUN11, IDC_CHECK_MULGRID_RUN12,
  IDC_CHECK_MULGRID_RUN13, IDC_EDIT_MULGRID_NAME1, IDC_EDIT_MULGRID_NAME2,
  IDC_EDIT_MULGRID_NAME3, IDC_EDIT_MULGRID_NAME4, IDC_EDIT_MULGRID_NAME5,
  IDC_EDIT_MULGRID_NAME6, IDC_EDIT_MULGRID_NAME7, IDC_EDIT_MULGRID_NAME8,
  IDC_EDIT_MULGRID_NAME9, IDC_EDIT_MULGRID_NAME10, IDC_EDIT_MULGRID_NAME11,
  IDC_EDIT_MULGRID_NAME12, IDC_EDIT_MULGRID_NAME13,
  IDCANCEL, IDOK, IDC_BUTHELP, PANEL_END, TABLE_END};

static int sTopTable[sizeof(idTable) / sizeof(int)];
static int sLeftTable[sizeof(idTable) / sizeof(int)];
static int sHeightTable[sizeof(idTable) / sizeof(int)];


// CMultiGridDlg dialog


CMultiGridDlg::CMultiGridDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MULTI_GRID, pParent)
{
  mNonModal = true;
}

CMultiGridDlg::~CMultiGridDlg()
{
}

void CMultiGridDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CMultiGridDlg, CBaseDlg)
END_MESSAGE_MAP()


// CMultiGridDlg message handlers
BOOL CMultiGridDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  SetupPanelTables(idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CMultiGridDlg::OnOK()
{
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CMultiGridDlg::OnCancel()
{
  mWinApp->mNavHelper->GetMultiGridPlacement();
  mWinApp->mNavHelper->mMultiGridDlg = NULL;
  DestroyWindow();
}

void CMultiGridDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CMultiGridDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}
