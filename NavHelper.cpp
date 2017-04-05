// NavHelper.cpp : Helper class for Navigator for task-oriented routines
//
//
// Copyright (C) 2007 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMView.h"
#include ".\NavHelper.h"
#include "SerialEMDoc.h"
#include "CameraController.h"
#include "EMmontageController.h"
#include "EMscope.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "NavigatorDlg.h"
#include "MapDrawItem.h"
#include "NavFileTypeDlg.h"
#include "NavRotAlignDlg.h"
#include "MontageSetupDlg.h"
#include "BeamAssessor.h"
#include "StateDlg.h"
#include "TiltSeriesParam.h"
#include "TSController.h"
#include "MultiTSTasks.h"
#include "Utilities\XCorr.h"
#include "Image\KStoreADOC.h"
#include "Shared\autodoc.h"

enum HelperTasks {TASK_FIRST_MOVE = 1, TASK_FIRST_SHOT, TASK_SECOND_MOVE, 
TASK_SECOND_SHOT, TASK_FINAL_SHOT};

CNavHelper::CNavHelper(void)
{
  SEMBuildTime(__DATE__, __TIME__);
	mWinApp = (CSerialEMApp *)AfxGetApp();
  mDocWnd = mWinApp->mDocWnd;
	mMagTab = mWinApp->GetMagTable();
	mCamParams = mWinApp->GetCamParams();
	mImBufs = mWinApp->GetImBufs();
	mMontParam = mWinApp->GetMontParam();
	mShiftManager = mWinApp->mShiftManager;
  mBufferManager = mWinApp->mBufferManager;
  mRealigning = 0;
  mAcquiringDual = false;
  mMaxMarginNeeded = 10.;
  mMinMarginWanted = 5.;
  mMinMarginNeeded = 1.5f;
  mRImaxLMfield = 15.;
  mRegistrationAtErr = -1;
  mStageErrX = mStageErrY = -999.;
  mRImapID = 0;
  mRIskipCenTimeCrit = 15;
  mRIskipCenErrorCrit = 0.;
  mRIabsTargetX = mRIabsTargetY = 0.;
  mRImaximumIS = 1.;
  mDistWeightThresh = 5.f;
  mRIweightSigma = 3.f;
  mTypeOfSavedState = STATE_NONE;
  mLastTypeWasMont = false;
  mLastMontFitToPoly = false;
  for (int i = 0; i < 4; i++) {
    mGridLimits[i] = 0.;
    mTestParams[i] = 0.;
  }
  mStateArray.SetSize(0, 4);
  mStateDlg = NULL;
  mStatePlacement.rcNormalPosition.right = 0;
  mRotAlignDlg = NULL;
  mRotAlignCenter = 0.f;
  mRotAlignRange = 20.f;
  mSearchRotAlign = true;
  mRotAlignPlace.rcNormalPosition.right = 0;
  mRIdefocusOffsetSet = 0.;
  mRIbeamShiftSetX = mRIbeamShiftSetY = 0.;
  mRIbeamTiltSetX = mRIbeamTiltSetY = 0.;
  mRIalphaSet = -999;
  mRIuseBeamOffsets = false;
  mRITiltTolerance = 2.;
  mContinuousRealign = 0;
  mGridGroupSize = 5.;
  mDivideIntoGroups = false;
  mEditReminderPrinted = false;
  mCollapseGroups = false;
  mRIstayingInLD = false;
  mNav = NULL;
}

CNavHelper::~CNavHelper(void)
{
  ClearStateArray();
}


void CNavHelper::ClearStateArray(void)
{
  int i;
  StateParams *param;
  for (i = 0; i < mStateArray.GetSize(); i++) {
    param = mStateArray.GetAt(i);
    delete param;
  }
  mStateArray.RemoveAll();
}

void CNavHelper::Initialize(void)
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
}

// Initialization when navigator is opened
void CNavHelper::NavOpeningOrClosing(bool open)
{
  if (open) {
    mNav = mWinApp->mNavigator;
    mItemArray = mNav->GetItemArray();
    mTSparamArray = mNav->GetTSparamArray();
    mFileOptArray = mNav->GetFileOptArray();
    mMontParArray = mNav->GetMontParArray();
    mGroupFiles = mNav->GetScheduledFiles();
    mAcqStateArray = mNav->GetAcqStateArray();
  } else
    mNav = NULL;
}

/////////////////////////////////////////////////
/////  REALIGN TO ITEM
/////////////////////////////////////////////////

// Finds the best map for realigning to the given item and sets all the member variables
// for the realign operation
int CNavHelper::FindMapForRealigning(CMapDrawItem * inItem, BOOL restoreState)
{
  int mapInd, magMax, ix, iy, ixPiece, iyPiece, ixSafer, iySafer, xFrame, yFrame, binning; 
  CMapDrawItem *item;
  int maxAligned, alignedMap, maxDrawn, drawnMap, samePosMap, maxSamePos;
  float toEdge, netViewShiftX, netViewShiftY;
  float incLowX, incLowY, incHighX, incHighY, targetX, targetY, borderDist, borderMax;
  double firstISX, firstISY;
  BOOL betterMap, differentMap, stayInLD;
  ScaleMat aMat, bMat;
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  float delX, delY, distMin, distMax, firstDelX, firstDelY;
  int magIndex = mScope->GetMagIndex();

  // Look at all maps that point is inside and find one with most distance from a frame 
  // center to the borders, since we will go to the center point for initial localization
  distMax = 0.;
  maxAligned = 0;
  maxDrawn = 0;
  maxSamePos = 0;
  borderMax = 0.;
  for (mapInd = 0; mapInd < mItemArray->GetSize(); mapInd++) {
    item = mItemArray->GetAt(mapInd);
    if (item->mType != ITEM_TYPE_MAP || item->mRegistration != inItem->mRegistration ||
      item->mImported || item->mColor == NO_REALIGN_COLOR)
      continue;

    // Allow low mag mode if the camera field of view is smaller than the limit, which is
    // determined by objective aperture size
    if (item->mMapMagInd < mScope->GetLowestNonLMmag(&mCamParams[item->mMapCamera]) &&
      mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd) *
      B3DMAX(mCamParams[item->mMapCamera].sizeX, mCamParams[item->mMapCamera].sizeY) >
      mRImaxLMfield)
      continue;

    // Get the target which we need to go to at this mag - this will be different from
    // absolute target if the user does not have apply offsets on and it is clear that
    // we will end up at another mag from this item
    targetX = inItem->mStageX;
    targetY = inItem->mStageY;

    // Accumulate the net stage shift needed to work at this mag relative to the final one
    firstDelX = firstDelY = 0.;
    alignedMap = 0;
    differentMap = inItem->mType == ITEM_TYPE_MAP && inItem->mMapID != item->mMapID;
    incHighX = incHighY = incLowX = incLowY = 0.;
    if (differentMap || ((restoreState || mWinApp->LowDoseMode()) && 
      !(mWinApp->LowDoseMode() && 
      (item->mNetViewShiftX != 0 || item->mNetViewShiftY != 0)))) {

      // Get potential adjustment for mag change if either the input item is a different
      // map from the current item or we will be restoring state at the end or we are in
      // low dose
      mNav->ConvertIStoStageIncrement(item->mMapMagInd, item->mMapCamera, 0., 0.,
        item->mMapTiltAngle, incLowX, incLowY);
      if (differentMap)
        mNav->ConvertIStoStageIncrement(inItem->mMapMagInd, inItem->mMapCamera, 0., 0.,
          item->mMapTiltAngle, incHighX, incHighY);
      else {
        ix = magIndex;

        // if in low dose, align at the mag of the area we are going back to since that
        // constitutes the restored state
        if (mWinApp->LowDoseMode()) {
          iy = mNav->TargetLDAreaForRealign();
          if (iy >= 0 && ldp[iy].magIndex)
            ix = ldp[iy].magIndex;
        }
        mNav->ConvertIStoStageIncrement(ix, mWinApp->GetCurrentCamera(), 0., 0.,
          item->mMapTiltAngle, incHighX, incHighY);
      }

      firstDelX += incHighX - incLowX;
      firstDelY += incHighY - incLowY;
    }

    // And if there is a recorded error when aligning to this map, set flag
    // Also subtract from target to get back to the original stage position that was
    // aligned to in map and presumably marked.
    if (inItem->mType == ITEM_TYPE_MAP && inItem->mRealignedID == item->mMapID && 
      inItem->mRealignReg == item->mRegistration) {
      targetX -= inItem->mRealignErrX;
      targetY -= inItem->mRealignErrY;
      alignedMap = 1;
    }
    drawnMap = inItem->mDrawnOnMapID == item->mMapID ? 1 : 0;
    samePosMap = differentMap && inItem->mAtSamePosID > 0 && 
      inItem->mAtSamePosID == item->mAtSamePosID ? 1 : 0;

    if (InsideContour(item->mPtX, item->mPtY, item->mNumPoints, targetX, targetY)) {

      // Get minimum distance to the border
      borderDist = 100000000.;
      for (ix = 0; ix < item->mNumPoints - 1; ix++) {
        toEdge = PointSegmentDistance(targetX, targetY, item->mPtX[ix], item->mPtY[ix], 
          item->mPtX[ix+1], item->mPtY[ix+1]);
        borderDist = B3DMIN(borderDist, toEdge);
      }

      // Find piece that target is closest to center of, and possibly safer piece that is
      // farther from edge of a montage; distMin is minimum distance from center of that
      // piece to edge of map
      if (DistAndPiecesForRealign(item, targetX, targetY, inItem->mPieceDrawnOn, mapInd,
        aMat, delX, delY, distMin, ixPiece, iyPiece, ixSafer, iySafer, ix, iy, false, 
        xFrame, yFrame, binning))
        continue;

      // Now can determine net view shift and adjust initial stage delta
      stayInLD = CanStayInLowDose(item, xFrame, yFrame, binning, ix, netViewShiftX, 
        netViewShiftY, false);

      // If realigning to a second map at record mag, need to adjust the target by the
      // calibration shift unless staying in low dose, in which case need to clear out
      // the initial image shift.  Sadly, this is all empirical.
      if (differentMap && ((mWinApp->LowDoseMode() || item->mNetViewShiftX || 
        item->mNetViewShiftY) && inItem->mMapMagInd == ldp[RECORD_CONSET].magIndex)) {
          if (stayInLD && (item->mNetViewShiftX || item->mNetViewShiftY)) {
            firstDelX = 0.;
            firstDelY = 0.;
          } else if (!stayInLD) {
            targetX += mRIcalShiftX;
            targetY += mRIcalShiftY;
          }
      }
      firstDelX += netViewShiftX;
      firstDelY += netViewShiftY;
      SEMTrace('h', "net shift %.2f %.2f  target %.2f %.2f firstDel %.2f %.2f", 
        netViewShiftX, netViewShiftY, targetX, targetY, firstDelX, firstDelY);

      // Convert the net stage shift to an image shift to apply instead
      bMat = MatMul(ItemStageToCamera(item),
        MatInv(mShiftManager->IStoGivenCamera(item->mMapMagInd, item->mMapCamera)));
      firstISX = firstDelX * bMat.xpx + firstDelY * bMat.xpy;
      firstISY = firstDelX * bMat.ypx + firstDelY * bMat.ypy;
 
      // Treat any edge distance larger than this margin equally
      distMin = B3DMIN(distMin, mMaxMarginNeeded);

      // A map is "better" if has a bigger edge distance, or if it is at a higher mag 
      // for the same effective distance, or if the target is more interior for same
      // effective distance
      betterMap = distMin > distMax || (distMin == distMax && magMax < item->mMapMagInd)
        || (distMin == distMax && borderDist > borderMax);

      // Same pos map trumps all: so if this is a same pos map and previous wasn't, accept
      // it if it is better or at least good enough
      // Then apply all the other conditions only if either previous map wasn't at same
      // pos or it was not quite good enough.  In this case:
      // If both or neither are aligned, take the better map
      // If new one is aligned and old one isn't, take it if it is beyond wanted dist
      // or better.  If new one is better and not aligned, take it unless old one is 
      // aligned and beyond wanted distance or if the target is more interior
      // Treat drawn on map same way as aligned to map - maybe it should count for more
      // but there is no criterion for "enough" border to apply here
      if ((samePosMap && !maxSamePos && (betterMap || distMin >= mMinMarginWanted)) ||
        ((!maxSamePos || distMin < mMinMarginWanted) &&
        (((alignedMap == maxAligned || drawnMap == maxDrawn) && betterMap) || 
        (((alignedMap && !maxAligned) || (drawnMap && !maxDrawn)) && 
        (betterMap || distMin >= mMinMarginWanted)) ||
        (betterMap && (!(((!alignedMap && maxAligned) || (!drawnMap && maxDrawn)) && 
        distMax >= mMinMarginWanted) ||
        (distMin == distMax && borderDist > borderMax)))))) {
          distMax = distMin;
          borderMax = borderDist;
          maxAligned = alignedMap;
          maxDrawn = drawnMap;
          maxSamePos = samePosMap;
          mRIitemInd = mapInd;
          magMax = item->mMapMagInd;
          mRImat = aMat;
          mRIdelX = delX;
          mRIdelY = delY;
          mRIix = ixPiece;
          mRIiy = iyPiece;
          mRIixSafer = ixSafer;
          mRIiySafer = iySafer;
          mRItargetX = targetX;
          mRItargetY = targetY;
          mRIfirstISX = firstISX;
          mRIfirstISY = firstISY;
      }
    }
  }
  
  if (distMax < mMinMarginNeeded)
    return distMax ? 5 : 4;

  return 0;
}

// Find distance to edge of map; for a montage, find the best piece for initial and
// target alignments
int CNavHelper::DistAndPiecesForRealign(CMapDrawItem *item, float targetX, float targetY,
  int pcDrawnOn, int mapInd, ScaleMat &aMat, float &delX, float &delY, float &distMin, 
  int &ixPiece, int &iyPiece, int &ixSafer, int &iySafer, int &imageX, int &imageY,
  bool keepStore, int &xFrame, int &yFrame, int &binning)
{
  KImageStore *imageStore;
  int curStore, ix, iy, xst, yst, imX, imY, idx, idy, ind, midDist;
  int xFrameDel, yFrameDel, finalMidMin, saferMidMin;
  float useWidth, useHeight, loadWidth, loadHeight, edgeDist, finalEdgeDist;
  float saferEdgeMin;
  MontParam *montP;
  float pixel, halfX, halfY;
  bool drawnOnPiece, finalDrawnOn = false, saferDrawnOn = false;

  // Get dimensions.  For montage, get pointers to file so that frame sizes,
  // etc are available
  if (item->mMapMontage) {
    montP = &mMapMontParam;
    if (mNav->AccessMapFile(item, imageStore, curStore, montP, useWidth, useHeight))
      return 1;
    xFrameDel = (montP->xFrame - montP->xOverlap);
    loadWidth = (float)(montP->xFrame + (montP->xNframes - 1) * xFrameDel);
    yFrameDel =  (montP->yFrame - montP->yOverlap);
    loadHeight = (float)(montP->yFrame + (montP->yNframes - 1) * yFrameDel);
    mWinApp->mMontageController->ListMontagePieces(imageStore, montP, 
      item->mMapSection, mPieceSavedAt);
    xFrame = montP->xFrame;
    yFrame = montP->yFrame;
    if (keepStore) {
      mMapMontP = montP;
      mMapStore = imageStore;
      mCurStoreInd = curStore;
    } else {
      if (curStore < 0)
        delete imageStore;
    }
    binning = item->mMontBinning ? item->mMontBinning : montP->binning;
  } else {
    xFrame = item->mMapWidth;
    yFrame = item->mMapHeight;
    loadWidth = useWidth = (float)item->mMapWidth;
    loadHeight = useHeight = (float)item->mMapHeight;
    binning = item->mMapBinning;
  }

  // Get transform to the unrotated full-sized image
  aMat = item->mMapScaleMat;
  aMat.xpx *= loadWidth / useWidth;
  aMat.xpy *= loadWidth / useWidth;
  aMat.ypx *= loadHeight / useHeight;
  aMat.ypy *= loadHeight / useHeight;
  delX = (item->mMapWidth / useWidth) * loadWidth / 2.f - 
    aMat.xpx * item->mStageX - aMat.xpy * item->mStageY;
  delY = (2.f - item->mMapHeight / useHeight) * loadHeight / 2.f - 
    aMat.ypx * item->mStageX - aMat.ypy * item->mStageY;
  pixel = mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd) * 
    binning;

  // For single frame, distance is just half the minimum frame size
  if (!item->mMapMontage) {
    halfX = 0.5f * pixel * loadWidth;
    halfY = 0.5f * pixel * loadHeight;
    distMin = B3DMIN(halfX, halfY);
  } else {

    // For montage, need to figure out the frame that the point is in.  Do this in
    // right-handed coordinates.  Find piece with center closest to point but far
    // enough away from edge
    imX = (int)(aMat.xpx * targetX + aMat.xpy * targetY + delX);
    imY = (int)(loadHeight - (aMat.ypx * targetX + 
      aMat.ypy * targetY + delY));
    if (imX < 0 || imY < 0 || imX > loadWidth || imY > loadHeight)
      return 2;

    finalMidMin = 1000000000;
    saferMidMin = 1000000000;
    saferEdgeMin = -1.;
    for (ix = 0; ix < montP->xNframes; ix++) {
      for (iy = 0; iy < montP->yNframes; iy++) {
        xst = ix * xFrameDel;
        yst = iy * yFrameDel;
        ind = iy + ix * montP->yNframes;
        if (mPieceSavedAt[ind] >= 0) {
          idx = imX - (xst + montP->xFrame / 2);
          idy = imY - (yst + montP->yFrame / 2);

          // Get distance of center from target, and minimum distance to edge
          midDist = idx * idx + idy * idy;
          edgeDist = PieceToEdgeDistance(montP, ix, iy) * pixel;
          drawnOnPiece = ix * montP->yNframes + iy == pcDrawnOn;

          // Take this as a new safer piece either if we haven't gotten far enough
          // from the edge yet and this piece is farther from edge; or if this
          // piece is either beyond the desired margin or as good as the last piece
          // and it is closer to the target than the last piece, as long as the last piece
          // wasn't the one drawn on; or if the edge distance is adequate and this is
          // the piece drawn on
          if ((saferEdgeMin < mMinMarginWanted && edgeDist > saferEdgeMin) ||
            ((edgeDist >= mMinMarginWanted || edgeDist == saferEdgeMin) &&
            midDist < saferMidMin && !saferDrawnOn) ||
            (edgeDist >= mMinMarginWanted && drawnOnPiece)) {
              saferEdgeMin = edgeDist;
              saferMidMin = midDist;
              saferDrawnOn = edgeDist >= mMinMarginWanted && drawnOnPiece;
              ixSafer = ix;
              iySafer = iy;
          }

          if ((imX >= xst && imX <= xst + montP->xFrame &&
            imY >= yst && imY <= yst + montP->yFrame) || drawnOnPiece) {

              // Take this as a new final piece if point is in it and it is
              // closer to the target, or if it is the piece drawn on
              if ((midDist < finalMidMin && !finalDrawnOn) || drawnOnPiece) {
                finalMidMin = midDist;
                finalDrawnOn = drawnOnPiece;
                ixPiece = ix;
                iyPiece = iy;
                finalEdgeDist = edgeDist;
                imageX = imX;
                imageY = imY;
              }
          }
        }
      }
    }

    // Skip if no pieces found containing point
    if (finalMidMin == 1000000000)
      return 2;

    // If the final piece is good enough or somehow no safer piece was identified,
    // use the final one as the safer one
    SEMTrace('h', "Map %d : saferMidMin %.2f  saferEdgeMin %.2f  safer ix,iy %d"
      " %d", mapInd, pixel * sqrt((double)saferMidMin), saferEdgeMin, ixSafer, iySafer);
    SEMTrace('h', "  finalMidMin %.2f drawnOn %s finalEdgeDist %.2f  final ix,iy %d"
      " %d", pixel * sqrt((double)finalMidMin), finalDrawnOn ? "Y" : "N", finalEdgeDist,
      ixPiece, iyPiece);
    if (finalEdgeDist >= mMinMarginWanted || saferEdgeMin < 0) {
      ixSafer = ixPiece;
      iySafer = iyPiece;
      saferEdgeMin = finalEdgeDist;
    }
    distMin = saferEdgeMin;
  }
  return 0;
}


