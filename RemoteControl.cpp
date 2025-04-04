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
#define MAX_BEAM_DELTA  0.81f
#define MIN_PCTC2_DELTA 0.015625f
#define MAX_PCTC2_DELTA 4.f
#define MAX_IA_DELTA   32.f
#define MAX_FOCUS_INDEX 12
#define MAX_FOCUS_DECIMALS 2
#define MAX_STAGE_INDEX 14
#define MAX_STAGE_DECIMALS 2

static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

// CRemoteControl dialog

CRemoteControl::CRemoteControl(CWnd* pParent /*=NULL*/)
	: CToolDlg(CRemoteControl::IDD, pParent)
  , m_bMagIntensity(FALSE)
  , m_strBeamDelta(_T(""))
  , m_strC2Delta(_T(""))
  , m_strC2Name(_T(""))
  , m_strFocusStep(_T(""))
  , m_strStageDelta(_T(""))
  , m_strMagWithC2(_T(""))
{
  SEMBuildTime(__DATE__, __TIME__);
  mBeamIncrement = 0.05f;
  mIntensityIncrement = 0.5f;
  mFocusIncrementIndex = 3 * MAX_FOCUS_DECIMALS;
  mStageIncIndex = 3 * MAX_STAGE_DECIMALS;
  mMaxClickInterval = 350;
  mDidExtendedTimeout = false;
  mDoingTask = 0;
}

CRemoteControl::~CRemoteControl()
{
}

void CRemoteControl::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_BUT_VALVES, m_butValves);
  DDX_Control(pDX, IDC_BUT_NANO_MICRO, m_butNanoMicro);
  //DDX_Control(pDX, IDC_CHECK_MAGINTENSITY, m_checkMagIntensity);
  //DDX_Check(pDX, IDC_CHECK_MAGINTENSITY, m_bMagIntensity);
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
  DDX_Control(pDX, IDC_STAT_ALPHA, m_statAlpha);
  DDX_Control(pDX, IDC_SPIN_ALPHA, m_sbcAlpha);
  DDX_Text(pDX, IDC_STAT_FOCUS_STEP, m_strFocusStep);
  DDX_Control(pDX, IDC_SPIN_FOCUS, m_sbcFocus);
  DDX_Control(pDX, IDC_BUT_DELFOCUSMINUS, m_butDelFocusMinus);
  DDX_Control(pDX, IDC_BUT_DELFOCUSPLUS, m_butDelFocusPlus);
  DDX_Control(pDX, IDC_BUT_SCREEN_UPDOWN, m_butScreenUpDown);
  DDX_Control(pDX, IDC_SPIN_STAGE_UP_DOWN, m_sbcStageUpDown);
  DDX_Control(pDX, IDC_SPIN_STAGE_LEFT_RIGHT, m_sbcStageLeftRight);
  DDX_Control(pDX, IDC_DELSTAGE_MINUS, m_butDelStageMinus);
  DDX_Control(pDX, IDC_DELSTAGE_PLUS, m_butDelStagePlus);
  DDX_Control(pDX, IDC_STAT_DELSTAGE, m_statStageDelta);
  DDX_Text(pDX, IDC_STAT_DELSTAGE, m_strStageDelta);
  DDX_Control(pDX, IDC_BLANK_UNBLANK, m_butBlankUnblank);
  DDX_Text(pDX, IDC_STAT_WITH_C2, m_strMagWithC2);
  DDX_Control(pDX, IDC_STAT_BEAM_LABEL, m_statBeamLabel);
  DDX_Control(pDX, IDC_STAT_BEAMDELTA, m_statBeamDelta);
  DDX_Control(pDX, IDC_STAT_FOCUS_LABEL, m_statFocusLabel);
  DDX_Control(pDX, IDC_STAT_FOCUS_STEP, m_statFocusDelta);
}


