// ImageLevelDlg.cpp:     Shows information about and allows control of image
//                           display
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ImageLevelDlg.h"
#include "EMbufferManager.h"
#include "Utilities\KGetOne.h"
#include ".\imageleveldlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CImageLevelDlg dialog


CImageLevelDlg::CImageLevelDlg(CWnd* pParent /*=NULL*/)
  : CToolDlg(CImageLevelDlg::IDD, pParent)
  , m_bScaleBars(FALSE)
  , m_bAntialias(FALSE)
  , m_bInvertCon(FALSE)
  , mBlackSlider(0)
  , mWhiteSlider(255)
  , m_bTiltAxis(FALSE)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CImageLevelDlg)
  mBrightSlider = 0;
  mContrastSlider = 0;
  m_editZoom = _T("");
  m_bFalseColor = FALSE;
  m_strWhite = _T("");
  m_strBlack = _T("");
  m_bCrosshairs = FALSE;
  //}}AFX_DATA_INIT
  mOpenClosed = 1;
}


void CImageLevelDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CImageLevelDlg)
  DDX_Control(pDX, IDC_TRUNCATION, m_butTruncation);
  DDX_Control(pDX, IDC_STATWHITE, m_statWhite);
  DDX_Control(pDX, IDC_AREAFRACTION, m_butAreaFrac);
  DDX_Control(pDX, IDC_AUTOZOOM, m_butFalseColor);
  DDX_Control(pDX, IDC_STATZOOM, m_statZoom);
  DDX_Control(pDX, IDC_STATCON, m_statCon);
  DDX_Control(pDX, IDC_STATBLACK, m_statBlack);
  DDX_Control(pDX, IDC_STATBRI, m_statBri);
  DDX_Control(pDX, IDC_SPINZOOM, m_sbcZoom);
  DDX_Control(pDX, IDC_EDITZOOM, m_eZoom);
  DDX_Control(pDX, IDC_WHITE, m_eWhiteLevel);
  DDX_Control(pDX, IDC_BLACK, m_eBlackLevel);
  DDX_Control(pDX, IDC_CONTRAST, m_scContrast);
  DDX_Control(pDX, IDC_BRIGHTNESS, m_scBright);
  DDX_Slider(pDX, IDC_BRIGHTNESS, mBrightSlider);
  DDX_Slider(pDX, IDC_CONTRAST, mContrastSlider);
  DDX_Text(pDX, IDC_EDITZOOM, m_editZoom);
  DDV_MaxChars(pDX, m_editZoom, 7);
  DDX_Check(pDX, IDC_AUTOZOOM, m_bFalseColor);
  DDX_Check(pDX, IDC_ANTIALIAS, m_bAntialias);
  DDX_Check(pDX, IDC_SCALEBAR, m_bScaleBars);
  DDX_Check(pDX, IDC_CROSSHAIRS, m_bCrosshairs);
  DDX_Text(pDX, IDC_WHITE, m_strWhite);
  DDX_Text(pDX, IDC_BLACK, m_strBlack);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_SCALEBAR, m_butScaleBars);
  DDX_Control(pDX, IDC_CROSSHAIRS, m_butCrosshairs);
  DDX_Control(pDX, IDC_ANTIALIAS, m_butAntialias);
  DDX_Control(pDX, IDC_INVERT_CON, m_butInvertCon);
  DDX_Check(pDX, IDC_INVERT_CON, m_bInvertCon);
  DDX_Control(pDX, IDC_SLIDER_BLACK, m_scBlack);
  DDX_Slider(pDX, IDC_SLIDER_BLACK, mBlackSlider);
  DDX_Control(pDX, IDC_SLIDER_WHITE, m_scWhite);
  DDX_Slider(pDX, IDC_SLIDER_WHITE, mWhiteSlider);
  DDX_Check(pDX, IDC_TILTAXIS, m_bTiltAxis);
}


BEGIN_MESSAGE_MAP(CImageLevelDlg, CToolDlg)
  //{{AFX_MSG_MAP(CImageLevelDlg)
  ON_BN_CLICKED(IDC_TRUNCATION, OnTruncation)
  ON_BN_CLICKED(IDC_AREAFRACTION, OnAreafraction)
  ON_WM_CLOSE()
  ON_EN_KILLFOCUS(IDC_BLACK, OnKillfocusBW)
  ON_WM_HSCROLL()
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINZOOM, OnDeltaposSpinzoom)
  ON_EN_KILLFOCUS(IDC_EDITZOOM, OnKillfocusEditzoom)
  ON_BN_CLICKED(IDC_AUTOZOOM, OnFalseColor)
  ON_EN_KILLFOCUS(IDC_WHITE, OnKillfocusBW)
  //}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_SCALEBAR, OnScalebar)
  ON_BN_CLICKED(IDC_CROSSHAIRS, OnCrosshairs)
  ON_BN_CLICKED(IDC_ANTIALIAS, OnAntialias)
  ON_BN_CLICKED(IDC_INVERT_CON, OnInvertContrast)
  ON_BN_CLICKED(IDC_TILTAXIS, OnTiltaxis)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImageLevelDlg message handlers
