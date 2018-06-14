#ifdef SETTINGS_MODULES
  CCameraController *camera = mWinApp->mCamera;
  MontParam *montParam = mWinApp->GetMontParam();
  MacroControl *macControl = mWinApp->GetMacControl();
// mBufferManager, mFilterParam, mTSParam
#endif
  // This section is close to full 11/24/17
#ifdef SET_TEST_SECT1
FLOAT_SETT_GETSET("AreaFractionForScaling", mWinApp->, PctAreaFraction)
FLOAT_SETT_GETSET("FFTTruncDiameter", mWinApp->, TruncDiamOfFFT)
INT_SETT_GETSET("FFTBackgroundGray", mWinApp->, BkgdGrayOfFFT)
BOOL_SETT_GETSET("UseViewForSearch", mWinApp->, UseViewForSearch)
BOOL_SETT_GETSET("UseRecordForMontage", mWinApp->, UseRecordForMontage)
BOOL_SETT_GETSET("OneK2FramePerFile", camera->, OneK2FramePerFile)
BOOL_SETT_GETSET("SkipK2FrameRotFlip", camera->, SkipK2FrameRotFlip)
INT_SETT_GETSET("SaveK2FramesAsTiff", camera->, K2SaveAsTiff)
INT_SETT_GETSET("SaveRawPacked", camera->, SaveRawPacked)
INT_SETT_GETSET("SaveTimes100", camera->, SaveTimes100)
BOOL_SETT_GETSET("SaveUnnormedFrames", camera->, SaveUnnormalizedFrames)
BOOL_SETT_GETSET("Use4BitMRCMode", camera->, Use4BitMrcMode)
BOOL_SETT_GETSET("SaveSuperResReduced", camera->, SaveSuperResReduced)
BOOL_SETT_GETSET("NameFramesAsMRCS", camera->, NameFramesAsMRCS)
BOOL_SETT_GETSET("AntialiasBinning", camera->, AntialiasBinning)
INT_SETT_GETSET("NumFrameAliLogLines", camera->, NumFrameAliLogLines)
INT_SETT_GETSET("BuffersToRollOnAcquire", mBufferManager->, ShiftsOnAcquire)
INT_SETT_GETSET("BufferToReadInto", mBufferManager->, BufToReadInto)
INT_SETT_GETSET("FifthCopyToBuf", mBufferManager->, FifthCopyToBuf)
INT_SETT_GETSET("AutoZoom", mBufferManager->, AutoZoom)
INT_SETT_GETSET("Antialias", mBufferManager->, Antialias)
INT_SETT_GETSET("FixedZoomSteps", mBufferManager->, FixedZoomStep)
INT_SETT_GETSET("DrawScaleBar", mBufferManager->, DrawScaleBar)
BOOL_SETT_GETSET("DrawCrosshairs", mBufferManager->, DrawCrosshairs)
BOOL_SETT_ASSIGN("MontageCorrectDrift", montParam->correctDrift)
BOOL_SETT_ASSIGN("MontageAdjustFocus", montParam->adjustFocus)
BOOL_SETT_ASSIGN("MontageFocusAfterStage", montParam->focusAfterStage)
BOOL_SETT_ASSIGN("MontageShowOverview", montParam->showOverview)
BOOL_SETT_ASSIGN("MontageAlignPieces", montParam->shiftInOverview)
BOOL_SETT_ASSIGN("MontageVerySloppy", montParam->verySloppy)
INT_SETT_GETSET("MontageMinimumCounts", mWinApp->mMontageController->, CountLimit)
BOOL_SETT_ASSIGN("MontageUseMontMap", montParam->useMontMapParams)
BOOL_SETT_ASSIGN("MontageUseViewInLD", montParam->useViewInLowDose)
BOOL_SETT_ASSIGN("MontageUseSearchInLD", montParam->useSearchInLowDose)
BOOL_SETT_ASSIGN("MontageUseHQparams", montParam->useHqParams)
BOOL_SETT_ASSIGN("MontageFocusInBlocks", montParam->focusInBlocks)
BOOL_SETT_ASSIGN("MontageRepeatFocus", montParam->repeatFocus)
FLOAT_SETT_ASSIGN("MontageDriftLimit", montParam->driftLimit)
BOOL_SETT_ASSIGN("MontageRealign", montParam->realignToPiece)
BOOL_SETT_ASSIGN("MontageISRealign", montParam->ISrealign)
FLOAT_SETT_ASSIGN("MontageMaxRealignIS", montParam->maxRealignIS)
BOOL_SETT_ASSIGN("MontageUseAnchor", montParam->useAnchorWithIS)
INT_SETT_ASSIGN("MontageAnchorMagInd", montParam->anchorMagInd)
BOOL_SETT_ASSIGN("MontageSkipCorrelations", montParam->skipCorrelations)
BOOL_SETT_ASSIGN("MontageSkipReblanks", montParam->skipRecReblanks)
INT_SETT_ASSIGN("MontageFocusBlockSize", montParam->focusBlockSize)
INT_SETT_ASSIGN("MontageRealignInterval", montParam->realignInterval)
FLOAT_SETT_ASSIGN("MontageMinOverlapFactor", montParam->minOverlapFactor)
FLOAT_SETT_ASSIGN("MontageMinMicronsOverlap", montParam->minMicronsOverlap)
FLOAT_SETT_ASSIGN("MontageHQdelayTime", montParam->hqDelayTime)
FLOAT_SETT_ASSIGN("MontageMaxAlignFrac", montParam->maxAlignFrac)
INT_SETT_GETSET("SelectedCameraParameterSet", mWinApp->, SelectedConSet)
FLOAT_SETT_GETSET("TargetDefocus", mWinApp->mFocusManager->, TargetDefocus)
FLOAT_SETT_GETSET("AutofocusOffset", mWinApp->mFocusManager->, DefocusOffset)
FLOAT_SETT_GETSET("RefocusThreshold", mWinApp->mFocusManager->, RefocusThreshold)
FLOAT_SETT_GETSET("AbortThreshold", mWinApp->mFocusManager->, AbortThreshold)
DOUBLE_SETT_GETSET("AutofocusBeamTilt", mWinApp->mFocusManager->, BeamTilt)
INT_SETT_GETSET("BeamTiltDirection", mWinApp->mFocusManager->, TiltDirection)
INT_SETT_GETSET("MinDDDfocusBinning", mWinApp->mFocusManager->, DDDminBinning)
BOOL_SETT_GETSET("DriftProtection", mWinApp->mFocusManager->, TripleMode)
BOOL_SETT_GETSET("FocusVerbose", mWinApp->mFocusManager->, Verbose)
FLOAT_SETT_GETSET("TiltIncrement", mWinApp->mScope->, Increment)
BOOL_SETT_GETSET("CosineTilting", mWinApp->mScope->, CosineTilt)
BOOL_SETT_GETSET("ShiftToTiltAxis", mWinApp->mScope->, ShiftToTiltAxis)
BOOL_SETT_GETSET("ApplyISoffset", mWinApp->mScope->, ApplyISoffset)
BOOL_SETT_GETSET("UsePiezoForLDAxis", mWinApp->mScope->, UsePiezoForLDaxis)
BOOL_SETT_GETSET("AdjustFocusForProbe", mWinApp->mScope->, AdjustFocusForProbe)
INT_SETT_GETSET("NormalizeAllOnMagChange", mWinApp->mScope->, NormAllOnMagChange)
BOOL_SETT_GETSET("NormAutofocusViaView", mWinApp->mFocusManager->, NormalizeViaView)
BOOL_SETT_GETSET("TrimBordersInAutoalign", mWinApp->mShiftManager->, TrimDarkBorders)
INT_SETT_GETSET("DisableAutoTrim", mWinApp->mShiftManager->, DisableAutoTrim)
BOOL_SETT_GETSET("MoveStageOnBigMouseShift", mWinApp->mShiftManager->, MouseMoveStage)
BOOL_SETT_GETSET("BacklashMouseAndISR", mWinApp->mShiftManager->, BacklashMouseAndISR)
FLOAT_SETT_GETSET("MouseStageThreshold", mWinApp->mShiftManager->, MouseStageThresh)
FLOAT_SETT_GETSET("MouseStageAbsoluteThreshold", mWinApp->mShiftManager->,
             MouseStageAbsThresh)
