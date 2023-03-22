
// Tilt series and montage parameters and file options saved to the Navigator file
// This first section contain items saved as settings also, which dictates the naming
// of the different macros for all the sections
#ifdef SET_TEST_SECT3
FLOAT_SETT_ASSIGN("TiltSeriesLastStartingTilt", tsParam->lastStartingTilt)
BOOL_SETT_ASSIGN("TiltSeriesTrackLowMag", tsParam->trackLowMag)
INT_SETT_ASSIGN("TiltSeriesTrackAfterFocus", tsParam->trackAfterFocus)
INT_SETT_ASSIGN("TiltSeriesBeamControl", tsParam->beamControl)
INT_SETT_ASSIGN("TiltSeriesMeanCounts", tsParam->meanCounts)
FLOAT_SETT_ASSIGN("TiltSeriesCosinePower", tsParam->cosinePower)
BOOL_SETT_ASSIGN("TiltSeriesTaperCounts", tsParam->taperCounts)
INT_SETT_ASSIGN("TiltSeriesHighCounts", tsParam->highCounts)
FLOAT_SETT_ASSIGN("TiltSeriesTaperAngle", tsParam->taperAngle)
BOOL_SETT_ASSIGN("TiltSeriesLimitIntensity", tsParam->limitIntensity)
BOOL_SETT_ASSIGN("TiltSeriesRefineEucentricity", tsParam->refineEucen)
BOOL_SETT_ASSIGN("TiltSeriesWalkLeaveAnchor", tsParam->leaveAnchor)
FLOAT_SETT_ASSIGN("TiltSeriesWalkAnchorTilt", tsParam->anchorTilt)
INT_SETT_ASSIGN("TiltSeriesLeftAnchorBuffer", tsParam->anchorBuffer)
FLOAT_SETT_ASSIGN("TiltSeriesImageShiftLimit", tsParam->ISlimit)
FLOAT_SETT_ASSIGN("TiltSeriesFocusCheckInterval", tsParam->focusCheckInterval)
BOOL_SETT_ASSIGN("TiltSeriesRepeatRecord", tsParam->repeatRecord)
BOOL_SETT_ASSIGN("TiltSeriesConfirmLDRepeat", tsParam->confirmLowDoseRepeat)
FLOAT_SETT_ASSIGN("TiltSeriesMaxRecordLoss", tsParam->maxRecordShift)
BOOL_SETT_ASSIGN("TiltSeriesStopOnBigShift", tsParam->stopOnBigAlignShift)
FLOAT_SETT_ASSIGN("TiltSeriesMaxAlignShift", tsParam->maxAlignShift)
BOOL_SETT_ASSIGN("TiltSeriesRepeatAutofocus", tsParam->repeatAutofocus)
FLOAT_SETT_ASSIGN("TiltSeriesMaxFocusDiff", tsParam->maxFocusDiff)
BOOL_SETT_ASSIGN("TiltSeriesAlwaysFocusAtHigh", tsParam->alwaysFocusHigh)
FLOAT_SETT_ASSIGN("TiltSeriesAlwaysFocusAngle", tsParam->alwaysFocusAngle)
BOOL_SETT_ASSIGN("TiltSeriesSkipAutofocus", tsParam->skipAutofocus)
BOOL_SETT_ASSIGN("TiltSeriesCheckAutofocus", tsParam->checkAutofocus)
BOOL_SETT_ASSIGN("TSLimitFocusChange", tsParam->limitDefocusChange)
FLOAT_SETT_ASSIGN("TSFocusChangeLimit", tsParam->defocusChangeLimit)
FLOAT_SETT_ASSIGN("TiltSeriesMinFocusAccuracy", tsParam->minFocusAccuracy)
BOOL_SETT_ASSIGN("TiltSeriesRefineZLP", tsParam->refineZLP)
INT_SETT_ASSIGN("TiltSeriesZLPInterval", tsParam->zlpInterval)
BOOL_SETT_ASSIGN("TiltSeriesCenterBeam", tsParam->centerBeamFromTrial)
BOOL_SETT_ASSIGN("TiltSeriesCloseValvesAtEnd", tsParam->closeValvesAtEnd)
BOOL_SETT_ASSIGN("TiltSeriesAlignTrackOnly", tsParam->alignTrackOnly)
BOOL_SETT_ASSIGN("TiltSeriesPreviewBeforeNewRef", tsParam->previewBeforeNewRef)
FLOAT_SETT_ASSIGN("TiltSeriesMaxRefDisagreement", tsParam->maxRefDisagreement)
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorXY", tsParam->maxPredictErrorXY)
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZ", tsParam->maxPredictErrorZ[0])
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZSN", tsParam->maxPredictErrorZ[1])
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZSM", tsParam->maxPredictErrorZ[2])
BOOL_SETT_ASSIGN("TiltSeriesCenBeamPeriodically", tsParam->cenBeamPeriodically)
INT_SETT_ASSIGN("TiltSeriesCenBeamInterval", tsParam->cenBeamInterval)
BOOL_SETT_ASSIGN("TiltSeriesCenBeamAbove", tsParam->cenBeamAbove)
FLOAT_SETT_ASSIGN("TiltSeriesCenBeamAngle", tsParam->cenBeamAngle)
INT_SETT_ASSIGN("TiltSeriesStackRecords", tsParam->stackRecords)
BOOL_SETT_ASSIGN("TSWaitForDrift", tsParam->waitForDrift)
INT_SETT_ASSIGN("TSOnlyWaitAboveBelow", tsParam->onlyWaitDriftAboveBelow)
FLOAT_SETT_ASSIGN("TSWaitAboveAngle", tsParam->waitDriftAboveAngle)
FLOAT_SETT_ASSIGN("TSWaitBelowAngle", tsParam->waitDriftBelowAngle)
BOOL_SETT_ASSIGN("TSWaitAtReversals", tsParam->waitAtDosymReversals)
BOOL_SETT_ASSIGN("TSWaitDosymIgnore", tsParam->waitDosymIgnoreAngles)
BOOL_SETT_ASSIGN("TSDoEarlyReturn", tsParam->doEarlyReturn)
INT_SETT_ASSIGN("TSNumEarlyFrames", tsParam->earlyReturnNumFrames)
INT_SETT_ASSIGN("TiltSeriesStackBinSize", tsParam->stackBinSize)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetExposure", tsParam->extraSetExposure)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetSpot", tsParam->extraSetSpot)
INT_SETT_ASSIGN("TiltSeriesExtraSpotSize", tsParam->extraSpotSize)
FLOAT_SETT_ASSIGN("TiltSeriesExtraDeltaIntensity", tsParam->extraDeltaIntensity)
FLOAT_SETT_ASSIGN("TiltSeriesExtraDrift", tsParam->extraDrift)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetBinning", tsParam->extraSetBinning)
INT_SETT_ASSIGN("TiltSeriesExtraBinIndex", tsParam->extraBinIndex)
BOOL_SETT_ASSIGN("TiltSeriesDoVariations", tsParam->doVariations)
BOOL_SETT_ASSIGN("TiltSeriesChangeRecExposure", tsParam->changeRecExposure)
BOOL_SETT_ASSIGN("TiltSeriesChangeAllExposures", tsParam->changeAllExposures)
BOOL_SETT_ASSIGN("TiltSeriesChangeSettling", tsParam->changeSettling)
#endif
#ifdef NAV_OTHER_TS_PARAMS
BOOL_SETT_ASSIGN("AnchorBidirWithView", tsParam->anchorBidirWithView)
BOOL_SETT_ASSIGN("ApplyAbsFocusLimits", tsParam->applyAbsFocusLimits)
FLOAT_SETT_ASSIGN("AutofocusOffset", tsParam->autofocusOffset)
DOUBLE_SETT_ASSIGN("BeamTilt", tsParam->beamTilt)
FLOAT_SETT_ASSIGN("BidirAngle", tsParam->bidirAngle)
INT_SETT_ASSIGN("Binning", tsParam->binning)
INT_SETT_ASSIGN("CameraIndex", tsParam->cameraIndex)
BOOL_SETT_ASSIGN("DoBidirectional", tsParam->doBidirectional)
BOOL_SETT_ASSIGN("DoDoseSymmetric", tsParam->doDoseSymmetric)
BOOL_SETT_ASSIGN("DosymAnchorIfRunToEnd", tsParam->dosymAnchorIfRunToEnd)
INT_SETT_ASSIGN("DosymBaseGroupSize", tsParam->dosymBaseGroupSize)
BOOL_SETT_ASSIGN("DosymDoRunToEnd", tsParam->dosymDoRunToEnd)
INT_SETT_ASSIGN("DosymGroupIncAmount", tsParam->dosymGroupIncAmount)
INT_SETT_ASSIGN("DosymGroupIncInterval", tsParam->dosymGroupIncInterval)
FLOAT_SETT_ASSIGN("DosymIncStartAngle", tsParam->dosymIncStartAngle)
BOOL_SETT_ASSIGN("DosymIncreaseGroups", tsParam->dosymIncreaseGroups)
INT_SETT_ASSIGN("DosymMinUniForAnchor", tsParam->dosymMinUniForAnchor)
FLOAT_SETT_ASSIGN("DosymRunToEndAngle", tsParam->dosymRunToEndAngle)
BOOL_SETT_ASSIGN("DosymSkipBacklash", tsParam->dosymSkipBacklash)
FLOAT_SETT_ASSIGN("DosymStartingISLimit", tsParam->dosymStartingISLimit)
FLOAT_SETT_ASSIGN("EndingTilt", tsParam->endingTilt)
BOOL_SETT_ASSIGN("IntensitySetAtZero", tsParam->intensitySetAtZero)
BOOL_SETT_ASSIGN("LimitToCurrent", tsParam->limitToCurrent)
BOOL_SETT_ASSIGN("MagLocked", tsParam->magLocked)
BOOL_SETT_ASSIGN("ManualTracking", tsParam->manualTracking)
INT_SETT_ASSIGN("NumExtraChannels", tsParam->numExtraChannels)
INT_SETT_ASSIGN("NumExtraExposures", tsParam->numExtraExposures)
INT_SETT_ASSIGN("NumExtraFilter", tsParam->numExtraFilter)
INT_SETT_ASSIGN("NumExtraFocus", tsParam->numExtraFocus)
INT_SETT_ASSIGN("extraRecordType", tsParam->extraRecordType);
INT_SETT_ASSIGN("ProbeMode", tsParam->probeMode)
BOOL_SETT_ASSIGN("RefIsInA", tsParam->refIsInA)
FLOAT_SETT_ASSIGN("RefocusThreshold", tsParam->refocusThreshold)
BOOL_SETT_ASSIGN("RetainBidirAnchor", tsParam->retainBidirAnchor)
FLOAT_SETT_ASSIGN("StartingTilt", tsParam->startingTilt)
FLOAT_SETT_ASSIGN("TargetDefocus", tsParam->targetDefocus)
FLOAT_SETT_ASSIGN("TiltDelay", tsParam->tiltDelay)
FLOAT_SETT_ASSIGN("TiltIncrement", tsParam->tiltIncrement)
BOOL_SETT_ASSIGN("UseAnchor", tsParam->useAnchor)
BOOL_SETT_ASSIGN("WalkBackForBidir", tsParam->walkBackForBidir)
INT_SETT_ASSIGN("NumVaryItems", tsParam->numVaryItems)
BOOL_SETT_ASSIGN("Initialized", tsParam->initialized)
INT_SETT_ASSIGN("RefCount", tsParam->refCount)
INT_SETT_ASSIGN("NavID", tsParam->navID)
#ifndef SKIP_ADOC_PUTS
ADOC_PUT(ThreeIntegers(ADOC_ARG, "MagIndex", tsParam->magIndex[0], 
                       tsParam->magIndex[1], tsParam->magIndex[2]));
