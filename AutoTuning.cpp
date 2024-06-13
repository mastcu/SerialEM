// Autotuning.cpp:    Does astigmatism correction and coma-free alignment
//
// Copyright (C) 2003-2018 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "AutoTuning.h"
#include "EMscope.h"
#include "SerialEMView.h"
#include "FocusManager.h"
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "BeamAssessor.h"
#include "NavHelper.h"
#include "MultiShotDlg.h"
#include "CameraController.h"
#include "ExternalTools.h"
#include "Shared\b3dutil.h"
#include "Shared\ctffind.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"


CAutoTuning::CAutoTuning(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mShiftManager = mWinApp->mShiftManager;
  mDoingCalAstig = false;
  mDoingFixAstig = false;
  mDoingComaFree = false;
  mDoingCtfBased = false;
  mAstigCenterFocus = -1.;
  mAstigFocusRange = 10.;
  mCalComaFocus = -1.;
  mAstigToApply = 0.04f;
  mAstigBeamTilt = 3.2f;
  mUsersAstigTilt = -1.;
  mUsersComaTilt = -1.;
  mMaxComaBeamTilt = 10.;
  mPostFocusDelay = 500;
  mPostAstigDelay = 900;
  mPostBeamTiltDelay = 750;
  mAstigIterationThresh = 0.015f;
  mMaxAstigIterations = 2;
  mComaMeanCritFac = 0.5f;
  mZemlinIndex = -1;
  mMontTableau = NULL;
  mSpectrum = NULL;
  mInitialComaIters = 2;
  mNumComaItersDone = 0;
  mMenuZemlinTilt = 6.;
  mMinCtfBasedDefocus = -0.9f;
  mAddToMinForAstigCTF = -0.5f;
  mMaxCtfBasedDefocus = -10.;
  mMinRingsForCtf = 3;
  mMaxRingsForCtf = 20;
  mMinCenteredScore = 0.1f;
  mMinPlotterCenScore = 0.5f;
  mMinScoreRatio = 0.33f;
  mMinPlotterScoreRatio = 0.6f;
  mAstigBacklash = -0.02f;
  mBeamTiltBacklash = -999.;
  mBacklashDelay = 100;
  mGotFocusMinuteStamp = 0;
  mCtfExposure = -1.;
  mCtfDriftSettling = 0.;
  mCtfBinning = 0;
  mCtfUseFullField = true;
  mComaIterationThresh = 1.f;
  mCtfDoFullArray = false;
  mCtfBasedLDareaDelay = 8000;   // Based on coming back from View on an F20
  mTestCtfTuningDefocus = 0.;
  mLastCtfBasedFailed = false;
  mLastXStigNeeded = mLastYStigNeeded = 0.;
  mLastXTiltNeeded = mLastYTiltNeeded = 0.;
  mComaVsISindex = -1;
  mComaVsIScal.magInd = -1;
  mComaVsIScal.spotSize = -1;
  mComaVsISextent = 3.f;
  mComaVsISrotation = 0;
  mComaVsISuseFullArray = 0;
}


CAutoTuning::~CAutoTuning(void)
{
}


void CAutoTuning::Initialize(void)
{
  mScope = mWinApp->mScope;
  mFocusManager = mWinApp->mFocusManager;

  // Overlay the defaults or property settings with the users values if any
  if (mUsersAstigTilt > 0)
    mAstigBeamTilt = mUsersAstigTilt;
  if (mUsersComaTilt > 0)
    mMaxComaBeamTilt = mUsersComaTilt;
}

//////////////////////////////////////////////////////////////////////////
// ASTIGMATISM CALIBRATION
//////////////////////////////////////////////////////////////////////////

// Routine to start the calibration
void CAutoTuning::CalibrateAstigmatism(void)
{
  CString str = TuningCalMessageStart("Astigmatism", "a beam tilt of", mAstigBeamTilt);
  str += "\n\nYou may need to remove the objective aperture if it is in;\n"
    "stop the procedure if it intrudes.\n\nAre you ready to proceed?";
  if (AfxMessageBox(str, MB_QUESTION) != IDYES)
    return;
  GeneralSetup();
  mSavedDefocus = mScope->GetDefocus();
  mScope->GetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
  mOperationInd = -1;
  mDirectionInd = 0;
  mDoingCalAstig = true;
  mFocusManager->AutoFocusStart(-1);
  mWinApp->AddIdleTask(TASK_CAL_ASTIG, 0, 0);
}

// Make the beginning of the initial message for either procedure
CString CAutoTuning::TuningCalMessageStart(const char * procedure, const char *ofUpTo, 
  float beamTilt)
{
  float scaling = mFocusManager->EstimatedBeamTiltScaling();
  CString *names = mWinApp->GetModeNames();
  CString str, str2;
  str.Format("%s calibration will take many %s images.\n"
    "Autofocus needs to be working well with the current parameters.", procedure,
    names[FOCUS_CONSET]);
  if (!FEIscope) {
    str2.Format("\n\nImages will be taken with %s %.2f%%", ofUpTo, beamTilt);
    str += str2;
    if (scaling > 0) {
      str2.Format(",\nwhich is about %.2f milliradians.", scaling * beamTilt);
      str += str2;
    }
  }
  return str;
}

// The next operation for astigmatism
void CAutoTuning::AstigCalNextTask(int param)
{
  AstigCalib cal;
  int calInd;
  float denomFocus = mAstigFocusRange * mAstigBeamTilt;
  float denomAstig = 2.f * mAstigToApply * mAstigBeamTilt;
  float xFacs[4] = {-1., 1., 0., 0.};
  float yFacs[4] = {0., 0., -1., 1.};
  if (!mDoingCalAstig)
    return;

  // Finish up if the last one was gotten
  if (mDirectionInd == 1 && mOperationInd == 5) {
    cal.focusMat.xpx = (mXshift[1][0] - mXshift[0][0]) / denomFocus;
    cal.focusMat.ypx = (mYshift[1][0] - mYshift[0][0]) / denomFocus;
    cal.focusMat.xpy = (mXshift[1][1] - mXshift[0][1]) / denomFocus;
    cal.focusMat.ypy = (mYshift[1][1] - mYshift[0][1]) / denomFocus;
    cal.astigXmat.xpx = (mXshift[3][0] - mXshift[2][0]) / denomAstig;
    cal.astigXmat.ypx = (mYshift[3][0] - mYshift[2][0]) / denomAstig;
    cal.astigXmat.xpy = (mXshift[3][1] - mXshift[2][1]) / denomAstig;
    cal.astigXmat.ypy = (mYshift[3][1] - mYshift[2][1]) / denomAstig;
    cal.astigYmat.xpx = (mXshift[5][0] - mXshift[4][0]) / denomAstig;
    cal.astigYmat.ypx = (mYshift[5][0] - mYshift[4][0]) / denomAstig;
    cal.astigYmat.xpy = (mXshift[5][1] - mXshift[4][1]) / denomAstig;
    cal.astigYmat.ypy = (mYshift[5][1] - mYshift[4][1]) / denomAstig;
    cal.alpha = mScope->GetAlpha();
    cal.beamTilt = mAstigBeamTilt;
    cal.magInd = mScope->GetMagIndex();
    cal.probeMode = mScope->ReadProbeMode();
    cal.defocus = mAstigCenterFocus;
    cal.beamTilt = mAstigBeamTilt;
    PrintMatrix(cal.focusMat, "Defocus");
    PrintMatrix(cal.astigXmat, "X astigmatism");
    PrintMatrix(cal.astigYmat, "Y astigmatism");
    calInd = LookupAstigCal(cal.probeMode, cal.alpha, cal.magInd, true);
    if (calInd < 0)
      mAstigCals.Add(cal);
    else
      mAstigCals[calInd] = cal;
    mWinApp->SetCalibrationsNotSaved(true);
    StopAstigCal();
    return;
  }

  // Otherwise set up for the next measurement - first time, process the autofocus
  if (mOperationInd < 0) {
    if (SaveAndSetDefocus(mAstigCenterFocus- mAstigFocusRange / 2.f)) {
      StopAstigCal();
      mWinApp->ErrorOccurred(1);
      return;
    }
    mFocusManager->SetBeamTilt(mAstigBeamTilt, "from AstigCalNextTask");
    mOperationInd = 0;
    mWinApp->SetStatusText(MEDIUM_PANE, "VARYING DEFOCUS");
  } else if (!mDirectionInd) {
    mDirectionInd = 1;
  } else {
    if (!mOperationInd) {
      mScope->SetDefocus(mSavedDefocus + mAstigCenterFocus + mAstigFocusRange / 2. - 
        mInitialDefocus);
      Sleep(mPostFocusDelay);
    } else {
      if (mOperationInd == 1) {
        mScope->SetDefocus(mSavedDefocus + mAstigCenterFocus - mInitialDefocus);
        mWinApp->SetStatusText(MEDIUM_PANE, "VARYING X STIGMATOR");
      }
      if (mOperationInd == 3)
        mWinApp->SetStatusText(MEDIUM_PANE, "VARYING Y STIGMATOR");
      if (TestAndSetStigmator(mSavedAstigX + xFacs[mOperationInd-1] * mAstigToApply,
        mSavedAstigY + yFacs[mOperationInd - 1] * mAstigToApply, 
        "The next stigmator value to test"))
          return;
      mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
    }
    mDirectionInd = 0;
    mOperationInd++;
  }
  mFocusManager->SetTiltDirection(2 * mDirectionInd);
  mFocusManager->DetectFocus(FOCUS_CAL_ASTIG);
  mWinApp->AddIdleTask(TASK_CAL_ASTIG, 0, 0);
}


void CAutoTuning::StopAstigCal(void)
{
  if (!mDoingCalAstig)
    return;
  mScope->SetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
  mDoingCalAstig = false;
  CommonStop();
}


void CAutoTuning::AstigCalCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting images for calibrating astigmatism"));
  StopAstigCal();
  mWinApp->ErrorOccurred(error);
}


int CAutoTuning::FixAstigmatism(bool imposeChange)
{
  if (LookupAstigCal(mScope->GetProbeMode(), mScope->GetAlpha(), mScope->GetMagIndex())
    < 0)
    return 1;
  GeneralSetup();
  mDirectionInd = 0;
  mOperationInd = 0;
  mDoingFixAstig = true;
  mOperationInd = -1;
  mNumIterations = 0;
  mImposeChange = imposeChange;
  mLastXStigNeeded = mLastYStigNeeded = 0.;
  mScope->GetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
  mFocusManager->AutoFocusStart(-1);
  mWinApp->AddIdleTask(TASK_FIX_ASTIG, 0, 0);
  return 0;
}

