// StepAdjustISDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "NavHelper.h"
#include "HoleFinderDlg.h"
#include "ShiftManager.h"
#include "MultiShotDlg.h"
#include "StepAdjustISDlg.h"


// CStepAdjustISDlg dialog


CStepAdjustISDlg::CStepAdjustISDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_STEP_AND_ADJUST_IS, pParent)
  , m_iAreaToUse(0)
  , m_iMagTypeToUse(0)
  , m_strOtherMag(_T(""))
  , m_bSetDefOffset(FALSE)
  , m_strDefOffset(_T(""))
  , m_bImageAfterIS(FALSE)
  , m_bDoAutomatic(FALSE)
  , m_iXCorrOrHoleCen(0)
  , m_fHoleSize(1.f)
  , m_iAutoMethod(0)
  , m_fShiftLimit(0.5f)
  , m_bSetPrevExp(FALSE)
  , m_fPrevExposure(1.0f)
{

}

CStepAdjustISDlg::~CStepAdjustISDlg()
{
}

void CStepAdjustISDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_STAA_SEARCH, m_iAreaToUse);
  DDX_Radio(pDX, IDC_STAA_CURRENT_MAG, m_iMagTypeToUse);
  DDX_Control(pDX, IDC_STAT_OTHER_MAG, m_statOtherMag);
  DDX_Text(pDX, IDC_STAT_OTHER_MAG, m_strOtherMag);
  DDX_Control(pDX, IDC_SPIN_OTHER_MAG, m_sbcOtherMag);
  DDX_Check(pDX, IDC_CHECK_SET_DEF_OFFSET, m_bSetDefOffset);
  DDX_Text(pDX, IDC_STAT_DEF_OFFSET, m_strDefOffset);
  DDX_Control(pDX, IDC_SPIN_DEF_OFFSET, m_sbcDefOffset);
  DDX_Control(pDX, IDC_CHECK_IMAGE_AFTER_IS, m_butImageAfterIS);
  DDX_Check(pDX, IDC_CHECK_IMAGE_AFTER_IS, m_bImageAfterIS);
  DDX_Check(pDX, IDC_STAA_DO_AUTOMATIC, m_bDoAutomatic);
  DDX_Text(pDX, IDC_EDIT_STAA_HOLE, m_fHoleSize);
  DDV_MinMaxFloat(pDX, m_fHoleSize, 0.05f, 10.f);
  DDX_Radio(pDX, IDC_STAA_CROSSCORR, m_iAutoMethod);
  DDX_Text(pDX, IDC_EDIT_STAA_LIMIT, m_fShiftLimit);
  DDV_MinMaxFloat(pDX, m_fShiftLimit, 0.05f, 1.f);
  DDX_Check(pDX, IDC_CHECK_SET_PREV_EXP, m_bSetPrevExp);
  DDX_Text(pDX, IDC_EDIT_PREVIEW_EXP, m_fPrevExposure);
	DDV_MinMaxFloat(pDX, m_fPrevExposure, 0.05f, 10.f);
}


BEGIN_MESSAGE_MAP(CStepAdjustISDlg, CBaseDlg)
  ON_BN_CLICKED(IDC_STAA_SEARCH, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_VIEW, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_RECORD, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_2X2MONTAGE, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_2X3MONTAGE, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_CURRENT_MAG, OnMagTypeToUse)
  ON_BN_CLICKED(IDC_STAA_PREV_MAG, OnMagTypeToUse)
  ON_BN_CLICKED(IDC_STAA_OTHER_MAG, OnMagTypeToUse)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_OTHER_MAG, OnDeltaposSpinOtherMag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_DEF_OFFSET, OnDeltaposSpinDefOffset)
  ON_BN_CLICKED(IDC_CHECK_SET_DEF_OFFSET, OnCheckSetDefOffset)
  ON_BN_CLICKED(IDC_STAA_DO_AUTOMATIC, OnStaaDoAutomatic)
  ON_BN_CLICKED(IDC_STAA_CROSSCORR, OnRadioCrosscorr)
  ON_BN_CLICKED(IDC_STAA_CENTER_HOLE, OnRadioCrosscorr)
  ON_EN_KILLFOCUS(IDC_EDIT_STAA_HOLE, OnKillfocusEditStaaHole)
  ON_EN_KILLFOCUS(IDC_EDIT_STAA_LIMIT, OnKillfocusEditStaaLimit)
  ON_BN_CLICKED(IDC_BUT_STAA_GO, OnButStaaStart)
  ON_BN_CLICKED(IDC_CHECK_SET_PREV_EXP, OnCheckSetPrevExp)
  ON_EN_KILLFOCUS(IDC_EDIT_PREVIEW_EXP, OnKillfocusEditPreviewExp)
