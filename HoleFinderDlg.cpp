// HoleFinderDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "HoleFinderDlg.h"
#include "afxdialogex.h"


// CHoleFinderDlg dialog

CHoleFinderDlg::CHoleFinderDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_HOLE_FINDER, pParent)
{

}

CHoleFinderDlg::~CHoleFinderDlg()
{
}

void CHoleFinderDlg::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CHoleFinderDlg, CBaseDlg)
END_MESSAGE_MAP()


// CHoleFinderDlg message handlers
BOOL CHoleFinderDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

void CHoleFinderDlg::OnOK()
{
  OnCancel();
}

void CHoleFinderDlg::OnCancel()
{
  //mWinApp->GetScreenShotPlacement();
  //mWinApp->mScreenShotDialog = NULL;
  DestroyWindow();
}

void CHoleFinderDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CHoleFinderDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

