// AutocenSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\AutocenSetupDlg.h"
#include "EMscope.h"
#include "CameraController.h"
#include "MultiTSTasks.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"

// CAutocenSetupDlg dialog

CAutocenSetupDlg::CAutocenSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CAutocenSetupDlg::IDD, pParent)
  , m_strCamName(_T(""))
  , m_bSetState(FALSE)
  , m_strSpot(_T(""))
  , m_strIntensity(_T(""))
  , m_strBinning(_T(""))
  , m_fExposure(0.1f)
  , m_iUseCentroid(0)
  , m_strMag(_T(""))
  , m_strSource(_T(""))
  , m_bShiftBeam(FALSE)
  , m_fBeamShift(1.f)
  , m_strAddedShift(_T(""))
  , m_bRecordingAddedShift(FALSE)
{
  mEnableAll = true;
  mLastTrialMismatch = false;
  mParam = NULL;
}

CAutocenSetupDlg::~CAutocenSetupDlg()
{
}

void CAutocenSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_STATCAMLABEL, m_statCamLabel1);
  DDX_Control(pDX, IDC_STATCAMLABEL2, m_statCamLabel2);
  DDX_Control(pDX, IDC_STATCAMNAME, m_statCamName);
  DDX_Text(pDX, IDC_STATCAMNAME, m_strCamName);
  DDX_Check(pDX, IDC_SETBEAMSTATE, m_bSetState);
  DDX_Text(pDX, IDC_AUTOCENSPOT, m_strSpot);
  DDX_Text(pDX, IDC_STATACBPCTC2, m_strIntensity);
  DDX_Text(pDX, IDC_STATACBBINNING, m_strBinning);
  DDX_Control(pDX, IDC_SPINACBSPOT, m_sbcSpot);
  DDX_Control(pDX, IDC_SPINACBBINNING, m_sbcBinning);
  DDX_Text(pDX, IDC_EDITACBEXPOSURE, m_fExposure);
  DDV_MinMaxFloat(pDX, m_fExposure, 0.001f, 10.f);
  DDX_Control(pDX, ID_TESTACQUIRE, m_butAcquire);
  DDX_Radio(pDX, IDC_RADIOUSEEDGE, m_iUseCentroid);
  DDX_Text(pDX, IDC_STATACBMAG, m_strMag);
  DDX_Control(pDX, IDC_SPINACBMAG, m_sbcMag);
  DDX_Control(pDX, ID_DELETESETTINGS, m_butDelete);
  DDX_Text(pDX, IDC_STATSOURCE, m_strSource);
  DDX_Control(pDX, IDC_BUT_AC_MORE, m_butMore);
  DDX_Control(pDX, IDC_BUT_AC_LESS, m_butLess);
  DDX_Control(pDX, IDC_STAT_NANOPROBE, m_statNanoprobe);
  DDX_Control(pDX, IDC_SETBEAMSTATE, m_butSetState);
  DDX_Control(pDX, IDC_EDITACBEXPOSURE, m_editExposure);
  DDX_Control(pDX, IDC_RADIOUSEEDGE, m_butUseEdge);
  DDX_Control(pDX, IDC_RADIOUSECENTROID, m_butUseCentroid);
  DDX_Control(pDX, IDC_ACS_SHIFT_BEAM, m_butShiftBeam);
  DDX_Control(pDX, IDC_STAT_ACS_MICRONS, m_statMicrons);
  DDX_Control(pDX, IDC_EDIT_ACS_BEAM_SHIFT, m_editBeamShift);
  DDX_Check(pDX, IDC_ACS_SHIFT_BEAM, m_bShiftBeam);
  DDX_Text(pDX, IDC_EDIT_ACS_BEAM_SHIFT, m_fBeamShift);
  DDV_MinMaxFloat(pDX, m_fBeamShift, -10.f, 10.f);
  DDX_Text(pDX, IDC_STAT_ACS_ADDED_SHIFT, m_strAddedShift);
  DDX_Check(pDX, IDC_RECORD_ADDED_SHIFT, m_bRecordingAddedShift);
  DDX_Control(pDX, IDC_RESET_ADDED_SHIFT, m_butResetAddedShift);
  DDX_Control(pDX, IDC_RECORD_ADDED_SHIFT, m_butRecordShift);
  DDX_Control(pDX, IDC_STAT_LD_TRACK_SCOPE, m_statLdTrackScope);
}


