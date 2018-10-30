// ComplexTasks.cpp      Performs tasks invvolving multiple image acquisitions and
//                         stage movement: finding eucentricity, resetting image
//                         shift, reversing tilt direction and walking up.
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include ".\ComplexTasks.h"
#include "MultiTSTasks.h"
#include "ShiftManager.h"
#include "FocusManager.h"
#include "CameraController.h"
#include "BeamAssessor.h"
#include "TSController.h"
#include "NavigatorDlg.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define RSRA_FIRST_SHOT  1
#define RSRA_RESETTING   2
#define RSRA_SECOND_SHOT 3
#define WALKUP_IMAGE     1
#define WALKUP_RESET     2
#define WALKUP_TILTED    3
#define WALKUP_REVERSED  4
#define RT_FIRST_SHOT    1
#define RT_FIRST_TILT    2
#define RT_SECOND_TILT   3
#define RT_SECOND_SHOT   4
#define FE_COARSE_RESET  1
#define FE_COARSE_MOVED  2
#define FE_COARSE_SHOT   3
#define FE_COARSE_LAST_MOVE 4
#define FE_FINE_RESET    5
#define FE_FINE_TILTED   6
#define FE_FINE_SHOT     7
#define FE_FINE_LAST_MOVE   8
#define FE_FINE_ALIGNSHOT1  9
#define FE_FINE_ALIGNTILT  10
#define FE_FINE_ALIGNSHOT2 11
enum {TASM_FIRST_SHOT, TASM_TILTED, TASM_SECOND_SHOT};
enum {BASP_FIRST_SHOT, BASP_MOVED, BASP_SECOND_SHOT};

BEGIN_MESSAGE_MAP(CComplexTasks, CCmdTarget)
  //{{AFX_MSG_MAP(CComplexTasks)
  ON_COMMAND(ID_REVERSE_TILT, OnReverseTilt)
  ON_COMMAND(ID_TASKS_WALKUP, OnWalkup)
  ON_COMMAND(ID_RESET_REALIGN, OnResetRealign)
  ON_COMMAND(ID_TASKS_FINEREALIGN, OnTasksFinerealign)
  ON_COMMAND(ID_TASKS_VERBOSE, OnTasksVerbose)
  ON_UPDATE_COMMAND_UI(ID_TASKS_VERBOSE, OnUpdateTasksVerbose)
	ON_COMMAND(ID_TASKS_WALKUPANCHOR, OnTasksWalkupanchor)
  ON_UPDATE_COMMAND_UI(ID_TASKS_WALKUP, OnUpdateNoTasksNoTS)
  ON_UPDATE_COMMAND_UI(ID_RESET_REALIGN, OnUpdateNoTasks)
	ON_COMMAND(ID_TASKS_SETTILTAXISOFFSET, OnTasksSettiltaxisoffset)
	ON_COMMAND(ID_TASKS_SETITERATIONLIMIT, OnTasksSetiterationlimit)
	ON_COMMAND(ID_TASKS_USETRIALINLD, OnTasksUsetrialinld)
  ON_UPDATE_COMMAND_UI(ID_TASKS_FINEREALIGN, OnUpdateNoTasksNoTSNoHitachi)
  ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_BOTH, OnUpdateNoTasksNoTSNoHitachi)
	ON_UPDATE_COMMAND_UI(ID_TASKS_WALKUPANCHOR, OnUpdateNoTasksNoTS)
	ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_BOTH, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_COARSE, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_FINE, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_REVERSE_TILT, OnUpdateNoTasksNoTS)
	ON_UPDATE_COMMAND_UI(ID_TASKS_SETTILTAXISOFFSET, OnUpdateNoTasksNoTS)
	ON_UPDATE_COMMAND_UI(ID_FOCUS_RESETDEFOCUS, OnUpdateNoTasksNoTS)
	ON_UPDATE_COMMAND_UI(ID_TASKS_USETRIALINLD, OnUpdateTasksUsetrialinld)
  ON_COMMAND(ID_TASKS_SETINCREMENTS, OnTasksSetincrements)
	ON_UPDATE_COMMAND_UI(ID_TASKS_SETINCREMENTS, OnUpdateNoTasks)
	//}}AFX_MSG_MAP
  ON_COMMAND_RANGE(ID_EUCENTRICITY_COARSE, ID_EUCENTRICITY_BOTH, OnEucentricity)
  ON_UPDATE_COMMAND_UI_RANGE(ID_EUCENTRICITY_COARSE, ID_EUCENTRICITY_FINE, OnUpdateNoTasksNoTS)
  ON_COMMAND(ID_TASKS_TRIAL_IN_LD_REFINE, OnTrialInLdRefine)
  ON_UPDATE_COMMAND_UI(ID_TASKS_TRIAL_IN_LD_REFINE, OnUpdateTrialInLdRefine)
  ON_COMMAND(ID_TASKS_USE_VIEW_IN_LOWDOSE, OnTasksUseViewInLowdose)
  ON_UPDATE_COMMAND_UI(ID_TASKS_USE_VIEW_IN_LOWDOSE, OnUpdateTasksUseViewInLowdose)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CComplexTasks::CComplexTasks()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConSets = mWinApp->GetConSets();
  mImBufs = mWinApp->GetImBufs();
  mMagStackInd = 0;
  mVerbose = false;
  mTotalDose = 0.;
  mOnAxisDose = 0.;
  mMinLMSlitWidth = 25;
  mSlitSafetyFactor = 0.75f;
  mRSRAHigherMagCriterion = 1.7f;
  mDoingRSRA = false;
  mMaxRSRAIterations = 2;
  mRSRACriterion = 0.5;
  mRSRAUserCriterion = -1.;
  mRSRAUseTrialInLDMode = false;
  mRSRAWarnedUseTrial = false;

  mDoingTASM = false;
  mDoingBASP = 0;

  mWalkShiftLimit = 2.0;
  mWULowDoseISLimit = 1.3f;
  mMinWalkIntDflt = 3.;
  mMaxWalkIntDflt = 8.;
  mMinWalkInterval = mMinWalkIntDflt;
  mMaxWalkInterval = mMaxWalkIntDflt;
  mWalkIndex = -1;
  mWalkAngles = NULL;
  mWalkTarget = -60.;
  mWalkSTEMfocusInterval = 19.;

  mReversingTilt = false;
  mRTThreshold = 3.;
  mDoingEucentricity = false;
  mFEBacklashZ = -3.;
  mFEInitialAngle = -5.;
  mFEInitialIncrement = 0.6;  // sin(0.6) = 0.01
  mFEResetISThreshold = 0.1;
  mFEMaxTilt = 10.;
  mFEMaxIncrement = 8.;
  mFETargetShift = 2.;
  mFEMaxIncrementChange = 3.;
  mFENumFineSteps = 8;
  mFETargetAngles[0] = 24.;
  mFETargetAngles[1] = 18.;
  mFETargetAngles[2] = 11.;
  mFETargetAngles[3] = 4.;
  mFETargetAngles[4] = -4.;
  mFETargetAngles[5] = -11.;
  mFETargetAngles[6] = -18.;
  mFETargetAngles[7] = -24.;
  mFEIterationLimit = 3;
  mFEMaxFineIS = 2.;
  mLastAxisOffset = -999.;
  mFEUseTrialInLD = false;
  mFEWarnedUseTrial = false;
  mZMicronsPerDialMark = 3.1f;
  mManualHitachiBacklash = 10.;
  mWalkUseViewInLD = false;
  mSkipNextBeamShift = false;
  mStageTimeoutFactor = 1.;
  
  // Default minimum field of view
  mMinRSRAField = 7.0;
  mMinRTField = 4.5f;
  mMinFECoarseField = 9.;
  mMinFEFineField = 4.5f;
  mMinFEFineAlignField = 9.;
  mMinWalkField = 1.5;
  mMinTASMField = -1.;     // Defer scope-dependent initialization
  mMinBASPField = 6.0f;
  mMinTaskExposure = 0.002f;
}

CComplexTasks::~CComplexTasks()
{

}

void CComplexTasks::Initialize()
{
  HitachiParams *hParams = mWinApp->GetHitachiParams();
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  mTSController = mWinApp->mTSController;
  if (mMinTASMField < 0.)
    mMinTASMField = JEOLscope ? 4.5f : 0.f;
  mHitachiWithoutZ = HitachiScope && !(hParams->flags & HITACHI_HAS_STAGEZ);
}

// MESSAGE HANDLERS
void CComplexTasks::OnUpdateNoTasks(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks());  
}

void CComplexTasks::OnUpdateNoTasksNoTS(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries());  
}

void CComplexTasks::OnEucentricity(UINT nID) 
{
  int index = nID - ID_EUCENTRICITY_COARSE;
  int flags = (index % 2 == 0 ? FIND_EUCENTRICITY_COARSE : 0) | 
    (index > 0 ? FIND_EUCENTRICITY_FINE : 0);
  FindEucentricity(flags);
}

void CComplexTasks::OnTasksFinerealign()
{
  FindEucentricity(FIND_EUCENTRICITY_FINE | REFINE_EUCENTRICITY_ALIGN);
}

void CComplexTasks::OnUpdateNoTasksNoTSNoHitachi(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() && 
    !mHitachiWithoutZ);  
}

void CComplexTasks::OnReverseTilt() 
{
  int iDir = (mScope->GetReversalTilt() > mScope->GetTiltAngle()) ? 1 : -1;
  ReverseTilt(iDir);
}

void CComplexTasks::OnWalkup() 
{
  if (KGetOneFloat("Angle to walk up to:", mWalkTarget, 2))
    WalkUp(mWalkTarget, -1, 0.);
}

