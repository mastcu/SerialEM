// VPPConditionSetup.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "VPPConditionSetup.h"
#include "EMscope.h"
#include "MultiTSTasks.h"

#define MIN_COND_TIME  5
#define MAX_COND_TIME 1000
#define MIN_COND_CHARGE 5
#define MAX_COND_CHARGE 250
#define MIN_MOVE_DELAY 3
#define MAX_MOVE_DELAY 120

// CVPPConditionSetup dialog

CVPPConditionSetup::CVPPConditionSetup(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_VPP_EXPOSE_SETUP, pParent)
  , m_strMag(_T(""))
  , m_strSpot(_T(""))
  , m_strIntensity(_T(""))
  , m_iWhichSettings(0)
  , m_bSetTrackState(FALSE)
  , m_strAlpha(_T(""))
  , m_strProbeMode(_T(""))
  , m_iCookTime(MIN_COND_TIME)
  , m_iCharge(MIN_COND_CHARGE)
  , m_bCondAtNavPoint(FALSE)
  , m_iLabelOrNote(0)
  , m_strNavText(_T(""))
  , m_iPostMoveDelay(MIN_MOVE_DELAY)
{
  mParams.magIndex = -1;
  mParams.lowDoseArea = -1;
  mNonModal = true;
}

CVPPConditionSetup::~CVPPConditionSetup()
{
}

void CVPPConditionSetup::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_COOKMAG, m_strMag);
  DDX_Text(pDX, IDC_COOKSPOT, m_strSpot);
  DDX_Text(pDX, IDC_COOKINTENSITY, m_strIntensity);
  DDX_Control(pDX, IDC_COOKMAG, m_statMag);
  DDX_Control(pDX, IDC_COOKSPOT, m_statSpot);
  DDX_Control(pDX, IDC_COOKINTENSITY, m_statIntensity);
  DDX_Radio(pDX, IDC_RLDRECORD, m_iWhichSettings);
  DDX_Control(pDX, IDC_SET_TRACK_STATE, m_butSetTrackState);
  DDX_Check(pDX, IDC_SET_TRACK_STATE, m_bSetTrackState);
  DDX_Control(pDX, IDC_STAT_ALPHA, m_statAlpha);
  DDX_Text(pDX, IDC_STAT_ALPHA, m_strAlpha);
  DDX_Control(pDX, IDC_STAT_PROBEMODE, m_statProbeMode);
  DDX_Text(pDX, IDC_STAT_PROBEMODE, m_strProbeMode);
  DDX_Control(pDX, IDC_EDITPPCONDTIME, m_editCookTime);
  DDX_Text(pDX, IDC_EDITPPCONDTIME, m_iCookTime);
  MinMaxInt(IDC_EDITPPCONDTIME, m_iCookTime, MIN_COND_TIME, MAX_COND_TIME, 
    "Exposure time");
  DDX_Control(pDX, IDC_EDITPPCONDDOSE, m_editCookCharge);
  DDX_Text(pDX, IDC_EDITPPCONDDOSE, m_iCharge);
  MinMaxInt(IDC_EDITPPCONDDOSE, m_iCharge, MIN_COND_CHARGE, MAX_COND_CHARGE, 
    "Exposure charge");
  DDX_Check(pDX, IDC_COND_AT_NAV_POINT, m_bCondAtNavPoint);
  DDX_Control(pDX, IDC_RWITHLABEL, m_butWithLabel);
  DDX_Control(pDX, IDC_RWITHNOTE, m_butWithNote);
  DDX_Radio(pDX, IDC_RWITHLABEL, m_iLabelOrNote);
  DDX_Control(pDX, IDC_EDITNAVTEXT, m_editNavText);
  DDX_Text(pDX, IDC_EDITNAVTEXT, m_strNavText);
  DDX_Control(pDX, IDC_STAT_STARTING, m_statStarting);
  DDX_Control(pDX, IDC_SPINPPCONDDOSE, m_sbcCharge);
  DDX_Control(pDX, IDC_SPINPPCONDTIME, m_sbcCookTime);
  DDX_Radio(pDX, IDC_RCONDDOSE, m_iTimeInstead);
  DDX_Control(pDX, IDC_PPCONDGO, m_butPPcondGo);
  DDX_Text(pDX, IDC_EDIT_POST_MOVE_DELAY, m_iPostMoveDelay);
  MinMaxInt(IDC_EDIT_POST_MOVE_DELAY, m_iPostMoveDelay, MIN_MOVE_DELAY, MAX_MOVE_DELAY,
    "Delay if moving to next position");
  DDX_Control(pDX, IDC_PPCOND_NEXT_AND_GO, m_butNextAndGo);
}


