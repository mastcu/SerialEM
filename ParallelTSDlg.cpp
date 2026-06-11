// ParallelTSDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "ParallelTSDlg.h"
#include "NavHelper.h"
#include "EMscope.h"
#include "NavigatorDlg.h"
#include "ParticleTasks.h"
#include "ComplexTasks.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "SerialEMDoc.h"
#include "MultiShotDlg.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "AutoTuning.h"
#include "FocusManager.h"
#include "TSController.h"
#include "SerialEMView.h"
#include "ParallelTSHelper.h"
#include ".\Utilities\SEMUtilities.h"
#include "Shared\b3dutil.h"


#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

static int idTable[] = { IDC_STATIC_PTS_MAGFOR, IDC_STATIC_PTS_MAPPING, 
IDC_STATIC_PTS_MAPMAGVAL, IDC_STATIC_PTS_ACQUISITION,
IDC_STATIC_PTS_ACQMAGVAL, IDC_SPIN_PTS_MAPMAG, 
IDC_SPIN_PTS_ACQMAG, IDC_TSS_LINE3,
PANEL_END,
IDC_STATIC_PTS_SPECPRETILTANGLE, IDC_BUT_PTS_DEFINEFITPLANE,
IDC_STATIC_PTS_PRETILT, IDC_EDIT_PTS_PRETILT,  IDC_STATIC_PTS_PRETILTDEG,
IDC_STATIC_PTS_XPITCH, IDC_EDIT_PTS_XPITCH, IDC_STATIC_PTS_XPITCHDEG, 
IDC_TSS_LINE4,
PANEL_END,
IDC_STATIC_PTS_DEFININGAREA, IDC_RADIO_PTS_CUSTOMTARGETS,
IDC_RADIO_PTS_MULTISHOTTARGETS, IDC_BUT_PTS_ROUGHEUCEN, IDC_BUT_PTS_EUCENBYFOCUS,
IDC_BUT_PTS_STARTNEWTARGETAREA, IDC_STATIC_PTS_ACQUIRE, IDC_BUT_PTS_LDSEARCHELSEVIEW,
IDC_BUT_PTS_LDVIEWELSETRIAL, IDC_BUT_PTS_PREVIEW, IDC_BUT_PTS_MONTAGE,
IDC_BUT_PTS_SAVEAREAMAP, IDC_STATIC_PTS_MAPSTATUS, IDC_STATIC_PTS_ALIGNMENTSTARTING,
IDC_RADIO_PTS_TAKEPREV, IDC_RADIO_PTS_EXTRACTREF, IDC_RADIO_PTS_NOALIGN,
IDC_CHECK_PTS_APPLYADJUSTING, IDC_STATIC_PTS_ADJUSTXFORMSTATUS,
IDC_BUT_PTS_ADDTARGETS, IDC_BUT_PTS_REFINE, IDC_BUT_PTS_SAVETARGETMAP, 
IDC_BUT_PTS_REMOVETARGET,
IDC_BUT_PTS_FINALIZEAREA, IDC_BUT_PTS_ABORTAREA, 
IDC_STATIC_PTS_INHERITTS, IDC_STATIC_PTS_TSITEMINDEXLABEL,
IDC_BUT_PTS_SETUPTILTSERIES,
PANEL_END,
IDC_BUT_PTS_OPENCLOSEOPTIONS, IDC_STATIC_PTS_EXTRAOPTIONS, IDC_TSS_LINE5,
PANEL_END,
IDC_EDIT_PTS_MAXTILT, IDC_EDIT_PTS_EXTRADELAY, IDC_CHECK_PTS_BEAMSIZECIRCLES,
IDC_EDIT_PTS_DIAM, IDC_RADIO_PTS_CTFNONE, IDC_RADIO_PTS_CTFPLOTTER, IDC_RADIO_PTS_CTFFIND,
IDC_CHECK_PTS_FINDASTIG, IDC_STATIC_PTS_MAXTILT, IDC_STATIC_PTS_MAXTILTDEG,
IDC_STATIC_PTS_DIAM, IDC_STATIC_PTS_DIAMUM, IDC_STATIC_PTS_EXTRADELAY,
IDC_CHECK_PTS_ADJUSTBEAMTILT, IDC_STATIC_PTS_CTFTYPE, 
IDC_STATIC_PTS_MAXALIGNSHIFT, IDC_EDIT_PTS_MAXALIGNSHIFT, IDC_STATIC_PTS_PERCENTOFFIELD,
PANEL_END,
IDC_STATIC_PTS_ALIGNTOREFS, IDC_STATIC_PTS_MAXROTATION, IDC_EDIT_PTS_MAXROTATION,
IDC_STATIC_PTS_MAXROTDEG, IDC_STATIC_PTS_MAXSCALING, IDC_EDIT_PTS_MAXSCALING,
IDC_STATIC_PTS_MAXSCALINGPCT,
PANEL_END,
IDCANCEL, IDOK, IDC_BUTHELP, PANEL_END, TABLE_END};

static int sTopTable[sizeof(idTable) / sizeof(int)];
static int sLeftTable[sizeof(idTable) / sizeof(int)];
static int sHeightTable[sizeof(idTable) / sizeof(int)];

// CParallelTSDlg dialog

CParallelTSDlg::CParallelTSDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_PARALLELTS, pParent)
  , m_strMappingMag(_T(""))
  , m_strAcquisitionMag(_T(""))
  , m_fPretilt(0)
  , m_fXpitch(0)
  , m_iCustomOrMultishotTargets(0)
  , m_strTSItemIndexLabel(_T(""))
  , m_fMaxTilt(0)
  , m_fBeamDiameter(0)
  , m_bBeamSizeCircles(FALSE)
  , m_fExtraDelayFactor(0)
  , m_bAdjustBeamTiltAstig(FALSE)
  , m_iCTFType(0)
  , m_bFindAstig(FALSE)
  , m_bApplyAdjusting(FALSE)
  , m_fRefMaxRotation(0)
  , m_fRefMaxScaling(0)
  , m_iAlignRef(0)
  , m_fMaxAlignShift(0)
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mShiftManager = mWinApp->mShiftManager;
  mScope = NULL;
  mNavHelper = NULL;
  mParallelTSHelper = NULL;
  mNonModal = true;
  mDisplayExtraOptions = true;
  mIsOpen = false;
  mFitPlaneGroupID = 0;
  mTargetGroupID = 0;
  mDefiningPoints = false;
  mSettingUpTargetArea = false;
  mHasAreaMap = false;
  mAddingTargets = false;
  mRefiningTargets = false;
  mFinalizedTargetArea = false;
  mFinishedRefiningTargets = false;
  mDrawingISTargets = false;
  mNumAddedTargets = 0;
}

CParallelTSDlg::~CParallelTSDlg()
{
}

void CParallelTSDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_STATIC_PTS_MAPMAGVAL, m_statMappingMag);
  DDX_Control(pDX, IDC_SPIN_PTS_MAPMAG, m_sbcMappingMag);
  DDX_Control(pDX, IDC_STATIC_PTS_ACQMAGVAL, m_statAcquisitionMag);
  DDX_Control(pDX, IDC_SPIN_PTS_ACQMAG, m_sbcAcquisitionMag);
  DDX_Text(pDX, IDC_STATIC_PTS_MAPMAGVAL, m_strMappingMag);
  DDX_Text(pDX, IDC_STATIC_PTS_ACQMAGVAL, m_strAcquisitionMag);
  DDX_Control(pDX, IDC_STATIC_PTS_SPECPRETILTANGLE, m_statSpecimenPretiltAngle);
  DDX_Control(pDX, IDC_BUT_PTS_DEFINEFITPLANE, m_butDefinePtsFitPlane);
  DDX_Text(pDX, IDC_EDIT_PTS_PRETILT, m_fPretilt);
  DDV_MinMaxFloat(pDX, m_fPretilt, -90, 90);
  DDX_Text(pDX, IDC_EDIT_PTS_XPITCH, m_fXpitch);
  DDV_MinMaxFloat(pDX, m_fXpitch, -90, 90);
  DDX_Control(pDX, IDC_STATIC_PTS_DEFININGAREA, m_statDefineAreaTargets);
  DDX_Radio(pDX, IDC_RADIO_PTS_CUSTOMTARGETS, m_iCustomOrMultishotTargets);
  DDX_Control(pDX, IDC_RADIO_PTS_CUSTOMTARGETS, m_butAddCustomTargets);
  DDX_Control(pDX, IDC_RADIO_PTS_MULTISHOTTARGETS, m_butAddMultishotItem);
  DDX_Control(pDX, IDC_BUT_PTS_ADDTARGETS, m_butAddTargets);
  DDX_Control(pDX, IDC_BUT_PTS_REFINE, m_butRefineTargets);
  DDX_Text(pDX, IDC_STATIC_PTS_TSITEMINDEXLABEL, m_strTSItemIndexLabel);
  DDX_Control(pDX, IDC_BUT_PTS_OPENCLOSEOPTIONS, m_butOpenCloseOptions);
  DDX_Control(pDX, IDC_STATIC_PTS_EXTRAOPTIONS, m_statAcqDisplayOptions);
  DDX_Text(pDX, IDC_EDIT_PTS_MAXTILT, m_fMaxTilt);
  DDV_MinMaxFloat(pDX, m_fMaxTilt, -90, 90);
  DDX_Text(pDX, IDC_EDIT_PTS_DIAM, m_fBeamDiameter);
  DDV_MinMaxFloat(pDX, m_fBeamDiameter, 0, 10);
  DDX_Control(pDX, IDC_CHECK_PTS_BEAMSIZECIRCLES, m_butBeamSizeCircles);
  DDX_Check(pDX, IDC_CHECK_PTS_BEAMSIZECIRCLES, m_bBeamSizeCircles);
  DDX_Text(pDX, IDC_EDIT_PTS_EXTRADELAY, m_fExtraDelayFactor);
  DDV_MinMaxFloat(pDX, m_fExtraDelayFactor, 0, 100);
  DDX_Check(pDX, IDC_CHECK_PTS_ADJUSTBEAMTILT, m_bAdjustBeamTiltAstig);
  DDX_Radio(pDX, IDC_RADIO_PTS_CTFNONE, m_iCTFType);
  DDX_Check(pDX, IDC_CHECK_PTS_FINDASTIG, m_bFindAstig);
  DDX_Control(pDX, IDC_BUT_PTS_SAVETARGETMAP, m_butSaveTargetMap);
  DDX_Control(pDX, IDC_BUT_PTS_REMOVETARGET, m_butRemoveTarget);
  DDX_Control(pDX, IDC_BUT_PTS_FINALIZEAREA, m_butFinalizeTargetArea);
  DDX_Control(pDX, IDC_BUT_PTS_ABORTAREA, m_butAbortArea);
  DDX_Control(pDX, IDC_CHECK_PTS_APPLYADJUSTING, m_butApplyAdjusting);
  DDX_Check(pDX, IDC_CHECK_PTS_APPLYADJUSTING, m_bApplyAdjusting);
  DDX_Control(pDX, IDC_STATIC_PTS_MAPPING, m_statMapping);
  DDX_Control(pDX, IDC_STATIC_PTS_ACQUISITION, m_statAcquisition);
  DDX_Text(pDX, IDC_EDIT_PTS_MAXROTATION, m_fRefMaxRotation);
  DDV_MinMaxFloat(pDX, m_fRefMaxRotation, 0, 60);
  DDX_Text(pDX, IDC_EDIT_PTS_MAXSCALING, m_fRefMaxScaling);
  DDV_MinMaxFloat(pDX, m_fRefMaxScaling, 0, 100);
  DDX_Radio(pDX, IDC_RADIO_PTS_TAKEPREV, m_iAlignRef);
  DDX_Control(pDX, IDC_STATIC_PTS_MAGFOR, m_statMagFor);
  DDX_Control(pDX, IDC_EDIT_PTS_PRETILT, m_butPretilt);
  DDX_Control(pDX, IDC_EDIT_PTS_XPITCH, m_butXpitch);
  DDX_Control(pDX, IDC_BUT_PTS_ROUGHEUCEN, m_butRoughEucen);
  DDX_Control(pDX, IDC_BUT_PTS_EUCENBYFOCUS, m_butEucenByFocus);
  DDX_Control(pDX, IDC_BUT_PTS_STARTNEWTARGETAREA, m_butStartNewTargetArea);
  DDX_Control(pDX, IDC_BUT_PTS_LDSEARCHELSEVIEW, m_butLDSearchElseView);
  DDX_Control(pDX, IDC_BUT_PTS_LDVIEWELSETRIAL, m_butLDViewElseTrial);
  DDX_Control(pDX, IDC_BUT_PTS_PREVIEW, m_butPreview);
  DDX_Control(pDX, IDC_BUT_PTS_MONTAGE, m_butMontage);
  DDX_Control(pDX, IDC_BUT_PTS_SAVEAREAMAP, m_butSaveAreaMap);
  DDX_Control(pDX, IDC_RADIO_PTS_TAKEPREV, m_butAlignPreview);
  DDX_Control(pDX, IDC_BUT_PTS_SETUPTILTSERIES, m_butSetupTiltSeries);
  DDX_Control(pDX, IDC_EDIT_PTS_MAXTILT, m_butMaxTilt);
  DDX_Control(pDX, IDC_EDIT_PTS_DIAM, m_butDiameter);
  DDX_Control(pDX, IDC_EDIT_PTS_EXTRADELAY, m_butExtraISDelay);
  DDX_Control(pDX, IDC_CHECK_PTS_ADJUSTBEAMTILT, m_butBeamTiltAstig);
  DDX_Control(pDX, IDC_RADIO_PTS_CTFNONE, m_butCTFnone);
  DDX_Control(pDX, IDC_CHECK_PTS_FINDASTIG, m_butFindAstig);
  DDX_Control(pDX, IDC_EDIT_PTS_MAXROTATION, m_butMaxRotation);
  DDX_Control(pDX, IDC_EDIT_PTS_MAXSCALING, m_butMaxScaling);
  DDX_Control(pDX, IDC_STATIC_PTS_PRETILT, m_statPretilt);
  DDX_Control(pDX, IDC_STATIC_PTS_PRETILTDEG, m_statPretiltDeg);
  DDX_Control(pDX, IDC_STATIC_PTS_XPITCH, m_statXpitchAngle);
  DDX_Control(pDX, IDC_STATIC_PTS_XPITCHDEG, m_statXpitchAngleDeg);
  DDX_Control(pDX, IDC_STATIC_PTS_ALIGNMENTSTARTING, m_statAlignStartingTilt);
  DDX_Control(pDX, IDC_STATIC_PTS_ADJUSTXFORMSTATUS, m_statAdjustingXformStatus);
  DDX_Control(pDX, IDC_STATIC_PTS_INHERITTS, m_statInheritingTSparams);
  DDX_Control(pDX, IDC_STATIC_PTS_TSITEMINDEXLABEL, m_statTSitemLabel);
  DDX_Text(pDX, IDC_EDIT_PTS_MAXALIGNSHIFT, m_fMaxAlignShift);
  DDV_MinMaxFloat(pDX, m_fMaxAlignShift, 0, 100);
  DDX_Control(pDX, IDC_STATIC_PTS_MAPSTATUS, m_statAreaMapStatus);
}

