// MenuTargets.cpp : A centralized place for command targets, to avoid 
//                   cluttering up SerialEM.cpp and avoid making modules
//                   with just a few targets be CmdTargets
// 
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include ".\MenuTargets.h"
#include "MacroSelector.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "MapDrawItem.h"
#include "DistortionTasks.h"
#include "FilterTasks.h"
#include "CameraController.h"
#include "GainRefMaker.h"
#include "BeamAssessor.h"
#include "ShiftManager.h"
#include "FocusManager.h"
#include "ShiftCalibrator.h"
#include "EMscope.h"
#include "ParameterIO.h"
#include "AutoTuning.h"
#include "RefPolicyDlg.h"
#include "TSPolicyDlg.h"
#include "TSController.h"
#include "EMmontageController.h"
#include "CalibCameraTiming.h"
#include "CookerSetupDlg.h"
#include "MultiShotDlg.h"
#include "MultiTSTasks.h"
#include "ParticleTasks.h"
#include "DriftWaitSetupDlg.h"
#include "NavBacklashDlg.h"
#include "NavRealignDlg.h"
#include "ManageDewarsDlg.h"
#include "ExternalTools.h"
#include "Mailer.h"
#include "PluginManager.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"
#include "ScreenShotDialog.h"
#include "Shared\\SEMCCDDefines.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMenuTargets

//IMPLEMENT_DYNCREATE(CMenuTargets, CCmdTarget)

CMenuTargets::CMenuTargets()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mNavigator = NULL;
}

CMenuTargets::~CMenuTargets()
{
}


void CMenuTargets::Initialize()
{
  mShiftManager = mWinApp->mShiftManager;
  mCamera = mWinApp->mCamera;
  mScope = mWinApp->mScope;
  mTSController = mWinApp->mTSController;
  mNavHelper = mWinApp->mNavHelper;
  mFlybackSet = "R";
}

