// ScreenMeter.cpp:      A system-model screen meter with control on averaging
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ScreenMeter.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CScreenMeter dialog


CScreenMeter::CScreenMeter(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CScreenMeter::IDD, pParent)
{
  //{{AFX_DATA_INIT(CScreenMeter)
  m_strCurrent = _T("");
  m_bSmoothed = FALSE;
  mNonModal = true;
  //}}AFX_DATA_INIT
}


void CScreenMeter::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CScreenMeter)
  DDX_Control(pDX, IDC_STATMETER, m_statCurrent);
  DDX_Text(pDX, IDC_STATMETER, m_strCurrent);
  DDX_Check(pDX, IDC_SMOOTHED, m_bSmoothed);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScreenMeter, CBaseDlg)
  //{{AFX_MSG_MAP(CScreenMeter)
  ON_BN_CLICKED(IDC_SMOOTHED, OnSmoothed)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScreenMeter message handlers

void CScreenMeter::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CScreenMeter::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  CRect rect;
  m_statCurrent.GetWindowRect(&rect);
  mFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, m_strFontName);
  m_statCurrent.SetFont(&mFont);

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CScreenMeter::OnCancel()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mScopeStatus.MeterClosing();
  DestroyWindow();
}

void CScreenMeter::OnSmoothed()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->RestoreViewFocus();
  UpdateData(TRUE);
}
