// NavAcquireDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "ParameterIO.h"
#include "EMscope.h"
#include "Mailer.h"
#include ".\NavAcquireDlg.h"
#include "NavigatorDlg.h"
#include "NavRealignDlg.h"
#include "CameraController.h"
#include "CookerSetupDlg.h"
#include "ManageDewarsDlg.h"
#include "ParticleTasks.h"
#include "AutoTuning.h"
#include "MultiTSTasks.h"
#include "GainRefMaker.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"

#define MIN_CYCLE_FOCUS -30.f
#define MAX_CYCLE_FOCUS 3.f
#define MAX_EVERY_N   300

#undef DOING_ACTION
#define DOING_ACTION(a) ((mActions[a].flags & NAA_FLAG_RUN_IT) ? 1 : 0)

// BEWARE: primary action radio buttons are now out of order, in SerialEM.rc or if
// reordering with Ctrl-D, need map, Just Acquire, run script, tilt series, multishot

static int idTable[] = {IDC_STAT_PRIMARY_GROUP, IDC_NA_ACQUIREMAP, IDC_NA_SAVE_AS_MAP,
IDC_NA_RUNMACRO, IDC_NA_ACQUIRE_TS, IDC_NA_DO_MULTISHOT, IDC_EDIT_SUBSET_END,
IDC_EDIT_SUBSET_START,  IDC_EDIT_CYCLE_TO, IDC_EDIT_CYCLE_FROM, IDC_EDIT_EARLY_FRAMES,
IDC_NA_CLOSE_VALVES, IDC_COMBO_MACRO, IDC_NA_SKIP_SAVING,
IDC_STAT_WHICH_CONSET, IDC_RMAP_WITH_REC, IDC_RMAP_WITH_VIEW, IDC_RMAP_WITH_SEARCH,
IDC_STAT_SAVING_FATE, IDC_STAT_FILE_SAVING_INTO, IDC_NA_DO_SUBSET, IDC_STAT_TASK_OPTIONS,
IDC_STAT_SUBSET_TO, IDC_NA_SKIP_INITIAL_MOVE, IDC_NA_SKIP_Z_MOVES, IDC_STAT_GEN_OPTIONS,
IDC_NA_CYCLE_DEFOCUS, IDC_NA_SETUP_MULTISHOT, IDC_STAT_CYCLE_STEPS,IDC_RACQUISITION,
IDC_STAT_CYCLE_TO, IDC_STAT_CYCLE_UM, IDC_SPIN_CYCLE_DEF, IDC_NA_EARLY_RETURN,
IDC_STAT_FRAMES, IDC_NA_NO_MBOX_ON_ERROR,  IDC_NA_SENDEMAIL, IDC_RMAPPING, 
IDC_NA_ADJUST_BT_FOR_IS, IDC_STAT_ACTION_OPTIONS, IDC_STAT_PARAM_FOR, 
IDC_NA_APPLY_REALIGN_ERR, IDC_NA_RELAX_STAGE, IDC_TSS_LINE1, IDC_TSS_LINE2, IDC_TSS_LINE3,
IDC_NA_HIDE_OPTIONS, IDC_STAT_CYCLE_FROM, IDC_STAT_SPACER3, IDC_TSS_LINE4, PANEL_END,
IDC_STAT_ACTION_GROUP, IDC_NA_TASK_LINE1, IDC_NA_TASK_LINE2, IDC_STAT_PRIMARY_LINE,
IDC_COMBO_PREMACRO, IDC_STAT_PREMACRO,  IDC_COMBO_POSTMACRO, IDC_STAT_POSTMACRO,
IDC_RADIO_NAVACQ_SEL1, IDC_RADIO_NAVACQ_SEL2,IDC_RADIO_NAVACQ_SEL3, IDC_RADIO_NAVACQ_SEL4,
IDC_RADIO_NAVACQ_SEL5, IDC_RADIO_NAVACQ_SEL6, IDC_RADIO_NAVACQ_SEL7, 
IDC_RADIO_NAVACQ_SEL8,
IDC_RADIO_NAVACQ_SEL9, IDC_RADIO_NAVACQ_SEL10, IDC_RADIO_NAVACQ_SEL11,
IDC_RADIO_NAVACQ_SEL12, IDC_RADIO_NAVACQ_SEL13, IDC_RADIO_NAVACQ_SEL14,
IDC_CHECK_NAVACQ_RUN1, IDC_CHECK_NAVACQ_RUN2, IDC_CHECK_NAVACQ_RUN3,
IDC_CHECK_NAVACQ_RUN4, IDC_CHECK_NAVACQ_RUN5, IDC_CHECK_NAVACQ_RUN6,
IDC_CHECK_NAVACQ_RUN7, IDC_CHECK_NAVACQ_RUN8, IDC_CHECK_NAVACQ_RUN9,
IDC_CHECK_NAVACQ_RUN10, IDC_CHECK_NAVACQ_RUN11, IDC_CHECK_NAVACQ_RUN12,
IDC_CHECK_NAVACQ_RUN13, IDC_CHECK_NAVACQ_RUN14, IDC_STAT_NAVACQ_WHEN1,
IDC_STAT_NAVACQ_WHEN2, IDC_STAT_NAVACQ_WHEN3, IDC_STAT_NAVACQ_WHEN4,
IDC_STAT_NAVACQ_WHEN5, IDC_STAT_NAVACQ_WHEN6, IDC_STAT_NAVACQ_WHEN7,
IDC_STAT_NAVACQ_WHEN8, IDC_STAT_NAVACQ_WHEN9, IDC_STAT_NAVACQ_WHEN10,
IDC_STAT_NAVACQ_WHEN11, IDC_STAT_NAVACQ_WHEN12, IDC_STAT_NAVACQ_WHEN13,
IDC_STAT_NAVACQ_WHEN14, IDC_BUT_NAVACQ_SETUP1, IDC_BUT_NAVACQ_SETUP2,
IDC_BUT_NAVACQ_SETUP3, IDC_BUT_NAVACQ_SETUP4, IDC_BUT_NAVACQ_SETUP5,
IDC_BUT_NAVACQ_SETUP6, IDC_BUT_NAVACQ_SETUP7, IDC_BUT_NAVACQ_SETUP8,
IDC_BUT_NAVACQ_SETUP9, IDC_BUT_NAVACQ_SETUP10, IDC_BUT_NAVACQ_SETUP11,
IDC_BUT_NAVACQ_SETUP12, IDC_BUT_NAVACQ_SETUP13, IDC_BUT_NAVACQ_SETUP14,
IDC_BUT_NAVACQ_SETUP15, IDC_BUT_NAVACQ_SETUP16, IDC_BUT_NAVACQ_SETUP17,
IDC_BUT_NAVACQ_SETUP18,
IDC_CHECK_NAVACQ_RUN15, IDC_RADIO_NAVACQ_SEL15, IDC_STAT_NAVACQ_WHEN15,
IDC_CHECK_NAVACQ_RUN16, IDC_RADIO_NAVACQ_SEL16, IDC_STAT_NAVACQ_WHEN16,
IDC_CHECK_NAVACQ_RUN17, IDC_RADIO_NAVACQ_SEL17, IDC_STAT_NAVACQ_WHEN17,
IDC_CHECK_NAVACQ_RUN18, IDC_RADIO_NAVACQ_SEL18, IDC_STAT_NAVACQ_WHEN18,
IDC_TSS_LINE5, PANEL_END,
IDC_RNAVACQ_EVERY_N, IDC_RNAVACQ_GROUP_START,
IDC_RNAVACQ_GROUP_END, IDC_RNAVACQ_AFTER_MINUTES, IDC_RNAVACQ_WHEN_MOVED,
IDC_STAT_INTERVAL_GROUP, IDC_RGOTO_LABEL, IDC_RGOTO_NOTE, IDC_NA_RUN_AT_OTHER,
IDC_EDIT_EVERY_N, IDC_EDIT_AFTER_MINUTES,
IDC_EDIT_WHEN_MOVED, IDC_EDIT_GOTO_ITEM,
IDC_SPIN_EVERY_N, IDC_STAT_EVERY_N, IDC_STAT_AFTER_MINUTES, IDC_STAT_WHEN_MOVED,
IDC_STAT_SELECTED_GROUP, IDC_STAT_ORDER_GROUP, IDC_BUT_MOVE_UP,
IDC_BUT_MOVE_DOWN, IDC_NA_RUN_AFTER_TASK, IDC_TSS_LINE6, PANEL_END,
IDC_NA_HIDE_UNUSED,
IDC_BUT_DEFAULT_ORDER,PANEL_END,
IDC_BUTHELP, IDC_BUT_POSTPONE, IDOK, IDCANCEL, IDC_NA_READ_PARAMS, IDC_NA_SAVE_PARAMS,
IDC_STAT_SPACER2, PANEL_END, TABLE_END
};

static int sTopTable[sizeof(idTable) / sizeof(int)];
static int sLeftTable[sizeof(idTable) / sizeof(int)];
static int sHeightTable[sizeof(idTable) / sizeof(int)];


// CNavAcquireDlg dialog

CNavAcquireDlg::CNavAcquireDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavAcquireDlg::IDD, pParent)
  , m_iAcquireChoice(0)
  , m_bCloseValves(FALSE)
  , m_bSendEmail(FALSE)
  , m_strSavingFate(_T(""))
  , m_strFileSavingInto(_T(""))
  , m_bDoSubset(FALSE)
  , m_iSubsetStart(0)
  , m_iSubsetEnd(0)
  , m_bSkipInitialMove(FALSE)
  , m_bSkipZmoves(FALSE)
  , m_iSelectedPos(0)
  , m_iTimingType(0)
  , m_iEveryNitems(1)
  , m_iAfterMinutes(15)
  , m_fWhenMoved(2.f)
  , m_bRunAtOther(FALSE)
  , m_iGotoLabelNote(0)
  , m_strGotoItem(_T(""))
  , m_bRunAfterTask(FALSE)
  , m_bCycleDefocus(FALSE)
  , m_fCycleFrom(0)
  , m_fCycleTo(0)
  , m_iEarlyFrames(0)
  , m_bEarlyReturn(FALSE)
  , m_bNoMBoxOnError(FALSE)
  , m_bSkipSaving(FALSE)
  , m_bHideUnusedActions(FALSE)
  , m_bAdjustBTforIS(FALSE)
  , m_iCurParamSet(0)
  , m_bHybridRealign(FALSE)
  , m_bRelaxStage(FALSE)
  , m_bHideUnselectedOpts(FALSE)
  , m_iMapWithViewSearch(0)
  , m_bSaveAsMap(FALSE)
{
  mCurActSelected = -1;
  mNonModal = true;
}

