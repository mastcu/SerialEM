// CMultiGridDlg.cpp : Dialog for controlling multiple grid operations
//
// Copyright (C) 2024 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "EMscope.h"
#include "MultiGridTasks.h"
#include "MultiGridDlg.h"
#include "ShiftManager.h"
#include "MGSettingsManagerDlg.h"
#include "NavigatorDlg.h"
#include "MontageSetupDlg.h"
#include "StateDlg.h"
#include "CameraController.h"
#include "NavHelper.h"
#include "Shared\b3dutil.h"
#include "Utilities\KGetOne.h"

#define MAX_DLG_SLOTS 13

static int sIdTable[] = {IDC_STAT_RUN, IDC_STAT_NAME, IDC_BUT_CLOSE_ALL, PANEL_END,
  IDC_CHECK_MULGRID_RUN1, IDC_CHECK_MULGRID_RUN2, IDC_CHECK_MULGRID_RUN3,
  IDC_CHECK_MULGRID_RUN4, IDC_CHECK_MULGRID_RUN5, IDC_CHECK_MULGRID_RUN6,
  IDC_CHECK_MULGRID_RUN7, IDC_CHECK_MULGRID_RUN8, IDC_CHECK_MULGRID_RUN9,
  IDC_CHECK_MULGRID_RUN10, IDC_CHECK_MULGRID_RUN11, IDC_CHECK_MULGRID_RUN12,
  IDC_CHECK_MULGRID_RUN13, IDC_RADIO_MULGRID_SEL1, IDC_RADIO_MULGRID_SEL2, 
  IDC_RADIO_MULGRID_SEL3, IDC_STAT_NOTE, IDC_EDIT_GRID_NOTE,
  IDC_RADIO_MULGRID_SEL4, IDC_RADIO_MULGRID_SEL5, IDC_RADIO_MULGRID_SEL6,
  IDC_RADIO_MULGRID_SEL7, IDC_RADIO_MULGRID_SEL8, IDC_RADIO_MULGRID_SEL9,
  IDC_RADIO_MULGRID_SEL10, IDC_RADIO_MULGRID_SEL11, IDC_RADIO_MULGRID_SEL12,
  IDC_RADIO_MULGRID_SEL13,IDC_EDIT_MULGRID_NAME1, IDC_EDIT_MULGRID_NAME2,
  IDC_EDIT_MULGRID_NAME3, IDC_EDIT_MULGRID_NAME4, IDC_EDIT_MULGRID_NAME5,
  IDC_EDIT_MULGRID_NAME6, IDC_EDIT_MULGRID_NAME7, IDC_EDIT_MULGRID_NAME8,
  IDC_EDIT_MULGRID_NAME9, IDC_EDIT_MULGRID_NAME10, IDC_EDIT_MULGRID_NAME11,
  IDC_EDIT_MULGRID_NAME12, IDC_EDIT_MULGRID_NAME13, IDC_BUT_LOAD_GRID, 
  IDC_STAT_MULGRID_STATUS1, IDC_STAT_MULGRID_STATUS6, IDC_STAT_MULGRID_STATUS10,
  IDC_STAT_MULGRID_STATUS2, IDC_STAT_MULGRID_STATUS7, IDC_STAT_MULGRID_STATUS11,
  IDC_STAT_MULGRID_STATUS3, IDC_STAT_MULGRID_STATUS8, IDC_STAT_MULGRID_STATUS12,
  IDC_STAT_MULGRID_STATUS4, IDC_STAT_MULGRID_STATUS9, IDC_STAT_MULGRID_STATUS13,
  IDC_STAT_MULGRID_STATUS5, IDC_CHECK_TOGGLE_ALL, IDC_STAT_NOTE, IDC_EDIT_GRID_NOTE,
  IDC_BUT_SET_ORDER, IDC_BUT_RESET_ORDER, IDC_BUT_OPEN_LOGS, IDC_RREFINE_VIEW,
  IDC_RREFINE_SEARCH, IDC_RREFINE_STATE, IDC_CHECK_MG_REFINE, IDC_COMBO_REFINE_STATE,
  IDC_BUT_REALIGN_TO_GRID_MAP, IDC_BUT_OPEN_NAV, IDC_BUT_SET_GRID_TYPE, 
  IDC_STAT_REFINE_FOV, PANEL_END,
  IDC_BUT_MG_SETUP, IDC_STAT_MG_GRID_SETUP, IDC_TSS_LINE4, PANEL_END,
  IDC_BUT_MG_GET_NAMES, IDC_BUT_MG_INVENTORY, IDC_STAT_MG_PREFIX,
  IDC_EDIT_MG_PREFIX, IDC_CHECK_MG_APPEND_NAME, IDC_BUT_MG_CLEAR,
  IDC_BUT_MG_RESET_NAMES, IDC_STAT_MG_ROOTNAME,
  IDC_STAT_MG_CURRENT_DIR, IDC_BUT_SET_CURRENT_DIR, IDC_CHECK_MG_USE_SUBDIRS, PANEL_END,
  IDC_BUT_MG_LOW_MAG_MAPS, IDC_CHECK_RUN_LMMS, IDC_TSS_LINE10, PANEL_END,
  IDC_RLMM_SEARCH, IDC_STAT_USE1, IDC_RLMM_VIEW, IDC_RLMM_CUR_OR_STATE,
  IDC_CHECK_SET_LMM_STATE, IDC_COMBO_LMM_STATE, IDC_CHECK_REMOVE_OBJ,
  IDC_CHECK_SET_CONDENSER, IDC_EDIT_CONDENSER, IDC_STAT_MG_MICRON, IDC_RFULL_GRID_MONT,
  IDC_RNUM_MONT_PIECES, IDC_STAT_MONTAGE, IDC_EDIT_MONT_XPCS, IDC_STAT_MONT_BY,
  IDC_EDIT_MONT_NUM_YPCS, IDC_STAT_MONT_PCS, IDC_CHECK_USE_OVERLAP, IDC_EDIT_OVERLAP,
  IDC_STAT_PERCENT, IDC_BUT_SETUP_LMM_MONT, IDC_CHECK_AUTOCONTOUR, IDC_STAT_CONDENSER,
  IDC_STAT_APERTURE_TO, IDC_RSET_C1_APERTURE, IDC_RSET_C2_APERTURE,
  IDC_BUT_SETUP_AUTOCONT2, IDC_COMBO_END_SCRIPT, IDC_CHECK_SCRIPT_AT_END, PANEL_END,
  IDC_BUT_MG_MEDIUM_MAG_MAPS, IDC_CHECK_TAKE_MMMS, IDC_TSS_LINE6, PANEL_END,
  IDC_BUT_SETUP_POLYMONT, IDC_RMMM_SEARCH, IDC_STAT_USE2, IDC_RMMM_VIEW,
  IDC_RMMM_CUR_OR_STATE, IDC_COMBO_MMM_STATE1, IDC_COMBO_MMM_STATE2, IDC_COMBO_MMM_STATE3,
  IDC_STAT_SET_MMM_STATES, IDC_STAT_MMM_ACQ_TYPE, IDC_RSINGLE_IMAGE, IDC_RPOLYGON_MONT,
  IDC_BUT_SETUP_MAPPING,  IDC_COMBO_MMM_STATE4, IDC_RFIXED_MONT, PANEL_END,
  IDC_BUT_MG_FINAL_ACQ, IDC_CHECK_RUN_FINAL_ACQ, IDC_TSS_LINE8, PANEL_END,
  IDC_COMBO_FINAL_STATE3, IDC_COMBO_FINAL_STATE2, IDC_COMBO_FINAL_STATE1,
  IDC_COMBO_FINAL_STATE4, IDC_STAT_SET_FINAL_STATES, IDC_BUT_SETUP_FINAL_ACQ, 
  IDC_CHECK_SET_FINAL_BY_GRID, IDC_BUT_REVERT_TO_GLOBAL, IDC_CHECK_FRAMES_UNDER_SESSION,
  IDC_STAT_VECTOR_SOURCE, IDC_RVECTORS_FROM_MAPS, IDC_RVECTORS_FROM_SETTINGS, PANEL_END,
  IDC_BUT_START_RUN, IDC_TSS_LINE2, IDCANCEL, IDOK, IDC_BUTHELP, IDC_STAT_SPACER,
  IDC_BUT_RUN_UNDONE, IDC_BUT_CLOSE_RIGHT, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];


// CMultiGridDlg dialog


CMultiGridDlg::CMultiGridDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_MULTI_GRID, pParent)
  , m_strCurrentDir(_T(""))
  , m_strRootname(_T(""))
  , m_bAppendNames(FALSE)
  , m_bUseSubdirs(FALSE)
  , m_strPrefix(_T(""))
  , m_bSetCondenser(FALSE)
  , m_bRemoveObjective(FALSE)
  , m_iCondenserSize(150)
  , m_bUseMontOverlap(FALSE)
  , m_iMontOverlap(10)
  , m_iXpieces(3)
  , m_iYpieces(4)
  , m_iLMMmontType(0)
  , m_iSingleVsPolyMont(0)
  , m_iMMMacquireType(0)
  , m_bSetLMMstate(FALSE)
  , m_bTakeLMMs(FALSE)
  , m_bTakeMMMs(FALSE)
  , m_bRunFinalAcq(FALSE)
  , m_bSetFinalByGrid(FALSE)
  , m_bToggleAll(FALSE)
  , m_strNote(_T(""))
  , m_bScriptAtEnd(FALSE)
  , m_bRefineRealign(FALSE)
  , m_iRefineRealign(FALSE)
  , m_strRefineFOV(_T(""))
  , m_iC1orC2(0)
  , m_iVectorSource(0)
{
  mNonModal = true;
  for (int ind = 0; ind < MAX_MULGRID_PANELS; ind++)
    mPanelStates[ind] = true;
  mSelectedGrid = -1;
  mSelGridOnStage = false;
  mOnStageDlgIndex = -1;
  mGridMontSetupOK = false;
  mMMMsetupAcqType = -1;
  mMMMsetupMontType = -1;
  mMMMsetupStateForMont = -1;
  mMMMsetupOK = false;
  mMMMmontWasSetup = 0;
  mLMneedsLowDose = 0;
  mMMMneedsLowDose = 0;
  mRightIsOpen = -1;
  mClosedAll = false;
  mSettingOrder = false;
}

CMultiGridDlg::~CMultiGridDlg()
{
}

void CMultiGridDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_MG_CURRENT_DIR, m_strCurrentDir);
  DDX_Text(pDX, IDC_STAT_MG_ROOTNAME, m_strRootname);
  DDX_Check(pDX, IDC_CHECK_MG_APPEND_NAME, m_bAppendNames);
  DDX_Check(pDX, IDC_CHECK_MG_USE_SUBDIRS, m_bUseSubdirs);
  DDX_Control(pDX, IDC_EDIT_MG_PREFIX, m_editPrefix);
  DDX_Text(pDX, IDC_EDIT_MG_PREFIX, m_strPrefix);
  DDX_Radio(pDX, IDC_RLMM_SEARCH, m_iLMMacquireType);
  DDX_Control(pDX, IDC_COMBO_LMM_STATE, m_comboLMMstate);
  DDX_Check(pDX, IDC_CHECK_SET_CONDENSER, m_bSetCondenser);
  DDX_Check(pDX, IDC_CHECK_REMOVE_OBJ, m_bRemoveObjective);
  DDX_MM_INT(pDX, IDC_EDIT_CONDENSER, m_iCondenserSize, JEOLscope ? 0 : 1, 250,
    "condenser size to switch to");
  DDX_Check(pDX, IDC_CHECK_USE_OVERLAP, m_bUseMontOverlap);
  DDX_MM_INT(pDX, IDC_EDIT_OVERLAP, m_iMontOverlap, 3, 30, "montage overlap");
  DDX_MM_INT(pDX, IDC_EDIT_MONT_XPCS, m_iXpieces, 1, 30,
    "number of montages pieces in X");
  DDX_MM_INT(pDX, IDC_EDIT_MONT_NUM_YPCS, m_iYpieces, 1, 30,
    "number of montages pieces in Y");
  DDX_Radio(pDX, IDC_RFULL_GRID_MONT, m_iLMMmontType);
  DDX_Control(pDX, IDC_COMBO_MMM_STATE1, m_comboMMMstate1);
  DDX_Control(pDX, IDC_COMBO_MMM_STATE2, m_comboMMMstate2);
  DDX_Control(pDX, IDC_COMBO_MMM_STATE3, m_comboMMMstate3);
  DDX_Control(pDX, IDC_COMBO_FINAL_STATE3, m_comboFinalState3);
  DDX_Control(pDX, IDC_COMBO_FINAL_STATE2, m_comboFinalState2);
  DDX_Control(pDX, IDC_COMBO_FINAL_STATE1, m_comboFinalState1);
  DDX_Control(pDX, IDC_STAT_MG_GRID_SETUP, m_statMgSetup);
  DDX_Radio(pDX, IDC_RSINGLE_IMAGE, m_iSingleVsPolyMont);
  DDX_Radio(pDX, IDC_RMMM_SEARCH, m_iMMMacquireType);
  DDX_Check(pDX, IDC_CHECK_SET_LMM_STATE, m_bSetLMMstate);
  DDX_Check(pDX, IDC_CHECK_AUTOCONTOUR, m_bAutocontour);
  DDX_Control(pDX, IDC_CHECK_RUN_LMMS, m_butTakeLMMs);
  DDX_Check(pDX, IDC_CHECK_RUN_LMMS, m_bTakeLMMs);
  DDX_Control(pDX, IDC_CHECK_TAKE_MMMS, m_butTakeMMMs);
  DDX_Check(pDX, IDC_CHECK_TAKE_MMMS, m_bTakeMMMs);
  DDX_Control(pDX, IDC_CHECK_RUN_FINAL_ACQ, m_butRunFinalAcq);
  DDX_Check(pDX, IDC_CHECK_RUN_FINAL_ACQ, m_bRunFinalAcq);
  DDX_Control(pDX, IDC_STAT_MG_PREFIX, m_statMgPrefix);
  DDX_Control(pDX, IDC_COMBO_MMM_STATE4, m_comboMMMstate4);
  DDX_Control(pDX, IDC_COMBO_FINAL_STATE4, m_comboFinalState4);
  DDX_Control(pDX, IDC_STAT_MG_CURRENT_DIR, m_statCurrentDir);
  DDX_Control(pDX, IDC_BUT_CLOSE_RIGHT, m_butCloseRight);
  DDX_Check(pDX, IDC_CHECK_SET_FINAL_BY_GRID, m_bSetFinalByGrid);
  DDX_Check(pDX, IDC_CHECK_FRAMES_UNDER_SESSION, m_bFramesUnderSession);
  DDX_Check(pDX, IDC_CHECK_TOGGLE_ALL, m_bToggleAll);
  DDX_Text(pDX, IDC_EDIT_GRID_NOTE, m_strNote);
  DDX_Control(pDX, IDC_COMBO_END_SCRIPT, m_comboEndScript);
  DDX_Check(pDX, IDC_CHECK_SCRIPT_AT_END, m_bScriptAtEnd);
  DDX_Check(pDX, IDC_CHECK_MG_REFINE, m_bRefineRealign);
  DDX_Radio(pDX, IDC_RREFINE_SEARCH, m_iRefineRealign);
  DDX_Control(pDX, IDC_COMBO_REFINE_STATE, m_comboRefineState);
  DDX_Text(pDX, IDC_STAT_REFINE_FOV, m_strRefineFOV);
  DDX_Radio(pDX, IDC_RSET_C1_APERTURE, m_iC1orC2);
  DDX_Radio(pDX, IDC_RVECTORS_FROM_MAPS, m_iVectorSource);
}


