// MontageSetupDlg.cpp:  To set up montaging frame number, size, overlap, binning
//                          and mag
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
#include ".\MontageSetupDlg.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "NavigatorDlg.h"
#include "ComplexTasks.h"
#include "NavHelper.h"
#include "MultiGridDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static int sIdTable[] = {IDC_STATCAMERA, IDC_RCAMERA1, IDC_RCAMERA2, IDC_RCAMERA3, 
  IDC_RCAMERA4, IDC_RCAMERA5, IDC_RCAMERA6, PANEL_END,
  IDC_STATNAVFIT1, IDC_STATNAVFIT2, IDC_STATNAVFIT3, PANEL_END,
  IDC_SPINMAG, IDC_STATMAGLABEL, IDC_STATMAG,  IDC_STATBINLABEL, IDC_STATBIN,
  IDC_SPINBIN, IDC_STATPIXEL, IDC_STATNPIECE, IDC_EDITXNFRAMES, IDC_SPINXFRAMES,
  IDC_STATICY1, IDC_EDITYNFRAMES, IDC_SPINYFRAMES, IDC_STATPIECESIZE, IDC_EDITXSIZE,
  IDC_STATICY2, IDC_EDITYSIZE, IDC_STATOVERLAP, IDC_EDITXOVERLAP, IDC_STATICY3,
  IDC_EDITYOVERLAP, IDC_STATTOTALLABEL, IDC_STATTOTALPIXELS, IDC_STATTOTALAREA, 
  IDC_UPDATE, IDC_MOVESTAGE, IDC_IGNORESKIPS, IDC_MINOVLABEL, IDC_MINOVVALUE, 
  IDC_SPINMINOV, IDC_EDITMINOVERLAP, IDC_STATMINAND, IDC_STATMINMICRON, IDC_STAT_LINE1,
  IDC_CHECK_SKIP_OUTSIDE, IDC_EDIT_SKIP_OUTSIDE, IDC_CHECKOFFERMAP, IDC_STAT_LINE2, 
  IDC_CHECK_USE_HQ_SETTINGS, IDC_BUT_RESET_OVERLAPS, IDC_CHECK_USE_MONT_MAP_PARAMS,
  IDC_CHECK_NO_DRIFT_CORR, IDC_CHECK_CONTINUOUS_MODE, IDC_EDIT_CONTIN_DELAY_FAC, 
  IDC_STAT_LD_PARAM_SET, IDC_RLD_USE_SEARCH, IDC_RLD_USE_VIEW, IDC_RLD_USE_RECORD,
  IDC_RLD_USE_PREVIEW, IDC_RLD_USE_MONTMAP, IDC_CHECK_USE_MULTISHOT,
  IDC_CHECK_CLOSE_WHEN_DONE, IDC_CHECK_IMSHIFT_IN_BLOCKS, IDC_EDIT_MAX_BLOCK_IS,
  IDC_STAT_IS_BLOCKPIECES, IDC_STAT_IS_BLOCK_STARS, IDC_STAT_LINE3,
  PANEL_END,
  IDC_STAT_HQSTAGEBOX, IDC_CHECK_FOCUS_EACH,IDC_CHECK_FOCUS_BLOCKS, 
  IDC_CHECK_SKIPCORR, IDC_CHECK_SKIP_REBLANK, IDC_STAT_BLOCKSIZE, IDC_STATBLOCKPIECES, 
  IDC_SPIN_BLOCK_SIZE, IDC_STAT_DELAY, 
  IDC_STATMAXALIGN, IDC_EDITMAXALIGN, IDC_EDITDELAY, IDC_STATDELAYSEC, 
  IDC_STATISMICRONS, IDC_CHECK_IS_REALIGN, IDC_EDIT_IS_REALIGN, IDC_CHECK_USE_ANCHOR, 
  IDC_STAT_ANCHOR_MAG, IDC_SPIN_ANCHOR_MAG, IDC_CHECK_REPEAT_FOCUS, IDC_EDIT_NMPERSEC,
  IDC_STATNMPERSEC, PANEL_END,
  IDOK, IDCANCEL, IDC_BUTHELP, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

/////////////////////////////////////////////////////////////////////////////
// CMontageSetupDlg dialog


CMontageSetupDlg::CMontageSetupDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CMontageSetupDlg::IDD, pParent)
  , m_strMinOvValue(_T(""))
  , m_bSkipOutside(FALSE)
  , m_iInsideNavItem(0)
  , m_bMakeMap(FALSE)
  , m_iMaxAlign(5)
  , m_bCloseWhenDone(FALSE)
  , m_bISrealign(FALSE)
  , m_fISlimit(0)
  , m_bUseAnchor(FALSE)
  , m_strAnchorMag(_T(""))
  , m_bRepeatFocus(FALSE)
  , m_fNmPerSec(0.1f)
  , m_bUseViewInLowDose(FALSE)
  , m_bSkipReblank(FALSE)
  , m_bNoDriftCorr(FALSE)
  , m_bUseContinMode(FALSE)
  , m_fContinDelayFac(0)
  , m_bUseSearchInLD(FALSE)
  , m_bUseMontMapParams(FALSE)
  , m_bUseMultishot(FALSE)
  , m_bImShiftInBlocks(FALSE)
  , m_fMaxBlockIS(1.f)
  , m_iLDParameterSet(0)
{
  //{{AFX_DATA_INIT(CMontageSetupDlg)
  m_iXnFrames = 1;
  m_iYnFrames = 1;
  m_iXsize = 1;
  m_iYsize = 1;
  m_strTotalArea = _T("");
  m_strPixelSize = _T("");
  m_strMag = _T("");
  m_strBinning = _T("");
  m_iXoverlap = 1;
  m_iYoverlap = 1;
  m_strTotalPixels = _T("");
  m_iCamera = -1;
	m_bMoveStage = FALSE;
	m_bIgnoreSkips = FALSE;
  //m_iRealignInterval = 0;
  //m_bRealign = FALSE;
  m_bFocusAll = FALSE;
  m_bFocusBlocks = FALSE;
  m_fDelay = 0.f;
  m_bSkipCorr = FALSE;
  m_bSkipReblank = FALSE;
  m_bUseHq = FALSE;
  m_fMinMicron = 0.f;
	//}}AFX_DATA_INIT
  mForceStage = false;
  mNoneFeasible = false;
  mLastRecordMoveStage = -1;
  mForMultiGridMap = 0;
}


void CMontageSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CMontageSetupDlg)
  DDX_Control(pDX, IDC_IGNORESKIPS, m_butIgnoreSkips);
  DDX_Control(pDX, IDC_STATCAMERA, m_butCameraBox);
  DDX_Control(pDX, IDC_UPDATE, m_butUpdate);
  DDX_Control(pDX, IDC_EDITXOVERLAP, m_editXoverlap);
  DDX_Control(pDX, IDC_EDITYOVERLAP, m_editYoverlap);
  DDX_Control(pDX, IDC_STATICY3, m_statY3);
  DDX_Control(pDX, IDC_EDITYSIZE, m_editYsize);
  DDX_Control(pDX, IDC_EDITXSIZE, m_editXsize);
  DDX_Control(pDX, IDC_EDITYNFRAMES, m_editYnFrames);
  DDX_Control(pDX, IDC_EDITXNFRAMES, m_editXnFrames);
  DDX_Control(pDX, IDC_STATOVERLAP, m_statOverlap);
  DDX_Control(pDX, IDC_STATPIECESIZE, m_statPieceSize);
  DDX_Control(pDX, IDC_STATNPIECE, m_statNPiece);
  DDX_Control(pDX, IDC_STATICY2, m_statY2);
  DDX_Control(pDX, IDC_STATICY1, m_statY1);
  DDX_Control(pDX, IDC_SPINMAG, m_sbcMag);
  DDX_Control(pDX, IDC_SPINBIN, m_sbcBinning);
  DDX_Control(pDX, IDC_SPINYFRAMES, m_sbcYnFrames);
  DDX_Control(pDX, IDC_SPINXFRAMES, m_sbcXnFrames);
  DDX_INT(pDX, IDC_EDITXNFRAMES, m_iXnFrames, "Number of pieces in X");
  DDX_INT(pDX, IDC_EDITYNFRAMES, m_iYnFrames, "Number of pieces in Y");
  DDX_INT(pDX, IDC_EDITXSIZE, m_iXsize, "Piece size in X");
  DDX_INT(pDX, IDC_EDITYSIZE, m_iYsize, "Piece size in Y");
  DDX_Text(pDX, IDC_STATTOTALAREA, m_strTotalArea);
  DDX_Text(pDX, IDC_STATPIXEL, m_strPixelSize);
  DDX_Text(pDX, IDC_STATMAG, m_strMag);
  DDX_Text(pDX, IDC_STATBIN, m_strBinning);
  DDX_MM_INT(pDX, IDC_EDITXOVERLAP, m_iXoverlap, 0, 30000, "Overlap in X");
  DDX_MM_INT(pDX, IDC_EDITYOVERLAP, m_iYoverlap, 0, 30000, "Overlap in Y");
  DDX_Text(pDX, IDC_STATTOTALPIXELS, m_strTotalPixels);
  DDX_Radio(pDX, IDC_RCAMERA1, m_iCamera);
  DDX_Check(pDX, IDC_MOVESTAGE, m_bMoveStage);
  DDX_Check(pDX, IDC_CHECK_SKIP_OUTSIDE, m_bSkipOutside);
  DDX_Check(pDX, IDC_IGNORESKIPS, m_bIgnoreSkips);
  DDX_Check(pDX, IDC_CHECKOFFERMAP, m_bMakeMap);
  DDX_Check(pDX, IDC_CHECK_CLOSE_WHEN_DONE, m_bCloseWhenDone);
  DDX_Control(pDX, IDC_MINOVLABEL, m_statMinOvLabel);
  DDX_Text(pDX, IDC_MINOVVALUE, m_strMinOvValue);
  DDX_Control(pDX, IDC_MINOVVALUE, m_statMinOvValue);
  DDX_Control(pDX, IDC_SPINMINOV, m_sbcMinOv);
  DDX_Control(pDX, IDC_EDIT_SKIP_OUTSIDE, m_editSkipOutside);
  DDX_Control(pDX, IDC_CHECKOFFERMAP, m_butOfferMap);
  DDX_MM_INT(pDX, IDC_EDIT_SKIP_OUTSIDE, m_iInsideNavItem, 0, 4096,
    "Skip pieces outside Navigator item");
  DDX_Control(pDX, IDC_CHECK_USE_HQ_SETTINGS, m_butUseHq);
  DDX_Check(pDX, IDC_CHECK_USE_HQ_SETTINGS, m_bUseHq);
  DDX_Check(pDX, IDC_CHECK_FOCUS_EACH, m_bFocusAll);
  DDX_Check(pDX, IDC_CHECK_FOCUS_BLOCKS, m_bFocusBlocks);
  DDX_Check(pDX, IDC_CHECK_SKIPCORR, m_bSkipCorr);
  //DDX_Control(pDX, IDC_CHECK_REALIGN, m_butRealign);
  //DDX_Check(pDX, IDC_CHECK_REALIGN, m_bRealign);
  DDX_Control(pDX, IDC_STAT_BLOCKSIZE, m_statBlockSize);
  DDX_Text(pDX, IDC_STAT_BLOCKSIZE, m_strBlockSize);
  DDX_Control(pDX, IDC_STATBLOCKPIECES, m_statBlockPieces);
  DDX_Control(pDX, IDC_SPIN_BLOCK_SIZE, m_sbcBlockSize);
  //DDX_Control(pDX, IDC_EDITREALIGN, m_editRealign);
  //DDX_Control(pDX, IDC_STATREALIGNPIECES, m_statRealignPieces);
  //DDX_Text(pDX, IDC_EDITREALIGN, m_iRealignInterval);
  //DDV_MinMaxInt(pDX, m_iRealignInterval, MIN_Y_MONT_REALIGN, 1000);
  DDX_Control(pDX, IDC_EDITDELAY, m_editDelay);
  DDX_MM_FLOAT(pDX, IDC_EDITDELAY, m_fDelay, 0.f, 100.f,
    "Delay time after moving stage");
  DDX_Control(pDX, IDC_STATMINAND, m_statMinAnd);
  DDX_Control(pDX, IDC_STATMINMICRON, m_statMinMicron);
  DDX_Control(pDX, IDC_EDITMINOVERLAP, m_editMinMicron);
  DDX_MM_FLOAT(pDX, IDC_EDITMINOVERLAP, m_fMinMicron, 0.f, 10.f,
    "Minimum overlap in microns");

  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_BUT_RESET_OVERLAPS, m_butResetOverlaps);
  DDX_Control(pDX, IDC_CHECK_FOCUS_EACH, m_butFocusEach);
  DDX_Control(pDX, IDC_CHECK_FOCUS_BLOCKS, m_butBlockFocus);
  DDX_Control(pDX, IDC_STATMAXALIGN, m_statMaxAlign);
  DDX_Control(pDX, IDC_EDITMAXALIGN, m_editMaxAlign);
  DDX_MM_INT(pDX, IDC_EDITMAXALIGN, m_iMaxAlign, 5, 100,
    "Maximum alignment shift as % of piece");
  DDX_Control(pDX, IDC_CHECK_IS_REALIGN, m_butISrealign);
  DDX_Control(pDX, IDC_STATISMICRONS, m_statISmicrons);
  DDX_Control(pDX, IDC_EDIT_IS_REALIGN, m_editISlimit);
  DDX_Check(pDX, IDC_CHECK_IS_REALIGN, m_bISrealign);
  DDX_MM_FLOAT(pDX, IDC_EDIT_IS_REALIGN, m_fISlimit, 0.f, 10.f,
    "Maximum image shift for realigning");
  DDX_Control(pDX, IDC_CHECK_USE_ANCHOR, m_butUseAnchor);
  DDX_Check(pDX, IDC_CHECK_USE_ANCHOR, m_bUseAnchor);
  DDX_Control(pDX, IDC_STAT_ANCHOR_MAG, m_statAnchorMag);
  DDX_Text(pDX, IDC_STAT_ANCHOR_MAG, m_strAnchorMag);
  DDX_Control(pDX, IDC_SPIN_ANCHOR_MAG, m_sbcAnchorMag);
  DDX_Control(pDX, IDC_CHECK_REPEAT_FOCUS, m_butRepeatFocus);
  DDX_Check(pDX, IDC_CHECK_REPEAT_FOCUS, m_bRepeatFocus);
  DDX_Control(pDX, IDC_EDIT_NMPERSEC, m_editNmPerSec);
  DDX_MM_FLOAT(pDX, IDC_EDIT_NMPERSEC, m_fNmPerSec, 0.1f, 30.f,
    "Drift limit for repeating focus");
  DDX_Control(pDX, IDC_STATNMPERSEC, m_statNmPerSec);
  DDX_Check(pDX, IDC_CHECK_SKIP_REBLANK, m_bSkipReblank);
  DDX_Control(pDX, IDC_CHECK_NO_DRIFT_CORR, m_butNoDriftCorr);
  DDX_Check(pDX, IDC_CHECK_NO_DRIFT_CORR, m_bNoDriftCorr);
  DDX_Control(pDX, IDC_CHECK_CONTINUOUS_MODE, m_butUseContinMode);
  DDX_Check(pDX, IDC_CHECK_CONTINUOUS_MODE, m_bUseContinMode);
  DDX_Control(pDX, IDC_EDIT_CONTIN_DELAY_FAC, m_editContinDelayFac);
  DDX_MM_FLOAT(pDX, IDC_EDIT_CONTIN_DELAY_FAC, m_fContinDelayFac, 0., 9.,
    "Settling factor for continuous mode");
  DDX_Control(pDX, IDC_CHECK_USE_MONT_MAP_PARAMS, m_butUseMontMapParams);
  DDX_Check(pDX, IDC_CHECK_USE_MONT_MAP_PARAMS, m_bUseMontMapParams);
  DDX_Control(pDX, IDC_CHECK_USE_MULTISHOT, m_butUseMultishot);
  DDX_Check(pDX, IDC_CHECK_USE_MULTISHOT, m_bUseMultishot);
  DDX_Control(pDX, IDC_CHECK_IMSHIFT_IN_BLOCKS, m_butImShiftInBlocks);
  DDX_Check(pDX, IDC_CHECK_IMSHIFT_IN_BLOCKS, m_bImShiftInBlocks);
  DDX_Control(pDX, IDC_EDIT_MAX_BLOCK_IS, m_editMaxBlockIS);
  DDX_MM_FLOAT(pDX, IDC_EDIT_MAX_BLOCK_IS, m_fMaxBlockIS, 1.0f, 1000.0f,
    "Maximum image shift within blocks");
  DDX_Radio(pDX, IDC_RLD_USE_SEARCH, m_iLDParameterSet);
}


BEGIN_MESSAGE_MAP(CMontageSetupDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CMontageSetupDlg)
  ON_EN_KILLFOCUS(IDC_EDITXNFRAMES, OnKillfocusEdits)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINBIN, OnDeltaposSpinbin)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINMAG, OnDeltaposSpinmag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINXFRAMES, OnDeltaposSpinxframes)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINYFRAMES, OnDeltaposSpinyframes)
  ON_BN_CLICKED(IDC_UPDATE, OnUpdate)
  ON_BN_CLICKED(IDC_RCAMERA1, OnRcamera)
  ON_EN_KILLFOCUS(IDC_EDITXOVERLAP, OnKillfocusEdits)
  ON_EN_KILLFOCUS(IDC_EDITXSIZE, OnKillfocusEdits)
  ON_EN_KILLFOCUS(IDC_EDITYNFRAMES, OnKillfocusEdits)
  ON_EN_KILLFOCUS(IDC_EDITYOVERLAP, OnKillfocusEdits)
  ON_EN_KILLFOCUS(IDC_EDITYSIZE, OnKillfocusEdits)
  ON_BN_CLICKED(IDC_RCAMERA2, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA3, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA4, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA5, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA6, OnRcamera)
	ON_BN_CLICKED(IDC_MOVESTAGE, OnMovestage)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINMINOV, OnDeltaposSpinminov)
  ON_BN_CLICKED(IDC_CHECK_SKIP_OUTSIDE, OnCheckSkipOutside)
  ON_BN_CLICKED(IDC_CHECKOFFERMAP, OnCheckOfferMap)
  ON_BN_CLICKED(IDC_CHECK_USE_HQ_SETTINGS, OnCheckUseHqSettings)
  ON_BN_CLICKED(IDC_CHECK_FOCUS_EACH, OnCheckFocusEach)
  ON_BN_CLICKED(IDC_CHECK_FOCUS_BLOCKS, OnCheckFocusBlocks)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_BLOCK_SIZE, OnDeltaposSpinBlockSize)

	//}}AFX_MSG_MAP
  //ON_BN_CLICKED(IDC_CHECK_REALIGN, OnCheckRealign)
  ON_BN_CLICKED(IDC_BUT_RESET_OVERLAPS, OnButResetOverlaps)
  ON_EN_KILLFOCUS(IDC_EDITMINOVERLAP, OnEnKillfocusEditMinOverlap)
  ON_BN_CLICKED(IDC_CHECK_IS_REALIGN, OnCheckIsRealign)
  ON_BN_CLICKED(IDC_CHECK_USE_ANCHOR, OnCheckUseAnchor)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ANCHOR_MAG, OnDeltaposSpinAnchorMag)
  ON_BN_CLICKED(IDC_CHECK_REPEAT_FOCUS, OnCheckRepeatFocus)
  ON_BN_CLICKED(IDC_CHECK_CONTINUOUS_MODE, OnCheckContinuousMode)
  ON_BN_CLICKED(IDC_CHECK_USE_MONT_MAP_PARAMS, OnUseMontMapParams)
  ON_BN_CLICKED(IDC_CHECK_USE_MULTISHOT, OnUseMultishot)
  ON_BN_CLICKED(IDC_CHECK_IMSHIFT_IN_BLOCKS, OnCheckImshiftInBlocks)
  ON_EN_KILLFOCUS(IDC_EDIT_MAX_BLOCK_IS, OnKillfocusEditMaxBlockIs)
  ON_BN_CLICKED(IDC_RLD_USE_SEARCH, OnRLDUseSearch)
  ON_BN_CLICKED(IDC_RLD_USE_PREVIEW, OnRLDUseSearch)
  ON_BN_CLICKED(IDC_RLD_USE_VIEW, OnRLDUseSearch)
  ON_BN_CLICKED(IDC_RLD_USE_RECORD, OnRLDUseSearch)
  ON_BN_CLICKED(IDC_RLD_USE_MONTMAP, OnRLDUseSearch)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMontageSetupDlg message handlers

