// RemoteControl.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "RemoteControl.h"
#include "EMscope.h"
#include "CameraController.h"
#include "BeamAssessor.h"
#include "ProcessImage.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"

#define MIN_BEAM_DELTA  0.00625f
#define MAX_BEAM_DELTA  0.8f
#define MIN_PCTC2_DELTA 0.015625f
#define MAX_PCTC2_DELTA 4.f

static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

// CRemoteControl dialog

CRemoteControl::CRemoteControl(CWnd* pParent /*=NULL*/)
	: CToolDlg(CRemoteControl::IDD, pParent)
  , m_bMagIntensity(FALSE)
  , m_strBeamDelta(_T(""))
  , m_strC2Delta(_T(""))
  , m_strC2Name(_T(""))
{
  SEMBuildTime(__DATE__, __TIME__);
  mInitialized = false;
  mBeamIncrement = 0.05f;
  mIntensityIncrement = 0.5f;
  mMaxClickInterval = 350;
  mDidExtendedTimeout = false;
}

CRemoteControl::~CRemoteControl()
{
}

void CRemoteControl::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_BUT_VALVES, m_butValves);
  DDX_Control(pDX, IDC_BUT_NANO_MICRO, m_butNanoMicro);
  DDX_Control(pDX, IDC_CHECK_MAGINTENSITY, m_checkMagIntensity);
  DDX_Check(pDX, IDC_CHECK_MAGINTENSITY, m_bMagIntensity);
  DDX_Control(pDX, IDC_SPIN_RCMAG, m_sbcMag);
  DDX_Control(pDX, IDC_SPIN_RCSPOT, m_sbcSpot);
  DDX_Control(pDX, IDC_SPIN_RCBEAM, m_sbcBeamShift);
  DDX_Control(pDX, IDC_SPIN_RCINTENSITY, m_sbcIntensity);
  DDX_Control(pDX, IDC_BUT_DELBEAMMINUS, m_butDelBeamMinus);
  DDX_Control(pDX, IDC_BUT_DELBEAMPLUS, m_butDelBeamPlus);
  DDX_Control(pDX, IDC_BUT_DELC2MINUS, m_butDelC2Minus);
  DDX_Control(pDX, IDC_BUT_DELC2PLUS, m_butDelC2Plus);
  DDX_Text(pDX, IDC_STAT_BEAMDELTA, m_strBeamDelta);
  DDX_Text(pDX, IDC_STAT_C2DELTA, m_strC2Delta);
  DDX_Control(pDX, IDC_SPIN_BEAM_LEFT_RIGHT, m_sbcBeamLeftRight);
  DDX_Text(pDX, IDC_STAT_RCC2IA, m_strC2Name);
}


BEGIN_MESSAGE_MAP(CRemoteControl, CToolDlg)
  ON_BN_CLICKED(IDC_BUT_VALVES, OnButValves)
  ON_BN_CLICKED(IDC_BUT_NANO_MICRO, OnButNanoMicro)
  ON_BN_CLICKED(IDC_CHECK_MAGINTENSITY, OnCheckMagIntensity)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCMAG, OnDeltaposSpinMag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCSPOT, OnDeltaposSpinSpot)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCBEAM, OnDeltaposSpinBeamShift)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCINTENSITY, OnDeltaposSpinIntensity)
  ON_BN_CLICKED(IDC_BUT_DELBEAMMINUS, OnButDelBeamMinus)
  ON_BN_CLICKED(IDC_BUT_DELBEAMPLUS, OnButDelBeamPlus)
  ON_BN_CLICKED(IDC_BUT_DELC2MINUS, OnButDelC2Minus)
  ON_BN_CLICKED(IDC_BUT_DELC2PLUS, OnButDelC2Plus)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_BEAM_LEFT_RIGHT, OnDeltaposSpinBeamLeftRight)
END_MESSAGE_MAP()