BEGIN_MESSAGE_MAP(CMultiGridDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_MG_APPEND_NAME, OnCheckMgAppendName)
  ON_BN_CLICKED(IDC_BUT_MG_GET_NAMES, OnButMgGetNames)
  ON_BN_CLICKED(IDC_BUT_MG_INVENTORY, OnButMgInventory)
  ON_BN_CLICKED(IDC_BUT_MG_RESET_NAMES, OnButMgResetNames)
  ON_BN_CLICKED(IDC_BUT_MG_CLEAR, OnButMgClearNames)
  ON_BN_CLICKED(IDC_BUT_SET_CURRENT_DIR, OnButSetCurrentDir)
  ON_EN_CHANGE(IDC_EDIT_MG_PREFIX, OnEnChangeEditMgPrefix)
  ON_BN_CLICKED(IDC_BUT_MG_SETUP, OnButMgSetup)
  ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_EDIT_MULGRID_NAME1, IDC_EDIT_MULGRID_NAME13,
    OnEnKillfocusEditName)
  ON_COMMAND_RANGE(IDC_RADIO_MULGRID_SEL1, IDC_RADIO_MULGRID_SEL13, OnRadioMulgridSelect)
  ON_BN_CLICKED(IDC_BUT_LOAD_GRID, OnButLoadGrid)
  ON_BN_CLICKED(IDC_BUT_REALIGN_TO_GRID_MAP, OnButRealignToGridMap)
  ON_BN_CLICKED(IDC_RLMM_SEARCH, OnRadioLMMSearch)
  ON_BN_CLICKED(IDC_RLMM_VIEW, OnRadioLMMSearch)
  ON_BN_CLICKED(IDC_RLMM_CUR_OR_STATE, OnRadioLMMSearch)
  ON_BN_CLICKED(IDC_CHECK_SET_LMM_STATE, OnCheckSetLmmState)
  ON_EN_KILLFOCUS(IDC_EDIT_CONDENSER, OnEnKillfocusEditCondenser)
  ON_BN_CLICKED(IDC_BUT_SETUP_LMM_MONT, OnButSetupLmmMont)
  ON_BN_CLICKED(IDC_CHECK_USE_OVERLAP, OnCheckUseOverlap)
  ON_EN_KILLFOCUS(IDC_EDIT_OVERLAP, OnEnKillfocusEditOverlap)
  ON_EN_KILLFOCUS(IDC_EDIT_MONT_XPCS, OnEnKillfocusEditMontXpcs)
  ON_EN_KILLFOCUS(IDC_EDIT_MONT_NUM_YPCS, OnEnKillfocusEditMontNumYpcs)
  ON_BN_CLICKED(IDC_RFULL_GRID_MONT, OnRfullGridMont)
  ON_BN_CLICKED(IDC_RNUM_MONT_PIECES, OnRfullGridMont)
  ON_BN_CLICKED(IDC_BUT_MG_LOW_MAG_MAPS, OnButMgLowMagMaps)
  ON_BN_CLICKED(IDC_BUT_MG_MEDIUM_MAG_MAPS, OnButMgMediumMagMaps)
  ON_BN_CLICKED(IDC_BUT_MG_FINAL_ACQ, OnButMgFinalAcq)
  ON_CBN_SELENDOK(IDC_COMBO_LMM_STATE, OnSelendokComboLmmState)
  ON_CBN_SELENDOK(IDC_COMBO_MMM_STATE1, OnSelendokComboMmmState1)
  ON_CBN_SELENDOK(IDC_COMBO_MMM_STATE2, OnSelendokComboMmmState2)
  ON_CBN_SELENDOK(IDC_COMBO_MMM_STATE3, OnSelendokComboMmmState3)
  ON_CBN_SELENDOK(IDC_COMBO_MMM_STATE4, OnSelendokComboMmmState4)
  ON_CBN_SELENDOK(IDC_COMBO_FINAL_STATE1, OnSelendokComboFinalState1)
  ON_CBN_SELENDOK(IDC_COMBO_FINAL_STATE2, OnSelendokComboFinalState2)
  ON_CBN_SELENDOK(IDC_COMBO_FINAL_STATE3, OnSelendokComboFinalState3)
  ON_CBN_SELENDOK(IDC_COMBO_FINAL_STATE4, OnSelendokComboFinalState4)
  ON_BN_CLICKED(IDC_BUT_SETUP_FINAL_ACQ, OnButSetupFinalAcq)
  ON_BN_CLICKED(IDC_BUT_SETUP_MAPPING, OnButSetupMapping)
  ON_BN_CLICKED(IDC_BUT_SETUP_POLYMONT, OnButSetupPolymont)
  ON_BN_CLICKED(IDC_RSINGLE_IMAGE, OnRsingleImage)
  ON_BN_CLICKED(IDC_RPOLYGON_MONT, OnRsingleImage)
  ON_BN_CLICKED(IDC_RFIXED_MONT, OnRsingleImage)
  ON_BN_CLICKED(IDC_RMMM_SEARCH, OnRmmmSearch)
  ON_BN_CLICKED(IDC_RMMM_VIEW, OnRmmmSearch)
  ON_BN_CLICKED(IDC_RMMM_CUR_OR_STATE, OnRmmmSearch)
  ON_BN_CLICKED(IDC_CHECK_RUN_LMMS, OnCheckRunLmms)
  ON_BN_CLICKED(IDC_CHECK_TAKE_MMMS, OnCheckTakeMmms)
  ON_BN_CLICKED(IDC_CHECK_RUN_FINAL_ACQ, OnCheckRunFinalAcq)
  ON_BN_CLICKED(IDC_CHECK_SET_CONDENSER, OnCheckSetCondenser)
  ON_BN_CLICKED(IDC_CHECK_AUTOCONTOUR, OnCheckAutocontour)
  ON_BN_CLICKED(IDC_BUT_SETUP_AUTOCONT2, OnButSetupAutocont)
  ON_BN_CLICKED(IDC_BUT_START_RUN, OnButStartRun)
  ON_COMMAND_RANGE(IDC_CHECK_MULGRID_RUN1, IDC_CHECK_MULGRID_RUN13, OnCheckMulgridRun)
  ON_BN_CLICKED(IDC_BUT_RUN_UNDONE, OnButRunUndone)
  ON_BN_CLICKED(IDC_BUT_OPEN_NAV, OnButOpenNav)
  ON_BN_CLICKED(IDC_BUT_SET_GRID_TYPE, OnButSetGridType)
  ON_WM_PAINT()
  ON_BN_CLICKED(IDC_BUT_CLOSE_RIGHT, OnButCloseRight)
  ON_BN_CLICKED(IDC_CHECK_SET_FINAL_BY_GRID, OnCheckSetFinalByGrid)
  ON_BN_CLICKED(IDC_BUT_REVERT_TO_GLOBAL, OnButRevertToGlobal)
  ON_BN_CLICKED(IDC_CHECK_FRAMES_UNDER_SESSION, OnCheckFramesUnderSession)
  ON_BN_CLICKED(IDC_CHECK_MG_USE_SUBDIRS, OnCheckMgUseSubdirs)
  ON_BN_CLICKED(IDC_BUT_CLOSE_ALL, OnButCloseAll)
  ON_BN_CLICKED(IDC_CHECK_TOGGLE_ALL, OnCheckToggleAll)
  ON_EN_KILLFOCUS(IDC_EDIT_GRID_NOTE, OnEnKillfocusEditGridNote)
  ON_BN_CLICKED(IDC_BUT_SET_ORDER, OnButSetOrder)
  ON_BN_CLICKED(IDC_BUT_RESET_ORDER, OnButResetOrder)
  ON_BN_CLICKED(IDC_BUT_OPEN_LOGS, OnButOpenLogs)
  ON_BN_CLICKED(IDC_CHECK_SCRIPT_AT_END, OnCheckScriptAtEnd)
  ON_CBN_SELENDOK(IDC_COMBO_END_SCRIPT, OnSelendokComboEndScript)
  ON_BN_CLICKED(IDC_CHECK_MG_REFINE, OnCheckMgRefine)
  ON_BN_CLICKED(IDC_RREFINE_SEARCH, OnRRefineRealign)
  ON_BN_CLICKED(IDC_RREFINE_VIEW, OnRRefineRealign)
  ON_BN_CLICKED(IDC_RREFINE_STATE, OnRRefineRealign)
  ON_CBN_SELENDOK(IDC_COMBO_REFINE_STATE, OnSelendokComboRefineState)
  ON_BN_CLICKED(IDC_RSET_C1_APERTURE, OnRsetC1Aperture)
  ON_BN_CLICKED(IDC_RSET_C2_APERTURE, OnRsetC1Aperture)
  ON_BN_CLICKED(IDC_RVECTORS_FROM_MAPS, OnRvectorsFromMaps)
  ON_BN_CLICKED(IDC_RVECTORS_FROM_SETTINGS, OnRvectorsFromMaps)
END_MESSAGE_MAP()


// CMultiGridDlg message handlers
/*
 * INITIALIZATION AND STANDARD OK/CANCEL ETC HANDLING
 */