BOOL CMontageSetupDlg::OnInitDialog()
{
  CButton *radio;
  int ind, i, delta, camPosInds[MAX_DLG_CAMERAS];
  CRect butrect;
  bool needFont, viewOK, recordOK, searchOK;
  BOOL tmpEnable;
  int smallSets[5] = {IDC_RLD_USE_SEARCH, IDC_RLD_USE_VIEW, IDC_RLD_USE_RECORD,
    IDC_RLD_USE_PREVIEW, IDC_RLD_USE_MONTMAP};
  BOOL forMultiLMmap = mForMultiGridMap == SETUPMONT_MG_FULL_GRID ||
    mForMultiGridMap == SETUPMONT_MG_LM_NBYN;
  BOOL overlapFixed = forMultiLMmap && mWinApp->mNavHelper->mMultiGridDlg &&
    mWinApp->mNavHelper->mMultiGridDlg->m_bUseMontOverlap;
  CFont *littleFont = mWinApp->GetLittleFont(GetDlgItem(IDC_STATICY1));
  CString *modeName = mWinApp->GetModeNames();
  CString str;
  mLowDoseMode = mForMultiGridMap ? mParam.setupInLowDose : mWinApp->LowDoseMode();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mActiveCameraList = mWinApp->GetActiveCameraList();
  mLdp = mWinApp->GetLowDoseParams();
  mNumCameras = mWinApp->GetNumActiveCameras();
  m_iCamera = mWinApp->mMontageController->GetMontageActiveCamera(&mParam);
  mCurrentCamera = mActiveCameraList[m_iCamera];
  mFittingItem = mWinApp->mNavigator && mWinApp->mNavigator->FittingMontage();
  mSizeScaling = BinDivisorI(&mCamParams[mCurrentCamera]);
  mCamSizeX = mCamParams[mCurrentCamera].sizeX;
  mCamSizeY = mCamParams[mCurrentCamera].sizeY;
  mMaxOverFrac = mWinApp->mDocWnd->GetMaxOverlapFraction();
  mMinOverlap = mWinApp->mDocWnd->GetMinMontageOverlap();
  mMaxPieces = mWinApp->mDocWnd->GetMaxMontagePieces();
  CBaseDlg::OnInitDialog();
  if (mParam.useViewInLowDose) {
    mParam.useSearchInLowDose = false;
    mParam.useMultiShot = false;
  } else if (mParam.useSearchInLowDose) {
    mParam.useMultiShot = false;
  }
  if (mForMultiGridMap) {
    EnableDlgItem(IDC_CHECK_SKIP_OUTSIDE, false);
    EnableDlgItem(IDC_MOVESTAGE, !forMultiLMmap);
    EnableDlgItem(IDC_CHECK_CLOSE_WHEN_DONE, false);
    EnableDlgItem(IDC_CHECKOFFERMAP, false);
    EnableDlgItem(IDC_CHECK_SKIPCORR, false);
  }

  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  if (mNumCameras > 4) {
    ind = 0;
    while (sIdTable[ind] != TABLE_END) {
      i = sIdTable[ind] - IDC_RCAMERA1;
      if (i >= 0 && i < MAX_DLG_CAMERAS)
        camPosInds[i] = ind;
      ind++;
    }
    delta = (sLeftTable[camPosInds[MAX_DLG_CAMERAS - 1]] - sLeftTable[camPosInds[1]]) /
      (MAX_DLG_CAMERAS / 2 - 1);
    radio = (CButton *)GetDlgItem(IDC_RCAMERA1 + MAX_DLG_CAMERAS - 1);
    radio->GetClientRect(&butrect);
    needFont = UtilCamRadiosNeedSmallFont(radio);
    for (ind = 0; ind < mNumCameras; ind++) {
      sLeftTable[camPosInds[ind]] = sLeftTable[camPosInds[0]] + (ind / 2) * delta;
      radio = (CButton *)GetDlgItem(IDC_RCAMERA1 + ind);
      radio->SetWindowPos(NULL, 0, 0, butrect.Width(), butrect.Height(),
        SWP_NOMOVE | SWP_NOZORDER);
      if (needFont)
        radio->SetFont(littleFont);
    }
  }


  for (ind = 0; ind < 5; ind++) {
    if (mLowDoseMode) {
      radio = (CButton *)GetDlgItem(smallSets[ind]);
      if (radio)
        radio->SetFont(littleFont);
    } else {
      mIDsToDrop.push_back(smallSets[ind]);
    }
  }
  mIDsToDrop.push_back(mLowDoseMode ? IDC_CHECK_USE_MONT_MAP_PARAMS : 
    IDC_STAT_LD_PARAM_SET);

  // Set up the camera buttons
  if (mNumCameras > 1) {
    for (i = 0; i < mNumCameras; i++) {
      SetDlgItemText(i + IDC_RCAMERA1, mCamParams[mActiveCameraList[i]].name);
      mCamFeasible[i] = true;
    }
  }

  // If size locked by existing open file, disable size controls and see if it is
  // oK to change the setup between low dose and regular
  mMismatchedModes = (mLowDoseMode ? 1 : 0) != (mParam.setupInLowDose ? 1 : 0);
  if (mSizeLocked || (mForMultiGridMap && mForMultiGridMap != SETUPMONT_MG_MMM_NBYN)) {

    // Setup just can't be changed if View is to be used, but otherwise it is always
    // OK going from LD to regular because the mag index just carries over, but if the
    // mag changes going from regulr to LD, treat it like mag change requiring one confirm
    if (mMismatchedModes && !mParam.useViewInLowDose &&
      !(mParam.useSearchInLowDose && mLdp[SEARCH_AREA].magIndex)) {
      if (mLowDoseMode && mParam.magIndex != mLdp[RECORD_CONSET].magIndex &&
        !mParam.warnedMagChange) {
        if (AfxMessageBox("These parameters have been used at one magnification in "
          "regular mode\n\nDo you really want to use them at a different magnification"
          " in Low Dose mode?", MB_QUESTION) == IDYES) {
          mParam.warnedMagChange = true;
          mParam.setupInLowDose = mLowDoseMode;
        }
      } else
        mParam.setupInLowDose = mLowDoseMode;
    }
    m_statNPiece.EnableWindow(false);
    m_statY1.EnableWindow(false);
    m_editXnFrames.EnableWindow(false);
    m_sbcXnFrames.EnableWindow(false);
    m_editYnFrames.EnableWindow(false);
    m_sbcYnFrames.EnableWindow(false);
  } else {

    // If not locked, and Low dose has changed from before, reformat for current set
    if (mMismatchedModes) {
      mParam.magIndex = mWinApp->mScope->GetMagIndex();
      if (mLowDoseMode)
        mParam.magIndex = mLdp[MontageLDAreaIndex(&mParam)].magIndex;
      mWinApp->mDocWnd->MontParamInitFromConSet(&mParam, MontageConSetNum(&mParam, false));
      mParam.setupInLowDose = mLowDoseMode;
    }
  }
  mMismatchedModes = (mLowDoseMode ? 1 : 0) != (mParam.setupInLowDose ? 1 : 0);
  viewOK = mLdp[VIEW_CONSET].magIndex != 0;
  recordOK = mLdp[RECORD_CONSET].magIndex != 0;
  searchOK = mLdp[SEARCH_AREA].magIndex != 0;
  ReplaceDlgItemText(IDC_CHECK_USE_VIEW_IN_LOWDOSE, "View", modeName[VIEW_CONSET]);
  ReplaceDlgItemText(IDC_CHECK_USE_MONT_MAP_PARAMS, "Record", modeName[RECORD_CONSET]);
  m_butUseMultishot.EnableWindow(mLowDoseMode && !mSizeLocked && recordOK &&
    !mFittingItem && !mForMultiGridMap);
  if (mLowDoseMode && !searchOK) {
    mParam.useSearchInLowDose = false;
    EnableDlgItem(IDC_RLD_USE_SEARCH, false);
  }
  if (mLowDoseMode && !recordOK && !mParam.useSearchInLowDose)
    mParam.useViewInLowDose = true;
  if (mLowDoseMode && !recordOK) {
    mParam.usePrevInLowDose = false;
    EnableDlgItem(IDC_RLD_USE_PREVIEW, false);
    EnableDlgItem(IDC_RLD_USE_RECORD, false);
    EnableDlgItem(IDC_RLD_USE_MONTMAP, false);
  }
  if (mLowDoseMode && !viewOK) {
    mParam.useViewInLowDose = false;
    EnableDlgItem(IDC_RLD_USE_VIEW, false);
  }
  if (mLowDoseMode && (!recordOK || mFittingItem || mParam.useViewInLowDose || 
    mParam.useSearchInLowDose || mParam.usePrevInLowDose || mForMultiGridMap))
    mParam.useMultiShot = false;
  m_butUseMontMapParams.EnableWindow(!mWinApp->GetUseRecordForMontage() && !mSizeLocked
    && !(mLowDoseMode && (mParam.useViewInLowDose || mParam.useSearchInLowDose ||
      mParam.useMultiShot || !recordOK)) && !mForMultiGridMap);
  if (mParam.useMultiShot && mLowDoseMode) {
    mParam.moveStage = false;
    EnableDlgItem(IDC_MOVESTAGE, false);
    EnableDlgItem(IDC_CHECK_SKIP_OUTSIDE, false);
  }
  tmpEnable = !mSizeLocked && !mFittingItem && !overlapFixed;
  m_editXoverlap.EnableWindow(tmpEnable);
  m_editYoverlap.EnableWindow(tmpEnable);
  m_statOverlap.EnableWindow(tmpEnable);
  m_statY3.EnableWindow(tmpEnable);
  //m_butOfferMap.EnableWindow(mWinApp->mNavigator != NULL);
  m_butResetOverlaps.EnableWindow(!mFittingItem && !overlapFixed);

  // Initialize the spin controls
  m_sbcXnFrames.SetRange(1, mMaxPieces);
  m_sbcYnFrames.SetRange(1, mMaxPieces);
  m_sbcMag.SetRange(1, MAX_MAGS);
  m_sbcBinning.SetRange(0, 100);
  m_sbcBinning.SetPos(50);
  m_sbcMinOv.SetRange(1, 200);
  m_sbcMinOv.SetPos(100);
  m_sbcBlockSize.SetRange(0,100);
  m_sbcBlockSize.SetPos(50);
  m_sbcAnchorMag.SetRange(0,200);
  m_sbcAnchorMag.SetPos(100);
  if (mParam.anchorMagInd <= 0)
    mParam.anchorMagInd = mWinApp->mComplexTasks->FindMaxMagInd(20., -2);
  m_strAnchorMag.Format("%d", MagForCamera(mCurrentCamera, mParam.anchorMagInd));

  LoadParamData(true);
  ManageSizeFields();

  if (mLowDoseMode || mForMultiGridMap) {
    if (forMultiLMmap)
      str = "FITTING TO FULL GRID AREA. ";
    else if (mForMultiGridMap == SETUPMONT_MG_POLYGON)
      str.Format("FITTING TO DUMMY %.0f UM AREA. ", mDummyAreaSize);
    else
      str = "FITTING TO NAVIGATOR AREA. ";
    if (mLowDoseMode) {
      if (!mForMultiGridMap)
        str += "Change LD area";
    } else {
      str += "Change mag";
    }
    SetDlgItemText(IDC_STATNAVFIT1, str);
    if (mForMultiGridMap && mLowDoseMode)
      str = "";
    else
      str = "to adjust number of pieces. ";
    str += "Changing ";
    if (!(mForMultiGridMap && mLowDoseMode))
      str += mLowDoseMode ? "LD area," : "mag,";
    SetDlgItemText(IDC_STATNAVFIT2, str);
    str = "binning";
    if (!overlapFixed)
      str += CString(forMultiLMmap ? " or" : ",") + " overlap";
    if (!forMultiLMmap)
      str += " or \"Move stage\"";
    str += " will refit to area";
    SetDlgItemText(IDC_STATNAVFIT3, str);
  }

  if (mSizeLocked || mLowDoseMode || mFittingItem || mMismatchedModes || 
    mForMultiGridMap) {
    ManageCameras();

    // If the current camera is not feasible but another is, switch to first feasible one
    if (!mLowDoseMode && !mFittingItem && !mCamFeasible[m_iCamera] && 
      !mNoneFeasible && !mMismatchedModes && !mForMultiGridMap) {
      for (int j = 0; j < mNumCameras; j++) {
        if (mCamFeasible[j]) {
          m_iCamera = j;
          mCurrentCamera = mActiveCameraList[m_iCamera];
          UpdateData(false);
          break;
        }
      }
    }
  }
  ManageCamAndStageEnables();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}


