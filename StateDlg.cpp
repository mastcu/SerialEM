// StateDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\StateDlg.h"
#include "Shared\b3dutil.h"
#include "NavHelper.h"
#include "EMscope.h"
#include "MultiGridDlg.h"
#include "FocusManager.h"
#include "NavigatorDlg.h"
#include "CameraController.h"
#include "ShiftManager.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static int sIdTable[] = {IDC_BUT_ADDCURSTATE, IDC_BUT_ADD_MONT_MAP, IDC_BUT_ADDNAVSTATE,
IDC_BUT_DELSTATE, IDC_BUT_SETIMSTATE, IDC_BUT_SETMAPSTATE, IDC_BUT_FORGETSTATE,
IDC_STAT_STATE_NAME, IDC_STAT_TITLELINE, IDC_BUTHELP, IDC_BUT_SETSCHEDSTATE,
IDC_BUT_SAVE_DEFOCUS, IDC_STAT_PRIOR_SUMMARY, IDC_STAT_NAV_GROUP, IDC_BUT_RESTORESTATE,
IDC_BUT_UPDATE_STATE, IDC_EDIT_STATENAME, IDC_BUT_LIST_STATE, IDC_STAT_SELECTED_LABEL,
IDC_CHECK_SET_APERTURES, PANEL_END,
IDC_LIST_STATES, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

int CStateDlg::mSetStateIndex[MAX_SAVED_STATE_IND + 1];
CArray<StateParams *, StateParams *> *CStateDlg::mStateArray;
CNavHelper *CStateDlg::mHelper;
StateParams *CStateDlg::mParam;
bool CStateDlg::mWarnedSharedParams;
bool CStateDlg::mWarnedNoMontMap;
bool CStateDlg::mRemindedToGoTo;
int CStateDlg::mCamOfSetState;
CSerialEMApp *CStateDlg::winApp;

// CStateDlg dialog

CStateDlg::CStateDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CStateDlg::IDD, pParent)
  , m_strName(_T(""))
  , m_strPriorSummary(_T(""))
  , m_bShowNumber(FALSE)
  , m_bSetApertures(FALSE)
{
  mCurrentItem = -1;
  mInitialized = false;
  mWarnedSharedParams = false;
  mWarnedNoMontMap = false;
  mRemindedToGoTo = false;
  mCamOfSetState = -1;
  mNonModal = true;
}

CStateDlg::~CStateDlg()
{
}

void CStateDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_BUT_ADDCURSTATE, m_butAddCurState);
  DDX_Control(pDX, IDC_BUT_ADDNAVSTATE, m_butAddNavItemState);
  DDX_Control(pDX, IDC_BUT_DELSTATE, m_butDelState);
  DDX_Control(pDX, IDC_EDIT_STATENAME, m_editName);
  DDX_Text(pDX, IDC_EDIT_STATENAME, m_strName);
  DDV_MaxChars(pDX, m_strName, 30);
  DDX_Control(pDX, IDC_LIST_STATES, m_listViewer);
  DDX_Control(pDX, IDC_BUT_SETIMSTATE, m_butSetImState);
  DDX_Control(pDX, IDC_BUT_SETMAPSTATE, m_butSetMapState);
  DDX_Control(pDX, IDC_BUT_RESTORESTATE, m_butRestore);
  DDX_Control(pDX, IDC_BUT_FORGETSTATE, m_butForget);
  DDX_Control(pDX, IDC_BUT_SETSCHEDSTATE, m_butSetSchedState);
  DDX_Control(pDX, IDC_BUT_UPDATE_STATE, m_butUpdate);
  DDX_Control(pDX, IDC_BUT_SAVE_DEFOCUS, m_butSaveDefocus);
  DDX_Text(pDX, IDC_STAT_PRIOR_SUMMARY, m_strPriorSummary);
  DDX_Control(pDX, IDC_BUT_ADD_MONT_MAP, m_butAddMontMap);
  DDX_Check(pDX, IDC_CHECK_SHOW_NUMBER, m_bShowNumber);
  DDX_Control(pDX, IDC_BUT_LIST_STATE, m_butListState);
  DDX_Control(pDX, IDC_CHECK_SET_APERTURES, m_butSetApertures);
  DDX_Check(pDX, IDC_CHECK_SET_APERTURES, m_bSetApertures);
}


