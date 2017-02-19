// ScopeStatusDlg.cpp:    Shows various critical microscope parameters
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\ScopeStatusDlg.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "CookerSetupDlg.h"
#include "AutocenSetupDlg.h"
#include "CameraController.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScopeStatusDlg dialog


CScopeStatusDlg::CScopeStatusDlg(CWnd* pParent /*=NULL*/)
  : CToolDlg(CScopeStatusDlg::IDD, pParent)
  , m_strIntensity(_T(""))
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CScopeStatusDlg)
  m_strDefocus = _T("");
  m_strImageShift = _T("");
  m_strMag = _T("");
  m_strCurrent = _T("");
  m_strObjective = _T("");
  m_strSpotSize = _T("");
	m_strUmMm = _T("um");
  m_strDoseRate = _T("");
	//}}AFX_DATA_INIT
  mInitialized = false;
  mCurrent = -1.;
  mISX = mISY = 69.;
  mMagInd = -1;
  mMag = 0;
  mPendingMag = -1;
  mPendingSpot = -1;
  mSTEM = -1;
  mCamLength = 0.;
  mCameraIndex = -1;
  mDefocus = 999.;
  mStageX = mStageY = mStageZ = 2000.;
  mDoseRate= -1.;
  mShowedDose = false;
  mScreenMeter = NULL;
  mDoseMeter = NULL;
  mVacStatus = -1;
  mIntCalStatus = -2;
  mShowIntensityCal = true;
  mTEMnanoProbe = false;
  // Made the base bigger to reduce noise at low readings
  mCurrentLogBase = 0.02f;
  mFloatingMeterSmoothed = 0;
  mSmootherThreshold1 = 0.007f;
  mSmootherThreshold2 = 0.0035f;
  mMeterPlace.rcNormalPosition.right = 0;
  mDosePlace.rcNormalPosition.right = 0;
  mFullCumDose = 0.;
  mWatchDose = false;
}


void CScopeStatusDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CScopeStatusDlg)
  DDX_Control(pDX, IDC_STATXLM, m_statXLM);
  DDX_Control(pDX, IDC_STATNANO, m_statNano);
  DDX_Control(pDX, IDC_BUTDOSE, m_butDose);
  DDX_Control(pDX, IDC_STAT_UMMM, m_statUmMm);
  DDX_Control(pDX, IDC_STATVACUUM, m_statVacuum);
  DDX_Control(pDX, IDC_SPOTSIZE, m_statSpotSize);
  DDX_Control(pDX, IDC_OBJECTIVE, m_statObjective);
  DDX_Control(pDX, IDC_BUTFLOAT, m_butFloat);
  DDX_Control(pDX, IDC_SCREEN_CURRENT, m_statCurrent);
  DDX_Control(pDX, IDC_MAGNIFICATION, m_statMag);
  DDX_Control(pDX, IDC_IMAGE_SHIFT, m_statImageShift);
  DDX_Control(pDX, IDC_DEFOCUS, m_statDefocus);
  DDX_Text(pDX, IDC_DEFOCUS, m_strDefocus);
  DDX_Text(pDX, IDC_IMAGE_SHIFT, m_strImageShift);
  DDX_Text(pDX, IDC_MAGNIFICATION, m_strMag);
  DDX_Text(pDX, IDC_SCREEN_CURRENT, m_strCurrent);
  DDX_Text(pDX, IDC_OBJECTIVE, m_strObjective);
  DDX_Text(pDX, IDC_SPOTSIZE, m_strSpotSize);
  DDX_Text(pDX, IDC_STAT_UMMM, m_strUmMm);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_BUTRESETDEFOCUS, m_butResetDef);
  DDX_Control(pDX, IDC_STATSPOTLABEL, m_statSpotLabel);
  DDX_Text(pDX, IDC_STAT_INTENSITY, m_strIntensity);
  DDX_Control(pDX, IDC_STAT_INTENSITY, m_statIntensity);
  DDX_Control(pDX, IDC_STAT_C23IA, m_statC23IA);
  DDX_Control(pDX, IDC_STAT_PCTUM, m_statPctUm);
  DDX_Control(pDX, IDC_STAT_STAGEX, m_statStageX);
  DDX_Control(pDX, IDC_STAT_STAGEY, m_statStageY);
  DDX_Control(pDX, IDC_STAT_STAGEZ, m_statStageZ);
  DDX_Control(pDX, IDC_STAT_ZLABEL, m_statZlabel);
  DDX_Control(pDX, IDC_STAT_UBPIX, m_statUbpix);
  DDX_Text(pDX, IDC_STAT_DOSERATE, m_strDoseRate);
  DDX_Control(pDX, IDC_STAT_DOSERATE, m_statDoseRate);
}


