// TSSetupdialog.cpp:    To set lots of parameters for a tilt series
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\TSSetupDialog.h"
#include "ShiftManager.h"
#include "FocusManager.h"
#include "TSController.h"
#include "CameraController.h"
#include "BeamAssessor.h"
#include "ComplexTasks.h"
#include "MultiTSTasks.h"
#include "TSVariationsDlg.h"
#include "TSDoseSymDlg.h"
#include "DriftWaitSetupDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define NUM_LEFT_PANELS 3

/////////////////////////////////////////////////////////////////////////////
// CTSSetupDialog dialog

// HERE IS HOW TO ENABLE TOOL TIPS IN A DIALOG:
//
//  Add to message map (outside VC++ maintained part)
// ON_NOTIFY_EX_RANGE( TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify )
// Put  EnableToolTips(true);  in initdialog
// Add the function  OnToolTipNotify
// All of which is taken care of by inheriting CBaseDlg!

// Put the order of the radio buttons here
static int beamCodes[3] = {BEAM_CONSTANT, BEAM_INVERSE_COSINE, BEAM_INTENSITY_FOR_MEAN};

static int idTable[] = {
  IDC_TSS_PLUS1, 0, 0, IDC_TSS_TITLE1, IDC_TSS_LINE1,
  IDC_STATSTARTAT, IDC_EDITSTARTANGLE, IDC_STATENDAT, IDC_EDITENDANGLE, IDC_STATBASICINC,
  IDC_EDITINCREMENT, IDC_COSINEINC, IDC_STATTOTALTILT, IDC_STATDELAY, IDC_EDITTILTDELAY,
  IDC_STATDELSEC, IDC_CHECK_VARY_PARAMS, IDC_BUT_SET_CHANGES, IDC_BUT_SWAP_ANGLES, 
  IDC_DO_BIDIR, IDC_BIDIR_ANGLE, IDC_BIDIR_WALK_BACK, IDC_STAT_BDMAG_LABEL,
  IDC_STAT_BIDIR_MAG, IDC_SPIN_BIDIR_MAG, IDC_BIDIR_USE_VIEW, IDC_STAT_BIDIR_FIELD_SIZE,
  IDC_USE_DOSE_SYM, IDC_BUT_SETUP_DOSE_SYM, IDC_STAT_STAR_TILT, IDC_STAT_STAR_BIDIR,
  PANEL_END,
  IDC_TSS_PLUS2, 0, 0, IDC_TSS_TITLE2, IDC_TSS_LINE2,
  IDC_STATMAGLABEL, IDC_STATRECORDMAG,IDC_SPINRECORDMAG,  IDC_STATBINLABEL, 
  IDC_STATBINNING, IDC_SPINBIN, IDC_STATPIXEL, IDC_LOWMAGTRACK, IDC_STATLOWMAG,
  IDC_SPINTRACKMAG, IDC_STATLIMITIS, IDC_STATISMICRONS, IDC_EDITLIMITIS,
  IDC_CENTER_FROM_TRIAL, IDC_AUTO_REFINE_ZLP, IDC_EDIT_ZLP_INTERVAL, IDC_STATMINUTES,
  IDC_AUTOCEN_AT_INTERVAL, IDC_EDIT_AC_INTERVAL, IDC_STAT_ACMINUTES, IDC_STAT_INTERSET,
  IDC_AUTOCEN_AT_CROSSING, IDC_EDIT_AC_ANGLE, IDC_STAT_ACDEGREES, PANEL_END,
  IDC_TSS_PLUS3, 0, 0, IDC_TSS_TITLE3, IDC_TSS_LINE3, IDC_CHANGE_EXPOSURE,
  IDC_RCONSTANTBEAM, IDC_RINTENSITYCOSINE, IDC_RINTENSITYMEAN, IDC_SPINCOSINEPOWER,
  IDC_STATPOWER, IDC_EDITCOUNTS, IDC_STATCURRENTCOUNTS, IDC_RAMPBEAM, IDC_EDITTAPERCOUNTS
  , IDC_STATTAPERFTA, IDC_EDITTAPERANGLE, IDC_STATTAPERDEG, IDC_LIMITTOCURRENT,
  IDC_LIMITINTENSITY, IDC_USEINTENSITYATZERO, IDC_STATTOTALDOSE, IDC_CHANGE_ALL_EXP,
  IDC_CHANGE_SETTLING, PANEL_END,
  IDC_TSS_PLUS4, 0, 0, IDC_TSS_TITLE4, IDC_TSS_LINE4,
  IDC_RCAMERA1, IDC_RCAMERA2, IDC_RCAMERA3, IDC_RCAMERA4, IDC_RCAMERA5, IDC_RCAMERA6, 
  IDC_AVERAGERECREF, IDC_STAT_FRAME_LABEL, IDC_DO_EARLY_RETURN, IDC_SPIN_EARLY_FRAMES,
  IDC_STAT_NUM_EARLY_FRAMES, PANEL_END,
  IDC_TSS_PLUS5, 0, 0, IDC_TSS_TITLE5, IDC_TSS_LINE5,
  IDC_STATDEFTARG, IDC_STATTARGMICRONS, IDC_STATBEAMTILT, IDC_STATFOCUSOFFSET, 
  IDC_STATOFFSETMICRONS, IDC_STAT_ITER_THRESH, IDC_STATTHRESHMICRONS, IDC_EDIT_ITER_THRESH
  , IDC_EDITFOCUSTARGET, IDC_EDITBEAMTILT, IDC_STATMILLIRAD, IDC_EDITFOCUSOFFSET,
  IDC_SKIPAUTOFOCUS, IDC_STATCHECKEVERY, IDC_EDITCHECKFOCUS, IDC_STATCHECKDEG,
  IDC_ALWAYSFOCUS, IDC_EDITALWAYSFOCUS, IDC_STATALWAYSFOCUS, IDC_CHECKAUTOFOCUS,
  IDC_STATREPFOCUSUM, IDC_STATDOFOCUSUM, IDC_REPEATFOCUS, IDC_STATDOAUTOFOC,
  IDC_EDITREPEATFOCUS, IDC_EDITPREDICTERRORZ,
  IDC_EDITMINFOCUSRATIO, IDC_STATTIMESACTUAL, IDC_CHECK_LIMIT_DELTA_FOCUS,
  IDC_CHECK_LIMIT_ABS_FOCUS, IDC_EDIT_FOCUS_MAX_DELTA, IDC_STAT_DELTA_MICRONS, PANEL_END,
  IDC_TSS_PLUS6, 0, 0, IDC_TSS_TITLE6, IDC_TSS_LINE6,
  IDC_REFINEEUCEN, IDC_LEAVEANCHOR, IDC_EDITANCHOR, IDC_STATANCHORDEG,IDC_USEAFORREF,
  IDC_USEANCHOR, IDC_STATANCHORBUF, IDC_SPINANCHOR, IDC_CLOSE_VALVES, PANEL_END,
  IDC_TSS_PLUS7, 0, 0, IDC_TSS_TITLE7, IDC_TSS_LINE7, 
  IDC_TSWAITFORDRIFT, IDC_BUT_SETUP_DRIFT_WAIT,
  IDC_REPEATRECORD, IDC_EDITREPEATRECORD, IDC_CONFIRM_LOWDOSE_REPEAT,
  IDC_EDITPREDICTERRORXY, IDC_RTRACKBEFORE, IDC_STAT_DW_DEGREES,
  IDC_RTRACKAFTER, IDC_RTRACKBOTH, IDC_ALIGNTRACKONLY, IDC_PREVIEWBEFOREREF,
  IDC_STATNEWTRACK, IDC_EDITNEWTRACKDIFF, IDC_STATPERCENT, IDC_STOPONBIGSHIFT, 
  IDC_EDITBIGSHIFT, IDC_MANUAL_TRACKING, IDC_STATPCTIMAGE, IDC_STATGETTRACK,
  IDC_STATPCTFIELD, IDC_STATTRACKWHEN, PANEL_END,
  IDC_BUT_TSS_PREV, IDC_BUT_TSS_NEXT, IDC_BUT_TSS_LEFT, IDC_BUT_TSS_RIGHT, 
  IDC_BUT_TSS_FULL, IDC_TSGO, IDC_SINGLESTEP, IDC_POSTPONE, IDCANCEL, 
  IDC_BUTHELP, PANEL_END, TABLE_END};

static int topTable[sizeof(idTable) / sizeof(int)];
static int heightTable[sizeof(idTable) / sizeof(int)];
static int leftTable[sizeof(idTable) / sizeof(int)];


CTSSetupDialog::CTSSetupDialog(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CTSSetupDialog::IDD, pParent)
  , m_bCenterBeam(FALSE)
  , m_bVaryParams(FALSE)
  , m_bChangeAllExp(FALSE)
  , m_bChangeSettling(FALSE)
  , m_bChangeExposure(FALSE)
  , m_bAutocenInterval(FALSE)
  , m_iAutocenInterval(1)
  , m_bAutocenCrossing(FALSE)
  , m_fAutocenAngle(1.f)
  , m_bConfirmLowDoseRepeat(FALSE)
  , m_strInterset(_T(""))
  , m_bUseDoseSym(FALSE)
  , m_bWaitForDrift(FALSE)
  , m_fIterThresh(1.)
{
  //{{AFX_DATA_INIT(CTSSetupDialog)
  m_bCosineInc = FALSE;
  m_fAnchorAngle = 0.0f;
  m_fBeamTilt = 0.0f;
  m_fCheckFocus = 0.0f;
  m_iCounts = 0;
  m_fEndAngle = 0.0f;
  m_fIncrement = 0.05f;
  m_fLimitIS = 0.0f;
  m_fRepeatFocus = 0.0f;
  m_fStartAngle = 0.0f;
  m_fTiltDelay = 0.0f;
  m_bLeaveAnchor = FALSE;
  m_bLowMagTrack = FALSE;
  m_iBeamControl = -1;
  m_bRefineEucen = FALSE;
  m_bRepeatFocus = FALSE;
  m_bRepeatRecord = FALSE;
  m_strBinning = _T("");
  m_strLowMag = _T("");
  m_strPixel = _T("");
  m_strRecordMag = _T("");
  m_strTotalTilt = _T("");
  m_bUseAnchor = FALSE;
  m_fRepeatRecord = 0.0f;
  m_fTargetDefocus = 0.0f;
  m_fFocusMaxDelta = 20.0f;
  m_bLimitDeltaFocus = FALSE;
  m_bLimitAbsFocus = FALSE;
  m_bUseAForRef = FALSE;
  m_bAlwaysFocus = FALSE;
  m_fAlwaysAngle = 0.0f;
  m_bRampBeam = FALSE;
  m_fTaperAngle = 0.0f;
  m_iTaperCounts = 0;
  m_fNewTrackDiff = 0.2f;
  m_bAlignTrackOnly = FALSE;
  m_fPredictErrorXY = 0.0f;
  m_fPredictErrorZ = 0.0f;
  m_iCamera = -1;
  m_bCheckAutofocus = FALSE;
  m_fMinFocusAccuracy = 0.0f;
	m_bLimitIntensity = FALSE;
	m_bPreviewBeforeRef = FALSE;
	m_bSkipAutofocus = FALSE;
	m_fFocusOffset = 0.0f;
	m_iTrackAfterFocus = -1;
	m_bLimitToCurrent = FALSE;
	m_strTotalDose = _T("");
	m_bIntensitySetAtZero = FALSE;
	m_bRefineZLP = FALSE;
	m_iZlpInterval = 0;
	m_statMilliRad = _T("milliradians");
	m_bStopOnBigShift = FALSE;
	m_fBigShift = 0.0f;
	m_bCloseValves = FALSE;
	m_bManualTrack = FALSE;
	m_bAverageDark = FALSE;
  m_bDoBidir = FALSE;
  m_bBidirWalkBack = FALSE;
  m_bBidirUseView = FALSE;
  m_fBidirAngle = 0.;
  m_strBidirMag = _T("");
  m_strNumEarlyFrames = _T("");
  m_strCosPower = _T("");
  m_bDoEarlyReturn = FALSE;
  mMaxTiltAngle = ((CSerialEMApp *)AfxGetApp())->mScope->GetMaxTiltAngle();
  if (mMaxTiltAngle < -900.)
    mMaxTiltAngle = 90.f;
  mMaxDelayAfterTilt = 15.f;
	//}}AFX_DATA_INIT
}


