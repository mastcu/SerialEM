// OneLineScript.cpp : Dialog for simple one-line scripts
//
// Copyright (C) 2018-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "OneLineScript.h"
#include "MacroEditer.h"
#include "MacroProcessor.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// COneLineScript dialog

COneLineScript::COneLineScript(CWnd* pParent /*=NULL*/)
	: CBaseDlg(COneLineScript::IDD, pParent)
  , m_strCompletions(_T("Tab or  `  to complete command"))
{
  mNonModal = true;
  mInitialized = false;
  mLineWithFocus = -1;
  mLineForSignature = -1;
}

COneLineScript::~COneLineScript()
{
}

void COneLineScript::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_COMPLETIONS, m_strCompletions);
  DDX_Control(pDX, IDC_STAT_COMPLETIONS, m_statCompletions);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE, m_editOneLine[0]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE2, m_editOneLine[1]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE3, m_editOneLine[2]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE4, m_editOneLine[3]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE5, m_editOneLine[4]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE6, m_editOneLine[5]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE7, m_editOneLine[6]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE8, m_editOneLine[7]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE9, m_editOneLine[8]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE10, m_editOneLine[9]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE, m_strOneLine[0]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE2, m_strOneLine[1]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE3, m_strOneLine[2]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE4, m_strOneLine[3]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE5, m_strOneLine[4]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE6, m_strOneLine[5]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE7, m_strOneLine[6]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE8, m_strOneLine[7]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE9, m_strOneLine[8]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE10, m_strOneLine[9]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE1, m_butRun[0]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE2, m_butRun[1]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE3, m_butRun[2]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE4, m_butRun[3]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE5, m_butRun[4]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE6, m_butRun[5]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE7, m_butRun[6]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE8, m_butRun[7]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE9, m_butRun[8]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE10, m_butRun[9]);
}


BEGIN_MESSAGE_MAP(COneLineScript, CDialog)
  ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_ONE_LINE, IDC_EDIT_ONE_LINE10, OnEnChangeEditOneLine)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_RUN_ONE_LINE1, IDC_RUN_ONE_LINE10, OnRunClicked)
  ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_EDIT_ONE_LINE, IDC_EDIT_ONE_LINE10, OnEnKillfocusEditOneLine)
  ON_CONTROL_RANGE(EN_SETFOCUS,IDC_EDIT_ONE_LINE, IDC_EDIT_ONE_LINE10, OnEnSetfocusEditOneLine)
  ON_WM_SIZE()
  ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL COneLineScript::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  int ind, lastOne = 0;

  // Here is the obscure set of information needed to reposition the Run button and resize
  // the text box when the width changes
  CRect wndRect, editRect, runRect, clientRect;
  GetClientRect(clientRect);
  GetWindowRect(wndRect);
  int iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  for (ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    m_butRun[ind].GetWindowRect(runRect);
    m_iRunTop[ind] = (runRect.top - wndRect.top) -
      (wndRect.Height() - clientRect.Height()) + iXoffset;
    if (!m_strOneLine[ind].IsEmpty())
      lastOne = ind;
  }
  m_iRunLeftOrig = runRect.left - wndRect.left - iXoffset;
  m_editOneLine[0].GetWindowRect(editRect);
  m_iWinXorig = wndRect.Width();
  m_iEditXorig = editRect.Width();
  m_iEditHeight = editRect.Height();
  m_statCompletions.GetWindowRect(editRect);
  m_iCompOffset = (editRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset;
  m_iCompLeft = editRect.left - wndRect.left - iXoffset;
  m_iCompWidthOrig = editRect.Width();

  // Resize to a bit longer than the last non-empty string: this is overriden by the
  // placement once that exists in settings
  if (lastOne < MAX_ONE_LINE_SCRIPTS - 1) {
    ind = wndRect.Height() + m_iRunTop[lastOne] - m_iRunTop[MAX_ONE_LINE_SCRIPTS - 1] +
      mWinApp->ScaleValueForDPI(5);
    SetWindowPos(NULL, 0, 0, wndRect.Width(), ind, SWP_NOMOVE);
  }
  UpdateData(false);
  mInitialized = true;
  SetDefID(45678);
  Invalidate();
  return FALSE;
}

