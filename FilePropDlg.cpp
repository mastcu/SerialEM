// FilePropDlg.cpp:      To set properties for saving images to files
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include ".\FilePropDlg.h"
#include "Image\KStoreMRC.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SetFlag(a,b) \
  if ((a)) \
    mFileOpt.typext |= (b); \
  else \
    mFileOpt.typext &= ~(b);

static int compressions[] = {COMPRESS_NONE, COMPRESS_ZIP, COMPRESS_LZW, COMPRESS_JPEG};

enum { RADIO_TYPE_MRC = 0, RADIO_TYPE_HDF, RADIO_TYPE_TIFF, RADIO_TYPE_ADOC, 
  RADIO_TYPE_JPEG };

/////////////////////////////////////////////////////////////////////////////
// CFilePropDlg dialog


CFilePropDlg::CFilePropDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CFilePropDlg::IDD, pParent)
  , m_bSaveMdoc(FALSE)
  , m_bSkipFileDlg(FALSE)
{
  //{{AFX_DATA_INIT(CFilePropDlg)
  m_iMaxSects = 1;
  m_iByteInt = 0;
  m_bStagePos = FALSE;
  m_bTiltAngle = FALSE;
  m_bMag = FALSE;
  m_fTruncBlack = 0.2f;
  m_fTruncWhite = 0.2f;
  m_iUnsignOpt = 0;
	m_bIntensity = FALSE;
  m_bExposure = FALSE;
  m_iCompress = 0;
  m_iFileType = 0;
	//}}AFX_DATA_INIT
}


void CFilePropDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CFilePropDlg)
  DDX_Control(pDX, IDC_RTRUNCATE, m_butTruncate);
  DDX_Control(pDX, IDC_RDIVIDE, m_butDivide);
  DDX_Control(pDX, IDC_RSHIFTDOWN, m_butShiftDown);
  DDX_Control(pDX, IDC_RUNSIGNED, m_butSaveUnsigned);
  DDX_Control(pDX, IDC_STATSAVING16, m_groupSaving16);
  DDX_Control(pDX, IDC_TRUNCWHITETEXT, m_statTruncWhite);
  DDX_Control(pDX, IDC_TRUNCWHITEEDIT, m_editTruncWhite);
  DDX_Control(pDX, IDC_TRUNCBLACKEDIT, m_editTruncBlack);
  DDX_Control(pDX, IDC_TRUNCBLACKTEXT, m_statTruncBlack);
  DDX_Control(pDX, IDC_TRUNCGROUP, m_groupTrunc);
  DDX_Control(pDX, IDC_MAXSECTSEDIT, m_editMaxSect);
  DDX_Control(pDX, IDC_MAXSECTSTEXT, m_statMaxSects);
  DDX_Control(pDX, IDC_BEGENEROUS, m_statGenerous);
  DDX_Text(pDX, IDC_MAXSECTSEDIT, m_iMaxSects);
  DDV_MinMaxInt(pDX, m_iMaxSects, 1, 10000);
  DDX_Radio(pDX, IDC_RBYTE, m_iByteInt);
  DDX_Check(pDX, IDC_STAGEPOS, m_bStagePos);
  DDX_Check(pDX, IDC_TILTANGLE, m_bTiltAngle);
  DDX_Check(pDX, IDC_MAGNIFICATION, m_bMag);
  DDX_Text(pDX, IDC_TRUNCBLACKEDIT, m_fTruncBlack);
  DDV_MinMaxFloat(pDX, m_fTruncBlack, 0., 2.5f);
  DDX_Text(pDX, IDC_TRUNCWHITEEDIT, m_fTruncWhite);
  DDV_MinMaxFloat(pDX, m_fTruncWhite, 0., 2.5f);
  DDX_Radio(pDX, IDC_RTRUNCATE, m_iUnsignOpt);
  DDX_Radio(pDX, IDC_RNOCOMPRESS, m_iCompress);
  DDX_Radio(pDX, IDC_RMRCFILE, m_iFileType);
  DDX_Check(pDX, IDC_SAVE_INTENSITY, m_bIntensity);
  DDX_Check(pDX, IDC_EXPOSURE_DOSE, m_bExposure);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_STATCOMPRESS, m_statCompress);
  DDX_Control(pDX, IDC_RNOCOMPRESS, m_butNoComp);
  DDX_Control(pDX, IDC_RZIPCOMPRESS, m_butZIPComp);
  DDX_Control(pDX, IDC_RLZWCOMPRESS, m_butLZWComp);
  DDX_Control(pDX, IDC_RJPEGCOMPRESS, m_butJPEGComp);
  DDX_Control(pDX, IDC_RTIFFFILE, m_butTiffFile);
  DDX_Control(pDX, IDC_EXTENDEDGROUP, m_statExtended);
  DDX_Control(pDX, IDC_TILTANGLE, m_butTiltangle);
  DDX_Control(pDX, IDC_SAVE_INTENSITY, m_butIntensity);
  DDX_Control(pDX, IDC_STAGEPOS, m_butStagePos);
  DDX_Control(pDX, IDC_MAGNIFICATION, m_butMagnification);
  DDX_Control(pDX, IDC_EXPOSURE_DOSE, m_butExpDose);
  DDX_Control(pDX, IDC_SAVEMDOC, m_butSaveMdoc);
  DDX_Check(pDX, IDC_SAVEMDOC, m_bSaveMdoc);
  DDX_Control(pDX, IDC_RBYTE, m_butSaveByte);
  DDX_Control(pDX, IDC_RINTEGERS, m_butSaveInteger);
  DDX_Check(pDX, IDC_SKIP_FILE_DLG, m_bSkipFileDlg);
  DDX_Control(pDX, IDC_RJPEGFILE, m_butJpegFile);
}


