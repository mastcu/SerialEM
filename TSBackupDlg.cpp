// TSBackupDlg.cpp:      To select a section to back up to in a tilt series
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TSBackupDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CTSBackupDlg dialog


CTSBackupDlg::CTSBackupDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CTSBackupDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CTSBackupDlg)
    // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void CTSBackupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CTSBackupDlg)
  DDX_Control(pDX, IDC_LISTZTILT, m_listZTilt);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTSBackupDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CTSBackupDlg)
  ON_LBN_DBLCLK(IDC_LISTZTILT, OnDblclkListztilt)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTSBackupDlg message handlers

void CTSBackupDlg::OnDblclkListztilt()
{
  OnOK();
}

BOOL CTSBackupDlg::OnInitDialog()
{
  CString item;
  int i;
  int assumedShown = 8; // Have to guess how many fit in the box!

  CBaseDlg::OnInitDialog();
  m_listZTilt.ResetContent();
  m_listZTilt.SetTabStops(15);
  for (i = 0; i < m_iNumEntries; i++) {
    item.Format("\t%d\t\t%.2f", m_iBaseZ + i, m_fpTilts[i]);
    m_listZTilt.AddString(item);
  }
  m_listZTilt.SetCurSel(m_iSelection);

  // try to make sure the bottom line shows up
  int topLine = m_iNumEntries - assumedShown;
  if (topLine < 0)
    topLine = 0;

  // And try to make the selection be in the middle
  int topLine2 = m_iSelection - assumedShown / 2;
  if (topLine2 < 0)
    topLine2 = 0;

  // Take the lower of the two
  if (topLine > topLine2)
    topLine = topLine2;
  m_listZTilt.SetTopIndex(topLine);

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CTSBackupDlg::OnOK()
{
  m_iSelection = m_listZTilt.GetCurSel();

  CDialog::OnOK();
}
