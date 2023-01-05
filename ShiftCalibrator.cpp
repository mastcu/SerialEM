// ShiftManager.cpp:      Calibrates image and stage shift, image shift offsets,
//                           and defocus-dependent mags
//
// Copyright (C) 2003-2015 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include ".\ShiftCalibrator.h"
#include "Utilities\XCorr.h"
#include "EMscope.h"
#include "CameraController.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "SerialEMView.h"
#include "SerialEMDoc.h"
#include "MultiTSTasks.h"
#include "NavHelper.h"
#include "FocusManager.h"
#include "ProcessImage.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"

#define CAL_BASE_CONSET  2

#pragma warning ( disable : 4244 )

static char calMessage[4][60] = {
  "THIS IS GOOD", "THIS IS NOT AS GOOD AS USUAL - you might want to redo it",
   "THIS IS NOT VERY GOOD - YOU SHOULD REDO IT", 
   "THIS IS TERRIBLE - CHECK SETTINGS AND TRY AGAIN" };

CShiftCalibrator::CShiftCalibrator(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mConSets = mWinApp->GetConSets();
  mMagTab = mWinApp->GetMagTable();
  mCamParams = mWinApp->GetCamParams();
  mImBufs = mWinApp->GetImBufs();
  mFiltParam = mWinApp->GetFilterParams();
  mScope = NULL;
  mFocusModified = false;
  mMaxCalShift = 5.;
  mMaxLMCalShift = 25.;
  mStageCalBacklash = -1.;   // Defer initialization with default until scope known
  mUseTrialSize = false;
  mUseTrialBinning = false;
  mWholeCorrForStageCal = true;
  mShiftIndex = -1;
  mCalISOindex = -1;
  mGIFofsFromMag = 0;
  mGIFofsToMag = 0;
  mZeroOffsetMode = -1;
  mCalISOstageLimit = 0.025f;
  mStageCycleX = -1.;
  mStageCycleY = -1.;
  mMaxStageCalExtent = 140.;  // Allow up to two cycles
  mISSindex = -1;
  mISStimeStampLast = -1.;
  mSkipLensNormNextIScal = false;
  mGaveFocusMagMessage = false;
  mCalToggleInterval = 250;
  mNumCalToggles = 10;
  mHighDefocusForIS = -50.;
  mLastISFocusTimeStamp = 0;
  mMinFieldSixPointCal = 3.f;
  mWarnedHighFocusNotCal = false;
}

CShiftCalibrator::~CShiftCalibrator(void)
{
}

// Further initialization of program component addresses
void CShiftCalibrator::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mSM = mWinApp->mShiftManager;   // Since we are on such familiar terms with it
  mImBufs = mWinApp->GetImBufs();
  mISSshifts = mSM->GetSTEMinterSetShifts();
  if (mStageCalBacklash < 0.) {
    if (JEOLscope)
      mStageCalBacklash = 10.;
    else if (HitachiScope)
      mStageCalBacklash = 20.;
    else
      mStageCalBacklash = 5.;
  }
  if (mStageCycleX < 0)            // Defaults are from two Tecnais
    mStageCycleX = FEIscope ? 62. : 40.;
  if (mStageCycleY < 0)
    mStageCycleY = FEIscope ? 41. : 40.;
}

//////////////////////////////////////////////////////////////////////
// Image shift calibration
//////////////////////////////////////////////////////////////////////

#define CAL_DELAY_FACTOR 2.0f