BEGIN_MESSAGE_MAP(CAutocenSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_SETBEAMSTATE, OnSetbeamstate)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINACBSPOT, OnDeltaposSpinspot)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINACBBINNING, OnDeltaposSpinbinning)
  ON_BN_CLICKED(IDC_RADIOUSEEDGE, OnRadiouseedge)
  ON_BN_CLICKED(IDC_RADIOUSECENTROID, OnRadiouseedge)
  ON_BN_CLICKED(ID_TESTACQUIRE, OnTestacquire)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINACBMAG, OnDeltaposSpinmag)
  ON_BN_CLICKED(ID_DELETESETTINGS, OnDeleteSettings)
  ON_BN_CLICKED(IDC_BUT_AC_MORE, OnButAcMore)
  ON_BN_CLICKED(IDC_BUT_AC_LESS, OnButAcLess)
  ON_EN_KILLFOCUS(IDC_EDITACBEXPOSURE, OnKillfocusEditExposure)
  ON_BN_CLICKED(IDC_ACS_SHIFT_BEAM, OnShiftBeam)
  ON_EN_KILLFOCUS(IDC_EDIT_ACS_BEAM_SHIFT, OnEnKillfocusEditBeamShift)
  ON_BN_CLICKED(IDC_RECORD_ADDED_SHIFT, OnRecordAddedShift)
  ON_BN_CLICKED(IDC_RESET_ADDED_SHIFT, OnResetAddedShift)
END_MESSAGE_MAP()


// CAutocenSetupDlg message handlers

BOOL CAutocenSetupDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
  mScope = mWinApp->mScope;
  mMultiTasks = mWinApp->mMultiTSTasks;
  mCamera = mWinApp->GetCurrentCamera();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams() + mCamera;
  mLowDoseMode = mWinApp->LowDoseMode();
  if (mWinApp->GetNumActiveCameras() > 1) {
    m_strCamName = mCamParams->name;
  } else {
    m_statCamLabel1.ShowWindow(SW_HIDE);
    m_statCamLabel2.ShowWindow(SW_HIDE);
    m_statCamName.ShowWindow(SW_HIDE);
  }

  // In low dose, start with the trial parameters for fetching the parameters to use
  if (mLowDoseMode) {
    mCurMagInd = ldp->magIndex;
    mCurSpot = ldp->spotSize;
    mCurProbe = ldp->probeMode;
    mCurIntensity = ldp->intensity;
    m_butSetState.ShowWindow(SW_HIDE);
  } else {

    // Otherwise, take current state of scope and get settings for it
    mCurMagInd = mScope->GetMagIndex();
    mCurSpot = mScope->GetSpotSize();
    mCurProbe = mScope->ReadProbeMode();
    m_statLdTrackScope.ShowWindow(SW_HIDE);

    // This is held at the starting value when not tracking state, is sync'ed to the param
    // value but is needed for fetching param when tracking state, and reverts to the
    // value at the start of state tracking when leaving tracking
    mCurIntensity = mScope->GetIntensity();
  }
  FetchParams();

  m_butAcquire.EnableWindow(mLowDoseMode);

  m_sbcSpot.SetRange(0,30000);
  m_sbcSpot.SetPos(15000);
  m_sbcMag.SetRange(0,30000);
  m_sbcMag.SetPos(15000);
  m_sbcBinning.SetRange(0,30000);
  m_sbcBinning.SetPos(15000);
  UpdateParamSettings();
  UpdateMagSpot();
  UpdateEnables();
  SetDefID(45678);    // Disable OK from being default button

  return TRUE;
}

void CAutocenSetupDlg::OnOK() 
{
  UpdateIfExposureChanged();
  mMultiTasks->AutocenClosing();
}

void CAutocenSetupDlg::OnCancel() 
{
  if (!mEnableAll)
    return;
  mMultiTasks->AutocenClosing();
}

void CAutocenSetupDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CAutocenSetupDlg::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Button to set and track scope state
void CAutocenSetupDlg::OnSetbeamstate()
{
  UpdateData(true);
  if (m_bSetState)
    StartTrackingState();
  else
    RestoreScopeState();
  m_butAcquire.EnableWindow(m_bSetState);
  m_butMore.EnableWindow(m_bSetState);
  m_butLess.EnableWindow(m_bSetState);
  mWinApp->RestoreViewFocus();
}

