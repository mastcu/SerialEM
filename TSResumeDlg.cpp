// TSResumeDlg.cpp:      To resume a tilt series at a specified action point 
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "TSResumeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTSResumeDlg dialog


CTSResumeDlg::CTSResumeDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CTSResumeDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CTSResumeDlg)
  m_bUseNewRef = FALSE;
  m_strStatus = _T("");
  m_iResumeOption = -1;
  //}}AFX_DATA_INIT
  m_iSingleStep = 0;
}


void CTSResumeDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CTSResumeDlg)
  DDX_Control(pDX, IDC_RRESUME, m_butResume);
  DDX_Control(pDX, IDC_RNEXTTILT, m_butRadio2);
  DDX_Control(pDX, IDC_RSAVERECINA, m_butRadio3);
  DDX_Control(pDX, IDC_RTAKEREC, m_butRadio4);
  DDX_Control(pDX, IDC_RFOCUSREC, m_butRadio5);
  DDX_Control(pDX, IDC_RTRACKFOCUS, m_butRadio6);
  DDX_Control(pDX, IDC_RLOWTRACKFOCUS, m_butRadio7);
  DDX_Control(pDX, IDC_STATSTATUS, m_statStatus);
  DDX_Check(pDX, IDC_USENEWREF, m_bUseNewRef);
  DDX_Text(pDX, IDC_STATSTATUS, m_strStatus);
  DDX_Radio(pDX, IDC_RRESUME, m_iResumeOption);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTSResumeDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CTSResumeDlg)
  ON_BN_CLICKED(IDC_SINGLELOOP, OnSingleloop)
  ON_BN_CLICKED(IDC_SINGLESTEP, OnSinglestep)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTSResumeDlg message handlers

void CTSResumeDlg::OnSingleloop() 
{
  m_iSingleStep = 1; 
  OnOK(); 
}

void CTSResumeDlg::OnSinglestep() 
{
  m_iSingleStep = -1;   
  OnOK();
}

void CTSResumeDlg::OnOK() 
{
  UpdateData(true);
  CDialog::OnOK();
}

BOOL CTSResumeDlg::OnInitDialog() 
{
  CString string, repString;
  CBaseDlg::OnInitDialog();
  
  repString = m_bMontaging ? "montage" : m_strRecordName + " image";

  // This is one way to take care of the mode name in labels!
  ReplaceWindowText(&m_butRadio3, "Record", m_strRecordName);
  ReplaceWindowText(&m_butRadio4, "Record image", (LPCTSTR)repString);
  ReplaceWindowText(&m_butRadio5, "Record image", (LPCTSTR)repString);
  ReplaceWindowText(&m_butRadio6, "Record image", (LPCTSTR)repString);
  ReplaceWindowText(&m_butRadio7, "Record image", (LPCTSTR)repString);
  
  // Disable button 3 if no record in A or montaging
  m_butRadio3.EnableWindow(m_bRecordInA && !m_bMontaging && m_bChangeOK);
  
  // Enable button 2 only if one could go on
  m_butRadio2.EnableWindow(m_bTiltable && m_bChangeOK);
  m_butRadio7.EnableWindow(m_bLowMagAvailable && m_bChangeOK);

  m_butRadio4.EnableWindow(m_bChangeOK);
  m_butRadio5.EnableWindow(m_bChangeOK);
  m_butRadio6.EnableWindow(m_bChangeOK);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

