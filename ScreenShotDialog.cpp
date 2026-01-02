// ScreenShotDialog.cpp : Take and saves a screen shot or zoomed shot with user settings
//
// Copyright (C) 2020-2026 by  the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "ChildFrm.h"
#include "ScreenShotDialog.h"
#include "NavHelper.h"
#include "EMbufferManager.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#define MAX_SCALING 8
#define MIN_JPEG_QUALITY 20

// CScreenShotDialog dialog

CScreenShotDialog::CScreenShotDialog(CWnd* pParent /*=NULL*/)
  : CBaseDlg(IDD_SCREENSHOT, pParent)
  , m_iImageScaleType(0)
  , m_strScaleBy(_T(""))
  , m_bScaleSizes(FALSE)
  , m_strScaleSizesBy(_T(""))
  , m_bDrawOverlays(FALSE)
  , m_iFileType(0)
  , m_iCompression(0)
  , m_iJpegQuality(MIN_JPEG_QUALITY)
{
  mNonModal = true;
}

CScreenShotDialog::~CScreenShotDialog()
{
}

BOOL CScreenShotDialog::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    SetFocus();
  return CDialog::PreTranslateMessage(pMsg);
}

void CScreenShotDialog::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Radio(pDX, IDC_RADIO_UNZOOMED, m_iImageScaleType);
  DDX_Control(pDX, IDC_STAT_SCALEBY, m_statScaleBy);
  DDX_Text(pDX, IDC_STAT_SCALEBY, m_strScaleBy);
  DDX_Control(pDX, IDC_SPIN_INCREASE_ZOOM_BY, m_sbcIncreaseZoom);
  DDX_Check(pDX, IDC_CHECK_SCALE_SIZE_BY, m_bScaleSizes);
  DDX_Control(pDX, IDC_STAT_SCALE_SIZES, m_statScaleSizesBy);
  DDX_Text(pDX, IDC_STAT_SCALE_SIZES, m_strScaleSizesBy);
  DDX_Control(pDX, IDC_SPIN_SCALE_SIZE_BY, m_sbcScaleSizesBy);
  DDX_Check(pDX, IDC_CHECK_DRAW_OVERLAYS, m_bDrawOverlays);
  DDX_Radio(pDX, IDC_RADIO_AS_TIFF, m_iFileType);
  DDX_Radio(pDX, IDC_RNOCOMPRESS, m_iCompression);
  DDX_Control(pDX, IDC_STATCOMPRESS, m_statCompress);
  DDX_Control(pDX, IDC_RNOCOMPRESS, m_butNoCompress);
  DDX_Control(pDX, IDC_RZIPCOMPRESS, m_butZipCompress);
  DDX_Control(pDX, IDC_RLZWCOMPRESS, m_butLZWCompress);
  DDX_Control(pDX, IDC_RJPEGCOMPRESS, m_butJpegCompress);
  DDX_Control(pDX, IDC_EDIT_JPEG_QUALITY, m_editJpegQuality);
  DDX_Text(pDX, IDC_EDIT_JPEG_QUALITY, m_iJpegQuality);
  MinMaxInt(IDC_EDIT_JPEG_QUALITY, m_iJpegQuality, MIN_JPEG_QUALITY, 100, "JPEG quality");
  DDX_Control(pDX, IDC_SPIN_JPEG_QUALITY, m_sbcJpegQuality);
  DDX_Control(pDX, IDC_STAT_JPEG_QUALITY, m_statJpegQuality);
  DDX_Control(pDX, ID_TAKE_TIFF, m_butTakeSnapshot);
}

BEGIN_MESSAGE_MAP(CScreenShotDialog, CBaseDlg)

  ON_BN_CLICKED(IDC_RADIO_UNZOOMED, OnImageScaling)
  ON_BN_CLICKED(IDC_RADIO_INCREASE_BY, OnImageScaling)
  ON_BN_CLICKED(IDC_RADIO_WHOLE_IMAGE, OnImageScaling)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SCALE_SIZE_BY, OnDeltaposSpinScaleSizeBy)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_INCREASE_ZOOM_BY, OnDeltaposSpinIncreaseZoomBy)
  ON_BN_CLICKED(IDC_RADIO_AS_TIFF, OnFileType)
  ON_BN_CLICKED(IDC_RADIO_AS_JPEG, OnFileType)
  ON_BN_CLICKED(ID_TAKE_TIFF, OnTakeSnapshot)
  ON_BN_CLICKED(IDC_CHECK_SCALE_SIZE_BY, OnCheckScaleSizeBy)
  ON_BN_CLICKED(IDC_CHECK_DRAW_OVERLAYS, OnCheckDrawOverlays)
  ON_BN_CLICKED(IDC_RNOCOMPRESS, OnRnocompress)
  ON_BN_CLICKED(IDC_RZIPCOMPRESS, OnRnocompress)
  ON_BN_CLICKED(IDC_RLZWCOMPRESS, OnRnocompress)
  ON_BN_CLICKED(IDC_RJPEGCOMPRESS, OnRnocompress)
  ON_EN_KILLFOCUS(IDC_EDIT_JPEG_QUALITY, OnEnKillfocusJpegQuality)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_JPEG_QUALITY, OnDeltaposSpinJpegQuality)
