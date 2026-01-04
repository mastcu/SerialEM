// NavBacklashDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "NavBacklashDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CNavBacklashDlg dialog

CNavBacklashDlg::CNavBacklashDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavBacklashDlg::IDD, pParent)
  , m_fMinField(0)
  , m_iAskOrAuto(0)
{

}

CNavBacklashDlg::~CNavBacklashDlg()
{
}

void CNavBacklashDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_EDIT_MIN_FIELD, m_editMinField);
  DDX_Control(pDX, IDC_STAT_MIN_FIELD, m_statMinField);
  DDX_Control(pDX, IDC_STAT_MICRONS, m_statMicrons);
  DDX_Text(pDX, IDC_EDIT_MIN_FIELD, m_fMinField);
  DDV_MinMaxFloat(pDX, m_fMinField, 1., 100.);
  DDX_Radio(pDX, IDC_RBACKLASH_NONE, m_iAskOrAuto);
}


BEGIN_MESSAGE_MAP(CNavBacklashDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RBACKLASH_NONE, &CNavBacklashDlg::OnRbacklashNone)
  ON_BN_CLICKED(IDC_RBACKLASH_ASK, &CNavBacklashDlg::OnRbacklashNone)
  ON_BN_CLICKED(IDC_RBACKLASH_AUTO, &CNavBacklashDlg::OnRbacklashNone)
END_MESSAGE_MAP()

BOOL CNavBacklashDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  UpdateData(false);
  ManageFOV();
	return TRUE;  // return TRUE  unless you set the focus to a control
}


// CNavBacklashDlg message handlers

void CNavBacklashDlg::OnOK()
{
  UpdateData(true);
	CBaseDlg::OnOK();
}

void CNavBacklashDlg::OnRbacklashNone()
{
  UpdateData(true);
  ManageFOV();
}

void CNavBacklashDlg::ManageFOV(void)
{
  m_statMicrons.EnableWindow(m_iAskOrAuto > 0);
  m_statMinField.EnableWindow(m_iAskOrAuto > 0);
  m_editMinField.EnableWindow(m_iAskOrAuto > 0);
}