BEGIN_MESSAGE_MAP(CFilePropDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CFilePropDlg)
  ON_BN_CLICKED(IDC_RBYTE, OnRbyte)
  ON_EN_KILLFOCUS(IDC_TRUNCBLACKEDIT, OnKillfocusTruncblackedit)
  ON_EN_KILLFOCUS(IDC_TRUNCWHITEEDIT, OnKillfocusTruncwhiteedit)
  ON_EN_KILLFOCUS(IDC_MAXSECTSEDIT, OnKillfocusMaxsectsedit)
  ON_BN_CLICKED(IDC_RINTEGERS, OnRbyte)
  ON_BN_CLICKED(IDC_TILTANGLE, OnRbyte)
  ON_BN_CLICKED(IDC_STAGEPOS, OnRbyte)
  ON_BN_CLICKED(IDC_MAGNIFICATION, OnRbyte)
	ON_BN_CLICKED(IDC_SAVE_INTENSITY, OnRbyte)
	ON_BN_CLICKED(IDC_EXPOSURE_DOSE, OnRbyte)
	ON_BN_CLICKED(IDC_RUNSIGNED, OnRbyte)
  ON_BN_CLICKED(IDC_RMRCFILE, OnRfileType)
  ON_BN_CLICKED(IDC_RHDFFILE, OnRfileType)
  ON_BN_CLICKED(IDC_RTIFFFILE, OnRfileType)
	ON_BN_CLICKED(IDC_RADOCFILE, OnRfileType)
	ON_BN_CLICKED(IDC_RJPEGFILE, OnRfileType)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_SAVEMDOC, OnSaveMdoc)
  ON_BN_CLICKED(IDC_RNOCOMPRESS, OnRNoCompress)
  ON_BN_CLICKED(IDC_RLZWCOMPRESS, OnRNoCompress)
  ON_BN_CLICKED(IDC_RZIPCOMPRESS, OnRNoCompress)
  ON_BN_CLICKED(IDC_RJPEGCOMPRESS, OnRNoCompress)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFilePropDlg message handlers

void CFilePropDlg::OnRbyte()
{
  UpdateData(TRUE); 

  if ((m_iFileType == RADIO_TYPE_TIFF || m_iFileType == RADIO_TYPE_ADOC) && 
    mFileOpt.mode == MRC_MODE_FLOAT && m_iCompress == 3) {
    m_iCompress = 1;
    UpdateData(false);
  }
  ManageStates(); 
}

void CFilePropDlg::OnRfileType()
{
  int prevFileType = m_iFileType;
  UpdateData(true);

  // Unload the compression appropriately for the previous file type
  if (prevFileType == RADIO_TYPE_HDF)
    mFileOpt.hdfCompression = compressions[m_iCompress];
  else
    mFileOpt.compression = compressions[m_iCompress];

  // Load in the respective one for the new file type
  SetCompressionFromFileOpt();
  UpdateData(false);
  ManageStates();
}

