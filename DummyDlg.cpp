// DummyDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DummyDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static VOID CALLBACK UpdateProc(
  HWND hwnd,     // handle of window for timer messages
  UINT uMsg,     // WM_TIMER message
  UINT idEvent,  // timer identifier
  DWORD dwTime   // current system time
);

static int timerID;

static CDummyDlg* thisDlg;

/////////////////////////////////////////////////////////////////////////////
// CDummyDlg dialog


CDummyDlg::CDummyDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDummyDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDummyDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDummyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDummyDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDummyDlg, CDialog)
	//{{AFX_MSG_MAP(CDummyDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDummyDlg message handlers

BOOL CDummyDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
#ifdef USE_SDK_FILEDLG
#ifdef USE_THREAD
  // Run SDK file dialog in thread
  ShowWindow(SW_HIDE);
  thisDlg = this;
  mfdTDp->done = false;
  MyFileDlgThread *dlgThread = (MyFileDlgThread *)AfxBeginThread(
    RUNTIME_CLASS(MyFileDlgThread), THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  dlgThread->mfdTDp = mfdTDp;
  dlgThread->ResumeThread();

  timerID = ::SetTimer(NULL, 1, 50, UpdateProc);
#else

  // Or show dialog directly
  EndDialog(RunSdkFileDlg(mfdTDp));
#endif
#endif
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

static VOID CALLBACK UpdateProc(
  HWND hwnd,     // handle of window for timer messages
  UINT uMsg,     // WM_TIMER message
  UINT idEvent,  // timer identifier
  DWORD dwTime   // current system time
)
{
  if (!thisDlg->mfdTDp->done)
    return;
  ::KillTimer(NULL, timerID);
  thisDlg->EndDialog(thisDlg->mfdTDp->retval);
}

void CDummyDlg::OnOK()
{
}
void CDummyDlg::OnCancel()
{

}
