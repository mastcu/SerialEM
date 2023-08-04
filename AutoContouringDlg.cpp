// AutoContouringDlg.cpp : Creates contours around grid squares and makes them into
// groups of polygons
// There is a resident instance of this class, and the dialog can be created and closed
//
// Copyright (C) 2022 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>
#include "SerialEM.h"
#include "SerialEMView.h"
#include "AutoContouringDlg.h"
#include "Shared\imodel.h"
#include "NavHelper.h"
#include "NavigatorDlg.h"
#include "ShiftManager.h"
#include "Shared\holefinder.h"


// CAutoContouringDlg dialog

CAutoContouringDlg::CAutoContouringDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_AUTOCONTOUR, pParent)
  , m_iReduceToWhich(0)
  , m_iThreshType(0)
  , m_iGroupType(0)
  , m_iReducePixels(300)
  , m_strLowerMean(_T(""))
  , m_strMinSize(_T(""))
  , m_strBorderDist(_T(""))
  , m_strIrregularity(_T(""))
  , m_strSquareSD(_T(""))
  , m_strMinLowerMean(_T(""))
  , m_strMinUpperMean(_T(""))
  , m_strMinMinSize(_T(""))
  , m_strMinIrregular(_T(""))
  , m_strMinSquareSD(_T(""))
  , m_strMinBorderDist(_T(""))
  , m_fReduceToPixSize(1.f)
  , m_fRelThresh(0.7f)
  , m_fAbsThresh(0)
  , m_fACminSize(2.f)
  , m_fACmaxSize(100.f)
  , m_strShowGroups(_T(""))
{
  mNonModal = true;
  mIsOpen = false;
  mAutoContThread = NULL;
  mFindingFromDialog = false;
  mHaveConts = false;
  mDoingAutocont = false;
  for (int ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++)
    mShowGroup[ind] = 1;
}

CAutoContouringDlg::~CAutoContouringDlg()
{
  for (int ind = 0; ind < (int)mPolyArray.GetSize(); ind++)
    delete mPolyArray.GetAt(ind);
}

void CAutoContouringDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_RTOPIXELS, m_iReduceToWhich);
  DDX_Radio(pDX, IDC_RRELATIVE_THRESH, m_iThreshType);
  DDX_Radio(pDX, IDC_RGROUP_BY_SIZE, m_iGroupType);
  DDX_MM_INT(pDX, IDC_IDC_EDIT_TO_PIXELS, m_iReducePixels, 200, 5000,
    "number of pixels to reduce to");
  DDX_Control(pDX, IDC_SPIN_NUM_GROUPS, m_sbcNumGroups);
  DDX_Text(pDX, IDC_EDIT_MIN_MEAN, m_strLowerMean);
  DDX_Text(pDX, IDC_EDIT_MAX_MEAN, m_strUpperMean);
  DDX_Text(pDX, IDC_EDIT_MIN_SIZE, m_strMinSize);
  DDX_Text(pDX, IDC_EDIT_BORDER_DIST, m_strBorderDist);
  DDX_Text(pDX, IDC_STAT_IRREGULARITY, m_strIrregularity);
  DDX_Text(pDX, IDC_STAT_SQUARE_SD, m_strSquareSD);
  DDX_Text(pDX, IDC_STAT_MIN_LOWER_MEAN, m_strMinLowerMean);
  DDX_Text(pDX, IDC_STAT_MIN_UPPER_MEAN, m_strMinUpperMean);
  DDX_Text(pDX, IDC_STAT_MIN_MIN_SIZE, m_strMinMinSize);
  DDX_Text(pDX, IDC_STAT_MIN_IRREGULAR, m_strMinIrregular);
  DDX_Text(pDX, IDC_STAT_MIN_SQUARE_SD, m_strMinSquareSD);
  DDX_Text(pDX, IDC_STAT_MIN_BORDER_DIST, m_strMinBorderDist);
  DDX_Text(pDX, IDC_STAT_MAX_LOWER_MEAN, m_strMaxLowerMean);
  DDX_Text(pDX, IDC_STAT_MAX_UPPER_MEAN, m_strMaxUpperMean);
  DDX_Text(pDX, IDC_STAT_MAX_MIN_SIZE, m_strMaxMinSize);
  DDX_Text(pDX, IDC_STAT_MAX_IRREGULAR, m_strMaxIrregular);
  DDX_Text(pDX, IDC_STAT_MAX_SQUARE_SD, m_strMaxSquareSD);
  DDX_Text(pDX, IDC_STAT_MAX_BORDER_DIST, m_strMaxBorderDist);
  DDX_MM_FLOAT(pDX, IDC_EDIT_TO_PIX_SIZE, m_fReduceToPixSize, .2f, 20.f,
    "Pixel size to reduce to");
  DDX_MM_FLOAT(pDX, IDC_EDIT_REL_THRESH, m_fRelThresh, 0.f, 1.f, "Relative threshold");
  DDX_FLOAT(pDX, IDC_EDIT_ABS_THRESH, m_fAbsThresh, "Absolute threshold");
  DDX_MM_FLOAT(pDX, IDC_EDIT_MINSIZE, m_fACminSize, 1.f, 500.f, "Minimum contour size");
  DDX_MM_FLOAT(pDX, IDC_EDIT_MAXSIZE, m_fACmaxSize, 2.f, 500.f, "Maximum contour size");
  DDX_Control(pDX, IDC_SLIDER_MIN_MEAN, m_sliderLowerMean);
  DDX_Control(pDX, IDC_SLIDER_MAX_MEAN, m_sliderUpperMean);
  DDX_Control(pDX, IDC_SLIDER_MIN_SIZE, m_sliderMinSize);
  DDX_Control(pDX, IDC_SLIDER_IRREGULARITY, m_sliderIrregular);
  DDX_Control(pDX, IDC_SLIDER_SQUARE_SD, m_sliderSquareSD);
  DDX_Control(pDX, IDC_SLIDER_BORDER_DIST, m_sliderBorderDist);
  DDX_Slider(pDX, IDC_SLIDER_MIN_MEAN, m_intLowerMean);
  DDX_Slider(pDX, IDC_SLIDER_MAX_MEAN, m_intUpperMean);
  DDX_Slider(pDX, IDC_SLIDER_MIN_SIZE, m_intMinSize);
  DDX_Slider(pDX, IDC_SLIDER_IRREGULARITY, m_intIrregular);
  DDX_Slider(pDX, IDC_SLIDER_SQUARE_SD, m_intSquareSD);
  DDX_Slider(pDX, IDC_SLIDER_BORDER_DIST, m_intBorderDist);
  DDV_MinMaxInt(pDX, m_intLowerMean, 0, 255);
  DDV_MinMaxInt(pDX, m_intUpperMean, 0, 255);
  DDV_MinMaxInt(pDX, m_intMinSize, 0, 255);
  DDV_MinMaxInt(pDX, m_intIrregular, 0, 255);
  DDV_MinMaxInt(pDX, m_intSquareSD, 0, 255);
  DDV_MinMaxInt(pDX, m_intBorderDist, 0, 255);
  DDX_Text(pDX, IDC_STAT_SHOW_GROUPS, m_strShowGroups);
  DDX_Control(pDX, IDC_STAT_SHOW_GROUPS, m_statShowGroups);
}


