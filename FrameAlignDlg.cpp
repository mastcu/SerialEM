// FrameAlignDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "FrameAlignDlg.h"
#include "CameraController.h"
#include "Utilities\KGetOne.h"
#include "XFolderDialog\XFolderDialog.h"

static int idTable[] = {IDC_STAT_WHERE_ALIGN, IDC_USE_FRAME_ALIGN, IDC_RALIGN_IN_DM, 
  IDC_RWITH_IMOD, PANEL_END,
  IDC_STAT_FILTER, IDC_COMBOFILTER, PANEL_END,
  IDC_USE_GPU, IDC_BUT_NEW_PARAMS, IDC_LIST_SET_NAMES, IDC_STAT_PARAM_NAME,
  IDC_EDIT_NAME, IDC_TRUNCATE_ABOVE, IDC_STAT_COUNT_LABEL, IDC_EDIT_TRUNCATION,
  IDC_STAT_CUTOFF_LABEL, IDC_EDIT_CUTOFF, IDC_STAT_ALIGN_METHOD, IDC_BUT_DELETE_SET,
  IDC_RPAIRWISE_NUM, IDC_RPAIRWISE_HALF, IDC_RPAIRWISE_ALL, IDC_RACCUM_REF,
  IDC_SPIN_PAIRWISE_NUM, IDC_STAT_PAIRWISE_NUM, IDC_STAT_FRAME_LABEL, IDC_WHOLE_SERIES,
  IDC_STAT_ALIGN_BIN, IDC_SPIN_ALIGN_BIN, IDC_REFINE_ALIGNMENT, IDC_BUT_SET_FOLDER,
  IDC_SPIN_REFINE_ITER, IDC_STAT_REFINE_ITER, IDC_STAT_ITER_LABEL,
  IDC_GROUP_FRAMES, IDC_STAT_GROUP_SIZE, IDC_SPIN_GROUP_SIZE,
  IDC_STAT_GROUP_NEEDS, IDC_SMOOTH_SHIFTS, IDC_STAT_SMOOTH_CRIT,
  IDC_STAT_MAX_SHIFT, IDC_EDIT_MAX_SHIFT, IDC_STAT_PIXEL_LABEL,
  IDC_SPIN_SMOOTH_CRIT, IDC_STAT_SMOOTH_LABEL, IDC_BUTMORE,
  IDC_STAT_MORE_PARAMS, PANEL_END,
  IDC_REFINE_GROUPS, IDC_STAT_REFINE_CUTOFF, IDC_EDIT_REFINE_CUTOFF,
  IDC_EDIT_CUTOFF2, IDC_EDIT_CUTOFF3, IDC_HYBRID_SHIFTS, IDC_EDIT_SIGMA_RATIO,
  IDC_STAT_SIGMA_RATIO, IDC_STAT_STOP_ITER, IDC_EDIT_STOP_ITER, IDC_STAT_OTHER_CUTS,
  IDC_STAT_STOP_LABEL, IDC_KEEP_PRECISION, IDC_SAVE_FLOAT_SUMS, PANEL_END,
  IDOK, IDCANCEL, IDC_BUTHELP, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];

// CFrameAlignDlg dialog

CFrameAlignDlg::CFrameAlignDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CFrameAlignDlg::IDD, pParent)
  , m_bUseGPU(FALSE)
  , m_iWhereAlign(0)
  , m_iAlignStrategy(0)
  , m_strCurName(_T(""))
  , m_bTruncateAbove(FALSE)
  , m_fTruncation(0)
  , m_strPairwiseNum(_T(""))
  , m_strAlignBin(_T(""))
  , m_fCutoff(0)
  , m_iMaxShift(0)
  , m_bGroupFrames(0)
  , m_strGroupSize(_T(""))
  , m_strGroupNeeds(_T(""))
  , m_bRefineAlignment(FALSE)
  , m_strRefineIter(_T(""))
  , m_bSmoothShifts(FALSE)
  , m_strSmoothCrit(_T(""))
  , m_bRefineGroups(FALSE)
  , m_strRefineCutoff(_T(""))
  , m_fStopIter(0)
  , m_bHybridShifts(FALSE)
  , m_fSigmaRatio(0)
  , m_strCutoff2(_T(""))
  , m_strCutoff3(_T(""))
  , m_bKeepPrecision(FALSE)
  , m_bSaveFloatSums(FALSE)
  , m_bWholeSeries(FALSE)
{
  mMoreParamsOpen = false;
}

