// K2SaveOptionDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "K2SaveOptionDlg.h"
#include "CameraController.h"
#include "DirectElectron\DirectElectronCamera.h"


// CK2SaveOptionDlg dialog

static int idTable[] = {IDC_STAT_FILETYPE, IDC_SAVE_MRC, IDC_COMPRESSED_TIFF, 
  IDC_TIFF_ZIP_COMPRESS, IDC_STAT_CURSET2, IDC_STAT_CURSET1,
  IDC_USE_EXTENSION_MRCS, PANEL_END,
  IDC_ONE_FRAME_PER_FILE, IDC_FRAME_STACK_MDOC, IDC_TSS_LINE2, PANEL_END,
  IDC_SAVE_UNNORMALIZED, IDC_PACK_COUNTING_4BIT, IDC_USE_4BIT_MRC_MODE, 
  IDC_SAVE_TIMES_100, IDC_PACK_RAW_FRAME, IDC_CHECK_REDUCE_SUPERRES, PANEL_END, 
  IDC_SKIP_ROTFLIP, IDC_TSS_LINE1, PANEL_END, 
  IDC_STAT_COMPTITLE, IDC_STAT_USE_COMP, IDC_STAT_FOLDER, IDC_STAT_FILE,
  IDC_CHECK_ROOT_FOLDER, IDC_EDIT_BASENAME, IDC_CHECK_ROOT_FILE,
  IDC_CHECK_SAVEFILE_FILE, IDC_CHECK_SAVEFILE_FOLDER, IDC_CHECK_NAVLABEL_FOLDER, 
  IDC_CHECK_NAVLABEL_FILE, IDC_CHECK_NUMBER, IDC_EDIT_START_NUMBER, IDC_STAT_DIGITS,
  IDC_SPIN_DIGITS, IDC_CHECK_ONLY_ACQUIRE, IDC_CHECK_TILT_ANGLE, PANEL_END,
  IDC_CHECK_MONTHDAY, IDC_CHECK_HOUR_MIN_SEC, IDC_CHECK_DATE_PREFIX, PANEL_END, 
  IDC_CHECK_HOLE_POS, IDC_STAT_EXAMPLE, IDC_STAT_MUST_EXIST, IDC_STAT_MUST_EXIST2,
  PANEL_END,
  IDOK, IDCANCEL, IDC_BUTHELP, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];

CK2SaveOptionDlg::CK2SaveOptionDlg(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CK2SaveOptionDlg::IDD, pParent)
  , m_bOneFramePerFile(FALSE)
  , m_iFileType(0)
  , m_bPackRawFrames(FALSE)
  , m_bSkipRotFlip(FALSE)
  , m_strBasename(_T(""))
  , m_bRootFolder(FALSE)
  , m_bSavefileFolder(FALSE)
  , m_bNavLabelFolder(FALSE)
  , m_bNumber(FALSE)
  , m_bMonthDay(FALSE)
  , m_bHourMinSec(FALSE)
  , m_strExample(_T(""))
  , m_iStartNumber(0)
  , m_bOnlyWhenAcquire(FALSE)
  , m_bTiltAngle(FALSE)
  , m_strDigits(_T(""))
  , m_bPackCounting4Bit(FALSE)
  , m_bUse4BitMode(FALSE)
  , m_bSaveTimes100(FALSE)
  , m_bUseExtensionMRCS(FALSE)
  , m_bSaveUnnormalized(FALSE)
  , m_bReduceSuperres(FALSE)
  , m_strCurSetSaves(_T(""))
  , m_bSaveFrameStackMdoc(FALSE)
  , m_bDatePrefix(FALSE)
  , m_bMultiHolePos(FALSE)
{
  m_bRootFile = false;
  m_bSavefileFile = false;
}

CK2SaveOptionDlg::~CK2SaveOptionDlg()
{
}