BEGIN_MESSAGE_MAP(CMenuTargets, CCmdTarget)
	//{{AFX_MSG_MAP(CMenuTargets)
	ON_UPDATE_COMMAND_UI(ID_LOAD_NAV_FILE, OnUpdateLoadNavFile)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_IMPORTMAP, OnUpdateLoadNavFile)
	ON_COMMAND(ID_LOAD_NAV_FILE, OnLoadNavFile)
	ON_COMMAND(ID_TASKS_NAVIGATOR, OnTasksNavigator)
	ON_UPDATE_COMMAND_UI(ID_TASKS_NAVIGATOR, OnUpdateTasksNavigator)
	ON_COMMAND(ID_SAVE_NAV_FILE, OnSaveNavFile)
	ON_UPDATE_COMMAND_UI(ID_SAVE_NAV_FILE, OnUpdateSaveNavFile)
	ON_COMMAND(ID_SAVEAS_NAV_FILE, OnSaveasNavFile)
	ON_COMMAND(ID_NAVIGATOR_CLOSE, OnNavigatorClose)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_CLOSE, OnUpdateNavigatorClose)
	ON_COMMAND(ID_CORNER_MONTAGE, OnCornerMontage)
	ON_UPDATE_COMMAND_UI(ID_CORNER_MONTAGE, OnUpdateCornerMontage)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_POLYGON_FROM_CORNERS, OnUpdateCornerMontage)
	ON_COMMAND(ID_POLYGON_MONTAGE, OnPolygonMontage)
	ON_UPDATE_COMMAND_UI(ID_POLYGON_MONTAGE, OnUpdatePolygonMontage)
	ON_COMMAND(ID_NAVIGATOR_ACQUIRE, OnNavigatorAcquire)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ACQUIRE, OnUpdateNavigatorAcquire)
	ON_COMMAND(ID_NEW_MAP, OnNewMap)
	ON_UPDATE_COMMAND_UI(ID_NEW_MAP, OnUpdateNewMap)
	ON_COMMAND(ID_TRANSFORM_PTS, OnTransformPts)
	ON_UPDATE_COMMAND_UI(ID_TRANSFORM_PTS, OnUpdateTransformPts)
	ON_COMMAND(ID_DELETEITEM, OnDeleteitem)
	ON_UPDATE_COMMAND_UI(ID_DELETEITEM, OnUpdateDeleteitem)
	ON_COMMAND(ID_CALIBRATION_DISTORTION, OnCalibrationDistortion)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_DISTORTION, OnUpdateNoTasksNoSTEM)
  ON_COMMAND(ID_CALIBRATION_MAGENERGYSHIFTS, OnCalibrationMagenergyshifts)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_MAGENERGYSHIFTS, OnUpdateCalibrationMagenergyshifts)
  ON_COMMAND(ID_CAMERA_ACQUIREGAINREF, OnAcquiregainref)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_ACQUIREGAINREF, OnUpdateAcquiregainref)
  ON_COMMAND(ID_CALIBRATION_BEAMINTENSITY, OnCalibrationBeamintensity)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_BEAMINTENSITY, OnUpdateCalibrationBeamintensity)
  ON_COMMAND(ID_CALIBRATION_BEAMSHIFT, OnCalibrationBeamshift)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_BEAMSHIFT, OnUpdateCalibrationBeamshift)
  ON_COMMAND(ID_CALIBRATION_IMAGESHIFT, OnCalibrationImageshift)
	ON_COMMAND(ID_CALIBRATION_STAGESHIFT, OnCalibrationStageshift)
  ON_COMMAND(ID_CALIBRATION_ISFROMSCRATCH, OnCalibrationIsfromscratch)
  ON_COMMAND(IDM_AUTOALIGN, OnAutoalign)
  ON_COMMAND(IDM_TILTDOWN, OnTiltdown)
  ON_UPDATE_COMMAND_UI(IDM_TILTDOWN, OnUpdateTiltdown)
  ON_COMMAND(IDM_TILTUP, OnTiltup)
	ON_COMMAND(ID_CALIBRATION_LISTMAGS, OnCalibrationListmags)
  ON_COMMAND(ID_CAMERA_STOPACQUIRE, OnCameraStopacquire)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_STOPACQUIRE, OnUpdateCameraStopacquire)
  ON_COMMAND(ID_CAMERA_VIEW, OnCameraView)
  ON_COMMAND(ID_CAMERA_FOCUS, OnCameraFocus)
  ON_COMMAND(ID_CAMERA_TRIAL, OnCameraTrial)
  ON_COMMAND(ID_CAMERA_RECORD, OnCameraRecord)
  ON_COMMAND(ID_CAMERA_PREVIEW, OnCameraPreview)
  ON_COMMAND(ID_CAMERA_SCREENDOWN, OnCameraScreendown)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SCREENDOWN, OnUpdateCameraScreendown)
  ON_COMMAND(ID_CAMERA_MONTAGE, OnCameraMontage)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_MONTAGE, OnUpdateCameraMontage)
  ON_COMMAND(ID_CAMERA_MONTAGETRIAL, OnCameraMontagetrial)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_MONTAGETRIAL, OnUpdateCameraMontagetrial)
  ON_COMMAND(ID_CAMERA_SETTIMING, OnCameraSettiming)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SETTIMING, OnUpdateCameraSettiming)
  ON_COMMAND(ID_CAMERA_DEBUGMODE, OnCameraDebugmode)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_DEBUGMODE, OnUpdateCameraDebugmode)
  ON_COMMAND(ID_CAMERA_SHOWGAINREF, OnCameraShowgainref)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SHOWGAINREF, OnUpdateCameraShowref)
  ON_COMMAND(ID_CAMERA_SHOWDARKREF, OnCameraShowdarkref)
  ON_COMMAND(IDM_CAMERATOGGLE, OnCameratoggle)
  ON_COMMAND(IDM_ESCAPE, OnEscape)
  ON_UPDATE_COMMAND_UI(IDM_ESCAPE, OnUpdateEscape)
  ON_COMMAND(ID_CAMERA_POSTACTIONS, OnCameraPostactions)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_POSTACTIONS, OnUpdateCameraPostactions)
	ON_COMMAND(ID_CALIBRATION_ELECTRONDOSE, OnCalibrationElectrondose)
	ON_COMMAND(ID_CALIBRATION_C2FACTOR, OnCalibrationC2factor)
  ON_COMMAND(ID_TASKS_TILTSERIES, OnTasksTiltseries)
  ON_UPDATE_COMMAND_UI(ID_TASKS_TILTSERIES, OnUpdateTasksTiltseries)
  ON_COMMAND(ID_TILTSERIES_BACKUP, OnTiltseriesBackup)
  ON_COMMAND(ID_TILTSERIES_STOP, OnTiltseriesStop)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_STOP, OnUpdateTiltseriesStop)
  ON_COMMAND(ID_TILTSERIES_ENDLOOP, OnTiltseriesEndloop)
  ON_COMMAND(ID_TILTSERIES_RESUME, OnTiltseriesResume)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_RESUME, OnUpdateTiltseriesResume)
  ON_COMMAND(ID_TILTSERIES_ONELOOP, OnTiltseriesOneloop)
  ON_COMMAND(ID_TILTSERIES_ONESTEP, OnTiltseriesOnestep)
  ON_COMMAND(ID_TILTSERIES_TERMINATE, OnTiltseriesTerminate)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_BACKUP, OnUpdateTiltseriesBackup)
  ON_COMMAND(ID_TILTSERIES_VERBOSE, OnTiltseriesVerbose)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_VERBOSE, OnUpdateTiltseriesVerbose)
  ON_COMMAND(ID_TILTSERIES_DEBUGMODE, OnTiltseriesDebugmode)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_DEBUGMODE, OnUpdateTiltseriesDebugmode)
	ON_COMMAND(ID_TILTSERIES_AUTOSAVELOG, OnTiltseriesAutosavelog)
	ON_UPDATE_COMMAND_UI(ID_TILTSERIES_AUTOSAVELOG, OnUpdateTiltseriesAutosavelog)
	ON_COMMAND(ID_CALIBRATION_BEAMCROSSOVER, OnCalibrationBeamcrossover)
	ON_COMMAND(ID_CALIBRATION_SETOVERLAPS, OnCalibrationSetoverlaps)
	ON_COMMAND(ID_NAVIGATOR_AUTOSAVE, OnNavigatorAutosave)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_AUTOSAVE, OnUpdateNavigatorAutosave)
	ON_COMMAND(ID_NAVIGATOR_SETUPFULLMONTAGE, OnNavigatorSetupfullmontage)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_SETUPFULLMONTAGE, OnUpdateNavigatorSetupfullmontage)
	ON_COMMAND(ID_CAMERA_DIVIDEBY2, OnCameraDivideby2)
	ON_UPDATE_COMMAND_UI(ID_CAMERA_DIVIDEBY2, OnUpdateCameraDivideby2)
	ON_COMMAND(ID_CALIBRATION_CAMERATIMING, OnCalibrationCameratiming)
	ON_COMMAND(ID_CAMERA_NORMALIZEHERE, OnCameraNormalizehere)
	ON_UPDATE_COMMAND_UI(ID_CAMERA_NORMALIZEHERE, OnUpdateCameraNormalizehere)
	ON_COMMAND(ID_CAMERA_SETCORRECTIONS, OnCameraSetcorrections)
	ON_UPDATE_COMMAND_UI(ID_CAMERA_SETCORRECTIONS, OnUpdateCameraSetcorrections)
	ON_COMMAND(ID_NAVIGATOR_CONVERTMAPSTOBYTES, OnNavigatorConvertmaps)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_CONVERTMAPSTOBYTES, OnUpdateNavigatorConvertmaps)
	ON_COMMAND(ID_CAMERA_SETSCANNING, OnCameraSetscanning)
	ON_COMMAND(ID_CALIBRATION_NEUTRALISVALUES, OnCalibrationNeutralISvalues)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_NEUTRALISVALUES, OnUpdateCalibrationNeutralISvalues)
	ON_COMMAND(ID_CALIBRATION_RESTORENEUTRALIS, OnCalibrationRestoreNeutralIS)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_RESTORENEUTRALIS, OnUpdateCalibrationRestoreNeutralIS)
	ON_COMMAND(ID_AUTOFOCUSFOCUS_BASEFOCUSVALUES, OnCalibrationBaseFocusValues)
	ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_BASEFOCUSVALUES, OnUpdateCalibrationRestoreNeutralIS)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_C2FACTOR, OnUpdateCalibrationC2factor)
	ON_COMMAND(ID_CALIBRATION_SPOTINTENSITY, OnCalibrationSpotintensity)
	ON_COMMAND(ID_CALIBRATION_PURGEOLDCALIBS, OnCalibrationPurgeoldcalibs)
	ON_COMMAND(ID_NAVIGATOR_SKEWEDSUPERMONTAGE, OnNavigatorSkewedsupermontage)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_SKEWEDSUPERMONTAGE, OnUpdateNavigatorSkewedsupermontage)
	ON_COMMAND(ID_NAVIGATOR_ALIGNEDSUPERMONTAGE, OnNavigatorAlignedsupermontage)
	ON_COMMAND(ID_CALIBRATION_LISTISVECTORS, OnCalibrationListisvectors)
	ON_COMMAND(ID_CALIBRATION_LISTSTAGECALS, OnCalibrationListstagecals)
  ON_COMMAND(ID_CALIBRATION_SHUTTERDEADTIME, OnCalibrationShutterdeadtime)
  ON_COMMAND(ID_CALIBRATION_SETDEBUGOUTPUT, OnCalibrationSetdebugoutput)
	ON_COMMAND(ID_TILTSERIES_EXTRAOUTPUT, OnTiltseriesExtraoutput)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_CAMERATIMING, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_LOAD_NAV_FILE, OnUpdateLoadNavFile)
	ON_UPDATE_COMMAND_UI(ID_SAVEAS_NAV_FILE, OnUpdateSaveNavFile)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_ISFROMSCRATCH, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_IMAGESHIFT, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_STAGESHIFT, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(IDM_AUTOALIGN, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(IDM_TILTUP, OnUpdateTiltdown)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_LISTMAGS, OnUpdateCalibrationListmags)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_VIEW, OnUpdateNoUserShots)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_PREVIEW, OnUpdateNoUserShots)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SHOWDARKREF, OnUpdateCameraShowref)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_FOCUS, OnUpdateNoUserShots)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_TRIAL, OnUpdateNoUserShots)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_RECORD, OnUpdateNoUserShots)
  ON_UPDATE_COMMAND_UI(IDM_CAMERATOGGLE, OnpdateCameratoggle)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_ELECTRONDOSE, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_ENDLOOP, OnUpdateTiltseriesStop)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_ONELOOP, OnUpdateTiltseriesResume)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_ONESTEP, OnUpdateTiltseriesResume)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_TERMINATE, OnUpdateTiltseriesResume)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_BEAMCROSSOVER, OnUpdateNoTasksNoSTEM)
	ON_UPDATE_COMMAND_UI(ID_BEAMSPOT_SPOTBEAMSHIFTS, OnUpdateNoTasksNoSTEM)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SETOVERLAPS, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_CAMERA_SETSCANNING, OnUpdateCameraSettiming)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SPOTINTENSITY, OnUpdateNoTasksNoSTEM)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_PURGEOLDCALIBS, OnUpdateNoTasksNoSTEM)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ALIGNEDSUPERMONTAGE, OnUpdateNavigatorSkewedsupermontage)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_LISTISVECTORS, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_LISTSTAGECALS, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SHUTTERDEADTIME, OnUpdateCalibrationShutterdeadtime)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SETDEBUGOUTPUT, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_TILTSERIES_EXTRAOUTPUT, OnUpdateNoTasks)
	ON_UPDATE_COMMAND_UI(ID_CALIBRATION_MAGISOFFSETS, OnUpdateNoTasksNoSTEM)
	ON_COMMAND(ID_CALIBRATION_EFTEMISOFFSET, OnCalibrationEftemISoffset)
  ON_COMMAND(ID_NAVIGATOR_CHANGEREGISTRATION, OnNavigatorChangeregistration)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_CHANGEREGISTRATION, OnUpdateNavigatorChangeregistration)
  ON_COMMAND(ID_CALIBRATION_MAGISOFFSETS, OnCalibrationMagisoffsets)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_EFTEMISOFFSET, OnUpdateCalibrationEftemISoffset)
  ON_COMMAND(ID_NAVIGATOR_ROTATEMAP, OnNavigatorRotatemap)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ROTATEMAP, OnUpdateNavigatorRotatemap)
  ON_COMMAND(ID_NAVIGATOR_ALIGNTOITEM, OnNavigatorAligntoitem)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ALIGNTOITEM, OnUpdateNavigatorAligntoitem)
  ON_COMMAND(ID_NAVIGATOR_SHIFTTOMARKER, OnNavigatorShifttomarker)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_SHIFTTOMARKER, OnUpdateNavigatorShifttomarker)
  ON_COMMAND(ID_NAVIGATOR_UNDOLASTSHIFT, OnNavigatorUndolastshift)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_UNDOLASTSHIFT, OnUpdateNavigatorUndolastshift)
  ON_COMMAND(ID_NAVIGATOR_LOADUNBINNED, OnNavigatorLoadunbinned)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_LOADUNBINNED, OnUpdateNavigatorLoadunbinned)
  ON_COMMAND(ID_NAVIGATOR_SETGRIDLIMITS, OnNavigatorSetgridlimits)
  ON_COMMAND(ID_CALIBRATION_STANDARDLMFOCUS, OnCalibrationStandardLMfocus)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_STANDARDLMFOCUS, OnUpdateNoTasksNoSTEM)
 	//}}AFX_MSG_MAP
  ON_COMMAND(ID_CAMERA_GAINREFPOLICY, OnCameraGainrefpolicy)
  ON_COMMAND(ID_TILTSERIES_RESETPOLEANGLE, OnTiltseriesResetpoleangle)
  ON_COMMAND(ID_TILTSERIES_FUTUREPOLICIES, OnTiltseriesFuturepolicies)
  ON_COMMAND(ID_NAVIGATOR_ENDACQUIRE, OnNavigatorEndacquire)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ENDACQUIRE, OnUpdateNavigatorEndacquire)
  ON_COMMAND(ID_NAVIGATOR_SETACQUIRESTATE, OnNavigatorSetacquirestate)
  ON_COMMAND(ID_NAVIGATOR_RESTORESTATE, OnNavigatorRestorestate)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_SETACQUIRESTATE, OnUpdateNavigatorSetacquirestate)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_RESTORESTATE, OnUpdateNavigatorRestorestate)
  ON_COMMAND(ID_NAVIGATOR_ADDCIRCLEPOLYGON, OnNavigatorAddcirclepolygon)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ADDCIRCLEPOLYGON, OnUpdateNavigatorAddCirclePolygon)
  ON_COMMAND(ID_NAVIGATOR_DELETESUPERMONTAGE, OnNavigatorDeleteGroup)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_DELETESUPERMONTAGE, OnUpdateNavigatorDeleteGroup)
  ON_COMMAND(ID_SUPERMONTAGING_POLYGONSUPERMONTAGE, OnPolygonsupermontage)
  ON_UPDATE_COMMAND_UI(ID_SUPERMONTAGING_POLYGONSUPERMONTAGE, OnUpdatePolygonsupermontage)
  ON_COMMAND(ID_NAVIGATOR_IMPORTMAP, OnNavigatorImportmap)
  ON_COMMAND(ID_NAVIGATOR_UNDOTRANSFORMATION, OnNavigatorUndotransformation)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_UNDOTRANSFORMATION, OnUpdateNavigatorUndotransformation)
  ON_COMMAND(ID_NAVIGATOR_ADDGRIDOFPOINTS, OnNavigatorAddGridOfPoints)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ADDGRIDOFPOINTS, OnUpdateNavigatorAddGridOfPoints)
  ON_COMMAND(ID_MONTAGINGGRIDS_GROUPPOINTSINGRID, OnNavigatorDivideIntoGroups)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_GROUPPOINTSINGRID, OnUpdateNavigatorDivideIntoGroups)
  ON_COMMAND(ID_MONTAGINGGRIDS_SETGROUPSIZE, OnNavigatorSetGridGroupSize)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_SETGROUPSIZE, OnUpdateNoTasks)
  ON_COMMAND(ID_MONTAGE_LISTFILESTOOPEN, OnMontageListFilesToOpen)
  ON_UPDATE_COMMAND_UI(ID_MONTAGE_LISTFILESTOOPEN, OnUpdateNavigatorAcquire)
  ON_COMMAND(ID_CAMERA_SETCOUNTLIMIT, OnCameraSetcountlimit)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SETCOUNTLIMIT, OnUpdateNoTasks)
  ON_COMMAND(ID_TASKS_SETUPCOOKER, OnTasksSetupcooker)
  ON_COMMAND(ID_TASKS_COOKSPECIMEN, OnTasksCookspecimen)
  ON_COMMAND(ID_TASKS_SETUPAUTOCENTER, OnTasksSetupAutocenter)
  ON_COMMAND(ID_TASKS_AUTOCENTERBEAM, OnTasksAutocenterBeam)
  ON_UPDATE_COMMAND_UI(ID_TASKS_AUTOCENTERBEAM, OnUpdateTasksAutocenterbeam)
  ON_COMMAND(ID_CAMERA_SETDRIFTINTERVAL, OnCameraSetdriftinterval)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SETDRIFTINTERVAL, OnUpdateNoTasks)
  ON_COMMAND(ID_TILTSERIES_SENDEMAIL, OnTiltseriesSendemail)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_SENDEMAIL, OnUpdateTiltseriesSendemail)
  ON_COMMAND(ID_TILTSERIES_SETADDRESS, OnTiltseriesSetaddress)
  ON_COMMAND(ID_TASKS_ASSESSANGLERANGE, OnTasksAssessAngleRange)
  ON_UPDATE_COMMAND_UI(ID_TASKS_ASSESSANGLERANGE, OnUpdateNoTasksNoTS)
  ON_COMMAND(ID_NAVIGATOR_OPENSTATEDLG, OnNavigatorOpenStateDlg)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_OPENSTATEDLG, OnUpdateNavigatorOpenStateDlg)
  ON_COMMAND(ID_NAVIGATOR_GROUP_ACQUIRE, OnNavigatorGroupAcquire)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_GROUP_ACQUIRE, OnUpdateNavigatorGroupAcquire)
  ON_COMMAND(ID_NAVIGATOR_ALIGN_TO_MAP, OnNavigatorAlignToMap)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ALIGN_TO_MAP, OnUpdateNavigatorAlignToMap)
  ON_COMMAND(ID_CAMERA_INTERPOLATE_DARK_REFS, OnCameraInterpolateDarkRefs)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_INTERPOLATE_DARK_REFS, OnUpdateCameraInterpolateDarkRefs)
  ON_COMMAND(ID_CALIBRATION_STAGESTRETCH, OnCalibrationStagestretch)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_STAGESTRETCH, OnUpdateCalibrationStagestretch)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_GAINREFPOLICY, OnUpdateCameraGainrefpolicy)
  ON_COMMAND(ID_CALIBRATION_SET_IS_DELAY_FACTOR, OnCalibrationSetIsDelayFactor)
  ON_COMMAND(ID_NAVIGATOR_POLYGON_FROM_CORNERS, OnNavigatorPolygonFromCorners)
  ON_UPDATE_COMMAND_UI(ID_TASKS_SETUPAUTOCENTER, OnUpdateTasksSetupAutocenter)
  ON_UPDATE_COMMAND_UI(ID_TASKS_SETUPCOOKER, OnUpdateNoTasksNoSTEM)
  ON_UPDATE_COMMAND_UI(ID_TASKS_COOKSPECIMEN, OnUpdateNoTasksNoTSnoSTEM)
  ON_COMMAND(ID_TASKS_ASSESS_INTERSET_SHIFTS, OnTasksAssessIntersetShifts)
  ON_UPDATE_COMMAND_UI(ID_TASKS_ASSESS_INTERSET_SHIFTS, OnUpdateTasksAssessIntersetShifts)
  ON_COMMAND(ID_TASKS_REVISE_CANCEL, OnTasksReviseCancel)
  ON_UPDATE_COMMAND_UI(ID_TASKS_REVISE_CANCEL, OnUpdateTasksReviseCancel)
  ON_COMMAND(ID_NAVIGATOR_REALIGN_SCALING, OnNavigatorRealignScaling)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_REALIGN_SCALING, OnUpdateNavigatorRealignScaling)
  ON_COMMAND(ID_CALIBRATION_QUICK_FLYBACK_TIME, OnCalibrationQuickFlybackTime)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_QUICK_FLYBACK_TIME, OnUpdateCalibrationQuickFlybackTime)
  ON_COMMAND(ID_NAVIGATOR_ADJUST_BACKLASH, OnNavigatorAdjustBacklash)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_ADJUST_BACKLASH, OnUpdateNavigatorAdjustBacklash)
  ON_COMMAND(ID_NAVIGATOR_BACKLASHSETTINGS, OnNavigatorBacklashSettings)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_BACKLASHSETTINGS, OnUpdateNoTasks)
  ON_COMMAND(ID_CALIBRATION_ALPHA_BEAM_SHIFTS, OnCalibrationAlphaBeamShifts)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_ALPHA_BEAM_SHIFTS, OnUpdateCalibrationAlphaBeamShifts)
  ON_COMMAND(ID_BEAMSPOT_SPOTBEAMSHIFTS, OnCalibrationSpotBeamShifts)
  ON_COMMAND(ID_CALIBRATION_SET_APERTURE_SIZE, OnCalibrationSetApertureSize)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SET_APERTURE_SIZE, OnUpdateCalibrationSetApertureSize)
  ON_COMMAND(ID_CALIBRATION_WHOLE_IMAGE_CORR, OnCalibrationWholeImageCorr)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_WHOLE_IMAGE_CORR, OnUpdateCalibrationWholeImageCorr)
  ON_COMMAND(ID_CALIBRATION_SET_CYCLE_LENGTHS, OnCalibrationSetCycleLengths)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_SET_CYCLE_LENGTHS, OnUpdateNoTasks)
  ON_COMMAND(ID_TILTSERIES_CALL_TS_PLUGIN, OnTiltseriesCallTsPlugin)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_CALL_TS_PLUGIN, OnUpdateTiltseriesCallTsPlugin)
  ON_COMMAND(ID_SPECIAL_SKIP_BEAM_SHIFT_IN_TS, OnSpecialSkipBeamShiftInTs)
  ON_UPDATE_COMMAND_UI(ID_SPECIAL_SKIP_BEAM_SHIFT_IN_TS, OnUpdateSpecialSkipBeamShiftInTs)
  ON_COMMAND(ID_SPECIAL_PIEZO_FOR_LD_AXIS_SHIFT, OnSpecialPiezoForLdAxisShift)
  ON_UPDATE_COMMAND_UI(ID_SPECIAL_PIEZO_FOR_LD_AXIS_SHIFT, OnUpdateSpecialPiezoForLdAxisShift)
  ON_COMMAND(ID_NAVIGATOR_MERGEFILE, OnNavigatorMergefile)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_MERGEFILE, OnUpdateNavigatorMergefile)
  ON_COMMAND(ID_CAMERA_SET_QUALITY, OnCameraSetQuality)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SET_QUALITY, OnUpdateCameraSetQuality)
  ON_COMMAND(ID_WINDOW_STAGE_MOVE_TOOL, OnWindowStageMoveTool)
  ON_UPDATE_COMMAND_UI(ID_WINDOW_STAGE_MOVE_TOOL, OnUpdateNoTasks)
  ON_COMMAND(ID_NAVIGATOR_FORCE_CENTER_ALIGN, OnNavigatorForceCenterAlign)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_FORCE_CENTER_ALIGN, OnUpdateNoTasks)
  ON_COMMAND(ID_SPECIAL_DISABLE_DARK_TRIM_IN_ALIGN, OnSpecialDisableDarkTrimInAlign)
  ON_UPDATE_COMMAND_UI(ID_SPECIAL_DISABLE_DARK_TRIM_IN_ALIGN, OnUpdateNoTasks)
  ON_COMMAND(ID_SPECIALOPTIONS_ALLOWCONTINUOUSINTASKS, OnAllowContinuousInTS)
  ON_UPDATE_COMMAND_UI(ID_SPECIALOPTIONS_ALLOWCONTINUOUSINTASKS, OnUpdateAllowContinuousInTS)
  ON_COMMAND(ID_IMAGESTAGESHIFT_USETRIALBINNING, OnUseTrialBinning)
  ON_COMMAND(ID_IMAGESTAGESHIFT_USETRIALSIZE, OnUseTrialSize)
  ON_UPDATE_COMMAND_UI(ID_IMAGESTAGESHIFT_USETRIALBINNING, OnUpdateUseTrialBinning)
  ON_UPDATE_COMMAND_UI(ID_IMAGESTAGESHIFT_USETRIALSIZE, OnUpdateUseTrialSize)
  ON_COMMAND(ID_IMAGESTAGESHIFT_REMOVEISCAL, OnRemoveIScal)
  ON_COMMAND(ID_IMAGESTAGESHIFT_REMOVESTAGECAL, OnRemoveStageCal)
  ON_UPDATE_COMMAND_UI(ID_IMAGESTAGESHIFT_REMOVEISCAL, OnUpdateNoTasksNoTS)
  ON_UPDATE_COMMAND_UI(ID_IMAGESTAGESHIFT_REMOVESTAGECAL, OnUpdateNoTasksNoTS)
  ON_COMMAND(ID_SPECIALOPTIONS_ADJUSTFOCUSONPROBEMODECHANGE, OnAdjustFocusOnProbeModeChange)
  ON_UPDATE_COMMAND_UI(ID_SPECIALOPTIONS_ADJUSTFOCUSONPROBEMODECHANGE, OnUpdateAdjustFocusOnProbeModeChange)
  ON_COMMAND(ID_TILTSERIES_SAVEX, OnTiltseriesSaveXYZFile)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_SAVEX, OnUpdateTiltseriesSaveXYZFile)
  ON_COMMAND(ID_CAMERA_NONORMOFDSDOSEFRAC, OnCameraNoNormOfDSdoseFrac)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_NONORMOFDSDOSEFRAC, OnUpdateCameraNoNormOfDSdoseFrac)
  ON_COMMAND(ID_TILTSERIES_AUTOSAVEX, OnTiltseriesAutosaveXYZ)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_AUTOSAVEX, OnUpdateTiltseriesAutosaveXYZ)
  ON_COMMAND(ID_FOCUS_CORRECTASTIGMATISM, OnFocusCorrectAstigmatism)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_CORRECTASTIGMATISM, OnUpdateFocusCorrectAstigmatism)
  ON_COMMAND(ID_FOCUS_COMA, OnFocusComaFree)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_COMA, OnUpdateFocusComaFree)
  ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_ASTIGMATI, OnUpdateCalibrateAstigmatism)
  ON_COMMAND(ID_AUTOFOCUSFOCUS_COMA, OnCalibrateComaFree)
  ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_COMA, OnUpdateCalibrateComaFree)
  ON_COMMAND(ID_AUTOFOCUSFOCUS_ASTIGMATI, OnCalibrateAstigmatism)
  ON_COMMAND(ID_AUTOFOCUSFOCUS_SETUPCOMA, OnCalibrateSetupComaFree)
  ON_COMMAND(ID_AUTOFOCUSFOCUS_SETUPASTIGMATISM, OnCalibrateSetupAstigmatism)
  ON_COMMAND(ID_FOCUS_SETASTIGBEAMTILT, OnFocusSetAstigBeamTilt)
  ON_COMMAND(ID_FOCUS_SETCOMABEAMTILT, OnFocusSetComaBeamTilt)
  ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_SETUPCOMA, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_AUTOFOCUSFOCUS_SETUPASTIGMATISM, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETASTIGBEAMTILT, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETCOMABEAMTILT, OnUpdateNoTasks)
  ON_COMMAND(ID_TILTSERIES_RUN_MACRO, OnTiltseriesRunMacro)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_RUN_MACRO, OnUpdateTiltseriesRunMacro)
  ON_COMMAND(ID_TILTSERIES_SETMACROTORUN, OnTiltseriesSetMacroToRun)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_SETMACROTORUN, OnUpdateNoTasks)
  ON_COMMAND(ID_CORRELATIONFILTER_USEFILTERSET2, OnMontageUseFilterSet2)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_USEFILTERSET2, OnUpdateMontageUseFilterSet2)
  ON_COMMAND(ID_CORRELATIONFILTER_USESET2INLOWDOSE, OnMontageUseSet2InLowDose)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_USESET2INLOWDOSE, OnUpdateMontageUseSet2InLowDose)
  ON_COMMAND(ID_CORRELATIONFILTER_DIVIDESET1BY2, OnMontageDivideSet1By2)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_DIVIDESET1BY2, OnUpdateMontageDivideSet1By2)
  ON_COMMAND(ID_CORRELATIONFILTER_DIVIDEFILTERSET2BY2, OnMontageDivideSet2By2)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_DIVIDEFILTERSET2BY2, OnUpdateMontageDivideSet2By2)
  ON_COMMAND(ID_CORRELATIONFILTER_TESTSET1VALUES, OnMontageTestSet1Values)
  ON_COMMAND(ID_CORRELATIONFILTER_TESTSET2VALUES, OnMontageTestSet2Values)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_TESTSET1VALUES, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_TESTSET2VALUES, OnUpdateNoTasks)
  ON_COMMAND(ID_CORRELATIONFILTER_REDOCORRWHENREAD, OnMontageRedoCorrWhenRead)
  ON_UPDATE_COMMAND_UI(ID_CORRELATIONFILTER_REDOCORRWHENREAD, OnUpdateMontageRedoCorrWhenRead)
  ON_COMMAND(ID_CALIBRATION_HIGH_DEFOCUS, OnCalibrationHighDefocus)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_HIGH_DEFOCUS, OnUpdateNoTasksNoSTEM)
  ON_COMMAND(ID_CALIBRATION_LISTFOCUS, OnListFocusMagCals)
  ON_UPDATE_COMMAND_UI(ID_CALIBRATION_LISTFOCUS, OnUpdateNoTasks)
  ON_COMMAND(ID_TILTSERIES_FIXED_NUM_FRAMES, OnTiltseriesFixedNumFrames)
  ON_UPDATE_COMMAND_UI(ID_TILTSERIES_FIXED_NUM_FRAMES, OnUpdateTiltseriesFixedNumFrames)
  ON_COMMAND(ID_NAVIGATOR_WRITEASXMLFILE, OnNavigatorWriteAsXmlFile)
  ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_WRITEASXMLFILE, OnUpdateNavigatorWriteAsXmlFile)
  ON_COMMAND(ID_OPTIONS_SETPOINTLABELTHRESHOLD, OnSetPointLabelThreshold)
  ON_COMMAND(ID_CAMERA_SETFRAMEALIGNLINES, OnCameraSetFrameAlignLines)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_SETFRAMEALIGNLINES, OnUpdateNoTasks)
  ON_COMMAND(ID_SETTINGS_SETAPROPERTY, OnSettingsSetProperty)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_SETAPROPERTY, OnUpdateNoTasks)
  ON_COMMAND(ID_FOCUS_SETCOMAITERATIONS, OnFocusSetComaIterations)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETCOMAITERATIONS, OnUpdateNoTasks)
  ON_COMMAND(ID_FOCUS_RUNANOTHERITERATION, OnFocusRunAnotherIteration)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_RUNANOTHERITERATION, OnUpdateFocusRunAnotherIteration)
  ON_COMMAND(ID_FOCUS_MAKEZEMLINTABLEAU, OnFocusMakeZemlinTableau)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_MAKEZEMLINTABLEAU, OnUpdateNoTasks)
  ON_COMMAND(ID_SPECIALOPTIONS_NORMALIZEALLLENS, OnNormalizeAllLensesOnMag)
  ON_UPDATE_COMMAND_UI(ID_SPECIALOPTIONS_NORMALIZEALLLENS, OnUpdateNoTasks)
  ON_COMMAND(ID_NAVOPTIONS_USECURRENTLDPARAMSINRI, OnUseCurrentLDparamsInNavRealign)
  ON_UPDATE_COMMAND_UI(ID_NAVOPTIONS_USECURRENTLDPARAMSINRI, OnUpdateUseCurrentLDparamsInNavRealign)
  ON_COMMAND(ID_CAMERA_USEVIEWFORSEARCH, OnCameraUseViewForSearch)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_USEVIEWFORSEARCH, OnUpdateCameraUseViewForSearch)
  ON_COMMAND(ID_CAMERA_USERECORDFORMONTAGE, OnCameraUseRecordForMontage)
  ON_UPDATE_COMMAND_UI(ID_CAMERA_USERECORDFORMONTAGE, OnUpdateCameraUseRecordForMontage)
  ON_COMMAND(ID_CAMERA_SEARCH, OnCameraSearch)
  ON_COMMAND(ID_OPTIONS_SHOWMULTI, OnNavigatorShowMultiShot)
  ON_COMMAND(ID_OPTIONS_SETMULTI, OnNavigatorSetMultiShotParams)
  ON_UPDATE_COMMAND_UI(ID_OPTIONS_SETMULTI, OnUpdateNoTasksNoTS)
  ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMULTI, OnUpdateNavigatorShowMultiShot)
  ON_COMMAND(ID_FOCUSTUNING_CTF, OnCalFocusTuningCtfAstig)
  ON_UPDATE_COMMAND_UI(ID_FOCUSTUNING_CTF, OnUpdateNoTasksNoSTEM)
  ON_COMMAND(ID_FOCUS_COMA_BY_CTF, OnFocusComaByCtf)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_COMA_BY_CTF, OnUpdateNoTasksNoSTEM)
  ON_COMMAND(ID_FOCUS_SETCTFACQUIREPARAMS, OnFocusSetCtfAcquireParams)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SETCTFACQUIREPARAMS, OnUpdateNoTasks)
  ON_COMMAND(ID_FOCUS_CORRECTASTIGMATISMWITHFFTS, OnFocusCorrectAstigmatismWithFfts)
  ON_COMMAND(ID_FOCUS_SET_CTF_COMA_BT, OnFocusSetCtfComaBt)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_SET_CTF_COMA_BT, OnUpdateNoTasks)
  ON_UPDATE_COMMAND_UI(ID_FOCUS_CORRECTASTIGMATISMWITHFFTS, OnUpdateFocusCorrectAstigmatismWithFfts)
  ON_COMMAND(ID_FOCUSTUNING_COMAVS, OnCalibrateComaVsIS)
  ON_UPDATE_COMMAND_UI(ID_FOCUSTUNING_COMAVS, OnUpdateNoTasksNoSTEM)
  ON_COMMAND(ID_MONTAGINGGRIDS_SHOWWHOLEAREAFORALLPOINTS, OnShowWholeAreaForAllPoints)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_SHOWWHOLEAREAFORALLPOINTS, OnUpdateShowWholeAreaForAllPoints)
  ON_COMMAND(ID_MONTAGINGGRIDS_ADDGRIDLIKELASTONE, OnMontagingGridsAddGridLikeLastOne)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_ADDGRIDLIKELASTONE, OnUpdateOnMontagingGridsAddGridLikeLastOne)
  ON_COMMAND_RANGE(ID_EXTERNAL_TOOL1, ID_EXTERNAL_TOOL25, OnExternalTool)
  ON_UPDATE_COMMAND_UI_RANGE(ID_EXTERNAL_TOOL1, ID_EXTERNAL_TOOL25, OnUpdateNoTasks)
  ON_COMMAND(ID_SPECIALSETTINGS_ALWAYSANTIALIASK2, OnCameraAlwaysAntialiasK23)
  ON_UPDATE_COMMAND_UI(ID_SPECIALSETTINGS_ALWAYSANTIALIASK2, OnUpdateCameraAlwaysAntialiasK23)
  ON_COMMAND(ID_BEAMSPOT_LISTCALIBRATIONS, OnBeamSpotListCalibrations)
  ON_UPDATE_COMMAND_UI(ID_BEAMSPOT_LISTCALIBRATIONS, OnUpdateNoTasks)
  ON_COMMAND(ID_TASKS_SETUPWAITFORDRIFT, OnTasksSetupWaitForDrift)
  ON_UPDATE_COMMAND_UI(ID_TASKS_SETUPWAITFORDRIFT, OnUpdateNoTasks)
  ON_COMMAND(ID_SPECIALIZEDOPTIONS_SKIPBLANKINGINLDWITHSCREENUP, OnSkipBlankingInLdWithScreenUp)
  ON_UPDATE_COMMAND_UI(ID_SPECIALIZEDOPTIONS_SKIPBLANKINGINLDWITHSCREENUP, OnUpdateSkipBlankingInLdWithScreenUp)
  ON_COMMAND(ID_TASKS_SETUPVPPCONDITIONING, OnTasksSetupVppConditioning)
  ON_UPDATE_COMMAND_UI(ID_TASKS_SETUPVPPCONDITIONING, OnUpdateTasksSetupVppConditioning)
  ON_COMMAND(ID_WINDOW_TAKESCREENSHOT, OnWindowTakeScreenShot)
  ON_UPDATE_COMMAND_UI(ID_WINDOW_TAKESCREENSHOT, OnUpdateNoTasks)
  ON_COMMAND(ID_MONTAGINGGRIDS_FINDREGULARARRAYOFHOLES, OnMontagingGridsFindHoles)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_FINDREGULARARRAYOFHOLES, OnUpdateMontagingGridsFindHoles)
  ON_COMMAND(ID_MONTAGINGGRIDS_COMBINEPOINTSINTOMULTI, OnCombinePointsIntoMultiShots)
  ON_UPDATE_COMMAND_UI(ID_MONTAGINGGRIDS_COMBINEPOINTSINTOMULTI, OnUpdateMontagingGridsFindHoles)
    ON_COMMAND(ID_CAMERA_SETEXTRADIVISIONBY2, OnCameraSetExtraDivisionBy2)
    ON_UPDATE_COMMAND_UI(ID_CAMERA_SETEXTRADIVISIONBY2, OnUpdateCameraSetExtraDivisionBy2)
    ON_COMMAND(ID_CAMERA_RETURNFLOATIMAGE, OnCameraReturnFloatImage)
    ON_UPDATE_COMMAND_UI(ID_CAMERA_RETURNFLOATIMAGE, OnUpdateCameraReturnFloatImage)
    ON_COMMAND(ID_BEAMSPOT_REFINEBEAMSHIFT, OnBeamspotRefineBeamShift)
    ON_UPDATE_COMMAND_UI(ID_BEAMSPOT_REFINEBEAMSHIFT, OnUpdateBeamspotRefineBeamShift)
    ON_COMMAND(ID_BEAMSPOT_ILLUMINATEDAREALIMITS, OnBeamspotIlluminatedAreaLimits)
    ON_UPDATE_COMMAND_UI(ID_BEAMSPOT_ILLUMINATEDAREALIMITS, OnUpdateIlluminatedAreaLimits)
    ON_COMMAND(ID_OPTIONS_SEARCH, OnOptionsSearchPlusMinus)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_SEARCH, OnUpdateOptionsSearchPlusMinus)
    ON_COMMAND(IDM_MARKER_TO_CENTER, OnMarkerToCenter)
    ON_COMMAND(ID_OPTIONS_USEITEMLABELSINFILENAMES, OnUseItemLabelsInFilenames)
    ON_UPDATE_COMMAND_UI(ID_OPTIONS_USEITEMLABELSINFILENAMES, OnUpdateUseItemLabelsInFilenames)
    ON_COMMAND(ID_SPECIALIZEDOPTIONS_CLOSEVALVESAFTERLONGINACTIVITY, OnCloseValvesAfterLongInactivity)
    ON_UPDATE_COMMAND_UI(ID_SPECIALIZEDOPTIONS_CLOSEVALVESAFTERLONGINACTIVITY, OnUpdateCloseValvesAfterLongInactivity)
    ON_COMMAND(ID_NAVIGATOR_APPLYSAVEDSHIFT, OnNavigatorApplySavedShift)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_APPLYSAVEDSHIFT, OnUpdateNavigatorApplySavedShift)
    ON_COMMAND(ID_EUCENTRICITY_EUCENTRICITYBYFOCUS, OnEucentricityByFocus)
    ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_EUCENTRICITYBYFOCUS, OnUpdateEucentricityByFocus)
    ON_COMMAND(ID_EUCENTRICITY_SETUPEUCENTRICITYBYFOCUS, OnSetupEucentricityByFocus)
    ON_UPDATE_COMMAND_UI(ID_EUCENTRICITY_SETUPEUCENTRICITYBYFOCUS, OnUpdateNoTasksNoSTEM)
    ON_COMMAND(ID_NAVIGATOR_SETUPALIGN, OnNavigatorSetupAlign)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATOR_SETUPALIGN, OnUpdateNoTasks)
    ON_COMMAND(ID_TASKS_SETUPSCOPEMANAGEMENT, OnSetupScopeManagement)
    ON_UPDATE_COMMAND_UI(ID_TASKS_SETUPSCOPEMANAGEMENT, OnUpdateSetupScopeManagement)
    END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMenuTargets message handlers

