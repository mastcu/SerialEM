// CMultiGridDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "EMscope.h"
#include "MultiGridTasks.h"
#include "MultiGridDlg.h"
#include "NavHelper.h"
#include "Shared\b3dutil.h"

#define MAX_DLG_SLOTS 13

static int sIdTable[] = {IDC_STAT_RUN, IDC_STAT_NAME,
  IDC_CHECK_MULGRID_RUN1, IDC_CHECK_MULGRID_RUN2, IDC_CHECK_MULGRID_RUN3,
  IDC_CHECK_MULGRID_RUN4, IDC_CHECK_MULGRID_RUN5, IDC_CHECK_MULGRID_RUN6,
  IDC_CHECK_MULGRID_RUN7, IDC_CHECK_MULGRID_RUN8, IDC_CHECK_MULGRID_RUN9,
  IDC_CHECK_MULGRID_RUN10, IDC_CHECK_MULGRID_RUN11, IDC_CHECK_MULGRID_RUN12,
  IDC_CHECK_MULGRID_RUN13, IDC_EDIT_MULGRID_NAME1, IDC_EDIT_MULGRID_NAME2,
  IDC_EDIT_MULGRID_NAME3, IDC_EDIT_MULGRID_NAME4, IDC_EDIT_MULGRID_NAME5,
  IDC_EDIT_MULGRID_NAME6, IDC_EDIT_MULGRID_NAME7, IDC_EDIT_MULGRID_NAME8,
  IDC_EDIT_MULGRID_NAME9, IDC_EDIT_MULGRID_NAME10, IDC_EDIT_MULGRID_NAME11,
  IDC_EDIT_MULGRID_NAME12, IDC_EDIT_MULGRID_NAME13, PANEL_END,
  IDC_BUT_MG_SETUP, IDC_STAT_MG_GRID_SETUP, PANEL_END,
  IDC_BUT_MG_GET_NAMES, IDC_BUT_MG_INVENTORY, IDC_STAT_MG_PREFIX,
  IDC_EDIT_MG_PREFIX, IDC_CHECK_MG_APPEND_NAME,
  IDC_BUT_MG_RESET_NAMES, IDC_BUT_MG_CLEAR, IDC_STAT_MG_ROOTNAME,
  IDC_STAT_MG_CURRENT_DIR, IDC_BUT_SET_CURRENT_DIR, IDC_CHECK_MG_USE_SUBDIRS, PANEL_END,
  IDCANCEL, IDOK, IDC_BUTHELP, PANEL_END, TABLE_END};

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
{
  mNonModal = true;
  for (int ind = 0; ind < MAX_MULGRID_PANELS; ind++)
    mPanelStates[ind] = true;
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
}


BEGIN_MESSAGE_MAP(CMultiGridDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_CHECK_MG_APPEND_NAME, &CMultiGridDlg::OnCheckMgAppendName)
  ON_BN_CLICKED(IDC_BUT_MG_GET_NAMES, &CMultiGridDlg::OnButMgGetNames)
  ON_BN_CLICKED(IDC_BUT_MG_INVENTORY, &CMultiGridDlg::OnButMgInventory)
  ON_BN_CLICKED(IDC_BUT_MG_RESET_NAMES, &CMultiGridDlg::OnButMgResetNames)
  ON_BN_CLICKED(IDC_BUT_MG_CLEAR, &CMultiGridDlg::OnButMgClearNames)
  ON_BN_CLICKED(IDC_BUT_SET_CURRENT_DIR, &CMultiGridDlg::OnButSetCurrentDir)
  ON_EN_CHANGE(IDC_EDIT_MG_PREFIX, &CMultiGridDlg::OnEnChangeEditMgPrefix)
  ON_BN_CLICKED(IDC_BUT_MG_SETUP, &CMultiGridDlg::OnButMgSetup)
  ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_EDIT_MULGRID_NAME1, IDC_EDIT_MULGRID_NAME13,
    &OnEnKillfocusEditName)
END_MESSAGE_MAP()