void CAutoTuning::FixAstigNextTask(int param)
{
  float xmat[6][4];
  double ss[4][4], denom, focus, xAstig, yAstig, error, errSum, baseX, baseY;
  float xFacs[3] = {1., 0.707107f, 0.};
  float yFacs[3] = {0., 0.707107f, 1.};
  float xFac, yFac, threshFac = 1.;
  double scaleX, scaleY;
  int calInd, ind, j, k;
  AstigCalib cal;

  if (!mDoingFixAstig)
    return;

  calInd = LookupAstigCal(mScope->GetProbeMode(), mScope->FastAlpha(), 
    mScope->FastMagIndex());
  cal = mAstigCals[calInd];
  if (mOperationInd < 2) {
    if (mOperationInd < 0 && !mNumIterations) {
      if (SaveAndSetDefocus(cal.defocus)) {
        StopFixAstig();
        mWinApp->ErrorOccurred(1);
        return;
      }
      mFocusManager->SetBeamTilt(mAstigBeamTilt, "from FixAstigNextTask");
      mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING ASTIGMATISM");
    }
    mOperationInd++;
    mFocusManager->SetTiltDirection(mOperationInd);
    mFocusManager->DetectFocus(FOCUS_ASTIGMATISM);
    mWinApp->AddIdleTask(TASK_FIX_ASTIG, 0, 0);
    return;
  }

  // Load the data matrix
  for (ind = 0; ind < 6; ind += 2) {
    xFac = xFacs[ind / 2] * mAstigBeamTilt;
    yFac = yFacs[ind / 2] * mAstigBeamTilt;
    xmat[ind][0] = cal.focusMat.xpx * xFac + cal.focusMat.xpy * yFac;
    xmat[ind + 1][0] = cal.focusMat.ypx * xFac + cal.focusMat.ypy * yFac;
    xmat[ind][1] = cal.astigXmat.xpx * xFac + cal.astigXmat.xpy * yFac;
    xmat[ind + 1][1] = cal.astigXmat.ypx * xFac + cal.astigXmat.ypy * yFac;
    xmat[ind][2] = cal.astigYmat.xpx * xFac + cal.astigYmat.xpy * yFac;
    xmat[ind + 1][2] = cal.astigYmat.ypx * xFac + cal.astigYmat.ypy * yFac;
    xmat[ind][3] = mXshift[ind / 2][0];
    xmat[ind + 1][3] = mYshift[ind / 2][0];
  }

  // Get the sum of squares and cross-products
  for (j = 0; j < 4; j++) {
    for (k =0; k < 4; k++) {
      ss[j][k] = 0.;
      for (ind= 0; ind < 6; ind++)
        ss[j][k] += xmat[ind][j] * xmat[ind][k];
    }
  }

  // Solution with no  constant term is solution to linear equation with these raw
  // sums of products
  denom = determ3(ss[0][0], ss[1][0], ss[2][0], ss[0][1], ss[1][1], ss[2][1], 
    ss[0][2], ss[1][2], ss[2][2]);
  focus = determ3(ss[3][0], ss[1][0], ss[2][0], ss[3][1], ss[1][1], ss[2][1], 
    ss[3][2], ss[1][2], ss[2][2]) / denom;
  xAstig = determ3(ss[0][0], ss[3][0], ss[2][0], ss[0][1], ss[3][1], ss[2][1], 
    ss[0][2], ss[3][2], ss[2][2]) / denom;
  yAstig = determ3(ss[0][0], ss[1][0], ss[3][0], ss[0][1], ss[1][1], ss[3][1], 
    ss[0][2], ss[1][2], ss[3][2]) / denom;
  errSum = 0.;
  for (ind = 0; ind < 6; ind++) {
    error = xmat[ind][0] * focus + xmat[ind][1] * xAstig + xmat[ind][2] * yAstig -
      xmat[ind][3];
    errSum += error * error;
  }
  error = sqrt(errSum / 6);
  PrintfToLog("Solved Astig X %.3f  Y %.3f  focus %.2f  RMS error %.3f", xAstig, 
    yAstig, focus, error);
  if (mImposeChange) {
    mScope->GetObjectiveStigmator(baseX, baseY);
    if (!mNumIterations) {
      mLastAstigX = xAstig;
      mLastAstigY = yAstig;
    }

    // After first iteration, see how much the solution should have been scaled to reach
    // 0,0.  If scale is too divergent, give up, but only if solution is still above 
    // threshold for iterating and the scale change was on an axis that was big enough
    // before.  If scaling is within safe range, trust it
    // and double the threshold for iterating.  Limit the scaling and apply it
    if (mNumIterations == 1) {
      scaleX = fabs(mLastAstigX) / B3DMAX(0.000001, fabs(mLastAstigX - xAstig));
      scaleY = fabs(mLastAstigY) / B3DMAX(0.000001, fabs(mLastAstigY - yAstig));
      if (sqrt(xAstig * xAstig + yAstig * yAstig) > mAstigIterationThresh &&
        ((fabs(mLastAstigX) > 0.7 * mAstigIterationThresh && 
        (scaleX > 3. || scaleX < 0.6)) || (fabs(mLastAstigY) > 0.7 * mAstigIterationThresh
        && (scaleY > 3. || scaleY < 0.6)))) {
          SEMMessageBox("Astigmatism did not change enough between iterations.\n\n"
            "Restoring original setting");
          mScope->SetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
          StopFixAstig();
          mWinApp->ErrorOccurred(1);
          return;
      }
      if ((scaleX > 0.83 && scaleX < 1.2) || (scaleY > 0.83 && scaleY < 1.2))
        threshFac = 2.;
      B3DCLAMP(scaleX, 0.83, 1.2);
      B3DCLAMP(scaleY, 0.83, 1.2);
      xAstig *= scaleX;
      yAstig *= scaleY;
    }

    // Set new value and see if need to iterate
    if (TestAndSetStigmator(baseX - xAstig, baseY - yAstig, "The new solved stigmator "
      "value"))
        return;

    mScope->SetObjectiveStigmator(baseX - xAstig, baseY - yAstig);
    if (sqrt(xAstig * xAstig + yAstig * yAstig) > threshFac * mAstigIterationThresh && 
      mNumIterations < mMaxAstigIterations) {
        mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
        mNumIterations++;
        mOperationInd = -1;
        PrintfToLog("Iterating because astigmatism is above threshold");
        FixAstigNextTask(0);
        return;
    }
  } else {
    mLastXStigNeeded = -(float)xAstig;
    mLastYStigNeeded = -(float)yAstig;
  }

  StopFixAstig();
}

// Test if a new stigmator value would be out of range and abort if so
int CAutoTuning::TestAndSetStigmator(double stigX, double stigY, const char * descrip)
{
  CString str;
  if (fabs(stigX) > 0.99 || fabs(stigY) > 0.99) {
    str.Format("%s, %.2f X  %.2f Y, would be out of range", descrip, stigX, stigY);
    SEMMessageBox(str);
    mWinApp->ErrorOccurred(1);
    return 1;
  }
  mScope->SetObjectiveStigmator(stigX, stigY);
  return 0;
}

void CAutoTuning::StopFixAstig(void)
{
  if (!mDoingFixAstig)
    return;
  mDoingFixAstig = false;
  CommonStop();
}


void CAutoTuning::FixAstigCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting images for adjusting astigmatism"));
  StopFixAstig();
  mWinApp->ErrorOccurred(error);
}

//////////////////////////////////////////////////////////////////////////
// CALIBRATION OR CORRECTION OF COMA
//////////////////////////////////////////////////////////////////////////

// Routine to start the procedure; calibrate or not; impose change or just measure
int CAutoTuning::ComaFreeAlignment(bool calibrate, int actionType)
{
  int magInd = mScope->GetMagIndex();
  int alpha = mScope->GetAlpha();
  int probe = mScope->GetProbeMode();
  CString str;
  bool continuing = !calibrate && (actionType == COMA_CONTINUE_RUN ||
    actionType == COMA_ADD_ONE_ITER);
  mCalibrateComa = calibrate;

  if (!calibrate && LookupComaCal(probe, alpha, magInd) < 0) {
      SEMMessageBox("There is no coma calibration for the current conditions");
      return 1;
  }
  if (mWinApp->LowDoseMode()) {
    SEMMessageBox("Coma-free operations cannot be run in Low Dose mode");
    return 1;
  }
  if (calibrate) {
    str = TuningCalMessageStart("Coma", "beam tilts up to", mMaxComaBeamTilt);
    str += "\n\nThe objective aperture must not be in for this procedure.\n\n"
      "Are you ready to proceed?";
    if (AfxMessageBox(str, MB_QUESTION) != IDYES)
      return 1;
  }

  if (continuing && !mNumComaItersDone) {
    SEMMessageBox("You cannot iterate coma-free alignment;\n"
      "there is no previous result to combine the new one with");
    return 1;
  }

  if (continuing && (magInd != mLastComaMagInd || probe != mLastComaProbe || 
    alpha != mLastComaAlpha)) {
      str = probe != mLastComaProbe ? "probe mode" : "alpha setting";
      if (magInd != mLastComaMagInd )
        str = "magnification";
      SEMMessageBox(CString("You cannot iterate coma-free alignment;\n"
        "the ") + str + " has changed");
      return 1;
  }

  if (actionType != COMA_CONTINUE_RUN)
    GeneralSetup();
  mDirectionInd = 0;
  mDoingComaFree = true;
  mOperationInd = -1;
  mComaActionType = actionType;
  mScope->SetLDCenteredShift(0., 0.);
  if (!calibrate && actionType == COMA_INITIAL_ITERS) {
    mMaxComaIters = mInitialComaIters;
    mNumComaItersDone = 0;
    mCumulTiltChangeX = mCumulTiltChangeY = 0.;
    mLastComaAlpha = alpha;
    mLastComaMagInd = magInd;
    mLastComaProbe = probe;
  } else if (!calibrate && actionType == COMA_ADD_ONE_ITER) {
    mMaxComaIters++;
  }
  if (actionType != COMA_CONTINUE_RUN)
    mFocusManager->AutoFocusStart(-1);
  mWinApp->AddIdleTask(TASK_COMA_FREE, 0, 0);
  return 0;
}

