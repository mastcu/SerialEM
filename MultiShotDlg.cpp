// MultiShotDlg.cpp:      Dialog for multiple Record parameters
//
// Copyright (C) 2017 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "AutoTuning.h"
#include "MacroProcessor.h"
#include "ParticleTasks.h"
#include "SerialEMView.h"
#include "ShiftCalibrator.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"

static int idTable[] = {IDC_CHECK_DO_SHOTS_IN_HOLE, PANEL_END,
IDC_SPIN_NUM_SHOTS, IDC_RNO_CENTER, IDC_RCENTER_AFTER, IDC_RCENTER_BEFORE, 
IDC_STAT_CENTER_GROUP, IDC_STAT_NUM_PERIPHERAL, IDC_STAT_NUM_SHOTS, IDC_STAT_CENTER_DIST,
IDC_EDIT_SPOKE_DIST, IDC_STAT_CENTER_UM, IDC_EDIT_RING2_DIST, IDC_CHECK_SECOND_RING,
IDC_STAT_NUM2_SHOTS, IDC_SPIN_RING2_NUM, IDC_STAT_RING2_SHOTS, IDC_STAT_RING2_UM, 
PANEL_END,
IDC_TSS_LINE1, IDC_CHECK_DO_MULTIPLE_HOLES, PANEL_END,
IDC_EDIT_HOLE_DELAY_FAC, IDC_STAT_REGULAR, IDC_STAT_NUM_X_HOLES, IDC_SPIN_NUM_X_HOLES, 
IDC_SPIN_NUM_Y_HOLES, IDC_STAT_SPACING, IDC_STAT_MEASURE_BOX, IDC_STAT_SAVE_INSTRUCTIONS,
IDC_BUT_SET_REGULAR, IDC_CHECK_USE_CUSTOM, IDC_BUT_SAVE_IS, IDC_CHECK_OMIT_3X3_CORNERS,
IDC_BUT_ABORT, IDC_BUT_END_PATTERN, IDC_BUT_IS_TO_PT,
IDC_BUT_SET_CUSTOM, IDC_STAT_HOLE_DELAY_FAC, IDC_STAT_NUM_Y_HOLES, PANEL_END,
IDC_TSS_LINE2, IDC_CHECK_SAVE_RECORD, PANEL_END,
IDC_RNO_EARLY, IDC_RLAST_EARLY, IDC_RALL_EARLY, IDC_RFIRST_FULL, IDC_STAT_NUM_EARLY,
IDC_STAT_EARLY_GROUP, IDC_EDIT_EARLY_FRAMES, PANEL_END,
IDC_EDIT_EXTRA_DELAY, IDC_STAT_BEAM_DIAM, IDC_EDIT_BEAM_DIAM,
IDC_STAT_BEAM_UM, IDC_CHECK_USE_ILLUM_AREA, IDC_STAT_EXTRA_DELAY, IDC_STAT_SEC,
IDC_CHECK_ADJUST_BEAM_TILT, IDC_STAT_COMA_IS_CAL,
IDC_STAT_COMA_CONDITIONS, IDCANCEL, IDOK, IDC_BUTHELP, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];

// CMultiShotDlg dialog


CMultiShotDlg::CMultiShotDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CMultiShotDlg::IDD, pParent)
  , m_strNumShots(_T(""))
  , m_fSpokeDist(0.1f)
  , m_iEarlyFrames(0)
  , m_bSaveRecord(FALSE)
  , m_bUseIllumArea(FALSE)
  , m_fBeamDiam(0.5f)
  , m_iEarlyReturn(0)
  , m_fExtraDelay(0)
  , m_bAdjustBeamTilt(FALSE)
  , m_bDoShotsInHoles(FALSE)
  , m_bDoMultipleHoles(FALSE)
  , m_strNumYholes(_T(""))
  , m_strNumXholes(_T(""))
  , m_bUseCustom(FALSE)
  , m_fHoleDelayFac(0.5f)
  , m_bOmit3x3Corners(FALSE)
  , m_bDoSecondRing(FALSE)
  , m_fRing2Dist(0.1f)
  , m_strNum2Shots(_T(""))
{
  mNonModal = true;
  mRecordingRegular = false;
  mRecordingCustom = false;
  mDisabledDialog = false;
  mWarnedIStoNav = false;
  mLastMagIndex = 0;
  mLastIntensity = 0.;
  mLastDrawTime = 0.;
  mSavedLDForCamera = -1;
  for (int ind = 0; ind < 7; ind++)
    mLastPanelStates[ind] = true;
}

CMultiShotDlg::~CMultiShotDlg()
{
}

void CMultiShotDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_NUM_SHOTS, m_strNumShots);
  DDX_Control(pDX, IDC_SPIN_NUM_SHOTS, m_sbcNumShots);
  DDX_Text(pDX, IDC_EDIT_SPOKE_DIST, m_fSpokeDist);
  MinMaxFloat(IDC_EDIT_SPOKE_DIST, m_fSpokeDist, 0.05f, 50., 
    "Main ring distance from center");
  DDX_Control(pDX, IDC_EDIT_EARLY_FRAMES, m_editEarlyFrames);
  DDX_Text(pDX, IDC_EDIT_EARLY_FRAMES, m_iEarlyFrames);
  MinMaxInt(IDC_EDIT_EARLY_FRAMES, m_iEarlyFrames, -1, 999, 
    "Early return number of frames");
  DDX_Check(pDX, IDC_CHECK_SAVE_RECORD, m_bSaveRecord);
  DDX_Control(pDX, IDC_CHECK_USE_ILLUM_AREA, m_butUseIllumArea);
  DDX_Check(pDX, IDC_CHECK_USE_ILLUM_AREA, m_bUseIllumArea);
  DDX_Control(pDX, IDC_STAT_BEAM_DIAM, m_statBeamDiam);
  DDX_Control(pDX, IDC_STAT_BEAM_UM, m_statBeamMicrons);
  DDX_Control(pDX, IDC_EDIT_BEAM_DIAM, m_editBeamDiam);
  DDX_Text(pDX, IDC_EDIT_BEAM_DIAM, m_fBeamDiam);
  MinMaxFloat(IDC_EDIT_BEAM_DIAM, m_fBeamDiam, 0.05f, 50., "Beam diameter");
  DDX_Radio(pDX, IDC_RNO_CENTER, m_iCenterShot);
  DDX_Control(pDX, IDC_STAT_NUM_EARLY, m_statNumEarly);
  DDX_Control(pDX, IDC_RNO_EARLY, m_butNoEarly);
  DDX_Radio(pDX, IDC_RNO_EARLY, m_iEarlyReturn);
  DDX_Text(pDX, IDC_EDIT_EXTRA_DELAY, m_fExtraDelay);
  MinMaxFloat(IDC_EDIT_EXTRA_DELAY, m_fExtraDelay, 0., 100., 
    "Extra delay after image shift");
  DDX_Control(pDX, IDC_CHECK_SAVE_RECORD, m_butSaveRecord);
  DDX_Control(pDX, IDC_CHECK_ADJUST_BEAM_TILT, m_butAdjustBeamTilt);
  DDX_Check(pDX, IDC_CHECK_ADJUST_BEAM_TILT, m_bAdjustBeamTilt);
  DDX_Control(pDX, IDC_STAT_COMA_IS_CAL, m_statComaIScal);
  DDX_Control(pDX, IDC_STAT_COMA_CONDITIONS, m_statComaConditions);
  DDX_Check(pDX, IDC_CHECK_DO_SHOTS_IN_HOLE, m_bDoShotsInHoles);
  DDX_Check(pDX, IDC_CHECK_DO_MULTIPLE_HOLES, m_bDoMultipleHoles);
  DDX_Control(pDX, IDC_STAT_NUM_X_HOLES, m_statNumXholes);
  DDX_Control(pDX, IDC_STAT_NUM_Y_HOLES, m_statNumYholes);
  DDX_Text(pDX, IDC_STAT_NUM_Y_HOLES, m_strNumYholes);
  DDX_Text(pDX, IDC_STAT_NUM_X_HOLES, m_strNumXholes);
  DDX_Control(pDX, IDC_SPIN_NUM_X_HOLES, m_sbcNumXholes);
  DDX_Control(pDX, IDC_SPIN_NUM_Y_HOLES, m_sbcNumYholes);
  DDX_Control(pDX, IDC_BUT_SET_REGULAR, m_butSetRegular);
  DDX_Control(pDX, IDC_BUT_SET_CUSTOM, m_butSetCustom);
  DDX_Control(pDX, IDC_CHECK_USE_CUSTOM, m_butUseCustom);
  DDX_Check(pDX, IDC_CHECK_USE_CUSTOM, m_bUseCustom);
  DDX_Control(pDX, IDC_STAT_REGULAR, m_statRegular);
  DDX_Control(pDX, IDC_STAT_SPACING, m_statSpacing);
  DDX_Control(pDX, IDC_RNO_CENTER, m_butNoCenter);
  DDX_Control(pDX, IDC_RCENTER_BEFORE, m_butCenterBefore);
  DDX_Control(pDX, IDC_RCENTER_AFTER, m_butCenterAfter);
  DDX_Control(pDX, IDC_EDIT_SPOKE_DIST, m_editSpokeDist);
  DDX_Control(pDX, IDC_STAT_NUM_PERIPHERAL, m_statNumPeripheral);
  DDX_Control(pDX, IDC_STAT_CENTER_GROUP, m_statCenterGroup);
  DDX_Control(pDX, IDC_STAT_NUM_SHOTS, m_statNumShots);
  DDX_Control(pDX, IDC_STAT_CENTER_DIST, m_statCenterDist);
  DDX_Control(pDX, IDC_STAT_CENTER_UM, m_statCenterUm);
  DDX_Control(pDX, IDC_STAT_HOLE_DELAY_FAC, m_statHoleDelayFac);
  DDX_Control(pDX, IDC_EDIT_HOLE_DELAY_FAC, m_editHoleDelayFac);
  DDX_Text(pDX, IDC_EDIT_HOLE_DELAY_FAC, m_fHoleDelayFac);
  MinMaxFloat(IDC_EDIT_HOLE_DELAY_FAC, m_fHoleDelayFac, 0.1f, 5.0f, 
    "Multiplier of delay after shifting to hole");
  DDX_Control(pDX, IDC_BUT_SAVE_IS, m_butSaveIS);
  DDX_Control(pDX, IDC_BUT_END_PATTERN, m_butEndPattern);
  DDX_Control(pDX, IDC_BUT_ABORT, m_butAbort);
  DDX_Control(pDX, IDC_STAT_SAVE_INSTRUCTIONS, m_statSaveInstructions);
  DDX_Control(pDX, IDC_EDIT_EXTRA_DELAY, m_editExtraDelay);
  DDX_Control(pDX, IDCANCEL, m_butCancel);
  DDX_Control(pDX, IDC_BUT_IS_TO_PT, m_butIStoPt);
  DDX_Control(pDX, IDC_CHECK_OMIT_3X3_CORNERS, m_butOmit3x3Corners);
  DDX_Check(pDX, IDC_CHECK_OMIT_3X3_CORNERS, m_bOmit3x3Corners);
  DDX_Check(pDX, IDC_CHECK_SECOND_RING, m_bDoSecondRing);
  DDX_Control(pDX, IDC_STAT_NUM2_SHOTS, m_statNum2Shots);
  DDX_Control(pDX, IDC_STAT_RING2_SHOTS, m_statRing2Shots);
  DDX_Control(pDX, IDC_STAT_RING2_UM, m_statRing2Um);
  DDX_Control(pDX, IDC_EDIT_RING2_DIST, m_editRing2Dist);
  DDX_Control(pDX, IDC_SPIN_RING2_NUM, m_sbcRing2Num);
  DDX_Text(pDX, IDC_EDIT_RING2_DIST, m_fRing2Dist);
  MinMaxFloat(IDC_EDIT_RING2_DIST, m_fRing2Dist, .05f, 10.f, "Distance to second ring");
  DDX_Text(pDX, IDC_STAT_NUM2_SHOTS, m_strNum2Shots);
  DDX_Control(pDX, IDC_CHECK_SECOND_RING, m_butDoSecondRing);
}


