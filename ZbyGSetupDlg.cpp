// ZbyGSetupDlg.cpp : Set parameters for calibrating and running a focus by eucentricity
//
// Copyright (C) 2021-2026 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "ZbyGSetupDlg.h"
#include "EMscope.h"
#include "CameraController.h"
#include "FocusManager.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif


// ZbyGSetupDlg dialog

CZbyGSetupDlg::CZbyGSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_Z_BY_G_SETUP, pParent)
  , m_strCurMag(_T(""))
  , m_strCalMag(_T(""))
  , m_strCurSpot(_T(""))
  , m_strCalSpot(_T(""))
  , m_strProbeLabel(_T(""))
  , m_strCurProbe(_T(""))
  , m_strCalProbe(_T(""))
  , m_strCurCam(_T(""))
  , m_strCurOffset(_T(""))
  , m_strCalOffset(_T(""))
  , m_bUseOffset(FALSE)
  , m_fFocusOffset(0)
  , m_fBeamTilt(0.5f)
  , m_bUseViewInLD(FALSE)
  , m_iMaxChange(100)
  , m_fIterThresh(0.5f)
  , m_strCurC2(_T(""))
  , m_strCalC2(_T(""))
  , m_strMradLabel(_T("% of range"))
  , m_bCalWithBT(FALSE)
  , m_strCurBeamTilt(_T(""))
  , m_strCalBeamTilt(_T(""))
  , m_iViewSubarea(0)
{
  mNonModal = true;
  mCalParams = NULL;
}

CZbyGSetupDlg::~CZbyGSetupDlg()
{
}

void CZbyGSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_CUR_MAG, m_strCurMag);
  DDX_Text(pDX, IDC_STAT_CAL_MAG, m_strCalMag);
  DDX_Text(pDX, IDC_STAT_CUR_SPOT, m_strCurSpot);
  DDX_Text(pDX, IDC_STAT_CAL_SPOT, m_strCalSpot);
  DDX_Text(pDX, IDC_STAT_PROBE_LABEL, m_strProbeLabel);
  DDX_Text(pDX, IDC_STAT_CUR_PROBE, m_strCurProbe);
  DDX_Text(pDX, IDC_STAT_CAL_PROBE, m_strCalProbe);
  DDX_Text(pDX, IDC_STAT_CUR_CAM, m_strCurCam);
  DDX_Text(pDX, IDC_STAT_CUR_FOCUS_OFFSET, m_strCurOffset);
  DDX_Text(pDX, IDC_STAT_CAL_FOCUS_OFFSET, m_strCalOffset);
  DDX_Check(pDX, IDC_CHECK_USE_OFFSET, m_bUseOffset);
  DDX_MM_FLOAT(pDX, IDC_EDIT_FOCUS_OFFSET, m_fFocusOffset, -500, 100,
    "Focus offset to use in calibration");
  DDX_Control(pDX, IDC_EDIT_FOCUS_OFFSET, m_editFocusOffset);
  DDX_MM_FLOAT(pDX, IDC_EDIT_BEAM_TILT, m_fBeamTilt, 0.09f, 25.f,
    "Beam tilt for measuring defocus");
  DDX_Control(pDX, IDC_BUT_USE_TO_CAL, m_butUseToCal);
  DDX_Control(pDX, IDC_BUT_USE_TO_RECAL, m_butUseToRecal);
  DDX_Check(pDX, IDC_CHECK_USE_VIEW_IN_LD, m_bUseViewInLD);
  DDX_MM_INT(pDX, IDC_EDIT_MAX_CHANGE, m_iMaxChange, 5, 500, "Maximum change in Z");
  DDX_MM_FLOAT(pDX, IDC_EDIT_ITER_THRESH, m_fIterThresh, 0.05f, 5.f,
    "Threshold Z change for doing another iteration");
  DDX_Text(pDX, IDC_STAT_CUR_C2, m_strCurC2);
  DDX_Text(pDX, IDC_STAT_CAL_C2, m_strCalC2);
  DDX_Control(pDX, IDC_ZBG_UPDATE_STATE, m_butUpdateState);
  DDX_Text(pDX, IDC_STAT_MRAD_LABEL, m_strMradLabel);
  DDX_Control(pDX, IDC_EDIT_BEAM_TILT, m_editBeamTilt);
  DDX_Check(pDX, IDC_CHECK_CAL_WITH_BT, m_bCalWithBT);
  DDX_Text(pDX, IDC_STAT_CUR_BEAM_TILT, m_strCurBeamTilt);
  DDX_Text(pDX, IDC_STAT_CAL_BEAM_TILT, m_strCalBeamTilt);
  DDX_Radio(pDX, IDC_RCURRENTAREA, m_iViewSubarea);
}


