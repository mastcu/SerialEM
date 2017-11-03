// AutocenSetupDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\AutocenSetupDlg.h"
#include "EMscope.h"
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
{
  mScope = mWinApp->mScope;
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
  if (mWinApp->GetNumActiveCameras() > 1) {
    m_strCamName = mCamParams->name;
  } else {
    m_statCamLabel1.ShowWindow(SW_HIDE);
    m_statCamLabel2.ShowWindow(SW_HIDE);
    m_statCamName.ShowWindow(SW_HIDE);
  }

  // In low dose, start with the trial parameters for fetching the parameters to use
  if (mWinApp->LowDoseMode()) {
    mCurMagInd = ldp->magIndex;
    mCurSpot = ldp->spotSize;
    mCurProbe = ldp->probeMode;
    mCurIntensity = ldp->intensity;
    m_sbcMag.EnableWindow(false);
    m_sbcSpot.EnableWindow(false);
  } else {

    // Otherwise, take current state of scope and get settings for it
    mCurMagInd = mScope->GetMagIndex();
    mCurSpot = mScope->GetSpotSize();
    mCurProbe = mScope->ReadProbeMode();

    // This is held at the starting value when not tracking state, is sync'ed to the param
    // value but is needed for fetching param when tracking state, and reverts to the
    // value at the start of state tracking when leaving tracking
    mCurIntensity = mScope->GetIntensity();
  }
  FetchParams();
  mTestAcquire = false;

  // Seemingly nothing to do if set state is already on, as long as saved values are intact
  m_butAcquire.EnableWindow(m_bSetState);
  m_butMore.EnableWindow(m_bSetState);
  m_butLess.EnableWindow(m_bSetState);

  m_sbcSpot.SetRange(0,30000);
  m_sbcSpot.SetPos(15000);
  m_sbcMag.SetRange(0,30000);
  m_sbcMag.SetPos(15000);
  m_sbcBinning.SetRange(0,30000);
  m_sbcBinning.SetPos(15000);
  UpdateParamSettings();
  UpdateMagSpot();

  return TRUE;
}

void CAutocenSetupDlg::OnOK() 
{
  UpdateIfExposureChanged();
  if (m_bSetState && !mTestAcquire)
    RestoreScopeState();
	CBaseDlg::OnOK();
}

void CAutocenSetupDlg::OnCancel() 
{
  if (m_bSetState)
    RestoreScopeState();
  CBaseDlg::OnCancel();
}

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
}

void CAutocenSetupDlg::OnDeltaposSpinspot(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newspot = mCurSpot + pNMUpDown->iDelta;
  if (newspot < 1 || newspot > mScope->GetNumSpotSizes()) {
    *pResult = 1;
    return;
  }
  if (m_bSetState)
    mScope->SetSpotSize(newspot);
  MagOrSpotChanged(-1, newspot, -1);
  *pResult = 0;
}

void CAutocenSetupDlg::OnDeltaposSpinmag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newmag = mCurMagInd + pNMUpDown->iDelta;
  if (newmag < 1 || newmag >= MAX_MAGS || !mMagTab[newmag].mag) {
    *pResult = 1;
    return;
  }
  if (m_bSetState)
    mScope->SetMagIndex(newmag);
  MagOrSpotChanged(newmag, -1, -1);
  *pResult = 0;
}

void CAutocenSetupDlg::OnDeltaposSpinbinning(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newind = mBinIndex + pNMUpDown->iDelta;
  if (newind >= mCamParams->numBinnings || newind < (CamHasDoubledBinnings(mCamParams) ? 
    1 : 0)){
      *pResult = 1;
      return;
  }
  if (UpdateIfExposureChanged())
    FetchParams();
  mBinIndex = newind;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mCamParams->binnings[mBinIndex],
    mCamParams));
  mParam->binning = mCamParams->binnings[mBinIndex];
  ParamChanged();
  *pResult = 0;
}

void CAutocenSetupDlg::OnRadiouseedge()
{
  if (UpdateIfExposureChanged())
    FetchParams();
  mParam->useCentroid = m_iUseCentroid;
  ParamChanged();
}

void CAutocenSetupDlg::OnKillfocusEditExposure()
{
  UpdateData(true);
}

void CAutocenSetupDlg::OnButAcMore()
{
  UpdateIfExposureChanged();
  mWinApp->mBeamAssessor->ChangeBeamStrength(1.25, -1);
}

void CAutocenSetupDlg::OnButAcLess()
{
  UpdateIfExposureChanged();
  mWinApp->mBeamAssessor->ChangeBeamStrength(0.8, -1);
}

void CAutocenSetupDlg::OnTestacquire()
{
  mTestAcquire = true;
  OnOK();
}

// Called from scope update with current values when they have changed
void CAutocenSetupDlg::LiveUpdate(int magInd, int spotSize, int probe, double intensity)
{
  if (!m_bSetState)
    return;
  if (magInd != mCurMagInd || spotSize != mCurSpot || probe != mCurProbe) {
    MagOrSpotChanged(magInd, spotSize, probe);
  } else if (intensity != mParam->intensity) {

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

void CAutocenSetupDlg::OnDeleteSettings()
{
  mMultiTasks->DeleteAutocenParams(mCamera, mCurMagInd, mCurSpot, mCurProbe, 
    mCurIntensity);
  FetchParams();
  if (m_bSetState) {
    mParam->intensity = mScope->GetIntensity();
    mCurIntensity = mParam->intensity;
    ParamChanged();
  } else
    UpdateParamSettings();
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

void CAutocenSetupDlg::UpdateParamSettings(void)
{
  CString str1, str2;
  m_fExposure = (float)mParam->exposure;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mParam->binning, mCamParams));
  m_iUseCentroid = mParam->useCentroid;
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
      m_strSource = "Settings were derived from " + str1 + " and " + str2;
    } else
      m_strSource = "Settings were directly set at this mag && spot";

  } else {
    m_strIntensity = "--x--";
    m_strSource = "No usable settings available for this mag && spot";
  }
  m_butDelete.EnableWindow(!mSynthesized);
  UpdateData(false);
}

// Turn on tracking of external state
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
  if (m_bSetState) {
    if (mParam->intensity >= 0.)
      mScope->DelayedSetIntensity(mParam->intensity, GetTickCount());
    else
      mParam->intensity = mScope->GetIntensity();
    mCurIntensity = mParam->intensity;
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
