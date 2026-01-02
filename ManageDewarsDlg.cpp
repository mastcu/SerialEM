// ManageDewarsDlg.cpp : Dialog for specifying periodic actions and checks for nitrogen
// dewars and vacuum pumps
//
// Copyright (C) 2021-2026 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "ManageDewarsDlg.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static int sIdTable[] = {IDC_CHECK_PVP_RUNNING, IDC_RUN_BUFFER_CYCLE,
IDC_EDIT_BUFFER_TIME, IDC_STAT_BUFFER_MIN, IDC_RUN_AUTOLOADER_CYCLE, IDC_STAT_LINE,
IDC_EDIT_AUTOLOADER_TIME, IDC_STAT_AUTOLOADER_MIN, IDC_STAT_VACUUM_TITLE, PANEL_END,
IDC_STAT_DEWAR_TITLE, IDC_REFILL_DEWARS, IDC_EDIT_REFILL_TIME, IDC_CHECK_DEWARS,
IDC_STAT_REFILL_HOURS, PANEL_END,
IDC_STAT_PAUSE_BEFORE, IDC_EDIT_PAUSE_BEFORE, IDC_STAT_BEFORE_MIN,
IDC_START_REFILL, IDC_EDIT_START_IF_BELOW, IDC_STAT_START_BELOW_MIN, PANEL_END,
IDC_STAT_WAIT_TIME, IDC_EDIT_WAIT_AFTER, IDC_STAT_AFTER_MIN, PANEL_END,
IDC_DO_CHECKS_JUST_BEFORE, IDC_STAT_WHEN_OCCURS, PANEL_END,
IDOK, IDCANCEL, IDC_BUTHELP, IDC_STAT_SPACER, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

// CManageDewarsDlg dialog

CManageDewarsDlg::CManageDewarsDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MANAGE_DEWARS, pParent)
  , m_bCheckPVPrunning(FALSE)
  , m_bRunBufferCycle(FALSE)
  , m_iBufferTime(10)
  , m_bRunAutoloader(FALSE)
  , m_iAutoloaderTime(10)
  , m_bRefillDewars(FALSE)
  , m_fRefillTime(1.)
  , m_bCheckDewars(FALSE)
  , m_fPauseBefore(1.)
  , m_bStartRefill(FALSE)
  , m_fStartIfBelow(0.)
  , m_fWaitAfter(1.)
  , m_bDoCheckJustBefore(FALSE)
{

}

CManageDewarsDlg::~CManageDewarsDlg()
{
}

void CManageDewarsDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK_PVP_RUNNING, m_bCheckPVPrunning);
  DDX_Check(pDX, IDC_RUN_BUFFER_CYCLE, m_bRunBufferCycle);
  DDX_Control(pDX, IDC_EDIT_BUFFER_TIME, m_editBufferTime);
  DDX_Text(pDX, IDC_EDIT_BUFFER_TIME, m_iBufferTime);
  DDV_MinMaxInt(pDX, m_iBufferTime, 10, 500);
  DDX_Check(pDX, IDC_RUN_AUTOLOADER_CYCLE, m_bRunAutoloader);
  DDX_Control(pDX, IDC_EDIT_AUTOLOADER_TIME, m_editAutoloaderTime);
  DDX_Text(pDX, IDC_EDIT_AUTOLOADER_TIME, m_iAutoloaderTime);
  DDV_MinMaxInt(pDX, m_iAutoloaderTime, 10, 500);
  DDX_Check(pDX, IDC_REFILL_DEWARS, m_bRefillDewars);
  DDX_Control(pDX, IDC_EDIT_REFILL_TIME, m_editRefillTime);
  DDX_Text(pDX, IDC_EDIT_REFILL_TIME, m_fRefillTime);
  DDV_MinMaxFloat(pDX, m_fRefillTime, 0.2f, 20.f);
  DDX_Check(pDX, IDC_CHECK_DEWARS, m_bCheckDewars);
  DDX_Control(pDX, IDC_EDIT_PAUSE_BEFORE, m_editPauseBefore);
  DDX_Text(pDX, IDC_EDIT_PAUSE_BEFORE, m_fPauseBefore);
  DDV_MinMaxFloat(pDX, m_fPauseBefore, .1f, 100.f);
  DDX_Control(pDX, IDC_START_REFILL, m_butStartRefill);
  DDX_Check(pDX, IDC_START_REFILL, m_bStartRefill);
  DDX_Control(pDX, IDC_EDIT_START_IF_BELOW, m_editStartIfBelow);
  DDX_Text(pDX, IDC_EDIT_START_IF_BELOW, m_fStartIfBelow);
  DDV_MinMaxFloat(pDX, m_fStartIfBelow, 0., 10000.f);
  DDX_Control(pDX, IDC_EDIT_WAIT_AFTER, m_editWaitAfter);
  DDX_Text(pDX, IDC_EDIT_WAIT_AFTER, m_fWaitAfter);
  DDV_MinMaxFloat(pDX, m_fWaitAfter, 0.1f, 20.f);
  DDX_Control(pDX, IDC_DO_CHECKS_JUST_BEFORE, m_butDoCheckBeforeTask);
  DDX_Check(pDX, IDC_DO_CHECKS_JUST_BEFORE, m_bDoCheckJustBefore);
}