void CComplexTasks::OnTasksWalkupanchor() 
{
  TiltSeriesParam *tsParam = mWinApp->mTSController->GetTiltSeriesParam();
  float anchor = (float)fabs((double)tsParam->anchorTilt);
  char letter = 'A' + MAX_BUFFERS - 1;
  double angle = mScope->GetTiltAngle();
  CString string = "Angle at which to take anchor (it will be stored in buffer " 
    + CString(letter) + "):";
  if (!KGetOneFloat("Angle to walk up to:", mWalkTarget, 2))
    return;
  if (mWalkTarget < 0.)
    anchor = -anchor;
  if (!KGetOneFloat(string, anchor, 2))
    return;
  if (fabs(angle - mWalkTarget) < 2. || 
    (angle < mWalkTarget && (anchor < angle || anchor > mWalkTarget - 2.)) ||
    (angle > mWalkTarget && (anchor > angle || anchor < mWalkTarget + 2.))) {
    mTSController->TSMessageBox("An anchor picture would not be taken with this\n"
      "arrangement of angles.  Try again.");
    return;
  }

  // Maintain sign of the stored parameter from ts param
  if (anchor * tsParam->anchorTilt < 0)
    tsParam->anchorTilt = -anchor;
  else
    tsParam->anchorTilt = anchor;
  WalkUp(mWalkTarget, MAX_BUFFERS - 1, anchor);
}

// Set walk-up increments
void CComplexTasks::OnTasksSetincrements()
{
  CString str;
  str.Format("Maximum tilt increment for walk-up (applies at zero tilt; standard"
    " setting is %.1f):", mMaxWalkIntDflt);
  if (!KGetOneFloat(str, mMaxWalkInterval, 1))
    return;
  mMaxWalkInterval = B3DMIN(30.f, B3DMAX(1.f, mMaxWalkInterval));
  str.Format("Minimum tilt increment for walk-up (applies at high tilt; standard"
    " setting is %.1f):", mMinWalkIntDflt);
  if (!KGetOneFloat(str, mMinWalkInterval, 1))
    return;
  mMinWalkInterval = B3DMIN(mMaxWalkInterval, B3DMAX(1.f, mMinWalkInterval));
  if (mWinApp->GetSTEMMode()) {
    if (!KGetOneFloat("Tilt interval at which to autofocus in STEM mode (0 to not focus):"
      , mWalkSTEMfocusInterval, 1))
      return;
    B3DCLAMP(mWalkSTEMfocusInterval, 0.f, 100.f);
  }
}

void CComplexTasks::OnResetRealign() 
{
  ResetShiftRealign();  
}

// Allow user to set tilt axis offset, provide last 
void CComplexTasks::OnTasksSettiltaxisoffset() 
{
  CString infoLine;
  float currentOffset = mScope->GetTiltAxisOffset();
  float newVal = mLastAxisOffset;
  if (newVal > -900.) {
    infoLine.Format("The axis offset is currently %.2f um; the total offset implied by "
      "the last full eucentricity run was %.2f um", currentOffset, mLastAxisOffset);
  } else {
    newVal = currentOffset;
    infoLine.Format("The axis offset is currently %.2f um; "
      "there is no information from refining eucentricity.", currentOffset);
  }

  if (KGetOneFloat(infoLine, "Enter new total tilt axis offset:", newVal, 2))
    mScope->SetTiltAxisOffset(newVal);
  if (!mScope->GetShiftToTiltAxis())
    AfxMessageBox("Your entry will have no effect unless you select the option to\n"
    "center image shift on the tilt axis in the Align&Focus control panel", MB_EXCLAME);
}

void CComplexTasks::OnTrialInLdRefine()
{
  if (!mFEUseTrialInLD && !mFEWarnedUseTrial) {
    mFEWarnedUseTrial = true;
    CString message;
    message.Format("The minimum field of view for reliable Fine Eucentricity is %.1f "
      "microns.\nThe procedure could fail if the field of view of a Trial image\n"
      "is much smaller than this and if the stage is not well-behaved.\n\n"
      "Also, the eucentricity will not be set well if the Trial area\n"
      "is not at about the same height as the Record area.\n\n"
      "Are you sure you want to use the Trial area?", mMinFEFineField);
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return;
  }
	mFEUseTrialInLD = !mFEUseTrialInLD;
}

void CComplexTasks::OnUpdateTrialInLdRefine(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mFEUseTrialInLD ? 1 : 0);
}

// View in Walkup low dose
void CComplexTasks::OnTasksUseViewInLowdose()
{
  mWalkUseViewInLD = !mWalkUseViewInLD;
}

void CComplexTasks::OnUpdateTasksUseViewInLowdose(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mWalkUseViewInLD ? 1 : 0);
}

void CComplexTasks::OnTasksVerbose() 
{
  mVerbose = !mVerbose; 
}

void CComplexTasks::OnUpdateTasksVerbose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mVerbose ? 1 : 0);
}

void CComplexTasks::OnTasksSetiterationlimit() 
{
  float limit = mRSRAUserCriterion >= 0. ? mRSRAUserCriterion : mRSRACriterion;
  if (KGetOneFloat("Enter minimum image shift in microns for doing another "
    "iteration of reset and realign:", limit, 1))
    mRSRAUserCriterion = limit;
}

void CComplexTasks::OnTasksUsetrialinld() 
{
  if (!mRSRAUseTrialInLDMode && !mRSRAWarnedUseTrial) {
    mRSRAWarnedUseTrial = true;
    CString message;
    message.Format("The minimum field of view for reliable Reset and Realign is %.1f "
      "microns.\nThe procedure could fail and lose positioning if the field of view of\n"
      "a Trial image is much smaller than this and if the stage is not well-behaved.\n\n"
      "Are you sure you want to use the Trial area?", mMinRSRAField);
    if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return;
  }
	mRSRAUseTrialInLDMode = !mRSRAUseTrialInLDMode;
}

void CComplexTasks::OnUpdateTasksUsetrialinld(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mRSRAUseTrialInLDMode ? 1 : 0);
}

// ComplexTasks will report in on tasks from MultiTSTasks too!
BOOL CComplexTasks::DoingTasks()
{
  return mDoingRSRA || mReversingTilt || mWalkIndex >= 0 || mDoingEucentricity || 
    mDoingTASM || mDoingBASP || mWinApp->mMultiTSTasks->GetAutoCentering() || 
    mWinApp->mMultiTSTasks->GetCooking() || mWinApp->mMultiTSTasks->GetAssessingRange()
    || mWinApp->mMultiTSTasks->DoingAnchorImage();
}

// Strangely enough, nothing in this collection is a complex enough task...
BOOL CComplexTasks::DoingComplexTasks()
{
  return mWinApp->mMultiTSTasks->GetAssessingRange();
}

////////////////////////////////////////////////////////////////////////
//  GENERAL HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////

// Make a control set for tracking shots
void CComplexTasks::MakeTrackingConSet(ControlSet *conSet, int targetSize, 
                                       int baseConset)
{
  int i;
  double dum1, dum2;
  float exposure, frameTime, tryExp;
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();

  // Set up control set for captures based on trial set
  *conSet = mConSets[baseConset];
  conSet->left = 0;
  conSet->right = camParam->sizeX;
  conSet->top = 0;
  conSet->bottom = camParam->sizeY;
  conSet->mode = SINGLE_FRAME;
  conSet->saveFrames = 0;
  conSet->doseFrac = 0;
  int size = camParam->sizeX < camParam->sizeY ? camParam->sizeX : camParam->sizeY;

  // loop until the binned image size reaches the target size
  while (size / conSet->binning > targetSize && !camParam->K2Type) {

    // First see if next binning up exists
    BOOL exists = false;
    for (i = 0; i < camParam->numBinnings; i++)
      exists = exists || (conSet->binning * 2 == camParam->binnings[i]);

    // If binning exists, shift to it, drop exposure time, and cut drift to
    // minimum if it exists
    if (exists) {

      // Get a trial exposure and constrain it.  If it falls too far below the minimum
      // task exposure or too far above the desired exposure, forget the binning change
      exposure = (conSet->exposure - camParam->deadTime) / 4.0f + camParam->deadTime;
      exposure = B3DMAX(exposure, mMinTaskExposure);
      tryExp = exposure;
      mCamera->ConstrainExposureTime(camParam, false, conSet->K2ReadMode, 
        conSet->binning * 2, conSet->alignFrames && !conSet->useFrameAlign, 1, exposure, 
        frameTime);
      if (exposure < 0.9 * mMinTaskExposure || exposure > 1.25 * tryExp || 
        exposure < 0.75 * tryExp)
        break;
      conSet->exposure = exposure;
      conSet->binning *= 2;
      if (conSet->exposure <= 0.25 &&
        conSet->drift > camParam->builtInSettling)
        conSet->drift = camParam->builtInSettling;
      if (conSet->exposure <= 0.1)
        conSet->drift = 0.;
    } else
      break;
  }
  if (camParam->STEMcamera && camParam->maxScanRate > 0.) {
    int sizeX = (conSet->right - conSet->left) / conSet->binning;
    int sizeY = (conSet->bottom - conSet->top) / conSet->binning;
    float pixel = (float)conSet->binning * mShiftManager->GetPixelSize(
      mWinApp->GetCurrentCamera(), mScope->FastMagIndex());
    mCamera->ComputePixelTime(camParam, sizeX, sizeY, conSet->lineSync,
        pixel, camParam->maxScanRate, conSet->exposure, dum1, dum2);
  }
  SEMTrace('t', "MakeTrackingConset: binning %d from %d, exposure %f from %f, drift %f", 
    conSet->binning, mConSets[baseConset].binning, conSet->exposure, 
    mConSets[baseConset].exposure, conSet->drift);
}

