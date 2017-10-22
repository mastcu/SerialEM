// LowDoseDlg.cpp:        Has controls for working in low dose mode
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include ".\LowDoseDlg.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "BeamAssessor.h"
#include "NavHelper.h"
#include "NavigatorDlg.h"
#include "MultiTSTasks.h"
#include "PiezoAndPPControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_VIEW_DEFOCUS  0
#define MIN_VIEW_DEFOCUS  -400
#define MIN_SEARCH_DEFOCUS -1999
#define MAX_SEARCH_DEFOCUS 1999
static int sMinVSDefocus[2] = {MIN_VIEW_DEFOCUS, MIN_SEARCH_DEFOCUS};
static int sMaxVSDefocus[2] = {MAX_VIEW_DEFOCUS, MAX_SEARCH_DEFOCUS};


/////////////////////////////////////////////////////////////////////////////
// CLowDoseDlg dialog


CLowDoseDlg::CLowDoseDlg(CWnd* pParent /*=NULL*/)
  : CToolDlg(CLowDoseDlg::IDD, pParent)
  , m_strViewDefocus(_T(""))
  , m_iAxisAngle(0)
  , m_bRotateAxis(FALSE)
  , m_iOffsetShown(0)
{
  SEMBuildTime(__DATE__, __TIME__);
  //{{AFX_DATA_INIT(CLowDoseDlg)
  m_iShowArea = -1;
  m_iDefineArea = -1;
  m_strMagSpot = _T("");
  m_bLowDoseMode = FALSE;
  m_bTieFocusTrial = FALSE;
  m_strOverlap = _T("");
  m_bBlankWhenDown = FALSE;
  m_strPosition = _T("");
  m_bContinuousUpdate = FALSE;
	m_strElecDose = _T("");
	m_bNormalizeBeam = FALSE;
	m_strBeamShift = _T("");
	m_bSetBeamShift = FALSE;
	m_strLDArea = _T("");
	//}}AFX_DATA_INIT
  mInitialized = false;
  mLastMag = 0;
  mLastSpot = 0;
  mLastIntensity = 0.;
  mLastDose = 0.;
  mLastISX = 0.;
  mLastISY = 0.;
  mIgnoreIS = false;
  mLastSetArea = -10;
  mLastBlanked = false;
  mTrulyLowDose = false;
  mHideOffState = false;
  mLastBeamX = 0.;
  mLastBeamY = 0.;
  mSearchName = "Search";
  mViewShiftX[0] = mViewShiftY[0] = 0.;
  mViewShiftX[1] = mViewShiftY[1] = 0.;
  mStampViewShift[0] = mStampViewShift[1] = -1.;
  mAxisPiezoPlugNum = -1;
  mAxisPiezoNum = -1;
  mPiezoFullDelay = 20.;
  mFocusPiezoDelayFac = 0.6f;
  mTVPiezoDelayFac = 0.3f;
}


void CLowDoseDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CLowDoseDlg)
  DDX_Control(pDX, IDC_STAT_LDAREA, m_statLDArea);
  DDX_Control(pDX, IDC_RESET_BEAM_SHIFT, m_butResetBeamShift);
  DDX_Control(pDX, IDC_SET_BEAM_SHIFT, m_butSetBeamShift);
  DDX_Control(pDX, IDC_STATBEAMSHIFT, m_statBeamShift);
  DDX_Control(pDX, IDC_LDNORMALIZE_BEAM, m_butNormalizeBeam);
  DDX_Control(pDX, IDC_STATELECDOSE, m_statElecDose);
  DDX_Control(pDX, IDC_STATBLANKED, m_statBlanked);
  DDX_Control(pDX, IDC_UNBLANK, m_butUnblank);
  DDX_Control(pDX, IDC_COPYTORECORD, m_butCopyToRecord);
  DDX_Control(pDX, IDC_COPYTOTRIAL, m_butCopyToTrial);
  DDX_Control(pDX, IDC_COPYTOFOCUS, m_butCopyToFocus);
  DDX_Control(pDX, IDC_COPYTOVIEW, m_butCopyToView);
  DDX_Control(pDX, IDC_BALANCESHIFTS, m_butBalanceShifts);
  DDX_Control(pDX, IDC_CENTERUNSHIFTED, m_butCenterUnshifted);
  DDX_Control(pDX, IDC_CONTINUOUSUPDATE, m_butContinuousUpdate);
  DDX_Control(pDX, IDC_RSHOWSEARCH, m_butShowSearch);
  DDX_Control(pDX, IDC_RSHOWRECORD, m_butShowRecord);
  DDX_Control(pDX, IDC_RSHOWTRIAL, m_butShowTrial);
  DDX_Control(pDX, IDC_RSHOWFOCUS, m_butShowFocus);
  DDX_Control(pDX, IDC_RSHOWVIEW, m_butShowView);
  DDX_Control(pDX, IDC_TIEFOCUSTRIAL, m_butTieFocusTrial);
  DDX_Control(pDX, IDC_LOWDOSEMODE, m_butLowDoseMode);
  DDX_Control(pDX, IDC_STATPOSITION, m_statPosition);
  DDX_Control(pDX, IDC_STATMICRON, m_statMicron);
  DDX_Control(pDX, IDC_RDEFINENONE, m_butDefineNone);
  DDX_Control(pDX, IDC_RDEFINEFOCUS, m_butDefineFocus);
  DDX_Control(pDX, IDC_RDEFINETRIAL, m_butDefineTrial);
  DDX_Control(pDX, IDC_EDITPOSITION, m_editPosition);
  DDX_Control(pDX, IDC_STATOVERLAP, m_statOverlap);
  DDX_Control(pDX, IDC_STATMAGSPOTC2, m_statMagSpot);
  DDX_Control(pDX, IDC_BLANKBEAM, m_butBlankBeam);
  DDX_Radio(pDX, IDC_RSHOWVIEW, m_iShowArea);
  DDX_Radio(pDX, IDC_RDEFINENONE, m_iDefineArea);
  DDX_Text(pDX, IDC_STATMAGSPOTC2, m_strMagSpot);
  DDX_Check(pDX, IDC_LOWDOSEMODE, m_bLowDoseMode);
  DDX_Check(pDX, IDC_TIEFOCUSTRIAL, m_bTieFocusTrial);
  DDX_Text(pDX, IDC_STATOVERLAP, m_strOverlap);
  DDX_Check(pDX, IDC_BLANKBEAM, m_bBlankWhenDown);
  DDX_Text(pDX, IDC_EDITPOSITION, m_strPosition);
  DDV_MaxChars(pDX, m_strPosition, 7);
  DDX_Check(pDX, IDC_CONTINUOUSUPDATE, m_bContinuousUpdate);
  DDX_Text(pDX, IDC_STATELECDOSE, m_strElecDose);
  DDX_Check(pDX, IDC_LDNORMALIZE_BEAM, m_bNormalizeBeam);
  DDX_Text(pDX, IDC_STATBEAMSHIFT, m_strBeamShift);
  DDX_Check(pDX, IDC_SET_BEAM_SHIFT, m_bSetBeamShift);
  DDX_Text(pDX, IDC_STAT_LDAREA, m_strLDArea);
  DDX_Text(pDX, IDC_STATVIEWDEFOCUS, m_strViewDefocus);
  DDX_Control(pDX, IDC_SPINVIEWDEFOCUS, m_sbcViewDefocus);
  DDX_Control(pDX, IDC_STATVIEWDEFLABEL, m_statViewDefLabel);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_SET_VIEW_SHIFT, m_butSetViewShift);
  DDX_Control(pDX, IDC_ZERO_VIEW_SHIFT, m_butZeroViewShift);
  DDX_Control(pDX, IDC_STAT_LD_DEG, m_statDegrees);
  DDX_Control(pDX, IDC_EDIT_AXISANGLE, m_editAxisAngle);
  DDX_Control(pDX, IDC_LD_ROTATE_AXIS, m_butRotateAxis);
  DDX_Check(pDX, IDC_LD_ROTATE_AXIS, m_bRotateAxis);
  DDX_Text(pDX, IDC_EDIT_AXISANGLE, m_iAxisAngle);
  DDV_MinMaxInt(pDX, m_iAxisAngle, -90, 90);
  DDX_Control(pDX, IDC_STATVIEWDEFOCUS, m_statViewDefocus);
  DDX_Radio(pDX, IDC_RVIEW_OFFSET, m_iOffsetShown);
}


BEGIN_MESSAGE_MAP(CLowDoseDlg, CToolDlg)
  //{{AFX_MSG_MAP(CLowDoseDlg)
  ON_BN_CLICKED(IDC_BLANKBEAM, OnBlankbeam)
  ON_BN_CLICKED(IDC_LOWDOSEMODE, OnLowdosemode)
  ON_BN_CLICKED(IDC_RDEFINEFOCUS, OnRdefine)
  ON_BN_CLICKED(IDC_RSHOWFOCUS, OnRshow)
  ON_BN_CLICKED(IDC_TIEFOCUSTRIAL, OnTiefocustrial)
  ON_EN_KILLFOCUS(IDC_EDITPOSITION, OnKillfocusEditposition)
  ON_BN_CLICKED(IDC_BALANCESHIFTS, OnBalanceshifts)
  ON_BN_CLICKED(IDC_CENTERUNSHIFTED, OnCenterunshifted)
  ON_BN_CLICKED(IDC_CONTINUOUSUPDATE, OnContinuousupdate)
  ON_BN_CLICKED(IDC_UNBLANK, OnUnblank)
	ON_BN_CLICKED(IDC_LDNORMALIZE_BEAM, OnLdNormalizeBeam)
  ON_BN_CLICKED(IDC_RDEFINENONE, OnRdefine)
  ON_BN_CLICKED(IDC_RDEFINETRIAL, OnRdefine)
  ON_BN_CLICKED(IDC_RSHOWRECORD, OnRshow)
  ON_BN_CLICKED(IDC_RSHOWTRIAL, OnRshow)
  ON_BN_CLICKED(IDC_RSHOWVIEW, OnRshow)
  ON_BN_CLICKED(IDC_RSHOWSEARCH, OnRshow)
	ON_BN_CLICKED(IDC_RESET_BEAM_SHIFT, OnResetBeamShift)
	ON_BN_CLICKED(IDC_SET_BEAM_SHIFT, OnSetBeamShift)
  ON_NOTIFY(UDN_DELTAPOS, IDC_SPINVIEWDEFOCUS, OnDeltaposSpinviewdefocus)
	//}}AFX_MSG_MAP
  ON_COMMAND_RANGE(IDC_COPYTOVIEW, IDC_COPYTORECORD, OnCopyArea)
  ON_BN_CLICKED(IDC_SET_VIEW_SHIFT, OnSetViewShift)
  ON_BN_CLICKED(IDC_ZERO_VIEW_SHIFT, OnZeroViewShift)
  ON_BN_CLICKED(IDC_LD_ROTATE_AXIS, OnLdRotateAxis)
  ON_EN_KILLFOCUS(IDC_EDIT_AXISANGLE, OnKillfocusEditAxisangle)
  ON_BN_CLICKED(IDC_RVIEW_OFFSET, OnRadioShowOffset)
  ON_BN_CLICKED(IDC_RSEARCH_OFFSET, OnRadioShowOffset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLowDoseDlg message handlers

// Changing beam blank when down: inform the scope
void CLowDoseDlg::OnBlankbeam() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();  
  mScope->SetBlankWhenDown(m_bBlankWhenDown);  

}

// External entry for toggling that state
void CLowDoseDlg::ToggleBlankWhenDown(void)
{
  m_bBlankWhenDown = !m_bBlankWhenDown;   
  mScope->SetBlankWhenDown(m_bBlankWhenDown);  
  UpdateData(false);
}

// External entry for changing the mode
void CLowDoseDlg::SetLowDoseMode(BOOL inVal, BOOL hideOffState)
{
  if (inVal == m_bLowDoseMode && inVal == mTrulyLowDose)
    return;
  if (inVal && mWinApp->GetSTEMMode() && mWinApp->DoSwitchSTEMwithScreen())
    return;

  // Set a one-shot flag for hiding the off state; only set the variable for 
  // the checkbox if it is not being hidden
  mHideOffState = hideOffState;
  if (inVal || !hideOffState)
    m_bLowDoseMode = inVal;
  UpdateData(false);
  OnLowdosemode();
}