CFrameAlignDlg::~CFrameAlignDlg()
{
}

void CFrameAlignDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_USE_GPU, m_bUseGPU);
  DDX_Control(pDX, IDC_STAT_PAIRWISE_NUM, m_statPairwiseNum);
  DDX_Control(pDX, IDC_SPIN_PAIRWISE_NUM, m_sbcPairwiseNum);
  DDX_Control(pDX, IDC_SPIN_ALIGN_BIN, m_sbcAlignBin);
  DDX_Control(pDX, IDC_SPIN_GROUP_SIZE, m_sbcGroupSize);
  DDX_Control(pDX, IDC_SPIN_REFINE_ITER, m_sbcRefineIter);
  DDX_Control(pDX, IDC_SPIN_SMOOTH_CRIT, m_sbcSmoothCrit);
  DDX_Radio(pDX, IDC_RALIGN_IN_DM, m_iWhereAlign);
  DDX_Radio(pDX, IDC_RPAIRWISE_NUM, m_iAlignStrategy);
  DDX_Text(pDX, IDC_EDIT_NAME, m_strCurName);
  DDV_MaxChars(pDX, m_strCurName, 28);
  DDX_Check(pDX, IDC_TRUNCATE_ABOVE, m_bTruncateAbove);
  DDX_Text(pDX, IDC_EDIT_TRUNCATION, m_fTruncation);
  DDV_MinMaxFloat(pDX, m_fTruncation, 0, 1000.f);
  DDX_Text(pDX, IDC_STAT_PAIRWISE_NUM, m_strPairwiseNum);
  DDX_Text(pDX, IDC_STAT_ALIGN_BIN, m_strAlignBin);
  DDX_Text(pDX, IDC_EDIT_CUTOFF, m_fCutoff);
  DDV_MinMaxFloat(pDX, m_fCutoff, 0.005f, 0.5f);
  DDX_Text(pDX, IDC_EDIT_MAX_SHIFT, m_iMaxShift);
  DDV_MinMaxInt(pDX, m_iMaxShift, 2, 100);
  DDX_Check(pDX, IDC_GROUP_FRAMES, m_bGroupFrames);
  DDX_Control(pDX, IDC_STAT_GROUP_SIZE, m_statGroupSize);
  DDX_Text(pDX, IDC_STAT_GROUP_SIZE, m_strGroupSize);
  DDX_Control(pDX, IDC_STAT_GROUP_NEEDS, m_statGroupNeeds);
  DDX_Text(pDX, IDC_STAT_GROUP_NEEDS, m_strGroupNeeds);
  DDX_Check(pDX, IDC_REFINE_ALIGNMENT, m_bRefineAlignment);
  DDX_Control(pDX, IDC_STAT_ITER_LABEL, m_statIterLabel);
  DDX_Control(pDX, IDC_STAT_REFINE_ITER, m_statRefineIter);
  DDX_Text(pDX, IDC_STAT_REFINE_ITER, m_strRefineIter);
  DDX_Check(pDX, IDC_SMOOTH_SHIFTS, m_bSmoothShifts);
  DDX_Control(pDX, IDC_STAT_SMOOTH_LABEL, m_statSmoothLabel);
  DDX_Control(pDX, IDC_STAT_SMOOTH_CRIT, m_statSmoothCrit);
  DDX_Text(pDX, IDC_STAT_SMOOTH_CRIT, m_strSmoothCrit);
  DDX_Control(pDX, IDC_REFINE_GROUPS, m_butRefineGroups);
  DDX_Check(pDX, IDC_REFINE_GROUPS, m_bRefineGroups);
  DDX_Control(pDX, IDC_STAT_REFINE_CUTOFF, m_statRefineCutoff);
  DDX_Control(pDX, IDC_EDIT_REFINE_CUTOFF, m_editRefineCutoff);
  DDX_Text(pDX, IDC_EDIT_REFINE_CUTOFF, m_strRefineCutoff);
  DDX_Control(pDX, IDC_STAT_STOP_ITER, m_statStopIter);
  DDX_Control(pDX, IDC_EDIT_STOP_ITER, m_editStopIter);
  DDX_Text(pDX, IDC_EDIT_STOP_ITER, m_fStopIter);
  DDV_MinMaxFloat(pDX, m_fStopIter, .01f, 5.f);
  DDX_Control(pDX, IDC_STAT_STOP_LABEL, m_statStopLabel);
  DDX_Control(pDX, IDC_HYBRID_SHIFTS, m_butHybridShifts);
  DDX_Check(pDX, IDC_HYBRID_SHIFTS, m_bHybridShifts);
  DDX_Text(pDX, IDC_EDIT_SIGMA_RATIO, m_fSigmaRatio);
  DDV_MinMaxFloat(pDX, m_fSigmaRatio, 0.01f, 20.f);
  DDX_Text(pDX, IDC_EDIT_CUTOFF2, m_strCutoff2);
  DDX_Text(pDX, IDC_EDIT_CUTOFF3, m_strCutoff3);
  DDX_Control(pDX, IDC_GROUP_FRAMES, m_butGroupFrames);
  DDX_Control(pDX, IDC_STAT_OTHER_CUTS, m_statOtherCuts);
  DDX_Control(pDX, IDC_EDIT_CUTOFF2, m_editCutoff2);
  DDX_Control(pDX, IDC_EDIT_CUTOFF3, m_editCutoff3);
  DDX_Control(pDX, IDC_STAT_FRAME_LABEL, m_statFrameLabel);
  DDX_Control(pDX, IDC_EDIT_TRUNCATION, m_editTruncation);
  DDX_Control(pDX, IDC_STAT_COUNT_LABEL, m_statCountLabel);
  DDX_Control(pDX, IDC_COMBOFILTER, m_comboFilter);
  DDX_Control(pDX, IDC_LIST_SET_NAMES, m_listSetNames);
  DDX_Control(pDX, IDC_BUT_DELETE_SET, m_butDeleteSet);
  DDX_Control(pDX, IDC_USE_GPU, m_butUseGPU);
  DDX_Control(pDX, IDC_BUT_SET_FOLDER, m_butSetFolder);
  DDX_Control(pDX, IDC_KEEP_PRECISION, m_butKeepPrecision);
  DDX_Check(pDX, IDC_KEEP_PRECISION, m_bKeepPrecision);
  DDX_Control(pDX, IDC_SAVE_FLOAT_SUMS, m_butSaveFloatSums);
  DDX_Check(pDX, IDC_SAVE_FLOAT_SUMS, m_bSaveFloatSums);
  DDX_Control(pDX, IDC_WHOLE_SERIES, m_butWholeSeries);
  DDX_Check(pDX, IDC_WHOLE_SERIES, m_bWholeSeries);
}