BEGIN_MESSAGE_MAP(CStateDlg, CBaseDlg)
	ON_WM_SIZE()
	ON_WM_MOVE()
  ON_EN_CHANGE(IDC_EDIT_STATENAME, OnEnChangeStatename)
  ON_LBN_SELCHANGE(IDC_LIST_STATES, OnSelchangeListStates)
  ON_LBN_DBLCLK(IDC_LIST_STATES, OnDblclkListStates)
  ON_BN_CLICKED(IDC_BUT_ADDCURSTATE, OnButAddCurState)
  ON_BN_CLICKED(IDC_BUT_ADDNAVSTATE, OnButAddNavState)
  ON_BN_CLICKED(IDC_BUT_DELSTATE, OnButDelState)
  ON_BN_CLICKED(IDC_BUT_SETIMSTATE, OnButSetImState)
  ON_BN_CLICKED(IDC_BUT_SETMAPSTATE, OnButSetMapState)
  ON_BN_CLICKED(IDC_BUT_RESTORESTATE, OnButRestoreState)
  ON_BN_CLICKED(IDC_BUT_FORGETSTATE, OnButForgetState)
  ON_BN_CLICKED(IDC_BUT_SETSCHEDSTATE, OnButSetSchedState)
  ON_BN_CLICKED(IDC_BUT_UPDATE_STATE, OnButUpdateState)
  ON_BN_CLICKED(IDC_BUT_SAVE_DEFOCUS, OnButSaveDefocus)
  ON_BN_CLICKED(IDC_BUT_ADD_MONT_MAP, OnButAddMontMap)
  ON_BN_CLICKED(IDC_CHECK_SHOW_NUMBER, OnCheckNumber)
  ON_BN_CLICKED(IDC_BUT_LIST_STATE, OnButListState)
  ON_BN_CLICKED(IDC_CHECK_SET_APERTURES, OnCheckSetApertures)
END_MESSAGE_MAP()

// CStateDlg message handlers
BOOL CStateDlg::OnInitDialog()
{
  // [LD], camera, mag, spot, C2, defocus, exposure, binning, frame, name
  int fields[24] = {12,8,18,18,17,16,17,12,28,5,2,2,1,1,1,1,1,1,1,1,1,1,1,1};
  int tabs[24], i;
  BOOL states[2] = {true, true};
  CRect clientRect, editRect;
  CBaseDlg::OnInitDialog();
  SetStaticPointers();
  m_bShowNumber = mHelper->GetShowStateNumbers();

  GetClientRect(clientRect);
  m_editName.GetWindowRect(editRect);
  mNameBorderX = clientRect.Width() - editRect.Width();
  mNameHeight = editRect.Height();
  m_listViewer.GetWindowRect(editRect);
  mListBorderX = clientRect.Width() - editRect.Width();
  mListBorderY = clientRect.Height() - editRect.Height();

  tabs[0] = fields[0];
  for (i = 1; i < 12; i++)
    tabs[i] = tabs[i - 1] + fields[i];
  for (i = 0; i <= MAX_SAVED_STATE_IND; i++)
    mSetStateIndex[i] = -1;
  m_listViewer.SetTabStops(11, tabs);
  ReplaceDlgItemText(IDC_STAT_TITLELINE,"C2", mWinApp->mScope->GetC2Name());
  FillListBox();
  mIDsToAdjustHeight.push_back(IDC_STAT_NAV_GROUP);
  mIDsForNextTop.push_back(IDC_BUT_RESTORESTATE);
  if (mWinApp->mScope->GetUseAperturesInStates() <= 0)
    mIDsToDrop.push_back(IDC_CHECK_SET_APERTURES);
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  CWnd *wnd = GetDlgItem(IDC_STAT_PRIOR_SUMMARY);
  wnd->GetWindowRect(clientRect);
  SetupUnitsToAdd(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    -2 * clientRect.Height() / 4);
  UpdateHiding();

  mInitialized = true;
  //UpdateData(false);

  return TRUE;
}

void CStateDlg::SetStaticPointers()
{
  winApp = (CSerialEMApp *)AfxGetApp();
  mHelper = winApp->mNavHelper;
  mStateArray = mHelper->GetStateArray();
}

void CStateDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

void CStateDlg::OnSize(UINT nType, int cx, int cy)
{
  CRect rect, clientRect;
	CDialog::OnSize(nType, cx, cy);
  if (!mInitialized)
    return;
  if (mAdjustingPanels) {
    GetClientRect(clientRect);
    m_listViewer.GetWindowRect(rect);
    mListBorderY = clientRect.Height() - rect.Height();
    return;
  }
  int newX = cx - mListBorderX;
  int newY = cy - mListBorderY;
  if (newX < 10)
    newX = 10;
  if (newY < 10)
    newY = 10;

  m_listViewer.SetWindowPos(NULL, 0, 0, newX, newY, SWP_NOZORDER | SWP_NOMOVE);

  newX = cx - mNameBorderX;
  m_editName.SetWindowPos(NULL, 0, 0, newX, mNameHeight, SWP_NOZORDER | SWP_NOMOVE);
  mWinApp->RestoreViewFocus();
}

// Returns come through here - needed to avoid closing window
// can restore focus, but do not set up a kill focus handler that does
// this because it makes other buttons unresponsive
void CStateDlg::OnOK()
{
  mWinApp->RestoreViewFocus();
}

void CStateDlg::OnCancel()
{
  // DO NOT call base class, it will leak memory
  //CDialog::OnCancel();
  mWinApp->mNavHelper->GetStatePlacement();
  mWinApp->mNavHelper->mStateDlg = NULL;
  DestroyWindow();
}