void CMontageSetupDlg::LoadParamData(BOOL setPos)
{

  // Set the spin button data
  bool binEnable = !(mLowDoseMode && (mSizeLocked || mParam.useMultiShot)) &&
    !mMismatchedModes;
  mLDsetNum = mParam.useViewInLowDose ? VIEW_CONSET : RECORD_CONSET;
  if (mParam.useSearchInLowDose && mLdp[SEARCH_AREA].magIndex)
    mLDsetNum = SEARCH_AREA;
  m_sbcXnFrames.SetPos(mParam.xNframes);
  m_sbcYnFrames.SetPos(mParam.yNframes);
  if (mLowDoseMode) {
    m_sbcMag.SetPos(mLdp[mLDsetNum].magIndex);
  } else if(setPos)
    m_sbcMag.SetPos(mParam.magIndex);
  m_sbcMag.EnableWindow(!mLowDoseMode && !mMismatchedModes);
  m_sbcBinning.EnableWindow(binEnable);
  EnableDlgItem(IDC_STATBIN, binEnable);
  EnableDlgItem(IDC_STATBINLABEL, binEnable);

  // Load the parameters into the text boxes
  m_iXnFrames = mParam.xNframes;
  mXoverlap = mParam.xOverlap;
  m_iXsize = mParam.xFrame;
  m_iYnFrames = mParam.yNframes;
  mYoverlap = mParam.yOverlap;
  m_iYsize = mParam.yFrame;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mParam.binning, 
    &mCamParams[mCurrentCamera]));
  m_bMoveStage = mParam.moveStage;
  m_bImShiftInBlocks = mParam.imShiftInBlocks;
  m_bSkipOutside = mParam.skipOutsidePoly;
  m_editSkipOutside.EnableWindow(m_bSkipOutside &&!(mLowDoseMode && mParam.useMultiShot));
  m_iInsideNavItem = mParam.insideNavItem + 1;
  m_bIgnoreSkips = mParam.ignoreSkipList;
  m_butIgnoreSkips.EnableWindow(mParam.numToSkip > 0 && !mForMultiGridMap);
  m_bMakeMap = mParam.makeNewMap;
  m_bCloseWhenDone = mParam.closeFileWhenDone;
  m_bUseContinMode = mParam.useContinuousMode;
  m_fContinDelayFac = mParam.continDelayFactor;
  m_bUseHq = mParam.useHqParams;
  m_bNoDriftCorr = m_bUseHq ? mParam.noHQDriftCorr : mParam.noDriftCorr;
  m_bFocusAll = mParam.focusAfterStage;
  m_bRepeatFocus = mParam.repeatFocus;
  m_fNmPerSec = mParam.driftLimit;
  m_bFocusBlocks = mParam.focusInBlocks;
  mParam.focusBlockSize = B3DMAX(2, mParam.focusBlockSize);
  //m_bRealign = mParam.realignToPiece;
  //m_iRealignInterval = mParam.realignInterval;
  m_iMaxAlign = B3DNINT(100. * mParam.maxAlignFrac);
  m_bISrealign = mParam.ISrealign;
  m_fISlimit = mParam.maxRealignIS;
  m_bUseAnchor = mParam.useAnchorWithIS;
  m_fDelay = mParam.hqDelayTime;
  m_bSkipCorr = mParam.skipCorrelations;
  m_bSkipReblank = mParam.skipRecReblanks;
  ManageStageAndGeometry(true);
  m_fMinMicron = mParam.minMicronsOverlap;
  mParam.minOverlapFactor = B3DMAX(0.01f, B3DMIN(mMaxOverFrac, mParam.minOverlapFactor));
  m_strMinOvValue.Format("%d%%", B3DNINT(100. * mParam.minOverlapFactor));
  m_bUseMontMapParams = m_bUseMontMapInLD = mParam.useMontMapParams;
  m_bUseViewInLowDose = mParam.useViewInLowDose;
  m_bUseSearchInLD = mParam.useSearchInLowDose;
  m_bUsePrevInLowDose = mParam.usePrevInLowDose;
  m_bUseMultishot = mParam.useMultiShot;
  if (mLowDoseMode) {
    if (mWinApp->GetUseRecordForMontage()) {
      m_bUseMontMapInLD = false;
      EnableDlgItem(IDC_RLD_USE_MONTMAP, false);
    }
    m_iLDParameterSet = 3;
    TurnOffOtherUseFlagsIfOn(m_bUseViewInLowDose, 1);
    TurnOffOtherUseFlagsIfOn(m_bUseSearchInLD, 0);
    TurnOffOtherUseFlagsIfOn(m_bUsePrevInLowDose, 2);
    TurnOffOtherUseFlagsIfOn(m_bUseMontMapInLD, 4);
    TurnOffOtherUseFlagsIfOn(m_bUseMultishot, -1);
  }
  LoadOverlapBoxes();
  UpdateFocusBlockSizes();
  UpdateData(false);
  ValidateEdits();
  UpdateSizes();
}

// Check whether the current size is feasible for a particular camera with this binning
BOOL CMontageSetupDlg::SizeIsFeasible(int camInd, int binning)
{
  int top, bot, left, right, xFrame, yFrame;
  int iCam = mActiveCameraList[camInd];
  int newScale = BinDivisorI(&mCamParams[iCam]);
  int binToEval = newScale * binning / mSizeScaling;
  if (!mWinApp->BinningIsValid(binToEval, iCam, false))
    return false;
  xFrame = (m_iXsize * mCamParams[iCam].sizeX / newScale) / (mCamSizeX / mSizeScaling);
  yFrame = (m_iYsize * mCamParams[iCam].sizeY / newScale) / (mCamSizeY / mSizeScaling);
  if (binToEval * xFrame > mCamParams[iCam].sizeX ||
    binToEval * yFrame > mCamParams[iCam].sizeY)
    return false;
  if (!mSizeLocked)
    return true;
  xFrame = m_iXsize;
  yFrame = m_iYsize;
  mWinApp->mCamera->CenteredSizes(xFrame, mCamParams[iCam].sizeX, mCamParams[iCam].moduloX
    , left, right, yFrame, mCamParams[iCam].sizeY, mCamParams[iCam].moduloY, top, bot,
    binToEval, iCam);
  return xFrame == m_iXsize && yFrame == m_iYsize;

}

// Simply enable cameras that size is feasible for
void CMontageSetupDlg::ManageCameras()
{
  mNoneFeasible = true;
  for (int iCam = 0; iCam < mNumCameras; iCam++) {
    CButton *radio = (CButton *)GetDlgItem(iCam + IDC_RCAMERA1);
    mCamFeasible[iCam] = SizeIsFeasible(iCam, mParam.binning);
    if (mCamFeasible[iCam])
      mNoneFeasible = false;
    radio->EnableWindow(mCamFeasible[iCam] && !mLowDoseMode && !mMismatchedModes
      && !mFittingItem && !mForMultiGridMap);
  }
}

// Make the "use" conset flags consistent but turning off all but one, set radio too
void CMontageSetupDlg::TurnOffOtherUseFlagsIfOn(BOOL &keepIfOn, int radioVal)
{
  if (!keepIfOn)
    return;
  m_bUseMontMapInLD = false;
  m_bUseViewInLowDose = false;
  m_bUseSearchInLD = false;
  m_bUsePrevInLowDose = false;
  m_bUseMultishot = false;
  keepIfOn = true;
  if (radioVal >= 0)
    m_iLDParameterSet = radioVal;
}

// Enable size if camera allows arbitrary sizes and sizes not locked
void CMontageSetupDlg::ManageSizeFields(void)
{
  bool useMulti = mLowDoseMode && m_bUseMultishot;
  BOOL enable = !mSizeLocked && mCamParams[mCurrentCamera].moduloX >= 0 && !useMulti &&
    mForMultiGridMap != SETUPMONT_MG_POLYGON;
  m_statPieceSize.EnableWindow(enable);
  m_statY2.EnableWindow(enable);
  m_editXsize.EnableWindow(enable);
  m_editYsize.EnableWindow(enable);
  SetDlgItemText(IDC_STATOVERLAP, useMulti ? "0-tilt % overlap in X:" : "Overlap in X:");
}