// Realign to the coordinates in the given item by correlating with maps,
int CNavHelper::RealignToItem(CMapDrawItem *inItem, BOOL restoreState)
{
  int i, ix, iy, ind, axes, action; 
  CMapDrawItem *item;
  KImageStore *imageStore;
  float useWidth, useHeight, montErrX, montErrY, itemBackX, itemBackY;
  MontParam *montP;
  ScaleMat aMat;
  NavParams *navParams = mWinApp->GetNavParams();
  CenterSkipData cenSkip;
  float finalX, finalY, stageX, stageY, firstDelX, firstDelY, mapAngle, angle;
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  i = FindMapForRealigning(inItem, restoreState);
  mRIContinuousMode = mContinuousRealign;
  mContinuousRealign = 0;
  if (i)
    return i;

  // Save the absolute target stage coordinate that gets adjusted on mag-dependent basis
  mRIabsTargetX = inItem->mStageX;
  mRIabsTargetY = inItem->mStageY;

  item = mItemArray->GetAt(mRIitemInd);
  mSecondRoundID = 0;
  if (inItem->mType == ITEM_TYPE_MAP && item->mMapID != inItem->mMapID)
    mSecondRoundID = inItem->mMapID;
  SEMTrace('1', "(Initial) alignment to map %d (%s)", mRIitemInd, (LPCTSTR)item->mLabel);

  // Get access to map file again
  mMapMontP = &mMapMontParam;
  i = mNav->AccessMapFile(item, imageStore, mCurStoreInd, mMapMontP, useWidth, useHeight);
  if (i)
    return i;
  montP = mMapMontP;
  mMapStore = imageStore;

  // Get rotation matrix of map images due to transformations only
  aMat = ItemStageToCamera(item);
  mRIrotAngle = mNav->RotationFromStageMatrices(aMat, mRImat, mRIinverted);
  mRIrMat = mNav->GetRotationMatrix(mRIrotAngle, mRIinverted);

  // Set up to get target montage error in adjustment coming next
  montErrX = montErrY = 0.;
  mUseMontStageError = false;
  if (item->mMapMontage) {
    mUseMontStageError = (mWinApp->GetRealignTestOptions() & 2) != 0 &&
      item->mRegistration == item->mOriginalReg;
    mWinApp->mMontageController->ListMontagePieces(mMapStore, mMapMontP, 
      item->mMapSection, mPieceSavedAt);
  }

  // Set the initial component of previous error so it can now be applied to frame 
  // positions, and adjust the target position to be in same shifted coordinates
  // But previous error only works if we are in the same registration...
  mPreviousErrX = mPreviousErrY = 0.;
  mPrevLocalErrX = mPrevLocalErrY = 0.;
  mLocalErrX = mLocalErrY = 0.;
  if (inItem->mRealignedID == item->mMapID && inItem->mRealignReg == item->mRegistration){
    if (mUseMontStageError)
      InterpMontStageOffset(mMapStore, mMapMontP, ItemStageToCamera(item), mPieceSavedAt, 
        mRItargetX - item->mStageX, mRItargetY - item->mStageY, montErrX, montErrY);
    mPreviousErrX = inItem->mRealignErrX - inItem->mLocalRealiErrX - montErrX;
    mPreviousErrY = inItem->mRealignErrY - inItem->mLocalRealiErrY - montErrY;
    mRItargetX += mPreviousErrX;
    mRItargetY += mPreviousErrY;
    mPrevLocalErrX = inItem->mLocalRealiErrX;
    mPrevLocalErrY = inItem->mLocalRealiErrY;
  }
  SEMTrace('1', "Target X, Y: %.3f %.3f", mRItargetX, mRItargetY);

  montErrX = montErrY = 0.;
  stageX = item->mStageX + mPreviousErrX;
  stageY = item->mStageY + mPreviousErrY;
  if (item->mMapMontage) {

    // Get stage position of final piece
    StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIix, mRIiy, finalX, 
      finalY, montErrX, montErrY);

    // Test for whether a montage center exists in its entirety
    ix = (mMapMontP->xNframes - 1) / 2;
    iy = (mMapMontP->yNframes - 1) / 2;
    ind = iy + ix * mMapMontP->yNframes;
    mUseMontCenter = false;
    if (mPieceSavedAt[ind] >= 0 && 
      ((!(montP->xNframes % 2) && !(montP->yNframes % 2) && mPieceSavedAt[ind + 1] >= 0 &&
      mPieceSavedAt[ind + montP->yNframes] >= 0 && 
      mPieceSavedAt[ind + montP->yNframes + 1] >= 0) ||
      ((montP->xNframes % 2) && !(montP->yNframes % 2) && mPieceSavedAt[ind + 1] >= 0) ||
      (!(montP->xNframes % 2) && (montP->yNframes % 2) && 
      mPieceSavedAt[ind + montP->yNframes] >= 0))) {

        // Then see if the target is closer to the montage center than to piece center
        mUseMontCenter = (pow(finalX - mRItargetX, 2) + pow(finalY - mRItargetY, 2)) >
          (pow(stageX - mRItargetX, 2) + pow(stageY - mRItargetY, 2));
    }

    // TODO: DECIDE WHAT TO DO ABOUT THIS
    mUseMontCenter = false;

    // Get stage position of safer piece
    StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIixSafer, mRIiySafer, 
      stageX, stageY, montErrX, montErrY);
    SEMTrace('1', "First round alignment to piece %d %d at %.2f %.2f, montErr %.2f %.2f",
      mRIixSafer, mRIiySafer, stageX, stageY, montErrX, montErrY);
  }

  // Better make sure stage is ready
  if (mScope->WaitForStageReady(5000)) {
    SEMMessageBox("The stage is busy or not ready trying to realign to an item");
    if (mCurStoreInd < 0)
      delete mMapStore;
    return 6;
  }

  // Set up to use scaling in first if option selected and not low dose
  mRItryScaling = mWinApp->GetTryRealignScaling() && !mWinApp->LowDoseMode();

  // Save state if desired; which will only work by forgetting prior state
  if (restoreState) {
    mTypeOfSavedState = STATE_NONE;
    SaveCurrentState(STATE_MAP_ACQUIRE, false);
  }

  // This sets mRInetViewShiftX/Y, mRIconSetNum, and mRIstayingInLD, and mrIcalShiftX/Y
  // Here is where low dose gets turned off if so
  PrepareToReimageMap(item, mMapMontP, conSet, TRIAL_CONSET, TRUE);
  if (mRIstayingInLD)
    mRIContinuousMode = 0;   // To avoid this, need to set/restore mode in LD conset
  if (mRIContinuousMode) {
    conSet->mode = CONTINUOUS;
    mCamera->ChangePreventToggle(1);
  }
  if (!mRIstayingInLD)
    SetMapOffsetsIfAny(item);
  mNav->BacklashForItem(item, itemBackX, itemBackY);
  mRIfirstStageX = stageX + montErrX;
  mRIfirstStageY = stageY + montErrY;
  mRItargMontErrX = 0.;
  mRItargMontErrY = 0.;
  if (mUseMontStageError)
    InterpMontStageOffset(mMapStore, mMapMontP, ItemStageToCamera(item), mPieceSavedAt, 
      mRItargetX - (item->mStageX + mPreviousErrX), 
      mRItargetY - (item->mStageY + mPreviousErrY), mRItargMontErrX, mRItargMontErrY);

  firstDelX = firstDelY = 0.;
  action = TASK_FIRST_MOVE;

  // If the mapID doesn't match, clear the center skip list; otherwise look up this
  // position in the list, and if time is within limit, skip first round
  if (mRImapID != item->mMapID)
    mCenterSkipArray.RemoveAll();
  mCenSkipIndex = -1;
  for (ind = 0; ind < mCenterSkipArray.GetSize(); ind++) {
    if (mRIfirstStageX == mCenterSkipArray[ind].firstStageX && 
      mRIfirstStageY == mCenterSkipArray[ind].firstStageY && 
      mRIfirstISX == mCenterSkipArray[ind].firstISX && 
      mRIfirstISY == mCenterSkipArray[ind].firstISY) {
        mCenSkipIndex = ind;
        if (mWinApp->MinuteTimeStamp() - mCenterSkipArray[ind].timeStamp <=
          mRIskipCenTimeCrit) {
            firstDelX = mCenterSkipArray[ind].baseDelX + mRItargetX + mRItargMontErrX;
            firstDelY = mCenterSkipArray[ind].baseDelY + mRItargetY + mRItargMontErrY;
            action = TASK_SECOND_MOVE;
            if (LoadForAlignAtTarget(item))
              return 7;
            mWinApp->AppendToLog("Skipping first round alignment to center of map frame");
        }
        break;
    }
  }

  // If there was no match, add this operation to the array with a zero time stamp since
  // it is not completely usable yet
  if (mCenSkipIndex < 0) {
    cenSkip.firstStageX = mRIfirstStageX;
    cenSkip.firstStageY = mRIfirstStageY;
    cenSkip.firstISX = mRIfirstISX;
    cenSkip.firstISY = mRIfirstISY;
    cenSkip.timeStamp = 0;
    mCenterSkipArray.Add(cenSkip);
    mCenSkipIndex = (int)mCenterSkipArray.GetSize() - 1;
  }

  // Go to Z position of item unless this is after doing rough eucentricity in acquire
  axes = axisXY;
  if (!(mNav->GetAcquiring() && navParams->acqRoughEucen))
    axes |= axisZ;

  // Go to map tilt angle or 0 if current angle differs by more than tolerance
  mapAngle = (float)B3DCHOICE(item->mMapTiltAngle > -1000., item->mMapTiltAngle, 0.);
  angle = (float)mScope->GetTiltAngle();
  if (fabs(angle - mapAngle) > B3DMAX(cos(DTOR * angle), 0.5) * mRITiltTolerance)
    axes |= axisA;

  // Adjust the net view shift down by the amount that will be added in AdjustAndMove so
  // that just it will be applied as an offset to the stage position
  mRInetViewShiftX -= mRIcalShiftX;
  mRInetViewShiftY -= mRIcalShiftY;
  SEMTrace('h', "Subtract %.2f %.2f from net shift", mRIcalShiftX, mRIcalShiftY);

  // 9/14/12: We have to use the aligning-to item's Z here to end up at the right Z
  // Add the view shift just to the coordinates being moved to, all the evaluations
  // of error below depend on comparisons to original first stage pos without it
  SEMTrace('h', "first stage %.2f %.2f  net view shift %.2f %.2f  firstdel %.2f %.2f",
    mRIfirstStageX ,mRIfirstStageY, mRInetViewShiftX, mRInetViewShiftY, firstDelX, firstDelY);
  mNav->AdjustAndMoveStage(mRIfirstStageX + mRInetViewShiftX + firstDelX, 
    mRIfirstStageY + mRInetViewShiftY + firstDelY, inItem->mStageZ, axes,
    itemBackX, itemBackY, mRIfirstISX + mRIleaveISX, mRIfirstISY + mRIleaveISY, mapAngle);

  mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_NAV_REALIGN, action, 30000);
  mWinApp->SetStatusText(MEDIUM_PANE, "REALIGNING TO ITEM");
  mCamera->SetRequiredRoll(1);
  mRealigning = 1;
  mRInumRounds = 0;
  mWinApp->UpdateBufferWindows();
  return 0;
}

// Perform the next task in the sequence
void CNavHelper::RealignNextTask(int param)
{
  CMapDrawItem *item = mItemArray->GetAt(mRIitemInd);
  std::vector<BOOL> pieceDone;
  ScaleMat bMat = ItemStageToCamera(item);
  ScaleMat bInv = MatInv(bMat);
  float stageDelX, stageDelY, peakMax, peakVal, shiftX, shiftY, stageX, stageY, scaling;
  float cenMontErrX, cenMontErrY, itemCenX, itemCenY, imagePixelSize;
  float CCC = 0.;
  float bestDelX, bestDelY, pcDelX, pcDelY, pcShiftX, pcShiftY, pcErrX, pcErrY, overlapMax;
  float pcStageX, pcStageY, pcMontErrX, pcMontErrY, wgtPeakMax, wgtPeakVal, overlap;
  double delISX, delISY;
  float samePosCrit = 0.35f;
  float closeCCCcrit = 0.8f;
  double overlapPow = 0.16667;
  bool betterPeak, setShiftInImage;
  int i, ixNew, iyNew, ixCen, iyCen, ix, iy, ind, indBest;
  CString report;
  if (!mRealigning)
    return;
  if (item->mMapMontage)
    pieceDone.resize(mMapMontP->xNframes * mMapMontP->yNframes);
  itemCenX = item->mStageX + mPreviousErrX;
  itemCenY = item->mStageY + mPreviousErrY;
  cenMontErrX = cenMontErrY = 0.;

  switch (param) {
    case TASK_FIRST_MOVE:
      StartRealignCapture(mRIContinuousMode != 0, TASK_FIRST_SHOT);
      return;
  
    case TASK_FIRST_SHOT:
      scaling = 0.;
      if (item->mMapMontage) {
        for (i = 0; i < mMapMontP->xNframes * mMapMontP->yNframes; i++)
          pieceDone[i] = false;
        ixNew = mRIixSafer;
        iyNew = mRIiySafer;
        ixCen = -1;
        wgtPeakMax = peakMax = -1.e20f;
        bestDelX = bestDelY = overlapMax = 0;

        // Evaluate up to 9 pieces around the central piece
        while (ixNew != ixCen || iyNew != iyCen) {
          ixCen = ixNew;
          iyCen = iyNew;
          for (ix = ixCen - 1; ix <= ixCen + 1; ix++) {
            for (iy = iyCen - 1; iy <= iyCen + 1; iy++) {
              if (ix < 0 || ix >= mMapMontP->xNframes || iy < 0 || 
                iy >= mMapMontP->yNframes)
                continue;
              ind = iy + ix * mMapMontP->yNframes;

              // For a valid piece not done yet, read, rotate and align
              if (!pieceDone[ind] && mPieceSavedAt[ind] >= 0) {
                mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[ind], 1, true);
                mImBufs[1].mBinning = item->mMontBinning;
                if (RotateForAligning(1)) {
                  RealignCleanup(1);
                  return;
                }

                // Align without shifting image
                mShiftManager->AutoAlign(1, -1, false, false, &peakVal, 0., 0., 0., 0., 
                  0., &CCC, &overlap, true, &pcShiftX, &pcShiftY);
                pieceDone[ind] = true;

                // Compute the change in position that this correlation implies and
                // disparity from original stage position for this move
                StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, ix, iy, 
                  pcStageX, pcStageY, pcMontErrX, pcMontErrY);
                pcErrX = -mImBufs->mBinning * (bInv.xpx * pcShiftX - bInv.xpy * pcShiftY);
                pcErrY = -mImBufs->mBinning * (bInv.ypx * pcShiftX - bInv.ypy * pcShiftY);
                pcDelX = pcErrX + mPrevLocalErrX + mRItargetX + mRItargMontErrX - 
                  (pcStageX + pcMontErrX);
                pcDelY = pcErrY + mPrevLocalErrY + mRItargetY + mRItargMontErrY - 
                  (pcStageY + pcMontErrY);
                pcErrX += mRIfirstStageX - (pcStageX + pcMontErrX), 
                pcErrY += mRIfirstStageY - (pcStageY + pcMontErrY);
                pcErrX = (float)sqrt(pcErrX * pcErrX + pcErrY * pcErrY);
                wgtPeakVal = (float)(CCC * pow((double)overlap, overlapPow)) *
                  mDistWeightThresh / B3DMAX(mDistWeightThresh, pcErrX);

                // If this position is essentially the same as the best one, take it if
                // the CCC is sufficiently higher, reject if it is sufficiently lower, 
                // and otherwise pick the one with the biggest overlap
                if (fabs((double)(pcDelX - bestDelX)) < samePosCrit && 
                  fabs((double)(pcDelY - bestDelY)) < samePosCrit) {
                  if (peakMax <= 1.e-20 || peakMax / CCC < closeCCCcrit)
                    betterPeak = true;
                  else if (CCC / peakMax < closeCCCcrit)
                    betterPeak = false;
                  else
                    betterPeak = overlap > overlapMax;
                } else {

                  // But for different positions, use the weighted CCC to decide
                  betterPeak = wgtPeakVal > wgtPeakMax;
                }
                SEMTrace('n', "Align to %d %d, peak= %.6g   CCC= %.4f  frac= "
                  "%.2f  delXY= %.2f, %.2f  err= %.2f  wgtCCC= %.4f%s", ix, iy, peakVal, 
                  CCC, overlap, pcDelX, pcDelY, pcErrX, wgtPeakVal, betterPeak? "*" : "");

                // If a new maximum is found, record position of peak and get the 
                // stage position and alignment shifts for this piece
                if (betterPeak) {
                  peakMax = CCC;
                  wgtPeakMax = wgtPeakVal;
                  overlapMax = overlap;
                  ixNew = ix;
                  iyNew = iy;
                  indBest = ind;
                  bestDelX = pcDelX;
                  bestDelY = pcDelY;
                  StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, ix, iy, 
                    stageX, stageY, cenMontErrX, cenMontErrY);
                  shiftX = pcShiftX;
                  shiftY = pcShiftY;
                }
              }
            }
          }
        }

        // Restore best shift to A and image in B
        setShiftInImage = true;
        if (ind != indBest || mRItryScaling) {
          mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[indBest], 1, true);
          mImBufs[1].mBinning = item->mMontBinning;
          RotateForAligning(1);
          if (mRItryScaling)
            AlignWithScaling(shiftX, shiftY, scaling);

          // Scaling will be 0 if it failed or wasn't done; either way, restore shifts
          setShiftInImage = scaling == 0.;
        }
        if (setShiftInImage) {
          mImBufs->mImage->setShifts(shiftX, shiftY);
          mImBufs->SetImageChanged(1);
          mNav->Redraw();
        }

      } else {

        // If single frame, read into B, rotate, align and set stage and shift values
        mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 1);
        mImBufs[1].mBinning = item->mMapBinning;
        if (RotateForAligning(1)) {
          RealignCleanup(1);
          return;
        }
        if (mRItryScaling)
          AlignWithScaling(shiftX, shiftY, scaling);
        if (!scaling) {
          mShiftManager->AutoAlign(1, -1, false, false, &peakVal);
          mImBufs->mImage->getShifts(shiftX, shiftY);
        }
        stageX = itemCenX;
        stageY = itemCenY;
      }

      // Now get the montage offset interpolated to the target position instead
      if (mUseMontStageError) {
        SEMTrace('1', "Change in montage error from center to target %.2f %.2f",
          mRItargMontErrX - cenMontErrX, mRItargMontErrY - cenMontErrY);
      }

      // Now we have a center stage position and the image alignment shift needed to get
      // there, so determine stage shift needed to get to target
      // Incorporate the change in montage error into this move instead of adding it to
      // the stage position in the move as was done in the initial move
      mStageErrX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      mStageErrY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      mRegistrationAtErr = mNav->GetCurrentRegistration();
      mRImapID = item->mMapID;
      report.Format("Disparity in stage position after aligning to center of map frame: "
        "%.2f %.2f", mStageErrX + mRIfirstStageX - (stageX + cenMontErrX), 
        mStageErrY + mRIfirstStageY - (stageY + cenMontErrY));
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      mCenterSkipArray[mCenSkipIndex].baseDelX = mStageErrX + mPrevLocalErrX - 
        (stageX + cenMontErrX);
      mCenterSkipArray[mCenSkipIndex].baseDelY = mStageErrY + mPrevLocalErrY - 
        (stageY + cenMontErrY);
      stageDelX = mCenterSkipArray[mCenSkipIndex].baseDelX + mRItargetX + mRItargMontErrX;
      stageDelY = mCenterSkipArray[mCenSkipIndex].baseDelY + mRItargetY + mRItargMontErrY;
      mCenterSkipArray[mCenSkipIndex].timeStamp = mWinApp->MinuteTimeStamp();

      // If there is scaling, the distance from center to target has shrunk from the
      // original image, so adjust by the change in this distance
      // Sign verified on 9% shrunk map, + gets to target, - doesn't
      if (scaling) {
        stageDelX += (1.f / scaling - 1.f) * (mRItargetX - stageX);
        stageDelY += (1.f / scaling - 1.f) * (mRItargetY - stageY);
      }
      mRInumRounds++;

      // If shift can be done with image shift, do that then go to second round
      if (sqrt(stageDelX * stageDelX + stageDelY * stageDelY) < mRImaximumIS) {
        bInv = MatMul(bMat, mShiftManager->CameraToIS(item->mMapMagInd));

        // Subtract the local error which is assumed to be from stage moves
        stageDelX -= mPrevLocalErrX;
        stageDelY -= mPrevLocalErrY;
        delISX = bInv.xpx * stageDelX + bInv.xpy * stageDelY;
        delISY = bInv.ypx * stageDelX + bInv.ypy * stageDelY;
        mScope->IncImageShift(delISX, delISY);
        mShiftManager->SetISTimeOut(mShiftManager->GetLastISDelay());
        report.Format("Aligning to target with image shift equivalent to stage shift:"
          " %.2f %.2f", stageDelX, stageDelY);
        mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

        // Shift the image as well (will need an override for debugging)
        //mImBufs->mImage->getShifts(shiftX, shiftY);
        bMat = mShiftManager->IStoCamera(item->mMapMagInd);
        shiftX = -(float)((bMat.xpx * delISX + bMat.xpy * delISY) / mImBufs->mBinning);
        shiftY = (float)((bMat.ypx * delISX + bMat.ypy * delISY) / mImBufs->mBinning);
        mImBufs->mImage->setShifts(shiftX , shiftY);
        mImBufs->SetImageChanged(1);
        mNav->Redraw();

        // Revise error based on current position and original adjusted target position
        mNav->GetAdjustedStagePos(stageX, stageY, shiftX);
        mStageErrX = (stageX - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftX, 
          mRInetViewShiftX)) - (mRItargetX - mPreviousErrX);
        mStageErrY = (stageY - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftY, 
          mRInetViewShiftY)) - (mRItargetY - mPreviousErrY);
        StartSecondRound();
        return;
      }

      // Otherwise first load images for aligning the next shot, get stage pos of center
      SEMTrace('1', "Moving to target with stage shift: %.2f %.2f",
        stageDelX, stageDelY);
      mRIscaling = scaling;

      if (LoadForAlignAtTarget(item))
        return;

      // Finally, move the stage, relative to the first stage move, adding the view shift
      // because it is not in the adjustment applied to the stage position
      mNav->BacklashForItem(item, shiftX, shiftY);
      mNav->AdjustAndMoveStage(mRIfirstStageX + mRInetViewShiftX + stageDelX, 
        mRIfirstStageY + mRInetViewShiftY + stageDelY, 0., axisXY, shiftX, shiftY,
        mRIfirstISX + mRIleaveISX, mRIfirstISY + mRIleaveISY);
      mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_NAV_REALIGN, TASK_SECOND_MOVE,
        30000);
      return;

    case TASK_SECOND_MOVE:
      StartRealignCapture(mRIContinuousMode != 0, TASK_SECOND_SHOT);
      return;

    case TASK_SECOND_SHOT:
      imagePixelSize = (float)mImBufs->mBinning *
        mShiftManager->GetPixelSize(item->mMapCamera, item->mMapMagInd);
      mShiftManager->AutoAlign(1, -1, true, false, &peakVal, mExpectedXshift, 
        mExpectedYshift, 0., mRIscaling, 0., NULL, NULL, true, NULL, NULL, 
        mRIweightSigma / imagePixelSize);
      mImBufs[1].mImage->setShifts(-mExpectedXshift, -mExpectedYshift);
      mImBufs[1].SetImageChanged(1);
      mImBufs->mImage->getShifts(shiftX, shiftY);
      mLocalErrX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      mLocalErrY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      SEMTrace('1', "Aligned to target with image shift of %.1f %.1f pixels\r\n    local "
        "stage err %.2f %.2f, implied total err %.2f %.2f", shiftX, shiftY, mLocalErrX, 
        mLocalErrY, mLocalErrX + mStageErrX, mLocalErrY + mStageErrY);
      mRInumRounds++;

      // Get new stage position and revise and report error, based on original adjusted
      // target position.  The net shift adjustments are sadly empirical
      mNav->GetAdjustedStagePos(stageX, stageY, shiftX);
      mStageErrX = (stageX - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftX, 
        mRInetViewShiftX)) - (mRItargetX - mPreviousErrX);
      mStageErrY = (stageY - B3DCHOICE(mRIstayingInLD, item->mNetViewShiftY, 
        mRInetViewShiftY)) - (mRItargetY - mPreviousErrY);
      report.Format("Disparity in stage position after aligning to target position: "
        "%.2f %.2f  (2nd move error %.2f %.2f)", mStageErrX, mStageErrY,
        mLocalErrX, mLocalErrY);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);

      // Evaluate whether center alignment can be re-used
      // Cancel re-use if error is bigger than a fraction of field size or if an error
      // crit is set and the error is above that; 
      // otherwise if an error crit is set, renew the time stamp
      shiftX = (float)sqrt(mLocalErrX * mLocalErrX + mLocalErrY * mLocalErrY);
      if (shiftX > 0.17 * imagePixelSize * 
        B3DMIN(mImBufs->mImage->getWidth(), mImBufs->mImage->getHeight()) ||
        (mRIskipCenErrorCrit > 0. && shiftX > mRIskipCenErrorCrit)) {
          mCenterSkipArray[mCenSkipIndex].timeStamp = 0;
      } else if (mRIskipCenErrorCrit > 0.) {
        mCenterSkipArray[mCenSkipIndex].timeStamp = mWinApp->MinuteTimeStamp();
      }
      StartSecondRound();
      return;

    case TASK_FINAL_SHOT:
      mShiftManager->AutoAlign(1, 0);
      mImBufs->mImage->getShifts(shiftX, shiftY);
      SEMTrace('1', "Realigned to map item itself with image shift of %.1f %.1f pixels",
        shiftX, shiftY);
      mRInumRounds++;

      // Get new stage position and revise and report error, based on target position of
      // map itself.  Do this before changing mag to get correct estimate
      mNav->GetAdjustedStagePos(stageX, stageY, cenMontErrX);
      mStageErrX = stageX - mRIabsTargetX;
      mStageErrY = stageY - mRIabsTargetY;
      item = mNav->FindItemWithMapID(mSecondRoundID);
      bInv = MatInv(ItemStageToCamera(item));
      stageX = -mImBufs->mBinning * (bInv.xpx * shiftX - bInv.xpy * shiftY);
      stageY = -mImBufs->mBinning * (bInv.ypx * shiftX - bInv.ypy * shiftY);
      report.Format("Disparity in stage position after aligning to map item itself: "
        "%.2f %.2f  (align shift %.2f %.2f)", mStageErrX, mStageErrY, stageX, stageY);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      report.Format("Finished aligning to item in %d rounds", mRInumRounds);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
      StopRealigning();
      return;
  }
}