BOOL CMenuTargets::DoingTasks()
{
  return mWinApp->DoingTasks();
}

void CMenuTargets::OnUpdateNoTasks(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks());  
}
void CMenuTargets::OnUpdateNoTasksNoTS(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries());  
}

void CMenuTargets::OnUpdateNoTasksNoSTEM(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnUpdateNoTasksNoTSnoSTEM(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && 
    !mWinApp->StartedTiltSeries());
}


// NAVIGATOR WINDOW

void CMenuTargets::OnTasksNavigator() 
{
  WINDOWPLACEMENT *placement = mWinApp->GetNavPlacement();
  NavParams *param = mWinApp->GetNavParams();
  if (!param->stockFile.IsEmpty()) {
    CString mess = "You have a file of 'stock states' in the Navigator file:\r\n" +
    param->stockFile + "\r\nAcquisition states are now kept in your settings file "
    "instead and\r\naccessed through the Imaging and Map Acquire State dialog.\r\n"
    "\r\nTo transfer your stock states:\r\n"
    "1) Open that file in the Navigator\r\n2) Open the State dialog\r\n"
    "3) Select an item in the Navigator\r\n4) Press 'Add Nav Item State' in the State"
    " dialog\r\n5) Repeat until all desired states are transferred\r\n\r\n"
    "This is the only time that this message will appear";
    mWinApp->AppendToLog(mess, LOG_OPEN_IF_CLOSED);
    AfxMessageBox(mess, MB_OK | MB_ICONINFORMATION);
    param->stockFile = "";
  }
  mNavigator = new CNavigatorDlg();
  mWinApp->mNavigator = mNavigator;
  mNavHelper->NavOpeningOrClosing(true);
  mNavigator->Create(IDD_NAVIGATOR);
  if (placement->rcNormalPosition.right > 0)
    mNavigator->SetWindowPlacement(placement);
  mNavigator->ShowWindow(SW_SHOW);
  if (mWinApp->GetOpenStateWithNav())
    mNavHelper->OpenStateDialog();
  if (!param->autosaveFile.IsEmpty())
    mNavigator->LoadNavFile(true, false);
  mWinApp->RestoreViewFocus();
}

// External call to get the Navigator open
void CMenuTargets::OpenNavigatorIfClosed(void)
{
  if (!mNavigator)
    OnTasksNavigator();
}

void CMenuTargets::OnUpdateTasksNavigator(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator == NULL); 
}

void CMenuTargets::OnUpdateLoadNavFile(CCmdUI* pCmdUI) 
{
  BOOL bEnable = !DoingTasks();
  if (mNavigator)
    bEnable = bEnable && mNavigator->NoDrawing()  && !mNavigator->GetAcquiring();
  pCmdUI->Enable(bEnable);	
}

void CMenuTargets::OnLoadNavFile() 
{
  NavParams *param = mWinApp->GetNavParams();
  bool opened = !mNavigator;
  if (!mNavigator)
    OnTasksNavigator();
  if (!opened || param->autosaveFile.IsEmpty())
    mNavigator->LoadNavFile(false, false);
}

void CMenuTargets::OnNavigatorMergefile()
{
  mNavigator->LoadNavFile(false, true);
}

void CMenuTargets::OnUpdateNavigatorMergefile(CCmdUI *pCmdUI)
{
  int navState = mWinApp->mCameraMacroTools.GetNavigatorState();
  pCmdUI->Enable(!DoingTasks() && mNavigator && mNavigator->NoDrawing() && 
    (!mNavigator->GetAcquiring() || navState == NAV_PAUSED || 
    navState == NAV_SCRIPT_STOPPED || navState == NAV_TS_STOPPED || 
    navState == NAV_PRE_TS_STOPPED));
}

void CMenuTargets::OnNavigatorOpenStateDlg()
{
  mNavHelper->OpenStateDialog();
}

void CMenuTargets::OnUpdateNavigatorOpenStateDlg(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mNavHelper->mStateDlg && !DoingTasks());
}

void CMenuTargets::OnNavigatorImportmap()
{
  if (!mNavigator)
    OnTasksNavigator();
  mNavigator->ImportMap();
}

void CMenuTargets::OnSaveNavFile() 
{
  mNavigator->DoSave();	
}

void CMenuTargets::OnSaveasNavFile() 
{
  mNavigator->DoSaveAs();	
}

void CMenuTargets::OnUpdateSaveNavFile(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && mNavigator->GetNumberOfItems() > 0 &&
    !DoingTasks());  	
}

void CMenuTargets::OnNavigatorClose() 
{
  mNavigator->DoClose();
}

void CMenuTargets::OnUpdateNavigatorClose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && !mNavigator->GetAcquiring() &&
    mNavigator->NoDrawing() && !mNavigator->GetLoadingMap());
}

void CMenuTargets::OnNavigatorAutosave() 
{
  mWinApp->mDocWnd->SetAutoSaveNav(!mWinApp->mDocWnd->GetAutoSaveNav());  	
}

void CMenuTargets::OnUpdateNavigatorAutosave(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(mWinApp->mDocWnd->GetAutoSaveNav());
}

void CMenuTargets::OnNavigatorWriteAsXmlFile()
{
  mNavHelper->SetWriteNavAsXML(!mNavHelper->GetWriteNavAsXML());
}

void CMenuTargets::OnUpdateNavigatorWriteAsXmlFile(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mNavHelper->GetWriteNavAsXML()); 
}

void CMenuTargets::OnCornerMontage() 
{
  mNavigator->CornerMontage();	
}

void CMenuTargets::OnUpdateCornerMontage(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && mNavigator->CornerMontageOK() && mNavigator->NoDrawing() 
    && !DoingTasks() && !mNavigator->GetAcquiring());	
}

void CMenuTargets::OnNavigatorPolygonFromCorners()
{
  mNavigator->PolygonFromCorners();
}

void CMenuTargets::OnPolygonMontage() 
{
  mNavigator->PolygonMontage(NULL, false);	
}

void CMenuTargets::OnUpdatePolygonMontage(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && mNavigator->GetItemType() == ITEM_TYPE_POLYGON &&
    mNavigator->NoDrawing() && !DoingTasks() && !mNavigator->GetAcquiring());	
}