// Image shift calibration: the startup routine
// ifRep is -1 to do from scratch; this sets mFirstCal true; and then on the second
// round it is called with ifRep 1
void CShiftCalibrator::CalibrateIS(int ifRep, BOOL calStage, BOOL fromMacro)
{
  ScaleMat pr;
  int i, j, binnedX, binnedY, top, left, bottom, right;
  int targetSize = 512;
  float scaleDown = 1.;
  ControlSet  *conSet = mConSets + ISCAL_CONSET;
  mFirstCal = false;
  float maxCalShift, pixel, roughScale, foundPixel, showPixScale;
  double baseZ, xExtent, yExtent;
  float wholeWarnDist = 75.;
  CString numToDo = "9";
  CString message;
  MagTable *magTab;
  CameraParameters *camP;
  if (!calStage && mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(mCamera->ConSetToLDArea(ISCAL_CONSET));

  mCalIndex = mScope->GetMagIndex();
  magTab = &mMagTab[mCalIndex];
  mCurrentCamera = mWinApp->GetCurrentCamera();
  mNumShiftCal = 9;
  mActPostExposure = mWinApp->ActPostExposure();
  camP = &mCamParams[mCurrentCamera];
  mFromMacro = fromMacro;
  showPixScale = BinDivisorF(camP) * 1000.f;
  if (mCamera->IsDirectDetector(camP))
    targetSize = 1024;

  // Get the rough scale and use it to scale the max shift from microns to units for IS
  // For PL, first scale max shift to pixels since scale is pixels / unit
  if (mWinApp->GetSTEMMode())
    roughScale = mSM->GetSTEMRoughISscale();
  else
    roughScale = (mCalIndex < mScope->GetLowestNonLMmag(camP) && mSM->GetLMRoughISscale())
      ? mSM->GetLMRoughISscale() : mSM->GetRoughISscale();
  maxCalShift = ((mCalIndex < mScope->GetLowestNonLMmag(camP))
    ? mMaxLMCalShift : mMaxCalShift);
  if (mScope->GetUsePLforIS(mCalIndex) && !mWinApp->GetSTEMMode()) {
    roughScale = 1000. * mScope->GetRoughPLscale() / camP->pixelMicrons;
    maxCalShift /= mSM->GetPixelSize(mCurrentCamera, mCalIndex);
  }
  maxCalShift /= roughScale;
  mCalIsHighFocus = !calStage && ifRep > 1;
  mSixPointIScal = mCalIsHighFocus;

  mCalStage = calStage;
  if (mCalStage) {

    // Stage calibrations are stored as a true coordinate transformation
    // so need to invert it to use for scaling movements here  (HUH?)
    // 4/22/06: Should use fallback to protect against bad calibrations
    //pr = StageToCamera(mCurrentCamera, mCalIndex);
    pr = MatMul(MatInv(mSM->FallbackSpecimenToStage(1., 1.)), 
      mSM->SpecimenToCamera(mCurrentCamera, mCalIndex));
    if (!mSM->GetAnyCalPixelSizes()) {
      if (AfxMessageBox("You should not run stage calibration without having at least\n"
        "one calibrated pixel size entered into properties (not necessarily for this mag"
        " and camera).\nThere are no calibrated pixel sizes in properties.\n\n"
        "Are you sure you want to proceed?", MB_QUESTION) == IDNO)
        return;
    }
    if (!pr.xpx && !pr.ypx) {
      AfxMessageBox("There is insufficient information about pixel sizes and rotations in"
        " the properties and calibrations to do a stage calibration\n\n"
        "This is probably due to a bug in the program", MB_EXCLAME);
      return;
    }
    mScope->GetStagePosition(mBaseISX, mBaseISY, baseZ);
  } else {

    // Get a matrix of pixels per unit of image shift.
    // If repeating, use the matrix from last time
    // If there is none, set up a matrix to move about 1/5 of normal amount in
    // arbitrary directions
     mScope->GetImageShift(mBaseISX, mBaseISY);
    if (ifRep > 0)
      pr = mFirstMat;
    else
      pr = mSM->IStoCamera(mCalIndex);
    if (!pr.xpx || ifRep < 0) {
      ifRep = 0;
      mFirstCal = true;

      // Go back to calibrated pixel size or fallback to defined properties, to avoid
      // contamination from any bad IS calibrations
      if (magTab->pixelSize[mCurrentCamera] > 0.0 && !magTab->pixDerived[mCurrentCamera])
        pixel = magTab->pixelSize[mCurrentCamera];
      else {
        pixel = camP->pixelMicrons / (MagForCamera(camP, mCalIndex) * camP->magRatio);
        foundPixel = mWinApp->mProcessImage->GetFoundPixelSize(mCurrentCamera, mCalIndex)
          / showPixScale;
        if (foundPixel > 0. && (pixel / foundPixel < 0.67 || pixel / foundPixel > 1.5)) {
          message.Format("There is a sizable difference between the nominal pixel size "
            "computed from the FilmToCameraMagnification property (%.3g nm) "
            "and the size from running Find Pixel Size at this mag (%.3g nm).\n\n"
            "Is the value from the Find Pixel Size routine accurate?\n\n"
            "Press:\n\"Use Found Size\" to use the value from Find Pixel Size,\n\n"
            "\"Use Nominal Size\" to use the value from the film-to-camera magnification "
            "ratio,\n\n\"Cancel\" to cancel this operation.", pixel * showPixScale, 
            foundPixel * showPixScale);
          j = SEMThreeChoiceBox(message, "Use Found Size", "Use Nominal Size", "Cancel",
            MB_YESNOCANCEL | MB_ICONQUESTION);
          if (j == IDCANCEL)
            return;
          if (j == IDYES)
            pixel = foundPixel;
        } else if (foundPixel <= 0.) {
          foundPixel = magTab->pixelSize[mCurrentCamera];
          if (foundPixel > 0. && (pixel / foundPixel < 0.5 || pixel / foundPixel > 2.)) {
            message.Format("There is a sizable difference between the nominal pixel size"
              " computed from the FilmToCameraMagnification property (%.3g nm) "
              "and the size derived from other calibrated pixel sizes (%.3g nm).\n\n"
              "This calibration may fail with one or the other of these values -\n"
              "if the specimen is a cross-line grating, you can prevent problems by "
              "taking an image and running the Find Pixel Size routine first.\n\n"
              "Press:\n\"Use Derived Size\" to use the value derived from other "
              "calibrated pixel sizes,\n\n"
              "\"Use Nominal Size\" to use the value from the film-to-camera "
              "magnification ratio,\n\n\"Cancel\" to cancel this operation.", 
              pixel * showPixScale, foundPixel * showPixScale);
            j = SEMThreeChoiceBox(message, "Use Derived Size", "Use Nominal Size", 
              "Cancel", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (j == IDCANCEL)
              return;
            if (j == IDYES)
              pixel = foundPixel;
          }
        }
      }
      if (mScope->GetUsePLforIS(mCalIndex) && !mWinApp->GetSTEMMode())
        pr.xpx = 5.f * roughScale * (camP->GIF ? 10. : 1.);
      else
        pr.xpx = 5.f * roughScale / pixel;
      pr.xpy = 0.f;
      pr.ypx = 0.f;
      pr.ypy = pr.xpx;
      numToDo = "13";
      mNumShiftCal = 4;
    } else if (!ifRep) {

      // Use 6-pt cal for regular cal if field is big enough
      if (B3DMIN(camP->sizeX, camP->sizeY) * mSM->GetPixelSize(mCurrentCamera, mCalIndex)
        > mMinFieldSixPointCal)
        mSixPointIScal = true;
    }
  }

  if (mSixPointIScal) {
    numToDo = "6";
    mNumShiftCal = 6;
    mNumStageShiftsX = mNumStageShiftsY = 3;
  }

  // Get the basic image shift outputs in X and Y for moving half of the field
  *conSet = mConSets[CAL_BASE_CONSET];
  mCalCamSizeX = camP->sizeX;
  mCalCamSizeY = camP->sizeY;
  if (mUseTrialSize) {
    mCalCamSizeX = conSet->right - conSet->left;
    mCalCamSizeY = conSet->bottom - conSet->top;
  }
  int size = mCalCamSizeX < mCalCamSizeY ? mCalCamSizeX : mCalCamSizeY;
  double xOut = 0.5 * size / sqrt ((double) (pr.xpx * pr.xpx + pr.ypx * pr.ypx));
  double yOut = 0.5 * size / sqrt ((double) (pr.xpy * pr.xpy + pr.ypy * pr.ypy));
  if (mCalStage) {
    OptimizeStageSteps(mStageCycleX, xOut, mNumStageShiftsX);
    OptimizeStageSteps(mStageCycleY, yOut, mNumStageShiftsY);
    mNumShiftCal = mNumStageShiftsX + mNumStageShiftsY;
    numToDo.Format("%d", mNumShiftCal);
    xExtent = (mNumStageShiftsX - 1) * xOut;
    yExtent = (mNumStageShiftsY - 1) * yOut;
    float cycleFac = FEIscope ? 0.9f : 0.5f;
    if ((xExtent < cycleFac * mStageCycleX || yExtent < cycleFac * mStageCycleY) && 
      mCalIndex > mScope->GetLowestNonLMmag(camP)) {
        if (xExtent < 0.25 * mStageCycleX || yExtent < 0.25 * mStageCycleY) {
          message.Format("WARNING: You should not do this calibration!\n"
            "It is over too short a distance (~ %.1f microns)\n"
            "to give an accurate calibration\n", (xExtent + yExtent) / 2.);
        } else {
          message = "WARNING: Stage calibrations become increasingly inaccurate"
            " at higher mags.\n";
          if (FEIscope)
            message += "(Especially when they run over less than the stage cycle length.)"
            "\n";
        }
        message += "\nIt is better to have a good stage calibration at ";
        if (FEIscope)
          message += "one nonLM mag\nand good image shift calibrations at that mag and"
          " above\n";
        else
          message += "a few lower nonLM mags\nand good image shift calibrations and "
          "RotationAndPixel entries at those mags and above\n";
        message += "than to have lots of stage calibrations at higher mags.\n\n"
          "Are you sure you want to do this calibration?";
        if (AfxMessageBox(message, MB_QUESTION) == IDNO)
          return;
    }
  }
  
  message = "To calibrate, " + numToDo + " images will be acquired using\nthe "
    + mModeNames[CAL_BASE_CONSET] + " parameter set.\nBefore starting, be sure that the"
    " beam brightness and\nexposure time are set so as to give "
    "an image\nwith moderately high counts with " + mModeNames[CAL_BASE_CONSET] + ".\n\n";
   if (mCalStage) {
     if (mNumShiftCal > 6) {
       numToDo.Format("The stage will be moved %.0f microns in X and %0.f microns in Y."
         "\nMake sure there are features for correlations, and no grid bars, over this"
         " range.\n\n", xExtent, yExtent);
       message += numToDo;
     }
     message += "Watch when it toggles between aligned images\n"
       "and STOP if there is a bad alignment.\n\n";
     if (mNumShiftCal == 6 && mWholeCorrForStageCal && (xExtent > wholeWarnDist || 
       yExtent > wholeWarnDist))
         message += "WARNING: Turn off Whole Image Correlations if\n"
           "there are multiple grid bars in the field of view\n\n";
   }
   if (!ifRep && !mFromMacro) {
    if (AfxMessageBox(message + "Are you ready to proceed?", MB_YESNO | MB_ICONQUESTION)
      == IDNO)
      return;
  }


  if (!mCalStage) {
    // If the shift is outside the maximum amount, scale shift down and reduce delay
    if (xOut > maxCalShift || yOut > maxCalShift) {
      scaleDown = maxCalShift / (xOut > yOut ? xOut : yOut);
      xOut *= scaleDown;
      yOut *= scaleDown;
    }
    
    mCalDelayFactor = scaleDown * CAL_DELAY_FACTOR;
    
    // Now check if the deviations take the shift outside the allowed range
    // BUt not for doing from scratch, where this is based on a potentially bad cal
    if (!mFirstCal && 
      !(mSM->ImageShiftIsOK(xOut, 0., true) && mSM->ImageShiftIsOK(-xOut, 0., true) &&
      mSM->ImageShiftIsOK(0., yOut, true) && mSM->ImageShiftIsOK(0., -yOut, true))) {
      AfxMessageBox(_T(
        "The image shift is too close to the end of its range\n"
        "or the computed output values are too large.\n\n"
        "Reset the image shifts to zero or try calibrating from scratch."), MB_EXCLAME);
      if (ifRep)
        StopCalibrating();
      return;
    }
  }
  
  SEMTrace('c', "Assumed matrix for computing output: %.3f %.3f %.3f %.3f\r\nBasic step"
    " size is xOut %f  yOut %f", pr.xpx, pr.xpy, pr.ypx, pr.ypy, xOut, yOut);

  // Normalize lenses on first round
  if ((mCalIndex != mScope->GetLastNormMagIndex() || !mSkipLensNormNextIScal) &&
    ifRep <= 0)
    mScope->NormalizeProjector();
  mSkipLensNormNextIScal = false;

  // If in low mag, go to a standard focus if one is defined
  mScope->SetFocusToStandardIfLM(mCalIndex);

  // Fill the arrays of outputs for the stage images or the 9 or 4 IS images
  for (i = 0; i < mNumShiftCal; i++)
    mCalOutX[i] = mCalOutY[i] = 0;
  if (mCalStage || mSixPointIScal) {
    i = 0;
    for (j = 0; j < mNumStageShiftsX; j++)
      mCalOutX[i++] = xOut * (j - mNumStageShiftsX / 2);
    for (j = 0; j < mNumStageShiftsY; j++)
      mCalOutY[i++] = yOut * (j - mNumStageShiftsY / 2);

  } else {
    mCalOutX[1] = xOut;
    if (mFirstCal) {
      mCalOutY[3] = yOut;
    } else {
      mCalOutX[3] = -xOut;
      mCalOutY[5] = yOut;
      mCalOutY[7] = -yOut;
    }
  }
  
  // If necessary, find binning where image size reaches the target size
  if (size / conSet->binning > targetSize && !mUseTrialBinning) {
    for (i = 0; i < camP->numBinnings; i++) {
      mCalBinning = camP->binnings[i];
      if (size / mCalBinning <= targetSize)
        break;
    }

    conSet->exposure = (conSet->exposure - camP->deadTime) * 
      (float)(conSet->binning * conSet->binning) / (float)(mCalBinning * mCalBinning) +
      camP->deadTime;
    conSet->exposure = B3DMAX(conSet->exposure, camP->minExposure);
    conSet->binning = mCalBinning;
    if (conSet->exposure <= 0.25 &&
      conSet->drift > camP->builtInSettling)
      conSet->drift = camP->builtInSettling;
  }
  mCalBinning = conSet->binning;

  // Finish composing the Control Set
  binnedX = mCalCamSizeX / mCalBinning;
  binnedY = mCalCamSizeY / mCalBinning;
  top = conSet->top / mCalBinning;
  left = conSet->left / mCalBinning;
  bottom = conSet->bottom / mCalBinning;
  right = conSet->right / mCalBinning;
  if (mUseTrialSize) {
    binnedX = right - left;
    binnedY = bottom - top;
    mCamera->AdjustSizes(binnedX, camP->sizeX, camP->moduloX, left,
      right, binnedY, camP->sizeY,
      camP->moduloY, top, bottom, mCalBinning);
  } else {
    mCamera->CenteredSizes(binnedX, camP->sizeX, camP->moduloX, left,
      right, binnedY, camP->sizeY,
      camP->moduloY, top, bottom, mCalBinning);
  }
  conSet->left = left * mCalBinning;
  conSet->right = right * mCalBinning;
  conSet->top = top * mCalBinning;
  conSet->bottom = bottom *mCalBinning;
  mCalCamSizeX = binnedX * mCalBinning;
  mCalCamSizeY = binnedY * mCalBinning;
  conSet->mode = SINGLE_FRAME;

  // Compute the base shifts between each pair of images; divide to allow
  // for binning; and then make sure they are even
  for (i = 0; i < mNumShiftCal; i++) {
    if (!mFirstCal && !(mCalStage && mWholeCorrForStageCal)) {
      mBaseShiftX[i] = 2 * ((int)(((mCalOutX[i + 1] - mCalOutX[i]) * pr.xpx +
              (mCalOutY[i + 1] - mCalOutY[i]) * pr.xpy) /
              mCalBinning) / 2);
      mBaseShiftY[i] =  2 * ((int)(((mCalOutX[i + 1] - mCalOutX[i]) * pr.ypx +
            (mCalOutY[i + 1] - mCalOutY[i]) * pr.ypy) /
            mCalBinning) / 2);
    } else {
      mBaseShiftX[i] = 0;
      mBaseShiftY[i] = 0;
    }
  } 
  mShiftIndex = 0;
  mCCCmin = 1.e30f;
  mCCCsum = 0.;
  mCalMovingStage = false;

  mCamera->SetRequiredRoll(B3DCHOICE(mCalStage, 5, mSixPointIScal ? 4 : 1));
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, B3DCHOICE(mCalStage, "CALIBRATING STAGE SHIFT", 
    mCalIsHighFocus ? "CALIBRATING HIGH FOCUS IS" : "CALIBRATING IMAGE SHIFT"));
  if (mCamera->CameraBusy()) 
    mCamera->StopCapture(0);
  DoNextShift();
}

// Initiate the capture at the next shift position
void CShiftCalibrator::DoNextShift()
{
  double ISX, ISY;
  StageMoveInfo smi;
  int timeOut = 25000;

  if (mCalStage) {
    if (mCalMovingStage) {
      mCalMovingStage = false;
    } else {
      smi.x = mBaseISX + mCalOutX[mShiftIndex];
      smi.y = mBaseISY + mCalOutY[mShiftIndex];
      smi.axisBits = axisXY;
      smi.backX = -mStageCalBacklash;
      smi.backY = -mStageCalBacklash;
      mCalMovingStage = true;
      mScope->MoveStage(smi, !mShiftIndex || mShiftIndex == mNumStageShiftsX);
      timeOut += 60000 * (mScope->GetJeol1230() ? 4 : 1);
      mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_CAL_IMAGESHIFT, 0, timeOut);
      return;
    }
  } else {
  
   if (mCamera->CameraBusy())
    timeOut += 20000;
   // If not acting post-exposure or if starting 6-point cal, do the current image shift
    if (!mActPostExposure || (!mShiftIndex && mSixPointIScal)) {
      mScope->SetImageShift(mBaseISX + mCalOutX[mShiftIndex], 
        mBaseISY + mCalOutY[mShiftIndex]);
      mSM->SetISTimeOut(mCalDelayFactor * mSM->GetLastISDelay());
    }
    
    if (mActPostExposure && mShiftIndex < mNumShiftCal - 1) {
      
      // Otherwise, unless its the last shot, set up the next image shift
      ISX = mBaseISX + mCalOutX[mShiftIndex + 1];
      ISY = mBaseISY + mCalOutY[mShiftIndex + 1];
      mCamera->QueueImageShift(ISX, ISY, 
        (int)(1000. * mCalDelayFactor * mSM->PredictISDelay(ISX, ISY)));
    }
  }
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_CAL_IMAGESHIFT, 0, 0);
  mCamera->InitiateCapture(ISCAL_CONSET);
}

// On idle tasks when error
void CShiftCalibrator::ShiftCleanup(int error)
{
  StopCalibrating();
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Timeout calibrating image or stage shifts"));
  mWinApp->ErrorOccurred(error);
}

