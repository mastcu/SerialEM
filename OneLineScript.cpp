// OneLineScript.cpp : Dialog for simple one-line scripts
//
// Copyright (C) 2018-2019 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "OneLineScript.h"
#include "MacroEditer.h"
#include "MacroProcessor.h"

// COneLineScript dialog

COneLineScript::COneLineScript(CWnd* pParent /*=NULL*/)
	: CBaseDlg(COneLineScript::IDD, pParent)
  , m_strCompletions(_T("Tab or  `  to complete command"))
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
  DDX_Control(pDX, IDC_EDIT_ONE_LINE, m_editOneLine[0]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE2, m_editOneLine[1]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE3, m_editOneLine[2]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE4, m_editOneLine[3]);
  DDX_Control(pDX, IDC_EDIT_ONE_LINE5, m_editOneLine[4]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE, m_strOneLine[0]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE2, m_strOneLine[1]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE3, m_strOneLine[2]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE4, m_strOneLine[3]);
  DDX_Text(pDX, IDC_EDIT_ONE_LINE5, m_strOneLine[4]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE1, m_butRun[0]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE2, m_butRun[1]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE3, m_butRun[2]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE4, m_butRun[3]);
  DDX_Control(pDX, IDC_RUN_ONE_LINE5, m_butRun[4]);
}


BEGIN_MESSAGE_MAP(COneLineScript, CDialog)
  ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_ONE_LINE, IDC_EDIT_ONE_LINE5, OnEnChangeEditOneLine)
  ON_CONTROL_RANGE(BN_CLICKED, IDC_RUN_ONE_LINE1, IDC_RUN_ONE_LINE5, OnRunClicked)
  ON_WM_SIZE()
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

  // Resize to a bit longer than the last non-empty string: this is overriden by the
  // placement once that exists in settings
  if (lastOne < MAX_ONE_LINE_SCRIPTS - 1) {
    ind = wndRect.Height() + m_iRunTop[lastOne] - m_iRunTop[MAX_ONE_LINE_SCRIPTS - 1] + 5;
    SetWindowPos(NULL, 0, 0, wndRect.Width(), ind, SWP_NOMOVE);
  }
  UpdateData(false);
  mInitialized = true;
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
  if (!m_strOneLine[ind].IsEmpty())
    mWinApp->mMacroProcessor->Run(MAX_MACROS + ind);
  mWinApp->RestoreViewFocus();
}

// Closing the window: transfer the strings
void COneLineScript::OnCancel()
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  if (mWinApp->mMacroProcessor->DoingMacro())
    return;
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++)
    mMacros[MAX_MACROS+ ind] = m_strOneLine[ind];
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
}

// Translate tab to our completion character.  It is good on US standard keyboards only
BOOL COneLineScript::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    pMsg->wParam = VK_OEM_3;
  return CDialog::PreTranslateMessage(pMsg);
}

// Enable/disable the buttons
void COneLineScript::Update()
{
  BOOL runnable = mWinApp->mMacroProcessor->MacroRunnable(MAX_MACROS);
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++)
    m_butRun[ind].EnableWindow(runnable);
}

// Process a character in a line
void COneLineScript::OnEnChangeEditOneLine(UINT nID)
{
  int sel1, sel2, ind = nID - IDC_EDIT_ONE_LINE;
  bool setCompletions, completing;
  UpdateData(true);
  m_editOneLine[ind].GetSel(sel1, sel2);
  CMacroEditer::HandleCompletionsAndIndent(m_strOneLine[ind], m_strCompletions, sel2, 
    setCompletions, completing);
  if (setCompletions)
    SetDlgItemText(IDC_STAT_COMPLETIONS, m_strCompletions);
  if (completing) {
    UpdateData(false);
    m_editOneLine[ind].SetSel(sel2, sel2);
  }
}

