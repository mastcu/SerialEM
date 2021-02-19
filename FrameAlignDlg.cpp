// FrameAlignDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "FrameAlignDlg.h"
#include "CameraController.h"
#include "Shared\SEMCCDDefines.h"
#include "Utilities\KGetOne.h"
#include "XFolderDialog\XFolderDialog.h"

#define MIN_TARGET_SIZE 250
#define MAX_TARGET_SIZE 4000

static int idTable[] = {IDC_STAT_WHERE_ALIGN, IDC_USE_FRAME_ALIGN, IDC_RALIGN_IN_DM, 
  IDC_RWITH_IMOD, PANEL_END,
  IDC_STAT_FILTER, IDC_COMBOFILTER, PANEL_END,
  IDC_USE_GPU, IDC_BUT_NEW_PARAMS, IDC_LIST_SET_NAMES, IDC_STAT_PARAM_NAME,
  IDC_USE_FRAME_FOLDER, IDC_BUT_SET_FOLDER, IDC_BUT_DELETE_SET,
  IDC_WHOLE_SERIES, IDC_EDIT_NAME, PANEL_END,
  IDC_STAT_USE_ONLY_GROUP, IDC_ONLY_IN_PLUGIN,
  IDC_ONLY_WITH_IMOD, IDC_ONLY_NON_SUPER, IDC_ONLY_SUPERRESOLUTION, PANEL_END,
  IDC_STAT_LINE1, PANEL_END,
  IDC_TRUNCATE_ABOVE, IDC_STAT_COUNT_LABEL, IDC_EDIT_TRUNCATION, PANEL_END,
  IDC_STAT_ALIGN_METHOD, IDC_STAT_CUTOFF_LABEL, IDC_EDIT_CUTOFF,
  IDC_RPAIRWISE_NUM, IDC_RPAIRWISE_HALF, IDC_RPAIRWISE_ALL, IDC_RACCUM_REF,
  IDC_SPIN_PAIRWISE_NUM, IDC_STAT_PAIRWISE_NUM, IDC_STAT_FRAME_LABEL,
  IDC_STAT_ALIGN_BIN, IDC_SPIN_ALIGN_BIN, IDC_REFINE_ALIGNMENT, 
  IDC_SPIN_REFINE_ITER, IDC_STAT_REFINE_ITER, IDC_STAT_ITER_LABEL, IDC_ALIGN_SUBSET,
  IDC_GROUP_FRAMES, IDC_STAT_GROUP_SIZE, IDC_SPIN_GROUP_SIZE, IDC_STAT_SUBSET_TO,
  IDC_STAT_GROUP_NEEDS, IDC_SMOOTH_SHIFTS, IDC_STAT_SMOOTH_CRIT, IDC_STAT_SUBSET_PAREN,
  IDC_STAT_MAX_SHIFT, IDC_EDIT_MAX_SHIFT, IDC_STAT_PIXEL_LABEL, IDC_FA_EDIT_SUB_START,
  IDC_SPIN_SMOOTH_CRIT, IDC_STAT_SMOOTH_LABEL, IDC_FA_EDIT_SUB_END, IDC_BUTMORE,
  IDC_STAT_MORE_PARAMS, IDC_STAT_EER_SUPER_LABEL, IDC_RSUPER_NONE, IDC_RSUPER_2X,
  IDC_SPIN_BIN_TO, IDC_STAT_ALIBIN_PIX, IDC_STAT_ALIBIN, IDC_RALIBIN_BY_FAC, 
  IDC_RSUPER_4X, IDC_RBIN_TO_SIZE, IDC_STAT_ALIGN_TARGET, PANEL_END,
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
  , m_fCutoff(0.05f)
  , m_iMaxShift(2)
  , m_bGroupFrames(0)
  , m_strGroupSize(_T(""))
  , m_strGroupNeeds(_T(""))
  , m_bRefineAlignment(FALSE)
  , m_strRefineIter(_T(""))
  , m_bSmoothShifts(FALSE)
  , m_strSmoothCrit(_T(""))
  , m_bRefineGroups(FALSE)
  , m_strRefineCutoff(_T(""))
  , m_fStopIter(0.01f)
  , m_bHybridShifts(FALSE)
  , m_fSigmaRatio(0.01f)
  , m_strCutoff2(_T(""))
  , m_strCutoff3(_T(""))
  , m_bKeepPrecision(FALSE)
  , m_bSaveFloatSums(FALSE)
  , m_bWholeSeries(FALSE)
  , m_bAlignSubset(FALSE)
  , m_iSubsetStart(0)
  , m_iSubsetEnd(0)
  , m_bOnlyInPlugin(FALSE)
  , m_bOnlyWithIMOD(FALSE)
  , m_bOnlyNonSuperRes(FALSE)
  , m_bOnlySuperRes(FALSE)
  , m_bUseFrameFolder(FALSE)
  , m_strAlignTarget(_T(""))
  , m_iBinByOrTo(0)
  , m_iEERsuperRes(0)
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
  DDX_Control(pDX, IDC_ALIGN_SUBSET, m_butAlignSubset);
  DDX_Check(pDX, IDC_ALIGN_SUBSET, m_bAlignSubset);
  DDX_Control(pDX, IDC_STAT_SUBSET_TO, m_statSubsetTo);
  DDX_Control(pDX, IDC_STAT_SUBSET_PAREN, m_statSubsetParen);
  DDX_Control(pDX, IDC_FA_EDIT_SUB_START, m_editSubsetStart);
  DDX_Control(pDX, IDC_FA_EDIT_SUB_END, m_editSubsetEnd);
  DDX_Text(pDX, IDC_FA_EDIT_SUB_START, m_iSubsetStart);
  DDX_Text(pDX, IDC_FA_EDIT_SUB_END, m_iSubsetEnd);
  DDX_Check(pDX, IDC_ONLY_IN_PLUGIN, m_bOnlyInPlugin);
  DDX_Check(pDX, IDC_ONLY_WITH_IMOD, m_bOnlyWithIMOD);
  DDX_Check(pDX, IDC_ONLY_NON_SUPER, m_bOnlyNonSuperRes);
  DDX_Check(pDX, IDC_ONLY_SUPERRESOLUTION, m_bOnlySuperRes);
  DDX_Control(pDX, IDC_USE_FRAME_FOLDER, m_butUseFrameFolder);
  DDX_Check(pDX, IDC_USE_FRAME_FOLDER, m_bUseFrameFolder);
  DDX_Control(pDX, IDC_TRUNCATE_ABOVE, m_butTruncateAbove);
  DDX_Control(pDX, IDC_STAT_ALIGN_TARGET, m_statAlignTarget);
  DDX_Text(pDX, IDC_STAT_ALIGN_TARGET, m_strAlignTarget);
  DDX_Control(pDX, IDC_STAT_ALIBIN_PIX, m_statAlibinPix);
  DDX_Control(pDX, IDC_SPIN_BIN_TO, m_sbcBinTo);
  DDX_Radio(pDX, IDC_RALIBIN_BY_FAC, m_iBinByOrTo);
  DDX_Control(pDX, IDC_STAT_ALIGN_BIN, m_statAlignBin);
  DDX_Radio(pDX, IDC_RSUPER_NONE, m_iEERsuperRes);
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
  ON_BN_CLICKED(IDC_ALIGN_SUBSET, OnAlignSubset)
  ON_BN_CLICKED(IDC_ONLY_IN_PLUGIN, OnOnlyInPlugin)
  ON_BN_CLICKED(IDC_ONLY_WITH_IMOD, OnOnlyWithImod)
  ON_BN_CLICKED(IDC_ONLY_NON_SUPER, OnOnlyNonSuper)
  ON_BN_CLICKED(IDC_ONLY_SUPERRESOLUTION, OnOnlySuperresolution)
  ON_EN_KILLFOCUS(IDC_FA_EDIT_SUB_START, OnKillfocusEditSubsetStart)
  ON_EN_KILLFOCUS(IDC_FA_EDIT_SUB_END, OnKillfocusEditSubsetEnd)
  ON_BN_CLICKED(IDC_USE_FRAME_FOLDER, OnUseFrameFolder)
  ON_BN_CLICKED(IDC_RALIBIN_BY_FAC, OnAliBinByFac)
  ON_BN_CLICKED(IDC_RBIN_TO_SIZE, OnAliBinByFac)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_BIN_TO, OnDeltaposSpinBinTo)
  ON_BN_CLICKED(IDC_RSUPER_NONE, OnEERsuperRes)
  ON_BN_CLICKED(IDC_RSUPER_2X, OnEERsuperRes)
  ON_BN_CLICKED(IDC_RSUPER_4X, OnEERsuperRes)