// Load the image needed for second round alignment, rotate if needed, and compute 
// expected shift for aligning
int CNavHelper::LoadForAlignAtTarget(CMapDrawItem *item)
{
  float shiftX = item->mStageX + mPreviousErrX;
  float shiftY = item->mStageY + mPreviousErrY;
  int ind, i, retval = 0;
  float tmpX, tmpY;
  ScaleMat bMat = ItemStageToCamera(item);

  for (i = mBufferManager->GetShiftsOnAcquire(); i > 0 ; i--)
    mBufferManager->CopyImageBuffer(i - 1, i);
  if (item->mMapMontage) {
    if (mUseMontCenter) {
      SEMTrace('1', "Second round alignment to montage center");
      retval = mWinApp->mMontageController->ReadMontage(item->mMapSection, mMapMontP,
        mMapStore, true);
    } else {
      ind = mRIiy + mRIix * mMapMontP->yNframes;
      retval = mBufferManager->ReadFromFile(mMapStore, mPieceSavedAt[ind], 0, true);
      StagePositionOfPiece(mMapMontP, mRImat, mRIdelX, mRIdelY, mRIix, mRIiy, shiftX,
        shiftY, tmpX, tmpY);
      SEMTrace('1', "Second round alignment to piece %d %d at %.2f %.2f", mRIix, 
        mRIiy,  shiftX, shiftY);
    }

  } else {
    retval = mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 0);
  }
  mImBufs[0].mBinning = item->mMapMontage ? item->mMontBinning : item->mMapBinning;

  // Rotate image then get expected shift in aligning to the given image
  if (!retval)
    retval = RotateForAligning(0);
  if (retval) {
    RealignCleanup(1);
    return 1;
  }
  shiftX = mRItargetX - shiftX;
  shiftY = mRItargetY - shiftY;
  mExpectedXshift = (bMat.xpx * shiftX + bMat.xpy * shiftY) / mImBufs->mBinning;
  mExpectedYshift = -(bMat.ypx * shiftX + bMat.ypy * shiftY) / mImBufs->mBinning;
  if (mUseMontCenter)
    mWinApp->mMontageController->AdjustShiftInCenter(mMapMontP, mExpectedXshift,
    mExpectedYshift);
  return 0;
}

void CNavHelper::RealignCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox(_T("Time out realigning to Navigator position"));
  StopRealigning();
  mWinApp->ErrorOccurred(error);
}

// To end the realigning and restore conditions
void CNavHelper::StopRealigning(void)
{
  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  if (!mRealigning)
    return;

  // Close file if not part of open sets
  if (mCurStoreInd < 0 && mMapStore)
    delete mMapStore;

  // Restore state if saved
  RestoreLowDoseConset();
  RestoreMapOffsets();
  RestoreSavedState();
  mCamera->ChangePreventToggle(-1);
  if (mRIContinuousMode == 1)
    mCamera->StopCapture(0);
  if (mRIContinuousMode > 1)
    conSet->mode = CONTINUOUS;
 
  mCamera->SetRequiredRoll(0);
  mWinApp->SetStatusText(MEDIUM_PANE, "");
  mRealigning = 0;
  mWinApp->UpdateBufferWindows();
}

// If aligning to a map item after a first round at lower mag, set up conditions
void CNavHelper::StartSecondRound(void)
{
  float useWidth, useHeight;

  CString report;
  ScaleMat aMat;
  KImageStore *imageStore;
  CMapDrawItem *item = mNav->FindItemWithMapID(mSecondRoundID);

  // Undo the image shift imposed in the first round if any
  if (mRIfirstISX || mRIfirstISY)
    mScope->IncImageShift(-mRIfirstISX, -mRIfirstISY);
  if (!item) {
    StopRealigning();
    report.Format("Finished aligning to item in %d rounds", mRInumRounds);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    return;
  }
  if (mCurStoreInd < 0)
    delete mMapStore;
  mMapStore = NULL;
  if (mNav->AccessMapFile(item, imageStore, mCurStoreInd, mMapMontP, useWidth, 
    useHeight)) {
    SEMMessageBox("Failed to access map file of item being aligned to");
    RealignCleanup(1);
    return;
  }
  mMapStore = imageStore;
  mRealigning = 2;

  // Get a new rotation matrix for this item
  aMat = ItemStageToCamera(item);
  mRIrotAngle = mNav->RotationFromStageMatrices(aMat, item->mMapScaleMat, mRIinverted);
  mRIrMat = mNav->GetRotationMatrix(mRIrotAngle, mRIinverted);

  ControlSet *conSet = mWinApp->GetConSets() + TRACK_CONSET;
  RestoreLowDoseConset();
  PrepareToReimageMap(item, mMapMontP, conSet, TRIAL_CONSET, TRUE);
  if (mRIstayingInLD)
    RestoreMapOffsets();
  else
    SetMapOffsetsIfAny(item);
  if (mRIContinuousMode) {
    mCamera->StopCapture(0);
    if (mRIContinuousMode > 1)
      conSet->mode = CONTINUOUS;
  }

  // Load the image to align to and rotate as usual
  for (int i = mBufferManager->GetShiftsOnAcquire(); i > 0 ; i--)
    mBufferManager->CopyImageBuffer(i - 1, i);
  if (item->mMapMontage) {
    mWinApp->mMontageController->ReadMontage(item->mMapSection, mMapMontP,
      mMapStore, true);
  } else {
     mBufferManager->ReadFromFile(mMapStore, item->mMapSection, 0);
  }
  mImBufs[0].mBinning = item->mMapMontage ? item->mMontBinning : item->mMapBinning;
  if (RotateForAligning(0)) {
    RealignCleanup(1);
    return;
  }

  // Fire off the shot - AGAIN TIMEOUT WAS 300000
  StartRealignCapture(mRIContinuousMode > 1, TASK_FINAL_SHOT);
}

// Start a capture for Realign if not using continuous mode or startContinuous is true,
// and set up the idel task appropriately
void CNavHelper::StartRealignCapture(bool useContinuous, int nextTask)
{
  if (!useContinuous || !mCamera->DoingContinuousAcquire())
    mCamera->InitiateCapture(mRIconSetNum);
  if (useContinuous) {
    mCamera->SetTaskWaitingForFrame(true);
    mWinApp->AddIdleTask(CCameraController::TaskGettingFrame, TASK_NAV_REALIGN, nextTask,
      0);
  } else {
    mWinApp->AddIdleTask(SEMStageCameraBusy, TASK_NAV_REALIGN, nextTask, 0);
  }
}

// Set scope and filter parameters and set up a control set imaging a map area
void CNavHelper::PrepareToReimageMap(CMapDrawItem *item, MontParam *param, 
                                     ControlSet *conSet, int baseNum, BOOL hideLDoff) 
{
  ControlSet *conSets = mWinApp->GetConSets();
  int  binning, xFrame, yFrame, area;
  StateParams stateParam;
  LowDoseParams *ldp;
  CString *names = mWinApp->GetModeNames();
  CameraParameters *camP = mWinApp->GetCamParams() + item->mMapCamera;
  StoreMapStateInParam(item, param, baseNum, &stateParam);

  // If coming from Realign to Item, see if we can STAY in low dose instead of hiding it
  mRIstayingInLD = false;
  mRIleaveISX = mRIleaveISY = 0.;
  if (hideLDoff) {
    if (item->mMapMontage) {
      xFrame = param->xFrame;
      yFrame = param->yFrame;
      binning = item->mMontBinning ? item->mMontBinning : param->binning;
    } else {
      xFrame = item->mMapWidth;
      yFrame = item->mMapHeight;
      binning = item->mMapBinning;
    }
    mRIstayingInLD = CanStayInLowDose(item, xFrame, yFrame, binning, mRIconSetNum,
      mRInetViewShiftX, mRInetViewShiftY, true);

    // If staying in low dose, go there now, so that the stage move will be adjusted at
    // the same mag as the anticipated adjustment was computed
    if (mRIstayingInLD) {
      SEMTrace('1', "Map matches low dose %s; staying in low dose but with exp %.3f "
        "intensity %.5f bin %f", (LPCTSTR)names[mRIconSetNum], item->mMapExposure, 
        item->mMapIntensity, binning / (camP->K2Type ? 2. : 1.));

      // Save the conSet and modify it to the map parameters
      mSavedConset = conSets[mRIconSetNum];
      StateCameraCoords(item->mMapCamera, xFrame, yFrame, binning, 
        conSets[mRIconSetNum].left, conSets[mRIconSetNum].right, 
        conSets[mRIconSetNum].top, conSets[mRIconSetNum].bottom);
      conSets[mRIconSetNum].exposure = item->mMapExposure;
      conSets[mRIconSetNum].binning = binning;
      conSets[mRIconSetNum].saveFrames = 0;
      conSets[mRIconSetNum].mode = SINGLE_FRAME;

      // Also save the intensity and us map one
      area = mCamera->ConSetToLDArea(item->mMapLowDoseConSet);
      ldp = mWinApp->GetLowDoseParams() + area;
      mRIsavedLDintensity = ldp->intensity;
      ldp->intensity = item->mMapIntensity;
      mScope->GotoLowDoseArea(mRIconSetNum);
      return;
    }

    // If leaving LD, set the base IS to leave on stage moves
    ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
    mRIleaveISX = ldp->ISX;
    mRIleaveISY = ldp->ISY;
  }
  SEMTrace('I', "PrepareToReimageMap set intensity to %.5f  %.3f%%", stateParam.intensity
    , mScope->GetC2Percent(stateParam.spotSize, stateParam.intensity));
  SetStateFromParam(&stateParam, conSet, baseNum, hideLDoff);
}

// Determine if the realign routine can stay in low dose
bool CNavHelper::CanStayInLowDose(CMapDrawItem *item, int xFrame, int yFrame, int binning,
  int &setNum, float &retShiftX, float &retShiftY, bool forReal)
{
  int area;
  int mapLDconSet = item->mMapLowDoseConSet;
  double netX, netY, fullX, fullY;
  bool match = true;
  LowDoseParams *ldp;
  CameraParameters *camP = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  bool hasAlpha = JEOLscope && !mScope->GetHasNoAlpha();
  bool filtered = (camP->GIF || mScope->GetHasOmegaFilter()) && item->mMapSlitWidth > 0.;
  setNum = TRACK_CONSET;
  retShiftX = item->mNetViewShiftX;
  retShiftY = item->mNetViewShiftY;

  // Compute the amount of mag offset calibration shift that the requested stage position
  // will be adjusted by so that it can be added onto the requested position, since the
  // view shifts incorporate the calibration as well
  mRIcalShiftX = mRIcalShiftY = 0.;
  if ((retShiftX || retShiftY) && !mScope->GetApplyISoffset()) {
    mWinApp->mLowDoseDlg.GetNetViewShift(netX, netY);
    mWinApp->mLowDoseDlg.GetFullViewShift(fullX, fullY);
    fullX -= netX;
    fullY -= netY;
    SimpleIStoStage(item, fullX, fullY, mRIcalShiftX, mRIcalShiftY);
   }

  // Now if in low dose, look for matching set
  if (mWinApp->LowDoseMode() && mapLDconSet >= 0 && mapLDconSet <= PREVIEW_CONSET) {
    area = mCamera->ConSetToLDArea(mapLDconSet);
    ldp = mWinApp->GetLowDoseParams() + area;

    // Check mag/spot/alpha and filter
    if (item->mMapCamera != mWinApp->GetCurrentCamera() || 
      item->mMapMagInd != ldp->magIndex || item->mMapSpotSize != ldp->spotSize ||
      (item->mMapProbeMode >= 0 && item->mMapProbeMode != ldp->probeMode) || 
      (hasAlpha && (item->mMapAlpha >= 0 || ldp->beamAlpha >= 0) &&  
      item->mMapAlpha != ldp->beamAlpha) ||
      (filtered && ((item->mMapSlitIn ? 1 : 0 != ldp->slitIn ? 1 : 0) || (ldp->slitIn &&
      fabs(item->mMapSlitWidth - ldp->slitWidth) > 6))) || item->mMapBinning <= 0) {
        if (forReal) {
          SEMTrace('1', "Leaving LD, map bin %d map/LD cam %d/%d mag %d/%d spot %d/%d"
            " probe %d/%d alpha %d/%.0f", item->mMapBinning, item->mMapCamera, 
            mWinApp->GetCurrentCamera(), item->mMapMagInd, ldp->magIndex, 
            item->mMapSpotSize, ldp->spotSize, item->mMapProbeMode >= 0 ? 
            item->mMapProbeMode : -1, item->mMapProbeMode >= 0 ? ldp->probeMode : -1, 
            hasAlpha ? item->mMapAlpha : -1, hasAlpha ? ldp->beamAlpha : -1.);
          if (filtered)
            SEMTrace('1', "   slit in %d/%d width %.1f/%.1f", item->mMapSlitIn ? 1 : 0,
              ldp->slitIn ? 1 : 0, item->mMapSlitWidth, ldp->slitWidth, -1.);
        }
        match = false;
    }

    // Check for defocus offset change
    if (match && item->mMapLowDoseConSet == VIEW_CONSET && 
      fabs(item->mDefocusOffset - mScope->GetLDViewDefocus()) > 5.) {
        if (forReal)
          SEMTrace('1', "Leaving LD, defocus offset map %.1f  LD %.1f",
            item->mDefocusOffset, mScope->GetLDViewDefocus());
        match = false;
    }

    // Found a match!
    if (match) {
      setNum = item->mMapLowDoseConSet;
      retShiftX = 0.;
      retShiftY = 0.;
      return true;
    }
  }

  // But if not staying in low dose, need to adjust the returned shift to be a full view
  // shift with the old shift and the new difference between full and net
  retShiftX += mRIcalShiftX;
  retShiftY += mRIcalShiftY;
  return false;
}

