// MultiCombinerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "NavHelper.h"
#include "MultiCombinerDlg.h"


// CMultiCombinerDlg dialog


CMultiCombinerDlg::CMultiCombinerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MULTI_COMBINER, pParent)
{

}

CMultiCombinerDlg::~CMultiCombinerDlg()
{
}

void CMultiCombinerDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CMultiCombinerDlg, CBaseDlg)
END_MESSAGE_MAP()


// CMultiCombinerDlg message handlers
BOOL CMultiCombinerDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CMultiCombinerDlg::OnOK()
{
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CMultiCombinerDlg::OnCancel()
{
  mWinApp->mNavHelper->GetMultiCombinerPlacement();
  mWinApp->mNavHelper->mMultiCombinerDlg = NULL;
  DestroyWindow();
}

void CMultiCombinerDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

void CMultiCombinerDlg::CloseWindow()
{
  OnOK();
}