// Spot size change
void CAutocenSetupDlg::OnDeltaposSpinspot(NMHDR *pNMHDR, LRESULT *pResult)
{
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, mCurSpot, mScope->GetMinSpotSize(),
    mScope->GetNumSpotSizes(), mCurSpot))
    return;
  if (mMultiTasks->AutocenTrackingState())
    mScope->SetSpotSize(mCurSpot);
  MagOrSpotChanged(-1, mCurSpot, -1);
}

// Mag change
void CAutocenSetupDlg::OnDeltaposSpinmag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newmag = mCurMagInd + pNMUpDown->iDelta;
  mWinApp->RestoreViewFocus();
  if (newmag < 1 || newmag >= MAX_MAGS || !mMagTab[newmag].mag) {
    *pResult = 1;
    return;
  }
  if (mMultiTasks->AutocenTrackingState())
    mScope->SetMagIndex(newmag);
  MagOrSpotChanged(newmag, -1, -1);
  *pResult = 0;
}

// Binning change
void CAutocenSetupDlg::OnDeltaposSpinbinning(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newind = mBinIndex + pNMUpDown->iDelta;
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, mBinIndex, (CamHasDoubledBinnings(mCamParams) ?
    1 : 0), mCamParams->numBinnings, mBinIndex))
      return;
  if (UpdateIfExposureChanged())
    FetchParams();
  mBinIndex = newind;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mCamParams->binnings[mBinIndex],
    mCamParams));
  mParam->binning = mCamParams->binnings[mBinIndex];
  ParamChanged();
}

// Use edge or centroid
void CAutocenSetupDlg::OnRadiouseedge()
{
  if (UpdateIfExposureChanged())
    FetchParams();
  mParam->useCentroid = m_iUseCentroid;
  ParamChanged();
  ManageShiftBeam();
  mWinApp->RestoreViewFocus();
}

// Shift beam for centering
void CAutocenSetupDlg::OnShiftBeam()
{
  if (UpdateIfExposureChanged())
    FetchParams();
  mParam->shiftBeamForCen = m_bShiftBeam;
  mParam->beamShiftUm = m_fBeamShift;
  ParamChanged();
  ManageShiftBeam();
  mWinApp->RestoreViewFocus();
}

// New value for beam shift to apply
void CAutocenSetupDlg::OnEnKillfocusEditBeamShift()
{
  if (UpdateIfExposureChanged())
    FetchParams();
  mParam->beamShiftUm = m_fBeamShift;
  ParamChanged();
  mWinApp->RestoreViewFocus();
}

// Manage those enables
void CAutocenSetupDlg::ManageShiftBeam()
{
  m_butShiftBeam.EnableWindow(!m_iUseCentroid);
  m_statMicrons.EnableWindow(!m_iUseCentroid);
  m_editBeamShift.EnableWindow(!m_iUseCentroid && m_bShiftBeam);
}

// New exposure: constrain and round it
void CAutocenSetupDlg::OnKillfocusEditExposure()
{
  ControlSet *conSet = mWinApp->GetConSets() + TRIAL_CONSET;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  float roundFac = mWinApp->mCamera->ExposureRoundingFactor(camParam);
  UpdateData(true);
  if (mWinApp->mCamera->ConstrainExposureTime(camParam, false, conSet->K2ReadMode,
    conSet->binning, false, 2, m_fExposure, conSet->frameTime)) {
    if (roundFac)
      m_fExposure = (float)(B3DNINT(m_fExposure * roundFac) / roundFac);
    UpdateData(false);
  }

  mWinApp->RestoreViewFocus();
}

// Bigger/smaller buttons
void CAutocenSetupDlg::OnButAcMore()
{
  MoreOrLessIntensity(1.25);
}

void CAutocenSetupDlg::OnButAcLess()
{
  MoreOrLessIntensity(0.8);
}

void CAutocenSetupDlg::MoreOrLessIntensity(double factor)
{
  double newIntensity, outFactor;
  int err;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
  mWinApp->RestoreViewFocus();
  if (UpdateIfExposureChanged())
    FetchParams();

  // In low dose, we may just be changing the value here and not on scope so get the
  // new intensity, apply only if appropriate
  err = mWinApp->mBeamAssessor->AssessBeamChange(mParam->intensity, 
    mLowDoseMode ? ldp->spotSize : mParam->spotSize, 
    mLowDoseMode ? ldp->probeMode : mParam->probeMode, factor, newIntensity, outFactor);
  if (err && err != BEAM_ENDING_OUT_OF_RANGE)
    return;
  if ((!mLowDoseMode && mMultiTasks->AutocenTrackingState()) ||
    (mLowDoseMode && mScope->GetLowDoseArea() == TRIAL_CONSET && 
      !mWinApp->mLowDoseDlg.m_bContinuousUpdate && mScope->FastScreenPos() == spDown))
    mScope->SetIntensity(newIntensity);
  mParam->intensity = newIntensity;
  mCurIntensity = newIntensity;
  ParamChanged();
}