// Do the next operation for coma calibration or correction
void CAutoTuning::ComaFreeNextTask(int param)
{
  float xFacs[4] = {-1., 1., 0., 0.};
  float yFacs[4] = {0., 0., -1., 1.};
  float xx1[8], xx2[4], yy[4];
  double xTiltFac, yTiltFac, delBSX, delBSY, maxTerm = 0.;
  float diffSum = 0., diffMax = 0., misAlignX = 0., misAlignY = 0.;
  ComaCalib cal;
  CString mess;
  int calInd, ind, idir;
  if (!mDoingComaFree)
    return;

  // Get the existing calibration, with exact match to beam tilt if calibrating
  calInd = LookupComaCal(mScope->ReadProbeMode(), mScope->FastAlpha(), 
    mScope->FastMagIndex(), mCalibrateComa);
  if (mDirectionInd == 1 && (mOperationInd == 7 || 
    (!mCalibrateComa && mOperationInd == 1))) {

      // Finished: Compute displacement differences for the two sides (two operations) in
      // the same tilt direction, which are the basic unit of operation
      if (mCalibrateComa)
        SEMTrace('c', "Oper dir displacement difference in nm");
      for (ind = 0; ind <= mOperationInd; ind += 2) {
        for (idir = 0; idir < 2; idir++) {
          mXshift[ind][idir] = mXshift[ind + 1][idir] - mXshift[ind][idir];
          mYshift[ind][idir] = mYshift[ind + 1][idir] - mYshift[ind][idir];
          if (mCalibrateComa)
            SEMTrace('c', "%d  %d  %.2f  %.2f", ind, idir, mXshift[ind][idir], 
            mYshift[ind][idir]);
        }
      }
      if (mCalibrateComa) {

        // Get the matrix components by differences in displacement differences
        cal.comaXmat.xpx = (mXshift[2][0] - mXshift[0][0]) / mMaxComaBeamTilt;
        cal.comaXmat.ypx = (mYshift[2][0] - mYshift[0][0]) / mMaxComaBeamTilt;
        cal.comaXmat.xpy = (mXshift[6][0] - mXshift[4][0]) / mMaxComaBeamTilt;
        cal.comaXmat.ypy = (mYshift[6][0] - mYshift[4][0]) / mMaxComaBeamTilt;
        cal.comaYmat.xpx = (mXshift[2][1] - mXshift[0][1]) / mMaxComaBeamTilt;
        cal.comaYmat.ypx = (mYshift[2][1] - mYshift[0][1]) / mMaxComaBeamTilt;
        cal.comaYmat.xpy = (mXshift[6][1] - mXshift[4][1]) / mMaxComaBeamTilt;
        cal.comaYmat.ypy = (mYshift[6][1] - mYshift[4][1]) / mMaxComaBeamTilt;

        // Set other parameters and add or replace the calibration
        cal.beamTilt = mMaxComaBeamTilt;
        cal.probeMode = mScope->GetProbeMode();
        cal.alpha = mScope->GetAlpha();
        cal.magInd = mScope->GetMagIndex();
        cal.defocus = mCalComaFocus;
        if (calInd < 0)
          mComaCals.Add(cal);
        else {

          // First collect data on the difference from the last calibration
          xx1[0] = (float)fabs(cal.comaXmat.xpx - mComaCals[calInd].comaXmat.xpx);
          xx1[1] = (float)fabs(cal.comaXmat.xpy - mComaCals[calInd].comaXmat.xpy);
          xx1[2] = (float)fabs(cal.comaXmat.ypx - mComaCals[calInd].comaXmat.ypx);
          xx1[3] = (float)fabs(cal.comaXmat.ypy - mComaCals[calInd].comaXmat.ypy);
          xx1[4] = (float)fabs(cal.comaYmat.xpx - mComaCals[calInd].comaYmat.xpx);
          xx1[5] = (float)fabs(cal.comaYmat.xpy - mComaCals[calInd].comaYmat.xpy);
          xx1[6] = (float)fabs(cal.comaYmat.ypx - mComaCals[calInd].comaYmat.ypx);
          xx1[7] = (float)fabs(cal.comaYmat.ypy - mComaCals[calInd].comaYmat.ypy);
          ACCUM_MAX(maxTerm, fabs(cal.comaXmat.xpx));
          ACCUM_MAX(maxTerm, fabs(cal.comaXmat.xpy));
          ACCUM_MAX(maxTerm, fabs(cal.comaXmat.ypx));
          ACCUM_MAX(maxTerm, fabs(cal.comaXmat.ypy));
          ACCUM_MAX(maxTerm, fabs(cal.comaYmat.xpx));
          ACCUM_MAX(maxTerm, fabs(cal.comaYmat.xpy));
          ACCUM_MAX(maxTerm, fabs(cal.comaYmat.ypx));
          ACCUM_MAX(maxTerm, fabs(cal.comaYmat.ypy));
          for (ind = 0; ind < 8; ind++) {
            diffSum += xx1[ind];
            ACCUM_MAX(diffMax, xx1[ind]);
          }
          mComaCals[calInd] = cal;
        }
        PrintMatrix(cal.comaXmat, "X beam tilt");
        PrintMatrix(cal.comaYmat, "Y beam tilt");
        if (calInd >= 0)
          PrintfToLog("Difference from previous calibration as percent of maximum term:"
          "\r\n   mean %.1f%%  maximum %.1f%%", 100. * diffSum / (8. * maxTerm),
          100. * diffMax / maxTerm);
      } else {

        // Solve for misalignment: load the arrays with data for fit
        cal = mComaCals[calInd];
        xx1[0] = cal.comaXmat.xpx;
        xx1[1] = cal.comaXmat.ypx;
        xx1[2] = cal.comaYmat.xpx;
        xx1[3] = cal.comaYmat.ypx;
        xx2[0] = cal.comaXmat.xpy;
        xx2[1] = cal.comaXmat.ypy;
        xx2[2] = cal.comaYmat.xpy;
        xx2[3] = cal.comaYmat.ypy;
        yy[0] = mXshift[0][0];
        yy[1] = mYshift[0][0];
        yy[2] = mXshift[0][1];
        yy[3] = mYshift[0][1];

        // Solve for misalignments with no constant term
        lsFit2(xx1, xx2, yy, 4, &misAlignX, &misAlignY, NULL);
        PrintfToLog("Displacement differences  %.2f  %.2f  %.2f  %.2f  nm", yy[0], yy[1],
          yy[2], yy[3]);
        ReportMisalignment("Solved misalignment =", misAlignX, misAlignY);
        if (mComaActionType != COMA_JUST_MEASURE) {

          // Average new result with previous ones, keep track of total change
          mNumComaItersDone++;
          delBSX = -misAlignX / mNumComaItersDone;
          delBSY = -misAlignY / mNumComaItersDone;
          mScope->IncBeamTilt(delBSX, delBSY);
          mCumulTiltChangeX += delBSX;
          mCumulTiltChangeY += delBSY;
          if (mNumComaItersDone < mMaxComaIters) {
            ComaFreeAlignment(false, COMA_CONTINUE_RUN);
            return;
          }
          if (mNumComaItersDone > 1) {
            mess.Format("Average misalignment from %d iterations =", mNumComaItersDone);
            ReportMisalignment((LPCTSTR)mess, -mCumulTiltChangeX, -mCumulTiltChangeY);
          }
        }
      }
      StopComaFree();
      return;
  }

  // Set up for the next run
  if (mOperationInd < 0) {

    // First time, process the defocus measurement, set beam tilt and direction
    if ((mCalibrateComa || mComaActionType != COMA_CONTINUE_RUN) && 
      SaveAndSetDefocus(mCalibrateComa ? mCalComaFocus : mComaCals[calInd].defocus)) {
      mWinApp->ErrorOccurred(1);
      StopComaFree();
      return;
    }
    mFocusManager->SetBeamTilt(mMaxComaBeamTilt / 4.f, "from ComaFreeNextTask");
    mFocusManager->SetTiltDirection(0);
    if (!mCalibrateComa) {
      mess = "MEASURING COMA";
      if (mMaxComaIters > 1)
        mess.Format("MEASURING COMA - ITER %d", mNumComaItersDone + 1);
      mWinApp->SetStatusText(MEDIUM_PANE, mess);
    }
    mOperationInd = 0;
  } else if (!mDirectionInd) {

    // Just toggle direction after direction 0
    mDirectionInd = 1;
    mFocusManager->SetTiltDirection(2);
  } else {

    // Or advance the operation after direction 1
    mDirectionInd = 0;
    mFocusManager->SetTiltDirection(0);
    mOperationInd++;
  }

  if (mFocusManager->GetLastFailed()) {
    SEMMessageBox("An image did not have the required intensity.\n\n"
      "Do you need to remove the objective aperture?");
    StopComaFree();
    mWinApp->ErrorOccurred(1);
    return;
  }

  // Even and odd operations alternate between the two sides of the central position or
  // of a misaligment
  xTiltFac = (1 - mDirectionInd) * (2 * (mOperationInd % 2) - 1);
  yTiltFac = mDirectionInd * (2 * (mOperationInd % 2) - 1);
  if (mCalibrateComa) {

    // Set up the different misalignment positions 
    misAlignX = xFacs[mOperationInd / 2] * mMaxComaBeamTilt / 2.f;
    misAlignY = yFacs[mOperationInd / 2] * mMaxComaBeamTilt / 2.f;
    if (!mDirectionInd && !(mOperationInd % 2)) {
      mess.Format("TESTING %s%s MISALIGNMENT", ((mOperationInd / 2) % 2) ? "+" : "-",
        mOperationInd < 4 ? "X" : "Y");
      mWinApp->SetStatusText(MEDIUM_PANE, mess);
    }
  }
  //PrintfToLog("oper %d dir %d xtf %f  ytf %f  misX  %f  misY %f", mOperationInd, 
  //  mDirectionInd, xTiltFac, yTiltFac, misAlignX, misAlignY);
  mFocusManager->SetNextTiltOffset(misAlignX + xTiltFac * mMaxComaBeamTilt / 4.,
    misAlignY + yTiltFac * mMaxComaBeamTilt / 4.);
  mFocusManager->SetRequiredBWMean(mComaMeanCritFac * mFirstFocusMean);
  mFocusManager->DetectFocus(FOCUS_COMA_FREE);
  mWinApp->AddIdleTask(TASK_COMA_FREE, 0, 0);
}

// End coma-free routine
void CAutoTuning::StopComaFree(void)
{
  if (!mDoingComaFree)
    return;
  mDoingComaFree = false;
  CommonStop();
}

void CAutoTuning::ComaFreeCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting images for coma-free alignment"));
  StopComaFree();
  mWinApp->ErrorOccurred(error);
}

///////////////////////////////////////////////////////////////////
// SUPPORT ROUTINES
///////////////////////////////////////////////////////////////////

// Callback from detectFocus routine with the unbinned shift: store it in arrays indexed
// by operation and direction
void CAutoTuning::TuningFocusData(float xShift, float yShift)
{
  double driftX, driftY;
  mFocusManager->GetLastDrift(driftX, driftY);
  SEMTrace('c', "UB pixel shift %.1f %.1f at oper %d  dir %d  drift %.1f  %.1f", xShift, 
    yShift, mOperationInd, mDirectionInd, driftX, driftY);
  mXshift[mOperationInd][mDirectionInd] = xShift * mCamToSpec.xpx + 
    yShift * mCamToSpec.xpy;
  mYshift[mOperationInd][mDirectionInd] = xShift * mCamToSpec.ypx + 
    yShift * mCamToSpec.ypy;
}

// Look up an astigmatism calibration at the nearest mag matching probe mode and alpha
int CAutoTuning::LookupAstigCal(int probeMode, int alpha, int magInd, bool matchMag)
{
  MagTable *magTabs = mWinApp->GetMagTable();
  double minRatio = 1.e30, ratio;
  int indMin = -1;
  for (int ind = 0; ind < mAstigCals.GetSize(); ind++) {
    if (mAstigCals[ind].alpha == alpha && mAstigCals[ind].probeMode == probeMode) {
      if (matchMag) {
        if (magInd == mAstigCals[ind].magInd)
          indMin = ind;
      } else {
        ratio = ((double)magTabs[magInd].mag) / magTabs[mAstigCals[ind].magInd].mag;
        if (ratio < 1.)
          ratio = 1. / ratio;
        if (ratio < minRatio) {
          minRatio = ratio;
          indMin = ind;
        }
      }
    }
  } 
  return indMin;
}

// Look up an astigmatism calibration matching probe mode, alpha, and beam tilt, and 
// matching mag if matchMag set, otherwise at nearest mag
int CAutoTuning::LookupComaCal(int probeMode, int alpha, int magInd, bool matchMag)
{
  MagTable *magTabs = mWinApp->GetMagTable();
  double minRatio = 1.e30, ratio;
  int indMin = -1;
  for (int ind = 0; ind < mComaCals.GetSize(); ind++) {
    if (mComaCals[ind].alpha == alpha && mComaCals[ind].probeMode == probeMode && 
      fabs(mComaCals[ind].beamTilt - mMaxComaBeamTilt) <= 0.011) {
        if (matchMag) {
          if (magInd == mComaCals[ind].magInd)
            indMin = ind;
        } else {
          ratio = ((double)magTabs[magInd].mag) / magTabs[mComaCals[ind].magInd].mag;
          if (ratio < 1.)
            ratio = 1. / ratio;
          if (ratio < minRatio) {
            minRatio = ratio;
            indMin = ind;
          }
        }
    }
  } 
  return indMin;
}

// Output a scaleMat
void CAutoTuning::PrintMatrix(ScaleMat mat, const char *descrip)
{
  PrintfToLog("%s matrix: %5g  %5g  %5g  %5g", descrip, mat.xpx, mat.xpy, mat.ypx, 
    mat.ypy);
}