// Come here when a picture is done
void CShiftCalibrator::ShiftDone()
{
  int j, numX, numY;
  KImage *imA, *imB;
  double diff, matRange, driftRange, minDriftX, maxDriftX, angle, xtheta, ytheta;
  double  minDriftY, maxDriftY, matMax, xOut, yOut, delX0, delY0, delX2, delY2;
  float aaXshift, aaYshift, aaCCC;
  float stageSymCrit = 0.2f;
  float transferSymCrit = 0.2f;
  float transferChangeLim = 10.f;
  CString report, rotmess;
  ScaleMat *mat;
  int pad = 32;
  int taper = 8;
  int binnedX = mCalCamSizeX / mCalBinning;
  int binnedY = mCalCamSizeY / mCalBinning;
  ScaleMat oldMat, oldInv, delMat, workMat;
  CString strSource = "derived";
  BOOL oldIsDirect = false;
  int magVal = MagForCamera(mCurrentCamera, mCalIndex);

  // Send the report to a log window if administrator, or message box if not
  int action = mFirstCal ? LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN :
    LOG_IF_ADMIN_MESSAGE_IF_NOT;
  if (mFromMacro || mCalIsHighFocus)
    action = LOG_OPEN_IF_CLOSED;

  mImBufs->mCaptured = BUFFER_CALIBRATION;

  // If a stop signal came in, call the stop again and quit
  if (mShiftIndex < 0) {
    StopCalibrating();
    return;
  }

  if (mCalMovingStage) {
    DoNextShift();
    return;
  }

  if (mShiftIndex > 0 && (!(mCalStage || mSixPointIScal) ||
    mShiftIndex != mNumStageShiftsX)) {
    imA = mImBufs[1].mImage;
    imB = mImBufs[0].mImage;
    if (imA == NULL || imB == NULL) {
      AfxMessageBox(_T("Error getting image buffers - calibration aborted"), MB_EXCLAME);
      StopCalibrating();
      return;
    }
    if (imA->getWidth() != binnedX || imB->getWidth() != binnedX ||
      imA->getHeight() != binnedY || imB->getHeight() != binnedY) {
      AfxMessageBox(_T(
        "Image sizes do not match expectations - calibration aborted"), MB_EXCLAME);
      StopCalibrating();
      return;
    }

    mSM->SetPeaksToEvaluate(1000, 0.2f);
    j = mSM->AutoAlign(1, -1, false, false, NULL, (float)mBaseShiftX[mShiftIndex - 1],
      -(float)mBaseShiftY[mShiftIndex - 1], 0., 0., 0., &aaCCC, NULL, true, &aaXshift,
      &aaYshift);
    mSM->SetPeaksToEvaluate(0, 0.);
    if (j) {
      AfxMessageBox(_T("Alignment routine error"), MB_EXCLAME);
      StopCalibrating();
      return;
    }
    mTotalShiftX[mShiftIndex - 1] = aaXshift;
    mTotalShiftY[mShiftIndex - 1] = -aaYshift;
    mCCCmin = B3DMIN(mCCCmin, aaCCC);
    mCCCsum += aaCCC;

    // Correlate middle images of 6-pt IS cal for drift estimate
    if (mSixPointIScal && mShiftIndex == 4) {
      mSM->SetPeaksToEvaluate(1000, 0.2f);
      j = mSM->AutoAlign(3, -1, false, false, NULL, 0., 0., 0., 0., 0., &aaCCC, NULL,
        true, &aaXshift, &aaYshift);
      mSM->SetPeaksToEvaluate(0, 0.);
      if (j) {
        AfxMessageBox(_T("Alignment routine error"), MB_EXCLAME);
        StopCalibrating();
        return;
      }
      if (sqrt(aaXshift * aaXshift + aaYshift * aaYshift) > 15 || aaCCC < 0.5 * mCCCmin) {
        mTotalShiftX[mNumShiftCal] = 0.;
        mTotalShiftY[mNumShiftCal] = 0.;
        mCCCsum *= 1.3333f;
      } else {
        mTotalShiftX[mNumShiftCal] = aaXshift;
        mTotalShiftY[mNumShiftCal] = -aaYshift;
        mCCCmin = B3DMIN(mCCCmin, aaCCC);
        mCCCsum += aaCCC;
      }
    }

    // Align the shifted shots by the average of the negative of this shift and previous
    if (!mCalStage && (((mShiftIndex - 1) % 2 && mNumShiftCal == 9) ||
      ((mShiftIndex - 1) % 2 == 0 && mNumShiftCal == 4) ||
      (mShiftIndex != 3 && mSixPointIScal))) {
      if (mNumShiftCal == 4 || (mSixPointIScal && mShiftIndex % 3 == 2)) {
        imB->setShifts(mTotalShiftX[mShiftIndex - 1], -mTotalShiftY[mShiftIndex - 1]);
        mImBufs[0].SetImageChanged(1);
      } else if (mSixPointIScal) {
        imA->setShifts(-mTotalShiftX[mShiftIndex - 1], mTotalShiftY[mShiftIndex - 1]);
        mImBufs[1].SetImageChanged(1);
      } else {
        imA->setShifts(0.5 * (mTotalShiftX[mShiftIndex - 2] -
          mTotalShiftX[mShiftIndex - 1]),
          0.5 * (mTotalShiftY[mShiftIndex - 1] - mTotalShiftY[mShiftIndex - 2]));
        mImBufs[1].SetImageChanged(1);
      }
      mWinApp->mMainView->DrawImage();
    }

    // For stage, align B and toggle 
    if (mCalStage) {
      imA->setShifts(-mTotalShiftX[mShiftIndex - 1], mTotalShiftY[mShiftIndex - 1]);
      mImBufs[1].SetImageChanged(1);
      for (j = 0; j < mNumCalToggles; j++) {
        if (j)
          Sleep(B3DMAX(1, mCalToggleInterval));
        mWinApp->SetCurrentBuffer((j + 1) % 2);
      }
    }
  }

  mShiftIndex++;
  if (mShiftIndex < mNumShiftCal) {
    DoNextShift();
    return;
  }
  xOut = mCCCsum / (mNumShiftCal - (mCalStage ? 2 : 1));
  PrintfToLog("\r\nThe mean cross-correlation coefficient was %.3f and the minimum was"
    " %.3f", xOut, mCCCmin);
  if (xOut < 0.1)
    PrintfToLog("THIS IS RIDICULOUSLY LOW: YOU NEED TO USE MORE EXPOSURE\r\n");
  else if (xOut < 0.2)
    PrintfToLog("THIS IS DANGEROUSLY LOW: YOU SHOULD USE MORE EXPOSURE\r\n");
  else if (xOut < 0.4)
    PrintfToLog("This is low, it is advisable to have a CCC of at least 0.5\r\n");

  // STAGE CALIBRATION

  if (mCalStage) {
    mat = &workMat;
    numX = mNumStageShiftsX;
    numY = mNumStageShiftsY;

    // Average all shifts; multiply to get from binned to unbinned pixels
    // Divide by the output value that represents the amount moved between
    // between successive pictures, just as for image shift, so get the negative of
    // original xOut, yOut
    xOut = mCalOutX[numX / 2 + 1];
    yOut = mCalOutY[numX + numY / 2 + 1];
    mat->xpx = mat->xpy = mat->ypx = mat->ypy = 0.;
    SEMTrace('c', "X and Y shifts for X movements, total and per micron");
    for (j = 0; j < numX - 1; j++) {
      mat->xpx += (float)(mCalBinning * mTotalShiftX[j] / (xOut * (numX - 1)));
      mat->ypx += (float)(mCalBinning * mTotalShiftY[j] / (xOut * (numX - 1)));
      SEMTrace('c', "%8.1f %8.1f %10.3f %10.3f", mTotalShiftX[j], mTotalShiftY[j],
        mCalBinning * mTotalShiftX[j] / xOut, mCalBinning * mTotalShiftY[j] / xOut);
    }
    SEMTrace('c', "X and Y shifts for Y movements, total and per micron");
    for (j = numX; j < numX + numY - 1; j++) {
      mat->xpy += (float)(mCalBinning * mTotalShiftX[j] / (yOut * (numY - 1)));
      mat->ypy += (float)(mCalBinning * mTotalShiftY[j] / (yOut * (numY - 1)));
      SEMTrace('c', "%8.1f %8.1f %10.3f %10.3f", mTotalShiftX[j], mTotalShiftY[j],
        mCalBinning * mTotalShiftX[j] / yOut, mCalBinning * mTotalShiftY[j] / yOut);
    }

    // Get the biggest deviation from the mean matrix values
    matRange = 0.;
    for (j = 0; j < numX - 1; j++) {
      diff = fabs(mCalBinning * mTotalShiftX[j] / xOut - mat->xpx);
      if (matRange < diff)
        matRange = diff;
      diff = fabs(mCalBinning * mTotalShiftY[j] / xOut - mat->ypx);
      if (matRange < diff)
        matRange = diff;
    }
    for (j = numX; j < numX + numY - 1; j++) {
      diff = fabs(mCalBinning * mTotalShiftX[j] / yOut - mat->xpy);
      if (matRange < diff)
        matRange = diff;
      diff = fabs(mCalBinning * mTotalShiftY[j] / yOut - mat->ypy);
      if (matRange < diff)
        matRange = diff;
    }

    matMax = MatrixMaximum(mat);
    diff = 100. * matRange / matMax;

    // Derive an angle from the matrix, but multiply by specimen to stage to work out the
    // negatives and inversions and get a specimen to camera angle
    oldMat = MatMul(mSM->FallbackSpecimenToStage(1., 1.), *mat);
    xtheta = atan2(oldMat.ypx, oldMat.xpx) / DTOR;
    ytheta = atan2(-oldMat.xpy, oldMat.ypy) / DTOR;
    angle = UtilGoodAngle(xtheta + 0.5 * UtilGoodAngle(ytheta - xtheta));

    // Derive the specimen to stage
    oldMat = MatMul(mSM->SpecimenToCamera(mCurrentCamera, mCalIndex), MatInv(*mat));

    // 12/27/06: deleted alignment of images when num = 6

    report.Format("Camera %d, %dx: the scale matrix is %8.2f  %8.2f  %8.2f  %8.2f\r\n"
      "The maximum deviation of one estimate from the mean\r\n"
      " is %8.2f (%.1f%% of maximum scale value)\r\n"
      "The implied Specimen to Stage matrix is %8.4f  %8.4f  %8.4f  %8.4f\r\n",
      mCurrentCamera, magVal, mat->xpx, mat->xpy, mat->ypx, mat->ypy, matRange, diff,
      oldMat.xpx, oldMat.xpy, oldMat.ypx, oldMat.ypy);
    mWinApp->AppendToLog(report,
      mFromMacro ? LOG_OPEN_IF_CLOSED : LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN);
    StopCalibrating();
    if (MatrixIsAsymmetric(mat, stageSymCrit)) {
      report.Format("This matrix is not very close to symmetric and is likely\n"
        "to be wrong.  Do you want to forget about this calibration?\n\n"
        "(Perhaps try again with Whole Image Correlations turned %s.",
        mWholeCorrForStageCal ? "OFF" : "ON");
      if (AfxMessageBox(report, MB_QUESTION) == IDYES)
        return;
    }
    report = "";
    if (fabs(UtilGoodAngle(ytheta - xtheta)) < 90) {
      rotmess.Format("The implied rotation angle is %8.1f degrees", angle);
      diff = mMagTab[mCalIndex].rotation[mCurrentCamera];
      if (mCamParams[mCurrentCamera].STEMcamera)
        diff = mCamParams[mCurrentCamera].imageRotation;
      if (fabs(UtilGoodAngle(angle - diff)) > 75) {
        report += rotmess + "\r\n\r\n";
        rotmess.Format("The rotation angle from this calibration, %.1f, is far from the"
          "\r\nangle implied by the current properties and calibrations, %.1f.\r\n"
          "Something is wrong, probably in the GlobalExtraRotation property\r\n"
          "or the ExtraRotation camera propertes.  See the Help or get help.", angle,
          diff);
        AfxMessageBox(rotmess, MB_EXCLAME);
      }
    } else {
      rotmess.Format("\r\nThe property InvertStageXaxis %s set and the angles from\r\n"
        "this calibration imply that it %s be set!\r\n(angle = %.1f, xtheta = %.1f, "
        "ytheta = %.1f)\r\n\r\nIf this was otherwise a good calibration, fix that "
        "property and\r\nrerun this"
        " calibration to get a correct rotation angle.", mSM->GetInvertStageXAxis() ?
        "IS" : "is NOT", mSM->GetInvertStageXAxis() ? "should NOT" : "SHOULD", angle,
        xtheta, ytheta);
      AfxMessageBox(rotmess, MB_EXCLAME);
    }
    report += rotmess;
    mWinApp->AppendToLog(report,
      mFromMacro ? LOG_OPEN_IF_CLOSED : LOG_MESSAGE_IF_NOT_ADMIN_OR_OPEN);


    mMagTab[mCalIndex].matStage[mCurrentCamera] = workMat;
    mMagTab[mCalIndex].stageCalFocus[mCurrentCamera] = mScope->GetFocus();
    mMagTab[mCalIndex].stageCalibrated[mCurrentCamera] = mWinApp->MinuteTimeStamp();
    mWinApp->mDocWnd->SetShortTermNotSaved();
    mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_STAGE);

    if (mCalIndex < mScope->GetLowestMModeMagInd()) {
      report.Format("Absolute focus value = %f",
        mMagTab[mCalIndex].stageCalFocus[mCurrentCamera]);
      mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);
    }
    return;
  }

  // IMAGE SHIFT CALIBRATION

  if (mCalIsHighFocus) {
    oldInv = MatInv(mFirstMat);
    mat = &mFirstMat;
    strSource = "starting";
  } else {
    oldInv = mSM->CameraToIS(mCalIndex);
    mat = &mMagTab[mCalIndex].matIS[mCurrentCamera];
    if (mat->xpx) {
      strSource = "directly measured";
      oldIsDirect = !mMagTab[mCalIndex].calibrated[mCurrentCamera];
    }
  }

  // But first time around, store matrix in local member and compute one drift estimate
  // from the two differences in X, adjust each positive shift by the drift
  if (mFirstCal) {
    mat = &mFirstMat;
    minDriftX = 0.5 * (mTotalShiftX[0] + mTotalShiftX[1]);
    minDriftY = 0.5 * (mTotalShiftY[0] + mTotalShiftY[1]);
    delX0 = mTotalShiftX[0] - minDriftX;
    delY0 = mTotalShiftY[0] - minDriftY;
    delX2 = mTotalShiftX[2] - minDriftX;
    delY2 = mTotalShiftY[2] - minDriftY;
    mat->xpx = (float)(mCalBinning * delX0 / mCalOutX[1]);
    mat->ypx = (float)(mCalBinning * delY0 / mCalOutX[1]);
    mat->xpy = (float)(mCalBinning * delX2 / mCalOutY[3]);
    mat->ypy = (float)(mCalBinning * delY2 / mCalOutY[3]);
    report.Format("%dx: first round, drift = %.1f %.1f binned pixels, matrix = %8.1f  "
      "%8.1f  %8.1f  %8.1f", magVal, minDriftX, minDriftY, mat->xpx, mat->xpy, mat->ypx,
      mat->ypy);
    mWinApp->AppendToLog(report, action);
    mScope->SetImageShift(mBaseISX, mBaseISY);
    mSM->SetISTimeOut(mCalDelayFactor * mSM->GetLastISDelay());
    if (sqrt(delX0 * delX0 + delY0 * delY0) < 5. ||
      sqrt(delX2 * delX2 + delY2 * delY2) < 5.) {
      report.Format("The image displacement was less than 5 pixels in one of the two "
        "directions.\r\nThis is too small to give a good estimate for proceeding.\r\n"
        "\r\nInsert a calibrated pixel size into a \"RotationAndPixel\" camera property"
        " for this mag\r\nOr, if this is a cross-line grating, get an image and run"
        " the\r\nFind Pixel Size routine at this mag before trying again.");
      AfxMessageBox(report, MB_EXCLAME);
      mWinApp->AppendToLog(report);
      return;
    }
    mShiftIndex = -1;
    CalibrateIS(1, false, mFromMacro);
    return;
  }

  float driftX, driftY, ISdriftX[4], ISdriftY[4];
  matRange = 0.;
  if (mSixPointIScal) {
    driftX = mTotalShiftX[mNumShiftCal] / 3.f;
    driftY = mTotalShiftY[mNumShiftCal] / 3.f;
    mat->xpx = (float)(-mCalBinning * 0.5 * (mTotalShiftX[0] + mTotalShiftX[1] - 
      2. * driftX) / mCalOutX[0]);
    mat->ypx = (float)(-mCalBinning * 0.5 * (mTotalShiftY[0] + mTotalShiftY[1] -
      2. * driftY) / mCalOutX[0]);
    mat->xpy = (float)(-mCalBinning * 0.5 * (mTotalShiftX[3] + mTotalShiftX[4] -
      2. * driftX) / mCalOutY[3]);
    mat->ypy = (float)(-mCalBinning * 0.5 * (mTotalShiftY[3] + mTotalShiftY[3] -
      2. * driftY) / mCalOutY[3]);
    diff = fabs(mCalBinning * (mTotalShiftX[0] - mTotalShiftX[1]) / mCalOutX[0]);
    ACCUM_MAX(matRange, diff);
    diff = fabs(mCalBinning * (mTotalShiftY[0] - mTotalShiftY[1]) / mCalOutX[0]);
    ACCUM_MAX(matRange, diff);
    diff = fabs(mCalBinning * (mTotalShiftX[3] - mTotalShiftX[4]) / mCalOutY[3]);
    ACCUM_MAX(matRange, diff);
    diff = fabs(mCalBinning * (mTotalShiftY[3] - mTotalShiftY[4]) / mCalOutY[3]);
    ACCUM_MAX(matRange, diff);

  } else {

    // Average the 4 shifts with the proper polarity; multiply to get from
    // binned to unbinned pixels per IS unit
    mat->xpx = (float)(mCalBinning * 0.25 * (mTotalShiftX[0] + mTotalShiftX[3] -
      (mTotalShiftX[1] + mTotalShiftX[2])) / mCalOutX[1]);
    mat->ypx = (float)(mCalBinning * 0.25 * (mTotalShiftY[0] + mTotalShiftY[3] -
      (mTotalShiftY[1] + mTotalShiftY[2])) / mCalOutX[1]);
    mat->xpy = (float)(mCalBinning * 0.25 * (mTotalShiftX[4] + mTotalShiftX[7] -
      (mTotalShiftX[5] + mTotalShiftX[6])) / mCalOutY[5]);
    mat->ypy = (float)(mCalBinning * 0.25 * (mTotalShiftY[4] + mTotalShiftY[7] -
      (mTotalShiftY[5] + mTotalShiftY[6])) / mCalOutY[5]);


    // Give a report, figure out the range of drift estimates and of matrix values
    minDriftX = 10000.;
    maxDriftX = -10000.;
    minDriftY = 10000.;
    maxDriftY = -10000.;
    ScaleMat matInv = MatInv(*mat);
    float diffX, diffY;

    for (int i = 0; i < 8; i += 2) {
      diffX = mTotalShiftX[i] + mTotalShiftX[i + 1];
      minDriftX = B3DMIN(minDriftX, diffX);
      maxDriftX = B3DMAX(maxDriftX, diffX);
      diffY = mTotalShiftY[i] + mTotalShiftY[i + 1];
      minDriftY = B3DMIN(minDriftY, diffY);
      maxDriftY = B3DMAX(maxDriftY, diffY);
      ISdriftX[i / 2] = 1000. * (matInv.xpx * diffX + matInv.xpy * diffY);
      ISdriftY[i / 2] = 1000. * (matInv.ypx * diffX + matInv.ypy * diffY);
    }
    driftRange = maxDriftX - minDriftX;
    if (driftRange < maxDriftY - minDriftY)
      driftRange = maxDriftY - minDriftY;
    diff = fabs(mCalBinning * 0.5 * ((mTotalShiftX[0] - mTotalShiftX[1]) -
      (mTotalShiftX[3] - mTotalShiftX[2])) / mCalOutX[1]);
    if (matRange < diff)
      matRange = diff;
    diff = fabs(mCalBinning * 0.5 * ((mTotalShiftY[0] - mTotalShiftY[1]) -
      (mTotalShiftY[3] - mTotalShiftY[2])) / mCalOutX[1]);
    if (matRange < diff)
      matRange = diff;
    diff = fabs(mCalBinning * 0.5 * ((mTotalShiftX[4] - mTotalShiftX[5]) -
      (mTotalShiftX[7] - mTotalShiftX[6])) / mCalOutY[5]);
    if (matRange < diff)
      matRange = diff;
    diff = fabs(mCalBinning * 0.5 * ((mTotalShiftY[4] - mTotalShiftY[5]) -
      (mTotalShiftY[7] - mTotalShiftY[6])) / mCalOutY[5]);
    if (matRange < diff)
      matRange = diff;
  }
  matMax = MatrixMaximum(mat);
  diff = 100. * matRange / matMax;

  // 2/17/04: doubled these since differences weren't being computed right before
  int mess = 0;
  float lowFactor = mMagIndex < mScope->GetLowestNonLMmag(&mCamParams[mCurrentCamera]) ? 
    2. : 1.;
  if (diff > 0.8 * lowFactor)
    mess = 1;
  if (diff > 2.0 * lowFactor)
    mess = 2;
  if (diff > 10.0 * lowFactor)
    mess = 3;

  if (mSixPointIScal) {
    report.Format("The scale matrix is %8.1f  %8.1f  %8.1f  %8.1f    drift = %.1f %.1f "
      "pixels\r\nMaximum range between paired estimates: %8.1f"
      " (%4.1f%% of maximum scale value)\r\n%s", mat->xpx, mat->xpy, mat->ypx, mat->ypy,
      driftX, driftY, matRange, diff, calMessage[mess]);
  } else {
    report.Format("Camera %d, %dx: the scale matrix is %8.1f  %8.1f  %8.1f  %8.1f\r\n"
      "The maximum range between paired estimates\r\n is %8.1f"
      " (%4.1f%% of maximum scale value)\r\n"
      "The maximum range of drift estimates is %6.1f pixels.\r\n\r\n%s",
      mCurrentCamera, magVal, mat->xpx, mat->xpy, mat->ypx, mat->ypy,
      matRange, diff, driftRange, calMessage[mess]);
  }
  mWinApp->AppendToLog(report, action);
  if (!mWinApp->GetSTEMMode() && mCalIndex < mScope->GetLowestMModeMagInd()) {
    report.Format("Absolute focus value = %f", mScope->GetFocus());
    mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);
  }

  if (!mSixPointIScal)
    SEMTrace('1', "Drift converted to IS in nm:\r\n%6.1f %6.1f"
      "\r\n%6.1f %6.1f\r\n%6.1f %6.1f\r\n%6.1f %6.1f", 
      ISdriftX[0], ISdriftY[0],
      ISdriftX[1], ISdriftY[1],
      ISdriftX[2], ISdriftY[2],
      ISdriftX[3], ISdriftY[3]);

  // Now compute an average percentage change in positions implied by the
  // change of matrix, and report to log window
  if (oldInv.xpx && !mCalIsHighFocus) {
    delMat = MatMul(oldInv, *mat);
    diff = 25. * (sqrt(pow(delMat.xpx - 1., 2.) + delMat.ypx * delMat.ypx) +
      sqrt(pow(delMat.ypy - 1., 2.) + delMat.xpy * delMat.xpy) +
      sqrt(0.5*(pow(delMat.xpx - 1. + delMat.xpy, 2.) + 
        pow(delMat.ypy - 1. + delMat.ypx, 2.))) +
      sqrt(0.5*(pow(-delMat.xpx + 1. + delMat.xpy, 2.) + 
        pow(delMat.ypy - 1. - delMat.ypx, 2.))));
    report.Format("This is a change of %.2f%% from the old %s matrix", diff, strSource);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_NOT_ADMIN_OR_OPEN);
  }

  if (mess == 3 && AfxMessageBox("Do you want the program to forget about this "
    "calibration?\n\n(Try rerunning with \"IS from scratch\")", MB_QUESTION) == IDYES) {
      mat->xpx = mat->xpy = 0.;
      StopCalibrating();
      return;
  }

  if (mCalIsHighFocus) {
    FinishHighFocusIScal();
    StopCalibrating();
    return;
  }

  mMagTab[mCalIndex].calibrated[mCurrentCamera] = mWinApp->MinuteTimeStamp();
  mWinApp->mDocWnd->SetShortTermNotSaved();

  // If it was directly measured and this was the first recalibration of this mag,
  // ask whether to change all directly measured ones by this amount
  // Allow this if either the change is small or neither matrix is very asymmetric
  int iCam = mCurrentCamera;
  if (oldIsDirect && !mFromMacro && (diff < transferChangeLim || 
    !(MatrixIsAsymmetric(&oldInv, transferSymCrit) || 
    MatrixIsAsymmetric(mat, transferSymCrit)))) {

    // First assess whether there are any others in mag range that are eligible - but if
    // there is possible transfer to a mag that has already been transferred from, skip it
    int limlo, limhi, minMagx = 100000000, maxMagx = 0;
    int numToDo = 0;
    mWinApp->GetMagRangeLimits(iCam, mCalIndex, limlo, limhi);
    for (int iMag = limlo; iMag <= limhi; iMag++) {
      if (mSM->CanTransferIS(mCalIndex, iMag, mCamParams[iCam].STEMcamera, 
        mCamParams[iCam].GIF ? 1 : 0)) {
        if (mMagsTransFrom.count(iMag) > 0) {
          numToDo = 0;
          break;
        }

        if (!mMagTab[iMag].calibrated[iCam] && mMagTab[iMag].matIS[iCam].xpx != 0.) {
          numToDo++;
          j = MagOrEFTEMmag(mCamParams[iCam].GIF, iMag, mCamParams[iCam].STEMcamera);
          minMagx = B3DMIN(minMagx, j);
          maxMagx = B3DMAX(maxMagx, j);
        }
      }
    }
    report.Format("Do you want to modify all other old, directly measured image "
      "shift\r\ncalibration matrices for this camera and mag range by the same change?"
      "\r\n     (%d matrices, mags %d to %d)", numToDo, minMagx, maxMagx);
    if (numToDo > 0 && AfxMessageBox(report, MB_QUESTION) == IDYES) {
      for (int iMag = limlo; iMag <= limhi; iMag++) {
        if (!mMagTab[iMag].calibrated[iCam] && mMagTab[iMag].matIS[iCam].xpx != 0. && 
          mSM->CanTransferIS(mCalIndex, iMag, mCamParams[iCam].STEMcamera,
            mCamParams[iCam].GIF ? 1 : 0))
          mMagTab[iMag].matIS[iCam] = MatMul(mMagTab[iMag].matIS[iCam], delMat);
      }
      mMagsTransFrom.insert(mCalIndex);
    }

    // An old idea that seems not to be any good if you think about it
    /* if (mWinApp->GetAdministrator() && mWinApp->mFocusManager->FocusReady() && 
      !mFocusModified && mCalIndex >= mScope->GetLowestMModeMagInd() &&
      !mCamParams[iCam].STEMcamera && AfxMessageBox("Do you want to modify all old\r\n" 
      "focus calibration tables by the same change?", 
      MB_YESNO | MB_ICONQUESTION) == IDYES) {
      mWinApp->mFocusManager->ModifyAllCalibrations(delMat, mCurrentCamera);
      mFocusModified = true;
    } */
  }

  // Recompute all pixel sizes and rotations with new IS information
  mSM->PropagatePixelSizes();
  mSM->PropagateRotations();
  StopCalibrating();
  mWinApp->mDocWnd->CalibrationWasDone(CAL_DONE_IS);
}