BEGIN_MESSAGE_MAP(CParallelTSDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_PTS_MAPMAG, OnDeltaposSpinPtsMapmag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_PTS_ACQMAG, OnDeltaposSpinPtsAcqmag)
  ON_BN_CLICKED(IDC_BUT_PTS_DEFINEFITPLANE, OnDefinePtsFitPlane)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_PRETILT, OnEnKillfocusEditPretilt)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_XPITCH, OnEnKillfocusEditXpitch)
  ON_BN_CLICKED(IDC_RADIO_PTS_CUSTOMTARGETS, OnRadioCustomTargets)
  ON_BN_CLICKED(IDC_RADIO_PTS_MULTISHOTTARGETS, OnRadioMultishotTargets)
  ON_BN_CLICKED(IDC_BUT_PTS_ROUGHEUCEN, OnRoughEucen)
  ON_BN_CLICKED(IDC_BUT_PTS_EUCENBYFOCUS, OnEucenByFocus)
  ON_BN_CLICKED(IDC_BUT_PTS_STARTNEWTARGETAREA, OnStartNewTargetArea)
  ON_BN_CLICKED(IDC_BUT_PTS_LDSEARCHELSEVIEW, OnLDSearchElseView)
  ON_BN_CLICKED(IDC_BUT_PTS_LDVIEWELSETRIAL, OnLDViewElseTrial)
  ON_BN_CLICKED(IDC_BUT_PTS_PREVIEW, OnPreview)
  ON_BN_CLICKED(IDC_BUT_PTS_SAVEAREAMAP, OnSaveAreaMap)
  ON_BN_CLICKED(IDC_BUT_PTS_ADDTARGETS, OnAddTargets)
  ON_BN_CLICKED(IDC_BUT_PTS_SAVETARGETMAP, OnSaveTargetMap)
  ON_BN_CLICKED(IDC_BUT_PTS_REMOVETARGET, OnRemoveTarget)
  ON_BN_CLICKED(IDC_BUT_PTS_FINALIZEAREA, OnFinalizeArea)
  ON_BN_CLICKED(IDC_BUT_PTS_ABORTAREA, OnAbortArea)
  ON_BN_CLICKED(IDC_BUT_PTS_SETUPTILTSERIES, OnSetupTiltSeries)
  ON_BN_CLICKED(IDC_BUT_PTS_OPENCLOSEOPTIONS, OnOpenCloseOptions)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_MAXTILT, OnEnKillfocusEditMaxtilt)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_DIAM, OnEnKillfocusEditDiam)
  ON_BN_CLICKED(IDC_RADIO_PTS_CTFNONE, OnCtf)
  ON_BN_CLICKED(IDC_RADIO_PTS_CTFPLOTTER, OnCtf)
  ON_BN_CLICKED(IDC_RADIO_PTS_CTFFIND, OnCtf)
  ON_BN_CLICKED(IDC_BUT_PTS_MONTAGE, OnMontage)
  ON_BN_CLICKED(IDC_RADIO_PTS_TAKEPREV, OnRadioAlignRef)
  ON_BN_CLICKED(IDC_RADIO_PTS_EXTRACTREF, OnRadioAlignRef)
  ON_BN_CLICKED(IDC_RADIO_PTS_NOALIGN, OnRadioAlignRef)
  ON_BN_CLICKED(IDC_CHECK_PTS_APPLYADJUSTING, OnApplyadjusting)
  ON_BN_CLICKED(IDC_CHECK_PTS_BEAMSIZECIRCLES, OnBeamsizecircles)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_EXTRADELAY, OnEnKillfocusEditExtradelay)
  ON_BN_CLICKED(IDC_CHECK_PTS_ADJUSTBEAMTILT, OnAdjustbeamtilt)
  ON_BN_CLICKED(IDC_CHECK_PTS_FINDASTIG, OnFindastig)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_MAXALIGNSHIFT, OnEnKillfocusEditMaxalignshift)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_MAXROTATION, OnEnKillfocusEditMaxrotation)
  ON_EN_KILLFOCUS(IDC_EDIT_PTS_MAXSCALING, OnEnKillfocusEditMaxscaling)
  ON_BN_CLICKED(IDC_BUT_PTS_REFINE, &CParallelTSDlg::OnRefineTargets)
END_MESSAGE_MAP()

BOOL CParallelTSDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mScope = mWinApp->mScope;
  mNavHelper = mWinApp->mNavHelper;
  mParallelTSHelper = mWinApp->mParallelTSHelper;

  mBoldFont = mWinApp->GetBoldFont(&m_statSpecimenPretiltAngle);
  mRegFont = GetDlgItem(IDC_STATIC_PTS_PRETILT)->GetFont();
  m_statSpecimenPretiltAngle.SetFont(mBoldFont);
  m_statDefineAreaTargets.SetFont(mBoldFont);
  m_statAcqDisplayOptions.SetFont(mBoldFont);
  mIsOpen = true;

  mSTEMindex = mWinApp->GetSTEMMode();
  mCurrentCamera = mWinApp->GetCurrentActiveCamera();
  mParTSopts = mNavHelper->GetParTSOptions();

  m_sbcMappingMag.SetRange(1, MAX_MAGS);
  m_sbcAcquisitionMag.SetRange(1, MAX_MAGS);
 
  //Manage initial button disabling
  m_butAddTargets.EnableWindow(false);
  m_butRemoveTarget.EnableWindow(false);
  m_butSaveTargetMap.EnableWindow(false);
  m_butFinalizeTargetArea.EnableWindow(false);

  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->ManageEnables();

  SetupPanelTables(idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart,
    sHeightTable);
  OptionsToDialog();
  Update();
  UpdateData(false);
  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

void CParallelTSDlg::OnOK()
{
  mNavHelper->GetParallelTSPlacement();

  // Stop defining points if closed in the middle of this
  CancelAddingDefining();

  //Abort area if in the middle of making area
  if (mSettingUpTargetArea && mRefiningTargets) {
    mParallelTSHelper->StopParallelTSShift();
    ClearArea();
  }

  UpdateData(true);
  DialogToOptions();
  
  mIsOpen = false;
  mWinApp->mLowDoseDlg.Update();
  if (mNavHelper->mMultiShotDlg)
    mNavHelper->mMultiShotDlg->ManageEnables();
  DestroyWindow();
}

void CParallelTSDlg::OnCancel()
{
  *mParTSopts = mSavedParTSopts;
  OnOK();
}

void CParallelTSDlg::PostNcDestroy()
{
  CBaseDlg::PostNcDestroy();
}

BOOL CParallelTSDlg::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

// External close
void CParallelTSDlg::CloseWindow()
{
  if (mIsOpen) {
    OnOK();
  }
}

