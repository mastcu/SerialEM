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
#include "Utilities\XCorr.h"
#include "EMmontageController.h"
#include "HoleFinderDlg.h"
#include "AutoContouringDlg.h"
#include "NavHelper.h"
#include "MultiShotDlg.h"
#include "MultiHoleCombiner.h"
#include "Shared\holefinder.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"
#include "Shared\b3dutil.h"
#define FORMAT4OR5G(a) Format(((a) >= 1.e4 && (a) < 1.e5) ? "%.0f" : "%.4g", a)

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
  , m_strUmSizeSep(_T("um"))
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
  , m_bHexArray(FALSE)
  , m_intHullDist(0)
  , m_strHullDist(_T(""))
  , m_strMaxHullDist(_T(""))
  , m_bUseBestSubset(FALSE)
{
  mNonModal = true;
  mHaveHoles = false;
  mFindingHoles = false;
  mIsOpen = false;
  mMiniOffsets = NULL;
  mBestThreshInd = -1;
  mLastHoleSize = 0.;
  mLastHoleSpacing = 0.;
  mFindingFromDialog = false;
  mSkipAutoCor = false;
  mLastUserSelectInd = -1;
  mLastMagIndex = -1;
  for (int ind = 0; ind < 3; ind++) {
    mGridImXVecs[ind] = 0.;
    mGridImYVecs[ind] = 0.;
    mGridStageXVecs[ind] = 0.;
    mGridStageYVecs[ind] = 0.;
  }
  mPMMminPtsForSkipIfIS = 1;
}

CHoleFinderDlg::~CHoleFinderDlg()
{
  // Data are cleared by MainFrm on exit
}

void CHoleFinderDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_MM_FLOAT(pDX, IDC_EDIT_HOLE_SIZE, m_fHolesSize, 0.1f, 5, "Hole size");
  DDX_MM_FLOAT(pDX, IDC_EDIT_SPACING, m_fSpacing, 0.5f, 10, "Hole spacing");
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
  DDX_MM_FLOAT(pDX, IDC_EDIT_MAX_ERROR, m_fMaxError, 0.001f, 1.f, "Maximum error");
  DDX_Control(pDX, IDC_BUT_CLEAR_DATA, m_butClearData);
  DDX_Control(pDX, IDC_BRACKET_LAST, m_butBracketLast);
  DDX_Check(pDX, IDC_BRACKET_LAST, m_bBracketLast);
  DDX_Control(pDX, IDC_BUT_SET_SIZE_SPACE, m_butSetSizeSpace);
  DDX_Text(pDX, IDC_STAT_UM_SIZE_SEP, m_strUmSizeSep);
  DDX_Control(pDX, IDC_BUT_TOGGLE_HOLES, m_butToggleHoles);
  DDX_Control(pDX, IDC_HEX_ARRAY, m_butHexArray);
  DDX_Check(pDX, IDC_HEX_ARRAY, m_bHexArray);
  DDX_Control(pDX, IDC_SLIDER_HULL_DIST, m_sliderHullDist);
  DDX_Slider(pDX, IDC_SLIDER_HULL_DIST, m_intHullDist);
  DDV_MinMaxInt(pDX, m_intHullDist, 0, 255);
  DDX_Control(pDX, IDC_EDIT_HULL_DIST, m_editHullDist);
  DDX_Text(pDX, IDC_EDIT_HULL_DIST, m_strHullDist);
  DDX_Text(pDX, IDC_STAT_MAX_HULL_DIST, m_strMaxHullDist);
  DDX_Check(pDX, IDC_USE_BEST_SUBSET, m_bUseBestSubset);
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
  ON_NOTIFY(NM_LDOWN, IDC_BUT_TOGGLE_HOLES, OnToggleDraw)
  ON_BN_CLICKED(IDC_HEX_ARRAY, OnHexArray)
  ON_EN_KILLFOCUS(IDC_EDIT_HULL_DIST, OnKillfocusEditHullDist)
  ON_BN_CLICKED(IDC_USE_BEST_SUBSET, OnUseBestSubset)
END_MESSAGE_MAP()


// CHoleFinderDlg message handlers
BOOL CHoleFinderDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  CWnd *wnd = GetDlgItem(IDC_STAT_STATUS_OUTPUT);
  CheckAndSetNav();
  mIsOpen = true;
  mMasterParams = mHelper->GetHoleFinderParams();
  mParams = *mMasterParams;
  mBoldFont = mWinApp->GetBoldFont(wnd);
  m_sliderLowerMean.SetRange(0, 255);
  m_sliderUpperMean.SetRange(0, 255);
  m_sliderSDcutoff.SetRange(0, 255);
  m_sliderBlackPct.SetRange(0, 255);
  m_sliderHullDist.SetRange(0, 255);
  m_sliderLowerMean.SetPageSize(4);
  m_sliderUpperMean.SetPageSize(4);
  m_sliderSDcutoff.SetPageSize(4);
  m_sliderBlackPct.SetPageSize(4);
  m_sliderHullDist.SetPageSize(4);
  m_butToggleHoles.m_bNotifyOnDraws = true;
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
  UPDATE_DATA_TRUE;
  ManageSizeSeparation(true);
  mWinApp->RestoreFocusWhenIdle();
}


void CHoleFinderDlg::OnKillfocusEditSpacing()
{
  UPDATE_DATA_TRUE;
  ManageSizeSeparation(true);
  mWinApp->RestoreFocusWhenIdle();
}

void CHoleFinderDlg::OnKillfocusEditMaxError()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreFocusWhenIdle();
}

void CHoleFinderDlg::OnHexArray()
{
  UPDATE_DATA_TRUE;
  SizeAndSpacingToDialog(true, true);
  EnableDlgItem(IDC_USE_BEST_SUBSET, m_bHexArray);
  mWinApp->RestoreViewFocus();
}

// Handle boxes that can have lists.  Allow commas, replace by spaces
// Filter sigmas
void CHoleFinderDlg::OnKillfocusEditSigmas()
{
  UPDATE_DATA_TRUE;
  mWinApp->mParamIO->StringToEntryList(1, m_strSigmas, mNumSigmas,
    NULL, &mSigmas[0], MAX_HOLE_TRIALS, true);
  m_strSigmas = ListToString(1, mNumSigmas, &mSigmas[0]);
  mBestThreshInd = -1;
  ManageEnables();
  mWinApp->RestoreFocusWhenIdle();
}

// Iterations for median filter
void CHoleFinderDlg::OnKillfocusEditMedianIters()
{
  int ind, indOut = 1;
  UPDATE_DATA_TRUE;
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
  mWinApp->RestoreFocusWhenIdle();
}

// Thresholds
void CHoleFinderDlg::OnKillfocusEditThresholds()
{
  UPDATE_DATA_TRUE;
  mWinApp->mParamIO->StringToEntryList(1, m_strThresholds, mNumThresholds,
    NULL, &mThresholds[0], MAX_HOLE_TRIALS, true);
  m_strThresholds = ListToString(1, mNumThresholds, &mThresholds[0]);
  mBestThreshInd = -1;
  ManageEnables();
  mWinApp->RestoreFocusWhenIdle();
}

// Bracketing
void CHoleFinderDlg::OnBracketLast()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Boundary contour
void CHoleFinderDlg::OnExcludeOutside()
{
  UPDATE_DATA_TRUE;
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
  CLEAR_RESIZE(mXmissStage, float, 0);
  CLEAR_RESIZE(mYmissStage, float, 0);
  CLEAR_RESIZE(mXmissCen, float, 0);
  CLEAR_RESIZE(mYmissCen, float, 0);
  CLEAR_RESIZE(mMissPieceOn, int, 0);
  CLEAR_RESIZE(mMissXinPiece, float, 0);
  CLEAR_RESIZE(mMissYinPiece, float, 0);
  CLEAR_RESIZE(mExcluded, short, 0);
  CLEAR_RESIZE(mHexSubset, unsigned char, 0);
  CLEAR_RESIZE(mMissHexSubset, unsigned char, 0);
  CLEAR_RESIZE(mPieceOn, int, 0);
  mWinApp->mNavHelper->mFindHoles->clearAll();
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
  ManageSizeSeparation(false);
  UpdateData(false);
}

// Respond to notify event from the Toggle button; ignore the right-click
void CHoleFinderDlg::OnToggleDraw(NMHDR *pNotifyStruct, LRESULT *result)
{
  static bool lastState = false;
  if (!BOOL_EQUIV(lastState, m_butToggleHoles.m_bSelected)) {
    mWinApp->mMainView->DrawImage();
    lastState = m_butToggleHoles.m_bSelected;
    if (!lastState)
      mWinApp->mScope->SetRestoreViewFocusCount(1);
  }
}

// Edit boxes for mean limits: process string, set slider
void CHoleFinderDlg::OnKillfocusEditLowerMean()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreFocusWhenIdle();
  mParams.lowerMeanCutoff = (float)atof(m_strLowerMean);
  m_strLowerMean.FORMAT4OR5G(mParams.lowerMeanCutoff);
  m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mMeanMin) /
    (mMidMean - mMeanMin));
  B3DCLAMP(m_intLowerMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndDraw();
}