CNavAcquireDlg::~CNavAcquireDlg()
{
}

void CNavAcquireDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_NA_ACQUIREMAP, m_iAcquireChoice);
  DDX_Control(pDX, IDC_NA_CLOSE_VALVES, m_butCloseValves);
  DDX_Check(pDX, IDC_NA_CLOSE_VALVES, m_bCloseValves);
  DDX_Control(pDX, IDC_NA_SENDEMAIL, m_butSendEmail);
  DDX_Check(pDX, IDC_NA_SENDEMAIL, m_bSendEmail);
  DDX_Control(pDX, IDC_NA_ACQUIRE_TS, m_butAcquireTS);
  DDX_Control(pDX, IDC_NA_DO_MULTISHOT, m_butDoMultishot);
  DDX_Control(pDX, IDC_NA_RUNMACRO, m_butRunMacro);
  DDX_Control(pDX, IDC_NA_ACQUIREMAP, m_butAcquireMap);
  DDX_Control(pDX, IDC_COMBO_PREMACRO, m_comboPremacro);
  DDX_Control(pDX, IDC_COMBO_POSTMACRO, m_comboPostmacro);
  DDX_Control(pDX, IDC_COMBO_MACRO, m_comboMacro);
  DDX_Text(pDX, IDC_STAT_SAVING_FATE, m_strSavingFate);
  DDX_Text(pDX, IDC_STAT_FILE_SAVING_INTO, m_strFileSavingInto);
  DDX_Check(pDX, IDC_NA_DO_SUBSET, m_bDoSubset);
  DDX_Control(pDX, IDC_EDIT_SUBSET_START, m_editSubsetStart);
  DDX_Text(pDX, IDC_EDIT_SUBSET_START, m_iSubsetStart);
  DDX_Control(pDX, IDC_EDIT_SUBSET_END, m_editSubsetEnd);
  DDX_Text(pDX, IDC_EDIT_SUBSET_END, m_iSubsetEnd);
  DDX_Control(pDX, IDC_NA_SKIP_INITIAL_MOVE, m_butSkipInitialMove);
  DDX_Check(pDX, IDC_NA_SKIP_INITIAL_MOVE, m_bSkipInitialMove);
  DDX_Check(pDX, IDC_NA_SKIP_Z_MOVES, m_bSkipZmoves);
  //DDX_Radio(pDX, IDC_RADIO_NAVACQ_SEL1, m_iSelectedPos);
  DDX_Radio(pDX, IDC_RNAVACQ_EVERY_N, m_iTimingType);
  DDX_Control(pDX, IDC_EDIT_EVERY_N, m_editEveryN);
  DDX_Text(pDX, IDC_EDIT_EVERY_N, m_iEveryNitems);
  MinMaxInt(IDC_EDIT_EVERY_N, m_iEveryNitems, 1, MAX_EVERY_N, "Interval between items");
  DDX_Control(pDX, IDC_SPIN_EVERY_N, m_sbcEveryN);
  DDX_Control(pDX, IDC_EDIT_AFTER_MINUTES, m_editAfterMinutes);
  MinMaxInt(IDC_EDIT_AFTER_MINUTES, m_iAfterMinutes, 5, 1500, "Time interval for action");
  DDX_Text(pDX, IDC_EDIT_AFTER_MINUTES, m_iAfterMinutes);
  DDX_Control(pDX, IDC_EDIT_WHEN_MOVED, m_editWhenMoved);
  DDX_Text(pDX, IDC_EDIT_WHEN_MOVED, m_fWhenMoved);
  MinMaxFloat(IDC_EDIT_WHEN_MOVED, m_fWhenMoved, 0.1f, 2000.f, "Distance moved");
  DDX_Check(pDX, IDC_NA_RUN_AT_OTHER, m_bRunAtOther);
  DDX_Radio(pDX, IDC_RGOTO_LABEL, m_iGotoLabelNote);
  DDX_Control(pDX, IDC_EDIT_GOTO_ITEM, m_editGotoItem);
  DDX_Text(pDX, IDC_EDIT_GOTO_ITEM, m_strGotoItem);
  DDX_Control(pDX, IDC_BUT_MOVE_UP, m_butMoveUp);
  DDX_Control(pDX, IDC_BUT_MOVE_DOWN, m_butMoveDown);
  DDX_Check(pDX, IDC_NA_RUN_AFTER_TASK, m_bRunAfterTask);
  DDX_Control(pDX, IDC_NA_SETUP_MULTISHOT, m_butSetupMultishot);
  DDX_Check(pDX, IDC_NA_CYCLE_DEFOCUS, m_bCycleDefocus);
  DDX_Control(pDX, IDC_EDIT_CYCLE_FROM, m_editCycleFrom);
  DDX_Text(pDX, IDC_EDIT_CYCLE_FROM, m_fCycleFrom);
  MinMaxFloat(IDC_EDIT_CYCLE_FROM, m_fCycleFrom, MIN_CYCLE_FOCUS, MAX_CYCLE_FOCUS,
    "Defocus cycle starting value");
  DDX_Control(pDX, IDC_EDIT_CYCLE_TO, m_editCycleTo);
  DDX_Text(pDX, IDC_EDIT_CYCLE_TO, m_fCycleTo);
  MinMaxFloat(IDC_EDIT_CYCLE_TO, m_fCycleTo, MIN_CYCLE_FOCUS, MAX_CYCLE_FOCUS,
    "Defocus cycle ending value");
  DDX_Text(pDX, IDC_STAT_CYCLE_UM, m_strCycleUm);
  DDX_Control(pDX, IDC_SPIN_CYCLE_DEF, m_sbcCycleDef);
  DDX_Control(pDX, IDC_EDIT_EARLY_FRAMES, m_editEarlyFrames);
  DDX_Text(pDX, IDC_EDIT_EARLY_FRAMES, m_iEarlyFrames);
  MinMaxInt(IDC_EDIT_EARLY_FRAMES, m_iEarlyFrames, -1, 300,
    "Number of frames in early return");
  DDX_Control(pDX, IDC_STAT_ACTION_GROUP, m_statActionGroup);
  DDX_Control(pDX, IDC_STAT_SELECTED_GROUP, m_statSelectedGroup);
  DDX_Check(pDX, IDC_NA_EARLY_RETURN, m_bEarlyReturn);
  DDX_Control(pDX, IDC_NA_EARLY_RETURN, m_butEarlyReturn);
  DDX_Check(pDX, IDC_NA_NO_MBOX_ON_ERROR, m_bNoMBoxOnError);
  DDX_Control(pDX, IDC_NA_SKIP_SAVING, m_butSkipSaving);
  DDX_Check(pDX, IDC_NA_SKIP_SAVING, m_bSkipSaving);
  DDX_Check(pDX, IDC_NA_HIDE_UNUSED, m_bHideUnusedActions);
  DDX_Control(pDX, IDC_NA_ADJUST_BT_FOR_IS, m_butAdjustBtforIS);
  DDX_Check(pDX, IDC_NA_ADJUST_BT_FOR_IS, m_bAdjustBTforIS);
  DDX_Radio(pDX, IDC_RMAPPING, m_iCurParamSet);
  DDX_Control(pDX, IDC_NA_APPLY_REALIGN_ERR, m_butHybridRealign);
  DDX_Check(pDX, IDC_NA_APPLY_REALIGN_ERR, m_bHybridRealign);
  DDX_Check(pDX, IDC_NA_RELAX_STAGE, m_bRelaxStage);
  DDX_Check(pDX, IDC_NA_HIDE_OPTIONS, m_bHideUnselectedOpts);
  DDX_Control(pDX, IDOK, m_butOKGO);
  DDX_Radio(pDX, IDC_RMAP_WITH_REC, m_iMapWithViewSearch);
  DDX_Control(pDX, IDC_NA_SAVE_AS_MAP, m_butSaveAsMap);
  DDX_Check(pDX, IDC_NA_SAVE_AS_MAP, m_bSaveAsMap);
}