BEGIN_MESSAGE_MAP(CMultiShotDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_SHOTS, OnDeltaposSpinNumShots)
  ON_BN_CLICKED(IDC_RNO_CENTER, OnRnoCenter)
  ON_BN_CLICKED(IDC_RCENTER_BEFORE, OnRnoCenter)
  ON_BN_CLICKED(IDC_RCENTER_AFTER, OnRnoCenter)
  ON_EN_KILLFOCUS(IDC_EDIT_SPOKE_DIST, OnKillfocusEditSpokeDist)
  ON_EN_KILLFOCUS(IDC_EDIT_BEAM_DIAM, OnKillfocusEditBeamDiam)
  ON_BN_CLICKED(IDC_CHECK_USE_ILLUM_AREA, OnCheckUseIllumArea)
  ON_BN_CLICKED(IDC_RNO_EARLY, OnRnoEarly)
  ON_BN_CLICKED(IDC_RLAST_EARLY, OnRnoEarly)
  ON_BN_CLICKED(IDC_RALL_EARLY, OnRnoEarly)
  ON_BN_CLICKED(IDC_RFIRST_FULL, OnRnoEarly)
  ON_EN_KILLFOCUS(IDC_EDIT_EARLY_FRAMES, OnKillfocusEditEarlyFrames)
  ON_BN_CLICKED(IDC_CHECK_DO_MULTIPLE_HOLES, OnDoMultipleHoles)
  ON_BN_CLICKED(IDC_CHECK_USE_CUSTOM, OnUseCustom)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_X_HOLES, OnDeltaposSpinNumXHoles)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_Y_HOLES, OnDeltaposSpinNumYHoles)
  ON_BN_CLICKED(IDC_BUT_SET_REGULAR, OnSetRegular)
  ON_BN_CLICKED(IDC_BUT_SET_CUSTOM, OnSetCustom)
  ON_BN_CLICKED(IDC_CHECK_DO_SHOTS_IN_HOLE, OnDoShotsInHole)
  ON_BN_CLICKED(IDC_BUT_SAVE_IS, OnButSaveIs)
  ON_BN_CLICKED(IDC_BUT_END_PATTERN, OnButEndPattern)
  ON_BN_CLICKED(IDC_BUT_ABORT, OnButAbort)
  ON_BN_CLICKED(IDC_BUT_IS_TO_PT, OnButIsToPt)
  ON_BN_CLICKED(IDC_CHECK_SAVE_RECORD, OnSaveRecord)
  ON_BN_CLICKED(IDC_CHECK_ADJUST_BEAM_TILT, OnAdjustBeamTilt)
  ON_BN_CLICKED(IDC_CHECK_OMIT_3X3_CORNERS, OnOmit3x3Corners)
  ON_BN_CLICKED(IDC_CHECK_SECOND_RING, OnDoSecondRing)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RING2_NUM, OnDeltaposSpinRing2Num)
  ON_EN_KILLFOCUS(IDC_EDIT_RING2_DIST, OnKillfocusEditRing2Dist)
END_MESSAGE_MAP()


// CMultiShotDlg message handlers