BEGIN_MESSAGE_MAP(CAutoContouringDlg, CBaseDlg)
  ON_WM_HSCROLL()
  ON_BN_CLICKED(IDC_RRELATIVE_THRESH, OnRRelativeThresh)
  ON_BN_CLICKED(IDC_RABS_THRESH, OnRRelativeThresh)
  ON_BN_CLICKED(IDC_RTOPIXELS, OnRToPixels)
  ON_BN_CLICKED(IDC_RTOPIXSIZE, OnRToPixels)
  ON_BN_CLICKED(IDC_RGROUP_BY_SIZE, OnRGroupBySize)
  ON_BN_CLICKED(IDC_RGROUP_BY_MEAN, OnRGroupBySize)
  ON_EN_KILLFOCUS(IDC_IDC_EDIT_TO_PIXELS, OnKillfocusEditToPixels)
  ON_EN_KILLFOCUS(IDC_EDIT_TO_PIX_SIZE, OnKillfocusEditToPixSize)
  ON_EN_KILLFOCUS(IDC_EDIT_REL_THRESH, OnKillfocusEditRelThresh)
  ON_EN_KILLFOCUS(IDC_EDIT_ABS_THRESH, OnKillfocusEditAbsThresh)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_NUM_GROUPS, OnDeltaposSpinNumGroups)
  ON_BN_CLICKED(IDC_BUT_MAKE_CONTOURS, OnButMakeContours)
  ON_BN_CLICKED(IDC_BUT_CLEAR_POLYS, OnButClearData)
  ON_COMMAND_RANGE(IDC_CHECK_GROUP1, IDC_CHECK_GROUP8, OnCheckShowGroup)
  ON_BN_CLICKED(IDC_BUT_CREATE_POLYS, OnButCreatePolys)
  ON_BN_CLICKED(IDC_BUT_UNDO_POLYS, OnButUndoPolys)
  ON_EN_KILLFOCUS(IDC_EDIT_MIN_MEAN, OnKillfocusEditMinMean)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_MEAN, OnKillfocusEditMaxMean)
  ON_EN_KILLFOCUS(IDC_EDIT_MIN_SIZE, OnKillfocusEditMinSize)
  ON_EN_KILLFOCUS(IDC_EDIT_BORDER_DIST, OnKillfocusEditBorderDist)
  ON_EN_KILLFOCUS(IDC_EDIT_MINSIZE, OnKillfocusEditACMinsize)
  ON_EN_KILLFOCUS(IDC_EDIT_MAXSIZE, OnKillfocusEditACMaxsize)
END_MESSAGE_MAP()


// CAutoContouringDlg message handlers
BOOL CAutoContouringDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  mMasterParams = mWinApp->mNavHelper->GetAutocontourParams();
  mParams = *mMasterParams;
  m_sbcNumGroups.SetRange(1, MAX_AUTOCONT_GROUPS);
  m_sbcNumGroups.SetPos(mParams.numGroups);
  mIsOpen = true;
  mBoldFont = mWinApp->GetBoldFont(&m_statShowGroups);

  // Set up sliders
  m_sliderLowerMean.SetRange(0, 255);
  m_sliderUpperMean.SetRange(0, 255);
  m_sliderMinSize.SetRange(0, 255);
  m_sliderSquareSD.SetRange(0, 255);
  m_sliderIrregular.SetRange(0, 255);
  m_sliderBorderDist.SetRange(0, 255);
  m_sliderLowerMean.SetPageSize(4);
  m_sliderUpperMean.SetPageSize(4);
  m_sliderMinSize.SetPageSize(4);
  m_sliderSquareSD.SetPageSize(4);
  m_sliderIrregular.SetPageSize(4);
  m_sliderBorderDist.SetPageSize(4);

  // Output parameters and manage enables
  ParamsToDialog();
  ManageGroupSelectors(1);
  ManageACEnables();
  ManagePostEnables(false);
  for (int ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++)
    mShowGroup[ind] = 1;

  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

// Closing on OK or when Nav closes
void CAutoContouringDlg::OnOK()
{
  DialogToParams();
  *mMasterParams = mParams;
  OnCancel();
}

// Cancel just sets placement and says it is closed; clears out data
void CAutoContouringDlg::OnCancel()
{
  mWinApp->mNavHelper->GetAutoContDlgPlacement();
  mIsOpen = false;
  mHaveConts = false;
  DestroyWindow();
}

// Do not delete the dialog
void CAutoContouringDlg::PostNcDestroy()
{
  CDialog::PostNcDestroy();
}

BOOL CAutoContouringDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// External close
void CAutoContouringDlg::CloseWindow()
{
  if (mIsOpen)
    OnOK();
}

// Make sure master params are updated
void CAutoContouringDlg::SyncToMasterParams()
{
  if (!mIsOpen) {
    mMasterParams = mWinApp->mNavHelper->GetAutocontourParams();
    mParams = *mMasterParams;
    mNumGroups = mParams.numGroups;
    return;
  }
  UPDATE_DATA_TRUE;
  DialogToParams();
  *mMasterParams = mParams;
}

// Callback from SerialEMView to get items to draw
MapItemArray* CAutoContouringDlg::GetPolyArrayToDraw(
  ShortVec **groupNums, ShortVec **excluded, int &numGroups, int **showGroup)
{
  if (!mHaveConts)
    return NULL;
  *groupNums = &mGroupNums;
  *excluded = &mExcluded;
  numGroups = mNumGroups;
  *showGroup = &mShowGroup[0];
  return &mPolyArray;
}

// Test if it can still be undone
bool CAutoContouringDlg::IsUndoFeasible()
{
  CMapDrawItem *item;
  int numConv = (int)mFirstConvertedIndex.size();
  MapItemArray *itemArray = mWinApp->mNavigator->GetItemArray();
  if (!numConv || itemArray->GetSize() - 1 != mLastConvertedIndex[numConv - 1])
    return false;
  item = itemArray->GetAt(mFirstConvertedIndex[numConv - 1]);
  if (item->mMapID != mFirstMapID[numConv - 1])
    return false;
  item = itemArray->GetAt(mLastConvertedIndex[numConv - 1]);
  return item->mMapID == mLastMapID[numConv - 1];
}

// Call all three managing functions
void CAutoContouringDlg::ManageAll(bool forBusy)
{
  ManageACEnables();
  ManageGroupSelectors(0);
  ManagePostEnables(forBusy);
}

// Possible settings change
void CAutoContouringDlg::UpdateSettings()
{
  mParams = *mMasterParams;
  ParamsToDialog();
  ManageAll(false);
}

// Determine which contours to exclude and divide the remaining ones into groups
void CAutoContouringDlg::SetExclusionsAndGroups()
{
  SetExclusionsAndGroups(mParams.groupByMean, mParams.lowerMeanCutoff,
    mParams.upperMeanCutoff, mParams.minSizeCutoff, mParams.SDcutoff, 
    mParams.irregularCutoff, mParams.borderDistCutoff);
}

void CAutoContouringDlg::SetExclusionsAndGroups(int groupByMean, float lowerMeanCutoff,
  float upperMeanCutoff, float minSizeCutoff, float SDcutoff, float irregularCutoff,
  float borderDistCutoff)
{
  int ind, group, numCont = (int)mPolyArray.GetSize();
  float minIncl = 1.e30f, maxIncl = -1.e30f, groupInc;
  AutoContData *acd = &mAutoContData;

  if (!mHaveConts)
    return;

  // Set to param if no values set by caller
  if (lowerMeanCutoff < EXTRA_VALUE_TEST)
    lowerMeanCutoff = mParams.lowerMeanCutoff;
  if (upperMeanCutoff < EXTRA_VALUE_TEST)
    upperMeanCutoff = mParams.upperMeanCutoff;
  if (SDcutoff < 0.)
    SDcutoff = mParams.SDcutoff;
  if (irregularCutoff < 0.)
    irregularCutoff = mParams.irregularCutoff;
  if (minSizeCutoff < 0.)
    minSizeCutoff = mParams.minSizeCutoff;
  if (borderDistCutoff < 0.)
    borderDistCutoff = mParams.borderDistCutoff;

  // Find ones excluded and get min/max of ones included
  mExcluded.resize(numCont);
  mGroupNums.resize(numCont);
  int num1 = 0, num2 = 0;
  for (ind = 0; ind < numCont; ind++) {
    if (acd->sqrMeans[ind] < lowerMeanCutoff || acd->sqrMeans[ind] > upperMeanCutoff ||
      acd->sqrSizes[ind] < minSizeCutoff || acd->sqrSDs[ind] > SDcutoff ||
      acd->squareness[ind] > irregularCutoff || acd->boundDists[ind] < borderDistCutoff) {
      mExcluded[ind] = 1;
    } else {
      mExcluded[ind] = 0;
      if (groupByMean) {
        ACCUM_MIN(minIncl, acd->sqrMeans[ind]);
        ACCUM_MAX(maxIncl, acd->sqrMeans[ind]);
      } else {
        ACCUM_MIN(minIncl, acd->sqrSizes[ind]);
        ACCUM_MAX(maxIncl, acd->sqrSizes[ind]);
      }
    }
  }

  // Assign groups
  memset(mNumInGroup, 0, MAX_AUTOCONT_GROUPS * sizeof(int));
  groupInc = (maxIncl - minIncl) / mNumGroups;
  for (ind = 0; ind < numCont; ind++) {
    if (mExcluded[ind]) {
      mGroupNums[ind] = 0;
    } else {
      group = (int)(((groupByMean ? acd->sqrMeans[ind] : acd->sqrSizes[ind]) - 
        minIncl) / groupInc);
      B3DCLAMP(group, 0, mNumGroups - 1);
      mGroupNums[ind] = group;
      mNumInGroup[group]++;
    }
  }
  if (mIsOpen)
    ManageGroupSelectors(0);
  mWinApp->mMainView->RedrawWindow();
}