// This doesn't work - there is a mouse up at end that sets focus to dialog
void CStateDlg::OnMove(int x, int y)
{
	CDialog::OnMove(x, y);
  if (mInitialized)
    mWinApp->RestoreViewFocus();
}

// Enable all items depending on conditions
void CStateDlg::Update(void)
{
  CMapDrawItem *item;
  ScheduledFile *sched;
  StateParams state;
  CString summary;
  CArray<ScheduledFile *, ScheduledFile *> *schedFiles;
  BOOL noTasks = !mWinApp->DoingTasks();
  BOOL noComplex = !(mWinApp->DoingComplexTasks() || mWinApp->mCamera->CameraBusy());
  BOOL navOpen = mWinApp->mNavigator != NULL;
  int j, ldArea = -2, paramArea = -1;
  int curCam = mWinApp->GetCurrentCamera();
  BOOL mapItem = false;
  BOOL schedItem = false, doUpdate = false;
  BOOL imOK = SetCurrentParam() && mWinApp->LookupActiveCamera(mParam->camIndex) >= 0;
  BOOL mgOK = !mHelper->mMultiGridDlg;
  int type = mHelper->GetTypeOfSavedState();
  if (mWinApp->LowDoseMode())
    ldArea = mWinApp->mScope->GetLowDoseArea();
  if (imOK)
    paramArea = mHelper->AreaFromStateLowDoseValue(mParam, NULL);
  if (navOpen) {
    item = mWinApp->mNavigator->GetCurrentItem();
    if (item) {
      mapItem = item->IsMap();
      if (item->mStateIndex >= 0) {
        schedItem = true;
      } else if ((j = mWinApp->mNavigator->GroupScheduledIndex(item->mGroupID)) >= 0) {
        schedFiles = mWinApp->mNavigator->GetScheduledFiles();
        sched = schedFiles->GetAt(j);
        schedItem = sched->stateIndex >= 0;
      }
      if (item->mFocusAxisPos > EXTRA_VALUE_TEST && mWinApp->LowDoseMode())
        schedItem = true;
    }
  }
  m_butAddCurState.EnableWindow(noTasks && (ldArea == -2 || ldArea != -1));
  m_butAddMontMap.ShowWindow((mWinApp->GetUseRecordForMontage() ||
    mWinApp->IsIDinHideSet(IDC_BUT_ADD_MONT_MAP)) ? SW_HIDE : SW_SHOW);
  m_butAddMontMap.EnableWindow(noTasks);
  m_butAddNavItemState.EnableWindow(noTasks && mapItem);
  m_butDelState.EnableWindow(noTasks && mCurrentItem >= 0 && mgOK);
  m_butListState.EnableWindow(noTasks && mCurrentItem >= 0);
  m_editName.EnableWindow(noTasks && mCurrentItem >= 0 && mgOK);
  m_butSetImState.EnableWindow(noTasks && noComplex && imOK && type != STATE_MAP_ACQUIRE
    && (type == STATE_NONE || !mParam->lowDose || mCamOfSetState< 0 ||
      mCamOfSetState == mParam->camIndex));
  m_butSetMapState.EnableWindow(noTasks && noComplex && mapItem && type == STATE_NONE);
  m_butSetSchedState.EnableWindow(noTasks && noComplex && schedItem &&
    type != STATE_MAP_ACQUIRE);
  m_butRestore.EnableWindow(noTasks && noComplex && type != STATE_NONE);
  m_butForget.EnableWindow(noTasks && noComplex && type != STATE_NONE);
  if (type != STATE_IMAGING)
    DisableUpdateButton();
  m_butUpdate.EnableWindow(noTasks && noComplex && CurrentMatchesSetState() >= 0 &&
    mgOK && ((ldArea == -2 && paramArea < 0) ||
    (ldArea == paramArea)) && imOK && curCam == mParam->camIndex);
  m_listViewer.EnableWindow(noTasks && noComplex && type != STATE_MAP_ACQUIRE);

  if (mHelper->GetSavedPriorState(state)) {
    state.name = "";
    StateToListString(&state, summary, "  ", -1, mSetStateIndex, MAX_SAVED_STATE_IND + 1,
      m_bShowNumber);
  } else {
    summary = "None";
  }
  summary = "Prior: " + summary;
  if (summary != m_strPriorSummary) {
    doUpdate = true;
    m_strPriorSummary = summary;
  }

  m_butSaveDefocus.EnableWindow(noTasks && noComplex && imOK && paramArea < 0);

  if (doUpdate)
    UpdateData(false);
}


void CStateDlg::UpdateSettings(void)
{
  FillListBox();
  Update();
}

// See if the current item matches any of the states that have been set
int CStateDlg::CurrentMatchesSetState()
{
  for (int ind = 0; ind <= MAX_SAVED_STATE_IND; ind++)
    if (mCurrentItem >= 0 && mSetStateIndex[ind] == mCurrentItem)
      return ind;
  return -1;
}