FLOAT_SETT_GETSET("TiltDelay", mWinApp->mShiftManager->, TiltDelay)
BOOL_SETT_ASSIGN("MacroLimitRuns", macControl->limitRuns)
BOOL_SETT_ASSIGN("MacroLimitScaleMax", macControl->limitScaleMax)
BOOL_SETT_ASSIGN("MacroLimitTiltUp", macControl->limitTiltUp)
BOOL_SETT_ASSIGN("MacroLimitTiltDown", macControl->limitTiltDown)
BOOL_SETT_ASSIGN("MacroLimitMontError", macControl->limitMontError)
BOOL_SETT_ASSIGN("MacroLimitIS", macControl->limitIS)
INT_SETT_ASSIGN("MacroRunLimit", macControl->runLimit)
INT_SETT_ASSIGN("MacroScaleMaxLimit", macControl->scaleMaxLimit)
FLOAT_SETT_ASSIGN("MacroTiltUpLimit", macControl->tiltUpLimit)
FLOAT_SETT_ASSIGN("MacroTiltDownLimit", macControl->tiltDownLimit)
FLOAT_SETT_ASSIGN("MacroMontErrorLimit", macControl->montErrorLimit)
FLOAT_SETT_ASSIGN("MacroISLimit", macControl->ISlimit)
INT_SETT_GETSET("ScriptIndentSize", mWinApp->mMacroProcessor->, AutoIndentSize)