END_MESSAGE_MAP()


// CFrameAlignDlg message handlers
BOOL CFrameAlignDlg::OnInitDialog()
{
  CString filter;
  CString *filterNames = mWinApp->mCamera->GetK2FilterNames();
  int i, ind;
  bool constrainByRes;
  CBaseDlg::OnInitDialog();
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);
  mParams = mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams *param = mParams->GetData();
  mCamParams = mWinApp->GetCamParams() + mCameraSelected;
  m_sbcPairwiseNum.SetRange(0, 500);
  m_sbcPairwiseNum.SetPos(250);
  m_sbcAlignBin.SetRange(0, 100);
  m_sbcAlignBin.SetPos(50);
  m_sbcBinTo.SetRange(0, 500);
  m_sbcBinTo.SetPos(250);
  m_sbcGroupSize.SetRange(0, 100);
  m_sbcGroupSize.SetPos(50);
  m_sbcSmoothCrit.SetRange(0, 500);
  m_sbcSmoothCrit.SetPos(250);
  m_sbcRefineIter.SetRange(0, 100);
  m_sbcRefineIter.SetPos(50);
  mFalconCanAlign = mCamParams->FEItype && FCAM_CAN_ALIGN(mCamParams);
  mDEcanAlign = mCamParams->DE_camType && (mCamParams->CamFlags & DE_CAM_CAN_ALIGN);
  mShowWhereToAlign = true;
  constrainByRes = mCamParams->K2Type || (mDEcanAlign &&
    (mCamParams->CamFlags & DE_CAM_CAN_COUNT));

  // set up filter combo box for K2 camera
  mNumDMFilters = 0;
  if (mCamParams->K2Type) {
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
  }
  m_butDeleteSet.EnableWindow(mParams->GetSize() > 1);
  EnableDlgItem(IDC_RALIGN_IN_DM, mEnableWhere && 
    (mCamParams->K2Type || mFalconCanAlign || mDEcanAlign));
  EnableDlgItem(IDC_USE_FRAME_ALIGN, mEnableWhere);
  EnableDlgItem(IDC_RWITH_IMOD, mEnableWhere);

  // Rename buttons for Falcon, disable some items
  if (!mCamParams->K2Type) {
    if (mFalconCanAlign)
      SetDlgItemText(IDC_RALIGN_IN_DM, "In Falcon processor");
    else if (mDEcanAlign)
      SetDlgItemText(IDC_RALIGN_IN_DM, "In DE server");
    else if (!m_iWhereAlign)
      m_iWhereAlign = 1;
    if (!mCamParams->GatanCam) {
      SetDlgItemText(IDC_USE_FRAME_ALIGN, "In SerialEM");
      SetDlgItemText(IDC_ONLY_IN_PLUGIN, "Aligning in SerialEM");
    }

    // Advanced Falcon 2 on remote computer with no local frame path cannot deal with
    // paths in a com file or send it elsewhere, so get rid of top section
    if (mCamParams->FEItype == FALCON2_TYPE && FCAM_ADVANCED(mCamParams) &&
      mServerIsRemote && mWinApp->mCamera->GetLocalFalconFramePath().IsEmpty()) {
      m_iWhereAlign = 1;
      mShowWhereToAlign = false;
    }
  }
  mShowRestrictions = (mShowWhereToAlign || constrainByRes);
  if (mCamParams->FEItype != FALCON4_TYPE) {
    mIDsToDrop.push_back(IDC_STAT_EER_SUPER_LABEL);
    mIDsToDrop.push_back(IDC_RSUPER_NONE);
    mIDsToDrop.push_back(IDC_RSUPER_2X);
    mIDsToDrop.push_back(IDC_RSUPER_4X);
    SetDlgItemText(IDC_RALIBIN_BY_FAC, "By");
  } else
    SetDlgItemText(IDC_RALIBIN_BY_FAC, "To");

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
  int stayIn, newIndex, oldInd = mCurParamInd;
  CString mess, mess2;
  SetFocus();
  UpdateData(true);
  UnloadCurrentPanel(m_iWhereAlign);
  stayIn = CheckConditionsOnClose(m_iWhereAlign, mCurParamInd, newIndex);

  // Set new index regardless, and if staying in dialog, load the parameters
  mCurParamInd = newIndex;
  if (stayIn) {
    if (oldInd != newIndex) {
      m_listSetNames.SetCurSel(newIndex);
      LoadParamToDialog();
    }
    return;
  }
	CBaseDlg::OnOK();
}