// A change in the name: transfer immediately to the string
void CStateDlg::OnEnChangeStatename()
{
  if (!SetCurrentParam())
    return;
  UpdateData(true);
  mParam->name = m_strName;
  UpdateListString(mCurrentItem);
}

// Toggle whether to show numbers in front of names
void CStateDlg::OnCheckNumber()
{
  UPDATE_DATA_TRUE;
  mHelper->SetShowStateNumbers(m_bShowNumber);
  FillListBox();
}

// Selection changes
void CStateDlg::OnSelchangeListStates()
{
  mWinApp->RestoreViewFocus();
  mCurrentItem = m_listViewer.GetCurSel();
  if (mCurrentItem == LB_ERR)
    mCurrentItem = -1;
  ManageName();
  Update();
}

// Double click is the same as set imaging state
void CStateDlg::OnDblclkListStates()
{
 mWinApp->RestoreViewFocus();
 ManageName();
 if (!SetCurrentParam() || mWinApp->DoingTasks() || mWinApp->mCamera->CameraBusy() ||
   mHelper->GetTypeOfSavedState() == STATE_MAP_ACQUIRE)
    return;
 OnButSetImState();
}

// Add the current imaging state
void CStateDlg::OnButAddCurState()
{
  int lowdose = mWinApp->LowDoseMode() ? 1 : 0;
  int area = mWinApp->mScope->GetLowDoseArea();
  mWinApp->RestoreViewFocus();
  StateParams *state = mHelper->NewStateParam(false);
  if (!state)
    return;
  if (lowdose && area < 0)
    return;
  mHelper->StoreCurrentStateInParam(state, lowdose,
    B3DCHOICE(area == FOCUS_CONSET || area == TRIAL_CONSET, area == FOCUS_CONSET ? 1 : 2,
      0), -1, IS_AREA_VIEW_OR_SEARCH(area) ? area + 1 : 0);
  AddNewStateToList();
}

// Add a mont-map state
void CStateDlg::OnButAddMontMap()
{
  mWinApp->RestoreViewFocus();
  StateParams *state = mHelper->NewStateParam(false);
  if (!state)
    return;
  state->montMapConSet = true;
  mHelper->StoreCurrentStateInParam(state, mWinApp->LowDoseMode() ? 1 : 0, 0, -1, 0);
  AddNewStateToList();
}

// Add a navigator map state
void CStateDlg::OnButAddNavState()
{
  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator)
    return;
  CMapDrawItem *item = mWinApp->mNavigator->GetCurrentItem();
  if (!item || item->IsNotMap())
    return;
  StateParams *state = mHelper->NewStateParam(false);
  if (!state)
    return;
  mHelper->StoreMapStateInParam(item, NULL, RECORD_CONSET, state);
  AddNewStateToList();
}