// CMultiGridDlg message handlers
BOOL CMultiGridDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  CString names;
  mMGTasks = mWinApp->mMultiGridTasks;
  mScope = mWinApp->mScope;
  mCartInfo = mWinApp->mScope->GetJeolLoaderInfo();
  SetupPanelTables(sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  mNumUsedSlots = 0;
  UpdateSettings();
  ManagePanels();
  UpdateEnables();
  SetRootname();

  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CMultiGridDlg::OnOK()
{
  OnCancel();
}

// Inform managing module of closing, or store placement and NULL out pointer
// right here
void CMultiGridDlg::OnCancel()
{
  mWinApp->mNavHelper->GetMultiGridPlacement();
  mWinApp->mNavHelper->mMultiGridDlg = NULL;
  DestroyWindow();
}

void CMultiGridDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

BOOL CMultiGridDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}


void CMultiGridDlg::OnCheckMgAppendName()
{
  UPDATE_DATA_TRUE;
  SetRootname();
}


void CMultiGridDlg::OnButMgGetNames()
{
  CString names, str;
  JeolCartridgeData jcd;
  ShortVec statVec;
  int status, numSlots = 1, slot = 1, highest = 0, ind;
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

    mCartInfo->RemoveAll();
    mNumUsedSlots = highest;

    for (slot = 0; slot < numSlots; slot++) {
      str = "";

      // Peel off the next name
      if (!names.IsEmpty()) {
        ind = names.Find("\n");
        if (ind < 0) {
          str = names;
        } else {
          str = names.Left(ind);
          names = names.Mid(ind + 1);
        }
      }

      // Add a cartridge if it is occupied
      if (statVec[slot] == 1) {
        jcd.id = slot + 1;
        jcd.slot = slot + 1;
        jcd.station = statVec[slot] == 1 ? JAL_STATION_MAGAZINE : JAL_STATION_STAGE;

        // Supply string if none
        if (str.IsEmpty())
          str.Format("Car%d", slot + 1);
        jcd.name = str;
        jcd.userName = "";
        mCartInfo->Add(jcd);
      }
    }
  } else
    mNumUsedSlots = (int)mCartInfo->GetSize();

  ReloadTable(1);
  UpdateEnables();
}


void CMultiGridDlg::OnButMgInventory()
{
  mMGTasks->GetInventory();
}


void CMultiGridDlg::OnButMgResetNames()
{
  if (!mNamesChanged || (mNamesChanged && AfxMessageBox("Resetting to names from scope "
    "will lose your name changes\n\nAre you sure you want to do this?", MB_QUESTION) == 
    IDYES))
    ReloadTable(1);
}


void CMultiGridDlg::OnButMgClearNames()
{
  if (!mNamesChanged || (mNamesChanged && AfxMessageBox("Clearing the names will throw "
    "away your name changes\n\nAre you sure you want to do this?", MB_QUESTION) == IDYES))
    ReloadTable(2);
}


void CMultiGridDlg::OnButSetCurrentDir()
{
  mWinApp->mDocWnd->OnFileSetCurrentDirectory();
}


void CMultiGridDlg::OnEnChangeEditMgPrefix()
{
  UPDATE_DATA_TRUE;
  mMGTasks->ReplaceBadCharacters(m_strPrefix);
  SetRootname();
}

void CMultiGridDlg::OnEnKillfocusEditName(UINT nID)
{
  CString str;
  JeolCartridgeData jcd;
  int car, ind = nID - IDC_EDIT_MULGRID_NAME1;
  CEdit *edit = (CEdit *)GetDlgItem(nID);
  if (!edit)
    return;
  edit->GetWindowText(str);
  if (mMGTasks->ReplaceBadCharacters(str))
    edit->SetWindowText(str);
  if (FEIscope) {
    for (car = 0; car < mCartInfo->GetSize(); car++) {
      jcd = mCartInfo->ElementAt(car);
      if (jcd.slot == mNumUsedSlots - ind) {
        ind = car;
        break;
      }
    }
  } else {
    jcd = mCartInfo->ElementAt(ind);
  }
  if (ind >= 0) {
    if (str.Compare(jcd.userName)) {
      mNamesChanged = true;
      jcd.userName = str;
    }
  }
}

