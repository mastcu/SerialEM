// HoleFinderDlg.cpp : Calls HoleFinder to find holes and turns them into Nav points
// There is a resident instance of this class, and the dialog can be created and closed
//
// Copyright (C) 2020 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>
#include "SerialEM.h"
#include "SerialEMView.h"
#include "EMscope.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "EMmontageController.h"
#include "HoleFinderDlg.h"
#include "Shared\holefinder.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"
#include "Shared\b3dutil.h"

// CHoleFinderDlg dialog

CHoleFinderDlg::CHoleFinderDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_HOLE_FINDER, pParent)
  , m_fHolesSize(0.1f)
  , m_fSpacing(0.5f)
  , m_strSigmas(_T(""))
  , m_strIterations(_T(""))
  , m_strThresholds(_T(""))
  , m_bExcludeOutside(FALSE)
  , m_strStatusOutput(_T(""))
  , m_strMinLowerMean(_T(""))
  , m_strMaxLowerMean(_T(""))
  , m_strLowerMean(_T(""))
  , m_intLowerMean(0)
  , m_strMinUpperMean(_T(""))
  , m_strMaxUpperMean(_T(""))
  , m_strUpperMean(_T(""))
  , m_intUpperMean(0)
  , m_strMinSDcutoff(_T(""))
  , m_strMaxSDcutoff(_T(""))
  , m_strSDcutoff(_T(""))
  , m_intSDcutoff(0)
  , m_strMinBlackPct(_T(""))
  , m_strMaxBlackPct(_T(""))
  , m_strBlackPct(_T(""))
  , m_intBlackPct(0)
  , m_bShowIncluded(TRUE)
  , m_bShowExcluded(FALSE)
  , m_iLayoutType(0)
  , m_fMaxError(0.05f)
  , m_bBracketLast(FALSE)
{
  mHaveHoles = false;
  mFindingHoles = false;
  mIsOpen = false;
  mMiniOffsets = NULL;
  mBestThreshInd = -1;
  mLastHoleSize = 0.;
  mLastHoleSpacing = 0.;
}

CHoleFinderDlg::~CHoleFinderDlg()
{
}

void CHoleFinderDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_HOLE_SIZE, m_fHolesSize);
  MinMaxFloat(IDC_EDIT_HOLE_SIZE, m_fHolesSize, 0.1f, 5, "Hole size");
  DDX_Text(pDX, IDC_EDIT_SPACING, m_fSpacing);
  MinMaxFloat(IDC_EDIT_SPACING, m_fSpacing, 0.5f, 10, "Hole spacing");
  DDX_Text(pDX, IDC_EDIT_SIGMAS, m_strSigmas);
  DDX_Text(pDX, IDC_EDIT_MEDIAN_ITERS, m_strIterations);
  DDX_Text(pDX, IDC_EDIT_THRESHOLDS, m_strThresholds);
  DDX_Control(pDX, IDC_EXCLUDE_OUTSIDE, m_butExcludeOutside);
  DDX_Check(pDX, IDC_EXCLUDE_OUTSIDE, m_bExcludeOutside);
  DDX_Control(pDX, IDC_BUT_FIND_HOLES, m_butFindHoles);
  DDX_Control(pDX, IDC_STAT_STATUS_OUTPUT, m_statStatusOutput);
  DDX_Text(pDX, IDC_STAT_STATUS_OUTPUT, m_strStatusOutput);
  DDX_Text(pDX, IDC_STAT_MIN_LOWER_MEAN, m_strMinLowerMean);
  DDX_Text(pDX, IDC_STAT_MAX_LOWER_MEAN, m_strMaxLowerMean);
  DDX_Control(pDX, IDC_SLIDER_LOW_MEAN, m_sliderLowerMean);
  DDX_Control(pDX, IDC_EDIT_LOWER_MEAN, m_editLowerMean);
  DDX_Text(pDX, IDC_EDIT_LOWER_MEAN, m_strLowerMean);
  DDX_Slider(pDX, IDC_SLIDER_LOW_MEAN, m_intLowerMean);
  DDV_MinMaxInt(pDX, m_intLowerMean, 0, 255);
  DDX_Text(pDX, IDC_STAT_MIN_UPPER_MEAN, m_strMinUpperMean);
  DDX_Text(pDX, IDC_STAT_MAX_UPPER_MEAN, m_strMaxUpperMean);
  DDX_Control(pDX, IDC_SLIDER_UPPER_MEAN, m_sliderUpperMean);
  DDX_Control(pDX, IDC_EDIT_UPPER_MEAN, m_editUpperMean);
  DDX_Text(pDX, IDC_EDIT_UPPER_MEAN, m_strUpperMean);
  DDX_Slider(pDX, IDC_SLIDER_UPPER_MEAN, m_intUpperMean);
  DDV_MinMaxInt(pDX, m_intUpperMean, 0, 255);
  DDX_Text(pDX, IDC_STAT_MIN_STANDEV, m_strMinSDcutoff);
  DDX_Text(pDX, IDC_STAT_MAX_STANDEV, m_strMaxSDcutoff);
  DDX_Control(pDX, IDC_SLIDER_STANDEV, m_sliderSDcutoff);
  DDX_Text(pDX, IDC_STAT_SD_CUTOFF, m_strSDcutoff);
  DDX_Slider(pDX, IDC_SLIDER_STANDEV, m_intSDcutoff);
  DDV_MinMaxInt(pDX, m_intSDcutoff, 0, 255);
  DDX_Text(pDX, IDC_STAT_MIN_BLACKPCT, m_strMinBlackPct);
  DDX_Text(pDX, IDC_STAT_MAX_BLACKPCT, m_strMaxBlackPct);
  DDX_Control(pDX, IDC_SLIDER_BLACK_CUTOFF, m_sliderBlackPct);
  DDX_Text(pDX, IDC_STAT_BLACK_CUTOFF, m_strBlackPct);
  DDX_Slider(pDX, IDC_SLIDER_BLACK_CUTOFF, m_intBlackPct);
  DDV_MinMaxInt(pDX, m_intBlackPct, 0, 255);
  DDX_Control(pDX, IDC_SHOW_INCLUDED, m_butShowIncluded);
  DDX_Check(pDX, IDC_SHOW_INCLUDED, m_bShowIncluded);
  DDX_Control(pDX, IDC_SHOW_EXCLUDED, m_butShowExcluded);
  DDX_Check(pDX, IDC_SHOW_EXCLUDED, m_bShowExcluded);
  DDX_Control(pDX, IDC_RZIGZAG, m_butZigzag);
  DDX_Control(pDX, IDC_RFROM_FOCUS, m_butFromFocus);
  DDX_Control(pDX, IDC_RIN_GROUPS, m_butInGroups);
  DDX_Radio(pDX, IDC_RZIGZAG, m_iLayoutType);
  DDX_Control(pDX, IDC_BUT_MAKE_NAV_PTS, m_butMakeNavPts);
  DDX_Text(pDX, IDC_EDIT_MAX_ERROR, m_fMaxError);
  MinMaxFloat(IDC_EDIT_MAX_ERROR, m_fMaxError, 0.001f, 1.f, "Maximum error");
  DDX_Control(pDX, IDC_BUT_CLEAR_DATA, m_butClearData);
  DDX_Control(pDX, IDC_BRACKET_LAST, m_butBracketLast);
  DDX_Check(pDX, IDC_BRACKET_LAST, m_bBracketLast);
  DDX_Control(pDX, IDC_BUT_SET_SIZE_SPACE, m_butSetSizeSpace);
}