void CComplexTasks::LowerMagIfNeeded(int maxMagInd, float calIntSafetyFac, 
                                     float intZoomSafetyFac, int conSetNum)
{
  // If we need to drop the mag, save the current mag and set intensity zoom
  double newIntensity, delBeam, newDelta;
  float safetyFactor, slitFactor, exposure, tryExp, frameTime;
  int magInd = mScope->GetMagIndex();
  FilterParams *filtParams = mWinApp->GetFilterParams();
  int camera = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  ControlSet *conSet = &mConSets[conSetNum];
  int error, sizeX, sizeY;
  float actualFac;
  float deadTime = camParam->deadTime;
  BOOL realCamera = !mCamera->IsCameraFaux();
  BOOL inverted = mScope->GetUseInvertedMagRange() && UtilMagInInvertedRange(maxMagInd,
    camParam->GIF);

  // Initialize for various settings if no change
  slitFactor = 1.;
  mSavedMagInd[mMagStackInd] = 0;
  mSavedIntensity[mMagStackInd] = 0.;
  mSavedSlitWidth[mMagStackInd] = 0;
  if (!mMagStackInd)
    mCumulSafetyFactor = 1.;
  mLowMagConSet = TRACK_CONSET;

  // If low dose mode, skip out, it is handled there
  if (mWinApp->LowDoseMode()) {
    mLowMagConSet = 0;
    if (mMagStackInd < MAX_MAG_STACK)
      mMagStackInd++;
    return;
  }

  if ((!inverted && magInd > maxMagInd) || (inverted && magInd < maxMagInd)) {
    mSavedMagInd[mMagStackInd] = magInd;
    mSavedExposure[mMagStackInd] = conSet->exposure;
    mConSetModified[mMagStackInd] = conSetNum;

    // For STEM, lower the mag, adjust exposure for scan rate limitation, and increment 
    // stack index
    if (camParam->STEMcamera) {
      if (camParam->maxScanRate > 0.) {
        sizeX = (conSet->right - conSet->left) / conSet->binning;
        sizeY = (conSet->bottom - conSet->top) / conSet->binning;
        mCamera->ComputePixelTime(camParam, sizeX, sizeY, conSet->lineSync,
          (float)conSet->binning * mShiftManager->GetPixelSize(camera, maxMagInd), 
          camParam->maxScanRate, conSet->exposure, delBeam, newDelta);
        SEMTrace('t', "LowerMagIfNeeded: STEM exposure set to %.3f", conSet->exposure);
      }
      mScope->SetMagIndex(maxMagInd);
      if (mMagStackInd < MAX_MAG_STACK)
        mMagStackInd++;
      return;
    }

    if (realCamera)
      mUsersIntensityZoom[mMagStackInd] = mScope->GetIntensityZoom();
    
    // Divide the given safety factor by the cumulative safety factor to get
    // remaining factor to change
    safetyFactor = calIntSafetyFac / mCumulSafetyFactor;
    if (safetyFactor > 1.)
      safetyFactor = 1.;

    // Do we need to open the GIF slit more?
    if (mWinApp->GetFilterMode() && filtParams->slitIn && 
      filtParams->slitWidth < mMinLMSlitWidth) {
      
      // Save the slit width, set up an intensity change factor that is
      // the product of slit width ratio and a safety factor
      mSavedSlitWidth[mMagStackInd] = filtParams->slitWidth;
      slitFactor = mSlitSafetyFactor * (filtParams->slitWidth / mMinLMSlitWidth);
      filtParams->slitWidth = mMinLMSlitWidth;
      mWinApp->mFilterControl.UpdateSettings();
      mCamera->SetupFilter();
    }
    
    // Can it be done with beam intensity calibration?
    delBeam = mShiftManager->GetPixelSize(camera, magInd) /
      mShiftManager->GetPixelSize(camera, maxMagInd);
    delBeam *= delBeam * safetyFactor * slitFactor;
    error = mWinApp->mBeamAssessor->AssessBeamChange(delBeam, newIntensity, 
      newDelta, -1);
    if (!error || error == BEAM_ENDING_OUT_OF_RANGE) {

      // If so, turn OFF intensity zoom, and make the mag and intensity changes
      // reduce the beam change if necessary and change exposure to compensate
      mSavedIntensity[mMagStackInd] = mScope->GetIntensity();
      if (realCamera)
        mScope->SetIntensityZoom(false);
      mScope->SetMagIndex(maxMagInd);
      if (FEIscope)
        mWinApp->mBeamAssessor->ChangeBeamStrength(delBeam / newDelta, -1);
      else
        mScope->DelayedSetIntensity(newIntensity, GetTickCount());

    } else {

      // If not, rely on intensity zoom, and reduce the exposure time for a margin
      // of safety.
      // But if intensity zoom fails, make the exposure change do the whole job
      if (realCamera) {
        if (!mScope->SetIntensityZoom(true))
          intZoomSafetyFac *= (float)delBeam;
      }
      mScope->SetMagIndex(maxMagInd);
      safetyFactor = intZoomSafetyFac / mCumulSafetyFactor;
      if (safetyFactor > 1.)
        safetyFactor = 1.;
      newDelta = safetyFactor * slitFactor;
    }
    
    mCumulSafetyFactor *= safetyFactor;

    // Scale down exposure, adjusting by dead time.  Also apply minima and constraints
    // and accept the scaled value if it is not too much smaller than the minimum and the
    // intended exposure
    exposure = (float)newDelta * (conSet->exposure - deadTime) + deadTime;
    exposure = B3DMAX(exposure, mMinTaskExposure);
    tryExp = exposure;
    mCamera->ConstrainExposureTime(camParam, false, conSet->K2ReadMode, conSet->binning, 
      conSet->alignFrames && !conSet->useFrameAlign, 1, exposure, frameTime);

    // If the constrained exposure comes out too small, try again with a bigger starting
    // value
    if (exposure < 0.8 * tryExp) {
      exposure = 1.3f * tryExp;
      mCamera->ConstrainExposureTime(camParam, false, conSet->K2ReadMode, conSet->binning,
        conSet->alignFrames && !conSet->useFrameAlign, 1, exposure, frameTime);
    }
    if (exposure >= 0.9 * mMinTaskExposure && exposure >= 0.8 * tryExp)
      conSet->exposure = exposure;
    actualFac = conSet->exposure / mSavedExposure[mMagStackInd];
    if (deadTime < mSavedExposure[mMagStackInd] - 0.0001)
      actualFac = (conSet->exposure - deadTime) / 
      (mSavedExposure[mMagStackInd] - deadTime);
    
    SEMTrace('t', "Total intensity change of %.3f needed; exposure change by %.3f needed\r\n"
      "effective exposure changed by %.3f to %.3f\r\n"
      "safety factor %.3f; cumulative safety factor %.3f; slit factor %.3f",
      delBeam, newDelta, actualFac, 
      conSet->exposure, safetyFactor, mCumulSafetyFactor, slitFactor);
  }

  // Increase stack index
  if (mMagStackInd < MAX_MAG_STACK)
    mMagStackInd++;
}

void CComplexTasks::RestoreMagIfNeeded()
{
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  FilterParams *filtParams = mWinApp->GetFilterParams();

  //Pop the stack index unless at bottom
  if (mMagStackInd > 0)
    mMagStackInd--;

  // If we changed mag, restore it and intensity zoom
  if (mSavedMagInd[mMagStackInd]) {
    // This will just return if we already set mag back
    mScope->SetMagIndex(mSavedMagInd[mMagStackInd]);
    if (!mCamera->IsCameraFaux() && !camParam->STEMcamera)
      mScope->SetIntensityZoom(mUsersIntensityZoom[mMagStackInd]);

    // Restore the intensity if saved.  Restore the exposure time
    if (mSavedIntensity[mMagStackInd] > 0.) {
      if (FEIscope)
        mScope->SetIntensity(mSavedIntensity[mMagStackInd]);
      else
        mScope->DelayedSetIntensity(mSavedIntensity[mMagStackInd], GetTickCount());
    }
    mConSets[mConSetModified[mMagStackInd]].exposure = mSavedExposure[mMagStackInd];

    // Restore slit if it was opened
    if (mSavedSlitWidth[mMagStackInd] > 0) {
      filtParams->slitWidth = mSavedSlitWidth[mMagStackInd];
      mWinApp->mFilterControl.UpdateSettings();
      mCamera->SetupFilter();
    }  
  }
}

// Find the highest mag that gives field size bigger than minimum size
// But do not drop below M mode.  Pass curMag as the current mag or -2 to get and USE the
// current mag and check for inverted mag range; the default is -1 which means ignore
// inverted mag range
int CComplexTasks::FindMaxMagInd(float inField, int curMag)
{
  int iMag,limlo, limhi, thisMag, prevMag;
  int iCam = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + iCam;
  MagTable *magTab = mWinApp->GetMagTable();
  int size = camParam->sizeX < camParam->sizeY ? camParam->sizeX : camParam->sizeY;
  int lowestInd = mScope->GetLowestNonLMmag(camParam);
  int GIFind = mScope->GetLowestGIFModeMag();
  if (camParam->GIF && GIFind)
    lowestInd = B3DMAX(GIFind, lowestInd);
  if (curMag == -2)
    curMag = mScope->FastMagIndex();

  if (curMag > 0 && mScope->GetUseInvertedMagRange() && 
    UtilMagInInvertedRange(curMag, camParam->GIF)) {
      UtilInvertedMagRangeLimits(camParam->GIF, limlo, limhi);
      for (iMag = limlo; iMag < limhi; iMag++) {
        thisMag = B3DCHOICE(camParam->GIF, magTab[iMag].EFTEMmag, magTab[iMag].mag);
        if (thisMag && mShiftManager->GetPixelSize(iCam, iMag) * size >= inField)
          return iMag;
      }
      return iMag;
  }

  mWinApp->GetMagRangeLimits(iCam, lowestInd, limlo, limhi);
  for (iMag = limhi; iMag > lowestInd; iMag--) {
    thisMag = B3DCHOICE(camParam->GIF, magTab[iMag].EFTEMmag, magTab[iMag].mag);
    prevMag = B3DCHOICE(camParam->GIF, magTab[iMag - 1].EFTEMmag, magTab[iMag - 1].mag);
    if (!thisMag || thisMag < prevMag)
      continue;
    if (mShiftManager->GetPixelSize(iCam, iMag) * size >= inField)
      return iMag;
  }
  return iMag;
}

// Check whether any task has set us into a lower mag - or TS controller
BOOL CComplexTasks::InLowerMag()
{
  int i;
  for (i = 0; i < mMagStackInd; i++)
    if (mSavedMagInd[i])
      return true;
  return false;
}

// Call initiate capture, get intensity and spot size from scope or low dose params,
// and add to the accumulated dose
void CComplexTasks::StartCaptureAddDose(int conSet)
{
  int spotSize, set, probe;
  double intensity, dose;
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();

  mCamera->SetCancelNextContinuous(true);
  mCamera->InitiateCapture(conSet);
  if (mWinApp->LowDoseMode()) {
    set = mCamera->ConSetToLDArea(conSet);
    spotSize = ldParam[set].spotSize;
    intensity = ldParam[set].intensity;
    probe = ldParam[set].probeMode;
  } else {
    spotSize = mScope->FastSpotSize();
    intensity = mScope->FastIntensity();
    probe = mScope->GetProbeMode();
  }

  dose = mWinApp->mBeamAssessor->GetElectronDose(spotSize, intensity, 
    mCamera->SpecimenBeamExposure(mWinApp->GetCurrentCamera(), &mConSets[conSet]), probe);
  mTotalDose += dose;
  if (mWinApp->LowDoseMode() && (set == VIEW_CONSET || set == RECORD_CONSET))
    mOnAxisDose += dose;
}