// Set some special items if appropriate for the given item: defocus offset, alpha change,
// and beam shift and tilt if enable by properties.
void CNavHelper::SetMapOffsetsIfAny(CMapDrawItem *item)
{

  // First restore existing offsets
  RestoreMapOffsets();

  // Handle defocus offset
  mRIdefocusOffsetSet = item->mDefocusOffset;
  if (item->mDefocusOffset)
    mScope->IncDefocus(item->mDefocusOffset);

  // Handle alpha change
  if (item->mMapAlpha >= 0) {
    mRIalphaSaved = mScope->GetAlpha();
    if (mRIalphaSaved >= 0) {
      mScope->ChangeAlphaAndBeam(mRIalphaSaved, item->mMapAlpha);
      mRIalphaSet = item->mMapAlpha;
    }
  }

  // Handle a beam shift/tilt after the alpha change, as in low dose
  if (mRIuseBeamOffsets) {
    mRIbeamShiftSetX = item->mViewBeamShiftX;
    mRIbeamShiftSetY = item->mViewBeamShiftY;
    if (mRIbeamShiftSetX || mRIbeamShiftSetY)
      mScope->IncBeamShift(mRIbeamShiftSetX, mRIbeamShiftSetY);
    mRIbeamTiltSetX = item->mViewBeamTiltX;
    mRIbeamTiltSetY = item->mViewBeamTiltY;
    if (mRIbeamTiltSetX || mRIbeamTiltSetY)
      mScope->IncBeamTilt(mRIbeamTiltSetX, mRIbeamTiltSetY);
  }
}

// Restore any special offsets or changes set by SetMapOffsetsIfAny
void CNavHelper::RestoreMapOffsets()
{

  // Restore defocus offset
  if (mRIdefocusOffsetSet)
    mScope->IncDefocus(-mRIdefocusOffsetSet);
  mRIdefocusOffsetSet = 0.;

  // Then restore alpha.  Do alpha and other beam changes in the same order as when
  // changing low dose areas, since that is what this mimics
  SEMTrace('b', "RestoreMapOffsets restore alpha %d to %d",mRIalphaSet + 1, 
      mRIalphaSaved + 1);
  if (mRIalphaSet >= 0 && mRIalphaSaved >= 0)
    mScope->ChangeAlphaAndBeam(mRIalphaSet, mRIalphaSaved);
  mRIalphaSet = -999;

  // Restore beam shift/tilt
  SEMTrace('b', "RestoreMapOffsets restore bs %f %f  bt %f %f", mRIbeamShiftSetX, 
    mRIbeamShiftSetY, mRIbeamTiltSetX, mRIbeamTiltSetY);
  if (mRIbeamShiftSetX || mRIbeamShiftSetY)
    mScope->IncBeamShift(-mRIbeamShiftSetX, -mRIbeamShiftSetY);
  if (mRIbeamTiltSetX || mRIbeamTiltSetY)
    mScope->IncBeamTilt(-mRIbeamTiltSetX, -mRIbeamTiltSetY);
  mRIbeamShiftSetX = mRIbeamShiftSetY = 0.;
  mRIbeamTiltSetX = mRIbeamTiltSetY = 0.;
}

/////////////////////////////////////////////////
/////  STATE SAVING, SETTING, RESTORING
/////////////////////////////////////////////////

// Restore state if it was saved
void CNavHelper::RestoreSavedState(void)
{
  ControlSet *masterSets = mWinApp->GetCamConSets();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  if (mTypeOfSavedState == STATE_NONE)
    return;
  SetStateFromParam(&mSavedState, conSet, RECORD_CONSET);
  if (mSavedState.lowDose && mSavedLowDoseArea >= 0)
    mScope->GotoLowDoseArea(mSavedLowDoseArea);
  SEMTrace('I', "RestoreSavedState restored intensity to %.5f", mSavedState.lowDose ?
    mSavedState.ldParams.intensity : mSavedState.intensity);
  masterSets[mSavedState.camIndex * MAX_CONSETS + RECORD_CONSET] = *conSet;
  mTypeOfSavedState = STATE_NONE;
}

// Set the scope and camera parameter state to that used to acquire a map item
int CNavHelper::SetToMapImagingState(CMapDrawItem * item, bool setCurFile)
{
  int camera, curStore, err, retval = 0;
  float width, height;
  ControlSet tmpSet;
  ControlSet *masterSets = mWinApp->GetCamConSets();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  KImageStore *imageStore;
  MontParam *masterMont = mWinApp->GetMontParam();

  if (item->mType != ITEM_TYPE_MAP || item->mImported)
    return 1;
  mMapMontP = NULL;
  SaveCurrentState(STATE_MAP_ACQUIRE, false);
  mMapStateItemID = item->mMapID;
  mSavedStoreName = "";
  mNewStoreName = "";
  if (setCurFile) {
    mMapMontP = &mMapMontParam;

    // Open the file or get the store # if it is currently open
    err = mNav->AccessMapFile(item, imageStore, curStore, mMapMontP, width, height, 
      setCurFile);
    if (err) {
      AfxMessageBox("The file for that map item can no longer be accessed", MB_EXCLAME);
      mMapMontP = NULL;
      curStore = -1;
      retval = 1;
    } else {

      // Save current name if we are going to a new file
      if (mWinApp->mStoreMRC && (curStore >= 0 || mDocWnd->GetNumStores() < MAX_STORES))
        mSavedStoreName = mWinApp->mStoreMRC->getName();

      // Change to the file if it is already open
      if (curStore >= 0 && mDocWnd->GetCurrentStore() != curStore)
        mDocWnd->SetCurrentStore(curStore);
    
      // Otherwise define it as new open store if there is enough space
      if (curStore < 0) {
        if (mDocWnd->GetNumStores() < MAX_STORES) {
          mDocWnd->LeaveCurrentFile();
          mWinApp->mStoreMRC = imageStore;
          mNewStoreName = imageStore->getName();
          if (item->mMapMontage) {
            *masterMont = *mMapMontP;
            mWinApp->SetMontaging(true);
          }
          mDocWnd->AddCurrentStore();
        } else {
          AfxMessageBox("The file for that map item cannot be opened for saving.\n"
            "There are too many other files open right now", MB_EXCLAME);
          delete imageStore;
          retval = 1;
        }
      }
    }
  }

  PrepareToReimageMap(item, mMapMontP, &tmpSet, RECORD_CONSET, FALSE);
  camera = mWinApp->GetCurrentCamera();
  /*if (conSet->binning != tmpSet.binning || conSet->exposure != tmpSet.exposure ||
    conSet->drift != tmpSet.drift)
    mWinApp->AppendToLog("\r\nCamera parameters were changed in setting to map item "
    "state", LOG_OPEN_IF_CLOSED); */
  *conSet = tmpSet;
  masterSets[camera * MAX_CONSETS + RECORD_CONSET] = tmpSet;
  UpdateStateDlg();
  return retval;
}

// Restore state from setting to a map state
int CNavHelper::RestoreFromMapState(void)
{
  ControlSet *masterSets = mWinApp->GetCamConSets();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  int camera, store;
  RestoreLowDoseConset();
  if (mTypeOfSavedState != STATE_MAP_ACQUIRE)
    return 1;
  RestoreSavedState();
  camera = mWinApp->GetCurrentCamera();
  masterSets[camera * MAX_CONSETS + RECORD_CONSET] = *conSet;

  // Close new file if it was opened just for this purpose
  if (!mNewStoreName.IsEmpty()) {
    store = mDocWnd->StoreIndexFromName(mNewStoreName);
    if (store >= 0) {
      mDocWnd->SetCurrentStore(store);
      mDocWnd->DoCloseFile();
    }
  }

  // Restore original file if it is still open
  if (!mSavedStoreName.IsEmpty()) {
    store = mDocWnd->StoreIndexFromName(mSavedStoreName);
    if (store >= 0)
      mDocWnd->SetCurrentStore(store);
  }
  UpdateStateDlg();
  return 0;
}

// Store the current scope state or the Record low dose params into the state param
void CNavHelper::StoreCurrentStateInParam(StateParams *param, BOOL lowdose, 
  bool saveLDfocusPos)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  param->lowDose = lowdose ? 1 : 0;
  if (lowdose) {
    param->ldParams = *ldp;
    param->probeMode = ldp->probeMode;
  } else {
    param->magIndex = mScope->GetMagIndex();
    param->intensity = mScope->GetIntensity();
    param->spotSize = mScope->GetSpotSize();
    param->probeMode = mScope->ReadProbeMode();
  
    // Store filter parameters unconditionally here; they will be set conditionally
    param->slitIn = filtParam->slitIn;
    param->slitWidth = filtParam->slitWidth;
    param->energyLoss = filtParam->energyLoss;
    param->zeroLoss = filtParam->zeroLoss;
  }
  param->camIndex = mWinApp->GetCurrentCamera();
  param->binning = conSet->binning;
  param->exposure = conSet->exposure;
  param->drift = conSet->drift;
  param->shuttering = conSet->shuttering;
  param->K2ReadMode = conSet->K2ReadMode;
  param->singleContMode = conSet->mode;
  param->saveFrames = conSet->saveFrames;
  param->doseFrac = conSet->doseFrac;
  param->frameTime = conSet->frameTime;
  param->processing = conSet->processing;
  param->alignFrames = conSet->alignFrames;
  param->useFrameAlign = conSet->useFrameAlign;
  param->faParamSetInd = conSet->faParamSetInd;

  param->xFrame = (conSet->right - conSet->left) / param->binning;
  param->yFrame = (conSet->bottom - conSet->top) / param->binning;

  param->focusXoffset = param->focusYoffset = 0;
  param->focusAxisPos = EXTRA_NO_VALUE;
  if (lowdose && saveLDfocusPos) {
    ldp = mWinApp->GetLowDoseParams() + FOCUS_CONSET;
    if (mWinApp->LowDoseMode() && ldp->magIndex)
      ldp->axisPosition = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(ldp->magIndex, ldp->ISX,
      ldp->ISY);
    conSet = mWinApp->GetConSets() + FOCUS_CONSET;
    param->focusXoffset = (conSet->right + conSet->left) / 2 - camParam->sizeX / 2;
    param->focusYoffset = (conSet->bottom + conSet->top) / 2 - camParam->sizeY / 2;
    param->focusAxisPos = ldp->axisPosition;
    SEMTrace('1', "Saved focus in state: axis pos %.2f  offsets %d %d", 
      param->focusAxisPos, param->focusXoffset, param->focusYoffset);
  }
}

// Store the acquire state defined in a map item into a state param, accessing the 
// montage param if non-NULL, or the camera conset specified by baseNum for some values
void CNavHelper::StoreMapStateInParam(CMapDrawItem *item, MontParam *montP, int baseNum,
                                      StateParams *param)
{
  ControlSet *camSets = mWinApp->GetCamConSets();
  ControlSet *conSet;

  // Start by filling with the current state in case some map items are not there
  StoreCurrentStateInParam(param, false, false);
  param->magIndex = item->mMapMagInd;
  if (item->mMapIntensity)
    param->intensity = item->mMapIntensity;
  if (item->mMapSpotSize)
    param->spotSize = item->mMapSpotSize;
  param->probeMode = item->mMapProbeMode;
  if (item->mMapCamera >= 0)
    param->camIndex = item->mMapCamera;
  param->slitWidth = 0.;
  if ((mCamParams[param->camIndex].GIF || mWinApp->ScopeHasFilter()) && 
    item->mMapSlitWidth) {
    param->slitIn = item->mMapSlitIn;
    param->slitWidth = item->mMapSlitWidth;
    param->energyLoss = 0.;
    param->zeroLoss = true;
  }
  if (item->mMapBinning > 0)
    param->binning = item->mMapBinning;
  if (item->mShutterMode >= 0) {
    param->exposure = item->mMapExposure;
    param->drift = item->mMapSettling;
    param->shuttering = item->mShutterMode;
    param->K2ReadMode = item->mK2ReadMode;
  }
  param->singleContMode = SINGLE_FRAME;
  param->saveFrames = 0;
  param->doseFrac = 0;
  param->processing = GAIN_NORMALIZED;

  if (item->mMapMontage) {
    if (montP) {
      param->xFrame = montP->xFrame;
      param->yFrame = montP->yFrame;
      param->binning = item->mMontBinning ? item->mMontBinning : montP->binning;
    } else {

      // If no param is entered, take current frame size, adjust for binning 
      // Use the indicated conset from camera sets as the base for this info
      conSet = &camSets[param->camIndex * MAX_CONSETS + baseNum];
      param->binning = item->mMontBinning ? item->mMontBinning : conSet->binning;
      param->xFrame = (conSet->right - conSet->left) / param->binning;
      param->yFrame = (conSet->bottom - conSet->top) / param->binning;
    }
  } else {
    param->xFrame = item->mMapWidth;
    param->yFrame = item->mMapHeight;
    param->binning = item->mMapBinning;
  }
}

// Set scope state and control set state from the given state param
void CNavHelper::SetStateFromParam(StateParams *param, ControlSet *conSet, int baseNum,
                                   BOOL hideLDoff)
{
  int left, right, top, bottom, xFrame, yFrame, i;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  CameraParameters *camP = &mCamParams[param->camIndex];
  int *activeList = mWinApp->GetActiveCameraList();
  LowDoseParams *ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;
  LowDoseParams ldsaParams;
  bool changed, gotoArea = false;
  ControlSet *workSets = mWinApp->GetConSets();
  ControlSet *camSets = mWinApp->GetCamConSets();
  ControlSet *focusSet = mWinApp->GetConSets() + FOCUS_CONSET;

  // Set camera first in case there is an automatic mag change and EFTEM change
  for (i = 0; i < mWinApp->GetNumActiveCameras(); i++) {
    if (activeList[i] == param->camIndex) {
      mWinApp->SetActiveCameraNumber(i);
      break;
    }
  }

  if (param->lowDose) {

    // If it is already in low dose and in record, need to save the params and tell it
    // to use those as current params; then need to go to area after changing params
    if (mWinApp->LowDoseMode() && mScope->GetLowDoseArea() == RECORD_CONSET) {
      ldsaParams = *ldp;
      mScope->SetLdsaParams(&ldsaParams);
      gotoArea = true;
    }
    if (mWinApp->LowDoseMode())
      ldp->axisPosition = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(
        ldp->magIndex, ldp->ISX, ldp->ISY);
    param->ldParams.axisPosition = ldp->axisPosition;
    *ldp = param->ldParams;
    mWinApp->mLowDoseDlg.ConvertOneAxisToIS(ldp->magIndex, ldp->axisPosition, ldp->ISX,
      ldp->ISY);
    if (gotoArea)
      mScope->GotoLowDoseArea(RECORD_CONSET);
    else
      mWinApp->mLowDoseDlg.SetLowDoseMode(true);
  } else {
    mWinApp->mLowDoseDlg.SetLowDoseMode(false, hideLDoff);
    mScope->SetMagIndex(param->magIndex);
    if (param->probeMode >= 0)
      mScope->SetProbeMode(param->probeMode, true);

    // Set the spot size before intensity tp make sure the intensity is legal for this
    // spot size on a FEI, and maybe to handle spot-size dependent changes elsewhere
    mScope->SetSpotSize(param->spotSize);
    if (!camP->STEMcamera)
      mScope->DelayedSetIntensity(param->intensity, GetTickCount());

    // Modify filter parameters if they are present: but don't update unless in filter mode
    if (param->slitWidth > 0.) {
      filtParam->slitIn = param->slitIn;
      filtParam->slitWidth = param->slitWidth;
      filtParam->zeroLoss = param->zeroLoss;
      filtParam->energyLoss = param->energyLoss;
      if (mWinApp->GetFilterMode()) {
        mCamera->SetIgnoreFilterDiffs(true);        
        mWinApp->mFilterControl.UpdateSettings();
      }
    }
  }

  // Copy control set in case need to fill in missing stuff (camera switch
  // copied the camera conset in)
  *conSet = *(mWinApp->GetConSets() + baseNum);

  StateCameraCoords(param->camIndex, param->xFrame, param->yFrame, param->binning, 
    conSet->left, conSet->right, conSet->top, conSet->bottom);
  conSet->binning = param->binning;
  conSet->mode = param->singleContMode;
  conSet->exposure = param->exposure;
  if (!camP->STEMcamera)
    conSet->drift = param->drift;
  conSet->shuttering = param->shuttering;
  conSet->K2ReadMode = param->K2ReadMode;
  conSet->doseFrac = param->doseFrac;
  conSet->saveFrames = param->saveFrames;
  if (param->frameTime > 0)
    conSet->frameTime = param->frameTime;
  if (param->processing >= 0)
    conSet->processing = param->processing;
  if (param->alignFrames >= 0)
    conSet->alignFrames = param->alignFrames;
  if (param->useFrameAlign >= 0)
    conSet->useFrameAlign = param->useFrameAlign;
  if (param->faParamSetInd >= 0) {
    conSet->faParamSetInd = param->faParamSetInd;
    B3DCLAMP(conSet->faParamSetInd, 0, mCamera->GetNumFrameAliParams());
  }

  // Set axis position and subarea of focus area if it was stored in param
  if (param->lowDose && param->focusAxisPos > EXTRA_VALUE_TEST) {
    ldp = mWinApp->GetLowDoseParams() + FOCUS_CONSET;
    changed = ldp->axisPosition != param->focusAxisPos;
    ldp->axisPosition = param->focusAxisPos;
    if (mWinApp->LowDoseMode() && ldp->magIndex)
      mWinApp->mLowDoseDlg.ConvertOneAxisToIS(ldp->magIndex, ldp->axisPosition, ldp->ISX,
      ldp->ISY);
    if (mWinApp->mLowDoseDlg.m_bTieFocusTrial) {
      ldp = mWinApp->GetLowDoseParams();
      ldp[TRIAL_CONSET] = ldp[FOCUS_CONSET];
    }
    mWinApp->mLowDoseDlg.ManageAxisPosition();
    xFrame = focusSet->right - focusSet->left;
    yFrame = focusSet->bottom - focusSet->top;
    left = B3DMAX(0, (camP->sizeX - xFrame) / 2 + param->focusXoffset);
    right = B3DMIN(camP->sizeX, left + xFrame);
    left = right - xFrame;
    top = B3DMAX(0, (camP->sizeY - yFrame) / 2 + param->focusYoffset);
    bottom = B3DMIN(camP->sizeY, top + yFrame);
    top = bottom - yFrame;
    if (top != focusSet->top || bottom != focusSet->bottom || left != focusSet->left ||
      right != focusSet->right || changed)
      PrintfToLog("Focus area changed from stored state, axis position %.2f, "
      "left %d right %d top %d bottom %d", param->focusAxisPos, left, right, top, bottom);
    focusSet->left = left;
    focusSet->right = right;
    focusSet->top = top; 
    focusSet->bottom = bottom;
  }

  // Copy back to the camera sets
  for (i = 0; i < MAX_CONSETS; i++)
    camSets[param->camIndex * MAX_CONSETS + i] = workSets[i];
}