END_MESSAGE_MAP()


// CScreenShotDialog message handlers

BOOL CScreenShotDialog::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  mMasterParams = mWinApp->GetScreenShotParams();
  mParams = *mMasterParams;
  m_sbcIncreaseZoom.SetRange(1, MAX_SCALING);
  m_sbcScaleSizesBy.SetRange(0, MAX_SCALING);
  m_sbcJpegQuality.SetRange(MIN_JPEG_QUALITY, 100);
  ParamsToDialog();
  UpdateActiveView();
  ManageEnables();

  SetDefID(45678);    // Disable OK from being default button
  return TRUE;
}

void CScreenShotDialog::OnOK()
{
  UpdateData(true);
  DialogToParams();
  *mMasterParams = mParams;
  OnCancel();
}

void CScreenShotDialog::OnCancel()
{
  mWinApp->GetScreenShotPlacement();
  mWinApp->mScreenShotDialog = NULL;
  DestroyWindow();
}

void CScreenShotDialog::PostNcDestroy()
{
  delete this;
  CDialog::PostNcDestroy();
}

void CScreenShotDialog::UpdateSettings()
{
  mParams = *mMasterParams;
  ParamsToDialog();
  ManageEnables();
}

void CScreenShotDialog::UpdateActiveView()
{
  CString title;;
  CChildFrame *parent = (CChildFrame *)(mWinApp->mActiveView->GetParent());
  parent->GetWindowTextA(title);
  SetDlgItemText(IDC_STAT_ACTIVE_VIEW, "Active window: " + title);
}

void CScreenShotDialog::UpdateRunnable()
{
  m_butTakeSnapshot.EnableWindow(!mWinApp->DoingTasks());
}

void CScreenShotDialog::OnImageScaling()
{
  UpdateData(true);
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

// Image/Size scaling spinners: translate 1 to a 1.5
void CScreenShotDialog::OnDeltaposSpinScaleSizeBy(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, 0, MAX_SCALING, newVal))
    return;
  if (newVal == 1)
    mParams.sizeScaling = 1.5;
  else
    mParams.sizeScaling = (float)newVal;
  ShowSpinnerValue(m_strScaleSizesBy, mParams.sizeScaling);
}


void CScreenShotDialog::OnDeltaposSpinIncreaseZoomBy(NMHDR *pNMHDR, LRESULT *pResult)
{
  int newVal;
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, 1, MAX_SCALING, newVal))
    return;
  if (newVal == 1)
    mParams.imageScaling = 1.5;
  else
    mParams.imageScaling = (float)newVal;
  ShowSpinnerValue(m_strScaleBy, mParams.imageScaling);
}