// Initialize dialog
BOOL CMultiShotDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  if (!mHasIlluminatedArea)
    mIDsToHide[mNumIDsToHide++] = IDC_CHECK_USE_ILLUM_AREA;
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);
  UpdateSettings();
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// The dialog is being opened or the settings have been read: load dialog from parameters
void CMultiShotDlg::UpdateSettings(void)
{
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  CString str, str2;
  int minNum0 = 1;

  m_butAdjustBeamTilt.SetWindowText(comaVsIS->magInd > 0 && comaVsIS->astigMat.xpx != 0. 
    && !mWinApp->mNavHelper->GetSkipAstigAdjustment() ?
    "Adjust beam tilt && astig to compensate for image shift" :
    "Adjust beam tilt to compensate for image shift");

  // Save original parameters and transfer them to interface elements
  mActiveParams = mWinApp->mNavHelper->GetMultiShotParams();
  mSavedParams = *mActiveParams;
  m_iEarlyReturn = mActiveParams->doEarlyReturn;
  m_iEarlyFrames = mActiveParams->numEarlyFrames;
  m_iCenterShot = mActiveParams->doCenter < 0 ? 1 : (2 * mActiveParams->doCenter);
  m_fBeamDiam = mActiveParams->beamDiam;
  m_fSpokeDist = mActiveParams->spokeRad[0];
  m_fRing2Dist = mActiveParams->spokeRad[1];
  m_bDoSecondRing = mActiveParams->doSecondRing;
  m_bSaveRecord = mActiveParams->saveRecord;
  m_fExtraDelay = mActiveParams->extraDelay;
  m_fHoleDelayFac = mActiveParams->holeDelayFactor;
  m_bAdjustBeamTilt = mActiveParams->adjustBeamTilt;
  m_bUseIllumArea = mActiveParams->useIllumArea;
  m_bUseCustom = mActiveParams->useCustomHoles;
  m_bOmit3x3Corners = mActiveParams->skipCornersOf3x3;
  m_bDoShotsInHoles = (mActiveParams->inHoleOrMultiHole & MULTI_IN_HOLE) != 0;
  m_bDoMultipleHoles = (mActiveParams->inHoleOrMultiHole & MULTI_HOLES) != 0;
  if (mActiveParams->numHoles[1] <= 1) {
    minNum0 = 2;
    mActiveParams->numHoles[1] = 1;
  }
  mActiveParams->numHoles[0] = B3DMAX(mActiveParams->numHoles[0], minNum0);

  m_sbcNumShots.SetRange(0, 100);
  m_sbcNumShots.SetPos(50);
  m_sbcRing2Num.SetRange(0, 100);
  m_sbcRing2Num.SetPos(50);
  m_sbcNumXholes.SetRange(0, 100);
  m_sbcNumXholes.SetPos(50);
  m_sbcNumYholes.SetRange(0, 100);
  m_sbcNumYholes.SetPos(50);

  ManageEnables();

  m_strNumShots.Format("%d", mActiveParams->numShots[0]);
  m_strNum2Shots.Format("%d", mActiveParams->numShots[1]);
  m_strNumXholes.Format("%d", mActiveParams->numHoles[0]);
  m_strNumYholes.Format("by %2d", mActiveParams->numHoles[1]);
  UpdateData(false);
  ManagePanels();
  mWinApp->mMainView->DrawImage();
}

void CMultiShotDlg::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

// OK, Cancel, handling returns in text fields
void CMultiShotDlg::OnOK() 
{
  UpdateData(true);
  UpdateAndUseMSparams();
  mWinApp->mNavHelper->GetMultiShotPlacement(false);
  StopRecording();
  mWinApp->mNavHelper->mMultiShotDlg = NULL;
  if (!mWinApp->GetAppExiting())
    mWinApp->mMainView->DrawImage();
  DestroyWindow();
}

void CMultiShotDlg::OnCancel() 
{
  if (mDisabledDialog)
    return;
  *mActiveParams = mSavedParams;
  mWinApp->mMainView->DrawImage();
  mWinApp->mNavHelper->GetMultiShotPlacement(false);
  StopRecording();
  mWinApp->mNavHelper->mMultiShotDlg = NULL;
  if (!mWinApp->GetAppExiting())
    mWinApp->mMainView->DrawImage();
  DestroyWindow();
}

BOOL CMultiShotDlg::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Adjust the panels for changes in selections, and close up most of dialog to "disable"
void CMultiShotDlg::ManagePanels(void)
{
  BOOL states[7] = {true, true, true, true, true, true, true};
  int ind, numStates = sizeof(states) / sizeof(BOOL);
  states[1] = m_bDoShotsInHoles;
  states[3] = m_bDoMultipleHoles;
  states[5] = mCanReturnEarly;
  if ((mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()) || 
    mWinApp->mParticleTasks->DoingMultiShot() || mWinApp->mMacroProcessor->DoingMacro()) {
      for (ind = 0; ind < numStates - 1; ind++)
        states[ind] = false;
      if (!mDisabledDialog) {
        mDisabledDialog = true;
        ManageEnables();
      }
  } else if (mDisabledDialog) {
    mDisabledDialog = false;
    ManageEnables();
  }
  for (ind = 0; ind < numStates; ind++) {
    if (!BOOL_EQUIV(states[ind], mLastPanelStates[ind])) {
      AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 0);
      mWinApp->RestoreViewFocus();
      break;
    }
  }
  for (ind = 0; ind < numStates; ind++)
    mLastPanelStates[ind] = states[ind];
}

// Do multiple shots in hole check box
void CMultiShotDlg::OnDoShotsInHole()
{
  UpdateData(true);
  ManageEnables();
  ManagePanels();
  UpdateAndUseMSparams();
}

// Number of shots spinner
void CMultiShotDlg::OnDeltaposSpinNumShots(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 2, 18, mActiveParams->numShots[0], 
    m_strNumShots, "%d");
  UpdateAndUseMSparams();
}

// Center shot radio button
void CMultiShotDlg::OnRnoCenter()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Off-center distance
void CMultiShotDlg::OnKillfocusEditSpokeDist()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Checkbox to do second ring of shots
void CMultiShotDlg::OnDoSecondRing()
{
  UpdateData(true);
  ManageEnables();
  UpdateAndUseMSparams();
}

// Spinner for second ring
void CMultiShotDlg::OnDeltaposSpinRing2Num(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 2, 18, mActiveParams->numShots[1],
    m_strNum2Shots, "%d");
  UpdateAndUseMSparams();
}

// Distance to second ring
void CMultiShotDlg::OnKillfocusEditRing2Dist()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Do multiple holes check box
void CMultiShotDlg::OnDoMultipleHoles()
{
  UpdateData(true);
  ManageEnables();
  ManagePanels();
  UpdateAndUseMSparams();
}

// Use custom pattern check box
void CMultiShotDlg::OnUseCustom()
{
  UpdateData(true);
  ManageEnables();
  UpdateAndUseMSparams();
}