// Save the current state if it is not already saved
void CNavHelper::SaveCurrentState(int type, bool saveLDfocusPos)
{
  if (mTypeOfSavedState != STATE_NONE)
    return;
  mTypeOfSavedState = type;
  StoreCurrentStateInParam(&mSavedState, mWinApp->LowDoseMode(), saveLDfocusPos);
  SEMTrace('I', "SaveCurrentState saved intensity %.5f", mWinApp->LowDoseMode() ? 
    mSavedState.ldParams.intensity : mSavedState.intensity);
  mSavedLowDoseArea = mScope->GetLowDoseArea();
}

// If stayed in Low Dose for a map imaging instead of switching to a map state, restore
// the saved conSet
void CNavHelper::RestoreLowDoseConset(void)
{
  int area;
  LowDoseParams *ldp;
  ControlSet *workSets = mWinApp->GetConSets();
  ControlSet *camSets = mWinApp->GetCamConSets();
  if (!mRIstayingInLD)
    return;
  workSets[mRIconSetNum] = mSavedConset;
  camSets[mWinApp->GetCurrentCamera() * MAX_CONSETS + mRIconSetNum] = mSavedConset;
  area = mCamera->ConSetToLDArea(mRIconSetNum);
  ldp = mWinApp->GetLowDoseParams() + area;
  ldp->intensity = mRIsavedLDintensity;

  // And be sure to turn flag off in case this is called from other situations
  mRIstayingInLD = false;
}

/////////////////////////////////////////////////
/////  VARIOUS UTILITIES
/////////////////////////////////////////////////

// Get the stage position of the center of a montage piece, adding previously found error
void CNavHelper::StagePositionOfPiece(MontParam * param, ScaleMat aMat, float delX,
                                      float delY, int ix, int iy, float &stageX, 
                                      float & stageY, float &montErrX, float &montErrY)
{
  int retval;
  int imX = ix * (param->xFrame - param->xOverlap) + param->xFrame / 2;
  int imY = (param->yNframes - 1 - iy) * (param->yFrame - param->yOverlap) + 
    param->yFrame / 2;
  ScaleMat aInv = MatInv(aMat);
  stageX = aInv.xpx * (imX - delX) + aInv.xpy * (imY - delY) + mPreviousErrX;
  stageY = aInv.ypx * (imX - delX) + aInv.ypy * (imY - delY) + mPreviousErrY;
  montErrX = montErrY = 0.;

  // Return stage error when acquiring montage if one was obtained and flag is set
  if (mUseMontStageError) {
    int adocInd = mMapStore->GetAdocIndex();
    if (adocInd < 0 || AdocGetMutexSetCurrent(adocInd) < 0)
      return;
    retval = LookupMontStageOffset(mMapStore, mMapMontP, ix, iy, mPieceSavedAt, montErrX,
      montErrY);
    AdocReleaseMutex();
    if (retval)
      return;
  }
}

// Get the montage stage offset for the given piece; assumes the autodoc index has been
// set successfully and mutex is held
int CNavHelper::LookupMontStageOffset(KImageStore *storeMRC, MontParam *param, int ix, 
                                      int iy, std::vector<int> &pieceSavedAt, 
                                      float &montErrX, float &montErrY)
{
  int iz = pieceSavedAt[iy + ix * param->yNframes];
  if (iz < 0)
    return 1;
  if (AdocGetTwoFloats(storeMRC->getStoreType() == STORE_TYPE_ADOC ? ADOC_IMAGE : 
    ADOC_ZVALUE, iz, ADOC_STAGEOFF, &montErrX, &montErrY))
    return 1;
  return 0;
}

// Finds a value for stage offset for the given position delX, delY relative to the
// center of the montage by interpolating from 4 surrounding piece centers
void CNavHelper::InterpMontStageOffset(KImageStore *imageStore, MontParam *montP,
                                       ScaleMat aMat,std::vector<int> &pieceSavedAt, 
                                       float delX, float delY, float &montErrX, 
                                       float &montErrY)
{
  int adocInd, pcxlo, pcxhi, pcylo, pcyhi;
  float pcX, pcY, cenX, cenY, fx, fy;
  float errX00, errX01, errX10, errX11, errY00, errY01, errY10, errY11;
  montErrX = montErrY = 0.;
  cenX = (aMat.xpx * delX + aMat.xpy * delY) / montP->binning;
  cenY = (aMat.ypx * delX + aMat.ypy * delY) / montP->binning;

  // Get fractional piece position, where 0,0 is middle of lower left piece
  pcX = (float)(cenX / (montP->xFrame - montP->xOverlap) + (montP->xNframes - 1.) / 2.);
  pcY = (float)(cenY / (montP->yFrame - montP->yOverlap) + (montP->yNframes - 1.) / 2.);

  // Determine lower and upper piece index and interpolation fraction
  fx = 0.;
  if (pcX < 0.) {
    pcxlo = pcxhi = 0;
  } else if (pcX >= montP->xNframes - 1.) {
    pcxlo = pcxhi = montP->xNframes - 1;
  } else {
    pcxlo = (int)pcX;
    pcxhi = pcxlo + 1;
    fx = pcX - pcxlo;
  }
  fy = 0.;
  if (pcY < 0.) {
    pcylo = pcyhi = 0;
  } else if (pcY >= montP->yNframes - 1.) {
    pcylo = pcyhi = montP->yNframes - 1;
  } else {
    pcylo = (int)pcY;
    pcyhi = pcylo + 1;
    fy = pcY - pcylo;
  }

  // Try to get the 4 mont errors and interpolate
  adocInd = imageStore->GetAdocIndex();
  if (adocInd >= 0 && AdocGetMutexSetCurrent(adocInd) >= 0) {
    if (!LookupMontStageOffset(imageStore, montP, pcxlo, pcylo, pieceSavedAt, errX00, 
      errY00) && !LookupMontStageOffset(imageStore, montP, pcxhi, pcylo, pieceSavedAt, 
      errX10, errY10) &&
      !LookupMontStageOffset(imageStore, montP, pcxlo, pcyhi, pieceSavedAt, errX01, errY01)
      && !LookupMontStageOffset(imageStore, montP, pcxhi, pcyhi, pieceSavedAt, errX11,
      errY11)) {
        montErrX = (1.f - fy) * ((1.f - fx) * errX00 + fx * errX10) + 
          fy * ((1.f - fx) * errX01 + fx * errX11);
        montErrY = (1.f - fy) * ((1.f - fx) * errY00 + fx * errY10) + 
          fy * ((1.f - fx) * errY01 + fx * errY11);
    }
    AdocReleaseMutex();
  }
}

// Rotating the image in the given buffer by current rotation matrix, or skip if identity 
int CNavHelper::RotateForAligning(int bufNum)
{
  int width, height, err;
  float sizingFrac = 0.5f;
  EMimageBuffer *imBuf = &mImBufs[bufNum];
  if (fabs(mRIrMat.xpx - 1.) < 1.e-6 && fabs(mRIrMat.xpy) < 1.e-6 && 
    fabs(mRIrMat.ypx) < 1.e-6 && fabs(mRIrMat.ypy - 1.) < 1.e-6)
    return 0;

  width = imBuf->mImage->getWidth();
  height = imBuf->mImage->getHeight();
  err = TransformBuffer(imBuf, mRIrMat, width, height, sizingFrac, mRIrMat);
  if (err)
    AfxMessageBox("Insufficient memory to rotate the image for aligning", MB_EXCLAME);
  return err;
}

// Transforms the image in imBuf by the matrix in rMat.  Determines the size of the new
// image by applying the matrix in sizingMat to the sizes in sizingWidth and sizingHeight
// and then allowing the fraction sizingFrac of the implied increase in size
int CNavHelper::TransformBuffer(EMimageBuffer * imBuf, ScaleMat sizingMat, 
                                int sizingWidth, int sizingHeight, float sizingFrac,
                                ScaleMat rMat)
{
  int width, height, sizeX, sizeY, tmpS, type;
  void *array;

  sizeX = (int)fabs(sizingMat.xpx * sizingWidth + sizingMat.xpy * sizingHeight);
  tmpS = (int)fabs(sizingMat.xpx * sizingWidth - sizingMat.xpy * sizingHeight);
  sizeX = (int)(sizingFrac * (B3DMAX(sizeX, tmpS) - sizingWidth) + sizingWidth);
  sizeY = (int)fabs(sizingMat.ypx * sizingWidth + sizingMat.ypy * sizingHeight);
  tmpS = (int)fabs(sizingMat.ypx * sizingWidth - sizingMat.ypy * sizingHeight);
  sizeY = (int)(sizingFrac * (B3DMAX(sizeY, tmpS) - sizingHeight) + sizingHeight);
  sizeX = 4 * (sizeX / 4);
  sizeY = 2 * (sizeY / 2);

  // Get the array for the rotation
  type = imBuf->mImage->getType();
  if (type == kUBYTE) {
    NewArray(array, unsigned char, sizeX * sizeY);
  } else if (type == kRGB) {
    NewArray(array, unsigned char, 3 * sizeX * sizeY);
  } else {
    NewArray(array, short int, sizeX * sizeY);
  }
  if (!array)
    return 2;

  // Get the real rotation matrix and do rotation
  imBuf->mImage->Lock();
  width = imBuf->mImage->getWidth();
  height = imBuf->mImage->getHeight();
  XCorrFastInterp(imBuf->mImage->getData(), type, array, width, height, sizeX, sizeY,
    rMat.xpx, rMat.xpy, rMat.ypx, rMat.ypy, width / 2.f, height / 2.f, 0., 0.);
  imBuf->mImage->UnLock();

  // This buffer could be anywhere, so let's do all the replacement steps here
  imBuf->DeleteImage();
  if (type == kUBYTE)
    imBuf->mImage = new KImage;
  else if (type == kRGB)
    imBuf->mImage = new KImageRGB;
  else {
    imBuf->mImage = new KImageShort;
    imBuf->mImage->setType(type);
  }
  imBuf->mImage->useData((char *)array, sizeX, sizeY);
  imBuf->SetImageChanged(1);
  return 0;
}

// Returns distance from ptX,ptY to line segment between x1,y1 and x2,y2
float CNavHelper::PointSegmentDistance(float ptX, float ptY, float x1, float y1, 
                                          float x2, float y2)
{
  float tmin, distsq, dx, dy;
  tmin=((ptX-x1)*(x2-x1)+(ptY-y1)*(y2-y1))/((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
  tmin=B3DMAX(0.f,B3DMIN(1.f,tmin));
  dx = (x1+tmin*(x2-x1)-ptX);
  dy = (y1+tmin*(y2-y1)-ptY);
  distsq = dx * dx + dy * dy;
  if (distsq < 0)
    return 0.;
  return (float)sqrt(distsq);
}

// Compute the minimum distance to any edge of a montage from the center of the given
// piece.  Requires that mPieceSavedAt already be filled in for the section
float CNavHelper::PieceToEdgeDistance(MontParam * montP, int ixPiece, int iyPiece)
{
  int ind, j, xFrameDel, yFrameDel, ix, iy; 
  float xpcen, ypcen, xCen, yCen, x1, x2, y1, halfX, halfY, dist, distMin, y2;
  int ixDir[] = {-1, 0, 1, 0};
  int iyDir[] = {0, -1, 0, 1};

  xFrameDel = (montP->xFrame - montP->xOverlap);
  yFrameDel =  (montP->yFrame - montP->yOverlap);
  halfX = montP->xFrame / 2.f;
  halfY = montP->yFrame / 2.f;
  xCen = ixPiece * xFrameDel + halfX;
  yCen = iyPiece * yFrameDel + halfY;
  distMin = 100000000.;
  for (ix = 0; ix < montP->xNframes; ix++) {
    for (iy = 0; iy < montP->yNframes; iy++) {
      ind = iy + ix * montP->yNframes;
      if (mPieceSavedAt[ind] < 0)
        continue;
      xpcen = ix * xFrameDel + halfX;
      ypcen = iy * yFrameDel + halfY;
      for (j = 0; j < 4; j++) {

        // Loop over the 4 directions; if there is no piece in a direction,
        // compose the coordinates of the edge and get distance to edge segment
        ind = iy + iyDir[j] + (ix + ixDir[j]) * montP->yNframes;
        if (ix + ixDir[j] < 0 || ix + ixDir[j] >= montP->xNframes || 
          iy + iyDir[j] < 0 || iy + iyDir[j] >= montP->yNframes ||
          mPieceSavedAt[ind] < 0) {
            if (ixDir[j]) {
              x1 = x2 = xpcen + ixDir[j] * halfX;
            } else {
              x1 = xpcen - halfX;
              x2 = xpcen + halfX;
            }
            if (iyDir[j]) {
              y1 = y2 = ypcen + iyDir[j] * halfY;
            } else {
              y1 = ypcen - halfY;
              y2 = ypcen + halfY;
            }
            dist = PointSegmentDistance(xCen, yCen, x1, y1, x2, y2);
            distMin = B3DMIN(dist, distMin);
          }
      }
    }
  }
  return distMin;
}

// Return the last stage error and the registration it was taken at
int CNavHelper::GetLastStageError(float &stageErrX, float &stageErrY, float &localErrX,
    float &localErrY)
{
  stageErrX = mStageErrX;
  stageErrY = mStageErrY;
  localErrX = mLocalErrX;
  localErrY = mLocalErrY;
  return mRegistrationAtErr;
}

// Return the target position where last stage error was found, and ID of map used
int CNavHelper::GetLastErrorTarget(float & targetX, float & targetY)
{
  targetX = mRIabsTargetX;
  targetY = mRIabsTargetY;
  return mRImapID;
}

// Function to change registrations of all buffers matching the mapID
void CNavHelper::ChangeAllBufferRegistrations(int mapID, int fromReg, int toReg)
{
  POSITION pos = mWinApp->mDocWnd->GetFirstViewPosition();
  while (pos != NULL) {
    CSerialEMView *pView = (CSerialEMView *)mWinApp->mDocWnd->GetNextView(pos);
    pView->ChangeAllRegistrations(mapID, fromReg, toReg);
  }
}

// Get the image and beam shift and beam tilt for View from current parameters
void CNavHelper::GetViewOffsets(CMapDrawItem * item, float &netShiftX, 
  float &netShiftY, float &beamShiftX, float & beamShiftY, float &beamTiltX, 
  float &beamTiltY)
{
  double shiftX, shiftY;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  beamTiltX = beamTiltY = 0.;
  mWinApp->mLowDoseDlg.GetNetViewShift(shiftX, shiftY);
  SimpleIStoStage(item, shiftX, shiftY, netShiftX, netShiftY);

  // Store incremental beam shift and tilt if any
  beamShiftX = (float)(ldParm[VIEW_CONSET].beamDelX - ldParm[RECORD_CONSET].beamDelX);
  beamShiftY = (float)(ldParm[VIEW_CONSET].beamDelY - ldParm[RECORD_CONSET].beamDelY);
  if (mScope->GetLDBeamTiltShifts()) {
    beamTiltX = (float)(ldParm[VIEW_CONSET].beamTiltDX - 
      ldParm[RECORD_CONSET].beamTiltDX);
    beamTiltY = (float)(ldParm[VIEW_CONSET].beamTiltDY - 
      ldParm[RECORD_CONSET].beamTiltDY);
  }
}

// Convert IS to a stage position: for the callers to this, it works better to use 0-focus
// stage calibration instead of defocused one
void CNavHelper::SimpleIStoStage(CMapDrawItem * item, double ISX, double ISY, 
  float &stageX, float &stageY)
{
  ScaleMat aMat, bMat;
  stageX = stageY = 0.;
  aMat = mShiftManager->IStoGivenCamera(item->mMapMagInd, item->mMapCamera);
  if (aMat.xpx && (ISX || ISY)) {
    //aMat = MatMul(aMat, MatInv(ItemStageToCamera(item)));
    bMat = MatInv(mShiftManager->StageToCamera(item->mMapCamera, item->mMapMagInd));
    if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
      mShiftManager->AdjustCameraToStageForTilt(bMat, item->mMapTiltAngle);
    aMat = MatMul(aMat, bMat);
    stageX = (float)(aMat.xpx * ISX + aMat.xpy * ISY);
    stageY = (float)(aMat.ypx * ISX + aMat.ypy * ISY);
  }
}

// Get the stage to camera matrix for an item, adjusting for defocus
ScaleMat CNavHelper::ItemStageToCamera(CMapDrawItem * item)
{
  ScaleMat aMat;
  if (item->mMapLowDoseConSet == VIEW_CONSET)
    aMat = mShiftManager->FocusAdjustedStageToCamera(item->mMapCamera, item->mMapMagInd,
      item->mMapSpotSize, item->mMapProbeMode, item->mMapIntensity, item->mDefocusOffset);
  else
    aMat = mShiftManager->StageToCamera(item->mMapCamera, item->mMapMagInd);
  if (item->mMapTiltAngle > RAW_STAGE_TEST && fabs((double)item->mMapTiltAngle) > 1)
    mShiftManager->AdjustStageToCameraForTilt(aMat, item->mMapTiltAngle);
  return aMat;
}

// Get the coordinates for a camera set from the state parameters
void CNavHelper::StateCameraCoords(int camIndex, int xFrame, int yFrame, int binning,
  int &left, int &right, int &top, int &bottom)
{
  CameraParameters *camP = mWinApp->GetCamParams() + camIndex;
  mCamera->CenteredSizes(xFrame, camP->sizeX, camP->moduloX, left, right, 
    yFrame, camP->sizeY, camP->moduloY, top, bottom, binning, camIndex);
  left *= binning;
  right *= binning;
  top *= binning;
  bottom *= binning;
}

/////////////////////////////////////////////////////
/////  SETTING FILES, TS PARAMS, STATES FOR ACQUIRES
///////////////////////////////////////////////////// 

// Generate the next filename in a series given a filename
// The rules are that digits before an extension or at the end of the filename,
// or digits followed by one non-digit before the extension or at the end,
// will be incremented by 1 and output with the same number of digits or more if needed
// If there are no digits, a "2" is added before the extension or at the end of the name
CString CNavHelper::NextAutoFilename(CString inStr)
{
  int dirInd, extInd, digInd, curnum, numdig, numroot;
  CString str, format, ext = "";
  dirInd = inStr.ReverseFind('\\');
  extInd = inStr.ReverseFind('.');
  if (extInd > dirInd && extInd > 0) {
    ext = inStr.Right(inStr.GetLength() - extInd);
    inStr = inStr.Left(extInd);
  }
  str = inStr;
  str = str.MakeReverse();
  digInd = str.FindOneOf("0123456789");
  if (digInd == 0 || digInd == 1) {
    if (digInd == 1)
      str = str.Right(str.GetLength() - 1);
    str = str.SpanIncluding("0123456789");
    str = str.MakeReverse();
    curnum = atoi((LPCTSTR)str);
    numdig = str.GetLength();
    format.Format("%%0%dd", numdig);
    str.Format((LPCTSTR)format, curnum + 1);
    numroot = inStr.GetLength() - numdig - digInd;
    if (numroot) 
      str = inStr.Left(numroot) + str;
    if (digInd == 1)
      str += inStr.GetAt(inStr.GetLength() - 1);
    str += ext;
  } else {
    str = inStr + "2" + ext;
  }
  return str;
}

// Check whether the name is already used in the lists of ones to open
bool CNavHelper::NameToOpenUsed(CString name)
{
  CMapDrawItem *item;
  ScheduledFile *sched;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (!item->mFileToOpen.IsEmpty() && item->mFileToOpen == name)
      return true;
  }
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched = mGroupFiles->GetAt(i);
    if (sched->filename == name)
      return true;
  }
  return false;
}