// Use overlap directly or convert to a percentage if doing multishot
void CMontageSetupDlg::LoadOverlapBoxes()
{
  if (mLowDoseMode && m_bUseMultishot) {
    m_iXoverlap = B3DNINT(100. * mXoverlap / m_iXsize);
    m_iYoverlap = B3DNINT(100. * mYoverlap / m_iXsize);
  } else {
    m_iXoverlap = mXoverlap;
    m_iYoverlap = mYoverlap;
  }
}

// Convert percentage back to overlap if useing multishot
void CMontageSetupDlg::UnloadOverlapBoxes()
{
  if (mLowDoseMode && m_bUseMultishot) {
    mXoverlap = B3DNINT(m_iXoverlap * m_iXsize / 100.);
    mYoverlap = B3DNINT(m_iYoverlap * m_iYsize / 100.);
  } else {
    mXoverlap = m_iXoverlap;
    mYoverlap = m_iYoverlap;
  }
}

// The focus leaves any edit field
void CMontageSetupDlg::OnKillfocusEdits() 
{
  ValidateEdits();
  ManageStageAndGeometry(false);
  UpdateSizes();
}

// Change in binning
void CMontageSetupDlg::OnDeltaposSpinbin(NMHDR* pNMHDR, LRESULT* pResult) 
{
  int newVal;
  CameraParameters *cam = &mCamParams[mCurrentCamera];
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  *pResult = 1;
  ValidateEdits();

  newVal = mWinApp->NextValidBinning(mParam.binning, pNMUpDown->iDelta, mCurrentCamera,
    false);
  if (newVal == mParam.binning)
    return;

  // If sizes are locked, make sure this is a valid binning for the given size unless
  // no cameras are feasible
  if (mSizeLocked) {
    if (!mNoneFeasible && !SizeIsFeasible(mParam.cameraIndex, newVal))
      return;

    // If the settings have already been used and the user has not been warned,
    // Make sure this is what is desired
    if (mParam.settingsUsed && !mParam.warnedBinChange) {
      if (MessageBox("This binning has already been used to acquire data.\n\n"
        "Are you sure that you want to change it?\n"
        "(If you say yes, you will be able to change it again without another warning)",
        NULL, MB_YESNO | MB_ICONQUESTION) == IDNO)
        return;
      mParam.warnedBinChange = true;
    }
    mParam.binning = newVal;
    ManageCameras();

  } else if (mFittingItem) {

    // If fitting a navigator item, consult the fitting in case it changes or forbids it
    if (CheckNavigatorFit(magIndex, newVal))
      return;
    *pResult = 0;  // COuld fall through, this avoids double update
    return;

  } else {

    // Otherwise, the size and overlap parameters should get changed
    // Round the unbinned size back up to full size if it is close
    int fullX = m_iXsize * mParam.binning;
    if (cam->sizeX - fullX < mParam.binning * B3DMAX(1, cam->moduloX))
      fullX = cam->sizeX;
    m_iXsize = fullX / newVal;

    int fullY = m_iYsize * mParam.binning;
    if (cam->sizeY - fullY < mParam.binning * B3DMAX(1, cam->moduloY))
      fullY = cam->sizeY;
    m_iYsize = fullY / newVal;

    // No good way to keep the overlap from wandering, but round it up to be even
    mXoverlap = (mParam.binning * mXoverlap) / newVal;
    mYoverlap = (mParam.binning * mYoverlap) / newVal;
    mXoverlap += mXoverlap % 2;
    mYoverlap += mYoverlap % 2;
    LoadOverlapBoxes();

    // Need to check that things still fit constraints
    UpdateData(false);
    mParam.binning = newVal;
    ValidateEdits();
    ManageCameras();
  }

  m_strBinning.Format("%s", 
    (LPCTSTR)mWinApp->BinningText(mParam.binning, cam));
  UpdateSizes();
  *pResult = 0;
}