void CFrameAlignDlg::OnCancel() 
{
  int newIndex;
  if (CheckConditionsOnClose(-1, -1, newIndex))
    return;
  CBaseDlg::OnCancel();
}

// Common function to check whether there is a mismatch between current set and the
// restrictions and ask the user what to do
int CFrameAlignDlg::CheckConditionsOnClose(int whereAlign, int curIndex, int &newIndex)
{
  int notOK;
  CString mess, mess2;
  int *activeList = mWinApp->GetActiveCameraList();
  ControlSet *camConSets = mWinApp->GetCamConSets();
  ControlSet *conSet = &camConSets[mConSetSelected + activeList[mCameraSelected] *
    MAX_CONSETS];
  if (whereAlign < 0)
    whereAlign = conSet->useFrameAlign;
  if (curIndex < 0)
    curIndex = conSet->faParamSetInd;
  notOK = UtilFindValidFrameAliParams(mCamParams, mReadMode, mTakingK3Binned && 
    (mWinApp->mCamera->CAN_PLUGIN_DO(CAN_BIN_K3_REF, 
      mWinApp->GetCamParams() + mCameraSelected) ||
      !(whereAlign > 1 && mWinApp->mCamera->GetSaveUnnormalizedFrames())),
    whereAlign, curIndex, newIndex, &mess);
  if (notOK || newIndex != curIndex) {
    mess += "\n\nPress:\n";
    if (notOK > 0)
      mess += "\"Use Set Anyway\" to use these parameters anyway";
    else if (notOK < 0) {
      mess2.Format("\"Use Other Set\" to use the first one (# %d)",
        newIndex + 1);
      mess += mess2;
    } else
      mess += "\"Use Other Set\" to use this other parameter set";
    mess += "\n\n\"Stay in Dialog\" to adjust parameters or restrictions in this dialog";
    if (SEMThreeChoiceBox(mess, notOK > 0 ? "Use Set Anyway" : "Use Other Set", 
      "Stay in Dialog", "", MB_YESNO | MB_ICONQUESTION) == IDNO)
      return 1;
  }
  if (whereAlign < 0)
    conSet->faParamSetInd = newIndex;
  return 0;
}