void CParallelTSDlg::Update()
{
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  CString mess;
  EMimageBuffer *imBuf, *activeImBuf;
  CMapDrawItem *item;
  int uncroppedX, uncroppedY;
  int numPoints;
  bool cropped, enable;
  IntVec indexVec;
  CString label, lastlabel;
  int numAcq;
  CWnd *pwnd;
  BOOL lowDose = mWinApp->LowDoseMode();
  BOOL noTasks = !mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() &&
    !mWinApp->mCamera->CameraBusy() && !mScope->GetMovingStage();

  if (mTargetGroupID) {
    numPoints = mWinApp->mNavigator->CountItemsInGroup(mTargetGroupID, label, lastlabel,
      numAcq, &indexVec);
  } else {
    numPoints = 0;
  }

  mWinApp->mLowDoseDlg.Update();
  m_butLDSearchElseView.SetWindowText(lowDose ? "Search" : "View");
  m_butLDViewElseTrial.SetWindowText(lowDose ? "View" : "Trial");
  m_butDefinePtsFitPlane.SetWindowText(mDefiningPoints ? "Stop Adding" :
    "Define Points and Fit Plane");
  m_butAddTargets.SetWindowText(mAddingTargets ? "Stop Adding" : (numPoints ?
    "Add More Targets" : "Add Targets"));
  m_butRefineTargets.SetWindowText(mAddingTargets ? "Stop Adding && Refine" : "Refine IS");

  CString saveType = m_iAlignRef == 0 ? "Map" : "IS";
  if (mRefiningTargets) {
    mess.Format("Save Target %s %d/%d", saveType, 
      mParallelTSHelper->GetSavedTargetsInNav(&indexVec) + 1, numPoints);
  } else {
    mess.Format("Save Target %s", saveType);
  }
  m_butSaveTargetMap.SetWindowText(mess);

  if (lowDose) {
    LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mAcqMagIndex = ldp->magIndex;
  }

  // Check map status, as well as if area map was deleted
  if (mHasAreaMap) {
    item = mWinApp->mNavigator->FindItemWithMapID(mParallelTSHelper->GetAreaMapID());
    if (!item) {
      SetDlgItemText(IDC_STATIC_PTS_MAPSTATUS, "Area map not defined");
      ClearArea();
    } else {
      mess.Format("Map note: %s", item->mNote);
      SetDlgItemText(IDC_STATIC_PTS_MAPSTATUS, mess);
    }
  } else {
    SetDlgItemText(IDC_STATIC_PTS_MAPSTATUS, "Area map not defined");
  }

  //Check if current buffer is ok for map
  activeImBuf = mWinApp->mActiveView->GetActiveImBuf();
  imBuf = &imBufs[(mWinApp->Montaging() &&
    imBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW) ? 1 : 0];
  cropped = imBuf->GetUncroppedSize(uncroppedX, uncroppedY) && uncroppedX > 0 &&
    uncroppedY > 0;
  enable = (activeImBuf->mImage && activeImBuf->mMapID > 0) || (imBuf->mImage &&
    !((mWinApp->Montaging() &&
    (imBuf->mCaptured != BUFFER_MONTAGE_OVERVIEW || imBuf->mSecNumber < 0))
      || (!mWinApp->Montaging() && imBuf->mCaptured < 0 && !cropped &&
        imBuf->mCaptured != BUFFER_PROC_OK_FOR_MAP)));
  m_butSaveAreaMap.EnableWindow(noTasks && mSettingUpTargetArea && enable
    && !(mDefiningPoints || mAddingTargets || mRefiningTargets) && 
    !mFinishedRefiningTargets);
  SetDlgItemText(IDC_BUT_PTS_SAVEAREAMAP,
    mHasAreaMap ? "Save New Area Map" : "Save Area Map");
  m_statAreaMapStatus.EnableWindow(mSettingUpTargetArea);
  
  
  //Manage other enable windows
  m_statMagFor.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_statMapping.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_statMappingMag.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_sbcMappingMag.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_statAcquisition.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_statAcquisitionMag.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_sbcAcquisitionMag.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  
  m_statSpecimenPretiltAngle.EnableWindow(!(mAddingTargets));
  m_butDefinePtsFitPlane.EnableWindow(!mParallelTSHelper->ISToTargetsBusy() &&
    mWinApp->mNavigator->m_butDrawPts.IsWindowEnabled() &&
    (mWinApp->mNavigator->NoDrawing() || mDefiningPoints) &&
    !(mAddingTargets || mRefiningTargets));
  m_statPretilt.EnableWindow(!(mAddingTargets));
  m_butPretilt.EnableWindow(noTasks && !(mDefiningPoints || mAddingTargets));
  m_statPretiltDeg.EnableWindow(!(mAddingTargets));
  m_statXpitchAngle.EnableWindow(!(mAddingTargets));
  m_butXpitch.EnableWindow(noTasks && !(mDefiningPoints || mAddingTargets));
  m_statXpitchAngleDeg.EnableWindow(!(mAddingTargets));
  
  m_statDefineAreaTargets.EnableWindow(!mDefiningPoints);
  m_butAddCustomTargets.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_butAddMultishotItem.EnableWindow(!(mDefiningPoints || mSettingUpTargetArea));
  m_butRoughEucen.EnableWindow(noTasks && !(mDefiningPoints || mSettingUpTargetArea));
  m_butEucenByFocus.EnableWindow(noTasks && !(mDefiningPoints || mSettingUpTargetArea));
  m_butStartNewTargetArea.EnableWindow(noTasks && 
    !(mDefiningPoints || mSettingUpTargetArea));
  m_butLDSearchElseView.EnableWindow(noTasks && (!mDefiningPoints));
  m_butLDViewElseTrial.EnableWindow(noTasks && (!mDefiningPoints));
  m_butPreview.EnableWindow(noTasks && (!mDefiningPoints));
  m_butMontage.EnableWindow(noTasks && (!mDefiningPoints));

  m_statAlignStartingTilt.EnableWindow(!mDefiningPoints);
  m_butAlignPreview.EnableWindow(!(mDefiningPoints || mAddingTargets));
  pwnd = GetDlgItem(IDC_RADIO_PTS_EXTRACTREF);
  if (pwnd)
    pwnd->EnableWindow(!(mDefiningPoints || mAddingTargets));
  pwnd = GetDlgItem(IDC_RADIO_PTS_NOALIGN);
  if (pwnd)
    pwnd->EnableWindow(!(mDefiningPoints || mAddingTargets));
  
  mess.Format("");
  int mag;
  mag = mParallelTSHelper->GetAreaMapMagInd();
  enable = mParallelTSHelper->CanAdjustISVectors(mag, mAcqMagIndex,
      m_iCustomOrMultishotTargets != 0, mess);
  SetDlgItemText(IDC_STATIC_PTS_ADJUSTXFORMSTATUS, mess);
  m_statAdjustingXformStatus.EnableWindow(!mDefiningPoints);
  m_butApplyAdjusting.EnableWindow(enable && !(mDefiningPoints || mAddingTargets));
  
  m_butAddTargets.EnableWindow(mSettingUpTargetArea && mHasAreaMap && 
    !(mDefiningPoints || mRefiningTargets) && 
    (!mAddingTargets || !mFinishedRefiningTargets || mNumAddedTargets == 0));
  m_butRefineTargets.EnableWindow(mTargetGroupID && mSettingUpTargetArea && mHasAreaMap &&
    !(mDefiningPoints || mRefiningTargets) && mNumAddedTargets);
  m_butSaveTargetMap.EnableWindow(mSettingUpTargetArea && !mFinishedRefiningTargets && 
    mRefiningTargets && noTasks);
  m_butRemoveTarget.EnableWindow(mSettingUpTargetArea && !mFinishedRefiningTargets && 
    mRefiningTargets && noTasks);
  m_butFinalizeTargetArea.EnableWindow(mSettingUpTargetArea && !mFinalizedTargetArea &&
    noTasks && ((m_iCustomOrMultishotTargets == 0 && mFinishedRefiningTargets &&
      mParallelTSHelper->GetSavedTargetsInNav(&indexVec) > 1) ||
      (m_iCustomOrMultishotTargets == 1)));
  m_butAbortArea.EnableWindow(noTasks && 
    ((mSettingUpTargetArea) || mAddingTargets) &&
    !mDefiningPoints && !mFinalizedTargetArea);

  m_butSetupTiltSeries.EnableWindow(mFinalizedTargetArea && !mDefiningPoints &&
    mParallelTSHelper->GetTSparamItem(item) >= 0);

  m_statInheritingTSparams.EnableWindow(!mDefiningPoints);

  // Check that the saved TS param still exists, and then update info on item where
  // TS parameters are stored
  mess.Format("");
  int TSparInd = mParallelTSHelper->GetSavedTSparamIndex();
  if (TSparInd >= 0) {
    if (TSparInd >= mWinApp->mNavigator->GetTSparamArray()->GetSize()) {
      mParallelTSHelper->SetSavedTSparamIndex(-1);
    } else {
      MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
      for (int ind = 0; ind < (int)itemArr->GetSize(); ind++) {
        item = itemArr->GetAt(ind);
        if (item->mTSparamIndex == TSparInd) {
          mess.Format("#%d, (%s)", ind + 1, item->mLabel);
          break;
        }
      }
    }  
  }
  SetDlgItemText(IDC_STATIC_PTS_TSITEMINDEXLABEL, mess);

  m_statTSitemLabel.EnableWindow(!mDefiningPoints);

  if (mHasIlluminatedArea <= 0)
    ReplaceWindowText(&m_butBeamSizeCircles, "illuminated area", "beam size from cal");

  // In case we want to enable or disable the extra buttons in the future
  /*m_butMaxTilt.EnableWindow();
  m_butDiameter.EnableWindow();
  m_butBeamSizeCircles.EnableWindow();
  m_butExtraISDelay.EnableWindow();
  m_butBeamTiltAstig.EnableWindow();
  m_butCTFnone.EnableWindow();
  pwnd = GetDlgItem(IDC_RADIO_PTS_CTFPLOTTER);
  if (pwnd)
    pwnd->EnableWindow();
  pwnd = GetDlgItem(IDC_RADIO_PTS_CTFFIND);
  if (pwnd)
    pwnd->EnableWindow();
  m_butFindAstig.EnableWindow();
  m_butMaxRotation.EnableWindow();
  m_butMaxScaling.EnableWindow();*/

  /*BOOL states[PARALLELTSDLG_NUM_PANELS] = { !mWinApp->LowDoseMode(), true, true, true,
    mDisplayExtraOptions, mDisplayExtraOptions && m_iAlignRef == 1, true };
  AdjustPanels(states, idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
    sHeightTable);
  mWinApp->RestoreViewFocus();*/
}

