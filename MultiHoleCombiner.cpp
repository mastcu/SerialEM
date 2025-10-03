// MultiHoleCombiner.cpp : For combining navigator acquire items into multishot items
//
// Copyright (C) 2020 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include "NavHelper.h"
#include "EMscope.h"
#include "ShiftManager.h"
#include "ParticleTasks.h"
#include "MultiHoleCombiner.h"
#include "MultiCombinerDlg.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"
#include "holefinder.h"
#include "MacroProcessor.h"
#include "LogWindow.h"
#include "MapDrawItem.h"
#include <algorithm>
#include "Shared\b3dutil.h"
#include "Utilities\PathOptimizer.h"

#define XY_IN_GRID(xx, yy) (xx >= 0 && xx < mNxGrid && yy >= 0 && yy < mNyGrid)

// Error reporting
enum {
  ERR_NO_NAV = 1, ERR_NO_IMAGE, ERR_NOT_POLYGON, ERR_NO_GROUP, ERR_NO_CUR_ITEM,
  ERR_TOO_FEW_POINTS, ERR_MEMORY, ERR_CUSTOM_HOLES, ERR_HOLES_NOT_ON, ERR_NOT_DEFINED, 
  ERR_NO_XFORM, ERR_BAD_UNIT_XFORM, ERR_TOO_MANY_HOLES, ERR_TOO_MANY_RINGS, 
  ERR_BAD_GEOMETRY, ERR_LAST_ENTRY
};

const char *sMessages[] = {"Navigator is not open or item array not accessible",
"There is no current image", "The current item is not a polygon",
"The current item is not a point or has no Group ID",
"There is no current item selected in the Navigator table",
"There are too few points to combine",  "Error allocating memory",
"Custom holes are selected in the Multiple Record parameters",
"The Multiple Record parameters do not specify to use multiple holes",
"The Multiple Record parameters do not have a regular hole geometry defined", 
"No transformation from image to stage shift is available", 
"Stage vectors for grid of holes do not correspond well enough to IS vectors for "
"acquisition positions", 
"The number of holes specified to override dialog settings is above the limit",
"The number of hexagonal rings specified to override dialog settings is above the limit",
"The values for number of holes specified to override dialog settings are invalid",
"",
""};

CMultiHoleCombiner::CMultiHoleCombiner(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();

}

CMultiHoleCombiner::~CMultiHoleCombiner(void)
{
  ClearSavedItemArray(true, false);
}

/*
 * Return error string
 */
const char *CMultiHoleCombiner::GetErrorMessage(int error)
{
  error--;
  B3DCLAMP(error, 0, ERR_LAST_ENTRY);
  return sMessages[error];
}

/*
 * Main call to combine items, picked by the specified boundary type. inXholes, inYholes
 * default values are -9, -9, vecItem non-NULL specifies item to take IS vectors from
 */