void CTSSetupDialog::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CTSSetupDialog)
  DDX_Control(pDX, IDC_AVERAGERECREF, m_butAverageDark);
  DDX_Control(pDX, IDC_CLOSE_VALVES, m_butCloseValves);
  DDX_Control(pDX, IDC_EDITBIGSHIFT, m_editBigShift);
  DDX_Control(pDX, IDC_AUTO_REFINE_ZLP, m_butRefineZLP);
  DDX_Control(pDX, IDC_EDIT_ZLP_INTERVAL, m_editZlpInterval);
  DDX_Control(pDX, IDC_STATMINUTES, m_statMinutes);
  DDX_Control(pDX, IDC_USEINTENSITYATZERO, m_butIntensitySetAtZero);
  DDX_Control(pDX, IDC_STATCURRENTCOUNTS, m_statCurrentCounts);
  DDX_Control(pDX, IDC_LIMITTOCURRENT, m_butLimitToCurrent);
  DDX_Control(pDX, IDC_SPINCOSINEPOWER, m_sbcCosinePower);
  DDX_Control(pDX, IDC_EDITCHECKFOCUS, m_editCheckFocus);
  DDX_Control(pDX, IDC_ALWAYSFOCUS, m_butAlwaysFocus);
  DDX_Control(pDX, IDC_SKIPAUTOFOCUS, m_butSkipAutofocus);
  DDX_Control(pDX, IDC_STATCHECKDEG, m_statCheckDeg);
  DDX_Control(pDX, IDC_STATCHECKEVERY, m_statCheckEvery);
  DDX_Control(pDX, IDC_STATALWAYSFOCUS, m_statAlwaysFocus);
  DDX_Control(pDX, IDC_PREVIEWBEFOREREF, m_butPreviewBeforeRef);
  DDX_Control(pDX, IDC_LIMITINTENSITY, m_butLimitIntensity);
  DDX_Control(pDX, IDC_STATTIMESACTUAL, m_statTimesActual);
  DDX_Control(pDX, IDC_CHECKAUTOFOCUS, m_butCheckAutofocus);
  DDX_Control(pDX, IDC_EDITMINFOCUSRATIO, m_editMinFocusAccuracy);
  DDX_Control(pDX, IDC_ALIGNTRACKONLY, m_butAlignTrackOnly);
  DDX_Control(pDX, IDC_STATPERCENT, m_statPercent);
  DDX_Control(pDX, IDC_STATNEWTRACK, m_statNewTrack);
  DDX_Control(pDX, IDC_EDITNEWTRACKDIFF, m_editNewTrackDiff);
  DDX_Control(pDX, IDC_RCONSTANTBEAM, m_butConstantBeam);
  DDX_Control(pDX, IDC_RINTENSITYMEAN, m_butIntensityMean);
  DDX_Control(pDX, IDC_RINTENSITYCOSINE, m_butIntensityCosine);
  DDX_Control(pDX, IDC_USEAFORREF, m_butUseAforRef);
  DDX_Control(pDX, IDC_EDITTAPERANGLE, m_editTaperAngle);
  DDX_Control(pDX, IDC_EDITTAPERCOUNTS, m_editTaperCounts);
  DDX_Control(pDX, IDC_RAMPBEAM, m_butRampBeam);
  DDX_Control(pDX, IDC_STATTAPERFTA, m_statTaperFta);
  DDX_Control(pDX, IDC_STATTAPERDEG, m_statTaperDeg);
  DDX_Control(pDX, IDC_EDITALWAYSFOCUS, m_editAlwaysAngle);
  DDX_Control(pDX, IDC_STATLOWMAG, m_statLowMag);
  DDX_Control(pDX, IDC_EDITREPEATRECORD, m_editRepeatRecord);
  DDX_Control(pDX, IDC_EDITREPEATFOCUS, m_editRepeatFocus);
  DDX_Control(pDX, IDC_EDITCOUNTS, m_editCounts);
  DDX_Control(pDX, IDC_USEANCHOR, m_butUseAnchor);
  DDX_Control(pDX, IDC_STATSTARTAT, m_statStartAt);
  DDX_Control(pDX, IDC_STATENDAT, m_statEndAt);
  DDX_Control(pDX, IDC_STATRECORDMAG, m_statRecordMag);
  DDX_Control(pDX, IDC_STATMAGLABEL, m_statMagLabel);
  DDX_Control(pDX, IDC_STATBINNING, m_statBinning);
  DDX_Control(pDX, IDC_STATBINLABEL, m_statBinLabel);
  DDX_Control(pDX, IDC_STATBASICINC, m_statbasicInc);
  DDX_Control(pDX, IDC_STATANCHORDEG, m_statAnchorDeg);
  DDX_Control(pDX, IDC_STATANCHORBUF, m_statAnchorBuf);
  DDX_Control(pDX, IDC_SPINTRACKMAG, m_sbcTrackMag);
  DDX_Control(pDX, IDC_SPINRECORDMAG, m_sbcRecordMag);
  DDX_Control(pDX, IDC_SPINBIN, m_sbcBin);
  DDX_Control(pDX, IDC_SPINANCHOR, m_sbcAnchor);
  DDX_Control(pDX, IDC_REFINEEUCEN, m_butRefineEucen);
  DDX_Control(pDX, IDC_LOWMAGTRACK, m_butLowMagTrack);
  DDX_Control(pDX, IDC_LEAVEANCHOR, m_butLeaveAnchor);
  DDX_Control(pDX, IDC_EDITSTARTANGLE, m_editStartAngle);
  DDX_Control(pDX, IDC_EDITENDANGLE, m_editEndAngle);
  DDX_Control(pDX, IDC_EDITINCREMENT, m_editIncrement);
  DDX_Control(pDX, IDC_EDITANCHOR, m_editAnchor);
  DDX_Control(pDX, IDC_COSINEINC, m_butCosineInc);
  DDX_Control(pDX, IDC_CHECK_LIMIT_ABS_FOCUS, m_butLimitAbsFocus);
  DDX_Control(pDX, IDC_CHECK_LIMIT_DELTA_FOCUS, m_butLimitDeltaFocus);
  DDX_Check(pDX, IDC_CHECK_LIMIT_ABS_FOCUS, m_bLimitAbsFocus);
  DDX_Check(pDX, IDC_CHECK_LIMIT_DELTA_FOCUS, m_bLimitDeltaFocus);
  DDX_Control(pDX, IDC_STAT_DELTA_MICRONS, m_statDeltaMicrons);
  DDX_Control(pDX, IDC_EDIT_FOCUS_MAX_DELTA, m_editFocusMaxDelta);
  DDX_Text(pDX, IDC_EDIT_FOCUS_MAX_DELTA, m_fFocusMaxDelta);
  DDV_MinMaxFloat(pDX, m_fFocusMaxDelta, 0.1f, 100.f);
  DDX_Check(pDX, IDC_COSINEINC, m_bCosineInc);
  DDX_Text(pDX, IDC_EDITANCHOR, m_fAnchorAngle);
  DDV_MinMaxFloat(pDX, m_fAnchorAngle, -80.f, 80.f);
  DDX_Text(pDX, IDC_EDITBEAMTILT, m_fBeamTilt);
  DDX_Text(pDX, IDC_EDITCHECKFOCUS, m_fCheckFocus);
  DDX_Text(pDX, IDC_EDITCOUNTS, m_iCounts);
  DDV_MinMaxInt(pDX, m_iCounts, 0, 1000000);
  DDX_Text(pDX, IDC_EDITENDANGLE, m_fEndAngle);
  DDV_MinMaxFloat(pDX, m_fEndAngle, -mMaxTiltAngle, mMaxTiltAngle);
  DDX_Text(pDX, IDC_EDITINCREMENT, m_fIncrement);
  DDV_MinMaxFloat(pDX, m_fIncrement, 0.05f, 30.f);
  DDX_Control(pDX, IDC_STATLIMITIS, m_statLimitIS);
  DDX_Control(pDX, IDC_EDITLIMITIS, m_editLimitIS);
  DDX_Text(pDX, IDC_EDITLIMITIS, m_fLimitIS);
  DDV_MinMaxFloat(pDX, m_fLimitIS, 0.f, 25.f);
  DDX_Text(pDX, IDC_EDITREPEATFOCUS, m_fRepeatFocus);
  DDV_MinMaxFloat(pDX, m_fRepeatFocus, 0.f, 20.f);
  DDX_Text(pDX, IDC_EDITSTARTANGLE, m_fStartAngle);
  DDV_MinMaxFloat(pDX, m_fStartAngle, -mMaxTiltAngle, mMaxTiltAngle);
  DDX_Text(pDX, IDC_EDITTILTDELAY, m_fTiltDelay);
  DDV_MinMaxFloat(pDX, m_fTiltDelay, 0.f, mMaxDelayAfterTilt);
  DDX_Check(pDX, IDC_LEAVEANCHOR, m_bLeaveAnchor);
  DDX_Check(pDX, IDC_LOWMAGTRACK, m_bLowMagTrack);
  DDX_Radio(pDX, IDC_RCONSTANTBEAM, m_iBeamControl);
  DDX_Check(pDX, IDC_REFINEEUCEN, m_bRefineEucen);
  DDX_Check(pDX, IDC_REPEATFOCUS, m_bRepeatFocus);
  DDX_Check(pDX, IDC_REPEATRECORD, m_bRepeatRecord);
  DDX_Text(pDX, IDC_STATBINNING, m_strBinning);
  DDX_Text(pDX, IDC_STATLOWMAG, m_strLowMag);
  DDX_Text(pDX, IDC_STATPIXEL, m_strPixel);
  DDX_Text(pDX, IDC_STATRECORDMAG, m_strRecordMag);
  DDX_Text(pDX, IDC_STATTOTALTILT, m_strTotalTilt);
  DDX_Text(pDX, IDC_STATPOWER, m_strCosPower);
  DDX_Check(pDX, IDC_USEANCHOR, m_bUseAnchor);
  DDX_Text(pDX, IDC_EDITREPEATRECORD, m_fRepeatRecord);
  DDV_MinMaxFloat(pDX, m_fRepeatRecord, 0.f, 99.f);
  DDX_Text(pDX, IDC_EDITFOCUSTARGET, m_fTargetDefocus);
  DDV_MinMaxFloat(pDX, m_fTargetDefocus, -25.f, 25.f);
  DDX_Check(pDX, IDC_USEAFORREF, m_bUseAForRef);
  DDX_Check(pDX, IDC_ALWAYSFOCUS, m_bAlwaysFocus);
  DDX_Text(pDX, IDC_EDITALWAYSFOCUS, m_fAlwaysAngle);
  DDX_Check(pDX, IDC_RAMPBEAM, m_bRampBeam);
  DDX_Text(pDX, IDC_EDITTAPERANGLE, m_fTaperAngle);
  DDV_MinMaxFloat(pDX, m_fTaperAngle, 0.f, 90.f);
  DDX_Text(pDX, IDC_EDITTAPERCOUNTS, m_iTaperCounts);
  DDV_MinMaxInt(pDX, m_iTaperCounts, 0, 60000);
  DDX_Text(pDX, IDC_EDITNEWTRACKDIFF, m_fNewTrackDiff);
  DDV_MinMaxFloat(pDX, m_fNewTrackDiff, 0.2f, 100.f);
  DDX_Check(pDX, IDC_ALIGNTRACKONLY, m_bAlignTrackOnly);
  DDX_Text(pDX, IDC_EDITPREDICTERRORXY, m_fPredictErrorXY);
  DDV_MinMaxFloat(pDX, m_fPredictErrorXY, 0.f, 100.f);
  DDX_Text(pDX, IDC_EDITPREDICTERRORZ, m_fPredictErrorZ);
  DDV_MinMaxFloat(pDX, m_fPredictErrorZ, 0.f, 10.f);
  DDX_Radio(pDX, IDC_RCAMERA1, m_iCamera);
  DDX_Check(pDX, IDC_CHECKAUTOFOCUS, m_bCheckAutofocus);
  DDX_Text(pDX, IDC_EDITMINFOCUSRATIO, m_fMinFocusAccuracy);
  DDV_MinMaxFloat(pDX, m_fMinFocusAccuracy, 0.f, 1.f);
  DDX_Check(pDX, IDC_LIMITINTENSITY, m_bLimitIntensity);
  DDX_Check(pDX, IDC_PREVIEWBEFOREREF, m_bPreviewBeforeRef);
  DDX_Check(pDX, IDC_SKIPAUTOFOCUS, m_bSkipAutofocus);
  DDX_Text(pDX, IDC_EDITFOCUSOFFSET, m_fFocusOffset);
  DDV_MinMaxFloat(pDX, m_fFocusOffset, -30.f, 30.f);
  DDX_Radio(pDX, IDC_RTRACKBEFORE, m_iTrackAfterFocus);
  DDX_Check(pDX, IDC_LIMITTOCURRENT, m_bLimitToCurrent);
  DDX_Text(pDX, IDC_STATTOTALDOSE, m_strTotalDose);
  DDX_Check(pDX, IDC_USEINTENSITYATZERO, m_bIntensitySetAtZero);
  DDX_Check(pDX, IDC_AUTO_REFINE_ZLP, m_bRefineZLP);
  DDX_Text(pDX, IDC_EDIT_ZLP_INTERVAL, m_iZlpInterval);
  DDV_MinMaxInt(pDX, m_iZlpInterval, 0, 600);
  DDX_Text(pDX, IDC_STATMILLIRAD, m_statMilliRad);
  DDX_Check(pDX, IDC_STOPONBIGSHIFT, m_bStopOnBigShift);
  DDX_Text(pDX, IDC_EDITBIGSHIFT, m_fBigShift);
  DDV_MinMaxFloat(pDX, m_fBigShift, 0.f, 100.f);
  DDX_Check(pDX, IDC_CLOSE_VALVES, m_bCloseValves);
  DDX_Check(pDX, IDC_MANUAL_TRACKING, m_bManualTrack);
  DDX_Check(pDX, IDC_AVERAGERECREF, m_bAverageDark);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_POSTPONE, m_butPostpone);
  DDX_Control(pDX, IDC_TSGO, m_butGo);
  DDX_Control(pDX, IDC_SINGLESTEP, m_butSingleLoop);
  DDX_Control(pDX, IDC_CENTER_FROM_TRIAL, m_butCenterBeam);
  DDX_Check(pDX, IDC_CENTER_FROM_TRIAL, m_bCenterBeam);
  DDX_Control(pDX, IDC_BUT_TSS_PREV, m_butPrev);
  DDX_Control(pDX, IDC_BUT_TSS_NEXT, m_butNext);
  DDX_Control(pDX, IDC_BUT_TSS_LEFT, m_butLeft);
  DDX_Control(pDX, IDC_BUT_TSS_RIGHT, m_butRight);
  DDX_Control(pDX, IDC_BUT_TSS_FULL, m_butFull);
  DDX_Control(pDX, IDC_TSS_VERTLINE, m_statVertline);
  DDX_Control(pDX, IDC_MANUAL_TRACKING, m_butManualTrack);
  DDX_Control(pDX, IDC_BUT_SET_CHANGES, m_butSetChanges);
  DDX_Check(pDX, IDC_CHECK_VARY_PARAMS, m_bVaryParams);
  DDX_Control(pDX, IDC_CHANGE_ALL_EXP, m_butChangeAllExp);
  DDX_Control(pDX, IDC_CHANGE_SETTLING, m_butChangeSettling);
  DDX_Control(pDX, IDC_CHANGE_EXPOSURE, m_butChangeExposure);
  DDX_Check(pDX, IDC_CHANGE_ALL_EXP, m_bChangeAllExp);
  DDX_Check(pDX, IDC_CHANGE_EXPOSURE, m_bChangeExposure);
  DDX_Check(pDX, IDC_CHANGE_SETTLING, m_bChangeSettling);
  DDX_Control(pDX, IDC_CHECK_VARY_PARAMS, m_butVaryParams);
  DDX_Control(pDX, IDC_EDITFOCUSTARGET, m_editFocusTarget);
  DDX_Control(pDX, IDC_AUTOCEN_AT_INTERVAL, m_butAutocenInterval);
  DDX_Control(pDX, IDC_EDIT_AC_INTERVAL, m_editAutocenInterval);
  DDX_Control(pDX, IDC_STAT_ACMINUTES, m_statACminutes);
  DDX_Control(pDX, IDC_STAT_ACDEGREES, m_statACdegrees);
  DDX_Control(pDX, IDC_AUTOCEN_AT_CROSSING, m_butAutocenCrossing);
  DDX_Control(pDX, IDC_EDIT_AC_ANGLE, m_editAutocenAngle);
  DDX_Check(pDX, IDC_AUTOCEN_AT_INTERVAL, m_bAutocenInterval);
  DDX_Check(pDX, IDC_AUTOCEN_AT_CROSSING, m_bAutocenCrossing);
  DDX_Text(pDX, IDC_EDIT_AC_INTERVAL, m_iAutocenInterval);
  DDV_MinMaxInt(pDX, m_iAutocenInterval, 1, 600);
  DDX_Text(pDX, IDC_EDIT_AC_ANGLE, m_fAutocenAngle);
  DDV_MinMaxFloat(pDX, m_fAutocenAngle, 1.f, 100.f);
  DDX_Control(pDX, IDC_BUT_SWAP_ANGLES, m_butSwapAngles);
  DDX_Control(pDX, IDC_EDITFOCUSOFFSET, m_editFocusOffset);
  DDX_Control(pDX, IDC_EDITBEAMTILT, m_editBeamTilt);
  DDX_Control(pDX, IDC_CONFIRM_LOWDOSE_REPEAT, m_butConfirmLowDoseRepeat);
  DDX_Check(pDX, IDC_CONFIRM_LOWDOSE_REPEAT, m_bConfirmLowDoseRepeat);
  DDX_Text(pDX, IDC_STAT_INTERSET, m_strInterset);
  DDX_Control(pDX, IDC_STAT_INTERSET, m_statInterset);
  DDX_Control(pDX, IDC_DO_BIDIR, m_butDoBidir);
  DDX_Check(pDX, IDC_DO_BIDIR, m_bDoBidir);
  DDX_Control(pDX, IDC_BIDIR_WALK_BACK, m_butBidirWalkBack);
  DDX_Check(pDX, IDC_BIDIR_WALK_BACK, m_bBidirWalkBack);
  DDX_Control(pDX, IDC_BIDIR_USE_VIEW, m_butBidirUseView);
  DDX_Check(pDX, IDC_BIDIR_USE_VIEW, m_bBidirUseView);
  DDX_Control(pDX, IDC_STAT_BDMAG_LABEL, m_statBDMagLabel);
  DDX_Text(pDX, IDC_STAT_BIDIR_MAG, m_strBidirMag);
  DDX_Control(pDX, IDC_STAT_BIDIR_MAG, m_statBidirMag);
  DDX_Text(pDX, IDC_STAT_BIDIR_FIELD_SIZE, m_strBidirFieldSize);
  DDX_Control(pDX, IDC_STAT_BIDIR_FIELD_SIZE, m_statBidirFieldSize);
  DDX_Control(pDX, IDC_SPIN_BIDIR_MAG, m_sbcBidirMag);
  DDX_Control(pDX, IDC_BIDIR_ANGLE, m_editBidirAngle);
  DDX_Text(pDX, IDC_BIDIR_ANGLE, m_fBidirAngle);
  DDV_MinMaxFloat(pDX, m_fBidirAngle, -80.f, 80.f);
  DDX_Check(pDX, IDC_DO_EARLY_RETURN, m_bDoEarlyReturn);
  DDX_Control(pDX, IDC_DO_EARLY_RETURN, m_butDoEarlyReturn);
  DDX_Text(pDX, IDC_STAT_NUM_EARLY_FRAMES, m_strNumEarlyFrames);
  DDX_Control(pDX, IDC_STAT_NUM_EARLY_FRAMES, m_statNumEarlyFrames);
  DDX_Control(pDX, IDC_SPIN_EARLY_FRAMES, m_sbcEarlyFrames);
  DDX_Control(pDX, IDC_STAT_FRAME_LABEL, m_statFrameLabel);
  DDX_Control(pDX, IDC_USE_DOSE_SYM, m_butUseDoseSym);
  DDX_Check(pDX, IDC_USE_DOSE_SYM, m_bUseDoseSym);
  DDX_Control(pDX, IDC_BUT_SETUP_DOSE_SYM, m_butSetupDoseSym);
  DDX_Control(pDX, IDC_STAT_DW_DEGREES, m_statDWdegrees);
  DDX_Check(pDX, IDC_TSWAITFORDRIFT, m_bWaitForDrift);
  DDX_Control(pDX, IDC_BUT_SETUP_DRIFT_WAIT, m_butSetupDriftWait);
  DDX_Text(pDX, IDC_EDIT_ITER_THRESH, m_fIterThresh);
  DDV_MinMaxFloat(pDX, m_fIterThresh, 0.05f, 50.f);
  DDX_Control(pDX, IDC_STAT_STAR_TILT, m_statStarTilt);
  DDX_Control(pDX, IDC_STAT_STAR_BIDIR, m_statStarBidir);
  DDX_Control(pDX, IDC_STATTARGMICRONS, m_statTargMicrons);
}