BEGIN_MESSAGE_MAP(CZbyGSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_ZBG_UPDATE_STATE, OnButUpdateState)
  ON_BN_CLICKED(IDC_CHECK_USE_OFFSET, OnCheckUseOffset)
  ON_BN_CLICKED(IDC_BUT_USE_TO_CAL, OnButUseCurToCal)
  ON_BN_CLICKED(IDC_BUT_USE_TO_RECAL, OnButUseStoredToRecal)
  ON_BN_CLICKED(IDC_CHECK_USE_VIEW_IN_LD, OnCheckUseViewInLd)
  ON_EN_KILLFOCUS(IDC_EDIT_FOCUS_OFFSET, OnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_CHANGE, OnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_ITER_THRESH, OnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_BEAM_TILT, OnKillfocusEditBox)
  ON_BN_CLICKED(IDC_CHECK_CAL_WITH_BT, OnCheckCalWithBt)
  ON_BN_CLICKED(IDC_RCURRENTAREA, OnRadioViewSubset)
END_MESSAGE_MAP()


// ZbyGSetupDlg message handlers
BOOL CZbyGSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mScope = mWinApp->mScope;
  mParticleTasks = mWinApp->mParticleTasks;
  mHasAlpha = JEOLscope && !mScope->GetHasNoAlpha();
  if (mHasAlpha)
    m_strProbeLabel = "Alpha";
  else if (!FEIscope)
    m_strProbeLabel = m_strCurProbe = m_strCalProbe = "";
  else
    m_strProbeLabel = "Probe mode";
  if (FEIscope)
    m_strMradLabel = "mrad";

  // Be sure to update into dialog calling routines that unload dialog
  UpdateData(false);
  GetCurrentState();
  UpdateCalStateForMag();
  UpdateSettings();
  UpdateEnables();
  m_editBeamTilt.EnableWindow(m_bCalWithBT);
  m_editFocusOffset.EnableWindow(m_bUseOffset);

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CZbyGSetupDlg::OnOK()
{
  UnloadControlParams();
  OnCancel();
}

// Inform managing module of closing
void CZbyGSetupDlg::OnCancel()
{
  mParticleTasks->ZbyGSetupClosing();
  DestroyWindow();
}

void CZbyGSetupDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CZbyGSetupDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// This needs to be called before running anything that needs these parameters
void CZbyGSetupDlg::UnloadControlParams()
{
  UPDATE_DATA_TRUE;
  mParticleTasks->SetZBGMaxTotalChange(m_iMaxChange);
  mParticleTasks->SetZBGIterThreshold(m_fIterThresh);
  mParticleTasks->SetZbyGUseViewInLD(m_bUseViewInLD);
  mParticleTasks->SetZbyGViewSubarea(m_iViewSubarea);
}

// Update scope and program state into dialog
void CZbyGSetupDlg::OnButUpdateState()
{
  UPDATE_DATA_TRUE;
  GetCurrentState();
  UpdateCalStateForMag();
  UpdateData(false);
  mWinApp->RestoreViewFocus();
}

// Manage enables for the check boxes
void CZbyGSetupDlg::OnCheckCalWithBt()
{
  UPDATE_DATA_TRUE;
  m_editBeamTilt.EnableWindow(m_bCalWithBT);
  mWinApp->RestoreViewFocus();
}