BEGIN_MESSAGE_MAP(CFrameAlignDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_RALIGN_IN_DM, OnAlignInDM)
  ON_BN_CLICKED(IDC_USE_FRAME_ALIGN, OnAlignInDM)
  ON_BN_CLICKED(IDC_RWITH_IMOD, OnAlignInDM)
  ON_BN_CLICKED(IDC_BUTMORE, OnButMoreParams)
  ON_BN_CLICKED(IDC_BUT_NEW_PARAMS, OnButNewParams)
  ON_BN_CLICKED(IDC_RPAIRWISE_NUM, OnPairwiseNum)
  ON_BN_CLICKED(IDC_RPAIRWISE_HALF, OnPairwiseNum)
  ON_BN_CLICKED(IDC_RPAIRWISE_ALL, OnPairwiseNum)
  ON_BN_CLICKED(IDC_RACCUM_REF, OnPairwiseNum)
  ON_EN_KILLFOCUS(IDC_EDIT_CUTOFF2, OnKillfocusEditCutoff2)
  ON_EN_KILLFOCUS(IDC_EDIT_CUTOFF3, OnKillfocusEditCutoff3)
  ON_BN_CLICKED(IDC_GROUP_FRAMES, OnGroupFrames)
  ON_BN_CLICKED(IDC_REFINE_ALIGNMENT, OnRefineAlignment)
  ON_BN_CLICKED(IDC_SMOOTH_SHIFTS, OnSmoothShifts)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_PAIRWISE_NUM, OnDeltaposSpinPairwiseNum)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALIGN_BIN, OnDeltaposSpinAlignBin)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_GROUP_SIZE, OnDeltaposSpinGroupSize)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_REFINE_ITER, OnDeltaposSpinRefineIter)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SMOOTH_CRIT, OnDeltaposSpinSmoothCrit)
  ON_EN_KILLFOCUS(IDC_EDIT_REFINE_CUTOFF, OnEnKillfocusEditRefineCutoff)
  ON_BN_CLICKED(IDC_TRUNCATE_ABOVE, OnTruncateAbove)
  ON_LBN_SELCHANGE(IDC_LIST_SET_NAMES, OnSelchangeListSetNames)
  ON_BN_CLICKED(IDC_BUT_DELETE_SET, OnButDeleteSet)
  ON_EN_CHANGE(IDC_EDIT_NAME, OnEnChangeEditName)
  ON_BN_CLICKED(IDC_BUT_SET_FOLDER, OnButSetFolder)