void CK2SaveOptionDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_ONE_FRAME_PER_FILE, m_bOneFramePerFile);
  DDX_Radio(pDX, IDC_SAVE_MRC, m_iFileType);
  DDX_Check(pDX, IDC_PACK_RAW_FRAME, m_bPackRawFrames);
  DDX_Control(pDX, IDC_SKIP_ROTFLIP, m_butSkipRotFlip);
  DDX_Check(pDX, IDC_SKIP_ROTFLIP, m_bSkipRotFlip);
  DDX_Text(pDX, IDC_EDIT_BASENAME, m_strBasename);
  DDV_MaxChars(pDX, m_strBasename, 40);
  DDX_Check(pDX, IDC_CHECK_ROOT_FOLDER, m_bRootFolder);
  DDX_Check(pDX, IDC_CHECK_SAVEFILE_FOLDER, m_bSavefileFolder);
  DDX_Check(pDX, IDC_CHECK_NAVLABEL_FOLDER, m_bNavLabelFolder);
  DDX_Check(pDX, IDC_CHECK_ROOT_FILE, m_bRootFile);
  DDX_Check(pDX, IDC_CHECK_SAVEFILE_FILE, m_bSavefileFile);
  DDX_Check(pDX, IDC_CHECK_NAVLABEL_FILE, m_bNavLabelFile);
  DDX_Check(pDX, IDC_CHECK_NUMBER, m_bNumber);
  DDX_Check(pDX, IDC_CHECK_MONTHDAY, m_bMonthDay);
  DDX_Check(pDX, IDC_CHECK_HOUR_MIN_SEC, m_bHourMinSec);
  DDX_Text(pDX, IDC_STAT_EXAMPLE, m_strExample);
  DDX_Control(pDX, IDC_STAT_MUST_EXIST, m_statMustExist);
  DDX_Control(pDX, IDC_STAT_MUST_EXIST2, m_statMustExist2);
  DDX_Text(pDX, IDC_EDIT_START_NUMBER, m_iStartNumber);
  DDV_MinMaxInt(pDX, m_iStartNumber, 0, 9999);
  DDX_Check(pDX, IDC_CHECK_ONLY_ACQUIRE, m_bOnlyWhenAcquire);
  DDX_Check(pDX, IDC_CHECK_TILT_ANGLE, m_bTiltAngle);
  DDX_Control(pDX, IDC_CHECK_ROOT_FOLDER, m_butRootFolder);
  DDX_Control(pDX, IDC_CHECK_SAVEFILE_FOLDER, m_butSaveFileFolder);
  DDX_Control(pDX, IDC_CHECK_NAVLABEL_FOLDER, m_butNavlabelFolder);
  DDX_Control(pDX, IDC_CHECK_MONTHDAY, m_butMonthDay);
  DDX_Control(pDX, IDC_CHECK_HOUR_MIN_SEC, m_butHourMinSec);
  DDX_Control(pDX, IDC_STAT_USE_COMP, m_statUseComp);
  DDX_Control(pDX, IDC_STAT_FOLDER, m_statFolder);
  DDX_Control(pDX, IDC_STAT_FILE, m_statFile);
  DDX_Control(pDX, IDC_STAT_DIGITS, m_statDigits);
  DDX_Text(pDX, IDC_STAT_DIGITS, m_strDigits);
  DDX_Control(pDX, IDC_SPIN_DIGITS, m_sbcDigits);
  DDX_Control(pDX, IDC_PACK_COUNTING_4BIT, m_butPackCounting4Bits);
  DDX_Check(pDX, IDC_PACK_COUNTING_4BIT, m_bPackCounting4Bit);
  DDX_Control(pDX, IDC_USE_4BIT_MRC_MODE, m_butUse4BitMode);
  DDX_Check(pDX, IDC_USE_4BIT_MRC_MODE, m_bUse4BitMode);
  DDX_Check(pDX, IDC_SAVE_TIMES_100, m_bSaveTimes100);
  DDX_Control(pDX, IDC_USE_EXTENSION_MRCS, m_butUseExtensionMRCS);
  DDX_Check(pDX, IDC_USE_EXTENSION_MRCS, m_bUseExtensionMRCS);
  DDX_Control(pDX, IDC_FRAME_STACK_MDOC, m_butSaveFrameStackMdoc);
  DDX_Check(pDX, IDC_FRAME_STACK_MDOC, m_bSaveFrameStackMdoc);
  DDX_Control(pDX, IDC_SAVE_UNNORMALIZED, m_butSaveUnnormalized);
  DDX_Check(pDX, IDC_SAVE_UNNORMALIZED, m_bSaveUnnormalized);
  DDX_Control(pDX, IDC_PACK_RAW_FRAME, m_butPackRawFrame);
  DDX_Control(pDX, IDC_SAVE_TIMES_100, m_butSavesTimes100);
  DDX_Control(pDX, IDC_CHECK_REDUCE_SUPERRES, m_butReduceSuperres);
  DDX_Check(pDX, IDC_CHECK_REDUCE_SUPERRES, m_bReduceSuperres);
  DDX_Text(pDX, IDC_STAT_CURSET2, m_strCurSetSaves);
  DDX_Control(pDX, IDC_CHECK_DATE_PREFIX, m_butDatePrefix);
  DDX_Check(pDX, IDC_CHECK_DATE_PREFIX, m_bDatePrefix);
  DDX_Check(pDX, IDC_CHECK_HOLE_POS, m_bMultiHolePos);
}


