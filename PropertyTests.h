// PropertyTests.h: lists of properties that can be set while the program is running, 
// including pointers needed to set their values 
// (or get them, if there were Get functions for each)
//
// Definitions relied on in the property lists
// Include this file at the top of a function with
// #define PROP_MODULES
// #include "PropertyTests.h"
// #undef PROP_MODULES
#ifdef PROP_MODULES
CComplexTasks *complexTasks = mWinApp->mComplexTasks;
CTSController *tsController = mWinApp->mTSController;
CCameraController *camera = mWinApp->mCamera;
CEMscope *scope = mWinApp->mScope;
CShiftManager *shiftManager =  mWinApp->mShiftManager;
CFocusManager *focusManager =  mWinApp->mFocusManager;
CNavHelper *navHelper =  mWinApp->mNavHelper;
CBeamAssessor *beamAssessor =  mWinApp->mBeamAssessor;
#endif

// Separate sections of the list, each small enough to be used in a chain of else if
// statements
// First define FLOAT_PROP_TEST, INT_PROP_TEST, BOOL_PROP_TEST, and DBL_PROP_TEST
// Define three recognized[123] = true;
// Start a series of else ifs with
// if (false) {
// }
// then
// #define PROP_TEST_SECT1
// #include "PropertyTests.h"
// #undef PROP_TEST_SECT1
// } else
//   recognized1 = false;
// if (recognized1) {
// }
// #define PROP_TEST_SECT2
// #include "PropertyTests.h"
// #undef PROP_TEST_SECT2
// and so on, ending with an error after the final else
//
#ifdef PROP_TEST_SECT1
FLOAT_PROP_TEST("DarkRefAgeLimit", camera->, DarkAgeLimit)
FLOAT_PROP_TEST("GainRefInactivityLimit", camera->, GainTimeLimit)
INT_PROP_TEST("BiggestGIFApertureNumber", camera->, GIFBiggestAperture)
FLOAT_PROP_TEST("MinimumBlankingTime", camera->, MinBlankingTime)
FLOAT_PROP_TEST("CameraTimeoutFactor", camera->, TimeoutFactor)
INT_PROP_TEST("CameraRetries", camera->, RetryLimit)
BOOL_PROP_TEST("MakeFEIerrorBeTimeout", camera->, MakeFEIerrorBeTimeout)
BOOL_PROP_TEST("OtherCamerasInTIA", camera->, OtherCamerasInTIA)
FLOAT_PROP_TEST("ScalingForK2Counts", camera->, ScalingForK2Counts)
FLOAT_PROP_TEST("MinimumK2FrameTime", camera->, MinK2FrameTime)
FLOAT_PROP_TEST("K2ReadoutInterval", camera->, K2ReadoutInterval)
FLOAT_PROP_TEST("MinimumK3FrameTime", camera->, MinK3FrameTime)
FLOAT_PROP_TEST("K3ReadoutInterval", camera->, K3ReadoutInterval)
FLOAT_PROP_TEST("K3HWDarkRefExposure", camera->, K3HWDarkRefExposure)
FLOAT_PROP_TEST("K2BaseModeScaling", camera->, K2BaseModeScaling)
BOOL_PROP_TEST("NoK2SaveFolderBrowse", camera->, NoK2SaveFolderBrowse)
BOOL_PROP_TEST("K2SynchronousSaving", camera->, K2SynchronousSaving)
FLOAT_PROP_TEST("K2MaxRamStackGB", camera->, K2MaxRamStackGB)
FLOAT_PROP_TEST("K2MinStartupDelay", camera->, K2MinStartupDelay)
FLOAT_PROP_TEST("CalibTimingK2InitialStartup", mWinApp->mCalibTiming->, K2InitialStartup)
BOOL_PROP_TEST("TestGpuInShrmemframe", camera->, TestGpuInShrmemframe)
INT_PROP_TEST("SmoothFocusExtraTime", camera->, SmoothFocusExtraTime)
INT_PROP_TEST("DEProtectionCoverDelay", mWinApp->mDEToolDlg., ProtCoverDelay)
INT_PROP_TEST("DETemperatureSetpoint", mWinApp->mDEToolDlg., TemperSetpoint)
INT_PROP_TEST("FalconPostMagDelay", scope->, FalconPostMagDelay)
FLOAT_PROP_TEST("FalconReadoutInterval", camera->, FalconReadoutInterval)
FLOAT_PROP_TEST("Ceta2ReadoutInterval", camera->, Ceta2ReadoutInterval)
INT_PROP_TEST("MaxFalconFrames", camera->, MaxFalconFrames)
INT_PROP_TEST("FrameMdocForFalcon", camera->, FrameMdocForFalcon)
INT_PROP_TEST("SetModeInFalconConfig", camera->, CanUseFalconConfig)
INT_PROP_TEST("RotFlipInFalcon3ComFile", camera->, RotFlipInFalcon3ComFile)
BOOL_PROP_TEST("SubdirsOkInFalcon3Save", camera->, SubdirsOkInFalcon3Save)
BOOL_PROP_TEST("Falcon4CanSaveEER", camera->, CanSaveEERformat)
BOOL_PROP_TEST("TakeUnbinnedIfSavingEER", camera->, TakeUnbinnedIfSavingEER)
FLOAT_PROP_TEST("DEPrevSetNameTimeout", camera->, DEPrevSetNameTimeout)
BOOL_PROP_TEST("WarnIfBeamNotOn", camera->, WarnIfBeamNotOn)
FLOAT_PROP_TEST("InnerXRayBorderDistance", mWinApp->mProcessImage->, InnerXRayDistance)
FLOAT_PROP_TEST("OuterXRayBorderDistance", mWinApp->mProcessImage->, OuterXRayDistance)
INT_PROP_TEST("XRayCriterionIterations", mWinApp->mProcessImage->, XRayCritIterations)
FLOAT_PROP_TEST("XRayCriterionIncrease", mWinApp->mProcessImage->, XRayCritIncrease)
BOOL_PROP_TEST("RetractCameraOnEnteringEFTEM", mWinApp->, RetractOnEFTEM)
BOOL_PROP_TEST("RetractCameraOnEnteringSTEM", mWinApp->, RetractOnSTEM)
BOOL_PROP_TEST("BlankWhenRetractingCamera", camera->, BlankWhenRetracting)
BOOL_PROP_TEST("DefaultActAfterExposures", mWinApp->, ActPostExposure)
BOOL_PROP_TEST("RamperWaitForBlank", camera->, RamperWaitForBlank)
INT_PROP_TEST("StartAsAdministrator", mWinApp->, AdministratorMode)
BOOL_PROP_TEST("IgnoreShortTermCals", mWinApp->mDocWnd->, IgnoreShortTerm)
BOOL_PROP_TEST("DiscardSettings", mWinApp->mDocWnd->, AbandonSettings)
BOOL_PROP_TEST("ExitWithUnsavedLog", mWinApp->, ExitWithUnsavedLog)
BOOL_PROP_TEST("ContinuousSaveLog", mWinApp->, ContinuousSaveLog)
BOOL_PROP_TEST("TestGainFactors", mWinApp->, TestGainFactors)
BOOL_PROP_TEST("ShiftScriptOnlyInAdmin", mWinApp->, ShiftScriptOnlyInAdmin)
INT_PROP_TEST("DoseLifetimeInHours", mWinApp->, DoseLifetimeHours)
INT_PROP_TEST("AddDPItoSnapshots", mWinApp->, AddDPItoSnapshots)
BOOL_PROP_TEST("SkipGainRefWarning", mWinApp->, SkipGainRefWarning)
INT_PROP_TEST("AssumeCameraForDummy", mWinApp->, AssumeCamForDummy)
BOOL_PROP_TEST("AllowCameraInSTEMmode", mWinApp->, AllowCameraInSTEMmode)
BOOL_PROP_TEST("EnableExternalPython", mWinApp->, EnableExternalPython)
FLOAT_PROP_TEST("TietzFilmShutterSwitchTime", camera->, TietzFilmSwitchTime)
INT_PROP_TEST("DynamicFocusInterval", camera->, DynFocusInterval)
INT_PROP_TEST("InitialDynamicFocusDelay", camera->, StartDynFocusDelay)
INT_PROP_TEST("DigiScanExtraShotDelay", camera->, DSextraShotDelay)
INT_PROP_TEST("DigiScanControlBeam", camera->, DScontrolBeam)
FLOAT_PROP_TEST("DigiScanLineSyncWait", camera->, DSsyncMargin)
BOOL_PROP_TEST("CameraSetsShareChannelList", camera->, ConsetsShareChannelList)
BOOL_PROP_TEST("AllowSpectroscopyImages", camera->, AllowSpectroscopyImages)
INT_PROP_TEST("STEMUnsignedOption", mWinApp->mDocWnd->, STEMunsignOpt)
FLOAT_PROP_TEST("UnsignedTruncationLimit", mWinApp->mBufferManager->, UnsignedTruncLimit)
INT_PROP_TEST("FileOptionsSaveMdocFile", mWinApp->mDocWnd->, DfltUseMdoc)
INT_PROP_TEST("MaxStackWindowSizeXY", mWinApp->mBufferManager->, StackWinMaxXY)
BOOL_PROP_TEST("BufferStatusShowFocus", mWinApp->mStatusWindow., ShowDefocus)
FLOAT_PROP_TEST("GlobalExtraRotation", shiftManager->, GlobalExtraRotation)
FLOAT_PROP_TEST("MicronsPerUnitImageShift", shiftManager->, RoughISscale)
FLOAT_PROP_TEST("LowMagMicronsPerUnitIS", shiftManager->, LMRoughISscale)
FLOAT_PROP_TEST("STEMMicronsPerUnitImageShift", shiftManager->, STEMRoughISscale)
FLOAT_PROP_TEST("DeltaZtoDefocusFactor", shiftManager->, DefocusZFactor)
DBL_PROP_TEST("StandardLowMagFocus", scope->, StandardLMFocus)
FLOAT_PROP_TEST("WarnIfKVnotAt", scope->, WarnIfKVnotAt)
FLOAT_PROP_TEST("ScreenCurrentFactor", scope->, ScreenCurrentFactor)
FLOAT_PROP_TEST("SmallScreenFactor", scope->, SmallScreenFactor)
FLOAT_PROP_TEST("FilamentCurrentScale", scope->, FilamentCurrentScale)
INT_PROP_TEST("UpdateBeamBlanker", scope->, UpdateBeamBlank)
INT_PROP_TEST("NoScope", scope->, NoScope)
BOOL_PROP_TEST("BlankDuringTransients", scope->, BlankTransients)
FLOAT_PROP_TEST("StageRelaxation", scope->, StageRelaxation)
BOOL_PROP_TEST("SkipAdvancedScripting", scope->, SkipAdvancedScripting)
BOOL_PROP_TEST("UpdateDuringAreaChange", scope->, UpdateDuringAreaChange)
INT_PROP_TEST("AdvancedScriptingVersion", scope->, AdvancedScriptVersion)
INT_PROP_TEST("HasSimpleOriginSystem", scope->, HasSimpleOrigin)
INT_PROP_TEST("DewarVacCapabilities", scope->, DewarVacCapabilities)
INT_PROP_TEST("ScopeCanFlashFEG", scope->, ScopeCanFlashFEG)
INT_PROP_TEST("ScopeHasPhasePlate", scope->, ScopeHasPhasePlate)
INT_PROP_TEST("SkipNormalizations", scope->, SkipNormalizations)