END_MESSAGE_MAP()


// CStepAdjustISDlg message handlers
BOOL CStepAdjustISDlg::OnInitDialog()
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CString str;
  CBaseDlg::OnInitDialog();
  mLowestM = mWinApp->mScope->GetLowestMModeMagInd();
  mParam = mWinApp->mNavHelper->GetMultiShotParams();
  mOtherMagInd = mParam->stepAdjOtherMag;
  if (mOtherMagInd < 0)
    mOtherMagInd = mParam->stepAdjLDarea == 2 ? ldp[RECORD_CONSET].magIndex - 1 : 
    mLowestM + 1;
  mDefocusOffset = mParam->stepAdjDefOffset;
  m_iAreaToUse = mParam->stepAdjLDarea;
  m_iMagTypeToUse = mParam->stepAdjWhichMag;
  m_bDoAutomatic = mParam->doAutoAdjustment;
  if (mWinApp->mNavHelper->mHoleFinderDlg->IsOpen())
    m_fHoleSize = mWinApp->mNavHelper->mHoleFinderDlg->m_fHolesSize;
  else
    m_fHoleSize = mParam->autoAdjHoleSize;
  m_bSetDefOffset = mParam->stepAdjSetDefOff;
  m_bImageAfterIS = mParam->stepAdjTakeImage;
  m_fShiftLimit = mParam->autoAdjLimitFrac;
  m_iAutoMethod = mParam->autoAdjMethod;
  m_bSetPrevExp = mParam->stepAdjSetPrevExp;
  m_fPrevExposure = mParam->stepAdjPrevExp;
  m_strOtherMag.Format("%d", MagForCamera(mWinApp->GetCurrentCamera(), mOtherMagInd));
  m_sbcOtherMag.SetRange(0, 10000);
  m_sbcOtherMag.SetPos(5000);
  m_sbcDefOffset.SetRange(0, 10000);
  m_sbcDefOffset.SetPos(5000);
  m_strDefOffset.Format("%d", mDefocusOffset);
  str.Format("Mag where shifts were recorded, %dx", 
    MagForCamera(mWinApp->GetCurrentCamera(), mPrevMag));
  SetDlgItemText(IDC_STAA_PREV_MAG, str);
  mStartRoutine = false;
  ManageEnables();
  UpdateData(false);
  SetDefID(IDC_BUT_STAA_GO);
  return TRUE;
}

// Close button
void CStepAdjustISDlg::OnOK()
{
  UpdateData(true);
  mParam->stepAdjDefOffset = mDefocusOffset;
  mParam->stepAdjOtherMag = mOtherMagInd;
  mParam->stepAdjLDarea = m_iAreaToUse;
  mParam->stepAdjWhichMag = m_iMagTypeToUse;
  mParam->stepAdjSetDefOff = m_bSetDefOffset;
  mParam->stepAdjTakeImage = m_bImageAfterIS;
  mParam->doAutoAdjustment = m_bDoAutomatic;
  mParam->autoAdjHoleSize = m_fHoleSize;
  mParam->stepAdjSetDefOff = m_bSetDefOffset;
  mParam->autoAdjLimitFrac = m_fShiftLimit;
  mParam->autoAdjMethod = m_iAutoMethod;
  mParam->stepAdjSetPrevExp = m_bSetPrevExp;
  mParam->stepAdjPrevExp = m_fPrevExposure;
  CBaseDlg::OnOK();
}

