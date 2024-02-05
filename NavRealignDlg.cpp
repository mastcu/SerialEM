// NavRealignDlg.cpp : Set parameters for template align and for clearing image shift
//                     after template align and realign to item
//
// Copyright (C) 2021 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "NavRealignDlg.h"
#include "NavigatorDlg.h"
#include "EMbufferManager.h"
#include "Utilities\KGetOne.h"


static int sIdTable[] = {IDC_STAT_TEMPLATE_TITLE, IDC_STAT_TEMP_LABEL, IDC_EDIT_MAP_LABEL,
IDC_STAT_NOT_EXIST, IDC_STAT_BUFFER_ROLLS, IDC_MAKE_TEMPLATE_MAP, IDC_STAT_LOAD_KEEP,
IDC_STAT_SELECTED_BUF, IDC_SPIN_TEMPLATE_BUF, IDC_EDIT_MAX_SHIFT, IDC_STAT_ALIGN_LIMIT,
IDC_STAT_MAXALI_UM, IDC_STAT_LINE, IDC_STAT_WHY_A_NO_GOOD, PANEL_END,
IDC_STAT_RESET_IS_TITLE, IDC_CHECK_RESET_IS, IDC_STAT_PARAMS_SHARED, IDC_STAT_ONLY_ONCE,
IDC_EDIT_RESET_IS, IDC_STAT_RESET_UM, IDC_STAT_REPEAT_RESET, IDC_STAT_RESET_TIMES,
IDC_SPIN_RESET_TIMES, IDC_CHECK_LEAVE_IS_ZERO, PANEL_END,
IDC_STAT_SCALED_TITLE, IDC_STAT_EXTRA_AREA,
IDC_EDIT_EXTRA_FOVS, IDC_STAT_X_FOVS, IDC_STAT_SIZE_CHANGE, IDC_EDIT_MAX_PCT_CHANGE, 
IDC_STAT_PCT, IDC_STAT_ROTATION, IDC_EDIT_MAX_ROTATION, IDC_STAT_DEG, IDC_STAT_LINE3,
IDC_STAT_REF_MAP_LABEL, IDC_STAT_REF_MAP_BUF, IDC_SPIN_REF_MAP_BUF, 
IDC_STAT_REF_BUF_ROLLS, PANEL_END,
IDOK, IDCANCEL, IDC_BUTHELP, IDC_STAT_SPACER, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

// CNavRealignDlg dialog

CNavRealignDlg::CNavRealignDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_NAVREALIGN, pParent)
  , m_strMapLabel(_T(""))
  , m_strSelectedBuf(_T(""))
  , m_fAlignShift(0.05f)
  , m_fResetThresh(0.02f)
  , m_strRepeatReset(_T(""))
  , m_bLeaveISzero(FALSE)
  , m_bResetIS(FALSE)
  , m_strRefMapBuf(_T(""))
  , m_fExtraFOVs(0)
  , m_fMaxPctChange(0)
  , m_fMaxRotation(0)
{
  mForRealignOnly = 0;
  mMaxRotChgd = mMaxPctChgd = false;
}

CNavRealignDlg::~CNavRealignDlg()
{
}

void CNavRealignDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_MAP_LABEL, m_strMapLabel);
  DDX_Control(pDX, IDC_MAKE_TEMPLATE_MAP, m_butMakeMap);
  DDX_Text(pDX, IDC_STAT_SELECTED_BUF, m_strSelectedBuf);
  DDX_Text(pDX, IDC_EDIT_MAX_SHIFT, m_fAlignShift);
  DDV_MinMaxFloat(pDX, m_fAlignShift, 0.02f, 25.f);
  DDX_Control(pDX, IDC_EDIT_RESET_IS, m_editResetIS);
  DDX_Text(pDX, IDC_EDIT_RESET_IS, m_fResetThresh);
  DDV_MinMaxFloat(pDX, m_fResetThresh, 0.01f, 20.f);
  DDX_Control(pDX, IDC_STAT_REPEAT_RESET, m_statRepeatReset);
  DDX_Text(pDX, IDC_STAT_REPEAT_RESET, m_strRepeatReset);
  DDX_Control(pDX, IDC_STAT_RESET_TIMES, m_statResetTimes);
  DDX_Control(pDX, IDC_SPIN_RESET_TIMES, m_sbcResetTimes);
  DDX_Control(pDX, IDC_CHECK_LEAVE_IS_ZERO, m_butLeaveISzero);
  DDX_Check(pDX, IDC_CHECK_LEAVE_IS_ZERO, m_bLeaveISzero);
  DDX_Control(pDX, IDC_SPIN_TEMPLATE_BUF, m_sbcTemplateBuf);
  DDX_Check(pDX, IDC_CHECK_RESET_IS, m_bResetIS);
  DDX_Control(pDX, IDC_SPIN_REF_MAP_BUF, n_sbcRefMapBuf);
  DDX_Control(pDX, IDC_STAT_REF_MAP_BUF, m_statRefMapBuf);
  DDX_Text(pDX, IDC_STAT_REF_MAP_BUF, m_strRefMapBuf);
  DDX_Text(pDX, IDC_EDIT_EXTRA_FOVS, m_fExtraFOVs);
  DDV_MinMaxFloat(pDX, m_fExtraFOVs, 0, 3.);
  DDX_Text(pDX, IDC_EDIT_MAX_PCT_CHANGE, m_fMaxPctChange);
  DDV_MinMaxFloat(pDX, m_fMaxPctChange, 0, 25.);
  DDX_Text(pDX, IDC_EDIT_MAX_ROTATION, m_fMaxRotation);
  DDV_MinMaxFloat(pDX, m_fMaxRotation, 0, 25.);
  DDX_Control(pDX, IDC_STAT_TEMPLATE_TITLE, m_statTemplateTitle);
  DDX_Control(pDX, IDC_STAT_RESET_IS_TITLE, m_statResetISTitle);
  DDX_Control(pDX, IDC_STAT_SCALED_TITLE, m_statScaledTitle);
}