// Change in Mag spinner
void CMontageSetupDlg::OnDeltaposSpinmag(NMHDR* pNMHDR, LRESULT* pResult) 
{
  int newVal = mParam.magIndex;
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;

  // Need to unload the text boxes because their data will get reloaded
  ValidateEdits();

  // Move in given direction until an index is reached with a listed
  // mag or until the end of the table
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, newVal, pNMUpDown->iDelta);
  if (newVal < 0)
    return;

  // If the settings have already been used and the user has not been warned,
  // Make sure this is what is desired
  if (mParam.settingsUsed && !mParam.warnedMagChange) {
    if (MessageBox("This magnification setting has already been used to acquire data.\n\n"
      "Are you sure that you want to change it?\n"
      "(If you say yes, you will be able to change it again without another warning)",
      NULL, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return;
    mParam.warnedMagChange = true;
  }

  // Revise the fit at this mag
  if (mFittingItem) {
    if (CheckNavigatorFit(newVal, mParam.binning))
      return;
  }
  
  // Output the mag and set the index
  mParam.magIndex = newVal;
  ManageStageAndGeometry(false);
  UpdateFocusBlockSizes();
  UpdateSizes();
  *pResult = 0;
}

// Change in # of X frames spinner
void CMontageSetupDlg::OnDeltaposSpinxframes(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  ValidateEdits();

  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;

  // Validate unconditionally updates the spin box position, so always return 1
  *pResult = 1;
  if (newVal < 1 || newVal > mMaxPieces || newVal * mParam.yNframes > MAX_MONT_PIECES)
    return;
  m_iXnFrames = newVal;
  UpdateData(false);
  ValidateEdits();
  UpdateSizes();
}

// Change in # of Y frames spinner
void CMontageSetupDlg::OnDeltaposSpinyframes(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  ValidateEdits();

  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  *pResult = 1;
  if (newVal < 1 || newVal > mMaxPieces || newVal * mParam.xNframes > MAX_MONT_PIECES)
    return;
  m_iYnFrames = newVal;
  UpdateData(false);
  ValidateEdits();
  ManageStageAndGeometry(false);
  UpdateSizes();
}

// Minimum overlap change
void CMontageSetupDlg::OnDeltaposSpinminov(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  float newVal = mParam.minOverlapFactor + 0.01f * (float)pNMUpDown->iDelta;
  *pResult = 1;
  if (newVal < 0.01 || newVal > mMaxOverFrac)
    return;
  if (mFittingItem) {
    if (CheckNavigatorFit(magIndex, mParam.binning, newVal))
      return;
  }
  *pResult = 0;
  mParam.minOverlapFactor = newVal;
  m_strMinOvValue.Format("%d%%", B3DNINT(100. * newVal));
  UpdateData(false);
  if (!mFittingItem) {
    ValidateEdits();
    UpdateSizes();
  }
}

// Change in minimum microns overlap
void CMontageSetupDlg::OnEnKillfocusEditMinOverlap()
{
  float overSave = m_fMinMicron;
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  UPDATE_DATA_TRUE;
  if (mFittingItem) {
    if (CheckNavigatorFit(magIndex, mParam.binning, -1.f, m_fMinMicron)) {
      m_fMinMicron = overSave;
      UpdateData(false);
      return;
    }
  } else {
    ValidateEdits();
    UpdateSizes();
  }
}

// A different camera is selected
void CMontageSetupDlg::OnRcamera() 
{
  if (mParam.settingsUsed && !mParam.warnedCamChange) {
    if (MessageBox("The previous camera has already been used to acquire data.\n\n"
      "Are you sure that you want to change it?\n"
      "(If you say yes, you will be able to change it again without another warning)",
      NULL, MB_YESNO | MB_ICONQUESTION) == IDNO) {
      UpdateData(false);
      return;
    }
    mParam.warnedCamChange = true;
  }

  // Get the new number into member variable
  // But if the current camera is not feasible, it's not enabled and MFC throws a fit
  // (because it can't disable that button and two are checked?)
  // So just switch to the first feasible camera
  if (!mCamFeasible[m_iCamera]) {
    for (int i = 0; i < mNumCameras; i++) {
      if (mCamFeasible[i]) {
        m_iCamera = i;
        UpdateData(false);
        break;
      }
    }
  } else if (!UpdateData(true))
    return;

  int newCam = mActiveCameraList[m_iCamera];
  int newScale = BinDivisorI(&mCamParams[newCam]);
  mParam.binning = newScale * (mParam.binning / mSizeScaling);
  if (!mSizeLocked) {

    // Change dimensions by change in size of camera
    m_iXsize = (m_iXsize * mCamParams[newCam].sizeX / newScale) / 
      (mCamSizeX / mSizeScaling);
    m_iYsize = (m_iYsize * mCamParams[newCam].sizeY / newScale) / 
      (mCamSizeY / mSizeScaling);
    mXoverlap = (mXoverlap * mCamParams[newCam].sizeX / newScale) / 
      (mCamSizeX / mSizeScaling);
    mYoverlap = (mYoverlap * mCamParams[newCam].sizeY / newScale) / 
      (mCamSizeY / mSizeScaling);
    mXoverlap += mXoverlap % 2;
    mYoverlap += mYoverlap % 2;
    LoadOverlapBoxes();
    UpdateData(false);
    mCurrentCamera = newCam;
    ValidateEdits();
  }

  // Finalize camera change and set sizes
  mCurrentCamera = newCam;
  SetDlgItemText(IDC_STATBINLABEL, mCamParams[newCam].STEMcamera ? "Sampling:" :
    "Binning:");
  mSizeScaling = newScale;
  mCamSizeX = mCamParams[newCam].sizeX;
  mCamSizeY = mCamParams[newCam].sizeY;
  mParam.cameraIndex = m_iCamera;
  ManageCamAndStageEnables();
  ManageSizeFields();
  UpdateSizes();
}

void CMontageSetupDlg::OnOK() 
{
  NavParams *navp = mWinApp->GetNavParams();
  CString mess;
  int xExtent, yExtent, maxPcX, maxPcY;
  double maxIS, montIS;
  int lowestM = mWinApp->mScope->GetLowestNonLMmag(&mCamParams[mCurrentCamera]);
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  ValidateEdits();
  FindMaxExtents(m_bMoveStage && m_bImShiftInBlocks, xExtent, yExtent, maxPcX, maxPcY, 
    maxIS, montIS);

  if (((!m_bMoveStage && !mFittingItem) || (m_bMoveStage && m_bImShiftInBlocks)) && 
    montIS > maxIS) {
    mess.Format("The largest image shift needed for the %s is\n"
      "%.1f microns, bigger than the maximum image shift of\n"
      "%.1f microns allowed when fitting a montage to a polygon.\n"
      " (This limit is controlled by the property %s)\n\n", 
      m_bMoveStage ? "blocks" : "montage", montIS, maxIS,
      magIndex >= lowestM ? "NavigatorMaxMontageIS" : "NavigatorMaxLMMontageIS");
    if (m_bMoveStage) {
      mess += "Do you want to proceed with this block size for image shift?";
      if (AfxMessageBox(mess, MB_QUESTION) == IDNO)
        return;
    } else {
      mess += "Do you want to use stage movement instead for this montage?";
      if (AfxMessageBox(mess, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        m_bMoveStage = true;
        UpdateData(false);
        OnMovestage();
        return;
      }
    }
  }

  if (maxPcX > 65535 || maxPcY > 65535)
    mWinApp->AppendToLog("Maximum piece coordinates are above 65535, too large to store "
      "in the image file header.\r\nPiece coordinates will be saved only in a Metadata "
      "Autodoc (.mdoc) file");

  UnloadParamData();
  CDialog::OnOK();
}

// Reload parameters into parameter set that don't live there
void CMontageSetupDlg::UnloadParamData(void)
{
  mParam.xNframes= m_iXnFrames;
  mParam.xOverlap = mXoverlap;
  mParam.xFrame = m_iXsize;
  mParam.yNframes= m_iYnFrames;
  mParam.yOverlap = mYoverlap;
  mParam.yFrame = m_iYsize;
  mParam.moveStage = m_bMoveStage;
  mParam.imShiftInBlocks = m_bImShiftInBlocks;
  mParam.ignoreSkipList = m_bIgnoreSkips;
  mParam.skipOutsidePoly = m_bSkipOutside;
  mParam.insideNavItem = m_iInsideNavItem - 1;
  mParam.makeNewMap = m_bMakeMap;
  mParam.closeFileWhenDone = m_bCloseWhenDone;
  mParam.useHqParams = m_bUseHq;
  mParam.focusAfterStage = m_bFocusAll;
  mParam.repeatFocus = m_bRepeatFocus;
  mParam.driftLimit = m_fNmPerSec;
  mParam.focusInBlocks = m_bFocusBlocks;
  //mParam.realignToPiece = m_bRealign;
  //mParam.realignInterval = m_iRealignInterval;
  mParam.hqDelayTime = m_fDelay;
  mParam.skipCorrelations = m_bSkipCorr;
  mParam.skipRecReblanks = m_bSkipReblank;
  mParam.maxAlignFrac = 0.01f * (float)m_iMaxAlign;
  mParam.minMicronsOverlap = m_fMinMicron;
  mParam.maxRealignIS = m_fISlimit;
  mParam.ISrealign = m_bISrealign;
  mParam.useAnchorWithIS = m_bUseAnchor;
  mParam.useMontMapParams = mLowDoseMode ? m_bUseMontMapInLD : m_bUseMontMapParams;
  if (mLowDoseMode) {
    mParam.useViewInLowDose = m_bUseViewInLowDose;
    mParam.useSearchInLowDose = m_bUseSearchInLD && !m_bUseViewInLowDose;
    mParam.usePrevInLowDose = m_bUsePrevInLowDose && !(m_bUseViewInLowDose ||
      m_bUseSearchInLD);
    mParam.useMultiShot = m_bUseMultishot && !(m_bUseViewInLowDose || m_bUseSearchInLD ||
      m_bUsePrevInLowDose);
  }
  mParam.useContinuousMode = m_bUseContinMode;
  mParam.continDelayFactor = m_fContinDelayFac;
  if (m_bUseHq)
    mParam.noHQDriftCorr = m_bNoDriftCorr;
  else
    mParam.noDriftCorr = m_bNoDriftCorr;
}

// Update pixel size, total pixels and total size
void CMontageSetupDlg::UpdateSizes()
{
  int iTotalX, iTotalY, magIndex;
  magIndex = (mLowDoseMode && !mForMultiGridMap) ? mLdp[mLDsetNum].magIndex : 
    mParam.magIndex;

  // Output the mag
  m_strMag.Format("%d", MagForCamera(mCurrentCamera, magIndex));

  // Get the best estimate of pixel size and output in nm
  float pixel = mParam.binning * mWinApp->mShiftManager->GetPixelSize
          (mCurrentCamera, magIndex);
  CString format = mWinApp->PixelFormat(pixel * 1000.f);
  m_strPixelSize.Format("Pixel size: " + format, pixel * 1000.);

  // Compute total extent of area and output as pixels and microns
  iTotalX = (m_iXnFrames * m_iXsize - (m_iXnFrames - 1) * mXoverlap);
  iTotalY = (m_iYnFrames * m_iYsize - (m_iYnFrames - 1) * mYoverlap);
  m_strTotalPixels.Format("%d x %d pixels", iTotalX, iTotalY);

  m_strTotalArea = FormatMicronSize(iTotalX, iTotalY, pixel) + " microns";

  UpdateFocusBlockSizes();

  UpdateData(false);
}

// Specifically update both of the block size spinner and the image shift block limit
void CMontageSetupDlg::UpdateFocusBlockSizes()
{
  NavParams *navp = mWinApp->GetNavParams();
  int numX, numY, lowestM;
  int magIndex = GetMagIndexAndLowestNonLMInd(lowestM);
  double maxIS = magIndex >= lowestM ? navp->maxMontageIS : navp->maxLMMontageIS;
  float pixelSize = (float)mParam.binning *
    mWinApp->mShiftManager->GetPixelSize(mCurrentCamera, magIndex);

  mWinApp->mMontageController->BlockSizesForNearSquare(m_iXsize, m_iYsize, mXoverlap,
    mYoverlap, mParam.focusBlockSize, numX, numY);
  if (m_bImShiftInBlocks) {
    mWinApp->mMontageController->ImageShiftBlockSizes(m_iXsize, m_iYsize, mXoverlap,
      mYoverlap, pixelSize,
      magIndex >= lowestM ? mParam.maxBlockImShiftNonLM : mParam.maxBlockImShiftLM,
      numX, numY);
  }
  m_strBlockSize.Format("%d x %d", numX, numY);
  m_fMaxBlockIS = (magIndex < lowestM) ? mParam.maxBlockImShiftLM : 
    mParam.maxBlockImShiftNonLM;
  ShowDlgItem(IDC_STAT_IS_BLOCK_STARS, m_fMaxBlockIS > maxIS);
  UpdateData(false);
}

// Find maximum iage shift extent for montage as whole or for image shifting in blocks
void CMontageSetupDlg::FindMaxExtents(bool ISinBlocks, int &xExtent, int &yExtent, 
  int &maxPcX, int &maxPcY, double &maxIS, double &montIS)
{
  NavParams *navp = mWinApp->GetNavParams();
  int numX, numY, lowestM;
  int magIndex = GetMagIndexAndLowestNonLMInd(lowestM);
  float pixelSize = (float)mParam.binning * 
    mWinApp->mShiftManager->GetPixelSize(mCurrentCamera, magIndex);
  xExtent = maxPcX = (m_iXsize - mXoverlap) * (m_iXnFrames - 1);
  yExtent = maxPcY = (m_iYsize - mYoverlap) * (m_iYnFrames - 1);
  if (ISinBlocks) {
    mWinApp->mMontageController->ImageShiftBlockSizes(m_iXsize, m_iYsize, mXoverlap,
      mYoverlap, pixelSize,
      magIndex >= lowestM ? mParam.maxBlockImShiftNonLM : mParam.maxBlockImShiftLM, 
      numX, numY);
    xExtent = (m_iXsize - mXoverlap) * (numX - 1);
    yExtent = (m_iYsize - mYoverlap) * (numY - 1);
  }
  maxIS = magIndex >= lowestM ? navp->maxMontageIS : navp->maxLMMontageIS;
  montIS = sqrt((double)xExtent * xExtent + yExtent * yExtent) * pixelSize / 2.;
}

// Get magindex given conditions
int CMontageSetupDlg::GetMagIndexAndLowestNonLMInd(int &lowestM)
{
  lowestM = mWinApp->mScope->GetLowestNonLMmag(&mCamParams[mCurrentCamera]);
  return mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
}

// Make a string with Y x Y size in microns with minimal digits
CString CMontageSetupDlg::FormatMicronSize(int xSize, int ySize, float pixel)
{
  CString format, str;
  int nDec;
  float totalX = xSize * pixel;
  float totalY = ySize * pixel;
  nDec = 0;
  if (totalX < 100. && totalY < 100.)
    nDec = 1;
  if (totalX < 10. && totalY < 10.)
    nDec = 2;
  if (totalX < 1. && totalY < 1.)
    nDec = 3;
  format.Format("%%.%df x %%.%df", nDec, nDec);
  str.Format(format, totalX, totalY);
  return str;
}

// Check the values in all of the edit boxes, adjust as needed
void CMontageSetupDlg::ValidateEdits()
{
  int left, right, tempX, tempY, top, bot, minOverX, minOverY;
  CameraParameters *cam = &mCamParams[mCurrentCamera];
  UPDATE_DATA_TRUE;
  UnloadOverlapBoxes();
  BOOL needUpdate = false;

  // Constrain maximum pieces in one direction
  if (m_iXnFrames > mMaxPieces || m_iXnFrames < 1) {
    m_iXnFrames = B3DMAX(1, B3DMIN(mMaxPieces, m_iXnFrames));
    needUpdate = true;
  }
  if (m_iYnFrames > mMaxPieces || m_iYnFrames < 1) {
    m_iYnFrames = B3DMAX(1, B3DMIN(mMaxPieces, m_iYnFrames));
    m_iYnFrames = mMaxPieces;
    needUpdate = true;
  }

  // Constrain total maximum pieces and coordinate size if file already opened
  while (m_iXnFrames * m_iYnFrames > MAX_MONT_PIECES || (mConstrainSize &&
    ((m_iXsize - mXoverlap) * (m_iXnFrames - 1) > 65535 ||
    (m_iYsize - mYoverlap) * (m_iYnFrames - 1) > 65535))) {
    needUpdate = true;
    if (m_iXnFrames > m_iYnFrames)
      m_iXnFrames--;
    else
      m_iYnFrames--;
  }

  if (!mSizeLocked) {

    // Size must be twice the min overlap and fit in camera with binning
    MinOverlapsInPixels(minOverX, minOverY);
    if (m_iXsize < 2 * minOverX) {
      m_iXsize = 2 * minOverX;
      needUpdate = true;
    }
    if (m_iXsize > cam->sizeX / mParam.binning) {
      m_iXsize = cam->sizeX / mParam.binning;
      needUpdate = true;
    }
    if (m_iYsize < minOverY) {
      m_iYsize = 2 * minOverY;
      needUpdate = true;
    }
    if (m_iYsize > cam->sizeY / mParam.binning) {
      m_iYsize = cam->sizeY / mParam.binning;
      needUpdate = true;
    }

    // Sizes must be what the camera will actually shoot
    tempX = m_iXsize;
    tempY = m_iYsize;
    mWinApp->mMontageController->LimitSizesToUsable(cam, mCurrentCamera, 
        mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex, m_iXsize, m_iYsize, 
      mParam.binning);
    mWinApp->mCamera->CenteredSizes(m_iXsize, cam->sizeX, cam->moduloX, left, right,
      m_iYsize, cam->sizeY, cam->moduloY, top, bot, mParam.binning, 
      mCurrentCamera);
    if (tempX != m_iXsize)
      needUpdate = true;
    if (tempY != m_iYsize)
      needUpdate = true;

    // Overlap must be at least the minimum and less than twice the size
    MinOverlapsInPixels(minOverX, minOverY);
    if (m_iXnFrames > 1 && mXoverlap < minOverX) {
      mXoverlap = minOverX;
      needUpdate = true;
    }
    if (mXoverlap > mMaxOverFrac * m_iXsize) {
      mXoverlap = B3DNINT(mMaxOverFrac * m_iXsize);
      needUpdate = true;
    }
    if (mXoverlap % 2) {
      mXoverlap--;
      needUpdate = true;
    }
    if (m_iYnFrames > 1 && mYoverlap < minOverY) {
      mYoverlap = minOverY;
      needUpdate = true;
    }
    if (mYoverlap > mMaxOverFrac * m_iYsize) {
      mYoverlap = B3DNINT(mMaxOverFrac * m_iYsize);
      needUpdate = true;
    }
    if (mYoverlap % 2) {
      mYoverlap--;
      needUpdate = true;
    }
  }

  if (needUpdate) {
    LoadOverlapBoxes();
    UpdateData(false);
  }

  // Update the spin box positions
  m_sbcXnFrames.SetPos(m_iXnFrames);
  m_sbcYnFrames.SetPos(m_iYnFrames);

}

// The update button
void CMontageSetupDlg::OnUpdate() 
{
  ValidateEdits();
  UpdateSizes();  
  m_butUpdate.SetButtonStyle(BS_PUSHBUTTON);
  SetFocus();
}

void CMontageSetupDlg::OnMovestage() 
{
  BOOL moveSave = m_bMoveStage;
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  if (mParam.settingsUsed && !mParam.warnedStageChange) {
    if (MessageBox("You have already acquired data with the previous mode.\n\n"
      "Are you sure that you want to change it?\n"
      "(If you say yes, you will be able to change it again without another warning)",
      NULL, MB_YESNO | MB_ICONQUESTION) == IDNO) {
      UpdateData(false);
      return;
    }
    mParam.warnedStageChange = true;
  }

  // Get the new number into member variable then check if fitting
  UPDATE_DATA_TRUE;
  if (mFittingItem) {
    mForceStage = m_bMoveStage;
    if (CheckNavigatorFit(magIndex, mParam.binning, mParam.minOverlapFactor)) {

      // If it failed with force stage set, then turn it off as well as the box itself
      if (mForceStage)
        mForceStage = false;
      m_bMoveStage = moveSave;
      UpdateData(false);
    }
  } else 
    ValidateEdits();
  UpdateSizes();
  ManageStageAndGeometry(m_bUseHq);
  ManageCamAndStageEnables();
  UpdateFocusBlockSizes();
}

void CMontageSetupDlg::OnCheckImshiftInBlocks()
{
  UPDATE_DATA_TRUE;
  ManageStageAndGeometry(false);
  UpdateFocusBlockSizes();
}

void CMontageSetupDlg::OnKillfocusEditMaxBlockIs()
{
  UPDATE_DATA_TRUE;
  int lowestM, magIndex = GetMagIndexAndLowestNonLMInd(lowestM);
  if (magIndex < lowestM)
    mParam.maxBlockImShiftLM = m_fMaxBlockIS;
  else
    mParam.maxBlockImShiftNonLM = m_fMaxBlockIS;
  UpdateFocusBlockSizes();
}

void CMontageSetupDlg::OnCheckSkipOutside()
{
  UPDATE_DATA_TRUE;
  m_editSkipOutside.EnableWindow(m_bSkipOutside);
}

void CMontageSetupDlg::OnCheckContinuousMode()
{
  UPDATE_DATA_TRUE;
  ManageCamAndStageEnables();
}

int CMontageSetupDlg::CheckNavigatorFit(int magIndex, int binning, float minOverlap, 
                                        float minMicrons, BOOL switchingVinLD)
{
  // Copy the param for trial use
  if (!mFittingItem)
    return 0;
  UnloadOverlapBoxes();
  UnloadParamData();
  MontParam tmpParam = mParam;
  if (minOverlap >= 0.)
    tmpParam.minOverlapFactor = minOverlap;
  if (minMicrons >= 0.)
    tmpParam.minMicronsOverlap = minMicrons;

  // If switching mode in low dose, re-init the param and set the mag index and binning
  if (switchingVinLD) {
    int setNum = MontageConSetNum(&tmpParam, false);
    ControlSet *conSet = mWinApp->GetConSets() + setNum;
    mWinApp->mDocWnd->MontParamInitFromConSet(&tmpParam, setNum);
    magIndex = mLdp[MontageLDAreaIndex(&tmpParam)].magIndex;
    binning = conSet->binning;
  }
  int err = mWinApp->mNavigator->FitMontageToItem(&tmpParam, binning, magIndex, 
    mForceStage, 0., mCurrentCamera, mLowDoseMode);
  if (!err) {
    mParam = tmpParam;
    if (switchingVinLD)
      mLDsetNum = MontageLDAreaIndex(&mParam);

    // Have to get the magIndex right before the load as it will validate again
    if (!mLowDoseMode)
      mParam.magIndex = magIndex;
    LoadParamData(false);
  }
  return err;
}

void CMontageSetupDlg::OnCheckOfferMap()
{
  UPDATE_DATA_TRUE;
}

void CMontageSetupDlg::OnCheckUseHqSettings()
{
  if (m_bUseHq)
    mParam.noHQDriftCorr = m_bNoDriftCorr;
  else
    mParam.noDriftCorr = m_bNoDriftCorr;
  UPDATE_DATA_TRUE;
  m_bNoDriftCorr = m_bUseHq ? mParam.noHQDriftCorr : mParam.noDriftCorr;
  UpdateData(false);
  ManageStageAndGeometry(true);
}

void CMontageSetupDlg::OnCheckFocusEach()
{
  UPDATE_DATA_TRUE;
  if (m_bFocusBlocks && m_bFocusAll) {
    m_bFocusBlocks = !m_bFocusAll;
    UpdateData(false);
  }
  ManageStageAndGeometry(false);
}

void CMontageSetupDlg::OnCheckRepeatFocus()
{
  UPDATE_DATA_TRUE;
  ManageStageAndGeometry(true);
}

void CMontageSetupDlg::OnCheckFocusBlocks()
{
  UPDATE_DATA_TRUE;
  if (m_bFocusBlocks && m_bFocusAll) {
    m_bFocusAll = !m_bFocusBlocks;
    UpdateData(false);
  }
  ManageStageAndGeometry(false);
}

/*void CMontageSetupDlg::OnCheckRealign()
{
  UPDATE_DATA_TRUE;
  ManageStageAndGeometry(false);
}*/

void CMontageSetupDlg::OnCheckIsRealign()
{
  UPDATE_DATA_TRUE;
  /*if (m_bRealign) {
    m_bRealign = false;
    UpdateData(false);
  }*/
  ManageStageAndGeometry(false);
}
void CMontageSetupDlg::OnCheckUseAnchor()
{
  UPDATE_DATA_TRUE;
  ManageStageAndGeometry(false);
}

void CMontageSetupDlg::OnDeltaposSpinAnchorMag(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal = mParam.anchorMagInd;
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  *pResult = 1;
  ValidateEdits();

  // Move in given direction until an index is reached with a listed
  // mag or until the end of the table
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, newVal, pNMUpDown->iDelta);
  if (newVal < 0)
    return;
  mParam.anchorMagInd = newVal;
  m_strAnchorMag.Format("%d", MagForCamera(mCurrentCamera, newVal));
  UpdateData(false);
  *pResult = 0;
}

void CMontageSetupDlg::OnDeltaposSpinBlockSize(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newVal = mParam.focusBlockSize + pNMUpDown->iDelta;
  if (newVal < 2 || newVal > 16) {
    *pResult = 1;
    return;
  }
  mParam.focusBlockSize = newVal;
  UpdateFocusBlockSizes();
  UpdateData(false);
  *pResult = 0;
}

// Manage all enable/disables that depend on stage settings, and reposition the items
void CMontageSetupDlg::ManageStageAndGeometry(BOOL reposition)
{
  BOOL states[5] = {0, 0, true, 0, true};
  BOOL hqOK = !mLowDoseMode || mWinApp->mMontageController->GetAllowHQMontInLD();
  BOOL bEnable = m_bMoveStage && m_bUseHq && hqOK;
  BOOL focusOK = mParam.magIndex >= 
    mWinApp->mScope->GetLowestNonLMmag(&mCamParams[mCurrentCamera]);
  BOOL tmpEnable, realignOn = false; //m_bRealign && m_iYnFrames >= MIN_Y_MONT_REALIGN;
  BOOL forMultiLMmap = mForMultiGridMap == SETUPMONT_MG_FULL_GRID ||
    mForMultiGridMap == SETUPMONT_MG_LM_NBYN;

  states[0] = mNumCameras > 1;
  states[1] = mFittingItem;
  states[3] = bEnable;
  tmpEnable = !mSizeLocked && m_bMoveStage && (!forMultiLMmap ||
    !mWinApp->mNavHelper->mMultiGridDlg || 
    !mWinApp->mNavHelper->mMultiGridDlg->m_bUseMontOverlap);
  m_statMinOvLabel.EnableWindow(tmpEnable);
  m_statMinOvValue.EnableWindow(tmpEnable);
  m_sbcMinOv.EnableWindow(tmpEnable);
  tmpEnable = !mSizeLocked && m_bMoveStage && !forMultiLMmap;
  m_statMinAnd.EnableWindow(tmpEnable);
  m_statMinMicron.EnableWindow(tmpEnable);
  m_editMinMicron.EnableWindow(tmpEnable);
  m_butImShiftInBlocks.EnableWindow(tmpEnable);
  m_editMaxBlockIS.EnableWindow(tmpEnable && m_bImShiftInBlocks);
  EnableDlgItem(IDC_STAT_IS_BLOCKPIECES, tmpEnable && m_bImShiftInBlocks);
  EnableDlgItem(IDC_STAT_IS_BLOCK_STARS, tmpEnable && m_bImShiftInBlocks);

  m_butUseHq.EnableWindow(m_bMoveStage && hqOK);
  m_butFocusEach.EnableWindow(bEnable && focusOK && 
    !(m_bFocusBlocks || m_bImShiftInBlocks));
  tmpEnable = bEnable && focusOK && (m_bFocusAll || 
    (m_bFocusBlocks && m_bImShiftInBlocks));
  m_butRepeatFocus.EnableWindow(tmpEnable);
  m_editNmPerSec.EnableWindow(tmpEnable && m_bRepeatFocus);
  m_statNmPerSec.EnableWindow(tmpEnable && m_bRepeatFocus);
  tmpEnable = bEnable && focusOK && (!m_bISrealign || m_bImShiftInBlocks);
  m_butBlockFocus.EnableWindow(tmpEnable);
  m_statBlockSize.EnableWindow(tmpEnable);
  m_sbcBlockSize.EnableWindow(tmpEnable && !m_bImShiftInBlocks && !mSizeLocked);
  m_statBlockPieces.EnableWindow(tmpEnable && m_bFocusBlocks);
  //m_butRealign.EnableWindow(bEnable && m_iYnFrames >= MIN_Y_MONT_REALIGN && 
  //  !m_bISrealign);
  //m_editRealign.EnableWindow(bEnable && realignOn);
  //m_statRealignPieces.EnableWindow(bEnable && realignOn);
  tmpEnable = bEnable && !realignOn && !m_bFocusBlocks && !m_bImShiftInBlocks;
  m_statMaxAlign.EnableWindow(bEnable && (realignOn || (tmpEnable && m_bISrealign)));
  m_editMaxAlign.EnableWindow(bEnable && (realignOn || (tmpEnable && m_bISrealign)));
  m_butISrealign.EnableWindow(tmpEnable);
  m_statISmicrons.EnableWindow(tmpEnable && m_bISrealign);
  m_editISlimit.EnableWindow(tmpEnable && m_bISrealign);
  m_butUseAnchor.EnableWindow(tmpEnable && m_bISrealign);
  m_statAnchorMag.EnableWindow(tmpEnable && m_bISrealign && m_bUseAnchor);
  m_sbcAnchorMag.EnableWindow(tmpEnable && m_bISrealign && m_bUseAnchor);

  if (!reposition)
    return;
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 
    mNumCameras, sHeightTable);
}