// Delete a state after confirmation
void CStateDlg::OnButDelState()
{
  int ind;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  if (AfxMessageBox("Are you sure you want to delete this state?",
    MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;
  delete mParam;
  ind = CurrentMatchesSetState();
  if (ind >= 0)
    mSetStateIndex[ind] = -1;
  mStateArray->RemoveAt(mCurrentItem);
  m_listViewer.DeleteString(mCurrentItem);
  if (mCurrentItem >= mStateArray->GetSize())
    mCurrentItem--;
  m_listViewer.SetCurSel(mCurrentItem);
  ManageName();
  Update();
}

// Update a state if it is still the current one
void CStateDlg::OnButUpdateState()
{
  CString str;
  int area = mWinApp->mScope->GetLowDoseArea();
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  DoUpdateState(mCurrentItem, mParam, area);
}

// List a state to the log window
void CStateDlg::OnButListState()
{
  CString str;
  if (!SetCurrentParam())
    return;
  bool noName = mParam->name.IsEmpty();
  mHelper->MakeStateOutputLine(mParam, str);
  PrintfToLog("%d: %s%s%s", mCurrentItem + 1, noName ? "" : (LPCTSTR)mParam->name, 
    noName ? "" : ": ", (LPCTSTR)str);
}

// Common function for updating state and dialog
void CStateDlg::DoUpdateState(int selInd, StateParams *param, int area)
{
  CString str;
  mHelper->StoreCurrentStateInParam(param, winApp->LowDoseMode() ? 1 : 0,
    B3DCHOICE(area == FOCUS_CONSET || area == TRIAL_CONSET,
      area == FOCUS_CONSET ? 1 : 2, 0), -1,
    IS_AREA_VIEW_OR_SEARCH(area) ? area + 1 : 0);
  str.Format("Updated state # %d  %s   with current imaging state", selInd + 1,
    (LPCTSTR)param->name);
  winApp->AppendToLog(str);
  if (mHelper->mStateDlg) {
    mHelper->mStateDlg->UpdateListString(selInd);
    mHelper->mStateDlg->Update();
    mHelper->mStateDlg->ManageName();
  }
}

// Option to set apertures toggled
void CStateDlg::OnCheckSetApertures()
{
  UpdateData(true);
  if (!SetCurrentParam())
    return;
  setOrClearFlags(&mParam->flags, STATEFLAG_SET_APERTURES, m_bSetApertures ? 1 : 0);
}

// External call to update a state
int CStateDlg::UpdateStateByNameOrNum(CString name, CString &errStr)
{
  int ind, selInd, ldArea = -2, paramArea;
  StateParams *param;

  // Lookup state
  ind = LookupStateByNameOrNum(name, selInd, errStr);
  if (ind)
    return ind;

  // Replicate almost all the tests made for enabling the Update button
  param = mStateArray->GetAt(selInd);
  ind = winApp->LookupActiveCamera(param->camIndex);
  if (ind < 0 || winApp->GetCurrentCamera() != param->camIndex) {
    errStr = "The camera for the specified state is not ";
    errStr += ind < 0 ? "available" : "the current camera";
    return ind < 0 ? 5 : 6;
  }
  if (mHelper->mMultiGridDlg &&
    selInd < mHelper->mMultiGridDlg->m_comboLMMstate.GetCount()) {
    errStr = "You cannot update a state while the Multiple Grid Operations dialog is"
      " open";
    return 7;
  }
  if (mHelper->GetTypeOfSavedState() != STATE_IMAGING) {
    errStr = "A map acquire state is currently set";
    return 4;
  }
  for (ind = 0; ind <= MAX_SAVED_STATE_IND; ind++)
    if (mSetStateIndex[ind] == selInd)
      break;
  if (ind > MAX_SAVED_STATE_IND) {
    errStr = "The specified state is not a currently set state";
    return 8;
  }
  if (winApp->LowDoseMode())
    ldArea = winApp->mScope->GetLowDoseArea();
  paramArea = mHelper->AreaFromStateLowDoseValue(param, NULL);
  if ((ldArea == -2 && paramArea < 0) || (ldArea == paramArea)) {

    // Finally, do it the same as above
    DoUpdateState(selInd, param, ldArea);

  } else {
    errStr = "The specified state does not match the current low dose area";
    return 9;
  }
  return 0;
}

// Set imaging state
void CStateDlg::OnButSetImState()
{
  CString errStr;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  if (DoSetImState(mCurrentItem, errStr))
    mWinApp->AppendToLog(errStr);
}

// Common function that does the work
int CStateDlg::DoSetImState(int stateNum, CString &errStr)
{
  SetStaticPointers();
  ControlSet *conSet = winApp->GetConSets();
  CString *names = winApp->GetModeNames();
  StateParams *param;
  CString str, mess;
  int type = mHelper->GetTypeOfSavedState();
  int area, setNum, indSave, areaInd, saveTarg = 0;
  param = mStateArray->GetAt(stateNum);

  if (winApp->LookupActiveCamera(param->camIndex) < 0) {
    errStr = "The camera number defined for this state is not an active camera";
    return 5;
  }
  if (type != STATE_NONE && param->lowDose && mCamOfSetState >= 0 &&
    param->camIndex != mCamOfSetState) {
    errStr = "Cannot set a low dose state with a camera different from that"
      " of the first state set";
    return 6;
  }
  area = mHelper->AreaFromStateLowDoseValue(param, &setNum);
  mess.Format("%s%s parameters set from state # %d  %s %s", area < 0 ? "" : "Low dose ",
    names[setNum], stateNum + 1, (LPCTSTR)param->name,
    (area >= 0 && !mRemindedToGoTo && !winApp->DoingTasks()) ?
    " (Press the Go To button in Low Dose to set the scope state)" : "");
  if (area >= 0 && !winApp->DoingTasks())
    mRemindedToGoTo = true;
  if (setNum == SEARCH_CONSET && winApp->GetUseViewForSearch())
    setNum = VIEW_CONSET;
  if (setNum == VIEW_CONSET && winApp->GetUseViewForSearch() && !mWarnedSharedParams) {
    winApp->AppendToLog("WARNING: Setting this state sets the shared View/Search camera"
      " parameters");
    mWarnedSharedParams = true;
  }
  if (setNum == MONT_USER_CONSET && winApp->GetUseRecordForMontage() &&
    !mWarnedNoMontMap) {
    winApp->AppendToLog("WARNING: Setting this state sets the Mont-map camera"
      " parameters, which are not currently in use");
    mWarnedNoMontMap = true;
  }

  // Restore state when leaving low dose mode, which restores LD params, doesn't assert
  // prior state because calling with the skipScope flag, and end up in R state
  // Do not restore when entering, it is a needless change
  if (type == STATE_IMAGING && param->lowDose == 0 && winApp->LowDoseMode())
    DoRestoreState(true);

  conSet += setNum;

  // Save the current defocus target or offset if one is set in this state
  if (param->targetDefocus > -9990. || param->ldDefocusOffset > -9990.) {
    if (IS_AREA_VIEW_OR_SEARCH(area))
      saveTarg = area + 1;
    else if (!param->lowDose)
      saveTarg = -1;
  }
  mHelper->SaveCurrentState(STATE_IMAGING, B3DCHOICE(area == FOCUS_CONSET ||
    area == TRIAL_CONSET, area == FOCUS_CONSET ? 1 : 2, 0), param->camIndex, saveTarg,
    param->montMapConSet, param->lowDose ? 0 : 1);
  mHelper->SaveLowDoseAreaForState(area, param->camIndex, saveTarg > 0,
    param->montMapConSet);
  mHelper->SetStateFromParam(param, conSet, setNum, 0, false, 
    (param->flags & STATEFLAG_SET_APERTURES) != 0);
  if (mHelper->GetCurStateSetApertures() && (param->flags & STATEFLAG_SET_APERTURES)) {
    if (mHelper->FormatApertureString(param, str, false)) {
      if (mess.Find("Press"))
        mess += "\r\n";
      mess += "   apertures set to " + str;
    }
  }
  SEMAppendToLog(mess);
  areaInd = param->montMapConSet ? MAX_SAVED_STATE_IND : (area + 1);
  indSave = mSetStateIndex[areaInd];
  mSetStateIndex[areaInd] = stateNum;
  if (mHelper->mStateDlg) {
    mHelper->mStateDlg->UpdateListString(stateNum);
    if (indSave >= 0)
      mHelper->mStateDlg->UpdateListString(indSave);
  }
  if (mCamOfSetState < 0)
    mCamOfSetState = param->camIndex;
  if (mHelper->mStateDlg)
    mHelper->mStateDlg->Update();
  if (area >= 0)
    winApp->mLowDoseDlg.RedrawAndUpdateSepIfDefining(area);
  return 0;
}

// Set map acquire state
void CStateDlg::OnButSetMapState()
{
  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator)
    return;
  CMapDrawItem *item = mWinApp->mNavigator->GetCurrentItem();
  if (!item || item->IsNotMap())
    return;
  mHelper->SetToMapImagingState(item, true,
    !(mWinApp->LowDoseMode() && item->mMapLowDoseConSet < 0) ? -1 : 0, 1);
}

// Set imaging state from scheduled state of a Nav acquire
void CStateDlg::OnButSetSchedState()
{
  ScheduledFile *sched;
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  CArray<ScheduledFile *, ScheduledFile *> *schedFiles =
    mWinApp->mNavigator->GetScheduledFiles();
  CArray<StateParams *, StateParams *> *stateArray =
    mWinApp->mNavigator->GetAcqStateArray();
  StateParams *state = NULL;
  int j, area, setNum;

  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator)
    return;
  CMapDrawItem *item = mWinApp->mNavigator->GetCurrentItem();
  if (!item)
    return;
  if (item->mStateIndex >= 0)
    state = stateArray->GetAt(item->mStateIndex);
  else if ((j = mWinApp->mNavigator->GroupScheduledIndex(item->mGroupID)) >= 0) {
    sched = schedFiles->GetAt(j);
    if (sched->stateIndex >= 0)
      state = stateArray->GetAt(sched->stateIndex);
  }
  if (state) {
    area = mHelper->AreaFromStateLowDoseValue(state, &setNum);
    if (setNum == SEARCH_CONSET && mWinApp->GetUseViewForSearch())
      setNum = VIEW_CONSET;
    mHelper->SaveCurrentState(STATE_IMAGING, 1, state->camIndex, 0, false,
      state->lowDose ? 0 : 1);
    mHelper->SetStateFromParam(state, conSet + setNum, setNum);
    DisableUpdateButton();
    Update();
    if (mWinApp->LowDoseMode())
      mWinApp->mScope->GotoLowDoseArea(area);
  }
  if (item->mFocusAxisPos > EXTRA_VALUE_TEST)
    mHelper->SetLDFocusPosition(mWinApp->GetCurrentCamera(), item->mFocusAxisPos,
      item->mRotateFocusAxis, item->mFocusAxisAngle, item->mFocusXoffset,
      item->mFocusYoffset, "position for item", false);
}