void CMenuTargets::OnNavigatorSetupfullmontage() 
{
  mNavigator->FullMontage();	
}

void CMenuTargets::OnUpdateNavigatorSetupfullmontage(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && !DoingTasks() &&
    !mNavigator->GetAcquiring());	
}

void CMenuTargets::OnMontageListFilesToOpen()
{
  mNavHelper->ListFilesToOpen();
}

void CMenuTargets::OnNavigatorAcquire() 
{
  mNavigator->AcquireAreas(true, false);	
}

void CMenuTargets::OnUpdateNavigatorAcquire(CCmdUI* pCmdUI) 
{
  int navState = mWinApp->mCameraMacroTools.GetNavigatorState();
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && (mNavigator->AcquireOK(false)
    || mNavigator->AcquireOK(true)) && (!mNavigator->GetAcquiring() || 
      navState == NAV_PAUSED) && !mNavigator->StartedMacro() && 
      (!DoingTasks() || mWinApp->GetJustNavAcquireOpen()) && 
    !mWinApp->StartedTiltSeries() && !mCamera->CameraBusy());	
}

void CMenuTargets::OnNavigatorEndacquire()
{
  if (mNavigator->GetPausedAcquire())
    mNavigator->EndAcquireWithMessage();
  else
    mNavigator->SetAcquireEnded(1);
}

void CMenuTargets::OnUpdateNavigatorEndacquire(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->GetAcquiring());
}

void CMenuTargets::OnNewMap() 
{
  mNavigator->NewMap();	
}

void CMenuTargets::OnUpdateNewMap(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() &&
    !DoingTasks() && mWinApp->mStoreMRC && !mNavigator->GetAcquiring());	
}

void CMenuTargets::OnNavigatorAdjustBacklash()
{
  mNavigator->AdjustBacklash(-1, true);
}

void CMenuTargets::OnUpdateNavigatorAdjustBacklash(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && !mNavigator->StartedMacro() && 
    !DoingTasks() && !mWinApp->StartedTiltSeries() && !mCamera->CameraBusy() &&
    !mNavigator->GetAcquiring() && mNavigator->OKtoAdjustBacklash(true));
}

void CMenuTargets::OnNavigatorBacklashSettings()
{
  CNavBacklashDlg dlg;
  dlg.m_iAskOrAuto = mNavHelper->GetAutoBacklashNewMap();
  dlg.m_fMinField = mNavHelper->GetAutoBacklashMinField();
  if (dlg.DoModal() == IDOK) {
    mNavHelper->SetAutoBacklashNewMap(dlg.m_iAskOrAuto);
    mNavHelper->SetAutoBacklashMinField(dlg.m_fMinField);
  }
}

void CMenuTargets::OnTransformPts() 
{
  mNavigator->TransformPts();	
}

void CMenuTargets::OnUpdateTransformPts(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && !mNavigator->GetAcquiring() &&
    mNavigator->NoDrawing()	&& mNavigator->TransformOK()); 
}

void CMenuTargets::OnNavigatorUndotransformation()
{
  mNavigator->UndoTransform();
}

void CMenuTargets::OnUpdateNavigatorUndotransformation(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && !mNavigator->GetAcquiring() &&
    mNavigator->NoDrawing()	&& mNavigator->GetNumSavedRegXforms()); 
}

void CMenuTargets::OnDeleteitem() 
{
  mNavigator->DeleteItem();	
}

void CMenuTargets::OnUpdateDeleteitem(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mNavigator && !mNavigator->GetAcquiring() && 
    !mNavHelper->GetRealigning() && !mNavigator->GetLoadingMap() &&
    mNavigator->NoDrawing()	&& mNavigator->GetItemType() >= 0);	
}

void CMenuTargets::OnNavigatorConvertmaps() 
{
  mNavHelper->SetConvertMaps(!mNavHelper->GetConvertMaps());
}

void CMenuTargets::OnUpdateNavigatorConvertmaps(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mNavHelper->GetConvertMaps() ? 1 : 0);
}

void CMenuTargets::OnNavigatorLoadunbinned()
{
  mNavHelper->SetLoadMapsUnbinned(!mNavHelper->GetLoadMapsUnbinned());
}

void CMenuTargets::OnUpdateNavigatorLoadunbinned(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mNavHelper->GetLoadMapsUnbinned() ? 1 : 0);
}

void CMenuTargets::OnSetPointLabelThreshold()
{
  int thresh = mNavHelper->GetPointLabelDrawThresh();
  if (!KGetOneInt("Enter maximum group size for which to draw point labels (or 0 to draw "
    "always):", thresh))
    return;
  mNavHelper->SetPointLabelDrawThresh(thresh);
  mWinApp->mMainView->DrawImage();
}

void CMenuTargets::OnUseItemLabelsInFilenames()
{
  mNavHelper->SetUseLabelInFilenames(!mNavHelper->GetUseLabelInFilenames());
}

void CMenuTargets::OnUpdateUseItemLabelsInFilenames(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mNavHelper->GetUseLabelInFilenames() ? 1 : 0);
}

void CMenuTargets::OnNavigatorShowMultiShot()
{
  b3dUInt32 multi = mNavHelper->GetEnableMultiShot();
  setOrClearFlags(&multi, 1, (multi & 1) ? 0  : 1);
  mNavHelper->SetEnableMultiShot(multi);
  mWinApp->mMainView->DrawImage();
}

void CMenuTargets::OnUpdateNavigatorShowMultiShot(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck((mNavHelper->GetEnableMultiShot() & 1) != 0 ? 1 : 0);
}

void CMenuTargets::OnShowWholeAreaForAllPoints()
{
  b3dUInt32 multi = mNavHelper->GetEnableMultiShot();
  setOrClearFlags(&multi, 2, (multi & 2) ? 0 : 1);
  mNavHelper->SetEnableMultiShot(multi);
  mWinApp->mMainView->DrawImage();
}

void CMenuTargets::OnUpdateShowWholeAreaForAllPoints(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck((mNavHelper->GetEnableMultiShot() & 2) != 0 ? 1 : 0);
}

void CMenuTargets::OnNavigatorSetMultiShotParams()
{
  mWinApp->mNavHelper->OpenMultishotDlg();
}

void CMenuTargets::OnMontagingGridsFindHoles()
{
  mWinApp->mNavHelper->OpenHoleFinder();
}

void CMenuTargets::OnUpdateMontagingGridsFindHoles(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->mNavigator);
}

void CMenuTargets::OnCombinePointsIntoMultiShots()
{
  mWinApp->mNavHelper->OpenMultiCombiner();
}

void CMenuTargets::OnNavigatorRealignScaling()
{
  mNavHelper->SetTryRealignScaling(!mNavHelper->GetTryRealignScaling());
  /*int options = mNavHelper->GetRealignTestOptions();
  if (KGetOneInt("Enter sum of 1 to use montage backlash and 2 to use montage stage "
    "errors", options))
    mNavHelper->SetRealignTestOptions(options); */
}

void CMenuTargets::OnUpdateNavigatorRealignScaling(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mNavHelper->GetTryRealignScaling() ? 1 : 0);
}

void CMenuTargets::OnNavigatorAlignedsupermontage() 
{
  mNavigator->SetupSuperMontage(false);
}

void CMenuTargets::OnNavigatorSkewedsupermontage() 
{
  mNavigator->SetupSuperMontage(true);
}

void CMenuTargets::OnUpdateNavigatorSkewedsupermontage(CCmdUI* pCmdUI) 
{
 	EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing()	&& !mNavigator->GetAcquiring() &&
    !DoingTasks() && imBuf && imBuf->mHasUserPt && mWinApp->Montaging());
}

void CMenuTargets::OnPolygonsupermontage()
{
  mNavigator->PolygonSupermontage();
}

void CMenuTargets::OnUpdatePolygonsupermontage(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->GetItemType() == ITEM_TYPE_POLYGON &&
    mNavigator->NoDrawing() && !DoingTasks() && mWinApp->Montaging()  &&
    !mNavigator->GetAcquiring());	
}

void CMenuTargets::OnNavigatorAddcirclepolygon()
{
  static float radius = 0.;
  if (!KGetOneFloat("Radius of circle polygon to add (microns):", radius, 2))
    return;
  if (radius <= 0)
    return;
  mNavigator->AddCirclePolygon(radius);
}

void CMenuTargets::OnUpdateNavigatorAddCirclePolygon(CCmdUI *pCmdUI)
{
 	EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && !mNavigator->GetAcquiring() &&
    !DoingTasks() && imBuf && imBuf->mHasUserPt);
}

void CMenuTargets::OnNavigatorAddGridOfPoints()
{
  mNavigator->AddGridOfPoints(false);
}

void CMenuTargets::OnUpdateNavigatorAddGridOfPoints(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->OKtoAddGrid(false) && !DoingTasks() &&
    !mNavigator->GetAcquiring());
}

void CMenuTargets::OnMontagingGridsAddGridLikeLastOne()
{
  mNavigator->AddGridOfPoints(true);
}

void CMenuTargets::OnUpdateOnMontagingGridsAddGridLikeLastOne(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->OKtoAddGrid(true) && !DoingTasks() &&
    !mNavigator->GetAcquiring());
}

void CMenuTargets::OnNavigatorDivideIntoGroups()
{
  mNavHelper->SetDivideIntoGroups(!mNavHelper->GetDivideIntoGroups());
}

void CMenuTargets::OnUpdateNavigatorDivideIntoGroups(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mNavHelper->GetDivideIntoGroups());
}

void CMenuTargets::OnNavigatorSetGridGroupSize()
{
  float size = mNavHelper->GetGridGroupSize();
  if (!KGetOneFloat("The radius is the maximum distance from first point to any other one"
    , "Enter maximum radius of groups in microns:", size, 1))
    return;
  if (size <= 0)
    return;
  mNavHelper->SetGridGroupSize(size);
}

void CMenuTargets::OnNavigatorChangeregistration()
{
  mNavigator->ChangeItemRegistration();
}

void CMenuTargets::OnUpdateNavigatorChangeregistration(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->RegistrationChangeOK() &&
    !mNavigator->GetAcquiring());
}

void CMenuTargets::OnNavigatorRotatemap()
{
  mNavigator->RotateMap(mWinApp->mActiveView->GetActiveImBuf(), true);
}

void CMenuTargets::OnUpdateNavigatorRotatemap(CCmdUI *pCmdUI)
{
 	EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing()	&&
    !DoingTasks() && imBuf && mNavigator->FindItemWithMapID(imBuf->mMapID)
     && !mCamera->CameraBusy());
}

void CMenuTargets::OnNavigatorAligntoitem()
{
  mWinApp->mNavigator->RealignToCurrentItem(true, 0., 0, 0, false);
}

void CMenuTargets::OnUpdateNavigatorAligntoitem(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && 
    mNavigator->GetItemType() >= 0 && !DoingTasks() && !mCamera->CameraBusy());	
}

void CMenuTargets::OnNavigatorForceCenterAlign()
{
  mNavHelper->ForceCenterRealign();
}

void CMenuTargets::OnNavigatorShifttomarker()
{
  mNavigator->ShiftToMarker();
}

void CMenuTargets::OnUpdateNavigatorShifttomarker(CCmdUI *pCmdUI)
{
 	EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && !mNavigator->GetAcquiring() &&
    !DoingTasks() && imBuf && imBuf->mHasUserPt && mNavigator->GetItemType() >= 0);
}

void CMenuTargets::OnNavigatorUndolastshift()
{
  mNavigator->UndoShiftToMarker();
}

void CMenuTargets::OnUpdateNavigatorUndolastshift(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing() && !mNavigator->GetAcquiring() &&
    !DoingTasks() && mNavigator->GetMarkerShiftReg());
}

void CMenuTargets::OnNavigatorApplySavedShift()
{
  mNavHelper->ApplyBaseMarkerShift();
}

void CMenuTargets::OnUpdateNavigatorApplySavedShift(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavHelper->OKtoApplyBaseMarkerShift());
}

void CMenuTargets::OnNavigatorSetgridlimits()
{
  char *limText[4] = {"Lower X", "Upper X", "Lower Y", "Upper Y"};
  CString str;
  float *limits = mNavHelper->GetGridLimits();
  for (int i = 0; i < 4; i++) {
    str.Format("%s limit in microns for full montage (enter 0 for default limit of %.0f)"
      , limText[i], mScope->GetStageLimit(i));
    if (!KGetOneFloat(str, limits[i], 0))
      return;
    if (((i % 2) ? 1 : -1) * (limits[i] - mScope->GetStageLimit(i)) > 0)
      AfxMessageBox("You need to set or alter the StageLimits entry in "
      "SerialEMproperties.txt\nin order for an entry beyond the default limit to have "
      "any effect", MB_EXCLAME);
  }
}

void CMenuTargets::OnNavigatorSetacquirestate()
{
  CMapDrawItem *item;
  if (mNavigator->GetCurrentOrAcquireItem(item) < 0)
    return;
  mNavHelper->SetToMapImagingState(item, true);
}

void CMenuTargets::OnUpdateNavigatorSetacquirestate(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing()	&& !DoingTasks() && 
    !mNavigator->GetAcquiring() && !mNavigator->CurrentIsImported() &&
    !mWinApp->StartedTiltSeries() && mNavigator->GetItemType() == ITEM_TYPE_MAP &&
    mNavHelper->GetTypeOfSavedState() == STATE_NONE);
}

void CMenuTargets::OnNavigatorRestorestate()
{
  if (mNavHelper->GetTypeOfSavedState() == STATE_MAP_ACQUIRE)
    mNavHelper->RestoreFromMapState();
  else
    mNavHelper->RestoreSavedState();
}

void CMenuTargets::OnUpdateNavigatorRestorestate(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->NoDrawing()	&& !DoingTasks() && 
    !mNavigator->GetAcquiring() &&
    !mWinApp->StartedTiltSeries() && mNavHelper->GetTypeOfSavedState() !=
    STATE_NONE);
}

// Deleted group acquire and delete from menu because to the extent they are not 
// redundant to the delete button with collapsed groups, the difference is confusing
void CMenuTargets::OnNavigatorGroupAcquire()
{
  if (mNavigator)
    mNavigator->ToggleGroupAcquire(false);
}

void CMenuTargets::OnUpdateNavigatorGroupAcquire(CCmdUI *pCmdUI)
{
  CMapDrawItem *item;
  BOOL enable = false;
  BOOL checked = false;
  if (mNavigator && mNavigator->NoDrawing() && !DoingTasks() && 
    !mWinApp->DoingComplexTasks() && mNavigator->GetItemType() != ITEM_TYPE_POLYGON &&
    mNavigator->GetCurrentOrGroupItem(item) >= 0 && !mNavigator->GetAcquiring()) {
    enable = item->mGroupID > 0;
    checked = enable && item->mAcquire;
  }
  pCmdUI->Enable(enable);
  pCmdUI->SetCheck(checked);
}

void CMenuTargets::OnNavigatorDeleteGroup()
{
  mNavigator->DeleteGroup(false);
}

void CMenuTargets::OnUpdateNavigatorDeleteGroup(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && mNavigator->CurrentIsInGroup() &&
    !mNavigator->GetAcquiring());
}

void CMenuTargets::OnNavigatorAlignToMap()
{
  if (!mNavHelper->OKtoAlignWithRotation()) {
    AfxMessageBox("To open this dialog, there must be a map in the Read buffer\n"
      "and an image in buffer A or a montage overview in B\n(and montage center in A) at"
      " a different registration from the map", MB_OK | MB_ICONINFORMATION);
    return;
  }
  mNavHelper->OpenRotAlignDlg();
}

void CMenuTargets::OnUpdateNavigatorAlignToMap(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && !DoingTasks() && !mNavigator->GetAcquiring());
}

void CMenuTargets::OnCalibrationStagestretch()
{
  mNavigator->XformForStageStretch(true);
}

void CMenuTargets::OnUpdateCalibrationStagestretch(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mNavigator && !DoingTasks() && mNavigator->XformForStageStretch(false));
}

void CMenuTargets::OnUseCurrentLDparamsInNavRealign()
{
  mNavHelper->SetRIuseCurrentLDparams(
    !mNavHelper->GetRIuseCurrentLDparams());
}

void CMenuTargets::OnUpdateUseCurrentLDparamsInNavRealign(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mNavHelper->GetRIuseCurrentLDparams());
}

void CMenuTargets::OnOptionsSearchPlusMinus()
{
  mNavHelper->SetPlusMinusRIScaling(!mNavHelper->GetPlusMinusRIScaling());
}

void CMenuTargets::OnUpdateOptionsSearchPlusMinus(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mNavHelper->GetPlusMinusRIScaling());
}

void CMenuTargets::OnNavigatorSetupAlign()
{
  CNavRealignDlg dlg;
  dlg.DoModal();
}

// DISTORTION
void CMenuTargets::OnCalibrationDistortion() 
{
  mWinApp->mDistortionTasks->CalibrateDistortion();
}

void CMenuTargets::OnCalibrationSetoverlaps() 
{
  mWinApp->mDistortionTasks->SetOverlaps();	
}

