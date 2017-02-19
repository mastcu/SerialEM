// ThreeChoiceBox.cpp : Presents message box equaivalent with configurable buttons
//
// Copyright (C) 2016 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "ThreeChoiceBox.h"
#include "afxdialogex.h"


// CThreeChoiceBox dialog

CThreeChoiceBox::CThreeChoiceBox(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CThreeChoiceBox::IDD, pParent)
  , m_strMessage(_T(""))
{
  mChoice = -1;
  mSetDefault = 0;
}

CThreeChoiceBox::~CThreeChoiceBox()
{
}

void CThreeChoiceBox::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDOK, m_butOK);
  DDX_Control(pDX, IDC_BUTMIDDLE, m_butMiddle);
  DDX_Control(pDX, IDCANCEL, m_butCancel);
  DDX_Text(pDX, IDC_STAT_MESSAGE, m_strMessage);
  DDX_Control(pDX, IDC_STAT_MESSAGE, m_statMessage);
  DDX_Control(pDX, IDC_STAT_GRAY_PANEL, m_statGrayPanel);
}


BEGIN_MESSAGE_MAP(CThreeChoiceBox, CBaseDlg)
  ON_BN_CLICKED(IDC_BUTMIDDLE, &CThreeChoiceBox::OnButMiddle)
  ON_WM_ERASEBKGND()
  ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