BEGIN_MESSAGE_MAP(CTSSetupDialog, CBaseDlg)
  //{{AFX_MSG_MAP(CTSSetupDialog)
  ON_EN_KILLFOCUS(IDC_EDITSTARTANGLE, OnAngleChanges)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINRECORDMAG, OnDeltaposSpinrecordmag)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINANCHOR, OnDeltaposSpinanchor)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINBIN, OnDeltaposSpinbin)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINTRACKMAG, OnDeltaposSpintrackmag)
  ON_BN_CLICKED(IDC_LOWMAGTRACK, OnLowmagtrack)
  ON_BN_CLICKED(IDC_LEAVEANCHOR, OnLeaveanchor)
  ON_BN_CLICKED(IDC_USEANCHOR, OnUseanchor)
  ON_BN_CLICKED(IDC_REPEATRECORD, OnRepeatrecord)
  ON_BN_CLICKED(IDC_REPEATFOCUS, OnRepeatfocus)
  ON_BN_CLICKED(IDC_RINTENSITYMEAN, OnRintensitymean)
  ON_BN_CLICKED(IDC_POSTPONE, OnPostpone)
  ON_BN_CLICKED(IDC_SINGLESTEP, OnSinglestep)
  ON_BN_CLICKED(IDC_ALWAYSFOCUS, OnAlwaysfocus)
  ON_BN_CLICKED(IDC_RAMPBEAM, OnRampbeam)
  ON_BN_CLICKED(IDC_ALIGNTRACKONLY, OnAligntrackonly)
  ON_BN_CLICKED(IDC_RCAMERA1, OnRcamera)
  ON_BN_CLICKED(IDC_CHECKAUTOFOCUS, OnCheckautofocus)
	ON_BN_CLICKED(IDC_SKIPAUTOFOCUS, OnSkipautofocus)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPINCOSINEPOWER, OnDeltaposSpincosinepower)
	ON_BN_CLICKED(IDC_LIMITTOCURRENT, OnLimittocurrent)
  ON_EN_KILLFOCUS(IDC_EDITBEAMTILT, OnKillfocus)
	ON_BN_CLICKED(IDC_AUTO_REFINE_ZLP, OnAutoRefineZlp)
	ON_BN_CLICKED(IDC_STOPONBIGSHIFT, OnStoponbigshift)
  ON_EN_KILLFOCUS(IDC_EDITENDANGLE, OnAngleChanges)
  ON_EN_KILLFOCUS(IDC_EDITINCREMENT, OnAngleChanges)
  ON_BN_CLICKED(IDC_COSINEINC, OnAngleChanges)
  ON_EN_KILLFOCUS(IDC_EDITCHECKFOCUS, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITCOUNTS, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITLIMITIS, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITMAXTILTFITX, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITREPEATFOCUS, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITREPEATRECORD, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITTILTDELAY, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITFOCUSTARGET, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDIT_ITER_THRESH, OnKillfocus)
  ON_BN_CLICKED(IDC_RINTENSITYCOSINE, OnRintensitymean)
  ON_BN_CLICKED(IDC_RCONSTANTBEAM, OnRintensitymean)
  ON_EN_KILLFOCUS(IDC_EDITTAPERCOUNTS, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITTAPERANGLE, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITMINQUADYFIT, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITMINQUADXFIT, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITMAXTILTFITZ, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITMAXTILTFITY, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITALWAYSFOCUS, OnKillfocus)
  ON_BN_CLICKED(IDC_TSGO, OnOK)
  ON_EN_KILLFOCUS(IDC_EDITNEWTRACKDIFF, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITPREDICTERRORXY, OnKillfocus)
  ON_EN_KILLFOCUS(IDC_EDITPREDICTERRORZ, OnKillfocus)
  ON_BN_CLICKED(IDC_RCAMERA2, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA3, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA4, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA5, OnRcamera)
  ON_BN_CLICKED(IDC_RCAMERA6, OnRcamera)
  ON_EN_KILLFOCUS(IDC_EDITMINFOCUSRATIO, OnKillfocus)
	ON_EN_KILLFOCUS(IDC_EDITANCHOR, OnAngleChanges)
	ON_BN_CLICKED(IDC_USEINTENSITYATZERO, OnLimittocurrent)
	ON_BN_CLICKED(IDC_AVERAGERECREF, OnAveragerecref)
	//}}AFX_MSG_MAP
  ON_CONTROL_RANGE(BN_CLICKED, IDC_TSS_PLUS1, IDC_TSS_PLUS12, OnOpenClose)
  ON_BN_CLICKED(IDC_BUT_TSS_LEFT, OnButTssLeft)
  ON_BN_CLICKED(IDC_BUT_TSS_RIGHT, OnButTssRight)
  ON_BN_CLICKED(IDC_BUT_TSS_PREV, OnButTssPrev)
  ON_BN_CLICKED(IDC_BUT_TSS_NEXT, OnButTssNext)
  ON_BN_CLICKED(IDC_BUT_TSS_FULL, OnButTssFull)
  ON_BN_CLICKED(IDC_BUT_SET_CHANGES, OnButSetChanges)
  ON_BN_CLICKED(IDC_CHECK_VARY_PARAMS, OnCheckVaryParams)
  ON_BN_CLICKED(IDC_CHANGE_ALL_EXP, OnChangeAllExp)
  ON_BN_CLICKED(IDC_CHANGE_EXPOSURE, OnChangeExposure)
  ON_BN_CLICKED(IDC_AUTOCEN_AT_CROSSING, OnAutocenAtCrossing)
  ON_BN_CLICKED(IDC_AUTOCEN_AT_INTERVAL, OnAutocenAtCrossing)
  ON_BN_CLICKED(IDC_BUT_SWAP_ANGLES, OnButSwapAngles)
  ON_BN_CLICKED(IDC_DO_BIDIR, OnDoBidir)
  ON_BN_CLICKED(IDC_BIDIR_USE_VIEW, OnBidirUseView)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_BIDIR_MAG, OnDeltaposSpinBidirMag)
  ON_EN_KILLFOCUS(IDC_BIDIR_ANGLE, OnAngleChanges)
  ON_BN_CLICKED(IDC_CHECK_LIMIT_DELTA_FOCUS, OnCheckLimitDeltaFocus)
  ON_BN_CLICKED(IDC_DO_EARLY_RETURN, OnDoEarlyReturn)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_EARLY_FRAMES, OnDeltaposSpinEarlyFrames)
  ON_BN_CLICKED(IDC_USE_DOSE_SYM, OnUseDoseSym)
  ON_BN_CLICKED(IDC_BUT_SETUP_DOSE_SYM, OnButSetupDoseSym)
  ON_BN_CLICKED(IDC_TSWAITFORDRIFT, OnWaitForDrift)
  ON_BN_CLICKED(IDC_BUT_SETUP_DRIFT_WAIT, OnButSetupDriftWait)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTSSetupDialog message handlers