// FILTER TASKS
void CMenuTargets::OnCalibrationMagenergyshifts() 
{
  mWinApp->mFilterTasks->CalibrateMagShift();  
}

void CMenuTargets::OnUpdateCalibrationMagenergyshifts(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mWinApp->GetFilterMode() && !mWinApp->LowDoseMode() && 
    !DoingTasks() && !mCamera->CameraBusy());
}

// GAIN REF MAKER
void CMenuTargets::OnAcquiregainref() 
{
  CameraParameters *params = mWinApp->GetActiveCamParam();
  if (params->DE_camType && !mCamera->CanProcessHere(params))
    mWinApp->mGainRefMaker->MakeRefInDEserver();
  else
    mWinApp->mGainRefMaker->AcquireGainRef();
}

void CMenuTargets::OnUpdateAcquiregainref(CCmdUI* pCmdUI) 
{
  CameraParameters *params = mWinApp->GetActiveCamParam();
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() && !mWinApp->GetSTEMMode() &&
    (mCamera->GetProcessHere() || 
    (params->DE_camType && !mCamera->CanProcessHere(params))));
}

// BEAM ASSESSOR
void CMenuTargets::OnCalibrationBeamintensity() 
{
  mWinApp->mBeamAssessor->CalIntensityCCD();  
}

void CMenuTargets::OnUpdateCalibrationBeamintensity(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() && !mScope->GetJeol1230() &&
    !mWinApp->GetSTEMMode());  
}

void CMenuTargets::OnCalibrationSetApertureSize()
{
  int newAp = mWinApp->mBeamAssessor->RequestApertureSize();
  if (newAp) {
    mWinApp->mBeamAssessor->ScaleTablesForAperture(newAp, false);
    mWinApp->mDocWnd->SetShortTermNotSaved();
  }
}

void CMenuTargets::OnUpdateCalibrationSetApertureSize(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks() && mScope->GetUseIllumAreaForC2());
}

void CMenuTargets::OnCalibrationBeamshift() 
{
  mWinApp->mBeamAssessor->CalibrateBeamShift();  
}

void CMenuTargets::OnUpdateCalibrationBeamshift(CCmdUI* pCmdUI) 
{
  ScaleMat mat = mShiftManager->IStoCamera(mScope->FastMagIndex());
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() && 
    mat.xpx != 0. && !mScope->GetJeol1230() && !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnBeamspotRefineBeamShift()
{
  mWinApp->mBeamAssessor->RefineBeamShiftCal();
}


void CMenuTargets::OnUpdateBeamspotRefineBeamShift(CCmdUI *pCmdUI)
{
  int magInd = mScope->FastMagIndex();
  ScaleMat mat = mShiftManager->GetBeamShiftCal(magInd);
  ScaleMat isMat = mShiftManager->IStoSpecimen(magInd);
  ScaleMat camMat = mShiftManager->CameraToIS(magInd);
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() && isMat.xpx != 0. &&
    mat.xpx != 0. && camMat.xpx != 0. && !FEIscope && !mScope->GetJeol1230() && 
    !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnCalibrationElectrondose()
{
  mWinApp->mBeamAssessor->CalibrateElectronDose(true);  
}

void CMenuTargets::OnCalibrationSpotintensity() 
{
  mWinApp->mBeamAssessor->CalibrateSpotIntensity();
}

// Remove all dose calibrations from before program startup
void CMenuTargets::OnCalibrationPurgeoldcalibs() 
{
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  int index;
  int stamp = (int)((mWinApp->mDocWnd->GetStartupTime()).GetTime() / 60);
  for (index = 0; index < 2 * mScope->GetNumSpotSizes(); index++)
    if (doseTables[index].dose && doseTables[index].timeStamp < stamp) {
      doseTables[index].dose = 0.;
      mWinApp->mDocWnd->SetShortTermNotSaved();
    }
}

// SHIFT MANAGER AND CALIBRATOR
void CMenuTargets::OnCalibrationImageshift() 
{
  mWinApp->mShiftCalibrator->CalibrateIS(0, false, false);
}
void CMenuTargets::OnCalibrationIsfromscratch() 
{
  mWinApp->mShiftCalibrator->CalibrateIS(-1, false, false);
}

void CMenuTargets::OnCalibrationStageshift() 
{
  mWinApp->mShiftCalibrator->CalibrateIS(0, true, false);
}

void CMenuTargets::OnCalibrationWholeImageCorr()
{
  mWinApp->mShiftCalibrator->SetWholeCorrForStageCal(
    !mWinApp->mShiftCalibrator->GetWholeCorrForStageCal());
}

void CMenuTargets::OnUpdateCalibrationWholeImageCorr(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mWinApp->mShiftCalibrator->GetWholeCorrForStageCal());
}

void CMenuTargets::OnCalibrationSetCycleLengths()
{
  float length = mWinApp->mShiftCalibrator->GetStageCycleX();
  if (!KGetOneFloat("Cycle lengths entered here are not saved when you exit.",
    "Cycle length for stage calibration in X:", length, 1))
    return;
  mWinApp->mShiftCalibrator->SetStageCycleX(length);
  length = mWinApp->mShiftCalibrator->GetStageCycleY();
  if (!KGetOneFloat("Cycle length for stage calibration in Y:", length, 1))
    return;
  mWinApp->mShiftCalibrator->SetStageCycleY(length);
}

void CMenuTargets::OnUseTrialBinning()
{
  mWinApp->mShiftCalibrator->SetUseTrialBinning(
    !mWinApp->mShiftCalibrator->GetUseTrialBinning());
}

void CMenuTargets::OnUseTrialSize()
{
  mWinApp->mShiftCalibrator->SetUseTrialSize(
    !mWinApp->mShiftCalibrator->GetUseTrialSize());
}

void CMenuTargets::OnUpdateUseTrialBinning(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mWinApp->mShiftCalibrator->GetUseTrialBinning() ? 1 : 0);
}

void CMenuTargets::OnUpdateUseTrialSize(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mWinApp->mShiftCalibrator->GetUseTrialSize() ? 1 : 0);
}

void CMenuTargets::OnCalibrationMagisoffsets()
{
  mWinApp->mShiftCalibrator->CalibrateISoffset(); 
}

void CMenuTargets::OnCalibrationEftemISoffset()
{
  mWinApp->mShiftCalibrator->CalibrateEFTEMoffset();
}

void CMenuTargets::OnUpdateCalibrationEftemISoffset(CCmdUI *pCmdUI)
{
  FilterParams *filtParam = mWinApp->GetFilterParams();
  pCmdUI->Enable(!DoingTasks() && filtParam->firstGIFCamera >= 0 && 
    filtParam->firstRegularCamera >= 0 && !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnAutoalign() 
{
  mShiftManager->AutoAlign(0, 0);
}

// List image shift vectors as lengths and angles on specimen
void CMenuTargets::OnCalibrationListisvectors()
{
  BOOL useCalPixel = AfxMessageBox("Do you want to use calibrated pixel sizes where "
    "available\ninstead of nominal pixel sizes calculated from the magnification?",
    MB_YESNO, MB_ICONQUESTION) == IDYES;
  DoListISVectors(useCalPixel);
}

void CMenuTargets::DoListISVectors(BOOL useCalPixel)
{
  CString str;
  int numCam = mWinApp->GetNumActiveCameras();
  int *active = mWinApp->GetActiveCameraList();
  CameraParameters *camP = mWinApp->GetCamParams();
  MagTable *magTab = mWinApp->GetMagTable();
  ScaleMat mat, rot;
  int iCam, mag, lowestMicro, limlo, limhi, numRanges, range;
  double xvec, yvec, xang, yang, pixel, rough, rotation, calRot;

  for (int actCam = 0; actCam < numCam; actCam++) {
    iCam = active[actCam];
    str.Format("\r\nImage shift calibration vectors for %s  (using %s pixel sizes)",
      camP[iCam].name, useCalPixel ? "calibrated" : "mag-derived");
    mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
    mWinApp->AppendToLog("Index  Mag     X length     Y length   X angle  Y angle",
      LOG_OPEN_IF_CLOSED);
    mWinApp->GetNumMagRanges(iCam, numRanges, lowestMicro);
    for (range = 0; range < numRanges; range++) {
      mWinApp->GetMagRangeLimits(iCam, range * lowestMicro, limlo, limhi);
      for (int i = limlo; i <= limhi; i++) {
        mat = magTab[i].matIS[iCam];
        if (!mat.xpx)
          continue;
        mag = MagForCamera(iCam, i);
        if (camP[iCam].STEMcamera)
          rough = mShiftManager->GetSTEMRoughISscale();
        else
          rough = i < mScope->GetLowestMModeMagInd() ? 
          mShiftManager->GetLMRoughISscale() : mShiftManager->GetRoughISscale();
        if (useCalPixel && magTab[i].pixelSize[iCam] > 0.0 && !magTab[i].pixDerived[iCam])
          pixel = magTab[i].pixelSize[iCam];
        else
          pixel = camP[iCam].pixelMicrons / (mag * camP[iCam].magRatio);

        // Get fallback or calibrated rotation angle
        rotation = (camP[iCam].GIF ?
          magTab[i].EFTEMtecnaiRotation : magTab[i].tecnaiRotation) + 
          mShiftManager->GetGlobalExtraRotation() + camP[iCam].extraRotation;
        calRot = mShiftManager->GetCalibratedImageRotation(iCam, i);
        if (calRot < 900. && useCalPixel)
          rotation = calRot;
        if (camP[iCam].STEMcamera)
          rotation = camP[iCam].imageRotation;

        // Rotate the matrix by the negative of the rotation
        rot.xpx = (float)cos(DTOR * rotation);
        rot.xpy = (float)sin(DTOR * rotation);
        rot.ypy = rot.xpx;
        rot.ypx = -rot.xpy;
        mat = mShiftManager->MatMul(mat, rot);

        // Scale the vectors to be microns on specimen per nominal micron of image shift
        xvec = pixel * sqrt((double)(mat.xpx * mat.xpx + mat.ypx * mat.ypx)) / rough;
        yvec = pixel * sqrt((double)(mat.xpy * mat.xpy + mat.ypy * mat.ypy)) / rough;
        xang = atan2(mat.xpx, mat.ypx) / DTOR;
        yang = atan2(mat.xpy, mat.ypy) / DTOR;
        str.Format("  %2d %7d   %10.3f     %10.3f   %8.1f    %8.1f", 
          i, mag, xvec, yvec, xang, yang);
        mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
      }
    }
  }
}

// List specimen to stage matrices for all stage calibrations
void CMenuTargets::OnCalibrationListstagecals()
{
  DoListStageCals();
}

void CMenuTargets::DoListStageCals()
{
  CString str;
  int numCam = mWinApp->GetNumActiveCameras();
  int *active = mWinApp->GetActiveCameraList();
  CameraParameters *camP = mWinApp->GetCamParams();
  MagTable *magTab = mWinApp->GetMagTable();
  ScaleMat mat;
  int iCam, iMag;
  double xtheta, ytheta, angle;
  bool needCam;
  str = CString("\r\nSpecimen to stage matrices  -   implied tilt axis angles");
  mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
  for (int actCam = 0; actCam < numCam; actCam++) {
    iCam = active[actCam];
    needCam = true;
    str.Format("%s:", camP[iCam].name);
    for (iMag = 1; iMag < MAX_MAGS; iMag++) {
      if (magTab[iMag].matStage[iCam].xpx != 0.) {
        if (needCam)
          mWinApp->AppendToLog(str);
        needCam = false;
        mat = MatMul(mShiftManager->FallbackSpecimenToStage(1., 1.), 
          magTab[iMag].matStage[iCam]);
        xtheta = atan2(mat.ypx, mat.xpx) / DTOR;
        ytheta = atan2(-mat.xpy, mat.ypy) / DTOR;
        angle = UtilGoodAngle(xtheta + 0.5 * UtilGoodAngle(ytheta - xtheta));
        mat = mShiftManager->MatMul(mShiftManager->SpecimenToCamera(iCam, iMag), 
          mShiftManager->MatInv(magTab[iMag].matStage[iCam]));
        str.Format("    %2d %7d   %9.5f  %9.5f  %9.5f  %9.5f   %7.2f deg", iMag, 
          MagForCamera(iCam, iMag), mat.xpx, mat.xpy, mat.ypx, mat.ypy, angle);
        mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
      }
    }
  }
  mat = mShiftManager->SpecimenToStage(1., 1.);
  str.Format("Average:   %9.5f  %9.5f  %9.5f  %9.5f", 
    mat.xpx, mat.xpy, mat.ypx, mat.ypy);
  mWinApp->AppendToLog(str, LOG_OPEN_IF_CLOSED);
}

void CMenuTargets::OnRemoveIScal()
{
  RemoveOneCal(false);
}

void CMenuTargets::OnRemoveStageCal()
{
  RemoveOneCal(true);
}

void CMenuTargets::RemoveOneCal(bool doStage)
{
  int camera = mWinApp->GetCurrentCamera();
  CameraParameters *camP = mWinApp->GetCamParams() + camera;
  MagTable *magTab;
  CString str;
  ScaleMat *mat;
  int *calibrated;
  str.Format("Magnification at which to remove %s shift calibration for camera %s",
    doStage ? "stage" : "image", (LPCTSTR)camP->name);
  int magInd = mScope->GetMagIndex();
  int mag = MagForCamera(camP, magInd);
  if (!KGetOneInt(str, mag))
    return;
  magInd = FindIndexForMagValue(mag, camera, magInd);
  if (magInd <= 0) {
    AfxMessageBox("That magnification value does not exist for this camera", MB_EXCLAME);
    return;
  }
  magTab = mWinApp->GetMagTable() + magInd;
  mat = B3DCHOICE(doStage, &magTab->matStage[camera], &magTab->matIS[camera]);
  calibrated = B3DCHOICE(doStage, &magTab->stageCalibrated[camera], 
    &magTab->calibrated[camera]);
  if (!mat->xpx) {
    AfxMessageBox("That magnification is not calibrated for this camera", MB_EXCLAME);
    return;
  }
  str.Format("The %s calibration for mag %d and camera %s is:\n  %.3f %.3f %3f %.3f\n\n"
    "Are you sure you want to remove this calibration?\n\n"
    "Doing so will remove it from this run of the program and from\n"
    "the short term calibration file, and will keep it from being\n"
    "saved in the main calibration file.  It will be removed from the\n"
    "main calibration file only if you save calibrations", doStage ? "stage" : "image",
    mag, (LPCTSTR)camP->name, mat->xpx, mat->xpy, mat->ypx, mat->ypy);
  if (AfxMessageBox(str, MB_QUESTION) != IDYES)
    return;

  // It gets saved if either of these is non-zero
  mat->xpx = mat->xpy = 0.;
  *calibrated = 0;
  mWinApp->mDocWnd->SetShortTermNotSaved();
  mWinApp->SetCalibrationsNotSaved(true);
}

void CMenuTargets::OnCalibrationSetIsDelayFactor()
{
  float factor = mShiftManager->GetISdelayScaleFactor();
  if (!KGetOneFloat("Overall scaling factor for delay after image shift:", factor, 2))
    return;
  mShiftManager->SetISdelayScaleFactor(factor);
}

// SCOPE

void CMenuTargets::OnTiltup() 
{
  mScope->TiltUp();   
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
}

void CMenuTargets::OnTiltdown() 
{
  mScope->TiltDown(); 
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, -1, 0, 0);
}

void CMenuTargets::OnUpdateTiltdown(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() 
    && mScope->StageBusy() <= 0); 
    
}

void CMenuTargets::OnCalibrationListmags() 
{
  mScope->ScanMagnifications();	
}

void CMenuTargets::OnUpdateCalibrationListmags(CCmdUI* pCmdUI) 
{ 
  pCmdUI->Enable(!DoingTasks() && !HitachiScope);
}

// CAMERA

// Menu or toolbar buttons to start a capture: go through common routine
void CMenuTargets::OnCameraView() 
{
  mWinApp->UserRequestedCapture(VIEW_CONSET);
}
void CMenuTargets::OnCameraFocus() 
{
  mWinApp->UserRequestedCapture(FOCUS_CONSET);
}
void CMenuTargets::OnCameraTrial() 
{
  mWinApp->UserRequestedCapture(TRIAL_CONSET);
}
void CMenuTargets::OnCameraRecord() 
{
  mWinApp->UserRequestedCapture(RECORD_CONSET);
}

void CMenuTargets::OnCameraPreview() 
{
  mWinApp->UserRequestedCapture(PREVIEW_CONSET);
}

void CMenuTargets::OnCameraSearch()
{
  mWinApp->UserRequestedCapture(SEARCH_CONSET);
}

void CMenuTargets::OnCameraUseViewForSearch()
{
  mWinApp->SetUseViewForSearch(!mWinApp->GetUseViewForSearch());
  mWinApp->mCameraMacroTools.Update();
}