void CFrameAlignDlg::OnAlignInDM()
{ 
  int whereBefore = m_iWhereAlign;
  int notOK, newIndex;
  UpdateData(true);
  if (m_iWhereAlign == whereBefore)
    return;
  UnloadCurrentPanel(whereBefore);
  if (m_iWhereAlign > 0) {
    notOK = UtilFindValidFrameAliParams(mCamParams, mReadMode, mTakingK3Binned  && 
      !(m_iWhereAlign > 1 && mWinApp->mCamera->GetSaveUnnormalizedFrames()), 
      m_iWhereAlign, mCurParamInd, newIndex, NULL);
    if (!notOK && mCurParamInd != newIndex) {
      mCurParamInd = newIndex;
      m_listSetNames.SetCurSel(newIndex);
    }
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
  BOOL states[9] = {true, true, true, true, true, true, true, true, true};
  bool constrainByRes;
  states[0] = mShowWhereToAlign;
  states[1] = m_iWhereAlign == 0 && mCamParams->K2Type;
  states[2] = states[6] = m_iWhereAlign > 0;
  states[3] = mShowRestrictions && m_iWhereAlign > 0;
  states[5] = (mCamParams->K2Type || mCamParams->FEItype == FALCON4_TYPE) &&
    m_iWhereAlign > 0;
  states[7] = states[2] && mMoreParamsOpen;
  m_butSetFolder.EnableWindow(m_iWhereAlign == 2 && !m_bUseFrameFolder);
  m_butUseFrameFolder.EnableWindow(m_iWhereAlign == 2);
  m_butKeepPrecision.EnableWindow(m_iWhereAlign == 1 && mNewerK2API && 
    mCamParams->K2Type == K2_SUMMIT);
  m_butSaveFloatSums.EnableWindow(m_iWhereAlign == 2);
  m_butWholeSeries.EnableWindow(m_iWhereAlign == 2);
  m_butUseGPU.EnableWindow(mGPUavailable || m_iWhereAlign > 1);
  SetDlgItemText(IDC_BUTMORE, mMoreParamsOpen ? "-" : "+");
  AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 0);
  if (mShowRestrictions && m_iWhereAlign > 0) {
    constrainByRes = mCamParams->K2Type || (mDEcanAlign &&
      (mCamParams->CamFlags & DE_CAM_CAN_COUNT));
    ShowDlgItem(IDC_ONLY_NON_SUPER, constrainByRes);
    ShowDlgItem(IDC_ONLY_SUPERRESOLUTION, constrainByRes);
  }
  if (!mCamParams->K2Type && !mFalconCanAlign && !mDEcanAlign)
    ShowDlgItem(IDC_RALIGN_IN_DM, false);
  if (!mShowWhereToAlign) {
    ShowDlgItem(IDC_WHOLE_SERIES, false);
    ShowDlgItem(IDC_USE_FRAME_FOLDER, false);
    ShowDlgItem(IDC_BUT_SET_FOLDER, false);
  }
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


void CFrameAlignDlg::OnUseFrameFolder()
{
  UpdateData(true);
  m_butSetFolder.EnableWindow(m_iWhereAlign == 2 && !m_bUseFrameFolder);
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

void CFrameAlignDlg::OnEERsuperRes()
{
  UpdateData(true);
}

void CFrameAlignDlg::OnAliBinByFac()
{
  UpdateData(true);
  ManageAlignBinning();
}

void CFrameAlignDlg::OnDeltaposSpinBinTo(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  int mult = 50;
  if (NewSpinnerValue(pNMHDR, pResult, mAlignTargetSize / mult, MIN_TARGET_SIZE / mult,
    MAX_TARGET_SIZE / mult, newVal))
    return;
  UpdateData(true);
  mAlignTargetSize = mult * newVal;
  m_strAlignTarget.Format("%d", mAlignTargetSize);
  UpdateData(false);
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
  FormattedSpinnerValue(pNMHDR, pResult, 7, 99, mPairwiseNum, m_strPairwiseNum, "%d");
}

void CFrameAlignDlg::OnDeltaposSpinAlignBin(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 2, 16, mAlignBin, m_strAlignBin, 
    "%d");
}

void CFrameAlignDlg::OnDeltaposSpinGroupSize(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mGroupSize, 2, 5, mGroupSize))
    return;
  UpdateData(true);
  ManageGroupSizeMinFrames();
  UpdateData(false);
}