void CHoleFinderDlg::OnKillfocusEditUpperMean()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreFocusWhenIdle();
  mParams.upperMeanCutoff = (float)atof(m_strUpperMean);
  m_strUpperMean.FORMAT4OR5G(mParams.upperMeanCutoff);
  m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMidMean) /
    (mMeanMax - mMidMean));
  B3DCLAMP(m_intUpperMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndDraw();
}

void CHoleFinderDlg::OnKillfocusEditHullDist()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreFocusWhenIdle();
  mParams.edgeDistCutoff = (float)atof(m_strHullDist);
  m_strHullDist.Format("%.2f", mParams.edgeDistCutoff);
  m_intHullDist = (int)(255. * mParams.edgeDistCutoff / B3DMAX(1., mEdgeDistMax));
  B3DCLAMP(m_intHullDist, 0, 255);
  UpdateData(false);
  SetExclusionsAndDraw();
}

// Respond to slider changes: change actual cutoff in parameters as well as output slider
// value
void CHoleFinderDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
  bool takeNint = true;
  bool dropping = nSBCode != TB_THUMBTRACK;
  CWnd *wnd;
  UPDATE_DATA_TRUE;
  CSliderCtrl *pSlider = (CSliderCtrl *)pScrollBar;
  if (pSlider == &m_sliderLowerMean) {
    mParams.lowerMeanCutoff = (float)(m_intLowerMean * (mMidMean - mMeanMin) / 255. +
      mMeanMin);
    m_strLowerMean.FORMAT4OR5G(mParams.lowerMeanCutoff);
    wnd = GetDlgItem(IDC_STAT_LOW_MEAN_LABEL);
    wnd->SetFont(dropping ? m_statStatusOutput.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderUpperMean) {
    mParams.upperMeanCutoff = (float)(m_intUpperMean * (mMeanMax - mMidMean) / 255. +
      mMidMean);
    m_strUpperMean.FORMAT4OR5G(mParams.upperMeanCutoff);
    wnd = GetDlgItem(IDC_STAT_UP_MEAN_LABEL);
    wnd->SetFont(dropping ? m_statStatusOutput.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderSDcutoff) {
    mParams.SDcutoff = (float)(m_intSDcutoff * (mSDmax - mSDmin) / 255. + mSDmin);
    m_strSDcutoff.FORMAT4OR5G(mParams.SDcutoff);
    wnd = GetDlgItem(IDC_STAT_SD_LABEL);
    wnd->SetFont(dropping ? m_statStatusOutput.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderBlackPct) {
    mParams.blackFracCutoff = (float)(m_intBlackPct * (mBlackFracMax - mBlackFracMin) /
      255. + mBlackFracMin);
    m_strBlackPct.Format("%.1f", 100. * mParams.blackFracCutoff);
    wnd = GetDlgItem(IDC_STAT_PCT_LABEL);
    wnd->SetFont(dropping ? m_statStatusOutput.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderHullDist) {
    mParams.edgeDistCutoff = (float)(m_intHullDist * mEdgeDistMax / 255.);
    m_strHullDist.Format("%.2f", mParams.edgeDistCutoff);
    wnd = GetDlgItem(IDC_STAT_HULL_DIST_LABEL);
    wnd->SetFont(dropping ? m_statStatusOutput.GetFont() : mBoldFont);
  }
  UpdateData(false);
  SetExclusionsAndDraw();
  if (dropping)
    mWinApp->RestoreViewFocus();
  CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

// Other options
void CHoleFinderDlg::OnUseBestSubset()
{
  UPDATE_DATA_TRUE;
  mParams.useHexDiagonals = m_bUseBestSubset;
  mWinApp->RestoreViewFocus();
  SetExclusionsAndDraw();

  // Keep the nav map vectors up to date with the state of this setting
  if (mHaveHoles)
    SetNavMapHoleVectors();
}

void CHoleFinderDlg::OnShowIncluded()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}

void CHoleFinderDlg::OnShowExcluded()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}


void CHoleFinderDlg::OnRadioLayoutType()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Make navigator points - the real work is done by Nav
void CHoleFinderDlg::OnButMakeNavPts()
{
  mParams.layoutType = m_iLayoutType;
  DoMakeNavPoints(mParams.layoutType, mParams.lowerMeanCutoff, mParams.upperMeanCutoff,
    mParams.SDcutoff, mParams.blackFracCutoff, mParams.edgeDistCutoff,
    mParams.useHexDiagonals ? 1 : 0);
}

//Externally called routine.  Pass -1 to use the layout in params and EXTRA_NO_VALUE to
// use the cutoffs in params
int CHoleFinderDlg::DoMakeNavPoints(int layoutType, float lowerMeanCutoff, 
  float upperMeanCutoff, float sdCutoff, float blackCutoff, float edgeDistCutoff,
  int useHexDiagonals)
{
  CMapDrawItem *poly = NULL;
  ShortVec gridX, gridY;
  int ind, numAdded = 0;
  bool anyNonDflt = false;
  float avgLen = -1., avgAngle = -999.;
  float incStageX1, incStageY1, incStageX2, incStageY2;
  if (CheckAndSetNav("making Navigator points from hole positions"))
    return -1;
  if (!HaveHolesToDrawOrMakePts()) {
    SEMMessageBox("There are no hole positions available for making Navigator points");
    return -1;
  }

  // Use the set parameters if alternatives not passed, and run exclusion again
  if (lowerMeanCutoff < EXTRA_VALUE_TEST)
    lowerMeanCutoff = mParams.lowerMeanCutoff;
  else
    anyNonDflt = true;
  if (upperMeanCutoff < EXTRA_VALUE_TEST)
    upperMeanCutoff = mParams.upperMeanCutoff;
  else
    anyNonDflt = true;
  if (sdCutoff < 0.)
    sdCutoff = mParams.SDcutoff;
  else
    anyNonDflt = true;
  if (blackCutoff < 0.)
    blackCutoff = mParams.blackFracCutoff;
  else
    anyNonDflt = true;
  if (edgeDistCutoff < 0)
    edgeDistCutoff = mParams.edgeDistCutoff;
  else
    anyNonDflt = true;
  if (layoutType < 0)
    layoutType = mParams.layoutType;
  if (useHexDiagonals < 0)
    useHexDiagonals = mParams.useHexDiagonals ? 1 : 0;
  SetExclusionsAndDraw(lowerMeanCutoff, upperMeanCutoff, sdCutoff, blackCutoff, 
    edgeDistCutoff, useHexDiagonals != 0);
  if (mIsOpen) {
    UpdateData(true);
    DialogToParams();
    *mMasterParams = mParams;
    mWinApp->RestoreViewFocus();
  }

  // Add up number to add, return if none left
  for (ind = 0; ind < (int)mExcluded.size(); ind++)
    if (mExcluded[ind] <= 0)
      numAdded++;
  if (!numAdded) {
    if (anyNonDflt)
      SetExclusionsAndDraw();
    return 0;
  }

  if (mBoundPolyID)
    poly = mNav->FindItemWithMapID(mBoundPolyID, false);

  // Get the grid positions and pass it all on
  // Note that the image positions here are Y-inverted, so a positive angle is in the
  // lower right of image, and 90 plus that is in lower left
  // Thus compute the stage vectors of the grid axes here and pass that in too
  mHelper->mFindHoles->assignGridPositions(mXcenters, mYcenters, gridX, gridY, avgAngle,
    avgLen);
  ApplyScaleMatrix(mImToStage, 
    avgLen * (float)cos(avgAngle * DTOR),
    avgLen * (float)sin(avgAngle * DTOR), incStageX1, incStageY1);
  ApplyScaleMatrix(mImToStage, 
    avgLen * (float)cos(DTOR *(avgAngle + 90.)),
    avgLen * (float)sin(DTOR * (avgAngle + 90.)), incStageX2, incStageY2);
  mAddedGroupID = mNav->AddFoundHoles(&mXstages, &mYstages, &mExcluded,
    &mXinPiece, &mYinPiece, &mPieceOn, &gridX, &gridY, avgLen * mPixelSize,
    2.f * mBestRadius * mReduction * mPixelSize, incStageX1, incStageY1, incStageX2,
    incStageY2, mRegistration, mMapID, (float)mZstage, poly, layoutType);
  if (mAddedGroupID) {
    mNav->FindItemWithMapID(mAddedGroupID, false, true);
    mIndexOfGroupItem = mNav->GetFoundItem();
    mWinApp->mMainView->DrawImage();
    if (mIsOpen)
      m_butMakeNavPts.EnableWindow(false);
    if (mPieceOn.size() && *std::min_element(mPieceOn.begin(), mPieceOn.end()) < 
      *std::max_element(mPieceOn.begin(), mPieceOn.end())) {
      int minpc = 1000;
      int maxpc = -1000;
      for (ind = mIndexOfGroupItem; ind < mIndexOfGroupItem + numAdded; ind++) {
        poly = mNav->GetOtherNavItem(ind);
        if (poly) {
          ACCUM_MIN(minpc, poly->mPieceDrawnOn);
          ACCUM_MAX(maxpc, poly->mPieceDrawnOn);
        }
      }
      if (minpc >= maxpc)
        PrintfToLog("WARNING: \"PieceDrawnOn\" values are wrong after making items "
          "(min %d, max %d)", minpc, maxpc);
    }
  }
   if (anyNonDflt)
     SetExclusionsAndDraw();
   return mAddedGroupID ? numAdded : 0;
}

// Transfer values from the dialog to the parameters
void CHoleFinderDlg::DialogToParams()
{
  int ind;
  if (!mIsOpen)
    return;
  mParams.useBoundary = m_bExcludeOutside;
  mParams.useHexDiagonals = m_bUseBestSubset;
  mParams.showExcluded = m_bShowExcluded;
  mParams.layoutType = m_iLayoutType;
  mParams.hexagonalArray = m_bHexArray;
  SizeAndSpacingToParam(false);
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

void CHoleFinderDlg::SyncToMasterParams()
{
  if (!mIsOpen)
    return;
  UPDATE_DATA_TRUE;
  DialogToParams();
  *mMasterParams = mParams;
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
    m_strSDcutoff.FORMAT4OR5G(mParams.SDcutoff);
    m_strBlackPct.Format("%.1f", 100. * mParams.blackFracCutoff);
    m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mMeanMin) /
      (mMidMean - mMeanMin));
    m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMidMean) /
      (mMeanMax - mMidMean));
    m_intSDcutoff = (int)(255. * (mParams.SDcutoff - mSDmin) / (mSDmax -mSDmin));
    m_intBlackPct = (int)(255. * (mParams.blackFracCutoff - mBlackFracMin) / 
      (mBlackFracMax - mBlackFracMin));
    m_intHullDist = (int)(255. * mParams.edgeDistCutoff / B3DMAX(1., mEdgeDistMax));
    B3DCLAMP(m_intLowerMean, 0, 255);
    B3DCLAMP(m_intUpperMean, 0, 255);
    B3DCLAMP(m_intSDcutoff, 0, 255);
    B3DCLAMP(m_intBlackPct, 0, 255);
    B3DCLAMP(m_intHullDist, 0, 255);
  } else {
    m_strSDcutoff = "";
    m_strBlackPct = "";
    m_intLowerMean = 0;
    m_intUpperMean = 255;
    m_intSDcutoff = 255;
    m_intBlackPct = 255;
    m_intHullDist = 0;
  }
  m_strLowerMean.FORMAT4OR5G(mParams.lowerMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.lowerMeanCutoff : 0.);
  m_strUpperMean.FORMAT4OR5G(mParams.upperMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.upperMeanCutoff : 0.);
  m_strHullDist.Format("%.2f", mParams.edgeDistCutoff);
  m_bHexArray = mParams.hexagonalArray;
  SizeAndSpacingToDialog(false, false);
  m_fMaxError = mParams.maxError;
  m_bExcludeOutside = mParams.useBoundary;
  m_bBracketLast = mParams.bracketLast;
  m_bUseBestSubset = mParams.useHexDiagonals;
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
    m_strMinLowerMean.FORMAT4OR5G(mMeanMin);
    m_strMaxLowerMean.FORMAT4OR5G(mMidMean);
    m_strMinUpperMean.FORMAT4OR5G(mMidMean);
    m_strMaxUpperMean.FORMAT4OR5G(mMeanMax);
    m_strMinSDcutoff.FORMAT4OR5G(mSDmin);
    m_strMaxSDcutoff.FORMAT4OR5G(mSDmax);
    m_strMinBlackPct.Format("%.1f", 100. * mBlackFracMin);
    m_strMaxBlackPct.Format("%.1f", 100. * mBlackFracMax);
    m_strMaxHullDist.Format("%.1f", mEdgeDistMax);
  } else {
    m_strMinLowerMean = "";
    m_strMaxLowerMean = "";
    m_strMinUpperMean = "";
    m_strMaxUpperMean = "";
    m_strMinSDcutoff = "";
    m_strMaxSDcutoff = "";
    m_strMinBlackPct = "";
    m_strMaxBlackPct = "";
    m_strMaxHullDist = "";
  }
  m_butBracketLast.EnableWindow(mBestThreshInd >= 0);
  EnableDlgItem(IDC_USE_BEST_SUBSET, m_bHexArray);
  m_butShowExcluded.EnableWindow(mHaveHoles);
  m_butShowIncluded.EnableWindow(mHaveHoles);
  m_butToggleHoles.EnableWindow(mHaveHoles);
  m_butZigzag.EnableWindow(mHaveHoles);
  m_butFromFocus.EnableWindow(mHaveHoles);
  m_butInGroups.EnableWindow(mHaveHoles && mHelper->GetGridGroupSize() > 0);
  m_butMakeNavPts.EnableWindow(HaveHolesToDrawOrMakePts() && !mNav->mNavAcquireDlg);
  UpdateData(false);
}

