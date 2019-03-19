// ToolDlg.cpp:           Base class of control panels handles all common features
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
// A base class for tool dialogs that have an open/close button and
// an optional more button to open a more detailed section

#include "stdafx.h"
#include "SerialEM.h"
#include "ToolDlg.h"
#include "XFolderDialog/XWinVer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// In order to measure title bar height the first time a panel floats
static bool sFirstFloatDlg = true;

/////////////////////////////////////////////////////////////////////////////
// CToolDlg dialog


CToolDlg::CToolDlg(UINT inIDD, CWnd* pParent /*=NULL*/)
  : CDialog(inIDD, pParent)
{
  //{{AFX_DATA_INIT(CToolDlg)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  mMidHeight = 0;
  mState = TOOL_OPENCLOSED | TOOL_FULLOPEN; 
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mBorderColor = RGB(192, 192, 192);
  mIDD = inIDD;
}


void CToolDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CToolDlg)
    // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolDlg, CDialog)
  //{{AFX_MSG_MAP(CToolDlg)
  ON_BN_CLICKED(IDC_BUTOPEN, OnButopen)
  ON_BN_CLICKED(IDC_BUTMORE, OnButmore)
  ON_WM_LBUTTONDOWN()
  ON_WM_MBUTTONDOWN()
  ON_WM_RBUTTONDOWN()
  ON_WM_LBUTTONUP()
  ON_WM_MBUTTONUP()
  ON_WM_RBUTTONUP()
  ON_WM_LBUTTONDBLCLK()
  ON_WM_MBUTTONDBLCLK()
  ON_WM_RBUTTONDBLCLK()
  ON_BN_CLICKED(IDC_BUTHELP, OnHelp)
  ON_BN_CLICKED(IDC_BUTFLOATDOCK, OnButfloat)
  ON_WM_MOVE()
  ON_WM_HELPINFO()
  ON_WM_CLOSE()
  ON_WM_PAINT()
  //}}AFX_MSG_MAP
  ON_NOTIFY_EX_RANGE( TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolDlg message handlers
// Set state of button and state value
void CToolDlg::SetOpenClosed(int inState)
{
  CRect rcWin;
  CButton *openBut;
  int dockHeight, floatHeight;
  mState = inState;
  if (mState & TOOL_OPENCLOSED) {
    SetDlgItemText(IDC_BUTOPEN, "-");
  } else {
    SetDlgItemText(IDC_BUTOPEN, "+");
  }

  if (GetDlgItem(IDC_BUTMORE) != NULL) {
    if (mState & TOOL_FULLOPEN) 
      SetDlgItemText(IDC_BUTMORE, "-");
    else
      SetDlgItemText(IDC_BUTMORE, "+");
  }

  if (mState & TOOL_FLOATDOCK) {
    if (sFirstFloatDlg) {
      openBut = (CButton *)GetDlgItem(IDC_BUTOPEN);
      dockHeight = CurrentButHeight(openBut);
    }
    ModifyStyle(0, WS_CAPTION);
    ModifyStyleEx(0, WS_EX_TOOLWINDOW);
    SetDlgItemText(IDC_BUTFLOATDOCK, "D");

    // This is needed to get a window redraw and avoid blue covering top controls
    // with XP style
    GetWindowRect( rcWin );
    SetWindowPos(NULL, rcWin.left, rcWin.top, rcWin.right - rcWin.left,
      rcWin.bottom - rcWin.top, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOZORDER);
    if (sFirstFloatDlg) {
      floatHeight = CurrentButHeight(openBut);
      sFirstFloatDlg = false;
      mWinApp->SetToolTitleHeight(1 + floatHeight - dockHeight);
    }
  } else {
    ModifyStyle(WS_CAPTION, 0);
    SetDlgItemText(IDC_BUTFLOATDOCK, "F");
  }
}

void CToolDlg::ToggleState(int inFlag)
{
  if (mState & inFlag)
    mState &= ~inFlag;
  else
    mState |= inFlag;
  SetOpenClosed(mState);
  mWinApp->DialogChangedState(this, mState);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// When the open - close button is pushed, change state of button and
// inform the tool dialog manager
void CToolDlg::OnButopen() 
{
  ToggleState(TOOL_OPENCLOSED);
}

// When the more button is pushed, change state of button and
// inform the tool dialog manager
void CToolDlg::OnButmore() 
{
  ToggleState(TOOL_FULLOPEN);
}

void CToolDlg::OnButfloat() 
{
  ToggleState(TOOL_FLOATDOCK);
}

// Get the height of the more button if there is one
int CToolDlg::GetMidHeight()
{
  // First make font bigger for both buttons
  CRect rect;
  CButton *butOpen = (CButton *)GetDlgItem(IDC_BUTOPEN);
  butOpen->GetWindowRect(&rect);
  mButFont.CreateFont((rect.Height() + 4), 0, 0, 0, FW_BOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  butOpen->SetFont(&mButFont);
  CButton *butMore = (CButton *)GetDlgItem(IDC_BUTMORE);
  if (!butMore)
    return 0;
  butMore->SetFont(&mButFont);
  return CurrentButHeight(butMore);
}

// Get the mid-height or equivalent value for a different button
int CToolDlg::CurrentButHeight(CButton *butMore)
{
  if (!butMore)
    butMore = (CButton *)GetDlgItem(IDC_BUTMORE);
  if (!butMore)
    return 0;
  CRect winRect;
  CRect butRect;
  GetWindowRect(&winRect);
  butMore->GetWindowRect(&butRect);
  return 2 + butRect.bottom - winRect.top;
}

// Capture all the stray mouse events not on a control and yield focus
void CToolDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnLButtonDown(nFlags, point);
}

void CToolDlg::OnMButtonDown(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnMButtonDown(nFlags, point);
}

void CToolDlg::OnRButtonDown(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
    
  CDialog::OnRButtonDown(nFlags, point);
}

void CToolDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();

  CDialog::OnLButtonUp(nFlags, point);
}

void CToolDlg::OnMButtonUp(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();

  CDialog::OnMButtonUp(nFlags, point);
}

void CToolDlg::OnRButtonUp(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnRButtonUp(nFlags, point);
}

void CToolDlg::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnLButtonDblClk(nFlags, point);
}