BEGIN_MESSAGE_MAP(CNavRealignDlg, CBaseDlg)
  ON_EN_KILLFOCUS(IDC_EDIT_MAP_LABEL, OnEnKillfocusEditMapLabel)
  ON_BN_CLICKED(IDC_MAKE_TEMPLATE_MAP, OnMakeTemplateMap)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_SHIFT, OnEnKillfocusEditMaxShift)
  ON_EN_KILLFOCUS(IDC_EDIT_RESET_IS, OnEnKillfocusEditMaxShift)
  ON_BN_CLICKED(IDC_CHECK_RESET_IS, OnCheckResetIs)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_RESET_TIMES, OnDeltaposSpinResetTimes)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_TEMPLATE_BUF, OnDeltaposSpinTemplateBuf)
  ON_BN_CLICKED(IDC_CHECK_LEAVE_IS_ZERO, OnCheckLeaveIsZero)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_REF_MAP_BUF, OnDeltaposSpinRefMapBuf)
  ON_EN_KILLFOCUS(IDC_EDIT_EXTRA_FOVS, OnKillfocusEditExtraFovs)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_PCT_CHANGE, OnKillfocusEditMaxPctChange)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_ROTATION, OnKillfocusEditMaxRotation)
END_MESSAGE_MAP()


// CNavRealignDlg message handlers
BOOL CNavRealignDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  BOOL states[4] = {true, true, true, true};
  CFont *boldFont = mWinApp->GetBoldFont(&m_statTemplateTitle);
  mHelper = mWinApp->mNavHelper;
  m_sbcResetTimes.SetRange(0, 10000);
  m_sbcTemplateBuf.SetRange(0, 10000);
  n_sbcRefMapBuf.SetRange(0, 10000);
  m_sbcResetTimes.SetPos(5000);
  m_sbcTemplateBuf.SetPos(5000);
  n_sbcRefMapBuf.SetPos(5000);
  mMasterParams = mHelper->GetNavAlignParams();
  mParams = *mMasterParams;
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  states[0] = mForRealignOnly <= 0;
  states[2] = mForRealignOnly >= 0;

  m_strMapLabel = mParams.templateLabel;
  m_fAlignShift = mParams.maxAlignShift;
  m_fResetThresh = mParams.resetISthresh;
  m_bLeaveISzero = mParams.leaveISatZero;
  mMaxNumResets = B3DMAX(1, B3DABS(mParams.maxNumResetIS));
  m_bResetIS = mParams.maxNumResetIS > 0;
  m_strRepeatReset.Format("Reset IS up to %d", mMaxNumResets);
  B3DCLAMP(mParams.loadAndKeepBuf, 1, MAX_BUFFERS - 1);
  char letter = 'A' + mParams.loadAndKeepBuf;
  m_strSelectedBuf = letter;
  m_fExtraFOVs = mParams.scaledAliExtraFOV;
  m_fMaxPctChange = mHelper->GetScaledRealignPctChg();
  m_fMaxRotation = mHelper->GetScaledRealignMaxRot();
  letter = 'A' + mParams.scaledAliLoadBuf;
  m_strRefMapBuf = letter;
  m_statTemplateTitle.SetFont(boldFont);
  m_statResetISTitle.SetFont(boldFont);
  m_statScaledTitle.SetFont(boldFont);
  UpdateData(false);
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
  ManageBufferUseStatus(IDC_STAT_BUFFER_ROLLS, mParams.loadAndKeepBuf);
  ManageBufferUseStatus(IDC_STAT_REF_BUF_ROLLS, mParams.scaledAliLoadBuf);
  if (mForRealignOnly <= 0)
    ManageMap();
  ManageResetIS();
  return TRUE;
}