int CMultiHoleCombiner::CombineItems(int boundType, BOOL turnOffOutside, int inXholes,
  int inYholes, CMapDrawItem *vecItem)
{
  CMapDrawItem *item, *curItem;
  MapItemArray *itemArray;
  MapItemArray tempArray;
  KImage *image;
  FloatVec xCenters, yCenters, peakVals, xCenAlt, yCenAlt, peakAlt, xMissing, yMissing;
  IntVec navInds, altInds, boxAssigns, ixSkip, iySkip;
  std::set<int> worsePoints;
  ShortVec gridX, gridY;
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  MultiShotParams msParamsCopy = *msParams;
  MontParam *montp;
  int *activeList;
  LowDoseParams *ldp;
  ScaleMat s2c, is2cam, st2is, is2st, gridMat, holeInv, holeMat, prodMat, gridStMat;
  PositionData data;
  CArray<PositionData, PositionData> fullArray, bestFullArray;
  float avgAngle, spacing, xfit[3], yfit[3];
  double hx, hy;
  float nearIntCrit = 0.33f, near60crit = 15.f;
  int ind, jnd, vnd, nx, ny, numPoints, ix, iy, groupID, drawnOnID = -1;
  int rowStart, colStart, num, minFullNum = 10000000;
  float fDTOR = (float)DTOR;
  float fullSd, fullBestSd, ptX, ptY, hxHex[3], hyHex[3];
  float minCenDist, cenDist, boxDx, boxDy, cenStageX, cenStageY;
  float stXaxis, stYaxis, diffSign, axisDiff;
  bool crossPattern, leftOK, rightOK, upOK, downOK, handled, useCenStage;
  int *mapVals;
  float gridXvecs[3], gridYvecs[3], restore[6] = {1., 0.5f, 0., sqrtf(3.f) / 2.f, 0., 0.};
  float tripletX[3], tripletY[3];
  int camera = mWinApp->GetCurrentCamera();
  int yStart, xStart, xDir, magInd, registration, divStart, hex, distSq;
  int ring, step, mainDir, sideDir, mainSign, sideSign, xCen, yCen, assignTemp = 1000000;
  int point, acqXstart, acqXend, acqYstart, acqYend, nxBox, nyBox, numInBox, bx, by;
  float stageX, stageY, boxXcen, boxYcen, dx, dy, vecn, vecm, minDist, dist;
  int realXstart, realYstart, realXend, realYend, baseInd, gridInd, extraHexNum, useHex;
  int hexIndXvecs[3] = {1, 0, -1}, hexIndYvecs[3] = {0, 1, 1};
  IntVec tripletsDelX[3];
  IntVec tripletsDelY[3];
  float gridAng1, gridAng2, holeAng1, holeAng2, gridDist1, gridDist2, holeDist1;
  float holeDist2, angDiff1, angDiff2, vecLenDiffCrit = 0.25f;
  int hexPosRot1, hexPosRot2, hexGrid;
  bool posRotInvertDir;
  BOOL skipAveraging;
  PathOptimizer pathOpt;

  int ori, crossDx[5] = {0, -1, 1, 0, 0}, crossDy[5] = {0, 0, 0, -1, 1};
  int numAdded = 0;
  msParams = &msParamsCopy;

  // When runing multi-grid, the map is passed in for the vectors to be taken from
  if (vecItem)
    mHelper->AssignNavItemHoleVectors(vecItem, msParams);

  // Process optional setting of # of holes
  if (inXholes != -9 && inYholes != -9) {
    if (inXholes > 0 && inYholes > 0) {
      if (inXholes > MAX_HOLE_SPINNERS || inYholes > MAX_HOLE_SPINNERS)
        return ERR_TOO_MANY_HOLES;
      msParams->doHexArray = false;
      msParams->skipCornersOf3x3 = false;
      msParams->numHoles[0] = inXholes;
      msParams->numHoles[1] = inYholes;
    } else if (inXholes == -3 && inYholes == -3) {
      msParams->doHexArray = false;
      msParams->skipCornersOf3x3 = true;
      msParams->numHoles[0] = 3;
      msParams->numHoles[1] = 3;
    } else if (inXholes == -1) {
      if (inYholes > MAX_HOLE_SPINNERS)
        return ERR_TOO_MANY_RINGS;
        msParams->doHexArray = true;
      msParams->skipCornersOf3x3 = false;
      msParams->numHexRings = inYholes;
    } else {
      return ERR_BAD_GEOMETRY;
    }
  }

  mDebug = (GetDebugOutput('C') && GetDebugOutput('*')) ? 1 : 0;
  mUseImageCoords = false;
  mGroupIDsInPoly.clear();
  hexGrid = msParams->doHexArray ? 1 : 0;

  // Check for Nav
  mHelper = mWinApp->mNavHelper;
  mFindHoles = mHelper->mFindHoles;
  mNav = mWinApp->mNavigator;
  if (!mNav)
    return ERR_NO_NAV;

  skipAveraging = mHelper->GetMHCskipAveragingPos();
  registration = mNav->GetCurrentRegistration();
  itemArray = mNav->GetItemArray();
  if (!itemArray)
    return ERR_NO_NAV;

  // Make sure a regular multi-hole is defined
  mHelper->UpdateMultishotIfOpen();
  if (!(msParams->inHoleOrMultiHole & MULTI_HOLES))
    return ERR_HOLES_NOT_ON;
  if (msParams->holeMagIndex[hexGrid] <= 0)
    return ERR_NOT_DEFINED;
  if (msParams->useCustomHoles)
    return ERR_CUSTOM_HOLES;
  mNumXholes = msParams->numHoles[0];
  mNumYholes = msParams->numHoles[1];
  crossPattern = mNumXholes == 3 && mNumYholes == 3 && msParams->skipCornersOf3x3;

  mNav->GetCurrentOrGroupItem(curItem);
  if (boundType != COMBINE_ON_IMAGE && boundType >= 0 && !curItem)
    return ERR_NO_CUR_ITEM;

  // Get transformation from IS to stage
  if (mWinApp->LowDoseMode() || mWinApp->GetDummyInstance()) {
    ldp = mWinApp->GetLowDoseParams();
    magInd = ldp[RECORD_CONSET].magIndex;
  } else if (mWinApp->Montaging()) {
    montp = mWinApp->GetMontParam();
    magInd = montp->magIndex;
    activeList = mWinApp->GetActiveCameraList();
    camera = activeList[montp->cameraIndex];
  } else
    magInd = mWinApp->mScope->FastMagIndex();
  s2c = mWinApp->mShiftManager->StageToCamera(camera, magInd);
  is2cam = mWinApp->mShiftManager->IStoGivenCamera(magInd, camera);
  if (!s2c.xpx || !is2cam.xpx)
    return ERR_NO_XFORM;
  st2is = MatMul(s2c, MatInv(is2cam));

  // Negative boundType indicates # of points hole finder just added - test Acquire anyway
  if (boundType < 0) {
    for (ind = (int)itemArray->GetSize() + boundType; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire && item->IsPoint())
        AddItemToCenters(xCenters, yCenters, navInds, item, ind, drawnOnID);
    }
    boundType = COMBINE_ON_IMAGE;
  } else if (boundType == COMBINE_ON_IMAGE) {

    // Find points that are on image
    mImBuf = mWinApp->mActiveView->GetActiveImBuf();
    image = mImBuf->mImage;
    if (!image)
      return ERR_NO_IMAGE;
    item = mNav->FindItemWithMapID(mImBuf->mMapID, true);
    if (item)
      registration = item->mRegistration;
    image->getSize(nx, ny);
    itemArray = mWinApp->mMainView->GetMapItemsForImageCoords(mImBuf, true);
    if (!itemArray)
      return ERR_NO_NAV;
    mUseImageCoords = item && item->mMapMontage;
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire && item->mRegistration == registration) {
        mWinApp->mMainView->GetItemImageCoords(mImBuf, item, ptX, ptY);
        if (ptX >= 0. && ptX <= nx && ptY >= 0. && ptY <= ny) {
          AddItemToCenters(xCenters, yCenters, navInds, item, ind, drawnOnID);
        }
      }
    }

  } else if (boundType == COMBINE_IN_POLYGON) {

    // OR inside polygon
    if (curItem->IsNotPolygon())
      return ERR_NOT_POLYGON;

    registration = curItem->mRegistration;
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire && item->mRegistration == registration) {
        if (item->IsPoint() && InsideContour(curItem->mPtX, curItem->mPtY,
          curItem->mNumPoints, item->mStageX, item->mStageY)) {
          AddItemToCenters(xCenters, yCenters, navInds, item, ind, drawnOnID);
        }
      }
    }
  } else {

    // Get group members
    if (curItem->IsNotPoint() || !curItem->mGroupID)
      return ERR_NO_GROUP;
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire) {
        if (item->IsPoint() && item->mGroupID == curItem->mGroupID) {
          AddItemToCenters(xCenters, yCenters, navInds, item, ind, drawnOnID);
        }
      }
    }
  }

  // Insist on at least 4 points
  numPoints = (int)navInds.size();
  if (numPoints < 4)
    return ERR_TOO_FEW_POINTS;

  // Try to find a buffer with the map if there is a common drawn on ID for poly/group
  if (drawnOnID > 0 && boundType != COMBINE_ON_IMAGE) {
    ind = mNav->FindBufferWithMontMap(drawnOnID);
    if (ind >= 0) {
      mImBuf = mWinApp->GetImBufs() + ind;
      itemArray = mWinApp->mMainView->GetMapItemsForImageCoords(mImBuf, true);
      mUseImageCoords = itemArray != NULL;
    }
  }

  if (mUseImageCoords && !mNav->BufferStageToImage(mImBuf, mBITSmat, mBSTIdelX,
    mBSTIdelY))
    mUseImageCoords = false;

  // If they were drawn on a loaded montage map, now replace the
  // centers with image coordinates
  if (mUseImageCoords) {
    for (ind = 0; ind < numPoints; ind++) {
      item = itemArray->GetAt(navInds[ind]);
      mWinApp->mMainView->GetItemImageCoords(mImBuf, item, ptX, ptY, item->mPieceDrawnOn);
      xCenters[ind] = ptX;
      yCenters[ind] = ptY;
    }

    // We need the image to stage matrix later, so invert
    // We need image to IS transform to replace stage to IS
    mBITSmat = MatInv(mBITSmat);
    st2is = MatMul(mBITSmat, st2is);
  }

  // Get average angle and length with whichever positions
  mFindHoles->analyzeNeighbors(xCenters, yCenters, peakVals, altInds, xCenAlt, yCenAlt,
    peakAlt, 0., 0., 0, spacing, xMissing, yMissing, hexGrid);
  mFindHoles->getGridVectors(gridXvecs, gridYvecs, avgAngle, spacing, hexGrid);
  gridMat.xpx = gridXvecs[0];
  gridMat.ypx = gridYvecs[0];
  gridMat.xpy = gridXvecs[1];
  gridMat.ypy = gridYvecs[1];

  // Solve for best transform from all the vectors for hex
  if (hexGrid) {
    xfit[0] = 1.;
    yfit[0] = 0.;
    xfit[1] = 0.;
    yfit[1] = 1.;
    xfit[2] = -1.;
    yfit[2] = 1.;
    lsFit2(xfit, yfit, gridXvecs, 3, &gridMat.xpx, &gridMat.xpy, NULL);
    lsFit2(xfit, yfit, gridYvecs, 3, &gridMat.ypx, &gridMat.ypy, NULL);
    gridStMat = gridMat;
    if (mUseImageCoords)
      gridStMat = MatMul(gridMat, mBITSmat);

    // Determine if stage to camera has a handedness inversion and set sign for angle diff
    stXaxis = atan2f(s2c.ypx, s2c.xpx) / (float)DTOR;
    stYaxis = atan2f(s2c.ypy, s2c.xpy) / (float)DTOR;
    axisDiff = (float)UtilGoodAngle(stYaxis - stXaxis);
    if (axisDiff > 180.)
      axisDiff -= 360.f;
    if (axisDiff < -180.)
      axisDiff += 360.f;
    diffSign = axisDiff > 0. ? 1.f : -1.f;

    is2st = MatInv(MatMul(s2c, MatInv(is2cam)));
    for (ind = 0; ind < 3; ind++) {
      mWinApp->mShiftManager->TransferGeneralIS(msParams->holeMagIndex[1],
        msParams->hexISXspacing[ind], msParams->hexISYspacing[ind], magInd, hx, hy, 
        camera);
      hxHex[ind] = (float)hx;
      hyHex[ind] = (float)hy;
      SEMTrace('C', "%d: Grid %.3f %.3f  IS %.3f %.3f", ind, gridXvecs[ind],
        gridYvecs[ind], hx, hy);
    }
    SEMTrace('C', "GridStMat %.3f %.3f %.3f %.3f", gridStMat.xpx, gridStMat.ypx,
      gridStMat.xpy, gridStMat.ypy);


    // This gives best estimate of first two vectors in IS space at the given mag
    // multiply by IS to Stage to get these vectors in stage space
    lsFit2(xfit, yfit, hxHex, 3, &holeInv.xpx, &holeInv.xpy, NULL);
    lsFit2(xfit, yfit, hyHex, 3, &holeInv.ypx, &holeInv.ypy, NULL);
    SEMTrace('C', "holeInv %.3f %.3f %.3f %.3f", holeInv.xpx, holeInv.ypx,
        holeInv.xpy, holeInv.ypy);
    holeMat = MatMul(holeInv, is2st);
    gridAng1 = (float)(atan2f(gridStMat.ypx, gridStMat.xpx) / DTOR);
    gridDist1 = sqrtf(gridStMat.xpx * gridStMat.xpx + gridStMat.ypx * gridStMat.ypx);
    gridAng2 = (float)(atan2f(gridStMat.ypy, gridStMat.xpy) / DTOR);
    gridDist2 = sqrtf(gridStMat.xpy * gridStMat.xpy + gridStMat.ypy * gridStMat.ypy);
    holeAng1 = (float)(atan2f(holeMat.ypx, holeMat.xpx) / DTOR);
    holeDist1 = sqrtf(holeMat.xpx * holeMat.xpx + holeMat.ypx * holeMat.ypx);
    holeAng2 = (float)(atan2f(holeMat.ypy, holeMat.xpy) / DTOR);
    holeDist2 = sqrtf(holeMat.xpy * holeMat.xpy + holeMat.ypy * holeMat.ypy);
    angDiff1 = diffSign * (float)UtilGoodAngle(holeAng1 - gridAng1);
    angDiff2 = diffSign * (float)UtilGoodAngle(holeAng2 - gridAng2);
    if (angDiff1 < -30.)
      angDiff1 += 360.;
    if (angDiff1 >= 330.)
      angDiff1 -= 360.;
    if (angDiff2 < -30.)
      angDiff2 += 360.;
    if (angDiff2 >= 330.)
      angDiff2 -= 360.;

    // Determine rotation between rings in the two spaces and check consistency
    hexPosRot1 = B3DNINT(angDiff1 / 60.);
    hexPosRot2 = B3DNINT(angDiff2 / 60.);
    if (fabs(holeDist1 / gridDist1 - 1.) > vecLenDiffCrit ||
      fabs(holeDist2 / gridDist2 - 1.) > vecLenDiffCrit ||
      fabs(hexPosRot1 * 60. - angDiff1) > near60crit || 
      fabs(hexPosRot2 * 60. - angDiff2) > near60crit || 
      (hexPosRot1 != hexPosRot2 && (hexPosRot2 + 2) % 6 != hexPosRot1 && 
      (hexPosRot2 + 4) % 6 != hexPosRot1)) {
      PrintfToLog("%s\r\nThis would not end well...  Please report these details:\r\n"
        "  Stage vectors from actual positions: %.2f um %.1f deg   %.2f um %.1f deg\r\n"
        "  Stage vectors from IS vectors: %.2f um %.1f deg   %.2f um %.1f deg\r\n"
        " Rotations between corresponding vectors: %.1f  %.1f deg",
        sMessages[ERR_BAD_UNIT_XFORM - 1], gridDist1, gridAng1, gridDist2, gridAng2,
        holeDist1, holeAng1, holeDist2, holeAng2, angDiff1, angDiff2);
      return ERR_BAD_UNIT_XFORM;
    }

    posRotInvertDir = hexPosRot2 != hexPosRot1;
    avgAngle = (float)(atan2f(gridMat.ypx, gridMat.xpx) / DTOR);
    SEMTrace('C', "  Stage vectors from actual positions: %.2f um %.1f deg   %.2f um %.1f"
      " deg\r\n  Stage vectors from IS vectors: %.2f um %.1f deg   %.2f um %.1f deg\r\n"
      " Rotations between corresponding vectors: %.1f  %.1f deg",
      gridDist1, gridAng1, gridDist2, gridAng2,
      holeDist1, holeAng1, holeDist2, holeAng2, angDiff1, angDiff2);
    SEMTrace('C', "posRotInvert %d  avgAngle %f posrot1 %d posrot2 %d", posRotInvertDir,
      avgAngle, hexPosRot1, hexPosRot2);

  } else {

    // Get a matrix that would transform from relative hole positions in unit vector stage
    // space to relative hole positions in unit vector image shift space
    // gridMat takes unit vectors in stage or image hole space to stage or image positions
    // st2is takes those positions to image shift positions
    // Transfer the hole vectors from where they were defined to the mag in question
    // Compute a transformation from IS space hole number to IS and take its inverse
    mWinApp->mShiftManager->TransferGeneralIS(msParams->holeMagIndex[0],
      msParams->holeISXspacing[0], msParams->holeISYspacing[0], magInd, hx, hy, camera);
    holeInv.xpx = (float)hx;
    holeInv.ypx = (float)hy;
    mWinApp->mShiftManager->TransferGeneralIS(msParams->holeMagIndex[0],
      msParams->holeISXspacing[1], msParams->holeISYspacing[1], magInd, hx, hy, camera);
    holeInv.xpy = (float)hx;
    holeInv.ypy = (float)hy;
    holeMat = MatInv(holeInv);
    prodMat = MatMul(MatMul(gridMat, st2is), holeMat);

    mSkipXform.xpx = (float)B3DNINT(prodMat.xpx);
    mSkipXform.xpy = (float)B3DNINT(prodMat.xpy);
    mSkipXform.ypx = (float)B3DNINT(prodMat.ypx);
    mSkipXform.ypy = (float)B3DNINT(prodMat.ypy);
    if (fabs(mSkipXform.xpx) > 1.01 || fabs(mSkipXform.xpy) > 1.01 ||
      fabs(mSkipXform.ypx) > 1.01 || fabs(mSkipXform.ypy) > 1.01 ||
      fabs(mSkipXform.xpx - prodMat.xpx) > nearIntCrit ||
      fabs(mSkipXform.xpy - prodMat.xpy) > nearIntCrit ||
      fabs(mSkipXform.ypx - prodMat.ypx) > nearIntCrit ||
      fabs(mSkipXform.ypy - prodMat.ypy) > nearIntCrit ||
      fabs(mSkipXform.xpx * mSkipXform.ypx + mSkipXform.xpy * mSkipXform.ypy) > 0.001) {
      PrintfToLog("%s\r\nThis would not end well...  Please report these details:\r\n"
        " Matrix to xform relative hole positions from %s to IS space: %.3f %.3f %.3f "
        "%.3f\r\n gridMat:  %.3f %.3f %.3f %.3f\r\n %s to IS:  %f  %f  %f  %f\r\n"
        "holeMat: %.3f %.3f %.3f %.3f", sMessages[ERR_BAD_UNIT_XFORM - 1],
        mUseImageCoords ? "image" : "stage",
        prodMat.xpx, prodMat.xpy, prodMat.ypx, prodMat.ypy,
        gridMat.xpx, gridMat.xpy, gridMat.ypx, gridMat.ypy,
        mUseImageCoords ? "image" : "stage", st2is.xpx, st2is.xpy, st2is.ypx, st2is.ypy,
        holeMat.xpx, holeMat.xpy, holeMat.ypx, holeMat.ypy);
      return ERR_BAD_UNIT_XFORM;
    }
    SEMTrace('C',
        " Matrix to xform relative hole positions from %s to IS space: %.3f %.3f %.3f "
        "%.3f\r\n gridMat:  %.3f %.3f %.3f %.3f\r\n %s to IS:  %f  %f  %f  %f\r\n"
        "holeMat: %.3f %.3f %.3f %.3f", mUseImageCoords ? "image" : "stage",
        prodMat.xpx, prodMat.xpy, prodMat.ypx, prodMat.ypy,
        gridMat.xpx, gridMat.xpy, gridMat.ypx, gridMat.ypy,
        mUseImageCoords ? "image" : "stage", st2is.xpx, st2is.xpy, st2is.ypx, st2is.ypy,
        holeMat.xpx, holeMat.xpy, holeMat.ypx, holeMat.ypy);
  }

  // Now get grid positions
  mFindHoles->assignGridPositions(xCenters, yCenters, gridX, gridY, avgAngle, spacing, 
    hexGrid, &worsePoints);
  if (mDebug)
    mWinApp->mMacroProcessor->SetNonMacroDeferLog(true);
  /*for (ind = 0; ind < numPoints; ind++) {
    item = itemArray->GetAt(navInds[ind]);
    PrintfToLog("%s  %d  %d %.2f %.2f", item->mLabel, gridX[ind], gridY[ind], 
    xCenters[ind], yCenters[ind]);
  }*/

 // If the xpx term is 0, this means thare is a 90 degree rotation involved
  // In this case, the number of holes in stage coordinates is transposed from
  // the specified geometry.  Holes will be analyzed in stage coordinates, but the hole
  // geometry has to be back in image shift space when making Nav items
  mTransposeSize = !hexGrid && fabs(mSkipXform.xpx) < 0.1;
  if (mTransposeSize)
    B3DSWAP(mNumXholes, mNumYholes, ix);

  // Set up a 2D map array
  mNxGrid = *std::max_element(gridX.begin(), gridX.end()) + 1;
  mNyGrid = *std::max_element(gridY.begin(), gridY.end()) + 1;
  mapVals = B3DMALLOC(int, mNxGrid * mNyGrid);
  if (!mapVals)
    return ERR_MEMORY;
  mGrid = (int **)makeLinePointers(mapVals, mNxGrid, mNyGrid, sizeof(int));
  if (!mGrid) {
    free(mapVals);
    return ERR_MEMORY;
  }

  // Fill the map with -1 then the indexes of the points at each location
  for (ind = 0; ind < mNxGrid * mNyGrid; ind++)
    mapVals[ind] = -1;
  for (ind = 0; ind < numPoints; ind++)
    if (!worsePoints.count(ind))
      mGrid[gridY[ind]][gridX[ind]] = ind;

  if (mDebug) {
    float xperx = 0., xpery = 0., yperx = 0., ypery = 0.;
    int numInY = 0, numInX = 0;
    for (ix = 0; ix < mNxGrid - 1;ix++) {
      for (iy = 0; iy < mNyGrid - 1; iy++) {
        if (mGrid[iy][ix] >= 0 && mGrid[iy + 1][ix] >= 0) {
          xpery += xCenters[mGrid[iy + 1][ix]] - xCenters[mGrid[iy][ix]];
          ypery += yCenters[mGrid[iy + 1][ix]] - yCenters[mGrid[iy][ix]];
          numInY++;
        }
        if (mGrid[iy][ix] >= 0 && mGrid[iy][ix + 1] >= 0) {
          xperx += xCenters[mGrid[iy][ix + 1]] - xCenters[mGrid[iy][ix]];
          yperx += yCenters[mGrid[iy][ix + 1]] - yCenters[mGrid[iy][ix]];
          numInX++;
        }
      }
    }
    PrintfToLog("Grid step in X: %.2f %.2f  in Y: %.2f %.2f", xperx / numInX, 
      yperx / numInX, xpery / numInY, ypery / numInY);
  }

  boxAssigns.resize(numPoints, -1);
  ClearSavedItemArray(false, true);
  mIDsForUndo.clear();
  mSetOfUndoIDs.clear();
  mIndexesForUndo.clear();
  mIDsOutsidePoly.clear();
  mOrphanIDsToUndo.clear();

  if (hexGrid) {

    // HEXAGONAL ARRAYS
    mNumRings = msParams->numHexRings;

    // Fill array of indexes to valid positions in hexagon.  Go from center outward
    mHexDelX.clear();
    mHexDelY.clear();
    mHexDelX.push_back(0);
    mHexDelY.push_back(0);
    for (ring = 1; ring < 2 * mNumRings + 2; ring++) {
      ind = 0;
      for (iy = 0; iy < 2 * mNumRings + 1; iy++) {
        for (ix = 0; ix < 2 * mNumRings + 1; ix++) {
          bx = ix - mNumRings;
          by = iy - mNumRings;
          distSq = bx * bx + by *by;
          if (distSq > (ring - 1) * (ring - 1) && distSq <= ring * ring) {
            ind = 1;
            if (!((iy < mNumRings && ix < mNumRings - iy) ||
              (iy > mNumRings && ix > 3 * mNumRings - iy))) {
              mHexDelX.push_back(bx);
              mHexDelY.push_back(by);
            }
          }
        }
      }
      if (!ind)
        break;
    }

    // Fill lookup arrays to get from lattice indexes back to position in rings
    mOneDIndexToHexRing.resize((2 * mNumRings + 1) * (2 * mNumRings + 1), -1);
    mOneDIndexToPosInRing.resize((2 * mNumRings + 1) * (2 * mNumRings + 1), -1);
    mOneDIndexToHexRing[mNumRings + mNumRings * (2 * mNumRings + 1)] = 0;
    mOneDIndexToPosInRing[mNumRings + mNumRings * (2 * mNumRings + 1)] = 0;
    for (ring = 1; ring <= mNumRings; ring++) {
      for (step = 0; step < 6; step++) {
        mWinApp->mParticleTasks->DirectionIndexesForHexSide(step, mainDir, mainSign, 
          sideDir, sideSign);
        for (ind = 0; ind < ring; ind++) {
          ix = ring * hexIndXvecs[mainDir] * mainSign + ind * hexIndXvecs[sideDir] *
            sideSign;
          iy = ring * hexIndYvecs[mainDir] * mainSign + ind * hexIndYvecs[sideDir] *
            sideSign;
          point = mNumRings + ix + (mNumRings + iy) * (2 * mNumRings + 1);
          mOneDIndexToHexRing[point] = ring;

          // Might as well take the rotation into account here!
          if (posRotInvertDir) {
            if (ind)
              mOneDIndexToPosInRing[point] = ((11 + hexPosRot1 - step) % 6) * ring + 
              ring - ind;
            else
              mOneDIndexToPosInRing[point] = ((12 + hexPosRot1 - step) % 6) * ring;

          } else {
            mOneDIndexToPosInRing[point] = ((step + hexPosRot1) % 6) * ring + ind;
          }
          SEMTrace('C', "Ring %d  step %d  ind %d  ixy %d %d  point %d  index %d", ring,
              step, ind, ix, iy, point, mOneDIndexToPosInRing[point]);
          if (ring > 1) {
            tripletsDelX[step / 2].push_back(ix);
            tripletsDelY[step / 2].push_back(iy);
          }
        }
      }
    }

    // Try hexes centered so that the center point is at all possible positions, and
    // try rows pitched above and below X axis
    int bestInd = -1, bestIx;
    for (ind = 0; ind < (int)mHexDelX.size(); ind++) {
      for (ix = 0; ix < 2; ix++) {
        //ind = 0;
        //ix = 0;
        fullArray.RemoveAll();
        num = EvaluateArrangementOfHexes(mNxGrid / 2 - mHexDelX[ind],
          mNyGrid / 2 - mHexDelY[ind], ix > 0, fullArray, fullSd, iy);
        //PrintfToLog("ind %d ix %d num %d  sd %.2f", ind, ix, num, fullSd);

        // Missing centers have either been handled by shifting or counted already as
        // being split into two parts, so go for the smallest # of acquires and the 
        // least variability when there is a tie
        if (num < minFullNum || (num == minFullNum && fullSd < fullBestSd)) {
          minFullNum = num;
          fullBestSd = fullSd;
          bestFullArray.Copy(fullArray);
          bestInd = ind;
          bestIx = ix;
        }
      }
    }
    // PrintfToLog("Best ind %d ix %d", bestInd, bestIx);
    mHexToItemIndex.clear();
    mHexToItemIndex.resize(bestFullArray.GetSize(), -1);
    extraHexNum = (int)bestFullArray.GetSize();

    // Loop on boxes in three rounds: center at original pos, center moved, and missing
    for (ind = 0; ind >= -1; ind--) {
      if (mDebug)
        PrintfToLog("Adding hex with %s", ind ? "cen moved" : "original cen");
      for (hex = 0; hex < bestFullArray.GetSize(); hex++) {
        if (bestFullArray[hex].cenMissing == ind)
          AddPointsToHexItem(itemArray, bestFullArray[hex], hex, boxAssigns, navInds,
            xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);
      }
    }

    for (hex = 0; hex < bestFullArray.GetSize(); hex++) {
      if (bestFullArray[hex].cenMissing <= 0)
        continue;
      xCen = bestFullArray[hex].xCen;
      yCen = bestFullArray[hex].yCen;

      // First see if it can shift or is empty now that there are assignments
      EvaluateHexAtPosition(xCen, yCen, data, &boxAssigns);
      if (!data.numAcquires)
        continue;
      if (data.cenMissing <= 0) {
        bestFullArray[hex] = data;
        AddPointsToHexItem(itemArray, bestFullArray[hex], hex, boxAssigns, navInds,
          xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);
        continue;
      }

      // Split boxes with center missing
      // First test opposite sides around center to see if can split in two
      handled = false;
      for (ind = 0; ind < 3; ind++) {
        ix = xCen + hexIndXvecs[ind];
        iy = yCen + hexIndYvecs[ind];
        if (XY_IN_GRID(ix, iy) && mGrid[iy][ix] >= 0) {
          ix = xCen - hexIndXvecs[ind];
          iy = yCen - hexIndYvecs[ind];
          if (XY_IN_GRID(ix, iy) && mGrid[iy][ix] >= 0) {

            // Can split in two
            // Set temporary assigns
            for (jnd = 0; jnd < (int)mHexDelX.size(); jnd++) {

              // Take cross product with 90-deg rotated line.  The current ix, iy is on
              // the negative side of this rotated line so preserve points that are 
              // on positive side
              if (hexIndYvecs[ind] * mHexDelY[jnd] + hexIndXvecs[ind] * mHexDelX[jnd] >
                0) {
                bx = xCen + mHexDelX[jnd];
                by = yCen + mHexDelY[jnd];
                if (XY_IN_GRID(bx, by) &&
                  mGrid[by][bx] >= 0 && boxAssigns[mGrid[by][bx]] < 0)
                  boxAssigns[mGrid[by][bx]] = assignTemp;
              }
            }
            data.xCen = ix;
            data.yCen = iy;
            AddPointsToHexItem(itemArray, data, hex, boxAssigns, navInds,
              xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);

            // Restore  assigns to -1
            for (jnd = 0; jnd < (int)mHexDelX.size(); jnd++) {
              bx = xCen + mHexDelX[jnd];
              by = yCen + mHexDelY[jnd];
              if (XY_IN_GRID(bx, by) &&
                mGrid[by][bx] >= 0 && boxAssigns[mGrid[by][bx]] == assignTemp)
                boxAssigns[mGrid[by][bx]] = -1;
            }

            // Add around the other center
            data.xCen = xCen + hexIndXvecs[ind];
            data.yCen = yCen + hexIndYvecs[ind];
            mHexToItemIndex.resize(extraHexNum + 1, -1);
            AddPointsToHexItem(itemArray, data, extraHexNum++, boxAssigns, navInds,
              xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);
            handled = true;
            break;
          }
        }
      }
      if (handled)
        continue;

      // Now see if there are any triplets all present that can be centers
      for (ind = 0; ind < (int)tripletsDelX[0].size(); ind++) {
        bx = 0;
        for (vnd = 0; vnd < 3; vnd++) {
          ix = xCen + tripletsDelX[vnd][ind];
          iy = yCen + tripletsDelY[vnd][ind];
          if (XY_IN_GRID(ix, iy) && mGrid[iy][ix] >= 0 && boxAssigns[mGrid[iy][ix]] < 0)
            bx++;
          else
            break;
        }
        if (bx == 3) {

          // Can split in 3
          // Divide into groups by whichever is closest in undistorted space
          for (vnd = 0; vnd < 3; vnd++)
            xfApply(restore, 0., 0., (float)tripletsDelX[vnd][ind], 
            (float)tripletsDelY[vnd][ind], &tripletX[vnd], &tripletY[vnd], 2);

          for (jnd = 0; jnd < (int)mHexDelX.size(); jnd++) {
            ix = xCen + mHexDelX[jnd];
            iy = yCen + mHexDelY[jnd];
            if (XY_IN_GRID(ix, iy) && mGrid[iy][ix] >= 0 && 
              boxAssigns[mGrid[iy][ix]] < 0) {
              minDist = 1.e10f;
              bx = 0;
              xfApply(restore, 0., 0., (float)mHexDelX[jnd], (float)mHexDelY[jnd], &boxDx,
                &boxDy, 2);
              for (vnd = 0; vnd < 3; vnd++) {
                dist = (boxDx - tripletX[vnd]) * (boxDx - tripletX[vnd]) +
                  (boxDy - tripletY[vnd]) * (boxDy - tripletY[vnd]);
                if (dist < minDist) {
                  minDist = dist;
                  bx = vnd;
                }
              }
              boxAssigns[mGrid[iy][ix]] = assignTemp + bx;
            }
          }

          // loop on the three centers and their groups
          for (vnd = 0; vnd < vnd; bx++) {

            // Restore this group's assigns
            for (jnd = 0; jnd < (int)mHexDelX.size(); jnd++) {
              bx = xCen + mHexDelX[jnd];
              by = yCen + mHexDelY[jnd];
              if (XY_IN_GRID(bx, by) &&
                mGrid[by][bx] >= 0 && boxAssigns[mGrid[by][bx]] == assignTemp + vnd)
                boxAssigns[mGrid[by][bx]] = -1;
            }
            data.xCen = xCen + tripletsDelX[vnd][ind];
            data.yCen = yCen + tripletsDelY[vnd][ind];
            useHex = hex;
            if (vnd) {
              useHex = extraHexNum++;
              mHexToItemIndex.resize(extraHexNum, -1);
            }
            AddPointsToHexItem(itemArray, data, useHex, boxAssigns, navInds,
              xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);
          }

          handled = true;
          break;
        }
      }
      if (handled)
        continue;

      // Stuck.  Start to dispose of points however we can: find first available
      // center and add points around it, loop through all points in hex
      bx = 0;
      for (jnd = 0; jnd < (int)mHexDelX.size(); jnd++) {
        ix = xCen + mHexDelX[jnd];
        iy = yCen + mHexDelY[jnd];
        if (XY_IN_GRID(ix, iy) && mGrid[iy][ix] >= 0 && boxAssigns[mGrid[iy][ix]] < 0) {
          data.xCen = ix;
          data.yCen = iy;
          useHex = hex;
          if (bx) {
            useHex = extraHexNum++;
            mHexToItemIndex.resize(extraHexNum, -1);
          }
          AddPointsToHexItem(itemArray, data, useHex, boxAssigns, navInds,
            xCenters, yCenters, gridMat, ixSkip, iySkip, numAdded);
          bx = 1;
        }
      }
    }

    // Loop on points and transfer items to temp array in order of point acquisition
    for (ind = 0; ind < numPoints; ind++) {
      hex = boxAssigns[ind];
      if (hex >= 0 && hex < (int)mHexToItemIndex.size()) {
        jnd = mHexToItemIndex[hex];
        if (jnd > 0 && itemArray->GetAt(jnd)) {
          tempArray.Add(itemArray->GetAt(jnd));
          itemArray->SetAt(jnd, NULL);
        }
      }
    }

    // Are there any left?  Transfer those
    jnd = (int)itemArray->GetSize();
    for (ind = jnd - numAdded; ind < jnd; ind++) {
      if (itemArray->GetAt(ind)) {
        PrintfToLog("Program error: item %s left over after repacking in order by points",
          (LPCTSTR)itemArray->GetAt(ind)->mLabel);
        tempArray.Add(itemArray->GetAt(ind));
      }
    }

    // Now copy to real array
    for (ind = 0; ind < (int)tempArray.GetSize(); ind++)
      itemArray->SetAt(ind + jnd - numAdded, tempArray[ind]);


  } else if (!crossPattern) {

    // NORMAL RECTANGULAR PATTERNS
    //
    // Set up boxes in rows first.  Loop on series of row starts in Y
    for (rowStart = 1 - mNumYholes; rowStart <= 0; rowStart++) {
      fullArray.RemoveAll();

      // At each, loop on the possible Y starts to sample the area
      for (yStart = rowStart; yStart < mNyGrid; yStart += mNumYholes) {
        TryBoxStartsOnLine(yStart, false, fullArray);
      }

      // Try to merge and get the Sd of # in each; keep track of best one
      mergeBoxesAndEvaluate(fullArray, fullSd);
      num = (int)fullArray.GetSize();
      if (num < minFullNum || (num == minFullNum && fullSd < fullBestSd)) {
        minFullNum = num;
        fullBestSd = fullSd;
        bestFullArray.Copy(fullArray);
        if (mDebug)
          PrintfToLog("Best at rowStart %d  num %d  fullSd %.2f", rowStart, num, fullSd);
      }
    }

    // Now repeat that in columns
    for (colStart = 1 - mNumXholes; colStart <= 0; colStart++) {
      fullArray.RemoveAll();

      // At each, loop on the possible X starts to sample the area
      for (xStart = colStart; xStart < mNxGrid; xStart += mNumXholes) {
        TryBoxStartsOnLine(xStart, true, fullArray);
      }
      mergeBoxesAndEvaluate(fullArray, fullSd);
      num = (int)fullArray.GetSize();
      if (num < minFullNum || (num == minFullNum && fullSd < fullBestSd)) {
        minFullNum = num;
        fullBestSd = fullSd;
        bestFullArray.Copy(fullArray);
        if (mDebug)
          PrintfToLog("Best at colStart %d  num %d  fullSd %.2f", colStart, num, fullSd);
      }
    }

    // Big loop on points
    for (point = 0; point < numPoints; point++) {
      if (boxAssigns[point] >= 0)
        continue;

      // Find the box it is in
      ix = gridX[point];
      iy = gridY[point];
      for (ind = 0; ind < (int)bestFullArray.GetSize(); ind++) {
        if (ix >= bestFullArray[ind].startX && ix <= bestFullArray[ind].endX &&
          iy >= bestFullArray[ind].startY && iy <= bestFullArray[ind].endY) {

          // Expand the box if necessary to match even/odd of # of holes
          SetBoxAcquireLimits(bestFullArray[ind].startX, bestFullArray[ind].endX,
            mNumXholes, mNxGrid, acqXstart, acqXend);
          SetBoxAcquireLimits(bestFullArray[ind].startY, bestFullArray[ind].endY,
            mNumYholes, mNyGrid, acqYstart, acqYend);

          // Check if the center is missing, and try to either shift or split in two
          if ((mNumXholes * mNumYholes) % 2) {
            ix = (acqXstart + acqXend) / 2;
            iy = (acqYstart + acqYend) / 2;
            if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid && mGrid[iy][ix] < 0) {

              // check if point on each side could be center of a box
              leftOK = ix > 0 && mGrid[iy][ix - 1] >= 0 && ix >=bestFullArray[ind].startX;
              rightOK = ix < mNxGrid - 1 && mGrid[iy][ix + 1] >= 0 && 
                ix < bestFullArray[ind].endX;
              downOK = iy > 0 && mGrid[iy - 1][ix] >= 0 && iy > bestFullArray[ind].startY;
              upOK = iy < mNyGrid - 1 && mGrid[iy + 1][ix] >= 0 && 
                iy < bestFullArray[ind].endY;

              // shift if it is empty on side opposite a good point
              if (leftOK && acqXend > bestFullArray[ind].endX) {
                acqXend--;
                acqXstart--;
                if (mDebug)
                  mWinApp->AppendToLog("Shift left");
              } else if (rightOK && acqXstart < bestFullArray[ind].startX) {
                acqXend++;
                acqXstart++;
                if (mDebug)
                  mWinApp->AppendToLog("Shift right");
              } else if (downOK && acqYend > bestFullArray[ind].endY) {
                acqYend--;
                acqYstart--;
                if (mDebug)
                  mWinApp->AppendToLog("Shift down");
              } else if (upOK && acqYstart < bestFullArray[ind].startY) {
                acqYend++;
                acqYstart++;
                if (mDebug)
                  mWinApp->AppendToLog("Shift up");

                // Or split if there is good center on each side
              } else if (leftOK && rightOK) {

                // Set the dividing point to favor minority side and copy position
                divStart = ix;
                if (ix - bestFullArray[ind].startX < bestFullArray[ind].endX - ix)
                  divStart = ix + 1;
                data = bestFullArray[ind];

                // If the point that started this all is to the left of the divider,
                // make new data starting at divider; otherwise make new data to left
                if (gridX[point] < divStart) {
                  data.startX = divStart;
                  bestFullArray[ind].endX = divStart - 1;
                  num = (bestFullArray[ind].endX + 1 - bestFullArray[ind].startX) / 2;
                  acqXstart = (ix - 1) - num;
                  acqXend = (ix - 1) + num;
                  if (mDebug)
                    mWinApp->AppendToLog("Split, new right");
                } else {
                  bestFullArray[ind].startX = divStart;
                  data.endX = divStart - 1;
                  num = (bestFullArray[ind].endX + 1 - bestFullArray[ind].startX) / 2;
                  acqXstart = (ix + 1) - num;
                  acqXend = (ix + 1) + num;
                  if (mDebug)
                    mWinApp->AppendToLog("Split, new left");
                }
                bestFullArray.Add(data);
              } else if (downOK && upOK) {

                // Do the same in Y
                divStart = iy;
                if (iy - bestFullArray[ind].startY < bestFullArray[ind].endY - iy)
                  divStart = iy + 1;
                data = bestFullArray[ind];
                if (gridY[point] < divStart) {
                  data.startY = divStart;
                  bestFullArray[ind].endY = divStart - 1;
                  num = (bestFullArray[ind].endY + 1 - bestFullArray[ind].startY) / 2;
                  acqYstart = (iy - 1) - num;
                  acqYend = (iy - 1) + num;
                  if (mDebug)
                      mWinApp->AppendToLog("Split, new above ");
                } else {
                  bestFullArray[ind].startY = divStart;
                  data.endY = divStart - 1;
                  num = (bestFullArray[ind].endY + 1 - bestFullArray[ind].startY) / 2;
                  acqYstart = (iy + 1) - num;
                  acqYend = (iy + 1) + num;
                  if (mDebug)
                    mWinApp->AppendToLog("Split, new below");
                }
                bestFullArray.Add(data);
              }
            }
          }

          // Determine limits of points that would actually go in the box
          realXstart = acqXend - 1;
          realYstart = acqYend + 1;
          realXend = acqXstart - 1;
          realYend = acqYend + 1;
          for (iy = acqYstart; iy <= acqYend; iy++) {
            for (ix = acqXstart; ix <= acqXend; ix++) {

              if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid &&
                ix >= bestFullArray[ind].startX && ix <= bestFullArray[ind].endX &&
                iy >= bestFullArray[ind].startY && iy <= bestFullArray[ind].endY &&
                mGrid[iy][ix] >= 0 && boxAssigns[mGrid[iy][ix]] < 0) {
                ACCUM_MIN(realXstart, ix);
                ACCUM_MAX(realXend, ix);
                ACCUM_MIN(realYstart, iy);
                ACCUM_MAX(realYend, iy);
              }
            }
          }

          // If a dimension is less by a multiple of 2, use the actual limits so the 
          // center ends up in the box
          if ((realXend - realXstart) % 2 != (acqXend - acqXstart) % 2) {
            realXstart = acqXstart;
            realXend = acqXend;
          }
          if ((realYend - realYstart) % 2 != (acqYend - acqYstart) % 2) {
            realYstart = acqYstart;
            realYend = acqYend;
          }
          if (realXend - realXstart < acqXend - acqXstart ||
            realYend - realYstart < acqYend - acqYstart) {
            num = 0;

            // But first make sure the center is not missing
            if ((mNumXholes * mNumYholes) % 2) {
              ix = (acqXstart + acqXend) / 2;
              iy = (acqYstart + acqYend) / 2;
              if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid &&
                (mGrid[iy][ix] < 0 || boxAssigns[mGrid[iy][ix]] >= 0))
                num = 1;
            }
            if (!num) {
              acqXstart = realXstart;
              acqXend = realXend;
              acqYstart = realYstart;
              acqYend = realYend;
            }
          }

          // Set up to find the included and skipped items
          nxBox = acqXend + 1 - acqXstart;
          nyBox = acqYend + 1 - acqYstart;
          boxXcen = (float)((nxBox - 1.) / 2.);
          boxYcen = (float)((nyBox - 1.) / 2.);
          ixSkip.clear();
          iySkip.clear();
          stageX = stageY = 0.;
          numInBox = 0;
          minCenDist = 1.e30f;

          if (mDebug)
            PrintfToLog("Point %d box %d x %d - %d y %d - %d", navInds[point], ind,
            acqXstart, acqXend, acqYstart, acqYend);

          // Loop on the positions in the (expanded) box 
          useCenStage = false;
          for (iy = acqYstart; iy <= acqYend; iy++) {
            for (ix = acqXstart; ix <= acqXend; ix++) {
              bx = ix - acqXstart;
              by = iy - acqYstart;

              // If there is a point within the original box set it as assigned to this
              // box and add in its adjusted stage position
              if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid &&
                ix >= bestFullArray[ind].startX && ix <= bestFullArray[ind].endX &&
                iy >= bestFullArray[ind].startY && iy <= bestFullArray[ind].endY &&
                mGrid[iy][ix] >= 0 && boxAssigns[mGrid[iy][ix]] < 0) {
                gridInd = mGrid[iy][ix];
                boxAssigns[gridInd] = ind;
                numInBox++;
                item = itemArray->GetAt(navInds[gridInd]);
                boxDx = (float)(boxXcen - bx);
                boxDy = (float)(boxYcen - by);
                stageX += xCenters[gridInd] + boxDx * gridMat.xpx + 
                  boxDy * gridMat.xpy;
                stageY += yCenters[gridInd] + boxDx * gridMat.ypx +
                  boxDy * gridMat.ypy;
                if (mDebug)
                  PrintfToLog("Assign %d at %d,%d (%s) to box %d, bdxy %.1f %.1f  stage X"
                  " Y %.3f %.1f", navInds[gridInd], ix, iy, (LPCTSTR)item->mLabel,
                  ind, boxDx, boxDy, item->mStageX + boxDx * gridMat.xpx + boxDy * 
                  gridMat.xpy, item->mStageY + boxDx * gridMat.ypx + boxDy * gridMat.ypy);
                cenDist = boxDx * boxDx + boxDy * boxDy;
                if (cenDist < minCenDist) {
                  minCenDist = cenDist;
                  groupID = item->mGroupID;
                  baseInd = navInds[gridInd];
                  if (skipAveraging && cenDist < .01) {
                    cenStageX = xCenters[gridInd];
                    cenStageY = yCenters[gridInd];
                    useCenStage = true;
                  }
                }
              } else {

                // Otherwise add to skip list
                ixSkip.push_back(bx);
                iySkip.push_back(by);
                if (mDebug)
                  PrintfToLog("missing %d %d", bx, by);
              }
            }
          }

          // Make a new Navigator item as clone of the first point found
          AddMultiItemToArray(itemArray, baseInd, 
            useCenStage ? cenStageX : (stageX / numInBox), 
            useCenStage ? cenStageY : (stageY / numInBox), nxBox, nyBox, boxXcen, boxYcen, 
            ixSkip, iySkip, groupID, numInBox, numAdded);
          break;
        }
      }
    }

  } else {

    // CROSSES 
    // Actually not so hard, try 5 origin positions in each of two directions
    for (xDir = -1; xDir <= 1; xDir += 2) {
      for (ori = 0; ori < 5; ori++) {
        //PrintfToLog("dir %d Origin %d %d", xDir, crossDy[ori], crossDy[ori]);
        fullArray.RemoveAll();
        peakVals.clear();
        for (iy = -2; iy <= mNyGrid + 2; iy++) {
          for (ix = -2; ix <= mNxGrid + 2; ix++) {

            // Solve for the position relative to the origin in terms of the 
            // "basis vectors", which are either (2,1) and (-1,2) or (2,-1) and (1,2)
            dx = (float)(ix - crossDx[ori]);
            dy = (float)(iy - crossDy[ori]);
            vecn = (2.f * dx + (float)xDir * dy) / 5.f;
            vecm = (2.f * dy - (float)xDir * dx) / 5.f;
            //PrintfToLog("%d %d  %.0f %.0f %.1f %.1f", ix, iy, dx, dy, vecn, vecm);
            if (fabs(vecn - B3DNINT(vecn)) < 0.01 && fabs(vecm - B3DNINT(vecm)) < 0.01) {

              // It is an integer sum of basis vectors so it is a legal position relative
              // to the origin
              EvaluateCrossAtPosition(ix, iy, data);
              if (data.numAcquires > 0) {
                fullArray.Add(data);
                peakVals.push_back((float)data.numAcquires);
              }
              //PrintfToLog("legal %d %d  n %d", ix, iy, data.numAcquires);
            }
          }
        }

        // Keep track of best one
        num = (int)fullArray.GetSize();
        avgSD(&peakVals[0], num, &dx, &fullSd, &dy);
        if (num < minFullNum || (num == minFullNum && fullSd < fullBestSd)) {
          minFullNum = num;
          fullBestSd = fullSd;
          bestFullArray.Copy(fullArray);
        }
      }
    }

    // Process points in the best arrangement
    for (point = 0; point < numPoints; point++) {
      if (boxAssigns[point] >= 0)
        continue;

      // Find the cross it is in
      ix = gridX[point];
      iy = gridY[point];
      for (ind = 0; ind < (int)bestFullArray.GetSize(); ind++) {
        bx = B3DABS(ix - bestFullArray[ind].startX);
        by = B3DABS(iy - bestFullArray[ind].startY);
        if ((bx + by < 2 && bestFullArray[ind].numAcquires > 1) || bx + by == 0) {
          /*PrintfToLog("Point %d x/y %d %d  box %d  %d %d", navInds[point], ix, iy, ind,
          bestFullArray[ind].startX, bestFullArray[ind].startY);*/

          // Set up to find the included and skipped items
          ixSkip.clear();
          iySkip.clear();
          stageX = stageY = 0.;
          numInBox = 0;
          groupID = -1;
          useCenStage = false;
          baseInd = navInds[point];
          for (ori = 0; ori < 5; ori++) {
            bx = crossDx[ori];
            by = crossDy[ori];
            ix = bestFullArray[ind].startX + bx;
            iy = bestFullArray[ind].startY + by;
            if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid && mGrid[iy][ix] >= 0
              && (ori == 0 || bestFullArray[ind].numAcquires > 1)) {
              gridInd = mGrid[iy][ix];
              boxAssigns[gridInd] = ind;
              numInBox++;
              item = itemArray->GetAt(navInds[gridInd]);
              stageX += xCenters[gridInd] - 
                (float)(bx * gridMat.xpx + by * gridMat.xpy);
              stageY += yCenters[gridInd] - 
                (float)(bx * gridMat.ypx + by * gridMat.ypy);
              /*PrintfToLog("Assign %d at %d,%d (%s) to box %d, bdxy %d %d  stage X"
                " Y %.3f %.1f", navInds[gridInd], ix, iy, (LPCTSTR)item->mLabel,
                ind, bx, by, item->mStageX - bx * gridMat.xpx - by *
                gridMat.xpy, item->mStageY - bx * gridMat.ypx - by * gridMat.ypy);*/
              if (groupID < 0 || bx + by == 0)
                groupID = item->mGroupID;
              if (skipAveraging && !ori) {
                cenStageX = xCenters[gridInd];
                cenStageY = yCenters[gridInd];
                useCenStage = true;
                baseInd = navInds[gridInd];
              }
            } else {
              ixSkip.push_back(1 + bx);
              iySkip.push_back(1 + by);
              //PrintfToLog("missing %d %d %d %d", ix, iy, 1 + bx, 1 + by);
            }
          }

          AddMultiItemToArray(itemArray, baseInd, 
            useCenStage ? cenStageX : (stageX / numInBox),
            useCenStage ? cenStageY : (stageY / numInBox), -3, -3, 1.f, 1.f, ixSkip, 
            iySkip, groupID, numInBox, numAdded);
          break;
        }
      }
    }
  }

  // Now need to save the items and remove the items from the array.  
  // Check for errors and eliminate from index list
  for (ind = numPoints - 1; ind >= 0; ind--) {
    if (boxAssigns[ind] < 0) {
      item = itemArray->GetAt(navInds[ind]);
      bx = 0;
      for (ix = 0; ix < mNxGrid && !bx; ix++) {
        for (iy = 0; iy < mNyGrid; iy++) {
          if (mGrid[iy][ix] == ind) {
            bx = 1;
            break;
          }
        }
      }
      if (bx)
        PrintfToLog("Program error: item # %d (%s) at grid %d %d not assigned to a "
          "multi-shot item; turning off Acquire", navInds[ind] + 1,
          item ? item->mLabel : "", ix, iy);
      else
        PrintfToLog("Item # %d (%s) could not be assigned a grid position; turning off "
          "Acquire", navInds[ind] + 1, item ? item->mLabel : "");
      if (item->mMapID)
        mOrphanIDsToUndo.push_back(item->mMapID);
      item->mAcquire = false;
      navInds.erase(navInds.begin() + ind);
    } 
  }
  if (mDebug) {
    mWinApp->mMacroProcessor->SetNonMacroDeferLog(false);
    if (mWinApp->mLogWindow)
      mWinApp->mLogWindow->FlushDeferredLines();
  }

  // Sort the remaining inds and save the items in the same order in the saved array
  num = (int)navInds.size();
  rsSortInts(&navInds[0], num);
  for (ind = 0; ind < num; ind++)
    mSavedItems.Add(itemArray->GetAt(navInds[ind]));

  // Remove from Nav array in reverse order, do not delete, they are in the saved array
  for (ind = num - 1; ind >= 0; ind--)
    itemArray->RemoveAt(navInds[ind]);

  // Now that single points are gone, find other items in same groups that were 
  // outside the polygon
  if ((boundType == COMBINE_IN_POLYGON || boundType == COMBINE_ON_IMAGE) &&
    turnOffOutside) {
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (mGroupIDsInPoly.count(item->mGroupID) && item->IsPoint() && item->mAcquire &&
        mSetOfUndoIDs.count(item->mMapID) == 0) {
        item->mAcquire = false;
        item->mDraw = false;
        mIDsOutsidePoly.insert(item->mMapID);
      }
    }
  }

  //Get the coordinates of new comined hole items so they can be arranged in better path
  FloatVec xCombineCenters, yCombineCenters;
  IntVec tourInd;
  int combineStart = (int) itemArray->GetSize() - numAdded;
  for (ind = combineStart; ind < itemArray->GetSize(); ind++) {
    item = itemArray->GetAt(ind);
    if (item->IsPoint() && item->mAcquire) {
      xCombineCenters.push_back(item->mStageX);
      yCombineCenters.push_back(item->mStageY);
    }
  }

  //Optimize the path of the acquire items
  ind = pathOpt.OptimizePath(xCombineCenters, yCombineCenters, tourInd);
  
  if (ind) {
    mWinApp->SetNextLogColorStyle(2,1);
    PrintfToLog(
     "WARNING: Combined hole items were not sorted into better acquisition path: %s", 
     pathOpt.returnErrorString(ind));
  }
  else {
    tempArray.Copy(*itemArray);
    for (ind = 0; ind < (int)tourInd.size(); ind++) {
      item = tempArray.GetAt(combineStart + tourInd[ind]);
      itemArray->SetAt(combineStart + ind, item);
    }

    //Calculate improvement in path length 
    if (pathOpt.mInitialPathLength > 0.) {
      //(Perhaps print the map label/note too so it's clearer)
      PrintfToLog("Length of acquisition path from item %d to %d was reduced by %.1f %%",
        combineStart + 1, combineStart + numAdded,
        100 * (1 - pathOpt.mPathLength / pathOpt.mInitialPathLength));
    }
  }

  mNav->FinishMultipleDeletion();
  for (ind = 0; ind < (int)mIndexesForUndo.size(); ind++)
    mIndexesForUndo[ind] -= (int)navInds.size();
  mWinApp->mMainView->DrawImage();
  free(mapVals);
  free(mGrid);
  return 0;
}

