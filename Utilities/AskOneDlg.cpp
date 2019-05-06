// AskOneDlg.cpp:         A modal dialog box for getting a one-line entry
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "..\SerialEM.h"
#include "AskOneDlg.h"
#include "..\XFolderDialog\XFolderDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAskOneDlg dialog


CAskOneDlg::CAskOneDlg(CWnd* pParent /*=NULL*/)
  : CDialog(CAskOneDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CAskOneDlg)
  m_sEditString = _T("");
  m_sTextString = _T("");
	m_strInfoLine = _T("");
	//}}AFX_DATA_INIT
  m_iExtraWidth = 0;
}


void CAskOneDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAskOneDlg)
  DDX_Control(pDX, IDC_ASKINFOLINE, m_statInfoLine);
  DDX_Control(pDX, IDOK, m_butOK);
  DDX_Control(pDX, IDCANCEL, m_butCancel);
  DDX_Control(pDX, IDC_ASKONE_TEXT, m_statText);
  DDX_Control(pDX, IDC_ASKONE_EDIT, m_pEditBox);
  DDX_Text(pDX, IDC_ASKONE_EDIT, m_sEditString);
  DDV_MaxChars(pDX, m_sEditString, 120);
  DDX_Text(pDX, IDC_ASKONE_TEXT, m_sTextString);
  DDX_Text(pDX, IDC_ASKINFOLINE, m_strInfoLine);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_BROWSE, m_butBrowse);
}


BEGIN_MESSAGE_MAP(CAskOneDlg, CDialog)
  //{{AFX_MSG_MAP(CAskOneDlg)
  //}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BROWSE, &CAskOneDlg::OnBrowse)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAskOneDlg message handlers

void CAskOneDlg::OnOK() 
{
  UpdateData(TRUE);
  
  CDialog::OnOK();
}

void CAskOneDlg::OnCancel() 
{
  
  CDialog::OnCancel();
}

BOOL CAskOneDlg::OnInitDialog() 
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!winApp->GetDisplayNotTruly120DPI())
    mSetDPI.Attach(AfxFindResourceHandle(MAKEINTRESOURCE(IDD), RT_DIALOG),
      m_hWnd, IDD, B3DNINT(1.25 * winApp->GetSystemDPI()));
  CDialog::OnInitDialog();
  bool showBrowse = !m_strBrowserTitle.IsEmpty();
  CString str = m_sEditString;
  WINDOWPLACEMENT winPlace;
  
  CRect rectNeed, rectWnd, rectOK, rectCancel, rectText, rectInfo, rectEdit, rectClient;
  int editWidth;

  // Figure out size needed for text
  CDC *pDC = m_statText.GetDC();
  pDC->SelectObject(m_statText.GetFont());
  pDC->DrawText(m_sTextString,  -1, rectNeed, DT_SINGLELINE | DT_CALCRECT);
  int textWidth = rectNeed.Width() + 5;
  pDC->DrawText(m_strInfoLine,  -1, rectNeed, DT_SINGLELINE | DT_CALCRECT);
  if (textWidth < rectNeed.Width() + 5)
    textWidth = rectNeed.Width() + 5;
  m_statText.ReleaseDC(pDC);
  
  // Get lots of rectangles for everything
  GetWindowRect(&rectWnd);
  GetClientRect(&rectClient);
  m_statText.GetWindowRect(&rectText);
  m_statInfoLine.GetWindowRect(&rectInfo);
  m_pEditBox.GetWindowRect(&rectEdit);
  editWidth = rectEdit.Width() + m_iExtraWidth;
  /*  if (m_iExtraWidth) {
    m_pEditBox.SetWindowPos(NULL, 0, 0, 
      rectEdit.Width() + m_iExtraWidth, rectEdit.Height(), SWP_NOZORDER | SWP_NOMOVE);
    m_pEditBox.GetWindowRect(&rectEdit);
  } */
  m_butOK.GetWindowRect(&rectOK);
  m_butCancel.GetWindowRect(&rectCancel);

  // Set some top and border coordinates: borderX defined by space to right of Edit
  int iXoffset = (rectWnd.Width() - rectClient.Width()) / 2;
  int iYoffset = (rectWnd.Height() - rectClient.Height()) / 2;
  int textTop = rectText.top - rectWnd.top - iYoffset;
  int infoTop = rectInfo.top - rectWnd.top - iYoffset;
  int editTop = rectEdit.top - rectWnd.top - iYoffset;
  int OKTop = rectOK.top - rectWnd.top - iYoffset;
  int borderX = rectWnd.right - rectEdit.right - iXoffset;

  // The desired width for window: if it has to be limited, cut text size
  int clientWidth = 2 * borderX + textWidth + editWidth +
    rectEdit.left - rectText.right;
  if (clientWidth > 1000) {
    textWidth -= clientWidth - 1000;
    clientWidth = 1000;
  }

  // But enforce minimum size for the buttons, and impose offset on text & edit
  int buttonMin = (showBrowse ? 9 : 7) * rectCancel.Width() / 2;
  int textOffset = 0;
  if (clientWidth < buttonMin) {
    textOffset = (buttonMin - clientWidth) / 2;
    clientWidth = buttonMin;
  }

  // Doing this breaks positioning somehow, so set it in middle of SerialEM area
  ((CSerialEMApp *)AfxGetApp())->GetWindowPlacement(&winPlace);
  ConstrainWindowPlacement(&winPlace);
  SetWindowPos(NULL, (winPlace.rcNormalPosition.right + winPlace.rcNormalPosition.left - 
    clientWidth) / 2, (winPlace.rcNormalPosition.bottom + winPlace.rcNormalPosition.top - 
    rectWnd.Height()) / 2, clientWidth + 2 * iXoffset, rectWnd.Height(), SWP_NOZORDER);
  m_statText.SetWindowPos(NULL, borderX + textOffset, textTop, textWidth, rectText.Height(),
    SWP_NOZORDER);
  m_statInfoLine.SetWindowPos(NULL, borderX + textOffset, infoTop, textWidth, rectText.Height(),
    SWP_NOZORDER);
  m_pEditBox.SetWindowPos(NULL, clientWidth - editWidth - borderX - textOffset, 
    editTop, editWidth, rectEdit.Height(), SWP_NOZORDER);
  m_butOK.SetWindowPos(NULL, clientWidth / 2 - (showBrowse ? 8 : 5) * 
    rectCancel.Width() / 4, OKTop, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butCancel.SetWindowPos(NULL, clientWidth / 2 + (showBrowse ? -2 : 1) * 
    rectCancel.Width() / 4, OKTop, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  if (showBrowse) {
    m_butBrowse.ShowWindow(SW_SHOW);
    m_butBrowse.SetWindowPos(NULL, clientWidth / 2 + rectCancel.Width(), OKTop, 0, 0, 
      SWP_NOZORDER | SWP_NOSIZE);
  }

  // This torture is required to get longer string to appear fully in the bigger box
  m_sEditString = "temp";
  UpdateData(FALSE);
  m_sEditString = str;
  UpdateData(FALSE);

  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CAskOneDlg::OnBrowse()
{
  UpdateData(true);
  if (m_sEditString.IsEmpty())
    m_sEditString = "C:\\";
  CXFolderDialog dlg(m_sEditString);
  dlg.SetTitle(m_strBrowserTitle);
  if (dlg.DoModal() == IDOK)
    m_sEditString = dlg.GetPath();
  UpdateData(false);
}
