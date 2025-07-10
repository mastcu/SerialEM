/*
 * StandardCalls.h
 *
 * This file is meant to be included after defining GET_ONE_INT, etc macros
 * and the LINE_END value if it is not ; .  They are all undefined at the end.
 * Define MATCHING_NAMES_ONLY to exclude ones whose internal and plugin name do not match
 * Define a SCOPE_SAMENAME macro to expand other statements from a typedef and function
 * name
 * Standard FEI calls go in the top section and will be expanded in various places in the 
 * plugin.  JEOL and other calls go in the lower section
 * See FeiScope PlugSocket.cpp for a checklist on adding a new function for FEI
 */
#ifndef FEISCOPE_PLUGIN_VERSION
#define FEISCOPE_PLUGIN_VERSION 116
#define SCALE_POWER_OFFSET  12
#endif

#ifndef LINE_END
#define LINE_END ;
#endif
#ifndef UTAPI_CALLS_ONLY
SET_ONE_BOOL(SetManualExposureMode) LINE_END
SET_ONE_DBL(SetManualExposureTime) LINE_END
CALL_NO_ARGS(TakeExposure) LINE_END
CALL_NO_ARGS(ForceRefill) LINE_END
CALL_NO_ARGS(LoaderBufferCycle) LINE_END
SET_ONE_BOOL(SetScreenDim) LINE_END
GET_ONE_INT(GetFilmStock) LINE_END
SET_ONE_INT(SkipAdvancedScripting) LINE_END
GET_ONE_INT(ASIstopContinuousRecord) LINE_END
#endif
#ifndef NON_UTAPI_ONLY
#ifndef UTGET_ONE_INT
#define UTGET_ONE_INT(a, b) GET_ONE_INT(a)
#define UTGET_ONE_BOOL(a, b) GET_ONE_BOOL(a)
#define UTGET_ONE_DBL(a, b) GET_ONE_DBL(a)
#define UTGET_TWO_DBL(a, b) GET_TWO_DBL(a)
#define UTSET_ONE_INT(a, b) SET_ONE_INT(a)
#define UTSET_ONE_BOOL(a, b) SET_ONE_BOOL(a)
#define UTGET_ONE_BOOL(a, b) GET_ONE_BOOL(a)
#define UTSET_ONE_DBL(a, b) SET_ONE_DBL(a)
#define UTSET_TWO_DBL(a, b) SET_TWO_DBL(a)
#define UTCALL_NO_ARGS(a, b) CALL_NO_ARGS(a)
#endif
UTGET_TWO_DBL(GetBeamShift, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetBeamShift, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetImageShift, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetImageShift, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetBeamTilt, UTSUP_DEFL_ALIGN) LINE_END
UTSET_TWO_DBL(SetBeamTilt, UTSUP_DEFL_ALIGN) LINE_END
UTGET_TWO_DBL(GetImageBeamTilt, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetImageBeamTilt, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetDarkFieldTilt, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetDarkFieldTilt, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetDiffractionShift, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetDiffractionShift, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetXLensTilt, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetXLensTilt, UTSUP_DEFLECTORS1) LINE_END
UTGET_TWO_DBL(GetXLensShift, UTSUP_DEFLECTORS1) LINE_END
UTSET_TWO_DBL(SetXLensShift, UTSUP_DEFLECTORS1) LINE_END
UTGET_ONE_DBL(GetXLensFocus, UTSUP_FOCUS) LINE_END
UTSET_ONE_DBL(SetXLensFocus, UTSUP_FOCUS) LINE_END
UTSET_ONE_INT(NormalizeLens, UTSUP_NORMALIZE) LINE_END
UTGET_ONE_BOOL(GetBeamBlank, UTSUP_BLANKER) LINE_END
UTSET_ONE_BOOL(SetBeamBlank, UTSUP_BLANKER) LINE_END
UTGET_ONE_DBL(GetHighVoltage, UTSUP_HIGH_TENSION) LINE_END
UTGET_ONE_INT(GetGunValve, UTSUP_COL_VALVES) LINE_END
UTSET_ONE_BOOL(SetGunValve, UTSUP_COL_VALVES) LINE_END
UTGET_ONE_BOOL(GetPVPRunning, UTSUP_VACUUM) LINE_END
UTCALL_NO_ARGS(RunBufferCycle, UTSUP_VACUUM) LINE_END
UTGET_ONE_DBL(GetScreenCurrent, UTSUP_FLUSCREEN) LINE_END
UTGET_ONE_INT(GetMainScreen, UTSUP_FLUSCREEN) LINE_END
UTSET_ONE_INT(SetMainScreen, UTSUP_FLUSCREEN) LINE_END
UTGET_ONE_DBL(GetAbsFocus, UTSUP_FOCUS) LINE_END
UTSET_ONE_DBL(SetAbsFocus, UTSUP_FOCUS) LINE_END
UTCALL_NO_ARGS(ResetDefocus, UTSUP_FOCUS) LINE_END
UTGET_ONE_DBL(GetDefocus, UTSUP_FOCUS) LINE_END
UTSET_ONE_DBL(SetDefocus, UTSUP_FOCUS) LINE_END
UTGET_ONE_DBL(GetTiltAngle, UTSUP_STAGE) LINE_END
UTGET_ONE_INT(GetStageStatus, UTSUP_STAGE) LINE_END
UTGET_ONE_DBL(GetStageBAxis, UTSUP_STAGE) LINE_END
UTGET_ONE_INT(GetSTEMMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_INT(SetSTEMMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetProbeMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_INT(SetProbeMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_BOOL(GetIntensityZoom, UTSUP_COL_MODE) LINE_END
UTSET_ONE_BOOL(SetIntensityZoom, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetEFTEMMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_INT(SetEFTEMMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_INT(SetImagingMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetSubMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetDarkFieldMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_INT(SetDarkFieldMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetXLensModeAvailable, UTSUP_COL_MODE) LINE_END
UTGET_ONE_BOOL(GetXLensMode, UTSUP_COL_MODE) LINE_END
UTSET_ONE_BOOL(SetXLensMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_INT(GetSpotSize, UTSUP_ILLUMINATION) LINE_END
UTSET_ONE_INT(SetSpotSize, UTSUP_ILLUMINATION) LINE_END
UTGET_ONE_DBL(GetIntensity, UTSUP_ILLUMINATION) LINE_END
UTSET_ONE_DBL(SetIntensity, UTSUP_ILLUMINATION) LINE_END
UTGET_ONE_DBL(GetIlluminatedArea, UTSUP_ILLUMINATION) LINE_END
UTSET_ONE_DBL(SetIlluminatedArea, UTSUP_ILLUMINATION) LINE_END
UTGET_ONE_DBL(GetImageDistanceOffset, UTSUP_ILLUMINATION) LINE_END
UTSET_ONE_DBL(SetImageDistanceOffset, UTSUP_ILLUMINATION) LINE_END
UTGET_TWO_DBL(GetObjectiveStigmator, UTSUP_STIGMATOR) LINE_END
UTSET_TWO_DBL(SetObjectiveStigmator, UTSUP_STIGMATOR) LINE_END
UTGET_TWO_DBL(GetCondenserStigmator, UTSUP_STIGMATOR) LINE_END
UTSET_TWO_DBL(SetCondenserStigmator, UTSUP_STIGMATOR) LINE_END
UTGET_TWO_DBL(GetDiffractionStigmator, UTSUP_STIGMATOR) LINE_END
UTSET_TWO_DBL(SetDiffractionStigmator, UTSUP_STIGMATOR) LINE_END
UTGET_TWO_DBL(GetXLensStigmator, UTSUP_STIGMATOR) LINE_END
UTSET_TWO_DBL(SetXLensStigmator, UTSUP_STIGMATOR) LINE_END
UTGET_ONE_DBL(GetFEGBeamCurrent, UTSUP_FEG) LINE_END
UTSET_ONE_BOOL(SetAutoNormEnabled, UTSUP_NORMALIZE) LINE_END
UTGET_ONE_INT(GetMagnificationIndex, UTSUP_MAGNIFICATION) LINE_END
UTGET_ONE_DBL(GetSTEMMagnification, UTSUP_MAGNIFICATION) LINE_END
UTSET_ONE_DBL(SetSTEMMagnification, UTSUP_MAGNIFICATION) LINE_END
UTSET_ONE_INT(SetMagnificationIndex, UTSUP_MAGNIFICATION) LINE_END
UTGET_ONE_DBL(GetMagnification, UTSUP_MAGNIFICATION) LINE_END
UTGET_ONE_DBL(GetCameraLength, UTSUP_MAGNIFICATION) LINE_END
UTGET_ONE_INT(GetCameraLengthIndex, UTSUP_MAGNIFICATION) LINE_END
UTSET_ONE_INT(SetCameraLengthIndex, UTSUP_MAGNIFICATION) LINE_END
UTGET_ONE_DBL(GetImageRotation, UTSUP_MAGNIFICATION) LINE_END
UTSET_ONE_INT(SetObjectiveMode, UTSUP_COL_MODE) LINE_END
UTGET_ONE_DBL(GetZeroLossPeakShift, UTSUP_FILTER) LINE_END
UTSET_ONE_DBL(SetEnergyShift, UTSUP_FILTER) LINE_END
UTSET_ONE_DBL(SetSlitWidth, UTSUP_FILTER) LINE_END
UTSET_ONE_INT(SetSlitIn, UTSUP_FILTER) LINE_END
UTSET_ONE_INT(LoadCartridge, UTSUP_LOADER) LINE_END
UTCALL_NO_ARGS(UnloadCartridge, UTSUP_LOADER) LINE_END
UTCALL_NO_ARGS(DoCassetteInventory, UTSUP_LOADER) LINE_END
UTGET_ONE_INT(NumberOfLoaderSlots, UTSUP_LOADER) LINE_END
UTGET_ONE_DBL(GetObjectiveStrength, UTSUP_ILLUMINATION) LINE_END
UTGET_ONE_BOOL(TempControlAvailable, UTSUP_TEMP_CONTROL) LINE_END
UTGET_ONE_BOOL(DewarsAreBusyFilling, UTSUP_TEMP_CONTROL) LINE_END
UTGET_ONE_INT(DewarsRemainingTime, UTSUP_TEMP_CONTROL) LINE_END
UTGET_ONE_INT(GetBeamStopper, UTSUP_BEAM_STOP) LINE_END
#ifndef  MATCHING_NAMES_ONLY
UTGET_ONE_BOOL(GetSmallScreenOut, UTSUP_FLUSCREEN) LINE_END
#endif
#undef UTGET_ONE_INT
#undef UTGET_ONE_BOOL
#undef UTGET_ONE_DBL
#undef UTGET_TWO_DBL
#undef UTSET_ONE_INT
#undef UTSET_ONE_BOOL
#undef UTSET_ONE_DBL
#undef UTSET_TWO_DBL
#undef UTCALL_NO_ARGS
#endif

#undef GET_ONE_INT
#undef GET_ONE_BOOL
#undef GET_ONE_DBL
#undef GET_TWO_DBL
#undef SET_ONE_INT
#undef SET_ONE_BOOL
#undef SET_ONE_DBL
#undef SET_TWO_DBL
#undef CALL_NO_ARGS
#undef LINE_END

#ifdef SCOPE_SAMENAME
SCOPE_SAMENAME(ScopeGetThreeDbl, GetStagePosition);
SCOPE_SAMENAME(ScopeSetStage, SetStagePosition);
SCOPE_SAMENAME(ScopeSetStageExtra, SetStagePositionExtra);
SCOPE_SAMENAME(ScopeGetTwoDbl, GetPiezoXYPosition);
SCOPE_SAMENAME(ScopeSetTwoDbl, SetPiezoXYPosition);
SCOPE_SAMENAME(ScopeGetGauge, GetGaugePressure);
SCOPE_SAMENAME(ScopeGetDblSetInt, GetRefrigerantLevel);
SCOPE_SAMENAME(ScopeGetSetInt, LoaderSlotStatus);
SCOPE_SAMENAME(ScopeGetInt, GetLoadedSlot);
SCOPE_SAMENAME(ScopeNoArg, InitializeScope);
SCOPE_SAMENAME(ScopeNoArg, UninitializeScope);
SCOPE_SAMENAME(ScopeSetInt, DoingUpdate);
SCOPE_SAMENAME(ScopeSetInt, GetValuesFast);
SCOPE_SAMENAME(GetErrStr, GetLastErrorString);
SCOPE_SAMENAME(ScopeGetBool, ScopeIsDisconnected);
SCOPE_SAMENAME(ScopeGetTwoDblByName, GetDeflectorByName);
SCOPE_SAMENAME(ScopeSet2DblIntByName, SetDeflectorByName);
SCOPE_SAMENAME(ScopeGetDblByName, GetLensByName);
SCOPE_SAMENAME(ScopeSetDblByName, SetLensByName);
SCOPE_SAMENAME(ScopeSetInt, SetObjFocus);
SCOPE_SAMENAME(ScopeSetDbl, SetDiffractionFocus);
SCOPE_SAMENAME(ScopeSetInt, SetFineFocus);
SCOPE_SAMENAME(ScopeSetInt, SetCoarseFocus);
SCOPE_SAMENAME(ScopeGetSetInt, CheckMagSelector);
SCOPE_SAMENAME(ScopeGetInt, GetImagingMode);
SCOPE_SAMENAME(ScopeGetInt, GetMDSMode);
SCOPE_SAMENAME(ScopeSetInt, SetMDSMode);
SCOPE_SAMENAME(ScopeGetInt, GetSpectroscopyMode);
SCOPE_SAMENAME(ScopeSetInt, SetSpectroscopyMode);
SCOPE_SAMENAME(ScopeSetThreeInt, TestSTEMShift);
SCOPE_SAMENAME(ScopeGetSetInt, GetDetectorPosition);
SCOPE_SAMENAME(ScopeSetTwoInt, SetDetectorPosition);
SCOPE_SAMENAME(ScopeSetTwoInt, SetDetectorSelected);
SCOPE_SAMENAME(ScopeSetInt, SetToNeutral);
SCOPE_SAMENAME(ScopeSetInt, UseMagInNextSetISXY);
SCOPE_SAMENAME(ScopeGetInt, GetAlpha);
SCOPE_SAMENAME(ScopeSetInt, SetAlpha);
SCOPE_SAMENAME(ScopeSetInt, EnableEnergyShift);
SCOPE_SAMENAME(ScopeGetIntTwoDbl, GetFilterState);
SCOPE_SAMENAME(ScopeSetInt, StartUpdates);
SCOPE_SAMENAME(ScopeGetThreeDbl, GetFilterRanges);
SCOPE_SAMENAME(ScopeSetTwoInt, SetFreeLensControl);
SCOPE_SAMENAME(ScopeSetTwoIntDbl, SetLensWithFLC);
SCOPE_SAMENAME(ScopeSetIntGetIntDbl, GetLensFLCStatus);
SCOPE_SAMENAME(ScopeGetDbl, GetFilamentCurrent);
SCOPE_SAMENAME(ScopeSetDbl, SetFilamentCurrent);
SCOPE_SAMENAME(ScopeGetInt, GetEmissionState);
SCOPE_SAMENAME(CamOneInt, SetEmissionState);
SCOPE_SAMENAME(CamOneInt, FlashFEG);
SCOPE_SAMENAME(ScopeGetSetInt, GetFlashingAdvised);
SCOPE_SAMENAME(CamOneInt, GetNitrogenStatus);
SCOPE_SAMENAME(CamTwoInt, RefillNitrogen);
SCOPE_SAMENAME(ScopeSetIntGetIntDbl, GetNitrogenInfo);
SCOPE_SAMENAME(CartridgeInfo, GetCartridgeInfo);
SCOPE_SAMENAME(CamThreeInt, MoveCartridge);
SCOPE_SAMENAME(CamOneInt, GetApertureSize);
SCOPE_SAMENAME(ScopeSetTwoInt, SetApertureSize);
SCOPE_SAMENAME(ScopeSetIntGetTwoDbl, GetAperturePosition);
SCOPE_SAMENAME(ScopeSetIntTwoDbl, SetAperturePosition);
SCOPE_SAMENAME(ScopeGetSetInt, GoToNextPhasePlatePos);
SCOPE_SAMENAME(ScopeGetInt, GetCurPhasePlatePos);
SCOPE_SAMENAME(CamGetTwoInt, GetNumStartupErrors);
SCOPE_SAMENAME(CamTwoInt, BeginThreadAccess);
SCOPE_SAMENAME(ScopeSetInt, EndThreadAccess);
SCOPE_SAMENAME(GetChannelList, GetFEIChannelList);
SCOPE_SAMENAME(GetBrightContrast, GetDetectorBrightContrast);
SCOPE_SAMENAME(SetBrightContrast, SetDetectorBrightContrast);
SCOPE_SAMENAME(LookupCamera, LookupScriptingCamera);
SCOPE_SAMENAME(FEIimage, AcquireFEIimage);
SCOPE_SAMENAME(FEIchannels, AcquireFEIchannels);
SCOPE_SAMENAME(FIFinit, FIFinitialize);
SCOPE_SAMENAME(FIFsetup, FIFsetupConfigFile);
SCOPE_SAMENAME(FIFbuildMap, FIFbuildFileMap);
SCOPE_SAMENAME(FIFgetNext, FIFgetNextFrame);
SCOPE_SAMENAME(FIFclean, FIFcleanUp);
SCOPE_SAMENAME(FIFclean2, FIFcleanUp2);
SCOPE_SAMENAME(ASIsetup, ASIsetupFrames);
SCOPE_SAMENAME(FindRef, FindNewestMatchingFile);
SCOPE_SAMENAME(GetFile, ReadAndReturnFile);
SCOPE_SAMENAME(ASIimage, ASIacquireFromCamera);
SCOPE_SAMENAME(GetErrStr, ASIgetLastStoragePath);
SCOPE_SAMENAME(ScopeGetInt, ASIgetVersion);
SCOPE_SAMENAME(ScopeSetInt, ASIsetVersion);
SCOPE_SAMENAME(CamOneInt, ASIisCameraInserted);
SCOPE_SAMENAME(CamTwoInt, ASIsetCameraInsertion);
SCOPE_SAMENAME(FIFconfInit, FIFinitFalconConfig);
SCOPE_SAMENAME(FIFclean2, FIFcheckFalconConfig);
SCOPE_SAMENAME(CamNoArg, FocusRamperInitialize);
SCOPE_SAMENAME(DoFocRamp, DoFocusRamp);
SCOPE_SAMENAME(FinishFocRamp, FinishFocusRamp);
SCOPE_SAMENAME(ShowMB, ShowMessageBox);
SCOPE_SAMENAME(ScopeSetInt, SetBeamStopper);
SCOPE_SAMENAME(ScopeSetDbl, SetHighVoltage);
SCOPE_SAMENAME(UtapiSTEMimage, UtapiAcquireSTEMimage);
SCOPE_SAMENAME(ScopeNoArg, UtapiStopContinuous);
#undef SCOPE_SAMENAME
#endif                                 