END_MESSAGE_MAP()


// CFrameAlignDlg message handlers
BOOL CFrameAlignDlg::OnInitDialog()
{
  CString filter;
  CString *filterNames = mWinApp->mCamera->GetK2FilterNames();
  int i, ind;
  CBaseDlg::OnInitDialog();
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);
  mParams = mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams *param = mParams->GetData();
  m_sbcPairwiseNum.SetRange(0, 500);
  m_sbcPairwiseNum.SetPos(250);
  m_sbcAlignBin.SetRange(0, 100);
  m_sbcAlignBin.SetPos(50);
  m_sbcGroupSize.SetRange(0, 100);
  m_sbcGroupSize.SetPos(50);
  m_sbcSmoothCrit.SetRange(0, 500);
  m_sbcSmoothCrit.SetPos(250);
  m_sbcRefineIter.SetRange(0, 100);
  m_sbcRefineIter.SetPos(50);

  // set up filter combo box for K2 camera
  mNumDMFilters = mWinApp->mCamera->GetNumK2Filters();
  for (i = 0; i < mNumDMFilters; i++) {
    filter = filterNames[i];
    ind = filter.Find(" (default)");
    if (ind > 0)
      filter.Truncate(ind);
    m_comboFilter.AddString((LPCTSTR)filter);
  }
  if (mNumDMFilters) {
    B3DCLAMP(mCurFiltInd, 0, mNumDMFilters - 1);
    m_comboFilter.SetCurSel(mCurFiltInd);
  }
  m_butDeleteSet.EnableWindow(mParams->GetSize() > 1);
  EnableDlgItem(IDC_RALIGN_IN_DM, mEnableWhere);
  EnableDlgItem(IDC_USE_FRAME_ALIGN, mEnableWhere);
  EnableDlgItem(IDC_RWITH_IMOD, mEnableWhere);

  // Load the set names and set the index
  for (ind = 0; ind < (int)mParams->GetSize(); ind++)
    m_listSetNames.AddString(param[ind].name);
  B3DCLAMP(mCurParamInd, 0, (int)mParams->GetSize() - 1);
  m_listSetNames.SetCurSel(mCurParamInd);

  // Load parameters so an updateData works and adjust the dialog
  m_bUseGPU = mUseGpuTransfer[m_iWhereAlign > 1 ? 1 : 0];
  LoadParamToDialog();
  ManagePanels();
  return TRUE;
}

void CFrameAlignDlg::OnOK() 
{
  SetFocus();
  UpdateData(true);
  UnloadCurrentPanel(m_iWhereAlign);
	CBaseDlg::OnOK();
}

void CFrameAlignDlg::OnCancel() 
{
  CBaseDlg::OnCancel();
}