// Closing: require entry in map label
void CNavRealignDlg::OnOK()
{
  UpdateData(true);
  if (m_strMapLabel.IsEmpty() && mForRealignOnly < 0) {
    AfxMessageBox("You must enter a label for the Navigator map to align to", MB_EXCLAME);
    return;
  }
  mParams.templateLabel = m_strMapLabel;
  mParams.maxAlignShift = m_fAlignShift;
  mParams.resetISthresh = m_fResetThresh;
  mParams.leaveISatZero = m_bLeaveISzero;
  if (m_bResetIS)
    mParams.maxNumResetIS = mMaxNumResets;
  else
    mParams.maxNumResetIS = -mMaxNumResets;
  mParams.scaledAliExtraFOV = m_fExtraFOVs;
  if (mMaxPctChgd)
    mParams.scaledAliPctChg = m_fMaxPctChange;
  if (mMaxRotChgd)
    mParams.scaledAliMaxRot = m_fMaxRotation;

  *mMasterParams = mParams;
  CBaseDlg::OnOK();
}

// New label: update for it
void CNavRealignDlg::OnEnKillfocusEditMapLabel()
{
  UpdateData(true);
  ManageMap();
}

// Making a template map from image in A, including saving if needed
void CNavRealignDlg::OnMakeTemplateMap()
{
  CString str, info;
  CMapDrawItem *item;
  int index, close = 0, mapErr = 1;
  int uncroppedX, uncroppedY;
  int answer = IDNO;
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  bool notCropped = !(imBuf->GetUncroppedSize(uncroppedX, uncroppedY) && uncroppedX > 0);
  
  // If not saved yet, open a file and save it
  if (imBuf->GetSaveCopyFlag() >= 0) {
    if (mWinApp->mBufferManager->CheckAsyncSaving())
      return;
    if (notCropped && mWinApp->mStoreMRC &&
      mWinApp->mBufferManager->IsBufferSavable(imBuf, mWinApp->mStoreMRC)) {
      answer = SEMThreeChoiceBox("This image can be saved into the current open file.  "
        "Press:\n\n"
        "\"Current File\" to save it into the current file\n\n"
        "\"New File\" to save it into a new single-image file\n\n"
        "\"Cancel\" to stop making a map this way", "Current File", "New File", "Cancel",
        MB_YESNOCANCEL);
      if (answer == IDCANCEL)
        return;
    }
    if (answer == IDNO) {
      if (mWinApp->mDocWnd->DoOpenNewFile())
        return;
      close = 1;
    }

    if (mWinApp->mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC, true))
      close = -close - 1;
  }

  // Make it a map if no error
  if (close >= 0)
    mapErr = mWinApp->mNavigator->NewMap();

  // Then close file
  if (close > 0 || close < -1)
    mWinApp->mDocWnd->DoCloseFile();
  if (mapErr)
    return;

  // Get the item just added
  index = mWinApp->mNavigator->GetNumberOfItems() - 1;
  item = mWinApp->mNavigator->GetOtherNavItem(index);
  if (!item) {
    AfxMessageBox("Warning: Cannot get the map item just created for changing its label",
      MB_EXCLAME);
    return;
  }

  // Offer to change label to current template label if unused, or just let them change it
  for (;;) {
    str = item->mLabel;
    if (!m_strMapLabel.IsEmpty() && !mWinApp->mNavigator->FindItemWithString(m_strMapLabel,
      false, true)) {
      info = "If you cancel, the label will be " + str;
      str = m_strMapLabel;
    }
    if (!KGetOneString(info, "Enter new label for item if desired:", str)) {
      str = item->mLabel;
      break;

      // But avoid duplicates
    } else if (str != item->mLabel && mWinApp->mNavigator->FindItemWithString(str,
      false, true)) {
      AfxMessageBox("This entry would be a duplicate of the label for another item; "
        "try again", MB_EXCLAME);
    } else {
      break;
    }
  }

  // Set the label in the item and here.  Turn off draw to avoid misleading map box
  item->mLabel = str;
  m_strMapLabel = str;
  item->mDraw = false;
  mWinApp->mNavigator->ManageCurrentControls();
  mWinApp->mNavigator->UpdateListString(index);
  mWinApp->mNavigator->Redraw();
  UpdateData(false);
  ManageMap();
}


void CNavRealignDlg::OnEnKillfocusEditMaxShift()
{
  UpdateData(true);
}