// Translate axis position back and forth to ISX,ISY when mode changes
void CLowDoseDlg::ConvertAxisPosition(BOOL axisToIS)
{
  int i, mag, magv, area;
  double delX, delY;
  MagTable *magTab = mWinApp->GetMagTable();

  // Convert axis to IS or back for all but the View set
  for (i = 1; i < MAX_LOWDOSE_SETS; i++) {
    mag = mLDParams[i].magIndex;
    if (!mag)
      continue;
    if (axisToIS)
      ConvertOneAxisToIS(mag, mLDParams[i].axisPosition, mLDParams[i].ISX, 
        mLDParams[i].ISY);
    else
      mLDParams[i].axisPosition = ConvertOneIStoAxis(mag, mLDParams[i].ISX, 
        mLDParams[i].ISY);
  }

  // Enforce view/search and Record having same position and add in the offset if any
  for (i = 0; i < 2; i ++) {
    area = i ? SEARCH_AREA : VIEW_CONSET;
    if (axisToIS) {
      TransferISonAxis(mLDParams[RECORD_CONSET].magIndex, mLDParams[RECORD_CONSET].ISX, 
        mLDParams[RECORD_CONSET].ISY,
        mLDParams[area].magIndex, mLDParams[area].ISX, mLDParams[area].ISY);
      mag = mLDParams[RECORD_CONSET].magIndex;
      magv = mLDParams[0].magIndex;
      if ((mViewShiftX[i] != 0. || mViewShiftY[i] != 0.) && mag && magv) {
        delX = mViewShiftX[i];      
        delY = mViewShiftY[i];

        // If offsets are being applied, use the net offset instead
        if (mScope->GetApplyISoffset())
          GetNetViewShift(delX, delY, area);
        mLDParams[area].ISX += delX;
        mLDParams[area].ISY += delY;
      }
    } else {
      mLDParams[area].axisPosition = mLDParams[RECORD_CONSET].axisPosition;
    }
  }
  for (i = 0; i < 4; i++)
    SEMTrace('l', "%d: mag %d  axisPos %.2f  IS  %f  %f", i, mLDParams[i].magIndex, 
      mLDParams[i].axisPosition, mLDParams[i].ISX, mLDParams[i].ISY);
}

// If there is a view shift set, unconditionally subtract the difference between
// the View offset and the transferred Record offset from the full stored shift
void CLowDoseDlg::GetNetViewShift(double &shiftX, double &shiftY, int area)
{
  int mag = mLDParams[RECORD_CONSET].magIndex, magv = mLDParams[area].magIndex;
  MagTable *magTab = mWinApp->GetMagTable();
  CameraParameters *camp = mWinApp->GetActiveCamParam();
  int areaInd = (area != VIEW_CONSET ? 1 : 0);
  shiftX = shiftY = 0.;
  if ((!mViewShiftX[areaInd] && !mViewShiftY[areaInd]) || !mag || !magv)
    return;
  mShiftManager->TransferGeneralIS(mag, magTab[mag].calOffsetISX[camp->GIF], 
    magTab[mag].calOffsetISY[camp->GIF], magv, shiftX, shiftY);
  shiftX += mViewShiftX[areaInd] - magTab[magv].calOffsetISX[camp->GIF];
  shiftY += mViewShiftY[areaInd] - magTab[magv].calOffsetISY[camp->GIF];
}

double CLowDoseDlg::ConvertOneIStoAxis(int mag, double ISX, double ISY)
{
  double specx, specy;
  ScaleMat cMat = mShiftManager->IStoSpecimen(mag);
  if (!cMat.xpx)
    return 0.;
  specx = cMat.xpx * ISX + cMat.xpy * ISY;
  if (m_bRotateAxis && m_iAxisAngle) {
    specy = cMat.ypx * ISX + cMat.ypy * ISY;
    specx = specx * cos(DTOR * m_iAxisAngle) + specy * sin(DTOR * m_iAxisAngle);
  }
  SEMTrace('L', "ConvertOneIStoAxis mag %d  cam %d IS %.2f  %.2f  axis %.2f",
    mag, mWinApp->GetCurrentCamera(), ISX,ISY, specx);
  return specx;
}

double CLowDoseDlg::ConvertIStoAxisAndAngle(int mag, double ISX, double ISY, int &angle)
{
  double specx, specy;
  ScaleMat cMat = mShiftManager->IStoSpecimen(mag);
  if (!cMat.xpx) {
    angle = 0;
    return 0.;
  }
  specx = cMat.xpx * ISX + cMat.xpy * ISY;
  specy = cMat.ypx * ISX + cMat.ypy * ISY;
  angle = B3DNINT(atan2(specy, fabs(specx)) / DTOR);
  SEMTrace('L', "ConvertIStoAxisAndAngle mag %d  IS %.2f %.2f  spec %.2f %.2f angle %.1f",
    mag, ISX,ISY, specx, specy, angle);
  return ((specx >= 0. ? 1. : -1.) * sqrt(specx * specx + specy * specy));
}

void CLowDoseDlg::ConvertOneAxisToIS(int mag, double axis, double &ISX, double &ISY)
{
  double rx, ry;
  ScaleMat cInv = mShiftManager->MatInv(mShiftManager->IStoSpecimen(mag));
  if (!cInv.xpx) {
    ISX = ISY = 0.;
    return;
  }
  if (m_bRotateAxis && m_iAxisAngle) {
    rx = axis * cos(DTOR * m_iAxisAngle);
    ry = axis * sin(DTOR * m_iAxisAngle);
    ISX = cInv.xpx * rx + cInv.xpy * ry;
    ISY = cInv.ypx * rx + cInv.ypy * ry;
  } else {
    ISX = cInv.xpx * axis;
    ISY = cInv.ypx * axis;
  }
  SEMTrace('L', "ConvertOneAxisToIS mag %d  cam %d  axis %.2f IS %.2f  %.2f",
    mag, mWinApp->GetCurrentCamera(), axis, ISX,ISY);
}

// Transfer an image shift from one mag to another: copy if IS is congruent, otherwise
// convert to axis position and back
void CLowDoseDlg::TransferISonAxis(int fromMag, double fromX, double fromY, int toMag, 
                             double &toX, double &toY)
{
  toX = toY = 0;
  if (!fromMag || !toMag)
    return;
  if (fromMag == toMag || mShiftManager->CanTransferIS(fromMag, toMag)) {
    toX = fromX;
    toY = fromY;
    if (fromMag != toMag)
      SEMTrace('L', "TransferISonAxis %d to %d direct copy", fromMag, toMag);
  } else {
    double axis = ConvertOneIStoAxis(fromMag, fromX, fromY);
    ConvertOneAxisToIS(toMag, axis, toX, toY);
    SEMTrace('L', "TransferISonAxis %d to %d:  %f %f  to %f %f", fromMag, toMag, fromX,
      fromY, toX, toY);
  }
}


// Transfer the base image shift from one mag to another
void CLowDoseDlg::TransferBaseIS(int mag, double &ISX, double &ISY)
{
  mShiftManager->TransferGeneralIS(mBaseMag, mBaseISX, mBaseISY, mag, ISX, ISY);
}

// Switching the mode: inform the scope, and update the enables
void CLowDoseDlg::OnLowdosemode() 
{
  // If STEM is on and set to switch with screen, restore check box and get it disabled
  if (mWinApp->GetSTEMMode() && mWinApp->DoSwitchSTEMwithScreen()) {
    UpdateData(false);
    Update();
    return;
  }
  UpdateData(true);

  // Set the true state of low dose; if hiding that it is off, set it off and
  // clear the one-shot flag
  mTrulyLowDose = m_bLowDoseMode;
  if (mHideOffState)
    mTrulyLowDose = false;

  if (m_bSetBeamShift)
    FinishSettingBeamShift(false);

  ConvertAxisPosition(mTrulyLowDose);
  if (mTrulyLowDose && mScope->GetUsePiezoForLDaxis())
    CheckAndActivatePiezoShift();
  if (mTrulyLowDose)
    mScope->SetFocusCameFromView(false);

  mScope->SetLowDoseMode(mTrulyLowDose, mHideOffState);
  mHideOffState = false;
  mWinApp->RestoreViewFocus();

  // Turn off continuous update when leaving low dose
  if (!mTrulyLowDose && m_bContinuousUpdate) {
    m_bContinuousUpdate = false;
    mScope->SetLDContinuousUpdate(false);
  }
  
  // Turn off define function in either direction to keep things simple
  if (m_iDefineArea > 0)
    TurnOffDefine();
  m_iDefineArea = 0;
  Update();

  // If read buffer is in proper relation to # of rolls, then change it to allow
  // space for second align buffer
  int delBuf = mTrulyLowDose ? 2 : 3;
  int nRoll = mWinApp->mBufferManager->GetShiftsOnAcquire();
  if (nRoll + delBuf == mWinApp->mBufferManager->GetBufToReadInto()) {
    if (nRoll + 5 - delBuf < MAX_BUFFERS)
      mWinApp->mBufferManager->SetBufToReadInto(nRoll + 5 - delBuf);
    else
      mWinApp->mBufferManager->SetShiftsOnAcquire(nRoll - 1);
  }
  mWinApp->UpdateWindowSettings();
  if (mWinApp->ScopeHasSTEM())
    mWinApp->mSTEMcontrol.UpdateEnables();
}

// Switching beam normalization - set the scope parameter
void CLowDoseDlg::OnLdNormalizeBeam() 
{
  UpdateData(true);
  mScope->SetLDNormalizeBeam(m_bNormalizeBeam);
}

// Changing the define area: just update the area and let scope update drive the rest
void CLowDoseDlg::OnRdefine() 
{
  double ISX, ISY;
  int lastArea = mScope->GetLowDoseArea();
  UpdateData(true);
  mWinApp->RestoreViewFocus();  

  // Get the base image shift and matching mag
  if (m_iDefineArea) {

    mScope->GetImageShift(mBaseISX, mBaseISY);
    mBaseMag = mScope->GetMagIndex();
    if (lastArea >= 0) {
      mShiftManager->TransferGeneralIS(mLDParams[lastArea].magIndex, 
        mLDParams[lastArea].ISX, mLDParams[lastArea].ISY, mBaseMag, ISX, ISY);
      mBaseISX -= ISX;
      mBaseISY -= ISY;
    }
    
    // Set the down area equal to the area being defined
    mShowAreaSave = m_iShowArea;
    m_iShowArea = m_iDefineArea;
    mScope->SetLowDoseDownArea(m_iShowArea);
  } else {
    TurnOffDefine();
  }
  Update();
  ManageAxisPosition();

  // (No longer go through a show cycle) and set up user point
  if (m_iDefineArea) {
    if (UsefulImageInA()) {
      mImBufs->mHasUserPt = true;
      FixUserPoint(0);
    }
  }
}

// Central place to handle turning off Define Areas
void CLowDoseDlg::TurnOffDefine()
{
  m_iDefineArea = 0;
  m_iShowArea = mShowAreaSave;
  mScope->SetLowDoseDownArea(m_iShowArea);

  // If balance shifts is on, turn off and back on to get them recentered
  if (ShiftsBalanced()) {
    OnCenterunshifted();
    OnBalanceshifts();
  }
  ManageAxisPosition();
}

// Changing the show area: inform the scope and update the mag line if screen up
void CLowDoseDlg::OnRshow() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
  
  mScope->SetLowDoseDownArea(m_iShowArea); 
  if (mTrulyLowDose && !mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy())
    mScope->GotoLowDoseArea(m_iShowArea);
}

// If focus or trial is selected when this is turned on, copy the parameters
void CLowDoseDlg::OnTiefocustrial() 
{
  UpdateData(true);

  // Copy from define area, or from current area if none being defined, or from trial
  int area = m_iDefineArea ? m_iDefineArea : mScope->GetLowDoseArea();
  if ((area + 1) / 2 != 1)
    area = 2;
  if (m_bTieFocusTrial)
    mLDParams[3 - area] = mLDParams[area];
  ManageAxisPosition();
  mWinApp->RestoreViewFocus();
}