// Add one point to initial vectors or center positions, keep track if drawnOnID is the
// same for all points
void CMultiHoleCombiner::AddItemToCenters(FloatVec &xCenters, FloatVec &yCenters, 
  IntVec &navInds, CMapDrawItem *item, int ind, int &drawnOnID)
{
  xCenters.push_back(item->mStageX);
  yCenters.push_back(item->mStageY);
  navInds.push_back(ind);
  if (drawnOnID < 0)
    drawnOnID = item->mDrawnOnMapID;
  else if (drawnOnID != item->mDrawnOnMapID)
    drawnOnID = 0;
  if (item->mGroupID)
    mGroupIDsInPoly.insert(item->mGroupID);
}

/*
 * External call to determine if undo is possible: Nav array must be same size and all the
 * IDs of what was made must still exist, not necessarily at the original indexes
 */
bool CMultiHoleCombiner::OKtoUndoCombine(void)
{
  int jnd;
  MapItemArray *itemArray;
  CMapDrawItem *item;
  mNav = mWinApp->mNavigator;

  if (!mNav || !mSavedItems.GetSize() || !mIDsForUndo.size() || 
    mIndexesForUndo.size() != mIDsForUndo.size())
    return false;
  itemArray = mNav->GetItemArray();
  if (!itemArray)
    return false;
  for (jnd = 0; jnd < (int)mIDsForUndo.size(); jnd++) {
    if (mIndexesForUndo[jnd] < 0 || mIndexesForUndo[jnd] >= itemArray->GetSize())
      return false;
    item = itemArray->GetAt(mIndexesForUndo[jnd]);
    if (item->mMapID != mIDsForUndo[jnd]) {
      item = mNav->FindItemWithMapID(mIDsForUndo[jnd], false);
      if (!item)
        return false;
      mIndexesForUndo[jnd] = mNav->GetFoundItem();
    }
  }
  return true;
}

