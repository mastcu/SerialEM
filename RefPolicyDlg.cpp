// RefPolicyDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "./RefPolicyDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CRefPolicyDlg dialog

CRefPolicyDlg::CRefPolicyDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CRefPolicyDlg::IDD, pParent)
  , m_bIgnoreHigh(FALSE)
  , m_bPreferBin2(FALSE)
  , m_iAskPolicy(FALSE)
{
}

CRefPolicyDlg::~CRefPolicyDlg()
{
}

void CRefPolicyDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_CHECK_IGNORE_HIGH, m_butIgnoreHigh);
  DDX_Check(pDX, IDC_CHECK_IGNORE_HIGH, m_bIgnoreHigh);
  DDX_Control(pDX, IDC_CHECK_PREFER_BIN2, m_butPreferBin2);
  DDX_Check(pDX, IDC_CHECK_PREFER_BIN2, m_bPreferBin2);
  DDX_Control(pDX, IDC_STAT_DMGROUP, m_statDMgroup);
  DDX_Control(pDX, IDC_RADIO_ASK_IF_NEWER, m_butAskNewer);
  DDX_Control(pDX, IDC_RADIO_ASK_NEVER, m_butAskNever);
  DDX_Control(pDX, IDC_RADIO_ASK_ALWAYS, m_butAskAlways);
  DDX_Radio(pDX, IDC_RADIO_ASK_IF_NEWER, m_iAskPolicy);
}


BEGIN_MESSAGE_MAP(CRefPolicyDlg, CBaseDlg)
END_MESSAGE_MAP()


// CRefPolicyDlg message handlers
void CRefPolicyDlg::OnOK()
{
  UpdateData(true);
	CBaseDlg::OnOK();
}

BOOL CRefPolicyDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  m_statDMgroup.EnableWindow(mAnyGatan);
  m_butAskNewer.EnableWindow(mAnyGatan);
  m_butAskNever.EnableWindow(mAnyGatan);
  m_butAskAlways.EnableWindow(mAnyGatan);
  m_butIgnoreHigh.EnableWindow(mAnyHigher);
  return TRUE;
}
