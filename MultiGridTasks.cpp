#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "MultiGridTasks.h"
#include "ComplexTasks.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "EMbufferManager.h"
#include "NavHelper.h"
#include "NavigatorDlg.h"
#include "MultiGridDlg.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "Shared\b3dutil.h"
#include "Utilities\XCorr.h"
#include <algorithm>


CMultiGridTasks::CMultiGridTasks()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mReplaceSpaces = true;
  mAllowOptional = false;
  mUseCaretToReplace = false;
  mDoingMultiGrid = 0;
  mRRGcurDirection = -1;
  mRRGangleRange = 15.;
}


CMultiGridTasks::~CMultiGridTasks()
{
}

void CMultiGridTasks::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mShiftManager = mWinApp->mShiftManager;
  mNavHelper = mWinApp->mNavHelper;
  mCartInfo = mScope->GetJeolLoaderInfo();
}

int CMultiGridTasks::ReplaceBadCharacters(CString &str)
{
  const char *mandatory = "`\'\"$?:*\\/";
  const char *optional = "!#%&(){};|";
  const char *charList = mandatory;
  int indCh, loop, retVal = 0;
  char replacement = B3DCHOICE(mReplaceSpaces, mUseCaretToReplace ? '^' : '@', '_');
  for (loop = 0; loop < (mAllowOptional ? 1 : 2); loop++) {
    for (indCh = 0; indCh < (int)strlen(charList); indCh++)
      if (str.Find(charList[indCh]) >= 0)
        retVal += str.Replace(charList[indCh], replacement);
    charList = optional;
  }
  for (indCh = 0; indCh < str.GetLength(); indCh++) {
    if ((unsigned char)(str.GetAt(indCh)) > 127) {
      str.SetAt(indCh, replacement);
      retVal++;
    }
  }
  if (mReplaceSpaces && str.Find(' ') >= 0)
    retVal += str.Replace(' ', '_');
  return retVal;
}

int CMultiGridTasks::GetInventory()
{
  int err, operations = LONG_OP_INVENTORY;
  float hours = 0.;
  err = mScope->StartLongOperation(&operations, &hours, 1);
  if (err) {
    SEMMessageBox("Could not run inventory; " + CString(err == 1 ?
      "the long operation thread is busy" : "program error starting operation"));
    return 1;
  }
  mWinApp->SetStatusText(MEDIUM_PANE, "RUNNING INVENTORY");
  mDoingMultiGrid = MG_INVENTORY;
  if (mNavHelper->mMultiGridDlg)
    mNavHelper->mMultiGridDlg->UpdateEnables();
  mWinApp->AddIdleTask(TASK_MULTI_GRID, MG_INVENTORY, 0);
  return 0;
}