void CParallelTSDlg::ManagePanels()
{
  CMapDrawItem *item;
  int TSparInd = mParallelTSHelper->GetSavedTSparamIndex();
  bool foundTSparInd = false;

  // Toggle buttons for custom positions or multishot item
  mIDsToDrop.clear();
  if (m_iCustomOrMultishotTargets == 0) {
    m_butAddCustomTargets.SetFont(mBoldFont);
    m_butAddMultishotItem.SetFont(mRegFont);
  } else {
    m_butAddCustomTargets.SetFont(mRegFont);
    m_butAddMultishotItem.SetFont(mBoldFont);
    mIDsToDrop.push_back(IDC_BUT_PTS_ADDTARGETS);
    mIDsToDrop.push_back(IDC_BUT_PTS_REFINE);
    mIDsToDrop.push_back(IDC_BUT_PTS_SAVETARGETMAP);
    mIDsToDrop.push_back(IDC_BUT_PTS_REMOVETARGET);
    mIDsToDrop.push_back(IDC_STATIC_PTS_ALIGNMENTSTARTING);
    mIDsToDrop.push_back(IDC_RADIO_PTS_TAKEPREV);
    mIDsToDrop.push_back(IDC_RADIO_PTS_EXTRACTREF);
    mIDsToDrop.push_back(IDC_RADIO_PTS_NOALIGN);
  }

  
  if (TSparInd >= 0) {
    if (TSparInd >= mWinApp->mNavigator->GetTSparamArray()->GetSize()) {
      mParallelTSHelper->SetSavedTSparamIndex(-1);
    } else {
      MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
      for (int ind = 0; ind < (int)itemArr->GetSize(); ind++) {
        item = itemArr->GetAt(ind);
        if (item->mTSparamIndex == TSparInd) {
          foundTSparInd = true;
          break;
        }
      }
    }
  }
  if (!foundTSparInd) {
    mIDsToDrop.push_back(IDC_STATIC_PTS_INHERITTS);
    mIDsToDrop.push_back(IDC_STATIC_PTS_TSITEMINDEXLABEL);
  } 

  BOOL states[PARALLELTSDLG_NUM_PANELS] = { !mWinApp->LowDoseMode(), true, true, true,
    mDisplayExtraOptions, mDisplayExtraOptions && m_iAlignRef == 1, true };
  AdjustPanels(states, idTable, sLeftTable, sTopTable, mNumInPanel, mPanelStart, 0,
  sHeightTable);
  mWinApp->RestoreViewFocus();
}

// Update function to be executed when adding points starts or stops outside this dialog
void CParallelTSDlg::ExternalUpdate()
{
  if (!mWinApp->mNavigator)
    return;

  // If points are allowed to be added and not in the middle off adding, enable define pts
  if (mWinApp->mNavigator->NoDrawing() && mWinApp->mNavigator->m_butDrawPts.IsWindowEnabled())
    m_butDefinePtsFitPlane.EnableWindow();
  
  // If in the middle of adding points to the navigator, disable define pts
  if (!mWinApp->mNavigator->m_butDrawPts.IsWindowEnabled() || 
    (!mWinApp->mNavigator->NoDrawing() && !IsAddingToNav()))
    m_butDefinePtsFitPlane.EnableWindow(false);
  
  // If stopping adding points from the navigator when it was started in ParallelTSDlg,
  // Stop defining/adding points in ParallelTSDlg
  if (mWinApp->mNavigator->NoDrawing()) {
    if (mDefiningPoints)
      OnDefinePtsFitPlane();
    else if (mAddingTargets)
      OnAddTargets();
  }
  //mWinApp->RestoreViewFocus();
}

// Save a copy of the original ParallelTSoptions and load them into the dialog
void CParallelTSDlg::OptionsToDialog()
{
  mSavedParTSopts = *mParTSopts;

  if (!mWinApp->LowDoseMode()) {
    mAcqMagIndex = mParTSopts->acqMagIndNonLD;
    mMapMagIndex = mParTSopts->mapMagIndNonLD;

    m_sbcMappingMag.SetPos(mMapMagIndex);
    m_strMappingMag.Format("%d", MagForCamera(mCurrentCamera, mMapMagIndex));
    m_sbcAcquisitionMag.SetPos(mAcqMagIndex);
    m_strAcquisitionMag.Format("%d", MagForCamera(mCurrentCamera, mAcqMagIndex));

  } else {
    LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mAcqMagIndex = ldp->magIndex;
    ldp = mWinApp->GetLowDoseParams() + VIEW_CONSET;
    mMapMagIndex = ldp->magIndex;
  }
  
  m_bAdjustBeamTiltAstig = mParTSopts->adjustBeamTilt;
  m_fExtraDelayFactor = mParTSopts->extraDelayFactor;
  m_fBeamDiameter = mParTSopts->beamDiam;
  m_bBeamSizeCircles = mParTSopts->useIAorBeamSize;
  m_fMaxTilt = B3DABS(mParTSopts->tiltForBeam) > 90 ? 0 : mParTSopts->tiltForBeam;
  m_iCTFType = mParTSopts->CtfMeasureType;
  m_bFindAstig = mParTSopts->findAstig;
  if (m_iCustomOrMultishotTargets == 0)
    m_iAlignRef = mParTSopts->extractVirtPrevs;
  m_fMaxAlignShift = mParTSopts->alignLimitFrac * 100.f;
  m_fRefMaxScaling = mNavHelper->GetParTSRefAliMaxPctChg();
  m_fRefMaxRotation = mNavHelper->GetParTSRefAliMaxRot();
  m_bApplyAdjusting = mParTSopts->applyAdjustingXform;
}

