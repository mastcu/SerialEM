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
#include "MultiHoleCombiner.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"
#include "holefinder.h"
#include "MapDrawItem.h"
#include <algorithm>
#include "Shared\b3dutil.h"

// Error reporting
enum {
  ERR_NO_NAV = 1, ERR_NO_IMAGE, ERR_NOT_POLYGON, ERR_NO_GROUP, ERR_NO_CUR_ITEM,
  ERR_TOO_FEW_POINTS, ERR_MEMORY, ERR_CUSTOM_HOLES, ERR_HOLES_NOT_ON, ERR_NOT_DEFINED, 
  ERR_NO_XFORM, ERR_BAD_UNIT_XFORM, ERR_LAST_ENTRY
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
"acquisition positions", ""};

CMultiHoleCombiner::CMultiHoleCombiner(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();

}

CMultiHoleCombiner::~CMultiHoleCombiner(void)
{
  ClearSavedItemArray();
 
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
 * Main call to combine items, picked by the specified boundary type
 */
int CMultiHoleCombiner::CombineItems(int boundType, BOOL turnOffOutside)
{
  CMapDrawItem *item, *curItem;
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
  KImage *image;
  FloatVec xCenters, yCenters, peakVals, xCenAlt, yCenAlt, peakAlt, xMissing, yMissing;
  IntVec navInds, altInds, boxAssigns, ixSkip, iySkip;
  ShortVec gridX, gridY;
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  MontParam *montp;
  int *activeList;
  LowDoseParams *ldp;
  ScaleMat s2c, is2cam, st2is, gridMat, holeInv, holeMat, prodMat;
  PositionData data;
  CArray<PositionData, PositionData> fullArray, bestFullArray;
  float avgAngle, spacing;
  double hx, hy;
  float nearIntCrit = 0.33f;
  int ind, nx, ny, numPoints, ix, iy, groupID, drawnOnID = -1;
  int rowStart, colStart, num, minFullNum = 10000000;
  float fDTOR = (float)DTOR;
  float fullSd, fullBestSd, ptX, ptY;
  float minCenDist, cenDist, boxDx, boxDy;
  bool crossPattern, leftOK, rightOK, upOK, downOK;
  int *mapVals;
  int camera = mWinApp->GetCurrentCamera();
  int yStart, xStart, xDir, magInd, registration, divStart;
  int point, acqXstart, acqXend, acqYstart, acqYend, nxBox, nyBox, numInBox, bx, by;
  float stageX, stageY, boxXcen, boxYcen, dx, dy, vecn, vecm;
  int realXstart, realYstart, realXend, realYend;

  int ori, crossDx[5] = {0, -1, 1, 0, 0}, crossDy[5] = {0, 0, 0, -1, 1};
  int numAdded = 0;
  mDebug = 0;
  mUseImageCoords = false;
  mGroupIDsInPoly.clear();

  // Check for Nav
  mHelper = mWinApp->mNavHelper;
  mFindHoles = mHelper->mFindHoles;
  mNav = mWinApp->mNavigator;
  if (!mNav)
    return ERR_NO_NAV;
  registration = mNav->GetCurrentRegistration();
  itemArray = mNav->GetItemArray();
  if (!itemArray)
    return ERR_NO_NAV;

  // Make sure a regular multi-hole is defined
  mHelper->UpdateMultishotIfOpen();
  if (!(msParams->inHoleOrMultiHole & MULTI_HOLES))
    return ERR_HOLES_NOT_ON;
  if (msParams->holeMagIndex <= 0)
    return ERR_NOT_DEFINED;
  if (msParams->useCustomHoles)
    return ERR_CUSTOM_HOLES;
  mNumXholes = msParams->numHoles[0];
  mNumYholes = msParams->numHoles[1];
  crossPattern = mNumXholes == 3 && mNumYholes == 3 && msParams->skipCornersOf3x3;

  mNav->GetCurrentOrGroupItem(curItem);
  if (boundType != COMBINE_ON_IMAGE && !curItem)
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

  // Find points that are on image
  if (boundType == COMBINE_ON_IMAGE) {
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
    drawnOnID = 0;
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
  if (drawnOnID > 0) {
    ind = mNav->FindBufferWithMontMap(drawnOnID);
    if (ind >= 0) {
      mImBuf = mWinApp->GetImBufs() + ind;
      itemArray = mWinApp->mMainView->GetMapItemsForImageCoords(mImBuf, true);
      mUseImageCoords = itemArray != NULL;
    }
  }

  if (mUseImageCoords && !mNav->BufferStageToImage(mImBuf, mBSTImat, mBSTIdelX,
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
    mBSTImat = MatInv(mBSTImat);
    st2is = MatMul(mBSTImat, st2is);
  }

  // Get average angle and length with whichever positions
  mFindHoles->analyzeNeighbors(xCenters, yCenters, peakVals, altInds, xCenAlt, yCenAlt,
    peakAlt, 0., 0., 0, spacing, xMissing, yMissing);
  mFindHoles->getGridVectors(gridMat.xpx, gridMat.xpy, gridMat.ypx, gridMat.ypy, avgAngle,
      spacing);

  // Get a matrix that would transform from relative hole positions in unit vector stage
  // space to relative hole positions in unit vector image shift space
  // gridMat takes unit vectors in stage or image hole space to stage or image positions
  // st2is takes those positions to image shift positions
  // Transfer the hole vectors from where they were defined to the mag in question
  // Compute a transformation from IS space hole number to IS and take its inverse
  mWinApp->mShiftManager->TransferGeneralIS(msParams->holeMagIndex,
    msParams->holeISXspacing[0], msParams->holeISYspacing[0], magInd, hx, hy, camera);
  holeInv.xpx = (float)hx;
  holeInv.ypx = (float)hy;
  mWinApp->mShiftManager->TransferGeneralIS(msParams->holeMagIndex,
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
  /*PrintfToLog(
    " Matrix to xform relative hole positions from %s to IS space: %.3f %.3f %.3f "
    "%.3f\r\n gridMat:  %.3f %.3f %.3f %.3f\r\n %s to IS:  %f  %f  %f  %f\r\n"
    "holeMat: %.3f %.3f %.3f %.3f", mUseImageCoords ? "image" : "stage",
    prodMat.xpx, prodMat.xpy, prodMat.ypx, prodMat.ypy,
    gridMat.xpx, gridMat.xpy, gridMat.ypx, gridMat.ypy,
    mUseImageCoords ? "image" : "stage", st2is.xpx, st2is.xpy, st2is.ypx, st2is.ypy,
    holeMat.xpx, holeMat.xpy, holeMat.ypx, holeMat.ypy);*/

  // Now get grid positions
  mFindHoles->assignGridPositions(xCenters, yCenters, gridX, gridY, avgAngle, spacing);

 // If the xpx term is 0, this means thare is a 90 degree rotation involved
  // In this case, the number of holes in stage coordinates is transposed from
  // the specified geometry.  Holes will be analyzed in stage coordinates, but the hole
  // geometry has to be back in image shift space when making Nav items
  mTransposeSize = fabs(mSkipXform.xpx) < 0.1;
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
    mGrid[gridY[ind]][gridX[ind]] = ind;

  boxAssigns.resize(numPoints, -1);
  ClearSavedItemArray();
  mIDsForUndo.clear();
  mSetOfUndoIDs.clear();
  mIndexesForUndo.clear();
  mIDsOutsidePoly.clear();

  if (!crossPattern) {

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
                  mWinApp->AppendToLog("Split, new right");
                } else {
                  bestFullArray[ind].startX = divStart;
                  data.endX = divStart - 1;
                  num = (bestFullArray[ind].endX + 1 - bestFullArray[ind].startX) / 2;
                  acqXstart = (ix + 1) - num;
                  acqXend = (ix + 1) + num;
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
                boxAssigns[mGrid[iy][ix]] = ind;
                numInBox++;
                item = itemArray->GetAt(navInds[mGrid[iy][ix]]);
                boxDx = (float)(boxXcen - bx);
                boxDy = (float)(boxYcen - by);
                stageX += xCenters[mGrid[iy][ix]] + boxDx * gridMat.xpx + 
                  boxDy * gridMat.xpy;
                stageY += yCenters[mGrid[iy][ix]] + boxDx * gridMat.ypx +
                  boxDy * gridMat.ypy;
                if (mDebug)
                  PrintfToLog("Assign %d at %d,%d (%s) to box %d, bdxy %.1f %.1f  stage X"
                  " Y %.3f %.1f", navInds[mGrid[iy][ix]], ix, iy, (LPCTSTR)item->mLabel,
                  ind, boxDx, boxDy, item->mStageX + boxDx * gridMat.xpx + boxDy * 
                  gridMat.xpy, item->mStageY + boxDx * gridMat.ypx + boxDy * gridMat.ypy);
                cenDist = boxDx * boxDy + boxDy * boxDy;
                if (cenDist < minCenDist) {
                  minCenDist = cenDist;
                  groupID = item->mGroupID;
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
          AddMultiItemToArray(itemArray, navInds[point], stageX / numInBox, 
            stageY / numInBox, nxBox, nyBox, boxXcen, boxYcen, ixSkip, iySkip, groupID,
            numAdded);
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
          for (ix = -2; ix <= mNyGrid + 2; ix++) {

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
          for (ori = 0; ori < 5; ori++) {
            bx = crossDx[ori];
            by = crossDy[ori];
            ix = bestFullArray[ind].startX + bx;
            iy = bestFullArray[ind].startY + by;
            if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid && mGrid[iy][ix] >= 0
              && (ori == 0 || bestFullArray[ind].numAcquires > 1)) {
              boxAssigns[mGrid[iy][ix]] = ind;
              numInBox++;
              item = itemArray->GetAt(navInds[mGrid[iy][ix]]);
              stageX += xCenters[mGrid[iy][ix]] - 
                (float)(bx * gridMat.xpx + by * gridMat.xpy);
              stageY += yCenters[mGrid[iy][ix]] - 
                (float)(bx * gridMat.ypx + by * gridMat.ypy);
              if (groupID < 0 || bx + by == 0)
                groupID = item->mGroupID;
            } else {
              ixSkip.push_back(1 + bx);
              iySkip.push_back(1 + by);
              //PrintfToLog("missing %d %d %d %d", ix, iy, 1 + bx, 1 + by);
            }
          }

          AddMultiItemToArray(itemArray, navInds[point], stageX / numInBox,
            stageY / numInBox, -3, -3, 1.f, 1.f, ixSkip, iySkip, groupID, numAdded);
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
      PrintfToLog("Program error: item # %d (%s) not assigned to a multi-shot item", 
        navInds[ind] + 1, item ? item->mLabel : "");
      navInds.erase(navInds.begin() + ind);
    }
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
  if (boundType == COMBINE_IN_POLYGON && turnOffOutside) {
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
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
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
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
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
  mIDsForUndo.clear();
  mSetOfUndoIDs.clear();
  mIndexesForUndo.clear();
  mIDsOutsidePoly.clear();
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
  if (xnums.size() > 1)
    avgSD(&xnums[0], (int)xnums.size(), &avg, &sdOfNums, &sem);
  if (mDebug)
    PrintfToLog("xs %d  ys %d col %d  numbox %d  avg %.1f sd %.2f", xStart, yStart, 
      doCol ? 1 : 0, xnums.size(), avg, sdOfNums);
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
    data.endY + 1 - data.startY == mNumXholes && 
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

// Add an item to the nav array with locked-in multi-shot parameters and skipped points
void CMultiHoleCombiner::AddMultiItemToArray(
  CArray<CMapDrawItem*, CMapDrawItem*>* itemArray, int baseInd, float stageX, 
  float stageY, int numXholes, int numYholes, float boxXcen, float boxYcen, 
  IntVec &ixSkip, IntVec &iySkip, int groupID, int &numAdded)
{
  CMapDrawItem *newItem, *item;
  int ix;
  float skipXrot, skipYrot, tempX;
  float backXcen = mTransposeSize ? boxYcen : boxXcen;
  float backYcen = mTransposeSize ? boxXcen : boxYcen;

  // Set the stage position and # of holes and put in the skip list
  item = itemArray->GetAt(baseInd);
  newItem = item->Duplicate();

  // But first convert image to stage after adjusting for montage offsets
  if (mUseImageCoords) {
    mNav->AdjustMontImagePos(mImBuf, stageX, stageY, &newItem->mPieceDrawnOn, 
      &newItem->mXinPiece, &newItem->mYinPiece);
    tempX = mBSTImat.xpx * (stageX - mBSTIdelX) + mBSTImat.xpy * (stageY - mBSTIdelY);
    stageY = mBSTImat.ypx * (stageX - mBSTIdelX) + mBSTImat.ypy * (stageY - mBSTIdelY);
    stageX = tempX;
  }
  newItem->mStageX = stageX;
  newItem->mStageY = stageY;
  newItem->mGroupID = groupID;
  if (newItem->mNumPoints) {
    newItem->mPtX[0] = stageX;
    newItem->mPtY[0] = stageY;
  }
  mIDsForUndo.push_back(newItem->mMapID);
  mSetOfUndoIDs.insert(newItem->mMapID);
  mIndexesForUndo.push_back((int)itemArray->GetSize());

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
      mWinApp->mShiftManager->ApplyScaleMatrix(mSkipXform, ixSkip[ix] - boxXcen,
        iySkip[ix] - boxYcen, skipXrot, skipYrot, false, false);
      newItem->mSkipHolePos[2 * ix] = (unsigned char)B3DNINT(skipXrot + backXcen);
      newItem->mSkipHolePos[2 * ix + 1] = (unsigned char)B3DNINT(skipYrot + backYcen);
      if (mDebug)
        PrintfToLog("Skip %d %d  cen %.1f %.1f skiprot  %.1f %.1f  cen %.1f %.1f  shp %d"
        " %d", ixSkip[ix], iySkip[ix], boxXcen, boxYcen, skipXrot, skipYrot, backXcen, 
         backYcen, newItem->mSkipHolePos[2 * ix], newItem->mSkipHolePos[2 * ix + 1]);
    }
  }
  itemArray->Add(newItem);
  if (!numAdded)
    item = itemArray->GetAt(itemArray->GetSize() - 1);
  numAdded++;
}

// Clear out the array of saved items
void CMultiHoleCombiner::ClearSavedItemArray(void)
{
  for (int ind = 0; ind < (int)mSavedItems.GetSize(); ind++)
    delete mSavedItems[ind];
  mSavedItems.RemoveAll();
}

