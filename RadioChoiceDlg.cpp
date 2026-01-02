// RadioChoiceDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "RadioChoiceDlg.h"
#include "afxdialogex.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CRadioChoiceDlg dialog


CRadioChoiceDlg::CRadioChoiceDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_RADIO_CHOICE, pParent)
  , m_iChoice(0)
{

}

CRadioChoiceDlg::~CRadioChoiceDlg()
{
}

void CRadioChoiceDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_RCHOICE_ONE, m_iChoice);
}


BEGIN_MESSAGE_MAP(CRadioChoiceDlg, CDialog)
END_MESSAGE_MAP()


// CRadioChoiceDlg message handlers
BOOL CRadioChoiceDlg::OnInitDialog()
{
  SetDlgItemText(IDC_STAT_INFO_LINE1, mInfoLine1);
  SetDlgItemText(IDC_STAT_INFO_LINE2, mInfoLine2);
  SetDlgItemText(IDC_RCHOICE_ONE, mChoiceOne);
  SetDlgItemText(IDC_RCHOICE_ONE2, mChoiceTwo);
  if (mChoiceThree.IsEmpty()) {
    CButton *but = (CButton *)GetDlgItem(IDC_RCHOICE_THREE);
    if (but)
      but->ShowWindow(SW_HIDE);
  } else
    SetDlgItemText(IDC_RCHOICE_THREE, mChoiceThree);
  B3DCLAMP(m_iChoice, 0, mChoiceThree.IsEmpty() ? 1 : 2);
  UpdateData(false);
  return TRUE;
}

void CRadioChoiceDlg::OnOK()
{
  UpdateData(true);
  CDialog::OnOK();
}
void CRadioChoiceDlg::OnCancel()
{
  CDialog::OnCancel();
}