void CFrameAlignDlg::OnAlignInDM()
{ 
  int whereBefore = m_iWhereAlign;
  UpdateData(true);
  if (m_iWhereAlign == whereBefore)
    return;
  UnloadCurrentPanel(whereBefore);
  if (m_iWhereAlign > 0) {
    m_bUseGPU = mUseGpuTransfer[m_iWhereAlign > 1 ? 1 : 0];
    LoadParamToDialog();
  }
  ManagePanels();
}

// Unload either the frame align params or the DM filter selection
void CFrameAlignDlg::UnloadCurrentPanel(int whereAlign)
{
  if (whereAlign > 0) {
    UnloadDialogToParam(mCurParamInd);
    mUseGpuTransfer[whereAlign > 1 ? 1 : 0] = m_bUseGPU;
  } else if (whereAlign == 0 && mNumDMFilters)
    mCurFiltInd = m_comboFilter.GetCurSel();
}

// Adjust panel layout
void CFrameAlignDlg::ManagePanels(void)
{
  BOOL states[5] = {true, true, true, true, true};
  states[1] = m_iWhereAlign == 0;
  states[2] = m_iWhereAlign > 0;
  states[3] = states[2] && mMoreParamsOpen;
  m_butSetFolder.EnableWindow(m_iWhereAlign == 2);
  m_butKeepPrecision.EnableWindow(m_iWhereAlign == 1 && mNewerK2API);
  m_butSaveFloatSums.EnableWindow(m_iWhereAlign == 2);
  m_butWholeSeries.EnableWindow(m_iWhereAlign == 2);
  m_butUseGPU.EnableWindow(mGPUavailable || m_iWhereAlign > 1);
  SetDlgItemText(IDC_BUTMORE, mMoreParamsOpen ? "-" : "+");
  AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 0);
}

// Show more or less parameters
void CFrameAlignDlg::OnButMoreParams()
{
  mMoreParamsOpen = !mMoreParamsOpen;
  ManagePanels();
}

// A new set is selected: unload old and load new
void CFrameAlignDlg::OnSelchangeListSetNames()
{
  int newInd = m_listSetNames.GetCurSel();
  if (newInd == LB_ERR || newInd < 0 || newInd >= m_listSetNames.GetCount()) {
    m_listSetNames.SetCurSel(mCurParamInd);
    return;
  }
  UpdateData(true);
  UnloadDialogToParam(mCurParamInd);
  mCurParamInd = newInd;
  LoadParamToDialog();
}

// Add a new parameter set: copy the existing one
void CFrameAlignDlg::OnButNewParams()
{
  FrameAliParams param;
  UpdateData(true);
  UnloadDialogToParam(mCurParamInd);
  param = mParams->GetAt(mCurParamInd);
  param.name = "New set";
  mParams->Add(param);
  mCurParamInd = (int)mParams->GetSize() - 1;
  LoadParamToDialog();
  m_listSetNames.AddString(param.name);
  m_listSetNames.SetCurSel(mCurParamInd);
  m_butDeleteSet.EnableWindow(mParams->GetSize() > 1);
}

// Delete the current parameter set after confirmation.  The last set cannot be deleted
void CFrameAlignDlg::OnButDeleteSet()
{
  int numActive = mWinApp->GetNumActiveCameras();
  int *activeList = mWinApp->GetActiveCameraList();
  int act, cam, set, curActive = mWinApp->GetCurrentActiveCamera();
  ControlSet *camConSets = mWinApp->GetCamConSets();
  ControlSet *conSets = mWinApp->GetConSets();
  ControlSet *consp;
  bool anyChanged = false;
  int newSel = B3DMAX(0, mCurParamInd - 1);
  if (AfxMessageBox("Are you sure you want to delete the current parameter set?",
    MB_QUESTION) == IDNO)
    return;
  mParams->RemoveAt(mCurParamInd);
  m_listSetNames.DeleteString(mCurParamInd);

  // Now have to change any existing parameters
  for (act = -1; act < numActive; act++) {
    if (act >= 0)
      cam = activeList[act];
    for (set = 0; set < NUMBER_OF_USER_CONSETS; set++) {
      if (act < 0)
        consp = &conSets[set];
      else
        consp = &camConSets[set + cam * MAX_CONSETS];
      if (consp->faParamSetInd >= mCurParamInd) {
        if (consp->faParamSetInd == mCurParamInd && consp->alignFrames && 
          consp->useFrameAlign && 
          !(set == mConSetSelected && ((act < 0 && mCameraSelected == curActive) || 
          (act >= 0 && mCameraSelected == act))))
          anyChanged = true;
        consp->faParamSetInd--;
      }
    }
  }
  mCurParamInd = newSel;
  m_listSetNames.SetCurSel(mCurParamInd);
  LoadParamToDialog();
  m_butDeleteSet.EnableWindow(mParams->GetSize() > 1);
  if (anyChanged)
    AfxMessageBox("Some other camera parameter sets were using this alignment set\n"
    "and have been shifted to the previous alignment set", MB_ICONINFORMATION | MB_OK);
}