void CFrameAlignDlg::OnDeltaposSpinRefineIter(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 1, 20, mRefineIter, m_strRefineIter, "%d");
}


void CFrameAlignDlg::OnDeltaposSpinSmoothCrit(NMHDR *pNMHDR, LRESULT *pResult)
{
  FormattedSpinnerValue(pNMHDR, pResult, 10, 30, mSmoothCrit, m_strSmoothCrit, "%d");
}

void CFrameAlignDlg::OnAlignSubset()
{
  UpdateData(true);
  m_editSubsetStart.EnableWindow(m_bAlignSubset);
  m_editSubsetEnd.EnableWindow(m_bAlignSubset);
}

void CFrameAlignDlg::OnKillfocusEditSubsetStart()
{
  CString mess = "The starting and ending frames must be positive\n"
      "and at least 2 frames must be aligned";
  int oldStart = m_iSubsetStart, oldEnd = m_iSubsetEnd;
  UpdateData(true);
  if (m_iSubsetStart < 1 || m_iSubsetStart > K2FA_SUB_START_MASK || 
    m_iSubsetStart > m_iSubsetEnd - 1) {
      if (m_iSubsetStart > K2FA_SUB_START_MASK)
        mess.Format("The starting frame to align must be no more than %d", 
          K2FA_SUB_START_MASK);
      m_iSubsetStart = oldStart;
      m_iSubsetEnd = oldEnd;
      UpdateData(false);
      AfxMessageBox(mess, MB_EXCLAME);
  }
}


void CFrameAlignDlg::OnKillfocusEditSubsetEnd()
{
  OnKillfocusEditSubsetStart();
}

void CFrameAlignDlg::OnOnlyInPlugin()
{
  UpdateData(true);
  if (m_bOnlyWithIMOD) {
    m_bOnlyWithIMOD = false;
    UpdateData(false);
  }
}

void CFrameAlignDlg::OnOnlyWithImod()
{
  UpdateData(true);
  if (m_bOnlyInPlugin) {
    m_bOnlyInPlugin = false;
    UpdateData(false);
  }
}

void CFrameAlignDlg::OnOnlyNonSuper()
{
  UpdateData(true);
  if (m_bOnlySuperRes) {
    m_bOnlySuperRes = false;
    UpdateData(false);
  }
}