BOOL CTSSetupDialog::OnInitDialog() 
{
  CRect idRect, goRect;
  CWnd *wnd;
  int index, panel, j;
  CFont *littleFont = mWinApp->GetLittleFont(GetDlgItem(IDC_STATSTARTAT));
  CBaseDlg::OnInitDialog();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mNumCameras = mWinApp->GetNumActiveCameras();
  mActiveCameraList = mWinApp->GetActiveCameraList();
  mCurrentCamera = mActiveCameraList[mTSParam.cameraIndex];
  mProbeMode = mTSParam.probeMode;
  mSTEMindex = mCamParams[mCurrentCamera].STEMcamera ? 1 + mProbeMode : 0;
  CString *modeNames = mWinApp->GetModeNames();
  mWinApp->mBufferManager->CheckAsyncSaving();
  BOOL fileWritten = !mFuture && (mWinApp->mStoreMRC != NULL && 
    mWinApp->mStoreMRC->getDepth() > 0);
  BOOL enable;
  CString str, str2;
  mPostpone = false;
  mSingleStep = false;

  SetupPanelTables(idTable, leftTable, topTable, mNumInPanel, mPanelStart, heightTable, 
    5);

  // The +/- bold used to work without a member variable, but not for high DPI
  wnd = GetDlgItem(IDC_TSS_PLUS1);
  wnd->GetClientRect(idRect);
  mBoldFont.CreateFont((idRect.Height()+ mWinApp->ScaleValueForDPI(4)), 0, 0, 0, FW_BOLD,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, "Microsoft Sans Serif");
  wnd = GetDlgItem(IDC_TSS_TITLE1);
  mTitleFont = mWinApp->GetBoldFont(wnd);
   for (panel = 0; panel < mNumPanels - 1; panel++) {
    index = mPanelStart[panel];
    wnd = GetDlgItem(idTable[index]);
    wnd->SetFont(&mBoldFont);
    for (index = index + 1; index < mPanelStart[panel] + 3; index++) {
      if (idTable[index] > 0) {
        wnd = GetDlgItem(idTable[index]);
        wnd->SetFont(littleFont);
      }
    }
    wnd = GetDlgItem(idTable[index]);
    wnd->SetFont(mTitleFont);
  }
  m_butPrev.SetFont(littleFont);
  m_butNext.SetFont(littleFont);
  m_butLeft.SetFont(littleFont);
  m_butRight.SetFont(littleFont);
  m_butFull.SetFont(littleFont);

  if (mNumCameras > 4) {
    SetDlgItemText(IDC_AVERAGERECREF, "Average Record dark refs.");
    m_butAverageDark.GetClientRect(&idRect);
    m_butAverageDark.SetWindowPos(NULL, 0, 0, (idRect.Width() * 4) / 5, idRect.Height(),
      SWP_NOMOVE);
  }

  if (mFuture > 0) {
    m_butGo.GetWindowRect(&goRect);
    m_butGo.SetWindowTextA("Set Up Extra Output");
    m_butGo.SetWindowPos(NULL, 0, 0, 2 * goRect.Width(), goRect.Height(), SWP_NOMOVE);
  }

  ManagePanels();

  m_statStarTilt.SetFont(mTitleFont);
  m_statStarBidir.SetFont(mTitleFont);
  if (mNavOverrideFlags & NAV_OVERRIDE_TILT)
    m_statStarTilt.SetWindowText("*");
  if (mNavOverrideFlags & NAV_OVERRIDE_BIDIR)
    m_statStarBidir.SetWindowText("*");
  if (mNavOverrideFlags & NAV_OVERRIDE_DEF_TARG) {
    m_statTargMicrons.SetFont(mTitleFont);
    m_statTargMicrons.SetWindowTextA("um*");
  }

  // Set up cosine powers/multipliers
  for (index = 5; index > 1; index--) {
    mCosinePowers.push_back((float)index);
    mCosMultipliers.push_back((float)pow(2., 1. / index));
  }
  for (index = 0; index < 6; index++) {
    mCosMultipliers.push_back((float)(1.6 + 0.2 * index));
    mCosinePowers.push_back((float)(log(2.) / log(1.6 + 0.2 * index)));
  }

  // Find nearest power to incoming value
  mCosPowerInd = 0;
  for (index = 1; index < (int)mCosinePowers.size(); index++)
    if (fabs(mTSParam.cosinePower - mCosinePowers[index]) <
      fabs(mTSParam.cosinePower - mCosinePowers[mCosPowerInd]))
      mCosPowerInd = index;

  // Load the dialog box with parameters
  m_fStartAngle = mTSParam.startingTilt;
  m_fEndAngle = mTSParam.endingTilt;
  m_fIncrement = mTSParam.tiltIncrement;
  m_bCosineInc = mTSParam.cosineTilt;
  m_fTiltDelay = mTSParam.tiltDelay;
  m_fBidirAngle = mTSParam.bidirAngle;
  m_bDoBidir = mTSParam.doBidirectional;
  m_bBidirUseView = mTSParam.anchorBidirWithView;
  m_bBidirWalkBack = mTSParam.walkBackForBidir;
  m_bUseDoseSym = mTSParam.doDoseSymmetric;
  ConstrainBidirAngle(false, false);

  m_iCamera = mTSParam.cameraIndex;
  for (j = 0; j < 3; j++) {
    mMagIndex[j] = mTSParam.magIndex[j];
    mLowMagIndex[j] = mTSParam.lowMagIndex[j];
    mBidirMagInd[2 * j] = mTSParam.bidirAnchorMagInd[2 * j];
    mBidirMagInd[2 * j + 1] = mTSParam.bidirAnchorMagInd[2 * j + 1];
  }
  ManageAnchorMag(mCurrentCamera);
  m_sbcBidirMag.SetRange(0, 1000);
  m_sbcBidirMag.SetPos(500);
  m_strRecordMag.Format("%d", RegularOrEFTEMMag(mMagIndex[mSTEMindex]));
  m_sbcRecordMag.SetRange(1, MAX_MAGS);
  m_sbcRecordMag.SetPos(mMagIndex[mSTEMindex]);
  mBinning = mTSParam.binning;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mBinning, 
    &mCamParams[mCurrentCamera]));
  m_sbcBin.SetRange(0, 100);
  m_sbcBin.SetPos(50);
  m_bLowMagTrack = mTSParam.trackLowMag;
  m_strLowMag.Format("%d", RegularOrEFTEMMag(mLowMagIndex[mSTEMindex]));
  m_sbcTrackMag.SetRange(1, MAX_MAGS);
  m_sbcTrackMag.SetPos(mLowMagIndex[mSTEMindex]);
  for (j = 0; j < 3; j++)
    if (beamCodes[j] == mTSParam.beamControl)
      m_iBeamControl = j;
  if (mSTEMindex && m_iBeamControl == 2)
    m_iBeamControl = 0;
  m_sbcCosinePower.SetRange(0, 100);
  m_sbcCosinePower.SetPos(50);
  m_iCounts = mTSParam.meanCounts;
  m_statCurrentCounts.GetWindowText(str);
  str2.Format("%s %d", str, mMeanInBufferA);
  str2.Replace("Record", (LPCTSTR)modeNames[RECORD_CONSET]);
  m_statCurrentCounts.SetWindowText(str2);
  m_bLimitIntensity = mTSParam.limitIntensity;
  m_bLimitToCurrent = mTSParam.limitToCurrent;
  m_bIntensitySetAtZero = mTSParam.intensitySetAtZero;
  m_bRefineEucen = mTSParam.refineEucen;
  m_bLeaveAnchor = mTSParam.leaveAnchor;
  m_fAnchorAngle = mTSParam.anchorTilt;
  m_editAnchor.EnableWindow(m_bLeaveAnchor);
  m_bUseAForRef = mTSParam.refIsInA;
  m_bUseAnchor = mTSParam.useAnchor;
  mAnchorBuf = mTSParam.anchorBuffer;
  m_sbcAnchor.SetRange(0, 100);
  m_sbcAnchor.SetPos(50);
  ManageAnchorBuffer();
  m_fLimitIS = (mLowDoseMode && mTSParam.doBidirectional && mTSParam.doDoseSymmetric) ?
    mTSParam.dosymStartingISLimit : mTSParam.ISlimit;
  m_fBeamTilt = (float)mTSParam.beamTilt;
  m_fTargetDefocus = mTSParam.targetDefocus;
  m_fFocusOffset = mTSParam.autofocusOffset;
  m_fIterThresh = mTSParam.refocusThreshold;
  m_bSkipAutofocus = mTSParam.skipAutofocus;
  m_fCheckFocus = mTSParam.focusCheckInterval;
  m_bLimitDeltaFocus = mTSParam.limitDefocusChange;
  m_fFocusMaxDelta = mTSParam.defocusChangeLimit;
  m_bLimitAbsFocus = mTSParam.applyAbsFocusLimits;
  m_bRepeatRecord = mTSParam.repeatRecord;
  m_bConfirmLowDoseRepeat = mTSParam.confirmLowDoseRepeat;
  m_fRepeatRecord = 100.f * mTSParam.maxRecordShift;
  m_editRepeatRecord.EnableWindow(m_bRepeatRecord);
  m_bStopOnBigShift = mTSParam.stopOnBigAlignShift;
  m_fBigShift = mTSParam.maxAlignShift;
  m_editBigShift.EnableWindow(m_bStopOnBigShift);
  m_bRepeatFocus = mTSParam.repeatAutofocus;
  m_fRepeatFocus = mTSParam.maxFocusDiff;
  m_editRepeatFocus.EnableWindow(m_bRepeatFocus);
  m_bAlwaysFocus = mTSParam.alwaysFocusHigh;
  m_fAlwaysAngle = mTSParam.alwaysFocusAngle;
  m_bCheckAutofocus = mTSParam.checkAutofocus;
  m_fMinFocusAccuracy = mTSParam.minFocusAccuracy;
  m_editAlwaysAngle.EnableWindow(m_bAlwaysFocus);
  m_bRampBeam = mTSParam.taperCounts;
  m_iTaperCounts = mTSParam.highCounts;
  m_fTaperAngle = mTSParam.taperAngle;
  m_iTrackAfterFocus = mTSParam.trackAfterFocus;
  m_bAlignTrackOnly = mTSParam.alignTrackOnly;
  m_bPreviewBeforeRef = mTSParam.previewBeforeNewRef;
  m_fNewTrackDiff = 100.f * mTSParam.maxRefDisagreement;
  m_fPredictErrorXY = 100.f * mTSParam.maxPredictErrorXY;
  m_fPredictErrorZ = mTSParam.maxPredictErrorZ[mSTEMindex];
  m_bRefineZLP = mTSParam.refineZLP;
  m_iZlpInterval = mTSParam.zlpInterval;
  m_bCenterBeam = mTSParam.centerBeamFromTrial;
  m_bCloseValves = mTSParam.closeValvesAtEnd;
  m_bManualTrack = mTSParam.manualTracking;
  m_bAverageDark = mCamParams[mCurrentCamera].TSAverageDark > 0;
  m_bVaryParams = mTSParam.doVariations;
  m_butSetChanges.EnableWindow(m_bVaryParams);
  mChangeRecExposure = mTSParam.changeRecExposure;
  m_bChangeExposure = mChangeRecExposure || mSTEMindex;
  m_bChangeAllExp = mTSParam.changeAllExposures;
  m_bChangeSettling = mTSParam.changeSettling;
  m_bAutocenCrossing = mTSParam.cenBeamAbove;
  m_bAutocenInterval = mTSParam.cenBeamPeriodically;
  m_iAutocenInterval = mTSParam.cenBeamInterval;
  m_fAutocenAngle = mTSParam.cenBeamAngle;
  m_bDoEarlyReturn = mTSParam.doEarlyReturn;
  m_bWaitForDrift = mTSParam.waitForDrift;
  m_sbcEarlyFrames.SetRange(0, 1000);
  m_sbcEarlyFrames.SetPos(500);
  mTSParam.earlyReturnNumFrames = B3DMAX(1, mTSParam.earlyReturnNumFrames);
  m_strNumEarlyFrames.Format("%d", mTSParam.earlyReturnNumFrames);
  if (!FEIscope)
    m_statMilliRad = "% of range";
  if (JEOLscope) {
    if (mWinApp->mScope->GetNoColumnValve())
      ReplaceWindowText(&m_butCloseValves, "Close column valves", "Turn off filament");
    else
      ReplaceWindowText(&m_butCloseValves, "column valves", "gun valve");
  }

  ManageNewTrackDiff();
  ManageTaperLine();
  SetPixelSize();
  SetTotalTilts();
  ManageFocusIntervals();
  ManageInitialActions();
  ManageRefineZLP();
  ManageExposures();
  ManageAutocen();
  ManageCamDependents();
  ManageIntersetStatus();
  ManageEarlyReturn();
  ManageDriftWait();
  ManageCosinePower();

  // Set up the camera buttons
  int i = 0;
  if (mNumCameras > 1) {
    for (; i < mNumCameras; i++) {
      SetDlgItemText(i + IDC_RCAMERA1, mCamParams[mActiveCameraList[i]].name);
      CButton *radio = (CButton *)GetDlgItem(i + IDC_RCAMERA1);
      radio->SetFont(littleFont);
    }
  } 

  // Turn off the rest of the camera buttons
  for (; i < MAX_DLG_CAMERAS; i++) {
    CButton *radio = (CButton *)GetDlgItem(i + IDC_RCAMERA1);
    if (radio)
      radio->ShowWindow(SW_HIDE);
  }
  ManageCameras();

  // Disable Record mag if montaging or doing TS or low dose
  enable = !mTSParam.magLocked && !mDoingTS && !mLowDoseMode;
  m_statMagLabel.EnableWindow(enable);
  m_statRecordMag.EnableWindow(enable);
  m_sbcRecordMag.EnableWindow(enable);

  // Disable binning if montaging or file written or doing TS
  enable = !mMontaging && !fileWritten && !mDoingTS;
  m_statBinning.EnableWindow(enable);
  m_statBinLabel.EnableWindow(enable);
  m_sbcBin.EnableWindow(enable);
  m_butLowMagTrack.EnableWindow(mLowMagIndex[mSTEMindex] < mMagIndex[mSTEMindex] &&
    !mLowDoseMode);

  // Disable low mag track but not intensity stuff if low dose mode
  if (mLowDoseMode) {
    m_statLowMag.EnableWindow(false);
    m_sbcTrackMag.EnableWindow(false);
    //m_statBeamBox.EnableWindow(false);
    //m_editCounts.EnableWindow(false);
    //m_butConstantBeam.EnableWindow(false);
    //m_butIntensityMean.EnableWindow(false);
    //m_butIntensityCosine.EnableWindow(false);
  } else {
    m_butAlignTrackOnly.EnableWindow(false);
    m_butPreviewBeforeRef.EnableWindow(false);
    m_butConfirmLowDoseRepeat.EnableWindow(false);
  }

  m_butRefineEucen.EnableWindow(!mDoingTS && mCanFindEucentricity && 
    !mWinApp->mComplexTasks->GetHitachiWithoutZ());

  // Now disable lots of things if doing series
  if (mDoingTS) {
    m_statStartAt.EnableWindow(mDisableStartOrEnd >= 0);
    m_editStartAngle.EnableWindow(mDisableStartOrEnd >= 0);
    m_statEndAt.EnableWindow(mDisableStartOrEnd <= 0);
    m_editEndAngle.EnableWindow(mDisableStartOrEnd <= 0);
    m_statbasicInc.EnableWindow(false);
    m_editIncrement.EnableWindow(false);
    m_butCosineInc.EnableWindow(false);
    m_statTimesActual.EnableWindow(false);
    m_butUseAnchor.EnableWindow(false);
    m_statAnchorBuf.EnableWindow(false);
    m_sbcAnchor.EnableWindow(false);
    m_butVaryParams.EnableWindow(false);
    m_butLimitToCurrent.SetWindowText(
      "Keep intensity below value before series was started");
  }
  m_butSwapAngles.EnableWindow(!mDoingTS && mFuture >= 0 && 
    (mFuture > 0 || fabs(mCurrentAngle) < 20.));
  ManageBidirectional();
  ManageDriftWait();

  // Fix strings
  ReplaceDlgItemText(IDC_RINTENSITYMEAN, "Record", modeNames[RECORD_CONSET]);
  ReplaceDlgItemText(IDC_AVERAGERECREF, "Record", modeNames[RECORD_CONSET]);
  ReplaceDlgItemText(IDC_REPEATRECORD, "Record", modeNames[RECORD_CONSET]);
  ReplaceDlgItemText(IDC_CONFIRM_LOWDOSE_REPEAT, "Record", modeNames[RECORD_CONSET]);
  ReplaceWindowText(&m_butAlignTrackOnly, "Record", modeNames[RECORD_CONSET]);
  ReplaceWindowText(&m_statNewTrack, "Record", modeNames[RECORD_CONSET]);
  ReplaceWindowText(&m_butChangeExposure, "Record", modeNames[RECORD_CONSET]);
  ReplaceWindowText(&m_butChangeAllExp, "Record", modeNames[RECORD_CONSET]);
  ReplaceWindowText(&m_butPreviewBeforeRef, "Preview", modeNames[PREVIEW_CONSET]);

  // Fix buttons for future mode
  if (mFuture) {
    m_butUseAforRef.EnableWindow(false);
    m_butUseAnchor.EnableWindow(false);
    m_butManualTrack.EnableWindow(false);
    m_butCloseValves.EnableWindow(false);
    if (mFuture > 0) {
      m_butPostpone.SetWindowText("OK");
      m_butPostpone.SetButtonStyle(BS_DEFPUSHBUTTON);
      m_butPostpone.SetFocus();
    }

    // But we shouldn't turn off user's favorite settings, disable them in controller
    m_bManualTrack = false;
  }
  UpdateData(false);

  return mFuture <= 0;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Enable or disable camera buttons