double CComplexTasks::GetAndClearDose()
{
  double temp = mTotalDose;
  mTotalDose = 0.;
  mOnAxisDose = 0.;
  return temp;
}


////////////////////////////////////////////////////////////////////////
//  RESET SHIFT WITH REALIGNMENT
////////////////////////////////////////////////////////////////////////

void CComplexTasks::ResetShiftRealign()
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  double ISX, ISY;
  int newMagInd = FindMaxMagInd(mMinRSRAField, -2);
  ControlSet  *conSet = mConSets + TRACK_CONSET;
  mRSRAActPostExposure = mWinApp->ActPostExposure();
  mRSRAIteration = 1;

  // Increase mag if shift is small enough
  mScope->GetLDCenteredShift(ISX, ISY);
  if (mShiftManager->RadialShiftOnSpecimen(ISX, ISY, 
    mScope->GetMagIndex()) < mRSRAHigherMagCriterion)
    newMagInd++;

  // Set flag and states now in case of mag change
  mDoingRSRA = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "RESET SHIFTS & REALIGN");

  // Make a conset for tracking unless another routine did
  if (!mMagStackInd || mConSetModified[mMagStackInd - 1] != TRACK_CONSET)
    MakeTrackingConSet(conSet, targetSize);

  // Enforce settling time after tilt
  mCamera->SetRequiredRoll(1);
  mCamera->SetObeyTiltDelay(true);

  LowerMagIfNeeded(newMagInd, 0.7f, 0.3f, TRACK_CONSET);
  if (mRSRAUseTrialInLDMode && mWinApp->LowDoseMode())
    mLowMagConSet = 2;

  StartCaptureAddDose(mLowMagConSet);
  mWinApp->AddIdleTask(TaskRSRABusy, TASK_RESET_REALIGN, RSRA_FIRST_SHOT, 0);
}

void CComplexTasks::RSRANextTask(int param)
{
  double ISX, ISY, radialIS;
  int stackInd = mMagStackInd > 0 ? mMagStackInd - 1 : 0;
  if (!mDoingRSRA)
    return;
  
  switch (param) {
  case RSRA_FIRST_SHOT:
    mImBufs->mCaptured = BUFFER_TRACKING;
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    if (mShiftManager->ResetImageShift(true, false)) {
      RSRACleanup(1);
      return;
    }
    mWinApp->AddIdleTask(TaskRSRABusy, TASK_RESET_REALIGN, RSRA_RESETTING, 0);
    return;

  case RSRA_RESETTING:
    // Change the mag after exposure if set for that, and this is last chance
    if (mRSRAActPostExposure && mSavedMagInd[stackInd] && 
      mRSRAIteration == mMaxRSRAIterations)
      mCamera->QueueMagChange(mSavedMagInd[stackInd]);
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(TaskRSRABusy, TASK_RESET_REALIGN, RSRA_SECOND_SHOT, 0);
    return;

  case RSRA_SECOND_SHOT:
    mImBufs->mCaptured = BUFFER_TRACKING;
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    mShiftManager->AutoAlign(1, 0);
    if (mRSRAIteration++ < mMaxRSRAIterations) {
      // If still able to iterate, find radial image shift distance and
      // iterate if it is greater than criterion
      mScope->GetLDCenteredShift(ISX, ISY);
      radialIS = mShiftManager->RadialShiftOnSpecimen(ISX, ISY, 
        mScope->GetMagIndex());
      if (radialIS > mRSRACriterion ||
        (mRSRAUserCriterion >= 0. && radialIS > mRSRAUserCriterion)) {
        //CString report;
        //report.Format("Image shift after reset = %.2f, doing it again", radialIS);
        //mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

        if (mSkipNextBeamShift)
          mScope->SetSkipNextBeamShift(true);
        if (mShiftManager->ResetImageShift(true, true, 10000)) {
          RSRACleanup(1);
          return;
        }
        mWinApp->AddIdleTask(TaskRSRABusy, TASK_RESET_REALIGN, RSRA_RESETTING, 0);
        
        // Copy back to A so it is correlated against
        mBufferManager->CopyImageBuffer(1, 0);
        return;
      }
    }
    
    StopResetShiftRealign();
    return;
  }
}

void CComplexTasks::StopResetShiftRealign()
{
  if (!mDoingRSRA)
    return;
  RestoreMagIfNeeded();
  
  mDoingRSRA = false;
  mSkipNextBeamShift = false;
  mCamera->SetRequiredRoll(0);
  mCamera->SetObeyTiltDelay(false);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

int CComplexTasks::TaskRSRABusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  return (winApp->mCamera->CameraBusy() || winApp->mShiftManager->ResettingIS()) ? 
    1 : 0;
}

void CComplexTasks::RSRACleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out trying to reset shifts and realign"));
  StopResetShiftRealign();
  mWinApp->ErrorOccurred(error);
}

////////////////////////////////////////////////////////////////////////
//  WALK UP
////////////////////////////////////////////////////////////////////////

// Start the walkup, return true if a regular mag image will be left behind
BOOL CComplexTasks::WalkUp(float targetAngle, int anchorBuf, float anchorAngle)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  int i = 0;
  BOOL retVal;
  double interval;
  ControlSet  *conSet = mConSets + TRACK_CONSET;
  double mWalkStartAngle = mScope->GetTiltAngle();
  int maxSteps = (int)(fabs(targetAngle - mWalkStartAngle) / mMinWalkInterval + 2.);
  mWalkMagInd = FindMaxMagInd(mMinWalkField, -2);
  mWalkDidResetShift = false;
 
  // Don't do it if too close to target
  if (fabs(targetAngle - mWalkStartAngle) < 0.5 * mMinWalkInterval) {
    mScope->TiltTo(targetAngle);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
    mLastWalkCompleted = true;
    return false;
  }

  mWalkAngles = new double[maxSteps];
  double cumAngle = mWalkStartAngle;
  int iDir = targetAngle > mWalkStartAngle ? 1 : -1;
  
  // Have to align to buffer past the roll except in bidir series return phase
  mWalkAlignBuffer = mBufferManager->GetShiftsOnAcquire() + 1;
  if (mWalkAlignBuffer == 1 || (mWinApp->DoingTiltSeries() && 
    mWinApp->mTSController->GetBidirSeriesPhase() == BIDIR_RETURN_PHASE))
    mWalkAlignBuffer = 2;

  // This works even with DM drift settling
  mWUActPostExposure = mWinApp->ActPostExposure();
  mWULastFocusAngle = (float)mWalkStartAngle;

  // Compute angles by having cosine intervals at least as
  // large as the minimum
  while (iDir * cumAngle < iDir * targetAngle && i < maxSteps) {
    interval = mMaxWalkInterval * cos(DTOR * cumAngle);
    if (interval < mMinWalkInterval)
      interval = mMinWalkInterval;
    cumAngle += iDir * interval;
    mWalkAngles[i++] = cumAngle;
  }

  // Replace the last angle with the target, but
  // Eliminate the last interval if it less than 1/3 of minimum
  if (i > 1 && fabs(targetAngle) - fabs(mWalkAngles[i - 2]) <
    mMinWalkInterval / 3.)
    i--;

  mWalkAngles[i - 1] = targetAngle;
  mNumWalkAngles = i;

  // If anchor requested, find the closest angle to given angle
  mWUAnchorBuf = anchorBuf;
  mWUAnchorIndex = 0;
  interval = 10000.;

  if (mWUAnchorBuf >= 0) {
    for (i = 0; i < mNumWalkAngles; i++) {
      if (fabs(mWalkAngles[i] - anchorAngle) < interval) {
        interval = fabs(mWalkAngles[i] - anchorAngle);
        mWUAnchorIndex = i + 1;
      } 
    }
  }

  // If low dose, stick with tracking conset and do not lower mag unless view flag set
  if (mWinApp->LowDoseMode() && !mWalkUseViewInLD) {
    mWalkConSet = TRIAL_CONSET;
  } else {

    // Otherwise make a conset for tracking unless another routine did
    mWalkConSet = TRACK_CONSET;
    if (!mMagStackInd || mConSetModified[mMagStackInd - 1] != TRACK_CONSET)
      MakeTrackingConSet(conSet, targetSize);
    LowerMagIfNeeded(mWalkMagInd, 0.8f, 0.6f, TRACK_CONSET);
    if (mWinApp->LowDoseMode())
      mWalkConSet = VIEW_CONSET;
  }
  retVal = !(InLowerMag() || (mWinApp->LowDoseMode() && mWalkUseViewInLD));
  mWalkIndex = 0;
  mWUGettingAnchor = false;
  mNeedTiltAfterReset = false;
  mLastWalkCompleted = false;

  // Reverse the tilt direction if necessary
  if (ReverseTilt(iDir))
    mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_REVERSED, 0);
  else
    WalkUpNextTask(WALKUP_REVERSED);
  return retVal;
}

