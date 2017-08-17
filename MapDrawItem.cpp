// MapDrawItem.cpp: implementation of the CMapDrawItem class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "MapDrawItem.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define POINT_CHUNK  5

static COLORREF colors[] = {RGB(255,0,0), RGB(0,255,0), RGB(0,150,255), RGB(255,255,0),
                            RGB(255,0,255), RGB(0,0,64), RGB(255,0,170), RGB(0,220,255),
                            RGB(240,170,0)};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMapDrawItem::CMapDrawItem()
{
  mNumPoints = 0;
  mMaxPoints = 0;
  mPtX = NULL;
  mPtY = NULL;
  mDraw = true;
  mCorner = false;
  mAcquire = false;
  mRotOnLoad = false;
  mRegPoint = 0;
  mRegistration = 1;
  mOriginalReg = 1;
  mOldReg = 0;
  mLabel = "";
  mColor = 0;
  mType = ITEM_TYPE_POINT;
  mStageX = mStageY = mStageZ = 0.;
  mRawStageX = mRawStageY = NO_RAW_STAGE;
  mMapTiltAngle = NO_RAW_STAGE;
  mMapID = 0;
  mGroupID = 0;
  mPolygonID = 0;
  mDrawnOnMapID = 0;
  mOldDrawnOnID = 0;
  mPieceDrawnOn = -1;
  mXinPiece = mYinPiece = -1.;
  mRealignedID = 0;
  mAtSamePosID = 0;
  mRegisteredToID = 0;
  mOldRegToID = 0;
  mRealignErrX = mRealignErrY = 0.;
  mRealignReg = 0;
  mLocalRealiErrX = mLocalRealiErrY = 0.;
  mImported = 0;
  mOldImported = 0;
  mImageType = STORE_TYPE_MRC;
  mSuperMontX = mSuperMontY = -1;
  mMontUseStage = -1;
  mBacklashX = mBacklashY = 0.;
  mDefocusOffset = 0.;
  mNetViewShiftX = mNetViewShiftY = 0.;
  mViewBeamShiftX = mViewBeamShiftY = 0.;
  mViewBeamTiltX = mViewBeamTiltY = 0.;
  mMapAlpha = -999;
  mMapProbeMode = -1;
  mMapLowDoseConSet = -1;
  mK2ReadMode = 0;
  mFilePropIndex =  mTSparamIndex = mMontParamIndex =  mStateIndex = -1;
  mFileToOpen = "";
}

CMapDrawItem::~CMapDrawItem()
{
  if (mMaxPoints > 0) {
    delete [] mPtX;
    delete [] mPtY;
  }
}

void CMapDrawItem::AddPoint(float inX, float inY, int index)
{
  float *newX, *newY;
  int i;
  if (index > mNumPoints || index < 0)
    return;

  // If more points are needed, get a chunk and copy old ones over
  // realloc is easier!
  if (mNumPoints >= mMaxPoints) {
    mMaxPoints += POINT_CHUNK;
    newX = new float[mMaxPoints];
    newY = new float[mMaxPoints];
    for (i = 0; i < mNumPoints; i++) {
      newX[i] = mPtX[i];
      newY[i] = mPtY[i];
    }
    if (mPtX)
      delete [] mPtX;
    if (mPtY)
      delete [] mPtY;
    mPtX = newX;
    mPtY = newY;
  }

  // Copy existing points up
  for (i = mNumPoints; i > index; i--) {
    mPtX[i] = mPtX[i - 1];
    mPtY[i] = mPtY[i - 1];
  }

  // Insert new point
  mPtX[index] = inX;
  mPtY[index] = inY;
  mNumPoints++;
}


void CMapDrawItem::AppendPoint(float inX, float inY)
{
    AddPoint(inX, inY, mNumPoints);
}

void CMapDrawItem::DeletePoint(int index)
{
  int i;
  if (index >= mNumPoints || index < 0)
    return;

  // Copy points down
  for (i = index + 1; i < mNumPoints; i++) {
    mPtX[i - 1] = mPtX[i];
    mPtY[i - 1] = mPtY[i];
  }
  mNumPoints--;
}

COLORREF CMapDrawItem::GetColor(bool highlight)
{
  if (highlight && mType == ITEM_TYPE_POINT && mColor == DEFAULT_POINT_COLOR)
    return colors[HIGHLIGHT_COLOR];
  if (mAcquire) {
    if (mType == ITEM_TYPE_POINT && mColor == DEFAULT_POINT_COLOR)
      return colors[POINT_ACQUIRE_COLOR];
    if (mType == ITEM_TYPE_MAP && mColor == DEFAULT_MAP_COLOR)
      return colors[MAP_ACQUIRE_COLOR];
  }
  return colors[mColor];
}