void CMultiGridTasks::MultiGridNextTask(int param)
{
  int err, ind, trimInAlign = 1;
  float xShift, yShift, rotBest, xTiltFac, yTiltFac, dxy[2];
  double xStage, yStage, zStage;
  CString mess;
  ScaleMat bMat, bInv;

  if (!mDoingMultiGrid)
    return;
  switch (param) {
  case MG_RRG_MOVED_STAGE:
    mWinApp->SetStatusText(SIMPLE_PANE, "");

    for (ind = mWinApp->mBufferManager->GetShiftsOnAcquire(); ind > 0; ind--)
      mWinApp->mBufferManager->CopyImageBuffer(ind - 1, ind);

    // Load image and rotate it with current rotation matrix
    if (mWinApp->mBufferManager->ReadFromFile(mRRGstore, mRRGpieceSec[mRRGcurDirection],
      0, true)) {
      StopMultiGrid();
      return;
    }
    if (mNavHelper->RotateForAligning(0, &mRRGrotMat)) {
      StopMultiGrid();
      return;
    }
    mCamera->InitiateCapture(mNavHelper->GetRIstayingInLD() ? 
      mNavHelper->GetRIconSetNum() : TRACK_CONSET);
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_RRG_TOOK_IMAGE, 0);
    break;


  case MG_RRG_TOOK_IMAGE:
    mess = "Error in autoalign routine aligning to map piece";
    if (mRRGcurDirection) {
      mShiftManager->SetNextAutoalignLimit(25.);
      err = mShiftManager->AutoAlign(1, 0, 0, 0, NULL, 0., 0., 0., 0., 0., NULL, NULL,
        trimInAlign);//, &xShift, &yShift);
      mImBufs->mImage->getShifts(xShift, yShift);
    } else {
      err = mNavHelper->AlignWithRotation(1, 0, mRRGangleRange, rotBest, xShift, yShift,
        0., 0, NULL, -1., 0);
      mess += " with rotation";
      if (err == 1)
        mess.Format("Rotation found when aligning to map piece was at the end of the "
          "search range.\r\nThe angular range for the search needs to be higher than %.1f"
          , mRRGangleRange);
      if (!err) 
      err = mShiftManager->AutoAlign(1, 0, 0, 0, NULL, 0., 0., 0., 0., 0., NULL, NULL,
        trimInAlign);//, &xShift, &yShift);
      mImBufs->mImage->getShifts(xShift, yShift);
    }
    if (err) {
      SEMMessageBox(mess);
      StopMultiGrid();
      return;
    }

    // Compute the stage position at center of shifted image.  The shift is amount that
    // image is shifted in inverted coordinates, so invert for RH camera coordinates
    // The rest is the same as in SetAlignShifts for getting stage move
    yShift = -yShift;
    bMat = mShiftManager->FocusAdjustedStageToCamera(mImBufs);
    bInv = MatInv(bMat);
    mShiftManager->GetStageTiltFactors(xTiltFac, yTiltFac);
    mScope->GetStagePosition(xStage, yStage, zStage);
    xStage -= mImBufs->mBinning * (bInv.xpx * xShift + bInv.xpy * yShift) / xTiltFac;
    yStage -= mImBufs->mBinning * (bInv.ypx * xShift + bInv.ypy * yShift) / yTiltFac;
    mRRGnewXstage[mRRGcurDirection] = (float)xStage;
    mRRGnewYstage[mRRGcurDirection] = (float)yStage;
    SEMTrace('1', "dir %d  shift %.1f %.1f   stage diff %.2f %.2f", mRRGcurDirection, 
      xShift, yShift, xStage - mRRGxStages[mRRGcurDirection], 
      yStage - mRRGyStages[mRRGcurDirection]);
    if (GetDebugOutput('1')) {
      /*if (!mRRGcurDirection) {
        mImBufs->mImage->setShifts(xShift, yShift);
        mImBufs->SetImageChanged(1);
      }*/
      mWinApp->mMainView->DrawImage();
      for (int ind = 0; ind < 3; ind++) {
        mWinApp->SetCurrentBuffer(1);
        Sleep(300);
        mWinApp->SetCurrentBuffer(0);
        Sleep(300);
      }
    }

    // For first position, set the post-rotation shift and adjust the expected rotation
    if (!mRRGcurDirection) {
      mRRGshiftX = (float)(xStage - mRRGxStages[0]);
      mRRGshiftY = (float)(yStage - mRRGyStages[0]);
      mRRGexpectedRot -= rotBest;
    }

    // Set up to go to next position unless done
    if (mRRGcurDirection < 4) {
      mRRGcurDirection++;
      if (MoveStageToTargetPiece())
        StopMultiGrid();
      return;
    }

    // Get transformation
    bMat = FindReloadTransform(dxy);
    StopMultiGrid();
    break;
  }
}

int CMultiGridTasks::MultiGridBusy()
{
  if (mDoingMultiGrid == MG_INVENTORY)
    return mScope->LongOperationBusy();
  return 0;
}