void CComplexTasks::WalkUpNextTask(int param)
{
  double shiftX, shiftY;
  StageMoveInfo smi;
  int delay;
  CString paneText;

  if (mWalkIndex < 0)
    return;

  paneText.Format("WALKING TO %.0f DEG", mWalkAngles[mNumWalkAngles - 1]);
  switch (param) {
  case WALKUP_REVERSED:
    mCamera->SetRequiredRoll(1);
    
    // Start the first picture, but first queue the tilt
    mPretilted = mWUActPostExposure;
    if (mWUActPostExposure) {
      smi.axisBits = axisA;
      smi.alpha = mWalkAngles[0];
      mCamera->QueueStageMove(smi, mShiftManager->GetMinTiltDelay());
    }
    mWinApp->UpdateBufferWindows();
    mWinApp->SetStatusText(MEDIUM_PANE, paneText);
    StartCaptureAddDose(mWalkConSet);
    mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_IMAGE, 0);
    return;

  
  case WALKUP_RESET:

    // When reset is done, do tilt if necessary
    mCamera->SetRequiredRoll(1);
    mWinApp->SetStatusText(MEDIUM_PANE, paneText);
    mNeedTiltAfterReset = mMinTASMField > 0.;
    if (!mPretilted) {
      if (mNeedTiltAfterReset)
        TiltAfterStageMove((float)mWalkAngles[mWalkIndex], false);
      else
        mScope->TiltTo(mWalkAngles[mWalkIndex]);
      mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_TILTED, 
        mNeedTiltAfterReset ? 0 : 60000);
      mNeedTiltAfterReset = false;
      return;
    }
    break;

  case WALKUP_IMAGE:
    
    mImBufs->mCaptured = BUFFER_TRACKING;
    
    // If got an anchor at regular mag, save the anchor and lower mag again
    if (mWUGettingAnchor) {
      mBufferManager->CopyImageBuffer(0, mWUAnchorBuf);
      mImBufs[mWUAnchorBuf].mCaptured = BUFFER_ANCHOR;
      mWUGettingAnchor = false;
      LowerMagIfNeeded(mWalkMagInd, 0.8f, 0.6f, TRACK_CONSET);
    
    } else {

      // Align image to previous if past first
      if (mWalkIndex)
        mShiftManager->AutoAlign(mWalkAlignBuffer, 0);
      if (mWalkIndex < 0)
        return;

      // Copy image to align buffer
      mBufferManager->CopyImageBuffer(0, mWalkAlignBuffer);
    
      // If this is anchor, copy it to requested buffer if we are at regular mag
      if (mWUAnchorBuf >= 0 && mWalkIndex == mWUAnchorIndex) {
        if (!InLowerMag() && !(mWinApp->LowDoseMode() && mWalkUseViewInLD)) {
          mBufferManager->CopyImageBuffer(0, mWUAnchorBuf);
          mImBufs[mWUAnchorBuf].mCaptured = BUFFER_ANCHOR;
        } else {

          // Otherwise, restore the mag and set up a shot
          RestoreMagIfNeeded();
          mWUGettingAnchor = true;
          StartCaptureAddDose((mWinApp->LowDoseMode() && mWalkUseViewInLD) ? TRIAL_CONSET
            : mWalkConSet);
          mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_IMAGE, 0);
          return;
       }
      }
    }

    // Test the image shift for out of range (???) and reset if needed
    mScope->GetLDCenteredShift(shiftX, shiftY);
    if (mWalkIndex < mNumWalkAngles && mShiftManager->RadialShiftOnSpecimen
      (shiftX, shiftY, mScope->FastMagIndex()) > 
      (mWinApp->LowDoseMode() ? mWULowDoseISLimit : mWalkShiftLimit)) {
      ResetShiftRealign();
      mWalkDidResetShift = true;
      mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_RESET, 0);
      return;
    }

    // Tilt if it hasn't happened yet, with post reset tracking or simple tilt
    if (!mPretilted && mWalkIndex < mNumWalkAngles) {
      if (mNeedTiltAfterReset)
        TiltAfterStageMove((float)mWalkAngles[mWalkIndex], false);
      else
        mScope->TiltTo(mWalkAngles[mWalkIndex]);
      mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_TILTED, 
        mNeedTiltAfterReset ? 0 : 60000);
      mNeedTiltAfterReset = false;
      return;
    }
    break;
    
  case WALKUP_TILTED:
    if (mWinApp->GetSTEMMode() && !mScope->GetProbeMode() && mWalkIndex < mNumWalkAngles
      && fabs(mWalkAngles[mWalkIndex] - mWULastFocusAngle) >= mWalkSTEMfocusInterval &&
      mWalkSTEMfocusInterval > 0. && mWinApp->mFocusManager->FocusReady()) {
        mWinApp->mFocusManager->AutoFocusStart(1);
        mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_TILTED, 0);
        mWULastFocusAngle = (float)mWalkAngles[mWalkIndex];
        return;
    }
    break;
  }

  // If there is another angle to get an image at, get one
  if (mWalkIndex++ < mNumWalkAngles) {

    // Queue a tilt if appropriate
    if (mWUActPostExposure && mWalkIndex < mNumWalkAngles && !mNeedTiltAfterReset &&
      !(mWalkIndex == mWUAnchorIndex &&  
      (InLowerMag() || (mWinApp->LowDoseMode() && mWalkUseViewInLD)))) {
      smi.axisBits = axisA;
      smi.alpha = mWalkAngles[mWalkIndex];

      // Set up minimal delay, or full delay on the last shot
      if (mWalkIndex < mNumWalkAngles - 1)
        delay = mShiftManager->GetMinTiltDelay();
      else
        delay = mShiftManager->GetAdjustedTiltDelay(
          fabs(mWalkAngles[mWalkIndex] - mWalkAngles[mWalkIndex - 1]));
      mCamera->QueueStageMove(smi, delay);
      mPretilted = true;
    } else
      mPretilted = false;

    // Start capture and return - obey the tilt delay on the last shot
    if (mWalkIndex == mNumWalkAngles)
      mCamera->SetObeyTiltDelay(true);
    StartCaptureAddDose(mWalkConSet);
    mWinApp->AddIdleTask(TaskWalkUpBusy, TASK_WALKUP, WALKUP_IMAGE, 0);
    return;
  }

  // Done
  mLastWalkCompleted = true;
  StopWalkUp();

}

int CComplexTasks::TaskWalkUpBusy()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  int stage = winApp->mScope->StageBusy();
  if (stage < 0)
    return -1;
  return (stage > 0 || winApp->mComplexTasks->DoingResetShiftRealign() || 
    winApp->mCamera->CameraBusy() || winApp->mComplexTasks->ReversingTilt() ||
    winApp->mComplexTasks->DoingTiltAfterMove() || winApp->mFocusManager->DoingFocus()) ?
    1 : 0; 
}


void CComplexTasks::WalkUpCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out in the walk-up routine"));
  StopWalkUp();
  mWinApp->ErrorOccurred(error);
}

void CComplexTasks::StopWalkUp()
{
  if (mWalkIndex < 0)
    return;
  if (!mWinApp->LowDoseMode())
    RestoreMagIfNeeded();
  mWalkIndex = -1;
  mCamera->SetRequiredRoll(0);
  mCamera->SetObeyTiltDelay(false);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  if (mWalkAngles)
    delete [] mWalkAngles;
  mWalkAngles = NULL;
}

////////////////////////////////////////////////////////////////////////
//  REVERSE TILT
////////////////////////////////////////////////////////////////////////

// Work out the backlash in the desired direction of tilt and realign
// Returns true if actions started, or false if nothing needed
BOOL CComplexTasks::ReverseTilt(int inDirection)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  ControlSet  *conSet = mConSets + TRACK_CONSET;
  int actionStarted = RT_FIRST_SHOT;
  StageMoveInfo smi;
  float maxerr = mWinApp->mTSController->GetMaxTiltError();

  // Find current angle and angle that needs to be tilted to
  // If the last reversal angle is higher than the needed reverse angle in
  // the direction opposite to the one we are going, there is no need for this
  mRTStartAngle = mScope->GetTiltAngle();
  mRTReverseAngle = mRTStartAngle - inDirection * mRTThreshold;
  if (fabs(mRTReverseAngle) > mScope->GetMaxTiltAngle())
    mRTReverseAngle = (mRTStartAngle >= 0. ? 1.f : -1.f) * mScope->GetMaxTiltAngle();
  if (inDirection * (mScope->GetReversalTilt() - mRTReverseAngle) <= maxerr ||
    fabs(mRTReverseAngle - mRTStartAngle) <= maxerr) {
      SEMTrace('1', "Skipping reverse tilt: dir %d start %.2f revang %.2f  revtilt %.2f"
        " maxtilt %.2f  maxerr %.2f", inDirection, mRTStartAngle, mRTReverseAngle,
        mScope->GetReversalTilt(), mScope->GetMaxTiltAngle(), maxerr);
      mSkipNextBeamShift = false;
      return false;
  }
  SEMTrace('1', "Doing reverse tilt to %.2f at %.2f", mRTReverseAngle, mRTStartAngle);
    
  // Set flag and states now in case of mag change
  mReversingTilt = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "CORRECTING TILT BACKLASH");

  mMaxRTMagInd = FindMaxMagInd(mMinRTField, -2);
  // Make a conset for tracking unless another routine did
  if (!mMagStackInd || mConSetModified[mMagStackInd - 1] != TRACK_CONSET)
    MakeTrackingConSet(conSet, targetSize);
  mCamera->SetRequiredRoll(1);
  LowerMagIfNeeded(mMaxRTMagInd, 0.75f, 0.5f, TRACK_CONSET);

  // It's silly, but tilt if possible and come in at first tilt done
  mRTActPostExposure = mWinApp->ActPostExposure();
  if (mRTActPostExposure) {
    smi.axisBits = axisA;
    smi.alpha = mRTReverseAngle;
    mCamera->QueueStageMove(smi, mShiftManager->GetMinTiltDelay());
    actionStarted = RT_FIRST_TILT;
  }

  StartCaptureAddDose(mLowMagConSet);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_REVERSE_TILT, actionStarted, 0);
  return true;
}

void CComplexTasks::ReverseTiltNextTask(int param)
{
  int stackInd = mMagStackInd > 0 ? mMagStackInd - 1 : 0;
  if (!mReversingTilt)
    return;
  mImBufs->mCaptured = BUFFER_TRACKING;
  switch (param) {
  case RT_FIRST_SHOT:
    mScope->TiltTo(mRTReverseAngle);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_REVERSE_TILT, RT_FIRST_TILT, 60000);
    return;

  case RT_FIRST_TILT:
    mScope->TiltTo(mRTStartAngle);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_REVERSE_TILT, RT_SECOND_TILT, 30000);
    return;

  case RT_SECOND_TILT:
    // Change the mag after exposure if set for that
    if (mRTActPostExposure && mSavedMagInd[stackInd])
      mCamera->QueueMagChange(mSavedMagInd[stackInd]);
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_REVERSE_TILT, RT_SECOND_SHOT, 0);
    return;

  case RT_SECOND_SHOT:
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    mShiftManager->AutoAlign(1, 0);
    mScope->SetSkipNextBeamShift(false);
    
    StopReverseTilt();
    return;
  }
}

void CComplexTasks::ReverseTiltCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out in the reverse-tilt routine"));
  StopReverseTilt();
  mWinApp->ErrorOccurred(error);
}