BEGIN_MESSAGE_MAP(CK2SaveOptionDlg, CBaseDlg)
  ON_EN_KILLFOCUS(IDC_EDIT_BASENAME, OnKillfocusEditBasename)
  ON_BN_CLICKED(IDC_CHECK_ROOT_FOLDER, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_SAVEFILE_FOLDER, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_NAVLABEL_FOLDER, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_NAVLABEL_FILE, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_SAVEFILE_FILE, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_ROOT_FILE, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_NUMBER, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_TILT_ANGLE, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_MONTHDAY, OnCheckComponent)
  ON_BN_CLICKED(IDC_CHECK_HOUR_MIN_SEC, OnCheckComponent)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_DIGITS, OnDeltaposSpinDigits)
  ON_BN_CLICKED(IDC_PACK_RAW_FRAME, OnPackRawFrame)
  ON_BN_CLICKED(IDC_SAVE_MRC, OnSaveMrc)
  ON_BN_CLICKED(IDC_COMPRESSED_TIFF, OnSaveMrc)
  ON_BN_CLICKED(IDC_TIFF_ZIP_COMPRESS, OnSaveMrc)
  ON_BN_CLICKED(IDC_SAVE_UNNORMALIZED, OnSaveUnnormalized)
  ON_BN_CLICKED(IDC_CHECK_REDUCE_SUPERRES, OnReduceSuperres)
  ON_BN_CLICKED(IDC_CHECK_DATE_PREFIX, OnCheckDatePrefix)
  ON_BN_CLICKED(IDC_CHECK_HOLE_POS, OnCheckComponent)
END_MESSAGE_MAP()