// Choice to use relative or absolute threshold
void CAutoContouringDlg::OnRRelativeThresh()
{
  UPDATE_DATA_TRUE;
  ManageACEnables();
  mWinApp->RestoreViewFocus();
}

// Choice to use pixels or pixel size
void CAutoContouringDlg::OnRToPixels()
{
  UPDATE_DATA_TRUE;
  ManageACEnables();
  mWinApp->RestoreViewFocus();
}

// Grouping by size or value
void CAutoContouringDlg::OnRGroupBySize()
{
  UPDATE_DATA_TRUE;
  mParams.groupByMean = m_iGroupType;
  if (mHaveConts)
    SetExclusionsAndGroups();
  mWinApp->RestoreViewFocus();
}

// New values of target pixels and pixel size
void CAutoContouringDlg::OnKillfocusEditToPixels()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

void CAutoContouringDlg::OnKillfocusEditToPixSize()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// New values of relative and absolute threshold
void CAutoContouringDlg::OnKillfocusEditRelThresh()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

void CAutoContouringDlg::OnKillfocusEditAbsThresh()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// New number of groups: redo groups
void CAutoContouringDlg::OnDeltaposSpinNumGroups(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, 1, MAX_AUTOCONT_GROUPS, mNumGroups))
    return;
  m_strShowGroups.Format("Split into %d", mNumGroups);
  ManageGroupSelectors(0);
  SetExclusionsAndGroups();
  UpdateData(false);
  mWinApp->RestoreViewFocus();
}

// External call to set the groups
void CAutoContouringDlg::ExternalSetGroups(int numGroups, int which, int *showGroups,
  int numShow)
{
  int ind;
  if (numGroups > 0 && numGroups <= MAX_AUTOCONT_GROUPS)
    mNumGroups = numGroups;
  if (which >= 0)
    mParams.groupByMean = m_iGroupType = which ? 1 : 0;
  for (ind = 0; ind < numShow; ind++)
    if (showGroups[ind] >= 0)
      mShowGroup[ind] = showGroups[ind] ? 1 : 0;
  if (!numShow && numGroups == 1)
      mShowGroup[0] = 1;
  SetExclusionsAndGroups();
  if (mIsOpen) {
    m_strShowGroups.Format("Split into %d", mNumGroups);
    ManageGroupSelectors(2);
    UpdateData(false);
  } else {
    mMasterParams = mWinApp->mNavHelper->GetAutocontourParams();
    mMasterParams->numGroups = mNumGroups;
  }
  mParams.numGroups = mNumGroups;
}

// Start autocontouring
void CAutoContouringDlg::OnButMakeContours()
{
  EMimageBuffer *imBuf = mWinApp->mMainView->GetActiveImBuf();
  mWinApp->RestoreViewFocus();
  if (!imBuf->mImage) {
    SEMMessageBox("There is no image in the active buffer for autocontouring");
    return;
  }
  mFindingFromDialog = true;
  DialogToParams();
  AutoContourImage(imBuf, mParams.usePixSize ? mParams.targetPixSizeUm :
    mParams.targetSizePixels, mParams.minSize, mParams.maxSize, mParams.useAbsThresh ?
    0.f : mParams.relThreshold, mParams.useAbsThresh ? mParams.absThreshold : 0.f);
  mFindingFromDialog = false;
  ManageAll(false);
}

// Clear out the data
void CAutoContouringDlg::OnButClearData()
{
  int ind;
  CMapDrawItem *item;
  AutoContData *acd = &mAutoContData;
  mWinApp->RestoreViewFocus();
  CLEAR_RESIZE(acd->sqrMeans, float, 0);
  CLEAR_RESIZE(acd->sqrSDs, float, 0);
  CLEAR_RESIZE(acd->sqrSizes, float, 0);
  CLEAR_RESIZE(acd->squareness, float, 0);
  CLEAR_RESIZE(acd->boundDists, float, 0);
  CLEAR_RESIZE(acd->pctBlackPix, float, 0);
  for (ind = 0; ind < (int)mPolyArray.GetSize(); ind++) {
    item = mPolyArray.GetAt(ind);
    delete item;
  }
  mPolyArray.RemoveAll();
  mHaveConts = false;
  CLEAR_RESIZE(mFirstConvertedIndex, int, 0);
  CLEAR_RESIZE(mLastConvertedIndex, int, 0);
  CLEAR_RESIZE(mFirstMapID, int, 0);
  CLEAR_RESIZE(mLastMapID, int, 0);
  mConvertedInds.clear();
  if (mIsOpen)
    ManageAll(false);
  mWinApp->mMainView->RedrawWindow();
}

// A group is toggled: redraw
void CAutoContouringDlg::OnCheckShowGroup(UINT nID)
{
  CButton *but = (CButton *)GetDlgItem(nID);
  mShowGroup[nID - IDC_CHECK_GROUP1] = but && but->GetCheck() != 0;
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}

// Convert to Navigator polygons
void CAutoContouringDlg::OnButCreatePolys()
{
  CString mess;
  mWinApp->RestoreViewFocus();
  if (DoCreatePolys(mess))
    SEMMessageBox(mess);
}

// External call to create the polygons with possible replcement cutoff values
int CAutoContouringDlg::ExternalCreatePolys(float lowerMeanCutoff, float upperMeanCutoff, 
  float minSizeCutoff, float SDcutoff, float irregularCutoff, float borderDistCutoff, 
  CString &mess)
{
  if (!mHaveConts) {
    mess = "There are no contours to convert";
    return 1;
  }
  if (lowerMeanCutoff < 0)
    lowerMeanCutoff = mParams.lowerMeanCutoff;
  else if (lowerMeanCutoff < 1)
    lowerMeanCutoff = mStatMinMean + lowerMeanCutoff * (mMedianMean - mStatMinMean);
  if (upperMeanCutoff < 0)
    upperMeanCutoff = mParams.upperMeanCutoff;
  else if (upperMeanCutoff <= 1.)
    upperMeanCutoff = mStatMinMean + upperMeanCutoff * (mStatMaxMean - mMedianMean);
  if (minSizeCutoff < 0)
    minSizeCutoff = mParams.minSizeCutoff;
  if (SDcutoff < 0)
    SDcutoff = mParams.SDcutoff;
  else if (SDcutoff <= 1.)
    SDcutoff = mStatMinSD + SDcutoff * (mStatMaxSD - mStatMinSD);
  if (borderDistCutoff < 0)
    borderDistCutoff = mParams.borderDistCutoff;
  SetExclusionsAndGroups(mParams.groupByMean, lowerMeanCutoff, upperMeanCutoff,
    minSizeCutoff, SDcutoff, irregularCutoff, borderDistCutoff);
  return DoCreatePolys(mess);
}

// Common function to creat polygons
int CAutoContouringDlg::DoCreatePolys(CString &mess)
{
  int firstID, lastID, num = 0, numAfter, ind;
  int numBefore = mWinApp->mNavigator->GetNumNavItems();
  IntVec indsInPoly;
  for (ind = 0; ind < mNumGroups; ind++)
    if (mShowGroup[ind])
      num += mNumInGroup[ind];
  if (!mHaveConts || !num) {
    mess = "There are no contours to convert" +
      CString(mHaveConts ? " in the selected groups" : "");
    return 1;
  }
  mWinApp->mNavigator->AddAutocontPolygons(mPolyArray, mExcluded, mGroupNums,
    &mShowGroup[0], mNumGroups, firstID, lastID, indsInPoly);
  numAfter = mWinApp->mNavigator->GetNumNavItems();
  if (numBefore < numAfter) {
    mFirstConvertedIndex.push_back(numBefore);
    mLastConvertedIndex.push_back(numAfter - 1);
    mFirstMapID.push_back(firstID);
    mLastMapID.push_back(lastID);
    mConvertedInds.push_back(indsInPoly);
    //SetExclusionsAndGroups();
    if (mIsOpen) {
      ManagePostEnables(false);
      ManageGroupSelectors(0);
    }
  }
  return 0;
}