BEGIN_MESSAGE_MAP(CRemoteControl, CToolDlg)
  ON_BN_CLICKED(IDC_BUT_VALVES, OnButValves)
  ON_BN_CLICKED(IDC_BUT_NANO_MICRO, OnButNanoMicro)
  //ON_BN_CLICKED(IDC_CHECK_MAGINTENSITY, OnCheckMagIntensity)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCMAG, OnDeltaposSpinMag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCSPOT, OnDeltaposSpinSpot)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCBEAM, OnDeltaposSpinBeamShift)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RCINTENSITY, OnDeltaposSpinIntensity)
  ON_BN_CLICKED(IDC_BUT_DELBEAMMINUS, OnButDelBeamMinus)
  ON_BN_CLICKED(IDC_BUT_DELBEAMPLUS, OnButDelBeamPlus)
  ON_BN_CLICKED(IDC_BUT_DELC2MINUS, OnButDelC2Minus)
  ON_BN_CLICKED(IDC_BUT_DELC2PLUS, OnButDelC2Plus)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_BEAM_LEFT_RIGHT, OnDeltaposSpinBeamLeftRight)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALPHA, OnDeltaposSpinAlpha)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_FOCUS, OnDeltaposSpinFocus)
  ON_BN_CLICKED(IDC_BUT_DELFOCUSMINUS, OnDelFocusMinus)
  ON_BN_CLICKED(IDC_BUT_DELFOCUSPLUS, OnDelFocusPlus)
  ON_BN_CLICKED(IDC_BUT_SCREEN_UPDOWN, OnButScreenUpdown)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_STAGE_UP_DOWN, OnDeltaposSpinStageUpDown)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_STAGE_LEFT_RIGHT, OnDeltaposSpinStageLeftRight)
  ON_BN_CLICKED(IDC_BLANK_UNBLANK, OnBlankUnblank)
  ON_BN_CLICKED(IDC_DELSTAGE_PLUS, OnDelStagePlus)
  ON_BN_CLICKED(IDC_DELSTAGE_MINUS, OnDelStageMinus)
END_MESSAGE_MAP()