ADOC_PUT(ThreeIntegers(ADOC_ARG, "LowMagIndex", tsParam->lowMagIndex[0], 
                       tsParam->lowMagIndex[1], tsParam->lowMagIndex[2]));
ADOC_PUT(ThreeIntegers(ADOC_ARG, "BidirAnchMagIndReg",
                       tsParam->bidirAnchorMagInd[0], tsParam->bidirAnchorMagInd[2],
                       tsParam->bidirAnchorMagInd[4]));
ADOC_PUT(ThreeIntegers(ADOC_ARG, "BidirAnchMagIndLD",
                       tsParam->bidirAnchorMagInd[1], tsParam->bidirAnchorMagInd[3],
                       tsParam->bidirAnchorMagInd[5]));
if (tsParam->numExtraExposures) {
  str = mWinApp->mParamIO->EntryListToString(1, 6, tsParam->numExtraExposures,
    NULL, tsParam->extraExposures);
  ADOC_PUT(KeyValue(ADOC_ARG, "ExtraExposures", (LPCTSTR)str));
}
if (tsParam->numExtraFocus) {
  str = mWinApp->mParamIO->EntryListToString(1, 6, tsParam->numExtraFocus,
    NULL, tsParam->extraFocus);
  ADOC_PUT(KeyValue(ADOC_ARG, "ExtraFocuses", (LPCTSTR)str));
}
if (tsParam->numExtraFilter) {
  str = mWinApp->mParamIO->EntryListToString(1, 6, tsParam->numExtraFilter,
    NULL, tsParam->extraLosses);
  ADOC_PUT(KeyValue(ADOC_ARG, "ExtraLosses", (LPCTSTR)str));
  str = mWinApp->mParamIO->EntryListToString(3, 6, tsParam->numExtraFilter,
    tsParam->extraSlits, NULL);
  ADOC_PUT(KeyValue(ADOC_ARG, "ExtraSlits", (LPCTSTR)str));
}
if (tsParam->numExtraChannels) {
  str = mWinApp->mParamIO->EntryListToString(3, 6, tsParam->numExtraChannels,
    tsParam->extraChannels, NULL);
  ADOC_PUT(KeyValue(ADOC_ARG, "ExtraChannels", (LPCTSTR)str));
}
if (tsParam->numVaryItems) {
  outInd = 0;
  for (ind2 = 0; ind2 < tsParam->numVaryItems; ind2++) {
    varyVals[outInd++] = tsParam->varyArray[ind2].angle;
    varyVals[outInd++] = tsParam->varyArray[ind2].plusMinus ? 1.f : 0.f;
    varyVals[outInd++] = tsParam->varyArray[ind2].linear ? 1.f : 0.f;
    varyVals[outInd++] = tsParam->varyArray[ind2].type;
    varyVals[outInd++] = tsParam->varyArray[ind2].value;
  }
  ADOC_PUT(FloatArray(ADOC_ARG, "VaryArray", varyVals, outInd));
}
#endif
#endif

