// BaseDlg.cpp:    Base class to handle tool tips, help button, panels, other utilities
//
// Copyright (C) 2003-2021 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\BaseDlg.h"
#include "Shared\b3dutil.h"

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
  mNumPanels = 0;
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

  // At 96 DPI, pixel coordinates are about twice the dialog units
  // Count the total windows needed for the positioning
  mSameLineCrit = mWinApp->ScaleValueForDPI(12);
  mBottomDrawMargin = mWinApp->ScaleValueForDPI(3);
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
  FixButtonFocus(m_butHelp);
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

// Draw a box around a button.
// Non-tool dialogs need a big offset that does NOT need DPI scaling
void CBaseDlg::DrawButtonOutline(CPaintDC &dc, CWnd *but, int thickness,
  COLORREF color, int offset)
{
  CRect winRect, clientRect, butRect;
  int iLeft, iTop, border;
  thickness = (int)(thickness * ((CSerialEMApp *)AfxGetApp())->GetScalingForDPI());
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  border = (winRect.Width() - clientRect.Width()) / 2;
  but->GetWindowRect(&butRect);
  iLeft = (butRect.left - winRect.left) + offset - thickness;
  iTop = butRect.top - winRect.top - (winRect.Height() - clientRect.Height() - border) -
    (thickness - 1);
  CRect dcRect(iLeft, iTop, iLeft + butRect.Width() + (thickness + 1),
    iTop + butRect.Height() + (thickness + 1));
  CPen pen;
  CBrush brush;
  pen.CreatePen(PS_SOLID, thickness, color);

  // "transparent" brush
  brush.CreateStockObject(HOLLOW_BRUSH);
  dc.SelectObject(&pen);
  dc.SelectObject(&brush);
  dc.Rectangle(&dcRect);
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

// Fix a button having focus after being pressed, instead of OK button
// ButtonStyle used to work, Win 10 seems to need SetFocus
void CBaseDlg::FixButtonFocus(CButton &button)
{
  button.SetButtonStyle(BS_PUSHBUTTON);
  SetFocus();
}

// Replacement function for DDV_MinMAxFlaot that needs to get used for nonmodal dialogs to
// avoid double validation messages or worse
void CBaseDlg::MinMaxFloat(UINT nID, float &value, float minVal, float maxVal,
  const char *descrip)
{
  CString str;
  if (value >= minVal && value <= maxVal)
    return;
  B3DCLAMP(value, minVal, maxVal);
  str.Format("%g", value);
  SetDlgItemText(nID, (LPCTSTR)str);
  str.Format("%s must be between %g and %g", descrip, minVal, maxVal);
  AfxMessageBox(str, MB_EXCLAME);
}

// Replacement for DDV_MinMaxInt
void CBaseDlg::MinMaxInt(UINT nID, int &value, int minVal, int maxVal,
  const char *descrip)
{
  CString str;
  if (value >= minVal && value <= maxVal)
    return;
  B3DCLAMP(value, minVal, maxVal);
  str.Format("%d", value);
  SetDlgItemText(nID, (LPCTSTR)str);
  str.Format("%s must be between %d and %d", descrip, minVal, maxVal);
  AfxMessageBox(str, MB_EXCLAME);
}

// Setup call for the panel tables
// The first item in the section for panel is recorded as the panelStart so it better be
// the top item; probably best to end panel section with the bottom item too
void CBaseDlg::SetupPanelTables(int *idTable, int *leftTable, int *topTable, 
  int *numInPanel, int *panelStart, int *heightTable, int sortStart)
{
  CRect wndRect, clientRect, idRect;
  CWnd *wnd;
  int iXoffset, iYoffset, index, ii, jj, indEnd, temp;
  std::set<int> *lineHideIDs = mWinApp->GetLineHideIDs();
  std::set<int> *basicLineHideIDs = mWinApp->GetBasicLineHideIDs();

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
        if (heightTable)
          heightTable[index] = idRect.Height();
        if (numInPanel[mNumPanels] == 1 && wndRect.Height() < 3)
          topTable[index] += 4;
      }
      index++;
    }

    // Order them by increasing top so dropping of elements/lines works
    // If two are equal, make one that drops a line come first
    indEnd = panelStart[mNumPanels] + numInPanel[mNumPanels];
    for (ii = panelStart[mNumPanels] + sortStart; ii < indEnd - 1; ii++) {
      for (jj = ii + 1; jj < indEnd; jj++) {
        if (topTable[jj] < topTable[ii] || ((topTable[jj] == topTable[ii]) &&
          (lineHideIDs->count(idTable[jj]) || basicLineHideIDs->count(idTable[jj])))) {
          B3DSWAP(topTable[jj], topTable[ii], temp);
          B3DSWAP(leftTable[jj], leftTable[ii], temp);
          B3DSWAP(idTable[jj], idTable[ii], temp);
        }
      }
    }
    index++;
    mNumPanels++;
  }
}