#endif
#ifdef SET_TEST_SECT2
BOOL_SETT_GETSET("CircleOnLiveFFT", mWinApp->mProcessImage->, CircleOnLiveFFT)
BOOL_SETT_GETSET("SideBySideFFT", mWinApp->mProcessImage->, SideBySideFFT)
BOOL_SETT_GETSET("AutoSingleFFT", mWinApp->mProcessImage->, AutoSingleFFT)
FLOAT_SETT_GETSET("PhasePlateShift", mWinApp->mProcessImage->, PlatePhase)
FLOAT_SETT_GETSET("ImageReductionFac", mWinApp->mProcessImage->, ReductionFactor)
FLOAT_SETT_GETSET("FixedRingDefocus", mWinApp->mProcessImage->, FixedRingDefocus)
FLOAT_SETT_GETSET("UserMaxCtfFitRes", mWinApp->mProcessImage->, UserMaxCtfFitRes)
BOOL_SETT_GETSET("AutosaveSettings", mWinApp->mDocWnd->, AutoSaveSettings)
BOOL_SETT_GETSET("AutosaveNavNew", mWinApp->mDocWnd->, AutoSaveNav)
BOOL_SETT_GETSET("SkipFileDlg", mWinApp->mDocWnd->, SkipFileDlg)
BOOL_SETT_GETSET("AutosaveLog", mWinApp->mTSController->, AutosaveLog)
BOOL_SETT_GETSET("AutosaveXYZ", mWinApp->mTSController->, AutosaveXYZ)
BOOL_SETT_GETSET("StageMoveToolImage", mWinApp->, ImageWithStageToolMove)
BOOL_SETT_GETSET("LoadMapsUnbinned", mWinApp->mNavHelper->, LoadMapsUnbinned)
BOOL_SETT_GETSET("WriteNavAsXML", mWinApp->mNavHelper->, WriteNavAsXML)
BOOL_SETT_GETSET("TryRealignScaling", mWinApp->mNavHelper->, TryRealignScaling)
INT_SETT_GETSET("PointLabelDrawThresh", mWinApp->mNavHelper->, PointLabelDrawThresh)
INT_SETT_GETSET("MultiShotEnable", mWinApp->mNavHelper->, EnableMultiShot)
BOOL_SETT_GETSET("RotAlignDoSearch", mWinApp->mNavHelper->, SearchRotAlign)
FLOAT_SETT_GETSET("RotAlignRange", mWinApp->mNavHelper->, RotAlignRange)
FLOAT_SETT_GETSET("GridGroupSize", mWinApp->mNavHelper->, GridGroupSize)
BOOL_SETT_GETSET("DivideGridIntoGroups", mWinApp->mNavHelper->, DivideIntoGroups)
BOOL_SETT_GETSET("RIuseCurrentLDparams", mWinApp->mNavHelper->, RIuseCurrentLDparams)
INT_SETT_GETSET("CameraDivide16BitBy2", camera->, DivideBy2)
BOOL_SETT_GETSET("InterpolateDarkRefs", camera->, InterpDarkRefs)
BOOL_SETT_GETSET("NoNormOfDSDoseFrac", camera->, NoNormOfDSdoseFrac)
BOOL_SETT_GETSET("ComPathIsFramePath", camera->, ComPathIsFramePath)
BOOL_SETT_GETSET("IgnoreHigherGainRefs", mWinApp->mGainRefMaker->, IgnoreHigherRefs)
BOOL_SETT_GETSET("UseOlderBinned2Ref", mWinApp->mGainRefMaker->, UseOlderBinned2)
INT_SETT_GETSET("DMgainRefAskPolicy", mWinApp->mGainRefMaker->, DMrefAskPolicy)
BOOL_SETT_GETSET("GainRefCalibrateDose", mWinApp->mGainRefMaker->, CalibrateDose)
BOOL_SETT_ASSIGN("LowDoseBlankWhenDown", mWinApp->mLowDoseDlg.m_bBlankWhenDown)
BOOL_SETT_ASSIGN("LowDoseNormalizeBeam", mWinApp->mLowDoseDlg.m_bNormalizeBeam)
FLOAT_SETT_GETSET("LowDoseViewDefocus", mWinApp->mScope->, LDViewDefocus)
FLOAT_SETT_GETSET("LowDoseSearchDefocus", mWinApp->mScope->, SearchDefocus)
INT_SETT_ASSIGN("LowDoseAreaToShow", mWinApp->mLowDoseDlg.m_iShowArea)
BOOL_SETT_ASSIGN("LowDoseTieFocusTrial", mWinApp->mLowDoseDlg.m_bTieFocusTrial)
BOOL_SETT_ASSIGN("LowDoseRotateAxis", mWinApp->mLowDoseDlg.m_bRotateAxis)
INT_SETT_ASSIGN("LowDoseAxisAngle", mWinApp->mLowDoseDlg.m_iAxisAngle)
FLOAT_SETT_GETSET("WalkUpTargetAngle", mWinApp->mComplexTasks->, WalkTarget)
FLOAT_SETT_GETSET("WalkUpMaxInterval", mWinApp->mComplexTasks->, MaxWalkInterval)
FLOAT_SETT_GETSET("WalkUpMinInterval", mWinApp->mComplexTasks->, MinWalkInterval)
BOOL_SETT_GETSET("ResetRealignUseTrialInLowDose", mWinApp->mComplexTasks->,
            RSRAUseTrialInLDMode)