// Spinner for X holes
void CMultiShotDlg::OnDeltaposSpinNumXHoles(NMHDR *pNMHDR, LRESULT *pResult)
{
  int minNum = mActiveParams->numHoles[1] == 1 ? 2 : 1;
  FormattedSpinnerValue(pNMHDR, pResult, minNum, 15, mActiveParams->numHoles[0], 
    m_strNumXholes, "%d");
  ManageEnables();
  UpdateAndUseMSparams();
}

// Spinner for Y holes
void CMultiShotDlg::OnDeltaposSpinNumYHoles(NMHDR *pNMHDR, LRESULT *pResult)
{
  int minNum = mActiveParams->numHoles[0] == 1 ? 2 : 1;
  FormattedSpinnerValue(pNMHDR, pResult, minNum, 15, mActiveParams->numHoles[1], 
    m_strNumYholes, "by %2d");
  ManageEnables();
  UpdateAndUseMSparams();
}

// Button to set spacing/angle from: The hole number is n x 1 if there are two points,
// or m x n (> 1) if there are 4.
void CMultiShotDlg::OnSetRegular()
{
  mRecordingRegular = true;
  StartRecording("Shift to define the hole at first corner of pattern");
}

// Button to set custom points: there are at least two Nav points in group
void CMultiShotDlg::OnSetCustom()
{
  mRecordingCustom = true;
  StartRecording("Shift image to central point of pattern (not acquired)");
}

// Common operations to start recording corners/points
void CMultiShotDlg::StartRecording(const char *instruct)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  mSavedISX.clear();
  mSavedISY.clear();
  if (mWinApp->mNavigator)
    mWinApp->mNavigator->Update();
  mSavedMouseStage = mWinApp->mShiftManager->GetMouseMoveStage();
  mWinApp->mShiftManager->SetMouseMoveStage(false);
  m_statSaveInstructions.SetWindowText(instruct);
  mNavPointIncrement = -9;
  if (mWinApp->LowDoseMode()) {
    mSavedLDRecParams = ldp[RECORD_CONSET];
    mSavedLDForCamera = mWinApp->GetCurrentCamera();
    PrintfToLog("Low Dose Record parameters will be restored when you finish defining the"
      " pattern");
  }
  ManageEnables();
}

// Turn off flags and notify Nav when recording ends
void CMultiShotDlg::StopRecording(void)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  if (mRecordingCustom || mRecordingRegular) {
    mRecordingCustom = mRecordingRegular = false;
    if (mWinApp->mNavigator)
      mWinApp->mNavigator->Update();
    mWinApp->mShiftManager->SetMouseMoveStage(mSavedMouseStage);
    if (mSavedLDForCamera >= 0) {
      if (!mWinApp->LowDoseMode()) {
        PrintfToLog("Low Dose Record parameters were NOT restored because you left Low "
          "Dose mode");
      } else if (mWinApp->GetCurrentCamera() != mSavedLDForCamera) {
        PrintfToLog("Low Dose Record parameters were NOT restored because you changed "
          "cameras");
      } else {
        ldp[RECORD_CONSET] = mSavedLDRecParams;
        if (mWinApp->mScope->GetLowDoseArea() == RECORD_CONSET)
          mWinApp->mScope->GotoLowDoseArea(RECORD_CONSET);
        PrintfToLog("Low Dose Record parameters were restored to original values");
      }
      mSavedLDForCamera = -1;
    }
  }
}

// Do IS to XY in Nav and advance point
void CMultiShotDlg::OnButIsToPt()
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
  CString mess;  
  int answer, itemInd, numInGroup, groupStart, groupEnd;
  if (nav->m_bCollapseGroups) {
    AfxMessageBox("This function will not work right if groups are collapsed\n\n"
      "Turn off \"Collapse groups\" in the Navigator window and try again");
    return;
  }
  if (mNavPointIncrement < -1) {
    itemInd = nav->GetCurrentIndex();
    numInGroup = nav->LimitsOfContiguousGroup(itemInd, groupStart, groupEnd);
    if (numInGroup == 1) {
      AfxMessageBox("There is only one point in the current Navigator group\n\n"
        "You will be reponsible for selecting the next point\n"
        "BEFORE pressing this button again");
      mNavPointIncrement = 0;
    } else if (groupStart == itemInd || (mRecordingRegular && numInGroup == 4)) {
      mNavPointIncrement = 1;
      if (groupStart != itemInd) {
        mess = "The first point in the group was selected for moving to with IS\n";
        mWarnedIStoNav = false;
      }
      if (!mWarnedIStoNav) {
        mess += "After moving to the current point with IS, the next Navigator\n"
          "point will be selected each time you press this button";
        AfxMessageBox(mess);
        mWarnedIStoNav = true;
      }
      nav->SetCurrentSelection(groupStart);
    } else {
      answer = SEMThreeChoiceBox("The current Navigator point is not at the beginning\n"
        "of its group so it is not clear how to change the selection after each move\n\n"
        "Press:\n\"Forward from Start\" to go to start of group and advance forward "
        "each time\n\n\"Backward from End\" to go to end of group and go backwards each "
        "time\n\n\"Leave Selection\" to always use the current item and leave\n"
        "the selection alone (you will be responsible for selecting the next point)",
        "Forward from Start",  "Backward from End", "Leave Selection",
        MB_YESNOCANCEL | MB_ICONQUESTION);
      mNavPointIncrement = 0;
      if (answer == IDYES) {
        nav->SetCurrentSelection(groupStart);
        mNavPointIncrement = 1;
      } else if (answer == IDNO) {
        nav->SetCurrentSelection(groupEnd);
        mNavPointIncrement = -1;
      }
    }
  }
  nav->IStoXYandAdvance(mNavPointIncrement);
}