void CBaseDlg::AdjustPanels(BOOL *states, int *idTable, int *leftTable, int *topTable, 
  int *numInPanel, int *panelStart, int numCameras, int *heightTable)
{
  bool draw, drop, droppingLine;
  int curTop = topTable[0];
  CRect rect, winRect;
  int panel, panelTop, index, id, cumulDrop, firstDropped, topPos, drawnMaxBottom;
  int topAtLastDraw;
  CWnd *wnd, *lastWnd;
  HDWP positions;

  index = 1;
  mSavedTopPos = -1;
  for (panel = 0; panel < mNumPanels; panel++)
    index += numInPanel[panel];
  positions = BeginDeferWindowPos(index);
  if (!positions)
    return;

  for (panel = 0; panel < mNumPanels; panel++) {
    panelTop = topTable[panelStart[panel]];
    cumulDrop = 0;
    firstDropped = -1;
    droppingLine = false;
    drawnMaxBottom = 0;
    topAtLastDraw = 0;
    for (index = panelStart[panel];
      index < panelStart[panel] + numInPanel[panel]; index++) {
      wnd = GetDlgItem(idTable[index]);
      draw = true;
      drop = false;

      // Hide cameras past the number that exist
      for (id = (numCameras > 1 ? numCameras : 0); id < MAX_DLG_CAMERAS; id++)
        if (idTable[index] == IDC_RCAMERA1 + id)
          draw = false;

      // Hide ones that the dialog wants to hide
      for (id = 0; id < mNumIDsToHide; id++)
        if (idTable[index] == mIDsToHide[id])
          draw = false;

      // Drops ones that are in the users list to drop or the dialog has set to drop
      ManageDropping(topTable, index, idTable[index], topAtLastDraw, cumulDrop,
        firstDropped, droppingLine, drop);

      // draw
      if (states[panel] && draw && !drop) {
        topPos = (curTop - cumulDrop) + topTable[index] - panelTop;
        if (heightTable)
          ACCUM_MAX(drawnMaxBottom, topPos + heightTable[index] + mBottomDrawMargin);
        if (idTable[index] == mIDToSaveTop)
          mSavedTopPos = topPos;
        topAtLastDraw = topTable[index];
        //wnd->ShowWindow(SW_SHOW);
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index], 
          topPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
        lastWnd = wnd;
      } else

        // Or hide
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, 0, 0, 0, 0, 
          SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
    }

    // If last one in panel dropped, add to cumulative distance
    if (firstDropped >= 0 &&
      B3DABS(topTable[firstDropped] - topAtLastDraw) > mSameLineCrit)
      cumulDrop += topTable[panelStart[panel + 1]] - topTable[firstDropped];

    // Set new current top unless panel was closed
    if (states[panel] && panel < mNumPanels - 1)
      curTop = B3DMAX(drawnMaxBottom, curTop + (topTable[panelStart[panel+1]] - 
        panelTop) - cumulDrop);
  }

  // Make all those changes occur then resize dialog to end below the last button
  EndDeferWindowPos(positions);
  lastWnd->GetWindowRect(rect);
  GetWindowRect(winRect);
  mSetToHeight = rect.bottom + 8 - winRect.top;
  SetWindowPos(NULL, 0, 0, mBasicWidth, mSetToHeight, SWP_NOMOVE);
}

// For a given dialog item, determine if it is to be dropped and manage the state of
// cumulative drop, first one dropped in group, whether a line is being dropped
void CBaseDlg::ManageDropping(int *topTable, int index, int nID, int topAtLastDraw,
  int &cumulDrop, int &firstDropped, bool &droppingLine, bool &drop)
{
  bool lineDrop;
  int ind;

  // Drop ones that are in the users list to drop or the dialog has set to drop
  drop = drop || mWinApp->IsIDinHideSet(nID);
  lineDrop = mWinApp->IsIDinLineHides(nID);
  for (ind = 0; ind < (int)mIDsToDrop.size() && !drop; ind++)
    if (nID == mIDsToDrop[ind])
      drop = true;

  // If one is to be dropped by those tests of within the Y distance to be on the same
  // line as the first dropped one, then drop it
  if (drop || lineDrop || (firstDropped >= 0 && droppingLine &&
    B3DABS(topTable[index] - topTable[firstDropped]) <= mSameLineCrit)) {

    // But if there is already dropping in play from a previous line, add to 
    // cumulative drop distance and reset index to this one
    if ((drop || lineDrop) && firstDropped >= 0) {
      if (lineDrop || (topTable[index] - topTable[firstDropped] > mSameLineCrit &&
        (!firstDropped || B3DABS(topTable[firstDropped] - topAtLastDraw) > 
        mSameLineCrit))) {
        cumulDrop += topTable[index] - topTable[firstDropped];
        firstDropped = -1;
      }
    }

    // Record index of first dropped one
    drop = true;
    if (firstDropped < 0) {
      firstDropped = index;
      droppingLine = lineDrop;
    }
    /*PrintfToLog("ID %d ind %d drop %d line %d first %d cumul %d", nID, index, 
    drop ? 1 : 0, droppingLine ? 1 : 0, firstDropped, cumulDrop);*/
  }

  // First one not dropped: add distance back to first one dropped to the cum distance
  if (firstDropped >= 0 && !drop) {
    if (topTable[index] - topTable[firstDropped] > mSameLineCrit &&
      (!firstDropped || B3DABS(topTable[firstDropped] - topAtLastDraw) > mSameLineCrit))
      cumulDrop += topTable[index] - topTable[firstDropped];
    firstDropped = -1;
    droppingLine = false;
  }
}

// Hide or show items in the hideable set
void CBaseDlg::ManageHideableItems(UINT *hideableIDs, int numHideable)
{
  CWnd *but;
  for (int ind = 0; ind < numHideable; ind++) {
    but = GetDlgItem(hideableIDs[ind]);
    if (but)
      but->ShowWindow(mWinApp->IsIDinHideSet(hideableIDs[ind]) ? SW_HIDE : SW_SHOW);
  }
}