BEGIN_MESSAGE_MAP(CHoleFinderDlg, CBaseDlg)
  ON_WM_HSCROLL()
  ON_EN_KILLFOCUS(IDC_EDIT_HOLE_SIZE, OnKillfocusEditHoleSize)
  ON_EN_KILLFOCUS(IDC_EDIT_SPACING, OnKillfocusEditSpacing)
  ON_EN_KILLFOCUS(IDC_EDIT_SIGMAS, OnKillfocusEditSigmas)
  ON_EN_KILLFOCUS(IDC_EDIT_MEDIAN_ITERS, OnKillfocusEditMedianIters)
  ON_EN_KILLFOCUS(IDC_EDIT_THRESHOLDS, OnKillfocusEditThresholds)
  ON_BN_CLICKED(IDC_EXCLUDE_OUTSIDE, OnExcludeOutside)
  ON_BN_CLICKED(IDC_BUT_FIND_HOLES, OnButFindHoles)
  ON_EN_KILLFOCUS(IDC_EDIT_LOWER_MEAN, OnKillfocusEditLowerMean)
  ON_EN_KILLFOCUS(IDC_EDIT_UPPER_MEAN, OnKillfocusEditUpperMean)
  ON_BN_CLICKED(IDC_SHOW_INCLUDED, OnShowIncluded)
  ON_BN_CLICKED(IDC_SHOW_EXCLUDED, OnShowExcluded)
  ON_BN_CLICKED(IDC_RZIGZAG, OnRadioLayoutType)
  ON_BN_CLICKED(IDC_RFROM_FOCUS, OnRadioLayoutType)
  ON_BN_CLICKED(IDC_RIN_GROUPS, OnRadioLayoutType)
  ON_BN_CLICKED(IDC_BUT_MAKE_NAV_PTS, OnButMakeNavPts)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_ERROR, OnKillfocusEditMaxError)
  ON_BN_CLICKED(IDC_BUT_CLEAR_DATA, OnButClearData)
  ON_BN_CLICKED(IDC_BRACKET_LAST, OnBracketLast)
  ON_BN_CLICKED(IDC_BUT_SET_SIZE_SPACE, OnButSetSizeSpace)
END_MESSAGE_MAP()


// CHoleFinderDlg message handlers
BOOL CHoleFinderDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  CheckAndSetNav();
  mIsOpen = true;
  mMasterParams = mHelper->GetHoleFinderParams();
  mParams = *mMasterParams;
  m_sliderLowerMean.SetRange(0, 255);
  m_sliderUpperMean.SetRange(0, 255);
  m_sliderSDcutoff.SetRange(0, 255);
  m_sliderBlackPct.SetRange(0, 255);
  m_sliderLowerMean.SetPageSize(4);
  m_sliderUpperMean.SetPageSize(4);
  m_sliderSDcutoff.SetPageSize(4);
  m_sliderBlackPct.SetPageSize(4);
  ParamsToDialog();
  ManageEnables();
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

void CHoleFinderDlg::OnOK()
{
  DialogToParams();
  *mMasterParams = mParams;
  OnCancel();
}

void CHoleFinderDlg::OnCancel()
{
  mHelper->GetHoleFinderPlacement();
  mIsOpen = false;
  mHaveHoles = false;
  DestroyWindow();
}

// Do not delete the dialog
void CHoleFinderDlg::PostNcDestroy()
{
  CDialog::PostNcDestroy();
}

BOOL CHoleFinderDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

void CHoleFinderDlg::CloseWindow()
{
  if (mIsOpen)
    OnOK();
}

// Handle simple edit boxes
void CHoleFinderDlg::OnKillfocusEditHoleSize()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}


void CHoleFinderDlg::OnKillfocusEditSpacing()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

void CHoleFinderDlg::OnKillfocusEditMaxError()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Handle boxes that can have lists.  Allow commas, replace by spaces
// Filter sigmas
void CHoleFinderDlg::OnKillfocusEditSigmas()
{
  UpdateData(true);
  mWinApp->mParamIO->StringToEntryList(1, m_strSigmas, mNumSigmas,
    NULL, &mSigmas[0], MAX_HOLE_TRIALS, true);
  m_strSigmas = ListToString(1, mNumSigmas, &mSigmas[0]);
  mBestThreshInd = -1;
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

// Iterations for median filter
void CHoleFinderDlg::OnKillfocusEditMedianIters()
{
  int ind, indOut = 1;
  UpdateData(true);
  mWinApp->mParamIO->StringToEntryList(3, m_strIterations, mNumIterations,
    &mIterations[0], NULL, MAX_HOLE_TRIALS, true);
  if (mNumIterations) {
    rsSortInts(&mIterations[0], mNumIterations);
    for (ind = 1; ind < mNumIterations; ind++)
      if (mIterations[ind] != mIterations[ind - 1])
        mIterations[indOut++] = mIterations[ind];
    mNumIterations = indOut;
  }
  m_strIterations = ListToString(3, mNumIterations,  &mIterations[0]);
  mBestThreshInd = -1;
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

// Thresholds
void CHoleFinderDlg::OnKillfocusEditThresholds()
{
  UpdateData(true);
  mWinApp->mParamIO->StringToEntryList(1, m_strThresholds, mNumThresholds,
    NULL, &mThresholds[0], MAX_HOLE_TRIALS, true);
  m_strThresholds = ListToString(1, mNumThresholds, &mThresholds[0]);
  mBestThreshInd = -1;
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

// Bracketing
void CHoleFinderDlg::OnBracketLast()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Boundary contour
void CHoleFinderDlg::OnExcludeOutside()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Clear the data here and in holefinder
// Safe to call externally
void CHoleFinderDlg::OnButClearData()
{
  mHaveHoles = false;
  CLEAR_RESIZE(mHoleMeans, float, 0);
  CLEAR_RESIZE(mHoleSDs, float, 0);
  CLEAR_RESIZE(mHoleBlackFracs, float, 0);
  CLEAR_RESIZE(mXcenters, float, 0);
  CLEAR_RESIZE(mYcenters, float, 0);
  CLEAR_RESIZE(mXstages, float, 0);
  CLEAR_RESIZE(mYstages, float, 0);
  CLEAR_RESIZE(mExcluded, bool, 0);
  CLEAR_RESIZE(mPieceOn, int, 0);
  mHelper->mFindHoles->clearAll();
  mWinApp->mMainView->DrawImage();
  mWinApp->RestoreViewFocus();
}

// Set the size and spacing from the last values
void CHoleFinderDlg::OnButSetSizeSpace()
{
  mWinApp->RestoreViewFocus();
  if (mLastHoleSize <= 0.)
    return;
  m_fHolesSize = mLastHoleSize;
  m_fSpacing = mLastHoleSpacing;
  UpdateData(false);
}

// Edit boxes for mean limits: process string, set slider
void CHoleFinderDlg::OnKillfocusEditLowerMean()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mParams.lowerMeanCutoff = (float)atof(m_strLowerMean);
  m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff);
  m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mMeanMin) /
    (mMidMean - mMeanMin));
  B3DCLAMP(m_intLowerMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndDraw();
}