// Take a test shot
void CAutocenSetupDlg::OnTestacquire()
{
  mMultiTasks->TestAutocenAcquire();
  mWinApp->RestoreViewFocus();
}

// Remove the current settings and replace
void CAutocenSetupDlg::OnDeleteSettings()
{
  mMultiTasks->DeleteAutocenParams(mCamera, mCurMagInd, mCurSpot, mCurProbe,
    mCurIntensity);
  FetchParams();
  if (mMultiTasks->AutocenTrackingState()) {
    mParam->intensity = mScope->GetIntensity();
    mCurIntensity = mParam->intensity;
    ParamChanged();
  } else
    UpdateParamSettings();
  mWinApp->RestoreViewFocus();
}

// Record an added shift after centering
void CAutocenSetupDlg::OnRecordAddedShift()
{
  double delBSX, delBSY;
  ScaleMat IStoBS, camToIS, BStoCam;
  float pixel;
  int magInd;
  UpdateData(true);
  if (!m_bRecordingAddedShift) {

    // Finished getting shift: convert to microns on the camera
    FetchParams();
    magInd = mScope->GetMagIndex();
    IStoBS = mWinApp->mShiftManager->GetBeamShiftCal(magInd);
    camToIS = mWinApp->mShiftManager->CameraToIS(magInd);
    mScope->GetBeamShift(delBSX, delBSY);
    delBSX -= mStartingBSX;
    delBSY -= mStartingBSY;
    BStoCam = MatInv(MatMul(camToIS, IStoBS));
    mWinApp->mShiftManager->ApplyScaleMatrix(BStoCam, (float)delBSX, (float)delBSY,
      mParam->addedShiftX, mParam->addedShiftY);
    pixel = mWinApp->mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(),
      magInd);
    mParam->addedShiftX *= pixel;
    mParam->addedShiftY *= pixel;
    ParamChanged();
    SetDlgItemText(IDC_RESET_ADDED_SHIFT, "Reset");
  } else {

    // Starting to rtecord: give a caution
    if (!FEIscope)
      mWinApp->AppendToLog("Stay at the same scope conditions while recording this"
        " beam shift");
    mScope->GetBeamShift(mStartingBSX, mStartingBSY);
    SetDlgItemText(IDC_RESET_ADDED_SHIFT, "Abort");
  }
  UpdateParamSettings();
  mWinApp->RestoreViewFocus();
}

// The reset button to clear out a shift or abort recording it
void CAutocenSetupDlg::OnResetAddedShift()
{
  if (m_bRecordingAddedShift) {
    m_bRecordingAddedShift = false;
    SetDlgItemText(IDC_RESET_ADDED_SHIFT, "Reset");
    UpdateData(false);
  } else {
    FetchParams();
    mParam->addedShiftX = mParam->addedShiftY = 0.;
    ParamChanged();
  }
  m_butResetAddedShift.EnableWindow(m_bRecordingAddedShift || (mParam &&
    (mParam->addedShiftX || mParam->addedShiftY)));
  mWinApp->RestoreViewFocus();
}

// Called from scope update with current values when they have changed
void CAutocenSetupDlg::LiveUpdate(int magInd, int spotSize, int probe, double intensity)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
  bool mismatch = false;
  if (!mMultiTasks->AutocenTrackingState())
    return;
  if (mLowDoseMode) {
    mismatch = magInd != ldp->magIndex || spotSize != ldp->spotSize || 
      probe != ldp->probeMode;
    magInd = ldp->magIndex;
    spotSize = ldp->spotSize;
    probe = ldp->probeMode;
    if (!BOOL_EQUIV(mismatch, mLastTrialMismatch))
      ManageLDtrackText(mMultiTasks->AutocenMatchingIntensity());
    mLastTrialMismatch = mismatch;
  }
  if (magInd != mCurMagInd || spotSize != mCurSpot || probe != mCurProbe) {
    MagOrSpotChanged(magInd, spotSize, probe);
  } else if (intensity != mParam->intensity && mMultiTasks->AutocenMatchingIntensity() &&
    !mismatch) {

    // If we the beam has passed through crossover, delete any existing param
    // on the new side of crossover first, so there is only one there
    if (mWinApp->mBeamAssessor->GetAboveCrossover(mCurSpot, mCurIntensity, probe) !=
      mWinApp->mBeamAssessor->GetAboveCrossover(mCurSpot, intensity, probe))
      mMultiTasks->DeleteAutocenParams(mCamera, mCurMagInd, mCurSpot, probe, intensity);
    mParam->intensity = intensity;
    mCurIntensity = intensity;
    ParamChanged();
  }
}