// Update the options struct with values from the dialog
void CParallelTSDlg::DialogToOptions()
{
  if (!mWinApp->LowDoseMode()) {
    mParTSopts->acqMagIndNonLD = mAcqMagIndex;
    mParTSopts->mapMagIndNonLD = mMapMagIndex;
  }
  mParTSopts->adjustBeamTilt = m_bAdjustBeamTiltAstig;
  mParTSopts->extraDelayFactor = m_fExtraDelayFactor;
  mParTSopts->beamDiam = m_fBeamDiameter;
  mParTSopts->useIAorBeamSize = m_bBeamSizeCircles;
  mParTSopts->tiltForBeam = m_fMaxTilt;
  mParTSopts->CtfMeasureType = m_iCTFType;
  mParTSopts->findAstig = m_bFindAstig;
  mParTSopts->extractVirtPrevs = m_iCustomOrMultishotTargets == 0 ? m_iAlignRef : 2;
  mParTSopts->alignLimitFrac  = m_fMaxAlignShift / 100.f;
  mParTSopts->refAliMaxPctChg = m_fRefMaxScaling;
  mParTSopts->refAliMaxRot = m_fRefMaxRotation;
  mParTSopts->applyAdjustingXform = m_bApplyAdjusting != 0;
}

bool CParallelTSDlg::AreaMapInBuf(EMimageBuffer *imBuf)
{
  int mapID = mParallelTSHelper->GetAreaMapID();
  return (mapID > 0 && imBuf->mMapID == mapID);
}

bool CParallelTSDlg::KeepAddingChoiceBox(CString mess)
{
  int numPoints, arrSize, choice;
  bool keepAdding;
  CMapDrawItem *item;
  MapItemArray *itemArray = mWinApp->mNavigator->GetItemArray();
  arrSize = (int)itemArray->GetSize();

  //numPoints = arrSize - mNumberBeforeAdd;

  IntVec indexVec;
  int numAcq;
  CString label, lastlabel;

  if (mWinApp->mNavigator->GetCurrentOrGroupItem(item) < 0)
    return false;

  int groupID = item->mGroupID;
  numPoints = mWinApp->mNavigator->CountItemsInGroup(groupID, label, lastlabel,
    numAcq, &indexVec);

  choice = SEMThreeChoiceBox(mess, "Start Over", "Add More Points", "Cancel",
    MB_YESNOCANCEL | MB_ICONQUESTION);
  keepAdding = choice == IDYES || choice == IDNO;

  if (choice == IDYES || choice == IDCANCEL) {
    if (numPoints == 1)
      mWinApp->mNavigator->DeleteItem();
    else
      mWinApp->mNavigator->ExternalDeleteGroup(mWinApp->mNavigator->m_bCollapseGroups != 0);
    mNumberBeforeAdd = (int)itemArray->GetSize();
  }

  if (choice == IDCANCEL) {
    mFitPlaneGroupID = 0;
  }
  return keepAdding;
}

void CParallelTSDlg::DoPlaneFit()
{
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  int err, numPoints;
  bool keepAdding = false;
  CString mess;
  IntVec indexVec, sortedIndexVec, mapIDs;
  CString label, lastlabel;
  int numAcq;

  numPoints = mWinApp->mNavigator->CountItemsInGroup(mFitPlaneGroupID, label, lastlabel, 
    numAcq, &indexVec);

  // Asses the quality of the sample points, and get them sorted with center point first
  err = mParallelTSHelper->AssessPtsToFitPlane(indexVec, sortedIndexVec, mess);
  if (err) {
    keepAdding = KeepAddingChoiceBox(mess);
    if (keepAdding)
      OnDefinePtsFitPlane();

    return;
  }

  for (int i = 0; i < (int)sortedIndexVec.size(); i++) {
    item = itemArr->GetAt(sortedIndexVec[i]);
    mapIDs.push_back(item->mMapID);
  }

  // Execute routine to image shift to each target, autofocusing at each point

  // TODO do something with this error code?
  err = mParallelTSHelper->StartShiftToTargets(mapIDs, true);

  Update();
}

void CParallelTSDlg::UpdatePlaneParams(float pretilt, float xPitchAngle) 
{
  m_fPretilt = roundf(pretilt * 100) / 100.f;
  m_fXpitch = roundf(xPitchAngle * 100) / 100.f;
  UpdateData(false);
  Update();
}

void CParallelTSDlg::StartRefineTargets()
{
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();
  int err, numPoints;
  bool keepAdding = false;
  CString mess;
  IntVec indexVec, mapIDs;
  CString label, lastlabel;
  int numAcq;

  numPoints = mWinApp->mNavigator->CountItemsInGroup(mTargetGroupID, label, lastlabel,
    numAcq, &indexVec);

  if (numPoints == 0) {
    err = -2;
    mess = "At least 1 targets is required";
  } else {
    // Remove points outside image shift limit
    err = mParallelTSHelper->AssessISTargetShiftLimit(indexVec, mess);
  }
  if (err) {
    keepAdding = KeepAddingChoiceBox(mess);
    if (keepAdding)
      OnAddTargets();

    return;
  }

  for (int i = 0; i < (int)indexVec.size(); i++) {
    item = itemArr->GetAt(indexVec[i]);
    mapIDs.push_back(item->mMapID);
  }

  // Execute routine to start image shifting to each target and taking Preview images
  err = mParallelTSHelper->StartShiftToTargets(mapIDs, false);
  /*if (err) {
    OnAddTargets();
  }*/
  if (!err) {

    //New targets were added that need to be refined
    mFinishedRefiningTargets = false;
    mRefiningTargets = true;
  }

  UpdateData(true);
  Update();
}

void CParallelTSDlg::FinishFitPlane()
{
  mFitPlaneGroupID = 0;
  mWinApp->mNavigator->Redraw();
  UpdateData(true);
  Update();
}

void CParallelTSDlg::FinishRefineTargets(bool error)
{
  mRefiningTargets = false;
  mFinishedRefiningTargets = !error;
  if (mFinishedRefiningTargets)
    mNumAddedTargets = 0;

  CMapDrawItem *mapItem = mWinApp->mNavigator->FindItemWithMapID(mParallelTSHelper->GetAreaMapID());
  mWinApp->mNavigator->DoLoadMap(true, mapItem, -1);

  UpdateData(true);
  Update();
  //mWinApp->RestoreViewFocus();
}


// CParallelTSDlg message handlers


void CParallelTSDlg::OnDeltaposSpinPtsMapmag(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;

  // Move in given direction until an index is reached with a listed
  // mag or until the end of the table
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, mMapMagIndex,
    pNMUpDown->iDelta);
  if (newVal <= 0 || newVal >= mAcqMagIndex)
    return;
  mMapMagIndex = newVal;
  UPDATE_DATA_TRUE;
  DialogToOptions();
  m_strMappingMag.Format("%d", MagForCamera(mCurrentCamera, newVal));
  UpdateData(false);
  *pResult = 0;
  Update();
  ManagePanels();
}