void CComplexTasks::StopReverseTilt()
{
  if (!mReversingTilt)
    return;
  RestoreMagIfNeeded();
  mReversingTilt = false;
  mSkipNextBeamShift = false;
  mCamera->SetRequiredRoll(0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

////////////////////////////////////////////////////////////////////////
// FINDING EUCENTRICITY
////////////////////////////////////////////////////////////////////////
void CComplexTasks::FindEucentricity(int coarseFine)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  ControlSet  *conSet = mConSets + TRACK_CONSET;
  double ISX, ISY;
  CString mess;
  int action;
  int stackInd = mMagStackInd > 0 ? mMagStackInd - 1 : 0;
  int curMag = mScope->GetMagIndex();

  if (!mDoingEucentricity) {

    // If this is an external call, save the flags for coarse/fine,
    // make the control set, and lower the mag as appropriate for coarse or fine
    mFECoarseFine = coarseFine;
    mMaxFECoarseMagInd = FindMaxMagInd(mMinFECoarseField, curMag);
    mMaxFEFineMagInd = FindMaxMagInd(mMinFEFineField, curMag);
    mMaxFEFineAlignMag = FindMaxMagInd(mMinFEFineAlignField, curMag);
    if (mHitachiWithoutZ) {
      mess.Format("If you want to adjust for backlash, turn the Z knob\n"
        "%d grooves left, then right by about\n"
        "the same amount, stopping at the nearest groove", 
        B3DNINT(mManualHitachiBacklash / mZMicronsPerDialMark));
      AfxMessageBox(mess, MB_OK | MB_ICONINFORMATION);
    }

    // Make a conset for tracking unless another routine did
    if (!mMagStackInd || mConSetModified[mMagStackInd - 1] != TRACK_CONSET)
      MakeTrackingConSet(conSet, targetSize);
    mFEIterationCount = 0;
    mFEUsersAngle = 0.;
    if (coarseFine & FIND_EUCENTRICITY_COARSE) {
      LowerMagIfNeeded(mMaxFECoarseMagInd, 0.7f, 0.3f, TRACK_CONSET);
      action = FE_COARSE_RESET;
      mFECoarseFine &= ~REFINE_EUCENTRICITY_ALIGN;
    } else if (coarseFine & REFINE_EUCENTRICITY_ALIGN) {
      LowerMagIfNeeded(mMaxFEFineAlignMag, 0.7f, 0.3f, TRACK_CONSET);
      action = FE_FINE_ALIGNSHOT1;
      mWalkAlignBuffer = mBufferManager->GetShiftsOnAcquire() + 1;
      if (mWalkAlignBuffer == 1)
        mWalkAlignBuffer = 2;
      mFEUsersAngle = mScope->GetTiltAngle();
    } else {
      LowerMagIfNeeded(mMaxFEFineMagInd, 0.75f, 0.5f, TRACK_CONSET);
      if (mFEUseTrialInLD && mWinApp->LowDoseMode())
        mLowMagConSet = 2;
      action = FE_FINE_RESET;
    }
  } else {

    // But if this is an internal call, and it is for fine after coarse,
    // change the mag if the saved mag index is bigger than the fine one
    // Have to do it the long way to keep beam intensity happy
    if (!(mFECoarseFine & REFINE_EUCENTRICITY_ALIGN) && 
      ((mSavedMagInd[stackInd] > mMaxFEFineMagInd && 
      mMaxFEFineMagInd != mMaxFECoarseMagInd) || 
      (mFEUseTrialInLD && mWinApp->LowDoseMode()))) {
      RestoreMagIfNeeded();
      LowerMagIfNeeded(mMaxFEFineMagInd, 0.75f, 0.5f, TRACK_CONSET);
      if (mFEUseTrialInLD && mWinApp->LowDoseMode())
        mLowMagConSet = 2;
    }
    action = FE_FINE_RESET;
  }

  mFEActPostExposure = mWinApp->ActPostExposure();
  mDoingEucentricity = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "FINDING EUCENTRICITY");
  mCamera->SetRequiredRoll(1);
  mCamera->SetObeyTiltDelay(true);

  // If doing fine & align, set up to get first shot
  if (action == FE_FINE_ALIGNSHOT1) {
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(TASK_EUCENTRICITY, action, 0);
    return;
  }
  
  // If image shift is too big, reset it
  mScope->GetLDCenteredShift(ISX, ISY);
  if (mShiftManager->RadialShiftOnSpecimen(ISX, ISY, mScope->GetMagIndex()) > 
    mFEResetISThreshold) {
    if (mShiftManager->ResetImageShift(true, false)) {
      EucentricityCleanup(1);
      return;
    }
    mWinApp->AddIdleTask(TASK_EUCENTRICITY, action, 0);
    return;
  }

  // Otherwise call directly to setup
  EucentricityNextTask(action);
}