void CMenuTargets::OnUpdateCameraUseViewForSearch(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mWinApp->GetUseViewForSearch() ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CMenuTargets::OnCameraUseRecordForMontage()
{
  mWinApp->SetUseRecordForMontage(!mWinApp->GetUseRecordForMontage());
}

void CMenuTargets::OnUpdateCameraUseRecordForMontage(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mWinApp->GetUseRecordForMontage() ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CMenuTargets::OnCameraSetcountlimit()
{
  int limit = mWinApp->mMontageController->GetCountLimit();
  if (!KGetOneInt("Mean counts for one piece below which to stop montage (0 for no "
    "limit):", limit))
    return;
  mWinApp->mMontageController->SetCountLimit(limit);
}

void CMenuTargets::OnMontageUseFilterSet2()
{
  mWinApp->mMontageController->SetUseFilterSet2(
    !mWinApp->mMontageController->GetUseFilterSet2());
  mWinApp->mMontageController->SetNeedBoxSetup(true);
}

void CMenuTargets::OnUpdateMontageUseFilterSet2(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mWinApp->mMontageController->GetUseFilterSet2());
}

void CMenuTargets::OnMontageUseSet2InLowDose()
{
  mWinApp->mMontageController->SetUseSet2InLD(
    !mWinApp->mMontageController->GetUseSet2InLD());
  mWinApp->mMontageController->SetNeedBoxSetup(true);
}

void CMenuTargets::OnUpdateMontageUseSet2InLowDose(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks() && !mWinApp->mMontageController->GetUseFilterSet2());
  pCmdUI->SetCheck(mWinApp->mMontageController->GetUseSet2InLD());
}

void CMenuTargets::OnMontageDivideSet1By2()
{
  mWinApp->mMontageController->SetDivFilterSet1By2(
    !mWinApp->mMontageController->GetDivFilterSet1By2());
  mWinApp->mMontageController->SetNeedBoxSetup(true);
}

void CMenuTargets::OnUpdateMontageDivideSet1By2(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mWinApp->mMontageController->GetDivFilterSet1By2());
}

void CMenuTargets::OnMontageDivideSet2By2()
{
  mWinApp->mMontageController->SetDivFilterSet2By2(
    !mWinApp->mMontageController->GetDivFilterSet2By2());
  mWinApp->mMontageController->SetNeedBoxSetup(true);
}

void CMenuTargets::OnUpdateMontageDivideSet2By2(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mWinApp->mMontageController->GetDivFilterSet2By2());
}

void CMenuTargets::OnMontageTestSet1Values()
{
  mWinApp->mMontageController->AdjustFilter(0);
}

void CMenuTargets::OnMontageTestSet2Values()
{
  mWinApp->mMontageController->AdjustFilter(1);
}

void CMenuTargets::OnMontageRedoCorrWhenRead()
{
  mWinApp->mMontageController->SetRedoCorrOnRead(
    !mWinApp->mMontageController->GetRedoCorrOnRead());
}

void CMenuTargets::OnUpdateMontageRedoCorrWhenRead(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks());
  pCmdUI->SetCheck(mWinApp->mMontageController->GetRedoCorrOnRead());
}

void CMenuTargets::OnCameraSetFrameAlignLines()
{
  int num = mCamera->GetNumFrameAliLogLines();
  if (!KGetOneInt("Enter number of lines of log output from frame alignment:",
    "(0 - no output; 1 = brief essentials; 2 = more detail, 3 = FRC also)", num))
    return;
  B3DCLAMP(num, 0, 3);
  mCamera->SetNumFrameAliLogLines(num);
}

// User shots are OK when calibrating IS offset
void CMenuTargets::OnUpdateNoUserShots(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(mWinApp->UserAcquireOK());
}

// Manage the Screen down mode and simulation mode, and post-action menu entries
void CMenuTargets::OnCameraScreendown() 
{
  mCamera->SetScreenDownMode(!mCamera->GetScreenDownMode());  
  mCamera->SetAMTblanking();
}

void CMenuTargets::OnUpdateCameraScreendown(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->StartedTiltSeries());
  pCmdUI->SetCheck(mCamera->GetScreenDownMode() ? 1 : 0);
}

// Process stop-acquire messages; enable if busy and not doing tasks
// Thus this will not stop other tasks
void CMenuTargets::OnCameraStopacquire() 
{
  mCamera->StopCapture(1);
}

void CMenuTargets::OnUpdateCameraStopacquire(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mCamera->CameraBusy() && !DoingTasks());
}

// Set timing parameters for finding properties
void CMenuTargets::OnCameraSettiming() 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  KGetOneFloat("Delay time for starting to take picture (seconds):", 
    param->startupDelay, 3);
  if (param->STEMcamera) {
    KGetOneFloat("Total flyback time (microseconds):", param->flyback, 0);
    return;
  }

  KGetOneFloat("Minimum drift settling (half of startup jitter, seconds):", 
    param->minimumDrift, 3);
  KGetOneFloat("Extra beam time before blanking after exposure (seconds):", 
    param->extraBeamTime, 3);
}

void CMenuTargets::OnUpdateCameraSettiming(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy() && 
    mWinApp->GetAdministrator());  
}

void CMenuTargets::OnCameraSetscanning() 
{
  float width = mCamera->GetBeamWidth();
  int margin = mCamera->GetScanMargin();
  int sleep = mCamera->GetScanSleep();
  KGetOneFloat("Width of slit beam in unbinned pixels (0 for no scanning):", 
    width, 1);
  mCamera->SetBeamWidth(width);
  if (width <= 0.)
    return;
  KGetOneInt("Width of extra margin on each side of camera frame in unbinned pixels:",
    margin);
  mCamera->SetScanMargin(margin);
  KGetOneInt("Milliseconds to sleep between beam movements:", sleep);
  mCamera->SetScanSleep(sleep);
}

void CMenuTargets::OnCameraDebugmode() 
{
  mCamera->SetDebugMode(!mCamera->GetDebugMode());
}

void CMenuTargets::OnUpdateCameraDebugmode(CCmdUI* pCmdUI) 
{
  BOOL bEnable = !DoingTasks() && !mCamera->CameraBusy();
  pCmdUI->Enable(bEnable);
  pCmdUI->SetCheck(mCamera->GetDebugMode() ? 1 : 0);
}

void CMenuTargets::OnCameraDivideby2() 
{
  mCamera->SetDivideBy2(mCamera->GetDivideBy2() > 0 ? 0 : 1);	
}

void CMenuTargets::OnUpdateCameraDivideby2(CCmdUI* pCmdUI) 
{
  BOOL bEnable = !DoingTasks() && !mCamera->CameraBusy() && 
    !mWinApp->StartedTiltSeries();
  pCmdUI->Enable(bEnable);
  pCmdUI->SetCheck(mCamera->GetDivideBy2() > 0 ? 1 : 0);
}

void CMenuTargets::OnCameraSetExtraDivisionBy2()
{
  int curVal = mCamera->GetExtraDivideBy2();
  if (!KGetOneInt("Extra division is on top of \"Divide 16-bit by 2\", which need not be "
    "on", "Enter number of added times to divide by 2 (for division by 2 to this "
    "power):", curVal))
    return;
  B3DCLAMP(curVal, 0, PLUGCAM_MOREDIV_MASK);
  mCamera->SetExtraDivideBy2(curVal);

  // Make it clear out dark references
  mCamera->SetDivideBy2(-1);
}

void CMenuTargets::OnUpdateCameraSetExtraDivisionBy2(CCmdUI *pCmdUI)
{
  CameraParameters *param = mWinApp->GetActiveCamParam();
  pCmdUI->Enable(!param->FEItype && (param->CamFlags & CAMFLAG_CAN_DIV_MORE) != 0);
  pCmdUI->SetCheck(mCamera->GetExtraDivideBy2() > 0);
}

void CMenuTargets::OnCameraReturnFloatImage()
{
  mCamera->SetAcquireFloatImages(!mCamera->GetAcquireFloatImages());
}

void CMenuTargets::OnUpdateCameraReturnFloatImage(CCmdUI *pCmdUI)
{
  CameraParameters *param = mWinApp->GetActiveCamParam();
  pCmdUI->SetCheck(mCamera->GetAcquireFloatImages());
  pCmdUI->Enable((param->CamFlags & CAMFLAG_FLOATS_BY_FLAG) != 0);
}

void CMenuTargets::OnCameraShowgainref()
{
  mCamera->ShowReference(GAIN_REFERENCE);
}

void CMenuTargets::OnCameraShowdarkref() 
{
  mCamera->ShowReference(DARK_REFERENCE);
}

void CMenuTargets::OnUpdateCameraShowref(CCmdUI* pCmdUI) 
{
  BOOL bEnable = mCamera->GetProcessHere() && !DoingTasks() && 
    !mCamera->CameraBusy();
  pCmdUI->Enable(bEnable);
}

void CMenuTargets::OnCameraNormalizehere() 
{
  mCamera->SetProcessHere(!mCamera->GetProcessHere());
}

void CMenuTargets::OnUpdateCameraNormalizehere(CCmdUI* pCmdUI) 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  pCmdUI->SetCheck(mCamera->GetProcessHere() ? 1 : 0);
  pCmdUI->Enable((!param->TietzType || 
    (param->TietzType && param->pluginCanProcess)) && (!param->AMTtype || 
    (param->AMTtype && mCamera->GetDMversion(2) >= AMT_VERSION_CAN_NORM)) && 
    (!param->DE_camType || param->DE_camType >= 2) && 
    mCamera->CanProcessHere(param) && (param->pluginName.IsEmpty() || 
     param->pluginCanProcess) && !DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnCameraInterpolateDarkRefs()
{
  mCamera->SetInterpDarkRefs(!mCamera->GetInterpDarkRefs());
}

void CMenuTargets::OnUpdateCameraInterpolateDarkRefs(CCmdUI *pCmdUI)
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  pCmdUI->SetCheck(mCamera->GetInterpDarkRefs() ? 1 : 0);
  pCmdUI->Enable(mCamera->GetProcessHere() && !DoingTasks() && 
    !mCamera->ReturningFloatImages(param) && !mCamera->CameraBusy() && 
    !(mWinApp->StartedTiltSeries() && mWinApp->mTSController->GetChangeRecExp()));
}

void CMenuTargets::OnCameraSetcorrections() 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  KGetOneInt("Enter sum of 1 for defect, 16 for bias, and 32 for",
    "linearization correction, or -1 for default corrections:", 
    param->corrections);
}

void CMenuTargets::OnUpdateCameraSetcorrections(CCmdUI* pCmdUI) 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  pCmdUI->Enable(mWinApp->GetAdministrator() && param->GatanCam && !DoingTasks() && 
    !mCamera->CameraBusy());
}

void CMenuTargets::OnCameraSetQuality()
{
  CameraParameters *param = mWinApp->GetActiveCamParam();
  KGetOneInt("Enter quality level to set in continuous acquire, or 0 not to set one:",
    param->continuousQuality);
  B3DCLAMP(param->continuousQuality, 0, 7);
}

void CMenuTargets::OnUpdateCameraSetQuality(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnCameraSetdriftinterval()
{
  int interval = mCamera->GetDriftISinterval();
  if (!KGetOneInt("Interval between image shift updates in msec:", interval))
    return;
  mCamera->SetDriftISinterval(B3DMAX(10, interval));
  float steps = mCamera->GetInitialDriftSteps();
  if (!KGetOneFloat("Number of shift steps to take immediately to get shift started:",
    steps, 1))
    return;
  mCamera->SetInitialDriftSteps(steps);
}

void CMenuTargets::OnCameraNoNormOfDSdoseFrac()
{
  mCamera->SetNoNormOfDSdoseFrac(!mCamera->GetNoNormOfDSdoseFrac());
}

void CMenuTargets::OnUpdateCameraNoNormOfDSdoseFrac(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mCamera->GetNoNormOfDSdoseFrac() ? 1 : 0);
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnCameraAlwaysAntialiasK23()
{
  mCamera->SetAntialiasBinning(mCamera->GetAntialiasBinning() > 0 ? -1 : 1);
}

void CMenuTargets::OnUpdateCameraAlwaysAntialiasK23(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mCamera->GetAntialiasBinning() > 0 ? 1 : 0);
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnCameraMontage() 
{
  mWinApp->RestoreViewFocus();
  mWinApp->StartMontageOrTrial(false); 
}

void CMenuTargets::OnCameraMontagetrial() 
{
  mWinApp->RestoreViewFocus();
  mWinApp->StartMontageOrTrial(true);
}

void CMenuTargets::OnUpdateCameraMontage(CCmdUI* pCmdUI) 
{
  BOOL bEnable = (!mWinApp->mStoreMRC || mWinApp->Montaging()) && !DoingTasks() 
    && !mWinApp->StartedTiltSeries() && !mCamera->CameraBusy() && 
    !mScope->GetMovingStage();
  pCmdUI->Enable(bEnable);
}

void CMenuTargets::OnUpdateCameraMontagetrial(CCmdUI* pCmdUI) 
{
  BOOL bEnable = (!mWinApp->mStoreMRC || mWinApp->Montaging()) && !DoingTasks()
    && !mCamera->CameraBusy() && !mScope->GetMovingStage();
  pCmdUI->Enable(bEnable);
}

void CMenuTargets::OnCameraPostactions() 
{
  mWinApp->SetActPostExposure(!mWinApp->ActPostExposure(NULL, true)); 
}

void CMenuTargets::OnUpdateCameraPostactions(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mCamera->PostActionsOK(NULL, true));
  pCmdUI->SetCheck(mWinApp->ActPostExposure(NULL, true) ? 1 : 0);
}

// Toggle camera with spacebar: either stop a continuous capture,
// or restart last parameter set
void CMenuTargets::OnCameratoggle() 
{
  if (DoingTasks() && (!mCamera->DoingContinuousAcquire() || 
    mCamera->GetPreventUserToggle()))
    return;
  if (mCamera->CameraBusy())
    mCamera->StopCapture(0);
  else
    mCamera->RestartCapture();
}

void CMenuTargets::OnpdateCameratoggle(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(!DoingTasks() || (mCamera->DoingContinuousAcquire() && 
    !mCamera->GetPreventUserToggle())); 
}

// Escape key is a general stop
void CMenuTargets::OnEscape() 
{
  mCamera->StopCapture(1);
  mWinApp->ErrorOccurred(0); 
}

void CMenuTargets::OnUpdateEscape(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(DoingTasks() || mCamera->CameraBusy());  
}