// Undo the conversion to Nav polygons
void CAutoContouringDlg::OnButUndoPolys()
{
  int vecInd = (int)mFirstConvertedIndex.size() - 1;
  if (!IsUndoFeasible()) {
    SEMMessageBox("Something has changed in the Navigator table; "
      "cannot undo the addition of polygons");
    return;
  }

  // Get things added back in and pop the undo stack
  mWinApp->mNavigator->UndoAutocontPolyAddition(mPolyArray, 
    mLastConvertedIndex[vecInd] + 1 - mFirstConvertedIndex[vecInd], 
    mConvertedInds[vecInd]);
  mConvertedInds.pop_back();
  mFirstConvertedIndex.pop_back();
  mLastConvertedIndex.pop_back();
  mFirstMapID.pop_back();
  mLastMapID.pop_back();
  ManagePostEnables(false);
  SetExclusionsAndGroups();
  mWinApp->RestoreViewFocus();
}

// Return some of the statistics
int CAutoContouringDlg::GetSquareStats(float &minMean, float &maxMean, 
  float &medianMean)
{
  if (!mHaveConts)
    return 1;
  minMean = mStatMinMean;
  maxMean = mStatMaxMean;
  medianMean = mMedianMean;
  return 0;
}

// Lower mean cutoff
void CAutoContouringDlg::OnKillfocusEditMinMean()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mParams.lowerMeanCutoff = (float)atof(m_strLowerMean);
  m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff);
  m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mMedianMean) /
    (mMedianMean - mStatMinMean));
  B3DCLAMP(m_intLowerMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndGroups();
}

// Upper mean cutoff
void CAutoContouringDlg::OnKillfocusEditMaxMean()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mParams.upperMeanCutoff = (float)atof(m_strUpperMean);
  m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff);
  m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMedianMean) /
    (mStatMaxMean - mMedianMean));
  B3DCLAMP(m_intUpperMean, 0, 255);
  UpdateData(false);
  SetExclusionsAndGroups();
}

// Min size cutoff
void CAutoContouringDlg::OnKillfocusEditMinSize()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mParams.minSizeCutoff = (float)atof(m_strUpperMean);
  m_strMinSize.Format("%.4g", mParams.minSizeCutoff);
  m_intMinSize = (int)(255. * (mParams.minSizeCutoff - mStatMinSize) /
    (mStatMaxSize - mStatMinSize));
  B3DCLAMP(m_intMinSize, 0, 255);
  UpdateData(false);
  SetExclusionsAndGroups();
}

// Border distance cutoff
void CAutoContouringDlg::OnKillfocusEditBorderDist()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  mParams.borderDistCutoff = (float)atof(m_strBorderDist);
  m_strBorderDist.Format("%.4g", mParams.borderDistCutoff);
  m_intBorderDist = (int)(255. * mParams.borderDistCutoff / mStatMaxBoundDist);
  B3DCLAMP(m_intBorderDist, 0, 255);
  UpdateData(false);
  SetExclusionsAndGroups();
}

// New entries for the size range to autocontour
void CAutoContouringDlg::OnKillfocusEditACMinsize()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

void CAutoContouringDlg::OnKillfocusEditACMaxsize()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Respond to a slider message, which may or may not be a change
void CAutoContouringDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
  bool dropping = nSBCode != TB_THUMBTRACK;
  CWnd *wnd;

  // Keep track of changes in actual integer value
  int lastLower = m_intLowerMean;
  int lastUpper = m_intUpperMean;
  int lastSize = m_intMinSize;
  int lastSD = m_intSquareSD;
  int lastIrregular = m_intIrregular;
  int lastBorder = m_intBorderDist;
  bool changed = false;

  UPDATE_DATA_TRUE;
  changed = lastBorder != m_intBorderDist || lastLower != m_intLowerMean ||
    lastUpper != m_intUpperMean || lastSize != m_intMinSize || lastSD != m_intSquareSD ||
    lastIrregular != m_intIrregular;
  CSliderCtrl *pSlider = (CSliderCtrl *)pScrollBar;

  // Compute cutoff from integer value of slider; set bold while adjusting
  if (pSlider == &m_sliderLowerMean) {
    mParams.lowerMeanCutoff = (float)(m_intLowerMean * (mMedianMean - mStatMinMean) / 
      255. + mStatMinMean);
    m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff);
    wnd = GetDlgItem(IDC_STAT_MIN_MEAN_LABEL);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderUpperMean) {
    mParams.upperMeanCutoff = (float)(m_intUpperMean * (mStatMaxMean - mMedianMean) / 
      255. + mMedianMean);
    m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff);
    wnd = GetDlgItem(IDC_STAT_MAX_MEAN_LABEL2);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderSquareSD) {
    mParams.SDcutoff = (float)(m_intSquareSD * (mStatMaxSD - mStatMinSD) / 255. + 
      mStatMinSD);
    m_strSquareSD.Format("%.4g", mParams.SDcutoff);
    wnd = GetDlgItem(IDC_STAT_PCT_BLACK_LABEL);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderMinSize) {
    mParams.minSizeCutoff = (float)(m_intMinSize * (mStatMaxSize - mStatMinSize) /
      255. + mStatMinSize);
    m_strMinSize.Format("%.1f", mParams.minSizeCutoff);
    wnd = GetDlgItem(IDC_EDIT_MIN_SIZE);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderIrregular) {
    mParams.irregularCutoff = (float)(m_intIrregular * (mStatMaxSquareness - mStatMinSquareness)
      / 255. + mStatMinSquareness);
    m_strIrregularity.Format("%.2f", mParams.irregularCutoff);
    wnd = GetDlgItem(IDC_STAT_IRREGULAR_LABEL);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  if (pSlider == &m_sliderBorderDist) {
    mParams.borderDistCutoff = (float)(m_intBorderDist * mStatMaxBoundDist / 255.);
    m_strBorderDist.Format("%.1f", mParams.borderDistCutoff);
    wnd = GetDlgItem(IDC_STAT_BORDER_DIST_LABEL);
    wnd->SetFont(dropping ? m_statShowGroups.GetFont() : mBoldFont);
  }
  UpdateData(false);
  if (changed)
    SetExclusionsAndGroups();
  if (dropping)
    mWinApp->RestoreViewFocus();
  CDialog::OnHScroll(nSBCode, nPos, pScrollBar);

}

// Enable dialog items that govern autocontouring
void CAutoContouringDlg::ManageACEnables()
{
  EnableDlgItem(IDC_EDIT_REL_THRESH, !m_iThreshType);
  EnableDlgItem(IDC_EDIT_ABS_THRESH, m_iThreshType);
  EnableDlgItem(IDC_IDC_EDIT_TO_PIXELS, !m_iReduceToWhich);
  EnableDlgItem(IDC_EDIT_TO_PIX_SIZE, m_iReduceToWhich);
  EnableDlgItem(IDC_BUT_MAKE_CONTOURS, !mWinApp->DoingTasks());
  EnableDlgItem(IDC_BUT_CLEAR_POLYS, mHaveConts && !mWinApp->DoingTasks());
}

// Manage dialog items that depend on contours existing
void CAutoContouringDlg::ManagePostEnables(bool forBusy)
{
  BOOL doingTasks = mWinApp->DoingTasks();
  if (!mIsOpen)
    return;
  EnableDlgItem(IDC_BUT_CREATE_POLYS, mHaveConts && !doingTasks);
  EnableDlgItem(IDC_BUT_UNDO_POLYS, IsUndoFeasible() && !doingTasks);
  if (forBusy)
    return;

  // Enable for having contours
  m_sliderLowerMean.EnableWindow(mHaveConts);
  m_sliderUpperMean.EnableWindow(mHaveConts);
  m_sliderMinSize.EnableWindow(mHaveConts);
  m_sliderIrregular.EnableWindow(mHaveConts);
  m_sliderBorderDist.EnableWindow(mHaveConts);
  m_sliderSquareSD.EnableWindow(mHaveConts);
  EnableDlgItem(IDC_EDIT_MIN_MEAN, mHaveConts);
  EnableDlgItem(IDC_EDIT_MAX_MEAN, mHaveConts);
  EnableDlgItem(IDC_EDIT_MIN_SIZE, mHaveConts);
  EnableDlgItem(IDC_EDIT_BORDER_DIST, mHaveConts);
  if (mHaveConts) {

    // Put the ranges on the sliders or blank them out
    m_strMinLowerMean.Format("%.4g", mStatMinMean);
    m_strMaxLowerMean.Format("%.4g", mMedianMean);
    m_strMinUpperMean.Format("%.4g", mMedianMean);
    m_strMaxUpperMean.Format("%.4g", mStatMaxMean);
    m_strMinSquareSD.Format("%.4g", mStatMinSD);
    m_strMaxSquareSD.Format("%.4g", mStatMaxSD);
    m_strMinMinSize.Format("%.1f", mStatMinSize);
    m_strMaxMinSize.Format("%.1f", mStatMaxSize);
    m_strMinIrregular.Format("%.2f", mStatMinSquareness);
    m_strMaxIrregular.Format("%.2f", mStatMaxSquareness);
    m_strMinBorderDist = "0";
    m_strMaxBorderDist.Format("%.1f", mStatMaxBoundDist);

  } else {
    m_strMinLowerMean = "";
    m_strMaxLowerMean = "";
    m_strMinUpperMean = "";
    m_strMaxUpperMean = "";
    m_strMinSquareSD = "";
    m_strMaxSquareSD = "";
    m_strMinMinSize = "";
    m_strMaxMinSize = "";
    m_strMinIrregular = "";
    m_strMaxIrregular = "";
    m_strMinBorderDist = "";
    m_strMaxBorderDist = "";
  }
  UpdateData(false);
}

