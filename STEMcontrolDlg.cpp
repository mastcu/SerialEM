// STEMcontrol.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "STEMcontrolDlg.h"
#include "CameraController.h"
#include "EMscope.h"
#include "ShiftManager.h"


// CSTEMcontrolDlg dialog

CSTEMcontrolDlg::CSTEMcontrolDlg(CWnd* pParent /*=NULL*/)
	: CToolDlg(CSTEMcontrolDlg::IDD, pParent)
  , m_bScreenSwitches(FALSE)
  , m_bMatchPixel(FALSE)
  , m_bStemMode(FALSE)
  , m_strOnOff(_T(""))
  , m_bInvertContrast(FALSE)
  , m_strAddedRot(_T(""))
  , m_bBlankInStem(FALSE)
  , m_bRetractToUnblank(FALSE)
{
  mLastProbeMode = -2;
  mLastSTEMmode = -1;
  mLastBlanked = false;
}

CSTEMcontrolDlg::~CSTEMcontrolDlg()
{
}

void CSTEMcontrolDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_SCREEN_SWITCH_STEM, m_butScreenSwitches);
  DDX_Control(pDX, IDC_MATCH_STEM_PIXEL, m_butMatchPixel);
  DDX_Check(pDX, IDC_SCREEN_SWITCH_STEM, m_bScreenSwitches);
  DDX_Check(pDX, IDC_MATCH_STEM_PIXEL, m_bMatchPixel);
  DDX_Control(pDX, IDC_CHECK_STEM_MODE, m_butStemMode);
  DDX_Check(pDX, IDC_CHECK_STEM_MODE, m_bStemMode);
  DDX_Control(pDX, IDC_STATONOFF, m_statOnOff);
  DDX_Text(pDX, IDC_STATONOFF, m_strOnOff);
  DDX_Control(pDX, IDC_CHECK_INVERT_CONTRAST, m_butInvertContrast);
  DDX_Control(pDX, IDC_EDIT_ADDED_ROT, m_editAddedRot);
  DDX_Check(pDX, IDC_CHECK_INVERT_CONTRAST, m_bInvertContrast);
  DDX_Text(pDX, IDC_EDIT_ADDED_ROT, m_strAddedRot);
  DDX_Control(pDX, IDC_BUT_SET_IS_NEUTRAL, m_butSetISneutral);
  DDX_Control(pDX, IDC_BLANK_IN_STEM, m_butBlankInStem);
  DDX_Check(pDX, IDC_BLANK_IN_STEM, m_bBlankInStem);
  DDX_Control(pDX, IDC_RETRACT_TO_UNBLANK, m_butRetractToUnblank);
  DDX_Check(pDX, IDC_RETRACT_TO_UNBLANK, m_bRetractToUnblank);
  DDX_Control(pDX, IDC_UNBLANK_STEM, m_butUnblank);
}


BEGIN_MESSAGE_MAP(CSTEMcontrolDlg, CToolDlg)
  ON_BN_CLICKED(IDC_SCREEN_SWITCH_STEM, &CSTEMcontrolDlg::OnScreenSwitchStem)
  ON_BN_CLICKED(IDC_MATCH_STEM_PIXEL, &CSTEMcontrolDlg::OnMatchStemPixel)
  ON_BN_CLICKED(IDC_CHECK_STEM_MODE, &CSTEMcontrolDlg::OnStemMode)
  ON_EN_KILLFOCUS(IDC_EDIT_ADDED_ROT, &CSTEMcontrolDlg::OnKillfocusAddedRot)
  ON_BN_CLICKED(IDC_CHECK_INVERT_CONTRAST, &CSTEMcontrolDlg::OnInvertContrast)
  ON_BN_CLICKED(IDC_BUT_SET_IS_NEUTRAL, &CSTEMcontrolDlg::OnSetIsNeutral)
  ON_BN_CLICKED(IDC_BLANK_IN_STEM, &CSTEMcontrolDlg::OnBlankInStem)
  ON_BN_CLICKED(IDC_RETRACT_TO_UNBLANK, &CSTEMcontrolDlg::OnRetractToUnblank)
  ON_BN_CLICKED(IDC_UNBLANK_STEM, &CSTEMcontrolDlg::OnUnblankStem)
END_MESSAGE_MAP()


// CSTEMcontrolDlg message handlers

BOOL CSTEMcontrolDlg::OnInitDialog() 
{
  CRect rect, rect2, winRect;
  CToolDlg::OnInitDialog();
  m_statOnOff.GetWindowRect(&rect);
  mFont.CreateFont((rect.Height() - 4), 0, 0, 0, FW_HEAVY,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  m_statOnOff.SetFont(&mFont);
  if (FEIscope) {
    m_butSetISneutral.ShowWindow(SW_HIDE);
    GetWindowRect(&winRect);
    m_butSetISneutral.GetWindowRect(&rect);
    m_editAddedRot.GetWindowRect(&rect2);
    SetWindowPos(NULL, 0, 0, 
      winRect.Width(), winRect.Height() - (rect.bottom - rect2.bottom), SWP_NOMOVE);
  }
  UpdateSettings();
  if (mWinApp->mCamera->GetInvertBrightField() <= 0)
    SetDlgItemText(IDC_CHECK_INVERT_CONTRAST, "Invert contrast unless Bright Field");
  m_butUnblank.SetWindowText(mLastBlanked ? "Unblank" : "Blank");
  mLastProbeMode = -1;
  UpdateSTEMstate(mWinApp->mScope->GetProbeMode());
  return TRUE;
}


void CSTEMcontrolDlg::OnStemMode()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus(); 
  mWinApp->SetSTEMMode(m_bStemMode);
}