void CHoleFinderDlg::ManageSizeSeparation(bool update)
{
  m_strUmSizeSep.Format("um (%.1f/%.1f)", m_fHolesSize, m_fSpacing - m_fHolesSize);
  if (update)
    UpdateData(false);
}

// Place the appropriate size and spacing for grid type into the dialog, optionally saving
// the existing values for other type
void CHoleFinderDlg::SizeAndSpacingToDialog(bool saveOther, bool update)
{
  if (saveOther) {
    SizeAndSpacingToParam(true);
  }
  if (m_bHexArray) {
    m_fHolesSize = mParams.hexDiameter;
    m_fSpacing = mParams.hexSpacing;
  } else {
    m_fHolesSize = mParams.diameter;
    m_fSpacing = mParams.spacing;
  }
  ManageSizeSeparation(update);
}

// Save size and spacing for the current grid type, or the other one if flag set
void CHoleFinderDlg::SizeAndSpacingToParam(bool other)
{
  BOOL which = m_bHexArray;
  if (other)
    which = !which;
  if (which) {
    mParams.hexDiameter = m_fHolesSize;
    mParams.hexSpacing = m_fSpacing;
  } else {
    mParams.diameter = m_fHolesSize;
    mParams.spacing = m_fSpacing;
  }
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
  mFindingFromDialog = true;
  DoFindHoles(mWinApp->mMainView->GetActiveImBuf(), false);
  mFindingFromDialog = false;
}