void CHoleFinderDlg::OnKillfocusEditUpperMean()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mParams.upperMeanCutoff = (float)atof(m_strUpperMean);
  m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff);
  m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMeanMin) /
    (mMidMean - mMeanMin));
  B3DCLAMP(m_intUpperMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndDraw();
}

// Respond to slider changes: change actual cutoff in parameters as well as output slider
// value
void CHoleFinderDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
  bool takeNint = true;
  UpdateData(TRUE);
  CSliderCtrl *pSlider = (CSliderCtrl *)pScrollBar;
  if (pSlider == &m_sliderLowerMean) {
    mParams.lowerMeanCutoff = (float)(m_intLowerMean * (mMidMean - mMeanMin) / 255. +
      mMeanMin);
    m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff);
  }
  if (pSlider == &m_sliderUpperMean) {
    mParams.upperMeanCutoff = (float)(m_intUpperMean * (mMeanMax - mMidMean) / 255. +
      mMidMean);
    m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff);
  }
  if (pSlider == &m_sliderSDcutoff) {
    mParams.SDcutoff = (float)(m_intSDcutoff * (mSDmax - mSDmin) / 255. + mSDmin);
    m_strSDcutoff.Format("%.4g", mParams.SDcutoff);
  }
  if (pSlider == &m_sliderBlackPct) {
    mParams.blackFracCutoff = (float)(m_intBlackPct * (mBlackFracMax - mBlackFracMin) /
      255. + mBlackFracMin);
    m_strBlackPct.Format("%.1f", 100. * mParams.blackFracCutoff);
  }
  UpdateData(false);
  SetExclusionsAndDraw();
  if (nSBCode != TB_THUMBTRACK)
    mWinApp->RestoreViewFocus();
  CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

// Other options
void CHoleFinderDlg::OnShowIncluded()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}

void CHoleFinderDlg::OnShowExcluded()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}


void CHoleFinderDlg::OnRadioLayoutType()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Make navigator points - the real work is done by Nav
void CHoleFinderDlg::OnButMakeNavPts()
{
  mParams.layoutType = m_iLayoutType;
  DoMakeNavPoints(mParams.layoutType, mParams.lowerMeanCutoff, mParams.upperMeanCutoff,
    mParams.SDcutoff, mParams.blackFracCutoff);
}

//Externally called routine.  Pass -1 to use the layout in params and EXTRA_NO_VALUE to
// use the cutoffs in params
int CHoleFinderDlg::DoMakeNavPoints(int layoutType, float lowerMeanCutoff, 
  float upperMeanCutoff, float sdCutoff, float blackCutoff)
{
  CMapDrawItem *poly = NULL;
  ShortVec gridX, gridY;
  int ind, numAdded = 0;
  float avgLen = -1., avgAngle = -999.;
  if (CheckAndSetNav("making Navigator points from hole positions"))
    return -1;
  if (!HaveHolesToDrawOrMakePts()) {
    SEMMessageBox("There are no hole positions available for making Navigator points");
    return -1;
  }

  // Use the set parameters if alternatives not passed, and run exclusion again
  if (lowerMeanCutoff < EXTRA_VALUE_TEST)
    lowerMeanCutoff = mParams.lowerMeanCutoff;
  if (upperMeanCutoff < EXTRA_VALUE_TEST)
    upperMeanCutoff = mParams.upperMeanCutoff;
  if (sdCutoff < 0.)
    sdCutoff = mParams.SDcutoff;
  if (blackCutoff < 0.)
    mParams.blackFracCutoff;
  if (layoutType < 0)
    layoutType = mParams.layoutType;
  SetExclusionsAndDraw(lowerMeanCutoff, upperMeanCutoff, sdCutoff, blackCutoff);
  if (mIsOpen) {
    UpdateData(true);
    DialogToParams();
    *mMasterParams = mParams;
    mWinApp->RestoreViewFocus();
  }

  // Add up number to add, return if none left
  for (ind = 0; ind < (int)mExcluded.size(); ind++)
    if (!mExcluded[ind])
      numAdded++;
  if (!numAdded)
    return 0;

  if (mBoundPolyID)
    poly = mNav->FindItemWithMapID(mBoundPolyID, false);

  // Get the grid positions and pass it all on
  mHelper->mFindHoles->assignGridPositions(mXcenters, mYcenters, gridX, gridY, avgAngle,
    avgLen);
  mAddedGroupID = mNav->AddFoundHoles(&mXstages, &mYstages, &mExcluded,
    &mXinPiece, &mYinPiece, &mPieceOn, &gridX, &gridY, avgLen * mPixelSize,
    2.f * mBestRadius * mReduction * mPixelSize, avgAngle, mRegistration, mMapID,
    (float)mZstage, poly, layoutType);
  if (mAddedGroupID) {
    mNav->FindItemWithMapID(mAddedGroupID, false, true);
    mIndexOfGroupItem = mNav->GetFoundItem();
    mWinApp->mMainView->DrawImage();
    if (mIsOpen)
      m_butMakeNavPts.EnableWindow(false);
    poly = mNav->GetOtherNavItem(mIndexOfGroupItem);
    return numAdded;
  }
  return 0;
}

// Transfer values from the dialog to the parameters
void CHoleFinderDlg::DialogToParams()
{
  int ind;
  if (!mIsOpen)
    return;
  mParams.useBoundary = m_bExcludeOutside;
  mParams.showExcluded = m_bShowExcluded;
  mParams.layoutType = m_iLayoutType;
  mParams.spacing = m_fSpacing;
  mParams.diameter = m_fHolesSize;
  mParams.maxError = m_fMaxError;
  mParams.bracketLast = m_bBracketLast;
  mParams.sigmas.clear();
  mParams.thresholds.clear();
  for (ind = 0; ind < mNumSigmas; ind++)
    mParams.sigmas.push_back((float)mSigmas[ind]);
  for (ind = 0; ind < mNumIterations; ind++)
    mParams.sigmas.push_back(-(float)mIterations[ind]);
  for (ind = 0; ind < mNumThresholds; ind++)
    mParams.thresholds.push_back((float)mThresholds[ind]);
}