BOOL_SETT_GETSET("FineEucenUseTrialInLowDose", mWinApp->mComplexTasks->, FEUseTrialInLD)
BOOL_SETT_GETSET("WalkupUseViewInLowDose", mWinApp->mComplexTasks->, WalkUseViewInLD)
BOOL_SETT_GETSET("ComplexTasksVerbose", mWinApp->mComplexTasks->, Verbose)
FLOAT_SETT_GETSET("UsersAstigTilt", mWinApp->mAutoTuning->, UsersAstigTilt)
FLOAT_SETT_GETSET("MenuZemlinTilt", mWinApp->mAutoTuning->, MenuZemlinTilt)
FLOAT_SETT_GETSET("UsersComaTilt", mWinApp->mAutoTuning->, UsersComaTilt)
FLOAT_SETT_GETSET("CtfExposure", mWinApp->mAutoTuning->, CtfExposure)
FLOAT_SETT_GETSET("CtfDriftSettling", mWinApp->mAutoTuning->, CtfDriftSettling)
INT_SETT_GETSET("CtfBinning", mWinApp->mAutoTuning->, CtfBinning)
BOOL_SETT_GETSET("CtfUseFullField", mWinApp->mAutoTuning->, CtfUseFullField)
BOOL_SETT_GETSET("CtfDoFullArray", mWinApp->mAutoTuning->, CtfDoFullArray)
FLOAT_SETT_GETSET("ComaIterationThresh", mWinApp->mAutoTuning->, ComaIterationThresh)
FLOAT_SETT_GETSET("ComaVsISextent", mWinApp->mAutoTuning->, ComaVsISextent)
#endif
#ifdef SET_TEST_SECT3
FLOAT_SETT_ASSIGN("TiltSeriesLastStartingTilt", mTSParam->lastStartingTilt)
BOOL_SETT_ASSIGN("TiltSeriesTrackLowMag", mTSParam->trackLowMag)
INT_SETT_ASSIGN("TiltSeriesTrackAfterFocus", mTSParam->trackAfterFocus)
INT_SETT_ASSIGN("TiltSeriesBeamControl", mTSParam->beamControl)
INT_SETT_ASSIGN("TiltSeriesMeanCounts", mTSParam->meanCounts)
INT_SETT_ASSIGN("TiltSeriesCosinePower", mTSParam->cosinePower)
BOOL_SETT_ASSIGN("TiltSeriesTaperCounts", mTSParam->taperCounts)
INT_SETT_ASSIGN("TiltSeriesHighCounts", mTSParam->highCounts)
FLOAT_SETT_ASSIGN("TiltSeriesTaperAngle", mTSParam->taperAngle)
BOOL_SETT_ASSIGN("TiltSeriesLimitIntensity", mTSParam->limitIntensity)
BOOL_SETT_ASSIGN("TiltSeriesRefineEucentricity", mTSParam->refineEucen)
BOOL_SETT_ASSIGN("TiltSeriesWalkLeaveAnchor", mTSParam->leaveAnchor)
FLOAT_SETT_ASSIGN("TiltSeriesWalkAnchorTilt", mTSParam->anchorTilt)
INT_SETT_ASSIGN("TiltSeriesLeftAnchorBuffer", mTSParam->anchorBuffer)
FLOAT_SETT_ASSIGN("TiltSeriesImageShiftLimit", mTSParam->ISlimit)
FLOAT_SETT_ASSIGN("TiltSeriesFocusCheckInterval", mTSParam->focusCheckInterval)
BOOL_SETT_ASSIGN("TiltSeriesRepeatRecord", mTSParam->repeatRecord)
BOOL_SETT_ASSIGN("TiltSeriesConfirmLDRepeat", mTSParam->confirmLowDoseRepeat)
FLOAT_SETT_ASSIGN("TiltSeriesMaxRecordLoss", mTSParam->maxRecordShift)
BOOL_SETT_ASSIGN("TiltSeriesStopOnBigShift", mTSParam->stopOnBigAlignShift)
FLOAT_SETT_ASSIGN("TiltSeriesMaxAlignShift", mTSParam->maxAlignShift)
BOOL_SETT_ASSIGN("TiltSeriesRepeatAutofocus", mTSParam->repeatAutofocus)
FLOAT_SETT_ASSIGN("TiltSeriesMaxFocusDiff", mTSParam->maxFocusDiff)
BOOL_SETT_ASSIGN("TiltSeriesAlwaysFocusAtHigh", mTSParam->alwaysFocusHigh)
FLOAT_SETT_ASSIGN("TiltSeriesAlwaysFocusAngle", mTSParam->alwaysFocusAngle)
BOOL_SETT_ASSIGN("TiltSeriesSkipAutofocus", mTSParam->skipAutofocus)
BOOL_SETT_ASSIGN("TiltSeriesCheckAutofocus", mTSParam->checkAutofocus)
BOOL_SETT_ASSIGN("TSLimitFocusChange", mTSParam->limitDefocusChange)
FLOAT_SETT_ASSIGN("TSFocusChangeLimit", mTSParam->defocusChangeLimit)
FLOAT_SETT_ASSIGN("TiltSeriesMinFocusAccuracy", mTSParam->minFocusAccuracy)
BOOL_SETT_ASSIGN("TiltSeriesRefineZLP", mTSParam->refineZLP)
INT_SETT_ASSIGN("TiltSeriesZLPInterval", mTSParam->zlpInterval)
BOOL_SETT_ASSIGN("TiltSeriesCenterBeam", mTSParam->centerBeamFromTrial)
BOOL_SETT_ASSIGN("TiltSeriesCloseValvesAtEnd", mTSParam->closeValvesAtEnd)
BOOL_SETT_ASSIGN("TiltSeriesAlignTrackOnly", mTSParam->alignTrackOnly)
BOOL_SETT_ASSIGN("TiltSeriesPreviewBeforeNewRef", mTSParam->previewBeforeNewRef)
FLOAT_SETT_ASSIGN("TiltSeriesMaxRefDisagreement", mTSParam->maxRefDisagreement)
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorXY", mTSParam->maxPredictErrorXY)
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZ", mTSParam->maxPredictErrorZ[0])
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZSN", mTSParam->maxPredictErrorZ[1])
FLOAT_SETT_ASSIGN("TiltSeriesMaxPredictErrorZSM", mTSParam->maxPredictErrorZ[2])
BOOL_SETT_ASSIGN("TiltSeriesCenBeamPeriodically", mTSParam->cenBeamPeriodically)
INT_SETT_ASSIGN("TiltSeriesCenBeamInterval", mTSParam->cenBeamInterval)
BOOL_SETT_ASSIGN("TiltSeriesCenBeamAbove", mTSParam->cenBeamAbove)
FLOAT_SETT_ASSIGN("TiltSeriesCenBeamAngle", mTSParam->cenBeamAngle)
INT_SETT_ASSIGN("TiltSeriesStackRecords", mTSParam->stackRecords)
BOOL_SETT_ASSIGN("TSDoEarlyReturn", mTSParam->doEarlyReturn)
INT_SETT_ASSIGN("TSNumEarlyFrames", mTSParam->earlyReturnNumFrames)
INT_SETT_ASSIGN("TiltSeriesStackBinSize", mTSParam->stackBinSize)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetExposure", mTSParam->extraSetExposure)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetSpot", mTSParam->extraSetSpot)
INT_SETT_ASSIGN("TiltSeriesExtraSpotSize", mTSParam->extraSpotSize)
FLOAT_SETT_ASSIGN("TiltSeriesExtraDeltaIntensity", mTSParam->extraDeltaIntensity)
FLOAT_SETT_ASSIGN("TiltSeriesExtraDrift", mTSParam->extraDrift)
BOOL_SETT_ASSIGN("TiltSeriesExtraSetBinning", mTSParam->extraSetBinning)
INT_SETT_ASSIGN("TiltSeriesExtraBinIndex", mTSParam->extraBinIndex)
BOOL_SETT_ASSIGN("TiltSeriesDoVariations", mTSParam->doVariations)
BOOL_SETT_ASSIGN("TiltSeriesChangeRecExposure", mTSParam->changeRecExposure)
BOOL_SETT_ASSIGN("TiltSeriesChangeAllExposures", mTSParam->changeAllExposures)
BOOL_SETT_ASSIGN("TiltSeriesChangeSettling", mTSParam->changeSettling)
FLOAT_SETT_GETSET("TiltSeriesExposureStep", mWinApp->mTSController->, ExpSeriesStep)
BOOL_SETT_GETSET("ExposureSeriesFixNumFrames", mWinApp->mTSController->,
            ExpSeriesFixNumFrames)