// CRemoteControl message handlers
BOOL CRemoteControl::OnInitDialog()
{
  UINT needLittle[] = {IDC_STAT_BEAMDELTA, IDC_STAT_DELSTAGE, IDC_STAT_FOCUS_STEP,
    IDC_STAT_C2DELTA};
  int ind;
  CToolDlg::OnInitDialog();
  CWnd *wnd;
  CFont *smallFont = &mDeltaFont;
  mScope = mWinApp->mScope;
  mShiftManager = mWinApp->mShiftManager;
  CRect rect;
  if (mWinApp->GetSystemDPI() >= 120 && !mWinApp->GetDisplayNotTruly120DPI()) {
    m_statBeamDelta.GetClientRect(rect);
    mDeltaFont.CreateFont(B3DNINT((mWinApp->GetSystemDPI() > 130 ? 0.87 : 0.82) 
      * rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  } else
    smallFont = mLittleFont;

  for (ind = 0; ind < sizeof(needLittle) / sizeof(UINT); ind++) {
    wnd = GetDlgItem(needLittle[ind]);
    if (wnd)
      wnd->SetFont(smallFont);
  }
  if (HitachiScope || mScope->GetNoScope())
    m_butValves.ShowWindow(SW_HIDE);
  if (mScope->GetNoColumnValve())
    m_butValves.SetWindowText("Turn On Beam");
  else
    m_butValves.SetWindowText(JEOLscope ? "Open Valve" : "Open Valves");
  if (!FEIscope)
    m_butNanoMicro.ShowWindow(SW_HIDE);
  if (!JEOLscope || mScope->GetHasNoAlpha()) {
    m_sbcAlpha.ShowWindow(SW_HIDE);
    m_statAlpha.ShowWindow(SW_HIDE);
  }
  m_sbcMag.SetRange(1, 2000);
  m_sbcSpot.SetRange(mScope->GetMinSpotSize(), mScope->GetNumSpotSizes());
  m_sbcBeamShift.SetRange(-30000, 30000);
  m_sbcBeamShift.SetPos(0);
  m_sbcIntensity.SetRange(-30000, 30000);
  m_sbcIntensity.SetPos(0);
  m_sbcAlpha.SetRange(-30000, 30000);
  m_sbcAlpha.SetPos(0);
  m_sbcFocus.SetRange(-30000, 30000);
  m_sbcFocus.SetPos(0);
  m_sbcBeamLeftRight.SetRange(-30000, 30000);
  m_sbcBeamLeftRight.SetPos(0);
  m_sbcStageUpDown.SetRange(-30000, 30000);
  m_sbcStageLeftRight.SetRange(-30000, 30000);
  m_sbcStageUpDown.SetPos(0);
  m_sbcStageLeftRight.SetPos(0);
  m_butDelBeamMinus.SetFont(&mButFont);
  m_butDelBeamPlus.SetFont(&mButFont);
  m_butDelC2Minus.SetFont(&mButFont);
  m_butDelC2Plus.SetFont(&mButFont);
  m_strC2Name = mScope->GetC2Name();
  m_butDelFocusMinus.SetFont(&mButFont);
  m_butDelFocusPlus.SetFont(&mButFont);
  mLastMagInd = mLastSpot = mLastSTEMmode = mLastProbeMode = mLastCamera = -1;
  mLastCamLenInd = -1;
  mLastScreenPos = -1;
  mLastGunOn = -2;
  mLastIntensity = -1.;
  mLastBlanked = -1;
  mLastAlpha = -999;
  m_butBlankUnblank.m_bShowSpecial = true;
  mSpotClicked = mMagClicked = false;
  mTimerID = NULL;
  mInitialized = true;
  SetIntensityIncrement(mIntensityIncrement);
  SetBeamOrStageIncrement(1., 0, 0);
  SetBeamOrStageIncrement(1., 0, 1);
  SetIncrementFromIndex(mFocusIncrement, mFocusIncrementIndex, mFocusIncrementIndex,
    MAX_FOCUS_INDEX, MAX_FOCUS_DECIMALS, m_strFocusStep);
  m_butDelFocusPlus.EnableWindow(mFocusIncrementIndex < MAX_FOCUS_INDEX);
  m_butDelFocusMinus.EnableWindow(mFocusIncrementIndex > 0);
  return TRUE;
}

// 12/13/24: Removed OnPaint For Guenter's white bands

// Called from scope update with current values; keeps track of last values seen and
// acts on changes only
void CRemoteControl::Update(int inMagInd, int inCamLen, int inSpot, double inIntensity, 
  int inProbe, int inGunOn, int inSTEM, int inAlpha, int inScreenPos, BOOL beamBlanked,
  BOOL stageReady)
{
  int junk;
  //bool enable;
  CString str;
  if (!mInitialized)
    return;
  BOOL doingOffset = mWinApp->mShiftCalibrator && 
    mWinApp->mShiftCalibrator->CalibratingOffset();
  bool baseEnable = !((mWinApp->DoingTasks() && !doingOffset && 
    !mWinApp->GetJustNavAcquireOpen()) || (mWinApp->mCamera && 
    mWinApp->mCamera->CameraBusy() && !mWinApp->mCamera->DoingContinuousAcquire()));
  bool stageBusy = !stageReady;

  if (inMagInd != mLastMagInd || inCamLen != mLastCamLenInd) {
    if (inMagInd) {
      m_sbcMag.SetPos(inMagInd);
      SetDlgItemText(IDC_STAT_MAG, "Mag");
    } else if (inCamLen) {
      m_sbcMag.SetPos(inCamLen);
      SetDlgItemText(IDC_STAT_MAG, "CamL");
    }
    m_sbcMag.EnableWindow((inMagInd > 0 || inCamLen > 0) && baseEnable && !doingOffset
      && !stageBusy);
    if (!mWinApp->mCamera->DoingContinuousAcquire())
    m_butNanoMicro.EnableWindow(((inSTEM && !mScope->MagIsInFeiLMSTEM(inMagInd)) ||
      (!inSTEM && inMagInd >= mScope->GetLowestMModeMagInd()) || 
      (!inMagInd && inCamLen < LAD_INDEX_BASE)) && baseEnable && !stageBusy);
    m_sbcAlpha.EnableWindow(inMagInd >= mScope->GetLowestMModeMagInd() && baseEnable && 
      !stageBusy);
  }

  if (inSpot != mLastSpot) {
    m_sbcSpot.SetPos(inSpot);
  }
  if (inScreenPos != mLastScreenPos) {
    m_butScreenUpDown.SetWindowText(inScreenPos == spUp ? "Lower Scrn" : "Raise Scrn");
    if (inScreenPos == spUnknown)
      m_butScreenUpDown.EnableWindow(false);
  }

  if (inGunOn != mLastGunOn) {
    if (mScope->GetNoColumnValve())
      m_butValves.SetWindowText(inGunOn > 0 ? "Turn Off Beam" : " Turn On Beam");
    else if (JEOLscope)
      m_butValves.SetWindowText(inGunOn > 0 ? "Close Valve" : "Open Valve");
    else
      m_butValves.SetWindowText(inGunOn > 0 ? "Close Valves" : "Open Valves");
    m_butValves.EnableWindow(inGunOn >= 0 && baseEnable);
    Invalidate();
  }

  if ((beamBlanked ? 1 : 0) != mLastBlanked) {
    if (baseEnable) {
      m_butBlankUnblank.m_bShowSpecial = true;
      m_butBlankUnblank.mSpecialColor = beamBlanked ? RGB(255, 255, 0) : RGB(0, 255, 0);
    } else {
      m_butBlankUnblank.m_bShowSpecial = false;
    }
    m_butBlankUnblank.SetWindowText(beamBlanked ? "Unblank" : "Blank");
    mLastBlanked = beamBlanked ? 1 : 0;
  }

  if (inProbe != mLastProbeMode && FEIscope) {
    m_butNanoMicro.SetWindowText(inProbe == 0 ? "-> uP" : "-> nP");
  }
  if (inSTEM != mLastSTEMmode)
    m_sbcSpot.SetRange(mScope->GetMinSpotSize(), mScope->GetNumSpotSizes(inSTEM));

  if (inIntensity != mLastIntensity || inProbe != mLastProbeMode || 
    inSTEM != mLastSTEMmode || inSpot != mLastSpot || inMagInd != mLastMagInd || 
    inCamLen != mLastCamLenInd) {
      if (inSTEM || inMagInd== 0)
        mMagIntensityOK = false;
      else
        mMagIntensityOK = mWinApp->mBeamAssessor->OutOfCalibratedRange(inIntensity, 
          inSpot, inProbe, junk) == 0;
      //m_checkMagIntensity.EnableWindow(enable);
  }

  mLastCamera = mWinApp->GetCurrentCamera();
  mLastMagInd = inMagInd;
  mLastCamLenInd = inCamLen;
  mLastSpot = inSpot;
  if (inGunOn != mLastGunOn) {
    m_butValves.m_bShowSpecial = inGunOn >= 0;
    if (inGunOn > 0)
      m_butValves.mSpecialColor = RGB(96, 255, 96);
    else
      m_butValves.mSpecialColor = JEOLscope ? RGB(255, 64, 64) : RGB(255, 255, 0);
  }
  mLastGunOn = inGunOn;
  mLastIntensity = inIntensity;
  mLastProbeMode = inProbe;
  mLastSTEMmode = inSTEM;
  mLastAlpha = inAlpha;
  mLastScreenPos = inScreenPos;
}

// Just update the window for magIntensity if settings changed, the increments already
// updated
void CRemoteControl::UpdateSettings()
{
  if (mInitialized)
    UpdateData(false);
}

// Just disable everything when a task or shot starts, only re-enable some items
// when free, and invalidate mag and spot so the regular update will re-enable the rest
// Not clear why this was done; it was originally just spot and intensity being enabled,
// and nano/micro flashed in continuous mode, so deferred it as well as alpha
// Leave everything but mag enabled when calibrating IS offsets
void CRemoteControl::UpdateEnables(void)
{
  if (!mInitialized)
    return;
  static int numTimes = 0;
  BOOL doingOffset = mWinApp->mShiftCalibrator &&
    mWinApp->mShiftCalibrator->CalibratingOffset();
  BOOL continuous = mWinApp->mCamera->DoingContinuousAcquire();
  bool stageBusy = mScope->StageBusy(-2) > 0;
  bool enable = !((mWinApp->DoingTasks() && !doingOffset && 
    !mWinApp->GetJustNavAcquireOpen()) || (mWinApp->mCamera && 
    mWinApp->mCamera->CameraBusy() && !continuous));
  m_sbcIntensity.EnableWindow(enable);
  m_sbcSpot.EnableWindow(enable && !stageBusy);
  m_sbcFocus.EnableWindow(enable);
  m_butScreenUpDown.EnableWindow(enable && !continuous && !stageBusy);
  m_sbcBeamShift.EnableWindow(enable && !stageBusy);
  m_sbcBeamLeftRight.EnableWindow(enable && !stageBusy);
  m_butBlankUnblank.EnableWindow(enable);

  if (numTimes++ < 4) {
    UINT needBold[] = {
    IDC_DELSTAGE_PLUS, IDC_BUT_DELBEAMPLUS, IDC_BUT_DELFOCUSPLUS, IDC_BUT_DELC2PLUS,
    IDC_DELSTAGE_MINUS, IDC_BUT_DELBEAMMINUS, IDC_BUT_DELFOCUSMINUS, IDC_BUT_DELC2MINUS};
    CFont *boldFont = mWinApp->GetBoldFont(&m_statAlpha);
    CString str;
    str.Format("%c", 0x96);
    for (int ind = 0; ind < sizeof(needBold) / sizeof(UINT); ind++) {
      CWnd *wnd = GetDlgItem(needBold[ind]);
      if (wnd) {
        wnd->SetFont(boldFont);
        if (ind >= 4)
          wnd->SetWindowText(str);
      }
    }
  }
  if (enable) {
    mLastSpot = -1;
    mLastGunOn = -2;
  } else {
    m_butValves.EnableWindow(false);
    m_sbcBeamShift.EnableWindow(false);
    m_sbcBeamLeftRight.EnableWindow(false);
    m_butNanoMicro.EnableWindow(false);
    m_sbcAlpha.EnableWindow(false);
  }
  if (enable && !doingOffset) {
    if (mScope->GetNoScope())
      m_sbcMag.EnableWindow(true);
    else
      mLastMagInd = -1;
  } else {
    m_sbcMag.EnableWindow(false);
  }
  m_butDelFocusMinus.EnableWindow(mFocusIncrementIndex > 0);
  m_butDelFocusPlus.EnableWindow(mFocusIncrementIndex < MAX_FOCUS_INDEX);
}

// Toggle valves/beam state
void CRemoteControl::OnButValves()
{
  int state = mScope->GetColumnValvesOpen();
  if (state >= 0) {
    if (mScope->SetColumnValvesOpen(state == 0) &&
      mScope->GetColumnValvesOpen() != state) {
      if (mScope->GetNoColumnValve())
        PrintfToLog("Beam should now be turning %s", state ? "OFF" : "ON");
      else
        PrintfToLog("Valve%s now %s", JEOLscope ? " is" : "s are",
          state ? "CLOSED" : "OPEN");
    }
  }
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Toggle probe mode
void CRemoteControl::OnButNanoMicro()
{
  int state = mScope->ReadProbeMode();
  SetFocus();
  mWinApp->RestoreViewFocus();
  mScope->SetProbeOrAdjustLDArea(state ? 0 : 1);
}

// Intensity zoom equivalent
/*void CRemoteControl::OnCheckMagIntensity()
{
  UpdateData(true);
  SetFocus();
  mWinApp->RestoreViewFocus();
}*/

// Mag
void CRemoteControl::OnDeltaposSpinMag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  CString str;
  int index, index2, numRegCamLens, numLADCamLens;
  if (mLastMagInd > 0) {
    if (mMagClicked) {
      index = mNewMagIndex;
    } else {
      index = mScope->GetMagIndex();
      mStartMagIndex = index;
    }

    // Call with noLM true for FEI STEM kept it from CHANGING in micro STEM, so test
    // here for going in or out
    index2 = mWinApp->FindNextMagForCamera(mWinApp->GetCurrentCamera(), index,
      pNMUpDown->iDelta > 0 ? 1 : -1);
    if (FEIscope && mLastSTEMmode && !mScope->UtapiSupportsService(UTSUP_MAGNIFICATION) &&
      (((index == mScope->GetLowestSTEMnonLMmag(0) - 1 ||
      index == mScope->GetLowestSTEMnonLMmag(1) - 1) && pNMUpDown->iDelta > 0) ||
      ((index == mScope->GetLowestSTEMnonLMmag(0) ||
        index == mScope->GetLowestSTEMnonLMmag(1) && pNMUpDown->iDelta < 0))))
      index2 = -1;
  } else if (mLastCamLenInd > 0) {
    mScope->GetNumCameraLengths(numRegCamLens, numLADCamLens); 
    if (mMagClicked)
      index = mNewCamLenIndex;
    else
      index = mScope->GetCamLenIndex();
    index2 = index + (pNMUpDown->iDelta > 0 ? 1 : -1);
    if (index2 < 1 || (index2 > numRegCamLens && !FEIscope)) {
      index2 = -1;
    } else if (FEIscope) {
      if (index2 > LAD_INDEX_BASE + numLADCamLens)
        index2 = -1;
      else if (index2 > numRegCamLens && index2 <= LAD_INDEX_BASE)
        index2 = pNMUpDown->iDelta > 0 ? LAD_INDEX_BASE + 1 : numRegCamLens;
    } 
  } else {
    return;
  }
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
  if (!mWinApp->GetSTEMMode() && mLastMagInd > 0 && mMagIntensityOK && mShiftPressed)
    str.Format("&& %s", mScope->GetC2Name());
  SetDlgItemText(IDC_STAT_WITH_C2, str);
  if (mLastMagInd > 0)
    mNewMagIndex = index2;
  else
    mNewCamLenIndex = index2;
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
    mScope->GetNumSpotSizes(-1), mNewSpotIndex)) {
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

// Proc to check for whether it is time to change mag or spot
static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mRemoteControl.SetMagOrSpot();
}

// Central routine for processing a mag or spot change in time-dependent way
void CRemoteControl::SetMagOrSpot(void)
{
  double delta, newInt, outFac;
  int curCam, err;
  bool magIntensity = false;
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
    if (mLastMagInd > 0) {
      magIntensity = mMagIntensityOK && mShiftPressed;
      curCam = mWinApp->GetCurrentCamera();
      if (magIntensity) {
        delta = pow((double)mShiftManager->GetPixelSize(curCam, mStartMagIndex) /
          mShiftManager->GetPixelSize(curCam, mNewMagIndex), 2.);
        err = mWinApp->mBeamAssessor->AssessBeamChange(delta, newInt, outFac, -1);
      } else
        SetDlgItemText(IDC_STAT_WITH_C2, "");
      mScope->SetMagOrAdjustLDArea(mNewMagIndex);
      if (magIntensity && !err)
        mScope->DelayedSetIntensity(newInt, GetTickCount());
      mWinApp->mAlignFocusWindow.UpdateAutofocus(mNewMagIndex);
    } else if (mLastCamLenInd > 0) {
      mScope->SetCamLenIndex(mNewCamLenIndex);
    }
    mMagClicked = false;
    SetDlgItemText(IDC_STAT_WITH_C2, "");
    m_sbcMag.EnableWindow(true);
    mWinApp->mCamera->InitiateIfPending();
  }
  if (mSpotClicked) {
    m_sbcSpot.EnableWindow(false);
    RedrawWindow();
    mScope->SetSpotSize(mNewSpotIndex);
    mSpotClicked = false;
    m_sbcSpot.EnableWindow(true);
    mWinApp->mCamera->InitiateIfPending();
  }
}

