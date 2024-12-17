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
#include "VPPConditionSetup.h"
#include "CameraController.h"
#include "ComplexTasks.h"

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
  , m_strEMmode(_T("EFTEM"))
  , m_iSpecVsCamDose(FALSE)
  , m_strProbeAlf(_T(""))
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CScopeStatusDlg)
  m_strDefocus = _T("");
  m_strImageShift = _T("");
  m_strMag = _T("");
  //m_strCurrent = _T("");
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
  mPendingCamLen = -1;
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
  mBeamAlpha = -999;
  mShowIntensityCal = true;
  mTEMnanoProbe = false;
  mEMmode = -1;
  mEMmodeChanged = false;
  // Made the base bigger to reduce noise at low readings
  mCurrentLogBase = 0.02f;
  mFloatingMeterSmoothed = 0;
  mSmootherThreshold1 = 0.007f;
  mSmootherThreshold2 = 0.0035f;
  mMeterPlace.rcNormalPosition.right = NO_PLACEMENT;
  mDosePlace.rcNormalPosition.right = NO_PLACEMENT;
  mFullCumDose = 0.;
  mWatchDose = false;
  mEnabledSpecCam = true;
  mCamDoseRate = -1.;
  mLastSpecCamDose = -1;
}


void CScopeStatusDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CScopeStatusDlg)
  DDX_Control(pDX, IDC_STATXLM, m_statXLM);
  //DDX_Control(pDX, IDC_STATNANO, m_statNano);
  DDX_Control(pDX, IDC_BUTDOSE, m_butDose);
  DDX_Control(pDX, IDC_STAT_UMMM, m_statUmMm);
  DDX_Control(pDX, IDC_STATVACUUM, m_statVacuum);
  DDX_Control(pDX, IDC_SPOTSIZE, m_statSpotSize);
  DDX_Control(pDX, IDC_OBJECTIVE, m_statObjective);
  DDX_Control(pDX, IDC_BUTFLOAT, m_butFloat);
  //DDX_Control(pDX, IDC_SCREEN_CURRENT, m_statCurrent);
  DDX_Control(pDX, IDC_MAGNIFICATION, m_statMag);
  DDX_Control(pDX, IDC_IMAGE_SHIFT, m_statImageShift);
  DDX_Control(pDX, IDC_DEFOCUS, m_statDefocus);
  DDX_Text(pDX, IDC_DEFOCUS, m_strDefocus);
  DDX_Text(pDX, IDC_IMAGE_SHIFT, m_strImageShift);
  DDX_Text(pDX, IDC_MAGNIFICATION, m_strMag);
  //DDX_Text(pDX, IDC_SCREEN_CURRENT, m_strCurrent);
  DDX_Text(pDX, IDC_OBJECTIVE, m_strObjective);
  DDX_Text(pDX, IDC_SPOTSIZE, m_strSpotSize);
  DDX_Text(pDX, IDC_STAT_UMMM, m_strUmMm);
  DDX_Text(pDX, IDC_STATVACUUM, m_strVacuum);
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
  DDX_Control(pDX, IDC_STAT_EM_MODE, m_statEMmode);
  DDX_Text(pDX, IDC_STAT_EM_MODE, m_strEMmode);
  DDX_Radio(pDX, IDC_RSPEC_DOSE, m_iSpecVsCamDose);
  DDX_Text(pDX, IDC_STAT_PROBEALF, m_strProbeAlf);
  DDX_Control(pDX, IDC_STAT_PROBEALF, m_statProbeAlf);
}


BEGIN_MESSAGE_MAP(CScopeStatusDlg, CToolDlg)
  //{{AFX_MSG_MAP(CScopeStatusDlg)
  ON_BN_CLICKED(IDC_BUTFLOAT, OnButfloat)
  ON_WM_PAINT()
	ON_WM_CANCELMODE()
	ON_BN_CLICKED(IDC_BUTDOSE, OnButdose)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BUTRESETDEFOCUS, OnResetDefocus)
  ON_BN_CLICKED(IDC_RSPEC_DOSE, OnRspecVsCamDose)
  ON_BN_CLICKED(IDC_RCAM_DOSE, OnRspecVsCamDose)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScopeStatusDlg message handlers

