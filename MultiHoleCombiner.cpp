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
  ERR_NO_XFORM, ERR_LAST_ENTRY
};

const char *sMessages[] = {"Navigator is not open or item array not accessible",
"There is no current image", "The current item is not a polygon",
"The current item is not a point or has no Group ID",
"There is no current item selected in the Navigator table",
"There are too few points to combine",  "Error allocating memory",
"Custom holes are selected in the Multiple Record parameters",
"The Multiple Record parameters do not specify to use multiple holes",
"The Multiple Record parameters do not have a regular hole geometry defined", 
"No transformation from image to stage shift is available", ""};

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
int CMultiHoleCombiner::CombineItems(int boundType)
{
  CMapDrawItem *item, *curItem;
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
  EMimageBuffer *imBuf;
  KImage *image;
  FloatVec xCenters, yCenters, peakVals, xCenAlt, yCenAlt, peakAlt, xMissing, yMissing;
  IntVec navInds, altInds, boxAssigns, ixSkip, iySkip;
  ShortVec gridX, gridY;
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  MontParam *montp;
  int *activeList;
  LowDoseParams *ldp;
  ScaleMat s2c, is2cam, is2st;
  PositionData data;
  CArray<PositionData, PositionData> fullArray, bestFullArray;
  float avgAngle, spacing, gridXdX, gridXdY, gridYdX, gridYdY;
  int ind, nx, ny, numPoints, ix, iy;
  int rowStart, colStart, num, minFullNum = 10000000;
  float fDTOR = (float)DTOR;
  float fullSd, fullBestSd, ptX, ptY, gridAngle, holeAngle, angleDiff;
  bool crossPattern;
  int *mapVals;
  int camera = mWinApp->GetCurrentCamera();
  int yStart, xStart, xDir, magInd;
  int point, acqXstart, acqXend, acqYstart, acqYend, nxBox, nyBox, numInBox, bx, by;
  float stageX, stageY, boxXcen, boxYcen, dx, dy, vecn, vecm, holeXdX, holeXdY;
  int ori, crossDx[5] = {0, -1, 1, 0, 0}, crossDy[5] = {0, 0, 0, -1, 1};

  // Check for Nav
  mHelper = mWinApp->mNavHelper;
  mFindHoles = mHelper->mFindHoles;
  mNav = mWinApp->mNavigator;
  if (!mNav)
    return ERR_NO_NAV;
  itemArray = mNav->GetItemArray();
  if (!itemArray)
    return ERR_NO_NAV;

  // Make sure a regular multi-hole is defined
  if (mHelper->mMultiShotDlg)
    mHelper->mMultiShotDlg->UpdateAndUseMSparams();
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
  is2st = MatMul(is2cam, MatInv(s2c));

  // Find points that are on image
  if (boundType == COMBINE_ON_IMAGE) {
    imBuf = mWinApp->mActiveView->GetActiveImBuf();
    image = imBuf->mImage;
    if (!image)
      return ERR_NO_IMAGE;
    image->getSize(nx, ny);
    itemArray = mWinApp->mMainView->GetMapItemsForImageCoords(imBuf, true);
    if (!itemArray)
      return ERR_NO_NAV;
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire) {
        mWinApp->mMainView->GetItemImageCoords(imBuf, item, ptX, ptY);
        if (ptX >= 0. && ptX <= nx && ptY >= 0. && ptY <= ny) {
          xCenters.push_back(item->mStageX);
          yCenters.push_back(item->mStageY);
          navInds.push_back(ind);
        }
      }
    }

  } else if (boundType == COMBINE_IN_POLYGON) {

    // OR inside polygon
    if (curItem->mType != ITEM_TYPE_POLYGON)
      return ERR_NOT_POLYGON;

    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire) {
        if (item->mType == ITEM_TYPE_POINT && InsideContour(curItem->mPtX, curItem->mPtY,
          curItem->mNumPoints, item->mStageX, item->mStageY)) {
          xCenters.push_back(item->mStageX);
          yCenters.push_back(item->mStageY);
          navInds.push_back(ind);
        }
      }
    }
  } else {

    // Get group members
    if (curItem->mType != ITEM_TYPE_POINT || !curItem->mGroupID)
      return ERR_NO_GROUP;
    for (ind = 0; ind < itemArray->GetSize(); ind++) {
      item = itemArray->GetAt(ind);
      if (item->mAcquire) {
        if (item->mType == ITEM_TYPE_POINT && item->mGroupID == curItem->mGroupID) {
          xCenters.push_back(item->mStageX);
          yCenters.push_back(item->mStageY);
          navInds.push_back(ind);
        }
      }
    }
  }

  // Insist on at least 4 points
  numPoints = (int)navInds.size();
  if (numPoints < 4)
    return ERR_TOO_FEW_POINTS;

  // Just get average angle and length from the analysis, then get grid positions
  mFindHoles->analyzeNeighbors(xCenters, yCenters, peakVals, altInds, xCenAlt, yCenAlt, 
    peakAlt, 0., 0., 0, spacing, xMissing, yMissing);
  avgAngle = -999.;
  mFindHoles->assignGridPositions(xCenters, yCenters, gridX, gridY, avgAngle, spacing);
  mFindHoles->getGridVectors(gridXdX, gridXdY, gridYdX, gridYdY);

  // Convert hole image shift vectors to stage vectors and get rotation from the grid 
  // vectors to these vectors
  mWinApp->mShiftManager->ApplyScaleMatrix(is2st, (float)msParams->holeISXspacing[0],
    (float)msParams->holeISYspacing[0], holeXdX, holeXdY);
  gridAngle = atan2f(gridXdY, gridXdX) / fDTOR;
  holeAngle = atan2f(holeXdY, holeXdX) / fDTOR;
  angleDiff = (float)UtilGoodAngle(holeAngle - gridAngle);
  angleDiff = 90.f * B3DNINT(angleDiff / 90.f);

  // If angle is near 90, then the number of holes in stage coordinates is transposed from
  // the specified geometry.  Holes will be analyzed in stage coordinates, but the hole
  // geometry has to be back in image shift space when making Nav items
  mTransposeSize = fabs(fabs(angleDiff) - 90.) < 1.;
  if (mTransposeSize)
    B3DSWAP(mNumXholes, mNumYholes, ix);

  // Sorry, it was empirically found that back-rotation by this angle is what was
  // needed to rotate the skip points into the right positions
  mSkipXform.xpx = mSkipXform.ypy = cosf(fDTOR * angleDiff);
  mSkipXform.xpy = sinf(fDTOR * angleDiff);
  mSkipXform.ypx = -mSkipXform.xpy;

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

          // Set up to find the included and skipped items
          nxBox = acqXend + 1 - acqXstart;
          nyBox = acqYend + 1 - acqYstart;
          boxXcen = (float)((nxBox - 1.) / 2.);
          boxYcen = (float)((nyBox - 1.) / 2.);
          ixSkip.clear();
          iySkip.clear();
          stageX = stageY = 0.;
          numInBox = 0;
          /*PrintfToLog("Point %d box %d x %d - %d y %d - %d", navInds[point], ind, 
            acqXstart, acqXend, acqYstart, acqYend);*/

          // Loop on the positions in the (expanded) box 
          for (iy = acqYstart; iy <= acqYend; iy++) {
            for (ix = acqXstart; ix <= acqXend; ix++) {
              bx = ix - acqXstart;
              by = iy - acqYstart;

              // if there is a point within the original box set it as assigned to this
              // box and add in its adjusted stage position
              if (ix >= 0 && ix < mNxGrid && iy >= 0 && iy < mNyGrid &&
                ix >= bestFullArray[ind].startX && ix <= bestFullArray[ind].endX &&
                iy >= bestFullArray[ind].startY && iy <= bestFullArray[ind].endY &&
                mGrid[iy][ix] >= 0) {
                boxAssigns[mGrid[iy][ix]] = ind;
                numInBox++;
                item = itemArray->GetAt(navInds[mGrid[iy][ix]]);
                stageX += item->mStageX +
                  (float)((boxXcen - bx) * gridXdX + (boxYcen - by) * gridYdX);
                stageY += item->mStageY +
                  (float)((boxXcen - bx) * gridXdY + (boxYcen - by) * gridYdY);
              } else {

                // Otherwise add to skip list
                ixSkip.push_back(bx);
                iySkip.push_back(by);
                //PrintfToLog("missing %d %d");
              }
            }
          }

          // Make a new Navigator item as clone of the first point found
          AddMultiItemToArray(itemArray, navInds[point], stageX / numInBox, 
            stageY / numInBox, nxBox, nyBox, boxXcen, boxYcen, ixSkip, iySkip);
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
        for (iy = -1; iy <= mNyGrid; iy++) {
          for (ix = -1; ix <= mNyGrid; ix++) {

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
          //PrintfToLog("Point %d x/y %d %d  box %d  %d %d", navInds[point], ix, iy, ind,
          //bestFullArray[ind].startX, bestFullArray[ind].startY);

          // Set up to find the included and skipped items
          ixSkip.clear();
          iySkip.clear();
          stageX = stageY = 0.;
          numInBox = 0;
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
              stageX += item->mStageX - (float)(bx * gridXdX + by * gridYdX);
              stageY += item->mStageY - (float)(bx * gridXdY + by * gridYdY);
            } else {
              ixSkip.push_back(1 + bx);
              iySkip.push_back(1 + by);
              //PrintfToLog("missing %d %d %d %d", ix, iy, 1 + bx, 1 + by);
            }
          }

          AddMultiItemToArray(itemArray, navInds[point], stageX / numInBox,
            stageY / numInBox, -3, -3, 1.f, 1.f, ixSkip, iySkip);
          break;
        }
      }
    }
  }

  // Now need to save the items and remove the items from the array.  
  // Check for errors and eliminate from index list
  for (ind = numPoints - 1; ind >= 0; ind--) {
    if (boxAssigns[ind] < 0) {
      PrintfToLog("Program error: item # %d not assigned to a multi-shot item", 
        navInds[ind] + 1);
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
  mNav->FinishMultipleDeletion();

  free(mapVals);
  free(mGrid);
  return 0;
}

/*
 * External call to determine if undo is possible: Nav array must be same size and all the
 * IDs of what was made must still exist
 */
bool CMultiHoleCombiner::OKtoUndoCombine(void)
{
  int jnd;
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
  mNav = mWinApp->mNavigator;

  if (!mNav || !mSavedItems.GetSize() || !mIDsForUndo.size())
    return false;
  itemArray = mNav->GetItemArray();
  if (!itemArray)
    return false;
  for (jnd = 0; jnd < (int)mIDsForUndo.size(); jnd++) {
    if (!mNav->FindItemWithMapID(mIDsForUndo[jnd], false))
      return false;
  }
  return true;
}

/*
* External call to undo it: remove the multi shot ones by ID and restore the saved items and remove the 
*/
void CMultiHoleCombiner::UndoCombination(void)
{
  int jnd;
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray;
  CMapDrawItem *item;
  mNav = mWinApp->mNavigator;

  if (!mNav || !mSavedItems.GetSize() || !mIDsForUndo.size())
    return;
  itemArray = mNav->GetItemArray();
  for (jnd = 0; jnd < (int)mIDsForUndo.size(); jnd++) {
    item = mNav->FindItemWithMapID(mIDsForUndo[jnd], false);
    if (item) {
      itemArray->RemoveAt(mNav->GetFoundItem());
      delete item;
    }
  }

  itemArray->Append(mSavedItems);
  mSavedItems.RemoveAll();
  mIDsForUndo.clear();
  mNav->FinishMultipleDeletion();
}

// For one line (a row or a column) find the best arrangement of boxes along that line
void CMultiHoleCombiner::TryBoxStartsOnLine(int otherStart, bool doCol, 
  CArray<PositionData, PositionData> &fullArray)
{
  CArray<PositionData, PositionData> bestLineArray, lineArray;
  int num, minNum = 10000000;
  float sdOfLine, sdAtBest;
  int vStart = 1 - (doCol ? mNumYholes : mNumXholes);

  // Try possible starts for the line of boxes at the otherStart
  for (; vStart <= 0; vStart++) {
    EvaluateLineOfBoxes(doCol ? otherStart : vStart, doCol ? vStart : otherStart, doCol,
      lineArray, sdOfLine);
    num = (int)lineArray.GetSize();
    if (num < minNum || (num == minNum && sdOfLine < sdAtBest)) {
      minNum = num;
      sdAtBest = sdOfLine;
      bestLineArray.Copy(lineArray);
    }
  }

  // Add best one to the full array
  fullArray.Append(bestLineArray);
}

// For a line of boxes at one start position, get the boxes and compute SD of # in each
void CMultiHoleCombiner::EvaluateLineOfBoxes(int xStart, int yStart, bool doCol,
  CArray<PositionData, PositionData> &posArray, float &sdOfNums)
{
  PositionData data;
  FloatVec xnums;
  float avg, sem;
  int vStart = doCol ? yStart : xStart;

  posArray.RemoveAll();
  for (vStart; vStart < (doCol ? mNyGrid : mNxGrid); 
    vStart += (doCol ? mNumYholes : mNumXholes)) {
    EvaluateBoxAtPosition(doCol ? xStart : vStart, doCol ? vStart : yStart, data);
    if (data.numAcquires > 0) {
      xnums.push_back((float)data.numAcquires);
      posArray.Add(data);
    }
  }
  sdOfNums = 0;
  if (xnums.size() > 1)
    avgSD(&xnums[0], (int)xnums.size(), &avg, &sdOfNums, &sem);
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
  IntVec &ixSkip, IntVec &iySkip)
{
  CMapDrawItem *newItem, *item;
  int ix;
  float skipXrot, skipYrot;
  float backXcen = mTransposeSize ? boxYcen : boxXcen;
  float backYcen = mTransposeSize ? boxXcen : boxYcen;

  // Set the stage position and # of holes and put in the skip list
  item = itemArray->GetAt(baseInd);
  newItem = item->Duplicate();
  newItem->mStageX = stageX;
  newItem->mStageY = stageY;
  if (newItem->mNumPoints) {
    newItem->mPtX[0] = stageX;
    newItem->mPtY[0] = stageY;
  }
  mIDsForUndo.push_back(newItem->mMapID);

  // The number of holes is still from the stage-space analysis and has to be transposed
  // for the item if IS-space is rotated +/-90 from that.  Also the incoming box centers
  // to subtract for the transform are in stage space, but need to add back on transposed
  // centers (above) to get to IS space values
  if (mTransposeSize)
    B3DSWAP(numXholes, numYholes, ix);
  newItem->mNumXholes = numXholes;
  newItem->mNumYholes = numYholes;
  newItem->mNumSkipHoles = (int)ixSkip.size();
  delete newItem->mSkipHolePos;
  newItem->mSkipHolePos = 0;
  if (ixSkip.size()) {
    newItem->mSkipHolePos = new unsigned char[2 * newItem->mNumSkipHoles];
    for (ix = 0; ix < newItem->mNumSkipHoles; ix++) {
      mWinApp->mShiftManager->ApplyScaleMatrix(mSkipXform, ixSkip[ix] - boxXcen,
        iySkip[ix] - boxYcen, skipXrot, skipYrot);
      newItem->mSkipHolePos[2 * ix] = (unsigned char)B3DNINT(skipXrot + backXcen);
      newItem->mSkipHolePos[2 * ix + 1] = (unsigned char)B3DNINT(skipYrot + backYcen);
    }
  }

  itemArray->Add(newItem);
}

// Clear out the arry of saved items
void CMultiHoleCombiner::ClearSavedItemArray(void)
{
  for (int ind = 0; ind < (int)mSavedItems.GetSize(); ind++)
    delete mSavedItems[ind];
  mSavedItems.RemoveAll();
}