int CHoleFinderDlg::DoFindHoles(EMimageBuffer *imBuf, bool synchronous)
{
  float curDiam, diamReduced, spacingReduced, maxRadius, area, minArea = 1.e30f;
  FloatVec *widths, *increments;
  IntVec *numCircles;
  FloatVec xBoundTemp, yBoundTemp;
  MapItemArray *itemArray;
  MontParam *masterMont = mWinApp->GetMontParam();
  KImage *image;
  CMapDrawItem *item;
  int reloadBinning, overBinSave, readBuf, err, numScans, ind, nav, minDrawnID = 0, bufID;
  int numMedians, numOnIm, sigStart, sigEnd, iterStart, iterEnd;
  float useWidth, useHeight, ptX, ptY, delX, delY, bufStageX, bufStageY, useDiameter;
  float autocorSpacing, testSpacing, vectors[6], autocorCrit = 0.14f;
  double tstageX, tstageY;
  ScaleMat rMat, rInv, aMat;
  MontParam *montP = &mMontParam;
  float targetDiam = mHelper->GetHFtargetDiamPix();
  float tooSmallCrit = 0.8f;
  float minFracPtsOnImage = 0.74f;
  BOOL convertSave, loadUnbinSave, hexArray;
  CString noMontReason, boundLabel, mess;

  if (!synchronous && CheckAndSetNav("finding holes"))
    return 1;

  // Initialize by clearing out existing hole info
  mMontage = 0;
  mXboundary.clear();
  mYboundary.clear();
  mXinPiece.clear();
  mMissPieceOn.clear();
  mMissXinPiece.clear();
  mMissYinPiece.clear();
  mPieceOn.clear();
  mExcluded.clear();
  mHexSubset.clear();
  mMissHexSubset.clear();
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
  hexArray = mParams.hexagonalArray;
  useDiameter = hexArray ? mParams.hexDiameter : mParams.diameter;
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
  mBufBinning = imBuf->mBinning;
  //if (mWinApp->mProcessImage->GetTestCtfPixelSize())
  //mPixelSize = 0.001f * mWinApp->mProcessImage->GetTestCtfPixelSize() * imBuf->mBinning;
  if (!mPixelSize) {
    SEMMessageBox("No pixel size is assigned to this image; it is needed for finding "
      "holes", MB_EXCLAME);
    return 1;
  }
  if (image->getType() == kRGB) {
    SEMMessageBox("Hole finder does not work on color images");
    return 1;
  }
  if (mNav && (!mNav->BufferStageToImage(imBuf, aMat, delX, delY) ||
    !imBuf->GetStagePosition(bufStageX, bufStageY))) {
    SEMMessageBox("The currently active image does not have enough information\n"
      "to convert positions to stage coordinates.", MB_EXCLAME);
    return 1;
  }

  if ((!hexArray && mParams.spacing < mParams.diameter + 0.5f) || 
    (hexArray && mParams.hexSpacing < mParams.hexDiameter + 0.15f)) {
    SEMMessageBox("The hole spacing is a center-to-center distance and must be at least "
      + CString(hexArray ? "0.15" : "0.5") + " micron bigger than the hole size");
    return 1;
  }
  mAdjustedStageToCam = mWinApp->mShiftManager->FocusAdjustedStageToCamera(imBuf);

  testSpacing = hexArray ? mParams.hexSpacing : mParams.spacing;
  autocorSpacing = testSpacing;
  if (mFindingFromDialog && !mSkipAutoCor && 
    image->getWidth() / testSpacing >= 4. && 
    image->getHeight() / testSpacing >= 4.) {
    if (!mWinApp->mProcessImage->FindPixelSize(0., 0., 0., 0., mBufInd,
      FIND_ACPK_NO_WAFFLE | FIND_PIX_NO_DISPLAY | FIND_PIX_NO_TARGET | 
      (hexArray ? FIND_ACPK_HEX_GRID : 0), autocorSpacing, vectors)) {
      autocorSpacing *= mPixelSize;
      delX = fabsf(testSpacing - autocorSpacing) / testSpacing;
      if (delX > autocorCrit) {
        mess.Format("Autocorrelation peak analysis indicates that the spacing\n"
          "between hole centers is %.2f microns, %.0f%% from the entered value\n\n"
          "You may need to measure hole spacing and size directly in this\n"
          "image (use Shift - Left mouse button to draw lines)"
          "\n\nPress:\n\"Stop & Check\" to check the size and spacing,"
          "\n\n\"Go On\" to proceed with your entered values,"
          "\n\n\"Skip Analysis\" to proceed and skip this peak analysis in the future",
          autocorSpacing, 100. * delX);
        ind = SEMThreeChoiceBox(mess, "Stop && Check", "Go On", "Skip Analysis",
          MB_YESNOCANCEL);
        if (ind == IDYES)
          return 1;
        mSkipAutoCor = ind == IDCANCEL;
      }
    }
  }

  // Get some sizes, get a nav item regardless of montage so it can be used as drawn on
  // and for registration and stage Z
  curDiam = useDiameter / mPixelSize;
  mFullYsize = image->getHeight();

  mNavItem = mNav ? mNav->FindItemWithMapID(imBuf->mMapID) : NULL;

  // Get map ID to assign as drawn on ID when holes are made, or negative of mag index if
  // it is non-map buffer
  mMapID = 0;
  if (mNavItem) {
    mZstage = mNavItem->mStageZ;
    mMapID = mNavItem->mMapID;
  } else if (imBuf->mMagInd > 0)
    mMapID = -imBuf->mMagInd;

  convertSave = mHelper->GetConvertMaps();
  readBuf = mWinApp->mBufferManager->GetBufToReadInto();

  // Montaging: check preconditions first
  if (!synchronous && imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW) {
    mMontage = -1;
    if (!imBuf->mMapID)
      noMontReason = "the buffer has no ID for finding its Navigator map";
    else if (!mNavItem)
      noMontReason = "could not find the map item from the ID in the buffer";
    if (mNavItem && mWinApp->mNavHelper->PrepareMontAdjustments(imBuf, rMat, rInv, ptX,
      ptY) > 0)
      noMontReason = "the map is rotated";
    else if (mNavItem && mNav->AccessMapFile(mNavItem, mImageStore,
      mCurStore, montP, useWidth, useHeight))
      noMontReason = "could not access the map file again";
    if (noMontReason.IsEmpty()) {
      mMontage = 1;

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
        loadUnbinSave = mHelper->GetLoadMapsUnbinned();
        if (mCurStore < 0)
          delete mImageStore;
        masterMont->overviewBinning = reloadBinning;
        mHelper->SetConvertMaps(false);
        mHelper->SetLoadMapsUnbinned(false);

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
          // This is deadly, uncomment to use it!
          /*if (mWinApp->mProcessImage->GetTestCtfPixelSize())
            mPixelSize = 0.001f * mWinApp->mProcessImage->GetTestCtfPixelSize() * 
            imBuf->mBinning;*/
          if (!mPixelSize) {
            SEMMessageBox("The image has no pixel size after reloading for finding "
              "holes");
            return 1;
          }
          curDiam = useDiameter / mPixelSize;
          if (mNav->AccessMapFile(mNavItem, mImageStore, mCurStore, montP,
            useWidth, useHeight)) {
            noMontReason = "could not access the map file again after reloading it";
          }
        }
      }

      // Save the overview binning and the minioffset so the imBuf does not need to be
      // accessed again
      mFullBinning = imBuf->mOverviewBin;
      mBufBinning = imBuf->mBinning;
      mMontage = montP->xNframes * montP->yNframes > 1 ? 1 : 0;
      if (mMontage > 0 && imBuf->mMiniOffsets) {
        mMiniOffsets = new MiniOffsets;
        *mMiniOffsets = *imBuf->mMiniOffsets;
      }
    }

    // But single frame byte images need to be reloaded too
  } else if (image->getType() == kUBYTE && mNavItem) {
    mHelper->SetConvertMaps(false);
    mWinApp->mBufferManager->SetBufToReadInto(mBufInd);
    err = mNav->DoLoadMap(true, mNavItem, -1);
    mHelper->SetConvertMaps(convertSave);
    mWinApp->mBufferManager->SetBufToReadInto(readBuf);
    if (err) {
      SEMMessageBox("Could not reload map to work with it as non-bytes");
      return 1;
    }
    image = imBuf->mImage;
  }

  mLastTiltAngle = 0.;
  imBuf->GetTiltAngle(mLastTiltAngle);
  mLastMagIndex = imBuf->mMagInd;

  // Report failure, but go on
  if (mMontage && !noMontReason.IsEmpty()) {
    mWinApp->AppendToLog("Cannot analyze montage pieces when finding holes: " + 
      noMontReason);
    mMontage = -1;
  }

  // Copy the montage params to our copy if the pointer got replaced with one from an
  // open file
  if (mMontage && montP != &mMontParam)
    mMontParam = *montP;

  mRegistration = mNav ? mNav->GetCurrentRegistration() : 1;
  if (mNavItem)
    mRegistration = mNavItem->mRegistration;

  // Look for boundary contour if selected
  if (!synchronous && mParams.useBoundary) {
    itemArray = mNav->GetItemArray();
    mNav->BufferStageToImage(imBuf, aMat, delX, delY);
    bufID = imBuf->mMapID;
    for (nav = 0; nav < (int)itemArray->GetSize(); nav++) {
      item = itemArray->GetAt(nav);

      // For polygon at same registration
      // transform points to image and count how many are on the image
      if (item->IsPolygon() && item->mRegistration == mRegistration) {
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

        // If enough are on image, get the area, and if it is the smallest such, take it
        // But give preference to polygon drawn on map
        if ((float)numOnIm / item->mNumPoints >= minFracPtsOnImage) {
          area = mNav->ContourArea(item->mPtX, item->mPtY, item->mNumPoints);
          if (minDrawnID != bufID && item->mDrawnOnMapID == bufID || 
            (area < minArea && (!bufID || 
              !(minDrawnID == bufID && item->mDrawnOnMapID != bufID)))) {
            minArea = area;
            minDrawnID = item->mDrawnOnMapID;
            mXboundary = xBoundTemp;
            mYboundary = yBoundTemp;
            mBoundPolyID = item->mMapID;
            boundLabel = item->mLabel;
          }
        }
      }
    }
    if (!boundLabel.IsEmpty())
      PrintfToLog("Points will be retained inside the polygon with label %s", 
      (LPCTSTR)boundLabel);
  }

  numScans = (int)widths->size();
  ACCUM_MIN(numScans, (int)numCircles->size());
  ACCUM_MIN(numScans, (int)increments->size());

  // Get the reduction
  mReduction = B3DMAX(1.f, curDiam / targetDiam);
  diamReduced = curDiam / mReduction;
  spacingReduced = (testSpacing / mPixelSize) / mReduction;

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
    SEMMessageBox(CString("Error in initializing holefinder routine:\n") +
      mHelper->mFindHoles->returnErrorString(err), MB_EXCLAME);
    StopScanning();
    return 1;
  }

  // Set up the parameters and call to start the scan
  mHelper->mFindHoles->setSequenceParams(diamReduced, spacingReduced,
    hexArray == TRUE, mHelper->GetHFretainFFTs() != 0, mErrorMax,
    mHelper->GetHFfractionToAverage(),
    mHelper->GetHFminNumForTemplate(), mHelper->GetHFmaxDiamErrFrac(),
    mHelper->GetHFavgOutlieCrit(), mHelper->GetHFfinalNegOutlieCrit(),
    mHelper->GetHFfinalPosOutlieCrit(), mHelper->GetHFfinalOutlieRadFrac());
  mHelper->mFindHoles->setRunsInSequence(&(*increments)[0], &(*widths)[0],
    &(*numCircles)[0], numScans, &mUseSigmas[0], (int)mUseSigmas.size(),
    &mUseThresholds[0], (int)mUseThresholds.size());
  mFindingHoles = true;
  mSigInd = 0;
  mThreshInd = 0;
  if (!synchronous) {
    mWinApp->SetStatusText(SIMPLE_PANE, "FINDING HOLES");
    mWinApp->UpdateBufferWindows();
  }
  ScanningNextTask(synchronous ? -999 : 0);
  return 0;
}