// Manage a few things that depend on camera and maybe stage too
void CMontageSetupDlg::ManageCamAndStageEnables(void)
{
  m_butUseContinMode.EnableWindow(mCamParams[mCurrentCamera].useContinuousMode > 0 &&
  !(mLowDoseMode && m_bUseMultishot));
  m_editContinDelayFac.EnableWindow(m_bUseContinMode && 
    mCamParams[mCurrentCamera].useContinuousMode > 0);
  m_butNoDriftCorr.EnableWindow(mCamParams[mCurrentCamera].OneViewType != 0 && 
    m_bMoveStage);
}

void CMontageSetupDlg::MinOverlapsInPixels(int & minOverX, int & minOverY)
{
  float absmin, pixel;
  int magIndex = mLowDoseMode ? mLdp[mLDsetNum].magIndex : mParam.magIndex;
  minOverX = mMinOverlap;
  minOverY = mMinOverlap;
  if (!m_bMoveStage)
    return;
  pixel = mParam.binning * mWinApp->mShiftManager->GetPixelSize(mCurrentCamera,magIndex);
  absmin = m_fMinMicron / pixel;
  minOverX = 2 * (B3DNINT(B3DMAX(mParam.minOverlapFactor * m_iXsize, absmin)) / 2);
  minOverY = 2 * (B3DNINT(B3DMAX(mParam.minOverlapFactor * m_iYsize, absmin)) / 2);
}

