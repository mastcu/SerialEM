// When you split a section be sure to change ParameterIO in all 4 places!
#ifdef SETTINGS_MODULES
  CCameraController *camera = mWinApp->mCamera;
  MontParam *montParam = mWinApp->GetMontParam();
  MacroControl *macControl = mWinApp->GetMacControl();
  TiltSeriesParam *tsParam = mTSParam;
// mBufferManager, mFilterParam must be set as well as mTSParam
#endif
  // This section is close to full 11/24/17
#ifdef SET_TEST_SECT1
FLOAT_SETT_GETSET("AreaFractionForScaling", mWinApp->, PctAreaFraction)
FLOAT_SETT_GETSET("FFTTruncDiameter", mWinApp->, TruncDiamOfFFT)
INT_SETT_GETSET("FFTBackgroundGray", mWinApp->, BkgdGrayOfFFT)
BOOL_SETT_GETSET("UseViewForSearch", mWinApp->, UseViewForSearch)
BOOL_SETT_GETSET("UseRecordForMontage", mWinApp->, UseRecordForMontage)
BOOL_SETT_GETSET("SettingsFixedForIACal", mWinApp->, SettingsFixedForIACal)
BOOL_SETT_GETSET("BasicMode", mWinApp->, BasicMode)
BOOL_SETT_GETSET("OneK2FramePerFile", camera->, OneK2FramePerFile)
BOOL_SETT_GETSET("SkipK2FrameRotFlip", camera->, SkipK2FrameRotFlip)
INT_SETT_GETSET("SaveK2FramesAsTiff", camera->, K2SaveAsTiff)
INT_SETT_GETSET("SaveRawPacked", camera->, SaveRawPacked)
INT_SETT_GETSET("SaveTimes100", camera->, SaveTimes100)
BOOL_SETT_GETSET("SaveUnnormedFrames", camera->, SaveUnnormalizedFrames)
BOOL_SETT_GETSET("Use4BitMRCMode", camera->, Use4BitMrcMode)
BOOL_SETT_GETSET("SaveSuperResReduced", camera->, SaveSuperResReduced)
BOOL_SETT_GETSET("TakeK3SuperResBinned", camera->, TakeK3SuperResBinned)
BOOL_SETT_GETSET("UseK3CDS", camera->, UseK3CorrDblSamp)
BOOL_SETT_GETSET("NameFramesAsMRCS", camera->, NameFramesAsMRCS)
BOOL_SETT_GETSET("SaveFrameStackMdoc", camera->, SaveFrameStackMdoc)
INT_SETT_GETSET("AntialiasBinning", camera->, AntialiasBinning)
INT_SETT_GETSET("NumFrameAliLogLines", camera->, NumFrameAliLogLines)
BOOL_SETT_GETSET("SaveInEERformat", camera->, SaveInEERformat)
INT_SETT_GETSET("BuffersToRollOnAcquire", mBufferManager->, ShiftsOnAcquire)
INT_SETT_GETSET("BufferToReadInto", mBufferManager->, BufToReadInto)
INT_SETT_GETSET("FifthCopyToBuf", mBufferManager->, FifthCopyToBuf)
INT_SETT_GETSET("AutoZoom", mBufferManager->, AutoZoom)
INT_SETT_GETSET("Antialias", mBufferManager->, Antialias)
INT_SETT_GETSET("FixedZoomSteps", mBufferManager->, FixedZoomStep)
INT_SETT_GETSET("DrawScaleBar", mBufferManager->, DrawScaleBar)
BOOL_SETT_GETSET("DrawCrosshairs", mBufferManager->, DrawCrosshairs)
BOOL_SETT_GETSET("DrawTiltAxis", mBufferManager->, DrawTiltAxis)
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
#endif
#ifdef SET_TEST_SECT15
INT_SETT_GETSET("SelectedCameraParameterSet", mWinApp->, SelectedConSet)
BOOL_SETT_GETSET("MonospacedLog", mWinApp->, MonospacedLog)
FLOAT_SETT_GETSET("TargetDefocus", mWinApp->mFocusManager->, TargetDefocus)
FLOAT_SETT_GETSET("AutofocusOffset", mWinApp->mFocusManager->, DefocusOffset)
FLOAT_SETT_GETSET("RefocusThreshold", mWinApp->mFocusManager->, RefocusThreshold)
FLOAT_SETT_GETSET("AbortThreshold", mWinApp->mFocusManager->, AbortThreshold)
FLOAT_SETT_GETSET("AutofocusBeamTilt", mWinApp->mFocusManager->, BeamTilt)
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
INT_SETT_GETSET("IdleTimeToCloseValves", mWinApp->mScope->, IdleTimeToCloseValves)
BOOL_SETT_GETSET("NormAutofocusViaView", mWinApp->mFocusManager->, NormalizeViaView)
BOOL_SETT_GETSET("SkipBlankingInLowDose", mWinApp->mScope->, SkipBlankingInLowDose)
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
BOOL_SETT_GETSET("ScriptUseMonoFont", mWinApp->mMacroProcessor->, UseMonoFont)
BOOL_SETT_GETSET("ShowIndentButtons", mWinApp->mMacroProcessor->, ShowIndentButtons)
BOOL_SETT_GETSET("RestoreMacroEditors", mWinApp->mMacroProcessor->, RestoreMacroEditors)
INT_SETT_GETSET("ZbyGMaxTotalChange", mWinApp->mParticleTasks->, ZBGMaxTotalChange)
FLOAT_SETT_GETSET("ZbyGIterThreshold", mWinApp->mParticleTasks->, ZBGIterThreshold)
BOOL_SETT_GETSET("ZbyGUseViewInLD", mWinApp->mParticleTasks->, ZbyGUseViewInLD)

