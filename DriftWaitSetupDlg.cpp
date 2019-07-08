// DriftWaitSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DriftWaitSetupDlg.h"


// CDriftWaitSetupDlg dialog

CDriftWaitSetupDlg::CDriftWaitSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_DRIFTWAITSETUP, pParent)
  , m_iMeasureType(0)
  , m_bSetTrial(FALSE)
  , m_strTrialBinning(_T(""))
  , m_fTrialExp(0)
  , m_fInterval(0)
  , m_fDriftRate(0)
  , m_fMaxWait(0)
  , m_bThrowError(FALSE)
  , m_bUseAngstroms(FALSE)
{

}

CDriftWaitSetupDlg::~CDriftWaitSetupDlg()
{
}

void CDriftWaitSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_RWD_TRIAL, m_iMeasureType);
  DDX_Control(pDX, IDC_CHECK_SET_TRIAL, m_butSetTrial);
  DDX_Check(pDX, IDC_CHECK_SET_TRIAL, m_bSetTrial);
  DDX_Control(pDX, IDC_STAT_TRIAL_EXP, m_statTrialExp);
  DDX_Control(pDX, IDC_STAT_EXP_SEC, m_statExpSec);
  DDX_Control(pDX, IDC_STAT_TRIAL_BINNING, m_statTrialBinning);
  DDX_Text(pDX, IDC_STAT_TRIAL_BINNING, m_strTrialBinning);
  DDX_Control(pDX, IDC_SPIN_TRIAL_BIN, m_sbcTrialBin);
  DDX_Control(pDX, IDC_EDIT_TRIAL_EXP, m_editTrialExp);
  DDX_Text(pDX, IDC_EDIT_TRIAL_EXP, m_fTrialExp);
  DDV_MinMaxFloat(pDX, m_fTrialExp, 0.001, 20.);
  DDX_Text(pDX, IDC_EDIT_INTERVAL, m_fInterval);
  DDV_MinMaxFloat(pDX, m_fInterval, 0.1, 60);
  DDX_Text(pDX, IDC_EDIT_DRIFT_RATE, m_fDriftRate);
  DDV_MinMaxFloat(pDX, m_fDriftRate, 0.01, 1000.);
  DDX_Text(pDX, IDC_EDIT_MAX_WAIT, m_fMaxWait);
  DDV_MinMaxFloat(pDX, m_fMaxWait, 1., 1000.);
  DDX_Check(pDX, IDC_CHECK_THROW_ERROR, m_bThrowError);
  DDX_Check(pDX, IDC_CHECK_USE_ANGSTROMS, m_bUseAngstroms);
}


BEGIN_MESSAGE_MAP(CDriftWaitSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RADIO4, &CDriftWaitSetupDlg::OnBnClickedRadio4)
  ON_EN_CHANGE(IDC_EDIT_INTERVAL, &CDriftWaitSetupDlg::OnEnChangeEditInterval)
  ON_BN_CLICKED(IDC_RWD_TRIAL, &CDriftWaitSetupDlg::OnMeasureType)
  ON_BN_CLICKED(IDC_CHECK_SET_TRIAL, &CDriftWaitSetupDlg::OnCheckSetTrial)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TRIAL_BIN, &CDriftWaitSetupDlg::OnDeltaposSpinTrialBin)
  ON_BN_CLICKED(IDC_CHECK_USE_ANGSTROMS, &CDriftWaitSetupDlg::OnCheckUseAngstroms)
END_MESSAGE_MAP()


// CDriftWaitSetupDlg message handlers
BOOL CDriftWaitSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  return TRUE;
}

void CDriftWaitSetupDlg::OnOK()
{
  SetFocus();
  CBaseDlg::OnOK();
}

void CDriftWaitSetupDlg::OnMeasureType()
{
  // TODO: Add your control notification handler code here
}


void CDriftWaitSetupDlg::OnCheckSetTrial()
{
  // TODO: Add your control notification handler code here
}


void CDriftWaitSetupDlg::OnDeltaposSpinTrialBin(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  // TODO: Add your control notification handler code here
  *pResult = 0;
}


void CDriftWaitSetupDlg::OnCheckUseAngstroms()
{
  // TODO: Add your control notification handler code here
}