void CNavRealignDlg::OnCheckResetIs()
{
  UpdateData(true);
  ManageResetIS();
}

// New # of iterations
void CNavRealignDlg::OnDeltaposSpinResetTimes(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 5, mMaxNumResets, m_strRepeatReset, 
    "Reset IS up to %d");
}

// New buffer
void CNavRealignDlg::OnDeltaposSpinTemplateBuf(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mParams.loadAndKeepBuf, 1, MAX_BUFFERS - 1,
    mParams.loadAndKeepBuf))
    return;
  char letter = 'A' + mParams.loadAndKeepBuf;
  m_strSelectedBuf = letter;
  UpdateData(false);
  ManageBufferUseStatus(IDC_STAT_BUFFER_ROLLS, mParams.loadAndKeepBuf);
}


void CNavRealignDlg::OnCheckLeaveIsZero()
{
  UpdateData(true);
}

void CNavRealignDlg::OnDeltaposSpinRefMapBuf(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mParams.scaledAliLoadBuf, 1, MAX_BUFFERS - 1,
    mParams.scaledAliLoadBuf))
    return;
  char letter = 'A' + mParams.scaledAliLoadBuf;
  m_strRefMapBuf = letter;
  UpdateData(false);
  ManageBufferUseStatus(IDC_STAT_REF_BUF_ROLLS, mParams.scaledAliLoadBuf);
}


void CNavRealignDlg::OnKillfocusEditExtraFovs()
{
  UpdateData(true);
}


void CNavRealignDlg::OnKillfocusEditMaxPctChange()
{
  float value = m_fMaxPctChange;
  UPDATE_DATA_TRUE;
  if (m_fMaxPctChange != value)
    mMaxPctChgd = true;
}


void CNavRealignDlg::OnKillfocusEditMaxRotation()
{
  float value = m_fMaxRotation;
  UPDATE_DATA_TRUE;
  if (m_fMaxRotation != value)
    mMaxRotChgd = true;
}

// Set enables and text in upper section
void CNavRealignDlg::ManageMap()
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  CString str = "The image in A ";
  CMapDrawItem *map;
  bool exists = true, notMap = false, isMont = false, badBin, isMap, noMeta;
  
  if (!m_strMapLabel.IsEmpty()) {
    map = mWinApp->mNavigator->FindItemWithString(m_strMapLabel, false, true);
    exists = map && map->IsMap() && !map->mMapMontage;
    notMap = map && !map->IsMap();
    isMont = map && map->mMapMontage;
  }
  ShowDlgItem(IDC_STAT_NOT_EXIST, !exists);
  SetDlgItemText(IDC_STAT_NOT_EXIST, notMap ? "The item with that label is not a map" :
    (isMont ? "The item with that label is a montage map" :
      "No item exists with that label"));
  exists = true;
  if (imBuf->mImage) {
    badBin = imBuf->mOverviewBin > 1;
    isMap = imBuf->mMapID != 0;
    noMeta = !imBuf->mImage->GetUserData();
    exists = !(isMap || noMeta || badBin);
  }
  ShowDlgItem(IDC_STAT_WHY_A_NO_GOOD, !exists);
  if (!exists) {
    if (isMap)
      str += "is already a map";
    else if (noMeta)
      str += "has insufficient metadata";
    else
      str += "is based on a binned montage overview";
    SetDlgItemText(IDC_STAT_WHY_A_NO_GOOD, str);
  }
  m_butMakeMap.EnableWindow(exists && imBuf->mImage);
}

// Set enables in reset IS section
void CNavRealignDlg::ManageResetIS()
{
  m_editResetIS.EnableWindow(m_bResetIS);
  m_statRepeatReset.EnableWindow(m_bResetIS);
  m_statResetTimes.EnableWindow(m_bResetIS);
  m_sbcResetTimes.EnableWindow(m_bResetIS);
  m_butLeaveISzero.EnableWindow(m_bResetIS);
}

void CNavRealignDlg::ManageBufferUseStatus(int nID, int bufNum)
{
  BOOL show = true;
  if (bufNum <= mWinApp->mBufferManager->GetShiftsOnAcquire())
    SetDlgItemText(nID, "Buffer is currently rolling");
  else if (bufNum <= mWinApp->mBufferManager->GetShiftsOnAcquire() +
    (mWinApp->LowDoseMode() ? 2 : 1))
    SetDlgItemText(nID, "Buffer is currently an autoalign buffer");
  else if (bufNum == mWinApp->mBufferManager->GetBufToReadInto())
    SetDlgItemText(nID, "Buffer is currently the Read buffer");
  else
    show = false;
  ShowDlgItem(nID, show);
}