// Split shifts evenly between Record and Focus/Trial
void CLowDoseDlg::OnBalanceshifts() 
{
  double delAxis, meanFTaxis, newViewAxis;

  mWinApp->RestoreViewFocus();  
  ConvertAxisPosition(false);

  // Average of focus/trial position
  meanFTaxis = 0.5 * (mLDParams[1].axisPosition + mLDParams[2].axisPosition);
  
  // new view/record position is displaced by half of distance between view and F/T
  newViewAxis = 0.5 * (mLDParams[3].axisPosition - meanFTaxis);

  // So change will bring it there
  delAxis = newViewAxis - mLDParams[3].axisPosition;
  ChangeAllShifts(delAxis);
}

// Set center of View/Record to zero and put all the shifts in focus/trial
void CLowDoseDlg::OnCenterunshifted() 
{
  mWinApp->RestoreViewFocus();  
  ConvertAxisPosition(false);
  ChangeAllShifts(-mLDParams[3].axisPosition);
}

// Change all shifts when the balance buttons are applied
void CLowDoseDlg::ChangeAllShifts(double delAxis)
{
  /* double delISX, delISY;
  int mag; */

  for (int i = 0; i < 4; i++)
    mLDParams[i].axisPosition += delAxis;
  ConvertAxisPosition(true);

  // It is traditional to leave bad ideas around in here.  This confused the users.
  /*// Actually make scope do shift; adjust base if defining
  mag = mScope->GetMagIndex();
  ConvertOneAxisToIS(mag, delAxis, delISX, delISY);
  mScope->IncImageShift(delISX, delISY);
  if (m_iDefineArea) {
    ConvertOneAxisToIS(mBaseMag, delAxis, delISX, delISY);
    mBaseISX += delISX;
    mBaseISY += delISY;
  }

  // Shift image too, so user knows what's what
  if (UsefulImageInA()) {
    ShiftImageInA(delAxis);
    FixUserPoint(1);
  }*/
  mWinApp->mNavHelper->ForceCenterRealign();
  mWinApp->RestoreViewFocus();  
  Update();
}

// A simple test for whether balance shifts is on
bool CLowDoseDlg::ShiftsBalanced(void)
{
  return fabs(mLDParams[3].ISX) > 1.e-5 || fabs(mLDParams[3].ISY) > 1.e-5;
}

void CLowDoseDlg::OnContinuousupdate() 
{
  UpdateData(true); 
  mWinApp->RestoreViewFocus();  
  m_statMagSpot.EnableWindow(m_bContinuousUpdate);
  mScope->SetLDContinuousUpdate(m_bContinuousUpdate);
}


// External call to turn off continuous update
void CLowDoseDlg::SetContinuousUpdate(BOOL state)
{
  if ((state ? 1 : 0) == (m_bContinuousUpdate ? 1 : 0))
    return;
  m_bContinuousUpdate = state;
  m_statMagSpot.EnableWindow(m_bContinuousUpdate);
  mScope->SetLDContinuousUpdate(m_bContinuousUpdate);
  UpdateData(false);  
}


// Copy active area to another one
void CLowDoseDlg::OnCopyArea(UINT nID) 
{
  CString *modeNames = mWinApp->GetModeNames();
  mWinApp->RestoreViewFocus();  
  int area = nID - IDC_COPYTOVIEW;
  int from = mScope->GetLowDoseArea();
  LowDoseParams *ldFrom = &mLDParams[from];
  LowDoseParams *ldArea = &mLDParams[area];
  if (area < 0 || area >= MAX_LOWDOSE_SETS || from < 0)
    return;
  if (AfxMessageBox("Are you sure you want to copy parameters from the " +  
    mModeNames[from] + " area to the " + mModeNames[area] + " area?",
    MB_YESNO | MB_ICONQUESTION) == IDNO)
    return;

  // Convert IS if necessary for change in mag
  TransferISonAxis(ldArea->magIndex, ldArea->ISX, ldArea->ISY, ldFrom->magIndex,
    ldArea->ISX, ldArea->ISY);
  ldArea->magIndex = ldFrom->magIndex;
  ldArea->spotSize = ldFrom->spotSize;
  ldArea->intensity = ldFrom->intensity;
  ldArea->probeMode = ldFrom->probeMode;
  ldArea->energyLoss = ldFrom->energyLoss;
  ldArea->zeroLoss = ldFrom->zeroLoss;
  ldArea->slitWidth = ldFrom->slitWidth;
  ldArea->slitIn = ldFrom->slitIn;
  ldArea->beamAlpha = ldFrom->beamAlpha;
  ldArea->beamDelX = ldFrom->beamDelX;
  ldArea->beamDelY = ldFrom->beamDelY;
  ldArea->beamTiltDX = ldFrom->beamTiltDX;
  ldArea->beamTiltDY = ldFrom->beamTiltDY;
  if ((area + 1) / 2 == 1) {
    ldArea->darkFieldMode = ldFrom->darkFieldMode;
    ldArea->dfTiltX = ldFrom->dfTiltX;
    ldArea->dfTiltY = ldFrom->dfTiltY;
  }
  if (m_bTieFocusTrial && (area + 1) / 2 == 1)
    mLDParams[3 - area] = mLDParams[area];
}

// Unblank the beam temporarily
void CLowDoseDlg::OnUnblank() 
{
  mWinApp->RestoreViewFocus();  
  mScope->BlankBeam(false);  
}

// Reset beam shift of current area to zero 
void CLowDoseDlg::OnResetBeamShift() 
{
  int area = mScope->GetLowDoseArea();
  bool focusTrial = (area + 1) / 2 == 1;
  mWinApp->RestoreViewFocus();  

  // First finish setting current shift if it was happening
  if (m_bSetBeamShift)
    FinishSettingBeamShift(false);
  if (area < 0)
    return;

  // Set actual beam shift back by param's amount, then set to zero
  mScope->IncBeamShift(-mLDParams[area].beamDelX, -mLDParams[area].beamDelY);
  mLDParams[area].beamDelX = 0.;
  mLDParams[area].beamDelY = 0.;
  if (mScope->GetLDBeamTiltShifts()) {
    mScope->IncBeamTilt(-mLDParams[area].beamTiltDX, -mLDParams[area].beamTiltDY);
    mLDParams[area].beamTiltDX = 0.;
    mLDParams[area].beamTiltDY = 0.;
    mWinApp->AppendToLog(CString("Incremental beam tilt for ") + mModeNames[area] +
      " area reset to 0.,0.");
  }
  // TODO: should it zero dark field tilt beam shift?
  if (m_bTieFocusTrial && focusTrial)
    mLDParams[3 - area] = mLDParams[area];
}

// Either start or terminate setting of beam shift
void CLowDoseDlg::OnSetBeamShift() 
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();  
  if (m_bSetBeamShift) {
    mScope->GetBeamShift(mStartingBeamX, mStartingBeamY);
    if (mScope->GetLDBeamTiltShifts())
      mScope->GetBeamTilt(mStartingBTiltX, mStartingBTiltY);
  } else
    FinishSettingBeamShift(true);
}

// Change the view defocus
void CLowDoseDlg::OnDeltaposSpinviewdefocus(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newval, curVal = B3DNINT(mScope->GetLDViewDefocus(m_iOffsetShown));
  int delta = 1;
  bool less = BOOL_EQUIV(pNMUpDown->iDelta > 0, curVal > 0); 
  if (B3DABS(curVal) >= B3DCHOICE(less, 110, 100))
    delta = 10;
  else if (B3DABS(curVal) >= B3DCHOICE(pNMUpDown->iDelta < 0, 55, 50))
    delta = 5;
  else if (B3DABS(curVal) >= B3DCHOICE(pNMUpDown->iDelta < 0, 22, 20))
    delta = 2;

  newval = -delta * ((-(curVal - delta * pNMUpDown->iDelta)) / delta);
  mWinApp->RestoreViewFocus();  
  *pResult = 1;
  if (newval < sMinVSDefocus[m_iOffsetShown] || newval> sMaxVSDefocus[m_iOffsetShown])
    return;

  // If in area now, change defocus by the change
  if (mScope->GetLowDoseArea() == (m_iOffsetShown ? SEARCH_AREA : VIEW_CONSET))
    mScope->IncDefocus((double)(newval - curVal));
  mScope->SetLDViewDefocus((float)newval, m_iOffsetShown);
  m_strViewDefocus.Format("%d", newval);
  UpdateData(false);
}

// Notification from mScope that area is changing - turn off beam shift setting
void CLowDoseDlg::AreaChanging(int inArea)
{
  if (m_bSetBeamShift)
    FinishSettingBeamShift(false);
}

// End of beam setting: turn off checkbox, get shift and add change in shift to 
// existing shift for the area.
void CLowDoseDlg::FinishSettingBeamShift(BOOL toggled)
{
  double beamX, beamY;
  int area = mScope->GetLowDoseArea();
  bool focusTrial = (area + 1) / 2 == 1;
  CString mess;
  m_bSetBeamShift = false;
  if (!toggled) 
    UpdateData(false);
  if (area < 0)
    return;
  mScope->GetBeamShift(beamX, beamY);
  mLDParams[area].beamDelX += beamX - mStartingBeamX;
  mLDParams[area].beamDelY += beamY - mStartingBeamY;
  if (mScope->GetLDBeamTiltShifts()) {
    mScope->GetBeamTilt(beamX, beamY);
    mLDParams[area].beamTiltDX += beamX - mStartingBTiltX;
    mLDParams[area].beamTiltDY += beamY - mStartingBTiltY;
    mess.Format("Incremental beam tilt for %s area set to %.3f, %.3f", 
      (LPCTSTR)mModeNames[area], mLDParams[area].beamTiltDX, mLDParams[area].beamTiltDY);
    mWinApp->AppendToLog(mess);
  }
  if (focusTrial) {
    mScope->GetDarkFieldTilt(mLDParams[area].darkFieldMode, mLDParams[area].dfTiltX,
    mLDParams[area].dfTiltY);
   if (mLDParams[area].darkFieldMode)
     PrintfToLog("Dark field beam tilt for %s area set to %.3f, %.3f", 
       (LPCTSTR)mModeNames[area], mLDParams[area].dfTiltX, mLDParams[area].dfTiltY);
  }
  if (m_bTieFocusTrial && focusTrial)
    mLDParams[3 - area] = mLDParams[area];
}

// Select which area offsets are controlled
void CLowDoseDlg::OnRadioShowOffset()
{
  mWinApp->RestoreViewFocus();  
  UpdateData(true);
  UpdateSettings();
}