// Returns the mag and/or spot that it will be changed to when the timer expires
void CRemoteControl::GetPendingMagOrSpot(int &pendingMag, int &pendingSpot, 
  int &pendingCamLen)
{
  pendingMag = pendingSpot = pendingCamLen = -1;
  if (mTimerID && mSpotClicked)
    pendingSpot = mNewSpotIndex;
  if (mTimerID && mMagClicked) {
    if (mLastMagInd > 0)
      pendingMag = mNewMagIndex;
    else if (mLastCamLenInd > 0)
      pendingCamLen = mNewCamLenIndex;
  }
}

// Do the mag change when Ctrl is released
void CRemoteControl::CtrlChanged(bool pressed)
{
  mCtrlPressed = pressed;
  if (!pressed && (mMagClicked || mSpotClicked))
    SetMagOrSpot();
}

void CRemoteControl::ShiftChanged(bool pressed)
{
  CString str;
  mShiftPressed = pressed;
  if (!mMagClicked)
    return;
  if (!mWinApp->GetSTEMMode() && mLastMagInd > 0 && mMagIntensityOK &&
    pressed)
    str.Format("&& %s", mScope->GetC2Name());
  SetDlgItemText(IDC_STAT_WITH_C2, str);
}

// Alpha
void CRemoteControl::OnDeltaposSpinAlpha(NMHDR *pNMHDR, LRESULT *pResult)
{
  CString str;
  int newVal;
  SetFocus();
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, mScope->GetAlpha(), 0, mScope->GetNumAlphas() - 1,
    newVal))
    return;
  mScope->SetAlpha(newVal);
  mWinApp->mAlignFocusWindow.UpdateAutofocus(-1);
}

