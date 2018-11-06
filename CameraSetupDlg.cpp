// CameraSetupDlg.cpp:   To set camera exposure parameters and select camera
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\CameraSetupDlg.h"
#include "CameraController.h"
#include "CalibCameraTiming.h"
#include "EMscope.h"
#include "Utilities\KGetOne.h"
#include "GatanSocket.h"
#include "TSController.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "FalconFrameDlg.h"
#include "FalconHelper.h"
#include "K2SaveOptionDlg.h"
#include "FrameAlignDlg.h"
#include "XFolderDialog\XFolderDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_BIN_BUTTONS 10
#define SHIFT_FRACTION 64
#define MAX_DARK_AVERAGE 20
#define MAX_DE_SUM_COUNT 250

static int idTable[] = {IDC_STATCAMERA, IDC_RCAMERA1, IDC_RCAMERA2, IDC_RCAMERA3,
IDC_RCAMERA4, IDC_RCAMERA5, IDC_RCAMERA6, IDC_MATCH_REG_PIXEL, IDC_MATCH_REG_INTENSITY,
PANEL_END,
IDC_STATPARAMBOX, IDC_RVIEW, IDC_RFOCUS, IDC_RTRIAL, IDC_RRECORD, IDC_RPREVIEW,
IDC_STATPARAMTEXT, IDC_BIGMODETEXT, IDC_BUTCOPYCAMERA, IDC_STATCOPYCAMERA,
IDC_RCONTINUOUS, IDC_RSINGLE, IDC_STATACQUIS, IDC_RUNPROCESSED,IDC_EDITBOTTOM,
IDC_EDITEXPOSURE, IDC_EDITSETTLING, IDC_EDITTOP, IDC_EDITLEFT,IDC_EDITRIGHT,
IDC_RDARKSUBTRACT, IDC_RGAINNORMALIZED, IDC_RBOOSTMAG3, IDC_RBOOSTMAG4, IDC_STATPROC, 
IDC_STATINTEGRATION, IDC_SPININTEGRATION, IDC_EDITINTEGRATION, IDC_STATEXPTIMESINT,
IDC_STATBIN, IDC_RBIN1, IDC_RBIN2, IDC_RBIN3, IDC_RBIN4, IDC_RBIN5, IDC_RBIN6, IDC_RBIN7,
IDC_RBIN8, IDC_RBIN9, IDC_RBIN10, IDC_STATEXPTIME, IDC_STATSEC1, IDC_DRIFTTEXT1, 
IDC_DRIFTTEXT2, IDC_MINIMUM_DRIFT, IDC_STATBOX, IDC_STATTOP, IDC_STATLEFT, IDC_STATBOT,
IDC_STATRIGHT, IDC_BUTQUARTER, IDC_BUTHALF, IDC_BUTFULL, IDC_REMOVEXRAYS, IDC_RMONTAGE,
IDC_STATMICRON, IDC_STATAREA, IDC_STATPOSIT, IDC_STATSIZE, IDC_SPINLEFTRIGHT, IDC_RSEARCH,
IDC_SPINUPDOWN, IDC_BUTRECENTER, IDC_BUTSWAPXY, IDC_KEEP_PIXEL_TIME,
IDC_CHECK_CORRECT_DRIFT, IDC_STAT_NORM_DSDF, IDC_CHECK_HARDWARE_ROI, PANEL_END,
IDC_STATSHUTTER, IDC_RBEAMONLY, IDC_RFILMONLY, IDC_RSHUTTERCOMBO, IDC_BUTWIDEQUARTER,
IDC_BUTWIDEHALF, IDC_BUTSMALLER10, IDC_BUTBIGGER10, IDC_BUTABITLESS, PANEL_END,
IDC_DARKNEXT, IDC_DARKALWAYS, IDC_EDITAVERAGE, IDC_AVERAGEDARK, IDC_SPINAVERAGE, 
IDC_STATAVERAGE, IDC_STAT_LINE1, PANEL_END,
IDC_BUTUPDATEDOSE, IDC_STATELECDOSE, IDC_STATDOSERATE, IDC_STAT_DOSE_PER_FRAME, PANEL_END,
IDC_STAT_SCANRATE, IDC_BUT_MAX_SCAN_RATE, IDC_STAT_TIMING_AVAIL,
IDC_CHECK_DYNFOCUS, IDC_CHECK_LINESYNC, IDC_STATCHANACQ, IDC_STATCHAN1, IDC_STATCHAN2,
IDC_STATCHAN3, IDC_STATCHAN4, IDC_STATCHAN5, IDC_STATCHAN6, IDC_STATCHAN7, IDC_STATCHAN8,
IDC_COMBOCHAN1,IDC_COMBOCHAN2, IDC_COMBOCHAN3, IDC_COMBOCHAN4, IDC_COMBOCHAN5, 
IDC_COMBOCHAN6, IDC_COMBOCHAN7, IDC_COMBOCHAN8, PANEL_END,
IDC_STAT_K2MODE, IDC_RLINEAR, IDC_RCOUNTING, IDC_RSUPERRES, IDC_DOSE_FRAC_MODE, 
IDC_STAT_FRAME_TIME, IDC_EDIT_FRAME_TIME, IDC_STAT_FRAME_SEC, IDC_ALIGN_DOSE_FRAC,
IDC_BUT_SETUP_ALIGN, IDC_SAVE_FRAMES, IDC_SET_SAVE_FOLDER, IDC_BUT_FILE_OPTIONS,
IDC_STAT_SAVE_SUMMARY, IDC_STAT_ANTIALIAS, IDC_SETUP_FALCON_FRAMES,
IDC_SETUP_K2_FRAME_SUMS, IDC_SAVE_FRAME_SUMS, IDC_ALWAYS_ANTIALIAS, IDC_STAT_WHERE_ALIGN,
IDC_STAT_INTERMEDIATE_ONOFF, IDC_STAT_ALIGN_SUMMARY, IDC_CHECK_TAKE_K3_BINNED, PANEL_END,
IDC_STAT_DEMODE, IDC_RDE_LINEAR, IDC_RDE_COUNTING, IDC_RDE_SUPERRES, IDC_EDIT_DE_FPS,
IDC_DE_SAVE_FRAMES, IDC_EDIT_DE_SUM_COUNT, IDC_DE_SAVE_MASTER,
IDC_DE_SAVE_FINAL, IDC_STAT_NUM_DE_RAW, IDC_BUT_NAME_SUFFIX, IDC_STAT_DE_FRAME_TIME,
IDC_STAT_DE_FRAME_SEC, IDC_STAT_DEFPS, IDC_DE_ALIGN_FRAMES, IDC_BUT_DE_SETUP_ALIGN,
IDC_EDIT_DE_FRAME_TIME, IDC_STAT_DE_SUM_NUM, IDC_SPIN_DE_SUM_NUM, IDC_STAT_DE_WHERE_ALIGN,
IDC_DE_SET_SAVE_FOLDER, PANEL_END,
IDOK, IDC_ACQUIRE_REOPEN, IDCANCEL, IDC_BUTHELP, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];

/////////////////////////////////////////////////////////////////////////////
// CCameraSetupDlg dialog

CCameraSetupDlg::CCameraSetupDlg(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CCameraSetupDlg::IDD, pParent)
  , m_bLineSync(FALSE)
  , m_bDynFocus(FALSE)
  , m_strScanRate(_T(""))
  , m_bKeepPixel(FALSE)
  , m_strTimingAvail(_T(""))
  , m_iK2Mode(0)
  , m_iDEMode(0)
  , m_bDoseFracMode(FALSE)
  , m_fFrameTime(0)
  , m_bAlignDoseFrac(FALSE)
  , m_bSaveFrames(FALSE)
  , m_strDoseRate(_T(""))
  , m_bDEsaveFrames(FALSE)
  , m_bDEsaveMaster(FALSE)
  , m_bDEsaveFinal(FALSE)
  , m_iSumCount(0)
  , m_fDEframeTime(0.04f)
  , m_fDEfps(25.f)
  , m_bDEalignFrames(FALSE)
  , m_strNumDEraw(_T(""))
  , mChangedBinning(false)
  , mMaxIntegration(1)
  , m_iIntegration(1)
  , m_bCorrectDrift(FALSE)
  , m_bUseHardwareROI(FALSE)
  , m_bSaveK2Sums(FALSE)
  , m_bAlwaysAntialias(FALSE)
  , m_bTakeK3Binned(FALSE)
{
  //{{AFX_DATA_INIT(CCameraSetupDlg)
  m_iBinning = -1;
  m_iContSingle = -1;
  m_iProcessing = -1;
  m_iControlSet = -1;
  m_iShuttering = -1;
  m_sBigText = _T("");
  m_bDarkAlways = FALSE;
  m_bDarkNext = FALSE;
  m_eBottom = 1;
  m_eTop = 0;
  m_eLeft = 0;
  m_eRight = 1;
  m_eExposure = 0.0f;
  m_eSettling = 0.0f;
  m_strSettling = "0";
  m_iCamera = -1;
  m_strCopyCamera = _T("");
	m_strElecDose = _T("");
	m_iAverageTimes = 2;
	m_bAverageDark = FALSE;
	m_bRemoveXrays = FALSE;
  m_bMatchPixel = FALSE;
  m_bMatchIntensity = FALSE;
	//}}AFX_DATA_INIT
  mClosing = FALSE;
  mMinExposure = 0.0001f;
}


void CCameraSetupDlg::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CCameraSetupDlg)
  DDX_Control(pDX, IDC_REMOVEXRAYS, m_butRemoveXrays);
  DDX_Control(pDX, IDC_AVERAGEDARK, m_butAverageDark);
  DDX_Control(pDX, IDC_EDITAVERAGE, m_editAverage);
  DDX_Control(pDX, IDC_SPINAVERAGE, m_spinAverage);
  DDX_Control(pDX, IDC_STATAVERAGE, m_timesText);
  DDX_Control(pDX, IDC_EDITINTEGRATION, m_editIntegration);
  DDX_Control(pDX, IDC_SPININTEGRATION, m_spinIntegration);
  DDX_Control(pDX, IDC_STATINTEGRATION, m_statIntegration);
  DDX_Control(pDX, IDC_STATEXPTIMESINT, m_statExpTimesInt);
  DDX_Text(pDX, IDC_STATEXPTIMESINT, m_strExpTimesInt);
  DDX_Control(pDX, IDC_BUTUPDATEDOSE, m_butUpdateDose);
  DDX_Control(pDX, IDC_BUTRECENTER, m_butRecenter);
  DDX_Control(pDX, IDC_BUTSWAPXY, m_butSwapXY);
  DDX_Control(pDX, IDC_STATCOPYCAMERA, m_statCopyCamera);
  DDX_Control(pDX, IDC_STATCAMERA, m_statCamera);
  DDX_Control(pDX, IDC_BUTCOPYCAMERA, m_butCopyCamera);
  DDX_Control(pDX, IDC_BUTSMALLER10, m_butSmaller10);
  DDX_Control(pDX, IDC_BUTBIGGER10, m_butBigger10);
  DDX_Control(pDX, IDC_BUTABITLESS, m_butABitLess);
  DDX_Control(pDX, IDC_BUTWIDEQUARTER, m_butWideQuarter);
  DDX_Control(pDX, IDC_BUTWIDEHALF, m_butWideHalf);
  DDX_Control(pDX, IDC_BUTQUARTER, m_butQuarter);
  DDX_Control(pDX, IDC_BUTHALF, m_butHalf);
  DDX_Control(pDX, IDC_BUTFULL, m_butFull);
  DDX_Control(pDX, IDC_STATBOX, m_statBox);
  DDX_Control(pDX, IDC_BIGMODETEXT, m_statBigMode);
  DDX_Control(pDX, IDC_EDITSETTLING, m_driftEdit);
  DDX_Control(pDX, IDC_DRIFTTEXT2, m_driftText2);
  DDX_Control(pDX, IDC_DRIFTTEXT1, m_driftText1);
  DDX_Radio(pDX, IDC_RBIN1, m_iBinning);
  DDX_Radio(pDX, IDC_RCONTINUOUS, m_iContSingle);
  DDX_Radio(pDX, IDC_RUNPROCESSED, m_iProcessing);
  DDX_Radio(pDX, IDC_RVIEW, m_iControlSet);
  DDX_Radio(pDX, IDC_RBEAMONLY, m_iShuttering);
  DDX_Text(pDX, IDC_BIGMODETEXT, m_sBigText);
  DDX_Check(pDX, IDC_DARKALWAYS, m_bDarkAlways);
  DDX_Check(pDX, IDC_DARKNEXT, m_bDarkNext);
  DDX_Text(pDX, IDC_EDITBOTTOM, m_eBottom);
  DDX_Text(pDX, IDC_EDITTOP, m_eTop);
  DDX_Text(pDX, IDC_EDITLEFT, m_eLeft);
  DDX_Text(pDX, IDC_EDITRIGHT, m_eRight);
  DDX_Text(pDX, IDC_EDITEXPOSURE, m_eExposure);
  DDV_MinMaxFloat(pDX, m_eExposure, mMinExposure, 1800.f);
  DDX_Text(pDX, IDC_EDITSETTLING, m_strSettling);
  //DDV_MinMaxFloat(pDX, m_eSettling, 0.f, 10.f);
  DDX_Radio(pDX, IDC_RCAMERA1, m_iCamera);
  DDX_Text(pDX, IDC_STATCOPYCAMERA, m_strCopyCamera);
  DDX_Text(pDX, IDC_STATELECDOSE, m_strElecDose);
  DDX_Text(pDX, IDC_STAT_DOSE_PER_FRAME, m_strDosePerFrame);
  DDX_Text(pDX, IDC_EDITAVERAGE, m_iAverageTimes);
  DDV_MinMaxInt(pDX, m_iAverageTimes, 2, MAX_DARK_AVERAGE);
  DDX_Text(pDX, IDC_EDITINTEGRATION, m_iIntegration);
  DDV_MinMaxInt(pDX, m_iIntegration, 1, mMaxIntegration);
  DDX_Check(pDX, IDC_AVERAGEDARK, m_bAverageDark);
  DDX_Check(pDX, IDC_REMOVEXRAYS, m_bRemoveXrays);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_SPINUPDOWN, m_sbcUpDown);
  DDX_Control(pDX, IDC_SPINLEFTRIGHT, m_sbcLeftRight);
  DDX_Control(pDX, IDC_EDITTOP, m_editTop);
  DDX_Control(pDX, IDC_EDITLEFT, m_editLeft);
  DDX_Control(pDX, IDC_EDITBOTTOM, m_editBottom);
  DDX_Control(pDX, IDC_EDITRIGHT, m_editRight);
  DDX_Control(pDX, IDC_DARKNEXT, m_butDarkNext);
  DDX_Control(pDX, IDC_DARKALWAYS, m_butDarkAlways);
  DDX_Control(pDX, IDC_MATCH_REG_PIXEL, m_butMatchPixel);
  DDX_Control(pDX, IDC_MATCH_REG_INTENSITY, m_butMatchIntensity);
  DDX_Check(pDX, IDC_MATCH_REG_PIXEL, m_bMatchPixel);
  DDX_Check(pDX, IDC_MATCH_REG_INTENSITY, m_bMatchIntensity);
  DDX_Control(pDX, IDC_EDITEXPOSURE, m_editExposure);
  DDX_Control(pDX, IDC_CHECK_LINESYNC, m_butLineSync);
  DDX_Check(pDX, IDC_CHECK_LINESYNC, m_bLineSync);
  DDX_Control(pDX, IDC_STATAREA, m_statArea);
  DDX_Control(pDX, IDC_CHECK_DYNFOCUS, m_butDynFocus);
  DDX_Check(pDX, IDC_CHECK_DYNFOCUS, m_bDynFocus);
  DDX_Control(pDX, IDC_BUT_MAX_SCAN_RATE, m_butmaxScanRate);
  DDX_Text(pDX, IDC_STAT_SCANRATE, m_strScanRate);
  DDX_Control(pDX, IDC_STATBIN, m_statBinning);
  DDX_Control(pDX, IDC_MINIMUM_DRIFT, m_statMinimumDrift);
  DDX_Control(pDX, IDC_KEEP_PIXEL_TIME, m_butKeepPixel);
  DDX_Check(pDX, IDC_KEEP_PIXEL_TIME, m_bKeepPixel);
  DDX_Control(pDX, IDC_STATPROC, m_statProcessing);
  DDX_Control(pDX, IDC_RUNPROCESSED, m_butUnprocessed);
  DDX_Control(pDX, IDC_RDARKSUBTRACT, m_butDarkSubtract);
  DDX_Control(pDX, IDC_RGAINNORMALIZED, m_butGainNormalize);
  DDX_Control(pDX, IDC_RBOOSTMAG3, m_butBoostMag3);
  DDX_Control(pDX, IDC_RBOOSTMAG4, m_butBoostMag4);
  DDX_Control(pDX, IDC_STAT_TIMING_AVAIL, m_statTimingAvail);
  DDX_Text(pDX, IDC_STAT_TIMING_AVAIL, m_strTimingAvail);
  DDX_Radio(pDX, IDC_RLINEAR, m_iK2Mode);
  DDX_Radio(pDX, IDC_RDE_LINEAR, m_iDEMode);
  DDX_Check(pDX, IDC_DOSE_FRAC_MODE, m_bDoseFracMode);
  DDX_Control(pDX, IDC_STAT_FRAME_TIME, m_statFrameTime);
  DDX_Control(pDX, IDC_EDIT_FRAME_TIME, m_editFrameTime);
  DDX_Text(pDX, IDC_EDIT_FRAME_TIME, m_fFrameTime);
  DDV_MinMaxFloat(pDX, m_fFrameTime, 0., 10.);
  DDX_Control(pDX, IDC_STAT_FRAME_SEC, m_statFrameSec);
  DDX_Control(pDX, IDC_ALIGN_DOSE_FRAC, m_butAlignDoseFrac);
  DDX_Check(pDX, IDC_ALIGN_DOSE_FRAC, m_bAlignDoseFrac);
  DDX_Check(pDX, IDC_SAVE_FRAMES, m_bSaveFrames);
  DDX_Control(pDX, IDC_SAVE_FRAMES, m_butSaveFrames);
  DDX_Control(pDX, IDC_SET_SAVE_FOLDER, m_butSetSaveFolder);
  DDX_Control(pDX, IDC_DE_SET_SAVE_FOLDER, m_butDESetSaveFolder);
  DDX_Text(pDX, IDC_STATDOSERATE, m_strDoseRate);
  DDX_Control(pDX, IDC_SETUP_FALCON_FRAMES, m_butSetupFalconFrames);
  DDX_Check(pDX, IDC_DE_SAVE_FRAMES, m_bDEsaveFrames);
  DDX_Control(pDX, IDC_DE_SAVE_FRAMES, m_butDESaveFrames);
  DDX_Check(pDX, IDC_DE_SAVE_FINAL, m_bDEsaveFinal);
  DDX_Check(pDX, IDC_DE_SAVE_MASTER, m_bDEsaveMaster);
  DDX_Control(pDX, IDC_DE_SAVE_MASTER, m_butDESaveMaster);
  DDX_Control(pDX, IDC_DE_SAVE_FINAL, m_butDESaveFinal);
  DDX_Control(pDX, IDC_STAT_DE_FRAME_TIME, m_statDEframeTime);
  DDX_Control(pDX, IDC_STAT_DE_FRAME_SEC, m_statDEframeSec);
  DDX_Control(pDX, IDC_STAT_DE_WHERE_ALIGN, m_statDEwhereAlign);
  DDX_Control(pDX, IDC_STAT_DE_SUM_NUM, m_statDEsumNum);
  DDX_Control(pDX, IDC_EDIT_DE_FRAME_TIME, m_editDEframeTime);
  DDX_Control(pDX, IDC_SPIN_DE_SUM_NUM, m_spinDEsumNum);
  DDX_Control(pDX, IDC_EDIT_DE_SUM_COUNT, m_editSumCount);
  DDX_Text(pDX, IDC_EDIT_DE_SUM_COUNT, m_iSumCount);
  DDV_MinMaxInt(pDX, m_iSumCount, 1, MAX_DE_SUM_COUNT);
  DDX_Text(pDX, IDC_EDIT_DE_FRAME_TIME, m_fDEframeTime);
  DDV_MinMaxFloat(pDX, m_fDEframeTime, 0., 10.);
  DDX_Text(pDX, IDC_EDIT_DE_FPS, m_fDEfps);
  DDV_MinMaxFloat(pDX, m_fDEfps, 0., 5000.);
  DDX_Check(pDX, IDC_DE_ALIGN_FRAMES, m_bDEalignFrames);
  DDX_Control(pDX, IDC_DE_ALIGN_FRAMES, m_butDEalignFrames);
  DDX_Control(pDX, IDC_BUT_DE_SETUP_ALIGN, m_butDEsetupAlign);
  DDX_Control(pDX, IDC_RDE_SUPERRES, m_butDEsuperRes);
  DDX_Control(pDX, IDC_STAT_NUM_DE_RAW, m_statNumDEraw);
  DDX_Text(pDX, IDC_STAT_NUM_DE_RAW, m_strNumDEraw);
  DDX_Control(pDX, IDC_DOSE_FRAC_MODE, m_butDoseFracMode);
  DDX_Control(pDX, IDC_BUT_FILE_OPTIONS, m_butFileOptions);
  DDX_Control(pDX, IDC_BUT_NAME_SUFFIX, m_butNameSuffix);
  DDX_Control(pDX, IDC_STAT_SAVE_SUMMARY, m_statSaveSummary);
  DDX_Control(pDX, IDC_STAT_ALIGN_SUMMARY, m_statAlignSummary);
  DDX_Check(pDX, IDC_CHECK_CORRECT_DRIFT, m_bCorrectDrift);
  DDX_Control(pDX, IDC_CHECK_CORRECT_DRIFT, m_butCorrectDrift);
  DDX_Check(pDX, IDC_CHECK_HARDWARE_ROI, m_bUseHardwareROI);
  DDX_Control(pDX, IDC_CHECK_HARDWARE_ROI, m_butUseHardwareROI);
  DDX_Check(pDX, IDC_SAVE_FRAME_SUMS, m_bSaveK2Sums);
  DDX_Control(pDX, IDC_SAVE_FRAME_SUMS, m_butSaveFrameSums);
  DDX_Control(pDX, IDC_SETUP_K2_FRAME_SUMS, m_butSetupK2FrameSums);
  DDX_Control(pDX, IDC_ALWAYS_ANTIALIAS, m_butAlwaysAntialias);
  DDX_Check(pDX, IDC_ALWAYS_ANTIALIAS, m_bAlwaysAntialias);
  DDX_Control(pDX, IDC_STAT_NORM_DSDF, m_statNormDSDF);
  DDX_Control(pDX, IDC_BUT_SETUP_ALIGN, m_butSetupAlign);
  DDX_Control(pDX, IDC_STAT_WHERE_ALIGN, m_statWhereAlign);
  DDX_Control(pDX, IDC_STAT_INTERMEDIATE_ONOFF, m_statIntermediateOnOff);
  DDX_Control(pDX, IDC_CHECK_TAKE_K3_BINNED, m_butTakeK3Binned);
  DDX_Check(pDX, IDC_CHECK_TAKE_K3_BINNED, m_bTakeK3Binned);
}


