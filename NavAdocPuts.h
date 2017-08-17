{
  CString adocStr;
  float fvals[4];
  ADOC_PUT(Integer(ADOC_ARG, "Color", item->mColor));
  ADOC_PUT(ThreeFloats(ADOC_ARG, "StageXYZ", item->mStageX,
                       item->mStageY, item->mStageZ));
  ADOC_PUT(Integer(ADOC_ARG, "NumPts", item->mNumPoints));
  if (item->mCorner)
    ADOC_PUT(Integer(ADOC_ARG, "Corner", 1));

  // Yes the default for draw is on save only if off
  if (!item->mDraw)
    ADOC_PUT(Integer(ADOC_ARG, "Draw", 0));
  if (item->mRegPoint)
    ADOC_PUT(Integer(ADOC_ARG, "RegPt", item->mRegPoint));
  ADOC_PUT(Integer(ADOC_ARG, "Regis", item->mRegistration));
  ADOC_PUT(Integer(ADOC_ARG, "Type", item->mType));
  if (!item->mNote.IsEmpty()) 
    ADOC_PUT(KeyValue(ADOC_ARG, "Note", (LPCTSTR)item->mNote));
  if (item->mGroupID)
    ADOC_PUT(Integer(ADOC_ARG, "GroupID", item->mGroupID));
  if (item->mPolygonID)
    ADOC_PUT(Integer(ADOC_ARG, "PolyID", item->mPolygonID));
  if (item->mImported)
    ADOC_PUT(Integer(ADOC_ARG, "Imported", item->mImported));
  if (item->mSuperMontX >= 0 || item->mSuperMontY >= 0)
    ADOC_PUT(TwoIntegers(ADOC_ARG, "SuperMontXY",
                          item->mSuperMontX, item->mSuperMontY));

  // Put out original registration only if it differs
  if (item->mOriginalReg != item->mRegistration)
    ADOC_PUT(Integer(ADOC_ARG, "OrigReg", item->mOriginalReg));
  if (item->mDrawnOnMapID)
    ADOC_PUT(Integer(ADOC_ARG, "DrawnID", item->mDrawnOnMapID));
  if (item->mBacklashX || item->mBacklashY)
    ADOC_PUT(TwoFloats(ADOC_ARG, "BklshXY",
                        item->mBacklashX, item->mBacklashY));
  if (item->mAtSamePosID)
    ADOC_PUT(Integer(ADOC_ARG, "SamePosId", item->mAtSamePosID));
  if (item->mRawStageX != NO_RAW_STAGE || item->mRawStageY != NO_RAW_STAGE)
    ADOC_PUT(TwoFloats(ADOC_ARG, "RawStageXY", item->mRawStageX,
                       item->mRawStageY));
  if (item->mAcquire)
    ADOC_PUT(Integer(ADOC_ARG, "Acquire", 1));

  // No need to subtract 1, it should be saved as is
  if (item->mPieceDrawnOn >= 0)
    ADOC_PUT(Integer(ADOC_ARG, "PieceOn", item->mPieceDrawnOn));
  if (item->mXinPiece >= 0. && item->mYinPiece >= 0.)
    ADOC_PUT(TwoFloats(ADOC_ARG, "XYinPc", item->mXinPiece, item->mYinPiece));
  if (item->mType == ITEM_TYPE_MAP) {
    ADOC_PUT(KeyValue(ADOC_ARG, "MapFile",  (LPCTSTR)item->mMapFile));
    ADOC_PUT(Integer(ADOC_ARG, "MapID", item->mMapID));
    ADOC_PUT(Integer(ADOC_ARG, "MapMontage", item->mMapMontage));
    ADOC_PUT(Integer(ADOC_ARG, "MapSection",item->mMapSection));
    ADOC_PUT(Integer(ADOC_ARG, "MapBinning",item->mMapBinning));
    ADOC_PUT(Integer(ADOC_ARG, "MapMagInd", item->mMapMagInd));
    ADOC_PUT(Integer(ADOC_ARG, "MapCamera", item->mMapCamera));
    fvals[0] = item->mMapScaleMat.xpx;
    fvals[1] = item->mMapScaleMat.xpy;
    fvals[2] = item->mMapScaleMat.ypx;
    fvals[3] = item->mMapScaleMat.ypy;
    ADOC_PUT(FloatArray(ADOC_ARG, "MapScaleMat", &fvals[0], 4));
    ADOC_PUT(TwoIntegers(ADOC_ARG, "MapWidthHeight", item->mMapWidth,
                         item->mMapHeight));

    ADOC_PUT(TwoFloats(ADOC_ARG, "MapMinMaxScale", item->mMapMinScale,
                       item->mMapMaxScale));
    ADOC_PUT(TwoIntegers(ADOC_ARG, "MapFramesXY", item->mMapFramesX,
                         item->mMapFramesY));
    ADOC_PUT(Integer(ADOC_ARG, "MontBinning", item->mMontBinning));
    ADOC_PUT(Float(ADOC_ARG, "MapExposure",item->mMapExposure));
    ADOC_PUT(Float(ADOC_ARG, "MapSettling",item->mMapSettling));
    ADOC_PUT(Integer(ADOC_ARG, "ShutterMode", item->mShutterMode));
    ADOC_PUT(Integer(ADOC_ARG, "MapSpotSize", item->mMapSpotSize));

    ADOC_PUT(Double(ADOC_ARG, "MapIntensity", item->mMapIntensity));
    ADOC_PUT(Integer(ADOC_ARG, "MapSlitIn", item->mMapSlitIn ? 1 : 0));
    ADOC_PUT(Float(ADOC_ARG, "MapSlitWidth", item->mMapSlitWidth));
    if (item->mRotOnLoad)
      ADOC_PUT(Integer(ADOC_ARG, "RotOnLoad", 1));
    if (item->mRealignedID)
      ADOC_PUT(Integer(ADOC_ARG, "RealignedID", item->mRealignedID));
    if (item->mRegisteredToID)
      ADOC_PUT(Integer(ADOC_ARG, "RegisteredToID", item->mRegisteredToID));
    if (item->mRealignErrX || item->mRealignErrX)
      ADOC_PUT(TwoFloats(ADOC_ARG, "RealignErrXY", item->mRealignErrX,
                         item->mRealignErrX));
    if (item->mLocalRealiErrX || item->mLocalRealiErrX)
      ADOC_PUT(TwoFloats(ADOC_ARG, "LocalErrXY", item->mLocalRealiErrX,
                         item->mLocalRealiErrX));
    if (item->mRealignReg)
      ADOC_PUT(Integer(ADOC_ARG, "RealignReg",item->mRealignReg));
    ADOC_PUT(Integer(ADOC_ARG, "ImageType", item->mImageType));
    ADOC_PUT(Integer(ADOC_ARG, "MontUseStage", 
                     item->mMontUseStage));
    if (item->mDefocusOffset)
      ADOC_PUT(Float(ADOC_ARG, "DefocusOffset", item->mDefocusOffset));
    if (item->mK2ReadMode)
      ADOC_PUT(Integer(ADOC_ARG, "K2ReadMode", item->mK2ReadMode));
    if (item->mNetViewShiftX || item->mNetViewShiftY)
      ADOC_PUT(TwoFloats(ADOC_ARG, "NetViewShiftXY", item->mNetViewShiftX,
                         item->mNetViewShiftY));
    if (item->mMapAlpha != -999)
      ADOC_PUT(Integer(ADOC_ARG, "MapAlpha", item->mMapAlpha));
    if (item->mViewBeamShiftX || item->mViewBeamShiftY)
      ADOC_PUT(TwoFloats(ADOC_ARG, "ViewBeamShiftXY", item->mViewBeamShiftX,
                         item->mViewBeamShiftY));
    if (item->mViewBeamTiltX || item->mViewBeamTiltY)
      ADOC_PUT(TwoFloats(ADOC_ARG, "ViewBeamTiltXY", item->mViewBeamTiltX,
                         item->mViewBeamTiltY));
    ADOC_PUT(Integer(ADOC_ARG, "MapProbeMode", item->mMapProbeMode));
    ADOC_PUT(Integer(ADOC_ARG, "MapLDConSet", item->mMapLowDoseConSet));
    if (item->mMapTiltAngle > RAW_STAGE_TEST)
      ADOC_PUT(Float(ADOC_ARG, "MapTiltAngle", item->mMapTiltAngle));
  } else {
    if (item->mMapID)
      ADOC_PUT(Integer(ADOC_ARG, "MapID", item->mMapID));
  }
  ADOC_PUT(FloatArray(ADOC_ARG, "PtsX", &item->mPtX[0], item->mNumPoints));
  ADOC_PUT(FloatArray(ADOC_ARG, "PtsY", &item->mPtY[0], item->mNumPoints));
}