// Set the shift for view images
void CLowDoseDlg::OnSetViewShift()
{
  float shiftX, shiftY;
  double delX, delY, ISX, ISY;
  int mag, magv, shiftType, topInd = 0;
  CString mess, refText = mModeNames[RECORD_CONSET], areaText = mModeNames[VIEW_CONSET];
  char *bufferText[3] = {"buffer A", "buffer B", "buffer A and buffer B"};
  MagTable *magTab = mWinApp->GetMagTable();
  mWinApp->RestoreViewFocus();  
  int area = (m_iOffsetShown ? SEARCH_AREA : VIEW_CONSET);
  shiftType = OKtoSetViewShift();
  if (!shiftType)
    return;
  if (shiftType < -1)
    topInd = 1;
  ScaleMat bInv = mShiftManager->CameraToIS(mImBufs[topInd].mMagInd);
  if (!bInv.xpx)
    return;
  mImBufs[topInd].mImage->getShifts(shiftX, shiftY);
  if (m_iOffsetShown) {
    refText += " or " + mModeNames[VIEW_CONSET];
    areaText = mModeNames[SEARCH_CONSET];
  }

  // There is no route for this
  if (shiftType == 1 && shiftX == 0. && shiftY == 0.) {
    mess = "To set a shift for the " + areaText + " area, center a feature"
      " in the " + refText + " area,\ntake a " + areaText + " image, and"
      " shift it with the mouse to center the same feature.\n\n"
      "Then press the Set button.";
    AfxMessageBox(mess, MB_OK | MB_ICONINFORMATION);
    return;
  }

  // Message and choice if it is ambiguous which method to use
  if (shiftType == 2) {
    mess = "The " + areaText + " image in the A buffer is shifted as if\n"
      "you have shifted it into alignment with the " + refText + " area,\n"
      "but there are marker points in both A and B as if you have\n"
      "marked corresponding points in the two images.\n\n"
      "Do you want to set the offset from the shift in the image in A?\n\n"
      "Press Yes to use the shift to set the offset\n\n"
      "To use the corresponding points, press No then press the \n"
      "\"Clear Alignment\" button to clear out the shift in A\n"
      "(and clear the alignment for B also if it has a shift).";
    if (AfxMessageBox(mess, MB_QUESTION) == IDNO)
      return;
    shiftType = 1;
  }

  // Or bail out if there are shifts and markers
  if (shiftType > 1) {
    mess.Format("There is an alignment or mouse shift in %s.\n"
      "The routine to set an offset from marker points does not allow such shifts.\n\n"
      "To use marker points, press the \"Clear Alignment\" button\n"
      "for %s and press \"Set\" again.", bufferText[shiftType - 3], 
      bufferText[shiftType - 3]);
    AfxMessageBox(mess, MB_EXCLAME);
    return;
  }

  mag = mLDParams[RECORD_CONSET].magIndex;
  magv = mLDParams[area].magIndex;
  if (mScope->GetApplyISoffset() && mag && !mViewShiftX[m_iOffsetShown] && 
    !mViewShiftY[m_iOffsetShown]) {

    // If IS offsets are being applied and V offset is still zero, start with the view IS
    // offset minus the transferred Record offset to end  up with the full stored offset
    mShiftManager->TransferGeneralIS(mag, magTab[mag].offsetISX, 
      magTab[mag].offsetISY, magv, delX, delY);
    mViewShiftX[m_iOffsetShown] = magTab[magv].offsetISX - delX;
    mViewShiftY[m_iOffsetShown] = magTab[magv].offsetISY - delY;
  }

  if (shiftType > 0) {

    // Y inversion for deltas, then subtract the image shift just as negative is taken
    // when mouse shifting  
    // Fixed 10/14/12, Y was "= -" not "-=", PLUS the applied offset was added repeatedly
    delX = mImBufs->mBinning * shiftX;
    delY = -mImBufs->mBinning * shiftY;
    mViewShiftX[m_iOffsetShown] -= (bInv.xpx * delX + bInv.xpy * delY);
    mViewShiftY[m_iOffsetShown] -= (bInv.ypx * delX + bInv.ypy * delY);
  } else {

    // Convert image point to image shift in main buffer and add it; 
    SEMTrace('1', "Starting shift %.3f %.3f", mViewShiftX[m_iOffsetShown], 
      mViewShiftY[m_iOffsetShown]);
    delX = mImBufs[topInd].mBinning * (mImBufs[topInd].mUserPtX - 
      mImBufs[topInd].mImage->getWidth() / 2.);
    delY = -mImBufs[topInd].mBinning * (mImBufs[topInd].mUserPtY - 
      mImBufs[topInd].mImage->getHeight() / 2.);
    mViewShiftX[m_iOffsetShown] += (bInv.xpx * delX + bInv.xpy * delY);
    mViewShiftY[m_iOffsetShown] += (bInv.ypx * delX + bInv.ypy * delY);
    SEMTrace('1', "Image delta %.1f %.1f  new shift %.3f %.3f", delX, delY,
      mViewShiftX[m_iOffsetShown], mViewShiftY[m_iOffsetShown]);

    // Convert image point in reference buffer to IS and transfer IS to main mag, subtract
    bInv = mShiftManager->CameraToIS(mImBufs[1 - topInd].mMagInd);
    delX = mImBufs[1 - topInd].mBinning * (mImBufs[1 - topInd].mUserPtX - 
      mImBufs[1 - topInd].mImage->getWidth() / 2.);
    delY = -mImBufs[1 - topInd].mBinning * (mImBufs[1 - topInd].mUserPtY - 
      mImBufs[1 - topInd].mImage->getHeight() / 2.);
    ISX = (bInv.xpx * delX + bInv.xpy * delY);
    ISY = (bInv.ypx * delX + bInv.ypy * delY);
    mShiftManager->TransferGeneralIS(mImBufs[1 - topInd].mMagInd, ISX, ISY, magv, delX,
      delY);
    SEMTrace('1', "IS %.3f %.3f  transfer %.3f %.3f", ISX, ISY, delX, delY);
    mViewShiftX[m_iOffsetShown] -= delX;
    mViewShiftY[m_iOffsetShown] -= delY;
  }
  mStampViewShift[m_iOffsetShown] = mImBufs->mTimeStamp;

  // revise the actual IS of the view area appropriately
  ConvertAxisPosition(false);
  ConvertAxisPosition(true);
  Update();
  if (shiftType > 0) {
    if (mImBufs->mStageShiftedByMouse) {
      mess.Format("The stage was shifted away from the area centered in the %s image.  "
        "To recover from this and check the offset, press \"Reset Image Shift\", then "
        "take a new %s image and center a feature, then take a new %s image.", 
        refText, refText, areaText);
      AfxMessageBox(mess, MB_EXCLAME);
    } else if (mWinApp->mCamera->CameraReady()) {
      mess.Format("Full %s shift offset is set to %.3f, %.3f image shift units\r\n"
        "Taking a new %s image - \r\n   if feature is not centered, shift to center "
        "it and press \"Set\" again", areaText, mViewShiftX[m_iOffsetShown],
        mViewShiftY[m_iOffsetShown], areaText);
      mWinApp->AppendToLog(mess);
      mWinApp->mCamera->SetCancelNextContinuous(true);
      mWinApp->mCamera->InitiateCapture(m_iOffsetShown ? SEARCH_CONSET : VIEW_CONSET);
    }
  } else {
     mess.Format("Full %s shift offset is set to %.3f, %.3f image shift units\r\n",
       areaText, mViewShiftX[m_iOffsetShown], mViewShiftY[m_iOffsetShown]);
    mWinApp->AppendToLog(mess);
  }
}

// Reset the shift
void CLowDoseDlg::OnZeroViewShift()
{
  mWinApp->RestoreViewFocus();  
  mViewShiftX[m_iOffsetShown] = mViewShiftY[m_iOffsetShown] = 0.;
  ConvertAxisPosition(false);
  ConvertAxisPosition(true);
  Update();
}

// Test if it is OK to set shift and return complicated set of values
int CLowDoseDlg::OKtoSetViewShift()
{
  float xx, yy, xx1, yy1;
  bool recOrPrev0, recOrPrev1, search0, search1, view0, view1, shiftOK, aShift, bShift;
  search0 = mImBufs[0].mConSetUsed == SEARCH_CONSET;
  view0 = mImBufs[0].mConSetUsed == VIEW_CONSET;
  if (!mImBufs->mImage || !mImBufs->mBinning || !mImBufs->mMagInd ||
    mStampViewShift[m_iOffsetShown] == mImBufs->mTimeStamp ||!mImBufs->mLowDoseArea || 
    mImBufs->mCamera != mWinApp->GetCurrentCamera())
    return 0;
  mImBufs->mImage->getShifts(xx, yy);
  aShift = (xx != 0. || yy != 0.);

  // Set flag if it is a valid image for shift method, then evaluate for marked points
  shiftOK = ((view0 && !m_iOffsetShown) || (search0 && m_iOffsetShown)) && aShift;
  if (mImBufs[1].mImage && mImBufs[1].mBinning && mImBufs[1].mLowDoseArea &&
    mImBufs[1].mMagInd  && mImBufs->mHasUserPt && mImBufs[1].mHasUserPt) {
      mImBufs[1].mImage->getShifts(xx1, yy1);
      bShift = (xx1 != 0. || yy1 != 0.);
      recOrPrev0 = mImBufs[0].mConSetUsed == RECORD_CONSET || 
        mImBufs[0].mConSetUsed == PREVIEW_CONSET;
      recOrPrev1 = mImBufs[1].mConSetUsed == RECORD_CONSET || 
        mImBufs[1].mConSetUsed == PREVIEW_CONSET;
      search1 = mImBufs[1].mConSetUsed == SEARCH_CONSET;
      view1 = mImBufs[1].mConSetUsed == VIEW_CONSET;

      // For valid pair of images, 3 cases: ambiguous
      // if A shifted, bad but repairable if anything shifted, good for marker method
      if ((m_iOffsetShown && (((view1 || recOrPrev1) && search0) ||
        ((view0 || recOrPrev0) && search1))) || 
        (!m_iOffsetShown && ((recOrPrev1 && view0) || (recOrPrev0 && view1)))) {
          if (shiftOK)
            return 2;
          if (aShift && bShift)
            return 5;
          if (aShift)
            return 3;
          if (bShift)
            return 4;

          // Area to adjust is in A if Search is in A or Record is in B
          return (recOrPrev1 || search0) ? -1 : - 2;
      }
  }
  return shiftOK ? 1 : 0;
}

// External call for enabling just the set view button when mouse shifting
void CLowDoseDlg::EnableSetViewShiftIfOK(void)
{
  m_butSetViewShift.EnableWindow(mTrulyLowDose && OKtoSetViewShift() && 
    !mWinApp->DoingTasks());
}

// Toggle rotation of axis
void CLowDoseDlg::OnLdRotateAxis()
{
  ConvertAxisPosition(false);
  UpdateData(true);
  ConvertAxisPosition(true);
  ManageAxisPosition();
  m_editAxisAngle.EnableWindow(m_bRotateAxis);
  m_statDegrees.EnableWindow(m_bRotateAxis);
  mWinApp->mMainView->DrawImage();
}

// A new axis angle is entered
void CLowDoseDlg::OnKillfocusEditAxisangle()
{
  ConvertAxisPosition(false);
  UpdateData(true);
  ConvertAxisPosition(true);
  ManageAxisPosition();
  mWinApp->mMainView->DrawImage();
}

// External call for modifying the axis position of T/F area and optionally, angle also
int CLowDoseDlg::NewAxisPosition(int area, double position, int angle, bool setAngle)
{
  if ((area + 1) / 2 != 1)
    return 1;
  if (mTrulyLowDose)
    ConvertAxisPosition(false);
  if (setAngle) {
    m_bRotateAxis = true;
    m_iAxisAngle = angle;
  }
  mLDParams[area].axisPosition = position;
  if (m_bTieFocusTrial)
    mLDParams[3 - area].axisPosition = position;
  ConvertAxisPosition(true);
  ManageAxisPosition();
  mWinApp->mMainView->DrawImage();
  return 0;
}