/*
* External call to undo it: remove the multi shot ones by ID and restore the saved items
* and remove the undo info
*/
void CMultiHoleCombiner::UndoCombination(void)
{
  int jnd;
  MapItemArray *itemArray;
  CMapDrawItem *item;

  if (!OKtoUndoCombine())
    return;
  itemArray = mNav->GetItemArray();
  for (jnd = 0; jnd < (int)mIDsForUndo.size(); jnd++) {
    item = mNav->FindItemWithMapID(mIDsForUndo[jnd], false);
    itemArray->RemoveAt(mNav->GetFoundItem());
    delete item;
  }

  itemArray->Append(mSavedItems);
  mSavedItems.RemoveAll();

  for (jnd = 0; jnd < itemArray->GetSize(); jnd++) {
    item = itemArray->GetAt(jnd);
    if (mIDsOutsidePoly.count(item->mMapID)) {
      item->mDraw = true;
      item->mAcquire = true;
    }
  }
  for (jnd = 0; jnd < (int)mOrphanIDsToUndo.size(); jnd++) {
    item = mNav->FindItemWithMapID(mOrphanIDsToUndo[jnd], false);
    if (item)
      item->mAcquire = true;
  }
  mIDsForUndo.clear();
  mSetOfUndoIDs.clear();
  mIndexesForUndo.clear();
  mIDsOutsidePoly.clear();
  mOrphanIDsToUndo.clear();
  mNav->FinishMultipleDeletion();
}