BEGIN_MESSAGE_MAP(CNavAcquireDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_NA_ACQUIREMAP, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_JUSTACQUIRE, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_RUNMACRO, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_ACQUIRE_TS, OnNaAcquiremap)
  ON_BN_CLICKED(IDC_NA_DO_MULTISHOT, OnNaAcquiremap)
  ON_CBN_SELENDOK(IDC_COMBO_PREMACRO, OnSelendokComboPremacro)
  ON_CBN_SELENDOK(IDC_COMBO_POSTMACRO, OnSelendokComboPostmacro)
  ON_CBN_SELENDOK(IDC_COMBO_MACRO, OnSelendokComboMacro)
  ON_BN_CLICKED(IDC_NA_DO_SUBSET, OnDoSubset)
  ON_EN_KILLFOCUS(IDC_EDIT_SUBSET_START, OnKillfocusSubsetStart)
  ON_EN_KILLFOCUS(IDC_EDIT_SUBSET_END, OnKillfocusSubsetEnd)
  ON_BN_CLICKED(IDC_NA_SKIP_INITIAL_MOVE, OnSkipInitialMove)
  ON_BN_CLICKED(IDC_BUT_POSTPONE, OnButPostpone)
  ON_BN_CLICKED(IDC_RNAVACQ_EVERY_N, OnRadioTimingType)
  ON_BN_CLICKED(IDC_RNAVACQ_EVERY_N, OnRadioTimingType)
  ON_BN_CLICKED(IDC_RNAVACQ_GROUP_START, OnRadioTimingType)
  ON_BN_CLICKED(IDC_RNAVACQ_GROUP_END, OnRadioTimingType)
  ON_BN_CLICKED(IDC_RNAVACQ_AFTER_MINUTES, OnRadioTimingType)
  ON_BN_CLICKED(IDC_RNAVACQ_WHEN_MOVED, OnRadioTimingType)
  ON_COMMAND_RANGE(IDC_RADIO_NAVACQ_SEL1, IDC_RADIO_NAVACQ_SEL25, OnRadioSelectAction)
  ON_COMMAND_RANGE(IDC_CHECK_NAVACQ_RUN1, IDC_CHECK_NAVACQ_RUN25, OnCheckRunAction)
  ON_COMMAND_RANGE(IDC_BUT_NAVACQ_SETUP1, IDC_BUT_NAVACQ_SETUP25, OnButSetupAction)
  ON_EN_KILLFOCUS(IDC_EDIT_EVERY_N, OnEnKillfocusEditEveryN)
  ON_EN_KILLFOCUS(IDC_EDIT_AFTER_MINUTES, OnEnKillfocusEditAfterMinutes)
  ON_EN_KILLFOCUS(IDC_EDIT_WHEN_MOVED, OnEnKillfocusEditWhenMoved)
  ON_BN_CLICKED(IDC_NA_RUN_AT_OTHER, OnNaRunAtOther)
  ON_EN_KILLFOCUS(IDC_EDIT_GOTO_ITEM, OnEnKillfocusEditGotoItem)
  ON_BN_CLICKED(IDC_BUT_MOVE_UP, OnButMoveUp)
  ON_BN_CLICKED(IDC_BUT_MOVE_DOWN, OnButMoveDown)
  ON_BN_CLICKED(IDC_BUT_HIDE_ACTION, OnButHideAction)
  ON_BN_CLICKED(IDC_BUT_SHOW_ALL, OnButShowAll)
  ON_BN_CLICKED(IDC_BUT_DEFAULT_ORDER, OnButDefaultOrder)
  ON_BN_CLICKED(IDC_NA_RUN_AFTER_TASK, OnNaRunAfterTask)
  ON_BN_CLICKED(IDC_RGOTO_LABEL, OnRgotoLabel)
  ON_BN_CLICKED(IDC_RGOTO_NOTE, OnRgotoLabel)
  ON_BN_CLICKED(IDC_NA_SETUP_MULTISHOT, OnNaSetupMultishot)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_EVERY_N, OnDeltaposSpinEveryN)
  ON_BN_CLICKED(IDC_NA_CYCLE_DEFOCUS, OnNaCycleDefocus)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CYCLE_DEF, OnDeltaposSpinCycleDef)
  ON_BN_CLICKED(IDC_NA_EARLY_RETURN, OnNaEarlyReturn)
  ON_BN_CLICKED(IDC_NA_SKIP_SAVING, OnNaSkipSaving)
  ON_BN_CLICKED(IDC_NA_HIDE_UNUSED, OnNaHideUnused)
  ON_BN_CLICKED(IDC_RMAPPING, OnRadioCurParamSet)
  ON_BN_CLICKED(IDC_RACQUISITION, OnRadioCurParamSet)
  ON_BN_CLICKED(IDC_NA_SAVE_PARAMS, OnSaveParams)
  ON_BN_CLICKED(IDC_NA_READ_PARAMS, OnReadParams)
  ON_BN_CLICKED(IDC_NA_HIDE_OPTIONS, OnHideUnselectedOptions)
  ON_BN_CLICKED(IDC_RMAP_WITH_REC, OnMapWithRecViewSearch)
  ON_BN_CLICKED(IDC_RMAP_WITH_VIEW, OnMapWithRecViewSearch)
  ON_BN_CLICKED(IDC_RMAP_WITH_SEARCH, OnMapWithRecViewSearch)
  ON_BN_CLICKED(IDC_NA_SAVE_AS_MAP, OnSaveAsMap)
END_MESSAGE_MAP()