// DIALOG INITALIZATION
BOOL CLowDoseDlg::OnInitDialog() 
{
  CToolDlg::OnInitDialog();
  mLDParams = mWinApp->GetLowDoseParams();
  mImBufs = mWinApp->GetImBufs();
  mModeNames = mWinApp->GetModeNames();
  mShiftManager = mWinApp->mShiftManager;
  mScope = mWinApp->mScope;
  CString string;
  CRect rect;
  int abbrev = 3;

  m_statLDArea.GetClientRect(rect);
  mBigFont.CreateFont((rect.Height() - 2), 0, 0, 0, FW_HEAVY,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
     FF_DONTCARE, mBigFontName);
  m_statLDArea.SetFont(&mBigFont);

  // Set names
  SetDlgItemText(IDC_RSHOWSEARCH, mSearchName.Left(abbrev) + ".");
  SetDlgItemText(IDC_RSHOWVIEW, mModeNames[0].Left(abbrev) + ".");
  SetDlgItemText(IDC_RSHOWFOCUS, mModeNames[1].Left(abbrev) + ".");
  SetDlgItemText(IDC_RSHOWTRIAL, mModeNames[2].Left(abbrev) + ".");
  SetDlgItemText(IDC_RSHOWRECORD, mModeNames[3].Left(abbrev) + ".");
  m_butShowView.SetFont(mLittleFont);
  m_butShowFocus.SetFont(mLittleFont);
  m_butShowTrial.SetFont(mLittleFont);
  m_butShowRecord.SetFont(mLittleFont);
  m_butShowSearch.SetFont(mLittleFont);
  SetDlgItemText(IDC_COPYTOVIEW, mModeNames[0].Left(1));
  SetDlgItemText(IDC_COPYTOFOCUS, mModeNames[1].Left(1));
  SetDlgItemText(IDC_COPYTOTRIAL, mModeNames[2].Left(1));
  SetDlgItemText(IDC_COPYTORECORD, mModeNames[3].Left(1));
  SetDlgItemText(IDC_RDEFINEFOCUS, mModeNames[1]);
  SetDlgItemText(IDC_RDEFINETRIAL, mModeNames[2]);
  SetDlgItemText(IDC_SEARCH, mSearchName);
  ReplaceWindowText(&m_butTieFocusTrial, "Focus", mModeNames[1]);
  ReplaceWindowText(&m_butTieFocusTrial, "Trial", mModeNames[2]);
  ReplaceWindowText(&m_butNormalizeBeam, "View", mModeNames[0]);
  ReplaceWindowText(&m_statViewDefLabel, "View", mModeNames[0]);
  if (mScope->GetLDBeamTiltShifts())
    SetDlgItemText(IDC_STAT_ADD_SHIFT_BOX, "Additional beam shift (and tilt)");
  else if (FEIscope && mScope->GetPluginVersion() >= FEI_PLUGIN_DARK_FIELD)
    SetDlgItemText(IDC_STAT_ADD_SHIFT_BOX, "Additional beam shift (and DF tilt)");
  m_statMagSpot.EnableWindow(m_bContinuousUpdate);
  m_sbcViewDefocus.SetRange(0, 100);
  m_sbcViewDefocus.SetPos(50);

  // Assume all variables are set into this dialog
  if (m_iShowArea < 0)
    m_iShowArea = 0;
  
  if (mScope->GetJeol1230()) {
    m_bBlankWhenDown = false;
    m_butBlankBeam.EnableWindow(false);
    m_butUnblank.EnableWindow(false);
  }

  // We will not start program with this on!
  // mScope->SetLowDoseMode(mTrulyLowDose);

  m_iDefineArea = 0;

  UpdateSettings();
  mInitialized = true;  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

// Respond to possible external changes in down area, blank, or tie focus-trial
void CLowDoseDlg::UpdateSettings()
{
  float defocus= mScope->GetLDViewDefocus(m_iOffsetShown);

  // If defining an area, reject a change in show area
  if (mTrulyLowDose && m_iDefineArea)
    m_iShowArea = m_iDefineArea;

  mScope->SetBlankWhenDown(m_bBlankWhenDown);
  mScope->SetLDNormalizeBeam(m_bNormalizeBeam);
  if (defocus < sMinVSDefocus[m_iOffsetShown] || defocus > sMaxVSDefocus[m_iOffsetShown]){
    B3DCLAMP(defocus, sMinVSDefocus[m_iOffsetShown], sMaxVSDefocus[m_iOffsetShown]);
    mScope->SetLDViewDefocus(defocus, m_iOffsetShown ? SEARCH_AREA : VIEW_CONSET);
  }
  m_strViewDefocus.Format("%d", B3DNINT(defocus));

  // Synchronize dialog to state
  UpdateData(false);

  // Inform scope of down area unconditionally (8/10/15)
  mScope->SetLowDoseDownArea(m_iShowArea);

  // If tie focus-trial set and one of them is now displayed, equalize them
  int area = m_iDefineArea ? m_iDefineArea : mScope->GetLowDoseArea();
  if ((area + 1) / 2 != 1)
    area = 2;
  if (m_bTieFocusTrial)
    mLDParams[3 - area] = mLDParams[area];
  ManageAxisPosition();

  Update();
}

// Update enables based on state of system
void CLowDoseDlg::Update()
{
  BOOL bCentered = fabs(mLDParams[3].ISX) < 1.e-5 && fabs(mLDParams[3].ISY) < 1.e-5;
  BOOL defined = mLDParams[0].magIndex && mLDParams[1].magIndex && 
    mLDParams[2].magIndex && mLDParams[3].magIndex;
  BOOL STEMmode = mWinApp->GetSTEMMode();
  BOOL camBusy = mWinApp->mCamera->CameraBusy();
  BOOL usePiezo =  mScope->GetUsePiezoForLDaxis();
  ManageDefines(mScope->GetLowDoseArea());

  // Enable unblank button if blanked and no tasks, and camera not busy
  BOOL bEnable = mTrulyLowDose && !mWinApp->DoingTasks();
  m_butUnblank.EnableWindow(mLastBlanked && !mWinApp->DoingTasks() && !camBusy && 
    !mScope->GetJeol1230());
  m_butZeroViewShift.EnableWindow(!STEMmode && bEnable && !camBusy &&
    (mViewShiftX[m_iOffsetShown] != 0. || mViewShiftY[m_iOffsetShown] != 0.));

  // Disable copy buttons on same conditions, require an active area
  bEnable = bEnable && (mScope->GetLowDoseArea() >= 0);
  m_butCopyToView.EnableWindow(bEnable);
  m_butCopyToFocus.EnableWindow(bEnable);
  m_butCopyToTrial.EnableWindow(bEnable);
  m_butCopyToRecord.EnableWindow(bEnable);
  m_butSetBeamShift.EnableWindow(!STEMmode && bEnable && 
    mScope->GetLowDoseArea() != RECORD_CONSET);
  m_butResetBeamShift.EnableWindow(!STEMmode && bEnable && !camBusy);

  m_butSetViewShift.EnableWindow(!STEMmode && bEnable && OKtoSetViewShift());
  m_sbcViewDefocus.EnableWindow(!STEMmode && !mWinApp->DoingTasks() && !camBusy);
  m_statViewDefocus.EnableWindow(!STEMmode);

  // Make mag-spot line visible if mode on
  m_statMagSpot.ShowWindow(mTrulyLowDose ? SW_SHOW : SW_HIDE);
  m_statLDArea.ShowWindow(mTrulyLowDose ? SW_SHOW : SW_HIDE);
  m_statElecDose.ShowWindow(mTrulyLowDose ? SW_SHOW : SW_HIDE);
  m_statBeamShift.ShowWindow(mTrulyLowDose ? SW_SHOW : SW_HIDE);
   
  // Disable low dose, if tasks or TS; montaging compatibility no longer checked here
  // Disable continuous update, tying focus/trial and shifting center if tasks
  bEnable = !mWinApp->DoingTasks();
  m_butLowDoseMode.EnableWindow(bEnable && !mWinApp->DoingComplexTasks() &&
    !mScope->GetJeol1230() &&
    (!mWinApp->GetSTEMMode() || !mWinApp->DoSwitchSTEMwithScreen()));
  m_butContinuousUpdate.EnableWindow(bEnable);
  m_butNormalizeBeam.EnableWindow(!STEMmode && bEnable);
  m_butBalanceShifts.EnableWindow(!STEMmode && bEnable && mTrulyLowDose && defined && 
    bCentered && !usePiezo);
  m_butCenterUnshifted.EnableWindow(!STEMmode && bEnable && mTrulyLowDose && defined && 
    !bCentered && !usePiezo);
  m_butTieFocusTrial.EnableWindow(bEnable && !usePiezo);

  // Turn off define position if doing tasks
  if (mWinApp->DoingTasks() && m_iDefineArea > 0)
    TurnOffDefine();

  // Disable the position and overlap stuff if define area is off
  m_statPosition.EnableWindow(m_iDefineArea > 0);
  m_editPosition.EnableWindow(m_iDefineArea > 0);
  m_statMicron.EnableWindow(m_iDefineArea > 0);
  m_statOverlap.EnableWindow(m_iDefineArea > 0);
  m_butRotateAxis.EnableWindow(m_iDefineArea > 0 && !usePiezo);
  m_editAxisAngle.EnableWindow(m_iDefineArea > 0 && m_bRotateAxis);
  m_statDegrees.EnableWindow(m_iDefineArea > 0 && m_bRotateAxis);

  // Disable the show box when define area is on
  bEnable = !m_iDefineArea && !(STEMmode && mWinApp->DoDropScreenForSTEM());
  m_butShowSearch.EnableWindow(bEnable);
  m_butShowView.EnableWindow(bEnable);
  m_butShowFocus.EnableWindow(bEnable);
  m_butShowTrial.EnableWindow(bEnable);
  m_butShowRecord.EnableWindow(bEnable);

  UpdateData(false);
}

// Manages mag/spot area line, dose, and the beam shift display
void CLowDoseDlg::ManageMagSpot(int inSetArea, BOOL screenDown)
{
  int mag, spotSize;
  double intensity, dose, beamX, beamY;
  ScaleMat BStoIS, IStoBS, IStoSpec, BStoSpec;
  LowDoseParams *ldArea = &mLDParams[inSetArea];
  ControlSet *conSet = mWinApp->GetConSets() + inSetArea;
  BOOL needUpdate = false;
  CString C2units = mScope->GetC2Units();

  if (inSetArea < 0) {
    if (inSetArea != mLastSetArea) {
      m_strMagSpot = "Undefined";
      m_strLDArea = "";
      m_strElecDose = "";
      m_strBeamShift = "";
      UpdateData(false);
      mLastSetArea = inSetArea;
    }
    return;
  }
  
  mag = 0;
  if (ldArea->magIndex) {
    MagTable *magTab = mWinApp->GetMagTable() + ldArea->magIndex;
    if (mWinApp->GetEFTEMMode())
      mag = screenDown ? magTab->EFTEMscreenMag : magTab->EFTEMmag;
    else if (mWinApp->GetSTEMMode())
      mag = B3DNINT(magTab->STEMmag);
    else
      mag = screenDown ? magTab->screenMag : magTab->mag;
  }

  spotSize = ldArea->spotSize;
  intensity = ldArea->intensity;
  dose = 0.;
  if (inSetArea != SEARCH_AREA)
    dose = mWinApp->mBeamAssessor->GetElectronDose(spotSize, intensity, 
      mWinApp->mCamera->SpecimenBeamExposure(mWinApp->GetCurrentCamera(), conSet), 
      ldArea->probeMode);

  // Compute beam shift in specimen units if it is calibrated
  IStoBS = mShiftManager->GetBeamShiftCal(B3DMAX(1, ldArea->magIndex), 
    B3DNINT(ldArea->beamAlpha), B3DCHOICE(ldArea->probeMode < 0, 1, ldArea->probeMode));
  if (mWinApp->GetSTEMMode()) {
    if (m_strBeamShift != "") {
      m_strBeamShift = "";
      needUpdate = true;
    }
  } else {
    if (IStoBS.xpx && ldArea->magIndex) {
      BStoIS = mShiftManager->MatInv(IStoBS);
      IStoSpec = mShiftManager->IStoSpecimen(ldArea->magIndex);
      if (IStoSpec.xpx) {
        BStoSpec = mShiftManager->MatMul(BStoIS, IStoSpec);
        beamX = BStoSpec.xpx * ldArea->beamDelX + BStoSpec.xpy * ldArea->beamDelY;
        beamY = BStoSpec.ypx * ldArea->beamDelX + BStoSpec.ypy * ldArea->beamDelY;
        if (inSetArea != mLastSetArea || beamX != mLastBeamX || beamY != mLastBeamY) {
          m_strBeamShift.Format("%.2f, %.2f", beamX, beamY);
          needUpdate = true;
        }
        mLastBeamX = beamX;
        mLastBeamY = beamY;
      }
    } else if (inSetArea != mLastSetArea) {
      m_strBeamShift = "Uncalibrated";
      needUpdate = true;
    }
  }

  if (inSetArea != mLastSetArea)
    ManageDefines(inSetArea);

  if (mag != mLastMag || spotSize != mLastSpot || dose != mLastDose ||
    intensity != mLastIntensity || inSetArea != mLastSetArea) {
    m_strLDArea = (inSetArea < 4 ? mModeNames[inSetArea] : mSearchName) + ":";
    if (!mag && ldArea->camLenIndex)
      m_strMagSpot.Format("DIFF   Sp %d   %s %.2f%s", spotSize, mScope->GetC2Name(), 
        mScope->GetC2Percent(spotSize, intensity), (LPCTSTR)C2units);
    else {
      for (int silly = 0; silly < 2; silly++) {
        m_strMagSpot.Format("%d%sx   %s %d   %s %.2f%s", mag >= 100000 ? mag / 1000 : mag,
          mag >= 100000 ? "K" : "", 
          ldArea->probeMode == 0 && !mWinApp->GetSTEMMode() ? "nP" : "Sp", spotSize, 
          mScope->GetC2Name(), mScope->GetC2Percent(spotSize, intensity), 
          (LPCTSTR)C2units);
        if (silly || m_strMagSpot.GetLength() <= 24)
          break;
        C2units = C2units.Left(1);
      }
    }
    if (!dose)
      m_strElecDose = "";
    else if (dose < 1.0)
      m_strElecDose.Format("%.3f e/A2", dose);
    else if (dose < 10.0)
      m_strElecDose.Format("%.2f e/A2", dose);
    else
      m_strElecDose.Format("%.1f e/A2", dose);

    needUpdate = true;

    // This will catch initialization oddities, I hope; but don't do it when autocentering
    if ((inSetArea + 1) / 2 == 1 && m_bTieFocusTrial && 
      !mWinApp->mMultiTSTasks->GetAutoCentering())
      mLDParams[3 - inSetArea] = mLDParams[inSetArea];
  }
  if (needUpdate)
    UpdateData(false);
  mLastSpot = spotSize;
  mLastMag  = mag;
  mLastIntensity = intensity;
  mLastSetArea = inSetArea;
  mLastDose = dose;
}

// Enable the define boxes if mode is on and no tasks and not in search
// Allow changes during a tilt series
void CLowDoseDlg::ManageDefines(int area)
{
  BOOL bEnable = mTrulyLowDose && !mWinApp->DoingTasks() && area != SEARCH_AREA;
  m_butDefineNone.EnableWindow(bEnable);
  m_butDefineTrial.EnableWindow(bEnable);
  m_butDefineFocus.EnableWindow(bEnable);
}

// Regular update of scope parameters when in low dose mode
void CLowDoseDlg::ScopeUpdate(int magIndex, int spotSize, double intensity, 
                double ISX, double ISY, BOOL screenDown, int inSetArea, int camLenIndex,
                double focus, float alpha, int probeMode)
{
  ScaleMat cMat, cInv;
  LowDoseParams *ldArea;
  double specX, specY, tiltAxisX, newISX, newISY, baseTransX, baseTransY;
  int shiftedA;
  MultiShotParams *msParams;

  if (!mInitialized || !mTrulyLowDose)
    return;

  // If the area has changed, update the enables
  if (mLastSetArea != inSetArea)
    Update();
  
  // If continuous update and no tasks, update the text if anything has changed, and
  // modify the current set mode unconditionally
  // Disable changes in spectroscopy mode
  if (m_bContinuousUpdate && !mScope->GetChangingLDArea()  && !mScope->GetClippingIS() &&
    !mWinApp->DoingTasks() && inSetArea >= 0 && 
    !(JEOLscope && mScope->GetHasOmegaFilter() && mScope->mJeolSD.spectroscopy)) {
    ldArea = &mLDParams[inSetArea];
    TransferISonAxis(ldArea->magIndex, ldArea->ISX, ldArea->ISY, magIndex,
      ldArea->ISX, ldArea->ISY);
    ldArea->magIndex = magIndex;
    ldArea->spotSize = spotSize;
    ldArea->camLenIndex = camLenIndex;
    ldArea->diffFocus = focus;
    ldArea->beamAlpha = alpha;
    ldArea->probeMode = probeMode;
    if (inSetArea == RECORD_CONSET && mWinApp->mNavigator &&
      mWinApp->mNavigator->m_bShowAcquireArea &&mWinApp->mNavHelper->GetEnableMultiShot()
      && fabs(ldArea->intensity - intensity) > 1.e-6 && mScope->GetUseIllumAreaForC2() &&
      ((mWinApp->mMainView->GetActiveImBuf())->mHasUserPt || 
      (mWinApp->mMainView->GetActiveImBuf())->mHasUserLine)) {
        msParams = mWinApp->mNavHelper->GetMultiShotParams();
        ldArea->intensity = intensity;
        if (msParams->useIllumArea)
          mWinApp->mMainView->DrawImage();
    } else
      ldArea->intensity = intensity;

    if (mWinApp->GetFilterMode())
      SyncFilterSettings(inSetArea);

    // Keep trial and focus together?
    if ((inSetArea + 1) / 2 == 1 && m_bTieFocusTrial)
      mLDParams[3 - inSetArea] = mLDParams[inSetArea];
  }

  ManageMagSpot(inSetArea, screenDown);

  // If image shift has changed and we are defining an area, and screen is down and
  // we have not been instructed to ignore the next shift change, process change
  if (m_iDefineArea) {
    if ((ISX != mLastISX || ISY != mLastISY) && !mIgnoreIS && screenDown &&
      !mScope->GetClippingIS() && !mWinApp->GetSTEMMode() && 
      !mScope->GetUsePiezoForLDaxis()) {
      ldArea = &mLDParams[m_iDefineArea];
      SEMTrace('W', "ScopeUpdate setting axis, IS change %.4f, %.4f to %.4f, %.4f, "
        "ignore %d  screen %d", mLastISX, mLastISY, ISX, ISY, mIgnoreIS ? 1: 0, 
        screenDown ? 1: 0);
    
      // Convert the base image shift to the current mag
      // Get the implied shifted IS, convert to specimen and back to snap to axis
      TransferBaseIS(magIndex, baseTransX, baseTransY);
      cMat = mShiftManager->IStoSpecimen(magIndex);
      if (cMat.xpx) {
        cInv = mShiftManager->MatInv(cMat);
        specX = cMat.xpx * (ISX - baseTransX) + cMat.xpy * (ISY - baseTransY);
        specY = 0.;
        tiltAxisX = specX;
        if (m_bRotateAxis && m_iAxisAngle) {
          specY = cMat.ypx * (ISX - baseTransX) + cMat.ypy * (ISY - baseTransY);
          tiltAxisX = specX * cos(DTOR * m_iAxisAngle) + specY * sin(DTOR * m_iAxisAngle);
          specX = tiltAxisX * cos(DTOR * m_iAxisAngle);
          specY = tiltAxisX  * sin(DTOR * m_iAxisAngle);
        }
        newISX = cInv.xpx * specX + cInv.xpy * specY;
        newISY = cInv.ypx * specX + cInv.ypy * specY;

        // Add base back; make this be the image shift and set them as last values
        mLastISX = baseTransX + newISX;
        mLastISY = baseTransY + newISY;
        mScope->SetImageShift(mLastISX, mLastISY);
        mScope->GetImageShift(ISX, ISY);

        // If A has an image from defining area, change the shift of the image
        shiftedA = UsefulImageInA() > 0 ? 1 : 0;
        if (shiftedA)
          ShiftImageInA(specX - ldArea->axisPosition);

        // Set new mode shift values and set the user point
        ldArea->axisPosition = tiltAxisX;
        ConvertOneAxisToIS(ldArea->magIndex, tiltAxisX, ldArea->ISX, ldArea->ISY);
        if (m_bTieFocusTrial)
          mLDParams[3 - m_iDefineArea] = mLDParams[m_iDefineArea];
        FixUserPoint(shiftedA);
        ManageAxisPosition();
      }
    
    } else {
      CheckSeparationChange(magIndex);
    }
  } else if (m_bTieFocusTrial)
    CheckSeparationChange(magIndex);

  mLastISX = ISX;
  mLastISY = ISY;
  mLastMagIndex = magIndex;
  mIgnoreIS = false;
}

// Routine update about blanking status regardless of low dose mode
void CLowDoseDlg::BlankingUpdate(BOOL blanked)
{
  if (blanked && mLastBlanked || !blanked && !mLastBlanked)
    return;
  mLastBlanked = blanked;
  m_statBlanked.ShowWindow(blanked ? SW_SHOW : SW_HIDE);
  m_butUnblank.EnableWindow(blanked && !mWinApp->DoingTasks() && 
    !mWinApp->mCamera->CameraBusy() && !mScope->GetJeol1230());
}

// Process a change in alignment shift of image in buffer A
BOOL CLowDoseDlg::ImageAlignmentChange(float &newX, float &newY, 
                     float oldX, float oldY, BOOL mouseShifting)
{
  ScaleMat bInv;
  float shiftX, shiftY;
  double delISX, delISY;
  LowDoseParams *ldArea = &mLDParams[m_iDefineArea];
  
  if (!mTrulyLowDose || !m_iDefineArea || !UsefulImageInA())
    return false;
  bInv = mShiftManager->CameraToIS(mImBufs->mMagInd);
  if (!bInv.xpx)
    return false;
  shiftX = newX - oldX;
  shiftY = newY - oldY;
  
  // If this is image from define area, constrain shift to axis and revise the
  // alignment values
  if (UsefulImageInA() > 0) {
    SnapCameraShiftToAxis(shiftX, shiftY);
    newX = oldX + shiftX;
    newY = oldY + shiftY;
  }

  // Stop now if mouse shifting; it will complete shift when button goes up
  if (mouseShifting)
    return true;

  // Convert change to image shift - minus sign like in Shift Manager
  delISX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
  delISY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);

  // for image from define area, increment axis position and set the mode shift;
  // otherwise change the base after changing IS to match base mag
  if (UsefulImageInA() > 0) {
    ldArea->axisPosition += ConvertOneIStoAxis(mImBufs->mMagInd, delISX, delISY);
    ConvertOneAxisToIS(ldArea->magIndex, ldArea->axisPosition, ldArea->ISX, ldArea->ISY);
    if (m_bTieFocusTrial)
      mLDParams[3 - m_iDefineArea] = mLDParams[m_iDefineArea];
    ManageAxisPosition();
    SEMTrace('W', "ImageAlignmentChange setting axis IS");
  } else {
    TransferISonAxis(mImBufs->mMagInd, delISX, delISY, mBaseMag, delISX, delISY);
    mBaseISX += delISX;
    mBaseISY += delISY;
  }

  // Fix up user points and do not draw
  // Need to set the shifts now for the fix to work - could unset them but what's the point?
  mImBufs->mImage->setShifts(newX, newY);
  FixUserPoint(-1);
  mIgnoreIS = true;
  return true;
}

