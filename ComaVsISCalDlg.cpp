// ComaVsISCalDlg.cpp : Dialog for setting rotation and distance in Coma Vs. IS cal and
// starting the cal
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "CameraController.h"
#include "ComaVsISCalDlg.h"
#include "NavHelper.h"
#include "AutoTuning.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#define MIN_EXTENT 0.f
#define MAX_EXTENT 18.f
#define EXTENT_INCREMENT 0.2f
#define ROTATION_INCREMENT 5
#define MAX_ROTATION 50

// CComaVsISCalDlg dialog

CComaVsISCalDlg::CComaVsISCalDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(IDD_COMA_VS_IS_CAL, pParent)
  , m_fDistance(0.5f)
  , m_iRotation(0)
  , m_iNumImages(FALSE)
{
  mNonModal = true;
}

CComaVsISCalDlg::~CComaVsISCalDlg()
{
}

void CComaVsISCalDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_SPIN_CVIC_DISTANCE, m_sbcDistance);
  DDX_Control(pDX, IDC_EDIT_CVIC_DISTANCE, m_editDistance);
  DDX_Text(pDX, IDC_EDIT_CVIC_DISTANCE, m_fDistance);
  MinMaxFloat(IDC_EDIT_CVIC_DISTANCE, m_fDistance, MIN_COMA_VS_IS_EXTENT,
    MAX_COMA_VS_IS_EXTENT, "distance at which to measure coma");
  DDX_Control(pDX, IDC_EDIT_CVIC_ROTATION, m_editRotation);
  DDX_Text(pDX, IDC_EDIT_CVIC_ROTATION, m_iRotation);
  MinMaxInt(IDC_EDIT_CVIC_ROTATION, m_iRotation, -MAX_ROTATION, MAX_ROTATION,
    "rotation of vectors");
  DDX_Control(pDX, IDC_SPIN_CVIC_ROTATION, m_sbcRotation);
  DDX_Control(pDX, IDOK, m_butCalibrate);
  DDX_Radio(pDX, IDC_RUSE_5IMAGES, m_iNumImages);
}


BEGIN_MESSAGE_MAP(CComaVsISCalDlg, CBaseDlg)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CVIC_DISTANCE, OnDeltaposSpinDistance)
  ON_EN_KILLFOCUS(IDC_EDIT_CVIC_DISTANCE, OnKillfocusEditDistance)
  ON_EN_KILLFOCUS(IDC_EDIT_CVIC_ROTATION, OnKillfocusEditRotation)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_CVIC_ROTATION, OnDeltaposSpinRotation)
  ON_BN_CLICKED(IDC_RUSE_5IMAGES, OnSetNumImages)
END_MESSAGE_MAP()


// CComaVsISCalDlg message handlers
// Initialize dialog
BOOL CComaVsISCalDlg::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  m_fDistance = mWinApp->mAutoTuning->GetComaVsISextent();
  m_iRotation = mWinApp->mAutoTuning->GetComaVsISrotation();
  m_iNumImages = mWinApp->mAutoTuning->GetComaVsISuseFullArray();
  if (m_iNumImages < 0)
    m_iNumImages = 2;
  SetDlgItemText(IDC_STAT_CUR_SETTING, mWinApp->mAutoTuning->GetCtfDoFullArray() ?
    "(currently 9)" : "(currently 5)");
  m_sbcDistance.SetRange(0, 10000);
  m_sbcDistance.SetPos(5000);
  m_sbcRotation.SetRange(0, 10000);
  m_sbcRotation.SetPos(5000);
  if (mWinApp->mNavHelper->GetSkipAstigAdjustment() < 0) {
    EnableDlgItem(IDC_RUSE_5IMAGES, false);
    EnableDlgItem(IDC_RUSE_FULL_ARRAY, false);
    EnableDlgItem(IDC_RUSE_SETTING, false);
  }
  UpdateData(false);
  mWinApp->mMainView->DrawImage();
  SetDefID(45678);    // Disable OK from being default button for non-modal
  return TRUE;
}

void CComaVsISCalDlg::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

// OK, Cancel, handling returns in text fields
void CComaVsISCalDlg::OnOK()
{
  int useFull;
  UpdateData(true);
  useFull = m_iNumImages > 1 ? -1 : m_iNumImages;
  mWinApp->mAutoTuning->SetComaVsISextent(m_fDistance);
  mWinApp->mAutoTuning->SetComaVsISrotation(m_iRotation);
  mWinApp->mAutoTuning->SetComaVsISuseFullArray(useFull);
  EnableDlgItem(IDOK, false);
  EnableDlgItem(IDCANCEL, false);
  mWinApp->mAutoTuning->CalibrateComaVsImageShift(m_fDistance, m_iRotation, useFull);
  OnCancel();
}

void CComaVsISCalDlg::OnCancel()
{
  mWinApp->mNavHelper->GetComaVsISDlgPlacement();
  mWinApp->mNavHelper->mComaVsISCalDlg = NULL;
  DestroyWindow();
}

BOOL CComaVsISCalDlg::PreTranslateMessage(MSG* pMsg)
{
  //PrintfToLog("message %d  keydown %d  param %d return %d", pMsg->message, WM_KEYDOWN, pMsg->wParam, VK_RETURN);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

void CComaVsISCalDlg::UpdateEnables()
{
  EnableDlgItem(IDOK, !mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy());
}

// Distance spin button
void CComaVsISCalDlg::OnDeltaposSpinDistance(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  mWinApp->RestoreViewFocus();
  if ((pNMUpDown->iDelta < 0 && m_fDistance <= MIN_COMA_VS_IS_EXTENT + 0.001) ||
    (pNMUpDown->iDelta > 0 && m_fDistance >= MAX_COMA_VS_IS_EXTENT - 0.001)) {
    *pResult = 1;
    return;
  }
  m_fDistance += (float)pNMUpDown->iDelta * EXTENT_INCREMENT;
  B3DCLAMP(m_fDistance, MIN_COMA_VS_IS_EXTENT, MAX_COMA_VS_IS_EXTENT);
  mWinApp->mMainView->DrawImage();
  UpdateData(false);
  *pResult = 0;
}

// Distance text box
void CComaVsISCalDlg::OnKillfocusEditDistance()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}

// Rotation text box
void CComaVsISCalDlg::OnKillfocusEditRotation()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  mWinApp->mMainView->DrawImage();
}

// Rotation spin button
void CComaVsISCalDlg::OnDeltaposSpinRotation(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, m_iRotation, -MAX_ROTATION, MAX_ROTATION, newVal))
    return;
  m_iRotation += ROTATION_INCREMENT * (newVal - m_iRotation);
  B3DCLAMP(m_iRotation, -MAX_ROTATION, MAX_ROTATION);
  UpdateData(false);
  mWinApp->mMainView->DrawImage();
}

// Choice for # of images
void CComaVsISCalDlg::OnSetNumImages()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}