BEGIN_MESSAGE_MAP(CVPPConditionSetup, CBaseDlg)
  ON_BN_CLICKED(IDC_RLDRECORD, OnRLDrecord)
  ON_BN_CLICKED(IDC_RLDTRIAL, OnRLDrecord)
  ON_BN_CLICKED(IDC_RRECORDEDHERE, OnRLDrecord)
  ON_BN_CLICKED(IDC_SET_TRACK_STATE, OnSetTrackState)
  ON_BN_CLICKED(IDC_COND_AT_NAV_POINT, OnCondAtNavPoint)
  ON_BN_CLICKED(IDC_PPCONDGO, OnCloseAndGo)
  ON_BN_CLICKED(IDC_RCONDDOSE, OnRcondCharge)
  ON_BN_CLICKED(IDC_RCONDTIME, OnRcondCharge)
  ON_BN_CLICKED(IDC_RWITHLABEL, OnRwithLabel)
  ON_BN_CLICKED(IDC_RWITHNOTE, OnRwithLabel)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINPPCONDDOSE, OnDeltaposSpinPPcondCharge)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINPPCONDTIME, OnDeltaposSpinPPcondTime)
  ON_BN_CLICKED(IDC_PPCOND_NEXT_AND_GO, &CVPPConditionSetup::OnCloseNextAndGo)
  ON_EN_KILLFOCUS(IDC_EDITPPCONDDOSE, &CVPPConditionSetup::OnKillfocusPPcondCharge)
  ON_EN_KILLFOCUS(IDC_EDITPPCONDTIME, &CVPPConditionSetup::OnKillfocusPPcondTime)
END_MESSAGE_MAP()


// CVPPConditionSetup message handlers

BOOL CVPPConditionSetup::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mScope = mWinApp->mScope;
  mMultiTasks = mWinApp->mMultiTSTasks;
  mMagTab = mWinApp->GetMagTable();
  mHasAlpha = !mScope->GetHasNoAlpha();
  ShowDlgItem(IDC_STAT_ALPHA, mHasAlpha);
  if (mHasAlpha)
    m_strProbeMode = "Alpha:";
  ShowDlgItem(IDC_STAT_PROBEMODE, FEIscope || mHasAlpha);
  m_iCookTime = mParams.seconds;
  m_iCharge = mParams.nanoCoulombs;
  m_iWhichSettings = mParams.whichSettings;
  m_iTimeInstead = mParams.timeInstead ? 1 : 0;
  m_bCondAtNavPoint = mParams.useNearestNav;
  m_iLabelOrNote = mParams.useNavNote ? 1 : 0;
  m_strNavText = mParams.navText;
  m_iPostMoveDelay = mParams.postMoveDelay;
  m_editCookCharge.EnableWindow(m_iTimeInstead == 0);
  m_editCookTime.EnableWindow(m_iTimeInstead != 0);
  m_sbcCharge.EnableWindow(m_iTimeInstead == 0);
  m_sbcCookTime.EnableWindow(m_iTimeInstead != 0);
  m_sbcCharge.SetRange(MIN_COND_CHARGE, MAX_COND_CHARGE);
  m_sbcCharge.SetPos(m_iCharge);
  m_sbcCookTime.SetRange(MIN_COND_TIME, MAX_COND_TIME);
  m_sbcCookTime.SetPos(m_iCookTime);
  mSavedParams = mParams;
  mLastLowDose = mWinApp->LowDoseMode();

  ManageNavEnables();
  ManageSettingEnables();
  DisplaySettings();
  UpdateData(false);
  SetDefID(45678);    // Disable OK from being default button

  return TRUE;
}

// Closing buttons send codes on what to do
void CVPPConditionSetup::OnOK()
{
  StoreParameters();
  mMultiTasks->VPPConditionClosing(1);
}

void CVPPConditionSetup::OnCancel()
{
  mMultiTasks->VPPConditionClosing(0);
}


void CVPPConditionSetup::OnCloseAndGo()
{
  StoreParameters();
  mMultiTasks->VPPConditionClosing(-1);
}

void CVPPConditionSetup::OnCloseNextAndGo()
{
  StoreParameters();
  mMultiTasks->VPPConditionClosing(-2);
}

void CVPPConditionSetup::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

// Save current settings back into the param
void CVPPConditionSetup::StoreParameters()
{
  UpdateData(true);
  mParams.seconds = m_iCookTime;
  mParams.nanoCoulombs = m_iCharge;
  mParams.whichSettings = m_iWhichSettings;
  mParams.timeInstead = m_iTimeInstead != 0;
  mParams.useNearestNav = m_bCondAtNavPoint;
  mParams.useNavNote = m_iLabelOrNote != 0;
  mParams.navText = m_strNavText;
  mParams.postMoveDelay = m_iPostMoveDelay;
}

BOOL CVPPConditionSetup::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Choice of settings chnages - if switching away from using set state, stop tracking
void CVPPConditionSetup::OnRLDrecord()
{
  UpdateData(true);
  ManageSettingEnables();
  if (m_bSetTrackState && m_iWhichSettings != VPPCOND_SETTINGS) {
    SetScopeState(true);
    m_bSetTrackState = false;
    UpdateData(false);
  }
  mWinApp->RestoreViewFocus();
}

// Turn tracking of state on or off
void CVPPConditionSetup::OnSetTrackState()
{
  SetScopeState(true);
  UpdateData(true);
  if (m_bSetTrackState) {

    // Save parameters regardless of low dose being on if they need to be copied
    mMultiTasks->SaveScopeStateInVppParams(&mSavedParams, mParams.magIndex < 0);
    if (mParams.magIndex >= 0) {
      SetScopeState(false);
    } else {
      mParams = mSavedParams;
      DisplaySettings();
    }
  }
  ManageSettingEnables();
  mWinApp->RestoreViewFocus();
}

