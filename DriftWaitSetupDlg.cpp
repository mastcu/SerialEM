// DriftWaitSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DriftWaitSetupDlg.h"
#include "CameraController.h"

// CDriftWaitSetupDlg dialog

CDriftWaitSetupDlg::CDriftWaitSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_DRIFTWAITSETUP, pParent)
  , m_iMeasureType(0)
  , m_bSetTrial(FALSE)
  , m_strTrialBinning(_T(""))
  , m_fTrialExp(.1f)
  , m_fInterval(1.f)
  , m_fDriftRate(1.f)
  , m_fMaxWait(1.f)
  , m_bThrowError(FALSE)
  , m_bUseAngstroms(FALSE)
  , m_strRateUnits(_T(""))
  , m_bChangeIS(FALSE)
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
  DDV_MinMaxFloat(pDX, m_fTrialExp, 0.001f, 20.f);
  DDX_Text(pDX, IDC_EDIT_INTERVAL, m_fInterval);
  DDV_MinMaxFloat(pDX, m_fInterval, 0.1f, 60);
  DDX_Text(pDX, IDC_EDIT_DRIFT_RATE, m_fDriftRate);
  DDV_MinMaxFloat(pDX, m_fDriftRate, 0.01f, 1000.);
  DDX_Text(pDX, IDC_EDIT_MAX_WAIT, m_fMaxWait);
  DDV_MinMaxFloat(pDX, m_fMaxWait, 1.f, 1000.);
  DDX_Check(pDX, IDC_CHECK_THROW_ERROR, m_bThrowError);
  DDX_Check(pDX, IDC_CHECK_USE_ANGSTROMS, m_bUseAngstroms);
  DDX_Text(pDX, IDC_STAT_RATE_UNITS, m_strRateUnits);
  DDX_Control(pDX, IDC_CHECK_CHANGE_IS, m_butChangeIS);
  DDX_Check(pDX, IDC_CHECK_CHANGE_IS, m_bChangeIS);
}


BEGIN_MESSAGE_MAP(CDriftWaitSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RWD_TRIAL, OnMeasureType)
  ON_BN_CLICKED(IDC_RWD_FOCUS, OnMeasureType)
  ON_BN_CLICKED(IDC_RWD_BETWEEN_AF, OnMeasureType)
  ON_BN_CLICKED(IDC_RWD_WITHIN_AF, OnMeasureType)
  ON_BN_CLICKED(IDC_CHECK_SET_TRIAL, OnCheckSetTrial)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TRIAL_BIN, OnDeltaposSpinTrialBin)
  ON_BN_CLICKED(IDC_CHECK_USE_ANGSTROMS, OnCheckUseAngstroms)
END_MESSAGE_MAP()


// CDriftWaitSetupDlg message handlers
BOOL CDriftWaitSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mCamParam = mWinApp->GetActiveCamParam();
  DriftWaitParams *params = mWinApp->mParticleTasks->GetDriftWaitParams();
  ControlSet *cset = mWinApp->GetConSets() + TRIAL_CONSET;
  int realBin;
  m_sbcTrialBin.SetRange(0, 1000);
  m_sbcTrialBin.SetPos(500);
  mBinDivisor = BinDivisorI(mCamParam);
  mWinApp->mCamera->FindNearestBinning(mCamParam, params->binning * mBinDivisor,
    cset->K2ReadMode, mBinIndex, realBin);
  mBinIndex = B3DMAX(mBinIndex, mBinDivisor - 1);
  m_strTrialBinning.Format("Binning %d", realBin / mBinDivisor);
  m_strRateUnits = params->useAngstroms ? "Angstroms/sec" : "nm/sec";

  m_iMeasureType = params->measureType - (int)WFD_USE_TRIAL;
  m_bSetTrial = params->setTrialParams;
  m_fTrialExp = params->exposure;
  m_fInterval = params->interval;
  m_fMaxWait = params->maxWaitTime;
  m_bThrowError = params->failureAction > 0;
  m_bUseAngstroms = params->useAngstroms;
  m_fDriftRate = params->driftRate * (params->useAngstroms ? 10.f : 1.f);
  m_bChangeIS = params->changeIS;
  ManageTrialEntries();
  m_butChangeIS.EnableWindow(m_iMeasureType < 3);
  UpdateData(false);
  return TRUE;
}

void CDriftWaitSetupDlg::OnOK()
{
  UpdateData(true);
  DriftWaitParams *params = mWinApp->mParticleTasks->GetDriftWaitParams();
  params->measureType = m_iMeasureType + (int)WFD_USE_TRIAL;
  params->setTrialParams = m_bSetTrial;
  params->exposure = m_fTrialExp;
  params->binning = mCamParam->binnings[mBinIndex] / mBinDivisor;
  params->driftRate = m_fDriftRate / (m_bUseAngstroms ? 10.f : 1.f);
  params->interval = m_fInterval;
  params->maxWaitTime = m_fMaxWait;
  params->failureAction = m_bThrowError ? 1 : 0;
  params->changeIS = m_bChangeIS;
  params->useAngstroms = m_bUseAngstroms;
  CBaseDlg::OnOK();
}

void CDriftWaitSetupDlg::OnMeasureType()
{
  UpdateData(true);
  m_butChangeIS.EnableWindow(m_iMeasureType < 3);
}

void CDriftWaitSetupDlg::OnCheckSetTrial()
{
  UpdateData(true);
  ManageTrialEntries();
}

void CDriftWaitSetupDlg::OnDeltaposSpinTrialBin(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mBinIndex, mBinDivisor - 1, 
    mCamParam->numBinnings - 1, mBinIndex))
    return;
  m_strTrialBinning.Format("Binning %d", mCamParam->binnings[mBinIndex] / mBinDivisor);
  UpdateData(false);
  *pResult = 0;
}


void CDriftWaitSetupDlg::OnCheckUseAngstroms()
{
  BOOL oldUse = m_bUseAngstroms;
  UpdateData(true);
  m_strRateUnits = m_bUseAngstroms ? "Angstroms/sec" : "nm/sec";
  m_fDriftRate = m_fDriftRate * (m_bUseAngstroms ? 10.f : 1.f) / (oldUse ? 10.f : 1.f);
  UpdateData(false);
}


void CDriftWaitSetupDlg::ManageTrialEntries()
{
  m_statTrialExp.EnableWindow(m_bSetTrial);
  m_statExpSec.EnableWindow(m_bSetTrial);
  m_statTrialBinning.EnableWindow(m_bSetTrial);
  m_editTrialExp.EnableWindow(m_bSetTrial);
  m_sbcTrialBin.EnableWindow(m_bSetTrial);
}
