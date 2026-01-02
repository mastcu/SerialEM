// TSPolicyDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\TSPolicyDlg.h"
#include "EMscope.h"
#include "TSController.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// CTSPolicyDlg dialog

CTSPolicyDlg::CTSPolicyDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CTSPolicyDlg::IDD, pParent)
  , m_iTiltFail(0)
  , m_iDimRecord(0)
  , m_iRecordLoss(0)
  , m_iErrorMessage(0)
  , m_bAutoNewLog(FALSE)
  , m_bMBValves(FALSE)
  , m_fMBMinutes(0.f)
  , m_bEndBidir(FALSE)
  , m_iDimEndsAbsAngle(0)
  , m_iDimEndsAngleDiff(0)
  , m_bEndHighExp(FALSE)
  , m_fMaxDeltaExp(0.)
{
}

CTSPolicyDlg::~CTSPolicyDlg()
{
}

void CTSPolicyDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_ERRORMESSAGE, m_iErrorMessage);
  DDX_Radio(pDX, IDC_TILTTERM, m_iTiltFail);
  DDX_Radio(pDX, IDC_DIM_DO_MESSAGE, m_iDimTermNotBox);
  DDX_Radio(pDX, IDC_LDDIMREPEAT, m_iDimRecord);
  DDX_Radio(pDX, IDC_LDLOSSREPEAT, m_iRecordLoss);
  DDX_Check(pDX, IDC_CHECK_AUTO_NEW_LOG, m_bAutoNewLog);
  DDX_Control(pDX, IDC_CHECK_MB_VALVES, m_butMBValves);
  DDX_Check(pDX, IDC_CHECK_MB_VALVES, m_bMBValves);
  DDX_Control(pDX, IDC_EDIT_MB_MINUTES, m_editMBMinutes);
  DDX_Text(pDX, IDC_EDIT_MB_MINUTES, m_fMBMinutes);
  DDV_MinMaxFloat(pDX, m_fMBMinutes, 0.25f, 120.f);
  DDX_Control(pDX, IDC_TILTTERM, m_butTiltTerm);
  DDX_Control(pDX, IDC_TILTCONT, m_butTiltCont);
  DDX_Control(pDX, IDC_LDLOSSREPEAT, m_butLDlossRepeat);
  DDX_Control(pDX, IDC_LDLOSSCONT, m_butLDlossCont);
  DDX_Control(pDX, IDC_LDLOSSTERM, m_butLDlossTerm);
  DDX_Control(pDX, IDC_LDDIMREPEAT, m_butLDdimRepeat);
  DDX_Control(pDX, IDC_LDDIMCONT, m_butLDdimCont);
  DDX_Control(pDX, IDC_LDDIMTERM, m_butLDdimTerm);
  DDX_Control(pDX, IDC_DIM_TERM_NOT_BOX, m_butDimTermNotBox);
  DDX_Control(pDX, IDC_DIM_DO_MESSAGE, m_butDimDoMessage);
  DDX_Control(pDX, IDC_STAT_TILT_FAIL, m_statTiltFail);
  DDX_Control(pDX, IDC_STAT_DIM_TERM_NOT_BOX, m_statDimTermNotBox);
  DDX_Control(pDX, IDC_STAT_LD_LOSS, m_statLDloss);
  DDX_Control(pDX, IDC_STAT_LD_DIM, m_statLDdim);
  DDX_Check(pDX, IDC_CHECK_END_BIDIR, m_bEndBidir);
  DDX_Control(pDX, IDC_EDIT_BIDIR_ABS_ANGLE, m_editDimEndsAbsAngle);
  DDX_Control(pDX, IDC_EDIT_BIDIR_END_DIFF, m_editBidirEndDiff);
  DDX_Text(pDX, IDC_EDIT_BIDIR_ABS_ANGLE, m_iDimEndsAbsAngle);
  DDV_MinMaxInt(pDX, m_iDimEndsAbsAngle, 20, 90);
  DDX_Text(pDX, IDC_EDIT_BIDIR_END_DIFF, m_iDimEndsAngleDiff);
  DDV_MinMaxInt(pDX, m_iDimEndsAngleDiff, 1, 50);
  DDX_Control(pDX, IDC_STAT_MAX_DELTA1, m_statMaxDelta1);
  DDX_Control(pDX, IDC_STAT_MAX_DELTA2, m_statMaxDelta2);
  DDX_Control(pDX, IDC_CHECK_END_HIGH_EXP, m_butEndHighExp);
  DDX_Check(pDX, IDC_CHECK_END_HIGH_EXP, m_bEndHighExp);
  DDX_Control(pDX, IDC_EDIT_MAX_DELTA_EXP, m_editMaxDeltaExp);
  DDX_Text(pDX, IDC_EDIT_MAX_DELTA_EXP, m_fMaxDeltaExp);
  DDV_MinMaxFloat(pDX, m_fMaxDeltaExp, 1.0f, 10.f);
}


BEGIN_MESSAGE_MAP(CTSPolicyDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_MB_VALVES, OnCheckMbValves)
  ON_BN_CLICKED(IDC_ERRORMESSAGE, OnErrorMessage)
  ON_BN_CLICKED(IDC_ERRORTERM, OnErrorMessage)
  ON_BN_CLICKED(IDC_CHECK_END_BIDIR, OnCheckEndBidir)
  ON_BN_CLICKED(IDC_CHECK_END_HIGH_EXP, OnCheckEndHighExp)