#endif
#ifdef SET_TEST_SECT2
BOOL_SETT_GETSET("CircleOnLiveFFT", mWinApp->mProcessImage->, CircleOnLiveFFT)
BOOL_SETT_GETSET("SideBySideFFT", mWinApp->mProcessImage->, SideBySideFFT)
BOOL_SETT_GETSET("AutoSingleFFT", mWinApp->mProcessImage->, AutoSingleFFT)
FLOAT_SETT_GETSET("PhasePlateShift", mWinApp->mProcessImage->, PlatePhase)
FLOAT_SETT_GETSET("ImageReductionFac", mWinApp->mProcessImage->, ReductionFactor)
FLOAT_SETT_GETSET("FixedRingDefocus", mWinApp->mProcessImage->, FixedRingDefocus)
FLOAT_SETT_GETSET("UserMaxCtfFitRes", mWinApp->mProcessImage->, UserMaxCtfFitRes)
FLOAT_SETT_GETSET("MinCtfFitResIfPhase", mWinApp->mProcessImage->, MinCtfFitResIfPhase)
BOOL_SETT_GETSET("AutosaveSettings", mWinApp->mDocWnd->, AutoSaveSettings)
BOOL_SETT_GETSET("AutosaveNavNew", mWinApp->mDocWnd->, AutoSaveNav)
BOOL_SETT_GETSET("SkipFileDlg", mWinApp->mDocWnd->, SkipFileDlg)
BOOL_SETT_GETSET("AutosaveLog", mWinApp->mTSController->, AutosaveLog)
BOOL_SETT_GETSET("AutosaveXYZ", mWinApp->mTSController->, AutosaveXYZ)
BOOL_SETT_GETSET("StageMoveToolImage", mWinApp->, ImageWithStageToolMove)
BOOL_SETT_GETSET("LoadMapsUnbinned", mWinApp->mNavHelper->, LoadMapsUnbinned)
BOOL_SETT_GETSET("WriteNavAsXML", mWinApp->mNavHelper->, WriteNavAsXML)
BOOL_SETT_GETSET("TryRealignScaling", mWinApp->mNavHelper->, TryRealignScaling)
BOOL_SETT_GETSET("PlusMinusRIScaling", mWinApp->mNavHelper->, PlusMinusRIScaling)
INT_SETT_GETSET("PointLabelDrawThresh", mWinApp->mNavHelper->, PointLabelDrawThresh)
INT_SETT_GETSET("UseLabelInFilenames", mWinApp->mNavHelper->, UseLabelInFilenames)
INT_SETT_GETSET("MultiShotEnable", mWinApp->mNavHelper->, EnableMultiShot)
BOOL_SETT_GETSET("RotAlignDoSearch", mWinApp->mNavHelper->, SearchRotAlign)
FLOAT_SETT_GETSET("RotAlignRange", mWinApp->mNavHelper->, RotAlignRange)
FLOAT_SETT_GETSET("GridGroupSize", mWinApp->mNavHelper->, GridGroupSize)
BOOL_SETT_GETSET("DivideGridIntoGroups", mWinApp->mNavHelper->, DivideIntoGroups)
BOOL_SETT_GETSET("RIuseCurrentLDparams", mWinApp->mNavHelper->, RIuseCurrentLDparams)
BOOL_SETT_GETSET("SkipMontFitDlgs", mWinApp->mNavHelper->, SkipMontFitDlgs)
INT_SETT_GETSET("CurAcqParamIndex", mWinApp->mNavHelper->, CurAcqParamIndex)
INT_SETT_GETSET("CameraDivide16BitBy2", camera->, DivideBy2)
INT_SETT_GETSET("ExtraDivideBy2", camera->, ExtraDivideBy2)
BOOL_SETT_GETSET("AcquireFloatImages", camera->, AcquireFloatImages)
BOOL_SETT_GETSET("InterpolateDarkRefs", camera->, InterpDarkRefs)
BOOL_SETT_GETSET("NoNormOfDSDoseFrac", camera->, NoNormOfDSdoseFrac)
BOOL_SETT_GETSET("ComPathIsFramePath", camera->, ComPathIsFramePath)
BOOL_SETT_GETSET("IgnoreHigherGainRefs", mWinApp->mGainRefMaker->, IgnoreHigherRefs)
BOOL_SETT_GETSET("UseOlderBinned2Ref", mWinApp->mGainRefMaker->, UseOlderBinned2)
INT_SETT_GETSET("DMgainRefAskPolicy", mWinApp->mGainRefMaker->, DMrefAskPolicy)
BOOL_SETT_GETSET("GainRefCalibrateDose", mWinApp->mGainRefMaker->, CalibrateDose)
#endif
#ifdef SET_TEST_SECT25
BOOL_SETT_ASSIGN("LowDoseBlankWhenDown", mWinApp->mLowDoseDlg.m_bBlankWhenDown)
BOOL_SETT_ASSIGN("LowDoseNormalizeBeam", mWinApp->mLowDoseDlg.m_bNormalizeBeam)
FLOAT_SETT_GETSET("LowDoseViewDefocus", mWinApp->mScope->, LDViewDefocus)
FLOAT_SETT_GETSET("LowDoseSearchDefocus", mWinApp->mScope->, SearchDefocus)
BOOL_SETT_ASSIGN("LowDoseTieFocusTrial", mWinApp->mLowDoseDlg.m_bTieFocusTrial)
BOOL_SETT_ASSIGN("LowDoseRotateAxis", mWinApp->mLowDoseDlg.m_bRotateAxis)
BOOL_SETT_GETSET("TieFocusTrialPos", mWinApp->mLowDoseDlg., TieFocusTrialPos)
INT_SETT_ASSIGN("LowDoseAxisAngle", mWinApp->mLowDoseDlg.m_iAxisAngle)
FLOAT_SETT_GETSET("WalkUpTargetAngle", mWinApp->mComplexTasks->, WalkTarget)
FLOAT_SETT_GETSET("WalkUpMaxInterval", mWinApp->mComplexTasks->, MaxWalkInterval)
FLOAT_SETT_GETSET("WalkUpMinInterval", mWinApp->mComplexTasks->, MinWalkInterval)
FLOAT_SETT_GETSET("WalkSTEMfocusInterval", mWinApp->mComplexTasks->, 
            WalkSTEMfocusInterval)