// Common routine for stopping the process and restoring shifts
void CShiftCalibrator::StopCalibrating()
{
  StageMoveInfo smi;
  if (mShiftIndex < 0)
    return;
  if (mCalStage) {
    smi.x = mBaseISX;
    smi.y = mBaseISY;
    smi.axisBits = axisXY;
    mScope->MoveStage(smi, false);
    mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
  } else {
    mScope->SetImageShift(mBaseISX, mBaseISY);
    mSM->SetISTimeOut(mSM->GetLastISDelay());
  }
  mCamera->StopCapture(1);
  mCamera->SetRequiredRoll(0);
  mShiftIndex = -1;
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
}

// Determine optimal step size and number of steps for a stage calibration to span the
// given cycle length
void CShiftCalibrator::OptimizeStageSteps(float cycle, double &xOut, int &numCal)
{
  double newOut;
  int multiple, numHalf;
  int halfMax = (MAX_SHIFTS / 2 - 1) / 2;   // Maximum moves away from center
  
  // Reduce xOut to be less than half the maximum extent
  if (xOut > mMaxStageCalExtent / 2.)
    xOut = B3DMIN(500., B3DMAX(xOut / 5., mMaxStageCalExtent / 2.));
  
  // Get number of moves from center and total steps
  numHalf = (cycle / 2.) / xOut + 0.95;
  if (numHalf < 1)
    numHalf = 1;
  if (numHalf > halfMax)
    numHalf = halfMax;
  numCal = 2 * numHalf + 1;

  // Find how many times smaller the half-cycle is than the half-field, and if > 1
  // set step to an exact multiple of the half-cycle
  multiple = (int)(xOut / (cycle / 2.));
  if (multiple > 1) {
    xOut = multiple * cycle / 2.;
  } else {

    // Otherwise reduce xOut to give exactly one cycle
    newOut = (cycle / 2.) / numHalf;
    if (newOut < xOut)
      xOut = newOut;
  }
}