/*void CFilePropDlg::OnChangeMaxsectsedit() 
{
  UpdateData(TRUE); 
}

void CFilePropDlg::OnChangeTruncblackedit() 
{
  UpdateData(TRUE); 
}

void CFilePropDlg::OnChangeTruncwhiteedit() 
{
  UpdateData(TRUE); 
}
*/
// Enable or disable various items based on byte versus int
// or whether any extra header info is being selected
void CFilePropDlg::ManageStates()
{
  BOOL tiffFile = (mFileOpt.TIFFallowed && m_iFileType == RADIO_TYPE_TIFF) || 
    m_iFileType == RADIO_TYPE_ADOC;
  BOOL jpegFile = m_iFileType == RADIO_TYPE_JPEG;
  BOOL notFloat = mFileOpt.mode != MRC_MODE_FLOAT;
  BOOL bEnable = (m_iByteInt == 0 || (tiffFile && compressions[m_iCompress] == 
    COMPRESS_JPEG) || jpegFile) && notFloat;
  m_groupTrunc.EnableWindow(bEnable);
  m_statTruncWhite.EnableWindow(bEnable);
  m_editTruncWhite.EnableWindow(bEnable);
  m_statTruncBlack.EnableWindow(bEnable);
  m_editTruncBlack.EnableWindow(bEnable);

  m_statCompress.EnableWindow(tiffFile || m_iFileType == RADIO_TYPE_HDF);
  m_butNoComp.EnableWindow(tiffFile || m_iFileType == RADIO_TYPE_HDF);
  m_butLZWComp.EnableWindow(tiffFile);
  m_butZIPComp.EnableWindow(tiffFile || m_iFileType == RADIO_TYPE_HDF);
  m_butJPEGComp.EnableWindow(tiffFile && notFloat);

  bEnable = (!tiffFile || compressions[m_iCompress] != COMPRESS_JPEG) && !jpegFile &&
    notFloat;
  m_butSaveByte.EnableWindow(bEnable);
  m_butSaveInteger.EnableWindow(bEnable);
  m_butSaveUnsigned.EnableWindow(bEnable);

  bEnable = (m_iByteInt == 1 && mAny16Bit) && bEnable;
  m_groupSaving16.EnableWindow(bEnable);
  m_butTruncate.EnableWindow(bEnable);
  m_butDivide.EnableWindow(bEnable);
  m_butShiftDown.EnableWindow(bEnable);

  bEnable = m_iFileType == RADIO_TYPE_MRC;
  m_statExtended.EnableWindow(bEnable);
  m_butTiltangle.EnableWindow(bEnable);
  m_butStagePos.EnableWindow(bEnable);
  m_butIntensity.EnableWindow(bEnable);
  m_butExpDose.EnableWindow(bEnable);
  m_butMagnification.EnableWindow(bEnable);
  //m_statExtended.SetWindowText(
    //m_iFileType == 2 ? "Save in autodoc file" : "Save in extended header");

  bEnable = !m_iFileType && (m_bStagePos || m_bTiltAngle || m_bMag || m_bExposure ||
    m_bIntensity || (mFileOpt.typext & MONTAGE_MASK));
  m_editMaxSect.EnableWindow(bEnable);
  m_statMaxSects.EnableWindow(bEnable);
  m_statGenerous.EnableWindow(bEnable);
  m_butSaveMdoc.EnableWindow(!mFileOpt.montageInMdoc && !m_iFileType);
}