BOOL_SETT_GETSET("ResetRealignUseTrialInLowDose", mWinApp->mComplexTasks->,
            RSRAUseTrialInLDMode)
BOOL_SETT_GETSET("FineEucenUseTrialInLowDose", mWinApp->mComplexTasks->, FEUseTrialInLD)
BOOL_SETT_GETSET("RoughEucenUseSearchIfInLM", mWinApp->mComplexTasks->, FEUseSearchIfInLM)
BOOL_SETT_GETSET("WalkupUseViewInLowDose", mWinApp->mComplexTasks->, WalkUseViewInLD)
BOOL_SETT_GETSET("TasksUseViewNotSearch", mWinApp->mComplexTasks->, TasksUseViewNotSearch)
BOOL_SETT_GETSET("ComplexTasksVerbose", mWinApp->mComplexTasks->, Verbose)
BOOL_SETT_GETSET("UseEasyAutocen", mWinApp->mMultiTSTasks->, UseEasyAutocen)
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
FLOAT_SETT_GETSET("MinCtfBasedDefocus", mWinApp->mAutoTuning->, MinCtfBasedDefocus)
#endif
#ifdef SET_TEST_SECT3
#include "NavAdocParams.h"
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
INT_SETT_GETSET("ReorderDoseSymFile", mWinApp->mTSController->, ReorderDoseSymFile)
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