// Select folder for saving com files
void CFrameAlignDlg::OnButSetFolder()
{
  CString str = mWinApp->mCamera->GetAlignFramesComPath();
  CString title = "SELECT folder for Alignframes com files (typing name may not work)";
  if (mServerIsRemote) {
    if (KGetOneString("Enter folder on Gatan server for saving Alignframes com files:", 
      str, 250, mWinApp->mCamera->GetNoK2SaveFolderBrowse() ? "" : 
      "SELECT (do not type in) folder accessible on Gatan server for saving Alignframes "
      "com files"))
      mWinApp->mCamera->SetAlignFramesComPath(str);
     m_butSetFolder.SetButtonStyle(BS_PUSHBUTTON);
    return;
  }
  if (str.IsEmpty())
    str = mWinApp->mCamera->GetDirForK2Frames();

  CXFolderDialog dlg(str);
  dlg.SetTitle(title);
  if (dlg.DoModal() == IDOK)
		mWinApp->mCamera->SetAlignFramesComPath(dlg.GetPath());
  m_butSetFolder.SetButtonStyle(BS_PUSHBUTTON);
}

void CFrameAlignDlg::OnTruncateAbove()
{
  UpdateData(true);
  ManageTruncation();
}

void CFrameAlignDlg::OnPairwiseNum()
{
  UpdateData(true);
  ManageAlignStategy();
}


void CFrameAlignDlg::OnEnChangeEditName()
{
  UpdateData(true);
  m_listSetNames.DeleteString(mCurParamInd);
  m_listSetNames.InsertString(mCurParamInd, m_strCurName);
}

void CFrameAlignDlg::OnKillfocusEditCutoff2()
{
  UpdateData(true);
  if (ProcessBlankIsZeroEditBox(m_strCutoff2, mCutoff2))
    UpdateData(false);
}

void CFrameAlignDlg::OnKillfocusEditCutoff3()
{
  UpdateData(true);
  if (ProcessBlankIsZeroEditBox(m_strCutoff3, mCutoff3))
    UpdateData(false);
}

void CFrameAlignDlg::OnEnKillfocusEditRefineCutoff()
{
  UpdateData(true);
  if (ProcessBlankIsZeroEditBox(m_strRefineCutoff, mRefineCutoff))
    UpdateData(false);
}

void CFrameAlignDlg::OnGroupFrames()
{
  UpdateData(true);
  ManageGrouping();
}


void CFrameAlignDlg::OnRefineAlignment()
{
  UpdateData(true);
  ManageRefinement();
}


void CFrameAlignDlg::OnSmoothShifts()
{
  UpdateData(true);
  ManageSmoothing();
}


void CFrameAlignDlg::OnDeltaposSpinPairwiseNum(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mPairwiseNum, 7, 99, mPairwiseNum))
    return;
  UpdateData(true);
  m_strPairwiseNum.Format("%d", mPairwiseNum);
  UpdateData(false);
  *pResult = 0;
}

void CFrameAlignDlg::OnDeltaposSpinAlignBin(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mAlignBin, 2, 10, mAlignBin))
    return;
  UpdateData(true);
  m_strAlignBin.Format("Alignment binning %d", mAlignBin);
  UpdateData(false);
  *pResult = 0;
}

