// MacroToolbar.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\MacroToolbar.h"
#include "MacroProcessor.h"


// CMacroToolbar dialog

CMacroToolbar::CMacroToolbar(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CMacroToolbar::IDD, pParent)
{
  mNonModal = true;
}

CMacroToolbar::~CMacroToolbar()
{
}

void CMacroToolbar::DoDataExchange(CDataExchange* pDX)
{
	CBaseDlg::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CMacroToolbar, CBaseDlg)
  ON_CONTROL_RANGE(BN_CLICKED, ID_MACRO_RUN1, ID_MACRO_RUN20, OnMacroRun)
  ON_NOTIFY_EX_RANGE( TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify )
END_MESSAGE_MAP()


// CMacroToolbar message handlers
BOOL CMacroToolbar::OnInitDialog() 
{
  CRect winrect, butrect, clientRect;
  int ixOffset, top;
  CBaseDlg::OnInitDialog();
  EnableToolTips(true);
  GetWindowRect(&winrect);
  GetClientRect(&clientRect);
  ixOffset = (winrect.Width() - clientRect.Width()) / 2;
  mWinWidthOrig = winrect.Width();
  CButton *button = (CButton *)GetDlgItem(ID_MACRO_RUN1);
  button->GetWindowRect(&butrect);
  mButWidthOrig = butrect.Width();
  mButHeightOrig = butrect.Height();
  mLeftPos = butrect.left - winrect.left - ixOffset;
  mTopPos = (butrect.top - winrect.top) - (winrect.Height() - clientRect.Height()) + 
    ixOffset;
  top = butrect.top;
  mBaseWinHeight = (top - winrect.top) + 2;
  button = (CButton *)GetDlgItem(ID_MACRO_RUN2);
  button->GetWindowRect(&butrect);
  mGutterHeight = butrect.top - top - mButHeightOrig;
  SetLength(mWinApp->mMacroProcessor->GetNumToolButtons(), 
    mWinApp->mMacroProcessor->GetToolButHeight());
  UpdateSettings();

  // Disable default button
  SetDefID(45678);

  return TRUE;  // return TRUE unless you set the focus to a control
}

void CMacroToolbar::OnMacroRun(UINT nID)
{
  mWinApp->RestoreViewFocus();
  if (GetAsyncKeyState(VK_CONTROL) / 2 != 0)
    mWinApp->mMacroProcessor->OpenMacroEditor(nID - ID_MACRO_RUN1);
  else
    mWinApp->mMacroProcessor->ToolbarMacroRun(nID);
}

void CMacroToolbar::UpdateSettings(void)
{
  for (int i = 0; i < MAX_MACROS; i++)
    SetOneMacroLabel(i);
  SetLength(mWinApp->mMacroProcessor->GetNumToolButtons(), 
    mWinApp->mMacroProcessor->GetToolButHeight());
  Update();
}

void CMacroToolbar::Update(void)
{
  CString *macros = mWinApp->GetMacros();
  for (int i = 0; i < MAX_MACROS; i++) {
    CButton *button = (CButton *)GetDlgItem(ID_MACRO_RUN1 + i);
    button->EnableWindow(mWinApp->mMacroProcessor->MacroRunnable(i));
  }
}

void CMacroToolbar::SetOneMacroLabel(int num)
{
  UINT nID = ID_MACRO_RUN1 + num;
  CString label;
  CString *names = mWinApp->GetMacroNames();
  if (names[num].IsEmpty())
    label.Format("Script %d", num + 1);
  else
    label = names[num];
  SetDlgItemText(nID, label);
}

void CMacroToolbar::SetLength(int num, int butHeight)
{
  int ind, gutter, width, height, maxExtent = 0, deltaForBut = 6, deltaForWin = 6;
  CButton *button;
  CString *names = mWinApp->GetMacroNames();
  if (butHeight < mButHeightOrig) {
    butHeight = mButHeightOrig;
    mWinApp->mMacroProcessor->SetToolButHeight(butHeight);
  }
  B3DCLAMP(num, 1, MAX_MACROS);

  // Make the gutter a little bigger
  ind = (mGutterHeight * butHeight) / mButHeightOrig;
  gutter = mGutterHeight + (ind - mGutterHeight) / 2;

  // Compute height of dialog
  height = mBaseWinHeight + num * butHeight + (num - 1) * gutter;

  // Determine maximum text extent
  button = (CButton *)GetDlgItem(ID_MACRO_RUN1);
  CClientDC dc(button);
  dc.SelectObject(button->GetFont());
  for (ind = 0; ind < num; ind++) {
    if (!names[ind].IsEmpty()) {
      CSize size = dc.GetTextExtent(names[ind]);
      maxExtent = B3DMAX(maxExtent, size.cx);
    }
  }

  // Resize window first
  width = B3DMAX(maxExtent + deltaForBut + deltaForWin, mWinWidthOrig);
  SetWindowPos(NULL, 0, 0, width, height, SWP_NOMOVE);

  // Resize buttons: need to do them all to get ones past the end out of the way
  width = B3DMAX(maxExtent + deltaForBut, mButWidthOrig);
  for (ind = 0; ind < MAX_MACROS; ind++) {
    button = (CButton *)GetDlgItem(ID_MACRO_RUN1 + ind);
    button->SetWindowPos(NULL, mLeftPos, mTopPos + ind * (butHeight + gutter), 
      width, butHeight, SWP_NOZORDER);
  }
  mWinApp->RestoreViewFocus();
}

void CMacroToolbar::OnCancel()
{
  mWinApp->mMacroProcessor->ToolbarClosing();
  DestroyWindow();
}

void CMacroToolbar::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

BOOL CMacroToolbar::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
  TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
  int nID;
  if (pTTT->uFlags & TTF_IDISHWND)
  {
    // idFrom is actually the HWND of the tool
    nID = ::GetDlgCtrlID((HWND)pNMHDR->idFrom);
    if(nID)
    {
      pTTT->lpszText = MAKEINTRESOURCE(nID);
      pTTT->hinst = AfxGetResourceHandle();
      return(TRUE);
    }
  }
  return(FALSE);
}