BOOL CScopeStatusDlg::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  HitachiParams *hitachi = mWinApp->GetHitachiParams();
  int medFontIDs[] = {IDC_STAT_DEFLABEL, IDC_STATISORPS, IDC_STAT_IS_UM, IDC_STATXLM,
    IDC_STAT_XLABEL, IDC_STAT_YLABEL, IDC_STAT_ZLABEL, IDC_STAT_OBJLABEL, IDC_STAT_UMMM,
    IDC_STAT_UBPIX, IDC_STAT_C23IA, 0};
  int ind = 0;

  // Get the fonts for big and medium windows
  CRect rect;
  m_statMag.GetWindowRect(&rect);
  mBigFont.CreateFont(B3DNINT(0.95 * rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  m_statObjective.GetWindowRect(&rect);
  mMedFont.CreateFont(B3DNINT(0.98 * rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, mBigFontName);
  mProbeFont.CreateFont(B3DNINT(0.8 * rect.Height()), 0, 0, 0, FW_MEDIUM,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
    FF_DONTCARE, mBigFontName);
  
  // Set the fonts
  //m_statCurrent.SetFont(&mMedFont);
  m_statEMmode.SetFont(&mBigFont);
  m_statMag.SetFont(&mBigFont);
  m_statProbeAlf.SetFont(&mBigFont);
  m_statDefocus.SetFont(&mBigFont);
  m_statImageShift.SetFont(&mBigFont);
  m_statStageX.SetFont(&mMedFont);
  m_statStageY.SetFont(&mMedFont);
  m_statStageZ.SetFont(&mMedFont);
  m_statC23IA.SetWindowText((LPCTSTR)mWinApp->mScope->GetC2Name());
  m_statPctUm.SetWindowText((LPCTSTR)mWinApp->mScope->GetC2Units());
  m_statIntensity.SetFont(&mBigFont);
  m_statDoseRate.SetFont(&mMedFont);
  m_statObjective.SetFont(&mMedFont);
  m_statSpotSize.SetFont(&mBigFont);
  m_butFloat.SetFont(mLittleFont);
  m_butDose.SetFont(mLittleFont);
  while (medFontIDs[ind] > 0) {
    CWnd *win = GetDlgItem(medFontIDs[ind++]);
    if (win)
      win->SetFont(&mMedFont);
  }
  m_statDoseRate.ShowWindow(SW_HIDE);
  m_statUbpix.ShowWindow(SW_HIDE);
  ShowDlgItem(IDC_RSPEC_DOSE, mWinApp->GetAnyDirectDetectors());
  ShowDlgItem(IDC_RCAM_DOSE, mWinApp->GetAnyDirectDetectors());
  m_butResetDef.SetFont(mLittleFont);
  if (JEOLscope || HitachiScope) {
    if (mWinApp->mScope->GetUsePLforIS()) {
      SetDlgItemText(IDC_STATISORPS, "PLA");
      SEMTrace('1',"Setting label to PLA");
    }
  } 
  if (mWinApp->mComplexTasks->GetHitachiWithoutZ()) {
    m_statStageZ.ShowWindow(SW_HIDE);
    m_statZlabel.ShowWindow(SW_HIDE);
  }
  if (HitachiScope && !hitachi->screenAreaSqCm) {
    //m_statCurrent.ShowWindow(SW_HIDE);
    m_butFloat.ShowWindow(SW_HIDE);
    //m_statNano.ShowWindow(SW_HIDE);
  }
 
  mCurrentSmoother.Setup(5, mSmootherThreshold1, mSmootherThreshold2);
  mInitialized = true;

  // Intensity status will draw the Spot text itself so this needs to hide.
  // Start with status -2 so it doesn't try to evaluate intensities until the real update
  if (mShowIntensityCal)
    m_statSpotLabel.ShowWindow(SW_HIDE);
  m_statProbeAlf.ShowWindow(FEIscope || (JEOLscope && !mWinApp->mScope->GetHasNoAlpha()) ?
    SW_SHOW : SW_HIDE);
  mShowVacInEMmode = false;
  if (mShowVacInEMmode)
    m_statEMmode.ShowWindow(SW_HIDE);
  Update(0., 1, 0., 0., 0., 0., 0., 0., true, false, false, false, 0, 1, 0., 0., 0., -1,
    0., 1, -1, -999);
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
                             int gunState, int inAlpha)
{
  double curTime = GetTickCount();
  double newDose, diffTime, pixel, camDose, dose = 0.;
  CString strCurrent, format;
  CString umMm = _T("um");
  CString naSm = smallScreen ? "nA-fs" : "nA";
  double screenCurrent = inCurrent;
  int camera = mWinApp->GetCurrentCamera();
  int pendingSpot = -1, pendingMag = -1, pendingCamLen = -1, numRegCamLens, numLADCamLens;
  int EMmode = 0;
  const char *modeNames[] = {"TEM", "EFTEM", "STEM", "DIFF"};
  bool needDraw = false, showPending = false, magChanged = false, haveCamDose;
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  int *camLenTab = mWinApp->GetCamLenTable();
  BOOL noScope = mWinApp->mScope->GetNoScope();
  bool changed = inDefocus != mDefocus || inISX != mISX ||
    inISY != mISY || fabs(inStageX - mStageX) > 0.1 || fabs(inStageY - mStageY) > 0.1 ||
    fabs(inStageZ - mStageZ) > 0.1 || inSpot != mSpot ||
    rawIntensity != mRawIntensity || inObjective != mObjective || inVacStatus != mVacStatus ||
    inAlpha != mBeamAlpha;
  if (!mInitialized)
    return;

  // Set probe mode and scope mode
  int temNano = B3DCHOICE(!STEM && inProbeMode == 0, 1, 0);
  if (STEM)
    EMmode = 2;
  else if (!inMagInd)
    EMmode = 3;
  else if (EFTEM)
    EMmode = 1;
  if (mEMmode != EMmode) {
    m_strEMmode = modeNames[EMmode];
    m_statEMmode.SetWindowText(modeNames[EMmode]);
    mEMmode = EMmode;
    mEMmodeChanged = true;
    needDraw = true;
  }
  if (mWinApp->GetShowRemoteControl())
    mWinApp->mRemoteControl.GetPendingMagOrSpot(pendingMag, pendingSpot, pendingCamLen);

  // Switch to pending mag and spot
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

  // Update align-focus window with select items
  if (!noScope && mIntCalStatus > -2 && (inMagInd != mMagInd || inAlpha != mBeamAlpha || 
    temNano != (mTEMnanoProbe ? 1 : 0)))
    mWinApp->mAlignFocusWindow.UpdateAutofocus(inMagInd);
  if (!noScope && mIntCalStatus > -2 && (inMagInd != mMagInd ||
    temNano != (mTEMnanoProbe ? 1 : 0) || mEMmodeChanged))
    mWinApp->mAlignFocusWindow.UpdateEucenFocus(inMagInd, temNano);

  // Maintain PLA/IS/CLA for JEOL
  if (STEM != mSTEM && JEOLscope) {
    SetDlgItemText(IDC_STATISORPS, B3DCHOICE(STEM, "CLA",
      mWinApp->mScope->GetUsePLforIS() ? "PLA" : "IS"));
    mWinApp->mAlignFocusWindow.SetDlgItemText(IDC_BUTRESETSHIFT, B3DCHOICE(STEM, 
      "Reset CLA", mWinApp->mScope->GetUsePLforIS() ? "Reset PLA" : "Reset IS"));
  }

  // Handle mag
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
        changed = true;
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
        magChanged = mMagInd != inMagInd;
        mMagInd = inMagInd;
        mSTEM = STEM;
        mPendingMag = pendingMag;
    }

  } else if (!noScope) {
    if (cameraLength != mCamLength || pendingCamLen != mPendingCamLen) {
      if (pendingCamLen > 0) {
        mWinApp->mScope->GetNumCameraLengths(numRegCamLens, numLADCamLens);
        if (pendingCamLen < MAX_CAMLENS && camLenTab[pendingCamLen] > 0) {
          cameraLength = camLenTab[pendingCamLen] / 1000.;
          showPending = true;
        }
      }
      if (cameraLength < 10.) {
        m_strMag.Format("%s%.0f", showPending ? "->" : "D ",  
          cameraLength * (JEOLscope ? 100. : 1000.));
        m_statXLM.SetWindowText(JEOLscope ? "cm" : "mm");
      } else {
        m_strMag.Format("%s%.1f", showPending ? "->" : "D ", cameraLength);
        m_statXLM.SetWindowText("m");
      }
      m_statMag.SetWindowText(m_strMag);
      mCamLength = cameraLength;
      mPendingCamLen = pendingCamLen;
    }
    mMag = 0;
    magChanged = mMagInd != 0;
    mMagInd = 0;
    inISX = 0.;
    inISY = 0.;
  }
  if (noScope)
    return;

  /*if (smallScreen && !mSmallScreen || !smallScreen && mSmallScreen)
    m_statNano.SetWindowText(naSm);*/
  mSmallScreen = smallScreen;

  // Set smoothed current in screen meter
  inCurrent = exp(mCurrentSmoother.Readout(log(inCurrent + mCurrentLogBase))) -
    mCurrentLogBase;
  if (inCurrent < 0 && inCurrent > -1.e-4)
    inCurrent = 0;
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
    //m_statCurrent.SetWindowText(m_strCurrent);
    mCurrent = inCurrent;
    if (mScreenMeter && mScreenMeter->m_bSmoothed) {
      mScreenMeter->m_strCurrent = m_strCurrent + " " + naSm;
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
    format.Format("%%.%df %s", ndec, (LPCTSTR)naSm);
    mScreenMeter->m_strCurrent.Format(format, screenCurrent);
    mScreenMeter->m_statCurrent.SetWindowText(mScreenMeter->m_strCurrent);
    mMeterCurrent = screenCurrent;
  }

  // Intensity
  if (inSpot != mSpot || rawIntensity != mRawIntensity) {
    m_strIntensity.Format("%.2f", inIntensity);
    m_statIntensity.SetWindowText(m_strIntensity);
  }

  // Update others
  if (mWinApp->mCookerDlg && (!mLastCooker || magChanged || 
    rawIntensity != mRawIntensity || inSpot != mSpot))
    mWinApp->mCookerDlg->LiveUpdate(inMagInd, inSpot, rawIntensity);
  mLastCooker = mWinApp->mCookerDlg != NULL;

  if (mWinApp->mAutocenDlg && (!mLastAutocen || magChanged ||
    rawIntensity != mRawIntensity || inSpot != mSpot || 
    temNano != (mTEMnanoProbe ? 1 : 0)))
    mWinApp->mAutocenDlg->LiveUpdate(inMagInd, inSpot, inProbeMode, rawIntensity);

  if (mWinApp->mVPPConditionSetup)
    mWinApp->mVPPConditionSetup->LiveUpdate(inMagInd, inSpot, inProbeMode, rawIntensity,
      inAlpha);
  mRawIntensity = rawIntensity;
  mLastAutocen = mWinApp->mAutocenDlg != NULL;

  // Focus
  if (inDefocus != mDefocus) {
    if (fabs(inDefocus) < 99.98)
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

  // Dose
  if (inMagInd > 0)
    dose = mWinApp->mBeamAssessor->GetElectronDose(inSpot, rawIntensity, 1.);
  if (B3DCHOICE(dose > 0., 1, 0) != B3DCHOICE(mShowedDose, 1, 0)) {
    mShowedDose = dose > 0.;
    m_statUbpix.ShowWindow(dose > 0. ? SW_SHOW : SW_HIDE);
    m_statDoseRate.ShowWindow(dose > 0. ? SW_SHOW : SW_HIDE);
  }

  haveCamDose = mWinApp->mCamera->IsDirectDetector(camParam) &&
    camParam->specToCamDoseFac > 0. && dose > 0;
  if (!BOOL_EQUIV(haveCamDose, mEnabledSpecCam)) {
    EnableDlgItem(IDC_RSPEC_DOSE, haveCamDose);
    EnableDlgItem(IDC_RCAM_DOSE, haveCamDose);
  }
  bool showCamDose = haveCamDose && m_iSpecVsCamDose > 0;
  bool switchCamDose = !BOOL_EQUIV(showCamDose, mEnabledSpecCam && mLastSpecCamDose);
  if (switchCamDose)
    m_statUbpix.SetWindowText(showCamDose ? "e/pix/s at camera" :
      "e/A2/s at specimen");

  if (mShowedDose) {
    if (showCamDose) {
      pixel = 10000. * mWinApp->mShiftManager->GetPixelSize(camera, inMagInd) *
        BinDivisorF(camParam);
      camDose = dose * pixel * pixel * camParam->specToCamDoseFac;
      if (fabs(camDose - mCamDoseRate) > 1.e-6 || switchCamDose) {
        m_strDoseRate.Format("%.2f", camDose);
        m_statDoseRate.SetWindowText(m_strDoseRate);
        mCamDoseRate = camDose;
      }

    } else if (fabs(dose - mDoseRate) > 1.e-6 || switchCamDose) {
      m_strDoseRate.Format("%.2f", dose);
      m_statDoseRate.SetWindowText(m_strDoseRate);
      mDoseRate = dose;
    }
  }
  mEnabledSpecCam = haveCamDose;
  mLastSpecCamDose = m_iSpecVsCamDose;

  // Stage
  if (B3DNINT(inStageX * 100.) != B3DNINT(mStageX * 100.)) {
    format.Format("%.2f", inStageX);
    m_statStageX.SetWindowText(format);
    mStageX = inStageX;
  }
  if (B3DNINT(inStageY * 100.) != B3DNINT(mStageY * 100.)) {
    format.Format("%.2f", inStageY);
    m_statStageY.SetWindowText(format);
    mStageY = inStageY;
  }
  if (B3DNINT(inStageZ * 100.) != B3DNINT(mStageZ * 100.)) {
    format.Format("%.2f", inStageZ);
    m_statStageZ.SetWindowText(format);
    mStageZ = inStageZ;
  }

  // Objective
  if (inObjective != mObjective) {
    m_strObjective.Format("%.2f%%", inObjective);
    m_statObjective.SetWindowText(m_strObjective);
    mObjective = inObjective;
  }

  // Spot
  if (inSpot != mSpot || pendingSpot != mPendingSpot) {
    m_strSpotSize.Format("%s%s%d", pendingSpot > 0 && inSpot < 10 ? "-" : "", 
      pendingSpot > 0 ? ">" : " ", inSpot);
    m_statSpotSize.SetWindowText(m_strSpotSize);
    mSpot = inSpot;
    mPendingSpot = pendingSpot;
  }
  // Alpha or probe
  if (inAlpha >= 0 && inAlpha != mBeamAlpha) {
    m_strProbeAlf.Format("a%d", inAlpha + 1);
    m_statProbeAlf.SetWindowText(m_strProbeAlf);
    mBeamAlpha = inAlpha;
  } else if (FEIscope && (mEMmodeChanged || temNano != (mTEMnanoProbe ? 0 : 1))) {
    m_strProbeAlf = "";
    if (EMmode < 2)
      m_strProbeAlf = temNano ? "nP" : "uP";
    m_statProbeAlf.SetWindowText(m_strProbeAlf);
  }

  // See if vacuum or intensity cal status have changed and cause a repaint
  if (inVacStatus != mVacStatus) {
    mVacStatus = inVacStatus;
    needDraw = true;
  }
  if (mShowIntensityCal && mIntCalStatus >= -1) {
    int error, junk;
    if (mWinApp->GetSTEMMode()) {
      error = 4;
    } else {
      if (!inProbeMode && mWinApp->mScope->GetConstantBrightInNano())
        error = 0;
      else
        error = mWinApp->mBeamAssessor->OutOfCalibratedRange(rawIntensity, inSpot,
        inProbeMode, junk, true);
      if (error) {
        if (error == BEAM_ENDING_OUT_OF_RANGE)
          error = 1;
        else if (error == BEAM_STARTING_OUT_OF_RANGE)
          error = 2;
        else
          error = 3;
      }
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
    changed = true;
  }
  if (needDraw)
    Invalidate();

  // Accuemulate dose
  if (!screenUp && !blanked && gunState != 0 && WatchingDose()) {
    int area;
    BOOL lowDose = mWinApp->LowDoseMode();
    if (lowDose)
      area = mWinApp->mScope->GetLowDoseArea();
    if (!lowDose || !(area == FOCUS_CONSET || area == TRIAL_CONSET)) {
      diffTime = SEMTickInterval(curTime, mLastDoseTime);
      newDose = mWinApp->mBeamAssessor->GetElectronDose(inSpot, rawIntensity,
        (float)(diffTime / 1000.));
      if (newDose)
        AddToDose(newDose);
    }
  }
  mLastDoseTime = curTime;
  if (changed)
    mWinApp->SetLastActivityTime(curTime);
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
  int border;
  CRect winRect, dcRect;
  COLORREF vacColors[3] = {RGB(0,255,0), RGB(255, 255, 0), RGB(255, 0, 0)};
  COLORREF intColors[5] = {RGB(0,192,255), RGB(128, 255, 255), RGB(255,128,0),  
    RGB(255, 0, 255), RGB(212, 208, 200)};
  CPaintDC dc(this); // device context for painting
  DrawSideBorders(dc);
  if (mVacStatus < 0 && mIntCalStatus < 0 && !mShowVacInEMmode) {
    return;
  }

  border = TopOffsetForFillingRectangle(winRect);
  if (mIntCalStatus >= 0) {
    FillDialogItemRectangle(dc, winRect, &m_statSpotLabel, border, 
      intColors[mIntCalStatus], dcRect);
    dc.SelectObject(&mProbeFont);
    dc.DrawText("Spot", &dcRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
  }

  if (mVacStatus >= 0 || mShowVacInEMmode) {
    FillDialogItemRectangle(dc, winRect, mShowVacInEMmode ? &m_statEMmode : &m_statVacuum,
      border, mVacStatus >= 0 ? vacColors[mVacStatus] : RGB(210, 210, 210), dcRect);
    dc.SelectObject(&mProbeFont);
    dc.DrawText(mShowVacInEMmode ? m_strEMmode : "VAC", &dcRect, 
      DT_SINGLELINE | DT_CENTER | DT_VCENTER);
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

void CScopeStatusDlg::OnRspecVsCamDose()
{
  UpdateData(true);
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