// If there is a change in user point, possibly snap to axis and affect shifts
void CLowDoseDlg::UserPointChange(float &ptX, float &ptY, EMimageBuffer *imBuf)
{
  ScaleMat bInv;
  float shiftX, shiftY, offsetX, offsetY;
  double delISX, delISY;
  int nx, ny, conSet;
  LowDoseParams *ldArea = &mLDParams[m_iDefineArea];
  
  if (!mTrulyLowDose || !m_iDefineArea || imBuf != mImBufs || !UsefulImageInA())
    return;
  bInv = mShiftManager->CameraToIS(mImBufs->mMagInd);
  if (!bInv.xpx)
    return;

  // Get offset from center of display area and snap it to axis; revise user point
  imBuf->mImage->getShifts(shiftX, shiftY);
  imBuf->mImage->getSize(nx, ny);
  offsetX = ptX + shiftX - nx / 2.f;
  offsetY = ptY + shiftY - ny / 2.f;
  SnapCameraShiftToAxis(offsetX, offsetY);
  ptX = offsetX + nx / 2.f - shiftX;
  ptY = offsetY + ny / 2.f - shiftY;

  // Convert to a change in image shift and use that to modify the mode shift
  conSet = mWinApp->mCamera->ConSetToLDArea(imBuf->mConSetUsed);
  delISX = imBuf->mBinning * (bInv.xpx * offsetX - bInv.xpy * offsetY);
  delISY = imBuf->mBinning * (bInv.ypx * offsetX - bInv.ypy * offsetY);

  ldArea->axisPosition = mLDParams[conSet].axisPosition +
    ConvertOneIStoAxis(imBuf->mMagInd, delISX, delISY);
  ConvertOneAxisToIS(ldArea->magIndex, ldArea->axisPosition, ldArea->ISX, ldArea->ISY);
  if (m_bTieFocusTrial)
    mLDParams[3 - m_iDefineArea] = mLDParams[m_iDefineArea];

  /*
  CString message;
  double curISX, curISY;
  mScope->GetImageShift(curISX, curISY);
  double trouble = mLDParams[m_iDefineArea].ISY;
  message.Format("cur %d def %d  was %.2f, %.2f  delISXY %.2f, %.2f  "
    "cur %.2f, %.2f  define %.2f, %.2f", conSet, m_iDefineArea, curISX,
    curISY, delISX, delISY, mLDParams[conSet].ISX, mLDParams[conSet].ISY, mLDParams[m_iDefineArea].ISX, trouble);
  mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
  */

  // If displaying the define area, shift this point to center
  if (UsefulImageInA() > 0) {
    imBuf->mImage->setShifts(shiftX - offsetX, shiftY - offsetY);
    imBuf->SetImageChanged(1);
  }

  // Actualize shift if defined area is currently shifted area
  ApplyNewISifDefineArea();
  ManageAxisPosition();
  SEMTrace('W', "UserPointChange setting axis IS");
}

