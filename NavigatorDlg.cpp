// NavigatorDlg.cpp : Dialog class to manage the Navigator window and perform
//                    all tasks associated with it
//
//
// Copyright (C) 2003-2020 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "direct.h"
#include "SerialEM.h"
#include <map>
#include <algorithm>
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "EMscope.h"
#include "ParameterIO.h"
#include "ProcessImage.h"
#include "FocusManager.h"
#include "ComplexTasks.h"
#include "ParticleTasks.h"
#include "MultiTSTasks.h"
#include "MacroProcessor.h"
#include "MacroEditer.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include ".\NavigatorDlg.h"
#include "NavHelper.h"
#include "NavAcquireDlg.h"
#include "NavImportDlg.h"
#include "MultiShotDlg.h"
#include "MultiCombinerDlg.h"
#include "HoleFinderDlg.h"
#include "ShiftToMarkerDlg.h"
#include "FilterTasks.h"
#include "GainRefMaker.h"
#include "AutoTuning.h"
#include "MapDrawItem.h"
#include "Mailer.h"
#include "LogWindow.h"
#include "CameraMacroTools.h"
#include "MontageSetupDlg.h"
//#include "TiltSeriesParam.h"
#include "TSController.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include "Shared\autodoc.h"
#include "Image\KStoreIMOD.h"
#include "Image\KStoreADOC.h"
#include "Shared\b3dutil.h"
#include "Shared\iimage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static char *colorNames[] = {"Red", "Grn", "Blu", "Yel", "Mag", "NRI"};
static char *typeString[] = {"Pt", "Poly", "Map", "Imp", "Anc"};

#define MAX_REGPT_NUM 99
#define NAV_FILE_VERSION "2.00"
#define VERSION_TIMES_100 200


// Simple derived drag list box class for notifying of a drag change
CMyDragListBox::CMyDragListBox()
: CDragListBox()
{
}

// Selection change and clicking in empty space both generate this message, so
// make sure it is legal new index
void CMyDragListBox::Dropped( int nSrcIndex, CPoint pt )
{
  CDragListBox::Dropped(nSrcIndex, pt);
  int newIndex = GetCurSel();
  if (newIndex == LB_ERR) {

    // This is the old way which was flawed when there was a scrolling change in the drop
    // Since we do a remove then insert below, index needs to be reduced for a higher
    // new value (a difference of 1 does nothing)
    newIndex = ItemFromPt(pt);
    if (newIndex > nSrcIndex)
     newIndex--;
  }

  if (newIndex >= 0 && nSrcIndex != newIndex)
    ((CSerialEMApp *)AfxGetApp())->mNavigator->OnListItemDrag(nSrcIndex, newIndex);
}


/////////////////////////////////////////////////////////////////////////////
// CNavigatorDlg dialog


CNavigatorDlg::CNavigatorDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CNavigatorDlg::IDD, pParent)
  , m_bRotate(FALSE)
  , m_bTiltSeries(FALSE)
  , m_bFileAtItem(FALSE)
  , m_bGroupFile(FALSE)
  , m_strFilename(_T("(Alongfilenameforyou)"))
  , m_bDualMap(FALSE)
  , m_strItemNum(_T(""))
  , m_bShowAcquireArea(FALSE)
  , m_bEditMode(FALSE)
  , m_bDrawLabels(TRUE)
  , m_bEditFocus(FALSE)
{
	//{{AFX_DATA_INIT(CNavigatorDlg)
	m_bRegPoint = FALSE;
	m_strLabel = _T("");
	m_strPtNote = _T("");
	m_bCorner = FALSE;
	m_strCurrentReg = _T("");
	m_strRegPtNum = _T("");
	m_bDrawOne = FALSE;
	m_iColor = -1;
	m_bAcquire = FALSE;
	m_bDrawNone = FALSE;
	m_bDrawAllReg = FALSE;
  m_bCollapseGroups = FALSE;
  srand(GetTickCount());
	//}}AFX_DATA_INIT
  mInitialized = false;
  mNonModal = true;
  mItemArray.SetSize(0, 5);
  mGroupFiles.SetSize(0, 4);
  mFileOptArray.SetSize(0, 4);
  mMontParArray.SetSize(0, 4);
  mTSparamArray.SetSize(0, 4);
  mAcqStateArray.SetSize(0, 4);
  mAddingPoints = false;
  mAddingPoly = 0;
  mMovingItem = false;
  mNavFilename = "";
  mChanged = false;
  mAcquireIndex = -1;
  mPausedAcquire = false;
  mResumedFromPause = false;
  mNavBackedUp = false;
  mSuperNumX = 0;
  mSuperNumY = 0;
  mSuperOverlap = 0;
  mMontItem = NULL;
  mMarkerShiftReg = 0;
  mLoadingMap = false;
  mShiftAIndex = -1;
  mShiftDIndex = -1;
  mShiftTIndex = -1;
  mShiftNIndex = -1;
  mMinNewFileInterval = 1;
  mNumSavedRegXforms = 0;
  mSuperCoordIndex = -1;
  mShiftByAlignStamp = -1.;
  mStartedTS = false;
  mDualMapID = -1;
  mLastMoveStageID = -1;
  mRemoveItemOnly = false;
  mSkipBacklashType = 0;
  mNextFileOptInd = -1;
  mNextMontParInd = -1;
  mLast5ptRegis = -1;
  mNumExtraFileSuffixes = 0;
  mNumExtraFilesToClose = 0;
  mAcqDlgPostponed = false;
  mMaxAngleExtraFullMont = 15.;
  mSettingUpFullMont = false;
  mLastGridPatternPoly = -1;
  mLastGridAwayFromFocus = -1;
  mLastGridInSpacing = 0.;
  mAddingFoundHoles = false;
  mDeferAddingToViewer = false;
  mMarkerShiftApplyTo = 0;
  mMarkerShiftSaveType = 0;
  mFocusCycleCounter = -1;
  mSavedBeamTiltX = EXTRA_NO_VALUE;
  mSavedAstigX = EXTRA_NO_VALUE;
  mNavAcquireDlg = NULL;
}


void CNavigatorDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CNavigatorDlg)
  DDX_Control(pDX, IDC_GOTO_MARKER, m_butGotoMarker);
  DDX_Control(pDX, IDC_CHECK_ACQUIRE, m_butAcquire);
  DDX_Control(pDX, IDC_NEW_MAP, m_butNewMap);
  DDX_Control(pDX, IDC_ADD_STAGE_POS, m_butAddStagePos);
  DDX_Control(pDX, IDC_DELETEITEM, m_butDeleteItem);
  DDX_Control(pDX, IDC_GOTO_XY, m_butGotoXy);
  DDX_Control(pDX, IDC_GOTO_POINT, m_butGotoPoint);
  DDX_Control(pDX, IDC_STAT_REGPT_NUM, m_statRegPtNum);
  DDX_Control(pDX, IDC_SPINCURRENT_REG, m_sbcCurrentReg);
  DDX_Control(pDX, IDC_SPIN_REGPT_NUM, m_sbcRegPtNum);
  DDX_Control(pDX, IDC_MOVE_ITEM, m_butMoveItem);
  DDX_Control(pDX, IDC_LOAD_MAP, m_butLoadMap);
  DDX_Control(pDX, IDC_DRAW_POLYGON, m_butDrawPoly);
  DDX_Control(pDX, IDC_DRAW_POINTS, m_butDrawPts);
  DDX_Control(pDX, IDC_CHECKCORNER, m_butCorner);
  DDX_Control(pDX, IDC_EDIT_PTNOTE, m_editPtNote);
  DDX_Control(pDX, IDC_CHECK_REGPOINT, m_butRegPoint);
  DDX_Control(pDX, IDC_LISTVIEWER, m_listViewer);
  DDX_Control(pDX, IDC_CHECKROTATE, m_butRotate);
  DDX_Check(pDX, IDC_CHECKROTATE, m_bRotate);
  DDX_Check(pDX, IDC_CHECK_REGPOINT, m_bRegPoint);
  DDX_Text(pDX, IDC_EDIT_PTLABEL, m_strLabel);
  DDV_MaxChars(pDX, m_strLabel, MAX_LABEL_SIZE);
  DDX_Text(pDX, IDC_EDIT_PTNOTE, m_strPtNote);
  DDX_Check(pDX, IDC_CHECKCORNER, m_bCorner);
  DDX_Text(pDX, IDC_STAT_CURRENTREG, m_strCurrentReg);
  DDX_Text(pDX, IDC_STAT_REGPT_NUM, m_strRegPtNum);
  DDX_Check(pDX, IDC_DRAW_ONE, m_bDrawOne);
  DDX_CBIndex(pDX, IDC_COMBOCOLOR, m_iColor);
  DDX_Check(pDX, IDC_CHECK_ACQUIRE, m_bAcquire);
  DDX_Check(pDX, IDC_DRAW_NONE, m_bDrawNone);
  DDX_Check(pDX, IDC_DRAW_ALL_REG, m_bDrawAllReg);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_REALIGNTOITEM, m_butRealign);
  DDX_Control(pDX, IDC_STATLISTHEADER, m_statListHeader);
  DDX_Control(pDX, IDC_CHECK_NAV_TS, m_butTiltSeries);
  DDX_Check(pDX, IDC_CHECK_NAV_TS, m_bTiltSeries);
  DDX_Control(pDX, IDC_CHECK_NAV_FILEATITEM, m_butFileAtItem);
  DDX_Control(pDX, IDC_CHECK_NAV_FILEATGROUP, m_butGroupFile);
  DDX_Check(pDX, IDC_CHECK_NAV_FILEATITEM, m_bFileAtItem);
  DDX_Check(pDX, IDC_CHECK_NAV_FILEATGROUP, m_bGroupFile);
  DDX_Control(pDX, IDC_STAT_NAV_FILENAME, m_statFilename);
  DDX_Text(pDX, IDC_STAT_NAV_FILENAME, m_strFilename);
  DDX_Text(pDX, IDC_STAT_NAVITEMNUM, m_strItemNum);
  DDX_Control(pDX, IDC_BUT_NAV_FILEPROPS, m_butFileProps);
  DDX_Control(pDX, IDC_BUT_UPDATEZ, m_butUpdateZ);
  DDX_Control(pDX, IDC_BUT_NAV_STATE, m_butScopeState);
  DDX_Control(pDX, IDC_BUT_NAV_TSPARAMS, m_butTSparams);
  DDX_Control(pDX, IDC_BUT_NAV_FILENAME, m_butFilename);
  DDX_Control(pDX, IDC_EDIT_PTLABEL, m_editPtLabel);
  DDX_Control(pDX, IDC_COMBOCOLOR, m_comboColor);
  DDX_Control(pDX, IDC_DRAW_ONE, m_butDrawOne);
  DDX_Control(pDX, IDC_CHECK_DUALMAP, m_checkDualMap);
  DDX_Control(pDX, IDC_BUT_DUAL_MAP, m_butDualMap);
  DDX_Check(pDX, IDC_CHECK_DUALMAP, m_bDualMap);
  DDX_Check(pDX, IDC_COLLAPSE_GROUPS, m_bCollapseGroups);
  DDX_Control(pDX, IDC_COLLAPSE_GROUPS, m_butCollapse);
  DDX_Check(pDX, IDC_SHOW_ACQUIRE_AREA, m_bShowAcquireArea);
  DDX_Control(pDX, IDC_ADD_MARKER, m_butAddMarker);
  DDX_Control(pDX, IDC_EDIT_MODE, m_butEditMode);
  DDX_Check(pDX, IDC_EDIT_MODE, m_bEditMode);
  DDX_Check(pDX, IDC_DRAW_LABELS, m_bDrawLabels);
  DDX_Control(pDX, IDC_BUT_NAV_FOCUS_POS, m_butNavFocusPos);
  DDX_Control(pDX, IDC_EDIT_FOCUS, m_butEditFocus);
  DDX_Check(pDX, IDC_EDIT_FOCUS, m_bEditFocus);
}


BEGIN_MESSAGE_MAP(CNavigatorDlg, CBaseDlg)
	//{{AFX_MSG_MAP(CNavigatorDlg)
	ON_BN_CLICKED(IDC_ADD_STAGE_POS, OnAddStagePos)
	ON_BN_CLICKED(IDC_CHECK_REGPOINT, OnCheckRegpoint)
	ON_BN_CLICKED(IDC_CHECKCORNER, OnCheckcorner)
	ON_BN_CLICKED(IDC_DELETEITEM, OnDeleteitem)
	ON_BN_CLICKED(IDC_DRAW_POINTS, OnDrawPoints)
	ON_BN_CLICKED(IDC_DRAW_POLYGON, OnDrawPolygon)
	ON_BN_CLICKED(IDC_GOTO_POINT, OnGotoPoint)
	ON_LBN_SELCHANGE(IDC_LISTVIEWER, OnSelchangeListviewer)
	ON_BN_CLICKED(IDC_LOAD_MAP, OnLoadMap)
	ON_BN_CLICKED(IDC_MOVE_ITEM, OnMoveItem)
	ON_BN_CLICKED(IDC_NEW_MAP, OnNewMap)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_REGPT_NUM, OnDeltaposSpinRegptNum)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCURRENT_REG, OnDeltaposSpincurrentReg)
	ON_EN_CHANGE(IDC_EDIT_PTLABEL, OnChangeEditPtlabel)
	ON_EN_CHANGE(IDC_EDIT_PTNOTE, OnChangeEditPtnote)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_DRAW_ONE, OnDrawOne)
	ON_BN_CLICKED(IDC_GOTO_XY, OnGotoXy)
	ON_WM_MOVE()
	ON_CBN_SELENDOK(IDC_COMBOCOLOR, OnSelendokCombocolor)
	ON_BN_CLICKED(IDC_CHECK_ACQUIRE, OnCheckAcquire)
  ON_LBN_DBLCLK(IDC_LISTVIEWER, OnDblclkListviewer)
  ON_LBN_SETFOCUS(IDC_LISTVIEWER, OnSetFocusListviewer)
  ON_BN_CLICKED(IDC_GOTO_MARKER, OnGotoMarker)
	ON_BN_CLICKED(IDC_DRAW_NONE, OnDrawNone)
	ON_BN_CLICKED(IDC_DRAW_ALL_REG, OnDrawNone)
  ON_BN_CLICKED(IDC_CHECKROTATE, OnCheckrotate)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_REALIGNTOITEM, OnRealigntoitem)
  ON_BN_CLICKED(IDC_CHECK_NAV_TS, OnCheckTiltSeries)
  ON_BN_CLICKED(IDC_CHECK_NAV_FILEATITEM, OnCheckFileatitem)
  ON_BN_CLICKED(IDC_CHECK_NAV_FILEATGROUP, OnCheckFileatgroup)
  ON_BN_CLICKED(IDC_BUT_NAV_FILEPROPS, OnButFileprops)
  ON_BN_CLICKED(IDC_BUT_NAV_STATE, OnButState)
  ON_BN_CLICKED(IDC_BUT_NAV_TSPARAMS, OnButTsparams)
  ON_BN_CLICKED(IDC_BUT_NAV_FILENAME, OnButFilename)
  ON_BN_CLICKED(IDC_BUT_UPDATEZ, OnButUpdatez)
  ON_BN_CLICKED(IDC_CHECK_DUALMAP, OnCheckDualMap)
  ON_BN_CLICKED(IDC_BUT_DUAL_MAP, OnButDualMap)
  ON_BN_CLICKED(IDC_COLLAPSE_GROUPS, OnCollapseGroups)
  ON_EN_KILLFOCUS(IDC_EDIT_PTNOTE, OnKillfocusEditBox)
  ON_EN_KILLFOCUS(IDC_EDIT_PTLABEL, OnKillfocusEditBox)
  ON_BN_CLICKED(IDC_ADD_MARKER, OnAddMarker)
  ON_BN_CLICKED(IDC_SHOW_ACQUIRE_AREA, OnShowAcquireArea)
  ON_BN_CLICKED(IDC_EDIT_MODE, OnEditMode)
  ON_BN_CLICKED(IDC_DRAW_LABELS, OnDrawLabels)
  ON_BN_CLICKED(IDC_BUT_NAV_FOCUS_POS, OnButNavFocusPos)
  ON_BN_CLICKED(IDC_EDIT_FOCUS, OnEditFocus)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// INITIALIZATION AND WINDOW-RELATED MESSAGES

BOOL CNavigatorDlg::OnInitDialog() 
{
  // label, color, X, Y, Z, type, reg, corner, extras
  int fields[10] = {37,16,25,25,21,15,19,15,8,8};
  int tabs[10], i;
	CBaseDlg::OnInitDialog();
  mDocWnd = mWinApp->mDocWnd;
	mMagTab = mWinApp->GetMagTable();
	mCamParams = mWinApp->GetCamParams();
	mImBufs = mWinApp->GetImBufs();
	mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
	mMontParam = mWinApp->GetMontParam();
	mShiftManager = mWinApp->mShiftManager;
  mBufferManager = mWinApp->mBufferManager;
  mParam = mWinApp->GetNavParams();
  mHelper = mWinApp->mNavHelper;
  mLowDoseDlg = &mWinApp->mLowDoseDlg;
  mMacroProcessor = mWinApp->mMacroProcessor;

  CRect editRect, clientRect;
  GetClientRect(clientRect);
  m_editPtNote.GetWindowRect(editRect);
  mNoteBorderX = clientRect.Width() - editRect.Width();
  mNoteHeight = editRect.Height();
  m_listViewer.GetWindowRect(editRect);
  mListBorderX = clientRect.Width() - editRect.Width();
  mListBorderY = clientRect.Height() - editRect.Height();
  m_statFilename.GetWindowRect(editRect);
  mFilenameBorderX = clientRect.Width() - editRect.Width();
  mInitialized = true;
  m_bCollapseGroups = mHelper->GetCollapseGroups();
  m_iColor = 0;

  tabs[0] = fields[0];
  for (i = 1; i < 10; i++)
    tabs[i] = tabs[i - 1] + fields[i];
  m_listViewer.SetTabStops(10, tabs);
  m_sbcCurrentReg.SetRange(1,MAX_CURRENT_REG);
  m_sbcRegPtNum.SetRange(1,MAX_REGPT_NUM);

  // Set up current registration
  // Set up default registration point number from first free value
  SetCurrentRegFromMax();
  SetRegPtNum(GetFreeRegPtNum(mCurrentRegistration, 0));

  FillListBox();
  mNewItemNum = (int)mItemArray.GetSize() + 1;
  
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// Respond to new size by fitting note edit and list box to window
void CNavigatorDlg::OnSize(UINT nType, int cx, int cy) 
{
  CRect rect;
	CDialog::OnSize(nType, cx, cy);
  if (!mInitialized)
    return;

  int newX = cx - mListBorderX;
  int newY = cy - mListBorderY;
  if (newX < 10)
    newX = 10;
  if (newY < 10)
    newY = 10;
  m_listViewer.SetWindowPos(NULL, 0, 0, newX, newY, SWP_NOZORDER | SWP_NOMOVE);

  newX = cx - mNoteBorderX;
  m_editPtNote.SetWindowPos(NULL, 0, 0, newX, mNoteHeight, SWP_NOZORDER | SWP_NOMOVE);
  m_statFilename.GetClientRect(rect);
  m_statFilename.SetWindowPos(NULL, 0, 0, cx - mFilenameBorderX, rect.Height(), 
    SWP_NOZORDER | SWP_NOMOVE);
  m_statFilename.Invalidate();
  mWinApp->RestoreViewFocus();
}

// Clean up when window is being destroyed
void CNavigatorDlg::PostNcDestroy() 
{
  mHelper->DeleteArrays();
  delete this;
	CDialog::PostNcDestroy();
}

// This override takes care of the closing situation, OnClose is redundant
void CNavigatorDlg::OnCancel()
{
  if (mLoadingMap)
    return;
  if (AskIfSave("closing window?"))
    return;
  mHelper->SetCollapseGroups(m_bCollapseGroups);

  // DO NOT call base class, it will leak memory
  //CDialog::OnCancel();
  mWinApp->NavigatorClosing();

  DestroyWindow();
}

// Returns come through here - needed to avoid closing window
// can restore focus, but do not set up a kill focus handler that does
// this because it makes other buttons unresponsive
void CNavigatorDlg::OnOK() 
{
    mWinApp->RestoreViewFocus();
}
	
void CNavigatorDlg::OnMove(int x, int y) 
{
	CDialog::OnMove(x, y);
  if (mInitialized)
    mWinApp->RestoreViewFocus();
}

////////////////////////////////////////////////////////////////////
// UPDATE/ENABLE CONTROLS; ROUTINES FOR DETERMINING MENU ITEM ENABLING

// Set the controls for the values of the current item
void CNavigatorDlg::ManageCurrentControls()
{
  CString *name;
  int index, trim, start, end, ind, doDraw = -1;
  std::set<int>::iterator iter;
  CMapDrawItem *item;
  bool exists = false;
  bool group = false;
  bool enableDraw = true;
  bool regOK;
  m_strFilename = "";
  m_strItemNum = "";
  if (SetCurrentItem()) {
    exists = true;
    m_strItemNum.Format("# %d", mCurrentItem + 1);
    m_strPtNote = mItem->mNote;
    m_strLabel = mItem->mLabel;
    m_iColor = mItem->mColor;
    m_bDrawOne = mItem->mDraw;
    m_bCorner = mItem->mCorner;
    m_bRotate = mItem->mRotOnLoad;
    m_bDualMap = mItem->IsMap() && mItem->mMapID == mDualMapID;
    m_bRegPoint = mItem->mRegPoint > 0;
    if (m_bRegPoint)
      SetRegPtNum(mItem->mRegPoint);
    m_bTiltSeries = mItem->mTSparamIndex >= 0;
  } else if (m_bCollapseGroups && mCurListSel >= 0) {
    group = true;
    GetCollapsedGroupLimits(mCurListSel, mCurrentItem, index);
    mItem = mItemArray[mCurrentItem];
    m_strItemNum.Format("# %d-%d", mCurrentItem + 1, index + 1);
    item = GetSingleSelectedItem();
    if (item)
      m_strLabel = item->mLabel;
    for (ind = mCurrentItem; ind <= index; ind++) {
      item = mItemArray[ind];
      if (doDraw < 0)
        doDraw = item->mDraw ? 1 : 0;
      if (doDraw != (item->mDraw ? 1 : 0)) {
        enableDraw = false;
        break;
      }
    }
    if (enableDraw)
      m_bDrawOne = doDraw > 0;
  }

  // Manage selection list if necessary
  // Make sure current item is in selection list, or if
  // groups are collapsed, make sure at least one selected item is in group
  index = (int)mSelectedItems.count(mCurrentItem);
  if (m_bCollapseGroups && mCurListSel >= 0) {
    GetCollapsedGroupLimits(mCurListSel, start, end);
    index = 0;
    for (iter = mSelectedItems.begin(); iter != mSelectedItems.end() && !index; iter++)
      if (*iter >= start && *iter <= end)
        index = 1;
  }

  // If this is not the case, clear out the list and add current item if valid
  if (!index) {
    mSelectedItems.clear();
    if (mCurrentItem >= 0)
      mSelectedItems.insert(mCurrentItem);
  }

  if (group || exists) {
    m_bAcquire = mItem->mAcquire;
    m_bFileAtItem = mItem->mFilePropIndex >= 0;
    index = GroupScheduledIndex(mItem->mGroupID);
    m_bGroupFile = mItem->mAcquire && index >= 0;
    if (m_bFileAtItem || m_bGroupFile) {
      if (m_bGroupFile) {
        ScheduledFile *sfile = (ScheduledFile *)mGroupFiles.GetAt(index);
        name = &sfile->filename;
      } else
        name = &mItem->mFileToOpen;
      trim = name->GetLength() - (name->ReverseFind('\\') + 1);
      m_strFilename = "(" + name->Right(trim) + ")";
    }

  }
  regOK = exists && (mItem->IsPoint() || mItem->IsMap()) && !mNavAcquireDlg;
  m_butRegPoint.EnableWindow(regOK);
  m_sbcRegPtNum.EnableWindow(regOK);
  m_statRegPtNum.EnableWindow(regOK);
  exists = exists && !mNavAcquireDlg;
  m_butCorner.EnableWindow(exists && mItem->IsPoint());
  m_butRotate.EnableWindow(exists && mItem->IsMap());
  m_checkDualMap.EnableWindow(exists && (m_bDualMap || 
    (mItem->IsMap() && !mItem->mImported)));
  m_editPtNote.EnableWindow(exists && mAcquireIndex < 0);
  m_editPtLabel.EnableWindow(exists && mAcquireIndex < 0);
  m_butDrawOne.EnableWindow(enableDraw);
  m_comboColor.EnableWindow(exists);
  UpdateData(false);
  Update();
}

// Manage the state of action buttons, some of which depend on task status
void CNavigatorDlg::Update()
{
  int index, i, start, end, numTS = 0, numAcq = 0, numFile = 0;
  CMapDrawItem *item;
  CString str;
  double timePerItem, sinceLastDone, remaining, montLeft = -1.;
  EMimageBuffer *imBuf = &mImBufs[mWinApp->Montaging() ? 1 : 0];
  BOOL noTasks = !mWinApp->DoingTasks() && mAcquireIndex < 0;
  BOOL justAcqOpen = mWinApp->DoingTasks() && mWinApp->GetJustNavAcquireOpen();
  BOOL curExists = SetCurrentItem();
  BOOL noDrawing = !(mAddingPoints || mAddingPoly || mMovingItem || mLoadingMap);
  BOOL stageMoveOK = curExists && noTasks && noDrawing && 
    RegistrationUseType(mItem->mRegistration) != NAVREG_IMPORT;
  BOOL fileOK = curExists && noTasks;
  BOOL groupOK = false;
  BOOL propsNameStateOK;
  BOOL grpExists = GetCollapsedGroupLimits(mCurListSel, start, end);
  BOOL recordingHoles = mHelper->mMultiShotDlg &&mHelper->mMultiShotDlg->RecordingHoles();
  if (grpExists)
    mItem = mItemArray[start];

  // If acquiring, update the number done and estimated completion
  if (mAcquireIndex >= 0) {
    str.Format("%d of %d Done", mNumDoneAcq, mInitialNumAcquire);
    if (mWinApp->mMontageController->DoingMontage())
      montLeft = mWinApp->mMontageController->GetRemainingTime();
    if (mNumDoneAcq || montLeft >= 0.) {
      sinceLastDone = SEMTickInterval(mLastAcqDoneTime);

      // For a montage, get estimated remaining time, and if this is the last one, then
      // set time per item to total time so this will come out as remaining time below,
      // otherwise set time per item based on estimated total of this plus previous runs
      if (montLeft >= 0.) {
        if (montLeft == mLastMontLeft) {
          timePerItem = mLastTimePerItem;
        } else {
          if (mNumDoneAcq == mInitialNumAcquire - 1)
            timePerItem = montLeft + sinceLastDone;
          else
            timePerItem = (mElapsedAcqTime + montLeft + sinceLastDone) /(mNumDoneAcq + 1);
          mLastMontLeft = montLeft;
          mLastTimePerItem = timePerItem;
        }
      } else {
        timePerItem = mElapsedAcqTime / mNumDoneAcq;
      }

      // If the current one is overdue, re-estimate time per item to give smooth increases
      // in total estimate at each completion
      if (sinceLastDone > timePerItem)
        timePerItem = (mElapsedAcqTime + sinceLastDone) / (mNumDoneAcq + 1);
      remaining = timePerItem * (mInitialNumAcquire - mNumDoneAcq) -
        B3DMIN(sinceLastDone, timePerItem);
      CTimeSpan ts(B3DNINT(B3DMAX(0., remaining) / 1000.));
      if (ts.GetDays() > 0)
        str += ts.Format(";  Estimated completion in %D days %H:%M:%S");
      else
        str += ts.Format(";  Estimated completion in %H:%M:%S");
    }
    ManageListHeader(str);
  }

  // Determine if it is OK to turn on file at group
  if ((fileOK && mItem->mFilePropIndex < 0 && mItem->mGroupID && mItem->mAcquire) ||
    grpExists) {
    groupOK = true;
    if (fileOK) {
      start = 0;
      end = (int)mItemArray.GetSize() - 1;
    }
    for (i = start; i <= end ; i++) {
      item = mItemArray[i];
      if (item->mGroupID == mItem->mGroupID) {
        if (item->mAcquire)
          numAcq++;
        if (item->mFilePropIndex >= 0)
          numFile++;
        if (item->mTSparamIndex >= 0)
          numTS++;
      }
    }
    groupOK = numAcq && !numTS && !numFile;
  }

  m_butMoveItem.EnableWindow(curExists && 
    !(mAddingPoints || mAddingPoly || mLoadingMap) && noTasks);
  m_butDrawPts.EnableWindow(!(mAddingPoly || mMovingItem || mLoadingMap) && noTasks);
  m_butDrawPoly.EnableWindow(!(mAddingPoints || mMovingItem || mLoadingMap) && noTasks);
  //m_butUpdatePos.EnableWindow(curExists && mItem->IsNotMap() && noDrawing &&
  //  mAcquireIndex < 0);
  m_butUpdateZ.EnableWindow((curExists || grpExists) && noTasks && noDrawing);
  m_butGotoPoint.EnableWindow(stageMoveOK && !recordingHoles);
  m_butGotoXy.EnableWindow(stageMoveOK);
  m_butGotoMarker.EnableWindow(noTasks && noDrawing);
  m_butGotoXy.SetWindowText(recordingHoles ? "IS To XY" : "Go To XY");
  m_butGotoMarker.SetWindowText(recordingHoles ? "IS To Marker" : "Go To Marker");
  m_butAddStagePos.EnableWindow(noTasks && noDrawing);
  if (curExists)
    item = FindMontMapDrawnOn(mItem);
  else
    item = FindMontMapDrawnOn(GetSingleSelectedItem());
  m_butLoadMap.EnableWindow((curExists || item) && (noTasks || justAcqOpen) && noDrawing
    && !mCamera->CameraBusy() && (mItem->IsMap() || item != NULL));
  m_butLoadMap.SetWindowText(item ? "Load Piece" : "Load Map");

  m_butNewMap.EnableWindow(noDrawing && noTasks && mWinApp->mStoreMRC != NULL);
  m_butDualMap.EnableWindow(noDrawing && noTasks && mDualMapID >= 0);
  m_butDeleteItem.EnableWindow((curExists || grpExists) && noDrawing && 
    mAcquireIndex < 0 && !mHelper->GetAcquiringDual() && !mWinApp->DoingComplexTasks() &&
    !mNavAcquireDlg);
  m_butRealign.EnableWindow(curExists && noDrawing && mAcquireIndex < 0 && noTasks &&
    !mCamera->CameraBusy());
  m_listViewer.EnableWindow(!mAddingPoints && !mLoadingMap && mAcquireIndex < 0);
  index = -1;

  if (curExists || grpExists)
    index = GroupScheduledIndex(mItem->mGroupID);
  m_butAcquire.EnableWindow(!mNavAcquireDlg && ((fileOK && mItem->mTSparamIndex < 0) || 
    (grpExists && noTasks && !numTS)));
  m_butTiltSeries.EnableWindow(fileOK && !mItem->mAcquire);
  m_butFileAtItem.EnableWindow(fileOK && mItem->mAcquire && mItem->mTSparamIndex < 0 &&
    index < 0);
  m_butGroupFile.EnableWindow(groupOK && noTasks);
  propsNameStateOK = (fileOK && (mItem->mFilePropIndex >= 0 || index >= 0)) ||
    (grpExists && noTasks && index >= 0);
  m_butFileProps.EnableWindow(propsNameStateOK);
  m_butFilename.EnableWindow(propsNameStateOK);
  m_butTSparams.EnableWindow(fileOK && mItem->mTSparamIndex >= 0);
  m_butScopeState.EnableWindow(propsNameStateOK);
  m_butNavFocusPos.EnableWindow(curExists && noTasks && mWinApp->LowDoseMode() &&
    (mItem->mAcquire || mItem->mTSparamIndex >= 0));
  mEditFocusEnabled = noTasks && noDrawing && mLowDoseDlg->GetTrulyLowDose() &&
    !mLowDoseDlg->m_iDefineArea;
  m_butEditFocus.EnableWindow(mEditFocusEnabled);
  m_sbcCurrentReg.EnableWindow(noTasks);
  m_butCollapse.EnableWindow(mAcquireIndex < 0);
  mHelper->UpdateStateDlg();
  if (mNavAcquireDlg)
    mNavAcquireDlg->ExternalUpdate();
  UpdateAddMarker();
}

// A call specifically for the Add Marker button which depends on the active buffer
void CNavigatorDlg::UpdateAddMarker(void)
{
  m_butAddMarker.EnableWindow(OKtoAddMarkerPoint());
}

// Return item type
int CNavigatorDlg::GetItemType()
{
  if (!SetCurrentItem())
    return -1;
  return mItem->mType;
}

// Evaluate whether a transformation can be done
BOOL CNavigatorDlg::TransformOK()
{
  CMapDrawItem *item;
  int minPts = 1;
  int i, reg, nRegPt = 0;
  int maxRegis = 0, nCurRegPt = 0;

  // Count up number of registration points and get max registration
  for (i = 0; i < mItemArray.GetSize(); i++) {
     item = mItemArray[i];
     if (maxRegis < item->mRegistration)
       maxRegis = item->mRegistration;
     if (item->mRegistration == mCurrentRegistration && item->mRegPoint > 0)
       nCurRegPt++;
  }

  // Look at all other registrations and stop if one has more than 1 reg point
  if (nCurRegPt >= minPts) {
    for (reg = 1; reg <= maxRegis; reg++) {
      if (reg == mCurrentRegistration)
        continue;
      nRegPt = 0;
      for (i = 0; i < mItemArray.GetSize(); i++) {
        item = mItemArray[i];
        if (item->mRegistration == reg && item->mRegPoint > 0)
          nRegPt++;
      }
      if (nRegPt >= minPts)
        break;
    }
  }
  return nRegPt >= minPts && nCurRegPt >= minPts;
}

// Find out if enough corners for a corner montage
BOOL CNavigatorDlg::CornerMontageOK()
{
  CMapDrawItem *item;
  int i, nCorner = 0;

  // Count up corners in current registration
  for (i = 0; i < mItemArray.GetSize(); i++) {
     item = mItemArray[i];
     if (item->mRegistration == mCurrentRegistration && item->mCorner)
       nCorner++;
  }
  return nCorner > 2;
}

// Find out if there are any acquires of regular or TS type in current registration
BOOL CNavigatorDlg::AcquireOK(bool tiltSeries, int startInd, int endInd)
{
  CMapDrawItem *item;
  if (endInd < 0)
    endInd = (int)mItemArray.GetSize() - 1;
  startInd = B3DMAX(0, startInd);
  endInd = B3DMIN((int)mItemArray.GetSize() - 1, endInd);
  for (int i = startInd; i <= endInd; i++) {
     item = mItemArray[i];
     if (item->mRegistration == mCurrentRegistration && 
       (!tiltSeries && item->mAcquire || tiltSeries && item->mTSparamIndex >= 0))
       return true;
  }
  return false;
}

// Check if it is OK to add a grid of points
BOOL CNavigatorDlg::OKtoAddGrid(bool likeLast)
{
  CString label, lastlab;
  int num, numacq;
  bool points, polyMap;
  if (!NoDrawing())
    return false;
  if (!SetCurrentItem(true))
    return false;

  // No imports are allowed; only polygons or points with group ID
  if (mItem->mImported)
    return false;
  points = mItem->IsPoint() && mItem->mGroupID;
  polyMap = mItem->IsPolygon() || mItem->IsMap();
  if (likeLast)
    return ((points && !mLastGridPatternPoly) || (polyMap && mLastGridPatternPoly > 0));
  if (!points)
    return (polyMap);
  num = CountItemsInGroup(mItem->mGroupID, label, lastlab, numacq);

  // If there is only one point, it must match registration and conditions used for
  // last 5-point draw
  if (num == 1) {
    if (mItem->mRegistration != mLast5ptRegis)
      return false;
    return (FindBufferWithMontMap(mItem->mDrawnOnMapID) >= 0 ? 1 : 0) ==
      (mLast5ptOnImage ? 1 : 0);
  }
  return (num == 3 || num == 5 || num == 7);
}

// Check if it is OK to average cropped-out areas marked by a group, if so return group ID
int CNavigatorDlg::OKtoAverageCrops(void)
{
  CString label, lastlab;
  int numacq;
  if (!SetCurrentItem(true))
    return false;
  if (mItem->mImported || mItem->IsNotPoint() || !mItem->mGroupID)
    return false;
  if (CountItemsInGroup(mItem->mGroupID, label, lastlab, numacq) > 1)
    return mItem->mGroupID;
  return 0;
}

// Registration change is OK as long as it is not a registration point
BOOL CNavigatorDlg::RegistrationChangeOK(void)
{ 
  if (!SetCurrentItem())
    return false;
  return (mItem->mRegPoint <= 0);
}

// Find out whether current item is in a group
BOOL CNavigatorDlg::CurrentIsInGroup(void)
{
  if (!SetCurrentItem(true))
    return false;
  return mItem->mGroupID != 0;
}

// Find out if the current item is imported
BOOL CNavigatorDlg::CurrentIsImported(void)
{
  if (!SetCurrentItem())
    return false;
  return mItem->mImported != 0;
}

// Change the registration of an item
int CNavigatorDlg::ChangeItemRegistration(void)
{
  if (!SetCurrentItem())
    return 1;
  CString str;
  int reg = mItem->mRegistration;
  if (!KGetOneInt("New Registration number for current item:", reg))
    return 1;
  if (ChangeItemRegistration(mCurrentItem, reg, str)) {
    AfxMessageBox(str);
    return 1;
  }
  return 0;
}

// Change the registration for given item index, put errors in str
int CNavigatorDlg::ChangeItemRegistration(int index, int newReg, CString &str)
{
  CMapDrawItem *item = mItemArray[index];
  int use, useold, regOld = item->mRegistration;
  if (newReg < 1 || newReg > MAX_CURRENT_REG) {
    str.Format("Registration number must be between 1 and %d", MAX_CURRENT_REG);
    return 1;
  }
  use = RegistrationUseType(newReg);
  useold = RegistrationUseType(regOld);
  if (use == NAVREG_IMPORT && useold == NAVREG_REGULAR) {
    str.Format("You cannot change item %d from a regular registration to one used for"
      " imported items (%d)", index, newReg);
    return 1;
  }
  if (useold == NAVREG_IMPORT && use == NAVREG_REGULAR) {
    str.Format("You cannot change item %d from a registration one used for"
      " imported items to a regular registration (%d)", index, newReg);
    return 1;
  }
  item->mRegistration = newReg;
  UpdateListString(index);
  SetChanged(true);

  // If it is a map look for buffers with this map loaded and change their registration
  if (item->IsMap())
    mHelper->ChangeAllBufferRegistrations(item->mMapID, regOld, newReg);
  Redraw();
  return 0;
}

// Invokes the realign routine for the current item, or for the item being acquired
// if a macro is being run at acquire points
int CNavigatorDlg::RealignToCurrentItem(BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS, BOOL justMoveIfSkipCen)
{
  if (GetCurrentOrAcquireItem(mItem) < 0)
    return 1;
  return RealignToAnItem(mItem, restore, resetISalignCrit, maxNumResetAlign, leaveZeroIS,
    justMoveIfSkipCen);
}

// Or, aligns to another item by index
int CNavigatorDlg::RealignToOtherItem(int index, BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS, BOOL justMoveIfSkipCen)
{
  if (!GetOtherNavItem(index)) {
    SEMMessageBox("The Navigator item index is out of range", MB_EXCLAME);
    return 1;
  }
  return RealignToAnItem(mItem, restore, resetISalignCrit, maxNumResetAlign, leaveZeroIS,
    justMoveIfSkipCen);
}

// The actual routine for calling the helper and giving error messages
int CNavigatorDlg::RealignToAnItem(CMapDrawItem * item, BOOL restore, 
  float resetISalignCrit, int maxNumResetAlign, int leaveZeroIS, BOOL justMoveIfSkipCen)
{
  int err = mHelper->RealignToItem(item, restore, resetISalignCrit, maxNumResetAlign, 
    leaveZeroIS, justMoveIfSkipCen);
  if (err && err < 4) 
    SEMMessageBox("An error occurred trying to access a map image file", MB_EXCLAME);
  if (err == 4 || err == 5) 
    SEMMessageBox(err == 4 ? "There is no appropriate map image to align to"
    : "The only available map image is too small to align to", MB_EXCLAME);
  return err;
}

// Simply move to an item; current or acquire item if index < 0
int CNavigatorDlg::MoveToItem(int index, bool skipZ)
{
  if (index < 0) {
    if (GetCurrentOrAcquireItem(mItem) < 0)
      return 1;
  } else {
    if (!GetOtherNavItem(index)) {
      SEMMessageBox("The Navigator item index is out of range", MB_EXCLAME);
      return 1;
    }
  }
  return MoveStage(axisXY | (skipZ ? 0 : axisZ));
}

// The button to realign is pressed
void CNavigatorDlg::OnRealigntoitem()
{
  mWinApp->RestoreViewFocus();
  RealignToCurrentItem(true, 0., 0, 0, false);
}

/////////////////////////////////////////////////////////////////////
// RESPONSES TO USER CHANGE OF ITEM STATE CONTROLS
void CNavigatorDlg::OnDrawNone() 
{
  UpdateData(true);	
  mWinApp->RestoreViewFocus();
  Redraw();
}

void CNavigatorDlg::OnDrawLabels()
{
  UpdateData(true);	
  mWinApp->RestoreViewFocus();
  Redraw();
}

// The corner checkbox
void CNavigatorDlg::OnCheckcorner() 
{
  if (UpdateIfItem())
    return;
  mItem->mCorner = m_bCorner;
  UpdateListString(mCurrentItem);
  SetChanged(true);
  Update();
}

// The rotate on load checkbox
void CNavigatorDlg::OnCheckrotate()
{
  if (UpdateIfItem())
    return;
  mItem->mRotOnLoad = m_bRotate;
  SetChanged(true);
}

// The checkbox for the dual map item
void CNavigatorDlg::OnCheckDualMap()
{
  if (UpdateIfItem())
    return;
  if (m_bDualMap) {

    // Find existing item and update its string after setting new map ID
    mFoundItem = -1;
    if (mDualMapID > 0)
      FindItemWithMapID(mDualMapID);
    mDualMapID = mItem->mMapID;
    if (mFoundItem >= 0)
      UpdateListString(mFoundItem);
  } else
    mDualMapID = -1;
  UpdateListString(mCurrentItem);
  Update();
}

// Registration point number spin button
void CNavigatorDlg::OnDeltaposSpinRegptNum(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  CMapDrawItem *item;
	int newPos = mRegPointNum + pNMUpDown->iDelta;
  int maxPtNum = 0;
  *pResult = 1;

  mWinApp->RestoreViewFocus();
  if (newPos < 1)
    return;
  if (pNMUpDown->iDelta > 0) {

    // Find maximum of all registration point numbers
    for (int i = 0; i < mItemArray.GetSize(); i++) {
      item = mItemArray[i];
      if (maxPtNum < item->mRegPoint)
        maxPtNum = item->mRegPoint;
    }
    
    // Do not allow it to go one past the maximum
    if (newPos > maxPtNum + 1 || newPos > MAX_REGPT_NUM)
      return;
  }

  mRegPointNum = newPos;
  m_strRegPtNum.Format("%d", mRegPointNum);
  UpdateData(false);
	*pResult = 0;
  if (!SetCurrentItem())
    return;
  if (mItem->mRegPoint > 0)
    mItem->mRegPoint = newPos;
  UpdateListString(mCurrentItem);
  SetChanged(true);
}

// Current registration spin button
void CNavigatorDlg::OnDeltaposSpincurrentReg(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newPos = mCurrentRegistration;
  mWinApp->RestoreViewFocus();
  do {
    newPos += pNMUpDown->iDelta;
    if (newPos < 1 || newPos > MAX_CURRENT_REG) {
      *pResult = 1;
      return;
    } 
  } while (RegistrationUseType(newPos) == NAVREG_IMPORT);
  mCurrentRegistration = newPos;
  m_strCurrentReg.Format("%d", mCurrentRegistration);
  UpdateData(false);
	*pResult = 0;
  Update();
  Redraw();
}

// Registration point number is turned on or off
void CNavigatorDlg::OnCheckRegpoint() 
{
  if (UpdateIfItem())
    return;
  if (m_bRegPoint) {
    mItem->mRegPoint = GetFreeRegPtNum(mItem->mRegistration, mRegPointNum);
    SetRegPtNum(mItem->mRegPoint); 
    UpdateData(false);
  } else {
    mItem->mRegPoint = 0;
  }
  UpdateListString(mCurrentItem);
  SetChanged(true);
  Update();
}

// Color change
void CNavigatorDlg::OnSelendokCombocolor() 
{
  if (UpdateIfItem())
    return;
  mItem->mColor = m_iColor;
  UpdateListString(mCurrentItem);
  SetChanged(true);
	Redraw();
}

// Label change
void CNavigatorDlg::OnChangeEditPtlabel() 
{
  if (!SetCurrentItem())
    return;
  UpdateData(true);
  mItem->mLabel = m_strLabel;
  UpdateListString(mCurrentItem);
  SetChanged(true);
  Redraw();
}

// Note change
void CNavigatorDlg::OnChangeEditPtnote() 
{
  if (!SetCurrentItem())
    return;
  UpdateData(true);
  mItem->mNote = m_strPtNote;
  UpdateListString(mCurrentItem);
  SetChanged(true);
}

// Restore focus when leave the boxes
void CNavigatorDlg::OnKillfocusEditBox()
{
  mWinApp->RestoreViewFocus();
}

// Draw flag changed
void CNavigatorDlg::OnDrawOne() 
{
   CMapDrawItem *item;
 int start, end, ind;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true)) 
    return;
  UpdateData(true);
  if (m_bCollapseGroups && mListToItem[mCurListSel] < 0) {
    GetCollapsedGroupLimits(mCurListSel, start, end);
    for (ind = start; ind <= end; ind++) {
      item = mItemArray[ind];
      item->mDraw = m_bDrawOne;
      UpdateListString(ind);
    }
  } else {
    mItem->mDraw = m_bDrawOne;
    UpdateListString(mCurrentItem);
  }
  SetChanged(true);
  Redraw();
}

// Show acquire area changed
void CNavigatorDlg::OnShowAcquireArea()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  Redraw();
}

// Edit mode changed
void CNavigatorDlg::OnEditMode()
{
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  if (m_bEditMode && !mHelper->GetEditReminderPrinted()) {
    mWinApp->AppendToLog("\r\nEDIT MODE REMINDER:\r\n"
      "    Left-click to select\r\n"
      "    Ctrl-Left-Click to add or remove from selected points\r\n"
      "    Left-double-click to delete point if already selected\r\n"
      "    Shift-Left-double-click to remove one position from multiple Record array\r\n"
      "    Middle-click to add a point\r\n    Right-click to move current point\r\n"
      "    Backspace to delete current point\r\n");
    mHelper->SetEditReminderPrinted(true);
  }
  if (m_bEditFocus) {
    m_bEditFocus = false;
    UpdateData(false);
  }
  if (m_bEditMode) {
    EMimageBuffer *imBuf = mWinApp->mMainView->GetActiveImBuf();
    imBuf->mHasUserPt = false;
  }
  Update();
}

// Edit Focus changed
void CNavigatorDlg::OnEditFocus()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  if (m_bEditMode) {
    m_bEditMode = false;
    UpdateData(false);
  }
  AddFocusAreaPoint(true);
}

// If edit focus is on and active, set that there is a user point and get it positioned
void CNavigatorDlg::AddFocusAreaPoint(bool drawFirst)
{
  EMimageBuffer *imBuf = mWinApp->mMainView->GetActiveImBuf();
  if (m_bEditFocus && mEditFocusEnabled) {
    if (drawFirst)
      Redraw();
    imBuf->mHasUserPt = true;
    mLowDoseDlg->FixUserPoint(imBuf, 0);
  }
}

// Flag to acquire changed
void CNavigatorDlg::OnCheckAcquire() 
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true)) 
    return;
  UpdateData(true);
  if (m_bCollapseGroups && mListToItem[mCurListSel] < 0) {
    ToggleGroupAcquire(true);
    return;
  }
  mItem->mAcquire = m_bAcquire;
  if (!m_bAcquire)
    mHelper->EndAcquireOrNewFile(mItem);
  ManageCurrentControls();
  UpdateListString(mCurrentItem);
  SetChanged(true);
  Redraw();
  if (m_bAcquire)
    AddFocusAreaPoint(false);
}

// Call from menu to toggle the acquire state of current whole group, or from acquire
// checkbox to toggle state of current collapsed group
void CNavigatorDlg::ToggleGroupAcquire(bool collapsedGroup)
{
  CMapDrawItem *item;
  BOOL acquire, needFocusArea = false;
  int start = 0, end = (int)mItemArray.GetSize() - 1;
  if (!SetCurrentItem(true)) 
    return;
  if (mItem->IsPolygon() || !mItem->mGroupID || mItem->mTSparamIndex >= 0)
    return;
  acquire = !mItem->mAcquire;
  if (collapsedGroup)
    GetCollapsedGroupLimits(mCurListSel, start, end);

  for (int i = start; i <= end; i++) {
    item = mItemArray[i];
    if (item->IsNotPolygon() && item->mGroupID == mItem->mGroupID && 
      mItem->mTSparamIndex < 0) {
      if (acquire) {
        if (!item->mAcquire)
          needFocusArea = true;
        item->mAcquire = true;
      } else if (item->mAcquire) {
        item->mAcquire = false;
        mHelper->EndAcquireOrNewFile(item);
      }
      UpdateListString(i);
    }
  }
  ManageCurrentControls();
  SetChanged(true);
  Redraw();
  if (needFocusArea)
    AddFocusAreaPoint(false);
}

// Flag to take tilt series changed
void CNavigatorDlg::OnCheckTiltSeries()
{
  if (UpdateIfItem())
    return;
  if (mItem->mTSparamIndex < 0 && m_bTiltSeries) {

    // Do second message if fail?
    mHelper->NewAcquireFile(mCurrentItem, NAVFILE_TS, NULL);
  } else if (mItem->mTSparamIndex >= 0 && !m_bTiltSeries) {
    mHelper->EndAcquireOrNewFile(mItem);
  }
  SetChanged(true);
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
  UpdateListString(mCurrentItem);
  Redraw();
  AddFocusAreaPoint(false);
}

// Open a file at the single item
void CNavigatorDlg::OnCheckFileatitem()
{
  if (UpdateIfItem())
    return;
  if (mItem->mFilePropIndex < 0 && m_bFileAtItem) {
    mHelper->NewAcquireFile(mCurrentItem, NAVFILE_ITEM, NULL);
  } else if (mItem->mFilePropIndex >= 0 && !m_bFileAtItem) {
    mHelper->EndAcquireOrNewFile(mItem);
  }
  SetChanged(true);
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
  UpdateListString(mCurrentItem);
}

// Open a file for the group
void CNavigatorDlg::OnCheckFileatgroup()
{
  ScheduledFile *sched;
  int index, groupID;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true) || !mItem->mGroupID)
    return;

  // Save the group ID, mItem does not stay set after opening dialogs
  groupID = mItem->mGroupID;
  UpdateData(true);
  index = GroupScheduledIndex(groupID);
  if (index < 0 && m_bGroupFile) {
    sched = new ScheduledFile;
    if (mHelper->NewAcquireFile(mCurrentItem, NAVFILE_GROUP, sched)) {
      delete sched;
    } else {
      sched->groupID = groupID;
      mGroupFiles.Add(sched);
    }
  } else if (index >= 0 && !m_bGroupFile) {
    mHelper->EndAcquireOrNewFile(mItem, true);
  }
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
  UpdateGroupStrings(groupID);
}

// Open file properties dialog
void CNavigatorDlg::OnButFileprops()
{
  ScheduledFile *sched;
  int fileType;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true) || (m_bCollapseGroups && mListToItem[mCurListSel] < 0 && 
    GroupScheduledIndex(mItem->mGroupID) < 0))
    return;
  sched = mHelper->GetFileTypeAndSchedule(mItem, fileType);
  if (fileType >= 0) {
    mHelper->SetFileProperties(mCurrentItem, fileType, sched, true, false);
    SetChanged(true);
  }
  mWinApp->RestoreViewFocus();
}

// Save the current state
void CNavigatorDlg::OnButState()
{
  ScheduledFile *sched;
  CString str;
  StateParams *state = NULL;
  int fileType;
  int *indexp;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true) || (m_bCollapseGroups && mListToItem[mCurListSel] < 0 && 
    GroupScheduledIndex(mItem->mGroupID) < 0))
    return;
  indexp = &mItem->mStateIndex;
  sched = mHelper->GetFileTypeAndSchedule(mItem, fileType);
  if (sched) 
    indexp = &sched->stateIndex;
  if (mItem->mFilePropIndex < 0 && !sched)
    return;
  if (*indexp >= 0) {
    state = mHelper->ExistingStateParam(*indexp, true);
    if (!state)
      AfxMessageBox("Existing stored state for this item could not be found, creating"
      " a new one", MB_EXCLAME);
  } 
  if (!state) {
    str.Format("Stored imaging state in a new parameter set for item # %d, label %s",
      mCurrentItem + 1, (LPCTSTR)mItem->mLabel);
    state = mHelper->NewStateParam(true);
    *indexp = (int)mAcqStateArray.GetSize() - 1;
  } else
    str.Format("Updated the stored imaging state for item # %d, label %s",
      mCurrentItem + 1, (LPCTSTR)mItem->mLabel);
  mWinApp->AppendToLog(str);
  mHelper->StoreCurrentStateInParam(state, mWinApp->LowDoseMode(), true);
  if (sched)
    UpdateGroupStrings(mItem->mGroupID);
  else
    UpdateListString(mCurrentItem);
  mHelper->UpdateStateDlg();
}

// Open TS params dialog
void CNavigatorDlg::OnButTsparams()
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem() || mItem->mTSparamIndex < 0)
    return;
  mHelper->SetTSParams(mCurrentItem);
  SetChanged(true);
  mWinApp->RestoreViewFocus();
}

// Change the filename for the new file
void CNavigatorDlg::OnButFilename()
{
  ScheduledFile *sched;
  int fileType;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem(true) || (m_bCollapseGroups && mListToItem[mCurListSel] < 0 && 
    GroupScheduledIndex(mItem->mGroupID) < 0))
    return;
  sched = mHelper->GetFileTypeAndSchedule(mItem, fileType);
  if (fileType >= 0) {
    mHelper->SetOrChangeFilename(mCurrentItem, fileType, sched);
    SetChanged(true);
  }
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
}

// Store focus position
void CNavigatorDlg::OnButNavFocusPos()
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem())
    return;
  mHelper->SaveLDFocusPosition(true, mItem->mFocusAxisPos, mItem->mRotateFocusAxis,
    mItem->mFocusAxisAngle, mItem->mFocusXoffset, mItem->mFocusYoffset, true);
  UpdateListString(mCurrentItem);
  SetChanged(true);
}

// Turn the collapsing of groups on or off
void CNavigatorDlg::OnCollapseGroups()
{
  int oldCurrent = mCurrentItem;
  UpdateData(true);
  mCurListSel = -1;
  
  // Tell FillListBox not to manage controls because current item needs to be restored
  if (m_bCollapseGroups) {
    MakeListMappings();
    FillListBox(true);
    if (oldCurrent >= 0) {
      mCurListSel = mItemToList[oldCurrent];
      IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
    }
  } else {
    FillListBox(true);
    mCurListSel = mCurrentItem = oldCurrent;
  }
  if (mCurListSel >= 0)
    m_listViewer.SetCurSel(mCurListSel);

  // Do not cause a redraw before all of the above is settled
  ManageCurrentControls();
  mWinApp->RestoreViewFocus();
}

/////////////////////////////////////////////////////////////////////
// LIST BOX MESSAGE RESPONSES AND ROUTINES TO MANAGE LIST BOX TEXT

// The selection is changed in the viewer
void CNavigatorDlg::OnSelchangeListviewer() 
{
  // Turn off moving of current item
  if (mMovingItem)
    OnMoveItem();

  // Turn off adding a polygon if points have been added already
  if (mAddingPoly > 1)
    OnDrawPolygon();

  // Get the new current point
  mSelectedItems.clear();
  mCurListSel = m_listViewer.GetCurSel();
  if (mCurListSel == LB_ERR) {
    mCurListSel = -1;
    mCurrentItem = -1;
  } else {
    IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
    ManageCurrentControls();
  }
  mWinApp->RestoreViewFocus(true);
  AddFocusAreaPoint(true);
}

// An item has been dragged from the old index to the new
void CNavigatorDlg::OnListItemDrag(int oldIndex, int newIndex)
{
  // Remove item from old place in array and insert it in new
  int moveInc, toInd, fromInd, i, num;
  int sel = m_listViewer.GetCurSel();
  CMapDrawItem *item = mItemArray.GetAt(oldIndex);
  mWinApp->RestoreViewFocus(true);
  if (!m_bCollapseGroups) {
    mItemArray.RemoveAt(oldIndex);
    mItemArray.InsertAt(newIndex, item);
  } else {

    // If there are collapsed groups, get number to move and starting index
    num = 1;
    if (IndexOfSingleOrFirstInGroup(oldIndex, fromInd)) {
      GetCollapsedGroupLimits(oldIndex, fromInd, num);
      num = num + 1 - fromInd;
    }

    // Moving up, they all come from and go to same place; going down, they come
    // from and go to successively higher places
    if (oldIndex < newIndex) {
      moveInc = 0;
      if (newIndex >= m_listViewer.GetCount() - 1)
        toInd = newIndex;
      else {
        IndexOfSingleOrFirstInGroup(newIndex+1, toInd);
        toInd--;
      }
    } else {
      moveInc = 1;
      IndexOfSingleOrFirstInGroup(newIndex, toInd);
    }
    
    for (i = 0; i < num; i++) {
      item = mItemArray[fromInd];
      mItemArray.RemoveAt(fromInd);
      mItemArray.InsertAt(toInd, item);
      fromInd += moveInc;
      toInd += moveInc;
    }
    MakeListMappings();
  }
  SetChanged(true);

  // Overblown logic: the moved item is always the current selection!
  if (mCurListSel < 0 || mCurListSel == oldIndex)
    mCurListSel = newIndex;
  else if (oldIndex < newIndex && mCurListSel > oldIndex && mCurListSel <= newIndex)
    mCurListSel--;
  else if (oldIndex > newIndex && mCurListSel < oldIndex && mCurListSel >= newIndex)
    mCurListSel++;
  m_listViewer.SetCurSel(mCurListSel);
  IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
  if (m_bCollapseGroups) {
    GetCollapsedGroupLimits(mCurListSel, mCurrentItem, fromInd);
    m_strItemNum.Format("# %d-%d", mCurrentItem + 1, fromInd + 1);
  } else
    m_strItemNum.Format("# %d", mCurrentItem + 1);
  mSelectedItems.clear();
  mSelectedItems.insert(mCurrentItem);
  UpdateData(false);
}

// Load a map if it is double clicked and conditions are right
void CNavigatorDlg::OnDblclkListviewer() 
{
  if (!SetCurrentItem() || (mWinApp->DoingTasks() && !mWinApp->GetJustNavAcquireOpen()) ||
    mAddingPoints || mAddingPoly || mMovingItem || !(mItem->IsMap() ||
      FindMontMapDrawnOn(mItem)) || mCamera->CameraBusy())
    return;
  OnLoadMap();
  mWinApp->SetNavTableHadFocus(true);
}

void CNavigatorDlg::OnSetFocusListviewer()
{
  mWinApp->SetNavTableHadFocus(true);
}

// Let arrow keys move the selection up and down
void CNavigatorDlg::MoveListSelection(int direction)
{
  int newIndex = mCurListSel + direction;
  if (newIndex < 0 || newIndex >= m_listViewer.GetCount())
    return;
  mCurListSel = newIndex;
  IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
  m_listViewer.SetCurSel(mCurListSel);
  mSelectedItems.clear();
  ManageCurrentControls();
  Redraw();
  AddFocusAreaPoint(false);
}


void CNavigatorDlg::ProcessCKey(void)
{
  if (mWinApp->DoingTasks() || !SetCurrentItem() || mItem->IsNotPoint())
    return;
  mItem->mCorner = !mItem->mCorner;
  UpdateListString(mCurrentItem);
  m_bCorner = mItem->mCorner;
  UpdateData(false);
  SetChanged(true);
}

// Variants on the A key to toggle acquire state
void CNavigatorDlg::ProcessAkey(BOOL ctrl, BOOL shift)
{
  CMapDrawItem *item;
  int start = 0;
  int end = (int)mItemArray.GetSize() - 1;
  BOOL oldAcquire, acquire, toggle = false, needFocusArea = false;
  if (!SetCurrentItem(true))
    return;

  // Shift A means record current spot, then do between there and next selected spot
  acquire = !mItem->mAcquire;
  if (shift && !ctrl) {
    if (ProcessRangeKey("A again for acquire", mShiftAIndex, start, end))
      return;
    toggle = true;
  } else if (!ctrl && !shift) {
    start = end = mCurrentItem;
    GetCollapsedGroupLimits(mCurListSel, start, end);
  }
  for (int index = start; index <= end; index++) {
    item = mItemArray[index];
    if (item->mTSparamIndex >= 0)
      continue;
    oldAcquire = item->mAcquire;
    if (toggle)
      item->mAcquire = !item->mAcquire;
    else
      item->mAcquire = acquire;
    if (!item->mAcquire && oldAcquire)
      mHelper->EndAcquireOrNewFile(item);
    if (item->mAcquire && !oldAcquire)
      needFocusArea = true;
    UpdateListString(index);
    SetChanged(true);
  }
  mShiftAIndex = -1;
  ManageCurrentControls();
  Redraw();
  if (needFocusArea)
    AddFocusAreaPoint(false);
}

// Delete a contiguous set of points with Shift D at both ends of range
void CNavigatorDlg::ProcessDKey(void)
{
  CString str;
  CMapDrawItem *item;
  int start, end, ind, num = 0;
  if (ProcessRangeKey("D again to delete", mShiftDIndex, start, end))
    return;

  // Count points in range
  for (ind = start; ind <= end; ind++) {
    item = mItemArray[ind];
    if (item->IsPoint())
      num++;
  }

  // Confirm and delete in inverse order, rather clumsily
  if (num > 3) {
    str.Format("Do you really want to delete the %d points from index %d to %d?", 
      num, start + 1, end + 1);
    if (AfxMessageBox(str, MB_QUESTION) == IDNO)
      return;
  }
  for (ind = end; ind >= start; ind--) {
    item = mItemArray[ind];
    if (item->IsPoint()) {
      mCurrentItem = ind;
      mCurListSel = ind;
      m_listViewer.SetCurSel(mCurListSel);
      OnDeleteitem();
    }
  }
}

// Start tilt series at contiguous set of points with Shift T at both ends of range
void CNavigatorDlg::ProcessTKey(void)
{
  CMapDrawItem *item;
  bool needFocusArea = false;
  int start, end, ind, allOn = true;
  if (ProcessRangeKey("T again for TS", mShiftTIndex, start, end))
    return;

  // See if they are all already on, in which case they will be turned off
  // Here and in next loop, ignore a map if there is a higher mag map in range
  for (ind = start; ind <= end; ind++) {
    item = mItemArray[ind];
    if (item->mTSparamIndex < 0 && !item->mAcquire && 
      !AtSamePosAsHigherMagMapInRange(ind, start, end)) {
      allOn = false;
      break;
    }
  }

  // Then loop and turn on ones not on, or turn off if all on
  for (ind = start; ind <= end; ind++) {
    if (AtSamePosAsHigherMagMapInRange(ind, start, end))
      continue;
    item = mItemArray[ind];
    if (item->mTSparamIndex < 0 && !item->mAcquire) {
      mHelper->NewAcquireFile(ind, NAVFILE_TS, NULL);
      UpdateListString(ind);
      needFocusArea = true;
    } else if (allOn && !item->mAcquire) {
      mHelper->EndAcquireOrNewFile(item);
      UpdateListString(ind);
    }
  }
  ManageCurrentControls();
  Redraw();
  if (needFocusArea)
    AddFocusAreaPoint(false);
}

// Returns true if this item is a map and there is a higher mag map within the given range
// of items
bool CNavigatorDlg::AtSamePosAsHigherMagMapInRange(int itemInd, int startInd, int endInd)
{
  CMapDrawItem *check, *item = mItemArray[itemInd];
  if (!item->mAtSamePosID || item->IsNotMap())
    return false;
  for (int ind = startInd; ind <= endInd; ind++) {
    if (ind == itemInd)
      continue;
    check = mItemArray[ind];
    if (item->mAtSamePosID == check->mAtSamePosID && check->IsMap() &&
      item->mMapMagInd < check->mMapMagInd)
      return true;
  }
  return false;
}


// Start new file at item at contiguous set of points with Shift N at both ends of range
void CNavigatorDlg::ProcessNKey(void)
{
  CMapDrawItem *item;
  int start, end, ind, interval, numAcq = 0, numOff = 0;
  int currItemSave = mCurrentItem, curSelSave = mCurListSel;
  bool allOn = true;
  if (ProcessRangeKey("N again for new files", mShiftNIndex, start, end))
    return;

  // See if they are all already on, in which case they will be turned off
  for (ind = start; ind <= end; ind++) {
    item = mItemArray[ind];
    if (item->mAcquire)
      numAcq++;
    if (item->mFilePropIndex < 0) {
      allOn = false;
      numOff++;
    }
  }

  if (!numAcq)
    return;

  if (!allOn) {
    if (!KGetOneInt("Enter 1 for file at all acquire items, or -1 for file only at "
      "polygons", "Interval between items at which to set up new files:", 
      mMinNewFileInterval))
      return;
  }
  interval = mMinNewFileInterval < 0 ? numOff + 10 : mMinNewFileInterval;
  mHelper->SetDoingMultipleFiles(true);

  // Then loop and turn on ones not on, or turn off if all on
  numOff = 0;
  for (ind = start; ind <= end; ind++) {
    mCurrentItem = mCurListSel = ind;
    item = mItemArray[ind];
    if (item->mAcquire && item->mFilePropIndex < 0) {
      numOff++;
      if (numOff >= interval || item->IsPolygon()) {
        if (mHelper->NewAcquireFile(ind, NAVFILE_ITEM, NULL))
          break;
        UpdateListString(ind);
        numOff = 0;
      }
    } else if (allOn && item->mAcquire) {
      mHelper->EndAcquireOrNewFile(item);
      UpdateListString(ind);
    }
  }
  mCurrentItem = currItemSave;
  mCurListSel = curSelSave;
  ManageCurrentControls();
  Redraw();
  mHelper->SetDoingMultipleFiles(false);
}

// Do common actions for selecting a range of contiguous items with collapse off
int CNavigatorDlg::ProcessRangeKey(const char *key, int &shiftIndex, int &start, int &end)
{
  if (!SetCurrentItem(true))
    return 1;

  if (m_bCollapseGroups)
    return 1;
  if (shiftIndex < 0) {

    // Only allow one to be on, don't confuse the header line etc
    if (mShiftTIndex >= 0 || mShiftDIndex >= 0 || mShiftAIndex >= 0 || mShiftNIndex > 0)
      return 1;
    shiftIndex = mCurrentItem;
    ManageListHeader(CString("Select other end of range, press Shift ") + 
      key);
    return 1;
  }

  // Get ordered range
  start = B3DMIN(shiftIndex, mCurrentItem);
  end = B3DMAX(shiftIndex, mCurrentItem);
  ManageListHeader();
  shiftIndex = -1;
  return 0;
}

// Construct list box string for the given item
void CNavigatorDlg::ItemToListString(int index, CString &string)
{
  CDC *pDC = m_listViewer.GetDC();
  CString substr;
  CMapDrawItem *item2;
  ScheduledFile *sched = NULL;
  int j, start, end, numAcq = 0, numTS = 0;
  CMapDrawItem *item = mItemArray[index];
  int type = (item->mImported == 1 || item->mImported == -1) ? 
    ITEM_TYPE_MAP + 1 : item->mType;
  j = GroupScheduledIndex(item->mGroupID);
  if (j >= 0)
    sched = (ScheduledFile *)mGroupFiles.GetAt(j);

  if (m_bCollapseGroups && GetCollapsedGroupLimits(mItemToList[index], start, end)) {
    item = mItemArray[start];
    item2 = mItemArray[end];
    string.Format("   Group of %d items, ID ...%04d,  labels %s to %s", 
      end + 1 - start, item->mGroupID % 10000, item->mLabel, item2->mLabel);
    for (j = start; j <= end; j++) {
      item2 = mItemArray[j];
      if (item2->mAcquire)
        numAcq++;
      if (item2->mTSparamIndex >= 0)
        numTS++;
    }
    if (numTS)
      string += (numTS < end + 1 - start) ? "  Some TS" : "  All TS";
    if (numAcq) {
      string += (numAcq < end + 1 - start) ? "  Some Acq" : "  All Acq";
      if (sched) {
        string += "  G";
        if (sched->stateIndex >= 0)
          string += "S";
      }
    }

  } else {
    if (item->IsMap() && item->mMapID == mDualMapID) 
      type = ITEM_TYPE_MAP + 2;
    string = item->mLabel + "\t" + CString(item->mDraw ? colorNames[item->mColor] : "Off")
      + "\t" + FormatCoordinate(item->mStageX, 7) + FormatCoordinate(item->mStageY, 7) +
      FormatCoordinate(item->mStageZ, 6) + typeString[type];
    substr.Format("\t%d\t", item->mRegistration);
    if (item->mRegPoint > 0)
      substr.Format("\t%dR%d\t", item->mRegistration, item->mRegPoint);
    string += substr;
    if (item->mAcquire) {
      string += "A";
      if (item->mFilePropIndex >= 0)
        string += "F";
      if (sched) {
        string += "G";
        if (sched->stateIndex >= 0)
          string += "S";
      }
    } else if (item->mTSparamIndex >= 0)
      string += "TS";
    if (item->mStateIndex >= 0)
      string += "S";
    if (item->mFocusAxisPos > EXTRA_VALUE_TEST)
      string += "P";
    string += CString(item->mCorner ? "C\t" : "\t") + item->mNote;
  }
  item->mTextExtent = (pDC->GetTextExtent(string)).cx;
  m_listViewer.ReleaseDC(pDC);
}

// Get a new string for the given item and replace it in list box
void CNavigatorDlg::UpdateListString(int index)
{
  CString string;
  if (mMacroProcessor->DoingMacro() && mMacroProcessor->GetSuspendNavRedraw())
    return;
  ItemToListString(index, string);
  if (m_bCollapseGroups)
    index = mItemToList[index];
  m_listViewer.DeleteString(index);
  m_listViewer.InsertString(index, string);
  if (mCurListSel >= 0)
    m_listViewer.SetCurSel(mCurListSel);
  ManageListScroll();
}

// Update the strings for all items in the group
void CNavigatorDlg::UpdateGroupStrings(int groupID)
{
  CMapDrawItem *item;
  int updated = -1;
  for (int index = 0; index < mItemArray.GetSize(); index++) {
    item = mItemArray[index];
    if (item->mAcquire && item->mGroupID == groupID) {
      if (m_bCollapseGroups) {
        if (mItemToList[index] != updated)
          UpdateListString(index);
        updated = mItemToList[index];
      } else
        UpdateListString(index);
    }
  }
}

// Fill the list box with all of the items in the array
// Skip managing current controls by default, reset selection to 0
void CNavigatorDlg::FillListBox(bool skipManage, bool keepSel)
{
  CString string;
  int updated = -1;
  int i;
  m_listViewer.ResetContent();
  if (mItemArray.GetSize()) {
    for (i = 0; i < mItemArray.GetSize(); i++) {
      if (!m_bCollapseGroups || mItemToList[i] != updated) {
        ItemToListString(i, string);
        m_listViewer.AddString(string);
        if (m_bCollapseGroups)
          updated = mItemToList[i];
      }
    }
    if (!keepSel) {
      mCurrentItem = 0;
      mCurListSel = 0;
    }
    m_listViewer.SetCurSel(mCurListSel);
    ManageListScroll();
  } else {
    mCurrentItem = -1;
    mCurListSel = -1;
  }
  if (!skipManage)
    ManageCurrentControls();
}

// Determine maximum length of list strings and set extent for slider to work
void CNavigatorDlg::ManageListScroll()
{
  int i;
  int xmax = 0;
  CMapDrawItem *item;
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (xmax < item->mTextExtent)
      xmax = item->mTextExtent;
  }
  m_listViewer.SetHorizontalExtent(xmax);
}

// Pad a number so that it is less than the given maximum length by 2 spaces
// at the front for each missing digit - workaround to lack of right-tab
CString CNavigatorDlg::FormatCoordinate(float inVal, int maxLen)
{
  CString str;
  str.Format("%.1f\t", inVal);
  int nadd = maxLen + 1 - str.GetLength();
  for (int i = 0; i < nadd; i++)
    str.Insert(0, i ? "  " : " ");
  return str;
}

/////////////////////////////////////////////////////////////////////
// STAGE-RELATED AND ITEM DELETION MESSAGE HANDLERS

// Add a point at the current stage position
void CNavigatorDlg::OnAddStagePos() 
{
	CMapDrawItem *item = MakeNewItem(0);
  SetCurrentStagePos(mCurrentItem);
  item->AppendPoint(item->mStageX, item->mStageY);
  item->mRawStageX = (float)mLastScopeStageX;
  item->mRawStageY = (float)mLastScopeStageY;
  CheckRawStageMatches();
  mScope->GetValidXYbacklash(mLastScopeStageX, mLastScopeStageY, item->mBacklashX, 
    item->mBacklashY);
  UpdateListString(mCurrentItem);
  SetChanged(true);
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
  Redraw();
}

// Update the current point with the current stage position
/*void CNavigatorDlg::OnUpdateStagePos() 
{
  float origX, origY;
  mWinApp->RestoreViewFocus();
	if (!SetCurrentItem())
    return;
  if (mItem->IsMap())
    return;
  origX = mItem->mStageX;
  origY = mItem->mStageY;

  SetCurrentStagePos(mCurrentItem);
  ShiftItemPoints(mItem, mItem->mStageX - origX, mItem->mStageX - origY);
  SetChanged(true);
  UpdateListString(mCurrentItem);
  Redraw();
} */

// Common routine to shift component points of an item
void CNavigatorDlg::ShiftItemPoints(CMapDrawItem *item, float delX, float delY)
{
  for (int i = 0; i < item->mNumPoints; i++) {
    item->mPtX[i] += delX;
    item->mPtY[i] += delY;
  }
}

// Shift non-imported items at a registration
int CNavigatorDlg::ShiftItemsAtRegistration(float shiftX, float shiftY, int reg, 
  int saveOrRestore)
{
  int i, numShift = 0, cohortID;
  CMapDrawItem *item;
  if (saveOrRestore > 0) {
    mHelper->ClearSavedMapMarkerShifts();
    cohortID = MakeUniqueID();
  }
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mRegistration == reg && item->mImported != 1) {
      if (saveOrRestore > 0)
        mHelper->SaveMapMarkerShiftToLists(item, cohortID, 0., 0.);
      else if (saveOrRestore < 0)
        mHelper->RestoreMapMarkerShift(item);
      ShiftItemAndPoints(shiftX, shiftY, item, i, numShift);
    }
  }
  if (numShift)
    SetChanged(true);
  Update();
  Redraw();
  return numShift;
}

// Shift a subset of items matching the mag index and either the cohort ID or the fact
// that they are not shifts.  Select maps and points/polygons marked on them
int CNavigatorDlg::ShiftCohortOfItems(float shiftX, float shiftY, int reg, 
  int magInd, int cohortID, bool useAll, bool wasShifted, int saveOrRestore)
{
  int i, numShift = 0;
  CMapDrawItem *item;
  std::set<int> shiftedMapIDs;

  // If saving, both clear out the saved shifts and set all the parameters for undo
  if (saveOrRestore > 0) {
    mHelper->ClearSavedMapMarkerShifts();
    mMarkerShiftReg = reg;
    mMarkerShiftX = shiftX;
    mMarkerShiftY = shiftY;
    mMarkerShiftType = useAll ? 1 : 2;
    mMarkerShiftMagInd = magInd;
    mMarkerShiftCohortID = cohortID;
  }

  // Loop on items and find maps that qualify, add to set
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mRegistration == reg && item->mImported != 1 && item->mMapMagInd == magInd &&
      (useAll || (wasShifted && item->mShiftCohortID == cohortID) || (!wasShifted &&
      (!item->mShiftCohortID || item->mMarkerShiftX < EXTRA_VALUE_TEST)))) {
      if (saveOrRestore > 0)
        mHelper->SaveMapMarkerShiftToLists(item, cohortID, shiftX, shiftY);
      else if (saveOrRestore < 0)
        mHelper->RestoreMapMarkerShift(item);
      shiftedMapIDs.insert(item->mMapID);
      ShiftItemAndPoints(shiftX, shiftY, item, i, numShift);
    }
  }

  // Loop again and find non-map items with drawnOnID in the set
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mRegistration == reg && item->IsNotMap() && item->mDrawnOnMapID
      && shiftedMapIDs.count(item->mDrawnOnMapID)) {
      ShiftItemAndPoints(shiftX, shiftY, item, i, numShift);
    }
  }
  if (numShift)
    SetChanged(true);
  return numShift;
}

// Shift one item's position and points by given amount
void CNavigatorDlg::ShiftItemAndPoints(float shiftX, float shiftY, CMapDrawItem *item,
  int itemInd, int &numShift)
{
  item->mStageX += shiftX;
  item->mStageY += shiftY;
  ShiftItemPoints(item, shiftX, shiftY);
  UpdateListString(itemInd);
  numShift++;
}

// Shift all items at the current registration by distance from current item to marker
void CNavigatorDlg::ShiftToMarker(void)
{
  float delX, delY, ptX, ptY;
  float shiftX, shiftY;
  int registration;
  CMapDrawItem *map = NULL;
  CString mess;
  ScaleMat aMat;
  BaseMarkerShift *baseShift;
  BaseMarkerShift newBase;
  EMimageBuffer * imBuf = mWinApp->mActiveView->GetActiveImBuf();
  CShiftToMarkerDlg dlg;
  CArray<BaseMarkerShift, BaseMarkerShift> *shiftArray = mHelper->GetMarkerShiftArray();
  if (!SetCurrentItem())
    return;

  if (!BufferStageToImage(imBuf, aMat, delX, delY)) {
    AfxMessageBox("The currently active image does not have enough information\n"
      "to convert the marker point to a stage coordinate.", MB_EXCLAME);
    return;
  }

  registration = imBuf->mRegistration ? imBuf->mRegistration : mItem->mRegistration;
  if (registration != mItem->mRegistration) {
    AfxMessageBox("The currently active image is not at the same registration\n"
      "as the currently selected Navigator item.", MB_EXCLAME);
    return;
  }

  MarkerStagePosition(imBuf, aMat, delX, delY, ptX, ptY);
  shiftX = ptX - mItem->mStageX;
  shiftY = ptY - mItem->mStageY;

  dlg.mToMag = imBuf->mMagInd;
  dlg.mFromMag = 0;
  dlg.mBaseToMag = 0;
  dlg.mMapWasShifted = false;
  dlg.m_iApplyToWhich = mMarkerShiftApplyTo;
  dlg.m_iSaveType = mMarkerShiftSaveType;
  if (mItem->mDrawnOnMapID) {
    map = FindItemWithMapID(mItem->mDrawnOnMapID);
    if (map) {
      dlg.mFromMag = map->mMapMagInd;
      dlg.mMapWasShifted = map->mShiftCohortID && map->mMarkerShiftX > EXTRA_VALUE_TEST;
    }
  } else
    dlg.mFromMag = mItem->mMapMagInd;
  baseShift = mHelper->FindNearestBaseShift(dlg.mFromMag, dlg.mToMag);
  if (baseShift)
   dlg.mBaseToMag = baseShift->toMagInd;

  dlg.m_strMarkerShift.Format("The shift between the current item and the marker point"
    " is %.2f, %.2f microns", shiftX, shiftY);
  dlg.m_strWhatShifts.Format("All selected %sitems at "
    "registration %d will be shifted by that amount",  
    (RegistrationUseType(registration) == NAVREG_IMPORT ? "non-imported " : ""), 
    registration);
  if (dlg.DoModal() == IDCANCEL)
    return;
  mMarkerShiftApplyTo = dlg.m_iApplyToWhich;
  mMarkerShiftSaveType = dlg.m_iSaveType;
  if (!dlg.m_iApplyToWhich || !dlg.mFromMag || !dlg.mToMag) {
    ShiftItemsAtRegistration(shiftX, shiftY, registration, 1);
    mMarkerShiftType = 0;
  } else {
    mMarkerShiftCohortID = (map && map->mShiftCohortID) ? map->mShiftCohortID :
      MakeUniqueID();
    ShiftCohortOfItems(shiftX, shiftY, registration, dlg.mFromMag, mMarkerShiftCohortID,
      dlg.m_iApplyToWhich == 1, dlg.mMapWasShifted, 1);
    if (baseShift && dlg.m_iSaveType == 2) {
      baseShift->shiftX += shiftX;
      baseShift->shiftY += shiftY;
    } else if (dlg.m_iSaveType == 1 || (!baseShift && dlg.m_iSaveType > 0)) {
      if (baseShift && dlg.mBaseToMag == dlg.mToMag) {
        baseShift->shiftX = shiftX;
        baseShift->shiftY = shiftY;
      } else {
        newBase.fromMagInd = dlg.mFromMag;
        newBase.toMagInd = dlg.mToMag;
        newBase.shiftX = shiftX;
        newBase.shiftY = shiftY;
        shiftArray->Add(newBase);
      }
    }
  }
  mMarkerShiftReg = registration;
  mMarkerShiftX = shiftX;
  mMarkerShiftY = shiftY;
}

// Undo last shift to marker and zero out the shift registration
void CNavigatorDlg::UndoShiftToMarker(void)
{
  if (!mMarkerShiftType) {
    ShiftItemsAtRegistration(-mMarkerShiftX, -mMarkerShiftY, mMarkerShiftReg, -1);
  } else {
    ShiftCohortOfItems(-mMarkerShiftX, -mMarkerShiftY, mMarkerShiftReg, 
      mMarkerShiftMagInd, mMarkerShiftCohortID, false, true, -1);
  }
  mMarkerShiftReg = 0;
}

// Shift items by the alignment of the image in A
int CNavigatorDlg::ShiftItemsByAlign(void)
{
  float delX, delY, ptX, ptY;
  float shiftX, shiftY;
  ScaleMat aMat, aInv;
  if (!BufferStageToImage(mImBufs, aMat, delX, delY)) {
    AfxMessageBox("The image in buffer A does not have enough information\n"
      "to convert its alignment shift to a stage shift.", MB_EXCLAME);
    return 1;
  }
  if (mImBufs->mRegistration != mCurrentRegistration) {
    AfxMessageBox("The image in buffer A is not at the current registration\n"
      , MB_EXCLAME);
    return 1;
  }
  mImBufs->mImage->getShifts(delX, delY);
  mImBufs->mImage->getShifts(ptX, ptY);

  // If this image has been aligned with already, shift to the new alignment
  if (mShiftByAlignStamp == mImBufs->mTimeStamp) {
    ptX -= mShiftByAlignX;
    ptY -= mShiftByAlignY;
  }
  aInv = MatInv(aMat);
  shiftX = aInv.xpx * ptX + aInv.xpy * ptY;
  shiftY = aInv.ypx * ptX + aInv.ypy * ptY;
  SEMTrace('1', "Shifting items at registration %d by %.3f, %.3f", mCurrentRegistration,
    -shiftX, -shiftY);
  ShiftItemsAtRegistration(-shiftX, -shiftY, mCurrentRegistration);

  // Record time stamp and shift applied
  mShiftByAlignStamp = mImBufs->mTimeStamp;
  mShiftByAlignX = delX;
  mShiftByAlignY = delY;
  return 0;
}

// Update a point with the current Z value in response to button
void CNavigatorDlg::OnButUpdatez()
{
  mWinApp->RestoreViewFocus();
  if (SetCurrentItem(true))
    DoUpdateZ(mCurrentItem, -1);
}

// Update Z: can be called from macro.  ifGroup = 0 for single item, -1 for whatever
// is currently displayed (group or item), 1 for whole group only
int CNavigatorDlg::DoUpdateZ(int index, int ifGroup)
{
  double stageZ;
  int start, end, i, groupID, num;
  CMapDrawItem *item;
  CString mess, label, lastlab;

  if (mAddingPoints || mAddingPoly )
    return 1;
  if (!mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ))
    return 2;
  start = end = index;
  item = mItemArray[index];
  groupID = item->mGroupID;
  if (ifGroup > 0) {
    if (!groupID)
      return 3;
    start = 0;
    end = (int)mItemArray.GetSize() - 1;
    num = CountItemsInGroup(groupID, label, lastlab, i);
    mess.Format("Changing Z value of %d items (labels %s - %s) to %.2f", num, 
      label, lastlab, stageZ);
    mWinApp->AppendToLog(mess);
  } else if (ifGroup < 0 && m_bCollapseGroups && mListToItem[mCurListSel] < 0) {
    GetCollapsedGroupLimits(mCurListSel, start, end);
    mess.Format("Changing Z value of %d items (# %d - %d) to %.2f", end + 1 - start, 
      start, end, stageZ);
    mWinApp->AppendToLog(mess);
  }
  for (i = start; i <= end; i++) {
    item = mItemArray[i];
    if (!groupID || item->mGroupID == groupID) { 
      item->mStageZ = (float)stageZ;
      if (i == end || ifGroup > 0)
        UpdateListString(i);
    }
  }
  return 0;
}

// Add a point at the current marker position
void CNavigatorDlg::OnAddMarker()
{
  float delX, delY,stageX, stageY, xInPiece, yInPiece;
  int pcInd;
  ScaleMat aMat;
 	EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  mWinApp->RestoreViewFocus();
  if (!OKtoAddMarkerPoint())
    return;
  BufferStageToImage(imBuf, aMat, delX, delY);
  MarkerStagePosition(imBuf, aMat, delX, delY, stageX, stageY, 0, &pcInd, &xInPiece,
    &yInPiece);
  CMapDrawItem *item = AddPointMarkedOnBuffer(imBuf, stageX, stageY, MakeUniqueID());
  item->mPieceDrawnOn = pcInd;
  item->mXinPiece = xInPiece;
  item->mYinPiece = yInPiece;
  UpdateListString(mCurrentItem);
  SetChanged(true);
  mWinApp->RestoreViewFocus();
  ManageCurrentControls();
  Redraw();
}

// Function for whether it is OK to add or marker point, or go to one, for that matter
bool CNavigatorDlg::OKtoAddMarkerPoint(void)
{
  float delX, delY;
  ScaleMat aMat;
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (!imBuf->mHasUserPt || !BufferStageToImage(imBuf, aMat, delX, delY) ||
    RegistrationUseType(imBuf->mRegistration) == NAVREG_IMPORT || mWinApp->DoingTasks() ||
    mAcquireIndex >= 0 || mAddingPoly || mAddingPoints || mMovingItem || mLoadingMap)
    return false;
  return true;
}

// Go to point in X,Y,Z
void CNavigatorDlg::OnGotoPoint() 
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem())
    return;
  MoveStage(axisXY | axisZ);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
}

// Go to point in X,Y only
void CNavigatorDlg::OnGotoXy() 
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem())
    return;
  MoveStageOrDoImageShift(axisXY);
}

// Go to the position of the user point in the active window
void CNavigatorDlg::OnGotoMarker() 
{
  float delX, delY;
  CMapDrawItem *item;
  ScaleMat aMat;
 	EMimageBuffer * imBuf = mWinApp->mActiveView->GetActiveImBuf();
  mWinApp->RestoreViewFocus();
  if (!imBuf->mHasUserPt) {
    AfxMessageBox("There is no marker point in the currently active image.\n\n"
      "Click with the left mouse button to place a temporary marker point.",
      MB_EXCLAME);
    return;
  }
  if (!BufferStageToImage(imBuf, aMat, delX, delY)) {
    AfxMessageBox("The currently active image does not have enough information\n"
      "to convert the marker point to a stage coordinate.", MB_EXCLAME);
    return;
  }
  if (RegistrationUseType(imBuf->mRegistration) == NAVREG_IMPORT) {
    AfxMessageBox("You cannot move to a position on an imported image until it has been"
      "\ntransformed into registration with regular images", MB_EXCLAME);
    return;
  }
  item = mItem = new CMapDrawItem;
  MarkerStagePosition(imBuf, aMat, delX, delY, mItem->mStageX, mItem->mStageY);
  mItem->mDrawnOnMapID = imBuf->mMapID;
  if (!imBuf->mMapID) {
    mItem->mBacklashX = imBuf->mBacklashX;
    mItem->mBacklashY = imBuf->mBacklashY;
  }
  MoveStageOrDoImageShift(axisXY);
  delete item;
}

// Return the image marker as a stage position
void CNavigatorDlg::MarkerStagePosition(EMimageBuffer * imBuf, ScaleMat aMat, float delX, 
  float delY, float & stageX, float & stageY, int useLineEnd, int *pcInd, 
  float *xInPiece, float *yInPiece)
{
  float ptX, ptY;
  ScaleMat aInv;

  // Use the end of the line if the value is positive, or keep it in screen middle if < 0
  ptX = useLineEnd > 0 ? imBuf->mLineEndX : imBuf->mUserPtX;
  ptY = useLineEnd > 0 ? imBuf->mLineEndY : imBuf->mUserPtY;
  if (useLineEnd < 0 && imBuf->mImage) {
    ptX = (float)(imBuf->mImage->getWidth() / 2.);
    ptY = (float)(imBuf->mImage->getHeight() / 2.);
  }
  AdjustMontImagePos(imBuf, ptX, ptY, pcInd, xInPiece, yInPiece);
  aInv = MatInv(aMat);
  stageX = aInv.xpx * (ptX - delX) + aInv.xpy * (ptY - delY);
  stageY = aInv.ypx * (ptX - delX) + aInv.ypy * (ptY - delY);
}

// Overload that handles getting transform too
BOOL CNavigatorDlg::MarkerStagePosition(EMimageBuffer *imBuf, float &stageX, 
  float &stageY)
{
  float delX, delY;
  ScaleMat aMat;
  if (!BufferStageToImage(imBuf, aMat, delX, delY))
    return false;
  MarkerStagePosition(imBuf, aMat, delX, delY, stageX, stageY);
  return true;
}

// Common routine to move stage to current item, with wait until it is ready
int CNavigatorDlg::MoveStage(int axisBits, bool justCheck)
{
  float backX, backY, montErrX, montErrY;

  // Check for outside limits
  if (mItem->mStageX < mScope->GetStageLimit(STAGE_MIN_X) ||
    mItem->mStageX > mScope->GetStageLimit(STAGE_MAX_X) ||
    mItem->mStageY < mScope->GetStageLimit(STAGE_MIN_Y) ||
    mItem->mStageY > mScope->GetStageLimit(STAGE_MAX_Y)) {
    if (mAcquireIndex < 0)
      SEMMessageBox("This position is outside the limits for stage movement",
          MB_EXCLAME);
    return -1;
  }
  if (!axisBits || justCheck)
    return 0;

  // Make sure it is ready
  if (mScope->WaitForStageReady(5000)) {
    SEMMessageBox("The stage is busy or not ready; move aborted");
    return 1;
  }

  // Now do the stage move and reset image shift
  MontErrForItem(mItem, montErrX, montErrY);
  BacklashForItem(mItem, backX, backY);
  mLastMoveStageID = mItem->mMoveStageID = GetTickCount() / 4;
  AdjustAndMoveStage(mItem->mStageX + montErrX, mItem->mStageY + montErrY, mItem->mStageZ,
    axisBits, backX, backY);
  return 0;
}

// Reset image shift or set it to value in leaveISX,Y and move stage to adjusted 
// coordinates for current settings
void CNavigatorDlg::AdjustAndMoveStage(float stageX, float stageY, float stageZ, 
                                       int axisBits, float backX, float backY,
                                       double leaveISX, double leaveISY,
                                       float tiltAngle)
{
  StageMoveInfo smi;
  double shiftX, shiftY, curStageX, curStageY, curStageZ, moveX, moveY;
  double viewX, viewY, searchX, searchY;
  float validX, validY, stageDx = 0., stageDy = 0.;
  bool validBack;
  BOOL doBacklash = mParam->stageBacklash != 0.;
  int magInd = mScope->GetMagIndex();
  ScaleMat aMat = mShiftManager->IStoGivenCamera(magInd, mWinApp->GetCurrentCamera());

  // This compensates for the IS being imposed
  mScope->GetLDCenteredShift(shiftX, shiftY);
  
  // So this gets it back to a 0 IS readout as long as there is an IS calibration
  if (aMat.xpx)
    mScope->IncImageShift(leaveISX - shiftX, leaveISY - shiftY);

  // In low dose mode, if balance shifts is on, need to leave that IS also
  if (mWinApp->LowDoseMode()) {
    LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    int area = mScope->GetLowDoseArea();
    if (mLowDoseDlg->ShiftsBalanced()) {
      leaveISX += ldp->ISX;
      leaveISY += ldp->ISY;
    }

    // And unless the compensation has been done (painfully) in realign or there are no
    // view offsets set in View or Search, set up to compensate to the Record area in the
    // next call
    mLowDoseDlg->GetFullViewShift(viewX, viewY, VIEW_CONSET);
    mLowDoseDlg->GetFullViewShift(searchX, searchY, SEARCH_AREA);
    if (!mHelper->GetRealigning() && ldp->magIndex && !(area == VIEW_CONSET && 
      !viewX && !viewY) && !(area == SEARCH_AREA && !searchX && !searchY))
        magInd = ldp->magIndex;
  }

  // If scope not applying mag offsets, this adjusts actual stage position to compensate
  ConvertIStoStageIncrement(magInd, mWinApp->GetCurrentCamera(), leaveISX, leaveISY, 
    (float)mScope->FastTiltAngle(), stageDx, stageDy);
  smi.x = stageX - stageDx;
  smi.y = stageY - stageDy;
  smi.z = stageZ;
  SEMTrace('n', "Nominal pos %.2f %.2f  leave IS %.2f %.2f  LDcenIS %.2f %.2f\r\ndelta"
    " %.2f  %.2f  final %.2f %.2f", stageX, stageY, leaveISX, leaveISY, shiftX, shiftY, 
    stageDx, stageDy, smi.x, smi.y);
  smi.axisBits = axisBits;
  if (mWinApp->mComplexTasks->GetHitachiWithoutZ())
    smi.axisBits &= ~(axisZ);
  smi.backX = -mParam->stageBacklash;
  smi.backY = -mParam->stageBacklash;
  if (backX > -900. && backY > -900.) {
    doBacklash = backX != 0. || backY != 0.;
    smi.backX = backX;
    smi.backY = backY;
  }

  // Cancel backlash if move is in right direction and either it is big enough or there
  // is already valid backlash in that direction.
  if (doBacklash) {
    mScope->GetStagePosition(curStageX, curStageY, curStageZ);
    validBack = mScope->GetValidXYbacklash(curStageX, curStageY, validX, validY);
    moveX = smi.x - curStageX;
    moveY = smi.y - curStageY;
    SEMTrace('n', "Backlash for item %.2f %.2f  move %.2f %.2f  %svalid is %.2f %.2f",
      smi.backX, smi.backY, moveX, moveY, validBack ? "" : "IN", validX, validY);
    if ((moveX > 0 ? 1 : 0) == (smi.backX < 0 ? 1 : 0) &&
      (moveY > 0 ? 1 : 0) == (smi.backY < 0 ? 1 : 0)) {
        if (fabs(moveX) >= fabs(backX) && fabs(moveY) >= fabs(backY)) {
          SEMTrace('n', "Cancel because direction is right and move is big enough");
          doBacklash = false;
          mScope->SetBacklashFromNextMove(curStageX, curStageY, 
            (float)B3DMAX(B3DABS(smi.backX), B3DABS(smi.backY)));
        } else if (validBack && validX * smi.backX > 0 && validY * smi.backY > 0) {
          doBacklash = false;
          SEMTrace('n', "Cancel because direction is right and backlash already set");
          mScope->CopyBacklashValid();
        }
    }
  }
  smi.backZ = 0.;
  if (axisBits & axisA) {
    smi.alpha = B3DCHOICE(tiltAngle > -1000., tiltAngle, 0.);
    smi.backAlpha = 0.;
  }
  mRequestedStageX = (float)smi.x;
  mRequestedStageY = (float)smi.y;
  mScope->MoveStage(smi, doBacklash);
}

// Either call the usual MoveStage routine, or do an image shift if recording holes in
// mulishot dialog
void CNavigatorDlg::MoveStageOrDoImageShift(int axisBits)
{
  float stageX, stageY, stageZ, delX, delY, delISX, delISY;
  double areaX = 0, areaY = 0;
  ScaleMat stage2IS;
  int magInd, area;
  if (mHelper->mMultiShotDlg && mHelper->mMultiShotDlg->RecordingHoles()) {
    GetAdjustedStagePos(stageX, stageY, stageZ);
    if (mWinApp->LowDoseMode()) {
      area = mScope->GetLowDoseArea();
      if (area == VIEW_CONSET || area == SEARCH_AREA)
        mLowDoseDlg->GetNetViewShift(areaX, areaY, area);
    }
    magInd = mScope->GetMagIndex();
    stage2IS = MatMul(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), magInd),
      mShiftManager->CameraToIS(magInd));
    if (stage2IS.xpx) {
      delX = mItem->mStageX - stageX;
      delY = mItem->mStageY - stageY;
      delISX = stage2IS.xpx * delX + stage2IS.xpy * delY;
      delISY = stage2IS.ypx * delX + stage2IS.ypy * delY;
      mScope->IncImageShift(delISX + areaX, delISY + areaY);
    }
  } else {
    MoveStage(axisXY);  
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  }
}

// Delete an item or portion of collapsed group on one line
void CNavigatorDlg::OnDeleteitem() 
{
  int start, end, delIndex = mCurrentItem;
  bool multipleInGroup = false;
  mWinApp->RestoreViewFocus();

  // This sets current item to actual item or beginning of group
  if (!SetCurrentItem(true))
    return;

  // If coming in through backspace in edit mode instead of when adding points, set the
  // actual current item and make sure (again) that it is a point
  if (m_bEditMode && mRemoveItemOnly && !mAddingPoints) {
    mItem = GetSingleSelectedItem(&delIndex);
    if (!mItem || mItem->IsNotPoint())
      return;
    if (m_bCollapseGroups) {
      GetCollapsedGroupLimits(mCurListSel, start, end);
      multipleInGroup = end > start;
    }
  }

  // Non-backspace case: Go to group deletion if groups collapsed or multiple
  if (!mRemoveItemOnly && ((m_bCollapseGroups && mListToItem[mCurListSel] < 0) || 
    mSelectedItems.size() > 1)) {
    DeleteGroup(true);
    return;
  }
  if (mItem->IsMap() && !mItem->mImported && 
    AfxMessageBox("This item is a map image\n\n"
    "It includes critical information about the file\n and coordinate scaling that "
    "would be very hard to recreate.\n\n Are you sure you want to delete the item?",
    MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;

  if (mItem->mAcquire || mItem->mTSparamIndex >= 0) {
    mItem->mAcquire = false;
    mHelper->EndAcquireOrNewFile(mItem);
  }
  if (mItem->IsMap() && mItem->mMapID == mDualMapID)
    mDualMapID = -1;

  // If removing a point while adding and groups are collapsed, set item to last entry in
  // array if it is not the only one; otherwise delete the string unless there will
  // still be items in it for edit-mode deletion
  if (mRemoveItemOnly && mAddingPoints && m_bCollapseGroups && mItemArray.GetSize() > 
    mNumberBeforeAdd + 1) {
    delIndex = (int)mItemArray.GetSize() - 1;
    mItem = mItemArray[delIndex];
  } else if (!multipleInGroup) {
    m_listViewer.DeleteString(mCurListSel);
  }
  delete mItem;
  mItemArray.RemoveAt(delIndex);
  if (mCurListSel >= m_listViewer.GetCount())
    mCurListSel--;
  m_listViewer.SetCurSel(mCurListSel);
  MakeListMappings();

  // Set current item to beginning of group
  IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
  if (multipleInGroup)
    UpdateListString(mCurrentItem);
  SetChanged(true);
  mSelectedItems.clear();
  delIndex = B3DMAX(0, delIndex - 1);
  if (m_bCollapseGroups && m_bEditMode && mRemoveItemOnly && !mAddingPoints && 
    mCurrentItem >= 0 && mItemToList[delIndex] == mItemToList[mCurrentItem])
    mSelectedItems.insert(delIndex);

  // But when adding points, set selection to last point
  if (mRemoveItemOnly && mAddingPoints && mItemArray.GetSize() > mNumberBeforeAdd)
    mSelectedItems.insert((int)mItemArray.GetSize() - 1);
  ManageCurrentControls();
  Redraw();
}

//////////////////////////////////////////////////////////////////////
// DRAWING POINTS AND POLYGONS OR MOVING ITEMS WITH MOUSE

// Initiate point drawing
void CNavigatorDlg::OnDrawPoints() 
{
  m_butDrawPts.SetWindowText(mAddingPoints ? "Add Points" : "Stop Adding");
  mAddingPoints = !mAddingPoints;
  mNumberBeforeAdd = (int)mItemArray.GetSize();
  mAddPointID = MakeUniqueID();
  if (mAddingPoints) {
    ManageListHeader("Use Backspace to remove added points one by one");
  } else
    ManageListHeader();

  Update();
  mWinApp->RestoreViewFocus();
}

// Initiate polygon drawing
void CNavigatorDlg::OnDrawPolygon() 
{
  float xsum = 0., ysum = 0.;
  bool addedPoints = mAddingPoly > 1;

  m_butDrawPoly.SetWindowText(mAddingPoly ? "Add Polygon" : "Stop Adding");
  if (mAddingPoly) {

    // If finishing with a polygon, turn off flag, delete item if empty
    mAddingPoly = 0;
    ManageListHeader();
    if (SetCurrentItem()) {
      if (!mItem->mNumPoints) {
        OnDeleteitem();
      } else if (addedPoints) {

        // Otherwise fix the center coordinate to be the mean of the corners
        for (int i = 0; i < mItem->mNumPoints; i++) {
          xsum += mItem->mPtX[i];
          ysum += mItem->mPtY[i];
        }
        mItem->mStageX = xsum / mItem->mNumPoints;
        mItem->mStageY = ysum / mItem->mNumPoints;

        // If one point, convert to a point type; if > 2, add point for closure
        if (mItem->mNumPoints == 1)
          mItem->mType = ITEM_TYPE_POINT;
        else if (mItem->mNumPoints > 2)
          mItem->AppendPoint(mItem->mPtX[0], mItem->mPtY[0]);

        UpdateListString(mCurrentItem);
      }
      ManageCurrentControls();
      Redraw();
    }
  } else {

    // Starting poly, just set the flag
    mAddingPoly = 1;
    ManageListHeader("Use Backspace to remove added points one by one");
  }
  Update();
  mWinApp->RestoreViewFocus();
}

// Initiate moving an item; check if it a map and if it possible to revise raw stage pos
void CNavigatorDlg::OnMoveItem() 
{
  CString mess;
  if (!mMovingItem) {
    if (!SetCurrentItem())
      return;
    if (mItem->IsMap()) {
      mess = "This item is a map.  Are you sure you want to change its position?";
      if (RawStageIsRevisable(false))
        mess += "\n\nIf you move to the exact center of the current image, the map will\n"
        "be assigned a new raw stage position at the current stage position";
      if (AfxMessageBox(mess, MB_QUESTION) == IDNO)
        return;
    }
    mRawStageIsMovable = false;
  } else if (mRawStageIsMovable && SetCurrentItem()) {
    mItem->mRawStageX = mMovedRawStageX;
    mItem->mRawStageY = mMovedRawStageY;
    mess.Format("Raw stage position of map was revised to %.2f, %.2f", mMovedRawStageX, 
      mMovedRawStageY);
    mWinApp->AppendToLog(mess);
  }
      
  m_butMoveItem.SetWindowText(mMovingItem ? "Move Item" : "Stop Moving");
  mMovingItem = !mMovingItem;
  Update();
  mWinApp->RestoreViewFocus();
}

// Determine whether the raw stage of a map can be changed: stage is at same position as 
// current image and item already has raw stage position
bool CNavigatorDlg::RawStageIsRevisable(bool fastStage)
{
  float bufX, bufY, tol;
  double stageX, stageY, stageZ;
  if (mItem->mRawStageX > RAW_STAGE_TEST && mWinApp->mActiveView) {
    EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
    if (imBuf->GetStagePosition(bufX, bufY)) {
      if (fastStage)
        mScope->FastStagePosition(stageX, stageY, stageZ);
      else {
        mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ);
        stageX = mLastScopeStageX;
        stageY = mLastScopeStageY;
      }
      tol = mScope->GetBacklashTolerance();
      if (fabs(stageX - bufX) < tol && fabs(stageY - bufY) < tol) {
        mMovedRawStageX = (float)stageX;
        mMovedRawStageY = (float)stageY;
        return true;
      }
    }
  }
  return false;
}

// Function to inform drawing routine that the cross should be drawn
bool CNavigatorDlg::MovingMapItem(void)
{
  if (!mMovingItem || !SetCurrentItem())
    return false;
  return mItem->IsMap() && RawStageIsRevisable(true);
}

// Respond to a mouse point from the user
BOOL CNavigatorDlg::UserMousePoint(EMimageBuffer *imBuf, float inX, float inY, 
                                   BOOL nearCenter, int button)
{
  ScaleMat aMat, aInv;
  float delX, delY, stageX, stageY,xInPiece, yInPiece, dist, distMin = 1.e10;
  int ind, indMin, nxBuf, nyBuf, pieceIndex;
  int groupID = 0;
  std::set<int>::iterator iter;
  float selXLimitFrac[4] = {-0.03f, 1.03f, 1.03f, -0.03f};
  float selYLimitFrac[4] = {-0.03f, -0.03f, 1.03f, 1.03f};
  float selXlimit[4], selYlimit[4], selXwindow[4], selYwindow[4];
  CMapDrawItem *item;
  BOOL acquire = false;
  bool ctrlKey = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  bool shiftKey = GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  bool selecting = button == VK_LBUTTON && (!shiftKey || m_bEditMode) && 
    (m_bEditMode || ctrlKey) && !mAddingPoints && !mAddingPoly;
  bool startingMultiDel = button == VK_LBUTTON && shiftKey && m_bEditMode &&
    !mAddingPoints && !mAddingPoly && mCurItemHoleXYpos.size();
  bool addingOne = m_bEditMode && button == VK_MBUTTON && !mAddingPoints && !mAddingPoly;
  bool movingOne = (m_bEditMode || mAddingPoints) && button == VK_RBUTTON && !mAddingPoly;

  mLastSelectWasCurrent = false;
  if (!(mAddingPoints || mAddingPoly || mMovingItem || selecting || movingOne || 
    addingOne || startingMultiDel))
    return false;
  if (mMovingItem && button == VK_MBUTTON)
    return false;
  if (!BufferStageToImage(imBuf, aMat, delX, delY))
    return false;
  AdjustMontImagePos(imBuf, inX, inY, &pieceIndex, &xInPiece, &yInPiece);
  aInv = MatInv(aMat);
  stageX = aInv.xpx * (inX - delX) + aInv.xpy * (inY - delY);
  stageY = aInv.ypx * (inX - delX) + aInv.ypy * (inY - delY);
  if (startingMultiDel) {
    mMultiDelStageX = stageX;
    mMultiDelStageY = stageY;
    mLastSelectWasCurrent = true;
    return true;
  }

  // Moving the current item
  if (mMovingItem || movingOne) {
    if (movingOne && m_bCollapseGroups) {
      item = GetSingleSelectedItem();
      if (!item)
        return false;
      mItem = item;
    } else if (!SetCurrentItem())
      return false;
    if (mItem->mRegistration != imBuf->mRegistration)
      return false;
    if (movingOne && mItem->IsNotPoint())
      return false;
    ShiftItemPoints(mItem, stageX - mItem->mStageX, stageY - mItem->mStageY);
    mItem->mStageX = stageX;
    mItem->mStageY = stageY;

    // If this is actaully a different map from the one drawn on, adjust the ID and Z
    if (mItem->mDrawnOnMapID && imBuf->mMapID && mItem->mDrawnOnMapID != imBuf->mMapID) {
      item = FindItemWithMapID(imBuf->mMapID);
      if (item) {
        mItem->mDrawnOnMapID = imBuf->mMapID;
        mItem->mStageZ = item->mStageZ;
      }
    }
    if (mItem->IsPoint()) {
      mItem->mPieceDrawnOn = pieceIndex;
      mItem->mXinPiece = xInPiece;
      mItem->mYinPiece = yInPiece;
    }
    UpdateListString(mCurrentItem);
    SetChanged(true);
    mRawStageIsMovable = mItem->IsMap() && nearCenter && 
      RawStageIsRevisable(false);
  
    // Adding points or the first point of a polygon
  } else if ((mAddingPoints || mAddingPoly == 1 || addingOne) && button != VK_RBUTTON) {

    /*// If the buffer is a map and its registration doesn't match, don't draw on it
    // Otherwise set buffer registration to current reg
    if (mCurrentRegistration != imBuf->mRegistration && imBuf->mMapID)
      return false;
    imBuf->mRegistration = mCurrentRegistration; */
    if (addingOne) {
      if (!SetCurrentItem(true) || mItem->IsNotPoint() || !mItem->mGroupID)
        return false;
      acquire = mItem->mAcquire;
      groupID = mItem->mGroupID;
    } else if (SetCurrentItem() && mItemArray.GetSize() > mNumberBeforeAdd && 
      mAddingPoints)
      acquire = mItem->mAcquire;
    item = AddPointMarkedOnBuffer(imBuf, stageX, stageY, 
      mAddingPoints ? mAddPointID : groupID);
    item->mAcquire = acquire;
    if (mAddingPoly) {
      mAddingPoly++;
      item->mType = ITEM_TYPE_POLYGON;
      item->mColor = DEFAULT_POLY_COLOR;
    } else {
      item->mPieceDrawnOn = pieceIndex;
      item->mXinPiece = xInPiece;
      item->mYinPiece = yInPiece;
    }
    UpdateListString(mCurrentItem);
    SetChanged(true);
    ManageCurrentControls();

    // Adding successive points of a polygon
  } else if (mAddingPoly) {
    if (!SetCurrentItem())
      return false;
    if (button != VK_RBUTTON) {
      mItem->AppendPoint(stageX, stageY);
    } else {
      mItem->mPtX[mItem->mNumPoints - 1] = stageX;
      mItem->mPtY[mItem->mNumPoints - 1] = stageY;
    }

    // Selecting nearest point
  } else if (selecting) {
    if (!imBuf->mImage)
      return false;

    // Get a region around the image and around the window
    imBuf->mImage->getSize(nxBuf, nyBuf);
    mWinApp->mMainView->WindowCornersInImageCoords(imBuf, &selXwindow[0], &selYwindow[0]);
    for (ind = 0; ind < 4; ind++) {
      selXlimit[ind] = aInv.xpx * (selXLimitFrac[ind] * nxBuf - delX) + 
        aInv.xpy * (selYLimitFrac[ind] * nyBuf - delY);
      selYlimit[ind] = aInv.ypx * (selXLimitFrac[ind] * nxBuf - delX) + 
        aInv.ypy * (selYLimitFrac[ind] * nyBuf - delY);
      dist = aInv.xpx * (selXwindow[ind] - delX) + aInv.xpy * (selYwindow[ind] - delY);
      selYwindow[ind] = aInv.ypx * (selXwindow[ind] - delX) + aInv.ypy * 
        (selYwindow[ind] - delY);
      selXwindow[ind] = dist;
    }

    // If there is one item selected and it is a map, clear out selection list
    if (m_bEditMode && ctrlKey && mSelectedItems.size() == 1 && SetCurrentItem() &&
      mItem->IsMap())
        mSelectedItems.clear();

    // Loop on items; skip ones that should not be visible or are outside region
    for (ind = 0; ind < mItemArray.GetSize(); ind++) {
      item = mItemArray.GetAt(ind);
      if (item->mRegistration != imBuf->mRegistration && !m_bDrawAllReg)
        continue;
      if (!item->mDraw || item->IsMap())
        continue;
      if (!(InsideContour(selXlimit, selYlimit, 4, item->mStageX, item->mStageY) ||
        InsideContour(selXwindow, selYwindow, 4, item->mStageX, item->mStageY)))
        continue;

      // Find nearest item
      delX = item->mStageX - stageX;
      delY = item->mStageY - stageY;
      dist = delX * delX + delY * delY;
      if (dist < distMin) {
        distMin = dist;
        indMin = ind;
      }
    }
    if (distMin > 1.e9)
      return false;

    // If doing multiple selection and this item is already in selection, remove it and
    // switch current item to previous or next one
    if (m_bEditMode && ctrlKey && mSelectedItems.size() > 1 && 
      mSelectedItems.count(indMin)) {
        iter = mSelectedItems.find(indMin);
        if (iter == mSelectedItems.begin())
          iter++;
        else
          iter--;
        ind = *iter;
        mSelectedItems.erase(indMin);
        indMin = ind;
    }

    // In edit mode, if there is a single point selected and it is clicked on,
    // set flag and return: this occurs before double click comes through
    if (m_bEditMode && !ctrlKey && mSelectedItems.size() == 1 && 
      mSelectedItems.count(indMin)) {
        mLastSelectWasCurrent = true;
        return true;
    }

    // Switch to item
    mCurListSel = mCurrentItem = indMin;
    if (m_bCollapseGroups)
      mCurListSel = mItemToList[indMin];
    m_listViewer.SetCurSel(mCurListSel);
    if (!(m_bEditMode && ctrlKey))
      mSelectedItems.clear();
    mSelectedItems.insert(indMin);
    ManageCurrentControls();
    Redraw();
  }
  return true;
}

// Double click on the currently selected point deletes it
void CNavigatorDlg::MouseDoubleClick(int button)
{
  int msNumXholes, msNumYholes, ind, jnd, minInd;
  int numHoles = (int)B3DMIN(mCurItemHoleIndex.size(), mCurItemHoleXYpos.size()) / 2;
  double dist, minDist = 1.e20, minInterHole = 1.e20;
  bool shiftKey = GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  unsigned char *newSkipPos;
  if (mLastSelectWasCurrent && button == VK_LBUTTON && m_bEditMode) {
    if (!shiftKey) {
      BackspacePressed();
    } else if (mCurItemHoleXYpos.size() && (mItem = GetSingleSelectedItem()) != NULL &&
      mHelper->GetNumHolesFromParam(msNumXholes, msNumYholes, ind)) {


      // Get interhole distance and distance to nearest
      for (ind = 0; ind < numHoles; ind++) {
        dist = sqrt(pow(mMultiDelStageX - mCurItemHoleXYpos[2 * ind], 2.f) + 
          pow(mMultiDelStageY - mCurItemHoleXYpos[2 * ind + 1], 2.f));
        if (dist < minDist) {
          minDist = dist;
          minInd = ind;
        }
        for (jnd = ind + 1; jnd < numHoles; jnd++) {
          dist = sqrt(pow(mCurItemHoleXYpos[2 * jnd] - mCurItemHoleXYpos[2 * ind], 2.f) +
            pow(mCurItemHoleXYpos[2 * jnd + 1] - mCurItemHoleXYpos[2 * ind + 1], 2.f));
          ACCUM_MIN(minInterHole, dist);
        }
      }

      // It must be close enough to one hole
      if (minDist < 0.33 * minInterHole) {
        if (!mItem->mNumXholes && !mItem->mNumYholes) {
          mItem->mNumXholes = msNumXholes;
          mItem->mNumYholes = msNumYholes;
        }
        newSkipPos = new unsigned char[2 * mItem->mNumSkipHoles + 2];
        for (jnd = 0; jnd < 2 * mItem->mNumSkipHoles; jnd++)
          newSkipPos[jnd] = mItem->mSkipHolePos[jnd];
        newSkipPos[jnd] = mCurItemHoleIndex[minInd * 2];
        newSkipPos[jnd + 1] = mCurItemHoleIndex[minInd * 2 + 1];
        delete[] mItem->mSkipHolePos;
        mItem->mSkipHolePos = newSkipPos;
        mItem->mNumSkipHoles += 1;
        if (mWinApp->mParticleTasks->ItemIsEmptyMultishot(mItem)) {
          BackspacePressed();
        } else {
          SetChanged(true);
          Redraw();
        }
      }
    }
  }
}

// Return the vectors used to save positions for the holes drawn around current point
// if appropriate for editing
bool CNavigatorDlg::GetHolePositionVectors(FloatVec **xypos, IntVec **index)
{
  CMapDrawItem *item = GetSingleSelectedItem();
  *xypos = &mCurItemHoleXYpos;
  *index = &mCurItemHoleIndex;
  if (!m_bEditMode || !item)
    return false;
  return item->IsPoint() && item->mNumPoints == 1 &&
    mSelectedItems.size() <= 1;
}

// Do common operations for adding a point marked on an image
CMapDrawItem *CNavigatorDlg::AddPointMarkedOnBuffer(EMimageBuffer *imBuf, float stageX, 
                                                    float stageY, int groupID)
{ 
  CMapDrawItem *item, *mapItem;
  float stageZ;
  item = MakeNewItem(groupID);
  SetCurrentStagePos(mCurrentItem);
  item->mStageX = stageX;
  item->mStageY = stageY;

  // The registration of the buffer rules.  If it doesn't have one, assign current reg
  if (!imBuf->mRegistration)
    imBuf->mRegistration = mCurrentRegistration;
  item->mRegistration = imBuf->mRegistration;
  item->mOriginalReg = imBuf->mRegistration;
  item->mDrawnOnMapID = imBuf->mMapID;

  // Find map item from imbuf mapID and check its import flag, set this as imported too
  // If the imported map has been registered to a real one, set the drawn ID to that
  // Also inherit the backlash directly from that item, or from the buffer
  mapItem = FindItemWithMapID(imBuf->mMapID);
  if (mapItem && mapItem->mImported > 0)
    item->mImported = 2;
  if (mapItem && mapItem->mRegisteredToID && mapItem->mImported)
    item->mDrawnOnMapID = mapItem->mRegisteredToID;
  if (mapItem)
    item->mStageZ = mapItem->mStageZ;
  else if (imBuf->GetStageZ(stageZ))
    item->mStageZ = stageZ;
  if (!item->mDrawnOnMapID)
    item->mMapMagInd = imBuf->mMagInd;
  TransferBacklash(imBuf, item);
  item->AppendPoint(stageX, stageY);
  return item;
}

// Delete points on backspace if drawing polygon, adding points, or in Edit mode
BOOL CNavigatorDlg::BackspacePressed()
{
  CMapDrawItem *item;
  if ((mAddingPoints || m_bEditMode) && !mAddingPoly) {
    if (mAddingPoints && mItemArray.GetSize() <= mNumberBeforeAdd)
      return false;
    if (m_bEditMode && !mAddingPoints) {
      item = GetSingleSelectedItem();
      if (!item || item->IsNotPoint())
        return false;
    }
    mRemoveItemOnly = true;
    DeleteItem();
    mRemoveItemOnly = false;
  } else {
    if (mAddingPoly < 2)
      return false;
    SetCurrentItem();
    if (!mItem->mNumPoints)
      return false;
    mItem->mNumPoints--;
  }
  Redraw();
  return true;
}

/////////////////////////////////////////////////////////////////////
// ROUTINES FOR PROVIDING ITEMS TO DRAW, AND GETTING DRAWING TRANSFORMS

// The call for SerialEMView to get the items to draw
CArray<CMapDrawItem *, CMapDrawItem *> *CNavigatorDlg::GetMapDrawItems(
  EMimageBuffer *imBuf, ScaleMat &aMat, float &delX, float &delY, BOOL &drawAllReg, 
  CMapDrawItem **acquireBox)
{
  float angle, tiltAngle;
  bool showMulti, asIfLowDose, showCurPtAcquire, showLDareas;
  int ring;
  if (!SetCurrentItem(true))
    mItem = NULL;
  *acquireBox = NULL;
  if (m_bDrawNone || !BufferStageToImage(imBuf, aMat, delX, delY))
    return NULL;
  drawAllReg = m_bDrawAllReg;
  MultiShotParams *msParams = mHelper->GetMultiShotParams();

  // Show multishot somehow if one or other type is on, and either the dialog is open
  // or acquire is on and "Show shots when show acquire" is checked
  showMulti = ((msParams->inHoleOrMultiHole & MULTI_IN_HOLE) || 
    mHelper->MultipleHolesAreSelected()) && 
    ((m_bShowAcquireArea && (mHelper->GetEnableMultiShot() & 1)) ||
    (mHelper->mMultiShotDlg && !mHelper->mMultiShotDlg->RecordingISValues()));

  // Show something on the current point if multi draw is on or show acquire is on and
  // there is no user point (turns off draw on the acquire box)
  showCurPtAcquire = !imBuf->mHasUserPt && mItem && (showMulti || (m_bShowAcquireArea &&
    mItem->mAcquire && mItem->mNumPoints == 1 && mItem->mDraw));
  showLDareas = mItem && (mItem->mAcquire || mItem->mTSparamIndex >= 0) && m_bEditFocus &&
    mEditFocusEnabled && mLowDoseDlg->ViewImageOKForEditingFocus(imBuf);

  // Showing low dose area in edit mode: just provide a copy of current item
  if (showLDareas) {
    *acquireBox = new CMapDrawItem(mItem);
    (*acquireBox)->AppendPoint(mItem->mStageX, mItem->mStageY);

  } else if ((imBuf->mHasUserPt || showCurPtAcquire) &&
    (m_bShowAcquireArea || showMulti) && 
    RegistrationUseType(imBuf->mRegistration) != NAVREG_IMPORT) {

      // If there is a user point and the box is on to draw acquire area, get needed
      // parameters: camera and mag index from current state or low dose or montage params
      ScaleMat s2c, c2s, is2cam;
      ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
      int camera = mWinApp->GetCurrentCamera();
      MontParam *montp;
      BOOL montaging = mWinApp->Montaging();
      int *activeList;
      LowDoseParams *ldp;
      int magInd, sizeX, sizeY, ind, useLineEnd;
      float ptX, ptY, cornX, cornY, rotation;
      asIfLowDose = mWinApp->LowDoseMode() || mWinApp->GetDummyInstance() || showMulti;
      if (montaging)
        montp = mWinApp->GetMontParam();
      if (asIfLowDose) {
        ldp = mWinApp->GetLowDoseParams();
        magInd = ldp[RECORD_CONSET].magIndex;
        if (montaging && !showMulti)
          magInd = ldp[MontageLDAreaIndex(montp)].magIndex;

      } else if (montaging) {
        magInd = montp->magIndex;
        activeList = mWinApp->GetActiveCameraList();
        camera = activeList[montp->cameraIndex];
      } else
        magInd = mScope->FastMagIndex();

      // Get the camera to stage matrix, determine the frame size, and make an item 
      // with stage coordinates in the corners of that frame
      s2c = mShiftManager->StageToCamera(camera, magInd);
      if (s2c.xpx) {
        c2s = MatInv(s2c);
        if (imBuf->GetTiltAngle(tiltAngle) && fabs((double)tiltAngle) > 1.)
          mShiftManager->AdjustCameraToStageForTilt(c2s, tiltAngle);
        CMapDrawItem *box = new CMapDrawItem;
        *acquireBox = box;
        useLineEnd = imBuf->mHasUserLine ? 1 : 0;
        if (showMulti && mWinApp->LowDoseMode() && mLowDoseDlg->m_iDefineArea)
          useLineEnd = -1;
        if (showCurPtAcquire) {
          box->mStageX = mItem->mStageX;
          box->mStageY = mItem->mStageY;
        } else {
          MarkerStagePosition(imBuf, aMat, delX, delY, box->mStageX, box->mStageY,
            useLineEnd);
        }
        sizeX = conSet->right - conSet->left;
        sizeY = conSet->bottom - conSet->top;
        if (montaging && !showMulti) {
          sizeX = montp->binning * (montp->xFrame + (montp->xFrame - montp->xOverlap) * 
            (montp->xNframes - 1));
          sizeY = montp->binning * (montp->yFrame + (montp->yFrame - montp->yOverlap) * 
            (montp->yNframes - 1));
        }
        for (ind = 0; ind < 5; ind++) {
          cornX = (float)(0.5 * sizeX * (1 - 2 * ((ind / 2) % 2)));
          cornY = (float)(0.5 * sizeY * (1 - 2 * (((ind + 1) / 2) % 2)));
          ptX = box->mStageX + c2s.xpx * cornX + c2s.xpy * cornY;
          ptY = box->mStageY + c2s.ypx * cornX + c2s.ypy * cornY;
          box->AppendPoint(ptX, ptY);
        }

        // If showing circles, then get the parameters and compute stage coordinates for 
        // each circle from camera coordinates; add more points to box
        if (showMulti) {
          float inHoleRadius = 0., beamRadius;
          float pixel = mShiftManager->GetPixelSize(camera, magInd);
          FloatVec delISX, delISY;
          IntVec holeIndex;
          CameraParameters *camParam = mWinApp->GetCamParams() + camera;
          bool custom = msParams->useCustomHoles && msParams->customHoleX.size() > 0;
          is2cam = mShiftManager->IStoGivenCamera(magInd, camera);
          if (pixel && is2cam.xpx) {
            mIStoStageForHoles = MatMul(is2cam, c2s);
            mMagIndForHoles = magInd;
            mCameraForHoles = camera;

            // Make multihole positions relative to center
            if (mHelper->MultipleHolesAreSelected()) {
              if (!imBuf->GetTiltAngle(tiltAngle))
                tiltAngle = -999.;
              int numHoles = mWinApp->mParticleTasks->GetHolePositions(delISX, delISY,
                holeIndex, magInd, camera, 0, 0, tiltAngle);
              AddHolePositionsToItemPts(delISX, delISY, holeIndex, custom, numHoles, box);
            }

            // Multishot positions in hole, with absolute positions
            if (msParams->inHoleOrMultiHole & MULTI_IN_HOLE) {
              rotation = 0.;
              if (imBuf->GetTiltAngle(tiltAngle) && fabs((double)tiltAngle) > 20.)
                rotation = (float)mShiftManager->GetImageRotation(camera, magInd);
              else if (camParam->sizeY < camParam->sizeX)
                rotation = 90.f;
              for (ring = 0; ring < (msParams->doSecondRing ? 2 : 1); ring++) {
                inHoleRadius = msParams->spokeRad[ring] / pixel;
                for (ind = 0; ind < msParams->numShots[ring]; ind++) {
                  angle = (float)(DTOR * (ind * 360. / msParams->numShots[ring] + 
                    rotation));
                  cornX = inHoleRadius * (float)cos(angle);
                  cornY = inHoleRadius * (float)sin(angle);
                  ptX = box->mStageX + c2s.xpx * cornX + c2s.xpy * cornY;
                  ptY = box->mStageY + c2s.ypx * cornX + c2s.ypy * cornY;
                  box->AppendPoint(ptX, ptY);
                }
              }
            }

            // Add one more point with beam radius as a position in X in camera coords
            beamRadius = 0.5f * msParams->beamDiam / pixel;
            if (msParams->useIllumArea && mWinApp->mScope->GetUseIllumAreaForC2() && 
              asIfLowDose)
              beamRadius = (float)(50. * 
              mWinApp->mScope->IntensityToIllumArea(ldp[RECORD_CONSET].intensity) /
                pixel);
            ptX = box->mStageX + c2s.xpx * beamRadius;
            ptY = box->mStageY + c2s.ypx * beamRadius;
            box->AppendPoint(ptX, ptY);

            // Add another point with outside radius for whole acquisition in hole
            ptX = box->mStageX + c2s.xpx * (beamRadius + inHoleRadius);
            ptY = box->mStageY + c2s.ypx * (beamRadius + inHoleRadius);
            box->AppendPoint(ptX, ptY);
          }
        }
        box->mRegistration = mCurrentRegistration;
        box->mType = ITEM_TYPE_POLYGON;
        box->mColor = POINT_ACQUIRE_COLOR;
        box->mDraw = ((mItem && mItem->mDraw && !mItem->mNumSkipHoles) || 
          imBuf->mHasUserPt);
        if (mItem)
          box->mPieceDrawnOn = mItem->mPieceDrawnOn;
      }
  }
  return &mItemArray;
}

// Make multihole positions relative to center and add them to given item
void CNavigatorDlg::AddHolePositionsToItemPts(FloatVec &delISX, FloatVec &delISY,
  IntVec &holeIndex, bool custom, int numHoles, CMapDrawItem *item)
{
  int ind, minInd = -1, indexInd;
  float dist, ptX, ptY, minDist = 1.e20f;
  for (ind = 0; ind < numHoles; ind++) {
    ptX = mIStoStageForHoles.xpx * delISX[ind] + mIStoStageForHoles.xpy * delISY[ind];
    ptY = mIStoStageForHoles.ypx * delISX[ind] + mIStoStageForHoles.ypy * delISY[ind];
    dist = sqrtf(ptX * ptX + ptY * ptY);
    if (dist < minDist) {
      minInd = item->mNumPoints;
      minDist = dist;
      indexInd = ind;
    }
    item->AppendPoint(ptX, ptY);
  }

  // Swap last point for closest point;
  if (!custom && numHoles > 0) {
    B3DSWAP(item->mPtX[minInd], item->mPtX[item->mNumPoints - 1], ptX);
    B3DSWAP(item->mPtY[minInd], item->mPtY[item->mNumPoints - 1], ptY);
    B3DSWAP(holeIndex[2 * indexInd], holeIndex[2 * numHoles - 2], ind);
    B3DSWAP(holeIndex[2 * indexInd + 1], holeIndex[2 * numHoles - 1], ind);
  }
}

// Compute the scale transformation from stage to image in given buffer
BOOL CNavigatorDlg::BufferStageToImage(EMimageBuffer *imBuf, ScaleMat &aMat, 
                                    float &delX, float &delY)
{
  float stageX, stageY, tmpX, tmpY;
  ScaleMat rMat;
  if (!imBuf->mImage)
    return false;
  float width = (float)imBuf->mImage->getWidth();
  float height = (float)imBuf->mImage->getHeight();
  CMapDrawItem *item = FindItemWithMapID(imBuf->mMapID);

  // If the buffer has a map ID and a matching map item, return its scaling
  if (item || imBuf->mStage2ImMat.xpx) {
    if (imBuf->mStage2ImMat.xpx) {
      aMat = imBuf->mStage2ImMat;
      delX = imBuf->mStage2ImDelX;
      delY = imBuf->mStage2ImDelY;
    } else if (item) {
      aMat = item->mMapScaleMat;

      // Because overview binning may not match when map was stored, adjust matrix
      // by the change in size and recompute delX and delY to fit this size
      aMat.xpx *= imBuf->mLoadWidth / imBuf->mUseWidth;
      aMat.xpy *= imBuf->mLoadWidth / imBuf->mUseWidth;
      aMat.ypx *= imBuf->mLoadHeight / imBuf->mUseHeight;
      aMat.ypy *= imBuf->mLoadHeight / imBuf->mUseHeight;
      delX = (item->mMapWidth / imBuf->mUseWidth) * imBuf->mLoadWidth / 2.f - 
        aMat.xpx * item->mStageX - aMat.xpy * item->mStageY;
      delY = (2.f - item->mMapHeight / imBuf->mUseHeight) * imBuf->mLoadHeight / 2.f - 
        aMat.ypx * item->mStageX - aMat.ypy * item->mStageY;
      /*SEMTrace('n', "Transform for map %s at %.3f %.3f: %f %f %f %f %.2f %.2f", 
      (LPCTSTR)item->mLabel, item->mStageX, 
      item->mStageY, aMat.xpx, aMat.xpy, aMat.ypx, aMat.ypy, delX, delY);
      SEMTrace('n', "W/H for im: %.0f %.0f  load: %d %d  use: %.0f %.0f  map: %d %d", width, 
      height, imBuf->mLoadWidth, imBuf->mLoadHeight, imBuf->mUseWidth, imBuf->mUseHeight,
      item->mMapWidth, item->mMapHeight); */
    }

    // Rotate the transformation if the map has been rotated.
    // But the cross-terms of rotation matrix need to be negated to operate on inverted Y
    if (imBuf->mRotAngle || imBuf->mInverted || width != imBuf->mLoadWidth || 
      height != imBuf->mLoadHeight) {
      rMat = GetRotationMatrix(imBuf->mRotAngle, imBuf->mInverted);
      rMat.xpy *= -1.;
      rMat.ypx *= -1.;
      aMat = MatMul(aMat, rMat);
      tmpX = delX - imBuf->mLoadWidth / 2.f;
      tmpY = delY - imBuf->mLoadHeight / 2.f;
      delX = rMat.xpx * tmpX + rMat.xpy * tmpY + width / 2.f;
      delY = rMat.ypx * tmpX + rMat.ypy * tmpY + height / 2.f;
      /*SEMTrace('n', "Transform after rotation (%.2f %d): %f %f %f %f %.2f %.2f", 
        imBuf->mRotAngle, imBuf->mInverted?1:0, aMat.xpx, aMat.xpy, aMat.ypx, aMat.ypy, 
        delX, delY); */
    }
    /*SEMTrace('n', "Map polygon: %.3f,%.3f  %.3f,%.3f  %.3f,%.3f  %.3f,%.3f", 
      item->mPtX[0], item->mPtY[0], item->mPtX[1], item->mPtY[1], 
      item->mPtX[2], item->mPtY[2], item->mPtX[3], item->mPtY[3]); */
    return true;
  }

  if (!imBuf->GetStagePosition(stageX, stageY))
    return false;

  if (!imBuf->mBinning || !imBuf->mMagInd)
    return false;
  ComputeStageToImage(imBuf, stageX, stageY, true, aMat, delX, delY);
  return true;
}

// Actually compute from the given stage coordinates and other image properties
void CNavigatorDlg::ComputeStageToImage(EMimageBuffer *imBuf, float stageX, float stageY,
                                        BOOL needAddIS, ScaleMat &aMat, float &delX, 
                                        float &delY)
{
  float angle = RAW_STAGE_TEST - 1000.;
  BOOL hasAngle = imBuf->GetTiltAngle(angle);

  // Convert any image shift of image into an additional stage shift
  if (needAddIS)
    ConvertIStoStageIncrement(imBuf->mMagInd, imBuf->mCamera, imBuf->mISX, imBuf->mISY,
      angle, stageX, stageY);

  // Sign woes as usual.  This transform is a true transformation from the stage
  // to the camera coordinate system, but invert Y to get to image coordinate system
  aMat = mShiftManager->FocusAdjustedStageToCamera(imBuf);
  aMat.xpx /= imBuf->mBinning;
  aMat.xpy /= imBuf->mBinning;
  aMat.ypx /= -imBuf->mBinning;
  aMat.ypy /= -imBuf->mBinning;

  // If tilt angle is available and it makes more than 0.02% difference, adjust matrix
  if (hasAngle && fabs((double)angle) > 1.)
    mShiftManager->AdjustStageToCameraForTilt(aMat, angle);
  delX = imBuf->mImage->getWidth() / 2.f - aMat.xpx * stageX - aMat.xpy * stageY;
  delY = imBuf->mImage->getHeight() / 2.f - aMat.ypx * stageX - aMat.ypy * stageY;
}

// If scaling exists, convert the image shift into additional stage shift
BOOL CNavigatorDlg::ConvertIStoStageIncrement(int magInd, int camera, double ISX, 
  double ISY, float angle, float &stageX, float &stageY)
{
  ScaleMat aMat, bMat;
  
  // If scope did not already subtract mag offsets, do so now for given camera type
  if (!mScope->GetApplyISoffset()) {
    int gif = mCamParams[camera].GIF ? 1 : 0;
    ISX -= mMagTab[magInd].calOffsetISX[gif];
    ISY -= mMagTab[magInd].calOffsetISY[gif];
  }

  // If shifting to tilt axis is on, convert offset to stage and adjust position to get
  // back to coordinates in unshifted system
  if (mScope->GetShiftToTiltAxis()) {
    aMat = mShiftManager->SpecimenToStage(1., 1.);
    stageX += aMat.xpy * mScope->GetTiltAxisOffset();
    stageY += aMat.ypy * mScope->GetTiltAxisOffset();
  }

  // Here the plus sign replicates the plus in image shift reset
  aMat = mShiftManager->IStoGivenCamera(magInd, camera);
  if (aMat.xpx) {
    bMat = MatInv(mShiftManager->StageToCamera(camera, magInd));
    if (angle > RAW_STAGE_TEST && fabs((double)angle) > 1.)
      mShiftManager->AdjustCameraToStageForTilt(bMat, angle);
    aMat = MatMul(aMat, bMat);
    stageX += (float)(aMat.xpx * ISX + aMat.xpy * ISY);
    stageY += (float)(aMat.ypx * ISX + aMat.ypy * ISY);
    return true;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////
// MONTAGE SETUP

// Setup a montage from corner points
void CNavigatorDlg::CornerMontage() 
{

  // Make a temporary item to put points in, and use to set up montage
  mWinApp->RestoreViewFocus();
	CMapDrawItem *itmp = new CMapDrawItem;
  FillItemWithCornerPoints(itmp);
  SetupMontage(itmp, NULL, false);
  delete itmp;
}

// Fill an item's points with the corner points
void CNavigatorDlg::FillItemWithCornerPoints(CMapDrawItem *itmp)
{
	CMapDrawItem *item;
  double cenX = 0., cenY = 0.;
  double *angles;
  double angTmp;
  float xyTmp;
  int i, j;

  // add positions for corners at this registration, and get centroid
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mCorner && item->mRegistration == mCurrentRegistration) {
      itmp->AppendPoint(item->mStageX, item->mStageY);
      cenX += item->mStageX;
      cenY += item->mStageY;
    }
  }

  // Calculate angle of each point around centroid and order them by angle
  cenX /= itmp->mNumPoints;
  cenY /= itmp->mNumPoints;
  itmp->mStageX = (float)cenX;
  itmp->mStageY = (float)cenY;
  angles = new double[itmp->mNumPoints];
  for (i = 0; i < itmp->mNumPoints; i++)
    angles[i] = atan2(itmp->mPtY[i] - cenY, itmp->mPtX[i] - cenX);
  for (i = 0; i < itmp->mNumPoints - 1; i++) {
    for (j = i + 1; j < itmp->mNumPoints; j++) {
      if (angles[i] > angles[j]) {
        angTmp = angles[i];
        angles[i] = angles[j];
        angles[j] = angTmp;
        xyTmp = itmp->mPtX[i];
        itmp->mPtX[i] = itmp->mPtX[j];
        itmp->mPtX[j] = xyTmp;
        xyTmp = itmp->mPtY[i];
        itmp->mPtY[i] = itmp->mPtY[j];
        itmp->mPtY[j] = xyTmp;
      }
    }
  }
  delete [] angles;
}

// Make a polygon from corner points so it can be used for later montage setup
void CNavigatorDlg::PolygonFromCorners(void)
{
  CMapDrawItem *item = MakeNewItem(0);
  FillItemWithCornerPoints(item);
  item->AppendPoint(item->mPtX[0], item->mPtY[0]);
  item->mType = ITEM_TYPE_POLYGON;
  item->mColor = DEFAULT_POLY_COLOR;
  UpdateListString(mCurrentItem);
  mWinApp->RestoreViewFocus();
  Redraw();
  if (AfxMessageBox("Do you want to delete the corner points used for\n"
    "the polygon from the Navigator table?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
      for (int i = (int)mItemArray.GetSize() - 1; i >= 0; i--) {
        item = mItemArray[i];
        if (item->mCorner && item->mRegistration == mCurrentRegistration) {
          mItemArray.RemoveAt(i);
          delete item;
        }
      }
  } else {
        
    // Turn off the corner setting for all these points to avoid errors if command
    // is used with new points
    for (int i = (int)mItemArray.GetSize() - 1; i >= 0; i--) {
      item = mItemArray[i];
      if (item->mCorner && item->mRegistration == mCurrentRegistration)
        item->mCorner = false;
    }
  }
  FinishMultipleDeletion();
}

// Setup a montage from a polygon
int CNavigatorDlg::PolygonMontage(CMontageSetupDlg *montDlg, bool skipSetupDlg) 
{
	CMapDrawItem *itmp;
  CString str;
  MontParam *montp;
  int err;
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem())
    return 1;
  if (mItem->IsNotPolygon())	
    return 1;
  itmp = new CMapDrawItem;
  for (int i = 0; i < mItem->mNumPoints; i++)
    itmp->AppendPoint(mItem->mPtX[i], mItem->mPtY[i]);
  err = SetupMontage(itmp, montDlg, skipSetupDlg);
  if (!err && (fabs(mItem->mStageX - itmp->mStageX) > 0.005 || 
    fabs(mItem->mStageY - itmp->mStageY) > 0.005)) {
      if (!mHelper->GetDoingMultipleFiles()) {
        str.Format("Changing center of polygon from %.2f,%.2f to center for montage, "
          "%.2f,%.2f", mItem->mStageX, mItem->mStageY, itmp->mStageX, itmp->mStageY);
      mWinApp->AppendToLog(str);
      }
    mItem->mStageX = itmp->mStageX;
    mItem->mStageY = itmp->mStageY;
    montp = mWinApp->GetMontParam();
    montp->fitToPolygonID = mItem->mMapID;
    UpdateListString(mCurrentItem);
    SetChanged(true);
  }
  delete itmp;
  return err;
}

// Setup a montage for the full area (or user specified area)
void CNavigatorDlg::FullMontage()
{
  float minX, minY, maxX, maxY, midX, midY, cornerX, cornerY;
  float minCornerX, maxCornerX, minCornerY, maxCornerY, maxOverNominal;
  float fullCornXmin, fullCornXmax, fullCornYmin, fullCornYmax;
  float cornerExtra = 93.;
  float *gridLim = mHelper->GetGridLimits();
  float nominalLim = JEOLscope ? 1200.f : 1000.f;
  MontParam *montp = mWinApp->GetMontParam();
  mWinApp->RestoreViewFocus();
	CMapDrawItem *itmp = new CMapDrawItem;

  // First get limits based on system defaults
  minX = mScope->GetStageLimit(STAGE_MIN_X);
  maxX = mScope->GetStageLimit(STAGE_MAX_X);
  minY = mScope->GetStageLimit(STAGE_MIN_Y);
  maxY = mScope->GetStageLimit(STAGE_MAX_Y);
  midX = 0.5f * (minX + maxX);
  midY = 0.5f * (minY + maxY);

  // Setup corners with an extra amount added.  Take maximum of corner indicated
  // by the defined limits and the corner for a full centered 1000 um area (but allow a
  // bigger limit to be entered for JEOL - 5/26/14).
  // Moving the corners outward in this step assumes the grid is centered at 0,0
  fullCornXmin = 0.707f * B3DMIN(-1000.f, minX);
  fullCornXmax = 0.707f * B3DMAX(1000.f, maxX);
  fullCornYmin = 0.707f * B3DMIN(-1000.f, minY);
  fullCornYmax = 0.707f * B3DMAX(1000.f, maxY);
  cornerX = 1.414f * (maxX - minX) / 4.f;
  cornerY = 1.414f * (maxY - minY) / 4.f;
  maxOverNominal = 0.;

  // Phase out the extra addition to the corners when set stage limit approaches nominal
  ACCUM_MAX(maxOverNominal, -nominalLim - minX);
  ACCUM_MAX(maxOverNominal, maxX - nominalLim);
  ACCUM_MAX(maxOverNominal, -nominalLim - minY);
  ACCUM_MAX(maxOverNominal, maxY - nominalLim);
  cornerExtra = B3DMAX(0.f, cornerExtra - maxOverNominal);
  minCornerX = B3DMIN(midX - cornerX - cornerExtra, fullCornXmin - cornerExtra);
  maxCornerX = B3DMAX(midX + cornerX + cornerExtra, fullCornXmax + cornerExtra);
  minCornerY = B3DMIN(midY - cornerY - cornerExtra, fullCornYmin - cornerExtra);
  maxCornerY = B3DMAX(midY + cornerY + cornerExtra, fullCornYmax + cornerExtra);

  // Get user limits if any, and limit by system limits (new 6/16/11)
  if (gridLim[STAGE_MIN_X])
    minX = B3DMAX(minX, gridLim[STAGE_MIN_X]);
  if (gridLim[STAGE_MAX_X])
    maxX = B3DMIN(maxX, gridLim[STAGE_MAX_X]);
  if (gridLim[STAGE_MIN_Y])
    minY = B3DMAX(minY, gridLim[STAGE_MIN_Y]);
  if (gridLim[STAGE_MAX_Y])
    maxY = B3DMIN(maxY, gridLim[STAGE_MAX_Y]);

  // Now make the corners be no farther out than whatever the limit is
  minCornerX = B3DMAX(minCornerX, minX);
  maxCornerX = B3DMIN(maxCornerX, maxX);
  minCornerY = B3DMAX(minCornerY, minY);
  maxCornerY = B3DMIN(maxCornerY, maxY);
  midX = 0.5f * (minX + maxX);
  midY = 0.5f * (minY + maxY);
  if (maxX - minX < 10 || maxY - minY < 10) {
    AfxMessageBox("Your grid limits for the full montage are set less than 10 microns"
      " apart\n\nCannot set up a full montage with these limits", MB_EXCLAME);
    delete itmp;
    return;
  }
  
  itmp->AppendPoint(minX, midY);
  itmp->AppendPoint(minCornerX, maxCornerY);
  itmp->AppendPoint(midX, maxY);
  itmp->AppendPoint(maxCornerX, maxCornerY);
  itmp->AppendPoint(maxX, midY);
  itmp->AppendPoint(maxCornerX, minCornerY);
  itmp->AppendPoint(midX, minY);
  itmp->AppendPoint(minCornerX, minCornerY);

  mSettingUpFullMont = true;
  SetupMontage(itmp, NULL, false);
  mSettingUpFullMont = false;
  if (mWinApp->Montaging()) {
    montp->forFullMontage = true;
    montp->fullMontStageX = itmp->mStageX;
    montp->fullMontStageY = itmp->mStageY;
  }
  delete itmp;
}

// Common routine to set up a montage
int CNavigatorDlg::SetupMontage(CMapDrawItem *item, CMontageSetupDlg *montDlg, 
  bool skipSetupDlg)
{
  ScaleMat aInv;
  int i, err;
  float extraSizeFactor = 0.05f;   // Fraction to increase size overall
  float overlapFactor = 0.1f;      // Could come from SEMDoc
  int iCam = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + iCam;
  float askLimitRatio = 0.9f;    // Ask user if Record frame smaller than camera by this
  int magIndex = mScope->GetMagIndex();
  BOOL lowDose = mWinApp->LowDoseMode();
  int trial, numTry = 1;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  MontParam *montParam = mMontParam;
  CString *modeNames = mWinApp->GetModeNames();
  BOOL forceStage = false;
  if (montDlg) {
    magIndex = montDlg->mParam.magIndex;
    montParam = &montDlg->mParam;
    forceStage = montDlg->mParam.wasFitToPolygon && montDlg->mParam.moveStage;
  }

  // Check number - it should be OK but just in case
  if (item->mNumPoints < 3) {
    AfxMessageBox("There must be at least 3 points to define a montage area",
      MB_EXCLAME);
    return 1;
  }

  // Leave the current file before touching the montage parameters
  if (!montDlg)
    mDocWnd->LeaveCurrentFile();

  // Copy the item so it can be used for camera coords by skip list setup
 	mMontItemCam = new CMapDrawItem;
  for (i = 0; i < item->mNumPoints; i++)
    mMontItemCam->AppendPoint(item->mPtX[i], item->mPtX[i]);
  if (item->mMapTiltAngle <= RAW_STAGE_TEST)
    item->mMapTiltAngle = (float)mScope->FastTiltAngle();
  mMontItemCam->mMapTiltAngle = item->mMapTiltAngle;
  mMontItem = item;

  if (lowDose && !montParam->useViewInLowDose && !(montParam->useSearchInLowDose && 
    ldp[SEARCH_AREA].magIndex))
    numTry = 2;

  for (trial = 0; trial < numTry; trial++) {
    int consetNum = MontageConSetNum(montParam, false);
    if (lowDose)
      magIndex = ldp[MontageLDAreaIndex(montParam)].magIndex;
    ControlSet *conSet = mWinApp->GetConSets() + consetNum;
    int binning = conSet->binning;

    // Check for Record area smaller than camera and ask if user wants that limit
    mFrameLimitX = camParam->sizeX;
    mFrameLimitY = camParam->sizeY;
    mWinApp->mMontageController->LimitSizesToUsable(camParam, iCam, magIndex, 
      mFrameLimitX, mFrameLimitY, 1);
    if ((conSet->right - conSet->left < askLimitRatio * mFrameLimitX ||
      conSet->bottom - conSet->top < askLimitRatio * mFrameLimitY) &&
      AfxMessageBox("The " + modeNames[consetNum] + " area is significantly smaller than "
      "the camera frame size."
      "\n\nDo you want to keep the montage frame size from being bigger than this area?"
      "\n (Otherwise the frame might be as big as the full camera field).",
      MB_YESNO | MB_ICONQUESTION) == IDYES) {
        mFrameLimitX = conSet->right - conSet->left;
        mFrameLimitY = conSet->bottom - conSet->top;
    }

    //montParam->minOverlapFactor = overlapFactor;  ??  HUH??
    if ((err = FitMontageToItem(montParam, binning, magIndex, forceStage))) {
      if (lowDose && !montParam->useViewInLowDose) {
        montParam->useViewInLowDose = true;
        continue;
      }
      AfxMessageBox(err == 2 ? "The amount of overlap between frames required to do a "
        "montage with stage\nmovement is too high (more than half the frame size) at this"
        " magnification" : "This area would require more than the\n allowed number of "
        "montage pieces", MB_EXCLAME);
      if (!montDlg)
        mDocWnd->RestoreCurrentFile();
      mMontItem = NULL;
      delete mMontItemCam;
      if (numTry == 2)
        montParam->useViewInLowDose = false;
      return 1;
    }
  }
  if (numTry == 2 && montParam->useViewInLowDose)
    AfxMessageBox("Turning on \"Use " + modeNames[VIEW_CONSET] + " in Low Dose\" because "
    "area could not be fit with " + modeNames[RECORD_CONSET] + " parameters", MB_EXCLAME);

  // Call with argument to indicate frame set up.  It takes care of restoring current
  // file if it is aborted
  if (montDlg) {
    err = B3DCHOICE(!skipSetupDlg && montDlg->DoModal() != IDOK, 1, 0);
  } else {
    err = mDocWnd->GetMontageParamsAndFile(true);
  }
  if (err) {
    mMontItem = NULL;
    delete mMontItemCam;
    return 1;
  }

  // If no cancel, redo the skip list in case of changes then move to center
  // Also set overview binning to 1 since that is the point of the overview
  montParam->overviewBinning = 1;
  mWinApp->mMontageWindow.UpdateSettings();
  SetupSkipList(montParam);
  mMontItem = NULL;
  delete mMontItemCam;

  // Store new center stage position in the temporary item for fixing polygon center
  // Go to center only if not defining for future
  aInv = MatInv(mPolyToCamMat);
  mShiftManager->AdjustCameraToStageForTilt(aInv, item->mMapTiltAngle);
  item->mStageX = aInv.xpx * mCamCenX + aInv.xpy * mCamCenY;
  item->mStageY = aInv.ypx * mCamCenX + aInv.ypy * mCamCenY;
  if (!montDlg) {
    AdjustAndMoveStage(item->mStageX, item->mStageY, 0., axisXY);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
    mWinApp->UpdateBufferWindows();
  }
  return 0;
} 


// Find the smallest montage framing that covers the region of the polygon
int CNavigatorDlg::FitMontageToItem(MontParam *montParam, int binning, int magIndex,
                                    BOOL forceStage)
{
  ScaleMat aMat;
  float xMin, xMax, yMin, yMax, xx, yy, pixel, maxIS, tmpov, rotation;
  double maxISX, maxISY;
  int i, xNframes, yNframes, overlap, camSize, left, right, which, xFrame, yFrame;
  int top, bot, saveOverlap, set = -1;
  int binx2 = 2 * binning;
  int maxOverlap = (int)(mDocWnd->GetMaxOverlapFraction() * 
    B3DMIN(mFrameLimitX, mFrameLimitY));
  float extraSizeFactor = 0.05f;   // Fraction to increase size overall
  float maxOverlapInc = 1.5f;      // Maximum to increase overlap with restrictied sizes
  int iCam = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + iCam;
  CMapDrawItem *item = mMontItem;

  // Force a stage montage if there is no image shift calibration available
  aMat = mShiftManager->IStoCamera(magIndex);
  if (!aMat.xpx)
    forceStage = true;

  // When the image is at an oblique angle for a full montage, adding extra area can
  // lead to corner pieces being lost due to stage coordinates out of range.  Try to
  // solve that by setting extra factor to 0 above a certain angle
  if (mSettingUpFullMont) {
    rotation = (float)mShiftManager->GetImageRotation(iCam, magIndex);
    while (rotation > 45.)
      rotation -= 90.;
    while (rotation < -45.)
      rotation += 90.;
    if (fabs(rotation) > mMaxAngleExtraFullMont)
      extraSizeFactor = 0.;
  }

  // Transform item to camera coordinates and save in second item, get min/max
  if (mWinApp->LowDoseMode() && montParam->useViewInLowDose)
    set = VIEW_CONSET;
  else if (mWinApp->LowDoseMode() && montParam->useSearchInLowDose)
    set = SEARCH_CONSET;
  PolygonToCameraCoords(item, iCam, magIndex, set, xMin, xMax, yMin, yMax);

  xx = (1.f + extraSizeFactor) * (xMax - xMin);
  yy = (1.f + extraSizeFactor) * (yMax - yMin);

  camSize = B3DMAX(mFrameLimitX, mFrameLimitY);
  overlap = binx2 * ((int)(montParam->minOverlapFactor * camSize) / binx2);
  pixel = mShiftManager->GetPixelSize(iCam, magIndex);
  maxIS = magIndex < mScope->GetLowestNonLMmag(&mCamParams[iCam]) ? 
    mParam->maxLMMontageIS : mParam->maxMontageIS;

  // Use stage cal if no image shift cal
  if (magIndex < mScope->GetLowestNonLMmag(&mCamParams[iCam]) && 
    !(mShiftManager->CameraToIS(magIndex)).xpx)
    forceStage = true;

  // Loop on doing image shift or stage montage
  for (which = forceStage ? 1 : 0; which < 2; which++) {
    xNframes = 0;
    yNframes = 0;
 
    // If have to do stage, adjust overlap upward and bail out if it is too high
    if (which && overlap < montParam->minMicronsOverlap / pixel) {
      overlap = binx2 * ((int)(montParam->minMicronsOverlap / pixel) / binx2);
      if (overlap > maxOverlap)
        return 2;
    }
 
    for (i = 1; i <= 65536 && (!xNframes || !yNframes); i++) {
      if (!xNframes && (xx + (i - 1) * overlap) / i <= mFrameLimitX)
        xNframes = i;
      if (!yNframes && (yy + (i - 1) * overlap) / i <= mFrameLimitY)
        yNframes = i;
    }

    if (!(xNframes * yNframes))
      continue;
  
    xFrame = (int)(xx + (xNframes - 1 ) * overlap) / (xNframes * binning);
    yFrame = (int)(yy + (yNframes - 1 ) * overlap) / (yNframes * binning);
    saveOverlap = overlap;

    // If fitting to full frames, increase the overlap by the fraction of the extra frame
    // size indicated by the fractional part of the parameter
    if (mParam->fitMontWithFullFrames >= 1.) {
      tmpov = (float)(B3DMIN(mFrameLimitX / binning - xFrame, mFrameLimitY / binning -
        yFrame) * B3DMIN(mParam->fitMontWithFullFrames - 1., 1.));
      overlap = binx2 * ((int)(B3DMIN(overlap + tmpov, maxOverlap) / binx2));
      xFrame = mFrameLimitX / binning;
      yFrame = mFrameLimitY / binning;
    }

    // If image shift, evaluate the maximum extent and either accept or restore overlap
    if (!which) {
      maxISX = (xx - xFrame) / 2.;
      maxISY = (yy - yFrame) / 2.;
      // 9-5-10: the comparison was to mMaxMontageIS and maxIS was unused...
      if (pixel * sqrt(maxISX * maxISX + maxISY * maxISY) < maxIS)
        break;
      overlap = saveOverlap;
    }

  }

  if (!(xNframes * yNframes) || xNframes * yNframes > 65536) {
    return 1;
  }

  // Adjust the sizes properly, set up parameters
  montParam->binning = binning;
  mCamera->CenteredSizes(xFrame, camParam->sizeX, camParam->moduloX, left, 
    right, yFrame, camParam->sizeY, camParam->moduloY, top, bot, binning);

  montParam->xFrame = xFrame;
  montParam->yFrame = yFrame;
  montParam->xNframes = xNframes;
  montParam->yNframes = yNframes;
  montParam->xOverlap = overlap / binning;
  montParam->yOverlap = overlap / binning;
  montParam->moveStage = which > 0;

  // If restricted sizes, increase overlaps by up to 50%
  if (camParam->moduloX < 0) {
    tmpov = (xNframes * xFrame * binning - xx) / B3DMAX(1, xNframes - 1) + 1;
    montParam->xOverlap = ((int)B3DMIN(tmpov, maxOverlapInc * overlap)) / binning;
    tmpov = (yNframes * yFrame * binning - yy) / B3DMAX(1, yNframes - 1) + 1;
    montParam->yOverlap = ((int)B3DMIN(tmpov, maxOverlapInc * overlap)) / binning;
  }

  // Set up a skip list by examining each piece
  mCamCenX = (xMax + xMin) / 2.f;
  mCamCenY = (yMax + yMin) / 2.f;
  mExtraX = 0.5f * extraSizeFactor * (xMax - xMin);
  mExtraY = 0.5f * extraSizeFactor * (yMax - yMin);
  montParam->xOverlap = B3DMAX(montParam->xOverlap, 0);
  montParam->yOverlap = B3DMAX(montParam->yOverlap, 0);
  SetupSkipList(montParam);
  return 0;
}

void CNavigatorDlg::PolygonToCameraCoords(CMapDrawItem * item, int iCam, int magIndex,
  int adjustForFocusSet, float &xMin, float &xMax, float &yMin, float &yMax)
{
  float xx, yy;
  mPolyToCamMat = mShiftManager->StageToCamera(iCam, magIndex);
  if (adjustForFocusSet && magIndex >= mScope->GetLowestNonLMmag())
    mPolyToCamMat = mShiftManager->FocusAdjustedStageToCamera(iCam, magIndex, 
      mScope->FastSpotSize(), mScope->GetProbeMode(), mScope->FastIntensity(), 
      mScope->GetLDViewDefocus(adjustForFocusSet));
  if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
    mShiftManager->AdjustStageToCameraForTilt(mPolyToCamMat, item->mMapTiltAngle);
  xMin = yMin = 1.e10;
  xMax = yMax = -1.e10;
  for (int i = 0; i < item->mNumPoints; i++) {
    xx = mPolyToCamMat.xpx * item->mPtX[i] + mPolyToCamMat.xpy * item->mPtY[i];
    yy = mPolyToCamMat.ypx * item->mPtX[i] + mPolyToCamMat.ypy * item->mPtY[i];
    mMontItemCam->mPtX[i] = xx;
    mMontItemCam->mPtY[i] = yy;
    SEMTrace('n', "-1 %.2f %.2f", xx, yy);
    if (xMin > xx)
      xMin = xx;
    if (xMax < xx)
      xMax = xx;
    if (yMin > yy)
      yMin = yy;
    if (yMax < yy)
      yMax = yy;
  }
}

// Analyze each piece of montage for being inside the polygon, add to skip list if not
void CNavigatorDlg::SetupSkipList(MontParam * montParam)
{
  float xOverlap, yOverlap, montCenX, montCenY, xMid, yMid;
  int ix, iy, xNframes, yNframes, xFrame, yFrame, binning, size;
  CMapDrawItem *item = mMontItemCam;
  binning = montParam->binning;
  xFrame = binning * montParam->xFrame;
  yFrame = binning * montParam->yFrame;
  xOverlap = (float)(binning * montParam->xOverlap);
  yOverlap = (float)(binning * montParam->yOverlap);
  xNframes = montParam->xNframes;
  yNframes = montParam->yNframes;
  montCenX = (float)(xNframes * xFrame - (xNframes - 1) * xOverlap) / 2.f;
  montCenY = (float)(yNframes * yFrame - (yNframes - 1) * xOverlap) / 2.f;
  montParam->numToSkip = 0;
  montParam->skipPieceX.clear();
  montParam->skipPieceX.swap(std::vector<short int>(montParam->skipPieceX));
  montParam->skipPieceY.clear();
  montParam->skipPieceY.swap(std::vector<short int>(montParam->skipPieceY));
  if (xNframes == 1 && yNframes == 1)
    return;

  for (iy = 0; iy < yNframes; iy++) {
    for (ix = 0; ix < xNframes; ix++) {
      
      if (!IsFrameNeeded(item, xFrame, yFrame, xOverlap, yOverlap, mExtraX, mExtraY, 
        mCamCenX - montCenX, mCamCenY- montCenY, ix, iy, xNframes, yNframes, xMid, 
        yMid)) {
        if ((int)montParam->skipPieceX.capacity() < montParam->numToSkip + 1) {
          size = (int)montParam->skipPieceX.capacity() + (xNframes * yNframes + 4) / 5;
          montParam->skipPieceX.resize(size);
          montParam->skipPieceY.resize(size);
        }
        SEMTrace('n', "SetupSkipList adding piece %d  %d to list", ix, iy);
        montParam->skipPieceX[montParam->numToSkip] = ix;
        montParam->skipPieceY[montParam->numToSkip++] = iy;
      }
    }
  }
}

// Test whether a given frame is needed
bool CNavigatorDlg::IsFrameNeeded(CMapDrawItem * item, int xFrame, int yFrame, 
                                  float xOverlap, float yOverlap, float xExtra, 
                                  float yExtra, float xOffset, float yOffset, int ix,
                                  int iy, int xNframes, int yNframes, float &xMid, 
                                  float &yMid)
{
  float xStart, yStart, xEnd, yEnd, xCen, yCen;
  float cornf = 0.6f, cenf = 1.f - cornf;

  // Get start and end coordinates of piece, in camera coordinates, and
  // adjust by extra factor and overlaps
  xStart = (float)(ix * (xFrame - xOverlap) - xExtra) + xOffset;
  xEnd = xStart + (float)xFrame + xExtra;
  xMid = 0.5f * (xStart + xEnd);
  if (ix)
    xStart += xOverlap;
  if (ix < xNframes - 1)
    xEnd -= xOverlap;
  yStart = (float)(iy * (yFrame - yOverlap) - yExtra) + yOffset;
  yEnd = yStart + (float)yFrame + yExtra;
  yMid = 0.5f * (yStart + yEnd);
  if (iy)
    yStart += yOverlap;
  if (iy < yNframes - 1)
    yEnd -= yOverlap;
  xCen = 0.5f * (xStart + xEnd);
  yCen = 0.5f * (yStart + yEnd);

  SEMTrace('n', "%d%d %.2f %.2f\r\n%d%d %.2f %.2f\r\n%d%d %.2f %.2f\r\n%d%d %.2f %.2f",
    ix, iy, xStart, yStart, ix, iy, xStart, yEnd, ix, iy, xEnd, yEnd, ix, iy,xEnd,yStart);

  // Check for each edge line and each diagonal point a fraction cornf from center to 
  // corner for being inside contours
  return (LineInsideContour(item->mPtX, item->mPtY, item->mNumPoints, xStart, yStart, 
    xStart, yEnd) ||
    LineInsideContour(item->mPtX, item->mPtY, item->mNumPoints, xStart, yEnd, xEnd, 
    yEnd) ||
    LineInsideContour(item->mPtX, item->mPtY, item->mNumPoints, xEnd, yEnd, xEnd, 
    yStart) ||
    LineInsideContour(item->mPtX, item->mPtY, item->mNumPoints, xEnd, yStart, xStart, 
    yStart) || InsideContour(item->mPtX, item->mPtY, item->mNumPoints, xCen, yCen) ||
    InsideContour(item->mPtX, item->mPtY, item->mNumPoints, cenf * xCen + cornf * xStart,
    cenf * yCen + cornf * yStart) ||
    InsideContour(item->mPtX, item->mPtY, item->mNumPoints, cenf * xCen + cornf * xEnd,
    cenf * yCen + cornf * yStart) ||
    InsideContour(item->mPtX, item->mPtY, item->mNumPoints, cenf * xCen + cornf * xStart,
    cenf * yCen + cornf * yEnd) ||
    InsideContour(item->mPtX, item->mPtY, item->mNumPoints, cenf * xCen + cornf * xEnd,
    cenf * yCen + cornf * yEnd));
}

// Determine if a line segment crosses inside a contour by sampling
bool CNavigatorDlg::LineInsideContour(float * ptsX, float * ptsY, int numPoints, 
                                      float xStart, float yStart, float xEnd, float yEnd)
{
  float x, y;
  int i, numSample = 10;
  for (i = 0; i <= numSample; i++) {
    x = xStart + (i * (xEnd - xStart)) / numSample;
    y = yStart + (i * (yEnd - yStart)) / numSample;
    if (InsideContour(ptsX, ptsY, numPoints, x, y))
      return true;
  }
  return false;
}


// Super montage setup
void CNavigatorDlg::SetupSuperMontage(BOOL skewed)
{
  float delX, delY, cenX, cenY, cornX, cornY, ptX, ptY;
  float imDelX, imDelY;
  double gamma, cosgam, singam, overlap;
  ScaleMat aMat, aInv, stepMat, imMat, imInv;
  int montSizeX, montSizeY, minSize, startNum, yOverlap;
  MontParam *montP = mWinApp->GetMontParam();
  CMapDrawItem *item;
  BOOL adjust = false;
  BOOL acquire = false;
  int superID = MakeUniqueID();
  int *activeList = mWinApp->GetActiveCameraList();
  int camInd = activeList[montP->cameraIndex];
  double stageZ;
 
  // Get the stage position and current montage size
  mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ);
  montSizeX = montP->xNframes * montP->xFrame - (montP->xNframes - 1) * montP->xOverlap;
  montSizeY = montP->yNframes * montP->yFrame - (montP->yNframes - 1) * montP->yOverlap;
  minSize = montSizeX < montSizeY ? montSizeX : montSizeY;

  // Get matrix for stage to point transformations
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  mWinApp->RestoreViewFocus();
  if (!BufferStageToImage(imBuf, imMat, imDelX, imDelY)) {
    AfxMessageBox("The currently active image does not have enough information\n"
      "to convert the marker point to a stage coordinate.", MB_EXCLAME);
    return;
  }
  imInv = MatInv(imMat);
  if (!mSuperNumX) {
    mSuperNumX = mSuperNumY = 2;
    mSuperOverlap = (montSizeX > montSizeY ? montSizeX : montSizeY) / 10;
  }

  // Get the definition of the montage
  if (!KGetOneInt("Number of montages in X:", mSuperNumX))
    return;
  if (mSuperNumX < 1)
    mSuperNumX = 1;
  if (mSuperNumX > 100)
    mSuperNumX = 100;
  if (!KGetOneInt("Number of montages in Y:", mSuperNumY))
    return;
  if (mSuperNumY < 1)
    mSuperNumY = 1;
  if (mSuperNumY > 100)
    mSuperNumY = 100;
  if (!KGetOneInt("Pixels of overlap in X:", mSuperOverlap))
    return;
  if (mSuperOverlap < 10)
    mSuperOverlap = 10;
  if (mSuperOverlap > montSizeX / 2)
    mSuperOverlap = montSizeX / 2;
  yOverlap = mSuperOverlap;

  // The overlap might need to be different for a rectangular montage but it varies
  // with the skew angle, so don't try to predict it
  if (!KGetOneInt("Pixels of overlap in Y:", yOverlap))
    return;
  if (yOverlap < 10)
    yOverlap = 10;
  if (yOverlap > montSizeY / 2)
    yOverlap = montSizeY / 2;

  if (imBuf->mMiniOffsets)
    adjust = (AfxMessageBox("Do you want to adjust positions based on the\n"
    "aligned positions of pieces in this montage?\n(Do not answer yes if the pieces are"
    " not well lined up\n in the region covered by the supermontage.)",
    MB_YESNO | MB_ICONQUESTION) == IDYES);

  acquire = (AfxMessageBox("Do you want to turn on Acquire for all these new points?",
    MB_YESNO | MB_ICONQUESTION) == IDYES);

  // Get matrix for getting from camera pixels to stage coordinates; add binning
  aMat = mShiftManager->StageToCamera(camInd, montP->magIndex);
  aInv = MatInv(aMat);
  aInv.xpx *= montP->binning;
  aInv.xpy *= montP->binning;
  aInv.ypx *= montP->binning;
  aInv.ypy *= montP->binning;

  // Set up step between frames
  stepMat.xpx = (float)(montSizeX - mSuperOverlap);
  stepMat.xpy = 0.;
  stepMat.ypx = 0.;
  stepMat.ypy = (float)(montSizeY - yOverlap);
  if (skewed) {

    // If skewing frames, get rotation angle and reduce it to -45 to 45 range
    gamma = mShiftManager->GetImageRotation(camInd, montP->magIndex);
    while (gamma > 45.)
      gamma -= 90.;
    while (gamma < -45.)
      gamma += 90.;
    cosgam = cos(DTOR * gamma);
    singam = sin(DTOR * gamma);

    // Adjust overlap and swing the interframe steps
    overlap = montSizeX - (montSizeX - mSuperOverlap) / cosgam;
    stepMat.xpx = (float)(cosgam * (montSizeX - overlap));
    stepMat.ypx = (float)(singam * (montSizeX - overlap));
    overlap = montSizeY - (montSizeY - yOverlap) / cosgam;
    stepMat.ypy = (float)(cosgam * (montSizeY - overlap));
    stepMat.xpy = (float)(-singam * (montSizeY - overlap));
  }

  startNum = mNewItemNum;
  cenX = (float)(0.5 * (mSuperNumX + 1));
  cenY = (float)(0.5 * (mSuperNumY + 1));

  // Matrix goes from step to supermont coords to stage to map image coords
  stepMat = MatMul(MatMul(stepMat, aInv), imMat);
  aMat = MatMul(aInv, imMat);
  for (int iy = 1; iy <= mSuperNumY; iy++) {
    for (int ix = 1; ix <= mSuperNumX; ix++) {
      item = MakeNewItem(superID);
      mNewItemNum--;
      item->mType = ITEM_TYPE_POLYGON;
      item->mColor = DEFAULT_SUPER_COLOR;
      item->mLabel.Format("%d-%d-%d", startNum, ix, iy);
      delX = (ix - cenX) * stepMat.xpx + (iy - cenY) * stepMat.xpy;
      delY = (ix - cenX) * stepMat.ypx + (iy - cenY) * stepMat.ypy;
      ptX = imBuf->mUserPtX + delX;
      ptY = imBuf->mUserPtY + delY;
      if (adjust)
        AdjustMontImagePos(imBuf, ptX, ptY);
      item->mAcquire = acquire;
      item->mDrawnOnMapID = imBuf->mMapID;
      TransferBacklash(imBuf, item);

      item->mStageX = imInv.xpx * (ptX - imDelX) + imInv.xpy * (ptY - imDelY);
      item->mStageY = imInv.ypx * (ptX - imDelX) + imInv.ypy * (ptY - imDelY);
      item->mStageZ = (float)stageZ;
      item->mSuperMontX = (ix - 1) * (montSizeX - mSuperOverlap);
      item->mSuperMontY = (iy - 1) * (montSizeY - yOverlap);

      // Compute corners in map image coordinates so they can be adjusted too
      for (int i = 0; i < 5; i++) {
        cornX = (float)(0.5 * montSizeX * (1 - 2 * ((i / 2) % 2)));
        cornY = (float)(0.5 * montSizeY * (1 - 2 * (((i + 1) / 2) % 2)));
        ptX = imBuf->mUserPtX + delX + aMat.xpx * cornX + aMat.xpy * cornY;
        ptY = imBuf->mUserPtY + delY + aMat.ypx * cornX + aMat.ypy * cornY;
        if (adjust)
          AdjustMontImagePos(imBuf, ptX, ptY);
        cornX = imInv.xpx * (ptX - imDelX) + imInv.xpy * (ptY - imDelY);
        cornY = imInv.ypx * (ptX - imDelX) + imInv.ypy * (ptY - imDelY);
        item->AppendPoint(cornX, cornY);
      }
      UpdateListString(mCurrentItem);
    }
  }
  mNewItemNum++;
  SetChanged(true);
  ManageCurrentControls();
  Update();
  Redraw();
}

// Setup a supermontage to fill the current polygon
void CNavigatorDlg::PolygonSupermontage(void)
{
  int montSizeX, montSizeY, minSize, startNum, overlap, superXnum, superYnum, ix, iy;
  float extraFactor = 0.4f;   // Fraction of frame size to allow extra
  MontParam *montP = mWinApp->GetMontParam();
  CMapDrawItem *item;
  BOOL acquire = false;
  ScaleMat aMat, aInv;
  int *activeList = mWinApp->GetActiveCameraList();
  int camInd = activeList[montP->cameraIndex];
  float extra, fullXsize, fullYsize, halfX, halfY, xMin, xMax, yMin, yMax, xCen, yCen;
  float xMid, yMid, ptX, ptY, cornX, cornY;
  mWinApp->RestoreViewFocus();
  double stageZ;
  int superID, polyID;
  if (!SetCurrentItem())
    return;
 
  // Get the stage position and current montage size and default overlap
  mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ);
  montSizeX = montP->xNframes * montP->xFrame - (montP->xNframes - 1) * montP->xOverlap;
  montSizeY = montP->yNframes * montP->yFrame - (montP->yNframes - 1) * montP->yOverlap;
  minSize = montSizeX < montSizeY ? montSizeX : montSizeY;
  if (!mSuperOverlap)
    mSuperOverlap = B3DMAX(montSizeX, montSizeY) / 10;
  if (!KGetOneInt("Pixels of overlap between montages:", mSuperOverlap))
    return;
  mSuperOverlap = B3DMIN(minSize / 2, B3DMAX(10, mSuperOverlap));

  // Convert to unbinned coords
  overlap = mSuperOverlap * montP->binning;
  montSizeX *= montP->binning;
  montSizeY *= montP->binning;

  acquire = (AfxMessageBox("Do you want to turn on Acquire for all these new points?",
    MB_YESNO | MB_ICONQUESTION) == IDYES);

  // Manage map and supermont IDs
  if (mItem->mMapID) {
    superID = MakeUniqueID();
    polyID = mItem->mMapID;
  } else {
    mItem->mMapID = MakeUniqueID();
    superID = MakeUniqueID();
    polyID = mItem->mMapID;
  }

  if (!montP->skipOutsidePoly) {
    if (AfxMessageBox("Do you want to turn on the option in Montage Setup to skip "
      "montage pieces outside of this polygon?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
      montP->skipOutsidePoly = true;
      montP->insideNavItem = -1;
    }
  }

  // Copy points and then transform them to camera coords (unbinned also)
  mMontItemCam = new CMapDrawItem;
  for (int i = 0; i < mItem->mNumPoints; i++)
    mMontItemCam->AppendPoint(mItem->mPtX[i], mItem->mPtX[i]);
  PolygonToCameraCoords(mItem, camInd, montP->magIndex, -1, xMin, xMax, yMin, yMax);

  // Get needed size with an extra allowance, compute number of supers and half coord
  extra = (float)(extraFactor * B3DMAX(montP->xFrame, montP->yFrame));
  fullXsize = extra + xMax - xMin;
  superXnum = (int)((fullXsize - overlap + montSizeX - overlap - 1) / 
    (montSizeX - overlap));
  halfX = (float)(superXnum * (montSizeX - overlap) + overlap) / 2.f;
  xCen = 0.5f * (xMax + xMin);
  fullYsize = extra + yMax - yMin;
  superYnum = (int)((fullYsize - overlap + montSizeY - overlap - 1) / 
    (montSizeY - overlap));
  halfY = (float)(superYnum * (montSizeY - overlap) + overlap) /2.f;
  yCen = 0.5f * (yMax + yMin);

  // Loop on potential montages, test if any of 4 corners or center are inside polygon
  startNum = mNewItemNum;
  aMat = mShiftManager->StageToCamera(camInd, montP->magIndex);
  aInv = MatInv(aMat);
  for (iy = 0; iy < superYnum; iy++) {
    for (ix = 0; ix < superXnum; ix++) {
      if (IsFrameNeeded(mMontItemCam, montSizeX, montSizeY, (float)overlap, 
        (float)overlap, extra / 2.f, extra / 2.f, xCen - halfX, yCen - halfY, 
        ix, iy, superXnum, superYnum, xMid, yMid)) {
        item = MakeNewItem(superID);
        mNewItemNum--;
        item->mType = ITEM_TYPE_POLYGON;
        item->mColor = DEFAULT_SUPER_COLOR;
        item->mLabel.Format("%d-%d-%d", startNum, ix + 1, iy + 1);

        item->mAcquire = acquire;
        item->mPolygonID = polyID;

        item->mStageX = aInv.xpx * xMid + aInv.xpy * yMid;
        item->mStageY = aInv.ypx * xMid + aInv.ypy * yMid;
        item->mStageZ = (float)stageZ;
        item->mSuperMontX = ix * (montSizeX / montP->binning - mSuperOverlap);
        item->mSuperMontY = iy * (montSizeY / montP->binning - mSuperOverlap);

        // Add the corners
        for (int i = 0; i < 5; i++) {
          cornX = xMid + (float)(0.5 * montSizeX * (1 - 2 * ((i / 2) % 2)));
          cornY = yMid + (float)(0.5 * montSizeY * (1 - 2 * (((i + 1) / 2) % 2)));
          ptX = aInv.xpx * cornX + aInv.xpy * cornY;
          ptY = aInv.ypx * cornX + aInv.ypy * cornY;
          item->AppendPoint(ptX, ptY);
        }
      }
      UpdateListString(mCurrentItem);
    }
  }
  mNewItemNum++;
  SetChanged(true);
  ManageCurrentControls();
  Update();
  Redraw();

  delete mMontItemCam;
}

// Delete a whole group, or the part on one line of collapsed groups, or multiple
// selected points.  "collapsedGroups" is true literally or for multiple selected points
void CNavigatorDlg::DeleteGroup(bool collapsedGroup)
{
  CString message, label, lastlab;
  CMapDrawItem *item;
  bool doSelection = collapsedGroup && mSelectedItems.size() > 1;
  int i, curID, num, start = 0, end = (int)mItemArray.GetSize() - 1;
  if (!SetCurrentItem(true))
    return;
  curID = mItem->mGroupID;
  if (!curID && !doSelection)
    return;
  
  if (doSelection) {
    num = (int)mSelectedItems.size();
    message.Format("Are you sure you want to delete the %d selected items?", num);
  } else if (collapsedGroup) {
    GetCollapsedGroupLimits(mCurListSel, start, end);
    num = end + 1 - start;
    message.Format("Are you sure you want to delete the %d items summarized on the "
      "current line?", num);
  } else {
    num = CountItemsInGroup(curID, label, lastlab, i);
    message.Format("Are you sure you want to delete the %d items in this group?\n\n"
      "(The labels of the first and last items in the group are %s and %s)", num, 
      (LPCTSTR)label, (LPCTSTR)lastlab);
  }
  if ((!doSelection || num > 2) && 
    AfxMessageBox(message, MB_ICONQUESTION | MB_YESNO) != IDYES)
    return;
  for (i = end; i >= start; i--) {
    item = mItemArray[i];
    if ((!doSelection && item->mGroupID == curID) || 
      (doSelection && mSelectedItems.count(i))) {
      if (item->mAcquire) {
        item->mAcquire = false;
        mHelper->EndAcquireOrNewFile(item);
      }
      mItemArray.RemoveAt(i);
      delete item;
    }
  }
  FinishMultipleDeletion();
}

void CNavigatorDlg::FinishMultipleDeletion(void)
{
  int num = B3DMIN(mCurrentItem, (int)mItemArray.GetSize() - 1);
  SetChanged(true);
  if (m_bCollapseGroups)
    MakeListMappings();
  FillListBox();
  mCurListSel = mCurrentItem = num;
  if (m_bCollapseGroups && num >= 0)
    mCurListSel = mItemToList[mCurrentItem];
  m_listViewer.SetCurSel(mCurListSel);
  mSelectedItems.clear();
  ManageCurrentControls();
  Redraw();
}

// Add a circle of the given radius
void CNavigatorDlg::AddCirclePolygon(float radius)
{
  float delX, delY, ptX, ptY, stageX, stageY;
  int numPts = 120;
  double theta;
  ScaleMat aMat, aInv;
  CMapDrawItem *item;
 	EMimageBuffer * imBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (!BufferStageToImage(imBuf, aMat, delX, delY)) {
    AfxMessageBox("The currently active image does not have enough information\n"
      "to convert the marker point to a stage coordinate.", MB_EXCLAME);
    return;
  }
  ptX = imBuf->mUserPtX;
  ptY = imBuf->mUserPtY;
  AdjustMontImagePos(imBuf, ptX, ptY);
  aInv = MatInv(aMat);
  stageX = aInv.xpx * (ptX - delX) + aInv.xpy * (ptY - delY);
  stageY = aInv.ypx * (ptX - delX) + aInv.ypy * (ptY - delY);
  item = MakeNewItem(0);
  SetCurrentStagePos(mCurrentItem);
  item->mStageX = stageX;
  item->mStageY = stageY;
  item->mType = ITEM_TYPE_POLYGON;
  item->mColor = DEFAULT_POLY_COLOR;
  item->mDrawnOnMapID = imBuf->mMapID;
  if (!item->mDrawnOnMapID)
    item->mMapMagInd = imBuf->mMagInd;
  TransferBacklash(imBuf, item);
  for (int i = 0; i <= numPts; i++) {
    theta = i * DTOR * 360 / numPts;
    ptX = stageX + (float)(radius * cos(theta));
    ptY = stageY + (float)(radius * sin(theta));
    item->AppendPoint(ptX, ptY);
  }
  UpdateListString(mCurrentItem);
  SetChanged(true);
  ManageCurrentControls();
  Redraw();
}

// Add regular grid of points based on a template and/or in a polygon
void CNavigatorDlg::AddGridOfPoints(bool likeLast)
{
  CString label;
  CMapDrawItem *item, *poly, *patItems[8], *searchPoly[2];
  int i, j, k, kstart, kend, dir, registration, numInPat = 0;
  int jmn, kmn, imin, start1, start2, end1, end2, num;
  int jdir, jstart, jend, drawnOnID;
  int numPairsOnAxes, numPairs, indPair[15], pairJ[15], pairK[15];
  int groupInd, groupNum[6], grpj, grpk, numInGrp[6], groupTmp[6][6], lineGroup[6][6];
  float vecx[6], vecy[6], length[6], angles[15], stageZ, polyArea[2], itemArea;
  float spacing, xmin, xmax, ymin, ymax, range, delX, delY, xcen, ycen;
  float inSpacing = mLastGridInSpacing > 0. ? mLastGridInSpacing : 1.0f;
  float incStageX1, incStageX2, incStageY1, incStageY2, groupExtent, jSpacing, kSpacing;
  float xcorner, ycorner;
  bool acquire, refresh, awayFromFocus = false;
  double err, errmin, diff, diffmax, axis;
  ScaleMat aMat, aInv;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();

  if (!SetCurrentItem(true))
    return;
  registration = mItem->mRegistration;
  drawnOnID = mItem->mDrawnOnMapID;
  if (!drawnOnID)
    drawnOnID = -mItem->mMapMagInd;
  groupExtent = mHelper->GetDivideIntoGroups() ? mHelper->GetGridGroupSize() : 0.f;
  poly = NULL;
  stageZ = mItem->mStageZ;
  xmin = ymin = 100000.;
  xmax = ymax = -100000.;

  // See if the map the points were drawn on is still loaded and is montage; if so
  // set up to get image coordinates
  if (mItem->IsPoint()) {

    mDrawnOnMontBufInd = FindBufferWithMontMap(drawnOnID);
    if (mDrawnOnMontBufInd >= 0) {
      BufferStageToImage(&mImBufs[mDrawnOnMontBufInd], aMat, delX, delY);
      aInv = MatInv(aMat);

      // This will null out mItem with groups collapsed, so set the item again after
      mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[mDrawnOnMontBufInd], true);
      SetCurrentItem(true);
    }

    for (i = 0; i < mItemArray.GetSize(); i++) {
      item = mItemArray[i];
      if (item->mGroupID == mItem->mGroupID) {
        patItems[numInPat++] = item;
        xmin = B3DMIN(xmin, item->mStageX);
        xmax = B3DMAX(xmax, item->mStageX);
        ymin = B3DMIN(ymin, item->mStageY);
        ymax = B3DMAX(ymax, item->mStageY);
        if (numInPat > 7)
          break;
      }
    }
    xcen = (xmin + xmax) / 2.f;
    ycen = (ymin + ymax) / 2.f;

    if (numInPat == 5 || numInPat == 7 || numInPat == 1) {

      if (numInPat == 5 || numInPat == 7) {
        // Find the corner point
        errmin = 10000.;
        numPairsOnAxes = numInPat == 5 ? 2 : 6;
        for (i = 0; i < numInPat; i++) {

          // For each possible corner point, get all the vectors and lengths to the others
          // then get the angles between all pairs of vectors and sort them
          InterPointVectors(patItems, vecx, vecy, length, i, numInPat);
          numPairs = 0;
          for (j = 0; j < numInPat - 2; j++) {
            for (k = j + 1; k < numInPat - 1; k++) {
              angles[numPairs] = (float)(acos((vecx[j] * vecx[k] + vecy[j] * vecy[k]) /
                (length[j] * length[k])) / DTOR);
              indPair[numPairs] = numPairs;
              pairJ[numPairs] = j;
              pairK[numPairs++] = k;
            }
          }

          rsSortIndexedFloats(angles, indPair, numPairs);

          // Compute an error as sum of the angle for the two or 6 lowest and the sum of
          // the difference from 90 for the rest.
          err = 0.;
          for (j = 0; j < numPairs; j++) {
            if (j < numPairsOnAxes)
              err += angles[indPair[j]];
            else
              err += fabs(90. - angles[indPair[j]]);
          }

          // Put points in groups from these lowest pairs  (ugh!)
          numInGrp[0] = numInGrp[1] = 0;
          for (j = 0; j < numInPat - 1; j++)
            groupNum[j] = -1;
          groupInd = 0;
          for (j = 0; j < numPairsOnAxes; j++) {
            grpj = groupNum[pairJ[indPair[j]]];
            grpk = groupNum[pairK[indPair[j]]];
            if (grpj < 0 && grpk >= 0) {
              groupNum[pairJ[indPair[j]]] = grpk;
              groupTmp[grpk][numInGrp[grpk]++] = pairJ[indPair[j]];
            }
            if (grpj >= 0 && grpk < 0) {
              groupNum[pairK[indPair[j]]] = grpj;
              groupTmp[grpj][numInGrp[grpj]++] = pairK[indPair[j]];
            }
            if (grpj < 0 && grpk < 0) {
              groupNum[pairJ[indPair[j]]] = groupInd;
              groupTmp[groupInd][numInGrp[groupInd]++] = pairJ[indPair[j]];
              groupNum[pairK[indPair[j]]] = groupInd;
              groupTmp[groupInd][numInGrp[groupInd]++] = pairK[indPair[j]];
              groupInd++;
            }
          }

          // Save the groups if the error is a minimum and the groups are correct
          if (err < errmin && numInGrp[0] == numInPat / 2 && numInGrp[0] == numInPat / 2) {
            errmin = err;
            imin = i;
            for (j = 0; j < numInPat / 2; j++) {
              lineGroup[0][j] = groupTmp[0][j];
              lineGroup[1][j] = groupTmp[1][j];
            }
          }
        }
        if (errmin > 9000) {
          AfxMessageBox("Cannot sort points into equal numbers on two lines", MB_EXCLAME);
          mLastGridPatternPoly = -1;
          return;
        }

        // Get the vectors and lengths again and sort the groups by length
        InterPointVectors(patItems, vecx, vecy, length, imin, numInPat);
        rsSortIndexedFloats(length, &lineGroup[0][0], numInPat / 2);
        rsSortIndexedFloats(length, &lineGroup[1][0], numInPat / 2);

        // Get number of intervals in second vector, then if there is a third, get real
        // number of intervals in that vector as its length divided by better estimate
        // of interval from length of second vector
        end1 = mLast5ptEnd1 = B3DNINT(length[lineGroup[0][1]] / length[lineGroup[0][0]]);
        end2 = mLast5ptEnd2 = B3DNINT(length[lineGroup[1][1]] / length[lineGroup[1][0]]);
        if (numInPat == 7) {
          end1 = mLast5ptEnd1 =
            B3DNINT(length[lineGroup[0][2]] / (length[lineGroup[0][1]] / end1));
          end2 = mLast5ptEnd2 =
            B3DNINT(length[lineGroup[1][2]] / (length[lineGroup[1][1]] / end2));
        }

        // Get the increments in X and Y
        mIncX1 = mLast5ptIncX1 = vecx[lineGroup[0][numInPat / 2 - 1]] / end1;
        mIncY1 = mLast5ptIncY1 = vecy[lineGroup[0][numInPat / 2 - 1]] / end1;
        mIncX2 = mLast5ptIncX2 = vecx[lineGroup[1][numInPat / 2 - 1]] / end2;;
        mIncY2 = mLast5ptIncY2 = vecy[lineGroup[1][numInPat / 2 - 1]] / end2;
        mLast5ptRegis = registration;
        mLast5ptOnImage = mDrawnOnMontBufInd >= 0;
        mLast5ptOffsetX = xcen - mItem->mPtX[imin];
        mLast5ptOffsetY = ycen - mItem->mPtY[imin];
        xcorner = mItem->mPtX[imin];
        ycorner = mItem->mPtX[imin];
      } else {

        // One point: reuse last 5-point geometry
        end1 = mLast5ptEnd1;
        mIncX1 = mLast5ptIncX1;
        mIncY1 = mLast5ptIncY1;
        end2 = mLast5ptEnd2;
        mIncX2 = mLast5ptIncX2;
        mIncY2 = mLast5ptIncY2;
        xcorner = xcen;
        ycorner = ycen;
        xcen += mLast5ptOffsetX;
        ycen += mLast5ptOffsetY;
        imin = 0;
      }

      // Common actions for 5-point case
      start1 = start2 = 0;

      if (likeLast) {
        if (mLastGridFillItem) {

          // Search for smallest polygon or map that either center point or current point 
          // is in.  Loop on corner ([0]) then center ([1])
          for (j = 0; j < 2; j++) {
            searchPoly[j] = NULL;
            for (i = 0; i < mItemArray.GetSize(); i++) {
              item = mItemArray[i];
              if ((item->IsPolygon() || item->IsMap()) &&
                item->mRegistration == registration && InsideContour(item->mPtX,
                  item->mPtY, item->mNumPoints, j ? xcen : xcorner, j ? ycen : ycorner)) {
                itemArea = ContourArea(item->mPtX, item->mPtY, item->mNumPoints);
                if (itemArea < 10. * mLastGridPolyArea &&
                  (!searchPoly[j] || itemArea < polyArea[j])) {
                  searchPoly[j] = item;
                  polyArea[j] = itemArea;
                  poly = item;
                }
              }
            }
          }
          if (!poly) {
            AfxMessageBox("The last grid filled a map or polygon, but neither\n"
              "the current point nor the middle of the grid that\n"
              "would extend from the current point as a corner are\n"
              "inside any map or polygon.", MB_EXCLAME);
            mLastGridPatternPoly = -1;
            return;
          }

          // If both points are in different polygons, see if one point is in both 
          // polygons, and if so its smallest polygon is the right one
          if (searchPoly[0] && searchPoly[1] && searchPoly[0] != searchPoly[1]) {
            if (InsideContour(searchPoly[1]->mPtX, searchPoly[1]->mPtY,
              searchPoly[1]->mNumPoints, xcorner, ycorner)) {
              poly = searchPoly[0];
            } else if (InsideContour(searchPoly[0]->mPtX, searchPoly[0]->mPtY,
              searchPoly[0]->mNumPoints, xcen, ycen)) {
              poly = searchPoly[1];
            } else {

              // Otherwise it is ambiguous, have to ask
              if (SEMThreeChoiceBox("It is ambiguous whether you want to fill polygon " +
                searchPoly[0]->mLabel + " or polygon " + searchPoly[1]->mLabel,
                "Use Polygon " + searchPoly[0]->mLabel, "Use Polygon " +
                searchPoly[1]->mLabel, "", MB_YESNO | MB_ICONQUESTION) == IDYES)
                poly = searchPoly[0];
              else
                poly = searchPoly[1];
              if (mDrawnOnMontBufInd >= 0)
                mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[mDrawnOnMontBufInd],
                  true);
              SetCurrentItem(true);
            }
          }
        }

      } else {

        // Ask about polygon and process the entered label
        if (!KGetOneString("Enter label of a polygon or map to fill with this grid, or "
          "nothing for a rectangular grid", label)) {
          mLastGridPatternPoly = -1;
          return;
        }

        // Have to redo this after every message box because the redraw sets new transform!
        if (mDrawnOnMontBufInd >= 0)
          mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[mDrawnOnMontBufInd],
            true);
        SetCurrentItem(true);
        mLastGridFillItem = !label.IsEmpty();
        if (mLastGridFillItem) {
          for (i = 0; i < mItemArray.GetSize(); i++) {
            item = mItemArray[i];
            if (item->mLabel == label) {
              if (item->IsNotPolygon() && item->IsNotMap()) {
                AfxMessageBox("This item is not a polygon or map", MB_EXCLAME);
                mLastGridPatternPoly = -1;
                return;
              }
              poly = item;
              break;
            }
          }
          if (!poly) {
            AfxMessageBox("No item was found with that label", MB_EXCLAME);
            mLastGridPatternPoly = -1;
            return;
          }
          if (poly->mRegistration != registration) {
            AfxMessageBox("This polygon is not at the same registration as the points",
              MB_EXCLAME);
            mLastGridPatternPoly = -1;
            return;
          }
          if (!(InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xcen, ycen) ||
            InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xcorner, ycorner))) {
            AfxMessageBox("Neither the middle of the points in the group\n"
              "nor the corner point is inside the polygon", MB_EXCLAME);
            mLastGridPatternPoly = -1;
            return;
          }
          mLastGridPolyArea = ContourArea(poly->mPtX, poly->mPtY, poly->mNumPoints);
        }
      }

    } else {

      // 3 points: find corner point as one with biggest difference between lengths
      diffmax = -1.;
      for (i = 0; i < 3; i++) {
        InterPointVectors(patItems, vecx, vecy, length, i, 3);
        diff = fabs(length[0] - length[1]);
        if (diff > diffmax) {
          diffmax = diff;
          imin = i;
        }
      }

      // Sort out short from long, get center, numer and increment
      InterPointVectors(patItems, vecx, vecy, length, imin, 3);
      if (length[0] < length[1]) {
        jmn = 0;
        kmn = 1;
      } else {
        jmn = 1;
        kmn = 0;
      }
      end1 = B3DNINT(length[kmn] / length[jmn]);
      mIncX1 = vecx[kmn] / end1;
      mIncY1 = vecy[kmn] / end1;
      start1 = start2 = end2 = 0;
    }
    StageOrImageCoords(patItems[imin], mCenX, mCenY);

    // To evaluate directions and spacing, we need actual stage increments
    GridImageToStage(aInv, delX, delY, mCenX + mIncX1, mCenY + mIncY1, incStageX1,
      incStageY1);
    incStageX1 -= patItems[imin]->mStageX;
    incStageY1 -= patItems[imin]->mStageY;
    spacing = (float)sqrt((double)incStageX1 * incStageX1 + incStageY1 * incStageY1);
    jSpacing = kSpacing = spacing;
    if (numInPat != 3) {
      GridImageToStage(aInv, delX, delY, mCenX + mIncX2, mCenY + mIncY2, incStageX2,
        incStageY2);
      incStageX2 -= patItems[imin]->mStageX;
      incStageY2 -= patItems[imin]->mStageY;
      kSpacing = (float)sqrt((double)incStageX2 * incStageX2 + incStageY2 * incStageY2);
      spacing = 0.5f * (spacing + kSpacing);
    }

  } else {

    // Polygon: get a spacing and set up increments
    // This value signals that the transformation irrelevant, positions become stage pos
    mDrawnOnMontBufInd = -1;
    poly = mItem;
    if (likeLast) {
      inSpacing = mLastGridInSpacing;
    } else {
      if (!KGetOneFloat("Spacing between points in microns:", inSpacing, 2)) {
        mLastGridPatternPoly = -1;
        return;
      }
      if (inSpacing <= 0.05) {
        inSpacing = 1;
        AfxMessageBox("The spacing between points must be positive", MB_EXCLAME);
        mLastGridPatternPoly = -1;
        return;
      }
      mLastGridInSpacing = inSpacing;
    }

    jSpacing = kSpacing = spacing = inSpacing;
    incStageX1 = mIncX1 = spacing;
    incStageY1 = mIncY1 = 0.;
    incStageX2 = mIncX2 = 0.;
    incStageY2 = mIncY2 = spacing;
    mCenX = mItem->mStageX;
    mCenY = mItem->mStageY;
  }

  axis = ldParm[1].axisPosition - ldParm[3].axisPosition;
  refresh = false;
  if (ldParm[1].magIndex && ldParm[2].magIndex && fabs(axis) > 1.e-3 && groupExtent <= 0) {
    if (likeLast && mLastGridAwayFromFocus >= 0) {
      awayFromFocus = mLastGridAwayFromFocus > 0;
    } else {
      awayFromFocus = (SEMThreeChoiceBox("Do you want to lay out points so that movements"
        " from one to the next will be away from the current direction of the Low Dose"
        " Focus area?\n\nIf not, points will be laid out in a zigzag pattern that "
        "minimizes stage"
        " movements.\n\nPress:\n\"Away from Focus Area\" to move in direction away from"
        " focus area,\n\n\"Zigzag\" to move in a zigzag pattern.", "Away from Focus Area",
        "Zigzag", "", MB_YESNO | MB_ICONQUESTION) == IDYES);
      refresh = true;
      mLastGridAwayFromFocus = awayFromFocus ? 1 : 0;
    }
  }

  if (likeLast) {
    acquire = mLastGridSetAcquire;
  } else {
    acquire = (AfxMessageBox("Do you want to turn on Acquire for all these new points?",
      MB_YESNO | MB_ICONQUESTION) == IDYES);
    refresh = true;
  }
  if (refresh) {
    if (mDrawnOnMontBufInd >= 0)
      mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[mDrawnOnMontBufInd], true);
    SetCurrentItem(true);
  }

  // Save flag as valid last grid
  mLastGridPatternPoly = mItem->IsPoint() ? 0 : 1;

  // Now for polygon, find the extent and make generous limits for start and end
  if (poly) {
    xmin = ymin = 1.e10;
    xmax = ymax = -xmin;
    for (i = 0; i < poly->mNumPoints; i++) {
      xmin = B3DMIN(xmin, poly->mPtX[i]);
      ymin = B3DMIN(ymin, poly->mPtY[i]);
      xmax = B3DMAX(xmax, poly->mPtX[i]);
      ymax = B3DMAX(ymax, poly->mPtY[i]);
    }
    range = B3DMAX(xmax - xmin, ymax - ymin);
    num = (int)(range / spacing) + 3;
    start1 = start2 = -num;
    end1 = end2 = num;
    xcen = (xmax + xmin) / 2.f;
    ycen = (ymax + ymin) / 2.f;
  }

  // Set up the point position limits for zigzag or away from focus, and modify if away
  // from focus
  dir = 1;
  kstart = start2;
  kend = end2;
  jstart = start1;
  jend = end1;
  jdir = 1;
  if (awayFromFocus) {
    SetupForAwayFromFocus(incStageX1, incStageY1, incStageX2, incStageY2, jstart, jend,
      jdir, kstart, kend, dir);
  }

  // Call common routine to add points
  MakeGridOrFoundPoints(jstart, jend, jdir, kstart, kend, dir, poly, spacing, jSpacing,
    kSpacing, registration, stageZ, xcen, ycen, delX, delY, acquire, aInv, groupExtent, 
    awayFromFocus, drawnOnID, likeLast);
}

// Actually add the points by the selected method, zigzag, away from focus, or in groups
// jSpacing and kSpacing are the actual spacings in the two directions, "spacing" is
// passed only to BoxedPointInPolygon and would be spacing for adding a grid or diameter
// for adding found holes
int CNavigatorDlg::MakeGridOrFoundPoints(int jstart, int jend, int jdir, int kstart,
  int kend, int dir, CMapDrawItem *poly, float spacing, float jSpacing, float kSpacing,
  int registration, float stageZ, float xcen, float ycen, 
  float delX, float delY, bool acquire, ScaleMat &aInv, float groupExtent, 
  bool awayFromFocus, int drawnOnID, bool likeLast)
{
  int startnum, num, jmin, jmax, kmin, kmax, j, k, jrange, krange, numInJgroup;
  int numInKgroup, curSelBefore, curItemBefore, numListStrBefore;
  int numJgroups, numKgroups, midj, midk, jgroup, kgroup, firstCurItem, kdir, i;
  int groupID = MakeUniqueID();
  int retval = groupID;
  float xx, yy, xmin, xmax, ymin, ymax, xrot, yrot;
  float cospr, sinpr, rotXmin, rotXmax, rotYmin, rotYmax, rotXrange, rotYrange;
  float longRange, shortRange, delLong;
  bool goodDivision, anyPoints;
  double err, errmin, diff, diffmax;
  float delXgroup, delYgroup, distMin;
  int polyRot, rotBest, shortDiv, shortStart, shortEnd, longDiv;
  int maxXind, maxYind, indX, indY, icen;
  ShortVec legalCenters, bestCenters;
  std::vector<ShortVec> kIncluded, jIncluded;
  std::vector<float> legalMaxDists;
  std::set<int> selListBefore;
  CString label;
  CMapDrawItem *item;

  mDeferAddingToViewer = true;

  // No grouping: loop on the point positions and add them
  if (groupExtent <= 0.) {
    startnum = mNewItemNum;
    num = 1;

    // Exchange loop order when going away from focus and swapping axes
    if (awayFromFocus && mSwapAxesAddingPoints) {
      for (k = kstart; dir * (k - kend) <= 0; k += dir)
        for (j = jstart; jdir * (j - jend) <= 0; j += jdir)
          AddPointOnGrid(j, k, poly, registration, groupID, startnum, num, drawnOnID,
            stageZ, spacing, delX, delY, acquire, aInv);
    } else {

      // Or do loops in regular order
      for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
        for (k = kstart; dir * (k - kend) <= 0; k += dir) {
          AddPointOnGrid(j, k, poly, registration, groupID, startnum, num, drawnOnID,
            stageZ, spacing, delX, delY, acquire, aInv);
        }

        // For zigzag, invert the k direction for next iteration
        if (!awayFromFocus) {
          dir = -dir;
          k = kstart;
          kstart = kend;
          kend = k;
        }
      }
    }
    mNewItemNum++;
    FillListBox(false, true);

  } else {

    // Grouping.  First determine extent of polygon and revise start/ends
    if (poly) {
      jmin = jend;
      jmax = jstart;
      kmin = kend;
      kmax = kstart;
      for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
        for (k = kstart; dir * (k - kend) <= 0; k += dir) {
          GridStagePos(j, k, delX, delY, aInv, xx, yy);
          if (!BoxedPointOutsidePolygon(poly, xx, yy, spacing)) {
            kmin = B3DMIN(kmin, k);
            kmax = B3DMAX(kmax, k);
            jmin = B3DMIN(jmin, j);
            jmax = B3DMAX(jmax, j);
          }
        }
      }
      jstart = jmin;
      jend = jmax;
      kstart = kmin;
      kend = kmax;
    }
    jrange = jdir * (jend + 1 - jstart);
    krange = dir * (kend + 1 - kstart);

    // Find how many spacings in each direction give the biggest legal size
    numInJgroup = 1;
    numInKgroup = 1;
    for (j = 1; j < jrange; j++) {

      // For a given step in j, find the number of steps in k that give closest to 45
      // degree line
      kmin = 0;
      errmin = 180.;
      for (k = 1; k < krange; k++) {
        err = fabs(atan(k * kSpacing / (j * jSpacing)) / DTOR - 45.);
        if (err < errmin) {
          errmin = err;
          kmin = k;
        }
      }

      // If point out there is acceptable distance, then record group extents
      if (sqrt(j * jSpacing * j * jSpacing + kmin * kSpacing * kmin * kSpacing) < 
        groupExtent) {
          numInJgroup = 1 + 2 * j;
          numInKgroup = 1 + 2 * kmin;
      } else
        break;
    }

    // Save state before starting so group division can be rejected
    mNumberBeforeAdd = (int)mItemArray.GetSize();
    curSelBefore = mCurListSel;
    curItemBefore = mCurrentItem;
    selListBefore = mSelectedItems;
    numListStrBefore = m_listViewer.GetCount();

    // For non-polygon, just add all the points and be done with it
    if (!poly && !mAddingFoundHoles) {

      // these numbers are a maximum, so we get a number of groups to keep size in range
      numJgroups = (jrange + numInJgroup - 1) / numInJgroup;
      numKgroups = (krange + numInKgroup - 1) / numInKgroup;


      // Loop on groups and get limits for the group
      for (jgroup = 0; jgroup < numJgroups; jgroup++) {
        UtilBalancedGroupLimits(jrange, numJgroups, jgroup, jmin, jmax);
        jmin = jstart + jdir * jmin;
        jmax = jstart + jdir * jmax;
        midj = (jmin + jmax) / 2;
        for (kgroup = 0; kgroup < numKgroups; kgroup++) {
          UtilBalancedGroupLimits(krange, numKgroups, kgroup, kmin, kmax);
          kmin = kstart + dir * kmin;
          kmax = kstart + dir * kmax;
          midk = (kmin + kmax) / 2;

          // Set up to add points, add the center point, then add all the other points
          startnum = mNewItemNum;
          num = 1;
          AddPointOnGrid(midj, midk, poly, registration, groupID, startnum, num, 
            drawnOnID, stageZ, spacing, delX, delY, acquire, aInv);
          firstCurItem = mCurrentItem;
          kdir = dir;
          for (j = jmin; jdir * (j - jmax) <= 0; j += jdir) {
            for (k = kmin; kdir * (k - kmax) <= 0; k += kdir) {
              if (k != midk || j != midj)
                AddPointOnGrid(j, k, poly, registration, groupID, startnum, num, 
                drawnOnID, stageZ, spacing, delX, delY, acquire, aInv);
            }

            // Zigzag
            k = kmin;
            kmin = kmax;
            kmax = k;
            kdir = -kdir;
          }

          // Prepare for next group
          groupID++;
          mNewItemNum++;
        }
      }
      numJgroups *= numKgroups;

    } else {


      // For a polygon, find the rotation that minimizes area
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             errmin = 1.e30;
      for (polyRot = -45; polyRot < 45; polyRot++) {
        cospr = (float)cos(DTOR * polyRot);
        sinpr = (float)sin(DTOR * polyRot);
        xmin = ymin = 1.e10;
        xmax = ymax = -xmin;
        for (i = 0; i < (mAddingFoundHoles ? (int)mHFxCenters->size() :
          poly->mNumPoints); i++) {
          if (mAddingFoundHoles) {
            xx = mHFxCenters->at(i);
            yy = mHFyCenters->at(i);
          } else {
            xx = poly->mPtX[i];
            yy = poly->mPtY[i];
          }
          xrot = cospr * (xx - xcen) - sinpr * (yy - ycen);
          yrot = sinpr * (xx - xcen) + cospr * (yy - ycen);
          ACCUM_MIN(xmin, xrot);
          ACCUM_MIN(ymin, yrot);
          ACCUM_MAX(xmax, xrot);
          ACCUM_MAX(ymax, yrot);
        }

        if ((ymax - ymin) * (xmax - xmin) < errmin) {
          errmin = (ymax - ymin) * (xmax - xmin);
          rotXmin = xmin;
          rotXmax = xmax;
          rotYmin = ymin;
          rotYmax = ymax;
          rotBest = polyRot;
        }
      }
      cospr = (float)cos(DTOR * rotBest);
      sinpr = (float)sin(DTOR * rotBest);

      // Account for the border fraction before dividing up the range
      if (poly || mAddingFoundHoles) {
        xmin = 0.45f * spacing *
          (mAddingFoundHoles ? mParam->holeInPolyBoxFrac : mParam->gridInPolyBoxFrac);
        rotXmin += xmin;
        rotXmax -= xmin;
        rotYmin += xmin;
        rotYmax -= xmin;
        rotXrange = rotXmax - rotXmin;
        rotYrange = rotYmax - rotYmin;
      }
      longRange = B3DMAX(rotXrange, rotYrange);
      shortRange = B3DMIN(rotXrange, rotYrange);
      if (longRange <= 0. || shortRange <= 0) {
        AfxMessageBox((mAddingFoundHoles ? "The set of points" : "The polygon") +
          CString(" is too small to be analyzed for division into groups"),  MB_EXCLAME);
        mLastGridPatternPoly = -1;
        return 0;
      }

      // Loop on increasing divisions of long dimension, and for each one, try one
      // or two of the nearest equal divisions of the short dimension
      goodDivision = false;
      for (longDiv = 1; longDiv < 2. * longRange / (0.5 * (jSpacing + kSpacing)) &&
        !goodDivision; longDiv++) {
        delLong = longRange / longDiv;
        shortEnd = (int)ceil(shortRange / delLong);
        shortStart = B3DMAX(1, shortEnd - 1);
        for (shortDiv = shortStart; shortDiv <= shortEnd && !goodDivision; shortDiv++) {

          // Set up deltas for figuring out group index
          if (rotXrange < rotYrange) {
            delXgroup = rotXrange / shortDiv;
            delYgroup = delLong;
            maxXind = shortDiv - 1;
            maxYind = longDiv - 1;
          } else {
            delYgroup = rotYrange / shortDiv;
            delXgroup = delLong;
            maxYind = shortDiv - 1;
            maxXind = longDiv - 1;
          }

          // Set up vectors
          kIncluded.clear();
          jIncluded.clear();
          jIncluded.resize(longDiv * shortDiv);
          kIncluded.resize(longDiv * shortDiv);
          bestCenters.clear();
          bestCenters.resize(longDiv * shortDiv);

          // Loop on ALL points, with the zigzag pattern
          kdir = dir;
          kmin = kstart;
          kmax = kend;
          anyPoints = false;
          for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
            for (k = kmin; kdir * (k - kmax) <= 0; k += kdir) {
              GridStagePos(j, k, delX, delY, aInv, xx, yy);
              if (BoxedPointOutsidePolygon(poly, xx, yy, spacing))
                continue;

              // Rotate point, get its group index in X and Y, and add to include list
              xrot = cospr * (xx - xcen) - sinpr * (yy - ycen);
              yrot = sinpr * (xx - xcen) + cospr * (yy - ycen);
              indX = (int)((xrot - rotXmin) / delXgroup);
              indY = (int)((yrot - rotYmin) / delYgroup);
              B3DCLAMP(indX, 0, maxXind);
              B3DCLAMP(indY, 0, maxYind);
              i = (maxXind + 1) * indY + indX;
              jIncluded[i].push_back(j);
              kIncluded[i].push_back(k);
              anyPoints = true;
            }

            // Zigzag
            k = kmin;
            kmin = kmax;
            kmax = k;
            kdir = -kdir;
          }
          if (!anyPoints) {
            AfxMessageBox("No points are sufficiently inside the polygon", MB_EXCLAME);
            mLastGridPatternPoly = -1;
            return 0;
          }

          // Loop on the groups
          goodDivision = true;
          for (jgroup = 0; jgroup < longDiv * shortDiv; jgroup++) {

            num = (int)jIncluded[jgroup].size();
            bestCenters[jgroup] = 0;
              
            // Either an empty or a 1-point group is OK
            if (num <= 1)
              continue;

            // Now find points that could be legal center points
            legalCenters.clear();
            legalMaxDists.clear();
            distMin = 1.e30f;
            for (icen = 0; icen < num; icen++) {
              diffmax = 0.;
              for (i = 0; i < num; i++) {
                if (i != icen) {
                  xx = (float)(jIncluded[jgroup][i] - jIncluded[jgroup][icen]) * jSpacing;
                  yy = (float)(kIncluded[jgroup][i] - kIncluded[jgroup][icen]) * kSpacing;
                  diff = (float)sqrt((double)xx * xx + yy * yy);
                  diffmax = B3DMAX(diff, diffmax);
                }
              }
              if (diffmax < groupExtent) {
                legalCenters.push_back(icen);
                legalMaxDists.push_back((float)diffmax);
                distMin = B3DMIN(distMin, (float)diffmax);
              }
            }

            // No legal centers means this division is no good, go on to next
            if (!legalCenters.size()) {
              goodDivision = false;
              break;
            }

            // Now find the best center point
            errmin = 1.e30;
            for (i = 0; i < (int)legalCenters.size(); i++) {
              if (legalMaxDists[i] < 1.05 * distMin) {
                xx = jIncluded[jgroup][legalCenters[i]] - (float)((jstart + jend) / 2.);
                yy = kIncluded[jgroup][legalCenters[i]] - (float)((kstart + kend) / 2.);
                diff = xx * xx + yy * yy;
                if (diff < errmin) {
                  icen = legalCenters[i];
                  errmin = diff;
                }
              }
            }
            bestCenters[jgroup] = icen;
          }
        }
      }

      numJgroups = 0;
      for (jgroup = 0; jgroup < (int)jIncluded.size(); jgroup++) {
        if (!jIncluded[jgroup].size())
          continue;

        // Set up to add points, add the center point, then add all the other points
        startnum = mNewItemNum;
        num = 1;
        midj = jIncluded[jgroup][bestCenters[jgroup]];
        midk = kIncluded[jgroup][bestCenters[jgroup]];
        AddPointOnGrid(midj, midk, poly, registration, groupID, startnum, num, drawnOnID,  
          stageZ, spacing, delX, delY, acquire, aInv);
        firstCurItem = mCurrentItem;
        for (i = 0; i < (int)jIncluded[jgroup].size(); i++) {
          j = jIncluded[jgroup][i];
          k = kIncluded[jgroup][i];
          if (k != midk || j != midj)
            AddPointOnGrid(j, k, poly, registration, groupID, startnum, num, drawnOnID,  
              stageZ, spacing, delX, delY, acquire, aInv);
        }

        // Prepare for next group
        groupID++;
        mNewItemNum++;
        numJgroups++;
      }
    }
    FillListBox(false, true);

    // See if this is a good division
    if (numJgroups && !likeLast && !mMacroProcessor->DoingMacro()) {
     Redraw();
     label.Format("There are %d groups with %.1f points per group\n\n"
       "Do you want to keep this set of groups?", numJgroups, 
       (float)(mItemArray.GetSize() - mNumberBeforeAdd) / numJgroups);
      if (AfxMessageBox(label, MB_QUESTION) == IDNO) {
        for (k = mNumberBeforeAdd; k < mItemArray.GetSize(); k++) {
          item = mItemArray[k];
          delete item;
        }
        mItemArray.RemoveAt(mNumberBeforeAdd, mItemArray.GetSize() - mNumberBeforeAdd);
        mSelectedItems = selListBefore;
        mCurListSel = curSelBefore;
        mCurrentItem = curItemBefore;
        firstCurItem = -1;
        for (k = m_listViewer.GetCount() - 1; k >= numListStrBefore; k--)
          m_listViewer.DeleteString(k);
        m_listViewer.SetCurSel(mCurListSel);
        MakeListMappings();
        retval = 0;
     }
    }
  }
  mDeferAddingToViewer = false;

  // Make the current item be the first in the last group added so it is highlighted
  if (firstCurItem >= 0) {
    mCurrentItem = firstCurItem;
    mSelectedItems.clear();
    mSelectedItems.insert(mCurrentItem);
  }
  ManageCurrentControls();
  Update();
  Redraw();
  return retval;
}

// Add one point on the grid given the index and other parameters
void CNavigatorDlg::AddPointOnGrid(int j, int k, CMapDrawItem *poly, int registration, 
                                   int groupID, int startnum, int &num, int drawnOnID, 
                                   float stageZ, float spacing, float delX, float delY,
                                   bool acquire, ScaleMat &aInv)
{
  CMapDrawItem *item;
  float xx, yy;
  GridStagePos(j, k, delX, delY, aInv, xx, yy);
  if (BoxedPointOutsidePolygon(poly, xx, yy, spacing))
      return;
  item = MakeNewItem(groupID);
  mNewItemNum--;
  item->mLabel.Format("%d-%d", startnum, num++);
  item->mStageX = xx;
  item->mStageY = yy;
  item->mStageZ = stageZ;
  item->AppendPoint(xx, yy);
  item->mRegistration = registration;
  item->mOriginalReg = registration;
  item->mAcquire = acquire;
  if (drawnOnID < 0)
    item->mMapMagInd = -drawnOnID;
  else
    item->mDrawnOnMapID = drawnOnID;
  item->mPieceDrawnOn = mPieceGridPointOn;
  item->mXinPiece = mGridPtXinPiece;
  item->mYinPiece = mGridPtYinPiece;
  if (mDrawnOnMontBufInd >= 0)
    TransferBacklash(&mImBufs[mDrawnOnMontBufInd], item);
  SetChanged(true);
}

// Test whether the given point is outside the polygon or close to being so
// by simply testing if any corners of a box around point are outside it
bool CNavigatorDlg::BoxedPointOutsidePolygon(CMapDrawItem *poly, float xx, float yy, 
                                             float spacing)
{
  float corn = 0.5f  * spacing * 
    (mAddingFoundHoles ? mParam->holeInPolyBoxFrac : mParam->gridInPolyBoxFrac);
  if (mAddingFoundHoles && xx < EXTRA_VALUE_TEST)
    return true;
  return poly && !(InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xx, yy) &&
      InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xx + corn, yy+corn) &&
      InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xx - corn, yy+corn) &&
      InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xx + corn, yy-corn) &&
      InsideContour(poly->mPtX, poly->mPtY, poly->mNumPoints, xx - corn, yy-corn));
}

// Get vectors and lengths between point i and other points
void CNavigatorDlg::InterPointVectors(CMapDrawItem **gitem, float *vecx, float *vecy,
                                      float *length, int i, int num)
{
  int j, k;
  float posXi, posXj, posYi, posYj;
  k = 0;
  StageOrImageCoords(gitem[i], posXi, posYi);
  for (j = 0; j < num; j++) {
    if (i == j)
      continue;
    StageOrImageCoords(gitem[j], posXj, posYj);
    vecx[k] = posXj - posXi;
    vecy[k] = posYj - posYi;
    length[k++] = (float)sqrt((double)(vecx[k] * vecx[k] + vecy[k] * vecy[k]));
  }
}

// Given stage increments in the two directions, set the starts and ends and directions
// for moving away from the focus area
void CNavigatorDlg::SetupForAwayFromFocus(float incStageX1, float incStageY1, 
  float incStageX2, float incStageY2, int &jstart, int &jend, int &jdir, int &kstart, 
  int &kend, int &dir)
{
  double specX, specY, stageX, stageY, dot1, dot2;
  ScaleMat cInv = MatInv(mShiftManager->SpecimenToStage(1., 1.));
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  double axis = ldParm[FOCUS_CONSET].axisPosition - ldParm[RECORD_CONSET].axisPosition;

  // Get specimen then stage coordinates of vector pointing away from LD focus area
  specX = -axis;
  specY = 0.;
  if (mLowDoseDlg->m_bRotateAxis) {
    specX = -axis * cos(DTOR * mLowDoseDlg->m_iAxisAngle);
    specY = -axis * sin(DTOR * mLowDoseDlg->m_iAxisAngle);
  }
  
  stageX = cInv.xpx * specX + cInv.xpy * specY;
  stageY = cInv.ypx * specX + cInv.ypy * specY;

  // If dot product of this vector with the increments is negative, invert a direction
  dot1 = stageX * incStageX1 + stageY * incStageY1;
  if (dot1 < 0) {
    B3DSWAP(jstart, jend, jdir);
    jdir = -1;
  }
  dot2 = stageX * incStageX2 + stageY * incStageY2;
  if (dot2 < 0) {
    B3DSWAP(kstart, kend, dir);
    dir = -1;
  }

  // And if the dot product of the first direction is bigger, change the loop order to
  // go in that direction on the inner loop
  mSwapAxesAddingPoints = jdir * dot1 > dir * dot2;
}

// Compute stage position of one item in grid, or get position of a found hole at that 
// grid position
void CNavigatorDlg::GridStagePos(int j, int k, float delX, float delY, ScaleMat &aInv, 
                                 float &xx, float &yy)
{
  if (mAddingFoundHoles) {
    xx = EXTRA_NO_VALUE;
    yy = EXTRA_NO_VALUE;

    // Look for matching grid index for non-excluded point
    for (int ind = 0; ind < (int)mHFgridXpos->size(); ind++) {
      if (mHFgridXpos->at(ind) == j && mHFgridYpos->at(ind) == k && !mHFexclude->at(ind)){
        xx = mHFxCenters->at(ind);
        yy = mHFyCenters->at(ind);
        if (mDrawnOnMontBufInd >= 0 && mHFpieceOn->size() > 0 && 
          mHFpieceOn->at(ind) >= 0) {
          mPieceGridPointOn = mHFpieceOn->at(ind);
          mGridPtXinPiece = mHFxInPiece->at(ind);
          mGridPtYinPiece = mHFyInPiece->at(ind);
        }
        return;
      }
    }
  } else {
    xx = mCenX + mIncX1 * j + mIncX2 * k;
    yy = mCenY + mIncY1 * j + mIncY2 * k;
    GridImageToStage(aInv, delX, delY, xx, yy, xx, yy);
  }
}

// If item was drawn on a buffer and image coordinate conversion was set up, this will
// give image coordinates, or stage coordinates if not
void CNavigatorDlg::StageOrImageCoords(CMapDrawItem *item, float &posX, float &posY)
{
  if (mDrawnOnMontBufInd < 0) {
    posX = item->mStageX;
    posY = item->mStageY;
  } else {
    mWinApp->mMainView->GetItemImageCoords(&mImBufs[mDrawnOnMontBufInd], item, posX, 
      posY, item->mPieceDrawnOn);
  }
}

void CNavigatorDlg::GridImageToStage(ScaleMat aInv, float delX, float delY, float posX,
                                     float posY, float &stageX, float &stageY)
{
  if (mDrawnOnMontBufInd < 0) {
    stageX = posX;
    stageY = posY;
    mPieceGridPointOn = -1;
    mGridPtXinPiece = mGridPtYinPiece = -1.;
  } else {
    AdjustMontImagePos(&mImBufs[mDrawnOnMontBufInd], posX, posY, &mPieceGridPointOn,
      &mGridPtXinPiece, &mGridPtYinPiece);
    stageX = aInv.xpx * (posX - delX) + aInv.xpy * (posY - delY);
    stageY = aInv.ypx * (posX - delX) + aInv.ypy * (posY - delY);
  }
}

// Add holes found by the HoleFinder using all these passed vectors
// The map ID should be the negative of the mag index if there was no map ID, this
// will make it save the mag in mMapMagInd
int CNavigatorDlg::AddFoundHoles(FloatVec *xCenters, FloatVec *yCenters,
  std::vector<short> *exclude, FloatVec *xInPiece, FloatVec *yInPiece, IntVec *pieceOn, 
  ShortVec *gridXpos, ShortVec *gridYpos, float spacing, float diameter, float incStageX1,
  float incStageY1, float incStageX2, float incStageY2,
  int registration, int mapID, float stageZ, CMapDrawItem *poly, int layoutType)
{
  float xcen = 0., ycen = 0., groupExtent = 0.;
  int jstart = 0, jend, kstart = 0, kend, jdir = 1, dir = 1, ind;
  float delX = 0., delY = 0.;
  ScaleMat aInv;
  mAddingFoundHoles = true;
  mHFxCenters = xCenters;
  mHFyCenters = yCenters;
  mHFexclude = exclude;
  mHFxInPiece = xInPiece;
  mHFyInPiece = yInPiece;
  mHFpieceOn = pieceOn;
  mHFgridXpos = gridXpos;
  mHFgridYpos = gridYpos;
  for (ind = 0; ind < (int)xCenters->size(); ind++) {
    xcen += xCenters->at(ind) / (float)xCenters->size();
    ycen += yCenters->at(ind) / (float)xCenters->size();
  }
  mDrawnOnMontBufInd = FindBufferWithMontMap(mapID);
  if (layoutType == 2)
    groupExtent = mHelper->GetGridGroupSize();
  jend = *std::max_element(gridXpos->begin(), gridXpos->end());
  kend = *std::max_element(gridYpos->begin(), gridYpos->end());
  if (layoutType == 1)
    SetupForAwayFromFocus(incStageX1, incStageY1, incStageX2,
    incStageY2, jstart, jend, jdir, kstart, kend, dir);

  // This returns the first group ID added, which is used to determine if original points
  // should still be displayed
  ind = MakeGridOrFoundPoints(jstart, jend, jdir, kstart, kend, dir, poly, diameter, 
    spacing, spacing, registration, stageZ, xcen, ycen, delX, delY, true, aInv, 
    groupExtent, layoutType == 1, mapID, false);
  mAddingFoundHoles = false;
  return ind;
}


/////////////////////////////////////////////////////////////////////
// TRANSFORM POINTS

#define MSIZE 5
#define XSIZE 5
void CNavigatorDlg::TransformPts() 
{
  CMapDrawItem *item, *item1, *item2;
  int maxRegis = 0, maxPtNum = 0;
  int i, reg, pt, nPairs, ptMax, numDone, ixy, nCol, reg1, reg2, regDrawnOn, curDrawnOn;
  CString report, fitText;
  float x[MAX_REGPT_NUM][XSIZE], sx[MSIZE], xm[MSIZE], sd[MSIZE], b[MSIZE];
  float ss[MSIZE][MSIZE], ssd[MSIZE][MSIZE], d[MSIZE][MSIZE], r[MSIZE][MSIZE];
  int pairPtNum[MAX_REGPT_NUM];
  float dxy[2], a[2][MSIZE], rsq, f, sinth, costh, gmag, xx, yy;
  float rotAngle, smag, stretch, phi, tmp, sign, smagMean, str;
  double xtheta, ytheta, ydtheta, absStr, thetad, alpha;
  ScaleMat aM, strMat, usMat, usInv;
  double devMax, devSum, devX, devY, devPnt, theta;
  int xsiz = XSIZE;
  bool fromImport, magFarOff;
  float meanDevCrit = 5.;     // Criterion for warning of large deviations
  float maxDevCrit = 10.;     // Criterion maximum deviation
  float magCrit = 0.8f;   // Criterion for mag change
  float strCrit = 0.8f;   // Criterion for stretch
  int minNumLinear = 5;   // Number of points required for linear transform

  mWinApp->RestoreViewFocus();

  // Find maximum of registrations and reg point numbers
  for (i = 0; i < mItemArray.GetSize(); i++) {
     item = mItemArray[i];
     if (maxRegis < item->mRegistration)
       maxRegis = item->mRegistration;
     if (maxPtNum < item->mRegPoint)
       maxPtNum = item->mRegPoint;
  }
  mNumSavedRegXforms = 0;
  
  // Loop on all registrations besides the current one
  for (reg = 1; reg <= maxRegis; reg++) {

    // Set up to find transformation from lower to higher registration number
    reg1 = mCurrentRegistration < reg ? mCurrentRegistration : reg;
    reg2 = mCurrentRegistration > reg ? mCurrentRegistration : reg;
    if (reg == mCurrentRegistration)
      continue;
    nPairs = 0;
    numDone = 0;
    regDrawnOn = curDrawnOn = -1;
  
    // Brute force approach - loop on all possible reg pt numbers and find pairs
    for (pt = 1; pt <= maxPtNum; pt++) {
      item1 = NULL;
      item2 = NULL;
      for (i = 0; i < mItemArray.GetSize(); i++) {
        item = mItemArray[i];
        if (item->mRegistration == reg1 && item->mRegPoint == pt) {
          if (item1) {
            report.Format("The items labeled %s and %s\n"
              "have the same registration point number, %d\n\n"
              "You must correct this before transforming points", item->mLabel,
              item1->mLabel, pt);
            AfxMessageBox(report, MB_EXCLAME);
            return;
          }
          item1 = item;
        }
        if (item->mRegistration == reg2 && item->mRegPoint == pt) {
          if (item2) {
            report.Format("The items labeled %s and %s\n"
              "have the same registration point number, %d\n\n"
              "You must correct this before transforming points", item->mLabel,
              item2->mLabel, pt);
            AfxMessageBox(report, MB_EXCLAME);
            return;
          }
          item2 = item;
       }

        // Count up items that need transforming too (1/12/07, count maps too)
        if (item->mRegistration == reg && !item->mRegPoint) 
          numDone++;
      }

      // Found a pair, add to array
      if (item1 && item2) {
        x[nPairs][0] = item1->mStageX;
        x[nPairs][1] = item1->mStageY;
        x[nPairs][3] = item2->mStageX;
        x[nPairs][4] = item2->mStageY;
        pairPtNum[nPairs++] = pt;

        // Keep track if all points were drawn on the same map in each case
        if (item1->mDrawnOnMapID) {
          if (regDrawnOn < 0)
            regDrawnOn = item1->mDrawnOnMapID;
          else if (regDrawnOn > 0 && regDrawnOn != item1->mDrawnOnMapID)
            regDrawnOn = 0;
        }
        if (item2->mDrawnOnMapID) {
          if (curDrawnOn < 0)
            curDrawnOn = item2->mDrawnOnMapID;
          else if (curDrawnOn > 0 && curDrawnOn != item2->mDrawnOnMapID)
            curDrawnOn = 0;
        }
      }
    }

    // Skip if no transformation pairs or nothing to transform
    if (!nPairs || !numDone)
      continue;

    sign = 1.;
    devSum = 0.;
    if (nPairs < 2) {
      fitText = "X/Y shift only";
      aM.xpx = aM.ypy = 1.;
      aM.xpy = aM.ypx = 0.;
      dxy[0] = x[0][3] - x[0][0];
      dxy[1] = x[0][4] - x[0][1];
      devMax  = 0.;
      devPnt = pairPtNum[0];
    } else {

      // Correct for stretch if possible
      strMat = mShiftManager->GetStageStretchXform();
      if (strMat.xpx) {
        usMat = mShiftManager->UnderlyingStretch(strMat, smagMean, thetad, str, alpha);
        usInv = MatInv(usMat);
        dxy[0] = dxy[1] = 0.;
        for (i = 0; i < nPairs; i++) {
          XfApply(usInv, dxy, x[i][0], x[i][1], x[i][0], x[i][1]);
          XfApply(usInv, dxy, x[i][3], x[i][4], x[i][3], x[i][4]);
        }
      }

      if (nPairs > 2) {

        // If more than 2 points, do full linear transform solution
        fitText = "full linear transformation";
        nCol = 3;
          for (ixy = 0; ixy < 2; ixy++) {
            for (i = 0; i < nPairs; i++)
              x[i][2] = x[i][3 + ixy];
            StatMultr(&x[0][0], &xsiz, &nCol, &nPairs, xm, sd, b, &a[ixy][0], &dxy[ixy], 
              &rsq, &f, NULL);
          } 
          aM.xpx = a[0][0];
          aM.xpy = a[0][1];
          aM.ypx = a[1][0];
          aM.ypy = a[1][1];
          SEMTrace('1', "Matrix of linear solution: %.5f %.5f %.5f %.5f",
            aM.xpx, aM.xpy, aM.ypx, aM.ypy);

          // Determine if there is an inversion; if so invert Y of starting points
          xtheta = atan2(aM.ypx, aM.xpx) / DTOR;
          ytheta = atan2(-aM.xpy, aM.ypy) / DTOR;
          ydtheta = UtilGoodAngle(ytheta - xtheta);
          if (fabs(ydtheta) > 90.)
            sign = -1.;
      }
      
      // If 2 or 3 points, replace with rotation/translation solution
      if (nPairs < minNumLinear) {
        fitText = "rotation/translation";
        StatCorrel(&x[0][0], XSIZE, 5, MSIZE, &nPairs, sx, &ss[0][0], &ssd[0][0], &d[0][0],
          &r[0][0], xm, sd, &nCol);

        // 1/12/07: switched to atan2 to get full range of angles.
        theta = atan2(-(sign * ssd[1][3] - ssd[0][4]), (ssd[0][3] + sign * ssd[1][4]));
        sinth = (float)sin(theta);
        costh = (float)cos(theta);
        SEMTrace('1', "Rotation solution angle %.1f  sign %.0f", theta / DTOR, sign);
        if (nPairs > 2) {

          // With 3 points, get a scale factor as well
          fitText = "magnification/rotation/translation";
          gmag = ((ssd[0][3] + sign * ssd[1][4]) * costh - 
            (sign * ssd[1][3] - ssd[0][4]) * sinth) / (ssd[0][0] + ssd[1][1]);
          sinth *= gmag;
          costh *= gmag;
        }
        aM.xpx = costh;
        aM.xpy = -sign * sinth;
        aM.ypx = sinth;
        aM.ypy = sign * costh;
        dxy[0] = xm[3] - xm[0] * costh + sign * xm[1] * sinth;
        dxy[1] = xm[4] - xm[0] * sinth - sign * xm[1] * costh;
      }

      // Get mean and maximum deviation
      devMax = -1.;
      for (i = 0; i < nPairs; i++) {
        XfApply(aM, dxy, x[i][0], x[i][1], xx, yy);
        devX = xx - x[i][3];
        devY = yy - x[i][4];
        devPnt = sqrt(devX * devX + devY * devY);
        devSum += devPnt;
        if (devMax < devPnt) {
          devMax = devPnt;
          ptMax = pairPtNum[i];
        }
      }
      devSum /= nPairs;

      // Adjust for the stretch by multiplying us inverse * matrix * us
      if (strMat.xpx) {
        tmp = dxy[0] * usMat.xpx + dxy[1] * usMat.xpy;
        dxy[1] = dxy[0] * usMat.ypx + dxy[1] * usMat.ypy;
        dxy[0] = tmp;
        aM = MatMul(MatMul(usInv, aM), usMat);
        fitText += " (corrected for stage stretch)";
      }
    }

    // Invert the transform if necessary to get one from other to current reg
    if (mCurrentRegistration < reg) {
      aM = MatInv(aM);
      tmp = -(aM.xpx * dxy[0] + aM.xpy * dxy[1]);
      dxy[1] = -(aM.ypx * dxy[0] + aM.ypy * dxy[1]);
      dxy[0] = tmp;
      i = regDrawnOn;
      regDrawnOn = curDrawnOn;
      curDrawnOn = i;
    }

    amatToRotmagstr(aM.xpx, aM.xpy, aM.ypx, aM.ypy, &rotAngle, 
      &smag, &stretch, &phi);
    absStr = fabs(stretch);
    SEMTrace('1', "Matrix %.5f %.5f %.5f %.5f; rot %.1f mag %.4f str %.4f at %.1f axis",
      aM.xpx, aM.xpy, aM.ypx, aM.ypy, rotAngle, smag, stretch, phi);

    // Test against criteria
    fromImport = RegistrationUseType(reg) == NAVREG_IMPORT;
    magFarOff = smag < magCrit || 1. / smag < magCrit;
    if (devMax > maxDevCrit || devSum > meanDevCrit || (!fromImport && magFarOff) || 
      absStr < strCrit || 1. / absStr < strCrit) {
      report.Format("The fit from registration %d to the current registration is based\n"
        "on %d points with a mean deviation of %.2f um\n"
        "and a maximum deviation of %.2f at point %d\n\n"
        "The transformation rotates by %.1f degrees, scales by %.3f,\n"
        "and stretches by %.3f along the %.1f degree axis.\n\n",
        reg, nPairs, devSum, devMax, ptMax, rotAngle, smag, stretch, phi);
      if (devSum > meanDevCrit)
        report += "The mean deviation is suspiciously high.\n";
      if (devMax > maxDevCrit)
        report += "The maximum deviation is suspiciously high.\n";
      if (magFarOff) {
        if (fromImport)
          report += "The scale change is far from 1., but this could\n"
          "just be from not having a good scale for imported maps.";
        else
          report += "The scale change is suspiciously far from 1.\n";
      }
      if (absStr < strCrit || 1. / absStr < strCrit)
        report += "The stretch factor is suspiciously far from 1.\n";
      report += "\nYou should probably stop and examine the\n"
        "registration point numbering and locations.\n\n"
        "Do you want to stop before transforming items?";
      if (AfxMessageBox(report, MB_YESNO | MB_ICONQUESTION) == IDYES)
        break;
    } else if (magFarOff) {
      report.Format("Warning: the fit from registration %d has a scale change of %.3f,"
        "\r\n far from 1., but this could just be from not having a good scale for"
        " imported maps.", reg, smag);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }

    // See if coming from an import reg and all points are drawn on a pair of maps
    if (fromImport && regDrawnOn > 0 && curDrawnOn > 0 && 
      (item = FindItemWithMapID(regDrawnOn)) != NULL) {
        item->mOldRegToID = item->mRegisteredToID;
        item->mRegisteredToID = curDrawnOn;
    } else {
      regDrawnOn = curDrawnOn = 0;
    }

    // Transform items that are not registration points
    numDone = TransformToCurrentReg(reg, aM, dxy, regDrawnOn, curDrawnOn);
    report.Format("Registration %d: %s found from %d points; \r\n"
      "   deviation mean = %.2f um, max = %.2f at point %d;  %d items transformed",
      reg, fitText, nPairs, devSum, devMax, ptMax, numDone);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  }
  mLastTransformPairs = nPairs;
  mMarkerShiftReg = 0;
  Update();
  Redraw();
}

// Transform all non-registration points at given reg to current reg, return # done
int CNavigatorDlg::TransformToCurrentReg(int reg, ScaleMat aM, float *dxy, int regDrawnOn,
  int curDrawnOn)
{
  CMapDrawItem *item;
  int numDone = 0;
  for (int i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mRegistration == reg && (!item->mRegPoint || item->mType==ITEM_TYPE_MAP)) {
      numDone++;
      TransformOneItem(item, aM, dxy, reg, mCurrentRegistration);
      item->mOldReg = reg;
      item->mOldImported = item->mImported;

      // Set the imported flag negative now.  Also, if this was drawn on the imported map,
      // switch the drawn on ID to the map it was registered to
      if (item->mImported > 0)
        item->mImported = -item->mImported;
      if (item->mDrawnOnMapID == regDrawnOn) {
        item->mOldDrawnOnID = item->mDrawnOnMapID;
        item->mDrawnOnMapID = curDrawnOn;
      }
      UpdateListString(i);
    }
  }

  if (mNumSavedRegXforms < MAX_SAVED_REGXFORM) {
    mMatSaved[mNumSavedRegXforms] = aM;
    mDxySaved[mNumSavedRegXforms][0] = dxy[0];
    mDxySaved[mNumSavedRegXforms][1] = dxy[1];
    mFromRegSaved[mNumSavedRegXforms++] = reg;
    mLastXformToReg = mCurrentRegistration;
  }
  return numDone;
}

// Undo the last transformation
void CNavigatorDlg::UndoTransform(void)
{
  float dxy[2];
  ScaleMat aM;
  CMapDrawItem *item;
  int i, indsave, reg;

  // Loop on different registrations transformed from, get inverse transform
  for (indsave = 0; indsave < mNumSavedRegXforms; indsave++) {
    aM = MatInv(mMatSaved[indsave]);
    dxy[0] = -(aM.xpx * mDxySaved[indsave][0] + aM.xpy * mDxySaved[indsave][1]);
    dxy[1] = -(aM.ypx * mDxySaved[indsave][0] + aM.ypy * mDxySaved[indsave][1]);
    reg = mFromRegSaved[indsave];
    for (i = 0; i < mItemArray.GetSize(); i++) {
      item = mItemArray[i];

      // For matching item, transform and restore import flag and other import items
      if (item->mRegistration == mLastXformToReg && item->mOldReg == reg) {
        TransformOneItem(item, aM, dxy, mLastXformToReg, reg);
        item->mImported = item->mOldImported;
        if (item->mOldDrawnOnID)
          item->mDrawnOnMapID = item->mOldDrawnOnID;
        if (item->mRegisteredToID)
          item->mRegisteredToID = item->mOldRegToID;
        UpdateListString(i);
      }
    }
  }

  // Clear out ALL old registrations and zero the saved count
  for (i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    item->mOldReg = 0;
    item->mOldDrawnOnID = 0;
    item->mOldRegToID = 0;
  }
  mNumSavedRegXforms = 0;
  Update();
  Redraw();
}


// Transform an item from one registration to another, including maps and errors
void CNavigatorDlg::TransformOneItem(CMapDrawItem * item, ScaleMat aM, float * dxy,
                                     int fromReg, int toReg)
{
  int pt;
  float tmp;
  XfApply(aM, dxy, item->mStageX, item->mStageY, item->mStageX, item->mStageY);
  for (pt = 0; pt < item->mNumPoints; pt++)
    XfApply(aM, dxy, item->mPtX[pt], item->mPtY[pt], item->mPtX[pt], item->mPtY[pt]);
  item->mRegistration = toReg;
  SetChanged(true);

  // A map needs to have its transformation matrix changed
  // Premultiply by the inverse of the transformation because that would
  // bring points back to the old coordinates, which can then be multiplied
  // by the map transformation to display at the right places
  if (item->IsMap()) {
    item->mMapScaleMat = MatMul(MatInv(aM), item->mMapScaleMat);

    // Also look for buffers with this map loaded and change their registration
    mHelper->ChangeAllBufferRegistrations(item->mMapID, fromReg, toReg);
  }

  // Also transform a stage error from realigning if present
  if (item->mRealignedID) {
    tmp = aM.xpx * item->mRealignErrX + aM.xpy * item->mRealignErrY;
    item->mRealignErrY = aM.ypx * item->mRealignErrX + aM.ypy * item->mRealignErrY;
    item->mRealignErrX = tmp;
    tmp = aM.xpx * item->mLocalRealiErrX + aM.xpy * item->mLocalRealiErrY;
    item->mLocalRealiErrY = aM.ypx * item->mLocalRealiErrX + aM.ypy * item->mLocalRealiErrY;
    item->mLocalRealiErrX = tmp;
  }
}

void CNavigatorDlg::XfApply(ScaleMat a, float *dxy, float inX, float inY,
                            float &outX, float &outY)
{
  float xTmp = a.xpx * inX + a.xpy * inY + dxy[0];
  outY = a.ypx * inX + a.ypy * inY + dxy[1];
  outX = xTmp;
}

// Transform items after the rotation alignment operation
int CNavigatorDlg::TransformFromRotAlign(int reg, ScaleMat aM, float *dxy)
{
  mNumSavedRegXforms = 0;
  int numDone = TransformToCurrentReg(reg, aM, dxy, 0, 0);
  mMarkerShiftReg = 0;
  Update();
  Redraw();
  return numDone;
}

// Rotate the map in the given buffer
int CNavigatorDlg::RotateMap(EMimageBuffer *imBuf, BOOL redraw)
{
  ScaleMat curMat, rMat, aMat;
  BOOL inverted, newInvert;
  float delX, delY, rotAngle, newAngle;
  float sizingFrac = 1.;
  int err;
  CMapDrawItem *item = FindItemWithMapID(imBuf->mMapID);
  if (!item && !imBuf->mStage2ImMat.xpx)
    return 1;
  if (item && item->mMapTiltAngle > RAW_STAGE_TEST && 
    fabs((double)item->mMapTiltAngle) > 2.5) {
      SEMMessageBox("You cannot rotate a map taken at a tilt higher than 2.5 degrees");
      return 1;
  }

  if (item && item->mImported == 1) {

    // If imported, just get an angle
    rotAngle = 0.;
    if (!KGetOneFloat("Angle to rotate imported map by:", rotAngle, 0))
      return 1;
    inverted = imBuf->mInverted;

  } else {

    // Compute the angle for this image, and new total angle and inversion of imbuf
    curMat = mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), 
      mScope->GetMagIndex());
    BufferStageToImage(imBuf, aMat, delX, delY);
    rotAngle = RotationFromStageMatrices(curMat, aMat, inverted);
  }
  newAngle = (float)UtilGoodAngle((double)(rotAngle + imBuf->mRotAngle));
  newInvert = (imBuf->mInverted ? -1 : 1) * (inverted ? -1 : 1) < 0;

  // Find matrix needed for sizing, and real rotation matrix
  aMat = GetRotationMatrix(newAngle, newInvert);
  rMat = GetRotationMatrix(rotAngle, inverted);
  mWinApp->SetStatusText(SIMPLE_PANE, "ROTATING MAP");
  err = mHelper->TransformBuffer(imBuf, aMat, imBuf->mLoadWidth, 
    imBuf->mLoadHeight, sizingFrac, rMat);
  mWinApp->SetStatusText(SIMPLE_PANE, "");
  if (err) {
    AfxMessageBox("Insufficient memory to rotate the map!", MB_EXCLAME);
    return 2;
  }

  imBuf->mRotAngle = newAngle;
  imBuf->mInverted = newInvert;
  if (redraw) {
    Redraw();
    mWinApp->UpdateBufferWindows();
  }
  return 0;
}

// If there is a suitable transform for calibrating stage stretch, return it
ScaleMat *CNavigatorDlg::XformForStageStretch(bool usingIt)
{
  float smagMean, str;
  double theta, alpha;
  CString mess;
  if (mNumSavedRegXforms != 1)
    return NULL;
  mShiftManager->UnderlyingStretch(mMatSaved[0], smagMean, theta, str, alpha);
  if (fabs(theta /DTOR) - 90. > 20. || mLastTransformPairs < 5)
    return NULL;
  if (usingIt) {
    mess.Format("The last transformation computed:\n"
      "  was based on %d points (at least 6 is recommended);\n"
      "  contained a mean mag change of %.4f (this should be close to 1.000);\n"
      "  and implies an underlying stretch of %.4f along an axis at %.1f degrees.\n\n"
      "Are you sure you want to use this transformation to calibrate stage stretch?",
      mLastTransformPairs, smagMean, 2. * str, alpha / DTOR);
    if (AfxMessageBox(mess, MB_YESNO | MB_ICONQUESTION) == IDYES) {
      mShiftManager->SetStageStretchXform(mMatSaved[0]);
      mWinApp->SetCalibrationsNotSaved(true);
    }
  }
  return &mMatSaved[0];
}


///////////////////////////////////////////////////////////////////////
// MAKING AND LOADING MAPS

// Make a new map image
void CNavigatorDlg::OnNewMap() 
{
  NewMap();
}

// Defaults are unsuitableOK = false, addOrReplaceNote = 0, newNote = NULL
int CNavigatorDlg::NewMap(bool unsuitableOK, int addOrReplaceNote, CString *newNote) 
{
  EMimageBuffer *imBuf, *readBuf;
  EMimageExtra *imExtra;
  float stageX, stageY, imX, imY, delX, delY, stageErrX, stageErrY, localErrX, localErrY;
  BOOL hasStage, cropped = false;
  CMapDrawItem *item, *item2;
  int trimCount, i, setNum, sizeX, sizeY, area, montSect = -1;
  int uncroppedX, uncroppedY;
  float slitIn;
  ScaleMat aInv;
  CString report;
  bool singleBufSaved, singleBufReadIn;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  MontParam *montp = mWinApp->GetMontParam();
  ControlSet *conSets = mWinApp->GetConSets();
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  mWinApp->RestoreViewFocus();

  if (!mWinApp->mStoreMRC) {
    if (unsuitableOK)
      return -1;
    SEMMessageBox("There must be an image file currently open to specify a map",
      MB_EXCLAME);
    return 1;
  }
  imBuf = &mImBufs[(mWinApp->Montaging() && 
    mImBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW) ? 1 : 0];
  cropped = imBuf->GetUncroppedSize(uncroppedX, uncroppedY) && uncroppedX > 0 &&
    uncroppedY > 0;
  if (imBuf->mMapID) {
    if (SEMMessageBox("This image already has a map ID identifying it as a map.\n\n"
      "Are you sure you want to create another map from this image?",
      MB_YESNO, MB_ICONQUESTION) == IDNO)
      return 2;
  }

  singleBufSaved = imBuf->GetSaveCopyFlag() < 0 && (imBuf->mCaptured > 0 || cropped);
  singleBufReadIn = !imBuf->mCaptured && imBuf->mSecNumber >= 0;
  if ((mWinApp->Montaging() || singleBufSaved || singleBufReadIn) && 
    imBuf->mCurStoreChecksum != mWinApp->mStoreMRC->getChecksum()) {
      if (unsuitableOK)
        return -1;
      SEMMessageBox("In order to use this image as a map, it must be\n"
        "either saved into or read in from the current image file.\n"
        "This image is in a different file from the current one.\n\n"
        "Switch to the appropriate file if it is already open,\n"
        "or reopen it if it is not open, and rerun this command",  MB_EXCLAME);
      return 1;
  }

  SEMTrace('n', "NewMap montaging %d  image %d  captured %d  BMO %d secno %d", 
    mWinApp->Montaging() ? 1 : 0, imBuf->mImage, imBuf->mCaptured, BUFFER_MONTAGE_OVERVIEW
    , imBuf->mSecNumber);
  if (mWinApp->Montaging()) {
    
    // Montaged images
    if (!imBuf->mImage || imBuf->mCaptured != BUFFER_MONTAGE_OVERVIEW ||
      imBuf->mSecNumber < 0) {
      if (unsuitableOK)
        return -1;
      SEMMessageBox("In order to use a montaged image as a map,\n"
        "Buffer B must contain a montage overview that was either\n"
        "newly captured into or read in from the current image file.\n\n"
        "This is not the case; no map was created.", MB_EXCLAME);
      return 1;
    }
    hasStage = imBuf->GetStagePosition(stageX, stageY);
    if (!hasStage && !cropped) {
      if (unsuitableOK)
        return -1;
      if (SEMMessageBox("This image was read in from the file.\n"
        "Use it for a map only if the current stage position\n"
        "and the magnification, camera selection, and binning in\n"
        "the montage setup are the same as when the image was taken.\n\n"
        "Are you sure you want to create a map from this image?",
        MB_YESNO | MB_ICONQUESTION) == IDNO)
        return 2;
    }

    // Get the adoc section with montage data, be sure to release mutex if return early
    montSect = mWinApp->mMontageController->AccessMontSectInAdoc(mWinApp->mStoreMRC, 
      imBuf->mSecNumber);
  } else {

    // Regular images
    imBuf = mImBufs;
    if (!imBuf->mImage || (imBuf->mCaptured < 0 && !cropped)) {
      if (unsuitableOK)
        return -1;
      SEMMessageBox("Buffer A does not contain an image that can be saved as a\n"
        "map (one that is newly captured or read in from a file).", MB_EXCLAME);
      return 1;
    }
    hasStage = imBuf->GetStagePosition(stageX, stageY) && imBuf->mCaptured;
    if (!singleBufSaved) {

      // If the image is not already saved, first check for read in and confirm that
      if (singleBufReadIn) {
        if (unsuitableOK)
          return -1;
        report.Format("This image was apparently read in from a file.\n\n"
          "Use it for a map only if it is a full field image read in from the\n"
          " current open file, and if the %scamera selection and\n"
          "magnification are the same as when the image was taken.\n\n"
          "Are you sure you want to create a map from this image?", 
          hasStage ? "" : "stage position, ");
        if (!cropped && SEMMessageBox(report, MB_YESNO, MB_ICONQUESTION) == IDNO)
          return 2;
      } else if (!mBufferManager->IsBufferSavable(imBuf)) {
        
        // Next complain if it can't be saved
        if (unsuitableOK)
          return -1;
        SEMMessageBox("Buffer A cannot be saved in the current file.\n\n"
          "Open or switch to a file that it can be saved in, save it, and try again.",
          MB_EXCLAME);
        return 1;
      } else {

        if (!(mMacroProcessor->DoingMacro() && mHelper->GetAcquiringDual())) {

          //If it can be saved, and they haven't been asked before, ask about auto save
          if (mDocWnd->GetSaveOnNewMap() < 0) {
            i = SEMMessageBox("This image is not saved yet and can be saved \ninto the "
              "current file.\n\nDo you always want to save images automatically into this"
              " file\nwhenever you press New Map and it is the current file?",
              MB_YESNO, MB_ICONQUESTION);
            mDocWnd->SetSaveOnNewMap(i == IDNO ? 0 : 1);
          }

          // Even if they don't want auto save, ask if they want to save this one
          if (!mDocWnd->GetSaveOnNewMap()) {
            if (SEMMessageBox("This image is not saved yet.\n\n"
              "Do you want to save it into the current file?", 
              MB_YESNO, MB_ICONQUESTION) == IDNO)
              return 2;
          }
        }

        // If we got here, they want it saved!
        if (mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC))
          return 1;
      }
    }
  }
  imExtra = imBuf->mImage->GetUserData();

  // Now check that the image isn't a map reduced to bytes
  if (imBuf->mImage->getType() == kUBYTE && mWinApp->mStoreMRC->getMode() != 
    MRC_MODE_BYTE && !cropped) {
      SEMMessageBox("It seems that the image being displayed has been converted to\n"
        "bytes, so its scaling will not be correct when read in as a map.\n"
        "To fix this, turn off \"Convert Maps to Bytes\" in the Navigator\n"
        " menu, load this map item, and make a new map again.\n"
        "Then you should be able to turn on conversion to bytes again.", MB_EXCLAME);
  }

  // Get a new item and save map properties in it
  item = MakeNewItem(0);
  SetCurrentStagePos(mCurrentItem);
  item->mType = ITEM_TYPE_MAP;
  item->mColor = cropped ? NO_REALIGN_COLOR : DEFAULT_MAP_COLOR;
  item->mRawStageX = (float)mLastScopeStageX;
  item->mRawStageY = (float)mLastScopeStageY;

  // Save the stage position in regular and raw spot, regular is adjusted below for IS
  if (hasStage) {
    item->mStageX = stageX;
    item->mStageY = stageY;
    item->mRawStageX = stageX;
    item->mRawStageY = stageY;
  }
  if (!imBuf->GetStageZ(delX))
    item->mStageZ = delX;
  CheckRawStageMatches();
  
  item->mMapBinning = imBuf->mBinning;
  item->mMapCamera = imBuf->mCamera;
  item->mMapMagInd = imBuf->mMagInd;
  if (!imBuf->GetTiltAngle(item->mMapTiltAngle))
    item->mMapTiltAngle = (float)mScope->GetTiltAngle();
  item->mMapMontage = mWinApp->Montaging();
  item->mMapFile = mWinApp->mStoreMRC->getName();
  trimCount = item->mMapFile.GetLength() - (item->mMapFile.ReverseFind('\\') + 1);
  item->mTrimmedName = item->mMapFile.Right(trimCount);
  ComputeStageToImage(imBuf, item->mStageX, item->mStageY, hasStage, item->mMapScaleMat,
    delX, delY);
  item->mMapWidth = cropped ? uncroppedX : imBuf->mImage->getWidth();
  item->mMapHeight = cropped ? uncroppedY : imBuf->mImage->getHeight();
  item->mMapSection = imBuf->mSecNumber;
  item->mNote.Format("Sec %d - %s", item->mMapSection, item->mTrimmedName);
  if (newNote) {
    if (addOrReplaceNote > 0)
      item->mNote = item->mNote + " " + *newNote;
    else if (addOrReplaceNote < 0)
      item->mNote = *newNote + " " + item->mNote;
    else
      item->mNote = *newNote;
  }
  item->mMontBinning = item->mMapBinning;
  setNum = RECORD_CONSET;
  if (item->mMapMontage) {
    item->mMapFramesX = montp->xNframes;
    item->mMapFramesY = montp->yNframes;
    item->mFitToPolygonID = montp->fitToPolygonID;
    if (montSect >= 0) {
      AdocGetTwoIntegers(ADOC_MONT_SECT, montSect, ADOC_MONT_FRAMES, &item->mMapFramesX,
        &item->mMapFramesY);
      AdocGetInteger(ADOC_MONT_SECT, montSect, ADOC_FIT_POLY_ID, &item->mFitToPolygonID);
    }

    // A read-in montage with no MontSection will have this as MONTAGE_CONSET, so this is
    // reliable otherwise
    if (imBuf->mConSetUsed <= RECORD_CONSET)
      setNum = imBuf->mConSetUsed;
    if (imExtra && imExtra->mBinning > 0 && imExtra->mCamera >= 0)
      item->mMontBinning = B3DNINT(imExtra->mBinning * 
      BinDivisorI(&mCamParams[imExtra->mCamera]));
    else
      item->mMontBinning = conSets[setNum].binning;

    // This is the protocol: set information the old way then overlay it with MontSection 
    // value, so it falls back to old way if there is an error
    item->mMontUseStage = montp->moveStage ? 1 : 0;
    if (montSect >= 0)
      AdocGetInteger(ADOC_MONT_SECT, montSect, ADOC_STAGE_MONT, &item->mMontUseStage);
  } else {
    item->mMapFramesX = item->mMapFramesY = 0;
    if (imBuf->mCaptured > 0)
      setNum = imBuf->mConSetUsed;
    else {

      // If read in from file, this convoluted logic checks for match against record,
      // trial, then view sizes; if none found it leaves it as record
      mCamera->AcquiredSize(&conSets[RECORD_CONSET], imBuf->mCamera, sizeX, 
        sizeY);
      if (sizeX != item->mMapWidth || sizeY != item->mMapWidth) {
        mCamera->AcquiredSize(&conSets[2], imBuf->mCamera, sizeX, sizeY);
        if (sizeX == item->mMapWidth && sizeY == item->mMapWidth)
          setNum = 2;
        else {
          mCamera->AcquiredSize(&conSets[0], imBuf->mCamera, sizeX, sizeY);
          if (sizeX == item->mMapWidth && sizeY == item->mMapWidth)
            setNum = 0;
        }
      }
    }
  }
  if (item->mMapMontage && item->mMontUseStage) {
    mWinApp->mMontageController->GetLastBacklash(item->mBacklashX, item->mBacklashY);
    if (montSect >= 0)
      AdocGetTwoFloats(ADOC_MONT_SECT, montSect, ADOC_MONT_BACKLASH, &item->mBacklashX, 
        &item->mBacklashY);
  } else {
    item->mBacklashX = imBuf->mBacklashX;
    item->mBacklashY = imBuf->mBacklashY;
  }

  item->mMapExposure = B3DCHOICE(imExtra && imExtra->mExposure >= 0., imExtra->mExposure,
    conSets[setNum].exposure);
  item->mMapSettling = conSets[setNum].drift;
  item->mShutterMode = conSets[setNum].shuttering;
  item->mK2ReadMode = conSets[setNum].K2ReadMode;
  if (montSect >= 0) {
    AdocGetFloat(ADOC_MONT_SECT, montSect, ADOC_DRIFT, &item->mMapSettling);
    AdocGetTwoIntegers(ADOC_MONT_SECT, montSect, ADOC_CAM_MODES, &item->mShutterMode,
      &item->mK2ReadMode);
  }
  if (!imBuf->GetIntensity(item->mMapIntensity))
    item->mMapIntensity = mScope->GetIntensity();
  if (!imBuf->GetSpotSize(item->mMapSpotSize))
    item->mMapSpotSize = mScope->GetSpotSize();
  item->mMapProbeMode = imBuf->mProbeMode;
  if (imBuf->mLowDoseArea || (item->mMapMontage && 
    IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed)))
    item->mMapLowDoseConSet = imBuf->mConSetUsed;

  // For a LD View image, store the defocus offset, and get the net view IS shift and 
  // convert it to a stage position for use in realign to item
  if (IS_SET_VIEW_OR_SEARCH(item->mMapLowDoseConSet)) {
    area = mCamera->ConSetToLDArea(item->mMapLowDoseConSet);
    item->mDefocusOffset = mScope->GetLDViewDefocus(area);
    mHelper->GetViewOffsets(item, item->mNetViewShiftX,
      item->mNetViewShiftY, item->mViewBeamShiftX, item->mViewBeamShiftY,
      item->mViewBeamTiltX, item->mViewBeamTiltY, area);
    if (montSect >= 0) {
      AdocGetFloat(ADOC_MONT_SECT, montSect, ADOC_FOCUS_OFFSET, &item->mDefocusOffset);
      AdocGetTwoFloats(ADOC_MONT_SECT, montSect, ADOC_NET_VIEW_SHIFT, 
        &item->mNetViewShiftX, &item->mNetViewShiftY);
      AdocGetTwoFloats(ADOC_MONT_SECT, montSect, ADOC_VIEW_BEAM_SHIFT, 
        &item->mViewBeamShiftX, &item->mViewBeamShiftY);
      AdocGetTwoFloats(ADOC_MONT_SECT, montSect, ADOC_VIEW_BEAM_TILT, 
        &item->mViewBeamTiltX, &item->mViewBeamTiltY);
    }
  }

  // For any low dose image, store the alpha value so beam adjustments can be applied in
  // realign to item
  if (imBuf->mLowDoseArea || (item->mMapMontage && 
    IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed))) {
      item->mMapAlpha = mScope->GetAlpha();
      if (montSect >= 0)
        AdocGetInteger(ADOC_MONT_SECT, montSect, ADOC_ALPHA, &item->mMapAlpha);
  }

  if (mCamParams[imBuf->mCamera].GIF || mScope->GetHasOmegaFilter()) {
    item->mMapSlitIn = filtParam->slitIn;
    item->mMapSlitWidth = filtParam->slitWidth;
    if (montSect >= 0) {
      if (!AdocGetTwoFloats(ADOC_MONT_SECT, montSect, ADOC_FILTER, &slitIn,
        &item->mMapSlitWidth))
        item->mMapSlitIn = slitIn != 0.;
    }
  } else {
    item->mMapSlitIn = false;
    item->mMapSlitWidth = 0;
  }

  // Release mutex now that access to section is done
  AdocReleaseMutex();

  // Once there is a transformation, adjusted stage position by any image shift
  // when image was taken (which is already in the transformation)
  if (hasStage)
    ConvertIStoStageIncrement(imBuf->mMagInd, imBuf->mCamera, imBuf->mISX, imBuf->mISY, 
      item->mMapTiltAngle, item->mStageX, item->mStageY);
  SEMTrace('n', "Raw stage %.3f %.3f   adjusted %.3f %.3f", item->mRawStageX, 
    item->mRawStageY, item->mStageX, item->mStageY);

  // Switch image to current registration and give it the map ID, clear out rotation info
  // Loop on image buffer then read buffer, changing read buffer only if it matches
  readBuf = imBuf;
  for (i = 0; i < 2; i++) {
    readBuf->mRegistration = mCurrentRegistration;
    readBuf->mMapID = item->mMapID;
    readBuf->mRotAngle = 0;
    readBuf->mInverted = false;
    readBuf->mLoadWidth = item->mMapWidth;
    readBuf->mLoadHeight = item->mMapHeight;
    readBuf->mUseHeight = (float)item->mMapHeight;
    readBuf->mUseWidth = (float)item->mMapWidth;
    readBuf = &mImBufs[mWinApp->mBufferManager->GetBufToReadInto()];
    if (!(readBuf->mImage && readBuf->mSecNumber == imBuf->mSecNumber && 
      readBuf->mCurStoreChecksum == imBuf->mCurStoreChecksum))
      break;
  }

  // Save scaling values, fixing them up for saved data, and convert to bytes if flag set
  item->mMapMinScale = imBuf->mImageScale->GetMinScale();
  item->mMapMaxScale = imBuf->mImageScale->GetMaxScale();
  if (imBuf->mChangeWhenSaved == SHIFT_UNSIGNED) {
    item->mMapMinScale -= 32768;
    item->mMapMaxScale -= 32768;
  } else if (imBuf->mChangeWhenSaved == SIGNED_SHIFTED) {
    item->mMapMinScale += 32768;
    item->mMapMaxScale += 32768;
  } else if (imBuf->mChangeWhenSaved == DIVIDE_UNSIGNED) {
    item->mMapMinScale /= 2.f;
    item->mMapMaxScale /= 2.f;
  }
  if (mHelper->GetConvertMaps() && !cropped)
    imBuf->ConvertToByte(0., 0.);

  // Make bounding box
  aInv = MatInv(item->mMapScaleMat);
  for (i = 0; i < 5; i++) {
    imX = (float)(((i + 1) / 2 == 1) ? item->mMapWidth : 0.);
    imY = (float)((i / 2 == 1) ? item->mMapHeight : 0.);
    stageX = (aInv.xpx * (imX - delX) + aInv.xpy * (imY - delY));
    stageY = (aInv.ypx * (imX - delX) + aInv.ypy * (imY - delY));
    item->AppendPoint(stageX, stageY);
  }

  // Set the realign error if this point is within the bounding box of the last map
  // aligned to and also, for montage, within 10 microns of aligned point
  if (mHelper->GetLastStageError(stageErrX, stageErrY, localErrX, localErrY) == 
    mCurrentRegistration) {
    i = mHelper->GetLastErrorTarget(imX, imY);
    item2 = FindItemWithMapID(i);
    stageX = item->mStageX - stageErrX;
    stageY = item->mStageY - stageErrY;
    if (item2 && InsideContour(&item2->mPtX[0], &item2->mPtY[0], item2->mNumPoints, 
      stageX, stageY) && (!item2->mMapMontage ||
      (fabs((double)(imX - stageX)) < 10. && 
      fabs((double)(imY - stageY)) < 10.))) {
      item->mRealignedID = i;
      item->mRealignErrX = stageErrX;
      item->mRealignErrY = stageErrY;
      item->mLocalRealiErrX = localErrX;
      item->mLocalRealiErrY = localErrY;
      item->mRealignReg = mCurrentRegistration;
      report.Format("Stage position error of %.2f, %.2f assigned to this map from last "
        "realign to nearby item", stageErrX, stageErrY);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    }
  }
  
  UpdateListString(mCurrentItem);
  SetChanged(true);
  ManageCurrentControls();
  mWinApp->UpdateBufferWindows();
  mWinApp->mActiveView->NewImageScale();
  Redraw();

  // Determine if backlash setting is needed
  delX = mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd) *
    item->mMapBinning;
  delY = (float)sqrt(delX * item->mMapWidth * delX * item->mMapHeight);
  if (item->mBacklashX == 0. && mHelper->GetAutoBacklashNewMap() > mSkipBacklashType && 
    delY >= mHelper->GetAutoBacklashMinField() && 
    item->mMapMagInd >= mScope->GetLowestMModeMagInd()) {
      i = 1;
      if (mHelper->GetAutoBacklashNewMap() == 1) {
        if (SEMMessageBox("Do you want to run the routine to adjust the\nstage position"
          " of this map for backlash?\n\n(Use Navigator - Backlash Settings to set "
          "whether this question is asked)", MB_QUESTION) == IDNO)
          i = 0;
      }
      if (i) {
        AdjustBacklash(mCurrentItem, true);
      }
  }

  // Reset the skip flag always
  mSkipBacklashType = 0;
  return 0;
}

// Make a map from an arbitrary image
void CNavigatorDlg::ImportMap(void)
{
  CFile *file;
  CMapDrawItem *item, *oldItem;
  CString cFilename;
  ScaleMat aInv;
  TiffField *tfield;
  double dval, idval, fracErr;
  CString stringval;
  float minErr, stageX = 0., stageY = 0., imX, imY;
  MontParam montP;
  KImageStore *storeMRC;
  KStoreIMOD *storeIMOD, *store;
  KImageStore *imageStore = NULL;
  char chan;
  CFileStatus status;
  FileOptions fileOpt;
  EMimageBuffer *imbuf[3];
  int i, idMatch, err, curStore = -1, trimCount, overfail, oldCurItem;
  static int lastXformNum = 0, lastRegNum = -1;
  err = mDocWnd->UserOpenOldMrcCFile(&file, cFilename, true);
  if (err == MRC_OPEN_NOTMRC) {

    // For TIFF or other IMOD type file
    storeIMOD = new KStoreIMOD(cFilename);
    if (!(storeIMOD && storeIMOD->FileOK())) {
      AfxMessageBox("Error: File is not an image file of a type that can be "
          "read by the program.", MB_EXCLAME);
      if (storeIMOD)
        delete storeIMOD;
      return;
    }
    imageStore = (KImageStore *)storeIMOD;

    // Try to find which import this matches
    idMatch = -1;
    minErr = 1.;
    for (i = 0; i < mParam->numImportXforms && minErr > 0.; i++) {
      tfield = &mParam->idField[i];

      if (tfield->tag && !mParam->xformID[i].IsEmpty()) {
        if (storeIMOD->getTiffValue(tfield->tag, tfield->type, tfield->tokenNum, dval,
          stringval) > 0) {
          if (tfield->type == TIFFTAG_STRING) {

            // String or integer require exact match
            if (stringval == mParam->xformID[i]) {
              idMatch = i;
              minErr = 0.;
            }
          } else if (tfield->type == TIFFTAG_UINT16 || tfield->type == TIFFTAG_UINT32) {
            if (B3DNINT(dval) == atoi((LPCTSTR)mParam->xformID[i])) {
              idMatch = i;
              minErr = 0.;
            }
          } else {

            // For float/double value, compute fractional error
            idval = atof((LPCTSTR)mParam->xformID[i]);
            fracErr = 0.;
            if (idval != dval)
              fracErr = fabs(dval - idval) / B3DMAX(fabs(dval), fabs(idval));
            if (fracErr < minErr) {
              idMatch = i;
              minErr = (float)fracErr;
            }
          }
        }
      }
    }

    // If there is a match, modify the lastXformNum
    if (idMatch >= 0 && minErr < 0.02)
      lastXformNum = idMatch;
    else
      idMatch = 0;

    // Try to get tag data for X, Y from match or from None entry
    tfield = &mParam->xField[idMatch];
    if (tfield->tag && storeIMOD->getTiffValue(tfield->tag, tfield->type,
      tfield->tokenNum, dval, stringval) > 0)
      stageX = (float)(dval - mParam->importXbase);
    tfield = &mParam->yField[idMatch];
    if (tfield->tag && storeIMOD->getTiffValue(tfield->tag, tfield->type, 
      tfield->tokenNum, dval, stringval) > 0)
      stageY = (float)(dval - mParam->importYbase);
    err = 0;

  } else {

    // For MRC or ADOC files
    if (err && err != MRC_OPEN_ALREADY_OPEN && err != MRC_OPEN_ADOC)
      return;
    curStore = mDocWnd->StoreIndexFromName(cFilename);
    if (curStore >= 0)
      storeMRC = mDocWnd->GetStoreMRC(curStore);
    else if (err == MRC_OPEN_ADOC)
      storeMRC = new KStoreADOC(cFilename);
    else
      storeMRC = new KStoreMRC(file);
    if (!(storeMRC && storeMRC->FileOK())) {
      AfxMessageBox("Some error occurred trying to access this file", MB_EXCLAME);
      err = 1;
    } else if (storeMRC->CheckMontage(&montP)) {
      AfxMessageBox("This file is a montage and cannot be imported at this time.\n\n"
        "Try opening it, reading the image under a new registration, and making a map.", 
        MB_EXCLAME);
      err = 1;
    } else {
      err = 0;
      imageStore = (KImageStore *)storeMRC;
    }
  }

  if (!err) {

    CNavImportDlg importDlg;
    importDlg.mNumSections = imageStore->getDepth();
    importDlg.m_iSection = 0;
    importDlg.m_fStageX = stageX;
    importDlg.m_fStageY = stageY;
    importDlg.mXformNum = lastXformNum;
    importDlg.mOverlayOK = SetCurrentItem();
    if (importDlg.mOverlayOK) 
      importDlg.mOverlayOK = mItem->mImported == 1 && 
      imageStore->getMode() != MRC_MODE_RGB && mItem->mMapWidth == imageStore->getWidth()
      && mItem->mMapHeight == imageStore->getHeight();
    oldItem = mItem;
    importDlg.m_strOverlay = mParam->overlayChannels;

    // Make sure last registration is good or find a free one
    if (lastRegNum > 0 && RegistrationUseType(lastRegNum) != NAVREG_REGULAR)
      importDlg.mRegNum = lastRegNum;
    else {

      // Look for free number but default to last one even if not free!
      for (err = 1; err < MAX_CURRENT_REG; err++)
        if (RegistrationUseType(err) != NAVREG_REGULAR && err != mCurrentRegistration)
          break;
      importDlg.mRegNum = err;
    }
    if (importDlg.DoModal() == IDOK) {
      lastXformNum = importDlg.mXformNum;
      lastRegNum = importDlg.mRegNum;
      mParam->overlayChannels = importDlg.m_strOverlay;

      // Get a new item and save map properties in it
      oldCurItem = mCurrentItem;
      item = MakeNewItem(0);
      item->mType = ITEM_TYPE_MAP;
      item->mColor = DEFAULT_MAP_COLOR;
      item->mStageX = importDlg.m_fStageX;
      item->mStageY = importDlg.m_fStageY;
      item->mRegistration = lastRegNum;
      item->mOriginalReg = lastRegNum;
      item->mImported = 1;
      item->mImageType = imageStore->getStoreType();

      item->mMapBinning = 1;
      item->mMapCamera = mWinApp->GetCurrentCamera();
      item->mMapMagInd = 0;
      item->mMapMontage = false;
      item->mMapFile = cFilename;
      trimCount = item->mMapFile.GetLength() - (item->mMapFile.ReverseFind('\\') + 1);
      item->mTrimmedName = item->mMapFile.Right(trimCount);
      item->mMapScaleMat = mParam->importXform[lastXformNum];
      item->mMapWidth = imageStore->getWidth();
      item->mMapHeight = imageStore->getHeight();
      item->mMapSection = importDlg.m_iSection;
      item->mNote.Format("Sec %d - %s", item->mMapSection, item->mTrimmedName);
      item->mMontBinning = 1;
      item->mMapFramesX = item->mMapFramesY = 0;
      item->mMapExposure = 0.;
      item->mMapSettling = 0.;
      item->mShutterMode = 0;
      item->mK2ReadMode = LINEAR_MODE;
      item->mMapIntensity = 0.;
      item->mMapSpotSize = 0;
      item->mMapSlitIn = false;
      item->mMapSlitWidth = 0;


      item->mMapMinScale = 0;
      item->mMapMaxScale = 0;

      // Make bounding box
      aInv = MatInv(item->mMapScaleMat);
      for (i = 0; i < 5; i++) {
        imX = (float)(0.5 * item->mMapWidth * (1 - 2 * ((i / 2) % 2)));
        imY = (float)(0.5 * item->mMapHeight * (1 - 2 * (((i + 1) / 2) % 2)));
        stageX = item->mStageX + aInv.xpx * imX + aInv.xpy * imY;
        stageY = item->mStageY + aInv.ypx * imX + aInv.ypy * imY;
        item->AppendPoint(stageX, stageY);
      }
      UpdateListString(mCurrentItem);
      
      // If an overlay is requested, need to load maps synchronously
      if (importDlg.m_bOverlay) {
        overfail = DoLoadMap(true, NULL, -1) ? -1 : 0;
        if (!overfail) {
          mBufferManager->CopyImageBuffer(mBufferManager->GetBufToReadInto(), 0);
          mCurrentItem = oldCurItem;
          overfail = DoLoadMap(true, NULL, -1) ? -2 : 0;
        }

        // Then get the overlay image into A
        if (!overfail) {
          for (i = 0; i < 3; i++) {
            chan = importDlg.m_strOverlay.GetAt(i);
            if (chan == '1')
              imbuf[i] = &mImBufs[mBufferManager->GetBufToReadInto()];
            else if (chan == '2')
              imbuf[i] = &mImBufs[0];
            else
              imbuf[i] = NULL;
          }
          overfail = mWinApp->mProcessImage->OverlayImages(imbuf[0], imbuf[1], imbuf[2])
            ? 0 : -3;
        }

        // Now get the filename and save the image
        if (!overfail) {
          for (i = 1; i < 1000; i++) {
            trimCount = oldItem->mMapFile.ReverseFind('.');
            if (trimCount < 0)
              trimCount = oldItem->mMapFile.GetLength();
            cFilename.Format("%s-ovla%d.tif", (LPCTSTR)oldItem->mMapFile.Left(trimCount),
              i);
            if (!CFile::GetStatus((LPCTSTR)cFilename, status))
              break;
          }
          fileOpt = *(mDocWnd->GetFileOpt());
          fileOpt.fileType = STORE_TYPE_TIFF;
          store = new KStoreIMOD(cFilename, fileOpt);
          overfail = store->FileOK() ? 0 : -4;
          if (!overfail)
            overfail = store->WriteSection(mImBufs->mImage);
          delete store;
        }

        // All good, make the map item
        if (!overfail) {
          item = MakeNewItem(0);
          stringval = item->mLabel;
          *item = *oldItem;
          item->mLabel = stringval;
          item->mNumPoints = 0;
          item->mMaxPoints = 0;
          item->mPtX = item->mPtY = NULL;
          item->mMapFile = cFilename;
          trimCount = item->mMapFile.GetLength() - (item->mMapFile.ReverseFind('\\') + 1);
          item->mTrimmedName = item->mMapFile.Right(trimCount);
          mImBufs->mMapID = item->mMapID;
          item->mMapSection = 0;
          item->mNote.Format("Overlay - %s", item->mTrimmedName);
          item->mImageType = STORE_TYPE_IMOD;
          for (i = 0; i < oldItem->mNumPoints; i++)
            item->AppendPoint(oldItem->mPtX[i], oldItem->mPtY[i]);
          UpdateListString(mCurrentItem);
        } else {
          stringval.Format("Could not make overlay from the two imported maps; "
            "error code %d", overfail);
          AfxMessageBox(stringval, MB_EXCLAME);
        }
      }

      SetChanged(true);
      ManageCurrentControls();

    }
  }
  if (imageStore && imageStore->getStoreType() == STORE_TYPE_IMOD) 
    delete storeIMOD;
  else if (storeMRC && curStore < 0)
    delete storeMRC;
}

// Call Helper to make a dual map
void CNavigatorDlg::OnButDualMap()
{
  mWinApp->RestoreViewFocus();
  DoMakeDualMap();
}

int CNavigatorDlg::DoMakeDualMap()
{
  CMapDrawItem *item = FindItemWithMapID(mDualMapID, true);
  return mHelper->MakeDualMap(item);
}

// Load a map image
void CNavigatorDlg::OnLoadMap() 
{
  if (!SetCurrentItem()) {
    mItem = GetSingleSelectedItem();
    if (!mItem)
      return;
  }
  if (mItem->IsPoint()) {
    mHelper->LoadPieceContainingPoint(mItem, mFoundItem);
    AddFocusAreaPoint(false);
  } else
    DoLoadMap(false, NULL, -1);
}

int CNavigatorDlg::DoLoadMap(bool synchronous, CMapDrawItem *item, int bufToReadInto)
{
  int err;
  static MontParam mntp;
  MontParam *montP = &mntp;
  MontParam *masterMont = mWinApp->GetMontParam();

  mWinApp->RestoreViewFocus(mWinApp->GetNavTableHadFocus());
  if (item)
    mItem = item;
  else if (!SetCurrentItem())
    return 1;
  mOriginalStore = mDocWnd->GetCurrentStore();
  mLoadItem = mItem;

  // Get the pointers to file, open if necessary
  err = AccessMapFile(mItem, mLoadStoreMRC, mCurStore, montP, mUseWidth, mUseHeight);
  if (err) {
    if (err == 1) 
      SEMMessageBox("The map file was not found (or just could not be opened) either\n"
        "under its original full path name or in the current directory.", MB_EXCLAME);
    else
      SEMMessageBox("The map file was found but no longer\nappears to be "
        "a valid MRC file or montage file.", MB_EXCLAME);
    return 1;
  }

  mReadingOther = mCurStore < 0;
  if (mCurStore >= 0 && mCurStore != mOriginalStore)
    mDocWnd->SetCurrentStore(mCurStore);

  // Save and set overview binning to 1 if selected, then
  mOverviewBinSave = masterMont->overviewBinning;
  if (mHelper->GetLoadMapsUnbinned())
    masterMont->overviewBinning = 1;

  if (mHelper->GetConvertMaps()) {
    masterMont->byteMinScale = mItem->mMapMinScale;
    masterMont->byteMaxScale = mItem->mMapMaxScale;
  }

  // Read in the appropriate way for the file
  mLoadingMap = true;
  mBufToLoadInto = bufToReadInto >= 0 ? bufToReadInto :mBufferManager->GetBufToReadInto();
  if (mItem->mMapMontage && !mReadingOther)
    err = mWinApp->mMontageController->ReadMontage(mItem->mMapSection, NULL, NULL, false,
      synchronous, bufToReadInto);
  else
    err = mBufferManager->ReadFromFile(mLoadStoreMRC, mItem->mMapSection, bufToReadInto,
      false, synchronous);

  if (err && err != READ_MONTAGE_OK) {
    SEMMessageBox("Error reading image from file.", MB_EXCLAME);
    LoadMapCleanup();
    return 1;
  }
  if (synchronous)
    FinishLoadMap();
  else
    mWinApp->AddIdleTask(NULL, TASK_LOAD_MAP, 0, 0);
  return 0;
}

// Finish up after loading a map
void CNavigatorDlg::FinishLoadMap(void)
{
  EMimageBuffer *imBuf = mImBufs + (mLoadItem->mMapMontage ? 1 : mBufToLoadInto);
  MontParam *masterMont = mWinApp->GetMontParam();
  CameraParameters *camP = &mCamParams[B3DMAX(0, mLoadItem->mMapCamera)];
  EMimageExtra *extra1;
  int uncroppedX, uncroppedY;
  float stageX, stageY, angle;
  bool noStage, noTilt, cropped;

  LoadMapCleanup();
  if (mLoadItem->mMapMontage && mWinApp->mMontageController->GetLastFailed()) {
    mWinApp->UpdateBufferWindows(); 
    return;
  }
  if (mReadingOther)
    imBuf->mSecNumber = -mLoadItem->mMapSection - 1;

  // Set the registration and map ID, clear out rotation info
  imBuf->mRegistration = mLoadItem->mRegistration;
  imBuf->mMapID = mLoadItem->mMapID;
  imBuf->mRotAngle = 0.;
  imBuf->mInverted = false;
  imBuf->mUseWidth = mUseWidth;
  imBuf->mUseHeight = mUseHeight;
  imBuf->mLoadWidth = imBuf->mImage->getWidth();
  imBuf->mLoadHeight = imBuf->mImage->getHeight();
  imBuf->mCamera = mLoadItem->mMapCamera;
  imBuf->mMagInd = mLoadItem->mMapMagInd;
  if (mLoadItem->mMapLowDoseConSet >= 0) {
    imBuf->mLowDoseArea = true;
    imBuf->mConSetUsed = mLoadItem->mMapLowDoseConSet;
  }

  // Attach the map stage position to the image if it is missing
  noStage = !imBuf->GetStagePosition(stageX, stageY);
  noTilt = !imBuf->GetTiltAngle(angle) && mLoadItem->mMapTiltAngle > RAW_STAGE_TEST;
  if (noStage || noTilt) {
    extra1 = (EMimageExtra *)imBuf->mImage->GetUserData();
    if (!extra1) {
      extra1 = new EMimageExtra;
      imBuf->mImage->SetUserData(extra1);
    }
    if (noStage) {
      extra1->mStageX = mLoadItem->mStageX;
      extra1->mStageY = mLoadItem->mStageY;
    }
    if (noTilt)
      extra1->m_fTilt = mLoadItem->mMapTiltAngle;
    extra1->ValuesIntoShorts();
  }

  // Set effective binning for imported map
  if (mLoadItem->mImported)
    imBuf->mEffectiveBin = B3DMIN(camP->sizeX / mUseWidth, camP->sizeY / mUseHeight);

  // Convert single frame map to bytes now if flag set
  cropped = mImBufs[mBufToLoadInto].GetUncroppedSize(uncroppedX, uncroppedY) &&
    uncroppedX > 0;
  if (mHelper->GetConvertMaps() && !cropped && !mLoadItem->mMapMontage && 
    mLoadItem->mMapMinScale != mLoadItem->mMapMaxScale)
    mImBufs[mBufToLoadInto].ConvertToByte(mLoadItem->mMapMinScale, mLoadItem->mMapMaxScale);
  
  // Copy montage to read buffer, get loaded size, rotate if requested, and display
  // 4/20/09: montage has already been copied there, but still work on buffer B first and
  // then copy it again so that both buffers have the map data
  if (mLoadItem->mMapMontage)
    mBufferManager->CopyImageBuffer(1, mBufToLoadInto);
  if (mLoadItem->mRotOnLoad)
    RotateMap(&mImBufs[mBufToLoadInto], false);
  mWinApp->SetCurrentBuffer(mBufToLoadInto);

  Redraw();
}

// Clean up from trying to load a map
void CNavigatorDlg::LoadMapCleanup(void)
{
  MontParam *masterMont = mWinApp->GetMontParam();
  masterMont->overviewBinning = mOverviewBinSave;
  masterMont->byteMinScale = 0.;
  masterMont->byteMaxScale = 0.;
  if (mReadingOther) {
    if (mLoadStoreMRC->getStoreType() == STORE_TYPE_MRC)
      delete (KStoreMRC *)mLoadStoreMRC;
    else if (mLoadStoreMRC->getStoreType() == STORE_TYPE_ADOC)
      delete (KStoreADOC *)mLoadStoreMRC;
    else
      delete (KStoreIMOD *)mLoadStoreMRC;
  } else if (mCurStore != mOriginalStore)
    mDocWnd->SetCurrentStore(mOriginalStore);
  mLoadingMap = false;
}

// Set up access to the map file for the given item: return a KImageStore, the store
// number if it is a current store, montage params, and the useWidth and useHeight values
// Supply a pointer to a valid montage param structure, it may be replaced or used
int CNavigatorDlg::AccessMapFile(CMapDrawItem *item, KImageStore *&imageStore, 
  int &curStore, MontParam *&montP, float &useWidth, float &useHeight, bool readWrite)
{
  CFile *file;
  CameraParameters *cam;
  int binInd;
  bool useNavPath = true;
  KImageStore *storeMRC;
  KStoreIMOD *storeIMOD;
  CFileStatus status;
  CString fullpath, navPathName, str;

  // If opening read-only, set the share permission to DenyNone so it will not try to
  // change the file's read/write status, which would be locked if a process has it open 
  // read/write
  UINT openFlags = readWrite ? (CFile::modeReadWrite | CFile::shareDenyWrite) : 
    (CFile::modeRead | CFile::shareDenyNone);

  // Get the path/name of the file if it is located in the same directory as the nav file,
  // or use the current dir if there is no file
  GetCurrentNavDir(navPathName);
  navPathName += item->mTrimmedName;

  curStore = -1;
  if (item->mImageType == STORE_TYPE_MRC || item->mImageType == STORE_TYPE_ADOC) {
    curStore = mDocWnd->StoreIndexFromName(item->mMapFile);

    // If this fails, check with the trimmed name and the nav/current directory, because
    // it is not possible to open a file twice and the open below would fail
    if (curStore < 0)
        curStore = mDocWnd->StoreIndexFromName(navPathName);

    if (curStore >= 0) {
      storeMRC = mDocWnd->GetStoreMRC(curStore);
      if (item->mMapMontage) {
        if (!mDocWnd->StoreIsMontage(curStore))
          return 3;
        if (curStore == mDocWnd->GetCurrentStore())
          montP = mWinApp->GetMontParam();
        else
          montP = mDocWnd->GetStoreMontParam(curStore);
      }
    } else {

      // Otherwise try to open file under full name then under trimmed name
      // It used to be opened read-only, until dual map needed write
      // Only try if file exists to avoid crash under wine
      if (CFile::GetStatus((LPCTSTR)item->mMapFile, status)) {
        try {
          file = new CFile(item->mMapFile, openFlags);
          useNavPath = false;
        }
        catch (CFileException *err1) {
          err1->Delete();
        }
      }
      if (useNavPath) {
        try {
          file = new CFile(navPathName, openFlags);
        }
        catch (CFileException *err2) {
          SEMTrace('n', "Exception, cause %d, errcode %d, opening file %s", err2->m_cause,
            err2->m_lOsError, (LPCTSTR)err2->m_strFileName);
          err2->Delete();
          return 1;
        }
      }

      // Check that it is still usable then use it
      if (KStoreMRC::IsMRC(file)) {
        storeMRC = new KStoreMRC(file);
      } else {
        fullpath = file->GetFilePath();
        delete file;
        str = KStoreADOC::IsADOC(fullpath);
        if (!str.IsEmpty()) {

        // Removed async saving check put in for 3.5, before adoc mutex was added
          storeMRC = new KStoreADOC(fullpath);
        } else {
          if (!mWinApp->mDocWnd->GetHDFsupported() || !iiTestIfHDF((char *)(LPCTSTR)fullpath))
            return 2;
          storeMRC = new KStoreIMOD(fullpath);
          if (!storeMRC->FileOK()) {
            delete storeMRC;
            return 2;
          }
        }
      }

      // Get montage params or return error if no good.  Copy master set to fill in all
      if (item->mMapMontage) {
        *montP = *(mWinApp->GetMontParam());
        if (storeMRC->CheckMontage(montP) <= 0) {
          delete storeMRC;
          return 3;
        }

        // Figure out the smallest binning that fits frame; fall back to 1
        cam = &mCamParams[item->mMapCamera];
        montP->binning = 1;
        for (binInd = cam->numBinnings - 1; binInd >= 0; binInd--) {
          if (montP->xFrame * cam->binnings[binInd] <= cam->sizeX && 
            montP->yFrame * cam->binnings[binInd] <= cam->sizeY) {
              montP->binning = cam->binnings[binInd];
              break;
            }
        }
      }
    }
    imageStore = storeMRC;

  } else {

    // For non-MRC, try to open under original and trimmed name
    storeIMOD = new KStoreIMOD(item->mMapFile);
    if (!(storeIMOD && storeIMOD->FileOK())) {
      if (storeIMOD)
        delete storeIMOD;
      storeIMOD = new KStoreIMOD(navPathName);
      if (!(storeIMOD && storeIMOD->FileOK())) {
        if (storeIMOD)
          delete storeIMOD;
        return 1;
      }
    }
    imageStore = (KImageStore *)storeIMOD;
  }

  // Adjust image size for montage based on the number of frames will actually be
  // created when the image is read from file
  useWidth = (float)item->mMapWidth;
  useHeight = (float)item->mMapHeight;
  if (item->mMapMontage && item->mMapFramesX && item->mMapFramesY) {
    useWidth = (float)(item->mMapWidth * (montP->xNframes * montP->xFrame - 
      (montP->xNframes - 1) * montP->xOverlap) / (item->mMapFramesX * montP->xFrame - 
      (item->mMapFramesX - 1) * montP->xOverlap));
    useHeight = (float)(item->mMapHeight * (montP->yNframes * montP->yFrame - 
      (montP->yNframes - 1) * montP->yOverlap) / (item->mMapFramesY * montP->yFrame - 
      (item->mMapFramesY - 1) * montP->yOverlap));
  }
  if (item->mMapMontage && item->mMontUseStage >= 0)
    montP->moveStage = item->mMontUseStage > 0;
  
  return 0;
}


/////////////////////////////////////////////////////////////////////
// NAVIGATOR FILE I/O

// Calls to do the save and save as, with potential error returns
int CNavigatorDlg::DoSave()
{
  if (!mItemArray.GetSize())
    return 0;
  if (mNavFilename.IsEmpty())
    return DoSaveAs();
  mDocWnd->ManageBackupFile(mNavFilename, mNavBackedUp);
  OpenAndWriteFile(false);
  return 0;
}

int CNavigatorDlg::DoSaveAs()
{
  if (!mItemArray.GetSize())
    return 0;
  if (GetNavFilename(false, OFN_HIDEREADONLY, false)) //| OFN_OVERWRITEPROMPT)) not needed
    return 1;

  // Any time a new name is gotten, make backup file unconditionally
  mNavBackedUp = false;
  mDocWnd->ManageBackupFile(mNavFilename, mNavBackedUp);
  OpenAndWriteFile(false);
  RemoveAutosaveFile();
  return 0;
}

// Check if needs saving and ask user before proceding
int CNavigatorDlg::AskIfSave(CString reason)
{
  if (!mChanged && mParam->autosaveFile.IsEmpty())
    return 0;

  UINT action = MessageBox("Save Navigator entries before " + reason, NULL, 
    MB_YESNOCANCEL | MB_ICONQUESTION);
  if (action == IDCANCEL)
    return 1;
  else if (action == IDNO) {
    RemoveAutosaveFile();
    return 0;
  }
  return  DoSave();
}

// Do autosave if file is open, and things have changed
void CNavigatorDlg::AutoSave()
{
  if (!mChanged)
    return;
  if (!mNavFilename.IsEmpty())
    DoSave();
  else {
    if (mParam->autosaveFile.IsEmpty()) {
      CString strCwd = mDocWnd->GetOriginalCwd();
      CTime ctdt = CTime::GetCurrentTime();
      mParam->autosaveFile.Format("AUTOSAVE%02d%02d%02d%02d.nav", ctdt.GetMonth(), 
        ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute());
      if (!strCwd.IsEmpty()) {

        // append \ if necessary
        if (strCwd.GetAt(strCwd.GetLength() - 1) != '\\')
          strCwd += '\\';
        mParam->autosaveFile = strCwd + mParam->autosaveFile;
      }
      mDocWnd->SetShortTermNotSaved();
      mDocWnd->SaveShortTermCal();
    }
    OpenAndWriteFile(true);
  }
}

// Remove an autosave file if one is current; clear out name
void CNavigatorDlg::RemoveAutosaveFile(void)
{
  CFileStatus status;
  if (mParam->autosaveFile.IsEmpty() || !CFile::GetStatus((LPCTSTR)mParam->autosaveFile, 
    status))
    return;
  try {
    CFile::Remove(mParam->autosaveFile);
  }
  catch (CFileException *err) {
    err->Delete();
  }
  mParam->autosaveFile = "";
  mDocWnd->SetShortTermNotSaved();
  mDocWnd->SaveShortTermCal();
}

// Finally, routine to write the file
/*
 * HOW TO ADD ELEMENTS:
 * 1) If it is a map, it goes before Points; if it is not, it goes before Mapfile
 * 2) Increment file version
 * 3) Add entries to title row
 * 4) Add entries to relevant format statement and variable list
 * 5) If the elements are added to a map, add another entry like
 *    if (version > 140)
 *      mapSkip += 1;
 *    AND add the same number of tabs to the nonmap output
 * 6) If it is a map, add the read conditionally to the end of the map section
 *    Otherwise add the read conditionally before the map section
 * 7) When adding future items, add NextTabField(str, index); for each inside the 
 *    conditional for the new version. When using up a future item, still increase the
 *    version number and take away the NextTabField(str, index);
 */
void CNavigatorDlg::OpenAndWriteFile(bool autosave)
{
  CStdioFile *cFile = NULL;
  mWinApp->RestoreViewFocus();
  CString str, sub, filename;
  IntVec skipIndex;
  int adocInd, adocErr, ind, sectInd, outInd, ind2;
  float varyVals[NUM_VARY_ELEMENTS * MAX_TS_VARIES];
  BaseMarkerShift baseShift;
  CArray<BaseMarkerShift, BaseMarkerShift> *shiftArray = mHelper->GetMarkerShiftArray();
  FILE *fp;
  if (autosave)
    filename = mParam->autosaveFile;
  else
    filename = mNavFilename;

  // Define macros for setting into autodoc for writing and XML
#define ADOC_PUT(a) adocErr += AdocSet##a;
#define ADOC_ARG "Item",sectInd

  adocErr = 0;

  if (mHelper->GetWriteNavAsXML()) {

    // If writing as XML, need to go through the autodoc structure, get mutex, etc
    if (!AdocAcquireMutex()) {
      AfxMessageBox("Could not acquire mutex for writing Navigator file as XML", 
        MB_EXCLAME);
      return;
    }

    adocInd = AdocNew();
    if (adocInd < 0) {
      AdocReleaseMutex();
      AfxMessageBox("Error creating new autodoc structure for writing Navigator file as "
        "XML", MB_EXCLAME);
      return;
    }

    AdocSetWriteAsXML(1);
    if (AdocSetXmlRootElement("navigator"))
      adocErr++;
    if (AdocSetKeyValue(ADOC_GLOBAL_NAME, 0, "AdocVersion", NAV_FILE_VERSION))
      adocErr++;
    if (AdocSetKeyValue(ADOC_GLOBAL_NAME, 0, "LastSavedAs", (LPCTSTR)filename))
      adocErr++;

    // Save all the members in the include file
    for (ind = 0; ind < mItemArray.GetSize(); ind++) {
      CMapDrawItem *item = mItemArray[ind];
      sectInd = AdocAddSection("Item", item->mLabel);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocPuts.h"
      }
    }

#define BOOL_SETT_ASSIGN(a, b) ADOC_PUT(Integer(ADOC_ARG, a, b ? 1 : 0)); 
#define INT_SETT_ASSIGN(a, b) ADOC_PUT(Integer(ADOC_ARG, a, b));
#define FLOAT_SETT_ASSIGN(a, b) ADOC_PUT(Float(ADOC_ARG, a, b));
#define DOUBLE_SETT_ASSIGN(a, b) ADOC_PUT(Float(ADOC_ARG, a, (float)b));

    // Save tilt series parameters
#undef ADOC_ARG
#define ADOC_ARG "TSParam",sectInd
#define SET_TEST_SECT3
#define NAV_OTHER_TS_PARAMS
    for (ind = 0; ind < mTSparamArray.GetSize(); ind++) {
      TiltSeriesParam *tsParam = mTSparamArray[ind];
      str.Format("%d", ind);
      sectInd = AdocAddSection("TSParam", str);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef SET_TEST_SECT3
#undef NAV_OTHER_TS_PARAMS

    // Save montage parameters
#undef ADOC_ARG
#define ADOC_ARG "MontParam",sectInd
#define NAV_MONT_PARAMS
    for (ind = 0; ind < mMontParArray.GetSize(); ind++) {
      MontParam *montParam = mMontParArray[ind];
      str.Format("%d", ind);
      sectInd = AdocAddSection("MontParam", str);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef NAV_MONT_PARAMS

    // Save file options  
#undef ADOC_ARG
#define ADOC_ARG "FileOptions",sectInd
#define NAV_FILE_OPTS
    for (ind = 0; ind < mFileOptArray.GetSize(); ind++) {
      FileOptions *fileOpt = mFileOptArray[ind];
      str.Format("%d", ind);
      sectInd = AdocAddSection("FileOptions", str);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef NAV_FILE_OPTS

    // Save base marker shifts
#undef ADOC_ARG
#define ADOC_ARG "BaseMarkerShift",sectInd
#define BASE_MARKER_SHIFTS
    for (ind = 0; ind < shiftArray->GetSize(); ind++) {
      baseShift = shiftArray->GetAt(ind);
      str.Format("%d", ind);
      sectInd = AdocAddSection("BaseMarkerShift", str);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef BASE_MARKER_SHIFTS

    if (adocErr) {
      str.Format("%d errors occurred setting Navigator data into autodoc structure before"
        " writing it as XML", adocErr);
      AfxMessageBox(str, MB_EXCLAME);
    }
    if (AdocWrite((LPCTSTR)filename))
      AfxMessageBox("An error occurred writing Navigator file as an XML file",MB_EXCLAME);
    AdocClear(adocInd);
    AdocReleaseMutex();
  } else {

    // redefine macros for direct writing to output file
#undef ADOC_PUT
#undef ADOC_ARG
#define ADOC_PUT(a) adocErr += AdocWrite##a;
#define ADOC_ARG fp
    fp = fopen((LPCTSTR)filename, "w");
    if (!fp) {
      AfxMessageBox("An error occurred opening a new file for saving the Navigator file",
        MB_EXCLAME);
      return;
    }
    adocErr += AdocWriteKeyValue(fp, "AdocVersion", NAV_FILE_VERSION);
    adocErr += AdocWriteKeyValue(fp, "LastSavedAs", (LPCTSTR)filename);

    // Regular members
    for (ind = 0; ind < mItemArray.GetSize(); ind++) {
      CMapDrawItem *item = mItemArray[ind];
      fprintf(fp, "\n");
      sectInd = AdocWriteSectionStart(fp, "Item", item->mLabel);
      if (sectInd) {
        adocErr++;
      } else {
#include "NavAdocPuts.h"
      }
    }

    // Tilt series params
#define SET_TEST_SECT3
#define NAV_OTHER_TS_PARAMS
    for (ind = 0; ind < mTSparamArray.GetSize(); ind++) {
      TiltSeriesParam *tsParam = mTSparamArray[ind];
      str.Format("%d", ind);
      fprintf(fp, "\n");
      sectInd = AdocWriteSectionStart(fp, "TSParam", str);
      if (sectInd) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef SET_TEST_SECT3
#undef NAV_OTHER_TS_PARAMS

    // Montage params
#define NAV_MONT_PARAMS
    for (ind = 0; ind < mMontParArray.GetSize(); ind++) {
      MontParam *montParam = mMontParArray[ind];
      str.Format("%d", ind);
      fprintf(fp, "\n");
      sectInd = AdocWriteSectionStart(fp, "MontParam", str);
      if (sectInd) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef NAV_MONT_PARAMS

    // File options
#define NAV_FILE_OPTS
    for (ind = 0; ind < mFileOptArray.GetSize(); ind++) {
      FileOptions *fileOpt = mFileOptArray[ind];
      str.Format("%d", ind);
      fprintf(fp, "\n");
      sectInd = AdocWriteSectionStart(fp, "FileOptions", str);
      if (sectInd) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef NAV_FILE_OPTS

    // Save base marker shifts
#define BASE_MARKER_SHIFTS
    for (ind = 0; ind < shiftArray->GetSize(); ind++) {
      baseShift = shiftArray->GetAt(ind);
      str.Format("%d", ind);
      fprintf(fp, "\n");
      sectInd = AdocWriteSectionStart(fp, "BaseMarkerShift", str);
      if (sectInd < 0) {
        adocErr++;
      } else {
#include "NavAdocParams.h"
      }
    }
#undef BASE_MARKER_SHIFTS

    if (adocErr) {
      str.Format("%d errors occurred writing Navigator data into file", adocErr);
      AfxMessageBox(str, MB_EXCLAME);
    }
    fclose(fp);
#undef ADOC_PUT
#undef ADOC_ARG

#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN
#undef DOUBLE_SETT_ASSIGN

  }
  mChanged = false;
}

// Macros for autodoc reading and handling of assignments of strings and booleans
#define ADOC_OPTIONAL(a) \
  retval = a; \
  if (retval < 0) \
    numAdocErr++;

#define ADOC_REQUIRED(a) \
  retval = a; \
  if (retval < 0) \
    numAdocErr++;  \
  if (retval > 0) \
    numLackRequired++;

#define ADOC_STR_ASSIGN(a)  \
  if (!retval && adocStr) {  \
    a = adocStr;  \
    free(adocStr);  \
  }

#define ADOC_BOOL_ASSIGN(a)  \
  if (!retval)  \
    a = index != 0;

// Macros for reading parameter sets to other sections
#define INT_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetInteger(SECT_NAME, ind1, a, &b));

#define BOOL_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetInteger(SECT_NAME, ind1, a, &index)); \
       ADOC_BOOL_ASSIGN(b);

#define FLOAT_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetFloat(SECT_NAME, ind1, a, &b));

#define DOUBLE_SETT_ASSIGN(a, b) ADOC_REQUIRED(AdocGetFloat(SECT_NAME, ind1, a, &xx)); \
       if (!retval) \
         b = xx;

#define SKIP_ADOC_PUTS

#define ADOC_STR_TO_LIST(a)   \
  if (!retval && adocStr) {  \
    str2 = adocStr;  \
    free(adocStr);  \
    retval = a; \
    if (retval > 0) \
      numAdocErr++;  \
  }

#define FIND_DUP_OR_ADD(arr, parm, parm2, nwind) \
  ind2 = (int)arr.GetSize();  \
  for (index = 0; index < ind2; index++) { \
    parm2 = arr[index];    \
    if (parm2->navID == parm->navID)  \
      ind2 = index;   \
  }   \
  if (ind2 == (int)arr.GetSize()) \
    arr.Add(parm);  \
  else   \
    delete parm;   \
  nwind.push_back(ind2);


// Loading a file
int CNavigatorDlg::LoadNavFile(bool checkAutosave, bool mergeFile, CString *inFilename) 
{
  CString str, str2, navRoot, ext, extra, root, lastSavedRoot, name = "";
  CStdioFile *cFile = NULL;
  CFileStatus status;
  int retval, externalErr, returnVal = 0;
  bool hasStage;
  BOOL found;
  int numSect, numAdocErr, numLackRequired, sectInd = 0, adocIndex = -1;
  int numToGet, numItemLack = 0, numItemErr = 0, numExtErr = 0;
  int extErrCounts[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const char *extErrMess[16] = {
    "an entry for StageXYZ or more than one entry for a CoordsIn... key", 
    "no map ID entry for DrawnOn", "no map matching the ID given in DrawnOn",
    "a CoordsIn... key indicating a montage, but the DrawnOn map is not a montage",
    "an error accessing the montage map file", 
    "a CoordsInPiece entry but no PieceDrawnOn entry",
    "a montage map missing aligned piece coordinates in the mdoc file",
    "a montage map missing nominal piece coordinates in the mdoc file",
    "a montage map file having no mdoc file",
    "an error accessing the mdoc file by its autodoc index", "", "", "", ""};
  int index, i, numPoints, trimCount, ind1, ind2, numFuture = 0, addIndex, highestLabel;
  float xx, yy, fvals[4], varyVals[NUM_VARY_ELEMENTS * MAX_TS_VARIES];
  int numExternal, externalType, numParams, curNum, numDig, maxNum;
  int holeSkips[2000];
  BaseMarkerShift baseShift;
  CArray<BaseMarkerShift, BaseMarkerShift> *shiftArray = mHelper->GetMarkerShiftArray();
  std::set<std::string> filesNotFound;
  std::set<std::string>::iterator fileIter;
  std::map<std::string, std::string> filesFoundMap;
  std::map<std::string, std::string>::iterator mapIter;
  std::string stdstr, stdstr2;
  IntVec newTSparamInds, newMontParamInds, newFileOptInds;
  const char *externalKeys[4] = {"CoordsInMap", "CoordsInAliMont", "CoordsInAliMontVS",
    "CoordsInPiece"};
  char *adocStr;
  TiltSeriesParam *tsParam, *tsp2;
  FileOptions *fileOpt, *fileo2;
  MontParam *montParam, *montp2;
  CMapDrawItem *item, *prev, *testItem;
  int originalSize = (int)mItemArray.GetSize();
  bool resetAcquireEnd = mAcquireIndex >= 0 && originalSize - 1 <= mEndingAcquireIndex;
  int version = 100;
  int mapSkip = 13;

  if (checkAutosave) {
    if (mParam->autosaveFile.IsEmpty())
      return 1;
    if (!CFile::GetStatus((LPCTSTR)mParam->autosaveFile, status))
      return 1;
    if (AfxMessageBox("There is an autosaved Navigator file from your previous session:"
      "\n" + mParam->autosaveFile + "\n\nDo you want to recover this file?",
      MB_YESNO | MB_ICONQUESTION) == IDNO) {
      mParam->autosaveFile = "";
      return 1;
    } else
      name = mParam->autosaveFile;
  } else {

    if (!mergeFile && AskIfSave("loading new data?"))
      return 1;

    // If name is supplied, assign to mergeName as happens when get filename from user,
    // and assign to navFilename if not merging
    if (inFilename) {
      mMergeName = *inFilename;
      if (!mergeFile) {
        SetCurrentNavFile(*inFilename);
      }
    } else {
      if (GetNavFilename(true, OFN_HIDEREADONLY, mergeFile))
        return 1;
    }
    name = mMergeName;
  }
  UtilSplitPath(name, navRoot, str);

  try {
    // Open the file for reading and verify first line; get optional version
    cFile = new CStdioFile(name, CFile::modeRead | CFile::shareDenyWrite);
    retval = cFile->ReadString(str);
    if (retval && (str.Left(11) == "AdocVersion") || (str.Left(5) == "<?xml")) {

      // Close file and open as autodoc
      cFile->Close();
      delete cFile;
      if (!AdocAcquireMutex()) {
        SEMMessageBox("The Navigator file is an autodoc and the mutex for accessing it "
          "could not be acquired", MB_EXCLAME);
        return 1;
      }
      adocIndex = AdocRead((LPCTSTR)name);
      if (adocIndex < 0) {
        SEMMessageBox("An error occurred reading in the Navigator file as an autodoc",
          MB_EXCLAME);
        AdocReleaseMutex();
        return 1;
      }
      if (!AdocGetString(ADOC_GLOBAL_NAME, 0, "LastSavedAs", &adocStr) && adocStr) {
        str = adocStr;
        free(adocStr);
        UtilSplitPath(str, lastSavedRoot, str2);
      }
      numSect = AdocGetNumberOfSections("Item");
      if (numSect < 0 || AdocGetFloat(ADOC_GLOBAL_NAME, 0, "AdocVersion", &xx)) {
        AdocClear(adocIndex);
        SEMMessageBox(numSect < 0 ? "An error occurred reading the number of items in "
          "the Navigator file" : "This file is missing the AdocVersion entry "
          "required for a Navigator file", MB_EXCLAME);
        AdocReleaseMutex();
        return 1;
      }
      version = B3DNINT(100. * xx);

    } else {

      // Or set up to process the old type of file
      if (retval && str.Left(7) == "Version") {
        index = 0;
        NextTabField(str, index);
        version = (int)(100. * atof(NextTabField(str, index)) + 0.5);
        if (version > VERSION_TIMES_100) {
          SEMMessageBox("The version number of this Navigator file is higher than can be "
            "read by this version of SerialEM", MB_EXCLAME);
          cFile->Close();
          delete cFile;
          return 1;
        }
        retval = cFile->ReadString(str);
      }
      if (!retval || str.Left(5) != "Label") {
        SEMMessageBox("This file does not have the right starting line\n"
          " to be a Navigator file", MB_EXCLAME);
        cFile->Close();
        delete cFile;
        return 1;
      }

      // ADD AN ENTRY HERE WHEN YOU ADD MAP ITEMS
      if (version > 100)
        mapSkip += 2;
      if (version > 110)
        mapSkip += 2;
      if (version > 130)
        mapSkip -= 2;
      if (version > 140)
        mapSkip += 1;
      if (version > 142)
        mapSkip += 1;
      if (version > 143)
        mapSkip += 3;
      if (version > 145)
        mapSkip += 14;
      if (version > 148)
        mapSkip += 2;
      if (version > 150)
        mapSkip += 4;
      if (version > 151)
        mapSkip += 4;
    }

    // Clean up existing data
    if (!mergeFile) {
      mHelper->DeleteArrays();
      mSetOfIDs.clear();
      mNewItemNum = 1;
      mDualMapID = -1;
      mHelper->ClearSavedMapMarkerShifts();
      shiftArray->RemoveAll();
    }
    mHelper->SetExtDrawnOnID(0);
    addIndex = mergeFile ? (int)mItemArray.GetSize() : 0;

    // Read in the params and file options first
    if (adocIndex >= 0) {

      // Tilt series params from macros in include file
#define SET_TEST_SECT3
#define NAV_OTHER_TS_PARAMS
#define SECT_NAME "TSParam"
      numParams = AdocGetNumberOfSections("TSParam");
      for (ind1 = 0; ind1 < numParams; ind1++) {
        tsParam = new TiltSeriesParam;
#include "NavAdocParams.h"

        // Other tilt series items: three ints, extra recs
        ADOC_REQUIRED(AdocGetThreeIntegers("TSParam", ind1, "MagIndex",
          &tsParam->magIndex[0], &tsParam->magIndex[1], &tsParam->magIndex[2]));
        ADOC_REQUIRED(AdocGetThreeIntegers("TSParam", ind1, "LowMagIndex",
          &tsParam->lowMagIndex[0], &tsParam->lowMagIndex[1], &tsParam->lowMagIndex[2]));
        ADOC_REQUIRED(AdocGetThreeIntegers("TSParam", ind1, "BidirAnchMagIndReg",
          &tsParam->bidirAnchorMagInd[0], &tsParam->bidirAnchorMagInd[2],
          &tsParam->bidirAnchorMagInd[4]));
        ADOC_REQUIRED(AdocGetThreeIntegers("TSParam", ind1, "BidirAnchMagIndLD",
          &tsParam->bidirAnchorMagInd[1], &tsParam->bidirAnchorMagInd[3],
          &tsParam->bidirAnchorMagInd[5]));
        if (tsParam->numExtraExposures) {
          ADOC_REQUIRED(AdocGetString("TSParam", ind1, "ExtraExposures", &adocStr));
          ADOC_STR_TO_LIST(mWinApp->mParamIO->StringToEntryList(1, str2,
            tsParam->numExtraExposures, NULL, tsParam->extraExposures,MAX_EXTRA_RECORDS));
        }
        if (tsParam->numExtraFocus) {
          ADOC_REQUIRED(AdocGetString("TSParam", ind1, "ExtraFocuses", &adocStr));
          ADOC_STR_TO_LIST(mWinApp->mParamIO->StringToEntryList(1, str2,
            tsParam->numExtraFocus, NULL, tsParam->extraFocus, MAX_EXTRA_RECORDS));
        }
        if (tsParam->numExtraFilter) {
          ADOC_REQUIRED(AdocGetString("TSParam", ind1, "ExtraLosses", &adocStr));
          ADOC_STR_TO_LIST(mWinApp->mParamIO->StringToEntryList(1, str2,
            tsParam->numExtraFilter, NULL, tsParam->extraLosses, MAX_EXTRA_RECORDS));
          ADOC_REQUIRED(AdocGetString("TSParam", ind1, "ExtraSlits", &adocStr));
          ADOC_STR_TO_LIST(mWinApp->mParamIO->StringToEntryList(3, str2,
            tsParam->numExtraFilter, tsParam->extraSlits, NULL, MAX_EXTRA_RECORDS));
        }
        if (tsParam->numExtraChannels) {
          ADOC_REQUIRED(AdocGetString("TSParam", ind1, "ExtraChannels", &adocStr));
          ADOC_STR_TO_LIST(mWinApp->mParamIO->StringToEntryList(3, str2,
            tsParam->numExtraChannels, tsParam->extraChannels, NULL, MAX_STEM_CHANNELS));
        }

        // Varying items
        if (tsParam->numVaryItems) {
          numToGet = tsParam->numVaryItems * NUM_VARY_ELEMENTS;
          ADOC_REQUIRED(AdocGetFloatArray("TSParam", ind1, "VaryArray", varyVals, 
            &numToGet, MAX_TS_VARIES * NUM_VARY_ELEMENTS));
          if (!retval) {
            i = 0;
            for (ind2 = 0; ind2 < tsParam->numVaryItems; ind2++) {
              tsParam->varyArray[ind2].angle = varyVals[i++];
              tsParam->varyArray[ind2].plusMinus = varyVals[i++] != 0.;
              tsParam->varyArray[ind2].linear = varyVals[i++] != 0.;
              tsParam->varyArray[ind2].type = (short)varyVals[i++];
              tsParam->varyArray[ind2].value = varyVals[i++];
            }
          }
        }

        // Find if this param already exists (for a merge) and delete it if so, or add it
        FIND_DUP_OR_ADD(mTSparamArray, tsParam, tsp2, newTSparamInds);
      }
#undef SET_TEST_SECT3
#undef NAV_OTHER_TS_PARAMS
#undef SECT_NAME

      // Montage params
#define NAV_MONT_PARAMS
#define SECT_NAME "MontParam"
      numParams = AdocGetNumberOfSections("MontParam");
      for (ind1 = 0; ind1 < numParams; ind1++) {
        montParam = new MontParam;
#include "NavAdocParams.h"
        if (montParam->numToSkip > 0) {
          montParam->skipPieceX.resize(montParam->numToSkip);
          numToGet = montParam->numToSkip;
          if (AdocGetIntegerArray("MontParam", ind1, "SkipPieceX", holeSkips, &numToGet,
            2000)) {
            numAdocErr++;
          } else {
            for (ind2 = 0; ind2 < montParam->numToSkip; ind2++)
              montParam->skipPieceX[ind2] = (short)holeSkips[ind2];
          }

          montParam->skipPieceY.resize(montParam->numToSkip);
          if (AdocGetIntegerArray("MontParam", ind1, "SkipPieceY", holeSkips, &numToGet,
            2000)) {
            numAdocErr++;
          } else {
            for (ind2 = 0; ind2 < montParam->numToSkip; ind2++)
              montParam->skipPieceY[ind2] = (short)holeSkips[ind2];
          }
        }

        FIND_DUP_OR_ADD(mMontParArray, montParam, montp2, newMontParamInds);
      }
#undef NAV_MONT_PARAMS
#undef SECT_NAME

      // File options
#define NAV_FILE_OPTS
#define SECT_NAME "FileOptions"
      numParams = AdocGetNumberOfSections("FileOptions");
      for (ind1 = 0; ind1 < numParams; ind1++) {
        fileOpt = new FileOptions;
#include "NavAdocParams.h"
        FIND_DUP_OR_ADD(mFileOptArray, fileOpt, fileo2, newFileOptInds);
      }
#undef NAV_FILE_OPTS
#undef SECT_NAME

      // Base marker shifts
#define BASE_MARKER_SHIFTS
#define SECT_NAME "BaseMarkerShift"
      numParams = AdocGetNumberOfSections("BaseMarkerShift");
      for (ind1 = 0; ind1 < numParams && !mergeFile; ind1++) {
#include "NavAdocParams.h"
        shiftArray->Add(baseShift);
      }
#undef BASE_MARKER_SHIFTS
#undef SECT_NAME

#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN
#undef DOUBLE_SETT_ASSIGN
#undef SKIP_ADOC_PUTS
#undef ADOC_STR_TO_LIST
#undef FIND_DUP_OR_ADD
    }

    // Loop on entries in either case
    for (;;) {
      numAdocErr = numLackRequired = numExternal = externalErr = 0;
      if (adocIndex >= 0) {

        // Fetch values from autodoc until # of items satisfied
        if (sectInd >= numSect)
          break;
        item = new CMapDrawItem();
        ADOC_REQUIRED(AdocGetSectionName("Item", sectInd, &adocStr));
        ADOC_STR_ASSIGN(item->mLabel);
        ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "Color", &item->mColor));

        // Get a stage coordinate or one of the four external pixel coordinate items
        // Store them all in the same place; require at least one here
        ADOC_OPTIONAL(AdocGetThreeFloats("Item", sectInd, "StageXYZ", &item->mStageX,
          &item->mStageY, &item->mStageZ));
        hasStage = retval == 0;
        for (i = 0; i < 4; i++) {
          ADOC_OPTIONAL(AdocGetThreeFloats("Item", sectInd, externalKeys[i], 
            &item->mStageX, &item->mStageY, &item->mStageZ));
          if (retval == 0) {
            numExternal++;
            externalType = i;
          }
        }
        if (!hasStage && numExternal == 0)
          numLackRequired++;
        ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "NumPts", &numPoints));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "Corner", &index));
        ADOC_BOOL_ASSIGN(item->mCorner);
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "Draw", &index));
        ADOC_BOOL_ASSIGN(item->mDraw);
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "RegPt", &item->mRegPoint));
        ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "Regis", &item->mRegistration));
        ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "Type", &item->mType));
        ADOC_OPTIONAL(AdocGetString("Item", sectInd, "Note", &adocStr));
        ADOC_STR_ASSIGN(item->mNote);
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "GroupID", &item->mGroupID));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "PolyID", &item->mPolygonID));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "Imported", &item->mImported));
        ADOC_OPTIONAL(AdocGetTwoIntegers("Item", sectInd, "SuperMontXY",
          &item->mSuperMontX, &item->mSuperMontY));

        // The default original reg is the current one
        item->mOriginalReg = item->mRegistration;
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "OrigReg", &item->mOriginalReg));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "DrawnID", &item->mDrawnOnMapID));
        ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "BklshXY",
          &item->mBacklashX, &item->mBacklashY));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "SamePosId", &item->mAtSamePosID));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "FitToPolygonID", 
          &item->mFitToPolygonID));
        ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "RawStageXY",
          &item->mRawStageX, &item->mRawStageY));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "Acquire", &index));
        ADOC_BOOL_ASSIGN(item->mAcquire);

        // No need to subtract 1, it should be saved as is
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "PieceOn", &item->mPieceDrawnOn));
        ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "XYinPc", &item->mXinPiece, 
          &item->mYinPiece));
        ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "FocusAxisPos",&item->mFocusAxisPos));
        ADOC_OPTIONAL(AdocGetTwoIntegers("Item", sectInd, "LDAxisAngle", 
          &item->mRotateFocusAxis, &item->mFocusAxisAngle));
        ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "TSstartEndAngles",
          &item->mTSstartAngle, &item->mTSendAngle));
        ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "TSbidirAngle",
          &item->mTSbidirAngle));
        ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "TargetDefocus",
          &item->mTargetDefocus));
        ADOC_OPTIONAL(AdocGetTwoIntegers("Item", sectInd, "FocusOffsets",
          &item->mFocusXoffset, &item->mFocusYoffset));
        ind1 = ind2 = 0;
        ADOC_OPTIONAL(AdocGetTwoIntegers("Item", sectInd, "HoleArray", &ind1, &ind2));
        item->mNumXholes = ind1;
        item->mNumYholes = ind2;
        numToGet = 0;
        ADOC_OPTIONAL(AdocGetIntegerArray("Item", sectInd, "SkipHoles", &holeSkips[0], 
          &numToGet, 2000));
        if (numToGet) {
          item->mSkipHolePos = new unsigned char[numToGet];
          for (ind1 = 0; ind1 < numToGet; ind1++)
            item->mSkipHolePos[ind1] = holeSkips[ind1];
          item->mNumSkipHoles = numToGet / 2;
        }
        ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "MarkerShift",
          &item->mMarkerShiftX, &item->mMarkerShiftY));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "ShiftCohortID", 
          &item->mShiftCohortID));
        for (ind1 = 1; ind1 <= MAX_NAV_USER_VALUES; ind1++) {
          str.Format("UserValue%d", ind1);
          retval = AdocGetString("Item", sectInd, (LPCTSTR)str, &adocStr);
          if (!retval && adocStr) {
            mHelper->SetUserValue(item, ind1, CString(adocStr));
            free(adocStr);
          }
        }
        ADOC_OPTIONAL(AdocGetString("Item", sectInd, "FileToOpen", &adocStr));
        ADOC_STR_ASSIGN(item->mFileToOpen);

        // Adjust name to be unique
        if (!retval && (mHelper->NameToOpenUsed(item->mFileToOpen) || 
            mDocWnd->StoreIndexFromName(item->mFileToOpen) >= 0)) {
          root = mHelper->DecomposeNumberedName(item->mFileToOpen, ext, curNum, numDig,
            extra);
          str = root;
          str.MakeUpper();
          maxNum = curNum;

          // Find highest existing number matching the root name
          for (ind1 = 0; ind1 < mItemArray.GetSize(); ind1++) {
            testItem = mItemArray[ind1];
            mHelper->CheckForSameRootAndNumber(str, ext, extra, testItem->mFileToOpen,
              maxNum, numDig);
          }
          for (ind1 = 0; ind1 < mDocWnd->GetNumStores(); ind1++) {
            str2 = (mDocWnd->GetStoreMRC(ind1))->getName();
            mHelper->CheckForSameRootAndNumber(str, ext, extra, str2, maxNum, numDig);
          }

          // Construct new name
          if (numDig) {
            str2.Format("%%0%dd", numDig);
            str.Format((LPCTSTR)str2, maxNum + 1);
            item->mFileToOpen = root + str + extra + ext;
          } else {
            item->mFileToOpen = root + "2" + ext;
          }
        }

        // Read in parameter indexes and map them to new values
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "TSparamIndex",
          &item->mTSparamIndex));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MontParamIndex",
          &item->mMontParamIndex));
        ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "FilePropIndex",
          &item->mFilePropIndex));
        if (item->mTSparamIndex >= 0 && item->mTSparamIndex < (int)newTSparamInds.size())
          item->mTSparamIndex = newTSparamInds[item->mTSparamIndex];
        if (item->mMontParamIndex >= 0 && item->mMontParamIndex < 
          (int)newMontParamInds.size())
          item->mMontParamIndex = newMontParamInds[item->mMontParamIndex];
        if (item->mFilePropIndex >= 0 && item->mFilePropIndex < 
          (int)newFileOptInds.size())
          item->mFilePropIndex = newFileOptInds[item->mFilePropIndex];

        if (item->IsMap()) {
          ADOC_REQUIRED(AdocGetString("Item", sectInd, "MapFile", &adocStr));
          ADOC_STR_ASSIGN(item->mMapFile);
          if (!retval) 
            UtilSplitPath(item->mMapFile, str, item->mTrimmedName);

          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapID", &item->mMapID));
          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapMontage", &index));
          ADOC_BOOL_ASSIGN(item->mMapMontage);
          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapSection",&item->mMapSection));
          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapBinning",&item->mMapBinning));
          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapMagInd", &item->mMapMagInd));
          ADOC_REQUIRED(AdocGetInteger("Item", sectInd, "MapCamera", &item->mMapCamera));
          numToGet = 4;
          ADOC_REQUIRED(AdocGetFloatArray("Item", sectInd, "MapScaleMat", &fvals[0], 
            &numToGet, 4));
          if (!retval) {
            item->mMapScaleMat.xpx = fvals[0];
            item->mMapScaleMat.xpy = fvals[1];
            item->mMapScaleMat.ypx = fvals[2];
            item->mMapScaleMat.ypy = fvals[3];
          }
          ADOC_REQUIRED(AdocGetTwoIntegers("Item", sectInd, "MapWidthHeight",
            &item->mMapWidth, &item->mMapHeight));

          // Make later map items optional
          item->mMapMinScale = item->mMapMaxScale = 0.;
          item->mMapFramesX = item->mMapFramesY = 0;
          item->mMontBinning = 0;
          item->mMapExposure = 0.;
          item->mMapSettling = 0.;
          item->mShutterMode = -1;
          item->mMapSpotSize = 0;
          item->mMapIntensity = 0.;
          item->mMapSlitIn = false;
          item->mMapSlitWidth = -1.f;
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "MapMinMaxScale",
            &item->mMapMinScale, &item->mMapMaxScale));
          ADOC_OPTIONAL(AdocGetTwoIntegers("Item", sectInd, "MapFramesXY",
            &item->mMapFramesX, &item->mMapFramesY));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MontBinning", 
            &item->mMontBinning));
          ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "MapExposure",&item->mMapExposure));
          ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "MapSettling",&item->mMapSettling));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "ShutterMode", 
            &item->mShutterMode));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapSpotSize", 
            &item->mMapSpotSize));

          // Preserve double precision if it is there
          ADOC_OPTIONAL(AdocGetString("Item", sectInd, "MapIntensity", &adocStr));
          if (!retval) {
            item->mMapIntensity = atof(adocStr);
            free(adocStr);
          }
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapSlitIn", &index));
          ADOC_BOOL_ASSIGN(item->mMapSlitIn);
          ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "MapSlitWidth", 
            &item->mMapSlitWidth));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "RotOnLoad", &index));
          ADOC_BOOL_ASSIGN(item->mRotOnLoad);
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "RealignedID", 
            &item->mRealignedID));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "RegisteredToID", 
            &item->mRegisteredToID));
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "RealignErrXY",
            &item->mRealignErrX, &item->mRealignErrX));
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "LocalErrXY",
            &item->mLocalRealiErrX, &item->mLocalRealiErrX));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "RealignReg",&item->mRealignReg));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "ImageType", &item->mImageType));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MontUseStage", 
            &item->mMontUseStage));

          // mBacklashX/Y was being saved twice since it was already in map when added
          // to regular output
          ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "DefocusOffset",
            &item->mDefocusOffset));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "K2ReadMode",&item->mK2ReadMode));
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "NetViewShiftXY",
            &item->mNetViewShiftX, &item->mNetViewShiftY));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapAlpha", &item->mMapAlpha));
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "ViewBeamShiftXY",
            &item->mViewBeamShiftX, &item->mViewBeamShiftY));
          ADOC_OPTIONAL(AdocGetTwoFloats("Item", sectInd, "ViewBeamTiltXY",
            &item->mViewBeamTiltX, &item->mViewBeamTiltY));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapProbeMode",
            &item->mMapProbeMode));
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapLDConSet",
            &item->mMapLowDoseConSet));
          ADOC_OPTIONAL(AdocGetFloat("Item", sectInd, "MapTiltAngle",
            &item->mMapTiltAngle));
        } else {
          ADOC_OPTIONAL(AdocGetInteger("Item", sectInd, "MapID", &item->mMapID));
        }

        // Get all the points: optional for external item
        if (numPoints > 0) {
          item->mPtX = new float[numPoints];
          item->mPtY = new float[numPoints];
          item->mNumPoints = numPoints;
          item->mMaxPoints = numPoints;
          numToGet = numPoints;
          ADOC_REQUIRED(AdocGetFloatArray("Item", sectInd, "PtsX", &item->mPtX[0], 
            &numToGet, numPoints));
          numToGet = numPoints;
          ADOC_REQUIRED(AdocGetFloatArray("Item", sectInd, "PtsY", &item->mPtY[0], 
            &numToGet, numPoints));
        } else if (numExternal && item->IsPoint()) {
          item->AppendPoint(item->mStageX, item->mStageY);
        } else {
          numLackRequired++;
        }

        // Process an external item, first making sure there is only one entry
        if (numExternal && !numLackRequired && !numAdocErr) {
          if (hasStage || numExternal > 1) {
            ind1 = NEXERR_MULTIPLE_ENTRY;
          } else {
            ind1 = mHelper->ProcessExternalItem(item, externalType);
          }
          if (ind1) {
            extErrCounts[ind1]++;
            externalErr = 1;
            if (GetDebugOutput('n'))
              PrintfToLog("Error for external item %s: has %s", (LPCTSTR)item->mLabel,
                extErrMess[ind1]);
          }
        }

        sectInd++;

      } else {

        // Or read lines from old table until the end
        if (!cFile->ReadString(str))
          break;
        index = 0;
        item = new CMapDrawItem();
        item->mLabel = NextTabField(str, index);
        item->mColor = atoi(NextTabField(str, index));
        item->mStageX = (float)atof(NextTabField(str, index));
        item->mStageY = (float)atof(NextTabField(str, index));
        item->mStageZ = (float)atof(NextTabField(str, index));
        numPoints = atoi(NextTabField(str, index));
        item->mCorner = atoi(NextTabField(str, index)) != 0;
        item->mDraw = atoi(NextTabField(str, index)) != 0;
        item->mRegPoint = atoi(NextTabField(str, index));
        item->mRegistration = atoi(NextTabField(str, index));
        item->mType = atoi(NextTabField(str, index));
        item->mNote = NextTabField(str, index);
        if (version > 130) {
          item->mGroupID = atoi(NextTabField(str, index));
          item->mPolygonID = atoi(NextTabField(str, index));
          item->mImported = atoi(NextTabField(str, index));
        }
        if (version >= 142) {
          item->mSuperMontX = atoi(NextTabField(str, index));
          item->mSuperMontY = atoi(NextTabField(str, index));
        }
        if (version >= 145) {
          item->mOriginalReg = atoi(NextTabField(str, index));
          item->mDrawnOnMapID = atoi(NextTabField(str, index));
        }
        if (version > 146) {
          item->mBacklashX = (float)atof(NextTabField(str, index));
          item->mBacklashY = (float)atof(NextTabField(str, index));
        }
        if (version > 147) {
          item->mAtSamePosID = atoi(NextTabField(str, index));
          item->mRawStageX = (float)atof(NextTabField(str, index));
          item->mRawStageX = (float)atof(NextTabField(str, index));
        }
        if (version > 149) {
          item->mAcquire = atoi(NextTabField(str, index)) != 0;

          // Convert a placekeeper 0 to -1 for no piece index, saved index + 1 above
          item->mPieceDrawnOn = atoi(NextTabField(str, index)) - 1;
          NextTabField(str, index);
        }
        if (item->IsMap()) {
          item->mMapFile = NextTabField(str, index);
          trimCount = item->mMapFile.GetLength() - (item->mMapFile.ReverseFind('\\') + 1);
          item->mTrimmedName = item->mMapFile.Right(trimCount);
          item->mMapID = atoi(NextTabField(str, index));
          item->mMapMontage = atoi(NextTabField(str, index)) != 0;
          item->mMapSection = atoi(NextTabField(str, index));
          item->mMapBinning = atoi(NextTabField(str, index));
          item->mMapMagInd = atoi(NextTabField(str, index));
          item->mMapCamera = atoi(NextTabField(str, index));
          item->mMapScaleMat.xpx = (float)atof(NextTabField(str, index));
          item->mMapScaleMat.xpy = (float)atof(NextTabField(str, index));
          item->mMapScaleMat.ypx = (float)atof(NextTabField(str, index));
          item->mMapScaleMat.ypy = (float)atof(NextTabField(str, index));
          item->mMapWidth = atoi(NextTabField(str, index));
          item->mMapHeight = atoi(NextTabField(str, index));
          if (version > 100) {
            item->mMapMinScale = (float)atof(NextTabField(str, index));
            item->mMapMaxScale = (float)atof(NextTabField(str, index));
          } else
            item->mMapMinScale = item->mMapMaxScale = 0.;
          if (version > 110) {
            item->mMapFramesX = atoi(NextTabField(str, index));
            item->mMapFramesY = atoi(NextTabField(str, index));
          } else
            item->mMapFramesX = item->mMapFramesY = 0;
          if (version > 120) {
            item->mMontBinning = atoi(NextTabField(str, index));
            item->mMapExposure = (float)atof(NextTabField(str, index));
            item->mMapSettling = (float)atof(NextTabField(str, index));
            item->mShutterMode = atoi(NextTabField(str, index));
            item->mMapSpotSize = atoi(NextTabField(str, index));
            item->mMapIntensity = atof(NextTabField(str, index));
            item->mMapSlitIn = atoi(NextTabField(str, index)) > 0;
            item->mMapSlitWidth = (float)atof(NextTabField(str, index));
            item->mRotOnLoad = atoi(NextTabField(str, index)) > 0;
            item->mRealignedID = atoi(NextTabField(str, index));
            item->mRealignErrX = (float)atof(NextTabField(str, index));
            item->mRealignErrY = (float)atof(NextTabField(str, index));
          } else {
            item->mMontBinning = 0;
            item->mMapExposure = 0.;
            item->mMapSettling = 0.;
            item->mShutterMode = -1;
            item->mMapSpotSize = 0;
            item->mMapIntensity = 0.;
            item->mMapSlitIn = false;
            item->mMapSlitWidth = -1.f;
          }
          if (version > 143) {
            item->mLocalRealiErrX = (float)atof(NextTabField(str, index));
            item->mLocalRealiErrY = (float)atof(NextTabField(str, index));
            item->mRealignReg = atoi(NextTabField(str, index));
          }
          if (version > 140)
            item->mImageType = atoi(NextTabField(str, index));
          if (version > 142)
            item->mMontUseStage = atoi(NextTabField(str, index));
          if (version == 146) {
            item->mBacklashX = (float)atof(NextTabField(str, index));
            item->mBacklashY = (float)atof(NextTabField(str, index));
          } else if (version > 146) {
            item->mDefocusOffset = (float)atof(NextTabField(str, index));
            item->mK2ReadMode = atoi(NextTabField(str, index));
          }
          if (version > 148) {
            item->mNetViewShiftX = (float)atof(NextTabField(str, index));
            item->mNetViewShiftY = (float)atof(NextTabField(str, index));
          }

          // Both 151 and 152 left 3 unused, then 152 used one up
          if (version > 150) {
            item->mMapAlpha = B3DNINT(atof(NextTabField(str, index)));
            numFuture = 3;
          }
          if (version > 151) {
            item->mViewBeamShiftX = (float)atof(NextTabField(str, index));
            item->mViewBeamShiftY = (float)atof(NextTabField(str, index));
            item->mViewBeamTiltX = (float)atof(NextTabField(str, index));
            item->mViewBeamTiltY = (float)atof(NextTabField(str, index));

            // Convert placekeeper 0 to a -1 for both probe and conset
            item->mMapProbeMode = B3DNINT(atof(NextTabField(str, index))) - 1;
            item->mMapLowDoseConSet = B3DNINT(atof(NextTabField(str, index))) - 1;
            numFuture = 1;
          }

          for (i = 0; i < numFuture; i++)
            NextTabField(str, index);
        } else {

          // If not reading map file entries, skip one, get map id, skip the rest
          if (version > 130) {
            NextTabField(str, index);
            item->mMapID = atoi(NextTabField(str, index));
          }
          for (i = 0; i < mapSkip; i++)
            NextTabField(str, index);
        }
        for (i = 0; i < numPoints; i++) {
          xx = (float)atof(NextTabField(str, index));
          yy = (float)atof(NextTabField(str, index));
          item->AppendPoint(xx, yy);
        }
      }

      if (numAdocErr + numLackRequired + externalErr == 0) {

        // Check that map file is there and corect name
        if (item->IsMap() && !item->mTrimmedName.IsEmpty()) {

          // First see if it is in the map of found files, if so just assign name
          stdstr = (LPCTSTR)item->mMapFile;
          if (filesFoundMap.count(stdstr)) {
            mapIter = filesFoundMap.find(stdstr);
            item->mMapFile = mapIter->second.c_str();
          } else {

            // Otherwise look for it where it is supposed to be, then in dir of nav file
            str = item->mMapFile;
            found = CFile::GetStatus((LPCTSTR)str, status);
            if (!found) {
              str = navRoot + "\\" + item->mTrimmedName;
              found = CFile::GetStatus((LPCTSTR)str, status);
            }

            // Then look for it in relative path locations indicated by ./ or ../, and
            // then go relative to the saved location of the nav file
            if (!found && item->mMapFile.Find("./") == 0 || item->mMapFile.Find(".\\") == 0
              || item->mMapFile.Find("../") == 0 || item->mMapFile.Find("..\\") == 0) {
                str = navRoot + "\\" + item->mMapFile;
                found = CFile::GetStatus((LPCTSTR)str, status);
            }
            if (!found && !lastSavedRoot.IsEmpty()) {
              UtilSplitPath(item->mMapFile, str2, str);
              if (!UtilRelativePath(lastSavedRoot, str2, str)) {
                str = navRoot + "\\" + str + "\\" + item->mTrimmedName;
                found = CFile::GetStatus((LPCTSTR)str, status);
              }
            }

            // If found add to map, if not add to set of not found ones
            if (found) {
              stdstr2 = (LPCTSTR)str;
              filesFoundMap.insert(std::pair<std::string, std::string>(stdstr, stdstr2));
              item->mMapFile = str;
            } else if (!found) {
              filesNotFound.insert(stdstr);
            }
          }
        }

        // For merging, check that the map ID is not previously present or that point is
        // not at the same position
        index = 1;
        if (mergeFile) {
          highestLabel = -1;
          for (i = 0; i < mItemArray.GetSize(); i++) {
            prev = mItemArray.GetAt(i);
            if (prev->mMapID == item->mMapID && (item->mMapID || 
              (fabs((double)prev->mStageX - item->mStageX) < 1.e-5 &&
              fabs((double)prev->mStageY - item->mStageY) < 1.e-5))) {
                index = 0;
                break;
            }
            if (prev->mLabel == item->mLabel) {
              highestLabel = B3DMAX(highestLabel, 0);
            } else if (prev->mLabel.Find(item->mLabel + '-') == 0) {
              ind1 = item->mLabel.GetLength() + 1;
              ind2 = prev->mLabel.GetLength();
              if (ind2 > ind1) {
                str = prev->mLabel.Right(ind2 - ind1);
                ind1 = atoi((LPCTSTR)str);
                highestLabel = B3DMAX(highestLabel, ind1);
              }
            }
          }
          if (index && highestLabel >= 0)
            item->mLabel.Format("%s-%d", (LPCTSTR)item->mLabel, highestLabel + 1);
        }

        // Add to array and get new item number past highest number in a label
        if (index) {
          if (numExternal && !item->mMapID)
            item->mMapID = MakeUniqueID();
          mItemArray.InsertAt(addIndex++, item);
          i = atoi((LPCTSTR)item->mLabel) + 1;
          mNewItemNum = B3DMAX(mNewItemNum, i);
          if (item->mMapID)
            mSetOfIDs.insert(item->mMapID);
          if (item->mGroupID)
            mSetOfIDs.insert(item->mGroupID);
          if (item->mShiftCohortID)
            mSetOfIDs.insert(item->mShiftCohortID);
        }
      } else {

        // Upon any error for an item, toss it out and increment error count
        delete item;
        numItemErr += (numAdocErr ? 1 : 0);
        numItemLack += (numLackRequired ? 1 : 0);
        numExtErr += externalErr;
      }
    }
  }
  catch(CFileException *perr) {
    TCHAR szCause[255];
    perr->GetErrorMessage(szCause, 255);
    perr->Delete();
    str = "Error reading from file " + mNavFilename + ":" + szCause;
    SEMMessageBox(str, MB_EXCLAME);
    returnVal = 1;
  } 
  if (adocIndex >= 0) {
    AdocClear(adocIndex);
    AdocReleaseMutex();
  } else if (cFile) {
    cFile->Close();
    delete cFile;
  }
  mHelper->CleanupFromExternalFileAccess();

  // Give error summaries
  if (numItemErr + numItemLack + numExtErr > 0) {
    str.Format("%d items could not be read in from Navigator file:\r\n"
      "   %d items gave an error getting the data\r\n"
      "   %d items were missing a required piece of data", numItemErr + numItemLack + 
      numExtErr, numItemErr, numItemLack);
    if (numExtErr > 0) {
      str2.Format("\r\n   %d externally added items with a processing error (see log for"
        " details)", numExtErr);
      str += str2;
      PrintfToLog("\r\nSummary of errors in processing externally added items:");
      for (i = 0; i < 16; i++)
        if (extErrCounts[i])
          PrintfToLog("  %d item(s) with %s", extErrCounts[i], extErrMess[i]);
      if (GetDebugOutput('n'))
        PrintfToLog(" ");
      else
        PrintfToLog("Set Debug Output to \"n\" for details on each item\r\n");
    } 
    SEMMessageBox(str, MB_EXCLAME);
    returnVal = 1;
  }

  if (filesNotFound.size()) {
    str = "The following map file(s) were not found either in \r\n"
      "their original location or with the Navigator file:\r\n\r\n";
    for (fileIter = filesNotFound.begin(); fileIter != filesNotFound.end(); fileIter++)
      str += CString(fileIter->c_str()) + "\r\n";
    str +=  "\r\nTo access these maps, move them into the directory with the current"
      " Navigator file";
    mWinApp->AppendToLog(str);
    SEMMessageBox(str, MB_EXCLAME);
    returnVal = 1;
  }

  // Set up list box and counters
  SetCurrentRegFromMax();
  SetRegPtNum(GetFreeRegPtNum(mCurrentRegistration, 0));
  if (m_bCollapseGroups)
    MakeListMappings();
  FillListBox(false, mergeFile);
  Redraw();
  mChanged = mergeFile;
  if (resetAcquireEnd) {
    mEndingAcquireIndex = (int)mItemArray.GetSize() - 1;
    mHelper->CountAcquireItems(originalSize, mEndingAcquireIndex, numSect, numToGet);
    mInitialNumAcquire += (mAcqParm->acquireType == ACQUIRE_DO_TS ? numToGet : numSect);
  }
  return returnVal;
}

// Get the filename for reading or writing
int CNavigatorDlg::GetNavFilename(BOOL openFile, DWORD flags, bool mergeFile)
{
  CString str, str2;
  static char BASED_CODE szFilter[] = 
    "Navigator files (*.nav)|*.nav|All files (*.*)|*.*||";
  MyFileDialog fileDlg(openFile, ".nav", NULL, flags, szFilter);
  int result = fileDlg.DoModal();
  mWinApp->RestoreViewFocus();
  if (result != IDOK)
    return 1;
  mMergeName = fileDlg.GetPathName();
  if (mMergeName.IsEmpty())
    return 1;
  if (mergeFile)
    return 0;
  SetCurrentNavFile(mMergeName);
  return 0;
}

// Get a filename for reading stock items; initialize to last file
int CNavigatorDlg::GetStockFilename(void)
{
  static char BASED_CODE szFilter[] = 
    "Navigator files (*.nav)|*.nav|All files (*.*)|*.*||";
  LPCTSTR lpszFileName = NULL;
  if (!mParam->stockFile.IsEmpty())
    lpszFileName = mParam->stockFile;
  MyFileDialog fileDlg(TRUE, ".nav", lpszFileName, OFN_HIDEREADONLY, szFilter);
  int result = fileDlg.DoModal();
  mWinApp->RestoreViewFocus();
  if (result != IDOK)
    return 1;
  mParam->stockFile = fileDlg.GetPathName();
  if (mParam->stockFile.IsEmpty())
    return 1;
  return 0;
}

// Get next field of file
CString CNavigatorDlg::NextTabField(CString inStr, int &index)
{
  CString str = "";
  if (index >= inStr.GetLength())
    return str;
  int start = index;// ? index - 1 : 0;
  int next = inStr.Find('\t', start);
  if (next < 0)
    next = inStr.GetLength();
  str = inStr.Mid(index, next - index);
  index = next + 1;
  return str;
}

/////////////////////////////////////////////////////////////////////
// ACQUIRING SEQUENCE OF MARKED AREAS AND MAKING MAPS / RUNNING MACRO / DOING TILT SERIES

// Initiate Acquisition
void CNavigatorDlg::AcquireAreas(bool fromMenu, bool dlgClosing)
{
  int loop, ind, loopStart, loopEnd, macnum, numNoMap = 0, numAtEdge = 0;
  int groupID = -1;
  BOOL rangeErr = false;
  BOOL runPremacro, runPostmacro;
  bool takingMap, takingImage, doingMultishot;
  CString *macros = mWinApp->GetMacros();
  CString mess, mess2;
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  CNavAcquireDlg *dlg;
  if (!dlgClosing && mNavAcquireDlg) {
    if (fromMenu)
      mNavAcquireDlg->BringWindowToTop();
    else
      SEMMessageBox("You cannot start Navigator acquisition with the acquire dialog "
        "open");
    return;
  }

  if (!dlgClosing) {
    mNavAcquireDlg = new CNavAcquireDlg();
    dlg = mNavAcquireDlg;

    // If postponed, set subset from postponed values; otherwise initialize to no subset
    dlg->mNumArrayItems = (int)mItemArray.GetSize();
    if (mAcqDlgPostponed) {
      dlg->m_iSubsetStart = B3DMIN(mPostponedSubsetStart, dlg->mNumArrayItems);
      dlg->m_iSubsetEnd = B3DMIN(mPostponedSubsetEnd, dlg->mNumArrayItems);
      dlg->m_bDoSubset = mPostposedDoSubset;
    } else {
      dlg->m_iSubsetStart = 1;
      dlg->m_iSubsetEnd = dlg->mNumArrayItems;
    }
    dlg->mLastNonTStype = mAcqParm->nonTSacquireType;
    EvaluateAcquiresForDlg(dlg);


    // Run the dialog
    if (fromMenu) {
      dlg->m_iAcquireType = mAcqParm->nonTSacquireType;
      if (dlg->mAnyTSpoints)
        dlg->m_iAcquireType = ACQUIRE_DO_TS;
      mNavAcquireDlg->Create(IDD_NAVACQUIRE);
      mWinApp->SetPlacementFixSize(mNavAcquireDlg, 
        mHelper->GetAcquireDlgPlacement(false));
      mWinApp->RestoreViewFocus();
      ManageCurrentControls();
      mWinApp->UpdateBufferWindows();
      return;
    } else if ((mAcqParm->acquireType == ACQUIRE_DO_TS && !dlg->mAnyTSpoints) ||
      (mAcqParm->acquireType != ACQUIRE_DO_TS && !dlg->mAnyAcquirePoints)) {
      ManageAcquireDlgCleanup(fromMenu, dlgClosing);
      return;
    }

  } else {

    // When dialog closing, process results
    dlg = mNavAcquireDlg;
    if (dlg->mPostponed < 0 || (!dlg->mAnyAcquirePoints && !dlg->mAnyTSpoints)) {
      ManageAcquireDlgCleanup(fromMenu, dlgClosing);
      return;
    }

    if (dlg->m_iAcquireType != ACQUIRE_DO_TS)
      mAcqParm->nonTSacquireType = dlg->m_iAcquireType;
    mAcqParm->acquireType = dlg->m_iAcquireType;

    // Postponing: save subset parameters
    mAcqDlgPostponed = dlg->mPostponed > 0;
    if (mAcqDlgPostponed) {
      mPostponedSubsetStart = dlg->m_iSubsetStart;
      mPostponedSubsetEnd = dlg->m_iSubsetEnd;
      mPostposedDoSubset = dlg->m_bDoSubset;
      ManageAcquireDlgCleanup(fromMenu, dlgClosing);
      return;
    }
  }
 
  runPremacro = mAcqParm->runPremacro;
  if (mAcqParm->acquireType != ACQUIRE_DO_TS)
    runPremacro = mAcqParm->runPremacroNonTS;
  runPostmacro = mAcqParm->runPostmacro;
  if (mAcqParm->acquireType != ACQUIRE_DO_TS)
    runPostmacro = mAcqParm->runPostmacroNonTS;
  setOrClearFlags(&mAcqActions[NAACT_RUN_PREMACRO].flags, NAA_FLAG_RUN_IT, runPremacro);
  setOrClearFlags(&mAcqActions[NAACT_RUN_POSTMACRO].flags, NAA_FLAG_RUN_IT, runPostmacro);

  if (mAcqParm->skipInitialMove && OKtoSkipStageMove(mAcqParm) != 0) {
    loop = IDYES;
    if (mParam->warnedOnSkipMove)
      mWinApp->AppendToLog("WARNING: Skipping stage move when possible; relying on your"
        " script to move stage or run Realign to Item");
    else 
      loop = AfxMessageBox("WARNING: You have selected to skip the stage move to "
      "item when possible.  Your script must either run Realign to Item or do the stage "
        "move explicitly.\n\nAre you sure you want to proceed?", MB_QUESTION);
    mParam->warnedOnSkipMove = true;
    if (loop != IDYES) {
      ManageAcquireDlgCleanup(fromMenu, dlgClosing);
      return;
    }
  }
  mSkipStageMoveInAcquire = mAcqParm->skipInitialMove;

  // Set up for macro, or check file if images
  if (mAcqParm->acquireType == ACQUIRE_RUN_MACRO || runPremacro || runPostmacro) {
    loopStart = mAcqParm->acquireType == ACQUIRE_RUN_MACRO ? 0 : 1;
    loopEnd = (runPremacro || runPostmacro) ? 3 : 1;
    for (loop = loopStart; loop < loopEnd; loop++) {
      macnum = -1;
      if (loop == 1 && runPremacro)
        macnum = (mAcqParm->acquireType == ACQUIRE_DO_TS ? mAcqParm->preMacroInd : 
          mAcqParm->preMacroIndNonTS) - 1;
      if (loop == 2 && runPostmacro)
        macnum = (mAcqParm->acquireType == ACQUIRE_DO_TS ? mAcqParm->postMacroInd : 
          mAcqParm->postMacroIndNonTS) - 1;
      if (loop == 0)
        macnum = mAcqParm->macroIndex - 1;
      if (macnum >= 0 && mMacroProcessor->EnsureMacroRunnable(macnum)) {
        ManageAcquireDlgCleanup(fromMenu, dlgClosing);
        return;
      }
    }

    // Set to nonresumable so it will not look like it is paused and we can go on
    mMacroProcessor->SetNonResumable();
  } 
  
  // Set convenience flag for early return and check conditions
  takingMap = mAcqParm->acquireType == ACQUIRE_TAKE_MAP;
  takingImage = mAcqParm->acquireType == ACQUIRE_IMAGE_ONLY;
  doingMultishot = mAcqParm->acquireType == ACQUIRE_MULTISHOT;
  mDoingEarlyReturn = (takingMap || takingImage) && camParams->K2Type &&
    !mWinApp->Montaging() && mAcqParm->earlyReturn;
  if (takingMap && mDoingEarlyReturn && !mAcqParm->numEarlyFrames) {
    AfxMessageBox("You must have a non-zero value for the number\n"
      "of early return frames when acquiring maps", MB_EXCLAME);
    ManageAcquireDlgCleanup(fromMenu, dlgClosing);
    return;
  }

  // If there is no file open for regular acquire, make sure one will be opened on 
  // first item
  mSkippingSave = (mDoingEarlyReturn && !mAcqParm->numEarlyFrames) ||
    mAcqParm->skipSaving;
  if (dlg->mNumAcqBeforeFile && ((!mWinApp->mStoreMRC && !mSkippingSave && 
    (takingMap || takingImage)) || ((!mWinApp->mStoreMRC || mWinApp->Montaging()) &&
        doingMultishot && mHelper->IsMultishotSaving()))) {
      AfxMessageBox("There is no open " + CString(doingMultishot ? "single-frame " : "")
        + "image file and\nno file set to open on first acquire item.", MB_EXCLAME);
      ManageAcquireDlgCleanup(fromMenu, dlgClosing);
      return;
  }

  // Now if realigning, make sure there are suitable maps; and check for mismatches
  // in camera between state and parameters, or for changes in binning
  // Set acquire on first so the map finding assumes the correct low dose area
  mAcquireIndex = 0;
  mEndingAcquireIndex = dlg->mNumArrayItems - 1;
  if (dlg->m_bDoSubset) {
    mAcquireIndex = B3DMAX(0, dlg->m_iSubsetStart - 1);
    mEndingAcquireIndex = B3DMIN(mEndingAcquireIndex, dlg->m_iSubsetEnd - 1);
  }
  if (mHelper->AssessAcquireProblems(mAcquireIndex, mEndingAcquireIndex)) {
    mAcquireIndex = -1;
    ManageAcquireDlgCleanup(fromMenu, dlgClosing);
    return;
  }
  mStartingAcquireIndex = mAcquireIndex;

  // done with dialog
  ManageAcquireDlgCleanup(fromMenu, dlgClosing);

  // Initialize the action run-time data
  mAcquireCounter = 0;
  for (ind = 0; ind < mHelper->GetNumAcqActions(); ind++) {
    mAcqActions[ind].lastDoneAtX = -10000.;
    mAcqActions[ind].lastDoneAtY = -10000.;
    mAcqActions[ind].timeLastDone = -1;
  }
  if (mAcqParm->cycleDefocus)
    mFocusCycleCounter = 0;

  mAcqStepIndex = -1;

  mSavedTargetDefocus = mWinApp->mFocusManager->GetTargetDefocus();
  mNumAcquired = 0;
  mAcquireEnded = 0;
  mPausedAcquire = false;
  mResumedFromPause = false;
  mEmailWasSent = false;
  mResetAcquireIndex = false;
  mLastGroupID = 0;
  mGroupIDtoSkip = 0;
  mSelectedItems.clear();
  mSaveCollapsed = m_bCollapseGroups;
  SetCollapsing(false);
  if (FindAndSetupNextAcquireArea()) {
    mAcquireIndex = -1;
    mFocusCycleCounter = -1;
    SetCollapsing(mSaveCollapsed);
    return;
  }

  mHelper->CountAcquireItems(mAcquireIndex, mEndingAcquireIndex, loop, loopEnd);
  mInitialNumAcquire = mAcqParm->acquireType == ACQUIRE_DO_TS ? loopEnd : loop;
  mElapsedAcqTime = 0.;
  mLastAcqDoneTime = GetTickCount();
  mLastAcquireTicks = mLastAcqDoneTime;
  mNumDoneAcq = 0;
  mLastMontLeft = -1.;
  if (!mScope->FastColumnValvesOpen()) {
    mWinApp->AppendToLog("Opening valves for acquisition!");
    mScope->SetColumnValvesOpen(true, false);
  }

  if (mAcqParm->acquireType != ACQUIRE_RUN_MACRO) {
    mSaveAlignOnSave = mBufferManager->GetAlignOnSave();
    mSaveProtectRecord = mBufferManager->GetConfirmBeforeDestroy(3);
    mBufferManager->SetAlignOnSave(false);
    mBufferManager->SetConfirmBeforeDestroy(3, 0);
    mWinApp->SetStatusText(COMPLEX_PANE, "ACQUIRING AREAS");
  }
  mWinApp->UpdateBufferWindows();
  AcquireNextTask(0);
}

// Function for cleaning up from either dialog closing or working with temporary dialog
void CNavigatorDlg::ManageAcquireDlgCleanup(bool fromMenu, bool dlgClosing)
{
  if (!fromMenu && !dlgClosing)
    delete mNavAcquireDlg;
  mNavAcquireDlg = NULL;
  if (dlgClosing) {
    mWinApp->UpdateBufferWindows();
    ManageCurrentControls();
  }
}

// Perform next task in acquisition sequence
void CNavigatorDlg::AcquireNextTask(int param)
{
  int err, len, zval, act, ind, next, after, timeOut = 0, elsewhereInd = -100;
  int stopErr = 0;
  unsigned char lastChar;
  CString report, str;
  bool skippingGroup;
  float hours, target, oldTarget;
  double ISX, ISY, ticks;
  BOOL runIt, vppNearestSave;
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  CMapDrawItem *item, *nextItem;
  TiltSeriesParam *tsp;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  MultiShotParams *msParams = mHelper->GetMultiShotParams();
  DriftWaitParams *dwParam = mWinApp->mParticleTasks->GetDriftWaitParams();
  VppConditionParams *vppParams = mWinApp->mMultiTSTasks->GetVppConditionParams();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  NavAlignParams *aliParams = mHelper->GetNavAlignParams();
  DewarVacParams *dvParams = mScope->GetDewarVacParams();
  mMovingStage = false;
  mStartedTS = false;
  int *actOrder = mHelper->GetAcqActCurrentOrder(mHelper->GetCurAcqParamIndex());
  const char *stepNames[ACQ_MAX_STEPS] = {"", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "Setup next item", "Open file if needed", "Acquire image/montage",
    "Do multishot", "Run script", "Tilt series", "Move to item", "Manage backlash",
    "Move to other item", "Relax stage"};

  if (mAcquireIndex < 0)
    return;
  item = mItemArray[mAcquireIndex];

  if (mAcqStepIndex < 0) {

    // SET UP THE SEQUENCE
    mNumAcqSteps = 0;
    mStepIndForDewarCheck = -1;
    mDidEucentricity = false;
    mWillDoTemplateAlign = false;
    for (after = 0; after < 2; after++) {

      // Do final things before primary action, at start of second loop
      if (after) {

        // Check for adding a stage relaxation after last move unless currently elsewhere
        if (mAcqParm->relaxStage && elsewhereInd == -1) {
          for (ind = mNumAcqSteps - 1; ind >= 0; ind--) {
            if (mAcqSteps[ind] == ACQ_MOVE_TO_AREA ||
              mAcqSteps[ind] == NAACT_REALIGN_ITEM || 
              (mAcqSteps[ind] == NAACT_ALIGN_TEMPLATE && aliParams->maxNumResetIS > 0)) {
              for (next = mNumAcqSteps - 1; next > ind; next--)
                mAcqSteps[next + 1] = mAcqSteps[next];
              mNumAcqSteps++;
              mAcqSteps[ind + 1] = ACQ_RELAX_STAGE;
              break;
            }
          }
        }

        // Add another dewar/PVP check if it is being done, it was not just added, 
        // flag is set to do checks before task and one of the checks is selected
        // Record the index where it should just check
        if (DOING_ACTION(NAACT_CHECK_DEWARS) && (!mNumAcqSteps ||
          mAcqSteps[mNumAcqSteps - 1] != NAACT_CHECK_DEWARS) && 
          dvParams->doChecksBeforeTask && (dvParams->checkDewars || dvParams->checkPVP)) {
          mStepIndForDewarCheck = mNumAcqSteps;
          mAcqSteps[mNumAcqSteps++] = NAACT_CHECK_DEWARS;
        }

        // Move stage if it is elsewhere, unless skipping is selected and the primary task
        // is a script.  Not calling OKtoSkipStageMove with its current simple form
        if (elsewhereInd != -1 && !(mAcqParm->skipInitialMove && 
          mAcqParm->acquireType == ACQUIRE_RUN_MACRO)) {
          mAcqSteps[mNumAcqSteps++] = ACQ_MOVE_TO_AREA;
          elsewhereInd = -1;
          if (mAcqParm->relaxStage)
            mAcqSteps[mNumAcqSteps++] = ACQ_RELAX_STAGE;
        }

        //Open file if needed and run the primary task
        mAcqSteps[mNumAcqSteps++] = ACQ_OPEN_FILE;
        if (mAcqParm->acquireType == ACQUIRE_RUN_MACRO)
          mAcqSteps[mNumAcqSteps++] = ACQ_RUN_MACRO;
        else if (mAcqParm->acquireType == ACQUIRE_DO_TS)
          mAcqSteps[mNumAcqSteps++] = ACQ_START_TS;
        else if (mAcqParm->acquireType == ACQUIRE_MULTISHOT)
          mAcqSteps[mNumAcqSteps++] = ACQ_DO_MULTISHOT;
        else
          mAcqSteps[mNumAcqSteps++] = ACQ_ACQUIRE;
        if (mAcqParm->acquireType == ACQUIRE_TAKE_MAP)
          mAcqSteps[mNumAcqSteps++] = ACQ_BACKLASH;

      }

      // Loop on the actions, selecting the ones to run before the first time, after the 
      // second time
      for (ind = 0; ind < mHelper->GetNumAcqActions(); ind++) {
        act = actOrder[ind];
        if (!BOOL_EQUIV((mAcqActions[act].flags & NAA_FLAG_AFTER_ITEM) != 0, after > 0) ||
          !DOING_ACTION(act))
          continue;
        runIt = false;

        // See if it is to be run for this item
        switch (mAcqActions[act].timingType) {

        case NAA_EVERY_N_ITEMS:
          runIt = mAcqActions[act].everyNitems < 2 ||
            mAcquireCounter % mAcqActions[act].everyNitems == 0;
          break;

        case NAA_GROUP_START:
          runIt = mFirstInGroup;
          break;

        case NAA_GROUP_END:
          nextItem = NULL;
          for (next = mAcquireIndex + 1; next <= mEndingAcquireIndex; next++) {
            nextItem = mItemArray[next];
            if (IsItemToBeAcquired(nextItem, skippingGroup))
              break;
          }
          runIt = nextItem && nextItem->mGroupID != item->mGroupID;
          break;

        case NAA_AFTER_TIME:

          // Hardware dark reference and FEG flashing times are kept track of by system
          if (act == NAACT_HW_DARK_REF || (JEOLscope && act == NAACT_FLASH_FEG)) {
            runIt = true;

            // Refine ZLP has its own time stamp
          } else if (act == NAACT_REFINE_ZLP) {
            if (mWinApp->GetEFTEMMode() && 
              SEMTickInterval(1000. * filtParam->alignZLPTimeStamp) > 60000. * 
              mAcqActions[act].minutes)
            runIt = true;

            // Everything else, the time is kept track of here
          } else if (mAcqActions[act].timeLastDone < 0 || mWinApp->MinuteTimeStamp() -
            mAcqActions[act].timeLastDone >= mAcqActions[act].minutes) {
            runIt = true;
            mAcqActions[act].timeLastDone = mWinApp->MinuteTimeStamp();
          }
          break;

        case NAA_IF_SEPARATED:
          if (sqrt(pow(item->mStageX - mAcqActions[act].lastDoneAtX, 2.f) +
            pow(item->mStageY - mAcqActions[act].lastDoneAtY, 2.f)) >=
            mAcqActions[act].distance) {
            runIt = true;
            mAcqActions[act].lastDoneAtX = item->mStageX;
            mAcqActions[act].lastDoneAtY = item->mStageY;
          }
          break;

        }

        // If running it, now see if it is done elsewhere and if it is not already there,
        // move there; if not to be done elsewhere and still elsewhere, go to area
        if (runIt) {

          // Any site OK, do not change location
          if (mAcqActions[act].flags & NAA_FLAG_ANY_SITE_OK) {

            // Realign to item DOES change to position
          } else if (act == NAACT_REALIGN_ITEM) {
            elsewhereInd = -1;

            // Other site, check if same and set it up if not
          } else if (mAcqActions[act].flags & NAA_FLAG_OTHER_SITE) {
            next = mHelper->FindNearestItemMatchingText(item->mStageX, item->mStageY,
              mAcqActions[act].labelOrNote,
              (mAcqActions[act].flags & NAA_FLAG_MATCH_NOTE) != 0);
            if (next != elsewhereInd) {
              mAcqSteps[mNumAcqSteps++] = ACQ_MOVE_ELSEWHERE;
              elsewhereInd = next;
            }

            // But if doing VPP, no other site specified here, but vpp params have a
            // different site, mark it as elsewhere, non-matching, and it will do the move
          } else if (act == NAACT_CONDITION_VPP && vppParams->useNearestNav) {
            elsewhereInd = 99;

            // If skipping selected and this is the ONLY macro being run, have to trust
            // them and say it is now moved to position
          } else if (act == NAACT_RUN_PREMACRO && mAcqParm->acquireType != ACQ_RUN_MACRO
            && mAcqParm->skipInitialMove) {
            elsewhereInd = -1;

            // Otherwise move to area if necessary 
          } else if (elsewhereInd != -1 && act != NAACT_REALIGN_ITEM) {
            mAcqSteps[mNumAcqSteps++] = ACQ_MOVE_TO_AREA;
            elsewhereInd = -1;
          }
          if (act == NAACT_ALIGN_TEMPLATE)
            mWillDoTemplateAlign = true;
          mAcqSteps[mNumAcqSteps++] = act;
        }
      }

      // At the very end, find next item if any
      if (after)
        mAcqSteps[mNumAcqSteps++] = ACQ_FIND_SETUP_NEXT;
    }

    if (GetDebugOutput('n')) {
      for (ind = 0; ind < mNumAcqSteps; ind++) {
        mWinApp->AppendToLog(mAcqSteps[ind] < mHelper->GetNumAcqActions() ?
          mAcqActions[mAcqSteps[ind]].name : stepNames[ind]);
      }
    }

    // Handle defocus target cycling
    if (mAcqParm->cycleDefocus) {
      target = (float)(mAcqParm->cycleDefFrom + (mAcqParm->cycleDefTo - 
        mAcqParm->cycleDefFrom) * mFocusCycleCounter / mAcqParm->cycleSteps);
      oldTarget = mWinApp->mFocusManager->GetTargetDefocus();
      mWinApp->mFocusManager->SetTargetDefocus(target);
      mScope->IncDefocus(target - oldTarget);
      mFocusCycleCounter = (mFocusCycleCounter + 1) % (mAcqParm->cycleSteps + 1);
      PrintfToLog("Target defocus set to %.2f um and scope defocus changed by %.2f", 
        target, target - oldTarget);
    }

  } else {

    // PROCESS THE PRECEDING ACTION
    switch (mAcqSteps[mAcqStepIndex]) {

      // After acquisition
    case ACQ_ACQUIRE:
      ManageNumDoneAcquired();
      RestoreBeamTiltIfSaved();
      if (!mSkippingSave) {
        if (mWinApp->Montaging()) {
          zval = mWinApp->mMontageWindow.m_iCurrentZ - 1;
        } else {
          if (!mBufferManager->IsBufferSavable(mImBufs)) {
            StopAcquiring();
            SEMMessageBox("This image cannot be saved to the currently open file.\n\n"
              "Switch to a different file");
            return;
          }
          if (mBufferManager->CheckAsyncSaving())
            return;

          zval = mWinApp->mStoreMRC->getDepth();
          if (mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC)) {
            StopAcquiring();
            return;
          }
        }
      }
      report.Format("%s acquired at item # %d with label %s",
        mAcqParm->acquireType == ACQUIRE_TAKE_MAP ? "Map" : "Image", mAcquireIndex + 1,
        (LPCTSTR)item->mLabel);
      if (!mSkippingSave) {
        str.Format(" saved at Z = %d", zval);
        report += str;
      }
      ticks = GetTickCount();
      str.Format("  (in %.1f sec)", 0.001 * SEMTickInterval(ticks, mLastAcquireTicks));
      report += str;
      mLastAcquireTicks = ticks;
      if (mAcqParm->acquireType != ACQUIRE_TAKE_MAP)
        report += "\r\n----------------------------------------------------------------";
      mWinApp->AppendToLog(report);

      item->mDraw = mAcqParm->acquireType != ACQUIRE_TAKE_MAP;
      SetItemSuccessfullyAcquired(item);
      if (mAcqParm->acquireType != ACQUIRE_TAKE_MAP)
        break;

      mSkipBacklashType = 2;
      if (NewMap()) {
        StopAcquiring();
        return;
      }
      SetCurrentItem();
      mItem->mLabel = item->mLabel;
      len = item->mLabel.GetLength();
      lastChar = item->mLabel[len - 1];
      if (len > 1 && item->mLabel[len - 2] == '-' && lastChar >= 'A' && lastChar < 'Z')
        mItem->mLabel.SetAt(len - 1, lastChar + 1);
      else
        mItem->mLabel += "-A";
      mItem->mNote = mItem->mNote + " - " + item->mNote;
      m_strLabel = mItem->mLabel;
      UpdateListString(mCurrentItem);
      break;

      // After a script
    case ACQ_RUN_MACRO:
      ManageNumDoneAcquired();
      if (mMacroProcessor->GetLastCompleted()) {
        SetItemSuccessfullyAcquired(item);

        // Allow acquisition to go on after failure if the flag is set
      } else if (mMacroProcessor->GetLastAborted() &&
        !mMacroProcessor->GetNoMessageBoxOnError()) {
        mAcquireEnded = 1;
      } else
        mAcquireIndex++;
      break;

      // After a tilt series
    case ACQ_START_TS:
      ManageNumDoneAcquired();

      // Count any success for now
      if (mWinApp->mTSController->GetLastSucceeded()) {
        mNumAcquired++;
        report.Format("Tilt series acquired at item # %d with label %s (item at %.2f, %.2f,"
          " TS at %.2f, %.2f)", mAcquireIndex + 1, (LPCTSTR)item->mLabel, item->mStageX,
          item->mStageY, mWinApp->mTSController->GetStopStageX(),
          mWinApp->mTSController->GetStopStageY());
        mWinApp->AppendToLog(report);
        item->mTargetDefocus = EXTRA_NO_VALUE;
      }
      break;

      // After a multishot
    case ACQ_DO_MULTISHOT:
      ManageNumDoneAcquired();
      err = mWinApp->mParticleTasks->GetWDLastFailed();
      if (!err || !mRetValFromMultishot)
        SetItemSuccessfullyAcquired(item);
      if (mRetValFromMultishot) {
        ticks = GetTickCount();
        PrintfToLog("%d Multiple Records at item # %d with label %s %s   (in %.1f sec)"
          "\r\n-------------------------------------------------------------------------",
          -mRetValFromMultishot, mAcquireIndex + 1, (LPCTSTR)item->mLabel,
          err ? " FAILED before completion" :
          "successfully completed", 0.001 * SEMTickInterval(ticks, mLastAcquireTicks));
        mLastAcquireTicks = ticks;

        // Stop or go on after error just like macro
        if (err && !mAcqParm->noMBoxOnError) {
          mAcquireEnded = 1;
        } else
          mAcquireIndex++;
      }
      break;

      // After pre-macro, if it set the skip flag, skip through to move
      // step; increase acquire index because it is still marked as acquire
      // The help says failure does not matter... not sure if this is a good idea
    case NAACT_RUN_PREMACRO:
      if (mSkipAcquiringItem) {
        SkipToNextItemInAcquire(item, "in pre-script");
      }
      break;

      // This is the next step when resuming from a pause; back up and set it up again
    case ACQ_FIND_SETUP_NEXT:
      if (mResumedFromPause) {
        mAcqStepIndex--;
        mLastAcqDoneTime = GetTickCount();
        if (mAcqParm->acquireType != ACQUIRE_RUN_MACRO)
          mWinApp->SetStatusText(COMPLEX_PANE, "ACQUIRING AREAS");
      }
      mResumedFromPause = false;
      break;

      // Handle failed autofocus by ending or skipping to next item, an abort will not end
    case NAACT_AUTOFOCUS:
      if (mWinApp->mFocusManager->GetLastFailed() && !mAcqParm->noMBoxOnError) {
        mAcquireEnded = 1;
      } else if (mWinApp->mFocusManager->GetLastFailed() ||
        mWinApp->mFocusManager->GetLastAborted()) {
        SkipToNextItemInAcquire(item, "in autofocus");
      }
      break;

      // Handle other failures similarly
    case NAACT_ALIGN_TEMPLATE:
      if (mWinApp->mParticleTasks->GetATLastFailed()) {
        SkipToNextItemInAcquire(item, "aligning to template");
      }
      break;

    case NAACT_WAIT_DRIFT:
      err = mWinApp->mParticleTasks->GetWDLastFailed();
      if (err && err != DW_EXTERNAL_STOP)
        SkipToNextItemInAcquire(item, "waiting for drift");
      break;

      // Record that eucentricity was done
    case NAACT_EUCEN_BY_FOCUS:
    case NAACT_ROUGH_EUCEN:
    case NAACT_FINE_EUCEN:
      mDidEucentricity = true;
      break;

    default:
      break;
    }
  }

  // NOW DO THE NEXT ACTION
  mSkipAcquiringItem = false;
  switch (mAcqSteps[++mAcqStepIndex]) {

    // Rough eucentricity
  case NAACT_ROUGH_EUCEN:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    mWinApp->mComplexTasks->FindEucentricity(FIND_EUCENTRICITY_COARSE);
    break;

    // Run the realign routine; restore state based on flag for macro, otherwise restore
    // only when outside low dose
  case NAACT_REALIGN_ITEM:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    mRealignedInAcquire = true;
    err = mHelper->RealignToItem(item, !mWinApp->LowDoseMode(),
      mWillDoTemplateAlign ? 0.f : aliParams->resetISthresh, 
      mWillDoTemplateAlign ? 0 : aliParams->maxNumResetIS, 
      mWillDoTemplateAlign ? false : aliParams->leaveISatZero, 
      mAcqParm->hybridRealign && mWillDoTemplateAlign && 
      mAcqActions[NAACT_ALIGN_TEMPLATE].timingType == NAA_EVERY_N_ITEMS && 
      mAcqActions[NAACT_ALIGN_TEMPLATE].everyNitems == 1);
    if (err && (err < 4 || err == 6)) {
      StopAcquiring();
      if (err < 4)
        SEMMessageBox("An error occurred trying to access a map image file");
      return;
    }
    if (err) {
      report.Format("Item %s could not be realigned to by correlating with another map."
        "\r\n    Images will be acquired at the nominal stage position without any "
        "alignment", (LPCTSTR)item->mLabel);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
    break;

    // Align to template
  case NAACT_ALIGN_TEMPLATE:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    mRealignedInAcquire = true;
    stopErr = mWinApp->mParticleTasks->AlignToTemplate(*aliParams);
    break;

  // Cook specimen
  case NAACT_COOK_SPEC:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    stopErr = mWinApp->mMultiTSTasks->StartCooker();
    break;

    // Fine eucentricity
  case NAACT_FINE_EUCEN:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    mWinApp->mComplexTasks->FindEucentricity(
      FIND_EUCENTRICITY_FINE | (mRealignedInAcquire ? REFINE_EUCENTRICITY_ALIGN : 0));
    break;

    // Eucentricity by focus
  case NAACT_EUCEN_BY_FOCUS:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    stopErr = mWinApp->mParticleTasks->EucentricityFromFocus(-1);
    break;

    // Center beam
  case NAACT_CEN_BEAM:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    stopErr = mWinApp->mMultiTSTasks->AutocenterBeam();
    break;

    // Autofocus
  case NAACT_AUTOFOCUS:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    if (mAcqParm->focusChangeLimit > 0.)
      mWinApp->mFocusManager->NextFocusChangeLimits(-mAcqParm->focusChangeLimit, 
        mAcqParm->focusChangeLimit);
    mWinApp->mFocusManager->AutoFocusStart(1);
    break;

    // Coma-free alignment
  case NAACT_COMA_FREE:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    stopErr = mWinApp->mAutoTuning->CtfBasedAstigmatismComa(
      mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1, false, 0, false,
      mAcqParm->noMBoxOnError);
    break;

    // Astigmatism correction
  case NAACT_ASTIGMATISM:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    if (mAcqParm->astigByBTID)
      stopErr = mWinApp->mAutoTuning->FixAstigmatism(true);
    else
      stopErr = mWinApp->mAutoTuning->CtfBasedAstigmatismComa(0, false, 0, false,
        mAcqParm->noMBoxOnError);
    break;

    // Conditioning a phase plate
  case NAACT_CONDITION_VPP:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    vppNearestSave = vppParams->useNearestNav;
    if (mAcqActions[NAACT_CONDITION_VPP].flags & NAA_FLAG_OTHER_SITE)
      vppParams->useNearestNav = false;
    stopErr = mWinApp->mMultiTSTasks->ConditionPhasePlate(true);
    vppParams->useNearestNav = vppNearestSave;
    break;

    // Dark ref from DE or K2/3
  case NAACT_HW_DARK_REF:
    hours = (float)(mAcqActions[NAACT_HW_DARK_REF].minutes / 60.);
    if (camParams->DE_camType) {
      stopErr = mWinApp->mGainRefMaker->MakeDEdarkRefIfNeeded(-1, hours, report);
      if (stopErr)
        SEMMessageBox("Cannot make a new dark reference in DE server:\n" + report);
      else
        SEMTrace('n', "%saking DE dark reference",
          mWinApp->mGainRefMaker->AcquiringGainRef() ? "M" : "NOT m");
    } else if (camParams->K2Type) {
      err = mCamera->UpdateK2HWDarkRef(hours);
      if (err == 1) {
        SEMMessageBox("The thread for updating a dark reference is already busy");
        StopAcquiring();
        return;
      }
      SEMTrace('n', "%spdating HW dark reference", err ? "NOT u" : "U");
    }
    break;

    // Wait for drift
  case NAACT_WAIT_DRIFT:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    err = mWinApp->mParticleTasks->WaitForDrift(*dwParam, false, 0.,
      mAcqParm->focusChangeLimit);
    stopErr = B3DMAX(0, err);
    break;

    // Refine the ZLP
  case NAACT_REFINE_ZLP:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    stopErr = mWinApp->mFilterTasks->RefineZLP(false) ? 0 : 1;
    break;

    // Flash FEG
  case NAACT_FLASH_FEG:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    ind = 0;
    if (FEIscope) {

      // For FEIscope, check for high flash if allowed, then for low; set to do now
      if (mAcqParm->highFlashIfOK) {
        if (!mScope->GetIsFlashingAdvised(1, ind)) {
          StopAcquiring();
          return;
        }
        if (ind)
          mScope->SetDoNextFEGFlashHigh(true);
      }
      if (!ind && mScope->GetIsFlashingAdvised(1, ind)) {
        StopAcquiring();
        return;
      }
      if (!ind)
        break;
      hours = 0.;
    } else {

      // For JEOL, it should be an interval for just doing it
      hours = (float)(mAcqActions[NAACT_FLASH_FEG].minutes / 60.);
    }
    ind = LONG_OP_FLASH_FEG;
    if (mScope->StartLongOperation(&ind, &hours, 1) > 0) {
      SEMMessageBox("Cannot flash FEG; the thread for long scope operations is busy");
      StopAcquiring();
      return;
    }
    break;

    // Check vacuum and nitrogen status
  case NAACT_CHECK_DEWARS:
    SEMTrace('n', "Doing %s", (LPCTSTR)mAcqActions[mAcqSteps[mAcqStepIndex]].name);
    next = 0;
    if (mStepIndForDewarCheck >= 0)
      next = (mAcqStepIndex == mStepIndForDewarCheck) ? 1 : -1;
    stopErr = mWinApp->mParticleTasks->ManageDewarsVacuum(*dvParams, next);
    break;

    // Relax the stage
  case ACQ_RELAX_STAGE:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    err = mMacroProcessor->DoStageRelaxation(mScope->GetStageRelaxation());
    SEMTrace('1', "Relaxed stage err %d", err);
    if (err < 0)
      mWinApp->AppendToLog("Stage is not in known backlash state so relaxation cannot "
        "be done");
    if (err > 0) {
      SEMMessageBox("Failed to get stage position for relaxing stage");
      StopAcquiring();
      return;
    }
    mMovingStage = true;
    break;

    // Open file if needed
  case ACQ_OPEN_FILE:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    if (OpenFileIfNeeded(item, false)) {
      StopAcquiring();
      return;
    }
    break;

    // Take a picture or montage
  case ACQ_ACQUIRE:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    if (mAcqParm->adjustBTforIS && comaVsIS->magInd > 0) {
      mScope->GetImageShift(ISX, ISY);
      mWinApp->mMacroProcessor->AdjustBeamTiltAndAstig(ISX, ISY, mSavedBeamTiltX, 
        mSavedBeamTiltY, mSavedAstigX,  mSavedAstigY);
    }
    if (mWinApp->Montaging() && !mSkippingSave) {
      stopErr = mWinApp->mMontageController->StartMontage(MONT_NOT_TRIAL, false);
    } else {
      timeOut = 300000;
      mCamera->SetCancelNextContinuous(true);
      if (mDoingEarlyReturn && mCamera->SetNextAsyncSumFrames(
        mAcqParm->numEarlyFrames < 0 ? 65535 : mAcqParm->numEarlyFrames, false)) {
        StopAcquiring();
        return;
      }
      ind = RECORD_CONSET;
      if (mWinApp->LowDoseMode() && mAcqParm->acquireType == ACQUIRE_TAKE_MAP &&
        mAcqParm->mapWithViewSearch)
        ind = mAcqParm->mapWithViewSearch > 1 ? SEARCH_CONSET : VIEW_CONSET;
      mCamera->InitiateCapture(ind);
    }
    break;

    // Run a pre-macro
  case NAACT_RUN_PREMACRO:
    mMacroProcessor->Run((mAcqParm->acquireType == ACQUIRE_DO_TS ?
      mAcqParm->preMacroInd : mAcqParm->preMacroIndNonTS) - 1);
    break;

    // Run a macro
  case ACQ_RUN_MACRO:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    report.Format("Running script %d at item %s   (%.2f, %.2f)", mAcqParm->macroIndex,
      (LPCTSTR)item->mLabel, item->mStageX, item->mStageY);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    mMacroProcessor->Run(mAcqParm->macroIndex - 1);
    break;

    // Start a tilt series: open the file if possible, then go on to get the TS params
    // This should leave things resumable if there is a problem
  case ACQ_START_TS:
    tsp = (TiltSeriesParam *)mTSparamArray.GetAt(item->mTSparamIndex);

    // Check item tilt angles before copying param
    if (item->mTSstartAngle > EXTRA_VALUE_TEST &&
      mHelper->CheckTiltSeriesAngles(item->mTSparamIndex, item->mTSstartAngle,
        item->mTSendAngle, item->mTSbidirAngle, report)) {
      str.Format("For item %s, ", (LPCTSTR)item->mLabel);
      SEMMessageBox(str + report);
      StopAcquiring();
      return;
    }
    mWinApp->mTSController->CopyParamToMaster(tsp, true);

    // If there are item changes, set them in master now, leave stored param alone
    if (item->mTSstartAngle > EXTRA_VALUE_TEST ||
      item->mTargetDefocus > EXTRA_VALUE_TEST) {
      tsp = mWinApp->mTSController->GetTiltSeriesParam();
      if (item->mTargetDefocus > EXTRA_VALUE_TEST)
        tsp->targetDefocus = item->mTargetDefocus;
      if (item->mTSstartAngle > EXTRA_VALUE_TEST) {
        tsp->startingTilt = item->mTSstartAngle;
        tsp->endingTilt = item->mTSendAngle;
        if (tsp->bidirAngle > EXTRA_VALUE_TEST && tsp->doBidirectional)
          tsp->bidirAngle = item->mTSbidirAngle;
      }
      mWinApp->mTSController->SyncParamToOtherModules();
    }
    mHelper->ChangeRefCount(NAVARRAY_TSPARAM, item->mTSparamIndex, -1);
    item->mTSparamIndex = -1;
    item->mTSstartAngle = EXTRA_NO_VALUE;
    item->mTSendAngle = EXTRA_NO_VALUE;
    item->mTSbidirAngle = EXTRA_NO_VALUE;
    SetChanged(true);
    mCurListSel = mCurrentItem = mAcquireIndex;
    UpdateListString(mAcquireIndex);
    ManageCurrentControls();

    // If tilt series fails to start, the email should already be sent, but
    // if it is not postponed we have to close the file.
    // Set flag regardless so tasks work normally
    if (mWinApp->mTSController->StartTiltSeries(false, 1)) {
      if (!mWinApp->mTSController->GetPostponed())
        CloseFileOpenedByAcquire();
    }
    mStartedTS = true;
    mWinApp->UpdateBufferWindows();
    break;

    // Do Multishot
  case ACQ_DO_MULTISHOT:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    mRetValFromMultishot = mWinApp->mParticleTasks->StartMultiShot(msParams->numShots[0],
      msParams->doCenter, msParams->spokeRad[0],
      msParams->doSecondRing ? msParams->numShots[1] : 0, msParams->spokeRad[1],
      msParams->extraDelay, 
      (msParams->doEarlyReturn != 2 || msParams->numEarlyFrames != 0) ?
      msParams->saveRecord : false, msParams->doEarlyReturn, msParams->numEarlyFrames,
      msParams->adjustBeamTilt, msParams->inHoleOrMultiHole);
    stopErr = B3DMAX(mRetValFromMultishot, 0);
    break;

  // Run a post-macro
  case NAACT_RUN_POSTMACRO:
    mMacroProcessor->Run((mAcqParm->acquireType == ACQUIRE_DO_TS ? 
      mAcqParm->postMacroInd : mAcqParm->postMacroIndNonTS) - 1);
    break;

    // Adjust for backlash if needed
  case ACQ_BACKLASH:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    AdjustBacklash(-1, true);
    break;

  // Set up for next area if there is one
  case ACQ_FIND_SETUP_NEXT:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    if (mResetAcquireIndex)
      mAcquireIndex = mStartingAcquireIndex;
    mResetAcquireIndex = false;
    if (mAcquireEnded || (err = FindAndSetupNextAcquireArea()) != 0) {
      if (mAcquireEnded < 0) {
        mAcquireEnded = 0;
        mPausedAcquire = true;
        mMontCurrentAtPause = -1;
        if (mWinApp->mStoreMRC)
          mMontCurrentAtPause = mWinApp->Montaging() ? 1 : 0;
        mWinApp->UpdateBufferWindows();
        mWinApp->SetStatusText(COMPLEX_PANE, "PAUSED NAV ACQUIRE");
        mWinApp->AddIdleTask(TASK_NAVIGATOR_ACQUIRE, 0, 0);
        return;
      }
      EndAcquireWithMessage();
    }
    mAcqStepIndex = -1;
    break;

    // Move to the area
  case ACQ_MOVE_TO_AREA:

    // This adds an idle task....
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    if (GotoCurrentAcquireArea()) {
      StopAcquiring();
    }
    return;

  case ACQ_MOVE_ELSEWHERE:
    SEMTrace('n', "Doing %s", stepNames[mAcqSteps[mAcqStepIndex]]);
    act = mAcqSteps[mAcqStepIndex + 1];
    next = mHelper->FindNearestItemMatchingText(item->mStageX, item->mStageY,
      mAcqActions[act].labelOrNote,
      (mAcqActions[act].flags & NAA_FLAG_MATCH_NOTE) != 0);
    if (MoveToItem(next)) {
      StopAcquiring();
      return;
    }
    mMovingStage = true;
    break;
  }

  if (stopErr) {
    StopAcquiring();
    return;
  }

  if (mAcquireIndex >= 0)
    m_listViewer.SetTopIndex(B3DMAX(0, mAcquireIndex - 2));
  mWinApp->AddIdleTask(TASK_NAVIGATOR_ACQUIRE, 0, 0);
}

// This is called when the dialog is closing, it will either reenter the AcquireAreas 
// routine or change the flags for resuming from pause
void CNavigatorDlg::AcquireDlgClosing()
{
  int navState = mWinApp->mCameraMacroTools.GetNavigatorState();
  mHelper->GetAcquireDlgPlacement(true);
  SetCurAcqParmActions(mHelper->GetCurAcqParamIndex());
  if (navState == NAV_PAUSED && mNavAcquireDlg->mPostponed == 0) {
    mMacroProcessor->SetNonResumable();
    mNavAcquireDlg = NULL;

    // There is an idle task that will start the next task
    ResumeFromPause();
    mWinApp->SetStatusText(COMPLEX_PANE, "");
  } else
    AcquireAreas(false, true);
}

// Resuming from pause: test for a change in the output file type, change flags
int CNavigatorDlg::ResumeFromPause()
{
  CString str;
  if (mAcqParm->acquireType != ACQUIRE_DO_TS && mMontCurrentAtPause >= 0 &&
    mWinApp->mStoreMRC && !BOOL_EQUIV(mWinApp->Montaging(), mMontCurrentAtPause > 0)) {
    str.Format("The current file was a %s file when you paused and is now a %s file\n\n"
      "Are you sure you want to resume?", mMontCurrentAtPause ? "montage" : "single-frame"
      , mMontCurrentAtPause ? "single-frame" : "montage");
    if (AfxMessageBox(str, MB_QUESTION) == IDNO)
      return 1;
  }
  mResumedFromPause = true;
  mPausedAcquire = false;
  return 0;
}

// Message box on normal stopping or ending from paused state
void CNavigatorDlg::EndAcquireWithMessage(void)
{
  CString report;
  StopAcquiring();
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  if (mNumAcquired) {
    report.Format("%s at %d areas", mAcqParm->acquireType == ACQUIRE_DO_TS ? 
      "Tilt series acquired" : (mAcqParm->acquireType == ACQUIRE_RUN_MACRO ?
      "Script run to completion" : (mAcqParm->acquireType == ACQUIRE_MULTISHOT ? 
        "Multiple Records acquired" : "Images acquired")), mNumAcquired);
    AfxMessageBox(report, MB_EXCLAME);
  }
}

// Task boilerplate
int CNavigatorDlg::TaskAcquireBusy()
{
  int stage = 0;

  // If we started a resumable macro or a suspended tilt series,
  // take sleep to preserve CPU and say busy
  if ((StartedMacro() && !mMacroProcessor->DoingMacro()) || mPausedAcquire ||
    (mStartedTS && (mWinApp->StartedTiltSeries() && !mWinApp->DoingTiltSeries() ||
    mWinApp->mTSController->GetPostponed()))) {
    Sleep(10);
    return 1;
  }
  if (mMovingStage || mStartedTS && mWinApp->mTSController->GetTiltedToZero())
    stage = mScope->StageBusy();
  if (stage < 0)
    return -1;
  return (stage > 0 || mWinApp->mMontageController->DoingMontage() || 
    mHelper->GetRealigning() || mWinApp->mFocusManager->DoingFocus() ||
    mCamera->CameraBusy() || mMacroProcessor->DoingMacro() ||
    mWinApp->mComplexTasks->DoingTasks() || mWinApp->DoingTiltSeries() ||
    mWinApp->mAutoTuning->DoingAutoTune() || mScope->GetDoingLongOperation() || 
    mWinApp->mGainRefMaker->AcquiringGainRef() || mWinApp->mFilterTasks->RefiningZLP() ||
    mWinApp->mParticleTasks->GetDVDoingDewarVac()) ? 1 : 0;
}

void CNavigatorDlg::AcquireCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(B3DCHOICE(mAcqParm->acquireType != ACQUIRE_RUN_MACRO, 
    _T("Time out acquiring images"), _T("Time out running script")));

  // Should this be called?  ErrorOccurred will call it too
  StopAcquiring();
  mWinApp->ErrorOccurred(error);
}

// Stop the acquisitions unless a macro or a TS was started
void CNavigatorDlg::StopAcquiring(BOOL testMacro)
{
  if (mAcquireIndex < 0)
    return;
  if (testMacro && (StartedMacro() || mStartedTS))
    return;
  if (!mWinApp->mCameraMacroTools.GetUserStop()) {
    SendEmailIfNeeded();
  }
  RestoreBeamTiltIfSaved();
  CloseFileOpenedByAcquire();
  mAcquireIndex = -1;
  mFocusCycleCounter = -1;
  mWinApp->mFocusManager->SetTargetDefocus(mSavedTargetDefocus);
  mPausedAcquire = false;
  ManageListHeader();
  SetCollapsing(mSaveCollapsed);
  mWinApp->UpdateBufferWindows();
  if (mAcqParm->closeValves && !HitachiScope && !mScope->SetColumnValvesOpen(false))
    AfxMessageBox("An error occurred closing the gun valve at the end of acquisitions.",
         MB_EXCLAME);
  if (mAcqParm->acquireType == ACQUIRE_RUN_MACRO)
    return;
  mBufferManager->SetAlignOnSave(mSaveAlignOnSave);
  mBufferManager->SetConfirmBeforeDestroy(3, mSaveProtectRecord);
  mWinApp->SetStatusText(COMPLEX_PANE, "");
}

// Do common actions whenthe acquire step is done
void CNavigatorDlg::SetItemSuccessfullyAcquired(CMapDrawItem * item)
{
  item->mAcquire = false;
  item->mTargetDefocus = EXTRA_NO_VALUE;
  SetChanged(true);
  mNumAcquired++;
  UpdateListString(mAcquireIndex);
  if (mAcquireIndex == mCurrentItem)
    ManageCurrentControls();
}

// Set up step position so it will go to next item after failure
void CNavigatorDlg::SkipToNextItemInAcquire(CMapDrawItem *item, const char *failedStep)
{
  ManageNumDoneAcquired();
  mAcqStepIndex = mNumAcqSteps - 2;
  PrintfToLog("Skipping item # %d with label %s due to failure %s",
    mAcquireIndex + 1, (LPCTSTR)item->mLabel, failedStep);
  mAcquireIndex++;
}

// Restoring beam tilt and astig after acquire
void CNavigatorDlg::RestoreBeamTiltIfSaved()
{
  if (mSavedBeamTiltX < EXTRA_VALUE_TEST)
    return;
  mWinApp->mAutoTuning->BacklashedBeamTilt(mSavedBeamTiltX, mSavedBeamTiltY,
    mScope->GetAdjustForISSkipBacklash() <= 0);
  mSavedBeamTiltX = EXTRA_NO_VALUE;
  if (mSavedAstigX < EXTRA_VALUE_TEST)
    return;
  mWinApp->mAutoTuning->BacklashedStigmator(mSavedAstigX, mSavedAstigY, 
    mScope->GetAdjustForISSkipBacklash() <= 0);
  mSavedAstigX = EXTRA_NO_VALUE;
}

// Count items before file opening, # of files to open and type, and whether there are
// any acquire or TS points in range specified in dialog parameters if doing a subset
void CNavigatorDlg::EvaluateAcquiresForDlg(CNavAcquireDlg *dlg)
{
  int i, groupInd, groupID, startInd = 0, endInd = (int)mItemArray.GetSize() - 1;
  ScheduledFile *sched;
  CMapDrawItem *item;

  if (dlg->m_bDoSubset) {
    startInd = B3DMAX(0, dlg->m_iSubsetStart - 1);
    endInd = B3DMIN(endInd, dlg->m_iSubsetEnd - 1);
  }

  // Count up the number of items to acquire before a file to open, and the number of
  // files to open.
  dlg->mNumFilesToOpen = 0;
  dlg->mNumAcqBeforeFile = 0;
  dlg->mOutputFile = "";
  dlg->mIfMontOutput = mWinApp->Montaging();
  if (mWinApp->mStoreMRC)
    dlg->mOutputFile = mWinApp->mStoreMRC->getName();
  for (i = startInd; i <= endInd; i++) {
    item = mItemArray[i];
    groupInd = GroupScheduledIndex(item->mGroupID);
    if (item->mAcquire && item->mRegistration == mCurrentRegistration) {
      if (item->mFilePropIndex >= 0 || (groupInd >= 0 && item->mGroupID != groupID)) {

        // Keep track of name and type of first file to open
        if (!dlg->mNumFilesToOpen && !dlg->mNumAcqBeforeFile) {
          dlg->mIfMontOutput = item->mMontParamIndex >= 0;
          dlg->mOutputFile = item->mFileToOpen;
          if (groupInd >= 0) {
            sched = (ScheduledFile *)mGroupFiles.GetAt(groupInd);
            dlg->mIfMontOutput = sched->montParamIndex >= 0;
            dlg->mOutputFile = sched->filename;
          }
        }
        dlg->mNumFilesToOpen++;
        if (groupInd >= 0)
          groupID = item->mGroupID;
          
      } else if (!dlg->mNumFilesToOpen) {
        dlg->mNumAcqBeforeFile++;
      }
    }
  }

  // See if any point of each type in range
  dlg->mAnyTSpoints = AcquireOK(true, startInd, endInd);
  dlg->mAnyAcquirePoints = AcquireOK(false, startInd, endInd);
}

// Take care of incrementing the done count and the elapsed time
void CNavigatorDlg::ManageNumDoneAcquired(void)
{
  double now = GetTickCount();
  mNumDoneAcq++;
  mAcquireCounter++;
  mElapsedAcqTime += SEMTickInterval(now, mLastAcqDoneTime);
  mLastAcqDoneTime = now;
}

void CNavigatorDlg::SetAcquireEnded(int state)
{
  mAcquireEnded = state;
  if (mAcqParm->acquireType != ACQUIRE_RUN_MACRO && mAcqParm->acquireType != ACQUIRE_DO_TS)
    mWinApp->SetStatusText(COMPLEX_PANE, state > 0 ? "ENDING NAV ACQUIRE" : 
    "PAUSING NAV ACQUIRE");
  else
    mWinApp->AppendToLog(CString(state > 0 ? "Ending" : "Pausing") + " Navigator "
    "acquisition after current item");
}

// Test for whether we started a macro and it is either running or resumable
BOOL CNavigatorDlg::StartedMacro(void)
{
  return (GetAcquiring() && (mAcqParm->acquireType == ACQUIRE_RUN_MACRO || 
    mAcqParm->runPremacro || mAcqParm->runPostmacro) && 
    (mMacroProcessor->DoingMacro() || mMacroProcessor->IsResumable()));
}

// Send an email if requested and not sent yet
void CNavigatorDlg::SendEmailIfNeeded(void)
{
  CString mess;
  if (!((mAcqParm->acquireType == ACQUIRE_DO_TS && mAcqParm->sendEmail) ||
    (mAcqParm->acquireType != ACQUIRE_DO_TS && mAcqParm->sendEmailNonTS)) ||
    mEmailWasSent)
    return;
  mess.Format("Your Navigator Acquire at Items operation %s %d areas",
    mAcquireEnded ? "successfully acquired from" : "stopped with an error "
    "after acquiring from", mNumAcquired);
  mWinApp->mMailer->SendMail("Message from SerialEM Navigator", mess);
  mEmailWasSent = true;
}

// Find the next area marked as an acquire in this registration and handle some aspects
// of doing a new area
int CNavigatorDlg::FindAndSetupNextAcquireArea()
{
  int ind, err, axisBits = axisXY | axisZ;
  bool skippedGroup = false, skippingGroup, acquire;
  CMapDrawItem *item;
  mRealignedInAcquire = false;
  for (ind = mAcquireIndex; ind <= mEndingAcquireIndex; ind++) {
    item = mItemArray[ind];
    acquire = IsItemToBeAcquired(item, skippingGroup);
    if (skippingGroup && !skippedGroup) {
      PrintfToLog("Skipping point(s) with group ID %d", mGroupIDtoSkip);
      skippedGroup = true;
    }
    if (!acquire)
      continue;

    SEMTrace('M', "Navigator Stage Start");

    // Set the current item for the stage move!
    mItem = item;

    err = MoveStage(axisBits, true);
    if (err < 0) {
      item->mAcquire = false;
      UpdateListString(ind);
      if (ind == mCurrentItem)
        ManageCurrentControls();
      mWinApp->AppendToLog("Item " + item->mLabel + " is being skipped because it is "
        "outside the limits for stage movement", LOG_MESSAGE_IF_CLOSED);
      continue;
    }
    mAcquireIndex = ind;

    // Set state now
    err = OpenFileIfNeeded(item, true);
    if (err)
      return err;

    // Update the log file for tilt series with autosave policy
    if (mAcqParm->acquireType == ACQUIRE_DO_TS && 
      mWinApp->mTSController->GetAutoSavePolicy()) {
        if (mWinApp->mLogWindow && 
          mWinApp->mLogWindow->SaveFileNotOnStack(item->mFileToOpen)) {
            mWinApp->mLogWindow->DoSave();
            mWinApp->mLogWindow->CloseLog();
            mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
        }
        mWinApp->AppendToLog("", LOG_OPEN_IF_CLOSED);
        mWinApp->mLogWindow->UpdateSaveFile(true, item->mFileToOpen);
    }

    // Set flag for first in group and maintain last group ID
    mFirstInGroup = item->mGroupID > 0 && item->mGroupID != mLastGroupID;
    mLastGroupID = item->mGroupID;

    UpdateListString(ind);
    return 0;
  }
  mAcquireEnded = 1;
  return -1;
}

// Actually do the move if called for
int CNavigatorDlg::GotoCurrentAcquireArea()
{
  int err, axisBits = axisXY | axisZ;
  mItem = mItemArray[mAcquireIndex];

  if (mAcqParm->skipZmoves)
    axisBits = axisXY;
  err = MoveStage(axisBits);
  if (err > 0)
    return 1;
  mMovingStage = axisBits != 0;
  mWinApp->AddIdleTask(TASK_NAVIGATOR_ACQUIRE, 0, 60000);
  return 0;
}

// Open file and set state if called for for this item
// This is called once for state only when going to next item, and again at File Open step
int CNavigatorDlg::OpenFileIfNeeded(CMapDrawItem * item, bool stateOnly)
{
  int j, ind;
  ScheduledFile *sched;
  FileOptions *fileOptp;
  CString root, extension, extraName;
  CString *namep;
  int *stateIndp;
  MontParam *masterMont = mWinApp->GetMontParam();
  MontParam *montp;
  StateParams *state;
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;

  mNextFileOptInd = item->mFilePropIndex;
  mNextMontParInd = item->mMontParamIndex;
  stateIndp = &item->mStateIndex;
  namep = &item->mFileToOpen;
  j = GroupScheduledIndex(item->mGroupID);
  if (j >= 0) {
    sched = (ScheduledFile *)mGroupFiles.GetAt(j);
    mNextFileOptInd = sched->filePropIndex;
    mNextMontParInd = sched->montParamIndex;
    stateIndp = &sched->stateIndex;
    namep = &sched->filename;
  }

  // Change state before opening file; what to do if no state is found?
  if (*stateIndp >= 0) {
    state = mHelper->ExistingStateParam(*stateIndp, true);
    if (state) {
      mHelper->SetStateFromParam(state, conSet, RECORD_CONSET);
      // Uncomment to set state twice, once at start and once before acquires
      //if (!stateOnly)
      mHelper->RemoveFromArray(NAVARRAY_STATE, *stateIndp);
      *stateIndp = -1;
    } else {
      SEMMessageBox("The state to be set for this item cannot be found in array of"
        " states");
      return 1;
    }
  }

  // Set low dose focus area if parameter was saved, overrides one in the state
  // This needs to be done the first time in when setting state only since autofocus step
  // is before File Open step
  if (item->mFocusAxisPos > EXTRA_VALUE_TEST && stateOnly) {
    mHelper->SetLDFocusPosition(mWinApp->GetCurrentCamera(), item->mFocusAxisPos,
      item->mRotateFocusAxis, item->mFocusAxisAngle, item->mFocusXoffset, 
      item->mFocusYoffset, "position for item");
  }
  if (item->mTargetDefocus > EXTRA_VALUE_TEST && stateOnly)
    mWinApp->mFocusManager->SetTargetDefocus(item->mTargetDefocus);
  if (stateOnly) {
    UpdateListString(mAcquireIndex);
    return 0;
  }

  if (mNextFileOptInd >= 0) {

    // Close current file if we opened it, check if it is still OK
    CloseFileOpenedByAcquire();
    if (mDocWnd->StoreIndexFromName(*namep) >= 0) {
      SEMMessageBox("The file set to be opened for this item,\n" + *namep 
        + "\n" + "is already open.");
      return 1;
    }

    // Check the extra file(s) before proceeding too
    for (ind = 0; ind < mNumExtraFileSuffixes; ind++) {
      UtilSplitExtension(*namep, root, extension);
      extraName = root + mExtraFileSuffix[ind] + extension;
      if (mDocWnd->StoreIndexFromName(extraName) >= 0) {
        SEMMessageBox("The extra file set to be opened for this item,\n" + extraName 
          + "\n" + "is already open.");
        return 1;
      }
    }

    // If Montaging, replace the master params with this param set before opening file
    // But leave the current file first because that copies master to current
    mDocWnd->LeaveCurrentFile();
    if (mNextMontParInd >= 0) {
      montp = mMontParArray.GetAt(mNextMontParInd);
      *masterMont = *montp;
    }
    fileOptp = (FileOptions *)mFileOptArray.GetAt(mNextFileOptInd);
    if (mNextMontParInd < 0) {
      fileOptp->typext &= ~MONTAGE_MASK;
      fileOptp->montageInMdoc = false;
    }

    // Now open the extra file(s)
    for (ind = 0; ind < mNumExtraFileSuffixes; ind++) {
      UtilSplitExtension(*namep, root, extension);
      extraName = root + mExtraFileSuffix[ind] + extension;
      mWinApp->mStoreMRC = mDocWnd->OpenNewFileByName(extraName, fileOptp);
      if (!mWinApp->mStoreMRC) {
        mDocWnd->RestoreCurrentFile();
        SEMMessageBox("An error occurred attempting to open an extra file:"
          "\n" + extraName);
        return 1;
      }
      mDocWnd->AddCurrentStore();
      mWinApp->AppendToLog("Opened extra file " + extraName, LOG_OPEN_IF_CLOSED);
      mExtraFileToClose[ind] = mWinApp->mStoreMRC->getFilePath();
      mDocWnd->LeaveCurrentFile();
    }
    mNumExtraFilesToClose = mNumExtraFileSuffixes;
    mNumExtraFileSuffixes = 0;

    // And open the regular file
    mWinApp->mStoreMRC = mDocWnd->OpenNewFileByName(*namep, fileOptp);
    if (!mWinApp->mStoreMRC) {
      mDocWnd->RestoreCurrentFile();
      SEMMessageBox("An error occurred attempting to open a new file:"
        "\n" + *namep);
      return 1;
    }
    mHelper->ChangeRefCount(NAVARRAY_FILEOPT, mNextFileOptInd, -1);
    mAcquireOpenedFile = mWinApp->mStoreMRC->getFilePath();

    // Need to set montaging before adding the store
    if (mNextMontParInd >= 0) {
      mWinApp->SetMontaging(true);
      mHelper->ChangeRefCount(NAVARRAY_MONTPARAM, mNextMontParInd, -1);
    }
    mDocWnd->AddCurrentStore();
    mWinApp->AppendToLog("Opened new file " + *namep, LOG_OPEN_IF_CLOSED);
    *namep = "";

    if (j >= 0) {
      delete sched;
      mGroupFiles.RemoveAt(j);
    } else {
      item->mFilePropIndex = -1;
      item->mMontParamIndex = -1;
    }
    UpdateListString(mAcquireIndex);
  }
  return 0;
}

// Close either the main file or an extra file that was opened by AcquireAreas
void CNavigatorDlg::CloseFileOpenedByAcquire(void)
{
  if (!mAcquireOpenedFile.IsEmpty() && mDocWnd->GetNumStores() > 0 && 
    mAcquireOpenedFile == mWinApp->mStoreMRC->getFilePath())
    mDocWnd->DoCloseFile();
  for (int ind = mNumExtraFilesToClose - 1; ind >= 0; ind--) {
    if (mDocWnd->GetNumStores() > 0 && 
      mExtraFileToClose[ind] == mWinApp->mStoreMRC->getFilePath())
        mDocWnd->DoCloseFile();
  }
  mNumExtraFilesToClose = 0;
  mAcquireOpenedFile = "";
}

// Tell FindMapForRealigning what low dose area to realign for: current area (with Focus/
// Trial promoted to Record) if not
// acquiring or running a macro; Record unless doing a montage with View parameters
int CNavigatorDlg::TargetLDAreaForRealign(void)
{
  int area = mScope->GetLowDoseArea();
  BOOL montaging = mWinApp->Montaging();
  MontParam *montP = mWinApp->GetMontParam();
  if (area == TRIAL_CONSET || area == FOCUS_CONSET)
    area = RECORD_CONSET;
  if (mAcquireIndex < 0 || mAcqParm->acquireType == ACQUIRE_RUN_MACRO)
    return area;
  area = RECORD_CONSET;
  if (mNextFileOptInd >= 0) {

    // Accessing the array with a bad index will give "Encountered an improper argument"
    // error message and a weird state from exception being handled that way
    montaging = mNextMontParInd >= 0 && mNextMontParInd < mMontParArray.GetSize();
    if (montaging)
      montP = mMontParArray.GetAt(mNextMontParInd);
  }
  if (montaging)
    area = MontageLDAreaIndex(montP);
  return area;
}

//////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS

// Set the current registration from the max of all registrations in list
void CNavigatorDlg::SetCurrentRegFromMax()
{
  mCurrentRegistration = 1;
  CMapDrawItem *item;
  for (int i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (mCurrentRegistration < item->mRegistration && item->mImported <= 0)
      mCurrentRegistration = item->mRegistration;
  }
  m_sbcCurrentReg.SetPos(mCurrentRegistration);
  m_strCurrentReg.Format("%d", mCurrentRegistration);
}

// Set registration number externally
int CNavigatorDlg::SetCurrentRegistration(int newReg)
{
  CMapDrawItem *item;
  int numNonTS, numTS, oldNumTS, oldNonTS, delta;
  if (newReg < 0 || newReg > MAX_CURRENT_REG ||
    RegistrationUseType(newReg) == NAVREG_IMPORT)
    return 1;
  if (mAcquireIndex >= 0)
    mHelper->CountAcquireItems(mAcquireIndex, mEndingAcquireIndex, oldNonTS, oldNumTS);
  mCurrentRegistration = newReg;
  m_sbcCurrentReg.SetPos(mCurrentRegistration);
  m_strCurrentReg.Format("%d", mCurrentRegistration);
  UpdateData(false);
  if (mAcquireIndex >= 0) {
    mResetAcquireIndex = true;
    mHelper->CountAcquireItems(mStartingAcquireIndex, mEndingAcquireIndex, numNonTS, 
      numTS);
    item = mItemArray[mAcquireIndex];
    delta = item->mAcquire ? 1 : 0;
    mInitialNumAcquire += (mAcqParm->acquireType == ACQUIRE_DO_TS ? 
      numTS + delta - oldNumTS: numNonTS + delta - oldNonTS);
  }
  return 0;
}

// Check the proposed registration point number at the given registration
// and if it used, find the first free one
int CNavigatorDlg::GetFreeRegPtNum(int registration, int proposed)
{
  CMapDrawItem *item;
  int free, check;
  BOOL found;

  // Loop on possible numbers; the first time, evaluate proposed one
  for (free = 0; free < MAX_REGPT_NUM; free++) {
    check = free;
    if (!free)
      check = proposed > 0 ? proposed : 1;
    found = false;
    for (int i = 0; i < mItemArray.GetSize(); i++) {
      item = mItemArray[i];
      if (item->mRegistration == registration && item->mRegPoint == check) {
        found = true;
        break;
      }
    }
    if (!found)
      return check;
  }
  return free;
}

// Set a registration point number
void CNavigatorDlg::SetRegPtNum(int inVal)
{
  mRegPointNum = inVal;
  m_strRegPtNum.Format("%d", inVal);
  m_sbcRegPtNum.SetPos(mRegPointNum);
}

// Find the type of the current registration
int CNavigatorDlg::RegistrationUseType(int reg)
{
  CMapDrawItem *item;
  for (int i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mRegistration == reg)
      return item->mImported > 0 ? NAVREG_IMPORT : NAVREG_REGULAR;
  }
  return NAVREG_UNUSED;
}

void CNavigatorDlg::SetChanged(BOOL inVal)
{
  mChanged = inVal;
  if (inVal && mHelper->mMultiCombinerDlg)
    mHelper->mMultiCombinerDlg->UpdateEnables();
  if (inVal && mHelper->mHoleFinderDlg->IsOpen())
    mHelper->mHoleFinderDlg->ManageEnables();
}

// Make a new item and make it current item
CMapDrawItem *CNavigatorDlg::MakeNewItem(int groupID)
{
  CString str;
  CMapDrawItem *item = new CMapDrawItem();
  bool addstr = true;
  CMapDrawItem *lastItem;
  mItemArray.Add(item);
  item->mColor = DEFAULT_POINT_COLOR;
  item->mLabel.Format("%d", mNewItemNum++);
  item->mRegistration = mCurrentRegistration;
  item->mOriginalReg = mCurrentRegistration;
  item->mMapID = MakeUniqueID();
  item->mGroupID = groupID;
  mCurrentItem = (int)mItemArray.GetSize() - 1;
  mCurListSel = mCurrentItem;
  if (m_bCollapseGroups) {
    lastItem = mItemArray[B3DMAX(0,mCurrentItem - 1)];

    // If in same group, set current list sel to that item
    if (groupID > 0 && mCurrentItem > 0 && groupID == lastItem->mGroupID) {
      mCurListSel = mItemToList[mCurrentItem - 1];
      addstr = false;
    } else {

      // Otherwise set to a new line
      if (mCurrentItem > 0)
        mCurListSel = mItemToList[mCurrentItem - 1] + 1;
      if (groupID > 0)
        mListToItem.push_back(-1 - mCurrentItem);
      else
        mListToItem.push_back(mCurrentItem);
    }
    mItemToList.push_back(mCurListSel);
  }

  if (!mDeferAddingToViewer) {
    ItemToListString(mCurrentItem, str);
    if (addstr)
      m_listViewer.AddString(str);
    m_listViewer.SetCurSel(mCurListSel);
  }
  mSelectedItems.clear();
  mSelectedItems.insert(mCurrentItem);
  // No longer manage controls: caller must do it
  return item;
}

// Make an ID number based on two random numbers and higher than a minimum value and
// not already in the set; add to set
int CNavigatorDlg::MakeUniqueID(void)
{
  int randVal;
  for (;;) {
    randVal = (rand() | (rand() << 16)) & 0x7FFFFFFF;
    if (randVal >= MIN_INTERNAL_ID && !mSetOfIDs.count(randVal))
      break;
  }
  mSetOfIDs.insert(randVal);
  return randVal;
}

// Set the current stage position into the given item
void CNavigatorDlg::SetCurrentStagePos(int index)
{
  CMapDrawItem *item = mItemArray[index];
  GetAdjustedStagePos(item->mStageX, item->mStageY, item->mStageZ);
}

void CNavigatorDlg::GetAdjustedStagePos(float & stageX, float & stageY, float & stageZ)
{
  double dStageZ, ISX, ISY;
  mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, dStageZ);
  stageX = (float)mLastScopeStageX;
  stageY = (float)mLastScopeStageY;
  stageZ = (float)dStageZ;

  // Convert the current image shift into additional stage shift
  mScope->GetLDCenteredShift(ISX, ISY);
  ConvertIStoStageIncrement(mScope->GetMagIndex(), mWinApp->GetCurrentCamera(), ISX, ISY,
    (float)mScope->FastTiltAngle(), stageX, stageY);
}

// This is all that is needed for redraw
void CNavigatorDlg::Redraw()
{
  if (!(mMacroProcessor->DoingMacro() || mMacroProcessor->GetSuspendNavRedraw()))
    mWinApp->mMainView->DrawImage();
}

// Get the current item into mItem and return true, or return false if no current item
BOOL CNavigatorDlg::SetCurrentItem(bool groupOK)
{
  if (mCurrentItem < 0 || mCurListSel < 0 ||
    (!groupOK && m_bCollapseGroups && mListToItem[mCurListSel] < 0))
    return false;
  IndexOfSingleOrFirstInGroup(mCurListSel, mCurrentItem);
  mItem = mItemArray[mCurrentItem];
  return true;
}

// Return the current item
CMapDrawItem * CNavigatorDlg::GetCurrentItem(void)
{
  if (SetCurrentItem())
    return mItem;
  return NULL;
}

// Return the current item or one being acquired to another module
int CNavigatorDlg::GetCurrentOrAcquireItem(CMapDrawItem *&item)
{
  int index;
  if (mAcquireIndex >= 0)
    index = mAcquireIndex;
  else if (IndexOfSingleOrFirstInGroup(mCurListSel, index))
    index = -1;
  item = NULL;
  if (index >= 0) {
    mItem = mItemArray[index];
    item = mItem;
  }
  return index;
}

// Return the current item of start of current group
int CNavigatorDlg::GetCurrentOrGroupItem(CMapDrawItem *& item)
{ 
  item = NULL;
  if (!SetCurrentItem(true))
    return -1;
  item = mItem;
  return mCurrentItem;
}

// Return an arbitrary item by index; set mItem to it
CMapDrawItem * CNavigatorDlg::GetOtherNavItem(int index)
{
  if (index < 0 || index >= mItemArray.GetSize())
    return NULL;
  mItem = mItemArray[index];
  return mItem;
}

// Finds the next item to be acquired, returns its index
CMapDrawItem *CNavigatorDlg::FindNextAcquireItem(int &index)
{
  CMapDrawItem *item;
  bool skippingGroup;
  for (index = mAcquireIndex + 1; index <= mEndingAcquireIndex; index++) {
    item = mItemArray[index];
    if (IsItemToBeAcquired(item, skippingGroup))
      return item;
  }
  index = -1;
  return NULL;
}

// Tests whether a given item is to be acquired
bool CNavigatorDlg::IsItemToBeAcquired(CMapDrawItem *item, bool &skippingGroup)
{
  skippingGroup = false;
  if (item->mRegistration == mCurrentRegistration && 
    ((item->mAcquire && mAcqParm->acquireType != ACQUIRE_DO_TS) || 
    (item->mTSparamIndex >= 0 && mAcqParm->acquireType == ACQUIRE_DO_TS))) {
      if (item->mGroupID && item->mGroupID == mGroupIDtoSkip)
        skippingGroup = true;
      else
        return true;
  }
  return false;
}

// Return a map or other item with the given ID if it exists, otherwise return NULL
CMapDrawItem * CNavigatorDlg::FindItemWithMapID(int mapID, bool requireMap, 
  bool matchGroup)
{
  CMapDrawItem *item;
  if (!mapID)
    return NULL;
  for (mFoundItem = 0; mFoundItem < mItemArray.GetSize(); mFoundItem++) {
    item = mItemArray[mFoundItem];
    if ((item->IsMap() || !requireMap) && 
      ((!matchGroup && item->mMapID == mapID) || (matchGroup && item->mGroupID == mapID)))
      return item;
  }
  mFoundItem = -1;
  return NULL;
}

// Return item whose label or note matchs the given string, case insensitive
CMapDrawItem * CNavigatorDlg::FindItemWithString(CString & string, BOOL ifNote, 
  bool caseSensitive)
{
  CMapDrawItem *item;
  if (string.IsEmpty())
    return NULL;
  for (mFoundItem = 0; mFoundItem < mItemArray.GetSize(); mFoundItem++) {
    item = mItemArray[mFoundItem];
    if ((!caseSensitive && ((!ifNote && !string.CompareNoCase(item->mLabel)) ||
      (ifNote && !string.CompareNoCase(item->mNote)))) ||
      (caseSensitive && ((!ifNote && !string.Compare(item->mLabel)) ||
      (ifNote && !string.Compare(item->mNote)))))
      return item;
  }
  mFoundItem = -1;
  return NULL;
}

// Return the montage map item that the item was drawn on.  Item can be NULL
CMapDrawItem *CNavigatorDlg::FindMontMapDrawnOn(CMapDrawItem *item)
{
 CMapDrawItem *mapItem;
 if (!item || item->IsNotPoint() || item->mDrawnOnMapID <= 0)
    return NULL;
 mapItem = FindItemWithMapID(item->mDrawnOnMapID);
 if (!mapItem || !mapItem->mMapMontage)
   return NULL;
 return mapItem;
}

// Look for a montage map in a buffer with the given ID, return the buffer index
// or -1 if it is not a montage or not in a buffer
int CNavigatorDlg::FindBufferWithMontMap(int mapID)
{
  CMapDrawItem *item = FindItemWithMapID(mapID);
  if (item && item->mMapMontage)
    for (int i = 0; i < MAX_BUFFERS; i++)
      if (mImBufs[i].mMapID == mapID && mImBufs[i].mImage)
        return i;
  return -1;
}

// If the selection list has only a single item, return it and optionally return its index
// if index not NULL (the default); otherwise return NULL
CMapDrawItem *CNavigatorDlg::GetSingleSelectedItem(int *index)
{ 
  std::set<int>::iterator iter;
  if (mSelectedItems.size() != 1)
    return NULL;
  iter = mSelectedItems.begin();
  if (*iter < 0 || *iter >= mItemArray.GetSize())
    return NULL;
  if (index)
    *index = *iter;
  return mItemArray.GetAt(*iter);
}

// Get backlash used to acquire map montage or the one this item was drawn on
// Return true if one exists, false and set values to -999 if not
bool CNavigatorDlg::BacklashForItem(CMapDrawItem *item, float &backX, float &backY)
{
  backX = backY = -999.;
  if (!item->mBacklashX && !item->mBacklashY && item->mDrawnOnMapID) {
    item = FindItemWithMapID(item->mDrawnOnMapID);
    if (!item)
      return false;
  }
  if ((!item->mBacklashX && !item->mBacklashY) || 
    (mHelper->GetRealignTestOptions() & 1) == 0 || 
    item->mRegistration != item->mOriginalReg)
      return false;
  backX = item->mBacklashX;
  backY = item->mBacklashY;
  return true;
}


void CNavigatorDlg::MontErrForItem(CMapDrawItem *inItem, float &montErrX, float &montErrY)
{
  CMapDrawItem *item;
  MontParam montParam;
  MontParam *montP = &montParam;
  KImageStore *imageStore;
  std::vector<int> pieceSavedAt;
  ScaleMat aMat;
  int curStore;
  float useWidth, useHeight, delX, delY;
  montErrX = montErrY = 0.;
  item = FindItemWithMapID(inItem->mDrawnOnMapID);
  if (!item || !item->mMapMontage || !item->mMontUseStage || 
    item->mRegistration != item->mOriginalReg)
    return;
  if (AccessMapFile(item, imageStore, curStore, montP, useWidth, useHeight))
    return;
  mWinApp->mMontageController->ListMontagePieces(imageStore, montP, 
      item->mMapSection, pieceSavedAt);

  aMat = mShiftManager->StageToCamera(item->mMapCamera, item->mMapMagInd);
  if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
    mShiftManager->AdjustStageToCameraForTilt(aMat, item->mMapTiltAngle);

  // Get stage then montage coordinate relative to center of the montage
  delX = inItem->mStageX - item->mStageX;
  delY = inItem->mStageY - item->mStageY;
  mHelper->InterpMontStageOffset(imageStore, montP, aMat, pieceSavedAt, delX, delY, 
    montErrX, montErrY);

  // Finish up with the file if it was not open before
  if (curStore < 0) {
    if (imageStore->getStoreType() == STORE_TYPE_MRC)
      delete (KStoreMRC *)imageStore;
    else if (imageStore->getStoreType() == STORE_TYPE_ADOC)
      delete (KStoreADOC *)imageStore;
    else
      delete (KStoreIMOD *)imageStore;
  }
}

// Return a matrix for rotation including an inversion if any
ScaleMat CNavigatorDlg::GetRotationMatrix(float rotAngle, BOOL inverted)
{
  ScaleMat rMat;
  float sign;
  rMat.xpx = (float)cos(DTOR * rotAngle);   
  rMat.ypx = (float)sin(DTOR * rotAngle);
  sign = inverted ? -1.f : 1.f;
  rMat.xpy = -sign * rMat.ypx;
  rMat.ypy = sign * rMat.xpx;
  return rMat;
}

// Test for whether montage adjustments are needed; return -1 if not, 0 if so with
// unrotated montage, or 1 for rotated map.  Return transformations in that case
int CNavigatorDlg::PrepareMontAdjustments(EMimageBuffer *imBuf, ScaleMat &rMat,
                                          ScaleMat &rInv, float &rDelX, float &rDelY)
{
  CMapDrawItem *item = NULL;
  float width = (float)imBuf->mImage->getWidth();
  float height = (float)imBuf->mImage->getHeight();

  // No offsets, nothing to do
  if (!imBuf->mMiniOffsets)
    return -1;
  item = FindItemWithMapID(imBuf->mMapID);
  if (item || imBuf->mStage2ImMat.xpx) {

    // If its a map with an item still and there is some sign of rotation,
    // prepare the rotation transform and its inverse for inverted Y
    if (imBuf->mRotAngle || imBuf->mInverted || width != imBuf->mLoadWidth || 
      height != imBuf->mLoadHeight) {
        rMat = GetRotationMatrix(imBuf->mRotAngle, imBuf->mInverted);
        rMat.xpy *= -1.;
        rMat.ypx *= -1.;
        rInv = MatInv(rMat);
        rDelX = (float)(width / 2. - rMat.xpx * (imBuf->mLoadWidth / 2.) - 
          rMat.xpy * (imBuf->mLoadHeight / 2.));
        rDelY = (float)(height / 2. - 
          rMat.ypx * (imBuf->mLoadWidth / 2.) - rMat.ypy * (imBuf->mLoadHeight / 2.));
        return 1;
    }

    // If it's a map with no item, forbid adjustment if obviously rotated
  } else if (!item && (imBuf->mRotAngle || imBuf->mInverted))
    return -1;
  return 0;
}

// Adjust the given position if there is a montage with piece offsets in the buffer
void CNavigatorDlg::AdjustMontImagePos(EMimageBuffer *imBuf, float & inX, float & inY,
  int *pcInd, float *xInPiece, float *yInPiece)
{
  ScaleMat rMat, rInv;
  float rDelX, rDelY, testX, testY, minXinPc, minYinPc;
  MiniOffsets *mini = imBuf->mMiniOffsets;
  int minInd;
  int adjust = PrepareMontAdjustments(imBuf, rMat, rInv, rDelX, rDelY);
  if (pcInd)
    *pcInd = -1;
  if (xInPiece)
    *xInPiece = -1.;
  if (yInPiece)
    *yInPiece = -1.;
  
  // if nothing to adjust return;
  if (adjust < 0)
    return;
  testX = inX;
  testY = inY;

  // If rotations, back-rotate coordinates to original map
  if (adjust) {
    testX = rInv.xpx * (inX - rDelX) + rInv.xpy * (inY - rDelY);
    testY = rInv.ypx * (inX - rDelX) + rInv.ypy * (inY - rDelY);
  }

  if (OffsetMontImagePos(mini, 0, mini->xNframes - 1, 0, mini->yNframes - 1, testX, 
    testY, minInd, minXinPc, minYinPc))
    return;

  if (pcInd)
    *pcInd = minInd;

  // Return unbinned, right-handed coordinates in the piece
  if (xInPiece)
    *xInPiece = minXinPc * B3DMAX(1, imBuf->mOverviewBin);
  if (yInPiece)
    *yInPiece = minYinPc * B3DMAX(1, imBuf->mOverviewBin);
  inX = testX;
  inY = testY;
  if (adjust) {
    inX = rMat.xpx * testX + rMat.xpy * testY + rDelX;
    inY = rMat.ypx * testX + rMat.ypy * testY + rDelY;
  }
}

int CNavigatorDlg::OffsetMontImagePos(MiniOffsets *mini, int xPcStart, int xPcEnd,
  int yPcStart, int yPcEnd, float &testX, float &testY, int &pcInd, float &xInPiece, 
  float &yInPiece)
{
  int ix, iy, index, xst, xnd, yst, ynd, xDist, yDist, minInd, minDist, xTest, yTest;

  // When there are two frames, the base is 0 and the delta is frame - overlap / 2
  // So get a correct adjustment from xst/yst to real start of second piece
  // which is also the amount to fix xnd/ynd when there are two pieces
  int xstAdjForInPc = mini->xNframes == 2 ? mini->xFrame - mini->xDelta : mini->xBase;
  int ystAdjForInPc = mini->yNframes == 2 ? mini->yFrame - mini->yDelta : mini->yBase;
  xTest = (int)testX;
  yTest = (int)testY;

  // Loop on pieces in given range; this piece numbering iy is inverted in Y!
  minDist = 100000000;
  pcInd = -1;
  xInPiece = yInPiece = -1.f;
  for (ix = xPcStart; ix <= xPcEnd; ix++) {
    for (iy = yPcStart; iy <= yPcEnd; iy++) {

      // Piece index in the array; Y was done before X and in order, so these indexes
      // follow normal right-hand numbering; incoming coordinates are inverted
      index = (ix + 1) * mini->yNframes - 1 - iy;
      if (mini->offsetX[index] == MINI_NO_PIECE)
        continue;

      // Get the boundaries for this piece, adding the offsets
      xst = mini->xBase + ix * mini->xDelta + mini->offsetX[index];
      xnd = xst + mini->xDelta;
      if (!ix)
        xst = mini->offsetX[index];
      if (ix == mini->xNframes - 1) {
        xnd = mini->xFrame + ix * mini->xDelta + mini->offsetX[index];
        if (mini->xNframes == 2)
          xnd -= xstAdjForInPc;
      }
      yst = mini->yBase + iy * mini->yDelta + mini->offsetY[index];
      ynd = yst + mini->yDelta;
      if (!iy)
        yst = mini->offsetY[index];
      if (iy == mini->yNframes - 1) {
        ynd = mini->yFrame + iy * mini->yDelta + mini->offsetY[index];
        if (mini->yNframes == 2)
          ynd -= ystAdjForInPc;
      }
      
      // Get the "distance" from point to piece, negative if inside
      if (xTest < xst)
        xDist = xst - xTest;
      else if (xTest > xnd)
        xDist = xTest - xnd;
      else
        xDist = -B3DMIN(xTest - xst, xnd - xTest);
      if (yTest < yst)
        yDist = yst - yTest;
      else if (yTest > ynd)
        yDist = yTest - ynd;
      else
        yDist = -B3DMIN(yTest - yst, ynd - yTest);
      xDist = B3DMAX(xDist, yDist);

      // Keep track of closest piece
      if (xDist < minDist) {
        minDist = xDist;
        minInd = index;
        if (!mini->subsetLoaded) {
          pcInd = minInd;

          // Return coordinates in piece with the binning of overview, but right-handed
          // Need to adjust starting coordinates by the base to be the actual coordinate
          // of the full piece and not of the subset pasted in to the overview
          xInPiece = (float)(testX - (xst - B3DCHOICE(ix > 0, xstAdjForInPc, 0)));
          yInPiece = (float)(mini->yFrame + (yst - B3DCHOICE(iy > 0, ystAdjForInPc, 0)) - 
            testY);
        }
      }
    }
  }
  if (minDist >= 10000000)
    return 1;

  // Subtract offsets to get coordinates in unshifted piece
  testX -= mini->offsetX[minInd];
  testY -= mini->offsetY[minInd];
  return 0;
}

// Compute an image rotation from a stage to camera matrix in curMat and the 
// stage to image matrix of a map
float CNavigatorDlg::RotationFromStageMatrices(ScaleMat curMat, ScaleMat mapMat, 
                                               BOOL &inverted)
{
  ScaleMat aProd;
  double xtheta, ytheta, ydtheta;

  // Get the transformation from the map's current matrix to the defined stage to camera
  // one as the inverse of the map transform times the stage to camera transform
  // But get the map transform back to right handed coordinates by inverting terms
  mapMat.ypx *= -1.;
  mapMat.ypy *= -1.;
  aProd = MatMul(MatInv(mapMat), curMat);
  xtheta = atan2(aProd.ypx, aProd.xpx) / DTOR;
  ytheta = atan2(-aProd.xpy, aProd.ypy) / DTOR;
  ydtheta = UtilGoodAngle(ytheta - xtheta);

  // If the angles differ by a lot, then there is an inversion, so adjust the
  // Y angle by 180 and recompute the difference
  inverted = false;
  if (ydtheta < -90 || ydtheta > 90) {
    ytheta = UtilGoodAngle(ytheta + 180.);
    ydtheta = UtilGoodAngle(ytheta - xtheta);
    inverted = true;
  }
  return ((float)UtilGoodAngle(xtheta + 0.5 * ydtheta));
}

// Returns true if the item has a supermontage-type label and returns components
BOOL CNavigatorDlg::SupermontLabel(CMapDrawItem * item, int & startNum, int & ix, int & iy)
{
  int len = item->mLabel.GetLength();
  int index2, index = item->mLabel.Find('-');
  if (index <= 0 || index == len - 1 || item->IsNotPolygon())
    return false;
  index2 = item->mLabel.Find('-', index + 1);
  if (index2 <= index || index2 == len - 1 || item->mLabel.Find('-', index2 + 1) >= 0)
    return false;
  startNum = atoi((LPCTSTR)(item->mLabel.Left(index)));
  ix = atoi((LPCTSTR)(item->mLabel.Mid(index + 1, index2 - index - 1)));
  iy = atoi((LPCTSTR)(item->mLabel.Right(len - index2 - 1)));
  return startNum > 0 && ix > 0 && iy > 0;
}

BOOL CNavigatorDlg::ImBufIsImportedMap(EMimageBuffer * imBuf)
{
  CMapDrawItem *item = FindItemWithMapID(imBuf->mMapID);
  return (item && item->mImported);
}

// Count items with same group #, return first and last label encountered, optionally
// return a list of indices in indexVec
int CNavigatorDlg::CountItemsInGroup(int curID, CString & label, CString & lastlab, 
                                     int &numAcquire, IntVec *indexVec)
{
  CMapDrawItem *item;
  int num = 0;
  numAcquire = 0;
  label = "";
  if (indexVec)
    indexVec->clear();
  for (int i = 0; i < mItemArray.GetSize(); i++) {
    item = mItemArray[i];
    if (item->mGroupID == curID) {
      num++;
      if (item->mAcquire)
        numAcquire++;
      if (label.IsEmpty())
        label = item->mLabel;
      lastlab = item->mLabel;
      if (indexVec)
        indexVec->push_back(i);
    }
  }
  return num;
}

// Return supermont coords of acquire item or item that a macro said to use
void CNavigatorDlg::GetSuperCoords(int &superX, int &superY)
{
  CMapDrawItem *item;
  int index = mAcquireIndex >= 0 ? mAcquireIndex : mSuperCoordIndex;
  if (index < 0 || index >= mItemArray.GetSize())
    return;
  item = mItemArray[index];
  superX = item->mSuperMontX;
  superY = item->mSuperMontY;
}


int CNavigatorDlg::GroupScheduledIndex(int ID)
{
  ScheduledFile *sched;
  if (!ID)
    return -1;
  for (int i = 0; i < mGroupFiles.GetSize(); i++) {
    sched = (ScheduledFile *)mGroupFiles.GetAt(i);
    if (sched->groupID == ID)
      return i;
  }
  return -1;
}

// Sets the current item as long and runs UpdateData as long as it is not collapsed group
bool CNavigatorDlg::UpdateIfItem(void)
{
  mWinApp->RestoreViewFocus();
  if (!SetCurrentItem())
    return true;
  UpdateData(true);
  return false;
}

// Make the mappings between list box and item array when groups are collapsed
// mItemToList gives the index in the list box of each item in the array
// mListToItem has the item index for a list item without a group #, or it
// -1 - item index of first item in a line corresponding to a collapsed group
void CNavigatorDlg::MakeListMappings(void)
{
  int i, list, lastGroup, num = (int)mItemArray.GetSize();
  CMapDrawItem *item;
  CLEAR_RESIZE(mItemToList, int, num);
  lastGroup = -1;
  list = 0;
  for (i = 0; i < num; i++) {
    item = mItemArray[i];
    if (item->mGroupID <= 0) {
      lastGroup = -1;
      list++;
    } else if (item->mGroupID != lastGroup) {
      lastGroup = item->mGroupID;
      list++;
    }
    mItemToList[i] = list - 1;
  }
  CLEAR_RESIZE(mListToItem, int, list);
  for (i = 0; i < num; i++) {
    item = mItemArray[i];
    if (item->mGroupID <= 0)
      mListToItem[mItemToList[i]] = i;
    else if (!i || mItemToList[i] != mItemToList[i-1])
      mListToItem[mItemToList[i]] = -1 - i;
  }
}

// Get the starting and ending item index for a collapsed group at listInd in list box
// Returns true if it is collapsed group, false otherwise
bool CNavigatorDlg::GetCollapsedGroupLimits(int listInd, int &start, int &end)
{
  if (listInd < 0 || !m_bCollapseGroups || mListToItem[listInd] >= 0)
    return false;
  start = -1 - mListToItem[listInd];
  if (listInd >= (int)mListToItem.size() - 1)
    end = (int)mItemArray.GetSize() - 1;
  else if (mListToItem[listInd+1] >= 0)
    end = mListToItem[listInd+1] - 1;
  else
    end = -2 - mListToItem[listInd+1];
  return true;
}

// Get the item index of the element at listInd in the list box, or the index of the
// first item if it is a group.  Returns true if it is a collapsed group
bool CNavigatorDlg::IndexOfSingleOrFirstInGroup(int listInd, int & itemInd)
{
  itemInd = listInd;
  if (m_bCollapseGroups && listInd >= 0) {
    if (mListToItem[listInd] < 0) {
      itemInd = -1 - mListToItem[listInd];
      return true;
    }
    itemInd = mListToItem[listInd];
  }
  return false;
}

// Change the collapse state internally
void CNavigatorDlg::SetCollapsing(BOOL state)
{
  if (m_bCollapseGroups && state || !m_bCollapseGroups && !state)
    return;
  m_bCollapseGroups = state;
  UpdateData(false);
  OnCollapseGroups();
}

// Replace the title line with a string and hide its parts, or restore the parts if
// str in Label, its default value.  Separate parts stay lined up at different DPIs
void CNavigatorDlg::ManageListHeader(CString str)
{
  int ind, hideShow = str == "Label" ? SW_SHOW : SW_HIDE;
  m_statListHeader.SetWindowText(str);
  for (ind = 0; ind < 8; ind++) {
    CStatic *part = (CStatic *)GetDlgItem(IDC_STATLISTHEADER2 + ind);
    if (part)
      part->ShowWindow(hideShow);
  }
}

// Store suffixes of extra files to open
void CNavigatorDlg::SetExtraFileSuffixes(CString *items, int numItems)
{
  mNumExtraFileSuffixes = numItems;
  for (int i = 0; i < numItems; i++)
    mExtraFileSuffix[i] = items[i];
}

// See if new current item raw stage position matches that of any other(s) and assign them
// the same ID
void CNavigatorDlg::CheckRawStageMatches(void)
{
  CMapDrawItem *curItem, *item;
  bool pending;
  float tol = mScope->GetBacklashTolerance();
  int lastMoveID = -1;
  curItem = mItemArray[mCurrentItem];
  if (mLastMoveStageID > 0) {
    if (mScope->StageIsAtSamePos((double)curItem->mRawStageX, (float)curItem->mRawStageY,
      mRequestedStageX, mRequestedStageY))
      lastMoveID = mLastMoveStageID;
  }
  for (int loop = 0; loop < 3; loop++) {
    pending = false;
    for (int ind = 0; ind < mItemArray.GetSize(); ind++) {
      if (ind == mCurrentItem)
        continue;
      item = mItemArray[ind];
      if ((item->mRawStageX > RAW_STAGE_TEST && 
        fabs((double)(curItem->mRawStageX - item->mRawStageX)) < tol &&
        fabs((double)(curItem->mRawStageY - item->mRawStageY)) < tol) ||
        (lastMoveID > 0 && item->mRawStageX < RAW_STAGE_TEST && 
        item->mMoveStageID == lastMoveID)) {
          if (!loop) {

            // First time through, assign only if other item has a same pos ID already
            if (item->mAtSamePosID > 0)
              curItem->mAtSamePosID = item->mAtSamePosID;
            else
              pending = true;
          } else {

            // Otherwise, assign current item's same pos ID to matching item, or current
            // item's map ID to the same pos ID of both items, or the other items's map
            // ID to both.  Second time, a pair with another nonmap is pending but this
            // is assigned on the third round if there was a map to pair with
            if (curItem->mAtSamePosID > 0)
              item->mAtSamePosID = curItem->mAtSamePosID;
            else if (curItem->mMapID > 0)
              item->mAtSamePosID = curItem->mAtSamePosID = curItem->mMapID;
            else if (item->mMapID > 0)
              item->mAtSamePosID = curItem->mAtSamePosID = item->mMapID;
            else
              pending = true;
          }
      }
    }
    if (!pending)
      break;
  }
}

// Assign the backlash to the given item either from the map item of a buffer, if there is
// one, or from the image buffer
void CNavigatorDlg::TransferBacklash(EMimageBuffer *imBuf, CMapDrawItem *item)
{
  CMapDrawItem *mapItem = FindItemWithMapID(imBuf->mMapID);
  item->mBacklashX = mapItem ? mapItem->mBacklashX : imBuf->mBacklashX;
  item->mBacklashY = mapItem ? mapItem->mBacklashY : imBuf->mBacklashY;
}

// When backlash adjustment measurement is done, adjust the item it was launched for and
// for other items marked as being at the same position
void CNavigatorDlg::BacklashFinished(float deltaX, float deltaY)
{
  CMapDrawItem *item;
  if (mBacklashItemInd >= mItemArray.GetSize())
    return;
  mItem = mItemArray[mBacklashItemInd];
  for (int ind = 0; ind < mItemArray.GetSize(); ind++) {
    item = mItemArray[ind];
    if ((mItem->mAtSamePosID > 0 && mItem->mAtSamePosID == item->mAtSamePosID) ||
      ind == mBacklashItemInd) {

        // Adjust raw stage if any
        if (item->mRawStageX > RAW_STAGE_TEST) {
          item->mRawStageX += deltaX;
          item->mRawStageY += deltaY;
        }

        // Adjust stage and points
        item->mStageX += deltaX;
        item->mStageY += deltaY;
        ShiftItemPoints(item, deltaX, deltaY);
        mWinApp->mMontageController->GetColumnBacklash(item->mBacklashX, item->mBacklashY);
        UpdateListString(ind);
        SEMTrace('1', "Adjusted item %d (%s)", ind + 1, (LPCTSTR)item->mLabel);
    }
  }
  Update();
  Redraw();
}

// Test for whether the current item can have a backlash adjusted: has a raw stage,
// does not have a backlash, and stage is nominally at that position
bool CNavigatorDlg::OKtoAdjustBacklash(bool fastStage)
{
  double stageZ;
  float tol = mScope->GetBacklashTolerance();
  if (!SetCurrentItem())
    return false;
  if (mItem->mBacklashX != 0. || mItem->mRawStageX < RAW_STAGE_TEST)
    return false;
  if (fastStage)
    mScope->FastStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ);
  else
    mScope->GetStagePosition(mLastScopeStageX, mLastScopeStageY, stageZ);
  return fabs(mLastScopeStageX - mItem->mRawStageX) < tol && 
    fabs(mLastScopeStageY - mItem->mRawStageY) < tol;
}

void CNavigatorDlg::AdjustBacklash(int index, bool showC)
{
  CMapDrawItem *item;
  float backlashX, backlashY;
  mWinApp->mMontageController->GetColumnBacklash(backlashX, backlashY);
  if (!OKtoAdjustBacklash(false))
    return;
  mBacklashItemInd = index >= 0 ? index : mCurrentItem;
  item = mItemArray[mBacklashItemInd];
  if (item->mMapMontage)
    mBufferManager->CopyImageBuffer(1, 0);
  mWinApp->mComplexTasks->BacklashAdjustStagePos(backlashX, backlashY, true, showC);
}

// Functions to provide central answer to whether initial stage move can be skipped
int CNavigatorDlg::OKtoSkipStageMove(NavAcqParams *param)
{
  return OKtoSkipStageMove(mAcqActions, param->acquireType);
}

int CNavigatorDlg::OKtoSkipStageMove(NavAcqAction *actions, int acqType)
{
  if ((actions[NAACT_RUN_PREMACRO].flags & NAA_FLAG_RUN_IT) != 0 || 
    acqType == ACQUIRE_RUN_MACRO)
    return 1;
  return 0;
}

// Returns the number of point items in the current group; if maxPoints is > 0, returns
// with maxPoints + 1 if number of items exceeds maxPoints; if stageX and stageY are
// non-NULL, returns the stage coordinates of the points
int CNavigatorDlg::GetCurrentGroupSizeAndPoints(int maxPoints, float *stageX, 
  float *stageY, float *defocusOffset)
{
  int ind, num = 0;
  CMapDrawItem *item;
  if (!SetCurrentItem(true) || mItem->IsNotPoint())
    return 0;
  if (defocusOffset)
    *defocusOffset = mItem->mDefocusOffset;
  for (ind = 0; ind < mItemArray.GetSize(); ind++) {
    item = mItemArray[ind];
    if (item->mGroupID == mItem->mGroupID && item->IsPoint()) {
      num++;
      if (maxPoints > 0 && num > maxPoints)
        return num;
      if (stageX)
        stageX[num - 1] = item->mStageX;
      if (stageY)
        stageY[num - 1] = item->mStageY;
    }
  }
  return num;
}

// Goes to the current item (with IS or not) and selects the next item in the given
// direction if nonzero; intended to be used without collapsed groups
void CNavigatorDlg::IStoXYandAdvance(int &direction)
{
  OnGotoXy();
  if (direction && mCurrentItem + direction >= 0 && 
    mCurrentItem + direction < mItemArray.GetSize()) {
    SetCurrentSelection(mCurListSel + direction);
  } else
    direction = 0;
}

// Returns the index of first and last item in the same group as the given item and in
// a contiguous set of items in same group
int CNavigatorDlg::LimitsOfContiguousGroup(int itemInd, int &groupStart, int & groupEnd)
{
  CMapDrawItem *item;
  int ind, groupID;
  item = mItemArray.GetAt(itemInd);
  groupID = item->mGroupID;
  for (ind = itemInd; ind >= 0; ind--) {
    item = mItemArray.GetAt(ind);
    if (item->mGroupID == groupID)
      groupStart = ind;
    else
      break;
  }
  for (ind = itemInd; ind < mItemArray.GetSize(); ind++) {
    item = mItemArray.GetAt(ind);
    if (item->mGroupID == groupID)
      groupEnd = ind;
    else
      break;
  }
  return groupEnd + 1 - groupStart;
}

// Sets the current selected item and updates display and window
void CNavigatorDlg::SetCurrentSelection(int listInd)
{
  if (listInd < 0 || listInd >= m_listViewer.GetCount())
    return;
  mCurListSel = listInd;
  m_listViewer.SetCurSel(mCurListSel);
  if (!m_bCollapseGroups)
    mCurrentItem = mCurListSel;
  else if (mListToItem[listInd] < 0)
    mCurrentItem = -1 - mListToItem[listInd];
  else
    mCurrentItem = mListToItem[listInd];
  ManageCurrentControls();
  Redraw();
}

// Returns the area inside the contour points
float CNavigatorDlg::ContourArea(float *ptx, float *pty, int numPoints)
{
  float sum = 0.;
  int i, next;
  if (numPoints < 3)
    return 0.;

  for (i = 0; i < numPoints; i++) {
    next = (i + 1) % numPoints;
    sum += (ptx[i] * pty[next]) - (pty[i] * ptx[next]);
  }
  return (float)fabs(sum * 0.5f);
}

// Returns the directory of the nav file, or the current dir if there is no file
void CNavigatorDlg::GetCurrentNavDir(CString &navPath)
{
  CString str;
  if (mNavFilename.IsEmpty())
    navPath = mDocWnd->GetOriginalCwd();
  else
    UtilSplitPath(mNavFilename, navPath, str);
  if (!navPath.IsEmpty() && navPath.GetAt(navPath.GetLength() - 1) != '\\')
    navPath += '\\';
}

void CNavigatorDlg::SetCurrentNavFile(CString & inFile)
{
  CString str, str2;
  mNavFilename = inFile;
  UtilSplitPath(mNavFilename, str, str2);
  SetWindowText("Navigator:  " + str2);
}
