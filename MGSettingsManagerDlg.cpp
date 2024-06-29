// MGSettingsManagerDlg.cpp : For storing settings for individual grids
//
// Copyright (C) 2024 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "MGSettingsManagerDlg.h"
#include "NavHelper.h"
#include "NavigatorDlg.h"
#include "MultiGridDlg.h"
#include "ShiftManager.h"


// CMGSettingsManagerDlg dialog

CMGSettingsManagerDlg::CMGSettingsManagerDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_MULGRID_SETTINGS, pParent)
  , m_strAutoContCurrent(_T(""))
  , m_strAutoContGrid(_T(""))
  , m_strHoleFinderCurrent(_T(""))
  , m_strHoleFinderGrid(_T(""))
  , m_strMultiShotCurrent(_T(""))
  , m_strMultiShotGrid(_T(""))
  , m_strGeneralCurrent(_T(""))
  , m_strGeneralGrid(_T(""))
{
  mNonModal = true;
  mSavedMultiShotParams = NULL;
  mSavedHoleFinderParams = NULL;
  mSavedAutoContParams = NULL;
  mSavedGeneralParams = NULL;
  mSavedAcqItemsParam = NULL;
  mNavHelper = mWinApp->mNavHelper;
}

// Clean up saved parameter structures
CMGSettingsManagerDlg::~CMGSettingsManagerDlg()
{
  delete mSavedMultiShotParams;
  delete mSavedHoleFinderParams;
  delete mSavedAutoContParams;
  delete mSavedGeneralParams;
  delete mSavedAcqItemsParam;
}

void CMGSettingsManagerDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_STAT_AC_TITLE, m_statACtitle);
  DDX_Text(pDX, IDC_STAT_ACSET_CURRENT, m_strAutoContCurrent);
  DDX_Text(pDX, IDC_STAT_ACSET_GRID, m_strAutoContGrid);
  DDX_Text(pDX, IDC_STAT_HFSET_CURRENT, m_strHoleFinderCurrent);
  DDX_Text(pDX, IDC_STAT_HFSET_GRID, m_strHoleFinderGrid);
  DDX_Text(pDX, IDC_STAT_MSSET_CURRENT, m_strMultiShotCurrent);
  DDX_Text(pDX, IDC_STAT_MSSET_GRID, m_strMultiShotGrid);
  DDX_Text(pDX, IDC_STAT_GENSET_CURRENT, m_strGeneralCurrent);
  DDX_Text(pDX, IDC_STAT_GENSET_GRID, m_strGeneralGrid);
  DDX_Control(pDX, IDC_STAT_HF_TITLE, m_statHFtitle);
  DDX_Control(pDX, IDC_STAT_MS_TITLE, m_statMStitle);
  DDX_Control(pDX, IDC_STAT_FINAL_TITLE, m_statFinalTitle);
  DDX_Control(pDX, IDC_STAT_GEN_TITLE, m_statGenTitle);
  DDX_Control(pDX, IDC_STAT_GRID1, m_statGrid1);
  DDX_Control(pDX, IDC_STAT_GRID2, m_statGrid2);
  DDX_Control(pDX, IDC_STAT_GRID3, m_statGrid3);
  DDX_Control(pDX, IDC_STAT_GRID4, m_statGrid4);
  DDX_Control(pDX, IDC_STAT_ACQ_ITEMS, m_statAcqItems);
}