// CNavAcquireDlg message handlers
BOOL CNavAcquireDlg::OnInitDialog()
{
  CWnd *wnd, *stat;
  CRect rect;
  CFont *boldFont;
  CBaseDlg::OnInitDialog();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  int navState = mWinApp->mCameraMacroTools.GetNavigatorState();
  int ind, groupIDsToBold[] = {IDC_STAT_PRIMARY_GROUP, IDC_STAT_GEN_OPTIONS, 
    IDC_STAT_TASK_OPTIONS, IDC_STAT_ACTION_OPTIONS, IDC_STAT_ACTION_GROUP, 
    IDC_STAT_SELECTED_GROUP, IDC_STAT_PRIMARY_LINE};
  mNumActions = mWinApp->mNavHelper->GetNumAcqActions();
  mSecondColPanel = 1;
  SetupPanelTables(idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);

  // Set up floating items in the task table
  mAddItemIDs.push_back(IDC_STAT_PREMACRO);
  mAddItemIDs.push_back(IDC_COMBO_PREMACRO);
  mAddItemIDs.push_back(IDC_STAT_PRIMARY_LINE);
  mAddItemIDs.push_back(IDC_NA_TASK_LINE1);
  mAddItemIDs.push_back(IDC_NA_TASK_LINE2);
  mAddItemIDs.push_back(IDC_STAT_POSTMACRO);
  mAddItemIDs.push_back(IDC_COMBO_POSTMACRO);
  mAddUnitStartInds.push_back(0);
  mAddUnitStartInds.push_back(2);
  mAddUnitStartInds.push_back(5);

  // Set up to adjust group boxes
  // It cannot be drawn after the big redraw without obscuring tooltips, so set up for
  // baseDlg to add it to the big redraw
  /*mIDsToAdjustHeight.push_back(IDC_STAT_ACTION_GROUP);
  mIDsForNextTop.push_back(IDC_STAT_SPACER);
  mIDsToAdjustHeight.push_back(IDC_STAT_PRIMARY_GROUP);
  mIDsForNextTop.push_back(IDC_STAT_SPACER4);
  mIDsToAdjustHeight.push_back(IDC_STAT_TASK_OPTIONS);
  mIDsToAdjustHeight.push_back(IDC_STAT_ACTION_OPTIONS);
  mIDsToAdjustHeight.push_back(IDC_STAT_GEN_OPTIONS);
  mIDsForNextTop.push_back(IDC_STAT_ACTION_OPTIONS);
  mIDsForNextTop.push_back(IDC_STAT_GEN_OPTIONS);
  mIDsForNextTop.push_back(IDC_STAT_SPACER3);*/

  wnd = GetDlgItem(IDC_STAT_SAVING_FATE);
  wnd->GetWindowRect(rect);
  SetupUnitsToAdd(idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 
    -3 * rect.Height() / 4);
  boldFont = mWinApp->GetBoldFont(wnd);

    // Add IDs for first opening height glitches!
  // Not all these are currently needed
  mIDsToIgnoreBot.insert(IDC_NA_EARLY_RETURN);
  mIDsToReplaceHeight.insert(IDC_NA_SENDEMAIL);
  mIDsToIgnoreBot.insert(IDC_STAT_FILE_SAVING_INTO);
  mIDsToIgnoreBot.insert(IDC_STAT_ACTION_GROUP);
  mIDsToIgnoreBot.insert(IDC_STAT_WHEN_MOVED);
  mIDsToIgnoreBot.insert(IDC_RNAVACQ_WHEN_MOVED);
  mIDsToIgnoreBot.insert(IDC_NA_RUN_AFTER_TASK);  // 2862
  mIDsToIgnoreBot.insert(IDC_SPIN_EVERY_N);       // 2876
  mIDsToIgnoreBot.insert(IDC_NA_EARLY_RETURN);    // 2861
  mIDsToIgnoreBot.insert(IDC_CHECK_NAVACQ_RUN4);
  mIDsToIgnoreBot.insert(IDC_RACQUISITION);       // 2941
  mIDsToIgnoreBot.insert(IDC_STAT_CYCLE_UM);      // 2859
  mIDsToIgnoreBot.insert(IDC_STAT_FRAMES);
  mIDsToIgnoreBot.insert(IDC_STAT_SUBSET_TO);      // 2317
  mIDsToIgnoreBot.insert(IDC_RNAVACQ_EVERY_N);     // 2874
  mIDsToReplaceHeight.insert(IDC_EDIT_GOTO_ITEM);  // 2848

  // Set text for adjusting for image shift
  mOKtoAdjustBT = comaVsIS->magInd > 0;
  m_butAdjustBtforIS.SetWindowText(comaVsIS->magInd > 0 && comaVsIS->astigMat.xpx != 0.
    && !mWinApp->mNavHelper->GetSkipAstigAdjustment() ?
    "Adjust beam tilt && astig for image shift" :
    "Adjust beam tilt to compensate for image shift");

  // Make the selector button text bold & big
  stat = GetDlgItem(IDC_STAT_PARAM_FOR);
  if (stat) {
    stat->GetWindowRect(rect);
    mBoldFont.CreateFont(rect.Height(), 0, 0, 0, FW_BOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");

    wnd = GetDlgItem(IDC_RMAPPING);
    if (wnd)
      wnd->SetFont(&mBoldFont);
    wnd = GetDlgItem(IDC_RACQUISITION);
    if (wnd)
      wnd->SetFont(&mBoldFont);
  }

  for (ind = 0; ind < sizeof(groupIDsToBold) / sizeof(int); ind++) {
    wnd = GetDlgItem(groupIDsToBold[ind]);
    if (wnd)
      wnd->SetFont(boldFont);
  }

  // Set up spinners and combo boxes
  mPostponed = 0;
  m_sbcEveryN.SetRange(0, 10000);
  m_sbcEveryN.SetPos(5000);
  m_sbcCycleDef.SetRange(0, 10000);
  m_sbcCycleDef.SetPos(5000);
  m_iCurParamSet = mWinApp->mNavHelper->GetCurAcqParamIndex();
  mParam = &mAllCurParam[m_iCurParamSet];
  mActions = &mAllActions[m_iCurParamSet][0];
  mCurrentOrder = &mAllOrders[m_iCurParamSet][0];
  LoadMacrosIntoDropDown(m_comboPremacro, false, false);
  LoadMacrosIntoDropDown(m_comboPostmacro, false, false);
  LoadMacrosIntoDropDown(m_comboMacro, false, false);

  // Adjust some text
  if (mWinApp->mScope->GetNoColumnValve())
    ReplaceWindowText(&m_butCloseValves, "Close column valves", "Turn off filament");
  else if (JEOLscope)
    ReplaceWindowText(&m_butCloseValves, "column valves", "gun valve");
  else if (HitachiScope)
    m_butCloseValves.EnableWindow(false);
  m_butSendEmail.EnableWindow(mWinApp->mMailer->GetInitialized() &&
    !(mWinApp->mMailer->GetSendTo()).IsEmpty());
  m_butOKGO.SetWindowText(navState == NAV_PAUSED ? "Resume" : "GO");
  EnableDlgItem(IDC_RMAPPING, navState != NAV_PAUSED);
  EnableDlgItem(IDC_RACQUISITION, navState != NAV_PAUSED);

  // Get params in and set everything right
  for (int which = 0; which < 2; which++) {
    mMasterParam = mWinApp->GetNavAcqParams(which);
    mMasterOrder = mWinApp->mNavHelper->GetAcqActCurrentOrder(which);
    mMasterActions = mWinApp->mNavHelper->GetAcqActions(which);
    for (int act = 0; act < mNumActions; act++) {
      mAllActions[which][act] = mMasterActions[act];
      mAllOrders[which][act] = mMasterOrder[act];
    }
    mAllCurParam[which] = *mMasterParam;
  }

  LoadParamsToDialog();
  ManageEnables();
  ManageOutputFile();

  BuildActionSection();
  SetWindowPos(NULL, 0, 0, mBasicWidth, mSetToHeight, SWP_NOMOVE);
  return TRUE;
}

// OK: check for validity of other site item and of early return
void CNavAcquireDlg::OnOK()
{
  UpdateData(true);
  mActions[mCurActSelected].everyNitems = m_iEveryNitems;
  mActions[mCurActSelected].minutes = m_iAfterMinutes;
  mActions[mCurActSelected].distance = m_fWhenMoved;
  if (CheckNearestItemText()) {
    mPostponed = 0;
    return;
  }
  if (OptionsToAcquireType() == ACQUIRE_TAKE_MAP && m_bEarlyReturn && !m_iEarlyFrames &&
    mWinApp->GetHasK2OrK3Camera()) {
    AfxMessageBox("You must have a non-zero value for the number\n"
      "of early return frames when acquiring maps", MB_EXCLAME);
    mPostponed = 0;
    return;
  }
  if (CheckActionOrder(mCurrentOrder))
    return;

  // Copy all params back to masters
  UnloadDialogToCurParams();
  for (int which = 0; which < 2; which++) {
    mMasterParam = mWinApp->GetNavAcqParams(which);
    mMasterOrder = mWinApp->mNavHelper->GetAcqActCurrentOrder(which);
    mMasterActions = mWinApp->mNavHelper->GetAcqActions(which);
    for (int act = 0; act < mNumActions; act++) {
      mMasterActions[act] = mAllActions[which][act];
      mMasterOrder[act] = mAllOrders[which][act];
    }
    *mMasterParam = mAllCurParam[which];
  }

  mWinApp->mNavHelper->SetCurAcqParamIndex(m_iCurParamSet);
	CBaseDlg::OnOK();
  DoCancel();
}

// Postpone: set flag and OK
void CNavAcquireDlg::OnButPostpone()
{
 mPostponed = 1;
 OnOK();
}

// Cancel and external close
void CNavAcquireDlg::OnCancel()
{
  mPostponed = -1;
  DoCancel();
}

void CNavAcquireDlg::DoCancel()
{
  if (mWinApp->mNavigator)
    mWinApp->mNavigator->AcquireDlgClosing();
  DestroyWindow();
}

void CNavAcquireDlg::CloseWindow()
{
  OnCancel();
}

void CNavAcquireDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CNavAcquireDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// Check if actions occur in logical order
int CNavAcquireDlg::CheckActionOrder(int * order)
{
  int realigned = -1;
  int ind, act;
  for (ind = 0; ind < mNumActions; ind++) {
    act = order[ind];
    if (DOING_ACTION(act) && !(mActions[act].flags & NAA_FLAG_AFTER_ITEM)) {
      if (act == NAACT_ALIGN_TEMPLATE)
        realigned = act;
      if (act == NAACT_REALIGN_ITEM) {
        if (realigned >= 0) {
          if (AfxMessageBox("It is illogical to have Realign to Item after Align to\n"
            "Template because the Realign to Item will undo the\n"
            "alignment established by the Align to Template.\n\n"
            "Are you sure you want to proceed?", MB_QUESTION) == IDNO)
            return 1;
        }
        realigned = act;
      }

      if ((act == NAACT_ROUGH_EUCEN || act == NAACT_EUCEN_BY_FOCUS) && realigned >= 0) {
        if (AfxMessageBox("It is illogical to have " + mActions[act].name + " after\n"
          + mActions[realigned].name + " because the " + mActions[act].name + 
          "\nwill undo the alignment established by the " + mActions[realigned].name + 
          ".\n\n"
          "Are you sure you want to proceed?", MB_QUESTION) == IDNO)
          return 1;
      }

      if ((mActions[act].flags & NAA_FLAG_OTHER_SITE) && realigned > 0) {
        if (AfxMessageBox("It is illogical to have " + mActions[act].name + " after\n"
          + mActions[realigned].name + " because moving to another location\n"
          " for " + mActions[act].name + " will undo the alignment\n"
          "established by the " + mActions[realigned].name + ".\n\n"
          "Are you sure you want to proceed?", MB_QUESTION) == IDNO)
          return 1;
      }
    }
  }
  return 0;
}

// Save parameters
void CNavAcquireDlg::OnSaveParams()
{
  CString cPathname, filename;
  if (mWinApp->mDocWnd->GetTextFileName(false, false, cPathname, &filename))
    return;
  UnloadDialogToCurParams();
  mWinApp->mParamIO->WriteAcqParamsToFile(mParam, mActions, mCurrentOrder, cPathname);
}

// Read parameters
void CNavAcquireDlg::OnReadParams()
{
  CString cPathname, filename;
  if (mWinApp->mDocWnd->GetTextFileName(true, false, cPathname, &filename))
    return;
  mWinApp->mParamIO->ReadAcqParamsFromFile(mParam, mActions, mCurrentOrder, cPathname);
  if (!mAnyTSpoints && mAnyAcquirePoints)
    AcquireTypeToOptions(mParam->nonTSacquireType);
  if (mAnyTSpoints && !mAnyAcquirePoints)
    AcquireTypeToOptions(ACQUIRE_DO_TS);
  LoadParamsToDialog();
  ManageEnables();
  ManageOutputFile();
  BuildActionSection();
}

// Get items from dialog that are stored separately for TS and nonTS
void CNavAcquireDlg::UnloadTSdependentFromDlg(int acquireType)
{
  if (acquireType == ACQUIRE_DO_TS) {
    mParam->preMacroInd = mPremacNum;
    mParam->postMacroInd = mPostmacNum;
    mParam->runPremacro = DOING_ACTION(NAACT_RUN_PREMACRO);
    mParam->runPostmacro = DOING_ACTION(NAACT_RUN_POSTMACRO);
    mParam->sendEmail = m_bSendEmail;
  } else {
    mParam->preMacroIndNonTS = mPremacNum;
    mParam->postMacroIndNonTS = mPostmacNum;
    mParam->runPremacroNonTS = DOING_ACTION(NAACT_RUN_PREMACRO);
    mParam->runPostmacroNonTS = DOING_ACTION(NAACT_RUN_POSTMACRO);
    mParam->sendEmailNonTS = m_bSendEmail;
  }
}

// Load items into dialog that are stored separately for TS and nonTS
void CNavAcquireDlg::LoadTSdependentToDlg(void)
{
  if (OptionsToAcquireType() == ACQUIRE_DO_TS) {
    mPremacNum = mParam->preMacroInd;
    mPostmacNum = mParam->postMacroInd;
    SET_ACTION(mActions, NAACT_RUN_PREMACRO, mParam->runPremacro);
    SET_ACTION(mActions, NAACT_RUN_POSTMACRO, mParam->runPostmacro);
    m_bSendEmail = mParam->sendEmail;
  } else {
    mPremacNum = mParam->preMacroIndNonTS;
    mPostmacNum = mParam->postMacroIndNonTS;
    SET_ACTION(mActions, NAACT_RUN_PREMACRO, mParam->runPremacroNonTS);
    SET_ACTION(mActions, NAACT_RUN_POSTMACRO, mParam->runPostmacroNonTS);
    m_bSendEmail = mParam->sendEmailNonTS;
  }
  B3DCLAMP(mPremacNum, 1, MAX_MACROS);
  B3DCLAMP(mPostmacNum, 1, MAX_MACROS);
  ManageMacro();
}

// Get dialog elements into current parameter set
void CNavAcquireDlg::UnloadDialogToCurParams()
{
  mParam->cycleDefocus = m_bCycleDefocus;
  mParam->cycleDefTo = m_fCycleTo;
  mParam->cycleDefFrom = m_fCycleFrom;
  mParam->noMBoxOnError = m_bNoMBoxOnError;
  mParam->earlyReturn = m_bEarlyReturn;
  mParam->numEarlyFrames = m_iEarlyFrames;
  mParam->macroIndex = mMacroNum;
  mParam->skipInitialMove = m_bSkipInitialMove;
  mParam->skipZmoves = m_bSkipZmoves;
  mParam->skipSaving = m_bSkipSaving;
  mParam->closeValves = m_bCloseValves;
  mParam->sendEmail = m_bSendEmail;
  mParam->adjustBTforIS = m_bAdjustBTforIS;
  mParam->hideUnusedActs = m_bHideUnusedActions;
  mParam->acqDlgSelectedPos = m_iSelectedPos;
  mParam->relaxStage = m_bRelaxStage;
  mParam->hybridRealign = m_bHybridRealign;
  mParam->hideUnselectedOpts = m_bHideUnselectedOpts;
  mParam->acquireType = OptionsToAcquireType();
  if (mParam->acquireType != ACQUIRE_DO_TS)
    mParam->nonTSacquireType = mParam->acquireType;
  mParam->saveAsMapChoice = m_bSaveAsMap;
  mParam->mapWithViewSearch = m_iMapWithViewSearch;
  UnloadTSdependentFromDlg(OptionsToAcquireType());
}

// Get parameters from current set into dialog
void CNavAcquireDlg::LoadParamsToDialog()
{
  m_bHideUnusedActions = mParam->hideUnusedActs;
  m_iSelectedPos = mParam->acqDlgSelectedPos;
  mMacroNum = mParam->macroIndex;
  m_bSkipInitialMove = mParam->skipInitialMove;
  m_bSkipZmoves = mParam->skipZmoves;
  m_bCloseValves = mParam->closeValves;
  m_fCycleFrom = mParam->cycleDefFrom;
  m_fCycleTo = mParam->cycleDefTo;
  m_bCycleDefocus = mParam->cycleDefocus;
  m_strCycleUm.Format("um in %d", mParam->cycleSteps);
  m_bNoMBoxOnError = mParam->noMBoxOnError;
  m_bEarlyReturn = mParam->earlyReturn;
  m_iEarlyFrames = mParam->numEarlyFrames;
  m_bSkipSaving = mParam->skipSaving;
  m_bAdjustBTforIS = mParam->adjustBTforIS;
  m_bRelaxStage = mParam->relaxStage;
  m_bHybridRealign = mParam->hybridRealign;
  m_bHideUnselectedOpts = mParam->hideUnselectedOpts;
  m_iMapWithViewSearch = mParam->mapWithViewSearch;
  mLastNonTStype = mParam->nonTSacquireType;
  // m_iAcquireChoice is handled by caller
  if (m_iAcquireChoice)
    m_bSaveAsMap = mParam->saveAsMapChoice;
  LoadTSdependentToDlg();
}

// Switching the parameter set: unload into current set, change points, load and rebuild
void CNavAcquireDlg::OnRadioCurParamSet()
{
  int oldCur = m_iCurParamSet;
  UpdateData(true);
  if (CheckActionOrder(mCurrentOrder)) {
    m_iCurParamSet = oldCur;
    UpdateData(false);
    return;
  }
  UnloadDialogToCurParams();
  mParam = &mAllCurParam[m_iCurParamSet];
  mActions = &mAllActions[m_iCurParamSet][0];
  mCurrentOrder = &mAllOrders[m_iCurParamSet][0];
  LoadParamsToDialog();
  AcquireTypeToOptions(mParam->acquireType);
  if (!mAnyTSpoints && mAnyAcquirePoints)
    AcquireTypeToOptions(mParam->nonTSacquireType);
  if (mAnyTSpoints && !mAnyAcquirePoints)
    AcquireTypeToOptions(ACQUIRE_DO_TS);
  ManageEnables();
  ManageOutputFile();
  BuildActionSection();
}

// Change in primary task
void CNavAcquireDlg::OnNaAcquiremap()
{
  int oldAcquire = OptionsToAcquireType();
  UpdateData(true);
  UnloadTSdependentFromDlg(oldAcquire);
  LoadTSdependentToDlg();
  ManageEnables();
  ManageMacro();
  ManageOutputFile();
}

// Checkbox to make a map
void CNavAcquireDlg::OnSaveAsMap()
{
  UpdateData(true);
  ManageEnables();
  ManageOutputFile();
}

// Functions to get back and forth between 4 radios & checkbox and 5 defined actions
void CNavAcquireDlg::AcquireTypeToOptions(int acqType)
{
  m_iAcquireChoice = acqType > 0 ? acqType - 1 : 0;
  if (acqType < 2)
    m_bSaveAsMap = !acqType;
}

int CNavAcquireDlg::OptionsToAcquireType()
{
  return B3DCHOICE(m_iAcquireChoice > 0, m_iAcquireChoice + 1, m_bSaveAsMap ? 0 : 1);
}

// Macro management and selections
void CNavAcquireDlg::ManageMacro(void)
{
  m_comboPremacro.SetCurSel(mPremacNum - 1);
  m_comboPostmacro.SetCurSel(mPostmacNum - 1);
  m_comboMacro.SetCurSel(mMacroNum - 1);
  m_comboMacro.EnableWindow(OptionsToAcquireType() == ACQUIRE_RUN_MACRO);
  m_comboPremacro.EnableWindow(DOING_ACTION(NAACT_RUN_PREMACRO));
  m_comboPostmacro.EnableWindow(DOING_ACTION(NAACT_RUN_POSTMACRO));
  EnableDlgItem(IDC_STAT_PREMACRO, DOING_ACTION(NAACT_RUN_PREMACRO));
  EnableDlgItem(IDC_STAT_POSTMACRO, DOING_ACTION(NAACT_RUN_POSTMACRO));
  m_comboPostmacro.EnableWindow(DOING_ACTION(NAACT_RUN_POSTMACRO));
  UpdateData(false);
}

void CNavAcquireDlg::OnSelendokComboPremacro()
{
  mPremacNum = m_comboPremacro.GetCurSel() + 1;
}

void CNavAcquireDlg::OnSelendokComboPostmacro()
{
  mPostmacNum = m_comboPostmacro.GetCurSel() + 1;
}

void CNavAcquireDlg::OnSelendokComboMacro()
{
  mMacroNum = m_comboMacro.GetCurSel() + 1;
}

// Subset entries
void CNavAcquireDlg::OnDoSubset()
{
  UpdateData(true);
  ManageForSubset();
}

void CNavAcquireDlg::OnKillfocusSubsetStart()
{
  UpdateData(true);
  B3DCLAMP(m_iSubsetStart, 1, mNumArrayItems);
  ManageForSubset();
}

void CNavAcquireDlg::OnKillfocusSubsetEnd()
{
  UpdateData(true);
  B3DCLAMP(m_iSubsetEnd, 1, mNumArrayItems);
  ManageForSubset();
}

// Skipping initial move
void CNavAcquireDlg::OnSkipInitialMove()
{
  UpdateData(true);
  ManageOutputFile();
}

// Set text about the output file or lack of output
void CNavAcquireDlg::ManageOutputFile(void)
{
  CString dir;
  bool allZeroER;
  int acquireType = OptionsToAcquireType();
  m_strFileSavingInto = "";
  m_strSavingFate = "";
  BOOL multiSaving = false;
  if (acquireType == ACQUIRE_MULTISHOT) {
    multiSaving = mWinApp->mNavHelper->IsMultishotSaving(&allZeroER);
    if (!multiSaving && allZeroER)
      m_strSavingFate = "NO SAVING: All 0-frame early returns in multishot";
    else if (!multiSaving)
      m_strSavingFate = "NO SAVING: \"Save Record\" not on in multishot";
  }
  if (mWinApp->GetHasK2OrK3Camera() && m_bEarlyReturn && !m_iEarlyFrames &&
    acquireType == ACQUIRE_IMAGE_ONLY) {
    m_strSavingFate = "NO SAVING: \"Early return\" with 0 frames selected";
  } else if (m_bSkipSaving && acquireType == ACQUIRE_IMAGE_ONLY) {
    m_strSavingFate = "NO SAVING: \"Skip saving\" option selected";
  } else if (acquireType == ACQUIRE_TAKE_MAP || acquireType == ACQUIRE_IMAGE_ONLY ||
    multiSaving) {
    if (mNumAcqBeforeFile && mOutputFile.IsEmpty()) {
      m_strSavingFate = "No output file is open for first acquire item!";
      m_strFileSavingInto = "Open a file or Postpone to set one up to open";
    } else if (multiSaving && mIfMontOutput) {
      m_strSavingFate = "Multiple Records cannot be saved to a montage file";
      if (mNumFilesToOpen)
        m_strFileSavingInto = "Postpone and set up to open single-frame files";
      else
        m_strFileSavingInto = "Make a single-frame file be the current one";
    } else {
      m_strSavingFate.Format("%s will%s be saved into the file:", 
        (!multiSaving && mIfMontOutput) ? "MONTAGES" : "IMAGES", 
        ((mNumAcqBeforeFile && mNumFilesToOpen) || mNumFilesToOpen > 1) ? 
        " initially" : "");
      UtilSplitPath(mOutputFile, dir, m_strFileSavingInto);
    }
  } else if (m_bSkipInitialMove && mWinApp->mNavigator->OKtoSkipStageMove(mActions,
    acquireType) != 0) {
      m_strFileSavingInto = "Relying on script to run Realign or move stage";
  }
  if (!mAnyAcquirePoints && !mAnyTSpoints) {
    m_strSavingFate = "Nothing will be acquired with this subset";
  }
  UpdateData(false);
}

// Take care of all the state-dependent enabling except the timing section
void CNavAcquireDlg::ManageEnables(void)
{
  int acquireType = OptionsToAcquireType();
  bool imageOrMap = acquireType == ACQUIRE_IMAGE_ONLY ||
    acquireType == ACQUIRE_TAKE_MAP;
  DriftWaitParams *dwParam = mWinApp->mParticleTasks->GetDriftWaitParams();
  bool cycleOK = acquireType == ACQUIRE_DO_TS || acquireType == ACQUIRE_RUN_MACRO ||
    DOING_ACTION(NAACT_RUN_POSTMACRO) || DOING_ACTION(NAACT_RUN_PREMACRO) ||
    DOING_ACTION(NAACT_AUTOFOCUS) || 
    (DOING_ACTION(NAACT_WAIT_DRIFT) && dwParam->measureType == WFD_WITHIN_AUTOFOC);
  bool consetOK = mWinApp->LowDoseMode() && 
    (acquireType == ACQUIRE_TAKE_MAP || acquireType == ACQUIRE_IMAGE_ONLY);
  m_butSetupMultishot.EnableWindow(acquireType == ACQUIRE_MULTISHOT);
  m_editSubsetStart.EnableWindow(m_bDoSubset);
  m_editSubsetEnd.EnableWindow(m_bDoSubset);
  m_butAcquireTS.EnableWindow(mAnyTSpoints);
  m_butAcquireMap.EnableWindow(mAnyAcquirePoints);
  m_butSaveAsMap.EnableWindow(mAnyAcquirePoints && !m_iAcquireChoice);
  m_butDoMultishot.EnableWindow(mAnyAcquirePoints);
  m_butRunMacro.EnableWindow(mAnyAcquirePoints);
  m_butSkipInitialMove.EnableWindow(mWinApp->mNavigator->OKtoSkipStageMove(mActions, 
    acquireType) != 0);
  EnableDlgItem(IDC_NA_CYCLE_DEFOCUS, cycleOK);
  m_editCycleFrom.EnableWindow(m_bCycleDefocus && cycleOK);
  m_editCycleTo.EnableWindow(m_bCycleDefocus && cycleOK);
  m_sbcCycleDef.EnableWindow(m_bCycleDefocus && cycleOK);
  EnableDlgItem(IDC_STAT_CYCLE_UM, m_bCycleDefocus && cycleOK);
  EnableDlgItem(IDC_STAT_CYCLE_STEPS, m_bCycleDefocus && cycleOK);
  m_editEarlyFrames.EnableWindow(m_bEarlyReturn && imageOrMap);
  m_butEarlyReturn.EnableWindow(imageOrMap);
  EnableDlgItem(IDC_STAT_FRAMES, imageOrMap);
  m_butAdjustBtforIS.EnableWindow(imageOrMap && mOKtoAdjustBT);
  m_butSkipSaving.EnableWindow(acquireType == ACQUIRE_IMAGE_ONLY);
  m_butHybridRealign.EnableWindow(DOING_ACTION(NAACT_REALIGN_ITEM) && 
    DOING_ACTION(NAACT_ALIGN_TEMPLATE) &&
    mActions[NAACT_ALIGN_TEMPLATE].timingType == NAA_EVERY_N_ITEMS &&
    mActions[NAACT_ALIGN_TEMPLATE].everyNitems == 1);
  EnableDlgItem(IDC_STAT_WHICH_CONSET, consetOK);
  EnableDlgItem(IDC_RMAP_WITH_REC, consetOK);
  EnableDlgItem(IDC_RMAP_WITH_VIEW, consetOK);
  EnableDlgItem(IDC_RMAP_WITH_SEARCH, consetOK);
  ManageOutputFile();
}

// For disabling/enabling action buttons when something else happens
void CNavAcquireDlg::ExternalUpdate()
{
  BOOL enable = (!mWinApp->DoingTasks() || mWinApp->GetJustNavAcquireOpen()) && 
    !mWinApp->mCamera->CameraBusy();
  EnableDlgItem(IDOK, enable);
  EnableDlgItem(IDC_BUT_POSTPONE, enable);
  EnableDlgItem(IDC_NA_READ_PARAMS, enable);
  EnableDlgItem(IDC_NA_SAVE_PARAMS, enable);
}

// When a subset is selected/changed, re-evaluate the acquisition and switch acquire type
// if appropriate
void CNavAcquireDlg::ManageForSubset(void)
{
  int acquireType = OptionsToAcquireType();
  mWinApp->mNavigator->EvaluateAcquiresForDlg(this);
  if (acquireType == ACQUIRE_DO_TS && !mAnyTSpoints && mAnyAcquirePoints)
    acquireType = mLastNonTStype;
  if (acquireType != ACQUIRE_DO_TS && mAnyTSpoints && !mAnyAcquirePoints) {
    mLastNonTStype = acquireType;
    acquireType = ACQUIRE_DO_TS;
  }
  ManageEnables();
  ManageOutputFile();
  UpdateData(false);
}

// Put the specific tasks into the general buttons in the task list
void CNavAcquireDlg::BuildActionSection(void)
{
  int ind, actInd, pos = 0, loop;
  CButton *button;
  CRect actRect;
  bool runIt;
  BOOL states[5] = {true, true, true, true, true};
  int acquireType = OptionsToAcquireType();
  Invalidate();

  // Leave out the hidden ones and configure the rest; loop on before then after task
  mIDsToDrop = mAddItemIDs;
  mAddUnitAfterIDs.resize(0);
  mAddUnitAfterIDs.resize(mNumUnitsToAdd, -1);
  mAddAfterIDSet.clear();
  for (loop = 0; loop < 2; loop++) {
    for (ind = 0; ind < mNumActions; ind++) {
      actInd = mCurrentOrder[ind];
      if (!BOOL_EQUIV(mActions[actInd].flags & NAA_FLAG_AFTER_ITEM, loop > 0))
        continue;
      runIt = (mActions[actInd].flags & NAA_FLAG_RUN_IT) != 0;
      if ((mActions[actInd].flags & (NAA_FLAG_ALWAYS_HIDE | NAA_FLAG_HIDE_IT)) == 0 &&
        (!m_bHideUnusedActions || runIt)) {

        // If any action is shown, load it into the next position
        button = (CButton *)GetDlgItem(IDC_RADIO_NAVACQ_SEL1 + pos);
        button->SetWindowText(mActions[actInd].name);
        button = (CButton *)GetDlgItem(IDC_CHECK_NAVACQ_RUN1 + pos);
        button->SetWindowText("");
        button->SetCheck(runIt ? BST_CHECKED : BST_UNCHECKED);
        FormatTimingString(actInd, pos);
        EnableDlgItem(IDC_STAT_NAVACQ_WHEN1 + pos, runIt);

        // Show or drop setup button
        if (mActions[actInd].flags & NAA_FLAG_HAS_SETUP)
          EnableDlgItem(IDC_BUT_NAVACQ_SETUP1 + pos, runIt ? 1 : 0);
        else
          mIDsToDrop.push_back(IDC_BUT_NAVACQ_SETUP1 + pos);

        // Add a macro when it is time
        if (actInd == NAACT_RUN_PREMACRO || actInd == NAACT_RUN_POSTMACRO) {
          mAddUnitAfterIDs[actInd == NAACT_RUN_PREMACRO ? 0 : 2] = 
            IDC_STAT_NAVACQ_WHEN1 + pos;
          mAddAfterIDSet.insert(IDC_STAT_NAVACQ_WHEN1 + pos);
        }

        // Keep track of current action and build up maps
        if (actInd == mCurActSelected && m_iSelectedPos >= 0)
          m_iSelectedPos = pos;
        mUnhiddenPosMap[pos] = ind;
        mShownPosToIndex[pos++] = mCurrentOrder[ind];
      }
    }

    // Add the task after the things run before
    if (!loop) {
      ind = IDC_STAT_ACTION_GROUP;
      mFirstPosAfterTask = pos;
      if (pos > 0)
        //ind = mShownPosToIndex[pos - 1] == NAACT_RUN_PREMACRO ? IDC_STAT_PREMACRO :
        ind = IDC_STAT_NAVACQ_WHEN1 + pos - 1;
      mAddUnitAfterIDs[1] = ind;
      mAddAfterIDSet.insert(ind);
    }
  }
    
  // Drop the rest of the buttons etc
  mNumShownActs = pos;
  for (; pos < mNumActions; pos++) {
    mIDsToDrop.push_back(IDC_CHECK_NAVACQ_RUN1 + pos);
    mIDsToDrop.push_back(IDC_STAT_NAVACQ_WHEN1 + pos);
    mIDsToDrop.push_back(IDC_RADIO_NAVACQ_SEL1 + pos);
    mIDsToDrop.push_back(IDC_BUT_NAVACQ_SETUP1 + pos);
  }

  ACCUM_MIN(m_iSelectedPos, mNumShownActs - 1);
  //if (mShownPosToIndex[m_iSelectedPos] != mCurActSelected)
  if (m_iSelectedPos >= 0)
    NewActionSelected(m_iSelectedPos);

  // Add any permanently dropped items now
  if (!mWinApp->GetHasK2OrK3Camera() || (m_bHideUnselectedOpts && !m_bEarlyReturn)) {
    mIDsToDrop.push_back(IDC_NA_EARLY_RETURN);
    mIDsToDrop.push_back(IDC_STAT_FRAMES);
    mIDsToDrop.push_back(IDC_EDIT_EARLY_FRAMES);
  }

  // Now drop unselected items
  if (m_bHideUnselectedOpts) {
    if (!m_bCycleDefocus) {
      mIDsToDrop.push_back(IDC_NA_CYCLE_DEFOCUS);
      mIDsToDrop.push_back(IDC_EDIT_CYCLE_FROM);
      mIDsToDrop.push_back(IDC_EDIT_CYCLE_TO);
      mIDsToDrop.push_back(IDC_STAT_CYCLE_TO);
      mIDsToDrop.push_back(IDC_STAT_CYCLE_FROM);
      mIDsToDrop.push_back(IDC_STAT_CYCLE_UM);
      mIDsToDrop.push_back(IDC_STAT_CYCLE_STEPS);
      mIDsToDrop.push_back(IDC_SPIN_CYCLE_DEF);
    }
    if (!m_bAdjustBTforIS)
      mIDsToDrop.push_back(IDC_NA_ADJUST_BT_FOR_IS);
    if (!m_bHybridRealign)
      mIDsToDrop.push_back(IDC_NA_APPLY_REALIGN_ERR);
    if (!m_bRelaxStage)
    mIDsToDrop.push_back(IDC_NA_RELAX_STAGE);
    if (!m_bSkipZmoves)
      mIDsToDrop.push_back(IDC_NA_SKIP_Z_MOVES);
    if (!m_bSkipInitialMove)
      mIDsToDrop.push_back(IDC_NA_SKIP_INITIAL_MOVE);
    if (!m_bDoSubset) {
      mIDsToDrop.push_back(IDC_NA_DO_SUBSET);
      mIDsToDrop.push_back(IDC_STAT_SUBSET_TO);
      mIDsToDrop.push_back(IDC_EDIT_SUBSET_START);
      mIDsToDrop.push_back(IDC_EDIT_SUBSET_END);
    }
    if (!m_bNoMBoxOnError)
      mIDsToDrop.push_back(IDC_NA_NO_MBOX_ON_ERROR);
    if (!m_bCloseValves)
      mIDsToDrop.push_back(IDC_NA_CLOSE_VALVES);
    if (!m_bSendEmail)
      mIDsToDrop.push_back(IDC_NA_SENDEMAIL);
    if (m_iAcquireChoice > 0) {
      mIDsToDrop.push_back(IDC_NA_ACQUIREMAP);
      mIDsToDrop.push_back(IDC_NA_SAVE_AS_MAP);
      mIDsToDrop.push_back(IDC_NA_SKIP_SAVING);
    }
    if (acquireType != ACQUIRE_MULTISHOT) {
      mIDsToDrop.push_back(IDC_NA_DO_MULTISHOT);
      mIDsToDrop.push_back(IDC_NA_SETUP_MULTISHOT);
    }
    if (acquireType != ACQUIRE_DO_TS)
      mIDsToDrop.push_back(IDC_NA_ACQUIRE_TS);
    if (acquireType != ACQUIRE_RUN_MACRO) {
      mIDsToDrop.push_back(IDC_NA_RUNMACRO);
      mIDsToDrop.push_back(IDC_COMBO_MACRO);
    }

  }

  // Reform the window and set the group box
  states[2] = m_iSelectedPos >= 0;
  AdjustPanels(states, idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
}

// Make text describing when an action runs
void CNavAcquireDlg::FormatTimingString(int actInd, int posInd)
{
  CString str;
  switch (mActions[actInd].timingType) {
  case NAA_EVERY_N_ITEMS:
    if (mActions[actInd].everyNitems <= 1)
      str = "every item";
    else
      str.Format("Every %d items", mActions[actInd].everyNitems);
    break;
  case NAA_GROUP_START:
    str = "At start of group";
    break;
  case NAA_GROUP_END:
    str = "At end of group";
    break;
  case NAA_AFTER_TIME:
    str.Format("Every %d min", mActions[actInd].minutes);
    break;

  case NAA_IF_SEPARATED:
    str.Format("If moved > %.1f um", mActions[actInd].distance);
    break;

  }

  if (mActions[actInd].flags & NAA_FLAG_AFTER_ITEM)
    str += ", after";
  SetDlgItemText(IDC_STAT_NAVACQ_WHEN1 + posInd, str);
  UpdateData(false);
}

// A different action is selected for editing timing
void CNavAcquireDlg::NewActionSelected(int posInd)
{
  int ind;
  CButton *button;
  int actInd = mShownPosToIndex[posInd];
  NavAcqAction *act = &mActions[actInd];
  m_statSelectedGroup.SetWindowText("General controls for task: " + act->name);

  // Load everything from the dialog to the action: the previous action is assumed to be
  // kept up to date
  if (act->flags & NAA_FLAG_EVERYN_ONLY) {
    act->timingType = NAA_EVERY_N_ITEMS;
    setOrClearFlags(&act->flags, NAA_FLAG_OTHER_SITE, 0);
  }
  m_iTimingType = act->timingType;
  m_iEveryNitems = act->everyNitems;
  m_iAfterMinutes = act->minutes;
  m_fWhenMoved = act->distance;
  m_bRunAtOther = (act->flags & NAA_FLAG_OTHER_SITE) != 0;
  m_iGotoLabelNote = (act->flags & NAA_FLAG_MATCH_NOTE) ? 1 : 0;
  m_strGotoItem = act->labelOrNote;
  m_bRunAfterTask = (act->flags & NAA_FLAG_AFTER_ITEM) != 0;
  m_iSelectedPos = posInd;
  mCurActSelected = actInd;
  for (ind = 0; ind < mNumShownActs; ind++) {
    button = (CButton *)GetDlgItem(IDC_RADIO_NAVACQ_SEL1 + ind);
    if (button)
      button->SetCheck(ind == posInd ? BST_CHECKED : BST_UNCHECKED);
  }

  ManageTimingEnables();
  UpdateData(false);
}

// Manage the enabling of the action-specific editing items
void CNavAcquireDlg::ManageTimingEnables()
{
  CButton radio;
  NavAcqAction *act = &mActions[mCurActSelected];
  bool runIt = (act->flags & NAA_FLAG_RUN_IT) != 0;
  bool anywhere = (act->flags & NAA_FLAG_ANY_SITE_OK) != 0 ||
    (act->flags & NAA_FLAG_HERE_ONLY) != 0;
  bool notOnlyEveryN = runIt && (act->flags & NAA_FLAG_EVERYN_ONLY) == 0;
  m_editEveryN.EnableWindow(act->timingType == NAA_EVERY_N_ITEMS && runIt);
  m_sbcEveryN.EnableWindow(act->timingType == NAA_EVERY_N_ITEMS && runIt);
  m_editAfterMinutes.EnableWindow(act->timingType == NAA_AFTER_TIME && notOnlyEveryN);
  m_editWhenMoved.EnableWindow(act->timingType == NAA_IF_SEPARATED && notOnlyEveryN);
  m_editGotoItem.EnableWindow(notOnlyEveryN && !anywhere);
  m_butMoveUp.EnableWindow(m_iSelectedPos > 0);// && m_iSelectedPos != mFirstPosAfterTask);
  m_butMoveDown.EnableWindow(m_iSelectedPos < mNumShownActs - 1 && 
    !(m_iSelectedPos == mFirstPosAfterTask - 1 && (act->flags & NAA_FLAG_ONLY_BEFORE)));
  EnableDlgItem(IDC_RNAVACQ_EVERY_N, runIt);
  EnableDlgItem(IDC_RNAVACQ_GROUP_START, notOnlyEveryN);
  EnableDlgItem(IDC_RNAVACQ_GROUP_END, notOnlyEveryN);
  EnableDlgItem(IDC_RNAVACQ_AFTER_MINUTES, notOnlyEveryN);
  EnableDlgItem(IDC_RNAVACQ_WHEN_MOVED, notOnlyEveryN);
  EnableDlgItem(IDC_STAT_EVERY_N, runIt);
  EnableDlgItem(IDC_STAT_AFTER_MINUTES, notOnlyEveryN);
  EnableDlgItem(IDC_STAT_WHEN_MOVED, notOnlyEveryN);
  EnableDlgItem(IDC_NA_RUN_AT_OTHER, notOnlyEveryN && !anywhere);
  EnableDlgItem(IDC_RGOTO_LABEL, m_bRunAtOther && notOnlyEveryN && !anywhere);
  EnableDlgItem(IDC_RGOTO_NOTE, m_bRunAtOther && notOnlyEveryN && !anywhere);
  EnableDlgItem(IDC_NA_RUN_AFTER_TASK, !(act->flags & NAA_FLAG_ONLY_BEFORE) && runIt);
}

// New timing type is selected
void CNavAcquireDlg::OnRadioTimingType()
{
  UpdateData(true);
  mActions[mCurActSelected].timingType = m_iTimingType;
  FormatTimingString(mCurActSelected, m_iSelectedPos);
  ManageTimingEnables();
  ManageEnables();
}

// An action is turned on or off
void CNavAcquireDlg::OnRadioSelectAction(UINT nID)
{
  BOOL states[5] = {true, true, true, true, true};
  CButton *button;
  int oldSelected = m_iSelectedPos;
  UpdateData(true);
  if (CheckNearestItemText()) {
    button = (CButton *)GetDlgItem(IDC_RADIO_NAVACQ_SEL1 + m_iSelectedPos);
    if (button)
      button->SetCheck(BST_CHECKED);
    button = (CButton *)GetDlgItem(nID);
    if (button)
      button->SetCheck(BST_UNCHECKED);
    return;
  }
  if (m_iSelectedPos == nID - IDC_RADIO_NAVACQ_SEL1) {
    button = (CButton *)GetDlgItem(nID);
    if (button)
      button->SetCheck(BST_UNCHECKED);
    m_iSelectedPos = -1;
  } else {
    NewActionSelected(nID - IDC_RADIO_NAVACQ_SEL1);
  }
  if (oldSelected < 0 || m_iSelectedPos < 0) {
    states[2] = m_iSelectedPos >= 0;
    AdjustPanels(states, idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
      sHeightTable);
  }
}

// New interval for every N
void CNavAcquireDlg::OnEnKillfocusEditEveryN()
{
  UpdateData(true);
  mActions[mCurActSelected].everyNitems = m_iEveryNitems;
  FormatTimingString(mCurActSelected, m_iSelectedPos);
  ManageEnables();
}

// New time for running after an interval
void CNavAcquireDlg::OnEnKillfocusEditAfterMinutes()
{
  UpdateData(true);
  mActions[mCurActSelected].minutes = m_iAfterMinutes;
  FormatTimingString(mCurActSelected, m_iSelectedPos);
}

// New distance for running after moving
void CNavAcquireDlg::OnEnKillfocusEditWhenMoved()
{
  UpdateData(true);
  mActions[mCurActSelected].distance = m_fWhenMoved;
  FormatTimingString(mCurActSelected, m_iSelectedPos);
}

// Running elsewhere is toggled
void CNavAcquireDlg::OnNaRunAtOther()
{
  UpdateData(true);
  setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_OTHER_SITE, 
    m_bRunAtOther ? 1 : 0);
  ManageTimingEnables();
}

// New starting text for lable/note
void CNavAcquireDlg::OnEnKillfocusEditGotoItem()
{
  UpdateData(true);
  mActions[mCurActSelected].labelOrNote = m_strGotoItem;
}

// Make sure there is some text if needed
int CNavAcquireDlg::CheckNearestItemText()
{ 
  if (!m_bRunAtOther || !m_strGotoItem.IsEmpty() || 
    !(mActions[mCurActSelected].flags & NAA_FLAG_RUN_IT))
    return 0;
  AfxMessageBox("You must fill in the starting text for the " + 
    CString(m_iGotoLabelNote ? "note" : "label") + 
    "\nor turn off \"Run at nearest item\"", MB_EXCLAME);
  return 1;
}

// Run action after task: rebuild everything
void CNavAcquireDlg::OnNaRunAfterTask()
{
  UpdateData(true);
  setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_AFTER_ITEM,
    m_bRunAfterTask ? 1 : 0);
  BuildActionSection();
//  FormatTimingString(mCurActSelected, m_iSelectedPos);
}