// Universal startup: get camera to specimen matrix in nm, save state
void CAutoTuning::GeneralSetup(void)
{
  mMagIndex = mScope->GetMagIndex();
  mCamToSpec = mShiftManager->CameraToSpecimen(mMagIndex);
  mCamToSpec.xpx *= 1000.f;
  mCamToSpec.xpy *= 1000.f;
  mCamToSpec.ypx *= 1000.f;
  mCamToSpec.ypy *= 1000.f;
  mSavedBeamTilt = mFocusManager->GetBeamTilt();
  mSavedTiltDir = mFocusManager->GetTiltDirection();
  mSavedDefocus = mScope->GetDefocus();
  mSavedPostTiltDelay = mFocusManager->GetPostTiltDelay();
}

// Do common tasks for stopping a routine
void CAutoTuning::CommonStop(void)
{
  mScope->SetDefocus(mSavedDefocus);
  mFocusManager->SetBeamTilt(mSavedBeamTilt, "from autotuning CommonStop");
  mFocusManager->SetTiltDirection(mSavedTiltDir);
  mFocusManager->SetPostTiltDelay(mSavedPostTiltDelay);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Process initial autofocus and do some more general operations
int CAutoTuning::SaveAndSetDefocus(float defocus)
{
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  if (mFocusManager->GetLastFailed() || mFocusManager->GetLastAborted()) {
    SEMMessageBox("Stopping the procedure because the initial focus measurement failed");
    return 1;
  }
  mFirstFocusMean = (mWinApp->mProcessImage->UnbinnedSpotMeanPerSec(imBufs) + 
    mWinApp->mProcessImage->UnbinnedSpotMeanPerSec(imBufs + 1)) / 2.f;
  mInitialDefocus = mFocusManager->GetCurrentDefocus();
  mScope->SetDefocus(mSavedDefocus + defocus - mInitialDefocus);
  Sleep(mPostFocusDelay);
  if (mSavedPostTiltDelay < mPostBeamTiltDelay)
    mFocusManager->SetPostTiltDelay(mPostBeamTiltDelay);
  return 0;
}

void CAutoTuning::ReportMisalignment(const char *prefix, double misAlignX, 
  double misAlignY)
{
  CString mess, mess2;
  mess.Format("%s %.2f  %.2f", (LPCTSTR)prefix, misAlignX, misAlignY);
  if (!FEIscope) {
    float scale = mFocusManager->EstimatedBeamTiltScaling();
    mess2.Format(" %%, about %.2f  %.2f  milliradians", misAlignX * scale, 
      misAlignY * scale);
    mess += mess2;
  } else
    mess += " milliradians";
  mWinApp->AppendToLog(mess);
}

// Apply a beam tilt with backlash if a there is a backlash delay and the flag is set
void CAutoTuning::BacklashedBeamTilt(double btX, double btY, bool doBacklash)
{
  if (mBacklashDelay && doBacklash) {
    GetBeamTiltBacklash();
    mScope->SetBeamTilt(btX + mBeamTiltBacklash, btY + mBeamTiltBacklash);
    Sleep(mBacklashDelay);
  }
  mScope->SetBeamTilt(btX, btY);
}

// Apply a stigmator change with backlash if a there is a delay and the flag is set
void CAutoTuning::BacklashedStigmator(double astX, double astY, bool doBacklash)
{
  if (mBacklashDelay && doBacklash) {
    mAstigBacklash = -fabs(mAstigBacklash);
    mScope->SetObjectiveStigmator(astX + mAstigBacklash, astY + mAstigBacklash);
    Sleep(mBacklashDelay);
  }
  mScope->SetObjectiveStigmator(astX, astY);
}

///////////////////////////////////////////////////////////////////
// ZEMLIN TABLEAU ROUTINES
///////////////////////////////////////////////////////////////////

// Start making the tableau
int CAutoTuning::MakeZemlinTableau(float beamTilt, int panelSize, int cropPix)
{
  if (mCtfExposure < 0) {
    SEMMessageBox("You must set up CTF acquire parameters\n"
      "before running a Zemlin tableau");
    return 1;
  }
  SetRecordConSetForCTF();
  mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(RECORD_CONSET);
  mScope->SetImageShift(0., 0.);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_ZEMLIN, 0, 0);
  mWinApp->SetStatusText(MEDIUM_PANE, "MAKING ZEMLIN TABLEAU");
  mWinApp->mCamera->InitiateCapture(RECORD_CONSET);
  mWinApp->UpdateBufferWindows();
  mZemlinIndex = 0;
  mZemlinBeamTilt = beamTilt;
  mPanelSize = panelSize;
  mCropPix = cropPix;
  return 0;
}

// All of the processing
void CAutoTuning::ZemlinNextTask(int param)
{
  double xFactors[9] = {0., 1., 0.707, 0., -0.707, -1., -0.707, 0., 0.707};
  double yFactors[9] = {0., 0., 0.707, 1., 0.707, 0., -0.707, -1., -0.707};
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  KImage *image = imBufs->mImage;
  int nx, ny, err, iy, xStart, yStart, dirSave, dirTry, mag, probe;
  double tiltX, tiltY, outX, outY, rotAngle = 0.;
  FocusTable focCal;
  CString str;
  if (!image) {
    StopZemlin();
    return;
  }
  if (mZemlinIndex < 0)
    return;
  image->getSize(nx, ny);
  if (!mZemlinIndex) {

    // First time, when you know the image size, set up all the sizes and get the buffers
    mPadSize = 2 * ((B3DMAX(nx, ny) + 1) / 2);
    mPadSize = XCorrNiceFrame(mPadSize, 2, 19);
    mSpecSize = B3DMIN(mPanelSize + mCropPix, mPadSize);
    mSpecSize = 2 * (mSpecSize / 2);
    mPanelSize = B3DMIN(mPanelSize, mSpecSize);
    mPanelSize = 2 * (mPanelSize / 2);
    mCropPix = mSpecSize - mPanelSize;
    mTableauSize = 3 * mPanelSize;
    NewArray2(mMontTableau, short, mTableauSize, mTableauSize);
    NewArray2(mSpectrum, short, mSpecSize, mSpecSize);
    if (!mMontTableau || !mSpectrum) {
      SEMMessageBox("Failed to get memory for assembling Zemlin tableau");
      StopZemlin();
      return;
    }
  }

  // Set index at which to copy the data
  xStart = mPanelSize * (1 + B3DNINT(xFactors[mZemlinIndex]));
  yStart = mPanelSize * (1 - B3DNINT(yFactors[mZemlinIndex]));

  // Get the scaled spectrum
  image->Lock();
  err = spectrumScaled(image->getData(), image->getType(), nx, ny, mSpectrum, mPadSize, 
    mSpecSize, 0, 0., 3, twoDfft);
  image->UnLock();
  if (err) {
    str.Format("Error (%d) getting scaled spectrum for panel %d of Zemlin tableau", err,
      mZemlinIndex);
    SEMMessageBox(str);
    StopZemlin();
    return;
  }

  // Copy it with cropping
  for (iy = 0; iy < mPanelSize; iy++)
    memcpy(mMontTableau + (iy + yStart) * mTableauSize + xStart, 
    mSpectrum + (iy + (mCropPix / 2)) * mSpecSize + mCropPix / 2, 
    sizeof(short) * mPanelSize);

  mZemlinIndex++;
  if (mZemlinIndex < 9) {

    // Try to get a focus calibration at any direction and rotate the beam tilt by
    // the difference between the nominal direction and the image shift direction
    mag = mScope->GetMagIndex();
    probe = mScope->ReadProbeMode();
    dirSave = mFocusManager->GetTiltDirection();
    for (dirTry = 0; dirTry < 4; dirTry++) {
      mFocusManager->SetTiltDirection(dirTry);
      if (mFocusManager->GetFocusCal(mag, mWinApp->GetCurrentCamera(), probe, 
        mScope->FastAlpha(), focCal)) {
          rotAngle = (45. * dirTry - 90.) * DTOR - 
            atan2((double)focCal.slopeY, (double)focCal.slopeX);
          break;
      }
    }
    mFocusManager->SetTiltDirection(dirSave);

    // Set beam tilt for next round
    tiltX = xFactors[mZemlinIndex] * mZemlinBeamTilt;
    tiltY = yFactors[mZemlinIndex] * mZemlinBeamTilt;
    outX = cos(rotAngle) * tiltX - sin(rotAngle) * tiltY;
    outY = sin(rotAngle) * tiltX + cos(rotAngle) * tiltY;
    mScope->SetBeamTilt(mBaseBeamTiltX + outX, mBaseBeamTiltY + outY);
    if (mPostBeamTiltDelay > 0)
      Sleep(mPostBeamTiltDelay);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_ZEMLIN, 0, 0);
    mWinApp->mCamera->InitiateCapture(RECORD_CONSET);
    return;
  }

  // When done, set the image in to A
  iy = B3DMAX(1, B3DNINT(mPadSize / (mPanelSize * 3.)));
  mWinApp->mProcessImage->NewProcessedImage(imBufs, mMontTableau, kSHORT, mTableauSize,
    mTableauSize, iy, BUFFER_FFT);
  mMontTableau = NULL;
  StopZemlin();
}

// Restore settings and clean up for normal or error stop
void CAutoTuning::StopZemlin(void)
{
  ControlSet *recSet = mWinApp->GetConSets() + RECORD_CONSET;
  if (mZemlinIndex < 0)
    return;
  mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  mZemlinIndex = -1;
  delete mMontTableau;
  delete mSpectrum;
  *recSet = mSavedRecSet;
  mMontTableau = NULL;
  mSpectrum = NULL;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

void CAutoTuning::ZemlinCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting images for Zemlin tableau"));
  StopComaFree();
  mWinApp->ErrorOccurred(error);
}

///////////////////////////////////////////////////////////////////
// CTF-BASED ASTIGMATISM/COMA ROUTINES
///////////////////////////////////////////////////////////////////