#ifdef NAV_MONT_PARAMS
INT_SETT_ASSIGN("XFrame", montParam->xFrame)
INT_SETT_ASSIGN("YFrame", montParam->yFrame)
INT_SETT_ASSIGN("XNframes", montParam->xNframes)
INT_SETT_ASSIGN("YNframes", montParam->yNframes)
INT_SETT_ASSIGN("XOverlap", montParam->xOverlap)
INT_SETT_ASSIGN("YOverlap", montParam->yOverlap)
INT_SETT_ASSIGN("Binning", montParam->binning)
INT_SETT_ASSIGN("MagIndex", montParam->magIndex)
INT_SETT_ASSIGN("CameraIndex", montParam->cameraIndex)
INT_SETT_ASSIGN("OverviewBinning", montParam->overviewBinning)
BOOL_SETT_ASSIGN("MoveStage", montParam->moveStage)
BOOL_SETT_ASSIGN("OfferToMakeMap", montParam->makeNewMap)
BOOL_SETT_ASSIGN("CloseMontWhenDone", montParam->closeFileWhenDone)
BOOL_SETT_ASSIGN("AdjustFocus", montParam->adjustFocus)
BOOL_SETT_ASSIGN("CorrectDrift", montParam->correctDrift)
BOOL_SETT_ASSIGN("FocusAfterStage", montParam->focusAfterStage)
BOOL_SETT_ASSIGN("RepeatFocus", montParam->repeatFocus)
FLOAT_SETT_ASSIGN("DriftLimit", montParam->driftLimit)
BOOL_SETT_ASSIGN("ShowOverview", montParam->showOverview)
BOOL_SETT_ASSIGN("ShiftInOverview", montParam->shiftInOverview)
BOOL_SETT_ASSIGN("VerySloppy", montParam->verySloppy)
BOOL_SETT_ASSIGN("EvalMultiplePeaks", montParam->evalMultiplePeaks)
INT_SETT_ASSIGN("NumToSkip", montParam->numToSkip)
INT_SETT_ASSIGN("FitToPolygonID", montParam->fitToPolygonID)
BOOL_SETT_ASSIGN("IgnoreSkipList", montParam->ignoreSkipList)
INT_SETT_ASSIGN("InsideNavItem", montParam->insideNavItem)
BOOL_SETT_ASSIGN("SkipOutsidePoly", montParam->skipOutsidePoly)
BOOL_SETT_ASSIGN("WasFitToPolygon", montParam->wasFitToPolygon)
BOOL_SETT_ASSIGN("UseMontMapParams", montParam->useMontMapParams)
BOOL_SETT_ASSIGN("UseViewInLowDose", montParam->useViewInLowDose)
BOOL_SETT_ASSIGN("UseSearchInLowDose", montParam->useSearchInLowDose)
BOOL_SETT_ASSIGN("UseMultiShot", montParam->useMultiShot)
BOOL_SETT_ASSIGN("SetupInLowDose", montParam->setupInLowDose)
BOOL_SETT_ASSIGN("UseContinuousMode", montParam->useContinuousMode)
FLOAT_SETT_ASSIGN("ContinDelayFactor", montParam->continDelayFactor)
BOOL_SETT_ASSIGN("NoDriftCorr", montParam->noDriftCorr)
BOOL_SETT_ASSIGN("NoHQDriftCorr", montParam->noHQDriftCorr)
BOOL_SETT_ASSIGN("UseHqParams", montParam->useHqParams)
BOOL_SETT_ASSIGN("FocusInBlocks", montParam->focusInBlocks)
INT_SETT_ASSIGN("FocusBlockSize", montParam->focusBlockSize)
BOOL_SETT_ASSIGN("ImShiftInBlocks", montParam->imShiftInBlocks)
BOOL_SETT_ASSIGN("RealignToPiece", montParam->realignToPiece)
INT_SETT_ASSIGN("RealignInterval", montParam->realignInterval)
FLOAT_SETT_ASSIGN("MaxAlignFrac", montParam->maxAlignFrac)
FLOAT_SETT_ASSIGN("HqDelayTime", montParam->hqDelayTime)
BOOL_SETT_ASSIGN("ISrealign", montParam->ISrealign)
FLOAT_SETT_ASSIGN("MaxRealignIS", montParam->maxRealignIS)
BOOL_SETT_ASSIGN("UseAnchorWithIS", montParam->useAnchorWithIS)
INT_SETT_ASSIGN("AnchorMagInd", montParam->anchorMagInd)
BOOL_SETT_ASSIGN("SkipCorrelations", montParam->skipCorrelations)
BOOL_SETT_ASSIGN("SkipRecReblanks", montParam->skipRecReblanks)
BOOL_SETT_ASSIGN("ForFullMontage", montParam->forFullMontage)
FLOAT_SETT_ASSIGN("FullMontStageX", montParam->fullMontStageX)
FLOAT_SETT_ASSIGN("FullMontStageY", montParam->fullMontStageY)
INT_SETT_ASSIGN("RefCount", montParam->refCount)
INT_SETT_ASSIGN("NavID", montParam->navID)
INT_SETT_ASSIGN("Reusability", montParam->reusability)
#ifndef SKIP_ADOC_PUTS
if (montParam->numToSkip > 0) {
  str = mWinApp->mParamIO->EntryListToString(4, 6, montParam->numToSkip,
    NULL, NULL, &montParam->skipPieceX[0]);
  ADOC_PUT(KeyValue(ADOC_ARG, "SkipPieceX", (LPCTSTR)str));
  str = mWinApp->mParamIO->EntryListToString(4, 6, montParam->numToSkip,
    NULL, NULL, &montParam->skipPieceY[0]);
  ADOC_PUT(KeyValue(ADOC_ARG, "SkipPieceY", (LPCTSTR)str));
}
#endif
#endif

