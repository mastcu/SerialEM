// CookerSetupDlg.cpp:   To set parameters for specimen pre-exposure
//
// Copyright (C) 2008 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\CookerSetupDlg.h"
#include "EMscope.h"
#include "BeamAssessor.h"

#define TARGET_MIN  1
#define TARGET_MAX  30000
#define TARGET_INC  50
#define TIME_MIN    0.1f
#define TIME_MAX    300.f


// CCookerSetupDlg dialog

CCookerSetupDlg::CCookerSetupDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CCookerSetupDlg::IDD, pParent)
  , m_strMag(_T(""))
  , m_strSpot(_T(""))
  , m_strIntensity(_T(""))
  , m_strDoseRate(_T(""))
  , m_strTotalTime(_T(""))
  , m_bTrackState(FALSE)
  , m_bAlignCooked(FALSE)
  , m_iDose(1000)
  , m_bTiltForCook(FALSE)
  , m_fTiltAngle(0.)
  , m_fTime(0)
  , m_strDoseTime(_T(""))
{
  mScope = mWinApp->mScope;
  mCookP = mWinApp->GetCookParams();
  mDisableGo = false;
}

CCookerSetupDlg::~CCookerSetupDlg()
{
}

void CCookerSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_COOKMAG, m_strMag);
  DDX_Text(pDX, IDC_COOKSPOT, m_strSpot);
  DDX_Text(pDX, IDC_COOKINTENSITY, m_strIntensity);
  DDX_Text(pDX, IDC_COOKDOSERATE, m_strDoseRate);
  DDX_Text(pDX, IDC_COOKTOTALTIME, m_strTotalTime);
  DDX_Check(pDX, IDC_SET_TRACK_STATE, m_bTrackState);
  DDX_Check(pDX, IDC_ALIGNCOOKED, m_bAlignCooked);
  DDX_Control(pDX, IDC_SPINCOOKDOSE, m_sbcDose);
  DDX_Text(pDX, IDC_EDITCOOKDOSE, m_iDose);
  DDV_MinMaxInt(pDX, m_iDose, TARGET_MIN, TARGET_MAX);
  DDX_Check(pDX, IDC_TILTFORCOOK, m_bTiltForCook);
  DDX_Control(pDX, IDC_EDITCOOKTILT, m_editCookTilt);
  DDX_Text(pDX, IDC_EDITCOOKTILT, m_fTiltAngle);
  DDV_MinMaxFloat(pDX, m_fTiltAngle, -70.f, 70.f);
  DDX_Control(pDX, IDC_EDITCOOKTIME, m_editCookTime);
  DDX_Text(pDX, IDC_EDITCOOKTIME, m_fTime);
  DDV_MinMaxFloat(pDX, m_fTime, TIME_MIN, TIME_MAX);
  DDX_Control(pDX, IDC_SPINCOOKTIME, m_sbcCookTime);
  DDX_Radio(pDX, IDC_RADIODOSE, m_iTimeInstead);
  DDX_Text(pDX, IDC_STATDOSETIME, m_strDoseTime);
  DDX_Control(pDX, IDC_EDITCOOKDOSE, m_editCookDose);
  DDX_Control(pDX, IDC_COOKGO, m_butGoCook);
  DDX_Control(pDX, IDC_STATDOSETIME, m_statDoseTime);
  DDX_Control(pDX, IDC_COOKTOTALTIME, m_statTotalTime);
}


BEGIN_MESSAGE_MAP(CCookerSetupDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_SET_TRACK_STATE, OnSetTrackState)
  ON_BN_CLICKED(IDC_ALIGNCOOKED, OnAligncooked)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCOOKDOSE, OnDeltaposSpincookdose)
  ON_EN_KILLFOCUS(IDC_EDITCOOKDOSE, OnKillfocusEditcookdose)
  ON_BN_CLICKED(IDC_TILTFORCOOK, OnTiltforcook)
  ON_EN_KILLFOCUS(IDC_EDITCOOKTILT, OnKillfocusEditcooktilt)
  ON_BN_CLICKED(IDC_RADIODOSE, OnRadiodose)
  ON_BN_CLICKED(IDC_RADIOTIME, OnRadiodose)
  ON_EN_KILLFOCUS(IDC_EDITCOOKTIME, OnKillfocusEditcooktime)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCOOKTIME, OnDeltaposSpincooktime)
  ON_BN_CLICKED(IDC_COOKGO, OnCookgo)