BEGIN_MESSAGE_MAP(CManageDewarsDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_PVP_RUNNING, OnCheckPvpRunning)
  ON_BN_CLICKED(IDC_RUN_BUFFER_CYCLE, OnRunBufferCycle)
  ON_BN_CLICKED(IDC_RUN_AUTOLOADER_CYCLE, OnRunAutoloaderCycle)
  ON_BN_CLICKED(IDC_REFILL_DEWARS, OnRefillDewars)
  ON_EN_KILLFOCUS(IDC_EDIT_BUFFER_TIME, OnEnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_AUTOLOADER_TIME, OnEnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_REFILL_TIME, OnEnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_PAUSE_BEFORE, OnEnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_START_IF_BELOW, OnEnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_WAIT_AFTER, OnEnKillfocusEditBox)
  ON_BN_CLICKED(IDC_CHECK_DEWARS, OnCheckDewars)
  ON_BN_CLICKED(IDC_START_REFILL, OnStartRefill)
  ON_BN_CLICKED(IDC_DO_CHECKS_JUST_BEFORE, OnDoChecksJustBefore)
END_MESSAGE_MAP()


// CManageDewarsDlg message handlers
BOOL CManageDewarsDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  DewarVacParams *param = mWinApp->mScope->GetDewarVacParams();
  BOOL states[6] = {true, true, true,true, true, true};
  int capabilities = mWinApp->mScope->GetDewarVacCapabilities();
  BOOL simpleOrig = mWinApp->mScope->GetHasSimpleOrigin();
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);

  // Drop items or whole panels based on properties
  states[0] = FEIscope;
  mUsingVibrationManager = mWinApp->mScope->UtapiSupportsService(UTSUP_VIBRATION);
  if ((capabilities & 1) == 0 || mUsingVibrationManager) {
    mIDsToDrop.push_back(IDC_RUN_AUTOLOADER_CYCLE);
    mIDsToDrop.push_back(IDC_EDIT_AUTOLOADER_TIME);
    mIDsToDrop.push_back(IDC_STAT_AUTOLOADER_MIN);
  }
  if ((capabilities & 2) == 0 && !simpleOrig) {
    states[1] = states[2] = states[3] = false;
    mIDsToDrop.push_back(IDC_STAT_LINE);
  }
  if (JEOLscope && !simpleOrig)
    states[2] = false;

  if (mUsingVibrationManager) {
    SetDlgItemText(IDC_STAT_DEWAR_TITLE, "Vibration Avoidance and Nitrogen Management");
    SetDlgItemText(IDC_REFILL_DEWARS, "Prepare to avoid every");
    SetDlgItemText(IDC_CHECK_DEWARS, "Check for ongoing filling or vibrations and wait"
      " until done");
    SetDlgItemText(IDC_START_REFILL, "Prepare to avoid if time to next refill is below");
    mIDsToDrop.push_back(IDC_STAT_PAUSE_BEFORE);
    mIDsToDrop.push_back(IDC_EDIT_PAUSE_BEFORE);
    mIDsToDrop.push_back(IDC_STAT_BEFORE_MIN);
  }

  // Load data from master params
  m_bCheckPVPrunning = param->checkPVP;
  m_bRunBufferCycle = param->runBufferCycle;
  m_iBufferTime = param->bufferTimeMin;
  m_bRunAutoloader = param->runAutoloaderCycle;
  m_iAutoloaderTime = param->autoloaderTimeMin;
  m_bRefillDewars = param->refillAtInterval;
  m_fRefillTime = param->dewarTimeHours;
  m_bCheckDewars = param->checkDewars;
  m_fPauseBefore = param->pauseBeforeMin;
  m_bStartRefill = param->startRefillEarly;
  m_fStartIfBelow = param->startIntervalMin;
  m_fWaitAfter = param->postFillWaitMin;
  m_bDoCheckJustBefore = param->doChecksBeforeTask;
  ManageEnables();
  UpdateData(false);
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
  return TRUE;
}