// Do next task for eucentricity routines
void CComplexTasks::EucentricityNextTask(int param)
{
  ScaleMat aInv, cMat;
  double stageX, stageY, ISX, ISY, specX, specY;
  float shiftX, shiftY, movedY, delZ, delY, intcp, movedX;
  float backlashTilt, backlashZ, delZact, delYact, yZero, yZeroGen, angle;
  double deltaInc, increment, radians;
  int action, i;
  BOOL needTilt;
  CString report;

  if (!mDoingEucentricity)
    return;
  if (mImBufs->mConSetUsed == TRACK_CONSET)
    mImBufs->mCaptured = BUFFER_TRACKING;
  mImBufs->GetTiltAngle(angle);
  switch (param) {
  case FE_COARSE_RESET:
    // Do the setup on coarse finding: set up angles and increments and
    // do a double stage move
    mScope->GetStagePosition(stageX, stageY, mFECurrentZ);
    backlashTilt = mFEInitialIncrement > 0. ? -mRTThreshold : mRTThreshold;
    mFECoarseIncrement = mFEInitialIncrement;
    mFEReferenceAngle = mFEInitialAngle;
    mFECurrentAngle = mFEInitialAngle;
    if (mHitachiWithoutZ)
      mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_COARSE_MOVED, 0);
    else
      DoubleMoveStage(mFECurrentZ, mFEBacklashZ, true, mFEInitialAngle, backlashTilt,
        true, FE_COARSE_MOVED);
    return;
    
  case FE_COARSE_MOVED:

    // A movement is done, need a picture
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_COARSE_SHOT, 0);
    return;

  case FE_COARSE_SHOT:

    // Got a picture.  If reference, need to tilt, replace with actual angle
    needTilt = false;
    if (mFECurrentAngle == mFEReferenceAngle) {
      needTilt = true;
      mFEReferenceAngle = mScope->GetTiltAngle();
    } else {

      // Otherwise, find out the shift and evaluate whether further tilt is needed
      // 5/27/06: call with new argument to avoid shift, so don't need to restore it
      if (mShiftManager->AutoAlign(1, 0, false, false))
        StopEucentricity();
      mImBufs->mImage->getShifts(shiftX, shiftY);

      aInv = mShiftManager->CameraToSpecimen(mScope->FastMagIndex());
      movedY = mConSets[mLowMagConSet].binning * 
        (aInv.ypx * shiftX - aInv.ypy * shiftY);

      // Use actual angle and increment to accommodate sloppiness of JEOL
      mFECurrentAngle = mScope->GetTiltAngle();
      mFECoarseIncrement = B3DMAX(mFECurrentAngle - mFEReferenceAngle, 0.1);
      report.Format("Angle = %.2f, increment = %.2f, shift = %.1f %.1f pixels, %.3f um Y",
        mFECurrentAngle, mFECoarseIncrement, shiftX, shiftY, movedY);
      if (mVerbose)
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);

      // If not close to target move, and not at max tilt, and not at max increment
      // then tilt again, setting increment that will reach target but not 
      // changing it too much
      // No point going less than 0.5 degree, though
      if (fabs(movedY) < 0.75 * mFETargetShift && 
        mFECurrentAngle < mFEMaxTilt - 0.5 &&
        mFECoarseIncrement < mFEMaxIncrement - 0.5) {
        deltaInc = mFETargetShift / fabs(movedY);
        if (deltaInc > mFEMaxIncrementChange)
          deltaInc = mFEMaxIncrementChange;
        mFECoarseIncrement *= deltaInc;

        // Also make sure increment is not too big and tilt does not become too big
        if (mFECoarseIncrement > mFEMaxIncrement)
          mFECoarseIncrement = mFEMaxIncrement;
        if (mFECoarseIncrement > mFEMaxTilt - mFEReferenceAngle)
          mFECoarseIncrement = mFEMaxTilt - mFEReferenceAngle;

        needTilt = true;
        mBufferManager->CopyImageBuffer(1, 0);
      }
    }

    // Do tilt if needed in either case
    if (needTilt) {
      mFECurrentAngle = mFEReferenceAngle + mFECoarseIncrement;
      mScope->TiltTo(mFECurrentAngle);
      mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_COARSE_MOVED, 
        B3DNINT(mStageTimeoutFactor * 30000));
      return;
    }

    // If not, it is time to move Z.  Compute a new Z, but based on a minimum
    // increment in case of small moves
    increment = mFECoarseIncrement;
    if (increment < 2. * mFEInitialIncrement)
      increment = 2. * mFEInitialIncrement;
    // SIGN?
    delZ = (float)(-movedY * (mShiftManager->GetStageInvertsZAxis() ? -1. : 1.) / 
      (sin(DTOR * (mFEInitialAngle + increment)) - sin(DTOR * mFEInitialAngle)));
    action = FE_COARSE_LAST_MOVE;
    mFEReferenceAngle = mFECurrentAngle;

    // If the increment is sufficiently less than the maximum, and also less
    // than the remaining distance to the maximum tilt, then set up to go again
    if (mFECoarseIncrement < 0.75 * mFEMaxIncrement && 
      mFECoarseIncrement < mFEMaxTilt - mFECurrentAngle)
      action = FE_COARSE_MOVED;

    if (mHitachiWithoutZ) {
      ReportManualZChange(delZ, "Rough");
      StopEucentricity();
      return;
    }
    backlashZ = 0;
    if (delZ * mFEBacklashZ > 0)
      backlashZ = mFEBacklashZ;
    mFECurrentZ += delZ;
    if (mWinApp->GetSTEMMode())
      mScope->IncDefocus(delZ / mWinApp->mFocusManager->GetSTEMdefocusToDelZ(-1));
    DoubleMoveStage(mFECurrentZ, backlashZ, true, 0., 0., false, action);
    report.Format("Rough eucentricity: changing Z by %.2f to %.2f, %s", delZ,
      mFECurrentZ, action == FE_COARSE_MOVED ? "continuing..." : "finished.");
    mWinApp->AppendToLog(report, 
      mVerbose ? LOG_OPEN_IF_CLOSED : LOG_SWALLOW_IF_CLOSED);
    return;

  case FE_COARSE_LAST_MOVE:

    // After the last move in Z, start the fine procedure or stop depending on
    // initial flags
    if (mFECoarseFine & FIND_EUCENTRICITY_FINE) {
      FindEucentricity(FIND_EUCENTRICITY_FINE);
      return;
    }
    StopEucentricity();
    return;
  
  case FE_FINE_ALIGNSHOT1:
    
    // Save the image in a protected buffer, do reset if needed, otherwise
    // fall through to the setup steps
    mBufferManager->CopyImageBuffer(0, mWalkAlignBuffer);
    mScope->GetLDCenteredShift(ISX, ISY);
    if (mFEUseTrialInLD && mWinApp->LowDoseMode())
      mLowMagConSet = 2;
    if (mShiftManager->RadialShiftOnSpecimen(ISX, ISY, mScope->GetMagIndex()) > 
      mFEResetISThreshold) {
      if (mShiftManager->ResetImageShift(true, false)) {
        EucentricityCleanup(1);
        return;
      }
      mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_FINE_RESET, 0);
      return;
    }

  case FE_FINE_RESET:
    
    // Setup the fine steps
    mWinApp->SetStatusText(MEDIUM_PANE, "REFINING EUCENTRICITY");
    mFEFineIndex = 0;
    mCumMovedX = 0.;
    mFEFineShifts[0] = 0.;
    backlashTilt = mFETargetAngles[0] > 0. ? mRTThreshold : -mRTThreshold;
    DoubleMoveStage(0., 0., false, mFETargetAngles[0], backlashTilt, true,
      FE_FINE_TILTED);
    return;

  case FE_FINE_TILTED:

    // Tilt done, get a picture
    EucentricityFineCapture();
    return;


  case FE_FINE_SHOT:

    // Shot done, find shift if past the first one
    shiftX = shiftY = movedX = movedY = 0.;
    if (!mImBufs->GetTiltAngle(angle))
      angle = (float)mFETargetAngles[mFEFineIndex];
    mFEFineAngles[mFEFineIndex] = angle;
    if (mFEFineIndex > 0) {
      mShiftManager->AutoAlign(1, 0);
      mImBufs->mImage->getShifts(shiftX, shiftY);
      aInv = mShiftManager->CameraToSpecimen(mScope->GetMagIndex());

      // More Y inversion
      movedX = mConSets[mLowMagConSet].binning * 
        (aInv.xpx * shiftX - aInv.xpy * shiftY);
      movedY = mConSets[mLowMagConSet].binning * 
        (aInv.ypx * shiftX - aInv.ypy * shiftY);
    }
    cMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    mScope->GetLDCenteredShift(ISX, ISY);
    specX = cMat.xpx * ISX + cMat.xpy * ISY;
    specY = cMat.ypx * ISX + cMat.ypy * ISY;
    mCumMovedX += movedX;
    mFEFineShifts[mFEFineIndex] = (float)specY;
      
    report.Format("Angle = %.2f, shift = %.1f, %.1f pixels, %.3f, %.3f um"
      /*", Cumul, Y = %.3f"/*, %.3f*/", IS = %.3f, %.3f",
      mFEFineAngles[mFEFineIndex], shiftX, shiftY, movedX, movedY,
      /*mCumMovedX, mFEFineShifts[mFEFineIndex],*/ specX, specY);
    if (mVerbose)
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    radians = DTOR * mFEFineAngles[mFEFineIndex];
    mAngleSines[mFEFineIndex] = -(float)sin(radians);
    mAngleCosines[mFEFineIndex++] = (float)cos(radians);

    // Test for terminating
    if (mFEFineIndex < mFENumFineSteps && 
      fabs((double)(mFEFineShifts[mFEFineIndex - 1] - mFEFineShifts[0])) < mFEMaxFineIS) {
      
      // Not terminating yet: go to next tilt or get next picture
      if (mFEActPostExposure)
        EucentricityFineCapture();
      else {
        mScope->TiltTo(mFETargetAngles[mFEFineIndex]);
        mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_FINE_TILTED, 
          B3DNINT(mStageTimeoutFactor * 30000));
      }
      return;
    }

    // Solve for vertical and horizontal offsets
    if (mFEFineIndex < 3) {
      delY = 0.;
      delZ = (mFEFineShifts[1] - mFEFineShifts[0]) / (mAngleSines[1] - mAngleSines[0]);

    } else {

      // First get old general solution with constant term
      StatLSFit2(mAngleSines, mAngleCosines, mFEFineShifts,
        mFEFineIndex, &delZ, &delY, &intcp);
      yZeroGen = delY + intcp;
      delY = -intcp;
        
      // 3/18/04: This is all useless but can't bear to delete it yet - the general
      // solution gave slightly more consistent fits for lateral offset; the offset
      // is quite sensitive to the assume yzero and so it looks like the error in the
      // measured value of Yzero leads to more variability and error.
      // Find shift at zero tilt if hit or crossed it
      for (i = 1; i < mFEFineIndex; i++) {
        if (fabs(mFEFineAngles[i]) < 0.01) {
          yZero = mFEFineShifts[i];
          break;
        }
        if ((mFEFineAngles[i] < 0. && mFEFineAngles[i - 1] > 0.) ||
          (mFEFineAngles[i] > 0. && mFEFineAngles[i - 1] < 0.)) {
          yZero = (float)((mFEFineAngles[i] * mFEFineShifts[i - 1] - 
            mFEFineAngles[i - 1] * mFEFineShifts[i]) / 
            (mFEFineAngles[i] - mFEFineAngles[i - 1]));
          break;
        }
      }

      // If got a shift, adjust to fit without a constant term
      if (i < mFEFineIndex) {
        for (i = 0; i < mFEFineIndex; i++) {
          mFEFineShifts[i] = (mFEFineShifts[i] - yZero);
          mAngleCosines[i] = mAngleCosines[i] - 1;
        }
        StatLSFit2(mAngleSines, mAngleCosines, mFEFineShifts,
          mFEFineIndex, &delZact, &delYact, NULL);
        delYact -= yZero;
      } else {
        yZero = yZeroGen;
        delZact = delZ;
        delYact = delY;
      }

      report.Format("General solution delZ %.2f, delY %.2f, yZero %.2f\r\n"
        " solution with assumed YZero %.2f: delZ %.2f, delY %.2f", delZ, delY,
        yZeroGen, yZero, delZact, delYact);
      if (mVerbose)
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }

    // Issue report after negating delY for consistency with old usage
    delY = -delY;
    if (mShiftManager->GetStageInvertsZAxis())
      delZ = -delZ;
    report.Format("Refining eucentricity: Fit to %d positions gives tilt axis Z"
      " displacement\r\n of %.2f microns and lateral displacement of %.2f microns", 
      mFEFineIndex, delZ, delY);
    mFEIterationCount++;
    mRepeatFine = (float)mFEFineIndex / mFENumFineSteps < 0.74 && 
      mFEIterationCount < mFEIterationLimit;
    
    // Save the dely as the last axis offset if really finished
    if (!mRepeatFine)
      mLastAxisOffset = delY + 
        (float)(mScope->GetShiftToTiltAxis() ? mScope->GetTiltAxisOffset() : 0.);
    action = mVerbose ? LOG_OPEN_IF_CLOSED : LOG_SWALLOW_IF_CLOSED;
    if (mVerbose || !mRepeatFine || mHitachiWithoutZ)
      mWinApp->AppendToLog(report, action);
    else if (mRepeatFine) {
      report.Format("Refining eucentricity: changing Z by %.2f microns and restarting",
        -delZ);
      mWinApp->AppendToLog(report, action);
    }
    if (mHitachiWithoutZ) {
      ReportManualZChange(delZ, "Refine");
      StopEucentricity();
      return;
    }

    mScope->GetStagePosition(stageX, stageY, mFECurrentZ);
    
    backlashZ = 0;
    if (-delZ * mFEBacklashZ > 0)
      backlashZ = mFEBacklashZ;
    if (mWinApp->GetSTEMMode())
      mScope->IncDefocus(-delZ / mWinApp->mFocusManager->GetSTEMdefocusToDelZ(-1));
    DoubleMoveStage(mFECurrentZ - delZ, backlashZ, true, 0., 0., false, 
      FE_FINE_LAST_MOVE);
    return;

  case FE_FINE_LAST_MOVE:

    // After last move, repeat if needed, or tilt for align shot, or stop
    if (mRepeatFine)
      FindEucentricity(FIND_EUCENTRICITY_FINE);
    else if (mFECoarseFine & REFINE_EUCENTRICITY_ALIGN) {
      mScope->TiltTo(mFEUsersAngle);
      mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_FINE_ALIGNTILT, 
        B3DNINT(mStageTimeoutFactor * 30000));
    } else
      StopEucentricity();
    return;

  case FE_FINE_ALIGNTILT:
    if (mFEUseTrialInLD && mWinApp->LowDoseMode())
      mLowMagConSet = 0;
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_FINE_ALIGNSHOT2, 0);
    return;

  case FE_FINE_ALIGNSHOT2:

    // Align to saved buffer and stop
    mShiftManager->AutoAlign(mWalkAlignBuffer, 0);
    StopEucentricity();
    return;
  }
}
void CComplexTasks::EucentricityFineCapture()
{
  StageMoveInfo smi;
  int delay;

  // Set up a tilt if allowed and there is one more to do
  if (mFEActPostExposure && mFEFineIndex + 1 < mFENumFineSteps) {
    smi.axisBits = axisA;
    smi.alpha = mFETargetAngles[mFEFineIndex + 1];
    delay = mShiftManager->GetAdjustedTiltDelay(fabs((double)
      (mFETargetAngles[mFEFineIndex + 1] - mFEFineAngles[mFEFineIndex])));
    mCamera->QueueStageMove(smi, delay);
  } 
  
  // Start capture and return
  StartCaptureAddDose(mLowMagConSet);
  mWinApp->AddIdleTask(TASK_EUCENTRICITY, FE_FINE_SHOT, 0);

}

