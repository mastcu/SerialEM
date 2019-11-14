// VPPConditionSetup.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "VPPConditionSetup.h"
#include "EMscope.h"
#include "MultiTSTasks.h"


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
  , m_iCookTime(0)
  , m_bCondAtNavPoint(FALSE)
  , m_iLabelOrNote(FALSE)
  , m_strNavText(_T(""))
{
  mParams.magIndex = 0;
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
  DDX_Text(pDX, IDC_EDITCOOKTIME, m_iCookTime);
  DDV_MinMaxInt(pDX, m_iCookTime, 1, 1000);
  DDX_Check(pDX, IDC_COND_AT_NAV_POINT, m_bCondAtNavPoint);
  DDX_Control(pDX, IDC_RWITHLABEL, m_butWithLabel);
  DDX_Control(pDX, IDC_RWITHNOTE, m_butWithNote);
  DDX_Radio(pDX, IDC_RWITHLABEL, m_iLabelOrNote);
  DDX_Control(pDX, IDC_EDITNAVTEXT, m_editNavText);
  DDX_Text(pDX, IDC_EDITNAVTEXT, m_strNavText);
  DDX_Control(pDX, IDC_STAT_STARTING, m_statStarting);
}


BEGIN_MESSAGE_MAP(CVPPConditionSetup, CBaseDlg)
  ON_BN_CLICKED(IDC_RLDRECORD, OnRLDrecord)
  ON_BN_CLICKED(IDC_RLDTRIAL, OnRLDrecord)
  ON_BN_CLICKED(IDC_RRECORDEDHERE, OnRLDrecord)
  ON_BN_CLICKED(IDC_SET_TRACK_STATE, OnSetTrackState)
  ON_BN_CLICKED(IDC_COND_AT_NAV_POINT, OnCondAtNavPoint)
  ON_BN_CLICKED(IDC_COOKGO, &CVPPConditionSetup::OnCloseAndGo)
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
  ManageNavEnables();
  ManageSettingEnables();
  DisplaySettings();
  SetDefID(45678);    // Disable OK from being default button

  return TRUE;
}

void CVPPConditionSetup::OnOK()
{
  mMultiTasks->VPPConditionClosing(1);
}

void CVPPConditionSetup::OnCancel()
{
  mMultiTasks->VPPConditionClosing(0);
}


void CVPPConditionSetup::OnCloseAndGo()
{
  mMultiTasks->VPPConditionClosing(-1);
}

void CVPPConditionSetup::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CVPPConditionSetup::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}


void CVPPConditionSetup::OnRLDrecord()
{
  UpdateData(true);
  ManageNavEnables();
  if (m_bSetTrackState && m_iWhichSettings != VPPCOND_SETTINGS) {
    SetScopeState(true);
    m_bSetTrackState = false;
    UpdateData(false);
  }
  mWinApp->RestoreViewFocus();
}


void CVPPConditionSetup::OnSetTrackState()
{
  SetScopeState(true);
  UpdateData(true);
  if (m_bSetTrackState) {
    mMultiTasks->SaveScopeStateInVppParams(&mSavedParams);
    if (mParams.magIndex > 0)
      SetScopeState(false);
  }
  mWinApp->RestoreViewFocus();
}


void CVPPConditionSetup::OnCondAtNavPoint()
{
  UpdateData(true);
  ManageNavEnables();
  mWinApp->RestoreViewFocus();
}


void CVPPConditionSetup::ManageSettingEnables()
{
  bool enable = m_iWhichSettings == VPPCOND_SETTINGS;
  if (FEIscope)
    m_statProbeMode.EnableWindow(enable);
  m_statMag.EnableWindow(enable);
  m_statSpot.EnableWindow(enable);
  m_statIntensity.EnableWindow(enable);
  m_statAlpha.EnableWindow(enable);
}


void CVPPConditionSetup::ManageNavEnables()
{
  m_butWithLabel.EnableWindow(m_bCondAtNavPoint);
  m_butWithNote.EnableWindow(m_bCondAtNavPoint);
  m_statStarting.EnableWindow(m_bCondAtNavPoint);
  m_editNavText.EnableWindow(m_bCondAtNavPoint);
}


void CVPPConditionSetup::DisplaySettings()
{
  m_strMag.Format("%d", mMagTab[mParams.magIndex].screenMag);
  m_strSpot.Format("%d", mParams.spotSize);
  m_strIntensity.Format("%.2f%s %s", mScope->GetC2Percent(mParams.spotSize, 
    mParams.intensity),
    mScope->GetC2Units(), mScope->GetC2Name());
  m_strAlpha.Format("%d", mParams.alpha + 1);
  if (FEIscope)
    m_strProbeMode = mParams.probeMode ? "Microprobe" : "Nanoprobe";
  UpdateData(false);
}

void CVPPConditionSetup::LiveUpdate(int magInd, int spotSize, int probe, double intensity,
  int alpha)
{
  if (!m_bSetTrackState)
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


void CVPPConditionSetup::SetScopeState(bool restore)
{
  if (!m_bSetTrackState)
    return;
  mMultiTasks->SetScopeStateFromVppParams(restore ? &mSavedParams : &mParams);
}