void CFrameAlignDlg::OnDeltaposSpinGroupSize(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mGroupSize, 2, 5, mGroupSize))
    return;
  UpdateData(true);
  ManageGroupSizeMinFrames();
  UpdateData(false);
  *pResult = 0;
}

void CFrameAlignDlg::OnDeltaposSpinRefineIter(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mRefineIter, 1, 20, mRefineIter))
    return;
  UpdateData(true);
  m_strRefineIter.Format("%d", mRefineIter);
  UpdateData(false);
  *pResult = 0;
}


void CFrameAlignDlg::OnDeltaposSpinSmoothCrit(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mSmoothCrit, 10, 30, mSmoothCrit))
    return;
  UpdateData(true);
  m_strSmoothCrit.Format("%d", mSmoothCrit);
  UpdateData(false);
  *pResult = 0;
}

void CFrameAlignDlg::ManageGroupSizeMinFrames(void)
{
  int numFrames, numGroups;
  m_strGroupSize.Format("%d", mGroupSize);
  for (numFrames = 3; numFrames < mGroupSize + 20; numFrames++) {
    numGroups = numFrames + 1 - mGroupSize;
    if ((numGroups + 1 - mGroupSize) * (numGroups - mGroupSize) / 2 >= numGroups)
      break;
  }
  m_strGroupNeeds.Format("(Requires at least %d frames)", numFrames);
}


void CFrameAlignDlg::ManageAlignStategy(void)
{
  m_statPairwiseNum.EnableWindow(m_iAlignStrategy == FRAMEALI_PAIRWISE_NUM);
  m_sbcPairwiseNum.EnableWindow(m_iAlignStrategy == FRAMEALI_PAIRWISE_NUM);
  m_statFrameLabel.EnableWindow(m_iAlignStrategy == FRAMEALI_PAIRWISE_NUM);
  m_editCutoff2.EnableWindow(m_iAlignStrategy != FRAMEALI_ACCUM_REF);
  m_editCutoff3.EnableWindow(m_iAlignStrategy != FRAMEALI_ACCUM_REF);
  m_statOtherCuts.EnableWindow(m_iAlignStrategy != FRAMEALI_ACCUM_REF);
  m_butHybridShifts.EnableWindow(m_iAlignStrategy != FRAMEALI_ACCUM_REF);
  ManageGrouping();
}


void CFrameAlignDlg::ManageRefinement(void)
{
  m_statRefineIter.EnableWindow(m_bRefineAlignment);
  m_sbcRefineIter.EnableWindow(m_bRefineAlignment);
  m_statIterLabel.EnableWindow(m_bRefineAlignment);
  m_statRefineCutoff.EnableWindow(m_bRefineAlignment);
  m_statStopIter.EnableWindow(m_bRefineAlignment);
  m_statStopLabel.EnableWindow(m_bRefineAlignment);
  m_editStopIter.EnableWindow(m_bRefineAlignment);
  m_butRefineGroups.EnableWindow(m_bGroupFrames && m_bRefineAlignment && 
    m_iAlignStrategy != FRAMEALI_ACCUM_REF);
}


void CFrameAlignDlg::ManageGrouping(void)
{
  bool enable = m_bGroupFrames && m_iAlignStrategy != FRAMEALI_ACCUM_REF;
  m_butGroupFrames.EnableWindow(m_iAlignStrategy != FRAMEALI_ACCUM_REF);
  m_statGroupSize.EnableWindow(enable);
  m_sbcGroupSize.EnableWindow(enable);
  m_statGroupNeeds.EnableWindow(enable);
  m_butRefineGroups.EnableWindow(enable && m_bRefineAlignment);
}

void CFrameAlignDlg::ManageSmoothing(void)
{
  m_statSmoothCrit.EnableWindow(m_bSmoothShifts);
  m_statSmoothLabel.EnableWindow(m_bSmoothShifts);
  m_sbcSmoothCrit.EnableWindow(m_bSmoothShifts);
}

void CFrameAlignDlg::ManageTruncation(void)
{
  m_editTruncation.EnableWindow(m_bTruncateAbove);
  m_statCountLabel.EnableWindow(m_bTruncateAbove);
}

