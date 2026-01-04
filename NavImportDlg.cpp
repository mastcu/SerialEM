// NavImportDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\NavImportDlg.h"
#include "NavigatorDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// CNavImportDlg dialog

CNavImportDlg::CNavImportDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavImportDlg::IDD, pParent)
  , m_strSection(_T(""))
  , m_iSection(0)
  , m_fStageX(0)
  , m_fStageY(0)
  , m_strRegNum(_T(""))
  , m_strSecLabel(_T(""))
  , m_bOverlay(false)
  , m_strOverlay(_T(""))
{
}

CNavImportDlg::~CNavImportDlg()
{
}

void CNavImportDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_EDITSECTION, m_editSection);
  DDX_Control(pDX, IDC_XFORMCOMBO, m_comboXform);
  DDX_Text(pDX, IDC_STATSECTION, m_strSecLabel);
  DDX_Text(pDX, IDC_STAT_REGNUM, m_strRegNum);
  DDX_Text(pDX, IDC_EDITSECTION, m_iSection);
  DDX_Text(pDX, IDC_EDIT_STAGEX, m_fStageX);
  DDX_Text(pDX, IDC_EDIT_STAGEY, m_fStageY);
  DDX_Text(pDX, IDC_EDIT_OVERLAY, m_strOverlay);
  DDV_MinMaxInt(pDX, m_iSection, 0, 100000);
  DDX_Control(pDX, IDC_SPIN_REGNUM, m_sbcRegNum);
  DDX_Control(pDX, IDC_CHECK_OVERLAY, m_butOverlay);
  DDX_Control(pDX, IDC_EDIT_OVERLAY, m_editOverlay);
  DDX_Control(pDX, IDC_STAT_OVERLAY1, m_statOverlay1);
  DDX_Control(pDX, IDC_STAT_OVERLAY2, m_statOverlay2);
  DDX_Check(pDX, IDC_CHECK_OVERLAY, m_bOverlay);
}


BEGIN_MESSAGE_MAP(CNavImportDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_REGNUM, OnDeltaposSpinRegnum)
  ON_EN_KILLFOCUS(IDC_EDIT_STAGEX, OnKillfocusEdit)
  ON_EN_KILLFOCUS(IDC_EDIT_STAGEY, OnKillfocusEdit)
  ON_EN_KILLFOCUS(IDC_EDITSECTION, OnKillfocusEdit)
  ON_BN_CLICKED(IDC_CHECK_OVERLAY, OnCheckOverlay)
END_MESSAGE_MAP()


// CNavImportDlg message handlers

void CNavImportDlg::OnDeltaposSpinRegnum(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newpos = mRegNum;
  do {
    newpos += pNMUpDown->iDelta;
    if (newpos < 1 || newpos > MAX_CURRENT_REG) {
     *pResult = 1;
      return;
    }
  } while (mWinApp->mNavigator->RegistrationUseType(newpos) == NAVREG_REGULAR ||
    newpos == mWinApp->mNavigator->GetCurrentRegistration());
  mRegNum = newpos;
  m_strRegNum.Format("%d", mRegNum);
  UpdateData(false);
  *pResult = 0;
}

void CNavImportDlg::OnKillfocusEdit()
{
  UpdateData(true);
  if (m_iSection >= mNumSections) {
    m_iSection = mNumSections - 1;
    UpdateData(false);
  }
}

BOOL CNavImportDlg::OnInitDialog()
{
  NavParams *navParam = mWinApp->GetNavParams();
	CBaseDlg::OnInitDialog();

  // Load the combo box
  for (int i = 0; i < navParam->numImportXforms; i++)
    m_comboXform.AddString((LPCTSTR)navParam->xformName[i]);
  m_comboXform.SetCurSel(mXformNum);
  m_comboXform.SetExtendedUI();
  SetDropDownHeight(&m_comboXform, navParam->numImportXforms);
  m_sbcRegNum.SetRange(1, 4 * MAX_CURRENT_REG);
  m_sbcRegNum.SetPos(2 * MAX_CURRENT_REG);

  // Set strings
  m_strSecLabel.Format("Section (0-%d):", mNumSections - 1);
  m_strRegNum.Format("%d", mRegNum);

  m_editSection.EnableWindow(mNumSections > 1);
  m_butOverlay.EnableWindow(mOverlayOK);
  m_editOverlay.EnableWindow(FALSE);
  m_statOverlay1.EnableWindow(FALSE);
  m_statOverlay2.EnableWindow(FALSE);
  UpdateData(false);
  return TRUE;
}

void CNavImportDlg::OnOK()
{
  bool channelsOK = true;
  char a, b, c;
  UpdateData(true);
  if (m_bOverlay) {
    channelsOK = m_strOverlay.GetLength() == 3;
    if (channelsOK) {
      a = m_strOverlay.GetAt(0);
      b = m_strOverlay.GetAt(1);
      c = m_strOverlay.GetAt(2);
      channelsOK = m_strOverlay.Find('1') >= 0 && m_strOverlay.Find('2') >= 0 &&
        (a == '0' || a == '1' || a == '2') && (b == '0' || b == '1' || b == '2') &&
        (c == '0' || c == '1' || c == '2');
    }
    if (!channelsOK) {
      AfxMessageBox("The entry for images to use for the 3 colors must be 3 digits,\r\n"
        "each one must be 0, 1, or 2,\r\n"
        "and 1 and 2 must both be included in the entry", MB_EXCLAME);
      return;
    }
  }
  mXformNum = m_comboXform.GetCurSel();
  m_iSection = B3DMAX(0, B3DMIN(m_iSection, mNumSections - 1));
	CBaseDlg::OnOK();
}

void CNavImportDlg::OnCheckOverlay()
{
  UpdateData(true);
  m_editOverlay.EnableWindow(m_bOverlay);
  m_statOverlay1.EnableWindow(m_bOverlay);
  m_statOverlay2.EnableWindow(m_bOverlay);
}