void CTSSetupDialog::ManageCameras()
{
  int iCamInd, iCam, binToEval;
  if (mNumCameras < 2)
    return;

  // loop through the cameras, making sure the current binning exists
  for (iCamInd = 0; iCamInd < mNumCameras; iCamInd++) {
    iCam = mActiveCameraList[iCamInd];
    binToEval = BinDivisorI(&mCamParams[iCam]) * mBinning / 
      BinDivisorI(&mCamParams[mCurrentCamera]);
    BOOL binOK = mWinApp->BinningIsValid(binToEval, iCam, true);
    CButton *radio = (CButton *)GetDlgItem(iCamInd + IDC_RCAMERA1);

    // Disable if montaging or already started
    radio->EnableWindow(!mMontaging && !mDoingTS && binOK);
  }
}

// The camera selection is switched
void CTSSetupDialog::OnRcamera() 
{
  int lastSTEM = mSTEMindex;
  int lastCam = mCurrentCamera;
  UpdateData(true);
  int index = 2 * mSTEMindex + (mLowDoseMode ? 1 : 0);
  mCurrentCamera = mActiveCameraList[m_iCamera];
  CameraParameters *camP = &mCamParams[mCurrentCamera];
  SetDlgItemText(IDC_STATBINLABEL, camP->STEMcamera ? "Sampling:" : "Binning:");
  mBinning = BinDivisorI(camP) * mBinning / BinDivisorI(&mCamParams[lastCam]);
  B3DCLAMP(mBinning, camP->binnings[0], camP->binnings[camP->numBinnings - 1]);
  mProbeMode = mWinApp->mScope->GetProbeMode();    // Let them change probe mode on fly
  mSTEMindex = mCamParams[mCurrentCamera].STEMcamera ? 1 + mProbeMode : 0;
  if (!mMagIndex[mSTEMindex])
    mMagIndex[mSTEMindex] = MagIndWithClosestFieldSize(lastCam, mMagIndex[lastSTEM],
    mCurrentCamera, mSTEMindex);
  if (!mBidirMagInd[index])
    mBidirMagInd[index] = MagIndWithClosestFieldSize(lastCam, 
      mBidirMagInd[2 * lastSTEM + (mLowDoseMode ? 1 : 0)], mCurrentCamera, mSTEMindex);
  if (!mLowMagIndex[mSTEMindex])
    mLowMagIndex[mSTEMindex] = MagIndWithClosestFieldSize(lastCam, 
    mLowMagIndex[lastSTEM], mCurrentCamera, mSTEMindex);
  m_strRecordMag.Format("%d", RegularOrEFTEMMag(mMagIndex[mSTEMindex]));
  m_strLowMag.Format("%d", RegularOrEFTEMMag(mLowMagIndex[mSTEMindex]));
  ManageAnchorMag(mCurrentCamera);
  m_butLowMagTrack.EnableWindow(mLowMagIndex[mSTEMindex] < mMagIndex[mSTEMindex] && 
    !mLowDoseMode);
  m_bAverageDark = mCamParams[mCurrentCamera].TSAverageDark > 0;
  if (mSTEMindex && m_iBeamControl == 2) {
    m_iBeamControl = 0;
    UpdateData(false);   // Do this before someone else disables the button
  }
  if (mSTEMindex && !lastSTEM) {
    mChangeRecExposure = m_bChangeExposure;
    m_bChangeExposure = true;
  } else if (!mSTEMindex && lastSTEM)
    m_bChangeExposure = mChangeRecExposure;
  mTSParam.maxPredictErrorZ[lastSTEM] = m_fPredictErrorZ;
  m_fPredictErrorZ = mTSParam.maxPredictErrorZ[mSTEMindex];
  ManageExposures();
  ManageTaperLine();
  ManageCamDependents();
  ManageIntersetStatus();
  SetPixelSize();
  ManageRefineZLP();
  ManageAutocen();
  ManageNewTrackDiff();
  SetTotalTilts();
  ManageEarlyReturn();
}

// Take care of any enables that depend on camera not handled elsewhere
void CTSSetupDialog::ManageCamDependents(void)
{
  // Disable average dark button if not processing here
  m_butAverageDark.EnableWindow((mWinApp->mCamera->GetProcessHere() ||
    mCamParams[mCurrentCamera].TietzType) && !mSTEMindex);
  //m_butSkipAutofocus.EnableWindow(mLowDoseMode || mSTEMindex);
  m_butCenterBeam.EnableWindow(mLowDoseMode && !mSTEMindex);
  m_editFocusOffset.EnableWindow(!mSTEMindex);
  m_editBeamTilt.EnableWindow(!mSTEMindex);
  m_butCheckAutofocus.EnableWindow(!mDoingTS && !mSTEMindex);
  m_editMinFocusAccuracy.EnableWindow(!mDoingTS && m_bCheckAutofocus && !mSTEMindex);
  m_butIntensityMean.EnableWindow(!mSTEMindex);
  m_statInterset.ShowWindow((mSTEMindex && mPanelOpen[1]) ? SW_SHOW : SW_HIDE);
  m_butCenterBeam.ShowWindow((!mSTEMindex && mPanelOpen[1]) ? SW_SHOW : SW_HIDE);
}

// Early Return handlers
void CTSSetupDialog::OnDoEarlyReturn()
{
  UpdateData(true);
  ManageEarlyReturn();
}

void CTSSetupDialog::OnDeltaposSpinEarlyFrames(NMHDR *pNMHDR, LRESULT *pResult)
{
  if (NewSpinnerValue(pNMHDR, pResult, mTSParam.earlyReturnNumFrames, 1, 99, 
    mTSParam.earlyReturnNumFrames))
    return;
  m_strNumEarlyFrames.Format("%d", mTSParam.earlyReturnNumFrames);
  UpdateData(false);
}

void CTSSetupDialog::ManageEarlyReturn(void)
{
  bool enable = mCamParams[mCurrentCamera].K2Type != 0;
  m_butDoEarlyReturn.EnableWindow(enable);
  m_sbcEarlyFrames.EnableWindow(enable && m_bDoEarlyReturn);
  m_statNumEarlyFrames.EnableWindow(enable && m_bDoEarlyReturn);
  m_statFrameLabel.EnableWindow(enable && m_bDoEarlyReturn);
}

void CTSSetupDialog::ManageRefineZLP()
{
  BOOL bEnable = (mCamParams[mCurrentCamera].GIF || mWinApp->mScope->GetHasOmegaFilter())
    && !mSTEMindex;
  m_butRefineZLP.EnableWindow(bEnable);
  m_statMinutes.EnableWindow(bEnable);
  m_editZlpInterval.EnableWindow(bEnable && m_bRefineZLP);
}

void CTSSetupDialog::ManageAnchorBuffer()
{
  char letter = 'A' + mAnchorBuf;
  CString string = letter;
  m_statAnchorBuf.SetWindowText(string);
  m_statAnchorBuf.EnableWindow(m_bUseAnchor && !mDoingTS && !mFuture);
  m_sbcAnchor.EnableWindow(m_bUseAnchor && !mDoingTS && !mFuture);
}

void CTSSetupDialog::ManageIntersetStatus(void)
{
  bool trialOK, focusOK, previewOK = false;
  float shiftX, shiftY;
  CString *modeNames = mWinApp->GetModeNames();
 if (!mSTEMindex)
    return;
  trialOK = mWinApp->mShiftManager->ShiftAdjustmentForSet(TRIAL_CONSET, 
    mMagIndex[mSTEMindex], shiftX, shiftY, mCurrentCamera, mBinning); 
  focusOK = mWinApp->mShiftManager->ShiftAdjustmentForSet(FOCUS_CONSET, 
    mMagIndex[mSTEMindex], shiftX, shiftY, mCurrentCamera, mBinning);
  if (mWinApp->LowDoseMode())
    previewOK = mWinApp->mShiftManager->ShiftAdjustmentForSet(PREVIEW_CONSET, 
      mMagIndex[mSTEMindex], shiftX, shiftY, mCurrentCamera, mBinning);
  if (trialOK || focusOK || previewOK) {
    m_strInterset = "Interset shifts WILL BE USED for ";
    if (trialOK)
      m_strInterset += modeNames[TRIAL_CONSET];
    if (focusOK) {
      if (trialOK)
        m_strInterset += previewOK ? ", " : " and ";
      m_strInterset += modeNames[FOCUS_CONSET];
    }
    if (previewOK) {
      if (trialOK || focusOK)
        m_strInterset += trialOK && focusOK ? ", and " : " and ";
      m_strInterset += modeNames[PREVIEW_CONSET];
    }
  } else
    m_strInterset = "Interset shifts are NOT AVAILABLE AND WILL NOT BE USED";
}

void CTSSetupDialog::SetPixelSize()
{
  float pixel = 1000.f * mBinning * mWinApp->mShiftManager->GetPixelSize
    (mCurrentCamera, mMagIndex[mSTEMindex]);
  m_strPixel.Format("Pixel size: " + mWinApp->PixelFormat(pixel), pixel);
  UpdateData(false);
}

void CTSSetupDialog::SetTotalTilts()
{
  int total, iDir, spotSize;
  double angle = m_fStartAngle;
  float signedInc = m_fIncrement;
  float cosPower = mCosinePowers[mCosPowerInd];
  double totalDose = 0.;
  double topDose, curDose, intensity, zeroDose;

  iDir = 1;
  if (m_fEndAngle < m_fStartAngle) {
    signedInc = -signedInc;
    iDir = -1;
  }

  // 2/13/04: switch to adding up by increments for regular as well as cosine
  // so that dose could be computed too.
  total = 0;
  while (iDir * (angle - m_fEndAngle) <= 0.1 * m_fIncrement) {
    if (m_bCosineInc && !(m_bUseDoseSym && m_bDoBidir && mLowDoseMode))
      angle += (float)(signedInc * cos(DTOR * angle));
    else
      angle += signedInc;
    if (m_iBeamControl == 0)
      totalDose += 1.;
    else if (m_iBeamControl == 1)
      totalDose += (float)pow(1. / cos(DTOR * angle), 1. / cosPower);
    total++;
  }

  topDose = totalDose;
  if (m_iBeamControl == 1)
    topDose = totalDose / pow(1. / cos(DTOR * m_fStartAngle), 1. / cosPower);
  m_strTotalTilt.Format("Total # of tilts: %d", total);

  if (mSTEMindex) {
    m_strTotalDose = "";
  } else if (totalDose) {
    curDose = mWinApp->mBeamAssessor->GetCurrentElectronDose(mCurrentCamera, 
      RECORD_CONSET, spotSize, intensity);
    if (curDose) {

      // Determine angle at which this dose applies
      // It is dose at start if limit to current is set and not started TS
      // It is dose at zero if intensity set at zero set and not started TS
      // Otherwise it is the current angle
      angle = mCurrentAngle;
      if (m_bLimitToCurrent && !mDoingTS)
        angle = m_fStartAngle;
      else if (m_bIntensitySetAtZero && !mDoingTS)
        angle = 0.;
      zeroDose = topDose = curDose;
      if (m_iBeamControl == 1) {
        zeroDose = curDose * pow(cos(DTOR * angle), 1. / cosPower);
        topDose = zeroDose * pow(1. / cos(DTOR * m_fStartAngle), 1. / cosPower);
      }
      totalDose *= zeroDose;
      m_strTotalDose.Format("Dose: %.2f e/A2 at 0 deg;  %.2f e/A2 at %.0f deg;  "
        "Total: %.1f e/A2", zeroDose, topDose, m_fStartAngle, totalDose);
    } else
      m_strTotalDose.Format("Total dose: %.1f x dose at 0 deg; %.1f x dose at %.0f deg",
        totalDose, topDose, m_fStartAngle);
  } else
    m_strTotalDose = "Total dose: no estimate with setting for constant counts";


  UpdateData(false);
}

// This is called on KillFocus of the start, end and increment
// boxes and anchor angle box and on a change of the cosine tilt button
void CTSSetupDialog::OnAngleChanges() 
{
  float oldStart = m_fStartAngle;
  float oldEnd = m_fEndAngle;
  float oldBidir = m_fBidirAngle;
  if (!UpdateData(true))
    return;
  ConstrainBidirAngle(true, oldStart != m_fStartAngle || oldEnd != m_fEndAngle
    || oldBidir != m_fBidirAngle);
  if (mBidirStage != BIDIR_NONE && (m_fBidirAngle < B3DMIN(m_fStartAngle, m_fEndAngle) ||
    m_fBidirAngle > B3DMAX(m_fStartAngle, m_fEndAngle))) {
      m_fStartAngle = oldStart;
      m_fEndAngle = oldEnd;
      UpdateData(false);
  }
  SetTotalTilts();
  ManageInitialActions();
}

// Button to swap the starting and ending angles
void CTSSetupDialog::OnButSwapAngles()
{
  float temp;
  FixButtonStyle(IDC_BUT_SWAP_ANGLES);
  UpdateData(true);
  temp = m_fStartAngle;
  m_fStartAngle = m_fEndAngle;
  m_fEndAngle = temp;
  m_fBidirAngle = -m_fBidirAngle;
  SetTotalTilts();
  ManageInitialActions();
}

// For other edit fields, validate data when they lose focus
void CTSSetupDialog::OnKillfocus() 
{
  UpdateData(true); 
}