// Transfer values from the parameters to the dialog
void CHoleFinderDlg::ParamsToDialog()
{
  int ind;
  mNumIterations = 0;
  mNumSigmas = 0;
  mNumThresholds = 0;
  for (ind = 0; ind < (int)mParams.sigmas.size(); ind++) {
    if (mParams.sigmas[ind] > 0)
      mSigmas[mNumSigmas++] = mParams.sigmas[ind];
    else if (mParams.sigmas[ind] < 0)
      mIterations[mNumIterations++] = -(int)mParams.sigmas[ind];
  }
  for (ind = 0; ind < (int)mParams.thresholds.size(); ind++)
    mThresholds[mNumThresholds++] = mParams.thresholds[ind];
  if (!mIsOpen)
    return;
  m_strSigmas = ListToString(1, mNumSigmas, &mSigmas[0]);
  m_strIterations = ListToString(3, mNumIterations, &mIterations[0]);
  if (mNumThresholds)
    m_strThresholds = mWinApp->mParamIO->EntryListToString(1, 2, mNumThresholds, NULL,
      &mThresholds[0]);

  // If there are holes, set up the sliders and cutoffs; clamp sliders not actual cutoffs
  if (mHaveHoles) {
    m_strSDcutoff.Format("%.4g", mParams.SDcutoff);
    m_strBlackPct.Format("%.1f", 100. * mParams.blackFracCutoff);
    m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mMeanMin) /
      (mMidMean - mMeanMin));
    m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMidMean) /
      (mMeanMax - mMidMean));
    m_intSDcutoff = (int)(255. * (mParams.SDcutoff - mSDmin) / (mSDmax -mSDmin));
    m_intBlackPct = (int)(255. * (mParams.blackFracCutoff - mBlackFracMin) / 
      (mBlackFracMax - mBlackFracMin));
    B3DCLAMP(m_intLowerMean, 0, 255);
    B3DCLAMP(m_intUpperMean, 0, 255);
    B3DCLAMP(m_intSDcutoff, 0, 255);
    B3DCLAMP(m_intBlackPct, 0, 255);
  } else {
    m_strSDcutoff = "";
    m_strBlackPct = "";
    m_intLowerMean = 0;
    m_intUpperMean = 255;
    m_intSDcutoff = 255;
    m_intBlackPct = 255;
  }
  m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.lowerMeanCutoff : 0.);
  m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.upperMeanCutoff : 0.);
  m_fSpacing = mParams.spacing;
  m_fHolesSize = mParams.diameter;
  m_fMaxError = mParams.maxError;
  m_bExcludeOutside = mParams.useBoundary;
  m_bBracketLast = mParams.bracketLast;
  m_bShowExcluded = mParams.showExcluded;
  m_iLayoutType = mParams.layoutType;
  UpdateData(false);
}

// Take care of disabling things as appropriate but also update the min/max slider values
void CHoleFinderDlg::ManageEnables()
{
  if (!mIsOpen)
    return;
  m_butFindHoles.EnableWindow(!mWinApp->DoingTasks() && mNumSigmas + mNumIterations > 0 &&
    mNumThresholds > 0);
  m_sliderLowerMean.EnableWindow(mHaveHoles);
  m_sliderUpperMean.EnableWindow(mHaveHoles);
  m_sliderSDcutoff.EnableWindow(mHaveHoles);
  m_sliderBlackPct.EnableWindow(mHaveHoles);
  m_editLowerMean.EnableWindow(mHaveHoles);
  m_editUpperMean.EnableWindow(mHaveHoles);
  m_butSetSizeSpace.EnableWindow(mLastHoleSize > 0.);
  if (mHaveHoles) {
    m_strMinLowerMean.Format("%.4g", mMeanMin);
    m_strMaxLowerMean.Format("%.4g", (mMeanMin + mMeanMax / 2.));
    m_strMinUpperMean.Format("%.4g", (mMeanMin + mMeanMax / 2.));
    m_strMaxUpperMean.Format("%.4g", mMeanMax);
    m_strMinSDcutoff.Format("%.4g", mSDmin);
    m_strMaxSDcutoff.Format("%.4g", mSDmax);
    m_strMinBlackPct.Format("%.1f", 100. * mBlackFracMin);
    m_strMaxBlackPct.Format("%.1f", 100. * mBlackFracMax);

  } else {
    m_strMinLowerMean = "";
    m_strMaxLowerMean = "";
    m_strMinUpperMean = "";
    m_strMaxUpperMean = "";
    m_strMinSDcutoff = "";
    m_strMaxSDcutoff = "";
    m_strMinBlackPct = "";
    m_strMaxBlackPct = "";
  }
  m_butBracketLast.EnableWindow(mBestThreshInd >= 0);
  m_butShowExcluded.EnableWindow(mHaveHoles);
  m_butShowIncluded.EnableWindow(mHaveHoles);
  m_butZigzag.EnableWindow(mHaveHoles);
  m_butFromFocus.EnableWindow(mHaveHoles);
  m_butInGroups.EnableWindow(mHaveHoles && mHelper->GetGridGroupSize() > 0);
  m_butMakeNavPts.EnableWindow(HaveHolesToDrawOrMakePts());
  UpdateData(false);
}

// New settings: copy master params, send to dialog
void CHoleFinderDlg::UpdateSettings()
{
  mParams = *mMasterParams;
  ParamsToDialog();
  ManageEnables();
}

// A few utilities
CString CHoleFinderDlg::ListToString(int type, int num, void * list)
{
  if (num)
    return mWinApp->mParamIO->EntryListToString(type, 2, num,
    (int *)(type == 3 ? list : NULL), (double *)(type == 1 ? list : NULL));
  else
    return "";
}

void CHoleFinderDlg::InvertVectorInY(FloatVec & vec, int ysize)
{
  for (int ind = 0; ind < (int)vec.size(); ind++)
    vec[ind] = ysize - vec[ind];
}

static char *formatSigma(float sigma)
{
  static char sigTextBuf[20];
  if (sigma < 0)
    sprintf(sigTextBuf, "med: %d", B3DNINT(-sigma));
  else
    sprintf(sigTextBuf, "sig: %.1f", sigma);
  return &sigTextBuf[0];
}

// The main attraction: find the holes
void CHoleFinderDlg::OnButFindHoles()
{
  DoFindHoles(mWinApp->mMainView->GetActiveImBuf());
}