bool CFrameAlignDlg::ProcessBlankIsZeroEditBox(CString & editStr, float &cutoff)
{
  float oldCut = cutoff;
  if (editStr.IsEmpty()) {
    cutoff = 0.;
    return false;
  }
  cutoff = (float)atof((LPCTSTR)editStr);
  SetBlankIsZeroEditBox(editStr, cutoff);
  return cutoff != oldCut || !cutoff;
}

void CFrameAlignDlg::SetBlankIsZeroEditBox(CString & editStr, float &cutoff)
{
  B3DCLAMP(cutoff, 0.f, 0.5f);
  if (!cutoff)
    editStr = "";
  else
    editStr.Format("%.3f", cutoff);
}

// Move data from a param into dialog
void CFrameAlignDlg::LoadParamToDialog(void)
{
  if (mCurParamInd < 0)
    return;
  FrameAliParams *param = mParams->GetData() + mCurParamInd;
  m_iAlignStrategy = param->strategy;
  mAlignBin = param->aliBinning;
  m_strAlignBin.Format("Alignment binning %d", mAlignBin);
  mPairwiseNum = param->numAllVsAll;
  m_strPairwiseNum.Format("%d", mPairwiseNum);
  m_fCutoff = param->rad2Filt1;
  mCutoff2 = param->rad2Filt2;
  mCutoff3 = param->rad2Filt3;
  SetBlankIsZeroEditBox(m_strCutoff2, mCutoff2);
  SetBlankIsZeroEditBox(m_strCutoff3, mCutoff3);
  mRefineCutoff = param->refRadius2;
  SetBlankIsZeroEditBox(m_strRefineCutoff, mRefineCutoff);
  m_bHybridShifts = param->hybridShifts;
  m_fSigmaRatio = 0.0001f * (float)B3DNINT(10000. * param->sigmaRatio);
  m_bRefineAlignment = param->doRefine;
  mRefineIter = param->refineIter;
  m_strRefineIter.Format("%d", mRefineIter);
  m_bGroupFrames = param->useGroups;
  mGroupSize = param->groupSize;
  m_strGroupSize.Format("%d", param->groupSize);
  ManageGroupSizeMinFrames();
  m_bSmoothShifts = param->doSmooth;
  mSmoothCrit = param->smoothThresh;
  m_strSmoothCrit.Format("%d", mSmoothCrit);
  m_bTruncateAbove = param->truncate;
  m_fTruncation = param->truncLimit;
  m_bRefineGroups = param->groupRefine;
  m_fStopIter = param->stopIterBelow;
  m_iMaxShift = param->shiftLimit;
  m_bKeepPrecision = param->keepPrecision;
  m_bSaveFloatSums = param->outputFloatSums;
  m_strCurName = param->name;
  ManageTruncation();
  ManageSmoothing();
  ManageGrouping();
  ManageAlignStategy();
  ManageRefinement();
  UpdateData(false);
  ManagePanels();
}


void CFrameAlignDlg::UnloadDialogToParam(int index)
{
  if (index < 0)
    return;
  FrameAliParams *param = mParams->GetData() + index;
  param->strategy = m_iAlignStrategy;
  param->aliBinning = mAlignBin;
  param->numAllVsAll = mPairwiseNum;
  param->rad2Filt1 = m_fCutoff;
  param->rad2Filt2 = mCutoff2;
  param->rad2Filt3 = mCutoff3;
  param->refRadius2 = mRefineCutoff;
  param->hybridShifts = m_bHybridShifts;
  param->sigmaRatio = m_fSigmaRatio;
  param->doRefine = m_bRefineAlignment;
  param->refineIter = mRefineIter;
  param->useGroups = m_bGroupFrames;
  param->doSmooth = m_bSmoothShifts;
  param->groupSize = mGroupSize;
  param->smoothThresh = mSmoothCrit;
  param->truncate = m_bTruncateAbove;
  param->truncLimit = m_fTruncation;
  param->groupRefine = m_bRefineGroups;
  param->stopIterBelow = m_fStopIter;
  param->shiftLimit = m_iMaxShift;
  param->keepPrecision = m_bKeepPrecision;
  param->outputFloatSums = m_bSaveFloatSums;
  param->name = m_strCurName;
}