// Switch between label and note
void CNavAcquireDlg::OnRgotoLabel()
{
  UpdateData(true);
  setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_MATCH_NOTE,
    m_iGotoLabelNote);
}

// Move an action up or down in order
void CNavAcquireDlg::OnButMoveUp()
{
  MoveAction(-1);
}


void CNavAcquireDlg::OnButMoveDown()
{
  MoveAction(1);
}

void CNavAcquireDlg::MoveAction(int dir)
{
  int temp;
  if (m_iSelectedPos + dir < 0 || m_iSelectedPos + dir >= mNumShownActs)
    return;
  if (m_iSelectedPos == mFirstPosAfterTask && dir < 0)
    setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_AFTER_ITEM, 0);
  else if (m_iSelectedPos == mFirstPosAfterTask - 1 && dir > 0)
    setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_AFTER_ITEM, 1);
  else
    B3DSWAP(mCurrentOrder[mUnhiddenPosMap[m_iSelectedPos]],
      mCurrentOrder[mUnhiddenPosMap[m_iSelectedPos + dir]], temp);
  m_iSelectedPos += dir;
  BuildActionSection();
}

// Hide the un-run actions
void CNavAcquireDlg::OnNaHideUnused()
{
  UpdateData(true);
  BuildActionSection();
}