int CHoleFinderDlg::DoFindHoles(EMimageBuffer *imBuf)
{
  float curDiam, diamReduced, spacingReduced, maxRadius, area, maxArea = 0.;
  FloatVec *widths, *increments;
  IntVec *numCircles;
  FloatVec xBoundTemp, yBoundTemp;
  CArray<CMapDrawItem *, CMapDrawItem *> *itemArray;
  MontParam *masterMont = mWinApp->GetMontParam();
  KImage *image;
  CMapDrawItem *item;
  int reloadBinning, overBinSave, readBuf, err, numScans, ind, nav;
  int numMedians, numOnIm, sigStart, sigEnd, iterStart, iterEnd;
  float useWidth, useHeight, ptX, ptY, delX, delY, bufStageX, bufStageY;
  double tstageX, tstageY;
  ScaleMat rMat, rInv, aMat;
  MontParam *montP = &mMontParam;
  float targetDiam = mHelper->GetHFtargetDiamPix();
  float tooSmallCrit = 0.8f;
  float minFracPtsOnImage = 0.74f;
  BOOL convertSave, loadUnbinSave;
  CString noMontReason;

  if (CheckAndSetNav("finding holes"))
    return 1;

  // Initialize by clearing out existing hole info
  mMontage = false;
  mXboundary.clear();
  mYboundary.clear();
  mXinPiece.clear();
  mPieceOn.clear();
  mBoundPolyID = 0;
  mAddedGroupID = 0;
  mCurStore = -2;
  mBufInd = (int)(imBuf - mWinApp->GetImBufs());
  mHaveHoles = false;
  mLastHoleSize = 0.;
  mLastHoleSpacing = 0.;

  // Check preconditions
  DialogToParams();
  if (!mIsOpen) {
    mMasterParams = mHelper->GetHoleFinderParams();
    mParams = *mMasterParams;
    ParamsToDialog();
  }
  mWinApp->RestoreViewFocus();
  if (mNumSigmas + mNumIterations == 0 || mNumThresholds == 0) {
    SEMMessageBox(mNumThresholds ? "There are no thresholds specified for finding holes" :
      "There are no filters specified for finding holes");
    return 1;
  }

  if (mBufInd < 0 || mBufInd >= MAX_BUFFERS || !(imBuf && imBuf->mImage)) {
    SEMMessageBox("The specified buffer is not in the active window or has no image for"
      " finding holes", MB_EXCLAME);
    return 1;
  }
  image = imBuf->mImage;
  mHelper->GetHFscanVectors(&widths, &increments, &numCircles);
  mPixelSize = mWinApp->mShiftManager->GetPixelSize(imBuf);
  mWinApp->mScope->GetStagePosition(tstageX, tstageY, mZstage);
  if (mWinApp->mProcessImage->GetTestCtfPixelSize())
    mPixelSize = 0.001f * mWinApp->mProcessImage->GetTestCtfPixelSize() * imBuf->mBinning;
  if (!mPixelSize) {
    SEMMessageBox("No pixel size is assigned to this image; it is needed for finding "
      "holes", MB_EXCLAME);
    return 1;
  }
  if (image->getType() == kRGB) {
    SEMMessageBox("Hole finder does not work on color images");
    return 1;
  }
  if (!mNav->BufferStageToImage(imBuf, aMat, delX, delY) ||
    !imBuf->GetStagePosition(bufStageX, bufStageY)) {
    SEMMessageBox("The currently active image does not have enough information\n"
      "to convert positions to stage coordinates.", MB_EXCLAME);
    return 1;
  }

  // Get some sizes, get a nav item regardless of montage so it can be used as drawn on
  // and for registration and stage Z
  curDiam = mParams.diameter / mPixelSize;
  mFullYsize = image->getHeight();

  mNavItem = mNav->FindItemWithMapID(imBuf->mMapID);
  mMapID = 0;
  if (mNavItem) {
    mZstage = mNavItem->mStageZ;
    mMapID = mNavItem->mMapID;
  }

  // Montaging: check preconditions first
  if (imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW) {
    if (!imBuf->mMapID)
      noMontReason = "the buffer has no ID for finding its Navigator map";
    else if (!mNavItem)
      noMontReason = "could not find the map item from the ID in the buffer";
    if (mNavItem && mNav->PrepareMontAdjustments(imBuf, rMat, rInv, ptX,
      ptY) > 0)
      noMontReason = "the map is rotated";
    else if (mNavItem && mNav->AccessMapFile(mNavItem, mImageStore,
      mCurStore, montP, useWidth, useHeight))
      noMontReason = "could not access the map file again";
    if (noMontReason.IsEmpty()) {
      mMontage = true;

      // Reload the overview if it is binned and holes are too small, or if it is bytes
      if ((image->getType() == kUBYTE && mImageStore->getMode() != MRC_MODE_BYTE) ||
        (imBuf->mOverviewBin > 1 && curDiam < targetDiam * tooSmallCrit)) {

        // Set the bining at which to reload
        reloadBinning = imBuf->mOverviewBin;
        while (reloadBinning > 1) {
          if ((curDiam * imBuf->mOverviewBin) / reloadBinning > targetDiam * tooSmallCrit)
            break;
          reloadBinning--;
        }
        
        // Save some params, get rid of store for now, and set params for reload
        overBinSave = masterMont->overviewBinning;
        convertSave = mHelper->GetConvertMaps();
        loadUnbinSave = mHelper->GetLoadMapsUnbinned();
        if (mCurStore < 0)
          delete mImageStore;
        masterMont->overviewBinning = reloadBinning;
        mHelper->SetConvertMaps(false);
        mHelper->SetLoadMapsUnbinned(false);
        readBuf = mWinApp->mBufferManager->GetBufToReadInto();

        // Reload and restore
        err = mNav->DoLoadMap(true, mNavItem, -1);
        masterMont->overviewBinning = overBinSave;
        mHelper->SetConvertMaps(convertSave);
        mHelper->SetLoadMapsUnbinned(loadUnbinSave);
        if (err) {
          noMontReason = "could not reload the map to work with it properly";
        } else {

          // Copy to current buffer and recompute diameter/pixel size
          if (readBuf != mBufInd) {
            mWinApp->mBufferManager->CopyImageBuffer(readBuf, mBufInd);
            mWinApp->SetCurrentBuffer(mBufInd);
          }
          image = imBuf->mImage;
          mFullYsize = image->getHeight();
          mPixelSize = mWinApp->mShiftManager->GetPixelSize(imBuf);
          if (mWinApp->mProcessImage->GetTestCtfPixelSize())
            mPixelSize = 0.001f * mWinApp->mProcessImage->GetTestCtfPixelSize() * 
            imBuf->mBinning;
          if (!mPixelSize) {
            SEMMessageBox("The image has no pixel size after reloading for finding "
              "holes");
            return 1;
          }
          curDiam = mParams.diameter / mPixelSize;
          if (mNav->AccessMapFile(mNavItem, mImageStore, mCurStore, montP,
            useWidth, useHeight)) {
            noMontReason = "could not access the map file again after reloading it";
          }
        }
      }

      // Save the overview binning and the minioffset so the imBuf does not need to be
      // accessed again
      mFullBinning = imBuf->mOverviewBin;
      mMontage = montP->xNframes * montP->yNframes > 1;
      if (mMontage && imBuf->mMiniOffsets) {
        mMiniOffsets = new MiniOffsets;
        *mMiniOffsets = *imBuf->mMiniOffsets;
      }
    }
  }

  // Report failure, but go on
  if (mMontage && !noMontReason.IsEmpty()) {
    mWinApp->AppendToLog("Cannot analyze montage pieces when finding holes: " + 
      noMontReason);
    mMontage = false;
  }

  // Copy the montage params to our copy if the pointer got replaced with one from an
  // open file
  if (mMontage && montP != &mMontParam)
    mMontParam = *montP;

  mRegistration = mNav->GetCurrentRegistration();
  if (mNavItem)
    mRegistration = mNavItem->mRegistration;

  // Look for boundary contour if selected
  if (mParams.useBoundary) {
    itemArray = mNav->GetItemArray();
    mNav->BufferStageToImage(imBuf, aMat, delX, delY);
    for (nav = 0; nav < (int)itemArray->GetSize(); nav++) {
      item = itemArray->GetAt(nav);

      // For polygon at same registration
      // transform points to image and count how many are on the image
      if (item->mType == ITEM_TYPE_POLYGON && item->mRegistration == mRegistration) {
        numOnIm = 0;
        xBoundTemp.resize(item->mNumPoints);
        yBoundTemp.resize(item->mNumPoints);
        for (ind = 0; ind < item->mNumPoints; ind++) {
          xBoundTemp[ind] = aMat.xpx * item->mPtX[ind] + aMat.xpy * item->mPtY[ind] +delX;
          yBoundTemp[ind] = aMat.ypx * item->mPtX[ind] + aMat.ypy * item->mPtY[ind] +delY;
          if (xBoundTemp[ind] >= 0 && xBoundTemp[ind] < image->getWidth() &&
            yBoundTemp[ind] >= 0 && yBoundTemp[ind] < mFullYsize)
            numOnIm++;

          // Stop if enough points are outside the image
          if ((ind + 1. - numOnIm) / item->mNumPoints > 1. - minFracPtsOnImage)
            break;
        }

        // If enough are on image, get the area, and if it is the biggest such, take it
        if ((float)numOnIm / item->mNumPoints >= minFracPtsOnImage) {
          area = mNav->ContourArea(item->mPtX, item->mPtY, item->mNumPoints);
          if (area > maxArea) {
            maxArea = area;
            mXboundary = xBoundTemp;
            mYboundary = yBoundTemp;
            mBoundPolyID = item->mMapID;
          }
        }
      }
    }
  }

  numScans = (int)widths->size();
  ACCUM_MIN(numScans, (int)numCircles->size());
  ACCUM_MIN(numScans, (int)increments->size());

  // Get the reduction
  mReduction = B3DMAX(1.f, curDiam / targetDiam);
  diamReduced = curDiam / mReduction;
  spacingReduced = (mParams.spacing / mPixelSize) / mReduction;

  // get the maximum radius assuming worst-case expansion
  maxRadius = diamReduced / 2.f;
  for (ind = 0; ind < numScans; ind++)
    maxRadius += (numCircles->at(ind) / 2.f) * increments->at(ind) + widths->at(ind);
  mErrorMax = mParams.maxError / mPixelSize;

  // Copy the parameters or set up to bracket last one
  mUseSigmas = mParams.sigmas;
  mUseThresholds = mParams.thresholds;
  if (mParams.bracketLast && mBestThreshInd >= 0) {
    mUseThresholds.clear();
    mUseSigmas.clear();
    for (ind = B3DMAX(0, mBestThreshInd - 1);
      ind <= B3DMIN(mNumThresholds - 1, mBestThreshInd + 1); ind++)
      mUseThresholds.push_back(mParams.thresholds[ind]);

    // Filter is complicated: if there are multiple ones of the type that is best, just
    // bracket with that type; otherwise use the one best and the middle one of the other
    // type
    sigStart = 0;
    sigEnd = -1;
    iterStart = mNumSigmas;
    iterEnd = mNumSigmas - 1;
    if (!mNumIterations || (mNumSigmas > 1 && mBestSigInd < mNumSigmas)) {
      sigStart = B3DMAX(0, mBestSigInd - 1);
      sigEnd = B3DMIN(mNumSigmas - 1, mBestSigInd + 1);
    } else if (!mNumSigmas || (mNumIterations > 1 && mBestSigInd >= mNumSigmas)) {
      iterStart = B3DMAX(mNumSigmas, mBestSigInd - 1);
      iterEnd = B3DMIN(mNumSigmas + mNumIterations + 1, mBestSigInd + 1);
    } else if (mBestSigInd < mNumSigmas) {
      sigStart = sigEnd = mBestSigInd;
      if (mNumIterations) {
        iterStart = iterEnd = mNumIterations / 2;
      }
    } else {
      iterStart = iterEnd = mBestSigInd;
      if (mNumSigmas) {
        sigStart = sigEnd = mNumSigmas / 2;
      }
    }
    for (ind = sigStart; ind <= sigEnd; ind++)
      mUseSigmas.push_back(mParams.sigmas[ind]);
    for (ind = iterStart; ind <= iterEnd; ind++)
      mUseSigmas.push_back(mParams.sigmas[ind]);
  }

  numMedians = 0;
  for (ind = 0; ind < (int)mUseSigmas.size(); ind++)
    if (mParams.sigmas[ind] < 0)
      numMedians++;

  // Initialize: reduce the image
  image->Lock();
  err = mHelper->mFindHoles->initialize(image->getData(), image->getType(),
    image->getWidth(), mFullYsize, mReduction, maxRadius,
    CACHE_KEEP_BOTH + (numMedians > 1 ? CACHE_KEEP_MEDIAN : 0));
  image->UnLock();
  if (err) {
    AfxMessageBox(CString("Error in initializing holefinder routine:\n") +
      mHelper->mFindHoles->returnErrorString(err), MB_EXCLAME);
    StopScanning();
    return 1;
  }

  // Set up the parameters and call to start the scan
  mHelper->mFindHoles->setSequenceParams(diamReduced, spacingReduced,
    mHelper->GetHFretainFFTs() != 0, mErrorMax, mHelper->GetHFfractionToAverage(),
    mHelper->GetHFminNumForTemplate(), mHelper->GetHFmaxDiamErrFrac(),
    mHelper->GetHFavgOutlieCrit(), mHelper->GetHFfinalNegOutlieCrit(),
    mHelper->GetHFfinalPosOutlieCrit(), mHelper->GetHFfinalOutlieRadFrac());
  mHelper->mFindHoles->setRunsInSequence(&(*increments)[0], &(*widths)[0],
    &(*numCircles)[0], numScans, &mUseSigmas[0], (int)mUseSigmas.size(),
    &mUseThresholds[0], (int)mUseThresholds.size());
  mFindingHoles = true;
  mSigInd = 0;
  mThreshInd = 0;
  mWinApp->SetStatusText(SIMPLE_PANE, "FINDING HOLES");
  mWinApp->UpdateBufferWindows();
  ScanningNextTask(0);
  return 0;
}