bool CMultiHoleCombiner::IsItemInUndoList(int mapID)
{
  return mSetOfUndoIDs.count(mapID) > 0;
}

// For one line (a row or a column) find the best arrangement of boxes along that line
void CMultiHoleCombiner::TryBoxStartsOnLine(int otherStart, bool doCol, 
  CArray<PositionData, PositionData> &fullArray)
{
  CArray<PositionData, PositionData> bestLineArray[2], lineArray;
  int num, cenMissing, minNum[2] = {10000000, 10000000};
  float sdOfLine, sdAtBest[2];
  int vStart = 1 - (doCol ? mNumYholes : mNumXholes);
  int bestv;

  // Try possible starts for the line of boxes at the otherStart
  for (; vStart <= 0; vStart++) {
    EvaluateLineOfBoxes(doCol ? otherStart : vStart, doCol ? vStart : otherStart, doCol,
      lineArray, sdOfLine, cenMissing);
    num = (int)lineArray.GetSize();
    if (num < minNum[cenMissing] || (num == minNum[cenMissing] && sdOfLine < 
      sdAtBest[cenMissing])) {
      minNum[cenMissing] = num;
      sdAtBest[cenMissing] = sdOfLine;
      bestLineArray[cenMissing].Copy(lineArray);
      bestv = vStart;
    }
  }

  // Add best one to the full array
  fullArray.Append(bestLineArray[minNum[0] < 1000 ? 0 : 1]);
  if (mDebug)
    PrintfToLog("Best start for %d at %d", otherStart, bestv);
}