// CK2SaveOptionDlg message handlers
BOOL CK2SaveOptionDlg::OnInitDialog()
{
  BOOL states[8] = {true, true, true, true, true, true, true, true};
  int show = (mDEtype && !mCanCreateDir) ? SW_HIDE : SW_SHOW;
  CString str;
  CBaseDlg::OnInitDialog();
  m_butSkipRotFlip.EnableWindow(mEnableSkipRotFlip);
  m_butSaveUnnormalized.EnableWindow(mCanGainNormSum);
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);
  states[0] = mCamParams->GatanCam;
  states[1] = mCanSaveFrameStackMdoc || mK2Type;
  states[2] = mK2Type;
  states[3] = mCamParams->GatanCam;
  states[5] = !mDEtype;
  if (mCamParams->FEItype == FALCON4_TYPE && mWinApp->mCamera->GetCanSaveEERformat() > 0) {
    SetDlgItemText(IDC_ONE_FRAME_PER_FILE, 
      "Save counting frames in EER file instead of MRC file");
    m_bOneFramePerFile = mWinApp->mCamera->GetSaveInEERformat();
  } else
    mIDsToDrop.push_back(IDC_ONE_FRAME_PER_FILE);
  AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 0);
  m_butSavesTimes100.ShowWindow(mK2Type == K2_SUMMIT);
  m_butPackCounting4Bits.ShowWindow(mK2Type == K2_SUMMIT || 
    (mK2Type == K3_TYPE && mWinApp->mCamera->CAN_PLUGIN_DO(CAN_BIN_K3_REF, mCamParams)));
  if (mK2Type == K3_TYPE) {
    m_butPackRawFrame.SetWindowText("Pack unnormalized data as 4-bit");
    ReplaceDlgItemText(IDC_PACK_COUNTING_4BIT, "Counting mode", "binned by 2");
  }
  m_bRootFolder = (mNameFormat & FRAME_FOLDER_ROOT) != 0;
  m_bSavefileFolder = (mNameFormat & FRAME_FOLDER_SAVEFILE) != 0;
  m_bNavLabelFolder = (mNameFormat & FRAME_FOLDER_NAVLABEL) != 0;
  m_bRootFile = (mNameFormat & FRAME_FILE_ROOT) != 0;
  m_bSavefileFile = (mNameFormat & FRAME_FILE_SAVEFILE) != 0;
  m_bNavLabelFile = (mNameFormat & FRAME_FILE_NAVLABEL) != 0;
  m_bNumber = (mNameFormat & FRAME_FILE_NUMBER) != 0;
  m_bTiltAngle = (mNameFormat & FRAME_FILE_TILT_ANGLE) != 0;
  m_bMonthDay = (mNameFormat & FRAME_FILE_MONTHDAY) != 0;
  m_bHourMinSec = (mNameFormat & FRAME_FILE_HOUR_MIN_SEC) || !m_bNumber;
  m_bDatePrefix = (mNameFormat & FRAME_FILE_DATE_PREFIX) != 0;
  m_bOnlyWhenAcquire = (mNameFormat & FRAME_LABEL_IF_ACQUIRE) != 0;
  m_bMultiHolePos = (mNameFormat & FRAME_FILE_HOLE_AND_POS) != 0;
  if (!mCanCreateDir && !mDEtype)
    SetDlgItemText(IDC_STAT_MUST_EXIST, B3DCHOICE(mFalconType,
    "Subfolder set with \"Set Folder\" must be blank to create folders", 
    "Plugin to DigitalMicrograph must be upgraded to create folders"));
  else if (mFalconType && (mCamFlags & PLUGFEI_USES_ADVANCED)) {
    SetDlgItemText(IDC_STAT_MUST_EXIST, "Folder will be created if necessary inside "
      "Falcon storage location");
    SetDlgItemText(IDC_STAT_MUST_EXIST2, "");
  } else if (!mCanCreateDir && mDEtype) {
    SetDlgItemText(IDC_STAT_MUST_EXIST, "SerialEM must run on DE server and server "
      "version");
    str.Format("must be at least %d.%d.%d to create folders", DE_CAN_SET_FOLDER / 1000000,
      (DE_CAN_SET_FOLDER / 10000) % 100, DE_CAN_SET_FOLDER % 10000);
    SetDlgItemText(IDC_STAT_MUST_EXIST2, str);
  }
  if (mDEtype)
    SetDlgItemText(IDC_STAT_COMPTITLE, mCanCreateDir ? "Components for Folder and "
    "Filename Suffix" : "Components for Filename Suffix");
  m_sbcDigits.SetPos(50);
  m_sbcDigits.SetRange(0, 100);
  B3DCLAMP(mNumberDigits, 3, 5);
  m_strDigits.Format("Digits: %d", mNumberDigits);
  m_butRootFolder.EnableWindow(mCanCreateDir);
  m_butSaveFileFolder.EnableWindow(mCanCreateDir);
  m_butNavlabelFolder.EnableWindow(mCanCreateDir);
  m_butRootFolder.ShowWindow(show);
  m_butSaveFileFolder.ShowWindow(show);
  m_butNavlabelFolder.ShowWindow(show);
  m_statUseComp.ShowWindow(show);
  m_statFolder.ShowWindow(show);
  m_statFile.ShowWindow(show);
  m_butHourMinSec.ShowWindow(mDEtype ? SW_HIDE : SW_SHOW);
  m_butMonthDay.ShowWindow(mDEtype ? SW_HIDE : SW_SHOW);
  m_butDatePrefix.ShowWindow(mDEtype ? SW_HIDE : SW_SHOW);
  m_butHourMinSec.EnableWindow(!m_bDatePrefix);
  m_butMonthDay.EnableWindow(!m_bDatePrefix);
  UpdateFormat();
  ManagePackOptions();
  return TRUE;
}

