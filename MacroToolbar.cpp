// MacroToolbar.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\MacroToolbar.h"
#include "MacroProcessor.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


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
  DDX_Control(pDX, ID_MACRO_RUN1, mRunButtons[0]);
  DDX_Control(pDX, ID_MACRO_RUN2, mRunButtons[1]);
  DDX_Control(pDX, ID_MACRO_RUN3, mRunButtons[2]);
  DDX_Control(pDX, ID_MACRO_RUN4, mRunButtons[3]);
  DDX_Control(pDX, ID_MACRO_RUN5, mRunButtons[4]);
  DDX_Control(pDX, ID_MACRO_RUN6, mRunButtons[5]);
  DDX_Control(pDX, ID_MACRO_RUN7, mRunButtons[6]);
  DDX_Control(pDX, ID_MACRO_RUN8, mRunButtons[7]);
  DDX_Control(pDX, ID_MACRO_RUN9, mRunButtons[8]);
  DDX_Control(pDX, ID_MACRO_RUN10, mRunButtons[9]);
  DDX_Control(pDX, ID_MACRO_RUN11, mRunButtons[10]);
  DDX_Control(pDX, ID_MACRO_RUN12, mRunButtons[11]);
  DDX_Control(pDX, ID_MACRO_RUN13, mRunButtons[12]);
  DDX_Control(pDX, ID_MACRO_RUN14, mRunButtons[13]);
  DDX_Control(pDX, ID_MACRO_RUN15, mRunButtons[14]);
  DDX_Control(pDX, ID_MACRO_RUN16, mRunButtons[15]);
  DDX_Control(pDX, ID_MACRO_RUN17, mRunButtons[16]);
  DDX_Control(pDX, ID_MACRO_RUN18, mRunButtons[17]);
  DDX_Control(pDX, ID_MACRO_RUN19, mRunButtons[18]);
  DDX_Control(pDX, ID_MACRO_RUN20, mRunButtons[19]);
  DDX_Control(pDX, ID_MACRO_RUN21, mRunButtons[20]);
  DDX_Control(pDX, ID_MACRO_RUN22, mRunButtons[21]);
  DDX_Control(pDX, ID_MACRO_RUN23, mRunButtons[22]);
  DDX_Control(pDX, ID_MACRO_RUN24, mRunButtons[23]);
  DDX_Control(pDX, ID_MACRO_RUN25, mRunButtons[24]);
  DDX_Control(pDX, ID_MACRO_RUN26, mRunButtons[25]);
  DDX_Control(pDX, ID_MACRO_RUN27, mRunButtons[26]);
  DDX_Control(pDX, ID_MACRO_RUN28, mRunButtons[27]);
  DDX_Control(pDX, ID_MACRO_RUN29, mRunButtons[28]);
  DDX_Control(pDX, ID_MACRO_RUN30, mRunButtons[29]);
  DDX_Control(pDX, ID_MACRO_RUN31, mRunButtons[30]);
  DDX_Control(pDX, ID_MACRO_RUN32, mRunButtons[31]);
  DDX_Control(pDX, ID_MACRO_RUN33, mRunButtons[32]);
  DDX_Control(pDX, ID_MACRO_RUN34, mRunButtons[33]);
  DDX_Control(pDX, ID_MACRO_RUN35, mRunButtons[34]);
  DDX_Control(pDX, ID_MACRO_RUN36, mRunButtons[35]);
  DDX_Control(pDX, ID_MACRO_RUN37, mRunButtons[36]);
  DDX_Control(pDX, ID_MACRO_RUN38, mRunButtons[37]);
  DDX_Control(pDX, ID_MACRO_RUN39, mRunButtons[38]);
  DDX_Control(pDX, ID_MACRO_RUN40, mRunButtons[39]);
  DDX_Control(pDX, ID_MACRO_RUN41, mRunButtons[40]);
  DDX_Control(pDX, ID_MACRO_RUN42, mRunButtons[41]);
  DDX_Control(pDX, ID_MACRO_RUN43, mRunButtons[42]);
  DDX_Control(pDX, ID_MACRO_RUN44, mRunButtons[43]);
  DDX_Control(pDX, ID_MACRO_RUN45, mRunButtons[44]);
  DDX_Control(pDX, ID_MACRO_RUN46, mRunButtons[45]);
  DDX_Control(pDX, ID_MACRO_RUN47, mRunButtons[46]);
  DDX_Control(pDX, ID_MACRO_RUN48, mRunButtons[47]);
  DDX_Control(pDX, ID_MACRO_RUN49, mRunButtons[48]);
  DDX_Control(pDX, ID_MACRO_RUN50, mRunButtons[49]);
  DDX_Control(pDX, ID_MACRO_RUN51, mRunButtons[50]);
  DDX_Control(pDX, ID_MACRO_RUN52, mRunButtons[51]);
  DDX_Control(pDX, ID_MACRO_RUN53, mRunButtons[52]);
  DDX_Control(pDX, ID_MACRO_RUN54, mRunButtons[53]);
  DDX_Control(pDX, ID_MACRO_RUN55, mRunButtons[54]);
  DDX_Control(pDX, ID_MACRO_RUN56, mRunButtons[55]);
  DDX_Control(pDX, ID_MACRO_RUN57, mRunButtons[56]);
  DDX_Control(pDX, ID_MACRO_RUN58, mRunButtons[57]);
  DDX_Control(pDX, ID_MACRO_RUN59, mRunButtons[58]);
  DDX_Control(pDX, ID_MACRO_RUN60, mRunButtons[59]);
}


BEGIN_MESSAGE_MAP(CMacroToolbar, CBaseDlg)
  ON_CONTROL_RANGE(BN_CLICKED, ID_MACRO_RUN1, ID_MACRO_RUN60, OnMacroRun)
  ON_NOTIFY_EX_RANGE( TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify )
  ON_NOTIFY_RANGE(NM_LDOWN, ID_MACRO_RUN1, ID_MACRO_RUN60, OnRunButDraw)
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

// Notification of a right-click event: open editor on right-click
void CMacroToolbar::OnRunButDraw(UINT nID, NMHDR *pNotifyStruct, LRESULT *result)
{
  int index = nID - ID_MACRO_RUN1;
  if (mRunButtons[index].m_bRightWasClicked) {
    mRunButtons[index].m_bRightWasClicked = false;
    mWinApp->mMacroProcessor->OpenMacroEditor(index);
    mWinApp->mScope->SetRestoreViewFocusCount(1);
  }
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
  if (!butHeight)
    butHeight = mButHeightOrig;
  if (butHeight < B3DNINT(0.8 * mButHeightOrig)) {
    butHeight = B3DNINT(0.8 * mButHeightOrig);
    mWinApp->mMacroProcessor->SetToolButHeight(butHeight);
  }
  B3DCLAMP(num, 1, MAX_MACROS);

  // Make the gutter a little bigger
  ind = (mGutterHeight * butHeight) / mButHeightOrig;
  if (num > 40 && ind <= mGutterHeight)
    gutter = B3DMAX(4, ind);
  else
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
  Invalidate();
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