void CMultiGridDlg::UpdateEnables()
{
  BOOL tasks = mWinApp->DoingTasks();
  EnableDlgItem(IDC_BUT_MG_GET_NAMES, (FEIscope || mCartInfo->GetSize() > 0) && !tasks);
  EnableDlgItem(IDC_BUT_MG_CLEAR, mNumUsedSlots > 0 && !tasks);
  EnableDlgItem(IDC_BUT_MG_RESET_NAMES, mNumUsedSlots > 0 && !tasks);

}

void CMultiGridDlg::UpdateSettings()
{
  m_strCurrentDir.Format("Current dir: %s", mWinApp->mDocWnd->GetInitialDir());
  UpdateData(false);
}

void CMultiGridDlg::ManagePanels()
{
  mIDsToDrop.clear();
  for (int ind = mNumUsedSlots; ind < MAX_DLG_SLOTS; ind++) {
    mIDsToDrop.push_back(IDC_CHECK_MULGRID_RUN1 + ind);
    mIDsToDrop.push_back(IDC_EDIT_MULGRID_NAME1 + ind);
  }
  AdjustPanels(mPanelStates, sIdTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);

}

void CMultiGridDlg::SetRootname()
{
  m_strRootname = "Root name: " + m_strPrefix;
  if (m_strPrefix.GetLength() < 3 || m_strPrefix.Find("Car") !=
    m_strPrefix.GetLength() - 3) {
    if (m_strPrefix.IsEmpty())
      m_strRootname = "C";
    else
      m_strRootname += "_C";
  }
  if (JEOLscope)
    m_strRootname += "##";
  m_strRootname += "##";
  if (m_bAppendNames)
    m_strRootname += "_name";
  UpdateData(false);
}

// Load the table with names; resetOrClear 0 to use usernames as is, 1 to use original
// name with replacements, 2 to clear
void CMultiGridDlg::ReloadTable(int resetOrClear)
{
  IntVec order;
  FloatVec idVec;
  int slot, ind, car;
  CString str;
  JeolCartridgeData jcd;

  // Get vector of IDs or inverse FEI IDs and order them
  if (JEOLscope) {
    for (slot = 0; slot < mNumUsedSlots; slot++) {
      order.push_back(slot);
      jcd = mCartInfo->ElementAt(slot);
      idVec.push_back((float)jcd.id);
    }
    rsSortIndexedFloats(&idVec[0], &order[0], mNumUsedSlots);
  }

  // Load the slots from the top in the given order, setting name as indicated
  for (slot = 0; slot < mNumUsedSlots; slot++) {
    if (JEOLscope) {
      ind = order[slot];
      jcd = mCartInfo->ElementAt(ind);
    } else {
      ind = -1;
      for (car = 0; car < mCartInfo->GetSize(); car++) {
        jcd = mCartInfo->ElementAt(car);
        if (jcd.slot == mNumUsedSlots - slot) {
          ind = car;
          break;
        }
      }
    }

    str.Format("%d", ind >= 0 ? jcd.id : mNumUsedSlots - slot);
    SetDlgItemText(IDC_CHECK_MULGRID_RUN1 + slot, str);
    EnableDlgItem(IDC_CHECK_MULGRID_RUN1 + slot, ind >= 0);
    EnableDlgItem(IDC_EDIT_MULGRID_NAME1 + slot, ind >= 0);

    if (ind < 0) {
      SetDlgItemText(IDC_EDIT_MULGRID_NAME1 + slot, "");

    } else {
      if (resetOrClear) {
        if (resetOrClear > 1)
          str = "";
        else {
          str = jcd.name;
          mMGTasks->ReplaceBadCharacters(str);
        }
        jcd.userName = str;
      }
      SetDlgItemText(IDC_EDIT_MULGRID_NAME1 + slot, jcd.userName);
    }
  }
  ManagePanels();
  mNamesChanged = false;
}

// Open-close setup section
void CMultiGridDlg::OnButMgSetup()
{
  mPanelStates[2] = !mPanelStates[2];
  SetDlgItemText(IDC_BUT_MG_SETUP, mPanelStates[2] ? "-" : "+");
  ManagePanels();
}