// Need to keep window from closing and take focus away from edit box
void CLowDoseDlg::OnOK()
{
  SetFocus();
}

// The user has entered a value for position in the edit box
void CLowDoseDlg::OnKillfocusEditposition() 
{
  double newAxis, specX, maxRange;
  int shiftedA;
  LowDoseParams *ldArea = &mLDParams[m_iDefineArea];

  UpdateData(true);
  mWinApp->RestoreViewFocus();
  if (!mTrulyLowDose || !m_iDefineArea)
    return;

  // Get the new axis position
  specX = atof((LPCTSTR)m_strPosition);
  if (mScope->GetUsePiezoForLDaxis()) {
    maxRange =  mPiezoMaxToUse - mPiezoMinToUse;
    B3DCLAMP(specX, -maxRange, maxRange);
  }
  newAxis = specX + mLDParams[3].axisPosition;
  
  // If define area is in buffer A, shift it by the change in mode shift
  shiftedA = UsefulImageInA() > 0 ? 1 : 0;
  if (shiftedA) 
    ShiftImageInA(newAxis - ldArea->axisPosition);

  // Set the axis position and image shift
  ldArea->axisPosition = newAxis;
  ConvertOneAxisToIS(ldArea->magIndex, newAxis, ldArea->ISX, ldArea->ISY);
  if (m_bTieFocusTrial)
    mLDParams[3 - m_iDefineArea] = mLDParams[m_iDefineArea];


  // Output new image shift if the defined area is current
  ApplyNewISifDefineArea();
  FixUserPoint(shiftedA);
  ManageAxisPosition();
  SEMTrace('W', "OnKillfocusEditposition setting axis IS");
}


// Actualize a new axis shift if defined area is currently shifted area; or always update
// the piezo position in case axis flips
void CLowDoseDlg::ApplyNewISifDefineArea(void)
{
  double delISX, delISY;
  LowDoseParams *ldArea = &mLDParams[m_iDefineArea];
  int curArea = mScope->GetLowDoseArea();
  if (mScope->GetUsePiezoForLDaxis()) {
    GoToPiezoPosForLDarea(curArea);
  } else if (curArea == m_iDefineArea || (m_bTieFocusTrial && 
    curArea == 3 - m_iDefineArea)) {
      TransferBaseIS(ldArea->magIndex, delISX, delISY);
      mScope->SetImageShift(ldArea->ISX + delISX, ldArea->ISY + delISY);
      mIgnoreIS = true;
  }
}

// Add a point to a new image if defining areas
void CLowDoseDlg::AddDefineAreaPoint()
{
  if (!m_iDefineArea)
    return;
  mImBufs->mHasUserPt = true;
  FixUserPoint(-1);
}

// Redisplay the user point at center of define area if there is one, or at correct point
// in the view area
void CLowDoseDlg::FixUserPoint(int needDraw)
{
  ScaleMat aMat;
  float shiftX, shiftY, offsetX, offsetY;
  int nx, ny, conSet;
  double delAxis, delX, delY;

  if (mImBufs->mHasUserPt && UsefulImageInA()) {
    mImBufs->mImage->getShifts(shiftX, shiftY);
    mImBufs->mImage->getSize(nx, ny);
    if (UsefulImageInA() > 0) {

      // If defining area is shown, put point in the middle of display
      offsetX = 0;
      offsetY = 0;
    } else {

      // for any other area, find location of point relative to center of that area
      aMat = mShiftManager->SpecimenToCamera(mWinApp->GetCurrentCamera(),
        mImBufs->mMagInd);
      if (!aMat.xpx)
        return;
      conSet = mWinApp->mCamera->ConSetToLDArea(mImBufs->mConSetUsed);
      delAxis = mLDParams[m_iDefineArea].axisPosition - mLDParams[conSet].axisPosition;
      delX = delAxis;
      delY = 0.;
      if (m_bRotateAxis && m_iAxisAngle) {
        delX = delAxis * cos(DTOR * m_iAxisAngle);
        delY = delAxis * sin(DTOR * m_iAxisAngle);
      }
      offsetX = (float)(aMat.xpx * delX + aMat.xpy * delY) / mImBufs->mBinning;
      offsetY = (float)(aMat.ypx * delX + aMat.ypy * delY) / mImBufs->mBinning;
    }
    mImBufs->mUserPtX = offsetX + nx / 2.f - shiftX;
    mImBufs->mUserPtY = -offsetY + ny / 2.f - shiftY;
    if (!needDraw) 
      needDraw = 1;
  } 

  // Draw if set a point or if asked to by caller, but not if asked not to (-1)
  if (needDraw > 0)
    mWinApp->SetCurrentBuffer(0);
}

// return 1 if A has an image from defining area, or -1 if it is from another area
int CLowDoseDlg::UsefulImageInA()
{
  int conSet = mWinApp->mCamera->ConSetToLDArea(mImBufs->mConSetUsed);
  if (!mImBufs->mBinning || !mImBufs->mCaptured || !mImBufs->mMagInd)
    return 0;
  if (conSet == m_iDefineArea || (m_bTieFocusTrial &&
    conSet == 3 - m_iDefineArea))
    return 1;
  return -1;
}

// Given a shift on camera, get the X coordinate on specimen and convert that
// back to a shift on the camera along the tilt axis
void CLowDoseDlg::SnapCameraShiftToAxis(float &shiftX, float &shiftY)
{
  ScaleMat aMat, aInv;
  double specX, specY, tiltAxisX, maxRange;

  aMat = mShiftManager->SpecimenToCamera(mWinApp->GetCurrentCamera(),
    mImBufs->mMagInd);
  if (!aMat.xpx)
    return;
  aInv = mShiftManager->MatInv(aMat);
  
  // We can ignore the binning.  But we do need to invert Y going in and out
  specX = aInv.xpx * shiftX - aInv.xpy * shiftY;
  if (mScope->GetUsePiezoForLDaxis()) {
    maxRange =  mPiezoMaxToUse - mPiezoMinToUse;
    B3DCLAMP(specX, -maxRange, maxRange);
  }
  specY = 0.;
  if (m_bRotateAxis && m_iAxisAngle) {
    specY = aInv.ypx * shiftX - aInv.ypy * shiftY;
    tiltAxisX = specX * cos(DTOR * m_iAxisAngle) + specY * sin(DTOR * m_iAxisAngle);
    specX = tiltAxisX * cos(DTOR * m_iAxisAngle);
    specY = tiltAxisX * sin(DTOR * m_iAxisAngle);
  }
  shiftX = (float)(aMat.xpx * specX + aMat.xpy * specY);
  shiftY = -(float)(aMat.ypx * specX + aMat.ypy * specY);
}

// Update the output to the position edit box and separation message
void CLowDoseDlg::ManageAxisPosition()
{
  int recSize, defSize;
  double angle, specX;
  float recUm, defUm, cenX, cenY, rh, sep1, sep2;
  int curCam = mWinApp->GetCurrentCamera();
  int area = m_iDefineArea ? m_iDefineArea : 2;

  if (!m_iDefineArea && !m_bTieFocusTrial) {
    m_strPosition = "";
    m_strOverlap = "";
    UpdateData(false);
    return;
  }
 
  // Axis positions are supposed to be completely up to date as image shift changes
  specX = mLDParams[area].axisPosition - mLDParams[3].axisPosition;
  m_strPosition.Format("%.2f", specX);

  // Update the separation and save the last size values
  GetAreaSizes(recSize, defSize, angle);
  if (!recSize || !mLDParams[3].magIndex || !mLDParams[area].magIndex)
    return;
  recUm = (float)(recSize * mShiftManager->GetPixelSize(curCam, mLDParams[3].magIndex));
  defUm = (float)(defSize * mShiftManager->GetPixelSize(curCam, 
    mLDParams[area].magIndex));
  rh = recUm / 2.f;
  cenX = (float)(fabs(specX) * cos(DTOR * angle));
  cenY = (float)(fabs(specX) * sin(DTOR * angle));
  if (cenX < rh && cenY < rh)
    cenX = cenY = rh;
  sep1 = mWinApp->mNavHelper->PointSegmentDistance(cenX, cenY, rh, -rh, rh, rh);
  sep2 = mWinApp->mNavHelper->PointSegmentDistance(cenX, cenY, -rh, rh, rh, rh); 
  double maxSep = B3DMIN(sep1, sep2) - sqrt(2.) * defUm / 2.;
  m_strOverlap.Format("Maximum area separation:  %.2f um", maxSep);
  UpdateData(false);
  mLastRecordSize = recSize;
  mLastDefineSize = defSize;
}

// Compute the minimum separation between Record and area being defined
// under the assumption that the spot circumscribing the defined area
// does not overlap the Record area
void CLowDoseDlg::GetAreaSizes(int &recSize, int &defSize, double &angle)
{
  int tmp;
  ControlSet *conSets = mWinApp->GetConSets();
  int area = m_iDefineArea ? m_iDefineArea : 2;
  angle = mShiftManager->GetImageRotation(mWinApp->GetCurrentCamera(),
    mLDParams[3].magIndex);
  if (m_bRotateAxis)
    angle += m_iAxisAngle;
  
  while (angle < -90.)
    angle += 180.;
  while (angle > 90.)
    angle -= 180.;

  // Use the specific axis of the record area along the tilt axis
  angle = fabs(angle);
  if (angle < 45.)
    recSize = conSets[3].right - conSets[3].left;
  else {
    recSize = conSets[3].bottom - conSets[3].top;
    angle = 90. - angle;
  }

  // but take the largest axis of trial/focus
  defSize = conSets[area].right - conSets[m_iDefineArea].left;
  tmp = conSets[area].bottom - conSets[area].top;
  if (defSize < tmp)
    defSize = tmp;
  if (m_bTieFocusTrial) {
    tmp = conSets[3 - area].right - conSets[3 - area].left;
    if (defSize < tmp)
      defSize = tmp;
    tmp = conSets[3 - area].bottom - conSets[3 - area].top;
    if (defSize < tmp)
      defSize = tmp;
  }
}

// Check that size of areas or mag hasn't changed, update position if so
void CLowDoseDlg::CheckSeparationChange(int magIndex)
{
  int recSize, defSize;
  double angle;
  GetAreaSizes(recSize, defSize, angle);
  if (recSize != mLastRecordSize || defSize != mLastDefineSize ||
    magIndex != mLastMagIndex) {
    SEMTrace('W', "ScopeUpdate setting axis, rec %d to %d, def %d to %d, mag %d to"
      " %d", mLastRecordSize, recSize, mLastDefineSize, defSize, mLastMagIndex, 
      magIndex);
    ManageAxisPosition();
  }
  mLastRecordSize = recSize;
  mLastDefineSize = defSize;
}

// Shift the image in A to correspond to the given change in image
void CLowDoseDlg::ShiftImageInA(double delAxis)
{
  ScaleMat aMat;
  float shiftX, shiftY;
  double delX = delAxis, delY = 0.;
  int nx, ny;

  mImBufs->mImage->getShifts(shiftX, shiftY);
  mImBufs->mImage->getSize(nx, ny);
  aMat = mShiftManager->SpecimenToCamera(mImBufs->mCamera, mImBufs->mMagInd);
  if (!aMat.xpx)
    return;
  if (m_bRotateAxis && m_iAxisAngle) {
    delX = delAxis * cos(DTOR * m_iAxisAngle);
    delY = delAxis * sin(DTOR * m_iAxisAngle);
  }
  shiftX -= (float)(aMat.xpx * delX + aMat.xpy * delY) / mImBufs->mBinning;
  shiftY += (float)(aMat.ypx * delX + aMat.ypy * delY) / mImBufs->mBinning;
  // It seems to be OK to let shifts change a lot
  /* if (shiftX < 4 - nx)
  shiftX = 4.f - nx;
  if (shiftX > nx - 4)
  shiftX = nx - 4.f;
  if (shiftY < 4 - ny)
  shiftY = 4.f - ny;
  if (shiftY > ny - 4)
  shiftY = ny - 4.f; */
  mImBufs->mImage->setShifts(shiftX, shiftY);
  mImBufs->SetImageChanged(1);
}

