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
{
  mForRealignOnly = false;
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
END_MESSAGE_MAP()


// CNavRealignDlg message handlers
BOOL CNavRealignDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  BOOL states[3] = {true, true, true};
  m_sbcResetTimes.SetRange(0, 10000);
  m_sbcTemplateBuf.SetRange(0, 10000);
  m_sbcResetTimes.SetPos(5000);
  m_sbcTemplateBuf.SetPos(5000);
  mMasterParams = mWinApp->mNavHelper->GetNavAlignParams();
  mParams = *mMasterParams;
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  if (mForRealignOnly)
    states[0] = false;

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
  UpdateData(false);
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
  if (!mForRealignOnly)
    ManageMap();
  ManageResetIS();
  return TRUE;
}

// Closing: require entry in map label
void CNavRealignDlg::OnOK()
{
  UpdateData(true);
  if (m_strMapLabel.IsEmpty() && !mForRealignOnly) {
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
  EMimageBuffer *imBuf = mWinApp->GetImBufs();

  // If not saved yet, open a file and save it
  if (imBuf->GetSaveCopyFlag() >= 0) {
    if (mWinApp->mBufferManager->CheckAsyncSaving())
      return;
    if (mWinApp->mDocWnd->DoOpenNewFile())
      return;
    close = 1;

    if (mWinApp->mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC, true))
      close = -1;
  }

  // Make it a map if no error
  if (close >= 0)
    mapErr = mWinApp->mNavigator->NewMap();

  // Then close file
  if (close)
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
  ManageMap();
}


void CNavRealignDlg::OnCheckLeaveIsZero()
{
  UpdateData(true);
}

// Set enables and text in upper section
void CNavRealignDlg::ManageMap()
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  int uncroppedX, uncroppedY;
  CString str = "The image in A ";
  CMapDrawItem *map;
  bool exists = true, notMap = false, isMont = false, badBin, isMap, noMeta;
  bool notCropped;
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
  ShowDlgItem(IDC_STAT_BUFFER_ROLLS, mParams.loadAndKeepBuf <
    mWinApp->mBufferManager->GetShiftsOnAcquire());
  exists = true;
  if (imBuf->mImage) {
    badBin = imBuf->mOverviewBin > 1;
    isMap = imBuf->mMapID != 0;
    noMeta = !imBuf->mImage->GetUserData();
    notCropped = !(imBuf->GetUncroppedSize(uncroppedX, uncroppedY) && uncroppedX > 0);
    exists = !(isMap || notCropped || noMeta || badBin);
  }
  ShowDlgItem(IDC_STAT_WHY_A_NO_GOOD, !exists);
  if (!exists) {
    if (isMap)
      str += "is already a map";
    else if (noMeta)
      str += "has insufficient metadata";
    else if (notCropped)
      str += "is not cropped";
    else
      str += "was cropped from a binned montage overview";
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