// Enable group selectors up to the chosen number and set if argument says to
void CAutoContouringDlg::ManageGroupSelectors(int set)
{
  int ind;
  BOOL busy = mWinApp->DoingTasks() && !mWinApp->GetJustNavAcquireOpen();
  CButton *but;
  for (ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++) {
    EnableDlgItem(IDC_CHECK_GROUP1 + ind, ind < mNumGroups && mHaveConts && !busy &&
      mNumInGroup[ind] > 0);
    if (set) {
      but = (CButton *)GetDlgItem(IDC_CHECK_GROUP1 + ind);
      if (but)
        but->SetCheck((mShowGroup[ind] || set == 1) ? 1 : 0);
    }
  }
}

// Copy parameters into dialog, including setting sliders from cutoff values
void CAutoContouringDlg::ParamsToDialog()
{
  m_iReducePixels = mParams.targetSizePixels;
  m_fReduceToPixSize = mParams.targetPixSizeUm;
  m_iReduceToWhich = mParams.usePixSize;
  m_fACminSize = mParams.minSize;
  m_fACmaxSize = mParams.maxSize;
  m_fRelThresh = mParams.relThreshold;
  m_fAbsThresh = mParams.absThreshold;
  m_iThreshType = mParams.useAbsThresh;
  mNumGroups = mParams.numGroups;
  m_strShowGroups.Format("Split into %d", mNumGroups);
  m_iGroupType = mParams.groupByMean;
  if (mIsOpen) {
    m_sbcNumGroups.SetPos(mNumGroups);
    ManageGroupSelectors(0);
  }

  if (mHaveConts) {

    // Adjuts some cutoffs to be in range
    if (mParams.lowerMeanCutoff < EXTRA_VALUE_TEST ||
      mParams.lowerMeanCutoff > mMedianMean)
      mParams.lowerMeanCutoff = mStatMinMean;
    if (mParams.upperMeanCutoff < EXTRA_VALUE_TEST ||
      mParams.upperMeanCutoff < mMedianMean)
      mParams.upperMeanCutoff = mStatMaxMean;
    if (mParams.SDcutoff < EXTRA_VALUE_TEST || mParams.SDcutoff < mStatMinSD)
      mParams.SDcutoff = mStatMaxSD;
    if (mParams.minSizeCutoff < mStatMinSize)
      mParams.minSizeCutoff = mStatMinSize;

    // Set the cutoff static text and compute slider values
    m_strSquareSD.Format("%.4g", mParams.SDcutoff);
    m_strIrregularity.Format("%.2f", mParams.irregularCutoff);
    m_intLowerMean = (int)(255. * (mParams.lowerMeanCutoff - mStatMinMean) /
      (mMedianMean - mStatMinMean));
    m_intUpperMean = (int)(255. * (mParams.upperMeanCutoff - mMedianMean) /
      (mStatMaxMean - mMedianMean));
    m_intSquareSD = (int)(255. * (mParams.SDcutoff - mStatMinSD) /
      (mStatMaxSD - mStatMinSD));
    m_intMinSize = (int)(255. * (mParams.minSizeCutoff - mStatMinSize) /
      (mStatMaxSize - mStatMinSize));
    m_intIrregular = (int)(255. * (mParams.irregularCutoff - mStatMinSquareness) /
      (mStatMaxSquareness - mStatMinSquareness));
    m_intBorderDist = (int)(255. * mParams.borderDistCutoff / mStatMaxBoundDist);
    B3DCLAMP(m_intLowerMean, 0, 255);
    B3DCLAMP(m_intUpperMean, 0, 255);
    B3DCLAMP(m_intSquareSD, 0, 255);
    B3DCLAMP(m_intMinSize, 0, 255);
    B3DCLAMP(m_intIrregular, 0, 255);
    B3DCLAMP(m_intBorderDist, 0, 255);
  } else {

    // Or blank things out
    m_strSquareSD = "";
    m_strIrregularity = "";
    m_intLowerMean = 0;
    m_intUpperMean = 255;
    m_intSquareSD = 255;
    m_intIrregular = 255;
    m_intBorderDist = 0;
  }

  // Set the cutoff text boxes regardless
  m_strLowerMean.Format("%.4g", mParams.lowerMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.lowerMeanCutoff : 0.);
  m_strUpperMean.Format("%.4g", mParams.upperMeanCutoff > EXTRA_VALUE_TEST ?
    mParams.upperMeanCutoff : 0.);
  m_strMinSize.Format("%.1f", mParams.minSizeCutoff > EXTRA_VALUE_TEST ?
    mParams.minSizeCutoff : 0.);
  m_strBorderDist.Format("%.1f", mParams.borderDistCutoff > EXTRA_VALUE_TEST ?
    mParams.minSizeCutoff : 0.);
  if (mIsOpen)
    UpdateData(false);
}

// Copy dialog variables to parameters
void CAutoContouringDlg::DialogToParams()
{
  if (!mIsOpen)
    return;
  mParams.targetSizePixels = m_iReducePixels;
  mParams.targetPixSizeUm = m_fReduceToPixSize;
  mParams.usePixSize = m_iReduceToWhich;
  mParams.minSize = m_fACminSize;
  mParams.maxSize = m_fACmaxSize;
  mParams.relThreshold = m_fRelThresh;
  mParams.absThreshold = m_fAbsThresh;
  mParams.useAbsThresh = m_iThreshType;
  mParams.numGroups = mNumGroups;
  mParams.groupByMean = m_iGroupType;
}