// Next task for scanning is to run the next set of parameters
void CHoleFinderDlg::ScanningNextTask(int param)
{
  int err, ind;
  int cen, bestCen = -1, maxCenNum = 0;
  int subsetXcen[3] = {0, 1, 0};
  int subsetYcen[3] = {0, 0, 1};
  ShortVec gridX, gridY;
  float sigUsed, threshUsed;
  FloatVec peakVals, xMissing, yMissing, xCenClose, yCenClose, xBound, yBound;
  FloatVec peakClose, xInPiece, yInPiece, xCenAlt, yCenAlt, peakAlt, xMissing2, yMissing2;
  float avgAngle, avgLen, anSpacing;
  IntVec pieceSavedAt, tileXnum, tileYnum, pieceIndex, secIxAliPiece, secIyAliPiece;
  IntVec fileZindex, altInds2;
  FloatVecArray pieceXcenVec, pieceYcenVec, piecePeakVec, pieceMeanVec, pieceSDsVec;
  FloatVecArray pieceOutlieVec;
  MontParam *montP = &mMontParam;
  EMimageBuffer *allBufs = mWinApp->GetImBufs();
  EMimageBuffer *imBuf = &allBufs[mBufInd];
  int numMissAdded, numPcInSec, ixpc, iypc, ipc, readBuf, numMiss, xGrid, yGrid, subset;
  int xcoord, ycoord, zcoord, numFromCorr, numPoints;
  ScaleMat aMat, aInv, adjInv;
  float delX, delY, ptX, ptY;
  bool syncForOneHole = param == -999;
  CString noMontReason, statStr;

  if (!mFindingHoles)
    return;
  if (!syncForOneHole && CheckAndSetNav()) {
    StopScanning();
    return;
  }
  
  for ( ;; ) {
    err = mHelper->mFindHoles->runSequence(mSigInd, sigUsed, mThreshInd,
      threshUsed, mXboundary, mYboundary, mBestSigInd, mBestThreshInd,
      mBestRadius, mTrueSpacing, mXcenters, mYcenters,
      peakVals, xMissing, yMissing, xCenClose, yCenClose, peakClose,
      numMissAdded);
    if (err < 0 && !syncForOneHole) {
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
    if (err >= 0)
      break;
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

  if (mMontage > 0 && !syncForOneHole) {
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
      int minBefore = 1000, maxBefore = -1000, minAfter = 1000, maxAfter = -1000;
      for (ind = 0; ind < (int)mPieceOn.size(); ind++) {
        ACCUM_MIN(minBefore, mPieceOn[ind]);
        ACCUM_MAX(maxBefore, mPieceOn[ind]);
        xcoord = mPieceOn[ind] / montP->yNframes;
        ycoord = mPieceOn[ind] % montP->yNframes;
        mPieceOn[ind] = (montP->yNframes - 1) - ycoord + xcoord * montP->yNframes;
        ACCUM_MIN(minAfter, mPieceOn[ind]);
        ACCUM_MAX(maxAfter, mPieceOn[ind]);
      }
      if (montP->xNframes * montP->yNframes > 3 && mPieceOn.size() > 40) {
        if (minBefore == maxBefore)
          PrintfToLog("WARNING: The \"pieceOn\" value before inversion is the same for"
            " all points, %d", minBefore);
        else if (minAfter == maxAfter)
          PrintfToLog("WARNING: The \"pieceOn\" value after inversion is the same for"
            " all points, %d", minBefore);
      }
    }

  } else {
    mPieceOn.clear();
    mPieceOn.resize(mXcenters.size(), -1);
    if (mMontage) {
      mXinPiece.resize(mXcenters.size());
      mYinPiece.resize(mXcenters.size());
    }
  }

  // OUTPUT summary line and skip if nothing
  numPoints = (int)mXcenters.size();
  mExcluded.resize(numPoints, 0);
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
  mLastWasHexGrid = mParams.hexagonalArray;
  statStr.Format("%s  thr: %.1f  #: %d  size: %.2f  per: %.2f",
    formatSigma(mParams.sigmas[mBestSigInd]), mParams.thresholds[mBestThreshInd],
    numPoints, mLastHoleSize, mLastHoleSpacing);
  if (mIsOpen)
    m_strStatusOutput = statStr;
  else
    mWinApp->AppendToLog("Hole finder: " + statStr);

  mHaveHoles = true;

  // Get the grid vectors and convert them to stage coordinates.  Vectors are right-handed
  mHelper->mFindHoles->analyzeNeighbors(mXcenters, mYcenters, peakVals, altInds2, xCenAlt,
    yCenAlt, peakAlt, 0., 0., 0, anSpacing, xMissing2, yMissing2);
  mHelper->mFindHoles->getGridVectors(mGridImXVecs, mGridImYVecs, avgAngle, avgLen, -1);
  adjInv = MatInv(mAdjustedStageToCam);
  for (ind = 0; ind < (mLastWasHexGrid ? 3 : 2); ind++)
    ApplyScaleMatrix(adjInv, mBufBinning * mGridImXVecs[ind],
    -mBufBinning * mGridImYVecs[ind], mGridStageXVecs[ind], mGridStageYVecs[ind]);

// Get stage positions, save matrix for converting average vector later
  if (!mNav)
    return;

  mNav->BufferStageToImage(imBuf, aMat, delX, delY);
  aInv = MatInv(aMat);
  mImToStage = aInv;
  mXstages.resize(numPoints);
  mYstages.resize(numPoints);
  rsMedian(&mHoleMeans[0], numPoints, &mXstages[0], &mMidMean);
  for (ind = 0; ind < numPoints; ind++) {
    ptX = mXcenters[ind];
    ptY = mYcenters[ind];

    // It turns out that the pieceOn values from above are not quite right or consistent
    // with what is needed for proper display, so replace them
    if (mMontage && mPieceOn.size())
      mWinApp->mNavHelper->AdjustMontImagePos(imBuf, ptX, ptY, &mPieceOn[ind], 
        &mXinPiece[ind], &mYinPiece[ind]);
    ApplyScaleMatrix(aInv, ptX - delX, ptY - delY, mXstages[ind],
      mYstages[ind]);
  }

  // Treat the missing points the same way so they can be added by mouse
  numMiss = (int)xMissing.size();
  if (numMiss) {
    mXmissStage.resize(numMiss);
    mYmissStage.resize(numMiss);
    mXmissCen = xMissing;
    mYmissCen = yMissing;
    if (mMontage && mPieceOn.size()) {
      mMissPieceOn.resize(numMiss);
      mMissXinPiece.resize(numMiss);
      mMissYinPiece.resize(numMiss);
    }
  }
  for (ind = 0; ind < numMiss; ind++) {
    ptX = xMissing[ind];
    ptY = yMissing[ind];

    if (mMontage && mPieceOn.size())
      mWinApp->mNavHelper->AdjustMontImagePos(imBuf, ptX, ptY, &mMissPieceOn[ind], 
        &mMissXinPiece[ind], &mMissYinPiece[ind]);
    ApplyScaleMatrix(aInv, ptX - delX, ptY - delY, mXmissStage[ind],
      mYmissStage[ind]);
  }

  // Get the hole vectors as IS values and store in Nav item if it is a map
  mIDofNavItem = 0;
  mCurHexSubset = -1;
  SetNavMapHoleVectors();

  // Get edge distances
  xBound.resize(numPoints);
  yBound.resize(numPoints);
  mHoleEdgeDists.clear();
  mHoleEdgeDists.resize(numPoints, 0);
  CAutoContouringDlg::FindDistancesFromHull(mXstages, mYstages, xBound, yBound,
    1., mHoleEdgeDists);

  // Get mins and maxes and set the sliders if necessary
  mMeanMin = *std::min_element(mHoleMeans.begin(), mHoleMeans.end());
  mMeanMax = *std::max_element(mHoleMeans.begin(), mHoleMeans.end());
  mSDmin = *std::min_element(mHoleSDs.begin(), mHoleSDs.end());
  mSDmax = *std::max_element(mHoleSDs.begin(), mHoleSDs.end());
  mBlackFracMin = *std::min_element(mHoleBlackFracs.begin(), mHoleBlackFracs.end());
  mBlackFracMax = *std::max_element(mHoleBlackFracs.begin(), mHoleBlackFracs.end());
  mEdgeDistMax = *std::max_element(mHoleEdgeDists.begin(), mHoleEdgeDists.end());
  if (mParams.lowerMeanCutoff < EXTRA_VALUE_TEST)
    mParams.lowerMeanCutoff = mMeanMin;
  if (mParams.upperMeanCutoff < EXTRA_VALUE_TEST)
    mParams.upperMeanCutoff = mMeanMax;
  if (mParams.SDcutoff  < EXTRA_VALUE_TEST)
    mParams.SDcutoff = mSDmax;
  if (mParams.blackFracCutoff  < EXTRA_VALUE_TEST)
    mParams.blackFracCutoff = mBlackFracMax;
  if (!mEdgeDistMax)
    mParams.edgeDistCutoff = 0.;
  m_bShowIncluded = true;
  ParamsToDialog();
  ManageEnables();
  if (mWinApp->mNavHelper->mMultiShotDlg)
    mWinApp->mNavHelper->mMultiShotDlg->ManageEnables();
  if (!mIsOpen)
    *mMasterParams = mParams;

  // Analyze hex subset for real and missing points
  if (mLastWasHexGrid) {

    // Concatenate real and missing points
    xMissing = mXcenters;
    yMissing = mYcenters;
    if (mXmissStage.size()) {
      xMissing.insert(xMissing.end(), mXmissCen.begin(), mXmissCen.end());
      yMissing.insert(yMissing.end(), mYmissCen.begin(), mYmissCen.end());
    }

    // Get the grid positions
    mHexSubset.resize(mXcenters.size());
    mMissHexSubset.resize(mXmissCen.size());
    avgLen = -1.; 
    avgAngle = -999.;
    mHelper->mFindHoles->assignGridPositions(xMissing, yMissing, gridX, gridY, avgAngle,
      avgLen);

    // For each center position, see if difference from center satisfied relation for
    // subset that X - Y is a multiple of 3
    for (ind = 0; ind < (int)xMissing.size(); ind++) {
      xGrid = gridX[ind];
      yGrid = gridY[ind];
      subset = -1;
      for (cen = 0; cen < 3 && subset < 0; cen++) {
        xGrid = gridX[ind] - subsetXcen[cen];
        yGrid = gridY[ind] - subsetYcen[cen];
        if ((xGrid - yGrid) % 3 == 0) {
          subset = cen;
          break;
        }
      }

      // Assign the subset #
      if (ind >= (int)mXcenters.size())
        mMissHexSubset[ind - mXcenters.size()] = subset;
      else
        mHexSubset[ind] = subset;
    }
  }
  SetExclusionsAndDraw();
  if (mParams.useHexDiagonals)
    SetNavMapHoleVectors();
}

void CHoleFinderDlg::SetExclusionsAndDraw()
{
  SetExclusionsAndDraw(mParams.lowerMeanCutoff, mParams.upperMeanCutoff, mParams.SDcutoff,
    mParams.blackFracCutoff, mParams.edgeDistCutoff, mParams.useHexDiagonals);
}

// Set the exclusions based on the current cutoffs
void CHoleFinderDlg::SetExclusionsAndDraw(float lowerMeanCutoff, float upperMeanCutoff,
  float sdCutoff, float blackCutoff, float edgeDistCutoff, BOOL useHexDiagonals)
{
  int ind, numInSubset[3] = {0, 0, 0};
  bool extreme;
  float middle = (lowerMeanCutoff + upperMeanCutoff) / 2.f;
  if (!mHaveHoles)
    return;
  for (ind = 0; ind < (int)mXcenters.size(); ind++) {

    // -1 and 3 are user include and exclude and are permanent; 1 and 2 are for "dark"
    // and "light" exclusions.  -1 becomes -2 and 3 becomes 5 for points in excluded
    // hex subsets, so first restore those values for the regular exclusion logic
    if (mExcluded[ind] < -1)
      mExcluded[ind] = -1;
    if (mExcluded[ind] > 4)
      mExcluded[ind] = 3;
    if (mExcluded[ind] != -1 && mExcluded[ind] != 3) {
      mExcluded[ind] = 0;
      extreme = mHoleSDs[ind] > sdCutoff || mHoleBlackFracs[ind] > blackCutoff;
      if (mHoleMeans[ind] < lowerMeanCutoff || (extreme && mHoleMeans[ind] <= middle) ||
        mHoleEdgeDists[ind] < edgeDistCutoff)
        mExcluded[ind] = 1;
      if (mHoleMeans[ind] > upperMeanCutoff || (extreme && mHoleMeans[ind] > middle))
        mExcluded[ind] = 2;
    }
  }

  // Find best hex subset: first count up non-excluded points in each subset and get best
  mCurHexSubset = -1;
  if (mLastWasHexGrid && useHexDiagonals) {
    for (ind = 0; ind < (int)mXcenters.size(); ind++)
      if ((!mExcluded[ind] || mExcluded[ind] == -1) && mHexSubset[ind] >= 0)
        numInSubset[mHexSubset[ind]]++;
    mCurHexSubset = 0;
    if (numInSubset[1] > numInSubset[0])
      mCurHexSubset = 1;
    if (numInSubset[2] > numInSubset[mCurHexSubset])
      mCurHexSubset = 2;

    // Set exclusion 4 for other points, or -2/5 for user include/excludes
    for (ind = 0; ind < (int)mXcenters.size(); ind++) {
      if (mHexSubset[ind] != mCurHexSubset) {
        if (mExcluded[ind] < 0)
          mExcluded[ind] = -2;
        else if (mExcluded[ind] == 3)
          mExcluded[ind] = 5;
        else
          mExcluded[ind] = 4;
      }
    }
  }
  mWinApp->mMainView->DrawImage();
}

// Convert the hole vectors to image shift vectors at a specified magnification index
// and optionally set the vectors in the multishot dialog
int CHoleFinderDlg::ConvertHoleToISVectors(int index, bool setVecs, double *xVecOut,
  double *yVecOut, CString &errStr)
{
  ScaleMat st2is;
  int dir, numVecs = mLastWasHexGrid ? 3 : 2, hexInd = mLastWasHexGrid ? 1 : 0;
  float *xVecs, *yVecs;
  float xFloat[3], yFloat[3], xTemp[3], yTemp[3], bestRot, maxAngDiff, maxScaleDiff;
  float ySign = 1.;
  float diagXvecs[3], diagYvecs[3];
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  double *xSpacing = mLastWasHexGrid ? &msParams->hexISXspacing[0] :
    &msParams->holeISXspacing[0];
  double *ySpacing = mLastWasHexGrid ? &msParams->hexISYspacing[0] :
    &msParams->holeISYspacing[0];
  if (index < 0) {
    ySign = -1;
    xVecs = mGridImXVecs;
    yVecs = mGridImYVecs;
  } else {
    xVecs = mGridStageXVecs;
    yVecs = mGridStageYVecs;
  }
  if (!xVecs[0]) {
    errStr = "The hole finder has not saved any vectors";
    return 0;
  }
  if (mLastWasHexGrid && mCurHexSubset >= 0) {
    diagXvecs[0] = xVecs[0] - xVecs[2];
    diagYvecs[0] = yVecs[0] - yVecs[2];
    diagXvecs[1] = xVecs[0] + xVecs[1];
    diagYvecs[1] = yVecs[0] + yVecs[1];
    diagXvecs[2] = xVecs[1] + xVecs[2];
    diagYvecs[2] = yVecs[1] + yVecs[2];
    xVecs = &diagXvecs[0];
    yVecs = &diagYvecs[0];
  }
  for (dir = 0; dir < numVecs; dir++) {
    xVecOut[dir] = xVecs[dir];
    yVecOut[dir] = ySign * yVecs[dir];
  }

  if (index > 0) {
    st2is = MatMul(mWinApp->mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(),
      index), mWinApp->mShiftManager->CameraToIS(index));
    if (!st2is.xpx) {
      errStr = "There is no calibration to get from stage to IS coordinates at the "
        "given mag";
      return 0;
    }

    for (dir = 0; dir < numVecs; dir++)
      ApplyScaleMatrix(st2is, xVecs[dir], ySign * yVecs[dir], xVecOut[dir], yVecOut[dir]);
  }
  if (setVecs) {

    // If setting vectors, copy to param and float variables
    for (dir = 0; dir < numVecs; dir++) {
      xSpacing[dir] = xVecOut[dir];
      ySpacing[dir] = yVecOut[dir];
      xFloat[dir] = (float)xVecOut[dir];
      yFloat[dir] = (float)yVecOut[dir];
    }

    // Then check if a rotation by one vector position matches last used vectors;
    // if so copy over the rotated vectors
    if (mHelper->PermuteISvecsToMatchLastUsed(xFloat, yFloat, hexInd, bestRot, 
      maxAngDiff, maxScaleDiff, xTemp, yTemp)) {
      for (dir = 0; dir < numVecs; dir++) {
        xSpacing[dir] = xFloat[dir] = xTemp[dir];
        ySpacing[dir] = yFloat[dir] = yTemp[dir];
      }
    }

    // Save that this was last vectors, set other parameters
    mHelper->SetLastUsedHoleISVecs(&xFloat[0], &yFloat[0], true);
    msParams->holeMagIndex[hexInd] = index;
    msParams->tiltOfHoleArray[hexInd] = mLastTiltAngle;
    msParams->doHexArray = mLastWasHexGrid;
    if (mWinApp->mNavHelper->mMultiShotDlg)
      mWinApp->mNavHelper->mMultiShotDlg->UpdateSettings();
  }
  return numVecs;
}