void CToolDlg::OnMButtonDblClk(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnMButtonDblClk(nFlags, point);
}

void CToolDlg::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
  mWinApp->RestoreViewFocus();
  
  CDialog::OnRButtonDblClk(nFlags, point);
}

BOOL CToolDlg::OnInitDialog() 
{
  if (mWinApp->GetDisplayNot120DPI())
    mSetDPI.Attach(AfxFindResourceHandle(MAKEINTRESOURCE(mIDD), RT_DIALOG),
                    m_hWnd, mIDD, 120.0);
  CDialog::OnInitDialog();

  // Make the font smaller for the title and "more" message
  CStatic *topLine = (CStatic *)GetDlgItem(IDC_STATTOPLINE);
  if (topLine) {
    // Get the right font for the OS (12/12/04: going to Win NT is probably hopeless now)
    if (Is2000OrGreater())
      mBigFontName = "Microsoft Sans Serif";
    else
      mBigFontName = "MS Sans Serif";

    // And the right small font for laptop or 120 DPI
    mLittleFont = mWinApp->GetLittleFont();

    topLine->SetFont(mLittleFont);
    topLine = (CStatic *)GetDlgItem(IDC_STATMORE);
    if (topLine)
      topLine->SetFont(mLittleFont);
    CButton *helpBut = (CButton *)GetDlgItem(IDC_BUTHELP);
    if (helpBut)
      helpBut->SetFont(mLittleFont);
    helpBut = (CButton *)GetDlgItem(IDC_BUTFLOATDOCK);
    if (helpBut)
      helpBut->SetFont(mLittleFont);
  }
  EnableToolTips(true);

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Tool windows can be closed by escape if they get focus!
// Include an invisible cancel button to get this message
void CToolDlg::OnCancel() 
{
  
  // CDialog::OnCancel();
}

BOOL CToolDlg::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
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

void CToolDlg::OnHelp() 
{
  mWinApp->OnHelp();
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CToolDlg::OnMove(int x, int y) 
{
  CDialog::OnMove(x, y);
  
  if (mState & TOOL_FLOATDOCK)
    mWinApp->RestoreViewFocus();  
}

void CToolDlg::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  DrawSideBorders(dc);
  
  // Do not call CDialog::OnPaint() for painting messages
}

// So individual panels can do things too
void CToolDlg::DrawSideBorders(CPaintDC &dc)
{
  CRect clientRect;
  int lrSize = 4;
  GetClientRect(&clientRect);
  dc.FillSolidRect(0, 0, lrSize, clientRect.Height(), mBorderColor);
  dc.FillSolidRect(clientRect.Width() - lrSize, 0, clientRect.Width(),
    clientRect.Height(), mBorderColor);
}

// Draw a box around a button
void CToolDlg::DrawButtonOutline(CPaintDC &dc, CWnd *but, int thickness, COLORREF color)
{
  CRect winRect, clientRect, butRect;
  int iLeft, iTop, border;
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  border = (winRect.Width() - clientRect.Width()) / 2;
  but->GetWindowRect(&butRect);
  iLeft = butRect.left - winRect.left - 5;
  iTop = butRect.top - winRect.top - (winRect.Height() - clientRect.Height() - border) -2;
  CRect dcRect(iLeft, iTop, iLeft + butRect.Width() + 4, iTop + butRect.Height() + 4);
  CPen pen;
  CBrush brush;
  pen.CreatePen(PS_SOLID, 3, color);

  // "transparent" brush
  brush.CreateStockObject(HOLLOW_BRUSH);
  dc.SelectObject(&pen);
  dc.SelectObject(&brush); 
  dc.Rectangle(&dcRect);
}

void CToolDlg::SetBorderColor(COLORREF inColor)
{
  mBorderColor = inColor;
}

// For easy editing of dialog text by window pointer or ID
void CToolDlg::ReplaceWindowText(CWnd * wnd, const char * fromText, CString toText)
{
  CString str;
  wnd->GetWindowText(str);
  str.Replace(fromText, (LPCTSTR)toText);
  wnd->SetWindowText(str);
}

void CToolDlg::ReplaceDlgItemText(int nID, const char * fromText, CString toText)
{
  CString str;
  GetDlgItemText(nID, str);
  str.Replace(fromText, (LPCTSTR)toText);
  SetDlgItemText(nID, str);
}