BEGIN_MESSAGE_MAP(CScopeStatusDlg, CToolDlg)
  //{{AFX_MSG_MAP(CScopeStatusDlg)
  ON_BN_CLICKED(IDC_BUTFLOAT, OnButfloat)
  ON_WM_PAINT()
	ON_WM_CANCELMODE()
	ON_BN_CLICKED(IDC_BUTDOSE, OnButdose)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BUTRESETDEFOCUS, OnResetDefocus)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScopeStatusDlg message handlers

BOOL CScopeStatusDlg::OnInitDialog() 
{
  CToolDlg::OnInitDialog();

  // Get the fonts for big and medium windows
  CRect rect;
  m_statCurrent.GetWindowRect(&rect);
  mBigFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  m_statObjective.GetWindowRect(&rect);
  mMedFont.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  
  // Set the fonts
  m_statCurrent.SetFont(&mBigFont);
  m_statMag.SetFont(&mBigFont);
  m_statDefocus.SetFont(&mBigFont);
  m_statImageShift.SetFont(&mMedFont);
  m_statStageX.SetFont(&mMedFont);
  m_statStageY.SetFont(&mMedFont);
  m_statStageZ.SetFont(&mMedFont);
  m_statC23IA.SetWindowText((LPCTSTR)mWinApp->mScope->GetC2Name());
  m_statPctUm.SetWindowText((LPCTSTR)mWinApp->mScope->GetC2Units());
  m_statIntensity.SetFont(&mMedFont);
  m_statDoseRate.SetFont(&mMedFont);
  m_statObjective.SetFont(&mMedFont);
  m_statSpotSize.SetFont(&mBigFont);
  m_butFloat.SetFont(mLittleFont);
  m_butDose.SetFont(mLittleFont);
  m_statDoseRate.ShowWindow(SW_HIDE);
  m_statUbpix.ShowWindow(SW_HIDE);
  if (JEOLscope || HitachiScope) {
    m_butResetDef.SetFont(mLittleFont);
    if (mWinApp->mScope->GetUsePLforIS()) {
      SetDlgItemText(IDC_STATISORPS, "PLA");
      SEMTrace('1',"Setting label to PLA");
    }
  } else
    m_butResetDef.ShowWindow(SW_HIDE);
  if (HitachiScope) {
    m_statStageZ.ShowWindow(SW_HIDE);
    m_statZlabel.ShowWindow(SW_HIDE);
    m_statCurrent.ShowWindow(SW_HIDE);
    m_butFloat.ShowWindow(SW_HIDE);
    m_statNano.ShowWindow(SW_HIDE);
  }
  mCurrentSmoother.Setup(5, mSmootherThreshold1, mSmootherThreshold2);
  mInitialized = true;

  // Intensity status will draw the Spot text itself so this needs to hide.
  // Start with status -2 so it doesn't try to evaluate intensities until the real update
  if (mShowIntensityCal)
    m_statSpotLabel.ShowWindow(SW_HIDE);
  Update(0., 1, 0., 0., 0., 0., 0., 0., true, false, false, false, 0, 1, 0., 0., 0., -1, 
    0., 1, -1);
  mIntCalStatus = -1;
    
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Return height of extra line with dose rate
int CScopeStatusDlg::DoseHeightAdjustment(void)
{
  CRect rect1, rect2;
  m_statZlabel.GetWindowRect(&rect1);
  m_statDoseRate.GetWindowRect(&rect2);
  return rect2.bottom - rect1.bottom;
}

void CScopeStatusDlg::Update(double inCurrent, int inMagInd, double inDefocus, 
                             double inISX, double inISY, double inStageX, double inStageY,
                             double inStageZ, BOOL screenUp, BOOL smallScreen, 
                             BOOL blanked, BOOL EFTEM, int STEM, int inSpot, 
                             double rawIntensity, double inIntensity, double inObjective, 
                             int inVacStatus, double cameraLength, int inProbeMode,
                             int gunState)
{
  double curTime = GetTickCount();
  double newDose, diffTime, dose = 0.;
  CString strCurrent, format;
  CString umMm = _T("um");
  CString naSm = smallScreen ? "nA-fs" : "nA";
  double screenCurrent = inCurrent;
  int camera = mWinApp->GetCurrentCamera();
  float pixel;
  int pendingSpot = -1, pendingMag = -1;
  bool needDraw = false;
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  BOOL noScope = mWinApp->mScope->GetNoScope();
  if (!mInitialized)
    return;

  int temNano = B3DCHOICE(!STEM && inProbeMode == 0, 1, 0);
  if (mWinApp->GetShowRemoteControl())
    mWinApp->mRemoteControl.GetPendingMagOrSpot(pendingMag, pendingSpot);
  if (pendingMag > 0)
    inMagInd = pendingMag;
  if (pendingSpot > 0)
    inSpot = pendingSpot;

  // Update Image shift first since it depends on mag too
  if (!noScope && (inISX != mISX || inISY != mISY || inMagInd != mMagInd || 
    mWinApp->GetCurrentCamera() != mCameraIndex)) {
      double specTotal = 0.;
      if (inMagInd)
        specTotal = mWinApp->mShiftManager->RadialShiftOnSpecimen(inISX, inISY, inMagInd);
      m_strImageShift.Format("%.2f", specTotal);
      m_statImageShift.SetWindowText(m_strImageShift);
      mISX = inISX;
      mISY = inISY;
      mCameraIndex = mWinApp->GetCurrentCamera();
      SEMTrace('i', "Scope status: IS %f %f units,  %.2f um", mISX, mISY, specTotal);
  }
  
  if (inMagInd) {
    MagTable *magTab = mWinApp->GetMagTable();
    int inMag = screenUp ? magTab[inMagInd].mag : magTab[inMagInd].screenMag;
    if (EFTEM)
      inMag = screenUp ? magTab[inMagInd].EFTEMmag : magTab[inMagInd].EFTEMscreenMag;
    if (STEM)
      inMag = B3DNINT(magTab[inMagInd].STEMmag);
    mCamLength = 0.;
    if (inMag != mMag || mMagInd != inMagInd || STEM != mSTEM || 
      pendingMag != mPendingMag) {
        if (STEM) {
          if (inMagInd < mWinApp->mScope->GetLowestSTEMnonLMmag(STEM-1))
            m_statXLM.SetWindowText("SL");
          else {
            bool ambig = false;

            // For FEI, a mag present in STEM LM and nonLM will always be reported as
            // nonLM, so look in LM for the same mag and mark with a ?
            if (FEIscope) {
              int limlo = STEM > 1 ? mWinApp->mScope->GetLowestMicroSTEMmag() : 1;
              for (int m = limlo; m < mWinApp->mScope->GetLowestSTEMnonLMmag(STEM-1); m++)
              {
                if (B3DNINT(magTab[m].STEMmag) == inMag) {
                  ambig = true;
                  break;
                }
              }
            }
            if (ambig)
              m_statXLM.SetWindowText("S?");
            else
              m_statXLM.SetWindowText("ST");
          }
        } else if (inMagInd < mWinApp->mScope->GetLowestMModeMagInd())
          m_statXLM.SetWindowText("LM");
      else if (UtilMagInInvertedRange(inMagInd, EFTEM))
        m_statXLM.SetWindowText("INV");
        else
          m_statXLM.SetWindowText("X");
        if (inMag >= 100000)
          m_strMag.Format("%s%dK", pendingMag > 0 ? "->" : "", inMag / 1000);
        else
          m_strMag.Format("%s%d", pendingMag > 0 ? "->" : "", inMag);
        m_statMag.SetWindowText(m_strMag);
        SEMTrace('i', "Scope status: mag change %d to %d", mMag, inMag);
        mMag = inMag;
        mMagInd = inMagInd;
        mSTEM = STEM;
        mPendingMag = pendingMag;
    }

  } else if (!noScope) {
    if (cameraLength != mCamLength) {
      if (cameraLength < 10.) {
        m_strMag.Format("D %.0f", cameraLength * (JEOLscope ? 100. : 1000.));
        m_statXLM.SetWindowText(JEOLscope ? "cm" : "mm");
      } else {
        m_strMag.Format("D %.1f", cameraLength);
        m_statXLM.SetWindowText("m");
      }
      m_statMag.SetWindowText(m_strMag);
      mCamLength = cameraLength;
    }
    mMag = 0;
    mMagInd = 0;
    inISX = 0.;
    inISY = 0.;
  }
  if (noScope)
    return;

  if (smallScreen && !mSmallScreen || !smallScreen && mSmallScreen)
    m_statNano.SetWindowText(naSm);
  mSmallScreen = smallScreen;

  inCurrent = exp(mCurrentSmoother.Readout(log(inCurrent + mCurrentLogBase))) -
    mCurrentLogBase;
  if (inCurrent != mCurrent) {
    int ndec = 4;
    if (inCurrent > 0.0001)
      ndec = (int)(floor(log10(2000. / inCurrent)));
    if (ndec < 0)
      ndec = 0;
    if (ndec > 4)
      ndec = 4;
    format.Format("%%.%df", ndec);
    m_strCurrent.Format(format, inCurrent);
    m_statCurrent.SetWindowText(m_strCurrent);
    mCurrent = inCurrent;
    if (mScreenMeter && mScreenMeter->m_bSmoothed) {
      mScreenMeter->m_strCurrent = m_strCurrent + " nA";
      mScreenMeter->m_statCurrent.SetWindowText(mScreenMeter->m_strCurrent);
    }
  }

  // If floating meter exists and smoothing is not on and it's changed, display
  if (mScreenMeter  && !mScreenMeter->m_bSmoothed && screenCurrent != mMeterCurrent) {
    int ndec = 4;
    if (screenCurrent > 0.0001)
      ndec = (int)(floor(log10(2000. / screenCurrent)));
    if (ndec < 0)
      ndec = 0;
    if (ndec > 4)
      ndec = 4;
    format.Format("%%.%df nA", ndec);
    mScreenMeter->m_strCurrent.Format(format, screenCurrent);
    mScreenMeter->m_statCurrent.SetWindowText(mScreenMeter->m_strCurrent);
    mMeterCurrent = screenCurrent;
  }

  if (inSpot != mSpot || rawIntensity != mRawIntensity) {
    m_strIntensity.Format("%.2f", inIntensity);
    m_statIntensity.SetWindowText(m_strIntensity);
  }

  if (mWinApp->mCookerDlg && (!mLastCooker || inMagInd != mMagInd || 
    rawIntensity != mRawIntensity || inSpot != mSpot))
    mWinApp->mCookerDlg->LiveUpdate(inMagInd, inSpot, rawIntensity);
  mLastCooker = mWinApp->mCookerDlg != NULL;

  if (mWinApp->mAutocenDlg && (!mLastAutocen || inMagInd != mMagInd || 
    rawIntensity != mRawIntensity || inSpot != mSpot || 
    temNano != (mTEMnanoProbe ? 1 : 0)))
    mWinApp->mAutocenDlg->LiveUpdate(inMagInd, inSpot, inProbeMode, rawIntensity);
  mRawIntensity = rawIntensity;
  mLastAutocen = mWinApp->mAutocenDlg != NULL;

  if (STEM != mSTEM && mWinApp->mScope->GetUsePLforIS()) {
    SetDlgItemText(IDC_STATISORPS, STEM ? "IS" : "PLA");
  }

  if (inDefocus != mDefocus) {
    if (fabs(inDefocus) < 100.)
      m_strDefocus.Format("%.2f", inDefocus);
    else if (fabs(inDefocus) < 1000.)
      m_strDefocus.Format("%.1f", inDefocus);
    else {
      m_strDefocus.Format("%.2f", inDefocus / 1000.);
      umMm = _T("mm");
    }
    if (umMm != m_strUmMm)
      m_statUmMm.SetWindowText(umMm);
    m_strUmMm = umMm;
    m_statDefocus.SetWindowText(m_strDefocus);
    mDefocus = inDefocus;
  }

  if (mWinApp->mCamera->IsDirectDetector(camParam) && inMagInd > 0) {
    dose = mWinApp->mBeamAssessor->GetElectronDose(inSpot, rawIntensity, 1.);
    pixel = 10000.f * (camParam->K2Type ? 2.f : 1.f ) * 
        mWinApp->mShiftManager->GetPixelSize(camera, inMagInd);
    dose *= pixel * pixel;
  }
  if (B3DCHOICE(dose > 0., 1, 0) != B3DCHOICE(mShowedDose, 1, 0)) {
    mShowedDose = dose > 0.;
    m_statUbpix.ShowWindow(dose > 0. ? SW_SHOW : SW_HIDE);
    m_statDoseRate.ShowWindow(dose > 0. ? SW_SHOW : SW_HIDE);
  }

  if (mShowedDose) {
    if (dose != mDoseRate) {
      m_strDoseRate.Format("%.2f", dose);
      m_statDoseRate.SetWindowText(m_strDoseRate);
      mDoseRate = dose;
    }
  }

  if (inStageX != mStageX) {
    format.Format("%.2f", inStageX);
    m_statStageX.SetWindowText(format);
    mStageX = inStageX;
  }
  if (inStageY != mStageY) {
    format.Format("%.2f", inStageY);
    m_statStageY.SetWindowText(format);
    mStageY = inStageY;
  }
  if (inStageZ != mStageZ) {
    format.Format("%.2f", inStageZ);
    m_statStageZ.SetWindowText(format);
    mStageZ = inStageZ;
  }

  if (inObjective != mObjective) {
    m_strObjective.Format("%.2f%%", inObjective);
    m_statObjective.SetWindowText(m_strObjective);
    mObjective = inObjective;
  }
  if (inSpot != mSpot || pendingSpot != mPendingSpot) {
    m_strSpotSize.Format("%s%s%d", pendingSpot > 0 && inSpot < 10 ? "-" : "", 
      pendingSpot > 0 ? ">" : "", inSpot);
    m_statSpotSize.SetWindowText(m_strSpotSize);
    mSpot = inSpot;
    mPendingSpot = pendingSpot;
  }

  // See if vacuum or intensity cal status have changed and cause a repaint
  if (inVacStatus != mVacStatus) {
    mVacStatus = inVacStatus;
    needDraw = true;
  }
  if (mShowIntensityCal && mIntCalStatus >= -1) {
    int error, junk;
    if (mWinApp->GetSTEMMode()) {
      error = 3;
    } else {
      error = mWinApp->mBeamAssessor->OutOfCalibratedRange(rawIntensity, inSpot, 
        inProbeMode, junk);
      if (error)
        error = error == BEAM_STARTING_OUT_OF_RANGE ? 1 : 2;
    }
    if (error != mIntCalStatus) {
      if (error)
        SEMTrace('d', "Beam calibration status change to %d, intensity %f, spot %d, "
        "polarity %d", error, rawIntensity, inSpot, junk);
      mIntCalStatus = error;
      needDraw = true;
    }
  }

  if (temNano != (mTEMnanoProbe ? 1 : 0)) {
    mTEMnanoProbe = temNano != 0;
    needDraw = true;
  }
  if (needDraw)
    Invalidate();

  if (!screenUp && !blanked && gunState != 0 && WatchingDose()) {
    diffTime = SEMTickInterval(curTime, mLastDoseTime);
    newDose = mWinApp->mBeamAssessor->GetElectronDose(inSpot, rawIntensity, 
      (float)(diffTime / 1000.));
    if (newDose)
      AddToDose(newDose);
  }
  mLastDoseTime = curTime;
}

void CScopeStatusDlg::OnButfloat() 
{
  if (mScreenMeter)
    return;
  mScreenMeter = new CScreenMeter();
  mScreenMeter->m_strFontName = mBigFontName;
  mScreenMeter->Create(IDD_SCREENMETER);
  mScreenMeter->m_strCurrent = m_strCurrent + " na";
  mScreenMeter->m_statCurrent.SetWindowText(mScreenMeter->m_strCurrent);
  mWinApp->SetPlacementFixSize(mScreenMeter, &mMeterPlace);
  m_butFloat.EnableWindow(false);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CScopeStatusDlg::MeterClosing() 
{
  mScreenMeter->GetWindowPlacement(&mMeterPlace);
  mScreenMeter = NULL;
  m_butFloat.EnableWindow(true);
}

WINDOWPLACEMENT *CScopeStatusDlg::GetMeterPlacement()
{
  if (mScreenMeter)
    mScreenMeter->GetWindowPlacement(&mMeterPlace);
  return &mMeterPlace;
}

void CScopeStatusDlg::OnPaint() 
{
  int iLeft, iTop, border;
  CRect winRect;
  CRect statRect;
  CRect clientRect;
  COLORREF vacColors[3] = {RGB(0,255,0), RGB(255, 255, 0), RGB(255, 0, 0)};
  COLORREF intColors[4] = {RGB(0,192,255), RGB(255, 128, 0), RGB(255, 0, 255),
    RGB(212, 208, 200)};
  CPaintDC dc(this); // device context for painting
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  border = (winRect.Width() - clientRect.Width()) / 2;

  // Needed to copy essential stuff from CToolDlg for side bars
  int lrSize = 4;
  dc.FillSolidRect(0, 0, lrSize, clientRect.Height(), mBorderColor);
  dc.FillSolidRect(clientRect.Width() - lrSize, 0, clientRect.Width(),
    clientRect.Height(), mBorderColor);
  if (mVacStatus < 0 && mIntCalStatus < 0) {
    return;
  }

  if (mIntCalStatus >= 0) {
    m_statSpotLabel.GetWindowRect(&statRect);
    iLeft = statRect.left - winRect.left;
    iTop = statRect.top - winRect.top - (winRect.Height() - clientRect.Height() - border);
    CRect dcRecti(iLeft, iTop, iLeft + statRect.Width(), iTop + statRect.Height());
    dc.SelectObject(GetFont());
    dc.FillSolidRect(&dcRecti, intColors[mIntCalStatus]);
    dc.DrawText(mTEMnanoProbe ? "nPr" : "Spot", &dcRecti, 
      DT_SINGLELINE | DT_CENTER | DT_VCENTER);
  }

  if (mVacStatus >= 0) {
    m_statVacuum.GetWindowRect(&statRect);
    iLeft = statRect.left - winRect.left;
    iTop = statRect.top - winRect.top - (winRect.Height() - clientRect.Height() - border);
    CRect dcRect(iLeft, iTop, iLeft + statRect.Width(), iTop + statRect.Height());
    dc.FillSolidRect(&dcRect, vacColors[mVacStatus]);
    dc.SelectObject(&mMedFont);
    dc.DrawText("VAC", &dcRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
  }

}

void CScopeStatusDlg::OnResetDefocus()
{
  if (!mWinApp->DoingTasks())
    mWinApp->mScope->ResetDefocus();

  // Gets the dotted line and border focus to the first button
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CScopeStatusDlg::OnButdose() 
{
  if (mDoseMeter)
    return;
  mDoseMeter = new CDoseMeter();
  mDoseMeter->m_strFontName = mBigFontName;
  mDoseMeter->Create(IDD_DOSEMETER);
  mDoseMeter->mCumDose = 0.;
  OutputDose();
  mWinApp->SetPlacementFixSize(mDoseMeter, &mDosePlace);
  m_butDose.EnableWindow(false);
  SetFocus();
  mWinApp->RestoreViewFocus();
}

void CScopeStatusDlg::DoseClosing()
{
  mDoseMeter->GetWindowPlacement(&mDosePlace);
  mDoseMeter = NULL;
  m_butDose.EnableWindow(true);
}

WINDOWPLACEMENT *CScopeStatusDlg::GetDosePlacement()
{
  if (mDoseMeter)
    mDoseMeter->GetWindowPlacement(&mDosePlace);
  return &mDosePlace;
}

void CScopeStatusDlg::OutputDose()
{
  if (!mDoseMeter)
    return;
  int ndec = 3;
  if (mDoseMeter->mCumDose > 0.001)
    ndec = (int)(floor(log10(1000. / mDoseMeter->mCumDose)));
  if (ndec < 0)
    ndec = 0;
  if (ndec > 3)
    ndec = 3;
  CString format;
  format.Format("%%.%df e/sq.A", ndec);
  mDoseMeter->m_strDose.Format(format, mDoseMeter->mCumDose);
  mDoseMeter->m_statDose.SetWindowText(mDoseMeter->m_strDose);
}

void CScopeStatusDlg::AddToDose(double dose)
{
  mFullCumDose += dose;
  if (!mDoseMeter)
    return;
  mDoseMeter->mCumDose += dose;
  OutputDose();
}