// AUTOCONTOURING
//
// Top function to start the process; all the error checking is in the thread
void CAutoContouringDlg::AutoContourImage(EMimageBuffer *imBuf, float targetSizeOrPix,
  float minSize, float maxSize, float interPeakThresh, float useThresh)
{
  AutoContData *acd = &mAutoContData;
  ScaleMat aMat;
  float delX, delY;

  // Except check this now, instead of at the end
  if (!mWinApp->mNavigator->BufferStageToImage(imBuf, aMat, delX, delY)) {
    SEMMessageBox("Cannot autocontour; the image deoes not enough information to convert"
      " contours to polygons");
    return;
  }
  OnButClearData();

  // Load the data
  mAutoContFailed = true;
  mDoingAutocont = true;
  acd->pixel = mWinApp->mShiftManager->GetPixelSize(imBuf);
  acd->imBuf = imBuf;
  acd->targetSizeOrPix = targetSizeOrPix;
  acd->minSize = minSize;
  acd->maxSize = maxSize;
  acd->interPeakThresh = interPeakThresh;
  acd->useThresh = useThresh;
  acd->tdata = acd->xlist = acd->ylist = NULL;
  acd->fdata = acd->idata = NULL;
  acd->linePtrs = NULL;
  acd->obj = NULL;
  acd->errString = "";
  acd->needRefill = acd->imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW;
  acd->needReduce = false;
  acd->holeFinder = mWinApp->mNavHelper->mFindHoles;

  // Start thread
  mAutoContThread = AfxBeginThread(AutoContProc, &mAutoContData,
    THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  mAutoContThread->m_bAutoDelete = false;
  mAutoContThread->ResumeThread();
  mWinApp->AddIdleTask(TASK_AUTO_CONTOUR, 0, 0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "AUTOCONTOURING");
}

// The thread process
UINT CAutoContouringDlg::AutoContProc(LPVOID pParam)
{
  AutoContData *acd = (AutoContData *)pParam;
  int nxBuf, nyBuf, longest, longix, longiy, idir, err = 0, nxRed, nyRed, dataSize;
  int numSamples, trim = 8, listSize, ind, ix, iy, numThreads;
  float xOffset = 0., yOffset = 0.;
  KImage *image = acd->imBuf->mImage;
  Ival val;
  int coNum, pt;
  Icont *cont;
  bool targetIsPix = acd->targetSizeOrPix < 10.;
  int mode = image->getType();
  unsigned char **zoomPtrs;
  float firstVal, lastVal, histDip, peakBelow, peakAbove, sampMin, sampMax, sampMean;
  const int maxSamples = 50000, histSize = 1000;
  float samples[maxSamples], bins[histSize], base;
  float minScale, maxScale, scaleFac;
  float sqrtBaseFac = 0.05f, sigma = 2.5f;
  Islice slSource;
  float fval, redFac, newFill, fillVal = -1.e30f;
  //double wallStart = wallTime();

  // Set up variables and initialize source slice with input data
  acd->imIsBytes = mode == MRC_MODE_BYTE;
  acd->slRefilled = &slSource;
  acd->slReduced = &slSource;
  image->getSize(nxBuf, nyBuf);
  sliceInit(&slSource, nxBuf, nyBuf, mode, image->getData());
  dataSizeForMode(mode, &dataSize, &idir);

  // Check conditions: is there scaling needed to scale a threshold
  if (!acd->pixel && (targetIsPix || acd->minSize || acd->maxSize)) {
    acd->errString = "The image has no pixel size and can be used only with a target size"
      "and no min or max contour sizes";
    return 1;
  }

  if (!acd->imIsBytes && !acd->imBuf->mImageScale) {
    acd->errString = "There is no scaling information in this image buffer, needed "
      "because it is not bytes";
    return 1;
  }
  if (acd->imBuf->mImageScale) {
    minScale = acd->imBuf->mImageScale->GetMinScale();
    maxScale = acd->imBuf->mImageScale->GetMaxScale();
  }

  if (acd->interPeakThresh <= 0. && acd->useThresh < 1.) {
    if (!acd->imBuf->mImageScale) {
      acd->errString = "There is no scaling information in this image buffer, needed "
        "because a relative threshold was specified";
      return 1;
    }
    acd->useThresh = minScale + acd->useThresh * (maxScale - minScale);
  }

  // Determine reduction
  if (targetIsPix) {
    redFac = acd->targetSizeOrPix / acd->pixel;
  } else {
    redFac = sqrt(nxBuf * nyBuf / (acd->targetSizeOrPix * acd->targetSizeOrPix));
  }
  acd->needReduce = redFac > 1.25;

  // Find the fill value and set that refill is needed if there is a long enough stretch
  if (acd->needRefill) {
    sliceFindFillValue(&slSource, &fillVal, &longest, &longix, &longiy, &idir);
    acd->needRefill = longest > 50;
  }

  // Make sure this is now NULL in case of error cleanup before it is allocated
  if (acd->needRefill)
    acd->slRefilled = NULL;

  // Set up the reduction
  if (acd->needReduce) {
    acd->slReduced = NULL;
    nxRed = (int)(nxBuf / redFac);
    nyRed = (int)(nyBuf / redFac);
    xOffset = (float)(nxBuf - redFac * nxRed) / 2.f;
    yOffset = (float)(nyBuf - redFac * nyRed) / 2.f;
    if (nxRed < 100 || nyRed < 100) {
      acd->errString.Format("The size reduction by %.1f would make the size too small,"
        " %d x %d", redFac, nxRed, nyRed);
      return 1;
    }
    if (selectZoomFilter(5, 1. / redFac, &idir)) {
      acd->errString.Format("Error setting filter for reducing by %.1f", redFac);
      return 1;
    }
  } else {
    redFac = 1.;
    nxRed = nxBuf;
    nyRed = nyBuf;
  }

  // Adjust size limits in pixel, set default for min
  acd->minSize /= acd->pixel * redFac;
  acd->maxSize /= acd->pixel * redFac;
  acd->minSize *= acd->minSize;
  acd->maxSize *= acd->maxSize;
  if (!acd->minSize)
    acd->minSize = 10;

  // Get number of threads
  numThreads = B3DNINT(sqrt(nxRed * nyRed / 50.));
  B3DCLAMP(numThreads, 1, MAX_AUTO_SLICE_THREADS);
  numThreads = numOMPthreads(numThreads);

  // Get arrays if needed for refill, reduction, and byte conversion
  if (acd->needRefill) {
    acd->slRefilled = sliceCreate(nxBuf, nyBuf, mode);
    if (!acd->slRefilled) {
      acd->errString = "Error allocating slice for copy of image to replace fill in";
      return 1;
    }
  }

  acd->slReduced = acd->slRefilled;
  if (acd->needReduce) {
    acd->slReduced = sliceCreate(nxRed, nyRed, mode);
    if (!acd->slReduced) {
      acd->errString = "Error allocating slice for copy of image to replace fill in";
      return 1;
    }
  }

  acd->idata = acd->slReduced->data.b;
  if (!acd->imIsBytes) {
    NewArray(acd->idata, unsigned char, nxRed * nyRed);
    if (!acd->idata) {
      acd->errString = "Error allocating array for byte copy of image";
      return 1;
    }
  }

  // Get the arrays needed internally by the auto routines
  listSize = 4 * (nxRed + nyRed);
  acd->tdata = B3DMALLOC(int, nxRed * nyRed);
  acd->fdata = B3DMALLOC(unsigned char, nxRed * nyRed * numThreads);
  acd->xlist = B3DMALLOC(int, listSize * numThreads);
  acd->ylist = B3DMALLOC(int, listSize * numThreads);
  acd->linePtrs = makeLinePointers(acd->idata, nxRed, nyRed, 1);
  acd->obj = imodObjectNew();
  if (!acd->tdata || !acd->fdata || !acd->xlist || !acd->ylist || !acd->linePtrs ||
    !acd->obj) {
    acd->errString = "Error allocating arrays for contouring the data";
    return 1;
  }

  // Start doing operations: refill
  if (acd->needRefill) {
    memcpy(acd->slRefilled->data.b, image->getData(), dataSize * nxBuf * nyBuf);
    if (!sliceReplaceFill(acd->slRefilled, 1, 1, &fillVal, &newFill))
      fillVal = newFill;
  }

  // Reduce
  if (acd->needReduce) {
    zoomPtrs = makeLinePointers(acd->slRefilled->data.b, nxBuf, nyBuf, dataSize);
    if (!zoomPtrs) {
      acd->errString = "Error making line pointers for reducing image";
      return 1;
    } else {

      err = zoomWithFilter(zoomPtrs, nxBuf, nyBuf, xOffset, yOffset, nxRed, nyRed, nxRed,
        0, mode, acd->slReduced->data.b, NULL, NULL);
      free(zoomPtrs);
      if (err) {
        acd->errString.Format("Error %d reducing image with zoomWithFilter", err);
        return 1;
      }
    }
  }

  // Sampling and histogram for threshold
  if (acd->interPeakThresh > 0.) {

    getSampleOfArray(acd->slReduced->data.b, mode, nxRed, nyRed, 1., trim, trim,
      nxRed - 2 * trim, nyRed - 2 * trim, fillVal, samples, maxSamples, &numSamples);
    fullArrayMinMaxMean(samples, MRC_MODE_FLOAT, numSamples, 1, &sampMin, &sampMax,
      &sampMean);

    // Take square root after adding a base.  Then mimic clip on the trimming
    base = sqrtBaseFac * (sampMean - sampMin) - sampMin;
    for (ind = 0; ind < numSamples; ind++)
      samples[ind] = sqrtf(samples[ind] + base);
    lastVal = -1.e37f;
    firstVal = 1.e37f;
    if (numSamples > 4000) {
      idir = (int)(0.0005 * numSamples);
      firstVal = percentileFloat(idir + 1, samples, numSamples);
      lastVal = percentileFloat(numSamples - idir, samples, numSamples);
    }

    // Find dip, convert back to original values and set threshold
    err = findHistogramDip(samples, numSamples, 0, bins, histSize, firstVal,
      lastVal, &histDip, &peakBelow, &peakAbove, 0);
    if (err) {
      acd->errString = "Failed to find a dip in histogram of square root values";
      return 1;
    } else {
      peakBelow = peakBelow * peakBelow - base;
      peakAbove = peakAbove * peakAbove - base;
      acd->useThresh = acd->interPeakThresh * (peakAbove - peakBelow) + peakBelow;
    }
  }


  // Convert to bytes and adjust threshold for scaling
  if (!acd->imIsBytes) {
    scaleFac = 255.f / (maxScale - minScale);
    acd->useThresh = (acd->useThresh - minScale) * scaleFac;
    for (iy = 0; iy < nyRed; iy++) {
      for (ix = 0; ix < nxRed; ix++) {
        sliceGetVal(acd->slReduced, ix, iy, val);
        fval = (val[0] - minScale) * scaleFac;
        B3DCLAMP(fval, 0.f, 255.f);
        acd->idata[ix + iy * nxRed] = (unsigned char)fval;
      }
    }
  }

  // Do it!
  if (imodAutoContoursFromSlice(sigma, acd->useThresh, 0., -1, 1, (int)acd->minSize,
    (int)acd->maxSize, 1, 0, 0., 0., 2, 3, NULL, 0, acd->obj, NULL, nxRed, nyRed,
    acd->tdata, acd->idata, acd->fdata, acd->xlist, acd->ylist, acd->linePtrs, 0., 0,
    listSize, numThreads)) {
    acd->errString = "Error allocating memory in autocontouring library routine";
    return 1;
  }

  // Get statistics on reduced image
  if (acd->obj->contsize)
    SquareStatistics(acd, nxRed, nyRed, minScale, maxScale, redFac, 2.25f);

  // Scale the contours back to full image coordinates
  for (coNum = 0; coNum < acd->obj->contsize; coNum++) {
    cont = &acd->obj->cont[coNum];
    for (pt = 0; pt < cont->psize; pt++) {
      cont->pts[pt].x = cont->pts[pt].x * redFac + xOffset;
      cont->pts[pt].y = cont->pts[pt].y * redFac + yOffset;
    }
    imodContourReduce(cont, 0.75);
  }

  return 0;
}

int CAutoContouringDlg::AutoContBusy()
{
  return UtilThreadBusy(&mAutoContThread);
}

// Done: convert to polygons
void CAutoContouringDlg::AutoContDone()
{
  AutoContData *acd = &mAutoContData;
  if (!DoingAutoContour())
    return;
  if (!acd->obj->contsize) {
    SEMMessageBox("No contours found - is the \"Range of sizes\" too narrow?");
    StopAutoCont();
    return;
  }
  PrintfToLog("Autocontouring found %d contours", acd->obj->contsize);
  if (acd->obj->contsize > 1) {
    mStatMinMean = *std::min_element(acd->sqrMeans.begin(), acd->sqrMeans.end());
    mStatMaxMean = *std::max_element(acd->sqrMeans.begin(), acd->sqrMeans.end());
    mStatMinSize = *std::min_element(acd->sqrSizes.begin(), acd->sqrSizes.end());
    mStatMaxSize = *std::max_element(acd->sqrSizes.begin(), acd->sqrSizes.end());
    mStatMinSD = *std::min_element(acd->sqrSDs.begin(), acd->sqrSDs.end());
    mStatMaxSD = *std::max_element(acd->sqrSDs.begin(), acd->sqrSDs.end());
    mStatMinSquareness = *std::min_element(acd->squareness.begin(), 
      acd->squareness.end());
    mStatMaxSquareness = *std::max_element(acd->squareness.begin(), 
      acd->squareness.end());
    mStatMaxBoundDist = *std::max_element(acd->boundDists.begin(), acd->boundDists.end());
    if (acd->spacing) {
      mStatMaxBoundDist = B3DMAX(3.f * acd->spacing, mStatMaxBoundDist / 3.f);
    } else {
      mStatMaxSquareness = 1.f;
      mStatMaxBoundDist = 100.f;
    }
  } else {
    mStatMinMean = B3DMIN(0.9f * mMedianMean, 1.1f * mMedianMean);
    mStatMaxMean = B3DMAX(0.9f * mMedianMean, 1.1f * mMedianMean);
    mStatMinSize = 0.9f * acd->sqrSizes[0];
    mStatMaxSize = 1.1f * acd->sqrSizes[0];
    mStatMinSD = 0.9f * acd->sqrSDs[0];
    mStatMaxSD = 1.1f * acd->sqrSDs[0];
    mStatMaxSquareness = 1.f;
    mStatMaxBoundDist = 100.f;
  }
  mMedianMean = acd->medianMean;

  if (mWinApp->mNavigator->ImodObjectToPolygons(acd->imBuf, acd->obj, mPolyArray)) {
    SEMMessageBox("An error occurred converting autocontours to Navigator polygons");
  } else {
    mAutoContFailed = false;
    mHaveConts = true;
    if (mIsOpen)
      ParamsToDialog();
    ManagePostEnables(false);
    SetExclusionsAndGroups();
  }
  StopAutoCont();
}

// This should be OK to call on a timeout, but...
void CAutoContouringDlg::CleanupAutoCont(int error)
{
  if (error == IDLE_TIMEOUT_ERROR) {
    UtilThreadCleanup(&mAutoContThread);
  }
  StopAutoCont();
  if (error)
    SEMMessageBox(_T(error == IDLE_TIMEOUT_ERROR ? "Time out doing autocontouring" :
      "Autocontouring failed: " + mAutoContData.errString));
  mWinApp->ErrorOccurred(error);
}

// Stop function, a bit problematic because stopping the thread doesn't stop the openmp
void CAutoContouringDlg::StopAutoCont()
{
  AutoContData *acd = &mAutoContData;
  double startTime = GetTickCount();
  int stillRunning;
  imodAutoContourStop();
  while (UtilThreadBusy(&mAutoContThread) && SEMTickInterval(startTime) < 3000) {
    Sleep(50);
  }
  stillRunning = UtilThreadBusy(&mAutoContThread);
  if (stillRunning) {

    // Cannot even suspend the thread without things going whacky 
    mAutoContThread = NULL;
    mWinApp->AppendToLog("Autocontouring thread did not end on signal, cannot free "
      "memory");

    // These are safe to free as long as contouring isn't using them
    if (!acd->imIsBytes && acd->needRefill)
      sliceFree(acd->slRefilled);
    if (!acd->imIsBytes && acd->needReduce)
      sliceFree(acd->slReduced);
  } else {
    UtilThreadCleanup(&mAutoContThread);
    if (acd->obj)
      imodObjectDelete(acd->obj);
    free(acd->xlist);
    free(acd->ylist);
    free(acd->tdata);
    free(acd->fdata);
    free(acd->linePtrs);
    if (acd->needRefill)
      sliceFree(acd->slRefilled);
    if (acd->needReduce)
      sliceFree(acd->slReduced);
    if (!acd->imIsBytes)
      free(acd->idata);
  }
  mDoingAutocont = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "");
}

