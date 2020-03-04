// ScreenShotDialog.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ScreenShotDialog.h"


// CScreenShotDialog dialog

CScreenShotDialog::CScreenShotDialog(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_SCREENSHOT, pParent)
{
}

CScreenShotDialog::~CScreenShotDialog()
{
}

BOOL CScreenShotDialog::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

void CScreenShotDialog::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CScreenShotDialog, CBaseDlg)
	
END_MESSAGE_MAP()


// CScreenShotDialog message handlers

BOOL CScreenShotDialog::OnInitDialog()
{
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

void CScreenShotDialog::OnOK()
{

}

void CScreenShotDialog::OnCancel()
{
}

void CScreenShotDialog::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}