BEGIN_MESSAGE_MAP(CCameraSetupDlg, CBaseDlg)
  //{{AFX_MSG_MAP(CCameraSetupDlg)
  ON_BN_CLICKED(IDC_BUTFULL, OnButfull)
  ON_BN_CLICKED(IDC_BUTHALF, OnButhalf)
  ON_BN_CLICKED(IDC_BUTQUARTER, OnButquarter)
  ON_BN_CLICKED(IDC_BUTWIDEHALF, OnButwidehalf)
  ON_BN_CLICKED(IDC_BUTWIDEQUARTER, OnButwidequarter)
  ON_BN_CLICKED(IDC_RBEAMONLY, OnShuttering)
  ON_BN_CLICKED(IDC_RFOCUS, OnConSet)
  ON_EN_CHANGE(IDC_EDITBOTTOM, OnChangeCoord)
  ON_WM_PAINT()
  ON_EN_KILLFOCUS(IDC_EDITLEFT, OnKillfocusLeft)
  ON_EN_KILLFOCUS(IDC_EDITRIGHT, OnKillfocusRight)
  ON_EN_KILLFOCUS(IDC_EDITTOP, OnKillfocusTop)
  ON_EN_KILLFOCUS(IDC_EDITBOTTOM, OnKillfocusBottom)
  ON_EN_KILLFOCUS(IDC_EDITEXPOSURE, OnKillfocusEditexposure)
  ON_EN_KILLFOCUS(IDC_EDITINTEGRATION, OnKillfocusEditIntegration)
  ON_EN_KILLFOCUS(IDC_EDIT_FRAME_TIME, OnKillfocusEditFrameTime)
  ON_EN_KILLFOCUS(IDC_EDITSETTLING, OnKillfocusEditsettling)
  ON_BN_CLICKED(IDC_BUTSMALLER10, OnButsmaller10)
  ON_BN_CLICKED(IDC_BUTBIGGER10, OnButbigger10)
  ON_BN_CLICKED(IDC_BUTABITLESS, OnButabitless)
  ON_BN_CLICKED(IDC_BUTCOPYCAMERA, OnButcopycamera)
  ON_BN_CLICKED(IDC_RCAMERA1, OnRcamera)
	ON_BN_CLICKED(IDC_RBIN1, OnBinning)
	ON_BN_CLICKED(IDC_BUTRECENTER, OnButrecenter)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINUPDOWN, OnDeltaposSpinupdown)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINLEFTRIGHT, OnDeltaposSpinleftright)
	ON_BN_CLICKED(IDC_BUTUPDATEDOSE, OnButupdatedose)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINAVERAGE, OnDeltaposSpinaverage)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPININTEGRATION, OnDeltaposSpinIntegration)
	ON_EN_KILLFOCUS(IDC_EDITAVERAGE, OnKillfocusEditaverage)
  ON_BN_CLICKED(IDC_RFILMONLY, OnShuttering)
  ON_BN_CLICKED(IDC_RSHUTTERCOMBO, OnShuttering)
  ON_BN_CLICKED(IDC_RRECORD, OnConSet)
  ON_BN_CLICKED(IDC_RTRIAL, OnConSet)
  ON_BN_CLICKED(IDC_RVIEW, OnConSet)
  ON_BN_CLICKED(IDC_RPREVIEW, OnConSet)
  ON_BN_CLICKED(IDC_RSEARCH, OnConSet)
  ON_BN_CLICKED(IDC_RMONTAGE, OnConSet)
  ON_EN_CHANGE(IDC_EDITLEFT, OnChangeCoord)
  ON_EN_CHANGE(IDC_EDITRIGHT, OnChangeCoord)
  ON_EN_CHANGE(IDC_EDITTOP, OnChangeCoord)
  ON_WM_HELPINFO()
  ON_BN_CLICKED(IDC_RCAMERA2, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA3, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA4, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA5, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA6, OnRcamera)
	ON_BN_CLICKED(IDC_RBIN2, OnBinning)
	ON_BN_CLICKED(IDC_RBIN3, OnBinning)
	ON_BN_CLICKED(IDC_RBIN4, OnBinning)
	ON_BN_CLICKED(IDC_RBIN5, OnBinning)
	ON_BN_CLICKED(IDC_RBIN6, OnBinning)
	ON_BN_CLICKED(IDC_RBIN7, OnBinning)
	ON_BN_CLICKED(IDC_RBIN8, OnBinning)
	ON_BN_CLICKED(IDC_RBIN9, OnBinning)
	ON_BN_CLICKED(IDC_RBIN10, OnBinning)
	ON_BN_CLICKED(IDC_RCONTINUOUS, OnContSingle)
  ON_BN_CLICKED(IDC_RSINGLE, OnContSingle)
  ON_BN_CLICKED(IDC_RUNPROCESSED, OnProcessing)
	ON_BN_CLICKED(IDC_RGAINNORMALIZED, OnProcessing)
	ON_BN_CLICKED(IDC_RDARKSUBTRACT, OnProcessing)
  ON_BN_CLICKED(IDC_ACQUIRE_REOPEN, OnAcquireReopen)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BUTSWAPXY, OnSwapXY)
  ON_CBN_SELENDOK(IDC_COMBOCHAN1, OnSelendokChan1)
  ON_CBN_SELENDOK(IDC_COMBOCHAN2, OnSelendokChan2)
  ON_CBN_SELENDOK(IDC_COMBOCHAN3, OnSelendokChan3)
  ON_CBN_SELENDOK(IDC_COMBOCHAN4, OnSelendokChan4)
  ON_CBN_SELENDOK(IDC_COMBOCHAN5, OnSelendokChan5)
  ON_CBN_SELENDOK(IDC_COMBOCHAN6, OnSelendokChan1)
  ON_CBN_SELENDOK(IDC_COMBOCHAN7, OnSelendokChan7)
  ON_CBN_SELENDOK(IDC_COMBOCHAN8, OnSelendokChan8)
  ON_BN_CLICKED(IDC_CHECK_LINESYNC, OnLinesync)
  ON_BN_CLICKED(IDC_BUT_MAX_SCAN_RATE, OnmaxScanRate)
  ON_BN_CLICKED(IDC_CHECK_DYNFOCUS, OnDynfocus)
  ON_BN_CLICKED(IDC_RLINEAR, OnK2Mode)
  ON_BN_CLICKED(IDC_RCOUNTING, OnK2Mode)
  ON_BN_CLICKED(IDC_RSUPERRES, OnK2Mode)
  ON_BN_CLICKED(IDC_RDE_LINEAR, OnK2Mode)
  ON_BN_CLICKED(IDC_RDE_COUNTING, OnK2Mode)
  ON_BN_CLICKED(IDC_RDE_SUPERRES, OnK2Mode)
  ON_BN_CLICKED(IDC_DOSE_FRAC_MODE, OnDoseFracMode)
  ON_BN_CLICKED(IDC_ALIGN_DOSE_FRAC, OnAlignDoseFrac)
  ON_BN_CLICKED(IDC_SAVE_FRAMES, OnSaveFrames)
  ON_BN_CLICKED(IDC_SET_SAVE_FOLDER, OnSetSaveFolder)
  ON_BN_CLICKED(IDC_DE_SET_SAVE_FOLDER, OnDESetSaveFolder)
  ON_BN_CLICKED(IDC_SETUP_FALCON_FRAMES, OnSetupFalconFrames)
  ON_BN_CLICKED(IDC_DE_SAVE_FRAMES, OnDeSaveFrames)
  ON_BN_CLICKED(IDC_DE_ALIGN_FRAMES, OnDeAlignFrames)
  ON_EN_KILLFOCUS(IDC_EDIT_DE_SUM_COUNT, OnKillfocusEditDeSumCount)
  ON_BN_CLICKED(IDC_BUT_FILE_OPTIONS, OnButFileOptions)
  ON_BN_CLICKED(IDC_BUT_NAME_SUFFIX, OnButFileOptions)
  ON_BN_CLICKED(IDC_SAVE_FRAME_SUMS, OnSaveK2FrameSums)
  ON_BN_CLICKED(IDC_SETUP_K2_FRAME_SUMS, OnSetupFalconFrames)
  ON_BN_CLICKED(IDC_ALWAYS_ANTIALIAS, OnAlwaysAntialias)
  ON_BN_CLICKED(IDC_BUT_SETUP_ALIGN, OnButSetupAlign)
  ON_BN_CLICKED(IDC_BUT_DE_SETUP_ALIGN, OnButSetupAlign)
  ON_BN_CLICKED(IDC_DE_SAVE_MASTER, OnDeSaveMaster)
  ON_EN_KILLFOCUS(IDC_EDIT_DE_FRAME_TIME, OnKillfocusDeFrameTime)
  ON_EN_KILLFOCUS(IDC_EDIT_DE_FPS, OnKillfocusEditDeFPS)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_DE_SUM_NUM, OnDeltaposSpinDeSumNum)
  ON_BN_CLICKED(IDC_CHECK_TAKE_K3_BINNED, OnTakeK3Binned)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCameraSetupDlg message handlers

void CCameraSetupDlg::SetFractionalSize(int fracX, int fracY)
{
  UpdateData(TRUE);   // Needed to preserve state of toggles
  m_eLeft = (fracX - 1) * mCameraSizeX / (2 * fracX) / mCoordScaling;
  m_eTop = (fracY - 1) * mCameraSizeY / (2 * fracY) / mCoordScaling;
  m_eRight = (fracX + 1) * mCameraSizeX / (2 * fracX) / mCoordScaling;
  m_eBottom = (fracY + 1) * mCameraSizeY / (2 * fracY) / mCoordScaling;
  if (mTietzBlocks || mParam->STEMcamera)
    AdjustCoords(mBinnings[m_iBinning]);
  UpdateData(FALSE);
  if (mParam->STEMcamera)
    ManageDrift();
  ManageTimingAvailable();
  DrawBox();
}