// Switch between doing charge or time
void CVPPConditionSetup::OnRcondCharge()
{
  UpdateData(true);
  m_editCookCharge.EnableWindow(m_iTimeInstead == 0);
  m_editCookTime.EnableWindow(m_iTimeInstead != 0);
  m_sbcCharge.EnableWindow(m_iTimeInstead == 0);
  m_sbcCookTime.EnableWindow(m_iTimeInstead != 0);
  mWinApp->RestoreViewFocus();
}

// Use label or note
void CVPPConditionSetup::OnRwithLabel()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// New charge
void CVPPConditionSetup::OnKillfocusPPcondCharge()
{
  UpdateData(true);
  m_sbcCharge.SetPos(m_iCharge);
  mWinApp->RestoreViewFocus();
}

// new time
void CVPPConditionSetup::OnKillfocusPPcondTime()
{
  UpdateData(true);
  m_sbcCookTime.SetPos(m_iCookTime);
  mWinApp->RestoreViewFocus();
}

// Charge spinner
void CVPPConditionSetup::OnDeltaposSpinPPcondCharge(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (!NewSpinnerValue(pNMHDR, pResult, MIN_COND_CHARGE, MAX_COND_CHARGE, m_iCharge))
    UpdateData(false);
  mWinApp->RestoreViewFocus();
}

// Time spinner
void CVPPConditionSetup::OnDeltaposSpinPPcondTime(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (!NewSpinnerValue(pNMHDR, pResult, MIN_COND_TIME, MAX_COND_TIME, m_iCookTime))
    UpdateData(false);
  mWinApp->RestoreViewFocus();
}

void CVPPConditionSetup::OnCondAtNavPoint()
{
  UpdateData(true);
  ManageNavEnables();
  mWinApp->RestoreViewFocus();
}

// Manage the enabling related to the setting choice
void CVPPConditionSetup::ManageSettingEnables()
{
  bool enable = m_iWhichSettings == VPPCOND_SETTINGS;
  m_butSetTrackState.EnableWindow(enable);
  if (FEIscope)
    m_statProbeMode.EnableWindow(enable);
  m_statMag.EnableWindow(enable);
  m_statSpot.EnableWindow(enable);
  m_statIntensity.EnableWindow(enable);
  m_statAlpha.EnableWindow(enable);
  enable = (enable && mParams.magIndex >= 0) || (!enable && mWinApp->LowDoseMode());
  m_butPPcondGo.EnableWindow(enable);
  m_butNextAndGo.EnableWindow(enable);
}

// Manges enables related to using the Nav point
void CVPPConditionSetup::ManageNavEnables()
{
  m_butWithLabel.EnableWindow(m_bCondAtNavPoint);
  m_butWithNote.EnableWindow(m_bCondAtNavPoint);
  m_statStarting.EnableWindow(m_bCondAtNavPoint);
  m_editNavText.EnableWindow(m_bCondAtNavPoint);
}

// Update the scope settings shown
void CVPPConditionSetup::DisplaySettings()
{
  if (mParams.magIndex <= 0)
    return;
  m_strMag.Format("%d", mMagTab[mParams.magIndex].mag);
  m_strSpot.Format("%d", mParams.spotSize);
  m_strIntensity.Format("%.2f%s %s", mScope->GetC2Percent(mParams.spotSize, 
    mParams.intensity, mParams.probeMode),
    mScope->GetC2Units(), mScope->GetC2Name());
  m_strAlpha.Format("%d", mParams.alpha + 1);
  if (FEIscope)
    m_strProbeMode = mParams.probeMode ? "Microprobe" : "Nanoprobe";
  UpdateData(false);
}

// This is called unconditionally by ScopeStatusDlg and the test for change happens here
void CVPPConditionSetup::LiveUpdate(int magInd, int spotSize, int probe, double intensity,
  int alpha)
{
  if (!BOOL_EQUIV(mLastLowDose, mWinApp->LowDoseMode())) {
    mLastLowDose = !mLastLowDose;
    ManageSettingEnables();
  }
  if (!m_bSetTrackState || !magInd)
    return;
  if (magInd == mParams.magIndex && spotSize == mParams.spotSize && probe == 
    mParams.probeMode && intensity == mParams.intensity && alpha == mParams.alpha)
    return;
  mParams.magIndex = magInd;
  mParams.spotSize = spotSize;
  mParams.intensity = intensity;
  mParams.alpha = alpha;
  mParams.probeMode = probe;
  DisplaySettings();
}

// Sets scope state into param or vice versa depending on whether tracking is on
// When restoring, use stored LD area, but when setting, ignore that and use actual state
void CVPPConditionSetup::SetScopeState(bool restore)
{
  if (!m_bSetTrackState)
    return;
  mMultiTasks->SetScopeStateFromVppParams(restore ? &mSavedParams : &mParams, !restore);
}
