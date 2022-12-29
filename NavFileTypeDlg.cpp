// NavFileTypeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\NavFileTypeDlg.h"


// CNavFileTypeDlg dialog


CNavFileTypeDlg::CNavFileTypeDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavFileTypeDlg::IDD, pParent)
  , m_iSingleMont(0)
  , m_bFitPoly(FALSE)
  , m_bSkipDlgs(FALSE)
  , m_iReusability(0)
  , m_bChangeReusable(FALSE)
{
  mJustChangeOK = false;
}

CNavFileTypeDlg::~CNavFileTypeDlg()
{
}

void CNavFileTypeDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_ROPEN_FOR_SINGLE, m_iSingleMont);
  DDX_Control(pDX, IDC_CHECK_FIT_POLY, m_butFitPoly);
  DDX_Check(pDX, IDC_CHECK_FIT_POLY, m_bFitPoly);
  DDX_Control(pDX, IDC_CHECK_SKIP_DLGS, m_butSkipDlgs);
  DDX_Check(pDX, IDC_CHECK_SKIP_DLGS, m_bSkipDlgs);
  DDX_Radio(pDX, IDC_RCLOSE_FOR_NEXT, m_iReusability);
  DDX_Check(pDX, IDC_CHECK_CHANGE_REUSABLE, m_bChangeReusable);
}

BOOL CNavFileTypeDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  UpdateData(false);
  ManageEnables();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

BEGIN_MESSAGE_MAP(CNavFileTypeDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_ROPEN_FOR_SINGLE, OnRopenForSingle)
  ON_BN_CLICKED(IDC_ROPEN_FOR_MONTAGE, OnRopenForSingle)
  ON_BN_CLICKED(IDC_RCLOSE_FOR_NEXT, OnRcloseForNext)
  ON_BN_CLICKED(IDC_RLEAVE_OPEN, OnRcloseForNext)
  ON_BN_CLICKED(IDC_RONLY_IF_NEEDED, OnRcloseForNext)
  ON_BN_CLICKED(IDC_CHECK_FIT_POLY, OnCheckFitPoly)
END_MESSAGE_MAP()


// CNavFileTypeDlg message handlers


void CNavFileTypeDlg::OnOK() 
{
  UpdateData(true);
	CBaseDlg::OnOK();
}
void CNavFileTypeDlg::OnRopenForSingle()
{
  UpdateData(true);
  ManageEnables();
}


void CNavFileTypeDlg::OnRcloseForNext()
{
  UpdateData(true);
}


void CNavFileTypeDlg::OnCheckFitPoly()
{
  UpdateData(true);
  ManageEnables();
}

void CNavFileTypeDlg::ManageEnables()
{
  bool enable = m_iSingleMont && mPolyFitOK;
  m_butFitPoly.EnableWindow(enable);
  enable = enable && m_bFitPoly;
  EnableDlgItem(IDC_RCLOSE_FOR_NEXT, enable);
  EnableDlgItem(IDC_RLEAVE_OPEN, enable);
  EnableDlgItem(IDC_RONLY_IF_NEEDED, enable);
  EnableDlgItem(IDC_CHECK_CHANGE_REUSABLE, enable && mJustChangeOK);
}