void CMultiGridTasks::CleanupMultiGrid(int error)
{
}

void CMultiGridTasks::StopMultiGrid()
{
  if (mDoingMultiGrid == MG_REALIGN_RELOADED) {
    if (mRRGcurStore < 0)
      delete mRRGstore;
    mCamera->SetRequiredRoll(0);
    mNavHelper->RestoreLowDoseConset();
    mNavHelper->RestoreMapOffsets();
    if (mRRGdidSaveState)
      mNavHelper->RestoreFromMapState();
    else if (!mNavHelper->GetRIstayingInLD() && mWinApp->mLowDoseDlg.m_bLowDoseMode ? 1 : 0)
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  }
  if (!mDoingMultiGrid)
    return;
  mDoingMultiGrid = 0;
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mWinApp->UpdateBufferWindows();
}

/*
 * Start procedure to realign after loading grid
 */
int CMultiGridTasks::RealignReloadedGrid(CMapDrawItem *item, float expectedRot, 
  bool moveInZ, CString &errStr)
{
  CNavigatorDlg *nav = mWinApp->mNavigator;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  int numPieces, numFull, ixMin, iyMin, ixMax, iyMax, midX, midY, delX, delY;
  int minInd[4], bestInd[5], pci, dir, indAng, numAngles = 18, bestAngInd, xTarg, yTarg;
  int numClose, ind, corn;
  float useWidth, useHeight, radDist[4], bestDist[4], maxAxis = 0., ang;
  float minDel, cenAngle, delAng, shortAxis, minAng, maxAng, rad, zStage;
  IntVec ixVec, iyVec, pieceSavedAt;
  FloatVec xStage, yStage, meanVec;
  MontParam *montP = &mRRGmontParam;
  float distFrac = 0.75f, meanRatioCrit = 1.5f;

  // Require a montage with autodoc info
  if (!nav) {
    errStr = "The Navigator is not open";
    return 1;
  }
  if (!item || !item->IsMap() || !item->mMapMontage) {
    errStr = "The item to realign to must be a montage map";
    return 1;
  }

  if (nav->AccessMapFile(item, mRRGstore, mRRGcurStore, montP, useWidth, useHeight)) {
    errStr = "Error trying to access map file " + item->mMapFile;
    return 1;
  }
  if (mRRGstore->GetAdocIndex() < 0) {
    errStr = "The map has no autodoc information (in .mdoc, .idoc, or .hdf file)";
    if (mRRGcurStore < 0)
      delete mRRGstore;
    return 1;
  }

  // Get piece coords and stage coords
  numPieces = mWinApp->mMontageController->ListMontagePieces(mRRGstore, montP,
    item->mMapSection, pieceSavedAt, &ixVec, &iyVec, &xStage, &yStage, &meanVec, 
    moveInZ ? &zStage : NULL);
  if (!numPieces || !xStage.size()) {
    errStr = "Error getting the montage piece coordinates or stage coordinates from "
      "autodoc";
    if (mRRGcurStore < 0)
      delete mRRGstore;
    return 1;
  }
  mScope->SetLDCenteredShift(0., 0.);
  nav->BacklashForItem(item, mRRGbacklashX, mRRGbacklashY);
  mRRGmapZvalue = moveInZ ? zStage : EXTRA_NO_VALUE;

  // Find the middle coordinate
  numFull = montP->xNframes * montP->yNframes;
  ixMin = VECTOR_MIN(ixVec);
  ixMax = VECTOR_MAX(ixVec) + montP->xFrame;
  iyMin = VECTOR_MIN(iyVec);
  iyMax = VECTOR_MAX(iyVec) + montP->yFrame;
  midX = (ixMin + ixMax) / 2;
  midY = (iyMin + iyMax) / 2;
  mRRGexpectedRot = expectedRot;

  // Scan angles in 4 directions finding farthest piece intersected by ray
  for (indAng = 0; indAng < numAngles; indAng++) {
    for (dir = 0; dir < 4; dir++) {
      radDist[dir] = 0.;
      minDel = 1.e10f;
      cenAngle = (float)((indAng * 90.) / numAngles + dir * 90.);
      for (pci = 0; pci < numFull; pci++) {
        if (pieceSavedAt[pci] < 0)
          continue;

        // Find angles of the 4 corners and their difference from the ray, get the 
        // min and max differences
        minAng = 999.;
        maxAng = -999.;
        for (corn = 0; corn < 4; corn++) {
          delX = ixVec[pci] + montP->xFrame * (corn % 2) - midX;
          delY = iyVec[pci] + montP->yFrame * (corn / 2) - midY;
          ang = (float)(atan2(delY, delX) / DTOR);
          delAng = (float)UtilGoodAngle(ang - cenAngle);
          ACCUM_MIN(minAng, delAng);
          ACCUM_MAX(maxAng, delAng);
        }

        // If the piece intersects the ray, get distance of its middle and accumulate max
        if (fabs(minAng) < 90. && fabs(maxAng) < 90. && minAng < 0. && maxAng > 0.) {
          delX = ixVec[pci] + montP->xFrame / 2 - midX;
          delY = iyVec[pci] + montP->yFrame / 2 - midY;
          rad = (float)sqrt(delX * delX + delY * delY);
          if (rad > radDist[dir]) {
            minInd[dir] = pci;
            radDist[dir] = rad;
          }
        }
      }
    }

    // Keep track of the direction with the longest short axis
    shortAxis = B3DMIN(radDist[0] + radDist[2], radDist[1] + radDist[3]);
    if (shortAxis > maxAxis) {
      maxAxis = shortAxis;
      bestAngInd = indAng;
      for (dir = 0; dir < 4; dir++) {
        bestInd[dir] = minInd[dir];
        bestDist[dir] = radDist[dir];
      }
    }
  }

  for (dir = 0; dir < 4; dir++) {
    cenAngle = (float)((bestAngInd * 90.) / numAngles + dir * 90.);
    xTarg = (int)(distFrac * bestDist[dir] * cos(DTOR * cenAngle)) + midX;
    yTarg = (int)(distFrac * bestDist[dir] * sin(DTOR * cenAngle)) + midY;

    // Find the 4 closest pieces to the target
    numClose = 0;
    for (pci = 0; pci < numFull; pci++) {
      if (pieceSavedAt[pci] >= 0)
        MaintainClosestFour((float)(ixVec[pci] + montP->xFrame / 2 - xTarg), 
        (float)(iyVec[pci] + montP->yFrame / 2 - yTarg), pci, radDist, minInd, 
          numClose);
    }

    // Pick a different one from closest if it is brighter enough
    bestInd[dir] = minInd[0];
    for (ind = 1; ind < numClose; ind++)
      if (meanVec[minInd[ind]] > meanVec[bestInd[dir]] * meanRatioCrit)
        bestInd[dir] = minInd[ind];
  }

  // Find the pieces closest to 0,0
  numClose = 0;
  for (pci = 0; pci < numFull; pci++) {
    if (pieceSavedAt[pci] >= 0)
      MaintainClosestFour(xStage[pci], yStage[pci], pci, radDist, minInd, numClose);
  }

  // Again pick different one if much brighter
  bestInd[4] = minInd[0];
  for (ind = 1; ind < numClose; ind++)
    if (meanVec[minInd[ind]] > meanVec[bestInd[4]] * meanRatioCrit)
      bestInd[4] = minInd[ind];

  // Save section numbers and stage positions of chosen pieces, center in first one
  for (dir = 0; dir < 5; dir++) {
    ind = (dir + 1) % 5;
    mRRGpieceSec[ind] = pieceSavedAt[bestInd[dir]];
    mRRGxStages[ind] = xStage[bestInd[dir]];
    mRRGyStages[ind] = yStage[bestInd[dir]];
  }

  mRRGdidSaveState = !mWinApp->LowDoseMode();
  if (mRRGdidSaveState) {
    mNavHelper->SetTypeOfSavedState(STATE_NONE);
    mNavHelper->SaveCurrentState(STATE_MAP_ACQUIRE, 0, item->mMapCamera, 0);
  }
  mNavHelper->PrepareToReimageMap(item, montP, conSet, TRIAL_CONSET, (mRRGdidSaveState
    || !(mWinApp->LowDoseMode() && item->mMapLowDoseConSet < 0)) ? 1 : 0, 1);
  if (!mNavHelper->GetRIstayingInLD())
    mNavHelper->SetMapOffsetsIfAny(item);
  mCamera->SetRequiredRoll(1);

  mRRGcurDirection = 0;
  mDoingMultiGrid = MG_REALIGN_RELOADED;
  if (MoveStageToTargetPiece()) {
    StopMultiGrid();
    return 1;
  }
  mWinApp->SetStatusText(MEDIUM_PANE, "REALIGNING RELOADED GRID");
  mWinApp->UpdateBufferWindows();

  return 0;
}