// The external call to start the operation for calibration or measurement of the given
// type, actionType = 0 to apply, 1 to measure, 2/3 to measure from existing images
int CAutoTuning::CtfBasedAstigmatismComa(int comaFree, bool calibrate, int actionType,
  bool leaveIS, BOOL noMessageBox)
{
  double curDefocus, tryFocus, stageX, stageY, stageZ;
  int maxMinutes = 5;
  float pixel, scanDelta = 0.1f, maxStageFocusChange = 0.05f;
  float minMagChangeInterval = 10000.;
  float ISdelayFactor = 2.f;
  UINT lastTimeOut;
  FloatVec radii;
  ControlSet *recSet = mWinApp->GetConSets() + RECORD_CONSET;
  CString str;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
  BOOL lowDoseMode = mWinApp->LowDoseMode();
  bool needAreaChange = lowDoseMode && mScope->GetLowDoseArea() != RECORD_CONSET;

  // Initialize the calibrations structure and looping variables
  mSkipMessageBox = noMessageBox;
  mDirectionInd = 0;
  mNumIterations = 0;
  mCtfComaFree = comaFree;
  mDoingFullArray = comaFree > 1 && !calibrate;
  mOperationInd = (actionType / 2 == 0) ? -1 : 0;
  mCtfActionType = actionType;
  mCtfCalibrating = calibrate;
  mCtfCal.magInd = lowDoseMode ? ldp->magIndex : mScope->GetMagIndex();
  mCtfCal.numFits = 0;
  mCtfCal.comaType = comaFree > 0;
  mCtfCal.amplitude = mAstigToApply;

  if (comaFree) 
    mLastXTiltNeeded = mLastYTiltNeeded = 0.;
  else
    mLastXStigNeeded = mLastYStigNeeded = 0.;

  if (comaFree && calibrate) {
    SEMMessageBox("There is no calibration procedure for CTF-based coma-free alignment");
    return 1;
  }
  if (!comaFree && !calibrate && LookupCtfBasedCal(comaFree > 0, mCtfCal.magInd, false) 
    < 0) {
      SEMMessageBox("There are no CTF-based astigmatism calibrations");
      return 1;
  }
  if (mCtfExposure < 0) {
    SEMMessageBox("You must set up acquire parameters\n"
      "before running CTF-based astigmatism or coma operations");
      return 1;
  }

  // Massage the control set
  SetRecordConSetForCTF();

  // Find limits to allowed focus range based on number of rings.  Unfortunately the 
  // absolute limits and the ones to use are in negative microns, the trial value is in
  // positive microns because that function takes it that way as an & reference
  mMinDefocusForMag = 0.;
  mMaxDefocusForMag = 0.;
  pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), 
    mCtfCal.magInd) * 1000.f * (float)recSet->binning;
  tryFocus = -mMinCtfBasedDefocus;
  while (tryFocus <= -mMaxCtfBasedDefocus) {
    mWinApp->mProcessImage->DefocusFromPointAndZeros(0., 0, pixel, 0.4f, &radii,tryFocus);
    if (!mMinDefocusForMag && (int)radii.size() >= mMinRingsForCtf)
      mMinDefocusForMag = -(float)tryFocus;

    // In fact it is now complicated to anticipate how many rings will be in the spectrum
    // actually analyzed and it is probably unnecessary because of automatic resampling
    //if ((int)radii.size() > mMaxRingsForCtf)
      //break;
    mMaxDefocusForMag = -(float)tryFocus;
    tryFocus += scanDelta;
  }
  if (actionType / 2 == 0 && (!mMinDefocusForMag || !mMaxDefocusForMag)) {
    str.Format("CTF analysis for this operation requires at least %d Thon rings with a "
      "defocus between %.1f and %.1f um\r\n\r\nThis cannot be obtained at the current "
      "magnification", mMinRingsForCtf, mMinCtfBasedDefocus,
      mMaxCtfBasedDefocus);
    SEMMessageBox(str);
    *recSet = mSavedRecSet;
    return 1;
  }
  
  // Go to Record now so IS and BT are set right and focus is recorded
  if (actionType / 2 == 0 && needAreaChange)
    mScope->GotoLowDoseArea(RECORD_CONSET);

  // Get starting values and apply backlash before first image
  if (comaFree) {

    // Operations with beam tilt:
    mCtfCal.amplitude = mUsersComaTilt <= 0. ? mMaxComaBeamTilt : mUsersComaTilt;
    mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
    BacklashedBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY, true);
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostBeamTiltDelay);
    mLastBeamX = mBaseBeamTiltX;
    mLastBeamY = mBaseBeamTiltY;
  } else {

    // Operations for astigmatism
    mAstigBacklash= -fabs(mAstigBacklash);
    mScope->GetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
    BacklashedStigmator(mSavedAstigX, mSavedAstigY, true);
    mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
    mLastAstigX = mSavedAstigX;
    mLastAstigY = mSavedAstigY;
  }

  // Zero the IS unless flag is set
  if (!leaveIS) {
    mScope->SetLDCenteredShift(0., 0.);
    mShiftManager->SetISTimeOut(ISdelayFactor *
      mShiftManager->GetLastISDelay());
  }
  
  // Decide whether a new autofocus is needed and save current state before starting
  curDefocus = mScope->GetDefocus();
  mScope->GetStagePosition(stageX, stageY, stageZ);
  mDoingCtfBased = true;
  mSkipMeasuringFocus = mWinApp->MinuteTimeStamp() - mGotFocusMinuteStamp <= maxMinutes && 
    fabs(stageX - mLastStageX) < maxStageFocusChange && fabs(stageY - mLastStageY) < 
    maxStageFocusChange && fabs(stageZ - mLastStageZ) < maxStageFocusChange && 
    fabs(curDefocus - mSavedDefocus) < maxStageFocusChange && mCtfCal.magInd == mMagIndex
    && !needAreaChange && 
    SEMTickInterval(mScope->GetInternalMagTime()) > minMagChangeInterval &&
    SEMTickInterval(mScope->GetUpdateSawMagTime()) > minMagChangeInterval;
  mSavedDefocus = curDefocus;
  mLastStageX = (float)stageX;
  mLastStageY = (float)stageY;
  mLastStageZ = (float)stageZ;
  mMagIndex = mCtfCal.magInd;

  // Add to timeout if area changed for coma-free align.  Astig should be OK because of
  // the focusing
  if (actionType / 2 == 0 && needAreaChange && comaFree) {
    lastTimeOut = mShiftManager->GetGeneralTimeOut(RECORD_CONSET);
    if (SEMTickInterval(lastTimeOut) > 0)
      lastTimeOut = GetTickCount();
    mShiftManager->SetGeneralTimeOut(lastTimeOut, mCtfBasedLDareaDelay);
  }
  if (mSkipMeasuringFocus)
    mGotFocusMinuteStamp = mWinApp->MinuteTimeStamp();
  if (actionType / 2 == 0 && !mSkipMeasuringFocus)
    mFocusManager->AutoFocusStart(-1, -1);
  mWinApp->AddIdleTask(TASK_CTF_BASED, 0, 0); 
  mWinApp->UpdateBufferWindows();
  return 0;
}

// Use the CTF-based parameters to set up the Record control set
void CAutoTuning::SetRecordConSetForCTF()
{
  ControlSet *recSet = mWinApp->GetConSets() + RECORD_CONSET;
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  int binInd, realBin;

  mSavedRecSet = *recSet;
  if (mCtfBinning > 0) {
    recSet->binning = mCtfBinning * BinDivisorI(camParam);
    mWinApp->mCamera->FindNearestBinning(camParam, recSet, binInd, realBin);
    recSet->binning = realBin;
  }
  if (mCtfExposure > 0)
    recSet->exposure = mCtfExposure;
  if (mCtfDriftSettling > 0)
    recSet->drift = mCtfDriftSettling;
  if (mCtfUseFullField) {
    recSet->left = recSet->top = 0;
    recSet->right = camParam->sizeX;
    recSet->bottom = camParam->sizeY;
  }
  recSet->mode = SINGLE_FRAME;
  recSet->alignFrames = 0;
  recSet->saveFrames = 0;
  recSet->doseFrac = 0;
  B3DCLAMP(recSet->K2ReadMode, 0, 1);
  if (camParam->K2Type)
    recSet->binning = B3DMAX(2, recSet->binning);
  recSet->processing = GAIN_NORMALIZED;
}