INT_SETT_GETSET("TiltSeriesTiltFailPolicy", mWinApp->mTSController->, TiltFailPolicy)
INT_SETT_GETSET("TiltSeriesLDDimRecordPolicy", mWinApp->mTSController->, LDDimRecordPolicy)
INT_SETT_GETSET("TiltSeriesTermNotAskOnDim", mWinApp->mTSController->, TermNotAskOnDim)
INT_SETT_GETSET("TiltSeriesLDRecordLossPolicy", mWinApp->mTSController->, LDRecordLossPolicy)
INT_SETT_GETSET("TiltSeriesAutoTerminatePolicy", mWinApp->mTSController->,
           AutoTerminatePolicy)
BOOL_SETT_GETSET("TSSeparateExtraRec", mWinApp->mTSController->, SeparateExtraRecFiles)
BOOL_SETT_GETSET("TiltSeriesAutoSavePolicy", mWinApp->mTSController->, AutoSavePolicy)
BOOL_SETT_GETSET("TSCallPlugins", mWinApp->mTSController->, CallTSplugins)
BOOL_SETT_GETSET("TSFixedNumFrames", mWinApp->mTSController->, FixedNumFrames)
BOOL_SETT_GETSET("TSSkipBeamShiftOnAlign", mWinApp->mTSController->, SkipBeamShiftOnAlign)
BOOL_SETT_GETSET("TSAllowContinuous", mWinApp->mTSController->, AllowContinuous)
INT_SETT_GETSET("EmailAfterTiltSeries", mWinApp->mTSController->, SendEmail)
FLOAT_SETT_ASSIGN("FilterSlitWidth", mFilterParam->slitWidth)
FLOAT_SETT_ASSIGN("FilterEnergyLoss", mFilterParam->energyLoss)
BOOL_SETT_ASSIGN("FilterZeroLoss", mFilterParam->zeroLoss)
BOOL_SETT_ASSIGN("FilterChangeMagWithScreen", mFilterParam->autoMag)
BOOL_SETT_ASSIGN("FilterMatchPixel", mFilterParam->matchPixel)
BOOL_SETT_ASSIGN("FilterMatchIntensity", mFilterParam->matchIntensity)
BOOL_SETT_ASSIGN("FilterDoMagShifts", mFilterParam->doMagShifts)
BOOL_SETT_GETSET("STEMmatchPixel", mWinApp->, STEMmatchPixel)
BOOL_SETT_GETSET("ScreenSwitchSTEM", mWinApp->, ScreenSwitchSTEM)
BOOL_SETT_GETSET("InvertSTEMimages", mWinApp->, InvertSTEMimages)
BOOL_SETT_GETSET("BlankBeamInSTEM", mWinApp->, BlankBeamInSTEM)
BOOL_SETT_GETSET("RetractToUnblankSTEM", mWinApp->, RetractToUnblankSTEM)
BOOL_SETT_GETSET("KeepPixelTime", mWinApp->, KeepPixelTime)
FLOAT_SETT_GETSET("RightBorderFrac", mWinApp->, RightBorderFrac)
FLOAT_SETT_GETSET("BottomBorderFrac", mWinApp->, BottomBorderFrac)
FLOAT_SETT_GETSET("MainFFTsplitFrac", mWinApp->, MainFFTsplitFrac)
#endif