// Hide the selected action
void CNavAcquireDlg::OnButHideAction()
{
  if (CheckNearestItemText())
    return;
  setOrClearFlags(&mActions[mCurActSelected].flags, NAA_FLAG_HIDE_IT, 1);
  if (m_iSelectedPos >= mNumShownActs - 1)
    m_iSelectedPos--;
  Invalidate();
  BuildActionSection();
}

// Show all actions that were hidden, turn of hiding unused ones
void CNavAcquireDlg::OnButShowAll()
{
  for (int ind = 0; ind < mNumActions; ind++)
    setOrClearFlags(&mActions[ind].flags, NAA_FLAG_HIDE_IT, 0);
  m_bHideUnusedActions = false;
  UpdateData(false);
  BuildActionSection();
}

// Restore the default order
void CNavAcquireDlg::OnButDefaultOrder()
{
  int *dfltOrder = mWinApp->mNavHelper->GetAcqActDefaultOrder();
  for (int ind = 0; ind < mNumActions; ind++)
    mCurrentOrder[ind] = dfltOrder[ind];
  BuildActionSection();
}

// An action is selected to run
void CNavAcquireDlg::OnCheckRunAction(UINT nID)
{
  CButton *button = (CButton *)GetDlgItem(nID);
  if (!button)
    return;
  UpdateData(true);
  bool runIt = button->GetCheck() == BST_CHECKED;
  int posInd = nID - IDC_CHECK_NAVACQ_RUN1;
  if (posInd >= mNumShownActs)
    return;
  int actInd = mShownPosToIndex[posInd];
  setOrClearFlags(&mActions[actInd].flags, NAA_FLAG_RUN_IT, runIt ? 1 : 0);
  EnableDlgItem(IDC_STAT_NAVACQ_WHEN1 + posInd, runIt);
  if (mActions[actInd].flags & NAA_FLAG_HAS_SETUP)
    EnableDlgItem(IDC_BUT_NAVACQ_SETUP1 + posInd, runIt);

  /*if (!runIt && m_bHideUnusedActions) {
    BuildActionSection();
  } else {*/
    ManageTimingEnables();
  //}
  ManageEnables();
  ManageMacro();
}

