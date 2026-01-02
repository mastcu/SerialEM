// NavAcqHoleFinder.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "NavHelper.h"
#include "NavAcqHoleFinder.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// NavAcqHoleFinder dialog


NavAcqHoleFinder::NavAcqHoleFinder(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_NAVACQ_HOLE_FINDER, pParent)
  , m_bRunCombiner(FALSE)
{

}

NavAcqHoleFinder::~NavAcqHoleFinder()
{
}

void NavAcqHoleFinder::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_RUN_COMBINER, m_bRunCombiner);
}


BEGIN_MESSAGE_MAP(NavAcqHoleFinder, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_RUN_COMBINER, OnRunCombiner)
  ON_BN_CLICKED(IDC_BUT_OPEN_HOLE_FINDER, OnOpenHoleFinder)
  ON_BN_CLICKED(IDC_BUT_OPEN_COMBINER, OnOpenCombiner)
END_MESSAGE_MAP()


// NavAcqHoleFinder message handlers
BOOL NavAcqHoleFinder::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  EnableDlgItem(IDC_BUT_OPEN_COMBINER, m_bRunCombiner);
  ShowDlgItem(IDC_BUTHELP, false);
  UpdateData(false);
  return TRUE;  // return TRUE  unless you set the focus to a control
}

void NavAcqHoleFinder::OnOK()
{
  UpdateData(true);
  CBaseDlg::OnOK();
}


void NavAcqHoleFinder::OnRunCombiner()
{
  UpdateData(true);
  EnableDlgItem(IDC_BUT_OPEN_COMBINER, m_bRunCombiner);
}


void NavAcqHoleFinder::OnOpenHoleFinder()
{
  mWinApp->mNavHelper->OpenHoleFinder();
}


void NavAcqHoleFinder::OnOpenCombiner()
{
  mWinApp->mNavHelper->OpenMultiCombiner();
}