void CTSSetupDialog::OnButSetChanges()
{
  CTSVariationsDlg dlg;
  FixButtonStyle(IDC_BUT_SET_CHANGES);
  dlg.mParam = &mTSParam;
  mTSParam.cameraIndex = m_iCamera;
  dlg.mFilterExists = !mSTEMindex && (mCamParams[mCurrentCamera].GIF || 
    mWinApp->mScope->GetHasOmegaFilter());
  dlg.mK2Selected = mCamParams[mCurrentCamera].K2Type;
  dlg.mDoingTS = mDoingTS;
  dlg.m_fSeriesStep = mWinApp->mTSController->GetExpSeriesStep();
  dlg.m_bNumFramesFixed = mWinApp->mTSController->GetExpSeriesFixNumFrames();
  dlg.mTopAngle = B3DMAX(fabs(m_fStartAngle), fabs(m_fEndAngle));
  dlg.mSTEMmode = mSTEMindex != 0;
  dlg.mSeriesPowerInd = mCosPowerInd;
  dlg.mCosinePowers = &mCosinePowers;
  dlg.DoModal();
  mWinApp->mTSController->SetExpSeriesStep(dlg.m_fSeriesStep);
  mWinApp->mTSController->SetExpSeriesFixNumFrames(dlg.m_bNumFramesFixed);
  ManageExposures();
  ManageTaperLine();
}

void CTSSetupDialog::OnCheckVaryParams()
{
  UpdateData(true);
  m_butSetChanges.EnableWindow(m_bVaryParams);
  ManageExposures();
  ManageTaperLine();
}

void CTSSetupDialog::OnDeltaposSpinanchor(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  int newVal = mAnchorBuf + pNMUpDown->iDelta;
  if (newVal < 0 || newVal >= MAX_BUFFERS) {
    *pResult = 1;
    return;
  }
  mAnchorBuf = newVal;
  ManageAnchorBuffer();
  *pResult = 0;
}

void CTSSetupDialog::OnDeltaposSpinbin(NMHDR* pNMHDR, LRESULT* pResult) 
{
  int newVal;
  CameraParameters *cam = &mCamParams[mCurrentCamera];
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;

  // Look in given direction for the next valid binning, quit if none
  newVal = mWinApp->NextValidBinning(mBinning, pNMUpDown->iDelta, mCurrentCamera, true);
  if (newVal == mBinning)
    return;
  mBinning = newVal;
  m_strBinning.Format("%s", (LPCTSTR)mWinApp->BinningText(mBinning, cam));
  ManageIntersetStatus();
  SetPixelSize();
  ManageCameras();
  *pResult = 0;
}

void CTSSetupDialog::OnDeltaposSpinrecordmag(NMHDR* pNMHDR, LRESULT* pResult) 
{
  int newVal;
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;

  // Move in given direction until an index is reached with a listed
  // mag or until the end of the table
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, mMagIndex[mSTEMindex], 
    pNMUpDown->iDelta);
  if (newVal <= 0)
    return;
  mMagIndex[mSTEMindex] = newVal;
  m_strRecordMag.Format("%d", RegularOrEFTEMMag(newVal));
  ManageIntersetStatus();
  SetPixelSize();
  m_butLowMagTrack.EnableWindow(mLowMagIndex[mSTEMindex] < mMagIndex[mSTEMindex] && 
    !mLowDoseMode);
  ManageNewTrackDiff();
  ManageAutocen();
  ManageInitialActions();
  *pResult = 0;
}

void CTSSetupDialog::OnDeltaposSpintrackmag(NMHDR* pNMHDR, LRESULT* pResult) 
{
  int newVal;
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  *pResult = 1;

  // Get the next mag in the given direction but stay out of low mag
  newVal = mWinApp->FindNextMagForCamera(mCurrentCamera, mLowMagIndex[mSTEMindex],
    pNMUpDown->iDelta, 
    true);
  if (newVal <= 0)
    return;
  *pResult = 0;
  mLowMagIndex[mSTEMindex] = newVal;
  m_strLowMag.Format("%d", RegularOrEFTEMMag(newVal));
  m_butLowMagTrack.EnableWindow(mLowMagIndex[mSTEMindex] < mMagIndex[mSTEMindex] &&
    !mLowDoseMode);
  ManageNewTrackDiff();
  UpdateData(false);
}

// Change in cosine power button
void CTSSetupDialog::OnDeltaposSpincosinepower(NMHDR* pNMHDR, LRESULT* pResult) 
{
  NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
  UpdateData(true);
  int newVal = mCosPowerInd + pNMUpDown->iDelta;
  if (newVal < 0 || newVal >= (int)mCosinePowers.size()) {
    *pResult = 1;
    return;
  }
	mCosPowerInd = newVal;
  ManageCosinePower();
	*pResult = 0;
  SetTotalTilts();
}

// Low mag button: Enable the tracking difference buttons
void CTSSetupDialog::OnLowmagtrack() 
{
  UpdateData(true);
  ManageNewTrackDiff();
}

void CTSSetupDialog::OnAutoRefineZlp() 
{
  UpdateData(true);
  ManageRefineZLP();
}

void CTSSetupDialog::OnAutocenAtCrossing()
{
  UpdateData(true);
  ManageAutocen();
}

void CTSSetupDialog::ManageAutocen(void)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int probe = mLowDoseMode ? ldp[TRIAL_CONSET].probeMode : 
    mWinApp->mScope->GetProbeMode();
  bool enable = mWinApp->mMultiTSTasks->AutocenParamExists(mActiveCameraList[m_iCamera],
    mMagIndex[mSTEMindex], probe) && !mSTEMindex;
  bool doingDosym = m_bDoBidir && m_bUseDoseSym && mLowDoseMode;
  m_butAutocenInterval.EnableWindow(enable);
  m_butAutocenCrossing.EnableWindow(enable && !doingDosym);
  m_editAutocenInterval.EnableWindow(enable && m_bAutocenInterval);
  m_statACminutes.EnableWindow(enable && m_bAutocenInterval);
  m_editAutocenAngle.EnableWindow(enable && m_bAutocenCrossing && !doingDosym);
  m_statACdegrees.EnableWindow(enable && m_bAutocenCrossing && !doingDosym);
}

void CTSSetupDialog::OnCheckautofocus() 
{
  UpdateData(true);
  m_editMinFocusAccuracy.EnableWindow(m_bCheckAutofocus);   
}

void CTSSetupDialog::OnLeaveanchor() 
{
  UpdateData(true);
  ManageInitialActions();
}

void CTSSetupDialog::OnUseanchor() 
{
  UpdateData(true);
  ManageInitialActions();
}

void CTSSetupDialog::OnRepeatrecord() 
{
  UpdateData(true);
  m_editRepeatRecord.EnableWindow(m_bRepeatRecord); 
}

void CTSSetupDialog::OnRepeatfocus() 
{
  UpdateData(true);
  m_editRepeatFocus.EnableWindow(m_bRepeatFocus);
}

void CTSSetupDialog::OnRintensitymean() 
{
  UpdateData(true);
  ManageExposures();
  ManageTaperLine();
  SetTotalTilts();
}

void CTSSetupDialog::OnLimittocurrent() 
{
  UpdateData(true);
  ManageTaperLine();
  SetTotalTilts();
}

void CTSSetupDialog::OnSkipautofocus() 
{
  UpdateData(true);
  ManageFocusIntervals();   	
}

void CTSSetupDialog::OnAlwaysfocus() 
{
  UpdateData(true);
  m_editAlwaysAngle.EnableWindow(m_bAlwaysFocus);
}

void CTSSetupDialog::OnAveragerecref() 
{
  UpdateData(true);
  mCamParams[mCurrentCamera].TSAverageDark = m_bAverageDark ? 1 : 0;
}

void CTSSetupDialog::ManageFocusIntervals()
{
  BOOL bEnable = !m_bSkipAutofocus;
  m_statAlwaysFocus.EnableWindow(bEnable);
  m_editCheckFocus.EnableWindow(bEnable);
  m_statCheckDeg.EnableWindow(bEnable);
  m_statCheckEvery.EnableWindow(bEnable);
  m_butAlwaysFocus.EnableWindow(bEnable);
  m_editAlwaysAngle.EnableWindow(m_bAlwaysFocus && bEnable);
  m_butLimitDeltaFocus.EnableWindow(bEnable);
  m_statDeltaMicrons.EnableWindow(bEnable);
  m_editFocusMaxDelta.EnableWindow(m_bLimitDeltaFocus && bEnable);
  m_butLimitAbsFocus.EnableWindow(bEnable &&(mWinApp->mFocusManager->GetEucenMinAbsFocus()
    != 0. || mWinApp->mFocusManager->GetEucenMaxAbsFocus() != 0.));
}

void CTSSetupDialog::OnCheckLimitDeltaFocus()
{
  UpdateData(true);
  ManageFocusIntervals();
}

void CTSSetupDialog::OnRampbeam() 
{
  UpdateData(true);
  ManageTaperLine();
}

void CTSSetupDialog::OnChangeAllExp()
{
  UpdateData(true);
  ManageExposures();
  ManageTaperLine();
}

void CTSSetupDialog::OnChangeExposure()
{
  UpdateData(true);
  ManageExposures();
  ManageTaperLine();
}

void CTSSetupDialog::OnStoponbigshift() 
{
  UpdateData(true);
 	m_editBigShift.EnableWindow(m_bStopOnBigShift);
}

void CTSSetupDialog::ManageTaperLine()
{
  bool expChange;
  int typeVaries[MAX_VARY_TYPES];
  BOOL bEnable = m_iBeamControl == 2; // && !mLowDoseMode;
  CTSVariationsDlg::ListVaryingTypes(mTSParam.varyArray, mTSParam.numVaryItems, 
    typeVaries, NULL);
  expChange = m_bVaryParams && typeVaries[TS_VARY_EXPOSURE];
  m_editCounts.EnableWindow(bEnable);
  m_butRampBeam.EnableWindow(bEnable);
  m_statTaperDeg.EnableWindow(bEnable);
  m_statTaperFta.EnableWindow(bEnable);
  m_editTaperCounts.EnableWindow(bEnable && m_bRampBeam);
  m_editTaperAngle.EnableWindow(bEnable && m_bRampBeam);
  m_butLimitToCurrent.EnableWindow(m_iBeamControl != 0 && !expChange && !m_bDoBidir &&
    (m_iBeamControl != 1 || !m_bIntensitySetAtZero));
  m_butLimitIntensity.EnableWindow(m_iBeamControl != 0 && !m_bLimitToCurrent && 
    !expChange && !m_bDoBidir);
  m_sbcCosinePower.EnableWindow(m_iBeamControl == 1);
  m_butIntensitySetAtZero.EnableWindow(m_iBeamControl == 1 && fabs(mCurrentAngle) > 10. 
    && (!m_bLimitToCurrent || expChange) && !mFuture && !(expChange && mSTEMindex));
}

// Manage text and enables that depend on whether exposure is varying continuously
// in place of intensity or being changed periodically
void CTSSetupDialog::ManageExposures(void)
{
  CString from, to;
  bool expChange;
  int typeVaries[MAX_VARY_TYPES];

  CTSVariationsDlg::ListVaryingTypes(mTSParam.varyArray, mTSParam.numVaryItems, 
    typeVaries, NULL);
  expChange = (m_bChangeExposure && m_iBeamControl > 0) || 
    (m_bVaryParams && typeVaries[TS_VARY_EXPOSURE]);
  if (m_bChangeExposure && (mSTEMindex || 
    !(m_bVaryParams && typeVaries[TS_VARY_EXPOSURE]))) {
      from = "intensity";
      to = "exposure";
      ReplaceDlgItemText(IDC_RCONSTANTBEAM, "beam intensity", "exposure time");
  } else {
    to = "intensity";
    from = "exposure";
    ReplaceDlgItemText(IDC_RCONSTANTBEAM, "exposure time", "beam intensity");
  }
  ReplaceDlgItemText(IDC_RINTENSITYCOSINE, from, to);
  ReplaceDlgItemText(IDC_RINTENSITYMEAN, from, to);
  ReplaceDlgItemText(IDC_LIMITTOCURRENT, from, to);
  ReplaceDlgItemText(IDC_LIMITINTENSITY, from, to);
  ReplaceDlgItemText(IDC_USEINTENSITYATZERO, from, to);
  m_butChangeExposure.EnableWindow(!(m_bVaryParams && typeVaries[TS_VARY_EXPOSURE]) && 
    !mDoingTS && m_iBeamControl > 0 && !mSTEMindex);
  m_butChangeAllExp.EnableWindow(expChange && !mDoingTS); 
  m_butChangeSettling.EnableWindow(expChange && !mDoingTS && !mSTEMindex &&
    (m_bChangeAllExp || !(m_bVaryParams && typeVaries[TS_VARY_DRIFT])));
  m_editFocusTarget.EnableWindow(!(m_bVaryParams && typeVaries[TS_VARY_FOCUS] && 
    mDoingTS) && !mSTEMindex);
  m_butConstantBeam.EnableWindow(!(mSTEMindex && m_bVaryParams && 
    typeVaries[TS_VARY_EXPOSURE]));
  m_butIntensityCosine.EnableWindow(!(mSTEMindex && m_bVaryParams && 
    typeVaries[TS_VARY_EXPOSURE]));
}

void CTSSetupDialog::OnAligntrackonly() 
{
  UpdateData(true);
  ManageNewTrackDiff();
}

void CTSSetupDialog::ManageNewTrackDiff()
{
  BOOL bEnable = (m_bLowMagTrack && !mLowDoseMode && mLowMagIndex[mSTEMindex] <
    mMagIndex[mSTEMindex]) ||
    (mLowDoseMode && !m_bAlignTrackOnly);
  m_editNewTrackDiff.EnableWindow(bEnable);
  m_statPercent.EnableWindow(bEnable);
  m_statNewTrack.EnableWindow(bEnable);
}

// Manage initial actions controls that depend on start angle etc.
void CTSSetupDialog::ManageInitialActions()
{
  BOOL useAnchor = m_bUseAnchor && !mFuture;
  BOOL bEnable = !mDoingTS && !useAnchor && !m_bDoBidir &&
    mWinApp->mTSController->WalkupCanTakeAnchor(mCurrentAngle, m_fAnchorAngle, 
    m_fStartAngle);
  m_butUseAforRef.EnableWindow(!mDoingTS && !mFuture && 
    (mWinApp->mTSController->UsableRefInA(
    (double)(m_bDoBidir ? m_fBidirAngle : m_fStartAngle), mMagIndex[mSTEMindex]) ||
    mWinApp->mTSController->UsableRefInA(mCurrentAngle, mMagIndex[mSTEMindex])));
  m_butLeaveAnchor.EnableWindow(bEnable);
  m_editAnchor.EnableWindow(!mDoingTS && !useAnchor && !m_bDoBidir);
  m_statAnchorDeg.EnableWindow(bEnable);
  m_statAnchorBuf.EnableWindow(!mDoingTS && useAnchor && !m_bDoBidir);   
  m_sbcAnchor.EnableWindow(!mDoingTS && useAnchor && !m_bDoBidir);
  m_butUseAnchor.EnableWindow(!m_bDoBidir);
}