// Move stage in Z and/or in tilt, potentially doing two moves with backlash correction
// This routine can only be called for eucentricity
void CComplexTasks::DoubleMoveStage(double finalZ, float backlashZ, BOOL doZ,
                  double finalTilt, float backlashTilt, BOOL doTilt,
                  int nextAction)
{
  StageMoveInfo smi;
  if (mScope->WaitForStageReady(5000)) {
    mTSController->TSMessageBox("Stage busy when trying to move stage");
    StopEucentricity();
    mWinApp->ErrorOccurred(1);
    return;
  }
  
  smi.axisBits = 0;
  if (doZ) {
    smi.axisBits |= axisZ;
    smi.z = finalZ;
    smi.backZ = backlashZ;
  }
  if (doTilt) {
    smi.axisBits |= axisA;
    smi.alpha = finalTilt;
    smi.backAlpha = backlashTilt;
  }
  mScope->MoveStage(smi, backlashZ != 0. || backlashTilt != 0.);
  mWinApp->AddIdleTask(TASK_EUCENTRICITY, nextAction, 
    B3DNINT(mStageTimeoutFactor * (HitachiScope ? 120000 : 30000)));
}


int CComplexTasks::EucentricityBusy(void)
{
  if (mShiftManager->ResettingIS())
    return 1;
  return SEMStageCameraBusy();
}

void CComplexTasks::EucentricityCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out in the eucentricity routine"));
  StopEucentricity();
  mWinApp->ErrorOccurred(error);
}

void CComplexTasks::StopEucentricity()
{
  if (!mDoingEucentricity)
    return;
  RestoreMagIfNeeded();
  mDoingEucentricity = false;
  if (!mScope->WaitForStageReady(10000)) {
    mScope->TiltTo(mFEUsersAngle);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  }
  mCamera->SetRequiredRoll(0);
  mCamera->SetObeyTiltDelay(false);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

void CComplexTasks::ReportManualZChange(float delZ, const char *roughFine)
{
  CString mess;
  int overshoot;
  int backlash = B3DNINT(mManualHitachiBacklash / mZMicronsPerDialMark);
  mess.Format("%s eucentricity: Z change of %.2f microns is needed: %.1f grooves"
    " on knob %s", roughFine, delZ, fabs(delZ / mZMicronsPerDialMark), 
    delZ > 0 ? "left" : "right");
  mWinApp->AppendToLog(mess);
  delZ /= mZMicronsPerDialMark;
  if (delZ < 0)
    mess.Format("Turn Z knob right %.1f grooves", fabs(delZ));
  else {
    overshoot = (int)ceil(fabs(delZ));
    mess.Format("Turn Z knob left %.1f grooves\n\n"
    "Or, if you adjusted for backlash, turn it\n"
    " left %d grooves then right %.1f grooves", fabs(delZ), 
    backlash + overshoot, backlash + overshoot - fabs(delZ));
  }
  AfxMessageBox(mess, MB_OK | MB_ICONINFORMATION);
}

////////////////////////////////////////////////////////////////////////
// TILT AFTER STAGE MOVE
////////////////////////////////////////////////////////////////////////

// Start a tilt after stage move or after a reverse tilt that was inadequate
void CComplexTasks::TiltAfterStageMove(float angle, bool reverseTilt)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  int newMagInd = FindMaxMagInd(reverseTilt ? mMinRTField : mMinTASMField, -2);
  int actionStarted = TASM_FIRST_SHOT;
  StageMoveInfo smi;
  ControlSet  *conSet = mConSets + TRACK_CONSET;
  mTASMAngle = angle;

  // Set flag and states now in case of mag change
  mDoingTASM = true;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, reverseTilt ? "TILT AFTER REVERSAL" : 
    "TILT AFTER STAGE MOVE");

  // Make a conset for tracking unless another routine did
  if (!mMagStackInd || mConSetModified[mMagStackInd - 1] != TRACK_CONSET)
    MakeTrackingConSet(conSet, targetSize);

  mCamera->SetRequiredRoll(1);

  LowerMagIfNeeded(newMagInd, 0.75f, 0.5f, TRACK_CONSET);

  mRTActPostExposure = mWinApp->ActPostExposure();
  if (mRTActPostExposure) {
    smi.axisBits = axisA;
    smi.alpha = mTASMAngle;
    mCamera->QueueStageMove(smi, mShiftManager->GetMinTiltDelay());
    actionStarted = TASM_TILTED;
  }

  StartCaptureAddDose(mLowMagConSet);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_TILT_AFTER_MOVE, 
    actionStarted, 0);
}

// Tilt after stage move next task: it is a lot like reverse tilt
void CComplexTasks::TASMNextTask(int param)
{
  int stackInd = mMagStackInd > 0 ? mMagStackInd - 1 : 0;
  if (!mDoingTASM)
    return;
  
  mImBufs->mCaptured = BUFFER_TRACKING;
  switch (param) {
  case TASM_FIRST_SHOT:
    mScope->TiltTo(mTASMAngle);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_TILT_AFTER_MOVE, TASM_TILTED, 120000);
    return;

  case TASM_TILTED:
    if (mRTActPostExposure && mSavedMagInd[stackInd])
      mCamera->QueueMagChange(mSavedMagInd[stackInd]);
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_TILT_AFTER_MOVE, 
      TASM_SECOND_SHOT, 0);
    return;

  case TASM_SECOND_SHOT:
    if (mSkipNextBeamShift)
      mScope->SetSkipNextBeamShift(true);
    mShiftManager->AutoAlign(1, 0);
    mScope->SetSkipNextBeamShift(false);
    
    StopTiltAfterMove();
    return;
  }
}

void CComplexTasks::TASMCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out in the tilt-after-stage-move routine"));
  StopTiltAfterMove();
  mWinApp->ErrorOccurred(error);
}

void CComplexTasks::StopTiltAfterMove()
{
  if (!mDoingTASM)
    return;
  RestoreMagIfNeeded();
  mDoingTASM = false;
  mSkipNextBeamShift = false;
  mCamera->SetRequiredRoll(0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}


////////////////////////////////////////////////////////////////////////
// BACKLASH ADJUST stage position
////////////////////////////////////////////////////////////////////////

// Start the routine with the given backlash to apply, and flags to call Navigator at
// end and to show buffer C when done
void CComplexTasks::BacklashAdjustStagePos(float backX, float backY, bool callNav, 
                                           bool showC)
{
  int targetSize = B3DMAX(512, mCamera->TargetSizeForTasks());
  int newMagInd = FindMaxMagInd(mMinBASPField, -2);
  ControlSet  *conSet = mConSets + TRACK_CONSET;

  // Set flag and states now in case of mag change
  mDoingBASP = 1 + (callNav ? 2 : 0) + (showC ? 4 : 0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "ADJUSTING FOR BACKLASH"); 

  // Make a conset for tracking
  MakeTrackingConSet(conSet, targetSize);
  mCamera->SetRequiredRoll(showC ? 2 : 1);

  LowerMagIfNeeded(newMagInd, 0.75f, 0.5f, TRACK_CONSET);
  mBASPBackX = backX;
  mBASPBackY = backY;
  StartCaptureAddDose(mLowMagConSet);
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_BACKLASH_ADJUST, BASP_FIRST_SHOT, 0); 
}

// Next task for backlash adjust
void CComplexTasks::BASPNextTask(int param)
{
  float shiftX, shiftY;
  CString report;
  StageMoveInfo smi;
  ScaleMat bMat = mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), 
    mScope->FastMagIndex());
  ScaleMat bInv = MatInv(bMat);
  if (!mDoingBASP)
    return;
  
  mImBufs->mCaptured = BUFFER_TRACKING;
  switch (param) {
  case BASP_FIRST_SHOT:
    mScope->GetStagePosition(smi.x, smi.y, smi.z);
    smi.backX = mBASPBackX;
    smi.backY = mBASPBackY;
    smi.axisBits = axisXY;
    mBASPDeltaX = (float)smi.x;
    mBASPDeltaY = (float)smi.y;
    mScope->MoveStage(smi, true);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_BACKLASH_ADJUST, BASP_MOVED, 120000);
    return;

  case BASP_MOVED:
    StartCaptureAddDose(mLowMagConSet);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_BACKLASH_ADJUST, BASP_SECOND_SHOT, 0);
    return;

  case BASP_SECOND_SHOT:
    mShiftManager->AutoAlign(1, 0);
    mScope->GetStagePosition(smi.x, smi.y, smi.z);

    // Get the change in stage position since the start.  This is the right sign because
    // if the 
    mBASPDeltaX = (float)(smi.x - mBASPDeltaX);
    mBASPDeltaY = (float)(smi.y - mBASPDeltaY);

    // Get the alignment shift, convert to a stage shift and subtract it from 
    // overall negative is used as in NavHelper and ShiftManager mouse shifting
    // negative is used for Y component because of Y inversion of image as in NavHelper
    mImBufs->mImage->getShifts(shiftX, shiftY);
    mBASPDeltaX -= mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
    mBASPDeltaY -= mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
    report.Format("Backlash adjustment to stage coordinates is %.2f,  %.2f", mBASPDeltaX,
      mBASPDeltaY);
    mWinApp->AppendToLog(report);
    if ((mDoingBASP & 2) && mWinApp->mNavigator)
      mWinApp->mNavigator->BacklashFinished(mBASPDeltaX, mBASPDeltaY);
    if (mDoingBASP & 4) {
      Sleep(500);
      mWinApp->SetCurrentBuffer(2);
      mWinApp->AppendToLog("The real map is now in Buffer C - be careful to mark points "
        "only on that!");
    }
    StopBacklashAdjust();
    return;
  }

}

void CComplexTasks::BASPCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    mTSController->TSMessageBox(_T("Time out in the adjust-for-backlash routine"));
  StopBacklashAdjust();
  mWinApp->ErrorOccurred(error);
}

void CComplexTasks::StopBacklashAdjust()
{
  if (!mDoingBASP)
    return;
  RestoreMagIfNeeded();
  mDoingBASP = 0;
  mCamera->SetRequiredRoll(0);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}