void CK2SaveOptionDlg::OnOK()
{
  UpdateData(true);
  if (!m_bNumber)
    m_bHourMinSec = true;
  mNameFormat = (m_bRootFolder ? FRAME_FOLDER_ROOT : 0) |
    (m_bSavefileFolder ? FRAME_FOLDER_SAVEFILE : 0) |
    (m_bNavLabelFolder ?FRAME_FOLDER_NAVLABEL : 0) |
    (m_bRootFile ? FRAME_FILE_ROOT : 0) |
    (m_bSavefileFile ? FRAME_FILE_SAVEFILE : 0) |
    (m_bNavLabelFile ? FRAME_FILE_NAVLABEL : 0) |
    (m_bNumber ? FRAME_FILE_NUMBER : 0) |
    (m_bTiltAngle ? FRAME_FILE_TILT_ANGLE : 0) |
    (m_bMonthDay ? FRAME_FILE_MONTHDAY : 0) |
    (m_bHourMinSec ? FRAME_FILE_HOUR_MIN_SEC : 0) |
    (m_bDatePrefix ? FRAME_FILE_DATE_PREFIX : 0) |
    (m_bOnlyWhenAcquire ? FRAME_LABEL_IF_ACQUIRE : 0) |
    (m_bMultiHolePos ? FRAME_FILE_HOLE_AND_POS : 0);
  CBaseDlg::OnOK();
}

void CK2SaveOptionDlg::OnCancel()
{
  CBaseDlg::OnCancel();
}

void CK2SaveOptionDlg::OnKillfocusEditBasename()
{
  UpdateData(true);
  UpdateFormat();
}

void CK2SaveOptionDlg::OnCheckComponent()
{
  BOOL numberBefore = m_bNumber;
  BOOL hmsBefore = m_bHourMinSec;
  UpdateData(true);
  if (numberBefore && !m_bNumber && !m_bHourMinSec)
    m_bHourMinSec = true;
  else if (hmsBefore && !m_bHourMinSec && !m_bNumber)
    m_bNumber = true;
  UpdateFormat();
}

void CK2SaveOptionDlg::OnDeltaposSpinDigits(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mNumberDigits, 3, 5, mNumberDigits))
    return;
  m_strDigits.Format("Digits: %d", mNumberDigits);
  UpdateFormat();
}

void CK2SaveOptionDlg::UpdateFormat(void)
{
  CString date, time, filename, folders;
  char digHash[6] = "#####";
  CTime ctDateTime = CTime::GetCurrentTime();
  mWinApp->mDocWnd->DateTimeComponents(date, time, m_bDatePrefix);
  m_strExample = (m_bRootFolder && !m_strBasename.IsEmpty() && mCanCreateDir) ? 
    "Base" : "";
  if (mFalconType && (mCamFlags & PLUGFEI_USES_ADVANCED) && !mCanCreateDir)
    m_strExample = "Subfolder";
  if (m_bSavefileFolder && mCanCreateDir) 
    UtilAppendWithSeparator(m_strExample, "File", "_");
  if (m_bNavLabelFolder && mCanCreateDir)
    UtilAppendWithSeparator(m_strExample, "Label", "_");
  if (!m_strExample.IsEmpty())
    m_strExample += "\\";
  if (mCanCreateDir)
    folders = m_strExample;
  m_statMustExist.ShowWindow((m_strExample.IsEmpty() && mCanCreateDir) 
    ? SW_HIDE : SW_SHOW);
  m_statMustExist2.ShowWindow(((!mDEtype && (m_strExample.IsEmpty() || !mCanCreateDir)) || 
    (mDEtype && mCanCreateDir)) ? SW_HIDE : SW_SHOW);
  if (m_bRootFile && !m_strBasename.IsEmpty())
    filename = "Base";
  if (m_bSavefileFile)
    UtilAppendWithSeparator(filename, "File", "_");
  if (m_bNavLabelFile) 
    UtilAppendWithSeparator(filename, "Label", "_");
  if (m_bNumber)
    UtilAppendWithSeparator(filename, &digHash[5 - mNumberDigits], "_");
  if (m_bTiltAngle)
    UtilAppendWithSeparator(filename, "Tilt", "_");
  if (m_bDatePrefix && !mDEtype) {
    if (!filename.IsEmpty())
      filename = "_" + filename;
    filename = date + "_" + time + filename;
  } else {
    if (m_bMonthDay && !mDEtype)
      UtilAppendWithSeparator(filename, date, "_");
    if (m_bHourMinSec && !mDEtype)
      UtilAppendWithSeparator(filename, time, "_");
  }
  if (m_bMultiHolePos)
    UtilAppendWithSeparator(filename, "Hole-pos", "_");
  if (mDEtype) {
    if (!filename.IsEmpty())
      filename += "_";
    m_strExample.Format("Format: %s%d%02d%02d_00001_%sRawImages", folders, 
      ctDateTime.GetYear(), ctDateTime.GetMonth(), ctDateTime.GetDay(), filename);
  } else
    m_strExample = CString("Format: ") + m_strExample + filename;
  UpdateData(false);
}

