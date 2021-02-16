// StateDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\StateDlg.h"
#include "NavHelper.h"
#include "EMscope.h"
#include "NavigatorDlg.h"
#include "CameraController.h"
#include "ShiftManager.h"

static int sIdTable[] = {IDC_BUT_ADDCURSTATE, IDC_BUT_ADDNAVSTATE, IDC_BUT_DELSTATE,
IDC_BUT_SETIMSTATE, IDC_BUT_SETMAPSTATE, IDC_BUT_RESTORESTATE, IDC_BUT_FORGETSTATE,
IDC_STAT_STATE_NAME, IDC_STAT_TITLELINE, IDC_BUTHELP, IDC_BUT_SETSCHEDSTATE,
IDC_STAT_IM_STATE_SET, IDC_BUT_UPDATE_STATE, IDC_EDIT_STATENAME, PANEL_END,
IDC_LIST_STATES, PANEL_END, TABLE_END};

static int sTopTable[sizeof(sIdTable) / sizeof(int)];
static int sLeftTable[sizeof(sIdTable) / sizeof(int)];
static int sHeightTable[sizeof(sIdTable) / sizeof(int)];

// CStateDlg dialog

CStateDlg::CStateDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CStateDlg::IDD, pParent)
  , m_strName(_T(""))
{
  mCurrentItem = -1;
  mInitialized = false;
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
END_MESSAGE_MAP()

// CStateDlg message handlers
BOOL CStateDlg::OnInitDialog()
{
  // [LD], camera, mag, spot, C2, exposure, binning, frame
  int fields[11] = {11,8,24,10,18,22,12,29,8,8,8};
  int tabs[11], i;
  BOOL states[2] = {true, true};
  CRect clientRect, editRect;
  CBaseDlg::OnInitDialog();
  mHelper = mWinApp->mNavHelper;
  mStateArray = mHelper->GetStateArray();

  GetClientRect(clientRect);
  m_editName.GetWindowRect(editRect);
  mNameBorderX = clientRect.Width() - editRect.Width();
  mNameHeight = editRect.Height();
  m_listViewer.GetWindowRect(editRect);
  mListBorderX = clientRect.Width() - editRect.Width();
  mListBorderY = clientRect.Height() - editRect.Height();

  tabs[0] = fields[0];
  for (i = 1; i < 11; i++)
    tabs[i] = tabs[i - 1] + fields[i]; 
  m_listViewer.SetTabStops(8, tabs);
  ReplaceDlgItemText(IDC_STAT_TITLELINE,"C2", mWinApp->mScope->GetC2Name());
  FillListBox();
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  UpdateHiding();

  mInitialized = true;
  //UpdateData(false);
   
  return TRUE;
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
  CArray<ScheduledFile *, ScheduledFile *> *schedFiles;
  BOOL noTasks = !mWinApp->DoingTasks();
  BOOL noComplex = !(mWinApp->DoingComplexTasks() || mWinApp->mCamera->CameraBusy());
  BOOL navOpen = mWinApp->mNavigator != NULL;
  int j;
  BOOL mapItem = false;
  BOOL schedItem = false;
  BOOL imOK = SetCurrentParam() && mWinApp->LookupActiveCamera(mParam->camIndex) >= 0;
  int type = mHelper->GetTypeOfSavedState();
  if (navOpen) {
    item = mWinApp->mNavigator->GetCurrentItem();
    if (item) {
      mapItem = item->mType == ITEM_TYPE_MAP;
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
  m_butAddCurState.EnableWindow(noTasks);
  m_butAddNavItemState.EnableWindow(noTasks && mapItem);
  m_butDelState.EnableWindow(noTasks && mCurrentItem >= 0);
  m_editName.EnableWindow(noTasks && mCurrentItem >= 0);
  m_butSetImState.EnableWindow(noTasks && noComplex && imOK && type != STATE_MAP_ACQUIRE);
  m_butSetMapState.EnableWindow(noTasks && noComplex && mapItem && type == STATE_NONE);
  m_butSetSchedState.EnableWindow(noTasks && noComplex && schedItem && 
    type != STATE_MAP_ACQUIRE);
  m_butRestore.EnableWindow(noTasks && noComplex && type != STATE_NONE);
  m_butForget.EnableWindow(noTasks && noComplex && type != STATE_NONE);
  if (type != STATE_IMAGING)
    DisableUpdateButton();
  m_butUpdate.EnableWindow(noTasks && noComplex && mCurrentItem >= 0 && 
    mCurrentItem == mSetStateIndex);
  m_listViewer.EnableWindow(noTasks && noComplex && type != STATE_MAP_ACQUIRE);
}


void CStateDlg::UpdateSettings(void)
{
  FillListBox();
  Update();
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

// Selection changes
void CStateDlg::OnSelchangeListStates()
{
  mWinApp->RestoreViewFocus();
  mCurrentItem = m_listViewer.GetCurSel();
  if (mCurrentItem == LB_ERR)
    mCurrentItem = -1;
  ManageName();
  Update();
  m_butUpdate.EnableWindow(mCurrentItem >= 0 && mSetStateIndex == mCurrentItem);
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
  mWinApp->RestoreViewFocus();
  StateParams *state = mHelper->NewStateParam(false);
  if (!state)
    return;
  mHelper->StoreCurrentStateInParam(state, mWinApp->LowDoseMode(), false);
  AddNewStateToList();
}

// Add a navigator map state
void CStateDlg::OnButAddNavState()
{
  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator)
    return;
  CMapDrawItem *item = mWinApp->mNavigator->GetCurrentItem();
  if (!item || item->mType != ITEM_TYPE_MAP)
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
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  if (AfxMessageBox("Are you sure you want to delete this state?", 
    MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;
  delete mParam;
  mStateArray->RemoveAt(mCurrentItem);
  m_listViewer.DeleteString(mCurrentItem);
  if (mCurrentItem >= mStateArray->GetSize())
    mCurrentItem--;
  m_listViewer.SetCurSel(mCurrentItem);
  ManageName();
  DisableUpdateButton();
  Update();
}

// Update a state if it is still the current one
void CStateDlg::OnButUpdateState()
{
  CString str;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentParam())
    return;
  mHelper->StoreCurrentStateInParam(mParam, mWinApp->LowDoseMode(), false);
  str.Format("Updated state # %d  %s   with current imaging state", mCurrentItem + 1, 
    (LPCTSTR)mParam->name);
  mWinApp->AppendToLog(str);
  UpdateListString(mCurrentItem);
}


// Set imaging state
void CStateDlg::OnButSetImState()
{
  mWinApp->RestoreViewFocus();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  if (!SetCurrentParam())
    return;
  if (mWinApp->LookupActiveCamera(mParam->camIndex) < 0)
    return;
  mHelper->SaveCurrentState(STATE_IMAGING, false);
  mHelper->SetStateFromParam(mParam, conSet, RECORD_CONSET);
  mSetStateIndex = mCurrentItem;
  m_butUpdate.EnableWindow(true);
  Update();
}

// Set map acquire state
void CStateDlg::OnButSetMapState()
{
  mWinApp->RestoreViewFocus();
  if (!mWinApp->mNavigator)
    return;
  CMapDrawItem *item = mWinApp->mNavigator->GetCurrentItem();
  if (!item || item->mType != ITEM_TYPE_MAP)
    return;
  mHelper->SetToMapImagingState(item, true);
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
  int j;

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
    mHelper->SaveCurrentState(STATE_IMAGING, true);
    mHelper->SetStateFromParam(state, conSet, RECORD_CONSET);
    DisableUpdateButton();
    Update();
  }
  if (item->mFocusAxisPos > EXTRA_VALUE_TEST)
    mHelper->SetLDFocusPosition(mWinApp->GetCurrentCamera(), item->mFocusAxisPos,
      item->mRotateFocusAxis, item->mFocusAxisAngle, item->mFocusXoffset,
      item->mFocusYoffset, "position for item");
}

// Restore state in the appropriate way
void CStateDlg::OnButRestoreState()
{
  mWinApp->RestoreViewFocus();
  if (mHelper->GetTypeOfSavedState() == STATE_MAP_ACQUIRE)
    mHelper->RestoreFromMapState();
  else {
    mHelper->RestoreSavedState();
    Update();
  }
  DisableUpdateButton();
}

// Cancel restorability of state
void CStateDlg::OnButForgetState()
{
  mWinApp->RestoreViewFocus();
  mHelper->SetTypeOfSavedState(STATE_NONE);
  DisableUpdateButton();
  Update();
}

// Format the string for the list box
void CStateDlg::StateToListString(int index, CString &string)
{
  StateParams *state = mStateArray->GetAt(index);
  int magInd = state->lowDose ? state->ldParams.magIndex : state->magIndex;
  int mag, active, spot, probe;
  int *activeList = mWinApp->GetActiveCameraList();
  double percentC2 = 0., intensity;
  CString lds = state->lowDose ? "SL" : "ST";
  MagTable *magTab = mWinApp->GetMagTable();
  CameraParameters *camp = mWinApp->GetCamParams() + state->camIndex;
  mag = MagForCamera(camp, magInd);
  active = mWinApp->LookupActiveCamera(state->camIndex) + 1;
  spot = state->lowDose ? state->ldParams.spotSize : state->spotSize;
  if (!camp->STEMcamera) {
    intensity = state->lowDose ? state->ldParams.intensity : state->intensity;
    probe = state->lowDose ? state->ldParams.probeMode : state->probeMode;
    percentC2 = mWinApp->mScope->GetC2Percent(spot, intensity);
    lds = state->lowDose ? "LD" : "";
  }

  string.Format("%s\t%d\t%d\t%d\t%.1f\t%.3f\t%s\t%.1fx%.1f\t%s", (LPCTSTR)lds, active,
    mag, spot, percentC2, state->exposure, mWinApp->BinningText(state->binning, camp), 
    state->xFrame / 1000., state->yFrame / 1000., (LPCTSTR)state->name);
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
}

// Take care of the name edit box
void CStateDlg::ManageName(void)
{
  m_strName = "";
  if (SetCurrentParam())
    m_strName = mParam->name;
  UpdateData(false);
}

void CStateDlg::DisableUpdateButton(void)
{
  mSetStateIndex = -1;
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
