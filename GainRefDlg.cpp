// GainRefDlg.cpp:       To set parameters for taking gain references
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "GainRefDlg.h"
#include "GainRefMaker.h"
#include "EMscope.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define FRAMES_MAX  100
#define TARGET_MIN  1
#define TARGET_MAX  250000
#define TARGET_INC  500
#define MAX_DARK_AVERAGE 20

/////////////////////////////////////////////////////////////////////////////
// CGainRefDlg dialog


CGainRefDlg::CGainRefDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CGainRefDlg::IDD, pParent)
  , m_bAutosaveRaw(FALSE)
{
  //{{AFX_DATA_INIT(CGainRefDlg)
  m_iFrames = 0;
  m_iTarget = 0;
  m_iBinning = -1;
  m_strExisting = _T("");
  m_strOddRef = _T("will use a reference from DigitalMicrograph");
	m_bCalDose = FALSE;
	m_bAverageDark = FALSE;
	m_iNumAverage = 0;
	//}}AFX_DATA_INIT
}


void CGainRefDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CGainRefDlg)
  DDX_Control(pDX, IDC_STATAVERAGE, m_statTimes);
  DDX_Control(pDX, IDC_GR_SPINAVERAGE, m_spinAverage);
  DDX_Control(pDX, IDC_GR_EDITAVERAGE, m_editNumAverage);
  DDX_Control(pDX, IDC_GR_AVERAGEDARK, m_butAverageDark);
  DDX_Control(pDX, IDC_CHECKNORMSPOT, m_butCalDose);
  DDX_Control(pDX, IDC_SPINTARGET, m_sbcTarget);
  DDX_Control(pDX, IDC_SPINFRAMES, m_sbcFrames);
  DDX_Text(pDX, IDC_EDITFRAMES, m_iFrames);
  DDV_MinMaxInt(pDX, m_iFrames, 1, FRAMES_MAX);
  DDX_Text(pDX, IDC_EDITTARGET, m_iTarget);
  DDV_MinMaxInt(pDX, m_iTarget, TARGET_MIN, TARGET_MAX);
  DDX_Radio(pDX, IDC_RBIN1, m_iBinning);
  DDX_Text(pDX, IDC_STATEXISTING, m_strExisting);
  DDX_Text(pDX, IDC_STATODDREF, m_strOddRef);
  DDX_Check(pDX, IDC_CHECKNORMSPOT, m_bCalDose);
  DDX_Check(pDX, IDC_GR_AVERAGEDARK, m_bAverageDark);
  DDX_Text(pDX, IDC_GR_EDITAVERAGE, m_iNumAverage);
  DDV_MinMaxInt(pDX, m_iNumAverage, 2, MAX_DARK_AVERAGE);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_AUTOSAVE_RAW, m_butAutosaveRaw);
  DDX_Check(pDX, IDC_AUTOSAVE_RAW, m_bAutosaveRaw);
}


BEGIN_MESSAGE_MAP(CGainRefDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CGainRefDlg)
  ON_EN_KILLFOCUS(IDC_EDITFRAMES, OnKillfocusEditframes)
  ON_EN_KILLFOCUS(IDC_EDITTARGET, OnKillfocusEdittarget)
  ON_BN_CLICKED(IDC_RBIN1, OnRadiobin)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINFRAMES, OnDeltaposSpinframes)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINTARGET, OnDeltaposSpintarget)
	ON_BN_CLICKED(IDC_CHECKNORMSPOT, OnCheckCalDose)
  ON_BN_CLICKED(IDC_RBIN2, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN3, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN4, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN5, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN6, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN7, OnRadiobin)
  ON_BN_CLICKED(IDC_RBIN8, OnRadiobin)
	ON_EN_KILLFOCUS(IDC_GR_EDITAVERAGE, OnKillfocusGrEditaverage)
	ON_NOTIFY(UDN_DELTAPOS, IDC_GR_SPINAVERAGE, OnDeltaposGrSpinaverage)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_AUTOSAVE_RAW, &CGainRefDlg::OnAutosaveRaw)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGainRefDlg message handlers

void CGainRefDlg::OnKillfocusEditframes() 
{
  UpdateData(true); 
  m_sbcFrames.SetPos(m_iFrames);
}

void CGainRefDlg::OnKillfocusEdittarget() 
{
  UpdateData(true); 
  m_sbcTarget.SetPos(m_iTarget);
}

// When binning changes, changes target by a factor of 4
void CGainRefDlg::OnRadiobin() 
{
  CSerialEMApp *winAp = (CSerialEMApp *)AfxGetApp();
  int oldBin = m_iBinning;
  float gainRatio = winAp->GetGainFactor(mCamera, 2) / winAp->GetGainFactor(mCamera, 1);
  UpdateData(true); 
  if (oldBin == m_iBinning)
    return;
  m_butCalDose.EnableWindow(m_iBinning < 2);
  ManageExistLine();
  if ((oldBin ? 1 : 0) == (m_iBinning ? 1 : 0))
    return;
  if (m_iBinning)
    m_iTarget = B3DNINT(4 * m_iTarget * gainRatio / TARGET_INC) * TARGET_INC;
  else
    m_iTarget = B3DNINT((m_iTarget / (4 * gainRatio)) / TARGET_INC) * TARGET_INC;
  m_iTarget = B3DMIN(TARGET_MAX, B3DMAX(TARGET_MIN, m_iTarget));
  m_sbcTarget.SetPos(m_iTarget / TARGET_INC);
  UpdateData(false);
}