// Save one image shift, or finish up for regular pattern when there are 4
void CMultiShotDlg::OnButSaveIs()
{
  double ISX, ISY, stageX, stageY, stageZ;
  float ISlimit = 2.f * mWinApp->mShiftCalibrator->GetCalISOstageLimit();
  float focusLim = -20.;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  CString str;
  int dir, numSteps[2], size = (int)mSavedISX.size() + 1;
  int startInd1[2] = {0, 0}, endInd1[2] = {1, 3}, startInd2[2] = {3, 1}, 
    endInd2[2] = {2, 2};

  // First time, record stage position and check for defocus
  if (size == 1) {
    mWinApp->mScope->GetStagePosition(mRecordStageX, mRecordStageY, stageZ);
    if ((imBufs->mLowDoseArea && imBufs->mConSetUsed == VIEW_CONSET && 
      imBufs->mViewDefocus < focusLim) ||
      (mWinApp->LowDoseMode() && mWinApp->mScope->GetLowDoseArea() == VIEW_CONSET &&
      mWinApp->mScope->GetLDViewDefocus() < focusLim)) {
        str.Format("It appears that the scope is in the View Low Dose\n"
          "area with a View defocus offset bigger than %.0f microns.\n\n"
          "This procedure should be run closer to focus.\n"
          "Press Abort to end it, or continue if you know what you are doing.", focusLim);
        AfxMessageBox(str,  MB_EXCLAME);
    }

  } else {

    // After that, check for stage moved
    mWinApp->mScope->GetStagePosition(stageX, stageY, stageZ);
    if (fabs(stageX - mRecordStageX) > ISlimit || 
      fabs(stageY - mRecordStageY) > ISlimit) {
        str.Format("The stage appears to have moved by %.3f in X and %.3f in Y\n"
          "(more than ISoffsetCalStageLimit = %f).\n\n"
          "There must be no stage movement during this procedure.",
          stageX - mRecordStageX, stageY - mRecordStageY, ISlimit);
        AfxMessageBox(str,  MB_EXCLAME);
    }
  }

  // Get the IS and save it, compute spacing on 4th point for regular
  mWinApp->mScope->GetImageShift(ISX, ISY);
  mSavedISX.push_back(ISX);
  mSavedISY.push_back(ISY);
  mWinApp->RestoreViewFocus();
  if (mRecordingRegular) {
    for (dir = 0; dir < 2; dir++)
      numSteps[dir] = B3DMAX(1, mActiveParams->numHoles[dir] - 1);

    if (size == 4) {
      for (dir = 0; dir < 2; dir++) {
        mActiveParams->holeISXspacing[dir] = 
          0.5 * ((mSavedISX[endInd1[dir]] - mSavedISX[startInd1[dir]]) + 
          (mSavedISX[endInd2[dir]] - mSavedISX[startInd2[dir]])) / numSteps[dir];
        mActiveParams->holeISYspacing[dir] = 
          0.5 * ((mSavedISY[endInd1[dir]] - mSavedISY[startInd1[dir]]) + 
          (mSavedISY[endInd2[dir]] - mSavedISY[startInd2[dir]])) / numSteps[dir];
      }
      mActiveParams->holeMagIndex = mWinApp->mScope->GetMagIndex();
      mActiveParams->tiltOfHoleArray = (float)mWinApp->mScope->GetTiltAngle();
      StopRecording();
      ManageEnables();
      UpdateAndUseMSparams();
      return;
    } else {

      // Set up the instruction label
      if (size == 1 || size == 3)
        str.Format("Shift %sby %d holes in first direction", size == 1 ? "" : "back ",
                  numSteps[0]);
      else
        str.Format("Shift by %d holes in second direction", numSteps[1]);
    }
  } else {
    if (size > 1)
      str.Format("Shift to acquire position # %d in the pattern", size);
    else
      str = "Shift to the first acquire position in the pattern";
  }
  m_statSaveInstructions.SetWindowText(str);
}

// Separate button to end the custom pattern
void CMultiShotDlg::OnButEndPattern()
{
  int ind, size = (int)mSavedISX.size() - 1;
  double ISX, ISY;
  StopRecording();
  if (size < 0)
    return;
  mWinApp->mScope->GetImageShift(ISX, ISY);
  mActiveParams->customMagIndex = mWinApp->mScope->GetMagIndex();
  mActiveParams->tiltOfCustomHoles = (float)mWinApp->mScope->GetTiltAngle();

  // Accept the current position if it is different from the last
  if (mWinApp->mShiftManager->RadialShiftOnSpecimen(ISX - mSavedISX[size], 
    ISY - mSavedISY[size], mActiveParams->customMagIndex) > 0.05) {
    mSavedISX.push_back(ISX);
    mSavedISY.push_back(ISY);
    size++;
  }

  // Save the differences from the first point
  if (size > 0) {
    mActiveParams->customHoleX.resize(size);
    mActiveParams->customHoleY.resize(size);
    for (ind = 0; ind < size; ind++) {
      mActiveParams->customHoleX[ind] =  (float)(mSavedISX[ind + 1] -  mSavedISX[0]);
      mActiveParams->customHoleY[ind] =  (float)(mSavedISY[ind + 1] -  mSavedISY[0]);
    }
  }
  ManageEnables();
  UpdateAndUseMSparams();
}

// Abort means give up on any entries so far
void CMultiShotDlg::OnButAbort()
{
  StopRecording();
  ManageEnables();
}