// CThreeChoiceBox message handlers
BOOL CThreeChoiceBox::OnInitDialog() 
{
  CBaseDlg::OnInitDialog();
  CRect butRect, rect2, rect3, statRect, winRect, clientRect, panelRect;
  int butHeight, yesWidth, noWidth, cancelWidth, gutter, ixOffset, rightEdge, messLeft;
  int allButWidth, addedWidth, pixPerLine, totLines, numWrap, endInd, addedHeight;
  int margin = (16 * mWinApp->GetSystemDPI()) / 120;
  int allButTop, butRight, panelTop, numLinesOrig = 4;
  CSize size;
  CString line, mess;
  CButton *defBut = &m_butOK;
  bool hasCancel = (mDlgType & MB_YESNOCANCEL) != 0;

  // Create background color
  mBkgdColor = RGB(255, 255, 255);    // At least on laptop, default is 212, 208, 200
  mBkgdBrush.CreateSolidBrush(mBkgdColor);

  // Set the labels
  SetDlgItemText(IDOK, (LPCTSTR)mYesText);
  SetDlgItemText(IDC_BUTMIDDLE, (LPCTSTR)mNoText);
  if (hasCancel) {
    SetDlgItemText(IDCANCEL, (LPCTSTR)mCancelText);
  } else {
    m_butCancel.ShowWindow(SW_HIDE);
  }

  // Measure the width needed for each button
  CClientDC dc(&m_butOK);
  dc.SelectObject(m_butOK.GetFont());
  m_butOK.GetClientRect(&butRect);
  butHeight = butRect.Height();
  size = dc.GetTextExtent(mYesText);
  yesWidth = B3DMAX(butRect.Width(), size.cx + margin);
  size = dc.GetTextExtent(mNoText);
  noWidth = B3DMAX(butRect.Width(), size.cx + margin);
  if (hasCancel) {
    size = dc.GetTextExtent(mCancelText);
    cancelWidth = B3DMAX(butRect.Width(), size.cx + margin);
    if (!mCancelText.Compare("Cancel"))
      ModifyStyle(0, WS_SYSMENU);
  }

  // Figure out coordinates for positioning calls and existing sizes and gutter
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  ixOffset = (winRect.Width() - clientRect.Width()) / 2;
  m_butOK.GetWindowRect(&butRect);
  m_butMiddle.GetWindowRect(&rect2);
  m_butCancel.GetWindowRect(&rect3);
  gutter = rect3.left - rect2.right;
  rightEdge = rect3.right - winRect.left - ixOffset;
  m_statMessage.GetWindowRect(&statRect);
  messLeft = statRect.left - winRect.left - ixOffset;
  allButWidth = hasCancel ? cancelWidth + gutter : 0;
  allButWidth += gutter + yesWidth + noWidth;
  addedWidth = B3DMAX(0, allButWidth - (rightEdge - messLeft));
  allButTop = (butRect.top - winRect.top) - (winRect.Height() - clientRect.Height()) + 
    ixOffset;
  m_statGrayPanel.GetWindowRect(&panelRect);
  panelTop = (panelRect.top - winRect.top) - (winRect.Height() - clientRect.Height()) + 
    ixOffset;

  // Count lines of text included estimate of wrapped lines needed for long lines
  pixPerLine = statRect.Height() / numLinesOrig;
  mess = m_strMessage;
  totLines = 0;
  do {
    endInd = mess.Find('\n');
    if (endInd < 0)
      line = mess;
    else {
      line = mess.Left(endInd + 1);
      mess = mess.Mid(endInd + 1);
    }
    size = dc.GetTextExtent(line);
    if (size.cx < statRect.Width() - 4)
      numWrap = 1;
    else
      numWrap = 1 + size.cx / ((9 * statRect.Width()) / 10);
    totLines += numWrap;
  } while (endInd >= 0);
  
  // Set up all the components to the right size and position
  addedHeight = pixPerLine * B3DMAX(0, totLines + 1 - numLinesOrig);
  SetWindowPos(NULL, 0, 0, winRect.Width() + addedWidth, winRect.Height() + addedHeight,
    SWP_NOMOVE);
  m_statMessage.SetWindowPos(NULL, 0, 0, statRect.Width(), statRect.Height() + 
    addedHeight, SWP_NOMOVE);
  m_statGrayPanel.SetWindowPos(&wndBottom, panelRect.left - winRect.left - ixOffset, 
    panelTop + addedHeight, panelRect.Width() + addedWidth, panelRect.Height(), 
    0);
  butRight = rightEdge + addedWidth;
  if (hasCancel) {
    m_butCancel.SetWindowPos(&m_statGrayPanel, butRight - cancelWidth, allButTop + addedHeight, 
      cancelWidth, butHeight, 0);
    butRight -= gutter + cancelWidth;
  }
  m_butMiddle.SetWindowPos(&m_statGrayPanel, butRight - noWidth, allButTop + addedHeight, 
      noWidth, butHeight, 0);
  butRight -= gutter + noWidth;
  m_butOK.SetWindowPos(&m_statGrayPanel,  butRight - yesWidth, allButTop + addedHeight, 
      yesWidth, butHeight, 0);

  // Take care of default button
  if (mSetDefault == 1)
    defBut = &m_butMiddle;
  if (hasCancel && mSetDefault == 2)
    defBut = &m_butCancel;

  // Set the style, set the focus and return FALSE like it says!
  // There is no reason to SetDefID or SendMessage(DM_SETDEFID, ID);
  defBut->SetButtonStyle(BS_DEFPUSHBUTTON);
  defBut->SetFocus();

  UpdateData(false);
  return FALSE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CThreeChoiceBox::OnOK() 
{
  if (mChoice < 0)
    mChoice = IDYES;
  CBaseDlg::OnOK();
}

// Cancel: do not do it (on Esc) if there is no third button
void CThreeChoiceBox::OnCancel() 
{
  if ((mDlgType & MB_YESNOCANCEL) == 0)
    return;
  mChoice = IDCANCEL;
  CBaseDlg::OnCancel();
}

void CThreeChoiceBox::OnButMiddle()
{
  mChoice = IDNO;
  OnOK();
}

// This takes care of the background of the dialog box
BOOL CThreeChoiceBox::OnEraseBkgnd(CDC* pDC)
{
  CRect rect;
  GetClientRect(&rect);
  CBrush *pOld = pDC->SelectObject(&mBkgdBrush);
  BOOL bRes  = pDC->PatBlt(0, 0, rect.Width(), rect.Height(), PATCOPY);
  pDC->SelectObject(pOld);    // restore old brush
  return bRes;                       // CDialog::OnEraseBkgnd(pDC);
}

// This takes care of the background of the static text, which consists of the background
// around the letters and the rest of the box.
// Yes this is a great mechanism.  No, it does not work with buttons
HBRUSH CThreeChoiceBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
  if (pWnd->GetDlgCtrlID() == IDC_STAT_MESSAGE) {
    pDC->SetTextColor(RGB(0, 0, 0));   
    pDC->SetBkColor(mBkgdColor);// for text background   
    return (HBRUSH)(mBkgdBrush.GetSafeHandle());//your brush   
  } else {
    return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
  }
}