// Compute maximum absolute value of matrix elements
float CShiftCalibrator::MatrixMaximum(ScaleMat *mat)
{
  float diff, matMax;
  matMax = fabs(mat->xpx);
  diff = fabs(mat->xpy);
  if (diff > matMax)
    matMax = diff;
  diff = fabs(mat->ypx);
  if (diff > matMax)
    matMax = diff;
  diff = fabs(mat->ypy);
  if (diff > matMax)
    matMax = diff;
  return matMax;
}

// Return true if the matrix asymmetry exceeds the criterion factor
bool CShiftCalibrator::MatrixIsAsymmetric(ScaleMat *mat, float symCrit)
{
  float matMax = MatrixMaximum(mat);
  return (fabs(fabs(mat->xpx) - fabs(mat->ypy)) > symCrit * matMax ||
      fabs(fabs(mat->xpy) - fabs(mat->ypx)) > symCrit * matMax);
}

///////////////////////////////////////////////////////////////////////////////////
// IMAGE SHIFT OFFSET CALIBRATIONS

// Start calibrating IS offset
void CShiftCalibrator::CalibrateISoffset(void)
{
  BOOL normalize = FEIscope;
  CString str , str2;
  CString *modeNames = mWinApp->GetModeNames();
  SetFirstCameras();
  BOOL bothExist = mFirstGIFCamera >= 0 && mFirstRegularCamera >= 0;
  int curCam = mWinApp->GetCurrentCamera();
  BOOL EFTEM = mWinApp->GetEFTEMMode();
  int firstCam = EFTEM ? mFirstGIFCamera : mFirstRegularCamera;
  mMagIndex = mScope->GetMagIndex();
  if (mWinApp->LowDoseMode()) {
    AfxMessageBox("You cannot do this calibration in Low Dose mode", MB_EXCLAME);
    return;
  }
  if (bothExist && mScope->GetSecondaryMode()) {
    AfxMessageBox("This calibration cannot be done in the secondary\n"
      " magnification mode when there is a GIF", MB_EXCLAME);
    return;
  }

  if (bothExist && mGIFofsFromMag && mGIFofsToMag) {

    // If two cameras exist and there is already a bridging offset, check for image shift
    // calibration in both modes and insist on the right camera
    if (!BothModesHaveIScal())
      return;
    if (curCam != firstCam) {
      str.Format("In %sEFTEM mode, this calibration should be done with "
        "the %s camera, but %s is the current camera.", EFTEM ? "" : "non-",
        (LPCTSTR)mCamParams[firstCam].name, (LPCTSTR)mCamParams[curCam].name);
      AfxMessageBox(str, MB_EXCLAME);
      return;
    }
    MagRangeWithoutIScal(curCam);
  } else {

    // Otherwise just check for image shift cals in the current camera
    int miss = MagRangeWithoutIScal(curCam);
    if (miss) {
      str.Format("This calibration should not be done until there is at least one\n"
        "image shift calibration in every mag range.\n\n"
        "There is no calibration for the mag range including %dx for the %s camera.",
        MagOrEFTEMmag(EFTEM, miss), (LPCTSTR)mCamParams[curCam].name);
      AfxMessageBox(str, MB_EXCLAME);
      return;
    }
  }

  if (mLowestISOmag >= mMagIndex) {
    str.Format("There is no image shift calibration at %s magnification\n"
      "so nothing can be calibrated from this mag", 
      mLowestISOmag > mMagIndex ? "this" : "the next lower");
    AfxMessageBox(str, MB_EXCLAME);
    return;
  }

  str = "To do this calibration:\r\n"
    "   1. Before starting this routine, find a feature that can be\r\n"
    "recognized and centered over the full mag range that is\r\n"
    "required, and go to the highest mag to be calibrated.\r\n"
    "   2. Start this procedure, then center the feature on the\r\n"
    "camera, using stage shift and image shift as necessary.\r\n"
    "   3. Press NextMag in the Camera & Script Tools control panel\r\n"
    "to record the image shift and step down to the next lower mag.\r\n"
    "   4. Center the feature on the camera using only image shift,\r\n"
    "and press NextMag to record image shift and go on.  Repeat\r\n"
    "this step until finished.\r\n"
    "   5. Press End to end the procedure and record the image\r\n"
    "shift at the last mag.  Press STOP to stop the procedure\r\n"
    "without recording image shift at the current mag.\r\n"
    "   6. In Low Mag mode, try to keep the beam nearly centered.";
  if (mScope->GetStandardLMFocus(mScope->GetLowestMModeMagInd() - 1) > -900.)
    str += "\r\nFocus will be set to a standard value when you enter Low Mag.";
  if (mLowestISOmag > 1) {
    str2.Format("\r\n   The calibration can only be done down to %dx\r\n"
      "because it can only be done through a continuous set of mags with calibrated "
      "image shift.",
      MagOrEFTEMmag(EFTEM, mLowestISOmag));
    str += str2;
  }
  if (AfxMessageBox(str + "\n\nAre you ready to proceed?", MB_YESNO | MB_ICONQUESTION) ==
    IDNO)
    return;
  mTakeTrial = AfxMessageBox("Do you want a " + modeNames[2] + " image taken after every"
    " mag change?\n\n(This will not happen when going into Low Mag mode or\nif the camera"
    " is acquiring in continuous mode)", MB_YESNO | MB_ICONQUESTION) 
    == IDYES;

  mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  mCalIndex = mMagIndex;

  // If in low mag already, go to a standard focus if one is defined
  mScope->SetFocusToStandardIfLM(mCalIndex);

  mSavedMouseStage = mSM->GetMouseMoveStage();
  mSM->SetMouseMoveStage(false);

  mCalISOindex = 0;
  mWinApp->mCameraMacroTools.CalibratingISoffset(true);
  mWinApp->SetStatusText(MEDIUM_PANE, "Calibrating shift offsets");
  mWinApp->UpdateBufferWindows();

  if (normalize) {
    mScope->NormalizeProjector();
    if (!mCamera->DoingContinuousAcquire())
      mCamera->InitiateCapture(2);
    mWinApp->AppendToLog("\r\nProjector was normalized; check position again", 
      LOG_OPEN_IF_CLOSED);
  }
}

// Process next mag when calibrating offsets
void CShiftCalibrator::CalISoffsetNextMag(void)
{
  CString str;
  int newIndex, magIndex = mScope->GetMagIndex();
  
  // Check against current mag, if it doesn't match, complain and reset
  if (mMagIndex != magIndex) {
    mScope->SetMagIndex(mMagIndex);
    str.Format("Going back to %dx because you must do every mag in sequence.\n\n"
      "If you need to skip this mag, press STOP and run the procedure\n"
      "again starting at the next lower mag.\nOtherwise center the feature at this mag",
      MagOrEFTEMmag(mWinApp->GetEFTEMMode(), mMagIndex));
    AfxMessageBox(str, MB_EXCLAME);
    return;
  }

  // First time, keep track of stage
  if (!mCalISOindex)
    mScope->GetStagePosition(mCalISOstageX, mCalISOstageY, mCalISOstageZ);

  // If it is the last mag, treat as an end
  newIndex = mWinApp->FindNextMagForCamera(mWinApp->GetCurrentCamera(), mMagIndex, -1);
  if (newIndex <= 0 || newIndex < mLowestISOmag) {
    CalISoffsetDone(false);
    return;
  }

  // If stage moved, treat as a stop
  if (CheckStageGetShift()) {
    CalISoffsetDone(true);
    return;
  }

  // Drop the mag and continue
  mMagIndex = newIndex;
  mScope->SetMagIndex(mMagIndex);
  mScope->SetFocusToStandardIfLM(mMagIndex);
  if (!mScope->BothLMorNotLM(mMagIndex, false, magIndex, false)) {
    mWinApp->AppendToLog("\r\nEntered Low Mag - check beam before taking a picture.",
      LOG_OPEN_IF_CLOSED);
  } else if (mTakeTrial && !mCamera->DoingContinuousAcquire())
    mCamera->InitiateCapture(2);
}

// Cal offset procedure is done, with or without a STOP
void CShiftCalibrator::CalISoffsetDone(BOOL ifStop)
{ 
  CString str;
  int i, iDiff, iStart, iEnd, loop, zeroMode, zeroInd, zeroMag, minDiff, numLoop;
  double ISX, ISY, delISX, delISY, diffX, diffY;
  int gif = mWinApp->GetEFTEMMode() ? 1 : 0;
  BOOL bothExist = mFirstGIFCamera >= 0 && mFirstRegularCamera >= 0;
  BOOL skipPrimary, inSecondary = mScope->GetSecondaryMode();
  int secondary = mScope->GetLowestSecondaryMag();
  int lowestM = mScope->GetLowestMModeMagInd();
  if (!ifStop) {
    if (mMagIndex != mScope->GetMagIndex()) {
    str.Format("Because you are not at the expected mag, %dx, no image shift is being\n"
      "recorded and this is being treated the same as stopping the procedure\n",
      MagOrEFTEMmag(gif != 0, mMagIndex));
      AfxMessageBox(str, MB_EXCLAME);
    } else {
      ifStop = CheckStageGetShift();
    }
  }

  if (ifStop && mCalISOindex > 1) {
    str.Format("The procedure has been stopped rather than ended.\n"
      "Shifts are available at %d mags.\n\nDo you want to incorporate them into the"
      " calibration (Yes) or abandon them (No)?", mCalISOindex);
    ifStop = AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDNO;
  }

  if (!ifStop && mCalISOindex > 1) {
    zeroMode = gif;
    if (bothExist && mGIFofsFromMag > 0 && mGIFofsToMag > 0)
      zeroMode = GetZeroOffsetMode();
    if (zeroMode == gif) {
      iStart = lowestM;
      iEnd = MAX_MAGS;
      if (secondary) {
        if (AfxMessageBox("You need to pick a mag in either the primary or\n"
          "the secondary mag mode at which the offset will be zero\n\n"
          "Do you want the zero offset in the primary mag mode?", MB_QUESTION) == IDYES) {
            iEnd = secondary;
        } else {
          iStart = secondary;
        }
      }
      zeroMag = gif ? mMagTab[mCalIndex].EFTEMmag : mMagTab[mCalIndex].mag;
      KGetOneInt("Mag at which to set offset to zero (NOT in low mag):", zeroMag);
      minDiff = 100000000;
      zeroInd = mCalIndex;
      for (i = iStart; i < iEnd; i++) {
        if (!mMagTab[i].mag)
          continue;
        iDiff = zeroMag - (gif ? mMagTab[i].EFTEMmag : mMagTab[i].mag);
        if (iDiff < 0)
          iDiff = -iDiff;
        if (iDiff < minDiff) {
          minDiff = iDiff;
          zeroInd = i;
        }
      }
    }

    // Complete the table by transferring any existing calibrations outside the range
    // Taking the difference at the end of the range, transferring to each other mag
    // First time through, go from top mag calibrated up to end of range
    iDiff = mCalIndex;
    iStart = mCalIndex + 1;
    iEnd = MAX_MAGS;
    numLoop = 2;
    skipPrimary = false;

    // But if there is a secondary mag range, add another loop and either skip the primary
    // mags or end before secondary ones
    if (secondary) {
      numLoop = 3;
      if (inSecondary) {
        skipPrimary = true;
      } else {
        iEnd = secondary;
      }
    }
    for (loop = 0; loop < numLoop; loop++) {
      delISX = mCalISX[iDiff] - mMagTab[iDiff].calOffsetISX[gif];
      delISY = mCalISY[iDiff] - mMagTab[iDiff].calOffsetISY[gif];
      for (i = iStart; i < iEnd; i++) {
        if (!mMagTab[i].mag)
          continue;
        if (skipPrimary && i >= lowestM && i < secondary)
          continue;
        mSM->TransferGeneralIS(iDiff, delISX, delISY, i, ISX, ISY);
        mCalISX[i] = mMagTab[i].calOffsetISX[gif] + ISX;
        mCalISY[i] = mMagTab[i].calOffsetISY[gif] + ISY;
      }

      // For second loop, go from bottom index calibrated down to 1
      if (!loop) {
        iDiff = mCalIndex + 1 - mCalISOindex;
        iStart = 1;
        iEnd = iDiff;
      } else if (secondary) {

        // For third loop, use the difference at the top of LM and propagate it in the
        // primary mode if in secondary, or secondary if not
        iDiff = lowestM - 1;
        skipPrimary = false;
        if (inSecondary) {
          iStart = lowestM;
          iEnd = secondary;
        } else {
          iStart = secondary;
          iEnd = MAX_MAGS;
        }
      }
    }

    // Copy table into place, shifting to zero at indicated mag if this is the mode
    delISX = delISY = 0.;
    if (zeroMode == gif) {
      delISX = -mCalISX[zeroInd];
      delISY = -mCalISY[zeroInd];
    }
    for (i = 1; i < MAX_MAGS; i++) {
      ISX = ISY = 0.;
      if (!mMagTab[i].mag)
        continue;
      if (zeroMode == gif) 
        mSM->TransferGeneralIS(zeroInd, delISX, delISY, i, ISX, ISY);
      diffX = -mMagTab[i].calOffsetISX[gif] + mCalISX[i] + ISX;
      diffY = -mMagTab[i].calOffsetISY[gif] + mCalISY[i] + ISY;
      mMagTab[i].calOffsetISX[gif] = mCalISX[i] + ISX;
      mMagTab[i].calOffsetISY[gif] = mCalISY[i] + ISY;
      SEMTrace('1', "Mag %d  measured/extended shift %.3f, %.3f   New offset %.3f, %.3f"
        "  change %.3f, %.3f", i, mCalISX[i], mCalISY[i], mMagTab[i].calOffsetISX[gif],
        mMagTab[i].calOffsetISY[gif], diffX, diffY);
    }

    // If there is another mode with a bridging offset, need to either fix the present
    // table with the offset or fix the other table
    if (bothExist && mGIFofsFromMag > 0 && mGIFofsToMag > 0)
      AdjustNonzeroMode(1 - zeroMode);

    mScope->SetApplyISoffset(mScope->GetApplyISoffset());
    mWinApp->SetCalibrationsNotSaved(true);
    str.Format("Shift offsets saved for %d mags", mCalISOindex);
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  }

  mSM->SetMouseMoveStage(mSavedMouseStage);
  mCalISOindex = -1;
  mWinApp->mCameraMacroTools.CalibratingISoffset(false);
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
}