// All the actions setups
void CNavAcquireDlg::OnButSetupAction(UINT nID)
{
  int posInd = nID - IDC_BUT_NAVACQ_SETUP1;
  int boolVal;
  if (posInd < 0 || posInd >= mNumShownActs)
    return;
  switch (mShownPosToIndex[posInd]) {

    // Center beam
  case NAACT_CEN_BEAM:
    mWinApp->mMultiTSTasks->SetupAutocenter();
    break;

    // Cook specimen
  case NAACT_COOK_SPEC:
    mWinApp->mCookerDlg = new CCookerSetupDlg;
    mWinApp->mCookerDlg->mDisableGo = true;
    mWinApp->mCookerDlg->DoModal();
    delete mWinApp->mCookerDlg;
    mWinApp->mCookerDlg = NULL;
    break;

    // Z by G
  case NAACT_EUCEN_BY_FOCUS:
    mWinApp->mParticleTasks->OpenZbyGDialog();
    break;

    // Coma-free alignment
  case NAACT_COMA_FREE:
    mWinApp->mMenuTargets.OnFocusSetCtfComaBt();
    mWinApp->mMenuTargets.OnFocusSetCtfAcquireParams();
    break;

    // Astigmatism
  case NAACT_ASTIGMATISM:
    boolVal = mParam->astigByBTID ? 1 : 0;
    if (!KGetOneChoice("", "What method do you want to use for correcting astigmatism?", 
      boolVal, "Use CTF fitting to a single image", 
      "Use the displacement of beam-tilted images (the BTID method)"))
      break;
    mParam->astigByBTID = boolVal != 0;
    if (mParam->astigByBTID)
      mWinApp->mMenuTargets.OnFocusSetAstigBeamTilt();
    else
      mWinApp->mMenuTargets.OnFocusSetCtfAcquireParams();
    break;

    // Autofocus
  case NAACT_AUTOFOCUS:
    KGetOneFloat("Autofocus will abort and restore focus when the limit is exceeded",
      "Limit on defocus change when autofocusing in microns (0 for no limit):",
      mParam->focusChangeLimit, 1);
    break;

    // Condition phase plate
  case NAACT_CONDITION_VPP:
    mWinApp->mMultiTSTasks->SetupVPPConditioning();
    break;

    // Wait for drift
  case NAACT_WAIT_DRIFT:
    mWinApp->mMenuTargets.OnTasksSetupWaitForDrift();
    ManageEnables();
    break;

    // Align to template
  case NAACT_ALIGN_TEMPLATE:
  {
    CNavRealignDlg dlg;
    dlg.DoModal();
  }
  break;
  
  // Realign to item    
  case NAACT_REALIGN_ITEM:
  {
    CNavRealignDlg dlg;
    dlg.mForRealignOnly = true;
    dlg.DoModal();
  }
  break;
  
  // Flash FEG, for FEI
  case NAACT_FLASH_FEG:
    if (FEIscope) {
      boolVal = mParam->highFlashIfOK ? 1 : 0;
      if (KGetOneChoice("Flashing will be done only if advised by scope software",
        "What kind of FEG flash do you want?",
        boolVal, "Just do low-temperature flashes when advised",
        "Do high-temperature flashes instead when advised"))
        mParam->highFlashIfOK = boolVal != 0;
    }
    break;

    // Hardware dark reference
  case NAACT_HW_DARK_REF:
    if (mWinApp->GetDEcamCount() > 0) {
      mWinApp->mGainRefMaker->MakeRefInDEserver(true);
    }
    break;
  
    // Check scope stuff
  case NAACT_CHECK_DEWARS:
  {
    CManageDewarsDlg dlg;
    dlg.DoModal();
  }
  break;
  }
}