// Save target defocus
void CStateDlg::OnButSaveDefocus()
{
  int area;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  area = mHelper->AreaFromStateLowDoseValue(mParam, NULL);
  if (area < 0)
    mParam->targetDefocus = mWinApp->mFocusManager->GetTargetDefocus();
  Update();
}

// Restore state in the appropriate way
void CStateDlg::OnButRestoreState()
{
  DoRestoreState(false);
}

void CStateDlg::DoRestoreState(bool skipScope)
{
  if (mHelper->mStateDlg)
    ((CSerialEMApp *)AfxGetApp())->RestoreViewFocus();
  mCamOfSetState = -1;
  if (mHelper->GetTypeOfSavedState() == STATE_MAP_ACQUIRE)
    mHelper->RestoreFromMapState();
  else {
    mHelper->RestoreSavedState(skipScope);
    if (mHelper->mStateDlg)
      mHelper->mStateDlg->Update();
  }
  if (mHelper->mStateDlg)
    mHelper->mStateDlg->DisableUpdateButton();
  ((CSerialEMApp *)AfxGetApp())->mLowDoseDlg.RedrawAndUpdateSepIfDefining(-1);
}

// Cancel restorability of state
void CStateDlg::OnButForgetState()
{
  mWinApp->RestoreViewFocus();
  mHelper->ForgetSavedState();
  DisableUpdateButton();
  mCamOfSetState = -1;
  Update();
}