// Check the stage shift, abort if too big and return true, or record shift,
// increment index, and return false
BOOL CShiftCalibrator::CheckStageGetShift(void)
{
  CString report;
  double stageX, stageY, stageZ, ISX, ISY;

  // Only test X and Y since Z output wobbles on JEOL
  mScope->GetStagePosition(stageX, stageY, stageZ);
  if (fabs(stageX - mCalISOstageX) > mCalISOstageLimit || 
    fabs(stageY - mCalISOstageY) > mCalISOstageLimit) {
      report.Format("The stage appears to have moved by %.3f in X and %.3f in Y\n"
        "(more than ISoffsetCalStageLimit = %f).\n"
        "The alignment image shift cannot be relied on at this mag.\n\n"
        "The procedure is being STOPPED without recording an image shift for this mag.",
        stageX - mCalISOstageX, stageY - mCalISOstageY, mCalISOstageLimit);
      AfxMessageBox(report,  MB_EXCLAME);
    return true;
  }

  // Get centered image shift and add back the current offset if any
  mScope->GetLDCenteredShift(ISX, ISY);
  mCalISX[mMagIndex] = ISX + mMagTab[mMagIndex].offsetISX;
  mCalISY[mMagIndex] = ISY + mMagTab[mMagIndex].offsetISY;
  mCalISOindex++;
  return false;
}

// Calibrate the nonGIF/GIF offset at one mag
void CShiftCalibrator::CalibrateEFTEMoffset(void)
{
  int bufNum = mWinApp->mBufferManager->GetShiftsOnAcquire() + 1;
  int fromMag, toMag, zeroMode;
  float imshfX, imshfY, stageX, stageY;
  double delISX, delISY, ISX, ISY;
  char letter = 'A' + bufNum;
  SetFirstCameras();
  if (!BothModesHaveIScal())
    return;
  CString letStr = letter;
  CString str = "To calibrate the offset between nonGIF and GIF cameras:\r\n"
    "   1. While not in EFTEM mode and not in Low Mag mode, center a recognizable\r\n"
    "feature in an image on the " + mCamParams[mFirstRegularCamera].name + 
    " camera.\r\n"
    "   2. Take a new image if necessary so that there is an image with no\r\n"
    "alignment shift (no mouse or autoalign shifting).\r\n"
    "   3. Copy this image to buffer " + letStr + ".\r\n"
    "   4. Go to EFTEM mode and center the feature by taking images on the " + 
    mCamParams[mFirstGIFCamera].name + " camera,\r\n"
    "using only image shift to align the feature.\r\n"
    "   5. Invoke the Calibrate EFTEM IS Offset command.";
  if (AfxMessageBox(str + "\n\nHave you already done all of those steps?",
    MB_YESNO | MB_ICONQUESTION) == IDNO) {
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
    return;
  }

  if (!mImBufs[bufNum].mImage || !mImBufs->mImage) {
    AfxMessageBox("There are not images in both of the required buffers", MB_EXCLAME);
    return;
  }
  mImBufs[bufNum].mImage->getShifts(imshfX, imshfY);
  if (imshfX || imshfY) {
    AfxMessageBox("The image in buffer " + letStr + "cannot be used, it has an alignment"
      " shift.", MB_EXCLAME);
    return;
  }

  mImBufs[bufNum].GetStagePosition(imshfX, imshfY);
  mImBufs->GetStagePosition(stageX, stageY);
  if (fabs(imshfX - stageX) > mCalISOstageLimit || 
    fabs(imshfY - stageY) > mCalISOstageLimit) {
    AfxMessageBox("The stage moved between these pictures so they cannot be used",
      MB_EXCLAME);
    return;
  }

  if (mImBufs[bufNum].mCamera != mFirstRegularCamera) {
    AfxMessageBox("The image in buffer " + letStr + "was not taken with the " +
       mCamParams[mFirstRegularCamera].name + " camera", MB_EXCLAME);
    return;
  }
  if (mImBufs->mCamera != mFirstGIFCamera) {
    AfxMessageBox("The image in buffer A was not taken with the " +
       mCamParams[mFirstGIFCamera].name + " camera", MB_EXCLAME);
    return;
  }
  fromMag = mImBufs[bufNum].mMagInd;
  toMag = mImBufs->mMagInd;
  if (fromMag < mScope->GetLowestMModeMagInd() ||
    fromMag < mScope->GetLowestMModeMagInd()) {
    AfxMessageBox("One of the images was taken in low mag mode", MB_EXCLAME);
    return;
  }

  // At last it is ready!
  // Get the image shifts and adjust them by existing offsets if the apply option was on
  // convert the shift in the regular image to a shift at this mag, 
  // and subtract to get the net shift
  mScope->GetLDCenteredShift(mGIFofsISX, mGIFofsISY);
  ISX = mImBufs[bufNum].mISX;
  ISY = mImBufs[bufNum].mISY;
  if (mScope->GetApplyISoffset()) {
    ISX += mMagTab[fromMag].calOffsetISX[0];
    ISY += mMagTab[fromMag].calOffsetISY[0];
    mGIFofsISX += mMagTab[toMag].calOffsetISX[1];
    mGIFofsISY += mMagTab[toMag].calOffsetISY[1];
  }
  ConvertIS(mFirstRegularCamera, fromMag, ISX, 
    ISY, mFirstGIFCamera, toMag, delISX, delISY);
  mGIFofsISX -= delISX;
  mGIFofsISY -= delISY;
  mGIFofsFromMag = fromMag;
  mGIFofsToMag = toMag;

  // Adjust the offsets of the mode not kept at zero and reapply them
  zeroMode = GetZeroOffsetMode();
  AdjustNonzeroMode(1 - zeroMode);
  mScope->SetApplyISoffset(mScope->GetApplyISoffset());
  mWinApp->SetCalibrationsNotSaved(true);
  str.Format("Shift offset from nonEFTEM mag %d to EFTEM mag %d: %.3f %.3f",
    fromMag, toMag, mGIFofsISX, mGIFofsISY);
  mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
}

// Convert an image shift from one camera/mag to another without assuming that
// different cameras have the same image shift-specimen relationship
void CShiftCalibrator::ConvertIS(int fromCam, int fromMag, double fromISX, 
                                 double fromISY, int toCam, int toMag, double &toISX,
                                 double &toISY)
{
  ScaleMat aProd;
  ScaleMat bMat = mSM->IStoGivenCamera(toMag, toCam);
  ScaleMat bfMat = mSM->IStoGivenCamera(fromMag, fromCam);
  
  // If it is same camera and IS can transfer, or if there is no matrix for one of them,
  // just copy the IS
  if ((fromCam == toCam && mSM->CanTransferIS(fromMag, toMag, 
    mCamParams[toCam].STEMcamera, mCamParams[toCam].GIF ? 1 : 0)) || bMat.xpx || 
    !bfMat.xpx) {
    toISX = fromISX;
    toISY = fromISY;
    return;
  }

  // Otherwise compute a matrix to go from IS to camera to specimen and back, i.e.
  // avoid assuming the IS-specimen relationship is same in both cameras
  aProd = MatMul(mSM->IStoGivenCamera(fromMag, fromCam), 
    MatInv(mSM->SpecimenToCamera(fromCam, fromMag)));
  aProd = MatMul(aProd, mSM->SpecimenToCamera(toCam, toMag));
  aProd = MatMul(aProd, MatInv(bMat));
  toISX = aProd.xpx * fromISX + aProd.xpy * fromISY;
  toISY = aProd.ypx * fromISX + aProd.ypy * fromISY;
}

int CShiftCalibrator::GetZeroOffsetMode(void)
{
  if (mZeroOffsetMode < 0)
    mZeroOffsetMode = AfxMessageBox("The image shift offset needs to be set to zero\n"
      "at one mag for either the pre-GIF camera or the GIF camera.\n"
      "The offsets for the other camera will then be incremented\n"
      " by the offset between the two modes.\n\n"
      "Do you want to set the offset to zero for a mag of the preGIF camera?",
      MB_YESNO | MB_ICONQUESTION) == IDYES ? 0 : 1;
  return mZeroOffsetMode;
}

// Adjust the image shifts of the mode that does not have a mag fixed at zero
void CShiftCalibrator::AdjustNonzeroMode(int mode)
{
  int fromCam, fromMag, toCam, toMag, i;
  double delISX, delISY, ISX, ISY;
  if (mode) {

    // To adjust GIF offsets, convert the offset at the regular From mag to a 
    // offset at the GIF To mag, add the calibrated GIF offset, subtract existing offset
    fromCam = mFirstRegularCamera;
    toCam = mFirstGIFCamera;
    fromMag = mGIFofsFromMag;
    toMag = mGIFofsToMag;
    ConvertIS(fromCam, fromMag, mMagTab[fromMag].calOffsetISX[1 - mode], 
      mMagTab[fromMag].calOffsetISY[1 - mode], toCam, toMag, delISX, delISY);
    delISX += mGIFofsISX - mMagTab[toMag].calOffsetISX[mode];
    delISY += mGIFofsISY - mMagTab[toMag].calOffsetISY[mode];
  } else {

    // To adjust nonGIF offsets, convert the offset at the GIF to mag, minus the
    // calibrated GIF offset, to an offset at the regular From mag, subtract existing
    // offset there
    toCam = mFirstRegularCamera;
    fromCam = mFirstGIFCamera;
    fromMag = mGIFofsToMag;
    toMag = mGIFofsFromMag;
    ConvertIS(fromCam, fromMag, mMagTab[fromMag].calOffsetISX[1 - mode] - mGIFofsISX, 
      mMagTab[fromMag].calOffsetISY[1 - mode] - mGIFofsISY, toCam, toMag, delISX, 
      delISY);
    delISX -= mMagTab[toMag].calOffsetISX[mode];
    delISY -= mMagTab[toMag].calOffsetISY[mode];
  }

  for (i = 1; i < MAX_MAGS; i++) {
    if (!mMagTab[i].mag)
      continue;
    ConvertIS(toCam, toMag, delISX, delISY, toCam, i, ISX, ISY);
    mCalISX[i] += ISX;
    mCalISY[i] += ISY;
  }
}