// CRemoteControl message handlers
BOOL CRemoteControl::OnInitDialog()
{
  CToolDlg::OnInitDialog();
  mScope = mWinApp->mScope;
  if (HitachiScope || mScope->GetNoScope())
    m_butValves.ShowWindow(SW_HIDE);
  if (!FEIscope)
    m_butNanoMicro.ShowWindow(SW_HIDE);
  m_sbcMag.SetRange(1, 2000);
  m_sbcSpot.SetRange(mScope->GetMinSpotSize(), mScope->GetNumSpotSizes());
  m_sbcBeamShift.SetRange(-30000, 30000);
  m_sbcBeamShift.SetPos(0);
  m_sbcIntensity.SetRange(-30000, 30000);
  m_sbcIntensity.SetPos(0);
  m_butDelBeamMinus.SetFont(&mButFont);
  m_butDelBeamPlus.SetFont(&mButFont);
  m_butDelC2Minus.SetFont(&mButFont);
  m_butDelC2Plus.SetFont(&mButFont);
  m_strC2Name = mScope->GetC2Name();
  mLastMagInd = mLastSpot = mLastSTEMmode = mLastProbeMode = mLastCamera = -1;
  mLastGunOn = -2;
  mLastIntensity = -1.;
  mSpotClicked = mMagClicked = false;
  mTimerID = NULL;
  mInitialized = true;
  SetIntensityIncrement(mIntensityIncrement);
  SetBeamIncrement(mBeamIncrement);
  return TRUE;
}

// Called from scope update with current values; keeps track of last values seen and
// acts on changes only
void CRemoteControl::Update(int inMagInd, int inSpot, double inIntensity, int inProbe,
  int inGunOn, int inSTEM)
{
  int junk;
  bool enable;
  bool baseEnable = !(mWinApp->DoingTasks() || (mWinApp->mCamera && 
    mWinApp->mCamera->CameraBusy() && !mWinApp->mCamera->DoingContinuousAcquire()));

  if (inMagInd != mLastMagInd || mWinApp->GetCurrentCamera() != mLastCamera) {
    enable = mWinApp->mProcessImage->MoveBeamByCameraFraction(0., 0.) == 0;
    m_sbcBeamShift.EnableWindow(enable && baseEnable);
    m_sbcBeamLeftRight.EnableWindow(enable && baseEnable);
  }

  if (inMagInd != mLastMagInd) {
    if (inMagInd)
      m_sbcMag.SetPos(inMagInd);
    m_sbcMag.EnableWindow(inMagInd > 0 && baseEnable);
    m_butNanoMicro.EnableWindow(inMagInd >= mScope->GetLowestMModeMagInd());
  }

  if (inSpot != mLastSpot) {
    m_sbcSpot.SetPos(inSpot);
  }

  if (inGunOn != mLastGunOn) {
    if (mScope->GetNoColumnValve())
      m_butValves.SetWindowText(inGunOn > 0 ? "Turn Off Beam" : " Turn On Beam");
    else if (JEOLscope)
      m_butValves.SetWindowText(inGunOn > 0 ? "Close Valve" : "Open Valve");
    else
      m_butValves.SetWindowText(inGunOn > 0 ? "Close Valves" : "Open Valves");
    m_butValves.EnableWindow(inGunOn >= 0 && baseEnable);
  }

  if (inProbe != mLastProbeMode && FEIscope) {
    m_butNanoMicro.SetWindowText(inProbe == 0 ? "Micro probe" : "Nano probe");
  }

  if (inIntensity != mLastIntensity || inProbe != mLastProbeMode || 
    inSTEM != mLastSTEMmode || inSpot != mLastSpot) {
      if (inSTEM)
        enable = false;
      else 
        enable = mWinApp->mBeamAssessor->OutOfCalibratedRange(inIntensity, inSpot, 
          inProbe, junk) == 0;
      m_checkMagIntensity.EnableWindow(enable);
  }

  mLastCamera = mWinApp->GetCurrentCamera();
  mLastMagInd = inMagInd;
  mLastSpot = inSpot;
  mLastGunOn = inGunOn;
  mLastIntensity = inIntensity;
  mLastProbeMode = inProbe;
  mLastSTEMmode = inSTEM;
}

// Just update the window for magIntensity if settings changed, the increments already
// updated
void CRemoteControl::UpdateSettings()
{
  if (mInitialized)
    UpdateData(false);
}

// Just disable everything when a task or shot starts, only re-enable intensity and spot
// when free, and invalidate mag and spot so the regular update will re-enable
// Leave everything but mag enabled when calibrating IS offsets
void CRemoteControl::UpdateEnables(void)
{
  if (!mInitialized)
    return;
  BOOL doingOffset = mWinApp->mShiftCalibrator->CalibratingOffset();
  bool enable = !((mWinApp->DoingTasks() && !doingOffset) || (mWinApp->mCamera && 
    mWinApp->mCamera->CameraBusy() && !mWinApp->mCamera->DoingContinuousAcquire()));
  m_sbcIntensity.EnableWindow(enable);
  m_sbcSpot.EnableWindow(enable);
  m_butNanoMicro.EnableWindow(enable && mLastMagInd >= mScope->GetLowestMModeMagInd());

  if (enable) {
    mLastSpot = -1;
    mLastGunOn = -2;
  } else {
    m_butValves.EnableWindow(false);
    m_sbcBeamShift.EnableWindow(false);
    m_sbcBeamLeftRight.EnableWindow(false);
  }
  if (enable && !doingOffset) {
    if (mScope->GetNoScope())
      m_sbcMag.EnableWindow(true);
    else
      mLastMagInd = -1;
  } else {
    m_sbcMag.EnableWindow(false);
  }
}