void CFrameAlignDlg::OnOnlySuperresolution()
{
  UpdateData(true);
  if (m_bOnlyNonSuperRes) {
    m_bOnlyNonSuperRes = false;
    UpdateData(false);
  }
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

void CFrameAlignDlg::ManageAlignBinning(void)
{
  m_statAlignBin.EnableWindow(!m_iBinByOrTo);
  m_sbcAlignBin.EnableWindow(!m_iBinByOrTo);
  m_statAlibinPix.EnableWindow(m_iBinByOrTo > 0);
  m_statAlignTarget.EnableWindow(m_iBinByOrTo > 0);
  m_sbcBinTo.EnableWindow(m_iBinByOrTo > 0);
  if (m_iBinByOrTo > 0 && (!mAlignTargetSize || 
    (mAlignTargetSize && mAlignTargetSize < MIN_TARGET_SIZE))) { 
      if (!mAlignTargetSize)
        mAlignTargetSize = 1000;
      B3DCLAMP(mAlignTargetSize, MIN_TARGET_SIZE, MAX_TARGET_SIZE);
      m_strAlignTarget.Format("%d", mAlignTargetSize);
      UpdateData(false);
  }
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
  bool enable = mCamParams->K2Type || mSavingInEERmode;
  m_butTruncateAbove.EnableWindow(enable);
  enable = enable && m_bTruncateAbove;
  m_editTruncation.EnableWindow(enable);
  m_statCountLabel.EnableWindow(enable);
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
  bool canSubset = mWinApp->mCamera->CAN_PLUGIN_DO(CAN_ALIGN_SUBSET, mCamParams) ||
    !mCamParams->K2Type;
  if (mCurParamInd < 0)
    return;
  FrameAliParams *param = mParams->GetData() + mCurParamInd;
  m_iAlignStrategy = param->strategy;
  m_iEERsuperRes = param->EERsuperRes;
  mAlignBin = param->aliBinning;
  m_strAlignBin.Format("%d", mAlignBin);
  m_iBinByOrTo = param->binToTarget ? 1 : 0;
  mAlignTargetSize = param->targetSize;
  m_strAlignTarget.Format("%d", mAlignTargetSize);
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
  m_bAlignSubset = param->alignSubset;
  m_iSubsetStart = param->subsetStart;
  m_iSubsetEnd = param->subsetEnd;
  m_bOnlyInPlugin = param->whereRestriction == 1;
  m_bOnlyWithIMOD = param->whereRestriction > 1;
  m_bOnlyNonSuperRes = param->sizeRestriction == 1;
  m_bOnlySuperRes = param->sizeRestriction > 1;
  m_strCurName = param->name;
  m_butAlignSubset.EnableWindow(canSubset);
  m_statSubsetTo.EnableWindow(canSubset);
  m_statSubsetParen.EnableWindow(canSubset);
  m_statSubsetTo.EnableWindow(canSubset);
  m_editSubsetStart.EnableWindow(canSubset && m_bAlignSubset);
  m_editSubsetEnd.EnableWindow(canSubset && m_bAlignSubset);
  ManageTruncation();
  ManageSmoothing();
  ManageGrouping();
  ManageAlignStategy();
  ManageRefinement();
  ManageAlignBinning();
  UpdateData(false);
  ManagePanels();
}


void CFrameAlignDlg::UnloadDialogToParam(int index)
{
  if (index < 0)
    return;
  FrameAliParams *param = mParams->GetData() + index;
  param->strategy = m_iAlignStrategy;
  param->EERsuperRes = m_iEERsuperRes;
  param->aliBinning = mAlignBin;
  param->binToTarget = m_iBinByOrTo > 0;
  param->targetSize = mAlignTargetSize;
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
  param->alignSubset = m_bAlignSubset;
  param->subsetStart = m_iSubsetStart;
  param->subsetEnd = m_iSubsetEnd;
  param->whereRestriction = 0;
  if (m_bOnlyInPlugin)
    param->whereRestriction = 1;
  else if (m_bOnlyWithIMOD)
    param->whereRestriction = 2;
  param->sizeRestriction = 0;
  if (m_bOnlyNonSuperRes)
    param->sizeRestriction = 1;
  else if (m_bOnlySuperRes)
    param->sizeRestriction = 2;
  param->name = m_strCurName;
}