// Set the filter parameters in filtParam into the given low dose area
// If filtParam is NULL or omitted the master set is used
void CLowDoseDlg::SyncFilterSettings(int inArea, FilterParams *filtParam)
{
  LowDoseParams *ldArea = &mLDParams[inArea];

  if (!filtParam)
    filtParam = mWinApp->GetFilterParams();
  ldArea->slitIn = filtParam->slitIn;
  ldArea->slitWidth = filtParam->slitWidth;
  ldArea->energyLoss = filtParam->energyLoss;
  ldArea->zeroLoss = filtParam->zeroLoss;
}

// Compute corner points for drawing an area on a view image, if an area is
// being defined.  type = 0 for the area, 1 for Record.  Binning and sizeX, sizeY
// describe the View image.  Four corner points in image coordinates are returned
// in corner[XY], and for the define area, 
int CLowDoseDlg::DrawAreaOnView(int type, EMimageBuffer *imBuf, 
                                float * cornerX, float * cornerY,
                                float & cenX, float & cenY, float &radius)
{
  int binning = imBuf->mBinning;
  int sizeX = imBuf->mImage->getWidth();
  int sizeY = imBuf->mImage->getHeight();
  int curCam = mWinApp->GetCurrentCamera();
  ControlSet *conSets = mWinApp->GetConSets();
  ControlSet *csp;
  ScaleMat aMat, vMat;
  int boxArea, cenArea, delX, delY, diamSq;
  float scale, rotation, pixel;
  if (!m_iDefineArea || !mTrulyLowDose)
    return 0;
  vMat = mShiftManager->SpecimenToCamera(curCam, mLDParams[0].magIndex);
  mShiftManager->GetScaleAndRotationForFocus(imBuf, scale, rotation);
  vMat = mShiftManager->MatScaleRotate(vMat, scale, rotation);

  // First fill the corner array with the actual define area
  boxArea = type ? RECORD_CONSET : m_iDefineArea;
  csp = &conSets[boxArea];
  aMat = mShiftManager->CameraToSpecimen(mLDParams[boxArea].magIndex);
  if (!aMat.xpx || !vMat.xpx)
    return 0;
  AreaAcqCoordToView(boxArea, binning, sizeX, sizeY, aMat, vMat, 
    csp->left, csp->top, cornerX[0], cornerY[0]);
  AreaAcqCoordToView(boxArea, binning, sizeX, sizeY, aMat, vMat, 
    csp->right, csp->top, cornerX[1], cornerY[1]);
  AreaAcqCoordToView(boxArea, binning, sizeX, sizeY, aMat, vMat, 
    csp->right, csp->bottom, cornerX[2], cornerY[2]);
  AreaAcqCoordToView(boxArea, binning, sizeX, sizeY, aMat, vMat, 
    csp->left, csp->bottom, cornerX[3], cornerY[3]);

  if (type)
    return boxArea;
  
  // Get squared diameter of area for radius
  cenArea = m_iDefineArea;
  delX = csp->right - csp->left;
  delY = csp->bottom - csp->top;
  diamSq = delX * delX + delY * delY;

  // If focus/trial tied, find which one has bigger radius
  if (m_bTieFocusTrial) {
    delX = conSets[3 - m_iDefineArea].right - conSets[3 - m_iDefineArea].left;
    delY = conSets[3 - m_iDefineArea].bottom - conSets[3 - m_iDefineArea].top;
    if (delX * delX + delY * delY > diamSq) {
      cenArea = 3 - m_iDefineArea;
      diamSq = delX * delX + delY * delY;
      csp = &conSets[cenArea];
      aMat = mShiftManager->CameraToSpecimen(mLDParams[cenArea].magIndex);
      if (!aMat.xpx)
        return 0;
    }
  }

  // Get radius in image coordinates and convert center to view image coord
  pixel = mShiftManager->GetPixelSize(imBuf);
  if (!pixel)
    pixel = binning * mShiftManager->GetPixelSize(curCam, mLDParams[0].magIndex);
  radius = (float)(sqrt(0.25 * diamSq) * 
    mShiftManager->GetPixelSize(curCam, mLDParams[cenArea].magIndex) / pixel);
  AreaAcqCoordToView(cenArea, binning, sizeX, sizeY, aMat, vMat, 
    (csp->left + csp->right) / 2, (csp->bottom + csp->top) / 2, cenX, cenY);

  return boxArea;
}

// Convert from camera acquisition coordinates (inverted Y) in the given inArea to
// image coordinates in the View image with given sizeX, sizeY and binning
void CLowDoseDlg::AreaAcqCoordToView(int inArea, int binning, int sizeX, int sizeY,
                                     ScaleMat aMat, ScaleMat vMat, int acqX, int acqY,
                                     float & imX, float & imY)
{
  CameraParameters *camP = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  double camX, camY, specX, specY, delX, delY;

  // Convert to centered camera coordinates
  camX = acqX - camP->sizeX / 2.;
  camY = camP->sizeY / 2. - acqY;

  // Convert to specimen coordinates and add difference of axis positions
  delX = mLDParams[inArea].axisPosition - mLDParams[0].axisPosition;
  delY = 0.;
  if (m_bRotateAxis && m_iAxisAngle) {
    delY = delX * sin(DTOR * m_iAxisAngle);
    delX = delX * cos(DTOR * m_iAxisAngle);
  }
  specX = aMat.xpx * camX + aMat.xpy * camY + delX;
  specY = aMat.ypx * camX + aMat.ypy * camY + delY;

  // Convert to centered view camera coordinates
  camX = vMat.xpx * specX + vMat.xpy * specY;
  camY = vMat.ypx * specX + vMat.ypy * specY;

  // Convert to image coordinates in the actual view image
  imX = (float)(camX / binning + sizeX / 2.);
  imY = (float)(sizeY / 2. - camY / binning);
}

bool CLowDoseDlg::SameAsFocusArea(int inArea)
{
  return inArea == FOCUS_CONSET || (m_bTieFocusTrial && inArea == TRIAL_CONSET);
}

void CLowDoseDlg::CheckAndActivatePiezoShift(void)
{
  PiezoScaling scaling;
  CString mess;
  float minUse, maxUse, backoff, range, backoffFrac = 0.02f;

  // Look up a scaling and turn off if there is not one
  if (!mWinApp->mPiezoControl->LookupScaling(mAxisPiezoPlugNum, 
    mAxisPiezoNum, true, scaling)) {
      mess.Format("A PiezoScaling property must be entered for piezo # %d%s\n"
        "in order to use it for on-axis shift in Low Dose\n\n"
        "On-axis shift with a piezo has been turned off", mAxisPiezoNum,
        mWinApp->mPiezoControl->DrivenByMess(mAxisPiezoPlugNum));
    SEMMessageBox(mess);
    mScope->SetUsePiezoForLDaxis(false);
    return;
  }

  // Compute minimum and maximum values to use, avoiding end of range
  mPiezoScaleFac = scaling.unitsPerMicron;
  backoff = backoffFrac * (scaling.maxPos - scaling.minPos);
  minUse = (scaling.minPos + backoff) / mPiezoScaleFac;
  maxUse = (scaling.maxPos - backoff) / mPiezoScaleFac;
  mPiezoMinToUse = B3DMIN(minUse, maxUse);
  mPiezoMaxToUse = B3DMAX(minUse, maxUse);
  range = mPiezoMaxToUse - mPiezoMinToUse;

  if (fabs(mLDParams[3].axisPosition) > 1.e-5) {
    OnCenterunshifted();
    PrintfToLog("Turned off Balanced Shifts");
  }

  if (!m_bTieFocusTrial) {
    m_bTieFocusTrial = true;
    mLDParams[1] = mLDParams[2];
    ManageAxisPosition();
    PrintfToLog("Tied Focus and Trial together (copied Trial to Focus)");
    UpdateData(false);
  }

  ConvertAxisPosition(false);
  if (fabs(mLDParams[2].axisPosition) > range) {
    B3DCLAMP(mLDParams[2].axisPosition, -range, range);
    mLDParams[1].axisPosition = mLDParams[2].axisPosition;
    ConvertAxisPosition(true);
    ManageAxisPosition();
    PrintfToLog("Truncated axis offset to maximum range for piezo, %.2f micron", range);
    UpdateData(false);
  }

  if (m_bRotateAxis) {
    m_bRotateAxis = false;
    UpdateData(false);
    OnLdRotateAxis();
    PrintfToLog("Turned off axis rotation relative to tilt axis");
  }

  // Go to the end of range for the current axis sign
  mPiezoCurrentSign = 0;
  if (GoToPiezoPosForLDarea(3)) {
    mScope->SetUsePiezoForLDaxis(false);
    SEMMessageBox("On-axis shift with a piezo has been turned off");
    return;
  }
}


// Go to the piezo position for the given area
bool CLowDoseDlg::GoToPiezoPosForLDarea(int area)
{
  double oldBase, newBase, currentZ, movement, offset = 0.;
  float delay;
  ScaleMat dMat;
  StageMoveInfo smi;
  CString mess;

  // Get the offset for trial/focus area
  ConvertAxisPosition(false);
  if (area == 1 || area == 2)
    offset = mLDParams[2].axisPosition;

  // Select in case have to get the position
  if (mWinApp->mPiezoControl->SelectPiezo(mAxisPiezoPlugNum, mAxisPiezoNum) || 
    mWinApp->mPiezoControl->GetZPosition(currentZ))
    return true;

  // Get the old and new base position
  if (mPiezoCurrentSign) {
    oldBase = mPiezoCurrentSign > 0 ? mPiezoMinToUse : mPiezoMaxToUse;
    mess = "WARNING: Moved stage to compensate for change in piezo base position"
      "\r\n   caused by change in sign of Trial/Focus position";
  } else {
    oldBase = currentZ;
    mess = "Moved stage to compensate for moving piezo to base position";
  }
  mPiezoCurrentSign = mLDParams[2].axisPosition >= 0 ? 1 : -1;
  newBase = mPiezoCurrentSign > 0 ? mPiezoMinToUse : mPiezoMaxToUse;
  movement = fabs(currentZ - (newBase + offset));
  if (movement < 1.e-4)
    return false;

  // Set position
  if (mWinApp->mPiezoControl->SetZPosition(newBase + offset))
    return true;
  if (mWinApp->mPiezoControl->WaitForPiezoReady(20000)) {
    SEMMessageBox("Timeout waiting for piezo movement for on-axis shift to complete");
    return true;
  }

  // Set a timeout that depends on distance moved and scaled down for non-records
  delay = (float)(mPiezoFullDelay * movement / (mPiezoMaxToUse - mPiezoMinToUse));
  if (area == FOCUS_CONSET)
    delay *= mFocusPiezoDelayFac;
  else if (area != RECORD_CONSET)
    delay *= mTVPiezoDelayFac;
  mShiftManager->SetGeneralTimeOut(GetTickCount(), (int)(1000. * delay));

  // If the base has changed, move stage to compensate and issue message
  if (mWinApp->mPiezoControl->GetLastMovementError())
    return true;
  if (fabs(newBase - oldBase) > 1.e-2) {
    dMat = mShiftManager->SpecimenToStage(1., 1.);
    mScope->GetStagePosition(smi.x, smi.y, smi.z);
    smi.x -= (newBase - oldBase) * dMat.xpx;
    smi.y -= (newBase - oldBase) * dMat.ypx;
    smi.axisBits = axisXY;
    mScope->MoveStage(smi);
    mWinApp->AppendToLog(mess);
    mScope->WaitForStageReady(20000);
  }
  return false;
}
