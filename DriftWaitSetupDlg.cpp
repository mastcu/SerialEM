// DriftWaitSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DriftWaitSetupDlg.h"


// CDriftWaitSetupDlg dialog

CDriftWaitSetupDlg::CDriftWaitSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_DRIFTWAITSETUP, pParent)
{

}

CDriftWaitSetupDlg::~CDriftWaitSetupDlg()
{
}

void CDriftWaitSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDriftWaitSetupDlg, CBaseDlg)
END_MESSAGE_MAP()


// CDriftWaitSetupDlg message handlers
BOOL CDriftWaitSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  return TRUE;
}

void CDriftWaitSetupDlg::OnOK()
{
  SetFocus();
  CBaseDlg::OnOK();
}