END_MESSAGE_MAP()


// CCookerSetupDlg message handlers

BOOL CCookerSetupDlg::OnInitDialog()
{
	CBaseDlg::OnInitDialog();
  CRect rect;
  m_statDoseTime.GetWindowRect(&rect);
  mBigFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  m_statDoseTime.SetFont(&mBigFont);
  m_statTotalTime.SetFont(&mBigFont);

  // Set parameters with current values if uninitialized
  if (mCookP->magIndex <= 0)
    mCookP->magIndex = mScope->GetMagIndex();
  if (mCookP->spotSize <= 0)
    mCookP->spotSize = mScope->GetSpotSize();
  if (mCookP->intensity < 0)
    mCookP->intensity = mScope->GetIntensity();

  m_bTrackState = false;
  mCurMagInd = mCookP->magIndex;
  mCurSpot = mCookP->spotSize;
  mCurIntensity = mCookP->intensity;
  m_bAlignCooked = mCookP->trackImage;
  m_iDose = mCookP->targetDose;
  m_fTime = mCookP->minutes;
  m_iTimeInstead = mCookP->timeInstead;
  m_bTiltForCook = mCookP->cookAtTilt;
  m_fTiltAngle = mCookP->tiltAngle;
  m_editCookTilt.EnableWindow(m_bTiltForCook);
  m_editCookDose.EnableWindow(m_iTimeInstead == 0);
  m_editCookTime.EnableWindow(m_iTimeInstead != 0);
  m_sbcDose.EnableWindow(m_iTimeInstead == 0);
  m_sbcCookTime.EnableWindow(m_iTimeInstead != 0);

  m_sbcDose.SetRange(TARGET_MIN, TARGET_MAX);
  m_sbcDose.SetPos(m_iDose);
  m_sbcCookTime.SetRange(0, 30000);
  m_sbcCookTime.SetPos(15000);
  m_bGoCook = false;

  UpdateMagBeam();

  return TRUE;
}

void CCookerSetupDlg::OnOK() 
{
  UpdateData(true);
  mCookP->magIndex = mCurMagInd;
  mCookP->spotSize = mCurSpot;
  mCookP->intensity = mCurIntensity;
  mCookP->trackImage = m_bAlignCooked;
  mCookP->targetDose = m_iDose;
  mCookP->cookAtTilt = m_bTiltForCook;
  mCookP->tiltAngle = m_fTiltAngle;
  mCookP->minutes = m_fTime;
  mCookP->timeInstead = m_iTimeInstead;
  
  if (m_bTrackState)
    RestoreScopeState();
  m_bTrackState = false;

	CBaseDlg::OnOK();
}
void CCookerSetupDlg::OnCookgo()
{
  m_bGoCook = true;
  OnOK();
}

void CCookerSetupDlg::OnCancel() 
{
  if (m_bTrackState)
    RestoreScopeState();
  m_bTrackState = false;
  CBaseDlg::OnCancel();
}

void CCookerSetupDlg::OnSetTrackState()
{
  UpdateData(true);
  if (m_bTrackState)
    SetScopeState();
  else
    RestoreScopeState();
}

void CCookerSetupDlg::OnAligncooked()
{
  UpdateData(true);
}

void CCookerSetupDlg::OnDeltaposSpincookdose(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int delta = pNMUpDown->iDelta;
  UpdateData(true);
  if (delta > 0 && m_iDose >= TARGET_MAX || delta < 0 && m_iDose <= TARGET_MIN) {
    *pResult = 1;
    return;
  }
  m_iDose += delta * TARGET_INC;
  if (m_iDose > TARGET_MAX)
    m_iDose = TARGET_MAX;
  if (m_iDose < TARGET_MIN)
    m_iDose = TARGET_MIN;
  UpdateData(false);
  UpdateDoseTime();
  *pResult = 0;
}