// New beam diameter
void CMultiShotDlg::OnKillfocusEditBeamDiam()
{
  UpdateData(true);
  UpdateAndUseMSparams();
}

// Whether to use illuminated area
void CMultiShotDlg::OnCheckUseIllumArea()
{
  UpdateData(true);
  ManageEnables();
  UpdateAndUseMSparams();
}

// Early return radio button
void CMultiShotDlg::OnRnoEarly()
{
  UpdateData(true);
  ManageEnables();
}

// Early return frames: manage 
void CMultiShotDlg::OnKillfocusEditEarlyFrames()
{
  UpdateData(true);
  ManageEnables();
}

// Omit corners of 3 by 3 pattern
void CMultiShotDlg::OnOmit3x3Corners()
{
  UpdateData(true);
  ManageEnables();
  UpdateAndUseMSparams();
}

void CMultiShotDlg::OnSaveRecord()
{
  UpdateAndUseMSparams();
}

void CMultiShotDlg::OnAdjustBeamTilt()
{
  UpdateAndUseMSparams();
}

// Unload dialog parameters into the structure and redraw
void CMultiShotDlg::UpdateAndUseMSparams(bool draw)
{
  UpdateData(true);
  mActiveParams->doEarlyReturn = m_iEarlyReturn;
  mActiveParams->numEarlyFrames = m_iEarlyFrames;
  mActiveParams->doCenter = m_iCenterShot == 1 ? -1 : (m_iCenterShot / 2);
  mActiveParams->beamDiam = m_fBeamDiam;
  mActiveParams->spokeRad[0] = m_fSpokeDist;
  mActiveParams->doSecondRing = m_bDoSecondRing;
  mActiveParams->spokeRad[1] = m_fRing2Dist;
  mActiveParams->saveRecord = m_bSaveRecord;
  mActiveParams->extraDelay = m_fExtraDelay;
  mActiveParams->holeDelayFactor = m_fHoleDelayFac;
  mActiveParams->adjustBeamTilt = m_bAdjustBeamTilt;
  mActiveParams->useIllumArea = m_bUseIllumArea;
  mActiveParams->skipCornersOf3x3 = m_bOmit3x3Corners;
  mActiveParams->useCustomHoles = m_bUseCustom;
  mActiveParams->inHoleOrMultiHole = (m_bDoShotsInHoles ? 1 : 0) +
    (m_bDoMultipleHoles ? 2 : 0);
  if (draw)
    mWinApp->mMainView->DrawImage();
  mWinApp->RestoreViewFocus();
}