// Tests whether each mag range has a shift calibration for given camera, returns index
// of mag missing a calibration
int CShiftCalibrator::MagRangeWithoutIScal(int camera)
{
  int *boundaries = mScope->GetShiftBoundaries();
  int numBound = mScope->GetNumShiftBoundaries();
  int secondary = mScope->GetLowestSecondaryMag();
  int lowestM = mScope->GetLowestMModeMagInd();
  int mag, i, curMag;
  ScaleMat aMat;
  mLowestISOmag = 0;
  if (mScope->GetUsePLforIS()) {

    // If using projector shift, check that the current mag has a shift cal and find
    // lowest mag in continuous sequence with calibrations
    mLowestISOmag = mMagIndex + 1;
    for (i = mMagIndex; i > 0; i--) {
      if (mMagTab[i].matIS[camera].xpx)
        mLowestISOmag = i;
      else
        break;
    }
    return 0;
  }

  curMag = mScope->GetMagIndex();

  // Otherwise check presence of usable image shift in each range
  // This could be a pretty harsh test if ranges are defined before image shift calibrated
  for (i = -1; i < numBound; i++) {
    if (i < 0)
      mag = boundaries[0] - 1;
    else
      mag = boundaries[i];
    if (curMag <  lowestM && mag >= lowestM)
      break;
    if (secondary && curMag < secondary && mag >= secondary)
      break;
    if (secondary && curMag >= secondary && mag >= lowestM && mag < secondary)
      continue;
    aMat = mSM->IStoGivenCamera(mag, camera);
    if (!aMat.xpx)
      return mag;
  }
  return 0;
}

// Test whether both GIF and nonGIF first cameras have IS cals, complain if not
BOOL CShiftCalibrator::BothModesHaveIScal(void)
{
  int gif, miss, cam;
  CString str;
  for (gif = 0; gif < 2; gif++) {
    cam = gif ? mFirstGIFCamera : mFirstRegularCamera;
    miss = MagRangeWithoutIScal(cam);
    if (miss) {
      str.Format("This calibration should not be done until there is at least one\n"
        "image shift calibration in every mag range, for both of the imaging modes.\n\n"
        "There is no calibration for the mag range including %dx for the %s camera.",
        MagOrEFTEMmag(gif != 0, miss), (LPCTSTR)mCamParams[cam].name);
      AfxMessageBox(str, MB_EXCLAME);
      return false;
    }
  }
  return true;
}

void CShiftCalibrator::SetFirstCameras()
{
  int *activeList = mWinApp->GetActiveCameraList();
  mFirstRegularCamera = mFirstGIFCamera = -1;
  if (mFiltParam->firstRegularCamera >= 0)
    mFirstRegularCamera = activeList[mFiltParam->firstRegularCamera];
  if (mFiltParam->firstGIFCamera >= 0)
    mFirstGIFCamera = activeList[mFiltParam->firstGIFCamera];
}

//////////////////////////////////////////////////////////////
//  STEM interset shift measurement

int CShiftCalibrator::MeasureInterSetShift(int fromTS)
{
  int i;
  if (!fromTS && AfxMessageBox(
    "This procedure measures the shifts between Record and other images.\n"
    "These shifts are used when autofocusing and during a tilt series.\n\n"
    "They are only valid at the mag and binning at which they are taken, and over\n"
    "a limited range of exposure times, so you should run this after the camera\n"
    "parameters are set up and at the mag where a tilt series is going to be done.\n\n"
    "Do you want to proceed?", MB_QUESTION) != IDYES)
    return 0;
  for (i = 0; i < 5; i++)
    mISSshifts->binning[i] = 0;
  mISSmagAllShotsSave = mConSets[FOCUS_CONSET].magAllShots;
  mCamera->SetRequiredRoll(2);
  mISSindex = 0;
  mWinApp->SetStatusText(MEDIUM_PANE, "MEASURING INTERSET SHIFTS");
  mWinApp->UpdateBufferWindows();
  mCamera->SetCancelNextContinuous(true);
  mCamera->InitiateCapture(RECORD_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_INTERSET_SHIFT, 0, 0);
  return 0;
}

void CShiftCalibrator::InterSetShiftNextTask(int param)
{
  int setMap[] = {RECORD_CONSET, PREVIEW_CONSET, TRIAL_CONSET, FOCUS_CONSET};
  int binning, recAliBin, set = setMap[mISSindex];
  int recInd = B3DMIN(2, param);
  int nx, ny, nxzoom, nyzoom, filtType;
  CString errMess = "";
  float focPix, recPix, sXoff, sYoff;
  double scale, fxzoom, fyzoom;
  LowDoseParams *ldParams = mWinApp->GetLowDoseParams();
  KImage *image = mImBufs->mImage;
  short *zoomData = NULL;
  unsigned char **linePtrs = NULL;
  if (mISSindex < 0)
    return;
  if (!image) {
    StopInterSetShift();
    return;
  }

  // save the parameters of this image
  nx = image->getWidth();
  ny = image->getHeight();
  mISSshifts->binning[set] = mImBufs->mBinning;
  mISSshifts->sizeX[set] = nx;
  mISSshifts->sizeY[set] = ny;
  mISSshifts->exposure[set] = mImBufs->mExposure;
  mISSshifts->magInd[set] = mImBufs->mMagInd;
  mISSshifts->shiftX[set] = mISSshifts->shiftY[set] = 0.;

  // For all but Record get the shift
  if (mISSindex > 0) {
    binning = mImBufs->mBinning;
    scale = 1.;
    if (set == FOCUS_CONSET) {

      // Get pixel sizes of the actual images, and find binning of Record that autoalign
      // can apply if record pixel is smaller than focus pixel
      focPix = mSM->GetPixelSize(mImBufs);
      recPix = mSM->GetPixelSize(&mImBufs[recInd]);
      recAliBin = 1;
      while (recPix * recAliBin < focPix)
        recAliBin++;

      // Now Focus needs to be scaled down by ratio of these pixel sizes
      scale = focPix / (recPix * recAliBin);
      if (scale < 0.99) {
        SEMTrace('1', "InterSetShiftNextTask recPix %f  focPix %f  recAliBin %d  scale %f",
          recPix, focPix, recAliBin, scale);
        fxzoom = scale * nx;
        fyzoom = scale * ny;
        nxzoom = 2 * (int)(fxzoom / 2.);
        nyzoom = 2 * (int)(fyzoom / 2.);
        NewArray2(zoomData, short int, nxzoom, nyzoom);
        if (!zoomData)
          errMess = "Failed to get array for zooming down focus image";
        if (errMess.IsEmpty() && selectZoomFilter(3, scale, &filtType))
          errMess = "Error setting up zoomdown filter";
        if (errMess.IsEmpty()) {
          linePtrs = makeLinePointers(image->getData(), nx, ny, 2);
          if (!linePtrs)
            errMess = "Failed to get array for line pointers for filtering";
        }
        image->Lock();
        filtType = image->getType() == kUSHORT ? SLICE_MODE_USHORT : SLICE_MODE_SHORT;
        sXoff = (float)B3DMAX(0., (fxzoom - nxzoom) / 2. - 0.01);
        sYoff = (float)B3DMAX(0., (fyzoom - nyzoom) / 2. - 0.01);
        if (errMess.IsEmpty() && zoomWithFilter(linePtrs, nx, ny, sXoff, sYoff,
          nxzoom, nyzoom, nxzoom, 0, filtType , zoomData, NULL, NULL))
          errMess = "Error zooming down focus image";

        image->UnLock();
        B3DFREE(linePtrs);
        if (!errMess.IsEmpty()) {
          SEMMessageBox(errMess, MB_EXCLAME);
          StopInterSetShift();
          delete [] zoomData;
          return;
        }

        mBufferManager->ReplaceImage((char *)zoomData, image->getType(), nxzoom, nyzoom, 
          0, BUFFER_PROCESSED, mImBufs->mConSetUsed);
        mImBufs->mBinning = recAliBin * mImBufs[recInd].mBinning;
      } else
        scale = 1.;
    }

    if (mSM->AutoAlign(recInd, 1, false)) {
      StopInterSetShift();
      return;
    }

    mISSsetLast = set;
    mISStimeStampLast = mImBufs->mTimeStamp;
    mISSscaleLast = scale;
    mISSbinningLast = binning;
    SaveLastInterSetShift();
  }

  // Increment the index, skip preview if not in low dose
  mISSindex++;
  if (mISSindex == 1 && !mWinApp->LowDoseMode())
    mISSindex++;

  // For trial in low dose, insist that the mag be the same and shift be zero
  if (mISSindex == 2 && mWinApp->LowDoseMode() && 
    (fabs(ldParams[TRIAL_CONSET].axisPosition) > 0.01 || 
    ldParams[TRIAL_CONSET].magIndex != ldParams[RECORD_CONSET].magIndex))
    mISSindex++;

  // For focus in Low dose just insist that axis position be 0
  if (mISSindex == 3 && mWinApp->LowDoseMode() && 
    (fabs(ldParams[FOCUS_CONSET].axisPosition) > 0.01 ||
    ldParams[FOCUS_CONSET].magIndex < ldParams[RECORD_CONSET].magIndex))
    mISSindex++;

  if (mISSindex > 3) {
    StopInterSetShift();
    errMess = "You should now scroll through the images to see if they are aligned"
      " properly\r\n\r\nIf all but the last is aligned, you can shift the last one "
      "(in buffer A)\r\n with the right mouse button and then use Revise/Cancel Shifts to"
      " revise the shift based on the proper alignment\r\n\r\n"
      "If another alignment is no good, you should use Revise/Cancel Shifts\r\n"
      "to disable use of these shifts (answer No to revise the shift of the\r\n"
      " image in A, and then Yes to disable the shifts).";
    AfxMessageBox(errMess, MB_EXCLAME);
    mWinApp->AppendToLog(errMess, LOG_SWALLOW_IF_CLOSED);
    return;
  }

  // If Record has rolled to C copy it up to B, then get next shot
  if (param == 2)
    mBufferManager->CopyImageBuffer(2, 1);
  if (setMap[mISSindex] == FOCUS_CONSET)
    mConSets[FOCUS_CONSET].magAllShots = 1;
  mCamera->SetCancelNextContinuous(true);
  mCamera->InitiateCapture(setMap[mISSindex]);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_INTERSET_SHIFT, param + 1,
    0);
  return;
}

void CShiftCalibrator::SaveLastInterSetShift(void)
{
  CString errMess;
  float shiftX, shiftY;
  KImage *image = mImBufs->mImage;
  image->getShifts(shiftX, shiftY);
  mISSshifts->shiftX[mISSsetLast] = (float)(mISSbinningLast * shiftX / mISSscaleLast);
  mISSshifts->shiftY[mISSsetLast] = (float)(-mISSbinningLast * shiftY / mISSscaleLast);
  errMess.Format("For %s, measured shift %.1f, %.1f in image, %.1f, %.1f unbinned "
    "pixels", (LPCTSTR)mModeNames[mISSsetLast], shiftX, shiftY, mISSshifts->shiftX[mISSsetLast],
    mISSshifts->shiftY[mISSsetLast]);
  mWinApp->AppendToLog(errMess);
}

void CShiftCalibrator::InterSetShiftCleanup(int error)
{
  StopInterSetShift();
  mWinApp->ErrorOccurred(error);
}

void CShiftCalibrator::StopInterSetShift(void)
{
  mISSindex = -1;
  mCamera->SetRequiredRoll(0);
  mConSets[FOCUS_CONSET].magAllShots = mISSmagAllShotsSave;
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
}

// Entry to adjust the shift of last image or zero them all out
void CShiftCalibrator::ReviseOrCancelInterSetShifts(void)
{
  CString str;
  if (mImBufs->mTimeStamp == mISStimeStampLast) {
    str.Format("Do you want to revise the shift for %s images with"
      " the current shift of the image in buffer A?", (LPCTSTR)mModeNames[mISSsetLast]);
    if (AfxMessageBox(str, MB_QUESTION) == IDYES) {
      SaveLastInterSetShift();
      return;
    }
  }
  str.Format("Do you want to disable (zero out) the shift adjustments\n"
    "between the different image types and %s?", (LPCTSTR)mModeNames[RECORD_CONSET]);
  if (AfxMessageBox(str, MB_QUESTION) == IDNO)
    return;
  for (int i = 0; i < 5; i++) {
    mISSshifts->binning[i] = 0;
    mISSshifts->shiftX[i] = 0.;
    mISSshifts->shiftY[i] = 0.;
  }
}

/////////////////////////////////////////////////////////////////////////////////
// CALIBRATE MAG AND ROTATION CHANGE WITH DEFOCUS AND INTENSITY
/////////////////////////////////////////////////////////////////////////////////