// The user enters the values for C2 in TUI
void CMenuTargets::OnCalibrationC2factor() 
{
  float intensity, C2Val, intLast, C2last, C2fac;
  int spotSize;
 	int spotSave = mScope->GetSpotSize();
  double intSave = mScope->GetIntensity();
  int probe = mScope->ReadProbeMode();

  if (AfxMessageBox("To calibrate the intensity to C2 factor,\nyou need to have "
    "a readout for the C2 lens in the Microscope User Interface.\n\n"
    "The program will go to each spot size and ask you to enter the C2 reading.\n",
    MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
    return;
   
  // Set intensity here just for first time
  mScope->SetIntensity(0.3);
  for (spotSize = 0; spotSize <= mScope->GetNumSpotSizes(); spotSize++) {
    if (mWinApp->mBeamAssessor->SetAndCheckSpotSize(spotSize ? spotSize : 1))
      return;
    intensity = (float)(100. * mScope->GetIntensity());
    C2Val = intensity;
    if (!KGetOneFloat("Enter the current value for the C2 lens in the microscope "
      "interface", C2Val, 3))
      break;

    // First time, save values and set intensity to 0.7
    if (!spotSize) {
      C2last = C2Val;
      intLast = intensity;
      mScope->SetIntensity(0.7);
    } else {
      // For first spot, compute factor; then offset for all of them
      if (spotSize < 2) {
        C2fac = (C2Val - C2last) / (intensity - intLast);
        mScope->SetC2IntensityFactor(probe, C2fac);
      }
      mScope->SetC2SpotOffset(spotSize, probe, (C2Val - C2fac * intensity) / 100.f);
      mWinApp->SetCalibrationsNotSaved(true);
    }
  }
  mScope->SetSpotSize(spotSave);
  mScope->SetIntensity(intSave);
}

void CMenuTargets::OnUpdateCalibrationC2factor(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(FEIscope && !mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && 
    !mScope->GetUseIllumAreaForC2());  
}

void CMenuTargets::OnCalibrationNeutralISvalues() 
{
  if (HitachiScope) {
    if (AfxMessageBox("This command will go to each magnification in LM and HR\n"
      "modes and record the currently stored image shift value.\n"
      "It should only be run if the mags are well-lined up when\n"
      "stepping through them in the Hitachi interface\nwith SerialEM not running.\n\n"
      "Are you sure you want to run this operation?", MB_QUESTION) == IDNO)
      return;
  }
  mScope->CalibrateNeutralIS(CAL_NTRL_FIND);
}

void CMenuTargets::OnUpdateCalibrationNeutralISvalues(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable((JEOLscope || HitachiScope) && !DoingTasks() && !mCamera->CameraBusy() && 
    !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnCalibrationRestoreNeutralIS() 
{
  mScope->CalibrateNeutralIS(CAL_NTRL_RESTORE);
}

void CMenuTargets::OnCalibrationBaseFocusValues() 
{
  mScope->CalibrateNeutralIS(CAL_NTRL_FOCUS);
}

void CMenuTargets::OnUpdateCalibrationRestoreNeutralIS(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(HitachiScope && !DoingTasks() && !mCamera->CameraBusy() && 
    !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnCalibrationBeamcrossover() 
{
  mWinApp->mBeamAssessor->CalibrateCrossover();
}

void CMenuTargets::OnCalibrationAlphaBeamShifts()
{
  mWinApp->mBeamAssessor->CalibrateAlphaBeamShifts();
}

void CMenuTargets::OnCalibrationSpotBeamShifts()
{
  mWinApp->mBeamAssessor->CalibrateSpotBeamShifts();
}

void CMenuTargets::OnUpdateCalibrationAlphaBeamShifts(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mScope->GetHasNoAlpha() && !DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnCalibrationHighDefocus()
{
  mWinApp->mShiftCalibrator->CalibrateHighDefocus();
}

void CMenuTargets::OnListFocusMagCals()
{
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals = 
    mWinApp->mShiftManager->GetFocusMagCals();
  HighFocusMagCal cal, calj;
  float tol = 5.;
  int ind, jnd, tmp, numCal = (int)focusMagCals->GetSize();
  if (!numCal)
    return;
  int *index = new int[numCal];
  PrintfToLog("Focus  spot    %s-%s      scale    rotation", mScope->GetC2Name(), 
    mScope->GetC2Units());
  for (ind = 0; ind < numCal; ind++)
    index[ind] = ind;
  for (ind = 0; ind < numCal - 1; ind++) {
    for (jnd = ind + 1; jnd < numCal; jnd++) {
      cal = focusMagCals->GetAt(index[ind]);
      calj = focusMagCals->GetAt(index[jnd]);
      if (calj.defocus > cal.defocus + tol || fabs((double)calj.defocus - cal.defocus) <
        tol && calj.intensity < cal.intensity) {
          tmp = index[ind];
          index[ind] = index[jnd];
          index[jnd] = tmp;
      }
    }
  }

  for (ind = 0; ind < numCal; ind++) {
    cal = focusMagCals->GetAt(index[ind]);
    PrintfToLog("%.0f      %d       %7.3f       %.3f       %.2f   %s", cal.defocus, 
      cal.spot, mScope->GetC2Percent(cal.spot, cal.intensity, cal.probeMode), cal.scale, 
      cal.rotation, cal.probeMode == 0 ? "nanoprobe": "");
  }
  delete [] index;
}

// TILT SERIES MENU

void CMenuTargets::OnTasksTiltseries()
{
  mTSController->SetupTiltSeries();
}

void CMenuTargets::OnUpdateTasksTiltseries(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !(mNavigator && mNavigator->StartedMacro()));
}

void CMenuTargets::OnTiltseriesExtraoutput() 
{
  mTSController->SetExtraOutput();  	
}

void CMenuTargets::OnTiltseriesResetpoleangle()
{
  mTSController->ResetPoleAngle();
}

void CMenuTargets::OnTiltseriesFuturepolicies()
{
  CTSPolicyDlg dlg;
  if (dlg.DoModal() != IDOK)
    return;
}

void CMenuTargets::OnTiltseriesRunMacro()
{
  mTSController->SetRunMacroInTS(!mTSController->GetRunMacroInTS());
}

void CMenuTargets::OnUpdateTiltseriesRunMacro(CCmdUI *pCmdUI)
{
  int macNum = mTSController->GetMacroToRun();
  pCmdUI->SetCheck(mTSController->GetRunMacroInTS());
  pCmdUI->Enable(!mWinApp->DoingTasks() && macNum > 0 && macNum <= MAX_MACROS);
}

void CMenuTargets::OnTiltseriesSetMacroToRun()
{
  CMacroSelector dlg;

  // The series macro is numbered from 1, 0 means not do one
  // The dialog is 0 based
  int macNum = mTSController->GetMacroToRun();
  dlg.mMacroIndex = B3DMAX(macNum - 1, 0);
  dlg.m_strEntryText = "Select script to run before step in tilt series";
  if (dlg.DoModal() == IDCANCEL)
    return;
  macNum = dlg.mMacroIndex + 1;
  mTSController->SetMacroToRun(macNum);
  macNum = mTSController->GetStepAfterMacro();
  B3DCLAMP(macNum, 1, TSMACRO_PRE_RECORD);
  if (!KGetOneInt("steps are: 1 for track before focus, 2 for focusing, 3 for track after"
    ", 4 for Record", "Enter number of step that the script will be run before:", macNum))
    return;
  B3DCLAMP(macNum, 1, TSMACRO_PRE_RECORD);
  mTSController->SetStepAfterMacro(macNum);  
}

void CMenuTargets::OnTiltseriesCallTsPlugin()
{
  mTSController->SetCallTSplugins(!mTSController->GetCallTSplugins());
}

void CMenuTargets::OnUpdateTiltseriesCallTsPlugin(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->mPluginManager->GetAnyTSplugins());
  pCmdUI->SetCheck(mTSController->GetCallTSplugins());
}

void CMenuTargets::OnTiltseriesSendemail()
{
  mTSController->SetSendEmail(mTSController->GetSendEmail() ? 0 : 1);
}

void CMenuTargets::OnUpdateTiltseriesSendemail(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mWinApp->mMailer->GetInitialized() && 
    !(mWinApp->mMailer->GetSendTo()).IsEmpty());
  pCmdUI->SetCheck(mTSController->GetSendEmail() != 0);
}

void CMenuTargets::OnTiltseriesSetaddress()
{
  CString str = mWinApp->mMailer->GetSendTo();
  if (KGetOneString("Email address for notifications (one or more, separated by comma"
    " or space):", str,250))
    mWinApp->mMailer->SetSendTo(str);
}

void CMenuTargets::OnTiltseriesFixedNumFrames()
{
  mTSController->SetFixedNumFrames(!mTSController->GetFixedNumFrames());
}

void CMenuTargets::OnUpdateTiltseriesFixedNumFrames(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mTSController->GetFixedNumFrames());
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CMenuTargets::OnTiltseriesBackup() 
{
  mTSController->BackUpSeries(); 
}

void CMenuTargets::OnUpdateTiltseriesBackup(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mTSController->CanBackUp());  
}

void CMenuTargets::OnTiltseriesStop() 
{
  mWinApp->mCameraMacroTools.SetUserStop(TRUE);
  mTSController->ExternalStop(0);
  mWinApp->mCameraMacroTools.SetUserStop(FALSE);
}

void CMenuTargets::OnUpdateTiltseriesStop(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mTSController->DoingTiltSeries()); 
}

void CMenuTargets::OnTiltseriesEndloop() 
{
  mTSController->EndLoop();  
}

void CMenuTargets::OnTiltseriesResume() 
{
  mTSController->Resume();
}

void CMenuTargets::OnUpdateTiltseriesResume(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mTSController->IsResumable());  
}

void CMenuTargets::OnTiltseriesOneloop() 
{
  mTSController->CommonResume(1, 0);
}

void CMenuTargets::OnTiltseriesOnestep() 
{
  mTSController->CommonResume(-1, 0);
}

void CMenuTargets::OnTiltseriesTerminate() 
{
  mTSController->Terminate();  
}

void CMenuTargets::OnTiltseriesVerbose() 
{
  mTSController->SetVerbose(!mTSController->GetVerbose());
}

void CMenuTargets::OnTiltseriesSaveXYZFile()
{
  mTSController->WriteTiltXYZFile(NULL);
}

void CMenuTargets::OnUpdateTiltseriesSaveXYZFile(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mTSController->GetTiltIndex() > 0);
}

void CMenuTargets::OnTiltseriesAutosaveXYZ()
{
  mTSController->SetAutosaveXYZ(!mTSController->GetAutosaveXYZ());
}

void CMenuTargets::OnUpdateTiltseriesAutosaveXYZ(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mTSController->GetAutosaveXYZ() ? 1 : 0);
}

void CMenuTargets::OnUpdateTiltseriesVerbose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mTSController->GetVerbose() ? 1 : 0);
}

void CMenuTargets::OnTiltseriesAutosavelog() 
{
  mTSController->SetAutosaveLog(!mTSController->GetAutosaveLog());
}

void CMenuTargets::OnUpdateTiltseriesAutosavelog(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mTSController->GetAutosaveLog() ? 1 : 0);
}

void CMenuTargets::OnTiltseriesDebugmode() 
{
  mTSController->SetDebugMode(!mTSController->GetDebugMode());
  if (mTSController->GetDebugMode())
    mTSController->DebugDump("Turned on debug mode");
}

void CMenuTargets::OnUpdateTiltseriesDebugmode(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mTSController->GetDebugMode() ? 1 : 0);
}

void CMenuTargets::OnCalibrationCameratiming() 
{
  mWinApp->mCalibTiming->CalibrateTiming(-1, -1., true);	
}

void CMenuTargets::OnCalibrationQuickFlybackTime()
{
  int i;
  CString *modeNames = mWinApp->GetModeNames();
  CString message = "Parameter set to measure flyback time for (";
  for (i = 0; i < 5; i++) {
    if (i)
      message += ", ";
    message += modeNames[i].Left(1);
  }
  if (!KGetOneString(message + "):", mFlybackSet))
    return;
  if (!mFlybackSet.IsEmpty())
    mFlybackSet = mFlybackSet.Left(1).MakeUpper();
  for (i = 0; i < 5; i++) {
    if (mFlybackSet == modeNames[i].Left(1).MakeUpper()) {
      mWinApp->mCalibTiming->CalibrateTiming(i, -1., true);	
      return;
    }
  }
  AfxMessageBox("You did not enter the first letter of a parameter set name", MB_EXCLAME);
}

void CMenuTargets::OnUpdateCalibrationQuickFlybackTime(CCmdUI *pCmdUI)
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  pCmdUI->Enable(param->STEMcamera && param->FEItype && !mWinApp->DoingTasks());
}

void CMenuTargets::OnCalibrationShutterdeadtime() 
{
  mWinApp->mCalibTiming->CalibrateDeadTime(); 	
}

void CMenuTargets::OnUpdateCalibrationShutterdeadtime(CCmdUI *pCmdUI)
{
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  pCmdUI->SetCheck(!mCamera->IsDirectDetector(camParam));
}

void CMenuTargets::OnCalibrationSetdebugoutput() 
{
  CString str = mWinApp->GetDebugKeys();
  if (KGetOneString("Key letters for debugging output (0 for none):", str))
    mWinApp->SetDebugOutput(str);
}

void CMenuTargets::OnCalibrationStandardLMfocus()
{
  DoCalibrateStandardLMfocus(false);
}

int CMenuTargets::DoCalibrateStandardLMfocus(bool fromMacro)
{
  int magInd = mScope->GetMagIndex();
  double *focTab = mScope->GetLMFocusTable();
  CString message;
  int probe = mScope->ReadProbeMode();
  if (mWinApp->GetSTEMMode() || magInd < 1) {
    if (!fromMacro)
      AfxMessageBox("Standard focus is not used in STEM mode\n"
        "or diffraction mode", MB_EXCLAME);
    return 1;
  }
  focTab[2 * magInd + probe] = mScope->GetFocus();
  message.Format("Eucentric focus value of %.5f saved for mag %dx%s", 
    focTab[2 * magInd + probe], MagForCamera(mWinApp->GetCurrentCamera(), magInd), 
    probe ? "" : " nanoprobe");
  mWinApp->AppendToLog(message);
  mWinApp->SetCalibrationsNotSaved(true);
  return 0;
}

void CMenuTargets::OnCameraGainrefpolicy()
{
  int *activeList = mWinApp->GetActiveCameraList();
  int i, cam;
  CameraParameters *camP = mWinApp->GetCamParams();
  CRefPolicyDlg dlg;
  dlg.m_bIgnoreHigh = mWinApp->mGainRefMaker->GetIgnoreHigherRefs();
  dlg.m_bPreferBin2 = mWinApp->mGainRefMaker->GetUseOlderBinned2();
  dlg.m_iAskPolicy = mWinApp->mGainRefMaker->GetDMrefAskPolicy();
  for (i= 0; i < mWinApp->GetActiveCamListSize(); i++) {
    cam = activeList[i];
    if (camP[cam].GatanCam)
      dlg.mAnyGatan = true;
    if (camP[cam].numExtraGainRefs)
      dlg.mAnyHigher = true;
  }
  if (dlg.DoModal() != IDOK)
    return;
  mWinApp->mGainRefMaker->SetIgnoreHigherRefs(dlg.m_bIgnoreHigh);
  mWinApp->mGainRefMaker->SetUseOlderBinned2(dlg.m_bPreferBin2);
  mWinApp->mGainRefMaker->SetDMrefAskPolicy(dlg.m_iAskPolicy);

  // Do it in same order as when KV changes
  mWinApp->mGainRefMaker->DeleteReferences();
  mWinApp->mGainRefMaker->InitializeRefArrays();
  mCamera->DeleteReferences(GAIN_REFERENCE, false);
}

void CMenuTargets::OnUpdateCameraGainrefpolicy(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingTasks() && !mCamera->CameraBusy());
}

void CMenuTargets::OnTasksSetupcooker()
{
  BOOL start;
  mWinApp->mCookerDlg = new CCookerSetupDlg;
  mWinApp->mCookerDlg->DoModal();
  start = mWinApp->mCookerDlg->m_bGoCook;
  delete mWinApp->mCookerDlg;
  mWinApp->mCookerDlg = NULL;
  if (start)
    mWinApp->mMultiTSTasks->StartCooker();
}

void CMenuTargets::OnTasksCookspecimen()
{
  mWinApp->mMultiTSTasks->StartCooker();
}

void CMenuTargets::OnTasksSetupAutocenter()
{
  mWinApp->mMultiTSTasks->SetupAutocenter();
}

void CMenuTargets::OnUpdateTasksSetupAutocenter(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->mAutocenDlg && !mWinApp->DoingTasks() && 
    !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnTasksAutocenterBeam()
{
  mWinApp->mMultiTSTasks->AutocenterBeam();
}

void CMenuTargets::OnUpdateTasksAutocenterbeam(CCmdUI *pCmdUI)
{
  int bestm, bests, spot, mag, probe;
  double roughInt;
  bool synth;
  AutocenParams *param;
  if (mWinApp->GetSTEMMode()) {
    pCmdUI->Enable(false);
    return;
  }
  if (mWinApp->LowDoseMode()) {
    LowDoseParams *ldParm = mWinApp->GetLowDoseParams() + TRIAL_CONSET;
    mag = ldParm->magIndex;
    spot = ldParm->spotSize;
    probe = ldParm->probeMode;
    roughInt = ldParm->intensity;
  } else {
    spot = mScope->FastSpotSize();
    mag = mScope->FastMagIndex();
    roughInt = mScope->FastIntensity();
    probe = mScope->GetProbeMode();
  }
  param = mWinApp->mMultiTSTasks->GetAutocenSettings(mWinApp->GetCurrentCamera(), mag,
    spot, probe, roughInt, synth, bestm, bests);
  pCmdUI->Enable(!mWinApp->DoingTasks() && param->intensity >= 0. && 
    !mScope->GetMovingStage());
}

void CMenuTargets::OnTasksAssessAngleRange()
{
  mWinApp->mMultiTSTasks->RangeFinder();
}

void CMenuTargets::OnTasksAssessIntersetShifts()
{
  mWinApp->mShiftCalibrator->MeasureInterSetShift(0);
}

void CMenuTargets::OnUpdateTasksAssessIntersetShifts(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->GetSTEMMode()); 
}

void CMenuTargets::OnTasksReviseCancel()
{
  mWinApp->mShiftCalibrator->ReviseOrCancelInterSetShifts();
}

void CMenuTargets::OnUpdateTasksReviseCancel(CCmdUI *pCmdUI)
{
  STEMinterSetShifts *isShifts = mShiftManager->GetSTEMinterSetShifts();
  bool anyset = false;
  for (int i = 0; i < 5; i++)
    if (isShifts->binning[i] && (isShifts->shiftX[i] || isShifts->shiftY[i]))
      anyset = true;
  pCmdUI->Enable(anyset || (mWinApp->GetImBufs())->mTimeStamp == 
    mWinApp->mShiftCalibrator->GetISStimeStampLast());
}

void CMenuTargets::OnTasksSetupWaitForDrift()
{
  CDriftWaitSetupDlg dlg;
  if (mWinApp->StartedTiltSeries())
    dlg.mTSparam = mWinApp->mTSController->GetTiltSeriesParam();
  dlg.DoModal();
}

void CMenuTargets::OnTasksSetupVppConditioning()
{
  mWinApp->mMultiTSTasks->SetupVPPConditioning();
}

void CMenuTargets::OnUpdateTasksSetupVppConditioning(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->mVPPConditionSetup && !mWinApp->DoingTasks());
}

void CMenuTargets::OnEucentricityByFocus()
{
  mWinApp->mParticleTasks->EucentricityFromFocus(
    mWinApp->mParticleTasks->GetZbyGUseViewInLD());
}

void CMenuTargets::OnUpdateEucentricityByFocus(CCmdUI *pCmdUI)
{
  int index, area, paramInd, nearest, error;
  pCmdUI->Enable(mWinApp->mParticleTasks->GetZbyGCalAndCheck(-1, index, area, paramInd,
    nearest, error) != NULL);
}