// Beam or stage up/down
void CRemoteControl::OnDeltaposSpinBeamShift(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  SetFocus();
  mWinApp->RestoreViewFocus();
  *pResult = 0;
  if (mLastMagInd > 0)
    mWinApp->mProcessImage->MoveBeamByCameraFraction(0.,
      mBeamIncrement * pNMUpDown->iDelta, true);
  else if (mLastCamLenInd > 0)
    ChangeDiffShift(0, pNMUpDown->iDelta);
}

// Beam or stage left-right: the horizontal spin box is backwards
void CRemoteControl::OnDeltaposSpinBeamLeftRight(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  SetFocus();
  mWinApp->RestoreViewFocus();
  *pResult = 0;
  if (mLastMagInd > 0)
    mWinApp->mProcessImage->MoveBeamByCameraFraction(
      -mBeamIncrement * pNMUpDown->iDelta, 0., true);
  else if (mLastCamLenInd > 0)
    ChangeDiffShift(pNMUpDown->iDelta, 0);
}

void CRemoteControl::ChangeDiffShift(int deltaX, int deltaY)
{
  double shiftX, shiftY, scale = 1.;
  if (FEIscope) {
    scale = mScope->GetLastCameraLength();
    if (!scale)
      return;
  }
  if (!mScope->GetDiffractionShift(shiftX, shiftY))
    return;
  mScope->SetDiffractionShift(shiftX + mBeamIncrement * deltaX / scale,
    shiftY + mBeamIncrement * deltaY / scale);
}