void CAutocenSetupDlg::ParamChanged(void)
{
  // If parameter set changed and it was synthesized, save it for real
  if (mSynthesized) {
    mMultiTasks->AddAutocenParams(mParam);
    FetchParams();
  }
  UpdateParamSettings();
}

// Update the display for current parameters
void CAutocenSetupDlg::UpdateParamSettings(void)
{
  CString str1, str2;
  CString prefix = mLowDoseMode ? "Intensity was" : "Settings were";
  m_fExposure = (float)mParam->exposure;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mParam->binning, mCamParams));
  m_iUseCentroid = mParam->useCentroid;
  if (mLowDoseMode && mParam->intensity < 0) {
    if (mScope->GetLowDoseArea() == TRIAL_CONSET)
      mCurIntensity = mScope->GetIntensity();
    mParam->intensity = mCurIntensity;
    ParamChanged();
  }

  if (mParam->intensity >= 0.) {
    m_strIntensity.Format("%.2f%s %s", mScope->GetC2Percent(mCurSpot, mParam->intensity,
      mParam->probeMode), mScope->GetC2Units(), mScope->GetC2Name());
    if (mSynthesized && mBestMag > 0) {
      str1 = "this mag";
      str2 = "this spot";
      if (mBestMag != mCurMagInd)
        str1.Format("%dx", MagOrEFTEMmag(mCamParams->GIF, mBestMag));
      if (mBestSpot != mCurSpot)
        str2.Format("spot %d", mBestSpot);
      m_strSource = prefix + " derived from " + str1 + " and " + str2;
    } else
      m_strSource = prefix + " directly set at this mag && spot";

  } else {
    m_strIntensity = "--x--";
    m_strSource = "No usable settings available for this mag && spot";
  }
  m_butDelete.EnableWindow(!mSynthesized);
  m_fBeamShift = mParam->beamShiftUm;
  m_bShiftBeam = mParam->shiftBeamForCen;
  m_strAddedShift.Format("%.2f, %.2f", mParam->addedShiftX, mParam->addedShiftY);
  m_butResetAddedShift.EnableWindow(m_bRecordingAddedShift || (mParam &&
    (mParam->addedShiftX || mParam->addedShiftY)));
  UpdateData(false);
}

// Turn on tracking of external state outside low dose
void CAutocenSetupDlg::StartTrackingState(void)
{
  DWORD postMag;
  mSavedMagInd = mScope->GetMagIndex();
  mSavedSpot = mScope->GetSpotSize();
  mSavedProbe = mScope->ReadProbeMode();
  mSavedIntensity = mScope->GetIntensity();
  mScope->SetMagIndex(mCurMagInd);
  postMag = GetTickCount();
  mScope->SetProbeMode(mCurProbe);
  mScope->SetSpotSize(mCurSpot);
  if (mParam->intensity >= 0) {
    mScope->DelayedSetIntensity(mParam->intensity, postMag);
    mCurIntensity = mParam->intensity;
  } else {
    mParam->intensity = mScope->GetIntensity();
    mCurIntensity = mParam->intensity;
    ParamChanged();
  }
}

// Update the strings for mag and spot
void CAutocenSetupDlg::UpdateMagSpot(void)
{
  m_strSpot.Format("%d", mCurSpot);
  m_strMag.Format("%d", MagOrEFTEMmag(mCamParams->GIF, mCurMagInd));
  m_statNanoprobe.ShowWindow(mCurProbe > 0 ? SW_HIDE : SW_SHOW);
  UpdateData(false);
}