BEGIN_MESSAGE_MAP(CMGSettingsManagerDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_BUT_GRIDUSE_AC, OnButGriduseAC)
  ON_BN_CLICKED(IDC_BUT_APPLY_AC, OnButApplyAC)
  ON_BN_CLICKED(IDC_BUT_RESTORE_CURAC, OnButRestoreCurAC)
  ON_BN_CLICKED(IDC_BUT_GRIDUSE_HF, OnButGriduseHF)
  ON_BN_CLICKED(IDC_BUT_APPLY_HF, OnButApplyHF)
  ON_BN_CLICKED(IDC_BUT_RESTORE_CURHF, OnButRestoreCurHF)
  ON_BN_CLICKED(IDC_BUT_GRIDUSE_MS, OnButGriduseMS)
  ON_BN_CLICKED(IDC_BUT_APPLY_MS, OnButApplyMS)
  ON_BN_CLICKED(IDC_BUT_RESTORE_CURMS, OnButRestoreCurMS)
  ON_BN_CLICKED(IDC_BUT_GRIDUSE_GEN, OnButGriduseGEN)
  ON_BN_CLICKED(IDC_BUT_APPLY_GEN, OnButApplyGEN)
  ON_BN_CLICKED(IDC_BUT_RESTORE_CURGEN, OnButRestoreCurGEN)
  ON_BN_CLICKED(IDC_BUT_REVERT_AC, OnButRevertAC)
  ON_BN_CLICKED(IDC_BUT_REVERT_HF, OnButRevertHF)
  ON_BN_CLICKED(IDC_BUT_REVERT_MS, OnButRevertMS)
  ON_BN_CLICKED(IDC_BUT_REVERT_GEN, OnButRevertGEN)
  ON_BN_CLICKED(IDC_BUT_REVERT_FINAL, OnButRevertFINAL)
  ON_BN_CLICKED(IDC_BUT_SET_VIEW_FINAL, OnButSetViewFinal)
  ON_BN_CLICKED(IDC_BUT_APPLY_AC, OnButApplyFinal)
  ON_BN_CLICKED(IDC_BUT_RESTORE_CURAC, OnButRestoreCurFinal)
END_MESSAGE_MAP()