// For an external caller to set a state specified by name or number in table
int CStateDlg::SetStateByNameOrNum(CString name, CString &errStr)
{
  int ind, selInd = -1;
  ind = LookupStateByNameOrNum(name, selInd, errStr);
  if (ind)
    return ind;
  if (mHelper->GetTypeOfSavedState() == STATE_MAP_ACQUIRE) {
    errStr = "A map acquire state is already set";
    return 4;
  }

  ind = DoSetImState(selInd, errStr);
  if (mHelper->mStateDlg)
    mHelper->mStateDlg->SetCurrentParam();
  return ind;
}

int CStateDlg::LookupStateByNameOrNum(CString name, int &selInd, CString &errStr)
{
  StateParams *state;
  CString ucName = name.Trim(), stateName;
  int ind, selNum, numExact = 0;
  bool numOK, twoMatch = false;
  const char *namePtr = (LPCTSTR)name;
  char *endPtr;
  SetStaticPointers();

  // Do case insensitive comparisons, convert to number and detect if there are bad chars
  ucName.MakeUpper();
  selNum = strtol(namePtr, &endPtr, 10) - 1;
  numOK = endPtr - namePtr == ucName.GetLength();
  selInd = -1;

  // Find a match or more than one
  for (ind = 0; ind < (int)mStateArray->GetSize(); ind++) {
    state = mStateArray->GetAt(ind);
    stateName = state->name;
    stateName.MakeUpper();
    if (stateName == ucName) {
      if (!numExact)
        selInd = ind;
      numExact++;
    }
    if (!numExact && !stateName.IsEmpty() && stateName.Find(ucName) == 0) {
      if (selInd < 0)
        selInd = ind;
      else
        twoMatch = true;
    }
  }

  // Error conditions
  if (selInd < 0 && !numOK) {
    errStr = "There is no state whose name starts with " + name;
    return 1;
  }
  if (numExact > 1 && !numOK) {
    errStr.Format("There are %d states whose names are %s", numExact, (LPCTSTR)name);
    return 2;
  }
  if (!numExact && twoMatch && !numOK) {
    errStr = "There are two states whose names start with " + name;
    return 2;
  }
  if (selInd < 0 && numOK) {
    if (selNum >= 0 && selNum < (int)mStateArray->GetSize())
      selInd = selNum;
    else {
      errStr = "There is no state numbered " + name;
      return 3;
    }
  }
  return 0;
}

// Format the string for the list box
void CStateDlg::StateToListString(int index, CString &string)
{
  StateParams *state = mStateArray->GetAt(index);
  StateToListString(state, string, "\t", index, mSetStateIndex, MAX_SAVED_STATE_IND + 1,
    m_bShowNumber);
}

void CStateDlg::StateToListString(StateParams *state, CString &string, const char *sep,
  int index, int *setStateIndex, int numSet, BOOL showNumber)
{
  int magInd = state->lowDose ? state->ldParams.magIndex : state->magIndex;
  int mag, active, spot, probe, ldArea;
  SetStaticPointers();
  int *activeList = winApp->GetActiveCameraList();
  double percentC2 = 0., intensity;
  CString *names = winApp->GetModeNames(), intStr;
  CString prbal, magstr, defstr, numName, lds = state->lowDose ? "SL" : "ST";
  MagTable *magTab = winApp->GetMagTable();
  CameraParameters *camp = winApp->GetCamParams() + state->camIndex;
  bool selected = false;
  defstr = "  " + CString((char)0x97);
  mag = MagForCamera(camp, magInd);
  active = winApp->LookupActiveCamera(state->camIndex) + 1;
  spot = state->lowDose ? state->ldParams.spotSize : state->spotSize;
  magstr = UtilFormattedMag(mag, 0);
  if (!camp->STEMcamera) {
    intensity = state->lowDose ? state->ldParams.intensity : state->intensity;
    probe = state->lowDose ? state->ldParams.probeMode : state->probeMode;
    percentC2 = winApp->mScope->GetC2Percent(spot, intensity);
    if (percentC2 > 99.)
      intStr.Format("%.0f", percentC2);
    else
      intStr.Format("%.1f", percentC2);
    if (state->lowDose) {
      ldArea = B3DCHOICE(state->lowDose > 0, RECORD_CONSET, -1 - state->lowDose);
      if (ldArea == SEARCH_AREA)
        ldArea = SEARCH_CONSET;
      lds = names[ldArea].Left(1);
    } else
      lds = (char)0x97;
    if (FEIscope)
      prbal = probe ? "uP" : "nP";
    else if (!winApp->mScope->GetHasNoAlpha() && state->beamAlpha >= 0)
      prbal.Format("a%d", state->beamAlpha + 1);
    if (state->lowDose && state->ldDefocusOffset > -9990.)
      defstr.Format("%d", B3DNINT(state->ldDefocusOffset));
    if (!state->lowDose && state->targetDefocus > -9990.)
      defstr.Format("%.1f", state->targetDefocus);
  }
  selected = index >= 0 && numberInList(index, setStateIndex, numSet,
    0);

  if (showNumber && index >= 0)
    numName.Format("%d: %s", index + 1, (LPCTSTR)state->name);
  else
    numName = state->name;
  string.Format("%s%s%d%s%s%s%d%s%s%s%s%s%s%.2f%s%s%s%.1fx%.1f%s%s%c%s%s", (LPCTSTR)lds,
    sep, active, sep, (LPCTSTR)magstr, sep, spot, (LPCTSTR)prbal, sep, intStr, sep,
    (LPCTSTR)defstr, sep, state->exposure, sep, winApp->BinningText(state->binning, camp)
    , sep, state->xFrame / 1000., state->yFrame / 1000., state->montMapConSet ? "M" : "",
    sep, selected ? (char)0x86 : ' ', sep, (LPCTSTR)numName);
}