void CMultiGridTasks::MaintainClosestFour(float delX, float delY, int pci, 
  float radDist[4], int minInd[4], int &numClose)
{
  int ind, jnd;
  bool inserted = false;
  float rad = sqrtf(delX * delX + delY * delY);
  for (ind = 0; ind < numClose; ind++) {
    if (rad < radDist[ind]) {
      numClose = B3DMIN(numClose + 1, 4);
      for (jnd = ind + 1; jnd < numClose; jnd++) {
        radDist[jnd] = radDist[jnd - 1];
        minInd[jnd] = minInd[jnd - 1];
      }
      radDist[ind] = rad;
      minInd[ind] = pci;
      inserted = true;
      break;
    }
  }
  if (!inserted && numClose < 4) {
    minInd[numClose] = pci;
    radDist[numClose++] = rad;
  }
}

int CMultiGridTasks::MoveStageToTargetPiece()
{
  StageMoveInfo info;
  float xSign = mShiftManager->GetInvertStageXAxis() ? -1.f : 1.f;
  mRRGrotMat = mNavHelper->GetRotationMatrix(mRRGexpectedRot, false);
  mShiftManager->ApplyScaleMatrix(mRRGrotMat, xSign * mRRGxStages[mRRGcurDirection],
    mRRGyStages[mRRGcurDirection], info.x, info.y);
  info.x *= xSign;
  info.x += mRRGshiftX;
  info.y += mRRGshiftY;
  info.axisBits = axisXY;
  info.backX = mRRGbacklashX;
  info.backY = mRRGbacklashY;
  SEMTrace('1', "dir %d pc pos %.2f %.2f  transformed %.2f %.2f", mRRGcurDirection,
    mRRGxStages[mRRGcurDirection], mRRGyStages[mRRGcurDirection], info.x, info.y);
  if (!mRRGcurDirection && mRRGmapZvalue > EXTRA_VALUE_TEST) {
    info.z = mRRGmapZvalue;
    info.axisBits |= axisZ;
    info.backZ = mWinApp->mComplexTasks->GetFEBacklashZ();
  }
  if (!mScope->MoveStage(info, true))
    return 1;
  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_MULTI_GRID, MG_RRG_MOVED_STAGE, 120000);
  mWinApp->SetStatusText(SIMPLE_PANE, "MOVING STAGE");
  return 0;
}