void CMenuTargets::OnSetupEucentricityByFocus()
{
  mWinApp->mParticleTasks->OpenZbyGDialog();
}

void CMenuTargets::OnSetupScopeManagement()
{
  CManageDewarsDlg dlg;
  dlg.DoModal();
}

void CMenuTargets::OnUpdateSetupScopeManagement(CCmdUI * pCmdUI)
{
  pCmdUI->Enable(FEIscope || mScope->GetHasSimpleOrigin() ||
    (JEOLscope && mScope->GetJeolHasNitrogenClass()));
}

void CMenuTargets::OnSpecialSkipBeamShiftInTs()
{
  mTSController->SetSkipBeamShiftOnAlign(!mTSController->GetSkipBeamShiftOnAlign());
}

void CMenuTargets::OnUpdateSpecialSkipBeamShiftInTs(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mTSController->GetSkipBeamShiftOnAlign());
  pCmdUI->Enable(!FEIscope && !mWinApp->DoingTasks());
}

void CMenuTargets::OnSpecialPiezoForLdAxisShift()
{
  mScope->SetUsePiezoForLDaxis(!mScope->GetUsePiezoForLDaxis());
}

void CMenuTargets::OnUpdateSpecialPiezoForLdAxisShift(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mScope->GetUsePiezoForLDaxis());
  pCmdUI->Enable(mWinApp->mLowDoseDlg.GetAxisPiezoPlugNum() >= 0 && 
    mWinApp->mLowDoseDlg.GetAxisPiezoNum() &&
    !mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() && !mWinApp->LowDoseMode());
}

void CMenuTargets::OnAllowContinuousInTS()
{
  mTSController->SetAllowContinuous(!mTSController->GetAllowContinuous());
}

void CMenuTargets::OnUpdateAllowContinuousInTS(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mTSController->GetAllowContinuous() ? 1 : 0);
}

void CMenuTargets::OnSpecialDisableDarkTrimInAlign()
{
  int disable = mWinApp->mShiftManager->GetDisableAutoTrim();
  if (!SetTwoFlags("Enter 0 for automatic trimming of dark borders in Autoalign in Low "
    "Dose","   or 1 to disable it in tilt series, or 2 to disable it always", 
    NOTRIM_LOWDOSE_TS, NOTRIM_LOWDOSE_ALL, disable))
    return;
  if (SetTwoFlags("Enter 0 for automatic trimming of dark borders in lower mag tasks",
    "   or 1 to disable it in tilt series, or 2 to disable it always", 
    NOTRIM_TASKS_TS, NOTRIM_TASKS_ALL, disable)) {
      SetTwoFlags("Enter 0 for automatic trimming in first round of Realign to Item",
        "   or 1 to disable it", NOTRIM_REALIGN_ITEM, 0, disable);
  }
  mWinApp->mShiftManager->SetDisableAutoTrim(disable);
}

// Manage two flags in the "disable" variable after asking for an entry of 1 or 2
int CMenuTargets::SetTwoFlags(CString mess1, CString mess2, int flag1, int flag2, 
                              int &disable)
{
  int value = (disable & flag1) ? 1 : 0;
  if (disable & flag2)
    value = 2;
  if (!KGetOneInt(mess1, mess2, value))
    return 0;
  disable &= ~(flag1 | flag2);
  if (value == 1 || (!flag2 && value > 0))
    disable |= flag1;
  else if (value > 1)
    disable |= flag2;
  return 1;
}

void CMenuTargets::OnAdjustFocusOnProbeModeChange()
{
  BOOL turningOff = mScope->GetAdjustFocusForProbe();
  mScope->SetAdjustFocusForProbe(!turningOff);
  if (turningOff)
    mScope->SetFirstFocusForProbe(EXTRA_NO_VALUE);
}

void CMenuTargets::OnUpdateAdjustFocusOnProbeModeChange(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mScope->GetAdjustFocusForProbe() ? 1 : 0);
}

void CMenuTargets::OnNormalizeAllLensesOnMag()
{
  int value = mScope->GetNormAllOnMagChange();
  if (KGetOneInt("This entry controls whether all lenses are normalized on a mag change",
    "Enter 0 to do so only when going between LM and nonLM, 1 to do so when staying in "
    "LM, or 2 to do so always", value))
    mScope->SetNormAllOnMagChange(value);
}

void CMenuTargets::OnSkipBlankingInLdWithScreenUp()
{
  mScope->SetSkipBlankingInLowDose(!mScope->GetSkipBlankingInLowDose());
}

void CMenuTargets::OnUpdateSkipBlankingInLdWithScreenUp(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
  pCmdUI->SetCheck(mScope->GetSkipBlankingInLowDose() ? 1 : 0);
}

void CMenuTargets::OnCloseValvesAfterLongInactivity()
{
  CString mess;
  int time = mScope->GetIdleTimeToCloseValves();
  if (mScope->GetNoColumnValve())
    mess = "filament should be turned off:";
  else
    mess.Format("the %s should be closed:", JEOLscope ? " gun valve" : "column valves");
  if (!KGetOneInt("Enter 0 to disable this feature",
    "Number of minutes of inactivity after which " + mess, time))
    return;
  mScope->SetIdleTimeToCloseValves(time);
}

void CMenuTargets::OnUpdateCloseValvesAfterLongInactivity(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mScope->GetIdleTimeToCloseValves() > 0 ? 1 : 0);
  pCmdUI->Enable(!mWinApp->DoingTasks() && !HitachiScope && !mScope->GetNoScope());
}

void CMenuTargets::OnWindowStageMoveTool()
{
  mWinApp->OpenStageMoveTool();
}

void CMenuTargets::OnWindowTakeScreenShot()
{
  mWinApp->OpenScreenShotDlg();
}

// Astigmatism and coma-free entries
void CMenuTargets::OnFocusCorrectAstigmatism()
{
  mWinApp->mAutoTuning->FixAstigmatism(true);
}


void CMenuTargets::OnUpdateFocusCorrectAstigmatism(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && 
    mWinApp->mAutoTuning->LookupAstigCal(mScope->GetProbeMode(), mScope->FastAlpha(),
    mScope->FastMagIndex()) >= 0
     && CamToSpecimenExists());
}


void CMenuTargets::OnFocusComaFree()
{
  mWinApp->mAutoTuning->ComaFreeAlignment(false, COMA_INITIAL_ITERS);
}


void CMenuTargets::OnUpdateFocusComaFree(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && 
    mWinApp->mAutoTuning->LookupComaCal(mScope->GetProbeMode(), mScope->FastAlpha(),
    mScope->FastMagIndex()) >= 0 && CamToSpecimenExists() && !mWinApp->LowDoseMode());
}

void CMenuTargets::OnFocusSetComaIterations()
{
  int iters = mWinApp->mAutoTuning->GetInitialComaIters();
  if (!KGetOneInt("Number of iterations in initial run of coma-free alignment:", iters))
    return;
  B3DCLAMP(iters, 1, 10);
  mWinApp->mAutoTuning->SetInitialComaIters(iters);
}


void CMenuTargets::OnFocusRunAnotherIteration()
{
  mWinApp->mAutoTuning->ComaFreeAlignment(false, COMA_ADD_ONE_ITER);
}


void CMenuTargets::OnUpdateFocusRunAnotherIteration(CCmdUI *pCmdUI)
{
  int magInd = mScope->FastMagIndex();
  int alpha = mScope->FastAlpha();
  int probe = mScope->GetProbeMode();
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->GetSTEMMode() && 
    mWinApp->mAutoTuning->LookupComaCal(probe, alpha, magInd) >= 0 && 
    CamToSpecimenExists() && !mWinApp->LowDoseMode() &&
    mWinApp->mAutoTuning->GetNumComaItersDone() > 0 && 
    magInd == mWinApp->mAutoTuning->GetLastComaMagInd() &&
    probe == mWinApp->mAutoTuning->GetLastComaProbe() &&
    alpha == mWinApp->mAutoTuning->GetLastComaAlpha());
}


void CMenuTargets::OnFocusMakeZemlinTableau()
{
  CString mess;
  float tilt = mWinApp->mAutoTuning->GetMenuZemlinTilt();
  mess = "Enter beam tilt for Zemlin tableau images ";
  if (FEIscope)
    mess += "in milliradians";
  else
    mess += "as percent of full range";
  if (!KGetOneFloat("Images are taken with current Focus parameters", mess, tilt, 1))
    return;
  mWinApp->mAutoTuning->SetMenuZemlinTilt(tilt);
  mWinApp->mAutoTuning->MakeZemlinTableau(tilt, 340, 170);
}

void CMenuTargets::OnUpdateCalibrateAstigmatism(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->mFocusManager->FocusReady() &&
    !mWinApp->GetSTEMMode() && CamToSpecimenExists());
}

void CMenuTargets::OnCalibrateComaFree()
{
  mWinApp->mAutoTuning->ComaFreeAlignment(true, COMA_JUST_MEASURE);
}

void CMenuTargets::OnUpdateCalibrateComaFree(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->mFocusManager->FocusReady() &&
    !mWinApp->GetSTEMMode() && CamToSpecimenExists() && !mWinApp->LowDoseMode());
}

void CMenuTargets::OnCalibrateAstigmatism()
{
  mWinApp->mAutoTuning->CalibrateAstigmatism();
}

bool CMenuTargets::CamToSpecimenExists(void)
{
  ScaleMat amat = mWinApp->mShiftManager->CameraToSpecimen(mScope->FastMagIndex());
  return amat.xpx != 0.;
}

void CMenuTargets::OnCalibrateSetupComaFree()
{
  float value = mWinApp->mAutoTuning->GetCalComaFocus();
  if (!KGetOneFloat("Defocus to use for calibrating coma-free alignment:", value, 1))
    return;
  mWinApp->mAutoTuning->SetCalComaFocus(value);
}

void CMenuTargets::OnCalibrateSetupAstigmatism()
{
  float value = mWinApp->mAutoTuning->GetAstigToApply();
  if (!KGetOneFloat("Stigmator value to apply when calibrating astigmatism:", value, 2))
    return;
  mWinApp->mAutoTuning->SetAstigToApply(value);
  value = mWinApp->mAutoTuning->GetAstigCenterFocus();
  if (!KGetOneFloat("This entry applies only when using beam-tilt-induced displacements "
    "(BTID)", "Defocus in microns at which to calibrate astigmatism by BTID:", value, 1))
    return;
  mWinApp->mAutoTuning->SetAstigCenterFocus(value);
  value = mWinApp->mAutoTuning->GetAstigFocusRange();
  if (!KGetOneFloat("This entry applies only when using beam-tilt-induced displacements "
    "(BTID)", "Defocus range to use for focus component of astigmatism calibration by"
    " BTID:", value, 1))
    return;
  mWinApp->mAutoTuning->SetAstigFocusRange(value);
}

void CMenuTargets::OnFocusSetAstigBeamTilt()
{
  float value = mWinApp->mAutoTuning->GetAstigBeamTilt();
  if (!FEIscope) {
    CString str;
    float scaling = mWinApp->mFocusManager->EstimatedBeamTiltScaling();
    str.Format("The scaling from %% beam tilt to milliradians is about %.2f", scaling);
    if (!KGetOneFloat(str, "Beam tilt (as % of full range) for calibrating or measuring "
      "astigmatism:", value, 2))
      return;
  } else {
    if (!KGetOneFloat("Beam tilt in milliradians for calibrating or measuring "
      "astigmatism:", value, 2))
      return;
  }
  mWinApp->mAutoTuning->SetAstigBeamTilt(value);
  mWinApp->mAutoTuning->SetUsersAstigTilt(value);
}

void CMenuTargets::OnFocusSetComaBeamTilt()
{
  CArray <ComaCalib, ComaCalib> *comaCal = mWinApp->mAutoTuning->GetComaCals();
  ComaCalib coma;
  CString str, str2;
  std::set<float> tilts;
  std::set<float>::iterator iter;
  int ind, alpha, probe;
  float value = mWinApp->mAutoTuning->GetMaxComaBeamTilt();
  alpha = mScope->GetAlpha();
  probe = mScope->ReadProbeMode();
  if (comaCal->GetSize()) {
    for (ind = 0; ind < comaCal->GetSize(); ind++) {
      coma = comaCal->GetAt(ind);
      if (coma.alpha == alpha && coma.probeMode == probe)
        tilts.insert(coma.beamTilt);
    }
    str = "The alignment must be run at the same beam tilt as a calibration; calibrations"
      " at current beam conditions are at ";
    for (iter = tilts.begin(); iter != tilts.end(); iter++) {
      str2.Format("%.2f", *iter);
      if (iter != tilts.begin())
        str += ", ";
      str += str2;
    }
  } else {
    if (FEIscope) {
      str = "There are no coma-free alignment calibrations at current beam conditions";
    } else {
      float scaling = mWinApp->mFocusManager->EstimatedBeamTiltScaling();
      str.Format("The scaling from %% beam tilt to milliradians is about %.2f", scaling);
    }
  }
  if (!KGetOneFloat(str, "Maximum beam tilt of the coma-free alignment calibration:",
    value, 2))
    return;
  mWinApp->mAutoTuning->SetMaxComaBeamTilt(value);
  mWinApp->mAutoTuning->SetUsersComaTilt(value);
}

void CMenuTargets::OnFocusSetCtfComaBt()
{
  float value = mWinApp->mAutoTuning->GetUsersComaTilt();
  if (value <= 0)
    value = mWinApp->mAutoTuning->GetMaxComaBeamTilt();
  if (!FEIscope) {
    CString str;
    float scaling = mWinApp->mFocusManager->EstimatedBeamTiltScaling();
    str.Format("The scaling from %% beam tilt to milliradians is about %.2f", scaling);
    if (!KGetOneFloat(str, "Beam tilt (as % of full range) for CTF-based coma-free "
      "alignment:", value, 2))
      return;
  } else {
    if (!KGetOneFloat("Beam tilt in milliradians for CTF-based coma-free "
      "alignment:", value, 2))
      return;
  }
  mWinApp->mAutoTuning->SetUsersComaTilt(value);
  int full = mWinApp->mAutoTuning->GetCtfDoFullArray() ? 1 : 0;
  if (!KGetOneInt("1 to do full 3x3 array of images for CTF-based coma-free "
    "alignment, 0 not to:", full))
    return;

  mWinApp->mAutoTuning->SetCtfDoFullArray(full != 0);
  value = mWinApp->mAutoTuning->GetComaIterationThresh();
  if (KGetOneFloat("Threshold in milliradians for iterating coma-free alignment:",
    value, 1))
    mWinApp->mAutoTuning->SetComaIterationThresh(value);
}

void CMenuTargets::OnCalFocusTuningCtfAstig()
{
  if (mWinApp->mAutoTuning->CheckAndSetupCtfAcquireParams("calibrating astigmatism", 
    false))
    return;
  mWinApp->mAutoTuning->CtfBasedAstigmatismComa(0, true, 0, false, false);
}

void CMenuTargets::OnFocusComaByCtf()
{
  if (mWinApp->mAutoTuning->CheckAndSetupCtfAcquireParams("correcting coma", 
    false))
    return;
  mWinApp->mAutoTuning->CtfBasedAstigmatismComa
    (mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1, false, 0, false, false);
}

void CMenuTargets::OnFocusSetCtfAcquireParams()
{
  mWinApp->mAutoTuning->SetupCtfAcquireParams(false);
}

void CMenuTargets::OnFocusCorrectAstigmatismWithFfts()
{
  if (mWinApp->mAutoTuning->CheckAndSetupCtfAcquireParams("correcting astigmatism", 
    false))
    return;
  mWinApp->mAutoTuning->CtfBasedAstigmatismComa(0, false, 0, false, false);
}

void CMenuTargets::OnUpdateFocusCorrectAstigmatismWithFfts(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mWinApp->mAutoTuning->LookupCtfBasedCal(false, mScope->GetMagIndex(), 
    false) >= 0 && !mWinApp->DoingTasks() && !mWinApp->GetSTEMMode());
}

void CMenuTargets::OnCalibrateComaVsIS()
{
  mWinApp->mAutoTuning->CalibrateComaVsImageShift(true);
}

void CMenuTargets::OnSettingsSetProperty()
{
  mWinApp->mParamIO->UserSetProperty();
}

void CMenuTargets::OnExternalTool(UINT nID)
{
  mWinApp->mExternalTools->RunToolCommand(nID - ID_EXTERNAL_TOOL1);
}

void CMenuTargets::OnBeamSpotListCalibrations()
{
  mShiftManager->ListBeamShiftCals();
  mWinApp->mBeamAssessor->ListIntensityCalibrations();
  mWinApp->mBeamAssessor->ListSpotCalibrations();
}


void CMenuTargets::OnBeamspotIlluminatedAreaLimits()
{
  mWinApp->mBeamAssessor->CalibrateIllumAreaLimits();
}


void CMenuTargets::OnUpdateIlluminatedAreaLimits(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mScope->GetUseIllumAreaForC2() && !mWinApp->DoingTasks());
}


void CMenuTargets::OnMarkerToCenter()
{
  mShiftManager->AlignmentShiftToMarker(false);
}