// Make the necessary changes when mag or spot changes
void CAutocenSetupDlg::MagOrSpotChanged(int magInd, int spot, int probe)
{
  UpdateIfExposureChanged();
  if (magInd > 0)
    mCurMagInd = magInd;
  if (spot >= 0)
    mCurSpot = spot;
  if (probe >= 0)
    mCurProbe = probe;
  FetchParams();
  if (mMultiTasks->AutocenTrackingState()) {
    if (mMultiTasks->AutocenMatchingIntensity()) {
      if (mParam->intensity >= 0.)
        mScope->DelayedSetIntensity(mParam->intensity, GetTickCount());
      else
        mParam->intensity = mScope->GetIntensity();
      mCurIntensity = mParam->intensity;
    }
    if (mSynthesized && mBestMag < 0) {
      mMultiTasks->AddAutocenParams(mParam);
      mSynthesized = false;
    }
  }
  UpdateMagSpot();
  UpdateParamSettings();
}

// Get params for current mag/spot and find the binning index
void CAutocenSetupDlg::FetchParams(void)
{
  mParam = mMultiTasks->GetAutocenSettings(mCamera, mCurMagInd, mCurSpot, mCurProbe,
    mCurIntensity, mSynthesized, mBestMag, mBestSpot);
  for (int i = 0; i < mCamParams->numBinnings; i++) {
    if (mCamParams->binnings[i] >= mParam->binning) {
      mBinIndex = i;
      break;
    }
  }
}

// Restore state when tracking was started
void CAutocenSetupDlg::RestoreScopeState(void)
{
  mScope->SetProbeMode(mSavedProbe);
  mScope->SetSpotSize(mSavedSpot);
  mScope->SetMagIndex(mSavedMagInd);
  mScope->DelayedSetIntensity(mSavedIntensity, GetTickCount());
  mCurIntensity = mSavedIntensity;
}

// Make sure exposure is synchronized with text box, add param if necessary if it changes
// Be sure to FetchParams if this returns true
BOOL CAutocenSetupDlg::UpdateIfExposureChanged(void)
{
  UpdateData(true);
  if (fabs(m_fExposure - mParam->exposure) > 0.0001 * m_fExposure) {
    mParam->exposure = m_fExposure;
    if (mSynthesized) {
      mMultiTasks->AddAutocenParams(mParam);
      return true;
    }
  }
  return false;
}

// Enable/disable buttons based on external conditions
void CAutocenSetupDlg::UpdateEnables()
{
  CButton *button;
  mEnableAll = !mWinApp->DoingTasks() && !(mWinApp->mCamera->GetInitialized() && 
    mWinApp->mCamera->CameraBusy() && !mWinApp->mCamera->DoingContinuousAcquire());
  bool canAcquire = mEnableAll && (m_bSetState || mLowDoseMode);
  button = (CButton *)GetDlgItem(IDOK);
  button->EnableWindow(mEnableAll);
  button = (CButton *)GetDlgItem(IDCANCEL);
  button->EnableWindow(mEnableAll);
  m_butSetState.EnableWindow(mEnableAll && !mLowDoseMode);
  m_editExposure.EnableWindow(mEnableAll);
  m_butUseEdge.EnableWindow(mEnableAll);
  m_butUseCentroid.EnableWindow(mEnableAll);
  m_butMore.EnableWindow(canAcquire);
  m_butLess.EnableWindow(canAcquire);
  m_butDelete.EnableWindow(mEnableAll);
  m_sbcMag.EnableWindow(mEnableAll && !mLowDoseMode);
  m_butAcquire.EnableWindow(canAcquire);
  m_sbcBinning.EnableWindow(mEnableAll);
  m_sbcSpot.EnableWindow(mEnableAll && !mLowDoseMode);
  m_butShiftBeam.EnableWindow(mEnableAll && !m_iUseCentroid);
  m_editBeamShift.EnableWindow(mEnableAll && !m_iUseCentroid && m_bShiftBeam);
  m_butRecordShift.EnableWindow(mEnableAll);
  m_butResetAddedShift.EnableWindow(mEnableAll);
  if (mLowDoseMode)
    ManageLDtrackText(mMultiTasks->AutocenMatchingIntensity());
}

// Set the text for whether intensity is being tracked
void CAutocenSetupDlg::ManageLDtrackText(bool tracking)
{
  m_statLdTrackScope.SetWindowText(tracking ? "Tracking intensity on scope" : 
    "NOT tracking intensity on scope");
}

// Hope this is all it takes!
void CAutocenSetupDlg::UpdateSettings()
{
  FetchParams();
  UpdateParamSettings();
  UpdateEnables();
}