// Next task for scanning is to run the next set of parameters
void CHoleFinderDlg::ScanningNextTask(int param)
{
  int err, ind;
  float sigUsed, threshUsed;
  FloatVec peakVals, xMissing, yMissing, xCenClose, yCenClose;
  FloatVec peakClose, xInPiece, yInPiece;
  IntVec pieceSavedAt, tileXnum, tileYnum, pieceIndex, secIxAliPiece, secIyAliPiece;
  IntVec fileZindex;
  FloatVecArray pieceXcenVec, pieceYcenVec, piecePeakVec, pieceMeanVec, pieceSDsVec;
  FloatVecArray pieceOutlieVec;
  MontParam *montP = &mMontParam;
  EMimageBuffer *allBufs = mWinApp->GetImBufs();
  EMimageBuffer *imBuf = &allBufs[mBufInd];
  int numMissAdded, numPcInSec, ixpc, iypc, ipc, readBuf;
  int xcoord, ycoord, zcoord, numFromCorr, numPoints;
  ScaleMat aMat, aInv;
  float delX, delY, ptX, ptY;
  CString noMontReason, statStr;

  if (!mFindingHoles)
    return;
  if (CheckAndSetNav()) {
    StopScanning();
    return;
  }
  
  err = mHelper->mFindHoles->runSequence(mSigInd, sigUsed, mThreshInd, 
    threshUsed, mXboundary,  mYboundary, mBestSigInd, mBestThreshInd,
      mBestRadius, mTrueSpacing, mXcenters, mYcenters,
      peakVals, xMissing, yMissing, xCenClose, yCenClose, peakClose,
      numMissAdded);
  if (err < 0) {
    statStr.Format("%s  thr: %.1f  #: %d  miss: %d", formatSigma(sigUsed),
      threshUsed, (int)mXcenters.size(), (int)xMissing.size());
    if (mIsOpen) {
      m_strStatusOutput = statStr;
      UpdateData(false);
    }
    mWinApp->SetStatusText(MEDIUM_PANE, statStr);
    mWinApp->AddIdleTask(TASK_FIND_HOLES, 0, 0);
    return;
  }

  if (!err) {
    mIntensityRad = mBestRadius - mErrorMax / mReduction;
    err = mHelper->mFindHoles->holeIntensityStats(mIntensityRad, mXcenters, mYcenters,
      false, true, &mHoleMeans, &mHoleSDs, &mHoleBlackFracs);
  }
  if (err) {
    SEMMessageBox(CString("Error in holefinder routine:\n") +
      mHelper->mFindHoles->returnErrorString(err), MB_EXCLAME);
    StopScanning();
    return;
  }

  if (mMontage) {
    readBuf = mWinApp->mBufferManager->GetBufToReadInto();
    if (mBufInd == readBuf)
      readBuf = mBufInd == 0 ? 1 : 0;

    // pieceSavedAt is indexed by ixpc * nYframes + iypc
    // But pieceIndex is expected to be indexed by ixpc + nXframes * iypc
    if (mWinApp->mMontageController->ListMontagePieces(mImageStore, montP,
      mNavItem->mMapSection, pieceSavedAt) < 0) {
      SEMMessageBox("Holefinder error from bad montage parameters; please report values "
        "printed in log");
      StopScanning();
      return;
    }
    ind = 0;
    pieceIndex.clear();
    pieceIndex.resize(pieceSavedAt.size(), -1);
    for (iypc = 0; iypc < montP->yNframes; iypc++) {
      for (ixpc = 0; ixpc < montP->xNframes; ixpc++) {
        ipc = ixpc * montP->yNframes + iypc;
        if (pieceSavedAt[ipc] >= 0) {
          tileXnum.push_back(ixpc);

          // Invert the Y tile number so that coordinates make sense in terms of adjcency
          tileYnum.push_back((montP->yNframes - 1) - iypc);
          fileZindex.push_back(pieceSavedAt[ipc]);
          mImageStore->getPcoord(pieceSavedAt[ipc], xcoord, ycoord, zcoord);
          secIxAliPiece.push_back(xcoord);
          secIyAliPiece.push_back(ycoord);

          // Minioffsets are indexed the same way and Y is inverted relative to piece
          // coords, which are still right-handed
          if (mMiniOffsets) {
            secIxAliPiece[ind] += mMiniOffsets->offsetX[ipc] * mFullBinning;
            secIyAliPiece[ind] -= mMiniOffsets->offsetY[ipc] * mFullBinning;
          }
          secIyAliPiece[ind] = mFullYsize * mFullBinning - 
            (secIyAliPiece[ind] + montP->yFrame);
          pieceIndex[ixpc + montP->xNframes * iypc] = ind++;
        }
      }
    }
    numPcInSec = ind;
    pieceXcenVec.resize(numPcInSec);
    pieceYcenVec.resize(numPcInSec);
    piecePeakVec.resize(numPcInSec);
    pieceMeanVec.resize(numPcInSec);
    pieceSDsVec.resize(numPcInSec);
    pieceOutlieVec.resize(numPcInSec);

    // Process montage pieces: set up, then loop on pieces
    mHelper->mFindHoles->setMontPieceVectors(&pieceXcenVec, &pieceYcenVec, &piecePeakVec,
      &pieceMeanVec, &pieceSDsVec, &pieceOutlieVec, &secIxAliPiece,
      &secIyAliPiece, &pieceIndex, &tileXnum, &tileYnum);
    mHelper->mFindHoles->setMontageParams(montP->xNframes, montP->yNframes, montP->xFrame,
      montP->yFrame, mFullBinning, mHelper->GetHFpcToPcSameFrac(),
      mHelper->GetHFpcToFullSameFrac(), mHelper->GetHFsubstOverlapDistFrac(),
      mHelper->GetHFusePieceEdgeDistFrac(), mHelper->GetHFaddOverlapFrac());
    for (ipc = 0; ipc < numPcInSec; ipc++) {
      err = mWinApp->mBufferManager->ReadFromFile(mImageStore, fileZindex[ipc], readBuf,
        true, true);
      if (err) {
        noMontReason.Format("Error reading z = %d from file", fileZindex[ipc]);
      } else {
        allBufs[readBuf].mImage->Lock();
        err = mHelper->mFindHoles->initialize(allBufs[readBuf].mImage->getData(),
          mImageStore->getMode(), montP->xFrame, montP->yFrame,
          mFullBinning * mReduction, mBestRadius + 4.f,
          CACHE_KEEP_BOTH + CACHE_KEEP_AVGS);
        allBufs[readBuf].mImage->UnLock();
      }
      if (!err)
        err = mHelper->mFindHoles->processMontagePiece(mParams.sigmas[mBestSigInd],
          mParams.thresholds[mBestThreshInd], mTrueSpacing / mReduction, ipc, mXboundary,
          mYboundary, mIntensityRad, numFromCorr);
      if (err) {
        if (noMontReason.IsEmpty())
          noMontReason = mHelper->mFindHoles->returnErrorString(err);
        SEMMessageBox("Refinement of positions with montage analysis failed:\n" +
          noMontReason, MB_EXCLAME);
        break;
      }
    }

    // USE the positions to improve the centers arrays
    if (!err) {
      mHelper->mFindHoles->resolvePiecePositions(true, mXcenters, mYcenters, peakVals,
        xMissing, yMissing, xCenClose, yCenClose, peakClose, mPieceOn, mXinPiece,
        mYinPiece, &mHoleMeans, &mHoleSDs, &mHoleBlackFracs);

      // Invert the Y positions in piece and fix up the pieceOn for right-handed order
      InvertVectorInY(mYinPiece, montP->yFrame);
      for (ind = 0; ind < (int)mPieceOn.size(); ind++) {
        xcoord = mPieceOn[ind] / montP->yNframes;
        ycoord = mPieceOn[ind] % montP->yNframes;
        mPieceOn[ind] = (montP->yNframes - 1) - ycoord + xcoord * montP->yNframes;
      }
    }

  } else {
    mPieceOn.clear();
    mPieceOn.resize(mXcenters.size(), -1);
  }

  // OUTPUT summary line and skip if nothing
  numPoints = (int)mXcenters.size();
  *mMasterParams = mParams;
  StopScanning();
  if (!numPoints) {
    if (mIsOpen) {
      m_strStatusOutput = "";
      UpdateData(false);
    }
    mWinApp->AppendToLog("No holes were found!");
    return;
  }
  mLastHoleSize = 0.01f * B3DNINT(100.f * 2.f * mBestRadius * mReduction * mPixelSize);
  mLastHoleSpacing = 0.01f * B3DNINT(100.f * mTrueSpacing * mPixelSize);
  statStr.Format("%s  thr: %.1f  #: %d  size: %.2f  sep: %.2f",
    formatSigma(mParams.sigmas[mBestSigInd]), mParams.thresholds[mBestThreshInd],
    numPoints, mLastHoleSize, mLastHoleSpacing);
  if (mIsOpen)
    m_strStatusOutput = statStr;
  else
    mWinApp->AppendToLog("Hole finder: " + statStr);

  mHaveHoles = true;

  // Get stage positions
  mNav->BufferStageToImage(imBuf, aMat, delX, delY);
  aInv = MatInv(aMat);
  mXstages.resize(numPoints);
  mYstages.resize(numPoints);
  rsMedian(&mHoleMeans[0], numPoints, &mXstages[0], &mMidMean);
  for (ind = 0; ind < numPoints; ind++) {
    ptX = mXcenters[ind];
    ptY = mYcenters[ind];
    mNav->AdjustMontImagePos(imBuf, ptX, ptY);
    mWinApp->mShiftManager->ApplyScaleMatrix(aInv, ptX - delX, ptY - delY, mXstages[ind],
      mYstages[ind]);
  }

  // Get mins and maxes and set the sliders if necessary
  mMeanMin = *std::min_element(mHoleMeans.begin(), mHoleMeans.end());
  mMeanMax = *std::max_element(mHoleMeans.begin(), mHoleMeans.end());
  mSDmin = *std::min_element(mHoleSDs.begin(), mHoleSDs.end());
  mSDmax = *std::max_element(mHoleSDs.begin(), mHoleSDs.end());
  mBlackFracMin = *std::min_element(mHoleBlackFracs.begin(), mHoleBlackFracs.end());
  mBlackFracMax = *std::max_element(mHoleBlackFracs.begin(), mHoleBlackFracs.end());
  if (mParams.lowerMeanCutoff < EXTRA_VALUE_TEST)
    mParams.lowerMeanCutoff = mMeanMin;
  if (mParams.upperMeanCutoff < EXTRA_VALUE_TEST)
    mParams.upperMeanCutoff = mMeanMax;
  if (mParams.SDcutoff  < EXTRA_VALUE_TEST)
    mParams.SDcutoff = mSDmax;
  if (mParams.blackFracCutoff  < EXTRA_VALUE_TEST)
    mParams.blackFracCutoff = mBlackFracMax;
  mExcluded.resize(numPoints);
  m_bShowIncluded = true;
  ParamsToDialog();
  ManageEnables();
  if (!mIsOpen)
    *mMasterParams = mParams;
  SetExclusionsAndDraw();
}

