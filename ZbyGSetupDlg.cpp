// ZbyGSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ZbyGSetupDlg.h"
#include "ParticleTasks.h"


// ZbyGSetupDlg dialog

CZbyGSetupDlg::CZbyGSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_Z_BY_G_SETUP, pParent)
{
  mNonModal = true;

}

CZbyGSetupDlg::~CZbyGSetupDlg()
{
}

void CZbyGSetupDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CZbyGSetupDlg, CBaseDlg)
END_MESSAGE_MAP()


// ZbyGSetupDlg message handlers
BOOL CZbyGSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CZbyGSetupDlg::OnOK()
{
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CZbyGSetupDlg::OnCancel()
{
  mWinApp->mParticleTasks->GetZbyGPlacement();
  mWinApp->mParticleTasks->mZbyGsetupDlg = NULL;
  DestroyWindow();
}

void CZbyGSetupDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CZbyGSetupDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}
