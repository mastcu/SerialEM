/*
 * StandardCalls.h
 *
 * This file is meant to be included after defining GET_ONE_INT, etc macros
 * and the LINE_END value if it is not ; .  They are all undefined at the end.
 * Define MATCHING_NAMES_ONLY to exclude ones whose internal and plugin name do not match
 * Define a SCOPE_SAMENAME macro to expand other statements from a typedef and function
 * name
 */
#ifndef FEISCOPE_PLUGIN_VERSION
#define FEISCOPE_PLUGIN_VERSION 105
#endif

#ifndef LINE_END
#define LINE_END ;
#endif
#ifndef  MATCHING_NAMES_ONLY
GET_ONE_BOOL(GetSmallScreenOut) LINE_END
#endif
GET_ONE_DBL(GetSTEMMagnification) LINE_END
SET_ONE_DBL(SetSTEMMagnification) LINE_END
GET_ONE_INT(GetSTEMMode) LINE_END
SET_ONE_INT(SetSTEMMode) LINE_END
GET_ONE_INT(GetProbeMode) LINE_END
SET_ONE_INT(SetProbeMode) LINE_END
SET_ONE_INT(NormalizeLens) LINE_END
GET_ONE_INT(GetSpotSize) LINE_END
SET_ONE_INT(SetSpotSize) LINE_END
GET_ONE_BOOL(GetIntensityZoom) LINE_END
SET_ONE_BOOL(SetIntensityZoom) LINE_END
GET_ONE_BOOL(GetBeamBlank) LINE_END
SET_ONE_BOOL(SetBeamBlank) LINE_END
GET_ONE_DBL(GetAbsFocus) LINE_END
SET_ONE_DBL(SetAbsFocus) LINE_END
GET_ONE_DBL(GetObjectiveStrength) LINE_END
GET_ONE_INT(GetEFTEMMode) LINE_END
SET_ONE_INT(SetEFTEMMode) LINE_END
SET_ONE_BOOL(SetManualExposureMode) LINE_END
GET_ONE_DBL(GetHighVoltage) LINE_END
GET_ONE_INT(GetGunValve) LINE_END
SET_ONE_BOOL(SetGunValve) LINE_END
GET_TWO_DBL(GetBeamShift) LINE_END
SET_TWO_DBL(SetBeamShift) LINE_END
GET_TWO_DBL(GetImageShift) LINE_END
SET_TWO_DBL(SetImageShift) LINE_END
GET_TWO_DBL(GetBeamTilt) LINE_END
SET_TWO_DBL(SetBeamTilt) LINE_END
GET_ONE_DBL(GetIntensity) LINE_END
SET_ONE_DBL(SetIntensity) LINE_END
GET_ONE_DBL(GetCameraLength) LINE_END
GET_ONE_INT(GetCameraLengthIndex) LINE_END
SET_ONE_INT(SetCameraLengthIndex) LINE_END
GET_ONE_DBL(GetDefocus) LINE_END
SET_ONE_DBL(SetDefocus) LINE_END
GET_ONE_DBL(GetMagnification) LINE_END
GET_ONE_INT(GetMagnificationIndex) LINE_END
SET_ONE_INT(SetMagnificationIndex) LINE_END
SET_ONE_INT(SetImagingMode) LINE_END
GET_ONE_DBL(GetImageRotation) LINE_END
CALL_NO_ARGS(ResetDefocus) LINE_END
GET_ONE_DBL(GetScreenCurrent) LINE_END
GET_ONE_INT(GetMainScreen) LINE_END
SET_ONE_INT(SetMainScreen) LINE_END
SET_ONE_DBL(SetManualExposureTime) LINE_END
CALL_NO_ARGS(TakeExposure) LINE_END
GET_ONE_INT(GetStageStatus) LINE_END
GET_ONE_DBL(GetTiltAngle) LINE_END
GET_ONE_DBL(GetIlluminatedArea) LINE_END
SET_ONE_DBL(SetIlluminatedArea) LINE_END
GET_TWO_DBL(GetObjectiveStigmator) LINE_END
SET_TWO_DBL(SetObjectiveStigmator) LINE_END
SET_ONE_BOOL(SetAutoNormEnabled) LINE_END
GET_ONE_INT(GetSubMode) LINE_END
GET_ONE_INT(GetDarkFieldMode) LINE_END
SET_ONE_INT(SetDarkFieldMode) LINE_END
GET_TWO_DBL(GetDarkFieldTilt) LINE_END
SET_TWO_DBL(SetDarkFieldTilt) LINE_END
GET_ONE_BOOL(TempControlAvailable) LINE_END
GET_ONE_BOOL(DewarsAreBusyFilling) LINE_END
GET_ONE_INT(DewarsRemainingTime) LINE_END
SET_ONE_INT(LoadCartridge) LINE_END
CALL_NO_ARGS(UnloadCartridge) LINE_END
GET_ONE_BOOL(GetPVPRunning) LINE_END
CALL_NO_ARGS(RunBufferCycle) LINE_END
CALL_NO_ARGS(ForceRefill) LINE_END
CALL_NO_ARGS(DoCassetteInventory) LINE_END
CALL_NO_ARGS(LoaderBufferCycle) LINE_END
GET_ONE_INT(NumberOfLoaderSlots) LINE_END
SET_ONE_BOOL(SetScreenDim) LINE_END
GET_ONE_INT(GetFilmStock) LINE_END

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
SCOPE_SAMENAME(ScopeGetGauge, GetGaugePressure);
SCOPE_SAMENAME(ScopeGetDblSetInt, GetRefrigerantLevel);
SCOPE_SAMENAME(ScopeGetSetInt, LoaderSlotStatus);
SCOPE_SAMENAME(ScopeNoArg, InitializeScope);
SCOPE_SAMENAME(ScopeNoArg, UninitializeScope);
SCOPE_SAMENAME(ScopeSetInt, DoingUpdate);
SCOPE_SAMENAME(ScopeSetInt, GetValuesFast);
SCOPE_SAMENAME(GetErrStr, GetLastErrorString);
SCOPE_SAMENAME(ScopeGetBool, ScopeIsDisconnected);
SCOPE_SAMENAME(ScopeGetTwoDblByName, GetDeflectorByName);
SCOPE_SAMENAME(ScopeGetDblByName, GetLensByName);
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
SCOPE_SAMENAME(ScopeSetDbl, SetEnergyShift);
SCOPE_SAMENAME(ScopeSetDbl, SetSlitWidth);
SCOPE_SAMENAME(ScopeSetInt, SetSlitIn);
SCOPE_SAMENAME(ScopeSetInt, EnableEnergyShift);
SCOPE_SAMENAME(ScopeSetInt, StartUpdates);
SCOPE_SAMENAME(ScopeGetThreeDbl, GetFilterRanges);
SCOPE_SAMENAME(ScopeSetTwoInt, SetFreeLensControl);
SCOPE_SAMENAME(ScopeSetTwoIntDbl, SetLensWithFLC);
SCOPE_SAMENAME(CamGetTwoInt, GetNumStartupErrors);
SCOPE_SAMENAME(CamTwoInt, BeginThreadAccess);
SCOPE_SAMENAME(ScopeSetInt, EndThreadAccess);
SCOPE_SAMENAME(GetChannelList, GetFEIChannelList);
SCOPE_SAMENAME(LookupCamera, LookupScriptingCamera);
SCOPE_SAMENAME(FEIimage, AcquireFEIimage);
SCOPE_SAMENAME(FEIchannels, AcquireFEIchannels);
SCOPE_SAMENAME(FIFinit, FIFinitialize);
SCOPE_SAMENAME(FIFsetup, FIFsetupConfigFile);
SCOPE_SAMENAME(FIFbuildMap, FIFbuildFileMap);
SCOPE_SAMENAME(FIFgetNext, FIFgetNextFrame);
SCOPE_SAMENAME(FIFclean, FIFcleanUp);
SCOPE_SAMENAME(FIFclean2, FIFcleanUp2);
SCOPE_SAMENAME(FIFconfInit, FIFinitFalconConfig);
SCOPE_SAMENAME(FIFclean2, FIFcheckFalconConfig);
SCOPE_SAMENAME(CamNoArg, FocusRamperInitialize);
SCOPE_SAMENAME(DoFocRamp, DoFocusRamp);
SCOPE_SAMENAME(FinishFocRamp, FinishFocusRamp);
SCOPE_SAMENAME(ShowMB, ShowMessageBox);
#undef SCOPE_SAMENAME
#endif                                 