// The next task after autofocus or an image is done
void CAutoTuning::CtfBasedNextTask(int tparm)
{
  CtffindParams param;
  CProcessImage *processImg = mWinApp->mProcessImage;
  CString str, filename, command;
  int ind, calInd, xyInd, minusInd, plusInd, numInLineFit = 0, numInDat = 36;
  EMimageBuffer *fftBuf = mWinApp->GetFFTBufs();
  float scaling = FEIscope ? 1.f : mWinApp->mFocusManager->EstimatedBeamTiltScaling();
  FloatVec radii;
  BOOL usePlotter = processImg->GetTuneUseCtfplotter();
  float fourXfacs[5] = {-1., 1., 0., 0.};
  float fourYfacs[5] = {0., 0., -1., 1.};
  const float sqrt2 = (float)sqrt(2.);
  const float diag = sqrt2 / 2.f;
  float fullArrXfacs[9] = {-1., -diag, 0., diag, 1., diag, 0., -diag, 0.};
  float fullArrYfacs[9] = {0., -diag, -1., -diag, 0., diag, 1., diag, 0.};
  float *xFacs = &fourXfacs[0], *yFacs = &fourYfacs[0];
  int centralInds[4][2] = {{1, 5}, {3, 7}, {2, 6}, {4, 8}};
  float resultsArray[7], solution[4];
  float newFocus = 0, negMicrons, xCoeff, yCoeff, angle, diffPlus, diffMinus, minFocus;
  float intercept, diff1, diff9, rotateImage, astig, minAstig, maxAstig;
  float plotterDef, phase;
  const int maxInLineFit = 50;
  float xfit[maxInLineFit], yfit[maxInLineFit], zfit[maxInLineFit];
  double xmax = 0., ymax = 0., nextXval, nextYval, assumedPosFocus;
  float minDef, maxDef, expectDef, reduction;
  int astigPhase, numOperations = 4;
  bool iterating = false;
  CtfBasedCalib calUse;
  CString suggest = "You may need more exposure time, a different binning, or more "
    "structure or carbon in the field of view.\n" + CString(mCtfComaFree ? "Try less "
      "beam tilt if the Thon rings are not visible on the long axis in beam-tilted "
      "images." : "");

  if (mDoingFullArray) {
    numOperations = 9;
    xFacs = &fullArrXfacs[0];
    yFacs = &fullArrYfacs[0];
  }

  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  if (!mDoingCtfBased)
    return;

  if (mCtfActionType / 2)
    imBufs += B3DCHOICE(mCtfCalibrating || mCtfComaFree, numOperations- mOperationInd, 0);
  if (!imBufs->mImage || imBufs->mCaptured == BUFFER_FFT) {
    ErrorInCtfBased("The buffers do not have the right images to do this operation");
    return;
  }
  str.Format("%s %s", mCtfCalibrating ? "CALIBRATING" : "MEASURING", 
    mCtfComaFree ? "COMA" : "ASTIGMATISM");
  mWinApp->SetStatusText(MEDIUM_PANE, str);

  // Handle initial focus determination
  if (mCtfActionType > 1 && mTestCtfTuningDefocus < 0)
    mInitialDefocus = mTestCtfTuningDefocus;
  if (mOperationInd < 0) {

    // Check and get the focus if actually did it
    if (!mSkipMeasuringFocus) {
      if (mFocusManager->GetLastFailed() || mFocusManager->GetLastAborted()) {
        ErrorInCtfBased("Stopping the procedure because the initial focus measurement "
          "failed");
        return;
      }
      mLastMeasuredFocus = mFocusManager->GetCurrentDefocus();
    }

    // In any case use that measurement to set and constrain initial defocus
    mInitialDefocus = mLastMeasuredFocus;
    minFocus = mMinDefocusForMag + B3DCHOICE(mCtfCalibrating || mCtfComaFree,  
      mAddToMinForAstigCTF, 0.f);
    if (mInitialDefocus > minFocus)
      newFocus = minFocus;
    else if (mInitialDefocus < mMaxDefocusForMag)
      newFocus = mMaxDefocusForMag;
    if (newFocus) {
      mScope->IncDefocus(newFocus - mInitialDefocus);
      mInitialDefocus = newFocus;
      PrintfToLog("Changing defocus to %.2f to get an acceptable number of CTF rings",
        newFocus);
    }
    mOperationInd = 0;
    mGotFocusMinuteStamp = mWinApp->MinuteTimeStamp();

  } else {

    // An image is ready to analyze
    if (processImg->InitializeCtffindParams(imBufs, param)) {
      ErrorInCtfBased("An error occurred setting up Ctffind parameters");
      return;
    }

    // Make box bigger if there are a lot of rings
    assumedPosFocus = -mInitialDefocus;
    if (mOperationInd > 0)
      assumedPosFocus = -0.9f * mCenteredDefocus;
    processImg->DefocusFromPointAndZeros(0., 0, 
      param.pixel_size_of_input_image, 0.4f, &radii, assumedPosFocus);
    if ((int)radii.size() > (2 * mMaxRingsForCtf) / 3)
      param.box_size = 384;

    // Get the default focus range and limit it
    // 11/1/23: change -1.0 -> -1.5, 1.5->2.0 because Chen thought it was losing some runs
    processImg->SetCtffindParamsForDefocus(param, assumedPosFocus, false);
    ACCUM_MAX(param.minimum_defocus, 10000.f * ((float)(assumedPosFocus - 
      B3DMAX(1.5, 0.3 * assumedPosFocus))));
    ACCUM_MIN(param.maximum_defocus, 10000.f * ((float)assumedPosFocus + 2.0f));
    if (usePlotter) {
      expectDef = (float)(mOperationInd ? mCenteredDefocus : -assumedPosFocus);
      if (mOperationInd) {

        // Defocus is lower for the beam-tilted images on F20: this is a compromise value
        // Scanning is not reliable for highly astigmatic images
        expectDef *= 0.75f;
        minDef = maxDef = expectDef;
      } else {
        minDef = -param.minimum_defocus / 10000.f;
        maxDef = -param.maximum_defocus / 10000.f;
      }
      if (processImg->MakeCtfplotterShrMemFile(0, filename, reduction)) {
        ErrorInCtfBased(filename);
        return;
      }
      if (mWinApp->mExternalTools->MakeCtfplotterCommand(filename, reduction, 0, 0.,
        minDef, maxDef, expectDef, 1, 0., mOperationInd ? -1 : 1, 0., 0., 0., 0, str, 
        command)) {
        processImg->DeletePlotterShrMemFile();
        ErrorInCtfBased(command);
        return;
      }
      if (processImg->RunCtfplotterOnBuffer(filename, command, 5000)) {
        ErrorInCtfBased(command);
        return;
      }
      if (mWinApp->mExternalTools->ReadCtfplotterResults(plotterDef, astig, 
        resultsArray[2], phase, astigPhase, resultsArray[5], resultsArray[4], str, 
        command)) {
        ErrorInCtfBased(command);
        return;
      }
      mWinApp->AppendToLog(str);

    } else {
      param.compute_extra_stats = true;
      SEMTrace('1', "rmin %.1f  rmax %.1f  dmin %.0f  dmax %.0f", param.minimum_resolution,
        param.maximum_resolution, param.minimum_defocus, param.maximum_defocus);
      if (processImg->RunCtffind(imBufs, param, resultsArray)) {
        ErrorInCtfBased("An error occurred running the Ctffind operation");
        return;
      }
    }

    // reality check on fit to centered image
    negMicrons = usePlotter ? plotterDef : -(resultsArray[0] + resultsArray[1]) / 20000.f;
    if (!mOperationInd) {
      mCenteredScore = resultsArray[4];
      mCenteredDefocus = negMicrons;
      if (fabs(negMicrons - mInitialDefocus) > 2. || 
        negMicrons > mMinCtfBasedDefocus + 0.5) {
          str.Format("The defocus from Ctf%s, %.2f um, is too far from the\r\n"
            "expected defocus of %.2f or too low to proceed (below %.2f)\r\n"
            "Check that starting defocus really is near %.2f", usePlotter ? 
            "plotter" : "find", negMicrons, mInitialDefocus, mMinCtfBasedDefocus + 0.5,
            mLastMeasuredFocus);

          // To force refocusing on some of these errors
          mGotFocusMinuteStamp -= 60;
          ErrorInCtfBased(str);
          return;
      }
      if (mCenteredScore < (usePlotter ? mMinPlotterCenScore : mMinCenteredScore)) {
        str.Format("The %s from the Ctf%s fit, %.3f, is below the\r\n"
          " minimum (%.3f) and probably too low for a reliable result.\r\n\r\n",
          usePlotter ? "CCC" : "score", usePlotter ? "plotter" : "find", mCenteredScore,
          usePlotter ? mMinPlotterCenScore : mMinCenteredScore); 
        mGotFocusMinuteStamp -= 60;
        ErrorInCtfBased(str + suggest);
        return;
      }
    }

    // Check general results
    if (resultsArray[4] < (usePlotter ? mMinPlotterScoreRatio : mMinScoreRatio) * 
      mCenteredScore) {
      str.Format("The %s from the Ctf%s fit, %.3f, has changed\r\n"
        "by %.2f from the initial fit so fitting may be unreliable", 
        usePlotter ? "CCC" : "score", usePlotter ? "plotter" : "find", resultsArray[4],
        resultsArray[4] / mCenteredScore);
      ErrorInCtfBased(str);
      return;
    }

    // Check fit to resolution, but try to ignore infinite result
    if (!usePlotter && resultsArray[5] > param.minimum_resolution && resultsArray[5] < 
      10. * imBufs->mImage->getWidth() * param.pixel_size_of_input_image) {
      str.Format("The resolution to which Thon rings fit,\r\n"
        "%.0f A is very high and indicates that CTF fitting is not working.\r\n\r\n",
        resultsArray[5]);
      if (!mOperationInd) {
        mGotFocusMinuteStamp -= 60;
        command.Format("Check that starting defocus really is near %.2f\r\n",
          mLastMeasuredFocus);
        suggest = command + suggest;
      }
      ErrorInCtfBased(str + suggest);
      return;
    }
    if (fabs(negMicrons - mCenteredDefocus) > B3DMAX(1., -0.33 * mCenteredDefocus)) {
      str.Format("The mean defocus from this fit, %.2f um, is\r\n"
        "%.2f from the defocus in the initial fit, the fit may be unreliable",
        negMicrons, fabs(negMicrons - mCenteredDefocus));
      if (B3DCHOICE(usePlotter, astig, fabs(resultsArray[0] - resultsArray[1]) / 10000.) >
        1.0)
        str += "\r\n\r\nIf the fit seems good, try a smaller beam tilt to get closer to"
        " coma-free; after it is close you maybe able to raise the beam tilt again";
      ErrorInCtfBased(str);
      return;
    }

    // Store the results for all but first cal astig shot. Convert to negative microns
    if (usePlotter) {
      resultsArray[0] = plotterDef - astig / 2.f;
      resultsArray[1] = plotterDef + astig / 2.f;
    } else {
      resultsArray[0] /= -10000.;
      resultsArray[1] /= -10000.;
    }
    if (mOperationInd > 0 || mCtfComaFree || !mCtfCalibrating) {
      for (ind = 0; ind < 3; ind++)
        mCtfCal.fitValues[3 * mCtfCal.numFits + ind] = resultsArray[ind];
      mCtfCal.numFits++;
    }

    // Display the circles if side-by-side FFT and new images
    // Take FFT if it wasn't done automatically or if side-by-side not open yet
    if (mCtfActionType / 2 == 0 && processImg->GetSideBySideFFT()) {
        if (!processImg->GetAutoSingleFFT() || !mWinApp->mFFTView)
          processImg->GetFFT(imBufs, 1, BUFFER_FFT);
        fftBuf->mCtfFocus1 = -resultsArray[0];
        fftBuf->mCtfFocus2 = -resultsArray[1];
        fftBuf->mCtfAngle = resultsArray[2];
        fftBuf->mMaxRingFreq = processImg->GetDrawExtraCtfRings() ?
          param.pixel_size_of_input_image / resultsArray[5] : 0;
        mWinApp->mFFTView->DrawImage();
    }
        
    // Set up scope for next shot
    if (mOperationInd < numOperations && (mCtfCalibrating || mCtfComaFree)) {
      if (mCtfActionType / 2 == 0 && mCtfComaFree) {
        nextXval = mLastBeamX + xFacs[mOperationInd] * mCtfCal.amplitude;
        nextYval = mLastBeamY + yFacs[mOperationInd] * mCtfCal.amplitude;
        BacklashedBeamTilt(nextXval, nextYval, mOperationInd == 0 ||
          (!mDoingFullArray && mOperationInd == numOperations / 2) || mDoingFullArray);
        mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostBeamTiltDelay);
        //PrintfToLog("Beam tilt set to %.3f %.3f", nextXval, nextYval);
      } else if (mCtfActionType / 2 == 0) {
        nextXval = mSavedAstigX + xFacs[mOperationInd] * mAstigToApply;
        nextYval = mSavedAstigY + yFacs[mOperationInd] * mAstigToApply;
        if (mOperationInd == 0 || mOperationInd == numOperations / 2) {
          if (TestAndSetStigmator(nextXval + mAstigBacklash, nextYval + mAstigBacklash,
            "The next stigmator value to test minus backlash"))
            return;
          Sleep(mBacklashDelay);
        }
        if (TestAndSetStigmator(nextXval, nextYval, "The next stigmator value to test"))
          return;
        mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
      } 
      mOperationInd++;
    } else {

      // or finish up:
      // with calibration of astigmatism
      if (mCtfCalibrating) {
        calInd = LookupCtfBasedCal(mCtfComaFree > 0, mMagIndex, true);
        if (calInd < 0)
          mCtfBasedCals.Add(mCtfCal);
        else
          mCtfBasedCals[calInd] = mCtfCal;
        for (ind = 0; ind < 8; ind++) {
          angle = (float)(22.5 * ind);
          AstigCoefficientsFromCtfFits(mCtfCal.fitValues, angle, 
            mCtfCal.amplitude, xCoeff, yCoeff);
          SEMTrace('c', "%.1f deg: %.4g  %.4g", angle, xCoeff, yCoeff);
        }
        mWinApp->SetCalibrationsNotSaved(true);
        StopCtfBased(true, false);
        return;

      } else {

        // Coma measurement..
        // Loop on pairs of images, measure defocus differences at all angles, average
        // the implied beam tilt zero over all angles
        if (mCtfComaFree) {

          // First check range of astigmatism values.  Defocus is now negative so 
          maxAstig = -10000.;
          minAstig = 10000.;
          for (ind = 0; ind < mCtfCal.numFits; ind++) {
            astig = mCtfCal.fitValues[3 * ind + 1] - mCtfCal.fitValues[3 * ind];
            ACCUM_MIN(minAstig, astig);
            ACCUM_MAX(maxAstig, astig);
          }
          if (maxAstig - minAstig < 0.05) {
            str.Format("The range of astigmatism values (%.2f) is too low for a reliable"
              " coma solution.\r\n\r\n" "You may need to increase the beam tilt set in"
              " \"Set CTF Coma-free Params\".\r\n", maxAstig - minAstig);
            if (mWinApp->mScope->GetUseIllumAreaForC2())
              str += "(Do not try to detect coma on a Cs-corrected microscope)";
            ErrorInCtfBased(str);
            mWinApp->AppendToLog(str);
            return;
          }

          for (xyInd = 0; xyInd < (mDoingFullArray ? 4 : 2); xyInd++) {
            xmax = 0.;
            ymax = 0.;
            solution[xyInd] = 0.;
            numInLineFit = 0;
            for (ind = 0; ind < numInDat; ind++) {
              angle = (float)((ind * 180.) / numInDat);
              if (mDoingFullArray) {
                minusInd = centralInds[xyInd][0];
                diff1 = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, minusInd, 0, angle);
                diff9 = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, minusInd, 9, angle);
                if (mCtfActionType < 3)
                  diffMinus = (diff1 + diff9) / 2.f;
                else if (mCtfActionType == 3) 
                  diffMinus = (float)(((9. - minusInd) * diff1 + minusInd * diff9) / 9.);
                else
                  diffMinus = minusInd < 5 ? diff1 : diff9;
                plusInd = centralInds[xyInd][1];
                diff1 = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, 0, plusInd, angle);
                diff9 = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, 9, plusInd, angle);
                if (mCtfActionType < 3)
                  diffPlus = (diff1 + diff9) / 2.f;
                else if (mCtfActionType == 3) 
                  diffPlus = (float)(((9. - minusInd) * diff1 + minusInd * diff9) / 9.);
                else
                  diffPlus = minusInd < 5 ? diff1 : diff9;
              } else {
                diffMinus = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, 1 + xyInd * 2, 0,
                  angle);
                diffPlus = DefocusDiffFromTwoCtfFits(mCtfCal.fitValues, 0, 2 + xyInd * 2,
                  angle);
              }
              xfit[ind] = -(diffPlus + diffMinus);

              // This slope is specific to this amplitude of tilt
              yfit[ind] = (diffPlus - diffMinus) / mCtfCal.amplitude;
              ACCUM_MAX(xmax, fabs(xfit[ind]));
              ACCUM_MAX(ymax, fabs(yfit[ind]));

              if (GetDebugOutput('c') && (ind % 4) == 0)
                PrintfToLog("%5.1f %8.4f  %8.4f  %8.4f  %8.4f", angle,  diffMinus,
                  diffPlus, xfit[ind], yfit[ind]);

            }

            // Use data only if measurement-based slopes above a criterion
            // Does not happen in practice and probably would bias the result
            for (ind = 0; ind < numInDat; ind++) {
              if (fabs(yfit[ind]) > 0.25 * ymax) {
                numInLineFit++;
                solution[xyInd] += -xfit[ind] / (2.f * yfit[ind]);
                angle = (float)((ind * 180.) / numInDat);
                if (GetDebugOutput('c') && (ind % 4) == 0)
                  PrintfToLog("%5.1f %9.5f  %9.5f  %9.5f", angle, xfit[ind], 
                    yfit[ind] , -xfit[ind] / (2.f * yfit[ind]));
              }
            }
            solution[xyInd] /= numInLineFit;
            SEMTrace('1', "axis %d  n = %d  solution %.4g", xyInd, 
              numInLineFit, solution[xyInd]);
          }

          // Rotate the diagonal results and average with the X and Y axis results
          if (mDoingFullArray) {
            xCoeff = (solution[2] - solution[3]) / sqrt2;
            solution[3] = (solution[2] + solution[3]) / sqrt2;
            solution[2] = xCoeff;
            SEMTrace('1', "Rotated diagonal solution: %.4g  %.4g", solution[2], 
              solution[3]);
            solution[0] = (solution[0] + solution[2]) / 2.f;
            solution[1] = (solution[1] + solution[3]) / 2.f;
          }

 
          // Output results and apply them; use ill-fated calibration if it exists
          str.Format("Beam tilt %s adjusted by", mCtfActionType ? "needs to be" : "was");
          ReportMisalignment((LPCTSTR)str, -solution[0], -solution[1]);
          if (mCtfActionType) {
            mLastXTiltNeeded = -solution[0];
            mLastYTiltNeeded = -solution[1];
          }
          nextXval = mLastBeamX - (mCtfActionType ? 0. : solution[0]);
          nextYval = mLastBeamY - (mCtfActionType ? 0. : solution[1]);
          if (mCtfActionType / 2 == 0) {
            BacklashedBeamTilt(nextXval, nextYval, true);
          }

            // Check if iterating
          if (!mCtfActionType && !mNumIterations && 
            sqrt((double)solution[0] * solution[0] + solution[1] * solution[1]) * scaling
            > mComaIterationThresh) {
              mNumIterations++;
              mLastBeamX = nextXval;
              mLastBeamY = nextYval;
              mShiftManager->SetGeneralTimeOut(GetTickCount(), 
                mPostBeamTiltDelay);
              mOperationInd = 0;
              mCtfCal.numFits = 0;
              PrintfToLog("Iterating because beam tilt is above threshold");
              iterating = true;
          }

        } else {

          // Astigmatism measurement: Lookup calibration and get angle that image
          // would need to be rotated
          calInd = LookupCtfBasedCal(mCtfComaFree > 0, mMagIndex, false, &rotateImage);
          calUse = mCtfBasedCals[calInd];

          // fit defocus versus coefficients at each angle
          for (ind = 0; ind < numInDat; ind++) {
            angle = (float)((ind * 180.) / numInDat);
            AstigCoefficientsFromCtfFits(calUse.fitValues, angle, 
              calUse.amplitude, xfit[ind], yfit[ind]);

            // Since we haven't rotated the image to match calib, look for defocus at 
            // calib angle rotated back by the image's rotation
            zfit[ind] = DefocusFromCtfFit(mCtfCal.fitValues, angle - rotateImage);
          }

          // Report and apply
          lsFit2(xfit, yfit, zfit, numInDat, &solution[0], &solution[1], &intercept);
          PrintfToLog("Objective stigmator %s adjusted by %.4f  %.4f", 
            mCtfActionType ? "needs to be" : "was", -solution[0], -solution[1]);
          if (mCtfActionType) {
            mLastXStigNeeded = -solution[0];
            mLastYStigNeeded = -solution[1];
          }
          if (!mCtfActionType) {
            nextXval = mLastAstigX - solution[0];
            nextYval = mLastAstigY - solution[1];
            TestAndSetStigmator(nextXval + mAstigBacklash, nextYval + mAstigBacklash,
              "The new solved stigmator value minus backlash");
            Sleep(mBacklashDelay); 
            TestAndSetStigmator(nextXval, nextYval,"The new solved stigmator value");

            // Check if iterating
            if (!mNumIterations && sqrt((double)solution[0] * solution[0] + solution[1] *
              solution[1]) > mAstigIterationThresh) {
                mNumIterations++;
                mLastAstigX = nextXval;
                mLastAstigY = nextYval;
                mShiftManager->SetGeneralTimeOut(GetTickCount(),mPostAstigDelay);
                mOperationInd = 0;
                mCtfCal.numFits = 0;
                PrintfToLog("Iterating because astigmatism is above threshold");
                iterating = true;
            }
          }
        }
        if (!iterating) {
          StopCtfBased(false, false);
          return;
        }
      }
    }
  }

  if (mCtfActionType / 2 == 0)
    mWinApp->mCamera->InitiateCapture(RECORD_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CTF_BASED, 0, 0);
}