// Bidirectional series items
void CTSSetupDialog::OnDoBidir()
{
  UpdateData(true);
  ManageBidirectional();
  ManageInitialActions();
  ManageDriftWait();
  ConstrainBidirAngle(true, false);
}

void CTSSetupDialog::OnBidirUseView()
{
  UpdateData(true);
  ManageBidirectional();
  ManageAnchorMag(mCurrentCamera);
  UpdateData(false);
}

void CTSSetupDialog::OnDeltaposSpinBidirMag(NMHDR *pNMHDR, LRESULT *pResult)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int lower, hiLim, index = 2 * mSTEMindex + (mLowDoseMode ? 1 : 0);
  int lowLim = mWinApp->mScope->GetLowestNonLMmag(mSTEMindex);
  mWinApp->GetMagRangeLimits(mCurrentCamera, lowLim, lower, hiLim);
  if (mLowDoseMode)
    hiLim = ldp->magIndex;
  if (NewSpinnerValue(pNMHDR, pResult, mBidirMagInd[index], lowLim, hiLim, 
    mBidirMagInd[index]))
    return;
  UpdateData(true);
  ManageAnchorMag(mCurrentCamera);
  UpdateData(false);
}

// Manage enables for bidirectional series
void CTSSetupDialog::ManageBidirectional(void)
{
  bool enable = !mDoingTS && m_bDoBidir;
  bool doingDosym = m_bDoBidir && m_bUseDoseSym && mLowDoseMode;
  m_butCosineInc.EnableWindow(!mDoingTS && !doingDosym);
  m_butDoBidir.EnableWindow(!mDoingTS);
  m_editBidirAngle.EnableWindow(enable);
  m_butBidirWalkBack.EnableWindow((enable || mBidirStage == BIDIR_FIRST_PART) && 
    !doingDosym);
  m_butBidirUseView.EnableWindow(enable && mLowDoseMode && !doingDosym);
  m_statBidirFieldSize.EnableWindow(enable && (!doingDosym || (mTSParam.dosymDoRunToEnd &&
    mTSParam.dosymAnchorIfRunToEnd)));
  m_butUseDoseSym.EnableWindow(enable && mLowDoseMode);
  m_butSetupDoseSym.EnableWindow(enable && doingDosym);
  enable = enable && !(m_bBidirUseView && mLowDoseMode) && !doingDosym;
  m_statBidirMag.EnableWindow(enable);
  m_statBDMagLabel.EnableWindow(enable);
  m_sbcBidirMag.EnableWindow(enable);
  SetDlgItemText(IDC_STATSTARTAT, m_bDoBidir ? "Tilt to" : "Start at");
  SetDlgItemText(IDC_STATISMICRONS, doingDosym ? "um at start" : "microns");
}

// Keeps the angles legal when bidirectional is enabled
void CTSSetupDialog::ConstrainBidirAngle(bool warningIfShift, bool anglesChanged)
{
  float minAngle = B3DMIN(m_fStartAngle, m_fEndAngle);
  float maxAngle = B3DMAX(m_fStartAngle, m_fEndAngle);
  bool changed = false;
  bool limitEnd = true;
  int dir;
  float minSegment = 6.;
  if (!m_bDoBidir)
    return;

  // If doing a dose-symmetric series, check against supplied limits if any
  if (mDoingTS) {
    if (mMinDosymStart > -900.) {
      dir = mMinDosymStart > m_fBidirAngle ? 1 : -1;
      if (dir * (m_fStartAngle - mMinDosymStart) < 0) {
        m_fStartAngle = (float)(0.01 * B3DNINT(100. * mMinDosymStart));
        changed = true;
      }
    }
    if (mMinDosymEnd > -900.) {
      dir = mMinDosymEnd > m_fBidirAngle ? 1 : -1;
      if (dir * (m_fEndAngle - mMinDosymEnd) < 0) {
        m_fEndAngle = (float)(0.01 * B3DNINT(100. * mMinDosymEnd));
        changed = true;
      }
    }
    if (changed)
      UpdateData(false);
    return;
  }

  // Angles are actively being edited and one is still zero, let it go
  if (anglesChanged && fabs((double)m_fBidirAngle) < 0.1 && 
    (fabs((double)m_fStartAngle) < 0.1 || fabs((double)m_fEndAngle) < 0.1))
    return;

  // Otherwise if the range is less than 20, set bidir to the midpoint
  if (maxAngle - minAngle < 2. * minSegment) {
    changed = fabs(m_fBidirAngle - 0.5 * (maxAngle + minAngle)) > 
      B3DMIN(0.5, maxAngle - minAngle);
    if (changed)
      m_fBidirAngle = 0.5f * (maxAngle + minAngle);

    // Otherwise move it in so it is 10 degrees from one end
  } else if (m_fBidirAngle < minAngle + (minSegment - 0.01) || 
    m_fBidirAngle > maxAngle - (minSegment - 0.01)) {
    changed = true;
    B3DCLAMP(m_fBidirAngle, minAngle + minSegment, maxAngle - minSegment);
    if (warningIfShift) {
      CString mess;
      mess.Format("The starting angle for bidirectional tilting should be at least\n"
        "%.0f degrees from either end; it has been changed to %.1f", minSegment,
        m_fBidirAngle);
      AfxMessageBox(mess, MB_EXCLAME);
    }
  }
  maxAngle = mWinApp->mScope->GetMaxTiltAngle() -
    (float)fabs(mWinApp->mComplexTasks->GetTiltBacklash());
  dir = mWinApp->mTSController->GetFixedDosymBacklashDir();
  limitEnd = !((dir > 0 && m_fStartAngle < m_fEndAngle) ||
    (dir < 0 && m_fStartAngle > m_fEndAngle));
  if (limitEnd && fabs(m_fEndAngle) > maxAngle) {
    B3DCLAMP(m_fEndAngle, -maxAngle, maxAngle);
    changed = true;
  }
  if (!limitEnd && fabs(m_fStartAngle) > maxAngle) {
    B3DCLAMP(m_fStartAngle, -maxAngle, maxAngle);
    changed = true;
  }
  if (changed)
    UpdateData(false);
}

// Take care of the anchor mag and field of view labels for bidirectional
void CTSSetupDialog::ManageAnchorMag(int camera)
{
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + VIEW_CONSET;
  int size = B3DMIN(camParam->sizeX, camParam->sizeY);

  // Make the spinner mag with the mag from properties, but then modify if using View
  // to get the actual field of view
  int magInd = mBidirMagInd[2 * mSTEMindex + (mLowDoseMode ? 1 : 0)];
  m_strBidirMag.Format("%d", MagForCamera(camParam, magInd));
  if (mLowDoseMode && (m_bBidirUseView || m_bUseDoseSym))
    magInd = ldp->magIndex;
  float fov = size * mWinApp->mShiftManager->GetPixelSize(camera, magInd);
  if (fov < 10.)
    m_strBidirFieldSize.Format("%.1f um", fov);
  else
    m_strBidirFieldSize.Format("%.0f um", fov);
  CString star = "*";
  if (fov < mWinApp->mTSController->GetMinFieldBidirAnchor()) {
    m_strBidirFieldSize = star + m_strBidirFieldSize + star;
    m_statBidirFieldSize.SetFont(mTitleFont);
  } else
    m_statBidirFieldSize.SetFont(m_statBDMagLabel.GetFont());
  if (fov < mWinApp->mTSController->GetAlarmBidirFieldSize())
    m_strBidirFieldSize = star + m_strBidirFieldSize + star;

  // I tried using OnPaint and drawing colored backgrounds, it was never called.
}

// Dose-symmetric tilting turned on or off: swap the IS limits
void CTSSetupDialog::OnUseDoseSym()
{
  UpdateData(true);
  if (m_bUseDoseSym) {
    mTSParam.ISlimit = m_fLimitIS;
    m_fLimitIS = mTSParam.dosymStartingISLimit;
  } else {
    mTSParam.dosymStartingISLimit = m_fLimitIS;
    m_fLimitIS = mTSParam.ISlimit;
  }
  UpdateData(false);
  ManageBidirectional();
  ManageDriftWait();
}

// Open dialog to set dose-symmetric parameters
void CTSSetupDialog::OnButSetupDoseSym()
{
  CTSDoseSymDlg dlg;
  FixButtonStyle(IDC_BUT_SETUP_DOSE_SYM);
  dlg.mTSparam = mTSParam;
  dlg.mTSparam.startingTilt = m_fStartAngle;
  dlg.mTSparam.endingTilt = m_fEndAngle;
  dlg.mTSparam.tiltIncrement = m_fIncrement;
  dlg.mTSparam.bidirAngle = m_fBidirAngle;
  if (dlg.DoModal() == IDOK) {
    dlg.mTSparam.startingTilt = mTSParam.startingTilt;
    dlg.mTSparam.endingTilt = mTSParam.endingTilt;
    dlg.mTSparam.tiltIncrement = mTSParam.tiltIncrement;
    dlg.mTSparam.bidirAngle = mTSParam.bidirAngle;
    mTSParam = dlg.mTSparam;
  }
  ManageBidirectional();
}

// Wait for drift checkbox handler and setup opener
void CTSSetupDialog::OnWaitForDrift()
{
  UpdateData(true);
  ManageDriftWait();
}

void CTSSetupDialog::OnButSetupDriftWait()
{
  CDriftWaitSetupDlg dlg;
  dlg.mTSparam = &mTSParam;
  dlg.DoModal();
  ManageDriftWait();
}

// Enable setup button and make the summary of when it will happen
void CTSSetupDialog::ManageDriftWait()
{
  bool doingDosym = m_bDoBidir && m_bUseDoseSym && mLowDoseMode;
  int aboveBelow = (doingDosym && mTSParam.waitDosymIgnoreAngles) ?
    0 : mTSParam.onlyWaitDriftAboveBelow;
  CString str2;
  CString str = aboveBelow ? "Wait only " : "Wait at every angle";
  m_butSetupDriftWait.EnableWindow(m_bWaitForDrift);
  m_statDWdegrees.EnableWindow(m_bWaitForDrift);
  if (!m_bWaitForDrift) {
    SetDlgItemText(IDC_STAT_DW_DEGREES, "");
    return;
  }
  if (doingDosym && mTSParam.waitAtDosymReversals)
    str = aboveBelow ? "Wait at reversals, only " : "Wait at tilt reversals";
  if (aboveBelow < 0)
    str2.Format("below %.1f deg", mTSParam.waitDriftBelowAngle);
  if (aboveBelow > 0)
    str2.Format("above %.1f deg", mTSParam.waitDriftAboveAngle);
  SetDlgItemText(IDC_STAT_DW_DEGREES, str + str2);
}

void CTSSetupDialog::ManageCosinePower()
{
  CString str;
  int typeVaries[MAX_VARY_TYPES];

  CTSVariationsDlg::ListVaryingTypes(mTSParam.varyArray, mTSParam.numVaryItems,
    typeVaries, NULL);
  str.Format("Vary %s to be %.2f", m_bChangeExposure && (mSTEMindex ||
    !(m_bVaryParams && typeVaries[TS_VARY_EXPOSURE])) ? "exposure" : "intensity", 
    mCosMultipliers[mCosPowerInd]);
  UtilTrimTrailingZeros(str);
  m_butIntensityCosine.SetWindowText(str);
  str.Format("%.2f", mCosinePowers[mCosPowerInd]);
  UtilTrimTrailingZeros(str);
  m_strCosPower.Format("x higher at 60 deg (1/cos to 1/%s power)", (LPCTSTR)str);
    
  UpdateData(false);
}

int CTSSetupDialog::RegularOrEFTEMMag(int magIndex)
{
  return MagForCamera(&mCamParams[mCurrentCamera], magIndex);
}

// Find mag index with field size closest to the current upon camera change
int CTSSetupDialog::MagIndWithClosestFieldSize(int oldCam, int oldMag, int newCam, 
                                               int newSTEM)
{
  int lower, upper, ind, minInd, lowest;
  double oldSize, newSize, diff, minDiff = 1.e30;
  oldSize = mWinApp->mShiftManager->GetPixelSize(oldCam, oldMag);
  oldSize *= oldSize * mCamParams[oldCam].sizeX * mCamParams[oldCam].sizeY;
  lowest = mWinApp->mScope->GetLowestNonLMmag(newSTEM);
  mWinApp->GetMagRangeLimits(newCam, lowest, lower, upper);
  for (ind = lowest; ind <= upper; ind++) {
    if (!MagForCamera(newCam, ind))
      continue;
    newSize = mWinApp->mShiftManager->GetPixelSize(newCam, ind);
    newSize *= newSize * mCamParams[newCam].sizeX * mCamParams[newCam].sizeY;
    diff = fabs(oldSize - newSize);
    if (diff < minDiff) {
      minDiff = diff;
      minInd = ind;
    }
  }
  return minInd;
}

// Unload all the parameters back to the structure
void CTSSetupDialog::OnOK()
{
  if (mFuture) {
    FixButtonStyle(IDC_TSGO);
    mWinApp->mTSController->SetExtraOutput(&mTSParam);
  } else {
    DoOK();
  }
}