// Set up a new file to open for an item or group.  Returns -1 if user cancels, or >0
// for other errors; cleans up references in either case 
int CNavHelper::NewAcquireFile(int itemNum, int listType, ScheduledFile *sched)
{
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  ScheduledFile *sched2;
  StateParams *state;
  MontParam *montp;
  int i, index;
  bool breakForFit = false;
  int *propIndexp = &item->mFilePropIndex;
  int *montIndexp = &item->mMontParamIndex;
  int *stateIndexp = &item->mStateIndex;
  CString autoName = "";
  CString *namep = &item->mFileToOpen;
  if (listType == NAVFILE_GROUP) {
    propIndexp = &sched->filePropIndex;
    montIndexp = &sched->montParamIndex;
    stateIndexp = &sched->stateIndex;
    namep = &sched->filename;
  }
  *propIndexp = *montIndexp = *stateIndexp = -1;

  // First check for low dose state mismatch for tilt series
  if (listType == NAVFILE_TS) {
    for (i = itemNum - 1; i >= 0; i--) {
      item2 = mItemArray->GetAt(i);
      if (item2->mTSparamIndex >= 0 && item2->mStateIndex >= 0) {
        state = mAcqStateArray->GetAt(item2->mStateIndex);
        if (state->lowDose && !mWinApp->LowDoseMode() || 
          !state->lowDose && mWinApp->LowDoseMode()) {
          autoName.Format("Low dose mode is currently %s but it is %s in the imaging "
            "state\nparameters of a previous item selected for a tilt series.\n\n"
            "If you do want to change modes between series, you should set the imaging\n"
            "state for this series and also set the tilt series parameters.",
            state->lowDose ? "OFF" : "ON", state->lowDose ? "ON" : "OFF");
          AfxMessageBox(autoName, MB_OK | MB_ICONINFORMATION);
        }
        break;
      }
    }
  }

  // First look for a prior item of the same kind to get names/references from
  for (i = itemNum - 1; i >= 0; i--) {
    item2 = mItemArray->GetAt(i);
    if (listType == NAVFILE_TS) {
      if (item2->mTSparamIndex >= 0) {
        item->mTSparamIndex = item2->mTSparamIndex;
        *propIndexp = item2->mFilePropIndex;
        *montIndexp = item2->mMontParamIndex;
        autoName = NextAutoFilename(item2->mFileToOpen);
        ChangeRefCount(NAVARRAY_TSPARAM, item->mTSparamIndex, 1);
      }
    } else if (item2->mTSparamIndex < 0) {
      if (item2->mFilePropIndex >= 0) {
        
        // Force a new parameter set if last one is fitting to polygon and this is a 
        // polygon item - otherwise let it inherit the fit
        autoName = NextAutoFilename(item2->mFileToOpen);
        if (item2->mMontParamIndex >= 0) {
          montp = mMontParArray->GetAt(item2->mMontParamIndex);
          breakForFit = montp->wasFitToPolygon && item->mType == ITEM_TYPE_POLYGON;
        }
        *propIndexp = item2->mFilePropIndex;
        *montIndexp = item2->mMontParamIndex;
      } else {
        index = mNav->GroupScheduledIndex(item2->mGroupID);
        if (index >= 0) {
          sched2 = mGroupFiles->GetAt(index);
          *propIndexp = sched2->filePropIndex;
          *montIndexp = sched2->montParamIndex;
          autoName = NextAutoFilename(sched2->filename);
        }
      }
    }

    // If succeeded, increment the remaining reference counts
    if (*propIndexp >= 0) {
      ChangeRefCount(NAVARRAY_FILEOPT, *propIndexp, 1);
      if (*montIndexp >= 0) 
        ChangeRefCount(NAVARRAY_MONTPARAM, *montIndexp, 1);

      // Check for uniqueness of fileness
      while (NameToOpenUsed(autoName) || mDocWnd->StoreIndexFromName(autoName) >= 0)
        autoName = NextAutoFilename(autoName);
      *namep = autoName;
      if (breakForFit)
        break;
      return 0;
    }
  }

  // If nothing found, need to create file options and filename
  index = SetFileProperties(itemNum, listType, sched);
  if (index) {
    *propIndexp = *montIndexp = -1;
    RecomputeArrayRefs();
    return index;
  }

  // If there is a problem with the name, abort the whole thing
  if (autoName.IsEmpty()) {
    index = SetOrChangeFilename(itemNum, listType, sched);
    if (index) {
      *propIndexp = *montIndexp = -1;
      RecomputeArrayRefs();
      return index;
    }
  } else {

    // But use the name if one was inherited for a new poly fit
    while (NameToOpenUsed(autoName) || mDocWnd->StoreIndexFromName(autoName) >= 0)
      autoName = NextAutoFilename(autoName);
    *namep = autoName;
  }

  // Do TS params last so the montaging is already known
  if (listType == NAVFILE_TS) {
    index = SetTSParams(itemNum);
    if (index) {
      *propIndexp = *montIndexp = -1;
      *namep = "";
      RecomputeArrayRefs();
    }
  }

  return index;
}

// Set or adjust tilt series params
int CNavHelper::SetTSParams(int itemNum)
{
  TiltSeriesParam *tsp, *tspSrc;
  MontParam *montp;
  int err, i;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  bool needNew = false;
  int future = 1, futureLDstate = -1, ldMagInd = 0;
  int prevIndex = item->mTSparamIndex;
  CString inheritors;

  // If parameters already exist, copy them to the master; otherwise just edit the master
  if (prevIndex >= 0) {
    tspSrc = mTSparamArray->GetAt(prevIndex);
    mWinApp->mTSController->CopyParamToMaster(tspSrc, false);
    future = 2;
  }
  
  // Set some parameters if montaging: magLocked indicates montaging
  tsp = mWinApp->mTSController->GetTiltSeriesParam();
  tsp->magLocked = item->mMontParamIndex >= 0;
  if (tsp->magLocked) {
    montp = mMontParArray->GetAt(item->mMontParamIndex);
    mWinApp->mTSController->MontageMagIntoTSparam(montp, tsp);      
    tsp->binning = montp->binning;
  }

  // Find the last state set for a TS param and set the future LD state from it
  for (i = itemNum; i>= 0; i--) {
    item2 = mItemArray->GetAt(i);
    if (item2->mTSparamIndex >= 0 && item2->mStateIndex >= 0) {
      StateParams *state = mAcqStateArray->GetAt(item2->mStateIndex);
      futureLDstate = state->lowDose ? 1 : 0;
      if (state->lowDose)
        ldMagInd = state->ldParams.magIndex;
      break;
    }
  }

  err = mWinApp->mTSController->SetupTiltSeries(future, futureLDstate, ldMagInd);
  if (err)
    return err;

  // Now that we have success, either copy it back to source or make a new one
  if (item->mTSparamIndex < 0)
    needNew = true;
  else {

    // See if this is a reference shared with an earlier item, if so split it off
    for (i = 0; i < itemNum && !needNew; i++) {
      item2 = mItemArray->GetAt(i);
      needNew = item2->mTSparamIndex == prevIndex;
      if (needNew)
        NoLongerInheritMessage(itemNum, i, "tilt series parameters");
    }
  }

  if (prevIndex >= 0)
    AddInheritingItems(itemNum, prevIndex, NAVARRAY_TSPARAM, inheritors);

  if (needNew) {
    tsp = new TiltSeriesParam;
    mWinApp->mTSController->CopyMasterToParam(tsp, false);
    mTSparamArray->Add(tsp);
    tsp->refCount = 0;
 
    // For this item and all following ones with the same original index, assign to the
    // new index and adjust the reference counts
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);
      if (item2->mTSparamIndex == prevIndex && (i == itemNum || prevIndex >= 0)) {
        if (prevIndex >= 0)
          ChangeRefCount(NAVARRAY_TSPARAM, prevIndex, -1);
        item2->mTSparamIndex = (int)mTSparamArray->GetSize() - 1;
        tsp->refCount++;
      }
    }
  } else
    mWinApp->mTSController->CopyMasterToParam(tspSrc, false);

  BeInheritedByMessage(itemNum, inheritors, "Changes in tilt series parameters");
  return 0;
}

// Set the file properties for a file to be opened for item or group, either by editing
// existing properties for that item or by cloning shared properties and editing those
int CNavHelper::SetFileProperties(int itemNum, int listType, ScheduledFile *sched)
{
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2;
  ScheduledFile *sched2;
  CNavFileTypeDlg typeDlg;
  MontParam *montpSrc = mWinApp->GetMontParam();
  MontParam *newMontp;
  FileOptions *fileOptSrc = mDocWnd->GetFileOpt();
  FileOptions *newFileOpt;
  CString inheritors;
  bool madeNewOpt = false, gotThisSched = false;
  int movingStage, prevIndex, i, j, sharedInd;
  int *propIndexp = &item->mFilePropIndex;
  int *montIndexp = &item->mMontParamIndex;
  prevIndex = *montIndexp;
  if (listType == NAVFILE_GROUP) {
    propIndexp = &sched->filePropIndex;
    montIndexp = &sched->montParamIndex;
  }
  if (prevIndex >= 0)
    montpSrc = mMontParArray->GetAt(prevIndex);
  if (*propIndexp >= 0)
    fileOptSrc = mFileOptArray->GetAt(*propIndexp);
  typeDlg.m_iSingleMont = mLastTypeWasMont ? 1 : 0;
  if (*propIndexp >= 0)
    typeDlg.m_iSingleMont = prevIndex >= 0 ? 1 : 0;
  typeDlg.mPolyFitOK = item->mType == ITEM_TYPE_POLYGON && listType == NAVFILE_ITEM;
  typeDlg.m_bFitPoly = mLastMontFitToPoly;
  if (prevIndex >= 0 && typeDlg.mPolyFitOK)
    typeDlg.m_bFitPoly = montpSrc->wasFitToPolygon;
  if (typeDlg.DoModal() != IDOK)
    return -1;
  mLastTypeWasMont = typeDlg.m_iSingleMont != 0;
  mLastMontFitToPoly = typeDlg.m_bFitPoly;

  AddInheritingItems(itemNum, prevIndex, NAVARRAY_MONTPARAM, inheritors);

  // If it is now supposed to be a montage, get parameters from existing param or
  // master ones, do dialog, then save params
  if (typeDlg.m_iSingleMont) {
    CMontageSetupDlg montDlg;
    montDlg.mParam = *montpSrc;
    if (prevIndex < 0)
      mDocWnd->InitMontParamsForDialog(&montDlg.mParam, false);

    montDlg.mSizeLocked = false;
    montDlg.mConstrainSize = false;
    if (listType == NAVFILE_TS)
      montDlg.mParam.moveStage = false;
    if (typeDlg.m_bFitPoly && typeDlg.mPolyFitOK) {

      // This assumes that it is the current item
      if (mNav->PolygonMontage(&montDlg))
        return -1;
      montDlg.mParam.wasFitToPolygon = true;
    } else if (montDlg.DoModal() != IDOK)
      return -1;

    // Set some params after the dialog: note offerToMakeMap is irrelevant in Nav acquire
    montDlg.mParam.verySloppy = montDlg.mParam.moveStage;
    montDlg.mParam.warnedCalOpen = true;
    montDlg.mParam.warnedCalAcquire = true;
    movingStage = montDlg.mParam.moveStage ? 1 : 0;

    // Need to make a new param if there is not one in array, or if the reference count
    // was greater than 1 and it is shared with earlier ones
    if (prevIndex < 0 || (montDlg.mParam.refCount > 1 && 
      EarlierItemSharesRef(itemNum, prevIndex, NAVARRAY_MONTPARAM, sharedInd))) {
          newMontp = new MontParam;
        *newMontp = montDlg.mParam;
        mMontParArray->Add(newMontp);
        newMontp->refCount = 0;
        if (prevIndex >= 0 && sharedInd >= 0)
          NoLongerInheritMessage(itemNum, sharedInd, "montage parameters");
        for (i = itemNum; i < mItemArray->GetSize(); i++) {
          item2 = mItemArray->GetAt(i);
          
          // Adjust indices of inheriting items and item itself if not doing a group file
          if (item2->mMontParamIndex == prevIndex && (prevIndex >= 0 ||
            (i == itemNum && listType != NAVFILE_GROUP))) {
              if (prevIndex >= 0)
                montpSrc->refCount--;
              item2->mMontParamIndex = (int)mMontParArray->GetSize() - 1;
              newMontp->refCount++;
          }

          // Adjust indices of group files
          j = mNav->GroupScheduledIndex(item2->mGroupID);
          if (j >= 0) {
            sched2 = mGroupFiles->GetAt(j);
            if (sched2->montParamIndex == prevIndex && (prevIndex >= 0 ||
              (sched2 == sched && listType == NAVFILE_GROUP))) {
                if (prevIndex >= 0)
                  montpSrc->refCount--;
                sched2->montParamIndex = (int)mMontParArray->GetSize() - 1;
                newMontp->refCount++;
                if (sched2 == sched)
                  gotThisSched = true;
            }
          }
        }
        if (listType == NAVFILE_GROUP && !gotThisSched) {
          sched->montParamIndex = (int)mMontParArray->GetSize() - 1;
          newMontp->refCount++;
        }
    } else
      *montpSrc = montDlg.mParam;
    BeInheritedByMessage(itemNum, inheritors, "Changes in montage parameters");

  } else if (prevIndex >= 0) {

    // If it used to be a montage and is no more, reduce reference count and set index to
    // -1 for each item
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);
      if (item2->mMontParamIndex == prevIndex) {
        ChangeRefCount(NAVARRAY_MONTPARAM, prevIndex, -1);
        item2->mMontParamIndex = -1;
      }
    }
    BeInheritedByMessage(itemNum, inheritors, "Changing from montage to single frame");
    movingStage = -1;
  }
  inheritors = "";
  sharedInd = -1;

  // Now do file options dialog but create a new copy first if needed
  newFileOpt = fileOptSrc;
  prevIndex = *propIndexp;
  if (prevIndex < 0 || (fileOptSrc->refCount > 1 && 
    EarlierItemSharesRef(itemNum, prevIndex, NAVARRAY_FILEOPT, sharedInd))) {
    newFileOpt = new FileOptions;
    mDocWnd->SetFileOptsForSTEM();
    *newFileOpt = *fileOptSrc;
    mDocWnd->RestoreFileOptsFromSTEM();
    newFileOpt->refCount = 0;
    madeNewOpt = true;
  }

  // Prepare the file options as appropriate for single frame or montage
  newFileOpt->TIFFallowed = 0;
  newFileOpt->montageInMdoc = false;
  newFileOpt->typext &= ~MONTAGE_MASK;
  if (*montIndexp < 0) {
    if (movingStage < 0)
      newFileOpt->typext &= ~VOLT_XY_MASK;
  } else {
    newFileOpt->mode = B3DMAX(newFileOpt->mode, 1);
    newMontp = mMontParArray->GetAt(*montIndexp);
    if ((newMontp->xNframes - 1) * (newMontp->xFrame - newMontp->xOverlap) > 65535 ||
      (newMontp->yNframes - 1) * (newMontp->yFrame - newMontp->yOverlap) > 65535)
      newFileOpt->montageInMdoc = true;
    else
      newFileOpt->typext |= MONTAGE_MASK;
    if (movingStage > 0)
      newFileOpt->typext |= VOLT_XY_MASK;
  }

  if (mDocWnd->FilePropForSaveFile(newFileOpt)) {
    if (madeNewOpt)
      delete newFileOpt;
    return -1;
  }

  if (prevIndex >= 0 && sharedInd >= 0)
    NoLongerInheritMessage(itemNum, sharedInd, "file properties");
  AddInheritingItems(itemNum, prevIndex, NAVARRAY_FILEOPT, inheritors);

  // For a new file option, add it to the array and dereference this item and any 
  // following ones that shared it from the old options and point them to the new one
  if (madeNewOpt) {
    mFileOptArray->Add(newFileOpt);
    gotThisSched = false;
    for (i = itemNum; i < mItemArray->GetSize(); i++) {
      item2 = mItemArray->GetAt(i);

      // Do the item itself only if not doing a file at group
      if (item2->mFilePropIndex == prevIndex && (prevIndex >= 0 ||
        (i == itemNum && listType != NAVFILE_GROUP))) {
          if (prevIndex >= 0)
            fileOptSrc->refCount--;
          item2->mFilePropIndex = (int)mFileOptArray->GetSize() - 1;
          newFileOpt->refCount++;
      }

      // Check and switch any scheduled groups too, keep track if do the passed schedule
      // A new one is not yet in array
      j = mNav->GroupScheduledIndex(item2->mGroupID);
      if (j >= 0) {
        sched2 = mGroupFiles->GetAt(j);
        if (sched2->filePropIndex == prevIndex && (prevIndex >= 0 ||
          (sched2 == sched && listType == NAVFILE_GROUP))) {
            if (prevIndex >= 0)
              fileOptSrc->refCount--;
            sched2->filePropIndex = (int)mFileOptArray->GetSize() - 1;
            newFileOpt->refCount++;
            if (sched2 == sched)
              gotThisSched = true;
        }
      }
    }
    if (listType == NAVFILE_GROUP && !gotThisSched) {
      sched->filePropIndex = (int)mFileOptArray->GetSize() - 1;
      newFileOpt->refCount++;
    }
  }
  BeInheritedByMessage(itemNum, inheritors, "Changes in file properties");

  // A successful pass through the dialog should (?) update the program master 
  mDocWnd->CopyMasterFileOpts(newFileOpt, COPY_TO_MASTER);
  return 0;
}

// Get a new filename for the item or group, defaulting the dialog to an existing one
// Return -1 if user cancels the dialog, or 1 if file already open, 2 if it is scheduled
// for a group, or 3 if it is scheduled for an item
int CNavHelper::SetOrChangeFilename(int itemNum, int listType, ScheduledFile *sched)
{
  int i, numacq, fileType = STORE_TYPE_MRC;
  CString filename, label, lastlab;
  ScheduledFile *sched2;
  CMapDrawItem *item2;
  LPCTSTR lpszFileName = NULL;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  CString *namep = &item->mFileToOpen;
  FileOptions *fileOptp = NULL;
  i = item->mFilePropIndex;
  if (listType == NAVFILE_GROUP) {
    namep = &sched->filename;
    i = sched->filePropIndex;
  }
  if (i >= 0) {
    fileOptp = mFileOptArray->GetAt(i);
    fileType = fileOptp->useMont() ? fileOptp->montFileType : fileOptp->fileType;
  }
  filename = *namep;
  if (!namep->IsEmpty())
    lpszFileName = (LPCTSTR)filename;
  if (mDocWnd->FilenameForSaveFile(fileType, lpszFileName, filename))
    return -1;

    // Check for file already open
  i = mDocWnd->StoreIndexFromName(filename);
  if (i >= 0) {
    label.Format("This file is already open (file #%d).\n\nPick another file name,"
      " or close the file (in which case it will be overwritten)", i + 1);
    AfxMessageBox(label, MB_EXCLAME);
    return 1;
  }

  // Check for duplication with group files
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched2 = mGroupFiles->GetAt(i);
    if (listType == NAVFILE_GROUP && sched2->groupID == sched->groupID)
      continue;
    if (sched2->filename == filename) {
      mNav->CountItemsInGroup(sched2->groupID, label, lastlab, numacq);
      AfxMessageBox("This filename is already set up for the group\n"
        "with labels from " + label + " to " + lastlab + "\n\nPick another name.",
        MB_EXCLAME);
      return 2;
    }
  }

  // Check for duplication with other defined files for items
  for (i = 0; i < mItemArray->GetSize(); i++) {
    if (i == itemNum)
      continue;
    item2 = mItemArray->GetAt(i);
    if (item2->mFilePropIndex >= 0 && item2->mFileToOpen == filename) {
      label.Format("This filename is already set up to be opened for item #%d with"
        " label %s\r\nPick another name.", i, (LPCTSTR)item2->mLabel);
      AfxMessageBox(label, MB_EXCLAME);
      return 3;
    }
  }

  // Copy name and return for success
  *namep = filename;
  return 0;
}