void CSTEMcontrolDlg::OnScreenSwitchStem()
{
  if (mWinApp->LowDoseMode() && mWinApp->GetSTEMMode()) {
    UpdateData(false);
    UpdateEnables();
    return;
  }
  UpdateData(true);
  mWinApp->SetScreenSwitchSTEM(m_bScreenSwitches);
  mWinApp->RestoreViewFocus(); 
  mWinApp->mLowDoseDlg.Update();
}

void CSTEMcontrolDlg::OnMatchStemPixel()
{
  UpdateData(true);
  mWinApp->SetSTEMmatchPixel(m_bMatchPixel);
  mWinApp->RestoreViewFocus(); 
}

void CSTEMcontrolDlg::OnInvertContrast()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->SetInvertSTEMimages(m_bInvertContrast);
}

void CSTEMcontrolDlg::OnBlankInStem()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();  
  mWinApp->SetBlankBeamInSTEM(m_bBlankInStem);
  mWinApp->ManageSTEMBlanking();
}

void CSTEMcontrolDlg::OnRetractToUnblank()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();  
  mWinApp->SetRetractToUnblankSTEM(m_bRetractToUnblank);
}

void CSTEMcontrolDlg::OnUnblankStem()
{
  mWinApp->RestoreViewFocus();  
  mWinApp->mScope->BlankBeam(!mLastBlanked);
  BlankingUpdate(!mLastBlanked);
}

void CSTEMcontrolDlg::OnKillfocusAddedRot()
{
  double angleIn, goodAng;
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  angleIn = atof(m_strAddedRot);
  goodAng = 0.1 * B3DNINT(10. * angleIn);
  goodAng = UtilGoodAngle(goodAng);
  if (angleIn != goodAng) {
    m_strAddedRot.Format("%.1f", goodAng);
    UpdateData(false);
  }
  mWinApp->SetAddedSTEMrotation((float)goodAng);
}

void CSTEMcontrolDlg::OnSetIsNeutral()
{
  mWinApp->RestoreViewFocus();
  mWinApp->mScope->ResetSTEMneutral();
}

void CSTEMcontrolDlg::OnOK()
{
  SetFocus();
}

void CSTEMcontrolDlg::UpdateEnables(void)
{
  BOOL startedTS = mWinApp->StartedTiltSeries();
  if (!mWinApp->mCamera)
    return;
  BOOL enable = !mWinApp->mCamera->CameraBusy() && !mWinApp->DoingTasks();
  m_butScreenSwitches.EnableWindow(enable && !mWinApp->LowDoseMode());
  m_butMatchPixel.EnableWindow(enable);
  m_butUnblank.EnableWindow(enable && mLastBlanked);
  enable = enable && !startedTS;
  m_butStemMode.EnableWindow(enable);
  m_butInvertContrast.EnableWindow(enable);
  m_editAddedRot.EnableWindow(enable);
  m_butSetISneutral.EnableWindow(enable && mWinApp->mScope->FastSTEMmode());
  m_butBlankInStem.EnableWindow(enable);
  m_butRetractToUnblank.EnableWindow(enable && !mWinApp->GetMustUnblankWithScreen() &&
    !mWinApp->GetRetractOnSTEM());
}

void CSTEMcontrolDlg::UpdateSettings(void)
{
  m_bScreenSwitches = mWinApp->GetScreenSwitchSTEM();
  m_bMatchPixel = mWinApp->GetSTEMmatchPixel();
  m_bInvertContrast = mWinApp->GetInvertSTEMimages();
  m_strAddedRot.Format("%.1f", mWinApp->GetAddedSTEMrotation());
  m_bBlankInStem = mWinApp->GetBlankBeamInSTEM();
  m_bRetractToUnblank = mWinApp->GetRetractToUnblankSTEM() && 
    !mWinApp->GetMustUnblankWithScreen();
  UpdateData(false);
}

void CSTEMcontrolDlg::UpdateSTEMstate(int probeMode, int magInd)
{
  BOOL mode = mWinApp->GetSTEMMode();
  if (mLastProbeMode < -1)
    return;
  if (magInd < 0)
    magInd = mWinApp->mScope->GetMagIndex();
  if (probeMode == mLastProbeMode && mode == mLastSTEMmode && magInd == mLastMagIndex)
    return;
  if (!mode)
    m_strOnOff = "OFF";
  else if (JEOLscope)
    m_strOnOff = "ON";
  else if (mWinApp->mScope->MagIsInFeiLMSTEM(magInd))
    m_strOnOff = "LM";
  else if (probeMode)
    m_strOnOff = "MICRO";
  else
    m_strOnOff = "NANO";
  m_bStemMode = mode;
  mLastProbeMode = probeMode;
  mLastSTEMmode = mode;
  mLastMagIndex = magInd;
  UpdateEnables();
  UpdateData(false);
}

void CSTEMcontrolDlg::BlankingUpdate(BOOL blanked)
{
  if (BOOL_EQUIV(blanked, mLastBlanked))
    return;
  mLastBlanked = blanked;
  m_butUnblank.SetWindowText(blanked ? "Unblank" : "Blank");
  m_butUnblank.EnableWindow(!mWinApp->mCamera->CameraBusy() && !mWinApp->DoingTasks());
}
