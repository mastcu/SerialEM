#include "stdafx.h"
#include "SerialEM.h"
#include "AutoTuning.h"
#include "EMscope.h"
#include "FocusManager.h"
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "Shared\b3dutil.h"


CAutoTuning::CAutoTuning(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mDoingCalAstig = false;
  mDoingFixAstig = false;
  mDoingComaFree = false;
  mAstigCenterFocus = -1.;
  mAstigFocusRange = 10.;
  mCalComaFocus = -1.;
  mAstigToApply = 0.1f;
  mAstigBeamTilt = 3.2f;
  mUsersAstigTilt = -1.;
  mUsersComaTilt = -1.;
  mMaxComaBeamTilt = 10.;
  mPostFocusDelay = 500;
  mPostAstigDelay = 900;
  mPostBeamTiltDelay = 500;
  mAstigIterationThresh = 0.015f;
  mMaxAstigIterations = 2;
  mComaMeanCritFac = 0.5f;
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
    mFocusManager->SetBeamTilt(mAstigBeamTilt);
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
      mWinApp->mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
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
      mFocusManager->SetBeamTilt(mAstigBeamTilt);
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
        mWinApp->mShiftManager->SetGeneralTimeOut(GetTickCount(), mPostAstigDelay);
        mNumIterations++;
        mOperationInd = -1;
        PrintfToLog("Iterating because astigmatism is above threshold");
        FixAstigNextTask(0);
        return;
    }
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
int CAutoTuning::ComaFreeAlignment(bool calibrate, bool imposeChange)
{
  mCalibrateComa = calibrate;

  if (!calibrate && LookupComaCal(mScope->GetProbeMode(), mScope->GetAlpha(), 
    mScope->GetMagIndex()) < 0) {
      SEMMessageBox("There is no coma calibration for the current conditions");
      return 1;
  }
  if (mWinApp->LowDoseMode()) {
    SEMMessageBox("Coma-free operations cannot be run in Low Dose mode");
    return 1;
  }
  if (calibrate) {
    CString str = TuningCalMessageStart("Coma", "beam tilts up to", mMaxComaBeamTilt);
    str += "\n\nThe objective aperture must not be in for this procedure.\n\n"
      "Are you ready to proceed?";
    if (AfxMessageBox(str, MB_QUESTION) != IDYES)
      return 1;
  }

  GeneralSetup();
  mDirectionInd = 0;
  mDoingComaFree = true;
  mOperationInd = -1;
  mImposeChange = imposeChange;
  mScope->SetLDCenteredShift(0., 0.);
  mFocusManager->AutoFocusStart(-1);
  mWinApp->AddIdleTask(TASK_COMA_FREE, 0, 0);
  return 0;
}

// Do the next operation for coma calibration or correction
void CAutoTuning::ComaFreeNextTask(int param)
{
  float xFacs[4] = {-1., 1., 0., 0.};
  float yFacs[4] = {0., 0., -1., 1.};
  float xx1[4], xx2[4], yy[4];
  double xTiltFac, yTiltFac;
  float scale, misAlignX = 0., misAlignY = 0.;
  ComaCalib cal;
  CString mess, mess2;
  int calInd, ind, idir;
  if (!mDoingComaFree)
    return;

  // Get the existing calibration, with exact match to beam tilt if calibrating
  calInd = LookupComaCal(mScope->ReadProbeMode(), mScope->FastAlpha(), 
    mScope->FastMagIndex(), mCalibrateComa);
  if (mDirectionInd == 1 && (mOperationInd == 7 || 
    (!mCalibrateComa && mOperationInd == 1))) {

      // Finished: Compute displacement differences which are the basic unit of operation
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
        else
          mComaCals[calInd] = cal;
        PrintMatrix(cal.comaXmat, "X beam tilt");
        PrintMatrix(cal.comaYmat, "Y beam tilt");
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
        mess.Format("Solved misalignment = %.2f  %.2f", misAlignX, misAlignY);
        if (!FEIscope) {
          scale = mFocusManager->EstimatedBeamTiltScaling();
          mess2.Format(" %%, about %.2f  %.2f  milliradians", misAlignX * scale, 
            misAlignY * scale);
          mess += mess2;
        } else
          mess += "milliradians";
        mWinApp->AppendToLog(mess);
        if (mImposeChange)
          mScope->IncBeamTilt(-misAlignX, -misAlignY);
      }
      StopComaFree();
      return;
  }

  // Set up for the next run
  if (mOperationInd < 0) {

    // First time, process the defocus measurement, set beam tilt and direction
    if (SaveAndSetDefocus(mCalibrateComa ? mCalComaFocus : mComaCals[calInd].defocus)) {
      mWinApp->ErrorOccurred(1);
      StopComaFree();
      return;
    }
    mFocusManager->SetBeamTilt(mMaxComaBeamTilt / 4.);
    mFocusManager->SetTiltDirection(0);
    if (!mCalibrateComa)
      mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING COMA");
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
  //PrintfToLog("oper %d dir %d xtf %f  ytf %f  misX  %f  misY %f", mOperationInd, mDirectionInd, 
   // xTiltFac, yTiltFac, misAlignX, misAlignY);
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
        ratio = magTabs[magInd].mag / magTabs[mAstigCals[ind].magInd].mag;
        if (ratio < 0)
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
          ratio = magTabs[magInd].mag / magTabs[mComaCals[ind].magInd].mag;
          if (ratio < 0)
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
  mCamToSpec = mWinApp->mShiftManager->CameraToSpecimen(mMagIndex);
  mCamToSpec.xpx *= 1000.f;
  mCamToSpec.xpy *= 1000.f;
  mCamToSpec.ypx *= 1000.f;
  mCamToSpec.ypy *= 1000.f;
  mSavedBeamTilt = mFocusManager->GetBeamTilt();
  mSavedTiltDir = mFocusManager->GetTiltDirection();
  mSavedDefocus = mScope->GetDefocus();
  mSavedPostTiltDelay = mFocusManager->GetPostTiltDelay();
}

// Do commons tasks for stopping a routine
void CAutoTuning::CommonStop(void)
{
  mScope->SetDefocus(mSavedDefocus);
  mFocusManager->SetBeamTilt(mSavedBeamTilt);
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
