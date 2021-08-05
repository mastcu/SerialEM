// DriftWaitSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DriftWaitSetupDlg.h"
#include "CameraController.h"
#include "TiltSeriesParam.h"

// CDriftWaitSetupDlg dialog
static int idTable[] = {IDC_RWD_TRIAL, IDC_RWD_FOCUS, IDC_RWD_WITHIN_AF,IDC_STAT_TYPE_BOX,
  IDC_STAT_INTERVAL, IDC_EDIT_INTERVAL, IDC_STAT_SEC1, IDC_CHECK_USE_ANGSTROMS,
  IDC_CHECK_SET_TRIAL, IDC_EDIT_TRIAL_EXP, IDC_STAT_EXP_SEC, IDC_STAT_TRIAL_BINNING,
  IDC_SPIN_TRIAL_BIN, IDC_STAT_TARGET, IDC_EDIT_DRIFT_RATE, IDC_STAT_RATE_UNITS,
  IDC_STAT_MAX_WAIT, IDC_EDIT_MAX_WAIT, IDC_STAT_SEC2, IDC_CHECK_THROW_ERROR,
  IDC_STAT_TRIAL_EXP, IDC_CHECK_CHANGE_IS, IDC_CHECK_USE_PRIOR_AUTOFOCUS,
  IDC_STAT_PRIOR_BELOW, IDC_EDIT_PRIOR_THRESH, IDC_STAT_PRIOR_NM, PANEL_END,
  IDC_STAT_ANGLE_BOX, IDC_LINE1, IDC_RANY_ANGLE, IDC_RRUN_BELOW, IDC_RRUN_ABOVE,
  IDC_EDIT_RUN_BELOW, IDC_STAT_DEG1, IDC_STAT_DEG2, IDC_EDIT_RUN_ABOVE,
  IDC_STAT_DOSYM_BOX, IDC_CHECK_AT_REVERSALS, IDC_CHECK_IGNORE_ANGLES, PANEL_END,
  IDC_STAT_SPACER, IDC_BUTHELP, IDOK, IDCANCEL, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];

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
  , m_bAtReversals(FALSE)
  , m_bIgnoreAngles(FALSE)
  , m_iAngleChoice(0)
  , m_fRunAbove(2.f)
  , m_fRunBelow(2.f)
  , m_bUsePriorAutofocus(FALSE)
  , m_fPriorThresh(1.f)
  , m_strPriorUnits(_T(""))
{
  mTSparam = NULL;
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
  DDX_Check(pDX, IDC_CHECK_AT_REVERSALS, m_bAtReversals);
  DDX_Check(pDX, IDC_CHECK_IGNORE_ANGLES, m_bIgnoreAngles);
  DDX_Radio(pDX, IDC_RANY_ANGLE, m_iAngleChoice);
  DDX_Control(pDX, IDC_EDIT_RUN_ABOVE, m_editRunAbove);
  DDX_Control(pDX, IDC_EDIT_RUN_BELOW, m_editRunBelow);
  DDX_Text(pDX, IDC_EDIT_RUN_ABOVE, m_fRunAbove);
  DDV_MinMaxFloat(pDX, m_fRunAbove, 1.f, 90.f);
  DDX_Text(pDX, IDC_EDIT_RUN_BELOW, m_fRunBelow);
  DDV_MinMaxFloat(pDX, m_fRunBelow, 1.f, 90.f);
  DDX_Control(pDX, IDC_CHECK_USE_PRIOR_AUTOFOCUS, m_butUsePriorFocus);
  DDX_Check(pDX, IDC_CHECK_USE_PRIOR_AUTOFOCUS, m_bUsePriorAutofocus);
  DDX_Control(pDX, IDC_EDIT_PRIOR_THRESH, m_editPriorThresh);
  DDX_Text(pDX, IDC_EDIT_PRIOR_THRESH, m_fPriorThresh);
  DDV_MinMaxFloat(pDX, m_fPriorThresh, .01f, 1000.);
  DDX_Text(pDX, IDC_STAT_PRIOR_NM, m_strPriorUnits);
}


BEGIN_MESSAGE_MAP(CDriftWaitSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RWD_TRIAL, OnMeasureType)
  ON_BN_CLICKED(IDC_RWD_FOCUS, OnMeasureType)
  ON_BN_CLICKED(IDC_RWD_WITHIN_AF, OnMeasureType)
  ON_BN_CLICKED(IDC_CHECK_SET_TRIAL, OnCheckSetTrial)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TRIAL_BIN, OnDeltaposSpinTrialBin)
  ON_BN_CLICKED(IDC_CHECK_USE_ANGSTROMS, OnCheckUseAngstroms)
  ON_BN_CLICKED(IDC_RANY_ANGLE, OnRadioAngle)
  ON_BN_CLICKED(IDC_RRUN_ABOVE, OnRadioAngle)
  ON_BN_CLICKED(IDC_RRUN_BELOW, OnRadioAngle)
  ON_BN_CLICKED(IDC_CHECK_USE_PRIOR_AUTOFOCUS, OnUsePriorAutofocus)
