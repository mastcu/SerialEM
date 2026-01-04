// TSViewRange.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\TSViewRange.h"
#include "MultiTSTasks.h"
#include "SerialEMView.h"
#include "ChildFrm.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CTSViewRange dialog

CTSViewRange::CTSViewRange(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CTSViewRange::IDD, pParent)
{
  mNonModal = true;
}

CTSViewRange::~CTSViewRange()
{
}

void CTSViewRange::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CTSViewRange, CBaseDlg)
  ON_BN_CLICKED(IDC_BUTUSEANGLE, OnButUseAngle)
  ON_BN_CLICKED(IDC_BUT_CLOSE_KEEP, OnButCloseKeep)
END_MESSAGE_MAP()


// CTSViewRange message handlers

BOOL CTSViewRange::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  return FALSE;
}

void CTSViewRange::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

void CTSViewRange::OnOK()
{
  CloseWindow(true);
}

void CTSViewRange::OnButUseAngle()
{
  CString str;
  EMimageBuffer *imbuf;
  if (mWinApp->mStackView) {
    mWinApp->mStackView->SetFocus();
    imbuf = mWinApp->mStackView->GetActiveImBuf();
    mWinApp->mMultiTSTasks->StoreLimitAngle(false, imbuf->mTiltAngleOrig);
    str.Format("Using %.1f as a limiting angle", imbuf->mTiltAngleOrig);
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  }
}

void CTSViewRange::OnButCloseKeep()
{
  CloseWindow(false);
}

void CTSViewRange::CloseWindow(bool closeStack)
{
  CRect rect;
  int offset;
  if (mWinApp->mStackView) {
    if (closeStack) {
      mWinApp->mMultiTSTasks->mRangeViewer = NULL;
      mWinApp->mStackView->CloseFrame();
    } else {
      mWinApp->GetMainRect(&rect, offset);
      mWinApp->ResizeStackView((rect.Width() - offset) / 2, (rect.Height() - 60) / 2);
      CChildFrame *childFrame = (CChildFrame *)mWinApp->mStackView->GetParent();
      childFrame->SetWindowText("Old Tilt Range Sample");
      mWinApp->DetachStackView();
    }
  }
  mWinApp->mMultiTSTasks->mRangeViewer = NULL;
  DestroyWindow();
}
