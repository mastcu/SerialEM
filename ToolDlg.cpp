// ToolDlg.cpp:    Base class for common features of control panels, inherits CBaseDlg
//
// Copyright (C) 2003-2021 by the Regents of the University of
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
  : CBaseDlg(inIDD, pParent)
{
  //{{AFX_DATA_INIT(CToolDlg)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  mMidHeight = 0;
  mState = TOOL_OPENCLOSED | TOOL_FULLOPEN; 
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mBorderColor = RGB(192, 192, 192);
  mIDD = inIDD;
  mInitialized = false;
  mNonModal = true;
}


void CToolDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CToolDlg)
    // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CToolDlg)
  ON_BN_CLICKED(IDC_BUTOPEN, OnButopen)
  ON_BN_CLICKED(IDC_BUTMORE, OnButmore)
  ON_BN_CLICKED(IDC_BUTFLOATDOCK, OnButfloat)
  ON_WM_MOVE()
  ON_WM_HELPINFO()
  ON_WM_CLOSE()
  ON_WM_PAINT()
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolDlg message handlers
// Set state of button and state value
void CToolDlg::SetOpenClosed(int inState)
{
  BOOL states[3] = {true, false, false};
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

  // Adjust panels after handling the float/dock distinctions
  if (mNumPanels) {
    states[1] = mState & TOOL_OPENCLOSED;
    states[2] = states[1] && (mState & TOOL_FULLOPEN);
    AdjustPanels(states, mIdTable, mLeftTable, mTopTable, mNumInPanel, mPanelStart, 0,
      mHeightTable);
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
  if (!mButFont.m_hObject)
    mButFont.CreateFont((rect.Height() + 4), 0, 0, 0, FW_BOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  butOpen->SetFont(&mButFont);
  CButton *butMore = (CButton *)GetDlgItem(IDC_BUTMORE);
  if (!butMore)
    return 0;
  butMore->SetFont(&mButFont);
  mMidHeight = CurrentButHeight(butMore);
  return mMidHeight;
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

// If there is a more/less button, hide or show this and the Options label
// When hiding, close up the options if open
void CToolDlg::HideOrShowOptions(int showHide)
{
  CWnd *but = GetDlgItem(IDC_BUTMORE);
  if (!but)
    return;
  but->ShowWindow(showHide);
  but = GetDlgItem(IDC_STATMORE);
  if (but)
    but->ShowWindow(showHide);

  // If there are panels, this hiding is done by adding or removing from list of IDs
  // to hide for the dialog.  SetOpenClosed does the right thing if panels or not.
  if (showHide == SW_HIDE) {
    if (mNumPanels) {
      mIDsToHide[mNumIDsToHide++] = IDC_BUTMORE;
      mIDsToHide[mNumIDsToHide++] = IDC_STATMORE;
    }
    SetOpenClosed(mState & (~TOOL_FULLOPEN));
  } else if (mNumPanels && mNumIDsToHide > 1 &&
    mIDsToHide[mNumIDsToHide - 1] == IDC_STATMORE) {
    mNumIDsToHide -= 2;
    SetOpenClosed(mState);
  }
}

// Convenience function to call with all arguments and store pointers so subclass can
// do the update
void CToolDlg::SetupPanels(int *idTable, int *leftTable, int *topTable,
  int *heightTable, int sortStart)
{
  CBaseDlg::SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart,
    heightTable, sortStart);
  mIdTable = idTable;
  mLeftTable = leftTable;
  mTopTable = topTable;
  mHeightTable = heightTable;
}

BOOL CToolDlg::OnInitDialog() 
{
  
  CBaseDlg::OnInitDialog();

  // Make the font smaller for the title and "more" message
  CStatic *topLine = (CStatic *)GetDlgItem(IDC_STATTOPLINE);
  if (topLine) {
    // Get the right font for the OS (12/12/04: going to Win NT is probably hopeless now)
    if (Is2000OrGreater())
      mBigFontName = "Microsoft Sans Serif";
    else
      mBigFontName = "MS Sans Serif";

    // And the right small font for laptop or 120 DPI
    mLittleFont = mWinApp->GetLittleFont(topLine);

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

void CToolDlg::OnMove(int x, int y) 
{
  CBaseDlg::OnMove(x, y);
  
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
  int lrSize = (int)(4 * mWinApp->GetScalingForDPI());
  GetClientRect(&clientRect);
  dc.FillSolidRect(0, 0, lrSize, clientRect.Height(), mBorderColor);
  dc.FillSolidRect(clientRect.Width() - lrSize, 0, clientRect.Width(),
    clientRect.Height(), mBorderColor);
}

// Draw box with small offset
void CToolDlg::DrawButtonOutline(CPaintDC &dc, CWnd *but, int thickness, COLORREF color)
{
  CBaseDlg::DrawButtonOutline(dc, but, thickness, color, -1);
}

void CToolDlg::SetBorderColor(COLORREF inColor)
{
  mBorderColor = inColor;
}