// Get various statistic about the contours for selection
void CAutoContouringDlg::SquareStatistics(AutoContData *acd, int nxRed, int nyRed, 
  float minScale, float maxScale, float redFac, float outlieCrit)
{
  int ind, ix, iy, ixStart, iyStart, ixEnd, iyEnd, nsum, numConts, xbase, numBelow;
  int ixSum, iySum;
  float avg, sd, val, fracLow, size, perim = 0., scaleFac, xcen;
  float avgAngle, cosAng, sinAng, xVecs[3], yVecs[3];
  float half, delx, dely, left, right, scaleForScan, area, sizeScale = redFac;
  double sum, sumsq;
  int minForScan = 30, maxSize = 0;
  Icont *cont, *rotCont, *scanCont;
  Ipoint bbMin, bbMax, tpt;
  FloatVec tmpVec, valVec, xCenters, yCenters, xBound, yBound, peaks, altPeaks;
  IntVec altInds;

  if (!acd->imIsBytes)
    scaleFac = 255.f / (maxScale - minScale);
  if (acd->pixel)
    sizeScale *= acd->pixel;
  acd->spacing = 0.;

  numConts = acd->obj->contsize;

  // Resize vectors to hold the data
  acd->sqrMeans.resize(numConts);
  acd->sqrSDs.resize(numConts);
  acd->sqrSizes.resize(numConts);
  acd->squareness.clear();
  acd->squareness.resize(numConts, 0.);
  acd->pctBlackPix.resize(numConts);
  acd->boundDists.clear();
  acd->boundDists.resize(numConts, 0.);
  xCenters.resize(numConts);
  yCenters.resize(numConts);
  xBound.resize(numConts);
  yBound.resize(numConts);


  for (ind = 0; ind < numConts; ind++) {
    sum = 0.;
    sumsq = 0.;
    nsum = 0;
    avg = 0.;
    sd = 0.;
    ixSum = 0;
    iySum = 0;
    valVec.clear();
    cont = &acd->obj->cont[ind];
    ACCUM_MAX(maxSize, cont->psize);
    size = sqrtf(imodContourArea(cont));
    acd->sqrSizes[ind] = size * sizeScale;
    /*perim = imodContourLength(cont, 1);
    acd->squareness[ind] = 0.25f * perim / size;
    if (acd->squareness[ind] < 1.)
      acd->squareness[ind] = 2.f - acd->squareness[ind]; */

    // Loop on bounding box, find pixels inside, accumulate values
    imodContourGetBBox(cont, &bbMin, &bbMax);
    xCenters[ind] = (bbMin.x + bbMax.x) / 2.f;
    yCenters[ind] = (bbMin.y + bbMax.y) / 2.f;
    ixStart = B3DMAX(0, (int)bbMin.x - 1);
    ixEnd = B3DMIN(nxRed - 1, (int)bbMax.x + 2);
    iyStart = B3DMAX(0, (int)bbMin.y - 1);
    iyEnd = B3DMIN(nyRed - 1, (int)bbMax.y + 2);
    for (iy = iyStart; iy <= iyEnd; iy++) {
      for (ix = ixStart; ix <= ixEnd; ix++) {
        tpt.x = (float)ix;
        tpt.y = (float)iy;
        if (imodPointInsideCont(cont, &tpt)) {
          xbase = ix + iy * nxRed;
          val = acd->idata[xbase];
          sum += val;
          sumsq += val * val;
          nsum++;
          valVec.push_back(val);
          ixSum += ix;
          iySum += iy;
        }
      }
    }

    // Get mean. sd, and centroid for center
    if (nsum) {
      sumsToAvgSDdbl(sum, sumsq, nsum, 1, &avg, &sd);
      xCenters[ind] = (float)ixSum / (float)nsum;
      yCenters[ind] = (float)iySum / (float)nsum;
    }
    if (!acd->imIsBytes) {
      sd /= scaleFac;
      avg = avg / scaleFac + minScale;
    }
    acd->sqrMeans[ind] = avg;
    acd->sqrSDs[ind] = sd;

    // Just in case someone wants % black pixels...
    fracLow = 0.;
    if (nsum >= 10) {
      tmpVec.resize(nsum);
      rsMadMedianOutliers(&valVec[0], nsum, outlieCrit, &tmpVec[0]);
      numBelow = 0;
      for (ix = 0; ix < nsum; ix++)
        if (tmpVec[ix] < 0)
          numBelow++;
      fracLow = (float)numBelow / nsum;

    }
    acd->pctBlackPix[ind] = fracLow;
  }

  if (numConts > 1)
    rsFastMedian(&acd->sqrMeans[0], numConts, &xBound[0], &acd->medianMean);
  else
    acd->medianMean = acd->sqrMeans[0];

  // Squareness and edge distance computation: skip if too few
  if (numConts > 3) {

    // Get the grid angle
    acd->holeFinder->analyzeNeighbors(xCenters, yCenters, peaks, altInds, xBound, yBound,
      altPeaks, 0., 0., 0, xcen, tmpVec, valVec);
    acd->holeFinder->getGridVectors(xVecs, yVecs, avgAngle, acd->spacing, -1);
    cosAng = (float)cos(DTOR * avgAngle);
    sinAng = (float)sin(DTOR * avgAngle);

    // Get a contour and allocated points for rotated, scaled contours
    rotCont = imodContourNew();
    if (rotCont) {
      rotCont->pts = B3DMALLOC(Ipoint, maxSize);
      if (!rotCont->pts) {
        imodContourDelete(rotCont);
        rotCont = NULL;
      }
    }
    if (rotCont) {
      for (ind = 0; ind < numConts; ind++) {
        cont = &acd->obj->cont[ind];

        // Set up equivalent box, determine how much to scale for good area measures
        half = 0.5f * acd->sqrSizes[ind] / sizeScale;
        scaleForScan = B3DMAX(1.f, minForScan / (2.f * half));
        half *= scaleForScan;

        // Rotate the contour
        rotCont->psize = cont->psize;
        for (ix = 0; ix < cont->psize; ix++) {
          delx = cont->pts[ix].x - xCenters[ind];
          dely = cont->pts[ix].y - yCenters[ind];
          rotCont->pts[ix].x = scaleForScan * (cosAng * delx + sinAng * dely);
          rotCont->pts[ix].y = scaleForScan * (-sinAng * delx + cosAng * dely);
        }

        // Get a scan contour
        scanCont = imodel_contour_scan(rotCont);
        area = 0.;
        if (scanCont) {
          for (ix = 0; ix < scanCont->psize; ix += 2) {

            // segment outside area in Y: add it entirely
            if (scanCont->pts[ix].y > half || scanCont->pts[ix].y < -half) {
              area += scanCont->pts[ix + 1].x - scanCont->pts[ix].x;
              continue;
            }

            // Segment at same Y as last: consider the gap first, add part inside
            if (ix && scanCont->pts[ix - 1].y == scanCont->pts[ix].y) {
              left = B3DMAX(scanCont->pts[ix - 1].x, -half);
              right = B3DMIN(scanCont->pts[ix].x, half);
              if (right > left)
                area += right - left;
            } else if (scanCont->pts[ix].x > -half) {

              // If no gap to left, add in part to left of this segment
              area += B3DMIN(scanCont->pts[ix].x, half) + half;
            }

            // Add parts of segment outside
            if (scanCont->pts[ix].x < -half)
              area += -half - scanCont->pts[ix].x;
            if (scanCont->pts[ix + 1].x > half)
              area += scanCont->pts[ix + 1].x - half;

            // If ends inside and no gap to right, add to right
            else if (ix + 2 < scanCont->psize &&
              scanCont->pts[ix + 2].y > scanCont->pts[ix].y)
              area += half - scanCont->pts[ix + 1].x;
          }
          imodContourDelete(scanCont);
        }

        // Divide by area of scaled square
        acd->squareness[ind] = area / (4.f * half * half);
      }
      imodContourDelete(rotCont);
    }

    FindDistancesFromHull(xCenters, yCenters, xBound, yBound, sizeScale, acd->boundDists);
  }
}