// Change number of frames
void CGainRefDlg::OnDeltaposSpinframes(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  UpdateData(true);
  int newVal = m_iFrames + pNMUpDown->iDelta;
  if (newVal < 1 || newVal > FRAMES_MAX) {
    *pResult = 1;
    return;
  }
  m_iFrames = newVal;
  UpdateData(false);
  *pResult = 0;
}

// Change target as long as it stays in a legal range
void CGainRefDlg::OnDeltaposSpintarget(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int delta = pNMUpDown->iDelta;
  UpdateData(true);
  if (delta > 0 && m_iTarget >= TARGET_MAX || delta < 0 && m_iTarget <= TARGET_MIN) {
    *pResult = 1;
    return;
  }
  m_iTarget += delta * TARGET_INC;
  if (m_iTarget > TARGET_MAX)
    m_iTarget = TARGET_MAX;
  if (m_iTarget < TARGET_MIN)
    m_iTarget = TARGET_MIN;
  UpdateData(false);
  *pResult = 0;
}

// Checkbox for calibrating dose
void CGainRefDlg::OnCheckCalDose() 
{
  UpdateData(true);	
}

// Checkbox for autosave DE frames
void CGainRefDlg::OnAutosaveRaw()
{
  UpdateData(true);
}

// Edit and spin box for averaging dark refs
void CGainRefDlg::OnKillfocusGrEditaverage() 
{
  UpdateData(true);
  m_spinAverage.SetPos(m_iNumAverage);
}

void CGainRefDlg::OnDeltaposGrSpinaverage(NMHDR* pNMHDR, LRESULT* pResult) 
{
  UpdateData(true);	
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = m_iNumAverage + pNMUpDown->iDelta;
  if (newVal < 2 || newVal > MAX_DARK_AVERAGE) {
    *pResult = 1;
    return;
  }
  m_iNumAverage = newVal;
  UpdateData(false);
	*pResult = 0;
}

// Take care of the line about whether a reference will be replaced
void CGainRefDlg::ManageExistLine()
{
  if (mRefExists[m_iBinning + mIndOffset])
    m_strExisting = "This new reference will replace an existing reference";
  else
    m_strExisting.Format("There is no existing reference with binning %d", 
      mBinnings[m_iBinning + mIndOffset]);
  UpdateData(false);
}

BOOL CGainRefDlg::OnInitDialog() 
{
  CString title;
  CButton *radio;
  CBaseDlg::OnInitDialog();
  
  // Make sure target and frame values are OK
  if (m_iTarget > TARGET_MAX)
    m_iTarget = TARGET_MAX;
  if (m_iTarget < TARGET_MIN)
    m_iTarget = TARGET_MIN;
  if (m_iFrames < 1)
    m_iFrames = 1;

  // Set up spin buttons
  m_sbcFrames.SetRange(1, FRAMES_MAX);
  m_sbcFrames.SetPos(m_iFrames);
  m_sbcTarget.SetRange(0, TARGET_MAX / TARGET_INC + 10);
  m_sbcTarget.SetPos(m_iTarget);
 
  m_spinAverage.SetRange(2, MAX_DARK_AVERAGE);
  m_spinAverage.SetPos(m_iNumAverage);

  // Set up the extra binning buttons
  for (int i = 2; i < MAX_EXTRA_REFS + 2; i++) {
    radio = (CButton *)GetDlgItem(i + IDC_RBIN1);
    radio->ShowWindow(B3DCHOICE(i < mNumBinnings - mIndOffset, SW_SHOW, SW_HIDE));
    if (i < mNumBinnings - mIndOffset) {
      title.Format("%d", mBinnings[i + mIndOffset]);
      SetDlgItemText(i + IDC_RBIN1, title);
    }
  }
  if (mBinnings[0] != 1) {
    radio = (CButton *)GetDlgItem(IDC_RBIN1);
    radio->EnableWindow(false);
    m_iBinning = B3DMAX(1, m_iBinning);
  }
  m_butAutosaveRaw.EnableWindow(mCanAutosave);

  // Take care of title
  ManageExistLine();
  GetWindowText(title);
  title += " - " + mName;
  SetWindowText(title);
  SetDefID(45678);    // Disable OK from being default button

  if (JEOLscope)
    m_butCalDose.SetWindowText("Calibrate dose with last image");
  m_butCalDose.EnableWindow(m_iBinning < 2 && m_bGainCalibrated);

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