// For a line of boxes at one start position, get the boxes and compute SD of # in each
void CMultiHoleCombiner::EvaluateLineOfBoxes(int xStart, int yStart, bool doCol,
  CArray<PositionData, PositionData> &posArray, float &sdOfNums, int &cenMissing)
{
  PositionData data;
  FloatVec xnums;
  float avg, sem;
  int vStart = doCol ? yStart : xStart;

  posArray.RemoveAll();
  cenMissing = 0;
  for (vStart; vStart < (doCol ? mNyGrid : mNxGrid); 
    vStart += (doCol ? mNumYholes : mNumXholes)) {
    EvaluateBoxAtPosition(doCol ? xStart : vStart, doCol ? vStart : yStart, data);
    if (data.numAcquires > 0) {
      xnums.push_back((float)data.numAcquires);
      posArray.Add(data);
      ACCUM_MAX(cenMissing, data.cenMissing);
    }
  }
  sdOfNums = 0;
  if (xnums.size() > 1) {
    avgSD(&xnums[0], (int)xnums.size(), &avg, &sdOfNums, &sem);
    if (mDebug)
      PrintfToLog("xs %d  ys %d col %d  numbox %d  avg %.1f sd %.2f", xStart, yStart,
        doCol ? 1 : 0, xnums.size(), avg, sdOfNums);
  }
}

// Find grid points inside a box and compute the limits actually occupied for the box
void CMultiHoleCombiner::EvaluateBoxAtPosition(int xStart, int yStart, PositionData &data)
{
  int ix, iy;
  data.numAcquires = 0;
  data.startX = mNxGrid;
  data.endX = -1;
  data.startY = mNyGrid;
  data.endY = -1;
  data.cenMissing = 0;
  for (ix = B3DMAX(xStart, 0); ix < B3DMIN(xStart + mNumXholes, mNxGrid); ix++) {
    for (iy = B3DMAX(yStart, 0); iy < B3DMIN(yStart + mNumYholes, mNyGrid); iy++) {
      if (mGrid[iy][ix] >= 0) {
        data.numAcquires++;
        ACCUM_MIN(data.startX, ix);
        ACCUM_MAX(data.endX, ix);
        ACCUM_MIN(data.startY, iy);
        ACCUM_MAX(data.endY, iy);
      }
    }
  }
  if ((mNumXholes * mNumYholes) % 2 != 0 && data.endX + 1 - data.startX == mNumXholes &&
    data.endY + 1 - data.startY == mNumYholes && 
    mGrid[yStart + mNumYholes / 2][xStart + mNumXholes / 2] < 0)
    data.cenMissing = 1;
}