void CZbyGSetupDlg::OnCheckUseOffset()
{
  UPDATE_DATA_TRUE;
  m_editFocusOffset.EnableWindow(m_bUseOffset);
  mWinApp->RestoreViewFocus();
}

void CZbyGSetupDlg::OnCheckUseViewInLd()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  UpdateEnables();
}

void CZbyGSetupDlg::OnRadioViewSubset()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// All edit box values are just stored locally
void CZbyGSetupDlg::OnKillfocusEditBox()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Start calibration one way or another
void CZbyGSetupDlg::OnButUseCurToCal()
{
  StartCalibration(0);
}

void CZbyGSetupDlg::OnButUseStoredToRecal()
{
  StartCalibration(1);
}

void CZbyGSetupDlg::StartCalibration(int which)
{
  mWinApp->RestoreViewFocus();
  OnButUpdateState();
  if (!mRecalEnabled && which)
    return;

  if (mScope->GetTiltAngle() > 5.) {
    AfxMessageBox("This calibration cannot be run with the stage tilted above 5 degrees");
    return;
  }

  // Make a copy of parameters to pass and fill in beam tilt and offset as specified
  ZbyGParams copy = (which && mCalParams) ? *mCalParams : mCurParams;
  copy.beamTilt = m_fBeamTilt;
  if (m_bUseOffset)
    copy.focusOffset = m_fFocusOffset;
  if (m_bCalWithBT)
    copy.beamTilt = m_fBeamTilt;
  mParticleTasks->DoZbyGCalibration(copy, mCalParams ? mIndexOfCal : -1, 3);
}

// Wasted a lot of time trying to have one function do this for both columns, so here
// is a macro
#define STATE_TO_DIALOG(a, b) \
  m_str##a##Mag.Format("%d", MagForCamera(b##camera, b##magIndex)); \
  m_str##a##C2.Format("%.2f%s %s", mScope->GetC2Percent(b##spotSize,  \
  b##intensity,  \
  b##probeOrAlpha), mScope->GetC2Units(), mScope->GetC2Name());  \
  m_str##a##Spot.Format("%d", b##spotSize);   \
  if (FEIscope)   \
    m_str##a##Probe = b##probeOrAlpha ? "uP" : "nP";   \
  else if (mHasAlpha)   \
    m_str##a##Probe.Format("%d", b##probeOrAlpha + 1);   \
  m_str##a##Offset.Format("%.1f", b##focusOffset);   \
  m_str##a##BeamTilt.Format("%.1f %s", b##beamTilt, FEIscope ? "mrad" : "%");

// Get the current state into the param from the scope or low dose params
// And set the top line for camera and low dose area
void CZbyGSetupDlg::GetCurrentState()
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CameraParameters *camP = mWinApp->GetCamParams();
  int area = -1;
  mCurParams.focusOffset = mWinApp->mFocusManager->GetDefocusOffset();
  mCurParams.camera = mWinApp->GetCurrentCamera();
  mCurParams.lowDoseArea = -1;
  mCurParams.beamTilt = (float)mWinApp->mFocusManager->GetBeamTilt();
  m_strCurCam = "For camera " + camP[mCurParams.camera].name;
  if (mWinApp->LowDoseMode()) {
    area = FOCUS_CONSET;
    if (mScope->GetLowDoseArea() == VIEW_CONSET) {
      area = VIEW_CONSET;
      mCurParams.focusOffset += mScope->GetLDViewDefocus();
      m_strCurCam += ",   in Low Dose View:";
    } else
      m_strCurCam += ",   in Low Dose Focus:";
    mCurParams.magIndex = ldp[area].magIndex;
    mCurParams.intensity = ldp[area].intensity;
    mCurParams.spotSize = ldp[area].spotSize;
    mCurParams.probeOrAlpha = 0;
    if (FEIscope)
      mCurParams.probeOrAlpha = ldp[area].probeMode;
    else if (mHasAlpha)
      mCurParams.probeOrAlpha = (int)ldp[area].beamAlpha;
  } else {
    mCurParams.magIndex = mScope->GetMagIndex();
    mCurParams.intensity = mScope->GetIntensity();
    mCurParams.spotSize = mScope->GetSpotSize();
    mCurParams.probeOrAlpha = 0;
    if (FEIscope)
      mCurParams.probeOrAlpha = mScope->GetProbeMode();
    else if (mHasAlpha)
      mCurParams.probeOrAlpha = mScope->GetAlpha();
    m_strCurCam += ",   not in Low Dose:";
  }

  mCurParams.lowDoseArea = area;
  STATE_TO_DIALOG(Cur, mCurParams.);
  m_butUseToCal.EnableWindow(FocusCalExistsForParams(&mCurParams));
  UpdateData(false);
}