static float *sOrigXstage, *sOrigYstage, *sNewXstage, *sNewYstage;
static int sSkipInd;

ScaleMat CMultiGridTasks::FindReloadTransform(float dxyOut[2])
{
  ScaleMat aM[6], strMat, usMat, usInv;
  const int MSIZE = 5, XSIZE = 5;
  float x[5][XSIZE], sx[MSIZE], xm[MSIZE], sd[MSIZE];
  float ss[MSIZE][MSIZE], ssd[MSIZE][MSIZE], d[MSIZE][MSIZE], r[MSIZE][MSIZE];
  float dxy[6][2], smagMean, str, sinth, costh, devAvg, devSD, tmp;
  double thetad, alpha, theta, lvoMax, dmaxMin, dsumMin, sdMin;
  double devMax[6], devSum[6], devSumSq[6], devX, devY, devPnt, xx, yy, leaveOut[5];
  int ind, skip, nPairs, i, nCol = 3, ptMax[6];
  int use, ptLvoMax = -1, ptMaxMin = -1, ptSumMin = -1, ptSDmin = -1;
  float threshForStretch = 10.;
  float leaveCrit = 3.f;

  if (fabs(mRRGexpectedRot) < threshForStretch) {

    strMat = mShiftManager->GetStageStretchXform();
    if (strMat.xpx) {
      usMat = mShiftManager->UnderlyingStretch(strMat, smagMean, thetad, str, alpha);
      usInv = MatInv(usMat);
    }

    for (skip = 0; skip < 6; skip++) {

      // Load the data matrix
      nPairs = 0;
      for (ind = 0; ind < 5; ind++) {
        if (ind == skip)
          continue;
        i = nPairs;
        x[nPairs][0] = mRRGxStages[ind];
        x[nPairs][1] = mRRGyStages[ind];
        x[nPairs][3] = mRRGnewXstage[ind];
        x[nPairs++][4] = mRRGnewYstage[ind];
        if (strMat.xpx) {
          mShiftManager->ApplyScaleMatrix(usInv, x[i][0], x[i][1], x[i][0], x[i][1]);
          mShiftManager->ApplyScaleMatrix(usInv, x[i][3], x[i][4], x[i][3], x[i][4]);
        }
      }

      StatCorrel(&x[0][0], XSIZE, 5, MSIZE, &nPairs, sx, &ss[0][0], &ssd[0][0], &d[0][0],
        &r[0][0], xm, sd, &nCol);

      theta = atan2(-(ssd[1][3] - ssd[0][4]), (ssd[0][3] + ssd[1][4]));
      sinth = (float)sin(theta);
      costh = (float)cos(theta);
      aM[skip].xpx = costh;
      aM[skip].xpy = -sinth;
      aM[skip].ypx = sinth;
      aM[skip].ypy = costh;
      dxy[skip][0] = xm[3] - xm[0] * costh + xm[1] * sinth;
      dxy[skip][1] = xm[4] - xm[0] * sinth - xm[1] * costh;

      // Get mean and maximum deviation
      devMax[skip] = -1.;
      devSum[skip] = 0.;
      devSumSq[skip] = 0.;
      for (i = 0; i < nPairs; i++) {
        mShiftManager->ApplyScaleMatrix(aM[skip], x[i][0], x[i][1], xx, yy);
        devX = xx + dxy[skip][0] - x[i][3];
        devY = yy + dxy[skip][1] - x[i][4];
        devPnt = sqrt(devX * devX + devY * devY);
        devSum[skip] += devPnt;
        devSumSq[skip] += devPnt * devPnt;
        if (devMax[skip] < devPnt) {
          devMax[skip] = devPnt;
          ptMax[skip] = i < skip ? i : i + 1;
        }
      }
      //wait
      //devSum[skip] /= nPairs;

      // Get the leave-out error for a skipped point
      if (skip < 5) {
        mShiftManager->ApplyScaleMatrix(aM[skip], mRRGxStages[skip], mRRGyStages[skip], 
          xx, yy);
        devX = xx + dxy[skip][0] - mRRGnewXstage[skip];
        devY = yy + dxy[skip][1] - mRRGnewYstage[skip];
        leaveOut[skip] = sqrt(devX * devX + devY * devY);
      }

      SEMTrace('1', "skip %d dev mean %.2f max %.2f leave-out %.2f", skip,
        devSum[skip] / nPairs, devMax[skip], skip < 5 ? leaveOut[skip] : 0.);
    }

    // Find point with lowest max, lowest mean/sum, and highest leave-out
    dmaxMin = 1.e10;
    dsumMin = 1.e10;
    lvoMax = -1.;
    sdMin = 1.e10;
    for (skip = 0; skip < 5; skip++) {
      if (ptMaxMin < 0 || dmaxMin > devMax[skip]) {
        dmaxMin = devMax[skip];
        ptMaxMin = skip;
      }
      if (ptSumMin < 0 || dsumMin > devSum[skip]) {
        dsumMin = devSum[skip];
        ptSumMin = skip;
      }
      if (ptLvoMax < 0 || lvoMax < leaveOut[skip]) {
        lvoMax = leaveOut[skip];
        ptLvoMax = skip;
      }
      sumsToAvgSDdbl(devSum[skip], devSumSq[skip], 4, 1, &devAvg, &devSD);
      if (ptSDmin < 0 || sdMin > devSD) {
        sdMin = devSD;
        ptSDmin = skip;
      }
    }
    SEMTrace('1', "max min %d  mean min %d  SD min %d leave max %d", ptSumMin, ptMaxMin, 
      ptSDmin, ptLvoMax);

    // They should all match; if so make sure that one has the highest error in
    // the other solution and that its leave-out error is statistically
    // above the deviations of the rest
    use = 5;
    if (ptMaxMin == ptSumMin && ptMaxMin == ptLvoMax && ptMaxMin == ptSDmin) {
      ind = 0;
      for (skip = 0; skip < 5; skip++)
        if (skip != ptLvoMax && ptMax[skip] == ptLvoMax)
          ind++;
      sumsToAvgSDdbl(devSum[ptLvoMax], devSumSq[ptLvoMax], 4, 1, &devAvg, &devSD);
      if (ind == 4 && fabs(lvoMax - devAvg) > leaveCrit * devSD)
        use = ptLvoMax;
      SEMTrace('1', "dev mean %.2f SD %.2f  # SD %.2f  use %d", devAvg, devSD,
        fabs(lvoMax - devAvg) / devSD, use);
    }

    // Adjust for the stretch by multiplying us inverse * matrix * us
    if (strMat.xpx) {
      tmp = dxy[use][0] * usMat.xpx + dxy[use][1] * usMat.xpy;
      dxy[use][1] = dxy[use][0] * usMat.ypx + dxy[use][1] * usMat.ypy;
      dxy[use][0] = tmp;
      aM[use] = MatMul(MatMul(usInv, aM[use]), usMat);
    }
    dxyOut[0] = dxy[use][0];
    dxyOut[1] = dxy[use][1];
  }

  SEMTrace('1', "xform  %.4f %.4f %.4f %.4f %.1f %.1f", aM[use].xpx, aM[use].xpy,
    aM[use].ypx, aM[use].ypy, dxyOut[0], dxyOut[1]);

  return aM[use];
}

static float RRGfunk(float *y, float *err)
{
  int ind;
  float dx, dy;
  float cosRot = (float)cos(y[0] * DTOR);
  float sinRot = (float)sin(y[0] * DTOR);
  *err = 0.;
  for (ind = 0; ind < 5; ind++) {
    if (ind == sSkipInd)
      continue;
    dx = sOrigXstage[ind] * (cosRot + y[1]) - sOrigYstage[ind] * (sinRot - y[2]) + y[3] -
      sNewXstage[ind];
    dy = sOrigXstage[ind] * (sinRot + y[2]) + sOrigYstage[ind] * (cosRot - y[1]) + y[4] -
      sNewYstage[ind];
    *err += dx * dx + dy * dy;
  }
}