// Toggle valves/beam state
void CRemoteControl::OnButValves()
{
  int state = mScope->GetColumnValvesOpen();
  if (state >= 0) {
    mScope->SetColumnValvesOpen(state == 0);
    if (mScope->GetNoColumnValve())
      PrintfToLog("Beam is now %s", state ? "OFF" : "ON");
    else
      PrintfToLog("Valve%s now %s", JEOLscope ? " is" : "s are", 
        state ? "CLOSED" : "OPEN");
  }
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Toggle probe mode
void CRemoteControl::OnButNanoMicro()
{
  int state = mScope->ReadProbeMode();
  mScope->SetProbeMode(state ? 0 : 1);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Intensity zoom equivalent
void CRemoteControl::OnCheckMagIntensity()
{
  UpdateData(true);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Mag
void CRemoteControl::OnDeltaposSpinMag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int index, index2;
  if (mMagClicked) {
    index = mNewMagIndex;
  } else {
    index = mScope->GetMagIndex();
    mStartMagIndex = index;
  }
  index2 = mWinApp->FindNextMagForCamera(mWinApp->GetCurrentCamera(), index, 
    pNMUpDown->iDelta > 0 ? 1 : -1);
  SetFocus();
  mWinApp->RestoreViewFocus();
  if (index2 < 0) {
    if (mMagClicked) {
      ::KillTimer(NULL, mTimerID);
      mTimerID = NULL;
      SetMagOrSpot();
    }
    return;
  }
  mMagClicked = true;
  mNewMagIndex = index2;
  mDidExtendedTimeout = false;
  mTimerID = ::SetTimer(NULL, mTimerID, mMaxClickInterval, TimerProc);
  if (!mTimerID)
     SetMagOrSpot();
  *pResult = 0;
}

// Spot size 
void CRemoteControl::OnDeltaposSpinSpot(NMHDR *pNMHDR, LRESULT *pResult)
{
  int oldVal;
  if (mSpotClicked)
    oldVal = mNewSpotIndex;
  else
    oldVal = mScope->GetSpotSize();

  SetFocus();
  mWinApp->RestoreViewFocus();
  if (!NewSpinnerValue(pNMHDR, pResult, oldVal, mScope->GetMinSpotSize(),
    mScope->GetNumSpotSizes(), mNewSpotIndex)) {
      mSpotClicked = true;
      mDidExtendedTimeout = false;
      mTimerID = ::SetTimer(NULL, mTimerID, mMaxClickInterval, TimerProc);
      if (!mTimerID)
        SetMagOrSpot();
  } else if (mSpotClicked) {
    ::KillTimer(NULL, mTimerID);
    mTimerID = NULL;
    SetMagOrSpot();
  }
}

static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mRemoteControl.SetMagOrSpot();
}

void CRemoteControl::SetMagOrSpot(void)
{
  double delta, newInt, outFac;
  int curCam, err;
  if (mTimerID)
    ::KillTimer(NULL, mTimerID);
  mTimerID = NULL;

  // If Ctrl is down, extend the timeout for ~2 seconds 
  if (mCtrlPressed && !mDidExtendedTimeout) {
    mTimerID = ::SetTimer(NULL, mTimerID, 6 * mMaxClickInterval, TimerProc);
    if (mTimerID) {
      mDidExtendedTimeout = true;
      return;
    }
  }
  if (mMagClicked) {
    m_sbcMag.EnableWindow(false);
    RedrawWindow();
    curCam = mWinApp->GetCurrentCamera();
    if (m_bMagIntensity) {
      delta = pow((double)mWinApp->mShiftManager->GetPixelSize(curCam, mStartMagIndex) /
        mWinApp->mShiftManager->GetPixelSize(curCam, mNewMagIndex), 2.);
      err = mWinApp->mBeamAssessor->AssessBeamChange(delta, newInt, outFac, -1);
    }
    mScope->SetMagIndex(mNewMagIndex);
    if (m_bMagIntensity && !err)
      mScope->DelayedSetIntensity(newInt, GetTickCount());
    mMagClicked = false;
    m_sbcMag.EnableWindow(true);
  }
  if (mSpotClicked) {
    m_sbcSpot.EnableWindow(false);
    RedrawWindow();
    mScope->SetSpotSize(mNewSpotIndex);
    mSpotClicked = false;
    m_sbcSpot.EnableWindow(true);
  }
}

// Returns the mag and/or spot that it will be changed to when the timer expires
void CRemoteControl::GetPendingMagOrSpot(int &pendingMag, int &pendingSpot)
{
  pendingMag = pendingSpot = -1;
  if (mTimerID && mSpotClicked)
    pendingSpot = mNewSpotIndex;
  if (mTimerID && mMagClicked)
    pendingMag = mNewMagIndex;
}

// Do the mag change when Ctrl is released
void CRemoteControl::CtrlChanged(bool pressed)
{
  mCtrlPressed = pressed;
  if (!pressed && (mMagClicked || mSpotClicked))
    SetMagOrSpot();
}

// Beam up/down
void CRemoteControl::OnDeltaposSpinBeamShift(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  mWinApp->mProcessImage->MoveBeamByCameraFraction(0, mBeamIncrement * pNMUpDown->iDelta);
  *pResult = 0;
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Beam left-right: the horizontal spin box is backwrds
void CRemoteControl::OnDeltaposSpinBeamLeftRight(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  mWinApp->mProcessImage->MoveBeamByCameraFraction(-mBeamIncrement * pNMUpDown->iDelta,
    0.);
  *pResult = 0;
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// C2 or other lens value
void CRemoteControl::OnDeltaposSpinIntensity(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  ChangeIntensityByIncrement(pNMUpDown->iDelta);
  /*double newVal = mScope->GetIntensity() + 0.01 * mIntensityIncrement * pNMUpDown->iDelta 
    / mScope->GetC2IntensityFactor();
  if (newVal > 0. && newVal < 1.0)
    mScope->SetIntensity(newVal);*/
  *pResult = 0;
  SetFocus();
  mWinApp->RestoreViewFocus();
}
// Change the step sizes...
void CRemoteControl::OnButDelBeamMinus()
{
  SetBeamIncrement(0.707107f * mBeamIncrement);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::OnButDelBeamPlus()
{
  SetBeamIncrement(1.414214f * mBeamIncrement);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::OnButDelC2Minus()
{
  SetIntensityIncrement(0.707107f * mIntensityIncrement);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::OnButDelC2Plus()
{
  SetIntensityIncrement(1.414214f * mIntensityIncrement);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Set the increment for beam shift and update the display if it is open
void CRemoteControl::SetBeamIncrement(float inVal)
{
  if (inVal < MIN_BEAM_DELTA - 0.00001 || inVal > MAX_BEAM_DELTA + 0.00001)
    return;
  B3DCLAMP(inVal, MIN_BEAM_DELTA, MAX_BEAM_DELTA);
  mBeamIncrement = inVal;
  if (!mInitialized)
    return;
  m_strBeamDelta.Format("%.3f", mBeamIncrement);
  UpdateData(false);
}

// Set the increment for intensity changes
void CRemoteControl::SetIntensityIncrement(float inVal)
{
  if (inVal < MIN_PCTC2_DELTA - 0.0001 || inVal > MAX_PCTC2_DELTA + 0.0001)
    return;
  B3DCLAMP(inVal, MIN_PCTC2_DELTA, MAX_PCTC2_DELTA);
  mIntensityIncrement = inVal;
  if (!mInitialized)
    return;
  m_strC2Delta.Format("%.3f", mIntensityIncrement);
  UpdateData(false);
}

// Do intensity change by a step
void CRemoteControl::ChangeIntensityByIncrement(int delta)
{
  double newVal, illum;
  double oldVal = mWinApp->mScope->GetIntensity();
  if (mWinApp->mScope->GetUseIllumAreaForC2()) {
    illum = mWinApp->mScope->IntensityToIllumArea(oldVal);
    newVal = mWinApp->mScope->IllumAreaToIntensity(illum + 0.01 * mIntensityIncrement * 
      delta);
  } else {
    newVal = mScope->GetIntensity() + 0.01 * mIntensityIncrement * delta /
      mScope->GetC2IntensityFactor();
  }
  if (newVal > 0. && newVal < 1.0)
    mScope->SetIntensity(newVal);
}
