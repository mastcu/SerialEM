// MacroControlDlg.cpp:  To set parameters for controlling macros
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "MacroControlDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMacroControlDlg dialog


CMacroControlDlg::CMacroControlDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CMacroControlDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CMacroControlDlg)
  m_strImageShift = _T("");
  m_strMontError = _T("");
  m_intRuns = 0;
  m_strTiltDown = _T("");
  m_strTiltUp = _T("");
  m_intWhiteLevel = 0;
  m_bImageShift = FALSE;
  m_bMontError = FALSE;
  m_bRuns = FALSE;
  m_bTiltDown = FALSE;
  m_bTiltUp = FALSE;
  m_bWhiteLevel = FALSE;
  //}}AFX_DATA_INIT
}


void CMacroControlDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CMacroControlDlg)
  DDX_Control(pDX, IDC_EDIT_WHITELEVEL, m_editWhiteLevel);
  DDX_Control(pDX, IDC_EDIT_TILTUP, m_editTiltUp);
  DDX_Control(pDX, IDC_EDIT_TILTDOWN, m_editTiltDown);
  DDX_Control(pDX, IDC_EDIT_RUNS, m_editRuns);
  DDX_Control(pDX, IDC_EDIT_MONTERROR, m_editMontError);
  DDX_Control(pDX, IDC_EDIT_IMAGESHIFT, m_editImageShift);
  DDX_Text(pDX, IDC_EDIT_IMAGESHIFT, m_strImageShift);
  DDX_Text(pDX, IDC_EDIT_MONTERROR, m_strMontError);
  DDX_Text(pDX, IDC_EDIT_RUNS, m_intRuns);
  DDV_MinMaxInt(pDX, m_intRuns, 1, 1000);
  DDX_Text(pDX, IDC_EDIT_TILTDOWN, m_strTiltDown);
  DDX_Text(pDX, IDC_EDIT_TILTUP, m_strTiltUp);
  DDX_Text(pDX, IDC_EDIT_WHITELEVEL, m_intWhiteLevel);
  DDV_MinMaxInt(pDX, m_intWhiteLevel, 0, 16000);
  DDX_Check(pDX, IDC_LIMIT_IMAGESHIFT, m_bImageShift);
  DDX_Check(pDX, IDC_LIMIT_MONTERROR, m_bMontError);
  DDX_Check(pDX, IDC_LIMIT_RUNS, m_bRuns);
  DDX_Check(pDX, IDC_LIMIT_TILTDOWN, m_bTiltDown);
  DDX_Check(pDX, IDC_LIMIT_TILTUP, m_bTiltUp);
  DDX_Check(pDX, IDC_LIMIT_WHITELEVEL, m_bWhiteLevel);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMacroControlDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CMacroControlDlg)
  ON_EN_KILLFOCUS(IDC_EDIT_IMAGESHIFT, OnKillfocusEditImageshift)
  ON_EN_KILLFOCUS(IDC_EDIT_MONTERROR, OnKillfocusEditMonterror)
  ON_EN_KILLFOCUS(IDC_EDIT_RUNS, OnKillfocusEditRuns)
  ON_EN_KILLFOCUS(IDC_EDIT_TILTDOWN, OnKillfocusEditTiltdown)
  ON_EN_KILLFOCUS(IDC_EDIT_TILTUP, OnKillfocusEditTiltup)
  ON_EN_KILLFOCUS(IDC_EDIT_WHITELEVEL, OnKillfocusEditWhitelevel)
  ON_BN_CLICKED(IDC_LIMIT_IMAGESHIFT, OnLimitImageshift)
  ON_BN_CLICKED(IDC_LIMIT_MONTERROR, OnLimitMonterror)
  ON_BN_CLICKED(IDC_LIMIT_RUNS, OnLimitRuns)
  ON_BN_CLICKED(IDC_LIMIT_TILTDOWN, OnLimitTiltdown)
  ON_BN_CLICKED(IDC_LIMIT_TILTUP, OnLimitTiltup)
  ON_BN_CLICKED(IDC_LIMIT_WHITELEVEL, OnLimitWhitelevel)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMacroControlDlg message handlers