// Setup for multishot: open dialog
void CNavAcquireDlg::OnNaSetupMultishot()
{
  mWinApp->mNavHelper->OpenMultishotDlg();
}

// Change in spinner for every N
void CNavAcquireDlg::OnDeltaposSpinEveryN(NMHDR *pNMHDR, LRESULT *pResult)
{
  UpdateData(true);
  if (NewSpinnerValue(pNMHDR, pResult, m_iEveryNitems, 1, MAX_EVERY_N, m_iEveryNitems))
    return;
  mActions[mCurActSelected].everyNitems = m_iEveryNitems;
  UpdateData(false);
  FormatTimingString(mCurActSelected, m_iSelectedPos);
  ManageEnables();
}

// Defocus cycling
void CNavAcquireDlg::OnNaCycleDefocus()
{
  UpdateData(true);
  ManageEnables();
}

void CNavAcquireDlg::OnDeltaposSpinCycleDef(NMHDR *pNMHDR, LRESULT *pResult)
{
  UpdateData(true);
  FormattedSpinnerValue(pNMHDR, pResult, 1, 30, mParam->cycleSteps, m_strCycleUm,
    "um in %d");
}

// Early return
void CNavAcquireDlg::OnNaEarlyReturn()
{
  UpdateData(true);
  ManageEnables();
}

// Skip saving
void CNavAcquireDlg::OnNaSkipSaving()
{
  UpdateData(true);
  ManageOutputFile();
}


void CNavAcquireDlg::OnHideUnselectedOptions()
{
  UpdateData(true);
  BuildActionSection();
}

void CNavAcquireDlg::OnMapWithRecViewSearch()
{
  UpdateData(true);
}