void CMontageSetupDlg::OnButResetOverlaps()
{
  mXoverlap = 2 * (m_iXsize / 20);
  mYoverlap = 2 * (m_iYsize / 20);
  if (m_bMoveStage)
    MinOverlapsInPixels(mXoverlap, mYoverlap);
  LoadOverlapBoxes();
  UpdateData(false);
  ValidateEdits();
  UpdateSizes();  
  m_butResetOverlaps.SetButtonStyle(BS_PUSHBUTTON);
  SetFocus();
}

// Mont-mapcheck box, not used in low dose
void CMontageSetupDlg::OnUseMontMapParams()
{
  BOOL dummy = false, dummy2 = false;
  UseViewOrSearchInLD(dummy, dummy2);
}

// Low dose area radio buttons
void CMontageSetupDlg::OnRLDUseSearch()
{
  UpdateData(true);
  if (m_bUseMultishot) {
    m_bUseMultishot = false;
    UpdateData(false);
  }
  if (m_iLDParameterSet == 1) {
    m_bUseViewInLowDose = true;
    m_bUseMontMapInLD = false;
    UseViewOrSearchInLD(m_bUseSearchInLD, m_bUsePrevInLowDose);
  } else if (!m_iLDParameterSet) {
    m_bUseSearchInLD = true;
    m_bUseMontMapInLD = false;
    UseViewOrSearchInLD(m_bUseViewInLowDose, m_bUsePrevInLowDose);
  } else if (m_iLDParameterSet == 2) {
    m_bUsePrevInLowDose = true;
    m_bUseMontMapInLD = false;
    UseViewOrSearchInLD(m_bUseViewInLowDose, m_bUseSearchInLD);
  } else if (m_iLDParameterSet == 3) {
    m_bUsePrevInLowDose = false;
    m_bUseMontMapInLD = false;
    UseViewOrSearchInLD(m_bUseViewInLowDose, m_bUseSearchInLD);
  } else {
    m_bUseMontMapInLD = true;
    m_bUsePrevInLowDose = false;
    UseViewOrSearchInLD(m_bUseViewInLowDose, m_bUseSearchInLD);
  }
}

void CMontageSetupDlg::OnUseMultishot()
{
  UpdateData(true);
  bool needUpdate = m_bUseMultishot && m_iLDParameterSet != 3;
  TurnOffOtherUseFlagsIfOn(m_bUseMultishot, 3);
  UseViewOrSearchInLD(m_bUseViewInLowDose, m_bUseSearchInLD);
  if (needUpdate) 
    UpdateData(false);
  EnableDlgItem(IDC_MOVESTAGE, !m_bUseMultishot);
  EnableDlgItem(IDC_CHECK_SKIP_OUTSIDE, !m_bUseMultishot);
}

void CMontageSetupDlg::UseViewOrSearchInLD(BOOL &otherFlag, BOOL &secondFlag)
{
  BOOL saveVinLD = m_bUseViewInLowDose;
  BOOL saveSinLD = m_bUseSearchInLD;
  BOOL saveUseMMP = m_bUseMontMapParams;
  int setNum;
  ControlSet *conSet = mWinApp->GetConSets();
  int saveMoveStage = m_bMoveStage ? 1 : 0;
  if (mLowDoseMode && !m_bUseViewInLowDose && !m_bUseSearchInLD)
    mLastRecordMoveStage = saveMoveStage;
  UnloadOverlapBoxes();

  UPDATE_DATA_TRUE;
  if (otherFlag || secondFlag) {
    otherFlag = false;
    secondFlag = false;
    UpdateData(false);
  } else if (!mLdp[RECORD_CONSET].magIndex) {
    otherFlag = true;
    secondFlag = false;
    UpdateData(false);
  }
  
  // If fitting item, the sequence is handled entirely by the check routine and the mag
  // index and binning are supplied there, but need to restore button if fit is rejected
  // Have to supply the actual binning in control set when not low dose
  if (mFittingItem) {
    setNum = B3DCHOICE((mLowDoseMode ? m_bUseMontMapInLD : m_bUseMontMapParams) && 
      !mWinApp->GetUseRecordForMontage(), MONT_USER_CONSET, RECORD_CONSET);
    if (CheckNavigatorFit(mParam.magIndex, conSet[setNum].binning, -1., -1., 
      mLowDoseMode)) {
        m_bUseViewInLowDose = saveVinLD;
        m_bUseSearchInLD = saveSinLD;
        m_bUseMontMapParams = saveUseMMP;
        UpdateData(false);
    }
    return;
  }
  UnloadParamData();
  saveUseMMP = mParam.useMontMapParams;
  if (mLowDoseMode)
    mParam.useMontMapParams = m_bUseMontMapInLD;
  mLDsetNum = MontageLDAreaIndex(&mParam);
  mWinApp->mDocWnd->MontParamInitFromConSet(&mParam, MontageConSetNum(&mParam, false));
  if (mLowDoseMode)
    mParam.useMontMapParams = saveUseMMP;
  if (!mLowDoseMode)
    mParam.moveStage = saveMoveStage > 0;
  else if (!m_bUseViewInLowDose && !m_bUseSearchInLD && !m_bUseMultishot &&
    mLastRecordMoveStage >= 0)
    mParam.moveStage = mLastRecordMoveStage > 0;
  LoadParamData(true);
  if ((m_bMoveStage ? 1 : 0) != saveMoveStage)
    OnMovestage();
  m_butUseMontMapParams.EnableWindow(!mWinApp->GetUseRecordForMontage() && !mSizeLocked
    && !(mLowDoseMode && (m_bUseViewInLowDose || m_bUseSearchInLD || m_bUseMultishot ||
      !mLdp[RECORD_CONSET].magIndex)));
  ManageSizeFields();
  ManageCamAndStageEnables();
}