BOOL CMacroControlDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  // Load the check boxes
  m_bTiltUp = mControl.limitTiltUp;
  m_bTiltDown = mControl.limitTiltDown;
  m_bWhiteLevel = mControl.limitScaleMax;
  m_bRuns = mControl.limitRuns;
  m_bMontError = mControl.limitMontError;
  m_bImageShift = mControl.limitIS;

  // Enable edit boxes based on check boxes
  m_editTiltUp.EnableWindow(m_bTiltUp);
  m_editTiltDown.EnableWindow(m_bTiltDown);
  m_editWhiteLevel.EnableWindow(m_bWhiteLevel);
  m_editRuns.EnableWindow(m_bRuns);
  m_editMontError.EnableWindow(m_bMontError);
  m_editImageShift.EnableWindow(m_bImageShift);

  // Load the edit boxes
  m_intWhiteLevel = mControl.scaleMaxLimit;
  m_intRuns = mControl.runLimit;
  m_strTiltUp.Format("%.1f", mControl.tiltUpLimit);
  m_strTiltDown.Format("%.1f", mControl.tiltDownLimit);
  m_strMontError.Format("%.2f", mControl.montErrorLimit);
  m_strImageShift.Format("%.2f", mControl.ISlimit);
  UpdateData(false);

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CMacroControlDlg::OnOK()
{
  UpdateData(true);

  // The floating values are already in the structure
  mControl.scaleMaxLimit = m_intWhiteLevel;
  mControl.runLimit = m_intRuns;

  mControl.limitTiltUp = m_bTiltUp;
  mControl.limitTiltDown = m_bTiltDown;
  mControl.limitScaleMax = m_bWhiteLevel;
  mControl.limitRuns = m_bRuns;
  mControl.limitMontError = m_bMontError;
  mControl.limitIS = m_bImageShift;

  CDialog::OnOK();
}

// For each edit box, unload data; process floats
void CMacroControlDlg::OnKillfocusEditImageshift()
{
  UpdateData(true);
  mControl.ISlimit = (float)atof((LPCTSTR)m_strImageShift);
  if (mControl.ISlimit == 0.0)
    m_editImageShift.SetWindowText("0.0");
}

void CMacroControlDlg::OnKillfocusEditMonterror()
{
  UpdateData(true);
  mControl.montErrorLimit = (float)atof((LPCTSTR)m_strMontError);
  if (mControl.montErrorLimit == 0.0)
    m_editMontError.SetWindowText("0.0");
}

void CMacroControlDlg::OnKillfocusEditRuns()
{
  UpdateData(true);
}

void CMacroControlDlg::OnKillfocusEditTiltdown()
{
  UpdateData(true);
  mControl.tiltDownLimit = (float)atof((LPCTSTR)m_strTiltDown);
  if (mControl.tiltDownLimit == 0.0)
    m_editTiltDown.SetWindowText("0.0");
}

void CMacroControlDlg::OnKillfocusEditTiltup()
{
  UpdateData(true);
  mControl.tiltUpLimit = (float)atof((LPCTSTR)m_strTiltUp);
  if (mControl.tiltUpLimit == 0.0)
    m_editTiltUp.SetWindowText("0.0");
}

void CMacroControlDlg::OnKillfocusEditWhitelevel()
{
  UpdateData(true);
}

// For each check box, enable or disable edit window
void CMacroControlDlg::OnLimitImageshift()
{
  UpdateData(true);
  m_editImageShift.EnableWindow(m_bImageShift);
}

void CMacroControlDlg::OnLimitMonterror()
{
  UpdateData(true);
  m_editMontError.EnableWindow(m_bMontError);
}

void CMacroControlDlg::OnLimitRuns()
{
  UpdateData(true);
  m_editRuns.EnableWindow(m_bRuns);
}

void CMacroControlDlg::OnLimitTiltdown()
{
  UpdateData(true);
  m_editTiltDown.EnableWindow(m_bTiltDown);
}

void CMacroControlDlg::OnLimitTiltup()
{
  UpdateData(true);
  m_editTiltUp.EnableWindow(m_bTiltUp);
}

void CMacroControlDlg::OnLimitWhitelevel()
{
  UpdateData(true);
  m_editWhiteLevel.EnableWindow(m_bWhiteLevel);
}