// Move the stage by a given number of microns in the current camera coordinate system
void CRemoteControl::MoveStageByMicronsOnCamera(double delCamX, double delCamY)
{
  int area, camera = mWinApp->GetCurrentCamera();
  int magIndex = mLastMagInd <= 0 ? mScope->GetMagIndex() : mLastMagInd;
  float pixel = mShiftManager->GetPixelSize(camera, magIndex);
  float defocus = 0.;
  double delX, delY, angle, delStageX, delStageY;
  StageMoveInfo moveInfo;
  ScaleMat bMat, bInv;
  delX = delCamX / pixel;
  delY = delCamY / pixel;
  if (mWinApp->LowDoseMode()) {
    area = mScope->GetLowDoseArea();
    if (area == VIEW_CONSET || area == SEARCH_AREA)
      defocus = mScope->GetLDViewDefocus(area);
  }
  bMat = mShiftManager->FocusAdjustedStageToCamera(camera, magIndex, 
    mLastSpot, mLastProbeMode, mScope->GetIntensity(), defocus);
  bInv = MatInv(bMat);
  delStageX = -(bInv.xpx * delX + bInv.xpy * delY);
  delStageY = -(bInv.ypx * delX + bInv.ypy * delY);
  angle = DTOR * mScope->GetTiltAngle();

  // If transformation exists, try to zero image shift too
  mShiftManager->AdjustStageMoveAndClearIS(camera, magIndex, delStageX, 
    delStageY, bInv);

  // Get the stage position and change it.  Set a flag and update state
  // to prevent early pictures, use reset shift task handlers
  mScope->GetStagePosition(moveInfo.x, moveInfo.y, moveInfo.z);
  mShiftManager->MaintainOrImposeBacklash(&moveInfo, delStageX, 
    delStageY / cos(angle),
    mShiftManager->GetBacklashMouseAndISR() && fabs(angle / DTOR) < 10.);
  moveInfo.axisBits = axisXY;
  mDoingTask = 2;
  m_sbcBeamShift.EnableWindow(false);
  m_sbcBeamLeftRight.EnableWindow(false);
  mWinApp->UpdateBufferWindows();
  mScope->MoveStage(moveInfo, moveInfo.backX != 0. || moveInfo.backY != 0.);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_REMOTE_CTRL, 0, 60000);
}

