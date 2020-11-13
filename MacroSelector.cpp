// CMacroSelector.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "MacroSelector.h"


// CMacroSelector dialog

CMacroSelector::CMacroSelector(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_MACROSELECTER, pParent)
  , m_strInfoText(_T(""))
  , m_strEntryText(_T(""))
{

}

CMacroSelector::~CMacroSelector()
{
}

void CMacroSelector::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_INFO_TEXT, m_strInfoText);
  DDX_Text(pDX, IDC_STAT_ENTRY_TEXT, m_strEntryText);
  DDX_Control(pDX, IDC_COMBO_SELECTOR, m_comboBox);
}

BOOL CMacroSelector::OnInitDialog()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!winApp->GetDisplayNotTruly120DPI())
    mSetDPI.Attach(AfxFindResourceHandle(MAKEINTRESOURCE(IDD), RT_DIALOG),
      m_hWnd, IDD, B3DNINT(1.25 * winApp->GetSystemDPI()));
  CDialog::OnInitDialog();
  LoadMacrosIntoDropDown(m_comboBox, false, true);
  m_comboBox.SetCurSel(mMacroIndex + 1);
  return TRUE;
}

void CMacroSelector::OnOK()
{
  CDialog::OnOK();
}

void CMacroSelector::OnCancel()
{
  CDialog::OnCancel();
}


BEGIN_MESSAGE_MAP(CMacroSelector, CDialog)
  ON_CBN_SELENDOK(IDC_COMBO_SELECTOR, &CMacroSelector::OnCbnSelendokComboSelector)
END_MESSAGE_MAP()


// CMacroSelector message handlers


void CMacroSelector::OnCbnSelendokComboSelector()
{
  mMacroIndex = m_comboBox.GetCurSel() - 1;
}
