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
{
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
}

BOOL CNavFileTypeDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  UpdateData(false);
  m_butFitPoly.EnableWindow(m_iSingleMont && mPolyFitOK);
	return TRUE;  // return TRUE  unless you set the focus to a control
}

BEGIN_MESSAGE_MAP(CNavFileTypeDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_ROPEN_FOR_SINGLE, OnRopenForSingle)
  ON_BN_CLICKED(IDC_ROPEN_FOR_MONTAGE, OnRopenForSingle)
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
  m_butFitPoly.EnableWindow(m_iSingleMont && mPolyFitOK);
}