// General function to find convex boundary of a set of points and compute distance of
// each point from the hull
int CAutoContouringDlg::FindDistancesFromHull(FloatVec &xCenters, FloatVec &yCenters, 
  FloatVec &xBound, FloatVec &yBound, float sizeScale, FloatVec &boundDists, 
  bool useBound)
{
  int numConts = (int)xCenters.size();
  float xcen, ycen;
  Ipoint tpt;
  Icont *cont;
  int numBelow, ind, ix;

  // Get convex boundary of centers
  if (useBound)
    numBelow = (int)xBound.size();
  else
    convexBound(&xCenters[0], &yCenters[0], numConts, 0., 0., &xBound[0], &yBound[0],
      &numBelow, &xcen, &ycen, numConts);

  // Put boundary in a contour and get distances
  cont = imodContourNew();
  if (!cont)
    return 1;
  cont->pts = B3DMALLOC(Ipoint, numBelow);
  if (cont->pts) {
    cont->psize = numBelow;
    for (ind = 0; ind < numBelow; ind++) {
      cont->pts[ind].x = xBound[ind];
      cont->pts[ind].y = yBound[ind];
      cont->pts[ind].z = 0.;
    }

    // Get distance of each center from boundary
    for (ind = 0; ind < numConts; ind++) {
      tpt.x = xCenters[ind];
      tpt.y = yCenters[ind];
      boundDists[ind] = sizeScale * imodPointContDistance(cont, &tpt, 0, 0, &ix);
      if (useBound && !imodPointInsideCont(cont, &tpt))
        boundDists[ind] = -boundDists[ind];
    }
  }
  imodContourDelete(cont);
  return 0;
}