void CAutoTuning::ErrorInCtfBased(const char * mess)
{
  if (mSkipMessageBox)
    PrintfToLog("WARNING: %s", mess);
  else
    SEMMessageBox(mess);
  StopCtfBased();
}

// Stop function: restore initial values if flag set (default true)
void CAutoTuning::StopCtfBased(bool restore, bool failed)
{
  ControlSet *recSet = mWinApp->GetConSets() + RECORD_CONSET;
  if (!mDoingCtfBased)
    return;
  mScope->SetDefocus(mSavedDefocus);
  if (mCtfComaFree && restore)
    mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  else if (mCtfCalibrating && restore)
    mScope->SetObjectiveStigmator(mSavedAstigX, mSavedAstigY);
  *recSet = mSavedRecSet;
  mDoingCtfBased = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mLastCtfBasedFailed = failed;
}


void CAutoTuning::CtfBasedCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout getting images for CTF-based astigmatism/coma operation"));
  StopCtfBased();
  mWinApp->ErrorOccurred(1);
}

// Returns the defocus at a particular angle implied by the given CTF fit 
// The equation here is from Rohou and Grigorieff, 2015
 // 0.5 * (df1 + df2 + (df1 - df2) * cos(2 * (angle - axis)))
 float CAutoTuning::DefocusFromCtfFit(float *fitValues, float angle)
{
  float axis = fitValues[2];
  return (float)(0.5 * (fitValues[0] + fitValues[1] + (fitValues[0] - fitValues[1]) *
    cos(DTOR * 2. * (angle - axis))));
}

// Returns the difference in defocus at a particular angle between two fits
float CAutoTuning::DefocusDiffFromTwoCtfFits(float *fitValues, int index1, int index2, 
  float angle)
{
  return DefocusFromCtfFit(&fitValues[3 * index2], angle) - 
    DefocusFromCtfFit(&fitValues[3 * index1], angle);
}

// Returns the two coefficients of astigmatism at a particular angle from the 4 fits
void CAutoTuning::AstigCoefficientsFromCtfFits(float *fitValues, float angle, float astig,
  float &xCoeff, float &yCoeff)
{
  xCoeff = DefocusDiffFromTwoCtfFits(fitValues, 0, 1, angle) / (2.f * astig);
  yCoeff = DefocusDiffFromTwoCtfFits(fitValues, 2, 3, angle) / (2.f * astig);
}

// Set default backlash and make sure it is negative, return value
float CAutoTuning::GetBeamTiltBacklash()
{
  static bool first = true;
  if (mBeamTiltBacklash < -900) {
    float scaling = mWinApp->mFocusManager->EstimatedBeamTiltScaling();
    mBeamTiltBacklash = -1.0f;
    if (!FEIscope && scaling > 0.)
      mBeamTiltBacklash /= scaling;
  }
  mBeamTiltBacklash = -fabs(mBeamTiltBacklash);
  if (first)
    SEMTrace('1', "Beam tilt backlash %.3f  delay  %d", mBeamTiltBacklash,mBacklashDelay);
  first = false;
  return mBeamTiltBacklash;
}

// Look up a calibration is the array, specific to the mag or from the closest mag
// Currently only astigmatism cals are saved to file
int CAutoTuning::LookupCtfBasedCal(bool coma, int magInd, bool matchMag,
  float *rotateImage)
{
  MagTable *magTabs = mWinApp->GetMagTable();
  double minRatio = 1.e30, ratio;
  int indMin = -1;
  double magAngle, calAngle;
  magAngle = mShiftManager->GetImageRotation(mWinApp->GetCurrentCamera(),magInd);
  for (int ind = 0; ind < mCtfBasedCals.GetSize(); ind++) {
    if (BOOL_EQUIV(mCtfBasedCals[ind].comaType, coma)) {
      if (matchMag) {
        if (magInd == mCtfBasedCals[ind].magInd)
          indMin = ind;
      } else {
        ratio = ((double)magTabs[magInd].mag) / magTabs[mCtfBasedCals[ind].magInd].mag;
        if (ratio < 1.)
          ratio = 1. / ratio;
        if (ratio < minRatio) {
          minRatio = ratio;
          indMin = ind;
          calAngle = mShiftManager->GetImageRotation(mWinApp->GetCurrentCamera(),
            mCtfBasedCals[ind].magInd);
          if (rotateImage)
            *rotateImage = (float)(calAngle - magAngle);
        }
      }
    }
  } 
  return indMin;
}

// Get user's values for acquisition parameters, calling the check function first to get
// an initial message if they have never been set up (unless called from there)
int CAutoTuning::SetupCtfAcquireParams(bool fromCheck)
{
  float val = B3DMAX(0.f, mCtfExposure);
  int full = mCtfUseFullField ? 1 : 0;
  if (!fromCheck)
    CheckAndSetupCtfAcquireParams("CTF-based astigmatism or coma-free operations", true);
  if (!KGetOneFloat("Exposure time (in sec), or 0 to use current Record exposure:", 
    val, 2))
    return 1;
  mCtfExposure = B3DMAX(0, val);
  if (!KGetOneInt("Binning, or 0 to use current Record binning", mCtfBinning))
    return 1;
  if (!KGetOneFloat("Drift settling time (in sec), or 0 to use current Record value",
    mCtfDriftSettling, 2))
    return 1;
  if (!KGetOneInt("1 to take full-field images or 0 to use current Record subarea",
    full))
    return 1;
  mCtfUseFullField = full != 0;
  if (!KGetOneFloat("Smallest defocus allowed for correcting astigmatism (must be at"
    " least -0.1)", mMinCtfBasedDefocus, 2))
    return 1;
  B3DCLAMP(mMinCtfBasedDefocus, -10.f, -0.1f);
  val = -mAddToMinForAstigCTF;
  if (!KGetOneFloat("Amount to increase underfocus when calbrating astigmatic or "
    "measuring coma (must be at least 0.2)", val, 2))
    return 1;
  mAddToMinForAstigCTF = -val;
  B3DCLAMP(mAddToMinForAstigCTF, -10.f, -0.2f);
  return 0;

}