// CMGSettingsManagerDlg message handlers
BOOL CMGSettingsManagerDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mMGTasks = mWinApp->mMultiGridTasks;
  mCartInfo = mWinApp->mScope->GetJeolLoaderInfo();
  mMGMultiShotParamArray = mMGTasks->GetMGMShotParamArray();
  mMGHoleFinderParamArray = mMGTasks->GetMGHoleParamArray();
  mMGGeneralParamArray = mMGTasks->GetMGGeneralParams();
  mMGAcqItemsParamArray = mMGTasks->GetMGAcqItemsParamArray();
  mMGAutoContParamArray = mMGTasks->GetMGAutoContParamArray();
  CFont *boldFont = mWinApp->GetBoldFont(&m_statACtitle);
  m_statACtitle.SetFont(boldFont);
  m_statHFtitle.SetFont(boldFont);
  m_statMStitle.SetFont(boldFont);
  m_statGenTitle.SetFont(boldFont);
  m_statFinalTitle.SetFont(boldFont);
  m_statGrid1.SetFont(boldFont);
  m_statGrid2.SetFont(boldFont);
  m_statGrid3.SetFont(boldFont);
  m_statGrid4.SetFont(boldFont);
  m_statAcqItems.SetFont(boldFont);
  SetJcdIndex(mJcdIndex);
  UpdateSettings();
  ManageEnables();

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CMGSettingsManagerDlg::OnOK()
{
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CMGSettingsManagerDlg::OnCancel()
{
  mNavHelper->GetMGSettingsPlacement();
  mNavHelper->mMGSettingsDlg = NULL;
  if (mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->UpdateEnables();
  DestroyWindow();
}

void CMGSettingsManagerDlg::PostNcDestroy()
{
  delete this;
  CBaseDlg::PostNcDestroy();
}

// Needs to be closed along with Nav
void CMGSettingsManagerDlg::CloseWindow()
{
  OnCancel();
}

BOOL CMGSettingsManagerDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CBaseDlg::PreTranslateMessage(pMsg);
}

// Do updates required when an index to the catalogue is initially set or changed
void CMGSettingsManagerDlg::SetJcdIndex(int jcdInd)
{
  mJcdIndex = jcdInd;
  ManageEnables();
  UpdateAutoContGrid();
  UpdateHoleFinderGrid();
  UpdateMultiShotGrid();
  UpdateGeneralGrid();
  UpdateAcqItemsGrid();
  UpdateData(false);
}

// Process external change in setting
void CMGSettingsManagerDlg::UpdateSettings()
{
  UpdateAutoContCurrent();
  UpdateHoleFinderCurrent();
  UpdateMultiShotCurrent();
  UpdateGeneralCurrent();
  UpdateData(false);
}

// Macro for function when grid uses current parameters
// Make new parameter or fetch an old one and move current settings into it
#define GRID_USE(typ, dex, brev) \
void CMGSettingsManagerDlg::OnButGriduse##brev##() \
{  \
  mWinApp->RestoreViewFocus();  \
  MGrid##typ##Params empty;    \
  JeolCartridgeData &jcd = mCartInfo->ElementAt(mJcdIndex);  \
  if (jcd.dex < 0) {   \
    jcd.dex = (int)mMG##typ##ParamArray->GetSize();   \
    mMGTasks->typ##ToGridSettings(empty);  \
    mMG##typ##ParamArray->Add(empty);   \
  } else {  \
    MGrid##typ##Params &param = mMG##typ##ParamArray->ElementAt(jcd.dex);   \
    mMGTasks->typ##ToGridSettings(param);  \
  }  \
  Update##typ##Grid();  \
  Update##typ##Current();  \
  ManageEnables();  \
  UpdateData(false);  \
}

// Macro for function when grid settings are reverted:
// Remove it from the array and adjust remaining indexes to elements above it
#define REMOVE_ADJUST(typ, dex, brev) \
void CMGSettingsManagerDlg::OnButRevert##brev##()  \
{  \
  mWinApp->RestoreViewFocus();  \
  JeolCartridgeData &jcd = mCartInfo->ElementAt(mJcdIndex); \
  mMG##typ##ParamArray->RemoveAt(jcd.dex);   \
  for (int jin = 0; jin < (int)mCartInfo->GetSize(); jin++) { \
    if (jin != jcd.dex) {   \
      JeolCartridgeData &jcd2 = mCartInfo->ElementAt(jin); \
      if (jcd2.dex > jcd.dex) \
         jcd2.dex--;        \
    }  \
  }   \
  jcd.dex = -1;  \
  Update##typ##Grid();  \
  Update##typ##Current();  \
  UpdateData(false);  \
  ManageEnables();  \
}

// Macro for function when grid settings are applied to current settings
// Make saved parameter structure for current ones if not done yet, then set parameters
#define APPLY_GRID(typ, dex, brev) \
  mWinApp->RestoreViewFocus();  \
  if (!mSaved##typ##Params) {  \
    mSaved##typ##Params = new MGrid##typ##Params;  \
    mMGTasks->typ##ToGridSettings(*mSaved##typ##Params);  \
  }   \
  JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndex);  \
  MGrid##typ##Params param;   \
  if (jcd.dex >= 0) { \
    param = mMG##typ##ParamArray->GetAt(jcd.dex);   \
    mMGTasks->GridTo##typ##Settings(param);  \
    UpdateData(false);  \
  } \
  Update##typ##Current();  \
  ManageEnables();