void CTSSetupDialog::DoOK()
{
  UpdateData(true);
  ConstrainBidirAngle(true, false);
  mTSParam.startingTilt = m_fStartAngle;
  mTSParam.endingTilt =  m_fEndAngle;
  mTSParam.tiltIncrement =  m_fIncrement;
  mTSParam.cosineTilt =  m_bCosineInc;
  mTSParam.tiltDelay =  m_fTiltDelay;
  mTSParam.tiltDelay = m_fTiltDelay;
  mTSParam.bidirAngle = m_fBidirAngle;
  mTSParam.doBidirectional = m_bDoBidir;
  mTSParam.anchorBidirWithView = m_bBidirUseView;
  mTSParam.walkBackForBidir = m_bBidirWalkBack;
  mTSParam.doDoseSymmetric = m_bUseDoseSym;
  mTSParam.cameraIndex = m_iCamera;
  mTSParam.probeMode = mProbeMode;
  for (int j = 0; j < 3; j++) {
    mTSParam.magIndex[j] =  mMagIndex[j];
    mTSParam.lowMagIndex[j] =  mLowMagIndex[j];
    mTSParam.bidirAnchorMagInd[2 * j] = mBidirMagInd[2 * j];
    mTSParam.bidirAnchorMagInd[2 * j + 1] = mBidirMagInd[2 * j + 1];
  }
  mTSParam.binning =  mBinning;
  mTSParam.trackLowMag = m_bLowMagTrack;
  mTSParam.beamControl =  beamCodes[m_iBeamControl];
  mTSParam.cosinePower = mCosinePowers[mCosPowerInd];
  mTSParam.meanCounts = m_iCounts;
  mTSParam.limitIntensity = m_bLimitIntensity;
  mTSParam.limitToCurrent = m_bLimitToCurrent;
  mTSParam.intensitySetAtZero = m_bIntensitySetAtZero;
  mTSParam.refineEucen = m_bRefineEucen;
  mTSParam.leaveAnchor = m_bLeaveAnchor;
  mTSParam.anchorTilt = m_fAnchorAngle;
  mTSParam.refIsInA = m_bUseAForRef;
  mTSParam.useAnchor = m_bUseAnchor;
  mTSParam.anchorBuffer = mAnchorBuf;
  if (m_bDoBidir && m_bUseDoseSym && mLowDoseMode)
    mTSParam.dosymStartingISLimit = m_fLimitIS;
  else
    mTSParam.ISlimit = m_fLimitIS;
  mTSParam.beamTilt = m_fBeamTilt;
  mTSParam.targetDefocus = m_fTargetDefocus;
  mTSParam.autofocusOffset = m_fFocusOffset;
  mTSParam.refocusThreshold = m_fIterThresh;
  mTSParam.skipAutofocus = m_bSkipAutofocus;
  mTSParam.focusCheckInterval = m_fCheckFocus;
  mTSParam.repeatRecord = m_bRepeatRecord;
  mTSParam.confirmLowDoseRepeat = m_bConfirmLowDoseRepeat;
  mTSParam.maxRecordShift = m_fRepeatRecord / 100.f;
  mTSParam.stopOnBigAlignShift = m_bStopOnBigShift;
  mTSParam.maxAlignShift = m_fBigShift;
  mTSParam.repeatAutofocus = m_bRepeatFocus;
  mTSParam.maxFocusDiff = m_fRepeatFocus;
  mTSParam.alwaysFocusHigh = m_bAlwaysFocus;
  mTSParam.alwaysFocusAngle = m_fAlwaysAngle;
  mTSParam.checkAutofocus = m_bCheckAutofocus;
  mTSParam.limitDefocusChange = m_bLimitDeltaFocus;
  mTSParam.defocusChangeLimit = m_fFocusMaxDelta;
  mTSParam.applyAbsFocusLimits = m_bLimitAbsFocus;
  mTSParam.minFocusAccuracy = m_fMinFocusAccuracy;
  mTSParam.taperCounts = m_bRampBeam;
  mTSParam.highCounts = m_iTaperCounts;
  mTSParam.taperAngle = m_fTaperAngle;
  mTSParam.trackAfterFocus = m_iTrackAfterFocus;
  mTSParam.alignTrackOnly = m_bAlignTrackOnly;
  mTSParam.previewBeforeNewRef = m_bPreviewBeforeRef;
  mTSParam.maxRefDisagreement = m_fNewTrackDiff / 100.f;
  mTSParam.maxPredictErrorXY = m_fPredictErrorXY / 100.f;
  mTSParam.maxPredictErrorZ[mSTEMindex] = m_fPredictErrorZ;
  mTSParam.refineZLP = m_bRefineZLP;
  mTSParam.zlpInterval = m_iZlpInterval;
  mTSParam.centerBeamFromTrial = m_bCenterBeam;
  mTSParam.closeValvesAtEnd = m_bCloseValves;
  mTSParam.manualTracking = m_bManualTrack;
  mTSParam.doVariations = m_bVaryParams;
  mTSParam.changeRecExposure = mSTEMindex ? mChangeRecExposure : m_bChangeExposure;
  mTSParam.changeAllExposures = m_bChangeAllExp;
  mTSParam.changeSettling = m_bChangeSettling;
  mTSParam.cenBeamAbove = m_bAutocenCrossing;
  mTSParam.cenBeamPeriodically = m_bAutocenInterval;
  mTSParam.cenBeamInterval = m_iAutocenInterval;
  mTSParam.cenBeamAngle = m_fAutocenAngle;
  mTSParam.waitForDrift = m_bWaitForDrift;
  mTSParam.doEarlyReturn = m_bDoEarlyReturn;
  
  CDialog::OnOK();
}

// Set the postpone flag then treat same as OK
void CTSSetupDialog::OnPostpone() 
{
  mPostpone = true;
  DoOK();
}

void CTSSetupDialog::OnSinglestep() 
{
  mSingleStep = true;
  DoOK();
}

// Manage the open and closing of panels in the dialog
void CTSSetupDialog::ManagePanels(void)
{
  int panel, curTop = topTable[0], cumulDrop, firstDropped, topPos, drawnMaxBottom;
  int topAtLastDraw;
  CRect rect, winRect;
  int panelTop, leftTop, index, id, nextTop, buttonHigh, ixOffset = 0;
  CWnd *wnd;
  HDWP positions;
  bool draw, twoCol, drop, droppingLine;
  wnd = GetDlgItem(idTable[0]);
  wnd->GetClientRect(rect);
  buttonHigh = rect.Height();

  index = 1;
  for (panel = 0; panel < mNumPanels; panel++)
    index += mNumInPanel[panel];
  positions = BeginDeferWindowPos(index);
  if (!positions)
    return;

  // First determine if two columns or one
  twoCol = PanelDisplayType() == 2;
  m_statVertline.ShowWindow(twoCol ? SW_SHOW : SW_HIDE);

  for (panel = 0; panel < mNumPanels; panel++) {
    panelTop = topTable[mPanelStart[panel]];
    
    // Draw the first 4 unconditionally
    for (index = mPanelStart[panel]; index < mPanelStart[panel] +  4; index++) {
      id = idTable[index];
      if (id > 0) {
        wnd = GetDlgItem(id);
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index] + 
          ixOffset, curTop + topTable[index] - panelTop, 0,0, SWP_NOZORDER | SWP_NOSIZE);
      }
      if (index == mPanelStart[panel] && panel < mNumPanels - 1)
        wnd->SetWindowText(mPanelOpen[panel] ? "-" : "+");
    }

    // Draw horizontal line higher up if closed and set top for next panel, otherwise
    // set top from full height of this panel
    if (!mPanelOpen[panel]) {
      wnd = GetDlgItem(idTable[index]);
      positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index] + 
        ixOffset, curTop + buttonHigh + 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
      nextTop = curTop + buttonHigh + 2 + topTable[mPanelStart[panel+1]] - topTable[index];
    } else if (panel < mNumPanels - 1) {
      nextTop = curTop + topTable[mPanelStart[panel+1]] - topTable[mPanelStart[panel]];
    }

    // Now draw or hide the rest of the widgets
    cumulDrop = 0;
    firstDropped = -1;
    droppingLine = false;
    drawnMaxBottom = 0;
    topAtLastDraw = 0;
    for (index = mPanelStart[panel] + B3DCHOICE(panel < mNumPanels - 1, 5, 4);
      index < mPanelStart[panel] + mNumInPanel[panel]; index++) {
      wnd = GetDlgItem(idTable[index]);
      draw = true;
      drop = false;
      for (id = (mNumCameras > 1 ? mNumCameras : 0); id < MAX_DLG_CAMERAS; id++) {
        if (idTable[index] == IDC_RCAMERA1 + id) {
          draw = false;
          drop = mNumCameras == 1;
        }
      }
      if (mFuture > 0 && idTable[index] == IDC_SINGLESTEP)
        draw = false;
      if (mSTEMindex && idTable[index] == IDC_CENTER_FROM_TRIAL || 
        !mSTEMindex && idTable[index] == IDC_STAT_INTERSET)
        draw = false;

      // Take care of dropping then draw or not, keeping track of top and maximum bottom
      // when draw
      ManageDropping(topTable, index, idTable[index], topAtLastDraw, cumulDrop,
        firstDropped, droppingLine, drop);
      if (mPanelOpen[panel] && draw && !drop) {
        topPos = (curTop - cumulDrop) + topTable[index] - panelTop;
        ACCUM_MAX(drawnMaxBottom, topPos + heightTable[index] + mBottomDrawMargin);
        topAtLastDraw = topTable[index];
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index] +
          ixOffset, topPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
      } else {
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, 0, 0, 0, 0,
          SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
      }
    }

    // If last one in panel dropped, add to cumulative distance
    if (firstDropped >= 0 &&
      B3DABS(topTable[firstDropped] - topAtLastDraw) > mSameLineCrit)
      cumulDrop += topTable[mPanelStart[panel + 1]] - topTable[firstDropped];

    // Now draw the bottom line in an open panel except for the last panel
    if (mPanelOpen[panel] && panel < mNumPanels - 1) {
      index = mPanelStart[panel] + 4;
      id = idTable[index];
      if (id > 0) {
        wnd = GetDlgItem(id);
        topPos = (curTop - cumulDrop) + (topTable[index] - panelTop) +
          (cumulDrop ? mBottomDrawMargin : 0);
        ACCUM_MAX(drawnMaxBottom, topPos + heightTable[index] + mBottomDrawMargin);
        positions = DeferWindowPos(positions, wnd->m_hWnd, NULL, leftTable[index] +
          ixOffset, topPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
      }
      nextTop -= cumulDrop;
    }
    curTop = B3DMAX(drawnMaxBottom, nextTop);
    if (twoCol && panel == NUM_LEFT_PANELS - 1) {
      leftTop = curTop;
      curTop = topTable[0];
      ixOffset = mBasicWidth - leftTable[0];
    }
    if (twoCol && panel == mNumPanels - 2) {
      curTop = B3DMAX(curTop, leftTop);
      ixOffset = mBasicWidth / 2;
    }
  }

  if (mFuture > 0) {
    m_butGo.ShowWindow(SW_HIDE);
    m_butSingleLoop.ShowWindow(SW_HIDE);
  }

  // resize dialog to end below the last button.  No need to force a redraw with defers
  EndDeferWindowPos(positions);
  wnd->GetWindowRect(rect);
  GetWindowRect(winRect);
  SetWindowPos(NULL, 0, 0, twoCol ? 2 * mBasicWidth - leftTable[0] : mBasicWidth,
    rect.bottom + 8 - winRect.top, SWP_NOMOVE);
  //Invalidate();
  //UpdateWindow();
}


void CTSSetupDialog::OnOpenClose(UINT nID)
{
  FixButtonStyle(nID);
  mPanelOpen[nID - IDC_TSS_PLUS1] = mPanelOpen[nID - IDC_TSS_PLUS1] ? 0 : 1;
  ManagePanels();
}

void CTSSetupDialog::OnButTssLeft()
{
  FixButtonStyle(IDC_BUT_TSS_LEFT);
  for (int i = 0; i < mNumPanels - 1; i++)
    mPanelOpen[i] = i < NUM_LEFT_PANELS ? 1 : 0;
  ManagePanels();
}

void CTSSetupDialog::OnButTssRight()
{
  FixButtonStyle(IDC_BUT_TSS_RIGHT);
  for (int i = 0; i < mNumPanels - 1; i++)
    mPanelOpen[i] = i >= NUM_LEFT_PANELS ? 1 : 0;
  ManagePanels();
}

void CTSSetupDialog::FixButtonStyle(UINT nID)
{
  CButton *button = (CButton *)GetDlgItem(nID);
  button->SetButtonStyle(BS_PUSHBUTTON);
  SetFocus();
}

void CTSSetupDialog::OnButTssPrev()
{
  int i;
  FixButtonStyle(IDC_BUT_TSS_PREV);
  for (i = mNumPanels - 2; i >= 0; i--)
    if (mPanelOpen[i])
      break;
  if (i > 0) {
    mPanelOpen[i] = 0;
    mPanelOpen[i-1] = 1;
  }
  ManagePanels();
}

void CTSSetupDialog::OnButTssNext()
{
  int i;
  FixButtonStyle(IDC_BUT_TSS_NEXT);
  for (i = mNumPanels - 2; i >= 0; i--)
    if (mPanelOpen[i])
      break;
  if (i < mNumPanels - 2) {
    if (i >= 0)
      mPanelOpen[i] = 0;
    mPanelOpen[i+1] = 1;
  }
  ManagePanels();
}

void CTSSetupDialog::OnButTssFull()
{
  FixButtonStyle(IDC_BUT_TSS_FULL);
  for (int i = 0; i < mNumPanels - 1; i++)
    mPanelOpen[i] = 1;
  ManagePanels();
}

// Return 2 if all of one side and at least panel open on the other side
// Return 1 if just all of one side is open, otherwise return 0
int CTSSetupDialog::PanelDisplayType(void)
{
  int type = 0, numLeft = 0, numRight = 0;
  for (int i = 0; i < NUM_TSS_PANELS - 1; i++) {
    if (mPanelOpen[i] && i < NUM_LEFT_PANELS)
      numLeft++;
    else if (mPanelOpen[i])
      numRight++;
  }
  if (numLeft == NUM_LEFT_PANELS && numRight || 
    numRight == NUM_TSS_PANELS - 1 - NUM_LEFT_PANELS && numLeft)
    type = 2;
  else if (numLeft == NUM_LEFT_PANELS || numRight == NUM_TSS_PANELS - 1 - NUM_LEFT_PANELS)
    type = 1;
  return type;
}

void CTSSetupDialog::InitializePanels(int type)
{
  mPanelOpen[0] = 1;
  mPanelOpen[NUM_TSS_PANELS - 1] = 1;
  for (int i = 1; i < NUM_TSS_PANELS - 1; i++) {
    if ((type && i < NUM_LEFT_PANELS) || type == 2)
      mPanelOpen[i] = 1;
    else
      mPanelOpen[i] = 0;
  }
}
