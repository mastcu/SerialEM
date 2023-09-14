// StepAdjustISDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "NavHelper.h"
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
}


BEGIN_MESSAGE_MAP(CStepAdjustISDlg, CDialog)
  ON_BN_CLICKED(IDC_STAA_SEARCH, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_VIEW, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_RECORD, OnAreaToUse)
  ON_BN_CLICKED(IDC_STAA_CURRENT_MAG, OnMagTypeToUse)
  ON_BN_CLICKED(IDC_STAA_PREV_MAG, OnMagTypeToUse)
  ON_BN_CLICKED(IDC_STAA_OTHER_MAG, OnMagTypeToUse)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_OTHER_MAG, OnDeltaposSpinOtherMag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_DEF_OFFSET, OnDeltaposSpinDefOffset)
  ON_BN_CLICKED(IDC_CHECK_SET_DEF_OFFSET, OnCheckSetDefOffset)
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
  m_bSetDefOffset = mParam->stepAdjSetDefOff;
  m_bImageAfterIS = mParam->stepAdjTakeImage;
  m_strOtherMag.Format("%d", MagForCamera(mWinApp->GetCurrentCamera(), mOtherMagInd));
  m_sbcOtherMag.SetRange(0, 10000);
  m_sbcOtherMag.SetPos(5000);
  m_sbcDefOffset.SetRange(0, 10000);
  m_sbcDefOffset.SetPos(5000);
  m_strDefOffset.Format("%d", mDefocusOffset);
  str.Format("Mag where shifts were recorded, %dx", 
    MagForCamera(mWinApp->GetCurrentCamera(), mPrevMag));
  SetDlgItemText(IDC_STAA_PREV_MAG, str);
  ManageEnables();
  UpdateData(false);
  return TRUE;
}

void CStepAdjustISDlg::OnOK()
{
  UpdateData(true);
  mParam->stepAdjDefOffset = mDefocusOffset;
  mParam->stepAdjOtherMag = mOtherMagInd;
  mParam->stepAdjLDarea = m_iAreaToUse;
  mParam->stepAdjWhichMag = m_iMagTypeToUse;
  mParam->stepAdjSetDefOff = m_bSetDefOffset;
  mParam->stepAdjTakeImage = m_bImageAfterIS;
  CBaseDlg::OnOK();
}

void CStepAdjustISDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}


void CStepAdjustISDlg::OnAreaToUse()
{
  UpdateData(true);
  ManageEnables();
}


void CStepAdjustISDlg::OnMagTypeToUse()
{
  UpdateData(true);
  ManageEnables();
}


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
  *pResult = 0;
}


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


void CStepAdjustISDlg::OnCheckSetDefOffset()
{
  UpdateData(true);
  ManageEnables();
}

void CStepAdjustISDlg::ManageEnables()
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CString str;
  bool prevOK, searchOK = ldp[SEARCH_AREA].magIndex >= mLowestM;
  bool viewOK = ldp[VIEW_CONSET].magIndex >= mLowestM;
  int area = m_iAreaToUse, magType = m_iMagTypeToUse;
  int ldArea;
  int vDiff = B3DABS(mPrevMag - ldp[VIEW_CONSET].magIndex);
  int rDiff = B3DABS(mPrevMag - ldp[RECORD_CONSET].magIndex);
  int sDiff = B3DABS(mPrevMag - ldp[SEARCH_AREA].magIndex);
  if (!m_iAreaToUse && !searchOK)
    m_iAreaToUse = viewOK ? 1 : 2;
  if (m_iAreaToUse == 1 && !viewOK)
    m_iAreaToUse = searchOK ? 0 : 2;
  if (!m_iAreaToUse)
    prevOK = vDiff < rDiff;
  else if (m_iAreaToUse == 1)
    prevOK = sDiff < rDiff;
  else
    prevOK = rDiff < B3DMIN(sDiff, vDiff);
  if (!prevOK && magType == 1)
    m_iMagTypeToUse = 0;
  ldArea = SEARCH_AREA;
  if (area)
    ldArea = area > 1 ? RECORD_CONSET : VIEW_CONSET;
  str.Format("Current mag of area, %dx", 
    MagForCamera(mWinApp->GetCurrentCamera(), ldp[ldArea].magIndex));
  SetDlgItemText(IDC_STAA_CURRENT_MAG, str);
  EnableDlgItem(IDC_CHECK_SET_DEF_OFFSET, m_iAreaToUse < 2);
  m_sbcDefOffset.EnableWindow(m_bSetDefOffset && m_iAreaToUse < 2);
  EnableDlgItem(IDC_STAT_DEF_OFFSET, m_bSetDefOffset && m_iAreaToUse < 2);
  EnableDlgItem(IDC_STAA_VIEW, viewOK);
  EnableDlgItem(IDC_STAA_SEARCH, searchOK);
  EnableDlgItem(IDC_STAA_PREV_MAG, prevOK);
  m_statOtherMag.EnableWindow(m_iMagTypeToUse == 2);
  m_sbcOtherMag.EnableWindow(m_iMagTypeToUse == 2);
  if (magType != m_iMagTypeToUse || area != m_iAreaToUse)
    UpdateData(false);
}