// Try to combine adjacent boxes if that fits wihing the box size, and then get the SD
// of the # of points in the boxes
void CMultiHoleCombiner::mergeBoxesAndEvaluate(
  CArray<PositionData, PositionData> &posArray, float &sdOfNums)
{
  FloatVec xnums;
  float avg, sem;
  int ind, jnd, startX, endX, startY, endY;

  for (ind = 0; ind < posArray.GetSize() - 1; ind++) {
    for (jnd = ind + 1; jnd < posArray.GetSize(); jnd++) {

      // For each pair, get the limits if merged and combine if that is still within
      // the size of a single box.  This is not optimized to pixk the BEST combinations
      startX = B3DMIN(posArray[ind].startX, posArray[jnd].startX);
      endX = B3DMAX(posArray[ind].endX, posArray[jnd].endX);
      if (endX - startX < mNumXholes) {
        startY = B3DMIN(posArray[ind].startY, posArray[jnd].startY);
        endY = B3DMAX(posArray[ind].endY, posArray[jnd].endY);
        if (endY - startY < mNumYholes) {
          if (mDebug)
            PrintfToLog("Merge %d: x %d %d y %d %d with %d: x %d %d y %d %d", 
              ind, posArray[ind].startX, posArray[ind].endX, posArray[ind].startY, 
              posArray[ind].endY, jnd, posArray[jnd].startX, posArray[jnd].endX,
              posArray[jnd].startY, posArray[jnd].endY);

          // Combine the ranges into the first position, make the second one impossible
          posArray[ind].startX = startX;
          posArray[ind].endX = endX;
          posArray[ind].startY = startY;
          posArray[ind].endY = endY;
          posArray[jnd].startX = -100;
          posArray[jnd].endX = 1000;
          posArray[jnd].startY = -100;
          posArray[jnd].endY = 1000;
          posArray[ind].numAcquires += posArray[jnd].numAcquires;
          posArray[jnd].numAcquires = 0;
        }
      }
    }
  }

  // Eliminate the merged ones and get the SD
  for (ind = (int)posArray.GetSize() - 1; ind >= 0; ind--) {
    if (posArray[ind].numAcquires)
      xnums.push_back((float)posArray[ind].numAcquires);
    else
      posArray.RemoveAt(ind);
  }
  sdOfNums = 0;
  if (xnums.size() > 1)
    avgSD(&xnums[0], (int)xnums.size(), &avg, &sdOfNums, &sem);
}

// For a particular box, get adjusted limits to define the size of the multi-shot item
// so that it matches the even/oddness of the nominal number of holes in that dimension
void CMultiHoleCombiner::SetBoxAcquireLimits(int start, int end, int numHoles, int nGrid,
  int &acqStart, int &acqEnd)
{
  acqStart = start;
  acqEnd = end;
  if ((end + 1 - start) % 2 != numHoles % 2) {
    if (end + start < nGrid)
      acqEnd++;
    else
      acqStart--;
  }
}

// Make a position if any points fit are within a cross centered at the given position
void CMultiHoleCombiner::EvaluateCrossAtPosition(int xCen, int yCen, PositionData &data)
{
  int ix, iy, ind;
  int ixDel[5] = {0, -1, 1, 0, 0}, iyDel[5] = {0, 0, 0, -1, 1};
  data.numAcquires = 0;

  // Store the center position in the starts and the point position in ends, so if there
  // is one point it can be shifted to for acquisition
  data.startX = xCen;
  data.startY = yCen;
  for (ind = 0; ind < 5; ind++) {
    ix = xCen + ixDel[ind];
    iy = yCen + iyDel[ind];
    if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid) {
      data.numAcquires++;
      data.endX = ix;
      data.endY = iy;
    }
  }
  if (data.numAcquires == 1) {
    data.startX = data.endX;
    data.startY = data.endY;
  }
}

// Work outward from a given starting point, try hexagons in all possible rows, with pitch
// along a row either up or down
int CMultiHoleCombiner::EvaluateArrangementOfHexes(int xCen, int yCen, bool pitchDown, 
  CArray<PositionData, PositionData> &posArray, float &sdOfNums, int &cenMissing)
{
  int xDir, yDir, tryXcen, tryYcen, rowXcen, rowYcen, delY, delX;
  bool foundInRow;
  PositionData data;
  FloatVec xnums;
  float avg, sem;

  // Loop Y directions from middle, start row center at center
  for (yDir = -1; yDir <= 1; yDir += 2) {
    rowXcen = xCen;
    rowYcen = yCen;
    //PrintfToLog("start ydir %d row cen %d %d", yDir, rowXcen, rowYcen);

    // Loop on rows in Y
    for (delY = 0; delY < mNyGrid; delY++) {
      if (yDir > 0 && !delY) {
        //PrintfToLog("delY %d skip", delY);
        continue;
      }
      if (delY) {

        // Take a step in the right direction for the selected pitch and Y direction
        if (pitchDown) {

          // Row pitches down below X axis
          if (yDir < 0) {
            rowXcen += 2 * mNumRings + 1;
            rowYcen -= mNumRings + 1;
          } else {
            rowXcen += mNumRings + 1;
            rowYcen += mNumRings;
          }

          // Keep shifting X back to the middle
          if (rowXcen > xCen + 3 * mNumRings + 2) {
            rowXcen -= 3 * mNumRings + 2;
            rowYcen += 1;
          }
        } else {

          // Similarly for pitching up above X axis
          if (yDir < 0) {
            rowXcen += 2 * mNumRings + 1;
            rowYcen -= mNumRings;
          } else {
            rowXcen += mNumRings;
            rowYcen += mNumRings + 1;
          }
          if (rowXcen > xCen + 3 * mNumRings + 1) {
            rowXcen -= 3 * mNumRings + 1;
            rowYcen -= 1;
          }
        }
      }
      //PrintfToLog("ydir %d delY %d  rowcen %d %d", yDir, delY, rowXcen, rowYcen);

      // Loop on X directions
      foundInRow = false;
      for (xDir = -1; xDir <= 1; xDir += 2) {
        tryXcen = rowXcen;
        tryYcen = rowYcen;
        //PrintfToLog("start xdir %d row/try cen %d %d", yDir, rowXcen, rowYcen);

        // Loop across the row
        for (delX = 0; delX < mNxGrid; delX++) {
          if (xDir > 0 && !delX) {
            //PrintfToLog("Skip xdir %d at 0", xDir);
            continue;
          }
          if (delX) {

            // This is simpler, just walk one way or the other along the row by amounts
            // that depend on the pitch
            if (pitchDown) {
              tryXcen += xDir * (3 * mNumRings + 2);
              tryYcen -= xDir;
            } else {
              tryXcen += xDir * (3 * mNumRings + 1);
              tryYcen += xDir;
            }
          }

          // End of row reached
          if (tryXcen + mNumRings < 0 || tryYcen + mNumRings < 0 ||
            tryXcen - mNumRings >= mNxGrid || tryYcen - mNumRings >= mNyGrid) {
            if ((xDir < 0 && tryXcen - mNumRings >= mNxGrid) ||
              (xDir > 0 && tryXcen + mNumRings < 0)|| ((tryYcen + mNumRings < 0 ||
              tryYcen - mNumRings >= mNyGrid) && tryXcen + mNumRings >= 0 && tryXcen - mNumRings < mNxGrid)) {
              //PrintfToLog("xd %d  cen %d %d out of range - continue", xDir, tryXcen, tryYcen);
            continue;
            }
            //PrintfToLog("xd %d  cen %d %d out of range - break", xDir, tryXcen, tryYcen);
            break;
          }

          // Otherwise evaluate
          foundInRow = true;
          EvaluateHexAtPosition(tryXcen, tryYcen, data, NULL);
          //PrintfToLog("dy %d yd %d dx %d xd %d trycen %d %d  num %d  miss %d  use cen %d %d", 
            //delY, yDir, delX, xDir, tryXcen, tryYcen, data.numAcquires, data.cenMissing, data.xCen, data.yCen);
          if (data.numAcquires) {

            // If center is missing, assume it will be split into two equal parts:
            // the evaluate routine already tried shifting to new center that would cover
            // the all points
            if (data.cenMissing > 0 && data.numAcquires > 1) {
              xnums.push_back((float)(data.numAcquires / 2));
              xnums.push_back((float)(data.numAcquires - data.numAcquires / 2));
            } else
              xnums.push_back((float)data.numAcquires);
            posArray.Add(data);
            if (data.cenMissing > 0)
              cenMissing += data.cenMissing;
          }
        }
      }

      // Done with Y direction when all positions in a row out of range
      if (!foundInRow) {
        //PrintfToLog("Nonew found in row from cen %d %d", rowXcen, rowYcen);
        break;
      }
    }
  }

  // Compute the SD assuming splits for cen missing, and return number based on that too
  sdOfNums = 0.;
  if (xnums.size() > 1)
    avgSD(&xnums[0], (int)xnums.size(), &avg, &sdOfNums, &sem);
  return (int)xnums.size();
}

// See how many points are included in a hexagon at the given center
void CMultiHoleCombiner::EvaluateHexAtPosition(int xCen, int yCen, PositionData &data,
  IntVec *boxAssigns)
{
  int ix, iy, ind, minXshift, maxXshift, minYshift, maxYshift, midXshift, midYshift;
  int ring, xShift, yShift, distSq, tryXcen, tryYcen;
  bool anyShifts, allInside;
  std::set<int> covered;
  IntVec indInHex;
  data.numAcquires = 0;
  data.startX = mNxGrid;
  data.endX = -1;
  data.startY = mNyGrid;
  data.endY = -1;

  // Set center missing here, save center for hexes
  data.cenMissing = (xCen < 0 || xCen >= mNxGrid || yCen < 0 || yCen>= mNyGrid || 
    mGrid[yCen][xCen] < 0) ? 1 : 0;
  data.xCen = xCen;
  data.yCen = yCen;
  for (ind = 0; ind < (int)mHexDelX.size(); ind++) {
    ix = xCen + mHexDelX[ind];
    iy = yCen + mHexDelY[ind];
    if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid && mGrid[iy][ix] >= 0 &&
      (!boxAssigns || boxAssigns->at(mGrid[iy][ix]) < 0)) {
      data.numAcquires++;
      ACCUM_MIN(data.startX, ix);
      ACCUM_MAX(data.endX, ix);
      ACCUM_MIN(data.startY, iy);
      ACCUM_MAX(data.endY, iy);

      // If center missing, make list of points in the hex at current position
      if (data.cenMissing)
        indInHex.push_back(ix + iy * mNxGrid);
    }
  }
  if (!data.cenMissing || !data.numAcquires)
    return;

  // If center missing, see if it can be shifted
  minXshift = B3DMAX(data.startX, xCen - mNumRings) - xCen;
  maxXshift = B3DMIN(data.endX, xCen + mNumRings) - xCen;
  minYshift = B3DMAX(data.startY, yCen - mNumRings) - yCen;
  maxYshift = B3DMIN(data.endY, yCen + mNumRings) - yCen;
  if (!minXshift && !maxXshift && !minYshift && !maxYshift)
    return;

  // Loop on possible shifts from the middle shift outward
  midXshift = (maxXshift + minXshift) / 2;
  midYshift = (maxYshift + minYshift) / 2;
  for (ring = 1; ring < 2 * mNumRings; ring++) {
    anyShifts = false;
    for (yShift = minYshift; yShift <= maxYshift; yShift++) {
      for (xShift = minXshift; xShift <= maxXshift; xShift++) {
        distSq = (xShift - midXshift) * (xShift - midXshift) +
          (yShift - midYshift) * (yShift - midYshift);
        if (distSq > (ring - 1) * (ring - 1) && distSq <= ring * ring) {

          // Testing a shift to a trial center - is there a point there?
          anyShifts = true;
          tryXcen = xCen + xShift;
          tryYcen = yCen + yShift;
          if (mGrid[tryYcen][tryXcen] < 0)
            continue;

          // If so make set of indexes covered by the hex
          covered.clear();
          for (ind = 0; ind < (int)mHexDelX.size(); ind++)
            covered.insert(tryXcen + mHexDelX[ind] + (tryYcen + mHexDelY[ind]) * mNxGrid);

          // Test if all points are covered
          allInside = true;

          for (ind = 0; ind < (int)indInHex.size(); ind++) {
            if (!covered.count(indInHex[ind])) {
              allInside = false;
              break;
            }
          }

          // If all covered, shift the center and set the missing flag to -1
          if (allInside) {
            data.cenMissing = -1;
            data.useXcen = tryXcen;
            data.useYcen = tryYcen;
            return;
          }
        }
      }
    }

    // Finished when no more shifts found
    if (!anyShifts)
      break;
  }
}