BOOL CMultiGridDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  CString names;
  CRect rect;
  int *activeList = mWinApp->GetActiveCameraList();
  int cam;
  bool noSubdirs, movable;
  CameraParameters *camParams = mWinApp->GetCamParams();
  mMGTasks = mWinApp->mMultiGridTasks;
  mMasterParams = mMGTasks->GetMultiGridParams();

  // Leave custom run settings as they were, but clear out this vector until reload
  mDlgIndToJCDindex.clear();
  mMontSetupSizesXY = mMGTasks->GetMontSetupConsetSizes();
  mCustomRunDlgInds = mMGTasks->GetCustomRunDlgInds();
  mParams = *mMasterParams;
  mScope = mWinApp->mScope;
  mCartInfo = mWinApp->mScope->GetJeolLoaderInfo();
  mStateArray = mWinApp->mNavHelper->GetStateArray();
  m_butTakeLMMs.GetWindowRect(&rect);
  mBiggerFont.CreateFont(B3DNINT(0.75 * rect.Height()), 0, 0, 0, FW_MEDIUM,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
    FF_DONTCARE, mWinApp->mScopeStatus.GetBigFontName());
  mBigBoldFont.CreateFont(B3DNINT(0.75 * rect.Height()), 0, 0, 0, FW_BOLD,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
    FF_DONTCARE, mWinApp->mScopeStatus.GetBigFontName());
  m_butCloseRight.SetFont(mWinApp->GetLittleFont(&m_butCloseRight));
  mSecondColPanel = 4;
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  mNumUsedSlots = 0;
  mNumMMMcombos = mMGTasks->GetNumMMMstateCombos();
  mNumFinalCombos = mMGTasks->GetNumFinalStateCombos();
  RepackStatesIfNeeded(mNumMMMcombos, &mParams.MMMstateNums[0],
    &mParams.MMMstateNames[0]);
  RepackStatesIfNeeded(mNumFinalCombos, &mParams.finalStateNums[0],
    &mParams.finalStateNames[0]);
  LoadAllComboBoxes();
  LoadMacrosIntoDropDown(m_comboEndScript, false, false);
  m_strPrefix = mMGTasks->GetPrefix();

  // Determine if any frame-saving camera has movable frame directory, otherwise drop
  mDropFrameOption = true;
  for (cam = 0; cam < mWinApp->GetNumActiveCameras(); cam++) {
    if (mWinApp->mCamera->IsDirectDetector(&camParams[activeList[cam]]) || 
      (camParams[activeList[cam]].canTakeFrames & 1)) {
      mWinApp->mCamera->GetCameraFrameFolder(&camParams[activeList[cam]], noSubdirs,
        movable);
      if (movable) {
        mDropFrameOption = false;
        break;
      }
    }
  }
  mSingleGridMode = !mScope->GetScopeHasAutoloader();
  if (mSingleGridMode) {
    mParams.appendNames = false;
  }

  // All hiding is in ManagePanelsq
  if (!mMGTasks->GetShowRefineAfterRealign() && !mMGTasks->GetSkipGridRealign())
    mParams.refineAfterRealign = false;
  ParamsToDialog();
  UpdateData(false);
  mPanelStates[5] = mParams.acquireLMMs;
  mPanelStates[7] = mParams.acquireMMMs;
  mPanelStates[9] = mParams.runFinalAcq;
  SetDlgItemText(IDC_BUT_MG_SETUP, mPanelStates[3] ? "-" : "+");
  SetDlgItemText(IDC_BUT_MG_LOW_MAG_MAPS, mPanelStates[5] ? "-" : "+");
  SetDlgItemText(IDC_BUT_MG_MEDIUM_MAG_MAPS, mPanelStates[7] ? "-" : "+");
  SetDlgItemText(IDC_BUT_MG_FINAL_ACQ, mPanelStates[9] ? "-" : "+");
  mBoldFont = mWinApp->GetBoldFont(&m_statMgSetup);
  m_statMgSetup.SetFont(&mBiggerFont);
  m_butTakeLMMs.SetFont(mParams.acquireLMMs ? &mBigBoldFont : &mBiggerFont);
  m_butTakeMMMs.SetFont(mParams.acquireMMMs ? &mBigBoldFont : &mBiggerFont);
  m_butRunFinalAcq.SetFont(mParams.runFinalAcq ? &mBigBoldFont : &mBiggerFont);
  ManagePanels();
  EnableDlgItem(IDC_BUT_LOAD_GRID, false);
  EnableDlgItem(IDC_BUT_OPEN_NAV, false);
  EnableDlgItem(IDC_BUT_REALIGN_TO_GRID_MAP, false);
  EnableDlgItem(IDC_BUT_OPEN_LOGS, false);
  if (mMGTasks->GetSkipGridRealign())
    SetDlgItemText(IDC_CHECK_MG_REFINE, "Refine instead of Realign using:");
  if (mSingleGridMode) {
    InitForSingleGrid();
  }
  CheckIfAnyUndoneToRun();
  UpdateEnables();
  ManageRefineFOV();
  ManageCondenserUnits();
  UpdateCurrentDir();
  SetRootname();

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CMultiGridDlg::OnOK()
{
  CString errStr;
  SyncToMasterParams();
  if (!mMGTasks->GetNamesLocked())
    mMGTasks->SetPrefix(m_strPrefix);
  if (mNumUsedSlots)
    mMGTasks->SaveSessionFile(errStr);
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CMultiGridDlg::OnCancel()
{
  IntVec *runInds = mMGTasks->GetLastDfltRunInds();
  mMGTasks->SetLastNumSlots(mNumUsedSlots);
  DefaultRunDlgIndexes(*runInds);
  mWinApp->mNavHelper->GetMultiGridPlacement();
  mWinApp->mNavHelper->mMultiGridDlg = NULL;
  if (mWinApp->mNavHelper->mStateDlg)
    mWinApp->mNavHelper->mStateDlg->Update();
  DestroyWindow();
}

void CMultiGridDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

// This is called on external close like exiting, so make sure settings saved
void CMultiGridDlg::CloseWindow()
{
  OnOK();
}

BOOL CMultiGridDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

void CMultiGridDlg::OnPaint()
{
  if (mOnStageDlgIndex >= 0) {
    CWnd *wnd = GetDlgItem(IDC_EDIT_MULGRID_NAME1 + mOnStageDlgIndex);
    CPaintDC dc(this);
    DrawButtonOutline(dc, wnd, 2, RGB(0, 255, 0), -2);
  }
  CBaseDlg::OnPaint();
}

/*
 * MESSAGES AND ROUTINES FOR GRID SETUP
 */
// Option to append names
void CMultiGridDlg::OnCheckMgAppendName()
{
  UPDATE_DATA_TRUE;
  SetRootname();
  mWinApp->RestoreViewFocus();
}

/*
 * Get the names from FEI autoloader or cartridge table and set up user names
 */
void CMultiGridDlg::OnButMgGetNames()
{
  CString names, str, nameArr[MAX_LOADER_SLOTS];
  JeolCartridgeData jcd;
  ShortVec statVec;
  int status, numSlots = 1, slot = 1, highest = 0, ind, numEmpty = 0;
  int maybeStage = -1, onStage = -1;
  if (FEIscope) {
    for (slot = 1; slot <= numSlots; slot++) {

      // Call with argument to get number of slots AND skip getting name
      if (!mScope->CassetteSlotStatus(slot, status, names, &numSlots))
        return;
      B3DCLAMP(numSlots, 1, MAX_DLG_SLOTS);
      statVec.push_back(status);
      if (status == 1)
        highest = slot;
    }

    // Now try to get all the names
    names = "";
    if (!mWinApp->mScope->CassetteSlotStatus(0, status, names) && !names.IsEmpty()) {
      AfxMessageBox(names, MB_EXCLAME);
      return;
    }
    for (slot = 0; slot < numSlots; slot++) {
      
      if (!names.IsEmpty()) {

      // Copy the names into the array
        ind = names.Find("\n");
        if (ind < 0) {
          nameArr[slot] = names;
        } else {
          nameArr[slot] = names.Left(ind);
          names = names.Mid(ind + 1);
        }
        if (nameArr[slot] == "!NONAME!")
          nameArr[slot] = "";
        if (statVec[slot] != 1 && !nameArr[slot].IsEmpty()) {
          numEmpty++;
          maybeStage = slot;
        }
      } else {

        // Or just see if there is one empty slot below the highest
        if (statVec[slot] != 1 && slot < highest) {
          numEmpty++;
          maybeStage = slot;
        }
      }
    }

    // Deal with figuring out what grid is on the stage, as this impacts how many slots
    // to show
    if (numEmpty == 1) {
      str.Format("Is the grid from slot %d loaded on the stage?", maybeStage + 1);
      if (AfxMessageBox(str, MB_QUESTION) != IDYES)
        maybeStage = -1;
    } else
      maybeStage = -1;
    if (maybeStage < 0) {
      maybeStage = highest + 1;
      if (KGetOneInt("If there is a grid on the stage, enter the slot it was loaded from;"
        " if not, enter 0", maybeStage))
        maybeStage--;
      else
        maybeStage = -1;
      if (maybeStage >= numSlots)
        maybeStage = -1;
    }

    mCartInfo->RemoveAll();
    mNumUsedSlots = 0;

    // Add to the cartInfo
    for (slot = 0; slot < numSlots; slot++) {
      str = nameArr[slot];

       // Add a cartridge if it is occupied
      if (statVec[slot] == 1 || slot == maybeStage) {
        jcd.Init();
        jcd.id = slot + 1;
        jcd.slot = slot + 1;
        jcd.station = statVec[slot] == 1 ? JAL_STATION_MAGAZINE : JAL_STATION_STAGE;
        if (statVec[slot] != 1)
          onStage = (int)mCartInfo->GetSize();

        // Supply string if none
        if (str.IsEmpty())
          str.Format("Car%d", slot + 1);
        jcd.name = str;
        mMGTasks->ReplaceBadCharacters(str);
        jcd.userName = str;
        mCartInfo->Add(jcd);
        mNumUsedSlots++;
      }
    }
  } else {

    // JEOL: fix names for username
    mNumUsedSlots = (int)mCartInfo->GetSize();
    for (ind = 0; ind < mNumUsedSlots; ind++) {
      JeolCartridgeData &jcdEl = mCartInfo->ElementAt(ind);
      if (jcdEl.name.IsEmpty())
        jcdEl.name.Format("Car%d", jcdEl.id);
      str = jcdEl.name;
      mMGTasks->ReplaceBadCharacters(str);
      jcdEl.userName = str;
      if (jcdEl.station == JAL_STATION_STAGE)
        onStage = ind;
    }
  }

  ReloadTable(1, 1);
  if (onStage >= 0)
    NewGridOnStage(onStage);
  CheckIfAnyUndoneToRun();
  UpdateEnables();
  mMGTasks->SetAdocChanged(true);
  mMGTasks->TurnOffSubset();
  mWinApp->RestoreViewFocus();
}

/*
 * Load the table with names; resetOrClear 0 to use names as is, 1 to use original
 * name with replacements, 2 to clear.  checkRuns -1 to turn off or 1 to turn on Run buts
 */
void CMultiGridDlg::ReloadTable(int resetOrClear, int checkRuns)
{
  IntVec order;
  FloatVec idVec;
  int slot, ind;
  CString str;
  CButton *button;
  JeolCartridgeData jcd;
  IntVec *lastRuns = mMGTasks->GetLastDfltRunInds();
  bool checkLast = checkRuns > 1 && mNumUsedSlots == mMGTasks->GetLastNumSlots() &&
    mNumUsedSlots >= (int)lastRuns->size();

  // Get vector of IDs or inverse FEI IDs and order them
  for (slot = 0; slot < mNumUsedSlots; slot++) {
    order.push_back(slot);
    jcd = mCartInfo->GetAt(slot);
    idVec.push_back((float)jcd.id);
  }
  if (mNumUsedSlots)
    rsSortIndexedFloats(&idVec[0], &order[0], mNumUsedSlots);

  // Load the slots from the top in the given order, setting name as indicated
  mDlgIndToJCDindex.clear();
  for (slot = 0; slot < mNumUsedSlots; slot++) {
    ind = order[JEOLscope ? slot : (mNumUsedSlots - 1) - slot];
    JeolCartridgeData &jcdEl = mCartInfo->ElementAt(ind);
    mDlgIndToJCDindex.push_back(ind);

    str.Format("%d", ind >= 0 ? jcdEl.id : mNumUsedSlots - slot);
    SetDlgItemText(IDC_RADIO_MULGRID_SEL1 + slot, str);
    EnableDlgItem(IDC_CHECK_MULGRID_RUN1 + slot, ind >= 0);
    EnableDlgItem(IDC_EDIT_MULGRID_NAME1 + slot, ind >= 0);
    if (checkRuns) {
      button = (CButton *)GetDlgItem(IDC_CHECK_MULGRID_RUN1 + slot);
      if (button) {
        if (checkRuns > 1 && mMGTasks->GetUseCustomOrder())
          button->SetCheck(LookupRunOrder(*mCustomRunDlgInds, slot) >= 0 ?
            BST_CHECKED : BST_UNCHECKED);
        else if (checkLast)
          button->SetCheck(LookupRunOrder(*lastRuns, slot) >= 0 ?
            BST_CHECKED : BST_UNCHECKED);
        else
          button->SetCheck((checkRuns > 0 && !(jcdEl.status & MGSTAT_FLAG_TOO_DARK)) ?
            BST_CHECKED : BST_UNCHECKED);
      }
    }

    if (ind < 0) {
      SetDlgItemText(IDC_EDIT_MULGRID_NAME1 + slot, "");

    } else {
      if (resetOrClear) {
        if (resetOrClear > 1)
          str = "";
        else {
          str = jcdEl.name;
          mMGTasks->ReplaceBadCharacters(str);
        }
        jcdEl.userName = str;
        mMGTasks->SetAdocChanged(true);
      }
      SetDlgItemText(IDC_EDIT_MULGRID_NAME1 + slot, UserNameWithNote(jcdEl.userName, 
        jcdEl.note));
      SetStatusText(ind);
    }
  }

  // open the right side the first time ames are gotten
  if (mRightIsOpen < 0 && mNumUsedSlots > 0)
    mRightIsOpen = 1;
  m_bToggleAll = true;
  ManagePanels();
  DisplayRunOrder();
  mNamesChanged = false;
}

// Return the user name, with option note appended
CString CMultiGridDlg::UserNameWithNote(CString &userName, CString &note)
{
  CString str = userName;
  if (!note.IsEmpty()) {
    str = (str + " ") + (const char)0x97;
    str += " " + note;
  }
  return str;
}

// Button to start inventory
void CMultiGridDlg::OnButMgInventory()
{
  mMGTasks->GetInventory();
  mWinApp->RestoreViewFocus();
}

// Button to reset the names to cartInfo names
void CMultiGridDlg::OnButMgResetNames()
{
  if (!mNamesChanged || (mNamesChanged && AfxMessageBox("Resetting to names from scope "
    "will lose your name changes\n\nAre you sure you want to do this?", MB_QUESTION) == 
    IDYES))
    ReloadTable(1, 0);
  mWinApp->RestoreViewFocus();
}

// Button to clear out the names
void CMultiGridDlg::OnButMgClearNames()
{
  if (!mNamesChanged || (mNamesChanged && AfxMessageBox("Clearing the names will throw "
    "away your name changes\n\nAre you sure you want to do this?", MB_QUESTION) == IDYES))
    ReloadTable(2, 0);
  mWinApp->RestoreViewFocus();
}

// Option to use subdirs: change name of frame option
void CMultiGridDlg::OnCheckMgUseSubdirs()
{
  mWinApp->RestoreViewFocus();
  UPDATE_DATA_TRUE;
  ManageFrameDirOption();
}

// Button to set the working directory
void CMultiGridDlg::OnButSetCurrentDir()
{
  mWinApp->RestoreViewFocus();
  mWinApp->mDocWnd->OnFileSetCurrentDirectory();
}

// Toggle refine after realign
void CMultiGridDlg::OnCheckMgRefine()
{
  UPDATE_DATA_TRUE;
  UpdateEnables();
  mWinApp->RestoreViewFocus();
  ManageRefineFOV();
}

// New radio button choice on refine image type
void CMultiGridDlg::OnRRefineRealign()
{
  UPDATE_DATA_TRUE;
  UpdateEnables();
  mWinApp->RestoreViewFocus();
  ManageRefineFOV();
}

// New prefix characters are entered: fix them up
void CMultiGridDlg::OnEnChangeEditMgPrefix()
{
  UPDATE_DATA_TRUE;
  mMGTasks->ReplaceBadCharacters(m_strPrefix);
  SetRootname();
}

// A new name is entered in the edit box: fix it up and save
void CMultiGridDlg::OnEnKillfocusEditName(UINT nID)
{
  CString str;
  int ind = nID - IDC_EDIT_MULGRID_NAME1;
  CEdit *edit = (CEdit *)GetDlgItem(nID);
  mWinApp->RestoreViewFocus();
  if (!edit)
    return;
  edit->GetWindowText(str);
  if (mMGTasks->ReplaceBadCharacters(str))
    edit->SetWindowText(str);
  if (ind >= 0 && ind < (int)mDlgIndToJCDindex.size() && mDlgIndToJCDindex[ind] >= 0) {
    JeolCartridgeData &jcdEl = FindCartDataForDlgIndex(ind);
    if (str.Compare(jcdEl.userName)) {
      mNamesChanged = true;
      mMGTasks->SetAdocChanged(true);
      jcdEl.userName = str;
    }
  }
}

// Returns a read-only copy of the jcd at the given dialog row
JeolCartridgeData CMultiGridDlg::FindCartDataForDlgIndex(int ind)
{
  static JeolCartridgeData jcd;
  jcd.slot = -1;
  if (ind >= 0 && ind < (int)mDlgIndToJCDindex.size() && mDlgIndToJCDindex[ind] >= 0 &&
    mDlgIndToJCDindex[ind] < (int)mCartInfo->GetSize())
    jcd = mCartInfo->GetAt(mDlgIndToJCDindex[ind]);
  return jcd;
}

/*
 * Return lists of the jcd indexes and slot #s (FEI) or index + 1 in JEOL catalogue ready
 * for use in loading grid, for custom or default order
 */
int CMultiGridDlg::GetListOfGridsToRun(ShortVec &jcdInds, ShortVec &slotNums)
{
  JeolCartridgeData jcd;
  int ind, jcdInd;
  IntVec dlgInds;
  jcdInds.clear();
  slotNums.clear();
  if (mMGTasks->GetUseCustomOrder())
    dlgInds = *mCustomRunDlgInds;
  else
    DefaultRunDlgIndexes(dlgInds);
  for (ind = 0; ind < (int)dlgInds.size(); ind++) {
    if (dlgInds[ind] < (int)mDlgIndToJCDindex.size()) {
      jcdInd = mDlgIndToJCDindex[dlgInds[ind]];
      jcdInds.push_back(jcdInd);
      if (FEIscope) {
        jcd = mCartInfo->GetAt(jcdInd);
        slotNums.push_back(jcd.slot);
      } else {
        slotNums.push_back(jcdInd + 1);
      }
    }
  }
  return (int)jcdInds.size();
}

/*
 * Return ordered dialog indexes for the default order, accounting for grid on stage
 */
void CMultiGridDlg::DefaultRunDlgIndexes(IntVec &dlgInds)
{
  int loop, ind, indOnStage = -1;
  short tmp;
  CButton *button;
  dlgInds.clear();
  for (loop = 0; loop < mNumUsedSlots; loop++) {
    ind = FEIscope ? mNumUsedSlots - (loop + 1) : loop;
    button = (CButton *)GetDlgItem(IDC_CHECK_MULGRID_RUN1 + ind);
    if (button && button->GetCheck() && ind < (int)mDlgIndToJCDindex.size()) {
      if (mOnStageDlgIndex == ind)
        indOnStage = (int)dlgInds.size();
      dlgInds.push_back(ind);
    }
  }
  if (mMGTasks->GetReferenceCounts() != 0. && indOnStage > 0) {
    tmp = dlgInds[indOnStage];
    for (ind = indOnStage; ind > 0; ind--)
      dlgInds[ind] = dlgInds[ind - 1];

    dlgInds[0] = tmp;
  }
}
/*
 * Set the run checkbox labels with the order numbers for ones to be run
 */
void CMultiGridDlg::DisplayRunOrder() 
{
  IntVec dlgInds;
  int ind, run;
  CString str;
  if (mMGTasks->GetUseCustomOrder())
    dlgInds = *mCustomRunDlgInds;
  else
    DefaultRunDlgIndexes(dlgInds);
  for (ind = 0; ind < mNumUsedSlots; ind++) {
    run = LookupRunOrder(dlgInds, ind) + 1;
    str = "";
    if (run > 0)
      str.Format("%d", run);
    SetDlgItemText(IDC_CHECK_MULGRID_RUN1 + ind, str);
  }
}

/*
 * Look up a dialog index in the given run list and return its index there, or -1
 */
int CMultiGridDlg::LookupRunOrder(IntVec &runDlgInds, int dlgInd)
{
  for (int ind = 0; ind < (int)runDlgInds.size(); ind++)
    if (runDlgInds[ind] == dlgInd)
      return ind;
  return -1;
}

/*
 * Set the rootname in the display given the contents of the prefix
 * Can't use the routine in MGtasks because the C# component is omitted if the name
 * is Car#
 */
void CMultiGridDlg::SetRootname()
{
  m_strRootname = "Root name: " + m_strPrefix;
  int prefLen = m_strPrefix.GetLength();
  char last;
  int pound, carLen, gridLen;
  if (prefLen > 1) {
    last = m_strPrefix.GetAt(prefLen - 1);
    pound = (last == '_' || last == '-') ? 1 : 0;
    carLen = prefLen - (3 + pound);
    gridLen = prefLen - (4 + pound);
  }

  // Allow for Car/car and Grid/grid with or without a - or _
  if ((prefLen < 3 || !(m_strPrefix.Find("Car") == carLen ||
    m_strPrefix.Find("car") == carLen || m_strPrefix.Find("Grid") == gridLen ||
    m_strPrefix.Find("grid") == gridLen)) && !mSingleGridMode) {
    if (m_strPrefix.IsEmpty())
      m_strRootname = "C";
    else
      m_strRootname += "_C";
  }
  if (!mSingleGridMode) {
    if (JEOLscope)
      m_strRootname += "##";
    m_strRootname += "##";
  }
  if (m_bAppendNames)
    m_strRootname += "_name";
  UpdateData(false);
}

/*
 * Interpret the status flags for a given item in catalogue and set up status text
 */
void CMultiGridDlg::SetStatusText(int jcdInd)
{
  CString str;
  JeolCartridgeData jcd = mCartInfo->GetAt(jcdInd);
  for (int dlin = 0; dlin < mNumUsedSlots; dlin++) {
    if (mDlgIndToJCDindex[dlin] == jcdInd) {
      str += jcd.separateState ? 'S' : (const char)0x97;
      str += (jcd.multiShotParamIndex >= 0 || jcd.holeFinderParamIndex >= 0 ||
        jcd.autoContParamIndex >= 0 || jcd.generalParamIndex >= 0 || 
        jcd.focusPosParamIndex >= 0 || jcd.finalDataParamIndex >= 0) ?
        'P' : (const char)0x97;
      str += ":";
      if (jcd.status & MGSTAT_FLAG_TOO_DARK)
        str += (jcd.status & MGSTAT_FLAG_LM_MAPPED) ? "G-D" : "DA";
      else if (jcd.status & MGSTAT_FLAG_ACQ_DONE)
        str += "AQ";
      else if (jcd.status & MGSTAT_FLAG_MMM_DONE)
        str += (jcd.status & MGSTAT_FLAG_FAILED) ? "M-F" : "MM";
      else if (jcd.status & MGSTAT_FLAG_LM_MAPPED)
        str += (jcd.status & MGSTAT_FLAG_FAILED) ? "G-F" : "GR";
      else if (jcd.status & MGSTAT_FLAG_FAILED)
        str += "FL";

      SetDlgItemText(IDC_STAT_MULGRID_STATUS1 + dlin, str);
      return;
    }
  }
}

/*
 * Routine that can be called whenever there is a different grid on the stage so it can
 * manage the bolding of the name and mOnStageDlgIndex
 */
void CMultiGridDlg::NewGridOnStage(int jcdInd)
{
  int id, ind, oldInd = mOnStageDlgIndex;
  CEdit *edit;
  JeolCartridgeData jcd;
  if (jcdInd < 0)
    jcdInd = mScope->FindCartridgeAtStage(id);

  // Manipulate font of name string: the ID would be not evident when only one digit AND
  // didn't seem to work, coloring outside the button didn't seem to work
  if (mOnStageDlgIndex >= 0) {
    edit = (CEdit *)GetDlgItem(IDC_EDIT_MULGRID_NAME1 + mOnStageDlgIndex);
    if (edit) {
      edit->SetFont(m_statMgPrefix.GetFont());
      edit->Invalidate();
    }
    mOnStageDlgIndex = -1;
  }
  if (jcdInd < 0) {
    mMGTasks->SetLoadedGridIsAligned(0);
    mSelGridOnStage = false;
    UpdateEnables();
    Invalidate();
    return;
  }
  for (ind = 0; ind < mNumUsedSlots; ind++) {
    if (mDlgIndToJCDindex[ind] == jcdInd) {
      edit = (CEdit *)GetDlgItem(IDC_EDIT_MULGRID_NAME1 + ind);
      if (edit) {
        edit->SetFont(mBoldFont);
        edit->Invalidate();
      }
      mOnStageDlgIndex = ind;
      jcd = mCartInfo->GetAt(jcdInd);
      mSelGridOnStage = jcd.station == JAL_STATION_STAGE;
      if (ind != oldInd)
        mMGTasks->SetLoadedGridIsAligned(0);
      UpdateEnables();
      break;
    }
  }
  DisplayRunOrder();
  Invalidate();
}

/*
 * Set up for a single grid: add the dummy item to catalogue, make it selected & on stage
 */
void CMultiGridDlg::InitForSingleGrid()
{
  JeolCartridgeData jcd;
  mNumUsedSlots = 1;
  jcd.Init();
  jcd.name = jcd.userName = "TheOneGrid";
  jcd.id = jcd.slot = 1;
  jcd.station = JAL_STATION_STAGE;
  mCartInfo->RemoveAll();
  mCartInfo->Add(jcd);
  CButton *button = (CButton *)GetDlgItem(IDC_RADIO_MULGRID_SEL1);
  if (button)
    button->SetCheck(BST_CHECKED);
  EnableDlgItem(IDC_CHECK_MULGRID_RUN1, false);
  button = (CButton *)GetDlgItem(IDC_CHECK_MULGRID_RUN1);
  if (button)
    button->SetCheck(BST_CHECKED);
  EnableDlgItem(IDC_CHECK_MULGRID_RUN1, false);
  mSelectedGrid = 0;
  ReloadTable(1, 1);
  NewGridOnStage(0);
}

/*
 * UPDATING, TRANSFERRING PARAMETERS
 */

/*
 * Update enable status of just about everything; set text for a few items
 */
void CMultiGridDlg::UpdateEnables()
{
  ShortVec dlgInds, slotNums;
  JeolCartridgeData jcd;
  int ind, noTaskList[] = {IDC_CHECK_RUN_LMMS, IDC_RLMM_SEARCH, IDC_RLMM_VIEW, 
    IDC_RLMM_CUR_OR_STATE, IDC_CHECK_SET_LMM_STATE, IDC_CHECK_REMOVE_OBJ, 
    IDC_CHECK_SET_CONDENSER, IDC_RFULL_GRID_MONT, IDC_RNUM_MONT_PIECES, 
    IDC_CHECK_USE_OVERLAP, IDC_CHECK_AUTOCONTOUR, IDC_BUT_SETUP_LMM_MONT,
    IDC_CHECK_TAKE_MMMS, IDC_RMMM_SEARCH, IDC_RMMM_VIEW, IDC_RMMM_CUR_OR_STATE,
    IDC_COMBO_MMM_STATE1, IDC_COMBO_MMM_STATE2, IDC_COMBO_MMM_STATE3, IDC_RFIXED_MONT,
    IDC_COMBO_MMM_STATE4, IDC_RSINGLE_IMAGE, IDC_RPOLYGON_MONT, IDC_BUT_SETUP_MAPPING,
    IDC_CHECK_RUN_FINAL_ACQ, IDC_BUT_SETUP_FINAL_ACQ, IDC_COMBO_FINAL_STATE1,
    IDC_COMBO_FINAL_STATE2, IDC_COMBO_FINAL_STATE3, IDC_COMBO_FINAL_STATE4,
    IDC_CHECK_MG_REFINE, IDC_RVECTORS_FROM_MAPS, IDC_RVECTORS_FROM_SETTINGS};
  bool locked = mMGTasks->GetNamesLocked() > 0;
  BOOL justTasks = mWinApp->DoingTasks();
  bool suspended = mMGTasks->GetSuspendedMulGrid();
  BOOL tasks = justTasks || suspended || mSettingOrder;
  bool acqStatesEnabled = !tasks && (mSelectedGrid >= 0 || !m_bSetFinalByGrid) && 
    mNumUsedSlots > 0;
  CString filename;
  EnableDlgItem(IDC_BUT_MG_GET_NAMES, (FEIscope || mCartInfo->GetSize() > 0) && !tasks &&
    !locked);
  EnableDlgItem(IDC_BUT_MG_CLEAR, mNumUsedSlots > 0 && !tasks && !locked);
  EnableDlgItem(IDC_BUT_MG_RESET_NAMES, mNumUsedSlots > 0 && !tasks && !locked);
  EnableDlgItem(IDC_BUT_MG_INVENTORY, !tasks && (!locked || JEOLscope));
  EnableDlgItem(IDC_BUT_SET_CURRENT_DIR, !tasks && !locked);
  EnableDlgItem(IDC_CHECK_MG_USE_SUBDIRS, !tasks && !locked);
  EnableDlgItem(IDC_CHECK_MG_APPEND_NAME, !tasks && !locked);
  EnableDlgItem(IDC_EDIT_MG_PREFIX, !tasks && !locked);
  EnableDlgItem(IDC_CHECK_TOGGLE_ALL, mNumUsedSlots > 0 && !tasks && !mSettingOrder);
  EnableDlgItem(IDC_BUT_SET_ORDER, mNumUsedSlots > 0 && !justTasks && !suspended);
  EnableDlgItem(IDC_BUT_RESET_ORDER, mNumUsedSlots > 0 && !justTasks && !suspended &&
    mMGTasks->GetUseCustomOrder());
  EnableDlgItem(IDC_EDIT_GRID_NOTE, mNumUsedSlots > 0 && !justTasks && locked && 
    mSelectedGrid >= 0);
  EnableDlgItem(IDC_BUT_LOAD_GRID, mNumUsedSlots > 0 && !tasks && mSelectedGrid >= 0 &&
    !mSelGridOnStage);
  if (!tasks)
    filename = NavFileIfExistsAndNotLoaded();
  EnableDlgItem(IDC_BUT_OPEN_NAV, !tasks && !filename.IsEmpty());
  EnableDlgItem(IDC_BUT_SET_GRID_TYPE, mNumUsedSlots > 0 && !tasks && mSelectedGrid >= 0);
  if (mNumUsedSlots > 0 && !tasks && mSelectedGrid >= 0 && mSelGridOnStage) {
    jcd = FindCartDataForDlgIndex(mSelectedGrid);
    EnableDlgItem(IDC_BUT_REALIGN_TO_GRID_MAP, 
      jcd.slot >= 0 && (jcd.status & MGSTAT_FLAG_LM_MAPPED));
  } else
    EnableDlgItem(IDC_BUT_REALIGN_TO_GRID_MAP, false);
  EnableDlgItem(IDC_RREFINE_SEARCH, m_bRefineRealign && !tasks);
  EnableDlgItem(IDC_RREFINE_VIEW, m_bRefineRealign && !tasks);
  EnableDlgItem(IDC_RREFINE_STATE, m_bRefineRealign && !tasks);
  EnableDlgItem(IDC_COMBO_REFINE_STATE, m_bRefineRealign && m_iRefineRealign == 2 &&
    !tasks);
  EnableDlgItem(IDC_COMBO_LMM_STATE, m_bSetLMMstate && !tasks);
  EnableDlgItem(IDC_EDIT_CONDENSER, m_bSetCondenser && !tasks);
  EnableDlgItem(IDC_RSET_C1_APERTURE, m_bSetCondenser && !tasks);
  EnableDlgItem(IDC_RSET_C2_APERTURE, m_bSetCondenser && !tasks);
  EnableDlgItem(IDC_EDIT_MONT_XPCS, m_iLMMmontType > 0 && !tasks);
  EnableDlgItem(IDC_EDIT_MONT_NUM_YPCS, m_iLMMmontType > 0 && !tasks);
  EnableDlgItem(IDC_BUT_SETUP_AUTOCONT2, m_bAutocontour && !tasks);
  EnableDlgItem(IDC_BUT_SETUP_POLYMONT, m_iSingleVsPolyMont > 0 && !tasks);
  EnableDlgItem(IDC_BUT_START_RUN, ((m_bTakeLMMs || m_bTakeMMMs || m_bRunFinalAcq) &&
    !tasks && GetListOfGridsToRun(dlgInds, slotNums) > 0 && 
    (!mSingleGridMode || !m_strPrefix.IsEmpty())) || (suspended && !justTasks));
  SetDlgItemText(IDC_BUT_START_RUN, suspended ? "End Run" : "Start Run");
  SetDlgItemText(IDC_BUT_RUN_UNDONE, suspended ? "Resume Run" : "Run Undone");
  EnableDlgItem(IDC_BUT_RUN_UNDONE, (mEnableRunUndone || suspended) && !justTasks && 
    !mSettingOrder);
  EnableDlgItem(IDC_EDIT_OVERLAP, m_bUseMontOverlap && !tasks);
  for (ind = 0; ind < sizeof(noTaskList) / sizeof(int); ind++)
    EnableDlgItem(noTaskList[ind], !tasks);
  for (ind = 0; ind < mNumUsedSlots; ind++) {
    jcd = FindCartDataForDlgIndex(ind);
    EnableDlgItem(IDC_EDIT_MULGRID_NAME1 + ind, !tasks && 
      (!locked || (jcd.status & MGSTAT_FLAG_NEW_GRID)) && !mSingleGridMode);
    EnableDlgItem(IDC_RADIO_MULGRID_SEL1 + ind, !justTasks && !suspended && 
      !mSingleGridMode);
    EnableDlgItem(IDC_CHECK_MULGRID_RUN1 + ind, !justTasks && !suspended && 
      !mSingleGridMode);
  }
  EnableDlgItem(IDC_COMBO_FINAL_STATE1, acqStatesEnabled);
  EnableDlgItem(IDC_COMBO_FINAL_STATE2, acqStatesEnabled);
  EnableDlgItem(IDC_COMBO_FINAL_STATE3, acqStatesEnabled);
  EnableDlgItem(IDC_COMBO_FINAL_STATE4, acqStatesEnabled);
  EnableDlgItem(IDC_CHECK_SET_FINAL_BY_GRID, !tasks && mNumUsedSlots > 0);
  EnableDlgItem(IDC_BUT_REVERT_TO_GLOBAL, 
    !tasks && mSelectedGrid >= 0 && m_bSetFinalByGrid && mNumUsedSlots > 0);
}

/*
 * For the first step being done, checks if there are some done and some not done
 */
void CMultiGridDlg::CheckIfAnyUndoneToRun()
{
  int grid, numDone = 0, numRun = 0, flag = 0;
  ShortVec jcdInds, slotNums;
  JeolCartridgeData jcd;
  mEnableRunUndone = false;
  if (m_bTakeLMMs)
    flag = MGSTAT_FLAG_LM_MAPPED;
  else if (m_bTakeMMMs)
    flag = MGSTAT_FLAG_MMM_DONE;
  else if (m_butRunFinalAcq)
    flag = MGSTAT_FLAG_ACQ_DONE;
  if (flag) {
    numRun = GetListOfGridsToRun(jcdInds, slotNums);
    for (grid = 0; grid < numRun; grid++) {
      jcd = mCartInfo->GetAt(jcdInds[grid]);
      if (jcd.status & flag)
        numDone++;
    }
    mEnableRunUndone = numRun > 0 && numDone > 0 && numDone < numRun;
  }
}

// Get the name of the frame location option set right
void CMultiGridDlg::ManageFrameDirOption()
{
  SetDlgItemText(IDC_CHECK_FRAMES_UNDER_SESSION, m_bUseSubdirs ?
    "Make frame directories under grid directories" :
    "Make frame directories under session directory");
}

// Copy in master params and move them to dialog
void CMultiGridDlg::UpdateSettings()
{
  mParams = *mMasterParams;
  ParamsToDialog();
  ManageRefineFOV();
  UpdateData(false);
}

/*
 * Update the current directory with the program-wide current dir or the locked-in working
 * directory
 */
void CMultiGridDlg::UpdateCurrentDir()
{
  CRect rect;
  CDC *pDC = m_statCurrentDir.GetDC();
  CString file = mMGTasks->GetNamesLocked() ?
    mMGTasks->GetWorkingDir() : mWinApp->mDocWnd->GetInitialDir();
  int maxWidth;
  CString current = mMGTasks->GetNamesLocked() ? "Session" : "Current";
  m_statCurrentDir.GetClientRect(rect);
  maxWidth = 18 * rect.Width() / 20;
  m_strCurrentDir = current + " dir: " + file;
  while ((pDC->GetTextExtent(m_strCurrentDir)).cx > maxWidth && file.GetLength() > 5) {
    file = file.Mid(1);
    m_strCurrentDir = current + " dir: ..." + file;
  }
  UpdateData(false);
}

void CMultiGridDlg::ManageInventory(int locked)
{
  SetDlgItemText(IDC_BUT_MG_INVENTORY, (JEOLscope && locked > 0) ? 
    "Refresh List" : "Run Inventory");
}

/*
 * Adjust panel display after reloading IDs to drop in lower part of table
 */
void CMultiGridDlg::ManagePanels()
{
  BOOL states[MAX_MULGRID_PANELS];
  int ind;
  UINT singleDrops[] = {IDC_BUT_SET_GRID_TYPE, IDC_BUT_LOAD_GRID, IDC_BUT_MG_INVENTORY,
    IDC_BUT_MG_GET_NAMES, IDC_CHECK_MG_APPEND_NAME, IDC_BUT_MG_RESET_NAMES,
    IDC_BUT_MG_CLEAR, IDC_CHECK_SET_FINAL_BY_GRID, IDC_BUT_REVERT_TO_GLOBAL, 
    IDC_CHECK_TOGGLE_ALL, IDC_BUT_RESET_ORDER, IDC_BUT_SET_ORDER, IDC_STAT_NOTE,
  IDC_EDIT_GRID_NOTE};
  UINT refineIDs[] = {IDC_CHECK_MG_REFINE, IDC_RREFINE_SEARCH, IDC_RREFINE_VIEW, 
    IDC_RREFINE_STATE, IDC_COMBO_REFINE_STATE, IDC_STAT_REFINE_FOV};
  UINT MMMcomboIDs[4] = {IDC_COMBO_MMM_STATE1, IDC_COMBO_MMM_STATE2, IDC_COMBO_MMM_STATE3,
    IDC_COMBO_MMM_STATE4};
  UINT finalComboIDs[4] = {IDC_COMBO_FINAL_STATE1, IDC_COMBO_FINAL_STATE2,
    IDC_COMBO_FINAL_STATE3, IDC_COMBO_FINAL_STATE4};
  for (ind = 0; ind < MAX_MULGRID_PANELS; ind++)
    states[ind] = mPanelStates[ind];
  if (mRightIsOpen <= 0)
    for (ind = 4; ind < 10; ind++)
      states[ind] = false;
  SetDlgItemText(IDC_BUT_CLOSE_RIGHT, mRightIsOpen > 0 ? "Close Right" : "Open Right");
  mIDsToDrop.clear();
  for (int ind = mNumUsedSlots; ind < MAX_DLG_SLOTS; ind++) {
    mIDsToDrop.push_back(IDC_CHECK_MULGRID_RUN1 + ind);
    mIDsToDrop.push_back(IDC_RADIO_MULGRID_SEL1 + ind);
    mIDsToDrop.push_back(IDC_EDIT_MULGRID_NAME1 + ind);
    mIDsToDrop.push_back(IDC_STAT_MULGRID_STATUS1 + ind);
  }
  DropComboBoxes(mNumMMMcombos, &MMMcomboIDs[0], IDC_STAT_SET_MMM_STATES);
  DropComboBoxes(mNumFinalCombos, &finalComboIDs[0], IDC_STAT_SET_FINAL_STATES);
  if (mDropFrameOption)
    mIDsToDrop.push_back(IDC_CHECK_FRAMES_UNDER_SESSION);
  if (mSingleGridMode) {
    for (ind = 0; ind < sizeof(singleDrops) / sizeof(UINT); ind++)
      mIDsToDrop.push_back(singleDrops[ind]);
  }
  if (!mMGTasks->GetShowRefineAfterRealign() && !mMGTasks->GetSkipGridRealign()) {
    for (ind = 0; ind < sizeof(refineIDs) / sizeof(UINT); ind++)
      mIDsToDrop.push_back(refineIDs[ind]);
  }
  if (mMGTasks->GetUseTwoJeolCondAp()) {
    mIDsToDrop.push_back(IDC_STAT_CONDENSER);
  } else {
    mIDsToDrop.push_back(IDC_RSET_C1_APERTURE);
    mIDsToDrop.push_back(IDC_RSET_C2_APERTURE);
  }
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
}

/*
 * Copy parameters from the local structure to the dialog
 */
void CMultiGridDlg::ParamsToDialog()
{
  BOOL locked = mMGTasks->GetNamesLocked() > 0;
  m_bAppendNames = locked ? mMGTasks->GetAppendNames() : mParams.appendNames;
  m_bUseSubdirs = locked ? mMGTasks->GetUseSubdirs() : mParams.useSubdirectory;
  m_bRefineRealign = mParams.refineAfterRealign;
  m_iRefineRealign = mParams.refineImageType;
  m_bSetLMMstate = mParams.setLMMstate;
  m_iLMMacquireType = mParams.LMMstateType;
  m_bRemoveObjective = mParams.removeObjectiveAp;
  m_bSetCondenser = mParams.setCondenserAp;
  m_iCondenserSize = mParams.condenserApSize;
  m_iC1orC2 = mParams.C1orC2condenserAp;
  m_iLMMmontType = mParams.LMMmontType;
  m_iXpieces = mParams.LMMnumXpieces;
  m_iYpieces = mParams.LMMnumYpieces;
  m_bUseMontOverlap = mParams.setLMMoverlap;
  m_iMontOverlap = mParams.LMMoverlapPct;
  m_bAutocontour = mParams.autocontour;
  m_iMMMacquireType = mParams.MMMstateType;
  m_bTakeLMMs = mParams.acquireLMMs;
  m_bTakeMMMs = mParams.acquireMMMs;
  m_bRunFinalAcq = mParams.runFinalAcq;
  m_iSingleVsPolyMont = mParams.MMMimageType;
  m_bFramesUnderSession = mParams.framesUnderSession;
  m_bScriptAtEnd = mParams.runMacroAfterLMM;
  m_iVectorSource = mParams.msVectorSource;
  mMacNumAtEnd = mParams.macroToRun;
  m_comboEndScript.SetCurSel(mMacNumAtEnd);
  m_comboEndScript.EnableWindow(m_bScriptAtEnd);
  SetAllComboBoxesFromNameOrNum();
  ManageFrameDirOption();
  UpdateData(false);
}

/*
 * Unload the dialog into the local parameters
 */
void CMultiGridDlg::DialogToParams()
{
  UPDATE_DATA_TRUE;
  if (!mMGTasks->GetNamesLocked()) {
    mParams.appendNames = m_bAppendNames;
    mParams.useSubdirectory = m_bUseSubdirs;
  }
  mParams.setLMMstate = m_bSetLMMstate;
  mParams.LMMstateType = m_iLMMacquireType;
  GetStateFromComboBox(m_comboLMMstate, mParams.LMMstateNum, mParams.LMMstateName, 0);
  mParams.removeObjectiveAp = m_bRemoveObjective;
  mParams.setCondenserAp = m_bSetCondenser;
  mParams.condenserApSize = m_iCondenserSize;
  mParams.C1orC2condenserAp = m_iC1orC2;
  mParams.LMMmontType = m_iLMMmontType;
  mParams.LMMnumXpieces = m_iXpieces;
  mParams.LMMnumYpieces = m_iYpieces;
  mParams.setLMMoverlap = m_bUseMontOverlap;
  mParams.LMMoverlapPct = m_iMontOverlap;
  mParams.autocontour = m_bAutocontour;
  mParams.MMMstateType = m_iMMMacquireType;
  mParams.acquireLMMs = m_bTakeLMMs;
  mParams.acquireMMMs = m_bTakeMMMs;
  mParams.runFinalAcq = m_bRunFinalAcq;
  mParams.MMMimageType = m_iSingleVsPolyMont;
  mParams.framesUnderSession = m_bFramesUnderSession;
  mParams.runMacroAfterLMM = m_bScriptAtEnd;
  mParams.msVectorSource = m_iVectorSource;
  mParams.macroToRun = mMacNumAtEnd;
  mParams.refineAfterRealign = m_bRefineRealign;
  mParams.refineImageType = m_iRefineRealign;
  GetStateFromComboBox(m_comboRefineState, mParams.refineStateNum, 
    mParams.refineStateName, 0);
  GetStateFromComboBox(m_comboMMMstate1, mParams.MMMstateNums[0],
    mParams.MMMstateNames[0], 1);
  GetStateFromComboBox(m_comboMMMstate2, mParams.MMMstateNums[1],
    mParams.MMMstateNames[1], 1);
  GetStateFromComboBox(m_comboMMMstate3, mParams.MMMstateNums[2],
    mParams.MMMstateNames[2], 1);
  GetStateFromComboBox(m_comboMMMstate4, mParams.MMMstateNums[3],
    mParams.MMMstateNames[3], 1);
  if (!m_bSetFinalByGrid) {
    GetFinalStateFomCombos(&mParams.finalStateNums[0], &mParams.finalStateNames[0], -1);
  }
}

/*
 * Get final states from combo boxes into param or grid jcd arrays
 */
void CMultiGridDlg::GetFinalStateFomCombos(int *stateNums, CString *stateNames, 
  int skipInd)
{
  if (skipInd != 0)
    GetStateFromComboBox(m_comboFinalState1, stateNums[0], stateNames[0], 1);
  if (skipInd != 1)
    GetStateFromComboBox(m_comboFinalState2, stateNums[1], stateNames[1], 1);
  if (skipInd != 2)
    GetStateFromComboBox(m_comboFinalState3, stateNums[2], stateNames[2], 1);
  if (skipInd != 3)
    GetStateFromComboBox(m_comboFinalState4, stateNums[3], stateNames[3], 1);
}

/*
 * Sync: unload and copy to master for use elsewhere
 */
void CMultiGridDlg::SyncToMasterParams()
{
  UPDATE_DATA_TRUE;
  DialogToParams();
  *mMasterParams = mParams;
}

/*
 * MANAGING COMBO BOXES AND STATES
 */

/*
 * Sets the selection in all the combo boxes
 */
void CMultiGridDlg::SetAllComboBoxesFromNameOrNum()
{
  SetComboBoxFromNameOrNum(m_comboRefineState, mParams.refineStateNum, 
    mParams.refineStateName, 0);
  SetComboBoxFromNameOrNum(m_comboLMMstate, mParams.LMMstateNum, mParams.LMMstateName, 0);
  SetComboBoxFromNameOrNum(m_comboMMMstate1, mParams.MMMstateNums[0],
    mParams.MMMstateNames[0], 1);
  SetComboBoxFromNameOrNum(m_comboMMMstate2, mParams.MMMstateNums[1],
    mParams.MMMstateNames[1], 1);
  SetComboBoxFromNameOrNum(m_comboMMMstate3, mParams.MMMstateNums[2],
    mParams.MMMstateNames[2], 1);
  SetComboBoxFromNameOrNum(m_comboMMMstate4, mParams.MMMstateNums[3],
    mParams.MMMstateNames[3], 1);
  SetFinalStateComboBoxes();
}

/*
 * Sets the selection for final acq boxes which can come from individual grids
 */
void CMultiGridDlg::SetFinalStateComboBoxes()
{
  int *stateNums = &mParams.finalStateNums[0];
  int numCombos = mNumFinalCombos;
  CString *stateNames = &mParams.finalStateNames[0];
  if (m_bSetFinalByGrid && mSelectedGrid >= 0) {
    JeolCartridgeData &jcdEl = mCartInfo->ElementAt(mDlgIndToJCDindex[mSelectedGrid]);
    if (jcdEl.separateState) {
      stateNums = &jcdEl.acqStateNums[0];
      stateNames = &jcdEl.acqStateNames[0];
    }
    RepackStatesIfNeeded(mNumFinalCombos, stateNums, stateNames);
    if (numCombos < mNumFinalCombos)
      ManagePanels();
  }
  SetComboBoxFromNameOrNum(m_comboFinalState1, stateNums[0],
    stateNames[0], 1);
  SetComboBoxFromNameOrNum(m_comboFinalState2, stateNums[1],
    stateNames[1], 1);
  SetComboBoxFromNameOrNum(m_comboFinalState3, stateNums[2],
    stateNames[2], 1);
  SetComboBoxFromNameOrNum(m_comboFinalState4, stateNums[3],
    stateNames[3], 1);
}

/*
 * Find the highest state used, repack them if this is more than the number of boxes
 * set by property, and increase that number of boxes if needed
 */
void CMultiGridDlg::RepackStatesIfNeeded(int &numBoxes, int *stateNums,
  CString *stateNames)
{
  int ind, highest = 0, out = 0;
  for (ind = 0; ind < 4; ind++)
    if (stateNums[ind] >= 0)
      highest = ind;
  if (highest >= numBoxes) {
    for (ind = 0; ind < 4; ind++) {
      if (stateNums[ind] >= 0) {
        stateNums[out] = stateNums[ind];
        stateNames[out++] = stateNames[ind];
      }
    }
    for (ind = out; ind < 4; ind++) {
      stateNums[ind] = -1;
      stateNames[ind] = "";
    }
    ACCUM_MAX(numBoxes, out);
    mMGTasks->SetAdocChanged(true);
  }
}

/*
 * Drop the combo boxes above the given number
 */
void CMultiGridDlg::DropComboBoxes(int numBoxes, UINT *comboIDs, UINT statID)
{
  int ind;
  for (ind = numBoxes; ind < 4; ind++)
    mIDsToDrop.push_back(comboIDs[ind]);
  SetDlgItemText(statID, numBoxes > 1 ? "States:" : "State:");
  if (!numBoxes)
    mIDsToDrop.push_back(statID);
}

/*
* Given the nominal number and possible name of a state, find the right state to select
* in the combo box
*/
void CMultiGridDlg::SetComboBoxFromNameOrNum(CComboBox &combo, int &num, CString &name,
  int addForNone)
{
  StateParams *state;
  int ind, selInd = -1, numStates = (int)mStateArray->GetSize();
  if (numStates) {

    // If there is a name, look it up by name and make sure it is unique
    if (!name.IsEmpty()) {
      ind = FindUniqueStateFromName(name);
      selInd = ind < 0 ? -1 : (ind + addForNone);
    }

    // By number - make sure its legal
    if (selInd < 0) {
      selInd = num + addForNone;
      B3DCLAMP(selInd, 0, numStates + addForNone - 1);
    }

    // Given the number now, get the current name - it will pass back into params
    num = selInd - addForNone;
    if (num >= 0) {
      state = mStateArray->GetAt(num);
      name = state->name;
    } else
      name = "";
  } else {

    // No states, set up for base item in box.
    num = -addForNone;
    name = "";
    selInd = addForNone - 1;
  }
  combo.SetCurSel(selInd);
}

/*
* Find all the states that match a stored name and return index if there is just one
*/
int CMultiGridDlg::FindUniqueStateFromName(CString &name)
{
  StateParams *state;
  int ind, numMatch = 0, stateInd = -1;
  for (ind = 0; ind < (int)mStateArray->GetSize(); ind++) {
    state = mStateArray->GetAt(ind);
    if (state->name == name) {
      if (!numMatch)
        stateInd = ind;
      numMatch++;
    }
  }

  return (numMatch == 1) ? stateInd : -1;
}

/*
 * Get selected state index and name from a combo box
 */
void CMultiGridDlg::GetStateFromComboBox(CComboBox &combo, int &num, CString &name, 
  int addForNone)
{
  StateParams *state;
  num = combo.GetCurSel() - addForNone;
  B3DCLAMP(num, -addForNone, (int)mStateArray->GetSize() - 1);
  if (num < 0)
    name = "";
  else if (mStateArray->GetSize() > 0) {
    state = mStateArray->GetAt(num);
    name = state->name;
  }
}

/*
 * Load the current states into the combo box
 */
void CMultiGridDlg::LoadStatesInComboBox(CComboBox &combo, bool addNone)
{
  CString str, fullStr, loadStr;
  StateParams *state;
  CRect rect;
  CDC *pDC = combo.GetDC();
  int ind, jnd, token, minusOne = -1, maxWidth, numStates = (int)mStateArray->GetSize();
  if (!numStates) {
    combo.EnableWindow(false);
    return;
  }
  if (addNone)
    combo.AddString("None");
  for (ind = 0; ind < numStates; ind++) {
    state = mStateArray->GetAt(ind);
    CStateDlg::StateToListString(state, fullStr, " ", ind, &minusOne, 1, false);
    loadStr.Format("%d:", ind + 1);
    for (token = 0; token < 4; token++) {
      jnd = fullStr.Find(" ");
      if ((token % 2) == 0 || state->name.IsEmpty() ||
        (token == 1 && fullStr.Left(jnd) == "0")) {
        loadStr += " " + fullStr.Left(jnd);
        if (token == 2)
          loadStr += "x";
      }
      if (token == 0 && state->montMapConSet)
        loadStr += "M";
      fullStr = fullStr.Mid(jnd + 1);
    }
    if (!state->name.IsEmpty())
      loadStr += ": " + state->name;
    combo.GetClientRect(rect);
    maxWidth = 17 * rect.Width() / 20;
    if (pDC->GetTextExtent(loadStr).cx > maxWidth) {
      while ((pDC->GetTextExtent(loadStr + "...")).cx > maxWidth &&
        loadStr.GetLength() > 5)
        loadStr = loadStr.Left(loadStr.GetLength() - 1);
      loadStr += "...";
    }
    combo.AddString((LPCTSTR)loadStr);
  }
  combo.ReleaseDC(pDC);
  SetDropDownHeight(&combo, B3DMIN(MAX_DROPDOWN_TO_SHOW, numStates));
}

/*
 * Load all the combo boxes
 */
void CMultiGridDlg::LoadAllComboBoxes()
{
  LoadStatesInComboBox(m_comboRefineState, false);
  LoadStatesInComboBox(m_comboLMMstate, false);
  LoadStatesInComboBox(m_comboMMMstate1, true);
  LoadStatesInComboBox(m_comboMMMstate2, true);
  LoadStatesInComboBox(m_comboMMMstate3, true);
  LoadStatesInComboBox(m_comboMMMstate4, true);
  LoadStatesInComboBox(m_comboFinalState1, true);
  LoadStatesInComboBox(m_comboFinalState2, true);
  LoadStatesInComboBox(m_comboFinalState3, true);
  LoadStatesInComboBox(m_comboFinalState4, true);
}

/*
* Check the selection in a combo box against others, make sure it doesn't create an
* illegal situation and revert if it does
*/
void CMultiGridDlg::ProcessMultiStateCombo(CComboBox &combo, int *stateNums,
  CString *stateNames, int index, int gridID)
{
  StateParams *state, *othState;
  int cmb, oth, numLD = 0, numNon = 0, camera = -1, prevNum = stateNums[index];
  CString prevName = stateNames[index];
  CString gridPref;
  bool sameArea = false;
  GetStateFromComboBox(combo, stateNums[index], stateNames[index], 1);
  mWinApp->RestoreViewFocus();
  if (stateNums[index] < 0)
    return;
  if (gridID > 0)
    gridPref.Format("For grid # %d: ", gridID);
  for (cmb = 0; cmb < 4; cmb++) {
    if (stateNums[cmb] >= 0 && stateNums[cmb] < (int)mStateArray->GetSize()) {
      state = mStateArray->GetAt(stateNums[cmb]);
      if (state->lowDose)
        numLD++;
      else
        numNon++;
      if (camera < 0)
        camera = state->camIndex;
      else if (camera != state->camIndex)
        break;
      for (oth = 0; oth < cmb && state->lowDose; oth++) {
        if (stateNums[oth] >= 0 && stateNums[oth] < (int)mStateArray->GetSize()) {
          othState = mStateArray->GetAt(stateNums[oth]);
          if (othState->lowDose == state->lowDose) {
            sameArea = true;
            break;
          }
        }
      }
    }
  }
  if (!(numNon > 1 || (numNon && numLD) || (camera >= 0 && camera != state->camIndex) ||
    sameArea))
    return;
  if (numNon > 1)
    AfxMessageBox(gridPref + "Only one non-Low Dose state can be selected", MB_EXCLAME);
  else if (numNon && numLD)
    AfxMessageBox(gridPref + "You cannot select both Low Dose and non-Low Dose states",
      MB_EXCLAME);
  else if (sameArea)
    AfxMessageBox(gridPref + "You cannot select more than one state in the same Low Dose"
      " area", MB_EXCLAME);
  else
    AfxMessageBox(gridPref + "All selected states for one phase must be for the same "
      "camera", MB_EXCLAME);
  stateNames[index] = prevName;
  stateNums[index] = prevNum;
  SetComboBoxFromNameOrNum(combo, stateNums[index], stateNames[index], 1);
}

/*
 * Set either the combo boxes or separate state flag for grid when show grid states is on
 */
void CMultiGridDlg::HandlePerGridStateChange()
{
  if (mSelectedGrid >= 0) {
    JeolCartridgeData &jcdEl = mCartInfo->ElementAt(mDlgIndToJCDindex[mSelectedGrid]);

    // If it already has states, set the combo boxes from them
    if (jcdEl.separateState) {
      SetFinalStateComboBoxes();
    } else {

      // Otherwise set from current values in combos
      jcdEl.separateState = 1;
      GetFinalStateFomCombos(&jcdEl.acqStateNums[0], &jcdEl.acqStateNames[0], -1);
      SetStatusText(mDlgIndToJCDindex[mSelectedGrid]);
      mMGTasks->SetAdocChanged(true);
    }
  }
  UpdateEnables();
}

/*
 * MESSAGE HANDLERS FOR THE ACTION SECTIONS OF THE DIALOG
 */

// Open-close setup section
void CMultiGridDlg::OnButMgSetup()
{
  mPanelStates[3] = !mPanelStates[3];
  SetDlgItemText(IDC_BUT_MG_SETUP, mPanelStates[3] ? "-" : "+");
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// Open/close low mag map section
void CMultiGridDlg::OnButMgLowMagMaps()
{
  mPanelStates[5] = !mPanelStates[5];
  SetDlgItemText(IDC_BUT_MG_LOW_MAG_MAPS, mPanelStates[5] ? "-" : "+");
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// Open/close medium mag map section
void CMultiGridDlg::OnButMgMediumMagMaps()
{
  mPanelStates[7] = !mPanelStates[7];
  SetDlgItemText(IDC_BUT_MG_MEDIUM_MAG_MAPS, mPanelStates[7] ? "-" : "+");
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// open/close final acquisition section
void CMultiGridDlg::OnButMgFinalAcq()
{
  mPanelStates[9] = !mPanelStates[9];
  SetDlgItemText(IDC_BUT_MG_FINAL_ACQ, mPanelStates[9] ? "-" : "+");
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// Button to open or close right side
void CMultiGridDlg::OnButCloseRight()
{
  mRightIsOpen = mRightIsOpen > 0 ? 0 : 1;
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// Close everything, or reopen to saved state
void CMultiGridDlg::OnButCloseAll()
{
  int ind;
  SetDlgItemText(IDC_BUT_CLOSE_ALL, mClosedAll ? "-" : "+");
  for (ind = 1; ind < MAX_MULGRID_PANELS; ind++) {
    if (mClosedAll) {
      mPanelStates[ind] = mSavedPanelStates[ind];
    } else {
      mSavedPanelStates[ind] = mPanelStates[ind];
      mPanelStates[ind] = false;
    }
  }
  mClosedAll = !mClosedAll;
  ManagePanels();
  mWinApp->RestoreViewFocus();
}

// The actual checkbox for running grid maps
void CMultiGridDlg::OnCheckRunLmms()
{
  UPDATE_DATA_TRUE;
  mPanelStates[5] = m_bTakeLMMs;
  SetDlgItemText(IDC_BUT_MG_LOW_MAG_MAPS, mPanelStates[5] ? "-" : "+");
  m_butTakeLMMs.SetFont(m_bTakeLMMs ? &mBigBoldFont : &mBiggerFont);
  ManagePanels();
  mWinApp->RestoreViewFocus();
  CheckIfAnyUndoneToRun();
  UpdateEnables();
}

// The actual checkbox for running medium mag maps
void CMultiGridDlg::OnCheckTakeMmms()
{
  UPDATE_DATA_TRUE;
  mPanelStates[7] = m_bTakeMMMs;
  SetDlgItemText(IDC_BUT_MG_MEDIUM_MAG_MAPS, mPanelStates[7] ? "-" : "+");
  m_butTakeMMMs.SetFont(m_bTakeMMMs ? &mBigBoldFont : &mBiggerFont);
  ManagePanels();
  mWinApp->RestoreViewFocus();
  CheckIfAnyUndoneToRun();
  UpdateEnables();
}

// The actual checkbox for running final acquisition
void CMultiGridDlg::OnCheckRunFinalAcq()
{
  UPDATE_DATA_TRUE;
  mPanelStates[9] = m_bRunFinalAcq;
  SetDlgItemText(IDC_BUT_MG_FINAL_ACQ, mPanelStates[9] ? "-" : "+");
  m_butRunFinalAcq.SetFont(m_bRunFinalAcq ? &mBigBoldFont : &mBiggerFont);
  ManagePanels();
  mWinApp->RestoreViewFocus();
  CheckIfAnyUndoneToRun();
  UpdateEnables();
}

// Open autocontour setup dialog
void CMultiGridDlg::OnButSetupAutocont()
{
  int ind, *showGroup;
  int *storedGroups = mMGTasks->GetAutoContGroups();
  if (storedGroups) {
    showGroup = mWinApp->mNavHelper->mAutoContouringDlg->GetShowGroup();
    for (ind = 0; ind < MAX_AUTOCONT_GROUPS; ind++)
      showGroup[ind] = storedGroups[ind];
  }
  mWinApp->mNavHelper->OpenAutoContouring(true);
  mWinApp->RestoreViewFocus();
}

// A button for selecting the grid to act on is pressed
void CMultiGridDlg::OnRadioMulgridSelect(UINT nID)
{
  CButton *button;
  JeolCartridgeData jcd;
  CString filename;
  int runInd, numAdd;
  jcd.slot = -1;
  button = (CButton *)GetDlgItem(nID);

  // Make button unchecked or checked (pushed)
  if (mSelectedGrid == nID - IDC_RADIO_MULGRID_SEL1 && !mSettingOrder) {
    if (button)
      button->SetCheck(BST_UNCHECKED);
    mSelectedGrid = -1;
  } else {
    if (button)
      button->SetCheck(BST_CHECKED);
    mSelectedGrid = nID - IDC_RADIO_MULGRID_SEL1;
    jcd = FindCartDataForDlgIndex(mSelectedGrid);
    if (mSettingOrder) {
      runInd = LookupRunOrder(*mCustomRunDlgInds, mSelectedGrid);
      if (runInd >= 0) {
        VEC_REMOVE_AT((*mCustomRunDlgInds), runInd);
        mWasAddedToOrder[mSelectedGrid] = 0;
        numAdd = GetNumAddedToOrder();
        mCustomRunDlgInds->insert(mCustomRunDlgInds->begin() + numAdd, mSelectedGrid);
        mWasAddedToOrder[mSelectedGrid] = 1;
        if (numAdd + 1 == (int)mCustomRunDlgInds->size())
          OnButSetOrder();
      }
      DisplayRunOrder();
    }
  }
  if (m_bSetFinalByGrid)
    HandlePerGridStateChange();
  else
    EnableDlgItem(IDC_BUT_REVERT_TO_GLOBAL, false);
  if (mSelectedGrid < 0)
    m_strNote = "";
  else
    m_strNote = jcd.note;
  EnableDlgItem(IDC_EDIT_GRID_NOTE, mMGTasks->GetNamesLocked() && mSelectedGrid >= 0);
  UpdateData(false);

  // See if it is grid on stage and set load, open and realign buttons
  mSelGridOnStage = jcd.slot >= 0 && jcd.station == JAL_STATION_STAGE;
  EnableDlgItem(IDC_BUT_LOAD_GRID, jcd.slot >= 0 && !mSelGridOnStage);
  EnableDlgItem(IDC_BUT_REALIGN_TO_GRID_MAP, jcd.slot >= 0 && mSelGridOnStage &&
    (jcd.status & MGSTAT_FLAG_LM_MAPPED));
  filename = NavFileIfExistsAndNotLoaded();
  EnableDlgItem(IDC_BUT_OPEN_NAV, !filename.IsEmpty());
  EnableDlgItem(IDC_BUT_SET_GRID_TYPE, jcd.slot >= 0);
  EnableDlgItem(IDC_BUT_OPEN_LOGS, FindLogFiles(NULL, false));
  if (mWinApp->mNavHelper->mMGSettingsDlg)
    mWinApp->mNavHelper->mMGSettingsDlg->SetJcdIndex(mSelectedGrid < 0 ? -1 :
      mDlgIndToJCDindex[mSelectedGrid]);
  if (filename && GetAsyncKeyState(VK_CONTROL) / 2 != 0) {
    mWinApp->mNavigator->LoadNavFile(false, false, &filename);
    UpdateEnables();
  }
  m_statCurrentDir.SetFocus();
  mWinApp->RestoreViewFocus();
}

// A checkbox for running a grid is pressed
void CMultiGridDlg::OnCheckMulgridRun(UINT nID)
{
  int numAdded, runInd, dlgInd = nID - IDC_CHECK_MULGRID_RUN1;
  UPDATE_DATA_TRUE;
  if (mMGTasks->GetUseCustomOrder()) {
    CButton *button = (CButton *)GetDlgItem(nID);
    if (button) {
      if (button->GetCheck() == BST_CHECKED) {

        // Checking button: insert it in custom list if setting, or add to end
        if (mSettingOrder) {
          numAdded = GetNumAddedToOrder();
          mCustomRunDlgInds->insert(mCustomRunDlgInds->begin() + numAdded, dlgInd);
          mWasAddedToOrder[dlgInd] = 1;
        } else {
          mCustomRunDlgInds->push_back(dlgInd);
        }

      } else {

        // Unchecking: find in list and remove
        runInd = LookupRunOrder(*mCustomRunDlgInds, dlgInd);
        if (runInd >= 0)
          VEC_REMOVE_AT((*mCustomRunDlgInds), runInd);
        if (mSettingOrder)
          mWasAddedToOrder[dlgInd] = 0;
      }
    }
  }
  CheckIfAnyUndoneToRun();
  UpdateEnables();
  DisplayRunOrder();
  mWinApp->RestoreViewFocus();
}

// Toggle all run checkboxes on or off
void CMultiGridDlg::OnCheckToggleAll()
{
  CButton *button;
  int loop, ind;
  UPDATE_DATA_TRUE;
  for (loop = 0; loop < mNumUsedSlots; loop++) {
    ind = FEIscope ? mNumUsedSlots - (loop + 1) : loop;
    button = (CButton *)GetDlgItem(IDC_CHECK_MULGRID_RUN1 + ind);
    if (button) {
      if (m_bToggleAll && mMGTasks->GetUseCustomOrder() &&
        LookupRunOrder(*mCustomRunDlgInds, ind) < 0)
        mCustomRunDlgInds->push_back(ind);
      button->SetCheck(m_bToggleAll ? BST_CHECKED : BST_UNCHECKED);
    }
  }
  if (!m_bToggleAll)
    mCustomRunDlgInds->clear();
  CheckIfAnyUndoneToRun();
  UpdateEnables();
  DisplayRunOrder();
  mWinApp->RestoreViewFocus();
}

// Start procedure to set order, or end it
void CMultiGridDlg::OnButSetOrder()
{
  mSettingOrder = !mSettingOrder;
  SetDlgItemText(IDC_BUT_SET_ORDER, mSettingOrder ? "Stop Setting" : "Set Order");
  if (mSettingOrder) {
    if (!mMGTasks->GetUseCustomOrder()) {
      mMGTasks->SetUseCustomOrder(true);
      DefaultRunDlgIndexes(*mCustomRunDlgInds);
    }
    CLEAR_RESIZE(mWasAddedToOrder, short, mNumUsedSlots);
  }
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

// Reset custom order
void CMultiGridDlg::OnButResetOrder()
{
  if (mSettingOrder)
    OnButSetOrder();
  mMGTasks->SetUseCustomOrder(false);
  DisplayRunOrder();
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

// Count the nunber of grids added to the order list
int CMultiGridDlg::GetNumAddedToOrder()
{
  int num = 0, ind;
  if (!mSettingOrder)
    return (int)mCustomRunDlgInds->size();
  for (ind = 0; ind < mNumUsedSlots; ind++)
    if (mWasAddedToOrder[ind])
      num++;
  return num;
}

// The note has lost focus, process the change
void CMultiGridDlg::OnEnKillfocusEditGridNote()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  if (mSelectedGrid < 0)
    return;
  int jcdInd = mDlgIndToJCDindex[mSelectedGrid];
  JeolCartridgeData &jcdEl = mCartInfo->ElementAt(jcdInd);
  jcdEl.note = m_strNote;
  mMGTasks->SetAdocChanged(true);
  SetDlgItemText(IDC_EDIT_MULGRID_NAME1 + mSelectedGrid, UserNameWithNote(jcdEl.userName,
    m_strNote));
}

// Open navigator file
void CMultiGridDlg::OnButOpenNav()
{
  CFileStatus status;
  CString filename = NavFileIfExistsAndNotLoaded();
  if (!filename.IsEmpty()) {
    mWinApp->mNavigator->LoadNavFile(false, false, &filename);
    mMGTasks->ApplyVectorsIfSavedAndOK(mDlgIndToJCDindex[mSelectedGrid]);
  }
  mWinApp->RestoreViewFocus();
  UpdateEnables();
}

// Open logs if any exist
void CMultiGridDlg::OnButOpenLogs()
{
  std::vector<std::string> strList;
  if (!FindLogFiles(&strList, true))
    return;
  for (int ind = 0; ind < (int)strList.size(); ind++)
    mWinApp->OpenSecondaryLog(strList[ind].c_str(), true);
}

// Find all the log files for the selected grid and return in list if returnAll is true;
// just return true if any exist if returnAll is false; strList can be NULL in this case
bool CMultiGridDlg::FindLogFiles(std::vector<std::string> *strList, bool returnAll)
{
  const char *suffixes[2] = {"*.log", "*.rtf"};
  HANDLE hFind = NULL;
  WIN32_FIND_DATA FindFileData;
  int suff;
  CString filename, path, name;

  // Loop on .log and .rtf files
  if (mSelectedGrid < 0)
    return false;
  for (suff = 0; suff < 2; suff++) {
    filename = mMGTasks->FullGridFilePath(-mDlgIndToJCDindex[mSelectedGrid] - 1,
      suffixes[suff]);
    UtilSplitPath(filename, path, name);
    path += "\\";
    hFind = FindFirstFile((LPCTSTR)filename, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
      continue;
    do {
      if (!returnAll) {
        FindClose(hFind);
        return true;
      }
      strList->push_back((LPCTSTR)(path + FindFileData.cFileName));
    } while (FindNextFile(hFind, &FindFileData) != 0);
  }
  FindClose(hFind);
  return returnAll && strList->size() > 0;
}

// Get name of nav file if it is needed for loading
CString CMultiGridDlg::NavFileIfExistsAndNotLoaded()
{
  CString filename;
  if (mSelectedGrid  < 0 || !mMGTasks->GetNamesLocked())
    return filename;
  filename = mMGTasks->FullGridFilePath(-mDlgIndToJCDindex[mSelectedGrid] - 1, ".nav");
  if (UtilFileExists(filename) && filename != mWinApp->mNavigator->GetCurrentNavFile())
    return filename;
  return "";
}

// Load selected grid
void CMultiGridDlg::OnButLoadGrid()
{
  JeolCartridgeData jcd;
  mWinApp->RestoreViewFocus();
  if (mSelectedGrid < 0)
    return;
  int jcdInd = mDlgIndToJCDindex[mSelectedGrid];
  jcd = mCartInfo->GetAt(jcdInd);
  mMGTasks->LoadOrUnloadGrid(FEIscope ? jcd.slot : jcdInd + 1, MG_USER_LOAD);
}

// Realign to the grid map
void CMultiGridDlg::OnButRealignToGridMap()
{
  CString errStr;
  int offsetTypes, ldArea, setNum, numXpc, numYpc;
  StateParams *state;
  float FOV;
  CMapDrawItem *item;
  bool applyMarker = false;
  BaseMarkerShift baseShift;

  mWinApp->RestoreViewFocus();
  if (mScope->GetColumnValvesOpen() < 1)
    mScope->SetColumnValvesOpen(true);
  if (mMGTasks->GetSkipGridRealign() && m_bRefineRealign) {

    // Run refine procedure if that is chosen
    DialogToParams();
    if (CheckRefineOptions(FOV, ldArea, setNum, &state, numXpc, numYpc, errStr)) {
      AfxMessageBox("Cannot refine alignment: " + errStr, MB_EXCLAME);
      return;
    }

    // Need to get Nav loaded, so use this call here
    if (mMGTasks->PrepareAlignToGridMap(mDlgIndToJCDindex[mSelectedGrid], true, &item,
      errStr)) {
      AfxMessageBox("Cannot refine alignment: " + errStr, MB_EXCLAME);
      return;
    }

    // If no marker shift has been applied to map yet
    if (!item->mMarkerShiftX && !item->mMarkerShiftY) {
      offsetTypes = mMGTasks->CheckShiftsForLMmode(mMGTasks->GetLMMmagIndex());

      // Message was already issued on error
      if (offsetTypes < 0)
        return;
      if (offsetTypes == 1)
        applyMarker = true;

      // Ask whether to apply marker shift if both kinds exist
      if (offsetTypes == 3 && AfxMessageBox(
        "There is both a shift offset for the Low Dose area\n"
        "used for the grid map, and a marker shift from the grid map\n"
        "magnification that has not been applied to this map yet.\n\n"
        "Do you want to apply the marker shift?\n"
        "(Answer No if the shift offset supercedes the marker shift;\n"
        "or Yes if the marker shift is needed even with the shift offset.)",
        MB_QUESTION) == IDYES)
        applyMarker = true;
      if (applyMarker) {
        mMGTasks->MarkerShiftIndexForMag(item->mMapMagInd, baseShift);
        mWinApp->mNavigator->ShiftItemsAtRegistration(baseShift.shiftX,
          baseShift.shiftY, item->mRegistration, 1);
        PrintfToLog("Applied marker shift of %.2f, %.2f to Navigator items for this grid",
          baseShift.shiftX, baseShift.shiftY);
      }
    }
    if (mMGTasks->RefineGridMapAlignment(mDlgIndToJCDindex[mSelectedGrid],
      mParams.refineStateNum, true, numXpc, numYpc, errStr))
      AfxMessageBox("Failed to refine grid map alignment:\n" + errStr, MB_EXCLAME);

    // Otherwise regular realign
  } else if (mMGTasks->RealignToGridMap(mDlgIndToJCDindex[mSelectedGrid], true, true, 
    errStr) > 0) {
      AfxMessageBox("Failed to realign to grid map: " + errStr, MB_EXCLAME);
  }
}

// Set some individual grid processing parameters
void CMultiGridDlg::OnButSetGridType()
{
  mWinApp->RestoreViewFocus();
  if (mSelectedGrid < 0)
    return;
  mWinApp->mNavHelper->OpenMGSettingsDlg(mDlgIndToJCDindex[mSelectedGrid]);
}

// Radio buttons for what to use for imaging LMM
void CMultiGridDlg::OnRadioLMMSearch()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Checkbox to enable setting/using LMM state
void CMultiGridDlg::OnCheckSetLmmState()
{
  UPDATE_DATA_TRUE;
  EnableDlgItem(IDC_COMBO_LMM_STATE, m_bSetLMMstate);
  mWinApp->RestoreViewFocus();
}

// Checkbox to enable setting condenser
void CMultiGridDlg::OnCheckSetCondenser()
{
  UPDATE_DATA_TRUE;
  UpdateEnables();
  mWinApp->RestoreViewFocus();
}

// Radio button to choose between C1 and C2 on JEOL with both
void CMultiGridDlg::OnRsetC1Aperture()
{
  UPDATE_DATA_TRUE;
  ManageCondenserUnits();
  mWinApp->RestoreViewFocus();
}

// New condenser aperture size entered
void CMultiGridDlg::OnEnKillfocusEditCondenser()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Select autocontouring 
void CMultiGridDlg::OnCheckAutocontour()
{
  UPDATE_DATA_TRUE;
  EnableDlgItem(IDC_BUT_SETUP_AUTOCONT2, m_bAutocontour);
  mWinApp->RestoreViewFocus();
}

// Enable running a script at the end
void CMultiGridDlg::OnCheckScriptAtEnd()
{
  UPDATE_DATA_TRUE;
  m_comboEndScript.EnableWindow(m_bScriptAtEnd);
  mWinApp->RestoreViewFocus();
}

// User selects a script
void CMultiGridDlg::OnSelendokComboEndScript()
{
  mWinApp->RestoreViewFocus();
  mMacNumAtEnd = m_comboEndScript.GetCurSel();
}

// Button to set up the LM montage
void CMultiGridDlg::OnButSetupLmmMont()
{
  UPDATE_DATA_TRUE;
  DoSetupLMMmont(false);
  mWinApp->RestoreViewFocus();
}

// Button to use specific overlap
void CMultiGridDlg::OnCheckUseOverlap()
{
  UPDATE_DATA_TRUE;
  EnableDlgItem(IDC_EDIT_OVERLAP, m_bUseMontOverlap);
  mWinApp->RestoreViewFocus();
}

// New percent overlap entered
void CMultiGridDlg::OnEnKillfocusEditOverlap()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// New # of X pieces entered
void CMultiGridDlg::OnEnKillfocusEditMontXpcs()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// New # of Y pieces entered
void CMultiGridDlg::OnEnKillfocusEditMontNumYpcs()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

// Radio buttons to select what kind of montage to take
void CMultiGridDlg::OnRfullGridMont()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
  UpdateEnables();
}

// New state for refine after realign
void CMultiGridDlg::OnSelendokComboRefineState()
{
  mWinApp->RestoreViewFocus();
  GetStateFromComboBox(m_comboRefineState, mParams.refineStateNum, 
    mParams.refineStateName, 0);
  ManageRefineFOV();
}

// New state selected in LMM combo box
void CMultiGridDlg::OnSelendokComboLmmState()
{
  mWinApp->RestoreViewFocus();
  GetStateFromComboBox(m_comboLMMstate, mParams.LMMstateNum, mParams.LMMstateName, 0);
}

// New state selected in MMM combo boxes
void CMultiGridDlg::OnSelendokComboMmmState1()
{
  ProcessMultiStateCombo(m_comboMMMstate1, mParams.MMMstateNums,
    mParams.MMMstateNames, 0, -1);
}


void CMultiGridDlg::OnSelendokComboMmmState2()
{
  ProcessMultiStateCombo(m_comboMMMstate2, mParams.MMMstateNums,
    mParams.MMMstateNames, 1, -1);
}


void CMultiGridDlg::OnSelendokComboMmmState3()
{
  ProcessMultiStateCombo(m_comboMMMstate3, mParams.MMMstateNums,
    mParams.MMMstateNames, 2, -1);
}

void CMultiGridDlg::OnSelendokComboMmmState4()
{
  ProcessMultiStateCombo(m_comboMMMstate4, mParams.MMMstateNums,
    mParams.MMMstateNames, 3, -1);
}

// New state selected in final acquire combo boxes
void CMultiGridDlg::OnSelendokComboFinalState1()
{
  ProcessFinalStateCombo(m_comboFinalState1, 0);
}


void CMultiGridDlg::OnSelendokComboFinalState2()
{
  ProcessFinalStateCombo(m_comboFinalState2, 1);
}


void CMultiGridDlg::OnSelendokComboFinalState3()
{
  ProcessFinalStateCombo(m_comboFinalState3, 2);
}

void CMultiGridDlg::OnSelendokComboFinalState4()
{
  ProcessFinalStateCombo(m_comboFinalState4, 3);
}

/*
 * For a final state combo box change, need to set arrays for global or per grid values
 */
void CMultiGridDlg::ProcessFinalStateCombo(CComboBox &combo, int index)
{
  int *stateNums = mParams.finalStateNums;
  CString *stateNames = mParams.finalStateNames;
  JeolCartridgeData &jcdEl = mCartInfo->ElementAt(
    mDlgIndToJCDindex[B3DMAX(0, mSelectedGrid)]);
  int gridID = -1;
  if (mSelectedGrid >= 0 && m_bSetFinalByGrid) {
    stateNums = jcdEl.acqStateNums;
    stateNames = jcdEl.acqStateNames;

    // If separate state was not on before, turn it on, get all the states but this one
    // from the other combos, and take this (previous) state from global params
    // This should happen only after a a revert
    if (!jcdEl.separateState) {
      jcdEl.separateState = 1;
      GetFinalStateFomCombos(stateNums, stateNames, index);
      jcdEl.acqStateNums[index] = mParams.finalStateNums[index];
      jcdEl.acqStateNames[index] = mParams.finalStateNames[index];
      SetStatusText(mDlgIndToJCDindex[mSelectedGrid]);
    }
  }
  ProcessMultiStateCombo(combo, stateNums, stateNames, index, gridID);
  mMGTasks->SetAdocChanged(true);
}

// Check box to set state grid by grid
void CMultiGridDlg::OnCheckSetFinalByGrid()
{
  UPDATE_DATA_TRUE;
  HandlePerGridStateChange();
  mWinApp->RestoreViewFocus();
}

// Button to revert state of current grid to global
void CMultiGridDlg::OnButRevertToGlobal()
{
  mWinApp->RestoreViewFocus();
  if (mSelectedGrid < 0)
    return;
  JeolCartridgeData &jcdEl = mCartInfo->ElementAt(mDlgIndToJCDindex[mSelectedGrid]);
  jcdEl.separateState = 0;
  for (int ind = 0; ind < 4; ind++) {
    jcdEl.acqStateNums[ind] = 0;
    jcdEl.acqStateNames[ind] = "";
  }
  SetFinalStateComboBoxes();
  SetStatusText(mDlgIndToJCDindex[mSelectedGrid]);
  EnableDlgItem(IDC_BUT_REVERT_TO_GLOBAL, false);
  mMGTasks->SetAdocChanged(true);
}

// Check for frame dir under session/grid dir
void CMultiGridDlg::OnCheckFramesUnderSession()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Radio buttons for source of multishot vectors
void CMultiGridDlg::OnRvectorsFromMaps()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

// Button to open nav acquire for final acquisition params
void CMultiGridDlg::OnButSetupFinalAcq()
{
  mWinApp->mNavigator->AcquireAreas(NAVACQ_SRC_MG_SET_ACQ, false, false);
  mWinApp->RestoreViewFocus();
}

// Button to open nav acquire for mapping params
void CMultiGridDlg::OnButSetupMapping()
{
  mWinApp->mNavigator->AcquireAreas(NAVACQ_SRC_MG_SET_MMM, false, false);
  mWinApp->RestoreViewFocus();
}

// Setup button for MMM montage, polygon or fixed
void CMultiGridDlg::OnButSetupPolymont()
{
  SetupMMMacquire(false);
  mWinApp->RestoreViewFocus();
}

// Radio buttons for single image, polygon, or fixed montage
void CMultiGridDlg::OnRsingleImage()
{
  UPDATE_DATA_TRUE;
  EnableDlgItem(IDC_BUT_SETUP_POLYMONT, m_iSingleVsPolyMont > 0);
  mWinApp->RestoreViewFocus();
}

// Radio buttons for image acquisition type of MMM
void CMultiGridDlg::OnRmmmSearch()
{
  UPDATE_DATA_TRUE;
  mWinApp->RestoreViewFocus();
}

/*
 * SETUP AND SUPPORT FUNCTIONS
 */

/*
 * Set up the grid montage initially or when going to run
 */
int CMultiGridDlg::DoSetupLMMmont(bool skipDlg)
{
  BOOL lowDose = mWinApp->LowDoseMode();
  bool refind = false;
  int ind, consNum, ldArea = -1, stateMag = 0, camera = mWinApp->GetCurrentCamera();
  int err;
  float overlapFrac;
  LowDoseParams *ldp;
  ControlSet *conSet;
  MontParam *montP = mMGTasks->GetGridMontParam();
  CString str;
  StateParams *state;
  CMontageSetupDlg montDlg;

  if (m_bUseMontOverlap) {
    montP->minMicronsOverlap = 1.f;
    montP->minOverlapFactor = (float)(m_iMontOverlap / 100.);
  }
  
  // If  there is a state, first make sure its still where it should be
  if (m_bSetLMMstate) {
    state = GetOrRefindState(mParams.LMMstateNum, mParams.LMMstateName, 
      "the state for grid mapping", m_comboLMMstate);
    if (!state)
      return 1;
 
    // Set low dose and its area from state
    lowDose = state->lowDose != 0;
    ldArea = -1;
    if (lowDose)
      ldArea = mWinApp->mNavHelper->AreaFromStateLowDoseValue(state, NULL);

    // Insist the state have something to do with the designated acquisition
    if ((m_iLMMacquireType == 0 && ldArea != SEARCH_AREA) ||
      (m_iLMMacquireType == 1 && ldArea != VIEW_CONSET)) {
      str.Format("You selected to acquire with %s but to set a %s.\n"
        "Resolve this discrepancy before setting up the montage",
        m_iLMMacquireType ? "View" : "Search", 
        lowDose ? "state for a different Low Dose area" : "non-Low Dose state");
      AfxMessageBox(str, MB_EXCLAME);
      return 1;
    }

    // Get camera and mag from state either way
    if (lowDose) {
      montP->magIndex = state->ldParams.magIndex;
      montP->useMontMapParams = ldArea == RECORD_CONSET && state->montMapConSet;
    } else {
      montP->magIndex = state->magIndex;
      montP->useMontMapParams = state->montMapConSet;
    } 
    camera = state->camIndex;
    montP->binning = state->binning;
    stateMag = montP->magIndex;
    mLMneedsLowDose = 0;
 
  } else {

    // No state: View or Search means low dose, otherwise current mode determines if LD
    if (m_iLMMacquireType == 0) {
      ldArea = SEARCH_AREA;
      lowDose = true;
    } else if (m_iLMMacquireType == 1) {
      ldArea = VIEW_CONSET;
      lowDose = true;
    } else if (lowDose) {

      // If current mode is LD, 
      ldArea = mScope->GetLowDoseArea();
      if (ldArea < 0) {
        if (montP->useSearchInLowDose)
          ldArea = SEARCH_AREA;
        else if (montP->useViewInLowDose)
          ldArea = VIEW_CONSET;
        else if (montP->useMontMapParams)
          ldArea = RECORD_CONSET;
      }
      if (ldArea < 0) {
        AfxMessageBox("You selected to used the current state and Low Dose\n"
          "mode is on, but there is neither a current low dose area\n"
          "nor do the current montage parameters specify to use View or Search "
          "in Low Dose.\n"
          "Resolve this problem before setting up the montage", MB_EXCLAME);
        return 1;
      }
    } else {

      // Non low dose: get mag index and that is it
      montP->magIndex = mScope->GetMagIndex();
    }

    if (lowDose) {
      ldp = mWinApp->GetLowDoseParams() + ldArea;
      montP->magIndex = ldp->magIndex;
    }

    // Set the binning
    consNum = MontageConSetNum(montP, true, lowDose ? 1 : 0);
    conSet = mWinApp->GetConSets() + consNum;
    montP->binning = conSet->binning;
    mLMneedsLowDose = lowDose ? 1 : -1;
  }

  montP->useViewInLowDose = ldArea == VIEW_CONSET;
  montP->useSearchInLowDose = ldArea == SEARCH_AREA;
  montP->usePrevInLowDose = false;
  montP->useMultiShot = false;
  ind = mWinApp->LookupActiveCamera(camera);
  if (ind < 0) {
    AfxMessageBox("The camera for the state to be set is not in the active list",
      MB_EXCLAME);
    return 1;
  }
  montP->cameraIndex = ind;
  montP->setupInLowDose = lowDose;
  montP->skipOutsidePoly = false;
  montP->skipCorrelations = false;
  montP->warnedConSetBin = true;
  overlapFrac = m_bUseMontOverlap ? (float)(m_iMontOverlap / 100.) : 0.f;

  if (m_iLMMmontType == 0) {

    // Setup full montage
    montP->ignoreSkipList = false;
    err = mWinApp->mNavigator->FullMontage(skipDlg, overlapFrac, SETUPMONT_MG_FULL_GRID);
    if (!err && stateMag > 0 && montP->magIndex != stateMag) {
      str.Format("You selected to set a state at %dx but changed the mag to %d"
          " in Montage Setup\n\nDo you want to proceed with these settings?",
        MagForCamera(camera, stateMag), MagForCamera(camera, montP->magIndex));
      if (AfxMessageBox(str, MB_QUESTION) != IDYES)
        err = 1;
    }

    mGridMontSetAtXpcs = 0;
  } else {

    // Setup specific piece number
    montP->xNframes = m_iXpieces;
    montP->yNframes = m_iYpieces;
    montP->ignoreSkipList = true;
    if (m_bSetLMMstate)
      mWinApp->mDocWnd->MontParamInitFromFrame(montP, camera, state->xFrame,
        state->yFrame, overlapFrac, state->saveFrames,
        state->alignFrames, state->useFrameAlign, state->K2ReadMode);
    else
      mWinApp->mDocWnd->MontParamInitFromConSet(montP, consNum, overlapFrac);
    montP->moveStage = true;
    err = 0;
    if (!skipDlg) {
      montDlg.mForMultiGridMap = SETUPMONT_MG_LM_NBYN;
      montDlg.mSizeLocked = false;
      montDlg.mParam = *montP;
      err = montDlg.DoModal() != IDOK ? 1 : 0;
      if (!err)
        *montP = montDlg.mParam;
    }
    mGridMontSetAtXpcs = m_iXpieces;
    mGridMontSetAtYpcs = m_iYpieces;

  }
  mGridMontSetupOK = err == 0;
  if (!err) {
    mGridMontSetupState = mParams.LMMstateNum;
    mGridMontSetupOverlap = m_bUseMontOverlap ? m_iMontOverlap : 0;
    mGridMontSetupAcqType = m_iLMMacquireType;
    if (mMGTasks->SaveSessionFile(str) > 0)
      AfxMessageBox(str, MB_EXCLAME);

    // Save the image size in the camera parameters
    SaveMontSetupSize(montP, lowDose, mMontSetupSizesXY[0], mMontSetupSizesXY[1]);
  }
  return err;
}

/*
 * Look for state by its index, and if name doesn't matc, look for it by name
 * and correct the index
 */
StateParams *CMultiGridDlg::GetOrRefindState(int &stateNum, CString &stateName, 
  const char *descrip, CComboBox &combo)
{
  StateParams *state;
  bool refind = false;
  CString str, str2;
  int ind;
  if (stateNum >= (int)mStateArray->GetSize()) {
    refind = true;
  } else {
    state = mStateArray->GetAt(stateNum);
    if (state->name != stateName)
      refind = true;
  }
  if (refind) {

    // Look for it again from the name if possible
    if (stateName.IsEmpty())
      ind = -1;
    else
      ind = FindUniqueStateFromName(stateName);

    // Bail out if can't identify it with confidence
    if (ind < 0) {
      str.Format("The set of states seems to have changed since state # %d ", 
        stateNum + 1);
      if (!stateName.IsEmpty())
        str += "(" + stateName + ") ";
      str += "was selected for ";
      str += descrip;
      str += ".\nYou need to check the selection of this state"
        " before proceeding.\n\nDo you want to reload the combo box "
        "with the current states first?";
      if (AfxMessageBox(str, MB_QUESTION) == IDYES) {
        LoadAllComboBoxes();
        SetAllComboBoxesFromNameOrNum();
      }
      return NULL;
    }
    stateNum = ind;
    state = mStateArray->GetAt(ind);
  }
  return state;
}

/*
 * For MMM or final acquire, check validity of states: one camera, one non-LD or multiple
 * LD states are allowed.  This has all been checked when combo boxes were set...
 */
int CMultiGridDlg::CheckMultipleStates(CComboBox **comboArr, int *stateNums,
  CString *stateNames, CString descrip, int &numStates, int &camera, StateParams **states,
  int *lowDose, int *magInds)
{
  int retVal = 0, ind, jnd, numNon = 0, numLD = 0;
  StateParams *state;
  camera = -1;
  numStates = 0;
  for (ind = 0; ind < 4; ind++) {
    if (stateNums[ind] >= 0) {
      state = GetOrRefindState(stateNums[ind], stateNames[ind], descrip,
        *(comboArr[ind]));
      if (!state)
        return 1;
      if (state->lowDose) {
        numLD++;
        magInds[numStates] = state->ldParams.magIndex;
        for (jnd = 0; jnd < numStates; jnd++) {
          if (state->lowDose == lowDose[jnd]) {
            AfxMessageBox("You have selected more that one state of the same Low Dose "
              "type for " + descrip + ", which makes no sense", MB_EXCLAME);
            return 1;
          }
        }
      } else {
        numNon++;
        magInds[numStates] = state->magIndex;
      }
      if (camera < 0)
        camera = state->camIndex;
      else if (camera != state->camIndex)
        break;
      states[numStates] = state;
      lowDose[numStates++] = state->lowDose;
    }
  }
  if (!(numNon > 1 || (numNon && numLD) || (camera >= 0 && camera != state->camIndex)))
    return 0;
  if (numNon > 1)
    AfxMessageBox("Only one non-Low Dose state can be selected for " + descrip, 
      MB_EXCLAME);
  else if (numNon && numLD)
    AfxMessageBox("You cannot select both Low Dose and non-Low Dose states for " + 
      descrip, MB_EXCLAME);
  else
    AfxMessageBox("All selected states for " + descrip + " must be for the same camera",
      MB_EXCLAME);
  return 1;
}

/*
 * Take the list of states and other settings and determine what set would be used for
 * montaging, low dose state, and the mag and binning for it
 */
void CMultiGridDlg::DeduceStateForMMM(int numStates, int camera, StateParams **states,
  int *lowDose, int *magInd, bool &useLD, bool &useSearch, bool &useView, 
  int &useMontMap, int &useCamera, int &useMag, int &useBin, int &stateForMont)
{
  LowDoseParams *ldParams;
  BOOL curLD = mWinApp->LowDoseMode();
  MontParam *montP = mMGTasks->GetMMMmontParam();
  ControlSet *conSets;
  int ind, area;
  useCamera = mWinApp->GetCurrentCamera();
  useMag = mScope->GetMagIndex();
  useLD = false;
  useView = false;
  useSearch = false;
  useMontMap = 0;
  stateForMont = -1;

  // In Low dose if acquire type is set in LD or if in LD or if low dose state(s) are set
  useLD = m_iMMMacquireType < 2 || (m_iMMMacquireType == 2 && ((curLD && !numStates) ||
    (numStates && lowDose[0])));

  // Use search if specified or if doing montage and it is that way in dialog; 
  // same way with View
  useSearch = m_iMMMacquireType == 0 || (useLD && m_iMMMacquireType != 1 && 
    montP->useSearchInLowDose && m_iSingleVsPolyMont);
  useView = m_iMMMacquireType == 1 || (useLD && m_iMMMacquireType != 2 && 
    montP->useViewInLowDose && m_iSingleVsPolyMont);

  // Leave mont-map flag alone outside low dose unless there is a state being set, either
  // mont map or not.  In LD, turn it off if using S or V
  if (m_iSingleVsPolyMont) {
    if (m_iMMMacquireType == 2 && !useLD && numStates)
      useMontMap = states[0]->montMapConSet ? 1 : -1;
    if (useLD && (useSearch || useView))
      useMontMap = -1;
  }
  if (numStates && useCamera != camera)
    ldParams = mWinApp->GetLDParamsForCamera(camera);
  else
    ldParams = mWinApp->GetLowDoseParams();

  // If states being set, use the camera for that after getting the right conSets
  if (numStates && useCamera != camera)
    conSets = mWinApp->GetCamConSets() + camera * MAX_CONSETS;
  else
    conSets = mWinApp->GetConSets();
  if (numStates)
    useCamera = camera;

  // If in LD, set default mag index and binning based on LD params of selected camera
  // Then loop though states and if one is set for the acquisition mode, use its mag
  if (useLD) {
    if (useView) {
      useMag = ldParams[VIEW_CONSET].magIndex;
      useBin = conSets[VIEW_CONSET].binning;
    } else if (useSearch) {
      useMag = ldParams[SEARCH_AREA].magIndex;
      useBin = conSets[SEARCH_CONSET].binning;
    } else {
      useMag = ldParams[RECORD_CONSET].magIndex;
      useBin = conSets[RECORD_CONSET].binning;
    }
    for (ind = 0; ind < numStates; ind++) {
      area = mWinApp->mNavHelper->AreaFromStateLowDoseValue(states[ind], NULL);
      if ((useView && area == VIEW_CONSET) || (useSearch && area == SEARCH_AREA) ||
        (!useView && !useSearch && area == RECORD_CONSET))
        break;
    }
    if (ind < numStates) {
      useMag = magInd[ind];
      useBin = states[ind]->binning;
      stateForMont = ind;

      // Set mont map one way or another if a Record state is set and not using V or S
      if (area == RECORD_CONSET)
        useMontMap = states[ind]->montMapConSet ? 1 : -1;
    }
  } else if (numStates) {

    // non-LD, use the mag from state if set
    useMag = states[0]->magIndex;
    useBin = states[0]->binning;
    stateForMont = 0;
  } else {
    useBin = conSets[(useMontMap > 0 || (m_iSingleVsPolyMont && useMontMap == 0 && 
      montP->useMontMapParams)) ? MONT_USER_CONSET : RECORD_CONSET].binning;
  }
}

/*
 * Analyze parameters and do everything needed to set up for taking MMM's, which may
 * involve montage setup
 */
int CMultiGridDlg::SetupMMMacquire(bool skipDlg)
{
  CMapDrawItem *item;
  CString str;
  CMontageSetupDlg montDlg;
  MontParam *montP = mMGTasks->GetMMMmontParam();
  AutoContourParams *acParams = mWinApp->mNavHelper->GetAutocontourParams();
  float itemSize = 60.;
  CComboBox *comboArr[4] = {&m_comboMMMstate1, &m_comboMMMstate2, &m_comboMMMstate3,
    &m_comboMMMstate4};
  bool useLD, useSearch, useView;
  int useMag, useCamera, useMontMap, useBin, err = 0, actCam, stateForMont;
  int indForMont = -1;
  StateParams *stateArr[4];
  int stateLowDose[4], stateMagInds[4], numStates, stateCamera;

  // Check the states
  if (CheckMultipleStates(comboArr, mParams.MMMstateNums, mParams.MMMstateNames,
    "medium-mag maps", numStates, stateCamera, stateArr, stateLowDose, stateMagInds))
    return 1;
  if (numStates && !stateLowDose[0] && m_iMMMacquireType < 2) {
    AfxMessageBox("You selected medium mag map acquisition with a Low-Dose\n"
      "area but are setting a non-Low Dose state.\n"
      "This makes no sense and needs to be fixed before proceeding", MB_EXCLAME);
    return 1;
  }

  // Get the state for acquisition
  DeduceStateForMMM(numStates, stateCamera, stateArr, stateLowDose, stateMagInds,
    useLD, useSearch, useView, useMontMap, useCamera, useMag, useBin, stateForMont);

  if (useMontMap > 0 && m_iSingleVsPolyMont == 0) {
    AfxMessageBox("You selected single-image for medium mag map acquisition\n"
      "but set a Mont-Map state which will not be used for that.\n"
      "This makes no sense and needs to be fixed before proceeding", MB_EXCLAME);
    return 1;
  }
  mMMMneedsLowDose = useLD ? 1 : -1;
  if (numStates)
    mMMMneedsLowDose = 0;
  if (m_iSingleVsPolyMont && stateForMont >= 0)
    indForMont = mParams.MMMstateNums[stateForMont];

  // If "skipDlg" is set it is being done for running, so check and return if OK
  if (skipDlg && mMMMsetupOK && mMMMsetupAcqType == m_iMMMacquireType &&
    mMMMsetupMontType == m_iSingleVsPolyMont && mMMMsetupStateForMont == indForMont) {
    if (!m_iSingleVsPolyMont || MontSetupSizesMatch(montP, useLD, mMontSetupSizesXY[2],
      mMontSetupSizesXY[3]))
      return 0;
  }
  mMMMsetupOK = false;
  if (m_iSingleVsPolyMont > 1)
    skipDlg = false;

  // Make sure camera is OK
  actCam = mWinApp->LookupActiveCamera(useCamera);
  if (actCam < 0) {
    AfxMessageBox("The camera for the state to be set is not in the active list",
      MB_EXCLAME);
    return 1;
  }

  // For montages: set the parameter set and low dose and other things that should be set
  if (m_iSingleVsPolyMont) {
    montP->useViewInLowDose = useView;
    montP->useSearchInLowDose = useSearch;
    montP->usePrevInLowDose = false;
    montP->useMultiShot = false;
    if (useMontMap)
      montP->useMontMapParams = useMontMap > 0;
    montP->cameraIndex = actCam;
    montP->setupInLowDose = useLD;
    montP->skipOutsidePoly = false;
    montP->skipCorrelations = false;
    montP->ignoreSkipList = false;
    montP->warnedConSetBin = true;
    montP->binning = useBin;
    montP->magIndex = useMag;
    montDlg.mForMultiGridMap = SETUPMONT_MG_POLYGON;
    if (!mMMMmontWasSetup)
      montP->moveStage = true;
    if (!skipDlg)
      montDlg.mSizeLocked = false;

    // For polygon montage, make a square in an item; use autocont range if appropriate
    if (m_iSingleVsPolyMont == 1) {
      if (m_bAutocontour) {
        mWinApp->mNavHelper->mAutoContouringDlg->SyncToMasterParams();
        itemSize = (acParams->minSize + acParams->maxSize) / 2.f;
      }
      item = new CMapDrawItem;
      item->AppendPoint(0., 0.);
      item->AppendPoint(itemSize, 0.);
      item->AppendPoint(itemSize, itemSize);
      item->AppendPoint(0., itemSize);
      item->mStageX = itemSize / 2.f;
      item->mStageY = itemSize / 2.f;
      if (!skipDlg) {
        montDlg.mParam = *montP;
        montDlg.mForMultiGridMap = SETUPMONT_MG_POLYGON;
        montDlg.mDummyAreaSize = itemSize;
      }

      // Set up the montage
      err = mWinApp->mNavigator->SetupMontage(item, skipDlg ? NULL : &montDlg, skipDlg,
        0., SETUPMONT_MG_POLYGON);
      delete item;
      if (!err && !skipDlg)
        *montP = montDlg.mParam;
      if (!err && stateForMont && useMag > 0 && montP->magIndex != useMag) {
        str.Format("You selected to set a state at %dx but changed the mag to %d"
          " in Montage Setup\n\nDo you want to proceed with these settings?",
          MagForCamera(useCamera, useMag), MagForCamera(useCamera, montP->magIndex));
        if (AfxMessageBox(str, MB_QUESTION) != IDYES)
          err = 1;
      }
    } else {

      //Fixed nxn: leave # of frames alone if done last time
      if (!(mMMMmontWasSetup && mMMMsetupMontType == m_iSingleVsPolyMont)) {
        montP->xNframes = mParams.MMMnumXpieces;
        montP->yNframes = mParams.MMMnumYpieces;
      }
      montP->ignoreSkipList = true;
      if (stateForMont >= 0) {
        mWinApp->mDocWnd->MontParamInitFromFrame(montP, useCamera,
          stateArr[stateForMont]->xFrame, stateArr[stateForMont]->yFrame, 0., 
          stateArr[stateForMont]->saveFrames, stateArr[stateForMont]->alignFrames, 
          stateArr[stateForMont]->useFrameAlign, stateArr[stateForMont]->K2ReadMode);
      } else {
        mWinApp->mDocWnd->MontParamInitFromConSet(montP, 
          MontageConSetNum(montP, true, useLD), 0.);
      }
      err = 0;
      if (!skipDlg) {

        // Run dialog
        montDlg.mForMultiGridMap = SETUPMONT_MG_MMM_NBYN;
        montDlg.mParam = *montP;
        err = montDlg.DoModal() != IDOK ? 1 : 0;
        if (!err) {
          *montP = montDlg.mParam;
          mParams.MMMnumXpieces = montP->xNframes;
          mParams.MMMnumYpieces = montP->yNframes;
         }
      }
    }
    mMMMmontWasSetup = !err;
  }

  // Save values so we know if still valid
  mMMMsetupOK = !err;
  if (!err) {
    mMMMsetupAcqType = m_iMMMacquireType;
    mMMMsetupMontType = m_iSingleVsPolyMont;
    mMMMsetupStateForMont = indForMont;
    if (m_iSingleVsPolyMont) {
      SaveMontSetupSize(montP, useLD, mMontSetupSizesXY[2], mMontSetupSizesXY[3]);
    }
    if (mMGTasks->SaveSessionFile(str) > 0)
      AfxMessageBox(str, MB_EXCLAME);
  }
  return err;
}

// Look up the control set used by the montage and save size in given variables
void CMultiGridDlg::SaveMontSetupSize(MontParam *montP, BOOL lowDose, int &setupSizeX, 
  int &setupSizeY)
{
  int consNum = MontageConSetNum(montP, true, lowDose ? 1 : 0);
  ControlSet *conSet = mWinApp->GetCamConSets() + montP->cameraIndex * MAX_CONSETS +
    consNum;
  setupSizeX = conSet->right - conSet->left;
  setupSizeY = conSet->bottom - conSet->top;
}

// Check current size in control set for whether it matches size in last setup
bool CMultiGridDlg::MontSetupSizesMatch(MontParam *montP, BOOL lowDose, int setupSizeX, 
  int setupSizeY)
{
  int consNum = MontageConSetNum(montP, true, lowDose ? 1 : 0);
  ControlSet *conSet = mWinApp->GetCamConSets() + montP->cameraIndex * MAX_CONSETS +
    consNum;
  return (setupSizeX == conSet->right - conSet->left && 
    setupSizeY == conSet->bottom - conSet->top);
}

/*
 * Make sure the image type or state options are valid for the refine operation and
 * that the FOV is big enough
 */
int CMultiGridDlg::CheckRefineOptions(float &FOV, int &ldArea, int &setNum, 
  StateParams **state, int &numXpiece, int &numYpiece, CString errStr)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  ControlSet *conSets;
  CameraParameters *camParams = mWinApp->GetCamParams();
  int *actList = mWinApp->GetActiveCameraList();
  CString *setNames = mWinApp->GetModeNames();
  int magInd, ind, avail = 0, camSize, selInd = -1, numPixNeeded, sizeX, sizeY;
  int xSpacing, ySpacing;
  int numStates = (int)mStateArray->GetSize();
  int camera = mWinApp->GetCurrentCamera();
  float pixel;
  FOV = 0.;
  ldArea = m_iRefineRealign > 0 ? VIEW_CONSET : SEARCH_AREA;
  setNum = m_iRefineRealign > 0 ? VIEW_CONSET : SEARCH_CONSET;
  *state = NULL;
  if (m_iRefineRealign > 1) {
    if (mParams.refineStateNum < 0 || !numStates) {
      errStr = "There is no state selected or none defined";
      return -1;
    }
    if (numStates) {

      // If there is a name, look it up by name and make sure it is unique
      if (!mParams.refineStateName.IsEmpty()) {
        selInd = FindUniqueStateFromName(mParams.refineStateName);
        if (selInd >= 0)
          mParams.refineStateNum = selInd;
      }

      // By number - make sure its legal
      if (selInd < 0) {
        if (mParams.refineStateNum >= numStates) {
          errStr = "The state could not be found by name and its number is out of range";
          return -1;
        }
        selInd = mParams.refineStateNum;
      }
    }
    *state = mStateArray->GetAt(selInd);
    mParams.refineStateName = (*state)->name;
    camera = (*state)->camIndex;
    for (ind = 0; ind < mWinApp->GetActiveCamListSize(); ind++)
      if (camera == actList[ind])
        avail = 1;
    if (!avail) {
      errStr = "Selected state is for a camera that is not available";
      return -1;
    }
    ldArea = -1;
    if ((*state)->lowDose != 0)
      ldArea = mWinApp->mNavHelper->AreaFromStateLowDoseValue((*state), NULL);
    if (ldArea < 0) {
      setNum = PREVIEW_CONSET;
      magInd = (*state)->magIndex;
    } else {
      if (ldArea != VIEW_CONSET && ldArea != SEARCH_AREA) {
        errStr = "The state must be either low dose View or Search, or a non-low dose "
          "state";
        return -1;
      }
      setNum = ldArea ? SEARCH_CONSET : VIEW_CONSET;
      magInd = (*state)->ldParams.magIndex;
    }
  } else {
    magInd = ldp[ldArea].magIndex;
  }

  conSets = mWinApp->GetCamConSets() + camera * MAX_CONSETS;
  camSize = B3DMIN(conSets[setNum].right - conSets[setNum].left,
    conSets[setNum].bottom - conSets[setNum].top);
  pixel = mWinApp->mShiftManager->GetPixelSize(camera, magInd);
  FOV = pixel * (float)camSize;
  numXpiece = numYpiece = 0;
  if (FOV > mMGTasks->GetRefineMinField())
    return 0;

  numPixNeeded = (int)(mMGTasks->GetRefineMinField() / pixel);
  sizeX = camParams[camera].sizeX;
  sizeY = camParams[camera].sizeY;
  xSpacing = sizeX - sizeX / 10;
  ySpacing = sizeY - sizeY / 10;
  numXpiece = 1 + ((numPixNeeded - sizeX) + xSpacing - 1) / xSpacing;
  numYpiece = 1 + ((numPixNeeded - sizeY) + ySpacing - 1) / ySpacing;
  FOV = pixel * B3DMIN(sizeX + xSpacing * (numXpiece - 1),
    sizeY + ySpacing * (numYpiece - 1));
  return 0;
}

int CMultiGridDlg::ManageRefineFOV()
{
  StateParams *state;
  int ldArea, setNum, err, numXpc, numYpc;
  float FOV;
  CString errStr;
  ShowDlgItem(IDC_STAT_REFINE_FOV, m_bRefineRealign);
  if (!m_bRefineRealign)
    return 0;
  err = CheckRefineOptions(FOV, ldArea, setNum, &state, numXpc, numYpc, errStr);
  if (err) {
    m_strRefineFOV = err < 0 ? "BAD STATE" : "BAD FOV";
    SEMAppendToLog(errStr);
  } else {
    m_strRefineFOV.Format("FOV %.1f um", FOV);
    if (numXpc * numYpc > 1) {
      errStr.Format(" in %dx%d", numXpc, numYpc);
      m_strRefineFOV += errStr;
    }
  }
  UpdateData(false);
  return err;
}

// Set label after aperture size box depending on what is available
void CMultiGridDlg::ManageCondenserUnits()
{
  int ind, apNum = mMGTasks->GetSingleCondenserAp();
  std::vector<ShortVec> *apList = mScope->GetApertureLists();
  
  if (!JEOLscope)
    return;
  if (mMGTasks->GetUseTwoJeolCondAp())
    apNum = m_iC1orC2 ? JEOL_C1_APERTURE : CONDENSER_APERTURE;
  for (ind = 0; ind < (int)apList->size(); ind++) {
    ShortVec &sizeList = apList->at(ind);
    if (sizeList[0] == apNum) {
      SetDlgItemText(IDC_STAT_MG_MICRON, "um");
      return;
    }
  }
  SetDlgItemText(IDC_STAT_MG_MICRON, "(index #)");
}

/*
 * RUNNING ONE OR MORE PHASES
 */
// Start the run with all checked items
void CMultiGridDlg::OnButStartRun()
{
  if (mMGTasks->GetSuspendedMulGrid())
    mMGTasks->EndMulGridSeq();
  else
    DoStartRun(false);
}

// Run undone only: skip ones that have finished first phase selected
void CMultiGridDlg::OnButRunUndone()
{
  if (mMGTasks->GetSuspendedMulGrid())
    mMGTasks->UserResumeMulGridSeq();
  else
    DoStartRun(true);
}

/*
 * Common function to do some final checks and hand off to MGTasks for MORE checks, etc
 */
void CMultiGridDlg::DoStartRun(bool undoneOnly)
{
  UPDATE_DATA_TRUE;
  int finalNeedsLowDose = mWinApp->LowDoseMode() ? 1 : -1;
  CComboBox *comboArr[4] = {&m_comboFinalState1, &m_comboFinalState2, &m_comboFinalState3,
    &m_comboFinalState4};
  StateParams *stateArr[4];
  bool sizesMatch = true;
  MontParam *montP;
  int stateLowDose[4], stateMagInds[4], numStates, stateCamera;
  mWinApp->RestoreViewFocus();
  DialogToParams();
  *mMasterParams = mParams;
  if (m_bTakeLMMs) {
    montP = mMGTasks->GetGridMontParam();
    sizesMatch = MontSetupSizesMatch(montP, montP->setupInLowDose,
      mMontSetupSizesXY[0], mMontSetupSizesXY[1]);
    if (!sizesMatch)
      mGridMontSetupOK = false;
    if (m_iLMMacquireType != mGridMontSetupAcqType || (mGridMontSetAtXpcs == 0 &&
      m_iLMMmontType) || (!m_iLMMmontType && mGridMontSetAtXpcs > 0) || (m_iLMMmontType &&
      (m_iXpieces != mGridMontSetAtXpcs || m_iYpieces != mGridMontSetAtYpcs)) ||
      mGridMontSetupOverlap != (m_bUseMontOverlap ? m_iMontOverlap : 0) ||
      mGridMontSetupState != mParams.LMMstateNum)
      mGridMontSetupOK = false;
    if (!mGridMontSetupOK) {
      if (DoSetupLMMmont(sizesMatch))
        return;
    }
  }

  if (m_bTakeMMMs) {
    montP = mMGTasks->GetMMMmontParam();
    if (SetupMMMacquire(MontSetupSizesMatch(montP, 
      montP->setupInLowDose, mMontSetupSizesXY[2], mMontSetupSizesXY[3])))
      return;
  }

  if (m_bRunFinalAcq) {
    if (CheckMultipleStates(comboArr, mParams.finalStateNums, mParams.finalStateNames,
      "final acquisition", numStates, stateCamera, stateArr, stateLowDose, stateMagInds))
      return;
    if (numStates)
      finalNeedsLowDose = 0;
  }
  mMGTasks->StartGridRuns(mLMneedsLowDose, mMMMneedsLowDose, finalNeedsLowDose,
    stateCamera < 0 ? mWinApp->GetCurrentCamera() : stateCamera, undoneOnly);
}