// Run the calibration
void CShiftCalibrator::CalibrateHighDefocus(void)
{
  int aaBufInd = mBufferManager->AutoalignBufferIndex();
  EMimageBuffer *refBuf = mImBufs + aaBufInd;
  float underPixels, dummy, underAngle, refPixels, refAngle, initialScale, initialAngle;
  float scale, rotation, refFocus, underFocus, focDiff, startScale, scaleStep;
  float angleRange, shiftX, shiftY, nearFoc;
  double intensity, nearC2dist[2];
  CString mess;
  ScaleMat aMat;
  int ind, scaleNumSteps, scaleNumCuts, underSpot, nearC2Ind[2], numNearC2;
  int underProbe = B3DCHOICE(mImBufs->mProbeMode < 0, mScope->GetProbeMode(), 
    mImBufs->mProbeMode);
  int refProbe = B3DCHOICE(refBuf->mProbeMode < 0, mScope->GetProbeMode(), 
    refBuf->mProbeMode);
  float saveStep = mWinApp->mMultiTSTasks->GetCkAlignStep();
  int saveNumSteps = mWinApp->mMultiTSTasks->GetCkNumAlignSteps();
  int saveStepCuts = mWinApp->mMultiTSTasks->GetCkNumStepCuts();
  bool haveLines = mImBufs->mHasUserLine && refBuf->mHasUserLine;
  char aaText = 'A' + (char)aaBufInd;

  // Give the instructions if not done in this session
  if (!mGaveFocusMagMessage) {
    mess.Format("The procedure for this calibration is:\r\n"
      "1. Take an image within a few microns of 0 defocus (such as a View image with"
      " 0 defocus offset).\r\n"
      "2. Copy it to the Autoalign buffer (%c).\r\n"
      "3. Take an image of the same area with the chosen amount of addition defocus.\r\n"
      "4. If there is not an existing calibration at this defocus, then you need to draw"
      " corresponding lines in each image.\r\n"
      "  a. Find two well-spaced and well-defined points that are visible in both\r\n"
      "images. (Use Insert and Home keys to toggle between the images).\r\n"
      "  b. To draw a line, hold down the Shift key, position the cursor\r\n"
      "     at one point, press the left mouse button,\r\n"
      "     and draw the line to the other point.\r\n"
      "5. But if there is an existing calibration at this defocus, no lines are"
      " needed.\r\n"
      "6. Start this calibration procedure.\r\n"
      "7. When it finishes, it will rotate and shift the image in A into alignment\r\n"
      "and toggle between the two images, then ask you if they appeared aligned.\r\n"
      "8. If the images are not aligned, say No, copy B to A or get a new\r\n"
      "image, and draw new or better lines between corresponding points.", aaText);
    mWinApp->AppendToLog(mess);
    mGaveFocusMagMessage = true;
    if (AfxMessageBox(mess + "\n\nAre you ready to proceed?", MB_QUESTION) == IDNO)
      return;
  }
  if (!mImBufs->mImage || !refBuf->mImage || underProbe != refProbe ||
    mImBufs->mMagInd != refBuf->mMagInd) {
      mess.Format("There needs to be a zero-defocus image in the Autoalign buffer (%c)\n"
        "and an underfocus image at the same magnification in Buffer A", aaText);
      AfxMessageBox(mess, MB_EXCLAME);
      return;
  }
  if (!mImBufs->GetDefocus(underFocus) || !mImBufs->GetSpotSize(underSpot) ||
    !mImBufs->GetIntensity(intensity) || !refBuf->GetDefocus(refFocus)) {
      AfxMessageBox("Images do not have enough focus or beam information to proceed");
      return;
  }
  focDiff = underFocus - refFocus;
  if (focDiff >= 0) {
    mess.Format("The zero-defocus image needs to be in the Autoalign buffer (%c)\n"
      " and the underfocus image needs to be in buffer A", aaText);
    AfxMessageBox(mess, MB_EXCLAME);
    return;
  }
  if (haveLines) {

    // If there are lines, set up scale/rotation and a more restricted search
    mWinApp->mMainView->GetLineLength(mImBufs, underPixels, dummy, underAngle);
    mWinApp->mMainView->GetLineLength(refBuf, refPixels, dummy, refAngle);
    if (underPixels < 100 || refPixels < 100) {
      AfxMessageBox("The lines should be at least 100 pixels long", MB_EXCLAME);
      return;
    }
    initialScale = refPixels / underPixels;
    initialAngle = UtilGoodAngle(refAngle - underAngle);
    PrintfToLog("Lines give initial scale %.3f  rotation %.1f", initialScale, 
      initialAngle);
    scaleStep = 0.01f;
    scaleNumSteps = 7;
    scaleNumCuts = 3;
    angleRange = 6.;
  } else {

    // If there are no lines, get nearby cal if any and set up from that with broader
    // search
    mSM->GetDefocusMagAndRot(underSpot, refProbe, intensity, focDiff, scale,
      rotation, nearFoc, nearC2dist, nearC2Ind, numNearC2);
    if (fabs((double)nearFoc - focDiff) > 10.) {
      AfxMessageBox("There are no calibrations near this defocus;\n"
        "you should draw lines between corresponding points on the two images", 
        MB_EXCLAME);
      return;
    }
    initialScale = 1.f / scale;
    initialAngle = -rotation;
    scaleStep = 0.02f;
    scaleNumSteps = 9;
    scaleNumCuts = 5;
    angleRange = 12.;
  }
  if (initialScale == 1.)
    startScale = 1.;
  else
    startScale = initialScale - (scaleNumSteps / 2.f) * scaleStep;

  // Set up search
  mWinApp->mMultiTSTasks->SetCkAlignStep(scaleStep);
  mWinApp->mMultiTSTasks->SetCkNumStepCuts(scaleNumCuts);
  mWinApp->mMultiTSTasks->SetCkNumAlignSteps(scaleNumSteps);

  // Do scaling then rotation
  mWinApp->mMultiTSTasks->AlignWithScaling(0, false, scale, startScale, initialAngle);

  if (mWinApp->mNavHelper->AlignWithRotation(0, initialAngle, angleRange, rotation, 
    shiftX, shiftY, scale)) {
      mess = CString("The best rotation angle was at the end of the search range\n\n") +
        B3DCHOICE(haveLines, "You need to draw the lines more accurately",
        "You need to draw lines between corresponding points in the images");
      AfxMessageBox(mess, MB_EXCLAME);
      mWinApp->mMultiTSTasks->SetCkAlignStep(saveStep);
      mWinApp->mMultiTSTasks->SetCkNumStepCuts(saveStepCuts);
      mWinApp->mMultiTSTasks->SetCkNumAlignSteps(saveNumSteps);
      return;
  }

  // Then a final scaling search
  scaleStep = 0.008f;
  scaleNumSteps = 5;
  startScale = scale - (scaleNumSteps / 2.f) * scaleStep;
  mWinApp->mMultiTSTasks->SetCkAlignStep(scaleStep);
  mWinApp->mMultiTSTasks->SetCkNumStepCuts(4);
  mWinApp->mMultiTSTasks->SetCkNumAlignSteps(scaleNumSteps);
  mWinApp->mMultiTSTasks->AlignWithScaling(0, false, scale, startScale, rotation);

  // Restore, report, and flash
  mWinApp->mMultiTSTasks->SetCkAlignStep(saveStep);
  mWinApp->mMultiTSTasks->SetCkNumStepCuts(saveStepCuts);
  mWinApp->mMultiTSTasks->SetCkNumAlignSteps(saveNumSteps);
  PrintfToLog("Correlations give final scale %.3f  rotation %.1f", scale, rotation);
  aMat = mSM->StretchCorrectedRotation(mImBufs->mCamera, mImBufs->mMagInd, rotation);
  aMat.xpx *= scale;
  aMat.xpy *= scale;
  aMat.ypx *= scale;
  aMat.ypy *= scale;
  mImBufs->mImage->getShifts(shiftX, shiftY);
  mImBufs->mImage->setShifts(0., 0.);
  for (ind = mBufferManager->GetShiftsOnAcquire(); ind > 0 ; ind--)
    mBufferManager->CopyImBuf(&mImBufs[ind - 1], &mImBufs[ind]);
  mWinApp->mNavHelper->TransformBuffer(mImBufs, aMat, mImBufs->mImage->getWidth(), 
    mImBufs->mImage->getHeight(), 0., aMat);
  mImBufs->mImage->setShifts(shiftX, shiftY);
  mImBufs->mHasUserLine = false;
  mImBufs->mTimeStamp = 0.001 * GetTickCount();
  mImBufs->mCaptured = BUFFER_PROCESSED;
  mWinApp->SetCurrentBuffer(0);
  for (ind = 0; ind < 7; ind++) {
    Sleep(300);
    mWinApp->SetCurrentBuffer(aaBufInd);
    Sleep(300);
    mWinApp->SetCurrentBuffer(0);
  }

  // Store if approved
  mess = CString("Is this calibration good?  Are the images aligned?\n\n"
    "(If not, copy buffer B to A and try drawing ") + B3DCHOICE(haveLines, 
    "the lines better)", "lines between corresponding points on each image)");
  if (AfxMessageBox(mess, MB_QUESTION) == IDYES)
    mSM->AddHighFocusMagCal(underSpot, refProbe, intensity, focDiff, 1. / scale,
      -rotation, 0);
}

// Calibration of image shift at high defocus runs the regular IS cal routine
int CShiftCalibrator::CalibrateISatHighDefocus(bool interactive, float curFocus)
{
  ScaleMat mat;
  int timeSinceLast = mWinApp->MinuteTimeStamp() - mLastISFocusTimeStamp;
  if (mWinApp->LowDoseMode())
    mScope->GotoLowDoseArea(mCamera->ConSetToLDArea(ISCAL_CONSET));

  mCalIndex = mScope->GetMagIndex();
  mCurrentCamera = mWinApp->GetCurrentCamera();
  mat = mSM->IStoCamera(mCalIndex);
  if (!mat.xpx) {
    SEMMessageBox("There is no image shift calibration at the current magnification",
      MB_EXCLAME);
    return 1;
  }

  if (interactive) {
    if (!mWarnedHighFocusNotCal && !mMagTab[mCalIndex].calibrated[mCurrentCamera] &&
      !mMagsTransFrom.count(mCalIndex)) {
      mWarnedHighFocusNotCal = true;
      if (AfxMessageBox("It is recommended that you redo the regular image shift "
        "calibration since it is the reference for the high-focus calibrations\n\n"
        "Do you want to do that first?", MB_QUESTION) == IDYES)
        return 1;
    }

    // If this is not the first time in a while, use the current defocus to put the default 
    // in the entry box
    if (mLastISFocusTimeStamp > 0 && mWinApp->MinuteTimeStamp() - mLastISFocusTimeStamp <
      60)
      mHighDefocusForIS = mScope->GetDefocus() - mLastISZeroFocus;

    if (!KGetOneFloat("You must already be at high defocus and be sure that Trial images "
      "are good for calibration", "What defocus (- for underfocus) has been applied, "
      "in microns?", mHighDefocusForIS, 0))
      return 1;
  } else {
    mHighDefocusForIS = curFocus;
  }

  // Save what is being done for next time
  mLastISZeroFocus = mScope->GetDefocus() - mHighDefocusForIS;
  mLastISFocusTimeStamp = mWinApp->MinuteTimeStamp();

 // Set up matrix and calibrate
  mFirstMat = mSM->FocusAdjustedISToCamera(mCurrentCamera, mCalIndex,
    mScope->GetSpotSize(), mScope->GetProbeMode(), mScope->GetIntensity(), 
    mHighDefocusForIS);

  CalibrateIS(2, false, false);
  return 0;
}

// This is called after successful completion and if user doesn't want to forget it
void CShiftCalibrator::FinishHighFocusIScal()
{
  float rotation, scale, stretch, phi;
  int spot = mScope->GetSpotSize();
  int probe = mScope->GetProbeMode();
  double intensity = mScope->GetIntensity();

  // Get regular IS cal and detremine transformation between them and convert to 
  // rot/mag/stretch
  ScaleMat mat = mSM->IStoCamera(mCalIndex);
  ScaleMat rotMat = MatMul(MatInv(mat), mFirstMat);
  amatToRotmagstr(rotMat.xpx, rotMat.xpy, rotMat.ypx, rotMat.ypy, &rotation, &scale, 
    &stretch, &phi);

  // Report and store
  PrintfToLog("Defocus %.1f  intensity %.2f%s: Scale %.4f  rotation %.2f  stretch %.2f%%", 
    mHighDefocusForIS, mScope->GetC2Percent(spot, intensity, probe),
    mScope->GetC2Units(), scale, rotation, 100. * (stretch - 1.0));
  mSM->AddHighFocusMagCal(spot, probe, intensity, mHighDefocusForIS, scale, rotation, 
    mCalIndex);
}
