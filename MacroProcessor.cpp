// MacroProcessor.cpp:    Runs macros
//
// Copyright (C) 2003-2018 by the Regents of the University of
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
#include "MacroEditer.h"
#include "MacroToolbar.h"
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
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "MacroControlDlg.h"
#include "MultiShotDlg.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "StateDlg.h"
#include "FalconHelper.h"
#include "OneLineScript.h"
#include "ExternalTools.h"
#include "MenuTargets.h"
#include "Mailer.h"
#include "PluginManager.h"
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

#define ABORT_NOLINE(a) \
{ \
  mWinApp->AppendToLog((a), mLogErrAction);  \
  SEMMessageBox((a), MB_EXCLAME); \
  AbortMacro(); \
  return; \
}

#define ABORT_LINE(a) \
{ \
  mWinApp->AppendToLog((a) + strLine, mLogErrAction);  \
  SEMMessageBox((a) + strLine, MB_EXCLAME); \
  AbortMacro(); \
  return; \
}

#define ABORT_NORET_LINE(a) \
{ \
  mWinApp->AppendToLog((a) + strLine, mLogErrAction);  \
  SEMMessageBox((a) + strLine, MB_EXCLAME); \
  AbortMacro(); \
}

#define ABORT_NONAV \
{ if (!mWinApp->mNavigator) \
  { \
    CString macStr = CString("The Navigator must be open to execute:\r\n") + strLine; \
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
  CString macStr = CString("Script suspended ") + (a) + strLine;  \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  SuspendMacro(); \
  return; \
}

#define FAIL_CHECK_LINE(a) \
{ \
  CString macStr = CString("Script failed initial checking - nothing was executed."  \
  "\r\n\r\n") + (a) + errmess + ":\r\n\r\n" + strLine;          \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  return 99; \
}

#define FAIL_CHECK_NOLINE(a) \
{ \
  CString macStr = CString("Script failed initial checking - nothing was executed." \
  "\r\n\r\n") + (a) + errmess;             \
  mWinApp->AppendToLog(macStr, mLogErrAction);  \
  SEMMessageBox(macStr, MB_EXCLAME); \
  return 98; \
}

#define CMD_IS(a) (cmdIndex == CME_##a)

#define MAX_TOKENS 60
#define LOOP_LIMIT_FOR_IF -2147000000

enum {VARTYPE_REGULAR, VARTYPE_PERSIST, VARTYPE_INDEX, VARTYPE_REPORT, VARTYPE_LOCAL};
enum {SKIPTO_ENDIF, SKIPTO_ELSE_ENDIF, SKIPTO_ENDLOOP};
enum {TXFILE_READ_ONLY, TXFILE_WRITE_ONLY, TXFILE_MUST_EXIST, TXFILE_MUST_NOT_EXIST,
  TXFILE_QUERY_ONLY};

// An enum with indexes to true commands, preceded by special operations that need to be
// processed with a case within the big switch.  Be sure to adjust the starting number
// so VIEW stays at 0
enum {CME_SCRIPTEND = -7, CME_LABEL, CME_SETVARIABLE, CME_SETSTRINGVAR, CME_DOKEYBREAK,
      CME_ZEROLOOPELSEIF, CME_NOTFOUND,
  CME_VIEW, CME_FOCUS, CME_TRIAL, CME_RECORD, CME_PREVIEW,
  CME_A, CME_ALIGNTO, CME_AUTOALIGN, CME_AUTOFOCUS, CME_BREAK, CME_CALL,
  CME_CALLMACRO, CME_CENTERBEAMFROMIMAGE, CME_CHANGEENERGYLOSS, CME_CHANGEFOCUS,
  CME_CHANGEINTENSITYBY, CME_CHANGEMAG, CME_CIRCLEFROMPOINTS, CME_CLEARALIGNMENT,
  CME_CLOSEFILE, CME_CONICALALIGNTO, CME_CONTINUE, CME_COPY, CME_D, CME_DELAY,
  CME_DOMACRO, CME_ECHO, CME_ELSE, CME_ENDIF, CME_ENDLOOP, CME_EUCENTRICITY,
  CME_EXPOSEFILM, CME_F, CME_G, CME_GOTOLOWDOSEAREA, CME_IF, CME_IMAGESHIFTBYMICRONS,
  CME_IMAGESHIFTBYPIXELS, CME_IMAGESHIFTBYUNITS, CME_INCPERCENTC2, CME_L, CME_LOOP,
  CME_M, CME_MACRONAME, CME_LONGNAME, CME_MANUALFILMEXPOSURE, CME_MONTAGE, CME_MOVESTAGE,
  CME_MOVESTAGETO, CME_NEWMAP, CME_OPENNEWFILE, CME_OPENNEWMONTAGE, CME_PAUSE,
  CME_QUADRANTMEANS, CME_R, CME_READFILE, CME_REALIGNTONAVITEM, CME_REALIGNTOOTHERITEM,
  CME_RECORDANDTILTDOWN, CME_RECORDANDTILTUP, CME_REFINEZLP, CME_REPEAT,
  CME_REPORTACCUMSHIFT, CME_REPORTALIGNSHIFT, CME_REPORTBEAMSHIFT, CME_REPORTCLOCK,
  CME_REPORTDEFOCUS, CME_REPORTFOCUS, CME_REPORTMAG, CME_REPORTMAGINDEX,
  CME_REPORTMEANCOUNTS, CME_REPORTNAVITEM, CME_REPORTOTHERITEM, CME_REPORTPERCENTC2,
  CME_REPORTSHIFTDIFFFROM, CME_REPORTSPOTSIZE, CME_REPORTSTAGEXYZ, CME_REPORTTILTANGLE,
  CME_RESETACCUMSHIFT, CME_RESETCLOCK, CME_RESETIMAGESHIFT, CME_RESETSHIFTIFABOVE,
  CME_RESTORECAMERASET, CME_RETRACTCAMERA, CME_REVERSETILT, CME_S, CME_SAVE,
  CME_SCREENDOWN, CME_SCREENUP, CME_SETBEAMBLANK, CME_SETBEAMSHIFT, CME_SETBINNING,
  CME_SETCAMLENINDEX, CME_SETCOLUMNORGUNVALVE, CME_SETDEFOCUS, CME_SETDIRECTORY,
  CME_SETENERGYLOSS, CME_SETEXPOSURE, CME_SETINTENSITYBYLASTTILT,
  CME_SETINTENSITYFORMEAN, CME_SETMAG, CME_SETOBJFOCUS, CME_SETPERCENTC2, CME_SETSLITIN,
  CME_SETSLITWIDTH, CME_SETSPOTSIZE, CME_SHOW, CME_SUBAREAMEAN, CME_SUPPRESSREPORTS,
  CME_SWITCHTOFILE, CME_T, CME_TILTBY, CME_TILTDOWN, CME_TILTTO,
  CME_TILTUP, CME_U, CME_V, CME_WAITFORDOSE, CME_WALKUPTO, CME_RETURN, CME_EXIT,
  CME_MOVETONAVITEM, CME_SKIPPIECESOUTSIDEITEM, CME_INCTARGETDEFOCUS, CME_REPORTFILEZSIZE,
  CME_SPECIALEXPOSEFILM, CME_SETMONTAGEPARAMS, CME_AUTOCENTERBEAM, CME_COOKSPECIMEN,
  CME_IMAGEPROPERTIES, CME_ITEMFORSUPERCOORD, CME_SETNEWFILETYPE,
  CME_SHIFTITEMSBYALIGNMENT, CME_SHIFTIMAGEFORDRIFT, CME_REPORTFOCUSDRIFT,
  CME_CALIBRATEIMAGESHIFT, CME_SETMAGANDINTENSITY, CME_CHANGEMAGANDINTENSITY,
  CME_OPENOLDFILE, CME_VERBOSE, CME_MAILSUBJECT, CME_SENDEMAIL, CME_UPDATEITEMZ,
  CME_UPDATEGROUPZ, CME_REPORTGROUPSTATUS, CME_CROPIMAGE, CME_CLEARPERSISTENTVARS,
  CME_ELSEIF, CME_YESNOBOX, CME_MAKEDIRECTORY, CME_ALLOWFILEOVERWRITE, CME_OPPOSITETRIAL,
  CME_OPPOSITEFOCUS, CME_OPPOSITEAUTOFOCUS, CME_REPORTAUTOFOCUS, CME_REPORTTARGETDEFOCUS,
  CME_PLUGIN, CME_LISTPLUGINCALLS, CME_SETSTANDARDFOCUS, CME_SETCAMERAAREA,
  CME_SETILLUMINATEDAREA, CME_REPORTILLUMINATEDAREA, CME_SELECTCAMERA, CME_SETLOWDOSEMODE,
  CME_TESTSTEMSHIFT, CME_QUICKFLYBACK, CME_NORMALIZELENSES, CME_REPORTSLOTSTATUS,
  CME_LOADCARTRIDGE, CME_UNLOADCARTRIDGE, CME_BACKLASHADJUST, CME_SAVEFOCUS,
  CME_MAKEDATETIMEDIR, CME_ENTERNAMEOPENFILE, CME_FINDPIXELSIZE, CME_SETEUCENTRICFOCUS,
  CME_CAMERAPROPERTIES, CME_SETIMAGESHIFT, CME_REPORTIMAGESHIFT, CME_ENTERONENUMBER,
  CME_SETHELPERPARAMS, CME_STEPFOCUSNEXTSHOT, CME_BEAMTILTDIRECTION, CME_OPENFRAMESUMFILE,
  CME_DEFERSTACKINGNEXTSHOT, CME_SETOBJECTIVESTIGMATOR, CME_REPORTOBJECTIVESTIGMATOR,
  CME_AREDEWARSFILLING, CME_DEWARSREMAININGTIME, CME_REFRIGERANTLEVEL,
  CME_OPENDECAMERACOVER, CME_SMOOTHFOCUSNEXTSHOT, CME_REPORTLOWDOSE, CME_ISPVPRUNNING,
  CME_LONGOPERATION, CME_UPDATEHWDARKREF, CME_RESTOREFOCUS, CME_CALEUCENTRICFOCUS,
  CME_EARLYRETURNNEXTSHOT, CME_SELECTPIEZO, CME_REPORTPIEZOXY, CME_REPORTPIEZOZ,
  CME_MOVEPIEZOXY, CME_MOVEPIEZOZ, CME_KEYBREAK, CME_REPORTALPHA, CME_SETALPHA,
  CME_SETSTEMDETECTORS, CME_SETCENTEREDSIZE, CME_IMAGELOWDOSESET, CME_FORCECENTERREALIGN,
  CME_SETTARGETDEFOCUS, CME_FOCUSCHANGELIMITS, CME_ABSOLUTEFOCUSLIMITS, 
  CME_REPORTCOLUMNMODE, CME_REPORTLENS, CME_REPORTCOIL, CME_REPORTABSOLUTEFOCUS,
  CME_SETABSOLUTEFOCUS, CME_REPORTENERGYFILTER, CME_REPORTBEAMTILT, CME_USERSETDIRECTORY,
  CME_SETJEOLSTEMFLAGS, CME_PROGRAMTIMESTAMPS, CME_SETCONTINUOUS, CME_USECONTINUOUSFRAMES,
  CME_WAITFORNEXTFRAME, CME_STOPCONTINUOUS, CME_REPORTCONTINUOUS, 
  CME_SETLIVESETTLEFRACTION, CME_SKIPTO, CME_FUNCTION, CME_ENDFUNCTION, CME_CALLFUNCTION,
  CME_PRECOOKMONTAGE, CME_REPORTALIGNTRIMMING, CME_NORMALIZEALLLENSES, CME_ADDTOAUTODOC,
  CME_ISVARIABLEDEFINED, CME_ENTERDEFAULTEDNUMBER, CME_SETUPWAFFLEMONTAGE,
  CME_INCMAGIFFOUNDPIXEL, CME_RESETDEFOCUS, CME_WRITEAUTODOC, CME_ELECTRONSTATS,
  CME_ERRORSTOLOG, CME_FLASHDISPLAY, CME_REPORTPROBEMODE, CME_SETPROBEMODE,
  CME_SAVELOGOPENNEW, CME_SETAXISPOSITION, CME_ADDTOFRAMEMDOC, CME_WRITEFRAMEMDOC,
  CME_REPORTFRAMEMDOCOPEN, CME_SHIFTCALSKIPLENSNORM, CME_REPORTLASTAXISOFFSET,
  CME_SETBEAMTILT, CME_CORRECTASTIGMATISM, CME_CORRECTCOMA, CME_SHIFTITEMSBYCURRENTDIFF,
  CME_REPORTCURRENTFILENAME, CME_SUFFIXFOREXTRAFILE, CME_REPORTSCREEN, CME_GETDEFERREDSUM,
  CME_REPORTJEOLGIF, CME_SETJEOLGIF, CME_TILTDURINGRECORD, CME_SETLDCONTINUOUSUPDATE,
  CME_ERRORBOXSENDEMAIL, CME_REPORTLASTFRAMEFILE, CME_TEST, CME_ABORTIFFAILED, 
  CME_PAUSEIFFAILED, CME_READOTHERFILE, CME_REPORTK2FILEPARAMS, CME_SETK2FILEPARAMS,
  CME_SETDOSEFRACPARAMS, CME_SETK2READMODE, CME_SETPROCESSING, CME_SETFRAMETIME,
  CME_REPORTCOUNTSCALING, CME_SETDIVIDEBY2, CME_REPORTNUMFRAMESSAVED, CME_REMOVEFILE,
  CME_REPORTFRAMEALIPARAMS, CME_SETFRAMEALIPARAMS, CME_REQUIRE, CME_ONSTOPCALLFUNC,
  CME_RETRYREADOTHERFILE, CME_DOSCRIPT, CME_CALLSCRIPT, CME_SCRIPTNAME, CME_SETFRAMEALI2,
  CME_REPORTFRAMEALI2, CME_REPORTMINUTETIME, CME_REPORTCOLUMNORGUNVALVE,
  CME_NAVINDEXWITHLABEL, CME_NAVINDEXWITHNOTE, CME_SETDIFFRACTIONFOCUS,
  CME_REPORTDIRECTORY, CME_BACKGROUNDTILT, CME_REPORTSTAGEBUSY, CME_SETEXPOSUREFORMEAN,
  CME_FFT, CME_REPORTNUMNAVACQUIRE, CME_STARTFRAMEWAITTIMER, CME_REPORTTICKTIME,
  CME_READTEXTFILE, CME_RUNINSHELL, CME_ELAPSEDTICKTIME, CME_NOMESSAGEBOXONERROR,
  CME_REPORTAUTOFOCUSOFFSET, CME_SETAUTOFOCUSOFFSET, CME_CHOOSERFORNEWFILE, 
  CME_SKIPACQUIRINGNAVITEM, CME_SHOWMESSAGEONSCOPE, CME_SETUPSCOPEMESSAGE, CME_SEARCH,
  CME_SETPROPERTY, CME_SETMAGINDEX, CME_SETNAVREGISTRATION, CME_LOCALVAR,
  CME_LOCALLOOPINDEXES, CME_ZEMLINTABLEAU, CME_WAITFORMIDNIGHT, CME_REPORTUSERSETTING,
  CME_SETUSERSETTING, CME_CHANGEITEMREGISTRATION, CME_SHIFTITEMSBYMICRONS,
  CME_SETFREELENSCONTROL, CME_SETLENSWITHFLC, CME_SAVETOOTHERFILE, CME_SKIPACQUIRINGGROUP,
  CME_REPORTIMAGEDISTANCEOFFSET, CME_SETIMAGEDISTANCEOFFSET, CME_REPORTCAMERALENGTH,
  CME_SETDECAMFRAMERATE, CME_SKIPMOVEINNAVACQUIRE, CME_TESTRELAXINGSTAGE, CME_RELAXSTAGE,
  CME_SKIPFRAMEALIPARAMCHECK, CME_ISVERSIONATLEAST, CME_SKIPIFVERSIONLESSTHAN,
  CME_RAWELECTRONSTATS, CME_ALIGNWHOLETSONLY, CME_WRITECOMFORTSALIGN, CME_RECORDANDTILTTO,
  CME_AREPOSTACTIONSENABLED, CME_MEASUREBEAMSIZE, CME_MULTIPLERECORDS, 
  CME_MOVEBEAMBYMICRONS, CME_MOVEBEAMBYFIELDFRACTION, CME_NEWDESERVERDARKREF,
  CME_STARTNAVACQUIREATEND, CME_REDUCEIMAGE, CME_REPORTAXISPOSITION, CME_CTFFIND,
  CME_CBASTIGCOMA, CME_FIXASTIGMATISMBYCTF, CME_FIXCOMABYCTF, CME_ECHOEVAL, 
  CME_REPORTFILENUMBER, CME_REPORTCOMATILTNEEDED, CME_REPORTSTIGMATORNEEDED,
  CME_SAVEBEAMTILT, CME_RESTOREBEAMTILT, CME_REPORTCOMAVSISMATRIX,CME_ADJUSTBEAMTILTFORIS,
  CME_LOADNAVMAP, CME_LOADOTHERMAP,CME_REPORTLENSFLCSTATUS, CME_TESTNEXTMULTISHOT,
  CME_ENTERSTRING, CME_COMPARESTRINGS, CME_COMPARENOCASE, CME_REPORTNEXTNAVACQITEM,
  CME_REPORTNUMTABLEITEMS, CME_CHANGEITEMCOLOR, CME_CHANGEITEMLABEL,CME_STRIPENDINGDIGITS,
  CME_MAKEANCHORMAP, CME_STAGESHIFTBYPIXELS, CME_REPORTPROPERTY, CME_SAVENAVIGATOR,
  CME_FRAMETHRESHOLDNEXTSHOT, CME_QUEUEFRAMETILTSERIES, CME_FRAMESERIESFROMVAR,
  CME_WRITEFRAMESERIESANGLES, CME_ECHOREPLACELINE, CME_ECHONOLINEEND, CME_REMOVEAPERTURE,
  CME_REINSERTAPERTURE, CME_PHASEPLATETONEXTPOS, CME_SETSTAGEBAXIS, CME_REPORTSTAGEBAXIS,
  CME_DEFERWRITINGFRAMEMDOC, CME_ADDTONEXTFRAMESTACKMDOC, CME_STARTNEXTFRAMESTACKMDOC,
  CME_REPORTPHASEPLATEPOS, CME_OPENFRAMEMDOC, CME_NEXTPROCESSARGS, CME_CREATEPROCESS,
  CME_RUNEXTERNALTOOL, CME_REPORTSPECIMENSHIFT, CME_REPORTNAVFILE, CME_READNAVFILE,
  CME_MERGENAVFILE, CME_REPORTIFNAVOPEN, CME_NEWARRAY, CME_NEW2DARRAY, CME_APPENDTOARRAY,
  CME_TRUNCATEARRAY, CME_READ2DTEXTFILE, CME_ARRAYSTATISTICS, CME_REPORTFRAMEBASENAME,
  CME_OPENTEXTFILE, CME_WRITELINETOFILE, CME_READLINETOARRAY, CME_READLINETOSTRING,
  CME_CLOSETEXTFILE, CME_FLUSHTEXTFILE, CME_READSTRINGSFROMFILE, CME_ISTEXTFILEOPEN,
  CME_CURRENTSETTINGSTOLDAREA, CME_UPDATELOWDOSEPARAMS, CME_RESTORELOWDOSEPARAMS,
  CME_CALLSTRINGARRAY, CME_STRINGARRAYTOSCRIPT, CME_MAKEVARPERSISTENT,
  CME_SETLDADDEDBEAMBUTTON, CME_KEEPCAMERASETCHANGES, CME_REPORTDATETIME,
  CME_REPORTFILAMENTCURRENT, CME_SETFILAMENTCURRENT, CME_CLOSEFRAMEMDOC,
  CME_DRIFTWAITTASK, CME_GETWAITTASKDRIFT, CME_CLOSELOGOPENNEW, CME_SAVELOG,
  CME_SAVECALIBRATIONS, CME_REPORTCROSSOVERPERCENTC2, CME_REPORTSCREENCURRENT,
  CME_SETFRAMESERIESPARAMS, CME_SETCUSTOMTIME, CME_REPORTCUSTOMINTERVAL, 
  CME_STAGETOLASTMULTIHOLE, CME_IMAGESHIFTTOLASTMULTIHOLE, CME_NAVINDEXITEMDRAWNON,
  CME_SETMAPACQUIRESTATE, CME_RESTORESTATE, CME_REALIGNTOMAPDRAWNON,
  CME_GETREALIGNTOITEMERROR, CME_DOLOOP, CME_REPORTVACUUMGAUGE, CME_REPORTHIGHVOLTAGE,
  CME_OKBOX, CME_LIMITNEXTAUTOALIGN, CME_SETDOSERATE, CME_CHECKFORBADSTRIPE,
  CME_REPORTK3CDSMODE, CME_SETK3CDSMODE, CME_CONDITIONPHASEPLATE, CME_LINEARFITTOVARS
};

// The two numbers are the minimum arguments and whether arithmetic is allowed
// Arithmetic is allowed for any command with the second number 1, and for any "Set"
// command that does not have a -1 there.  Starting with % lists all explicitly allowed
// commands.  It is redundant to put a 1 in for Set command
static CmdItem cmdList[] = {{NULL,0,0}, {NULL,0,0}, {NULL,0,0}, {NULL,0,0}, {NULL,0,0},
{"A",0,0}, {"AlignTo",1,0}, {"AutoAlign",0,0}, {"AutoFocus",0,0}, {"Break",0,0}, 
{"Call",1,0}, {"CallMacro",1,0},{"CenterBeamFromImage",0,0}, {"ChangeEnergyLoss",1,1},
{"ChangeFocus",1,1},{"ChangeIntensityBy",1,1}, {"ChangeMag",1,1},{"CircleFromPoints",6,1},
{"ClearAlignment",0,0}, {"CloseFile",0,0}, {"ConicalAlignTo",1,0}, {"Continue", 0, 0},
{"Copy",2,0}, {"D",0,0}, {"Delay",1,1}, {"DoMacro",1,0}, {"Echo", 0, 0}, {"Else", 0, 0},
{"Endif", 0, 0}, {"EndLoop", 0, 0}, {"Eucentricity", 0,0}, {"ExposeFilm",0,1}, {"F",0,0},
{"G",0,0}, {"GoToLowDoseArea",1,0}, {"If",3,0}, {"ImageShiftByMicrons", 2, 1},
{"ImageShiftByPixels",2,1}, {"ImageShiftByUnits",2,1}, {"IncPercentC2",1,1}, {"L", 0, 0},
{"Loop",1,1}, {"M",0,0}, {"MacroName",1,0}, {"LongName",1,0}, {"ManualFilmExposure", 1,1},
{"Montage",0,0}, {"MoveStage",2,1},{"MoveStageTo",2,1},{"NewMap",0,0},{"OpenNewFile",1,0},
{"OpenNewMontage",1,0}, {"Pause",0,0}, {"QuadrantMeans",0,0}, {"R",0,0}, {"ReadFile",0,0},
{"RealignToNavItem",1,0}, {"RealignToOtherItem",2,0}, {"RecordAndTiltDown",0,0},
{"RecordAndTiltUp", 0, 0}, {"RefineZLP", 0,0}, {"Repeat", 0,0}, {"ReportAccumShift", 0,0},
{"ReportAlignShift", 0, 0}, {"ReportBeamShift", 0, 0}, {"ReportClock", 0, 0},
{"ReportDefocus",0,0}, {"ReportFocus", 0,0}, {"ReportMag", 0,0}, {"ReportMagIndex", 0,0},
{"ReportMeanCounts", 0, 0}, {"ReportNavItem", 0, 0}, {"ReportOtherItem", 1, 0},
{"ReportPercentC2", 0, 0}, {"ReportShiftDiffFrom", 1, 0}, {"ReportSpotSize", 0, 0},
{"ReportStageXYZ", 0, 0}, {"ReportTiltAngle", 0, 0}, {"ResetAccumShift", 0, 0},
{"ResetClock", 0, 0}, {"ResetImageShift", 0, 0}, {"ResetShiftIfAbove", 1, 1},
{"RestoreCameraSet", 0, 0}, {"RetractCamera", 0, 0}, {"ReverseTilt", 0, 0}, {"S", 0, 0},
{"Save", 0, 0}, {"ScreenDown", 0, 0}, {"ScreenUp", 0, 0}, {"SetBeamBlank", 1, 0},
{"SetBeamShift", 2, 0}, {"SetBinning", 2, 0}, {"SetCamLenIndex", 1, 0},
{"SetColumnOrGunValve", 1, 0}, {"SetDefocus", 1, 0}, {"SetDirectory", 1, -1},
{"SetEnergyLoss", 1, 0}, {"SetExposure", 2, 0}, {"SetIntensityByLastTilt", 0, -1},
{"SetIntensityForMean", 0,0}, {"SetMag",1,0}, {"SetObjFocus",1,0}, {"SetPercentC2", 1,0},
{"SetSlitIn", 0, 1}, {"SetSlitWidth", 1, 0}, {"SetSpotSize", 1, 0}, {"Show", 1, 0},
{"SubareaMean", 4, 1}, {"SuppressReports", 0,0}, {"SwitchToFile", 1,0}, {"T", 0, 0},
{"TiltBy", 1, 1}, {"TiltDown", 0, 0}, {"TiltTo", 1, 1}, {"TiltUp", 0, 0},
{"U",0,0}, {"V",0,0}, {"WaitForDose",1,1}, {"WalkUpTo",1,1}, {"Return",0,0}, {"Exit",0,0},
{"MoveToNavItem", 0, 0}, {"SkipPiecesOutsideItem", 1, 0}, {"IncTargetDefocus", 1, 1},
{"ReportFileZsize", 0, 0}, {"SpecialExposeFilm", 2, 1}, {"SetMontageParams", 1, 0},
{"AutoCenterBeam", 0, 0}, {"CookSpecimen", 0, 0}, {"ImageProperties", 0, 0},
{"ItemForSuperCoord", 1, 0}, {"SetNewFileType", 1, 0}, {"ShiftItemsByAlignment", 0, 0},
{"ShiftImageForDrift", 3, 0}, {"ReportFocusDrift", 0, 0}, {"CalibrateImageShift", 0, 0},
{"SetMagAndIntensity", 1, 0}, {"ChangeMagAndIntensity", 1, 1}, {"OpenOldFile", 1, 0},
{"verbose", 1, 0}, {"MailSubject", 1, 0}, {"SendEmail", 1, 0}, {"UpdateItemZ", 0, 0},
{"UpdateGroupZ", 0, 0}, {"ReportGroupStatus", 0, 0}, {"CropImage", 5, 1},
{"ClearPersistentVars", 0, 0}, {"ElseIf", 3,0}, {"YesNoBox", 1,0}, {"MakeDirectory",1,0},
{"AllowFileOverwrite", 1, 0}, {"OppositeTrial", 0, 0}, {"OppositeFocus", 0, 0},
{"OppositeAutoFocus", 0, 0}, {"ReportAutoFocus", 0, 0}, {"ReportTargetDefocus", 0, 0},
{"Plugin",2,0}, {"ListPluginCalls",0,0}, {"SetStandardFocus",1,0}, {"SetCameraArea",2,0},
{"SetIlluminatedArea", 1, 0}, {"ReportIlluminatedArea", 0, 0}, {"SelectCamera", 1, 0},
{"SetLowDoseMode",1,0},{"TestSTEMshift",3,1},{"QuickFlyback",2,1},{"NormalizeLenses",1,0},
{"ReportSlotStatus", 1, 0}, {"LoadCartridge", 1, 0}, {"UnloadCartridge", 0, 0},
{"BacklashAdjust", 0, 0}, {"SaveFocus", 0, 0}, {"MakeDateTimeDir", 0, 0},
{"EnterNameOpenFile", 0, 0}, {"FindPixelSize", 0, 0}, {"SetEucentricFocus", 0, 0},
{"CameraProperties", 0, 0}, {"SetImageShift", 2, 0}, {"ReportImageShift", 0, 0},
{"EnterOneNumber", 1, 0}, {"SetHelperParams", 1, 0}, {"StepFocusNextShot", 2, 1},
{"BeamTiltDirection", 1, 1}, {"OpenFrameSumFile", 1, 0}, {"DeferStackingNextShot", 0, 0},
{"SetObjectiveStigmator",2,0}, {"ReportObjectiveStigmator",0,0}, {"AreDewarsFilling",0,0},
{"DewarsRemainingTime", 0, 0}, {"RefrigerantLevel", 0, 0}, {"OpenDECameraCover", 0, 0},
{"SmoothFocusNextShot", 2, 1}, {"ReportLowDose", 0, 0}, {"IsPVPRunning", 0, 0},
{"LongOperation", 1, 1}, {"UpdateHWDarkRef", 1, 1}, {"RestoreFocus", 0, 0},
{"CalEucentricFocus", 0, 0}, {"EarlyReturnNextShot", 1, 1}, {"SelectPiezo", 2, 0},
{"ReportPiezoXY", 0, 0}, {"ReportPiezoZ", 0, 0}, {"MovePiezoXY",2,1}, {"MovePiezoZ",1,1},
{"KeyBreak", 0, 0}, {"ReportAlpha", 0, 0}, {"SetAlpha", 1, 0}, {"SetSTEMDetectors", 2, 0},
{"SetCenteredSize", 4, 0}, {"ImageLowDoseSet", 0, 0}, {"ForceCenterRealign", 0, 0},
{"SetTargetDefocus", 1, 0}, {"FocusChangeLimits", 2, 1}, {"AbsoluteFocusLimits", 2, 1},
{"ReportColumnMode",0,0},{"ReportLens",1,0},{"ReportCoil",1,0},{"ReportAbsoluteFocus",0,0},
{"SetAbsoluteFocus", 1, 0}, {"ReportEnergyFilter", 0, 0}, {"ReportBeamTilt", 0, 0},
{"UserSetDirectory", 0, 0}, {"SetJeolSTEMflags", 2, -1}, {"ProgramTimeStamps", 0, 0},
{"SetContinuous", 2, -1}, {"UseContinuousFrames", 1, 0}, {"WaitForNextFrame", 0, 0},
{"StopContinuous", 0, 0}, {"ReportContinuous", 0, 0}, {"SetLiveSettleFraction", 1, 0},
{"SkipTo", 1, 0}, {"Function", 1, 0}, {"EndFunction", 0, 0}, {"CallFunction", 1, 0},
{"PreCookMontage", 1, 1}, {"ReportAlignTrimming", 0, 0}, {"NormalizeAllLenses", 0, 0},
{"AddToAutodoc", 2}, {"IsVariableDefined", 1, 0}, {"EnterDefaultedNumber", 3, 0},
{"SetupWaffleMontage", 2, -1}, {"IncMagIfFoundPixel", 1, 0}, {"ResetDefocus", 0, 0},
{"WriteAutodoc",0,0}, {"ElectronStats",0,0}, {"ErrorsToLog", 0,0}, {"FlashDisplay", 0,0},
{"ReportProbeMode",0,0},{"SetProbeMode",1,0},{"SaveLogOpenNew",0,0},{"SetAxisPosition",2,0}
, {"AddToFrameMdoc", 2, 0}, {"WriteFrameMdoc", 0, 0}, {"ReportFrameMdocOpen", 0, 0},
{"ShiftCalSkipLensNorm",0,0}, {"ReportLastAxisOffset",0,0}, {"SetBeamTilt",2,0},//end in 3.5
{"CorrectAstigmatism", 0, 0}, {"CorrectComa", 0, 0}, {"ShiftItemsByCurrentDiff", 1, 0},
{"ReportCurrentFilename", 0, 0}, {"SuffixForExtraFile", 1, 0}, {"ReportScreen", 0, 0},
{"GetDeferredSum",0,0}, {"ReportJeolGIF",0,0},{"SetJeolGIF",1,0},{"TiltDuringRecord",2,1},
{"SetLDContinuousUpdate", 1, 0}, {"ErrorBoxSendEmail", 1,0}, {"ReportLastFrameFile", 0,0},
{"Test", 1, 0}, {"AbortIfFailed", 1, 0}, {"PauseIfFailed", 0, 0}, {"ReadOtherFile", 3, 0},
{"ReportK2FileParams", 0, 0}, {"SetK2FileParams", 1, 0}, {"SetDoseFracParams", 1, 0},
{"SetK2ReadMode", 2, 0}, {"SetProcessing", 2, 0}, {"SetFrameTime", 2, 0},
{"ReportCountScaling", 0, 0}, {"SetDivideBy2", 1, 0}, {"ReportNumFramesSaved", 0, 0},
{"RemoveFile", 1, 0}, {"ReportFrameAliParams", 0, 0}, {"SetFrameAliParams", 1, 0},
{"Require", 1, 0}, {"OnStopCallFunc", 1,0}, {"RetryReadOtherFile",1,0}, {"DoScript",1,0},
{"CallScript",1,0}, {"ScriptName", 1, 0}, {"SetFrameAli2", 1,0}, {"ReportFrameAli2", 0,0},
{"ReportMinuteTime", 0, 0}, {"ReportColumnOrGunValve", 0, 0}, {"NavIndexWithLabel", 1,0},
{"NavIndexWithNote", 1, 0}, {"SetDiffractionFocus", 1, 0}, {"ReportDirectory", 0, 0},
{"BackgroundTilt",1,1}, {"ReportStageBusy",0,0}, {"SetExposureForMean",1,0}, {"FFT",1,0},
{"ReportNumNavAcquire", 0, 0}, {"StartFrameWaitTimer", 0,0}, {"ReportTickTime", 0, 0},
{"ReadTextFile",2,0},{"RunInShell",1,0},{"ElapsedTickTime",1,1},{"NoMessageBoxOnError",0,0},
{"ReportAutofocusOffset", 0, 0}, {"SetAutofocusOffset", 1, 0}, {"ChooserForNewFile", 2,0},
{"SkipAcquiringNavItem", 0, 0}, {"ShowMessageOnScope", 1, 0}, {"SetupScopeMessage", 1,-1},
{"Search", 0, 0}, {"SetProperty", 2, 0}, /* End in 3.6 */{"SetMagIndex", 1, 0},
{"SetNavRegistration",1,0},{"LocalVar",1,0},{"LocalLoopIndexes",0,0},{"ZemlinTableau",1,1},
{"WaitForMidnight", 0, 1}, {"ReportUserSetting", 1, 0}, {"SetUserSetting", 2, 0},
{"ChangeItemRegistration",2,1}, {"ShiftItemsByMicrons",2,1}, {"SetFreeLensControl", 2, 1},
{"SetLensWithFLC", 2, 0}, {"SaveToOtherFile", 4, 0}, {"SkipAcquiringGroup", 0, 0},
{"ReportImageDistanceOffset", 0, 0}, {"SetImageDistanceOffset", 1, 0},
{"ReportCameraLength", 0, 0}, {"SetDECamFrameRate", 1, 0}, {"SkipMoveInNavAcquire", 0, 0},
{"TestRelaxingStage", 2, 1}, {"RelaxStage", 0, 1}, {"SkipFrameAliParamCheck", 0, 0},
{"IsVersionAtLeast", 1, 0}, {"SkipIfVersionLessThan", 1, 0}, {"RawElectronStats", 0, 0},
{"AlignWholeTSOnly", 0, 0}, {"WriteComForTSAlign", 0, 0}, {"RecordAndTiltTo", 1, 1},
{"ArePostActionsEnabled", 0, 0}, {"MeasureBeamSize", 0, 0}, {"MultipleRecords", 0, 1},
{"MoveBeamByMicrons",2,1}, {"MoveBeamByFieldFraction", 2, 1}, {"NewDEserverDarkRef", 2,0},
{"StartNavAcquireAtEnd", 0, 0}, {"ReduceImage", 2, 1}, {"ReportAxisPosition", 1, 0},
{"CtfFind",3,1}, {"CBAstigComa",3,0}, {"FixAstigmatismByCTF",0,0}, {"FixComaByCTF", 0, 0},
{"EchoEval", 0, 1}, {"ReportFileNumber", 0, 0}, {"ReportComaTiltNeeded", 0, 0},
{"ReportStigmatorNeeded", 0, 0}, {"SaveBeamTilt", 0, 0}, {"RestoreBeamTilt", 0, 0},
{"ReportComaVsISmatrix", 0, 0}, {"AdjustBeamTiltforIS", 0, 0}, {"LoadNavMap", 0, 0},
{"LoadOtherMap", 1, 0}, {"ReportLensFLCStatus", 1, 0}, {"TestNextMultiShot", 1, 0},
{"EnterString", 2, 0}, {"CompareStrings", 2, 0}, {"CompareNoCase", 2, 0},
{"ReportNextNavAcqItem", 0, 0}, {"ReportNumTableItems", 0, 0}, {"ChangeItemColor", 2, 0},
{"ChangeItemLabel", 2, 0}, {"StripEndingDigits", 2, 0}, {"MakeAnchorMap", 0, 0},
{"StageShiftByPixels", 2, 1}, {"ReportProperty", 1, 0}, {"SaveNavigator", 0, 0},
{"FrameThresholdNextShot", 1, 1}, {"QueueFrameTiltSeries", 3, 0},
{"FrameSeriesFromVar", 2, 0}, {"WriteFrameSeriesAngles", 1, 0}, {"EchoReplaceLine", 1, 1},
{"EchoNoLineEnd", 1, 1}, {"RemoveAperture", 1, 0}, {"ReInsertAperture", 1, 0},
{"PhasePlateToNextPos", 0, 0}, {"SetStageBAxis", 1, 1}, {"ReportStageBAxis", 0, 0},
{"DeferWritingFrameMdoc", 0, 0}, {"AddToNextFrameStackMdoc", 2, 0},
{"StartNextFrameStackMdoc", 2, 0}, {"ReportPhasePlatePos", 0, 0}, {"OpenFrameMdoc", 1, 0},
{"NextProcessArgs", 1, 0}, {"CreateProcess", 1, 0}, {"RunExternalTool", 1, 0},
{"ReportSpecimenShift", 0, 0}, {"ReportNavFile", 0, 0}, {"ReadNavFile", 1, 0},
{"MergeNavFile", 1, 0}, {"ReportIfNavOpen",0, 0}, /* End in 3.7 */
{"NewArray", 3, 0}, {"New2DArray", 1, 0}, {"AppendToArray", 2, 0},{"TruncateArray", 2, 0},
{"Read2DTextFile", 2, 0}, {"ArrayStatistics", 1, 0}, {"ReportFrameBaseName", 0, 0},
{"OpenTextFile", 4, 0}, {"WriteLineToFile", 1, 0}, {"ReadLineToArray", 2, 0},
{"ReadLineToString", 2, 0}, {"CloseTextFile", 1, 0}, {"FlushTextFile", 1, 0},
{"ReadStringsFromFile", 2, 0}, {"IsTextFileOpen", 1, 0},{"CurrentSettingsToLDArea", 1, 0},
{"UpdateLowDoseParams", 1, 0}, {"RestoreLowDoseParams", 0, 0}, {"CallStringArray", 1, 0},
{"StringArrayToScript", 1, 0}, {"MakeVarPersistent", 1, 0},{"SetLDAddedBeamButton", 0, 0},
{"KeepCameraSetChanges", 0, 0}, {"ReportDateTime", 0, 0}, {"ReportFilamentCurrent", 0, 0},
{"SetFilamentCurrent", 1, 0}, {"CloseFrameMdoc", 0, 0}, {"DriftWaitTask", 0, 1},
{"GetWaitTaskDrift", 0, 0}, {"CloseLogOpenNew", 0, 0}, {"SaveLog", 0, 0},
{"SaveCalibrations", 0, 0}, {"ReportCrossoverPercentC2", 0, 0}, {"ReportScreenCurrent", 0, 0},
{"SetFrameSeriesParams", 1, 0}, {"SetCustomTime", 1, 0}, {"ReportCustomInterval", 1, 0},
{"StageToLastMultiHole", 0, 0}, {"ImageShiftToLastMultiHole", 0, 0}, 
{"NavIndexItemDrawnOn", 1, 0}, {"SetMapAcquireState", 1, 0}, {"RestoreState", 0, 0},
{"RealignToMapDrawnOn", 2, 0}, {"GetRealignToItemError", 0, 0}, {"DoLoop", 3, 1},
{"ReportVacuumGauge", 1, 0}, {"ReportHighVoltage", 0, 0},{"OKBox", 1, 0},
{"LimitNextAutoAlign", 1, 1}, {"SetDoseRate", 1, 0},{"CheckForBadStripe", 0, 0},/*CAI3.8*/
{"ReportK3CDSmode", 0, 0}, {"SetK3CDSmode", 1, 0}, {"ConditionPhasePlate", 0, 0},
{"LinearFitToVars", 2, 0},
{NULL, 0, 0}
};
// The longest is now 25 characters but 23 is a more common limit

#define NUM_COMMANDS (sizeof(cmdList) / sizeof(CmdItem))

static int sProcessExitStatus;
static int sProcessErrno;

BEGIN_MESSAGE_MAP(CMacroProcessor, CCmdTarget)
  //{{AFX_MSG_MAP(CMacroProcessor)
  ON_COMMAND(ID_MACRO_END, OnMacroEnd)
  ON_COMMAND(ID_MACRO_STOP, OnMacroStop)
  ON_COMMAND(ID_MACRO_RESUME, OnMacroResume)
  ON_UPDATE_COMMAND_UI(ID_MACRO_RESUME, OnUpdateMacroResume)
  ON_COMMAND(ID_MACRO_CONTROLS, OnMacroControls)
  ON_UPDATE_COMMAND_UI(ID_MACRO_STOP, OnUpdateMacroStop)
  //}}AFX_MSG_MAP
  ON_COMMAND_RANGE(ID_MACRO_EDIT1, ID_MACRO_EDIT10, OnMacroEdit)
  ON_UPDATE_COMMAND_UI_RANGE(ID_MACRO_EDIT1, ID_MACRO_EDIT10, OnUpdateMacroEdit)
  ON_COMMAND_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN40, OnMacroRun)
  ON_UPDATE_COMMAND_UI_RANGE(ID_MACRO_RUN1, ID_MACRO_RUN40, OnUpdateMacroRun)
  ON_COMMAND(ID_MACRO_TOOLBAR, OnMacroToolbar)
  ON_UPDATE_COMMAND_UI(ID_MACRO_END, OnUpdateMacroEnd)
  ON_COMMAND(ID_MACRO_SETLENGTH, OnMacroSetlength)
  ON_COMMAND(ID_MACRO_VERBOSE, OnMacroVerbose)
  ON_UPDATE_COMMAND_UI(ID_MACRO_VERBOSE, OnUpdateMacroVerbose)
  ON_COMMAND(ID_MACRO_EDIT15, OnMacroEdit15)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT15, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT20, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT20, OnMacroEdit20)
  ON_COMMAND(ID_MACRO_EDIT25, OnMacroEdit25)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT25, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT30, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT30, OnMacroEdit30)
  ON_COMMAND(ID_MACRO_EDIT35, OnMacroEdit35)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT35, OnUpdateMacroEdit)
  ON_UPDATE_COMMAND_UI(ID_MACRO_EDIT40, OnUpdateMacroEdit)
  ON_COMMAND(ID_MACRO_EDIT40, OnMacroEdit40)
  ON_COMMAND(ID_MACRO_READMANY, OnMacroReadMany)
  ON_UPDATE_COMMAND_UI(ID_MACRO_READMANY, OnUpdateMacroReadMany)
  ON_COMMAND(ID_MACRO_WRITEALL, OnMacroWriteAll)
  ON_UPDATE_COMMAND_UI(ID_MACRO_WRITEALL, OnUpdateMacroWriteAll)
  ON_COMMAND(ID_MACRO_LISTFUNCTIONS, OnMacroListFunctions)
  ON_UPDATE_COMMAND_UI(ID_MACRO_LISTFUNCTIONS, OnUpdateMacroWriteAll)
  ON_COMMAND(ID_SCRIPT_SETINDENTSIZE, OnScriptSetIndentSize)
  ON_COMMAND(ID_SCRIPT_CLEARPERSISTENTVARS, OnScriptClearPersistentVars)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_CLEARPERSISTENTVARS, OnUpdateClearPersistentVars)
  ON_COMMAND(ID_SCRIPT_RUNONECOMMAND, OnScriptRunOneCommand)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_RUNONECOMMAND, OnUpdateScriptRunOneCommand)
  ON_COMMAND(ID_SCRIPT_OPENEDITORSONSTART, OnOpenEditorsOnStart)
  ON_UPDATE_COMMAND_UI(ID_SCRIPT_OPENEDITORSONSTART, OnUpdateOpenEditorsOnStart)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMacroProcessor::CMacroProcessor()
{
  int i;
  unsigned int hash;
  std::string tmpOp1[] = { "SQRT", "COS", "SIN", "TAN", "ATAN", "ABS", "NEARINT",
    "LOG", "LOG10", "EXP" };
  std::string tmpOp2[] = { "ROUND", "POWER", "ATAN2", "MODULO", "DIFFABS",
    "FRACDIFF", "MIN", "MAX" };
  std::string keywords[] = { "REPEAT", "ENDLOOP", "DOMACRO", "LOOP", "CALLMACRO", "DOLOOP"
    , "ENDIF", "IF", "ELSE", "BREAK", "CONTINUE", "CALL", "EXIT", "RETURN", "KEYBREAK",
    "SKIPTO", "FUNCTION", "CALLFUNCTION", "ENDFUNCTION", "CALLSCRIPT", "DOSCRIPT" };
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mModeNames = mWinApp->GetModeNames();
  mMacNames = mWinApp->GetMacroNames();
  mMagTab = mWinApp->GetMagTable();
  mImBufs = mWinApp->GetImBufs();
  mMacros = mWinApp->GetMacros();
  mControl = mWinApp->GetMacControl();
  mMacroEditer = &(mWinApp->mMacroEditer[0]);
  mConSets = mWinApp->GetConSets();
  mOneLineScript = NULL;
  mFunctionSet1.insert(tmpOp1, tmpOp1 + sizeof(tmpOp1) / sizeof(std::string));
  mFunctionSet2.insert(tmpOp2 , tmpOp2 + sizeof(tmpOp2) / sizeof(std::string));
  mReservedWords.insert(tmpOp1, tmpOp1 + sizeof(tmpOp1) / sizeof(std::string));
  mReservedWords.insert(tmpOp2, tmpOp2 + sizeof(tmpOp2) / sizeof(std::string));
  mReservedWords.insert(keywords, keywords + sizeof(keywords) / sizeof(std::string));
  mDoingMacro = false;
  mCurrentMacro = -1;
  mInitialVerbose = false;
  mVarArray.SetSize(0, 5);
  mSleepTime = 0.;
  mDoseTarget = 0.;
  mMovedStage = false;
  mMovedScreen = false;
  mExposedFilm = false;
  mStartedLongOp = false;
  mMovedPiezo = false;
  mMovedAperture = false;
  mLoadingMap = false;
  mMakingDualMap = false;
  mLastCompleted = false;
  mLastAborted = false;
  mEnteredName = "";
  mToolPlacement.rcNormalPosition.right = 0;
  mNumToolButtons = 10;
  mToolButHeight = 0;
  mAutoIndentSize = 3;
  mRestoreMacroEditors = true;
  mOneLinePlacement.rcNormalPosition.right = 0;
  mMailSubject = "Message from SerialEM script";
  for (i = 0; i < 5; i++)
    cmdList[i].mixedCase = (LPCTSTR)mModeNames[i];
  CString cstr;
  for (i = 0; i < NUM_COMMANDS - 1; i++) {
    cstr = cmdList[i].mixedCase;
    cstr.MakeUpper();
    cmdList[i].cmd = (LPCTSTR)cstr;
    if (cmdList[i].arithAllowed > 0)
      mArithAllowed.insert(cmdList[i].cmd);
    if (cmdList[i].arithAllowed < 0)
      mArithDenied.insert(cmdList[i].cmd);

    // Get a hash value from the upper case command string and map it to index, or to 0
    // if there is a collision
    hash = StringHashValue((LPCTSTR)cstr);
    if (!mCmdHashMap.count(hash))
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, i));
    else
      mCmdHashMap.insert(std::pair<unsigned int, int>(hash, 0));
  }
  for (i = 0; i < MAX_MACROS; i++) {
    mStrNum[i].Format("%d", i + 1);
    mFuncArray[i].SetSize(0, 4);
    mEditerPlacement[i].rcNormalPosition.right = 0;
  }
  srand(GetTickCount());
  mProcessThread = NULL;
}

CMacroProcessor::~CMacroProcessor()
{
  ClearVariables();
  ClearVariables(VARTYPE_PERSIST);
  ClearFunctionArray(-1);
  CloseFileForText(-2);
}

void CMacroProcessor::Initialize()
{
  mScope = mWinApp->mScope;
  mCamera = mWinApp->mCamera;
  mBufferManager = mWinApp->mBufferManager;
  mShiftManager = mWinApp->mShiftManager;
  if (GetDebugOutput('%')) {
    mWinApp->AppendToLog("Commands allowing arithmetic in arguments:");
    for (int i = 0; i < NUM_COMMANDS - 1; i++)
      if (cmdList[i].arithAllowed > 0)
        mWinApp->AppendToLog(cmdList[i].mixedCase);
  }
}

// MESSAGE HANDLERS
void CMacroProcessor::OnMacroEdit(UINT nID)
{
  int index = nID - ID_MACRO_EDIT1;
  if (nID == ID_MACRO_EDIT15)
    index = 14;
  if (nID == ID_MACRO_EDIT20)
    index = 19;
  if (nID == ID_MACRO_EDIT25)
    index = 24;
  if (nID == ID_MACRO_EDIT30)
    index = 29;
  if (nID == ID_MACRO_EDIT35)
    index = 34;
  if (nID == ID_MACRO_EDIT40)
    index = 39;
  OpenMacroEditor(index);
}

// Actually open an editor if it is not open
void CMacroProcessor::OpenMacroEditor(int index)
{
  if (index < 0 || index >= MAX_MACROS || mMacroEditer[index])
    return;
  mMacroEditer[index] = new CMacroEditer;
  ASSERT(mMacroEditer[index] != NULL);
  mMacroEditer[index]->m_strMacro = mMacros[index];
  mMacroEditer[index]->m_iMacroNumber = index;
  mMacroEditer[index]->Create(IDD_MACRO);
  mMacroEditer[index]->ShowWindow(SW_SHOW);
  mWinApp->mCameraMacroTools.Update();
}

void CMacroProcessor::OnUpdateMacroEdit(CCmdUI* pCmdUI)
{
  int index = pCmdUI->m_nID - ID_MACRO_EDIT1;
  if (pCmdUI->m_nID == ID_MACRO_EDIT15)
    index = 14;
  if (pCmdUI->m_nID == ID_MACRO_EDIT20)
    index = 19;
  if (pCmdUI->m_nID == ID_MACRO_EDIT25)
    index = 24;
  if (pCmdUI->m_nID == ID_MACRO_EDIT30)
    index = 29;
  if (pCmdUI->m_nID == ID_MACRO_EDIT35)
    index = 34;
  if (pCmdUI->m_nID == ID_MACRO_EDIT40)
    index = 39;
  pCmdUI->Enable(!DoingMacro() && !mMacroEditer[index]);
}

void CMacroProcessor::OnMacroEdit15()
{
  OnMacroEdit(ID_MACRO_EDIT15);
}

void CMacroProcessor::OnMacroEdit20()
{
  OnMacroEdit(ID_MACRO_EDIT20);
}

void CMacroProcessor::OnMacroEdit25()
{
  OnMacroEdit(ID_MACRO_EDIT25);
}

void CMacroProcessor::OnMacroEdit30()
{
  OnMacroEdit(ID_MACRO_EDIT30);
}

void CMacroProcessor::OnMacroEdit35()
{
  OnMacroEdit(ID_MACRO_EDIT35);
}

void CMacroProcessor::OnMacroEdit40()
{
  OnMacroEdit(ID_MACRO_EDIT40);
}

void CMacroProcessor::OnMacroToolbar()
{
  if (mWinApp->mMacroToolbar) {
    mWinApp->mMacroToolbar->BringWindowToTop();
    return;
  }
  mWinApp->mMacroToolbar = new CMacroToolbar();
  mWinApp->mMacroToolbar->Create(IDD_MACROTOOLBAR);
  mWinApp->SetPlacementFixSize(mWinApp->mMacroToolbar, &mToolPlacement);
  mWinApp->RestoreViewFocus();
}

void CMacroProcessor::OnMacroSetlength()
{
  CString str;
  int num = mNumToolButtons;
  str.Format("Number of buttons to show in script toolbar (between 5 and %d):",
    MAX_MACROS);
  if (!KGetOneInt(str, num))
    return;
  if (!KGetOneInt("Height of each button in pixels (0 for default):", mToolButHeight))
    return;
  mNumToolButtons = B3DMIN(MAX_MACROS, B3DMAX(5, num));
  if (mWinApp->mMacroToolbar)
    mWinApp->mMacroToolbar->SetLength(mNumToolButtons, mToolButHeight);
}

void CMacroProcessor::OnScriptClearPersistentVars()
{
  ClearVariables(VARTYPE_PERSIST);
  mCurrentMacro = -1;
  mWinApp->mCameraMacroTools.Update();
}

void CMacroProcessor::OnUpdateClearPersistentVars(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingMacro() && !mWinApp->DoingTasks());
}

void CMacroProcessor::OnScriptSetIndentSize()
{
  KGetOneInt("Number of spaces for automatic indentation, or 0 to disable:", 
    mAutoIndentSize);
}

void CMacroProcessor::OnOpenEditorsOnStart()
{
  mRestoreMacroEditors = !mRestoreMacroEditors;
}

void CMacroProcessor::OnUpdateOpenEditorsOnStart(CCmdUI *pCmdUI)
{
  pCmdUI->Enable();
  pCmdUI->SetCheck(mRestoreMacroEditors ? 1 : 0);
}

void CMacroProcessor::OnMacroVerbose()
{
  mInitialVerbose = !mInitialVerbose;
}

void CMacroProcessor::OnUpdateMacroVerbose(CCmdUI *pCmdUI)
{
  CString menuText;
  CString *longMacNames = mWinApp->GetLongMacroNames();
  pCmdUI->Enable();
  pCmdUI->SetCheck(mInitialVerbose ? 1 : 0);
  for (int ind = 0; ind < 20; ind++) {
    if (!longMacNames[ind].IsEmpty())
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)longMacNames[ind]);
    else if (mMacNames[ind].IsEmpty())
      menuText.Format("Run %d", ind + 1);
    else
      menuText.Format("%d: %s", ind + 1, (LPCTSTR)mMacNames[ind]);
    UtilModifyMenuItem(5, ID_MACRO_RUN1 + ind, (LPCTSTR)menuText);
  }

}

// Read and write many macros
void CMacroProcessor::OnMacroReadMany()
{
  CString filename;
  if (mWinApp->mDocWnd->GetTextFileName(true, false, filename))
    return;
  mWinApp->mParamIO->ReadMacrosFromFile(filename);
  if (mWinApp->mMacroToolbar)
    mWinApp->mMacroToolbar->SetLength(mNumToolButtons, mToolButHeight);
}

void CMacroProcessor::OnUpdateMacroReadMany(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!(mWinApp->DoingTasks() || mWinApp->DoingTiltSeries() ||
    (mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring())));
}

void CMacroProcessor::OnMacroWriteAll()
{
  CString filename;
  if (mWinApp->mDocWnd->GetTextFileName(false, false, filename))
    return;
  mWinApp->mParamIO->WriteMacrosToFile(filename);
}

void CMacroProcessor::OnUpdateMacroWriteAll(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks());
}

void CMacroProcessor::OpenMacroToolbar(void)
{
  OnMacroToolbar();
}

void CMacroProcessor::ToolbarMacroRun(UINT nID)
{
  OnMacroRun(nID);
}

WINDOWPLACEMENT * CMacroProcessor::GetToolPlacement(void)
{
  if (mWinApp->mMacroToolbar)
    mWinApp->mMacroToolbar->GetWindowPlacement(&mToolPlacement);
  return &mToolPlacement;
}

void CMacroProcessor::ToolbarClosing(void)
{
  mWinApp->mMacroToolbar->GetWindowPlacement(&mToolPlacement);
  mWinApp->mMacroToolbar = NULL;
}

WINDOWPLACEMENT *CMacroProcessor::FindEditerPlacement(int index)
{
  if (mMacroEditer[index])
    mMacroEditer[index]->GetWindowPlacement(&mEditerPlacement[index]);
  return &mEditerPlacement[index];
}

// List all the functions defined in macros
void CMacroProcessor::OnMacroListFunctions()
{
  int index, fun;
  CString title;
  MacroFunction *funcP;
  for (index = 0; index < MAX_MACROS; index++) {
    if (mMacroEditer[index])
      mMacroEditer[index]->TransferMacro(true);
    ScanForName(index, &mMacros[index]);
    if (mFuncArray[index].GetSize()) {
      title.Format("\r\nScript %d", index + 1);
      if (!mMacNames[index].IsEmpty())
        title += ": " + mMacNames[index];
      mWinApp->AppendToLog(title);
      for (fun = 0; fun < (int)mFuncArray[index].GetSize(); fun++) {
        funcP = mFuncArray[index].GetAt(fun);
        PrintfToLog("%s %d %d", (LPCTSTR)funcP->name, funcP->numNumericArgs, 
          funcP->ifStringArg ? 1 : 0);
      }
    }
  }
}

void CMacroProcessor::OnMacroRun(UINT nID)
{
  int index = nID - ID_MACRO_RUN1;
  if (!MacroRunnable(index))
    return;

  // If the editor is open, unload the string and copy to macro
  if (mMacroEditer[index])
    mMacroEditer[index]->TransferMacro(true);
  if (!mMacros[index].IsEmpty())
    Run(index);
}

// Enable a macro if there are no tasks, and it is nonempty or being edited
void CMacroProcessor::OnUpdateMacroRun(CCmdUI* pCmdUI)
{
  int index = pCmdUI->m_nID - ID_MACRO_RUN1;
  pCmdUI->Enable(MacroRunnable(index));
}


void CMacroProcessor::OnScriptRunOneCommand()
{
  if (mOneLineScript) {
    mOneLineScript->BringWindowToTop();
    return;
  }
  mOneLineScript = new COneLineScript();
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    mOneLineScript->m_strOneLine[ind] = mMacros[MAX_MACROS + ind];
    mOneLineScript->m_strOneLine[ind].TrimRight("\r\n");
  }
  mOneLineScript->mMacros = mMacros;
  mOneLineScript->Create(IDD_ONELINESCRIPT);
  if (mOneLinePlacement.rcNormalPosition.right > 0)
    mOneLineScript->SetWindowPlacement(&mOneLinePlacement);
  mOneLineScript->ShowWindow(SW_SHOW);
  mWinApp->RestoreViewFocus();
}

void CMacroProcessor::OnUpdateScriptRunOneCommand(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!DoingMacro());
}

WINDOWPLACEMENT *CMacroProcessor::GetOneLinePlacement(void)
{
  if (mOneLineScript)
    mOneLineScript->GetWindowPlacement(&mOneLinePlacement);
  return &mOneLinePlacement;
}

void CMacroProcessor::OneLineClosing(void)
{
  mOneLineScript->GetWindowPlacement(&mOneLinePlacement);
  mOneLineScript = NULL;
}

// Transfer all one-line scripts to or from the dialog if it is open
void CMacroProcessor::TransferOneLiners(bool fromDialog)
{
  if (!mOneLineScript)
    return;
  for (int ind = 0; ind < MAX_ONE_LINE_SCRIPTS; ind++) {
    mOneLineScript->UpdateData(fromDialog);
    if (fromDialog)
      mMacros[MAX_MACROS + ind] = mOneLineScript->m_strOneLine[ind];
    else
      mOneLineScript->m_strOneLine[ind] = mMacros[MAX_MACROS + ind];
  }
}

// This is called on startup or after reading settings
void CMacroProcessor::OpenOrJustCloseOneLiners(bool reopen)
{
  if (reopen && !mOneLineScript) {
    OnScriptRunOneCommand();
  } else if (!reopen && mOneLineScript) {
    mOneLineScript->DestroyWindow();
    mOneLineScript = NULL;
  }
}

// Central place to determine if a macro is theoretically runnable
BOOL CMacroProcessor::MacroRunnable(int index)
{
  return !mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() &&
    !(mWinApp->mNavigator && mWinApp->mNavigator->StartedMacro()) &&
    !(mWinApp->mNavigator && mWinApp->mNavigator->GetPausedAcquire()) &&
    !mWinApp->NavigatorStartedTS() && (index >= MAX_MACROS || !mMacros[index].IsEmpty() || 
    mMacroEditer[index]);
}

void CMacroProcessor::OnMacroEnd()
{
  if (DoingMacro())
    Stop(false, true);
  else
    SetNonResumable();
}

void CMacroProcessor::OnUpdateMacroEnd(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(DoingMacro() ||
    (mWinApp->mNavigator && mWinApp->mNavigator->StartedMacro() && IsResumable()));
}

void CMacroProcessor::OnMacroStop()
{
  Stop(true, true);
}

// Allow stop through the menu if there is a macro-driven non Navigator tilt series
void CMacroProcessor::OnUpdateMacroStop(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(DoingMacro());
}

void CMacroProcessor::OnMacroResume()
{
  Resume();
}

void CMacroProcessor::OnUpdateMacroResume(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() &&
    !mWinApp->NavigatorStartedTS() && IsResumable());
}

void CMacroProcessor::OnMacroControls()
{
  CMacroControlDlg conDlg;
  conDlg.mControl = *mControl;
  if (conDlg.DoModal() == IDOK)
    *mControl = conDlg.mControl;
}

// TASK HANDLERS AND ANCILLARY FUNCTIONS

// Check for conditions that macro may have started
int CMacroProcessor::TaskBusy()
{
  double diff, dose;
  CString report;

  // If accumulating dose: do periodic reports, stop when done
  if (mDoingMacro && mDoseTarget > 0.) {
    dose = mWinApp->mScopeStatus.GetFullCumDose() - mDoseStart;

    if (dose < mDoseTarget) {
      if (dose >= mDoseNextReport) {
        diff = SEMTickInterval(mDoseTime);
        diff = (mDoseTarget - dose) * (diff / dose) / 60000.;
        report.Format("Dose so far %.1f electrons/A2, time remaining ~%.1f minutes",
          dose, diff);
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
        mDoseNextReport += mDoseTarget / mNumDoseReports;
      }
      Sleep(10);
      return 1;
    } else {
      mDoseTarget = 0.;
      report.Format("Accumulated dose = %.1f electrons/A2", dose);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      mWinApp->SetStatusText(SIMPLE_PANE, "");
      mWinApp->mScopeStatus.SetWatchDose(false);
      return 0;
    }
  }

  // If sleeping, take little naps to avoid using lots of CPU
  if (mDoingMacro && mSleepTime > 0.) {
    diff = SEMTickInterval(mSleepStart);
    if (diff < mSleepTime) {
      if (mSleepTime - diff > 20.)
        Sleep(10);
      return 1;
    }
    mWinApp->SetStatusText(SIMPLE_PANE, "");
    mSleepTime = 0.;
  }

  // If ran a process, check if it is still running; if not, set the output values and
  // fall through to test other conditions
  if (mProcessThread) {
    if (UtilThreadBusy(&mProcessThread) > 0)
      return 1;
    SetReportedValues(sProcessExitStatus, sProcessErrno);
    report.Format("Shell command exited with status %d, error code %d",
      sProcessExitStatus, sProcessErrno);
    mWinApp->AppendToLog(report, mLogAction);
  }

  return ((mScope->GetInitialized() && ((mMovedStage &&
    mScope->StageBusy() > 0) || (mExposedFilm && mScope->FilmBusy() > 0) ||
    (mMovedScreen && mScope->ScreenBusy()))) ||
    (mStartedLongOp && mScope->LongOperationBusy()) ||
    (mMovedPiezo && mWinApp->mPiezoControl->PiezoBusy() > 0) ||
    (mMovedAperture && mScope->GetMovingAperture()) ||
    (mLoadingMap && mWinApp->mNavigator && mWinApp->mNavigator->GetLoadingMap()) ||
    (mMakingDualMap && mWinApp->mNavHelper->GetAcquiringDual()) ||
    mWinApp->mShiftCalibrator->CalibratingIS() ||
    (mCamera->GetInitialized() && mCamera->CameraBusy() && 
    (mCamera->GetTaskWaitingForFrame() || 
    !(mUsingContinuous && mCamera->DoingContinuousAcquire()))) ||
    mWinApp->mMontageController->DoingMontage() || 
    mWinApp->mParticleTasks->DoingMultiShot() || 
    mWinApp->mParticleTasks->GetWaitingForDrift() ||
    mWinApp->mFocusManager->DoingFocus() || mWinApp->mAutoTuning->DoingAutoTune() ||
    mShiftManager->ResettingIS() || mWinApp->mCalibTiming->Calibrating() ||
    mWinApp->mFilterTasks->RefiningZLP() ||
    (mWinApp->mNavHelper && mWinApp->mNavHelper->GetRealigning()) ||
    mWinApp->mComplexTasks->DoingTasks() || mWinApp->DoingRegisteredPlugCall()) ? 1 : 0;
}

// When a task is done, if the macro flag is still set, call next command
void CMacroProcessor::TaskDone(int param)
{
  if (DoingMacro())
    NextCommand();
  else
    SuspendMacro();
}

// To run a macro: set the current macro with index at start
void CMacroProcessor::Run(int which)
{
  int mac, ind;
  MacroFunction *func;
  if (mMacros[which].IsEmpty())
    return;

  // Check the macro(s) for commands, number of arguments, etc.
  PrepareForMacroChecking(which);
  mLastAborted = true;
  mLastCompleted = false;
  if (CheckBlockNesting(which, -1))
    return;

  // Clear out wasCalled flags
  for (mac = 0; mac < MAX_TOT_MACROS; mac++) {
    for (ind = 0; ind < mFuncArray[mac].GetSize(); ind++) {
      func = mFuncArray[mac].GetAt(ind);
      func->wasCalled = false;
    }
    if (mac >= MAX_MACROS)
      mMacNames[mac] = "";
  }
  mCurrentMacro = which;
  mBlockLevel = -1;
  mBlockDepths[0] = -1;
  mCallFunction[0] = NULL;
  mCurrentIndex = 0;
  mLastIndex = -1;
  mOnStopMacroIndex = -1;
  mExitAtFuncEnd = false;
  mLoopIndsAreLocal = false;
  mStartNavAcqAtEnd = false;
  mConsetNums.clear();
  mConsetsSaved.clear();
  mChangedConsets.clear();
  mSavedSettingNames.clear();
  mSavedSettingValues.clear();
  mNewSettingValues.clear();
  mLDareasSaved.clear();
  mLDParamsSaved.clear();
  for (mac = 0; mac < MAX_LOWDOSE_SETS; mac++)
    mKeepOrRestoreArea[mac] = 0;
  ClearVariables();
  mUsingContinuous = false;
  mShowedScopeBox = false;
  mRetryReadOther = 0;
  mEmailOnError = "";
  mNoMessageBoxOnError = false;
  mSkipFrameAliCheck = false;
  mAlignWholeTSOnly = false;
  mDisableAlignTrim = false;
  mBoxOnScopeText = "SerialEM message";
  mBoxOnScopeType = 0;
  mBoxOnScopeInterval = 0.;
  mNumRuns = 0;
  mTestNextMultiShot = 0;
  mAccumShiftX = 0.;
  mAccumShiftY = 0.;
  mAccumDiff = 0.;
  mMinDeltaFocus = 0;
  mMaxDeltaFocus = 0.;
  mMinAbsFocus = 0.;
  mMaxAbsFocus = 0.;
  mNumTempMacros = 0;
  mLogAction = LOG_OPEN_IF_CLOSED;
  mLogErrAction = LOG_IGNORE;
  mStartClock = GetTickCount();
  mOverwriteOK = false;
  mVerbose = mInitialVerbose ? 1 : 0;
  CloseFileForText(-1);
  RunOrResume();
}

// Do all the common initializations for running or restarting a macro
void CMacroProcessor::RunOrResume()
{
  int ind;
  mDoingMacro = true;
  mOpenDE12Cover = false;
  mStopAtEnd = false;
  mAskRedoOnResume = false;
  mTestScale = false;
  mTestMontError = false;
  mTestTiltAngle = false;
  mLastCompleted = false;
  mLastAborted = false;
  mLastTestResult = true;
  mCamera->SetTaskWaitingForFrame(false);
  mFrameWaitStart = -1.;
  mNumStatesToRestore = 0;
  mFocusToRestore = -999.;
  mFocusOffsetToRestore = -9999.;
  mDEframeRateToRestore = -1.;
  mDEcamIndToRestore = -1;
  mLDSetAddedBeamRestore = -1;
  mK3CDSmodeToRestore = -1;
  mNextProcessArgs = "";
  mBeamTiltXtoRestore[0] = mBeamTiltXtoRestore[1] = EXTRA_NO_VALUE;
  mBeamTiltYtoRestore[0] = mBeamTiltXtoRestore[1] = EXTRA_NO_VALUE;
  mCompensatedBTforIS = false;
  mKeyPressed = 0;
  if (mChangedConsets.size() > 0 && mCamWithChangedSets == mWinApp->GetCurrentCamera())
    for (ind = 0; ind < (int)B3DMIN(mConsetNums.size(), mChangedConsets.size());ind++)
      mConSets[mConsetNums[ind]] = mChangedConsets[ind];
  for (ind = 0 ; ind < (int)mSavedSettingNames.size(); ind++)
    mWinApp->mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()), 
      mNewSettingValues[ind]);
  mStoppedContSetNum = mCamera->DoingContinuousAcquire() - 1;
  mWinApp->UpdateBufferWindows();
  SetComplexPane();
  if (mStoppedContSetNum >= 0) {
    mCamera->StopCapture(0);
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return;
  }
  NextCommand();
}

// Set the macro name or message in complex pane
void CMacroProcessor::SetComplexPane(void)
{
  if (mMacNames[mCurrentMacro].IsEmpty())
    mMacroPaneText.Format("DOING SCRIPT %d", mCurrentMacro + 1);
  else
    mMacroPaneText.Format("DOING %d: %s", mCurrentMacro + 1, mMacNames[mCurrentMacro]);
  mWinApp->SetStatusText(mWinApp->DoingTiltSeries() &&
    mWinApp->mTSController->GetRunningMacro() ? MEDIUM_PANE : COMPLEX_PANE,
    mMacroPaneText);
}

// A macro is resumable if there is a current one still, and its index is
// not at the end
BOOL CMacroProcessor::IsResumable()
{
  if (mDoingMacro || mCurrentMacro < 0)
    return false;
  return (mCurrentIndex < mMacros[mCurrentMacro].GetLength());
}

// Resume by setting flag and doing next command or repeating last
void CMacroProcessor::Resume()
{
  int lastind = mLastIndex;
  CString stopCom;
  if (!IsResumable())
    return;

  // If there was an error or incomplete action, let user choose whether to redo
  if (mAskRedoOnResume) {
    GetNextLine(&mMacros[mCurrentMacro], lastind, stopCom);
    if (AfxMessageBox("The script stopped on the following line:\n\n" + stopCom +
      "\nDo you want to repeat this command or not?",
      MB_YESNO | MB_ICONQUESTION) == IDYES)
      mCurrentIndex = mLastIndex;
  }
  RunOrResume();
}

// Stop either now or at an ending place
void CMacroProcessor::Stop(BOOL ifNow, BOOL fromTSC)
{
  if (ifNow) {
    if (TestAndStartFuncOnStop())
      return;
    if (mProcessThread && UtilThreadBusy(&mProcessThread) > 0)
      UtilThreadCleanup(&mProcessThread);
    if (mDoingMacro && mLastIndex >= 0)
      mAskRedoOnResume = true;
    SuspendMacro();
  } else
    mStopAtEnd = true;
}

// If there is a stop function registered, set up to run it
int CMacroProcessor::TestAndStartFuncOnStop(void)
{
  if (mOnStopMacroIndex >= 0) {
    mCurrentMacro = mOnStopMacroIndex;
    mCurrentIndex = mOnStopLineIndex;
    mOnStopMacroIndex = -1;
    mExitAtFuncEnd = true;
    mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
    return 1;
  }
  return 0;
}


///////////////////////////////////////////////////////
// THE MAIN MACRO PROCESSING FUNCTION

// Do the next command in a macro
void CMacroProcessor::NextCommand()
{
  CString strLine, strCopy, item1upper;
  CString strItems[MAX_TOKENS];
  CString report;
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  BOOL truth, doShift, keyBreak, doPause, doAbort;
  bool doBack;
  ScaleMat aMat, bInv;
  EMimageBuffer *imBuf;
  KImage *image;
  CFile *cfile;
  double delISX, delISY, delX, delY, specDist, h1, v1, v2, h2, h3, v3, v4, h4;
  double stageX, stageY, stageZ;
  int cmdIndex, index, index2, i, ix0, ix1, iy0, iy1, sizeX, sizeY, mag, lastNonEmptyInd;
  float backlashX, backlashY, bmin, bmax, bmean, bSD, cpe, shiftX, shiftY, fitErr;
  FilterParams *filtParam = mWinApp->GetFilterParams();
  int *activeList = mWinApp->GetActiveCameraList();
  CameraParameters *camParams = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  MontParam *montP = mWinApp->GetMontParam();
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  CMapDrawItem *navItem;
  CNavigatorDlg *navigator = mWinApp->mNavigator;
  CNavHelper *navHelper = mWinApp->mNavHelper;
  MacroFunction *func;
  Variable *var;
  CString *valPtr;
  int *numElemPtr;
  CFileStatus status;
  FileForText *txFile;
  StageMoveInfo smi;
  int readOtherSleep = 2000;

  // Be sure to add an entry for longHasTime when adding long operation
  const char *longKeys[MAX_LONG_OPERATIONS] = {"BU", "RE", "IN", "LO", "$=", "DA", "UN", 
    "$=", "RS", "RT", "FF"};
  int longHasTime[MAX_LONG_OPERATIONS] = {1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};
  mSleepTime = 0.;
  mDoseTarget = 0.;
  mMovedStage = false;
  mMovedScreen = false;
  mExposedFilm = false;
  mStartedLongOp = false;
  mMovedAperture = false;
  mLoadingMap = false;

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

  // Stopping conditions that are suspensions - but backing up is a bad idea
  if (mTestMontError && mWinApp->Montaging() && mControl->limitMontError &&
    mLastMontError > mControl->montErrorLimit) {
    mWinApp->AppendToLog("Script suspended because montage error specified\r\n"
      " in Script Control dialog exceeded limit", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return;
  }

  index = mTestMontError ? 1 : 0;
  if (mControl->limitScaleMax && mImBufs[index].mImage && mImBufs[index].mImageScale &&
    mTestScale && mImBufs[index].mImageScale->GetMaxScale() < mControl->scaleMaxLimit) {
    mWinApp->AppendToLog("Script suspended because image intensity fell below limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
    SuspendMacro();
    return;
  }

  if (mControl->limitIS) {
    double ISX, ISY, specX, specY;
    ScaleMat cMat;
    mScope->GetLDCenteredShift(ISX, ISY);

    cMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    specX = cMat.xpx * ISX + cMat.xpy * ISY;
    specY = cMat.ypx * ISX + cMat.ypy * ISY;
    if (cMat.xpx != 0.0 && sqrt(specX * specX + specY * specY) > mControl->ISlimit) {
      mWinApp->AppendToLog("Script suspended because image shift exceeded limit "
      "specified\r\n in Script Control dialog ", LOG_MESSAGE_IF_CLOSED);
      SuspendMacro();
      return;
    }
  }

  if (mShowedScopeBox)
    SetReportedValues(1., mScope->GetMessageBoxReturnValue());
  mShowedScopeBox = false;

  // Save the current index
  mLastIndex = mCurrentIndex;

  // Find the next real command
  CString *macro = &mMacros[mCurrentMacro];
  strItems[0] = "";
  while (strItems[0].IsEmpty() && mCurrentIndex < macro->GetLength()) {

    GetNextLine(macro, mCurrentIndex, strLine);
    if (mVerbose > 0)
      mWinApp->AppendToLog("COMMAND: " + strLine, LOG_OPEN_IF_CLOSED);

    strCopy = strLine;

    // Parse the line
    if (mWinApp->mParamIO->ParseString(strCopy, strItems, MAX_TOKENS))
      ABORT_LINE("Too many items on line in script: \n\n");
    strItems[0].MakeUpper();
  }

  // Convert a single number to a DOMACRO (obsolete and bad!)
  InsertDomacro(&strItems[0]);

  // Substitute variables in parsed items and check for control word substitution
  report = strItems[0];
  if (SubstituteVariables(strItems, MAX_TOKENS, strLine)) {
    AbortMacro();
    return;
  }
  strItems[0].MakeUpper();
  item1upper = strItems[1];
  item1upper.MakeUpper();
  if (report != strItems[0] && WordIsReserved(strItems[0]))
    ABORT_LINE("You cannot make a command by substituting a\n"
      "variable value that is a control command on line: \n\n");

  cmdIndex = LookupCommandIndex(strItems[0]);

  // Do arithmetic on selected commands
  if (cmdIndex >= 0 && ArithmeticIsAllowed(strItems[0])) {
    if (SeparateParentheses(&strItems[1], MAX_TOKENS - 1))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    EvaluateExpression(&strItems[1], MAX_TOKENS - 1, strLine, 0, index, index2);
    for (i = index + 1; i <= index2; i++)
      strItems[i] = "";
    index = CheckLegalCommandAndArgNum(&strItems[0], strLine, mCurrentMacro);
    if (index) {
      if (index == 1)
        ABORT_LINE("There is no longer a legal command after evaluating arithmetic on "
          "this line:\n\n");
      AbortMacro();
      return;
    }
  }

  // Evaluate emptiness, ints and doubles
  for (i = 0; i < MAX_TOKENS; i++) {
    itemEmpty[i] = strItems[i].IsEmpty();
    if (!itemEmpty[i]) {
      itemInt[i] = atoi((LPCTSTR)strItems[i]);
      itemDbl[i] = atof((LPCTSTR)strItems[i]);
      lastNonEmptyInd = i;
    }
  }
  if (itemEmpty[0]) {
    cmdIndex = CME_SCRIPTEND;
  } else {
    if (strItems[0].GetAt(strItems[0].GetLength() - 1) == ':')
      cmdIndex = CME_LABEL;
    else if (strItems[1] == "@=" || strItems[1] == ":@=")
      cmdIndex = CME_SETSTRINGVAR;
    else if (strItems[1] == "=" || strItems[1] == ":=")
      cmdIndex = CME_SETVARIABLE;
  }
  if (CMD_IS(ELSEIF) && mBlockLevel >= 0 && mLoopLimit[mBlockLevel] == LOOP_LIMIT_FOR_IF)
    cmdIndex = CME_ZEROLOOPELSEIF;
    
  // See if we are supposed to stop at an ending place
  if (mStopAtEnd && (CMD_IS(REPEAT) || CMD_IS(ENDLOOP) || CMD_IS(DOMACRO) || 
    CMD_IS(DOSCRIPT))) {
    if (mLastIndex >= 0)
      mCurrentIndex = mLastIndex;
    SuspendMacro();   // Leave it resumable
    return;
  }

  keyBreak = CMD_IS(KEYBREAK) && ((itemEmpty[1] && mKeyPressed == 'B') ||
    (strItems[1].GetLength() == 1 && item1upper.GetAt(0) == mKeyPressed));
  if (keyBreak)
    cmdIndex = CME_DOKEYBREAK;

  // THE MASSIVE SWITCH ON COMMAND INDEXES
  switch (cmdIndex) {

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
    if (CMD_IS(RETURN)) {
      while (mBlockDepths[mCallLevel] >= 0) {
        ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
        mBlockLevel--;
        mBlockDepths[mCallLevel]--;
      }
    }
    ClearVariables(VARTYPE_LOCAL, mCallLevel);
    if (mCallFunction[mCallLevel])
      mCallFunction[mCallLevel]->wasCalled = false;
    mCallLevel--;
    mCurrentMacro = mCallMacro[mCallLevel];
    mCurrentIndex = mCallIndex[mCallLevel];
    mLastIndex = -1;
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
    if (mBlockDepths[mCallLevel] < 0 || mLoopLimit[mBlockLevel] < LOOP_LIMIT_FOR_IF) {
      AbortMacro();
      AfxMessageBox("The script contains an ENDLOOP without a LOOP or DOLOOP statement",
        MB_EXCLAME);
      return;
    }

    // If count is not past limit, go back to start;
    // otherwise clear index variable and decrease level indexes
    mLoopCount[mBlockLevel] += mLoopIncrement[mBlockLevel];
    if ((mLoopIncrement[mBlockLevel] < 0 ? -1 : 1) * 
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
      mLoopLimit[mBlockLevel] = B3DMAX(0, B3DNINT(itemDbl[1]));
      mLoopCount[mBlockLevel] = 1;
      index = 2;
    } else {
      mLoopCount[mBlockLevel] = itemInt[2];
      mLoopLimit[mBlockLevel] = itemInt[3];
      if (!itemEmpty[4]) {
        mLoopIncrement[mBlockLevel] = itemInt[4];
        if (!itemInt[4])
          ABORT_LINE("The loop increment is 0 at line:\n\n");
        index = 1;
      }
    }
    mLastIndex = -1;
    if ((mLoopIncrement[mBlockLevel] < 0 ? -1 : 1) *
      (mLoopCount[mBlockLevel] - mLoopLimit[mBlockLevel]) > 0) {
      if (SkipToBlockEnd(SKIPTO_ENDLOOP, strLine)) {
        AbortMacro();
        return;
      }
    } else if (!itemEmpty[index]) {
      if (SetVariable(strItems[index], 1.0, VARTYPE_INDEX, mBlockLevel, true, &report))
        ABORT_LINE(report + " in script line: \n\n");
    }
    break;
    
  case CME_IF:
  case CME_ZEROLOOPELSEIF:                                  // If, Elseif

    // IF statement: evaluate it, go up one block level with 0 limit
    if (EvaluateIfStatement(&strItems[1], MAX_TOKENS - 1, strLine, truth)) {
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
    if (!truth) {
      if (SkipToBlockEnd(SKIPTO_ELSE_ENDIF, strLine) ){
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
    if (SkipToBlockEnd(SKIPTO_ENDIF, strLine)) {
      AbortMacro();
      return;
    }
    mLastIndex = -1;
    break;

  case CME_BREAK:
  case CME_CONTINUE:
  case CME_DOKEYBREAK:    // Break, Continue
     
    if (SkipToBlockEnd(SKIPTO_ENDLOOP, strLine)) {
      AbortMacro();
      return;
    }
    mLastIndex = -1;

    // Pop any IFs on the loop stack
    while (mBlockLevel >= 0 && mLoopLimit[mBlockLevel] <= LOOP_LIMIT_FOR_IF) {
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
    if (mBlockLevel >= 0 && (CMD_IS(BREAK) || keyBreak))
      mLoopCount[mBlockLevel] = mLoopLimit[mBlockLevel];
    if (keyBreak) {
      PrintfToLog("Broke out of loop after %c key pressed", (char)mKeyPressed);
      mKeyPressed = 0;
    }
    break;

  case CME_SKIPTO:                                          // SkipTo
    if (SkipToLabel(item1upper, strLine, index)) {
      AbortMacro();
      return;
    }

    // Pop proper number of ifs and loops from stack
    for (i = 0; i < index && mBlockLevel >= 0; i++) {
      ClearVariables(VARTYPE_INDEX, mCallLevel, mBlockLevel);
      mBlockLevel--;
      mBlockDepths[mCallLevel]--;
    }
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
      index2 = 0;
      func = NULL;
      if (CMD_IS(CALLFUNCTION)) {
        func = FindCalledFunction(strLine, false, index, ix0);
        if (!func)
          AbortMacro();
        index2 = func->startIndex;
        if (func->wasCalled)
          ABORT_LINE("Trying to call a function already being called in line: \n\n");
        func->wasCalled = true;
      } else if (CMD_IS(CALL)) {
        index = FindCalledMacro(strLine, false);
      } else if (CMD_IS(CALLSTRINGARRAY)) {
        index = MakeNewTempMacro(strItems[1], strItems[2], true, strLine);
        if (!index)
          return;
        if (CheckBlockNesting(index, -1)) {
          AbortMacro();
          return;
        }

      } else {
        index = itemInt[1] - 1;
      }

      // Save the current index at this level and move up a level
      // If doing a function with string arg, substitute in line for that first so the
      // pre-existing level applies
      if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
        if (mCallLevel >= MAX_CALL_DEPTH)
          ABORT_LINE("Trying to call too many levels of scripts/functions in line: \n\n");
        if (func && func->ifStringArg)
          SubstituteVariables(&strLine, 1, strLine);
        mCallIndex[mCallLevel++] = mCurrentIndex;
        mCallMacro[mCallLevel] = index;
        mBlockDepths[mCallLevel] = -1;
        mCallFunction[mCallLevel] = func;
      } else {
        mNumRuns++;
      }

      // Doing another macro (or this one over), just reset macro # and index
      mCurrentMacro = index;
      mCurrentIndex = index2;
      mLastIndex = -1;

      // Create argument variables now that level is raised
      if (func && (func->numNumericArgs || func->ifStringArg)) {
        truth = false;
        for (index = 0; index < func->numNumericArgs; index++) {
          report = func->argNames[index];
          if (SetVariable(func->argNames[index], itemEmpty[index + ix0] ? 
            0. : itemDbl[index + ix0], VARTYPE_LOCAL, -1, false, &report)) {
              truth = true;
              break;
          }
        }
        if (!truth && func->ifStringArg) {
          mWinApp->mParamIO->StripItems(strLine, index + ix0, strCopy);
          truth = SetVariable(func->argNames[index], strCopy, VARTYPE_LOCAL, 
            -1, false, &report);
        }
        if (truth)
          ABORT_LINE("Failed to set argument variables in function call:\r\n" + report +
          " in line:\n\n")
        for (index = 0; index < func->numNumericArgs + (func->ifStringArg ? 1 : 0);
          index++)
            if (itemEmpty[index + ix0])
              break;
        SetVariable("NUMCALLARGS", index, VARTYPE_LOCAL, -1, false);
     }
    }
    break;

  case CME_STRINGARRAYTOSCRIPT:                             // StringArrayToScript
    index = MakeNewTempMacro(strItems[1], strItems[2], false, strLine);
    if (!index)
      return;
    break;

  case CME_ONSTOPCALLFUNC:                                  // OnStopCallFunc
    if (!mExitAtFuncEnd) {
      func = FindCalledFunction(strLine, false, mOnStopMacroIndex, ix0);
      if (!func) {
        AbortMacro();
        return;
      }
      mOnStopLineIndex = func->startIndex;
      if (func->numNumericArgs || func->ifStringArg)
        ABORT_LINE("The function to call takes arguments, which is not allowed here:"
        "\n\n");
    }
    break;
    
  case CME_NOMESSAGEBOXONERROR:                             // NoMessageBoxOnError
    mNoMessageBoxOnError = true;
    if (!itemEmpty[1])
      mNoMessageBoxOnError = itemInt[1] != 0;
    break;
    
  case CME_KEYBREAK:
    break;
    
  case CME_TEST:                                            // Test
    if (itemEmpty[2])
      mLastTestResult = itemDbl[1] != 0.;
    else
      EvaluateIfStatement(&strItems[1], MAX_TOKENS - 1, strLine, mLastTestResult);
    SetReportedValues(mLastTestResult ? 1. : 0.);
    SetVariable("TESTRESULT", mLastTestResult ? 1. : 0., VARTYPE_REGULAR, -1, false);
    break;
    
  case CME_SETVARIABLE:                                     // Variable assignment
  case CME_APPENDTOARRAY:                                   // AppendToArray

    // Do assignment to variable before any non-reserved commands
    index2 = 2;
    ix0 = CheckForArrayAssignment(strItems, index2);
    if (SeparateParentheses(&strItems[index2], MAX_TOKENS - index2))
      ABORT_LINE("Too many items on line after separating out parentheses in line: \n\n");
    if (EvaluateExpression(&strItems[index2], MAX_TOKENS - index2, strLine, ix0, index, 
      ix1)) {
        AbortMacro();
        return;
    }

    // Concatenate array elements separated by \n
    if (ix0 > 0) {
      for (ix1 = index2 + 1; ix1 < MAX_TOKENS; ix1++) {
        if (strItems[ix1].IsEmpty())
          break;
        strItems[index2] += "\n" + strItems[ix1];
      }
    }

    // Set the variable
    if (CMD_IS(SETVARIABLE)) {
      index = strItems[1] == "=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
      if (SetVariable(strItems[0], strItems[index2], index, -1, false, &report))
        ABORT_LINE(report + " in script line: \n\n");

    } else {

      // Append to array: Split off an index to look it up
      ArrayRow newRow;

      var = GetVariableValuePointers(item1upper, &valPtr, &numElemPtr, "append to", 
        report);
      if (!var)
        ABORT_LINE(report + " for line:\n\n");
      truth = valPtr == NULL;
      if (truth) {
        valPtr = &newRow.value;
        numElemPtr = &newRow.numElements;
      }

      // Add the string to the value and compute the number of elements
      if (!valPtr->IsEmpty())
        *valPtr += "\n";
      *valPtr += strItems[index2];
      *numElemPtr = 1;
      ix0 = -1;
      while ((ix0 = valPtr->Find('\n', ix0 + 1)) >= 0)
        *numElemPtr += 1;

      // Both 1D array and existing row of 2D array should be done: add new row to 2D
      if (truth)
        var->rowsFor2d->Add(newRow);
    }
    break;

  case CME_TRUNCATEARRAY:                                   // TruncateArray
    index = itemInt[2];
    if (index < 1)
      ABORT_LINE("The number to truncate to must be at least 1 for line:\n\n");
    var = GetVariableValuePointers(item1upper, &valPtr, &numElemPtr, "truncate", report);
    if (!var)
      ABORT_LINE(report + " for line:\n\n");
    if (!valPtr) {
      if (var->rowsFor2d->GetSize() > index)
        var->rowsFor2d->SetSize(index);
    } else if (*numElemPtr > index) {
      FindValueAtIndex(*valPtr, index, ix0, ix1);
      *valPtr = valPtr->Left(ix1);
      *numElemPtr = index;
    }
    break;

  case CME_ARRAYSTATISTICS:                                 // ArrayStatistics
  {
    float *fvalues = FloatArrayFromVariable(item1upper, index2, report);
    if (!fvalues)
      ABORT_LINE(report + " in line\n\n");
    bmin = 1.e37f;
    bmax = -1.e37f;
    for (index = 0; index < index2; index++) {
      ACCUM_MIN(bmin, fvalues[index]);
      ACCUM_MAX(bmax, fvalues[index]);
    }
    avgSD(fvalues, index2, &bmean, &bSD, &cpe);
    rsFastMedianInPlace(fvalues, index2, &cpe);
    report.Format("n= %d  min= %.6g  max = %.6g  mean= %.6g  sd= %.6g  median= %.6g",
      index2, bmin, bmax, bmean, bSD, cpe);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], index2, bmin, bmax, bmean, bSD, cpe);

    delete[] fvalues;
    break;
  }

  case CME_LINEARFITTOVARS:                                 // LinearFitToVars
  {
    float *xx1, *xx2, *xx3 = NULL;
    truth = !itemEmpty[3];
    xx1 = FloatArrayFromVariable(item1upper, index, report);
    if (!xx1)
      ABORT_LINE(report + " in line\n\n");
    xx2 = FloatArrayFromVariable(strItems[2], index2, report);
    if (!xx2) {
      delete[] xx1;
      ABORT_LINE(report + " in line\n\n");
    }
    if (truth) {
      xx3 = FloatArrayFromVariable(strItems[3], ix0, report);
      if (!xx3) {
        delete[] xx1, xx2;
        ABORT_LINE(report + " in line\n\n");
      }
    }
    if (index != index2 || (truth && ix0 != index)) {
      delete[] xx1, xx2, xx3;
      ABORT_LINE("Variables do not have the same number of elements in line:\n\n");
    }
    if (index < 2 || (truth && index < 3)) {
      delete[] xx1, xx2, xx3;
      ABORT_LINE("There are not enough array values for a linear fit in line:\n\n");
    }
    if (truth) {
      lsFit2(xx1, xx2, xx3, index, &bmean, &bSD, &cpe);
      report.Format("n= %d  a1= %f  a2= %f  c= %f", index, bmean, bSD, cpe);
    } else {
      lsFit(xx1, xx2, index, &bmean, &bSD, &cpe);
      report.Format("n= %d  slope= %f  intercept= %f  ro= %.4f", index, bmean, bSD, cpe);
    }
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index, bmean, bSD, cpe);
    delete[] xx1, xx2, xx3;
    break;
  }

  case CME_SETSTRINGVAR:                                    // String Assignment
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
    index = strItems[1] == "@=" ? VARTYPE_REGULAR : VARTYPE_PERSIST;
    if (SetVariable(strItems[0], strCopy, index, -1, false, &report))
      ABORT_LINE(report + " in script line: \n\n");
    break;

  case CME_CLEARPERSISTENTVARS:                             // ClearPersistentVars
    ClearVariables(VARTYPE_PERSIST);
    break;

  case CME_MAKEVARPERSISTENT:                               // MakeVarPersistent
    var = LookupVariable(strItems[1], ix0);
    if (!var)
      ABORT_LINE("The variable " + strItems[1] + " is not defined in line:\n\n");
    index = 1;
    if (!itemEmpty[2] && !itemInt[2])
      index = 0;
    if ((index && var->type != VARTYPE_REGULAR) ||
      (!index && var->type != VARTYPE_PERSIST))
      ABORT_LINE("The variable " + strItems[1] + " must be " + 
        CString(index ? "regular" : "persistent") + " to change its type in line:\n\n");
    var->type = index ? VARTYPE_PERSIST : VARTYPE_REGULAR;
    break;
    
  case CME_ISVARIABLEDEFINED:                               // IsVariableDefined
    index = B3DCHOICE(LookupVariable(item1upper, index2) != NULL, 1, 0);
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS);
    report.Format("Variable %s %s defined", (LPCTSTR)strItems[1],
      B3DCHOICE(index, "IS", "is NOT"));
    SetReportedValues(&strItems[2], index);
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_NEWARRAY:                                        // NewArray
  case CME_NEW2DARRAY:                                      // New2DArray
  {
    CArray<ArrayRow, ArrayRow> *rowsFor2d = NULL;
    ArrayRow arrRow;

    // Process local vesus persistent property
    ix0 = VARTYPE_REGULAR;
    if (!itemEmpty[2] && itemInt[2] < 0) {
      ix0 = VARTYPE_LOCAL;
      if (LocalVarAlreadyDefined(strItems[1], strLine) > 0)
        return;
    }
    if (!itemEmpty[2] && itemInt[2] > 0)
      ix0 = VARTYPE_PERSIST;

    // Get number of elements: 3 for 1D and 4 for 2D 
    truth = CMD_IS(NEW2DARRAY);
    index = !itemEmpty[3] ? itemInt[3] : 0;
    if (truth) {
      index2 = !itemEmpty[3] ? itemInt[3] : 0;
      index = !itemEmpty[4] ? itemInt[4] : 0;
      if (index > 0 && !index2)
        ABORT_LINE("The number of elements per row must be 0 because no rows are created"
        " in:\n\n");
    }
    if (index <= 0 || index2 < 0)
      ABORT_LINE("The number of elements to create must be positive in:\n\n");
    strCopy = "0";
    for (ix1 = 1; ix1 < index; ix1++)
      strCopy += "\n0";
    
    // Create the 2D array rows and add string to the rows
    if (truth) {
      rowsFor2d = new CArray<ArrayRow, ArrayRow>;
      arrRow.value = strCopy;
      arrRow.numElements = index;
      for (iy0 = 0; iy0 < index2; iy0++)
        rowsFor2d->Add(arrRow);
    }
    if (SetVariable(item1upper, truth ? "0" : strCopy, ix0, -1, false, &report,
      rowsFor2d)) {
      delete rowsFor2d;
      ABORT_LINE(report + " in script line: \n\n");
    }
    break;
  }

  case CME_LOCALVAR:                                        // LocalVar
    for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++) {
      index2 = LocalVarAlreadyDefined(strItems[index], strLine);
      if (index2 > 0)
        return;
      if (!index2 && SetVariable(strItems[index], "0", VARTYPE_LOCAL, mCurrentMacro, true,
        &report))
        ABORT_LINE(report + " in script line: \n\n");
    }
    break;
    
  case CME_LOCALLOOPINDEXES:                                // LocalLoopIndexes
    if (!itemEmpty[1])
      ABORT_NOLINE("LocalLoopIndexes does not take any values; it cannot be turned off");
    if (mBlockLevel > 0)
      ABORT_NOLINE("LocalLoopIndexes cannot be used inside of a loop");
    mLoopIndsAreLocal = true;
    break;
    
  case CME_PLUGIN:                                          // Plugin
    SubstituteVariables(&strLine, 1, strLine);
    delX = mWinApp->mPluginManager->ExecuteCommand(strLine, itemInt, itemDbl, itemEmpty,
      report, delY, delISX, delISY, index2, index);
    if (index) {
      AbortMacro();
      return;
    }
    strCopy = "";
    if (index2 == 3) {
      SetReportedValues(delX, delY, delISX, delISY);
      strCopy.Format(" and it returned %6g, %6g, %6g", delY, delISX, delISY);
    } else if (index == 2) {
      SetReportedValues(delX, delY, delISX);
      strCopy.Format(" and it returned %6g, %6g", delY, delISX);
    } else if (index == 1) {
      SetReportedValues(delX, delY);
      strCopy.Format(" and it returned %6g", delY);
    } else
      SetReportedValues(delX);
    report += strCopy;
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_LISTPLUGINCALLS:                                 // ListPluginCalls
    mWinApp->mPluginManager->ListCalls();
    break;
    
  case CME_FLASHDISPLAY:                                    // FlashDisplay
    delX = 0.3;
    index2 = 4;
    if (!itemEmpty[1] && itemInt[1] > 0)
      index2 = itemInt[1];
    if (!itemEmpty[2] && itemDbl[2] >= 0.001)
      delX = itemDbl[2];
    if (delX * index2 > 3)
      ABORT_LINE("Flashing duration is too long in line:\r\n\r\n");
    for (index = 0; index < index2; index++) {
      mWinApp->mMainView->SetFlashNextDisplay(true);
      mWinApp->mMainView->DrawImage();
      Sleep(B3DNINT(1000. * delX));
      mWinApp->mMainView->DrawImage();
      if (index < index2 - 1)
        Sleep(B3DNINT(1000. * delX));
    }
    break;
    
  case CME_U:
  case CME_TILTUP:                                          // TiltUp

    // For tilting, if stage is ready, do the action; otherwise back up the index
    if (mScope->StageBusy() <= 0) {
      SetIntensityFactor(1);
      SetupStageRestoreAfterTilt(&strItems[1], delISX, delISY);
      mScope->TiltUp(delISX, delISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    } else
      mLastIndex = mCurrentIndex;
    break;

  case CME_D:
  case CME_TILTDOWN:                                        // TiltDown
    if (mScope->StageBusy() <= 0) {
      SetIntensityFactor(-1);
      SetupStageRestoreAfterTilt(&strItems[1], delISX, delISY);
      mScope->TiltDown(delISX, delISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    } else
      mLastIndex = mCurrentIndex;
    break;

  case CME_TILTTO:                                          // TiltTo
  case CME_TILTBY:                                          // TiltBy
  {
    double angle = (float)itemDbl[1];
    if (CMD_IS(TILTBY))
      angle += mScope->GetTiltAngle();
    if (angle < -80. || angle > 80.) {
      CString message;
      message.Format("Attempt to tilt too high in script, to %.1f degrees\n\n%s",
        angle, strLine);
      AfxMessageBox(message, MB_EXCLAME);
      AbortMacro();
      return;
    }
    if (mScope->StageBusy() <= 0) {
      SetupStageRestoreAfterTilt(&strItems[2], delISX, delISY);
      mScope->TiltTo(angle, delISX, delISY);
      mMovedStage = true;
      mTestTiltAngle = true;
    }
    else
      mLastIndex = mCurrentIndex;
    break;
  }

  case CME_SETSTAGEBAXIS:                                   // SetStageBAxis
    if (mScope->StageBusy() <= 0) {
      if (!mScope->SetStageBAxis(itemDbl[1])) {
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
    
  case CME_V:
  case CME_VIEW:                                            // View
    mCamera->InitiateCapture(VIEW_CONSET);
    mTestScale = true;
    break;

  case CME_F:
  case CME_FOCUS:                                           // Focus
    mCamera->InitiateCapture(FOCUS_CONSET);
    mTestScale = true;
    break;

  case CME_T:
  case CME_TRIAL:                                           // Trial
    mCamera->InitiateCapture(TRIAL_CONSET);
    mTestScale = true;
    break;

  case CME_R:
  case CME_RECORD:                                          // Record
    mCamera->InitiateCapture(RECORD_CONSET);
    mTestScale = true;
    break;

  case CME_L:
  case CME_PREVIEW:                                         // Preview
    mCamera->InitiateCapture(PREVIEW_CONSET);
    mTestScale = true;
    break;

  case CME_SEARCH:                                          // Search
    mCamera->InitiateCapture(SEARCH_CONSET);
    mTestScale = true;
    break;
    
  case CME_M:
  case CME_MONTAGE:                                         // Montage
  case CME_PRECOOKMONTAGE:                                  // PreCookMontage
    truth = CMD_IS(PRECOOKMONTAGE);
    index = MONT_NOT_TRIAL;
    if (truth)
      index = MONT_TRIAL_PRECOOK;
    else if (!itemEmpty[1] && itemInt[1] != 0)
      index = MONT_TRIAL_IMAGE;
    if (!mWinApp->Montaging())
      ABORT_NOLINE("The script contains a montage statement and \n"
        "montaging is not activated");
    if (mWinApp->mMontageController->StartMontage(index, false, 
      (float)(truth ? itemDbl[1] : 0.), (truth && !itemEmpty[2]) ? itemInt[2] : 0, 
      truth && !itemEmpty[3] && itemInt[3] != 0, 
      (truth && !itemEmpty[4]) ? (float)itemDbl[4] : 0.f))
      AbortMacro();
    mTestMontError = !truth;
    mTestScale = !truth;
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
      index = itemEmpty[1] ? 1 : itemInt[1];
      mWinApp->mFocusManager->AutoFocusStart(index);
    } else if (CMD_IS(OPPOSITETRIAL))
      mCamera->InitiateCapture(2);
    else
      mCamera->InitiateCapture(1);
    break;

  case CME_STEPFOCUSNEXTSHOT:                              // StepFocusNextShot
    delX = 0.;
    delY = 0.;
    if (!itemEmpty[4]) {
      delX = itemDbl[3];
      delY = itemDbl[4];
    }
    if (itemDbl[1] <= 0. || delX < 0. || (delX > 0. && delX < itemDbl[1]) ||
      (!delX && !itemEmpty[3]))
      ABORT_LINE("Negative time, times out of order, or odd number of values in "
      "statement: \n\n");
    mCamera->QueueFocusSteps((float)itemDbl[1], itemDbl[2], (float)delX, delY);
    break;
    
  case CME_SMOOTHFOCUSNEXTSHOT:                            // SmoothFocusNextShot
    if (fabs(itemDbl[1] - itemDbl[2]) < 0.1)
      ABORT_LINE("Focus change must be as least 0.1 micron in statement: \n\n");
    mCamera->QueueFocusSteps(0., itemDbl[1], 0., itemDbl[2]);
    break;
    
  case CME_DEFERSTACKINGNEXTSHOT:                          // DeferStackingNextShot
    if (!IS_BASIC_FALCON2(camParams))
      ABORT_NOLINE("Deferred stacking is available only for Falcon2 with old interface");
    mCamera->SetDeferStackingFrames(true);
    break;
    
  case CME_EARLYRETURNNEXTSHOT:                            // EarlyReturnNextShot
    index = itemInt[1];
    if (index < 0)
      index = 65535;
    if (mCamera->SetNextAsyncSumFrames(index, !itemEmpty[2] || itemInt[2] > 0)) {
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
    mCamera->SetNextFrameSkipThresh((float)itemDbl[1]);
    break;
    
  case CME_QUEUEFRAMETILTSERIES:                           // QueueFrameTiltSeries
  case CME_FRAMESERIESFROMVAR:                             // FrameSeriesFromVar
  {
    FloatVec openTime, tiltToAngle, waitOrInterval, focusChange, deltaISX, deltaISY;

    // Get matrix for image shift conversion
    index = mWinApp->LowDoseMode() ? ldParam[RECORD_CONSET].magIndex : 
      mScope->GetMagIndex();
    aMat = MatMul(MatInv(mShiftManager->CameraToSpecimen(index)), 
      mShiftManager->CameraToIS(index));

    // Set up series based on specifications
    if (CMD_IS(QUEUEFRAMETILTSERIES)) {
      delISX = itemEmpty[4] ? 0. : itemDbl[4];
      for (index = 0; index < itemInt[3]; index++) {
        tiltToAngle.push_back((float)(itemDbl[1] + itemDbl[2] * index));
        if (!itemEmpty[5])
          openTime.push_back((float)itemDbl[5]);
        if (!itemEmpty[6])
          waitOrInterval.push_back((float)itemDbl[6]);
        if (!itemEmpty[7])
          focusChange.push_back((float)(itemDbl[7] * index));
        if (!itemEmpty[9]) {
          deltaISX.push_back((float)((itemDbl[8] * aMat.xpx + itemDbl[9] * aMat.xpy) * 
            index));
          deltaISY.push_back((float)((itemDbl[8] * aMat.ypx + itemDbl[9] * aMat.ypy) * 
            index));
        }
      }
    
    } else {

      // Or pull the values out of the big variable array: figure out how many per step
      delISX = itemEmpty[3] ? 0. : itemDbl[3];
      var = LookupVariable(item1upper, index2);
      if (!var)
        ABORT_LINE("The variable " + strItems[1] + " is not defined in line:\n\n");
      sizeX = 0;
      if (itemInt[2] > 31 || itemInt[2] < 1)
        ABORT_LINE("The entry with flags must be between 1 and 31 in line:\n\n");
      if (itemInt[2] & 1)
        sizeX++;
      if (itemInt[2] & 2)
        sizeX++;
      if (itemInt[2] & 4)
        sizeX++;
      if (itemInt[2] & 8)
        sizeX++;
      if (itemInt[2] & 16)
        sizeX += 2;
      if (var->rowsFor2d) {
        sizeY = (int)var->rowsFor2d->GetSize();
        for (ix0 = 0; ix0 < sizeY; ix0++) {
          ArrayRow& tempRow = var->rowsFor2d->ElementAt(ix0);
          if (tempRow.numElements < sizeX) {
            report.Format("Row %d or the 2D array %s has only %d elements, not the %d"
              " required", ix0, item1upper, tempRow.numElements, sizeX);
            ABORT_NOLINE(report);
          }
        }
      } else {
        sizeY = var->numElements / sizeX;
        valPtr = &var->value;
        if (var->numElements % sizeX) {
          report.Format("Variable %s has %d elements, not divisible by the\n"
            "%d values per step implied by the flags entry of %d", strItems[1],
            var->numElements, sizeX, itemInt[2]);
          ABORT_NOLINE(report);
        }
      }

      // Load the vectors
      iy0 = 1;
      for (ix0 = 0; ix0 < sizeY; ix0++) {
        if (var->rowsFor2d) {
          ArrayRow& tempRow = var->rowsFor2d->ElementAt(ix0);
          valPtr = &tempRow.value;
          iy0 = 1;
        }
        if (itemInt[2] & 1) {
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          tiltToAngle.push_back((float)atof((LPCTSTR)report));
          iy0++;
        }
        if (itemInt[2] & 2) {
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          openTime.push_back((float)atof((LPCTSTR)report));
          iy0++;
        }
        if (itemInt[2] & 4) {
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          waitOrInterval.push_back((float)atof((LPCTSTR)report));
          iy0++;
        }
        if (itemInt[2] & 8) {
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          focusChange.push_back((float)atof((LPCTSTR)report));
          iy0++;
        }
        if (itemInt[2] & 16) {
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          delX = atof((LPCTSTR)report);
          iy0++;
          FindValueAtIndex(*valPtr, iy0, ix1, iy1);
          report = valPtr->Mid(ix1, iy1 - ix1);
          delY = atof((LPCTSTR)report);
          deltaISX.push_back((float)(delX * aMat.xpx + delY * aMat.xpy));
          deltaISY.push_back((float)(delX * aMat.ypx + delY * aMat.ypy));
          iy0++;
        }
      }
    }

    // Queue it: This does all the error checking
    if (mCamera->QueueTiltSeries(openTime, tiltToAngle, waitOrInterval, focusChange, 
      deltaISX, deltaISY, (float)delISX)) {
        AbortMacro();
        return;
    }
    break;
  }

  case CME_SETFRAMESERIESPARAMS:                            // SetFrameSeriesParams
    truth = itemInt[1] != 0;
    backlashX = 0.;
    if (!itemEmpty[2])
      backlashX = (float)itemDbl[2];
    if (!itemEmpty[3] && itemEmpty[4] ||
      !BOOL_EQUIV(fabs(itemDbl[3]) > 9000., fabs(itemDbl[4]) > 9000.))
      ABORT_LINE("A Y position to restore must be entered when X is on line:\n\n");
    if (mCamera->SetFrameTSparams(truth, backlashX,
      (itemEmpty[3] || fabs(itemDbl[3]) > 9000.) ? EXTRA_NO_VALUE : itemDbl[3],
      (itemEmpty[4] || fabs(itemDbl[4]) > 9000.) ? EXTRA_NO_VALUE : itemDbl[4]))
      ABORT_LINE("The FEI scope plugin version is not new enough to support variable"
        " speed for line:\n\n");
    break;

  case CME_WRITEFRAMESERIESANGLES:                          // WriteFrameSeriesAngles
  {
    FloatVec angles;
    mCamera->GetFrameTSactualAngles(angles);
    if (!angles.size())
      ABORT_NOLINE("There are no angles available from a frame tilt series");
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    CString message = "Error opening file ";
    CStdioFile *csFile = NULL;
    try {
      csFile = new CStdioFile(strCopy, CFile::modeCreate | CFile::modeWrite | 
        CFile::shareDenyWrite);
      message = "Writing angles to file ";
      for (index = 0; index < (int)angles.size(); index++) {
        report.Format("%.2f\n", angles[index]);
        csFile->WriteString((LPCTSTR)report);
      }
    }
    catch (CFileException *perr) {
      perr->Delete();
      ABORT_NOLINE(message + strCopy);
    } 
    delete csFile;
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
      doBack = false;
      delISX = mScope->GetTiltAngle();
      index = 1;
      if (CMD_IS(RECORDANDTILTTO)) {
        index = 3;
        increment = itemDbl[1] - delISX;
        if (!itemEmpty[2] && itemDbl[2]) {
          doBack = true;
          backlashX = (float)fabs(itemDbl[2]);
          if (increment < 0)
            backlashX = -backlashX;
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
      smiRAT.alpha = delISX + increment;
      smiRAT.backAlpha = backlashX;
      truth = SetupStageRestoreAfterTilt(&strItems[index], smiRAT.x, smiRAT.y);
      mCamera->QueueStageMove(smiRAT, delay, doBack, truth);
      mCamera->InitiateCapture(RECORD_CONSET);
      mTestScale = true;
      mMovedStage = true;
      mTestTiltAngle = true;
      break;
    }

  case CME_AREPOSTACTIONSENABLED:                          // ArePostActionsEnabled
    truth = mWinApp->ActPostExposure();
    doShift = mCamera->PostActionsOK(&mConSets[RECORD_CONSET]);
    report.Format("Post-exposure actions %s allowed %sfor this camera%s", 
      truth ? "ARE" : "are NOT", doShift ? "in general " : "", 
      doShift ? " but ARE for Records currently" : "");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], truth ? 1. : 0., doShift ? 1 : 0.);
    break;
    
  case CME_TILTDURINGRECORD:                               // TiltDuringRecord
    delX = itemEmpty[3] ? 0. : itemDbl[3];
    if (mCamera->QueueTiltDuringShot(itemDbl[1], itemInt[2], delX)) {
      report.Format("Moving the stage with variable speed requires\n"
        "FEI plugin version %d (and same for server, if relevant)\n"
        "You only have version %d and this line cannot be used:\n\n", 
        FEI_PLUGIN_STAGE_SPEED, mScope->GetPluginVersion());
      ABORT_LINE(report);
    }
    mCamera->InitiateCapture(3);
    mMovedStage = true;
    mTestTiltAngle = true;
    break;
    
  case CME_TESTNEXTMULTISHOT:                              // TestNextMultiShot
    if (itemInt[1] < 0 || itemInt[1] > 2)
      ABORT_LINE("The value must be between 0 and 2 in line:\n\n");
    mTestNextMultiShot = itemInt[1];
    break;
    
  case CME_MULTIPLERECORDS:                                // MultipleRecords
  {
    if (mWinApp->mNavHelper->mMultiShotDlg)
      mWinApp->mNavHelper->mMultiShotDlg->UpdateAndUseMSparams();
    MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
    index = (itemEmpty[6] || itemInt[6] < -8) ? msParams->doEarlyReturn : itemInt[6];
    if (!camParams->K2Type)
      index = 0;
    index2 = msParams->inHoleOrMultiHole | (mTestNextMultiShot << 2);
    if (!itemEmpty[9] && itemInt[9] > -9) {
      index2 = itemInt[9];
      if (index2 < 1 || index2 > 11)
        ABORT_LINE("The ninth entry for doing within holes or\n"
          "in multiple holes must be between 1 and 11 in line:\n\n");
    }
    truth = (index == 0 || msParams->numEarlyFrames != 0) ? msParams->saveRecord : false;
    if (mWinApp->mParticleTasks->StartMultiShot(
      (itemEmpty[1] || itemInt[1] < -8) ? msParams->numShots : itemInt[1],
      (itemEmpty[2] || itemInt[2] < -8) ? msParams->doCenter : itemInt[2],
      (itemEmpty[3] || itemDbl[3] < -8.) ? msParams->spokeRad : (float)itemDbl[3],
      (itemEmpty[4] || itemDbl[4] < -8.) ? msParams->extraDelay : (float)itemDbl[4],
      (itemEmpty[5] || itemInt[5] < -8) ? truth : itemInt[5] != 0, index,
      (itemEmpty[7] || itemInt[7] < -8) ? msParams->numEarlyFrames : itemInt[7],
      (itemEmpty[8] || itemInt[8] < -8) ? msParams->adjustBeamTilt : itemInt[8] != 0,
      index2)) {
      AbortMacro();
      return;
    }
    break;
  }
    
  case CME_A:
  case CME_AUTOALIGN:                                       // Autoalign
  case CME_ALIGNTO:                                         // AlignTo
  case CME_CONICALALIGNTO:                                  // ConicalAlignTo
    index = 0;
    if (CMD_IS(ALIGNTO)  || CMD_IS(CONICALALIGNTO)) {
      if (ConvertBufferLetter(strItems[1], -1, true, index, report))
        ABORT_LINE(report);
    }
    delX = 0;
    truth = false;
    doShift = true;
    if (CMD_IS(CONICALALIGNTO)) {
      delX = itemDbl[2];
      truth = !itemEmpty[3] && itemInt[3] != 0;
    }
    if (CMD_IS(ALIGNTO) && !itemEmpty[2] && itemInt[2]) 
      doShift = false;
    mDisableAlignTrim = CMD_IS(ALIGNTO) && !itemEmpty[3] && itemInt[3];
    index2 = mShiftManager->AutoAlign(index, 0, doShift, truth, NULL, 0., 0.,(float)delX);
    mDisableAlignTrim = false;
    if (index2)
      SUSPEND_NOLINE("because of failure to autoalign");
    break;

  case CME_LIMITNEXTAUTOALIGN:                              // LimitNextAutoalign
    mShiftManager->SetNextAutoalignLimit((float)itemDbl[1]);
    break;

  case CME_G:
  case CME_AUTOFOCUS:                                       // AutoFocus
    index = 1;
    if (!itemEmpty[1])
      index = itemInt[1];
    if (index > -2 && !mWinApp->mFocusManager->FocusReady())
      SUSPEND_NOLINE("because autofocus not calibrated");
    if (mMinDeltaFocus != 0. || mMaxDeltaFocus != 0.)
      mWinApp->mFocusManager->NextFocusChangeLimits(mMinDeltaFocus, mMaxDeltaFocus);
    if (mMinAbsFocus != 0. || mMaxAbsFocus != 0.)
      mWinApp->mFocusManager->NextFocusAbsoluteLimits(mMinAbsFocus, mMaxAbsFocus);
    if (index < -1)
      mWinApp->mFocusManager->DetectFocus(FOCUS_REPORT, 
      !itemEmpty[2] ? itemInt[2] : 0);
    else
      mWinApp->mFocusManager->AutoFocusStart(index, 
      !itemEmpty[2] ? itemInt[2] : 0);
    mTestScale = true;
    break;

  case CME_BEAMTILTDIRECTION:                              // BeamTiltDirection
    mWinApp->mFocusManager->SetTiltDirection(itemInt[1]);
    break;
    
  case CME_FOCUSCHANGELIMITS:                              // FocusChangeLimits
    mMinDeltaFocus = (float)itemDbl[1];
    mMaxDeltaFocus = (float)itemDbl[2];
    break;
    
  case CME_ABSOLUTEFOCUSLIMITS:                            // AbsoluteFocusLimits
    mMinAbsFocus = itemDbl[1];
    mMaxAbsFocus = itemDbl[2];
    break;
    
  case CME_CORRECTASTIGMATISM:                             // CorrectAstigmatism
    if (mWinApp->mAutoTuning->FixAstigmatism(itemEmpty[1] || itemInt[1] >= 0))
      ABORT_NOLINE("There is no astigmatism calibration for the current settings");
    break;
    
  case CME_CORRECTCOMA:                                    // CorrectComa
    index = COMA_INITIAL_ITERS;
    if (!itemEmpty[1] && itemInt[1])
      index = itemInt[1] > 0 ? COMA_ADD_ONE_ITER : COMA_JUST_MEASURE;
    if (mWinApp->mAutoTuning->ComaFreeAlignment(false, index))
      AbortMacro();
    break;
    
  case CME_ZEMLINTABLEAU:                                  // ZemlinTableau
    index = 340;
    index2 = 170;
    if (!itemEmpty[2] && itemInt[2] > 10)
      index = itemInt[2];
    if (!itemEmpty[3] && itemInt[3] > 0)
      index2 = itemInt[3];
    mWinApp->mAutoTuning->MakeZemlinTableau((float)itemDbl[1], index, index2);
    break;
    
  case CME_CBASTIGCOMA:                                    // CBAstigComa
    B3DCLAMP(itemInt[1], 0, 2);
    if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(itemInt[1], itemInt[2] != 0, 
      itemInt[3], !itemEmpty[4] && itemInt[4] > 0)) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_FIXASTIGMATISMBYCTF:                             // FixAstigmatismByCTF
  case CME_FIXCOMABYCTF:                                    // FixComaByCTF
    index = 0;
    if (CMD_IS(FIXCOMABYCTF)) {
       index = mWinApp->mAutoTuning->GetCtfDoFullArray() ? 2 : 1;
      if (!itemEmpty[3])
        index = itemInt[3] > 0 ? 2 : 1;
    }
    if (mWinApp->mAutoTuning->CtfBasedAstigmatismComa(index, false,
      (!itemEmpty[1] && itemInt[1] > 0) ? 1 : 0, !itemEmpty[2] && itemInt[2] > 0)) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_REPORTSTIGMATORNEEDED:                           // ReportStigmatorNeeded
    mWinApp->mAutoTuning->GetLastAstigNeeded(backlashX, backlashY);
    report.Format("Last measured stigmator change needed: %.4f  %.4f", backlashX,
      backlashY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], backlashX, backlashY);
    break;
    
  case CME_REPORTCOMATILTNEEDED:                            // ReportComaTiltNeeded
    mWinApp->mAutoTuning->GetLastBeamTiltNeeded(backlashX, backlashY);
    report.Format("Last measured beam tilt change needed: %.2f  %.2f", backlashX,
      backlashY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], backlashX, backlashY);
    break;
    
  case CME_REPORTCOMAVSISMATRIX:                            // ReportComaVsISmatrix
  {
    ComaVsISCalib *cvsis = mWinApp->mAutoTuning->GetComaVsIScal();
    if (cvsis->magInd <= 0)
      ABORT_LINE("There is no calibration of beam tilt versus image shift for line:\n\n");
    report.Format("Coma versus IS calibration is %f  %f  %f  %f", cvsis->matrix.xpx,
      cvsis->matrix.xpy, cvsis->matrix.ypx, cvsis->matrix.ypy);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], cvsis->matrix.xpx, cvsis->matrix.xpy,
      cvsis->matrix.ypx, cvsis->matrix.ypy);
    break;
  }
    
  case CME_S:
  case CME_SAVE:                                            // Save
  {
    index2 = -1;
    if (ConvertBufferLetter(strItems[1], 0, true, i, report))
      ABORT_LINE(report);
    if (!itemEmpty[2]) {
      index2 = itemInt[2] - 1;
      if (index2 < 0 || index2 >= mWinApp->mDocWnd->GetNumStores())
        ABORT_LINE("File # to save to is beyond range of open file numbers in "
          "statement: \n\n");
    }

    report = i == 0 ? "A" : strItems[1];
    if (index2 < 0 && (mWinApp->Montaging() ||
      !mBufferManager->IsBufferSavable(mImBufs + i)))
      SUSPEND_NOLINE("because buffer " + report + " is not savable to current file");
    if (itemEmpty[1])
      mWinApp->mDocWnd->SaveRegularBuffer();
    else if (mWinApp->mDocWnd->SaveBufferToFile(i, index2))
      SUSPEND_LINE("because of error saving to file in statement: \n\n");
    EMimageExtra *extra = (EMimageExtra *)mImBufs[i].mImage->GetUserData();
    if (extra) {
      if (index2 < 0)
        report.Format("Saved Z =%4d, %6.2f degrees", mImBufs[i].mSecNumber,
          extra->m_fTilt);
      else
        report.Format("Saved Z =%4d, %6.2f degrees to file #%d", mImBufs[i].mSecNumber,
          extra->m_fTilt, index2 + 1);
      mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    }
    break;
  }

  case CME_READFILE:                                        // ReadFile
    index = itemInt[1];
    iy0 = mBufferManager->GetBufToReadInto();
    if (!mWinApp->mStoreMRC)
      SUSPEND_NOLINE("on ReadFile because there is no open file");
    if (ConvertBufferLetter(strItems[2], iy0, false, ix0, report))
      ABORT_LINE(report);
    mBufferManager->SetBufToReadInto(ix0);
    if (mWinApp->Montaging())
      index2 = mWinApp->mMontageController->ReadMontage(index, NULL, NULL, false, true);
    else {
      index2 = mBufferManager->ReadFromFile(mWinApp->mStoreMRC, index);
      if (!index2)
        mWinApp->DrawReadInImage();
    }
    mBufferManager->SetBufToReadInto(iy0);
    if (index2)
      ABORT_NOLINE("Script stopped because of error reading from file");
    break;
    
  case CME_READOTHERFILE:                                   // ReadOtherFile
    index = itemInt[1];
    if (index < 0)
      ABORT_LINE("Section to read is negative in line:\n\n");
    iy0 = mBufferManager->GetBufToReadInto();
    if (itemInt[2] < 0)
      ix0 = iy0;
    else if (ConvertBufferLetter(strItems[2], -1, false, ix0, report))
      ABORT_LINE(report);
    if (CheckConvertFilename(strItems, strLine, 3, report))
      return;
    for (ix1 = 0; ix1 <= mRetryReadOther; ix1++) {
      mBufferManager->SetOtherFile(report);
      mBufferManager->SetBufToReadInto(ix0);
      mBufferManager->SetNextSecToRead(index);
      index2 = mBufferManager->RereadOtherFile(strCopy);
      if (!index2) {
        mWinApp->DrawReadInImage();
        break;
      }
      if (ix1 < mRetryReadOther)
        Sleep(readOtherSleep);
    }
    mBufferManager->SetBufToReadInto(iy0);
    if (index2)
      ABORT_NOLINE("Script stopped because of error reading from other file:\n" + 
      strCopy);
    break;
    
  case CME_RETRYREADOTHERFILE:                              // RetryReadOtherFile
    mRetryReadOther = B3DMAX(0, itemInt[1]);
    break;
    
  case CME_SAVETOOTHERFILE:                                 // SaveToOtherFile
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    index2 = -1;
    if (strItems[2] == "MRC")
      index2 = STORE_TYPE_MRC;
    else if (strItems[2] == "TIF" || strItems[2] == "TIFF")
      index2 = STORE_TYPE_TIFF;
    else if (strItems[2] == "JPG" || strItems[2] == "JPEG")
      index2 = STORE_TYPE_JPEG;
    else if (strItems[2] != "CUR" && strItems[2] != "-1")
      ABORT_LINE("Second entry must be MRC, TIF, TIFF, JPG, JPEG, CUR, or -1 in line:"
      "\n\n");
    ix1 = -1;
    if (strItems[3] == "NONE")
      ix1 = COMPRESS_NONE;
    else if (strItems[3] == "LZW")
      ix1 = COMPRESS_LZW;
    else if (strItems[3] == "ZIP")
      ix1 = COMPRESS_ZIP;
    else if (strItems[3] == "JPG" || strItems[3] == "JPEG")
      ix1 = COMPRESS_JPEG;
    else if (strItems[3] != "CUR" && strItems[3] != "-1")
      ABORT_LINE("Third entry must be NONE, LZW, ZIP, JPG, JPEG, CUR, or -1 in line:"
      "\n\n");
    if (CheckConvertFilename(strItems, strLine, 4, report))
      return;
    iy1 = mWinApp->mDocWnd->SaveToOtherFile(index, index2, ix1, &report);
    if (iy1 == 1)
      return;
    if (iy1) {
      report.Format("Error %s file for line:\n\n", iy1 == 2 ? "opening" : "saving to");
      ABORT_LINE(report);
    }
    break;
    
  case CME_OPENNEWFILE:                                     // OpenNewFile
  case CME_OPENNEWMONTAGE:                                  // OpenNewMontage
  case CME_OPENFRAMESUMFILE:                                // OpenFrameSumFile
    index = 1;
    if (CMD_IS(OPENNEWMONTAGE)) { 
      index = 3;
      ix0 = itemInt[1];
      if (!ix0)
        ix0 = montP->xNframes;
      iy0 = itemInt[2];
      if (!iy0)
        iy0 = montP->yNframes;
      if (ix0 < 1 || iy0 < 1)
        ABORT_LINE("Number of montages pieces is missing or not positive"
          " in statement:\n\n");
    }
    if (CMD_IS(OPENFRAMESUMFILE)) {                         
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, index, strCopy);
      report = mCamera->GetFrameFilename() + strCopy;
      mWinApp->AppendToLog("Opening file: " + report, LOG_SWALLOW_IF_CLOSED);
    } else if (CheckConvertFilename(strItems, strLine, index, report))
      return;

    // Check if file already exists
    if (!mOverwriteOK && CFile::GetStatus((LPCTSTR)report, status))
      SUSPEND_NOLINE("opening new file because " + report + " already exists");

    if (index == 1)
      index2 = mWinApp->mDocWnd->DoOpenNewFile(report);
    else {
      mWinApp->mDocWnd->LeaveCurrentFile();
      montP->xNframes = ix0;
      montP->yNframes = iy0;
      index2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, ix0, iy0, report);
    }
    if (index2)
      SUSPEND_LINE("because of error opening new file in statement:\n\n");
    mWinApp->mBufferWindow.UpdateSaveCopy();
    break;

  case CME_SETUPWAFFLEMONTAGE:                              // SetupWaffleMontage
    if (itemInt[1] < 2)
      ABORT_LINE("Minimum number of blocks in waffle grating must be at least 2 in:\n\n");
    backlashX = (float)(0.462 * itemInt[1]);
    sizeX = mConSets[RECORD_CONSET].right - mConSets[RECORD_CONSET].left;
    sizeY = mConSets[RECORD_CONSET].bottom - mConSets[RECORD_CONSET].top;
    ix0 = PiecesForMinimumSize(backlashX, sizeX, 0.1f);
    iy0 = PiecesForMinimumSize(backlashX, sizeY, 0.1f);
    if (ix0 < 2 && iy0 < 2) {
      mWinApp->AppendToLog("No montage is needed to measure pixel size at this "
        "magnification");
      SetReportedValues(0.);
    } else {
      if (mWinApp->Montaging() && montP->xNframes == ix0 && montP->yNframes == iy0) {
        montP->magIndex = mScope->GetMagIndex();
        mWinApp->AppendToLog("Existing montage can be used to measure pixel size at "
          "this magnification");
      } else {

                   // If it is montaging already, close file
        if (mWinApp->Montaging())
          mWinApp->mDocWnd->DoCloseFile();

                   // Follow same procedure as above for opening montage file
        if (CheckConvertFilename(strItems, strLine, 2, report))
          return;
        mWinApp->mDocWnd->LeaveCurrentFile();
        montP->xNframes = ix0;
        montP->yNframes = iy0;
        montP->binning = mConSets[RECORD_CONSET].binning;
        montP->xFrame = sizeX / montP->binning;
        montP->yFrame = sizeY / montP->binning;
        montP->xOverlap = montP->xFrame / 10;
        montP->yOverlap = montP->yFrame / 10;
        index2 = mWinApp->mDocWnd->GetMontageParamsAndFile(true, ix0, iy0, report);
        mWinApp->mBufferWindow.UpdateSaveCopy();
        if (index2)
          ABORT_NOLINE("Error trying to open new montage the right size for current"
          " magnification");
        PrintfToLog("Opened file %s for %d x %d montage", (LPCTSTR)report, ix0, iy0);
      }
      montP->warnedMagChange = true;
      montP->overviewBinning = 1;
      montP->shiftInOverview = true;
      mWinApp->mMontageWindow.UpdateSettings();
      SetReportedValues(1.);
    }
    break;
    
  case CME_ENTERNAMEOPENFILE:                               // EnterNameOpenFile
    strCopy = "Enter name for new file:";
    if (!strItems[1].IsEmpty())
      mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    if (!KGetOneString(strCopy, mEnteredName, 100))
      SUSPEND_NOLINE("because no new file was opened");
    if (mWinApp->mDocWnd->DoOpenNewFile(mEnteredName))
      SUSPEND_NOLINE("because of error opening new file by that name");
    break;
    
  case CME_CHOOSERFORNEWFILE:                               // ChooserForNewFile
    index = (itemInt[1] != 0) ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    if (mWinApp->mDocWnd->FilenameForSaveFile(itemInt[1], NULL, strCopy)) {
      AbortMacro();
      return;
    }
    report = strItems[2];
    report.MakeUpper();
    if (SetVariable(report, strCopy, VARTYPE_REGULAR, -1, false))
      ABORT_NOLINE("Error setting variable " + strItems[2] + " with filename " + strCopy);
    mOverwriteOK = true;
    break;
    
  case CME_READTEXTFILE:                                    // ReadTextFile
  case CME_READ2DTEXTFILE:                                  // Read2DTextFile
  case CME_READSTRINGSFROMFILE:                             // ReadStringsFromFile
  {
    SubstituteLineStripItems(strLine, 2, strCopy);
    CString newVar;
    CStdioFile *csFile = NULL;
    CArray<ArrayRow, ArrayRow> *rowsFor2d = NULL;
    ArrayRow arrRow;
    truth = CMD_IS(READ2DTEXTFILE);
    try {
      csFile = new CStdioFile(strCopy, CFile::modeRead | CFile::shareDenyWrite);
    }
    catch (CFileException *perr) {
      perr->Delete();
      ABORT_NOLINE("Error opening file " + strCopy);
    }
    if (truth)
      rowsFor2d = new CArray<ArrayRow, ArrayRow>;
    while ((index = mWinApp->mParamIO->ReadAndParse(csFile, report, strItems,
      MAX_TOKENS)) == 0) {
      if (truth)
        newVar = "";
      if (CMD_IS(READSTRINGSFROMFILE)) {
        if (newVar.GetLength())
          newVar += '\n';
        newVar += report;
      } else {
        for (index2 = 0; index2 < MAX_TOKENS; index2++) {
          if (strItems[index2].IsEmpty())
            break;
          if (newVar.GetLength())
            newVar += '\n';
          newVar += strItems[index2];
        }
      }
      if (truth && index2 > 0) {
        arrRow.value = newVar;
        arrRow.numElements = index2;
        rowsFor2d->Add(arrRow);
      }
    }
    delete csFile;
    if (index > 0) {
      delete rowsFor2d;
      ABORT_NOLINE("Error reading text from file " + strCopy);
    }

    if (SetVariable(item1upper, truth ? "0" : newVar, VARTYPE_REGULAR, -1, false, &report,
      rowsFor2d)) {
      delete rowsFor2d;
      ABORT_NOLINE("Error setting an array variable with text from file " + strCopy +
        ":\n" + report);
    }
    break;
  }

  case CME_OPENTEXTFILE:                                    // OpenTextFile
  {
    UINT openFlags = CFile::typeText;
    if (LookupFileForText(item1upper, TXFILE_MUST_NOT_EXIST, strLine, index))
      return;
    report = strItems[2].MakeUpper();
    if (report.GetLength() > 1 || CString("RTWAO").Find(report) < 0)
      ABORT_LINE("The second entry must be R, T, W, A, or O in line\n\n:");
    SubstituteLineStripItems(strLine, 4, strCopy);

    // Set up flags and read/write from mode entry
    if (report == "R" || report == "T") {
      openFlags |= CFile::shareDenyNone | CFile::modeRead;
      truth = true;
    } else {
      truth = false;
      openFlags |= CFile::shareDenyWrite;
      if (report == "A") {
        openFlags |= CFile::modeReadWrite;
      } else {
        openFlags |= CFile::modeWrite | CFile::modeCreate;
        if (report == "W" && !mOverwriteOK && CFile::GetStatus((LPCTSTR)strCopy, status))
          ABORT_LINE("File already exists and you entered W instead of O for line:\n\n");
      }
    }

    // Create new entry aand try to open file; allowing  failure with 'T'
    txFile = new FileForText;
    txFile->readOnly = truth;
    txFile->ID = item1upper;
    txFile->persistent = itemInt[3] != 0;
    txFile->csFile = NULL;
    try {
      txFile->csFile = new CStdioFile(strCopy, openFlags);
      if (report == "A")
        txFile->csFile->SeekToEnd();
    }
    catch (CFileException *perr) {
      perr->Delete();
      delete txFile->csFile;
      delete txFile;
      if (report != "T")
        ABORT_NOLINE("Error opening file " + strCopy);
      txFile = NULL;
    }

    // Add to array if success; set value for 'T'
    if (txFile)
      mTextFileArray.Add(txFile);
    if (report == "T")
      SetReportedValues(txFile ? 1 : 0);
    break;
  }

  case CME_WRITELINETOFILE:                                 // WriteLineToFile
    txFile = LookupFileForText(item1upper, TXFILE_WRITE_ONLY, strLine, index);
    if (!txFile)
      return;

    // To allow comments to be written, there is only one required argument in initial
    // check so we need to check here
    SubstituteLineStripItems(strLine, 2, strCopy);
    if (strCopy.IsEmpty())
      ABORT_LINE("You must enter some text after the ID for line:\n\n");
    try {
      txFile->csFile->WriteString(strCopy + "\n");
    }
    catch (CFileException *perr) {
      perr->Delete();
      ABORT_LINE("Error writing string to file for line:\n\n");
    }
    break;

  case CME_READLINETOARRAY:                                 // ReadLineToArray
  case CME_READLINETOSTRING:                                // ReadLineToString
    txFile = LookupFileForText(item1upper, TXFILE_READ_ONLY, strLine, index);
    if (!txFile)
      return;
    strLine = strItems[2];
    truth = CMD_IS(READLINETOARRAY);

    // Skip blank lines, skip comment lines if reading into array
    for (;;) {
      index = mWinApp->mParamIO->ReadAndParse(txFile->csFile, report, strItems,
        MAX_TOKENS);
      if (index || !strItems[0].IsEmpty() || (!truth && !report.IsEmpty()))
        break;
    }

    // Check error conditions
    if (index > 0)
      ABORT_LINE("Error reading from file for line:\n\n");
    if (index) {
      mWinApp->AppendToLog("End of file reached for file with ID " + txFile->ID,
        mLogAction);
    } else {

      // For array, concatenate items into report; otherwise report is all set
      if (truth) {
        report = "";
        for (index2 = 0; index2 < MAX_TOKENS; index2++) {
          if (strItems[index2].IsEmpty())
            break;
          if (report.GetLength())
            report += '\n';
          report += strItems[index2];
        }
      }
      if (SetVariable(strLine, report, VARTYPE_REGULAR, -1, false, &strCopy))
        ABORT_NOLINE("Error setting a variable with line from file:\n" + strCopy);
    }
    SetReportedValues(index ? 0 : 1);
    break;

  case CME_CLOSETEXTFILE:                                   // CloseTextFile
    if (!LookupFileForText(item1upper, TXFILE_MUST_EXIST, strLine, index))
      return;
    CloseFileForText(index);
    break;
    
  case CME_FLUSHTEXTFILE:                                   // FlushTextFile
    txFile = LookupFileForText(item1upper, TXFILE_MUST_EXIST, strLine, index);
    if (!txFile)
      return;
    txFile->csFile->Flush();
    break;

  case CME_ISTEXTFILEOPEN:                                  // IsTextFileOpen
    txFile = LookupFileForText(item1upper, TXFILE_QUERY_ONLY, strLine, index);
    truth = false;
    if (!txFile && !itemEmpty[2]) {

      // Check the file name if the it doesn't match ID
      char fullBuf[_MAX_PATH];
      SubstituteLineStripItems(strLine, 2, strCopy);
      if (GetFullPathName((LPCTSTR)strCopy, _MAX_PATH, fullBuf, NULL) > 0) {
        for (index = 0; index < mTextFileArray.GetSize(); index++) {
          txFile = mTextFileArray.GetAt(index);
          if (!(txFile->csFile->GetFilePath()).CompareNoCase(fullBuf)) {
            truth = true;
            break;
          }
        }
      }
      report.Format("Text file with name %s %s open", (LPCTSTR)strCopy, truth ? "IS" :
        "is NOT");
      SetReportedValues(truth ? 1 : 0);
    } else {
      report.Format("Text file with identifier %s %s open", (LPCTSTR)item1upper, 
        txFile ? "IS" : "is NOT");
      SetReportedValues(txFile ? 1 : 0);
    }
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_USERSETDIRECTORY:                                // UserSetDirectory
  {
    strCopy = "Choose a new current working directory:";
    if (!strItems[1].IsEmpty())
      mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    char *cwd = _getcwd(NULL, _MAX_PATH);
    CXFolderDialog dlg(cwd);
    dlg.SetTitle(strCopy);
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
    FileOptions *fileOpt = mWinApp->mDocWnd->GetFileOpt();
    if (itemInt[2] != 0)
      fileOpt->montFileType = itemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    else
      fileOpt->fileType = itemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
    break;
  }
    
  case CME_OPENOLDFILE:                                     // OpenOldFile
    if (CheckConvertFilename(strItems, strLine, 1, report))
      return;
    index = mWinApp->mDocWnd->OpenOldMrcCFile(&cfile, report, false);
    if (index == MRC_OPEN_NOERR || index == MRC_OPEN_ADOC)
      index = mWinApp->mDocWnd->OpenOldFile(cfile, report, index);
    if (index != MRC_OPEN_NOERR)
      SUSPEND_LINE("because of error opening old file in statement:\n\n");
    break;
    
  case CME_CLOSEFILE:                                       // CloseFile
    if (mWinApp->mStoreMRC) {
      mWinApp->mDocWnd->DoCloseFile();
    } else if (!itemEmpty[1]) {
      SuspendMacro();
      AfxMessageBox("Script suspended on CloseFile because there is no open file",
        MB_EXCLAME);
      return;
    }
    break;
    
  case CME_REMOVEFILE:                                      // RemoveFile
    if (CheckConvertFilename(strItems, strLine, 1, report))
      return;
    try {
      CFile::Remove(report);
    }
    catch (CFileException* pEx) {
      pEx->Delete();
      PrintfToLog("WARNING: File %s cannot be removed", (LPCTSTR)report);
    }
    break;
    
  case CME_REPORTCURRENTFILENAME:                           // ReportCurrentFilename
  case CME_REPORTLASTFRAMEFILE:                             // ReportLastFrameFile
  case CME_REPORTNAVFILE:                                   // ReportNavFile
  {
    truth = !CMD_IS(REPORTLASTFRAMEFILE);
    if (CMD_IS(REPORTCURRENTFILENAME)) {
      if (!mWinApp->mStoreMRC)
        SUSPEND_LINE("because there is no file open currently for statement:\n\n");
      report = mWinApp->mStoreMRC->getFilePath();
      strCopy = "Current open image file is: ";
    }
    else if (truth) {
      ABORT_NONAV;
      report = mWinApp->mNavigator->GetCurrentNavFile();
      if (report.IsEmpty())
        ABORT_LINE("There is no Navigator file open for:\n\n");
      strCopy = "Current open Navigator file is: ";
    }
    else {
      report = mCamera->GetPathForFrames();
      if (report.IsEmpty())
        ABORT_LINE("There is no last frame file name available for:\n\n");
      strCopy = "Last frame file is: ";
    }
    mWinApp->AppendToLog(strCopy + report, mLogAction);
    CString root = report;
    CString ext;
    if (!itemEmpty[1] && itemInt[1] && truth)
      UtilSplitExtension(report, root, ext);
    SetOneReportedValue(!truth ? &strItems[1] : NULL, root, 1);

    if (!ext.IsEmpty())
      SetOneReportedValue(ext, 2);
    if (!itemEmpty[1] && itemInt[1] && truth) {
      UtilSplitPath(root, report, ext);
      SetOneReportedValue(report, 3);
      SetOneReportedValue(ext, 4);
    }
    break;
  }

  case CME_REPORTFRAMEBASENAME:                             // ReportFrameBaseName
    strCopy = mCamera->GetFrameBaseName();
    index = mCamera->GetFrameNameFormat();
    if ((index & FRAME_FILE_ROOT) && !strCopy.IsEmpty()) {
      report = "The frame base name is " + strCopy;
      SetOneReportedValue(&strItems[1], 1., 1);
      SetOneReportedValue(&strItems[1], strCopy, 2);
    } else {
      report = "The base name is not being used in frame file names";
      SetOneReportedValue(&strItems[1], 0., 1);
    }
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_ALLOWFILEOVERWRITE:                              // AllowFileOverwrite
    mOverwriteOK = itemInt[1] != 0;
    break;
    
  case CME_SETDIRECTORY:                                    // SetDirectory
  case CME_MAKEDIRECTORY:                                   // Make Directory
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    if (strCopy.IsEmpty())
      ABORT_LINE("Missing directory name in statement:\n\n");
    if (CMD_IS(SETDIRECTORY)) {
      if (_chdir((LPCTSTR)strCopy))
        SUSPEND_NOLINE("because of failure to change directory to " + strCopy);
    } else {
      if (CFile::GetStatus((LPCTSTR)strCopy, status)) {
        mWinApp->AppendToLog("Not making directory " + strCopy + " - it already exists");
      } else {
        if (!_mkdir((LPCTSTR)strCopy))
          mWinApp->AppendToLog("Created directory " + strCopy);
        else
          SUSPEND_NOLINE("because of failure to create directory " + strCopy);
      }
    }
    break;

  case CME_REPORTDIRECTORY:                                 // ReportDirectory
  {
    char *cwd = _getcwd(NULL, _MAX_PATH);
    if (!cwd)
      ABORT_LINE("Could not determine current directory for:\n\n");
    strCopy = cwd;
    free(cwd);
    mWinApp->AppendToLog("Current directory is " + strCopy, mLogAction);
    SetOneReportedValue(&strItems[1], strCopy, 1);
    break;
  }
    
  case CME_MAKEDATETIMEDIR:                                 // MakeDateTimeDir
    strCopy = mWinApp->mDocWnd->DateTimeForFrameSaving();
    if (_mkdir((LPCTSTR)strCopy))
      SUSPEND_NOLINE("because of failure to create directory " + strCopy);
    if (_chdir((LPCTSTR)strCopy))
      SUSPEND_NOLINE("because of failure to change directory to " + strCopy);
    mWinApp->AppendToLog("Created directory " + strCopy);
    break;
    
  case CME_SWITCHTOFILE:                                    // SwitchToFile
    index = itemInt[1];
    if (index < 1 || index > mWinApp->mDocWnd->GetNumStores())
      ABORT_LINE("Number of file to switch to is absent or out of range in statement:"
        " \n\n");
    mWinApp->mDocWnd->SetCurrentStore(index - 1);
    break;
    
  case CME_REPORTFILENUMBER:                                // ReportFileNumber
    index = mWinApp->mDocWnd->GetCurrentStore();
    if (index >= 0) {
      index++;
      report.Format("Current open file number is %d", index);
    } else 
      report = "There is no file open currently";
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index);
    break;
    
  case CME_ADDTOAUTODOC:                                    // AddToAutodoc
  case CME_WRITEAUTODOC:                                    // WriteAutodoc
    if (!mWinApp->mStoreMRC)
      ABORT_LINE("There is no open image file for: \n\n");
    if (mBufferManager->CheckAsyncSaving())
      ABORT_NOLINE("There was an error writing to file.\n"
      "Not adding to the autodoc because it would go into the wrong section");
    index = mWinApp->mStoreMRC->GetAdocIndex();
    index2 = mWinApp->mStoreMRC->getDepth();
    if (index < 0)
      ABORT_LINE("There is no .mdoc file for the current image file for: \n\n");
    if (AdocGetMutexSetCurrent(index) < 0)
      ABORT_LINE("Error making autodoc be the current one for: \n\n");
    if (CMD_IS(ADDTOAUTODOC)) {
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
      mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS);
      if (AdocSetKeyValue(
        index2 ? B3DCHOICE(mWinApp->mStoreMRC->getStoreType() == STORE_TYPE_ADOC,
        ADOC_IMAGE, ADOC_ZVALUE) : ADOC_GLOBAL, 
        index2 ? index2 - 1 : 0, (LPCTSTR)strItems[1], (LPCTSTR)strCopy)) {
          AdocReleaseMutex();
          ABORT_LINE("Error adding string to autodoc file for: \n\n");
      }
    } else if (AdocWrite((char *)(LPCTSTR)mWinApp->mStoreMRC->getAdocName()) < 0) {
      AdocReleaseMutex();
      ABORT_NOLINE("Error writing to autodoc file");
    }
    AdocReleaseMutex();
    break;
  
  case CME_ADDTOFRAMEMDOC:                                  // AddToFrameMdoc
  case CME_WRITEFRAMEMDOC:                                  // WriteFrameMdoc
  {
    const char *frameMsg[] = { "There is no autodoc for frames open in:\n\n",
      "Error selecting frame .mdoc as current autodoc in:\n\n",
      "There is no current section to add to in frame .mdoc for:\n\n",
      "Error adding to frame .mdoc in:\n\n",
      "Error writing to frame .mdoc in:\n\n" };
    if (CMD_IS(ADDTOFRAMEMDOC)) {
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
      mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS);
      index = mWinApp->mDocWnd->AddValueToFrameMdoc(strItems[1], strCopy);
    }
    else {
      index = mWinApp->mDocWnd->WriteFrameMdoc();
    }
    if (index > 0)
      ABORT_LINE(CString(frameMsg[index - 1]));
    break;
  }

  case CME_REPORTFRAMEMDOCOPEN:                             // reportFrameMdocOpen
    index = mWinApp->mDocWnd->GetFrameAdocIndex() >= 0 ? 1 : 0;
    SetReportedValues(&strItems[1], index);
    report.Format("Autodoc for frames %s open", index ? "IS" : "is NOT");
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_DEFERWRITINGFRAMEMDOC:                           // DeferWritingFrameMdoc
    mWinApp->mDocWnd->SetDeferWritingFrameMdoc(true);
    break;
    
  case CME_OPENFRAMEMDOC:                                   // OpenFrameMdoc
    SubstituteVariables(&strLine, 1, strLine);
    if (mWinApp->mDocWnd->GetFrameAdocIndex() >= 0)
      ABORT_LINE("The frame mdoc file is already open for line:\n\n");
    if (CheckConvertFilename(strItems, strLine, 1, report))
      return;
    if (mWinApp->mDocWnd->DoOpenFrameMdoc(report))
      SUSPEND_LINE("because of error opening frame mdoc file in statement:\n\n")
    break;

  case CME_CLOSEFRAMEMDOC:                                  // CloseFrameMdoc
    if (!itemEmpty[1] && itemInt[1] && mWinApp->mDocWnd->GetFrameAdocIndex() < 0)
      ABORT_LINE("There is no frame mdoc file open for line:\n\n");
    mWinApp->mDocWnd->DoCloseFrameMdoc();
    break;
    
  case CME_ADDTONEXTFRAMESTACKMDOC:                         // AddToNextFrameStackMdoc
  case CME_STARTNEXTFRAMESTACKMDOC:                         // StartNextFrameStackMdoc
    doBack = CMD_IS(STARTNEXTFRAMESTACKMDOC);
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
    mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS);
    if (mCamera->AddToNextFrameStackMdoc(strItems[1], strCopy, doBack, &report))
      ABORT_LINE(report + " in:\n\n");
    break;

  case CME_ALIGNWHOLETSONLY:                                // SetAlignWholeTSOnly 
    index  = (itemEmpty[1] || itemInt[1] != 0) ? 1 : 0;
    if (mCamera->IsConSetSaving(&mConSets[RECORD_CONSET], RECORD_CONSET, camParams, false)
      && (mConSets[RECORD_CONSET].alignFrames || !index) && 
      mConSets[RECORD_CONSET].useFrameAlign > 1 && mCamera->GetAlignWholeSeriesInIMOD()) {
        if (index && !mWinApp->mStoreMRC)
          ABORT_LINE("There must be an output file before this command can be used:\n\n");
        if (index && mWinApp->mStoreMRC->GetAdocIndex() < 0)
          ABORT_NOLINE("The output file was not opened with an associated .mdoc\r\n"
            "file, which is required to align whole tilt series in IMOD");
        SaveControlSet(RECORD_CONSET);
        mConSets[RECORD_CONSET].alignFrames = 1 - index;
        mAlignWholeTSOnly = index > 0;
    } else
      index = 0;
    SetReportedValues(index);
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
    truth = CMD_IS(CLOSELOGOPENNEW);
    if (mWinApp->mLogWindow) {
      report = mWinApp->mLogWindow->GetSaveFile();
      if (!report.IsEmpty())
        mWinApp->mLogWindow->DoSave();
      else if ((!truth || itemEmpty[1] || !itemInt[1]) && 
        mWinApp->mLogWindow->AskIfSave("closing and opening it again?")) {
        AbortMacro();
        return;
      }
      mWinApp->mLogWindow->SetUnsaved(false);
        mWinApp->mLogWindow->CloseLog();
    }
    mWinApp->AppendToLog(mWinApp->mDocWnd->DateTimeForTitle());
    if (!itemEmpty[1] && !truth) {
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 1, report);
      mWinApp->mLogWindow->UpdateSaveFile(true, report);
    }
    break;
    
  case CME_SAVELOG:                                         // SaveLog
    if (!mWinApp->mLogWindow)
      ABORT_LINE("The log window must already be open for line:\n\n");
    if (itemEmpty[2]) {
      report = mWinApp->mLogWindow->GetSaveFile();
      if (!report.IsEmpty())
        mWinApp->mLogWindow->DoSave();
      else
        mWinApp->OnFileSavelog();
    } else {
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 2, report);
      mWinApp->mLogWindow->UpdateSaveFile(true, report, true, itemInt[1] != 0);
      mWinApp->mLogWindow->DoSave();
    }

    break;

  case CME_SAVECALIBRATIONS:								// SaveCalibrations
	  if (mWinApp->GetAdministrator()) {
		  mWinApp->mDocWnd->SaveCalibrations();
	  }
	  else {
		  mWinApp->AppendToLog("Calibrations NOT saved from script, administrator mode not enabled");
	  }
	  break;
  

  case CME_SETPROPERTY:                                     // SetProperty
    if (mWinApp->mParamIO->MacroSetProperty(strItems[1], itemDbl[2]))
      AbortMacro();

    break;
    // ReportUserSetting, ReportProperty
  case CME_REPORTUSERSETTING:
  case CME_REPORTPROPERTY:                   
    truth = CMD_IS(REPORTPROPERTY);
    strCopy = truth ? "property" : "user setting";
    if ((!truth && mWinApp->mParamIO->MacroGetSetting(strItems[1], delX)) ||
      (truth && mWinApp->mParamIO->MacroGetProperty(strItems[1], delX)))
      ABORT_LINE(strItems[1] + " is not a recognized " + strCopy + " or cannot be "
      "accessed by script command in:\n\n");
    SetReportedValues(&strItems[2], delX);
    report.Format("Value of %s %s is %g", (LPCTSTR)strCopy, (LPCTSTR)strItems[1], delX);
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_SETUSERSETTING:                                  // SetUserSetting
    if (mWinApp->mParamIO->MacroGetSetting(strItems[1], delX) || 
      mWinApp->mParamIO->MacroSetSetting(strItems[1], itemDbl[2]))
        ABORT_LINE(strItems[1] + " is not a recognized setting or cannot be set by "
        "script command in:\n\n");
    mWinApp->UpdateWindowSettings();
  
    // See if property already saved
    truth = false;
    for (index = 0; index < (int)mSavedSettingNames.size(); index++) {
      if (strItems[1].CompareNoCase(mSavedSettingNames[index].c_str()) == 0) {
        truth = true;
        break;
      }
    }

    // If old value should be saved, either replace existing new value or push old and new
    if (itemEmpty[3] || !itemInt[3]) {
      if (truth) {
        mNewSettingValues[index] = itemDbl[2];
      } else {
        mSavedSettingNames.push_back(std::string((LPCTSTR)strItems[1]));
        mSavedSettingValues.push_back(delX);
        mNewSettingValues.push_back(itemDbl[2]);
      }
    } else if (truth) {

      // Otherwise get rid of a saved value if new value being kept
      mSavedSettingNames.erase(mSavedSettingNames.begin() + index);
      mSavedSettingValues.erase(mSavedSettingValues.begin() + index);
      mNewSettingValues.erase(mNewSettingValues.begin() + index);
    }
    break;
    
  case CME_COPY:                                            // Copy
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    if (ConvertBufferLetter(strItems[2], -1, false, index2, report))
      ABORT_LINE(report);
    if (mBufferManager->CopyImageBuffer(index, index2))
      SUSPEND_LINE("because of buffer copy failure in statement: \n\n");
    break;
    
  case CME_SHOW:                                            // Show
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    mWinApp->SetCurrentBuffer(index);
    break;
    
  case CME_CHANGEFOCUS:                                     // ChangeFocus
    if (itemDbl[1] == 0.0 || itemEmpty[1] || fabs(itemDbl[1]) > 1000.)
      ABORT_LINE("Improper focus change in statement: \n\n");
    mScope->IncDefocus(itemDbl[1]);
    break;
    
  case CME_SETDEFOCUS:                                      // SetDefocus
    if (itemEmpty[1] || fabs(itemDbl[1]) > 5000.)
      ABORT_LINE("Improper focus setting in statement: \n\n");
    mScope->SetDefocus(itemDbl[1]);
    break;
    
  case CME_SETSTANDARDFOCUS:                                // SetStandardFocus
  case CME_SETABSOLUTEFOCUS:                                // SetAbsoluteFocus
    mScope->SetFocus(itemDbl[1]);
    break;

  case CME_SETEUCENTRICFOCUS:                               // SetEucentricFocus
  {
    double *focTab = mScope->GetLMFocusTable();
    index = mScope->GetMagIndex();
    if (index <= 0 || mWinApp->GetSTEMMode())
      ABORT_NOLINE("You cannot set eucentric focus in diffraction or STEM mode");
    delX = focTab[index * 2 + mScope->GetProbeMode()];
    if (delX < -900.)
      delX = mScope->GetStandardLMFocus(index);
    if (delX < -900.)
      ABORT_NOLINE("There is no standard/eucentric focus defined for the current mag"
        " range");
    mScope->SetFocus(delX);
    if (focTab[index * 2 + mScope->GetProbeMode()] < -900. && !JEOLscope &&
      index >= mScope->GetLowestMModeMagInd())
      mWinApp->AppendToLog("WARNING: Setting eucentric focus using a calibrated "
        "standard focus\r\n   from the nearest mag, not the current mag");
    break;
  }

  case CME_CALEUCENTRICFOCUS:                               // CalEucentricFocus
    if (mWinApp->mMenuTargets.DoCalibrateStandardLMfocus(true))
      ABORT_NOLINE("You cannot calibrate eucentric focus in diffraction or STEM mode");
    break;
    
  case CME_INCTARGETDEFOCUS:                                // IncTargetDefocus
    if (fabs(itemDbl[1]) > 100.)
      ABORT_LINE("Change in target defocus too large in statement: \n\n");
    delX = mWinApp->mFocusManager->GetTargetDefocus();
    mWinApp->mFocusManager->SetTargetDefocus((float)(delX + itemDbl[1]));
    mWinApp->mAlignFocusWindow.UpdateSettings();
    break;
    
  case CME_SETTARGETDEFOCUS:                                // SetTargetDefocus
    if (itemDbl[1] < -200. || itemDbl[1] > 50.)
      ABORT_LINE("Target defocus too large in statement: \n\n");
    mWinApp->mFocusManager->SetTargetDefocus((float)itemDbl[1]);
    mWinApp->mAlignFocusWindow.UpdateSettings();
    break;
    
  case CME_REPORTAUTOFOCUSOFFSET:                           // ReportAutofocusOffset
    delX = mWinApp->mFocusManager->GetDefocusOffset();
    strCopy.Format("Autofocus offset is: %.2f um", delX);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_SETAUTOFOCUSOFFSET:                              // SetAutofocusOffset
    if (itemDbl[1] < -200. || itemDbl[1] > 200.)
      ABORT_LINE("Autofocus offset too large in statement: \n\n");
    if (mFocusOffsetToRestore < -9000.) {
      mFocusOffsetToRestore = mWinApp->mFocusManager->GetDefocusOffset();
      mNumStatesToRestore++;
    }
    mWinApp->mFocusManager->SetDefocusOffset((float)itemDbl[1]);
    break;
    
  case CME_SETOBJFOCUS:                                     // SetObjFocus
    index = itemInt[1];
    delX = mScope->GetDefocus();
    mScope->SetObjFocus(index);
    delY = mScope->GetDefocus();
    report.Format("Defocus before = %.4f   after = %.4f", delX, delY);
    mWinApp->AppendToLog(report, LOG_SWALLOW_IF_CLOSED);
    break;
    
  case CME_SETDIFFRACTIONFOCUS:                             // SetDiffractionFocus
    mScope->SetDiffractionFocus(itemDbl[1]);
    UpdateLDAreaIfSaved();
    break;
    
  case CME_RESETDEFOCUS:                                    // ResetDefocus
    if (!mScope->ResetDefocus())
      AbortMacro();
    break;
    
  case CME_SETMAG:                                          // SetMag
    index = B3DNINT(itemDbl[1]);
    if (!itemEmpty[2] && mWinApp->GetSTEMMode()) {
      mScope->SetSTEMMagnification(itemDbl[1]);
    } else {
      i = FindIndexForMagValue(index, -1, -2);
      if (!i)
        ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
      mScope->SetMagIndex(i);
      UpdateLDAreaIfSaved();
    }
    break;
    
  case CME_SETMAGINDEX:                                     // SetMagIndex
    index = itemInt[1];
    if (index <= 0 || index >= MAX_MAGS)
      ABORT_LINE("Invalid magnification index in:\n\n");
    delX = camParams->GIF ? mMagTab[index].EFTEMmag : mMagTab[index].mag;
    if (camParams->STEMcamera)
      delX =  mMagTab[index].STEMmag;
    if (!delX)
      ABORT_LINE("There is a zero in the magnification table at the index given in:\n\n");
    mScope->SetMagIndex(index);
    UpdateLDAreaIfSaved();
    break;
    
  case CME_CHANGEMAG:                                       // ChangeMag
  case CME_INCMAGIFFOUNDPIXEL:                              // IncMagIfFoundPixel 
    index = mScope->GetMagIndex();
    if (!index)
      SUSPEND_NOLINE("because you cannot use ChangeMag in diffraction mode");
    iy0 = mWinApp->GetCurrentCamera();
    ix0 = B3DNINT(itemDbl[1]);
    if (!CMD_IS(CHANGEMAG)) {
      ix0 = itemDbl[1] < 0 ? -1 : 1;
      truth = mWinApp->mProcessImage->GetFoundPixelSize(iy0, index) > 0.;
      SetReportedValues(truth ? 1. : 0.);
    }
    if (CMD_IS(CHANGEMAG) || truth) {
      index2 = mWinApp->FindNextMagForCamera(iy0, index, ix0);
      if (index2 < 0)
        ABORT_LINE("Improper mag change in statement: \n\n");
      mScope->SetMagIndex(index2);
    }
    UpdateLDAreaIfSaved();
    break;
                              
  case CME_CHANGEMAGANDINTENSITY:                           // ChangeMagAndIntensity
  case CME_SETMAGANDINTENSITY:                              // SetMagAndIntensity

    // Get starting and ending mag
    index = mScope->GetMagIndex();
    if (!index || mScope->GetSTEMmode())
      SUSPEND_NOLINE("because you cannot use Change/SetMagAndIntensity in diffraction"
      " or STEM mode");
    if (CMD_IS(CHANGEMAGANDINTENSITY)) {                    
      index2 = mWinApp->FindNextMagForCamera(mWinApp->GetCurrentCamera(), index, 
        B3DNINT(itemDbl[1]));
      if (itemEmpty[1] || index2 < 1)
        ABORT_LINE("Improper mag change in statement: \n\n");
    } else {
      i = B3DNINT(itemDbl[1]);
      index2 = FindIndexForMagValue(i, -1, -2);
      if (!index2)
        ABORT_LINE("The value is not near enough to an existing mag in:\n\n");
    }

    // Get the intensity and change in intensity
    delISX = mScope->GetIntensity();
    delISX = mScope->GetC2Percent(mScope->FastSpotSize(), delISX);
    i = mWinApp->GetCurrentCamera();
    delISY = pow((double)mShiftManager->GetPixelSize(i, index) /
      mShiftManager->GetPixelSize(i, index2), 2.);
    i = mWinApp->mBeamAssessor->AssessBeamChange(delISY, delX, delY, -1);
    if (CheckIntensityChangeReturn(i))
      return;

    // Change the mag then the intensity
    mScope->SetMagIndex(index2);
    if (!i)
      mScope->DelayedSetIntensity(delX, GetTickCount());
    strCopy.Format("%s before mag change %.3f%s, remaining factor of change "
      "needed %.3f", mScope->GetC2Name(), delISX, mScope->GetC2Units(), delY);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(delISX, delY);
    UpdateLDAreaIfSaved();
    break;

  case CME_SETCAMLENINDEX:                                  // SetCamLenIndex
    index = B3DNINT(itemDbl[1]);
    if (itemEmpty[1] || index < 1)
      ABORT_LINE("Improper camera length index in statement: \n\n");
    if (!mScope->SetCamLenIndex(index))
      ABORT_LINE("Error setting camera length index in statement: \n\n");
    UpdateLDAreaIfSaved();
    break;
    
  case CME_SETSPOTSIZE:                                     // SetSpotSize
    index = B3DNINT(itemDbl[1]);
    if (itemEmpty[1] || index < mScope->GetMinSpotSize() || 
      index > mScope->GetNumSpotSizes())
      ABORT_LINE("Improper spot size in statement: \n\n");
    if (!mScope->SetSpotSize(index)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;
    
  case CME_SETPROBEMODE:                                    // SetProbeMode
    index = B3DNINT(itemDbl[1]);
    if (strItems[1].Find("MICRO") == 0)
      index = 1;
    if (strItems[1].Find("NANO") == 0)
      index = 0;
    if (index < 0 || index > 1)
      ABORT_LINE("Probe mode must be 0, 1, nano, or micro in statement: \n\n");
    if (!mScope->SetProbeMode(index)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;
    
  case CME_DELAY:                                           // Delay
    delISX = itemDbl[1];
    if (!strItems[2].CompareNoCase("MSEC"))
      mSleepTime = delISX;
    else if (!strItems[2].CompareNoCase("SEC"))
      mSleepTime = 1000. * delISX;
    else if (!strItems[2].CompareNoCase("MIN"))
      mSleepTime = 60000. * delISX;
    else if (delISX > 60)
      mSleepTime = delISX;
    else
      mSleepTime = 1000. * delISX;
    mSleepStart = GetTickCount();
    if (mSleepTime > 3000)
      mWinApp->SetStatusText(SIMPLE_PANE, "WAITING FOR DELAY");
    break;
    
  case CME_WAITFORMIDNIGHT:                                 // WaitForMidnight
  {
    // Get the times before and after the target time and the alternative target
    delX = delY = 5.;
    if (!itemEmpty[1])
      delX = itemDbl[1];
    if (!itemEmpty[2])
      delY = itemDbl[2];
    ix0 = 24;
    ix1 = 0;
    if (!itemEmpty[3] && !itemEmpty[4]) {
      ix0 = itemInt[3];
      ix1 = itemInt[4];
    }

    // Compute minutes into the day and the interval from now to the target
    CTime time = CTime::GetCurrentTime();
    delISX = time.GetHour() * 60 + time.GetMinute() + time.GetSecond() / 60.;
    delISY = ix0 * 60 + ix1 - delISX;

    // If wthin the window at all, set up the sleep
    if (delISY + delY > 0 && delISY < delX) {
      mSleepTime = 60000. * (delISY + delY);
      mSleepStart = GetTickCount();
      report.Format("Sleeping until %.1f minutes after ", delY);
      strCopy = "midnight";
      if (!itemEmpty[4])
        strCopy.Format("%02d:%02d", ix0, ix1);
      mWinApp->AppendToLog(report + strCopy, mLogAction);
      strCopy.MakeUpper();
      report.Format(" + %.0f", delY);
      mWinApp->SetStatusText(SIMPLE_PANE, "WAIT TO " + strCopy);
    }
    break;
  }
    
  case CME_WAITFORDOSE:                                     // WaitForDose
    mDoseTarget = itemDbl[1];
    mNumDoseReports = 10;
    if (!itemEmpty[2])
      mNumDoseReports = B3DMAX(1, itemInt[2]);
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
    index = mScope->GetScreenPos() == spDown ? 1 : 0;
    report.Format("Screen is %s", index ? "DOWN" : "UP");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index);
    break;

  case CME_REPORTSCREENCURRENT:                                    // ReportScreenCurrent
    delX = mScope->GetScreenCurrent();
    report.Format("Screen current is %.3f nA", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_IMAGESHIFTBYPIXELS:                              // ImageShiftByPixels
    bInv = mShiftManager->CameraToIS(mScope->GetMagIndex());
    ix1 = BinDivisorI(camParams);
    index = B3DNINT(itemDbl[1] * ix1);
    index2 = B3DNINT(itemDbl[2] * ix1);

    delISX = bInv.xpx * index + bInv.xpy * index2;
    delISY = bInv.ypx * index + bInv.ypy * index2;
    if (AdjustBeamTiltIfSelected(delISX, delISY, !itemEmpty[4] && itemInt[4], report))
      ABORT_LINE(report);

    // Make the change in image shift
    mScope->IncImageShift(delISX, delISY);
    if (!itemEmpty[3] && itemDbl[3] > 0)
      mShiftManager->SetISTimeOut((float)itemDbl[3] * mShiftManager->GetLastISDelay());
    break;
    
  case CME_IMAGESHIFTBYUNITS:                               // ImageShiftByUnits
    delISX = itemDbl[1];
    delISY = itemDbl[2];
    if (AdjustBeamTiltIfSelected(delISX, delISY, !itemEmpty[4] && itemInt[4], report))
      ABORT_LINE(report);

    // Make the change in image shift
    mScope->IncImageShift(delISX, delISY);
    if (!itemEmpty[3] && itemDbl[3] > 0)
      mShiftManager->SetISTimeOut((float)itemDbl[3] * mShiftManager->GetLastISDelay());

    // Report distance on specimen
    aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    delX = aMat.xpx * delISX + aMat.xpy * delISY;
    delY = aMat.ypx * delISX + aMat.ypy * delISY;
    specDist = 1000. * sqrt(delX * delX + delY * delY);
    strCopy.Format("%.1f nm shifted on specimen", specDist);
    mWinApp->AppendToLog(strCopy, LOG_OPEN_IF_CLOSED);
    break;
    
  case CME_IMAGESHIFTBYMICRONS:                             // ImageShiftByMicrons
    delX = itemDbl[1];
    delY = itemDbl[2];
    aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    bInv = mShiftManager->MatInv(aMat);
    delISX = bInv.xpx * delX + bInv.xpy * delY;
    delISY = bInv.ypx * delX + bInv.ypy * delY;
    if (AdjustBeamTiltIfSelected(delISX, delISY, !itemEmpty[4] && itemInt[4], report))
      ABORT_LINE(report);
    mScope->IncImageShift(delISX, delISY);
    if (!itemEmpty[3] && itemDbl[3] > 0)
      mShiftManager->SetISTimeOut((float)itemDbl[3] * mShiftManager->GetLastISDelay());
    break;

  case CME_IMAGESHIFTTOLASTMULTIHOLE:                       // ImageShiftToLastMultiHole
    mWinApp->mParticleTasks->GetLastHoleImageShift(backlashX, backlashY);
    mScope->IncImageShift(backlashX, backlashY);
    break;
    
  case CME_SHIFTIMAGEFORDRIFT:                              // ShiftImageForDrift
    mCamera->QueueDriftRate(itemDbl[1], itemDbl[2], itemInt[3] != 0);
    break;
    
  case CME_SHIFTCALSKIPLENSNORM:                            // ShiftCalSkipLensNorm
    mWinApp->mShiftCalibrator->SetSkipLensNormNextIScal(itemEmpty[1] || itemInt[1] != 0);
    break;
    
  case CME_CALIBRATEIMAGESHIFT:                             // CalibrateImageShift
    index = 0;
    if (!itemEmpty[1] && itemInt[1])
      index = -1;
    mWinApp->mShiftCalibrator->CalibrateIS(index, false, true);
    break;
    
  case CME_REPORTFOCUSDRIFT:                                // ReportFocusDrift
    if (mWinApp->mFocusManager->GetLastDrift(delX, delY))
      ABORT_LINE("No drift available from last autofocus for statement: \n\n");
    strCopy.Format("Last drift in autofocus: %.3f %.3f nm/sec", delX, delY);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX, delY);
    break;
    
  case CME_TESTSTEMSHIFT:                                   // TestSTEMshift
    mScope->TestSTEMshift(itemInt[1], itemInt[2], itemInt[3]);
    break;
    
  case CME_QUICKFLYBACK:                                    // QuickFlyback
    index = CString("VFTRP").Find(strItems[1].Left(1));
    if (index < 0)
      ABORT_NOLINE("QuickFlyback must be followed by one of V, F, T, R, or P");
    if (!(camParams->STEMcamera && camParams->FEItype))
      ABORT_NOLINE("QuickFlyback can be run only if the current camera is an FEI STEM"
      " camera")
    mWinApp->mCalibTiming->CalibrateTiming(index, (float)itemDbl[2], false);
    break;
    
  case CME_REPORTAUTOFOCUS:                                 // ReportAutoFocus
    delX = mWinApp->mFocusManager->GetCurrentDefocus();
    index = mWinApp->mFocusManager->GetLastFailed() ? -1 : 0;
    index2 = mWinApp->mFocusManager->GetLastAborted();
    if (index2)
      index = index2;
    if (index)
      strCopy.Format("Last autofocus FAILED with error type %d", index);
    else
      strCopy.Format("Last defocus in autofocus: %.2f um", delX);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX, (double)index);
    break;
    
  case CME_REPORTTARGETDEFOCUS:                             // ReportTargetDefocus
    delX = mWinApp->mFocusManager->GetTargetDefocus();
    strCopy.Format("Target defocus is: %.2f um", delX);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_SETBEAMSHIFT:                                    // SetBeamShift
    delX = itemDbl[1];
    delY = itemDbl[2];
    if (!mScope->SetBeamShift(delX, delY)) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_MOVEBEAMBYMICRONS:                               // MoveBeamByMicrons
    if (mWinApp->mProcessImage->MoveBeam(NULL, (float)itemDbl[1], (float)itemDbl[2]))
      ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
      " line:\n\n");
    break;
    
  case CME_MOVEBEAMBYFIELDFRACTION:                         // MoveBeamByFieldFraction
    if (mWinApp->mProcessImage->MoveBeamByCameraFraction((float)itemDbl[1], 
      (float)itemDbl[2]))
      ABORT_LINE("Either an image shift or a beam shift calibration is not available for"
      " line:\n\n");
    break;
    
  case CME_SETBEAMTILT:                                     // SetBeamTilt
    if (!mScope->SetBeamTilt(itemDbl[1], itemDbl[2])) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_REPORTBEAMSHIFT:                                 // ReportBeamShift
    if (!mScope->GetBeamShift(delX, delY)) {
      AbortMacro();
      return;
    }
    strCopy.Format("Beam shift %.3f %.3f (putative microns)", delX, delY);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX, delY);
    break;
    
  case CME_REPORTBEAMTILT:                                  // ReportBeamTilt
    if (!mScope->GetBeamTilt(delX, delY)) {
      AbortMacro();
      return;
    }
    strCopy.Format("Beam tilt %.3f %.3f", delX, delY);
    mWinApp->AppendToLog(strCopy, mLogAction);
    SetReportedValues(&strItems[1], delX, delY);
    break;
    
  case CME_SETIMAGESHIFT:                                   // SetImageShift
    delX = itemDbl[1];
    delY = itemDbl[2];
    truth = !itemEmpty[4] && itemInt[4];
    if (truth)
      mScope->GetLDCenteredShift(delISX, delISY);
    if (!mScope->SetLDCenteredShift(delX, delY)) {
      AbortMacro();
      return;
    }
    if (AdjustBeamTiltIfSelected(delX - delISX, delY - delISY, truth, report))
      ABORT_LINE(report);
    if (!itemEmpty[3] && itemDbl[3] > 0)
      mShiftManager->SetISTimeOut((float)itemDbl[3] * mShiftManager->GetLastISDelay());
    break;

  case CME_ADJUSTBEAMTILTFORIS:                             // AdjustBeamTiltforIS
    if (!itemEmpty[1] && itemEmpty[2])
      ABORT_LINE("There must be either no entries or X and Y IS entries for line:\n\n");
    if (!itemEmpty[2]) {
      delISX = itemDbl[1];
      delISY = itemDbl[2];
    } else
      mScope->GetLDCenteredShift(delISX, delISY);
    if (AdjustBeamTiltIfSelected(delISX, delISY, true, report))
      ABORT_LINE(report);
    break;
    
  case CME_REPORTIMAGESHIFT:                                // ReportImageShift
    if (!mScope->GetLDCenteredShift(delX, delY)) {
      AbortMacro();
      return;
    }
    strCopy.Format("Image shift %.3f %.3f IS units", delX, delY);
    mag = mScope->GetMagIndex();
    aMat = mShiftManager->IStoCamera(mag);
    delISX = delISY = stageX = stageY = 0.;
    if (aMat.xpx) {
      index = BinDivisorI(camParams);
      delISX = -(delX * aMat.xpx + delY * aMat.xpy) / index;
      delISY = -(delX * aMat.ypx + delY * aMat.ypy) / index;
      h1 = cos(DTOR * mScope->GetTiltAngle());
      bInv = MatMul(aMat, 
        MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), mag)));
      stageX = (bInv.xpx * delX + bInv.xpy * delY) / (HitachiScope ? h1 : 1.);
      stageY = (bInv.ypx * delX + bInv.ypy * delY) / (HitachiScope ? 1. : h1);
      report.Format("   %.1f %.1f unbinned pixels; need stage %.3f %.3f if reset", delISX,
                    delISY, stageX, stageY);
      strCopy += report;
    }
    SetReportedValues(&strItems[1], delX, delY, delISX, delISY, stageX, stageY);
    mWinApp->AppendToLog(strCopy, mLogAction);
    break;

  case CME_SETOBJECTIVESTIGMATOR:                           // SetObjectiveStigmator
    delX = itemDbl[1];
    delY = itemDbl[2];
    if (!mScope->SetObjectiveStigmator(delX, delY)) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_REPORTSPECIMENSHIFT:                             // ReportSpecimenShift
    if (!mScope->GetLDCenteredShift(delISX, delISY)) {
      AbortMacro();
      return;
    }
    aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    if (aMat.xpx) {
      delX = aMat.xpx * delISX + aMat.xpy * delISY;
      delY = aMat.ypx * delISX + aMat.ypy * delISY;
      report.Format("Image shift is %.3f  %.3f in specimen coordinates",  delX, delY);
      SetReportedValues(&strItems[1], delX, delY);
      mWinApp->AppendToLog(report, mLogAction);
    } else {
      mWinApp->AppendToLog("There is no calibration for converting image shift to "
        "specimen coordinates", mLogAction);
    }
    break;
    
  case CME_REPORTOBJECTIVESTIGMATOR:                        // ReportObjectiveStigmator
   if (!mScope->GetObjectiveStigmator(delX, delY)) {
     AbortMacro();
     return;
   }
   strCopy.Format("Objective stigmator is %.5f %.5f", delX, delY);
   mWinApp->AppendToLog(strCopy, mLogAction);
   SetReportedValues(&strItems[1], delX, delY);
   break;
   
  case CME_SUPPRESSREPORTS:                                 // SuppressReports
    if (!itemEmpty[1] && !itemInt[1])
      mLogAction = LOG_OPEN_IF_CLOSED;
    else
      mLogAction = LOG_IGNORE;
    break;
    
  case CME_ERRORSTOLOG:                                     // ErrorsToLog
    if (!itemEmpty[1] && !itemInt[1])
      mLogErrAction = LOG_IGNORE;
    else
      mLogErrAction = LOG_OPEN_IF_CLOSED;
    break;

                            
  case CME_REPORTALIGNSHIFT:                                // ReportAlignShift
  case CME_REPORTSHIFTDIFFFROM:                             // ReportShiftDiffFrom
    delISY = 0.;
    if (CMD_IS(REPORTSHIFTDIFFFROM)) {                      
      delISY = itemDbl[1];
      if (itemEmpty[1] || delISY <= 0)
        ABORT_LINE("Improper or missing comparison value in statement: \n\n");
    }
    if (mImBufs->mImage) {
      float shiftX, shiftY;
      mImBufs->mImage->getShifts(shiftX, shiftY);
      shiftX *= mImBufs->mBinning;
      shiftY *= -mImBufs->mBinning;
      mAccumShiftX += shiftX;
      mAccumShiftY += shiftY;
      index = mScope->GetMagIndex();
      index2 = BinDivisorI(camParams);
      if (delISY) {
        delX = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), index);
        delY = delX * sqrt(shiftX * shiftX + shiftY * shiftY);
        delISX = 100. * (delY / delISY - 1.);
        mAccumDiff += (float)(delY - delISY);
        delISY = delX * sqrt(mAccumShiftX * mAccumShiftX + mAccumShiftY * mAccumShiftY);
        strCopy.Format("%6.1f %% diff, cumulative diff %6.2f  um, total distance %.1f",
          delISX, mAccumDiff, delISY);
        SetReportedValues(&strItems[2], delISX, mAccumDiff, delISY);
      } else {
        bInv = mShiftManager->CameraToIS(index);
        aMat = mShiftManager->IStoSpecimen(index);

                   // Convert to image shift units, then determine distance on specimen
                   // implied by each axis of image shift separately
        delISX = bInv.xpx * shiftX + bInv.xpy * shiftY;
        delISY = bInv.ypx * shiftX + bInv.ypy * shiftY;
        h1 = 1000. * (delISX * aMat.xpx + delISY * aMat.xpy);
        v1 = 1000. * (delISX * aMat.ypx + delISY * aMat.ypy);
        delX = 1000. * sqrt(pow(delISX * aMat.xpx, 2) + pow(delISX * aMat.ypx, 2));
        delY = 1000. * sqrt(pow(delISY * aMat.xpy, 2) + pow(delISY * aMat.ypy, 2));
        strCopy.Format("%6.1f %6.1f unbinned pixels; %6.1f %6.1f nm along two shift axes;"
          " %6.1f %6.1f nm on specimen axes",
          shiftX / index2, shiftY / index2, delX, delY, h1, v1);
        SetReportedValues(&strItems[1], shiftX, shiftY, delX, delY, h1, v1);
      }
      mWinApp->AppendToLog(strCopy, mLogAction);
    }
    break;
    
  case CME_REPORTACCUMSHIFT:                                // ReportAccumShift
    index2 = BinDivisorI(camParams);
    report.Format("%8.1f %8.1f cumulative pixels", mAccumShiftX / index2,
      mAccumShiftY / index2);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], mAccumShiftX, mAccumShiftY);
    break;
    
  case CME_RESETACCUMSHIFT:                                 // ResetAccumShift
    mAccumShiftX = 0.;
    mAccumShiftY = 0.;
    mAccumDiff = 0.;
    break;
    
  case CME_REPORTALIGNTRIMMING:                             // ReportAlignTrimming
    mShiftManager->GetLastAlignTrims(ix0, ix1, iy0, iy1);
    report.Format("Total border trimmed in last autoalign in X & Y for A: %d %d   "
      "Reference: %d %d", ix0, ix1, iy0, iy1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], ix0, ix1, iy0, iy1);
    break;
    
  case CME_REPORTCLOCK:                                     // ReportClock
    delX = 0.001 * GetTickCount() - 0.001 * mStartClock;
    report.Format("%.2f seconds elapsed time", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_RESETCLOCK:                                      // ResetClock
    mStartClock = GetTickCount();
    break;
    
  case CME_REPORTMINUTETIME:                                // ReportMinuteTime
    index = mWinApp->MinuteTimeStamp();
    report.Format("Absolute minute time = %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index);
    if (!itemEmpty[1] && SetVariable(strItems[1], (double)index, VARTYPE_PERSIST, -1,
      false, &report))
        ABORT_LINE(report + "\n(This command assigns to a persistent variable):\n\n");
    break;
    
  case CME_SETCUSTOMTIME:                                   // SetCustomTime
  case CME_REPORTCUSTOMINTERVAL:                            // ReportCustomInterval
  {
    std::string sstr = (LPCTSTR)item1upper;
    std::map<std::string, int>::iterator custIter;
    index = mWinApp->MinuteTimeStamp();
    if (!itemEmpty[2])
      index = itemInt[2];
    index2 = (int)mCustomTimeMap.count(sstr);
    if (index2)
      custIter = mCustomTimeMap.find(sstr);
    if (CMD_IS(SETCUSTOMTIME)) {

      // Insert does not replace a value!  You have to get the iterator and assign it
      if (index2)
        custIter->second = index;
      else
        mCustomTimeMap.insert(std::pair<std::string, int>(sstr, index));
      mWinApp->mDocWnd->SetShortTermNotSaved();
    } else {
      if (index2) {
        index -= custIter->second;
        report.Format("%d minutes elapsed since custom time %s set", index, (LPCTSTR)strItems[1]);
      } else {
        index = 2 * MAX_CUSTOM_INTERVAL;
        report = "Custom time " + strItems[1] + " has not been set";
      }
      mWinApp->AppendToLog(report, mLogAction);
      SetReportedValues(index);
    }
    break;
  }

  case CME_REPORTTICKTIME:                                  // ReportTickTime
    delISX = SEMTickInterval(mWinApp->ProgramStartTime()) / 1000.;
    report.Format("Tick time from program start = %.3f", delISX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(delISX);
    if (!itemEmpty[1] && SetVariable(strItems[1], delISX, VARTYPE_PERSIST, -1,
      false, &report))
        ABORT_LINE(report + "\n(This command assigns to a persistent variable):\n\n");
    break;
    
  case CME_ELAPSEDTICKTIME:                                 // ElapsedTickTime
    delISY = SEMTickInterval(mWinApp->ProgramStartTime());
    delISX = SEMTickInterval(delISY, itemDbl[1] * 1000.) / 1000.;
    report.Format("Elapsed tick time = %.3f", delISX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], delISX);
    break;

  case CME_REPORTDATETIME:                                  // ReportDateTime
  {
    CTime ctDateTime = CTime::GetCurrentTime();
    index = 10000 * ctDateTime.GetYear() + 100 * ctDateTime.GetMonth() +
      ctDateTime.GetDay();
    index2 = 100 * ctDateTime.GetHour() + ctDateTime.GetMinute();
    report.Format("%d  %04d", index, index2);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index, index2);
    break;
  }

  case CME_MOVESTAGE:                                       // MoveStage
  case CME_MOVESTAGETO:                                     // MoveStageTo 
  case CME_TESTRELAXINGSTAGE:                               // TestRelaxingStage
  case CME_STAGESHIFTBYPIXELS:                              // StageShiftByPixels
  case CME_STAGETOLASTMULTIHOLE:                            // StageToLastMultiHole
      smi.z = 0.;
      smi.alpha = 0.;
      smi.axisBits = 0;
      smi.backX = smi.backY = smi.relaxX = smi.relaxY = 0.;
      truth = CMD_IS(TESTRELAXINGSTAGE);

      // If stage not ready, back up and try again, otherwise do action
      if (mScope->StageBusy() > 0)
        mLastIndex = mCurrentIndex;
      else {
        if (CMD_IS(STAGESHIFTBYPIXELS)) {
          h1 = DTOR * mScope->GetTiltAngle();
          aMat = mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(),
            mScope->GetMagIndex());
          if (!aMat.xpx)
            ABORT_LINE("There is no stage to camera calibration available for line:\n\n");
          bInv = MatInv(aMat);
          stageX = bInv.xpx * itemDbl[1] + bInv.xpy * itemDbl[2];
          stageY = (bInv.ypx * itemDbl[1] + bInv.ypy * itemDbl[2]) / cos(h1);
          stageZ = 0.;
        } else if (CMD_IS(STAGETOLASTMULTIHOLE)) {
          mWinApp->mParticleTasks->GetLastHoleStagePos(stageX, stageY);
          if (stageX < EXTRA_VALUE_TEST)
            ABORT_LINE("The multiple Record routine has not been run for line:\n\n");
          stageZ = 0.;
        } else {

          if (itemEmpty[2])
            ABORT_LINE("Stage movement command does not have at least 2 numbers: \n\n");
          stageX = itemDbl[1];
          stageY = itemDbl[2];
          stageZ = (itemEmpty[3] || truth) ? 0. : itemDbl[3];
        }
        if (CMD_IS(MOVESTAGE) || CMD_IS(STAGESHIFTBYPIXELS) || truth) {
          if (!mScope->GetStagePosition(smi.x, smi.y, smi.z))
            SUSPEND_NOLINE("because of failure to get stage position");
            //CString report;
          //report.Format("Start at %.2f %.2f %.2f", smi.x, smi.y, smi.z);
          //mWinApp->AppendToLog(report, LOG_IF_ADMIN_OPEN_IF_CLOSED);

          // For each of X, Y, Z, set axis bit if nonzero;
          smi.x += stageX;
          if (stageX != 0.)
            smi.axisBits |= axisX;
          smi.y += stageY;
          if (stageY != 0.)
            smi.axisBits |= axisY;
          smi.z += stageZ;
          if (stageZ != 0.)
            smi.axisBits |= axisZ;
          if (truth) {
            backlashX = itemEmpty[3] ? mWinApp->mMontageController->GetStageBacklash() :
              (float)itemDbl[3];
          backlashY = itemEmpty[4] ? mScope->GetStageRelaxation() : (float)itemDbl[4];
          if (backlashY <= 0)
            backlashY = 0.025f;

          // Set relaxation in the direction of backlash
          if (stageX) {
            smi.backX = (stageX > 0. ? backlashX : -backlashX);
            smi.relaxX = (smi.backX > 0. ? backlashY : -backlashY);
          }
          if (stageY) {
            smi.backY = (stageY > 0. ? backlashX : -backlashX);
            smi.relaxY = (smi.backY > 0. ? backlashY : -backlashY);
          }
          }
        } else {

          // Absolute move: set to values; set Z axis bit if entered
          smi.x = stageX;
          smi.y = stageY;
          smi.z = stageZ;

          smi.axisBits |= (axisX | axisY);
          if (!itemEmpty[3] && !CMD_IS(STAGETOLASTMULTIHOLE))
            smi.axisBits |= axisZ;
        }

        // Start the movement
        if (smi.axisBits) {
          if (!mScope->MoveStage(smi, truth && backlashX != 0., false, false, truth))
            SUSPEND_NOLINE("because of failure to start stage movement");
          mMovedStage = true;
        }
      }
    break;

  case CME_RELAXSTAGE:                                      // RelaxStage
    delX = itemEmpty[1] ? mScope->GetStageRelaxation() : itemDbl[1];
    if (!mScope->GetStagePosition(smi.x, smi.y, smi.z))
      SUSPEND_NOLINE("because of failure to get stage position");
    if (mScope->GetValidXYbacklash(smi.x, smi.y, backlashX, backlashY)) {
      mScope->CopyBacklashValid();

      // Move in direction of the backlash, which is opposite to direction of stage move
      smi.x += (backlashX > 0. ? delX : -delX);
      smi.y += (backlashY > 0. ? delX : -delX);
      smi.z = 0.;
      smi.alpha = 0.;
      smi.axisBits = axisXY;
      mScope->MoveStage(smi);
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
      smi.alpha = itemDbl[1];
      delX = 1.;
      smi.useSpeed = false;
      if (!itemEmpty[2]) {
        smi.speed = itemDbl[2];
        if (smi.speed <= 0.)
          ABORT_LINE("Speed entry must be positive in line:/n/n");
        smi.useSpeed = true;
      }
      smi.axisBits = axisA;
      if (!mScope->MoveStage(smi, false, smi.useSpeed, true)) {
        AbortMacro();
        return;
      }
    }
    break;
    
  case CME_REPORTSTAGEXYZ:                                  // ReportStageXYZ
    mScope->GetStagePosition(stageX, stageY, stageZ);
    report.Format("Stage %.2f %.2f %.2f", stageX, stageY, stageZ);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], stageX, stageY, stageZ);
    break;
    
  case CME_REPORTTILTANGLE:                                 // ReportTiltAngle
    delX = mScope->GetTiltAngle();
    report.Format("%.2f degrees", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_REPORTSTAGEBUSY:                                 // ReportStageBusy
    index = mScope->StageBusy();
    report.Format("Stage is %s", index > 0 ? "BUSY" : "NOT busy");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index > 0 ? 1 : 0);
    break;
    
  case CME_REPORTSTAGEBAXIS:                                // ReportStageBAxis
    delX = mScope->GetStageBAxis();
    report.Format("B axis = %.2f degrees", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_REPORTMAG:                                       // ReportMag
    index2 = mScope->GetMagIndex();

    // This is not right if the screen is down and FEI is not in EFTEM
    index = MagOrEFTEMmag(mWinApp->GetEFTEMMode(), index2, mScope->GetSTEMmode());
    report.Format("Mag is %d%s", index,
      index2 < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, 
      index2 < mScope->GetLowestNonLMmag() ? 1. : 0.);
    break;
    
  case CME_REPORTMAGINDEX:                                  // ReportMagIndex
    index = mScope->GetMagIndex();
    report.Format("Mag index is %d%s", index,
      index < mScope->GetLowestNonLMmag() ? "   (Low mag)":"");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, 
      index < mScope->GetLowestNonLMmag() ? 1. : 0.);
    break;
    
  case CME_REPORTCAMERALENGTH:                              // ReportCameraLength
    delX = 0;
    if (!mScope->GetMagIndex())
      delX = mScope->GetLastCameraLength();
    report.Format("%s %g%s", delX ? "Camera length is " : "Not in diffraction mode - (",
      delX, delX ? " m" : ")");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1],  delX);
    break;
    
  case CME_REPORTDEFOCUS:                                   // ReportDefocus
    delX = mScope->GetDefocus();
    report.Format("Defocus = %.3f um", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_REPORTFOCUS:                                     // ReportFocus
  case CME_REPORTABSOLUTEFOCUS:                             // ReportAbsoluteFocus
    delX = mScope->GetFocus();
    report.Format("Absolute focus = %.5f", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;

  case CME_REPORTPERCENTC2:                                 // ReportPercentC2
    delY = mScope->GetIntensity();
    delX = mScope->GetC2Percent(mScope->FastSpotSize(), delY);
    report.Format("%s = %.3f%s  -  %.5f", mScope->GetC2Name(), delX, mScope->GetC2Units(),
      delY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX, delY);
    break;

  case CME_REPORTCROSSOVERPERCENTC2:                                 // ReportCrossoverPercentC2
    delY = mScope->GetCrossover(mScope->GetSpotSize()); // probe mode not required, uses current mode it
    delX = mScope->GetC2Percent(mScope->FastSpotSize(), delY);
    report.Format("Crossover %s at current conditions = %.3f%s  -  %.5f", mScope->GetC2Name(), delX, mScope->GetC2Units(), delY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX, delY);
    break;

  case CME_REPORTILLUMINATEDAREA:                           // ReportIlluminatedArea
    delX = mScope->GetIlluminatedArea();
    report.Format("IlluminatedArea %f", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_REPORTIMAGEDISTANCEOFFSET:                       // ReportImageDistanceOffset
    delX = mScope->GetImageDistanceOffset();
    report.Format("Image distance offset %f", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_REPORTALPHA:                                     // ReportAlpha
    index = mScope->GetAlpha() + 1;
    report.Format("Alpha = %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_REPORTSPOTSIZE:                                  // ReportSpotSize
    index = mScope->GetSpotSize();
    report.Format("Spot size is %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_REPORTPROBEMODE:                                 // ReportProbeMode
    index = mScope->ReadProbeMode();
    report.Format("Probe mode is %s", index ? "microprobe" : "nanoprobe");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_REPORTENERGYFILTER:                              // ReportEnergyFilter
    if (!mWinApp->ScopeHasFilter())
      ABORT_NOLINE("You cannot use ReportEnergyFilter; there is no filter");
    if (mScope->GetHasOmegaFilter())
      mScope->updateEFTEMSpectroscopy(truth);
    else if (mCamera->CheckFilterSettings())
      ABORT_NOLINE("An error occurred checking filter settings");
    delX = filtParam->zeroLoss ? 0. : filtParam->energyLoss;
    report.Format("Filter slit width %.1f, energy loss %.1f, slit %s", 
      filtParam->slitWidth, delX, filtParam->slitIn ? "IN" : "OUT");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], filtParam->slitWidth, delX,
      filtParam->slitIn ? 1. : 0.);
    break;
    
  case CME_REPORTCOLUMNMODE:                                // ReportColumnMode
    if (!mScope->GetColumnMode(index, index2)) {
      AbortMacro();
      return;
    }
    report.Format("Column mode %d (%X), submode %d (%X)", index, index, index2, index2);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, (double)index2);
    break;
    
  case CME_REPORTLENS:                                      // ReportLens
    mWinApp->mParamIO->StripItems(strLine, 1, report);
    if (!mScope->GetLensByName(report, delX)) {
      AbortMacro();
      return;
    }
    report.Format("Lens %s = %f", (LPCTSTR)report, delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(delX);
    break;
    
  case CME_REPORTCOIL:                                      // ReportCoil
    if (!mScope->GetDeflectorByName(strItems[1], delX, delY)) {
      AbortMacro();
      return;
    }
    report.Format("Coil %s = %f  %f", (LPCTSTR)strItems[1], delX, delY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], delX, delY);
    break;
    
  case CME_SETFREELENSCONTROL:                              // SetFreeLensControl
    if (!mScope->SetFreeLensControl(itemInt[1], itemInt[2]))
      ABORT_LINE("Error trying to run:\n\n");
    break;
    
  case CME_SETLENSWITHFLC:                                  // SetLensWithFLC
    if (!mScope->SetLensWithFLC(itemInt[1], itemDbl[2], !itemEmpty[3] && itemInt[3] != 0))
      ABORT_LINE("Error trying to run:\n\n");
    break;
    
  case CME_REPORTLENSFLCSTATUS:                            // ReportLensFLCStatus
    if (!mScope->GetLensFLCStatus(itemInt[1], index, delX))
      ABORT_LINE("Error trying to run:\n\n");
    report.Format("Lens %d, FLC %s  value  %f", itemInt[1], index ? "ON" : "OFF", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], index, delX);
    break;
    
  case CME_SETJEOLSTEMFLAGS:                                // SetJeolSTEMflags
    if (itemInt[1] < 0 || itemInt[1] > 0xFFFFFF || itemInt[2] < 0 || itemInt[2] > 15)
        ABORT_LINE("Entries must fit in 24 and 4 bits in: \n\n");
    mCamera->SetBaseJeolSTEMflags(itemInt[1] + (itemInt[2] << 24));

    break;
    
  case CME_REMOVEAPERTURE:                                  // RemoveAperture
  case CME_REINSERTAPERTURE:                                // ReInsertAperture
    index = itemInt[1];
    if (strItems[1] == "CL")
      index = 1;
    if (strItems[1] == "OL")
      index = 2;
    if (CMD_IS(REINSERTAPERTURE))
      index2 = mScope->ReInsertAperture(index);
    else
      index2 = mScope->RemoveAperture(index);
    if (index2)
      ABORT_LINE("Script aborted due to error starting aperture movement in:\n\n");
    mMovedAperture = true;
    break;

  case CME_PHASEPLATETONEXTPOS:                             // PhasePlateToNextPos
    if (!mScope->MovePhasePlateToNextPos())
      ABORT_LINE("Script aborted due to error starting phase plate movement in:\n\n");
    mMovedAperture = true;
    break;
    
  case CME_REPORTPHASEPLATEPOS:                             // ReportPhasePlatePos
    index = mScope->GetCurrentPhasePlatePos();
    if (index < 0)
      ABORT_LINE("Script aborted due to error in:\n\n");
    report.Format("Current phase plate position is %d", index + 1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index + 1.);
    break;
    
  case CME_REPORTMEANCOUNTS:                                // ReportMeanCounts
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    imBuf = &mImBufs[index];
    delX = mWinApp->mProcessImage->WholeImageMean(imBuf);
    if (strItems[2] == "1" && imBuf->mBinning && imBuf->mExposure > 0.) {
      delX /= imBuf->mBinning * imBuf->mBinning * imBuf->mExposure *
        mWinApp->GetGainFactor(imBuf->mCamera, imBuf->mBinning);
      report.Format("Mean of buffer %s = %.2f unbinned counts/sec", strItems[1], delX);
    } else
      report.Format("Mean of buffer %s = %.1f", strItems[1], delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(delX);
    break;
    
  case CME_REPORTFILEZSIZE:                                 // ReportFileZsize
    if (!mWinApp->mStoreMRC)
      SUSPEND_LINE("because there is no open image file to report for line: \n\n");
    if (mBufferManager->CheckAsyncSaving())
      SUSPEND_NOLINE("because of file write error");
    index = mWinApp->mStoreMRC->getDepth();
    report.Format("Z size of file = %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_SUBAREAMEAN:                                     // SubareaMean
    ix0 = itemInt[1];
    ix1 = itemInt[2];
    iy0 = itemInt[3];
    iy1 = itemInt[4];
    if (mImBufs->mImage)
      mImBufs->mImage->getSize(sizeX, sizeY);
    if (!mImBufs->mImage || itemEmpty[4] || ix0 < 0 || ix1 >= sizeX || ix1 < ix0
      || iy0 < 0 || iy1 >= sizeY || iy1 < iy0)
        ABORT_LINE("Not enough coordinates, coordinates out of bounds, or no image "
          "in A in statement: \n\n");
    delX = ProcImageMean(mImBufs->mImage->getData(), mImBufs->mImage->getType(),
      sizeX, sizeY, ix0, ix1, iy0, iy1);
    report.Format("Mean of subarea (%d, %d) to (%d, %d) = %.2f", ix0, iy0, ix1, iy1,
      delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[5], delX);
    break;
    
  case CME_ELECTRONSTATS:                                   // ElectronStats
  case CME_RAWELECTRONSTATS:                                // RawElectronStats
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    delX = mImBufs[index].mBinning;
    if (mImBufs[index].mCamera < 0 || !delX || !mImBufs[index].mExposure)
      ABORT_LINE("Image buffer does not have enough information for dose statistics in:"
      "\r\n\r\n");
    cpe = mWinApp->mProcessImage->CountsPerElectronForImBuf(&mImBufs[index]);
    if (!cpe)
      ABORT_LINE("Camera does not have a CountsPerElectron property for:\r\n\r\n");
    image = mImBufs[index].mImage;
    image->getSize(sizeX, sizeY);
    ProcMinMaxMeanSD(image->getData(), image->getType(), sizeX, sizeY, 0,
      sizeX - 1, 0, sizeY - 1, &bmean, &bmin, &bmax, &bSD);
    camParams = mWinApp->GetCamParams() + mImBufs[index].mCamera;
    if (CamHasDoubledBinnings(camParams))
      delX /= 2;
    bmin /= cpe;
    bmax /= cpe;
    bmean /= cpe;
    bSD /= cpe;
    backlashX = (float)(bmean / (mImBufs[index].mExposure * delX * delX));
    backlashY = backlashX;
    if (CMD_IS(ELECTRONSTATS) && mImBufs[index].mK2ReadMode > 0)
      backlashY = mWinApp->mProcessImage->LinearizedDoseRate(mImBufs[index].mCamera, 
        backlashX);
    if (mImBufs[index].mDoseRatePerUBPix > 0.) {
      SEMTrace('1', "Dose rate computed from mean %.3f  returned from DM %.3f", backlashY,
        mImBufs[index].mDoseRatePerUBPix);
      backlashY = mImBufs[index].mDoseRatePerUBPix;
    }
    shiftX = backlashY / backlashX;
    report.Format("Min = %.3f  max = %.3f  mean = %.3f  SD = %.3f electrons/pixel; "
      "dose rate = %.3f e/unbinned pixel/sec", bmin * shiftX, bmax * shiftX, 
      bmean * shiftX, bSD * shiftX, backlashY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], bmin * shiftX, bmax * shiftX, bmean * shiftX, 
      bSD * shiftX, backlashY);
    break;

  case CME_CROPIMAGE:                                       // CropImage
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    ix0 = itemInt[2];
    ix1 = itemInt[3];
    iy0 = itemInt[4];
    iy1 = itemInt[5];
    ix0 = mWinApp->mProcessImage->CropImage(&mImBufs[index], iy0, ix0, iy1, ix1);
    if (ix0) {
      report.Format("Error # %d attempting to crop image in buffer %c in statement: \n\n"
        ,ix0, strItems[1].GetAt(0));
      ABORT_LINE(report);
    }
    break;
    
  case CME_REDUCEIMAGE:                                     // ReduceImage
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    ix0 = mWinApp->mProcessImage->ReduceImage(&mImBufs[index], (float)itemDbl[2], 
      &report);
    if (ix0) {
      report += " in statement:\n\n";
      ABORT_LINE(report);
    }
    break;
    
  case CME_FFT:                                             // FFT
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    index2 = 1;
    if (!itemEmpty[2])
      index2 = itemInt[2];
    if (index2 < 1 || index2 > 8)
      ABORT_LINE("Binning must be between 1 and 8 in line:\n\n");
    mWinApp->mProcessImage->GetFFT(&mImBufs[index], index2, BUFFER_FFT);
    break;
    
  case CME_CTFFIND:                                         // CtfFind
  {
    if (ConvertBufferLetter(strItems[1], -1, true, index, report))
      ABORT_LINE(report);
    CtffindParams param;
    float resultsArray[7];
    if (mWinApp->mProcessImage->InitializeCtffindParams(&mImBufs[index], param))
      ABORT_LINE("Error initializing Ctffind parameters for line:\n\n");
    if (itemDbl[2] > 0. || itemDbl[3] > 0. || itemDbl[2] < -100 || itemDbl[3] < -100)
      ABORT_LINE("Minimum and maximum defocus must be negative and in microns");
    if (itemDbl[2] < itemDbl[3]) {
      delX = itemDbl[2];
      itemDbl[2] = itemDbl[3];
      itemDbl[3] = delX;
    }
    param.minimum_defocus = -(float)(10000. * itemDbl[2]);
    param.maximum_defocus = -(float)(10000. * itemDbl[3]);
    param.slower_search = itemEmpty[4] || itemInt[4] == 0;
    if (!itemEmpty[5] && itemInt[5] != 0) {
      if (itemInt[5] < 128 || itemInt[5] > 640)
        ABORT_LINE("The box size must be between 128 and 640 in the line:\n\n");
      param.box_size = itemInt[5];
    }

    // Phase entries.  Convert all from degrees to radians.  The assumed phase shift is
    // in radians and was being converted to degrees and passed that way (12/20/18)
    if (!itemEmpty[6]) {
      delX = itemDbl[6] * DTOR;
      if (delX == 0)
        delX = mWinApp->mProcessImage->GetPlatePhase();
      param.minimum_additional_phase_shift = param.maximum_additional_phase_shift = 
        (float)(delX);
      param.find_additional_phase_shift = true;
      if (!itemEmpty[7] && (itemDbl[6] != 0 || itemDbl[7] != 0)) {
        delY = itemDbl[7] * DTOR;
        if (delY < delX)
          ABORT_LINE("The maximum phase shift is less than the minimum in line:\n\n");
        if (delY - delX > 125 * DTOR)
          ABORT_LINE("The range of phase shift to search is more than 125 degrees in "
          "line:\n\n");
        param.maximum_additional_phase_shift = (float)delY;
      }
      if (!itemEmpty[8] && itemDbl[8])
        param.additional_phase_shift_search_step = (float)(itemDbl[8] * DTOR);
      if (!itemEmpty[9] && itemInt[9]) {
        param.astigmatism_is_known = true;
        param.known_astigmatism = 0.;
        param.known_astigmatism_angle = 0.;
      }
    }
    mWinApp->mProcessImage->SetCtffindParamsForDefocus(param, 
      0.8 * itemDbl[2] + 0.2 * itemDbl[3], true);
    param.compute_extra_stats = true;
    if (mWinApp->mProcessImage->RunCtffind(&mImBufs[index], param, resultsArray))
      ABORT_LINE("Ctffind fitting returned an error for line:\n\n");
    SetReportedValues(-(resultsArray[0] + resultsArray[1]) / 20000., 
      (resultsArray[0] - resultsArray[1]) / 10000., resultsArray[2], 
      resultsArray[3] / DTOR, resultsArray[4], resultsArray[5]);
    break;
  }
    
  case CME_IMAGEPROPERTIES:                                 // ImageProperties
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    mImBufs[index].mImage->getSize(sizeX, sizeY);
    delX = 1000. * mWinApp->mShiftManager->GetPixelSize(&mImBufs[index]);
    report.Format("Image size %d by %d, binning %s, exposure %.4f",
      sizeX, sizeY, (LPCTSTR)mImBufs[index].BinningText(),
      mImBufs[index].mExposure);
    if (delX) {
      strCopy.Format(", pixel size " + mWinApp->PixelFormat((float)delX), (float)delX);
      report += strCopy;
    }
    if (mImBufs[index].mSecNumber < 0) {
      delY = -1;
      strCopy = ", read in";
    } else {
      delY = mImBufs[index].mConSetUsed;
      strCopy.Format(", %s parameters", mModeNames[mImBufs[index].mConSetUsed]);
    }
    report += strCopy;
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], (double)sizeX, (double)sizeY,
      mImBufs[index].mBinning / (double)B3DMAX(1, mImBufs[index].mDivideBinToShow),
      (double)mImBufs[index].mExposure, delX, delY);
    break;
    
  case CME_IMAGELOWDOSESET:                                 // ImageLowDoseSet
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    index2 = mImBufs[index].mConSetUsed;

    if (index2 == MONTAGE_CONSET)
      index2 = RECORD_CONSET;
    if (mImBufs[index].mLowDoseArea && index2 >= 0) {
      report = "Image is from " + mModeNames[index2] + " in Low Dose";
    } else {
      if (index2 == TRACK_CONSET) {

        // Try to match View vs R by mag first
        mag = mImBufs[index].mMagInd;
        if (ldParam[VIEW_CONSET].magIndex == mag) {
          index2 = VIEW_CONSET;
        } else if (ldParam[RECORD_CONSET].magIndex == mag) {

          // Then decide between preview and record based on exposure
          delX = mImBufs[index].mExposure;
          if (fabs(delX - mConSets[RECORD_CONSET].exposure) <
            fabs(delX - mConSets[PREVIEW_CONSET].exposure))
            index2 = RECORD_CONSET;
          else
            index2 = PREVIEW_CONSET;
        }
        report = "Image matches one from " + mModeNames[index2] + " in Low Dose";
      } else if (index2 >= 0) {
        report = "Image is from " + mModeNames[index2] + " parameters, not in Low Dose";
      }
    }
    if (index2 > SEARCH_CONSET || index2 < 0) {
      index2 = -1;
      report = "Image properties do not match any Low Dose area well enough";
    }
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], (double)index2, 
      mImBufs[index].mLowDoseArea ? 1. : 0.);
    break;
    
  case CME_MEASUREBEAMSIZE:                                 // MeasureBeamSize
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    ix0 = mWinApp->mProcessImage->FindBeamCenter(&mImBufs[index], backlashX, backlashY,
      cpe, bmin, bmax, bmean, bSD, index2, ix1, shiftX, shiftY, fitErr);
    if (ix0)
      ABORT_LINE("No beam edges were detected in image for line:\n\n");
    bmean = mWinApp->mShiftManager->GetPixelSize(&mImBufs[index]);
    if (!bmean)
      ABORT_LINE("No pixel size is available for the image for line:\n\n");
    cpe *= 2.f * index2 * bmean;
    report.Format("Beam diameter measured to be %.3f um from %d quadrants, fit error %.3f"
      , cpe, ix1, fitErr);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(cpe, ix1, fitErr);
    break;
    
  case CME_QUADRANTMEANS:                                   // QuadrantMeans
    delY = 0.1;
    delX = 0.1;
    index = 2;
    if (!itemEmpty[1])
      index = itemInt[1];
    if (!itemEmpty[2])
      delX = itemDbl[2];
    if (!itemEmpty[3])
      delY = itemDbl[3];
    image = mImBufs->mImage;
    if (image)
      image->getSize(sizeX, sizeY);
    if (!image || index > B3DMIN(sizeX, sizeY) / 4 || index < 0 || delX <= 0.
      || delY < 0. || delX > 0.5 || delY > 0.4)
        ABORT_LINE("Parameter out of bounds for image, or no image in A in statement:"
        "\n\n");
    ix0 = B3DMAX((int)(delY * sizeX / 2), 1);   // Trim length in X
    iy0 = B3DMAX((int)(delY * sizeY / 2), 1);   // Trim length in Y
    ix1 = B3DMAX((int)(delX * sizeX / 2), 1);   // Width in X
    iy1 = B3DMAX((int)(delX * sizeY / 2), 1);   // Width in Y
    h4 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 + index, sizeX - 1 - ix0, sizeY / 2 + index, sizeY / 2 + index + iy1 - 1);
    v4 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 + index, sizeX / 2 + index + ix1 - 1, sizeY / 2 + index, sizeY - 1 - iy0);
    v3 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 - index - ix1, sizeX / 2 - index - 1, sizeY / 2 + index, sizeY - 1 - iy0);
    h3 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      ix0, sizeX / 2 - index - 1, sizeY / 2 + index, sizeY / 2 + index + iy1 - 1);
    h2 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      ix0, sizeX / 2 - index - 1, sizeY / 2 - index - iy1, sizeY / 2 - index - 1);
    v2 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 - index - ix1, sizeX / 2 - index - 1, iy0, sizeY / 2 - index - 1);
    v1 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 + index, sizeX / 2 + index + ix1 - 1, iy0, sizeY / 2 - index - 1);
    h1 = ProcImageMean(image->getData(), image->getType(), sizeX, sizeY,
      sizeX / 2 + index, sizeX - 1 - ix0, sizeY / 2 - index - iy1, sizeY / 2 - index - 1);
    report.Format("h1, v1, v2, h2, h3, v3, v4, h4:  %.2f   %.2f   %.2f   %.2f   %.2f   "
      "%.2f   %.2f   %.2f", h1, v1, v2, h2, h3, v3, v4, h4);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    break;

  case CME_CHECKFORBADSTRIPE:                               // CheckForBadStripe
    if (ConvertBufferLetter(strItems[1], 0, true, index, report))
      ABORT_LINE(report);
    ix0 = itemInt[2];
    if (itemEmpty[2]) {
      if (mImBufs[index].mCamera < 0)
        ABORT_LINE("The image has no camera defined and requires an entry for horizontal"
          " versus vertical analysis in:\n\n");
      ix0 = (mWinApp->GetCamParams() + mImBufs[index].mCamera)->rotationFlip % 2;
    }
    index2 = mWinApp->mProcessImage->CheckForBadStripe(&mImBufs[index], ix0, ix1);
    if (!index2)
      report = "No bad stripes detected";
    else
      report.Format("Bad stripes detected with %d sharp transitions; %d near expected"
        " boundaries", index2, ix1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index2, ix1);
    break;
    
  case CME_CIRCLEFROMPOINTS:                                // CircleFromPoints
    float radius, xcen, ycen;
    if (itemEmpty[6])
      ABORT_LINE("Need three sets of X, Y coordinates on line:\n" + strLine);
    h1 = itemDbl[1];
    v1 = itemDbl[2];
    h2 = itemDbl[3];
    v2 = itemDbl[4];
    h3 = itemDbl[5];
    v3 = itemDbl[6];
    if (circleThrough3Pts((float)h1, (float)v1, (float)h2, (float)v2, (float)h3,
      (float)v3, &radius, &xcen, &ycen)) {
        mWinApp->AppendToLog("The three points are too close to being on a straight line"
          , LOG_OPEN_IF_CLOSED);
    } else {
      report.Format("Circle center = %.2f  %.2f  radius = %.2f", xcen, ycen, radius);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      SetReportedValues(&strItems[7], xcen, ycen, radius);
    }
    break;
    
  case CME_FINDPIXELSIZE:                                   // FindPixelSize
    mWinApp->mProcessImage->FindPixelSize(0., 0., 0., 0.);
    break;
    
  case CME_ECHO:                                            // Echo
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, report);
    report.Replace("\n", "  ");
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    break;

  case CME_ECHOEVAL:                                        // EchoEval
  case CME_ECHOREPLACELINE:                                 // EchoReplaceLine
  case CME_ECHONOLINEEND:                                   // EchoNoLineEnd
    report = "";
    for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++) {
      if (index > 1)
        report += " ";
      report += strItems[index];
    }
    report.Replace("\n", "  ");
    index2 = 0;
    if (CMD_IS(ECHOREPLACELINE))
      index2 = 3;
    else if (CMD_IS(ECHONOLINEEND))
      index2 = 1;
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED, index2);
    break;

  case CME_VERBOSE:                                         // verbose
    mVerbose = itemInt[1];
    break;
    
  case CME_PROGRAMTIMESTAMPS:                               // ProgramTimeStamps
    mWinApp->AppendToLog(mWinApp->GetStartupMessage(), LOG_OPEN_IF_CLOSED);
    break;

    // IsVersionAtLeast, SkipIfVersionLessThan  break;
    
  case CME_ISVERSIONATLEAST:
  case CME_SKIPIFVERSIONLESSTHAN: 
    index2 = itemEmpty[2] ? 0 : itemInt[2];
    ix0 = mWinApp->GetIntegerVersion();
    ix1 = mWinApp->GetBuildDayStamp();
    truth = itemInt[1] <= ix0 && index2 <= ix1;
    if (CMD_IS(ISVERSIONATLEAST)) {
      SetReportedValues(truth ? 1. : 0.);
      report.Format("Program version is %d date %d, %s than %d %s", ix0, ix1, 
        truth ? "later" : "earlier", itemInt[1], (LPCTSTR)strItems[2]);
      mWinApp->AppendToLog(report, mLogAction);
    } else if (!truth && mCurrentIndex < macro->GetLength())
      GetNextLine(macro, mCurrentIndex, strLine);
    break;

  case CME_PAUSE:                         // Pause, YesNoBox, PauseIfFailed, AbortIfFailed
  case CME_YESNOBOX:
  case CME_PAUSEIFFAILED:
  case CME_ABORTIFFAILED:
      doPause = CMD_IS(PAUSEIFFAILED);
      doAbort = CMD_IS(ABORTIFFAILED);
      if (!(doPause || doAbort) || !mLastTestResult) {
        SubstituteVariables(&strLine, 1, strLine);
        mWinApp->mParamIO->StripItems(strLine, 1, report);
        if (doPause || doAbort)
          mWinApp->AppendToLog(report);
        doPause = doPause || CMD_IS(PAUSE);
        if (doPause) {
          report += "\n\nDo you want to proceed with the script?";
          index = AfxMessageBox(report, doAbort ? MB_EXCLAME : MB_QUESTION);
        } else
          index = SEMMessageBox(report, doAbort ? MB_EXCLAME : MB_QUESTION);
        if ((doPause && index == IDNO) || doAbort) {
          SuspendMacro(doAbort);
          return;
        } else
          SetReportedValues(index == IDYES ? 1. : 0.);
      }
    break;

  case CME_OKBOX:                                           // OKBox
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, report);
    AfxMessageBox(report, MB_OK | MB_ICONINFORMATION);
    break;
                          
  case CME_ENTERONENUMBER:                                  // EnterOneNumber
  case CME_ENTERDEFAULTEDNUMBER:                            // EnterDefaultedNumber
    backlashX = 0.;
    index = 1;
    index2 = 3;
    if (CMD_IS(ENTERDEFAULTEDNUMBER)) {

      // Here, enter the value and the number of digits, or < 0 to get an integer entry
      backlashX = (float)itemDbl[1];
      index = 3;
      index2 = itemInt[2];
      if (index2 < 0)
        ix0 = itemInt[1];
    }
    mWinApp->mParamIO->StripItems(strLine, index, strCopy);
    if (index2 >= 0) {
      truth = KGetOneFloat(strCopy, backlashX, index2);
    } else {
      truth = KGetOneInt(strCopy, ix0);
      backlashX = (float)ix0;
    }
    if (!truth)
      SUSPEND_NOLINE("because no number was entered");
    report.Format("%s: user entered  %g", (LPCTSTR)strCopy, backlashX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(backlashX);
    break;

  case CME_ENTERSTRING:                                     // EnterString
    strCopy = "Enter a text string:";
    report = "";
    mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
    if (!KGetOneString(strCopy, report))
      SUSPEND_NOLINE("because no string was entered");
    if (SetVariable(item1upper, report, VARTYPE_REGULAR, -1, false))
      ABORT_NOLINE("Error setting variable " + strItems[1] + " with string " + report);
    break;
    
  case CME_COMPARENOCASE:                                   // CompareStrings
  case CME_COMPARESTRINGS:                                  // CompareNoCase
    var = LookupVariable(item1upper, index2);
    if (!var)
      ABORT_LINE("The variable " + strItems[1] + " is not defined in line:\n\n");
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
    if (CMD_IS(COMPARESTRINGS))
      index = var->value.Compare(strCopy);
    else
      index = var->value.CompareNoCase(strCopy);
    report.Format("The strings %s equal", index ? "are NOT" : "ARE");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index);
    break;

  case CME_STRIPENDINGDIGITS:                               // StripEndingDigits
    var = LookupVariable(item1upper, index2);
    if (!var)
      ABORT_LINE("The variable " + strItems[1] + " is not defined in line:\n\n");
    report = var->value;
    for (index = report.GetLength() - 1; index > 0; index--)
      if ((int)report.GetAt(index) < '0' || (int)report.GetAt(index) > '9')
        break;
    strCopy = report.Right(report.GetLength() - (index + 1));
    report = report.Left(index + 1);
    item1upper = strItems[2];
    item1upper.MakeUpper();
    if (SetVariable(item1upper, report, VARTYPE_REGULAR, -1, false))
      ABORT_LINE("Error setting variable " + strItems[2] + " with string " + report +
      " in:\n\n");
    SetReportedValues(atoi((LPCTSTR)strCopy));
    break;
    
  case CME_MAILSUBJECT:                                     // MailSubject
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, mMailSubject);

    break;
    // SendEmail, ErrorBoxSendEmail
  case CME_SENDEMAIL:
  case CME_ERRORBOXSENDEMAIL:
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, report);
    if (CMD_IS(SENDEMAIL)) {
      mWinApp->mMailer->SendMail(mMailSubject, report);
    } else {
      if (!mWinApp->mMailer->GetInitialized() || mWinApp->mMailer->GetSendTo().IsEmpty())
        ABORT_LINE("Mail server not initialized or email address not defined; cannot "
        "do:\n");
      mEmailOnError = report;
    }
    break;

  case CME_CLEARALIGNMENT:                                  // ClearAlignment
    doShift = (itemEmpty[1] || !itemInt[1]) && !mScope->GetNoScope();
    mShiftManager->SetAlignShifts(0., 0., false, mImBufs, doShift);
    break;
    
  case CME_RESETIMAGESHIFT:                                 // ResetImageShift
    truth = mShiftManager->GetBacklashMouseAndISR();
    backlashX = 0.;
    if (!itemEmpty[1] && itemInt[1] > 0) {
      mShiftManager->SetBacklashMouseAndISR(true);
      if (itemInt[1] > 1)
        backlashX = itemEmpty[2] ? mScope->GetStageRelaxation() : (float)itemDbl[2];
    }
    index = mShiftManager->ResetImageShift(true, false, 10000, backlashX);
    mShiftManager->SetBacklashMouseAndISR(truth);
    if (index) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    break;
    
  case CME_RESETSHIFTIFABOVE:                               // ResetShiftIfAbove
    if (itemEmpty[1])
      ABORT_LINE("ResetShiftIfAbove must be followed by a number in: \n\n");
    mScope->GetLDCenteredShift(delISX, delISY);
    aMat = mShiftManager->IStoSpecimen(mScope->GetMagIndex());
    delX = aMat.xpx * delISX + aMat.xpy * delISY;
    delY = aMat.ypx * delISX + aMat.ypy * delISY;
    specDist = sqrt(delX * delX + delY * delY);
    if (specDist > itemDbl[1])
      mWinApp->mComplexTasks->ResetShiftRealign();
    break;
    
  case CME_EUCENTRICITY:                                    // Eucentricity
    index = FIND_EUCENTRICITY_FINE;
    if (!itemEmpty[1])
      index = itemInt[1];
    if ((index & (FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE)) == 0) {
      AbortMacro();
      report.Format("Error in script: value on Eucentricity statement \r\n"
        "should be %d for coarse, %d for fine, or %d for both",
        FIND_EUCENTRICITY_COARSE, FIND_EUCENTRICITY_FINE,
        FIND_EUCENTRICITY_FINE | FIND_EUCENTRICITY_COARSE);
      AfxMessageBox(report, MB_EXCLAME);
      return;
    }
    mWinApp->mComplexTasks->FindEucentricity(index);
    break;
    
  case CME_REPORTLASTAXISOFFSET:                            // ReportLastAxisOffset
    delX = mWinApp->mComplexTasks->GetLastAxisOffset();
    if (delX < -900)
      ABORT_NOLINE("There is no last axis offset; fine eucentricity has not been run");
    report.Format("Lateral axis offset in last run of Fine Eucentricity was %.2f", delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delX);
    break;
    
  case CME_WALKUPTO:                                        // WalkUpTo
    if (itemEmpty[1])
      ABORT_LINE("WalkUpTo must be followed by a target angle in: \n\n");
    delISX = itemDbl[1];
    if (delISX < -80. || delISX > 80.)
      ABORT_LINE("Target angle is too high in: \n\n");
    mWinApp->mComplexTasks->WalkUp((float)delISX, -1, 0);
    break;
    
  case CME_REVERSETILT:                                     // ReverseTilt
    index = (mScope->GetReversalTilt() > mScope->GetTiltAngle()) ? 1 : -1;
    if (!itemEmpty[1]) {
      index = itemInt[1];
      if (index > 0)
        index = 1;
      else if (index < 0)
        index = -1;
      else
        ABORT_NOLINE("Error in script: ReverseTilt should not be followed by 0");
    }
    mWinApp->mComplexTasks->ReverseTilt(index);
    break;
    
  case CME_DRIFTWAITTASK:                                   // DriftWaitTask
  {
    DriftWaitParams *dfltParam = mWinApp->mParticleTasks->GetDriftWaitParams();
    DriftWaitParams dwparm = *dfltParam;
    if (!itemEmpty[2]) {
      if (strItems[2] == "nm")
        dwparm.useAngstroms = false;
      else if (strItems[2] == "A")
        dwparm.useAngstroms = true;
      else if (strItems[2] != "0")
        ABORT_LINE("The second entry should be \"nm\", \"A\", or \"0\" in line:\n\n");
    }
    if (!itemEmpty[1] && itemDbl[1] > 0.) {
      dwparm.driftRate = (float)itemDbl[1] / (dwparm.useAngstroms ? 10.f : 1.f);
    }
    if (!itemEmpty[3] && itemDbl[3] > 0.) {
      dwparm.maxWaitTime = (float)itemDbl[3];
    }
    if (!itemEmpty[4] && itemDbl[4] > 0.) {
      dwparm.interval = (float)itemDbl[4];
    }
    if (!itemEmpty[5] && itemInt[5])
      dwparm.failureAction = itemInt[5] > 0 ? 1 : 0;
    if (!itemEmpty[6]) {
      if (strItems[6] == "T")
        dwparm.measureType = WFD_USE_TRIAL;
      else if (strItems[6] == "F")
        dwparm.measureType = WFD_USE_FOCUS;
      else if (strItems[6] == "A")
        dwparm.measureType = WFD_WITHIN_AUTOFOC;
      else if (strItems[6] != "0")
        ABORT_LINE("The image type to measure defocus from must be one of T, F, "
          "A, or 0 in line:\n\n");
    }
    if (!itemEmpty[7] && itemInt[7])
      dwparm.changeIS = itemInt[7] > 0 ? 1 : 0;
    if (!itemEmpty[8]) {
      dwparm.setTrialParams = true;
      if (itemDbl[8] > 0)
        dwparm.exposure = (float)itemDbl[8];
      if (!itemEmpty[9])
        dwparm.binning = itemInt[9];
    }
    mWinApp->mParticleTasks->WaitForDrift(dwparm, false);
    break;
  }

  case CME_CONDITIONPHASEPLATE:                             // ConditionPhasePlate
    if (mWinApp->mMultiTSTasks->ConditionPhasePlate(!itemEmpty[1] && itemInt[1] != 0)) {
      AbortMacro();
      return;
    }
    break;

  case CME_GETWAITTASKDRIFT:                                // GetWaitTaskDrift
    SetReportedValues(&strItems[1], mWinApp->mParticleTasks->GetWDLastDriftRate(),
      mWinApp->mParticleTasks->GetWDLastFailed());
    break;

  case CME_BACKLASHADJUST:                                  // BacklashAdjust
    mWinApp->mMontageController->GetColumnBacklash(backlashX, backlashY);
    mWinApp->mComplexTasks->BacklashAdjustStagePos(backlashX, backlashY, false, false);
    break;
    
  case CME_CENTERBEAMFROMIMAGE:                             // CenterBeamFromImage
    truth = !itemEmpty[1] && itemInt[1] != 0;
    delISX = !itemEmpty[2] ? itemDbl[2] : 0.;
    index = mWinApp->mProcessImage->CenterBeamFromActiveImage(0., 0., truth, delISX);
    if (index > 0 && index <= 3)
      ABORT_LINE("Script aborted centering beam because of no image,\n"
      "unusable image type, or failure to get memory");
    SetReportedValues(index);
    break;
    
  case CME_AUTOCENTERBEAM:                                  // AutoCenterBeam
    delISX = itemEmpty[1] ? 0. : itemDbl[1];
    if (mWinApp->mMultiTSTasks->AutocenterBeam((float)delISX)) {
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
    index2 = mWinApp->LowDoseMode() ? 3 : -1;
    if (CMD_IS(SETINTENSITYFORMEAN)) {
      if (!mImBufs->mImage || mImBufs->IsProcessed() ||
        (mWinApp->LowDoseMode() &&
        mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != 3))
        ABORT_LINE("Script aborted setting intensity because there\n"
          " is no image or a processed image in Buffer A\n"
          "(or, if in Low Dose mode, because the image in\n"
          " Buffer A is not from the Record area)");
      delISX = itemDbl[1] /
        mWinApp->mProcessImage->EquivalentRecordMean(0);
    } else if (CMD_IS(SETINTENSITYBYLASTTILT)) { 
      delISX = mIntensityFactor;
    } else {
      delISX = itemDbl[1];
      index2 = mScope->GetLowDoseArea();
    }

    index = mWinApp->mBeamAssessor->ChangeBeamStrength(delISX, index2);
    if (CheckIntensityChangeReturn(index))
      return;
    UpdateLDAreaIfSaved();
    break;

  case CME_SETDOSERATE:                                     // SetDoseRate
    if (itemDbl[1] <= 0)
      ABORT_LINE("Dose rate must be positive for line:\n\n");
    if (!mImBufs->mImage)
      ABORT_LINE("There must be an image in buffer A for line:\n\n");
    index = mWinApp->mProcessImage->DoSetIntensity(true, (float)itemDbl[1]);
    if (index < 0) {
      AbortMacro();
      return;
    }
    if (CheckIntensityChangeReturn(index))
      return;
    UpdateLDAreaIfSaved();
    break;

  case CME_SETPERCENTC2:   // Set/IncPercentC2
  case CME_INCPERCENTC2:

    // The entered number is always in terms of % C2 or illuminated area, so for
    // incremental, first comparable value and add to get the absolute to set
    delISX = itemDbl[1];
    if (itemEmpty[1])
      ABORT_LINE("Script aborted because no C2 percent was entered in:\n\n");
    index = mScope->FastSpotSize();
    if (CMD_IS(INCPERCENTC2))
      delISX += mScope->GetC2Percent(index, mScope->GetIntensity());

    // Then convert to an intensity as appropriate for scope
    if (mScope->GetUseIllumAreaForC2()) {
      delISY = mScope->IllumAreaToIntensity(delISX / 100.);
    } else {
      delISY = (0.01 * delISX - mScope->GetC2SpotOffset(index)) /
        mScope->GetC2IntensityFactor();
    }
    mScope->SetIntensity(delISY);
    delISY = mScope->FastIntensity();
    delISX = mScope->GetC2Percent(index, delISY);
    report.Format("Intensity set to %.3f%s  -  %.5f", delISX, mScope->GetC2Units(),
      delISY);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    UpdateLDAreaIfSaved();
    break;

  case CME_SETILLUMINATEDAREA:                              // SetIlluminatedArea
    if (!mScope->SetIlluminatedArea(itemDbl[1])) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;
    
  case CME_SETIMAGEDISTANCEOFFSET:                          // SetImageDistanceOffset
    if (!mScope->SetImageDistanceOffset(itemDbl[1])) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_SETALPHA:                                        // SetAlpha
    if (!mScope->SetAlpha(itemInt[1] - 1)) {
      AbortMacro();
      return;
    }
    UpdateLDAreaIfSaved();
    break;
    
  case CME_REPORTJEOLGIF:                                   // ReportJeolGIF
    index = mScope->GetJeolGIF();
    report.Format("JEOL GIF MODE return value %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_SETJEOLGIF:                                      // SetJeolGIF
    if (!mScope->SetJeolGIF(itemInt[1] ? 1 : 0)) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_NORMALIZELENSES:                                 // NormalizeLenses
    if (itemInt[1] & 1)
      index = mScope->NormalizeProjector();
    if (itemInt[1] & 2)
      index = mScope->NormalizeObjective();
    if (itemInt[1] & 4)
      index = mScope->NormalizeCondenser();
    if (!index)
      AbortMacro();
    break;
    
  case CME_NORMALIZEALLLENSES:                              // NormalizeAllLenses
    index = 0;
    if (!itemEmpty[1])
      index = itemInt[1];
    if (index < 0 || index > 3)
      ABORT_LINE("Lens group specifier must be between 0 and 3 in: \n\n");
    if (!mScope->NormalizeAll(index))
      AbortMacro();
    break;
    
  case CME_REPORTSLOTSTATUS:                                // ReportSlotStatus
    if (!mScope->CassetteSlotStatus(itemInt[1], index)) {
      AbortMacro();
      return;
    }
    if (index < -1)
      report.Format("Requesting status of slot %d gives an error", itemInt[1]);
    else
      report.Format("Slot %d %s", itemInt[1], index < 0 ? "has unknown status" :
      (index ? "is occupied" : "is empty"));
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], (double)index);
    break;
    
  case CME_LOADCARTRIDGE:
  case CME_UNLOADCARTRIDGE: // Load/UnloadCartridge
    if (CMD_IS(LOADCARTRIDGE))
      index = mScope->LoadCartridge(itemInt[1]);
    else
      index = mScope->UnloadCartridge();
    if (index)
      ABORT_LINE(index == 1 ? "The thread is already busy for a long operation in:\n\n" :
      "There was an error trying to run a long operation with:\n\n");
    mStartedLongOp = true;
    break;

  case CME_REFRIGERANTLEVEL:                                // RefrigerantLevel
    if (itemInt[1] < 1 || itemInt[1] > 3)
      ABORT_LINE("Dewar number must be between 1 and 3 in: \n\n");
    if (!mScope->GetRefrigerantLevel(itemInt[1], delX)) {
      AbortMacro();
      return;
    }
    report.Format("Refrigerant level in dewar %d is %.3f", itemInt[1], delX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], delX);
    break;
    
  case CME_DEWARSREMAININGTIME:                             // DewarsRemainingTime
    if (!mScope->GetDewarsRemainingTime(index)) {
      AbortMacro();
      return;
    }
    report.Format("Remaining time to fill dewars is %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_AREDEWARSFILLING:                                // AreDewarsFilling
    if (!mScope->AreDewarsFilling(index)) {
      AbortMacro();
      return;
    }
    if (FEIscope)
      report.Format("Dewars %s busy filling", index ? "ARE" : "are NOT");
    else {
      char *dewarTxt[4] = {"No tanks are", "Stage tank is", "Transfer tank is", 
        "Stage and transfer tanks are"};
      B3DCLAMP(index, 0, 3);
      report.Format("%s being refilled", dewarTxt[index]);
    }
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index);
    break;

  case CME_REPORTVACUUMGAUGE:                               // ReportVacuumGauge
    if (!mScope->GetGaugePressure((LPCTSTR)strItems[1], index, delISX)) {
      if (JEOLscope && index < -1 && index > -4) {
        report.Format("Name must be \"Pir\" or \"Pen\" followed by number between 0 and "
          "9 in line:\n\n");
        ABORT_LINE(report);
      }
      AbortMacro();
      return;
    }
    report.Format("Gauge %s status is %d, pressure is %f", (LPCTSTR)strItems[1], index, 
      delISX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[2], index, delISX);
    break;
    
  case CME_REPORTHIGHVOLTAGE:                               // ReportHighVoltage
    delISX = mScope->GetHTValue();
    report.Format("High voltage is %.1f kV", delISX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delISX);
    break;

  case CME_SETSLITWIDTH:                                    // SetSlitWidth
    if (itemEmpty[1])
      ABORT_LINE("SetSlitWidth must be followed by a number in: \n\n");
    delISX = itemDbl[1];
    if (delISX < filtParam->minWidth || delISX > filtParam->maxWidth)
      ABORT_LINE("This is not a legal slit width in: \n\n");
    filtParam->slitWidth = (float)delISX;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();

    break;
    // SetEnergyLoss, ChangeEnergyLoss
  case CME_SETENERGYLOSS:
  case CME_CHANGEENERGYLOSS:
    if (itemEmpty[1])
      ABORT_LINE(strItems[0] + " must be followed by a number in: \n\n");
    delISX = itemDbl[1];
    if (CMD_IS(CHANGEENERGYLOSS))
      delISX += filtParam->energyLoss;
    if (mWinApp->mFilterControl.LossOutOfRange(delISX, delISY)) {
      AbortMacro();
      report.Format("The energy loss requested in:\n\n%s\n\nrequires a net %s of %.1f"
        "with the current adjustments.\nThis net value is beyond the allowed range.",
        (LPCTSTR)strLine, mScope->GetHasOmegaFilter() ? "shift" : "offset", delISY);
      AfxMessageBox(report, MB_EXCLAME);
      return;
    }
    filtParam->energyLoss = (float)delISX;
    filtParam->zeroLoss = false;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();
    break;

  case CME_SETSLITIN:                                       // SetSlitIn
    index = 1;
    if (!itemEmpty[1])
      index = itemInt[1];
    filtParam->slitIn = index != 0;
    mWinApp->mFilterControl.UpdateSettings();
    mCamera->SetupFilter();
    UpdateLDAreaIfSaved();
    break;
    
  case CME_REFINEZLP:                                       // RefineZLP
    if (itemEmpty[1] || SEMTickInterval(1000. * filtParam->alignZLPTimeStamp) > 
        60000. * itemDbl[1]) {
      CTime ctdt = CTime::GetCurrentTime();
      report.Format("%02d:%02d:%02d", ctdt.GetHour(), ctdt.GetMinute(),ctdt.GetSecond());
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
      mWinApp->mFilterTasks->RefineZLP(false);
    }
    break;
    
  case CME_SELECTCAMERA:                                    // SelectCamera
    index = itemInt[1];
    if (index < 1 || index > mWinApp->GetNumActiveCameras())
      ABORT_LINE("Camera number out of range in: \n\n");
    RestoreCameraSet(-1, true);
    mWinApp->SetActiveCameraNumber(index - 1);
    break;
    
  case CME_SETEXPOSURE:                                     // SetExposure
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    delX = itemDbl[2];
    delY = itemEmpty[3] ? 0. : itemDbl[3];
    if (itemEmpty[2] || delX <=0. || delY < 0.)
      ABORT_LINE("Incorrect entry for setting exposure: \n\n");
    SaveControlSet(index);
    mConSets[index].exposure = (float)delX;
    if (!itemEmpty[3])
      mConSets[index].drift = (float)delY;
    break;
    
  case CME_SETBINNING:                                      // SetBinning
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (CheckCameraBinning(itemDbl[2], index2, strCopy))
      ABORT_LINE(strCopy);
    SaveControlSet(index);
    mConSets[index].binning = index2;
    break;
    
  case CME_SETCAMERAAREA:                                   // SetCameraArea
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    report = strItems[2];
    report.MakeUpper();
    if (report == "F" || report == "H" || report == "Q" ||
      report == "WH" || report == "WQ") {
      if (report == "F" || report == "WH") {
        ix0 = 0;
        ix1 = camParams->sizeX;
      } else if (report == "Q") {
        ix0 = 3 * camParams->sizeX / 8;
        ix1 = 5 * camParams->sizeX / 8;
      } else {
        ix0 = camParams->sizeX / 4;
        ix1 = 3 * camParams->sizeX / 4;
      }
      if (report == "F") {
        iy0 = 0;
        iy1 = camParams->sizeY;
      } else if (report == "H" || report == "WH") {
        iy0 = camParams->sizeY / 4;
        iy1 = 3 * camParams->sizeY / 4;
      } else {
        iy0 = 3 * camParams->sizeY / 8;
        iy1 = 5 * camParams->sizeY / 8;
      }
    } else {
      if (itemEmpty[5])
        ABORT_LINE("Not enough coordinates for setting camera area: \n\n");
      index2 = BinDivisorI(camParams);
      ix0 = B3DMAX(0, B3DMIN(camParams->sizeX - 4, index2 * itemInt[2]));
      ix1 = B3DMAX(ix0 + 1, B3DMIN(camParams->sizeX, index2 * itemInt[3]));
      iy0 = B3DMAX(0, B3DMIN(camParams->sizeY - 4, index2 * itemInt[4]));
      iy1 = B3DMAX(iy0 + 1, B3DMIN(camParams->sizeY, index2 * itemInt[5]));
    }
    index2 = mConSets[index].binning;
    iy0 /= index2;
    iy1 /= index2;
    ix0 /= index2;
    ix1 /= index2;
    sizeX = ix1 - ix0;
    sizeY = iy1 - iy0;
    mCamera->AdjustSizes(sizeX, camParams->sizeX, camParams->moduloX, ix0, ix1, sizeY,
      camParams->sizeY, camParams->moduloY, iy0, iy1, index2);
    SaveControlSet(index);
    mConSets[index].left = ix0 * index2;
    mConSets[index].right = ix1 * index2;
    mConSets[index].top = iy0 * index2;
    mConSets[index].bottom = iy1 * index2;
    break;
    
  case CME_SETCENTEREDSIZE:                                 // SetCenteredSize
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (CheckCameraBinning(itemDbl[2], index2, strCopy))
      ABORT_LINE(strCopy);
    sizeX = itemInt[3];
    sizeY = itemInt[4];
    if (sizeX < 4 || sizeX * index2 > camParams->sizeX || sizeY < 4 ||
      sizeY * index2 > camParams->sizeY)
      ABORT_LINE("Image size is out of range for current camera at given binning in:"
      " \n\n");
    SaveControlSet(index);
    mConSets[index].binning = index2;
    mCamera->CenteredSizes(sizeX, camParams->sizeX, camParams->moduloX, ix0, ix1,
      sizeY, camParams->sizeY, camParams->moduloY, iy0, iy1, index2);
    mConSets[index].left = ix0 * index2;
    mConSets[index].right = ix1 * index2;
    mConSets[index].top = iy0 * index2;
    mConSets[index].bottom = iy1 * index2;
    break;
    
  case CME_SETEXPOSUREFORMEAN:                              // SetExposureForMean
    index = RECORD_CONSET;
    if (!mImBufs->mImage || mImBufs->IsProcessed() ||
      (mWinApp->LowDoseMode() && mCamera->ConSetToLDArea(mImBufs->mConSetUsed) != index))
      ABORT_LINE("Script aborted setting exposure time because\n"
      " there is no image or a processed image in Buffer A\n"
      "(or, if in Low Dose mode, because the image in\n"
      " Buffer A is not from the Record area) for line:\n\n");
    if (itemDbl[1] <= 0) 
      ABORT_LINE("Exposure time must be positive for line:\n\n");
    delISX = itemDbl[1] / mWinApp->mProcessImage->EquivalentRecordMean(0);
    delISY = delISX * B3DMAX(0.001, mConSets[index].exposure - camParams->deadTime) +
      camParams->deadTime;
    if (!itemEmpty[2] && itemInt[2]) {

      // Adjusting frame time to keep constant number of frames
      if (!camParams->K2Type || !mConSets[index].doseFrac)
        ABORT_LINE("Frame time can be adjusted only for K2/K3 camera"
        " with dose fractionation mode on in line:\n\n");
      index2 = B3DNINT(mConSets[index].exposure /
        B3DMAX(0.001, mConSets[index].frameTime));
      bmin = (float)(delISY / index2);
      mCamera->ConstrainFrameTime(bmin, camParams);
      if (fabs(bmin - mConSets[index].frameTime) < 0.0001) {
        PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
          "too small a change in frame time", (LPCTSTR)strItems[1], delISX);
        bmean = 0.;
      } else {
        SaveControlSet(index);
        mConSets[index].frameTime = bmin;
        bmean = index2 * bmin;
        PrintfToLog("In SetExposureForMean %s, frame time changed to %.4f, exposure time"
          " changed to %.4f", (LPCTSTR)strItems[1], mConSets[index].frameTime, 
          bmean);
      }
    } else {

      // Just adjusting exposure time
      bmean = (float)delISY;
      ix1 = mCamera->DESumCountForConstraints(camParams, &mConSets[index]);
      mCamera->CropTietzSubarea(camParams, mConSets[index].right - mConSets[index].left,
        mConSets[index].bottom - mConSets[index].top, mConSets[index].processing,
        mConSets[index].mode, iy1);
      mCamera->ConstrainExposureTime(camParams, mConSets[index].doseFrac, 
        mConSets[index].K2ReadMode, mConSets[index].binning, 
        mConSets[index].alignFrames && !mConSets[index].useFrameAlign, ix1, bmean, 
        mConSets[index].frameTime, iy1, mConSets[index].mode);
      if (fabs(bmean - mConSets[index].exposure) < 0.00001) {
        PrintfToLog("In SetExposureForMean %s, change by a factor of %.4f would require "
          "too small a change in exposure time", (LPCTSTR)strItems[1], delISX);
        bmean = 0.;
      } else {
        SaveControlSet(index);
        bmean = mWinApp->mFalconHelper->AdjustSumsForExposure(camParams, &mConSets[index],
          bmean);
        /* This is good for seeing how the distribute frames function works
        report.Format("Summed frame list:");
        for (index2 = 0; index2 < mConSets[index].summedFrameList.size(); index2++) {
          strCopy.Format(" %d", mConSets[index].summedFrameList[index2]);
          report+= strCopy;
        }
        mWinApp->AppendToLog(report); */
        PrintfToLog("In SetExposureForMean %s, exposure time changed to %.4f",
          (LPCTSTR)strItems[1], bmean);
      }
    }

    // Commit the new exposure and report if change is not good
    if (bmean) {
      float diffThresh = 0.05f;
      mConSets[index].exposure = bmean;
      if (fabs(bmean - delISY) / delISY > diffThresh)
        PrintfToLog("WARNING: Desired exposure time (%.3f) differs from actual one "
        "(%.3f) by more than %d%%", delISY, bmean, B3DNINT(100. * diffThresh));
    }
    break;
    
  case CME_SETCONTINUOUS:                                   // SetContinuous
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    SaveControlSet(index);
    mConSets[index].mode = itemInt[2] ? CONTINUOUS : SINGLE_FRAME;
    break;
    
  case CME_SETPROCESSING:                                   // SetProcessing
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (itemInt[2] < 0 || itemInt[2] > 2)
      ABORT_LINE("Processing must 0, 1, or 2 in:\n\n");
    SaveControlSet(index);
    mConSets[index].processing = itemInt[2];
    break;
    
  case CME_SETFRAMETIME:                                    // SetFrameTime
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (!camParams->K2Type && !camParams->canTakeFrames)
      ABORT_NOLINE("Frame time cannot be set for the current camera type");
    SaveControlSet(index);
    mConSets[index].frameTime = (float)itemDbl[2];
    mCamera->CropTietzSubarea(camParams, mConSets[index].right - mConSets[index].left,
      mConSets[index].bottom - mConSets[index].top, mConSets[index].processing, 
      mConSets[index].mode, iy1);
    mCamera->ConstrainFrameTime(mConSets[index].frameTime, camParams,
      mConSets[index].binning, camParams->OneViewType ? mConSets[index].K2ReadMode : iy1);
    break;
    
  case CME_SETK2READMODE:                                   // SetK2ReadMode
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (!camParams->K2Type && !camParams->OneViewType && 
      camParams->FEItype != FALCON3_TYPE && !camParams->DE_camType)
      ABORT_NOLINE("Read mode cannot be set for the current camera type");
    if (itemInt[2] < 0 || itemInt[2] > 2)
      ABORT_LINE("Read mode must 0, 1, or 2 in:\n\n");
    SaveControlSet(index);
    mConSets[index].K2ReadMode = itemInt[2];
    break;

  case CME_SETDOSEFRACPARAMS:                               // SetDoseFracParams
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    if (!itemEmpty[5] && (itemInt[5] < 0 || itemInt[5] > 2))
      ABORT_LINE("Alignment method (the fourth number) must be 0, 1, or 2 in:\n\n");
    SaveControlSet(index);
    mConSets[index].doseFrac = itemInt[2] ? 1 : 0;
    if (!itemEmpty[3])
      mConSets[index].saveFrames = itemInt[3] ? 1 : 0;
    if (!itemEmpty[4])
      mConSets[index].alignFrames = itemInt[4] ? 1 : 0;
    if (!itemEmpty[5])
      mConSets[index].useFrameAlign = itemInt[5];
    if (!itemEmpty[6])
      mConSets[index].sumK2Frames = itemInt[6] ? 1 : 0;
    break;
    
  case CME_SETDECAMFRAMERATE:                               // SetDECamFrameRate
    if (!(camParams->DE_camType && camParams->DE_FramesPerSec > 0))
      ABORT_LINE("The current camera must be a DE camera with adjustable frame rate for"
        " line:\n\n");
    delISX = itemDbl[1];
    if (delISX < camParams->DE_MaxFrameRate + 1.)
      delISX = B3DMIN(delISX, camParams->DE_MaxFrameRate);
    if (delISX <= 0. || delISX > camParams->DE_MaxFrameRate) {
      report.Format("The new frame rate must be greater than zero\n"
        "and less than %.2f FPS for line:\n\n", camParams->DE_MaxFrameRate);
      ABORT_LINE(report);
    }
    if (mDEframeRateToRestore < 0) {
      mNumStatesToRestore++;
      mDEframeRateToRestore = camParams->DE_FramesPerSec;
      mDEcamIndToRestore = mWinApp->GetCurrentCamera();
    }
    camParams->DE_FramesPerSec = (float)delISX;
    mWinApp->mDEToolDlg.UpdateSettings();
    SetReportedValues(mDEframeRateToRestore);
    report.Format("Changed frame rate of DE camera from %.2f to %.2f", 
      mDEframeRateToRestore, delISX);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_USECONTINUOUSFRAMES:                             // UseContinuousFrames
    truth = itemInt[1] != 0;
    if ((mUsingContinuous ? 1 : 0) != (truth ? 1 : 0))
      mCamera->ChangePreventToggle(truth ? 1 : -1);
    mUsingContinuous = itemInt[1] != 0;
    break;
    
  case CME_STOPCONTINUOUS:                                  // StopContinuous
    mCamera->StopCapture(0);
    break;
    
  case CME_REPORTCONTINUOUS:                                // ReportContinuous
    index = mCamera->DoingContinuousAcquire();
    if (index)
      report.Format("Continuous acquire is running with set %d", index - 1);
    else
      report = "Continuous acquire is not running";
    SetReportedValues(&strItems[1], index - 1.);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_STARTFRAMEWAITTIMER:                             // StartFrameWaitTimer
    mFrameWaitStart = GetTickCount();
    break;
    
  case CME_WAITFORNEXTFRAME:                                // WaitForNextFrame
    if (!itemEmpty[1])
      mCamera->AlignContinuousFrames(itemInt[1], !itemEmpty[2] && itemInt[2] != 0);
    mCamera->SetTaskFrameWaitStart(mFrameWaitStart >= 0 ? mFrameWaitStart : 
      (double)GetTickCount());
    mFrameWaitStart = -1.;
    break;
    
  case CME_SETLIVESETTLEFRACTION:                           // SetLiveSettleFraction
    mCamera->SetContinuousDelayFrac((float)B3DMAX(0., itemDbl[1])); 
    break;
    
  case CME_SETSTEMDETECTORS:                                // SetSTEMDetectors
    if (!camParams->STEMcamera)
      ABORT_LINE("The current camera is not a STEM camera in: \n\n");
    if (CheckAndConvertCameraSet(strItems[1], itemInt[1], index, strCopy))
      ABORT_LINE(strCopy);
    sizeX = mCamera->GetMaxChannels(camParams);
    if (itemEmpty[sizeX + 1])
      ABORT_LINE("There must be a detector number for each channel in: \n\n");
    index2 = 0;
    for (ix0 = 2; ix0 < sizeX + 2; ix0++) {
      if (itemInt[ix0] < -1 || itemInt[ix0] >= camParams->numChannels)
        ABORT_LINE("Detector number out of range in: \n\n");
      for (iy0 = ix0 + 1; iy0 < sizeX + 2; iy0++) {
        if (itemInt[ix0] == itemInt[iy0] && itemInt[ix0] != -1)
          ABORT_LINE("Duplicate detector number in: \n\n");
      }
      if (itemInt[ix0] >= 0)
        index2++;
    }
    if (!index2)
      ABORT_LINE("There must be at least one detector listed in: \n\n");
    SaveControlSet(index);
    for (ix0 = 0; ix0 < sizeX; ix0++)
      mConSets[index].channelIndex[ix0] = itemInt[ix0 + 2];
    break;
    
  case CME_RESTORECAMERASET:                                // RestoreCameraSet
    index = -1;
    if (!itemEmpty[1] && CheckAndConvertCameraSet(strItems[1], itemInt[1], index, 
      strCopy))
        ABORT_LINE(strCopy);
    if (RestoreCameraSet(index, true))
      ABORT_NOLINE("No camera parameters were changed; there is nothing to restore");
    if (index == RECORD_CONSET && mAlignWholeTSOnly) {
      SaveControlSet(index);
      mConSets[index].alignFrames = 0;
    }
    break;
    
  case CME_KEEPCAMERASETCHANGES:                            // KeepCameraSetChanges
  {
    ControlSet *masterSets = mWinApp->GetCamConSets() +
      MAX_CONSETS * mWinApp->GetCurrentCamera();
    index = -1;
    if (!itemEmpty[1] && CheckAndConvertCameraSet(strItems[1], itemInt[1], index,
      strCopy))
      ABORT_LINE(strCopy);
    for (ix0 = (int)mConsetNums.size() - 1; ix0 >= 0; ix0--) {
      if (mConsetNums[ix0] == index || index < 0) {
        masterSets[mConsetNums[ix0]] = mConSets[mConsetNums[ix0]];
        mConsetsSaved.erase(mConsetsSaved.begin() + ix0);
        mConsetNums.erase(mConsetNums.begin() + ix0);
      }
    }
  }
  break;

  case CME_REPORTK2FILEPARAMS:                              // ReportK2FileParams
    index = mCamera->GetK2SaveAsTiff();
    index2 = mCamera->GetSaveRawPacked();
    ix0 = mCamera->GetUse4BitMrcMode();
    ix1 = mCamera->GetSaveTimes100();
    iy0 = mCamera->GetSkipK2FrameRotFlip();
    iy1 = mCamera->GetOneK2FramePerFile();
    report.Format("File type %s  raw packed %d  4-bit mode %d  x100 %d  Skip rot %d  "
      "file/frame %d", index > 1 ? "TIFF ZIP" : (index ? "MRC" : "TIFF LZW"), index2,
      ix0, ix1, iy0, iy1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], index, index2, ix0, ix1, iy0, iy1);
    break;

  case CME_SETK2FILEPARAMS:                                 // SetK2FileParams
    if (itemInt[1] < 0 || itemInt[1] > 2)
      ABORT_LINE("File type (first number) must be 0, 1, or 2 in:\n\n");
    if (itemInt[2] < 0 || itemInt[2] > 3)
      ABORT_LINE("Saving raw packed (second number) must be 0, 1, 2, or 3 in:\n\n");
    mCamera->SetK2SaveAsTiff(itemInt[1]);
    if (!itemEmpty[2])
      mCamera->SetSaveRawPacked(itemInt[2]);
    if (!itemEmpty[3])
      mCamera->SetUse4BitMrcMode(itemInt[3] ? 1 : 0);
    if (!itemEmpty[4])
      mCamera->SetSaveTimes100(itemInt[4] ? 1 : 0);
    if (!itemEmpty[5])
      mCamera->SetSkipK2FrameRotFlip(itemInt[5] ? 1 : 0);
    if (!itemEmpty[6])
      mCamera->SetOneK2FramePerFile(itemInt[6] ? 1 : 0);
    break;
       
  case CME_REPORTFRAMEALIPARAMS:                            // ReportFrameAliParams
  case CME_SETFRAMEALIPARAMS:                               // SetFrameAliParams
  case CME_REPORTFRAMEALI2:                                 // ReportFrameAli2
  case CME_SETFRAMEALI2:                                    // SetFrameAli2
    {
      CArray<FrameAliParams, FrameAliParams> *faParamArr = mCamera->GetFrameAliParams();
      FrameAliParams *faParam;
      BOOL *useGPUArr = mCamera->GetUseGPUforK2Align();
      index = mConSets[RECORD_CONSET].faParamSetInd;
      if (index < 0 || index >= (int)faParamArr->GetSize())
        ABORT_LINE("The frame alignment parameter set index for Record is out of range "
        "for:\n\n");
      faParam = faParamArr->GetData() + index;
      ix1 = camParams->useSocket ? 1 : 0;
      if (CMD_IS(REPORTFRAMEALIPARAMS)) { 
        index = (faParam->doRefine ? 1 : -1) * faParam->refineIter;
        index2 = (faParam->useGroups ? 1 : -1) * faParam->groupSize;
        report.Format("Frame alignment for Record has %s %d, keep precision %d"
          ", strategy %d, all-vs-all %d, refine %d, group %d", 
          faParam->binToTarget ? "target" : "binning",
          faParam->binToTarget ? faParam->targetSize : faParam->aliBinning, 
          faParam->keepPrecision, faParam->strategy, 
          faParam->numAllVsAll, index, index2);
        SetReportedValues(&strItems[1], faParam->aliBinning, faParam->keepPrecision,
          faParam->strategy, faParam->numAllVsAll, index, index2);
        mWinApp->AppendToLog(report, mLogAction);

      } else if (CMD_IS(SETFRAMEALIPARAMS)) { 
        if (itemInt[1] < 1 || (itemInt[1] > 16 && itemInt[1] < 100))
          ABORT_LINE("Alignment binning is out of range in:\n\n");
        if (itemInt[1] > 16)
          faParam->targetSize = itemInt[1];
        else
          faParam->aliBinning = itemInt[1];
        faParam->binToTarget = itemInt[1] > 16;
        if (!itemEmpty[2])
          faParam->keepPrecision = itemInt[2] > 0;
        if (!itemEmpty[3]) {
          B3DCLAMP(itemInt[3], 0, 3);
          faParam->strategy = itemInt[3];
        }
        if (!itemEmpty[4])
          faParam->numAllVsAll = itemInt[4];
        if (!itemEmpty[5]) {
          faParam->doRefine = itemInt[5] > 0;
          faParam->refineIter = B3DABS(itemInt[5]);
        }
        if (!itemEmpty[6]) {
          faParam->useGroups = itemInt[6] > 0;
          faParam->groupSize = B3DABS(itemInt[6]);
        }

      } else if (CMD_IS(REPORTFRAMEALI2)) {                 // ReportFrameAli2
        delX = (faParam->truncate ? 1 : -1) * faParam->truncLimit;
        report.Format("Frame alignment for Record has GPU %d, truncation %.2f, hybrid %d,"
          " filters %.4f %.4f %.4f", useGPUArr[ix1], delX, faParam->hybridShifts,
          faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);
        SetReportedValues(&strItems[1], useGPUArr[0], delX, faParam->hybridShifts,
          faParam->rad2Filt1, faParam->rad2Filt2, faParam->rad2Filt3);
        mWinApp->AppendToLog(report, mLogAction);

      } else {                                              // SetFrameAli2
        useGPUArr[ix1] = itemInt[1] > 0;
        if (!itemEmpty[2]) {
          faParam->truncate = itemDbl[2] > 0;
          faParam->truncLimit = (float)B3DABS(itemDbl[2]);
        }
        if (!itemEmpty[3]) 
          faParam->hybridShifts = itemInt[3] > 0;
        if (!itemEmpty[4]) {
          faParam->rad2Filt1 = (float)itemDbl[4];
          faParam->rad2Filt2 = itemEmpty[5] ? 0.f : (float)itemDbl[5];
          faParam->rad2Filt3 = itemEmpty[6] ? 0.f : (float)itemDbl[6];
        } 
      }
      break;
    }
    
  case CME_SKIPFRAMEALIPARAMCHECK:                          // SkipFrameAliCheck
    index = itemEmpty[1] ? 1 : itemInt[1];
    mSkipFrameAliCheck = index > 0;
    break;
    
  case CME_REPORTK3CDSMODE:                                 // ReportK3CDSmode
    index = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
    report.Format("CDS mode is %s", index ? "ON" : "OFF");
    SetReportedValues(&strItems[1], index);
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_SETK3CDSMODE:                                    // SetK3CDSmode
    if (itemEmpty[2] || !itemInt[2]) {
      if (mK3CDSmodeToRestore < 0) {
        mNumStatesToRestore++;
        mK3CDSmodeToRestore = mCamera->GetUseK3CorrDblSamp() ? 1 : 0;
      }
    } else {
      if (mK3CDSmodeToRestore >= 0)
        mNumStatesToRestore--;
      mK3CDSmodeToRestore = -1;
    }
    mCamera->SetUseK3CorrDblSamp(itemInt[1] != 0);
    break;

  case CME_REPORTCOUNTSCALING:                              // ReportCountScaling
    index = mCamera->GetDivideBy2();
    delX = mCamera->GetCountScaling(camParams);
    if (camParams->K2Type == K3_TYPE)
      delX = camParams->countsPerElectron;
    SetReportedValues(&strItems[1], index, delX);
    report.Format("Division by 2 is %s; count scaling is %.3f", index ? "ON" : "OFF", 
      delX);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_SETDIVIDEBY2:                                    // SetDivideBy2
    mCamera->SetDivideBy2(itemInt[1] != 0 ? 1 : 0);
    break;

  case CME_REPORTNUMFRAMESSAVED:                            // ReportNumFramesSaved
    index = mCamera->GetNumFramesSaved();
    SetReportedValues(&strItems[1], index);
    report.Format("Number of frames saved was %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_CAMERAPROPERTIES:                                // CameraProperties
    if (!itemEmpty[1]) {
      if (itemInt[1] < 1 || itemInt[1] > mWinApp->GetActiveCamListSize())
        ABORT_LINE("Active camera number is out of range in:\n\n")
      camParams = mWinApp->GetCamParams() + activeList[itemInt[1] - 1];
    }
    ix1 = BinDivisorI(camParams);
    index = camParams->sizeX / ix1;
    index2 = camParams->sizeY / ix1;
    report.Format("%s: size %d x %d    rotation/flip %d   physical pixel %.1f", 
      (LPCSTR)camParams->name, index, index2, camParams->rotationFlip, 
      camParams->pixelMicrons * ix1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, (double)index2, 
      (double)camParams->rotationFlip, camParams->pixelMicrons * ix1);
    break;
    
  case CME_REPORTCOLUMNORGUNVALVE:                        // ReportColumnOrGunValve
    index = mScope->GetColumnValvesOpen();
    if (index == -2)
      ABORT_NOLINE("An error occurred getting the state of the column/gun valve");
    SetReportedValues(&strItems[1], index);
    report.Format("Column/gun valve state is %d", index);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_SETCOLUMNORGUNVALVE:                             // SetColumnOrGunValve
    if (itemEmpty[1])
      ABORT_LINE("Entry requires a number for setting valve open or closed: \n\n");
    if (!mScope->SetColumnValvesOpen(itemInt[1] != 0))
      ABORT_NOLINE(itemInt[1] ? "An error occurred opening the valve" :
      "An error occurred closing the valve");
    break;

  case CME_REPORTFILAMENTCURRENT:                           // ReportFilamentCurrent
    delISX = mScope->GetFilamentCurrent();
    if (delISX < 0)
      ABORT_NOLINE("An error occurred getting the filament current");
    SetReportedValues(&strItems[1], delISX);
    report.Format("Filament current is %.5g", delISX);
    mWinApp->AppendToLog(report, mLogAction);
    break;

  case CME_SETFILAMENTCURRENT:                              // SetFilamentCurrent
    if (!mScope->SetFilamentCurrent(itemDbl[1]))
      ABORT_NOLINE("An error occurred setting the filament current");
    break;

  case CME_ISPVPRUNNING:                                    // IsPVPRunning
    if (!mScope->IsPVPRunning(truth))
      ABORT_NOLINE("An error occurred determining whether the PVP is running");
    report.Format("The PVP %s running", truth ? "IS" : "is NOT");
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], truth ? 1. : 0.);
    break;
    
  case CME_SETBEAMBLANK:                                    // SetBeamBlank
    if (itemEmpty[1])
      ABORT_LINE("Entry requires a number for setting blank on or off: \n\n");
    index = itemInt[1];
    mScope->BlankBeam(index != 0);
    break;
    
  case CME_MOVETONAVITEM:                                   // MoveToNavItem
    ABORT_NONAV;
    index = -1;
    if (!itemEmpty[1]) {
      index = itemInt[1] - 1;
      if (index < 0)
        ABORT_LINE("The Navigator item index must be positive: \n\n");
    }
    if (navigator->MoveToItem(index))
      ABORT_NOLINE("Error moving to Navigator item");
    mMovedStage = true;
    break;
    
  case CME_REALIGNTONAVITEM:                                // RealignToNavItem
  case CME_REALIGNTOOTHERITEM:                              // RealignToOtherItem
    ABORT_NONAV;
    truth = CMD_IS(REALIGNTOOTHERITEM);
    index2 = truth ? 2 : 1;
    index = itemInt[1] - 1;
    bmax = 0.;
    ix1 = ix0 = 0;
    if (!itemEmpty[index2 + 2]) {
      if (itemEmpty[index2 + 4])
        ABORT_LINE("Entry requires three values for controlling image shift reset in:"
        "\n\n");
      bmax = (float)itemDbl[index2 + 2];
      ix0 = itemInt[index2 + 3];
      ix1 = itemInt[index2 + 4];
    }
    if (!itemEmpty[index2 + 1])
      navHelper->SetContinuousRealign(itemInt[index2 + 1]);
    if (truth)
      iy0 = navigator->RealignToOtherItem(index, itemInt[index2] != 0, bmax, ix0, ix1);
    else
      iy0 = navigator->RealignToCurrentItem(itemInt[index2] != 0, bmax, ix0, ix1);
    if (iy0) {
      report.Format("Script halted due to failure %d in Realign to Item routine", iy0);
      ABORT_NOLINE(report);
      navHelper->SetContinuousRealign(0);
    }
    break;
    
  case CME_REALIGNTOMAPDRAWNON:                             // RealignToMapDrawnOn
    navItem = CurrentOrIndexedNavItem(itemInt[1], strLine);
    if (!navItem)
      return;
    if (!navItem->mDrawnOnMapID)
      ABORT_LINE("The specified item has no ID for being drawn on a map in line:\n\n");
    ix0 = navHelper->RealignToDrawnOnMap(navItem, itemInt[2] != 0);
    if (ix0) {
      report.Format("Script halted due to failure %d in Realign to Item for line:\n\n", 
        ix0);
      ABORT_LINE(report);
    }
    break;

  case CME_GETREALIGNTOITEMERROR:                           // GetRealignToItemError
    ABORT_NONAV;
    mWinApp->mNavHelper->GetLastStageError(backlashX, backlashY, bmin, bmax);
    SetReportedValues(&strItems[1], backlashX, backlashY, bmin, bmax);
    break;

    // ReportNavItem, ReportOtherItem, ReportNextNavAcqItem, LoadNavMap, LoadOtherMap
  case CME_REPORTNAVITEM:
  case CME_REPORTOTHERITEM:
  case CME_LOADNAVMAP:
  case CME_LOADOTHERMAP:
  case CME_REPORTNEXTNAVACQITEM:
      ABORT_NONAV;
      truth = CMD_IS(REPORTNEXTNAVACQITEM);
      if (CMD_IS(REPORTNAVITEM) || CMD_IS(LOADNAVMAP)) {                            
        index = navigator->GetCurrentOrAcquireItem(navItem);
        if (index < 0)
          ABORT_LINE("There is no current Navigator item for line:\n\n.");
      } else if (truth) {
        if (!navigator->GetAcquiring())
          ABORT_LINE("The Navigator must be acquiring for line:\n\n");
        navItem = navigator->FindNextAcquireItem(index);
        if (index < 0) {
          mWinApp->AppendToLog("There is no next item to be acquired", mLogAction);
          SetVariable("NAVINDEX", "-1", VARTYPE_REGULAR, -1, false);
          SetReportedValues(-1);
        }
      } else {
        if (itemInt[1] < 0) {
          index = navigator->GetNumNavItems() + itemInt[1];
        } else
          index = itemInt[1] - 1;
        navItem = navigator->GetOtherNavItem(index);
        if (!navItem)
          ABORT_LINE("Index is out of range in statement:\n\n");
      }

      if (CMD_IS(REPORTNAVITEM) || CMD_IS(REPORTOTHERITEM) || (truth && index >= 0)) {
        report.Format("%stem %d:  Stage: %.2f %.2f %2.f  Label: %s", 
          truth ? "Next i" : "I", index + 1, navItem->mStageX, navItem->mStageY, 
          navItem->mStageZ, (LPCTSTR)navItem->mLabel);
        if (!navItem->mNote.IsEmpty())
          report += "\r\n    Note: " + navItem->mNote;
        mWinApp->AppendToLog(report, mLogAction);
        SetReportedValues(index + 1., navItem->mStageX, navItem->mStageY, navItem->mStageZ,
          (double)navItem->mType);
        report.Format("%d", index + 1);
        SetVariable("NAVINDEX", report, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVLABEL", navItem->mLabel, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVNOTE", navItem->mNote, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVCOLOR", navItem->mColor, VARTYPE_REGULAR, -1, false);
        SetVariable("NAVREGIS", navItem->mRegistration, VARTYPE_REGULAR, -1, false);
        index = atoi(navItem->mLabel);
        report.Format("%d", index);
        SetVariable("NAVINTLABEL", report, VARTYPE_REGULAR, -1, false);
        if (navigator->GetAcquiring()) {
          report.Format("%d", navigator->GetNumAcquired() + (truth ? 2 : 1));
          SetVariable("NAVACQINDEX", report, VARTYPE_REGULAR, -1, false);
        }
      } else if (!truth) {
        if (navItem->mType != ITEM_TYPE_MAP)
          ABORT_LINE("The Navigator item is not a map for line:\n\n");
        navigator->DoLoadMap(false, navItem);
        mLoadingMap = true;
      }
    break;
                            
  case CME_NAVINDEXWITHLABEL:                               // NavIndexWithLabel
  case CME_NAVINDEXWITHNOTE:                                // NavIndexWithNote
    ABORT_NONAV;
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    truth = CMD_IS(NAVINDEXWITHNOTE);
    navigator->FindItemWithString(strCopy, truth);
    index = navigator->GetFoundItem() + 1;
    if (index > 0)
      report.Format("Item with %s %s has index %d", truth ? "note" : "label",
        (LPCTSTR)strCopy, index);
    else
      report.Format("No item has %s %s", truth ? "note" : "label", (LPCTSTR)strCopy);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index); 
    break;

  case CME_NAVINDEXITEMDRAWNON:                             // NavIndexItemDrawnOn
    index = itemInt[1];
    navItem = CurrentOrIndexedNavItem(index, strLine);
    if (!navItem)
      return;
    index2 = 0;
    if (!navItem->mDrawnOnMapID) {
      report.Format("Navigator item %d does not have an ID for being drawn on a map", 
        index + 1);
    } else {
      navItem = navigator->FindItemWithMapID(navItem->mDrawnOnMapID, true);
      if (!navItem) {
        report.Format("The map that navigator item %d was drawn on is no longer in the "
          "table", index + 1);
      } else {
        index2 = navigator->GetFoundItem() + 1;
        report.Format("Navigator item %d was drawn on map item %d", index + 1, index2);
      }
    }
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(index2);
    break;

  case CME_SETMAPACQUIRESTATE:                              // SetMapAcquireState
    ABORT_NONAV;
    navItem = navigator->GetOtherNavItem(itemInt[1] - 1);
    if (!navItem)
      ABORT_LINE("Index is out of range in statement:\n\n");
    if (navItem->mType != ITEM_TYPE_MAP) {
      report.Format("Navigator item %d is not a map for line:\n\n", itemInt[1]);
      ABORT_LINE(report);
    }
    if (mWinApp->mNavHelper->SetToMapImagingState(navItem, true))
      ABORT_LINE("Failed to set map imaging state for line:\n\n");
    break;

  case CME_RESTORESTATE:                                    // RestoreState
    index = mWinApp->mNavHelper->GetTypeOfSavedState();
    if (index == STATE_NONE) {
      report.Format("Cannot Restore State: no state has been saved");
      if (!itemEmpty[1] && itemInt[1])
        ABORT_LINE(report);
      mWinApp->AppendToLog(report, mLogAction);
    } else {
      if (index == STATE_MAP_ACQUIRE)
        mWinApp->mNavHelper->RestoreFromMapState();
      else {
        mWinApp->mNavHelper->RestoreSavedState();
        if (mWinApp->mNavHelper->mStateDlg)
          mWinApp->mNavHelper->mStateDlg->Update();
      }
      if (mWinApp->mNavHelper->mStateDlg)
        mWinApp->mNavHelper->mStateDlg->DisableUpdateButton();
    }
    break;

  case CME_REPORTNUMNAVACQUIRE:                             // ReportNumNavAcquire
    navHelper->CountAcquireItems(0, -1, index, index2);
    if (index < 0)
      report = "The Navigator is not open; there are no acquire items";
    else
      report.Format("Navigator has %d Acquire items, %d Tilt Series items", index,index2);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, (double)index2);
    break;
    
  case CME_REPORTNUMTABLEITEMS:                             // ReportNumTableItems
    ABORT_NONAV;
    index = navigator->GetNumberOfItems();
      report.Format("Navigator table has %d items", index);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_SETNAVREGISTRATION:                              // SetNavRegistration
    ABORT_NONAV;
    if (navigator->SetCurrentRegistration(itemInt[1]))
      ABORT_LINE("New registration number is out of range or used for imported items "
      "in:\n\n");
    break;
    
  case CME_SAVENAVIGATOR:                                   // SaveNavigator
    ABORT_NONAV;
    navigator->DoSave();
    break;
    
  case CME_REPORTIFNAVOPEN:                                 // ReportIfNavOpen
    index = 0;
    report = "Navigator is NOT open";
    if (mWinApp->mNavigator) {
      index = 1;
      report = "Navigator IS open";
      if (mWinApp->mNavigator->GetCurrentNavFile()) {
        report += "; file is defined";
        index = 2;
      }
    }
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index);
    break;
    
  case CME_READNAVFILE:                                     // ReadNavFile
  case CME_MERGENAVFILE:                                    // MergeNavFile
    truth = CMD_IS(MERGENAVFILE);
    if (truth) {
      ABORT_NONAV;
    } else  {
      mWinApp->mMenuTargets.OpenNavigatorIfClosed();
    }
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    if (!CFile::GetStatus((LPCTSTR)strCopy, status))
      ABORT_LINE("The file " + strCopy + " does not exist in line:\n\n");
    if (mWinApp->mNavigator->LoadNavFile(false, CMD_IS(MERGENAVFILE), &strCopy))
      ABORT_LINE("Script stopped due to error processing Navigator file for line:\n\n");
    break;


  case CME_CHANGEITEMREGISTRATION:                          // ChangeItemRegistration
  case CME_CHANGEITEMCOLOR:                                 // ChangeItemColor
  case CME_CHANGEITEMLABEL:                                 // ChangeItemLabel
      ABORT_NONAV;
      index = itemInt[1];
      index2 = itemInt[2];
      navItem = navigator->GetOtherNavItem(index - 1);
      report.Format("The Navigator item index, %d, is out of range in:\n\n", index);
      if (!navItem)
        ABORT_LINE(report);
      if (CMD_IS(CHANGEITEMREGISTRATION)) {
        report.Format("The Navigator item with index %d is a registration point in:\n\n", 
          index);
        if (navItem->mRegPoint)
          ABORT_LINE(report);
        if (navigator->ChangeItemRegistration(index - 1, index2, report))
          ABORT_LINE(report + " in line:\n\n");
      } else {
        if (CMD_IS(CHANGEITEMCOLOR)) {
          report.Format("The Navigator item color must be between 0 and %d in:\n\n", 
            NUM_ITEM_COLORS - 1);
          if (index2 < 0 || index2 >= NUM_ITEM_COLORS)
            ABORT_LINE(report);
          navItem->mColor = index2;
        } else {
          SubstituteVariables(&strLine, 1, strLine);
          mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
          report.Format("The Navigator label size must be no more than %d characters "
            "in:\n\n", MAX_LABEL_SIZE);
          if (strCopy.GetLength() > MAX_LABEL_SIZE)
            ABORT_LINE(report);
          navItem->mLabel = strCopy;
        }
        navigator->UpdateListString(index - 1);
        navigator->Redraw();
      }
    break;

  case CME_SKIPACQUIRINGNAVITEM:                            // SkipAcquiringNavItem
    ABORT_NONAV;
    if (!navigator->GetAcquiring())
      mWinApp->AppendToLog("SkipAcquiringNavItem has no effect except from a\r\n"
      "    pre-macro when acquiring Navigator items", mLogAction);
    navigator->SetSkipAcquiringItem(itemEmpty[1] || itemInt[1] != 0);
    break;
    
  case CME_SKIPACQUIRINGGROUP:                              // SkipAcquiringGroup
    ABORT_NONAV;
    if (!navigator->GetAcquiring())
      ABORT_NOLINE("The navigator must be acquiring to set a group ID to skip");
    if (itemEmpty[1]) {
      index2 = navigator->GetCurrentOrAcquireItem(navItem);
      index = navItem->mGroupID;
    } else {
      index = itemInt[1];
    }
    navigator->SetGroupIDtoSkip(index);
    break;
    
  case CME_SKIPMOVEINNAVACQUIRE:                            // SkipMoveInNavAcquire
    ABORT_NONAV;
    if (!navigator->GetAcquiring())
      ABORT_NOLINE("The navigator must be acquiring to enable skipping the stage move");
    navigator->SetSkipStageMoveInAcquire(itemEmpty[1] || itemInt[1] != 0);
    break;
    
  case CME_STARTNAVACQUIREATEND:                            // StartNavAcquireAtEnd
    ABORT_NONAV;
    mStartNavAcqAtEnd = itemEmpty[1] || itemInt[1] != 0;
    if (mStartNavAcqAtEnd&& (mWinApp->DoingTiltSeries() || navigator->GetAcquiring()))
      ABORT_NOLINE(CString("You cannot use StartNavAcquireAtEnd when ") + 
        (mWinApp->DoingTiltSeries() ? "a tilt series is running" : 
        "Navigator is already acquiring"));
    break;
    
  case CME_SUFFIXFOREXTRAFILE:                              // SuffixForExtraFile
    ABORT_NONAV;
    navigator->SetExtraFileSuffixes(&strItems[1], 
      B3DMIN(lastNonEmptyInd, MAX_STORES - 1));
    break;
    
  case CME_ITEMFORSUPERCOORD:                               // ItemForSuperCoord
    ABORT_NONAV;
    navigator->SetSuperCoordIndex(itemInt[1] - 1);
    break;
    
  case CME_UPDATEITEMZ:
  case CME_UPDATEGROUPZ:  // UpdateItemZ, UpdateGroupZ
    ABORT_NONAV;
    index2 = CMD_IS(UPDATEGROUPZ) ? 1 : 0;
    index = navigator->GetCurrentOrAcquireItem(navItem);
    if (index < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    index = navigator->DoUpdateZ(index, index2);
    if (index == 3)
      ABORT_LINE("Current Navigator item is not in a group for statement:\n\n");
    if (index)
      ABORT_LINE("Error updating Z of Navigator item in statement:\n\n");
    break;

  case CME_REPORTGROUPSTATUS:                               // ReportGroupStatus
    ABORT_NONAV;
    index = navigator->GetCurrentOrAcquireItem(navItem);
    if (index < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    index2 = navItem->mGroupID;
    index = -1;
    ix0 = navigator->CountItemsInGroup(index2, report, strCopy, ix1);
    if (navigator->GetAcquiring()) {
      index = 0;
      if (index2)
        index = navigator->GetFirstInGroup() ? 1 : 2;
    }
    report.Format("Group acquire status %d, group ID %d, # in group %d, %d set to acquire"
      , index, index2, ix0, ix1);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, (double)index2, (double)ix0, 
      (double)ix1);
    break;
    
  case CME_NEWMAP:                                          // NewMap
    ABORT_NONAV;
    navigator->SetSkipBacklashType(1);
    index = 0;
    if (!itemEmpty[1]) {
      index = itemInt[1];
      if (itemEmpty[2])
        ABORT_LINE("There must be text for the Navigator note after the number in:\n\n");
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 2, strCopy);
    }
    if (navigator->NewMap(false, index, itemEmpty[1] ? NULL : &strCopy)) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    SetReportedValues(&strItems[1], (double)navigator->GetNumberOfItems());
    break;
    
  case CME_MAKEANCHORMAP:                                   // MakeAnchorMap
    ABORT_NONAV;
    if (navigator->DoMakeDualMap()) {
      AbortMacro();
      return;
    }
    mMakingDualMap = true;
    break;
    
  case CME_SHIFTITEMSBYALIGNMENT:                           // ShiftItemsByAlignment
    ABORT_NONAV;
    if (navigator->ShiftItemsByAlign()) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_SHIFTITEMSBYCURRENTDIFF:                       // ShiftItemsByCurrentDiff
    ABORT_NONAV;
    index = navigator->GetCurrentOrAcquireItem(navItem);
    if (index < 0)
      ABORT_NOLINE("There is no current Navigator item.");
    mScope->GetStagePosition(stageX, stageY, stageZ);
    shiftX = (float)stageX;
    shiftY = (float)stageY;
    mScope->GetImageShift(delISX, delISY);
    index2 = mScope->GetLowDoseArea();
    if (mWinApp->LowDoseMode() && (index2 == VIEW_CONSET || index2 == SEARCH_AREA)) {
      mWinApp->mLowDoseDlg.GetNetViewShift(stageX, stageY, index2);
      delISX -= stageX;
      delISY -= stageY;
    }
    navigator->ConvertIStoStageIncrement(mScope->FastMagIndex(), 
        mWinApp->GetCurrentCamera(), delISX, delISY, (float)mScope->FastTiltAngle(), 
        shiftX, shiftY);
    shiftX -= navItem->mStageX;
    shiftY -= navItem->mStageY;
    specDist = sqrt(shiftX * shiftX + shiftY * shiftY);
    if (specDist <= itemDbl[1]) {
      navigator->ShiftItemsAtRegistration(shiftX, shiftY, 
        navItem->mRegistration);
      PrintfToLog("Items at registration %d shifted by %.2f, %.2f", 
        navItem->mRegistration, shiftX, shiftY);
    } else {
      PrintfToLog("Current stage position is too far from item position (%.2f microns);" 
        "nothing was shifted", specDist);
    }
    break;
    
  case CME_SHIFTITEMSBYMICRONS:                             // ShiftItemsByMicrons
    ABORT_NONAV;
    if (fabs(itemDbl[1]) > 100. || fabs(itemDbl[2]) > 100.)
      ABORT_LINE("You cannot shift items by more than 100 microns in:\n\n");
    index = navigator->GetCurrentRegistration();
    if (!itemEmpty[3]) {
      index = itemInt[3];
      report.Format("Registration number %d is out of range in:\n\n", index);
      if (index <= 0 || index > MAX_CURRENT_REG)
        ABORT_LINE(report);
    }
    index2 = navigator->ShiftItemsAtRegistration((float)itemDbl[1], (float)itemDbl[2],
      index);
    report.Format("%d items at registration %d were shifted by %.2f, %.2f", index2, index,
      itemDbl[1], itemDbl[2]);
    mWinApp->AppendToLog(report, mLogAction);
    break;
    
  case CME_FORCECENTERREALIGN:                              // ForceCenterRealign
    ABORT_NONAV;
    navHelper->ForceCenterRealign();
    break;
    
  case CME_SKIPPIECESOUTSIDEITEM:                           // SkipPiecesOutsideItem
    if (!mWinApp->Montaging())
      ABORT_LINE("Montaging must be on already to use this command:\n\n");
    if (itemInt[1] >= 0)
      montP->insideNavItem = itemInt[1] - 1;
    montP->skipOutsidePoly = itemInt[1] >= 0;
    break;
    
  case CME_SETHELPERPARAMS:                                 // SetHelperParams
    navHelper->SetTestParams(&itemDbl[1]);
    break;
    
  case CME_SETMONTAGEPARAMS:                                // SetMontageParams
    if (!mWinApp->Montaging())
      ABORT_LINE("Montaging must be on already to use this command:\n\n");
    if (mWinApp->mStoreMRC && mWinApp->mStoreMRC->getDepth() > 0 &&
      ((!itemEmpty[2] && itemInt[2] > 0) || (!itemEmpty[3] && itemInt[3] > 0) ||
      (!itemEmpty[4] && itemInt[4] > 0) || (!itemEmpty[5] && itemInt[5] > 0)))
        ABORT_LINE("Atfer writing to the file, you cannot change frame size or overlaps "
          " in line:\n\n");
    if (itemInt[1] >= 0)
      montP->moveStage = itemInt[1] > 0;
    if (!itemEmpty[4] && itemInt[4] > 0) {
      if (itemInt[4] < montP->xOverlap * 2)
        ABORT_LINE("The X frame size is less than twice the overlap in statement:\n\n");
      montP->xFrame = itemInt[4];
    }
    if (!itemEmpty[5] && itemInt[5] > 0) {
      if (itemInt[5] < montP->yOverlap * 2)
        ABORT_LINE("The Y frame size is less than twice the overlap in statement:\n\n");
      montP->yFrame = itemInt[5];
    }
    if (!itemEmpty[2] && itemInt[2] > 0) {
      if (itemInt[2] > montP->xFrame / 2)
        ABORT_LINE("X overlap is more than half the frame size in statement:\n\n");
      montP->xOverlap = itemInt[2];
    }
    if (!itemEmpty[3] && itemInt[3] > 0) {
      if (itemInt[3] > montP->yFrame / 2)
        ABORT_LINE("Y overlap is more than half the frame size in statement:\n\n");
      montP->yOverlap = itemInt[3];
    }
    if (!itemEmpty[6] && itemInt[6] >= 0)
      montP->skipCorrelations = itemInt[6] != 0;
    if (!itemEmpty[7] && itemDbl[7] >= 0.5) {
      if (CheckCameraBinning(itemDbl[7], index, report))
        ABORT_LINE(report);
      montP->binning = index;
    }
    mWinApp->mMontageWindow.UpdateSettings();
    break;
    
  case CME_MANUALFILMEXPOSURE:                              // ManualFilmExposure
    delX = itemDbl[1];
    mScope->SetManualExposure(delX);
    break;
 
  case CME_EXPOSEFILM:                                      // ExposeFilm
  case CME_SPECIALEXPOSEFILM:                               // SpecialExposeFilm
    delX = 0.;
    delY = 0.;
    index = 0;
    if (CMD_IS(SPECIALEXPOSEFILM)) {                        
      delX = itemDbl[1];
      if (!itemEmpty[2]) delY = itemDbl[2];
      if (delX < 0. || delY < 0. || itemEmpty[1])
        ABORT_LINE("There must be one or two non-negative values in statement:\n\n");
      if (!itemEmpty[3])
        index = itemInt[3];
    }
    if (!mScope->TakeFilmExposure(CMD_IS(SPECIALEXPOSEFILM), delX, delY, index != 0)) {
      mCurrentIndex = mLastIndex;
      SuspendMacro();
      return;
    }
    mExposedFilm = true;
    break;

  case CME_GOTOLOWDOSEAREA:                                 // GoToLowDoseArea
    if (CheckAndConvertLDAreaLetter(strItems[1], 1, index, strLine))
      return;
    mScope->GotoLowDoseArea(index);
    break;
    
  case CME_SETLDCONTINUOUSUPDATE:                           // SetLDContinuousUpdate
    if (mWinApp->mTSController->DoingTiltSeries())
      ABORT_NOLINE("You cannot use SetLDContinuousUpdate during a tilt series");
    mWinApp->mLowDoseDlg.SetContinuousUpdate(itemInt[1] != 0);
    break;
    
  case CME_SETLOWDOSEMODE:                                  // SetLowDoseMode
    index = mWinApp->LowDoseMode() ? 1 : 0;
    if (index != (itemInt[1] ? 1 : 0))
      mWinApp->mLowDoseDlg.SetLowDoseMode(itemInt[1] != 0);
    SetReportedValues(&strItems[2], (double)index);
    break;

  case CME_SETAXISPOSITION:                                 // SetAxisPosition
  case CME_REPORTAXISPOSITION:                              // ReportAxisPosition
    if (CheckAndConvertLDAreaLetter(strItems[1], 1, index, strLine))
      return;
    if ((index + 1) / 2 != 1)
      ABORT_LINE("This command must be followed by F or T:\n\n");
    if (CMD_IS(REPORTAXISPOSITION)) {
      delX = mWinApp->mLowDoseDlg.ConvertOneIStoAxis(ldParam[index].magIndex,
        ldParam[index].ISX, ldParam[index].ISY);
      ix0 = ((mConSets[index].right + mConSets[index].left) / 2 - camParams->sizeX / 2) /
        BinDivisorI(camParams);
      iy0 = ((mConSets[index].bottom + mConSets[index].top) / 2 - camParams->sizeY / 2) /
        BinDivisorI(camParams);
      index2 = mWinApp->mLowDoseDlg.m_bRotateAxis ? mWinApp->mLowDoseDlg.m_iAxisAngle : 0;
      report.Format("%s axis position %.2f microns, %d degrees; camera offset %d, %d "
        "unbinned pixels", mModeNames[index], delX, index2, ix0, iy0);
      mWinApp->AppendToLog(report, mLogAction);
      SetReportedValues(&strItems[2], delX, (double)index2, (double)ix0, (double)iy0);

    } else {
      index2 = 0;
      if (fabs(itemDbl[2]) > 19.)
        ABORT_LINE("The axis distance is too large in:\n\n");
      if (!itemEmpty[3])
        index2 = B3DNINT(UtilGoodAngle(itemDbl[3]));
      ix0 = (mScope->GetLowDoseArea() + 1) / 2;
      if (ix0 == 1)
        mScope->GetLDCenteredShift(delX, delY);
      mWinApp->mLowDoseDlg.NewAxisPosition(index, itemDbl[2], index2, !itemEmpty[3]);
      if (ix0 == 1)
        mScope->SetLDCenteredShift(delX, delY);
    }
    break;

  case CME_REPORTLOWDOSE:                                   // ReportLowDose
  {
    char *modeLets = "VFTRS";
    index = mWinApp->LowDoseMode() ? 1 : 0;
    index2 = mScope->GetLowDoseArea();
    report.Format("Low Dose is %s%s%c", index ? "ON" : "OFF",
      index && index2 >= 0 ? " in " : "", index && index2 >= 0 ? modeLets[index2] : ' ');
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], (double)index, (double)index2);
    break;
  }

  case CME_CURRENTSETTINGSTOLDAREA:                         // CurrentSettingsToLDArea
    if (CheckAndConvertLDAreaLetter(strItems[1], -1, index, strLine))
      return;
    mWinApp->InitializeOneLDParam(ldParam[index]);
    mWinApp->mLowDoseDlg.SetLowDoseMode(true);
    mScope->GotoLowDoseArea(index);
    break;

  case CME_UPDATELOWDOSEPARAMS:                             // UpdateLowDoseParams
    if (mWinApp->mTSController->DoingTiltSeries())
      ABORT_NOLINE("You cannot use ChangeLowDoseParams during a tilt series");
    if (CheckAndConvertLDAreaLetter(strItems[1], 0, index, strLine))
      return;
    if (!mKeepOrRestoreArea[index]) {
      mLDParamsSaved.push_back(ldParam[index]);
      mLDareasSaved.push_back(index);
      mKeepOrRestoreArea[index] = (itemEmpty[2] || !itemInt[2]) ? 1 : -1;
      UpdateLDAreaIfSaved();
    }
    break;

  case CME_RESTORELOWDOSEPARAMS:                            // RestoreLowDoseParams
    if (CheckAndConvertLDAreaLetter(strItems[1], 0, index, strLine))
      return;
    RestoreLowDoseParams(index);
    break;

  case CME_SETLDADDEDBEAMBUTTON:                            // SetLDAddedBeamButton
    truth = itemEmpty[1] || itemInt[1];
    if (!mWinApp->LowDoseMode() || mScope->GetLowDoseArea() < 0)
      ABORT_LINE("You must be in Low Dose mode and in a defined area for line:\n\n");
    if (mLDSetAddedBeamRestore < 0) {
      mLDSetAddedBeamRestore = mWinApp->mLowDoseDlg.m_bSetBeamShift ? 1 : 0;
      mNumStatesToRestore++;
    }
    mWinApp->mLowDoseDlg.SetBeamShiftButton(truth);
    if ((truth ? 1 : 0) == mLDSetAddedBeamRestore) {
      mLDSetAddedBeamRestore = -1;
      mNumStatesToRestore--;
    }
    break;

  case CME_SHOWMESSAGEONSCOPE:                              // ShowMessageOnScope
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    if (!FEIscope)
      ABORT_LINE("This command can be run only on an FEI scope");
    if (mScope->GetPluginVersion() < FEI_PLUGIN_MESSAGE_BOX) {
      report.Format("You need to upgrade the FEI plugin and/or server\n"
        "to at least version %d get a message box in:\n\n", FEI_PLUGIN_MESSAGE_BOX);
      ABORT_LINE(report);
    }
    mScope->SetMessageBoxArgs(mBoxOnScopeType, strCopy, mBoxOnScopeText);
    iy1 = LONG_OP_MESSAGE_BOX;
    index = mScope->StartLongOperation(&iy1, &mBoxOnScopeInterval, 1);
    if (index > 0)
      ABORT_LINE("The thread is already busy for a long operation so cannot run:\n\n");
    if (!index) {
      mStartedLongOp = true;
      mShowedScopeBox = true;
    } else {
      SetReportedValues(0.);
    }
    break;
    
  case CME_SETUPSCOPEMESSAGE:                               // SetupScopeMessage
    mBoxOnScopeType = itemInt[1];
    if (!itemEmpty[2])
      mBoxOnScopeInterval = (float)itemDbl[2];
    if (!itemEmpty[3]) {
      SubstituteVariables(&strLine, 1, strLine);
      mWinApp->mParamIO->StripItems(strLine, 3, mBoxOnScopeText);
    }
    break;
    
  case CME_UPDATEHWDARKREF:                                 // UpdateHWDarkRef
    index = mCamera->UpdateK2HWDarkRef((float)itemDbl[1]);
    if (index == 1)
      ABORT_LINE("The thread is already busy for this operation:\n\n")
    mStartedLongOp = !index;
    break;
    
  case CME_LONGOPERATION:                                   // LongOperation
  {
    ix1 = 0;
    iy1 = 1;
    int used[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int operations[MAX_LONG_OPERATIONS + 1];
    float intervals[MAX_LONG_OPERATIONS + 1];
    for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++) {
      for (index2 = 0; index2 < MAX_LONG_OPERATIONS; index2++) {
        if (!strItems[index].Left(2).CompareNoCase(CString(longKeys[index2]))) {
          if (longHasTime[index2] && itemDbl[index + 1] < -1.5) {
            backlashX = (float)itemDbl[index + 1];
            mScope->StartLongOperation(&index2, &backlashX, 1);
            SetOneReportedValue(backlashX, iy1++);
            report.Format("Time since last long operation %s is %.2f hours", 
              (LPCTSTR)strItems[index], backlashX);
            mWinApp->AppendToLog(report, mLogAction);
            index++;
            break;
          }
          if (used[index2])
            ABORT_LINE("The same operation is specified twice in:\n\n");
          used[index2]++;
          operations[ix1] = index2;
          if (index2 == LONG_OP_HW_DARK_REF && 
            !mCamera->CanDoK2HardwareDarkRef(camParams, report))
            ABORT_LINE(report + " in line:\n\n");
          if (longHasTime[index2]) {
            if (index == MAX_TOKENS - 1 || itemEmpty[index + 1])
              ABORT_LINE("The last operation must be followed by an interval in hours "
                "in:\n\n");
            report = strItems[index + 1];
            if (report.MakeLower() != strItems[index + 1]) {
              report.Format("%s must be followed by an interval in hours in:\n\n",
                (LPCTSTR)strItems[index]);
              ABORT_LINE(report);
            }
            index++;
            intervals[ix1++] = (float)itemDbl[index];
          }
          else
            intervals[ix1++] = 0.;
          break;
        }
      }
      if (index2 == MAX_LONG_OPERATIONS) {
        report.Format("%s is not an allowed entry for a long operation in:\n\n",
          (LPCTSTR)strItems[index]);
        ABORT_LINE(report);
      }
    }
    index = mScope->StartLongOperation(operations, intervals, ix1);
    if (index > 0)
      ABORT_LINE(index == 1 ? "The thread is already busy for a long operation in:\n\n" :
        "A long scope operation can be done only on an FEI scope for:\n\n");
    mStartedLongOp = !index;
    break;
  }
    
  case CME_NEWDESERVERDARKREF:                              // NewDEserverDarkRef
    if (mWinApp->mGainRefMaker->MakeDEdarkRefIfNeeded(itemInt[1], (float)itemDbl[2], 
      report))
      ABORT_NOLINE(CString("Cannot make a new dark reference in DE server with "
      "NewDEserverDarkRef:\n") + report);
    break;
    
  case CME_RUNINSHELL:                                      // RunInShell
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, mEnteredName);
    mProcessThread = AfxBeginThread(RunInShellProc, &mEnteredName, THREAD_PRIORITY_NORMAL,
       0, CREATE_SUSPENDED);
    mProcessThread->m_bAutoDelete = false;
    mProcessThread->ResumeThread();
    break;
    
  case CME_NEXTPROCESSARGS:                                 // NextProcessArgs
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, mNextProcessArgs);
    break;
    
  case CME_CREATEPROCESS:                                   // CreateProcess
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    index = mWinApp->mExternalTools->RunCreateProcess(strCopy, mNextProcessArgs);
    mNextProcessArgs = "";
    if (index)
      ABORT_LINE("Script aborted due to failure to run process for line:\n\n");
    break;
    
  case CME_RUNEXTERNALTOOL:                                 // RunExternalTool
    SubstituteVariables(&strLine, 1, strLine);
    mWinApp->mParamIO->StripItems(strLine, 1, strCopy);
    if(mWinApp->mExternalTools->RunToolCommand(strCopy))
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
     index = mScope->GetProbeMode();
     if (mBeamTiltXtoRestore[index] > EXTRA_VALUE_TEST) {
       report = "There is a second SaveBeamTilt without a RestoreBeamTilt";
       if (FEIscope)
         report += " for " + CString(index ? "micro" : "nano") + "probe mode";
       ABORT_NOLINE(report);
     }
     mScope->GetBeamTilt(mBeamTiltXtoRestore[index], mBeamTiltYtoRestore[index]);
     mNumStatesToRestore++;
     break;
     
  case CME_RESTOREBEAMTILT:                                 // RestoreBeamTilt
    index = mScope->GetProbeMode();
    if (mBeamTiltXtoRestore[index] < EXTRA_VALUE_TEST) {
       report = "There is a RestoreBeamTilt, but beam tilt was not saved or has been "
        "restored already";
       if (FEIscope)
         report += " for " + CString(index ? "micro" : "nano") + "probe mode";
       ABORT_NOLINE(report);
    }
    mScope->SetBeamTilt(mBeamTiltXtoRestore[index], mBeamTiltYtoRestore[index]);
    mNumStatesToRestore--;
    mBeamTiltXtoRestore[index] = mBeamTiltYtoRestore[index] = EXTRA_NO_VALUE;
    mCompensatedBTforIS = false;
    break;

    // PIEZO COMMANDS
  case CME_SELECTPIEZO:                                     // SelectPiezo
    if (mWinApp->mPiezoControl->SelectPiezo(itemInt[1], itemInt[2])) {
      AbortMacro();
      return;
    }
    break;
    
  case CME_REPORTPIEZOXY:                                   // ReportPiezoXY
    if (mWinApp->mPiezoControl->GetNumPlugins()) {
      if (mWinApp->mPiezoControl->GetXYPosition(delISX, delISY)) {
        AbortMacro();
        return;
      }
    } else {
      if (!mScope->GetPiezoXYPosition(delISX, delISY)) {
        AbortMacro();
        return;
      }
    }
    report.Format("Piezo X/Y position is %6g, %6g", delISX, delISY);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delISX, delISY);
    break;
    
  case CME_REPORTPIEZOZ:                                    // ReportPiezoZ
    if (mWinApp->mPiezoControl->GetZPosition(delISX)) {
      AbortMacro();
      return;
    }
    report.Format("Piezo Z or only position is %6g", delISX);
    mWinApp->AppendToLog(report, mLogAction);
    SetReportedValues(&strItems[1], delISX);
    break;
    
  case CME_MOVEPIEZOXY:                                     // MovePiezoXY
    if (mWinApp->mPiezoControl->GetNumPlugins()) {
      if (mWinApp->mPiezoControl->SetXYPosition(itemDbl[1], itemDbl[2],
        !itemEmpty[3] && itemInt[3] != 0)) {
        AbortMacro();
        return;
      }
      mMovedPiezo = true;
    } else {
      if (!mScope->SetPiezoXYPosition(itemDbl[1], itemDbl[2],
        !itemEmpty[3] && itemInt[3] != 0)) {
        AbortMacro();
        return;
      }
      mMovedStage = true;
    }
    break;
    
  case CME_MOVEPIEZOZ:                                      // MovePiezoZ
    if (mWinApp->mPiezoControl->SetZPosition(itemDbl[1],
      !itemEmpty[2] && itemInt[2] != 0)) {
        AbortMacro();
        return;
    }
    mMovedPiezo = true;
    break;
    
  case CME_MACRONAME:                                       // MacroName
  case CME_SCRIPTNAME:                                      // ScriptName
  case CME_LONGNAME:                                        // LongName
    index = 0; 
    break;
  default:
    ABORT_LINE("Unrecognized statement in script: \n\n");
    break;
  }

  // The action is taken or started: now set up an idle task
  mWinApp->AddIdleTask(NULL, TASK_MACRO_RUN, 0, 0);
}

void CMacroProcessor::AbortMacro()
{
  SuspendMacro(true);
}

void CMacroProcessor::SuspendMacro(BOOL abort)
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  int probe, ind;
  bool restoreArea = false;
  if (!mDoingMacro)
    return;
  if (TestAndStartFuncOnStop())
    return;

  // restore user settings
  for (ind = 0; ind < (int)mSavedSettingNames.size(); ind++)
    mWinApp->mParamIO->MacroSetSetting(CString(mSavedSettingNames[ind].c_str()), 
      mSavedSettingValues[ind]);
  if (mSavedSettingNames.size())
    mWinApp->UpdateWindowSettings();
  for (ind = 0; ind < MAX_LOWDOSE_SETS; ind++)
    if (mKeepOrRestoreArea[ind] > 0)
      restoreArea = true;

  // Restore other things and make it non-resumable as they have no mechanism to resume
  if (abort || mNumStatesToRestore > 0 || restoreArea) {
    mCurrentMacro = -1;
    mLastAborted = !mLastCompleted;
    if (mNumStatesToRestore) {
      if (mFocusToRestore > -2.)
        mScope->SetFocus(mFocusToRestore);
      if (mFocusOffsetToRestore > -9000.)
        mWinApp->mFocusManager->SetDefocusOffset(mFocusOffsetToRestore);
      if (mDEframeRateToRestore > 0 || mDEcamIndToRestore >= 0) {
        camParams[mDEcamIndToRestore].DE_FramesPerSec = mDEframeRateToRestore;
        mWinApp->mDEToolDlg.UpdateSettings();
      }
      probe = mScope->GetProbeMode();
      if (mBeamTiltXtoRestore[probe] > EXTRA_VALUE_TEST) {
        mScope->SetBeamTilt(mBeamTiltXtoRestore[probe], mBeamTiltYtoRestore[probe]);
        if (mWinApp->mFocusManager->DoingFocus())
          mWinApp->mFocusManager->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
        if (mWinApp->mAutoTuning->DoingZemlin() || 
          mWinApp->mAutoTuning->GetDoingCtfBased())
          mWinApp->mAutoTuning->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
        if (mWinApp->mParticleTasks->DoingMultiShot())
          mWinApp->mParticleTasks->SetBaseBeamTilt(mBeamTiltXtoRestore[probe], 
          mBeamTiltYtoRestore[probe]);
      }
      if (mBeamTiltXtoRestore[1 - probe] > EXTRA_VALUE_TEST) {
        mWinApp->AppendToLog("Switching probe modes to restore beam tilt value");
        mScope->SetProbeMode(1 - probe);
        mScope->SetBeamTilt(mBeamTiltXtoRestore[1 - probe], 
          mBeamTiltYtoRestore[1 - probe]);
        mScope->SetProbeMode(probe);
      }
      if (mLDSetAddedBeamRestore >= 0)
        mWinApp->mLowDoseDlg.SetBeamShiftButton(mLDSetAddedBeamRestore > 0);
      if (mK3CDSmodeToRestore >= 0)
        mCamera->SetUseK3CorrDblSamp(mK3CDSmodeToRestore > 0);
    }
    mSavedSettingNames.clear();
    mSavedSettingValues.clear();
    mNewSettingValues.clear();
    if (restoreArea)
      RestoreLowDoseParams(-2);
  }

  // restore camera sets, clear if non-resumable
  RestoreCameraSet(-1, mCurrentMacro < 0);
  if (mCurrentMacro < 0 && mUsingContinuous)
    mCamera->ChangePreventToggle(-1);
  if (mCurrentMacro < 0)
    CloseFileForText(-1);
  mDoingMacro = false;
  if (mStoppedContSetNum >= 0)
    mCamera->InitiateCapture(mStoppedContSetNum);
  if (abort) {
    for (ind = MAX_MACROS + MAX_ONE_LINE_SCRIPTS; ind < MAX_TOT_MACROS; ind++)
      ClearFunctionArray(ind);
  }
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(mWinApp->DoingTiltSeries() &&
    mWinApp->mTSController->GetRunningMacro() ? MEDIUM_PANE : COMPLEX_PANE, 
    (IsResumable() && mWinApp->mNavigator && mWinApp->mNavigator->GetAcquiring()) ?
    "STOPPED NAV SCRIPT" : "");
  if (!mCamera->CameraBusy())
    mWinApp->SetStatusText(SIMPLE_PANE, "");
  mWinApp->mScopeStatus.SetWatchDose(false);
  mWinApp->mDocWnd->SetDeferWritingFrameMdoc(false);
}


void CMacroProcessor::SetIntensityFactor(int iDir)
{
  double angle = mScope->GetTiltAngle();
  double increment = iDir * mScope->GetIncrement();
  mIntensityFactor = cos(angle * DTOR) / cos((angle + increment) * DTOR);
}

// Get the next line or multiple lines if they end with backslash
void CMacroProcessor::GetNextLine(CString * macro, int & currentIndex, CString &strLine)
{
  int index, testInd;
  strLine = "";
  for (;;) {

    // Find next linefeed
    index = macro->Find('\n', currentIndex);
    if (index < 0) {

      // If none, get rest of string, set index past end
      strLine = macro->Mid(currentIndex);
      currentIndex = macro->GetLength();
      break;
    } else {

      // Set index past end of line then test for space backslash after skipping a \r
      index++;
      testInd = index - 2;
      if (testInd >= 0 && macro->GetAt(testInd) == '\r')
        testInd--;
      if (testInd > 0 && macro->GetAt(testInd) == '\\' && 
        macro->GetAt(testInd - 1) == ' ') {

          // To continue, add the line through the space then set the index to next line
          strLine += macro->Mid(currentIndex, testInd - currentIndex);
          currentIndex = index;
      } else {

        // otherwise get the whole line and break out
        strLine += macro->Mid(currentIndex, index - currentIndex);
        currentIndex = index;
        break;
      }
    }
  }
}

// Scan the macro for a name, set it in master name list, and return 1 if it changed
// Scan the given macro string, or the master macro if NULL
int CMacroProcessor::ScanForName(int macroNumber, CString *macro)
{
  CString newName = "";
  CString longName = "";
  CString strLine, argName, strItem[MAX_TOKENS];
  CString *longMacNames = mWinApp->GetLongMacroNames();
  MacroFunction *funcP;
  int currentIndex = 0;
  if ((macroNumber >= MAX_MACROS && macroNumber < MAX_MACROS + MAX_ONE_LINE_SCRIPTS) ||
    (mDoingMacro && mCallLevel > 0))
      return 0;
  if (!macro)
    macro = &mMacros[macroNumber];
  ClearFunctionArray(macroNumber);
  while (currentIndex < macro->GetLength()) {
    GetNextLine(macro, currentIndex, strLine);
    if (!strLine.IsEmpty()) {
      mWinApp->mParamIO->ParseString(strLine, strItem,MAX_TOKENS);
      if ((strItem[0].CompareNoCase("MacroName") == 0 || 
        strItem[0].CompareNoCase("ScriptName") == 0) && !strItem[1].IsEmpty())
        mWinApp->mParamIO->StripItems(strLine, 1, newName);
      if (strItem[0].CompareNoCase("LongName") == 0 && !strItem[1].IsEmpty())
        mWinApp->mParamIO->StripItems(strLine, 1, longName);

      // Put all the functions in there that won't be eliminated by minimum argument
      // requirement and let pre-checking complain about details
      if (strItem[0].CompareNoCase("Function") == 0 && !strItem[1].IsEmpty()) {
        funcP = new MacroFunction;
        funcP->name = strItem[1];
        funcP->numNumericArgs = strItem[2].IsEmpty() ? 0 : atoi((LPCTSTR)strItem[2]);
        funcP->ifStringArg = !strItem[3].IsEmpty() && strItem[3] != "0";
        funcP->startIndex = currentIndex;  // Trust that this can be used for call
        funcP->wasCalled = false;
        for (int arg = 0; arg < funcP->numNumericArgs + (funcP->ifStringArg ? 1 : 0); 
          arg++) {
           if (strItem[4 + arg].IsEmpty())
             argName.Format("ARGVAL%d", arg + 1);
           else
             argName = strItem[4 + arg].MakeUpper();
           funcP->argNames.Add(argName);
        }
        mFuncArray[macroNumber].Add(funcP);
      }
     }
  }
  longMacNames[macroNumber] = longName;
  if (newName != mMacNames[macroNumber]) {
    mMacNames[macroNumber] = newName;
    return 1;
  }
  return 0;
}

// Clear out all the functions in a specific macro or all macros
void CMacroProcessor::ClearFunctionArray(int index)
{
  int start = index < 0 ? 0 : index;
  int end = index < 0 ? MAX_TOT_MACROS - 1 : index;
  for (int mac = start; mac <= end; mac++) {
    for (int ind = (int)mFuncArray[mac].GetSize() - 1; ind >= 0; ind--)
      delete mFuncArray[mac].GetAt(ind);
    mFuncArray[mac].RemoveAll();
  }
}

// Find macro called on this line; unload and scan names if scanning is true
int CMacroProcessor::FindCalledMacro(CString strLine, bool scanning)
{
  CString strCopy;
  int index, index2;
  mWinApp->mParamIO->StripItems(strLine, 1, strCopy);

  // Look for the name
  index = -1;
  for (index2 = 0; index2 < MAX_TOT_MACROS; index2++) {
    if (index >= MAX_MACROS && index < MAX_MACROS + MAX_ONE_LINE_SCRIPTS)
      continue;
    ScanMacroIfNeeded(index2, scanning);
    if (strCopy == mMacNames[index2]) {
      if (index >= 0) {
        AfxMessageBox("Two scripts have a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
        return -1;
      }
      index = index2;
    }
  }
  if (index < 0)
    AfxMessageBox("No script has a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
  return index;
}

// Make sure macro is unloaded and name scanned for any macros being edited
void CMacroProcessor::ScanMacroIfNeeded(int index, bool scanning)
{
  if (index < MAX_MACROS && mMacroEditer[index] && scanning) {
    for (int ix0 = 0; ix0 <= mCallLevel; ix0++)
      if (mCallMacro[ix0] == index)
        return;
    mMacroEditer[index]->TransferMacro(true);
    ScanForName(index, &mMacros[index]);
  }
}

// Given the parsed line that calls a function, find the macro it is in and the function
// structure in that macro's array, and index of first argument if any
MacroFunction *CMacroProcessor::FindCalledFunction(CString strLine, bool scanning, 
  int &macroNum, int &argInd, int currentMac)
{
  int colonLineInd = strLine.Find("::");
  int num, mac, ind, loop, colonItemInd, start = 0, end = MAX_TOT_MACROS - 1;
  CString funcName, macName, strItems[MAX_TOKENS];
  MacroFunction *func, *retFunc = NULL;
  macroNum = -1;
  if (currentMac < 0)
    currentMac = mCurrentMacro;

  // Reparse the line so everthing is original case
  mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS);

  // No colon, name is first item and need to search all
  if (colonLineInd < 0) {
    funcName = strItems[1];
    argInd = 2;
  } else {

    // colon: first find the item with it (has to be there)
    for (ind = 1; ind < MAX_TOKENS; ind++) {
      colonItemInd = strItems[ind].Find("::");
      if (colonItemInd == 0) {
        AfxMessageBox("There must be a script name or number before the :: in this "
          "call:\n\n" + strLine, MB_EXCLAME);
        return NULL;
      }
      if (colonItemInd == strItems[ind].GetLength() - 2) {
        AfxMessageBox("There must be a function name after the :: in this "
          "call:\n\n" + strLine, MB_EXCLAME);
        return NULL;
      }

      // If there is a legal colon and this is the second item, first try to convert the
      // macro name to a number and see if that works
      if (colonItemInd > 0) {
        if (ind == 1) {
          macName = strItems[ind].Left(colonItemInd);
          num = atoi((LPCTSTR)macName);
          funcName.Format("%d", num);
          if (funcName == macName) {
            macroNum = num - 1;
            ScanMacroIfNeeded(macroNum, scanning);
          }
        }

        // If not, go ahead and look up the name
        if (macroNum < 0)
          macroNum = FindCalledMacro(strLine.Left(colonLineInd), scanning);
        if (macroNum < 0)
          return NULL;
        start = macroNum;
        end = macroNum;
        funcName = strItems[ind].Mid(colonItemInd + 2);
        argInd = ind + 1;
        break;
      }
    }
  }

  // Now search one or all function arrays for the function name, make sure only one
  // Search current macro first if there is no colon
  for (loop = (colonLineInd < 0 ? 0 : 1); loop < 2; loop++) {
    for (mac = (loop ? start : currentMac); mac <= (loop ? end : currentMac); mac++)
    {
      if (colonLineInd < 0 || !loop)
        ScanMacroIfNeeded(mac, scanning);
      for (ind = 0; ind < mFuncArray[mac].GetSize(); ind++) {
        func = mFuncArray[mac].GetAt(ind);
        if (func->name == funcName) {
          if (retFunc) {
            if (macroNum == mac)
              AfxMessageBox("Two functions in a script have the same name for this "
              "call:\n\n" + strLine,  MB_EXCLAME);
            else
              AfxMessageBox("Two functions have the same name and you need \nto specify "
              "the script name or number for this call:\n\n" + strLine,  MB_EXCLAME);
            return NULL;
          }
          retFunc = func;
          macroNum = mac;
        }
      }
    }
    if (!loop && retFunc)
      break;
  }
  if (!retFunc) 
    AfxMessageBox("No function has a matching name for this call:\n\n" + strLine,
          MB_EXCLAME);
  return retFunc;
}

// Sets a variable of the given name to the value, with the type and index.
// The variable must not exist if mustBeNew is true; returns true for error and fills
// in errStr if it is non-null
bool CMacroProcessor::SetVariable(CString name, CString value, int type, 
  int index, bool mustBeNew, CString *errStr, CArray<ArrayRow, ArrayRow> *rowsFor2d)
{
  int ind, leftInd, rightInd, rightInd2, arrInd, rowInd, numElements = 1;
  int *oldNumElemPtr;
  CString *oldValuePtr;
  CString temp;
  name.MakeUpper();
  if (name[0] == '$' || value[0] == '$') {
    if (errStr)
      errStr->Format("The %s still starts with a $ after substituting variables\r\n"
      "Is it an undefined variable?", name[0] == '$' ? "variable name" : "value");
    return true;
  }
  if (WordIsReserved(name))
    PrintfToLog("WARNING: assigning to a variable %s, which is named the same as a "
      "reserved keyword", (LPCTSTR)name);
  if (index == -1)
    index = mCurrentMacro;

  // Count number of elements in new value
  ind = 0;
  while ((ind = value.Find("\n", ind + 1)) >= 0)
    numElements++;

  // See if this is an assignment to an array element and check legality
  leftInd = name.Find('[');
  rightInd = name.Find(']');
  if (leftInd > 0 && FindAndCheckArrayIndexes(name, leftInd, rightInd, rightInd2, errStr)
    < 0)
      return true;
    
  if ((leftInd < 0 && rightInd >= 0) || leftInd == 0 ||
    (rightInd >= 0 && ((rightInd2 <= 0 && rightInd < name.GetLength() - 1) || 
    (rightInd2 > 0 && rightInd2 < name.GetLength() - 1)))) {
      if (errStr)
        errStr->Format("Illegal use or placement of [ and/or ] in variable name %s", 
          (LPCTSTR)name);
      return true;
  }

  // Get actual variable name to look up, make sure it exists for element assignment
  temp = name;
  if (leftInd > 0)
    temp = name.Left(leftInd);
  Variable *var = LookupVariable(temp, ind);
  if (var && type == VARTYPE_LOCAL && var->type != VARTYPE_LOCAL)
    var = NULL;
  if (leftInd > 0 && !var) {
    if (errStr)
      errStr->Format("Variable %s must be defined to assign to an array element", 
      (LPCTSTR)temp);
    return true;
  }

  // Define a new variable
  if (!var) {
    var = new Variable;
    var->name = name;
    var->value = value;
    var->numElements = numElements;
    var->type = type;
    var->callLevel = mCallLevel;
    var->index = index;
    var->definingFunc = mCallFunction[mCallLevel];
    var->rowsFor2d = rowsFor2d;
    mVarArray.Add(var);
    return false;
  }

  // Clear out any existing 2D array if not assigning to an array element
  if (leftInd < 0) {
    delete var->rowsFor2d;
    var->rowsFor2d = rowsFor2d;
  }

  // There is a set error if it must be new or if a user variable is using the name
  // of an existing variable of another type
  if (mustBeNew) {
    if (errStr)
      errStr->Format("Variable %s must not already exist and does", (LPCTSTR)name);
    return true;
  }
  if (type != VARTYPE_INDEX && var->type == VARTYPE_INDEX) {
    if (errStr)
      errStr->Format("Variable %s is a loop index variable and cannot be assigned to", 
      (LPCTSTR)name);
    return true;
  }
  if ((type == VARTYPE_PERSIST && var->type != VARTYPE_PERSIST) ||
    (type != VARTYPE_PERSIST && var->type == VARTYPE_PERSIST)) {
      if (errStr)
        errStr->Format("Variable %s is already defined as %spersistent and cannot"
          " be reassigned as %spersistent", (LPCTSTR)name, 
          (type != VARTYPE_PERSIST) ? "" : "non-", type == VARTYPE_PERSIST ? "" : "non-");
    return true;
  }

  if (leftInd > 0) {
    oldNumElemPtr = &var->numElements;
    oldValuePtr = &var->value;

    // For 2D array assignment, get row index, element and value pointers for that row
    if (rightInd2 > 0) {
      rowInd = ConvertArrayIndex(name, leftInd, rightInd, var->name, 
        (int)var->rowsFor2d->GetSize(), errStr);
      if (!rowInd)
        return true;
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(rowInd - 1);
      oldNumElemPtr = &arrRow.numElements;
      oldValuePtr = &arrRow.value;
      leftInd = rightInd + 1;
      rightInd = rightInd2;
    } 

    // If assigning to an array element, get the index value 
    arrInd = ConvertArrayIndex(name, leftInd, rightInd, var->name, 
      (var->rowsFor2d && rightInd2 <= 0) ? (int)var->rowsFor2d->GetSize() : 
      *oldNumElemPtr, errStr);
    if (!arrInd)
      return true;
    if (var->rowsFor2d && rightInd2 <= 0) {

      // If assigning (array) to row of 2D array, just set it into the value
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
      arrRow.numElements = numElements;
      arrRow.value = value;
    } else {

      // For assignment to a single element, get the starting and
      // ending (+1) indexes in the value of that element
      FindValueAtIndex(*oldValuePtr, arrInd, leftInd, rightInd);

      // Substitute the new value for the existing element and adjust numElements
      temp = "";
      if (leftInd)
        temp = oldValuePtr->Left(leftInd);
      temp += value;
      ind = oldValuePtr->GetLength();
      if (rightInd < ind)
        temp += oldValuePtr->Mid(rightInd, ind - rightInd);
      *oldValuePtr = temp;
      *oldNumElemPtr += numElements - 1;
    }
  } else {

    // Regular assignment to variable value
    var->value = value;
    var->numElements = numElements;
  }

  // If value is empty, remove a persistent variable
  if (type == VARTYPE_PERSIST && value.IsEmpty()) {
    delete var->rowsFor2d;
    delete var;
    mVarArray.RemoveAt(ind);
  }
  return false;
}

// Overloaded function can be passed a double instead of string value
bool CMacroProcessor::SetVariable(CString name, double value, int type,  
  int index, bool mustBeNew, CString *errStr, CArray<ArrayRow, ArrayRow> *rowsFor2d)
{
  CString str;
  str.Format("%f", value);
  TrimTrailingZeros(str);
  return SetVariable(name, str, type, index, mustBeNew, errStr, rowsFor2d);
}

// Test whether a local variable is already defined as global, issue message and 
// return 1 if it is, return 0 if not defined, -1 if already local
int CMacroProcessor::LocalVarAlreadyDefined(CString & item, CString &strLine)
{
  int index2;
  CString mess;
  Variable *var = LookupVariable(item, index2);
  if (var && var->type != VARTYPE_LOCAL && var->callLevel == mCallLevel &&
    var->definingFunc == mCallFunction[mCallLevel] && (var->index == mCurrentMacro ||
      var->type == VARTYPE_INDEX || var->type == VARTYPE_REPORT)) {
    mess = "Variable " + item + " has already been defined as global"
      "\nin this script/function and cannot be made local in line:\n\n";
    ABORT_NORET_LINE(mess);
    return -1;
  }
  return var ? -1 : 0;
}

// Looks up a variable by name and returns pointer if found, NULL if not, and index in ind
// Looks first for a local variable at the current level and function, then for non-local
Variable *CMacroProcessor::LookupVariable(CString name, int &ind)
{
  Variable *var;
  int global;
  bool localVar;
  name.MakeUpper();

  // Loop twice, look for local variable first, then a global one
  for (global = 0; global < 2; global++) {
    for (ind = 0; ind < mVarArray.GetSize(); ind++) {
      var = mVarArray[ind];
      localVar = var->type == VARTYPE_LOCAL || 
        (mLoopIndsAreLocal && var->type == VARTYPE_INDEX);
      if ((global && !localVar) || (!global && localVar && var->callLevel == mCallLevel && 
        (var->index == mCurrentMacro || var->type == VARTYPE_INDEX) && 
        var->definingFunc == mCallFunction[mCallLevel])) {
          if (var->name == name)
            return var;
      }
    }
  }
  return NULL;
}

// Removes all variables of the given type (or any type except persistent by default) AND
// at the given level unless level < 0 (the default) AND with the given index value 
// (or any index by default) 
void CMacroProcessor::ClearVariables(int type, int level, int index)
{
  Variable *var;
  int ind;
  for (ind = (int)mVarArray.GetSize() - 1; ind >= 0; ind--) {
    var = mVarArray[ind];
    if (((type < 0 && var->type != VARTYPE_PERSIST) || var->type == type) &&
      (index < 0 || index == var->index)  && (level < 0 || var->callLevel >= level)) {
      delete var->rowsFor2d;
      delete var;
      mVarArray.RemoveAt(ind);
    }
  }
}

// Given a variable reference that could include an array subscript, look up the variable
// and row of a 2D array return pointers to the value and numElements elements, or return
// NULL for those pointers for a 2D array without subscript.  Returns NULL for the 
// variable for any error condition
Variable * CMacroProcessor::GetVariableValuePointers(CString & name, CString **valPtr, 
  int **numElemPtr, const char *action, CString & errStr)
{
  CString strCopy = name;
  Variable *var;
  int rightInd, right2, index;
  int leftInd = strCopy.Find('[');
  if (leftInd > 0) {
    if (FindAndCheckArrayIndexes(strCopy, leftInd, rightInd, right2, &errStr) < 0)
      return NULL;
    if (right2 > 0) {
      errStr = CString("Cannot ") + action + " to a single element of a 2D array";
      return NULL;
    }
    strCopy = strCopy.Left(leftInd);
  }
  var = LookupVariable(strCopy, right2);
  if (!var) {
    errStr = "Could not find variable " + strCopy;
    return NULL;
  }

  // Get pointers to value and number of elements for all cases
  if (var->rowsFor2d) {
    if (leftInd > 0) {
      index = ConvertArrayIndex(name, leftInd, rightInd, strCopy,
        (int)var->rowsFor2d->GetSize(), &errStr);
      if (!index)
        return NULL;
      ArrayRow& tempRow = var->rowsFor2d->ElementAt(index - 1);
      *valPtr = &tempRow.value;
      *numElemPtr = &tempRow.numElements;
    } else {
      *valPtr = NULL;
      *numElemPtr = NULL;
    }
  } else {
    if (leftInd > 0) {
      errStr = CString("Cannot ") + action + "to a single element of an array";
      return NULL;
    }
    *valPtr = &var->value;
    *numElemPtr = &var->numElements;
  }
  return var;
  }

// Looks for variables and substitutes them in each string item
int CMacroProcessor::SubstituteVariables(CString * strItems, int maxItems, CString line)
{
  Variable *var;
  CString newstr, value;
  int subInd, varInd, maxlen, maxInd, nright, varlen, arrInd, beginInd, endInd, nameInd;
  int itemLen, global, nright2, leftInd, numElements;
  bool localVar, subArrSize;

  // For each item, look for $ repeatedly
  for (int ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    while (1) {

      // Search from the back so array indexes can be substituted first
      subInd = strItems[ind].ReverseFind('$');
      if (subInd < 0)
        break;

      // Set index of actual name and adjust it if this is looking for # of elements
      itemLen = strItems[ind].GetLength();
      subArrSize = subInd < itemLen - 1 && strItems[ind].GetAt(subInd + 1) == '#';
      nameInd = subInd + (subArrSize ? 2 : 1);

      // Now look for the longest matching variable, first looking for local, then global
      for (global = 0; global < 2; global++) {
        maxlen = 0;
        for (varInd = 0; varInd < mVarArray.GetSize(); varInd++) {
          var = mVarArray[varInd];
          localVar = var->type == VARTYPE_LOCAL ||
            (mLoopIndsAreLocal && var->type == VARTYPE_INDEX);
          if ((global && !localVar) || (!global && localVar &&
            var->callLevel == mCallLevel &&
            (var->index == mCurrentMacro || var->type == VARTYPE_INDEX) &&
            var->definingFunc == mCallFunction[mCallLevel])) {
            varlen = var->name.GetLength();
            if (itemLen - nameInd >= varlen) {
              newstr = strItems[ind].Mid(nameInd, varlen);
              if (!newstr.CompareNoCase(var->name) && maxlen < varlen) {
                maxInd = varInd;
                maxlen = varlen;
              }
            }
          }
        }
        if (maxlen)
          break;
      }

      if (!maxlen) {
        AfxMessageBox("Undefined variable in script line:\n\n" +
          line, MB_EXCLAME);
        return 1;
      }
      var = mVarArray[maxInd];

      // If it is a loop index, look up the value and put in value string
      if (var->type == VARTYPE_INDEX) {
        if (var->index < 0 || var->index >= MAX_LOOP_DEPTH) {
          AfxMessageBox("The variable " + var->name + " is apparently a loop index "
            "variable,\nbut the pointer to the loop is out of range in script line:\n\n" +
            line, MB_EXCLAME);
          return 2;
        }
        var->value.Format("%d", mLoopCount[var->index]);
      }

      // If it is an array element, first check usage
      value = var->value;
      leftInd = nameInd + maxlen;
      numElements = var->numElements;
      if (FindAndCheckArrayIndexes(strItems[ind], leftInd, nright, nright2, &newstr) < 0) {
        SEMMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
        return 2;
      }
      if (nright > 0) {
        if (subArrSize && (!var->rowsFor2d || nright2 > 0)) {
          SEMMessageBox("Illegal use of $# with an array element in line:\n\n" + line,
            MB_EXCLAME);
          return 2;
        }

        // 2D array reference: make sure it IS 2D array, then get the index of the row
        if (nright2 > 0) {
          if (!var->rowsFor2d) {
            SEMMessageBox("Reference to 2D array element, but " + var->name + "is not"
              "a 2D array in line:\n\n" + line, MB_EXCLAME);
            return 2;
          }
          arrInd = ConvertArrayIndex(strItems[ind], leftInd, nright, var->name,
            (int)var->rowsFor2d->GetSize(), &newstr);
          if (!arrInd) {
            AfxMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
            return 2;
          }
          ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
          numElements = arrRow.numElements;
          value = arrRow.value;
          leftInd = nright + 1;
          nright = nright2;
        } else if (var->rowsFor2d) {
          numElements = (int)var->rowsFor2d->GetSize();
        }

        // Get the array index
        arrInd = ConvertArrayIndex(strItems[ind], leftInd, nright, var->name,
          numElements, &newstr);
        if (!arrInd) {
          SEMMessageBox(newstr + " in line:\n\n" + line, MB_EXCLAME);
          return 2;
        }

        // The returned value for 1D reference without index or 2D with one index is the
        // raw string including newlines so that it can be assigned to a new array
        // variable.  echo and its variants convert them to two spaces; other output like
        // to a file will need conversion too.
        if (var->rowsFor2d && nright2 <= 0) {
          ArrayRow& arrRow = var->rowsFor2d->ElementAt(arrInd - 1);
          value = arrRow.value;
          numElements = arrRow.numElements;
        } else {

          // Get the starting index of the value and of its terminator
          FindValueAtIndex(value, arrInd, beginInd, endInd);
          value = value.Mid(beginInd, endInd - beginInd);
        }
        maxlen += nright - (subInd + maxlen);
      } else if (var->rowsFor2d) {
        if (!subArrSize) {
          SEMMessageBox("Reference to 2D array variable " + var->name +
            " without any subscripts in line:\n\n" + line, MB_EXCLAME);
          return 2;
        }
        numElements = (int)var->rowsFor2d->GetSize();
      }

      // If doing $#, now substitute the number of elements
      if (subArrSize)
        value.Format("%d", numElements);

      // Build up the substituted string
      newstr = "";
      if (subInd)
        newstr = strItems[ind].Left(subInd);
      newstr += value;
      nright = strItems[ind].GetLength() - (nameInd + maxlen);
      if (nright)
        newstr += strItems[ind].Right(nright);
      strItems[ind] = newstr;
    }
  }
  return 0;
}

//  Get an array of floats from a variable
float *CMacroProcessor::FloatArrayFromVariable(CString name, int &numVals, 
  CString &report)
{
  int index, index2, ix0, ix1, numRows = 1;
  float *fvalues;
  float oneVal;
  char *endPtr;
  Variable *var;
  CString *valPtr;
  CString strCopy;
  int *numElemPtr;
  bool noValPtr;

  numVals = 0;
  name.MakeUpper();
  var = GetVariableValuePointers(name, &valPtr, &numElemPtr, "get float array from",
    report);
  if (!var)
    return NULL;
  noValPtr = !valPtr;
  if (noValPtr) {
    numRows = (int)var->rowsFor2d->GetSize();
    for (index = 0; index < numRows; index++) {
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(index);
      numVals += arrRow.numElements;
    }
  } else
    numVals = *numElemPtr;
  if (!numVals) {
    report = "There are no array elements";
    return NULL;
  }
  fvalues = new float[numVals];
  numVals = 0;
  for (index = 0; index < numRows; index++) {
    if (noValPtr) {
      ArrayRow& arrRow = var->rowsFor2d->ElementAt(index);
      valPtr = &arrRow.value;
      numElemPtr = &arrRow.numElements;
    }

    // This function wants array indexes from 1
    for (index2 = 1; index2 <= *numElemPtr; index2++) {
      FindValueAtIndex(*valPtr, index2, ix0, ix1);
      report = valPtr->Mid(ix0, ix1 - ix0);
      oneVal = (float)strtod((LPCTSTR)report, &endPtr);
      if (endPtr - (LPCTSTR)report < report.GetLength()) {
        strCopy = "";
        if (noValPtr)
          strCopy.Format(" in row %d", index);
        report.Format("Cannot get array statistics: item %s at index %d%s of array %s"
          " has non-numeric characters", (LPCTSTR)report, index2 + 1, (LPCTSTR)strCopy,
          (LPCTSTR)name);
        delete[] fvalues;
        return NULL;
      }

      fvalues[numVals++] = oneVal;
    }
  }
  return fvalues;
}

// This checks for the array assignment delimiter being at both ends of the set of
// string items starting from firstInd, and strips it from the set of items
// Returns -1 for unbalanced delimiter, 0 for none, 1 for array
int CMacroProcessor::CheckForArrayAssignment(CString *strItems, int &firstInd)
{
  int last, ifStart, ifEnd, lastLen;
  char endSep = '}', startSep = '{';
  for (last = firstInd; last < MAX_TOKENS; last++)
    if (strItems[last].IsEmpty())
      break;
  if (last == firstInd)
    return 0;
  ifStart = (strItems[firstInd].GetAt(0) == startSep) ? 1 : 0;
  lastLen = strItems[last - 1].GetLength();
  ifEnd = (strItems[last - 1].GetAt(lastLen - 1) == endSep) ? 1 : 0;
  if (ifStart != ifEnd)
    return -1;
  if (!ifStart)
    return 0;
  if (strItems[firstInd] == startSep)
    firstInd++;
  else
    strItems[firstInd] = strItems[firstInd].Right(strItems[firstInd].GetLength() - 1);
  if (strItems[last - 1] == endSep)
    strItems[last - 1] = "";
  else
    strItems[last - 1] = strItems[last - 1].Left(lastLen - 1);
  return 1;
}

// Finds array indexes, if any, starting at position leftInd in item.  Returns 0 if there
// is no index there or it is beyond end of string; 1 for an index, and -1 for various
// errors.  Returns right1 with the index of the first right bracket or 0 for no index, 
// returns right2 with the index a second right brack or 0 if there is no 2nd index
int CMacroProcessor::FindAndCheckArrayIndexes(CString &item, int leftIndex, int &right1,
  int &right2, CString *errStr)
{
  right1 = right2 = 0;
  if (leftIndex >= item.GetLength() || item.GetAt(leftIndex) != '[')
    return 0;
  right1 = item.Find(']', leftIndex);
  if (right1 < 0) {
    if (errStr)
      *errStr = "Unbalanced array index delimiters: [ without ] (is there a space between [ and ]?)";
    return -1;
  }
  if (item.Left(right1).ReverseFind('[') != leftIndex) {
    if (errStr)
      *errStr = "Unbalanced or extra array delimiters: [ followed by two ]";
    return -1;
  }
  if (right1 == item.GetLength() - 1 || item.GetAt(right1 + 1) != '[')
    return 1;
  right2 = item.Find(']', right1 + 1);
  if (right2 < 0) {
    if (errStr)
      *errStr = "Unbalanced array index delimiters for second dimension: [ without ]";
    return -1;
  }
  if (item.Left(right2).ReverseFind('[') != right1 + 1) {
    if (errStr)
      *errStr = "Unbalanced or extra array delimiters: [ followed by two ]";
    return -1;
  }
  return 1;
}

// Convert an array index in a string item given the index of left and right delimiters
// Fill in an error message and return 0 if there is a problem
int CMacroProcessor::ConvertArrayIndex(CString strItem, int leftInd, int rightInd,
  CString name, int numElements, CString *errMess)
{
  int arrInd;
  double dblInd;
  CString temp;
  char *endPtr;
  if (rightInd == leftInd + 1) {
    errMess->Format("Empty array index for variable %s", name);
    return 0;
  }
  temp = strItem.Mid(leftInd + 1, rightInd - (leftInd + 1));
  if (temp.FindOneOf("()-+*/") >= 0 && EvalExpressionInIndex(temp)) {
    *errMess = "Error doing arithmetic in array index " + temp;
    return 0;
  }
  dblInd = strtod((LPCTSTR)temp, &endPtr);
  if (endPtr - (LPCTSTR)temp < temp.GetLength()) {
    errMess->Format("Illegal character in array index %s", (LPCTSTR)temp);
    return 0;
  }
  arrInd = B3DNINT(dblInd);
  if (fabs(arrInd - dblInd) > 0.001) {
    errMess->Format("Array index %s is not close enough to an integer", (LPCTSTR)temp);
    return 0;
  }
  if (arrInd < 1 || arrInd > numElements) {
    errMess->Format("Array index evaluates to %d and is out of range for variable %s",
      arrInd, (LPCTSTR)name);
    return 0;
  }
  return arrInd;
}

// For a variable with possible multiple elements, find element at index arrInd
// (numbered from 1, must be legal) and return starting index and index of terminator
// which is null or \n
void CMacroProcessor::FindValueAtIndex(CString &value, int arrInd, int &beginInd, 
  int &endInd)
{
  endInd = -1;
  for (int valInd = 0; valInd < arrInd; valInd++) {
    beginInd = endInd + 1;
    endInd = value.Find('\n', beginInd);
    if (endInd < 0) {
      endInd = value.GetLength();
      return;
    }
  }
}

// Evaluate an arithmetic expression inside array index delimiters, which cannot contain
// spaces, but expanding all parenthese and operators into separate tokens so that the
// regular arithmetic function can be used.
int CMacroProcessor::EvalExpressionInIndex(CString &indStr)
{
  const int maxItems = 40;
  CString strItems[maxItems];
  CString strLeft, temp = indStr.Mid(1);
  int numItems, opInd, splitInd = -1;
  char opChar;
  bool atStartOfClause = true;
  bool lastWasOp = false;
  if ((indStr.GetAt(0) == '-' || indStr.GetAt(0) == '+') && temp.FindOneOf("()+-*/") < 0)
    return 0;
  strLeft = indStr;
  while (1) {
    opInd = strLeft.FindOneOf("()+-*/");

    // Process operator at start first: if it is + or -, see if last was true operator
    // or we are at start of clause and next is a digit;
    // if so keep it together with next and find operator after that
    if (opInd == 0) {
      opChar = strLeft.GetAt(0);
      if (opChar == '-' || opChar == '+') {
        if (lastWasOp || (atStartOfClause && strLeft.GetLength() > 1 &&
          (char)strLeft.GetAt(1) >= '0' && (char)strLeft.GetAt(1) <= '9')) {
          temp = strLeft.Mid(1);
          opInd = temp.FindOneOf("()+-*/");
          if (opInd > 0)
            opInd++;
        }
      }
    }

    // If no more operators, assign the remaining string
    if (opInd < 0) {
      if (strLeft.GetLength() > 0) {
        splitInd++;
        if (splitInd >= maxItems)
          break;
        strItems[splitInd] = strLeft;
      }
      break;
    }

    // Split to the left of the operator off if any
    if (opInd > 0) {
      splitInd++;
      if (splitInd >= maxItems)
        break;
      strItems[splitInd] = strLeft.Left(opInd);
      lastWasOp = false;
      strLeft = strLeft.Mid(opInd);
      opInd = 0;
    }

    // Record if this was an operator or start of parentheses and split it off
    opChar = strLeft.GetAt(0);
    lastWasOp = opChar == '+' || opChar == '-' || opChar == '*' || opChar == '/';
    atStartOfClause = opChar == '(';
    splitInd++;
    if (splitInd >= maxItems)
      break;
    strItems[splitInd] = opChar;
    strLeft = strLeft.Mid(1);
  }

  if (splitInd >= maxItems) {
    SEMMessageBox("Too many components to evaluate expression in array index");
    return 1;
  }
  if (EvaluateExpression(strItems, splitInd + 1, "", 0, numItems, opInd))
    return 1;
  if (numItems > 1) {
    SEMMessageBox("Array index still has multiple components after doing arithmetic");
    return 1;
  }
  indStr = strItems[0];
  return 0;
}

// Evaluates an arithmetic expression in the array of items
int CMacroProcessor::EvaluateExpression(CString *strItems, int maxItems, CString line,
                                        int ifArray, int &numItems, int &numOrig)
{
  int ind, left, right, maxLevel, level;
  CString cstr;
  std::string str;
  bool noLine = line.GetLength() == 0;

  // Count original number of items
  numItems = 0;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;
  numOrig = numItems;

  // Loop on parenthesis pairs
  for (;;) {

    // Find the highest level and the index of ( and ) for the first occurrence of it
    maxLevel = level = 0;
    left = right = -1;
    for (ind = 0; ind < numItems; ind++) {
      if (strItems[ind] == '(') {
        level++;
        if (level > maxLevel) {
          maxLevel = level;
          left = ind;
          right = -1;
        }
      }
      if (strItems[ind] == ')') {
        if (level == maxLevel && right < 0)
          right = ind;
        level--;
      }
    }

    if (level) {
      SEMMessageBox("Unbalanced parentheses while evaluating an expression in script" +
        CString(noLine ? "" : " line:\n\n") + line, MB_EXCLAME);
        return 1;
    }

    // When no parentheses, do whatever remains directly or return with it
    if (!maxLevel && ifArray) {
      strItems[numItems] = "";
      return 0;
    }
    if (!maxLevel)
      return EvaluateArithmeticClause(strItems, numItems, line, numItems);

    // Otherwise evaluate inside the parentheses; but first move a function inside
    cstr = strItems[left - 1];
    cstr.MakeUpper();
    str = std::string((LPCTSTR)cstr);
    if (left > 0 && (mFunctionSet1.count(str) > 0 || mFunctionSet2.count(str) > 0)) {
      strItems[left] = strItems[left - 1];
      left--;
      strItems[left] = "(";
    }
    if (EvaluateArithmeticClause(&strItems[left + 1], right - left - 1, line, ind))
      return 1;

    // Get rid of the ( and ) and pack down the rest of the items
    strItems[left] = strItems[left + 1];
    for (ind = 0; ind < numItems - right; ind++)
      strItems[left + 1 + ind] = strItems[right + 1 + ind];
    numItems -= right - left;
  }
  return 0;  
}

// Evaluates one arithmetic clause possibly inside parentheses
int CMacroProcessor::EvaluateArithmeticClause(CString * strItems, int maxItems, 
                                              CString line, int &numItems)
{
  int ind, loop;
  double left, right, result = 0.;
  CString str, *strp;
  std::string stdstr;
  numItems = 0;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;

  // Loop on * and / then on + and -
  for (loop = 0; loop < 2; loop++) {
    for (ind = 0; ind < numItems; ind++) {
      strp = &strItems[ind];
      if ((!loop && (*strp == "*" || *strp == "/")) ||
        (loop && (*strp == "+" || *strp == "-"))) {

        // Process minus at start, reject other operators at start/end
        if (!ind && *strp == "-") {
          if (ItemToDouble(strItems[1], line, right))
            return 1;
          result = -right;

          // Replace minus sign and shift strings down by 1
          ReplaceWithResult(result, strItems, 0, numItems, 1);
          ind--;

        }  else if (!ind || ind == numItems - 1) {
           SEMMessageBox("Arithmetic operator at beginning or end of script line or "
             "clause:\n\n" + line, MB_EXCLAME);
           return 1;
        } else {

          // Get the values on either side
          if (ItemToDouble(strItems[ind - 1], line, left) ||
            ItemToDouble(strItems[ind + 1], line, right))
            return 1;

          // Compute value
          if (*strp == "*")
            result = left * right;
          else if (*strp == "+")
            result = left + right;
          else if (*strp == "-")
            result = left - right;
          else {
            if (fabs(right) < 1.e-30 * fabs(left)) {
              SEMMessageBox("Division by 0 or very small number in script line:\n\n" +
                line, MB_EXCLAME);
              return 1;
            }
            result = left / right;
          }

          // Replace left value and shift strings down by 2
          ReplaceWithResult(result, strItems, ind - 1, numItems, 2);
          ind--;
        }
      }
    }
  }

  // Evaluate functions if possible - will really only work at start
  for (ind = 0; ind < numItems; ind++) {
    str = strItems[ind];
    str.MakeUpper();

    // First process items without arguments
    if (str == "RAND") {
      result = rand() / (float)RAND_MAX;
      ReplaceWithResult(result, strItems, ind, numItems, 0);
    }
    if (ind == numItems - 1)
      break;
    stdstr = std::string((LPCTSTR)str);
    if (mFunctionSet1.count(stdstr) > 0) {
      if (ItemToDouble(strItems[ind + 1], line, right))
        return 1;
      if (str == "SQRT") {
        if (right < 0.) {
          SEMMessageBox("Taking square root of negative number in script line:\n\n" +
              line, MB_EXCLAME);
          return 1;
        }
        result = sqrt(right);
      } else if (str == "COS")
        result = cos(DTOR * right);
      else if (str == "SIN")
        result = sin(DTOR * right);
      else if (str == "TAN")
        result = tan(DTOR * right);
      else if (str == "ATAN")
        result = atan(right) / DTOR;
      else if (str == "ABS")
        result = fabs(right);
      else if (str == "NEARINT")
        result = B3DNINT(right);
      else if (str == "LOG" || str == "LOG10") {
        if (right <= 0.) {
          SEMMessageBox("Taking logarithm of non-positive number in script line:\n\n" +
              line, MB_EXCLAME);
          return 1;
        }
        result = str == "LOG" ? log(right) : log10(right);
      } else if (str == "EXP")
        result = exp(right);
      ReplaceWithResult(result, strItems, ind, numItems, 1);
    } else if (mFunctionSet2.count(stdstr) > 0) {
      if (ItemToDouble(strItems[ind + 1], line, left) ||
        ItemToDouble(strItems[ind + 2], line, right))
        return 1;
      if (str == "ATAN2")
        result = atan2(left, right) / DTOR;
      else if (str == "MODULO")
        result = B3DNINT(left) % B3DNINT(right);
      else if (str == "FRACDIFF") {
        result = B3DMAX(fabs(left), fabs(right));
        if (result < 1.e-37)
          result = 0.;
        else
          result = fabs(left - right) / result;
      } else if (str == "DIFFABS")
        result = fabs(left - right);
      else if (str == "ROUND") {
        result = pow(10., B3DNINT(right));
        result = B3DNINT(left * result) / result;
      } else if (str == "POWER")
        result = pow(left, right);
      else if (str == "MIN")
        result = B3DMIN(left, right);
      else if (str == "MIN")
        result = B3DMIN(left, right);
      else if (str == "MAX")
        result = B3DMAX(left, right);
      ReplaceWithResult(result, strItems, ind, numItems, 2);
    }
  }
  return 0;
}

// Evaluates a potentially compound IF statement
int CMacroProcessor::EvaluateIfStatement(CString *strItems, int maxItems, CString line,
                                         BOOL &truth)
{
  int ind, left, right, maxLevel, level, numItems = 0;
  int leftInds[MAX_LOOP_DEPTH];

  if (SeparateParentheses(strItems, maxItems)) {
    AfxMessageBox("Too many items on line after separating out parentheses in script"
      " line:\n\n" + line, MB_EXCLAME);
    return 1;
  }

  // Count original number of items
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++)
    numItems++;

  // Loop on parenthesis pairs containing one or more booleans
  for (;;) {

    // Find the highest level with a boolean and the index of ( and ) for the first 
    // occurrence of it.  Keep track of open paren index by level for this
    maxLevel = level = 0;
    left = right = -1;
    for (ind = 0; ind < numItems; ind++) {
      if (strItems[ind] == '(') {
        level++;
        leftInds[level] = ind;
      }
      if (strItems[ind] == ')') {
        if (level == maxLevel && right < 0)
          right = ind;
        level--;
      }
      if ((!strItems[ind].CompareNoCase("AND") || !strItems[ind].CompareNoCase("OR")) && 
        level > maxLevel) {
        maxLevel = level;
        left = leftInds[level];
        right = -1;
      }
    }

    if (level) {
      AfxMessageBox("Unbalanced parentheses while evaluating an expression in script"
        " line:\n\n" + line, MB_EXCLAME);
        return 1;
    }

    // When no parentheses, do whatever remains directly
    if (!maxLevel)
      return EvaluateBooleanClause(strItems, numItems, line, truth);

    // Otherwise evaluate inside the parentheses
    if (EvaluateBooleanClause(&strItems[left + 1], right - left - 1, line, truth))
      return 1;

    // Replace the ( ) with a true or false statement and pack down the rest
    strItems[left] = "1";
    strItems[left + 1] = "==";
    strItems[left + 2] = truth ? "1" : "0";
    for (ind = 0; ind < numItems - right; ind++)
      strItems[left + 3 + ind] = strItems[right + 1 + ind];
    numItems -= right - left - 2;
  }
  return 1; 
}

// Evaluates a unit of an IF statement containing one or more boolean operators at one
// parenthetical level
int CMacroProcessor::EvaluateBooleanClause(CString * strItems, int maxItems, CString line,
                                           BOOL &truth)
{
  int ind, numItems, opind, indstr = 0;
  BOOL clauseVal, lastAnd;

  // Loop on logical operators
  for (;;) {
    opind = -1;
    numItems = 0;

    // Find next logical operator if any
    for (ind = indstr; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
      if (!strItems[ind].CompareNoCase("OR") || !strItems[ind].CompareNoCase("AND")) {
        opind = ind;
        break;
      }
      numItems++;
    }

    // Check for one end or other of clause/line
    if (opind == indstr || opind == maxItems - 1 || strItems[opind + 1].IsEmpty()) {
      AfxMessageBox("AND/OR operator at start of line or clause or at end of line "
        "in script line:\n\n" + line, MB_EXCLAME);
      return 1;
    }

    // get the value of the clause and assign for first clause, or AND or OR with truth
    if (EvaluateComparison(&strItems[indstr], numItems, line, clauseVal))
      return 1;
    if (!indstr)
      truth = clauseVal;
    else if (lastAnd)
      truth = truth && clauseVal;
    else
      truth = truth || clauseVal;

    // Done if no more operators, otherwise set up for next clause
    if (opind < 0)
      break;
    lastAnd = !strItems[opind].CompareNoCase("AND");
    indstr = opind + 1;
  }
  return 0;
}

// Evaluates an IF clause for a comparison operator and arithmetic expressions on
// each side, returns value in truth
int CMacroProcessor::EvaluateComparison(CString * strItems, int maxItems, CString line,
                                         BOOL &truth)
{
  int ind, numItems = 0, opind = -1, numLeft = 1, numRight = 1;
  double left, right;
  CString *str;
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    numItems++;
    str = &strItems[ind];
    if (*str == "==" || *str == "!=" || *str == ">=" || *str == "<=" || *str == "<" ||
      *str == ">") {
      if (opind >= 0) {
        AfxMessageBox("More than one comparison operator in clause in script line:\n\n" +
           line, MB_EXCLAME);
         return 1;
      }
      opind = ind;
    }
  }
  if (opind < 0) {
    AfxMessageBox("No comparison operator for IF statement clause in script line:\n\n" +
      line, MB_EXCLAME);
    return 1;
  }

  // Strip parentheses around the entire clause by adjusting pointer, index and count
  if (strItems[0] == "(" && strItems[numItems - 1] == ")") {
    strItems++;
    opind--;
    numItems -= 2;
  }
  if (!opind || opind == numItems - 1) {
    AfxMessageBox("Comparison operator at beginning or end of IF statement/clause:\n\n" +
      line, MB_EXCLAME);
    return 1;
  }

  // If there is more than one item, evaluate expressions, make sure there is only one
  if (opind > 1 && EvaluateExpression(strItems, opind, line, 0, numLeft, ind))
    return 1;
  if (opind < numItems - 2 && EvaluateExpression(&strItems[opind + 1],
    numItems - 1 - opind, line, 0, numRight, ind))
    return 1;
  if (numLeft > 1 || numRight > 1) {
    AfxMessageBox("Items being compared do not reduce to a single number in script line:"
      "\n\n" + line, MB_EXCLAME);
    return 1;
  }

  // Convert the single strings to values and evaluate
  if (ItemToDouble(strItems[0], line, left))
    return 1;
  if (ItemToDouble(strItems[opind + 1], line, right))
    return 1;
  str = &strItems[opind];
  if (*str == ">")
    truth = left > right;
  else if (*str == "<")
    truth = left < right;
  else if (*str == ">=")
    truth = left >= right;
  else if (*str == "<=")
    truth = left <= right;
  else if (*str == "==")
    truth = fabs(left - right) <= 1.e-10 * B3DMAX(fabs(left), fabs(right));
  else if (*str == "!=")
    truth = fabs(left - right) > 1.e-10 * B3DMAX(fabs(left), fabs(right));

  return 0;
}

// Convert an item being used for arithmetic to a double
BOOL CMacroProcessor::ItemToDouble(CString str, CString line, double & value)
{
  char *strptr = (char *)(LPCTSTR)str;
  char *invalid;
  value = strtod(strptr, &invalid);
  if (invalid - strptr != str.GetLength()) {
    SEMMessageBox("Invalid character in " +  str +
      " which you are trying to do arithmetic with in script line:\n\n" +
      line, MB_EXCLAME);
    return true;
  }
  return false;
}

// Replace the item at index in the array of items with text for the result value
// and shift the remainder of the items down as needed (by 1 or 2)
void CMacroProcessor::ReplaceWithResult(double result, CString * strItems, int index,
                                        int & numItems, int numDrop)
{
  strItems[index].Format("%f", result);
  TrimTrailingZeros(strItems[index]);
  numItems -= numDrop;
  for (int lr = index + 1; lr < numItems; lr++)
    strItems[lr] = strItems[lr + numDrop];
}

// Saves reported values in variables
void CMacroProcessor::SetReportedValues(double val1, double val2, double val3,
                                       double val4, double val5, double val6)
{
  SetReportedValues(NULL, val1, val2, val3, val4, val5, val6);
}

void CMacroProcessor::SetReportedValues(CString *strItems, double val1, double val2, double val3,
  double val4, double val5, double val6)
{
  if (val1 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val1, 1);
  if (val2 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val2, 2);
  if (val3 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val3, 3);
  if (val4 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val4, 4);
  if (val5 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val5, 5);
  if (val6 != MACRO_NO_VALUE)
    SetOneReportedValue(strItems, val6, 6);
}

// Sets one reported value with a numeric or string value, also sets an optional variable
// Pass the address of the FIRST optional variable
// Clears all report variables when index is 1 and then requires report variables to
// be new, so call with 1 first
void CMacroProcessor::SetOneReportedValue(CString *strItems, CString *valStr, 
  double value, int index)
{
  CString num, str;
  if (index < 1 || index > 6)
    return;
  num.Format("%d", index);
  if (valStr)
    str = *valStr;
  else {
    str.Format("%f", value);
    TrimTrailingZeros(str);
  }
  if (index == 1)
    ClearVariables(VARTYPE_REPORT);
  SetVariable("REPORTEDVALUE" + num, str, VARTYPE_REPORT, index, true);
  SetVariable("REPVAL" + num, str, VARTYPE_REPORT, index, true);
  if (strItems && !strItems[index - 1].IsEmpty())
    SetVariable(strItems[index - 1], str, VARTYPE_REGULAR, -1, false);
}

// Overloads for convenience
void CMacroProcessor::SetOneReportedValue(CString &valStr, int index)
{
  SetOneReportedValue(NULL, &valStr, 0., index);
}
void CMacroProcessor::SetOneReportedValue(double value, int index)
{
  SetOneReportedValue(NULL, NULL, value, index);
}
void CMacroProcessor::SetOneReportedValue(CString *strItem, CString &valStr, int index)
{
  SetOneReportedValue(strItem, &valStr, 0., index);
}
void CMacroProcessor::SetOneReportedValue(CString *strItem, double value, int index)
{
  SetOneReportedValue(strItem, NULL, value, index);
}

// Checks the nesting of IF and LOOP blocks in the given macro, as well
// as the appropriateness of ELSE, BREAK, CONTINUE
int CMacroProcessor::CheckBlockNesting(int macroNum, int startLevel)
{
  int blockType[MAX_LOOP_DEPTH];
  bool elseFound[MAX_LOOP_DEPTH];
  CString strLabels[MAX_MACRO_LABELS], strSkips[MAX_MACRO_SKIPS];
  int labelBlockLevel[MAX_MACRO_LABELS], labelSubBlock[MAX_MACRO_LABELS];
  int skipBlockLevel[MAX_MACRO_SKIPS], skipCurIndex[MAX_MACRO_SKIPS];
  int labelCurIndex[MAX_MACRO_LABELS];
  short skipSubBlocks[MAX_MACRO_SKIPS][MAX_LOOP_DEPTH + 1];
  int blockLevel = startLevel;
  bool stringAssign, inFunc = false;
  MacroFunction *func;
  int subBlockNum[MAX_LOOP_DEPTH + 1];
  CString *macro = &mMacros[macroNum];
  int numLabels = 0, numSkips = 0;
  int i, inloop, needVers, index, skipInd, labInd, length, cmdIndex, currentIndex = 0;
  int currentVersion = 202;
  CString strLine, strItems[MAX_TOKENS], errmess, intCheck;
  const char *features[] = {"variable1", "arrays", "keepcase", "zeroloop", "evalargs"};
  int numFeatures = sizeof(features) / sizeof(char *);

  mAlreadyChecked[macroNum] = true;
  for (i = 0; i <= MAX_LOOP_DEPTH; i++)
    subBlockNum[i] = 0;

  errmess.Format(" in script #%d", macroNum + 1);
  while (currentIndex < macro->GetLength()) {
    GetNextLine(macro, currentIndex, strLine);
    if (mWinApp->mParamIO->ParseString(strLine, strItems, MAX_TOKENS))
      FAIL_CHECK_LINE("Too many items on line");
    if (!strItems[0].IsEmpty()) {
      strItems[0].MakeUpper();
      InsertDomacro(&strItems[0]);
      cmdIndex = LookupCommandIndex(strItems[0]);

      // Check validity of variable assignment then skip further checks
      stringAssign = strItems[1] == "@=" || strItems[1] == ":@=";
      if (strItems[1] == "=" || strItems[1] == ":=" || stringAssign) {
        index = 2;
        if (!stringAssign) {
          i = CheckForArrayAssignment(strItems, index);
          if (i < 0)
            FAIL_CHECK_LINE("Array delimiters \"{}\" are not at start and end of values");
        }
        if (strItems[1] == "=" && strItems[index].IsEmpty())
          FAIL_CHECK_LINE("Empty assignment in line");
        if (WordIsReserved(strItems[0]))
          FAIL_CHECK_LINE("Assignment to reserved command name in line");
        if (strItems[0].GetAt(0) == '$')
          FAIL_CHECK_LINE("You cannot assign to a name that is a variable in line");
        if (!stringAssign &&
          CheckBalancedParentheses(strItems, MAX_TOKENS, strLine, errmess))
          return 99;
        continue;
      }

      // If the command is a variable, skip checks except to make sure it is not a label
      if (strItems[0].GetAt(0) == '$') {
        if (strItems[0].GetAt(strItems[0].GetLength() - 1) == ':')
          FAIL_CHECK_LINE("Label cannot be a variable in line");
        continue;
      }

      // See if a feature is required
      if (CMD_IS(REQUIRE)) {
        for (i = 1; i < MAX_TOKENS; i++) {
          if (strItems[i].IsEmpty())
            break;
          inloop = 0;
          for (index = 0; index < numFeatures; index++)
            if (!strItems[i].CompareNoCase(features[index]))
              inloop = 1;
          if (!inloop) {
            needVers = atoi(strItems[1]);
            if (needVers > 0) {
              if (needVers <= currentVersion) {
                inloop = 1;
              } else {
                 intCheck.Format("Required scripting version %d is not provided by this\n"
                   "version of SerialEM, which has scripting version %d.\n"
                   "If this number is right, then you need a later version."
                   "\nThe Require command is", needVers, currentVersion);
                 FAIL_CHECK_NOLINE(intCheck);
              }
            }
          }
          if (!inloop) {
            intCheck.Format("Required feature \"%s\" is not available in this version\n"
              "of SerialEM.  If this is spelled right, then you need a later version\n\n"
              "Features available in this version are:\n", (LPCTSTR)strItems[1]);
            for (index = 0; index < numFeatures; index++)
              intCheck += CString(" ") + features[index];
            intCheck += "\nThe Require command is";
            FAIL_CHECK_NOLINE(intCheck);
          }
        }
      }

      // Validate a label and record its level and sub-block #
      length = strItems[0].GetLength();
      if (strItems[0].GetAt(length - 1) == ':') {
        if (!strItems[1].IsEmpty())
          FAIL_CHECK_LINE("Label must not be followed by anything in line");
        if (length == 1)
          FAIL_CHECK_LINE("Empty label in line");
        if (numLabels >= MAX_MACRO_LABELS)
          FAIL_CHECK_NOLINE("Too many labels in script");
        strLabels[numLabels] = strItems[0].Left(length - 1);
        for (i = 0; i < numLabels; i++) {
          if (strLabels[i] == strLabels[numLabels])
            FAIL_CHECK_LINE("Duplicate of label already used in line")
        }
        labelCurIndex[numLabels] = currentIndex;
        labelBlockLevel[numLabels] = blockLevel;
        labelSubBlock[numLabels++] = subBlockNum[1 + blockLevel];
        continue;
      }
        
      // Repeat and DoMacro may not be called from inside any nest or called macro
      if ((CMD_IS(REPEAT) || CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT)) && 
        (blockLevel >= 0 || mCallLevel))
        FAIL_CHECK_LINE("Repeat, DoMacro, or DoScript statement cannot be used\n"
        "inside a called script, an IF block, or a loop,");

      if (CMD_IS(REPEAT))
        break;

      // Check macro call if it has an argument - leave to general command check if not
      else if ((CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT) || CMD_IS(CALLMACRO) || 
        CMD_IS(CALLSCRIPT) || CMD_IS(CALL) || CMD_IS(CALLFUNCTION)) && 
        !strItems[1].IsEmpty()) {
          func = NULL;

          if (strItems[1].GetAt(0) == '$')
            FAIL_CHECK_LINE("The script number or name cannot be a variable,");
          if (CMD_IS(CALL)) {
            index = FindCalledMacro(strLine, true);
            if (index < 0)
              return 12;
          } else if (CMD_IS(CALLFUNCTION)) {
            func = FindCalledFunction(strLine, true, index, i, macroNum);
            if (!func)
              return 12;
          } else {
            index = atoi(strItems[1]) - 1;

            // If this is an unchecked macro, better unload the macro if editor open
            if (index >= 0 && index < MAX_MACROS && !mAlreadyChecked[index] &&
              mMacroEditer[index])
              mMacroEditer[index]->TransferMacro(true);

            if (index < 0 || index >= MAX_MACROS || mMacros[index].IsEmpty())
              FAIL_CHECK_LINE("Trying to do illegal script number or empty script,");
          }

          // For calling a macro, check that it is not too deep and that it's not circular
          // There is no good way to check circular function calls, so this relies on 
          // run-time testing
          if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))) {
            mCallLevel++;
            if (mCallLevel >= MAX_CALL_DEPTH)
              FAIL_CHECK_LINE("Too many nested script calls");
            if (!CMD_IS(CALLFUNCTION)) {
              for (i = 0; i < mCallLevel; i++) {
                if (mCallMacro[i] == index && mCallFunction[i] == func)
                  FAIL_CHECK_LINE("Trying to call a script that is already calling a "
                  "script");
              }
            }

            mCallMacro[mCallLevel] = index;
            mCallFunction[mCallLevel] = func;
          }

          // Now call this function reentrantly to check macro, unless it is a DoMacro
          // and already checked
          if (!(CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT) || CMD_IS(CALLFUNCTION)) ||
            !mAlreadyChecked[index]) {
            mAlreadyChecked[index] = true;
            if (CheckBlockNesting(index, blockLevel))
              return 14;
          }

          // Skip out if doing macro; otherwise pop stack again
          if (CMD_IS(DOMACRO) || CMD_IS(DOSCRIPT))
            break;
          mCallLevel--;

      // Examine loops
      } else if ((CMD_IS(LOOP) && !strItems[1].IsEmpty()) || 
        (CMD_IS(DOLOOP) && !strItems[3].IsEmpty()) || 
        (CMD_IS(IF) && !strItems[3].IsEmpty())) {
        blockLevel++;
        subBlockNum[1 + blockLevel]++;
        if (blockLevel >= MAX_LOOP_DEPTH)
          FAIL_CHECK_NOLINE("Too many nested LOOP/DOLOOP and IF statements");
        blockType[blockLevel] = CMD_IS(IF) ? 1 : 0;
        elseFound[blockLevel] = false;
        if (CMD_IS(IF) && CheckBalancedParentheses(strItems, MAX_TOKENS, strLine,errmess))
          return 99;

      } else if (CMD_IS(ELSE) || CMD_IS(ELSEIF)) {
        subBlockNum[1 + blockLevel]++;
        if (blockLevel <= startLevel || !blockType[blockLevel])
          FAIL_CHECK_NOLINE("ELSE or ELSEIF statement not in an IF block");
        if (elseFound[blockLevel])
          FAIL_CHECK_NOLINE("ELSEIF or ELSE following an ELSE statement in an IF block");
        if (CMD_IS(ELSE))
          elseFound[blockLevel] = true;
        if (CMD_IS(ELSE) && strItems[1].MakeUpper() == "IF")
          FAIL_CHECK_NOLINE("ELSE IF must be ELSEIF without a space");
        if (CMD_IS(ELSE) && !strItems[1].IsEmpty())
          FAIL_CHECK_NOLINE("ELSE line with extraneous entries after the ELSE");
        if (CMD_IS(ELSEIF) && CheckBalancedParentheses(strItems, MAX_TOKENS, strLine, 
          errmess))
          return 99;

      } else if (CMD_IS(ENDLOOP)) {
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("ENDLOOP without corresponding LOOP or DOLOOP statement");
        if (blockType[blockLevel])
          FAIL_CHECK_NOLINE("ENDLOOP contained in an IF block");
        blockLevel--;
      } else if (CMD_IS(ENDIF)) {
        if (blockLevel <= startLevel)
          FAIL_CHECK_NOLINE("ENDIF without corresponding IF statement");
        if (!blockType[blockLevel])
          FAIL_CHECK_NOLINE("ENDIF contained in a LOOP or DOLOOP block");
        blockLevel--;
      } else if (CMD_IS(BREAK) || CMD_IS(KEYBREAK) || CMD_IS(CONTINUE)) {
        inloop =0;
        for (i = startLevel + 1; i <= blockLevel; i++) {
          if (!blockType[i])
            inloop = 1;
        }
        if (!inloop)
          FAIL_CHECK_NOLINE("BREAK, KEYBREAK, or CONTINUE not contained in a LOOP or "
            "DOLOOP block");
      } else if (CMD_IS(SKIPTO) && !strItems[1].IsEmpty()) {

        // For a SkipTo, record the location and level and subblock at all lower levels
        if (numSkips >= MAX_MACRO_SKIPS)
          FAIL_CHECK_NOLINE("Too many SkipTo commands in script");
        strSkips[numSkips] = strItems[1].MakeUpper();
        skipCurIndex[numSkips] = currentIndex;
        for (i = startLevel; i <= blockLevel; i++)
          skipSubBlocks[numSkips][1 + i] = subBlockNum[1 + i];
        skipBlockLevel[numSkips++] = blockLevel;

      } else if (CMD_IS(FUNCTION) && !strItems[1].IsEmpty()) {

        // For a function, the loop level must be back to base, must not be in a function,
        // and the 3rd and 4th items must be integers
        if (inFunc)
          FAIL_CHECK_LINE("Starting a new function without ending previous one on line");
        inFunc = true;
        if (blockLevel > startLevel)
          FAIL_CHECK_LINE("Unclosed IF, LOOP, or DOLOOP block in script before function");
        for (i = 2; i < 4; i++) {
          intCheck = "";
          if (!strItems[i].IsEmpty()) {
            index = atoi((LPCTSTR)strItems[i]);
            intCheck.Format("%d", index);
          }
          if (intCheck != strItems[i])
            FAIL_CHECK_LINE("Function name must be in one word without spaces, followed"
              " by\nnumber of arguments and integer flag for whether there is a string "
              "argument");
        }

      } else if (CMD_IS(ENDFUNCTION)) {
        if (!inFunc)
          FAIL_CHECK_NOLINE("An EndFunction command occurred when not in a function");
        inFunc = false;
        if (blockLevel > startLevel)
          FAIL_CHECK_NOLINE("Unclosed IF, LOOP, or DOLOOP block inside function");

      } else {

        i = CheckLegalCommandAndArgNum(&strItems[0], strLine, macroNum);
        if (i == 17)
          return 17;
        if (i)
          FAIL_CHECK_LINE("Unrecognized command in line");
      }

      // Check commands where arithmetic is allowed
      if (ArithmeticIsAllowed(strItems[0]) && 
        CheckBalancedParentheses(strItems, MAX_TOKENS, strLine, errmess))
        return 99;
      
      if (CMD_IS(SKIPIFVERSIONLESSTHAN) && currentIndex < macro->GetLength())
        GetNextLine(macro, currentIndex, strLine);
    }
  }
  if (blockLevel > startLevel)
    FAIL_CHECK_NOLINE("Unclosed IF, LOOP, or DOLOOP block");

  if (!numSkips)
    return 0;

  // Validate the labels if there are any skips
  for (skipInd = 0; skipInd < numSkips; skipInd++) {
    for (labInd = 0; labInd < numLabels; labInd++) {
      if (strSkips[skipInd] == strLabels[labInd]) {
        if (labelCurIndex[labInd] < skipCurIndex[skipInd])
          FAIL_CHECK_NOLINE(CString("You cannot skip backwards; label occurs before "
          "SkipTo ") + strSkips[skipInd]);
        if (labelBlockLevel[labInd] > skipBlockLevel[skipInd])
          FAIL_CHECK_NOLINE(CString("Trying to skip into a higher level LOOP, DOLOOP, or"
            " IF block with SkipTo ") + strSkips[skipInd]);
        if (labelSubBlock[labInd] != skipSubBlocks[skipInd][1 + labelBlockLevel[labInd]])
          FAIL_CHECK_NOLINE(CString("Trying to skip into a different LOOP/DOLOOP block or"
          " section of IF block with SkipTo ") + strSkips[skipInd]);
        break;
      }
    }
    if (labInd == numLabels)
      FAIL_CHECK_NOLINE(CString("Label not found for SkipTo ") + strSkips[skipInd]);
  }
  return 0;
}


// Check for whether the command is legal and has enough arguments
// Returns 17 if there are not enough arguments, 1 if not legal command
int CMacroProcessor::CheckLegalCommandAndArgNum(CString * strItems, CString strLine, 
  int macroNum)
{
  int i = 0;
  CString errmess;

  // Loop on the commands
  while (cmdList[i].cmd.length()) {
    if (strItems[0] == cmdList[i].cmd.c_str()) {
      if (strItems[cmdList[i].minargs].IsEmpty()) {
        errmess.Format("The command must be followed by at least %d entries\n"
          " on this line in script #%d:\n\n", cmdList[i].minargs, macroNum + 1);
        SEMMessageBox(errmess + strLine, MB_EXCLAME);
        return 17;
      }
      return 0;
    }
    i++;
  }
  return 1;
}

// Check a line for balanced parentheses and no empty clauses
int CMacroProcessor::CheckBalancedParentheses(CString *strItems, int maxItems, 
                                              CString &strLine, CString &errmess)
{
  int ind, level = 0;
  if (SeparateParentheses(strItems, maxItems))
    FAIL_CHECK_LINE("Too many items in line after separating out parentheses")
    
  for (ind = 0; ind < maxItems && !strItems[ind].IsEmpty(); ind++) {
    if (strItems[ind] == "(") {
      level++;
    }
    if (strItems[ind] == ")") {
      level--;
      if (level < 0)
        FAIL_CHECK_LINE("Unbalanced parentheses");
      if (strItems[ind - 1] == "(")
        FAIL_CHECK_LINE("Nothing inside parentheses");
    }
  }
  if (level != 0)
    FAIL_CHECK_LINE("Unbalanced parentheses");
  return 0;
}

// Expand the line to put parentheses in separate items, return error if too many items
int CMacroProcessor::SeparateParentheses(CString *strItems, int maxItems)
{
  int icopy, length, ind = maxItems - 1;
  int numItems = -1;
  while (ind >= 0) {

    // Move from the back end, skip empty items, and initialize number of items
    if (strItems[ind].IsEmpty()) {
      ind--;
      continue;
    }
    if (numItems < 0)
      numItems = ind + 1;
    length = strItems[ind].GetLength();

    // Expand an item with more than one character
    if (length > 1 && (strItems[ind].GetAt(0) == '(' ||
      strItems[ind].GetAt(length - 1) == ')')) {
        if (numItems >= maxItems)
          return 1;
        for (icopy = numItems; icopy > ind + 1; icopy--)
          strItems[icopy] = strItems[icopy - 1];
        numItems++;
        if (strItems[ind].GetAt(0) == '(') {
          strItems[ind + 1] = strItems[ind].Right(length - 1);
          strItems[ind] = "(";
          ind++;
        } else {
          strItems[ind] = strItems[ind].Left(length - 1);
          strItems[ind + 1] = ")";
        }
    } else {
      ind--;
    }
  }
  return 0;
}

void CMacroProcessor::PrepareForMacroChecking(int which)
{
  for (int i = 0; i < MAX_TOT_MACROS; i++)
    mAlreadyChecked[i] = false;
  mCallLevel = 0;
  mCallMacro[0] = which;
}

// Skips to the end of a block or to an else statement, leaves current index
// at start of statement to be processed (the end statement, or past an else)
int CMacroProcessor::SkipToBlockEnd(int type, CString line)
{
  CString *macro = &mMacros[mCurrentMacro];
  CString strLine, strItems[4];
  int ifLevel = 0, loopLevel = 0;
  int nextIndex = mCurrentIndex, cmdIndex;
  while (nextIndex < macro->GetLength()) {
    mCurrentIndex = nextIndex;
    GetNextLine(macro, nextIndex, strLine);
    if (!strLine.IsEmpty()) {
      mWinApp->mParamIO->ParseString(strLine, strItems, 4);
      strItems[0].MakeUpper();
      cmdIndex = LookupCommandIndex(strItems[0]);

      // If we're at same block level as we started and we find end, return
      if ((!ifLevel && ((type == SKIPTO_ELSE_ENDIF && (CMD_IS(ELSE) || CMD_IS(ELSEIF)))
        || ((type == SKIPTO_ENDIF || type == SKIPTO_ELSE_ENDIF) && CMD_IS(ENDIF))) ||
        (!loopLevel && type == SKIPTO_ENDLOOP && CMD_IS(ENDLOOP)))) {
        if (CMD_IS(ELSE))
          mCurrentIndex = nextIndex;
        return 0;
      }

      // Otherwise keep track of the block level as it goes up and down
      if (CMD_IS(LOOP))
        loopLevel++;
      if (CMD_IS(IF))
        ifLevel++;
      if (CMD_IS(ENDLOOP))
        loopLevel--;
      if (CMD_IS(ENDIF))
        ifLevel--;
    }
  }
  AfxMessageBox("No appropriate statement found to skip forward to from script "
    "line:\n\n" + line, MB_EXCLAME);
  return 1;
}

// Skip to a label and return the number of blocks levels to descend
int CMacroProcessor::SkipToLabel(CString label, CString line, int &numPops)
{
  CString *macro = &mMacros[mCurrentMacro];
  CString strLine, strItems[4];
  int nextIndex = mCurrentIndex;
  int cmdIndex;
  label += ":";
  numPops = 0;
  while (nextIndex < macro->GetLength()) {
    mCurrentIndex = nextIndex;
    GetNextLine(macro, nextIndex, strLine);
    if (!strLine.IsEmpty()) {
      mWinApp->mParamIO->ParseString(strLine, strItems, 4);
      strItems[0].MakeUpper();
      cmdIndex = LookupCommandIndex(strItems[0]);

      // For a match, make sure there is not a negative number of pops
      if (strItems[0] == label) {
        if (numPops >= 0)
          return 0;
        AfxMessageBox("Trying to skip into a higher block level in script line:\n\n" +
          line, MB_EXCLAME);
        return 1;
      }

      // Otherwise keep track of the block level as it goes up and down
      if (CMD_IS(LOOP) || CMD_IS(IF))
        numPops--;
      if (CMD_IS(ENDLOOP) || CMD_IS(ENDIF))
        numPops++;
    }
  }
  AfxMessageBox("No label found to skip forward to from script line:\n\n" + line, 
    MB_EXCLAME);
  return 1;
}

// Convert a single number to a DOMACRO
void CMacroProcessor::InsertDomacro(CString * strItems)
{
  if (strItems[1].IsEmpty()) {
    for (int i = 0; i < MAX_MACROS; i++) {
      if (mStrNum[i] == strItems[0]) {
        strItems[0] = "DOMACRO";
        strItems[1] = mStrNum[i];
        break;
      }
    }
  }
}

// Common place to check for reserved control commands
BOOL CMacroProcessor::WordIsReserved(CString str)
{
  std::string stdStr = (LPCTSTR)str;
  return mReservedWords.count(stdStr) > 0;
}

// Common place to check if arithmetic is allowed for a command
bool CMacroProcessor::ArithmeticIsAllowed(CString &str)
{
  std::string sstr = (LPCTSTR)str;
  return ((str.Find("SET") == 0 && mArithDenied.count(sstr) <= 0) || 
    mArithAllowed.count(sstr) > 0);
}

// Remove zeros from end of a string to be stored in a variable
void CMacroProcessor::TrimTrailingZeros(CString &str)
{
  int length, trim = 0;
  if (str.Find('.')) {
    length = str.GetLength();
    while (str.GetAt(length - trim - 1) == '0')
      trim++;
    if (str.GetAt(length - trim - 1) == '.')
      trim++;
    if (trim)
      str = str.Left(length - trim);
  }
}

// Check for the return values from an intensity change and issue warning or message
int CMacroProcessor::CheckIntensityChangeReturn(int err)
{
  CString report;
  if (err) {
    if (err == BEAM_STARTING_OUT_OF_RANGE || err == BEAM_ENDING_OUT_OF_RANGE) {
      mWinApp->AppendToLog("Warning: attempting to set beam strength beyond"
        " calibrated range", LOG_OPEN_IF_CLOSED);
    } else {
      report = "Error trying to change beam strength";
      if (err == BEAM_STRENGTH_NOT_CAL || err == BEAM_STRENGTH_WRONG_SPOT)
        report += "\nBeam strength is not calibrated for this spot size";
      SEMMessageBox(report, MB_EXCLAME);
      AbortMacro();
      return 1;
    }
  }
  return 0;
}

// Make sure a filename is not empty and try to convert to a full path
int CMacroProcessor::CheckConvertFilename(CString * strItems, CString strLine, int index,
                                          CString & strCopy)
{
  char absPath[_MAX_PATH];
  char *fullp;
  if (strItems[index].IsEmpty()) {
    AbortMacro();
    AfxMessageBox("Missing filename in statement:\n\n" + strLine, MB_EXCLAME);
    return 1;
  }

  // Get the filename in original case
  SubstituteVariables(&strLine, 1, strLine);
  mWinApp->mParamIO->StripItems(strLine, index, strCopy);
  fullp = _fullpath(absPath, (LPCTSTR)strCopy, _MAX_PATH);
  if (!fullp) {
    AbortMacro();
    AfxMessageBox("The filename cannot be converted to an absolute path in statement:"
      "\n\n" + strLine, MB_EXCLAME);
    return 1;
  }
  strCopy = fullp;
  return 0;
}

CmdItem *CMacroProcessor::GetCommandList(int & numCommands)
{
  numCommands = NUM_COMMANDS - 1;
  return &cmdList[0];
}

// Converts a buffer letter in the item to a buffer index, supplies emptyDefault as the
// result for an empty and returns false if that is >= 0, true otherwise; checks legality
// of letter and also of image in buffer if checkImage true; composes message on error
bool CMacroProcessor::ConvertBufferLetter(CString strItem, int emptyDefault,
                                          bool checkImage, int &bufIndex,
                                          CString &message)
{
  strItem.MakeUpper();
  if (strItem.IsEmpty()) {
    bufIndex = emptyDefault;
    if (bufIndex < 0) {
      message = "No buffer letter is present in statement: \r\n\r\n";
      return true;
    }
  } else {
    bufIndex = atoi((LPCTSTR)strItem);
    if (!bufIndex) {
      bufIndex = (int)strItem.GetAt(0) - (int)'A';
      if (bufIndex < 0 || bufIndex >= MAX_BUFFERS || strItem.GetLength() > 1) {
        message = "Improper buffer letter " + strItem + " in statement: \r\n\r\n";
        return true;
      }
    } else {
      if (bufIndex < 1 || bufIndex > MAX_BUFFERS || strItem.GetLength() > 
        1 + (bufIndex / 10)) {
          message = "Improper buffer number " + strItem + " in statement: \r\n\r\n";
          return true;
      }
      bufIndex--;
   }
  }
  if (checkImage && !mImBufs[bufIndex].mImage) {
    message = "There is no image in buffer " + strItem + " in statement: \r\n\r\n";
    return true;
  }
  return false;
}

// Takes incoming  binning and scales binning
// for K2, and checks that binning exists.  Returns true with message on error
bool CMacroProcessor::CheckCameraBinning(double binDblIn, int &binning, CString &message)
{
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  binning = B3DNINT(BinDivisorF(camParams) * binDblIn);
  for (int ix0 = 0; ix0 < camParams->numBinnings; ix0++)
    if (binning == camParams->binnings[ix0])
      return false;
  message = "Not a valid binning for camera in: \n\n";
  return true;
}

// Takes incoming set number or letter in strItem and itemInt, checks its validity, and 
// puts a set number converted from a letter in itemInt and copy the number to index
// Returns tre with message on error
bool CMacroProcessor::CheckAndConvertCameraSet(CString &strItem, int &itemInt, int &index,
                                             CString &message)
{
  char letter;
  itemInt = -1;

  // To allow strings from a reported value, if it starts with a digit, strip trailing 0's
  // and the decimal place if that is all that is after the digit
  letter = (char)strItem.GetAt(0);
  if (letter >= '0' && letter <= '9') {
    while (strItem.Find('.') >= 0 && strItem.Right(1) == "0")
      strItem.Delete(strItem.GetLength() - 1);
    if (strItem.GetLength() == 2 || strItem.Right(1) == ".")
      strItem.Delete(1);
  }
  if (strItem.GetLength() == 1) {
    letter = (char)strItem.GetAt(0);
    if (!strItem.CompareNoCase("V"))
      itemInt = 0;
    else if (!strItem.CompareNoCase("F"))
      itemInt = 1;
    else if (!strItem.CompareNoCase("T"))
      itemInt = 2;
    else if (!strItem.CompareNoCase("R"))
      itemInt = 3;
    else if (!strItem.CompareNoCase("P"))
      itemInt = 4;
    else if (letter >= '0' && letter <= '9') 
      itemInt = atoi((LPCTSTR)strItem);
    if (itemInt > NUMBER_OF_USER_CONSETS)
      itemInt = -1;
    index = itemInt;
    if (itemInt >= 0)
      return false;
  }
  message = "Inappropriate parameter set letter/number in: \n\n";
  return true;
}

// If adjustment of beam tilt for given image shift is requested and available, do it
int CMacroProcessor::AdjustBeamTiltIfSelected(double delISX, double delISY, BOOL doAdjust,
  CString &message)
{
  double delBTX, delBTY, transISX, transISY;
  int probe = mScope->GetProbeMode();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  if (!doAdjust)
    return 0;
  if (comaVsIS->magInd <= 0) {
    message = "There is no calibration of beam tilt needed to compensate image shift "
      "in:\n\n";
    return 1;
  }
  if (mBeamTiltXtoRestore[probe] < EXTRA_VALUE_TEST) {
    mScope->GetBeamTilt(mBeamTiltXtoRestore[probe], mBeamTiltYtoRestore[probe]);
    mNumStatesToRestore++;
  }
  mShiftManager->TransferGeneralIS(mScope->FastMagIndex(), delISX, delISY, 
    comaVsIS->magInd, transISX, transISY);
  delBTX = comaVsIS->matrix.xpx * transISX + comaVsIS->matrix.xpy * transISY;
  delBTY = comaVsIS->matrix.ypx * transISX + comaVsIS->matrix.ypy * transISY;
  mScope->IncBeamTilt(delBTX, delBTY);
  mCompensatedBTforIS = true;
  return 0;
}

// Saves a control set in the stack if it has not been saved already
void CMacroProcessor::SaveControlSet(int index)
{
  for (int ind = 0; ind < (int)mConsetNums.size(); ind++)
    if (index == mConsetNums[ind])
      return;
  mConsetNums.push_back(index);
  mConsetsSaved.push_back(mConSets[index]);
}

// Restores a single control set given by index, or all control sets if index is < 0
// Erases the corresponding saved set if erase is true; otherwise (and if index is < 0)
// 
bool CMacroProcessor::RestoreCameraSet(int index, BOOL erase)
{
  bool retval = true;
  if (index < 0 && !erase) {
    mChangedConsets.clear();
    mCamWithChangedSets = mWinApp->GetCurrentCamera();
  }
  for (int ind = (int)mConsetNums.size() - 1; ind >= 0; ind--) {
    if (mConsetNums[ind] == index || index < 0) {
      if (index < 0 && !erase)
        mChangedConsets.push_back(mConSets[mConsetNums[ind]]);
      mConSets[mConsetNums[ind]] = mConsetsSaved[ind];
      if (erase) {
        mConsetsSaved.erase(mConsetsSaved.begin() + ind);
        mConsetNums.erase(mConsetNums.begin() + ind);
      }
      retval = false;
    }
  }
  return retval;
}

// Returns a navigator item specified by a positive index, a negative distance from end
// of table (-1 = last item), or 0 for current or acquire item.  Updates the value of
// index with the true 0-based value.  Issues message, aborts and returns NULL on failures
CMapDrawItem *CMacroProcessor::CurrentOrIndexedNavItem(int &index, CString &strLine)
{
  CMapDrawItem *item;
  if (!mWinApp->mNavigator) {
    ABORT_NORET_LINE("The Navigator must be open to execute line:\n\n");
    return NULL;
  }
  if (index > 0)
    index--;
  else if (index < 0)
    index = mWinApp->mNavigator->GetNumNavItems() + index;
  else {
    index = mWinApp->mNavigator->GetCurrentOrAcquireItem(item);
    if (index < 0) {
      ABORT_NORET_LINE("There is no current Navigator item for line:\n\n.");
      return NULL;
    }
  }
  item = mWinApp->mNavigator->GetOtherNavItem(index);
  if (!item) {
    ABORT_NORET_LINE("Index is out of range in statement:\n\n");
    return NULL;
  }
  return item;
}

// Computes number of montage pieces needed to achieve the minimum size in microns given
// the camera number of pixels in camSize and fractional overlap
int CMacroProcessor::PiecesForMinimumSize(float minMicrons, int camSize, 
  float fracOverlap)
{
  int binning = mConSets[RECORD_CONSET].binning;
  int magInd = mScope->GetMagIndex();
  float pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), magInd) * 
    binning;
  int minPixels = (int)(minMicrons / pixel);
  camSize /= binning;
  return (int)ceil(1. + (minPixels - camSize) / ((1. - fracOverlap) * camSize));
}

// Transfer macro if open and give error if macro is empty (1) or fails (2)
int CMacroProcessor::EnsureMacroRunnable(int macnum)
{
  if (mMacroEditer[macnum])
    mMacroEditer[macnum]->TransferMacro(true);
  if (mMacros[macnum].IsEmpty()) {
    SEMMessageBox("The script you selected to run is empty", MB_EXCLAME);
    return 1;
  } 
  mWinApp->mMacroProcessor->PrepareForMacroChecking(macnum);
  if (mWinApp->mMacroProcessor->CheckBlockNesting(macnum, -1))
    return 2;
 return 0;
}

// Send an email from a message box error if command defined the text
void CMacroProcessor::SendEmailIfNeeded(void)
{
  if (!mEmailOnError.IsEmpty())
    mWinApp->mMailer->SendMail(mMailSubject, mEmailOnError);
}

// Thread Procedure for running a process
UINT CMacroProcessor::RunInShellProc(LPVOID pParam)
{
  CString *strCopy = (CString *)pParam;
  std::string command = (LPCTSTR)(*strCopy);
  _flushall();
  sProcessErrno = 0;
  sProcessExitStatus = system(command.c_str());
  if (sProcessExitStatus == -1) {
    switch (errno) {
    case E2BIG: sProcessErrno = 1; break;
    case ENOENT: sProcessErrno = 2; break;
    case ENOEXEC: sProcessErrno = 3; break;
    case ENOMEM: sProcessErrno = 4; break;
    default: sProcessErrno = 5;
    }
    return 1;
  }
  return 0;
}

// Busy function for the task to start the Navigator Acquire
int CMacroProcessor::StartNavAvqBusy(void)
{
  if (DoingMacro())
    return 1;
  if (mLastCompleted && mWinApp->mNavigator) 
    mWinApp->mNavigator->AcquireAreas(false);
  return 0;
}

// Create a simple hash value with the djb2 alogorithm, mask bit before the multiply
unsigned int CMacroProcessor::StringHashValue(const char *str)
{
  unsigned int hash = 5381;
  int c;
  while ((c = (unsigned int)(*str++)) != 0)
    hash = (hash & 0x3FFFFFF) * 33 + c;
  return hash;
}

// Lookup a command or other candidate for command by first trying to find the hash value
// in the map, then falling back to matching the string
// The lookup has been tested with hash values masked to 0 - 63
int CMacroProcessor::LookupCommandIndex(CString & item)
{
  unsigned int hash = StringHashValue((LPCTSTR)item);
  int ind;
  std::map<unsigned int, int>::iterator mapit;

  if (!mCmdHashMap.count(hash))
    return CME_NOTFOUND;
  mapit = mCmdHashMap.find(hash);
  ind = mapit->second;

  // Return the map value as long as the string matches: we need to exclude the universe
  // of variable names that might collide with a command having unique hash
  if (ind && item == cmdList[ind].cmd.c_str())
    return ind;

  // Fallback to looking up command by string
  for (ind = 0; ind < NUM_COMMANDS - 1; ind++) {
    if (item == cmdList[ind].cmd.c_str())
      return ind;
  }
  return CME_NOTFOUND;
}

// Find a text file given the ID, performing a check on its existent specified by 
// checkType and returning its index in the array
FileForText *CMacroProcessor::LookupFileForText(CString &ID, int checkType, 
  CString &strLine, int &index)
{
  FileForText *tfile;
  CString errStr;
  for (index = 0; index < (int)mTextFileArray.GetSize(); index++) {
    tfile = mTextFileArray.GetAt(index);
    if (tfile->ID == ID) {
      if (checkType == TXFILE_MUST_NOT_EXIST)
        errStr = "There is already an open file with identifier " + ID;
      if (checkType == TXFILE_READ_ONLY && !tfile->readOnly)
        errStr = "The file with identifier " + ID + " cannot be read from";
      if (checkType == TXFILE_WRITE_ONLY && tfile->readOnly)
        errStr = "The file with identifier " + ID + " cannot be written to";
      if (!errStr.IsEmpty())
        break;
      return tfile;
    }
  }
  if (errStr.IsEmpty() && checkType != TXFILE_MUST_NOT_EXIST && 
    checkType != TXFILE_QUERY_ONLY)
      errStr = "There is no open file with identifier " + ID;
  if (!errStr.IsEmpty())
    ABORT_NORET_LINE(errStr + " for line:\n\n");
  return NULL;
}

// Close a text file given its index
void CMacroProcessor::CloseFileForText(int index)
{
  FileForText *txFile;
  int startInd = index, endInd = index, ind;
  if (index >= mTextFileArray.GetSize())
    return;
  if (index < 0) {
    startInd = 0;
    endInd = (int)mTextFileArray.GetSize() - 1;
  }
  for (ind = endInd; ind >= startInd; ind--) {
    txFile = mTextFileArray.GetAt(ind);
    if (index != -1 || !txFile->persistent) {
      txFile->csFile->Close();
      delete txFile->csFile;
      delete txFile;
      mTextFileArray.RemoveAt(ind);
    }
  }
}

// Convenience function to extract part of entered line after substituting variables
void CMacroProcessor::SubstituteLineStripItems(CString & strLine, int numStrip, 
  CString &strCopy)
{
  SubstituteVariables(&strLine, 1, strLine);
  mWinApp->mParamIO->StripItems(strLine, numStrip, strCopy);
}

// Convert letter after command to LD area # or abort if not legal.  Tests for whether
// Low dose is on or off if needOnOrOff >0 or <0.  Returns index -1 and no error if item
// is empty
int CMacroProcessor::CheckAndConvertLDAreaLetter(CString &item, int needOnOrOff,
  int &index, CString &strLine)
{
  if (needOnOrOff && !BOOL_EQUIV(mWinApp->LowDoseMode(), needOnOrOff > 0)) {
    ABORT_NORET_LINE("You must" + CString(needOnOrOff > 0 ? "" : " NOT") + 
      " be in low dose mode to use this command:\n\n");
    return 1;
  }
  if (item.IsEmpty()) {
    index = -1;
    return 0;
  }
  index = CString("VFTRS").Find(item.Left(1));
  if (index < 0)
    ABORT_NORET_LINE("Command must be followed by one of V, F, T, R, or S in line:\n\n");
  return index < 0 ? 1 : 0;
}

// Restore saved low dose parameters for the set given by index, or for all sets if index
// is -1, or for ones marked to be restored if index is < -1
void CMacroProcessor::RestoreLowDoseParams(int index)
{
  LowDoseParams *ldParam = mWinApp->GetLowDoseParams();
  LowDoseParams ldsaParams;
  int ind, set;
  int curArea = mWinApp->LowDoseMode() ? mScope->GetLowDoseArea() : -1;
  for (ind = 0; ind < (int)mLDareasSaved.size(); ind++) {
    set = mLDareasSaved[ind];
    if (set == index || index == -1 || (index < -1 && mKeepOrRestoreArea[set] > 0)) {

      // When going to the current area, save the current params in the local variable
      // and tell LD that is the current set of parameters to use, then set in new
      // parameters and go to the area to get them handled right
      if (set == curArea) {
        ldsaParams = ldParam[set];
        mScope->SetLdsaParams(&ldsaParams);
      }
      ldParam[set] = mLDParamsSaved[ind];
      if (set == curArea)
        mScope->GotoLowDoseArea(curArea);
      if (index >= 0) {
        mLDareasSaved.erase(mLDareasSaved.begin() + ind);
        mLDParamsSaved.erase(mLDParamsSaved.begin() + ind);
        return;
      }
    }
  }
  if (index < 0) {
    mLDareasSaved.clear();
    mLDParamsSaved.clear();
  }
}

// External call to find out if a low dose area is saved/free to modify
bool CMacroProcessor::IsLowDoseAreaSaved(int which)
{
  return mKeepOrRestoreArea[which] != 0;
}

// Call to update the current Low dose area if it was saved
void CMacroProcessor::UpdateLDAreaIfSaved()
{
  BOOL saveContinuous = mWinApp->mLowDoseDlg.GetContinuousUpdate();
  int curArea = mWinApp->LowDoseMode() ? mScope->GetLowDoseArea() : -1;
  if (curArea < 0 || !mKeepOrRestoreArea[curArea])
    return;
  if (!saveContinuous)
    mWinApp->mLowDoseDlg.SetContinuousUpdate(true);
  mScope->ScopeUpdate(GetTickCount());
  if (!saveContinuous)
    mWinApp->mLowDoseDlg.SetContinuousUpdate(false);
}

// Store string variable into an extra script
int CMacroProcessor::MakeNewTempMacro(CString &strVar, CString &strIndex, bool tempOnly,
  CString &strLine)
{
  int ix0, index, endInd, lastEnd, nextEnd, length;
  int lowLim = tempOnly ? 0 : -MAX_MACROS;
  Variable *var = LookupVariable(strVar.MakeUpper(), ix0);
  if (!var) {
    ABORT_NORET_LINE("The variable " + strVar + " is not defined in line:\n\n");
    return 0;
  }
  if (var->rowsFor2d) {
    ABORT_NORET_LINE("The variable " + strVar + " is a 2D array and cannot be run "
      "as a script in line:\n\n");
    return 0;
  }
  index = mNumTempMacros;
  if (!strIndex.IsEmpty()) {
    index = atoi((LPCTSTR)strIndex);
    if (!index || index > MAX_TEMP_MACROS || index < lowLim) {
      ABORT_NORET_LINE("The specified extra script number is out of range in line:\n\n");
      return 0;
    }
    if (index < 0) {
      index = -index - 1;
    } else {
      index--;
      if (index > mNumTempMacros) {
        ABORT_NORET_LINE("The script number must specify the number of an existing "
          "\r\nextra script or the next unused number in line:\n\n");
        return 1;
      }
      mNumTempMacros = B3DMAX(mNumTempMacros, index + 1);
      index += MAX_MACROS + MAX_ONE_LINE_SCRIPTS;
    }
  } else {
    if (mNumTempMacros == MAX_TEMP_MACROS) {
      ABORT_NORET_LINE("Too many extra scripts are defined to use another in line:\n\n");
      return 0;
    }
    mNumTempMacros++;
    index += MAX_MACROS + MAX_ONE_LINE_SCRIPTS;
  }
  mMacros[index] = "";
  lastEnd = 0;
  length = var->value.GetLength();
  for (;;) {
    endInd = var->value.Find("\n", lastEnd);
    if (endInd < 0)
      endInd = length;
    nextEnd = endInd + 1;
    if (endInd > 0 && var->value.GetAt(endInd - 1) == '\r')
      endInd--;
    mMacros[index] += var->value.Mid(lastEnd, endInd - lastEnd) + "\r\n";
    lastEnd = nextEnd;
    if (lastEnd >= length)
      break;
  }

  ScanForName(index, &mMacros[index]);
  return index;
}

// Test whether a stage restore is specified in the first passed item, and use the current
// stage position if the next passed items do not have one
bool CMacroProcessor::SetupStageRestoreAfterTilt(CString *strItems, double &stageX, 
  double &stageY)
{
  double stageZ;
  stageX = stageY = EXTRA_NO_VALUE;
  if (strItems[0].IsEmpty() || !atoi((LPCTSTR)strItems[0])) {
    return false;
  } else if (strItems[1].IsEmpty() || strItems[2].IsEmpty()) {
    mScope->GetStagePosition(stageX, stageY, stageZ);
  } else {
    stageX = atof((LPCTSTR)strItems[1]);
    stageY = atof((LPCTSTR)strItems[2]);
  }
  return true;
}