#ifdef NAV_FILE_OPTS
INT_SETT_ASSIGN("Mode", fileOpt->mode)
INT_SETT_ASSIGN("Typext", fileOpt->typext)
INT_SETT_ASSIGN("MaxSec", fileOpt->maxSec)
BOOL_SETT_ASSIGN("UseMdoc", fileOpt->useMdoc)
BOOL_SETT_ASSIGN("MontageInMdoc", fileOpt->montageInMdoc)
FLOAT_SETT_ASSIGN("PctTruncLo", fileOpt->pctTruncLo)
FLOAT_SETT_ASSIGN("PctTruncHi", fileOpt->pctTruncHi)
INT_SETT_ASSIGN("UnsignOpt", fileOpt->unsignOpt)
INT_SETT_ASSIGN("SignToUnsignOpt", fileOpt->signToUnsignOpt)
INT_SETT_ASSIGN("FileType", fileOpt->fileType)
INT_SETT_ASSIGN("Compression", fileOpt->compression)
INT_SETT_ASSIGN("HdfCompression", fileOpt->hdfCompression)
INT_SETT_ASSIGN("JpegQuality", fileOpt->jpegQuality)
INT_SETT_ASSIGN("TIFFallowed", fileOpt->TIFFallowed)
BOOL_SETT_ASSIGN("SeparateForMont", fileOpt->separateForMont)
BOOL_SETT_ASSIGN("MontUseMdoc", fileOpt->montUseMdoc)
BOOL_SETT_ASSIGN("LeaveExistingMdoc", fileOpt->leaveExistingMdoc)
INT_SETT_ASSIGN("MontFileType", fileOpt->montFileType)
INT_SETT_ASSIGN("RefCount", fileOpt->refCount)
INT_SETT_ASSIGN("NavID", fileOpt->navID)
#endif