void CParallelTSDlg::OnDeltaposSpinPtsAcqmag(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;
  int lowLim = mScope->GetLowestNonLMmag(mSTEMindex);

  // Move in given direction until an index is reached with a listed
  // mag or until the end of the table
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, mAcqMagIndex,
    pNMUpDown->iDelta);
  if (newVal < lowLim || newVal <= mMapMagIndex)
    return;
  mAcqMagIndex = newVal;
  UPDATE_DATA_TRUE;
  DialogToOptions();
  m_strAcquisitionMag.Format("%d", MagForCamera(mCurrentCamera, newVal));
  UpdateData(false);
  *pResult = 0;
  Update();
  ManagePanels();
}

void CParallelTSDlg::CancelAddingDefining() 
{
  CString label, lastlabel;
  int numAcq, numPoints, arrSize;
  IntVec indexVec;
  CMapDrawItem *item;
  MapItemArray *itemArray;

  if (mDefiningPoints)
    mDefiningPoints = false;
  else if (mAddingTargets)
    mAddingTargets = false;
  else
    return;

  if (mWinApp->mNavigator) {
    itemArray = mWinApp->mNavigator->GetItemArray();
    arrSize = (int)itemArray->GetSize();
    if (!mWinApp->mNavigator->NoDrawing()) {
      mDrawingISTargets = true;
      mWinApp->mNavigator->OnDrawPoints();
      mDrawingISTargets = false;
    }

    if (arrSize > mNumberBeforeAdd && 
      mWinApp->mNavigator->GetCurrentOrGroupItem(item) >= 0) {
      numPoints = mWinApp->mNavigator->CountItemsInGroup(item->mGroupID, label, lastlabel,
        numAcq, &indexVec);
      if (numPoints == 1)
        mWinApp->mNavigator->DeleteItem();
      else
        mWinApp->mNavigator->ExternalDeleteGroup(
          mWinApp->mNavigator->m_bCollapseGroups != 0);
    }
  }
}

void CParallelTSDlg::OnDefinePtsFitPlane()
{
  int numPoints, numAcq;
  bool keepAdding = false;
  CString label, lastlabel;
  IntVec indexVec;
  CNavigatorDlg *nav = mWinApp->mNavigator;
  MapItemArray *itemArray = mWinApp->mNavigator->GetItemArray();
  int arrSize = (int)itemArray->GetSize();

  mDefiningPoints = !mDefiningPoints;

  if (mDefiningPoints) {
    mNumberBeforeAdd = arrSize;
    if (!mFitPlaneGroupID) {
      mFitPlaneGroupID = mWinApp->mNavigator->MakeUniqueID();
    }
  } 
  
  // Call on navigator to start adding points, or if stopping, tell it to 
  // stop adding points if not already stopped from the navigator  
  if (mDefiningPoints || !nav->NoDrawing()) {
    mDrawingISTargets = true;
    mWinApp->mNavigator->OnDrawPoints();
    mDrawingISTargets = false;
  }
  
  if (!mDefiningPoints) {
    if (nav->m_bCollapseGroups) {
      nav->MakeListMappings();
      nav->FillListBox();
    }
    nav->Redraw();

    //If no points have been added, don't bother doing anything.
    numPoints = nav->CountItemsInGroup(mFitPlaneGroupID, label, lastlabel,
      numAcq, &indexVec);
    if (numPoints > 0)
      DoPlaneFit();
  }
  Update();
  ManagePanels();
}


void CParallelTSDlg::OnEnKillfocusEditPretilt()
{
  UpdateData(true);
  mParallelTSHelper->SetPretilt(m_fPretilt);
}


void CParallelTSDlg::OnEnKillfocusEditXpitch()
{
  UpdateData(true);
  mParallelTSHelper->SetXpitch(m_fXpitch);
}


void CParallelTSDlg::OnRadioCustomTargets()
{
  UpdateData(true);
  Update();
  ManagePanels();
  DialogToOptions();
}


void CParallelTSDlg::OnRadioMultishotTargets()
{
  UpdateData(true);
  Update();
  ManagePanels();
  DialogToOptions();
}