#endif
#ifdef PROP_TEST_SECT2
INT_PROP_TEST("ContinuousAlignBinning", mWinApp->mFalconHelper->, ContinAliBinning)
INT_PROP_TEST("ContinuousAlignPairwise", mWinApp->mFalconHelper->, ContinAliPairwise)
FLOAT_PROP_TEST("ContinuousAlignFilter", mWinApp->mFalconHelper->, ContinAliFilter)
INT_PROP_TEST("UseGPUforContinuousAlign", mWinApp->mFalconHelper->, GpuForContinuousAli)
INT_PROP_TEST("UseIlluminatedAreaForC2", scope->, UseIllumAreaForC2)
INT_PROP_TEST("LowestMModeMagIndex", scope->, LowestMModeMagInd)
INT_PROP_TEST("LowestEFTEMNonLMIndex", scope->, LowestEFTEMNonLMind)
INT_PROP_TEST("LowestSecondaryModeMag", scope->, LowestSecondaryMag)
INT_PROP_TEST("HighestLMindexToScan", scope->, HighestLMindexToScan)
INT_PROP_TEST("LowestGIFModeMagIndex", scope->, LowestGIFModeMag)
INT_PROP_TEST("LowerScreenForSTEM", camera->, LowerScreenForSTEM)
INT_PROP_TEST("InvertBrightField", camera->, InvertBrightField)
FLOAT_PROP_TEST("ShotTimeToInsertDetector", camera->, InsertDetShotTime)
INT_PROP_TEST("BaseJeolSTEMflags", camera->, BaseJeolSTEMflags)
BOOL_PROP_TEST("MustUnblankWithScreen", mWinApp->, MustUnblankWithScreen)
INT_PROP_TEST("LowestMicroSTEMmag", scope->, LowestMicroSTEMmag)
INT_PROP_TEST("NumberOfSpotSizes", scope->, NumSpotSizes)
INT_PROP_TEST("MinSpotSizeToUse", scope->, MinSpotSize)
INT_PROP_TEST("NumberOfAlphas", scope->, NumAlphas)
FLOAT_PROP_TEST("STEMFocusVsZBacklash", focusManager->, SFVZbacklashZ)
FLOAT_PROP_TEST("STEMCalFocusRange", focusManager->, SFcalRange)
BOOL_PROP_TEST("STEMFocusShowBestAtEnd", focusManager->, SFshowBestAtEnd)
INT_PROP_TEST("FocusViewNormDelay", focusManager->, ViewNormDelay)
FLOAT_PROP_TEST("TiltAxisOffset", scope->, TiltAxisOffset)
INT_PROP_TEST("LensNormalizationDelay", shiftManager->, NormalizationDelay)
INT_PROP_TEST("LowMagNormalizationDelay", shiftManager->, LowMagNormDelay)
INT_PROP_TEST("StageMoveDelay", shiftManager->, StageMovedDelay)
INT_PROP_TEST("MinimumTiltDelay", shiftManager->, MinTiltDelay)
FLOAT_PROP_TEST("ExtraISdelayPerMagDoubling", shiftManager->, DelayPerMagDoubling)
FLOAT_PROP_TEST("StartupForISDelays", shiftManager->, StartupForISDelays)
FLOAT_PROP_TEST("ISdelayScaleFactor", shiftManager->, ISdelayScaleFactor)
FLOAT_PROP_TEST("ImageShiftLimit", shiftManager->, RegularShiftLimit)
FLOAT_PROP_TEST("LowMagShiftLimit", shiftManager->, LowMagShiftLimit)
INT_PROP_TEST("UseSquareShiftLimits", shiftManager->, UseSquareShiftLimits)
INT_PROP_TEST("FloatingCurrentMeterSmoothed", mWinApp->mScopeStatus., FloatingMeterSmoothed)
FLOAT_PROP_TEST("CurrentMeterLogBase", mWinApp->mScopeStatus., CurrentLogBase)
FLOAT_PROP_TEST("CurrentMeterSmootherThreshold1", mWinApp->mScopeStatus., CurrentSmootherThreshold1)
FLOAT_PROP_TEST("CurrentMeterSmootherThreshold2", mWinApp->mScopeStatus., CurrentSmootherThreshold2)
BOOL_PROP_TEST("SmallFontsBad", mWinApp->, SmallFontsBad)
BOOL_PROP_TEST("DisplayIsNot120DPI", mWinApp->, DisplayNotTruly120DPI)
INT_PROP_TEST("AssumeDPI", mWinApp->, SystemDPI)
FLOAT_PROP_TEST("CheckAutofocusChange", focusManager->, CheckDelta)
FLOAT_PROP_TEST("FocusPadFraction", focusManager->, PadFrac)
FLOAT_PROP_TEST("FocusTaperFraction", focusManager->, TaperFrac)
FLOAT_PROP_TEST("FocusFilterSigma1", focusManager->, Sigma1)
FLOAT_PROP_TEST("FocusFilterSigma2", focusManager->, Sigma2)
FLOAT_PROP_TEST("FocusFilterRadius2", focusManager->, Radius2)
FLOAT_PROP_TEST("FocusMaxPeakDistanceRatio", focusManager->, MaxPeakDistanceRatio)
INT_PROP_TEST("FocusPostTiltDelay", focusManager->, PostTiltDelay)
FLOAT_PROP_TEST("AlignTrimFraction", shiftManager->, TrimFrac)
FLOAT_PROP_TEST("AlignTaperFraction", shiftManager->, TaperFrac)
FLOAT_PROP_TEST("AlignFilterSigma1", shiftManager->, Sigma1)
FLOAT_PROP_TEST("AlignFilterSigma2", shiftManager->, Sigma2)
FLOAT_PROP_TEST("AlignFilterRadius2", shiftManager->, Radius2)
FLOAT_PROP_TEST("MaxCalibrationImageShift", mWinApp->mShiftCalibrator->, MaxCalShift)
FLOAT_PROP_TEST("MaxLMCalibrationImageShift", mWinApp->mShiftCalibrator->, MaxLMCalShift)
FLOAT_PROP_TEST("StageCycleLengthX", mWinApp->mShiftCalibrator->, StageCycleX)
FLOAT_PROP_TEST("StageCycleLengthY", mWinApp->mShiftCalibrator->, StageCycleY)
FLOAT_PROP_TEST("MaxStageCalExtent", mWinApp->mShiftCalibrator->, MaxStageCalExtent)
BOOL_PROP_TEST("UseTrialSizeForShiftCal", mWinApp->mShiftCalibrator->, UseTrialSize)
BOOL_PROP_TEST("UseTrialBinningForShiftCal", mWinApp->mShiftCalibrator->, UseTrialBinning)
FLOAT_PROP_TEST("StageCalibrationBacklash", mWinApp->mShiftCalibrator->, StageCalBacklash)
INT_PROP_TEST("StageCalToggleInterval", mWinApp->mShiftCalibrator->, CalToggleInterval)
INT_PROP_TEST("StageCalNumToggles", mWinApp->mShiftCalibrator->, NumCalToggles)
INT_PROP_TEST("DistortionStageDelay", mWinApp->mDistortionTasks->, StageDelay)
FLOAT_PROP_TEST("DistortionMaxMoveToCheck", mWinApp->mDistortionTasks->, MaxMoveToCheck)
FLOAT_PROP_TEST("ISoffsetCalStageLimit", mWinApp->mShiftCalibrator->, CalISOstageLimit)
FLOAT_PROP_TEST("SixPointIScalMinField", mWinApp->mShiftCalibrator->, MinFieldSixPointCal)
FLOAT_PROP_TEST("StageMontageBacklash", mWinApp->mMontageController->, StageBacklash)
BOOL_PROP_TEST("LimitMontageToUsable", mWinApp->mMontageController->, LimitMontToUsable)
BOOL_PROP_TEST("MontageRealignToNearest", mWinApp->mMontageController->, AlignToNearestPiece)
INT_PROP_TEST("MontageISRealignInterval", mWinApp->mMontageController->, ISrealignInterval)
BOOL_PROP_TEST("MontageUseTrialInISRealign", mWinApp->mMontageController->, UseTrialSetInISrealign)
DBL_PROP_TEST("MontageOverlapFraction", mWinApp->mDocWnd->, OverlapFraction)
FLOAT_PROP_TEST("MontMaxOverlapFraction", mWinApp->mDocWnd->, MaxOverlapFraction)
BOOL_PROP_TEST("ShootFilmIfMontageDark", mWinApp->mMontageController->, ShootFilmIfDark)
INT_PROP_TEST("MontageDuplicateRetries", mWinApp->mMontageController->, DuplicateRetryLimit)
INT_PROP_TEST("MontageDriftRepeatLimit", mWinApp->mMontageController->, DriftRepeatLimit)
FLOAT_PROP_TEST("MontageDriftRepeatDelay", mWinApp->mMontageController->, DriftRepeatDelay)
FLOAT_PROP_TEST("MontageMinZigzagOverlap", mWinApp->mMontageController->, MinOverlapForZigzag)
BOOL_PROP_TEST("MontageAutosaveLog", mWinApp->mMontageController->, AutosaveLog)
#endif
#ifdef PROP_TEST_SECT30
FLOAT_PROP_TEST("JEOLStageMotorRounding", scope->, JeolStageRounding)
FLOAT_PROP_TEST("JEOLStagePiezoRounding", scope->, JeolPiezoRounding)
FLOAT_PROP_TEST("MaximumTiltAngle", scope->, MaxTiltAngle)
FLOAT_PROP_TEST("TiltSpeedFactor", scope->, TiltSpeedFactor)
FLOAT_PROP_TEST("StageXYSpeedFactor", scope->, StageXYSpeedFactor)
INT_PROP_TEST("ScopeUpdateInterval", scope->, UpdateInterval)
INT_PROP_TEST("LowDoseBeamNormDelay", scope->, LDBeamNormDelay)
INT_PROP_TEST("PostProbeDelay", scope->, PostProbeDelay)
BOOL_PROP_TEST("CheckPosOnScreenError", scope->, CheckPosOnScreenError)
FLOAT_PROP_TEST("DiffractionShiftScaling", scope->, DiffShiftScaling)
BOOL_PROP_TEST("LowDoseBeamTiltShifts", scope->, LDBeamTiltShifts)
INT_PROP_TEST("NormCondenserInLowDose", scope->, UseNormForLDNormalize)
INT_PROP_TEST("JeolUpdateSleep", scope->, JeolUpdateSleep)
INT_PROP_TEST("InitializeJEOLDelay", scope->, InitializeJeolDelay)
BOOL_PROP_TEST("HasOmegaFilter", scope->, HasOmegaFilter)
BOOL_PROP_TEST("JeolUpdateByEvent", scope->, UpdateByEvent)
BOOL_PROP_TEST("UpdateSpectroscopyByEvent", scope->, SpectrumByEvent)
INT_PROP_TEST("UseJeolGIFmodeCalls", scope->, UseJeolGIFmodeCalls)
INT_PROP_TEST("JeolHasNitrogenClass", scope->, JeolHasNitrogenClass)
BOOL_PROP_TEST("JeolHasExtraApertures", scope->, JeolHasExtraApertures)
BOOL_PROP_TEST("RelaxLensesSequentially", scope->, SequentialLensRelax)
BOOL_PROP_TEST("JeolHasBrightnessZoom", scope->, JeolHasBrightnessZoom)
INT_PROP_TEST("JeolRelaxationFlags", scope->, JeolRelaxationFlags)
INT_PROP_TEST("JeolStartRelaxTimeout", scope->, JeolStartRelaxTimeout)
INT_PROP_TEST("JeolEndRelaxTimeout", scope->, JeolEndRelaxTimeout)
DBL_PROP_TEST("JEOLObjectiveLensToMicrons", scope->, Jeol_OLfine_to_um)
DBL_PROP_TEST("JEOLObjectiveMiniToMicrons", scope->, Jeol_OM_to_um)
DBL_PROP_TEST("JeolSTEMdefocusFactor", scope->, JeolSTEMdefocusFac)
INT_PROP_TEST("JeolSTEMrotation", scope->, JeolSTEMrotation)
DBL_PROP_TEST("JEOLBeamShiftToMicrons", scope->, Jeol_CLA1_to_um)
DBL_PROP_TEST("JEOLLowMagBeamShiftToMicrons", scope->, Jeol_LMCLA1_to_um)
DBL_PROP_TEST("JeolMMperUnitProjector", scope->, RoughPLscale)
BOOL_PROP_TEST("JEOLReportsSmallScreen", scope->, ReportsSmallScreen)
INT_PROP_TEST("JEOLReportsLargeScreen", scope->, ReportsLargeScreen)
INT_PROP_TEST("JeolMainDetectorID", scope->, MainDetectorID)
INT_PROP_TEST("JeolPairedDetectorID", scope->, PairedDetectorID)
INT_PROP_TEST("JeolHasNoAlpha", scope->, HasNoAlpha)
INT_PROP_TEST("JeolForceMDSmode", scope->, JeolForceMDSmode)
BOOL_PROP_TEST("JeolUseProjectorForIS", scope->, UsePLforIS)
BOOL_PROP_TEST("JeolUseCLA2ForSTEM", scope->, UseCLA2forSTEM)
BOOL_PROP_TEST("MessageBoxWhenClipIS", scope->, MessageWhenClipIS)
BOOL_PROP_TEST("LowDoseChangeAtZeroIS", scope->, ChangeAreaAtZeroIS)
BOOL_PROP_TEST("JEOL1230", scope->, Jeol1230)
INT_PROP_TEST("MagChangeFixISdelay", scope->, MagFixISdelay)
INT_PROP_TEST("MagChangeIntensityDelay", scope->, MagChgIntensityDelay)
INT_PROP_TEST("AlphaChangeDelay", scope->, AlphaChangeDelay)
INT_PROP_TEST("FocusChangeDelay", scope->, PostFocusChgDelay)
INT_PROP_TEST("JeolSTEMPreMagDelay", scope->, JeolSTEMPreMagDelay)
INT_PROP_TEST("JeolMagEventWait", scope->, JeolMagEventWait)
INT_PROP_TEST("PostJeolGIFdelay", scope->, PostJeolGIFdelay)
INT_PROP_TEST("JeolExternalMagDelay", scope->, JeolExternalMagDelay)
INT_PROP_TEST("InsertJeolDetectorDelay", scope->, InsertDetectorDelay)
INT_PROP_TEST("SelectJeolDetectorDelay", scope->, SelectDetectorDelay)
BOOL_PROP_TEST("JeolSTEMmagUnitsAreX", scope->, JeolSTEMunitsX)
BOOL_PROP_TEST("JeolReadStageForWait", scope->, JeolReadStageForWait)
INT_PROP_TEST("JeolFlashFegTimeout", scope->, JeolFlashFegTimeout)
INT_PROP_TEST("JeolEmissionTimeout", scope->, JeolEmissionTimeout)
INT_PROP_TEST("JeolBeamRampupTimeout", scope->, BeamRampupTimeout)
BOOL_PROP_TEST("SkipJeolNeutralCheck", scope->, SkipJeolNeutralCheck)
INT_PROP_TEST("SimulationMode", scope->, SimulationMode)
INT_PROP_TEST("RestoreStageXYdelay", scope->, RestoreStageXYdelay)
BOOL_PROP_TEST("UseInvertedMagRange", scope->, UseInvertedMagRange)
FLOAT_PROP_TEST("HitachiZMicronsPerGroove", mWinApp->mComplexTasks->, ZMicronsPerDialMark)
INT_PROP_TEST("HitachiSpotStepDelay", scope->, HitachiSpotStepDelay)
INT_PROP_TEST("HitachiDoesBSforISflags", scope->, HitachiDoesBSforIS)
BOOL_PROP_TEST("InvertStageXAxis", shiftManager->, InvertStageXAxis)
INT_PROP_TEST("StageInvertsZAxis", shiftManager->, StageInvertsZAxis)
INT_PROP_TEST("RotateHeaderAngleBy180", mWinApp->mBufferManager->, RotateAxisAngle)
BOOL_PROP_TEST("BackgroundSaveToFile", mWinApp->mBufferManager->, SaveAsynchronously)
FLOAT_PROP_TEST("HdfUpdateTimePerSection", mWinApp->mBufferManager->, HdfUpdateTimePerSect)
FLOAT_PROP_TEST("ResetRealignMinField", complexTasks->, MinRSRAField)
FLOAT_PROP_TEST("ReverseTiltMinField", complexTasks->, MinRTField)
FLOAT_PROP_TEST("EucentricityCoarseMinField", complexTasks->, MinFECoarseField)
FLOAT_PROP_TEST("EucentricityFineMinField", complexTasks->, MinFEFineField)
FLOAT_PROP_TEST("EucentricityFineAlignMinField", complexTasks->, MinFEFineAlignField)
FLOAT_PROP_TEST("WalkUpMinField", complexTasks->, MinWalkField)
FLOAT_PROP_TEST("TiltAfterMoveMinField", complexTasks->, MinTASMField)
FLOAT_PROP_TEST("BacklashAdjustMinField", complexTasks->, MinBASPField)
FLOAT_PROP_TEST("CookerRealignMinField", mWinApp->mMultiTSTasks->, MinCookField)
FLOAT_PROP_TEST("NavVisitReturnMinField", mWinApp->mMultiTSTasks->, MinNavVisitField)
#endif
#ifdef PROP_TEST_SECT35
FLOAT_PROP_TEST("MinLowMagSlitWidth", complexTasks->, MinLMSlitWidth)
FLOAT_PROP_TEST("SlitWideningSafetyFactor", complexTasks->, SlitSafetyFactor)
INT_PROP_TEST("ResetRealignMaxIterations", complexTasks->, MaxRSRAIterations)
FLOAT_PROP_TEST("ResetRealignIterationCriterion", complexTasks->, RSRACriterion)
FLOAT_PROP_TEST("ResetRealignHigherMagCriterion", complexTasks->, RSRAHigherMagCriterion)
FLOAT_PROP_TEST("WalkUpShiftLimit", complexTasks->, WalkShiftLimit)
FLOAT_PROP_TEST("WalkUpLowDoseISLimit", complexTasks->, WULowDoseISLimit)
FLOAT_PROP_TEST("EucentricityBacklashZ", complexTasks->, FEBacklashZ)
DBL_PROP_TEST("EucentricityCoarseInitialAngle", complexTasks->, FEInitialAngle)
DBL_PROP_TEST("EucentricityCoarseInitialIncrement", complexTasks->, FEInitialIncrement)
DBL_PROP_TEST("EucentricityResetISThreshold", complexTasks->, FEResetISThreshold)
DBL_PROP_TEST("EucentricityCoarseMaxTilt", complexTasks->, FEMaxTilt)
DBL_PROP_TEST("EucentricityCoarseMaxIncrement", complexTasks->, FEMaxIncrement)
FLOAT_PROP_TEST("EucentricityCoarseTargetShift", complexTasks->, FETargetShift)
FLOAT_PROP_TEST("EucentricityCoarseMaxIncrementChange", complexTasks->, FEMaxIncrementChange)
INT_PROP_TEST("EucentricityFineIterationLimit", complexTasks->, FEIterationLimit)
DBL_PROP_TEST("EucentricityMaxFineIS", complexTasks->, FEMaxFineIS)
INT_PROP_TEST("EucentricityRestoreStageXY", complexTasks->, EucenRestoreStageXY)
FLOAT_PROP_TEST("TiltBacklash", complexTasks->, TiltBacklash)
FLOAT_PROP_TEST("StageTimeoutFactor", complexTasks->, StageTimeoutFactor)
FLOAT_PROP_TEST("CookerISLimit", mWinApp->mMultiTSTasks->, CkISLimit)
FLOAT_PROP_TEST("MinTaskExposure", complexTasks->, MinTaskExposure)
BOOL_PROP_TEST("UseTrialSizeForTasks", complexTasks->, UseTrialSize)
FLOAT_PROP_TEST("EucentricityMaxFineAngle", complexTasks->, MaxFEFineAngle)
FLOAT_PROP_TEST("EucentricityFineIntervalAtZero", complexTasks->, MaxFEFineInterval)
FLOAT_PROP_TEST("EucentricityFineSubareaMeans", complexTasks->, FESizeOrFracForMean)
FLOAT_PROP_TEST("RealignItemMinMarginNeeded", navHelper->, MinMarginNeeded)
FLOAT_PROP_TEST("RealignItemMinMarginWanted", navHelper->, MinMarginWanted)
FLOAT_PROP_TEST("RealignItemMaxMarginNeeded", navHelper->, MaxMarginNeeded)
FLOAT_PROP_TEST("RealignItemMaxIS", navHelper->, RImaximumIS)
FLOAT_PROP_TEST("RealignItemMaxLMfield", navHelper->, RImaxLMfield)
FLOAT_PROP_TEST("RealignItemWeightCCCThresh", navHelper->, DistWeightThresh)
INT_PROP_TEST("RealignItemSkipCenTime", navHelper->, RIskipCenTimeCrit)
FLOAT_PROP_TEST("RealignItemSkipCenError", navHelper->, RIskipCenErrorCrit)
FLOAT_PROP_TEST("RealignItemWeightingSigma", navHelper->, RIweightSigma)
BOOL_PROP_TEST("RealignItemUseBeamOffsets", navHelper->, RIuseBeamOffsets)
FLOAT_PROP_TEST("RealignItemTiltTolerance", navHelper->, RITiltTolerance)
FLOAT_PROP_TEST("RealignItemFocusChangeLimit", navHelper->, RIdefocusChangeLimit)
BOOL_PROP_TEST("ConvertMapsToBytesDefault", navHelper->, ConvertMaps)
BOOL_PROP_TEST("SkipAstigAdjustmentForIS", navHelper->, SkipAstigAdjustment)
FLOAT_PROP_TEST("MaxMontReuseWaste", navHelper->, MaxMontReuseWaste)
FLOAT_PROP_TEST("MultiInHoleStartAngle", mWinApp->mParticleTasks->, MSinHoleStartAngle)
BOOL_PROP_TEST("AllowWindowWithTools", mWinApp->mExternalTools->, AllowWindow)
#endif
#ifdef PROP_TEST_SECT4
FLOAT_PROP_TEST("TSDefaultStartAngle", tsController->, DefaultStartAngle)
FLOAT_PROP_TEST("TSMaxUsableAngleDiff", tsController->, MaxUsableAngleDiff)
FLOAT_PROP_TEST("TSBadShotCrit", tsController->, BadShotCrit)
FLOAT_PROP_TEST("TSBadLowMagCrit", tsController->, BadLowMagCrit)
FLOAT_PROP_TEST("TSMaxTiltError", tsController->, MaxTiltError)
INT_PROP_TEST("TSMaxDelayAfterTilt", tsController->, MaxDelayAfterTilt)
FLOAT_PROP_TEST("TSLowMagFieldFrac", tsController->, LowMagFieldFrac)
FLOAT_PROP_TEST("TSStageMovedTolerance", tsController->, StageMovedTolerance)
FLOAT_PROP_TEST("TSUserFocusChangeTol", tsController->, UserFocusChangeTol)
FLOAT_PROP_TEST("TSFitDropErrorRatio", tsController->, FitDropErrorRatio)
FLOAT_PROP_TEST("TSFitDropBackoffRatio", tsController->, FitDropBackoffRatio)
INT_PROP_TEST("TSMaxImageFailures", tsController->, MaxImageFailures)
INT_PROP_TEST("TSMaxPositionFailures", tsController->, MaxPositionFailures)
INT_PROP_TEST("TSMaxDisturbValidChange", tsController->, MaxDisturbValidChange)
INT_PROP_TEST("TSMaxDropAsShiftDisturbed", tsController->, MaxDropAsShiftDisturbed)
INT_PROP_TEST("TSMaxDropAsFocusDisturbed", tsController->, MaxDropAsFocusDisturbed)
INT_PROP_TEST("TSMinFitXAfterDrop", tsController->, MinFitXAfterDrop)
INT_PROP_TEST("TSMinFitYAfterDrop", tsController->, MinFitYAfterDrop)
INT_PROP_TEST("TSMinFitZAfterDrop", tsController->, MinFitZAfterDrop)
BOOL_PROP_TEST("TSEarlyK2RecordReturn", tsController->, EarlyK2RecordReturn)
FLOAT_PROP_TEST("TSTrialCenterMaxRadFrac", tsController->, TrialCenterMaxRadFrac)
FLOAT_PROP_TEST("TSStepForBidirReturn", tsController->, StepForBidirReturn)
FLOAT_PROP_TEST("TSSpeedForOneStepReturn", tsController->, SpeedForOneStepReturn)
INT_PROP_TEST("TSRestoreStageXYonTilt", tsController->, RestoreStageXYonTilt)
INT_PROP_TEST("TSDosymFitPastReversals", tsController->, DosymFitPastReversals)
INT_PROP_TEST("TSDosymBacklashDir", tsController->, FixedDosymBacklashDir)
INT_PROP_TEST("TSCheckScopeDisturbances", tsController->, CheckScopeDisturbances)
INT_PROP_TEST("TSTrackAfterScopeStopTime", tsController->, TrackAfterScopeStopTime)
INT_PROP_TEST("TSFocusAfterScopeStopTime", tsController->, FocusAfterScopeStopTime)
INT_PROP_TEST("TSWaitAfterScopeFilling", tsController->, WaitAfterScopeFilling)
FLOAT_PROP_TEST("BeamCalMinField", beamAssessor->, CalMinField)
FLOAT_PROP_TEST("BeamCalExtraRangeNeeded", beamAssessor->, ExtraRangeAtMinMag)
INT_PROP_TEST("BeamCalMinCounts", beamAssessor->, MinCounts)
INT_PROP_TEST("BeamCalMaxCounts", beamAssessor->, MaxCounts)
FLOAT_PROP_TEST("BeamCalMinExposure", beamAssessor->, MinExposure)
FLOAT_PROP_TEST("BeamCalMaxExposure", beamAssessor->, MaxExposure)
DBL_PROP_TEST("BeamCalSpacingFactor", beamAssessor->, SpacingFactor)
BOOL_PROP_TEST("BeamCalUseTrialSettling", beamAssessor->, UseTrialSettling)
DBL_PROP_TEST("BeamCalInitialIncrement", beamAssessor->, InitialIncrement)
INT_PROP_TEST("BeamCalChangeDelay", beamAssessor->, PostSettingDelay)
BOOL_PROP_TEST("BeamCalFavorMagChanges", beamAssessor->, FavorMagChanges)
INT_PROP_TEST("FilterSlitInOutDelay", camera->, GIFslitInOutDelay)
FLOAT_PROP_TEST("FilterOffsetDelayCriterion", camera->, GIFoffsetDelayCrit)
FLOAT_PROP_TEST("FilterOffsetDelayBase1", camera->, GIFoffsetDelayBase1)
FLOAT_PROP_TEST("FilterOffsetDelaySlope1", camera->, GIFoffsetDelaySlope1)
FLOAT_PROP_TEST("FilterOffsetDelayBase2", camera->, GIFoffsetDelayBase2)
FLOAT_PROP_TEST("FilterOffsetDelaySlope2", camera->, GIFoffsetDelaySlope2)
BOOL_PROP_TEST("NoSpectrumOffset", camera->, NoSpectrumOffset)
BOOL_PROP_TEST("NoFilterControl", camera->, NoFilterControl)
FLOAT_PROP_TEST("RefineZLPStepSize", mWinApp->mFilterTasks->, RZlpStepSize)
FLOAT_PROP_TEST("RefineZLPSlitWidth", mWinApp->mFilterTasks->, RZlpSlitWidth)
FLOAT_PROP_TEST("RefineZLPMinimumExposure", mWinApp->mFilterTasks->, RZlpMinExposure)
FLOAT_PROP_TEST("RefineZLPMaxMeanRatio", mWinApp->mFilterTasks->, RZlpMeanCrit)
BOOL_PROP_TEST("RefineZLPRedoInLowDose", mWinApp->mFilterTasks->, RZlpRedoInLowDose)
BOOL_PROP_TEST("RefineZLPLeaveCDSMode", mWinApp->mFilterTasks->, RZlpLeaveCDSmode)
FLOAT_PROP_TEST("EnergyShiftCalMinField", mWinApp->mFilterTasks->, ShiftCalMinField)
FLOAT_PROP_TEST("GridLinesPerMM", mWinApp->mProcessImage->, GridLinesPerMM)
FLOAT_PROP_TEST("TestCtfPixelSize", mWinApp->mProcessImage->, TestCtfPixelSize)
FLOAT_PROP_TEST("DefaultMaxCtfFitRes", mWinApp->mProcessImage->, DefaultMaxCtfFitRes)
FLOAT_PROP_TEST("FindBeamOutsideFrac", mWinApp->mProcessImage->, FindBeamOutsideFrac)
FLOAT_PROP_TEST("ThicknessCoefficient", mWinApp->mProcessImage->, ThicknessCoefficient)
FLOAT_PROP_TEST("TestCtfTuningDefocus", mWinApp->mAutoTuning->, TestCtfTuningDefocus)
INT_PROP_TEST("CtfBasedLDareaDelay", mWinApp->mAutoTuning->, CtfBasedLDareaDelay)
INT_PROP_TEST("AstigBTBacklashDelay", mWinApp->mAutoTuning->, BacklashDelay)
INT_PROP_TEST("AdjustForISSkipBacklash", scope->, AdjustForISSkipBacklash)
FLOAT_PROP_TEST("AddToRawIntensity", scope->, AddToRawIntensity)
#endif