// Getting lazy: one function for multiple cases
// Some of this is superceded by panel closing
void CMultiShotDlg::ManageEnables(void)
{
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  CString str2, str = "Use custom pattern (NONE DEFINED)";
  bool enable = !(mHasIlluminatedArea && m_bUseIllumArea && mWinApp->LowDoseMode());
  bool recording = mRecordingRegular || mRecordingCustom;
  m_statBeamDiam.EnableWindow(enable);
  m_statBeamMicrons.EnableWindow(enable);
  m_editBeamDiam.EnableWindow(enable && !mDisabledDialog);
  m_editExtraDelay.EnableWindow(!mDisabledDialog);
  m_butCancel.EnableWindow(!mDisabledDialog);
  m_butUseIllumArea.EnableWindow(!mDisabledDialog);
  m_butAdjustBeamTilt.EnableWindow(comaVsIS->magInd > 0 && !mDisabledDialog);
  m_statNumEarly.EnableWindow(m_iEarlyReturn > 0);
  m_editEarlyFrames.EnableWindow(m_iEarlyReturn > 0);
  m_butSaveRecord.EnableWindow(m_iEarlyReturn == 0 || m_iEarlyFrames != 0);
  m_statCenterUm.EnableWindow(m_bDoShotsInHoles);
  m_statCenterDist.EnableWindow(m_bDoShotsInHoles);
  m_statCenterGroup.EnableWindow(m_bDoShotsInHoles);
  m_statNumPeripheral.EnableWindow(m_bDoShotsInHoles);
  m_sbcNumShots.EnableWindow(m_bDoShotsInHoles);
  m_butCenterAfter.EnableWindow(m_bDoShotsInHoles);
  m_butCenterBefore.EnableWindow(m_bDoShotsInHoles);
  m_butNoCenter.EnableWindow(m_bDoShotsInHoles);
  m_statNumShots.EnableWindow(m_bDoShotsInHoles);
  m_editSpokeDist.EnableWindow(m_bDoShotsInHoles);
  m_butDoSecondRing.EnableWindow(m_bDoShotsInHoles);
  enable = m_bDoShotsInHoles && m_bDoSecondRing;
  m_statNum2Shots.EnableWindow(enable);
  m_statRing2Shots.EnableWindow(enable);
  m_statRing2Um.EnableWindow(enable);
  m_editRing2Dist.EnableWindow(enable);
  m_sbcRing2Num.EnableWindow(enable);
  enable = m_bDoMultipleHoles && (!m_bUseCustom || !mActiveParams->customHoleX.size());
  m_statHoleDelayFac.EnableWindow(m_bDoMultipleHoles);
  m_editHoleDelayFac.EnableWindow(m_bDoMultipleHoles);
  m_statRegular.EnableWindow(enable);
  m_statSpacing.EnableWindow(enable);
  m_statNumXholes.EnableWindow(enable);
  m_statNumYholes.EnableWindow(enable);
  m_sbcNumXholes.EnableWindow(enable);
  m_sbcNumYholes.EnableWindow(enable);
  m_butOmit3x3Corners.EnableWindow(enable && mActiveParams->numHoles[0] == 3 && 
    mActiveParams->numHoles[1] == 3);
  m_butUseCustom.EnableWindow(m_bDoMultipleHoles && mActiveParams->customHoleX.size() >0);
  m_butSetRegular.EnableWindow(enable && !recording &&
    (mActiveParams->numHoles[0] > 1 || mActiveParams->numHoles[1] > 1));
  m_butSaveIS.EnableWindow(recording);
  m_butIStoPt.EnableWindow(recording && mWinApp->mNavigator);
  m_butAbort.EnableWindow(recording);
  m_butEndPattern.EnableWindow(mRecordingCustom);
  m_butSetCustom.EnableWindow(m_bDoMultipleHoles && !recording);
  if (mActiveParams->customHoleX.size() > 0)
    str.Format("Use custom pattern (%d positions defined)", 
    mActiveParams->customHoleX.size());
  m_butUseCustom.SetWindowText(str);
  if (!recording)
    m_statSaveInstructions.SetWindowText("Image shift should be measured near focus");
  str = "No regular pattern defined";

  // Report spacing and angle
  if (mActiveParams->holeMagIndex > 0) {
    int camera = mWinApp->GetCurrentCamera();
    ScaleMat s2c = mWinApp->mShiftManager->StageToCamera(camera, 
      mActiveParams->holeMagIndex);
    ScaleMat is2c = mWinApp->mShiftManager->IStoCamera(mActiveParams->holeMagIndex);
    if (s2c.xpx && is2c.xpx) {
      ScaleMat bMat = MatMul(is2c, MatInv(s2c));
      double delX = bMat.xpx * mActiveParams->holeISXspacing[0] + 
        bMat.xpy * mActiveParams->holeISYspacing[0];
      double delY = bMat.ypx * mActiveParams->holeISXspacing[0] + 
        bMat.ypy * mActiveParams->holeISYspacing[0];
      double dist2, dist = sqrt(delX * delX + delY * delY);
      double angle2, angle = atan2(delY, delX) / DTOR;
      delX = bMat.xpx * mActiveParams->holeISXspacing[1] + 
        bMat.xpy * mActiveParams->holeISYspacing[1];
      delY = bMat.ypx * mActiveParams->holeISXspacing[1] + 
        bMat.ypy * mActiveParams->holeISYspacing[1];
      dist2 = sqrt(delX * delX + delY * delY); 
      angle2 = atan2(delY, delX) / DTOR - 90.;
      angle2 = UtilGoodAngle(angle2 - angle) / 2.;
      angle = (float)(0.1 * B3DNINT(10. * (angle + angle2)));
      str.Format("Spacing: %.2f and %.2f um   Angle to stage X: %.1f deg", dist, dist2, 
        angle);
    }
  }
  m_statSpacing.SetWindowText(str);

  // Update the calibration conditions in case they changed
  if (comaVsIS->magInd <= 0) {
    SetDlgItemText(IDC_STAT_COMA_IS_CAL, "Coma versus image shift is not calibrated");
    SetDlgItemText(IDC_STAT_COMA_CONDITIONS, "");
  } else {
    str.Format("%.4g%s %s, spot %d", mWinApp->mScope->GetC2Percent(comaVsIS->spotSize, 
      comaVsIS->intensity, comaVsIS->probeMode), mWinApp->mScope->GetC2Units(), 
      mWinApp->mScope->GetC2Name(),
      comaVsIS->spotSize);
    if (!mWinApp->mScope->GetHasNoAlpha() && comaVsIS->alpha >= 0) {
      str2.Format(", alpha %d", comaVsIS->alpha + 1);
      str += str2;
    }
    if (comaVsIS->probeMode == 0)
      str += ", nanoprobe";
    if (comaVsIS->aperture > 0) {
      str2.Format(", %d um aperture", comaVsIS->aperture);
      str += str2;
    }
    SetDlgItemText(IDC_STAT_COMA_CONDITIONS, str);
  }

  mWinApp->RestoreViewFocus();
}

// Check for whether condistions have changed and display should be updated
void CMultiShotDlg::UpdateMultiDisplay(int magInd, double intensity)
{
  MontParam *montp;
  LowDoseParams *ldp;

  // This reproduces logic in NavigatorDlg::GetMapDrawItems for whether drawing and how to
  // get magInd
  bool showMulti = mWinApp->mNavigator && mActiveParams->inHoleOrMultiHole &&
    ((mWinApp->mNavigator->m_bShowAcquireArea && 
    (mWinApp->mNavHelper->GetEnableMultiShot() & 1)) || !RecordingISValues());
  if (!showMulti)
    return;

  // Get the intensity and mag that apply to current conditions
  if (mWinApp->LowDoseMode()) {
    ldp = mWinApp->GetLowDoseParams();
    magInd = ldp[RECORD_CONSET].magIndex;
    intensity = ldp[RECORD_CONSET].intensity;
  } else if (mWinApp->Montaging()) {
    montp = mWinApp->GetMontParam();
    magInd = montp->magIndex;
  }
  if (!magInd)
    mLastDrawTime = GetTickCount();

  // redraw if mag changed, or if IA changed but not too often for that
  bool draw = magInd > 0 && magInd != mLastMagIndex;
  if (mHasIlluminatedArea && m_bUseIllumArea && fabs(intensity - mLastIntensity) > 1.e-6){
    if (SEMTickInterval(mLastDrawTime) > 0.5)
      draw = true;
  }
  if (draw) {
    mLastDrawTime = GetTickCount();
    mLastIntensity = intensity;
    mWinApp->mMainView->DrawImage();
  }
  mLastMagIndex = magInd;
}