// Macro for function to restore current parameters to saved ones
#define RESTORE_CURRENT(typ, brev) \
void CMGSettingsManagerDlg::OnButRestoreCur##brev##()  \
{  \
  mWinApp->RestoreViewFocus();  \
  if (!mSaved##typ##Params)  \
    return;  \
  mMGTasks->GridTo##typ##Settings(*mSaved##typ##Params);   \
  delete mSaved##typ##Params; \
  mSaved##typ##Params = NULL; \
  Update##typ##Current();  \
  UpdateData(false);  \
  ManageEnables(); \
}

// Convenience macro for all four actions and the actual macro entries
#define ON_THREE_BUTTONS(typ, dex, brev) \
  GRID_USE(typ, dex, brev);  \
  REMOVE_ADJUST(typ, dex, brev);  \
  RESTORE_CURRENT(typ, brev);

void CMGSettingsManagerDlg::OnButApplyAC()
{ 
  APPLY_GRID(AutoCont, autoContParamIndex, AC);
  mNavHelper->OpenAutoContouring(true);
}

ON_THREE_BUTTONS(AutoCont, autoContParamIndex, AC);

void CMGSettingsManagerDlg::OnButApplyHF()
{
  APPLY_GRID(HoleFinder, holeFinderParamIndex, HF);
  mNavHelper->OpenHoleFinder();
}

ON_THREE_BUTTONS(HoleFinder, holeFinderParamIndex, HF);

void CMGSettingsManagerDlg::OnButApplyMS()
{
  APPLY_GRID(MultiShot, multiShotParamIndex, MS);
  mNavHelper->OpenMultishotDlg();
}

ON_THREE_BUTTONS(MultiShot, multiShotParamIndex, MS);

void CMGSettingsManagerDlg::OnButApplyGEN()
{
  APPLY_GRID(General, generalParamIndex, GEN);
}

ON_THREE_BUTTONS(General, generalParamIndex, GEN);

/*
 *  Corresponding functions (not quite) for Final data parameters 
 */

// Set up a new set of stored parameters old load with an existing one
void CMGSettingsManagerDlg::OnButSetViewFinal()
{
  JeolCartridgeData &jcd = mCartInfo->ElementAt(mJcdIndex);
  MGridAcqItemsParams mgParam;

  // Create parameter copy in array if not one yet
  if (jcd.finalDataParamIndex < 0) {
    mNavHelper->CopyAcqParamsAndActions(mNavHelper->GetAcqActions(1),
      mWinApp->GetNavAcqParams(1), mNavHelper->GetAcqActCurrentOrder(1),
      mgParam.actions, &mgParam.params, mgParam.actOrder);
    jcd.finalDataParamIndex = (int)mMGAcqItemsParamArray->GetSize();
    mMGAcqItemsParamArray->Add(mgParam);
    UpdateAcqItemsGrid();
    UpdateData(false);
    EnableDlgItem(IDC_BUT_REVERT_FINAL, true);
    EnableDlgItem(IDC_BUT_APPLY_FINAL, true);
  }

  // For opening dialog, first save the parameters in "preDlg" structure
  mNavHelper->CopyAcqParamsAndActions(mNavHelper->GetAcqActions(1),
    mWinApp->GetNavAcqParams(1), mNavHelper->GetAcqActCurrentOrder(1),
    mPreDlgAcqItemsParam.actions, &mPreDlgAcqItemsParam.params,
    mPreDlgAcqItemsParam.actOrder);
  mDidSavePreDlgParams = true;
  mgParam = mMGAcqItemsParamArray->GetAt(jcd.finalDataParamIndex);
  mNavHelper->CopyAcqParamsAndActions(mgParam.actions, &mgParam.params,
    mgParam.actOrder, mNavHelper->GetAcqActions(1), mWinApp->GetNavAcqParams(1),
    mNavHelper->GetAcqActCurrentOrder(1));
  mWinApp->mNavigator->AcquireAreas(NAVACQ_SRC_MG_SET_ACQ, false, false);
}

// Revert is the same as for others
REMOVE_ADJUST(AcqItems, finalDataParamIndex, FINAL);

// Apply stored settings as current ones : if not saved, saved in different structure,
// then copy
void CMGSettingsManagerDlg::OnButApplyFinal()
{
  JeolCartridgeData &jcd = mCartInfo->ElementAt(mJcdIndex);
  MGridAcqItemsParams mgParam;
  mWinApp->RestoreViewFocus();
  if (jcd.finalDataParamIndex < 0)
    return;
  mgParam = mMGAcqItemsParamArray->GetAt(jcd.finalDataParamIndex);
  if (!mSavedAcqItemsParam) {
    mSavedAcqItemsParam = new MGridAcqItemsParams;
    mNavHelper->CopyAcqParamsAndActions(mNavHelper->GetAcqActions(1),
      mWinApp->GetNavAcqParams(1), mNavHelper->GetAcqActCurrentOrder(1),
      mSavedAcqItemsParam->actions, &mSavedAcqItemsParam->params,
      mSavedAcqItemsParam->actOrder);
  }
  mNavHelper->CopyAcqParamsAndActions(mgParam.actions, &mgParam.params,
    mgParam.actOrder, mNavHelper->GetAcqActions(1), mWinApp->GetNavAcqParams(1),
    mNavHelper->GetAcqActCurrentOrder(1));
  EnableDlgItem(IDC_BUT_RESTORE_CURFINAL, true);
}

// Restore current params from the saved ones
void CMGSettingsManagerDlg::OnButRestoreCurFinal()
{
  mWinApp->RestoreViewFocus();
  if (!mSavedAcqItemsParam)
    return;
  mNavHelper->CopyAcqParamsAndActions(mSavedAcqItemsParam->actions, 
    &mSavedAcqItemsParam->params, mSavedAcqItemsParam->actOrder, 
    mNavHelper->GetAcqActions(1), mWinApp->GetNavAcqParams(1), 
    mNavHelper->GetAcqActCurrentOrder(1));
  EnableDlgItem(IDC_BUT_RESTORE_CURFINAL, false);
  delete mSavedAcqItemsParam;
  mSavedAcqItemsParam = NULL;
}

// Or restore params when dialog closes: but first copy them into grid's structure
void CMGSettingsManagerDlg::RestorePreDlgParams(int postponed)
{
  
  if (!mDidSavePreDlgParams)
    return;
  if (postponed >= 0 && mJcdIndex >= 0) {
    JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndex);
    MGridAcqItemsParams &mgParam = 
      mMGAcqItemsParamArray->ElementAt(jcd.finalDataParamIndex);
    mNavHelper->CopyAcqParamsAndActions(mNavHelper->GetAcqActions(1),
      mWinApp->GetNavAcqParams(1), mNavHelper->GetAcqActCurrentOrder(1),
      mgParam.actions, &mgParam.params, mgParam.actOrder);
  }
  mNavHelper->CopyAcqParamsAndActions(mPreDlgAcqItemsParam.actions, 
    &mPreDlgAcqItemsParam.params, mPreDlgAcqItemsParam.actOrder,
    mNavHelper->GetAcqActions(1), mWinApp->GetNavAcqParams(1), 
    mNavHelper->GetAcqActCurrentOrder(1));
  mDidSavePreDlgParams = false;
}