// Check whether the acquisition parameters have ever been set up, and if not, call to
// do so, unless called from there
int CAutoTuning::CheckAndSetupCtfAcquireParams(const char *operation, bool fromSetup)
{
  CString str;
  if (mCtfExposure >= 0.)
    return 0;
  str = CString("You must set the exposure time, binning,\n"
    "and image area to acquire before ") + operation + ".\n\n"
    "Images must give FFTs with clear enough Thon rings\n"
    "to allow reliable 2-D CTF analysis so that the\n "
    "astigmatism in the image can be determined.\n\n"
    "For each parameter, you will have the choice\n"
    "of setting a specific value or using the current Record setting";
  AfxMessageBox(str, MB_OK | MB_ICONINFORMATION);
  if (fromSetup)
    return 0;
  return SetupCtfAcquireParams(true);
}

// Calibrate the dependence of beam tilt and astigmatism on image shift
int CAutoTuning::CalibrateComaVsImageShift(float extent, int rotation, int useFullArray)
{
  CString str;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;

  int magIndex = mWinApp->LowDoseMode() ? ldp->magIndex : mScope->FastMagIndex();
  ScaleMat aMat = mShiftManager->IStoSpecimen(magIndex);
  if (!aMat.xpx) {
    SEMMessageBox("Beam tilt versus image shift cannot be calibrated.\n"
      "There is no image shift to specimen calibration available at this magnification");
    return 1;
  }

  // Check extent
  extent = (float)fabs(extent);
  if (extent < MIN_COMA_VS_IS_EXTENT || extent > MAX_COMA_VS_IS_EXTENT) {
    str.Format("The value for image shift extent, %.1f, is out of the allowed range"
      " (%.1f to %.1f)", extent, MIN_COMA_VS_IS_EXTENT, MAX_COMA_VS_IS_EXTENT);
    SEMMessageBox(str, MB_EXCLAME);
    return 1;
  }

  // Go to Record, set variables to use
  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(RECORD_CONSET);
  mCVISextentToUse = extent;
  mCVISrotationToUse = rotation;
  mCVISfullArrayToUse = useFullArray;
  mComaVsISindex = 0;
  mLastCtfBasedFailed = false;
  mScope->GetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  mWinApp->UpdateBufferWindows();
  ComaVsISNextTask(0);
  return 0;
}

// An operation is done, start the next one
void CAutoTuning::ComaVsISNextTask(int param)
{
  float delayFactor = 2.;
  float delISX1, delISY1, delISX2, delISY2, denom;
  int posIndex, index = mComaVsISindex;
  int magInd = mScope->FastMagIndex();
  int useFullFac = 1;
  bool skipBT = mWinApp->mNavHelper->GetSkipAstigAdjustment() < 0;
  if (mCVISfullArrayToUse > 0 || (mCVISfullArrayToUse < 0 && mCtfDoFullArray))
    useFullFac = 2;

  if (index < 0)
    return;

  // Save results of last measurement or stop if it failed
  if (index > 0) {
    if (mLastCtfBasedFailed) {
      StopComaVsISCal();
      return;
    }

    // On odd indexes, record the beam tilt needed; on even indexes record the astigmatism
    // needed and restore the beam tilt
    posIndex = (index - 1) / 2;
    if (index % 2) {
      mComaVsISXTiltNeeded[posIndex] = mLastXTiltNeeded;
      mComaVsISYTiltNeeded[posIndex] = mLastYTiltNeeded;
    } else {
      mComaVsISXAstigNeeded[posIndex] = mLastXStigNeeded;
      mComaVsISYAstigNeeded[posIndex] = mLastYStigNeeded;
      BacklashedBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY, true);
    }
  }

  // Set IS for next measurement and start one routine or the other
  if (index < 8) {
    posIndex = index / 2;

    // On odd indexes apply the needed beam tilt in order to measure astigmatism
    // On even indexes, go to the next image shift
    if (index % 2) {
      if (!skipBT)
        BacklashedBeamTilt(mBaseBeamTiltX + mComaVsISXTiltNeeded[posIndex],
          mBaseBeamTiltY + mComaVsISYTiltNeeded[posIndex], true);
    } else {
      GetComaVsISVector(magInd, mCVISextentToUse, mCVISrotationToUse, posIndex,
        mComaVsISAppliedISX[posIndex], mComaVsISAppliedISY[posIndex]);
      PrintfToLog("Measuring at image shift %.1f %.1f (IS units)",
        mComaVsISAppliedISX[posIndex], mComaVsISAppliedISY[posIndex]);
      mScope->SetImageShift(mComaVsISAppliedISX[posIndex], mComaVsISAppliedISY[posIndex]);
      mShiftManager->SetISTimeOut(delayFactor *
        mShiftManager->GetLastISDelay());

      // To test that the equations are right, uncomment this and comment out CtfBased...
      /*ApplyScaleMatrix(mComaVsIScal.matrix,
        mComaVsISAppliedISX[posIndex], mComaVsISAppliedISY[posIndex], mLastXTiltNeeded,
        mLastYTiltNeeded);
      ApplyScaleMatrix(mComaVsIScal.astigMat,
        mComaVsISAppliedISX[posIndex], mComaVsISAppliedISY[posIndex], mLastXStigNeeded,
        mLastYStigNeeded);*/
    }
    mComaVsISindex++;
    if (index % 2 || !skipBT) {
      mWinApp->AddIdleTask(TASK_CAL_COMA_VS_IS, 0, 0);
      CtfBasedAstigmatismComa(useFullFac * ((index + 1) % 2), false, 1, true, false);
    } else {
      mLastXTiltNeeded = mLastYTiltNeeded = 0.;
      ComaVsISNextTask(0);
    }
    return;
  }

  // Finished; compute the calibration
  delISX1 = mComaVsISAppliedISX[1] - mComaVsISAppliedISX[0];
  delISY1 = mComaVsISAppliedISY[1] - mComaVsISAppliedISY[0];
  delISX2 = mComaVsISAppliedISX[3] - mComaVsISAppliedISX[2];
  delISY2 = mComaVsISAppliedISY[3] - mComaVsISAppliedISY[2];
  denom = delISX1 * delISY2 - delISX2 * delISY1;
  mComaVsIScal.matrix.xpx = ((mComaVsISXTiltNeeded[1] - mComaVsISXTiltNeeded[0]) * 
    delISY2 - (mComaVsISXTiltNeeded[3] - mComaVsISXTiltNeeded[2]) * delISY1) / denom;
  mComaVsIScal.matrix.xpy = ((mComaVsISXTiltNeeded[3] - mComaVsISXTiltNeeded[2])  *
    delISX1 - (mComaVsISXTiltNeeded[1] - mComaVsISXTiltNeeded[0]) * delISX2) / denom;
  mComaVsIScal.matrix.ypx = ((mComaVsISYTiltNeeded[1] - mComaVsISYTiltNeeded[0]) *
    delISY2 - (mComaVsISYTiltNeeded[3] - mComaVsISYTiltNeeded[2]) * delISY1) / denom;
  mComaVsIScal.matrix.ypy = ((mComaVsISYTiltNeeded[3] - mComaVsISYTiltNeeded[2])  *
    delISX1 - (mComaVsISYTiltNeeded[1] - mComaVsISYTiltNeeded[0]) * delISX2) / denom;
  PrintfToLog("IS to beam tilt matrix: %f  %f  %f  %f", mComaVsIScal.matrix.xpx,
    mComaVsIScal.matrix.xpy, mComaVsIScal.matrix.ypx, mComaVsIScal.matrix.ypy);

  mComaVsIScal.astigMat.xpx = ((mComaVsISXAstigNeeded[1] - mComaVsISXAstigNeeded[0]) *
    delISY2 - (mComaVsISXAstigNeeded[3] - mComaVsISXAstigNeeded[2]) * delISY1) / denom;
  mComaVsIScal.astigMat.xpy = ((mComaVsISXAstigNeeded[3] - mComaVsISXAstigNeeded[2])  *
    delISX1 - (mComaVsISXAstigNeeded[1] - mComaVsISXAstigNeeded[0]) * delISX2) / denom;
  mComaVsIScal.astigMat.ypx = ((mComaVsISYAstigNeeded[1] - mComaVsISYAstigNeeded[0]) *
    delISY2 - (mComaVsISYAstigNeeded[3] - mComaVsISYAstigNeeded[2]) * delISY1) / denom;
  mComaVsIScal.astigMat.ypy = ((mComaVsISYAstigNeeded[3] - mComaVsISYAstigNeeded[2])  *
    delISX1 - (mComaVsISYAstigNeeded[1] - mComaVsISYAstigNeeded[0]) * delISX2) / denom;
  PrintfToLog("IS to astigmatism matrix: %f  %f  %f  %f", mComaVsIScal.astigMat.xpx,
    mComaVsIScal.astigMat.xpy, mComaVsIScal.astigMat.ypx, 
    mComaVsIScal.astigMat.ypy);

  // Save the calibration conditions
  mComaVsIScal.magInd = mMagIndex;
  mComaVsIScal.spotSize = mScope->GetSpotSize();
  mComaVsIScal.intensity = (float)mScope->GetIntensity();
  mComaVsIScal.probeMode = mScope->ReadProbeMode();
  mComaVsIScal.alpha = mScope->GetAlpha();
  mComaVsIScal.aperture = 0;
  if (mScope->GetUseIllumAreaForC2())
    mComaVsIScal.aperture = mWinApp->mBeamAssessor->RequestApertureSize();
  if (mWinApp->mNavHelper->mMultiShotDlg)
    mWinApp->mNavHelper->mMultiShotDlg->ManageEnables();
  StopComaVsISCal();
}

void CAutoTuning::ComaVsISCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Timeout doing calibration of coma versus image shift"));
  StopComaVsISCal();
  mWinApp->ErrorOccurred(1);
}

// Stop function, restore values
void CAutoTuning::StopComaVsISCal(void)
{
  mComaVsISindex = -1;
  mScope->SetImageShift(0., 0.);
  mScope->SetBeamTilt(mBaseBeamTiltX, mBaseBeamTiltY);
  mWinApp->UpdateBufferWindows();
}

// Function to provide the IS vectors at the given mag for a position
void CAutoTuning::GetComaVsISVector(int magInd, float extent, int rotation, int posIndex,
  float & delISX, float & delISY)
{
  int delx[4] = {-1, 1, 0, 0};
  int dely[4] = {0, 0, -1, 1};
  ScaleMat aMat = mShiftManager->IStoSpecimen(magInd);
  double ISX = extent / sqrt(aMat.xpx * aMat.xpx + aMat.ypx * aMat.ypx);
  double ISY = extent / sqrt(aMat.xpy * aMat.xpy + aMat.ypy * aMat.ypy);
  double cosRot = cos(DTOR * rotation);
  double sinRot = sin(DTOR * rotation);
  B3DCLAMP(posIndex, 0, 3);
  delISX = (float)(cosRot  * delx[posIndex] * ISX - sinRot * dely[posIndex] * ISY);
  delISY = (float)(sinRot *  delx[posIndex] * ISX + cosRot * dely[posIndex] * ISY);
}