// See if there is a usable calibration at this mag and display it
void CZbyGSetupDlg::UpdateCalStateForMag()
{
  int nearest;
  UPDATE_DATA_TRUE;
  mCalParams = mWinApp->mParticleTasks->FindZbyGCalForMagAndArea(
    mCurParams.magIndex, mCurParams.lowDoseArea, mCurParams.camera, mIndexOfCal, nearest);
  if (mCalParams) {
    STATE_TO_DIALOG(Cal, mCalParams->);
  } else {
    if (nearest < 0)
      m_strCalMag = "No cals for cam and LD";
    else
      m_strCalMag.Format("No cal, nearest at %d", MagForCamera(mCurParams.camera,
        nearest));
    m_strCalC2 = "";
    m_strCalSpot = "";
    m_strCalProbe = "";
    m_strCalOffset = "";
    m_strCalBeamTilt = "";
  }

  // Record whether the calibrated conditions can actually be run and enable button
  mRecalEnabled = mCalParams != NULL && (mWinApp->LowDoseMode() || (mCalParams->spotSize
    == mCurParams.spotSize && mCalParams->probeOrAlpha == mCurParams.probeOrAlpha)) &&
    FocusCalExistsForParams(mCalParams);
  m_butUseToRecal.EnableWindow(mRecalEnabled && !mWinApp->DoingTasks());
  UpdateData(false);
}

// Callable for external change in settings
void CZbyGSetupDlg::UpdateSettings()
{
  m_iMaxChange = mParticleTasks->GetZBGMaxTotalChange();
  m_fIterThresh = mParticleTasks->GetZBGIterThreshold();
  m_bUseViewInLD = mParticleTasks->GetZbyGUseViewInLD();
  m_iViewSubarea = mParticleTasks->GetZbyGViewSubarea();
  UpdateData(false);
}

// General call for enabling
void CZbyGSetupDlg::UpdateEnables()
{
  CameraParameters *camP = mWinApp->GetActiveCamParam();
  bool enable = !((mWinApp->DoingTasks() && !mWinApp->GetJustNavAcquireOpen()) ||
    mWinApp->mCamera->CameraBusy());
  m_butUseToRecal.EnableWindow(mRecalEnabled && enable);
  m_butUseToCal.EnableWindow(enable && FocusCalExistsForParams(&mCurParams));
  m_butUpdateState.EnableWindow(enable);
  enable = m_bUseViewInLD && !camP->subareasBad;
  EnableDlgItem(IDC_STAT_USE_SUBAREA, enable);
  EnableDlgItem(IDC_RCURRENTAREA, enable);
  EnableDlgItem(IDC_RQUARTER, enable);
  EnableDlgItem(IDC_R3EIGHTHS, enable && camP->moduloX >= 0);
  EnableDlgItem(IDC_RHALFFIELD, enable);
}

bool CZbyGSetupDlg::FocusCalExistsForParams(ZbyGParams *params)
{
  FocusTable focTmp;
  return (mWinApp->mFocusManager->GetFocusCal(params->magIndex, params->camera,
    FEIscope ? params->probeOrAlpha : mScope->GetProbeMode(),
    mHasAlpha ? params->probeOrAlpha : -999, focTmp) != 0);
}
