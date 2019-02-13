// OneLineScript.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "OneLineScript.h"
#include "MacroEditer.h"
#include "MacroProcessor.h"

// COneLineScript dialog

COneLineScript::COneLineScript(CWnd* pParent /*=NULL*/)
	: CBaseDlg(COneLineScript::IDD, pParent)
  , m_strCompletions(_T("Tab or  `  to complete command"))
  , m_strOneLine(_T(""))
{
  mNonModal = true;
  mInitialized = false;
}

COneLineScript::~COneLineScript()
{
}

void COneLineScript::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_COMPLETIONS, m_strCompletions);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE, m_editOneLine);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE, m_strOneLine);
  DDX_Control(pDX, IDOK, m_butRun);
}


BEGIN_MESSAGE_MAP(COneLineScript, CDialog)
  ON_EN_CHANGE(IDC_EDIT_ONE_LINE, OnEnChangeEditOneLine)
  ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL COneLineScript::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  // Here is the obscure set of information needed to reposition the Run button and resize
  // the text box when the width changes
  CRect wndRect, editRect, runRect, clientRect;
  GetClientRect(clientRect);
  GetWindowRect(wndRect);
  int iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  m_butRun.GetWindowRect(runRect);
  m_iRunTop = (runRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset;
  m_iRunLeftOrig = runRect.left - wndRect.left - iXoffset;
  m_editOneLine.GetWindowRect(editRect);
  m_iWinXorig = wndRect.Width();
  m_iEditXorig = editRect.Width();
  m_iEditHeight = editRect.Height();
  UpdateData(false);
  mInitialized = true;
  return FALSE;
}

// COneLineScript message handlers
void COneLineScript::OnOK()
{
  UpdateData(true);
  mMacros[MAX_MACROS] = m_strOneLine;
  if (!m_strOneLine.IsEmpty())
    mWinApp->mMacroProcessor->Run(MAX_MACROS);
  mWinApp->RestoreViewFocus();
}

void COneLineScript::OnCancel()
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  if (mWinApp->mMacroProcessor->DoingMacro())
    return;
  mMacros[MAX_MACROS] = m_strOneLine;
  mWinApp->mMacroProcessor->OneLineClosing();
  DestroyWindow();
}

void COneLineScript::PostNcDestroy()
{
  delete this;  
  CDialog::PostNcDestroy();
}

void COneLineScript::OnSize(UINT nType, int cx, int cy)
{
  if (!mInitialized)
    return;
  CRect wndRect, editRect, runRect;
  CDialog::OnSize(nType, cx, cy);
  GetWindowRect(wndRect);
  int delta = wndRect.Width() - m_iWinXorig;
  int newx = B3DMAX(5, m_iEditXorig + delta);
  m_editOneLine.SetWindowPos(NULL, 0, 0, newx, m_iEditHeight, SWP_NOZORDER | SWP_NOMOVE);
  m_butRun.SetWindowPos(NULL, B3DMAX(5, m_iRunLeftOrig + delta), m_iRunTop, 0, 0, 
    SWP_NOZORDER | SWP_NOSIZE);
}

// Translate tab to our completion character.  It is good on US standard keyboards only
BOOL COneLineScript::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    pMsg->wParam = VK_OEM_3;
  return CDialog::PreTranslateMessage(pMsg);
}

void COneLineScript::Update()
{
  BOOL runnable = mWinApp->mMacroProcessor->MacroRunnable(MAX_MACROS);
  ((CButton *)GetDlgItem(IDOK))->EnableWindow(runnable);
  //((CButton *)GetDlgItem(IDCANCEL))->EnableWindow(
    //!mWinApp->mMacroProcessor->DoingMacro());
}

void COneLineScript::OnEnChangeEditOneLine()
{
  int sel1, sel2;
  bool setCompletions, completing;
  UpdateData(true);
  m_editOneLine.GetSel(sel1, sel2);
  CMacroEditer::HandleCompletionsAndIndent(m_strOneLine, m_strCompletions, sel2, 
    setCompletions, completing);
  if (setCompletions)
    SetDlgItemText(IDC_STAT_COMPLETIONS, m_strCompletions);
  if (completing) {
    UpdateData(false);
    m_editOneLine.SetSel(sel2, sel2);
  }
}