// unload data into master param
void CManageDewarsDlg::OnOK()
{
  UpdateData(true);
  DewarVacParams *param = mWinApp->mScope->GetDewarVacParams();
  param->checkPVP = m_bCheckPVPrunning;
  param->runBufferCycle = m_bRunBufferCycle;
  param->bufferTimeMin = m_iBufferTime;
  param->runAutoloaderCycle = m_bRunAutoloader;
  param->autoloaderTimeMin = m_iAutoloaderTime;
  param->refillAtInterval = m_bRefillDewars;
  param->dewarTimeHours = m_fRefillTime;
  param->checkDewars = m_bCheckDewars;
  param->pauseBeforeMin = m_fPauseBefore;
  param->startRefillEarly = m_bStartRefill;
  param->startIntervalMin = m_fStartIfBelow;
  param->postFillWaitMin = m_fWaitAfter;
  param->doChecksBeforeTask = m_bDoCheckJustBefore;
  CBaseDlg::OnOK();
}

// Check PVP
void CManageDewarsDlg::OnCheckPvpRunning()
{
  UpdateData(true);
  ManageEnables();
}

// Run regular buffer cycle
void CManageDewarsDlg::OnRunBufferCycle()
{
  UpdateData(true);
  ManageEnables();
}

// Run autoloader buffer cycle
void CManageDewarsDlg::OnRunAutoloaderCycle()
{
  UpdateData(true);
  ManageEnables();
}

// Refill dewars
void CManageDewarsDlg::OnRefillDewars()
{
  UpdateData(true);
  ManageEnables();
}

// General edit box update
void CManageDewarsDlg::OnEnKillfocusEditBox()
{
  UpdateData(true);
}

// Check dewars
void CManageDewarsDlg::OnCheckDewars()
{
  UpdateData(true);
  ManageEnables();
}

// Start if timing indicates
void CManageDewarsDlg::OnStartRefill()
{
  UpdateData(true);
  ManageEnables();
}

// Do checks just before acquire
void CManageDewarsDlg::OnDoChecksJustBefore()
{
  UpdateData(true);
}

// Manage conditional enabling
void CManageDewarsDlg::ManageEnables()
{
  m_editBufferTime.EnableWindow(m_bRunBufferCycle);
  m_editAutoloaderTime.EnableWindow(m_bRunAutoloader);
  m_editRefillTime.EnableWindow(m_bRefillDewars);
  m_editPauseBefore.EnableWindow(m_bCheckDewars);
  EnableDlgItem(IDC_STAT_PAUSE_BEFORE, m_bCheckDewars);
  EnableDlgItem(IDC_STAT_BEFORE_MIN, m_bCheckDewars);
  m_editStartIfBelow.EnableWindow(m_bStartRefill && m_bCheckDewars);
  m_butStartRefill.EnableWindow(m_bCheckDewars);
  EnableDlgItem(IDC_STAT_START_BELOW_MIN, m_bCheckDewars);
  m_editWaitAfter.EnableWindow(m_bRefillDewars || m_bCheckDewars);
  EnableDlgItem(IDC_STAT_WAIT_TIME, m_bRefillDewars || m_bCheckDewars);
  EnableDlgItem(IDC_STAT_AFTER_MIN, m_bRefillDewars || m_bCheckDewars);
  m_butDoCheckBeforeTask.EnableWindow(m_bCheckPVPrunning || m_bCheckDewars);
}