// Cancel
void CStepAdjustISDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}

// Start the procedure
void CStepAdjustISDlg::OnButStaaStart()
{
  mStartRoutine = true;
  OnOK();
}

// Nothing special in any message responses
void CStepAdjustISDlg::OnAreaToUse()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::OnCheckSetPrevExp()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::OnKillfocusEditPreviewExp()
{
  UpdateData(true);
}

void CStepAdjustISDlg::OnStaaDoAutomatic()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::OnRadioCrosscorr()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::OnKillfocusEditStaaHole()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::OnKillfocusEditStaaLimit()
{
  UpdateData(true);
}

void CStepAdjustISDlg::OnMagTypeToUse()
{
  UpdateData(true);
  ManageEnables();
}

// The Other mag has changed
void CStepAdjustISDlg::OnDeltaposSpinOtherMag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newmag;
  newmag = mWinApp->FindNextMagForCamera(mWinApp->GetCurrentCamera(), mOtherMagInd,
    pNMUpDown->iDelta > 0 ? 1 : -1);
  if (newmag < mWinApp->mScope->GetLowestMModeMagInd() || newmag == mOtherMagInd) {
    *pResult = 1;
    return;
  }

  mOtherMagInd = newmag;
  m_strOtherMag.Format("%d", MagForCamera(mWinApp->GetCurrentCamera(), mOtherMagInd));
  UpdateData(false);
  ManageEnables();
  *pResult = 0;
}

// Defocus offset has changed
void CStepAdjustISDlg::OnDeltaposSpinDefOffset(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newval, delta = 1;
  bool less = pNMUpDown->iDelta > 0;
  if (B3DABS(mDefocusOffset) >= B3DCHOICE(less, 110, 100))
    delta = 10;
  else if (B3DABS(mDefocusOffset) >= B3DCHOICE(pNMUpDown->iDelta < 0, 55, 50))
    delta = 5;
  else if (B3DABS(mDefocusOffset) >= B3DCHOICE(pNMUpDown->iDelta < 0, 22, 20))
    delta = 2;

  newval = -delta * ((-(mDefocusOffset - delta * pNMUpDown->iDelta)) / delta);
  mWinApp->RestoreViewFocus();
  *pResult = 1;
  if (newval < -400 || newval> 0)
    return;
  mDefocusOffset = newval;
  m_strDefOffset.Format("%d", newval);
  UpdateData(false);
  *pResult = 0;
}

// Checkbox to use defocus offset
void CStepAdjustISDlg::OnCheckSetDefOffset()
{
  UpdateData(true);
  ManageEnables();
}