void CNavHelper::NoLongerInheritMessage(int itemNum, int sharedInd, char * typeText)
{
  CString str;
  CMapDrawItem *item1 = mItemArray->GetAt(itemNum);
  CMapDrawItem *item2 = mItemArray->GetAt(sharedInd);
  str.Format("Item # %d (%s) no longer inherits changes in %s from item # %d (%s)", 
    itemNum + 1, (LPCTSTR)item1->mLabel, typeText, sharedInd + 1, (LPCTSTR)item2->mLabel);
  mWinApp->AppendToLog(str);
}

void CNavHelper::AddInheritingItems(int num, int prevIndex, int type, CString & listStr)
{
  CString str;
  CMapDrawItem *item;
  if (prevIndex < 0)
    return;
  for (int i = num + 1; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if ((type == NAVARRAY_FILEOPT && item->mFilePropIndex == prevIndex) ||
      (type == NAVARRAY_MONTPARAM && item->mMontParamIndex == prevIndex) ||
      (type == NAVARRAY_TSPARAM && item->mTSparamIndex == prevIndex)) {
        str.Format("%d (%s)", i + 1, (LPCTSTR)item->mLabel);
        if (!listStr.IsEmpty())
          listStr += ", ";
        listStr += str;
    }
  }
}

void CNavHelper::BeInheritedByMessage(int itemNum, CString & listStr, char * typeText)
{
  CString str;
  CMapDrawItem *item = mItemArray->GetAt(itemNum);
  if (listStr.IsEmpty())
    return;
  str.Format("%s for item # %d (%s) are being inherited by these items:", 
    typeText, itemNum + 1, (LPCTSTR)item->mLabel);
  mWinApp->AppendToLog(str);
  mWinApp->AppendToLog("     " + listStr);
}

// Remove a param from one of the arrays and delete it, and decrease any references to 
// ones above it
void CNavHelper::RemoveFromArray(int which, int index)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (which == NAVARRAY_FILEOPT && item->mFilePropIndex > index)
      item->mFilePropIndex--;
    if (which == NAVARRAY_MONTPARAM && item->mMontParamIndex > index)
      item->mMontParamIndex--;
    if (which == NAVARRAY_TSPARAM && item->mTSparamIndex > index)
      item->mTSparamIndex--;
    if (which == NAVARRAY_STATE && item->mStateIndex > index)
      item->mStateIndex--;
  }
  for (i = 0; i < mGroupFiles->GetSize() && which != NAVARRAY_TSPARAM; i++) {
    sched = mGroupFiles->GetAt(i);
    if (which == NAVARRAY_FILEOPT && sched->filePropIndex > index)
      sched->filePropIndex--;
    if (which == NAVARRAY_MONTPARAM && sched->montParamIndex > index)
      sched->montParamIndex--;
    if (which == NAVARRAY_STATE && sched->stateIndex > index)
      sched->stateIndex--;
  }
  if (which == NAVARRAY_FILEOPT) {
    FileOptions *opt = mFileOptArray->GetAt(index);
    delete opt;
    mFileOptArray->RemoveAt(index);
  } else if (which == NAVARRAY_MONTPARAM) {
    MontParam *montp = mMontParArray->GetAt(index);
    delete montp;
    mMontParArray->RemoveAt(index);
  } else if (which == NAVARRAY_TSPARAM) {
    TiltSeriesParam *tsp = mTSparamArray->GetAt(index);
    delete tsp;
    mTSparamArray->RemoveAt(index);
  } else if (which == NAVARRAY_STATE) {
    StateParams *state = mAcqStateArray->GetAt(index);
    delete state;
    mAcqStateArray->RemoveAt(index);
  }
}

// Reduce the reference count of the given param, and remove it if it is zero
void CNavHelper::ChangeRefCount(int which, int index, int dir)
{
  int newCount;
  if (which == NAVARRAY_FILEOPT) {
    FileOptions *opt = mFileOptArray->GetAt(index);
    opt->refCount += dir;
    newCount = opt->refCount;
  } else if (which == NAVARRAY_MONTPARAM) {
    MontParam *montp = mMontParArray->GetAt(index);
    montp->refCount += dir;
    newCount = montp->refCount;
  } else if (which == NAVARRAY_TSPARAM) {
    TiltSeriesParam *tsp = mTSparamArray->GetAt(index);
    tsp->refCount += dir;
    newCount = tsp->refCount;
  }
  if (!newCount)
    RemoveFromArray(which, index);
}

// A garbage collection routine to handle cancels that leave things in strange states
void CNavHelper::RecomputeArrayRefs(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  FileOptions *opt;
  MontParam *montp;
  TiltSeriesParam *tsp;
  int i, loop, limit, fileInd, montInd, tsInd;

  // First zero all the references
  for (i = 0; i < mFileOptArray->GetSize(); i++) {
    opt = mFileOptArray->GetAt(i);
    opt->refCount = 0;
  }
  for (i = 0; i < mMontParArray->GetSize(); i++) {
    montp = mMontParArray->GetAt(i);
    montp->refCount = 0;
  }
  for (i = 0; i < mTSparamArray->GetSize(); i++) {
    tsp = mTSparamArray->GetAt(i);
    tsp->refCount = 0;
  }

  // Add up the references again
  limit = (int)mItemArray->GetSize();
  for (loop = 0; loop < 2; loop++) {
    for (i = 0; i < limit; i++) {
      if (loop) {
        sched = mGroupFiles->GetAt(i);
        fileInd = sched->filePropIndex;
        montInd = sched->montParamIndex;
        tsInd = -1;
      } else {
        item = mItemArray->GetAt(i);
        fileInd = item->mFilePropIndex;
        montInd = item->mMontParamIndex;
        tsInd = item->mTSparamIndex;
      }

      if (fileInd >= 0) {
        opt = mFileOptArray->GetAt(fileInd);
        opt->refCount++;
      }
      if (montInd >= 0) {
        montp = mMontParArray->GetAt(montInd);
        montp->refCount++;
      }
      if (tsInd >= 0) {
        tsp = mTSparamArray->GetAt(tsInd);
        tsp->refCount++;
      }
    }
    limit = (int)mGroupFiles->GetSize();
  }

  // Remove items with no references from top down
  for (i = (int)mFileOptArray->GetSize() - 1; i >= 0; i--) {
    opt = mFileOptArray->GetAt(i);
    if (!opt->refCount)
      RemoveFromArray(NAVARRAY_FILEOPT, i);
  }
  for (i = (int)mMontParArray->GetSize() - 1; i >= 0; i--) {
    montp = mMontParArray->GetAt(i);
    if (!montp->refCount)
      RemoveFromArray(NAVARRAY_MONTPARAM, i);
  }
  for (i = (int)mTSparamArray->GetSize() - 1; i >= 0; i--) {
    tsp = mTSparamArray->GetAt(i);
    if (!tsp->refCount)
      RemoveFromArray(NAVARRAY_TSPARAM, i);
  }
}

// See if an earlier item shares a reference to the given type of index
bool CNavHelper::EarlierItemSharesRef(int itemNum, int refInd, int which, int &shareInd)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  int i, index;
  shareInd = -1;
  for (i = 0; i < itemNum; i++) {
    item = mItemArray->GetAt(i);
    if (which == NAVARRAY_FILEOPT && item->mFilePropIndex == refInd ||
      which == NAVARRAY_MONTPARAM && item->mMontParamIndex == refInd) {
        shareInd = i;
        return true;
    }
    index = mNav->GroupScheduledIndex(item->mGroupID);
    if (index >= 0) {
      sched = mGroupFiles->GetAt(index);
      if (which == NAVARRAY_FILEOPT && sched->filePropIndex == refInd ||
        which == NAVARRAY_MONTPARAM && sched->montParamIndex == refInd)
        return true;
    }
  }
  return false;
}

// Takes care of removing references and resetting indexes when either acquire is
// turned off or tilt series or new file at item is. Acquire should already be set false
// if appropriate
void CNavHelper::EndAcquireOrNewFile(CMapDrawItem * item, bool endGroupFile)
{
  ScheduledFile *sched;
  CMapDrawItem *item2;
  int i, index;
  bool active = false;
  if (item->mTSparamIndex >= 0) {
    ChangeRefCount(NAVARRAY_TSPARAM, item->mTSparamIndex, -1);
    item->mTSparamIndex = -1;
  }
  if (item->mFilePropIndex >= 0) {
    ChangeRefCount(NAVARRAY_FILEOPT, item->mFilePropIndex, -1);
    item->mFilePropIndex = -1;
    if (item->mMontParamIndex >= 0)
      ChangeRefCount(NAVARRAY_MONTPARAM, item->mMontParamIndex, -1);
    item->mMontParamIndex = -1;
    item->mFileToOpen = "";
    if (item->mStateIndex >= 0)
      RemoveFromArray(NAVARRAY_STATE, item->mStateIndex);
    item->mStateIndex = -1;
  } else {
    index = mNav->GroupScheduledIndex(item->mGroupID);
    if (index >= 0) {
      
      // Are there other acquire items with the same group ID?
      for (i = 0; i < mItemArray->GetSize(); i++) {
        item2 = mItemArray->GetAt(i);
        if (item2->mGroupID == item->mGroupID && item2->mAcquire) {
          active = true;
          break;
        }
      }
      if (!active || endGroupFile) {
        sched = mGroupFiles->GetAt(index);
        ChangeRefCount(NAVARRAY_FILEOPT, sched->filePropIndex, -1);
        if (sched->montParamIndex >= 0)
          ChangeRefCount(NAVARRAY_MONTPARAM, sched->montParamIndex, -1);
        if (sched->stateIndex >= 0)
          RemoveFromArray(NAVARRAY_STATE, sched->stateIndex);
        delete sched;
        mGroupFiles->RemoveAt(index);
      }
    }
  }
}

// Gets the file type and returns the scheduled file object if any for an item
ScheduledFile * CNavHelper::GetFileTypeAndSchedule(CMapDrawItem * item, int & fileType)
{
  ScheduledFile *sched = NULL;
  int index = mNav->GroupScheduledIndex(item->mGroupID);
  fileType = -1;
  if (item->mTSparamIndex >= 0) {
    fileType = NAVFILE_TS;
  } else if (index >= 0) {
    sched = mGroupFiles->GetAt(index);
    fileType = NAVFILE_GROUP;
  } else if (item->mFilePropIndex >= 0)
    fileType = NAVFILE_ITEM;
  return sched;
}

// Clean up all the Navigator arrays for closing or reading new file
void CNavHelper::DeleteArrays(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  FileOptions *opt;
  MontParam *montp;
  TiltSeriesParam *tsp;
  StateParams *state;
  int i;
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    delete item;
  }   
  mItemArray->RemoveAll();
  for (i = 0; i < mGroupFiles->GetSize(); i++) {
    sched = mGroupFiles->GetAt(i);
    delete sched;
  }   
  mGroupFiles->RemoveAll();
  for (i = 0; i < mFileOptArray->GetSize(); i++) {
    opt = mFileOptArray->GetAt(i);
    delete opt;
  }   
  mFileOptArray->RemoveAll();
  for (i = 0; i < mMontParArray->GetSize(); i++) {
    montp = mMontParArray->GetAt(i);
    delete montp;
  }   
  mMontParArray->RemoveAll();
  for (i = 0; i < mTSparamArray->GetSize(); i++) {
    tsp = mTSparamArray->GetAt(i);
    delete tsp;
  }   
  mTSparamArray->RemoveAll();
  for (i = 0; i < mAcqStateArray->GetSize(); i++) {
    state = mAcqStateArray->GetAt(i);
    delete state;
  }   
  mAcqStateArray->RemoveAll();
}

StateParams *CNavHelper::NewStateParam(bool navAcquire)
{
  StateParams *param = new StateParams;

  // Zero out items in case it is a low dose set
  param->energyLoss = param->slitWidth = 0.;
  param->intensity = 0.;
  param->magIndex = param->spotSize = 0;
  param->probeMode = -1;
  param->slitIn = param->zeroLoss = false;
  param->ldParams.delayFactor = 0.6f;
  param->singleContMode = SINGLE_FRAME;
  if (navAcquire)
    mAcqStateArray->Add(param);
  else
    mStateArray.Add(param);
  return param;
}

StateParams *CNavHelper::ExistingStateParam(int index, bool navAcquire)
{
  if (index < 0 || (navAcquire && index >= mAcqStateArray->GetSize()) ||
    (!navAcquire && index >= mStateArray.GetSize()))
    return NULL;
  if (navAcquire)
    return mAcqStateArray->GetAt(index);
  else
    return mStateArray.GetAt(index);
}

void CNavHelper::OpenStateDialog(void)
{
  if (mStateDlg)
    return;
  mStateDlg = new CStateDlg();
  mStateDlg->Create(IDD_STATEDLG);
  if (mStatePlacement.rcNormalPosition.right > 0)
    mStateDlg->SetWindowPlacement(&mStatePlacement);
  mStateDlg->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetStatePlacement(void)
{
  if (mStateDlg)
    mStateDlg->GetWindowPlacement(&mStatePlacement);
  return &mStatePlacement;
  return NULL;
}

// Update the dialogs we are responsible for
void CNavHelper::UpdateStateDlg(void)
{
  if (mStateDlg)
    mStateDlg->Update();
  if (mRotAlignDlg)
    mRotAlignDlg->Update();
}

/////////////////////////////////////////////////
/////  DUAL MAP  (THE ORIGINAL NAME)  NOW ANCHOR MAP
/////////////////////////////////////////////////

void CNavHelper::MakeDualMap(CMapDrawItem *item)
{
  EMimageBuffer *imBuf;
  int err;
  mIndexAfterDual = mNav->GetCurListSel();
  if (!item) {
    AfxMessageBox("Cannot find the item that was to be used for the map acquire state",
      MB_EXCLAME);
    return;
  }
  
  // Make a map if it is not already one and it is suitable
  imBuf = &mImBufs[mWinApp->Montaging() ? 1 : 0];
  if (!imBuf->mMapID && imBuf->mMagInd > item->mMapMagInd) {
    err = mNav->NewMap(true);
    if (err > 0)
      return;
    if (!err) {
      mWinApp->AppendToLog("Made a new map from the existing image before getting"
      " anchor map");
      mIndexAfterDual = mNav->GetCurListSel();
    }
  }

  // If already in a map acquire state, save the ID of that and restore before new state
  // But if in any other set state, forget the prior state so we return to this state
  mSavedMapStateID = 0;
  if (mTypeOfSavedState == STATE_MAP_ACQUIRE) {
    mSavedMapStateID = mMapStateItemID;
    RestoreFromMapState();
  } else {
    mTypeOfSavedState = STATE_NONE;
  }

  // Go to imaging state, apply any defocus offset
  if (SetToMapImagingState(item, true)) {
    RestoreFromMapState();
    return;
  }
  SetMapOffsetsIfAny(item);

  if (item->mMapMontage) {
    mWinApp->StartMontageOrTrial(false);
  } else {
    mCamera->SetCancelNextContinuous(true);
    mWinApp->mCamera->InitiateCapture(RECORD_CONSET);
  }
  mAcquiringDual = true;

  mWinApp->AddIdleTask(TASK_DUAL_MAP, 0, 0);
  mWinApp->SetStatusText(COMPLEX_PANE, "MAKING ANCHOR MAP");
  mWinApp->UpdateBufferWindows();
}

int CNavHelper::DualMapBusy(void)
{
  int err = mCamera->TaskCameraBusy();
  if (err < 0)
    return err;
  return (err || mWinApp->mMontageController->DoingMontage()) ? 1 : 0;
}

void CNavHelper::DualMapDone(int param)
{
  CMapDrawItem *item;
  int savedIndex = -1;
  if (!(mWinApp->Montaging() && mWinApp->mMontageController->GetLastFailed())) {
    if (!mWinApp->Montaging()) {
      if (!mBufferManager->IsBufferSavable(mImBufs)) {
        AfxMessageBox("The file into which the anchor map was to be saved has changed\n"
          "and this image cannot be saved there", MB_EXCLAME);
        StopDualMap();
        return;
      }
      mBufferManager->SaveImageBuffer(mWinApp->mStoreMRC);
    }
    mNav->SetSkipBacklashType(2);
    if (!mNav->NewMap()) {
      item = mNav->GetCurrentItem();
      if (item) {

        // restore nav current item; adjust note and copy the defocus offset
        savedIndex = mNav->GetCurrentIndex();
        mNav->SetCurListSel(mIndexAfterDual);
        item->mNote = "Anchor - " + item->mNote;
        mNav->UpdateListString((int)mItemArray->GetSize() - 1);
        item->mDefocusOffset = (float)mRIdefocusOffsetSet;
        item->mMapAlpha = mRIalphaSet;
      }
    }
  }
  StopDualMap();
  if (savedIndex >= 0)
    mNav->AdjustBacklash(savedIndex, true);
}

void CNavHelper::DualMapCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out acquiring anchor map"), MB_EXCLAME);
  StopDualMap();
  mWinApp->ErrorOccurred(error);
}

void CNavHelper::StopDualMap(void)
{
  CMapDrawItem *item;
  if (!mAcquiringDual)
    return;
  mAcquiringDual = false;
  RestoreMapOffsets();
  RestoreFromMapState();

  // Restore the prior map imaging state if any and if it still exists
  if (mSavedMapStateID) {
    item = mNav->FindItemWithMapID(mSavedMapStateID);
    if (item)
      SetToMapImagingState(item, true);
  }
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  mWinApp->UpdateBufferWindows();
}

/////////////////////////////////////////////////
/////  ACQUIRE-RELATED ROUTINES, CHECKING AND LISTING
/////////////////////////////////////////////////