void CScreenShotDialog::OnFileType()
{
  UpdateData(true);
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

void CScreenShotDialog::OnCheckScaleSizeBy()
{
  UpdateData(true);
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

void CScreenShotDialog::OnCheckDrawOverlays()
{
  mWinApp->RestoreViewFocus();
}

void CScreenShotDialog::OnRnocompress()
{
  UpdateData(true);
  ManageEnables();
  mWinApp->RestoreViewFocus();
}

void CScreenShotDialog::OnEnKillfocusJpegQuality()
{
  UpdateData(true);
  m_sbcJpegQuality.SetPos(m_iJpegQuality);
  mWinApp->RestoreViewFocus();
}

void CScreenShotDialog::OnDeltaposSpinJpegQuality(NMHDR *pNMHDR, LRESULT *pResult)
{
  mWinApp->RestoreViewFocus();
  if (NewSpinnerValue(pNMHDR, pResult, MIN_JPEG_QUALITY, 100, m_iJpegQuality))
    return;
  UpdateData(false);
}

// Take the snapshot with the current parameters and assign them to master
void CScreenShotDialog::OnTakeSnapshot()
{
  float imScale = 1., sizeScale = 1.;
  int storeType, err;
  int compressions[] = {COMPRESS_NONE, COMPRESS_ZIP, COMPRESS_LZW, COMPRESS_JPEG};
  CString root, ext, newName = mParams.lastFilePath;
  mWinApp->RestoreViewFocus();
  UpdateData(true);
  DialogToParams();
  *mMasterParams = mParams;
  B3DCLAMP(mParams.compression, 0, 3);
  storeType = mParams.fileType ? STORE_TYPE_JPEG : STORE_TYPE_TIFF;
  if (!newName.IsEmpty()) {
    UtilSplitExtension(newName, root, ext);
    if ((!ext.CompareNoCase(".tif") || !ext.CompareNoCase(".tiff")) && mParams.fileType)
      newName = root + ".jpg";
    else if ((!ext.CompareNoCase(".jpg") || !ext.CompareNoCase(".jpeg")) &&
      !mParams.fileType)
      newName = root + ".tif";
    newName = mWinApp->mNavHelper->NextAutoFilename(newName);
  }
  if (mWinApp->mDocWnd->FilenameForSaveFile(storeType,
    newName.IsEmpty() ? NULL : (LPCTSTR)newName, mParams.lastFilePath))
    return;
  if (mParams.imageScaleType)
    imScale = mParams.imageScaleType == 1 ? mParams.imageScaling : -1.f;
  if (mParams.ifScaleSizes)
    sizeScale = mParams.sizeScaling;
  err = mWinApp->mActiveView->TakeSnapshot(imScale, sizeScale, mParams.skipOverlays,
    storeType, compressions[mParams.compression], m_iJpegQuality, mParams.lastFilePath);
  if (GetSnapshotError(err, root))
    SEMMessageBox("Error taking image snapshot: " + root);
}


#define NUM_MESSAGES 6
static char *messages[NUM_MESSAGES] = {
  "No image in buffer", "Not enough memory available for a snapshot this large",
  "Call to get information needed to take snapshot failed",
  "Cannot take a snapshot with data in the current graphics format",
  "Taking a snapshot with an image scaled up this large is not supported; try "
  "using \"Whole image at 1:1 zoom\"", "Unknown error"
};

bool CScreenShotDialog::GetSnapshotError(int err, CString &report)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!err)
    return false;
  if (err < 0) {
    report = winApp->mBufferManager->ComposeErrorMessage(-err, "");
  } else {
    B3DCLAMP(err, 1, NUM_MESSAGES);
    report = messages[err - 1];
  }
  return true;
}

void CScreenShotDialog::DialogToParams()
{
  mParams.fileType = m_iFileType;
  mParams.compression = m_iCompression;
  mParams.imageScaleType = m_iImageScaleType;
  mParams.skipOverlays = m_bDrawOverlays ? 0 : 3;
  mParams.ifScaleSizes = m_bScaleSizes;
}

void CScreenShotDialog::ParamsToDialog()
{
  m_iFileType = mParams.fileType;
  m_iCompression = mParams.compression;
  m_iImageScaleType = mParams.imageScaleType;
  m_bDrawOverlays = mParams.skipOverlays == 0;
  m_bScaleSizes = mParams.ifScaleSizes;
  m_iJpegQuality = mParams.jpegQuality;
  FixScaleSetSpinner(&m_sbcIncreaseZoom, mParams.imageScaling, 1.);
  ShowSpinnerValue(m_strScaleBy, mParams.imageScaling);
  FixScaleSetSpinner(&m_sbcScaleSizesBy, mParams.sizeScaling, 0.);
  ShowSpinnerValue(m_strScaleSizesBy, mParams.sizeScaling);
  m_sbcJpegQuality.SetPos(m_iJpegQuality);
}

void CScreenShotDialog::FixScaleSetSpinner(CSpinButtonCtrl *sbc, float &scale, float minVal)
{
  int spinVal;
  scale = B3DMAX(minVal, scale);
  spinVal = B3DNINT(scale);
  if (scale < 1.75 && scale > 0.5) {
    scale = 1.5;
    spinVal = 1;
  } else
    scale = (float)spinVal;
  sbc->SetPos(spinVal);
}

void CScreenShotDialog::ShowSpinnerValue(CString &str, float scale)
{
  if (scale < 1)
    str = "Scaling";
  else if (scale < 1.75)
    str = "1.5";
  else
    str.Format("%d", B3DNINT(scale));
  UpdateData(false);
}

void CScreenShotDialog::ManageEnables()
{
  m_statScaleBy.EnableWindow(m_iImageScaleType == 1);
  m_sbcIncreaseZoom.EnableWindow(m_iImageScaleType == 1);
  m_statScaleSizesBy.EnableWindow(m_bScaleSizes);
  m_sbcScaleSizesBy.EnableWindow(m_bScaleSizes);
  m_statCompress.EnableWindow(!m_iFileType);
  m_butNoCompress.EnableWindow(!m_iFileType);
  m_butZipCompress.EnableWindow(!m_iFileType);
  m_butLZWCompress.EnableWindow(!m_iFileType);
  m_butJpegCompress.EnableWindow(!m_iFileType);
  m_statJpegQuality.EnableWindow(m_iFileType || m_iCompression == 3);
  m_editJpegQuality.EnableWindow(m_iFileType || m_iCompression == 3);
  m_sbcJpegQuality.EnableWindow(m_iFileType || m_iCompression == 3);
}