// All-importany enabling
void CStepAdjustISDlg::ManageEnables()
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CString str;
  bool prevOK, searchOK = ldp[SEARCH_AREA].magIndex >= mLowestM;
  bool viewOK = ldp[VIEW_CONSET].magIndex >= mLowestM;
  bool autoEnabled = false, methodEnabled = false;
  int area = m_iAreaToUse, magType = m_iMagTypeToUse, cropSize;
  float min2x3Aspect = 1.33f;
  int ldArea, camera = mWinApp->GetCurrentCamera();
  CameraParameters *camP = mWinApp->GetActiveCamParam();
  float aspect = (float)camP->sizeX / camP->sizeY;
  int vDiff = B3DABS(mPrevMag - ldp[VIEW_CONSET].magIndex);
  int rDiff = B3DABS(mPrevMag - ldp[RECORD_CONSET].magIndex);
  int sDiff = B3DABS(mPrevMag - ldp[SEARCH_AREA].magIndex);
  if (aspect < 1.)
    aspect = 1.f / aspect;
  if (!m_iAreaToUse && !searchOK)
    m_iAreaToUse = viewOK ? 1 : 2;
  if (m_iAreaToUse == 1 && !viewOK)
    m_iAreaToUse = searchOK ? 0 : 2;
  ShowDlgItem(IDC_STAA_2X3MONTAGE, aspect >= min2x3Aspect);
  if (camP->sizeY > camP->sizeX)
    SetDlgItemText(IDC_STAA_2X3MONTAGE, "3 x 2 Preview montage");
  if (aspect < min2x3Aspect && m_iAreaToUse > 3)
    m_iAreaToUse = 3;

  if (!m_iAreaToUse)
    prevOK = vDiff < rDiff;
  else if (m_iAreaToUse == 1)
    prevOK = sDiff < rDiff;
  else
    prevOK = rDiff < B3DMIN(sDiff, vDiff);
  if (!prevOK && magType == 1)
    m_iMagTypeToUse = 0;
  ldArea = SEARCH_AREA;
  if (m_iAreaToUse)
    ldArea = m_iAreaToUse > 1 ? RECORD_CONSET : VIEW_CONSET;
  str.Format("Current mag of area, %dx", MagForCamera(camera, ldp[ldArea].magIndex));
  SetDlgItemText(IDC_STAA_CURRENT_MAG, str);
  EnableDlgItem(IDC_CHECK_SET_DEF_OFFSET, m_iAreaToUse < 2);
  m_sbcDefOffset.EnableWindow(m_bSetDefOffset && m_iAreaToUse < 2);
  EnableDlgItem(IDC_STAT_DEF_OFFSET, m_bSetDefOffset && m_iAreaToUse < 2);
  EnableDlgItem(IDC_STAA_VIEW, viewOK);
  EnableDlgItem(IDC_STAA_SEARCH, searchOK);
  EnableDlgItem(IDC_STAA_PREV_MAG, prevOK);
  m_statOtherMag.EnableWindow(m_iMagTypeToUse == 2);
  m_sbcOtherMag.EnableWindow(m_iMagTypeToUse == 2);
  autoEnabled = CMultiShotDlg::CanDoAutoAdjust(m_iMagTypeToUse, m_iAreaToUse, mOtherMagInd
    , mPrevMag, m_fHoleSize, mHexGrid, mUseCustom, cropSize, str);
  methodEnabled = autoEnabled && m_bDoAutomatic;
  EnableDlgItem(IDC_STAA_DO_AUTOMATIC, autoEnabled);
  EnableDlgItem(IDC_STAA_CROSSCORR, methodEnabled);
  EnableDlgItem(IDC_STAA_CENTER_HOLE, methodEnabled);
  EnableDlgItem(IDC_CHECK_SET_PREV_EXP, m_iAreaToUse > 1);
  EnableDlgItem(IDC_EDIT_PREVIEW_EXP, m_iAreaToUse > 1);
  EnableDlgItem(IDC_STAT_PREV_SEC, m_iAreaToUse > 1);
  EnableDlgItem(IDC_STAT_LIMIT_FRAC, methodEnabled);
  EnableDlgItem(IDC_EDIT_STAA_LIMIT, methodEnabled);
  EnableDlgItem(IDC_STAT_MICRONS, !mParam->useCustomHoles);
  EnableDlgItem(IDC_EDIT_STAA_HOLE, !mParam->useCustomHoles);
  EnableDlgItem(IDC_STAT_HOLE_SIZE, !mParam->useCustomHoles);
  EnableDlgItem(IDC_CHECK_IMAGE_AFTER_IS, !methodEnabled);
  SetDlgItemText(IDC_STAT_MAG_INFO_LINE, str);
  if (magType != m_iMagTypeToUse || area != m_iAreaToUse)
    UpdateData(false);
}
