// MacroCommands.cpp:    Subclass for executing the next command
//
// Copyright (C) 2003-2020 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <direct.h>
#include <process.h>
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\MacroProcessor.h"
#include "FocusManager.h"
#include "AutoTuning.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "EMmontageController.h"
#include "LogWindow.h"
#include "EMscope.h"
#include "CameraController.h"
#include "GainRefMaker.h"
#include "CalibCameraTiming.h"
#include "TSController.h"
#include "EMbufferManager.h"
#include "ParameterIO.h"
#include "BeamAssessor.h"
#include "ProcessImage.h"
#include "ComplexTasks.h"
#include "MultiTSTasks.h"
#include "HoleFinderDlg.h"
#include "MultiHoleCombiner.h"
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "MacroControlDlg.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "NavRotAlignDlg.h"
#include "StateDlg.h"
#include "FalconHelper.h"
#include "ExternalTools.h"
#include "ScreenShotDialog.h"
#include "Mailer.h"
#include "PiezoAndPPControl.h"
#include "Utilities\XCorr.h"
#include "Utilities\KGetOne.h"
#include "Shared\autodoc.h"
#include "Shared\b3dutil.h"
#include "Shared\ctffind.h"
#include "Image\KStoreADOC.h"
#include "XFolderDialog\XFolderDialog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define CMD_IS(a) (cCmdIndex == CME_##a)

#define ABORT_NOLINE(a) \
{ \
  mWinApp->AppendToLog((a), mLogErrAction);  \
  SEMMessageBox((a), MB_EXCLAME); \
  AbortMacro(); \
  return; \
}

#define ABORT_LINE(a) \
{ \
  mWinApp->AppendToLog((a) + cStrLine, mLogErrAction);  \
  SEMMessageBox((a) + cStrLine, MB_EXCLAME); \
  AbortMacro(); \
  return; \
}

#define ABORT_NONAV \
{ if (!mWinApp->mNavigator) \
  { \
    CString macStr = CString("The Navigator must be open to execute:\r\n") + cStrLine; \
    mWinApp->AppendToLog(macStr, mLogErrAction);  \
    SEMMessageBox(macStr, MB_EXCLAME); \
    AbortMacro(); \
    return; \
  } \
}

#define SUSPEND_NOLINE(a) \
{ \
  mCurrentIndex = mLastIndex; \
  CString macStr = CString("Script suspended ") + (a);  \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  SuspendMacro(); \
  return; \
}

#define SUSPEND_LINE(a) \
{ \
  mCurrentIndex = mLastIndex; \
  CString macStr = CString("Script suspended ") + (a) + cStrLine;  \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  SuspendMacro(); \
  return; \
}

// The two numbers are the minimum arguments and whether arithmetic is allowed
// Arithmetic is allowed for any command with bit 1 set in the second number (1), and for 
// any "Set" command that does not have bit 2 set there (2).  Starting with % lists all 
// explicitly allowed commands.  It is redundant to set bit 1 for Set command
// Bit 3 (4) signifies that looping in OnIdle is allowed for a command, it is redundant
// to set this for a Report command
static CmdItem cmdList[] = {{"ScriptEnd", 0, 0}, {"Label", 0, 0}, {"SetVariable", 0, 0},
{"SetStringVar", 0, 0}, {"DoKeyBreak", 0, 0}, {"ZeroLoopElseIf", 0, 0}, {"NotFound", 0, 0},
  {NULL,0,0}, {NULL,0,0}, {NULL,0,0}, {NULL,0,0}, {NULL,0,0},
{"A",0,0}, {"AlignTo",1,0}, {"AutoAlign",0,0}, {"AutoFocus",0,0}, {"Break",0,4},
{"Call",1,4}, {"CallMacro",1,4},{"CenterBeamFromImage",0,0}, {"ChangeEnergyLoss",1,1},
{"ChangeFocus",1,1},{"ChangeIntensityBy",1,1}, {"ChangeMag",1,1},{"CircleFromPoints",6,1},
{"ClearAlignment",0,0}, {"CloseFile",0,4}, {"ConicalAlignTo",1,0}, {"Continue", 0, 4},
{"Copy",2,0}, {"D",0,0}, {"Delay",1,1}, {"DoMacro",1,4}, {"Echo", 0, 4}, {"Else", 0, 4},
{"Endif", 0, 4}, {"EndLoop", 0, 4}, {"Eucentricity", 0,0}, {"ExposeFilm",0,1}, {"F",0,0},
{"G",0,0}, {"GoToLowDoseArea",1,0}, {"If",3,4}, {"ImageShiftByMicrons", 2, 1},
{"ImageShiftByPixels",2,1}, {"ImageShiftByUnits",2,1}, {"IncPercentC2",1,1}, {"L", 0, 0},
{"Loop",1,5}, {"M",0,0}, {"MacroName",1,4}, {"LongName",1,4}, {"ManualFilmExposure", 1,1},
{"Montage",0,0}, {"MoveStage",2,1},{"MoveStageTo",2,1},{"NewMap",0,0},{"OpenNewFile",1,4},
{"OpenNewMontage",1,4}, {"Pause",0,4}, {"QuadrantMeans",0,4}, {"R",0,0}, {"ReadFile",0,0},
{"RealignToNavItem",1,0}, {"RealignToOtherItem",2,0}, {"RecordAndTiltDown",0,0},
{"RecordAndTiltUp", 0, 0}, {"RefineZLP", 0,0}, {"Repeat", 0,4}, {"ReportAccumShift", 0,0},
{"ReportAlignShift", 0, 0}, {"ReportBeamShift", 0, 0}, {"ReportClock", 0, 0},
{"ReportDefocus",0,0}, {"ReportFocus", 0,0}, {"ReportMag", 0,0}, {"ReportMagIndex", 0,0},
{"ReportMeanCounts", 0, 0}, {"ReportNavItem", 0, 0}, {"ReportOtherItem", 1, 0},
{"ReportPercentC2", 0, 0}, {"ReportShiftDiffFrom", 1, 0}, {"ReportSpotSize", 0, 0},
{"ReportStageXYZ", 0, 0}, {"ReportTiltAngle", 0, 0}, {"ResetAccumShift", 0, 0},
{"ResetClock", 0, 4}, {"ResetImageShift", 0, 0}, {"ResetShiftIfAbove", 1, 1},
{"RestoreCameraSet", 0, 4}, {"RetractCamera", 0, 0}, {"ReverseTilt", 0, 0}, {"S", 0, 0},
{"Save", 0, 0}, {"ScreenDown", 0, 0}, {"ScreenUp", 0, 0}, {"SetBeamBlank", 1, 0},
{"SetBeamShift", 2, 0}, {"SetBinning", 2, 4}, {"SetCamLenIndex", 1, 0},
{"SetColumnOrGunValve", 1, 0}, {"SetDefocus", 1, 0}, {"SetDirectory", 1, 6},
{"SetEnergyLoss", 1, 0}, {"SetExposure", 2, 0}, {"SetIntensityByLastTilt", 0, 2},
{"SetIntensityForMean", 0,0}, {"SetMag",1,0}, {"SetObjFocus",1,0}, {"SetPercentC2", 1,0},
{"SetSlitIn", 0, 1}, {"SetSlitWidth", 1, 0}, {"SetSpotSize", 1, 0}, {"Show", 1, 0},
{"SubareaMean", 4, 5}, {"SuppressReports", 0,4}, {"SwitchToFile", 1,4}, {"T", 0, 0},
{"TiltBy", 1, 1}, {"TiltDown", 0, 0}, {"TiltTo", 1, 1}, {"TiltUp", 0, 0},
{"U",0,0}, {"V",0,0}, {"WaitForDose",1,1}, {"WalkUpTo",1,1}, {"Return",0,4}, {"Exit",0,4},
{"MoveToNavItem", 0, 0}, {"SkipPiecesOutsideItem", 1, 4}, {"IncTargetDefocus", 1, 5},
{"ReportFileZsize", 0, 0}, {"SpecialExposeFilm", 2, 1}, {"SetMontageParams", 1, 4},
{"AutoCenterBeam", 0, 0}, {"CookSpecimen", 0, 0}, {"ImageProperties", 0, 4},
{"ItemForSuperCoord", 1, 4}, {"SetNewFileType", 1, 4}, {"ShiftItemsByAlignment", 0, 4},
{"ShiftImageForDrift", 3, 0}, {"ReportFocusDrift", 0, 0}, {"CalibrateImageShift", 0, 0},
{"SetMagAndIntensity", 1, 0}, {"ChangeMagAndIntensity", 1, 1}, {"OpenOldFile", 1, 4},
{"verbose", 1, 4}, {"MailSubject", 1, 4}, {"SendEmail", 1, 0}, {"UpdateItemZ", 0, 4},
{"UpdateGroupZ", 0, 4}, {"ReportGroupStatus", 0, 0}, {"CropImage", 5, 1},
{"ClearPersistentVars", 0, 4}, {"ElseIf", 3,4}, {"YesNoBox", 1,4}, {"MakeDirectory",1,4},
{"AllowFileOverwrite", 1, 4}, {"OppositeTrial", 0, 0}, {"OppositeFocus", 0, 0},
{"OppositeAutoFocus", 0, 0}, {"ReportAutoFocus", 0, 0}, {"ReportTargetDefocus", 0, 0},
{"Plugin",2,0}, {"ListPluginCalls",0,4}, {"SetStandardFocus",1,4}, {"SetCameraArea",2,4},
{"SetIlluminatedArea", 1, 0}, {"ReportIlluminatedArea", 0, 0}, {"SelectCamera", 1, 0},
{"SetLowDoseMode",1,0},{"TestSTEMshift",3,1},{"QuickFlyback",2,1},{"NormalizeLenses",1,0},
{"ReportSlotStatus", 1, 4}, {"LoadCartridge", 1, 0}, {"UnloadCartridge", 0, 0},
{"BacklashAdjust", 0, 0}, {"SaveFocus", 0, 4}, {"MakeDateTimeDir", 0, 4},
{"EnterNameOpenFile", 0, 4}, {"FindPixelSize", 0, 0}, {"SetEucentricFocus", 0, 4},
{"CameraProperties", 0, 4}, {"SetImageShift", 2, 0}, {"ReportImageShift", 0, 0},
{"EnterOneNumber", 1, 4}, {"SetHelperParams", 1, 4}, {"StepFocusNextShot", 2, 5},
{"BeamTiltDirection", 1, 5}, {"OpenFrameSumFile", 1, 4}, {"DeferStackingNextShot", 0, 4},
{"SetObjectiveStigmator",2,0}, {"ReportObjectiveStigmator",0,0}, {"AreDewarsFilling",0,4},
{"DewarsRemainingTime", 0, 4}, {"RefrigerantLevel", 0, 4}, {"OpenDECameraCover", 0, 0},
{"SmoothFocusNextShot", 2, 5}, {"ReportLowDose", 0, 4}, {"IsPVPRunning", 0, 4},
{"LongOperation", 1, 1}, {"UpdateHWDarkRef", 1, 1}, {"RestoreFocus", 0, 4},
{"CalEucentricFocus", 0, 4}, {"EarlyReturnNextShot", 1, 5}, {"SelectPiezo", 2, 0},
{"ReportPiezoXY", 0, 0}, {"ReportPiezoZ", 0, 0}, {"MovePiezoXY",2,1}, {"MovePiezoZ",1,1},
{"KeyBreak", 0, 4}, {"ReportAlpha", 0, 0}, {"SetAlpha", 1, 0}, {"SetSTEMDetectors", 2, 0},
{"SetCenteredSize", 4, 4}, {"ImageLowDoseSet", 0, 4}, {"ForceCenterRealign", 0, 4},
{"SetTargetDefocus", 1, 4}, {"FocusChangeLimits", 2, 5}, {"AbsoluteFocusLimits", 2, 5},
{"ReportColumnMode",0,0},{"ReportLens",1,0},{"ReportCoil",1,0},{"ReportAbsoluteFocus",0,0},
{"SetAbsoluteFocus", 1, 0}, {"ReportEnergyFilter", 0, 0}, {"ReportBeamTilt", 0, 0},
{"UserSetDirectory", 0, 4}, {"SetJeolSTEMflags", 2, 2}, {"ProgramTimeStamps", 0, 4},
{"SetContinuous", 2, 2}, {"UseContinuousFrames", 1, 4}, {"WaitForNextFrame", 0, 0},
{"StopContinuous", 0, 0}, {"ReportContinuous", 0, 4}, {"SetLiveSettleFraction", 1, 4},
{"SkipTo", 1, 4}, {"Function", 1, 4}, {"EndFunction", 0, 4}, {"CallFunction", 1, 4},
{"PreCookMontage", 1, 1}, {"ReportAlignTrimming", 0, 0}, {"NormalizeAllLenses", 0, 0},
{"AddToAutodoc", 6}, {"IsVariableDefined", 1, 4}, {"EnterDefaultedNumber", 3, 4},
{"SetupWaffleMontage", 2, 6}, {"IncMagIfFoundPixel", 1, 0}, {"ResetDefocus", 0, 0},
{"WriteAutodoc",0,4}, {"ElectronStats",0,4}, {"ErrorsToLog", 0,4}, {"FlashDisplay", 0,0},
{"ReportProbeMode",0,0},{"SetProbeMode",1,0},{"SaveLogOpenNew",0,4},{"SetAxisPosition",2,0}
, {"AddToFrameMdoc", 2, 4}, {"WriteFrameMdoc", 0, 4}, {"ReportFrameMdocOpen", 0, 4},
{"ShiftCalSkipLensNorm",0,4}, {"ReportLastAxisOffset",0,0}, {"SetBeamTilt",2,0},//end in 3.5
{"CorrectAstigmatism", 0, 0}, {"CorrectComa", 0, 0}, {"ShiftItemsByCurrentDiff", 1, 4},
{"ReportCurrentFilename", 0, 4}, {"SuffixForExtraFile", 1, 4}, {"ReportScreen", 0, 0},
{"GetDeferredSum",0,0}, {"ReportJeolGIF",0,0},{"SetJeolGIF",1,0},{"TiltDuringRecord",2,1},
{"SetLDContinuousUpdate", 1, 4}, {"ErrorBoxSendEmail", 1,4}, {"ReportLastFrameFile", 0,4},
{"Test", 1, 4}, {"AbortIfFailed", 1, 4}, {"PauseIfFailed", 0, 4}, {"ReadOtherFile", 3, 4},
{"ReportK2FileParams", 0, 0}, {"SetK2FileParams", 1, 4}, {"SetDoseFracParams", 1, 4},
{"SetK2ReadMode", 2, 4}, {"SetProcessing", 2, 4}, {"SetFrameTime", 2, 4},
{"ReportCountScaling", 0, 0}, {"SetDivideBy2", 1, 4}, {"ReportNumFramesSaved", 0, 0},
{"RemoveFile", 1, 4}, {"ReportFrameAliParams", 0, 0}, {"SetFrameAliParams", 1, 4},
{"Require", 1, 4}, {"OnStopCallFunc", 1,4}, {"RetryReadOtherFile",1,4}, {"DoScript",1,4},
{"CallScript",1,4}, {"ScriptName", 1, 4}, {"SetFrameAli2", 1,4}, {"ReportFrameAli2", 0,0},
{"ReportMinuteTime", 0, 0}, {"ReportColumnOrGunValve", 0, 0}, {"NavIndexWithLabel", 1,4},
{"NavIndexWithNote", 1, 4}, {"SetDiffractionFocus", 1, 0}, {"ReportDirectory", 0, 0},
{"BackgroundTilt",1,1}, {"ReportStageBusy",0,0}, {"SetExposureForMean",1,0}, {"FFT",1,0},
{"ReportNumNavAcquire", 0, 0}, {"StartFrameWaitTimer", 0,4}, {"ReportTickTime", 0, 0},
{"ReadTextFile",2,4},{"RunInShell",1,0},{"ElapsedTickTime",1,5},{"NoMessageBoxOnError",0,4},
{"ReportAutofocusOffset", 0, 0}, {"SetAutofocusOffset", 1, 4}, {"ChooserForNewFile", 2,4},
{"SkipAcquiringNavItem", 0, 4}, {"ShowMessageOnScope", 1, 0}, {"SetupScopeMessage", 1,6},
{"Search", 0, 0}, {"SetProperty", 2, 4}, /* End in 3.6 */{"SetMagIndex", 1, 0},
{"SetNavRegistration",1,4},{"LocalVar",1,4},{"LocalLoopIndexes",0,4},{"ZemlinTableau",1,1},
{"WaitForMidnight", 0, 1}, {"ReportUserSetting", 1, 0}, {"SetUserSetting", 2, 4},
{"ChangeItemRegistration",2,5}, {"ShiftItemsByMicrons",2,5}, {"SetFreeLensControl", 2, 1},
{"SetLensWithFLC", 2, 0}, {"SaveToOtherFile", 4, 4}, {"SkipAcquiringGroup", 0, 4},
{"ReportImageDistanceOffset", 0, 0}, {"SetImageDistanceOffset", 1, 0},
{"ReportCameraLength", 0, 0}, {"SetDECamFrameRate", 1, 4}, {"SkipMoveInNavAcquire", 0, 4},
{"TestRelaxingStage", 2, 1}, {"RelaxStage", 0, 1}, {"SkipFrameAliParamCheck", 0, 4},
{"IsVersionAtLeast", 1, 4}, {"SkipIfVersionLessThan", 1, 4}, {"RawElectronStats", 0, 4},
{"AlignWholeTSOnly", 0, 4}, {"WriteComForTSAlign", 0, 4}, {"RecordAndTiltTo", 1, 1},
{"ArePostActionsEnabled", 0, 4}, {"MeasureBeamSize", 0, 4}, {"MultipleRecords", 0, 1},
{"MoveBeamByMicrons",2,1}, {"MoveBeamByFieldFraction", 2, 1}, {"NewDEserverDarkRef", 2,0},
{"StartNavAcquireAtEnd", 0, 0}, {"ReduceImage", 2, 1}, {"ReportAxisPosition", 1, 0},
{"CtfFind",3,1}, {"CBAstigComa",3,0}, {"FixAstigmatismByCTF",0,0}, {"FixComaByCTF", 0, 0},
{"EchoEval", 0, 5}, {"ReportFileNumber", 0, 0}, {"ReportComaTiltNeeded", 0, 0},
{"ReportStigmatorNeeded", 0, 0}, {"SaveBeamTilt", 0, 4}, {"RestoreBeamTilt", 0, 0},
{"ReportComaVsISmatrix", 0, 0}, {"AdjustBeamTiltforIS", 0, 0}, {"LoadNavMap", 0, 0},
{"LoadOtherMap", 1, 0}, {"ReportLensFLCStatus", 1, 0}, {"TestNextMultiShot", 1, 0},
{"EnterString", 2, 4}, {"CompareStrings", 2, 4}, {"CompareNoCase", 2, 4},
{"ReportNextNavAcqItem", 0, 4}, {"ReportNumTableItems", 0, 4}, {"ChangeItemColor", 2, 4},
{"ChangeItemLabel", 2, 4}, {"StripEndingDigits", 2, 4}, {"MakeAnchorMap", 0, 0},
{"StageShiftByPixels", 2, 1}, {"ReportProperty", 1, 0}, {"SaveNavigator", 0, 4},
{"FrameThresholdNextShot", 1, 5}, {"QueueFrameTiltSeries", 3, 4},
{"FrameSeriesFromVar", 2, 4}, {"WriteFrameSeriesAngles", 1, 4}, {"EchoReplaceLine", 1, 5},
{"EchoNoLineEnd", 1, 5}, {"RemoveAperture", 1, 0}, {"ReInsertAperture", 1, 0},
{"PhasePlateToNextPos", 0, 0}, {"SetStageBAxis", 1, 1}, {"ReportStageBAxis", 0, 0},
{"DeferWritingFrameMdoc", 0, 4}, {"AddToNextFrameStackMdoc", 2, 4},
{"StartNextFrameStackMdoc", 2, 4}, {"ReportPhasePlatePos", 0, 0}, {"OpenFrameMdoc", 1, 4},
{"NextProcessArgs", 1, 4}, {"CreateProcess", 1, 0}, {"RunExternalTool", 1, 0},
{"ReportSpecimenShift", 0, 0}, {"ReportNavFile", 0, 0}, {"ReadNavFile", 1, 4},
{"MergeNavFile", 1, 4}, {"ReportIfNavOpen",0, 4}, /* End in 3.7 */
{"NewArray", 3, 4}, {"New2DArray", 1, 4}, {"AppendToArray", 2, 4},{"TruncateArray", 2, 4},
{"Read2DTextFile", 2, 4}, {"ArrayStatistics", 1, 4}, {"ReportFrameBaseName", 0, 0},
{"OpenTextFile", 4, 4}, {"WriteLineToFile", 1, 4}, {"ReadLineToArray", 2, 4},
{"ReadLineToString", 2, 4}, {"CloseTextFile", 1, 4}, {"FlushTextFile", 1, 4},
{"ReadStringsFromFile", 2, 4}, {"IsTextFileOpen", 1, 4},{"CurrentSettingsToLDArea", 1, 0},
{"UpdateLowDoseParams", 1, 0}, {"RestoreLowDoseParams", 0, 0}, {"CallStringArray", 1, 4},
{"StringArrayToScript", 1, 4}, {"MakeVarPersistent", 1, 4},{"SetLDAddedBeamButton", 0, 4},
{"KeepCameraSetChanges", 0, 4}, {"ReportDateTime", 0, 0}, {"ReportFilamentCurrent", 0, 0},
{"SetFilamentCurrent", 1, 0}, {"CloseFrameMdoc", 0, 4}, {"DriftWaitTask", 0, 1},
{"GetWaitTaskDrift", 0, 0}, {"CloseLogOpenNew", 0, 4}, {"SaveLog", 0, 4},
{"SaveCalibrations", 0, 4}, {"ReportCrossoverPercentC2", 0, 0},
{"ReportScreenCurrent", 0, 0}, {"ListVars", 0, 4 }, {"ListPersistentVars", 0, 4 },
{"ReportItemAcquire", 0, 0 }, {"SetItemAcquire", 0, 0 }, {"ChangeItemNote", 1, 4 },
{"CircularSubareaMean", 3, 4 }, {"ReportItemImageCoords", 0, 4},
{"SetFrameSeriesParams", 1, 4}, {"SetCustomTime", 1, 4}, {"ReportCustomInterval", 1, 0},
{"StageToLastMultiHole", 0, 0}, {"ImageShiftToLastMultiHole", 0, 0},
{"NavIndexItemDrawnOn", 1, 4}, {"SetMapAcquireState", 1, 0}, {"RestoreState", 0, 0},
{"RealignToMapDrawnOn", 2, 0}, {"GetRealignToItemError", 0, 4}, {"DoLoop", 3, 5},
{"ReportVacuumGauge", 1, 0}, {"ReportHighVoltage", 0, 0},{"OKBox", 1, 4},
{"LimitNextAutoAlign", 1, 5}, {"SetDoseRate", 1, 0},{"CheckForBadStripe", 0, 4},
{"ReportK3CDSmode", 0, 0}, {"SetK3CDSmode", 1, 4}, {"ConditionPhasePlate", 0, 0},
{"LinearFitToVars", 2, 4}, {"ReportNumMontagePieces", 0, 4}, {"NavItemFileToOpen", 1, 4},
{"CropCenterToSize", 3, 0}, {"AcquireToMatchBuffer", 1, 0}, {"ReportXLensDeflector", 1,0},
{"SetXLensDeflector", 3, 0}, {"ReportXLensFocus", 0, 0}, {"SetXLensFocus", 1, 0},
{"ExternalToolArgPlace", 1, 0},{"ReadOnlyUnlessAdmin", 0, 4},
{"ImageShiftByStageDiff", 2, 0},{"GetFileInWatchedDir", 1, 4},
{"RunScriptInWatchedDir", 1, 4}, {"ParseQuotedStrings", 0, 4}, {"SnapshotToFile", 6, 0},
{"CombineHolesToMulti", 1, 4}, {"UndoHoleCombining", 0, 4}, {"MoveStageWithSpeed", 3, 1},
{"FindHoles", 0, 0}, {"MakeNavPointsAtHoles", 0, 4}, {"ClearHoleFinder", 0, 4}, 
{"ReportFrameTiltSum", 0, 0}, {"ModifyFrameTSShifts", 3, 5},{"ReplaceFrameTSFocus", 2, 4}, 
{"ReplaceFrameTSShifts", 1, 4}, {"CameraToISMatrix", 1, 4}, {"ISToCameraMatrix", 1, 4}, 
{"CameraToStageMatrix", 1, 4}, {"StageToCameraMatrix", 1, 4},
{"CameraToSpecimenMatrix", 1, 4}, {"SpecimenToCameraMatrix", 1, 4},
{"ISToSpecimenMatrix", 1, 4}, {"SpecimenToISMatrix", 1, 4}, {"ISToStageMatrix", 1, 4}, 
{"StageToISMatrix", 1, 4},{"StageToSpecimenMatrix", 1, 4},{"SpecimenToStageMatrix", 1, 4},
{"ReportISforBufferShift", 0, 0}, {"AlignWithRotation", 3, 0}, {"Try", 0, 4},
{"Catch", 0, 4}, {"EndTry", 0, 4}, {"Throw", 0, 4},{"AlignAndTransformItems", 2, 0},
{"RotateMultiShotPattern", 1, 4},{"SetNavItemUserValue", 2, 6},
{"ReportItemUserValue", 2, 0},/* End in 3.8 */ {"FilterImage", 5, 0}, 
// Place for others to add commands
// End of reserved region
{"SetFolderForFrames", 1, 4},{"SetItemTargetDefocus", 2, 4},{"SetItemSeriesAngles", 3, 4},
{"SuspendNavRedraw", 0, 4}, {"DeferLogUpdates", 0, 4}, /*CAI3.9*/
{NULL, 0, 0}
};
// The longest is now 25 characters but 23 is a more common limit

#define NUM_COMMANDS (sizeof(cmdList) / sizeof(CmdItem))

// Be sure to add an entry for longHasTime when adding long operation
const char *CMacCmd::cLongKeys[MAX_LONG_OPERATIONS] = {"BU", "RE", "IN", "LO", "$=", "DA", "UN",
                                           "$=", "RS", "RT", "FF"};
int CMacCmd::cLongHasTime[MAX_LONG_OPERATIONS] = {1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};

CMacCmd::CMacCmd() : CMacroProcessor()
{
  int i;
  unsigned int hash;
  SEMBuildTime(__DATE__, __TIME__);
  for (i = 0; i < 5; i++)
    cmdList[i + CME_VIEW].mixedCase = (LPCTSTR)mModeNames[i];
  CString cstr;
  for (i = 0; i < NUM_COMMANDS - 1; i++) {
    cstr = cmdList[i].mixedCase;
    cstr.MakeUpper();
    cmdList[i].cmd = (LPCTSTR)cstr;
    if (cmdList[i].arithAllowed & 1)
      mArithAllowed.insert(cmdList[i].cmd);
    if (cmdList[i].arithAllowed & 2)
      mArithDenied.insert(cmdList[i].cmd);

    // Get a hash value from the upper case command string and map it to index, or to 0
    // if there is a collision
    hash = StringHashValue((LPCTSTR)cstr);
    if (!mCmdHashMap.count(hash))
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, i));
    else
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, 0));
  }
  mCmdList = &cmdList[0];
  mNumCommands = NUM_COMMANDS;
}

CMacCmd::~CMacCmd()
{
}

// When a task is done, if the macro flag is still set, call next command
void CMacCmd::TaskDone(int param)
{
  if (DoingMacro())
    NextCommand(false);
  else
    SuspendMacro();
}

void CMacCmd::NextCommand(bool startingOut)
{

  bool inComment = false;

  cReadOtherSleep = 2000;
  cCurrentCam = mWinApp->GetCurrentCamera();
  cFiltParam = mWinApp->GetFilterParams();
  cActiveList = mWinApp->GetActiveCameraList();
  cCamParams = mWinApp->GetCamParams() + cCurrentCam;
  cMontP = mWinApp->GetMontParam();
  cLdParam = mWinApp->GetLowDoseParams();
  cNavigator = mWinApp->mNavigator;

  mSleepTime = 0.;
  mDoseTarget = 0.;
  mMovedStage = false;
  mMovedScreen = false;
  mExposedFilm = false;
  mStartedLongOp = false;
  mMovedAperture = false;
  mLoadingMap = false;
  mLoopInOnIdle = false;

  if (mMovedPiezo && mWinApp->mPiezoControl->GetLastMovementError()) {
    AbortMacro();
    return;
  }
  mMovedPiezo = false;

  // First test for termination conditions
  // Stopping conditions that do not allow resumption:
  if (mTestTiltAngle && ((mControl->limitTiltUp
    && mScope->GetTiltAngle() > mControl->tiltUpLimit) ||
    (mControl->limitTiltDown && mScope->GetTiltAngle() < mControl->tiltDownLimit))) {
    mWinApp->AppendToLog("Script stopped because tilt limit specified\r\n"
      " in Script Control dialog was reached", LOG_MESSAGE_IF_CLOSED);
    AbortMacro();
    return;
  }

  if (mControl->limitRuns && mNumRuns >= mControl->runLimit) {
    mWinApp->AppendToLog("Script stopped because script run limit specified\r\n"
      " in Script Control dialog was reached", LOG_MESSAGE_IF_CLOSED);
    AbortMacro();
    return;
  }

  // Pop control set that was to be restored after shot
  if (mRestoreConsetAfterShot) {
    mRestoreConsetAfterShot = false;
    cI = mConsetNums.back();
    mConSets[cI] = mConsetsSaved.back();
    mConsetNums.pop_back();
    mConsetsSaved.pop_back();
  }

  // Stopping conditions that are suspensions - but backing up is a bad idea
  if (mTestMontError && mWinApp->Montaging() && mControl->limitMontError &&
    mLastMontError > mControl->montErrorLimit) {
    mWinApp->AppendToLog("Script suspended because montage error specified\r\n"
      " in Script Control dialog exceeded limit", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return;
  }

  cIndex = mTestMontError ? 1 : 0;
  if (mControl->limitScaleMax && mImBufs[cIndex].mImage && mImBufs[cIndex].mImageScale &&
    mTestScale && mImBufs[cIndex].mImageScale->GetMaxScale() < mControl->scaleMaxLimit) {
    mWinApp->AppendToLog("Script suspended because image intensity fell below limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return;
  }

  if (mControl->limitIS) {
    double ISX, ISY, cSpecX, cSpecY;
    ScaleMat cMat;
    mScope->GetLDCenteredShift(ISX, ISY);

    cMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    cSpecX = cMat.xpx * ISX + cMat.xpy * ISY;
    cSpecY = cMat.ypx * ISX + cMat.ypy * ISY;
    if (cMat.xpx != 0.0 && sqrt(cSpecX * cSpecX + cSpecY * cSpecY) > mControl->ISlimit) {
      mWinApp->AppendToLog("Script suspended because image shift exceeded limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
      SuspendMacro();
      return;
    }
  }

  // Crop image now if that was needed
  if (mCropXafterShot > 0) {
    mImBufs->mImage->getSize(cSizeX, cSizeY);
    cIx0 = (cSizeX - mCropXafterShot) / 2;
    cIy0 = (cSizeY - mCropYafterShot) / 2;
    cIx1 = cIx0 + mCropXafterShot - 1;
    cIy1 = cIy0 + mCropYafterShot - 1;
    mCropXafterShot = -1;
    cIx0 = mWinApp->mProcessImage->CropImage(mImBufs, cIy0, cIx0, cIy1, cIx1);
    if (cIx0) {
      cReport.Format("Error # %d attempting to crop new image to match buffer", cIx0);
      ABORT_NOLINE(cReport);
    }
    mImBufs->mCaptured = BUFFER_CROPPED;
  }

  if (mShowedScopeBox)
    SetReportedValues(1., mScope->GetMessageBoxReturnValue());
  mShowedScopeBox = false;

  // Save the current index
  mLastIndex = mCurrentIndex;

  // Find the next real command
  CString * cMacro = &mMacros[mCurrentMacro];
  cStrItems[0] = "";
  while (cStrItems[0].IsEmpty() && mCurrentIndex < cMacro->GetLength()) {

    GetNextLine(cMacro, mCurrentIndex, cStrLine);
    if (inComment) {
      if (cStrLine.Find("*/") >= 0)
        inComment = false;
      continue;
    }
    cStrCopy = cStrLine;

    // Parse the line
    if (mWinApp->mParamIO->ParseString(cStrCopy, cStrItems, MAX_MACRO_TOKENS, 
      mParseQuotes))
      ABORT_LINE("Too many items on line in script: \n\n");
    if (cStrItems[0].Find("/*") == 0) {
      cStrItems[0] = "";
      inComment = true;
      continue;
    }
    if (mVerbose > 0)
      mWinApp->AppendToLog("COMMAND: " + cStrLine, LOG_OPEN_IF_CLOSED);
    cStrItems[0].MakeUpper();
  }

  // Convert a single number to a DOMACRO (obsolete and bad!)
  InsertDomacro(&cStrItems[0]);

  // Substitute variables in parsed items and check for control word substitution
  cReport = cStrItems[0];
  if (SubstituteVariables(cStrItems, MAX_MACRO_TOKENS, cStrLine)) {
    AbortMacro();
    return;
  }
  cStrItems[0].MakeUpper();
  cItem1upper = cStrItems[1];
  cItem1upper.MakeUpper();
  if (cReport != cStrItems[0] && WordIsReserved(cStrItems[0]))
    ABORT_LINE("You cannot make a command by substituting a\n"
      "variable value that is a control command on line: \n\n");

  cCmdIndex = LookupCommandIndex(cStrItems[0]);

  // Do arithmetic on selected commands
  if (cCmdIndex >= 0 && ArithmeticIsAllowed(cStrItems[0])) {
    if (SeparateParentheses(&cStrItems[1], MAX_MACRO_TOKENS - 1))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    EvaluateExpression(&cStrItems[1], MAX_MACRO_TOKENS - 1, cStrLine, 0, cIndex, cIndex2);
    for (cI = cIndex + 1; cI <= cIndex2; cI++)
      cStrItems[cI] = "";
    cIndex = CheckLegalCommandAndArgNum(&cStrItems[0], cStrLine, mCurrentMacro);
    if (cIndex) {
      if (cIndex == 1)
        ABORT_LINE("There is no longer a legal command after evaluating arithmetic on "
          "this line:\n\n");
      AbortMacro();
      return;
    }
  }

  // Evaluate emptiness, ints and doubles
  for (cI = 0; cI < MAX_MACRO_TOKENS; cI++) {
    cItemEmpty[cI] = cStrItems[cI].IsEmpty();
    cItemInt[cI] = 0;
    cItemDbl[cI] = 0.;
    if (!cItemEmpty[cI]) {
      cItemInt[cI] = atoi((LPCTSTR)cStrItems[cI]);
      cItemDbl[cI] = atof((LPCTSTR)cStrItems[cI]);
      cLastNonEmptyInd = cI;
    }
    cItemFlt[cI] = (float)cItemDbl[cI];
  }
  if (cItemEmpty[0]) {
    cCmdIndex = CME_SCRIPTEND;
  } else {
    if (cStrItems[0].GetAt(cStrItems[0].GetLength() - 1) == ':')
      cCmdIndex = CME_LABEL;
    else if (cStrItems[1] == "@=" || cStrItems[1] == ":@=")
      cCmdIndex = CME_SETSTRINGVAR;
    else if (cStrItems[1] == "=" || cStrItems[1] == ":=")
      cCmdIndex = CME_SETVARIABLE;
  }
  if (CMD_IS(ELSEIF) && mBlockLevel >= 0 && mLoopLimit[mBlockLevel] == LOOP_LIMIT_FOR_IF)
    cCmdIndex = CME_ZEROLOOPELSEIF;

  // See if we are supposed to stop at an ending place
  if (mStopAtEnd && (CMD_IS(REPEAT) || CMD_IS(ENDLOOP) || CMD_IS(DOMACRO) ||
    CMD_IS(DOSCRIPT))) {
    if (mLastIndex >= 0)
      mCurrentIndex = mLastIndex;
    SuspendMacro();   // Leave it resumable
    return;
  }

  cKeyBreak = CMD_IS(KEYBREAK) && ((cItemEmpty[1] && mKeyPressed == 'B') ||
    (cStrItems[1].GetLength() == 1 && cItem1upper.GetAt(0) == mKeyPressed));
  if (cKeyBreak)
    cCmdIndex = CME_DOKEYBREAK;

  // THE MASSIVE SWITCH ON COMMAND INDEXES
  switch (cCmdIndex) {

  // If we are at end, finish up unless there is a caller to return to
  case CME_SCRIPTEND:                                       // Script end
  case CME_EXIT:                                            // Exit
  case CME_RETURN:                                          // Return
  case CME_ENDFUNCTION:                                     // EndFunction
  case CME_FUNCTION:                                        // Function
    if (!mCallLevel || CMD_IS(EXIT) || (CMD_IS(ENDFUNCTION) && mExitAtFuncEnd)) {
      AbortMacro();
      mLastCompleted = !mExitAtFuncEnd;
      if (mLastCompleted && mStartNavAcqAtEnd)
        mWinApp->AddIdleTask(TASK_START_NAV_ACQ, 0, 0);
      return;
    }

    // For a return, pop any loops, clear index variables
    LeaveCallLevel(CMD_IS(RETURN));
    break;

  case CME_REPEAT:                                          // Repeat
    mCurrentIndex = 0;
    mLastIndex = -1;
    mNumRuns++;
    break;

  case CME_LABEL:                                           // Label
    // Nothing to do here!
    break;

  case CME_REQUIRE:                                         // Require
    break;

  case CME_ENDLOOP:                                         // EndLoop

    // First see if we are actually doing a loop: if not, error
    if (mBlockDepths[mCallLevel] < 0 || mLoopLimit[mBlockLevel] < LOOP_LIMIT_FOR_IF ||
      mLoopLimit[mBlockLevel] == LOOP_LIMIT_FOR_TRY) {
      ABORT_NOLINE("The script contains an ENDLOOP without a LOOP or DOLOOP statement");
    }

    // If count is not past limit, go back to start;
    // otherwise clear index variable and decrease level indexes
    mLoopCount[mBlockLevel] += mLoopIncrement[mBlockLevel];
    if ((mLoopIncrement[mBlockLevel] < 0 ? - 1 : 1) *
      (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) <= 0)
      mCurrentIndex = mLoopStart[mBlockLevel];
    else {
      ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
    mLastIndex = -1;
    break;

  case CME_LOOP:                                            // Loop
  case CME_DOLOOP:                                          // DoLoop

    // Doing a loop: get the count, make sure it is legal, and save the current
    // index as the starting place to go back to
    if (mBlockLevel >= MAX_LOOP_DEPTH)
      ABORT_LINE("Nesting of loops, IF blocks, and script or function calls is too deep"
      " at line: \n\n:");
    mBlockLevel++;
    mBlockDepths[mCallLevel]++;
    mLoopStart[mBlockLevel] = mCurrentIndex;
    mLoopIncrement[mBlockLevel] = 1;
    if (CMD_IS(LOOP)) {
      mLoopLimit[mBlockLevel] = B3DMAX(0, B3DNINT(cItemDbl[1]));
      mLoopCount[mBlockLevel] = 1;
      cIndex = 2;
    } else {
      mLoopCount[mBlockLevel] = cItemInt[2];
      mLoopLimit[mBlockLevel] = cItemInt[3];
      if (!cItemEmpty[4]) {
        mLoopIncrement[mBlockLevel] = cItemInt[4];
        if (!cItemInt[4])
          ABORT_LINE("The loop increment is 0 at line:\n\n");
      }
      cIndex = 1;
    }
    mLastIndex = -1;
    if ((mLoopIncrement[mBlockLevel] < 0 ? - 1 : 1) *
      (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) > 0) {
      if (SkipToBlockEnd(SKIPTO_ENDLOOP, cStrLine)) {
        AbortMacro();
        return;
      }
    } else if (!cItemEmpty[cIndex]) {
      if (SetVariable(cStrItems[cIndex], 1.0, VARTYPE_INDEX, mBlockLevel, true, &cReport))
        ABORT_LINE(cReport + " in script line: \n\n");
    }
    break;

  case CME_IF:
  case CME_ZEROLOOPELSEIF:                                  // If, Elseif

    // IF statement: evaluate it, go up one block level with 0 limit
    if (EvaluateIfStatement(&cStrItems[1], MAX_MACRO_TOKENS - 1, cStrLine, cTruth)) {
      AbortMacro();
      return;
    }
    if (CMD_IS(IF)) {                                       // If
      mBlockLevel++;
      if (mBlockLevel >= MAX_LOOP_DEPTH)
        ABORT_LINE("Nesting of loops, IF blocks, and script or function calls is too deep"
        " at line: \n\n:");
      mBlockDepths[mCallLevel]++;
      mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_IF;
    }

    // If not true, skip forward; if it is true, mark if as satisfied with a -1
    if (!cTruth) {
      if (SkipToBlockEnd(SKIPTO_ELSE_ENDIF, cStrLine) ){
        AbortMacro();
        return;
      }
      mLastIndex = -1;
    } else
      mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_IF - 1;
    break;

  case CME_ENDIF:                                           // Endif

    // Trust the initial check
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
    mLastIndex = -1;
    break;

  case CME_ELSE:
  case CME_ELSEIF:                                          // Else, Elseif
    if (SkipToBlockEnd(SKIPTO_ENDIF, cStrLine)) {
      AbortMacro();
      return;
    }
    mLastIndex = -1;
    break;

  case CME_BREAK:
  case CME_CONTINUE:
  case CME_DOKEYBREAK:    // Break, Continue

    if (SkipToBlockEnd(SKIPTO_ENDLOOP, cStrLine, &cIndex, &cIndex2)) {
      AbortMacro();
      return;
    }
    mLastIndex = -1;

    // Pop any IFs and TRYs on the loop stack
    while (mBlockLevel >= 0 && mLoopLimit[mBlockLevel] <= LOOP_LIMIT_FOR_TRY) {
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
    mTryCatchLevel += cIndex2;
    if (mBlockLevel >= 0 && (CMD_IS(BREAK) || cKeyBreak))
      mLoopCount[mBlockLevel] = mLoopLimit[mBlockLevel];
    if (cKeyBreak) {
      PrintfToLog("Broke out of loop after %c key pressed", (char)mKeyPressed);
      mKeyPressed = 0;
    }
    break;

  case CME_TRY:                                             // Try
    mBlockLevel++;
    if (mBlockLevel >= MAX_LOOP_DEPTH)
      ABORT_LINE("Nesting of loops, IF or TRY blocks, and script or function calls is too"
        " deep at line: \n\n:");
    mBlockDepths[mCallLevel]++;
    mLoopLimit[mBlockLevel] = LOOP_LIMIT_FOR_TRY;
    mTryCatchLevel++;
    break;

  case CME_CATCH:                                           // Catch
    if (SkipToBlockEnd(SKIPTO_ENDTRY, cStrLine)) {
      AbortMacro();
      return;
    }
    mTryCatchLevel--;
    mLastIndex = -1;
    break;

  case CME_ENDTRY:                                          // EndTry
    mBlockLevel--;
    mBlockDepths[mCallLevel]--;
    mLastIndex = -1;
    break;

  case CME_THROW:                                           // Throw
    cStrCopy = "";
    if (!cStrItems[1].IsEmpty())
      SubstituteLineStripItems(cStrLine, 1, cStrCopy);
    if (TestTryLevelAndSkip(&cStrCopy))
      return;
    ABORT_NOLINE("Stopping because a THROW statement was encountered outside of a TRY "
      "block");
    break;

  case CME_SKIPTO:                                          // SkipTo
    if (SkipToLabel(cItem1upper, cStrLine, cIndex, cIndex2)) {
      AbortMacro();
      return;
    }

    // Pop proper number of ifs and loops from stack
    for (cI = 0; cI < cIndex && mBlockLevel >= 0; cI++) {
      ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
    mTryCatchLevel += cIndex2;
    mLastIndex = -1;
    break;

  case CME_DOMACRO:                                         // DoMacro
  case CME_DOSCRIPT:                                        // DoScript
  case CME_CALLMACRO:                                       // CallMacro
  case CME_CALLSCRIPT:                                      // CallScript
  case CME_CALL:                                            // Call
  case CME_CALLFUNCTION:                                    // CallFunction
  case CME_CALLSTRINGARRAY:                                 // CallStringArray

    // Skip any of these operations if we are in a termination function
    if (!mExitAtFuncEnd) {

      // Calling a macro or function: rely on initial error checks, get the number
      cIndex2 = 0;
      cFunc = NULL;
      if (CMD_IS(CALLFUNCTION)) {
        cFunc = FindCalledFunction(cStrLine, false, cIndex, cIx0);
        if (!cFunc)
          AbortMacro();
        cIndex2 = cFunc->startIndex;
        if (cFunc->wasCalled)
          ABORT_LINE("Trying to call a function already being called in line: \n\n");
        cFunc->wasCalled = true;
      } else if (CMD_IS(CALL)) {
        cIndex = FindCalledMacro(cStrLine, false);
      } else if (CMD_IS(CALLSTRINGARRAY)) {
        cIndex = MakeNewTempMacro(cStrItems[1], cStrItems[2], true, cStrLine);
        if (!cIndex)
          return;
        cIx0 = 0;
        if (CheckBlockNesting(cIndex, -1, cIx0)) {
          AbortMacro();
          return;
        }

      } else {
        cIndex = cItemInt[1] - 1;
      }

      // Save the current index at this level and move up a level
      // If doing a function with string arg, substitute in line for that first so the
      // pre-existing level applies
      if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
        if (mCallLevel >= MAX_CALL_DEPTH)
          ABORT_LINE("Trying to call too many levels of scripts/functions in line: \n\n");
        if (cFunc && cFunc->ifStringArg)
          SubstituteVariables(&cStrLine, 1, cStrLine);
        mCallIndex[mCallLevel++] = mCurrentIndex;
        mCallMacro[mCallLevel] = cIndex;
        mBlockDepths[mCallLevel] = -1;
        mCallFunction[mCallLevel] = cFunc;
      } else {
        mNumRuns++;
      }

      // Doing another macro (or this one over), just reset macro # and index
      mCurrentMacro = cIndex;
      mCurrentIndex = cIndex2;
      mLastIndex = -1;

      // Create argument variables now that level is raised
      if (cFunc && (cFunc->numNumericArgs || cFunc->ifStringArg)) {
        cTruth = false;
        for (cIndex = 0; cIndex < cFunc->numNumericArgs; cIndex++) {
          cReport = cFunc->argNames[cIndex];
          if (SetVariable(cFunc->argNames[cIndex], cItemEmpty[cIndex + cIx0] ?
            0. : cItemDbl[cIndex + cIx0], VARTYPE_LOCAL, -1, false, &cReport)) {
              cTruth = true;
              break;
          }
        }
        if (!cTruth && cFunc->ifStringArg) {
          mWinApp->mParamIO->StripItems(cStrLine, cIndex + cIx0, cStrCopy);
          cTruth = SetVariable(cFunc->argNames[cIndex], cStrCopy, VARTYPE_LOCAL,
            - 1, false, &cReport);
        }
        if (cTruth)
          ABORT_LINE("Failed to set argument variables in function call:\r\n" + cReport +
          " in line:\n\n")
        for (cIndex = 0; cIndex < cFunc->numNumericArgs + (cFunc->ifStringArg ? 1 : 0);
          cIndex++)
            if (cItemEmpty[cIndex + cIx0])
              break;
        SetVariable("NUMCALLARGS", cIndex, VARTYPE_LOCAL, -1, false);
     }
    }
    break;

  case CME_PARSEQUOTEDSTRINGS:                              // ParseQuotedStrings
    mParseQuotes = cItemEmpty[1] || cItemInt[1] != 0;
    break;

  case CME_STRINGARRAYTOSCRIPT:                             // StringArrayToScript
    cIndex = MakeNewTempMacro(cStrItems[1], cStrItems[2], false, cStrLine);
    if (!cIndex)
      return;
    break;

  case CME_ONSTOPCALLFUNC:                                  // OnStopCallFunc
    if (!mExitAtFuncEnd) {
      cFunc = FindCalledFunction(cStrLine, false, mOnStopMacroIndex, cIx0);
      if (!cFunc) {
        AbortMacro();
        return;
      }
      mOnStopLineIndex = cFunc->startIndex;
      if (cFunc->numNumericArgs || cFunc->ifStringArg)
        ABORT_LINE("The function to call takes arguments, which is not allowed here:"
        "\n\n");
    }
    break;

  case CME_NOMESSAGEBOXONERROR:                             // NoMessageBoxOnError
    mNoMessageBoxOnError = true;
    if (!cItemEmpty[1])
      mNoMessageBoxOnError = cItemInt[1] != 0;
    break;

  case CME_KEYBREAK:
    break;

  case CME_TEST:                                            // Test
    if (cItemEmpty[2])
      mLastTestResult = cItemDbl[1] != 0.;
    else
      EvaluateIfStatement(&cStrItems[1], MAX_MACRO_TOKENS - 1, cStrLine, mLastTestResult);
    SetReportedValues(mLastTestResult ? 1. : 0.);
    SetVariable("TESTRESULT", mLastTestResult ? 1. : 0., VARTYPE_REGULAR, -1, false);
    break;

  case CME_SETVARIABLE:                                     // Variable assignment
  case CME_APPENDTOARRAY:                                   // AppendToArray

    // Do assignment to variable before any non-reserved commands
    cIndex2 = 2;
    cIx0 = CheckForArrayAssignment(cStrItems, cIndex2);
    if (SeparateParentheses(&cStrItems[cIndex2], MAX_MACRO_TOKENS - cIndex2))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    if (EvaluateExpression(&cStrItems[cIndex2], MAX_MACRO_TOKENS - cIndex2, cStrLine, cIx0, cIndex,
      cIx1)) {
        AbortMacro();
        return;
    }

    // Concatenate array elements separated by \n
    if (cIx0 > 0) {
      for (cIx1 = cIndex2 + 1; cIx1 < MAX_MACRO_TOKENS; cIx1++) {
        if (cStrItems[cIx1].IsEmpty())
          break;
        cStrItems[cIndex2] += "\n" + cStrItems[cIx1];
      }
    }

    // Set the variable
    if (CMD_IS(SETVARIABLE)) {
      cIndex = cStrItems[1] == "=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
      if (SetVariable(cStrItems[0], cStrItems[cIndex2], cIndex, -1, false, &cReport))
        ABORT_LINE(cReport + " in script line: \n\n");

    } else {

      // Append to array: Split off an index to look it up
      ArrayRow newRow;

      cVar = GetVariableValuePointers(cItem1upper, &cValPtr, &cNumElemPtr, "append to",
        cReport);
      if (!cVar)
        ABORT_LINE(cReport + " for line:\n\n");
      cTruth = cValPtr == NULL;
      if (cTruth) {
        cValPtr = &newRow.value;
        cNumElemPtr = &newRow.numElements;
      }

      // Add the string to the value and compute the number of elements
      if (!cValPtr->IsEmpty())
        *cValPtr += "\n";
      *cValPtr += cStrItems[cIndex2];
      *cNumElemPtr = 1;
      cIx0 = -1;
      while ((cIx0 = cValPtr->Find('\n', cIx0 + 1)) >= 0)
        *cNumElemPtr += 1;

      // Both 1D array and existing row of 2D array should be done: add new row to 2D
      if (cTruth)
        cVar->rowsFor2d->Add(newRow);
    }
    break;

  case CME_TRUNCATEARRAY:                                   // TruncateArray
    cIndex = cItemInt[2];
    if (cIndex < 1)
      ABORT_LINE("The number to truncate to must be at least 1 for line:\n\n");
    cVar = GetVariableValuePointers(cItem1upper, &cValPtr, &cNumElemPtr, "truncate", cReport);
    if (!cVar)
      ABORT_LINE(cReport + " for line:\n\n");
    if (!cValPtr) {
      if (cVar->rowsFor2d->GetSize() > cIndex)
        cVar->rowsFor2d->SetSize(cIndex);
    } else if (*cNumElemPtr > cIndex) {
      FindValueAtIndex(*cValPtr, cIndex, cIx0, cIx1);
      *cValPtr = cValPtr->Left(cIx1);
      *cNumElemPtr = cIndex;
    }
    break;

  case CME_ARRAYSTATISTICS:                                 // ArrayStatistics
  {
    float * fvalues = FloatArrayFromVariable(cItem1upper, cIndex2, cReport);
    if (!fvalues)
      ABORT_LINE(cReport + " in line\n\n");
    cBmin = 1.e37f;
    cBmax = -1.e37f;
    for (cIndex = 0; cIndex < cIndex2; cIndex++) {
      ACCUM_MIN(cBmin, fvalues[cIndex]);
      ACCUM_MAX(cBmax, fvalues[cIndex]);
    }
    avgSD(fvalues, cIndex2, &cBmean, &cBSD, &cCpe);
    rsFastMedianInPlace(fvalues, cIndex2, &cCpe);
    cLogRpt.Format("n= %d  min= %.6g  max = %.6g  mean= %.6g  sd= %.6g  median= %.6g",
      cIndex2, cBmin, cBmax, cBmean, cBSD, cCpe);
    SetReportedValues(&cStrItems[2], cIndex2, cBmin, cBmax, cBmean, cBSD, cCpe);

    delete[] fvalues;
    break;
  }

  case CME_LINEARFITTOVARS:                                 // LinearFitToVars
  {
    float * xx1, *xx2, *xx3 = NULL;
    cTruth = !cItemEmpty[3];
    xx1 = FloatArrayFromVariable(cItem1upper, cIndex, cReport);
    if (!xx1)
      ABORT_LINE(cReport + " in line\n\n");
    xx2 = FloatArrayFromVariable(cStrItems[2], cIndex2, cReport);
    if (!xx2) {
      delete[] xx1;
      ABORT_LINE(cReport + " in line\n\n");
    }
    if (cTruth) {
      xx3 = FloatArrayFromVariable(cStrItems[3], cIx0, cReport);
      if (!xx3) {
        delete[] xx1, xx2;
        ABORT_LINE(cReport + " in line\n\n");
      }
    }
    if (cIndex != cIndex2 || (cTruth && cIx0 != cIndex)) {
      delete[] xx1, xx2, xx3;
      ABORT_LINE("Variables do not have the same number of elements in line:\n\n");
    }
    if (cIndex < 2 || (cTruth && cIndex < 3)) {
      delete[] xx1, xx2, xx3;
      ABORT_LINE("There are not enough array values for a linear fit in line:\n\n");
    }
    if (cTruth) {
      lsFit2(xx1, xx2, xx3, cIndex, &cBmean, &cBSD, &cCpe);
      cLogRpt.Format("n= %d  a1= %f  a2= %f  c= %f", cIndex, cBmean, cBSD, cCpe);
    } else {
      lsFit(xx1, xx2, cIndex, &cBmean, &cBSD, &cCpe);
      cLogRpt.Format("n= %d  slope= %f  intercept= %f  ro= %.4f", cIndex, cBmean, cBSD, cCpe);
    }
    SetReportedValues(cIndex, cBmean, cBSD, cCpe);
    delete[] xx1, xx2, xx3;
    break;
  }

  case CME_SETSTRINGVAR:                                    // String Assignment
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
    cIndex = cStrItems[1] == "@=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
    if (SetVariable(cStrItems[0], cStrCopy, cIndex, -1, false, &cReport))
      ABORT_LINE(cReport + " in script line: \n\n");
    break;

  case CME_LISTVARS:                                        // ListVars
    ListVariables();
    break;

  case CME_LISTPERSISTENTVARS:                              // ListPersistentVars
    ListVariables(VARTYPE_PERSIST);
    break;

  case CME_CLEARPERSISTENTVARS:                             // ClearPersistentVars
    ClearVariables(VARTYPE_PERSIST);
    break;

  case CME_MAKEVARPERSISTENT:                               // MakeVarPersistent
    cVar = LookupVariable(cStrItems[1], cIx0);
    if (!cVar)
      ABORT_LINE("The variable " + cStrItems[1] + " is not defined in line:\n\n");
    cIndex = 1;
    if (!cItemEmpty[2] && !cItemInt[2])
      cIndex = 0;
    if ((cIndex && cVar->type != VARTYPE_REGULAR) ||
      (!cIndex && cVar->type != VARTYPE_PERSIST))
      ABORT_LINE("The variable " + cStrItems[1] + " must be " +
        CString(cIndex ? "regular" : "persistent") + " to change its type in line:\n\n");
    cVar->type = cIndex ? VARTYPE_PERSIST : VARTYPE_REGULAR;
    break;

  case CME_ISVARIABLEDEFINED:                               // IsVariableDefined
    cIndex = B3DCHOICE(LookupVariable(cItem1upper, cIndex2) != NULL, 1, 0);
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->ParseString(cStrLine, cStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    cLogRpt.Format("Variable %s %s defined", (LPCTSTR)cStrItems[1],
      B3DCHOICE(cIndex, "IS", "is NOT"));
    SetReportedValues(&cStrItems[2], cIndex);
    break;

  case CME_NEWARRAY:                                        // NewArray
  case CME_NEW2DARRAY:                                      // New2DArray
  {
    CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
    ArrayRow arrRow;

    // Process local vesus persistent property
    cIx0 = VARTYPE_REGULAR;
    if (cItemInt[2] < 0) {
      cIx0 = VARTYPE_LOCAL;
      if (LocalVarAlreadyDefined(cStrItems[1], cStrLine) > 0)
        return;
    }
    if (cItemInt[2] > 0)
      cIx0 = VARTYPE_PERSIST;

    // Get number of elements: 3 for 1D and 4 for 2D
    cTruth = CMD_IS(NEW2DARRAY);
    cIndex = !cItemEmpty[3] ? cItemInt[3] : 0;
    if (cTruth) {
      cIndex2 = !cItemEmpty[3] ? cItemInt[3] : 0;
      cIndex = !cItemEmpty[4] ? cItemInt[4] : 0;
      if (cIndex > 0 && !cIndex2)
        ABORT_LINE("The number of elements per row must be 0 because no rows are created"
        " in:\n\n");
    }
    if (cIndex < 0 || (cTruth && cIndex2 < 0))
      ABORT_LINE("The number of elements to create must be positive in:\n\n");
    cStrCopy = "0";
    for (cIx1 = 1; cIx1 < cIndex; cIx1++)
      cStrCopy += "\n0";

    // Create the 2D array rows and add string to the rows
    if (cTruth) {
      rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
      arrRow.value = cStrCopy;
      arrRow.numElements = cIndex;
      for (cIy0 = 0; cIy0 < cIndex2; cIy0++)
        rowsFor2d->Add(arrRow);
    }
    if (SetVariable(cItem1upper, cTruth ? "0" : cStrCopy, cIx0, -1, false, &cReport,
      rowsFor2d)) {
      delete rowsFor2d;
      ABORT_LINE(cReport + " in script line: \n\n");
    }
    break;
  }

  case CME_LOCALVAR:                                        // LocalVar
    for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !cItemEmpty[cIndex]; cIndex++) {
      cIndex2 = LocalVarAlreadyDefined(cStrItems[cIndex], cStrLine);
      if (cIndex2 > 0)
        return;
      if (!cIndex2 && SetVariable(cStrItems[cIndex], "0", VARTYPE_LOCAL, mCurrentMacro, true,
        &cReport))
        ABORT_LINE(cReport + " in script line: \n\n");
    }
    break;

  case CME_LOCALLOOPINDEXES:                                // LocalLoopIndexes
    if (!cItemEmpty[1])
      ABORT_NOLINE("LocalLoopIndexes does not take any values; it cannot be turned off");
    if (mBlockLevel > 0)
      ABORT_NOLINE("LocalLoopIndexes cannot be used inside of a loop");
    mLoopIndsAreLocal = true;
    break;

  case CME_PLUGIN:                                          // Plugin
    SubstituteVariables(&cStrLine, 1, cStrLine);
    cDelX = mWinApp->mPluginManager->ExecuteCommand(cStrLine, cItemInt, cItemDbl, cItemEmpty,
      cReport, cDelY, cDelISX, cDelISY, cIndex2, cIndex);
    if (cIndex) {
      AbortMacro();
      return;
    }
    cStrCopy = "";
    if (cIndex2 == 3) {
      SetReportedValues(cDelX, cDelY, cDelISX, cDelISY);
      cStrCopy.Format(" and it returned %6g, %6g, %6g", cDelY, cDelISX, cDelISY);
    } else if (cIndex == 2) {
      SetReportedValues(cDelX, cDelY, cDelISX);
      cStrCopy.Format(" and it returned %6g, %6g", cDelY, cDelISX);
    } else if (cIndex == 1) {
      SetReportedValues(cDelX, cDelY);
      cStrCopy.Format(" and it returned %6g", cDelY);
    } else
      SetReportedValues(cDelX);
    cReport += cStrCopy;
    mWinApp->AppendToLog(cReport, mLogAction);
    break;

  case CME_LISTPLUGINCALLS:                                 // ListPluginCalls
    mWinApp->mPluginManager->ListCalls();
    break;

  case CME_FLASHDISPLAY:                                    // FlashDisplay
    cDelX = 0.3;
    cIndex2 = 4;
    cI = RGB(192, 192, 0);
    if (cItemInt[1] > 0)
      cIndex2 = cItemInt[1];
    if (!cItemEmpty[2] && cItemDbl[2] >= 0.001)
      cDelX = cItemDbl[2];
    if (!cItemEmpty[3] && cStrItems[3].GetLength() == 6)
      if (sscanf(cStrItems[3], "%x", &cIx0) == 1)
        cI = ((cIx0 & 0xFF0000) >> 16) | (cIx0 & 0x00FF00) | ((cIx0 & 0x0000FF) << 16);
    if (cDelX * cIndex2 > 3)
      ABORT_LINE("Flashing duration is too long in line:\r\n\r\n");
    for (cIndex = 0; cIndex < cIndex2; cIndex++) {
      mWinApp->mMainView->SetFlashNextDisplay(true);
      mWinApp->mMainView->SetFlashColor(cI);
      mWinApp->mMainView->DrawImage();
      Sleep(B3DNINT(1000. * cDelX));
      mWinApp->mMainView->DrawImage();
      if (cIndex < cIndex2 - 1)
        Sleep(B3DNINT(1000. * cDelX));
    }
    break;

  case CME_TILTUP:                                          // TiltUp
  case CME_U:

    // For tilting, if stage is ready, do the action; otherwise back up the index
    if (mScope->StageBusy() <= 0) {
      SetIntensityFactor(1);
      SetupStageRestoreAfterTilt(&cStrItems[1], cDelISX, cDelISY);
      mScope->TiltUp(cDelISX, cDelISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    } else
      mLastIndex = mCurrentIndex;
    break;

  case CME_TILTDOWN:                                        // TiltDown
  case CME_D:
    if (mScope->StageBusy() <= 0) {
      SetIntensityFactor(-1);
      SetupStageRestoreAfterTilt(&cStrItems[1], cDelISX, cDelISY);
      mScope->TiltDown(cDelISX, cDelISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    } else
      mLastIndex = mCurrentIndex;
    break;

  case CME_TILTTO:                                          // TiltTo
  case CME_TILTBY:                                          // TiltBy
  {
    double angle = cItemFlt[1];
    if (CMD_IS(TILTBY))
      angle += mScope->GetTiltAngle();
    if (fabs(angle) > mScope->GetMaxTiltAngle() + 0.05) {
      cReport.Format("Attempt to tilt too high in script, to %.1f degrees, in line:\n\n",
        angle);
      ABORT_LINE(cReport);
    }
    if (mScope->StageBusy() <= 0) {
      SetupStageRestoreAfterTilt(&cStrItems[2], cDelISX, cDelISY);
      mScope->TiltTo(angle, cDelISX, cDelISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    }
    else
      mLastIndex = mCurrentIndex;
    break;
  }

  case CME_SETSTAGEBAXIS:                                   // SetStageBAxis
    if (mScope->StageBusy() <= 0) {
      if (!mScope->SetStageBAxis(cItemDbl[1])) {
        AbortMacro();
        return;
      }
      mMovedStage = true;
    } else
      mLastIndex = mCurrentIndex;
    break;

  case CME_OPENDECAMERACOVER:                               // OpenDECameraCover
    mOpenDE12Cover = true;
    mWinApp->mDEToolDlg.Update();
    break;

  case CME_VIEW:                                            // View
  case CME_V:
    mCamera->InitiateCapture(VIEW_CONSET);
    mTestScale = true;
    break;

  case CME_FOCUS:                                           // Focus
  case CME_F:
    mCamera->InitiateCapture(FOCUS_CONSET);
    mTestScale = true;
    break;

  case CME_TRIAL:                                           // Trial
  case CME_T:
    mCamera->InitiateCapture(TRIAL_CONSET);
    mTestScale = true;
    break;

  case CME_RECORD:                                          // Record
  case CME_R:
    mCamera->InitiateCapture(RECORD_CONSET);
    mTestScale = true;
    break;

  case CME_PREVIEW:                                         // Preview
  case CME_L:
    mCamera->InitiateCapture(PREVIEW_CONSET);
    mTestScale = true;
    break;

  case CME_SEARCH:                                          // Search
    mCamera->InitiateCapture(SEARCH_CONSET);
    mTestScale = true;
    break;

  case CME_MONTAGE:                                         // Montage
  case CME_M:
  case CME_PRECOOKMONTAGE:                                  // PreCookMontage
    cTruth = CMD_IS(PRECOOKMONTAGE);
    cIndex = MONT_NOT_TRIAL;
    if (cTruth)
      cIndex = MONT_TRIAL_PRECOOK;
    else if (cItemInt[1] != 0)
      cIndex = MONT_TRIAL_IMAGE;
    if (!mWinApp->Montaging())
      ABORT_NOLINE("The script contains a montage statement and \n"
        "montaging is not activated");
    if (mWinApp->mMontageController->StartMontage(cIndex, false,
      (float)(cTruth ? cItemDbl[1] : 0.), (cTruth && !cItemEmpty[2]) ? cItemInt[2] : 0,
      cTruth && cItemInt[3] != 0,
      (cTruth && !cItemEmpty[4]) ? cItemFlt[4] : 0.f))
      AbortMacro();
    mTestMontError = !cTruth;
    mTestScale = !cTruth;
    break;

  case CME_OPPOSITETRIAL:                                   // OppositeTrial
  case CME_OPPOSITEFOCUS:                                   // OppositeFocus
  case CME_OPPOSITEAUTOFOCUS:                               // OppositeAutoFocus
    if (!mWinApp->LowDoseMode())
      ABORT_LINE("Low dose mode needs to be on to use opposite area in statement: \n\n");
    if (CMD_IS(OPPOSITEAUTOFOCUS) && !mWinApp->mFocusManager->FocusReady())
      ABORT_NOLINE("because autofocus not calibrated");
    if (mCamera->OppositeLDAreaNextShot())
      ABORT_LINE("You can not use opposite areas when Balance Shifts is on in "
      "statement: \n\n");
    mTestScale = true;
    if (CMD_IS(OPPOSITEAUTOFOCUS)) {
      mWinApp->mFocusManager->SetUseOppositeLDArea(true);
      cIndex = cItemEmpty[1] ? 1 : cItemInt[1];
      mWinApp->mFocusManager->AutoFocusStart(cIndex);
    } else if (CMD_IS(OPPOSITETRIAL))
      mCamera->InitiateCapture(2);
    else
      mCamera->InitiateCapture(1);
    break;

  case CME_ACQUIRETOMATCHBUFFER:                           // AcquireToMatchBuffer
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIndex2 = mImBufs[cIndex].mConSetUsed;
    cBinning = mImBufs[cIndex].mBinning;
    if (cIndex2 < 0 || cBinning <= 0)
      ABORT_LINE("Either the parameter set or the binning is not specified in the buffer"
        " for:\n\n");
    if (mImBufs[cIndex].mCamera != cCurrentCam)
      ABORT_LINE("Current camera is no longer the same as was used to acquire the image "
        "for:\n\n");
    if (cIndex2 == MONTAGE_CONSET)
      cIndex2 = RECORD_CONSET;
    if (cIndex2 == TRACK_CONSET)
      cIndex2 = TRIAL_CONSET;
    mImBufs[cIndex].mImage->getSize(mCropXafterShot, mCropYafterShot);
    cSizeX = mCropXafterShot;
    cSizeY = mCropYafterShot;
    cV1 = 1.05;

    // Get the parameters to achieve a size that is big enough
    for (;;) {
      mCamera->CenteredSizes(cSizeX, cCamParams->sizeX, cCamParams->moduloX, cIx0, cIx1,
        cSizeY, cCamParams->sizeY, cCamParams->moduloY, cIy0, cIy1, cBinning);
      if (cSizeX >= cCamParams->sizeX / cBinning && cSizeY >= cCamParams->sizeY / cBinning)
        break;
      if (cSizeX >= mCropXafterShot && cSizeY >= mCropYafterShot)
        break;
      if (cSizeX < mCropXafterShot)
        cSizeX = B3DMIN((int)(cV1 * mCropXafterShot), cCamParams->sizeX / cBinning);
      if (cSizeY < mCropYafterShot)
        cSizeY = B3DMIN((int)(cV1 * mCropYafterShot), cCamParams->sizeY / cBinning);
      cV1 *= 1.05;
    }

    // Save the control set on top of stack and make modifications
    mConsetsSaved.push_back(mConSets[cIndex2]);
    mConsetNums.push_back(cIndex2);
    mRestoreConsetAfterShot = true;
    mConSets[cIndex2].binning = cBinning;
    mConSets[cIndex2].left = cIx0 * cBinning;
    mConSets[cIndex2].right = cIx1 * cBinning;
    mConSets[cIndex2].top = cIy0 * cBinning;
    mConSets[cIndex2].bottom = cIy1 * cBinning;
    mConSets[cIndex2].exposure = mImBufs[cIndex].mExposure;
    mConSets[cIndex2].K2ReadMode = mImBufs[cIndex].mK2ReadMode;
    mConSets[cIndex2].mode = SINGLE_FRAME;
    mConSets[cIndex2].alignFrames = 0;
    mConSets[cIndex2].saveFrames = 0;

    // Cancel crop if it isn't needed
    if (cSizeX == mCropXafterShot && cSizeY == mCropYafterShot)
      mCropXafterShot = -1;
    mCamera->InitiateCapture(cIndex2);
    break;

  case CME_STEPFOCUSNEXTSHOT:                              // StepFocusNextShot
    cDelX = 0.;
    cDelY = 0.;
    if (!cItemEmpty[4]) {
      cDelX = cItemDbl[3];
      cDelY = cItemDbl[4];
    }
    if (cItemDbl[1] <= 0. || cDelX < 0. || (cDelX > 0. && cDelX < cItemDbl[1]) ||
      (!cDelX && !cItemEmpty[3]))
      ABORT_LINE("Negative time, times out of order, or odd number of values in "
      "statement: \n\n");
    mCamera->QueueFocusSteps(cItemFlt[1], cItemDbl[2], (float)cDelX, cDelY);
    break;

  case CME_SMOOTHFOCUSNEXTSHOT:                            // SmoothFocusNextShot
    if (fabs(cItemDbl[1] - cItemDbl[2]) < 0.1)
      ABORT_LINE("Focus change must be as least 0.1 micron in statement: \n\n");
    mCamera->QueueFocusSteps(0., cItemDbl[1], 0., cItemDbl[2]);
    break;

  case CME_DEFERSTACKINGNEXTSHOT:                          // DeferStackingNextShot
    if (!IS_BASIC_FALCON2(cCamParams))
      ABORT_NOLINE("Deferred stacking is available only for Falcon2 with old interface");
    mCamera->SetDeferStackingFrames(true);
    break;

  case CME_EARLYRETURNNEXTSHOT:                            // EarlyReturnNextShot
    cIndex = cItemInt[1];
    if (cIndex < 0)
      cIndex = 65535;
    if (mCamera->SetNextAsyncSumFrames(cIndex, cItemInt[2] > 0)) {
      AbortMacro();
      return;
    }
    break;

  case CME_GETDEFERREDSUM:                                 // GetDeferredSum
    if (mCamera->GetDeferredSum()) {
      AbortMacro();
      return;
    }
    break;

  case CME_FRAMETHRESHOLDNEXTSHOT:                         // FrameThresholdNextShot
    mCamera->SetNextFrameSkipThresh(cItemFlt[1]);
    if (!cItemEmpty[2]) {
      cBacklashX = cItemFlt[2];
      cBacklashY = cItemEmpty[3] ? cBacklashX : cItemFlt[3];
      if (cBacklashX >= 1. || cBacklashX < 0. || cBacklashY >= 1. || cBacklashY < 0)
        ABORT_LINE("Partial frame thresholds for Alignframes must be >= 0 and < 1 for "
          "line:\n\n");
      mCamera->SetNextPartialThresholds(cBacklashX, cBacklashY);
    }
    if (!cItemEmpty[4]) {
      if (cItemDbl[4] < 0 || cItemDbl[4] >= 1.)
        ABORT_LINE("The relative frame thresholds must be >= 0 and < 1 for line:\n\n");
      mCamera->SetNextRelativeThresh(cItemFlt[4]);
    }
    break;

  case CME_QUEUEFRAMETILTSERIES:                           // QueueFrameTiltSeries
  case CME_FRAMESERIESFROMVAR:                             // FrameSeriesFromVar
  {
    FloatVec openTime, tiltToAngle, waitOrInterval, focusChange, deltaISX, deltaISY;

    // Get matrix for image shift conversion
    cIndex = mWinApp->LowDoseMode() ? cLdParam[RECORD_CONSET].magIndex :
      mScope->GetMagIndex();
    cAMat = MatMul(MatInv(mShiftManager->CameraToSpecimen(cIndex)),
      mShiftManager->CameraToIS(cIndex));

    // Set up series based on specifications
    if (CMD_IS(QUEUEFRAMETILTSERIES)) {
      cDelISX = cItemEmpty[4] ? 0. : cItemDbl[4];
      for (cIndex = 0; cIndex < cItemInt[3]; cIndex++) {
        tiltToAngle.push_back((float)(cItemDbl[1] + cItemDbl[2] * cIndex));
        if (!cItemEmpty[5])
          openTime.push_back(cItemFlt[5]);
        if (!cItemEmpty[6])
          waitOrInterval.push_back(cItemFlt[6]);
        if (!cItemEmpty[7])
          focusChange.push_back((float)(cItemDbl[7] * cIndex));
        if (!cItemEmpty[9]) {
          deltaISX.push_back((float)((cItemDbl[8] * cAMat.xpx + cItemDbl[9] * cAMat.xpy) *
            cIndex));
          deltaISY.push_back((float)((cItemDbl[8] * cAMat.ypx + cItemDbl[9] * cAMat.ypy) *
            cIndex));
        }
      }

    } else {

      // Or pull the values out of the big variable array: figure out how many per step
      cDelISX = cItemEmpty[3] ? 0. : cItemDbl[3];
      cVar = LookupVariable(cItem1upper, cIndex2);
      if (!cVar)
        ABORT_LINE("The variable " + cStrItems[1] + " is not defined in line:\n\n");
      cSizeX = 0;
      if (cItemInt[2] > 31 || cItemInt[2] < 1)
        ABORT_LINE("The entry with flags must be between 1 and 31 in line:\n\n");
      if (cItemInt[2] & 1)
        cSizeX++;
      if (cItemInt[2] & 2)
        cSizeX++;
      if (cItemInt[2] & 4)
        cSizeX++;
      if (cItemInt[2] & 8)
        cSizeX++;
      if (cItemInt[2] & 16)
        cSizeX += 2;
      if (cVar->rowsFor2d) {
        cSizeY = (int)cVar->rowsFor2d->GetSize();
        for (cIx0 = 0; cIx0 < cSizeY; cIx0++) {
          ArrayRow& tempRow = cVar->rowsFor2d->ElementAt(cIx0);
          if (tempRow.numElements < cSizeX) {
            cReport.Format("Row %d or the 2D array %s has only %d elements, not the %d"
              " required", cIx0, cItem1upper, tempRow.numElements, cSizeX);
            ABORT_NOLINE(cReport);
          }
        }
      } else {
        cSizeY = cVar->numElements / cSizeX;
        cValPtr = &cVar->value;
        if (cVar->numElements % cSizeX) {
          cReport.Format("Variable %s has %d elements, not divisible by the\n"
            "%d values per step implied by the flags entry of %d", cStrItems[1],
            cVar->numElements, cSizeX, cItemInt[2]);
          ABORT_NOLINE(cReport);
        }
      }

      // Load the vectors
      cIy0 = 1;
      for (cIx0 = 0; cIx0 < cSizeY; cIx0++) {
        if (cVar->rowsFor2d) {
          ArrayRow& tempRow = cVar->rowsFor2d->ElementAt(cIx0);
          cValPtr = &tempRow.value;
          cIy0 = 1;
        }
        if (cItemInt[2] & 1) {
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          tiltToAngle.push_back((float)atof((LPCTSTR)cReport));
          cIy0++;
        }
        if (cItemInt[2] & 2) {
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          openTime.push_back((float)atof((LPCTSTR)cReport));
          cIy0++;
        }
        if (cItemInt[2] & 4) {
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          waitOrInterval.push_back((float)atof((LPCTSTR)cReport));
          cIy0++;
        }
        if (cItemInt[2] & 8) {
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          focusChange.push_back((float)atof((LPCTSTR)cReport));
          cIy0++;
        }
        if (cItemInt[2] & 16) {
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          cDelX = atof((LPCTSTR)cReport);
          cIy0++;
          FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
          cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
          cDelY = atof((LPCTSTR)cReport);
          deltaISX.push_back((float)(cDelX * cAMat.xpx + cDelY * cAMat.xpy));
          deltaISY.push_back((float)(cDelX * cAMat.ypx + cDelY * cAMat.ypy));
          cIy0++;
        }
      }
    }

    // Queue it: This does all the error checking
    if (mCamera->QueueTiltSeries(openTime, tiltToAngle, waitOrInterval, focusChange,
      deltaISX, deltaISY, (float)cDelISX)) {
        AbortMacro();
        return;
    }
    break;
  }

  case CME_SETFRAMESERIESPARAMS:                            // SetFrameSeriesParams
    cTruth = cItemInt[1] != 0;
    cBacklashX = 0.;
    if (!cItemEmpty[2])
      cBacklashX = cItemFlt[2];
    if (!cItemEmpty[3] && cItemEmpty[4] ||
      !BOOL_EQUIV(fabs(cItemDbl[3]) > 9000., fabs(cItemDbl[4]) > 9000.))
      ABORT_LINE("A Y position to restore must be entered when X is on line:\n\n");
    if (mCamera->SetFrameTSparams(cTruth, cBacklashX,
      (cItemEmpty[3] || fabs(cItemDbl[3]) > 9000.) ? EXTRA_NO_VALUE : cItemDbl[3],
      (cItemEmpty[4] || fabs(cItemDbl[4]) > 9000.) ? EXTRA_NO_VALUE : cItemDbl[4]))
      ABORT_LINE("The FEI scope plugin version is not new enough to support variable"
        " speed for line:\n\n");
    if (!cItemEmpty[5]) {
      if (cItemInt[5] <= 0 || cItemInt[5] > 99)
        ABORT_LINE("Minimum gap size for recognizing new tilts must be between 1 and 99"
          " for line:\n\n");
      mCamera->SetNextMinTiltGap(cItemInt[5]);
    }
    if (!cItemEmpty[6]) {
      if (cItemInt[6] < 0 || cItemInt[6] > 30)
        ABORT_LINE("Number of frames to drop from tilt sums must be between 0 and 30"
          " for line:\n\n");
      mCamera->SetNextDropFromTiltSum(cItemInt[6]);
    }
    break;

  case CME_WRITEFRAMESERIESANGLES:                          // WriteFrameSeriesAngles
  {
    FloatVec * angles = mCamera->GetFrameTSactualAngles();
    IntVec * startTime = mCamera->GetFrameTSrelStartTime();
    IntVec * endTime = mCamera->GetFrameTSrelEndTime();
    float frame = mCamera->GetFrameTSFrameTime();
    if (!angles->size())
      ABORT_NOLINE("There are no angles available from a frame tilt series");
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    CString message = "Error opening file ";
    CStdioFile * csFile = NULL;
    try {
      csFile = new CStdioFile(cStrCopy, CFile::modeCreate | CFile::modeWrite |
        CFile::shareDenyWrite);
      message = "Writing angles to file ";
      for (cIndex = 0; cIndex < (int)angles->size(); cIndex++) {
        cReport.Format("%7.2f", angles->at(cIndex));
        if (cIndex < (int)startTime->size())
          cStrCopy.Format(" %5d %5d\n", B3DNINT(0.001 * startTime->at(cIndex) / frame),
            B3DNINT(0.001 * endTime->at(cIndex) / frame));
        else
          cStrCopy = "\n";
        csFile->WriteString((LPCTSTR)cReport + cStrCopy);
      }
    }
    catch (CFileException * perr) {
      perr->Delete();
      ABORT_NOLINE(message + cStrCopy);
    }
    delete csFile;
    break;
  }

  case CME_REPORTFRAMETILTSUM:                              // ReportFrameTiltSum
    cTruth = mCamera->GetTiltSumProperties(cIndex, cIndex2, cBacklashX, cIx0, cIx1);
    if (cTruth) {
      cLogRpt = "There are no more tilt sums available";
      SetReportedValues(&cStrItems[1], 1);
    } else {
      cLogRpt.Format("Tilt sum from %.1f deg, index %d, %d frames from %d to %d",
        cBacklashX, cIndex, cIndex2, cIx0, cIx1);
      SetReportedValues(&cStrItems[1], 0, cBacklashX, cIndex, cIndex2, cIx0, cIx1);
    }
    break;

  case CME_MODIFYFRAMETSSHIFTS:                             // ModifyFrameTSShifts
    mCamera->ModifyFrameTSShifts(cItemInt[1], cItemFlt[2], cItemFlt[3]);
    break;

  case CME_REPLACEFRAMETSFOCUS:                             // ReplaceFrameTSFocus
  case CME_REPLACEFRAMETSSHIFTS:                            // ReplaceFrameTSShifts
  {
    FloatVec newFocus, newSecond, *newVec = &newFocus;
    cTruth = CMD_IS(REPLACEFRAMETSSHIFTS);
    for (cIndex = 1; cIndex < (cTruth ? 3 : 2); cIndex++) {
      cStrCopy = cStrItems[cIndex];
      cStrCopy.MakeUpper();
      cVar = LookupVariable(cStrCopy, cIndex2);
      if (!cVar)
        ABORT_LINE("The variable " + cStrItems[cIndex] + " is not defined in line:\n\n");
      if (cVar->rowsFor2d)
        ABORT_LINE("The variable " + cStrItems[cIndex] + " should not be a 2D array for "
          "line:\n\n");
      cValPtr = &cVar->value;
      for (cIy0 = 0; cIy0 < cVar->numElements; cIy0++) {
        FindValueAtIndex(*cValPtr, cIy0, cIx1, cIy1);
        cReport = cValPtr->Mid(cIx1, cIy1 - cIx1);
        newVec->push_back((float)atof((LPCTSTR)cReport));
      }
      if (cTruth)
        newVec = &newSecond;
    }
    if (cTruth)
      cIndex2 = mCamera->ReplaceFrameTSShifts(newFocus, newSecond);
    else
      cIndex2 = mCamera->ReplaceFrameTSFocusChange(newFocus);
    if (cIndex2)
      ABORT_LINE("The variable does not have the same number of focus changes as the"
        " original list of changes for line:\n\n");
    break;
  }

  case CME_RETRACTCAMERA:                                   // RetractCamera

    // Back up index unless camera ready
    if (mCamera->CameraBusy())
      mLastIndex = mCurrentIndex;
    else
      mCamera->RetractAllCameras();
    break;

  case CME_RECORDANDTILTUP:                                 // RecordAndTiltUp
  case CME_RECORDANDTILTDOWN:                               // RecordAndTiltDown
  case CME_RECORDANDTILTTO:                                 // RecordAndTiltTo
    {
      if (!mCamera->PostActionsOK(&mConSets[RECORD_CONSET]))
        ABORT_LINE("Post-exposure actions are not allowed for the current camera"
        " for line:\n\n");
      double increment = mScope->GetIncrement();
      cDoBack = false;
      cDelISX = mScope->GetTiltAngle();
      cIndex = 1;
      if (CMD_IS(RECORDANDTILTTO)) {
        cIndex = 3;
        increment = cItemDbl[1] - cDelISX;
        if (!cItemEmpty[2] && cItemDbl[2]) {
          cDoBack = true;
          cBacklashX = (float)fabs(cItemDbl[2]);
          if (increment < 0)
            cBacklashX = -cBacklashX;
        }
      }
      int delay = mShiftManager->GetAdjustedTiltDelay(increment);
      if (CMD_IS(RECORDANDTILTDOWN)) {
        increment = -increment;
        SetIntensityFactor(-1);
      } else if (CMD_IS(RECORDANDTILTUP)) {
        SetIntensityFactor(1);
      }

      StageMoveInfo smiRAT;
      smiRAT.axisBits = axisA;
      smiRAT.alpha = cDelISX + increment;
      smiRAT.backAlpha = cBacklashX;
      cTruth = SetupStageRestoreAfterTilt(&cStrItems[cIndex], smiRAT.x, smiRAT.y);
      mCamera->QueueStageMove(smiRAT, delay, cDoBack, cTruth);
      mCamera->InitiateCapture(RECORD_CONSET);
      mTestScale = true;
      mMovedStage = true;
      mTestTiltAngle = true;
      break;
    }

  case CME_AREPOSTACTIONSENABLED:                          // ArePostActionsEnabled
    cTruth = mWinApp->ActPostExposure();
    cDoShift = mCamera->PostActionsOK(&mConSets[RECORD_CONSET]);
    cLogRpt.Format("Post-exposure actions %s allowed %sfor this camera%s",
      cTruth ? "ARE" : "are NOT", cDoShift ? "in general " : "",
      cDoShift ? " but ARE for Records currently" : "");
    SetReportedValues(&cStrItems[1], cTruth ? 1. : 0., cDoShift ? 1 : 0.);
    break;

  case CME_TILTDURINGRECORD:                               // TiltDuringRecord
    cDelX = cItemEmpty[3] ? 0. : cItemDbl[3];
    if (mCamera->QueueTiltDuringShot(cItemDbl[1], cItemInt[2], cDelX)) {
      cReport.Format("Moving the stage with variable speed requires\n"
        "FEI plugin version %d (and same for server, if relevant)\n"
        "You only have version %d and this line cannot be used:\n\n",
        FEI_PLUGIN_STAGE_SPEED, mScope->GetPluginVersion());
      ABORT_LINE(cReport);
    }
    mCamera->InitiateCapture(3);
    mMovedStage = true;
    mTestTiltAngle = true;
    break;

  case CME_TESTNEXTMULTISHOT:                              // TestNextMultiShot
    if (cItemInt[1] < 0 || cItemInt[1] > 2)
      ABORT_LINE("The value must be between 0 and 2 in line:\n\n");
    mTestNextMultiShot = cItemInt[1];
    break;

  case CME_MULTIPLERECORDS:                                // MultipleRecords
  {
    if (mNavHelper->mMultiShotDlg)
      mNavHelper->mMultiShotDlg->UpdateAndUseMSparams();
    MultiShotParams * msParams = mNavHelper->GetMultiShotParams();
    cIndex = (cItemEmpty[6] || cItemInt[6] < -8) ? msParams->doEarlyReturn : cItemInt[6];
    if (!cCamParams->K2Type)
      cIndex = 0;
    cIndex2 = msParams->inHoleOrMultiHole | (mTestNextMultiShot << 2);
    if (!cItemEmpty[9] && cItemInt[9] > -9) {
      cIndex2 = cItemInt[9];
      if (cIndex2 < 1 || cIndex2 > 11)
        ABORT_LINE("The ninth entry for doing within holes or\n"
          "in multiple holes must be between 1 and 11 in line:\n\n");
    }
    cTruth = (cIndex == 0 || msParams->numEarlyFrames != 0) ? msParams->saveRecord : false;
    cIx0 = msParams->doSecondRing ? 1 : 0;
    if (!cItemEmpty[10] && cItemInt[10] > -9)
      cIx0 = cItemInt[10] ? 1 : 0;
    cIx1 = 0;
    if (cIx0)
      cIx1 = (cItemEmpty[11] || cItemInt[11] < -8) ? msParams->numShots[1] : cItemInt[11];
    cIy1 = mWinApp->mParticleTasks->StartMultiShot(
      (cItemEmpty[1] || cItemInt[1] < -8) ? msParams->numShots[0] : cItemInt[1],
      (cItemEmpty[2] || cItemInt[2] < -8) ? msParams->doCenter : cItemInt[2],
      (cItemEmpty[3] || cItemDbl[3] < -8.) ? msParams->spokeRad[0] : cItemFlt[3], cIx1,
      (cItemEmpty[12] || cItemDbl[12] < -8.) ? msParams->spokeRad[1] : cItemFlt[12],
      (cItemEmpty[4] || cItemDbl[4] < -8.) ? msParams->extraDelay : cItemFlt[4],
      (cItemEmpty[5] || cItemInt[5] < -8) ? cTruth : cItemInt[5] != 0, cIndex,
      (cItemEmpty[7] || cItemInt[7] < -8) ? msParams->numEarlyFrames : cItemInt[7],
      (cItemEmpty[8] || cItemInt[8] < -8) ? msParams->adjustBeamTilt : cItemInt[8] != 0,
      cIndex2);
    if (cIy1 > 0) {
      AbortMacro();
      return;
    }
    break;
  }

  case CME_ROTATEMULTISHOTPATTERN:                          // RotateMultiShotPattern
    if (mNavHelper->mMultiShotDlg)
      mNavHelper->mMultiShotDlg->UpdateAndUseMSparams();
    cIndex = mNavHelper->RotateMultiShotVectors(mNavHelper->GetMultiShotParams(),
      cItemFlt[1], cItemInt[2] != 0);
    if (cIndex)
      ABORT_LINE(cIndex > 1 ? "No image shift to specimen transformation available "
        "for line:\n\n" : "Selected pattern is not defined for line:/n/n");
    if (mNavHelper->mMultiShotDlg)
      mNavHelper->mMultiShotDlg->UpdateSettings();
    break;

  case CME_AUTOALIGN:                                       // Autoalign
  case CME_A:
  case CME_ALIGNTO:                                         // AlignTo
  case CME_CONICALALIGNTO:                                  // ConicalAlignTo
    cIndex = 0;
    if (CMD_IS(ALIGNTO)  || CMD_IS(CONICALALIGNTO)) {
      if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
        ABORT_LINE(cReport);
    }
    cDelX = 0;
    cTruth = false;
    cDoShift = true;
    if (CMD_IS(CONICALALIGNTO)) {
      cDelX = cItemDbl[2];
      cTruth = cItemInt[3] != 0;
    }
    if (CMD_IS(ALIGNTO) && cItemInt[2])
      cDoShift = false;
    mDisableAlignTrim = CMD_IS(ALIGNTO) && cItemInt[3];
    cIndex2 = mShiftManager->AutoAlign(cIndex, 0, cDoShift, cTruth, NULL, 0., 0., (float)cDelX);
    mDisableAlignTrim = false;
    if (cIndex2)
      SUSPEND_NOLINE("because of failure to autoalign");
    break;

  case CME_LIMITNEXTAUTOALIGN:                              // LimitNextAutoalign
    mShiftManager->SetNextAutoalignLimit(cItemFlt[1]);
    break;

  case CME_ALIGNWITHROTATION:                               // AlignWithRotation
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    if (cItemDbl[3] < 0.2 || cItemDbl[3] > 50.)
      ABORT_LINE("The angle range to search must be between 0.2 and 50 degrees in:\n\n");
    if (mNavHelper->AlignWithRotation(cIndex, cItemFlt[2], cItemFlt[3], cBmin,
      cShiftX, cShiftY))
      ABORT_LINE("Failure to autoalign in:\n\n");
    SetReportedValues(&cStrItems[4], cBmin, cShiftX, cShiftY);
    break;

  case CME_AUTOFOCUS:                                       // AutoFocus
  case CME_G:
    cIndex = 1;
    if (!cItemEmpty[1])
      cIndex = cItemInt[1];
    if (cIndex > -2 && !mWinApp->mFocusManager->FocusReady())
      SUSPEND_NOLINE("because autofocus not calibrated");
    if (mMinDeltaFocus != 0. || mMaxDeltaFocus != 0.)
      mWinApp->mFocusManager->NextFocusChangeLimits(mMinDeltaFocus, mMaxDeltaFocus);
    if (mMinAbsFocus != 0. || mMaxAbsFocus != 0.)
      mWinApp->mFocusManager->NextFocusAbsoluteLimits(mMinAbsFocus, mMaxAbsFocus);
    if (cIndex < -1)
      mWinApp->mFocusManager->DetectFocus(FOCUS_REPORT,
      !cItemEmpty[2] ? cItemInt[2] : 0);
    else
      mWinApp->mFocusManager->AutoFocusStart(cIndex,
      !cItemEmpty[2] ? cItemInt[2] : 0);
    mTestScale = true;
    break;

  case CME_BEAMTILTDIRECTION:                              // BeamTiltDirection
    mWinApp->mFocusManager->SetTiltDirection(cItemInt[1]);
    break;

  case CME_FOCUSCHANGELIMITS:                              // FocusChangeLimits
    mMinDeltaFocus = cItemFlt[1];
    mMaxDeltaFocus = cItemFlt[2];
    break;

  case CME_ABSOLUTEFOCUSLIMITS:                            // AbsoluteFocusLimits
    mMinAbsFocus = cItemDbl[1];
    mMaxAbsFocus = cItemDbl[2];
    break;

  case CME_CORRECTASTIGMATISM:                             // CorrectAstigmatism
    if (mWinApp->mAutoTuning->FixAstigmatism(cItemEmpty[1] || cItemInt[1] >= 0))
      ABORT_NOLINE("There is no astigmatism calibration for the current settings");
    break;

  case CME_CORRECTCOMA:                                    // CorrectComa
    cIndex = COMA_INITIAL_ITERS;
    if (cItemInt[1])
      cIndex = cItemInt[1] > 0 ? COMA_ADD_ONE_ITER : COMA_JUST_MEASURE;
    if (mWinApp->mAutoTuning->ComaFreeAlignment(false, cIndex))
      AbortMacro();
    break;

  case CME_ZEMLINTABLEAU:                                  // ZemlinTableau
    cIndex = 340;
    cIndex2 = 170;
    if (cItemInt[2] > 10)
      cIndex = cItemInt[2];
    if (cItemInt[3] > 0)
      cIndex2 = cItemInt[3];
    mWinApp->mAutoTuning->MakeZemlinTableau(cItemFlt[1], cIndex, cIndex2);
    break;

  case CME_CBASTIGCOMA:                                    // CBAstigComa
    B3DCLAMP(cItemInt[1], 0, 2);
    if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(cItemInt[1], cItemInt[2] != 0,
      cItemInt[3], cItemInt[4] > 0, cItemInt[5] > 0)) {
      AbortMacro();
      return;
    }
    break;

  case CME_FIXASTIGMATISMBYCTF:                             // FixAstigmatismByCTF
  case CME_FIXCOMABYCTF:                                    // FixComaByCTF
    cIndex = 0;
    if (CMD_IS(FIXCOMABYCTF)) {
       cIndex = mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1;
      if (!cItemEmpty[3])
        cIndex = cItemInt[3] > 0 ? 2 : 1;
      cTruth = cItemInt[4] > 0;
    } else
      cTruth = cItemInt[3] > 0;
    if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(cIndex, false,
      (cItemInt[1] > 0) ? 1 : 0, cItemInt[2] > 0, cTruth)){
      AbortMacro();
      return;
    }
    break;

  case CME_REPORTSTIGMATORNEEDED:                           // ReportStigmatorNeeded
    mWinApp->mAutoTuning->GetLastAstigNeeded(cBacklashX, cBacklashY);
    cLogRpt.Format("Last measured stigmator change needed: %.4f  %.4f", cBacklashX,
      cBacklashY);
    SetReportedValues(&cStrItems[1], cBacklashX, cBacklashY);
    break;

  case CME_REPORTCOMATILTNEEDED:                            // ReportComaTiltNeeded
    mWinApp->mAutoTuning->GetLastBeamTiltNeeded(cBacklashX, cBacklashY);
    cLogRpt.Format("Last measured beam tilt change needed: %.2f  %.2f", cBacklashX,
      cBacklashY);
    SetReportedValues(&cStrItems[1], cBacklashX, cBacklashY);
    break;

  case CME_REPORTCOMAVSISMATRIX:                            // ReportComaVsISmatrix
  {
    ComaVsISCalib * cvsis = mWinApp->mAutoTuning->GetComaVsIScal();
    if (cvsis->magInd <= 0)
      ABORT_LINE("There is no calibration of beam tilt versus image shift for line:\n\n");
    cLogRpt.Format("Coma versus IS calibration is %f  %f  %f  %f", cvsis->matrix.xpx,
      cvsis->matrix.xpy, cvsis->matrix.ypx, cvsis->matrix.ypy);
    SetReportedValues(&cStrItems[1], cvsis->matrix.xpx, cvsis->matrix.xpy,
      cvsis->matrix.ypx, cvsis->matrix.ypy);
    break;
  }

  case CME_SAVE:                                            // Save
  case CME_S:
  {
    cIndex2 = -1;
    if (ConvertBufferLetter(cStrItems[1], 0, true, cI, cReport))
      ABORT_LINE(cReport);
    if (!cItemEmpty[2]) {
      cIndex2 = cItemInt[2] - 1;
      if (cIndex2 < 0 || cIndex2 >= mWinApp->mDocWnd->GetNumStores())
        ABORT_LINE("File # to save to is beyond range of open file numbers in "
          "statement: \n\n");
    }

    cReport = cI == 0 ? "A" : cStrItems[1];
    if (cIndex2 < 0 && (mWinApp->Montaging() ||
      !mBufferManager->IsBufferSavable(mImBufs + cI)))
      SUSPEND_NOLINE("because buffer " + cReport + " is not savable to current file");
    if (cItemEmpty[1])
      mWinApp->mDocWnd->SaveRegularBuffer();
    else if (mWinApp->mDocWnd->SaveBufferToFile(cI, cIndex2))
      SUSPEND_LINE("because of error saving to file in statement: \n\n");
    EMimageExtra * extra = (EMimageExtra * )mImBufs[cI].mImage->GetUserData();
    if (extra) {
      if (cIndex2 < 0)
        cReport.Format("Saved Z =%4d, %6.2f degrees", mImBufs[cI].mSecNumber,
          extra->m_fTilt);
      else
        cReport.Format("Saved Z =%4d, %6.2f degrees to file #%d", mImBufs[cI].mSecNumber,
          extra->m_fTilt, cIndex2 + 1);
      mWinApp->AppendToLog(cReport, LOG_SWALLOW_IF_CLOSED);
    }
    break;
  }

  case CME_READFILE:                                        // ReadFile
    cIndex = cItemInt[1];
    cIy0 = mBufferManager->GetBufToReadInto();
    if (!mWinApp->mStoreMRC)
      SUSPEND_NOLINE("on ReadFile because there is no open file");
    if (ConvertBufferLetter(cStrItems[2], cIy0, false, cIx0, cReport))
      ABORT_LINE(cReport);
    mBufferManager->SetBufToReadInto(cIx0);
    if (mWinApp->Montaging())
      cIndex2 = mWinApp->mMontageController->ReadMontage(cIndex, NULL, NULL, false, true);
    else {
      cIndex2 = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, cIndex);
      if (!cIndex2)
        mWinApp->DrawReadInImage();
    }
    mBufferManager->SetBufToReadInto(cIy0);
    if (cIndex2)
      ABORT_NOLINE("Script stopped because of error reading from file");
    break;

  case CME_READOTHERFILE:                                   // ReadOtherFile
    cIndex = cItemInt[1];
    if (cIndex < 0)
      ABORT_LINE("Section to read is negative in line:\n\n");
    cIy0 = mBufferManager->GetBufToReadInto();
    if (cItemInt[2] < 0)
      cIx0 = cIy0;
    else if (ConvertBufferLetter(cStrItems[2], -1, false, cIx0, cReport))
      ABORT_LINE(cReport);
    if (CheckConvertFilename(cStrItems, cStrLine, 3, cReport))
      return;
    for (cIx1 = 0; cIx1 <= mRetryReadOther; cIx1++) {
      mBufferManager->SetOtherFile(cReport);
      mBufferManager->SetBufToReadInto(cIx0);
      mBufferManager->SetNextSecToRead(cIndex);
      cIndex2 = mBufferManager->RereadOtherFile(cStrCopy);
      if (!cIndex2) {
        mWinApp->DrawReadInImage();
        break;
      }
      if (cIx1 < mRetryReadOther)
        Sleep(cReadOtherSleep);
    }
    mBufferManager->SetBufToReadInto(cIy0);
    if (cIndex2)
      ABORT_NOLINE("Script stopped because of error reading from other file:\n" +
      cStrCopy);
    break;

  case CME_RETRYREADOTHERFILE:                              // RetryReadOtherFile
    mRetryReadOther = B3DMAX(0, cItemInt[1]);
    break;

  case CME_SAVETOOTHERFILE:                                 // SaveToOtherFile
  case CME_SNAPSHOTTOFILE:                                  // SnapshotToFile
  {
    ScreenShotParams * snapParams = mWinApp->GetScreenShotParams();
    int compressions[] = {COMPRESS_NONE, COMPRESS_ZIP, COMPRESS_LZW, COMPRESS_JPEG};
    cTruth = CMD_IS(SNAPSHOTTOFILE);
    if (cTruth) {
      cIx0 = 4;
      if (cItemDbl[1] > 0. && cItemDbl[1] < 1.)
        ABORT_LINE("Factor to zoom by must be >= 1 or <= 0 in line:\n\n");
      if (cItemDbl[2] > 0. && cItemDbl[2] < 1.)
        ABORT_LINE("Factor to scale sizes by must be >= 1 or <= 0 in line:\n\n");
    } else {
      cIx0 = 2;
      if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
        ABORT_LINE(cReport);
    }
    cIndex2 = -1;
    if (cStrItems[cIx0] == "MRC")
      cIndex2 = STORE_TYPE_MRC;
    else if (cStrItems[cIx0] == "TIF" || cStrItems[2] == "TIFF")
      cIndex2 = STORE_TYPE_TIFF;
    else if (cStrItems[cIx0] == "JPG" || cStrItems[2] == "JPEG")
      cIndex2 = STORE_TYPE_JPEG;
    else if (cStrItems[cIx0] != "CUR" && cStrItems[2] != "-1")
      ABORT_LINE("Second entry must be MRC, TIF, TIFF, JPG, JPEG, CUR, or -1 in line:"
        "\n\n");
    if (cTruth && cIndex2 == STORE_TYPE_MRC)
      ABORT_LINE("A snapshot cannot be saved to an MRC file in line:\n\n");
    cIx1 = -1;
    if (cStrItems[cIx0 + 1] == "NONE")
      cIx1 = COMPRESS_NONE;
    else if (cStrItems[cIx0 + 1] == "LZW")
      cIx1 = COMPRESS_LZW;
    else if (cStrItems[cIx0 + 1] == "ZIP")
      cIx1 = COMPRESS_ZIP;
    else if (cStrItems[cIx0 + 1] == "JPG" || cStrItems[cIx0 + 1] == "JPEG")
      cIx1 = COMPRESS_JPEG;
    else if (cStrItems[cIx0 + 1] != "CUR" && cStrItems[cIx0 + 1] != "-1")
      ABORT_LINE("Third entry must be NONE, LZW, ZIP, JPG, JPEG, CUR, or -1 in line:"
        "\n\n");
    if (CheckConvertFilename(cStrItems, cStrLine, cIx0 + 2, cReport))
      return;
    if (cTruth) {
      if (cIndex2 < 0)
        cIndex2 = snapParams->fileType ? STORE_TYPE_JPEG : STORE_TYPE_TIFF;
      if (cIx1 < 0) {
        B3DCLAMP(snapParams->compression, 0, 3);
        cIx1 = compressions[snapParams->compression];
      }
      cIy1 = mWinApp->mActiveView->TakeSnapshot(cItemFlt[1], cItemFlt[2],
        cItemInt[3], cIndex2, cIx1, snapParams->jpegQuality, cReport);
      if (CScreenShotDialog::GetSnapshotError(cIy1, cReport)) {
        ABORT_LINE("Error taking snapshot, " + cReport + "for line:\n\n");
      }
    } else {
      cIy1 = mWinApp->mDocWnd->SaveToOtherFile(cIndex, cIndex2, cIx1, &cReport);
      if (cIy1 == 1)
        return;
      if (cIy1) {
        cReport.Format("Error %s file for line:\n\n", cIy1 == 2 ? "opening" : "saving to");
        ABORT_LINE(cReport);
      }
    }
    break;
  }

  case CME_OPENNEWFILE:                                     // OpenNewFile
  case CME_OPENNEWMONTAGE:                                  // OpenNewMontage
  case CME_OPENFRAMESUMFILE:                                // OpenFrameSumFile
    cIndex = 1;
    if (CMD_IS(OPENNEWMONTAGE)) {
      cIndex = 3;
      cIx0 = cItemInt[1];
      if (!cIx0)
        cIx0 = cMontP->xNframes;
      cIy0 = cItemInt[2];
      if (!cIy0)
        cIy0 = cMontP->yNframes;
      if (cIx0 < 1 || cIy0 < 1)
        ABORT_LINE("Number of montages pieces is missing or not positive"
          " in statement:\n\n");
    }
    if (CMD_IS(OPENFRAMESUMFILE)) {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, cIndex, cStrCopy);
      cReport = mCamera->GetFrameFilename() + cStrCopy;
      mWinApp->AppendToLog("Opening file: " + cReport, LOG_SWALLOW_IF_CLOSED);
    } else if (CheckConvertFilename(cStrItems, cStrLine, cIndex, cReport))
      return;

    // Check if file already exists
    if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)cReport, cStatus))
      SUSPEND_NOLINE("opening new file because " + cReport + " already exists");

    if (cIndex == 1)
      cIndex2 = mWinApp->mDocWnd->DoOpenNewFile(cReport);
    else {
      mWinApp->mDocWnd->LeaveCurrentFile();
      cMontP->xNframes = cIx0;
      cMontP->yNframes = cIy0;
      cIndex2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, cIx0, cIy0, cReport);
    }
    if (cIndex2)
      SUSPEND_LINE("because of error opening new file in statement:\n\n");
    mWinApp->mBufferWindow.UpdateSaveCopy();
    break;

  case CME_SETUPWAFFLEMONTAGE:                              // SetupWaffleMontage
    if (cItemInt[1] < 2)
      ABORT_LINE("Minimum number of blocks in waffle grating must be at least 2 in:\n\n");
    cBacklashX = (float)(0.462 * cItemInt[1]);
    cSizeX = mConSets[RECORD_CONSET].right - mConSets[RECORD_CONSET].left;
    cSizeY = mConSets[RECORD_CONSET].bottom - mConSets[RECORD_CONSET].top;
    cIx0 = PiecesForMinimumSize(cBacklashX, cSizeX, 0.1f);
    cIy0 = PiecesForMinimumSize(cBacklashX, cSizeY, 0.1f);
    if (cIx0 < 2 && cIy0 < 2) {
      mWinApp->AppendToLog("No montage is needed to measure pixel size at this "
        "magnification");
      SetReportedValues(0.);
    } else {
      if (mWinApp->Montaging() && cMontP->xNframes == cIx0 && cMontP->yNframes == cIy0) {
        cMontP->magIndex = mScope->GetMagIndex();
        mWinApp->AppendToLog("Existing montage can be used to measure pixel size at "
          "this magnification");
      } else {

                   // If it is montaging already, close file
        if (mWinApp->Montaging())
          mWinApp->mDocWnd->DoCloseFile();

                   // Follow same procedure as above for opening montage file
        if (CheckConvertFilename(cStrItems, cStrLine, 2, cReport))
          return;
        mWinApp->mDocWnd->LeaveCurrentFile();
        cMontP->xNframes = cIx0;
        cMontP->yNframes = cIy0;
        cMontP->binning = mConSets[RECORD_CONSET].binning;
        cMontP->xFrame = cSizeX / cMontP->binning;
        cMontP->yFrame = cSizeY / cMontP->binning;
        cMontP->xOverlap = cMontP->xFrame / 10;
        cMontP->yOverlap = cMontP->yFrame / 10;
        cIndex2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, cIx0, cIy0, cReport);
        mWinApp->mBufferWindow.UpdateSaveCopy();
        if (cIndex2)
          ABORT_NOLINE("Error trying to open new montage the right size for current"
          " magnification");
        PrintfToLog("Opened file %s for %d x %d montage", (LPCTSTR)cReport, cIx0, cIy0);
      }
      cMontP->warnedMagChange = true;
      cMontP->overviewBinning = 1;
      cMontP->shiftInOverview = true;
      mWinApp->mMontageWindow.UpdateSettings();
      SetReportedValues(1.);
    }
    break;

  case CME_REPORTNUMMONTAGEPIECES:                          // ReportNumMontagePieces
    if (mWinApp->Montaging()) {
      cIndex = cMontP->xNframes * cMontP->yNframes -
        (cMontP->ignoreSkipList ? 0 : cMontP->numToSkip);
      cLogRpt.Format("%d pieces will be acquired in the current montage", cIndex);
    } else {
      cIndex = 1;
      cLogRpt = "Montaging is not used for the current file";
    }
    SetReportedValues(&cStrItems[1], cIndex);
    break;

  case CME_ENTERNAMEOPENFILE:                               // EnterNameOpenFile
    cStrCopy = "Enter name for new file:";
    if (!cStrItems[1].IsEmpty())
      mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    if (!KGetOneString(cStrCopy, mEnteredName, 100))
      SUSPEND_NOLINE("because no new file was opened");
    if (mWinApp->mDocWnd->DoOpenNewFile(mEnteredName))
      SUSPEND_NOLINE("because of error opening new file by that name");
    break;

  case CME_CHOOSERFORNEWFILE:                               // ChooserForNewFile
    cIndex = (cItemInt[1] != 0) ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    if (mWinApp->mDocWnd->FilenameForSaveFile(cItemInt[1], NULL, cStrCopy)) {
      AbortMacro();
      return;
    }
    cReport = cStrItems[2];
    cReport.MakeUpper();
    if (SetVariable(cReport, cStrCopy, VARTYPE_REGULAR, -1, false))
      ABORT_NOLINE("Error setting variable " + cStrItems[2] + " with filename " + cStrCopy);
    mOverwriteOK = true;
    break;

  case CME_READTEXTFILE:                                    // ReadTextFile
  case CME_READ2DTEXTFILE:                                  // Read2DTextFile
  case CME_READSTRINGSFROMFILE:                             // ReadStringsFromFile
  {
    SubstituteLineStripItems(cStrLine, 2, cStrCopy);
    CString newVar;
    CStdioFile * csFile = NULL;
    CArray < ArrayRow, ArrayRow > *rowsFor2d = NULL;
    ArrayRow arrRow;
    cTruth = CMD_IS(READ2DTEXTFILE);
    try {
      csFile = new CStdioFile(cStrCopy, CFile::modeRead | CFile::shareDenyWrite);
    }
    catch (CFileException * perr) {
      perr->Delete();
      ABORT_NOLINE("Error opening file " + cStrCopy);
    }
    if (cTruth)
      rowsFor2d = new CArray < ArrayRow, ArrayRow > ;
    while ((cIndex = mWinApp->mParamIO->ReadAndParse(csFile, cReport, cStrItems,
      MAX_MACRO_TOKENS)) == 0) {
      if (cTruth)
        newVar = "";
      if (CMD_IS(READSTRINGSFROMFILE)) {
        if (newVar.GetLength())
          newVar += '\n';
        newVar += cReport;
      } else {
        for (cIndex2 = 0; cIndex2 < MAX_MACRO_TOKENS; cIndex2++) {
          if (cStrItems[cIndex2].IsEmpty())
            break;
          if (newVar.GetLength())
            newVar += '\n';
          newVar += cStrItems[cIndex2];
        }
      }
      if (cTruth && cIndex2 > 0) {
        arrRow.value = newVar;
        arrRow.numElements = cIndex2;
        rowsFor2d->Add(arrRow);
      }
    }
    delete csFile;
    if (cIndex > 0) {
      delete rowsFor2d;
      ABORT_NOLINE("Error reading text from file " + cStrCopy);
    }

    if (SetVariable(cItem1upper, cTruth ? "0" : newVar, VARTYPE_REGULAR, -1, false, &cReport,
      rowsFor2d)) {
      delete rowsFor2d;
      ABORT_NOLINE("Error setting an array variable with text from file " + cStrCopy +
        ":\n" + cReport);
    }
    break;
  }

  case CME_OPENTEXTFILE:                                    // OpenTextFile
  {
    UINT openFlags = CFile::typeText;
    if (LookupFileForText(cItem1upper, TXFILE_MUST_NOT_EXIST, cStrLine, cIndex))
      return;
    cReport = cStrItems[2].MakeUpper();
    if (cReport.GetLength() > 1 || CString("RTWAO").Find(cReport) < 0)
      ABORT_LINE("The second entry must be R, T, W, A, or O in line\n\n:");
    SubstituteLineStripItems(cStrLine, 4, cStrCopy);

    // Set up flags and read/write from mode entry
    if (cReport == "R" || cReport == "T") {
      openFlags |= CFile::shareDenyNone | CFile::modeRead;
      cTruth = true;
    } else {
      cTruth = false;
      openFlags |= CFile::shareDenyWrite;
      if (cReport == "A") {
        openFlags |= CFile::modeReadWrite;
      } else {
        openFlags |= CFile::modeWrite | CFile::modeCreate;
        if (cReport == "W" && !mOverwriteOK && CFile::GetStatus((LPCTSTR)cStrCopy, cStatus))
          ABORT_LINE("File already exists and you entered W instead of O for line:\n\n");
      }
    }

    // Create new entry and try to open file; allowing  failure with 'T'
    cTxFile = new FileForText;
    cTxFile->readOnly = cTruth;
    cTxFile->ID = cItem1upper;
    cTxFile->persistent = cItemInt[3] != 0;
    cTxFile->csFile = NULL;
    try {
      cTxFile->csFile = new CStdioFile(cStrCopy, openFlags);
      if (cReport == "A")
        cTxFile->csFile->SeekToEnd();
    }
    catch (CFileException * perr) {
      perr->Delete();
      delete cTxFile->csFile;
      delete cTxFile;
      if (cReport != "T")
        ABORT_NOLINE("Error opening file " + cStrCopy);
      cTxFile = NULL;
    }

    // Add to array if success; set value for 'T'
    if (cTxFile)
      mTextFileArray.Add(cTxFile);
    if (cReport == "T")
      SetReportedValues(cTxFile ? 1 : 0);
    break;
  }

  case CME_WRITELINETOFILE:                                 // WriteLineToFile
    cTxFile = LookupFileForText(cItem1upper, TXFILE_WRITE_ONLY, cStrLine, cIndex);
    if (!cTxFile)
      return;

    // To allow comments to be written, there is only one required argument in initial
    // check so we need to check here
    SubstituteLineStripItems(cStrLine, 2, cStrCopy);
    if (cStrCopy.IsEmpty())
      ABORT_LINE("You must enter some text after the ID for line:\n\n");
    try {
      cTxFile->csFile->WriteString(cStrCopy + "\n");
    }
    catch (CFileException * perr) {
      perr->Delete();
      ABORT_LINE("Error writing string to file for line:\n\n");
    }
    break;

  case CME_READLINETOARRAY:                                 // ReadLineToArray
  case CME_READLINETOSTRING:                                // ReadLineToString
    cTxFile = LookupFileForText(cItem1upper, TXFILE_READ_ONLY, cStrLine, cIndex);
    if (!cTxFile)
      return;
    cStrLine = cStrItems[2];
    cTruth = CMD_IS(READLINETOARRAY);

    // Skip blank lines, skip comment lines if reading into array
    for (;;) {
      cIndex = mWinApp->mParamIO->ReadAndParse(cTxFile->csFile, cReport, cStrItems,
        MAX_MACRO_TOKENS, mParseQuotes);
      if (cIndex || !cStrItems[0].IsEmpty() || (!cTruth && !cReport.IsEmpty()))
        break;
    }

    // Check error conditions
    if (cIndex > 0)
      ABORT_LINE("Error reading from file for line:\n\n");
    if (cIndex) {
      mWinApp->AppendToLog("End of file reached for file with ID " + cTxFile->ID,
        mLogAction);
    } else {

      // For array, concatenate items into report; otherwise report is all set
      if (cTruth) {
        cReport = "";
        for (cIndex2 = 0; cIndex2 < MAX_MACRO_TOKENS; cIndex2++) {
          if (cStrItems[cIndex2].IsEmpty())
            break;
          if (cReport.GetLength())
            cReport += '\n';
          cReport += cStrItems[cIndex2];
        }
      }
      if (SetVariable(cStrLine, cReport, VARTYPE_REGULAR, -1, false, &cStrCopy))
        ABORT_NOLINE("Error setting a variable with line from file:\n" + cStrCopy);
    }
    SetReportedValues(cIndex ? 0 : 1);
    break;

  case CME_CLOSETEXTFILE:                                   // CloseTextFile
    if (!LookupFileForText(cItem1upper, TXFILE_MUST_EXIST, cStrLine, cIndex))
      return;
    CloseFileForText(cIndex);
    break;

  case CME_FLUSHTEXTFILE:                                   // FlushTextFile
    cTxFile = LookupFileForText(cItem1upper, TXFILE_MUST_EXIST, cStrLine, cIndex);
    if (!cTxFile)
      return;
    cTxFile->csFile->Flush();
    break;

  case CME_ISTEXTFILEOPEN:                                  // IsTextFileOpen
    cTxFile = LookupFileForText(cItem1upper, TXFILE_QUERY_ONLY, cStrLine, cIndex);
    cTruth = false;
    if (!cTxFile && !cItemEmpty[2]) {

      // Check the file name if the it doesn't match ID
      char fullBuf[_MAX_PATH];
      SubstituteLineStripItems(cStrLine, 2, cStrCopy);
      if (GetFullPathName((LPCTSTR)cStrCopy, _MAX_PATH, fullBuf, NULL) > 0) {
        for (cIndex = 0; cIndex < mTextFileArray.GetSize(); cIndex++) {
          cTxFile = mTextFileArray.GetAt(cIndex);
          if (!(cTxFile->csFile->GetFilePath()).CompareNoCase(fullBuf)) {
            cTruth = true;
            break;
          }
        }
      }
      cLogRpt.Format("Text file with name %s %s open", (LPCTSTR)cStrCopy, cTruth ? "IS" :
        "is NOT");
      SetReportedValues(cTruth ? 1 : 0);
    } else {
      cLogRpt.Format("Text file with identifier %s %s open", (LPCTSTR)cItem1upper,
        cTxFile ? "IS" : "is NOT");
      SetReportedValues(cTxFile ? 1 : 0);
    }
    break;

  case CME_USERSETDIRECTORY:                                // UserSetDirectory
  {
    cStrCopy = "Choose a new current working directory:";
    if (!cStrItems[1].IsEmpty())
      mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    char * cwd = _getcwd(NULL, _MAX_PATH);
    CXFolderDialog dlg(cwd);
    dlg.SetTitle(cStrCopy);
    free(cwd);
    if (dlg.DoModal() == IDOK) {
      if (_chdir((LPCTSTR)dlg.GetPath()))
        SUSPEND_NOLINE("because of failure to change directory to " + dlg.GetPath());
    }
    SetOneReportedValue(dlg.GetPath(), 1);
    break;
  }

  case CME_SETNEWFILETYPE:                                  // SetNewFileType
  {
    FileOptions * fileOpt = mWinApp->mDocWnd->GetFileOpt();
    if (cItemInt[2] != 0)
      fileOpt->montFileType = cItemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    else
      fileOpt->fileType = cItemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    break;
  }

  case CME_OPENOLDFILE:                                     // OpenOldFile
    if (CheckConvertFilename(cStrItems, cStrLine, 1, cReport))
      return;
    cIndex = mWinApp->mDocWnd->OpenOldMrcCFile(&cCfile, cReport, false);
    if (cIndex == MRC_OPEN_NOERR || cIndex == MRC_OPEN_ADOC)
      cIndex = mWinApp->mDocWnd->OpenOldFile(cCfile, cReport, cIndex);
    if (cIndex != MRC_OPEN_NOERR)
      SUSPEND_LINE("because of error opening old file in statement:\n\n");
    break;

  case CME_CLOSEFILE:                                       // CloseFile
    if (mWinApp->mStoreMRC) {
      mWinApp->mDocWnd->DoCloseFile();
    } else if (!cItemEmpty[1]) {
      SUSPEND_LINE("Script suspended on CloseFile because there is no open file");
    }
    break;

  case CME_REMOVEFILE:                                      // RemoveFile
    if (CheckConvertFilename(cStrItems, cStrLine, 1, cReport))
      return;
    try {
      CFile::Remove(cReport);
    }
    catch (CFileException * pEx) {
      pEx->Delete();
      PrintfToLog("WARNING: File %s cannot be removed", (LPCTSTR)cReport);
    }
    break;

  case CME_REPORTCURRENTFILENAME:                           // ReportCurrentFilename
  case CME_REPORTLASTFRAMEFILE:                             // ReportLastFrameFile
  case CME_REPORTNAVFILE:                                   // ReportNavFile
  {
    cTruth = !CMD_IS(REPORTLASTFRAMEFILE);
    if (CMD_IS(REPORTCURRENTFILENAME)) {
      if (!mWinApp->mStoreMRC)
        SUSPEND_LINE("because there is no file open currently for statement:\n\n");
      cReport = mWinApp->mStoreMRC->getFilePath();
      cStrCopy = "Current open image file is: ";
    }
    else if (cTruth) {
      ABORT_NONAV;
      cReport = mWinApp->mNavigator->GetCurrentNavFile();
      if (cReport.IsEmpty())
        ABORT_LINE("There is no Navigator file open for:\n\n");
      cStrCopy = "Current open Navigator file is: ";
    }
    else {
      cReport = mCamera->GetPathForFrames();
      if (cReport.IsEmpty())
        ABORT_LINE("There is no last frame file name available for:\n\n");
      cStrCopy = "Last frame file is: ";
    }
    mWinApp->AppendToLog(cStrCopy + cReport, mLogAction);
    CString root = cReport;
    CString ext;
    if (cItemInt[1] && cTruth)
      UtilSplitExtension(cReport, root, ext);
    SetOneReportedValue(!cTruth ? &cStrItems[1] : NULL, root, 1);

    if (!ext.IsEmpty())
      SetOneReportedValue(ext, 2);
    if (cItemInt[1] && cTruth) {
      UtilSplitPath(root, cReport, ext);
      SetOneReportedValue(cReport, 3);
      SetOneReportedValue(ext, 4);
    }
    break;
  }

  case CME_REPORTFRAMEBASENAME:                             // ReportFrameBaseName
    cStrCopy = mCamera->GetFrameBaseName();
    cIndex = mCamera->GetFrameNameFormat();
    if ((cIndex & FRAME_FILE_ROOT) && !cStrCopy.IsEmpty()) {
      cLogRpt = "The frame base name is " + cStrCopy;
      SetOneReportedValue(&cStrItems[1], 1., 1);
      SetOneReportedValue(&cStrItems[1], cStrCopy, 2);
    } else {
      cLogRpt = "The base name is not being used in frame file names";
      SetOneReportedValue(&cStrItems[1], 0., 1);
    }
    break;

  case CME_GETFILEINWATCHEDDIR:                             // GetFileInWatchedDir
  case CME_RUNSCRIPTINWATCHEDDIR:                           // RunScriptInWatchedDir
  {
    WIN32_FIND_DATA findFileData;
    CStdioFile * sFile = NULL;
    if (CMD_IS(RUNSCRIPTINWATCHEDDIR)) {
      if (mNumTempMacros >= MAX_TEMP_MACROS)
        ABORT_LINE("No free temporary scripts available for line:\n\n");
      if (mNeedClearTempMacro >= 0)
        ABORT_LINE("When running a script from a file, you cannot run another one in"
          " line:\n\n");
    }
    if (CheckConvertFilename(cStrItems, cStrLine, 1, cReport))
      return;

    // If the string has no wild card and it is a directory, add *
    if (cReport.Find('*') < 0) {
      cTruth = CFile::GetStatus((LPCTSTR)cReport, cStatus);
      if (cTruth && (cStatus.m_attribute & CFile::directory))
        cReport += "\\*";
    }
    HANDLE hFind = FindFirstFile((LPCTSTR)cReport, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
      SetReportedValues(0.);
      break;
    }
    cTruth = false;
    do {
      cReport = findFileData.cFileName;

      // If found a file, look for lock file and wait until goes away, or give up and
      // go on to next file if any
      for (cIndex = 0; cIndex < 10; cIndex++) {
        if (!CFile::GetStatus((LPCTSTR)(cReport + ".lock"), cStatus)) {
          cTruth = true;
          break;
        }
        Sleep(20);
      }
      if (cTruth)
        break;

    } while (FindNextFile(hFind, &findFileData) == 0);
    FindClose(hFind);

    // Just set report value if no file. give message if there is one
    if (!cTruth) {
      SetReportedValues(0.);
    } else {
      SetOneReportedValue(1., 1);
      SetOneReportedValue(cReport, 2);
      cLogRpt = "Found file " + cReport;
    }

    if (CMD_IS(GETFILEINWATCHEDDIR))
      break;

    // Run a script: read it in
    cIndex2 = 0;
    cIndex = MAX_MACROS + MAX_ONE_LINE_SCRIPTS + mNumTempMacros++;
    try {
      cStrCopy = "opening file";
      sFile = new CStdioFile(cReport, CFile::modeRead | CFile::shareDenyWrite);
      cStrCopy = "reading file";
      mMacros[cIndex] = "";
      while (sFile->ReadString(cItem1upper)) {
        if (!mMacros[cIndex].IsEmpty())
          mMacros[cIndex] += "\r\n";
        mMacros[cIndex] += cItem1upper;
      }
    }
    catch (CFileException * perr) {
      perr->Delete();
      cIndex2 = 1;
    }
    delete sFile;

    // Delete the file
    try {
      CFile::Remove(cReport);
    }
    catch (CFileException * pEx) {
      pEx->Delete();
      cStrCopy = "removing";
      cIndex2 = 0;
    }
    if (cIndex2)
      ABORT_LINE("Error " + cStrCopy + " " + cReport + " for line:\n\n");
    if (mMacros[cIndex].IsEmpty()) {
      cLogRpt += ", which is empty (nothing to run)";
      break;
    }

    // Set it up like callScript
    mCallIndex[mCallLevel++] = mCurrentIndex;
    mCurrentMacro = cIndex;
    mCallMacro[mCallLevel] = mCurrentMacro;
    mBlockDepths[mCallLevel] = -1;
    mCallFunction[mCallLevel] = NULL;
    mCurrentIndex = 0;
    mLastIndex = -1;
    mNeedClearTempMacro = mCurrentMacro;
    break;
  }

  case CME_ALLOWFILEOVERWRITE:                              // AllowFileOverwrite
    mOverwriteOK = cItemInt[1] != 0;
    break;

  case CME_SETDIRECTORY:                                    // SetDirectory
  case CME_MAKEDIRECTORY:                                   // Make Directory
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    if (cStrCopy.IsEmpty())
      ABORT_LINE("Missing directory name in statement:\n\n");
    if (CMD_IS(SETDIRECTORY)) {
      if (_chdir((LPCTSTR)cStrCopy))
        SUSPEND_NOLINE("because of failure to change directory to " + cStrCopy);
    } else {
      if (CFile::GetStatus((LPCTSTR)cStrCopy, cStatus)) {
        mWinApp->AppendToLog("Not making directory " + cStrCopy + " - it already exists");
      } else {
        if (!_mkdir((LPCTSTR)cStrCopy))
          mWinApp->AppendToLog("Created directory " + cStrCopy);
        else
          SUSPEND_NOLINE("because of failure to create directory " + cStrCopy);
      }
    }
    break;

  case CME_REPORTDIRECTORY:                                 // ReportDirectory
  {
    char * cwd = _getcwd(NULL, _MAX_PATH);
    if (!cwd)
      ABORT_LINE("Could not determine current directory for:\n\n");
    cStrCopy = cwd;
    free(cwd);
    mWinApp->AppendToLog("Current directory is " + cStrCopy, mLogAction);
    SetOneReportedValue(&cStrItems[1], cStrCopy, 1);
    break;
  }

  case CME_MAKEDATETIMEDIR:                                 // MakeDateTimeDir
    cStrCopy = mWinApp->mDocWnd->DateTimeForFrameSaving();
    if (_mkdir((LPCTSTR)cStrCopy))
      SUSPEND_NOLINE("because of failure to create directory " + cStrCopy);
    if (_chdir((LPCTSTR)cStrCopy))
      SUSPEND_NOLINE("because of failure to change directory to " + cStrCopy);
    mWinApp->AppendToLog("Created directory " + cStrCopy);
    break;

  case CME_SWITCHTOFILE:                                    // SwitchToFile
    cIndex = cItemInt[1];
    if (cIndex < 1 || cIndex > mWinApp->mDocWnd->GetNumStores())
      ABORT_LINE("Number of file to switch to is absent or out of range in statement:"
        " \n\n");
    mWinApp->mDocWnd->SetCurrentStore(cIndex - 1);
    break;

  case CME_REPORTFILENUMBER:                                // ReportFileNumber
    cIndex = mWinApp->mDocWnd->GetCurrentStore();
    if (cIndex >= 0) {
      cIndex++;
      cLogRpt.Format("Current open file number is %d", cIndex);
    } else
      cLogRpt = "There is no file open currently";
    SetReportedValues(&cStrItems[1], cIndex);
    break;

  case CME_ADDTOAUTODOC:                                    // AddToAutodoc
  case CME_WRITEAUTODOC:                                    // WriteAutodoc
    if (!mWinApp->mStoreMRC)
      ABORT_LINE("There is no open image file for: \n\n");
    if (mBufferManager->CheckAsyncSaving())
      ABORT_NOLINE("There was an error writing to file.\n"
      "Not adding to the autodoc because it would go into the wrong section");
    cIndex = mWinApp->mStoreMRC->GetAdocIndex();
    cIndex2 = mWinApp->mStoreMRC->getDepth();
    if (cIndex < 0)
      ABORT_LINE("There is no .mdoc file for the current image file for: \n\n");
    if (AdocGetMutexSetCurrent(cIndex) < 0)
      ABORT_LINE("Error making autodoc be the current one for: \n\n");
    if (CMD_IS(ADDTOAUTODOC)) {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
      mWinApp->mParamIO->ParseString(cStrLine, cStrItems, MAX_MACRO_TOKENS, mParseQuotes);
      if (AdocSetKeyValue(
        cIndex2 ? B3DCHOICE(mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC,
        ADOC_IMAGE, ADOC_ZVALUE) : ADOC_GLOBAL,
        cIndex2 ? cIndex2 - 1 : 0, (LPCTSTR)cStrItems[1], (LPCTSTR)cStrCopy)) {
          AdocReleaseMutex();
          ABORT_LINE("Error adding string to autodoc file for: \n\n");
      }
    } else if (AdocWrite((char * )(LPCTSTR)mWinApp->mStoreMRC->getAdocName()) < 0) {
      AdocReleaseMutex();
      ABORT_NOLINE("Error writing to autodoc file");
    }
    AdocReleaseMutex();
    break;

  case CME_ADDTOFRAMEMDOC:                                  // AddToFrameMdoc
  case CME_WRITEFRAMEMDOC:                                  // WriteFrameMdoc
  {
    const char * frameMsg[] = { "There is no autodoc for frames open in:\n\n",
      "Error selecting frame .mdoc as current autodoc in:\n\n",
      "There is no current section to add to in frame .mdoc for:\n\n",
      "Error adding to frame .mdoc in:\n\n",
      "Error writing to frame .mdoc in:\n\n" };
    if (CMD_IS(ADDTOFRAMEMDOC)) {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
      mWinApp->mParamIO->ParseString(cStrLine, cStrItems, MAX_MACRO_TOKENS, mParseQuotes);
      cIndex = mWinApp->mDocWnd->AddValueToFrameMdoc(cStrItems[1], cStrCopy);
    }
    else {
      cIndex = mWinApp->mDocWnd->WriteFrameMdoc();
    }
    if (cIndex > 0)
      ABORT_LINE(CString(frameMsg[cIndex - 1]));
    break;
  }

  case CME_REPORTFRAMEMDOCOPEN:                             // reportFrameMdocOpen
    cIndex = mWinApp->mDocWnd->GetFrameAdocIndex() >= 0 ? 1 : 0;
    SetReportedValues(&cStrItems[1], cIndex);
    cLogRpt.Format("Autodoc for frames %s open", cIndex ? "IS" : "is NOT");
    break;

  case CME_DEFERWRITINGFRAMEMDOC:                           // DeferWritingFrameMdoc
    mWinApp->mDocWnd->SetDeferWritingFrameMdoc(true);
    break;

  case CME_OPENFRAMEMDOC:                                   // OpenFrameMdoc
    SubstituteVariables(&cStrLine, 1, cStrLine);
    if (mWinApp->mDocWnd->GetFrameAdocIndex() >= 0)
      ABORT_LINE("The frame mdoc file is already open for line:\n\n");
    if (CheckConvertFilename(cStrItems, cStrLine, 1, cReport))
      return;
    if (mWinApp->mDocWnd->DoOpenFrameMdoc(cReport))
      SUSPEND_LINE("because of error opening frame mdoc file in statement:\n\n")
    break;

  case CME_CLOSEFRAMEMDOC:                                  // CloseFrameMdoc
    if (cItemInt[1] && mWinApp->mDocWnd->GetFrameAdocIndex() < 0)
      ABORT_LINE("There is no frame mdoc file open for line:\n\n");
    mWinApp->mDocWnd->DoCloseFrameMdoc();
    break;

  case CME_ADDTONEXTFRAMESTACKMDOC:                         // AddToNextFrameStackMdoc
  case CME_STARTNEXTFRAMESTACKMDOC:                         // StartNextFrameStackMdoc
    cDoBack = CMD_IS(STARTNEXTFRAMESTACKMDOC);
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
    mWinApp->mParamIO->ParseString(cStrLine, cStrItems, MAX_MACRO_TOKENS, mParseQuotes);
    if (mCamera->AddToNextFrameStackMdoc(cStrItems[1], cStrCopy, cDoBack, &cReport))
      ABORT_LINE(cReport + " in:\n\n");
    break;

  case CME_ALIGNWHOLETSONLY:                                // SetAlignWholeTSOnly
    cIndex  = (cItemEmpty[1] || cItemInt[1] != 0) ? 1 : 0;
    if (mCamera->IsConSetSaving(&mConSets[RECORD_CONSET], RECORD_CONSET, cCamParams, false)
      && (mConSets[RECORD_CONSET].alignFrames || !cIndex) &&
      mConSets[RECORD_CONSET].useFrameAlign > 1 && mCamera->GetAlignWholeSeriesInIMOD()) {
        if (cIndex && !mWinApp->mStoreMRC)
          ABORT_LINE("There must be an output file before this command can be used:\n\n");
        if (cIndex && mWinApp->mStoreMRC->GetAdocIndex() < 0)
          ABORT_NOLINE("The output file was not opened with an associated .mdoc\r\n"
            "file, which is required to align whole tilt series in IMOD");
        SaveControlSet(RECORD_CONSET);
        mConSets[RECORD_CONSET].alignFrames = 1 - cIndex;
        mAlignWholeTSOnly = cIndex > 0;
    } else
      cIndex = 0;
    SetReportedValues(cIndex);
    break;

  case CME_WRITECOMFORTSALIGN:                              //  WriteComForTSAlign
    if (mAlignWholeTSOnly) {
      mConSets[RECORD_CONSET].alignFrames = 1;
      if (mCamera->MakeMdocFrameAlignCom(""))
        ABORT_NOLINE("Problem writing com file for aligning whole tilt series");
      mConSets[RECORD_CONSET].alignFrames = 0;
    }
    break;

  case CME_SAVELOGOPENNEW:                                  // SaveLogOpenNew
  case CME_CLOSELOGOPENNEW:                                 // CloseLogOpenNew
    cTruth = CMD_IS(CLOSELOGOPENNEW);
    if (mWinApp->mLogWindow) {
      cReport = mWinApp->mLogWindow->GetSaveFile();
      if (!cReport.IsEmpty())
        mWinApp->mLogWindow->DoSave();
      else if ((!cTruth || cItemEmpty[1] || !cItemInt[1]) &&
        mWinApp->mLogWindow->AskIfSave("closing and opening it again?")) {
        AbortMacro();
        return;
      }
      mWinApp->mLogWindow->SetUnsaved(false);
        mWinApp->mLogWindow->CloseLog();
    }
    mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
    if (!cItemEmpty[1] && !cTruth) {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
      mWinApp->mLogWindow->UpdateSaveFile(true, cReport);
    }
    break;

  case CME_SAVELOG:                                         // SaveLog
    if (!mWinApp->mLogWindow)
      ABORT_LINE("The log window must already be open for line:\n\n");
    if (cItemEmpty[2]) {
      cReport = mWinApp->mLogWindow->GetSaveFile();
      if (!cReport.IsEmpty())
        mWinApp->mLogWindow->DoSave();
      else
        mWinApp->OnFileSavelog();
    } else {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 2, cReport);
      mWinApp->mLogWindow->UpdateSaveFile(true, cReport, true, cItemInt[1] != 0);
      mWinApp->mLogWindow->DoSave();
    }
    break;

  case CME_DEFERLOGUPDATES:                                  // DeferLogUpdates
    mDeferLogUpdates = cItemEmpty[1] || cItemInt[1] != 0;;
    if (!mDeferLogUpdates && mWinApp->mLogWindow)
      mWinApp->mLogWindow->FlushDeferredLines();
    break;

  case CME_SAVECALIBRATIONS:                                 // SaveCalibrations
    if (mWinApp->GetAdministrator())
      mWinApp->mDocWnd->SaveCalibrations();
    else
      mWinApp->AppendToLog("Calibrations NOT saved from script, administrator mode "
       "not enabled");
    break;

  case CME_SETPROPERTY:                                     // SetProperty
    if (mWinApp->mParamIO->MacroSetProperty(cStrItems[1], cItemDbl[2]))
      AbortMacro();
    break;

  case CME_REPORTUSERSETTING:                               // ReportUserSetting
  case CME_REPORTPROPERTY:                                  // ReportProperty
    cTruth = CMD_IS(REPORTPROPERTY);
    cStrCopy = cTruth ? "property" : "user setting";
    if ((!cTruth && mWinApp->mParamIO->MacroGetSetting(cStrItems[1], cDelX)) ||
      (cTruth && mWinApp->mParamIO->MacroGetProperty(cStrItems[1], cDelX)))
      ABORT_LINE(cStrItems[1] + " is not a recognized " + cStrCopy + " or cannot be "
      "accessed by script command in:\n\n");
    SetReportedValues(&cStrItems[2], cDelX);
    cLogRpt.Format("Value of %s %s is %g", (LPCTSTR)cStrCopy, (LPCTSTR)cStrItems[1], cDelX);
    break;

  case CME_SETUSERSETTING:                                  // SetUserSetting
    if (mWinApp->mParamIO->MacroGetSetting(cStrItems[1], cDelX) ||
      mWinApp->mParamIO->MacroSetSetting(cStrItems[1], cItemDbl[2]))
        ABORT_LINE(cStrItems[1] + " is not a recognized setting or cannot be set by "
        "script command in:\n\n");
    mWinApp->UpdateWindowSettings();

    // See if property already saved
    cTruth = false;
    for (cIndex = 0; cIndex < (int)mSavedSettingNames.size(); cIndex++) {
      if (cStrItems[1].CompareNoCase(mSavedSettingNames[cIndex].c_str()) == 0) {
        cTruth = true;
        break;
      }
    }

    // If old value should be saved, either replace existing new value or push old and new
    if (cItemEmpty[3] || !cItemInt[3]) {
      if (cTruth) {
        mNewSettingValues[cIndex] = cItemDbl[2];
      } else {
        mSavedSettingNames.push_back(std::string((LPCTSTR)cStrItems[1]));
        mSavedSettingValues.push_back(cDelX);
        mNewSettingValues.push_back(cItemDbl[2]);
      }
    } else if (cTruth) {

      // Otherwise get rid of a saved value if new value being kept
      mSavedSettingNames.erase(mSavedSettingNames.begin() + cIndex);
      mSavedSettingValues.erase(mSavedSettingValues.begin() + cIndex);
      mNewSettingValues.erase(mNewSettingValues.begin() + cIndex);
    }
    break;

  case CME_COPY:                                            // Copy
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    if (ConvertBufferLetter(cStrItems[2], -1, false, cIndex2, cReport))
      ABORT_LINE(cReport);
    if (mBufferManager->CopyImageBuffer(cIndex, cIndex2))
      SUSPEND_LINE("because of buffer copy failure in statement: \n\n");
    break;

  case CME_SHOW:                                            // Show
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    mWinApp->SetCurrentBuffer(cIndex);
    break;

  case CME_CHANGEFOCUS:                                     // ChangeFocus
    if (cItemDbl[1] == 0.0 || cItemEmpty[1] || fabs(cItemDbl[1]) > 1000.)
      ABORT_LINE("Improper focus change in statement: \n\n");
    mScope->IncDefocus(cItemDbl[1]);
    break;

  case CME_SETDEFOCUS:                                      // SetDefocus
    if (cItemEmpty[1] || fabs(cItemDbl[1]) > 5000.)
      ABORT_LINE("Improper focus setting in statement: \n\n");
    mScope->SetDefocus(cItemDbl[1]);
    break;

  case CME_SETSTANDARDFOCUS:                                // SetStandardFocus
  case CME_SETABSOLUTEFOCUS:                                // SetAbsoluteFocus
    mScope->SetFocus(cItemDbl[1]);
    break;

  case CME_SETEUCENTRICFOCUS:                               // SetEucentricFocus
  {
    double * focTab = mScope->GetLMFocusTable();
    cIndex = mScope->GetMagIndex();
    if (cIndex <= 0 || mWinApp->GetSTEMMode())
      ABORT_NOLINE("You cannot set eucentric focus in diffraction or STEM mode");
    cDelX = focTab[cIndex * 2 + mScope->GetProbeMode()];
    if (cDelX < -900.)
      cDelX = mScope->GetStandardLMFocus(cIndex);
    if (cDelX < -900.)
      ABORT_NOLINE("There is no standard/eucentric focus defined for the current mag"
        " range");
    mScope->SetFocus(cDelX);
    if (focTab[cIndex * 2 + mScope->GetProbeMode()] < -900. && !JEOLscope &&
      cIndex >= mScope->GetLowestMModeMagInd())
      mWinApp->AppendToLog("WARNING: Setting eucentric focus using a calibrated "
        "standard focus\r\n   from the nearest mag, not the current mag");
    break;
  }

  case CME_CALEUCENTRICFOCUS:                               // CalEucentricFocus
    if (mWinApp->mMenuTargets.DoCalibrateStandardLMfocus(true))
      ABORT_NOLINE("You cannot calibrate eucentric focus in diffraction or STEM mode");
    break;

  case CME_INCTARGETDEFOCUS:                                // IncTargetDefocus
    if (fabs(cItemDbl[1]) > 100.)
      ABORT_LINE("Change in target defocus too large in statement: \n\n");
    cDelX = mWinApp->mFocusManager->GetTargetDefocus();
    mWinApp->mFocusManager->SetTargetDefocus((float)(cDelX + cItemDbl[1]));
    mWinApp->mAlignFocusWindow.UpdateSettings();
    break;

  case CME_SETTARGETDEFOCUS:                                // SetTargetDefocus
    if (cItemDbl[1] < -200. || cItemDbl[1] > 50.)
      ABORT_LINE("Target defocus too large in statement: \n\n");
    mWinApp->mFocusManager->SetTargetDefocus(cItemFlt[1]);
    mWinApp->mAlignFocusWindow.UpdateSettings();
    break;

  case CME_REPORTAUTOFOCUSOFFSET:                           // ReportAutofocusOffset
    cDelX = mWinApp->mFocusManager->GetDefocusOffset();
    cStrCopy.Format("Autofocus offset is: %.2f um", cDelX);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_SETAUTOFOCUSOFFSET:                              // SetAutofocusOffset
    if (cItemDbl[1] < -200. || cItemDbl[1] > 200.)
      ABORT_LINE("Autofocus offset too large in statement: \n\n");
    if (mFocusOffsetToRestore < -9000.) {
      mFocusOffsetToRestore = mWinApp->mFocusManager->GetDefocusOffset();
      mNumStatesToRestore++;
    }
    mWinApp->mFocusManager->SetDefocusOffset(cItemFlt[1]);
    break;

  case CME_SETOBJFOCUS:                                     // SetObjFocus
    cIndex = cItemInt[1];
    cDelX = mScope->GetDefocus();
    mScope->SetObjFocus(cIndex);
    cDelY = mScope->GetDefocus();
    cReport.Format("Defocus before = %.4f   after = %.4f", cDelX, cDelY);
    mWinApp->AppendToLog(cReport, LOG_SWALLOW_IF_CLOSED);
    break;

  case CME_SETDIFFRACTIONFOCUS:                             // SetDiffractionFocus
    mScope->SetDiffractionFocus(cItemDbl[1]);
    UpdateLDAreaIfSaved();
    break;

  case CME_RESETDEFOCUS:                                    // ResetDefocus
    if (!mScope->ResetDefocus())
      AbortMacro();
    break;

  case CME_SETMAG:                                          // SetMag
    cIndex = B3DNINT(cItemDbl[1]);
    if (!cItemEmpty[2] && mWinApp->GetSTEMMode()) {
      mScope->SetSTEMMagnification(cItemDbl[1]);
    } else {
      cI = FindIndexForMagValue(cIndex, -1, -2);
      if (!cI)
        ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
      mScope->SetMagIndex(cI);
      UpdateLDAreaIfSaved();
    }
    break;

  case CME_SETMAGINDEX:                                     // SetMagIndex
    cIndex = cItemInt[1];
    if (cIndex <= 0 || cIndex >= MAX_MAGS)
      ABORT_LINE("Invalid magnification index in:\n\n");
    cDelX = cCamParams->GIF ? mMagTab[cIndex].EFTEMmag : mMagTab[cIndex].mag;
    if (cCamParams->STEMcamera)
      cDelX =  mMagTab[cIndex].STEMmag;
    if (!cDelX)
      ABORT_LINE("There is a zero in the magnification table at the index given in:\n\n");
    mScope->SetMagIndex(cIndex);
    UpdateLDAreaIfSaved();
    break;

  case CME_CHANGEMAG:                                       // ChangeMag
  case CME_INCMAGIFFOUNDPIXEL:                              // IncMagIfFoundPixel
    cIndex = mScope->GetMagIndex();
    if (!cIndex)
      SUSPEND_NOLINE("because you cannot use ChangeMag in diffraction mode");
    cIy0 = cCurrentCam;
    cIx0 = B3DNINT(cItemDbl[1]);
    if (!CMD_IS(CHANGEMAG)) {
      cIx0 = cItemDbl[1] < 0 ? - 1 : 1;
      cTruth = mWinApp->mProcessImage->GetFoundPixelSize(cIy0, cIndex) > 0.;
      SetReportedValues(cTruth ? 1. : 0.);
    }
    if (CMD_IS(CHANGEMAG) || cTruth) {
      cIndex2 = mWinApp->FindNextMagForCamera(cIy0, cIndex, cIx0);
      if (cIndex2 < 0)
        ABORT_LINE("Improper mag change in statement: \n\n");
      mScope->SetMagIndex(cIndex2);
    }
    UpdateLDAreaIfSaved();
    break;

  case CME_CHANGEMAGANDINTENSITY:                           // ChangeMagAndIntensity
  case CME_SETMAGANDINTENSITY:                              // SetMagAndIntensity

    // Get starting and ending mag
    cIndex = mScope->GetMagIndex();
    if (!cIndex || mScope->GetSTEMmode())
      SUSPEND_NOLINE("because you cannot use Change/SetMagAndIntensity in diffraction"
      " or STEM mode");
    if (CMD_IS(CHANGEMAGANDINTENSITY)) {
      cIndex2 = mWinApp->FindNextMagForCamera(cCurrentCam, cIndex,
        B3DNINT(cItemDbl[1]));
      if (cItemEmpty[1] || cIndex2 < 1)
        ABORT_LINE("Improper mag change in statement: \n\n");
    } else {
      cI = B3DNINT(cItemDbl[1]);
      cIndex2 = FindIndexForMagValue(cI, -1, -2);
      if (!cIndex2)
        ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
    }

    // Get the intensity and change in intensity
    cDelISX = mScope->GetIntensity();
    cDelISX = mScope->GetC2Percent(mScope->FastSpotSize(), cDelISX);
    cI = cCurrentCam;
    cDelISY = pow((double)mShiftManager->GetPixelSize(cI, cIndex) /
      mShiftManager->GetPixelSize(cI, cIndex2), 2.);
    cI = mWinApp->mBeamAssessor->AssessBeamChange(cDelISY, cDelX, cDelY, -1);
    if (CheckIntensityChangeReturn(cI))
      return;

    // Change the mag then the intensity
    mScope->SetMagIndex(cIndex2);
    if (!cI)
      mScope->DelayedSetIntensity(cDelX, GetTickCount());
    cStrCopy.Format("%s before mag change %.3f%s, remaining factor of change "
      "needed %.3f", mScope->GetC2Name(), cDelISX, mScope->GetC2Units(), cDelY);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(cDelISX, cDelY);
    UpdateLDAreaIfSaved();
    break;

  case CME_SETCAMLENINDEX:                                  // SetCamLenIndex
    cIndex = B3DNINT(cItemDbl[1]);
    if (cItemEmpty[1] || cIndex < 1)
      ABORT_LINE("Improper camera length index in statement: \n\n");
    if (!mScope->SetCamLenIndex(cIndex))
      ABORT_LINE("Error setting camera length index in statement: \n\n");
    UpdateLDAreaIfSaved();
    break;

  case CME_SETSPOTSIZE:                                     // SetSpotSize
    cIndex = B3DNINT(cItemDbl[1]);
    if (cItemEmpty[1] || cIndex < mScope->GetMinSpotSize() ||
      cIndex > mScope->GetNumSpotSizes())
      ABORT_LINE("Improper spot size in statement: \n\n");
    if (!mScope->SetSpotSize(cIndex)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;

  case CME_SETPROBEMODE:                                    // SetProbeMode
    if (cItem1upper.Find("MICRO") == 0 || cItem1upper == "1")
      cIndex = 1;
    else if (cItem1upper.Find("NANO") == 0 || cItem1upper == "0")
      cIndex = 0;
    else
      ABORT_LINE("Probe mode must be 0, 1, nano, or micro in statement: \n\n");
    if (!mScope->SetProbeMode(cIndex)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;

  case CME_DELAY:                                           // Delay
    cDelISX = cItemDbl[1];
    if (!cStrItems[2].CompareNoCase("MSEC"))
      mSleepTime = cDelISX;
    else if (!cStrItems[2].CompareNoCase("SEC"))
      mSleepTime = 1000. * cDelISX;
    else if (!cStrItems[2].CompareNoCase("MIN"))
      mSleepTime = 60000. * cDelISX;
    else if (cDelISX > 60)
      mSleepTime = cDelISX;
    else
      mSleepTime = 1000. * cDelISX;
    mSleepStart = GetTickCount();
    if (mSleepTime > 3000)
      mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DELAY");
    break;

  case CME_WAITFORMIDNIGHT:                                 // WaitForMidnight
  {
    // Get the times before and after the target time and the alternative target
    cDelX = cDelY = 5.;
    if (!cItemEmpty[1])
      cDelX = cItemDbl[1];
    if (!cItemEmpty[2])
      cDelY = cItemDbl[2];
    cIx0 = 24;
    cIx1 = 0;
    if (!cItemEmpty[3] && !cItemEmpty[4]) {
      cIx0 = cItemInt[3];
      cIx1 = cItemInt[4];
    }

    // Compute minutes into the day and the interval from now to the target
    CTime time = CTime::GetCurrentTime();
    cDelISX = time.GetHour() * 60 + time.GetMinute() + time.GetSecond() / 60.;
    cDelISY = cIx0 * 60 + cIx1 - cDelISX;

    // If wthin the window at all, set up the sleep
    if (cDelISY + cDelY > 0 && cDelISY < cDelX) {
      mSleepTime = 60000. * (cDelISY + cDelY);
      mSleepStart = GetTickCount();
      cReport.Format("Sleeping until %.1f minutes after ", cDelY);
      cStrCopy = "midnight";
      if (!cItemEmpty[4])
        cStrCopy.Format("%02d:%02d", cIx0, cIx1);
      mWinApp->AppendToLog(cReport + cStrCopy, mLogAction);
      cStrCopy.MakeUpper();
      cReport.Format(" + %.0f", cDelY);
      mWinApp->SetStatusText(SIMPLE_PANE, "WAIT TO " + cStrCopy);
    }
    break;
  }

  case CME_WAITFORDOSE:                                     // WaitForDose
    mDoseTarget = cItemDbl[1];
    mNumDoseReports = 10;
    if (!cItemEmpty[2])
      mNumDoseReports = B3DMAX(1, cItemInt[2]);
    mWinApp->mScopeStatus.SetWatchDose(true);
    mDoseStart = mWinApp->mScopeStatus.GetFullCumDose();
    mDoseNextReport = mDoseTarget / (mNumDoseReports * 10);
    mDoseTime = GetTickCount();
    mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DOSE");
    break;

  case CME_SCREENUP:                                        // ScreenUp
    mScope->SetScreenPos(spUp);
    mMovedScreen = true;
    break;

  case CME_SCREENDOWN:                                      // ScreenDown
    mScope->SetScreenPos(spDown);
    mMovedScreen = true;
    break;

  case CME_REPORTSCREEN:                                    // ReportScreen
    cIndex = mScope->GetScreenPos() == spDown ? 1 : 0;
    cLogRpt.Format("Screen is %s", cIndex ? "DOWN" : "UP");
    SetReportedValues(&cStrItems[1], cIndex);
    break;

  case CME_REPORTSCREENCURRENT:                             // ReportScreenCurrent
    cDelX = mScope->GetScreenCurrent();
    cLogRpt.Format("Screen current is %.3f nA", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_IMAGESHIFTBYPIXELS:                              // ImageShiftByPixels
    cBInv = mShiftManager->CameraToIS(mScope->GetMagIndex());
    cIx1 = BinDivisorI(cCamParams);
    cIndex = B3DNINT(cItemDbl[1] * cIx1);
    cIndex2 = B3DNINT(cItemDbl[2] * cIx1);

    cDelISX = cBInv.xpx * cIndex + cBInv.xpy * cIndex2;
    cDelISY = cBInv.ypx * cIndex + cBInv.ypy * cIndex2;
    if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, cItemInt[4],
      !cItemEmpty[3], cItemDbl[3], cReport))
      ABORT_LINE(cReport);
    break;

  case CME_IMAGESHIFTBYUNITS:                               // ImageShiftByUnits
    cDelISX = cItemDbl[1];
    cDelISY = cItemDbl[2];
    if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, cItemInt[4],
      !cItemEmpty[3], cItemDbl[3], cReport))
      ABORT_LINE(cReport);

    // Report distance on specimen
    cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
    cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
    cSpecDist = 1000. * sqrt(cDelX * cDelX + cDelY * cDelY);
    cStrCopy.Format("%.1f nm shifted on specimen", cSpecDist);
    mWinApp->AppendToLog(cStrCopy, LOG_OPEN_IF_CLOSED);
    break;

  case CME_IMAGESHIFTBYMICRONS:                             // ImageShiftByMicrons
    cDelX = cItemDbl[1];
    cDelY = cItemDbl[2];
    cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    cBInv = mShiftManager->MatInv(cAMat);
    cDelISX = cBInv.xpx * cDelX + cBInv.xpy * cDelY;
    cDelISY = cBInv.ypx * cDelX + cBInv.ypy * cDelY;
    if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, cItemInt[4],
      !cItemEmpty[3], cItemDbl[3], cReport))
      ABORT_LINE(cReport);
    break;

  case CME_IMAGESHIFTBYSTAGEDIFF:                           // ImageShiftByStageDiff
    mShiftManager->GetStageTiltFactors(cBacklashX, cBacklashY);
    cIndex = mScope->GetMagIndex();
    cAMat = mShiftManager->StageToCamera(cCurrentCam, cIndex);
    cBInv = MatMul(cAMat, mShiftManager->CameraToIS(cIndex));
    mShiftManager->ApplyScaleMatrix(cBInv, cItemFlt[1] * cBacklashX,
      cItemFlt[2] * cBacklashY, cDelISX, cDelISY);
    if (AdjustBTApplyISSetDelay(cDelISX, cDelISY, cItemInt[4],
      !cItemEmpty[3], cItemDbl[3], cReport))
      ABORT_LINE(cReport);
    break;

  case CME_IMAGESHIFTTOLASTMULTIHOLE:                       // ImageShiftToLastMultiHole
    mWinApp->mParticleTasks->GetLastHoleImageShift(cBacklashX, cBacklashY);
    mScope->IncImageShift(cBacklashX, cBacklashY);
    break;

  case CME_SHIFTIMAGEFORDRIFT:                              // ShiftImageForDrift
    mCamera->QueueDriftRate(cItemDbl[1], cItemDbl[2], cItemInt[3] != 0);
    break;

  case CME_SHIFTCALSKIPLENSNORM:                            // ShiftCalSkipLensNorm
    mWinApp->mShiftCalibrator->SetSkipLensNormNextIScal(cItemEmpty[1] || cItemInt[1] != 0);
    break;

  case CME_CALIBRATEIMAGESHIFT:                             // CalibrateImageShift
    cIndex = 0;
    if (cItemInt[1])
      cIndex = -1;
    mWinApp->mShiftCalibrator->CalibrateIS(cIndex, false, true);
    break;

  case CME_REPORTFOCUSDRIFT:                                // ReportFocusDrift
    if (mWinApp->mFocusManager->GetLastDrift(cDelX, cDelY))
      ABORT_LINE("No drift available from last autofocus for statement: \n\n");
    cStrCopy.Format("Last drift in autofocus: %.3f %.3f nm/sec", cDelX, cDelY);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX, cDelY);
    break;

  case CME_TESTSTEMSHIFT:                                   // TestSTEMshift
    mScope->TestSTEMshift(cItemInt[1], cItemInt[2], cItemInt[3]);
    break;

  case CME_QUICKFLYBACK:                                    // QuickFlyback
    cIndex = CString("VFTRP").Find(cStrItems[1].Left(1));
    if (cIndex < 0)
      ABORT_NOLINE("QuickFlyback must be followed by one of V, F, T, R, or P");
    if (!(cCamParams->STEMcamera && cCamParams->FEItype))
      ABORT_NOLINE("QuickFlyback can be run only if the current camera is an FEI STEM"
      " camera")
    mWinApp->mCalibTiming->CalibrateTiming(cIndex, cItemFlt[2], false);
    break;

  case CME_REPORTAUTOFOCUS:                                 // ReportAutoFocus
    cDelX = mWinApp->mFocusManager->GetCurrentDefocus();
    cIndex = mWinApp->mFocusManager->GetLastFailed() ? - 1 : 0;
    cIndex2 = mWinApp->mFocusManager->GetLastAborted();
    if (cIndex2)
      cIndex = cIndex2;
    if (cIndex)
      cStrCopy.Format("Last autofocus FAILED with error type %d", cIndex);
    else
      cStrCopy.Format("Last defocus in autofocus: %.2f um", cDelX);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX, (double)cIndex);
    break;

  case CME_REPORTTARGETDEFOCUS:                             // ReportTargetDefocus
    cDelX = mWinApp->mFocusManager->GetTargetDefocus();
    cStrCopy.Format("Target defocus is: %.2f um", cDelX);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_SETBEAMSHIFT:                                    // SetBeamShift
    cDelX = cItemDbl[1];
    cDelY = cItemDbl[2];
    if (!mScope->SetBeamShift(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    break;

  case CME_MOVEBEAMBYMICRONS:                               // MoveBeamByMicrons
    if (mWinApp->mProcessImage->MoveBeam(NULL, cItemFlt[1], cItemFlt[2]))
      ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
      " line:\n\n");
    break;

  case CME_MOVEBEAMBYFIELDFRACTION:                         // MoveBeamByFieldFraction
    if (mWinApp->mProcessImage->MoveBeamByCameraFraction(cItemFlt[1],
      cItemFlt[2]))
      ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
      " line:\n\n");
    break;

  case CME_SETBEAMTILT:                                     // SetBeamTilt
    if (!mScope->SetBeamTilt(cItemDbl[1], cItemDbl[2])) {
      AbortMacro();
      return;
    }
    break;

  case CME_REPORTBEAMSHIFT:                                 // ReportBeamShift
    if (!mScope->GetBeamShift(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    cStrCopy.Format("Beam shift %.3f %.3f (putative microns)", cDelX, cDelY);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX, cDelY);
    break;

  case CME_REPORTBEAMTILT:                                  // ReportBeamTilt
    if (!mScope->GetBeamTilt(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    cStrCopy.Format("Beam tilt %.3f %.3f", cDelX, cDelY);
    mWinApp->AppendToLog(cStrCopy, mLogAction);
    SetReportedValues(&cStrItems[1], cDelX, cDelY);
    break;

  case CME_SETIMAGESHIFT:                                   // SetImageShift
    cDelX = cItemDbl[1];
    cDelY = cItemDbl[2];
    cTruth = cItemInt[4];
    if (cTruth)
      mScope->GetLDCenteredShift(cDelISX, cDelISY);
    if (!mScope->SetLDCenteredShift(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    if (AdjustBeamTiltIfSelected(cDelX - cDelISX, cDelY - cDelISY, cTruth, cReport))
      ABORT_LINE(cReport);
    if (!cItemEmpty[3] && cItemDbl[3] > 0)
      mShiftManager->SetISTimeOut(cItemFlt[3] * mShiftManager->GetLastISDelay());
    break;

  case CME_ADJUSTBEAMTILTFORIS:                             // AdjustBeamTiltforIS
    if (!cItemEmpty[1] && cItemEmpty[2])
      ABORT_LINE("There must be either no entries or X and Y IS entries for line:\n\n");
    if (!cItemEmpty[2]) {
      cDelISX = cItemDbl[1];
      cDelISY = cItemDbl[2];
    } else
      mScope->GetLDCenteredShift(cDelISX, cDelISY);
    if (AdjustBeamTiltIfSelected(cDelISX, cDelISY, true, cReport))
      ABORT_LINE(cReport);
    break;

  case CME_REPORTIMAGESHIFT:                                // ReportImageShift
    if (!mScope->GetLDCenteredShift(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Image shift %.3f %.3f IS units", cDelX, cDelY);
    cMag = mScope->GetMagIndex();
    cAMat = mShiftManager->IStoCamera(cMag);
    cDelISX = cDelISY = cStageX = cStageY = 0.;
    if (cAMat.xpx) {
      cIndex = BinDivisorI(cCamParams);
      cDelISX = -(cDelX * cAMat.xpx + cDelY * cAMat.xpy) / cIndex;
      cDelISY = -(cDelX * cAMat.ypx + cDelY * cAMat.ypy) / cIndex;
      cH1 = cos(DTOR * mScope->GetTiltAngle());
      cBInv = MatMul(cAMat,
        MatInv(mShiftManager->StageToCamera(cCurrentCam, cMag)));
      cStageX = (cBInv.xpx * cDelX + cBInv.xpy * cDelY) / (HitachiScope ? cH1 : 1.);
      cStageY = (cBInv.ypx * cDelX + cBInv.ypy * cDelY) / (HitachiScope ? 1. : cH1);
      cReport.Format("   %.1f %.1f unbinned pixels; need stage %.3f %.3f if reset", cDelISX,
                    cDelISY, cStageX, cStageY);
      cLogRpt += cReport;
    }
    SetReportedValues(&cStrItems[1], cDelX, cDelY, cDelISX, cDelISY, cStageX, cStageY);
    break;

  case CME_SETOBJECTIVESTIGMATOR:                           // SetObjectiveStigmator
    cDelX = cItemDbl[1];
    cDelY = cItemDbl[2];
    if (!mScope->SetObjectiveStigmator(cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    break;

  case CME_REPORTXLENSDEFLECTOR:                            // ReportXLensDeflector
  case CME_SETXLENSDEFLECTOR:                               // SetXLensDeflector
  case CME_REPORTXLENSFOCUS:                                // ReportXLensFocus,
  case CME_SETXLENSFOCUS:                                   // SetXLensFocus
  {
    const char * defNames[] = {"Shift", "Tilt", "Stigmator"};
    switch (cCmdIndex) {
    case CME_REPORTXLENSDEFLECTOR:
      cIndex = mScope->GetXLensDeflector(cItemInt[1], cDelX, cDelY);
      if (!cIndex) {
        cLogRpt.Format("X Lens %s is %f %f", defNames[cItemInt[1] - 1], cDelX, cDelY);
        SetReportedValues(&cStrItems[1], cDelX, cDelY);
      }
      break;
    case CME_SETXLENSDEFLECTOR:
      cIndex = mScope->SetXLensDeflector(cItemInt[1], cItemDbl[2], cItemDbl[3]);
      break;
    case CME_REPORTXLENSFOCUS:
      cIndex = mScope->GetXLensFocus(cDelX);
      if (!cIndex) {
        cLogRpt.Format("X Lens focus is %f", cDelX);
        SetReportedValues(&cStrItems[1], cDelX);
      }
      break;
    case CME_SETXLENSFOCUS:
      cIndex = mScope->SetXLensFocus(cItemDbl[1]);
      break;
    }
    if (cIndex == 1) {
      ABORT_LINE("Scope is not initialized for:\n\n");
    } else if (cIndex == 2) {
      ABORT_LINE("Plugin is missing needed function for:\n\n");
    } else if (cIndex == 3) {
      ABORT_LINE("Deflector number must be between 1 and 3 in:\n\n");
    } else if (cIndex == 5) {
      ABORT_LINE("There is no connection to adatl COM object for:\n\n");
    } else if (cIndex > 5) {
      ABORT_LINE("X Mode is not available for:\n\n");
    }
    break;
  }

  case CME_REPORTSPECIMENSHIFT:                             // ReportSpecimenShift
    if (!mScope->GetLDCenteredShift(cDelISX, cDelISY)) {
      AbortMacro();
      return;
    }
    cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    if (cAMat.xpx) {
      cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
      cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
      cLogRpt.Format("Image shift is %.3f  %.3f in specimen coordinates",  cDelX, cDelY);
      SetReportedValues(&cStrItems[1], cDelX, cDelY);
    } else {
      mWinApp->AppendToLog("There is no calibration for converting image shift to "
        "specimen coordinates", mLogAction);
    }
    break;

  case CME_REPORTOBJECTIVESTIGMATOR:                        // ReportObjectiveStigmator
   if (!mScope->GetObjectiveStigmator(cDelX, cDelY)) {
     AbortMacro();
     return;
   }
   cLogRpt.Format("Objective stigmator is %.5f %.5f", cDelX, cDelY);
   SetReportedValues(&cStrItems[1], cDelX, cDelY);
   break;

  case CME_SUPPRESSREPORTS:                                 // SuppressReports
    if (!cItemEmpty[1] && !cItemInt[1])
      mLogAction = LOG_OPEN_IF_CLOSED;
    else
      mLogAction = LOG_IGNORE;
    break;

  case CME_ERRORSTOLOG:                                     // ErrorsToLog
    if (!cItemEmpty[1] && !cItemInt[1])
      mLogErrAction = LOG_IGNORE;
    else
      mLogErrAction = LOG_OPEN_IF_CLOSED;
    break;


  case CME_REPORTALIGNSHIFT:                                // ReportAlignShift
  case CME_REPORTSHIFTDIFFFROM:                             // ReportShiftDiffFrom
  case CME_REPORTISFORBUFFERSHIFT:                          // ReportISforBufferShift
    cDelISY = 0.;
    if (CMD_IS(REPORTSHIFTDIFFFROM)) {
      cDelISY = cItemDbl[1];
      if (cItemEmpty[1] || cDelISY <= 0)
        ABORT_LINE("Improper or missing comparison value in statement: \n\n");
    }
    if (mImBufs->mImage) {
      float cShiftX, cShiftY;
      mImBufs->mImage->getShifts(cShiftX, cShiftY);
      cShiftX *= mImBufs->mBinning;
      cShiftY *= -mImBufs->mBinning;
      mAccumShiftX += cShiftX;
      mAccumShiftY += cShiftY;
      cIndex = mScope->GetMagIndex();
      cIndex2 = BinDivisorI(cCamParams);
      if (cDelISY) {
        cDelX = mShiftManager->GetPixelSize(cCurrentCam, cIndex);
        cDelY = cDelX * sqrt(cShiftX * cShiftX + cShiftY * cShiftY);
        cDelISX = 100. * (cDelY / cDelISY - 1.);
        mAccumDiff += (float)(cDelY - cDelISY);
        cDelISY = cDelX * sqrt(mAccumShiftX * mAccumShiftX + mAccumShiftY * mAccumShiftY);
        cStrCopy.Format("%6.1f %% diff, cumulative diff %6.2f  um, total distance %.1f",
          cDelISX, mAccumDiff, cDelISY);
        SetReportedValues(&cStrItems[2], cDelISX, mAccumDiff, cDelISY);
      } else {
        cBInv = mShiftManager->CameraToIS(cIndex);
        cAMat = mShiftManager->IStoSpecimen(cIndex);

                   // Convert to image shift units, then determine distance on specimen
                   // implied by each axis of image shift separately
        cDelISX = cBInv.xpx * cShiftX + cBInv.xpy * cShiftY;
        cDelISY = cBInv.ypx * cShiftX + cBInv.ypy * cShiftY;
        if (CMD_IS(REPORTISFORBUFFERSHIFT)) {
          cStrCopy.Format("Buffer shift corresponds to %.3f %.3f-  IS units", -cDelISX,
            - cDelISY);
          SetReportedValues(&cStrItems[1], -cDelISX, -cDelISY);
        } else {
          cH1 = 1000. * (cDelISX * cAMat.xpx + cDelISY * cAMat.xpy);
          cV1 = 1000. * (cDelISX * cAMat.ypx + cDelISY * cAMat.ypy);
          cDelX = 1000. * sqrt(pow(cDelISX * cAMat.xpx, 2) + pow(cDelISX * cAMat.ypx, 2));
          cDelY = 1000. * sqrt(pow(cDelISY * cAMat.xpy, 2) + pow(cDelISY * cAMat.ypy, 2));
          cStrCopy.Format("%6.1f %6.1f unbinned pixels; %6.1f %6.1f nm along two shift axes;"
            " %6.1f %6.1f nm on specimen axes",
            cShiftX / cIndex2, cShiftY / cIndex2, cDelX, cDelY, cH1, cV1);
          SetReportedValues(&cStrItems[1], cShiftX, cShiftY, cDelX, cDelY, cH1, cV1);
        }
      }
      mWinApp->AppendToLog(cStrCopy, mLogAction);
    }
    break;

  case CME_REPORTACCUMSHIFT:                                // ReportAccumShift
    cIndex2 = BinDivisorI(cCamParams);
    cLogRpt.Format("%8.1f %8.1f cumulative pixels", mAccumShiftX / cIndex2,
      mAccumShiftY / cIndex2);
    SetReportedValues(&cStrItems[1], mAccumShiftX, mAccumShiftY);
    break;

  case CME_RESETACCUMSHIFT:                                 // ResetAccumShift
    mAccumShiftX = 0.;
    mAccumShiftY = 0.;
    mAccumDiff = 0.;
    break;

  case CME_REPORTALIGNTRIMMING:                             // ReportAlignTrimming
    mShiftManager->GetLastAlignTrims(cIx0, cIx1, cIy0, cIy1);
    cLogRpt.Format("Total border trimmed in last autoalign in X & Y for A: %d %d   "
      "Reference: %d %d", cIx0, cIx1, cIy0, cIy1);
    SetReportedValues(&cStrItems[1], cIx0, cIx1, cIy0, cIy1);
    break;


    // CameraToISMatrix,  ISToCameraMatrix, CameraToStageMatrix, StageToCameraMatrix,
    // CameraToSpecimenMatrix, SpecimenToCameraMatrix, ISToSpecimenMatrix,
    // SpecimenToISMatrix, ISToStageMatrix, StageToISMatrix,
    // StageToSpecimenMatrix, SpecimenToStageMatrix
  case CME_CAMERATOISMATRIX:
  case CME_ISTOCAMERAMATRIX:
  case CME_CAMERATOSTAGEMATRIX:
  case CME_STAGETOCAMERAMATRIX:
  case CME_CAMERATOSPECIMENMATRIX:
  case CME_SPECIMENTOCAMERAMATRIX:
  case CME_ISTOSPECIMENMATRIX:
  case CME_SPECIMENTOISMATRIX:
  case CME_ISTOSTAGEMATRIX:
  case CME_STAGETOISMATRIX:
  case CME_STAGETOSPECIMENMATRIX:
  case CME_SPECIMENTOSTAGEMATRIX:
    cIndex = cItemInt[1];
    if (cIndex <= 0)
      cIndex = mScope->GetMagIndex();
    if (!cIndex)
      ABORT_LINE("There are no scale matrices for diffraction mode for line:\n\n");
    if (cIndex >= MAX_MAGS)
      ABORT_LINE("The mag index is out of range in line:\n\n");
    cTruth = false;
    switch (cCmdIndex) {

    case CME_CAMERATOISMATRIX:
      cTruth = true;
    case CME_ISTOCAMERAMATRIX:
      cAMat = mShiftManager->IStoGivenCamera(cIndex, cCurrentCam);
      break;
    case CME_CAMERATOSTAGEMATRIX:
      cTruth = true;
    case CME_STAGETOCAMERAMATRIX:
      cAMat = mShiftManager->StageToCamera(cCurrentCam, cIndex);
      break;

    case CME_SPECIMENTOCAMERAMATRIX:
      cTruth = true;
    case CME_CAMERATOSPECIMENMATRIX:
      cAMat = mShiftManager->CameraToSpecimen(cIndex);
      break;

    case CME_SPECIMENTOISMATRIX:
      cTruth = true;
    case CME_ISTOSPECIMENMATRIX:
      cAMat = cAMat = mShiftManager->IStoSpecimen(cIndex, cCurrentCam);
      break;
    case CME_STAGETOISMATRIX:
      cTruth = true;
    case CME_ISTOSTAGEMATRIX:
      cAMat = mShiftManager->StageToCamera(cCurrentCam, cIndex);
      if (cAMat.xpx) {
        cBInv = mShiftManager->StageToCamera(cCurrentCam, cIndex);
        if (cBInv.xpx)
          cAMat = MatMul(cAMat, MatInv(cBInv));
        else
          cAMat.xpx = 0.;
      }
      break;
    case CME_STAGETOSPECIMENMATRIX:
      cTruth = true;
    case CME_SPECIMENTOSTAGEMATRIX:
      cAMat = mShiftManager->SpecimenToStage(1., 1.);
      break;
    }
    if (cTruth && cAMat.xpx)
      cAMat = MatInv(cAMat);
    cReport = cmdList[cCmdIndex].mixedCase;
    cReport.Replace("To", " to ");
    cReport.Replace("Matrix", " matrix");
    if (cAMat.xpx)
      cLogRpt.Format("%s at mag index %d is %.5g  %.5g  %.5g  %.5g", (LPCTSTR)cReport,
        cIndex, cAMat.xpx, cAMat.xpy, cAMat.ypx, cAMat.ypy);
    else
      cLogRpt.Format("%s is not available for mag index %d", (LPCTSTR)cReport, cIndex);
    SetReportedValues(&cStrItems[2], cAMat.xpx, cAMat.xpy, cAMat.ypx, cAMat.ypy);
    break;

  case CME_REPORTCLOCK:                                     // ReportClock
    cDelX = 0.001 * GetTickCount() - 0.001 * mStartClock;
    cLogRpt.Format("%.2f seconds elapsed time", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_RESETCLOCK:                                      // ResetClock
    mStartClock = GetTickCount();
    break;

  case CME_REPORTMINUTETIME:                                // ReportMinuteTime
    cIndex = mWinApp->MinuteTimeStamp();
    if (!cItemEmpty[1] && SetVariable(cStrItems[1], (double)cIndex, VARTYPE_PERSIST, -1,
      false, &cReport))
        ABORT_LINE(cReport + "\n(This command assigns to a persistent variable):\n\n");
    cLogRpt.Format("Absolute minute time = %d", cIndex);
    SetReportedValues(cIndex);
    break;

  case CME_SETCUSTOMTIME:                                   // SetCustomTime
  case CME_REPORTCUSTOMINTERVAL:                            // ReportCustomInterval
  {
    std::string sstr = (LPCTSTR)cItem1upper;
    std::map < std::string, int > ::iterator custIter;
    cIndex = mWinApp->MinuteTimeStamp();
    if (!cItemEmpty[2])
      cIndex = cItemInt[2];
    cIndex2 = (int)mCustomTimeMap.count(sstr);
    if (cIndex2)
      custIter = mCustomTimeMap.find(sstr);
    if (CMD_IS(SETCUSTOMTIME)) {

      // Insert does not replace a value!  You have to get the iterator and assign it
      if (cIndex2)
        custIter->second = cIndex;
      else
        mCustomTimeMap.insert(std::pair < std::string, int > (sstr, cIndex));
      mWinApp->mDocWnd->SetShortTermNotSaved();
    } else {
      if (cIndex2) {
        cIndex -= custIter->second;
        cLogRpt.Format("%d minutes elapsed since custom time %s set", cIndex,
           (LPCTSTR)cStrItems[1]);
      } else {
        cIndex = 2 * MAX_CUSTOM_INTERVAL;
        cLogRpt = "Custom time " + cStrItems[1] + " has not been set";
      }
      SetReportedValues(cIndex);
    }
    break;
  }

  case CME_REPORTTICKTIME:                                  // ReportTickTime
    cDelISX = SEMTickInterval(mWinApp->ProgramStartTime()) / 1000.;
    if (!cItemEmpty[1] && SetVariable(cStrItems[1], cDelISX, VARTYPE_PERSIST, -1,
      false, &cReport))
        ABORT_LINE(cReport + "\n(This command assigns to a persistent variable):\n\n");
    cLogRpt.Format("Tick time from program start = %.3f", cDelISX);
    SetReportedValues(cDelISX);
    break;

  case CME_ELAPSEDTICKTIME:                                 // ElapsedTickTime
    cDelISY = SEMTickInterval(mWinApp->ProgramStartTime());
    cDelISX = SEMTickInterval(cDelISY, cItemDbl[1] * 1000.) / 1000.;
    cLogRpt.Format("Elapsed tick time = %.3f", cDelISX);
    SetReportedValues(&cStrItems[2], cDelISX);
    break;

  case CME_REPORTDATETIME:                                  // ReportDateTime
  {
    CTime ctDateTime = CTime::GetCurrentTime();
    cIndex = 10000 * ctDateTime.GetYear() + 100 * ctDateTime.GetMonth() +
      ctDateTime.GetDay();
    cIndex2 = 100 * ctDateTime.GetHour() + ctDateTime.GetMinute();
    cLogRpt.Format("%d  %04d", cIndex, cIndex2);
    SetReportedValues(&cStrItems[1], cIndex, cIndex2);
    break;
  }

  case CME_MOVESTAGE:                                       // MoveStage
  case CME_MOVESTAGETO:                                     // MoveStageTo
  case CME_MOVESTAGEWITHSPEED:                              // MoveStageWithSpeed
  case CME_TESTRELAXINGSTAGE:                               // TestRelaxingStage
  case CME_STAGESHIFTBYPIXELS:                              // StageShiftByPixels
  case CME_STAGETOLASTMULTIHOLE:                            // StageToLastMultiHole
      cSmi.z = 0.;
      cSmi.alpha = 0.;
      cSmi.axisBits = 0;
      cSmi.useSpeed = false;
      cSmi.backX = cSmi.backY = cSmi.relaxX = cSmi.relaxY = 0.;
      cTruth = CMD_IS(TESTRELAXINGSTAGE);

      // If stage not ready, back up and try again, otherwise do action
      if (mScope->StageBusy() > 0)
        mLastIndex = mCurrentIndex;
      else {
        if (CMD_IS(STAGESHIFTBYPIXELS)) {
          cH1 = DTOR * mScope->GetTiltAngle();
          cAMat = mShiftManager->StageToCamera(cCurrentCam,
            mScope->GetMagIndex());
          if (!cAMat.xpx)
            ABORT_LINE("There is no stage to camera calibration available for line:\n\n");
          cBInv = MatInv(cAMat);
          cStageX = cBInv.xpx * cItemDbl[1] + cBInv.xpy * cItemDbl[2];
          cStageY = (cBInv.ypx * cItemDbl[1] + cBInv.ypy * cItemDbl[2]) / cos(cH1);
          cStageZ = 0.;
        } else if (CMD_IS(STAGETOLASTMULTIHOLE)) {
          mWinApp->mParticleTasks->GetLastHoleStagePos(cStageX, cStageY);
          if (cStageX < EXTRA_VALUE_TEST)
            ABORT_LINE("The multiple Record routine has not been run for line:\n\n");
          cStageZ = 0.;
        } else {

          if (cItemEmpty[2])
            ABORT_LINE("Stage movement command does not have at least 2 numbers: \n\n");
          cStageX = cItemDbl[1];
          cStageY = cItemDbl[2];
          if (CMD_IS(MOVESTAGEWITHSPEED)) {
            cStageZ = 0.;
            cSmi.useSpeed = true;
            cSmi.speed = cItemDbl[3];
            if (cSmi.speed <= 0.)
              ABORT_LINE("Speed entry must be positive in line:/n/n");
          } else {
            cStageZ = (cItemEmpty[3] || cTruth) ? 0. : cItemDbl[3];
          }
        }
        if (CMD_IS(MOVESTAGE) || CMD_IS(STAGESHIFTBYPIXELS) || CMD_IS(MOVESTAGEWITHSPEED)
          || cTruth) {
          if (!mScope->GetStagePosition(cSmi.x, cSmi.y, cSmi.z))
            SUSPEND_NOLINE("because of failure to get stage position");
            //CString report;
          //report.Format("Start at %.2f %.2f %.2f", smi.x, smi.y, smi.z);
          //mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);

          // For each of X, Y, Z, set axis bit if nonzero;
          cSmi.x += cStageX;
          if (cStageX != 0.)
            cSmi.axisBits |= axisX;
          cSmi.y += cStageY;
          if (cStageY != 0.)
            cSmi.axisBits |= axisY;
          cSmi.z += cStageZ;
          if (cStageZ != 0.)
            cSmi.axisBits |= axisZ;
          if (cTruth) {
            cBacklashX = cItemEmpty[3] ? mWinApp->mMontageController->GetStageBacklash() :
              cItemFlt[3];
          cBacklashY = cItemEmpty[4] ? mScope->GetStageRelaxation() : cItemFlt[4];
          if (cBacklashY <= 0)
            cBacklashY = 0.025f;

          // Set relaxation in the direction of backlash
          if (cStageX) {
            cSmi.backX = (cStageX > 0. ? cBacklashX : -cBacklashX);
            cSmi.relaxX = (cSmi.backX > 0. ? cBacklashY : -cBacklashY);
          }
          if (cStageY) {
            cSmi.backY = (cStageY > 0. ? cBacklashX : -cBacklashX);
            cSmi.relaxY = (cSmi.backY > 0. ? cBacklashY : -cBacklashY);
          }
          }
        } else {

          // Absolute move: set to values; set Z axis bit if entered
          cSmi.x = cStageX;
          cSmi.y = cStageY;
          cSmi.z = cStageZ;

          cSmi.axisBits |= (axisX | axisY);
          if (!cItemEmpty[3] && !CMD_IS(STAGETOLASTMULTIHOLE))
            cSmi.axisBits |= axisZ;
        }

        // Start the movement
        if (cSmi.axisBits) {
          if (!mScope->MoveStage(cSmi, cTruth && cBacklashX != 0., cSmi.useSpeed, false,
            cTruth))
            SUSPEND_NOLINE("because of failure to start stage movement");
          mMovedStage = true;
        }
      }
    break;

  case CME_RELAXSTAGE:                                      // RelaxStage
    cDelX = cItemEmpty[1] ? mScope->GetStageRelaxation() : cItemDbl[1];
    if (!mScope->GetStagePosition(cSmi.x, cSmi.y, cSmi.z))
      SUSPEND_NOLINE("because of failure to get stage position");
    if (mScope->GetValidXYbacklash(cSmi.x, cSmi.y, cBacklashX, cBacklashY)) {
      mScope->CopyBacklashValid();

      // Move in direction of the backlash, which is opposite to direction of stage move
      cSmi.x += (cBacklashX > 0. ? cDelX : -cDelX);
      cSmi.y += (cBacklashY > 0. ? cDelX : -cDelX);
      cSmi.z = 0.;
      cSmi.alpha = 0.;
      cSmi.axisBits = axisXY;
      mScope->MoveStage(cSmi);
        mMovedStage = true;
    } else {
      mWinApp->AppendToLog("Stage is not in known backlash state so RelaxStage cannot "
        "be done", mLogAction);
    }
    break;

  case CME_BACKGROUNDTILT:                                  // BackgroundTilt
    if (mScope->StageBusy() > 0)
      mLastIndex = mCurrentIndex;
    else {
      cSmi.alpha = cItemDbl[1];
      cDelX = 1.;
      cSmi.useSpeed = false;
      if (!cItemEmpty[2]) {
        cSmi.speed = cItemDbl[2];
        if (cSmi.speed <= 0.)
          ABORT_LINE("Speed entry must be positive in line:/n/n");
        cSmi.useSpeed = true;
      }
      cSmi.axisBits = axisA;
      if (!mScope->MoveStage(cSmi, false, cSmi.useSpeed, true)) {
        AbortMacro();
        return;
      }
    }
    break;

  case CME_REPORTSTAGEXYZ:                                  // ReportStageXYZ
    mScope->GetStagePosition(cStageX, cStageY, cStageZ);
    cLogRpt.Format("Stage %.2f %.2f %.2f", cStageX, cStageY, cStageZ);
    SetReportedValues(&cStrItems[1], cStageX, cStageY, cStageZ);
    break;

  case CME_REPORTTILTANGLE:                                 // ReportTiltAngle
    cDelX = mScope->GetTiltAngle();
    cLogRpt.Format("%.2f degrees", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTSTAGEBUSY:                                 // ReportStageBusy
    cIndex = mScope->StageBusy();
    cLogRpt.Format("Stage is %s", cIndex > 0 ? "BUSY" : "NOT busy");
    SetReportedValues(&cStrItems[1], cIndex > 0 ? 1 : 0);
    break;

  case CME_REPORTSTAGEBAXIS:                                // ReportStageBAxis
    cDelX = mScope->GetStageBAxis();
    cLogRpt.Format("B axis = %.2f degrees", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTMAG:                                       // ReportMag
    cIndex2 = mScope->GetMagIndex();

    // This is not right if the screen is down and FEI is not in EFTEM
    cIndex = MagOrEFTEMmag(mWinApp->GetEFTEMMode(), cIndex2, mScope->GetSTEMmode());
    cLogRpt.Format("Mag is %d%s", cIndex,
      cIndex2 < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
    SetReportedValues(&cStrItems[1], (double)cIndex,
      cIndex2 < mScope->GetLowestNonLMmag() ? 1. : 0.);
    break;

  case CME_REPORTMAGINDEX:                                  // ReportMagIndex
    cIndex = mScope->GetMagIndex();
    cLogRpt.Format("Mag index is %d%s", cIndex,
      cIndex < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
    SetReportedValues(&cStrItems[1], (double)cIndex,
      cIndex < mScope->GetLowestNonLMmag() ? 1. : 0.);
    break;

  case CME_REPORTCAMERALENGTH:                              // ReportCameraLength
    cDelX = 0;
    if (!mScope->GetMagIndex())
      cDelX = mScope->GetLastCameraLength();
    cLogRpt.Format("%s %g%s", cDelX ? "Camera length is " : "Not in diffraction mode - (",
      cDelX, cDelX ? " m" : ")");
    SetReportedValues(&cStrItems[1],  cDelX);
    break;

  case CME_REPORTDEFOCUS:                                   // ReportDefocus
    cDelX = mScope->GetDefocus();
    cLogRpt.Format("Defocus = %.3f um", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTFOCUS:                                     // ReportFocus
  case CME_REPORTABSOLUTEFOCUS:                             // ReportAbsoluteFocus
    cDelX = mScope->GetFocus();
    cLogRpt.Format("Absolute focus = %.5f", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTPERCENTC2:                                 // ReportPercentC2
    cDelY = mScope->GetIntensity();
    cDelX = mScope->GetC2Percent(mScope->FastSpotSize(), cDelY);
    cLogRpt.Format("%s = %.3f%s  -  %.5f", mScope->GetC2Name(), cDelX, mScope->GetC2Units(),
      cDelY);
    SetReportedValues(&cStrItems[1], cDelX, cDelY);
    break;

  case CME_REPORTCROSSOVERPERCENTC2:                        // ReportCrossoverPercentC2
    cIndex = mScope->GetSpotSize();
    cDelY = mScope->GetCrossover(cIndex); // probe mode not required, uses current mode
    cDelX = mScope->GetC2Percent(cIndex, cDelY);
    cLogRpt.Format("Crossover %s at current conditions = %.3f%s  -  %.5f",
      mScope->GetC2Name(), cDelX, mScope->GetC2Units(), cDelY);
    SetReportedValues(&cStrItems[1], cDelX, cDelY);
    break;

  case CME_REPORTILLUMINATEDAREA:                           // ReportIlluminatedArea
    cDelX = mScope->GetIlluminatedArea();
    cLogRpt.Format("IlluminatedArea %f", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTIMAGEDISTANCEOFFSET:                       // ReportImageDistanceOffset
    cDelX = mScope->GetImageDistanceOffset();
    cLogRpt.Format("Image distance offset %f", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_REPORTALPHA:                                     // ReportAlpha
    cIndex = mScope->GetAlpha() + 1;
    cLogRpt.Format("Alpha = %d", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_REPORTSPOTSIZE:                                  // ReportSpotSize
    cIndex = mScope->GetSpotSize();
    cLogRpt.Format("Spot size is %d", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_REPORTPROBEMODE:                                 // ReportProbeMode
    cIndex = mScope->ReadProbeMode();
    cLogRpt.Format("Probe mode is %s", cIndex ? "microprobe" : "nanoprobe");
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_REPORTENERGYFILTER:                              // ReportEnergyFilter
    if (!mWinApp->ScopeHasFilter())
      ABORT_NOLINE("You cannot use ReportEnergyFilter; there is no filter");
    if (mScope->GetHasOmegaFilter())
      mScope->updateEFTEMSpectroscopy(cTruth);
    else if (mCamera->CheckFilterSettings())
      ABORT_NOLINE("An error occurred checking filter settings");
    cDelX = cFiltParam->zeroLoss ? 0. : cFiltParam->energyLoss;
    cLogRpt.Format("Filter slit width %.1f, energy loss %.1f, slit %s",
      cFiltParam->slitWidth, cDelX, cFiltParam->slitIn ? "IN" : "OUT");
    SetReportedValues(&cStrItems[1], cFiltParam->slitWidth, cDelX,
      cFiltParam->slitIn ? 1. : 0.);
    break;

  case CME_REPORTCOLUMNMODE:                                // ReportColumnMode
    if (!mScope->GetColumnMode(cIndex, cIndex2)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Column mode %d (%X), submode %d (%X)", cIndex, cIndex, cIndex2, cIndex2);
    SetReportedValues(&cStrItems[1], (double)cIndex, (double)cIndex2);
    break;

  case CME_REPORTLENS:                                      // ReportLens
    mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
    if (!mScope->GetLensByName(cReport, cDelX)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Lens %s = %f", (LPCTSTR)cReport, cDelX);
    SetReportedValues(cDelX);
    break;

  case CME_REPORTCOIL:                                      // ReportCoil
    if (!mScope->GetDeflectorByName(cStrItems[1], cDelX, cDelY)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Coil %s = %f  %f", (LPCTSTR)cStrItems[1], cDelX, cDelY);
    SetReportedValues(&cStrItems[2], cDelX, cDelY);
    break;

  case CME_SETFREELENSCONTROL:                              // SetFreeLensControl
    if (!mScope->SetFreeLensControl(cItemInt[1], cItemInt[2]))
      ABORT_LINE("Error trying to run:\n\n");
    break;

  case CME_SETLENSWITHFLC:                                  // SetLensWithFLC
    if (!mScope->SetLensWithFLC(cItemInt[1], cItemDbl[2], cItemInt[3] != 0))
      ABORT_LINE("Error trying to run:\n\n");
    break;

  case CME_REPORTLENSFLCSTATUS:                            // ReportLensFLCStatus
    if (!mScope->GetLensFLCStatus(cItemInt[1], cIndex, cDelX))
      ABORT_LINE("Error trying to run:\n\n");
    cLogRpt.Format("Lens %d, FLC %s  value  %f", cItemInt[1], cIndex ? "ON" : "OFF", cDelX);
    SetReportedValues(&cStrItems[2], cIndex, cDelX);
    break;

  case CME_SETJEOLSTEMFLAGS:                                // SetJeolSTEMflags
    if (cItemInt[1] < 0 || cItemInt[1] > 0xFFFFFF || cItemInt[2] < 0 || cItemInt[2] > 15)
        ABORT_LINE("Entries must fit in 24 and 4 bits in: \n\n");
    mCamera->SetBaseJeolSTEMflags(cItemInt[1] + (cItemInt[2] << 24));

    break;

  case CME_REMOVEAPERTURE:                                  // RemoveAperture
  case CME_REINSERTAPERTURE:                                // ReInsertAperture
    cIndex = cItemInt[1];
    if (cStrItems[1] == "CL")
      cIndex = 1;
    if (cStrItems[1] == "OL")
      cIndex = 2;
    if (CMD_IS(REINSERTAPERTURE))
      cIndex2 = mScope->ReInsertAperture(cIndex);
    else
      cIndex2 = mScope->RemoveAperture(cIndex);
    if (cIndex2)
      ABORT_LINE("Script aborted due to error starting aperture movement in:\n\n");
    mMovedAperture = true;
    break;

  case CME_PHASEPLATETONEXTPOS:                             // PhasePlateToNextPos
    if (!mScope->MovePhasePlateToNextPos())
      ABORT_LINE("Script aborted due to error starting phase plate movement in:\n\n");
    mMovedAperture = true;
    break;

  case CME_REPORTPHASEPLATEPOS:                             // ReportPhasePlatePos
    cIndex = mScope->GetCurrentPhasePlatePos();
    if (cIndex < 0)
      ABORT_LINE("Script aborted due to error in:\n\n");
    cLogRpt.Format("Current phase plate position is %d", cIndex + 1);
    SetReportedValues(&cStrItems[1], cIndex + 1.);
    break;

  case CME_REPORTMEANCOUNTS:                                // ReportMeanCounts
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cImBuf = &mImBufs[cIndex];
    cDelX = mWinApp->mProcessImage->WholeImageMean(cImBuf);
    if (cStrItems[2] == "1" && cImBuf->mBinning && cImBuf->mExposure > 0.) {
      cDelX /= cImBuf->mBinning * cImBuf->mBinning * cImBuf->mExposure *
        mWinApp->GetGainFactor(cImBuf->mCamera, cImBuf->mBinning);
      cLogRpt.Format("Mean of buffer %s = %.2f unbinned counts/sec", cStrItems[1], cDelX);
    } else
      cLogRpt.Format("Mean of buffer %s = %.1f", cStrItems[1], cDelX);
    SetReportedValues(cDelX);
    break;

  case CME_REPORTFILEZSIZE:                                 // ReportFileZsize
    if (!mWinApp->mStoreMRC)
      SUSPEND_LINE("because there is no open image file to report for line: \n\n");
    if (mBufferManager->CheckAsyncSaving())
      SUSPEND_NOLINE("because of file write error");
    cIndex = mWinApp->mStoreMRC->getDepth();
    cLogRpt.Format("Z size of file = %d", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_SUBAREAMEAN:                                     // SubareaMean
    cIx0 = cItemInt[1];
    cIx1 = cItemInt[2];
    cIy0 = cItemInt[3];
    cIy1 = cItemInt[4];
    if (mImBufs->mImage)
      mImBufs->mImage->getSize(cSizeX, cSizeY);
    if (!mImBufs->mImage || cItemEmpty[4] || cIx0 < 0 || cIx1 >= cSizeX || cIx1 < cIx0
      || cIy0 < 0 || cIy1 >= cSizeY || cIy1 < cIy0)
        ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
          "in A in statement: \n\n");
    cDelX = ProcImageMean(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
      cSizeX, cSizeY, cIx0, cIx1, cIy0, cIy1);
    cLogRpt.Format("Mean of subarea (%d, %d) to (%d, %d) = %.2f", cIx0, cIy0, cIx1, cIy1,
      cDelX);
    SetReportedValues(&cStrItems[5], cDelX);
    break;

  case CME_CIRCULARSUBAREAMEAN:                             // CircularSubareaMean
    cIx0 = cItemInt[1];
    cIy0 = cItemInt[2];
    cI = cItemInt[3];
    if (mImBufs->mImage)
      mImBufs->mImage->getSize(cSizeX, cSizeY);
    if (!mImBufs->mImage || cItemEmpty[3] || cIx0 - cI < 0 || cIy0 - cI < 0 ||
      cIx0 + cI >= cSizeX || cIy0 + cI >= cSizeY)
      ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
        "in A in statement: \n\n");
    cDelX = ProcImageMeanCircle(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
      cSizeX, cSizeY, cIx0, cIy0, cI);
    cLogRpt.Format("Mean of subarea with radius %d centered around (%d, %d) = %.2f", cI,
      cIx0, cIy0, cDelX);
    SetReportedValues(&cStrItems[4], cDelX);
    break;

  case CME_ELECTRONSTATS:                                   // ElectronStats
  case CME_RAWELECTRONSTATS:                                // RawElectronStats
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cDelX = mImBufs[cIndex].mBinning;
    if (mImBufs[cIndex].mCamera < 0 || !cDelX || !mImBufs[cIndex].mExposure)
      ABORT_LINE("Image buffer does not have enough information for dose statistics in:"
      "\r\n\r\n");
    cCpe = mWinApp->mProcessImage->CountsPerElectronForImBuf(&mImBufs[cIndex]);
    if (!cCpe)
      ABORT_LINE("Camera does not have a CountsPerElectron property for:\r\n\r\n");
    cImage = mImBufs[cIndex].mImage;
    cImage->getSize(cSizeX, cSizeY);
    ProcMinMaxMeanSD(cImage->getData(), cImage->getType(), cSizeX, cSizeY, 0,
      cSizeX - 1, 0, cSizeY - 1, &cBmean, &cBmin, &cBmax, &cBSD);
    cCamParams = mWinApp->GetCamParams() + mImBufs[cIndex].mCamera;
    if (CamHasDoubledBinnings(cCamParams))
      cDelX /= 2;
    cBmin /= cCpe;
    cBmax /= cCpe;
    cBmean /= cCpe;
    cBSD /= cCpe;
    cBacklashX = (float)(cBmean / (mImBufs[cIndex].mExposure * cDelX * cDelX));
    cBacklashY = cBacklashX;
    if (CMD_IS(ELECTRONSTATS)) {
      if (mImBufs[cIndex].mK2ReadMode > 0)
        cBacklashY = mWinApp->mProcessImage->LinearizedDoseRate(mImBufs[cIndex].mCamera,
          cBacklashX);
      if (mImBufs[cIndex].mDoseRatePerUBPix > 0.) {
        SEMTrace('1', "Dose rate computed from mean %.3f  returned from DM %.3f", cBacklashY,
          mImBufs[cIndex].mDoseRatePerUBPix);
        cBacklashY = mImBufs[cIndex].mDoseRatePerUBPix;
      }
    }
    cShiftX = cBacklashY / cBacklashX;
    cLogRpt.Format("Min = %.3f  max = %.3f  mean = %.3f  SD = %.3f electrons/pixel; "
      "dose rate = %.3f e/unbinned pixel/sec", cBmin * cShiftX, cBmax * cShiftX,
      cBmean * cShiftX, cBSD * cShiftX, cBacklashY);
    SetReportedValues(&cStrItems[2], cBmin * cShiftX, cBmax * cShiftX, cBmean * cShiftX,
      cBSD * cShiftX, cBacklashY);
    break;

  case CME_CROPIMAGE:                                       // CropImage
  case CME_CROPCENTERTOSIZE:                                // CropCenterToSize
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    mImBufs[cIndex].mImage->getSize(cSizeX, cSizeY);
    mImBufs[cIndex].mImage->getShifts(cBacklashX, cBacklashY);
    cTruth = CMD_IS(CROPIMAGE);
    if (cTruth) {
      cIx0 = cItemInt[2];
      cIx1 = cItemInt[3];
      cIy0 = cItemInt[4];
      cIy1 = cItemInt[5];

      // Test for whether centered:if not, it needs to be marked as processed
      cTruth = B3DABS(cIx0 + cIx1 + 1 - cSizeX) > 1 || B3DABS(cIy0 + cIy1 + 1 - cSizeY) > 1;
    } else {
      if (cItemInt[2] > cSizeX || cItemInt[3] > cSizeY)
        ABORT_LINE("Image is already smaller than size requested in:\n\n");
      if (cItemInt[2] <= 0 || cItemInt[3] <= 0)
        ABORT_LINE("Size to crop to must be positive in:\n\n");
      cIx0 = (cSizeX - cItemInt[2]) / 2;
      cIy0 = (cSizeY - cItemInt[3]) / 2;
      cIx1 = cIx0 + cItemInt[2] - 1;
      cIy1 = cIy0 + cItemInt[3] - 1;
    }
    cIx0 = mWinApp->mProcessImage->CropImage(&mImBufs[cIndex], cIy0, cIx0, cIy1, cIx1);
    if (cIx0) {
      cReport.Format("Error # %d attempting to crop image in buffer %c in statement: \n\n"
        , cIx0, cStrItems[1].GetAt(0));
      ABORT_LINE(cReport);
    }

    // Mark as cropped if centered and unshifted: this allows autoalign to apply image
    // shift on scope
    mImBufs[cIndex].mCaptured = (cTruth || cBacklashX || cBacklashY) ? BUFFER_PROCESSED :
      BUFFER_CROPPED;
    break;

  case CME_REDUCEIMAGE:                                     // ReduceImage
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIx0 = mWinApp->mProcessImage->ReduceImage(&mImBufs[cIndex], cItemFlt[2],
      &cReport);
    if (cIx0) {
      cReport += " in statement:\n\n";
      ABORT_LINE(cReport);
    }
    break;

  case CME_FFT:                                             // FFT
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIndex2 = 1;
    if (!cItemEmpty[2])
      cIndex2 = cItemInt[2];
    if (cIndex2 < 1 || cIndex2 > 8)
      ABORT_LINE("Binning must be between 1 and 8 in line:\n\n");
    mWinApp->mProcessImage->GetFFT(&mImBufs[cIndex], cIndex2, BUFFER_FFT);
    break;

  case CME_FILTERIMAGE:                                     // FilterImage
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    if (mWinApp->mProcessImage->FilterImage(&mImBufs[cIndex], cItemFlt[2], cItemFlt[5],
      cItemFlt[3], cItemFlt[4]))
      ABORT_LINE("Failed to filter image for line:\n\n");
    break;

  case CME_CTFFIND:                                         // CtfFind
  {
    if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
      ABORT_LINE(cReport);
    CtffindParams param;
    float resultsArray[7];
    if (mWinApp->mProcessImage->InitializeCtffindParams(&mImBufs[cIndex], param))
      ABORT_LINE("Error initializing Ctffind parameters for line:\n\n");
    if (cItemDbl[2] > 0. || cItemDbl[3] > 0. || cItemDbl[2] < -100 || cItemDbl[3] < -100)
      ABORT_LINE("Minimum and maximum defocus must be negative and in microns");
    if (cItemDbl[2] < cItemDbl[3]) {
      cDelX = cItemDbl[2];
      cItemDbl[2] = cItemDbl[3];
      cItemDbl[3] = cDelX;
    }
    param.minimum_defocus = -(float)(10000. * cItemDbl[2]);
    param.maximum_defocus = -(float)(10000. * cItemDbl[3]);
    param.slower_search = cItemEmpty[4] || cItemInt[4] == 0;
    if (cItemInt[5] != 0) {
      if (cItemInt[5] < 128 || cItemInt[5] > 640)
        ABORT_LINE("The box size must be between 128 and 640 in the line:\n\n");
      param.box_size = cItemInt[5];
    }

    // Phase entries.  Convert all from degrees to radians.  The assumed phase shift is
    // in radians and was being converted to degrees and passed that way (12/20/18)
    if (!cItemEmpty[6]) {
      cDelX = cItemDbl[6] * DTOR;
      if (cDelX == 0)
        cDelX = mWinApp->mProcessImage->GetPlatePhase();
      param.minimum_additional_phase_shift = param.maximum_additional_phase_shift =
        (float)(cDelX);
      param.find_additional_phase_shift = true;
      if (!cItemEmpty[7] && (cItemDbl[6] != 0 || cItemDbl[7] != 0)) {
        cDelY = cItemDbl[7] * DTOR;
        if (cDelY < cDelX)
          ABORT_LINE("The maximum phase shift is less than the minimum in line:\n\n");
        if (cDelY - cDelX > 125 * DTOR)
          ABORT_LINE("The range of phase shift to search is more than 125 degrees in "
          "line:\n\n");
        param.maximum_additional_phase_shift = (float)cDelY;
      }
      if (!cItemEmpty[8] && cItemDbl[8])
        param.additional_phase_shift_search_step = (float)(cItemDbl[8] * DTOR);
      if (cItemInt[9]) {
        param.astigmatism_is_known = true;
        param.known_astigmatism = 0.;
        param.known_astigmatism_angle = 0.;
      }
    }
    mWinApp->mProcessImage->SetCtffindParamsForDefocus(param,
      0.8 * cItemDbl[2] + 0.2 * cItemDbl[3], true);
    param.compute_extra_stats = true;
    if (mWinApp->mProcessImage->RunCtffind(&mImBufs[cIndex], param, resultsArray))
      ABORT_LINE("Ctffind fitting returned an error for line:\n\n");
    SetReportedValues(-(resultsArray[0] + resultsArray[1]) / 20000.,
      (resultsArray[0] - resultsArray[1]) / 10000., resultsArray[2],
      resultsArray[3] / DTOR, resultsArray[4], resultsArray[5]);
    break;
  }

  case CME_IMAGEPROPERTIES:                                 // ImageProperties
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    mImBufs[cIndex].mImage->getSize(cSizeX, cSizeY);
    cDelX = 1000. * mWinApp->mShiftManager->GetPixelSize(&mImBufs[cIndex]);
    cLogRpt.Format("Image size %d by %d, binning %s, exposure %.4f",
      cSizeX, cSizeY, (LPCTSTR)mImBufs[cIndex].BinningText(),
      mImBufs[cIndex].mExposure);
    if (cDelX) {
      cStrCopy.Format(", pixel size " + mWinApp->PixelFormat((float)cDelX), (float)cDelX);
      cLogRpt += cStrCopy;
    }
    if (mImBufs[cIndex].mSecNumber < 0) {
      cDelY = -1;
      cStrCopy = ", read in";
    } else {
      cDelY = mImBufs[cIndex].mConSetUsed;
      cStrCopy.Format(", %s parameters", mModeNames[mImBufs[cIndex].mConSetUsed]);
    }
    cLogRpt += cStrCopy;
    if (mImBufs[cIndex].mMapID) {
      cStrCopy.Format(", mapID %d", mImBufs[cIndex].mMapID);
      cLogRpt += cStrCopy;
    }
    SetReportedValues(&cStrItems[2], (double)cSizeX, (double)cSizeY,
      mImBufs[cIndex].mBinning / (double)B3DMAX(1, mImBufs[cIndex].mDivideBinToShow),
      (double)mImBufs[cIndex].mExposure, cDelX, cDelY);
    break;

  case CME_IMAGELOWDOSESET:                                 // ImageLowDoseSet
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIndex2 = mImBufs[cIndex].mConSetUsed;

    if (cIndex2 == MONTAGE_CONSET)
      cIndex2 = RECORD_CONSET;
    if (mImBufs[cIndex].mLowDoseArea && cIndex2 >= 0) {
      cLogRpt = "Image is from " + mModeNames[cIndex2] + " in Low Dose";
    } else {
      if (cIndex2 == TRACK_CONSET) {

        // Try to match View vs R by mag first
        cMag = mImBufs[cIndex].mMagInd;
        if (cLdParam[VIEW_CONSET].magIndex == cMag) {
          cIndex2 = VIEW_CONSET;
        } else if (cLdParam[RECORD_CONSET].magIndex == cMag) {

          // Then decide between preview and record based on exposure
          cDelX = mImBufs[cIndex].mExposure;
          if (fabs(cDelX - mConSets[RECORD_CONSET].exposure) <
            fabs(cDelX - mConSets[PREVIEW_CONSET].exposure))
            cIndex2 = RECORD_CONSET;
          else
            cIndex2 = PREVIEW_CONSET;
        }
        cLogRpt = "Image matches one from " + mModeNames[cIndex2] + " in Low Dose";
      } else if (cIndex2 >= 0) {
        cLogRpt = "Image is from " + mModeNames[cIndex2] + " parameters, not in Low Dose";
      }
    }
    if (cIndex2 > SEARCH_CONSET || cIndex2 < 0) {
      cIndex2 = -1;
      cLogRpt = "Image properties do not match any Low Dose area well enough";
    }
    SetReportedValues(&cStrItems[2], (double)cIndex2,
      mImBufs[cIndex].mLowDoseArea ? 1. : 0.);
    break;

  case CME_MEASUREBEAMSIZE:                                 // MeasureBeamSize
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIx0 = mWinApp->mProcessImage->FindBeamCenter(&mImBufs[cIndex], cBacklashX, cBacklashY,
      cCpe, cBmin, cBmax, cBmean, cBSD, cIndex2, cIx1, cShiftX, cShiftY, cFitErr);
    if (cIx0)
      ABORT_LINE("No beam edges were detected in image for line:\n\n");
    cBmean = mWinApp->mShiftManager->GetPixelSize(&mImBufs[cIndex]);
    if (!cBmean)
      ABORT_LINE("No pixel size is available for the image for line:\n\n");
    cCpe *= 2.f * cIndex2 * cBmean;
    cLogRpt.Format("Beam diameter measured to be %.3f um from %d quadrants, fit error %.3f"
      , cCpe, cIx1, cFitErr);
    SetReportedValues(cCpe, cIx1, cFitErr);
    break;

  case CME_QUADRANTMEANS:                                   // QuadrantMeans
    cDelY = 0.1;
    cDelX = 0.1;
    cIndex = 2;
    if (!cItemEmpty[1])
      cIndex = cItemInt[1];
    if (!cItemEmpty[2])
      cDelX = cItemDbl[2];
    if (!cItemEmpty[3])
      cDelY = cItemDbl[3];
    cImage = mImBufs->mImage;
    if (cImage)
      cImage->getSize(cSizeX, cSizeY);
    if (!cImage || cIndex > B3DMIN(cSizeX, cSizeY) / 4 || cIndex < 0 || cDelX <= 0.
      || cDelY < 0. || cDelX > 0.5 || cDelY > 0.4)
        ABORT_LINE("Parameter out of bounds for image, or no image in A in statement:"
        "\n\n");
    cIx0 = B3DMAX((int)(cDelY * cSizeX / 2), 1);   // Trim length in X
    cIy0 = B3DMAX((int)(cDelY * cSizeY / 2), 1);   // Trim length in Y
    cIx1 = B3DMAX((int)(cDelX * cSizeX / 2), 1);   // Width in X
    cIy1 = B3DMAX((int)(cDelX * cSizeY / 2), 1);   // Width in Y
    cH4 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 + cIndex, cSizeX - 1 - cIx0, cSizeY / 2 + cIndex, cSizeY / 2 + cIndex + cIy1 - 1);
    cV4 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 + cIndex, cSizeX / 2 + cIndex + cIx1 - 1, cSizeY / 2 + cIndex, cSizeY - 1 - cIy0);
    cV3 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 - cIndex - cIx1, cSizeX / 2 - cIndex - 1, cSizeY / 2 + cIndex, cSizeY - 1 - cIy0);
    cH3 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cIx0, cSizeX / 2 - cIndex - 1, cSizeY / 2 + cIndex, cSizeY / 2 + cIndex + cIy1 - 1);
    cH2 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cIx0, cSizeX / 2 - cIndex - 1, cSizeY / 2 - cIndex - cIy1, cSizeY / 2 - cIndex - 1);
    cV2 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 - cIndex - cIx1, cSizeX / 2 - cIndex - 1, cIy0, cSizeY / 2 - cIndex - 1);
    cV1 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 + cIndex, cSizeX / 2 + cIndex + cIx1 - 1, cIy0, cSizeY / 2 - cIndex - 1);
    cH1 = ProcImageMean(cImage->getData(), cImage->getType(), cSizeX, cSizeY,
      cSizeX / 2 + cIndex, cSizeX - 1 - cIx0, cSizeY / 2 - cIndex - cIy1, cSizeY / 2 - cIndex - 1);
    cReport.Format("h1, v1, v2, h2, h3, v3, v4, h4:  %.2f   %.2f   %.2f   %.2f   %.2f   "
      "%.2f   %.2f   %.2f", cH1, cV1, cV2, cH2, cH3, cV3, cV4, cH4);
    mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
    break;

  case CME_CHECKFORBADSTRIPE:                               // CheckForBadStripe
    if (ConvertBufferLetter(cStrItems[1], 0, true, cIndex, cReport))
      ABORT_LINE(cReport);
    cIx0 = cItemInt[2];
    if (cItemEmpty[2]) {
      if (mImBufs[cIndex].mCamera < 0)
        ABORT_LINE("The image has no camera defined and requires an entry for horizontal"
          " versus vertical analysis in:\n\n");
      cIx0 = (mWinApp->GetCamParams() + mImBufs[cIndex].mCamera)->rotationFlip % 2;
    }
    cIndex2 = mWinApp->mProcessImage->CheckForBadStripe(&mImBufs[cIndex], cIx0, cIx1);
    if (!cIndex2)
      cLogRpt = "No bad stripes detected";
    else
      cLogRpt.Format("Bad stripes detected with %d sharp transitions; %d near expected"
        " boundaries", cIndex2, cIx1);
    SetReportedValues(cIndex2, cIx1);
    break;

  case CME_CIRCLEFROMPOINTS:                                // CircleFromPoints
    float radius, xcen, ycen;
    if (cItemEmpty[6])
      ABORT_LINE("Need three sets of X, Y coordinates on line:\n" + cStrLine);
    cH1 = cItemDbl[1];
    cV1 = cItemDbl[2];
    cH2 = cItemDbl[3];
    cV2 = cItemDbl[4];
    cH3 = cItemDbl[5];
    cV3 = cItemDbl[6];
    if (circleThrough3Pts((float)cH1, (float)cV1, (float)cH2, (float)cV2, (float)cH3,
      (float)cV3, &radius, &xcen, &ycen)) {
        mWinApp->AppendToLog("The three points are too close to being on a straight line"
          , LOG_OPEN_IF_CLOSED);
    } else {
      cReport.Format("Circle center = %.2f  %.2f  radius = %.2f", xcen, ycen, radius);
      mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
      SetReportedValues(&cStrItems[7], xcen, ycen, radius);
    }
    break;

  case CME_FINDPIXELSIZE:                                   // FindPixelSize
    mWinApp->mProcessImage->FindPixelSize(0., 0., 0., 0.);
    break;

  case CME_ECHO:                                            // Echo
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
    cReport.Replace("\n", "  ");
    mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
    break;

  case CME_ECHOEVAL:                                        // EchoEval
  case CME_ECHOREPLACELINE:                                 // EchoReplaceLine
  case CME_ECHONOLINEEND:                                   // EchoNoLineEnd
    cReport = "";
    for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !cItemEmpty[cIndex]; cIndex++) {
      if (cIndex > 1)
        cReport += " ";
      cReport += cStrItems[cIndex];
    }
    cReport.Replace("\n", "  ");
    cIndex2 = 0;
    if (CMD_IS(ECHOREPLACELINE))
      cIndex2 = 3;
    else if (CMD_IS(ECHONOLINEEND))
      cIndex2 = 1;
    mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED, cIndex2);
    break;

  case CME_VERBOSE:                                         // verbose
    mVerbose = cItemInt[1];
    break;

  case CME_PROGRAMTIMESTAMPS:                               // ProgramTimeStamps
    mWinApp->AppendToLog(mWinApp->GetStartupMessage(cItemInt[1] != 0));
    break;

    // IsVersionAtLeast, SkipIfVersionLessThan  break;

  case CME_ISVERSIONATLEAST:
  case CME_SKIPIFVERSIONLESSTHAN:
    cIndex2 = cItemEmpty[2] ? 0 : cItemInt[2];
    cIx0 = mWinApp->GetIntegerVersion();
    cIx1 = mWinApp->GetBuildDayStamp();
    cTruth = cItemInt[1] <= cIx0 && cIndex2 <= cIx1;
    if (CMD_IS(ISVERSIONATLEAST)) {
      SetReportedValues(cTruth ? 1. : 0.);
      cLogRpt.Format("Program version is %d date %d, %s than %d %s", cIx0, cIx1,
        cTruth ? "later" : "earlier", cItemInt[1], (LPCTSTR)cStrItems[2]);
    } else if (!cTruth && mCurrentIndex < cMacro->GetLength())
      GetNextLine(cMacro, mCurrentIndex, cStrLine);
    break;

  case CME_PAUSE:                         // Pause, YesNoBox, PauseIfFailed, AbortIfFailed
  case CME_YESNOBOX:
  case CME_PAUSEIFFAILED:
  case CME_ABORTIFFAILED:
      cDoPause = CMD_IS(PAUSEIFFAILED);
      cDoAbort = CMD_IS(ABORTIFFAILED);
      if (!(cDoPause || cDoAbort) || !mLastTestResult) {
        SubstituteVariables(&cStrLine, 1, cStrLine);
        mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
        if (cDoPause || cDoAbort)
          mWinApp->AppendToLog(cReport);
        cDoPause = cDoPause || CMD_IS(PAUSE);
        if (cDoPause) {
          cReport += "\n\nDo you want to proceed with the script?";
          cIndex = AfxMessageBox(cReport, cDoAbort ? MB_EXCLAME : MB_QUESTION);
        } else
          cIndex = SEMMessageBox(cReport, cDoAbort ? MB_EXCLAME : MB_QUESTION);
        if ((cDoPause && cIndex == IDNO) || cDoAbort) {
          SuspendMacro(cDoAbort);
          return;
        } else
          SetReportedValues(cIndex == IDYES ? 1. : 0.);
      }
    break;

  case CME_OKBOX:                                           // OKBox
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
    AfxMessageBox(cReport, MB_OK | MB_ICONINFORMATION);
    break;

  case CME_ENTERONENUMBER:                                  // EnterOneNumber
  case CME_ENTERDEFAULTEDNUMBER:                            // EnterDefaultedNumber
    cBacklashX = 0.;
    cIndex = 1;
    cIndex2 = 3;
    if (CMD_IS(ENTERDEFAULTEDNUMBER)) {

      // Here, enter the value and the number of digits, or < 0 to get an integer entry
      cBacklashX = cItemFlt[1];
      cIndex = 3;
      cIndex2 = cItemInt[2];
      if (cIndex2 < 0)
        cIx0 = cItemInt[1];
    }
    mWinApp->mParamIO->StripItems(cStrLine, cIndex, cStrCopy);
    if (cIndex2 >= 0) {
      cTruth = KGetOneFloat(cStrCopy, cBacklashX, cIndex2);
    } else {
      cTruth = KGetOneInt(cStrCopy, cIx0);
      cBacklashX = (float)cIx0;
    }
    if (!cTruth)
      SUSPEND_NOLINE("because no number was entered");
    cLogRpt.Format("%s: user entered  %g", (LPCTSTR)cStrCopy, cBacklashX);
    SetReportedValues(cBacklashX);
    break;

  case CME_ENTERSTRING:                                     // EnterString
    cStrCopy = "Enter a text string:";
    cReport = "";
    mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
    if (!KGetOneString(cStrCopy, cReport))
      SUSPEND_NOLINE("because no string was entered");
    if (SetVariable(cItem1upper, cReport, VARTYPE_REGULAR, -1, false))
      ABORT_NOLINE("Error setting variable " + cStrItems[1] + " with string " + cReport);
    break;

  case CME_COMPARENOCASE:                                   // CompareStrings
  case CME_COMPARESTRINGS:                                  // CompareNoCase
    cVar = LookupVariable(cItem1upper, cIndex2);
    if (!cVar)
      ABORT_LINE("The variable " + cStrItems[1] + " is not defined in line:\n\n");
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
    if (CMD_IS(COMPARESTRINGS))
      cIndex = cVar->value.Compare(cStrCopy);
    else
      cIndex = cVar->value.CompareNoCase(cStrCopy);
    cLogRpt.Format("The strings %s equal", cIndex ? "are NOT" : "ARE");
    SetReportedValues(cIndex);
    break;

  case CME_STRIPENDINGDIGITS:                               // StripEndingDigits
    cVar = LookupVariable(cItem1upper, cIndex2);
    if (!cVar)
      ABORT_LINE("The variable " + cStrItems[1] + " is not defined in line:\n\n");
    cReport = cVar->value;
    for (cIndex = cReport.GetLength() - 1; cIndex > 0; cIndex--)
      if ((int)cReport.GetAt(cIndex) < '0' || (int)cReport.GetAt(cIndex) > '9')
        break;
    cStrCopy = cReport.Right(cReport.GetLength() - (cIndex + 1));
    cReport = cReport.Left(cIndex + 1);
    cItem1upper = cStrItems[2];
    cItem1upper.MakeUpper();
    if (SetVariable(cItem1upper, cReport, VARTYPE_REGULAR, -1, false))
      ABORT_LINE("Error setting variable " + cStrItems[2] + " with string " + cReport +
      " in:\n\n");
    SetReportedValues(atoi((LPCTSTR)cStrCopy));
    break;

  case CME_MAILSUBJECT:                                     // MailSubject
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, mMailSubject);

    break;
    // SendEmail, ErrorBoxSendEmail
  case CME_SENDEMAIL:
  case CME_ERRORBOXSENDEMAIL:
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cReport);
    if (CMD_IS(SENDEMAIL)) {
      mWinApp->mMailer->SendMail(mMailSubject, cReport);
    } else {
      if (!mWinApp->mMailer->GetInitialized() || mWinApp->mMailer->GetSendTo().IsEmpty())
        ABORT_LINE("Mail server not initialized or email address not defined; cannot "
        "do:\n");
      mEmailOnError = cReport;
    }
    break;

  case CME_CLEARALIGNMENT:                                  // ClearAlignment
    cDoShift = (cItemEmpty[1] || !cItemInt[1]) && !mScope->GetNoScope();
    mShiftManager->SetAlignShifts(0., 0., false, mImBufs, cDoShift);
    break;

  case CME_RESETIMAGESHIFT:                                 // ResetImageShift
    cTruth = mShiftManager->GetBacklashMouseAndISR();
    cBacklashX = 0.;
    if (cItemInt[1] > 0) {
      mShiftManager->SetBacklashMouseAndISR(true);
      if (cItemInt[1] > 1)
        cBacklashX = cItemEmpty[2] ? mScope->GetStageRelaxation() : cItemFlt[2];
    }
    cIndex = mShiftManager->ResetImageShift(true, false, 10000, cBacklashX);
    mShiftManager->SetBacklashMouseAndISR(cTruth);
    if (cIndex) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    break;

  case CME_RESETSHIFTIFABOVE:                               // ResetShiftIfAbove
    if (cItemEmpty[1])
      ABORT_LINE("ResetShiftIfAbove must be followed by a number in: \n\n");
    mScope->GetLDCenteredShift(cDelISX, cDelISY);
    cAMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    cDelX = cAMat.xpx * cDelISX + cAMat.xpy * cDelISY;
    cDelY = cAMat.ypx * cDelISX + cAMat.ypy * cDelISY;
    cSpecDist = sqrt(cDelX * cDelX + cDelY * cDelY);
    if (cSpecDist > cItemDbl[1])
      mWinApp->mComplexTasks->ResetShiftRealign();
    break;

  case CME_EUCENTRICITY:                                    // Eucentricity
    cIndex = FIND_EUCENTRICITY_FINE;
    if (!cItemEmpty[1])
      cIndex = cItemInt[1];
    if ((cIndex & (FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE)) == 0) {
      cReport.Format("Error in script: value on Eucentricity statement \r\n"
        "should be %d for coarse, %d for fine, or %d for both",
        FIND_EUCENTRICITY_COARSE, FIND_EUCENTRICITY_FINE,
        FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE);
      ABORT_LINE(cReport);
    }
    mWinApp->mComplexTasks->FindEucentricity(cIndex);
    break;

  case CME_REPORTLASTAXISOFFSET:                            // ReportLastAxisOffset
    cDelX = mWinApp->mComplexTasks->GetLastAxisOffset();
    if (cDelX < -900)
      ABORT_NOLINE("There is no last axis offset; fine eucentricity has not been run");
    cLogRpt.Format("Lateral axis offset in last run of Fine Eucentricity was %.2f", cDelX);
    SetReportedValues(&cStrItems[1], cDelX);
    break;

  case CME_WALKUPTO:                                        // WalkUpTo
    if (cItemEmpty[1])
      ABORT_LINE("WalkUpTo must be followed by a target angle in: \n\n");
    cDelISX = cItemDbl[1];
    if (cDelISX < -80. || cDelISX > 80.)
      ABORT_LINE("Target angle is too high in: \n\n");
    mWinApp->mComplexTasks->WalkUp((float)cDelISX, -1, 0);
    break;

  case CME_REVERSETILT:                                     // ReverseTilt
    cIndex = (mScope->GetReversalTilt() > mScope->GetTiltAngle()) ? 1 : -1;
    if (!cItemEmpty[1]) {
      cIndex = cItemInt[1];
      if (cIndex > 0)
        cIndex = 1;
      else if (cIndex < 0)
        cIndex = -1;
      else
        ABORT_NOLINE("Error in script: ReverseTilt should not be followed by 0");
    }
    mWinApp->mComplexTasks->ReverseTilt(cIndex);
    break;

  case CME_DRIFTWAITTASK:                                   // DriftWaitTask
  {
    DriftWaitParams * dfltParam = mWinApp->mParticleTasks->GetDriftWaitParams();
    DriftWaitParams dwparm = *dfltParam;
    if (!cItemEmpty[2]) {
      if (cStrItems[2] == "nm")
        dwparm.useAngstroms = false;
      else if (cStrItems[2] == "A")
        dwparm.useAngstroms = true;
      else if (cStrItems[2] != "0")
        ABORT_LINE("The second entry should be \"nm\", \"A\", or \"0\" in line:\n\n");
    }
    if (!cItemEmpty[1] && cItemDbl[1] > 0.) {
      dwparm.driftRate = cItemFlt[1] / (dwparm.useAngstroms ? 10.f : 1.f);
    }
    if (!cItemEmpty[3] && cItemDbl[3] > 0.) {
      dwparm.maxWaitTime = cItemFlt[3];
    }
    if (!cItemEmpty[4] && cItemDbl[4] > 0.) {
      dwparm.interval = cItemFlt[4];
    }
    if (cItemInt[5])
      dwparm.failureAction = cItemInt[5] > 0 ? 1 : 0;
    if (!cItemEmpty[6]) {
      if (cStrItems[6] == "T")
        dwparm.measureType = WFD_USE_TRIAL;
      else if (cStrItems[6] == "F")
        dwparm.measureType = WFD_USE_FOCUS;
      else if (cStrItems[6] == "A")
        dwparm.measureType = WFD_WITHIN_AUTOFOC;
      else if (cStrItems[6] != "0")
        ABORT_LINE("The image type to measure defocus from must be one of T, F, "
          "A, or 0 in line:\n\n");
    }
    if (cItemInt[7])
      dwparm.changeIS = cItemInt[7] > 0 ? 1 : 0;
    if (!cItemEmpty[8]) {
      dwparm.setTrialParams = true;
      if (cItemDbl[8] > 0)
        dwparm.exposure = cItemFlt[8];
      if (!cItemEmpty[9])
        dwparm.binning = cItemInt[9];
    }
    mWinApp->mParticleTasks->WaitForDrift(dwparm, false);
    break;
  }

  case CME_CONDITIONPHASEPLATE:                             // ConditionPhasePlate
    if (mWinApp->mMultiTSTasks->ConditionPhasePlate(cItemInt[1] != 0)) {
      AbortMacro();
      return;
    }
    break;

  case CME_GETWAITTASKDRIFT:                                // GetWaitTaskDrift
    SetReportedValues(&cStrItems[1], mWinApp->mParticleTasks->GetWDLastDriftRate(),
      mWinApp->mParticleTasks->GetWDLastFailed());
    break;

  case CME_BACKLASHADJUST:                                  // BacklashAdjust
    mWinApp->mMontageController->GetColumnBacklash(cBacklashX, cBacklashY);
    mWinApp->mComplexTasks->BacklashAdjustStagePos(cBacklashX, cBacklashY, false, false);
    break;

  case CME_CENTERBEAMFROMIMAGE:                             // CenterBeamFromImage
    cTruth = cItemInt[1] != 0;
    cDelISX = !cItemEmpty[2] ? cItemDbl[2] : 0.;
    cIndex = mWinApp->mProcessImage->CenterBeamFromActiveImage(0., 0., cTruth, cDelISX);
    if (cIndex > 0 && cIndex <= 3)
      ABORT_LINE("Script aborted centering beam because of no image,\n"
      "unusable image type, or failure to get memory");
    SetReportedValues(cIndex);
    break;

  case CME_AUTOCENTERBEAM:                                  // AutoCenterBeam
    cDelISX = cItemEmpty[1] ? 0. : cItemDbl[1];
    if (mWinApp->mMultiTSTasks->AutocenterBeam((float)cDelISX)) {
      AbortMacro();
      return;
    }
    break;

  case CME_COOKSPECIMEN:                                    // CookSpecimen
    if (mWinApp->mMultiTSTasks->StartCooker()) {
      AbortMacro();
      return;
    }
    break;

  case CME_SETINTENSITYBYLASTTILT:                          // SetIntensityByLastTilt
  case CME_SETINTENSITYFORMEAN:                             // SetIntensityForMean
  case CME_CHANGEINTENSITYBY:                               // ChangeIntensityBy
    cIndex2 = mWinApp->LowDoseMode() ? 3 : -1;
    if (CMD_IS(SETINTENSITYFORMEAN)) {
      if (!mImBufs->mImage || mImBufs->IsProcessed() ||
        (mWinApp->LowDoseMode() &&
        mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != 3))
        ABORT_LINE("Script aborted setting intensity because there\n"
          " is no image or a processed image in Buffer A\n"
          "(or, if in Low Dose mode, because the image in\n"
          " Buffer A is not from the Record area)");
      cDelISX = cItemDbl[1] /
        mWinApp->mProcessImage->EquivalentRecordMean(0);
    } else if (CMD_IS(SETINTENSITYBYLASTTILT)) {
      cDelISX = mIntensityFactor;
    } else {
      cDelISX = cItemDbl[1];
      cIndex2 = mScope->GetLowDoseArea();
    }

    cIndex = mWinApp->mBeamAssessor->ChangeBeamStrength(cDelISX, cIndex2);
    if (CheckIntensityChangeReturn(cIndex))
      return;
    UpdateLDAreaIfSaved();
    break;

  case CME_SETDOSERATE:                                     // SetDoseRate
    if (cItemDbl[1] <= 0)
      ABORT_LINE("Dose rate must be positive for line:\n\n");
    if (!mImBufs->mImage)
      ABORT_LINE("There must be an image in buffer A for line:\n\n");
    cIndex = mWinApp->mProcessImage->DoSetIntensity(true, cItemFlt[1]);
    if (cIndex < 0) {
      AbortMacro();
      return;
    }
    if (CheckIntensityChangeReturn(cIndex))
      return;
    UpdateLDAreaIfSaved();
    break;

  case CME_SETPERCENTC2:   // Set/IncPercentC2
  case CME_INCPERCENTC2:

    // The entered number is always in terms of % C2 or illuminated area, so for
    // incremental, first comparable value and add to get the absolute to set
    cDelISX = cItemDbl[1];
    if (cItemEmpty[1])
      ABORT_LINE("Script aborted because no C2 percent was entered in:\n\n");
    cIndex = mScope->FastSpotSize();
    if (CMD_IS(INCPERCENTC2))
      cDelISX += mScope->GetC2Percent(cIndex, mScope->GetIntensity());

    // Then convert to an intensity as appropriate for scope
    if (mScope->GetUseIllumAreaForC2()) {
      cDelISY = mScope->IllumAreaToIntensity(cDelISX / 100.);
    } else {
      cDelISY = (0.01 * cDelISX - mScope->GetC2SpotOffset(cIndex)) /
        mScope->GetC2IntensityFactor();
    }
    mScope->SetIntensity(cDelISY);
    cDelISY = mScope->FastIntensity();
    cDelISX = mScope->GetC2Percent(cIndex, cDelISY);
    cLogRpt.Format("Intensity set to %.3f%s  -  %.5f", cDelISX, mScope->GetC2Units(),
      cDelISY);
    UpdateLDAreaIfSaved();
    break;

  case CME_SETILLUMINATEDAREA:                              // SetIlluminatedArea
    if (!mScope->SetIlluminatedArea(cItemDbl[1])) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;

  case CME_SETIMAGEDISTANCEOFFSET:                          // SetImageDistanceOffset
    if (!mScope->SetImageDistanceOffset(cItemDbl[1])) {
      AbortMacro();
      return;
    }
    break;

  case CME_SETALPHA:                                        // SetAlpha
    if (!mScope->SetAlpha(cItemInt[1] - 1)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;

  case CME_REPORTJEOLGIF:                                   // ReportJeolGIF
    cIndex = mScope->GetJeolGIF();
    cLogRpt.Format("JEOL GIF MODE return value %d", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_SETJEOLGIF:                                      // SetJeolGIF
    if (!mScope->SetJeolGIF(cItemInt[1] ? 1 : 0)) {
      AbortMacro();
      return;
    }
    break;

  case CME_NORMALIZELENSES:                                 // NormalizeLenses
    if (cItemInt[1] & 1)
      cIndex = mScope->NormalizeProjector();
    if (cItemInt[1] & 2)
      cIndex = mScope->NormalizeObjective();
    if (cItemInt[1] & 4)
      cIndex = mScope->NormalizeCondenser();
    if (!cIndex)
      AbortMacro();
    break;

  case CME_NORMALIZEALLLENSES:                              // NormalizeAllLenses
    cIndex = 0;
    if (!cItemEmpty[1])
      cIndex = cItemInt[1];
    if (cIndex < 0 || cIndex > 3)
      ABORT_LINE("Lens group specifier must be between 0 and 3 in: \n\n");
    if (!mScope->NormalizeAll(cIndex))
      AbortMacro();
    break;

  case CME_REPORTSLOTSTATUS:                                // ReportSlotStatus
    if (!mScope->CassetteSlotStatus(cItemInt[1], cIndex)) {
      AbortMacro();
      return;
    }
    if (cIndex < -1)
      cLogRpt.Format("Requesting status of slot %d gives an error", cItemInt[1]);
    else
      cLogRpt.Format("Slot %d %s", cItemInt[1], cIndex < 0 ? "has unknown status" :
      (cIndex ? "is occupied" : "is empty"));
    SetReportedValues(&cStrItems[2], (double)cIndex);
    break;

  case CME_LOADCARTRIDGE:
  case CME_UNLOADCARTRIDGE: // Load/UnloadCartridge
    if (CMD_IS(LOADCARTRIDGE))
      cIndex = mScope->LoadCartridge(cItemInt[1]);
    else
      cIndex = mScope->UnloadCartridge();
    if (cIndex)
      ABORT_LINE(cIndex == 1 ? "The thread is already busy for a long operation in:\n\n" :
      "There was an error trying to run a long operation with:\n\n");
    mStartedLongOp = true;
    break;

  case CME_REFRIGERANTLEVEL:                                // RefrigerantLevel
    if (cItemInt[1] < 1 || cItemInt[1] > 3)
      ABORT_LINE("Dewar number must be between 1 and 3 in: \n\n");
    if (!mScope->GetRefrigerantLevel(cItemInt[1], cDelX)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Refrigerant level in dewar %d is %.3f", cItemInt[1], cDelX);
    SetReportedValues(&cStrItems[2], cDelX);
    break;

  case CME_DEWARSREMAININGTIME:                             // DewarsRemainingTime
    if (!mScope->GetDewarsRemainingTime(cIndex)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Remaining time to fill dewars is %d", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_AREDEWARSFILLING:                                // AreDewarsFilling
    if (!mScope->AreDewarsFilling(cIndex)) {
      AbortMacro();
      return;
    }
    if (FEIscope)
      cLogRpt.Format("Dewars %s busy filling", cIndex ? "ARE" : "are NOT");
    else {
      char * dewarTxt[4] = {"No tanks are", "Stage tank is", "Transfer tank is",
        "Stage and transfer tanks are"};
      B3DCLAMP(cIndex, 0, 3);
      cLogRpt.Format("%s being refilled", dewarTxt[cIndex]);
    }
    SetReportedValues(&cStrItems[1], cIndex);
    break;

  case CME_REPORTVACUUMGAUGE:                               // ReportVacuumGauge
    if (!mScope->GetGaugePressure((LPCTSTR)cStrItems[1], cIndex, cDelISX)) {
      if (JEOLscope && cIndex < -1 && cIndex > -4) {
        cReport.Format("Name must be \"Pir\" or \"Pen\" followed by number between 0 and "
          "9 in line:\n\n");
        ABORT_LINE(cReport);
      }
      AbortMacro();
      return;
    }
    cLogRpt.Format("Gauge %s status is %d, pressure is %f", (LPCTSTR)cStrItems[1], cIndex,
      cDelISX);
    SetReportedValues(&cStrItems[2], cIndex, cDelISX);
    break;

  case CME_REPORTHIGHVOLTAGE:                               // ReportHighVoltage
    cDelISX = mScope->GetHTValue();
    cLogRpt.Format("High voltage is %.1f kV", cDelISX);
    SetReportedValues(&cStrItems[1], cDelISX);
    break;

  case CME_SETSLITWIDTH:                                    // SetSlitWidth
    if (cItemEmpty[1])
      ABORT_LINE("SetSlitWidth must be followed by a number in: \n\n");
    cDelISX = cItemDbl[1];
    if (cDelISX < cFiltParam->minWidth || cDelISX > cFiltParam->maxWidth)
      ABORT_LINE("This is not a legal slit width in: \n\n");
    cFiltParam->slitWidth = (float)cDelISX;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();

    break;
    // SetEnergyLoss, ChangeEnergyLoss
  case CME_SETENERGYLOSS:
  case CME_CHANGEENERGYLOSS:
    if (cItemEmpty[1])
      ABORT_LINE(cStrItems[0] + " must be followed by a number in: \n\n");
    cDelISX = cItemDbl[1];
    if (CMD_IS(CHANGEENERGYLOSS))
      cDelISX += cFiltParam->energyLoss;
    if (mWinApp->mFilterControl.LossOutOfRange(cDelISX, cDelISY)) {
      cReport.Format("The energy loss requested in:\n\n%s\n\nrequires a net %s of %.1f"
        "with the current adjustments.\nThis net value is beyond the allowed range.",
        (LPCTSTR)cStrLine, mScope->GetHasOmegaFilter() ? "shift" : "offset", cDelISY);
      ABORT_LINE(cReport);
    }
    cFiltParam->energyLoss = (float)cDelISX;
    cFiltParam->zeroLoss = false;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();
    break;

  case CME_SETSLITIN:                                       // SetSlitIn
    cIndex = 1;
    if (!cItemEmpty[1])
      cIndex = cItemInt[1];
    cFiltParam->slitIn = cIndex != 0;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();
    break;

  case CME_REFINEZLP:                                       // RefineZLP
    if (cItemEmpty[1] || SEMTickInterval(1000. * cFiltParam->alignZLPTimeStamp) >
        60000. * cItemDbl[1]) {
      CTime ctdt = CTime::GetCurrentTime();
      cReport.Format("%02d:%02d:%02d", ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
      mWinApp->AppendToLog(cReport, LOG_OPEN_IF_CLOSED);
      mWinApp->mFilterTasks->RefineZLP(false);
    }
    break;

  case CME_SELECTCAMERA:                                    // SelectCamera
    cIndex = cItemInt[1];
    if (cIndex < 1 || cIndex > mWinApp->GetNumActiveCameras())
      ABORT_LINE("Camera number out of range in: \n\n");
    RestoreCameraSet(-1, true);
    mWinApp->SetActiveCameraNumber(cIndex - 1);
    break;

  case CME_SETEXPOSURE:                                     // SetExposure
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    cDelX = cItemDbl[2];
    cDelY = cItemEmpty[3] ? 0. : cItemDbl[3];
    if (cItemEmpty[2] || cDelX <= 0. || cDelY < 0.)
      ABORT_LINE("Incorrect entry for setting exposure: \n\n");
    SaveControlSet(cIndex);
    mConSets[cIndex].exposure = (float)cDelX;
    if (!cItemEmpty[3])
      mConSets[cIndex].drift = (float)cDelY;
    break;

  case CME_SETBINNING:                                      // SetBinning
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (CheckCameraBinning(cItemDbl[2], cIndex2, cStrCopy))
      ABORT_LINE(cStrCopy);
    SaveControlSet(cIndex);
    mConSets[cIndex].binning = cIndex2;
    break;

  case CME_SETCAMERAAREA:                                   // SetCameraArea
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    cReport = cStrItems[2];
    cReport.MakeUpper();
    if (cReport == "F" || cReport == "H" || cReport == "Q" ||
      cReport == "WH" || cReport == "WQ") {
      if (cReport == "F" || cReport == "WH") {
        cIx0 = 0;
        cIx1 = cCamParams->sizeX;
      } else if (cReport == "Q") {
        cIx0 = 3 * cCamParams->sizeX / 8;
        cIx1 = 5 * cCamParams->sizeX / 8;
      } else {
        cIx0 = cCamParams->sizeX / 4;
        cIx1 = 3 * cCamParams->sizeX / 4;
      }
      if (cReport == "F") {
        cIy0 = 0;
        cIy1 = cCamParams->sizeY;
      } else if (cReport == "H" || cReport == "WH") {
        cIy0 = cCamParams->sizeY / 4;
        cIy1 = 3 * cCamParams->sizeY / 4;
      } else {
        cIy0 = 3 * cCamParams->sizeY / 8;
        cIy1 = 5 * cCamParams->sizeY / 8;
      }
    } else {
      if (cItemEmpty[5])
        ABORT_LINE("Not enough coordinates for setting camera area: \n\n");
      cIndex2 = BinDivisorI(cCamParams);
      cIx0 = B3DMAX(0, B3DMIN(cCamParams->sizeX - 4, cIndex2 * cItemInt[2]));
      cIx1 = B3DMAX(cIx0 + 1, B3DMIN(cCamParams->sizeX, cIndex2 * cItemInt[3]));
      cIy0 = B3DMAX(0, B3DMIN(cCamParams->sizeY - 4, cIndex2 * cItemInt[4]));
      cIy1 = B3DMAX(cIy0 + 1, B3DMIN(cCamParams->sizeY, cIndex2 * cItemInt[5]));
    }
    cIndex2 = mConSets[cIndex].binning;
    cIy0 /= cIndex2;
    cIy1 /= cIndex2;
    cIx0 /= cIndex2;
    cIx1 /= cIndex2;
    cSizeX = cIx1 - cIx0;
    cSizeY = cIy1 - cIy0;
    mCamera->AdjustSizes(cSizeX, cCamParams->sizeX, cCamParams->moduloX, cIx0, cIx1, cSizeY,
      cCamParams->sizeY, cCamParams->moduloY, cIy0, cIy1, cIndex2);
    SaveControlSet(cIndex);
    mConSets[cIndex].left = cIx0 * cIndex2;
    mConSets[cIndex].right = cIx1 * cIndex2;
    mConSets[cIndex].top = cIy0 * cIndex2;
    mConSets[cIndex].bottom = cIy1 * cIndex2;
    break;

  case CME_SETCENTEREDSIZE:                                 // SetCenteredSize
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (CheckCameraBinning(cItemDbl[2], cIndex2, cStrCopy))
      ABORT_LINE(cStrCopy);
    cSizeX = cItemInt[3];
    cSizeY = cItemInt[4];
    if (cSizeX < 4 || cSizeX * cIndex2 > cCamParams->sizeX || cSizeY < 4 ||
      cSizeY * cIndex2 > cCamParams->sizeY)
      ABORT_LINE("Image size is out of range for current camera at given binning in:"
      " \n\n");
    SaveControlSet(cIndex);
    mConSets[cIndex].binning = cIndex2;
    mCamera->CenteredSizes(cSizeX, cCamParams->sizeX, cCamParams->moduloX, cIx0, cIx1,
      cSizeY, cCamParams->sizeY, cCamParams->moduloY, cIy0, cIy1, cIndex2);
    mConSets[cIndex].left = cIx0 * cIndex2;
    mConSets[cIndex].right = cIx1 * cIndex2;
    mConSets[cIndex].top = cIy0 * cIndex2;
    mConSets[cIndex].bottom = cIy1 * cIndex2;
    break;

  case CME_SETEXPOSUREFORMEAN:                              // SetExposureForMean
    cIndex = RECORD_CONSET;
    if (!mImBufs->mImage || mImBufs->IsProcessed() ||
      (mWinApp->LowDoseMode() && mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != cIndex))
      ABORT_LINE("Script aborted setting exposure time because\n"
      " there is no image or a processed image in Buffer A\n"
      "(or, if in Low Dose mode, because the image in\n"
      " Buffer A is not from the Record area) for line:\n\n");
    if (cItemDbl[1] <= 0)
      ABORT_LINE("Exposure time must be positive for line:\n\n");
    cDelISX = cItemDbl[1] / mWinApp->mProcessImage->EquivalentRecordMean(0);
    cDelISY = cDelISX * B3DMAX(0.001, mConSets[cIndex].exposure - cCamParams->deadTime) +
      cCamParams->deadTime;
    if (cItemInt[2]) {

      // Adjusting frame time to keep constant number of frames
      if (!cCamParams->K2Type || !mConSets[cIndex].doseFrac)
        ABORT_LINE("Frame time can be adjusted only for K2/K3 camera"
        " with dose fractionation mode on in line:\n\n");
      cIndex2 = B3DNINT(mConSets[cIndex].exposure /
        B3DMAX(0.001, mConSets[cIndex].frameTime));
      cBmin = (float)(cDelISY / cIndex2);
      mCamera->ConstrainFrameTime(cBmin, cCamParams);
      if (fabs(cBmin - mConSets[cIndex].frameTime) < 0.0001) {
        PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
          "too small a change in frame time", (LPCTSTR)cStrItems[1], cDelISX);
        cBmean = 0.;
      } else {
        SaveControlSet(cIndex);
        mConSets[cIndex].frameTime = cBmin;
        cBmean = cIndex2 * cBmin;
        PrintfToLog("In SetExposureForMean %s, frame time changed to %.4f, exposure time"
          " changed to %.4f", (LPCTSTR)cStrItems[1], mConSets[cIndex].frameTime,
          cBmean);
      }
    } else {

      // Just adjusting exposure time
      cBmean = (float)cDelISY;
      cIx1 = mCamera->DESumCountForConstraints(cCamParams, &mConSets[cIndex]);
      mCamera->CropTietzSubarea(cCamParams, mConSets[cIndex].right - mConSets[cIndex].left,
        mConSets[cIndex].bottom - mConSets[cIndex].top, mConSets[cIndex].processing,
        mConSets[cIndex].mode, cIy1);
      mCamera->ConstrainExposureTime(cCamParams, mConSets[cIndex].doseFrac,
        mConSets[cIndex].K2ReadMode, mConSets[cIndex].binning,
        mConSets[cIndex].alignFrames && !mConSets[cIndex].useFrameAlign, cIx1, cBmean,
        mConSets[cIndex].frameTime, cIy1, mConSets[cIndex].mode);
      if (fabs(cBmean - mConSets[cIndex].exposure) < 0.00001) {
        PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
          "too small a change in exposure time", (LPCTSTR)cStrItems[1], cDelISX);
        cBmean = 0.;
      } else {
        SaveControlSet(cIndex);
        cBmean = mWinApp->mFalconHelper->AdjustSumsForExposure(cCamParams, &mConSets[cIndex],
          cBmean);
        /* This is good for seeing how the distribute frames function works
        report.Format("Summed frame list:");
        for (index2 = 0; index2 < mConSets[index].summedFrameList.size(); index2++) {
          strCopy.Format(" %d", mConSets[index].summedFrameList[index2]);
          report+= strCopy;
        }
        mWinApp->AppendToLog(report); */
        PrintfToLog("In SetExposureForMean %s, exposure time changed to %.4f",
          (LPCTSTR)cStrItems[1], cBmean);
      }
    }

    // Commit the new exposure and report if change is not good
    if (cBmean) {
      float diffThresh = 0.05f;
      mConSets[cIndex].exposure = cBmean;
      if (fabs(cBmean - cDelISY) / cDelISY > diffThresh)
        PrintfToLog("WARNING: Desired exposure time (%.3f) differs from actual one "
        "(%.3f) by more than %d%%", cDelISY, cBmean, B3DNINT(100. * diffThresh));
    }
    break;

  case CME_SETCONTINUOUS:                                   // SetContinuous
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    SaveControlSet(cIndex);
    mConSets[cIndex].mode = cItemInt[2] ? CONTINUOUS : SINGLE_FRAME;
    break;

  case CME_SETPROCESSING:                                   // SetProcessing
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (cItemInt[2] < 0 || cItemInt[2] > 2)
      ABORT_LINE("Processing must 0, 1, or 2 in:\n\n");
    SaveControlSet(cIndex);
    mConSets[cIndex].processing = cItemInt[2];
    break;

  case CME_SETFRAMETIME:                                    // SetFrameTime
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (!cCamParams->K2Type && !cCamParams->canTakeFrames)
      ABORT_NOLINE("Frame time cannot be set for the current camera type");
    SaveControlSet(cIndex);
    mConSets[cIndex].frameTime = cItemFlt[2];
    mCamera->CropTietzSubarea(cCamParams, mConSets[cIndex].right - mConSets[cIndex].left,
      mConSets[cIndex].bottom - mConSets[cIndex].top, mConSets[cIndex].processing,
      mConSets[cIndex].mode, cIy1);
    mCamera->ConstrainFrameTime(mConSets[cIndex].frameTime, cCamParams,
      mConSets[cIndex].binning, cCamParams->OneViewType ? mConSets[cIndex].K2ReadMode : cIy1);
    break;

  case CME_SETK2READMODE:                                   // SetK2ReadMode
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (!cCamParams->K2Type && !cCamParams->OneViewType &&
      cCamParams->FEItype != FALCON3_TYPE && !cCamParams->DE_camType)
      ABORT_NOLINE("Read mode cannot be set for the current camera type");
    if (cItemInt[2] < 0 || cItemInt[2] > 2)
      ABORT_LINE("Read mode must 0, 1, or 2 in:\n\n");
    SaveControlSet(cIndex);
    mConSets[cIndex].K2ReadMode = cItemInt[2];
    break;

  case CME_SETDOSEFRACPARAMS:                               // SetDoseFracParams
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    if (!cItemEmpty[5] && (cItemInt[5] < 0 || cItemInt[5] > 2))
      ABORT_LINE("Alignment method (the fourth number) must be 0, 1, or 2 in:\n\n");
    SaveControlSet(cIndex);
    mConSets[cIndex].doseFrac = cItemInt[2] ? 1 : 0;
    if (!cItemEmpty[3])
      mConSets[cIndex].saveFrames = cItemInt[3] ? 1 : 0;
    if (!cItemEmpty[4])
      mConSets[cIndex].alignFrames = cItemInt[4] ? 1 : 0;
    if (!cItemEmpty[5])
      mConSets[cIndex].useFrameAlign = cItemInt[5];
    if (!cItemEmpty[6])
      mConSets[cIndex].sumK2Frames = cItemInt[6] ? 1 : 0;
    break;

  case CME_SETDECAMFRAMERATE:                               // SetDECamFrameRate
    if (!(cCamParams->DE_camType && cCamParams->DE_FramesPerSec > 0))
      ABORT_LINE("The current camera must be a DE camera with adjustable frame rate for"
        " line:\n\n");
    cDelISX = cItemDbl[1];
    if (cDelISX < cCamParams->DE_MaxFrameRate + 1.)
      cDelISX = B3DMIN(cDelISX, cCamParams->DE_MaxFrameRate);
    if (cDelISX <= 0. || cDelISX > cCamParams->DE_MaxFrameRate) {
      cReport.Format("The new frame rate must be greater than zero\n"
        "and less than %.2f FPS for line:\n\n", cCamParams->DE_MaxFrameRate);
      ABORT_LINE(cReport);
    }
    if (mDEframeRateToRestore < 0) {
      mNumStatesToRestore++;
      mDEframeRateToRestore = cCamParams->DE_FramesPerSec;
      mDEcamIndToRestore = cCurrentCam;
    }
    cCamParams->DE_FramesPerSec = (float)cDelISX;
    mWinApp->mDEToolDlg.UpdateSettings();
    SetReportedValues(mDEframeRateToRestore);
    cLogRpt.Format("Changed frame rate of DE camera from %.2f to %.2f",
      mDEframeRateToRestore, cDelISX);
    break;

  case CME_USECONTINUOUSFRAMES:                             // UseContinuousFrames
    cTruth = cItemInt[1] != 0;
    if ((mUsingContinuous ? 1 : 0) != (cTruth ? 1 : 0))
      mCamera->ChangePreventToggle(cTruth ? 1 : -1);
    mUsingContinuous = cItemInt[1] != 0;
    break;

  case CME_STOPCONTINUOUS:                                  // StopContinuous
    mCamera->StopCapture(0);
    break;

  case CME_REPORTCONTINUOUS:                                // ReportContinuous
    cIndex = mCamera->DoingContinuousAcquire();
    if (cIndex)
      cLogRpt.Format("Continuous acquire is running with set %d", cIndex - 1);
    else
      cLogRpt = "Continuous acquire is not running";
    SetReportedValues(&cStrItems[1], cIndex - 1.);
    break;

  case CME_STARTFRAMEWAITTIMER:                             // StartFrameWaitTimer
    if (cItemInt[1] < 0)
      mFrameWaitStart = -2.;
    else
      mFrameWaitStart = GetTickCount();
    break;

  case CME_WAITFORNEXTFRAME:                                // WaitForNextFrame
    if (!cItemEmpty[1])
      mCamera->AlignContinuousFrames(cItemInt[1], cItemInt[2] != 0);
    mCamera->SetTaskFrameWaitStart((mFrameWaitStart >= 0 || mFrameWaitStart < -1.1) ?
      mFrameWaitStart : (double)GetTickCount());
    mFrameWaitStart = -1.;
    break;

  case CME_SETLIVESETTLEFRACTION:                           // SetLiveSettleFraction
    mCamera->SetContinuousDelayFrac((float)B3DMAX(0., cItemDbl[1]));
    break;

  case CME_SETSTEMDETECTORS:                                // SetSTEMDetectors
    if (!cCamParams->STEMcamera)
      ABORT_LINE("The current camera is not a STEM camera in: \n\n");
    if (CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex, cStrCopy))
      ABORT_LINE(cStrCopy);
    cSizeX = mCamera->GetMaxChannels(cCamParams);
    if (cItemEmpty[cSizeX + 1])
      ABORT_LINE("There must be a detector number for each channel in: \n\n");
    cIndex2 = 0;
    for (cIx0 = 2; cIx0 < cSizeX + 2; cIx0++) {
      if (cItemInt[cIx0] < -1 || cItemInt[cIx0] >= cCamParams->numChannels)
        ABORT_LINE("Detector number out of range in: \n\n");
      for (cIy0 = cIx0 + 1; cIy0 < cSizeX + 2; cIy0++) {
        if (cItemInt[cIx0] == cItemInt[cIy0] && cItemInt[cIx0] != -1)
          ABORT_LINE("Duplicate detector number in: \n\n");
      }
      if (cItemInt[cIx0] >= 0)
        cIndex2++;
    }
    if (!cIndex2)
      ABORT_LINE("There must be at least one detector listed in: \n\n");
    SaveControlSet(cIndex);
    for (cIx0 = 0; cIx0 < cSizeX; cIx0++)
      mConSets[cIndex].channelIndex[cIx0] = cItemInt[cIx0 + 2];
    break;

  case CME_RESTORECAMERASET:                                // RestoreCameraSet
    cIndex = -1;
    if (!cItemEmpty[1] && CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex,
      cStrCopy))
        ABORT_LINE(cStrCopy);
    if (RestoreCameraSet(cIndex, true))
      ABORT_NOLINE("No camera parameters were changed; there is nothing to restore");
    if (cIndex == RECORD_CONSET && mAlignWholeTSOnly) {
      SaveControlSet(cIndex);
      mConSets[cIndex].alignFrames = 0;
    }
    break;

  case CME_KEEPCAMERASETCHANGES:                            // KeepCameraSetChanges
  {
    ControlSet * masterSets = mWinApp->GetCamConSets() +
      MAX_CONSETS * cCurrentCam;
    cIndex = -1;
    if (!cItemEmpty[1] && CheckAndConvertCameraSet(cStrItems[1], cItemInt[1], cIndex,
      cStrCopy))
      ABORT_LINE(cStrCopy);
    for (cIx0 = (int)mConsetNums.size() - 1; cIx0 >= 0; cIx0--) {
      if (mConsetNums[cIx0] == cIndex || cIndex < 0) {
        masterSets[mConsetNums[cIx0]] = mConSets[mConsetNums[cIx0]];
        mConsetsSaved.erase(mConsetsSaved.begin() + cIx0);
        mConsetNums.erase(mConsetNums.begin() + cIx0);
      }
    }
  }
  break;

  case CME_REPORTK2FILEPARAMS:                              // ReportK2FileParams
    cIndex = mCamera->GetK2SaveAsTiff();
    cIndex2 = mCamera->GetSaveRawPacked();
    cIx0 = mCamera->GetUse4BitMrcMode();
    cIx1 = mCamera->GetSaveTimes100();
    cIy0 = mCamera->GetSkipK2FrameRotFlip();
    cIy1 = mCamera->GetOneK2FramePerFile();
    cLogRpt.Format("File type %s  raw packed %d  4-bit mode %d  x100 %d  Skip rot %d  "
      "file/frame %d", cIndex > 1 ? "TIFF ZIP" : (cIndex ? "MRC" : "TIFF LZW"), cIndex2,
      cIx0, cIx1, cIy0, cIy1);
    SetReportedValues(&cStrItems[1], cIndex, cIndex2, cIx0, cIx1, cIy0, cIy1);
    break;

  case CME_SETK2FILEPARAMS:                                 // SetK2FileParams
    if (cItemInt[1] < 0 || cItemInt[1] > 2)
      ABORT_LINE("File type (first number) must be 0, 1, or 2 in:\n\n");
    if (cItemInt[2] < 0 || cItemInt[2] > 3)
      ABORT_LINE("Saving raw packed (second number) must be 0, 1, 2, or 3 in:\n\n");
    mCamera->SetK2SaveAsTiff(cItemInt[1]);
    if (!cItemEmpty[2])
      mCamera->SetSaveRawPacked(cItemInt[2]);
    if (!cItemEmpty[3])
      mCamera->SetUse4BitMrcMode(cItemInt[3] ? 1 : 0);
    if (!cItemEmpty[4])
      mCamera->SetSaveTimes100(cItemInt[4] ? 1 : 0);
    if (!cItemEmpty[5])
      mCamera->SetSkipK2FrameRotFlip(cItemInt[5] ? 1 : 0);
    if (!cItemEmpty[6])
      mCamera->SetOneK2FramePerFile(cItemInt[6] ? 1 : 0);
    break;

  case CME_REPORTFRAMEALIPARAMS:                            // ReportFrameAliParams
  case CME_SETFRAMEALIPARAMS:                               // SetFrameAliParams
  case CME_REPORTFRAMEALI2:                                 // ReportFrameAli2
  case CME_SETFRAMEALI2:                                    // SetFrameAli2
    {
      CArray < FrameAliParams, FrameAliParams > *faParamArr = mCamera->GetFrameAliParams();
      FrameAliParams * faParam;
      BOOL * useGPUArr = mCamera->GetUseGPUforK2Align();
      cIndex = mConSets[RECORD_CONSET].faParamSetInd;
      if (cIndex < 0 || cIndex >= (int)faParamArr->GetSize())
        ABORT_LINE("The frame alignment parameter set index for Record is out of range "
        "for:\n\n");
      faParam = faParamArr->GetData() + cIndex;
      cIx1 = cCamParams->useSocket ? 1 : 0;
      if (CMD_IS(REPORTFRAMEALIPARAMS)) {
        cIndex = (faParam->doRefine ? 1 : -1) * faParam->refineIter;
        cIndex2 = (faParam->useGroups ? 1 : -1) * faParam->groupSize;
        cLogRpt.Format("Frame alignment for Record has %s %d, keep precision %d"
          ", strategy %d, all-vs-all %d, refine %d, group %d",
          faParam->binToTarget ? "target" : "binning",
          faParam->binToTarget ? faParam->targetSize : faParam->aliBinning,
          faParam->keepPrecision, faParam->strategy,
          faParam->numAllVsAll, cIndex, cIndex2);
        SetReportedValues(&cStrItems[1], faParam->aliBinning, faParam->keepPrecision,
          faParam->strategy, faParam->numAllVsAll, cIndex, cIndex2);

      } else if (CMD_IS(SETFRAMEALIPARAMS)) {
        if (cItemInt[1] < 1 || (cItemInt[1] > 16 && cItemInt[1] < 100))
          ABORT_LINE("Alignment binning is out of range in:\n\n");
        if (cItemInt[1] > 16)
          faParam->targetSize = cItemInt[1];
        else
          faParam->aliBinning = cItemInt[1];
        faParam->binToTarget = cItemInt[1] > 16;
        if (!cItemEmpty[2])
          faParam->keepPrecision = cItemInt[2] > 0;
        if (!cItemEmpty[3]) {
          B3DCLAMP(cItemInt[3], 0, 3);
          faParam->strategy = cItemInt[3];
        }
        if (!cItemEmpty[4])
          faParam->numAllVsAll = cItemInt[4];
        if (!cItemEmpty[5]) {
          faParam->doRefine = cItemInt[5] > 0;
          faParam->refineIter = B3DABS(cItemInt[5]);
        }
        if (!cItemEmpty[6]) {
          faParam->useGroups = cItemInt[6] > 0;
          faParam->groupSize = B3DABS(cItemInt[6]);
        }

      } else if (CMD_IS(REPORTFRAMEALI2)) {                 // ReportFrameAli2
        cDelX = (faParam->truncate ? 1 : -1) * faParam->truncLimit;
        cLogRpt.Format("Frame alignment for Record has GPU %d, truncation %.2f, hybrid %d,"
          " filters %.4f %.4f %.4f", useGPUArr[cIx1], cDelX, faParam->hybridShifts,
          faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);
        SetReportedValues(&cStrItems[1], useGPUArr[0], cDelX, faParam->hybridShifts,
          faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);

      } else {                                              // SetFrameAli2
        useGPUArr[cIx1] = cItemInt[1] > 0;
        if (!cItemEmpty[2]) {
          faParam->truncate = cItemDbl[2] > 0;
          faParam->truncLimit = (float)B3DABS(cItemDbl[2]);
        }
        if (!cItemEmpty[3])
          faParam->hybridShifts = cItemInt[3] > 0;
        if (!cItemEmpty[4]) {
          faParam->rad2Filt1 = cItemFlt[4];
          faParam->rad2Filt2 = cItemEmpty[5] ? 0.f : cItemFlt[5];
          faParam->rad2Filt3 = cItemEmpty[6] ? 0.f : cItemFlt[6];
        }
      }
      break;
    }

  case CME_SETFOLDERFORFRAMES:                              // SetFolderForFrames
    if (!(mCamera->IsDirectDetector(cCamParams) ||
      (cCamParams->canTakeFrames & FRAMES_CAN_BE_SAVED)))
      ABORT_LINE("Cannot save frames from the current camera for line:\n\n");
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    if (cCamParams->DE_camType || (cCamParams->FEItype && FCAM_ADVANCED(cCamParams))) {
      if (cStrCopy.FindOneOf("/\\") >= 0)
        ABORT_LINE("Only a single folder can be entered for this camera type in:\n\n");
    } else {
      cIndex = cStrCopy.GetAt(0);
      if (!(((cIndex == ' / ' || cIndex == '\\') &&
        (cStrCopy.GetAt(1) == ' / ' || cStrCopy.GetAt(1) == '\\')) ||
        ((cStrCopy.GetAt(2) == ' / ' || cStrCopy.GetAt(2) == '\\') && cStrCopy.GetAt(1) == ':'
          && ((cIndex >= 'A' && cIndex <= 'Z') || (cIndex >= 'a' && cIndex <= 'z')))))
        ABORT_LINE("A complete absolute path must be entered in:\n\n");
    }
    if (cCamParams->K2Type)
      mCamera->SetDirForK2Frames(cStrCopy);
    else if (cCamParams->DE_camType)
      mCamera->SetDirForDEFrames(cStrCopy);
    else if (cCamParams->FEItype)
      mCamera->SetDirForFalconFrames(cStrCopy);
    else
      cCamParams->dirForFrameSaving = cStrCopy;
    break;

  case CME_SKIPFRAMEALIPARAMCHECK:                          // SkipFrameAliCheck
    cIndex = cItemEmpty[1] ? 1 : cItemInt[1];
    mSkipFrameAliCheck = cIndex > 0;
    break;

  case CME_REPORTK3CDSMODE:                                 // ReportK3CDSmode
    cIndex = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
    cLogRpt.Format("CDS mode is %s", cIndex ? "ON" : "OFF");
    SetReportedValues(&cStrItems[1], cIndex);
    break;

  case CME_SETK3CDSMODE:                                    // SetK3CDSmode
    if (cItemEmpty[2] || !cItemInt[2]) {
      if (mK3CDSmodeToRestore < 0) {
        mNumStatesToRestore++;
        mK3CDSmodeToRestore = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
      }
    } else {
      if (mK3CDSmodeToRestore >= 0)
        mNumStatesToRestore--;
      mK3CDSmodeToRestore = -1;
    }
    mCamera->SetUseK3CorrDblSamp(cItemInt[1] != 0);
    break;

  case CME_REPORTCOUNTSCALING:                              // ReportCountScaling
    cIndex = mCamera->GetDivideBy2();
    cDelX = mCamera->GetCountScaling(cCamParams);
    if (cCamParams->K2Type == K3_TYPE)
      cDelX = cCamParams->countsPerElectron;
    SetReportedValues(&cStrItems[1], cIndex, cDelX);
    cLogRpt.Format("Division by 2 is %s; count scaling is %.3f", cIndex ? "ON" : "OFF",
      cDelX);
    break;

  case CME_SETDIVIDEBY2:                                    // SetDivideBy2
    mCamera->SetDivideBy2(cItemInt[1] != 0 ? 1 : 0);
    break;

  case CME_REPORTNUMFRAMESSAVED:                            // ReportNumFramesSaved
    cIndex = mCamera->GetNumFramesSaved();
    SetReportedValues(&cStrItems[1], cIndex);
    cLogRpt.Format("Number of frames saved was %d", cIndex);
    break;

  case CME_CAMERAPROPERTIES:                                // CameraProperties
    if (!cItemEmpty[1]) {
      if (cItemInt[1] < 1 || cItemInt[1] > mWinApp->GetActiveCamListSize())
        ABORT_LINE("Active camera number is out of range in:\n\n")
      cCamParams = mWinApp->GetCamParams() + cActiveList[cItemInt[1] - 1];
    }
    cIx1 = BinDivisorI(cCamParams);
    cIndex = cCamParams->sizeX / cIx1;
    cIndex2 = cCamParams->sizeY / cIx1;
    if (cItemEmpty[2])
      cIy1 = mScope->GetMagIndex();
    else
      cIy1 = cItemInt[2];
    cDelX = 1000. * cIx1 * mShiftManager->GetPixelSize(
      cItemEmpty[1] ? cCurrentCam : cActiveList[cItemInt[1] - 1], cIy1);
    cLogRpt.Format("%s: size %d x %d    rotation/flip %d   pixel on chip %.1f   "
      "unbinned pixel %.3f nm at %dx",
      (LPCSTR)cCamParams->name, cIndex, cIndex2, cCamParams->rotationFlip,
      cCamParams->pixelMicrons * cIx1, cDelX, MagForCamera(cCamParams, cIy1));
    SetReportedValues((double)cIndex, (double)cIndex2,
      (double)cCamParams->rotationFlip, cCamParams->pixelMicrons * cIx1, cDelX);
    break;

  case CME_REPORTCOLUMNORGUNVALVE:                        // ReportColumnOrGunValve
    cIndex = mScope->GetColumnValvesOpen();
    if (cIndex == -2)
      ABORT_NOLINE("An error occurred getting the state of the column/gun valve");
    SetReportedValues(&cStrItems[1], cIndex);
    cLogRpt.Format("Column/gun valve state is %d", cIndex);
    break;

  case CME_SETCOLUMNORGUNVALVE:                             // SetColumnOrGunValve
    if (cItemEmpty[1])
      ABORT_LINE("Entry requires a number for setting valve open or closed: \n\n");
    if (!mScope->SetColumnValvesOpen(cItemInt[1] != 0))
      ABORT_NOLINE(cItemInt[1] ? "An error occurred opening the valve" :
      "An error occurred closing the valve");
    break;

  case CME_REPORTFILAMENTCURRENT:                           // ReportFilamentCurrent
    cDelISX = mScope->GetFilamentCurrent();
    if (cDelISX < 0)
      ABORT_NOLINE("An error occurred getting the filament current");
    SetReportedValues(&cStrItems[1], cDelISX);
    cLogRpt.Format("Filament current is %.5g", cDelISX);
    break;

  case CME_SETFILAMENTCURRENT:                              // SetFilamentCurrent
    if (!mScope->SetFilamentCurrent(cItemDbl[1]))
      ABORT_NOLINE("An error occurred setting the filament current");
    break;

  case CME_ISPVPRUNNING:                                    // IsPVPRunning
    if (!mScope->IsPVPRunning(cTruth))
      ABORT_NOLINE("An error occurred determining whether the PVP is running");
    cLogRpt.Format("The PVP %s running", cTruth ? "IS" : "is NOT");
    SetReportedValues(&cStrItems[1], cTruth ? 1. : 0.);
    break;

  case CME_SETBEAMBLANK:                                    // SetBeamBlank
    if (cItemEmpty[1])
      ABORT_LINE("Entry requires a number for setting blank on or off: \n\n");
    cIndex = cItemInt[1];
    mScope->BlankBeam(cIndex != 0);
    break;

  case CME_MOVETONAVITEM:                                   // MoveToNavItem
    ABORT_NONAV;
    cIndex = -1;
    if (!cItemEmpty[1]) {
      cIndex = cItemInt[1] - 1;
      if (cIndex < 0)
        ABORT_LINE("The Navigator item index must be positive: \n\n");
    }
    if (cNavigator->MoveToItem(cIndex))
      ABORT_NOLINE("Error moving to Navigator item");
    mMovedStage = true;
    break;

  case CME_REALIGNTONAVITEM:                                // RealignToNavItem
  case CME_REALIGNTOOTHERITEM:                              // RealignToOtherItem
    ABORT_NONAV;
    cTruth = CMD_IS(REALIGNTOOTHERITEM);
    cIndex2 = cTruth ? 2 : 1;
    cIndex = cItemInt[1] - 1;
    cBmax = 0.;
    cIx1 = cIx0 = 0;
    if (!cItemEmpty[cIndex2 + 2]) {
      if (cItemEmpty[cIndex2 + 4])
        ABORT_LINE("Entry requires three values for controlling image shift reset in:"
        "\n\n");
      cBmax = cItemFlt[cIndex2 + 2];
      cIx0 = cItemInt[cIndex2 + 3];
      cIx1 = cItemInt[cIndex2 + 4];
    }
    if (!cItemEmpty[cIndex2 + 1])
      mNavHelper->SetContinuousRealign(cItemInt[cIndex2 + 1]);
    if (cTruth)
      cIy0 = cNavigator->RealignToOtherItem(cIndex, cItemInt[cIndex2] != 0, cBmax, cIx0, cIx1);
    else
      cIy0 = cNavigator->RealignToCurrentItem(cItemInt[cIndex2] != 0, cBmax, cIx0, cIx1);
    if (cIy0) {
      cReport.Format("Script halted due to failure %d in Realign to Item routine", cIy0);
      ABORT_NOLINE(cReport);
      mNavHelper->SetContinuousRealign(0);
    }
    break;

  case CME_REALIGNTOMAPDRAWNON:                             // RealignToMapDrawnOn
    cNavItem = CurrentOrIndexedNavItem(cItemInt[1], cStrLine);
    if (!cNavItem)
      return;
    if (!cNavItem->mDrawnOnMapID)
      ABORT_LINE("The specified item has no ID for being drawn on a map in line:\n\n");
    cIx0 = mNavHelper->RealignToDrawnOnMap(cNavItem, cItemInt[2] != 0);
    if (cIx0) {
      cReport.Format("Script halted due to failure %d in Realign to Item for line:\n\n",
        cIx0);
      ABORT_LINE(cReport);
    }
    break;

  case CME_GETREALIGNTOITEMERROR:                           // GetRealignToItemError
    ABORT_NONAV;
    mNavHelper->GetLastStageError(cBacklashX, cBacklashY, cBmin, cBmax);
    SetReportedValues(&cStrItems[1], cBacklashX, cBacklashY, cBmin, cBmax);
    break;

    // ReportNavItem, ReportOtherItem, ReportNextNavAcqItem, LoadNavMap, LoadOtherMap
  case CME_REPORTNAVITEM:
  case CME_REPORTOTHERITEM:
  case CME_LOADNAVMAP:
  case CME_LOADOTHERMAP:
  case CME_REPORTNEXTNAVACQITEM:
      ABORT_NONAV;
      cTruth = CMD_IS(REPORTNEXTNAVACQITEM);
      if (CMD_IS(REPORTNAVITEM) || CMD_IS(LOADNAVMAP)) {
        cIndex = cNavigator->GetCurrentOrAcquireItem(cNavItem);
        if (cIndex < 0)
          ABORT_LINE("There is no current Navigator item for line:\n\n.");
        cIndex2 = 1;
      } else if (cTruth) {
        if (!cNavigator->GetAcquiring())
          ABORT_LINE("The Navigator must be acquiring for line:\n\n");
        cNavItem = cNavigator->FindNextAcquireItem(cIndex);
        if (cIndex < 0) {
          mWinApp->AppendToLog("There is no next item to be acquired", mLogAction);
          SetVariable("NAVINDEX", "-1", VARTYPE_REGULAR, -1, false);
          SetReportedValues(-1);
        }
      } else {
        if (cItemInt[1] < 0) {
          cIndex = cNavigator->GetNumNavItems() + cItemInt[1];
        } else
          cIndex = cItemInt[1] - 1;
        cNavItem = cNavigator->GetOtherNavItem(cIndex);
        if (!cNavItem)
          ABORT_LINE("Index is out of range in statement:\n\n");
        cIndex2 = 2;
      }

      if (CMD_IS(REPORTNAVITEM) || CMD_IS(REPORTOTHERITEM) || (cTruth && cIndex >= 0)) {
        cLogRpt.Format("%stem %d:  Stage: %.2f %.2f %2.f  Label: %s",
          cTruth ? "Next i" : "I", cIndex + 1, cNavItem->mStageX, cNavItem->mStageY,
          cNavItem->mStageZ, (LPCTSTR)cNavItem->mLabel);
        if (!cNavItem->mNote.IsEmpty())
          cLogRpt += "\r\n    Note: " + cNavItem->mNote;
        SetReportedValues(cIndex + 1., cNavItem->mStageX, cNavItem->mStageY,
         cNavItem->mStageZ, (double)cNavItem->mType);
        cReport.Format("%d", cIndex + 1);
        SetVariable("NAVINDEX", cReport, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVLABEL", cNavItem->mLabel, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVNOTE", cNavItem->mNote, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVCOLOR", cNavItem->mColor, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVREGIS", cNavItem->mRegistration, VARTYPE_REGULAR, -1, false);
        cIndex = atoi(cNavItem->mLabel);
        cReport.Format("%d", cIndex);
        SetVariable("NAVINTLABEL", cReport, VARTYPE_REGULAR, -1, false);
        if (cNavigator->GetAcquiring()) {
          cReport.Format("%d", cNavigator->GetNumAcquired() + (cTruth ? 2 : 1));
          SetVariable("NAVACQINDEX", cReport, VARTYPE_REGULAR, -1, false);
        }
      } else if (!cTruth) {
        if (cNavItem->mType != ITEM_TYPE_MAP)
          ABORT_LINE("The Navigator item is not a map for line:\n\n");
        if (ConvertBufferLetter(cStrItems[cIndex2], mBufferManager->GetBufToReadInto(),
          false, cIx0, cReport))
          ABORT_LINE(cReport);
        cNavigator->DoLoadMap(false, cNavItem, cIx0);
        mLoadingMap = true;
      }
    break;

  case CME_REPORTITEMACQUIRE:                               // ReportItemAcquire
    cIndex = cItemEmpty[1] ? 0 : cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
   cLogRpt.Format("Navigator item %d has Acquire %s", cIndex + 1,
      (cNavItem->mAcquire == 0) ? "disabled" : "enabled");
    SetReportedValues(cNavItem->mAcquire);
    break;

  case CME_SETITEMACQUIRE:                                  // SetItemAcquire
    cIndex = cItemEmpty[1] ? 0 : cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    if (cNavigator->GetAcquiring() && cIndex >= cNavigator->GetAcquireIndex() &&
      cIndex <= cNavigator->GetEndingAcquireIndex())
      ABORT_NOLINE("When the Navigator is acquiring, you cannot set an\n"
        "item to Acquire within the range still being acquired");
    cTruth = (cItemEmpty[2] || (cItemInt[2] != 0));
    if (cTruth && cNavItem->mTSparamIndex >= 0)
      ABORT_LINE("You cannot turn on Acquire for an item set for a tilt series for "
        "line:\n\n")
    cNavItem->mAcquire = cTruth;
    cNavigator->UpdateListString(cIndex);
    cNavigator->Redraw();
    cLogRpt.Format("Navigator item %d Acquire set to %s", cIndex + 1,
      cTruth ? "enabled" : "disabled");
    cNavigator->SetChanged(true);
    mLoopInOnIdle = !cNavigator->GetAcquiring();
    break;

  case CME_NAVINDEXWITHLABEL:                               // NavIndexWithLabel
  case CME_NAVINDEXWITHNOTE:                                // NavIndexWithNote
    ABORT_NONAV;
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    cTruth = CMD_IS(NAVINDEXWITHNOTE);
    cNavigator->FindItemWithString(cStrCopy, cTruth);
    cIndex = cNavigator->GetFoundItem() + 1;
    if (cIndex > 0)
      cLogRpt.Format("Item with %s %s has index %d", cTruth ? "note" : "label",
        (LPCTSTR)cStrCopy, cIndex);
    else
      cLogRpt.Format("No item has %s %s", cTruth ? "note" : "label", (LPCTSTR)cStrCopy);
    SetReportedValues(cIndex);
    break;

  case CME_NAVINDEXITEMDRAWNON:                             // NavIndexItemDrawnOn
    cIndex = cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    cIndex2 = 0;
    if (!cNavItem->mDrawnOnMapID) {
      cLogRpt.Format("Navigator item %d does not have an ID for being drawn on a map",
        cIndex + 1);
    } else {
      cNavItem = cNavigator->FindItemWithMapID(cNavItem->mDrawnOnMapID, true);
      if (!cNavItem) {
        cLogRpt.Format("The map that navigator item %d was drawn on is no longer in the "
          "table", cIndex + 1);
      } else {
        cIndex2 = cNavigator->GetFoundItem() + 1;
        cLogRpt.Format("Navigator item %d was drawn on map item %d", cIndex + 1, cIndex2);
      }
    }
    SetReportedValues(cIndex2);
    break;

  case CME_NAVITEMFILETOOPEN:                               // NavItemFileToOpen
    cIndex = cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    cReport = cNavItem->mFileToOpen;
    if (cReport.IsEmpty()) {
      cLogRpt.Format("No file is set to be opened for Navigator item %d", cIndex + 1);
      cReport = "0";
    } else {
      cLogRpt.Format("File to open at Navigator item %d is: %s", cIndex + 1,
        (LPCTSTR)cReport);
    }
    SetOneReportedValue(cReport, 1);
    break;

  case CME_SETNAVITEMUSERVALUE:                             // SetNavItemUserValue
  case CME_REPORTITEMUSERVALUE:                             // ReportItemUserValue
    cIndex = cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    cIndex2 = cItemInt[2];
    if (cIndex2 < 1 || cIndex2 > MAX_NAV_USER_VALUES) {
      cReport.Format("The user value number must be between 1 and %d in line:\n\n",
        MAX_NAV_USER_VALUES);
      ABORT_LINE(cReport);
    }
    if (CMD_IS(SETNAVITEMUSERVALUE)) {
      SubstituteLineStripItems(cStrLine, 3, cStrCopy);
      mNavHelper->SetUserValue(cNavItem, cIndex2, cStrCopy);
      cNavigator->SetChanged(true);
    } else {
      if (mNavHelper->GetUserValue(cNavItem, cIndex2, cStrCopy)) {
        cLogRpt.Format("Navigator item %d has no user value # %d", cIndex + 1, cIndex2);
        cStrCopy = "none";
      } else {
        cLogRpt.Format("User value # %d for Navigator item %d is %s", cIndex2, cIndex + 1,
          (LPCTSTR)cStrCopy);
      }
      SetOneReportedValue(cStrCopy, 1);
    }
    break;

  case CME_SETMAPACQUIRESTATE:                              // SetMapAcquireState
    ABORT_NONAV;
    cNavItem = cNavigator->GetOtherNavItem(cItemInt[1] - 1);
    if (!cNavItem)
      ABORT_LINE("Index is out of range in statement:\n\n");
    if (cNavItem->mType != ITEM_TYPE_MAP) {
      cReport.Format("Navigator item %d is not a map for line:\n\n", cItemInt[1]);
      ABORT_LINE(cReport);
    }
    if (mNavHelper->SetToMapImagingState(cNavItem, true))
      ABORT_LINE("Failed to set map imaging state for line:\n\n");
    break;

  case CME_RESTORESTATE:                                    // RestoreState
    cIndex = mNavHelper->GetTypeOfSavedState();
    if (cIndex == STATE_NONE) {
      cReport.Format("Cannot Restore State: no state has been saved");
      if (cItemInt[1])
        ABORT_LINE(cReport);
      mWinApp->AppendToLog(cReport, mLogAction);
    } else {
      if (cIndex == STATE_MAP_ACQUIRE)
        mNavHelper->RestoreFromMapState();
      else {
        mNavHelper->RestoreSavedState();
        if (mNavHelper->mStateDlg)
          mNavHelper->mStateDlg->Update();
      }
      if (mNavHelper->mStateDlg)
        mNavHelper->mStateDlg->DisableUpdateButton();
    }
    break;

  case CME_REPORTNUMNAVACQUIRE:                             // ReportNumNavAcquire
    mNavHelper->CountAcquireItems(0, -1, cIndex, cIndex2);
    if (cIndex < 0)
      cLogRpt = "The Navigator is not open; there are no acquire items";
    else
      cLogRpt.Format("Navigator has %d Acquire items, %d Tilt Series items", cIndex, cIndex2);
    SetReportedValues(&cStrItems[1], (double)cIndex, (double)cIndex2);
    break;

  case CME_REPORTNUMTABLEITEMS:                             // ReportNumTableItems
    ABORT_NONAV;
    cIndex = cNavigator->GetNumberOfItems();
    cLogRpt.Format("Navigator table has %d items", cIndex);
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_SETNAVREGISTRATION:                              // SetNavRegistration
    ABORT_NONAV;
    if (cNavigator->SetCurrentRegistration(cItemInt[1]))
      ABORT_LINE("New registration number is out of range or used for imported items "
      "in:\n\n");
    cNavigator->SetChanged(true);
    break;

  case CME_SAVENAVIGATOR:                                   // SaveNavigator
    ABORT_NONAV;
    cNavigator->DoSave();
    break;

  case CME_REPORTIFNAVOPEN:                                 // ReportIfNavOpen
    cIndex = 0;
    cLogRpt = "Navigator is NOT open";
    if (mWinApp->mNavigator) {
      cIndex = 1;
      cLogRpt = "Navigator IS open";
      if (mWinApp->mNavigator->GetCurrentNavFile()) {
        cLogRpt += "; file is defined";
        cIndex = 2;
      }
    }
    SetReportedValues(&cStrItems[1], (double)cIndex);
    break;

  case CME_READNAVFILE:                                     // ReadNavFile
  case CME_MERGENAVFILE:                                    // MergeNavFile
    cTruth = CMD_IS(MERGENAVFILE);
    if (cTruth) {
      ABORT_NONAV;
    } else  {
      mWinApp->mMenuTargets.OpenNavigatorIfClosed();
    }
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    if (!CFile::GetStatus((LPCTSTR)cStrCopy, cStatus))
      ABORT_LINE("The file " + cStrCopy + " does not exist in line:\n\n");
    if (mWinApp->mNavigator->LoadNavFile(false, CMD_IS(MERGENAVFILE), &cStrCopy))
      ABORT_LINE("Script stopped due to error processing Navigator file for line:\n\n");
    break;


  case CME_CHANGEITEMREGISTRATION:                          // ChangeItemRegistration
  case CME_CHANGEITEMCOLOR:                                 // ChangeItemColor
  case CME_CHANGEITEMLABEL:                                 // ChangeItemLabel
  case CME_CHANGEITEMNOTE:                                  // ChangeItemNote
      ABORT_NONAV;
      cIndex = cItemInt[1];
      cIndex2 = cItemInt[2];
      cNavItem = cNavigator->GetOtherNavItem(cIndex - 1);
      cReport.Format("The Navigator item index, %d, is out of range in:\n\n", cIndex);
      if (!cNavItem)
        ABORT_LINE(cReport);
      if (CMD_IS(CHANGEITEMREGISTRATION)) {
        cReport.Format("The Navigator item with index %d is a registration point in:\n\n",
          cIndex);
        if (cNavItem->mRegPoint)
          ABORT_LINE(cReport);
        if (cNavigator->ChangeItemRegistration(cIndex - 1, cIndex2, cReport))
          ABORT_LINE(cReport + " in line:\n\n");
      } else {
        if (CMD_IS(CHANGEITEMCOLOR)) {
          cReport.Format("The Navigator item color must be between 0 and %d in:\n\n",
            NUM_ITEM_COLORS - 1);
          if (cIndex2 < 0 || cIndex2 >= NUM_ITEM_COLORS)
            ABORT_LINE(cReport);
          cNavItem->mColor = cIndex2;
        } else if (CMD_IS(CHANGEITEMNOTE)) {
          SubstituteVariables(&cStrLine, 1, cStrLine);
          if (cItemEmpty[2])
            cStrCopy = "";
          else
            mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
          cNavItem->mNote = cStrCopy;
        } else {
          SubstituteVariables(&cStrLine, 1, cStrLine);
          mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
          cReport.Format("The Navigator label size must be no more than %d characters "
            "in:\n\n", MAX_LABEL_SIZE);
          if (cStrCopy.GetLength() > MAX_LABEL_SIZE)
            ABORT_LINE(cReport);
          cNavItem->mLabel = cStrCopy;
        }
        cNavigator->SetChanged(true);
        cNavigator->UpdateListString(cIndex - 1);
        cNavigator->Redraw();
      }
    break;

  case CME_SETITEMTARGETDEFOCUS:                            // SetItemTargetDefocus
    cIndex = cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    if (!cNavItem->mAcquire && cNavItem->mTSparamIndex < 0)
      ABORT_LINE("Specified Navigator item is not marked for acquisition in line:\n\n");
    if (cItemFlt[2] < -20 || cItemFlt[2] > 0)
      ABORT_LINE("Target defocus must be between 0 and -20 in line:\n\n:");
    cNavItem->mTargetDefocus = cItemFlt[2];
    cNavigator->SetChanged(true);
    cLogRpt.Format("For Navigator item %d, target defocus set to %.2f", cIndex + 1,
      cItemFlt[2]);
    break;

  case CME_SETITEMSERIESANGLES:                             // SetItemSeriesAngles
    cIndex = cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    if (cNavItem->mTSparamIndex < 0)
      ABORT_LINE("The specified Navigator item is not marked for a tilt series"
        " in line:\n\n");
    if (mNavHelper->CheckTiltSeriesAngles(cNavItem->mTSparamIndex, cItemFlt[2], cItemFlt[3],
      cItemEmpty[4] ? EXTRA_NO_VALUE : cItemFlt[4], cReport))
      ABORT_LINE(cReport + "in line:\n\n");
    cNavItem->mTSstartAngle = cItemFlt[2];
    cNavItem->mTSendAngle = cItemFlt[3];
    cLogRpt.Format("For Navigator item %d, tilt series range set to %.1f to %.1f",
      cIndex + 1, cItemFlt[2], cItemFlt[3]);
    if (!cItemEmpty[4]) {
      cNavItem->mTSbidirAngle = cItemFlt[4];
      cReport.Format(", start %.1f", cItemFlt[4]);
      cLogRpt += cReport;
    }
    cNavigator->SetChanged(true);
    break;

  case CME_SKIPACQUIRINGNAVITEM:                            // SkipAcquiringNavItem
    ABORT_NONAV;
    if (!cNavigator->GetAcquiring())
      mWinApp->AppendToLog("SkipAcquiringNavItem has no effect except from a\r\n"
      "    pre-script when acquiring Navigator items", mLogAction);
    cNavigator->SetSkipAcquiringItem(cItemEmpty[1] || cItemInt[1] != 0);
    break;

  case CME_SKIPACQUIRINGGROUP:                              // SkipAcquiringGroup
    ABORT_NONAV;
    if (!cNavigator->GetAcquiring())
      ABORT_NOLINE("The navigator must be acquiring to set a group ID to skip");
    if (cItemEmpty[1]) {
      cIndex2 = cNavigator->GetCurrentOrAcquireItem(cNavItem);
      cIndex = cNavItem->mGroupID;
    } else {
      cIndex = cItemInt[1];
    }
    cNavigator->SetGroupIDtoSkip(cIndex);
    break;

  case CME_SKIPMOVEINNAVACQUIRE:                            // SkipMoveInNavAcquire
    ABORT_NONAV;
    if (!cNavigator->GetAcquiring())
      ABORT_NOLINE("The navigator must be acquiring to enable skipping the stage move");
    cNavigator->SetSkipStageMoveInAcquire(cItemEmpty[1] || cItemInt[1] != 0);
    break;

  case CME_SUSPENDNAVREDRAW:                                // SuspendNavRedraw
    ABORT_NONAV;
    mSuspendNavRedraw = cItemEmpty[1] || cItemInt[1] != 0;
    if (!mSuspendNavRedraw) {
      cNavigator->FillListBox(false, true);
      cNavigator->Redraw();
    }
    break;

  case CME_STARTNAVACQUIREATEND:                            // StartNavAcquireAtEnd
    ABORT_NONAV;
    mStartNavAcqAtEnd = cItemEmpty[1] || cItemInt[1] != 0;
    if (mStartNavAcqAtEnd && (mWinApp->DoingTiltSeries() || cNavigator->GetAcquiring()))
      ABORT_NOLINE(CString("You cannot use StartNavAcquireAtEnd when ") +
        (mWinApp->DoingTiltSeries() ? "a tilt series is running" :
        "Navigator is already acquiring"));
    break;

  case CME_SUFFIXFOREXTRAFILE:                              // SuffixForExtraFile
    ABORT_NONAV;
    cNavigator->SetExtraFileSuffixes(&cStrItems[1],
      B3DMIN(cLastNonEmptyInd, MAX_STORES - 1));
    break;

  case CME_ITEMFORSUPERCOORD:                               // ItemForSuperCoord
    ABORT_NONAV;
    cNavigator->SetSuperCoordIndex(cItemInt[1] - 1);
    break;

  case CME_UPDATEITEMZ:
  case CME_UPDATEGROUPZ:  // UpdateItemZ, UpdateGroupZ
    ABORT_NONAV;
    cIndex2 = CMD_IS(UPDATEGROUPZ) ? 1 : 0;
    cIndex = cNavigator->GetCurrentOrAcquireItem(cNavItem);
    if (cIndex < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    cIndex = cNavigator->DoUpdateZ(cIndex, cIndex2);
    if (cIndex == 3)
      ABORT_LINE("Current Navigator item is not in a group for statement:\n\n");
    if (cIndex)
      ABORT_LINE("Error updating Z of Navigator item in statement:\n\n");
    cNavigator->SetChanged(true);
    break;

  case CME_REPORTGROUPSTATUS:                               // ReportGroupStatus
    ABORT_NONAV;
    cIndex = cNavigator->GetCurrentOrAcquireItem(cNavItem);
    if (cIndex < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    cIndex2 = cNavItem->mGroupID;
    cIndex = -1;
    cIx0 = cNavigator->CountItemsInGroup(cIndex2, cReport, cStrCopy, cIx1);
    if (cNavigator->GetAcquiring()) {
      cIndex = 0;
      if (cIndex2)
        cIndex = cNavigator->GetFirstInGroup() ? 1 : 2;
    }
    cLogRpt.Format("Group acquire status %d, group ID %d, # in group %d, %d set to acquire"
      , cIndex, cIndex2, cIx0, cIx1);
    SetReportedValues(&cStrItems[1], (double)cIndex, (double)cIndex2, (double)cIx0,
      (double)cIx1);
    break;

  case CME_REPORTITEMIMAGECOORDS:                            // ReportItemImageCoords
    cIndex = cItemEmpty[1] ? 0 : cItemInt[1];
    cNavItem = CurrentOrIndexedNavItem(cIndex, cStrLine);
    if (!cNavItem)
      return;
    if (ConvertBufferLetter(cStrItems[2], 0, true, cIndex2, cReport))
      ABORT_LINE(cReport);
    if (mWinApp->mMainView->GetMapItemsForImageCoords(&mImBufs[cIndex2], true)) {
      mWinApp->mMainView->GetItemImageCoords(&mImBufs[cIndex2], cNavItem, cFloatX,
        cFloatY);
      mImBufs[cIndex2].mImage->getSize(cSizeX, cSizeY);
      if (cFloatX >= 0 && cFloatY >= 0 && cFloatX <= (cSizeX - 1) &&
        cFloatY <= (cSizeY - 1)) {
        SetReportedValues(cFloatX, cFloatY, 1);
        cLogRpt.Format("Navigator item %d has image coordinates %.2f, %.2f on %c",
          cIndex + 1, cFloatX, cFloatY, cIndex2 + 65);
      } else {
        SetReportedValues(cFloatX, cFloatY, 0);
        cLogRpt.Format("Navigator item %d is outside of %c", cIndex + 1, cIndex2 + 65);
      }
    } else {
      SetReportedValues(0.0, 0.0, -1);
      cLogRpt.Format("Navigator item %d could not be transformed to %c", cIndex + 1,
        cIndex2 + 65);
    }
    break;

  case CME_NEWMAP:                                          // NewMap
    ABORT_NONAV;
    cNavigator->SetSkipBacklashType(1);
    cIndex = 0;
    if (!cItemEmpty[1]) {
      cIndex = cItemInt[1];
      if (cItemEmpty[2])
        ABORT_LINE("There must be text for the Navigator note after the number in:\n\n");
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 2, cStrCopy);
    }
    if (cNavigator->NewMap(false, cIndex, cItemEmpty[1] ? NULL : &cStrCopy)) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    SetReportedValues(&cStrItems[1], (double)cNavigator->GetNumberOfItems());
    break;

  case CME_MAKEANCHORMAP:                                   // MakeAnchorMap
    ABORT_NONAV;
    if (cNavigator->DoMakeDualMap()) {
      AbortMacro();
      return;
    }
    mMakingDualMap = true;
    break;

  case CME_SHIFTITEMSBYALIGNMENT:                           // ShiftItemsByAlignment
    ABORT_NONAV;
    if (cNavigator->ShiftItemsByAlign()) {
      AbortMacro();
      return;
    }
    cNavigator->SetChanged(true);
    break;

  case CME_SHIFTITEMSBYCURRENTDIFF:                       // ShiftItemsByCurrentDiff
    ABORT_NONAV;
    cIndex = cNavigator->GetCurrentOrAcquireItem(cNavItem);
    if (cIndex < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    mScope->GetStagePosition(cStageX, cStageY, cStageZ);
    cShiftX = (float)cStageX;
    cShiftY = (float)cStageY;
    mScope->GetImageShift(cDelISX, cDelISY);
    cIndex2 = mScope->GetLowDoseArea();
    if (mWinApp->LowDoseMode() && (cIndex2 == VIEW_CONSET || cIndex2 == SEARCH_AREA)) {
      mWinApp->mLowDoseDlg.GetNetViewShift(cStageX, cStageY, cIndex2);
      cDelISX -= cStageX;
      cDelISY -= cStageY;
    }
    cNavigator->ConvertIStoStageIncrement(mScope->FastMagIndex(),
        cCurrentCam, cDelISX, cDelISY, (float)mScope->FastTiltAngle(),
        cShiftX, cShiftY);
    cShiftX -= cNavItem->mStageX;
    cShiftY -= cNavItem->mStageY;
    cSpecDist = sqrt(cShiftX * cShiftX + cShiftY * cShiftY);
    if (cSpecDist <= cItemDbl[1]) {
      cNavigator->ShiftItemsAtRegistration(cShiftX, cShiftY,
        cNavItem->mRegistration);
      PrintfToLog("Items at registration %d shifted by %.2f, %.2f",
        cNavItem->mRegistration, cShiftX, cShiftY);
      cNavigator->SetChanged(true);
    } else {
      PrintfToLog("Current stage position is too far from item position (%.2f microns);"
        "nothing was shifted", cSpecDist);
    }
    break;

  case CME_SHIFTITEMSBYMICRONS:                             // ShiftItemsByMicrons
    ABORT_NONAV;
    if (fabs(cItemDbl[1]) > 100. || fabs(cItemDbl[2]) > 100.)
      ABORT_LINE("You cannot shift items by more than 100 microns in:\n\n");
    cIndex = cNavigator->GetCurrentRegistration();
    if (!cItemEmpty[3]) {
      cIndex = cItemInt[3];
      cReport.Format("Registration number %d is out of range in:\n\n", cIndex);
      if (cIndex <= 0 || cIndex > MAX_CURRENT_REG)
        ABORT_LINE(cReport);
    }
    cIndex2 = cNavigator->ShiftItemsAtRegistration(cItemFlt[1], cItemFlt[2],
      cIndex);
    cLogRpt.Format("%d items at registration %d were shifted by %.2f, %.2f", cIndex2, cIndex,
      cItemDbl[1], cItemDbl[2]);
    cNavigator->SetChanged(true);
    break;

  case CME_ALIGNANDTRANSFORMITEMS:                          // AlignAndTransformItems
    cIndex2 = mNavHelper->BufferForRotAlign(cIndex);
    if (cIndex != cNavigator->GetCurrentRegistration()) {
      cReport.Format("Image in buffer %c is at registration %d, not the current Navigator"
        " registration, %d, for line:\n\n", 'A' + cIndex2, cIndex,
        cNavigator->GetCurrentRegistration());
      ABORT_LINE(cReport);
    }
    cIx0 = CNavRotAlignDlg::CheckAndSetupReference();
    if (cIx0 < 0)
      ABORT_LINE("Cannot find a Navigator item corresponding to the image in the Read"
        " buffer for line:\n\n");
    if (cIx0 == cIndex)
      ABORT_LINE("The image in the Read buffer is at the current Navigator"
        " registration for line:\n\n");
    if (fabs(cItemDbl[2]) < 0.2 || fabs(cItemDbl[2]) > 50.)
      ABORT_LINE("Angular range to search must be between 0.2 and 50 degrees for "
        "line:\n\n");
    CNavRotAlignDlg::PrepareToUseImage();
    if (CNavRotAlignDlg::AlignToMap(cItemFlt[1], cItemFlt[2], cBacklashX)) {
      AbortMacro();
      return;
    }
    if (!cItemEmpty[3] && fabs(cBacklashX - cItemDbl[1]) > cItemDbl[3]) {
      cLogRpt.Format("The rotation found by alignment, %.1f degrees, is farther from\r\n"
        " the center angle than the specified limit, nothing was transformed", cBacklashX);
    } else {
      cIndex = CNavRotAlignDlg::TransformItemsFromAlignment();
      cLogRpt.Format("%d items transformed after alignment with rotation of %.1f degrees",
        cIndex, cBacklashX);
      cNavigator->SetChanged(true);
    }
    SetReportedValues(cBacklashX);
    break;

  case CME_FORCECENTERREALIGN:                              // ForceCenterRealign
    ABORT_NONAV;
    mNavHelper->ForceCenterRealign();
    break;

  case CME_SKIPPIECESOUTSIDEITEM:                           // SkipPiecesOutsideItem
    if (!mWinApp->Montaging())
      ABORT_LINE("Montaging must be on already to use this command:\n\n");
    if (cItemInt[1] >= 0)
      cMontP->insideNavItem = cItemInt[1] - 1;
    cMontP->skipOutsidePoly = cItemInt[1] >= 0;
    break;

  case CME_FINDHOLES:                                       // FindHoles
    ABORT_NONAV;
    cImBuf = mWinApp->mMainView->GetActiveImBuf();
    if (!cItemEmpty[1]) {
      if (ConvertBufferLetter(cStrItems[1], -1, true, cIndex, cReport))
        ABORT_LINE(cReport);
      cImBuf = &mImBufs[cIndex];
    }
    if (mNavHelper->mHoleFinderDlg->DoFindHoles(cImBuf)) {
      AbortMacro();
      return;
    }
    break;

  case CME_MAKENAVPOINTSATHOLES:                            //  MakeNavPointsAtHoles
    ABORT_NONAV;
    cIndex = cItemEmpty[1] || cItemInt[1] < 0 ? - 1 : cItemInt[1];
    if (cIndex > 2)
      ABORT_LINE("The layout type must be less than 3 for line:\n\n");
    cIndex = mNavHelper->mHoleFinderDlg->DoMakeNavPoints(cIndex,
      (float)((cItemEmpty[2] || cItemDbl[2] < -900000) ? EXTRA_NO_VALUE : cItemDbl[2]),
      (float)((cItemEmpty[3] || cItemDbl[3] < -900000) ? EXTRA_NO_VALUE : cItemDbl[3]),
      (float)((cItemEmpty[4] || cItemDbl[4] < 0.) ? - 1. : cItemDbl[4]) / 100.f,
      (float)((cItemEmpty[5] || cItemDbl[5] < 0.) ? - 1. : cItemDbl[5]) / 100.f);
    if (cIndex < 0) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Hole finder made %d Navigator points", cIndex);
    break;

  case CME_CLEARHOLEFINDER:                                 //    ClearHoleFinder
    mNavHelper->mHoleFinderDlg->OnButClearData();
    break;

  case CME_COMBINEHOLESTOMULTI:                             // CombineHolesToMulti
    ABORT_NONAV;
    B3DCLAMP(cItemInt[1], 0, 2);
    cIndex = mNavHelper->mCombineHoles->CombineItems(cItemInt[1]);
    if (cIndex)
      ABORT_NOLINE("Error trying to combine hole for multiple Records:\n" +
        CString(mNavHelper->mCombineHoles->GetErrorMessage(cIndex)));
    break;

  case CME_UNDOHOLECOMBINING:                               // UndoHoleCombining
    ABORT_NONAV;
    if (!mWinApp->mNavHelper->mCombineHoles->OKtoUndoCombine())
      ABORT_NOLINE("It is no longer possible to undo the last hole combination");
    mWinApp->mNavHelper->mCombineHoles->UndoCombination();
    break;

  case CME_SETHELPERPARAMS:                                 // SetHelperParams
    mNavHelper->SetTestParams(&cItemDbl[1]);
    break;

  case CME_SETMONTAGEPARAMS:                                // SetMontageParams
    if (!mWinApp->Montaging())
      ABORT_LINE("Montaging must be on already to use this command:\n\n");
    if (mWinApp->mStoreMRC && mWinApp->mStoreMRC->getDepth() > 0 &&
      ((cItemInt[2] > 0) || (cItemInt[3] > 0) ||
      (cItemInt[4] > 0) || (cItemInt[5] > 0)))
        ABORT_LINE("Atfer writing to the file, you cannot change frame size or overlaps "
          " in line:\n\n");
    if (cItemInt[1] >= 0)
      cMontP->moveStage = cItemInt[1] > 0;
    if (cItemInt[4] > 0) {
      if (cItemInt[4] < cMontP->xOverlap * 2)
        ABORT_LINE("The X frame size is less than twice the overlap in statement:\n\n");
      cMontP->xFrame = cItemInt[4];
    }
    if (cItemInt[5] > 0) {
      if (cItemInt[5] < cMontP->yOverlap * 2)
        ABORT_LINE("The Y frame size is less than twice the overlap in statement:\n\n");
      cMontP->yFrame = cItemInt[5];
    }
    if (cItemInt[2] > 0) {
      if (cItemInt[2] > cMontP->xFrame / 2)
        ABORT_LINE("X overlap is more than half the frame size in statement:\n\n");
      cMontP->xOverlap = cItemInt[2];
    }
    if (cItemInt[3] > 0) {
      if (cItemInt[3] > cMontP->yFrame / 2)
        ABORT_LINE("Y overlap is more than half the frame size in statement:\n\n");
      cMontP->yOverlap = cItemInt[3];
    }
    if (!cItemEmpty[6] && cItemInt[6] >= 0)
      cMontP->skipCorrelations = cItemInt[6] != 0;
    if (!cItemEmpty[7] && cItemDbl[7] >= 0.5) {
      if (CheckCameraBinning(cItemDbl[7], cIndex, cReport))
        ABORT_LINE(cReport);
      cMontP->binning = cIndex;
    }
    mWinApp->mMontageWindow.UpdateSettings();
    break;

  case CME_MANUALFILMEXPOSURE:                              // ManualFilmExposure
    cDelX = cItemDbl[1];
    mScope->SetManualExposure(cDelX);
    break;

  case CME_EXPOSEFILM:                                      // ExposeFilm
  case CME_SPECIALEXPOSEFILM:                               // SpecialExposeFilm
    cDelX = 0.;
    cDelY = 0.;
    cIndex = 0;
    if (CMD_IS(SPECIALEXPOSEFILM)) {
      cDelX = cItemDbl[1];
      if (!cItemEmpty[2]) cDelY = cItemDbl[2];
      if (cDelX < 0. || cDelY < 0. || cItemEmpty[1])
        ABORT_LINE("There must be one or two non-negative values in statement:\n\n");
      if (!cItemEmpty[3])
        cIndex = cItemInt[3];
    }
    if (!mScope->TakeFilmExposure(CMD_IS(SPECIALEXPOSEFILM), cDelX, cDelY, cIndex != 0)) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    mExposedFilm = true;
    break;

  case CME_GOTOLOWDOSEAREA:                                 // GoToLowDoseArea
    if (CheckAndConvertLDAreaLetter(cStrItems[1], 1, cIndex, cStrLine))
      return;
    mScope->GotoLowDoseArea(cIndex);
    break;

  case CME_SETLDCONTINUOUSUPDATE:                           // SetLDContinuousUpdate
    if (mWinApp->mTSController->DoingTiltSeries())
      ABORT_NOLINE("You cannot use SetLDContinuousUpdate during a tilt series");
    mWinApp->mLowDoseDlg.SetContinuousUpdate(cItemInt[1] != 0);
    break;

  case CME_SETLOWDOSEMODE:                                  // SetLowDoseMode
    cIndex = mWinApp->LowDoseMode() ? 1 : 0;
    if (cIndex != (cItemInt[1] ? 1 : 0))
      mWinApp->mLowDoseDlg.SetLowDoseMode(cItemInt[1] != 0);
    SetReportedValues(&cStrItems[2], (double)cIndex);
    break;

  case CME_SETAXISPOSITION:                                 // SetAxisPosition
  case CME_REPORTAXISPOSITION:                              // ReportAxisPosition
    if (CheckAndConvertLDAreaLetter(cStrItems[1], 1, cIndex, cStrLine))
      return;
    if ((cIndex + 1) / 2 != 1)
      ABORT_LINE("This command must be followed by F or T:\n\n");
    if (CMD_IS(REPORTAXISPOSITION)) {
      cDelX = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(cLdParam[cIndex].magIndex,
        cLdParam[cIndex].ISX, cLdParam[cIndex].ISY);
      cIx0 = ((mConSets[cIndex].right + mConSets[cIndex].left) / 2 - cCamParams->sizeX / 2) /
        BinDivisorI(cCamParams);
      cIy0 = ((mConSets[cIndex].bottom + mConSets[cIndex].top) / 2 - cCamParams->sizeY / 2) /
        BinDivisorI(cCamParams);
      cIndex2 = mWinApp->mLowDoseDlg.m_bRotateAxis ? mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
      cLogRpt.Format("%s axis position %.2f microns, %d degrees; camera offset %d, %d "
        "unbinned pixels", mModeNames[cIndex], cDelX, cIndex2, cIx0, cIy0);
      SetReportedValues(&cStrItems[2], cDelX, (double)cIndex2, (double)cIx0, (double)cIy0);

    } else {
      cIndex2 = 0;
      if (fabs(cItemDbl[2]) > 19.)
        ABORT_LINE("The axis distance is too large in:\n\n");
      if (!cItemEmpty[3])
        cIndex2 = B3DNINT(UtilGoodAngle(cItemDbl[3]));
      cIx0 = (mScope->GetLowDoseArea() + 1) / 2;
      if (cIx0 == 1)
        mScope->GetLDCenteredShift(cDelX, cDelY);
      mWinApp->mLowDoseDlg.NewAxisPosition(cIndex, cItemDbl[2], cIndex2, !cItemEmpty[3]);
      if (cIx0 == 1)
        mScope->SetLDCenteredShift(cDelX, cDelY);
    }
    break;

  case CME_REPORTLOWDOSE:                                   // ReportLowDose
  {
    char * modeLets = "VFTRS";
    cIndex = mWinApp->LowDoseMode() ? 1 : 0;
    cIndex2 = mScope->GetLowDoseArea();
    cLogRpt.Format("Low Dose is %s%s%c", cIndex ? "ON" : "OFF",
      cIndex && cIndex2 >= 0 ? " in " : "", cIndex && cIndex2 >= 0 ? modeLets[cIndex2] : ' ');
    SetReportedValues(&cStrItems[1], (double)cIndex, (double)cIndex2);
    break;
  }

  case CME_CURRENTSETTINGSTOLDAREA:                         // CurrentSettingsToLDArea
    if (CheckAndConvertLDAreaLetter(cStrItems[1], -1, cIndex, cStrLine))
      return;
    mWinApp->InitializeOneLDParam(cLdParam[cIndex]);
    mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    mScope->GotoLowDoseArea(cIndex);
    break;

  case CME_UPDATELOWDOSEPARAMS:                             // UpdateLowDoseParams
    if (mWinApp->mTSController->DoingTiltSeries())
      ABORT_NOLINE("You cannot use ChangeLowDoseParams during a tilt series");
    if (CheckAndConvertLDAreaLetter(cStrItems[1], 0, cIndex, cStrLine))
      return;
    if (!mKeepOrRestoreArea[cIndex]) {
      mLDParamsSaved.push_back(cLdParam[cIndex]);
      mLDareasSaved.push_back(cIndex);
      mKeepOrRestoreArea[cIndex] = (cItemEmpty[2] || !cItemInt[2]) ? 1 : -1;
      UpdateLDAreaIfSaved();
    }
    break;

  case CME_RESTORELOWDOSEPARAMS:                            // RestoreLowDoseParams
    if (CheckAndConvertLDAreaLetter(cStrItems[1], 0, cIndex, cStrLine))
      return;
    RestoreLowDoseParams(cIndex);
    break;

  case CME_SETLDADDEDBEAMBUTTON:                            // SetLDAddedBeamButton
    cTruth = cItemEmpty[1] || cItemInt[1];
    if (!mWinApp->LowDoseMode() || mScope->GetLowDoseArea() < 0)
      ABORT_LINE("You must be in Low Dose mode and in a defined area for line:\n\n");
    if (mLDSetAddedBeamRestore < 0) {
      mLDSetAddedBeamRestore = mWinApp->mLowDoseDlg.m_bSetBeamShift ? 1 : 0;
      mNumStatesToRestore++;
    }
    mWinApp->mLowDoseDlg.SetBeamShiftButton(cTruth);
    if ((cTruth ? 1 : 0) == mLDSetAddedBeamRestore) {
      mLDSetAddedBeamRestore = -1;
      mNumStatesToRestore--;
    }
    break;

  case CME_SHOWMESSAGEONSCOPE:                              // ShowMessageOnScope
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    if (!FEIscope)
      ABORT_LINE("This command can be run only on an FEI scope");
    if (mScope->GetPluginVersion() < FEI_PLUGIN_MESSAGE_BOX) {
      cReport.Format("You need to upgrade the FEI plugin and/or server\n"
        "to at least version %d get a message box in:\n\n", FEI_PLUGIN_MESSAGE_BOX);
      ABORT_LINE(cReport);
    }
    mScope->SetMessageBoxArgs(mBoxOnScopeType, cStrCopy, mBoxOnScopeText);
    cIy1 = LONG_OP_MESSAGE_BOX;
    cIndex = mScope->StartLongOperation(&cIy1, &mBoxOnScopeInterval, 1);
    if (cIndex > 0)
      ABORT_LINE("The thread is already busy for a long operation so cannot run:\n\n");
    if (!cIndex) {
      mStartedLongOp = true;
      mShowedScopeBox = true;
    } else {
      SetReportedValues(0.);
    }
    break;

  case CME_SETUPSCOPEMESSAGE:                               // SetupScopeMessage
    mBoxOnScopeType = cItemInt[1];
    if (!cItemEmpty[2])
      mBoxOnScopeInterval = cItemFlt[2];
    if (!cItemEmpty[3]) {
      SubstituteVariables(&cStrLine, 1, cStrLine);
      mWinApp->mParamIO->StripItems(cStrLine, 3, mBoxOnScopeText);
    }
    break;

  case CME_UPDATEHWDARKREF:                                 // UpdateHWDarkRef
    cIndex = mCamera->UpdateK2HWDarkRef(cItemFlt[1]);
    if (cIndex == 1)
      ABORT_LINE("The thread is already busy for this operation:\n\n")
    mStartedLongOp = !cIndex;
    break;

  case CME_LONGOPERATION:                                   // LongOperation
  {
    cIx1 = 0;
    cIy1 = 1;
    int used[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int operations[MAX_LONG_OPERATIONS + 1];
    float intervals[MAX_LONG_OPERATIONS + 1];
    for (cIndex = 1; cIndex < MAX_MACRO_TOKENS && !cItemEmpty[cIndex]; cIndex++) {
      for (cIndex2 = 0; cIndex2 < MAX_LONG_OPERATIONS; cIndex2++) {
        if (!cStrItems[cIndex].Left(2).CompareNoCase(CString(cLongKeys[cIndex2]))) {
          if (cLongHasTime[cIndex2] && cItemDbl[cIndex + 1] < -1.5) {
            cBacklashX = cItemFlt[cIndex + 1];
            mScope->StartLongOperation(&cIndex2, &cBacklashX, 1);
            SetOneReportedValue(cBacklashX, cIy1++);
            cReport.Format("Time since last long operation %s is %.2f hours",
              (LPCTSTR)cStrItems[cIndex], cBacklashX);
            mWinApp->AppendToLog(cReport, mLogAction);
            cIndex++;
            break;
          }
          if (used[cIndex2])
            ABORT_LINE("The same operation is specified twice in:\n\n");
          used[cIndex2]++;
          operations[cIx1] = cIndex2;
          if (cIndex2 == LONG_OP_HW_DARK_REF &&
            !mCamera->CanDoK2HardwareDarkRef(cCamParams, cReport))
            ABORT_LINE(cReport + " in line:\n\n");
          if (cLongHasTime[cIndex2]) {
            if (cIndex == MAX_MACRO_TOKENS - 1 || cItemEmpty[cIndex + 1])
              ABORT_LINE("The last operation must be followed by an interval in hours "
                "in:\n\n");
            cReport = cStrItems[cIndex + 1];
            if (cReport.MakeLower() != cStrItems[cIndex + 1]) {
              cReport.Format("%s must be followed by an interval in hours in:\n\n",
                (LPCTSTR)cStrItems[cIndex]);
              ABORT_LINE(cReport);
            }
            cIndex++;
            intervals[cIx1++] = cItemFlt[cIndex];
          }
          else
            intervals[cIx1++] = 0.;
          break;
        }
      }
      if (cIndex2 == MAX_LONG_OPERATIONS) {
        cReport.Format("%s is not an allowed entry for a long operation in:\n\n",
          (LPCTSTR)cStrItems[cIndex]);
        ABORT_LINE(cReport);
      }
    }
    cIndex = mScope->StartLongOperation(operations, intervals, cIx1);
    if (cIndex > 0)
      ABORT_LINE(cIndex == 1 ? "The thread is already busy for a long operation in:\n\n" :
        "A long scope operation can be done only on an FEI scope for:\n\n");
    mStartedLongOp = !cIndex;
    break;
  }

  case CME_NEWDESERVERDARKREF:                              // NewDEserverDarkRef
    if (mWinApp->mGainRefMaker->MakeDEdarkRefIfNeeded(cItemInt[1], cItemFlt[2],
      cReport))
      ABORT_NOLINE(CString("Cannot make a new dark reference in DE server with "
      "NewDEserverDarkRef:\n") + cReport);
    break;

  case CME_RUNINSHELL:                                      // RunInShell
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, mEnteredName);
    mProcessThread = AfxBeginThread(RunInShellProc, &mEnteredName, THREAD_PRIORITY_NORMAL,
       0, CREATE_SUSPENDED);
    mProcessThread->m_bAutoDelete = false;
    mProcessThread->ResumeThread();
    break;

  case CME_NEXTPROCESSARGS:                                 // NextProcessArgs
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, mNextProcessArgs);
    break;

  case CME_CREATEPROCESS:                                   // CreateProcess
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    cIndex = mWinApp->mExternalTools->RunCreateProcess(cStrCopy, mNextProcessArgs);
    mNextProcessArgs = "";
    if (cIndex)
      ABORT_LINE("Script aborted due to failure to run process for line:\n\n");
    break;

  case CME_EXTERNALTOOLARGPLACE:                            // ExternalToolArgPlace
    mRunToolArgPlacement = cItemInt[1];
    break;

  case CME_RUNEXTERNALTOOL:                                 // RunExternalTool
    SubstituteVariables(&cStrLine, 1, cStrLine);
    mWinApp->mParamIO->StripItems(cStrLine, 1, cStrCopy);
    cIndex = mWinApp->mExternalTools->RunToolCommand(cStrCopy, mNextProcessArgs,
      mRunToolArgPlacement);
    mNextProcessArgs = "";
    if (cIndex)
      ABORT_LINE("Script aborted due to failure to run command for line:\n\n");
    break;

  case CME_SAVEFOCUS:                                       // SaveFocus
    if (mFocusToRestore > -2.)
      ABORT_NOLINE("There is a second SaveFocus without a RestoreFocus");
    mFocusToRestore = mScope->GetFocus();
    mNumStatesToRestore++;
    break;

  case CME_RESTOREFOCUS:                                    // RestoreFocus
    if (mFocusToRestore < -2.)
      ABORT_NOLINE("There is a RestoreFocus, but focus was not saved or has been "
        "restored already");
    mScope->SetFocus(mFocusToRestore);
    mNumStatesToRestore--;
    mFocusToRestore = -999.;
    break;

  case CME_SAVEBEAMTILT:                                  // SaveBeamTilt
     cIndex = mScope->GetProbeMode();
     if (mBeamTiltXtoRestore[cIndex] > EXTRA_VALUE_TEST) {
       cReport = "There is a second SaveBeamTilt without a RestoreBeamTilt";
       if (FEIscope)
         cReport += " for " + CString(cIndex ? "micro" : "nano") + "probe mode";
       ABORT_NOLINE(cReport);
     }
     mScope->GetBeamTilt(mBeamTiltXtoRestore[cIndex], mBeamTiltYtoRestore[cIndex]);
     mNumStatesToRestore++;
     break;

  case CME_RESTOREBEAMTILT:                                 // RestoreBeamTilt
    cIndex = mScope->GetProbeMode();
    if (mBeamTiltXtoRestore[cIndex] < EXTRA_VALUE_TEST) {
       cReport = "There is a RestoreBeamTilt, but beam tilt was not saved or has been "
        "restored already";
       if (FEIscope)
         cReport += " for " + CString(cIndex ? "micro" : "nano") + "probe mode";
       ABORT_NOLINE(cReport);
    }
    mScope->SetBeamTilt(mBeamTiltXtoRestore[cIndex], mBeamTiltYtoRestore[cIndex]);
    mNumStatesToRestore--;
    mBeamTiltXtoRestore[cIndex] = mBeamTiltYtoRestore[cIndex] = EXTRA_NO_VALUE;
    mCompensatedBTforIS = false;
    break;

    // PIEZO COMMANDS
  case CME_SELECTPIEZO:                                     // SelectPiezo
    if (mWinApp->mPiezoControl->SelectPiezo(cItemInt[1], cItemInt[2])) {
      AbortMacro();
      return;
    }
    break;

  case CME_REPORTPIEZOXY:                                   // ReportPiezoXY
    if (mWinApp->mPiezoControl->GetNumPlugins()) {
      if (mWinApp->mPiezoControl->GetXYPosition(cDelISX, cDelISY)) {
        AbortMacro();
        return;
      }
    } else {
      if (!mScope->GetPiezoXYPosition(cDelISX, cDelISY)) {
        AbortMacro();
        return;
      }
    }
    cLogRpt.Format("Piezo X/Y position is %6g, %6g", cDelISX, cDelISY);
    SetReportedValues(&cStrItems[1], cDelISX, cDelISY);
    break;

  case CME_REPORTPIEZOZ:                                    // ReportPiezoZ
    if (mWinApp->mPiezoControl->GetZPosition(cDelISX)) {
      AbortMacro();
      return;
    }
    cLogRpt.Format("Piezo Z or only position is %6g", cDelISX);
    SetReportedValues(&cStrItems[1], cDelISX);
    break;

  case CME_MOVEPIEZOXY:                                     // MovePiezoXY
    if (mWinApp->mPiezoControl->GetNumPlugins()) {
      if (mWinApp->mPiezoControl->SetXYPosition(cItemDbl[1], cItemDbl[2],
        cItemInt[3] != 0)) {
        AbortMacro();
        return;
      }
      mMovedPiezo = true;
    } else {
      if (!mScope->SetPiezoXYPosition(cItemDbl[1], cItemDbl[2],
        cItemInt[3] != 0)) {
        AbortMacro();
        return;
      }
      mMovedStage = true;
    }
    break;

  case CME_MOVEPIEZOZ:                                      // MovePiezoZ
    if (mWinApp->mPiezoControl->SetZPosition(cItemDbl[1],
      cItemInt[2] != 0)) {
        AbortMacro();
        return;
    }
    mMovedPiezo = true;
    break;

  case CME_MACRONAME:                                       // MacroName
  case CME_SCRIPTNAME:                                      // ScriptName
  case CME_LONGNAME:                                        // LongName
  case CME_READONLYUNLESSADMIN:                             // ReadOnlyUnlessAdmin
    cIndex = 0;
    break;
  default:
    ABORT_LINE("Unrecognized statement in script: \n\n");
    break;
  }

  // Output the standard log report variable if set
  if (!cLogRpt.IsEmpty())
    mWinApp->AppendToLog(cLogRpt, mLogAction);

  // The action is taken or started: now set up an idle task unless looping is allowed
  // for this command and not too many loops have happened
  if (startingOut || !((cmdList[cCmdIndex].arithAllowed & 4) || mLoopInOnIdle ||
    cmdList[cCmdIndex].cmd.find("REPORT") == 0) ||
    mNumCmdSinceAddIdle > mMaxCmdToLoopOnIdle ||
    SEMTickInterval(mLastAddIdleTime) > 1000. * mMaxSecToLoopOnIdle) {
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    mNumCmdSinceAddIdle = 0;
    mLastAddIdleTime = GetTickCount();
  } else {

    // Or let it loop back in to NextCommand
    mNumCmdSinceAddIdle++;
    mLoopInOnIdle = true;
  }
}