void CCookerSetupDlg::OnKillfocusEditcookdose()
{
  UpdateData(true);
  m_sbcDose.SetPos(m_iDose);
  UpdateDoseTime();
}

void CCookerSetupDlg::OnDeltaposSpincooktime(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int delta = pNMUpDown->iDelta;
  UpdateData(true);
  if (delta > 0 && m_fTime >= TIME_MAX || delta < 0 && m_fTime <= TIME_MIN) {
    *pResult = 1;
    return;
  }
  m_fTime += delta;
  if (m_fTime > TIME_MAX)
    m_fTime = TIME_MAX;
  if (m_fTime < TIME_MIN)
    m_fTime = TIME_MIN;
  UpdateData(false);
  UpdateDoseTime();
  *pResult = 0;
}

void CCookerSetupDlg::OnKillfocusEditcooktime()
{
  UpdateData(true);
  UpdateDoseTime();
}


void CCookerSetupDlg::OnRadiodose()
{
  UpdateData(true);
  m_editCookDose.EnableWindow(m_iTimeInstead == 0);
  m_editCookTime.EnableWindow(m_iTimeInstead != 0);
  m_sbcDose.EnableWindow(m_iTimeInstead == 0);
  m_sbcCookTime.EnableWindow(m_iTimeInstead != 0);
  UpdateDoseTime();
}

void CCookerSetupDlg::OnTiltforcook()
{
  UpdateData(true);
  m_editCookTilt.EnableWindow(m_bTiltForCook);
}

void CCookerSetupDlg::OnKillfocusEditcooktilt()
{
  UpdateData(true);
}

void CCookerSetupDlg::LiveUpdate(int magInd, int spotSize, double intensity)
{
  if (!m_bTrackState)
    return;
  mCurMagInd = magInd;
  mCurSpot = spotSize;
  mCurIntensity = intensity;
  UpdateMagBeam();
}

void CCookerSetupDlg::UpdateDoseTime(void)
{
  double dose = mWinApp->mBeamAssessor->GetElectronDose(mCurSpot, mCurIntensity, 1.);
  if (dose) {
    if (dose < 1.)
      m_strDoseRate.Format("%.2f", dose);
    else
      m_strDoseRate.Format("%.1f", dose);
    if (m_iTimeInstead) {
      double total = m_fTime * dose * 60.;
      if (total < 100.)
        m_strTotalTime.Format("%.1f e-/A2", total);
      else
        m_strTotalTime.Format("%.0f e-/A2", total);
    } else {
      double time = m_iDose / (dose * 60.);
      if (time < 100.)
        m_strTotalTime.Format("%.1f min", time);
      else
        m_strTotalTime.Format("%.0f min", time);
    }
  } else {
    m_strDoseRate = "--x--";
    m_strTotalTime = "--x--";
  }
  m_strDoseTime = m_iTimeInstead ? "Total dose:" : "Total time:";
  m_butGoCook.EnableWindow((dose > 0. || m_iTimeInstead) && !mDisableGo);
  UpdateData(false);
}


void CCookerSetupDlg::UpdateMagBeam(void)
{
  MagTable *magTab = mWinApp->GetMagTable();
  m_strMag.Format("%d", magTab[mCurMagInd].screenMag);
  m_strSpot.Format("%d", mCurSpot);
  m_strIntensity.Format("%.2f%s %s", mScope->GetC2Percent(mCurSpot, mCurIntensity),
    mScope->GetC2Units(), mScope->GetC2Name());
  UpdateDoseTime();
}

void CCookerSetupDlg::SetScopeState(void)
{
  mSavedIntensity = mScope->GetIntensity();
  mSavedMagInd = mScope->GetMagIndex();
  mSavedSpot = mScope->GetSpotSize();
  mScope->SetSpotSize(mCurSpot);
  mScope->SetMagIndex(mCurMagInd);
  mScope->DelayedSetIntensity(mCurIntensity, GetTickCount(), mCurSpot);
}

void CCookerSetupDlg::RestoreScopeState(void)
{
  mScope->SetSpotSize(mSavedSpot);
  mScope->SetMagIndex(mSavedMagInd);
  mScope->DelayedSetIntensity(mSavedIntensity, GetTickCount(), mSavedSpot);
}