// Determine which points are in a hex item, average the implied center position, and
// compose the skip list for skipped positions
void CMultiHoleCombiner::AddPointsToHexItem(MapItemArray *itemArray, 
  PositionData &data, int hex, IntVec &boxAssigns, IntVec &navInds, 
  FloatVec &xCenters, FloatVec &yCenters, ScaleMat gridMat, IntVec &ixSkip, 
  IntVec &iySkip, int &numAdded)
{
  CMapDrawItem *item;
  int ind, ix, iy, xCen, yCen, numInBox, ptNum, groupID, bx, by, fullInd;
  int acqXcen, acqYcen, baseInd, itemInd;
  float stageX, stageY, boxDx, boxDy, cenDist, cenStageX, cenStageY, minCenDist = 1.e30f;
  bool useCenStage = false;
  std::set<int> cenSet;
  ixSkip.clear();
  iySkip.clear();
  stageX = stageY = 0.;
  numInBox = 0;
  
  // Set the acquisition center: if it got moved, then make a list of point indexes that
  // are in range of the original center
  acqXcen = xCen = data.xCen;
  acqYcen = yCen = data.yCen;
  if (data.cenMissing < 0) {
    acqXcen = data.useXcen;
    acqYcen = data.useYcen;
    for (ind = 0; ind < (int)mHexDelX.size(); ind++) {
      cenSet.insert(xCen + mHexDelX[ind] + mNxGrid * (yCen + mHexDelY[ind]));
    }
  }
  if (mDebug)
    PrintfToLog("Hex %d  cen %d %d  acq cen %d %d   numAdded start %d", hex, xCen, yCen, 
      acqXcen, acqYcen, numAdded);
  for (ind = 0; ind < (int)mHexDelX.size(); ind++) {
    bx = mHexDelX[ind];
    by = mHexDelY[ind];
    ix = acqXcen + bx;
    iy = acqYcen + by;
    fullInd = ix + mNxGrid * iy;
    if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid && mGrid[iy][ix] >= 0 &&
      boxAssigns[mGrid[iy][ix]] < 0 && (data.cenMissing >= 0 || cenSet.count(fullInd))) {
      ptNum = mGrid[iy][ix];
      boxAssigns[ptNum] = hex;
      numInBox++;
      item = itemArray->GetAt(navInds[ptNum]);
      boxDx = (float)bx;
      boxDy = (float)by;

      // It makes perfect sense to subtract this here - why is it added for square grids?
      stageX += xCenters[ptNum] - (boxDx * gridMat.xpx + boxDy * gridMat.xpy);
      stageY += yCenters[ptNum] - (boxDx * gridMat.ypx + boxDy * gridMat.ypy);
      if (mDebug)
        PrintfToLog("Assign %d at %d,%d (%s) to hex %d, bdxy %.0f %.0f  %s X"
          " Y %.3f %.3f -> %.3f %.3f", navInds[ptNum], ix, iy, (LPCTSTR)item->mLabel,
          hex, boxDx, boxDy, mUseImageCoords ? "image" : "stage", xCenters[ptNum],
          yCenters[ptNum], xCenters[ptNum] - (boxDx * gridMat.xpx + boxDy *
          gridMat.xpy), yCenters[ptNum] - (boxDx * gridMat.ypx + boxDy * gridMat.ypy));
      cenDist = boxDx * boxDx + boxDy * boxDy;
      if (cenDist < minCenDist) {
        minCenDist = cenDist;
        groupID = item->mGroupID;
        baseInd = navInds[ptNum];
        if (cenDist < 0.1 && mHelper->GetMHCskipAveragingPos()) {
          cenStageX = xCenters[ptNum];
          cenStageY = yCenters[ptNum];
          useCenStage = true;
          if (mDebug)
            PrintfToLog("Set center stage %.3f %.3f", cenStageX, cenStageY);
        }
      }
    } else {
      ixSkip.push_back(bx);
      iySkip.push_back(by);
      if (mDebug)
        PrintfToLog("missing %d %d at %d %d", bx, by, ix, iy);
    }
  }

  itemInd = (int)itemArray->GetSize();
  AddMultiItemToArray(itemArray, baseInd, useCenStage ? cenStageX : (stageX / numInBox),
    useCenStage ? cenStageY : (stageY / numInBox), mNumRings,
    -1, 0., 0., ixSkip, iySkip, groupID, numInBox, numAdded);
  if (itemArray->GetSize() > itemInd) {
    if (mDebug && mHexToItemIndex[hex] > 0) {
      item = itemArray->GetAt(mHexToItemIndex[hex]);
      PrintfToLog("HEX INDEX %d ALREADY ASSIGNED TO ITEM %d  %s", hex, 
        mHexToItemIndex[hex], item->mLabel);
    }
    mHexToItemIndex[hex] = itemInd;
  }
}

// Add an item to the nav array with locked-in multi-shot parameters and skipped points
void CMultiHoleCombiner::AddMultiItemToArray(
  MapItemArray* itemArray, int baseInd, float stageX, 
  float stageY, int numXholes, int numYholes, float boxXcen, float boxYcen, 
  IntVec &ixSkip, IntVec &iySkip, int groupID, int numInBox, int &numAdded)
{
  CMapDrawItem *newItem, *item;
  int ix, ind;
  float skipXrot, skipYrot, tempX;
  float backXcen = mTransposeSize ? boxYcen : boxXcen;
  float backYcen = mTransposeSize ? boxXcen : boxYcen;
  bool fewItems = numInBox < mWinApp->mNavHelper->GetMHCthreshNumHoles();
  if (!numInBox || (mWinApp->mNavHelper->GetMHCdelOrTurnOffIfFew() == 1 && fewItems))
    return;

  // Set the stage position and # of holes and put in the skip list
  item = itemArray->GetAt(baseInd);
  newItem = item->Duplicate();

  // But first convert image to stage after adjusting for montage offsets
  if (mUseImageCoords) {

    mWinApp->mNavHelper->AdjustMontImagePos(mImBuf, stageX, stageY, &newItem->mPieceDrawnOn,
      &newItem->mXinPiece, &newItem->mYinPiece);
    tempX = mBITSmat.xpx * (stageX - mBSTIdelX) + mBITSmat.xpy * (stageY - mBSTIdelY);
    stageY = mBITSmat.ypx * (stageX - mBSTIdelX) + mBITSmat.ypy * (stageY - mBSTIdelY);
    stageX = tempX;
  }

  newItem->mStageX = stageX;
  newItem->mStageY = stageY;
  newItem->mGroupID = groupID;
  if (newItem->mNumPoints) {
    newItem->mPtX[0] = stageX;
    newItem->mPtY[0] = stageY;
  }
  newItem->mNote.Format("%d holes", numInBox);
  mIDsForUndo.push_back(newItem->mMapID);
  mSetOfUndoIDs.insert(newItem->mMapID);
  mIndexesForUndo.push_back((int)itemArray->GetSize());
  if (mWinApp->mNavHelper->GetMHCdelOrTurnOffIfFew() > 1 && fewItems) {
    newItem->mAcquire = false;
    newItem->mDraw = false;
  }

  // The number of holes is still from the stage-space analysis and has to be transposed
  // for the item if IS-space is rotated +/-90 from that.  Also the incoming box centers
  // to subtract for the transform are in stage space, but need to add back on transposed
  // centers (above) to get to IS space values
  if (mTransposeSize)
    B3DSWAP(numXholes, numYholes, ix);
  newItem->mNumXholes = numXholes;
  newItem->mNumYholes = numYholes;
  newItem->mNumSkipHoles = (short)ixSkip.size();
  delete newItem->mSkipHolePos;
  newItem->mSkipHolePos = NULL;
  if (ixSkip.size()) {
    newItem->mSkipHolePos = new unsigned char[2 * newItem->mNumSkipHoles];
    for (ix = 0; ix < newItem->mNumSkipHoles; ix++) {

      if (numYholes != -1) {
        ApplyScaleMatrix(mSkipXform, ixSkip[ix] - boxXcen,
          iySkip[ix] - boxYcen, skipXrot, skipYrot, false, false);
        newItem->mSkipHolePos[2 * ix] = (unsigned char)B3DNINT(skipXrot + backXcen);
        newItem->mSkipHolePos[2 * ix + 1] = (unsigned char)B3DNINT(skipYrot + backYcen);
        if (mDebug)
          PrintfToLog("Skip %d %d  cen %.1f %.1f skiprot  %.1f %.1f  cen %.1f %.1f shp %d"
            " %d", ixSkip[ix], iySkip[ix], boxXcen, boxYcen, skipXrot, skipYrot, backXcen,
            backYcen, newItem->mSkipHolePos[2 * ix], newItem->mSkipHolePos[2 * ix + 1]);
      } else {
        ind = ixSkip[ix] + mNumRings + (iySkip[ix] + mNumRings) * (2 * mNumRings + 1);
        if (mOneDIndexToHexRing[ind] >= 0) {
          newItem->mSkipHolePos[2 * ix] = (unsigned char)mOneDIndexToHexRing[ind];
          newItem->mSkipHolePos[2 * ix + 1] = (unsigned char)mOneDIndexToPosInRing[ind];
          if (mDebug)
            PrintfToLog("%d %d (%d) -> %d %d", ixSkip[ix], ixSkip[ix], ind, 
            (int)newItem->mSkipHolePos[2 * ix], (int)newItem->mSkipHolePos[2 * ix + 1]);
        } else {
          PrintfToLog("Program error: no ring/pos value for ix %d iy %d ind %d",
            ixSkip[ix], iySkip[ix], ind);
        }
      }
    }
  }
  itemArray->Add(newItem);
  if (!numAdded)
    item = itemArray->GetAt(itemArray->GetSize() - 1);
  numAdded++;
}

// Clear out the array of saved items
void CMultiHoleCombiner::ClearSavedItemArray(bool originalToo, bool updateDlg)
{
  mPreCombineHoles.Append(mSavedItems);
  mSavedItems.RemoveAll();
  if (originalToo) {
    for (int ind = 0; ind < (int)mPreCombineHoles.GetSize(); ind++)
      delete mPreCombineHoles[ind];
    mPreCombineHoles.RemoveAll();
  }
  if (updateDlg && mWinApp->mNavHelper->mMultiCombinerDlg)
    mWinApp->mNavHelper->mMultiCombinerDlg->UpdateEnables();
}