BOOL CFilePropDlg::OnInitDialog() 
{
  CBaseDlg::OnInitDialog();
  CButton *hdfBut = (CButton *)GetDlgItem(IDC_RHDFFILE);

  // Set the variables based on the FileOption structure
  m_iByteInt = mFileOpt.mode < 2 ? mFileOpt.mode : 2;
  m_iMaxSects = mFileOpt.maxSec;
  m_fTruncBlack = mFileOpt.pctTruncLo;
  m_fTruncWhite = mFileOpt.pctTruncHi;
  m_bTiltAngle = ((mFileOpt.typext & TILT_MASK) != 0);
  m_bStagePos = ((mFileOpt.typext & VOLT_XY_MASK) != 0);
  m_bMag = ((mFileOpt.typext & MAG100_MASK) != 0);
  m_bIntensity = ((mFileOpt.typext & INTENSITY_MASK) != 0);
  m_bExposure = ((mFileOpt.typext & DOSE_MASK) != 0);
  m_iUnsignOpt = mFileOpt.unsignOpt;
  m_butSaveUnsigned.ShowWindow(mAny16Bit ? SW_SHOW : SW_HIDE);
  m_bSaveMdoc = (mFileOpt.useMont() ? mFileOpt.montUseMdoc : mFileOpt.useMdoc) ||
    mFileOpt.montageInMdoc;

  // Originally TIFFallowed = 2 was meant to force a tiff, leave that aside with ADOC
  m_butTiffFile.EnableWindow(mFileOpt.TIFFallowed);
  m_butJpegFile.EnableWindow(mFileOpt.TIFFallowed && mFileOpt.mode != MRC_MODE_FLOAT);
  if ((mFileOpt.fileType == STORE_TYPE_TIFF ||  mFileOpt.fileType == STORE_TYPE_ADOC) &&
    mFileOpt.TIFFallowed)
    m_iFileType = RADIO_TYPE_TIFF;
  else if ((mFileOpt.useMont() ? mFileOpt.montFileType : mFileOpt.fileType) == 
    STORE_TYPE_ADOC)
    m_iFileType = RADIO_TYPE_ADOC;
  else if ((mFileOpt.useMont() ? mFileOpt.montFileType : mFileOpt.fileType) ==
    STORE_TYPE_HDF)
    m_iFileType = RADIO_TYPE_HDF;
  if (!mWinApp->mDocWnd->GetHDFsupported()) {
    hdfBut->EnableWindow(false);
    if (m_iFileType == RADIO_TYPE_HDF)
      m_iFileType = RADIO_TYPE_MRC;
  }

  if (mFileOpt.fileType == STORE_TYPE_JPEG && mFileOpt.mode == MRC_MODE_FLOAT)
    m_iFileType = (mFileOpt.TIFFallowed ? RADIO_TYPE_TIFF : RADIO_TYPE_MRC);
  SetCompressionFromFileOpt();

  UpdateData(FALSE);
  ManageStates();
//  EnableToolTips(true);
  if (m_bSkipFileDlg && !mShowDlgThisTime)
    OnOK();
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CFilePropDlg::SetCompressionFromFileOpt()
{
  m_iCompress = 0;
  for (int i = 0; i < 4; i++)
    if (mFileOpt.compression == compressions[i])
      m_iCompress = i;
  if (m_iFileType == RADIO_TYPE_HDF)
    m_iCompress = mFileOpt.hdfCompression == COMPRESS_ZIP ? 1 : 0;
  if ((m_iFileType == RADIO_TYPE_TIFF || m_iFileType == RADIO_TYPE_ADOC) &&
    mFileOpt.mode == MRC_MODE_FLOAT && m_iCompress == 3)
    m_iCompress = 1;
}

void CFilePropDlg::OnOK() 
{
  // Unload the data and then recode it into the FileOption
  int filety = STORE_TYPE_MRC;
  UpdateData(TRUE); 
  if (mFileOpt.mode != 2) 
    mFileOpt.mode = m_iByteInt < 2 ? m_iByteInt : MRC_MODE_USHORT;
  mFileOpt.maxSec = m_iMaxSects;
  mFileOpt.pctTruncLo = m_fTruncBlack;
  mFileOpt.pctTruncHi = m_fTruncWhite;
  mFileOpt.unsignOpt = m_iUnsignOpt;
  SetFlag(m_bTiltAngle, TILT_MASK);
  SetFlag(m_bMag, MAG100_MASK);
  SetFlag(m_bStagePos, VOLT_XY_MASK);
  SetFlag(m_bIntensity, INTENSITY_MASK);
  SetFlag(m_bExposure, DOSE_MASK);
  if (!mFileOpt.montageInMdoc) {
    if (mFileOpt.useMont())
      mFileOpt.montUseMdoc = m_bSaveMdoc;
    else
      mFileOpt.useMdoc = m_bSaveMdoc;
  }
  if (m_iFileType == RADIO_TYPE_TIFF)
    filety = STORE_TYPE_TIFF;
  if (m_iFileType == RADIO_TYPE_ADOC)
    filety = STORE_TYPE_ADOC;
  if (m_iFileType == RADIO_TYPE_HDF) {
    filety = STORE_TYPE_HDF;
    if (mFileOpt.isMontage()) {
      mFileOpt.montageInMdoc = true;
      mFileOpt.typext &= ~MONTAGE_MASK;
    }
  }
  if (m_iFileType == RADIO_TYPE_JPEG)
    filety = STORE_TYPE_JPEG;
  if (mFileOpt.useMont())
    mFileOpt.montFileType = filety;
  else
    mFileOpt.fileType = filety;
  if (m_iFileType == RADIO_TYPE_HDF)
    mFileOpt.hdfCompression = compressions[m_iCompress];
  else
    mFileOpt.compression = compressions[m_iCompress];
  CDialog::OnOK();
}

void CFilePropDlg::OnKillfocusTruncblackedit() 
{
  UpdateData(TRUE);
}

void CFilePropDlg::OnKillfocusTruncwhiteedit() 
{
  UpdateData(TRUE);
}

void CFilePropDlg::OnKillfocusMaxsectsedit() 
{
  UpdateData(TRUE); 
}

void CFilePropDlg::OnSaveMdoc()
{
}


void CFilePropDlg::OnRNoCompress()
{
  UpdateData(TRUE);
  ManageStates();
}