// Macro to handle enable/disable on the standard 4 buttons
#define ENABLE_FOUR_BUTS(typ, dex, brev) \
  gridIsSet = (mJcdIndex >= 0 && jcd.dex >= 0);  \
  EnableDlgItem(IDC_BUT_GRIDUSE_##brev, !tasks && mJcdIndex >= 0); \
  EnableDlgItem(IDC_BUT_APPLY_##brev, !tasks && (gridIsSet || \
    (!gridIsSet && mSaved##typ##Params == NULL)));  \
  SetDlgItemText(IDC_BUT_APPLY_##brev, gridIsSet ? \
    "Apply Grid Settings to Global" : "Save Global for Restoring"); \
  EnableDlgItem(IDC_BUT_REVERT_##brev, !tasks && gridIsSet);  \
  EnableDlgItem(IDC_BUT_RESTORE_CUR##brev, !tasks && mSaved##typ##Params != NULL);  \
  SetDlgItemText(IDC_STAT_GLOBAL_##brev, mSaved##typ##Params != NULL ? \
    "Saved global:" : "Global:");

// take care of all enables for all buttons
void CMGSettingsManagerDlg::ManageEnables()
{
  BOOL tasks = mWinApp->DoingTasks();
  JeolCartridgeData jcd;
  bool gridIsSet;
  if (mJcdIndex >= 0)
    jcd = mCartInfo->GetAt(mJcdIndex);
  ENABLE_FOUR_BUTS(AutoCont, autoContParamIndex, AC);
  ENABLE_FOUR_BUTS(HoleFinder, holeFinderParamIndex, HF);
  ENABLE_FOUR_BUTS(MultiShot, multiShotParamIndex, MS);
  ENABLE_FOUR_BUTS(General, generalParamIndex, GEN);
  EnableDlgItem(IDC_BUT_SET_VIEW_FINAL, !tasks && mJcdIndex >= 0);
  EnableDlgItem(IDC_BUT_APPLY_FINAL, !tasks && mJcdIndex >= 0 && 
    jcd.finalDataParamIndex >= 0);
  EnableDlgItem(IDC_BUT_REVERT_FINAL, !tasks && mJcdIndex >= 0 &&
    jcd.finalDataParamIndex >= 0);
  EnableDlgItem(IDC_BUT_RESTORE_CURFINAL, !tasks && mJcdIndex >= 0
    && mSavedAcqItemsParam != NULL);
}

// Macro to define two standard functions for updating the grid and current lines,
// calling the setting-specific "Format" functions
#define UPDATE_GRID_CURRENT(typ, dex) \
void CMGSettingsManagerDlg::Update##typ##Grid() {  \
  if (mJcdIndex < 0) {  \
    m_str##typ##Grid = "";  \
    return;  \
  }  \
  JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndex);  \
  MGrid##typ##Params param;  \
   m_str##typ##Grid = "NO stored settings for grid";  \
  if (jcd.dex >= 0) {  \
    param = mMG##typ##ParamArray->GetAt(jcd.dex);  \
    Format##typ##Settings(param, m_str##typ##Grid); \
  }  \
}  \
void CMGSettingsManagerDlg::Update##typ##Current()  \
{   \
  MGrid##typ##Params param;   \
  mMGTasks->##typ##ToGridSettings(param);  \
  Format##typ##Settings(mSaved##typ##Params != NULL ? *mSaved##typ##Params : param,  \
    m_str##typ##Current);  \
}

// Implement the standard functions for the 4 typical sets
UPDATE_GRID_CURRENT(AutoCont, autoContParamIndex);
UPDATE_GRID_CURRENT(HoleFinder, holeFinderParamIndex);
UPDATE_GRID_CURRENT(MultiShot, multiShotParamIndex);
UPDATE_GRID_CURRENT(General, generalParamIndex);

// Simple update for final data settings
void CMGSettingsManagerDlg::UpdateAcqItemsGrid()
{
  if (mJcdIndex < 0) {
    SetDlgItemText(IDC_STAT_ACQ_ITEMS, "");
    return;
  }
  JeolCartridgeData jcd = mCartInfo->GetAt(mJcdIndex);
  SetDlgItemText(IDC_STAT_ACQ_ITEMS, jcd.finalDataParamIndex < 0 ? 
    "NO stored settings for grid" : "Grid has separate settings stored");
}

// Format autocontouring settings, one line
void CMGSettingsManagerDlg::FormatAutoContSettings(MGridAutoContParams &param,
  CString &str)
{
  str.Format("Size %.1f - %.1f um, rel thresh %.2f, mean cutoffs %s - %s, edge %.1f um", 
    param.values[AC_minSize], param.values[AC_maxSize], param.values[AC_relThreshold],
    FormatValueOrNone(param.values[AC_lowerMeanCutoff]),
    FormatValueOrNone(param.values[AC_upperMeanCutoff]), 
    param.values[AC_borderDistCutoff]);
}

// Format hole-finder settings, will wrap onto two lines
void CMGSettingsManagerDlg::FormatHoleFinderSettings(MGridHoleFinderParams &param,
  CString &str)
{
  CString line, one;
  int ind;
  float *vals = param.values;
  str.Format("%s %.2f @ %.2f um, cutoffs: mean %s - %s, SD %s, black %s, edge %.1f, "
    "sigmas", vals[HF_hexagonalArray] ? "Hex" : "Sqr", vals[HF_diameter], 
    vals[HF_spacing], FormatValueOrNone(vals[HF_lowerMeanCutoff]),
    FormatValueOrNone(vals[HF_upperMeanCutoff]), FormatValueOrNone(vals[HF_SDcutoff]),
    FormatValueOrNone(vals[HF_blackFracCutoff]), vals[HF_edgeDistCutoff]);
  for (ind = 0; ind < (int)param.sigmas.size(); ind++) {
    one.Format(" %.1f", param.sigmas[ind]);
    str += one;
  }
  str += ", thresh";
  for (ind = 0; ind < (int)param.thresholds.size(); ind++) {
    one.Format(" %.1f", param.thresholds[ind]);
    str += one;
  }
}

// Format multishot settings, will wrap onto two lines
void CMGSettingsManagerDlg::FormatMultiShotSettings(MGridMultiShotParams &param,
  CString &str)
{
  CString line, one;
  float *vals = param.values;
  int nxHoles, nyHoles, doCen = B3DNINT(vals[MS_doCenter]);
  str = "Within hole: one";
  if (B3DNINT(vals[MS_inHoleOrMultiHole]) & MULTI_IN_HOLE) {
    str.Format("Within hole: %d @ %.2f um, %scenter%s, ", B3DNINT(vals[MS_numShots0]),
      vals[MS_spokeRad0], doCen ? "" : "NO ",
      B3DCHOICE(doCen, doCen < 0 ? " before" : " after", ""));
    if (B3DNINT(vals[MS_doSecondRing]))
      one.Format("2nd ring %d @ %.2f\n", B3DNINT(vals[MS_numShots1]), vals[MS_spokeRad1]);
    else
      one = "No 2nd ring\n";
    str += one;
  }
  if (B3DNINT(vals[MS_inHoleOrMultiHole]) & MULTI_HOLES) {
    nxHoles = B3DNINT(vals[MS_numHoles0]);
    nyHoles = B3DNINT(vals[MS_numHoles1]);
    if (B3DNINT(vals[MS_numHexRings]))
      one.Format("Multi-hole: %d rings of hex pattern", B3DNINT(vals[MS_numHexRings]));
    else
      one.Format("Multi-hole: %d by %d %s pattern", nxHoles, nyHoles,
        (B3DNINT(vals[MS_skipCornersOf3x3]) && 
        nxHoles == 3 && nyHoles == 3 )? "cross" : "rectangle");
    str += one;
  } else
    str += "NO multiple holes";
}

// Format general settings, break lines as needed
void CMGSettingsManagerDlg::FormatGeneralSettings(MGridGeneralParams &param,
  CString &str)
{
  int disable = param.values[GP_disableAutoTrim];
  str.Format("Disable dark border trim in align %d && %d && %d,\n"
    "Erase periodic peaks %s in Realign to Item, %s generally",
    disable & (NOTRIM_LOWDOSE_TS | NOTRIM_LOWDOSE_ALL),
    (disable & (NOTRIM_TASKS_TS | NOTRIM_TASKS_ALL)) >> 2, 
    (disable & NOTRIM_REALIGN_ITEM) >> 4,
    param.values[GP_RIErasePeriodicPeaks] ? "ON" : "OFF",
    param.values[GP_erasePeriodicPeaks] ? "ON" : "OFF");
}

// Simple function to print a floating value or "none"
CString CMGSettingsManagerDlg::FormatValueOrNone(float value)
{
  CString str = "none";
  if (value > EXTRA_VALUE_TEST)
    str.Format("%.4g", value);
  return str;
}