void CK2SaveOptionDlg::ManagePackOptions(void)
{
  bool unNormed = !mSetIsGainNormalized || 
    (mCanGainNormSum && m_bSaveUnnormalized && mK2Type);
  m_butPackRawFrame.EnableWindow(unNormed);
  bool binning = mWinApp->mCamera->IsK3BinningSuperResFrames(mCamParams, 1, mSaveSetting,
    mAlignSetting, mUseFrameAlign, unNormed ? DARK_SUBTRACTED : GAIN_NORMALIZED, 
    mK2mode, mTakingK3Binned);
  bool reducing = !unNormed && (mK2Type == K3_TYPE || mK2mode == K2_SUPERRES_MODE) &&
    mK2mode > 0 && !binning && mCanReduceSuperres && m_bReduceSuperres && mK2Type;
  m_butPackCounting4Bits.EnableWindow(unNormed && m_bPackRawFrames && 
    mCan4BitModeAndCounting);
  m_butUse4BitMode.EnableWindow(unNormed && m_bPackRawFrames && !m_iFileType &&
    mCan4BitModeAndCounting);
  m_butReduceSuperres.EnableWindow(!unNormed && mCanReduceSuperres && !mTakingK3Binned);
  m_butSavesTimes100.EnableWindow(!unNormed && mCanSaveTimes100 && !m_bReduceSuperres);
  m_butUseExtensionMRCS.EnableWindow(!m_iFileType && mCanUseExtMRCS);
  m_butSaveFrameStackMdoc.EnableWindow(mCanSaveFrameStackMdoc);
  m_strCurSetSaves.Format("%s to %s%s%s%s", unNormed ? "raw" : "norm",
      m_iFileType > 0 ? (m_iFileType > 2 ? "TIF-ZIP" : "TIF-LZW") : "MRC", 
      (reducing || binning) ? "" : (m_bOneFramePerFile ? " files" : " stack"),
      (unNormed && mK2mode > 0 && m_bPackRawFrames && mK2Type)? ", packed" : "",
      reducing ? ", reduced" : (binning ? ", binned" : ""));
  UpdateData(false);
}

void CK2SaveOptionDlg::OnPackRawFrame()
{
  UpdateData(true);
  ManagePackOptions();
}

void CK2SaveOptionDlg::OnSaveMrc()
{
  UpdateData(true);
  ManagePackOptions();
}

void CK2SaveOptionDlg::OnSaveUnnormalized()
{
  UpdateData(true);
  ManagePackOptions();
}

void CK2SaveOptionDlg::OnReduceSuperres()
{
  UpdateData(true);
  ManagePackOptions();
}

void CK2SaveOptionDlg::OnCheckDatePrefix()
{
  UpdateData(true);
  m_butHourMinSec.EnableWindow(!m_bDatePrefix);
  m_butMonthDay.EnableWindow(!m_bDatePrefix);
  UpdateFormat();
}