END_MESSAGE_MAP()


// CDriftWaitSetupDlg message handlers
BOOL CDriftWaitSetupDlg::OnInitDialog()
{
  BOOL states[] = {true, false, true};
  CBaseDlg::OnInitDialog();
  DriftWaitParams *params = mWinApp->mParticleTasks->GetDriftWaitParams();
  ControlSet *cset = mWinApp->GetConSets() + TRIAL_CONSET;
  int realBin;
  mCamParam = mWinApp->GetActiveCamParam();
  states[1] = mTSparam != NULL;
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);
  AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 0);

  m_sbcTrialBin.SetRange(0, 1000);
  m_sbcTrialBin.SetPos(500);
  mBinDivisor = BinDivisorI(mCamParam);
  mWinApp->mCamera->FindNearestBinning(mCamParam, params->binning * mBinDivisor,
    cset->K2ReadMode, mBinIndex, realBin);
  mBinIndex = B3DMAX(mBinIndex, mBinDivisor - 1);
  m_strTrialBinning.Format("Binning %d", realBin / mBinDivisor);
  m_strRateUnits = m_strPriorUnits =  params->useAngstroms ? "Angstroms/sec" : "nm/sec";

  m_iMeasureType = params->measureType - (int)WFD_USE_TRIAL;
  m_bSetTrial = params->setTrialParams;
  m_fTrialExp = params->exposure;
  m_fInterval = params->interval;
  m_fMaxWait = params->maxWaitTime;
  m_bThrowError = params->failureAction > 0;
  m_bUseAngstroms = params->useAngstroms;
  m_fDriftRate = params->driftRate * (params->useAngstroms ? 10.f : 1.f);
  m_bChangeIS = params->changeIS;
  m_bUsePriorAutofocus = params->usePriorAutofocus;
  m_fPriorThresh = params->priorAutofocusRate  * (params->useAngstroms ? 10.f : 1.f);
  if (mTSparam) {
    if (mTSparam->onlyWaitDriftAboveBelow)
      m_iAngleChoice = mTSparam->onlyWaitDriftAboveBelow > 0 ? 1 : 2;
    m_fRunAbove = mTSparam->waitDriftAboveAngle;
    m_fRunBelow = mTSparam->waitDriftBelowAngle;
    m_bAtReversals = mTSparam->waitAtDosymReversals;
    m_bIgnoreAngles = mTSparam->waitDosymIgnoreAngles;
    m_editRunAbove.EnableWindow(m_iAngleChoice == 1);
    m_editRunBelow.EnableWindow(m_iAngleChoice == 2);
  }
  ManageTrialEntries();
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
  params->usePriorAutofocus = m_bUsePriorAutofocus;
  params->priorAutofocusRate = m_fPriorThresh / (m_bUseAngstroms ? 10.f : 1.f);
  if (mTSparam) {
    mTSparam->onlyWaitDriftAboveBelow = (m_iAngleChoice > 1 ? -1 : m_iAngleChoice);
    mTSparam->waitDriftAboveAngle = m_fRunAbove;
    mTSparam->waitDriftBelowAngle = m_fRunBelow;
    mTSparam->waitAtDosymReversals = m_bAtReversals;
    mTSparam->waitDosymIgnoreAngles = m_bIgnoreAngles;
  }
  CBaseDlg::OnOK();
}

void CDriftWaitSetupDlg::OnMeasureType()
{
  UpdateData(true);
  ManageTrialEntries();
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
  m_strRateUnits = m_strPriorUnits = m_bUseAngstroms ? "Angstroms/sec" : "nm/sec";
  m_fDriftRate = m_fDriftRate * (m_bUseAngstroms ? 10.f : 1.f) / (oldUse ? 10.f : 1.f);
  m_fPriorThresh *= (m_bUseAngstroms ? 10.f : 1.f) / (oldUse ? 10.f : 1.f);
  UpdateData(false);
}

void CDriftWaitSetupDlg::OnRadioAngle()
{
  UpdateData(true);
  m_editRunAbove.EnableWindow(m_iAngleChoice == 1);
  m_editRunBelow.EnableWindow(m_iAngleChoice == 2);
}

void CDriftWaitSetupDlg::ManageTrialEntries()
{
  m_butSetTrial.EnableWindow(!m_iMeasureType);
  m_statTrialExp.EnableWindow(m_bSetTrial && !m_iMeasureType);
  m_statExpSec.EnableWindow(m_bSetTrial && !m_iMeasureType);
  m_statTrialBinning.EnableWindow(m_bSetTrial && !m_iMeasureType);
  m_editTrialExp.EnableWindow(m_bSetTrial && !m_iMeasureType);
  m_sbcTrialBin.EnableWindow(m_bSetTrial && !m_iMeasureType);
  m_editPriorThresh.EnableWindow(m_bUsePriorAutofocus);
}


void CDriftWaitSetupDlg::OnUsePriorAutofocus()
{
  UpdateData(true);
  ManageTrialEntries();
}