// COneLineScript message handlers
void COneLineScript::OnOK()
{
}

// Run one of them
void COneLineScript::OnRunClicked(UINT nID)
{
  int ind = nID - IDC_RUN_ONE_LINE1;
  UpdateData(true);
  mMacros[MAX_MACROS + ind] = m_strOneLine[ind];
  if (mMacros[MAX_MACROS + ind].Find("#!Python") == 0) {
    if (mMacros[MAX_MACROS + ind].Find("import serialem") < 0 &&
      mMacros[MAX_MACROS + ind].Find("from serialem import") < 0)
      mMacros[MAX_MACROS + ind].Replace("#!Python", "#!Python;import serialem as sem");
    if (mMacros[MAX_MACROS + ind].Find(";   ") < 0)
      while (mMacros[MAX_MACROS + ind].Replace("; ", ";") > 0) {}
  }
  mMacros[MAX_MACROS + ind].Replace(";", "\r\n");
  if (!m_strOneLine[ind].IsEmpty()) {
    mWinApp->mMacroProcessor->Run(MAX_MACROS + ind);
    if (mWinApp->mMacroProcessor->GetKeepOneLineFocus())
      mWinApp->mMacroProcessor->SetFocusedWndWhenSavedStatus(m_editOneLine[ind].m_hWnd);
  }
  mWinApp->RestoreViewFocus();
}

// Closing the window: transfer the strings
void COneLineScript::OnCancel()
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  if (mWinApp->mMacroProcessor->DoingMacro())
    return;
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    mMacros[MAX_MACROS + ind] = m_strOneLine[ind];
    mMacros[MAX_MACROS + ind].Replace(";", "\r\n");
  }
  mWinApp->mMacroProcessor->OneLineClosing();
  DestroyWindow();
}

void COneLineScript::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

// New size: lengthen the text boxes, reposition the buttons
void COneLineScript::OnSize(UINT nType, int cx, int cy)
{
  if (!mInitialized)
    return;
  CRect wndRect, editRect, runRect;
  int ind;
  CDialog::OnSize(nType, cx, cy);
  GetWindowRect(wndRect);
  int delta = wndRect.Width() - m_iWinXorig;
  int newx = B3DMAX(5, m_iEditXorig + delta);
  for (ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    m_editOneLine[ind].SetWindowPos(NULL, 0, 0, newx, m_iEditHeight,
      SWP_NOZORDER | SWP_NOMOVE);
    m_butRun[ind].SetWindowPos(NULL, B3DMAX(5, m_iRunLeftOrig + delta), m_iRunTop[ind], 0,
      0, SWP_NOZORDER | SWP_NOSIZE);
  }
  m_statCompletions.GetWindowRect(editRect);
  m_statCompletions.SetWindowPos(NULL, m_iCompLeft, m_iCompOffset,
    m_iCompWidthOrig + delta, editRect.Height(), SWP_NOZORDER);
  m_statCompletions.RedrawWindow();
}