// C2 or other lens value
void CRemoteControl::OnDeltaposSpinIntensity(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  ChangeIntensityByIncrement(pNMUpDown->iDelta);
  *pResult = 0;
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Change the beam or stage step sizes
void CRemoteControl::OnButDelBeamMinus()
{
  SetBeamOrStageIncrement(0.707107f, -1, 0);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::OnButDelBeamPlus()
{
  SetBeamOrStageIncrement(1.414214f, 1, 0);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Set the appropriate increment
void CRemoteControl::SetBeamOrStageIncrement(float beamIncFac, int stageIndAdd,
  int stageNotBeam)
{
  if (stageNotBeam)
    SetStageIncrementIndex(mStageIncIndex + stageIndAdd);
  else
    SetBeamIncrement(beamIncFac * mBeamIncrement);
}

// Set the increment for beam shift and update display if it is open and beam is selected
void CRemoteControl::SetBeamIncrement(float inVal)
{
  if (inVal < MIN_BEAM_DELTA - 0.00001 || inVal > MAX_BEAM_DELTA + 0.00001)
    return;
  B3DCLAMP(inVal, MIN_BEAM_DELTA, MAX_BEAM_DELTA);
  mBeamIncrement = inVal;
  if (!mInitialized)
    return;
  m_strBeamDelta.Format("%c%.1f%%", 0xB1, 100. * mBeamIncrement);
  UpdateData(false);
}

// Set increment for stage shift and update display if it is open and stage is selected
void CRemoteControl::SetStageIncrementIndex(int inVal)
{
  SetIncrementFromIndex(mStageIncrement, mStageIncIndex, inVal, 
    MAX_STAGE_INDEX, MAX_STAGE_DECIMALS, m_strStageDelta);
}

// Set C2 step size
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

// Set the increment for intensity changes
void CRemoteControl::SetIntensityIncrement(float inVal)
{
  float maxDelta = (mWinApp->mScope && !mWinApp->mScope->GetUseIllumAreaForC2()) ?
    MAX_IA_DELTA : MAX_PCTC2_DELTA;
  if (inVal < MIN_PCTC2_DELTA - 0.0001 || inVal > maxDelta + 0.0001)
    return;
  B3DCLAMP(inVal, MIN_PCTC2_DELTA, maxDelta);
  mIntensityIncrement = inVal;
  if (!mInitialized)
    return;
  m_strC2Delta.Format("%c%s%s", 0xB1, FormattedNumber(mIntensityIncrement, "", 1, 3, 1.f, 
    true), mScope->GetC2Units());
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
    if (newVal <= 0. && delta > 0)
      newVal = 0.01 * mIntensityIncrement;
    if (newVal >= 1. && delta < 0)
      newVal = 1. - 0.01 * mIntensityIncrement;
  }
  if (newVal > 0. && (newVal < 1.0 || mWinApp->mScope->GetUseIllumAreaForC2()))
    mScope->SetIntensity(newVal, mLastSpot, mLastProbeMode);
}

// Change focus
void CRemoteControl::OnDeltaposSpinFocus(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  mScope->IncDefocus(mFocusIncrement * pNMUpDown->iDelta);
  *pResult = 0;
  SetFocus();
  mWinApp->RestoreViewFocus();
}

// Decrease or increase focus increment
void CRemoteControl::OnDelFocusMinus()
{
  SetFocusIncrementIndex(mFocusIncrementIndex - 1);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::OnDelFocusPlus()
{
  SetFocusIncrementIndex(mFocusIncrementIndex + 1);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CRemoteControl::SetFocusIncrementIndex(int inVal)
{
  SetIncrementFromIndex(mFocusIncrement, mFocusIncrementIndex, inVal,
    MAX_FOCUS_INDEX, MAX_FOCUS_DECIMALS, m_strFocusStep);
  if (!mInitialized)
    return;
  m_butDelFocusPlus.EnableWindow(mFocusIncrementIndex < MAX_FOCUS_INDEX);
  m_butDelFocusMinus.EnableWindow(mFocusIncrementIndex > 0);
}

// Set an increment via its index
void CRemoteControl::SetIncrementFromIndex(float &incrVal, int &incrInd, int newInd, 
  int maxIndex, int maxDecimals, CString &str)
{
  float digits[3] = {1., 2., 5.};
  B3DCLAMP(newInd, 0, maxIndex);
  incrVal = digits[newInd % 3] * (float)pow(10., newInd / 3 - maxDecimals);
  incrInd = newInd;
  if (!mInitialized)
    return;
  str.Format("%c%gum", 0xB1, incrVal);
  UpdateData(false);
}

// Handle screen raise or lower
void CRemoteControl::OnButScreenUpdown()
{
  SetFocus();
  mWinApp->RestoreViewFocus();
  if (mLastScreenPos == spUnknown)
    return;
  mDoingTask = 1;
  mWinApp->UpdateBufferWindows();
  mScope->SetScreenPos(mLastScreenPos == spUp ? spDown : spUp);
  mWinApp->AddIdleTask(CEMscope::TaskScreenBusy, TASK_REMOTE_CTRL, 0, 60000);
  mWinApp->SetStatusText(SIMPLE_PANE, mLastScreenPos == spUp ? "LOWERING SCREEN" :
    "RAISING SCREEN");
}

// Task routines
void CRemoteControl::TaskDone(int param)
{
  mDoingTask = 0;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "");
}

void CRemoteControl::TaskCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(mDoingTask > 1 ? _T("Time out moving stage") : 
      _T("Time out setting screen position"));
  TaskDone(0);
  mWinApp->ErrorOccurred(1);
}


void CRemoteControl::OnDeltaposSpinStageUpDown(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  SetFocus();
  mWinApp->RestoreViewFocus();
  *pResult = 0;
  MoveStageByMicronsOnCamera(0., mStageIncrement * pNMUpDown->iDelta);
}


void CRemoteControl::OnDeltaposSpinStageLeftRight(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  SetFocus();
  mWinApp->RestoreViewFocus();
  *pResult = 0;
  MoveStageByMicronsOnCamera(-mStageIncrement * pNMUpDown->iDelta, 0.);
}


void CRemoteControl::OnBlankUnblank()
{
  mScope->BlankBeam(!mLastBlanked, "scope control");
}


void CRemoteControl::OnDelStagePlus()
{
  SetBeamOrStageIncrement(1.414214f, 1, 1);
  SetFocus();
  mWinApp->RestoreViewFocus();
}


void CRemoteControl::OnDelStageMinus()
{
  SetBeamOrStageIncrement(0.707107f, -1, 1);
  SetFocus();
  mWinApp->RestoreViewFocus();
}