END_MESSAGE_MAP()


// CTSPolicyDlg message handlers
void CTSPolicyDlg::OnOK()
{
  UpdateData(true);
  mTSController->SetLDDimRecordPolicy(m_iDimRecord);
  mTSController->SetLDRecordLossPolicy(m_iRecordLoss);
  mTSController->SetTiltFailPolicy(m_iTiltFail);
  mTSController->SetAutoTerminatePolicy(m_iErrorMessage);
  mTSController->SetTermNotAskOnDim(m_iDimTermNotBox);
  mTSController->SetMessageBoxCloseValves(m_bMBValves);
  mTSController->SetAutoSavePolicy(m_bAutoNewLog);
  mTSController->SetMessageBoxValveTime(m_fMBMinutes);
  mTSController->SetEndOnHighDimImage(m_bEndBidir);
  mTSController->SetDimEndsAbsAngle(m_iDimEndsAbsAngle);
  mTSController->SetDimEndsAngleDiff(m_iDimEndsAngleDiff);
  mTSController->SetTermOnHighExposure(m_bEndHighExp);
  mTSController->SetMaxExposureIncrease(m_fMaxDeltaExp);
	CBaseDlg::OnOK();
}

BOOL CTSPolicyDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  mTSController = mWinApp->mTSController;
  m_iDimRecord = mTSController->GetLDDimRecordPolicy();
  m_iRecordLoss = mTSController->GetLDRecordLossPolicy();
  m_iTiltFail = mTSController->GetTiltFailPolicy();
  m_iErrorMessage = mTSController->GetAutoTerminatePolicy();
  m_iDimTermNotBox = mTSController->GetTermNotAskOnDim();
  m_bMBValves = mTSController->GetMessageBoxCloseValves();
  m_bAutoNewLog = mTSController->GetAutoSavePolicy();
  m_fMBMinutes = mTSController->GetMessageBoxValveTime();
  if (mWinApp->mScope->GetNoColumnValve())
    ReplaceWindowText(&m_butMBValves, "Close column valves", "Turn off filament");
  else if (JEOLscope)
    ReplaceWindowText(&m_butMBValves, "column valves", "gun valve");
  m_editMBMinutes.EnableWindow(m_bMBValves);
  ManagePolicies();
  m_bEndBidir = mTSController->GetEndOnHighDimImage();
  m_iDimEndsAbsAngle = mTSController->GetDimEndsAbsAngle();
  m_iDimEndsAngleDiff = mTSController->GetDimEndsAngleDiff();
  m_bEndHighExp = mTSController->GetTermOnHighExposure();
  m_fMaxDeltaExp = mTSController->GetMaxExposureIncrease();
  ManageEndBidir();
  UpdateData(false);
  return TRUE;
}

void CTSPolicyDlg::OnCheckMbValves()
{
  UpdateData(true);
  m_editMBMinutes.EnableWindow(m_bMBValves);
}

void CTSPolicyDlg::OnErrorMessage()
{
  UpdateData(true);
  ManagePolicies();
}

void CTSPolicyDlg::ManagePolicies(void)
{
   m_butTiltTerm.EnableWindow(m_iErrorMessage != 0);
   m_butTiltCont.EnableWindow(m_iErrorMessage != 0);
   m_statTiltFail.EnableWindow(m_iErrorMessage != 0);
   m_butLDlossRepeat.EnableWindow(m_iErrorMessage != 0);
   m_butLDlossCont.EnableWindow(m_iErrorMessage != 0);
   m_butLDlossTerm.EnableWindow(m_iErrorMessage != 0);
   m_statLDloss.EnableWindow(m_iErrorMessage != 0);
   m_butLDdimRepeat.EnableWindow(m_iErrorMessage != 0);
   m_butLDdimCont.EnableWindow(m_iErrorMessage != 0);
   m_butLDdimTerm.EnableWindow(m_iErrorMessage != 0);
   m_statLDdim.EnableWindow(m_iErrorMessage != 0);
   m_butDimTermNotBox.EnableWindow(m_iErrorMessage == 0);
   m_butDimDoMessage.EnableWindow(m_iErrorMessage == 0);
   m_statDimTermNotBox.EnableWindow(m_iErrorMessage == 0);
}

void CTSPolicyDlg::OnCheckEndBidir()
{
  UpdateData(true);
  ManageEndBidir();
}


void CTSPolicyDlg::OnCheckEndHighExp()
{
  UpdateData(true);
  ManageEndBidir();
}


void CTSPolicyDlg::ManageEndBidir(void)
{
  m_editDimEndsAbsAngle.EnableWindow(m_bEndBidir);
  m_editBidirEndDiff.EnableWindow(m_bEndBidir);
  m_butEndHighExp.EnableWindow(m_bEndBidir);
  m_statMaxDelta1.EnableWindow(m_bEndBidir);
  m_statMaxDelta2.EnableWindow(m_bEndBidir);
  m_editMaxDeltaExp.EnableWindow(m_bEndBidir);
  m_editMaxDeltaExp.EnableWindow(m_bEndHighExp && m_bEndBidir);
}