void CHoleFinderDlg::SetExclusionsAndDraw()
{
  SetExclusionsAndDraw(mParams.lowerMeanCutoff, mParams.upperMeanCutoff, mParams.SDcutoff,
    mParams.blackFracCutoff);
}

void CHoleFinderDlg::SetExclusionsAndDraw(float lowerMeanCutoff, float upperMeanCutoff, 
  float sdCutoff, float blackCutoff)
{
  int ind;
  if (!mHaveHoles)
    return;
  for (ind = 0; ind < (int)mXcenters.size(); ind++) {
    mExcluded[ind] = mHoleMeans[ind] < lowerMeanCutoff || 
      mHoleMeans[ind] > upperMeanCutoff || mHoleSDs[ind] > sdCutoff ||
      mHoleBlackFracs[ind] > blackCutoff;
  }
  mWinApp->mMainView->DrawImage();
}


// This should not be called
void CHoleFinderDlg::ScanningCleanup(int error)
{
  StopScanning();
}

void CHoleFinderDlg::StopScanning(void)
{
  if (mMontage && mCurStore < 0)
    delete mImageStore;
  mFindingHoles = false;
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
  delete mMiniOffsets;
  mMiniOffsets = NULL;
}

// External call for drawing routine to find out whether to draw hole positions
bool CHoleFinderDlg::GetHolePositions(FloatVec **x, FloatVec **y, IntVec **pcOn,
  std::vector<bool> **exclude, BOOL &incl, BOOL &excl)
{
  *x = &mXstages;
  *y = &mYstages;
  *pcOn = &mPieceOn;
  *exclude = &mExcluded;
  incl = !mIsOpen || m_bShowIncluded;
  excl = mIsOpen ? m_bShowExcluded : mParams.showExcluded;
  return HaveHolesToDrawOrMakePts();
}

// External call to find out if there is anything to draw, used internally to see if
// points can be made
bool CHoleFinderDlg::HaveHolesToDrawOrMakePts()
{
  CMapDrawItem *item;
  if (CheckAndSetNav() || !mHaveHoles)
    return false;
  if (!mAddedGroupID)
    return true;
  item = mNav->GetOtherNavItem(mIndexOfGroupItem);
  if (!item || item->mGroupID != mAddedGroupID) {
    item = mNav->FindItemWithMapID(mAddedGroupID, false, true);
    mIndexOfGroupItem = mNav->GetFoundItem();
  }
  return !item;

}

// Set pointers and make sure Nav is open, message defaults to NULL
bool CHoleFinderDlg::CheckAndSetNav(const char *message) 
{ 
  mNav = mWinApp->mNavigator; 
  mHelper = mWinApp->mNavHelper;
  if (!mNav && message)
    SEMMessageBox(CString("The Navigator must be open to ") + message);
  return mNav == NULL; 
}