BOOL CImageLevelDlg::OnInitDialog() 
{
  CToolDlg::OnInitDialog();

  // Get master values from the App
  mWinApp->GetDisplayTruncation(mPctLo, mPctHi);
  mAreaFrac = mWinApp->GetPctAreaFraction();

  // Set up slider ranges and ticks, and mouse click interval
  m_scContrast.SetTicFreq(32);
  m_scContrast.SetRange(-128, 127, TRUE);
  m_scContrast.SetPageSize(4);
  m_scBright.SetTicFreq(32);
  m_scBright.SetRange(-128, 127, TRUE);
  m_scBright.SetPageSize(4);
  m_scBlack.SetTicFreq(32);
  m_scBlack.SetRange(0, 255, TRUE);
  m_scBlack.SetPageSize(4);
  m_scWhite.SetTicFreq(32);
  m_scWhite.SetRange(0, 255, TRUE);
  m_scWhite.SetPageSize(4);

  // Make the font bigger for the Edit boxes

  CRect rect;
  m_eBlackLevel.GetWindowRect(&rect);
  if (!mFont.m_hObject)
    mFont.CreateFont(rect.Height() - (int)(4 * mWinApp->GetScalingForDPI()), 0, 0, 0,
      FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  m_eBlackLevel.SetFont(&mFont);
  m_eWhiteLevel.SetFont(&mFont);
  m_eZoom.SetFont(&mFont);

    // Set up the spin control and a zoom
  m_sbcZoom.SetRange(0,100);
  m_sbcZoom.SetPos(50);
  mInitialized = true;

  NewZoom(1.0);
  UpdateSettings();

  // Give up the focus
  mWinApp->RestoreViewFocus();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Set items that can be affected by external settings
void CImageLevelDlg::UpdateSettings()
{
  m_bAntialias = (mWinApp->mBufferManager->GetAntialias() != 0);
  m_bScaleBars = (mWinApp->mBufferManager->GetDrawScaleBar() != 0);
  m_bCrosshairs = mWinApp->mBufferManager->GetDrawCrosshairs();
  m_bTiltAxis = mWinApp->mBufferManager->GetDrawTiltAxis();
  if (mInitialized)
    UpdateData(false);
}

void CImageLevelDlg::OnTruncation() 
{
  float oldPctLo = mPctLo;
  float oldPctHi = mPctHi;
  int oldFFTgray = mWinApp->GetBkgdGrayOfFFT();
  float oldFFTtrunc = mWinApp->GetTruncDiamOfFFT();
  KGetOneFloat("Percent of pixels to truncate as black:", mPctLo, 2);
  KGetOneFloat("Percent of pixels to truncate as white:", mPctHi, 2);
  KGetOneFloat("Diameter of central area of FFT to truncate as fraction of image size:",
    oldFFTtrunc, 3);
  KGetOneInt("Mean gray level (0-255) for low frequencies in FFT, or to disable special"
    " FFT scaling:", oldFFTgray);
  if (mPctLo != oldPctLo || mPctHi != oldPctHi || 
    oldFFTgray != mWinApp->GetBkgdGrayOfFFT() ||
    oldFFTtrunc != mWinApp->GetTruncDiamOfFFT()) {
      mWinApp->SetDisplayTruncation(mPctLo, mPctHi);
      mWinApp->SetBkgdGrayOfFFT(oldFFTgray);
      mWinApp->SetTruncDiamOfFFT(oldFFTtrunc);
      AnalyzeImage();
  }
  mWinApp->RestoreViewFocus();
}

void CImageLevelDlg::OnAreafraction() 
{
  float oldAreaFrac = mAreaFrac;
  KGetOneFloat("Fraction of area's linear extent to analyze for white/black:", mAreaFrac, 2);
  if (mAreaFrac != oldAreaFrac)
    mWinApp->SetPctAreaFraction(mAreaFrac);
  AnalyzeImage();
  mWinApp->RestoreViewFocus();
}

// If percent truncation or fraction area has changed, find new stretch and redraw image
void CImageLevelDlg::AnalyzeImage()
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (!imBuf->mImage || !imBuf->mImageScale)
    return;
  imBuf->mImageScale->FindPctStretch(imBuf->mImage, mPctLo, mPctHi, mAreaFrac,
    B3DCHOICE(imBuf->mCaptured == BUFFER_FFT || imBuf->mCaptured == BUFFER_LIVE_FFT, 
    mWinApp->GetBkgdGrayOfFFT(), 0), mWinApp->GetTruncDiamOfFFT());
  mWinApp->mActiveView->DrawImage();

  // Also, set the new limits into the text box
  mBlackLevel = imBuf->mImageScale->GetMinScale();
  mWhiteLevel = imBuf->mImageScale->GetMaxScale();
  mSampleMin = imBuf->mImageScale->GetSampleMin();
  mSampleMax = imBuf->mImageScale->GetSampleMax();
  SetEditBoxes();
}



void CImageLevelDlg::OnClose() 
{
  MessageBox("Got to Close");
  //CDialog::OnClose();
}

// This signal comes through when Enter is pressed
// Set the focus to a checkbox so checkboxes work.  The Process call may be redundant
// since the focus setting generates a killfocus
// The other key to having checkboxes work after editing is for the tab order to take
// focus to a checkbox after tabbing out of an edit, and for a checkbox to be first in
// the tab order
void CImageLevelDlg::OnOK()
{
  m_butAntialias.SetFocus();
  ProcessEditBoxes(); 
}

void CImageLevelDlg::OnKillfocusBW() 
{
  ProcessEditBoxes(); 
}

void CImageLevelDlg::SetEditBoxes()
{
  float sampleDiff = B3DMAX(1.f, mSampleMax - mSampleMin);
  m_strBlack.Format("%.5g", mBlackLevel);
  m_strWhite.Format("%.5g", mWhiteLevel);
  mBlackSlider = B3DNINT(255. * (mBlackLevel - mSampleMin) / sampleDiff);
  B3DCLAMP(mBlackSlider, 0, 255);
  mWhiteSlider = B3DNINT(255. * (mWhiteLevel - mSampleMin) / sampleDiff);
  B3DCLAMP(mWhiteSlider, 0, 255);
  UpdateData(FALSE);
}

void CImageLevelDlg::ProcessEditBoxes()
{
  // White or black level text: If the data validates OKjust update the values,
  //  change the image scale, redraw the image, pass the focus on
  UpdateData(TRUE);
  mBlackLevel = (float)atof((LPCTSTR)m_strBlack);
  mWhiteLevel = (float)atof((LPCTSTR)m_strWhite);
  ProcessNewBlackWhite();
  if (mWinApp->mActiveView)
    mWinApp->RestoreViewFocus();
}


void CImageLevelDlg::ProcessNewBlackWhite(void)
{
  B3DCLAMP(mBlackLevel, -32768.f, 65535.f);
  B3DCLAMP(mWhiteLevel, -32768.f, 65535.f);
  SetEditBoxes();
  if (mWinApp->mActiveView) {
    EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
    if (imBuf->mImageScale) {
      imBuf->mImageScale->SetMinMax(mBlackLevel, mWhiteLevel);
      mWinApp->mActiveView->DrawImage();
    }
  }
}

void CImageLevelDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
  bool takeNint = true;
  UpdateData(TRUE);
  CSliderCtrl *pSlider = (CSliderCtrl *)pScrollBar;
  if (mWinApp->mActiveView) {
    EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
    if (imBuf->mImageScale) {
      if (pSlider == &m_scBright || pSlider == &m_scContrast) {
        imBuf->mImageScale->mBrightness = mBrightSlider;
        imBuf->mImageScale->mContrast = mContrastSlider;
        mWinApp->mActiveView->DrawImage();
      } else {
        if (imBuf->mImage && imBuf->mImage->getType() == kFLOAT)
          takeNint = false;
        if (pSlider == &m_scBlack) {
          mBlackLevel = (float)(mBlackSlider * (mSampleMax - mSampleMin) / 255. +
            mSampleMin);
          mBlackLevel = (float)B3DNINT(mBlackLevel);
        } else {
          mWhiteLevel = (float)(mWhiteSlider * (mSampleMax - mSampleMin) / 255. +
            mSampleMin);
          mWhiteLevel = (float)B3DNINT(mWhiteLevel);
        }
        ProcessNewBlackWhite();
      }
    }
    if (nSBCode != TB_THUMBTRACK)
      mWinApp->RestoreViewFocus();
  }
  CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CImageLevelDlg::OnInvertContrast()
{
  if (mInitialized)
    UpdateData(true);
  if (mWinApp->mActiveView) {
    EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
    if (imBuf->mImageScale) {
      imBuf->mImageScale->mInverted = m_bInvertCon ? 1 : 0;
      mWinApp->mActiveView->DrawImage();
    }
  }
  mWinApp->RestoreViewFocus();
}

// When notified of a new image scale, update the dialog
void CImageLevelDlg::NewImageScale(KImageScale *inImageScale)
{
  // There are a lot of repeated calls here sometimes... so ignore them
  if (mBrightSlider == inImageScale->mBrightness &&
    mContrastSlider == inImageScale->mContrast && 
    mBlackLevel == inImageScale->GetMinScale() &&
    mWhiteLevel == inImageScale->GetMaxScale() &&
    mSampleMin == inImageScale->GetSampleMin() &&
    mSampleMax == inImageScale->GetSampleMax() &&
    (m_bInvertCon ? 1 : 0) == (inImageScale->GetInverted() ? 1 : 0) &&
    BOOL_EQUIV(m_bFalseColor, inImageScale->GetFalseColor() != 0))
      return;
  mBrightSlider = inImageScale->mBrightness;
  mContrastSlider = inImageScale->mContrast;
  mBlackLevel = inImageScale->GetMinScale();
  mWhiteLevel = inImageScale->GetMaxScale();
  mSampleMin = inImageScale->GetSampleMin();
  mSampleMax = inImageScale->GetSampleMax();
  m_bInvertCon = inImageScale->GetInverted() != 0;
  m_bFalseColor = inImageScale->GetFalseColor() != 0;
  if (mInitialized)
    SetEditBoxes();
}

// External call to invert contrast with a hot key
void CImageLevelDlg::ToggleInvertContrast(void)
{
  m_bInvertCon = !m_bInvertCon;
  if (mInitialized)
    UpdateData(false);
  OnInvertContrast();
}

// External call to toggle false color with a hot key
void CImageLevelDlg::ToggleFalseColor(void)
{
  m_bFalseColor = !m_bFalseColor;
  if (mInitialized)
    UpdateData(false);
  OnFalseColor();

}

// The spin control for zoom
void CImageLevelDlg::OnDeltaposSpinzoom(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  if (pNMUpDown->iDelta > 0)
    mWinApp->mActiveView->ZoomUp();
  else
    mWinApp->mActiveView->ZoomDown();
  mWinApp->RestoreViewFocus();
  *pResult = 1; // Better forbid the change to keep it from running out
}

// The edit window for zoom
void CImageLevelDlg::OnKillfocusEditzoom() 
{
  UpdateData(TRUE);
  double fVal = atof((LPCTSTR)m_editZoom);
  mWinApp->mActiveView->SetZoom(fVal);
  mWinApp->RestoreViewFocus();
}

// When notified of a new zoom, set into text box
void CImageLevelDlg::NewZoom(double inZoom)
{
  char format[6];
  int zoom = (int)floor((inZoom >= 0.1 ? 100. : 1000.) * inZoom + 0.5);
  int ndec = inZoom >= 0.1 ? 2 : 3;
  if (!mInitialized)
    return;
  while (zoom > 0 && zoom % 10 == 0 && ndec > 0) {
    zoom /= 10;
    ndec--;
  }
  sprintf(format, "%%.%df", ndec);
  m_editZoom.Format(format, inZoom);
  UpdateData(FALSE);
}

void CImageLevelDlg::OnFalseColor() 
{
  if (mInitialized)
    UpdateData(true);
  if (mWinApp->mActiveView) {
    EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
    if (imBuf->mImageScale) {
      imBuf->mImageScale->mFalseColor = m_bFalseColor ? 1 : 0;
      mWinApp->mActiveView->DrawImage();
    }
  }
  mWinApp->RestoreViewFocus();
}

void CImageLevelDlg::OnScalebar()
{
  UpdateData(true);
  mWinApp->mBufferManager->SetDrawScaleBar(m_bScaleBars ? 192 : 0);
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
  mWinApp->RestoreViewFocus();
}

void CImageLevelDlg::ToggleExtraInfo(void)
{
  mWinApp->mBufferManager->SetDrawScaleBar(mWinApp->mBufferManager->GetDrawScaleBar() ? 
    0 : 192);
  UpdateSettings();
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
}

void CImageLevelDlg::OnCrosshairs()
{
  UpdateData(true);
  mWinApp->mBufferManager->SetDrawCrosshairs(m_bCrosshairs); 
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
  mWinApp->RestoreViewFocus();
}

void CImageLevelDlg::ToggleCrosshairs()
{
  mWinApp->mBufferManager->SetDrawCrosshairs(
    !mWinApp->mBufferManager->GetDrawCrosshairs());
  UpdateSettings();
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
}

void CImageLevelDlg::OnAntialias()
{
  UpdateData(true);
  mWinApp->mBufferManager->SetAntialias(m_bAntialias ? 1 : 0);
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
  mWinApp->RestoreViewFocus();
}


void CImageLevelDlg::OnTiltaxis()
{
  UpdateData(true);
  mWinApp->mBufferManager->SetDrawTiltAxis(m_bTiltAxis);
  if (mWinApp->mActiveView)
    mWinApp->mActiveView->DrawImage();
  mWinApp->RestoreViewFocus();
}