// Update the string of an item in the box
void CStateDlg::UpdateListString(int index)
{
  CString string;
  StateToListString(index, string);
  m_listViewer.DeleteString(index);
  m_listViewer.InsertString(index, string);
  if (mCurrentItem >= 0)
    m_listViewer.SetCurSel(mCurrentItem);
}

// Fill the whole box from scratch
void CStateDlg::FillListBox(void)
{
  CString string;
  int i;
  m_listViewer.ResetContent();
  mCurrentItem = -1;
  if (mStateArray->GetSize()) {
    for (i = 0; i < mStateArray->GetSize(); i++) {
      StateToListString(i, string);
      m_listViewer.AddString(string);
    }
    mCurrentItem = 0;
    m_listViewer.SetCurSel(0);
  }
  ManageName();
  Update();
}

// Convenience function to set member param with current item
BOOL CStateDlg::SetCurrentParam(void)
{
  if (mCurrentItem < 0 || mCurrentItem >= mStateArray->GetSize())
    return false;
  mParam = mStateArray->GetAt(mCurrentItem);
  return true;
}

// FInish adding new state to a list box
void CStateDlg::AddNewStateToList(void)
{
  CString str;
  mCurrentItem = (int)mStateArray->GetSize() - 1;
  StateToListString(mCurrentItem, str);
  m_listViewer.AddString(str);
  m_listViewer.SetCurSel(mCurrentItem);
  UpdateListString(mCurrentItem);
  ManageName();
  Update();
  if (mHelper->mMultiGridDlg) {
    mHelper->mMultiGridDlg->LoadAllComboBoxes();
    mHelper->mMultiGridDlg->SetAllComboBoxesFromNameOrNum();
  }
}

// Take care of the name edit box
void CStateDlg::ManageName(void)
{
  CString str;
  m_strName = "";
  if (SetCurrentParam())
    m_strName = mParam->name;
  if (mWinApp->mScope->GetUseAperturesInStates() > 0) {
    if (mParam->objectiveAp >= 0 || mParam->condenserAp >= 0 || mParam->JeolC1Ap >= 0) {
      m_butSetApertures.EnableWindow(true);
      if (mHelper->FormatApertureString(mParam, str, true))
        str = " (" + str + ")";
    } else {
      m_butSetApertures.EnableWindow(false);
    }
    m_butSetApertures.SetWindowText("Set apertures with state" + str);
    m_bSetApertures = (mParam->flags & STATEFLAG_SET_APERTURES) != 0;
  }
  UpdateData(false);
}

void CStateDlg::DisableUpdateButton(void)
{
  int indSave;
  for (int ind = 0; ind <= MAX_SAVED_STATE_IND; ind++) {
    indSave = mSetStateIndex[ind];
    mSetStateIndex[ind] = -1;
    if (indSave >= 0)
      UpdateListString(indSave);
  }
  m_butUpdate.EnableWindow(false);
}

void CStateDlg::UpdateHiding(void)
{
  CRect rect, clientRect;
  BOOL states[2] = {true, true};
  mAdjustingPanels = true;
  m_listViewer.GetWindowRect(&rect);
  sHeightTable[sizeof(sIdTable) / sizeof(int) - 3] = rect.Height();
  AdjustPanels(states, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    0, sHeightTable);
  mAdjustingPanels = false;
  GetClientRect(clientRect);
  mListBorderY = clientRect.Height() - rect.Height();
  mWinApp->RestoreViewFocus();
}
