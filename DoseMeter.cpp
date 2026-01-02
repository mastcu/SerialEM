// DoseMeter.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DoseMeter.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CDoseMeter dialog


CDoseMeter::CDoseMeter(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CDoseMeter::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDoseMeter)
	m_strDose = _T("");
	//}}AFX_DATA_INIT
  mNonModal = true;
}


void CDoseMeter::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDoseMeter)
	DDX_Control(pDX, IDC_STATCUMDOSE, m_statDose);
	DDX_Text(pDX, IDC_STATCUMDOSE, m_strDose);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDoseMeter, CBaseDlg)
	//{{AFX_MSG_MAP(CDoseMeter)
	ON_BN_CLICKED(IDC_BUTRESET, OnButreset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDoseMeter message handlers

void CDoseMeter::OnButreset()
{
  mCumDose = 0.;
  mWinApp->mScopeStatus.OutputDose();
  mWinApp->RestoreViewFocus();
}

BOOL CDoseMeter::OnInitDialog()
{
	CBaseDlg::OnInitDialog();

  CRect rect;
  m_statDose.GetWindowRect(&rect);
  mFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, m_strFontName);
  m_statDose.SetFont(&mFont);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDoseMeter::OnCancel()
{
  mWinApp->mScopeStatus.DoseClosing();
  DestroyWindow();
}

void CDoseMeter::PostNcDestroy()
{
	delete this;
	CDialog::PostNcDestroy();
}