// Translate tab to our completion character.  It is good on US standard keyboards only
// Keep track of Ctrl so Ctrl A and Ctrl Z can be implemented
BOOL COneLineScript::PreTranslateMessage(MSG* pMsg)
{
  CEdit *edit;
  int length;
  CString text;
  static bool ctrlPressed = false;
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) {
    mWinApp->RestoreViewFocus();
    return FALSE;
  }
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    pMsg->wParam = VK_OEM_3;
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_CONTROL)
    ctrlPressed = true;
  if (pMsg->message == WM_KEYUP && pMsg->wParam == VK_CONTROL)
    ctrlPressed = false;
  if (pMsg->message == VK_PRIOR || pMsg->message == VK_NEXT)
    return FALSE;
  if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_DOWN ||pMsg->wParam == VK_UP)) {
    if ((pMsg->wParam == VK_DOWN && mLineWithFocus < MAX_ONE_LINE_SCRIPTS - 1) ||
      (pMsg->wParam == VK_UP && mLineWithFocus > 0)) {
      edit = (CEdit *)GetDlgItem(mLineWithFocus + IDC_EDIT_ONE_LINE +
        (pMsg->wParam == VK_UP ? -1 : 1));
      edit->SetFocus();
      edit->GetWindowText(text);
      length = text.GetLength();
      edit->SetSel(length, length);
    }
    pMsg->wParam = 0x00;
  }
  if (ctrlPressed && pMsg->message == WM_KEYDOWN && mLineWithFocus >= 0 &&
    mLineWithFocus < 10) {
    edit = (CEdit *)GetDlgItem(mLineWithFocus + IDC_EDIT_ONE_LINE);
    if (pMsg->wParam == 'A')
      edit->SetSel(0xFFFF0000);
    if (pMsg->wParam == 'Z')
      edit->Undo();
    if (pMsg->wParam == 'U') {
      edit->SetSel(0xFFFF0000);
      edit->Clear();
      m_strOneLine[mLineWithFocus] = "";
    }
    if (pMsg->wParam == 'C')
      edit->Copy();
    if (pMsg->wParam == 'V')
      edit->Paste();
    if (pMsg->wParam == 'X')
      edit->Cut();
    pMsg->wParam = 0x00;
  }
  return CDialog::PreTranslateMessage(pMsg);
}

// Enable/disable the buttons
void COneLineScript::Update()
{
  BOOL runnable = mWinApp->mMacroProcessor->MacroRunnable(MAX_MACROS);
  BOOL busy = mWinApp->DoingTasks();
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    m_butRun[ind].EnableWindow(runnable);
    m_editOneLine[ind].EnableWindow(!busy);
  }
}

// Process a character in a line
void COneLineScript::OnEnChangeEditOneLine(UINT nID)
{
  int sel1, sel2, ind = nID - IDC_EDIT_ONE_LINE;
  bool setCompletions, completing;
  UpdateData(true);
  m_editOneLine[ind].GetSel(sel1, sel2);
  CMacroEditer::HandleCompletionsAndIndent(m_strOneLine[ind], m_strCompletions, sel2,
    setCompletions, completing, true, mLineForSignature, 0);
  if (setCompletions)
    SetDlgItemText(IDC_STAT_COMPLETIONS, m_strCompletions);
  if (completing) {
    UpdateData(false);
    m_editOneLine[ind].SetSel(sel2, sel2);
  }
}

// Set Run to default when a line gets focus
// Keep track of which line has the focus so outline, Ctrl, Ctrl Z work
void COneLineScript::OnEnSetfocusEditOneLine(UINT nID)
{
  CButton *but;
  if (!mInitialized || mWinApp->GetStartingProgram())
    return;
  mLineWithFocus = nID - IDC_EDIT_ONE_LINE;
  SetDefID(mLineWithFocus + IDC_RUN_ONE_LINE1);
  but = (CButton *)GetDlgItem(mLineWithFocus + IDC_RUN_ONE_LINE1);
  but->SetFont(mWinApp->GetBoldFont(GetDlgItem(IDC_STAT_COMPLETIONS)));
  but->SetButtonStyle(BS_DEFPUSHBUTTON);
  mLineForSignature = -1;
}

// When focus lost, set line to -1
void COneLineScript::OnEnKillfocusEditOneLine(UINT nID)
{
  CButton *but;
  if (!mInitialized || mWinApp->GetStartingProgram())
    return;
  if (mLineWithFocus >= 0) {
    but = (CButton *)GetDlgItem(mLineWithFocus + IDC_RUN_ONE_LINE1);
    but->SetFont((GetDlgItem(IDC_STAT_COMPLETIONS))->GetFont());
  }
  mLineWithFocus = -1;
  SetDefID(45678);
}