void CCameraSetupDlg::OnButfull() 
{
  SetFractionalSize(1, 1);
  m_butFull.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButhalf() 
{
  SetFractionalSize(2, 2);
  m_butHalf.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButquarter() 
{
  SetFractionalSize(4, 4);
  m_butQuarter.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButwidehalf() 
{
  SetFractionalSize(1, 2);
  m_butWideHalf.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButwidequarter() 
{
  SetFractionalSize(2, 4);
  m_butWideQuarter.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::SetAdjustedSize(int deltaX, int deltaY)
{
  int Left = mCoordScaling * m_eLeft;
  int Right = mCoordScaling * m_eRight;
  int Top = mCoordScaling * m_eTop;
  int Bottom = mCoordScaling * m_eBottom;
  BOOL update = false;
  if (Left - deltaX >= 0 && Right + deltaX <= mCameraSizeX &&
    2 * deltaX + Right - Left >= 16) {
    m_eLeft -= deltaX / mCoordScaling;
    m_eRight += deltaX / mCoordScaling;
    update = true;
  }
  if (Top - deltaY >= 0 && Bottom + deltaY <= mCameraSizeY &&
    2 * deltaY + Bottom - Top >= 16) {
    m_eTop -= deltaY / mCoordScaling;
    m_eBottom += deltaY / mCoordScaling;
    update = true;
  }
  if (AdjustCoords(mBinnings[m_iBinning]))
    update = true;
  if (update) {
    UpdateData(FALSE);
    DrawBox();
  }
}

void CCameraSetupDlg::OnButsmaller10() 
{
  int nearInd;
  UpdateData(TRUE);
  int deltaX = (int)(0.04545 * (m_eRight - m_eLeft) + 0.5) * mCoordScaling;
  int deltaY = (int)(0.04545 * (m_eBottom - m_eTop) + 0.5) * mCoordScaling;
  if (mTietzBlocks)
    deltaX = deltaY = mTietzBlocks / 2;
  if (mTietzSizes) {
    nearInd = mCamera->NearestTietzSizeIndex(m_eRight - m_eLeft, mTietzSizes, 
      mNumTietzSizes);
    deltaX = ((m_eRight - m_eLeft) - mTietzSizes[B3DMAX(nearInd - 1, 0)]) / 2;
    nearInd = mCamera->NearestTietzSizeIndex(m_eBottom - m_eTop, mTietzSizes, 
      mNumTietzSizes);
    deltaY =  ((m_eBottom - m_eTop) - mTietzSizes[B3DMAX(nearInd - 1, 0)]) / 2;
  }
  SetAdjustedSize(-deltaX, -deltaY);
  m_butSmaller10.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButbigger10() 
{
  int nearInd;
  UpdateData(TRUE);
  int deltaX = (int)(mCoordScaling * ((m_eRight - m_eLeft) / 20));
  int deltaY = (int)(mCoordScaling * ((m_eBottom - m_eTop) / 20));
  if (mTietzBlocks)
    deltaX = deltaY = mTietzBlocks / 2;
  if (mTietzSizes) {
    nearInd = mCamera->NearestTietzSizeIndex(m_eRight - m_eLeft, mTietzSizes, 
      mNumTietzSizes);
    deltaX = (mTietzSizes[B3DMIN(nearInd + 1, mNumTietzSizes - 1)] - (m_eRight - m_eLeft))
      / 2;
    nearInd = mCamera->NearestTietzSizeIndex(m_eBottom - m_eTop, mTietzSizes, 
      mNumTietzSizes);
    deltaY = (mTietzSizes[B3DMIN(nearInd + 1, mNumTietzSizes - 1)] - (m_eBottom - m_eTop))
      / 2;
  }
  SetAdjustedSize(deltaX, deltaY);
  m_butBigger10.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnButabitless() 
{
  int deltaX = -8, deltaY = -8;
  int sizeX = mCoordScaling * (m_eRight - m_eLeft);
  int sizeY = mCoordScaling * (m_eBottom - m_eTop);
  UpdateData(TRUE);
  if (mParam->moduloX > 0 && mParam->moduloY > 0 && 
    B3DMAX(mParam->moduloX, mParam->moduloY) > 8)
    deltaX = deltaY = -B3DMAX(mParam->moduloX, mParam->moduloY);
  if (mSquareForBitLess) {
    deltaX = deltaY = 0;
    if (sizeY < sizeX)
      deltaX = (sizeY - sizeX) / 2;
    else
      deltaY = (sizeX - sizeY) / 2;
  }
  SetAdjustedSize(deltaX, deltaY);
  m_butABitLess.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnShuttering() 
{
  UpdateData(TRUE);
  ManageDrift();
}

void CCameraSetupDlg::OnConSet() 
{
  UnloadDialogToConset();
  LoadConsetToDialog();
}

void CCameraSetupDlg::OnButcopycamera() 
{
  int setBase = mActiveCameraList[mCurrentCamera] * MAX_CONSETS;
  ControlSet *conSet = &mConSets[mCurrentSet + setBase];
  m_butCopyCamera.SetButtonStyle(BS_PUSHBUTTON);
  if (!mWinApp->GetUseViewForSearch() && mCurrentSet == SEARCH_CONSET)
    *conSet = mConSets[VIEW_CONSET + setBase];
  else
    mWinApp->TransferConSet(mCurrentSet, mActiveCameraList[mLastCamera],
      mActiveCameraList[mCurrentCamera]); 
  LoadConsetToDialog();
}

// Switch camera - keep track of last camera
void CCameraSetupDlg::OnRcamera() 
{
  mLastCamera = mCurrentCamera;
  UnloadDialogToConset();
  ManageCamera();
  LoadConsetToDialog();
}


void CCameraSetupDlg::OnBinning() 
{
  UpdateData(TRUE);
  if ((mTietzBlocks || mParam->STEMcamera) && AdjustCoords(mBinnings[m_iBinning])) {
    UpdateData(FALSE);
    DrawBox();
  }
  if (mParam->STEMcamera) {
    if (m_bKeepPixel) {
      mChangedBinning = true;
      OnKillfocusEditsettling();
      mChangedBinning = false;
    }
    ManageDrift();
  }
  if (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_BIN))
    m_butCorrectDrift.EnableWindow(m_iBinning > 0);
  ManageExposure();
  ManageBinnedSize();
  ManageTimingAvailable();
  ManageAntialias();
}

void CCameraSetupDlg::OnContSingle()
{
  UpdateData(TRUE);
  ManageIntegration();
  ManageAntialias();
}

void CCameraSetupDlg::OnLinesync()
{
  UpdateData(true);
  ManageDrift();
}

void CCameraSetupDlg::OnDynfocus()
{
  UpdateData(true);
  m_statTimingAvail.EnableWindow(m_bDynFocus);
}

void CCameraSetupDlg::OnProcessing() 
{
  int oldProc = m_iProcessing;
  UpdateData(TRUE);
  if (CheckFrameAliRestrictions(m_iK2Mode, mCamera->GetSaveUnnormalizedFrames(), 
    mUserSaveFrames, "setting for processing mode")) {
     m_iProcessing = oldProc;
     UpdateData(false);
     return;
  }

  m_butRemoveXrays.EnableWindow((!mParam->STEMcamera && m_iProcessing == GAIN_NORMALIZED)
    || (mParam->STEMcamera && m_iProcessing > 0));
  if (mParam->K2Type) {
    ManageExposure();
    ManageK2Binning();
    ManageK2SaveSummary();
    ManageDoseFrac();
    ManageDose();
  }
  if (mParam->OneViewType)
    ManageIntegration();
}

void CCameraSetupDlg::OnButrecenter() 
{
  int deltaY = mCameraSizeY / (2 * mCoordScaling) - (m_eTop + m_eBottom) / 2;
  int deltaX = mCameraSizeX / (2 * mCoordScaling) - (m_eLeft + m_eRight) / 2;
  m_eLeft += deltaX;
  m_eRight += deltaX;
  m_eTop += deltaY;
  m_eBottom += deltaY;
  if (mTietzBlocks || mTietzSizes)
    AdjustCoords(mBinnings[m_iBinning]);
  UpdateData(FALSE);
  DrawBox();
  m_butRecenter.SetButtonStyle(BS_PUSHBUTTON);
}

void CCameraSetupDlg::OnSwapXY()
{
  int temp = m_eLeft;
  m_eLeft = B3DMIN(m_eTop, mCameraSizeX / mCoordScaling);
  m_eTop = B3DMIN(temp, mCameraSizeY / mCoordScaling);
  temp = m_eRight;
  m_eRight = B3DMIN(m_eBottom, mCameraSizeX / mCoordScaling);
  m_eBottom = B3DMIN(temp, mCameraSizeY / mCoordScaling);
  if (mTietzBlocks || mTietzSizes)
    AdjustCoords(mBinnings[m_iBinning]);
  UpdateData(FALSE);
  DrawBox();
  m_butSwapXY.SetButtonStyle(BS_PUSHBUTTON);
}

// Shift box up or down.  The signs of these delta make no sense
void CCameraSetupDlg::OnDeltaposSpinupdown(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int sizeY = mCameraSizeY / mCoordScaling;
  int delta = pNMUpDown->iDelta * sizeY / SHIFT_FRACTION;
  if (mTietzSizes)
    delta = pNMUpDown->iDelta * mTietzOffsetModulo;
  if (mTietzBlocks)
    delta = pNMUpDown->iDelta * mTietzBlocks;
  if (m_eTop + delta < 0)
    delta = -m_eTop;
  if (m_eBottom + delta > sizeY)
    delta = sizeY - m_eBottom;
	*pResult = 1;
  if (!delta)
    return;
  m_eTop += delta;
  m_eBottom += delta;
  if (mTietzBlocks || mTietzSizes)
    AdjustCoords(mBinnings[m_iBinning]);
  UpdateData(FALSE);
  DrawBox();
}

// Shift box left or right
void CCameraSetupDlg::OnDeltaposSpinleftright(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int sizeX = mCameraSizeX / mCoordScaling;
  int delta = -pNMUpDown->iDelta * sizeX / SHIFT_FRACTION;
  if (mTietzSizes)
    delta = -pNMUpDown->iDelta * mTietzOffsetModulo;
  if (mTietzBlocks)
    delta = -pNMUpDown->iDelta * mTietzBlocks;
  if (m_eLeft + delta < 0)
    delta = -m_eLeft;
  if (m_eRight + delta > sizeX)
    delta = sizeX - m_eRight;
	*pResult = 1;
  if (!delta)
    return;
  m_eLeft += delta;
  m_eRight += delta;
  if (mTietzBlocks || mTietzSizes)
    AdjustCoords(mBinnings[m_iBinning]);
  UpdateData(FALSE);
  DrawBox();
}

// Update the dose
void CCameraSetupDlg::OnButupdatedose() 
{
  ManageDose();	
  m_butUpdateDose.SetButtonStyle(BS_PUSHBUTTON);
}

// Set the label with the binned size
void CCameraSetupDlg::ManageBinnedSize()
{
  int binning = mParam->binnings[m_iBinning];
  int left = mCoordScaling * m_eLeft / binning;
  int right = mCoordScaling * m_eRight / binning;
  int top = mCoordScaling * m_eTop / binning;
  int bottom = mCoordScaling * m_eBottom / binning;
  int sizeX = right - left;
  int sizeY = bottom - top;
  int magInd = GetMagIndexForCamAndSet();
  CString str;
  float pixel = (float)(binning * mWinApp->mShiftManager->GetPixelSize(
    mActiveCameraList[mCurrentCamera], magInd));

  mCamera->AdjustSizes(sizeX, mCameraSizeX, mParam->moduloX, left, right, 
    sizeY, mCameraSizeY, mParam->moduloY, top, bottom, 
    binning, mActiveCameraList[mCurrentCamera]);
  str.Format("Binned size: %d x %d", sizeX, sizeY);
  SetDlgItemText(IDC_STATSIZE, str);
  str.Format("%.2f x %.2f um @ " + mWinApp->PixelFormat(pixel * 1000.f), sizeX * pixel, 
    sizeY * pixel, pixel * 1000.);
  SetDlgItemText(IDC_STATMICRON, str);
}

// Returns either the currented mag index or the one for current camera and set
int CCameraSetupDlg::GetMagIndexForCamAndSet(void)
{
  int magInd, curLDind = 0, camLDind = 0, ldSet;
  LowDoseParams *ldParams;

  // Get the right mag in low dose mode for the selected camera
  if (mLowDoseMode) {
    ldSet = mCamera->ConSetToLDArea(m_iControlSet);
    curLDind = CamLDParamIndex();
    camLDind = CamLDParamIndex(mParam);
    if (camLDind == curLDind)
      ldParams = mWinApp->GetLowDoseParams() + ldSet;
    else
      ldParams = mWinApp->GetCamLowDoseParams() + MAX_LOWDOSE_SETS * camLDind + ldSet;
    magInd = ldParams->magIndex;

  } else
    magInd = mWinApp->mScope->FastMagIndex();
  return magInd;
}

void CCameraSetupDlg::OnOK() 
{
  mClosing = true;

  // This was before the OnKillfocusBTLR calls, so keep it before grabbing focus
  OnChangeCoord();

  // This gets OnKillfocus called for an edit box if it has focus.
  SetFocus();
  UnloadDialogToConset(); 
  mCamera->SetAntialiasBinning(m_bAlwaysAntialias);
  mCamera->SetTakeK3SuperResBinned(m_bTakeK3Binned);
  GetWindowPlacement(&mPlacement);
  CBaseDlg::OnOK();
}

void CCameraSetupDlg::OnAcquireReopen()
{
  mAcquireAndReopen = true;
  OnOK();
}

// Cancel: Restore last saved values of the FPS for the DE camera, save placement
void CCameraSetupDlg::OnCancel() 
{
  if (mWinApp->mDEToolDlg.CanSaveFrames(mParam)) {
    mParam->DE_FramesPerSec = mSaveLinearFPS;
    mParam->DE_CountingFPS = mSaveCountingFPS;
  }
  GetWindowPlacement(&mPlacement);
  CBaseDlg::OnCancel();
}

static int binVals[MAX_BINNINGS] = {1, 2, 3, 4, 6, 8};

// Load the current control set into the dialog
void CCameraSetupDlg::LoadConsetToDialog()
{
  int i, indMin, binning, showProc, lockSet;
  bool noDark, noGain, alwaysAdjust, noRaw, show, canCopyView;
  int *modeP = mParam->DE_camType ? &m_iDEMode : &m_iK2Mode;
  CButton *radio;
  MontParam *montp = mWinApp->GetMontParam();
  mCurSet = &mConSets[mCurrentSet + mActiveCameraList[mCurrentCamera] * MAX_CONSETS];
  
  lockSet = MontageConSetNum(montp, false, montp->setupInLowDose);
  mBinningEnabled = !((mCurrentSet == RECORD_CONSET && mStartedTS || 
    (mCurrentSet == lockSet && mMontaging && 
      montp->cameraIndex == mCurrentCamera && !mLowDoseMode)));
  mCamera->FindNearestBinning(mParam, mCurSet, m_iBinning, binning);

    // If this is Record set, disable the binning if doing TS, or if montaging and either
    // in low dose mode or this is the montage camera
  for (i = 0; i < mNumBinnings; i++) {
    radio = (CButton *)GetDlgItem(i + IDC_RBIN1);
    radio->EnableWindow(mBinningEnabled);
  }

  // Fix exposure and frame times in control set before loading
  mCamera->ConstrainExposureTime(mParam, mCurSet);
  m_eLeft = mCurSet->left / mCoordScaling;
  m_eRight = mCurSet->right / mCoordScaling;
  m_eTop = mCurSet->top / mCoordScaling;
  m_eBottom = mCurSet->bottom / mCoordScaling;
  m_bDoseFracMode = B3DCHOICE(mParam->K2Type, mCurSet->doseFrac > 0, 
    mCamera->GetFrameSavingEnabled() || (FCAM_ADVANCED(mParam)&&IS_FALCON2_OR_3(mParam)));
  if (mWeCanAlignFalcon && !FCAM_CAN_ALIGN(mParam) && !mCurSet->useFrameAlign)
    mCurSet->useFrameAlign = 1;
  m_fFrameTime = mCurSet->frameTime;
  m_fDEframeTime = RoundedDEframeTime(m_fFrameTime);
  m_bAlignDoseFrac = mCurSet->alignFrames > 0 || 
    (mCurrentSet == RECORD_CONSET && mWinApp->mTSController->GetFrameAlignInIMOD());
  mUserSaveFrames = m_bSaveFrames = mCurSet->saveFrames > 0;
  m_bSaveK2Sums = mCurSet->sumK2Frames > 0 && 
    mCamera->CAN_PLUGIN_DO(CAN_SUM_FRAMES, mParam);
  if (mParam->K2Type == K2_BASE)
    mCurSet->K2ReadMode = 0;
  if (mParam->K2Type == K3_TYPE && mCurSet->K2ReadMode > 0)
    mCurSet->K2ReadMode = COUNTING_MODE;
  *modeP = mCurSet->K2ReadMode;
  if (CamHasDoubledBinnings(mParam)) {
    radio = (CButton *)GetDlgItem(IDC_RBIN1);
    radio->EnableWindow(mBinningEnabled && IS_SUPERRES(mParam, *modeP));
  }
  if (mParam->OneViewType)
    m_bCorrectDrift = mCurSet->alignFrames != 0;
  else if (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_BIN))
    m_bCorrectDrift = mCurSet->boostMag != 0;
  if (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_ROI))
    m_bUseHardwareROI = mCurSet->magAllShots != 0;

  mSummedFrameList = mCurSet->summedFrameList;
  mNumSkipBefore = mCurSet->numSkipBefore;
  mNumSkipAfter = mCurSet->numSkipAfter;
  mUserFrameFrac = mCurSet->userFrameFractions;
  mUserSubframeFrac = mCurSet->userSubframeFractions;

  // For DE camera, set checkboxes by the flags and the appropriate sum count and FPS
  // for the mode, and save the starting values of FPS
  if (mWinApp->mDEToolDlg.CanSaveFrames(mParam)) {
    m_bDEsaveMaster = (mCurSet->saveFrames & DE_SAVE_MASTER) != 0;
    m_bDEsaveFrames = (mCurSet->saveFrames & DE_SAVE_SINGLE) != 0;
    m_bDEsaveFinal = (mCurSet->saveFrames & DE_SAVE_FINAL) != 0;
    mUserSaveFrames = m_bDEsaveMaster;
    if (!(mParam->CamFlags & DE_CAM_CAN_COUNT))
      *modeP = 0;
    m_iSumCount = (*modeP > 0) ? mCurSet->sumK2Frames : mCurSet->DEsumCount;
    m_fDEfps = (*modeP > 0 && mParam->DE_CountingFPS > 0.) ? mParam->DE_CountingFPS : 
      mParam->DE_FramesPerSec;
    m_bDEalignFrames = m_bAlignDoseFrac;
    mSaveLinearFPS = mParam->DE_FramesPerSec;
    mSaveCountingFPS = mParam->DE_CountingFPS;
    ManageDEpanel();
  }
  B3DCLAMP(m_iSumCount, 1, MAX_DE_SUM_COUNT);

  // Adjust the size in case of restricted sizes
  alwaysAdjust = (mParam->STEMcamera && mParam->FEItype) || 
    (mParam->K2Type && m_bDoseFracMode);
  if (mParam->moduloX < 0 || mParam->centeredOnly || mParam->squareSubareas || 
    mTietzBlocks || alwaysAdjust)
    AdjustCoords(binning, alwaysAdjust);

  m_iContSingle = 1 - mCurSet->mode;
  m_iShuttering = mCurSet->shuttering;
  m_iControlSet = mCurrentSet;
  B3DCLAMP(mCurrentCamera, 0, MAX_DLG_CAMERAS - 1);
  m_iCamera = mCurrentCamera;
  m_eExposure = mCurSet->exposure;
  ManageExposure(false);
  m_eSettling = mCurSet->drift;
  if (mParam->K2Type)
    m_strSettling.Format("%.4f", m_eSettling);
  else if (!mParam->STEMcamera)
    m_strSettling.Format("%.2f", m_eSettling);
  m_bDarkAlways = mCurSet->forceDark > 0;
  m_bDarkNext = mCurSet->onceDark > 0;
  m_bAverageDark = mCurSet->averageDark > 0;
  m_bLineSync = mCurSet->lineSync > 0;
  m_bDynFocus = mCurSet->dynamicFocus > 0;
  m_iIntegration = mCurSet->integration;
  B3DCLAMP(m_iIntegration, 1, mMaxIntegration);

  // Initialize dark ref averaging to defaults if out of range
  if (mCurSet->numAverage < 2)
    mCurSet->numAverage = mCurrentSet == 3 ? 10 : 4;
  m_iAverageTimes = mCurSet->numAverage;
  B3DCLAMP(m_iAverageTimes, 2, MAX_DARK_AVERAGE);
  m_spinAverage.SetRange(2, MAX_DARK_AVERAGE);
  m_spinAverage.SetPos(m_iAverageTimes);
  m_sBigText = mModeNames[mCurrentSet];
  
  // If beam shutter and beam is not OK, or if in either DM shutter mode and settling
  // is not OK and there is settling, kick it into nonDM shutter mode
  if (mGatanType && ((m_iShuttering == USE_BEAM_BLANK && !mDMbeamShutterOK) || 
    (m_iShuttering != USE_DUAL_SHUTTER && !mDMsettlingOK && m_eSettling > 0.)))
    m_iShuttering = mCurSet->shuttering = USE_DUAL_SHUTTER;

  // For Tietz camera or advanced/K2, make sure third option is converted to beam shutter
  // For AMT and FEI do this unconditionally
  // For plugin camera, convert second or third option based on number of shutters
  if (((mTietzType || mParam->K2Type || mParam->OneViewType) && 
    m_iShuttering == USE_DUAL_SHUTTER) || mAMTtype || mFEItype || 
    (mPluginType && ((mNumPlugShutters < 2 && m_iShuttering != USE_BEAM_BLANK) ||
    (mNumPlugShutters < 3 && m_iShuttering == USE_DUAL_SHUTTER))))
    m_iShuttering = mCurSet->shuttering = USE_BEAM_BLANK;

  if (mParam->STEMcamera) {
    mCurSet->processing = UNPROCESSED;
    m_iProcessing = mCurSet->boostMag;
    m_bRemoveXrays = mCurSet->magAllShots > 0;
    showProc = (mCurrentSet == FOCUS_CONSET && mParam->GatanCam) ? SW_SHOW : SW_HIDE;
    m_butUnprocessed.EnableWindow(!mLowDoseMode);
    m_butGainNormalize.EnableWindow(!mLowDoseMode);
    m_butDarkSubtract.EnableWindow(!mLowDoseMode);
    m_butBoostMag3.EnableWindow(!mLowDoseMode);
    m_butBoostMag4.EnableWindow(!mLowDoseMode);
    m_butRemoveXrays.EnableWindow(!mLowDoseMode && m_iProcessing > 0);
    m_butRemoveXrays.ShowWindow(showProc);
  } else {

    // Manage the dark subtract button for FEI, promote to gain normalized if disabled
    // Manage nonSTEM enables of these buttons here, show/hide and STEM enables in 
    // ManageProcessing, where is varies by set
    m_iProcessing = mCurSet->processing;
    m_bRemoveXrays = mCurSet->removeXrays > 0;
    noDark = mFEItype || (mPluginType && mCanProcess && !(mCanProcess & DARK_SUBTRACTED));
    noRaw = mFEItype && FCAM_ADVANCED(mParam) && mCamera->GetASIgivesGainNormOnly();
    m_butDarkSubtract.EnableWindow((mParam->processHere || !noDark) && !noRaw);
    m_butUnprocessed.EnableWindow(!noRaw);
    if ((noDark && !mParam->processHere && m_iProcessing == DARK_SUBTRACTED) || noRaw)
      m_iProcessing = mCurSet->processing = GAIN_NORMALIZED;

    // Do same for plugin with no gain normalization, demote to DS
    noGain = mPluginType && mCanProcess && !(mCanProcess & GAIN_NORMALIZED);
    m_butGainNormalize.EnableWindow(!noGain);
    if (noGain && mCurSet->processing == GAIN_NORMALIZED)
      m_iProcessing = mCurSet->processing = DARK_SUBTRACTED;
    
    showProc = SW_SHOW;
    m_butRemoveXrays.EnableWindow(m_iProcessing == GAIN_NORMALIZED);
    m_butUnprocessed.ShowWindow(true);
  }
  m_butUnprocessed.ShowWindow(showProc);
  m_butDarkSubtract.ShowWindow(showProc);
  m_butGainNormalize.ShowWindow(showProc);
  m_butBoostMag3.ShowWindow(showProc && mParam->STEMcamera);
  m_butBoostMag4.ShowWindow(showProc && mParam->STEMcamera);
  m_statProcessing.ShowWindow(showProc);

  if (mParam->STEMcamera) {
    
    // Set the combo box selections to unique legal values
    indMin = mCamera->GetMaxChannels(mParam);
    for (i = 0; i < indMin; i++) {
      CComboBox *combo = (CComboBox *)GetDlgItem(IDC_COMBOCHAN1 + i);
      int sel = mCurSet->channelIndex[i] + (indMin > 1 ? 1 : 0);
      if (sel < 0 || sel > mParam->numChannels)
        sel = 0;
      for (int j = 0; sel !=0 && j < i; j++)
        if (mCamera->MutuallyExclusiveChannels(sel, mCurSet->channelIndex[j] + 1))
          sel = 0;
      combo->SetCurSel(sel);
    }
  }
  
  if (mParam->K2Type || mFalconCanSave)
    ManageDoseFrac();
  if (mLowDoseMode && mWinApp->GetUseViewForSearch()) {
    if (mCurrentSet == VIEW_CONSET) {
      if (mWinApp->mScope->GetLowDoseArea() == SEARCH_AREA)
        SetDlgItemText(IDC_ACQUIRE_REOPEN, "Acquire Search");
      else
        SetDlgItemText(IDC_ACQUIRE_REOPEN, (LPCTSTR)(CString("Acquire ") +mModeNames[0]));
    } else
      SetDlgItemText(IDC_ACQUIRE_REOPEN, "Acquire");
  }

  canCopyView = !mWinApp->GetUseViewForSearch() && mCurrentSet == SEARCH_CONSET;
  show = mNumCameras > 1 || canCopyView;
  m_butCopyCamera.ShowWindow(show ? SW_SHOW : SW_HIDE);
  m_statCopyCamera.ShowWindow(show ? SW_SHOW : SW_HIDE);
  if (canCopyView)
    m_strCopyCamera = mModeNames[VIEW_CONSET];
  else if (mNumCameras > 1)
    m_strCopyCamera = mCamParam[mActiveCameraList[mLastCamera]].name;

  UpdateData(false);  
  ManageDrift();
  ManageTimingAvailable();
  ManageDose();
  ManageK2Binning();
  ManageAntialias();
  ManageK2SaveSummary();
  ManageIntegration();
  ManageDarkRefs();
  DrawBox();
}

// Unload the data in the dialog into the current control set
void CCameraSetupDlg::UnloadDialogToConset()
{
  int *modeP = mParam->DE_camType ? &m_iDEMode : &m_iK2Mode;
  UpdateData(TRUE);
  mCurSet->binning = mBinnings[m_iBinning];
  mCurSet->mode = 1 - m_iContSingle ;
  mCurSet->shuttering = m_iShuttering;
  mCurSet->exposure = ManageExposure(false);
  if (!mParam->STEMcamera) {
    m_eSettling = (float)atof(m_strSettling);
    B3DCLAMP(m_eSettling, 0.f, 10.f);
  }
  mCurSet->drift = m_eSettling;
  mCurSet->left = m_eLeft * mCoordScaling;
  mCurSet->right = m_eRight * mCoordScaling;
  mCurSet->top = m_eTop * mCoordScaling;
  mCurSet->bottom = m_eBottom * mCoordScaling;
  mCurSet->forceDark = m_bDarkAlways ? 1 : 0;
  mCurSet->onceDark = m_bDarkNext ? 1 : 0;
  mCurSet->averageDark = m_bAverageDark ? 1 : 0;
  mCurSet->numAverage = m_iAverageTimes;
  mCurSet->K2ReadMode =  *modeP;
  if (mParam->K2Type)
    mCurSet->doseFrac = m_bDoseFracMode ? 1 : 0;
  mCurSet->frameTime = ActualFrameTime(m_fFrameTime);
  mCurSet->saveFrames = mUserSaveFrames ? 1 : 0;
  if (!mWinApp->mDEToolDlg.CanSaveFrames(mParam))
    mCurSet->sumK2Frames = m_bSaveK2Sums ? 1 : 0;
  mCurSet->summedFrameList = mSummedFrameList;
  mCurSet->numSkipBefore = mNumSkipBefore;
  mCurSet->numSkipAfter = mNumSkipAfter;
  mCurSet->userFrameFractions = mUserFrameFrac;
  mCurSet->userSubframeFractions = mUserSubframeFrac;

  // DE camera: Recompose the save flags, put sum count in appropriate spot, and update
  // the saved FPS values
  if (mWinApp->mDEToolDlg.CanSaveFrames(mParam)) {
    mCurSet->saveFrames = (m_bDEsaveFrames ? DE_SAVE_SINGLE : 0) +
      (mUserSaveFrames ? DE_SAVE_MASTER : 0) + (m_bDEsaveFinal ? DE_SAVE_FINAL : 0);
    if (*modeP)
      mCurSet->sumK2Frames = m_iSumCount;
    else
      mCurSet->DEsumCount = m_iSumCount; 
    mSaveLinearFPS = mParam->DE_FramesPerSec;
    mSaveCountingFPS = mParam->DE_CountingFPS;
    m_bAlignDoseFrac = m_bDEalignFrames;
  }
  if (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_BIN))
    mCurSet->boostMag = m_bCorrectDrift ? 1 : 0;
  if (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_ROI))
    mCurSet->magAllShots = m_bUseHardwareROI ? 1 : 0;

  // First set alignFrames for direct detectors, then override that for OneView
  if (!(mCurrentSet == RECORD_CONSET && mWinApp->mTSController->GetFrameAlignInIMOD()))
    mCurSet->alignFrames = m_bAlignDoseFrac ? 1 : 0;
  if (mParam->OneViewType)
    mCurSet->alignFrames = m_bCorrectDrift ? 1 : 0;

  mCurSet->lineSync = m_bLineSync ? 1 : 0;
  mCurSet->dynamicFocus = m_bDynFocus ? 1 : 0;
  mCurSet->integration = m_iIntegration;
  if (mParam->STEMcamera) {
    mCurSet->boostMag = m_iProcessing;
    mCurSet->magAllShots = m_bRemoveXrays ? 1 : 0;
    for (int i = 0; i < mCamera->GetMaxChannels(mParam); i++) {
      CComboBox *combo = (CComboBox *)GetDlgItem(IDC_COMBOCHAN1 + i);
      int sel = combo->GetCurSel();
      if (sel == CB_ERR)
        sel = -1;
      else if (mCamera->GetMaxChannels(mParam) > 1)
        sel--;
      mCurSet->channelIndex[i] = sel;
    }
  } else {
    mCurSet->processing = m_iProcessing;
    mCurSet->removeXrays = m_bRemoveXrays ? 1 : 0;
  }

  // After accessing current data, set possibly new camera and set number, refresh param
  mCurrentSet = m_iControlSet;
  mCurrentCamera = m_iCamera;
  mParam = &mCamParam[mActiveCameraList[mCurrentCamera]];
}

// Make sure the exposure time fits constraints, round it to nice value in some cases for
// display and return the true exposure time
float CCameraSetupDlg::ManageExposure(bool updateIfChange)
{
  float realExp = m_eExposure;
  int *modeP = mDE_Type ? &m_iDEMode : &m_iK2Mode;
  bool saySaving, savingDE = m_bDEsaveMaster || m_bDEalignFrames;

  // General call and computations
  BOOL changed = mCamera->ConstrainExposureTime(mParam, m_bDoseFracMode, *modeP, 
    mBinnings[m_iBinning], m_bAlignDoseFrac && !mCurSet->useFrameAlign, 
    savingDE ? m_iSumCount : 1, realExp, m_fFrameTime);

  float roundFac = mCamera->ExposureRoundingFactor(mParam);
  if (roundFac) {
    m_eExposure = (float)(B3DNINT(realExp * roundFac) / roundFac);
    changed = changed || fabs(m_eExposure - realExp) > 1.e-5;
  } else
    m_eExposure = realExp;

  if (mParam->K2Type == K3_TYPE)
    m_fFrameTime = RoundedDEframeTime(m_fFrameTime);

  // Special for DE
  if (mWinApp->mDEToolDlg.CanSaveFrames(mParam)) {
    m_fDEframeTime = RoundedDEframeTime(m_fFrameTime * (savingDE ? 1 : m_iSumCount));
    int frames = (int)floor(realExp / m_fFrameTime) + 1;
    m_statNumDEraw.ShowWindow(savingDE ? SW_SHOW : SW_HIDE);
    changed = savingDE;
    saySaving = m_bDEsaveMaster || (m_bDEalignFrames && mCurSet->useFrameAlign > 1);
    if (savingDE)
      m_strNumDEraw.Format("%d frames will be %s%s%s", frames, 
        saySaving ? "saved" : "", saySaving && m_bDEalignFrames ? "/" : "",
        m_bDEalignFrames ? "aligned" : "");
  }
  if (changed && updateIfChange)
    ManageDose();

  // Special for variable-size sum saving, K2 or Falcon
  if (changed && m_bDoseFracMode && (mFalconCanSave || 
    (mParam->K2Type && m_bSaveK2Sums && m_bSaveFrames))) {
      mWinApp->mFalconHelper->AdjustForExposure(mSummedFrameList, 
        mFalconCanSave ? mNumSkipBefore : 0, mFalconCanSave ? mNumSkipAfter : 0, realExp, 
        mFalconCanSave ? mCamera->GetFalconReadoutInterval() : m_fFrameTime, 
        mUserFrameFrac, mUserSubframeFrac, mFalconCanSave && FCAM_CAN_ALIGN(mParam) &&
        m_bAlignDoseFrac && !mCurSet->useFrameAlign);
      ManageK2SaveSummary();
  }
  return realExp;
}

//  Set the minimum drift settling text based on shuttering mode
void CCameraSetupDlg::ManageDrift(bool useMinRate)
{
  CString limit, report;
  TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
  BOOL recForTS = mWinApp->StartedTiltSeries() && tsParam->cameraIndex == mCurrentCamera
    && mCurrentSet == RECORD_CONSET;
  BOOL driftOK = !(recForTS && mWinApp->mTSController->GetTypeVaries(TS_VARY_DRIFT));
  mMinimumSettling = (m_iShuttering == USE_DUAL_SHUTTER || mTietzType > 0 || mAMTtype > 0
    || mFEItype > 0 || mDE_Type > 0 || mPluginType || mParam->K2Type) ? mMinDualSettling : 
    mBuiltInSettling;

  if (mParam->STEMcamera) {

    // For STEM, get the pixel time and revise the exposure time
    int left, right, top, bottom, itmp, binning = mBinnings[m_iBinning];
    double pixelTime, scanRate;
    float expTmp, flyback = mParam->flyback;
    float pixelSize = binning * mWinApp->mShiftManager->GetPixelSize(
      mActiveCameraList[mCurrentCamera], GetMagIndexForCamAndSet());
    left = m_eLeft / binning;
    right = m_eRight / binning;
    top = m_eTop / binning;
    bottom = m_eBottom / binning;

    if (useMinRate && mParam->maxScanRate)
      m_eExposure = 0.01f;
    mCamera->ComputePixelTime(mParam, right - left, bottom - top, 
      (m_bLineSync && !mParam->FEItype) ? 1 : 0,
      pixelSize, useMinRate ? mParam->maxScanRate : 0.f, m_eExposure, pixelTime,scanRate);
    if (pixelTime < 9.99)
      m_strSettling.Format("%.2f", pixelTime);
    else
      m_strSettling.Format("%.1f", pixelTime);
    m_eExposure = (float)(0.001 * B3DNINT(m_eExposure * 1000.));
    if (mParam->FEItype)
      GetFEIflybackTime(flyback);
    m_butDynFocus.EnableWindow(mCamera->DynamicFocusOK(m_eExposure, bottom - top,
      mParam->flyback, itmp, pixelTime) > 0);
    expTmp = 0.01f;
    m_strScanRate.Format("Scan rate = %.2f  um/msec", scanRate);
    if (mParam->maxScanRate > 0. && scanRate > mParam->maxScanRate + 0.001)
      m_strScanRate += " ***";
    else if (mParam->advisableScanRate > 0. && scanRate > mParam->advisableScanRate + 
      0.001)
      m_strScanRate += " *";
    mCamera->ComputePixelTime(mParam, right - left, bottom - top, 
      (m_bLineSync && !mParam->FEItype) ? 1 : 0,
      pixelSize, mParam->maxScanRate, expTmp, pixelTime, scanRate);
    UpdateData(false);
  } else {

    // Enable drift edit box if settling is OK in all modes or non-DM mode
    // or if Tietz beam shutter
    if (mCamera->CanPreExpose(mParam, m_iShuttering)) {
      if (mParam->K2Type)
        limit.Format("Minimum %.4f if not 0.0", mCamera->GetK2ReadoutInterval
        (mParam->K2Type));
      else
        limit.Format("Minimum %.2f if not 0.0", mMinimumSettling);
      m_driftText1.EnableWindow(true);
      m_driftText2.EnableWindow(true);
      m_driftEdit.EnableWindow(driftOK);
    } else {
      limit = " ";
      m_driftText1.EnableWindow(false);
      m_driftText2.EnableWindow(false);
      m_driftEdit.EnableWindow(false);
      if (m_eSettling && !mParam->OneViewType) {
        m_eSettling = 0.;
        m_strSettling = "0.0";
        UpdateData(false);
      }
    }
    if (mMinimumSettling == 0.0)
      limit = " ";
    SetDlgItemText(IDC_MINIMUM_DRIFT, limit);

    // For K2, constrain settling to multiple of frame time
    if (m_eSettling > 0.0 && mParam->K2Type) {
      float newSettling = (float)(mCamera->GetK2ReadoutInterval(mParam->K2Type) * 
        B3DNINT(m_eSettling / mCamera->GetK2ReadoutInterval(mParam->K2Type)));
      if (fabs(newSettling - m_eSettling) > 1.e-5) {
        m_eSettling = newSettling;
        m_strSettling.Format("%.4f", m_eSettling);
        UpdateData(false);
      }

      // Otherwise If settling nonzero, constrain it to minimum
    } else if (m_eSettling > 0.0 && m_eSettling < mMinimumSettling) {
      m_eSettling = mMinimumSettling;
      m_strSettling.Format("%.2f", m_eSettling);
      UpdateData(false);
    }

    // And if settling not OK in DM, enable/disable the DM film button
    if (!mDMsettlingOK) {
      CButton *radio = (CButton *)GetDlgItem(IDC_RFILMONLY);
      radio->EnableWindow(m_eSettling == 0.0);
      radio = (CButton *)GetDlgItem(IDC_RBEAMONLY);
      radio->EnableWindow(mDMbeamShutterOK && m_eSettling == 0.0);
    }
  }
  m_editExposure.EnableWindow(!(recForTS && 
    mWinApp->mTSController->GetTypeVaries(TS_VARY_EXPOSURE)));
  if (mParam->K2Type)
    m_editFrameTime.EnableWindow(m_bDoseFracMode && !(recForTS && 
    mWinApp->mTSController->GetTypeVaries(TS_VARY_FRAME_TIME)));
}

// Take care of many camera-dependent items
void CCameraSetupDlg::ManageCamera()
{
  int i, j, dir, err, showbox, maxChan, visTop, lastTop, areaHeight = mAreaBaseHeight;
  int canConfig, state;
  BOOL states[NUM_CAMSETUP_PANELS] = {0, true, 0, 0, 0, 0, 0, 0, true};
  bool twoRowPos;
  int hideForFalcon[] = {IDC_STAT_FRAME_TIME, IDC_EDIT_FRAME_TIME, IDC_STAT_FRAME_SEC,
    IDC_STAT_K2MODE, IDC_BUT_SETUP_ALIGN, IDC_RLINEAR, IDC_RCOUNTING, IDC_RSUPERRES, 
    IDC_ALIGN_DOSE_FRAC, IDC_STAT_ANTIALIAS, IDC_STAT_SAVE_SUMMARY, IDC_SAVE_FRAME_SUMS,
    IDC_SETUP_K2_FRAME_SUMS, IDC_ALWAYS_ANTIALIAS, IDC_STAT_ALIGN_SUMMARY, 
    IDC_CHECK_TAKE_K3_BINNED};
  CButton *radio;
  CString strBin;
  CComboBox *combo;
  CStatic *stat;
  CWnd *win;
  CRect rect;
  ControlSet *conSets = &mConSets[mActiveCameraList[mCurrentCamera] * MAX_CONSETS];
  CString title = "Camera Parameters   --   " + mParam->name;
  mWeCanAlignFalcon = mCamera->CanWeAlignFalcon(mParam, true, mFalconCanSave); 
  states[0] = mNumCameras > 1;
  states[2] = !mParam->STEMcamera && !IS_FALCON2_OR_3(mParam);  // Shutter and other sizes
  states[4] = !mParam->STEMcamera;    // Dose
  states[5] = mParam->STEMcamera;
  states[6] = mParam->K2Type || mFalconCanSave;
  states[7] = mWinApp->mDEToolDlg.CanSaveFrames(mParam);
  SetWindowText((LPCTSTR)title);
  mCameraSizeX = mParam->sizeX;
  mCameraSizeY = mParam->sizeY;
  mTietzType = mParam->TietzType;
  mTietzBlocks = mTietzType ? mParam->TietzBlocks : 0;
  mTietzSizes = mCamera->GetTietzSizes(mParam, mNumTietzSizes, mTietzOffsetModulo);
  mPluginType = !mParam->pluginName.IsEmpty();
  if (mPluginType) {
    mPlugCanPreExpose = mParam->canPreExpose;
    mNumPlugShutters = 1;
    if (mParam->noShutter)
      mNumPlugShutters = 0;
    else if (!mParam->onlyOneShutter && !mParam->shutterLabel2.IsEmpty() && 
      !mParam->shutterLabel3.IsEmpty())
      mNumPlugShutters = 3;
    else if (!mParam->onlyOneShutter && !mParam->shutterLabel2.IsEmpty())
      mNumPlugShutters = 2;
  }

  mDE_Type = mParam->DE_camType;
  mTietzCanPreExpose = mParam->TietzCanPreExpose;
  mAMTtype = mParam->AMTtype;
  mFEItype = mParam->FEItype;
  mGatanType = mParam->GatanCam;
  mCanProcess = 0;
  if (mGatanType)
    mCanProcess = GAIN_NORMALIZED | DARK_SUBTRACTED;
  else if (mFEItype)
    mCanProcess = GAIN_NORMALIZED;
  else if (mPluginType)
    mCanProcess = mParam->canDoProcessing;

  // Dark reference section
  states[3] = !mParam->STEMcamera && mCamera->CanProcessHere(mParam) && 
    (mParam->processHere || !mCanProcess || mGatanType);
  
  mBuiltInSettling = mParam->builtInSettling;
  mDMbeamShutterOK = mParam->DMbeamShutterOK || !mParam->GatanCam;
  mMinDualSettling = mParam->K2Type ? mCamera->GetK2ReadoutInterval(mParam->K2Type) : 
    mParam->minimumDrift;
  mMinExposure = mParam->minExposure;
  mDMsettlingOK = mParam->DMsettlingOK;
  mNumBinnings = mParam->numBinnings;
  for (i = 0; i < mParam->numBinnings; i++)
    mBinnings[i] = mParam->binnings[i];
  mCoordScaling = BinDivisorI(mParam);
  mMaxIntegration = B3DMAX(1, mParam->maxIntegration);
  m_spinIntegration.SetRange(1, 10000);
  m_spinIntegration.SetPos(5000);

  leftTable[mDoseFracTableInd] = mFalconCanSave ? mDoseFracLeftFalcon : mDoseFracLeftOrig;
  m_butDoseFracMode.SetWindowPos(NULL, 0, 0,
    mFalconCanSave ? mDoseFracWidthOrig : mDoseFracWidthOrig * 6 / 10, mDoseFracHeight,
    SWP_NOMOVE);

  if (mParam->STEMcamera && mParam->FEItype)
    mWinApp->mScope->GetFEIChannelList(mParam, true);
  twoRowPos = mParam->STEMcamera && mParam->moduloX >= 0;
  dir = 0;
  if (mShiftedPosButtons && !twoRowPos) {
    dir = 1;
    mShiftedPosButtons = false;
  } else if (!mShiftedPosButtons && twoRowPos) {
    dir = -1;
    mShiftedPosButtons = true;
  }
  for (i = 0; i < 3; i++)
    topTable[mPosButtonIndex[i]] += dir * mUpperPosShift;

  // Reform the window for a STEM / noSTEM change: all shows/hides must be done below this
  AdjustPanels(states, idTable, leftTable, topTable, mNumInPanel, mPanelStart, 
    mNumCameras);
  m_statBox.ShowWindow(SW_HIDE);

  // Repair effects of adjusting the panels

  // Set up the binning buttons and resize the group box
  for (i = 0; i < MAX_BIN_BUTTONS; i++) {
    radio = (CButton *)GetDlgItem(i + IDC_RBIN1);
    radio->ShowWindow(i < mNumBinnings ? SW_SHOW : SW_HIDE);
    if (i < mNumBinnings) {
      SetDlgItemText(i + IDC_RBIN1, mWinApp->BinningText(mBinnings[i], mParam));
    }
    if (i == mNumBinnings - 1) {
      radio->GetWindowRect(&rect);
      visTop = rect.top;
    }
    if (i == MAX_BIN_BUTTONS - 1) {
      radio->GetWindowRect(&rect);
      lastTop = rect.top;
    }
  }
  m_statBinning.SetWindowPos(NULL, 0, 0, mBinWidth, mBinFullHeight + visTop - lastTop,
    SWP_NOMOVE);

  ShowDlgItem(IDC_RSEARCH, !mWinApp->GetUseViewForSearch());
  ShowDlgItem(IDC_RMONTAGE, !mWinApp->GetUseRecordForMontage());

  // Disable beam shutter if that is not controllable
  radio = (CButton *)GetDlgItem(IDC_RBEAMONLY);
  radio->EnableWindow(mDMbeamShutterOK);
  SetDlgItemText(IDC_RBEAMONLY, B3DCHOICE(((mTietzType && mTietzCanPreExpose) || mAMTtype
    || mFEItype == EAGLE_TYPE) && !mParam->noShutter, 
    "Chosen based on drift settling", "Beam blanking only"));

  // Set film shutter text for camera type, and show dual shuttering for Gatan
  SetDlgItemText(IDC_RFILMONLY, mTietzType ? "Film shutter - beam left ON" 
    : "DM film shutter with beam blanking");
  if (mPluginType && mNumPlugShutters > 1)
    SetDlgItemText(IDC_RFILMONLY, mParam->shutterLabel2);
  radio = (CButton *)GetDlgItem(IDC_RFILMONLY);
  radio->ShowWindow(!(mParam->onlyOneShutter || mAMTtype || mFEItype || mParam->STEMcamera
    || mDE_Type || (mPluginType && mNumPlugShutters < 2)) ? SW_SHOW : SW_HIDE);
  radio = (CButton *)GetDlgItem(IDC_RSHUTTERCOMBO);
  radio->ShowWindow(((mParam->GatanCam && !mParam->onlyOneShutter && 
    !mParam->STEMcamera && !mParam->K2Type && !mParam->OneViewType) || 
    (mPluginType && mNumPlugShutters == 3))? SW_SHOW : SW_HIDE);
  if (mPluginType) {
    if (!mParam->shutterLabel1.IsEmpty())
      SetDlgItemText(IDC_RBEAMONLY, mParam->shutterLabel1);
    if (mNumPlugShutters > 1)
      SetDlgItemText(IDC_RFILMONLY, mParam->shutterLabel2);
    if (mNumPlugShutters > 2)
      SetDlgItemText(IDC_RSHUTTERCOMBO, mParam->shutterLabel3);
  }

  // Manage lower pane for K2 versus Falcon
  for (i = 0; i < sizeof(hideForFalcon) / sizeof(int); i++) {
    win = GetDlgItem(hideForFalcon[i]);
    win->ShowWindow(mParam->K2Type ? SW_SHOW : SW_HIDE);
  }
  m_butSetupFalconFrames.ShowWindow(mFalconCanSave ? SW_SHOW : SW_HIDE);
  canConfig = mFalconCanSave ? mCamera->GetCanUseFalconConfig() : -1;
  if (mFalconCanSave && FCAM_ADVANCED(mParam)) {
    canConfig = -2;
    m_bDoseFracMode = true;
    if (FCAM_CAN_COUNT(mParam)) {
      ShowDlgItem(IDC_STAT_K2MODE, true);
      ShowDlgItem(IDC_RLINEAR, true);
      ShowDlgItem(IDC_RCOUNTING, true);
    }
  }
  if (mWeCanAlignFalcon || FCAM_CAN_ALIGN(mParam))
    m_butAlignDoseFrac.ShowWindow(SW_SHOW);
  if (mWeCanAlignFalcon) {
    ShowDlgItem(IDC_BUT_SETUP_ALIGN, true);
    ShowDlgItem(IDC_STAT_ALIGN_SUMMARY, true);
  } 

  if (!canConfig) {
    err = mWinApp->mFalconHelper->CheckFalconConfig(-1, state, "The config file will no "
      "longer be checked;\nyou will have to use the stupid checkbox");
    if (err) {
      mCamera->SetCanUseFalconConfig(-1);
      canConfig = -1;
      m_bDoseFracMode = false;
    } else {
      strBin.Format("Intermediate frame saving is %s in FEI dialog", state ? "ON": "OFF");
      m_statIntermediateOnOff.SetWindowText(strBin);
      m_bDoseFracMode = state > 0;
    }
    UpdateData(false);
  }
  m_statIntermediateOnOff.ShowWindow(canConfig ? SW_HIDE : SW_SHOW);
  m_butDoseFracMode.ShowWindow((mParam->K2Type || (mFalconCanSave && canConfig == -1)) ?
    SW_SHOW : SW_HIDE);
  SetDlgItemText(IDC_RCOUNTING, mParam->K2Type ? "Counting" : "Electron counting");

  if (mParam->K2Type) {
    radio = (CButton *)GetDlgItem(IDC_RCOUNTING);
    radio->EnableWindow(mParam->K2Type != K2_BASE);
    radio = (CButton *)GetDlgItem(IDC_RSUPERRES);
    radio->ShowWindow(mParam->K2Type == K2_SUMMIT ? SW_SHOW : SW_HIDE);
    radio->EnableWindow(mParam->K2Type == K2_SUMMIT);
    SetDlgItemText(IDC_DOSE_FRAC_MODE, "Dose Fractionation mode");
    m_butAlwaysAntialias.EnableWindow(mCamera->GetPluginVersion(mParam) >= 
      PLUGIN_CAN_ANTIALIAS);
    m_butTakeK3Binned.ShowWindow(mParam->K2Type == K3_TYPE ? SW_SHOW : SW_HIDE);
  } else {
    SetDlgItemText(IDC_DOSE_FRAC_MODE, "Intermediate frame saving is ON in FEI dialog");
  }
  m_statNormDSDF.ShowWindow(SW_HIDE);

  // Manage the DE panel
  if (mDE_Type) {
    mDEweCanAlign = (mParam->CamFlags & DE_WE_CAN_ALIGN) != 0;
    if (!mParam->DE_AutosaveDir.IsEmpty())
      ReplaceWindowText(&m_butNameSuffix, "Suffix", "Options");
    if (!(mParam->CamFlags & DE_CAM_CAN_COUNT)) {
      ShowDlgItem(IDC_RDE_LINEAR, false);
      ShowDlgItem(IDC_RDE_COUNTING, false);
      ShowDlgItem(IDC_RDE_SUPERRES, false);
      ShowDlgItem(IDC_STAT_DEMODE, false);
    }
    ShowDlgItem(IDC_DE_ALIGN_FRAMES, mDEweCanAlign || 
      (mParam->CamFlags & DE_CAM_CAN_ALIGN));
    if (mParam->CamFlags && DE_NORM_IN_SERVER)
       ReplaceWindowText(&m_butDESaveFrames, "single", "raw");
    ShowDlgItem(IDC_BUT_DE_SETUP_ALIGN, mDEweCanAlign);
    ShowDlgItem(IDC_STAT_DE_WHERE_ALIGN, mDEweCanAlign);
    m_spinDEsumNum.SetRange(1, 10000);
    m_spinDEsumNum.SetPos(5000);
  }
  
  // Dark ref management is now con-set dependent, moved it there

  // Show remove X rays box if it is enabled for camera or if any sets have it on
  showbox = mParam->showImageXRayBox;
  for (i = 0; i < NUMBER_OF_USER_CONSETS; i++)
    showbox += conSets[i].removeXrays;
  m_butRemoveXrays.ShowWindow((showbox && !mParam->STEMcamera) ? SW_SHOW : SW_HIDE);

  m_butCorrectDrift.ShowWindow((mParam->OneViewType || 
    (mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_BIN))) ? SW_SHOW : SW_HIDE);
  m_butCorrectDrift.SetWindowText(mParam->OneViewType ? "Correct drift" : 
    "Use hardware binning");

  m_butUseHardwareROI.ShowWindow((mDE_Type && (mParam->CamFlags & DE_HAS_HARDWARE_ROI)) ?
    SW_SHOW : SW_HIDE);

  // Disable many things for restricted sizes
  ManageSizeAndPositionButtons(m_bDoseFracMode && mParam->K2Type);

  // Decide whether to convert A bit Less to Square
  showbox = B3DMAX(mCameraSizeX, mCameraSizeY);
  mSquareForBitLess = (showbox - B3DMIN(mCameraSizeX, mCameraSizeY) >= 0.1 * showbox ||
    mTietzBlocks) && !mParam->squareSubareas;
  if (mSquareForBitLess)
    m_butABitLess.SetWindowText("Square");
  else
    m_butABitLess.SetWindowText("A Bit Less");
  if (!mSquareForBitLess && mTietzBlocks)
    m_butABitLess.EnableWindow(false);

  // Make modifications for STEM camera
  m_butLineSync.EnableWindow(mParam->GatanCam);
  maxChan = mCamera->GetMaxChannels(mParam);
  for (i = 0; i < MAX_STEM_CHANNELS; i++) {
    m_butmaxScanRate.EnableWindow(mParam->maxScanRate > 0.);
    combo = (CComboBox *)GetDlgItem(IDC_COMBOCHAN1 + i);
    combo->ShowWindow(i < maxChan  && mParam->STEMcamera ? SW_SHOW : SW_HIDE);
    stat = (CStatic *)GetDlgItem(IDC_STATCHAN1 + i);
    stat->ShowWindow(i < maxChan  && mParam->STEMcamera ? SW_SHOW : SW_HIDE);
    if (i < maxChan && mParam->STEMcamera) {

      // Load each combo box with all the channel names
      for (j = combo->GetCount() - 1; j >= 0; j--)
        combo->DeleteString(j);
      if (maxChan > 1)
        combo->AddString("None");
      for (j = 0; j < mParam->numChannels; j++) {
        strBin = mParam->channelName[j];
        if (!mParam->availableChan[j])
          strBin += ": NA";
        combo->AddString((LPCTSTR)strBin);
      }
    }
  }
  showbox = mParam->STEMcamera ? SW_HIDE : SW_SHOW;
  stat = (CStatic *)GetDlgItem(IDC_STATPROC);
  stat->ShowWindow(showbox);
  m_butKeepPixel.ShowWindow(mParam->STEMcamera ? SW_SHOW : SW_HIDE);
  m_statMinimumDrift.ShowWindow(showbox);
  m_statTimingAvail.ShowWindow(mParam->STEMcamera && mParam->FEItype ? SW_SHOW : SW_HIDE);
  SetDlgItemText(IDC_DRIFTTEXT1, 
    mParam->STEMcamera ? "Pixel time" : "Drift settling");
  SetDlgItemText(IDC_DRIFTTEXT2, mParam->STEMcamera ? "usec" : "sec");
  if (mParam->STEMcamera)
    areaHeight = mParam->moduloX >= 0 ? mAreaTwoRowHeight : mAreaSTEMHeight;
  else if (IS_FALCON2_OR_3(mParam))
    areaHeight = mAreaSTEMHeight;
  m_statArea.SetWindowPos(NULL, 0, 0, mAreaWidth, areaHeight, SWP_NOMOVE);
  m_statProcessing.SetWindowPos(NULL, 0, 0, mProcWidth, 
    mParam->STEMcamera ? mProcSTEMHeight : mProcBaseHeight, SWP_NOMOVE);
  showbox = (mParam->STEMcamera && mMaxIntegration > 1) ? SW_SHOW : SW_HIDE;
  m_statIntegration.ShowWindow(showbox);
  m_editIntegration.ShowWindow(showbox);
  m_spinIntegration.ShowWindow(showbox);
  m_statExpTimesInt.ShowWindow(showbox);
  if (mParam->STEMcamera) {
    TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
    BOOL recForTS = mWinApp->StartedTiltSeries() && tsParam->cameraIndex == mCurrentCamera
      && mCurrentSet == RECORD_CONSET;
    m_driftText1.EnableWindow(true);
    m_driftText2.EnableWindow(true);
    m_driftEdit.EnableWindow(!(recForTS && 
      mWinApp->mTSController->GetTypeVaries(TS_VARY_EXPOSURE)));
    SetDlgItemText(IDC_MINIMUM_DRIFT, "");

    SetDlgItemText(IDC_STATPROC, "Mag for Autofocusing");
    SetDlgItemText(IDC_RUNPROCESSED, "Unchanged");
    SetDlgItemText(IDC_RDARKSUBTRACT, "Down 2x");
    SetDlgItemText(IDC_RGAINNORMALIZED, "Up 2x");
    SetDlgItemText(IDC_RBOOSTMAG3, "Up 3x");
    SetDlgItemText(IDC_RBOOSTMAG4, "Up 4x");
    SetDlgItemText(IDC_REMOVEXRAYS, "Do for all Focus shots");
  } else {
    SetDlgItemText(IDC_STATPROC, "Processing");
    SetDlgItemText(IDC_RUNPROCESSED, "Unprocessed");
    SetDlgItemText(IDC_RDARKSUBTRACT, "Dark Subtracted");
    SetDlgItemText(IDC_RGAINNORMALIZED, "Gain Normalized");
    SetDlgItemText(IDC_REMOVEXRAYS, "Remove X rays");
  }
  if (twoRowPos) {
    showbox = SWP_NOSIZE | SWP_SHOWWINDOW;
    m_butSmaller10.SetWindowPos(NULL, leftTable[mPosButtonIndex[3]], 
      mSavedTopPos + mLowerPosShift, 0, 0, showbox);
    m_butBigger10.SetWindowPos(NULL, leftTable[mPosButtonIndex[4]], 
      mSavedTopPos + mLowerPosShift, 0, 0, showbox);
    m_butABitLess.SetWindowPos(NULL, leftTable[mPosButtonIndex[5]], 
      mSavedTopPos + mLowerPosShift, 0, 0, showbox);
    m_butLineSync.ShowWindow(SW_HIDE);
  }
} 

// Manage the dark reference buttons
void CCameraSetupDlg::ManageDarkRefs(void)
{
  // Manage the dark averaging based on processing here
  BOOL normOnly = mParam->FEItype && FCAM_ADVANCED(mParam) && 
    mCamera->GetASIgivesGainNormOnly();
  BOOL enable = (mParam->processHere || !mCanProcess) && 
    !(mFalconCanSave && m_bDoseFracMode) && !normOnly;
  m_butAverageDark.EnableWindow(enable);
  m_spinAverage.EnableWindow(enable);
  m_editAverage.EnableWindow(enable);
  m_timesText.EnableWindow(enable);

  // Manage dark references based on camera type and processing
  enable = enable || (mParam->GatanCam && !mParam->K2Type && !mParam->OneViewType);
  m_butDarkAlways.EnableWindow(enable);
  m_butDarkNext.EnableWindow(enable);
  if (mFalconCanSave && !normOnly) {
    m_butDarkSubtract.EnableWindow(mParam->processHere && !m_bDoseFracMode);
    if (m_bDoseFracMode && m_iProcessing == DARK_SUBTRACTED) {
      m_iProcessing = GAIN_NORMALIZED;
      UpdateData(false);
    }
  }
}

// Take care of size and position controls for camera or K2 mode
void CCameraSetupDlg::ManageSizeAndPositionButtons(BOOL disableAll)
{
  bool enable = (mParam->moduloX >= 0 || mTietzSizes) && !disableAll;
  m_butABitLess.EnableWindow(enable && mParam->moduloX >= 0);
  m_butBigger10.EnableWindow(enable);
  m_butWideHalf.EnableWindow(enable && !mParam->squareSubareas);
  m_butWideQuarter.EnableWindow(enable && !mParam->squareSubareas);
  m_butSwapXY.EnableWindow(enable);
  m_butSmaller10.EnableWindow(enable);
  m_editTop.EnableWindow(enable);
  m_editLeft.EnableWindow(enable);
  enable = enable && !mParam->centeredOnly;
  m_sbcUpDown.EnableWindow(enable);
  m_sbcLeftRight.EnableWindow(enable);
  m_butRecenter.EnableWindow(enable);
  m_editBottom.EnableWindow(enable);
  m_editRight.EnableWindow(enable);
  enable = mParam->moduloX != -2 && (mParam->moduloX > -5 || mParam->moduloX < -8) && 
    !disableAll;
  m_butFull.EnableWindow(enable);
  m_butHalf.EnableWindow(enable);
  m_butQuarter.EnableWindow(enable);
}

void CCameraSetupDlg::ManageIntegration()
{
  if (mMaxIntegration > 1) {
    m_strExpTimesInt.Format("(x%d)", m_iContSingle ? m_iIntegration : 1);
    UpdateData(false);
  }
  if (mParam->OneViewType)
    m_butCorrectDrift.EnableWindow(mCamera->OneViewDriftCorrectOK(mParam) &&
    m_iProcessing == GAIN_NORMALIZED && !mParam->processHere);
  else if (mDE_Type)
    m_butCorrectDrift.EnableWindow(m_iBinning > 0);
}

BOOL CCameraSetupDlg::OnInitDialog() 
{
  int i, ind, space, delta, viewInd = -1, numRegular = 0;
  float scale;
  CRect boxrect, butrect;
  CButton *radio;
  CFont *littleFont = mWinApp->GetLittleFont();
  BOOL useViewForSearch = mWinApp->GetUseViewForSearch();
  int camPosInds[MAX_DLG_CAMERAS];
  int posButtonIDs[6] = {IDC_BUTQUARTER, IDC_BUTHALF, IDC_BUTFULL, IDC_BUTSMALLER10, 
    IDC_BUTBIGGER10, IDC_BUTABITLESS};
  int setPosInds[7];
  int setButtonIDs[7] = {IDC_RSEARCH, IDC_RVIEW, IDC_RFOCUS, IDC_RTRIAL, IDC_RRECORD,
    IDC_RPREVIEW, IDC_RMONTAGE};
  int fiveSetPos[7] = {11, 19, 69, 117, 165, 213, 230};
  int noSearchPos[7] = {11, 11, 65, 107, 147, 189, 230};
  int noMontPos[7] = {11, 54, 95, 136, 178, 224, 232};
  int *posPtr;

  CBaseDlg::OnInitDialog();
  mCamera = mWinApp->mCamera;
  mLowDoseMode = mWinApp->LowDoseMode();
  
  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart);

  // Save original left position of dose frac checkbox
  ind = 0;
  mDoseFracLeftOrig = mDoseFracLeftFalcon = -1;
  while (idTable[ind] != TABLE_END && (mDoseFracLeftFalcon < 0 || mDoseFracLeftOrig < 0)){
    if (idTable[ind] == IDC_DOSE_FRAC_MODE) {
      mDoseFracTableInd = ind;
      mDoseFracLeftOrig = leftTable[ind];
    }
    if (idTable[ind] == IDC_RBEAMONLY)
      mDoseFracLeftFalcon = leftTable[ind];
    if (idTable[ind] == IDC_RVIEW)
      viewInd = ind;

    // Look up indexes of the position buttons that may need shifting
    for (i = 0; i < 6; i++)
      if (idTable[ind] == posButtonIDs[i])
        mPosButtonIndex[i] = ind;

    for (i = 0; i < 7; i++)
      if (idTable[ind] == setButtonIDs[i])
        setPosInds[i] = ind;

    i = idTable[ind] - IDC_RCAMERA1;
    if (i >= 0 && i < MAX_DLG_CAMERAS)
      camPosInds[i] = ind;
    ind++;
  }
  m_butDoseFracMode.GetWindowRect(butrect);
  mDoseFracWidthOrig = butrect.Width();
  mDoseFracHeight = butrect.Height();

  // Figure out how much to shift position buttons
  m_butFull.GetWindowRect(butrect);
  space = (topTable[mPosButtonIndex[3]] - topTable[mPosButtonIndex[0]]) / 2 - 
    butrect.Height();
  mUpperPosShift = (6 * space) / 10;
  mLowerPosShift = butrect.Height() + space - mUpperPosShift;
  mShiftedPosButtons = false;
  mIDToSaveTop = IDC_BUTFULL;

  // Fix camera buttons if more than 4
  if (mNumCameras >4) {
    radio = (CButton *)GetDlgItem(IDC_RCAMERA6);
    radio->GetWindowRect(butrect);
    bool needFont = UtilCamRadiosNeedSmallFont(radio);
    delta = (leftTable[camPosInds[MAX_DLG_CAMERAS - 1]] - leftTable[camPosInds[0]]) / 
      (MAX_DLG_CAMERAS - 1);
    for (ind = 0; ind < mNumCameras; ind++) {
      leftTable[camPosInds[ind]] = leftTable[camPosInds[0]] + ind * delta;
      radio = (CButton *)GetDlgItem(IDC_RCAMERA1 + ind);
      radio->SetWindowPos(NULL, 0, 0, butrect.Width(), butrect.Height(), 
        SWP_NOMOVE | SWP_NOZORDER);
      if (needFont)
        radio->SetFont(littleFont);
    }
  }
  
  // Fill the names of the control sets
  SetDlgItemText(IDC_RVIEW, mModeNames[0]);
  SetDlgItemText(IDC_RFOCUS, mModeNames[1]);
  SetDlgItemText(IDC_RTRIAL, mModeNames[2]);
  SetDlgItemText(IDC_RRECORD, mModeNames[3]);
  SetDlgItemText(IDC_RPREVIEW, mModeNames[4]);
  SetDlgItemText(IDC_RSEARCH, mModeNames[5]);
  SetDlgItemText(IDC_RMONTAGE, mModeNames[6]);

  // Adjust positions if less than 7 camera sets
  if (useViewForSearch || mWinApp->GetUseRecordForMontage()) {
    if (useViewForSearch && mWinApp->GetUseRecordForMontage())
      posPtr = &fiveSetPos[0];
    else if (useViewForSearch)
      posPtr = &noSearchPos[0];
    else
      posPtr = &noMontPos[0];
    scale = (float)(leftTable[setPosInds[6]] - leftTable[setPosInds[0]]) / 
      (float)(fiveSetPos[6] - fiveSetPos[0]);
    for (ind = 0; ind < 7; ind++)
      leftTable[setPosInds[ind]] = leftTable[setPosInds[0]] + 
        B3DNINT(scale * (posPtr[ind] - fiveSetPos[0]));

    // Handle making View big enough, change to View/Search, move to left if low dose
    if (mLowDoseMode && useViewForSearch) {
      radio = (CButton *)GetDlgItem(IDC_RVIEW);
      radio->GetWindowRect(butrect);
      radio->SetWindowPos(NULL, 0, 0, B3DNINT(1.45 * butrect.Width()), butrect.Height(), 
        SWP_NOMOVE | SWP_NOZORDER);
      leftTable[setPosInds[1]] = leftTable[camPosInds[0]];
      SetDlgItemText(IDC_RVIEW, mModeNames[0] + "/" + mModeNames[5]);
    }
  }
  if (mCurrentSet == SEARCH_CONSET && useViewForSearch)
    mCurrentSet = VIEW_CONSET;
  if (mCurrentSet == MONT_USER_CONSET && mWinApp->GetUseRecordForMontage())
    mCurrentSet = RECORD_CONSET;
  
  // Make text big for set name
  CFont font;
  CRect rect;
  m_statBigMode.GetWindowRect(&rect);
  font.CreateFont((rect.Height()), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  m_statBigMode.SetFont(&font);

  // Make bolder text for antialiasing
  CWnd *wnd = GetDlgItem(IDC_STAT_ANTIALIAS);
  wnd->GetClientRect(rect);
  mBoldFont.CreateFont(rect.Height(), 0, 0, 0, FW_BOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  wnd->SetFont(&mBoldFont);

  // Set up the camera buttons: but the copy button and label need to be set in ManageCam
  m_statCamera.ShowWindow(mNumCameras > 1 ? SW_SHOW : SW_HIDE);
  for (i = 0; mNumCameras > 1 && i < mNumCameras; i++) {
    SetDlgItemText(i + IDC_RCAMERA1, mCamParam[mActiveCameraList[i]].name);
    if (!mCamParam[mActiveCameraList[i]].GIF && 
      !mCamParam[mActiveCameraList[i]].STEMcamera)
      numRegular++;
  }
  m_butMatchPixel.ShowWindow(numRegular > 1 ? SW_SHOW : SW_HIDE);
  m_butMatchIntensity.ShowWindow(numRegular > 1 ? SW_SHOW : SW_HIDE);
  m_butMatchPixel.EnableWindow(!mLowDoseMode);
  m_butMatchIntensity.EnableWindow(!mLowDoseMode);
  if (numRegular < 2) {
    mNumIDsToHide = 2;
    mIDsToHide[0] = IDC_MATCH_REG_PIXEL;
    mIDsToHide[1] = IDC_MATCH_REG_INTENSITY;
  }

  // Turn off the rest of the camera buttons
  /*for (; i < MAX_DLG_CAMERAS; i++) {
    CButton *radio = (CButton *)GetDlgItem(i + IDC_RCAMERA1);
    if (radio)
      radio->ShowWindow(SW_HIDE);
  } */

  // Get original and STEM size of area button group box, and info for binning box
  m_statArea.GetClientRect(&boxrect);
  mAreaWidth = boxrect.Width();
  mAreaBaseHeight = boxrect.Height();
  m_butQuarter.GetWindowRect(&boxrect);
  m_butSmaller10.GetWindowRect(&butrect);
  mAreaSTEMHeight = mAreaBaseHeight - (butrect.top - boxrect.top);
  mAreaTwoRowHeight = mAreaBaseHeight - (butrect.top - boxrect.top) / 2 - (3 * space) / 2;
  m_statBinning.GetClientRect(&boxrect);
  mBinWidth = boxrect.Width();
  mBinFullHeight = boxrect.Height();

  // Get original and STEM size of processing group box
  m_statProcessing.GetClientRect(&boxrect);
  mProcWidth = boxrect.Width();
  mProcBaseHeight = boxrect.Height();
  m_butGainNormalize.GetWindowRect(&boxrect);
  m_butRemoveXrays.GetWindowRect(&butrect);
  mProcSTEMHeight = mProcBaseHeight + (butrect.top - boxrect.top);

  m_bAlwaysAntialias = mCamera->GetAntialiasBinning();
  m_bTakeK3Binned = mCamera->GetTakeK3SuperResBinned();

  mParam = &mCamParam[mActiveCameraList[mCurrentCamera]];
  ManageCamera();
    
  // Unload the control set
  LoadConsetToDialog();
  if (mPlacement.rcNormalPosition.right > 0)
    SetWindowPlacement(&mPlacement);
  CButton *button = (CButton *)GetDlgItem(IDOK);
  button->SetFocus();

  // Need to set the window size again because it comes up at last size otherwise
  SetWindowPos(NULL, 0, 0, mBasicWidth, mSetToHeight, SWP_NOMOVE);

  return FALSE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CCameraSetupDlg::DrawBox()
{
  ManageBinnedSize();
  Invalidate();
}

void CCameraSetupDlg::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  CRect winRect;
  CRect statRect;
  CRect clientRect;
  int maxsize = B3DMAX(mCameraSizeX, mCameraSizeY);
  int swidth, sheight, owidth, oheight;
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  m_statBox.GetWindowRect(&statRect);
  swidth = statRect.Width();
  sheight = statRect.Height();

  COLORREF fillColor = RGB(0,0,128);
  COLORREF tlColor = RGB(0,0,0);
  COLORREF brColor = RGB(0,0,0);
  int iLeft = (swidth * (maxsize - mCameraSizeX)) / (2 * maxsize) + statRect.left - 
    winRect.left;
  int iTop = (sheight * (maxsize - mCameraSizeY)) / (2 * maxsize) + statRect.top - 
    winRect.top - (winRect.Height() - clientRect.Height());
  owidth = (swidth * mCameraSizeX) / maxsize;
  oheight = (sheight * mCameraSizeY) / maxsize;

  dc.Draw3dRect(iLeft, iTop, owidth, oheight, tlColor, brColor);

  int boxTop = mCoordScaling * m_eTop * oheight / mCameraSizeY;
  int boxLeft = mCoordScaling * m_eLeft * owidth / mCameraSizeX;
  int boxHeight = mCoordScaling * (m_eBottom - m_eTop) * oheight / mCameraSizeY;
  int boxWidth = mCoordScaling * (m_eRight - m_eLeft) * owidth / mCameraSizeX;
  if (boxWidth > 0 && boxHeight > 0 && boxLeft < statRect.Width()
    && boxTop < statRect.Height())
    dc.FillSolidRect(iLeft + boxLeft, iTop + boxTop, boxWidth, boxHeight,
      fillColor);
  
  // Do not call CDialog::OnPaint() for painting messages
}

void CCameraSetupDlg::OnChangeCoord() 
{
  if (!mClosing)
    UpdateData(TRUE);
  int left = mCoordScaling * m_eLeft;
  int right = mCoordScaling * m_eRight;
  int top = mCoordScaling * m_eTop;
  int bottom = mCoordScaling * m_eBottom;

  // Just limit sizes here unless keeping it centered
  BOOL update = false;
  if (mParam->centeredOnly) {
    if (left >= mCameraSizeX / 2)
      left = mCameraSizeX / 2 - 1;
    right = mCameraSizeX - left;
    if (top >= mCameraSizeY / 2)
      top = mCameraSizeX / 2 - 1;
    bottom = mCameraSizeY - top;
    update = true;
  }
  if (top < 0 || top > mCameraSizeY - 1) {
    top = B3DMAX(0, B3DMIN(mCameraSizeY - 1, top));
    update = true;
  }
  if (bottom < 1 || bottom > mCameraSizeY) {
    bottom = B3DMAX(1, B3DMIN(mCameraSizeY, bottom));
    update = true;
  }
  if (left < 0 || left > mCameraSizeX - 1) {
    left = B3DMAX(0, B3DMIN(mCameraSizeX - 1, left));
    update = true;
  }
  if (right < 1 || right > mCameraSizeX) {
    right = B3DMAX(1, B3DMIN(mCameraSizeX, right));
    update = true;
  }
  m_eLeft = left / mCoordScaling;
  m_eRight = right / mCoordScaling;
  m_eTop = top / mCoordScaling;
  m_eBottom = bottom / mCoordScaling;
  if (update && !mClosing)
    UpdateData(FALSE);
  if (!mClosing)
    DrawBox();
}

// When a coordinate box loses focus, force the opposite one to conform
// The initial validation may not be needed...
void CCameraSetupDlg::OnKillfocusLeft() 
{
  int Left = mCoordScaling * m_eLeft;
  int Right = mCoordScaling * m_eRight;
  int limit = mCameraSizeX / (mParam->centeredOnly ? 2 : 1) - 1;
  BOOL update = false;
  if (Left < 0 || Left > limit) {
    Left = B3DMAX(0, B3DMIN(limit, Left));
    update = true;
  }
  if (Right <= Left) {
    Right = Left + 1;
    update = true;
  }
  if (mParam->centeredOnly) {
    Right = mCameraSizeX - Left;
    update = true;
  }
  m_eLeft = Left / mCoordScaling;
  m_eRight = Right / mCoordScaling;
  if (AdjustCoords(mBinnings[m_iBinning]))
    update = true;
  if (update) {
    UpdateData(FALSE);
    if (!mClosing)
      DrawBox();
  }
}

void CCameraSetupDlg::OnKillfocusRight() 
{
  int Left = mCoordScaling * m_eLeft;
  int Right = mCoordScaling * m_eRight;
  BOOL update = false;
  if (Right < 1 || Right > mCameraSizeX) {
    Right = B3DMAX(1, B3DMIN(mCameraSizeX, Right));
    update = true;
  }
  if (Right <= Left) {
    Left = Right - 1;
    update = true;
  }
  m_eLeft = Left / mCoordScaling;
  m_eRight = Right / mCoordScaling;
  if (AdjustCoords(mBinnings[m_iBinning]))
    update = true;
  if (update) {
    UpdateData(FALSE);
    if (!mClosing)
      DrawBox();
  }
}

void CCameraSetupDlg::OnKillfocusTop() 
{
  int Top = mCoordScaling * m_eTop;
  int Bottom = mCoordScaling * m_eBottom;
  int limit = mCameraSizeY / (mParam->centeredOnly ? 2 : 1) - 1;
  BOOL update = false;
  if (Top < 0 || Top > limit) {
    Top = B3DMAX(0, B3DMIN(limit, Top));
    update = true;
  }
  if (Bottom <= Top) {
    Bottom = Top + 1;
    update = true;
  }
  if (mParam->centeredOnly) {
    Bottom = mCameraSizeY - Top;
    update = true;
  }
  m_eTop = Top / mCoordScaling;
  m_eBottom = Bottom / mCoordScaling;
  if (AdjustCoords(mBinnings[m_iBinning]))
    update = true;
  if (update) {
    UpdateData(FALSE);
    if (!mClosing)
      DrawBox();
  }
}

void CCameraSetupDlg::OnKillfocusBottom() 
{
  int Top = mCoordScaling * m_eTop;
  int Bottom = mCoordScaling * m_eBottom;
  BOOL update = false;
  if (Bottom < 1 || Bottom > mCameraSizeY) {
    Bottom = B3DMAX(1, B3DMIN(mCameraSizeY, Bottom));
    update = true;
  }
  if (Bottom <= Top) {
    Top = Bottom - 1;
    update = true;
  }
  m_eTop = Top / mCoordScaling;
  m_eBottom = Bottom / mCoordScaling;
  if (AdjustCoords(mBinnings[m_iBinning]))
    update = true;
  if (update) {
    UpdateData(FALSE);
    if (!mClosing)
      DrawBox();
  }
}

void CCameraSetupDlg::OnKillfocusEditexposure() 
{
  if (!UpdateData(TRUE))
    return;
  ManageExposure();
  if (m_bDoseFracMode && ((m_bSaveFrames && mParam->K2Type && m_bSaveK2Sums) ||
    ((m_bSaveFrames || m_bAlignDoseFrac) && mFalconCanSave))) {
      mWinApp->mFalconHelper->AdjustForExposure(mSummedFrameList, mNumSkipBefore,
        mNumSkipAfter, m_eExposure, 
        mFalconCanSave ? mCamera->GetFalconReadoutInterval() : m_fFrameTime, 
        mUserFrameFrac, mUserSubframeFrac, mFalconCanSave && FCAM_CAN_ALIGN(mParam) &&
        m_bAlignDoseFrac && !mCurSet->useFrameAlign);
      ManageAntialias();
  }
  ManageK2SaveSummary();
  if (mParam->STEMcamera)
    ManageDrift();
  ManageTimingAvailable();
  ManageDose();
}

void CCameraSetupDlg::OnKillfocusEditIntegration() 
{
  UpdateData(TRUE);
  ManageIntegration();
}

void CCameraSetupDlg::OnKillfocusEditsettling() 
{
  CString str = m_strSettling;
  float topTime = (float)(mParam->maxPixelTime > 0 ? mParam->maxPixelTime : 100000.);
  UpdateData(TRUE);
  float settling = (float)atof(m_strSettling);
  if (mParam->STEMcamera) {

    // Do not let the exposure time be revised unless the text here actually changed
    if (str != m_strSettling || mChangedBinning) {
      B3DCLAMP(settling, mParam->minPixelTime, topTime);
      m_eExposure = (float)(settling * (m_eBottom - m_eTop) * (m_eRight - m_eLeft) / 
        (1.e6 * mBinnings[m_iBinning] * mBinnings[m_iBinning]));
      UpdateData(false);
    }
  } else {
    m_eSettling = settling;
    if (m_eSettling < 0. || m_eSettling > 10.) {
      B3DCLAMP(m_eSettling, 0.f, 10.f);
      UpdateData(false);
    }
  }
  ManageDrift();
  ManageTimingAvailable();
  ManageDose();
}

void CCameraSetupDlg::OnDeltaposSpinaverage(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = pNMUpDown->iPos + pNMUpDown->iDelta;
  UpdateData(true);
  if (newVal < 2 || newVal > MAX_DARK_AVERAGE) {
    *pResult = 1;
    return;
  }
  m_iAverageTimes = newVal;
  UpdateData(false);
	*pResult = 0;
}

void CCameraSetupDlg::OnDeltaposSpinIntegration(NMHDR* pNMHDR, LRESULT* pResult) 
{
  if (NewSpinnerValue(pNMHDR, pResult, m_iIntegration, 1, mMaxIntegration, 
    m_iIntegration))
    return;
  ManageIntegration();
}

void CCameraSetupDlg::OnKillfocusEditaverage() 
{
  UpdateData(true);
  m_spinAverage.SetPos(m_iAverageTimes);
}

void CCameraSetupDlg::ManageDose()
{
  int spotSize;
  double intensity, dose;
  int camera = mActiveCameraList[mCurrentCamera];
  float doseFac = mParam->specToCamDoseFac;
  
  // Need to synchronize back to camera LDP since we are accessing them
  mWinApp->CopyCurrentToCameraLDP();
  float realExp = m_eExposure;
  mCamera->ConstrainExposureTime(mParam, m_bDoseFracMode, mParam->DE_camType ? m_iDEMode :
    m_iK2Mode, mBinnings[m_iBinning], mCurSet->alignFrames && !mCurSet->useFrameAlign,
    m_bDEsaveMaster ? m_iSumCount : 1, realExp, m_fFrameTime);
  m_fDEframeTime = RoundedDEframeTime(m_fFrameTime * (m_bDEsaveMaster ? 1 : m_iSumCount));
  if (mParam->K2Type)
    m_fFrameTime = RoundedDEframeTime(m_fFrameTime);
  dose = mWinApp->mBeamAssessor->GetCurrentElectronDose(camera, mCurrentSet, realExp, 
    m_eSettling, spotSize, intensity);
  if (dose)
    m_strElecDose.Format("Dose: %.2f e/A2 at spot %d, %s %.2f%s", dose, spotSize,
       mWinApp->mScope->GetC2Name() ,mWinApp->mScope->GetC2Percent(spotSize, intensity),
       mWinApp->mScope->GetC2Units());
  else
    m_strElecDose = "Dose: Not calibrated";

  // Do dose rate for a direct detector
  if (mCamera->IsDirectDetector(mParam)) {
    m_strDosePerFrame = "";
    if (dose) {
      float pixel = 10000.f * BinDivisorF(mParam) * 
        mWinApp->mShiftManager->GetPixelSize(mActiveCameraList[mCurrentCamera], 
        GetMagIndexForCamAndSet());
      m_strDoseRate.Format("%.2f e/ub pixel/s at %s", (doseFac > 0. ? doseFac : 1.) *
        dose * pixel * pixel /B3DMAX(0.0025, realExp - mParam->deadTime), 
        doseFac > 0 ? "camera" : "specimen");

      // And get a dose per frame if saving frames, just divide dose by total frames
      int dummy, frames = 0;
      if (mParam->K2Type && m_bDoseFracMode && (m_bAlignDoseFrac || m_bSaveFrames)) {
        frames = B3DNINT(m_eExposure / B3DMAX(mCamera->GetMinK2FrameTime(mParam->K2Type),
          ActualFrameTime(m_fFrameTime)));
        if (m_bSaveFrames && m_bSaveK2Sums && mSummedFrameList.size() > 0)
          frames = mWinApp->mFalconHelper->GetFrameTotals(mSummedFrameList, dummy);
      } else if (mFalconCanSave && m_bDoseFracMode && (m_bSaveFrames || 
        (mWeCanAlignFalcon && m_bAlignDoseFrac))) {
        frames = mWinApp->mFalconHelper->GetFrameTotals(mSummedFrameList, dummy);
      } else if (mWinApp->mDEToolDlg.CanSaveFrames(mParam) && 
        (m_bDEsaveMaster || m_bDEalignFrames)) {
          frames = (int)floor(realExp / m_fDEframeTime) + 1;
      }
      if (frames)
        m_strDosePerFrame.Format("%.2f e/A2 per frame", dose / frames);
    } else {
      m_strDoseRate = "Dose rate: Not calibrated";
    }
  } else {
    m_strDoseRate = "";
    m_strDosePerFrame = "";
  }

  UpdateData(FALSE);
}

bool CCameraSetupDlg::AdjustCoords(int binning, bool alwaysUpdate)
{
  bool update;
  int left, right, top, bottom, sizeX, sizeY, ucrit = 4 * binning - 1;
  double dbin = binning;
  int moduloX = (mParam->K2Type && m_bDoseFracMode) ? -2 : mParam->moduloX;
  int moduloY = (mParam->K2Type && m_bDoseFracMode) ? -2 : mParam->moduloY;
  left = mCoordScaling * m_eLeft / binning;
  right = mCoordScaling * m_eRight / binning;
  top = mCoordScaling * m_eTop / binning;
  bottom = mCoordScaling * m_eBottom / binning;
  sizeX = right - left;
  sizeY = bottom - top;
  mCamera->AdjustSizes(sizeX, mCameraSizeX, moduloX, left, right, sizeY, mCameraSizeY, 
    moduloY, top, bottom, binning, mActiveCameraList[mCurrentCamera]);
  update = fabs(mCoordScaling * m_eLeft - left * dbin) > ucrit || 
    fabs(mCoordScaling * m_eRight - right * dbin) > ucrit ||
    fabs(mCoordScaling * m_eTop - top * dbin) > ucrit || 
    fabs(mCoordScaling * m_eBottom - bottom * dbin) > ucrit;

  // Update parameters in the conset and the screen if there is a substantial change
  // this is to avoid losing pixels when going between binnings
  if (update || alwaysUpdate) {
    m_eLeft = left * binning / mCoordScaling;
    m_eRight = right * binning / mCoordScaling;
    m_eTop = top * binning / mCoordScaling;
    m_eBottom = bottom * binning / mCoordScaling;
  }
  return update;
}

/////////////////////////////////////////////
// STEM
/////////////////////////////////////////////

// Handle selection of new channel in one combo box
void CCameraSetupDlg::OnSelendokChan1() { NewChannelSel(0); }
void CCameraSetupDlg::OnSelendokChan2() { NewChannelSel(1); }
void CCameraSetupDlg::OnSelendokChan3() { NewChannelSel(2); }
void CCameraSetupDlg::OnSelendokChan4() { NewChannelSel(3); }
void CCameraSetupDlg::OnSelendokChan5() { NewChannelSel(4); }
void CCameraSetupDlg::OnSelendokChan6() { NewChannelSel(5); }
void CCameraSetupDlg::OnSelendokChan7() { NewChannelSel(6); }
void CCameraSetupDlg::OnSelendokChan8() { NewChannelSel(7); }

void CCameraSetupDlg::NewChannelSel(int which)
{
  CComboBox *chgbox = (CComboBox *)GetDlgItem(IDC_COMBOCHAN1 + which);
  int newsel = chgbox->GetCurSel();
  for (int i = 0; newsel && i < mCamera->GetMaxChannels(mParam); i++) {
    if (i == which)
      continue;
    CComboBox *combo = (CComboBox *)GetDlgItem(IDC_COMBOCHAN1 + i);
    int sel = combo->GetCurSel();

    // subtract 1 because this loop runs only when there is more than one channel at once
    if (mCamera->MutuallyExclusiveChannels(sel - 1, newsel - 1))
      combo->SetCurSel(0);
  }
}

void CCameraSetupDlg::OnmaxScanRate()
{
  ManageDrift(true);
  m_butmaxScanRate.SetButtonStyle(BS_PUSHBUTTON);
}


void CCameraSetupDlg::ManageTimingAvailable(void)
{
  const char *messages[] = { "Error getting usable timing", 
    "No appropriate timing available",
    "Measured timing available", "Interpolated timing available", 
    "Extrapolated timing available", "Timing available, extrapolated from one point"};
  float flyback;
  if (!(mParam->STEMcamera && mParam->FEItype))
    return;
  int status = GetFEIflybackTime(flyback);
  SetDlgItemText(IDC_STAT_TIMING_AVAIL, CString(messages[status]));;
  m_statTimingAvail.EnableWindow(m_bDynFocus);
}

int CCameraSetupDlg::GetFEIflybackTime(float &flyback)
{
  float startup;
  int binning = mBinnings[m_iBinning];
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + 
    mCamera->ConSetToLDArea(mCurrentSet);
  int magIndex = GetMagIndexForCamAndSet();
  return mWinApp->mCalibTiming->FlybackTimeFromTable(binning, (m_eRight - m_eLeft) /
    binning, magIndex, m_eExposure, flyback, startup);
}

/////////////////////////////////////////////
// K2 Camera and some others
/////////////////////////////////////////////

// Linear/counting/superres Mode change for all cameras
void CCameraSetupDlg::OnK2Mode()
{
  int *modeP = mParam->DE_camType ? &m_iDEMode : &m_iK2Mode;
  int oldMode = *modeP;
  CString message, str;
  UpdateData(true);

  // Check the frame align parameters and restore the old mode if that is the choice
  if (CheckFrameAliRestrictions(*modeP, mCamera->GetSaveUnnormalizedFrames(), 
    mUserSaveFrames, "operating mode")) {
      *modeP = oldMode;
      UpdateData(false);
      return;
  }

  // Handle DE and return
  if (mParam->DE_camType) {
    if (oldMode == 0 || m_iDEMode == 0) {

      // Unload the mode-dependent items and reload appropriate ones if switching in or 
      // out of linear mode
      if (m_iDEMode) {
        mCurSet->DEsumCount = m_iSumCount;
        m_iSumCount = mCurSet->sumK2Frames;
        mParam->DE_FramesPerSec = m_fDEfps;
        if (mParam->DE_CountingFPS > 0.)
          m_fDEfps = mParam->DE_CountingFPS;
      } else {
        mCurSet->sumK2Frames = m_iSumCount;
        m_iSumCount = mCurSet->DEsumCount;
        mParam->DE_CountingFPS = m_fDEfps;
        m_fDEfps = mParam->DE_FramesPerSec;
      }
      m_iSumCount = B3DMAX(1, m_iSumCount);
    }
    ManageDEpanel();
    ManageExposure();
    UpdateData(false);
    return;
  }

  // Handle K2, do general updates for Falcon too
  if (mParam->K2Type) {
    ManageK2Binning();
  }
  ManageDoseFrac();
  ManageAntialias();
  ManageExposure();
  ManageK2SaveSummary();
  ManageDose();
}

// Calls routine to find valid frame align parameters given restrictions and selections,
// then offers 
int CCameraSetupDlg::CheckFrameAliRestrictions(int useMode, BOOL saveUnnormed, 
  BOOL useSave, const char *descrip)
{
  int newIndex, notOK, answ;
  CString message, str;
  bool takeBinned = m_bTakeK3Binned && m_iProcessing == GAIN_NORMALIZED && 
    !(saveUnnormed && (mCurSet->useFrameAlign > 1 || useSave));
  if (mCurSet->useFrameAlign) {
    notOK = UtilFindValidFrameAliParams(mParam, useMode, takeBinned, 
      mCurSet->useFrameAlign, mCurSet->faParamSetInd, newIndex, &message);
    if (notOK) {
      message += "\n\nPress:\n\"Switch Back\" to change back to the previous " + 
        CString(descrip) + "\n\n";
      if (notOK > 0)
         message += "\"Use Set Anyway\" to use these alignment parameters anyway";
      else {
        str.Format("\"Use Other Set\" to use the first suitable alignment parameters "
         "(# %d)", newIndex + 1);
        message += str;
      }
      message += "\n\n\"Open Dialog\" to open the Frame Alignment Parameters dialog\n"
        "and adjust parameters or restrictions";
      answ = SEMThreeChoiceBox(message, "Switch Back", notOK > 0 ? "Use Set Anyway" :
        "Use Other Set", "Open Dialog", MB_YESNOCANCEL | MB_ICONQUESTION);
      if (answ == IDCANCEL) {
        OnButSetupAlign();
      } else if (answ == IDYES) {
        return 1;
      } else {
        mCurSet->faParamSetInd = newIndex;
      }
    } else {
      mCurSet->faParamSetInd = newIndex;
    }
  }
  return 0;
}


// Checks whether binning 0.5 is allowed in current conditions, enables radio button, and
// adjusts binning to 1 if not
void CCameraSetupDlg::ManageK2Binning(void)
{
  if (!mParam->K2Type)
    return;
  CButton *radio = (CButton *)GetDlgItem(IDC_RBIN1);
  bool superResOK = IS_SUPERRES(mParam, m_iK2Mode) && !mCamera->IsK3BinningSuperResFrames(
    mParam->K2Type, m_bDoseFracMode ? 1 : 0, m_bSaveFrames ? 1 : 0, m_bAlignDoseFrac ? 1 : 0,
    mCurSet->useFrameAlign,  m_iProcessing, m_iK2Mode, m_bTakeK3Binned);
  radio->EnableWindow(mBinningEnabled && superResOK);
  if (!superResOK && !m_iBinning) {
    m_iBinning = 1;
    ManageBinnedSize();
    UpdateData(FALSE);
  }
}

// Dose fractionation mode change
void CCameraSetupDlg::OnDoseFracMode()
{
  UpdateData(true);
  if (mFalconCanSave)
    mCamera->SetFrameSavingEnabled(m_bDoseFracMode);
  if (mParam->K2Type && m_bDoseFracMode && m_bSaveFrames && m_bSaveK2Sums)
    OnSaveK2FrameSums();
  if (mFalconCanSave && m_bDoseFracMode && m_bSaveFrames)
    OnSaveFrames();
  ManageK2Binning();
  ManageDoseFrac();
  // TODO: is this the case for Falcon too?
  if (m_bDoseFracMode) {
    AdjustCoords(mBinnings[m_iBinning], true);
    UpdateData(FALSE);
    DrawBox();
  }
  ManageAntialias();
  ManageExposure();
  ManageK2SaveSummary();
  ManageDose();
}

// Align frames toggled
void CCameraSetupDlg::OnAlignDoseFrac()
{
  UpdateData(true);
  CheckFalconFrameSumList();
  ManageK2Binning();
  ManageDoseFrac();
}

void CCameraSetupDlg::OnKillfocusEditFrameTime() 
{
  UpdateData(TRUE);
  float startFrame = m_fFrameTime;
  if (mParam->K2Type)
    mCamera->ConstrainFrameTime(m_fFrameTime, mParam->K2Type);
  if (m_bSaveFrames && m_bSaveK2Sums)
    mWinApp->mFalconHelper->AdjustForExposure(mSummedFrameList, 0,
      0, m_eExposure, m_fFrameTime, mUserFrameFrac, mUserSubframeFrac, false);
  if (mParam->K2Type)
    m_fFrameTime = RoundedDEframeTime(m_fFrameTime);
  if (fabs(startFrame - m_fFrameTime) > 1.e-5)
    UpdateData(false);
  ManageExposure();
  ManageK2SaveSummary();
  ManageDose();
}

// Manage various controls depending on settings
void CCameraSetupDlg::ManageDoseFrac(void)
{
  CString str;
  bool enable;
  bool forceSaving = m_bDoseFracMode && m_bAlignDoseFrac && (mCurSet->useFrameAlign > 1 && 
    ((mParam->K2Type && mCamera->CAN_PLUGIN_DO(CAN_ALIGN_FRAMES, mParam)) || 
    mWeCanAlignFalcon));
  if ((forceSaving && !m_bSaveFrames) || (!forceSaving && !BOOL_EQUIV(m_bSaveFrames,
    mUserSaveFrames))) {
    m_bSaveFrames = B3DCHOICE(forceSaving, true, mUserSaveFrames);
    UpdateData(false);
  }
  
  m_statFrameTime.EnableWindow(m_bDoseFracMode);
  m_statFrameSec.EnableWindow(m_bDoseFracMode);
  m_editFrameTime.EnableWindow(m_bDoseFracMode);
  m_butAlignDoseFrac.EnableWindow(m_bDoseFracMode && (m_bSaveFrames || !mFEItype || 
    mWeCanAlignFalcon) &&
    !(mCurrentSet == RECORD_CONSET && mWinApp->mTSController->GetFrameAlignInIMOD()));
  m_butSaveFrames.EnableWindow(m_bDoseFracMode && !forceSaving);
  m_butSetupAlign.EnableWindow(m_bDoseFracMode && m_bAlignDoseFrac);
  enable = m_bDoseFracMode && (m_bSaveFrames || (mWeCanAlignFalcon && m_bAlignDoseFrac &&
    mCurSet->useFrameAlign));
  m_butSetSaveFolder.EnableWindow(enable); 
  m_butFileOptions.EnableWindow(enable);
  m_statSaveSummary.ShowWindow((m_bSaveFrames && m_bDoseFracMode && 
    (mParam->K2Type || mFalconCanSave)) ? SW_SHOW : SW_HIDE);
  m_statAlignSummary.ShowWindow((!m_bSaveFrames && m_bAlignDoseFrac && m_bDoseFracMode && 
    (mParam->K2Type || mWeCanAlignFalcon)) ? SW_SHOW : SW_HIDE);
  enable = m_bSaveFrames && m_bDoseFracMode && 
    mCamera->CAN_PLUGIN_DO(CAN_SUM_FRAMES, mParam);  
  m_butSaveFrameSums.EnableWindow(enable);
  m_butSetupK2FrameSums.EnableWindow(enable && m_bSaveK2Sums);
  m_butSetupFalconFrames.EnableWindow((m_bSaveFrames || m_bAlignDoseFrac) && 
    m_bDoseFracMode);
  SetDlgItemText(IDC_SETUP_FALCON_FRAMES, (m_bAlignDoseFrac && !m_bSaveFrames &&
    mWeCanAlignFalcon) ? "Set Up Frames to Align" : "Set Up Frames to Save");
  enable = mParam->K2Type && m_bSaveFrames && m_bDoseFracMode && 
    ((mCamera->CAN_PLUGIN_DO(CAN_GAIN_NORM, mParam) && 
    m_iProcessing == DARK_SUBTRACTED) || (m_iProcessing == UNPROCESSED && 
    mCamera->CAN_PLUGIN_DO(UNPROC_LIKE_DS, mParam))) &&
    !mCamera->GetNoNormOfDSdoseFrac() &&
    ((!IS_SUPERRES(mParam, m_iK2Mode) && !mParam->countingRefForK2.IsEmpty()) ||
    (IS_SUPERRES(mParam, m_iK2Mode) && !mParam->superResRefForK2.IsEmpty()));
  m_statNormDSDF.ShowWindow(enable ? SW_SHOW : SW_HIDE);
  m_statWhereAlign.ShowWindow((m_bDoseFracMode && m_bAlignDoseFrac && 
    (mParam->K2Type || mWeCanAlignFalcon)) ? SW_SHOW : SW_HIDE);
  ComposeWhereAlign(str);
  SetDlgItemText(IDC_STAT_WHERE_ALIGN, (LPCTSTR)str);
  // This is vestigial from when save enabled align with UFA = 2
  m_statWhereAlign.EnableWindow(mCurSet->useFrameAlign < 2 || m_bSaveFrames);

  ManageSizeAndPositionButtons(m_bDoseFracMode && mParam->K2Type);
  ManageDarkRefs();
}

void CCameraSetupDlg::ComposeWhereAlign(CString &str)
{
  CArray<FrameAliParams, FrameAliParams> *faParams = mCamera->GetFrameAliParams();
  FrameAliParams fap;
  if (!mCurSet->useFrameAlign) {
    if (mDE_Type)
      str = "Align in DE server";
    else
      str = mParam->K2Type ? "Align in DM" : "Align in Falcon processor";
  } else if (mCurSet->useFrameAlign == 1)
    str = mParam->K2Type ? "Align in Plugin" : "Align in SerialEM";
  else
    str = (mCurrentSet == RECORD_CONSET && mCamera->GetAlignWholeSeriesInIMOD()) ?
    "TS only in IMOD" : "Align in IMOD";
  if (mCurSet->useFrameAlign && mCurSet->faParamSetInd >= 0 && 
    mCurSet->faParamSetInd < faParams->GetSize()) {
      fap = faParams->GetAt(mCurSet->faParamSetInd);
      str += " with \"" + fap.name + "\"";
  }
}

// Save frames toggled (dose frac mode is on)
void CCameraSetupDlg::OnSaveFrames()
{
  UpdateData(true);
  if (CheckFrameAliRestrictions(m_iK2Mode, mCamera->GetSaveUnnormalizedFrames(),
    m_bSaveFrames, "setting for saving frames")) {
      m_bSaveFrames = false;
      UpdateData(false);
      return;
  }
  mUserSaveFrames = m_bSaveFrames;
  CheckFalconFrameSumList();
  if (mParam->K2Type && m_bSaveFrames && m_bSaveK2Sums)
    OnSaveK2FrameSums();
  ManageK2Binning();
  ManageDoseFrac();
  ManageAntialias();
  ManageK2SaveSummary();
  ManageDose();
}

// Make sure there is a good summed frame list when save or align is turned on
void CCameraSetupDlg::CheckFalconFrameSumList(void)
{
  if (mFalconCanSave && (m_bSaveFrames || m_bAlignDoseFrac)) {
    if (!mSummedFrameList.size())
      OnSetupFalconFrames();
    if (!mSummedFrameList.size()) {
      mUserSaveFrames = m_bSaveFrames = false;
      m_bAlignDoseFrac = false;
      UpdateData(false);
    } else {
      ManageExposure();
      mWinApp->mFalconHelper->AdjustForExposure(mSummedFrameList, mNumSkipBefore,
        mNumSkipAfter, m_eExposure, mCamera->GetFalconReadoutInterval(), mUserFrameFrac,
        mUserSubframeFrac, FCAM_CAN_ALIGN(mParam) && m_bAlignDoseFrac && 
        !mCurSet->useFrameAlign);
    }
  }
}

void CCameraSetupDlg::OnButFileOptions()
{
  CK2SaveOptionDlg optDlg;
  optDlg.m_bOneFramePerFile = mCamera->GetOneK2FramePerFile();
  optDlg.m_iFileType = mCamera->GetK2SaveAsTiff();
  B3DCLAMP(optDlg.m_iFileType, 0, 2);
  optDlg.m_bPackRawFrames = (mCamera->GetSaveRawPacked() & 1) > 0;
  optDlg.m_bPackCounting4Bit = (mCamera->GetSaveRawPacked() & 2) > 0;
  optDlg.m_bSaveTimes100 = mCamera->GetSaveTimes100() != 0 && mParam->K2Type == K2_SUMMIT;
  optDlg.m_bReduceSuperres = mCamera->GetSaveSuperResReduced();
  optDlg.mTakingK3Binned = m_bTakeK3Binned && mParam->K2Type == K3_TYPE;
  optDlg.m_bUse4BitMode = mCamera->GetUse4BitMrcMode();
  optDlg.m_bSaveUnnormalized = mCamera->GetSaveUnnormalizedFrames();
  optDlg.m_bUseExtensionMRCS = mCamera->GetNameFramesAsMRCS();
  optDlg.m_bSkipRotFlip = mCamera->GetSkipK2FrameRotFlip();
  optDlg.mEnableSkipRotFlip = mParam->rotationFlip;
  optDlg.mFalconType = mParam->FEItype;
  optDlg.mCamFlags = mParam->CamFlags;
  optDlg.mK2Type = mParam->K2Type;
  optDlg.mSaveSetting = m_bSaveFrames ? 1 : 0;
  optDlg.mAlignSetting = m_bAlignDoseFrac ? 1 : 0;
  optDlg.mUseFrameAlign = mCurSet->useFrameAlign;
  optDlg.mK2mode = m_iK2Mode;
  mCamera->FixDirForFalconFrames(mParam);
  optDlg.mDEtype = mWinApp->mDEToolDlg.CanSaveFrames(mParam);
  optDlg.m_strBasename = mCamera->GetFrameBaseName();
  optDlg.mNameFormat = mCamera->GetFrameNameFormat();
  optDlg.mNumberDigits = mCamera->GetDigitsForNumberedFrame();
  optDlg.m_iStartNumber = mCamera->GetFrameNumberStart();
  optDlg.mCanCreateDir = IS_BASIC_FALCON2(mParam) || 
    (mParam->FEItype && (mCamera->GetDirForFalconFrames()).IsEmpty()) || 
    (mParam->GatanCam && mCamera->CAN_PLUGIN_DO(CREATES_DIRECTORY, mParam)) ||
    (mParam->DE_camType && !mParam->DE_AutosaveDir.IsEmpty());
  optDlg.mCan4BitModeAndCounting = 
    mCamera->CAN_PLUGIN_DO(4BIT_101_COUNTING, mParam);
  optDlg.mCanSaveTimes100 = 
    mCamera->CAN_PLUGIN_DO(SAVES_TIMES_100, mParam) && mParam->K2Type == K2_SUMMIT;
  optDlg.mCanUseExtMRCS = mCamera->CAN_PLUGIN_DO(CAN_SET_MRCS_EXT, mParam);
  optDlg.mCanGainNormSum = mCamera->CAN_PLUGIN_DO(CAN_GAIN_NORM, mParam);
  optDlg.mCanReduceSuperres = mCamera->CAN_PLUGIN_DO(CAN_REDUCE_SUPER, mParam);
  optDlg.mSetIsGainNormalized = m_iProcessing == GAIN_NORMALIZED;
  if (optDlg.DoModal() == IDOK) {
    mCamera->SetOneK2FramePerFile(optDlg.m_bOneFramePerFile);
    mCamera->SetSaveRawPacked((optDlg.m_bPackRawFrames ? 1 : 0) + 
      (optDlg.m_bPackCounting4Bit ? 2 : 0));
    if (mParam->K2Type == K2_SUMMIT)
      mCamera->SetSaveTimes100(optDlg.m_bSaveTimes100);
    mCamera->SetSaveSuperResReduced(optDlg.m_bReduceSuperres);
    mCamera->SetUse4BitMrcMode(optDlg.m_bUse4BitMode);
    if (!CheckFrameAliRestrictions(m_iK2Mode, optDlg.m_bSaveUnnormalized, mUserSaveFrames,
      "setting for saving frames unnormalized"))
        mCamera->SetSaveUnnormalizedFrames(optDlg.m_bSaveUnnormalized);
    mCamera->SetNameFramesAsMRCS(optDlg.m_bUseExtensionMRCS);
    mCamera->SetK2SaveAsTiff(optDlg.m_iFileType);
    mCamera->SetSkipK2FrameRotFlip(optDlg.m_bSkipRotFlip);
    mCamera->SetFrameBaseName(optDlg.m_strBasename);
    mCamera->SetFrameNameFormat(optDlg.mNameFormat);
    mCamera->SetFrameNumberStart(optDlg.m_iStartNumber);
    mCamera->SetDigitsForNumberedFrame(optDlg.mNumberDigits);
    ManageK2Binning();
    ManageK2SaveSummary();
    ManageDoseFrac();
  }
  m_butFileOptions.SetButtonStyle(BS_PUSHBUTTON);
}

// Set the folder for saving
void CCameraSetupDlg::OnSetSaveFolder()
{
  CString str = mCamera->GetDirForK2Frames();
  CString title = "SELECT folder for saving frames (typing name may not work)";
  if (mParam->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID)) {
    if (KGetOneString("Enter folder on Gatan server for saving frames:", str, 250,
      mCamera->GetNoK2SaveFolderBrowse() ? "" : 
      "SELECT (do not type in) folder accessible on Gatan server for saving frames"))
      mCamera->SetDirForK2Frames(str);
    m_butSetSaveFolder.SetButtonStyle(BS_PUSHBUTTON);
    return;
  }
  if (mFEItype) {
    mCamera->FixDirForFalconFrames(mParam);
    str = mCamera->GetDirForFalconFrames();
    if (FCAM_ADVANCED(mParam)) {
      if (KGetOneString("Frames can be saved in the Falcon storage location or a new or"
        " existing subfolder of it",
        "Enter name of subfolder to save frames in, or leave blank for none", str)) {
          if (str.FindOneOf("/\\") < 0)
            mCamera->SetDirForFalconFrames(str);
          else
            AfxMessageBox("You can enter only a single folder name without \\ or /");
      }
      m_butSetSaveFolder.SetButtonStyle(BS_PUSHBUTTON);
      return;
    }
  }
  if (str.IsEmpty())
    str = "C:\\";

  CXFolderDialog dlg(str);
  dlg.SetTitle(title);
  if (dlg.DoModal() == IDOK) {
    if (mParam->K2Type)
  		mCamera->SetDirForK2Frames(dlg.GetPath());
    else
  		mCamera->SetDirForFalconFrames(dlg.GetPath());
  }
  m_butSetSaveFolder.SetButtonStyle(BS_PUSHBUTTON);
}

// And a separate call for DE
void CCameraSetupDlg::OnDESetSaveFolder()
{
  CString str = mCamera->GetDirForDEFrames();
  if (KGetOneString("Here, you can specify a single subfolder under this camera's "
    "autosave directory", "Enter name of a new or existing subfolder to save frames in, "
    "or leave blank for none", str)) {
       if (str.FindOneOf("/\\") < 0)
         mCamera->SetDirForDEFrames(str);
       else
         AfxMessageBox("You can enter only a single folder name without \\ or /");
  }
  m_butDESetSaveFolder.SetButtonStyle(BS_PUSHBUTTON);
}

// Run dialog to set frame align parameters
void CCameraSetupDlg::OnButSetupAlign()
{
  CFrameAlignDlg dlg;
  CArray<FrameAliParams, FrameAliParams> *faParams = mCamera->GetFrameAliParams();
  int DMind = mParam->useSocket ? 1 : 0;
  BOOL *useGPU = mCamera->GetUseGPUforK2Align();
  dlg.mCurFiltInd = mCurSet->filterType;
  dlg.m_iWhereAlign = mCurSet->useFrameAlign;
  dlg.m_bUseFrameFolder = mCamera->GetComPathIsFramePath();
  dlg.mCurParamInd = mCurSet->faParamSetInd;
  if (mParam->K2Type) {
    dlg.mGPUavailable = mCamera->GetGpuAvailable(DMind);
    dlg.mUseGpuTransfer[0] = useGPU[DMind];
    dlg.mUseGpuTransfer[1] = useGPU[2];
    dlg.mServerIsRemote = mParam->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID);
  } else {
    dlg.mGPUavailable = mWinApp->mFalconHelper->GetGpuMemory() > 0;
    dlg.mUseGpuTransfer[0] = mWinApp->mFalconHelper->GetUseGpuForAlign(0);
    dlg.mUseGpuTransfer[1] = mWinApp->mFalconHelper->GetUseGpuForAlign(1);
    dlg.mServerIsRemote = mDE_Type ? false : CBaseSocket::ServerIsRemote(FEI_SOCK_ID);
  }
  dlg.mNewerK2API = mCamera->HasNewK2API(mParam);
  dlg.mEnableWhere = !(mCurrentSet == RECORD_CONSET && mStartedTS);
  dlg.mMoreParamsOpen = mWinApp->GetFrameAlignMoreOpen();
  dlg.m_bWholeSeries = mCamera->GetAlignWholeSeriesInIMOD();
  dlg.mCameraSelected = mActiveCameraList[mCurrentCamera];
  dlg.mConSetSelected = mCurrentSet;
  dlg.mReadMode = m_iK2Mode;
  
  // Set this so that actual value can be modified by whether aligning IMOD
  dlg.mTakingK3Binned = m_iProcessing == GAIN_NORMALIZED && 
    mCamera->GetTakeK3SuperResBinned() && !(mCamera->GetSaveUnnormalizedFrames() && 
    mUserSaveFrames);
  if (dlg.DoModal() == IDOK) {
    mCurSet->filterType = dlg.mCurFiltInd;
    mCurSet->useFrameAlign = dlg.m_iWhereAlign;
    mCurSet->faParamSetInd = dlg.mCurParamInd;
    if (mParam->K2Type) {
      useGPU[DMind] = dlg.mUseGpuTransfer[0];
      useGPU[2] = dlg.mUseGpuTransfer[1];
    } else {
      mWinApp->mFalconHelper->SetUseGpuForAlign(0, dlg.mUseGpuTransfer[0]);
      mWinApp->mFalconHelper->SetUseGpuForAlign(1, dlg.mUseGpuTransfer[1]);
    }
    mCamera->SetAlignWholeSeriesInIMOD(dlg.m_bWholeSeries);
    mCamera->SetComPathIsFramePath(dlg.m_bUseFrameFolder);
    if (mParam->FEItype == FALCON3_TYPE && !mCurSet->useFrameAlign)
      CheckFalconFrameSumList();
  } else {
    B3DCLAMP(mCurSet->faParamSetInd, 0 , (int)faParams->GetSize() - 1);  
  }
  mWinApp->SetFrameAlignMoreOpen(dlg.mMoreParamsOpen);
  m_butSetupAlign.SetButtonStyle(BS_PUSHBUTTON);
  m_butDEsetupAlign.SetButtonStyle(BS_PUSHBUTTON);
  ManageK2Binning();
  if (mDE_Type)
    ManageDEpanel();
  else
    ManageDoseFrac();
  ManageExposure();
}

// Manage the line that reports whether antialiasing will be applied
void CCameraSetupDlg::ManageAntialias(void)
{
  CString str = "";
  bool isSuperRes = mParam->K2Type && IS_SUPERRES(mParam, m_iK2Mode);
  bool antialias = (m_bDoseFracMode && m_iBinning > 1) || (isSuperRes && m_iBinning > 0)
    || (m_bAlwaysAntialias && mCamera->CAN_PLUGIN_DO(CAN_ANTIALIAS, mParam) &&
    m_iBinning > (isSuperRes ? 0 : 1) && m_iContSingle > 0);
  if (mParam->K2Type) {
    if ((isSuperRes && m_iBinning == 0) || (!isSuperRes && m_iBinning == 1)) {
      SetDlgItemText(IDC_STAT_ANTIALIAS, "");
    } else {
      SetDlgItemText(IDC_STAT_ANTIALIAS, antialias ? "Anti-aliasing" : "Binning");
    }
  }
}

// Manage the line with the summary about K2 saving
void CCameraSetupDlg::ManageK2SaveSummary(void)
{
  CString str;
  int dummy, frames;
  bool unNormed = m_iProcessing != GAIN_NORMALIZED || 
    (mCamera->GetSaveUnnormalizedFrames() && mCamera->GetPluginVersion(mParam) > 
    PLUGIN_CAN_GAIN_NORM);
  bool binning = mCamera->IsK3BinningSuperResFrames(mParam->K2Type, 
    m_bDoseFracMode ? 1 : 0, m_bSaveFrames ? 1 : 0, m_bAlignDoseFrac ? 1 : 0,
    mCurSet->useFrameAlign,  m_iProcessing, m_iK2Mode, m_bTakeK3Binned);
  bool reducing = !unNormed && IS_SUPERRES(mParam, m_iK2Mode) && !binning &&
    mCamera->GetSaveSuperResReduced() && mCamera->CAN_PLUGIN_DO(CAN_REDUCE_SUPER, mParam);
  if (mParam->K2Type && m_bDoseFracMode) {
    frames = B3DNINT(m_eExposure / B3DMAX(mCamera->GetMinK2FrameTime(mParam->K2Type),
      ActualFrameTime(m_fFrameTime)));
    int tiff = mCamera->GetK2SaveAsTiff();
    str.Format("%d frames", frames);
    SetDlgItemText(IDC_STAT_ALIGN_SUMMARY, str);
    if (m_bSaveK2Sums && mSummedFrameList.size() > 0)
      frames = mWinApp->mFalconHelper->GetFrameTotals(mSummedFrameList, dummy);
    str.Format("%d %s to %s%s%s%s", frames, unNormed ? "raw" : "norm",
      tiff > 0 ? (tiff > 1 ? "TIF-ZIP" : "TIF-LZW") : "MRC", 
      (reducing || binning) ? "" : (mCamera->GetOneK2FramePerFile() ?" files" : " stack"),
      (unNormed && m_iK2Mode > 0 && (mCamera->GetSaveRawPacked() & 1))? ", packed" : "",
      reducing ? ", reduced" : (binning ? ", binned" : ""));
    SetDlgItemText(IDC_STAT_SAVE_SUMMARY, str);
  } else if (mFalconCanSave) {
    frames = mWinApp->mFalconHelper->GetFrameTotals(mSummedFrameList, dummy);
    if (m_bDoseFracMode && m_bSaveFrames)
      str.Format("%d frames will be saved", frames);
    SetDlgItemText(IDC_STAT_SAVE_SUMMARY, str);
    if (mWeCanAlignFalcon && m_bDoseFracMode && m_bAlignDoseFrac) {
      str.Format("%d frames", frames);
      SetDlgItemText(IDC_STAT_ALIGN_SUMMARY, str);
    }
  }
}

// Run the frame summing setup dialog for Falcon or K2
void CCameraSetupDlg::OnSetupFalconFrames()
{
  UpdateData(true);
  CFalconFrameDlg dlg;
  dlg.mExposure = m_eExposure;
  dlg.mNumSkipBefore = (mParam->K2Type || mParam->FEItype == FALCON3_TYPE) ? 
    0 : mNumSkipBefore;
  dlg.mNumSkipAfter = (mParam->K2Type  || mParam->FEItype == FALCON3_TYPE) ?
    0 : mNumSkipAfter;
  dlg.mSummedFrameList = mSummedFrameList;
  dlg.mUserFrameFrac = mUserFrameFrac;
  dlg.mUserSubframeFrac = mUserSubframeFrac;
  dlg.mK2Type = mParam->K2Type> 0;
  dlg.mReadoutInterval = B3DCHOICE(mParam->K2Type, ActualFrameTime(m_fFrameTime),
    mCamera->GetFalconReadoutInterval());
  dlg.m_fSubframeTime = (float)(B3DNINT(dlg.mReadoutInterval * 2000.) / 2000.);
  dlg.mMaxFrames = mParam->K2Type ? 1000 : mCamera->GetMaxFalconFrames(mParam);
  dlg.mMaxPerFrame = 200;   // TODO: IS THERE AN ACTUAL OR USEFUL VALUE?
  dlg.mCamParams = mParam;
  dlg.mReadMode = m_iK2Mode;
  dlg.mAligningInFalcon = mParam->FEItype == FALCON3_TYPE && FCAM_CAN_ALIGN(mParam) &&
    m_bAlignDoseFrac && !mCurSet->useFrameAlign;
  if (dlg.DoModal() != IDOK) {
    m_butSetupFalconFrames.SetButtonStyle(BS_PUSHBUTTON);
    m_butSetupK2FrameSums.SetButtonStyle(BS_PUSHBUTTON);
    return;
  }
  m_eExposure = dlg.mExposure;
  if (mParam->K2Type)
    m_fFrameTime = RoundedDEframeTime(dlg.mReadoutInterval);
  UpdateData(false);
  mNumSkipBefore = dlg.mNumSkipBefore;
  mNumSkipAfter = dlg.mNumSkipAfter;
  mSummedFrameList = dlg.mSummedFrameList;
  mUserFrameFrac = dlg.mUserFrameFrac;
  mUserSubframeFrac = dlg.mUserSubframeFrac;
  ManageExposure();
  ManageAntialias();
  ManageK2SaveSummary();
  ManageDoseFrac();
  ManageDose();
  m_butSetupFalconFrames.SetButtonStyle(BS_PUSHBUTTON);
  m_butSetupK2FrameSums.SetButtonStyle(BS_PUSHBUTTON);
}

// Enable/disable saved summed frames from K2
void CCameraSetupDlg::OnSaveK2FrameSums()
{
  UpdateData(true);
  if (m_bSaveK2Sums) {
    if (!mSummedFrameList.size())
      OnSetupFalconFrames();
    if (!mSummedFrameList.size()) {
      m_bSaveK2Sums = false;
      UpdateData(false);
    } else {
      mWinApp->mFalconHelper->AdjustForExposure(mSummedFrameList, 0,
        0, m_eExposure, m_fFrameTime, mUserFrameFrac, mUserSubframeFrac, false);
      ManageExposure();
    }
  }
  ManageDoseFrac();
  ManageK2SaveSummary();
  ManageDose();
}

void CCameraSetupDlg::OnAlwaysAntialias()
{
  UpdateData(true);
  ManageAntialias();
}

void CCameraSetupDlg::OnTakeK3Binned()
{
  UpdateData(true);
  if (CheckFrameAliRestrictions(m_iK2Mode, mCamera->GetSaveUnnormalizedFrames(), 
    mUserSaveFrames, "setting for binning frames")) {
      m_bTakeK3Binned = !m_bTakeK3Binned;
      UpdateData(false);
      return;
  }
  ManageK2Binning();
  ManageK2SaveSummary();
  ManageDoseFrac();
}

// DE CAMERA:
// The master button for saving any frames
void CCameraSetupDlg::OnDeSaveMaster()
{
  UpdateData(true);
  mUserSaveFrames = m_bDEsaveMaster;
  if (!m_bDEalignFrames && !m_bDEsaveMaster && m_iDEMode == SUPERRES_MODE)
    m_iDEMode = COUNTING_MODE;
  ManageDEpanel();
  ManageExposure();
  UpdateData(false);
}

// Now this is for saving single frames
void CCameraSetupDlg::OnDeSaveFrames()
{
  UpdateData(true);
  ManageDEpanel();
  ManageExposure();
  UpdateData(false);
}

// Process a new sum count
void CCameraSetupDlg::OnKillfocusEditDeSumCount()
{
  UpdateData(true);
  ManageExposure();
  UpdateData(false);
}

void CCameraSetupDlg::OnDeAlignFrames()
{
  UpdateData(true);
  if (!m_bDEalignFrames && !m_bDEsaveMaster && m_iDEMode == SUPERRES_MODE)
    m_iDEMode = COUNTING_MODE;
  ManageDEpanel();
  ManageExposure();
  UpdateData(false);
}

// Manage the enables in the panel
void CCameraSetupDlg::ManageDEpanel(void)
{
  CString str;
  bool saving = m_bDEsaveMaster || m_bDEalignFrames;
  bool forceSaving = m_bDEalignFrames && mCurSet->useFrameAlign > 1 && mDEweCanAlign;
  if ((forceSaving && !m_bDEsaveMaster) || (!forceSaving && !BOOL_EQUIV(m_bDEsaveMaster,
    mUserSaveFrames))) {
    m_bDEsaveMaster = B3DCHOICE(forceSaving, true, mUserSaveFrames);
    UpdateData(false);
  }
  m_butNameSuffix.EnableWindow(!mParam->DE_AutosaveDir.IsEmpty() && saving);
  m_butDESetSaveFolder.EnableWindow(saving);
  m_editSumCount.EnableWindow(saving);
  m_butDESaveMaster.EnableWindow(!forceSaving);
  m_butDESaveFrames.EnableWindow(m_bDEsaveMaster && (m_iSumCount > 1 || m_iDEMode > 0));
  m_statDEframeTime.EnableWindow(saving);
  m_statDEframeSec.EnableWindow(saving);
  m_statDEwhereAlign.ShowWindow(m_bDEalignFrames && mDEweCanAlign);
  m_statDEsumNum.EnableWindow(saving);
  m_editDEframeTime.EnableWindow(saving);
  m_spinDEsumNum.EnableWindow(saving);
  m_butDEsetupAlign.EnableWindow(m_bDEalignFrames);
  m_butDEsuperRes.EnableWindow(saving);
  if (m_bDEalignFrames) {
    ComposeWhereAlign(str);
    SetDlgItemText(IDC_STAT_DE_WHERE_ALIGN, (LPCTSTR)str);
  }
}

// new frame time: adjust the sum count and then make sure it is legal
void CCameraSetupDlg::OnKillfocusDeFrameTime()
{
  UpdateData(true);
  m_iSumCount = B3DNINT(m_fDEframeTime * m_fDEfps);
  B3DCLAMP(m_iSumCount, 1, MAX_DE_SUM_COUNT);
  m_fDEframeTime = RoundedDEframeTime((float)(m_iSumCount / m_fDEfps));
  ManageExposure();
  UpdateData(false);
}

// Process new FPS value to appropriate camera parameter
void CCameraSetupDlg::OnKillfocusEditDeFPS()
{
  UpdateData(true);
  if (m_iDEMode > 0)
    mParam->DE_CountingFPS = m_fDEfps;
  else
    mParam->DE_FramesPerSec = m_fDEfps;
  OnKillfocusDeFrameTime();
  UpdateData(false);
}

// Convenience spinner for number of frames to sum
void CCameraSetupDlg::OnDeltaposSpinDeSumNum(NMHDR *pNMHDR, LRESULT *pResult)
{
  UpdateData(true);
  if (NewSpinnerValue(pNMHDR, pResult, m_iSumCount, 1, MAX_DE_SUM_COUNT, m_iSumCount))
    return;
  m_butDESaveFrames.EnableWindow(m_bDEsaveMaster && (m_iSumCount > 1 || m_iDEMode > 0));
  ManageExposure();
  UpdateData(false);
}

// Round the DE frame time for display
float CCameraSetupDlg::RoundedDEframeTime(float frameTime)
{
  float roundFac = 0.0005f;
  return roundFac * B3DNINT(frameTime / roundFac);
}


float CCameraSetupDlg::ActualFrameTime(float roundedTime)
{
  if (mParam->K2Type)
    mCamera->ConstrainFrameTime(roundedTime, mParam->K2Type);
  return roundedTime;
}
