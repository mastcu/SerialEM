// BaseDlg.cpp:          Base class to handle tool tips and help button
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\BaseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBaseDlg dialog


CBaseDlg::CBaseDlg(UINT inIDD, CWnd* pParent /*=NULL*/)
  : CDialog(inIDD, pParent)
{
  //{{AFX_DATA_INIT(CBaseDlg)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  mIDD = inIDD;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mNonModal = false;
  mNumIDsToHide = 0;
  mIDToSaveTop = -1;
}


void CBaseDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CBaseDlg)
    // NOTE: the ClassWizard will add DDX and DDV calls here
  DDX_Control(pDX, IDC_BUTHELP, m_butHelp);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBaseDlg, CDialog)
  //{{AFX_MSG_MAP(CBaseDlg)
    // NOTE: the ClassWizard will add message map macros here
  ON_BN_CLICKED(IDC_BUTHELP, OnButhelp)
  ON_WM_LBUTTONDOWN()
  ON_WM_MBUTTONDOWN()
  ON_WM_RBUTTONDOWN()
  ON_WM_LBUTTONUP()
  ON_WM_MBUTTONUP()
  ON_WM_RBUTTONUP()
  ON_WM_LBUTTONDBLCLK()
  ON_WM_MBUTTONDBLCLK()
  ON_WM_RBUTTONDBLCLK()
  //}}AFX_MSG_MAP
  ON_NOTIFY_EX_RANGE( TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBaseDlg message handlers

BOOL CBaseDlg::OnInitDialog() 
{
  if (!mWinApp->GetDisplayNotTruly120DPI())
    mSetDPI.Attach(AfxFindResourceHandle(MAKEINTRESOURCE(mIDD), RT_DIALOG),
                  m_hWnd, mIDD, B3DNINT(1.25 *mWinApp->GetSystemDPI()));
  CDialog::OnInitDialog();
  EnableToolTips(true);
  return TRUE;
}

BOOL CBaseDlg::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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

void CBaseDlg::OnButhelp() 
{
  mWinApp->OnHelp(); 
  m_butHelp.SetButtonStyle(BS_PUSHBUTTON);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}

// For easy editing of dialog text by window pointer or ID
void CBaseDlg::ReplaceWindowText(CWnd * wnd, const char * fromText, CString toText)
{
  CString str;
  wnd->GetWindowText(str);
  str.Replace(fromText, (LPCTSTR)toText);
  wnd->SetWindowText(str);
}

void CBaseDlg::ReplaceDlgItemText(int nID, const char * fromText, CString toText)
{
  CString str;
  GetDlgItemText(nID, str);
  str.Replace(fromText, (LPCTSTR)toText);
  SetDlgItemText(nID, str);
}

// For enabling radio buttons particularly
void CBaseDlg::EnableDlgItem(int nID, BOOL enable)
{
  CWnd *but = GetDlgItem(nID);
  if (but)
    but->EnableWindow(enable);
}

// For showing/hiding dialog item by ID
void CBaseDlg::ShowDlgItem(int nID, bool show)
{
  CWnd *but = GetDlgItem(nID);
  if (but)
    but->ShowWindow(show ? SW_SHOW : SW_HIDE);
}

// Gets a new spinner value within the given limits and sets some formatted text with the
// value
void CBaseDlg::FormattedSpinnerValue(NMHDR *pNMHDR, LRESULT *pResult, int lowerLim,
  int upperLim, int &oldNewVal, CString &str, const char *format)
{
  if (NewSpinnerValue(pNMHDR, pResult, oldNewVal, lowerLim, upperLim, oldNewVal))
    return;
  UpdateData(true);
  str.Format(format, oldNewVal);
  UpdateData(false);
  *pResult = 0;
}

// If nonModal, capture all the stray mouse events not on a control and yield focus
void CBaseDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
  CDialog::OnLButtonDown(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnMButtonDown(UINT nFlags, CPoint point) 
{
  CDialog::OnMButtonDown(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnRButtonDown(UINT nFlags, CPoint point) 
{
  CDialog::OnRButtonDown(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
  CDialog::OnLButtonUp(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnMButtonUp(UINT nFlags, CPoint point) 
{
  CDialog::OnMButtonUp(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnRButtonUp(UINT nFlags, CPoint point) 
{
  CDialog::OnRButtonUp(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
  CDialog::OnLButtonDblClk(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnMButtonDblClk(UINT nFlags, CPoint point) 
{
  CDialog::OnMButtonDblClk(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}
void CBaseDlg::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
  CDialog::OnRButtonDblClk(nFlags, point);
  if (mNonModal)
    mWinApp->RestoreViewFocus();
}

// Setup call for the panel tables
// The first item in the section for panel is recorded as the panelStart so it better be
// the top item; probably best to end panel section with the bottom item too
void CBaseDlg::SetupPanelTables(int * idTable, int * leftTable, int * topTable, 
                                int * numInPanel, int * panelStart)
{
  CRect wndRect, clientRect, idRect;
  CWnd *wnd;
  int iXoffset, iYoffset, index;

  GetClientRect(clientRect);
  GetWindowRect(wndRect);
  mBasicWidth = wndRect.Width();
  iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  iYoffset = (wndRect.Height() - clientRect.Height()) - iXoffset;

  // Get indexes to panels in table and top/left positions in settable form
  index = 0;
  mNumPanels = 0;
  while (idTable[index] != TABLE_END) {
    panelStart[mNumPanels] = index;
    numInPanel[mNumPanels] = 0;
    while (idTable[index] != PANEL_END) {
      numInPanel[mNumPanels]++;
      if (idTable[index]) {
        wnd = GetDlgItem(idTable[index]);
        wnd->GetWindowRect(idRect);
        leftTable[index] = idRect.left - wndRect.left - iXoffset;
        topTable[index] = idRect.top - wndRect.top - iYoffset;
        if (numInPanel[mNumPanels] == 1 && wndRect.Height() < 3)
          topTable[index] += 4;
      }
      index++;
    }
    index++;
    mNumPanels++;
  }
}

void CBaseDlg::AdjustPanels(BOOL *states, int *idTable, int *leftTable, int *topTable, 
                            int *numInPanel, int *panelStart, int numCameras)
{
  bool draw;
  int curTop = topTable[0];
  CRect rect, winRect;
  int panel, panelTop, index, id;
  CWnd *wnd;
  HDWP positions;

  // Count the total windows needed for the positioning
  index = 1;
  mSavedTopPos = -1;
  for (panel = 0; panel < mNumPanels; panel++)
    index += numInPanel[panel];
  positions = BeginDeferWindowPos(index);
  if (!positions)
    return;

  for (panel = 0; panel < mNumPanels; panel++) {
    panelTop = topTable[panelStart[panel]];
    for (index = panelStart[panel];
      index < panelStart[panel] + numInPanel[panel]; index++) {
      wnd = GetDlgItem(idTable[index]);
      draw = true;
      for (id = (numCameras > 1 ? numCameras : 0); id < MAX_DLG_CAMERAS; id++)
        if (idTable[index] == IDC_RCAMERA1 + id)
          draw = false;
      for (id = 0; id < mNumIDsToHide; id++)
        if (idTable[index] == mIDsToHide[id])
          draw = false;
      if (states[panel] && draw) {
        if (idTable[index] == mIDToSaveTop)
          mSavedTopPos = curTop + topTable[index] - panelTop;
        wnd->ShowWindow(SW_SHOW);
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index], 
          curTop + topTable[index] - panelTop, 0, 0, 
          SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
      } else
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, 0, 0, 0, 0, 
          SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
    }
    if (states[panel] && panel < mNumPanels - 1)
      curTop += topTable[panelStart[panel+1]] - topTable[panelStart[panel]];
  }

  // Make all those changes occur then resize dialog to end below the last button
  EndDeferWindowPos(positions);
  wnd->GetWindowRect(rect);
  GetWindowRect(winRect);
  mSetToHeight = rect.bottom + 8 - winRect.top;
  SetWindowPos(NULL, 0, 0, mBasicWidth, mSetToHeight, SWP_NOMOVE);
}