// Get the hole vectors as IS values and store in Nav item if it is a map
void CHoleFinderDlg::SetNavMapHoleVectors()
{
  CString statStr;
  double xISvectors[3], yISvectors[3];
  int ind;
  xISvectors[2] = yISvectors[2] = 0.;
  if (mNavItem && ConvertHoleToISVectors(mLastMagIndex, false, &xISvectors[0],
    &yISvectors[0], statStr)) {
    for (ind = 0; ind < 3; ind++) {
      mNavItem->mXHoleISSpacing[ind] = (float)xISvectors[ind];
      mNavItem->mYHoleISSpacing[ind] = (float)yISvectors[ind];
    }
    mNav->SetChanged(true);
    mIDofNavItem = mNavItem->mMapID;
  }
}

// This should not be called
void CHoleFinderDlg::ScanningCleanup(int error)
{
  StopScanning();
}

void CHoleFinderDlg::StopScanning(void)
{
  if (mMontage > 0 && mCurStore < 0)
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
  std::vector<short> **exclude, BOOL &incl, BOOL &excl)
{
  *x = &mXstages;
  *y = &mYstages;
  *pcOn = &mPieceOn;
  *exclude = &mExcluded;
  incl = !mIsOpen || (m_bShowIncluded && !m_butToggleHoles.m_bSelected);
  excl = mIsOpen ? (m_bShowExcluded && !m_butToggleHoles.m_bSelected) :
  mParams.showExcluded;
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

// Process Ctrl - left mouse action at the given position to include/exclude points
bool CHoleFinderDlg::MouseSelectPoint(EMimageBuffer *imBuf, float inX, float inY,
  float imDistLim, bool dragging)
{
  float selXlimit[4], selYlimit[4], selXwindow[4], selYwindow[4];
  float dist, distMin = 1.e10, distNext = 1.e10;
  int ind, indMin = -1, miss, numDo, missMin;
  ScaleMat aInv;
  float delX, delY, stageX, stageY, xInPiece, yInPiece, distLim, stPtX, stPtY;
  int pieceIndex;

  // Start out much as in navigator selection: make sure it's OK, convert to stage
  // and get limits intsage coordinates (which are not square)
  if (!mNav->OKtoMouseSelect())
    return false;
  if (!mNav->ConvertMousePoint(imBuf, inX, inY, stageX, stageY, aInv, delX, delY,
    xInPiece, yInPiece, pieceIndex))
    return false;
  distLim = imDistLim * sqrtf(aInv.xpx * aInv.xpx + aInv.ypx * aInv.ypx);
  mNav->GetSelectionLimits(imBuf, aInv, delX, delY, selXlimit, selYlimit, selXwindow,
    selYwindow);

  // Loop over actual and missing points to find nearest
  for (miss = 0; miss < 2; miss++) {
    numDo = miss ? (int)mXmissStage.size() : (int)mXstages.size();
    for (ind = 0; ind < numDo; ind++) {
      if (mCurHexSubset >= 0 && (miss ? mMissHexSubset[ind] : mHexSubset[ind]) !=
        mCurHexSubset)
        continue;
      stPtX = miss ? mXmissStage[ind] : mXstages[ind];
      stPtY = miss ? mYmissStage[ind] : mYstages[ind];
      if (!(InsideContour(selXlimit, selYlimit, 4, stPtX, stPtY) ||
        InsideContour(selXwindow, selYwindow, 4, stPtX, stPtY)))
        continue;

      // Find nearest item
      delX = stPtX - stageX;
      delY = stPtY - stageY;
      dist = delX * delX + delY * delY;
      if (dist < distMin) {
        distNext = distMin;
        distMin = dist;
        indMin = ind;
        missMin = miss;
      }

    }
  }

  // Apply same criteria for drag and click in this case; do not operate on last selected
  // item when dragging - but rearm the dragging when it goes outside a  bit
  if (distMin > distLim || distMin > 0.5 * distNext ||
    (indMin == mLastUserSelectInd && (dragging || mLastSelectWasDrag))) {
    if (dragging && indMin == mLastUserSelectInd && distMin > 1.1 * distLim &&
      distMin > 0.55 * distNext)
      mLastUserSelectInd = -1;
    mLastSelectWasDrag = dragging;
    return false;
  }
  mLastSelectWasDrag = dragging;

  // For missing point, add it to list of points, maintain pieceOn regardless
  if (missMin) {
    mXstages.push_back(mXmissStage[indMin]);
    mYstages.push_back(mYmissStage[indMin]);
    mXcenters.push_back(mXmissCen[indMin]);
    mYcenters.push_back(mYmissCen[indMin]);
    if (mMissHexSubset.size())
      mHexSubset.push_back(mMissHexSubset[indMin]);
    if (mXinPiece.size()) {
      mXinPiece.push_back(mMissXinPiece[indMin]);
      mYinPiece.push_back(mMissYinPiece[indMin]);
      mPieceOn.push_back(mMissPieceOn[indMin]);
    } else {
      mPieceOn.push_back(-1);
    }
    mExcluded.push_back(-1);
    mLastUserSelectInd = (int)mXstages.size() - 1;

  } else if (indMin >= 0) {

    // For regular point, toggle exclude and set to special values
    if (mExcluded[indMin] <= 0)
      mExcluded[indMin] = 3;   // User exclude
    else 
      mExcluded[indMin] = -1;  // User include
    mLastUserSelectInd = indMin;
  }
  if (m_bUseBestSubset)
    SetExclusionsAndDraw();
  else
    mWinApp->mMainView->DrawImage();
  return true;
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

/*
 * Find holes, make points, and optionally combine in a range of maps at one mag index
 */
int CHoleFinderDlg::ProcessMultipleMaps(int indStart, int indEnd, int minForCombine, 
  int ifAutoCont, float target, float minSize,
  float maxSize, float relThresh, float absThresh)
{
  if (CheckAndSetNav("process multiple maps"))
    return 1;
  MapItemArray *itemArray = mNav->GetItemArray();
  int ind, camera, numMaps = 0;
  int magList[MAX_MAGS + 1], camList[MAX_MAGS + 1];
  CMapDrawItem *item;
  IntVec magInds;
  indEnd = B3DMIN(indEnd, (int)itemArray->GetSize() - 1);
  for (ind = 0; ind < MAX_MAGS + 1; ind++) {
    magList[ind] = 0;
    camList[ind] = 0;
  }

  // Find qualifying maps and bin their mag indexes
  for (ind = B3DMAX(0, indStart); ind <= indEnd; ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsMap() && item->mMapMagInd > 0 && item->mMapMagInd <= MAX_MAGS &&
      item->mColor != NAV_SKIP_COLOR && (item->mAcquire || ifAutoCont < 2) &&
      ((!item->mXHoleISSpacing[0] && !item->mYHoleISSpacing[0]) ||
        CountAcquirePointsDrawnOnMap(itemArray, item->mMapID) < mPMMminPtsForSkipIfIS)) {
      magList[item->mMapMagInd]++;
      camList[item->mMapMagInd] += item->mMapCamera;
    }
  }

  // Find the most popular mag index
  for (ind = 0; ind < MAX_MAGS + 1; ind++) {
    if (magList[ind] > numMaps) {
      numMaps = magList[ind];
      mPMMmagIndex = ind;
    }
  }
  if (!numMaps) {
    SEMMessageBox("There are no undone maps within the range of items");
    return 1;
  }
  
  mPMMcurrentInd = indStart;
  mPMMendInd = indEnd;
  mPMMcombineMinPts = minForCombine;
  mPMMifAutoCont = ifAutoCont;
  mPMMtarget= target;
  mPMMminSize = minSize;
  mPMMmaxSize = maxSize;
  mPMMrelThresh = relThresh;
  mPMMabsThresh = absThresh;
  camera = B3DNINT((float)camList[mPMMmagIndex] / numMaps);
  B3DCLAMP(camera, 0, MAX_CAMERAS - 1);
  PrintfToLog("Processing %d maps at magnification %d", numMaps, 
    MagForCamera(camera, mPMMmagIndex));
  SyncToMasterParams();
    
  mWinApp->UpdateBufferWindows();
  MultiMapNextTask(0);
  return 0;
}

// Count number of points drawn on the given map item
int CHoleFinderDlg::CountAcquirePointsDrawnOnMap(MapItemArray *itemArray, int mapID)
{
  int numPts = 0, ind;
  CMapDrawItem *item;
  for (ind = 0; ind < (int)itemArray->GetSize(); ind++) {
    item = itemArray->GetAt(ind);
    if (item->mAcquire && item->mDrawnOnMapID == mapID)
      numPts++;
  }
  return numPts;
}

// Next task means process the next map after finishing last one
void CHoleFinderDlg::MultiMapNextTask(int param)
{
  CMapDrawItem *item;
  CString str;
  double tmp1, tmp2, tmp3;
  float shiftX, shiftY, xcen, ycen;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  int ind, numPts, readBuf = mWinApp->mBufferManager->GetBufToReadInto();
  if (!mPMMmagIndex)
    return;
  mNav = mWinApp->mNavigator;
  if (!mNav) {
    StopMultiMap();
    return;
  }
  MapItemArray *itemArray = mNav->GetItemArray();

  // Process results of hole finder: make points and combine
  if (param == 1) {
    numPts = DoMakeNavPoints(-1, EXTRA_NO_VALUE, EXTRA_NO_VALUE, -1., -1., -1., -1);
    if (numPts < 0) {
      StopMultiMap();
      return;
    }
    PrintfToLog("Hole finder made %d Navigator points at holes", numPts);
    if (mPMMcombineMinPts > 0 && numPts >= mPMMcombineMinPts) {

      // Asign the hole vectors: need the item back for this
      if (mPMMcurrentInd < 0 || mPMMcurrentInd >= (int)itemArray->GetSize()) {
        str.Format("Program error: current index (%d) out of range of table size (%d)",
          mPMMcurrentInd, itemArray->GetSize());
        SEMMessageBox(str);
        StopMultiMap();
        return;
      }
      item = itemArray->GetAt(mPMMcurrentInd);
      mHelper->AssignNavItemHoleVectors(item);
      
      // Combine: tell it number to use
      ind = mHelper->mCombineHoles->CombineItems(-numPts,
        mHelper->GetMHCturnOffOutsidePoly(), -9, -9);
      if (ind) {
        SEMMessageBox("Error trying to combine hole for multiple Records:\n" +
          CString(mHelper->mCombineHoles->GetErrorMessage(ind)));
        StopMultiMap();
        return;
      }
    } else if (mPMMcombineMinPts > 0) {
      SEMAppendToLog("Too few points to combine; turning them off");
      for (ind = 0; ind < numPts; ind++) {
        item = itemArray->GetAt(ind + itemArray->GetSize() - numPts);
        item->mAcquire = false;
      }
    }
    mPMMcurrentInd++;
    if (mPMMifAutoCont > 0) {
      item = itemArray->GetAt(mPMMcurrentInd - 1);
      item->mAcquire = false;
      ind = mNav->FindBufferWithMapID(item->mMapID);
      if (ind >= 0) {
        mWinApp->mProcessImage->GetCentroidOfBuffer(&imBufs[ind], xcen, ycen, shiftX, 
          shiftY, false, tmp1, tmp2, tmp3);
        mHelper->mAutoContouringDlg->AutoContourImage(&imBufs[ind], mPMMtarget,
          mPMMminSize, mPMMmaxSize, mPMMrelThresh, mPMMabsThresh, false, xcen, ycen);
        mWinApp->AddIdleTask(TASK_MULTI_MAP_HOLES, 2, 0);
        return;
      } else {
        SEMAppendToLog("Cannot find map in buffer for autocontouring");
      }
    }
  }

  // Loop to look for next map
  for (; mPMMcurrentInd <= mPMMendInd; mPMMcurrentInd++) {
    item = itemArray->GetAt(mPMMcurrentInd);

    // Same test as above except for the specific mag
    if (item->IsMap() && item->mMapMagInd == mPMMmagIndex && 
      item->mColor != NAV_SKIP_COLOR && (item->mAcquire || mPMMifAutoCont < 2) &&
      ((!item->mXHoleISSpacing[0] && !item->mYHoleISSpacing[0]) ||
        CountAcquirePointsDrawnOnMap(itemArray, item->mMapID) < mPMMminPtsForSkipIfIS)) {

      // Load the map
      if (mNav->DoLoadMap(true, item, readBuf)) {
        str.Format("Error loading map for item %d (%s)", mPMMcurrentInd + 1,
          (LPCTSTR)item->mLabel);
        SEMMessageBox(str);
        StopMultiMap();
        return;
      }

      // Find holes
      PrintfToLog("Finding holes in map for item %d (%s)", mPMMcurrentInd + 1,
        (LPCTSTR)item->mLabel);
      if (DoFindHoles(&imBufs[readBuf], false)) {
        StopMultiMap();
        return;
      }
      mWinApp->AddIdleTask(TASK_MULTI_MAP_HOLES, 1, 0);
      return;
    }
  }

  // It came to the end of the range
  StopMultiMap();
}

// Busy if hole finder running
int CHoleFinderDlg::MultiMapBusy()
{
  return (mFindingHoles || mHelper->mAutoContouringDlg->AutoContBusy()) ? 1 : 0;
}

// Stop: just clear mag index
void CHoleFinderDlg::StopMultiMap()
{
  if (!mPMMmagIndex)
    return;
  mPMMmagIndex = 0;
  mWinApp->UpdateBufferWindows();
}

void CHoleFinderDlg::MultimapCleanup(int error)
{
  StopMultiMap();
}

/*
 * Routine to run hole finder to a small number of holes, return the pixel coordinates
 * of the one nearest the center, and optionally apply an image shift if it is not too big
 * It can take a less accurate diameter if a lot of circles are tried, and runs the
 * hole finder synchronously
 */
int CHoleFinderDlg::FindAndCenterOneHole(EMimageBuffer *imBuf, float diameter, int numTry,
  float maxFracShift, float &xCenter, float &yCenter)
{
  FloatVec *widths, *increments;
  IntVec *numCircles;
  ScaleMat bInv;
  int numSave, err, nx, ny, ind, bestInd;
  float diamSave, xcen, ycen, dist, minDist, pixel, fbin = (float)imBuf->mBinning;
  float xShift, yShift;
  HoleFinderParams *hfParams = mHelper->GetHoleFinderParams();
  double ISX, ISY;

  // Get the parameters and save # of circles and diameter
  SyncToMasterParams();
  mHelper->GetHFscanVectors(&widths, &increments, &numCircles);
  numSave = numCircles->at(0);
  if (numTry)
    *(numCircles->data()) = numTry;
  if (hfParams->hexagonalArray) {
    diamSave = hfParams->hexDiameter;
    hfParams->hexDiameter = diameter;
  } else {
    diamSave = hfParams->diameter;
    hfParams->diameter = diameter;
  }
  if (mIsOpen)
    UpdateSettings();

  // Find holes synchronously
  err = DoFindHoles(imBuf, true);

  // restore parameters
  if (hfParams->hexagonalArray) {
    hfParams->hexDiameter = diamSave;
  } else {
    hfParams->diameter = diamSave;
  }
  *(numCircles->data()) = numSave;
  if (mIsOpen)
    UpdateSettings();

  // Error return
  if (err || !mXcenters.size())
    return err;

  // Find point closest to center
  imBuf->mImage->getSize(nx, ny);
  xcen = (float)(nx / 2.);
  ycen = (float)(ny / 2.);
  for (ind = 0; ind < (int)mXcenters.size(); ind++) {
    dist = powf(mXcenters[ind] - xcen, 2.f) + powf(mXcenters[ind] - xcen, 2.f);
    if (!ind || dist < minDist) {
      bestInd = ind;
      minDist = dist;
    }
  }
  xCenter = mXcenters[bestInd];
  yCenter = mYcenters[bestInd];

  // If shifting requested, make sure not already done, compute it, apply if above limit
  if (maxFracShift > 0) {
    imBuf->mImage->getShifts(xShift, yShift);
    if (xShift != 0. || yShift != 0.) {
      SEMAppendToLog("Not applying image shift, the image is already shifted");
      return -2;
    }

    // Right-handed shift for coordinate conversion to IS
    xShift = xcen - xCenter;
    yShift = yCenter - ycen;
    pixel = mWinApp->mShiftManager->GetPixelSize(imBuf);
    if (sqrtf(minDist) * pixel > maxFracShift * diameter) {
      PrintfToLog("Not applying image shift of %.2f, %.2f microns; it is above "
        "limit of %.2f", pixel * xShift, pixel * yShift, maxFracShift * diameter);
      return -1;
    }
    bInv = mWinApp->mShiftManager->CameraToIS(imBuf->mMagInd);
    mWinApp->mShiftManager->ApplyScaleMatrix(bInv, -fbin * xShift, -fbin * yShift,
      ISX, ISY);
    mWinApp->mScope->IncImageShift(ISX, ISY);

    // Left-handed shift for image
    imBuf->mImage->setShifts(xShift, -yShift);
    imBuf->SetImageChanged(1);
    mWinApp->mMainView->DrawImage();
    PrintfToLog("Applied image shift of %.2f, %.2f microns to center hole",
      pixel * xShift, pixel * yShift);
  }
  return 0;
}