// Check for problems in the set of items/files/etc to acquire
int CNavHelper::AssessAcquireProblems()
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  MontParam *montp;
  StateParams *state;
  TiltSeriesParam *tsp;
  CString mess, mess2, label;
  int montParInd, stateInd, camMismatch, binMismatch, numNoMap, numAtEdge, err;
  int *activeList = mWinApp->GetActiveCameraList();
  ControlSet *masterSets = mWinApp->GetCamConSets();
  NavParams *navParam = mWinApp->GetNavParams();
  int lastBin[MAX_CAMERAS];
  int cam, bin, i, j, k, stateCam, numBroke, numGroups, curGroup, fileOptInd;
  bool seen;
  int *seenGroups = new int[mItemArray->GetSize()];

  camMismatch = 0;
  binMismatch = 0;
  numNoMap = numAtEdge = numBroke = numGroups = 0;
  for (i = 0; i < MAX_CAMERAS; i++)
    lastBin[i] = -1;

  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration() && 
      ((item->mAcquire && navParam->acquireType != ACQUIRE_DO_TS) || 
      (item->mTSparamIndex >=0 && navParam->acquireType == ACQUIRE_DO_TS))) {

      // Get parameter index for item or group, ignore current group
      montParInd = item->mMontParamIndex;
      fileOptInd = item->mFilePropIndex;
      stateInd = item->mStateIndex;
      j = mNav->GroupScheduledIndex(item->mGroupID);
      if (j >= 0 && item->mGroupID != curGroup) {
        sched = mGroupFiles->GetAt(j);

        // Check for file open
        k = mDocWnd->StoreIndexFromName(sched->filename);
        if (k >= 0) {
          mNav->CountItemsInGroup(sched->groupID, label, mess2, j);
          mess.Format("The file set to be opened for the group with\n"
            "labels from %s to %s is already open."
            "\n(file # %d, %s)\n\nYou should set a different name for the file to open,"
            "\nor close the file (in which case it will be overwritten)", (LPCTSTR)label,
            (LPCTSTR)mess2, k + 1, item->mFileToOpen);
          AfxMessageBox(mess, MB_EXCLAME);
          return 1;
        }

        // First see if group has been seen before
        seen = false;
        for (k = 0; k< numGroups; k++) {
          if (item->mGroupID == seenGroups[k]) {
            seen = true;
            break;
          }
        }

        // If seen, it is bad; if not get parameter indices and add to seen list
        // Either way, make it current group so further items in this group are ignored
        curGroup = item->mGroupID;
        if (seen)
          numBroke++;
        else {
          montParInd = sched->montParamIndex;
          fileOptInd = sched->filePropIndex;
          stateInd = sched->stateIndex;
          seenGroups[numGroups++] = curGroup;
        }
      }

      // Check file already open
      if (item->mFilePropIndex >= 0) {
        k = mDocWnd->StoreIndexFromName(item->mFileToOpen);
        if (k >= 0) {
          mess.Format("The file set to be opened for item # %d, label %s is already open"
            "\n(file # %d, %s)\n\nYou should set a different name for the file to open,"
            "\nor close the file (in which case it will be overwritten)", i + 1, 
            item->mLabel, k + 1, item->mFileToOpen);
          AfxMessageBox(mess, MB_EXCLAME);
          return 1;
        }
      }

      // If there is new file for item not group, cancel current group
      if (j < 0 && item->mFilePropIndex >= 0)
        curGroup = 0;

      // Check that realign to map can work if requested now that file indices are known
      if (navParam->acqRealign) {
        mNav->SetNextFileIndices(fileOptInd, montParInd);
        err = FindMapForRealigning(item, navParam->acqRestoreOnRealign || 
          navParam->acquireType != ACQUIRE_RUN_MACRO);
        if (err == 4)
          numNoMap++;
        if (err == 5)
          numAtEdge++;
      }

      // Get camera and binning from state first 
      stateCam = -1;
      if (stateInd >= 0) {
        state = mAcqStateArray->GetAt(stateInd);
        for (j = 0; j < mWinApp->GetNumActiveCameras(); j++) {
          if (activeList[j] == state->camIndex) {
            stateCam = j;
            lastBin[j] = state->binning;
            break;
          }
        }
      }

      // Get defined camera and binning from the parameter sets
      if (montParInd >= 0) {
        montp = mMontParArray->GetAt(montParInd);
        cam = montp->cameraIndex;
        bin = montp->binning;
      } else if (item->mTSparamIndex >= 0) {
        tsp = mTSparamArray->GetAt(item->mTSparamIndex);
        cam = tsp->cameraIndex;
        bin = tsp->binning;
      } else
        continue;

      // If the camera does not match the one defined in this item's state, it's mismatch
      if (stateCam >= 0 && cam != stateCam)
        camMismatch++;

      // No binning defined by state defined yet: get the binning from the camera conset
      if (lastBin[cam] < 0)
        lastBin[cam] = masterSets[activeList[cam] * MAX_CONSETS + RECORD_CONSET].binning;
      if (lastBin[cam] != bin)
        binMismatch++;

      // This param binning becomes the binning for next time unless there is a state
      lastBin[cam] = bin;
    }
  }

  delete [] seenGroups;

  if (numNoMap || numAtEdge) {
    mess = "You selected 'Realign to Item' but:\n";
    if (numNoMap) {
      mess2.Format("    %d items are not located within a map that can be used for"
        " realigning\n", numNoMap);
      mess += mess2;
    }
    if (numAtEdge) {
      mess2.Format("    %d items are located too close to the edge of a map for "
        "realigning to be reliable\n", numAtEdge);
      mess += mess2;
    }
    mess += "\nDo you want to proceed anyway and just go to the\n"
      "nominal stage coordinates for those items?";
    if (AfxMessageBox(mess, MB_YESNO | MB_ICONQUESTION) == IDNO)
      return 1;
  }

  if (numBroke) {
    mess.Format("There is a new file opened for another item in the middle of a group "
      "with a designated file, so some of the items in that group would be "
      "acquired into the new file.  This happens %d times.\n\nPress:\n"
      "\"Stop && Fix\" to stop and try to fix this,\n\"Go On\" to just go on.", numBroke);
    if (SEMThreeChoiceBox(mess, "Stop && Fix", "Go On", "", MB_YESNO | MB_ICONQUESTION) ==
      IDYES)
      return 1;
  }

  if (camMismatch || binMismatch) {
    mess = "";
    if (camMismatch)
      mess.Format("There are %d mismatches between the camera specified in the montage "
      "or tilt series parameters for an item and the camera in the imaging state "
      "to be set for that item.  This is odd.\n\n", camMismatch);
    if (binMismatch) {
      mess2.Format("The binning specified in the montage or tilt series parameters for "
        "an item differs from the existing Record binning for the particular camera. "
        "This happens %d times.\nThe exposure time would be changed to compensate, but"
      " this is probably undesirable\n\n", binMismatch);
      mess += mess2;
    }
    mess += "Press:\n\"Stop && Fix\" to stop and fix these problems by resetting state"
      " parameters,\n\n"
      "\"Go On\" to go on regardless of these problems.";
    if (SEMThreeChoiceBox(mess, "Stop && Fix", "Go On", "", MB_YESNO | MB_ICONQUESTION) ==
      IDYES)
      return 1;
  }
  return 0;
}

// List files, tilt series, montages, and states
void CNavHelper::ListFilesToOpen(void)
{
  ScheduledFile *sched;
  CMapDrawItem *item;
  MontParam *montp;
  StateParams *state;
  TiltSeriesParam *tsp;
  CString mess, mess2, label, lastlab;
  CString *namep;
  int mag, active, magInd, spot;
  double intensity;
  int *activeList = mWinApp->GetActiveCameraList();
  MagTable *magTab = mWinApp->GetMagTable();
  CameraParameters *camp;
  int montParInd, stateInd, num, numacq;
  int i, j, k, numGroups = 0;
  bool seen;

  if (!mItemArray->GetSize())
    return;
  int *seenGroups = new int[mItemArray->GetSize()];
  mWinApp->AppendToLog("\r\nListing of files to open and states to be set", LOG_OPEN_IF_CLOSED);
  for (i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration() && 
      (item->mAcquire || item->mTSparamIndex >= 0)) {

      namep = NULL;
      if (item->mFilePropIndex >= 0) {
        namep = &item->mFileToOpen;
        montParInd = item->mMontParamIndex;
        stateInd = item->mStateIndex;
      }
      j = mNav->GroupScheduledIndex(item->mGroupID);
      if (j >= 0) {

        // First see if group has been seen before
        seen = false;
        for (k = 0; k< numGroups; k++) {
          if (item->mGroupID == seenGroups[k]) {
            seen = true;
            break;
          }
        }
        if (!seen) {
          sched = mGroupFiles->GetAt(j);
          montParInd = sched->montParamIndex;
          stateInd = sched->stateIndex;
          namep = &sched->filename;
          seenGroups[numGroups++] = item->mGroupID;
        }
      }

      if (!namep)
        continue;
      if (j >= 0) {
        num = mNav->CountItemsInGroup(sched->groupID, label, lastlab, numacq);
        mess.Format("Group of %d items (%d set to Acquire), labels from %s to %s",
          num, numacq, (LPCTSTR)label, (LPCTSTR)lastlab);
      } else if (item->mTSparamIndex >= 0) {
        tsp = mTSparamArray->GetAt(item->mTSparamIndex);
        camp = mWinApp->GetActiveCamParam(tsp->cameraIndex);
        mess.Format("Tilt series with camera %d,   binning %d,   %.1f to %.1f at %.2f "
          "deg,  item # %d,  label %s", tsp->cameraIndex + 1, 
          tsp->binning / (camp->K2Type ? 2 : 1), tsp->startingTilt, 
          tsp->endingTilt, tsp->tiltIncrement, i, (LPCTSTR)item->mLabel);
      } else {
        mess.Format("Single item # %d,  label %s", i, (LPCTSTR)item->mLabel);
      }
      mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
      mess = CString("   ") + *namep;
      if (montParInd >= 0) {
        montp = mMontParArray->GetAt(montParInd);
        mess2.Format("     %d x %d montage", montp->xNframes, montp->yNframes);
        mess += mess2;
        if (item->mTSparamIndex < 0) {
          camp = mWinApp->GetActiveCamParam(montp->cameraIndex);
          mess2.Format("   with camera %d   binning %d", montp->cameraIndex+1, 
            montp->binning / (camp->K2Type ? 2 : 1));
          mess += mess2;
        }
      }
      mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
      if (stateInd >= 0) {
        state = mAcqStateArray->GetAt(stateInd);
        magInd = state->lowDose ? state->ldParams.magIndex : state->magIndex;
        camp = mWinApp->GetCamParams() + state->camIndex;
        mag = MagForCamera(camp, magInd);
        active = mWinApp->LookupActiveCamera(state->camIndex) + 1;
        spot = state->lowDose ? state->ldParams.spotSize : state->spotSize;
        intensity = state->lowDose ? state->ldParams.intensity : state->intensity;

        mess.Format("   New state:  %s   cam %d   mag %d   spot %d   %s %.2f%s   exp "
          "%.3f   bin %d   %dx%d", state->lowDose ? "LD" : "", active, mag, spot, 
          mScope->GetC2Name(), mScope->GetC2Percent(spot, intensity), 
          mScope->GetC2Units(), state->exposure,
          state->binning / (camp->K2Type ? 2 : 1), state->xFrame, state->yFrame);
        mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
      }
    }
  }
  delete [] seenGroups;

}

// return count of items marked for acquire and tilt series, or -1 if Nav not open
void CNavHelper::CountAcquireItems(int & numAcquire, int & numTS)
{
  CMapDrawItem *item;
  if (!mNav) {
    numAcquire = -1;
    numTS = -1;
    return;
  }
  numAcquire = 0;
  numTS = 0;
  for (int i = 0; i < mItemArray->GetSize(); i++) {
    item = mItemArray->GetAt(i);
    if (item->mRegistration == mNav->GetCurrentRegistration()) {
      if (item->mAcquire)
        numAcquire++;
      if (item->mTSparamIndex >= 0)
        numTS++;
    }
  }
}

// Align an image, searching for best rotation, with possible scaling as well
int CNavHelper::AlignWithRotation(int buffer, float centerAngle, float angleRange, 
                                  float &rotBest, float &shiftXbest, float &shiftYbest,
                                  float scaling)
{
  float step = 1.5f;
  int numSteps = B3DNINT(angleRange / step) + 1;
  int ist, idir, istMax, numStepCuts = 4;
  float peakmax = -1.e30f;
  float rotation, curMax, peak, shiftX, shiftY, CCC, fracPix;
  float *CCCp = &CCC, *fracPixP = &fracPix;
  double overlapPow = 0.166667;
  CString report;

  if (!scaling) {
    CCCp = NULL;
    fracPixP = NULL;
  }

  step = 0.;
  if (numSteps > 1)
    step = angleRange / (numSteps - 1);

  // Scan for the number of steps
  for (ist = 0; ist < numSteps; ist++) {
    rotation = centerAngle - 0.5f * angleRange + (float)ist * step;
    if (mShiftManager->AutoAlign(buffer, 0, false, false, &peak, 0., 0., 0., scaling, 
      rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
      return 1;
    if (CCCp)
      peak = (float)(CCC * pow((double)fracPix, overlapPow));
    if (peak > peakmax) {
      peakmax = peak;
      rotBest = rotation;
      istMax = ist;
      mImBufs->mImage->getShifts(shiftXbest, shiftYbest);
    }
    mImBufs->mImage->setShifts(0., 0.);
    SEMTrace('1', "Rotation %.1f  peak  %g  shift %.1f %.1f", rotation, peak,
      shiftX, shiftY);
  }

  if (numSteps > 2 && (istMax == 0 || istMax == numSteps - 1)) {
    mWinApp->AppendToLog("The best correlation was found at the end of the range tested;"
    "\r\nyou should redo this with a different angular range");
    if (CCCp)
      return 1;
  }

  // Cut the step and look on either side of the peak 
  if (numSteps > 2 && !(istMax == 0 || istMax == numSteps - 1)) {
    for (ist = 0; ist < numStepCuts; ist++) {
      step /= 2.f;
      curMax = rotBest;
      for (idir = -1; idir <= 1; idir += 2) {
        rotation = curMax + idir * step;
        if (mShiftManager->AutoAlign(buffer, 0, false, false, &peak, 0., 0., 0., scaling,
          rotation, CCCp, fracPixP, true, &shiftX, &shiftY))
          return 1;
        if (CCCp)
          peak = (float)(CCC * pow((double)fracPix, overlapPow));
        if (peak > peakmax) {
          peakmax = peak;
          rotBest = rotation;
          mImBufs->mImage->getShifts(shiftXbest, shiftYbest);
        }
        mImBufs->mImage->setShifts(0., 0.);
        SEMTrace('1', "Rotation %.1f  peak  %g  shift %.1f %.1f", rotation, 
          peak, shiftX, shiftY);
      }
    }
  }

  mWinApp->SetCurrentBuffer(0);
  report.Format("The two images correlate most strongly with a rotation of %.1f",
    rotBest);
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  return 0;
}

int CNavHelper::OKtoAlignWithRotation(void)
{
  ScaleMat aMat;
  float delX, delY;
  int readBuf = mWinApp->mBufferManager->GetBufToReadInto();
  int registration, topBuf;
  topBuf = BufferForRotAlign(registration);
  if (!mImBufs[topBuf].mImage || !mNav->BufferStageToImage(&mImBufs[topBuf], aMat, delX,
    delY) || mImBufs[topBuf].mCaptured == BUFFER_PROCESSED ||
    mImBufs[topBuf].mCaptured == BUFFER_FFT)
    return 0;
  if (!mImBufs[readBuf].mImage || !mImBufs[readBuf].mMapID || 
    mImBufs[readBuf].mRegistration == registration || 
    !mNav->FindItemWithMapID(mImBufs[readBuf].mMapID))
    return 0;
  return 1;
}

void CNavHelper::OpenRotAlignDlg(void)
{
  if (mRotAlignDlg) {
    mRotAlignDlg->BringWindowToTop();
    return;
  }
  mRotAlignDlg = new CNavRotAlignDlg();
  mRotAlignDlg->Create(IDD_NAVROTALIGN);
  mWinApp->SetPlacementFixSize(mRotAlignDlg, &mRotAlignPlace);
  mRotAlignDlg->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CNavHelper::GetRotAlignPlacement(void)
{
  if (mRotAlignDlg) {
    mRotAlignDlg->GetWindowPlacement(&mRotAlignPlace);
    mRotAlignDlg->UnloadData();
  }
  return &mRotAlignPlace;
}

// Returns the buffer number for rotation align, and the registration of it
int CNavHelper::BufferForRotAlign(int &registration)
{
  int topBuf = 0;
  if (mWinApp->Montaging() && mImBufs[1].mCaptured == BUFFER_MONTAGE_OVERVIEW &&
    mImBufs[0].mCaptured == BUFFER_MONTAGE_CENTER)
    topBuf = 1;
  registration = mImBufs[topBuf].mRegistration ? mImBufs[topBuf].mRegistration :
    mNav->GetCurrentRegistration();
  return topBuf;
}

int CNavHelper::AlignWithScaling(float & shiftX, float & shiftY, float & scaling)
{
  if (mWinApp->mMultiTSTasks->AlignWithScaling(1, false, scaling)) {
    scaling = 0;
    return 1;
  }
  mImBufs->mImage->getShifts(shiftX, shiftY);
  return 0;
}

// Loads a piece or synthesizes piece containing the given item
void CNavHelper::LoadPieceContainingPoint(CMapDrawItem *ptItem, int mapIndex)
{
  int cenX[2], cenY[2], xFrame, yFrame, xOverlap, yOverlap, err, yNframes, binning;
  int imageX, imageY, ix, iy;
  ScaleMat aMat;
  float distMin, delX, delY, stageX = 0., stageY = 0., montErrX, montErrY, tmpX, tmpY;
  float useShiftedRatio = 1.25;
  CMapDrawItem *map = mItemArray->GetAt(mapIndex);
  mWinApp->RestoreViewFocus();
  err = DistAndPiecesForRealign(map, ptItem->mStageX, ptItem->mStageY, 
    ptItem->mPieceDrawnOn, mapIndex, aMat, delX, delY, distMin, cenX[0], cenY[0], cenX[1],
    cenY[1], imageX, imageY, true, xFrame, yFrame, binning);
  if (err > 1 && mCurStoreInd < 0)
    delete mMapStore;
  if (err)
    return;
  cenX[1] = cenX[0];
  cenY[1] = cenY[0];
  xOverlap = mMapMontP->xOverlap;
  yOverlap = mMapMontP->yOverlap;
  yNframes = mMapMontP->yNframes;
  
  // Adjust coordinate relative to edge of frame then shift to using two frames
  // if it is possible and the distance to the edge in shifted frame is enough bigger 
  // than the distance in the real frame
  imageX -= cenX[0] * (xFrame - xOverlap);
  if (cenX[0] > 0 && (xFrame + xOverlap) / 2. - imageX > useShiftedRatio * imageX)
    cenX[0]--;
  else if (cenX[0] < mMapMontP->xNframes - 1 && 
    imageX - (xFrame - xOverlap) / 2. > useShiftedRatio * (xFrame - imageX))
    cenX[1]++;
  imageY -= cenY[0] * (yFrame - yOverlap);
  if (cenY[0] > 0 && (yFrame + yOverlap) / 2. - imageY > useShiftedRatio * imageY)
    cenY[0]--;
  else if (cenY[0] < yNframes - 1 && 
    imageY - (yFrame - yOverlap) / 2. > useShiftedRatio * (yFrame - imageY))
    cenY[1]++;

  // But make sure the added piece exists, or at least one of two added ones, and if
  // they don't, then revert the index back to the main piece
  if (cenX[0] < cenX[1] && mPieceSavedAt[cenY[0] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[0] * yNframes] < 0)
    cenX[0]++;
  if (cenX[0] < cenX[1] && mPieceSavedAt[cenY[0] + cenX[1] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[1] * yNframes] < 0)
    cenX[1]--;
  if (cenY[0] < cenY[1] && mPieceSavedAt[cenY[0] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[0] + cenX[1] * yNframes] < 0)
    cenY[0]++;
  if (cenY[0] < cenX[1] && mPieceSavedAt[cenY[1] + cenX[0] * yNframes] < 0 && 
    mPieceSavedAt[cenY[1] + cenX[1] * yNframes] < 0)
    cenY[1]--;

  // Set the center frames and read the montage
  mWinApp->mMontageController->SetFramesForCenterOnly(cenX[0], cenX[1], cenY[0], cenY[1]);
  if (mWinApp->mMontageController->ReadMontage(map->mMapSection, mMapMontP,
    mMapStore, true)) {
      if (mCurStoreInd < 0)
        delete mMapStore;
      return;
  }

  // Get the stage position of this piece by averaging all contributing pieces
  mPreviousErrX = mPreviousErrY = 0.;
  mUseMontStageError = false;
  for (ix = 0; ix < 2; ix++) {
    for (iy = 0; iy < 2; iy++) {
      StagePositionOfPiece(mMapMontP, aMat, delX, delY, cenX[ix], cenY[iy], tmpX, 
        tmpY, montErrX, montErrY);
      stageX += tmpX / 4.f;
      stageY += tmpY / 4.f;
    }
  }

  // Adjust buffer properties
  mImBufs->mStage2ImMat = aMat;
  mImBufs->mStage2ImDelX = mImBufs->mImage->getWidth() / 2.f - 
    aMat.xpx * stageX - aMat.xpy * stageY;
  mImBufs->mStage2ImDelY = mImBufs->mImage->getHeight() / 2.f - 
    aMat.ypx * stageX - aMat.ypy * stageY;
  mImBufs->mLoadWidth = mImBufs->mImage->getWidth();
  mImBufs->mLoadHeight = mImBufs->mImage->getHeight();
  mImBufs->mISX = mImBufs->mISY = 0.;
  mImBufs->mRotAngle = 0.;
  mImBufs->mInverted = false;
  mImBufs->mCamera = map->mMapCamera;
  mImBufs->mCaptured = BUFFER_MONTAGE_PIECE;

  // Rotate buffer just like map
  if (map->mRotOnLoad && !map->mImported)
    mNav->RotateMap(mImBufs, false);

  if (mCurStoreInd < 0)
    delete mMapStore;
  mWinApp->SetCurrentBuffer(0);
}