void CParallelTSDlg::OnRoughEucen()
{
  mWinApp->mComplexTasks->FindEucentricity(1);
  Update();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnEucenByFocus()
{
  mWinApp->mParticleTasks->EucentricityFromFocus(-1);
  Update();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnStartNewTargetArea()
{
  CMapDrawItem *item;
  UpdateData(true);
  ClearArea();
  mSettingUpTargetArea = true;
  item = mWinApp->mNavigator->FindItemWithMapID(mParallelTSHelper->GetAreaMapID());
  mHasAreaMap = item != NULL;
  mWinApp->mLowDoseDlg.Update();
  mScope->TiltTo(m_fPretilt);
  Update();
  ManagePanels();
}


void CParallelTSDlg::OnLDSearchElseView()
{
  if (mWinApp->LowDoseMode())
    mWinApp->UserRequestedCapture(SEARCH_CONSET);
  else {
    if (mScope->GetMagIndex() != mMapMagIndex &&
      !mScope->SetMagIndex(mMapMagIndex)) {
      AfxMessageBox("Failed to set the desired map magnification", MB_EXCLAME);
      return;
    }
    mWinApp->UserRequestedCapture(VIEW_CONSET);
  }
  Update();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnLDViewElseTrial()
{
  if (mWinApp->LowDoseMode())
    mWinApp->UserRequestedCapture(VIEW_CONSET);
  else {
    if (mScope->GetMagIndex() != mMapMagIndex &&
      !mScope->SetMagIndex(mMapMagIndex)) {
      AfxMessageBox("Failed to set the desired map magnification", MB_EXCLAME);
      return;
    }
    mWinApp->UserRequestedCapture(TRIAL_CONSET);
  }
  Update();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnPreview()
{
  if (!mWinApp->LowDoseMode()) {
    if (mScope->GetMagIndex() != mAcqMagIndex &&
      !mScope->SetMagIndex(mAcqMagIndex)) {
      AfxMessageBox("Failed to set the desired acquire magnification", MB_EXCLAME);
      return;
    }
  }
  mWinApp->UserRequestedCapture(PREVIEW_CONSET);
  Update();
  //mWinApp->RestoreViewFocus();
}

void CParallelTSDlg::OnMontage()
{
  if (!mWinApp->LowDoseMode()) {
    if (mScope->GetMagIndex() != mMapMagIndex &&
      !mScope->SetMagIndex(mMapMagIndex)) {
      AfxMessageBox("Failed to set the desired map magnification", MB_EXCLAME);
      return;
    }
  }
  mWinApp->StartMontageOrTrial(false);
  Update();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnSaveAreaMap()
{
  CString mess;

  if (mParallelTSHelper->SaveAreaMap(mess)) {
    AfxMessageBox(mess, MB_EXCLAME);
  } else {
    mHasAreaMap = true;
  }

  Update();
  ManagePanels();
}


void CParallelTSDlg::OnAddTargets()
{
  MapItemArray *itemArray = mWinApp->mNavigator->GetItemArray();
  CNavigatorDlg *nav = mWinApp->mNavigator;
  int arrSize = (int)itemArray->GetSize();

  mAddingTargets = !mAddingTargets;

  if (mAddingTargets) {
    mFinalizedTargetArea = false;
    mNumberBeforeAdd = arrSize;
    mNumAddedTargets = 0;
    if (!mTargetGroupID) {
      mTargetGroupID = nav->MakeUniqueID();
      mNavHelper->SetParTSSetupGroupID(mTargetGroupID);
    }
  } 

  // Do not call on navigator to start/stop adding points, if adding points was 
  // already succesfully stopped from the navigator button 
  if (mAddingTargets || !mWinApp->mNavigator->NoDrawing()) {
    mDrawingISTargets = true;
    mWinApp->mNavigator->OnDrawPoints();
    mDrawingISTargets = false;
  }

  if (!mAddingTargets) {
    if (nav->m_bCollapseGroups) {
      nav->MakeListMappings();
      nav->FillListBox();
    }
    nav->Redraw();
  }

  UpdateData(true);
  Update();
  ManagePanels();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnSaveTargetMap()
{
  mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
  mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnRemoveTarget()
{
  CMapDrawItem *item = mParallelTSHelper->GetCurISTargetItem();
  mWinApp->mNavigator->FindItemWithMapID(item->mMapID, false);
  if (item) {
    mWinApp->mNavigator->ExternalDeleteItem(item, mWinApp->mNavigator->GetFoundItem());
  }
  mWinApp->AddIdleTask(TASK_IS_TO_PARALLELTS_TARGET, 0, 0);
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnFinalizeArea()
{
  int ans;
  CMapDrawItem *item;
  CString err;

  UpdateData(true);
  
  if (m_iCustomOrMultishotTargets == 0) {
    ans = AfxMessageBox("Are you done rearranging or removing points from the Navigator "
      "table, and is the first target point in the table the desired central point?", 
      MB_YESNO);
    if (ans != IDYES) {
      return;
    }
    if (mParallelTSHelper->GetNumSavedTargets() < 2) {
      AfxMessageBox("There are must be at least two IS targets.", MB_EXCLAME);
      return;
    }
    item = NULL;
  } else if (m_iCustomOrMultishotTargets == 1) {
    item = mWinApp->mNavigator->GetCurrentItem();
  }

  if (mParallelTSHelper->ConvertToParTSItem(err, item)) {
    AfxMessageBox(err, MB_EXCLAME);
    return;
  }
  
  mTargetGroupID = 0;
  mNavHelper->SetParTSSetupGroupID(0);
  mWinApp->mNavigator->Redraw();
  
  mFinalizedTargetArea = true;
  mFinishedRefiningTargets = false;
  mSettingUpTargetArea = false;

  Update();
  ManagePanels();
  //mWinApp->RestoreViewFocus();
}

//For internal calls to abort the area, without needing to update the dialog
void CParallelTSDlg::ClearArea()
{
  mSettingUpTargetArea = false;
  if (mRefiningTargets) {
    mParallelTSHelper->StopParallelTSShift();
  }
  if (mTargetGroupID) {
    mTargetGroupID = 0;
    mNavHelper->SetParTSSetupGroupID(0);
    mWinApp->mNavigator->Redraw();
  }
  mParallelTSHelper->ClearTargets(false);
  mRefiningTargets = false;
  mFinishedRefiningTargets = false;
  mFinalizedTargetArea = false;
  mNumAddedTargets = 0;
}

void CParallelTSDlg::OnAbortArea()
{
  int numPoints, ind, jnd;
  bool del;
  IntVec indexVec, mapIDs;
  CString label, lastlabel;
  CMapDrawItem *item;
  MapItemArray *itemArr = mWinApp->mNavigator->GetItemArray();

  UpdateData(true);

  // If currently adding, stop navigator adding and delete points
  if (mAddingTargets || !mWinApp->mNavigator->NoDrawing()) {
    CancelAddingDefining();
  }

  mapIDs = mParallelTSHelper->GetPreviewMapIDs();
  del = true;
  if (mapIDs.size() > 0) {
    ind = AfxMessageBox("Are you sure you want to delete all of this area's targets and "
      "target maps from the Navigator?", MB_QUESTION);
    if (ind == IDNO)
      return;
  }
  
  //Delete un-finalized targets, plus all others if user said so
  numPoints = mWinApp->mNavigator->CountItemsInGroup(mTargetGroupID, label, lastlabel,
    jnd, &indexVec);
  if (mTargetGroupID && numPoints > 0) {
    mWinApp->mNavigator->SetSelectedItem(indexVec[0], false);
    mWinApp->mNavigator->ExternalDeleteGroup(mWinApp->mNavigator->m_bCollapseGroups != 0);

    for (ind = 0; ind < (int)mapIDs.size(); ind++) {
      item = mWinApp->mNavigator->FindItemWithMapID(mapIDs[ind]);
      if (item) {
        jnd = mWinApp->mNavigator->GetFoundItem();
        mWinApp->mNavigator->ExternalDeleteItem(item, jnd);
      }
    }
  }

  ClearArea();
  Update();
  ManagePanels();
  //mWinApp->RestoreViewFocus();
}

void CParallelTSDlg::OnSetupTiltSeries()
{
  mParallelTSHelper->UpdateTSParams();
  Update();
  ManagePanels();
  //mWinApp->RestoreViewFocus();
}

void CParallelTSDlg::OnOpenCloseOptions()
{
  UpdateData(true);
  if (mDisplayExtraOptions) {
    SetDlgItemText(IDC_BUT_PTS_OPENCLOSEOPTIONS, "+");
    mDisplayExtraOptions = false;
  } else {
    SetDlgItemText(IDC_BUT_PTS_OPENCLOSEOPTIONS, "-");
    mDisplayExtraOptions = true;
  }
  Update();
  ManagePanels();
  //mWinApp->RestoreViewFocus();
}


void CParallelTSDlg::OnEnKillfocusEditMaxtilt()
{
  UpdateData(true);
  DialogToOptions();
  mWinApp->mNavigator->Redraw();
}


void CParallelTSDlg::OnEnKillfocusEditDiam()
{
  UpdateData(true);
  DialogToOptions();
  mWinApp->mNavigator->Redraw();
}



void CParallelTSDlg::OnCtf()
{
  UpdateData(true);
  DialogToOptions();
}




void CParallelTSDlg::OnRadioAlignRef()
{
  UpdateData(true);
  Update();
  ManagePanels();
  DialogToOptions();
}


void CParallelTSDlg::OnApplyadjusting()
{

  CString mess;
  MultiShotParams *params = mNavHelper->GetMultiShotParams();
  
  UpdateData(true);

  if (m_iCustomOrMultishotTargets) {
    mNavHelper->AdjustMultiShotVectors(params,
      B3DCHOICE(params->useCustomHoles, 1, params->doHexArray ? -1 : 0), false, mess);
    if (mWinApp->mNavigator)
      mWinApp->mNavigator->Redraw();
  }

  DialogToOptions();
}


void CParallelTSDlg::OnBeamsizecircles()
{
  UpdateData(true);
  DialogToOptions();
  mWinApp->mNavigator->Redraw();
}


void CParallelTSDlg::OnEnKillfocusEditExtradelay()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnAdjustbeamtilt()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnFindastig()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnEnKillfocusEditMaxalignshift()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnEnKillfocusEditMaxrotation()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnEnKillfocusEditMaxscaling()
{
  UpdateData(true);
  DialogToOptions();
}


void CParallelTSDlg::OnRefineTargets()
{
  MapItemArray *itemArray = mWinApp->mNavigator->GetItemArray();
  int arrSize = (int)itemArray->GetSize();

  if (mAddingTargets) {
    OnAddTargets();
  }

  ////Start refining targets if points have been added
  //if (arrSize > mNumberBeforeAdd) {
  //  StartRefineTargets();
  //}

  StartRefineTargets();
}
