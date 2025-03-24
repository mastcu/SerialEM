// ParameterIO.cpp:       Reads property file, reads and writes settings and
//                          calibration files
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include ".\ParameterIO.h"
#include "SerialEMDoc.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "ShiftCalibrator.h"
#include "FocusManager.h"
#include "MacroEditer.h"
#include "BeamAssessor.h"
#include "TSController.h"
#include "ComplexTasks.h"
#include "DistortionTasks.h"
#include "CameraController.h"
#include "CalibCameraTiming.h"
#include "ProcessImage.h"
#include "MacroProcessor.h"
#include "GainRefMaker.h"
#include "LogWindow.h"
#include "ParticleTasks.h"
#include "FilterTasks.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "FalconHelper.h"
#include "EMmontageController.h"
#include "MultiTSTasks.h"
#include "MultiGridTasks.h"
#include "HoleFinderDlg.h"
#include "AutoContouringDlg.h"
#include "MultiGridDlg.h"
#include "Mailer.h"
#include "MultiShotDlg.h"
#include "MultiCombinerDlg.h"
#include "CtffindParamDlg.h"
#include "AutocenSetupDlg.h"
#include "VPPConditionSetup.h"
#include "CtffindParamDlg.h"
#include "ZbyGSetupDlg.h"
#include "ScreenShotDialog.h"
#include "StateDlg.h"
#include "ReadFileDlg.h"
#include "StageMoveTool.h"
#include "OneLineScript.h"
#include "MacroToolbar.h"
#include "ScreenMeter.h"
#include "NavRotAlignDlg.h"
#include "DoseMeter.h"
#include "BaseSocket.h"
#include "AutoTuning.h"
#include "ExternalTools.h"
#include "TSVariationsDlg.h"
#include "PiezoAndPPControl.h"
#include "Shared\b3dutil.h"
#include "Utilities\KGetOne.h"
#include <set>
#include <string>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define MAX_TOKENS 40
#define FILTER_LIFETIME_MINUTES  60
#define IS_STAGE_LIFETIME_HOURS 22
#define PIXEL_LIFETIME_HOURS 24

// 5/27/06: Properties read in under 5 ms even with this test
#define MatchNoCase(a) (strItems[0].CompareNoCase(a) == 0)
#define NAME_IS(a) (strItems[0] == (a))

#include "DisableHideTable.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParameterIO::CParameterIO()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mShiftManager = mWinApp->mShiftManager;
  mDocWnd = mWinApp->mDocWnd;
  mModeNames = mWinApp->GetModeNames();
  mConSets = mWinApp->GetCamConSets();
  mMagTab = mWinApp->GetMagTable();
  mCamParam = mWinApp->GetCamParams();

  mBufferManager = mWinApp->mBufferManager;
  mFocTab = mWinApp->mFocusManager->GetFocusTable();
  mLowDoseParams = mWinApp->GetCamLowDoseParams();
  mTSParam = mWinApp->mTSController->GetTiltSeriesParam();
  mFilterParam = mWinApp->GetFilterParams();
  mNumLDSets = 5;
  mMaxReadInMacros = 20;
  mCheckForComments = false;
}

CParameterIO::~CParameterIO()
{

}

// Macros for setting variables from the settings values
#define INT_SETT_GETSET(a, b, c) \
  else if (strItems[0] == a) \
    b##Set##c(itemInt[1]);
#define BOOL_SETT_GETSET(a, b, c) \
  else if (strItems[0] == a) \
    b##Set##c(itemInt[1] != 0);
#define FLOAT_SETT_GETSET(a, b, c) \
  else if (strItems[0] == a)   \
    b##Set##c((float)itemDbl[1]);
#define DOUBLE_SETT_GETSET(a, b, c) \
  else if (strItems[0] == a)   \
    b##Set##c(itemDbl[1]);
#define INT_SETT_ASSIGN(a, b) \
  else if (strItems[0] == a) \
    b = itemInt[1];
#define BOOL_SETT_ASSIGN(a, b) \
  else if (strItems[0] == a) \
    b = itemInt[1] != 0;
#define FLOAT_SETT_ASSIGN(a, b) \
  else if (strItems[0] == a) \
    b = (float)itemDbl[1];

#define SET_PLACEMENT(tag, win) \
  if (NAME_IS(tag) && win != NULL && place->rcNormalPosition.right != NO_PLACEMENT) \
    win->SetWindowPlacement(place);

// BEWARE: FOR ANYTHING THAT CAN BE ON MULTIPLE LINES AND IS JUST ADDED TO A VECTOR,
// THE VECTOR MUST BE CLEARED FIRST

int CParameterIO::ReadSettings(CString strFileName, bool readingSys)
{
  int retval = 0;
  int err;
  int index, mag, spot, GIF;
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  ControlSet *cs;
  CameraParameters *camParams = mWinApp->GetCamParams();
  CString strLine, strCopy, unrecognized = "";
  CString strItems[MAX_TOKENS];
  CString message;
  CString *stateNames;
  int *stateNums;
  char absPath[_MAX_PATH];
  char *fullp;
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  float itemFlt[MAX_TOKENS];
  int nLines = 0;
  FileOptions *defFileOpt = mDocWnd->GetDefFileOpt();
  FileOptions *otherFileOpt = mDocWnd->GetOtherFileOpt();
  int *initialDlgState = mWinApp->GetInitialDlgState();
  int *macroButtonNumbers = mWinApp->mCameraMacroTools.GetMacroNumbers();
  float pctLo, pctHi;
  float *gridLim = mWinApp->mNavHelper->GetGridLimits();
  WINDOWPLACEMENT winPlace;
  WINDOWPLACEMENT *place;
  RECT *dlgPlacements = mWinApp->GetDlgPlacements();
  NavParams *navParams = mWinApp->GetNavParams();
  NavAcqParams *naqParams;
  CookParams *cookParams = mWinApp->GetCookParams();
  AutocenParams acParams;
  AutocenParams *acParmP;
  RangeFinderParams *tsrParams = mWinApp->GetTSRangeParams();
  int *tssPanelStates = mWinApp->GetTssPanelStates();

  // When you split a section, you need to add "recognized" code here and in the two macro
  // get/set functions, and add another include in WriteSettings
  BOOL recognized, recognized15, recognized2, recognized25, recognized3, frameListOK;
  LowDoseParams *ldp;
  StateParams *stateP;
  CArray<StateParams *, StateParams *> *stateArray = mWinApp->mNavHelper->GetStateArray();
  int *deNumRepeats = mWinApp->mGainRefMaker->GetDEnumRepeats();
  float *deExposures = mWinApp->mGainRefMaker->GetDEexposureTimes();
  CArray<FrameAliParams, FrameAliParams> *faParamArray =
    mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams faParam, *faData;
  BOOL *useGPU4K2Ali = mWinApp->mCamera->GetUseGPUforK2Align();
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  HoleFinderParams *hfParams = mWinApp->mNavHelper->GetHoleFinderParams();
  AutoContourParams *contParams = mWinApp->mNavHelper->GetAutocontourParams();
  DriftWaitParams *dwParams = mWinApp->mParticleTasks->GetDriftWaitParams();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  VppConditionParams *vppParams = mWinApp->mMultiTSTasks->GetVppConditionParams();
  ScreenShotParams *snapParams = mWinApp->GetScreenShotParams();
  CArray<ZbyGParams> *zbgArray = mWinApp->mParticleTasks->GetZbyGcalArray();
  ZbyGParams zbgParam;
  NavAcqAction *navActions;
  CString *fKeyMapping = mWinApp->mMacroProcessor->GetFKeyMapping();
  NavAlignParams *navAliParm = mWinApp->mNavHelper->GetNavAlignParams();
  DewarVacParams *dewar = mWinApp->mScope->GetDewarVacParams();
  CArray<BaseMarkerShift, BaseMarkerShift> *markerShiftArr =
    mWinApp->mNavHelper->GetMarkerShiftArray();
  BaseMarkerShift markerShift;
  MultiGridParams *mgParams = mWinApp->mMultiGridTasks->GetMultiGridParams();
  CFileStatus status;
  BOOL startingProg = mWinApp->GetStartingProgram();
  int faLastFileIndex = -1, faLastArrayIndex = -1;

  // Clear all arrays being added to
  mWinApp->mMultiTSTasks->ClearAutocenParams();
  mWinApp->mNavHelper->ClearStateArray();
  zbgArray->RemoveAll();
  markerShiftArr->RemoveAll();

  mWinApp->mCamera->SetFrameAliDefaults(faParam, "4K default set", 4, 0.06f, 1);
  mWinApp->SetAbsoluteDlgIndex(false);
  msParams->customHoleX.clear();
  msParams->customHoleY.clear();
  for (index = 0; index <= MAX_MACROS; index++)
    mWinApp->SetReopenMacroEditor(index, false);
  mDocWnd->SetCurScriptPackPath("");
  mWinApp->ClearAllMacros();
  mWinApp->SetSettingsFixedForIACal(false);
  for (index = 0; index < 4; index++) {
    mgParams->MMMstateNums[index] = -1;
    mgParams->MMMstateNames[index] = "";
    mgParams->finalStateNums[index] = -1;
    mgParams->finalStateNames[index] = "";
  }

  try {
    // Open the file for reading, verify that it is a settings file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                         MAX_TOKENS);
    if (err)
      retval = 1;
    else if (CheckForByteOrderMark(strItems[0], "SerialEMSettings", strFileName, 
      "settings")) {
        retval = 1;
    }

    // Clear out low dose params now in case there are none in the file
    mWinApp->InitializeLDParams();

    while (retval == 0 && (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt,
                                                itemDbl, itemFlt,MAX_TOKENS))
           == 0) {
      recognized = true;
      recognized15 = true;
      recognized2 = true;
      recognized25 = true;
      recognized3 = true;
      if (NAME_IS("SystemPath")) {

        // There could be multiple words - have to assume 
        // separated by spaces
        CString path = strItems[1];
        index = 2;
        while (!strItems[index].IsEmpty() && index < MAX_TOKENS)
          path += " " + strItems[index++];
        mDocWnd->SetSystemPath(path);

      } else if (NAME_IS("CameraControlSet")) {
        int iset = itemInt[1];
        int iCam = 0;
        if (!strItems[2].IsEmpty())
          iCam = itemInt[2];
        cs = &mConSets[iset + iCam * MAX_CONSETS];
        frameListOK = true;
        cs->summedFrameList.clear();
        cs->userFrameFractions.clear();
        cs->userSubframeFractions.clear();
        while ((err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl,
                                     itemFlt, MAX_TOKENS)) == 0) {
          if (NAME_IS("EndControlSet"))
            break;
          if (NAME_IS("ControlSetName"))
            err = 0;
          else if (NAME_IS("AcquisitionMode"))
            cs->mode = itemInt[1];
          else if (NAME_IS("Processing"))
            cs->processing = itemInt[1];
          else if (NAME_IS("DarkReferenceEveryTime"))
            cs->forceDark = itemInt[1];
          else if (NAME_IS("AverageDarkReference"))
            cs->averageDark = itemInt[1];
          else if (NAME_IS("TimesToAverageDarkRef"))
            cs->numAverage = itemInt[1];
          else if (NAME_IS("RemoveXrays"))
            cs->removeXrays = itemInt[1];
          else if (NAME_IS("Binning"))
            cs->binning = itemInt[1];
          else if (NAME_IS("ShutteringMode"))
            cs->shuttering = itemInt[1];
          else if (NAME_IS("Top"))
            cs->top = itemInt[1];
          else if (NAME_IS("Left"))
            cs->left = itemInt[1];
          else if (NAME_IS("Right"))
            cs->right = itemInt[1];
          else if (NAME_IS("Bottom"))
            cs->bottom = itemInt[1];
          else if (NAME_IS("ExposureTime"))
            cs->exposure = itemFlt[1];
          else if (NAME_IS("DriftSettling"))
            cs->drift = itemFlt[1];
          else if (NAME_IS("LineSync"))
            cs->lineSync = itemInt[1];
          else if (NAME_IS("DynamicFocus"))
            cs->dynamicFocus = itemInt[1];
          else if (NAME_IS("CorrectDrift"))
            cs->correctDrift = itemInt[1];
          else if (NAME_IS("K2ReadMode"))
            cs->K2ReadMode = itemInt[1];
          else if (NAME_IS("DoseFracMode"))
            cs->doseFrac = itemInt[1];
          else if (NAME_IS("FrameTime"))
            cs->frameTime = itemFlt[1];
          else if (NAME_IS("AlignFrames"))
            cs->alignFrames = itemInt[1];
          else if (NAME_IS("UseFrameAlign"))
            cs->useFrameAlign = itemInt[1];
          else if (NAME_IS("FAParamSetInd"))
            cs->faParamSetInd = itemInt[1];
          else if (NAME_IS("SaveFrames"))
            cs->saveFrames = itemInt[1];
          else if (NAME_IS("SumK2Frames"))
            cs->sumK2Frames = itemInt[1];
          else if (NAME_IS("DEsumCount"))
            cs->DEsumCount = itemInt[1];
          else if (NAME_IS("SkipFramesBefore"))
            cs->numSkipBefore = (itemInt[1] < 0 || itemInt[1] > 10) ? 0 : itemInt[1];
          else if (NAME_IS("SkipFramesAfter"))
            cs->numSkipAfter = (itemInt[1] < 0 || itemInt[1] > 10) ? 0 : itemInt[1];
          else if (NAME_IS("ReadoutsPerFrame")) {

            // Backward-compatibility: read old one entry per frame
            spot = itemInt[1];
            mag = 0;
            for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++) {
              if (spot == itemInt[index])
                mag++;
              else {
                cs->summedFrameList.push_back(mag);
                cs->summedFrameList.push_back(spot);
                spot = itemInt[index];
                mag = 1;
              }
            }
            if (mag) {
              cs->summedFrameList.push_back(mag);
              cs->summedFrameList.push_back(spot);
            }
          } else if (NAME_IS("SummedFrameList")) {
            if (frameListOK)
              for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
                cs->summedFrameList.push_back(itemInt[index]);
            if (cs->summedFrameList.size() > 200) {
              frameListOK = false;
              message.Format("Excessively long SummedFrameList entry found for camera %d,"
                " parameter set %d\nThese frame summing parameters are being removed - "
                "this may take alarmingly long after you close this message.\n\n"
                " You will need to set up frame summing parameters again.", iCam, iset);
              AfxMessageBox(message, MB_EXCLAME);
              cs->summedFrameList.clear();
              cs->userFrameFractions.clear();
              cs->userSubframeFractions.clear();
            }
          } else if (NAME_IS("UserFrameFracs")) {
            if (frameListOK)
              for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
                cs->userFrameFractions.push_back(itemFlt[index]);
          } else if (NAME_IS("UserSubframeFracs")) {
            if (frameListOK)
              for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
                cs->userSubframeFractions.push_back(itemFlt[index]);
          } else if (NAME_IS("FilterType"))
            cs->filterType = itemInt[1];
          else if (NAME_IS("ChannelIndex")) {
            for (index = 0; index < MAX_STEM_CHANNELS; index++)
              if (!strItems[index+1].IsEmpty())
                cs->channelIndex[index] = itemInt[index+1];
          } else if (NAME_IS("BoostMag")) {
            cs->boostMag = itemInt[1];
            cs->magAllShots = itemInt[2];
          } else {
            // Unrecognized camera parameter      
            message.Format("For camera parameter set %d: %s\n", iset, strLine);
            unrecognized += message;
          }

        }
        if (err > 0) {
          retval = err;
          break;
        }

      } else if (NAME_IS("Macro")) {
        err = ReadOneMacro(itemInt[1], strLine, strItems,
          MAX_MACROS + MAX_ONE_LINE_SCRIPTS);
        if (err > 0) {
          retval = err;
          break;
        }
      } else if (NAME_IS("ScriptPackagePath")) {
        StripItems(strLine, 1, strCopy);
        fullp = _fullpath(absPath, (LPCTSTR)strCopy, _MAX_PATH);
        if (fullp)
          strCopy = fullp;
        mDocWnd->SetCurScriptPackPath(strCopy);
      } else if (NAME_IS("CurrentDirectory")) {
        StripItems(strLine, 1, strCopy);
        mDocWnd->SetCurrentDirReadIn(strCopy);
      } else if (NAME_IS("ScriptToRunAtStart")) {
        StripItems(strLine, 1, strCopy);
        mWinApp->SetScriptToRunAtStart(strCopy);
      } else if (NAME_IS("ScriptToRunAtEnd")) {
        StripItems(strLine, 1, strCopy);
        mWinApp->SetScriptToRunAtEnd(strCopy);
      } else if (NAME_IS("ScriptToRunOnIdle")) {
        StripItems(strLine, 1, strCopy);
        mWinApp->SetScriptToRunOnIdle(strCopy);
      } else if (NAME_IS("FKeyMapping")) {
        if (itemInt[1] >= 0 && itemInt[1] < 10)
          StripItems(strLine, 2, fKeyMapping[itemInt[1]]);
      } else if (MatchNoCase("BasicModeFile")) {
        StripItems(strLine, 1, strCopy);
        mDocWnd->SetBasicModeFile(strCopy);
      } else if (NAME_IS("LastDPI")) {
        mWinApp->SetLastSystemDPI(itemInt[1]);
      } else if (NAME_IS("FrameNameData")) {
        camera->SetFrameNameFormat(itemInt[1]);
        camera->SetFrameNumberStart(itemInt[2]);
        camera->SetLastFrameNumberStart(itemInt[3]);
        camera->SetLastUsedFrameNumber(itemInt[4]);
        if (!itemEmpty[5])
          camera->SetDigitsForNumberedFrame(itemInt[5]);
      } else if (NAME_IS("DirForK2Frames")) {
        StripItems(strLine, 1, strCopy);
        camera->SetDirForK2Frames(strCopy);
      } else if (NAME_IS("DirForDEFrames")) {
        StripItems(strLine, 1, strCopy);
        camera->SetDirForDEFrames(strCopy);
      } else if (NAME_IS("DirForFalconFrames")) {
        StripItems(strLine, 1, strCopy);
        if (strCopy == "EmptyPath")
          strCopy = "";
        camera->SetDirForFalconFrames(strCopy);
      } else if (NAME_IS("DirForFrameSaving")) {
        index = itemInt[1];
        B3DCLAMP(index, 0, MAX_CAMERAS - 1);
        StripItems(strLine, 2, camParams[index].dirForFrameSaving);
      } else if (NAME_IS("UseGPUforFrameAlign")) {
        index = itemInt[1];
        B3DCLAMP(index, 0, MAX_CAMERAS - 1);
        camParams[index].useGPUforAlign[0] = itemInt[2] != 0;
        camParams[index].useGPUforAlign[1] = itemInt[3] != 0;
      } else if (NAME_IS("FrameBaseName")) {
        StripItems(strLine, 1, strCopy);
        camera->SetFrameBaseName(strCopy);
      } else if (NAME_IS("NumberedFramePrefix")) {
        StripItems(strLine, 1, strCopy);
        camera->SetNumberedFramePrefix(strCopy);
      } else if (NAME_IS("NumberedFrameFolder")) {
        StripItems(strLine, 1, strCopy);
        camera->SetNumberedFrameFolder(strCopy);
      } else if (NAME_IS("DE12FPS")) {
        if (!itemEmpty[2] && itemInt[1] >= 0 && itemInt[1] < MAX_CAMERAS) {
          mCamParam[itemInt[1]].DE_FramesPerSec = itemFlt[2];
          if (!itemEmpty[3])
            mCamParam[itemInt[1]].DE_CountingFPS = itemFlt[3];
        }
      } else if (NAME_IS("DEAutosaveFormat")) {
        mWinApp->mDEToolDlg.SetFormatForAutoSave(itemInt[1]);
      } else if (NAME_IS("PercentDisplayTruncationLo")) {
        mWinApp->GetDisplayTruncation(pctLo, pctHi);
        pctLo = itemFlt[1];
        mWinApp->SetDisplayTruncation(pctLo, pctHi);
      } else if (NAME_IS("PercentDisplayTruncationHi")) {
        mWinApp->GetDisplayTruncation(pctLo, pctHi);
        pctHi = itemFlt[1];
        mWinApp->SetDisplayTruncation(pctLo, pctHi);
      } else if (NAME_IS("CopyToBufferOnSave")) {
      } else if (NAME_IS("ProtectRecordImages")) {
        mBufferManager->SetConfirmBeforeDestroy(3, itemInt[1]);
      } else if (NAME_IS("AlignOnSave")) {
      } else if (NAME_IS("MontageUseContinuous")) {
        montParam->useContinuousMode = itemInt[1] != 0;
        montParam->continDelayFactor = itemFlt[2];
      } else if (NAME_IS("MontageNoDriftCorr")) {
        montParam->noDriftCorr = itemInt[1] != 0;
        montParam->noHQDriftCorr = itemInt[2] != 0;
      } else if (NAME_IS("MontageFilterOptions")) {
        mWinApp->mMontageController->SetUseFilterSet2(itemInt[1] != 0);
        mWinApp->mMontageController->SetUseSet2InLD(itemInt[2] != 0);
        mWinApp->mMontageController->SetDivFilterSet1By2(itemInt[3] != 0);
        mWinApp->mMontageController->SetDivFilterSet2By2(itemInt[4] != 0);
      } else if (NAME_IS("TIFFcompression"))
        defFileOpt->compression = itemInt[1];
      else if (NAME_IS("HDFcompression"))
        defFileOpt->hdfCompression = itemInt[1];
      else if (MatchNoCase("FileOptionsPctTruncLo"))
        defFileOpt->pctTruncLo = itemFlt[1];
      else if (MatchNoCase("FileOptionsPctTruncHi"))
        defFileOpt->pctTruncHi = itemFlt[1];
      else if (MatchNoCase("AutoPruneLogLines")) {
        if (itemInt[1] && itemInt[1] != 500 &&
          itemInt[1] < mWinApp->GetAutoPruneLogLines()) {
          strCopy.Format("The number of lines for log pruning has been changed to "
            "the new default of %d", mWinApp->GetAutoPruneLogLines());
          AfxMessageBox(strCopy, MB_OK | MB_ICONINFORMATION);
        }
      } else if (NAME_IS("AutofocusEucenAbsParams")) {
        mWinApp->mFocusManager->SetEucenAbsFocusParams(itemDbl[1], itemDbl[2], 
        itemFlt[3], itemFlt[4], itemInt[5] != 0, itemInt[6] != 0);
      } else if (NAME_IS("AssessMultiplePeaksInAlign") || NAME_IS("AutoZoom")) {
      } 
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
      else
        recognized = false;
    
      if (recognized) {
      
      } 
#define SET_TEST_SECT15
#include "SettingsTests.h"
#undef SET_TEST_SECT15
      else
        recognized15 = false;

      if (recognized || recognized15) {
        recognized = true;
      }
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
        
      else if (NAME_IS("AutosaveNavigator")) {
        if (!itemInt[1])
          AfxMessageBox("It is now the default to autosave a Navigator file\n"
          "periodically.  If you do not want Navigator files to be\n"
          "saved automatically, turn this option off with the\n"
          "Navigator - Autosave Nav File menu item.", MB_OK | MB_ICONINFORMATION);
      } else if (NAME_IS("AutoBacklashNewMap")) {
        mWinApp->mNavHelper->SetAutoBacklashNewMap(itemInt[1]);
        mWinApp->mNavHelper->SetAutoBacklashMinField(itemFlt[2]);
      } else if (NAME_IS("NavCollapseGroups")) {
        mWinApp->mNavHelper->SetCollapseGroups(itemInt[1] != 0);
      } else if (NAME_IS("MultiShotParams")) {
        msParams->beamDiam = itemFlt[1];
        msParams->spokeRad[0] = itemFlt[2]; 
        msParams->numShots[0] =  itemInt[3];
        msParams->doCenter =  itemInt[4];
        msParams->doEarlyReturn =  itemInt[5];
        msParams->numEarlyFrames =  itemInt[6];
        msParams->saveRecord = itemInt[7] != 0;
        msParams->extraDelay = itemFlt[8];
        msParams->useIllumArea = itemInt[9] != 0;
        if (!itemEmpty[10]) {
          msParams->adjustBeamTilt = itemInt[10] != 0;
          msParams->inHoleOrMultiHole = itemInt[11];
          msParams->useCustomHoles = itemInt[12] != 0;
          msParams->holeDelayFactor = itemFlt[13];
          msParams->holeISXspacing[0] = itemDbl[14];
          msParams->holeISYspacing[0] = itemDbl[15];
          msParams->holeISXspacing[1] = itemDbl[16];
          msParams->holeISYspacing[1] = itemDbl[17];
          msParams->numHoles[0] = itemInt[18];
          msParams->numHoles[1] = itemInt[19];
          msParams->holeMagIndex[0] = itemInt[20];
          msParams->customMagIndex = itemInt[21];
        }
        if (!itemEmpty[22]) {
          msParams->skipCornersOf3x3 = itemInt[22] != 0;
        }
        if (!itemEmpty[28]) {
          msParams->doSecondRing = itemInt[23] != 0;
          msParams->numShots[1] = itemInt[24];
          msParams->spokeRad[1] = itemFlt[25];
          msParams->tiltOfHoleArray[0] = itemFlt[26];
          msParams->tiltOfCustomHoles = itemFlt[27];
          msParams->holeFinderAngle = itemFlt[28];
        }
 
      } else if (NAME_IS("MultiHexParams")) {
        msParams->doHexArray = itemInt[1] != 0;
        msParams->numHexRings = itemInt[2];
        for (index = 0; index < 3; index++) {
          msParams->hexISXspacing[index] = itemDbl[2 * index + 3];
          msParams->hexISYspacing[index] = itemDbl[2 * index + 4];
        }
        if (!itemEmpty[10]) {
          msParams->holeMagIndex[1] = itemInt[9];
          msParams->tiltOfHoleArray[1] = itemFlt[10];
        } else {
          msParams->holeMagIndex[1] = msParams->holeMagIndex[0];
          msParams->tiltOfHoleArray[1] = msParams->tiltOfHoleArray[0];
        }
      } else if (NAME_IS("StepAdjustParams")) {
        msParams->stepAdjLDarea = itemInt[1];
        msParams->stepAdjWhichMag = itemInt[2];
        msParams->stepAdjOtherMag = itemInt[3];
        msParams->stepAdjSetDefOff = itemInt[4] != 0;
        msParams->stepAdjDefOffset = itemInt[5];
        if (!itemEmpty[6])
          msParams->stepAdjTakeImage = itemInt[6] != 0;
        if (!itemEmpty[12]) {
          msParams->doAutoAdjustment = itemInt[7] != 0;
          msParams->autoAdjMethod = itemInt[8] ? 1 : 0;
          msParams->autoAdjHoleSize = itemFlt[9];
          msParams->autoAdjLimitFrac = itemFlt[10];
          msParams->stepAdjSetPrevExp = itemInt[11] != 0;
          msParams->stepAdjPrevExp = itemFlt[12];
        }

      } else if (NAME_IS("HoleAdjustXform")) {
        msParams->origMagOfArray[0] = itemInt[1];
        msParams->origMagOfArray[1] = itemInt[2];
        msParams->origMagOfCustom = itemInt[3];
        msParams->xformFromMag = itemInt[4];
        msParams->xformToMag = itemInt[5];
        msParams->adjustingXform.xpx = itemFlt[6];
        msParams->adjustingXform.xpy = itemFlt[7];
        msParams->adjustingXform.ypx = itemFlt[8];
        msParams->adjustingXform.ypy = itemFlt[9];
        if (!itemEmpty[10])
          msParams->xformMinuteTime = itemInt[10];
      } else if (NAME_IS("CustomHoleX")) {
        for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
          msParams->customHoleX.push_back(itemFlt[index]);
      } else if (NAME_IS("CustomHoleY")) {
        for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
          msParams->customHoleY.push_back(itemFlt[index]);
      } else if (NAME_IS("HoleFinderParams")) {
        hfParams->spacing = itemFlt[1];
        hfParams->diameter = itemFlt[2];
        hfParams->useBoundary = itemInt[3] != 0;
        hfParams->lowerMeanCutoff = itemFlt[4];
        hfParams->upperMeanCutoff = itemFlt[5];
        hfParams->SDcutoff = itemFlt[6];
        hfParams->blackFracCutoff = itemFlt[7];
        hfParams->showExcluded = itemInt[8] != 0;
        hfParams->layoutType = itemInt[9];
        hfParams->bracketLast = itemInt[10] != 0;
        if (!itemEmpty[13]) {
          hfParams->hexagonalArray = itemInt[11] != 0;
          hfParams->hexSpacing = itemFlt[12];
          hfParams->hexDiameter = itemFlt[13];
        }
        if (!itemEmpty[14])
          hfParams->edgeDistCutoff = itemFlt[14];
        if (!itemEmpty[15])
          hfParams->useHexDiagonals = itemInt[15] != 0;
      } else if (NAME_IS("HoleFiltSigmas")) {
        hfParams->sigmas.clear();
        for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
          hfParams->sigmas.push_back(itemFlt[index]);
      } else if (NAME_IS("HoleEdgeThresholds")) {
        hfParams->thresholds.clear();
        for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
          hfParams->thresholds.push_back(itemFlt[index]);

      } else if (NAME_IS("HoleCombinerParams")) {
        mWinApp->mNavHelper->SetMHCcombineType(itemInt[1]);
        mWinApp->mNavHelper->SetMHCenableMultiDisplay(itemInt[2] != 0);
        if (!itemEmpty[3])
          mWinApp->mNavHelper->SetMHCturnOffOutsidePoly(itemInt[3] != 0);
        if (!itemEmpty[5]) {
          mWinApp->mNavHelper->SetMHCdelOrTurnOffIfFew(itemInt[4]);
          mWinApp->mNavHelper->SetMHCthreshNumHoles(itemInt[5]);
        }
        if (!itemEmpty[6])
          mWinApp->mNavHelper->SetMHCskipAveragingPos(itemInt[6] != 0);
      } else if (NAME_IS("AutoContParams")) {
        contParams->targetSizePixels = itemInt[1];
        contParams->targetPixSizeUm = itemFlt[2];
        contParams->usePixSize = itemInt[3] != 0;
        contParams->minSize = itemFlt[4];
        contParams->maxSize = itemFlt[5];
        contParams->relThreshold = itemFlt[6];
        contParams->absThreshold = itemFlt[7];
        contParams->useAbsThresh = itemInt[8] != 0;
        contParams->numGroups = itemInt[9];
        contParams->groupByMean = itemInt[10] != 0;
        contParams->lowerMeanCutoff = itemFlt[11];
        contParams->upperMeanCutoff = itemFlt[12];
        contParams->minSizeCutoff = itemFlt[13];
        contParams->SDcutoff = itemFlt[14];
        contParams->irregularCutoff = itemFlt[15];
        contParams->borderDistCutoff = itemFlt[16];
        if (!itemEmpty[17])
          contParams->useCurrentPolygon = itemInt[17] != 0;
      } else if (NAME_IS("DriftWaitParams")) {
        dwParams->measureType = B3DMAX(0, B3DMIN(2, itemInt[1]));
        dwParams->driftRate = itemFlt[2];
        dwParams->useAngstroms = itemInt[3] != 0;
        dwParams->interval = itemFlt[4];
        dwParams->maxWaitTime = itemFlt[5];
        dwParams->failureAction = itemInt[6];
        dwParams->setTrialParams = itemInt[7] != 0;
        dwParams->exposure = itemFlt[8];
        dwParams->binning = itemInt[9];
        dwParams->changeIS = itemInt[10] != 0;
        if (!itemEmpty[11]) {
          dwParams->usePriorAutofocus = itemInt[11] != 0;
          dwParams->priorAutofocusRate = itemFlt[12];
        }

      } else if (NAME_IS("NavigatorAcquireParams")) {

        // For backward compatibility: set options into new structures as needed
        for (index = 0; index < 2; index++) {
          navActions = mWinApp->mNavHelper->GetAcqActions(index);
          naqParams = mWinApp->GetNavAcqParams(index);
          SET_ACTION(navActions, NAACT_AUTOFOCUS, itemInt[1]);
          SET_ACTION(navActions, NAACT_FINE_EUCEN, itemInt[2]);
          SET_ACTION(navActions, NAACT_REALIGN_ITEM, itemInt[3]);
          naqParams->restoreOnRealign = itemInt[4] != 0;
          SET_ACTION(navActions, NAACT_ROUGH_EUCEN, itemInt[5]);
          naqParams->nonTSacquireType = itemInt[6];
          naqParams->macroIndex = itemInt[7];
          if (!strItems[8].IsEmpty())
            naqParams->closeValves = itemInt[8] != 0;
          if (!strItems[9].IsEmpty())
            naqParams->sendEmail = itemInt[9] != 0;
          if (!strItems[11].IsEmpty()) {
            naqParams->preMacroInd = itemInt[11];
            naqParams->runPremacro = itemInt[12] != 0;
            naqParams->preMacroIndNonTS = itemInt[13];
            naqParams->runPremacroNonTS = itemInt[14] != 0;
            naqParams->sendEmailNonTS = itemInt[15] != 0;
          }
          if (!strItems[16].IsEmpty()) {
            naqParams->skipInitialMove = itemInt[16] != 0;
            naqParams->skipZmoves = itemInt[17] != 0;
          }
          if (!strItems[18].IsEmpty()) {
            naqParams->postMacroInd = itemInt[18];
            naqParams->runPostmacro = itemInt[19] != 0;
            naqParams->postMacroIndNonTS = itemInt[20];
            naqParams->runPostmacroNonTS = itemInt[21] != 0;
          }
        }

      } else if (NAME_IS("NavAcquireParams")) {
        index = itemInt[1];
        B3DCLAMP(index, 0, 1);
        ReadNavAcqParams(mWinApp->GetNavAcqParams(index), 
          mWinApp->mNavHelper->GetAcqActions(index), 
          mWinApp->mNavHelper->GetAcqActCurrentOrder(index), unrecognized);

      } else if (NAME_IS("NavAlignParams")) {
        navAliParm->loadAndKeepBuf = itemInt[1];
        navAliParm->maxAlignShift = itemFlt[2];
        navAliParm->maxNumResetIS = itemInt[3];
        navAliParm->resetISthresh = itemFlt[4];
        navAliParm->leaveISatZero = itemInt[5] != 0;
        if (!itemEmpty[9]) {
          navAliParm->scaledAliMaxRot = itemFlt[6];
          navAliParm->scaledAliPctChg = itemFlt[7];
          navAliParm->scaledAliExtraFOV = itemFlt[8];
          navAliParm->scaledAliLoadBuf = itemInt[9];
        }
      }
      else if (NAME_IS("NavAliMapLabel")) {
        StripItems(strLine, 1, navAliParm->templateLabel);

      } else if (NAME_IS("MarkerShift")) {
        mag = 0;
        for (index = 0; index < (int)markerShiftArr->GetSize(); index++) {
          markerShift = markerShiftArr->GetAt(index);
          if (markerShift.fromMagInd == itemInt[1] &&
            markerShift.toMagInd == itemInt[2]) {
            markerShift.shiftX = itemFlt[3];
            markerShift.shiftY = itemFlt[4];
            mag = 1;
            break;
          }
        }
        if (!mag) {
          markerShift.fromMagInd = itemInt[1];
          markerShift.toMagInd = itemInt[2];
          markerShift.shiftX = itemFlt[3];
          markerShift.shiftY = itemFlt[4];
          markerShiftArr->Add(markerShift);
        }

      } else if (NAME_IS("DewarVacParams")) {
        dewar->checkPVP = itemInt[1] != 0;
        dewar->runBufferCycle = itemInt[2] != 0;
        dewar->bufferTimeMin = itemInt[3];
        dewar->runAutoloaderCycle = itemInt[4] != 0;
        dewar->autoloaderTimeMin = itemInt[5];
        dewar->refillDewars = itemInt[6] != 0;
        dewar->dewarTimeHours = itemFlt[7];
        dewar->checkDewars = itemInt[8] != 0;
        dewar->pauseBeforeMin = itemFlt[9];
        dewar->startRefill = itemInt[10] != 0;
        dewar->startIntervalMin = itemFlt[11];
        dewar->postFillWaitMin = itemFlt[12];
        dewar->doChecksBeforeTask = itemInt[13] != 0;

      } else if (NAME_IS("SingleFileOptions")) {

        // If you add any, need to preserve in SerialEMDoc::CopyDefaultToOtherFileOpts
        otherFileOpt->fileType = itemInt[1];
        otherFileOpt->compression = itemInt[2];
        if (!itemEmpty[4]) {
          otherFileOpt->pctTruncLo = itemFlt[3];
          otherFileOpt->pctTruncHi = itemFlt[4];
        }
      } else if (NAME_IS("CookerParams")) {
        cookParams->magIndex = itemInt[1];
        cookParams->spotSize = itemInt[2];
        cookParams->intensity = itemDbl[3];
        cookParams->targetDose = itemInt[4];
        cookParams->timeInstead = itemInt[5] != 0;
        cookParams->minutes = itemFlt[6];
        cookParams->trackImage = itemInt[7] != 0;
        cookParams->cookAtTilt = itemInt[8] != 0;
        cookParams->tiltAngle = itemFlt[9];

      } else if (NAME_IS("VppConditionParams")) {
        vppParams->magIndex = itemInt[1];
        vppParams->spotSize = itemInt[2];
        vppParams->intensity = itemDbl[3];
        vppParams->alpha = itemInt[4];
        vppParams->probeMode = itemInt[5];
        vppParams->seconds = itemInt[6];
        vppParams->nanoCoulombs = itemInt[7];
        vppParams->timeInstead = itemInt[8] != 0;
        vppParams->whichSettings = itemInt[9];
        vppParams->useNearestNav = itemInt[10] != 0;
        vppParams->useNavNote = itemInt[11] != 0;
        vppParams->postMoveDelay = itemInt[12];

      } else if (NAME_IS("SnapshotParams")) {
        snapParams->imageScaleType = itemInt[1];
        snapParams->imageScaling = itemFlt[2];
        snapParams->ifScaleSizes = itemInt[3] != 0;
        snapParams->sizeScaling = itemFlt[4];
        snapParams->fileType = itemInt[5];
        snapParams->compression = itemInt[6];
        snapParams->jpegQuality = itemInt[7];
        snapParams->skipOverlays = itemInt[8];

      } else if (NAME_IS("VppCondNavText")) {
        StripItems(strLine, 1, vppParams->navText);

      } else if (NAME_IS("MultiGridParams")) {
        mgParams->appendNames = itemInt[1] != 0;
        mgParams->useSubdirectory = itemInt[2] != 0;
        mgParams->setLMMstate = itemInt[3] != 0;
        mgParams->LMMstateType = itemInt[4];
        mgParams->removeObjectiveAp = itemInt[5] != 0;
        mgParams->setCondenserAp = itemInt[6] != 0;
        mgParams->condenserApSize = itemInt[7];
        mgParams->LMMmontType = itemInt[8];
        mgParams->LMMnumXpieces = itemInt[9];
        mgParams->LMMnumYpieces = itemInt[10];
        mgParams->setLMMoverlap = itemInt[11] != 0;
        mgParams->LMMoverlapPct = itemInt[12];
        mgParams->autocontour = itemInt[13] != 0;
        mgParams->acquireMMMs = itemInt[14] != 0;
        mgParams->MMMstateType = itemInt[15];
        mgParams->MMMimageType = itemInt[16];
        mgParams->acquireLMMs = itemInt[17] != 0;
        mgParams->runFinalAcq = itemInt[18] != 0;
        mgParams->MMMnumXpieces = itemInt[19];
        mgParams->MMMnumYpieces = itemInt[20];
        mgParams->framesUnderSession = itemInt[21] > 0;
        if (!itemEmpty[23]) {
          mgParams->runMacroAfterLMM = itemInt[22] != 0;
          mgParams->macroToRun = itemInt[23];
        }
        if (!itemEmpty[25]) {
          mgParams->refineAfterRealign = itemInt[24] != 0;
          mgParams->refineImageType = itemInt[25];
        }
        if (!itemEmpty[26] && itemInt[26] >= 0)
          mgParams->C1orC2condenserAp = itemInt[26];
        if (!itemEmpty[27])
          mgParams->msVectorSource = itemInt[27];

      } else if (strItems[0].Find("MG") == 0 && (strItems[0].Find("MMMstate") == 2 ||
        strItems[0].Find("State") == 7)) {
        stateNames = strItems[0].Find("MMM") == 2 ? &mgParams->MMMstateNames[0] :
          &mgParams->finalStateNames[0];
        stateNums = strItems[0].Find("MMM") == 2 ? &mgParams->MMMstateNums[0] :
          &mgParams->finalStateNums[0];
        index = atoi((LPCTSTR)strItems[0].Right(1)) - 1;
        if (index >= 0 && index < 4) {
          stateNums[index] = itemInt[1];
          if (!itemEmpty[2])
            StripItems(strLine, 2, stateNames[index]);
        }
      } else if (NAME_IS("MGLMMstate")) {
        mgParams->LMMstateNum = itemInt[1];
        if (!itemEmpty[2])
          StripItems(strLine, 2, mgParams->LMMstateName);
      } else if (NAME_IS("MGREFstate")) {
        mgParams->refineStateNum = itemInt[1];
        if (!itemEmpty[2])
          StripItems(strLine, 2, mgParams->refineStateName);

      } else if (NAME_IS("MGSessionFile")) {
        StripItems(strLine, 1, message);
        mWinApp->mMultiGridTasks->SetLastSessionFile(message);

      } else if (NAME_IS("AutocenterParams")) {
        acParmP = &acParams;
        acParmP->camera = itemInt[1];
        acParmP->magIndex = itemInt[2];
        acParmP->spotSize = itemInt[3];
        acParmP->intensity = itemDbl[4];
        acParmP->binning = B3DMAX(1, itemInt[5]);
        acParmP->exposure = itemFlt[6];
        acParmP->useCentroid = itemInt[7];
        acParmP->probeMode = itemInt[8] < 0 ? 1 : itemInt[8];
        acParmP->shiftBeamForCen = itemInt[9] < 0 ? 0 : itemInt[9];
        acParmP->beamShiftUm = itemInt[9] < 0 ? 1.f : itemFlt[10];
        acParmP->addedShiftX = itemInt[9] < 0 ? 0.f : itemFlt[11];
        acParmP->addedShiftY = itemInt[9] < 0 ? 0.f : itemFlt[12];
        mWinApp->mMultiTSTasks->AddAutocenParams(acParmP);
      } else if (NAME_IS("ZbyGParams")) {
        zbgParam.lowDoseArea = itemInt[1];
        zbgParam.magIndex = itemInt[2];
        zbgParam.intensity = itemDbl[3];
        zbgParam.spotSize = itemInt[4];
        zbgParam.probeOrAlpha = itemInt[5];
        zbgParam.camera = itemInt[6];
        zbgParam.focusOffset = itemFlt[7];
        zbgParam.beamTilt = itemFlt[8];
        zbgParam.targetDefocus = itemFlt[9];
        zbgParam.standardFocus = itemDbl[10];
        zbgArray->Add(zbgParam);
      } else if (NAME_IS("RangeFinderParams")) {
        index = B3DMIN(1, B3DMAX(0, itemInt[1]));
        tsrParams[index].eucentricity = itemInt[2] != 0;
        tsrParams[index].walkup = itemInt[3] != 0;
        tsrParams[index].autofocus = itemInt[4] != 0;
        tsrParams[index].imageType = itemInt[5];
        tsrParams[index].startAngle = itemFlt[6];
        tsrParams[index].endAngle = itemFlt[7];
        tsrParams[index].angleInc = itemFlt[8];
        tsrParams[index].direction = itemInt[9];

      } else if (NAME_IS("ComaVsISCal")) {
        comaVsIS->magInd = itemInt[1];
        comaVsIS->spotSize = itemInt[2];
        comaVsIS->probeMode = itemInt[3];
        comaVsIS->alpha = itemInt[4];
        comaVsIS->aperture = itemInt[5];
        comaVsIS->intensity = itemFlt[6];
        comaVsIS->matrix.xpx = itemFlt[7];
        comaVsIS->matrix.xpy = itemFlt[8];
        comaVsIS->matrix.ypx = itemFlt[9];
        comaVsIS->matrix.ypy = itemFlt[10];
        comaVsIS->astigMat.xpx = comaVsIS->astigMat.xpy = comaVsIS->astigMat.ypx =
          comaVsIS->astigMat.ypy = 0.;
        if (!itemEmpty[14]) {
          comaVsIS->astigMat.xpx = itemFlt[11];
          comaVsIS->astigMat.xpy = itemFlt[12];
          comaVsIS->astigMat.ypx = itemFlt[13];
          comaVsIS->astigMat.ypy = itemFlt[14];
        }

      } else if (NAME_IS("NavigatorStockFile"))
        StripItems(strLine, 1, navParams->stockFile);
      else if (NAME_IS("ProcessOverlayChannels"))
        mWinApp->mProcessImage->SetOverlayChannels(strItems[1]);
      else if (NAME_IS("ImportOverlayChannels"))
        navParams->overlayChannels = strItems[1];
      else if (NAME_IS("CtffindParams")) {
        mWinApp->mProcessImage->SetCtffindOnClick(itemInt[1] != 0);
        mWinApp->mProcessImage->SetSlowerCtfFit(itemInt[2]);
        mWinApp->mProcessImage->SetExtraCtfStats(itemInt[3]);
        mWinApp->mProcessImage->SetDrawExtraCtfRings(itemInt[4]); 
        mWinApp->mProcessImage->SetCtfFitFocusRangeFac(itemFlt[5]);
        if (!itemEmpty[6]) {
          mWinApp->mProcessImage->SetCtfFindPhaseOnClick(itemInt[6] != 0);
          mWinApp->mProcessImage->SetCtfFixAstigForPhase(itemInt[7] != 0);
          mWinApp->mProcessImage->SetCtfMinPhase(itemInt[8]);
          mWinApp->mProcessImage->SetCtfMaxPhase(itemInt[9]);
        }
      } else if (NAME_IS("RemoteControlParams")) {
        mWinApp->SetShowRemoteControl(itemInt[1] != 0);
        mWinApp->mRemoteControl.SetBeamIncrement(itemFlt[2]);
        mWinApp->mRemoteControl.SetIntensityIncrement(itemFlt[3]);
        mWinApp->mRemoteControl.m_bMagIntensity = itemInt[4] != 0;

        // set the beam/stage selector before either of its increments
        //if (!itemEmpty[6])
          //mWinApp->mRemoteControl.SetBeamOrStage(itemInt[6]);
        if (!itemEmpty[5])
          mWinApp->mRemoteControl.SetFocusIncrementIndex(itemInt[5]);
        if (!itemEmpty[7])
          mWinApp->mRemoteControl.SetStageIncrementIndex(itemInt[7]);

      // Tool dialog placements and states now work when reading settings
      } else if (NAME_IS("ToolDialogStates") || NAME_IS("ToolDialogStates2")) {
        index = 0;
        mWinApp->SetAbsoluteDlgIndex(NAME_IS("ToolDialogStates2"));
        while (!strItems[index + 1].IsEmpty() && index < MAX_TOOL_DLGS) {
          initialDlgState[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
      } else if (NAME_IS("ToolDialogPlacement")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_TOOL_DLGS || strItems[5].IsEmpty()) {
            AfxMessageBox("Error in panel placement line in settings file "
              + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          ConstrainWindowPlacement(&itemInt[2], &itemInt[3], &itemInt[4], &itemInt[5], 
            !startingProg);
          dlgPlacements[index].left = itemInt[2];
          dlgPlacements[index].top = itemInt[3];
          dlgPlacements[index].right = itemInt[4];
          dlgPlacements[index].bottom = itemInt[5];
        }

      } else if (NAME_IS("MacroButtonNumbers")) {
        index = 0;
        while (!strItems[index + 1].IsEmpty() && index < NUM_SPINNER_MACROS) {
          macroButtonNumbers[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
      } else if (NAME_IS("MoreScriptButtons")) {
        index = 0;
        while (!strItems[index + 1].IsEmpty() && index + 3 < NUM_SPINNER_MACROS) {
          macroButtonNumbers[index + 3] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
      } else if (NAME_IS("MacroToolbarButtons")) {
        mWinApp->mMacroProcessor->SetNumToolButtons(itemInt[1]);
        if (!itemEmpty[2])
          mWinApp->mMacroProcessor->SetToolButHeight(itemInt[2]);
      }
      else
        recognized2 = false;

      if (recognized || recognized2) {
        recognized = true;
      }
#define SET_TEST_SECT25
#include "SettingsTests.h"
#undef SET_TEST_SECT25

      else if (NAME_IS("WindowPlacement")) {
        mWinApp->GetWindowPlacement(&winPlace);
        winPlace.showCmd = itemInt[1];
        if (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                                 MAX_TOKENS)) 
          break;
        winPlace.ptMaxPosition.x = itemInt[0];
        winPlace.ptMaxPosition.y = itemInt[1];
        winPlace.ptMinPosition.x = itemInt[2];
        winPlace.ptMinPosition.y = itemInt[3];
        winPlace.rcNormalPosition.left = itemInt[4];
        winPlace.rcNormalPosition.top = itemInt[5];
        winPlace.rcNormalPosition.right = itemInt[6];
        winPlace.rcNormalPosition.bottom = itemInt[7];
        ConstrainWindowPlacement(&winPlace, !startingProg);
        if (winPlace.rcNormalPosition.right != NO_PLACEMENT &&
          winPlace.rcNormalPosition.bottom > 0)
          mWinApp->SetWindowPlacement(&winPlace);

      } else if (strItems[0].Find("Placement") == strItems[0].GetLength() - 9) {
        index = NAME_IS("OneEditerPlacement") ? 1 : 0;
        if (strItems[index + 10].IsEmpty() || 
          (index && (itemInt[1] < 0 || itemInt[1] >= MAX_MACROS))) {
          if (index && itemInt[1] < 0)
            AfxMessageBox("Error in window placement line in settings file "
              + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          place = mWinApp->GetNavPlacement();
          if (NAME_IS("MeterPlacement"))
            place = mWinApp->mScopeStatus.GetMeterPlacement();
          else if (NAME_IS("DosePlacement"))
            place = mWinApp->mScopeStatus.GetDosePlacement();
          else if (NAME_IS("RotAlignPlacement"))
            place = mWinApp->mNavHelper->GetRotAlignPlacement();
          else if (NAME_IS("MultiShotPlacement"))
            place = mWinApp->mNavHelper->GetMultiShotPlacement(false);
          else if (NAME_IS("HoleFinderPlacement"))
            place = mWinApp->mNavHelper->GetHoleFinderPlacement();
          else if (NAME_IS("MultiCombinerPlacement"))
            place = mWinApp->mNavHelper->GetMultiCombinerPlacement();
          else if (NAME_IS("AutoContPlacement"))
            place = mWinApp->mNavHelper->GetAutoContDlgPlacement();
          else if (NAME_IS("MultiGridPlacement"))
            place = mWinApp->mNavHelper->GetMultiGridPlacement();
          else if (NAME_IS("NavAcqPlacement"))
            place = mWinApp->mNavHelper->GetAcquireDlgPlacement(false);
          else if (NAME_IS("CtffindPlacement"))
            place = mWinApp->mProcessImage->GetCtffindPlacement();
          else if (NAME_IS("AutocenPlacement"))
            place = mWinApp->mMultiTSTasks->GetAutocenPlacement();
          else if (NAME_IS("VppCondPlacement"))
            place = mWinApp->mMultiTSTasks->GetConditionPlacement();
          else if (NAME_IS("ZbyGSetupPlacement"))
            place = mWinApp->mParticleTasks->GetZbyGPlacement();
          else if (NAME_IS("SnapshotPlacement"))
            place = mWinApp->GetScreenShotPlacement();
          else if (NAME_IS("SecondaryLogPlacement"))
            place = mWinApp->GetSecondaryLogPlacement();
          else if (NAME_IS("StatePlacement")) {
            mWinApp->SetOpenStateWithNav(itemInt[1] != 0);
            place = mWinApp->mNavHelper->GetStatePlacement();
          } else if (NAME_IS("ReadDlgPlacement"))
            place = mDocWnd->GetReadDlgPlacement();
          else if (NAME_IS("StageToolPlacement"))
            place = mWinApp->GetStageToolPlacement();
          else if (NAME_IS("MacroEditerPlacement"))
            place = mWinApp->mMacroProcessor->GetEditerPlacement();
          else if (NAME_IS("OneEditerPlacement")) {
            place = mWinApp->mMacroProcessor->GetEditerPlacement() + itemInt[1];
            mWinApp->SetReopenMacroEditor(itemInt[1], itemInt[2] != 0);
          } else if (NAME_IS("OneLinePlacement")) {
            place = mWinApp->mMacroProcessor->GetOneLinePlacement();
            mWinApp->SetReopenMacroEditor(MAX_MACROS, itemInt[1] != 0);
          } else if (NAME_IS("MacroToolPlacement")) {
            mWinApp->SetReopenMacroToolbar(itemInt[1] != 0);
            place = mWinApp->mMacroProcessor->GetToolPlacement();
          } else if (NAME_IS("LogPlacement")) {
            mWinApp->SetReopenLog(itemInt[1] != 0);
            place = mWinApp->GetLogPlacement();
          } 
          place->showCmd = itemInt[index + 2];
          place->ptMaxPosition.x = itemInt[index + 3];
          place->ptMaxPosition.y = itemInt[index + 4];
          place->ptMinPosition.x = itemInt[index + 5];
          place->ptMinPosition.y = itemInt[index + 6];
          place->rcNormalPosition.left = itemInt[index + 7];
          place->rcNormalPosition.top = itemInt[index + 8];
          place->rcNormalPosition.right = itemInt[index + 9];
          place->rcNormalPosition.bottom = itemInt[index + 10];
          ConstrainWindowPlacement(place, !startingProg);
          if (NAME_IS("MacroEditerPlacement")) {
            for (index = 1; index < MAX_MACROS; index++)
              place[index] = place[0];
          }
          if (!startingProg) {
            SET_PLACEMENT("NavigatorPlacement", mWinApp->mNavigator);
            SET_PLACEMENT("MeterPlacement", mWinApp->mScopeStatus.mScreenMeter);
            SET_PLACEMENT("DosePlacement", mWinApp->mScopeStatus.mDoseMeter);
            SET_PLACEMENT("RotAlignPlacement", mWinApp->mNavHelper->mRotAlignDlg);
            SET_PLACEMENT("LogPlacement", mWinApp->mLogWindow);
            SET_PLACEMENT("MultiShotPlacement", mWinApp->mNavHelper->mMultiShotDlg);
            SET_PLACEMENT("MultiCombinerPlacement",
              mWinApp->mNavHelper->mMultiCombinerDlg);
            SET_PLACEMENT("MultiGridPlacement", mWinApp->mNavHelper->mMultiGridDlg);
            SET_PLACEMENT("CtffindPlacement", mWinApp->mProcessImage->mCtffindParamDlg);
            SET_PLACEMENT("AutocenPlacement", mWinApp->mAutocenDlg);
            SET_PLACEMENT("VppCondPlacement", mWinApp->mVPPConditionSetup);
            SET_PLACEMENT("ZbyGSetupPlacement", mWinApp->mParticleTasks->mZbyGsetupDlg);
            SET_PLACEMENT("SnapshotPlacement", mWinApp->mScreenShotDialog);
            SET_PLACEMENT("StatePlacement", mWinApp->mNavHelper->mStateDlg);
            SET_PLACEMENT("ReadDlgPlacement", mDocWnd->mReadFileDlg);
            SET_PLACEMENT("StageToolPlacement", mWinApp->mStageMoveTool);
            SET_PLACEMENT("OneLinePlacement", mWinApp->mMacroProcessor->mOneLineScript);
            SET_PLACEMENT("MacroToolPlacement", mWinApp->mMacroToolbar);
            if (NAME_IS("HoleFinderPlacement") &&
              mWinApp->mNavHelper->mHoleFinderDlg->IsOpen())
              mWinApp->mNavHelper->mHoleFinderDlg->SetWindowPlacement(place);
            if (NAME_IS("AutoContPlacement") &&
              mWinApp->mNavHelper->mAutoContouringDlg->IsOpen())
              mWinApp->mNavHelper->mAutoContouringDlg->SetWindowPlacement(place);
          }
        }

      } else if (NAME_IS("StateParameters")) {
        stateP = mWinApp->mNavHelper->NewStateParam(false);
        stateP->lowDose = itemInt[1];;
        if (stateP->lowDose)    // Add 100 until low dose params found
          stateP->lowDose += 100;    
        stateP->camIndex = itemInt[2];
        stateP->magIndex = itemInt[3];
        stateP->spotSize = itemInt[4];
        stateP->intensity = itemDbl[5];
        stateP->slitIn = itemInt[6] != 0;
        stateP->energyLoss = itemFlt[7];
        stateP->slitWidth = itemFlt[8];
        stateP->zeroLoss = itemInt[9] != 0;
        stateP->binning = itemInt[10];
        stateP->xFrame = itemInt[11];
        stateP->yFrame = itemInt[12];
        stateP->exposure = itemFlt[13];
        stateP->drift = itemFlt[14];
        stateP->shuttering = itemInt[15];
        stateP->K2ReadMode = itemEmpty[16] ? 0 : itemInt[16];
        stateP->probeMode = itemEmpty[17] ? -1 : itemInt[17];
        stateP->frameTime = (float)(itemEmpty[18] ? 0. : itemDbl[18]);
        stateP->doseFrac = itemEmpty[19] ? 0 : itemInt[19];
        stateP->saveFrames = itemEmpty[20] ? 0 : itemInt[20];
        stateP->processing = itemEmpty[21] ? -1 : itemInt[21];
        stateP->alignFrames = itemEmpty[22] ? -1 : itemInt[22];
        stateP->useFrameAlign = itemEmpty[23] ? -1 : itemInt[23];
        stateP->faParamSetInd = itemEmpty[24] ? -1 : itemInt[24];
        stateP->readModeView = itemEmpty[25] ? -1 : itemInt[25];
        stateP->readModeFocus = itemEmpty[26] ? -1 : itemInt[26];
        stateP->readModeTrial = itemEmpty[27] ? -1 : itemInt[27];
        stateP->readModePrev = itemEmpty[28] ? -1 : itemInt[28];
        stateP->readModeSrch = itemEmpty[29] ? -1 : itemInt[29];
        stateP->readModeMont = itemEmpty[30] ? -1 : itemInt[30];
        stateP->focusAxisPos = EXTRA_NO_VALUE;
        stateP->beamAlpha = itemEmpty[31] ? -999 : itemInt[31];
        stateP->targetDefocus = itemEmpty[32] ? -9999.f : itemFlt[32];
        stateP->ldDefocusOffset = itemEmpty[33] ? -9999.f : itemFlt[33];
        if (!itemEmpty[35]) {
          stateP->ldShiftOffsetX = itemFlt[34];
          stateP->ldShiftOffsetY = itemFlt[35];
        }
        if (!itemEmpty[36])
          stateP->montMapConSet = itemInt[36] != 0;
        // ADD NEW ITEMS TO NAV READING
 
      } else if (NAME_IS("StateName")) {
        index = itemInt[1];
        if (index < 0 || index >= stateArray->GetSize()) {
          AfxMessageBox("Index out of range in state name line in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          stateP = stateArray->GetAt(index);
          StripItems(strLine, 2, stateP->name);
        }

      } else if (NAME_IS("StateParams2")) {
        index = itemInt[1];
        if (index < 0 || index >= stateArray->GetSize()) {
          AfxMessageBox("Index out of range in StateParams2 line in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          stateP = stateArray->GetAt(index);
          if (itemFlt[2] > -999)
            stateP->focusAxisPos = itemFlt[2];
          stateP->rotateAxis = itemInt[3] != 0;
          stateP->axisRotation = itemInt[4];
          stateP->focusXoffset = itemInt[5];
          stateP->focusYoffset = itemInt[6];
        }

      } else if (NAME_IS("LowDoseParameters")) {
        index = itemInt[1];
        mag = itemInt[2];
        spot = itemInt[3];
        GIF = strItems[6].IsEmpty() ? 0 : itemInt[6];
        if (index < 0 && index >= -stateArray->GetSize())
          stateP = stateArray->GetAt(-index - 1);
        if (index < -stateArray->GetSize() || index >= MAX_LOWDOSE_SETS || GIF < 0 ||
          GIF > 2 || strItems[5].IsEmpty() || (index < 0 && !stateP->lowDose) ||
          (!strItems[6].IsEmpty() && strItems[9].IsEmpty()) ||
          (!strItems[11].IsEmpty() && strItems[12].IsEmpty()) ) {
          AfxMessageBox("Error in low dose parameter line in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          if (index >= 0) {
            ldp = &mLowDoseParams[GIF * MAX_LOWDOSE_SETS + index];
          } else {
            ldp = &stateP->ldParams;
            stateP->lowDose -= 100;    // Restore to proper value
          }
          if (mag < 0) {
            ldp->magIndex = 0;
            ldp->camLenIndex = -mag;
          } else {
            ldp->magIndex = mag;
            ldp->camLenIndex = 0;
          }
          ldp->spotSize = spot;
          ldp->intensity = itemDbl[4];
          ldp->axisPosition = itemDbl[5];
          if (fabs(ldp->axisPosition) > 100.)
            ldp->axisPosition = 0.;
          ldp->probeMode = 1;
          if (!itemEmpty[6]) {
            ldp->slitIn = itemInt[7] != 0;
            ldp->slitWidth = itemFlt[8];
            ldp->energyLoss = itemFlt[9];
          }
          if (!itemEmpty[10]) 
            ldp->zeroLoss = itemInt[10] != 0;
          if (!itemEmpty[11]) {
            ldp->beamDelX = itemDbl[11];
            ldp->beamDelY = itemDbl[12];
          }
          if (!itemEmpty[15]) {
            ldp->beamAlpha = itemFlt[13];
            ldp->diffFocus = itemDbl[14];
          }
          if (!itemEmpty[15]) {
            ldp->beamTiltDX = itemDbl[15];
            ldp->beamTiltDY = itemDbl[16];
          }
          if (!itemEmpty[17])
            ldp->probeMode = itemInt[17];
          if (!itemEmpty[18]) {
            ldp->darkFieldMode = itemInt[18];
            ldp->dfTiltX = itemDbl[19];
            ldp->dfTiltY = itemDbl[20];
          }
          if (!itemEmpty[21])
            ldp->EDMPercent = itemFlt[21];
          // ADD NEW ITEMS TO NAV STATE READING
        }

      } else if (NAME_IS("ImageXrayCriteria")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_CAMERAS || strItems[4].IsEmpty()) {
          AfxMessageBox("Incorrect entry in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          mCamParam[index].imageXRayAbsCrit = itemFlt[2];
          mCamParam[index].imageXRayNumSDCrit = itemFlt[3];
          mCamParam[index].imageXRayBothCrit = itemInt[4];
        }

      } else if (NAME_IS("GainReferenceTarget")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_CAMERAS || strItems[4].IsEmpty()) {
          AfxMessageBox("Incorrect entry in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          mCamParam[index].gainRefBinning = itemInt[2];
          mCamParam[index].gainRefTarget = itemInt[3];
          mCamParam[index].gainRefFrames = itemInt[4];
          if (!strItems[5].IsEmpty()) {
            mCamParam[index].gainRefAverageDark = itemInt[5];
            mCamParam[index].gainRefNumDarkAvg = itemInt[6];
            mCamParam[index].TSAverageDark = itemInt[7];
          }
          if (!strItems[8].IsEmpty())
            mCamParam[index].gainRefSaveRaw = itemInt[8];
       }

      } else if (NAME_IS("GridMapLimits")) {
        for (index = 0; index < 4; index++)
          gridLim[index] = (float)atof(strItems[index + 1]);

      } else if (NAME_IS("GainRefNormalizeSpot")) {  // Obsolete or confused setting...
        mWinApp->mGainRefMaker->SetCalibrateDose(itemInt[1] != 0);
      } else if (NAME_IS("DErefExposures")) {
        for (index = 0; index < MAX_DE_REF_TYPES; index++)
          deExposures[index] = itemFlt[index + 1];
      } else if (NAME_IS("DErefNumRepeats")) {
        for (index = 0; index < MAX_DE_REF_TYPES; index++)
          deNumRepeats[index] = itemInt[index + 1];
      } else if (NAME_IS("DElastRefParams")) {
        mWinApp->mGainRefMaker->SetDElastProcessType(itemInt[1]);
        mWinApp->mGainRefMaker->SetDElastReferenceType(itemInt[2]);
        mWinApp->mGainRefMaker->SetDEuseHardwareBin(itemInt[3]);
        if (!itemEmpty[4])
          mWinApp->mGainRefMaker->SetDEuseHardwareROI(itemInt[4]);
      } else if (NAME_IS("LowDoseViewShift")) {
        mWinApp->mLowDoseDlg.mViewShiftX[0] = itemDbl[1];
        mWinApp->mLowDoseDlg.mViewShiftY[0] = itemDbl[2];
      } else if (NAME_IS("LDSearchShift")) {
        mWinApp->mLowDoseDlg.mViewShiftX[1] = itemDbl[1];
        mWinApp->mLowDoseDlg.mViewShiftY[1] = itemDbl[2];
      } else if (NAME_IS("ResetRealignIterationCriterion")) {
        mWinApp->mComplexTasks->SetRSRAUserCriterion(itemFlt[1]);
      } else if (NAME_IS("MaxMacros")) {
        mMaxReadInMacros = itemInt[1];
      } else
        recognized25 = false;

      if (recognized || recognized25) {
        recognized = true;
      }
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3
      else
        recognized3 = false;

      if (recognized || recognized3) {
        recognized = true;
      } else if (NAME_IS("TiltSeriesLowMagIndex")) {
        mTSParam->lowMagIndex[0] = itemInt[1];
        mTSParam->lowMagIndex[1] = itemInt[2];
        mTSParam->lowMagIndex[2] = itemInt[3];
      
      // Retired 2/13/04 - moved to properties
      // Dose cal moved to short term cal 12/21/05
      // Retired shoot after autofocus 12/22/06
      // Eliminated down area 8/3/18
      } else if (NAME_IS("TiltSeriesXFitInterval") ||
        NAME_IS("TiltSeriesYFitInterval") ||
        NAME_IS("TiltSeriesZFitInterval") ||
        NAME_IS("TiltSeriesXMinForQuadratic") ||
        NAME_IS("TiltSeriesYMinForQuadratic") || NAME_IS("LowDoseAreaToShow") ||
        NAME_IS("DoseCalibration") || NAME_IS("ShootAfterAutofocus"))
        err = 0;

      else if (NAME_IS("TiltSeriesFocusSeries") ||
        NAME_IS("TiltSeriesFilterSeries") || NAME_IS("TiltSeriesExtraChannels") ||
        NAME_IS("TiltSeriesExtraExposure")) {
        strCopy = strLine;
        FindToken(strCopy, strItems[0]);
        if (NAME_IS("TiltSeriesFocusSeries"))
          index = StringToEntryList(1, strCopy, mTSParam->numExtraFocus, NULL,
            mTSParam->extraFocus, MAX_EXTRA_RECORDS);
        else if (NAME_IS("TiltSeriesExtraExposure"))
          index = StringToEntryList(1, strCopy, mTSParam->numExtraExposures, NULL,
            mTSParam->extraExposures, MAX_EXTRA_RECORDS);
        else if NAME_IS("TiltSeriesFilterSeries")
          index = StringToEntryList(2, strCopy, mTSParam->numExtraFilter,
            mTSParam->extraSlits, mTSParam->extraLosses, MAX_EXTRA_RECORDS);
        else
          index = StringToEntryList(3, strCopy, mTSParam->numExtraChannels,
            mTSParam->extraChannels, NULL, MAX_STEM_CHANNELS);
        if (index)
          AfxMessageBox("Extra Record series line badly formatted or too long in settings"
            " file " + strFileName + " :\n" + strLine, MB_EXCLAME);
      } else if (NAME_IS("TiltSeriesVariation")) {
        if (itemInt[4] < 0 || itemInt[4] >= MAX_VARY_TYPES || 
          mTSParam->numVaryItems >= MAX_TS_VARIES) {
          AfxMessageBox("Invalid type of change or too many changes for array in settings"
            " file " + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          mTSParam->varyArray[mTSParam->numVaryItems].angle = itemFlt[1];
          mTSParam->varyArray[mTSParam->numVaryItems].plusMinus = itemInt[2] != 0;
          mTSParam->varyArray[mTSParam->numVaryItems].linear = itemInt[3] != 0;
          mTSParam->varyArray[mTSParam->numVaryItems].type = itemInt[4];
          mTSParam->varyArray[mTSParam->numVaryItems++].value = itemFlt[5];
        }
      } else if (NAME_IS("TiltSeriesBidirParams")) {
        mTSParam->doBidirectional = itemInt[1] != 0;
        mTSParam->bidirAngle = itemFlt[2];
        mTSParam->anchorBidirWithView = itemInt[3] != 0;
        mTSParam->walkBackForBidir = itemInt[4] != 0;
        if (!itemEmpty[5])
          mTSParam->retainBidirAnchor = itemInt[5] != 0;
      } else if (NAME_IS("TiltSeriesDoseSymParams")) {
        mTSParam->doDoseSymmetric = itemInt[1] != 0;
        mTSParam->dosymBaseGroupSize = itemInt[2];
        mTSParam->dosymIncreaseGroups = itemInt[3] != 0;
        mTSParam->dosymIncStartAngle = itemFlt[4];
        mTSParam->dosymGroupIncAmount = itemInt[5];
        mTSParam->dosymGroupIncInterval = itemInt[6];
        mTSParam->dosymDoRunToEnd = itemInt[7] != 0;
        mTSParam->dosymRunToEndAngle = itemFlt[8];
        mTSParam->dosymAnchorIfRunToEnd = itemInt[9] != 0;
        mTSParam->dosymMinUniForAnchor = itemInt[10];
        mTSParam->dosymStartingISLimit = itemFlt[11];
        if (!itemEmpty[12])
          mTSParam->dosymSkipBacklash = itemInt[12] != 0;

      } else if (NAME_IS("TiltSeriesBDAnchorMags")) {
        for (index = 0; index < 6; index++)
          mTSParam->bidirAnchorMagInd[index] = itemInt[index + 1];
      } else if (NAME_IS("TiltSeriesRunMacro")) {
        mWinApp->mTSController->SetRunMacroInTS(itemInt[1] != 0);
        mWinApp->mTSController->SetMacroToRun(itemInt[2]), 
        mWinApp->mTSController->SetStepAfterMacro(itemInt[3]);
      } else if (NAME_IS("TSTermOnHighExposure")) {
        mWinApp->mTSController->SetTermOnHighExposure(itemInt[1] != 0);
        mWinApp->mTSController->SetMaxExposureIncrease(itemFlt[2]);
      } else if (MatchNoCase("TiltSeriesBidirDimPolicy")) {
        mWinApp->mTSController->SetEndOnHighDimImage(itemInt[1] != 0);
        mWinApp->mTSController->SetDimEndsAbsAngle(itemInt[2]);
        mWinApp->mTSController->SetDimEndsAngleDiff(itemInt[3]);
      } else if (NAME_IS("TSExtraSuffixes")) {
        StripItems(strLine, 1, mTSParam->extraFileSuffixes);
      } else if (MatchNoCase("CloseValvesDuringMessageBox")) {
        mWinApp->mTSController->SetMessageBoxCloseValves(itemInt[1] != 0);
        mWinApp->mTSController->SetMessageBoxValveTime(itemFlt[2]);
      } else if (NAME_IS("EmailAddress")) {
        StripItems(strLine, 1, strCopy);
        mWinApp->mMailer->SetSendTo(strCopy);
      } else if (NAME_IS("TSSetupPanelStates")) {
        for (index = 0; index < NUM_TSS_PANELS - 1; index++)
          tssPanelStates[index] = itemInt[index + 1];
      /*else if (NAME_IS("AddedSTEMrotation"))
        mWinApp->SetAddedSTEMrotation((float)itemInt[1]); */
      } else if (NAME_IS("NonGIFMatchPixelIntensity")) {
        mWinApp->SetNonGIFMatchPixel(itemInt[1] != 0);
        mWinApp->SetNonGIFMatchIntensity(itemInt[2] != 0);

      } else if (NAME_IS("FrameAlignParams1")) {
        if (faLastArrayIndex < 0)
          faParamArray->RemoveAll();
        if (itemInt[1] != faLastFileIndex || faLastArrayIndex < 0) {
          faParamArray->Add(faParam);
          faLastArrayIndex++;
        }
        faData = ((FrameAliParams *)faParamArray->GetData()) + faLastArrayIndex;
        faLastFileIndex = itemInt[1];
        faData->strategy = itemInt[2];
        faData->aliBinning = itemInt[3];
        faData->numAllVsAll = itemInt[4]; 
        faData->rad2Filt1 = itemFlt[5];
        faData->rad2Filt2 = itemFlt[6];
        faData->rad2Filt3 = itemFlt[7];
        faData->rad2Filt4 = itemFlt[8]; 
        faData->hybridShifts = itemInt[9] != 0;
        faData->sigmaRatio = itemFlt[10];
        faData->refRadius2 = itemFlt[11]; 
        faData->doRefine = itemInt[12] != 0;
        faData->refineIter = itemInt[13];
        faData->useGroups = itemInt[14] != 0;
        faData->groupSize = itemInt[15];
        faData->doSmooth = itemInt[16] != 0;
        faData->smoothThresh = itemInt[17]; 
        faData->shiftLimit = itemInt[18];

      } else if (NAME_IS("FrameAlignParams2") || NAME_IS("FrameAliSetName")) {
        if (itemInt[1] != faLastFileIndex || faLastArrayIndex < 0) {
          AfxMessageBox("Frame alignment parameter or set name line not preceded by a\n"
            "FrameAlignParams1 with the same parameter set index in settings\n"
            " file " + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          faData = ((FrameAliParams *)faParamArray->GetData()) + faLastArrayIndex;
          if (NAME_IS("FrameAlignParams2")) {
            faData->truncate = itemInt[2] != 0;
            faData->truncLimit = itemFlt[3];
            faData->antialiasType = itemInt[4]; 
            faData->stopIterBelow = itemFlt[5];
            faData->groupRefine = itemInt[6] != 0;
            faData->keepPrecision = itemInt[7] != 0;
            faData->outputFloatSums = itemInt[8] != 0;
            if (itemEmpty[9]) {
              faData->alignSubset = false;
              faData->subsetStart = 1;
              faData->subsetEnd = 20;
              faData->sizeRestriction = 0;
              faData->whereRestriction = 0;
            } else {
              faData->alignSubset = itemInt[9] != 0;
              faData->subsetStart = itemInt[10];
              faData->subsetEnd = itemInt[11];
              faData->sizeRestriction = itemInt[12];
              faData->whereRestriction = itemInt[13];
            }
            if (itemEmpty[14]) {
              faData->binToTarget = false;
              faData->targetSize = 1000;
            } else {
              faData->binToTarget = itemInt[14] > 0;
              faData->targetSize = itemInt[15];
            }
            if (itemEmpty[16])
              faData->EERsuperRes = 0;
            else
              faData->EERsuperRes = itemInt[16];
          } else {
            StripItems(strLine, 2, faData->name);
          }
        }
      } else if (NAME_IS("FrameAliGlobals")) {
        useGPU4K2Ali[0] = itemInt[1] != 0;
        useGPU4K2Ali[1] = itemInt[2] != 0;
        useGPU4K2Ali[2] = itemInt[3] != 0;
        mWinApp->SetFrameAlignMoreOpen(itemInt[4] != 0);
        mWinApp->mCamera->SetAlignWholeSeriesInIMOD(itemInt[5] != 0);
      } else if (NAME_IS("FalconAliUseGPU")) {
        mWinApp->mFalconHelper->SetUseGpuForAlign(0, itemInt[1]);
        mWinApp->mFalconHelper->SetUseGpuForAlign(1, itemInt[2]);
      } else if (NAME_IS("AlignframesComPath")) {
        StripItems(strLine, 1, strCopy);
        mWinApp->mCamera->SetAlignFramesComPath(strCopy);
      } else
        unrecognized += strLine + "\n";

      nLines++;
    }
    if (err > 0)
      retval = err;
      
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  if (!unrecognized.IsEmpty())
    AfxMessageBox("Unrecognized entries in settings file " + strFileName 
      + " :\n" + unrecognized , MB_EXCLAME);

  // If there are no frame align params, set up default sets
  mWinApp->mCamera->AddFrameAliDefaultsIfNone();
  
  // Remove state params that are supposed to be low dose but don't have LD param
  err = 0;
  for (index = (int)stateArray->GetSize() - 1; index >= 0; index--) {
    stateP = stateArray->GetAt(index);
    if (stateP->lowDose > 90) {
      stateArray->RemoveAt(index);
      err++;
      delete stateP;
    }
  }
  if (err) {
    strLine.Format("Removed %d low dose states from list because no matching \n"
      "low dose parameters were found in the settings file", err);
    AfxMessageBox(strLine, MB_EXCLAME);
  }

  // Purge the duplicates from tilt series variations
  index = -1;
  CTSVariationsDlg::PurgeVariations(mTSParam->varyArray, mTSParam->numVaryItems, index);

  // Keep track if reading the script pack fails
  strLine = mDocWnd->GetCurScriptPackPath();
  recognized = true;
  if (!strLine.IsEmpty()) {
    if (ReadMacrosFromFile(strLine, strFileName, MAX_MACROS + MAX_ONE_LINE_SCRIPTS)) {
      recognized = false;
      strLine = "";
    } else
      mDocWnd->SetCurScriptPackPath(strLine);
  }
  mDocWnd->SetReadScriptPack(!strLine.IsEmpty());

  // If no package read, set the name of the output package, switching to current name
  // of system settings being read.  Then try to read THAT package so we don't just blow
  // it away
  if (strLine.IsEmpty()) {
    message = readingSys ? mDocWnd->GetCurrentSettingsPath() : strFileName;
    UtilSplitExtension(message, strLine, strCopy);
    strLine += "-scripts.txt";
    mDocWnd->SetCurScriptPackPath(strLine);
    if (!recognized && !ReadMacrosFromFile(strLine, message,
      MAX_MACROS + MAX_ONE_LINE_SCRIPTS))
      mDocWnd->SetReadScriptPack(true);

    // But if there was no script pack defined in settings, we need to stick with macros
    // in there if any but save an existing file
    if (recognized && CFile::GetStatus((LPCTSTR)strLine, status)) {
      message = strLine;
      message.Replace("-scripts.txt", "-scripts-saved.txt");
      if (CopyFile((LPCTSTR)strLine, (LPCTSTR)message, true))
        AfxMessageBox("Existing copy of " + strLine + "\r\nwas saved as " + message + 
          "\r\nto keep it from being overwritten by scripts from the settings file");
    }
  }

  mWinApp->mMacroProcessor->TransferOneLiners(false);

  // Return -1 if only one line was read (system path)
  if (!retval && nLines == 1) retval = -1;
  if (retval >= 0 && !mWinApp->GetStartingProgram())
    ReportSpecialOptions();
  return retval;
}

#undef INT_SETT_GETSET
#undef BOOL_SETT_GETSET
#undef FLOAT_SETT_GETSET
#undef DOUBLE_SETT_GETSET
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN

// Macros for writing each kind of variable
#define INT_SETT_GETSET(a, b, c) \
  WriteInt(a, b##Get##c());
#define BOOL_SETT_GETSET(a, b, c) \
  WriteInt(a, b##Get##c() ? 1 : 0);
#define FLOAT_SETT_GETSET(a, b, c) \
  WriteFloat(a, b##Get##c());
#define DOUBLE_SETT_GETSET(a, b, c) \
  WriteFloat(a, (float)b##Get##c());
#define INT_SETT_ASSIGN(a, b) \
  WriteInt(a, b);
#define BOOL_SETT_ASSIGN(a, b) \
  WriteInt(a, b ? 1 : 0);
#define FLOAT_SETT_ASSIGN(a, b) \
  WriteFloat(a, b);


void CParameterIO::WriteSettings(CString strFileName)
{
  ControlSet *cs;
  float pctLo, pctHi;
  FileOptions *fileOpt = mDocWnd->GetFileOpt();
  FileOptions *otherFileOpt = mDocWnd->GetOtherFileOpt();
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  CString *macros = mWinApp->GetMacros();
  DialogTable *dlgTable = mWinApp->GetDialogTable();
  int *dlgColorIndex = mWinApp->GetDlgColorIndex();
  int *initialDlgState = mWinApp->GetInitialDlgState();
  RECT *dlgPlacements = mWinApp->GetDlgPlacements();
  CFocusManager *focusMan = mWinApp->mFocusManager;
  CString states = "ToolDialogStates2";
  CString oneState;
  CString macCopy;
  int i, j, ldi, ldj;
  float *gridLim = mWinApp->mNavHelper->GetGridLimits();
  WINDOWPLACEMENT winPlace;
  WINDOWPLACEMENT *logPlace = mWinApp->GetLogPlacement();
  WINDOWPLACEMENT *navPlace = mWinApp->GetNavPlacement();
  WINDOWPLACEMENT *statePlace = mWinApp->mNavHelper->GetStatePlacement();
  WINDOWPLACEMENT *meterPlace = mWinApp->mScopeStatus.GetMeterPlacement();
  WINDOWPLACEMENT *dosePlace = mWinApp->mScopeStatus.GetDosePlacement();
  WINDOWPLACEMENT *toolPlace = mWinApp->mMacroProcessor->GetToolPlacement();
  WINDOWPLACEMENT *oneLinePlace = mWinApp->mMacroProcessor->GetOneLinePlacement();
  WINDOWPLACEMENT *readPlace = mDocWnd->GetReadDlgPlacement();
  WINDOWPLACEMENT *stageToolPlace = mWinApp->GetStageToolPlacement();
  WINDOWPLACEMENT *ctffindPlace = mWinApp->mProcessImage->GetCtffindPlacement();
  WINDOWPLACEMENT *rotAlignPlace = mWinApp->mNavHelper->GetRotAlignPlacement();
  WINDOWPLACEMENT *multiShotPlace = mWinApp->mNavHelper->GetMultiShotPlacement(true);
  WINDOWPLACEMENT *autocenPlace = mWinApp->mMultiTSTasks->GetAutocenPlacement();
  WINDOWPLACEMENT *vppPlace = mWinApp->mMultiTSTasks->GetConditionPlacement();
  WINDOWPLACEMENT *zbgPlace = mWinApp->mParticleTasks->GetZbyGPlacement();
  WINDOWPLACEMENT *navAcqPlace = mWinApp->mNavHelper->GetAcquireDlgPlacement(true);
  WINDOWPLACEMENT *scndLogPlace = mWinApp->GetSecondaryLogPlacement();
  int *macroButtonNumbers = mWinApp->mCameraMacroTools.GetMacroNumbers();
  mWinApp->CopyCurrentToCameraLDP();
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  NavParams *navParams = mWinApp->GetNavParams();
  CookParams *cookParams = mWinApp->GetCookParams();
  CArray<AutocenParams *, AutocenParams *> *autocenArray = 
    mWinApp->mMultiTSTasks->GetAutocenParams();
  CArray<ZbyGParams> *zbgArray = mWinApp->mParticleTasks->GetZbyGcalArray();
  ZbyGParams zbgParam;
  RangeFinderParams *tsrParams = mWinApp->GetTSRangeParams();
  int *tssPanelStates = mWinApp->GetTssPanelStates();
  AutocenParams *acParams, *acParmP;
  LowDoseParams *ldp;
  StateParams *stateP;
  BaseMarkerShift markShft;
  CArray<StateParams *, StateParams *> *stateArray = mWinApp->mNavHelper->GetStateArray();
  int *deNumRepeats = mWinApp->mGainRefMaker->GetDEnumRepeats();
  float *deExposures = mWinApp->mGainRefMaker->GetDEexposureTimes();
  CArray<FrameAliParams, FrameAliParams> *faParamArray =
    mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams faParam;
  BOOL *useGPU4K2Ali = mWinApp->mCamera->GetUseGPUforK2Align();
  CString *fKeyMapping = mWinApp->mMacroProcessor->GetFKeyMapping();
  MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
  HoleFinderParams *hfParams = mWinApp->mNavHelper->GetHoleFinderParams();
  AutoContourParams *contParams = mWinApp->mNavHelper->GetAutocontourParams();
  DriftWaitParams *dwParams = mWinApp->mParticleTasks->GetDriftWaitParams();
  ComaVsISCalib *comaVsIS = mWinApp->mAutoTuning->GetComaVsIScal();
  VppConditionParams *vppParams = mWinApp->mMultiTSTasks->GetVppConditionParams();
  ScreenShotParams *snapParams = mWinApp->GetScreenShotParams();
  NavAlignParams *navAliParm = mWinApp->mNavHelper->GetNavAlignParams();
  DewarVacParams *dewar = mWinApp->mScope->GetDewarVacParams();
  CArray<BaseMarkerShift, BaseMarkerShift> *markerShiftArr =
    mWinApp->mNavHelper->GetMarkerShiftArray();
  MultiGridParams *mgParams = mWinApp->mMultiGridTasks->GetMultiGridParams();

  // Transfer macros from any open editing windows
  for (i = 0; i < MAX_MACROS; i++)
    if (mWinApp->mMacroEditer[i])
      mWinApp->mMacroEditer[i]->TransferMacro(true);
  mWinApp->mMacroProcessor->TransferOneLiners(true);

  mWinApp->SyncNonModalsToMasterParams();

  try {
    // Open the file for writing, 
    mFile = new CStdioFile(strFileName, CFile::modeCreate |
      CFile::modeWrite | CFile::shareDenyWrite);

    mFile->WriteString("SerialEMSettings\n");
    WriteString("SystemPath", mDocWnd->GetSysPathForSettings());
    WriteString("ScriptPackagePath", mDocWnd->GetCurScriptPackPath());
    WriteString("CurrentDirectory", mDocWnd->GetInitialDir());
    oneState = mWinApp->GetScriptToRunAtStart();
    if (!oneState.IsEmpty())
      WriteString("ScriptToRunAtStart", oneState);
    oneState = mWinApp->GetScriptToRunAtEnd();
    if (!oneState.IsEmpty())
      WriteString("ScriptToRunAtEnd", oneState);
    oneState = mWinApp->GetScriptToRunOnIdle();
    if (!oneState.IsEmpty())
      WriteString("ScriptToRunOnIdle", oneState);
    for (i = 0; i < 10; i++) {
      if (!fKeyMapping[i].IsEmpty()) {
        oneState.Format("%d %s", i, (LPCTSTR)fKeyMapping[i]);
        WriteString("FKeyMapping", oneState);
      }
    }
    oneState = mDocWnd->GetBasicModeFile();
    if (!oneState.IsEmpty())
      WriteString("BasicModeFile", oneState);
    WriteInt("LastDPI", mWinApp->GetSystemDPI());

    // Write any consets that are initialized (right != 0)
    for (int iCam = 0; iCam < MAX_CAMERAS; iCam++) {
      for (int iset = 0; iset < NUMBER_OF_USER_CONSETS; iset++) {
        cs = &mConSets[iCam * MAX_CONSETS + iset];
        if (!cs->right)
          continue;
        oneState.Format("CameraControlSet  %d  %d\n", iset, iCam);
        mFile->WriteString(oneState);
        WriteInt("AcquisitionMode", cs->mode);
        WriteInt("Processing", cs->processing);
        WriteInt("DarkReferenceEveryTime", cs->forceDark);
        WriteInt("AverageDarkReference", cs->averageDark);
        WriteInt("TimesToAverageDarkRef", cs->numAverage);
        WriteInt("RemoveXrays", cs->removeXrays);
        WriteInt("Binning", cs->binning);
        WriteInt("ShutteringMode", cs->shuttering);
        WriteInt("Top", cs->top);
        WriteInt("Left", cs->left);
        WriteInt("Right", cs->right);
        WriteInt("Bottom", cs->bottom);
        WriteFloat("ExposureTime", cs->exposure);
        WriteFloat("DriftSettling", cs->drift);
        WriteInt("LineSync", cs->lineSync);
        WriteInt("DynamicFocus", cs->dynamicFocus);
        WriteInt("CorrectDrift", cs->correctDrift);
        WriteInt("K2ReadMode", cs->K2ReadMode);
        WriteInt("SumK2Frames", cs->sumK2Frames);
        WriteInt("DoseFracMode", cs->doseFrac);
        WriteFloat("FrameTime", cs->frameTime);
        WriteInt("AlignFrames", cs->alignFrames);
        WriteInt("SaveFrames", cs->saveFrames);
        WriteInt("UseFrameAlign", cs->useFrameAlign);
        WriteInt("FAParamSetInd", cs->faParamSetInd);
        if (cs->DEsumCount)
          WriteInt("DEsumCount", cs->DEsumCount);
        WriteInt("FilterType", cs->filterType);
        WriteIndexedInts("ChannelIndex", cs->channelIndex, MAX_STEM_CHANNELS);
        oneState.Format("BoostMag %d %d\n", cs->boostMag, cs->magAllShots);
        mFile->WriteString(oneState);
        if (cs->numSkipBefore)
          WriteInt("SkipFramesBefore", cs->numSkipBefore);
        if (cs->numSkipBefore)
          WriteInt("SkipFramesAfter", cs->numSkipAfter);
        OutputVector("SummedFrameList", (int)cs->summedFrameList.size(),
          &cs->summedFrameList, NULL);
        OutputVector("UserFrameFracs", (int)cs->userFrameFractions.size(), NULL,
          &cs->userFrameFractions);
        OutputVector("UserSubframeFracs", (int)cs->userSubframeFractions.size(), NULL,
          &cs->userSubframeFractions);
        mFile->WriteString("EndControlSet\n");
      }

      if (mCamParam[iCam].DE_camType && mCamParam[iCam].DE_FramesPerSec > 0) {
        oneState.Format("DE12FPS %d %f", iCam, mCamParam[iCam].DE_FramesPerSec);
        if (mCamParam[iCam].DE_CountingFPS > 0.) {
          macCopy.Format(" %f", mCamParam[iCam].DE_CountingFPS);
          oneState += macCopy;
        }
        mFile->WriteString(oneState + "\n");
      }
      if (!mCamParam[iCam].dirForFrameSaving.IsEmpty()) {
        oneState.Format("DirForFrameSaving %d %s\n", iCam,
          (LPCTSTR)mCamParam[iCam].dirForFrameSaving);
        mFile->WriteString(oneState);
      }
      if (mCamParam[iCam].useGPUforAlign[0] || mCamParam[iCam].useGPUforAlign[1]) {
        oneState.Format("UseGPUforFrameAlign %d %d %d\n", iCam,
          mCamParam[iCam].useGPUforAlign[0], mCamParam[iCam].useGPUforAlign[1]);
        mFile->WriteString(oneState);
      }
    }

    oneState.Format("FrameNameData %d %d %d %d %d\n", camera->GetFrameNameFormat(),
      camera->GetFrameNumberStart(), camera->GetLastFrameNumberStart(),
      camera->GetLastUsedFrameNumber(), camera->GetDigitsForNumberedFrame());
    mFile->WriteString(oneState);
    macCopy = camera->GetDirForK2Frames();
    if (!macCopy.IsEmpty()) {
      oneState.Format("DirForK2Frames %s\n", (LPCTSTR)macCopy);
      mFile->WriteString(oneState);
    }
    macCopy = camera->GetDirForDEFrames();
    if (!macCopy.IsEmpty()) {
      oneState.Format("DirForDEFrames %s\n", (LPCTSTR)macCopy);
      mFile->WriteString(oneState);
    }
    macCopy = camera->GetDirForFalconFrames();
    if (macCopy.IsEmpty())
      macCopy = "EmptyPath";
    oneState.Format("DirForFalconFrames %s\n", (LPCTSTR)macCopy);
    mFile->WriteString(oneState);
    macCopy = camera->GetFrameBaseName();
    if (!macCopy.IsEmpty()) {
      oneState.Format("FrameBaseName %s\n", (LPCTSTR)macCopy);
      mFile->WriteString(oneState);
    }
    macCopy = camera->GetNumberedFrameFolder();
    if (!macCopy.IsEmpty()) {
      oneState.Format("NumberedFrameFolder %s\n", (LPCTSTR)macCopy);
      mFile->WriteString(oneState);
    }
    macCopy = camera->GetNumberedFramePrefix();
    if (!macCopy.IsEmpty()) {
      oneState.Format("NumberedFramePrefix %s\n", (LPCTSTR)macCopy);
      mFile->WriteString(oneState);
    }
    if (mWinApp->mDEToolDlg.GetFormatForAutoSave() >= 0)
      WriteInt("DEAutosaveFormat", mWinApp->mDEToolDlg.GetFormatForAutoSave());
    mWinApp->GetDisplayTruncation(pctLo, pctHi);
    WriteFloat("PercentDisplayTruncationLo", pctLo);
    WriteFloat("PercentDisplayTruncationHi", pctHi);
    WriteInt("ProtectRecordImages", mBufferManager->GetConfirmBeforeDestroy(3));
    oneState.Format("MontageUseContinuous %d %f\n", montParam->useContinuousMode ? 1 : 0,
      montParam->continDelayFactor);
    mFile->WriteString(oneState);
    oneState.Format("MontageNoDriftCorr %d %d\n", montParam->noDriftCorr ? 1 : 0,
      montParam->noHQDriftCorr ? 1 : 0);
    mFile->WriteString(oneState);
    oneState.Format("MontageFilterOptions %d %d %d %d\n",
      mWinApp->mMontageController->GetUseFilterSet2() ? 1 : 0,
      mWinApp->mMontageController->GetUseSet2InLD() ? 1 : 0,
      mWinApp->mMontageController->GetDivFilterSet1By2() ? 1 : 0,
      mWinApp->mMontageController->GetDivFilterSet2By2() ? 1 : 0);
    mFile->WriteString(oneState);
    WriteInt("TIFFcompression", fileOpt->compression);
    WriteInt("HDFcompression", fileOpt->hdfCompression);
    WriteFloat("FileOptionsPctTruncLo", fileOpt->pctTruncLo);
    WriteFloat("FileOptionsPctTruncHi", fileOpt->pctTruncHi);
    oneState.Format("SingleFileOptions %d %d %f %f\n", otherFileOpt->fileType,
      otherFileOpt->compression, otherFileOpt->pctTruncLo, otherFileOpt->pctTruncHi);
    mFile->WriteString(oneState);
    oneState.Format("AutofocusEucenAbsParams %f %f %f %f %d %d\n",
      focusMan->GetEucenMinAbsFocus(), focusMan->GetEucenMaxAbsFocus(),
      focusMan->GetEucenMinDefocus(), focusMan->GetEucenMaxDefocus(),
      focusMan->GetUseEucenAbsLimits() ? 1 : 0, focusMan->GetTestOffsetEucenAbs() ? 1 : 0);
    mFile->WriteString(oneState);
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
#define SET_TEST_SECT15
#include "SettingsTests.h"
#undef SET_TEST_SECT15
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
#define SET_TEST_SECT25
#include "SettingsTests.h"
#undef SET_TEST_SECT25
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3

    oneState.Format("AutoBacklashNewMap %d %f\n",
      mWinApp->mNavHelper->GetAutoBacklashNewMap(),
      mWinApp->mNavHelper->GetAutoBacklashMinField());
    mFile->WriteString(oneState);
    WriteInt("NavCollapseGroups", B3DCHOICE(mWinApp->mNavigator, 
      mWinApp->mNavigator->m_bCollapseGroups, mWinApp->mNavHelper->GetCollapseGroups()) ?
      1 : 0);
    oneState.Format("MultiShotParams %f %f %d %d %d %d %d %f %d %d %d %d %f %f %f %f %f "
      "%d %d %d %d %d %d %d %f %f %f %f\n", msParams->beamDiam, msParams->spokeRad[0],
      msParams->numShots[0], msParams->doCenter, msParams->doEarlyReturn,
      msParams->numEarlyFrames, msParams->saveRecord ? 1 : 0, msParams->extraDelay,
      msParams->useIllumArea ? 1 : 0, msParams->adjustBeamTilt ? 1 : 0,
      msParams->inHoleOrMultiHole, msParams->useCustomHoles ? 1 : 0,
      msParams->holeDelayFactor, msParams->holeISXspacing[0], msParams->holeISYspacing[0],
      msParams->holeISXspacing[1], msParams->holeISYspacing[1],
      msParams->numHoles[0], msParams->numHoles[1], msParams->holeMagIndex[0],
      msParams->customMagIndex, msParams->skipCornersOf3x3 ? 1 : 0,
      msParams->doSecondRing ? 1 : 0, msParams->numShots[1], msParams->spokeRad[1],
      msParams->tiltOfHoleArray[0], msParams->tiltOfCustomHoles, msParams->holeFinderAngle);
    mFile->WriteString(oneState);
    oneState.Format("MultiHexParams %d %d %f %f %f %f %f %f %d %f\n",
      msParams->doHexArray ? 1 : 0, msParams->numHexRings, msParams->hexISXspacing[0],
      msParams->hexISYspacing[0], msParams->hexISXspacing[1], msParams->hexISYspacing[1],
      msParams->hexISXspacing[2], msParams->hexISYspacing[2], msParams->holeMagIndex[1],
      msParams->tiltOfHoleArray[1]);
    mFile->WriteString(oneState);
    oneState.Format("StepAdjustParams %d %d %d %d %d %d %d %d %f %f %d %f\n",
      msParams->stepAdjLDarea, msParams->stepAdjWhichMag, msParams->stepAdjOtherMag,
      msParams->stepAdjSetDefOff ? 1 : 0, msParams->stepAdjDefOffset,
      msParams->stepAdjTakeImage ? 1 : 0, msParams->doAutoAdjustment ? 1 : 0,
      msParams->autoAdjMethod, msParams->autoAdjHoleSize, msParams->autoAdjLimitFrac,
      msParams->stepAdjSetPrevExp ? 1 : 0, msParams->stepAdjPrevExp);
    mFile->WriteString(oneState);
    oneState.Format("HoleAdjustXform %d %d %d %d %d %f %f %f %f %d\n",
      msParams->origMagOfArray[0], msParams->origMagOfArray[1], msParams->origMagOfCustom,
      msParams->xformFromMag, msParams->xformToMag, msParams->adjustingXform.xpx,
      msParams->adjustingXform.xpy, msParams->adjustingXform.ypx, 
      msParams->adjustingXform.ypy, msParams->xformMinuteTime);
    mFile->WriteString(oneState);
    if (msParams->customHoleX.size()) {
      OutputVector("CustomHoleX", (int)msParams->customHoleX.size(), NULL,
        &msParams->customHoleX);
      OutputVector("CustomHoleY", (int)msParams->customHoleY.size(), NULL,
        &msParams->customHoleY);
    }
    oneState.Format("HoleFinderParams %f %f %d %f %f %f %f %d %d %d %d %f %f %f %d\n", 
      hfParams->spacing, hfParams->diameter, hfParams->useBoundary ? 1 : 0, 
      hfParams->lowerMeanCutoff, hfParams->upperMeanCutoff, hfParams->SDcutoff, 
      hfParams->blackFracCutoff, hfParams->showExcluded ? 1 : 0, hfParams->layoutType, 
      hfParams->bracketLast ? 1 : 0, hfParams->hexagonalArray ? 1 : 0, 
      hfParams->hexSpacing, hfParams->hexDiameter, hfParams->edgeDistCutoff,
      hfParams->useHexDiagonals);
    mFile->WriteString(oneState);
    if (hfParams->sigmas.size())
      OutputVector("HoleFiltSigmas", (int)hfParams->sigmas.size(), NULL,
        &hfParams->sigmas);
    if (hfParams->thresholds.size())
      OutputVector("HoleEdgeThresholds", (int)hfParams->thresholds.size(), NULL,
        &hfParams->thresholds);
    oneState.Format("HoleCombinerParams %d %d %d %d %d %d\n", 
      mWinApp->mNavHelper->GetMHCcombineType(),
      mWinApp->mNavHelper->GetMHCenableMultiDisplay() ? 1 : 0, 
      mWinApp->mNavHelper->GetMHCturnOffOutsidePoly() ? 1 : 0,
      mWinApp->mNavHelper->GetMHCdelOrTurnOffIfFew(), 
      mWinApp->mNavHelper->GetMHCthreshNumHoles(),
      mWinApp->mNavHelper->GetMHCskipAveragingPos() ? 1 : 0);
    mFile->WriteString(oneState);
    oneState.Format("AutoContParams %d %f %d %f %f %f %f %d %d %d %f %f %f %f %f %f %d\n",
      contParams->targetSizePixels, contParams->targetPixSizeUm,
      contParams->usePixSize ? 1 : 0, contParams->minSize, contParams->maxSize,
      contParams->relThreshold, contParams->absThreshold, contParams->useAbsThresh ? 1 : 0,
      contParams->numGroups, contParams->groupByMean ? 1 : 0, contParams->lowerMeanCutoff,
      contParams->upperMeanCutoff, contParams->minSizeCutoff, contParams->SDcutoff,
      contParams->irregularCutoff, contParams->borderDistCutoff, 
      contParams->useCurrentPolygon);
    mFile->WriteString(oneState);
    oneState.Format("DriftWaitParams %d %f %d %f %f %d %d %f %d %d %d %f\n", 
      dwParams->measureType, dwParams->driftRate, dwParams->useAngstroms ? 1 :0, 
      dwParams->interval, dwParams->maxWaitTime, dwParams->failureAction,
      dwParams->setTrialParams ? 1 : 0, dwParams->exposure, dwParams->binning, 
      dwParams->changeIS ? 1 : 0, dwParams->usePriorAutofocus ? 1 : 0, 
      dwParams->priorAutofocusRate);
    mFile->WriteString(oneState);

    for (i = 0; i < 2; i++)
      WriteNavAcqParams(i, mWinApp->GetNavAcqParams(i),
      mWinApp->mNavHelper->GetAcqActions(i),
      mWinApp->mNavHelper->GetAcqActCurrentOrder(i), false);
    oneState.Format("NavAlignParams %d %f %d %f %d %f %f %f %d\n", 
      navAliParm->loadAndKeepBuf, navAliParm->maxAlignShift, navAliParm->maxNumResetIS,
      navAliParm->resetISthresh, navAliParm->leaveISatZero ? 1 : 0, 
      navAliParm->scaledAliMaxRot, navAliParm->scaledAliPctChg, navAliParm->scaledAliExtraFOV,
      navAliParm->scaledAliLoadBuf);
    mFile->WriteString(oneState);
    mFile->WriteString("NavAliMapLabel " + navAliParm->templateLabel + "\n");

    for (i = 0; i < markerShiftArr->GetSize(); i++) {
      markShft = markerShiftArr->GetAt(i);
      oneState.Format("MarkerShift %d %d %f %f\n", markShft.fromMagInd, markShft.toMagInd,
        markShft.shiftX, markShft.shiftY);
      mFile->WriteString(oneState);
    }

    oneState.Format("DewarVacParams %d %d %d %d %d %d %f %d %f %d %f %f %d\n",
      dewar->checkPVP ? 1 : 0, dewar->runBufferCycle ? 1 : 0,
      dewar->bufferTimeMin, dewar->runAutoloaderCycle ? 1 : 0, dewar->autoloaderTimeMin,
      dewar->refillDewars ? 1 : 0, dewar->dewarTimeHours, dewar->checkDewars ? 1 : 0,
      dewar->pauseBeforeMin, dewar->startRefill ? 1 : 0, dewar->startIntervalMin,
      dewar->postFillWaitMin, dewar->doChecksBeforeTask ? 1 : 0);
    mFile->WriteString(oneState);

    oneState.Format("CookerParams %d %d %f %d %d %f %d %d %f -999 -999\n",
      cookParams->magIndex, cookParams->spotSize, cookParams->intensity, 
      cookParams->targetDose, cookParams->timeInstead ? 1 : 0, cookParams->minutes,
      cookParams->trackImage ? 1 : 0, 
      cookParams->cookAtTilt ? 1 : 0, cookParams->tiltAngle);
    mFile->WriteString(oneState);
    oneState.Format("VppConditionParams %d %d %f %d %d %d %d %d %d %d %d %d -999 -999\n",
      vppParams->magIndex, vppParams->spotSize, vppParams->intensity, vppParams->alpha,
      vppParams->probeMode, vppParams->seconds, vppParams->nanoCoulombs,
      vppParams->timeInstead ? 1 : 0, vppParams->whichSettings,
      vppParams->useNearestNav ? 1 : 0, vppParams->useNavNote ? 1 : 0, 
      vppParams->postMoveDelay);
    mFile->WriteString(oneState);
    mFile->WriteString("VppCondNavText " + vppParams->navText + "\n");
    oneState.Format("SnapshotParams %d %f %d %f %d %d %d %d -999 -999\n",
      snapParams->imageScaleType, snapParams->imageScaling,
      snapParams->ifScaleSizes ? 1 : 0, snapParams->sizeScaling, snapParams->fileType,
      snapParams->compression, snapParams->jpegQuality, snapParams->skipOverlays);
    mFile->WriteString(oneState);
    oneState.Format("MultiGridParams %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d"
      " %d %d %d %d %d %d %d %d %d %d\n", mgParams->appendNames ? 1 : 0,
      mgParams->useSubdirectory ? 1 : 0, mgParams->setLMMstate ? 1 : 0,
      mgParams->LMMstateType, mgParams->removeObjectiveAp ? 1 : 0,
      mgParams->setCondenserAp ? 1 : 0, mgParams->condenserApSize, mgParams->LMMmontType,
      mgParams->LMMnumXpieces, mgParams->LMMnumYpieces, mgParams->setLMMoverlap ? 1 : 0,
      mgParams->LMMoverlapPct, mgParams->autocontour ? 1 : 0, 
      mgParams->acquireMMMs ? 1 : 0,
      mgParams->MMMstateType, mgParams->MMMimageType, mgParams->acquireLMMs ? 1 : 0,
      mgParams->runFinalAcq ? 1 : 0, mgParams->MMMnumXpieces, mgParams->MMMnumYpieces,
      mgParams->framesUnderSession ? 1 : 0, mgParams->runMacroAfterLMM ? 1 : 0,
      mgParams->macroToRun, mgParams->refineAfterRealign ? 1 : 0, 
      mgParams->refineImageType, mgParams->C1orC2condenserAp, mgParams->msVectorSource);
    mFile->WriteString(oneState);
    oneState.Format("MGLMMstate %d %s\n", mgParams->LMMstateNum,
      (LPCTSTR)mgParams->LMMstateName);
    mFile->WriteString(oneState);
    oneState.Format("MGREFstate %d %s\n", mgParams->refineStateNum,
      (LPCTSTR)mgParams->refineStateName);
    mFile->WriteString(oneState);
    for (i = 0; i < 4; i++) {
      if (mgParams->MMMstateNums[i] >= 0) {
        oneState.Format("MGMMMstate%d %d %s\n", i + 1, mgParams->MMMstateNums[i],
          (LPCTSTR)mgParams->MMMstateNames[i]);
        mFile->WriteString(oneState);
      }
      if (mgParams->finalStateNums[i] >= 0) {
        oneState.Format("MGfinalState%d %d %s\n", i + 1, mgParams->finalStateNums[i],
          (LPCTSTR)mgParams->finalStateNames[i]);
        mFile->WriteString(oneState);
      }
    }
    oneState = mWinApp->mMultiGridTasks->GetLastSessionFile();
    if (!oneState.IsEmpty())
      mFile->WriteString("MGSessionFile " + oneState + "\n");
    if (!navParams->stockFile.IsEmpty()) {
      oneState.Format("NavigatorStockFile %s\n", navParams->stockFile);
      mFile->WriteString(oneState);
    }
    for (i = 0; i < autocenArray->GetSize(); i++) {
      acParams = autocenArray->GetAt(i);
      if (acParams->intensity >= 0. || acParams->camera == -1) {
        acParmP = mWinApp->mMultiTSTasks->LookupAutocenParams(acParams->camera, 
          acParams->magIndex, acParams->spotSize, acParams->probeMode, 
          acParams->intensity, j);
        if (j == i) {
          oneState.Format("AutocenterParams %d %d %d %f %d %f %d %d %d %f %f %f -999\n", 
            acParams->camera, acParams->magIndex, acParams->spotSize, acParams->intensity,
            acParams->binning, acParams->exposure, acParams->useCentroid ? 1 : 0, 
            acParams->probeMode, acParams->shiftBeamForCen ? 1 : 0, acParams->beamShiftUm,
            acParams->addedShiftX, acParams->addedShiftY);
          mFile->WriteString(oneState);
        }
      }
    }
    for (i = 0; i < zbgArray->GetSize(); i++) {
      zbgParam = zbgArray->GetAt(i);
      oneState.Format("ZbyGParams %d %d %f %d %d %d %f %f %f %f -999\n",
        zbgParam.lowDoseArea, zbgParam.magIndex, zbgParam.intensity, zbgParam.spotSize,
        zbgParam.probeOrAlpha, zbgParam.camera, zbgParam.focusOffset, zbgParam.beamTilt,
        zbgParam.targetDefocus, zbgParam.standardFocus);
      mFile->WriteString(oneState);
    }
    if (comaVsIS->magInd >= 0) {
      oneState.Format("ComaVsISCal %d %d %d %d %d %f %f %f %f %f %f %f %f %f\n",
        comaVsIS->magInd, comaVsIS->spotSize, comaVsIS->probeMode, comaVsIS->alpha, 
        comaVsIS->aperture, comaVsIS->intensity, comaVsIS->matrix.xpx, 
        comaVsIS->matrix.xpy, comaVsIS->matrix.ypx, comaVsIS->matrix.ypy, 
        comaVsIS->astigMat.xpx, comaVsIS->astigMat.xpy, comaVsIS->astigMat.ypx, 
        comaVsIS->astigMat.ypy);
      mFile->WriteString(oneState);
    }
    for (i = 0; i < 2; i++) {
      oneState.Format("RangeFinderParams %d %d %d %d %d %f %f %f %d -999 -999\n", i,
        tsrParams[i].eucentricity ? 1:0, tsrParams[i].walkup ? 1:0, 
        tsrParams[i].autofocus ? 1:0, tsrParams[i].imageType, tsrParams[i].startAngle,
        tsrParams[i].endAngle, tsrParams[i].angleInc, tsrParams[i].direction);
      mFile->WriteString(oneState);
    }
    oneState.Format("ImportOverlayChannels %s\n", navParams->overlayChannels);
    mFile->WriteString(oneState);
    oneState.Format("ProcessOverlayChannels %s\n", 
      mWinApp->mProcessImage->GetOverlayChannels());
    mFile->WriteString(oneState);
    oneState.Format("CtffindParams %d %d %d %d %f %d %d %d %d\n", 
      mWinApp->mProcessImage->GetCtffindOnClick() ? 1 : 0,
      mWinApp->mProcessImage->GetSlowerCtfFit(),
      mWinApp->mProcessImage->GetExtraCtfStats(),
      mWinApp->mProcessImage->GetDrawExtraCtfRings(), 
      mWinApp->mProcessImage->GetCtfFitFocusRangeFac(),
      mWinApp->mProcessImage->GetCtfFindPhaseOnClick(),
      mWinApp->mProcessImage->GetCtfFixAstigForPhase(),
      mWinApp->mProcessImage->GetCtfMinPhase(), mWinApp->mProcessImage->GetCtfMaxPhase());
    mFile->WriteString(oneState);

    // Save states of tool windows, but set montage closed
    for (i = 0; i < MAX_TOOL_DLGS; i++) {
      j = mWinApp->LookupToolDlgIndex(i);
      int state = j >= 0 ? dlgTable[j].state : initialDlgState[i];
      if (i == MONTAGE_DIALOG_INDEX)
        state &= ~1;
      oneState.Format(" %d", state);
      states += oneState;
    }
    states += "\n";
    mFile->WriteString(states);

    for (i = 0; i < MAX_TOOL_DLGS; i++) {
      j = mWinApp->LookupToolDlgIndex(i);
      int state = j >= 0 ? dlgTable[j].state : initialDlgState[i];
      if (state & TOOL_FLOATDOCK) {
        RECT dlgRect;
        if (j >= 0) {
          dlgTable[j].pDialog->GetWindowPlacement(&winPlace);
          dlgRect = winPlace.rcNormalPosition;
        } else {
          dlgRect = dlgPlacements[i];
        }
        oneState.Format("ToolDialogPlacement %d %d %d %d %d\n", i, 
          dlgRect.left, dlgRect.top, dlgRect.right, dlgRect.bottom);
        mFile->WriteString(oneState);
      }
    }

    oneState.Format("MacroButtonNumbers %d %d %d\n", macroButtonNumbers[0],
      macroButtonNumbers[1], macroButtonNumbers[2]);
    mFile->WriteString(oneState);
    oneState = "MoreScriptButtons ";
    for (i = 3; i < NUM_SPINNER_MACROS; i++) {
      macCopy.Format(" %d", macroButtonNumbers[i]);
      oneState += macCopy;
    }
    mFile->WriteString(oneState + "\n");

    oneState.Format("RemoteControlParams %d %f %f %d %d %d %d\n", 
      mWinApp->GetShowRemoteControl() ? 1 : 0, mWinApp->mRemoteControl.GetBeamIncrement(), 
      mWinApp->mRemoteControl.GetIntensityIncrement(), 
      mWinApp->mRemoteControl.m_bMagIntensity ? 1: 0,
      mWinApp->mRemoteControl.GetFocusIncrementIndex(), 
      0, mWinApp->mRemoteControl.GetStageIncIndex());
    mFile->WriteString(oneState);

    // Get window placement and write it out
    if (mWinApp->GetWindowPlacement(&winPlace)) {
      if (winPlace.showCmd == SW_SHOWMINIMIZED)
        winPlace.showCmd = SW_SHOWNORMAL;
      WriteInt("WindowPlacement", winPlace.showCmd);
      oneState.Format("%d %d %d %d %d %d %d %d\n", 
        winPlace.ptMaxPosition.x, winPlace.ptMaxPosition.y,
        winPlace.ptMinPosition.x, winPlace.ptMinPosition.y,
        winPlace.rcNormalPosition.left, winPlace.rcNormalPosition.top,
        winPlace.rcNormalPosition.right, winPlace.rcNormalPosition.bottom);
      mFile->WriteString(oneState);
    }

    // Get Log window placement and write it out
    if (mWinApp->mLogWindow)
      mWinApp->mLogWindow->GetWindowPlacement(logPlace);
    WritePlacement("LogPlacement", mWinApp->mLogWindow ? 1 : 0, logPlace);
    if (scndLogPlace->rcNormalPosition.right != NO_PLACEMENT)
      WritePlacement("SecondaryLogPlacement", 1, scndLogPlace);

    // Get Navigator window placement and write it out
    if (mWinApp->mNavigator)
      mWinApp->mNavigator->GetWindowPlacement(navPlace);
    WritePlacement("NavigatorPlacement", mWinApp->mNavigator ? 1 : 0, navPlace);

    // Write screen meter placement
    WritePlacement("MeterPlacement", 0, meterPlace);
    WritePlacement("DosePlacement", 0, dosePlace);
    WritePlacement("StatePlacement", (mWinApp->GetOpenStateWithNav() || 
      (mWinApp->mNavigator && mWinApp->mNavHelper->mStateDlg)) ? 1 : 0, statePlace);
    WritePlacement("RotAlignPlacement", 0, rotAlignPlace);
    WritePlacement("MultiShotPlacement", 0, multiShotPlace);
    WritePlacement("ReadDlgPlacement", 0, readPlace);
    WritePlacement("StageToolPlacement", 0, stageToolPlace);
    WritePlacement("CtffindPlacement", 0, ctffindPlace);
    WritePlacement("AutocenPlacement", 0, autocenPlace);
    WritePlacement("VppCondPlacement", 0, vppPlace);
    WritePlacement("ZbyGSetupPlacement", 0, zbgPlace);
    WritePlacement("NavAcqPlacement", 0, navAcqPlace);
    WritePlacement("SnapshotPlacement", 0, mWinApp->GetScreenShotPlacement());
    WritePlacement("HoleFinderPlacement", 0, 
      mWinApp->mNavHelper->GetHoleFinderPlacement());
    WritePlacement("MultiCombinerPlacement", 0, 
      mWinApp->mNavHelper->GetMultiCombinerPlacement());
    WritePlacement("AutoContPlacement", 0, 
      mWinApp->mNavHelper->GetAutoContDlgPlacement());
    WritePlacement("MultiGridPlacement", 0, mWinApp->mNavHelper->GetMultiGridPlacement());
    WritePlacement("MacroToolPlacement", mWinApp->mMacroToolbar ? 1 : 0, toolPlace);
    WritePlacement("OneLinePlacement", mWinApp->mMacroProcessor->mOneLineScript ? 1 : 0, 
      oneLinePlace);
    for (i = 0; i < MAX_MACROS; i++) {
      oneState.Format("OneEditerPlacement %d", i);
      WritePlacement((LPCTSTR)oneState, mWinApp->mMacroEditer[i] != NULL ? 1 : 0,
        mWinApp->mMacroProcessor->FindEditerPlacement(i));
    }
    oneState.Format("MacroToolbarButtons %d %d\n", 
      mWinApp->mMacroProcessor->GetNumToolButtons(), 
      mWinApp->mMacroProcessor->GetToolButHeight());
    mFile->WriteString(oneState);

    // Save stored states BEFORE low dose params
    for (i = 0; i < stateArray->GetSize(); i++) {
      stateP = stateArray->GetAt(i);
      WriteStateToString(stateP, oneState);
      oneState = "StateParameters " + oneState + "\n";
      mFile->WriteString(oneState);
      if (!stateP->name.IsEmpty()) {
        oneState.Format("StateName %d %s\n", i, (LPCTSTR)stateP->name);
        mFile->WriteString(oneState);
      }
      if (stateP->focusAxisPos > EXTRA_VALUE_TEST) {
        oneState.Format("StateParams2 %d %f %d %d %d %d\n", i, stateP->focusAxisPos,
          stateP->rotateAxis ? 1 : 0, stateP->axisRotation, stateP->focusXoffset,
          stateP->focusYoffset);
        mFile->WriteString(oneState);
      }
    }

    // Save low dose params, including those in states
    for (j = 0; j < 4; j++) {
      for (i = 0; i < (j < 3 ? mNumLDSets : stateArray->GetSize()); i++) {
        if (j < 3) {
          ldp = &mLowDoseParams[j * MAX_LOWDOSE_SETS + i];
          ldi = i;
          ldj = j;
        } else {
          stateP = stateArray->GetAt(i);
          if (!stateP->lowDose)
            continue;
          ldp = &stateP->ldParams;
          ldi = -1 - i;
          ldj = 0;
        }

        WriteLowDoseToString(ldp, ldi, ldj, oneState);
        oneState = "LowDoseParameters " + oneState + "\n";
        mFile->WriteString(oneState);
      }
    }
    oneState.Format("LowDoseViewShift %f %f\n", mWinApp->mLowDoseDlg.mViewShiftX[0], 
      mWinApp->mLowDoseDlg.mViewShiftY[0]); 
    mFile->WriteString(oneState);
    oneState.Format("LDSearchShift %f %f\n", mWinApp->mLowDoseDlg.mViewShiftX[1], 
      mWinApp->mLowDoseDlg.mViewShiftY[1]); 
    mFile->WriteString(oneState);
    pctLo = mWinApp->mComplexTasks->GetRSRAUserCriterion();
    if (pctLo >= 0.)
      WriteFloat("ResetRealignIterationCriterion", pctLo);

    // Save Tilt Series Params
    oneState.Format("TiltSeriesLowMagIndex %d %d %d\n", mTSParam->lowMagIndex[0], 
      mTSParam->lowMagIndex[1], mTSParam->lowMagIndex[2]);
    mFile->WriteString(oneState);
    if (mTSParam->numExtraFocus) {
      oneState = EntryListToString(1, 2, mTSParam->numExtraFocus, NULL, mTSParam->extraFocus);
      WriteString("TiltSeriesFocusSeries", oneState);
    }
    if (mTSParam->numExtraExposures) {
      oneState = EntryListToString(1, 2, mTSParam->numExtraExposures, NULL, 
        mTSParam->extraExposures);
      WriteString("TiltSeriesExtraExposure", oneState);
    }
    if (mTSParam->numExtraFilter) {
      oneState = EntryListToString(2, 2, mTSParam->numExtraFilter, mTSParam->extraSlits, 
        mTSParam->extraLosses);
      WriteString("TiltSeriesFilterSeries", oneState);
    }
    if (mTSParam->numExtraChannels) {
      oneState = EntryListToString(3, 2, mTSParam->numExtraChannels, 
        mTSParam->extraChannels, NULL);
      WriteString("TiltSeriesExtraChannels", oneState);
    }
    if (!mTSParam->extraFileSuffixes.IsEmpty()) {
      WriteString("TSExtraSuffixes", mTSParam->extraFileSuffixes);
    }
    for (i = 0; i < mTSParam->numVaryItems; i++) {
      oneState.Format("TiltSeriesVariation %f %d %d %d %f\n", mTSParam->varyArray[i].angle,
        mTSParam->varyArray[i].plusMinus ? 1 : 0, mTSParam->varyArray[i].linear ? 1 : 0, 
        mTSParam->varyArray[i].type, mTSParam->varyArray[i].value);
      mFile->WriteString(oneState);
    }
    oneState.Format("%d %f %d %d %d", mTSParam->doBidirectional ? 1 : 0, 
      mTSParam->bidirAngle, mTSParam->anchorBidirWithView ? 1 : 0, 
      mTSParam->walkBackForBidir ? 1 : 0, mTSParam->retainBidirAnchor ? 1 : 0);
    WriteString("TiltSeriesBidirParams", oneState);
    oneState.Format("%d %d %d %f %d %d %d %f %d %d %f %d", mTSParam->doDoseSymmetric ? 
      1 : 0, mTSParam->dosymBaseGroupSize, mTSParam->dosymIncreaseGroups ? 1 : 0,
      mTSParam->dosymIncStartAngle, mTSParam->dosymGroupIncAmount, 
      mTSParam->dosymGroupIncInterval, mTSParam->dosymDoRunToEnd ? 1 : 0, 
      mTSParam->dosymRunToEndAngle, mTSParam->dosymAnchorIfRunToEnd ? 1 : 0,
      mTSParam->dosymMinUniForAnchor, mTSParam->dosymStartingISLimit, 
      mTSParam->dosymSkipBacklash);
    WriteString("TiltSeriesDoseSymParams", oneState);
    WriteIndexedInts("TiltSeriesBDAnchorMags", mTSParam->bidirAnchorMagInd, 6);
    oneState.Format("%d %d %d", mWinApp->mTSController->GetRunMacroInTS() ? 1 : 0,
      mWinApp->mTSController->GetMacroToRun(), 
      mWinApp->mTSController->GetStepAfterMacro());
    WriteString("TiltSeriesRunMacro", oneState);
    oneState.Format("%d %f", mWinApp->mTSController->GetTermOnHighExposure() ? 1 : 0, 
      mWinApp->mTSController->GetMaxExposureIncrease());
    WriteString("TSTermOnHighExposure", oneState);
    oneState.Format("%d %d %d", mWinApp->mTSController->GetEndOnHighDimImage() ? 1 : 0,
      mWinApp->mTSController->GetDimEndsAbsAngle(), 
      mWinApp->mTSController->GetDimEndsAngleDiff());
    WriteString("TiltSeriesBidirDimPolicy", oneState);
    oneState.Format("CloseValvesDuringMessageBox %d %f\n", 
      mWinApp->mTSController->GetMessageBoxCloseValves() ? 1 : 0,
      mWinApp->mTSController->GetMessageBoxValveTime());
    mFile->WriteString(oneState);
    oneState = mWinApp->mMailer->GetSendTo();
    if (!oneState.IsEmpty())
      WriteString("EmailAddress", oneState);
    WriteIndexedInts("TSSetupPanelStates", tssPanelStates, NUM_TSS_PANELS - 1);
    //WriteFloat("AddedSTEMrotation", mWinApp->GetAddedSTEMrotation());
    oneState.Format("NonGIFMatchPixelIntensity %d %d\n", 
      mWinApp->GetNonGIFMatchPixel(), mWinApp->GetNonGIFMatchIntensity());
    mFile->WriteString(oneState);

    for (i = 0; i < MAX_CAMERAS; i++) {
      if (mCamParam[i].gainRefBinning) {
        oneState.Format("GainReferenceTarget %d %d %d %d %d %d %d %d\n", i, 
          mCamParam[i].gainRefBinning, mCamParam[i].gainRefTarget, 
          mCamParam[i].gainRefFrames, mCamParam[i].gainRefAverageDark,
          mCamParam[i].gainRefNumDarkAvg, mCamParam[i].TSAverageDark, 
          mCamParam[i].gainRefSaveRaw);
        mFile->WriteString(oneState);
      }
      oneState.Format("ImageXrayCriteria %d %f %f %d\n", i, mCamParam[i].imageXRayAbsCrit,
        mCamParam[i].imageXRayNumSDCrit, mCamParam[i].imageXRayBothCrit);
      mFile->WriteString(oneState);
    }
    WriteIndexedFloats("GridMapLimits", gridLim, 4);
    WriteIndexedInts("DErefNumRepeats", deNumRepeats, MAX_DE_REF_TYPES);
    WriteIndexedFloats("DErefExposures", deExposures, MAX_DE_REF_TYPES);
    oneState.Format("DElastRefParams %d %d %d %d\n", 
      mWinApp->mGainRefMaker->GetDElastProcessType(),
      mWinApp->mGainRefMaker->GetDElastReferenceType(),
      mWinApp->mGainRefMaker->GetDEuseHardwareBin(),
      mWinApp->mGainRefMaker->GetDEuseHardwareROI());
    mFile->WriteString(oneState);

    for (i = 0; i < (int)faParamArray->GetSize(); i++) {
      faParam = faParamArray->GetAt(i);
      oneState.Format("FrameAlignParams1 %d %d %d %d %f %f %f %f %d %f %f %d %d %d %d %d"
        " %d %d\n", i, faParam.strategy, faParam.aliBinning, faParam.numAllVsAll, 
        faParam.rad2Filt1, faParam.rad2Filt2, faParam.rad2Filt3, faParam.rad2Filt4, 
        faParam.hybridShifts ? 1 : 0, faParam.sigmaRatio, faParam.refRadius2, 
        faParam.doRefine ? 1 : 0, faParam.refineIter, faParam.useGroups ? 1 : 0, 
        faParam.groupSize, faParam.doSmooth ? 1 : 0, faParam.smoothThresh,
        faParam.shiftLimit);
      mFile->WriteString(oneState);
      oneState.Format("FrameAlignParams2 %d %d %f %d %f %d %d %d %d %d %d %d %d %d %d "
        "%d\n", i, faParam.truncate ? 1 : 0, faParam.truncLimit, faParam.antialiasType, 
        faParam.stopIterBelow, faParam.groupRefine ? 1 : 0, faParam.keepPrecision ? 1 : 0,
        faParam.outputFloatSums ? 1 : 0, faParam.alignSubset ? 1 : 0, faParam.subsetStart,
        faParam.subsetEnd, faParam.sizeRestriction, faParam.whereRestriction,
        faParam.binToTarget ? 1 : 0, faParam.targetSize, faParam.EERsuperRes);
      mFile->WriteString(oneState);
      oneState.Format("FrameAliSetName %d %s\n", i, (LPCTSTR)faParam.name);
      mFile->WriteString(oneState);
    }
    oneState.Format("FrameAliGlobals %d %d %d %d %d\n", useGPU4K2Ali[0] ? 1 : 0, 
      useGPU4K2Ali[1] ? 1 : 0, useGPU4K2Ali[2] ? 1 : 0, 
      mWinApp->GetFrameAlignMoreOpen() ? 1 : 0,
      mWinApp->mCamera->GetAlignWholeSeriesInIMOD() ? 1 : 0); 
    mFile->WriteString(oneState);
    oneState.Format("FalconAliUseGPU %d %d\n", mWinApp->mFalconHelper->GetUseGpuForAlign(
      0), mWinApp->mFalconHelper->GetUseGpuForAlign(1));
    mFile->WriteString(oneState);
    oneState = mWinApp->mCamera->GetAlignFramesComPath();
    if (!oneState.IsEmpty())
      WriteString("AlignframesComPath", oneState);

    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing settings to file " + strFileName;
    AfxMessageBox(message, MB_EXCLAME);
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }

  oneState = mDocWnd->GetCurScriptPackPath();
  if (oneState.IsEmpty()) {
    UtilSplitExtension(strFileName, oneState, macCopy);
    oneState += "-scripts.txt";
    mDocWnd->SetCurScriptPackPath(oneState);
  }
  WriteMacrosToFile(oneState, MAX_MACROS + MAX_ONE_LINE_SCRIPTS);

}
#undef INT_SETT_GETSET
#undef BOOL_SETT_GETSET
#undef FLOAT_SETT_GETSET
#undef DOUBLE_SETT_GETSET
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN

// Read macros from a file with the given name
int CParameterIO::ReadMacrosFromFile(CString &filename, const CString &curSettings,
  int maxMacros, bool printMess)
{
  CString strLine, setPath, scriptTail;
  CString strItems[MAX_TOKENS];
  CFileStatus status;
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  int retval = 0, numLoaded = 0;
  int err;
  bool loadMess = curSettings.IsEmpty();
  if (!CFile::GetStatus((LPCTSTR)filename, status)) {
    UtilSplitPath(curSettings, setPath, strLine);
    UtilSplitPath(filename, strLine, scriptTail);
    if (loadMess || setPath == strLine) {
      SEMMessageBox("The script file " + filename + " does not exist", MB_EXCLAME);
      return 1;
    }
    if (setPath.GetLength() > 0 && setPath.GetAt(setPath.GetLength() - 1) != '\\')
      setPath += "\\";
    strLine = setPath + scriptTail;
    if (!CFile::GetStatus((LPCTSTR)strLine, status)) {
      SEMMessageBox("Neither script file " + filename + " nor " + strLine + " exists",
        MB_EXCLAME);
      return 1;
    }
    filename = strLine;
  }
  try {
    mFile = new CStdioFile(filename, CFile::modeRead | CFile::shareDenyWrite);

    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
      MAX_TOKENS)) == 0) {
        if (NAME_IS("Macro")) {
          err = ReadOneMacro(itemInt[1], strLine, strItems, maxMacros);
          if (err > 0) {
            retval = err;
            break;
          }
          numLoaded++;
        } else if (NAME_IS("MaxMacros"))
          mMaxReadInMacros = itemInt[1];
    }
    if (err > 0)
      retval = err;
      
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  if (retval) {
    SEMMessageBox("An error occurred reading scripts from the file\n" + filename,
      MB_EXCLAME);
    filename = "";
  }  else if (loadMess) {
    strLine.Format("Loaded %d scripts from the file", numLoaded);
    if (printMess)
      mWinApp->AppendToLog(strLine);
    else
      AfxMessageBox(strLine, MB_OK | MB_ICONINFORMATION);
  }
  return retval;
}

// Write macros to a file with the given name
void CParameterIO::WriteMacrosToFile(CString filename, int maxMacros)
{
  try {

    // Open the file for writing, 
    mFile = new CStdioFile(filename, CFile::modeCreate | CFile::modeWrite | 
      CFile::shareDenyWrite);
    WriteAllMacros(maxMacros);
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing scripts to file " + filename;
    AfxMessageBox(message, MB_EXCLAME);
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
}

// Write low dose parameters to a string without \n for use here and in Navigator
void CParameterIO::WriteLowDoseToString(LowDoseParams *ldp, int ldi, int ldj, CString &str)
{
  // Set number or - state # + 1, mag or -cam length, spot, intensity, axis offset, 
  // 0-2 for regular/GIF/STEM or 0 for state, slit in, slit width, energy loss,
  // zero loss, beam X offset, beam Y offset, alpha, diffraction focus, beam tilt X,
  // beam tilt Y, probe mode, dark field, dark field tilt X and Y
  str.Format("%d %d %d %f %f %d %d %f %f %d %f %f %f %f %f"
    " %f %d %d %f %f %f",
    ldi, (ldp->magIndex ? ldp->magIndex : -ldp->camLenIndex), ldp->spotSize,
    ldp->intensity, ldp->axisPosition, ldj, ldp->slitIn ? 1 : 0, ldp->slitWidth,
    ldp->energyLoss, ldp->zeroLoss, ldp->beamDelX, ldp->beamDelY, ldp->beamAlpha,
    ldp->diffFocus, ldp->beamTiltDX, ldp->beamTiltDY, ldp->probeMode,
    ldp->darkFieldMode, ldp->dfTiltX, ldp->dfTiltY, ldp->EDMPercent);
}

// Write state parameters to a string without \n for use here and in Navigator
void CParameterIO::WriteStateToString(StateParams *stateP, CString &str)
{
  str.Format("%d %d %d %d %f %d %f %f %d %d %d %d %f %f "
    "%d %d %d %f %d %d %d %d %d %d %d %d %d %d %d %d %d %f %f %f %f %d",
    stateP->lowDose, stateP->camIndex, stateP->magIndex,
    stateP->spotSize, stateP->intensity, stateP->slitIn ? 1 : 0, stateP->energyLoss,
    stateP->slitWidth, stateP->zeroLoss ? 1 : 0, stateP->binning, stateP->xFrame,
    stateP->yFrame, stateP->exposure, stateP->drift, stateP->shuttering,
    stateP->K2ReadMode, stateP->probeMode, stateP->frameTime, stateP->doseFrac,
    stateP->saveFrames, stateP->processing, stateP->alignFrames,
    stateP->useFrameAlign, stateP->faParamSetInd, stateP->readModeView,
    stateP->readModeFocus, stateP->readModeTrial, stateP->readModePrev,
    stateP->readModeSrch, stateP->readModeMont, stateP->beamAlpha,
    stateP->targetDefocus, stateP->ldDefocusOffset, stateP->ldShiftOffsetX,
    stateP->ldShiftOffsetY, stateP->montMapConSet ? 1 : 0);
}

#define HARD_CODED_FLAGS (NAA_FLAG_HAS_SETUP | NAA_FLAG_ALWAYS_HIDE | \
NAA_FLAG_ONLY_BEFORE | NAA_FLAG_EVERYN_ONLY | NAA_FLAG_ANY_SITE_OK)

// Reads navigator acquire params from the current file, settings or other file
int CParameterIO::ReadNavAcqParams(NavAcqParams *navParams, NavAcqAction *navActions, 
  int *actOrder, CString &unrecognized)
{
  int err, index, ind2, numActions = mWinApp->mNavHelper->GetNumAcqActions();
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  float itemFlt[MAX_TOKENS];
  bool included[NAA_MAX_ACTIONS];
  for (index = 0; index < numActions; index++)
    included[index] = false;

  while ((err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
    itemFlt, MAX_TOKENS)) == 0) {
    if (NAME_IS("NavAcquireParams"))
      continue;
    if (NAME_IS("EndNavAcquireParams"))
      break;
    if (NAME_IS("AcquireParams1")) {
      SET_ACTION(navActions, NAACT_AUTOFOCUS, itemInt[1]);
      SET_ACTION(navActions, NAACT_FINE_EUCEN, itemInt[2]);
      SET_ACTION(navActions, NAACT_REALIGN_ITEM, itemInt[3]);
      navParams->restoreOnRealign = itemInt[4] != 0;
      SET_ACTION(navActions, NAACT_ROUGH_EUCEN, itemInt[5]);
      navParams->nonTSacquireType = itemInt[6];
      navParams->macroIndex = itemInt[7];
      navParams->closeValves = itemInt[8] != 0;
      navParams->sendEmail = itemInt[9] != 0;
      navParams->preMacroInd = itemInt[10];
      navParams->runPremacro = itemInt[11] != 0;
      navParams->preMacroIndNonTS = itemInt[12];
      navParams->runPremacroNonTS = itemInt[13] != 0;
      navParams->sendEmailNonTS = itemInt[14] != 0;
      navParams->skipInitialMove = itemInt[15] != 0;
      navParams->skipZmoves = itemInt[16] != 0;
      navParams->postMacroInd = itemInt[17];
      navParams->runPostmacro = itemInt[18] != 0;
      navParams->postMacroIndNonTS = itemInt[19];
      navParams->runPostmacroNonTS = itemInt[20] != 0;
      if (itemEmpty[21])
        navParams->saveAsMapChoice = navParams->nonTSacquireType == 0;
      else
        navParams->saveAsMapChoice = itemInt[21] != 0;
      if (!itemEmpty[22])
        navParams->skipZinRunAtNearest = itemInt[22] != 0;
      if (!itemEmpty[23])
        navParams->refineZlpOptions = itemInt[23];
      if (!itemEmpty[27]) {
        navParams->runExtramacro = itemInt[24] != 0;
        navParams->extraMacroInd = itemInt[25];
        navParams->runStartMacro = itemInt[27] != 0;
        navParams->startMacroInd = itemInt[26];
      }

    } else if (NAME_IS("AcquireParams2")) {
      navParams->cycleDefocus = itemInt[1] != 0;
      navParams->cycleDefFrom = itemFlt[2];
      navParams->cycleDefTo = itemFlt[3];
      navParams->cycleSteps = itemInt[4];
      navParams->earlyReturn = itemInt[5] != 0;
      navParams->numEarlyFrames = itemInt[6];;
      navParams->noMBoxOnError = itemInt[7] != 0;
      navParams->skipSaving = itemInt[8] != 0;
      navParams->hideUnusedActs = itemInt[9] != 0;
      navParams->acqDlgSelectedPos = itemInt[10];
      navParams->focusChangeLimit = itemFlt[11];
      navParams->DEdarkRefOperatingMode = itemInt[12];
      navParams->highFlashIfOK = itemInt[13];
      navParams->astigByBTID = itemInt[14] != 0;
      navParams->adjustBTforIS = itemInt[15] != 0;
      navParams->relaxStage = itemInt[16] != 0;
      navParams->hybridRealign = itemInt[17] != 0;
      navParams->hideUnselectedOpts = itemInt[18] != 0;
      if (!itemEmpty[19])
        navParams->mapWithViewSearch = itemInt[19];
      if (!itemEmpty[20])
        navParams->retractCameras = itemInt[20] != 0;
      if (!itemEmpty[21])
        navParams->runHoleCombiner = itemInt[21] != 0;
      if (!itemEmpty[22])
        navParams->useMapHoleVectors = itemInt[22] != 0;
      if (!itemEmpty[23]) {
        navParams->endMacroInd = itemInt[23];
        navParams->runEndMacro = itemInt[24] != 0;
      }
      if (!itemEmpty[25]) {
        navParams->realignToScaledMap = itemInt[25] != 0;
        navParams->conSetForScaledAli = itemInt[26];
      }
      if (!itemEmpty[27])
        navParams->multiGridSubset = itemInt[27];
      if (!itemEmpty[29]) {
        navParams->mulGridSubsetFrom = itemInt[28];
        navParams->mulGridItemsOrShots = itemInt[29];
      }

    } else if (strItems[0].Find("NavAcqAction") == 0) {
      index = atoi((LPCTSTR)strItems[0].Mid(12));
      if (index >= 0 && index < NAA_MAX_ACTIONS) {
        navActions[index].flags = (navActions[index].flags & HARD_CODED_FLAGS) | 
          (itemInt[1] & ~HARD_CODED_FLAGS);
        navActions[index].timingType = itemInt[2];
        navActions[index].everyNitems = itemInt[3];
        navActions[index].minutes = itemInt[4];
        navActions[index].distance = itemFlt[5];
        StripItems(strLine, 6, navActions[index].labelOrNote);
      }

    } else if (NAME_IS("ActionOrder")) {
      for (index = 0; index < NAA_MAX_ACTIONS; index++) {
        if (itemEmpty[index + 1])
          break;
        actOrder[index] = itemInt[index + 1];
        if (actOrder[index] >= 0 && actOrder[index] < numActions)
          included[actOrder[index]] = true;
      }
      for (; index < numActions; index++) {
        for (ind2 = 0; ind2 < numActions; ind2++) {
          if (!included[ind2]) {
            actOrder[index] = ind2;
            included[ind2] = true;
            break;
          }
        }
      }
    } else {
      unrecognized += strLine + "\n";
    }
  }
  return err;
}

// Writes navigator acquire params to the current file, settings or other file
void CParameterIO::WriteNavAcqParams(int which, NavAcqParams *navParams,
  NavAcqAction *mAcqActions, int *actOrder, bool skipNum)
{
  int i;
  CString oneState, orderLine;

  // Leave off number for a free-standing file, it is misleading and would mess up the
  //  check for the word at start
  if (skipNum)
    mFile->WriteString("NavAcquireParams\n");
  else
    WriteInt("NavAcquireParams", which);
  oneState.Format("AcquireParams1 %d %d %d %d %d %d %d %d %d %d %d %d %d"
    " %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", DOING_ACTION(NAACT_AUTOFOCUS),
    DOING_ACTION(NAACT_FINE_EUCEN),
    DOING_ACTION(NAACT_REALIGN_ITEM), navParams->restoreOnRealign ? 1 : 0,
    DOING_ACTION(NAACT_ROUGH_EUCEN), navParams->nonTSacquireType,
    navParams->macroIndex, navParams->closeValves ? 1 : 0,
    navParams->sendEmail ? 1 : 0,
    navParams->preMacroInd, navParams->runPremacro ? 1 : 0,
    navParams->preMacroIndNonTS, navParams->runPremacroNonTS ? 1 : 0,
    navParams->sendEmailNonTS ? 1 : 0, navParams->skipInitialMove ? 1 : 0,
    navParams->skipZmoves ? 1 : 0, navParams->postMacroInd,
    navParams->runPostmacro ? 1 : 0, navParams->postMacroIndNonTS,
    navParams->runPostmacroNonTS ? 1 : 0, navParams->saveAsMapChoice ? 1 : 0,
    navParams->skipZinRunAtNearest ? 1 : 0, navParams->refineZlpOptions,
    navParams->runExtramacro ? 1 : 0, navParams->extraMacroInd,
    navParams->runStartMacro ? 1 : 0, navParams->startMacroInd);
  mFile->WriteString(oneState);
  oneState.Format("AcquireParams2 %d %f %f %d %d %d %d %d %d %d %f %d %d %d %d %d %d %d "
    "%d %d %d %d %d %d %d %d %d %d %d\n", navParams->cycleDefocus ? 1 : 0,
    navParams->cycleDefFrom,
    navParams->cycleDefTo, navParams->cycleSteps,
    navParams->earlyReturn ? 1 : 0, navParams->numEarlyFrames,
    navParams->noMBoxOnError ? 1 : 0, navParams->skipSaving ? 1 : 0,
    navParams->hideUnusedActs ? 1 : 0, navParams->acqDlgSelectedPos,
    navParams->focusChangeLimit, navParams->DEdarkRefOperatingMode,
    navParams->highFlashIfOK, navParams->astigByBTID ? 1 : 0,
    navParams->adjustBTforIS ? 1 : 0, navParams->relaxStage ? 1 : 0,
    navParams->hybridRealign ? 1 : 0, navParams->hideUnselectedOpts ? 1 : 0,
    navParams->mapWithViewSearch, navParams->retractCameras ? 1 : 0,
    navParams->runHoleCombiner ? 1 : 0, navParams->useMapHoleVectors ? 1 : 0,
    navParams->endMacroInd, navParams->runEndMacro ? 1 : 0, 
    navParams->realignToScaledMap ? 1 : 0, navParams->conSetForScaledAli,
    navParams->multiGridSubset, navParams->mulGridSubsetFrom,
    navParams->mulGridItemsOrShots);
  mFile->WriteString(oneState);

  orderLine = "ActionOrder";
  for (i = 0; i < mWinApp->mNavHelper->GetNumAcqActions(); i++) {
    oneState.Format(" %d", actOrder[i]);
    orderLine += oneState;
    oneState.Format("NavAcqAction%d %d %d %d %d %f %s\n", i, mAcqActions[i].flags,
      mAcqActions[i].timingType, mAcqActions[i].everyNitems, mAcqActions[i].minutes,
      mAcqActions[i].distance, (LPCTSTR)mAcqActions[i].labelOrNote);
    mFile->WriteString(oneState);
  }
  orderLine += "\n";
  mFile->WriteString(orderLine);
  mFile->WriteString("EndNavAcquireParams\n");

}

// read acquire params from the given file
int CParameterIO::ReadAcqParamsFromFile(NavAcqParams *navParams, 
  NavAcqAction *acqActions, int *actOrder, CString & filename, CString &errStr)
{
  CString strLine, strItems[2];
  int err, retval = 0;
  bool readForMulGrid = navParams == NULL;
  MGridAcqItemsParams mgParam;
  CArray<MGridAcqItemsParams, MGridAcqItemsParams> *paramArr =
    mWinApp->mMultiGridTasks->GetMGAcqItemsParamArray();
  if (readForMulGrid) {
    paramArr->RemoveAll();
    mWinApp->mNavHelper->CopyAcqParamsAndActions(mWinApp->mNavHelper->GetAcqActions(1),
      mWinApp->GetNavAcqParams(1), mWinApp->mNavHelper->GetAcqActCurrentOrder(1),
      mgParam.actions, &mgParam.params, mgParam.actOrder);
  }
  errStr = "";

  try {
    // Open the file for reading, verify that it is a param file
    mFile = new CStdioFile(filename, CFile::modeRead | CFile::shareDenyWrite);

    while (!retval) {
      err = ReadAndParse(strLine, strItems, 2);
      if (err < 0 && readForMulGrid)
        break;
      if (err)
        retval = 1;
      else if (!(readForMulGrid && paramArr->GetSize() > 0) &&
        CheckForByteOrderMark(strItems[0], "NavAcquireParams", filename,
          "Navigator acquisition parameters")) {
        retval = 1;
      }
      if (!retval) {
        if (readForMulGrid) {
          retval = ReadNavAcqParams(&mgParam.params, &mgParam.actions[0],
            &mgParam.actOrder[0], strLine);
          if (!retval)
            paramArr->Add(mgParam);
          else
            errStr = strLine + "\r\nUnexpected end of file or other error "
            "reading Navigator acquire parameters";
        } else {
          retval = ReadNavAcqParams(navParams, acqActions, actOrder, strLine);
          break;
        }
      }
    }
    mFile->Close();
  }
  catch (CFileException *perr) {
    perr->Delete();
    retval = 1;
  }
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }

  return retval;
}

// Write acquire params to the given file
void CParameterIO::WriteAcqParamsToFile(NavAcqParams *navParams, 
  NavAcqAction *acqActions, int *actOrder, CString & filename)
{
  try {
    // Open the file for writing, 
    mFile = new CStdioFile(filename, CFile::modeCreate |
      CFile::modeWrite | CFile::shareDenyWrite);
    WriteNavAcqParams(0, navParams, acqActions, actOrder, false);
  }
  catch (CFileException *perr) {
    perr->Delete();
    CString message = "Error writing Navigator acquisition parameters to file " +filename;
    AfxMessageBox(message, MB_EXCLAME);
  }
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
}

// Writes the possibly multiple nav acquire parameters for multi-grid
int CParameterIO::WriteMulGridAcqParams(CString &filename, CString &errStr)
{
  int ind, retval = 0;
  CArray<MGridAcqItemsParams, MGridAcqItemsParams> *paramArr = 
    mWinApp->mMultiGridTasks->GetMGAcqItemsParamArray();
  
  try {
    // Open the file for writing, 
    mFile = new CStdioFile(filename, CFile::modeCreate |
      CFile::modeWrite | CFile::shareDenyWrite);
    for (ind = 0; ind < (int)paramArr->GetSize(); ind++) {
      MGridAcqItemsParams &param = paramArr->ElementAt(ind);
      WriteNavAcqParams(ind, &param.params, &param.actions[0], &param.actOrder[0], true);
    }
  }
  catch (CFileException *perr) {
    perr->Delete();
    errStr = "Error writing Navigator acquisition parameters to file " + filename;
    retval = 1;
  }

  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  return retval;
}

// Reads a file to hide or disable items containing either defined strings in the
// table in sDisableHideList or menu item IDs to be hidden
void CParameterIO::ReadDisableOrHideFile(CString & filename, std::set<int>  *IDsToHide,
  std::set<int>  *lineHideIDs, std::set<int> *IDsToDisable, StringSet *stringHides)
{
  CStdioFile *file = NULL;
  int err = 0, type, ind, space;
  int numInTable = sizeof(sDisableHideList) / sizeof(DisableHideItem);
  CString strLine, tag, mess = "Error opening", unknown;
  std::string sstr;
  char *endPtr;
  bool versionEnd, checkBOM = true;
  bool toggleMode = !mWinApp->GetStartingProgram() && mWinApp->GetBasicMode();
  if (toggleMode)
    mWinApp->SetBasicMode(false);
  try {
    file = new CStdioFile(filename, CFile::modeRead | CFile::shareDenyWrite);
    mess = "Error reading from";
    while (file->ReadString(strLine)) {
      if (checkBOM) {
        if (CheckForByteOrderMark(strLine, "", filename, "items to hide or disable"))
          return;
        checkBOM = false;
        IDsToHide->clear();
        IDsToDisable->clear();
        lineHideIDs->clear();
        stringHides->clear();
      }
      if (strLine.IsEmpty() || strLine.GetAt(0) == '#')
        continue;

      // Separate the type from the tag string and convert the type
      if (ParseString(strLine, &tag, 1, false) == 2) {
        versionEnd = tag.CompareNoCase("EndIfVersionBelow") == 0;
        type = atoi((LPCTSTR)tag);
        if (type < 1 && !versionEnd) {
          AfxMessageBox("Incorrect number for disabling or hiding in line:\n" +
            strLine + "\n\nin file:  " + filename, MB_EXCLAME);
        } else {
          StripItems(strLine, 1, tag);
          if (versionEnd) {
            if (mWinApp->GetIntegerVersion() < atoi(tag))
              break;
            continue;
          }

          // See if the tag starts with a legal number which is assumed to be an ID to
          // hide if hiding is specified
          sstr = tag;
          ind = strtol(sstr.c_str(), &endPtr, 10);
          if (ind > 0 && ind < 65536 && (*endPtr == 0x00 || *endPtr == ' ') &&
            type == 2) {
            IDsToHide->insert(ind);
          } else {

            // Otherwise look up the string in the list
            for (ind = 0; ind < numInTable; ind++) {
              if (!tag.CompareNoCase(sDisableHideList[ind].descrip)) {
                if (sDisableHideList[ind].disableOrHide & type) {

                  // Store as string, disable, lin ehide, or simple hide
                  if (sDisableHideList[ind].nID < 1 && sDisableHideList[ind].nID > -10) {
                    space = tag.ReverseFind(' ');
                    tag = tag.Left(space);
                    stringHides->insert(std::string((LPCTSTR)tag));
                  } else if (type == 1)
                    IDsToDisable->insert(sDisableHideList[ind].nID);
                  else if (sDisableHideList[ind].wholeLine)
                    lineHideIDs->insert(sDisableHideList[ind].nID);
                  else
                    IDsToHide->insert(sDisableHideList[ind].nID);
                } else {
                  tag.Format("This menu item can only be %s not %s in line:\n",
                    type == 1 ? "hidden" : "disabled", type > 1 ? "hidden" : "disabled");
                  AfxMessageBox(tag + strLine + "\n\nin file:  " + filename, MB_EXCLAME);
                }
                break;
              }
            }
            if (ind >= numInTable) {
              unknown += "\n" + strLine;
            }
          }
        }
      } else
        AfxMessageBox("Incomplete entry to disable or hide in line:\n"
          + strLine + "\n\nin file:  " + filename, MB_EXCLAME);
    }
    if (!unknown.IsEmpty())
      AfxMessageBox("Unrecognized description of item(s) to disable or hide in "
        "\nin file:  " + filename + "\n" + unknown, MB_EXCLAME);
    file->Close();
  }
  catch (CFileException *perr) {
    perr->Delete();
    err = 1;
  }
  if (file) {
    if (mess.Find("opening") < 0)
      delete file;
    file = NULL;
  }
  if (err)
    AfxMessageBox(mess + " file of items to disable or hide:\n" + filename);
  if (!err && toggleMode)
    mWinApp->SetBasicMode(true);
}

// Properties are measured outside the program, entered by hand into the
// text file
int CParameterIO::ReadProperties(CString strFileName)
{
  int retval = 0;
  int err;
  int nMags, index, magInd, col, ind;
  float scale;
  int *kvList;
  int *activeList = mWinApp->GetActiveCameraList();
  int *camLengths = mWinApp->GetCamLenTable();
  float *camLenCal = mWinApp->GetCamLenCalibrated();
  float *diffRotations = mWinApp->GetDiffModeRotations();
  int *C2apertures = mWinApp->mBeamAssessor->GetC2Apertures();
  float *radii = mWinApp->mProcessImage->GetFFTCircleRadii();
  float *alphaFacs = mWinApp->mBeamAssessor->GetBSCalAlphaFactors();
  BOOL recognized, recognized2, recognized30, recognized35, recognizedc, recognizedc1;
  BOOL recognized15, recognized4;
  bool startCcomment, warned999 = false;
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  float itemFlt[MAX_TOKENS];
  CString message;
  CameraParameters *mCamParam = mWinApp->GetCamParams();
  CameraParameters *camP;
  std::map<int, float> *refinedPix = mShiftManager->GetRefinedPixelSizes();
  FileOptions *defFileOpt = mDocWnd->GetDefFileOpt();
  CArray<CString, CString> *globalValues = mDocWnd->GetGlobalAdocValues();
  CArray<CString, CString> *globalKeys = mDocWnd->GetGlobalAdocKeys();
  MacroControl *macControl = mWinApp->GetMacControl();
  ShortVec *bsBoundaries = mShiftManager->GetBeamShiftBoundaries();
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES  
  NavParams *navParams = mWinApp->GetNavParams();
  RelRotations *relRotations = mShiftManager->GetRelRotations();
  CArray<RotStretchXform, RotStretchXform> *rotXformArray = 
    mShiftManager->GetRotXforms();
  CArray<ChannelSet, ChannelSet> *channelSets = camera->GetChannelSets();
  CArray<ChannelSet, ChannelSet> *blockSets = scope->GetBlockedChannels();
  CArray<PiezoScaling, PiezoScaling> *piezoScalings = 
    mWinApp->mPiezoControl->GetScalings();
  CArray<MontLimits, MontLimits> *montLimits = 
    mWinApp->mMontageController->GetMontageLimits();
  CArray<LensRelaxData, LensRelaxData> *relaxData = scope->GetLensRelaxProgs();
  CArray<FreeLensSequence, FreeLensSequence> *FLCsequences = scope->GetFLCSequences();
  CString *K2FilterNames = camera->GetK2FilterNames();
  HitachiParams *hitachi = mWinApp->GetHitachiParams();
  std::vector<ShortVec> *apertureSizes = scope->GetApertureLists();
  RotStretchXform rotXform;
  LensRelaxData relax;
  FreeLensSequence FLCseq;
  FloatVec *vec;
  IntVec *extraTasks = mWinApp->mNavHelper->GetExtraTaskList();
  unsigned char *palette = mWinApp->GetPaletteColors();
  short lensNormMap[] = {nmSpotsize, nmCondenser, pnmObjective, pnmProjector, nmAll,
    pnmAll};
  PiezoScaling pzScale;
  ChannelSet chanSet;
  MontLimits montLim;
  TiffField *tfield;
  int camEntered[MAX_CAMERAS];
  std::set<std::string> genPropSet;
  std::set<std::string> camPropSets[MAX_CAMERAS];
  std::string tmpg[] = {"cameraproperties", "importstagetoimage", "importtiffxfield", 
    "importtiffyfield", "importtiffidfield", "k2filtername", "controlsetname", 
    "othershiftboundaries", "watchgauge", "mappedtodschannel", "detectorblocks", 
    "mutuallyexcludedetectors", "socketserverip", "socketserveripif64", 
    "socketserverport", "socketserverportif64", "externaltool", "toolcommand", 
    "toolarguments", "pathtopython", "endifversionbelow", "globalautodocentry",
    "aperturesizes", "jeollowdoseflcseries"};
  std::set<std::string> dupOKgenProps(tmpg, tmpg + sizeof(tmpg) / sizeof(tmpg[0]));
  std::string tmpc[] = {"hotpixels", "rotationandpixel", "detectorname", "channelname",
    "rotationstretchxform", "specialrelativerotation", "binningoffset", "hotcolumns",
    "partialbadcolumn", "badcolumns", "partialbadrow", "badrows", "badpixels",
    "refinedpixel"};
  std::set<std::string> dupOKcamProps(tmpc, tmpc + sizeof(tmpc) / sizeof(tmpc[0]));
  std::string tmpnv[] = {"endcameraproperties"};
  std::set<std::string> noValOKprops(tmpnv, tmpnv + sizeof(tmpnv) / sizeof(tmpnv[0]));
  memset(&camEntered[0], 0, MAX_CAMERAS * sizeof(int));
  mCheckForComments = true;

#define INT_PROP_TEST(a, b) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    camP->b = itemInt[1];
#define BOOL_PROP_TEST(a, b) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    camP->b = itemInt[1] != 0;
#define FLOAT_PROP_TEST(a, b) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    camP->b = itemFlt[1];

  try {
    // Open the file for reading, verify that it is a properties file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead | CFile::shareDenyWrite);

    for (;;) {
      err = ReadAndParse(strLine, strItems, MAX_TOKENS);
      if (err)
        retval = 1;
      else if (strLine.IsEmpty())
        continue;
      else if (CheckForByteOrderMark(strItems[0], "SerialEMProperties", strFileName,
        "properties"))
        retval = 1;
      break;
    }

    while (retval == 0 &&
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
        MAX_TOKENS)) == 0) {
      recognized = true;
      recognized2 = true;
      recognized30 = true;
      recognized35 = true;
      recognized4 = true;
      recognizedc = true;
      recognized15 = true;

      message = strItems[0];
      startCcomment = strItems[0].Find("/*") == 0;
      std::string propLower = (LPCTSTR)message.MakeLower();
      if (!itemEmpty[0] && !startCcomment) {
        if (genPropSet.count(propLower) && !dupOKgenProps.count(propLower))
          mDupMessage += strItems[0] + " (general property)\r\n";
        else
          genPropSet.insert(propLower);
        if (itemEmpty[1] && !noValOKprops.count(propLower)) {
          message.Format("The property %s is present without a value and\n"
            "must have a value to be interpreted properly", (LPCTSTR)strItems[0]);
          AfxMessageBox(message, MB_EXCLAME);
        }
      }
      if (!startCcomment)
        CheckForSpecialChars(strLine);
      if (MatchNoCase("EndIfVersionBelow")) {
        if (mWinApp->GetIntegerVersion() < itemInt[1])
          break;

      } else if (startCcomment) {
        while (strLine.Find("*/") < 0 &&
          (err = ReadAndParse(strLine, strItems, MAX_TOKENS)) == 0) {
        }
        if (err)
          AfxMessageBox(CString(err < 0 ? "End of file was reached" :
            "A read error occurred") + " looking for */ (the end of a C-style comment) in"
            " SerialEMproperties.txt", MB_EXCLAME);

      } else if (MatchNoCase("CameraProperties")) {
        int iset = itemInt[1];
        if (iset >= MAX_CAMERAS) {
          message.Format("Index in \"CameraProperties %d\" is out of range", iset);
          AfxMessageBox(message, MB_EXCLAME);
          retval = 1;
          break;
        }
        if (iset >= 0) {
          camP = &mCamParam[iset];
          if (camEntered[iset]) {
            message.Format("More than one entry in properties file for camera %d", iset);
            AfxMessageBox(message, MB_EXCLAME);
          }
          camEntered[iset] = 1;
        }
        while ((err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl,
          itemFlt, MAX_TOKENS)) == 0) {
          message = strItems[0];
          if (iset >= 0) {
            std::string propLower = (LPCTSTR)message.MakeLower();
            if (!itemEmpty[0]) {
              if (camPropSets[iset].count(propLower) && !dupOKcamProps.count(propLower)) {
                message.Format("%s (property for camera # %d)\r\n", (LPCTSTR)strItems[0],
                  iset);
                mDupMessage += message;
              } else
                camPropSets[iset].insert(propLower);
            }
          }

          CheckForSpecialChars(strLine);
          recognizedc1 = true;
          if (MatchNoCase("EndCameraProperties"))
            break;
          if (iset < 0)
            continue;
          else if (MatchNoCase("HalfSizes")) {
            index = 0;
            while (index < 2 * MAX_BINNINGS && !itemEmpty[index + 1]) {
              if (index % 2)
                camP->halfSizeY[index / 2] = B3DMAX(0, itemInt[index + 1]);
              else
                camP->halfSizeX[index / 2] = B3DMAX(0, itemInt[index + 1]);
              index++;
            }
          } else if (MatchNoCase("QuarterSizes")) {
            index = 0;
            while (index < 2 * MAX_BINNINGS && !itemEmpty[index + 1]) {
              if (index % 2)
                camP->quarterSizeY[index / 2] = B3DMAX(0, itemInt[index + 1]);
              else
                camP->quarterSizeX[index / 2] = B3DMAX(0, itemInt[index + 1]);
              index++;
            }
          } else if (MatchNoCase("XMustBeMultipleOf")) {
            if (camP->moduloX >= 0)
              camP->moduloX = itemInt[1];
          } else if (MatchNoCase("YMustBeMultipleOf")) {
            if (camP->moduloY >= 0)
              camP->moduloY = itemInt[1];
          } else if (MatchNoCase("RestrictedSizeType")) {

            // Restricted size type 3 means centered coordinates only, 4 for weird Tietz
            if (itemInt[1] == 3) {
              camP->centeredOnly = 1;
            } else if (itemInt[1] == 4) {
              camP->TietzBlocks = 1024;
            } else if (itemInt[1] != 0) {

              // Otherwise, type is encoded as negative modulo, #2 or #5 means no subareas
              camP->moduloX = -itemInt[1];
              camP->moduloY = camP->moduloX;
              if (camP->moduloX == -2 || camP->moduloX == -5) {
                camP->subareasBad = 2;
              }
            }
          } else if (MatchNoCase("FEICameraType")) {
            if (itemInt[1] == FALCON4_TYPE + 1 || itemInt[1] == FALCON4_TYPE + 11) {
              itemInt[1] -= 1;
              camP->falconVariant = FALCON4I_VARIANT;
            }
            camP->FEItype = itemInt[1];
            camP->moduloX = camP->moduloY = -1;
          } else if (MatchNoCase("AMTCameraType"))
            camP->AMTtype = itemInt[1];
          else if (MatchNoCase("DECameraType")) {

            // Old Direct Electron LC_1100 camera is centered only
            camP->DE_camType = itemInt[1];
            camP->centeredOnly = itemInt[1] >= 2 ? 0 : 1;
            if (camP->DE_camType == DE_TYPE_APOLLO) {
              camP->CamFlags |= DE_APOLLO_CAMERA;
              camP->DE_camType = DE_12;
            }
          } else if (MatchNoCase("DEAutosaveFolder"))
            StripItems(strLine, 1, camP->DE_AutosaveDir);
          else if (MatchNoCase("DECameraServerIP"))
            camP->DEServerIP = strItems[1];

#define CAM_PROP_SECT1
#include "PropertyTests.h"
#undef CAM_PROP_SECT1

          else if (MatchNoCase("TietzCameraType")) {
            camP->TietzType = itemInt[1];
            if (itemInt[1] == 8 || itemInt[1] == 11 || itemInt[1] == 12 ||
              itemInt[1] == 13)
              camP->TietzBlocks = 1024;
            if (itemInt[1] >= 15 && itemInt[1] <= 18) {
              camP->moduloX = -9;
              camP->moduloY = -9;
            }
            if (camP->TietzType < 0)
              camP->STEMcamera = true;
          } else if (MatchNoCase("DropFramesStartEnd")) {
            B3DCLAMP(itemInt[1], 0, TIETZ_DROP_FRAME_MASK);
            B3DCLAMP(itemInt[2], 0, TIETZ_DROP_FRAME_MASK);
            camP->dropFrameFlags = itemInt[1] + (itemInt[2] << TIETZ_DROP_FRAME_BITS);
          } else if (MatchNoCase("TietzFlatfieldDir"))
            StripItems(strLine, 1, camP->TietzFlatfieldDir);
          else if (MatchNoCase("SubareasAreBad")) {
            if (camP->subareasBad < 2)
              camP->subareasBad = itemInt[1];
          } else if (MatchNoCase("UseContinuousMode")) {
            camP->useContinuousMode = itemInt[1];
            if (!itemEmpty[2])
              camP->setContinuousReadout = itemInt[2] != 0;
            if (!itemEmpty[3])
              camP->continuousQuality = itemInt[3];
            B3DCLAMP(camP->continuousQuality, 0, 7);
            if (!itemEmpty[4])
              camP->balanceHalves = itemInt[4];
          } else if (MatchNoCase("BoundaryBetweenHalves")) {
            camP->halfBoundary = itemInt[1];
            if (!itemEmpty[2])
              camP->ifHorizontalBoundary = itemInt[2];
          } else if (MatchNoCase("PluginName"))
            StripItems(strLine, 1, camP->pluginName);
          else if (MatchNoCase("ShutterLabel1"))
            StripItems(strLine, 1, camP->shutterLabel1);
          else if (MatchNoCase("ShutterLabel2"))
            StripItems(strLine, 1, camP->shutterLabel2);
          else if (MatchNoCase("ShutterLabel3"))
            StripItems(strLine, 1, camP->shutterLabel3);
          else if (MatchNoCase("RestoreSettings"))
            camP->CamFlags |= CAMFLAG_RESTORE_SETTINGS;
          else if (MatchNoCase("DMRotationAndFlip") || MatchNoCase("AlignedSumRotFlip"))
            camP->DMrotationFlip = itemInt[1];
          else if (MatchNoCase("MinimumFrameTimes"))
            StoreFloatsPerBinning(strItems, "minimum frame times", iset, strFileName,
              camP->minFrameTime);
          else if (MatchNoCase("FrameTimeDivisors"))
            StoreFloatsPerBinning(strItems, "divisor for frame times", iset, strFileName,
              camP->frameTimeDivisor);
          else if (MatchNoCase("MaximumScanRate")) {
            camP->maxScanRate = itemFlt[1];
            if (itemEmpty[2])
              camP->advisableScanRate = camP->maxScanRate;
            else
              camP->advisableScanRate = itemFlt[2];
          } else if (MatchNoCase("STEMsubareaInCorner")) {
            camP->subareaInCorner = itemInt[1] != 0;
            if (!itemEmpty[2])
              camera->SetSubareaShiftDelay(itemInt[2]);
          } else if (MatchNoCase("ChannelName")) {
            if (camP->numChannels < MAX_STEM_CHANNELS)
              StripItems(strLine, 1,
                camP->channelName[camP->numChannels++]);
          } else if (MatchNoCase("DetectorName")) {
            if (itemInt[1] >= 0 && itemInt[1] < MAX_STEM_CHANNELS)
              StripItems(strLine, 2, camP->detectorName[itemInt[1]]);
          } else if (MatchNoCase("NeedShotToInsert")) {
            index = 1;
            while (!strItems[index].IsEmpty()) {
              if (itemInt[index] >= 0 && itemInt[index] < MAX_STEM_CHANNELS)
                camP->needShotToInsert[itemInt[index]] = true;
              index++;
            }
          } else if (MatchNoCase("MinMultiChannelBinning")) {
            index = 1;
            while (!strItems[index].IsEmpty() && index < MAX_STEM_CHANNELS) {
              camP->minMultiChanBinning[index] = itemInt[index];
              index++;
            }
          } else if (MatchNoCase("UsableAtMag")) {
            montLim.camera = iset;
            montLim.magInd = itemInt[1];
            montLim.top = itemInt[2];
            montLim.left = itemInt[3];
            montLim.bottom = itemInt[4];
            montLim.right = itemInt[5];
            montLimits->Add(montLim);
          } else
            recognizedc1 = false;

          if (recognizedc1) {

          } else if (MatchNoCase("GIF")) {
            if (itemInt[1] != 0)
              camP->GIF = true;
          } else if (MatchNoCase("FEIFilterType")) {
            if (itemInt[1] > 0) {
              camP->filterIsFEI = itemInt[1];
              camP->GIF = true;
              mWinApp->mScope->SetAdvancedScriptVersion(ASI_FILTER_FEG_LOAD_TEMP);
            }
          } else if (MatchNoCase("RotationAndPixel")) {
            magInd = itemInt[1];
            if (magInd < 1 || magInd >= MAX_MAGS) {
              message.Format("Mag index out of range in \"RotationAndPixel %d\" line for"
                "camera %d\nin properties file %s", magInd, iset, strFileName);
              AfxMessageBox(message, MB_EXCLAME);
            } else {
              mMagTab[magInd].deltaRotation[iset] = itemFlt[2];
              mMagTab[magInd].rotation[iset] = itemFlt[3];
              if (fabs(itemDbl[4]) > 998.9 && fabs(itemDbl[4]) < 999.1) {
                if (!warned999)
                  AfxMessageBox("RotationAndPixel lines should have 0 for undefined"
                    " pixel\nsizes, not 999, in properties file " + strFileName,
                    MB_EXCLAME);
                warned999 = true;
                itemDbl[4] = 0.;
              }
              mMagTab[magInd].pixelSize[iset] = (float)(0.001 * itemDbl[4]);
            }

          } else if (MatchNoCase("RefinedPixel")) {
            magInd = itemInt[1];
            if (itemEmpty[2] || magInd < 1 || magInd >= MAX_MAGS) {
                message.Format("Mag index out of range or missing pixel size in"
                  " \"RefinedPixel %d\" line for camera %d\nin properties file %s", iset,
                  strFileName);
                AfxMessageBox(message, MB_EXCLAME);
            } else {
              refinedPix->insert(std::pair<int, float>(10 * magInd + iset, itemFlt[2]));
            }

          } else if (MatchNoCase("Binnings")) {
            index = 0;
            while (!strItems[index + 1].IsEmpty()) {
              if (index >= MAX_BINNINGS) {
                message.Format("Too many binnings for camera %d\n"
                  "in properties file %s", iset, strFileName);
                AfxMessageBox(message, MB_EXCLAME);
                break;
              }
              camP->binnings[index] = atoi((LPCTSTR)strItems[index + 1]);
              index++;
            }
            camP->numBinnings = index;
          } else if (MatchNoCase("RelativeGainFactors")) {
            StoreFloatsPerBinning(strItems, "gain factor", iset, strFileName,
              camP->gainFactor);
          } else if (MatchNoCase("Name")) {
            StripItems(strLine, 1, camP->name);

          } else if (MatchNoCase("DMGainReferenceName"))
            StripItems(strLine, 1, camP->DMRefName);

#define CAM_PROP_SECT2
#include "PropertyTests.h"
#undef CAM_PROP_SECT2

          else if (MatchNoCase("FalconLocalFramePath")) {
            StripItems(strLine, 1, camP->falconFramePath);
          } else if (MatchNoCase("FalconGainRefDir")) {
            StripItems(strLine, 1, camP->falconRefDir);

          } else if (MatchNoCase("UsableArea")) {
            if (!strItems[1].IsEmpty())
              camP->defects.usableTop = itemInt[1];
            if (!strItems[2].IsEmpty())
              camP->defects.usableLeft = itemInt[2];
            if (!strItems[3].IsEmpty())
              camP->defects.usableBottom = itemInt[3];
            if (!strItems[4].IsEmpty())
              camP->defects.usableRight = itemInt[4];

          } else if (MatchNoCase("BadColumns")) {
            ReadBadColumns(strItems, camP->defects.badColumnStart,
              camP->defects.badColumnWidth);

          } else if (MatchNoCase("BadRows")) {
            ReadBadColumns(strItems, camP->defects.badRowStart,
              camP->defects.badRowHeight);

          } else if (MatchNoCase("PartialBadColumn")) {
            ReadPartialBad(strItems, itemInt, camP->defects.partialBadCol,
              camP->defects.partialBadWidth, camP->defects.partialBadStartY,
              camP->defects.partialBadEndY, "column", strLine);

          } else if (MatchNoCase("PartialBadRow")) {
            ReadPartialBad(strItems, itemInt, camP->defects.partialBadRow,
              camP->defects.partialBadHeight, camP->defects.partialBadStartX,
              camP->defects.partialBadEndX, "row", strLine);

          } else if (MatchNoCase("HotColumns")) {
            index = 1;
            while (!strItems[index].IsEmpty()) {
              if (camP->numHotColumns >= MAX_HOT_COLUMNS) {
                message.Format("Too many hot columns for camera %d\n"
                  "in properties file %s", iset, iset, strFileName);
                AfxMessageBox(message, MB_EXCLAME);
                break;
              }
              camP->hotColumn[camP->numHotColumns++] =
                atoi((LPCTSTR)strItems[index++]);
            }

          } else if (MatchNoCase("HotPixels")) {
            index = 1;
            while (!strItems[index + 1].IsEmpty()) {
              if (camP->hotPixelX.size() >= MAX_HOT_PIXELS) {
                message.Format("Too many hot pixels for camera %d\n"
                  "in properties file %s", iset, strFileName);
                AfxMessageBox(message, MB_EXCLAME);
                break;
              }
              camP->hotPixelX.push_back(atoi((LPCTSTR)strItems[index++]));
              camP->hotPixelY.push_back(atoi((LPCTSTR)strItems[index++]));
            }

          } else if (MatchNoCase("BadPixels")) {
            index = 1;
            while (!strItems[index + 1].IsEmpty()) {
              camP->defects.badPixelX.push_back(atoi((LPCTSTR)strItems[index++]));
              camP->defects.badPixelY.push_back(atoi((LPCTSTR)strItems[index++]));
            }

          } else if (MatchNoCase("BinningOffset")) {
            if (strItems[4].IsEmpty()) {
              message.Format("Binning offset entry needs four numbers\n"
                "for camera %d: %s", iset, strLine);
              AfxMessageBox(message, MB_EXCLAME);
            } else if (camP->numBinnedOffsets >= MAX_BINNINGS) {
              message.Format("Too many binning offsets for camera %d\n"
                "in properties file %s", iset, strFileName);
              AfxMessageBox(message, MB_EXCLAME);
            } else {
              camP->offsetBinning[camP->numBinnedOffsets] =
                itemInt[1];
              camP->offsetRefBinning[camP->numBinnedOffsets] =
                itemInt[2];
              camP->binnedOffsetX[camP->numBinnedOffsets] =
                itemInt[3];
              camP->binnedOffsetY[camP->numBinnedOffsets++] =
                itemInt[4];
            }

          } else if (MatchNoCase("SpecialRelativeRotation")) {
            ind = mShiftManager->GetNumRelRotations();
            if (strItems[3].IsEmpty()) {
              message.Format("Relative rotation entry needs three numbers\n"
                "for camera %d: %s", iset, strLine);
              AfxMessageBox(message, MB_EXCLAME);
            } else if (ind >= MAX_REL_ROT) {
              AfxMessageBox("Too many special relative rotations for arrays",
                MB_EXCLAME);
            } else {
              relRotations[ind].camera = iset;
              relRotations[ind].fromMag = itemInt[1];
              relRotations[ind].toMag = itemInt[2];
              relRotations[ind].rotation = itemDbl[3];
              mShiftManager->SetNumRelRotations(ind + 1);
            }


          } else if (MatchNoCase("RotationStretchXform")) {
            rotXform.camera = iset;
            rotXform.magInd = itemInt[1];
            rotXform.mat.xpx = itemFlt[2];
            rotXform.mat.xpy = itemFlt[3];
            rotXform.mat.ypx = itemFlt[4];
            rotXform.mat.ypy = itemFlt[5];
            rotXformArray->Add(rotXform);
          } else if (MatchNoCase("DoseRateTable")) {
            nMags = itemInt[1];
            scale = itemEmpty[2] ? 1.f : itemFlt[2];
            if (nMags <= 0 || scale <= 0) {
              message.Format("Incorrect DoseRateTable entry for camera %d: %s",
                iset, strLine);
              AfxMessageBox(message, MB_EXCLAME);

            } else {
              err = 0;
              for (ind = 0; ind < nMags; ind++) {
                if (ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl,
                  itemFlt, MAX_TOKENS) || itemEmpty[1]) {
                  err++;
                } else {
                  camP->doseTabCounts.push_back(itemFlt[0] / scale);
                  camP->doseTabRates.push_back(itemFlt[1]);
                }
              }
              if (err) {
                message.Format("%d errors occurred reading the Dose Rate table for "
                  "camera %d", err, iset);
                AfxMessageBox(message, MB_EXCLAME);
              }
            }

            // 4/24/06: make image criteria be user settings

          } else if (!strItems[0].IsEmpty()) {
            // Unrecognized camera parameter
            message.Format("Unrecognized entry in properties file %s\n"
              "for camera %d: %s\n\nCheck the help to see if it is a general property"
              " instead of a camera property", strFileName, iset, strLine);
            AfxMessageBox(message, MB_EXCLAME);
          }

        }
        if (err > 0) {
          retval = 1;
          break;
        }
      } else
        recognizedc = false;

#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#define INT_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemInt[1]);
#define BOOL_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemInt[1] != 0);
#define FLOAT_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemFlt[1]);
#define DBL_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemDbl[1]);
      if (recognizedc) {

      } else if (MatchNoCase("NumberOfCameras") || MatchNoCase("DigitalMicrographVersion")
        || MatchNoCase("FileOptionsPixelsTruncatedLo") ||
        MatchNoCase("FileOptionsPixelsTruncatedHi") || MatchNoCase("ScopeIsFEI") ||
        MatchNoCase("ScopeIsJEOL"))
        err = 0;
#define PROP_TEST_SECT1
#include "PropertyTests.h"
#undef PROP_TEST_SECT1
      else if (MatchNoCase("GainNormalizeInSerialEM"))
        mWinApp->SetProcessHere(itemInt[1] != 0);
      else
        recognized = false;

      if (recognized || recognizedc) {
        recognized = true;
      }

      else if (MatchNoCase("ReferenceMemoryLimitMB"))
        camera->SetRefMemoryLimit(1000000 * itemInt[1]);
      else if (MatchNoCase("DarkRefMaxMeanOrSD")) {
        camera->SetDarkMaxMeanCrit(itemFlt[1]);
        if (!itemEmpty[2])
          camera->SetDarkMaxSDcrit(itemFlt[2]);
      } else if (MatchNoCase("OneViewMinExposures")) {
        StoreFloatsPerBinning(strItems, "OneView minimum exposure", -1, strFileName,
          camera->GetOneViewMinExposure(0));
      } else if (MatchNoCase("OneViewDeltaExposures")) {
        StoreFloatsPerBinning(strItems, "OneView exposure increment", -1, strFileName,
          camera->GetOneViewDeltaExposure(0));
      } else if (MatchNoCase("RioMinExposures")) {
        StoreFloatsPerBinning(strItems, "Rio minimum exposure", -1, strFileName,
          camera->GetOneViewMinExposure(1));
      } else if (MatchNoCase("RioDeltaExposures")) {
        StoreFloatsPerBinning(strItems, "Rio exposure increment", -1, strFileName,
          camera->GetOneViewDeltaExposure(1));
      } else if (MatchNoCase("OneViewLikeMinExposures") ||
        MatchNoCase("OneViewLikeDeltaExposures")) {
        index = itemInt[1];
        if (index < 1 || index > MAX_1VIEW_TYPES) {
          message.Format("OneView type value of %d is out of range in %s entry\n"
            "in properties file %s", index, strItems[0], strFileName);
          AfxMessageBox(message, MB_EXCLAME);
        } else {
          if (MatchNoCase("OneViewLikeMinExposures"))
            StoreFloatsPerBinning(&strItems[1], "OneView-like minimum exposure", -1,
              strFileName, camera->GetOneViewMinExposure(index - 1));
          else
            StoreFloatsPerBinning(&strItems[1], "OneView-like exposure increment", -1,
              strFileName, camera->GetOneViewDeltaExposure(index - 1));
        }
      } else if (MatchNoCase("K2FilterName")) {
        ind = camera->GetNumK2Filters();
        if (ind < MAX_K2_FILTERS && ind > 0) {
          StripItems(strLine, 1, K2FilterNames[ind]);
          camera->SetNumK2Filters(ind + 1);
        } else
          AfxMessageBox("Too many K2/K3 filter names for arrays", MB_EXCLAME);
      } else if (MatchNoCase("K2SuperResReference")) {
        StripItems(strLine, 1, message);
        camera->SetSuperResRef(message);
      } else if (MatchNoCase("K2CountingReference")) {
        StripItems(strLine, 1, message);
        camera->SetCountingRef(message);
      } else if (MatchNoCase("K2PackRawFramesDefault")) {
        if (camera->GetSaveRawPacked() < 0)
          camera->SetSaveRawPacked(itemInt[1] != 0 ? 1 : 0);
      } else if (MatchNoCase("DEProtectionCoverOpen"))
        mWinApp->mDEToolDlg.SetProtCoverChoice(itemInt[1] ? 0 : 1);
      else if (MatchNoCase("FalconFrameConfigFile")) {
        StripItems(strLine, 1, message);
        camera->SetFalconFrameConfig(message);
      } else if (MatchNoCase("FalconConfigFile")) {
        StripItems(strLine, 1, message);
        camera->SetFalconConfigFile(message);
      } else if (MatchNoCase("LocalFalconFramePath")) {
        StripItems(strLine, 1, message);
        camera->SetLocalFalconFramePath(message);
      } else if (MatchNoCase("FalconReferenceDir")) {
        StripItems(strLine, 1, message);
        camera->SetFalconReferenceDir(message);

      } else if (MatchNoCase("DefaultGIFCamera"))
        camera->SetDefaultGIFCamera(itemInt[1]);
      else if (MatchNoCase("DefaultRegularCamera"))
        camera->SetDefaultRegularCamera(itemInt[1]);
      else if (MatchNoCase("DefaultCameraDivide16BitBy2")) {
        if (camera->GetDivideBy2() < 0)
          camera->SetDivideBy2(itemInt[1]);
      } else if (MatchNoCase("StartCameraInDebugMode"))
        mWinApp->SetStartCameraInDebug(itemInt[1] != 0);
      else if (MatchNoCase("ExitOnBadScopeStartup"))
        mWinApp->SetExitOnScopeError(itemInt[1] != 0);

      else if (MatchNoCase("DigiScanFlipAndRotation")) {
        camera->SetDSshouldFlip(itemInt[1]);
        camera->SetDSglobalRotOffset(itemFlt[2]);
      } else if (MatchNoCase("GatanServerIP"))
        CBaseSocket::SetServerIP(GATAN_SOCK_ID, strItems[1]);
      else if (MatchNoCase("GatanServerPort"))
        CBaseSocket::SetServerPort(GATAN_SOCK_ID, itemInt[1]);
      else if (MatchNoCase("FEISEMServerIP"))
        CBaseSocket::SetServerIP(FEI_SOCK_ID, strItems[1]);
      else if (MatchNoCase("FEISEMServerPort"))
        CBaseSocket::SetServerPort(FEI_SOCK_ID, itemInt[1]);
#ifdef _WIN64
      else if (MatchNoCase("SocketServerIP") || MatchNoCase("SocketServerIPif64")) {
#else
      else if (MatchNoCase("SocketServerIPif64")) {
      } else if (MatchNoCase("SocketServerIP")) {
#endif 
        if (itemInt[1] < 0 || itemEmpty[2])
          AfxMessageBox("The SocketServerIP property needs two values: a socket"
            " ID and the IP address", MB_EXCLAME);
        CBaseSocket::SetServerIP(itemInt[1], strItems[2]);
#ifdef _WIN64
      } else if (MatchNoCase("SocketServerPort") || MatchNoCase("SocketServerPortIf64")) {
#else
      } else if (MatchNoCase("SocketServerPortIf64")) {
      } else if (MatchNoCase("SocketServerPort")) {
#endif
        if (itemInt[1] < 0 || itemEmpty[2])
          AfxMessageBox("The SocketServerPort property needs two values: a socket"
            " ID and the port number", MB_EXCLAME);
        CBaseSocket::SetServerPort(itemInt[1], itemInt[2]);
      } else if (MatchNoCase("InitialCurrentCamera"))
        mWinApp->SetInitialCurrentCamera(itemInt[1]);
      else if (MatchNoCase("KeepEFTEMstate"))
        mWinApp->SetKeepEFTEMstate(itemInt[1] != 0);
      else if (MatchNoCase("KeepSTEMstate"))
        mWinApp->SetKeepSTEMstate(itemInt[1] != 0);
      else if (MatchNoCase("NoCameras"))
        mWinApp->SetNoCameras(itemInt[1] != 0);
      else if (MatchNoCase("DebugOutput"))
        mWinApp->SetDebugOutput(strItems[1]);
      else if (MatchNoCase("ActiveCameraList")) {
        index = 0;
        while (!strItems[index + 1].IsEmpty() && index < MAX_CAMERAS) {
          activeList[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
        mWinApp->SetActiveCamListSize(index);

      } else if (MatchNoCase("IgnoreGatanCameras") || MatchNoCase("IgnoreSocketCameras")
        || MatchNoCase("IgnoreAMTCameras")) {
        ind = 0;
        if (MatchNoCase("IgnoreSocketCameras"))
          ind = 1;
        if (MatchNoCase("IgnoreAMTCameras"))
          ind = 2;
        index = 0;
        kvList = camera->GetIgnoreDMList(ind);
        while (!strItems[index + 1].IsEmpty() && index < MAX_IGNORE_GATAN) {
          kvList[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
        camera->SetNumIgnoreDM(ind, index);

      } else if (MatchNoCase("DisableOrHideFile")) {
        StripItems(strLine, 1, message);
        mCheckForComments = false;
        ReadDisableOrHideFile(message, mWinApp->GetIDsToHide(), mWinApp->GetLineHideIDs(),
          mWinApp->GetIDsToDisable(), mWinApp->GetHideStrings());
        mCheckForComments = true;

      } else if (MatchNoCase("BasicModeDisableHideFile")) {
        StripItems(strLine, 1, message);
        if (mDocWnd->GetBasicModeFile().IsEmpty())
          mDocWnd->SetBasicModeFile(message);

      } else if (MatchNoCase("ProgramTitleText")) {
        StripItems(strLine, 1, message);
        mWinApp->SetProgramTitleText(message);

      } else if (MatchNoCase("ScriptMonospaceFont")) {
        StripItems(strLine, 1, message);
        mWinApp->mMacroProcessor->SetMonoFontName(message);

      } else if (MatchNoCase("PythonModulePath")) {
        StripItems(strLine, 1, message);
        mWinApp->mMacroProcessor->SetPyModulePath(message);

      } else if (MatchNoCase("PythonIncludePath")) {
        StripItems(strLine, 1, message);
        mWinApp->mMacroProcessor->SetPyIncludePath(message);

      } else if (MatchNoCase("PathToPython")) {
        if (strItems[2].IsEmpty()) {
          AfxMessageBox("Entry must contain a version number then the path in properties"
            " file " + strFileName + " : " + strLine, MB_EXCLAME);
        } else {
          StripItems(strLine, 2, message);
          mWinApp->mMacroProcessor->SetPathToPython(strItems[1], message);
        }

      } else if (MatchNoCase("ControlSetName")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_CONSETS || strItems[2].IsEmpty())
          AfxMessageBox("Index out of range or missing control set name in properties"
            " file " + strFileName + " : " + strLine, MB_EXCLAME);
        else
          mModeNames[index] = strItems[2];

      } else if (MatchNoCase("SearchAreaName")) {
        if (strItems[1].IsEmpty())
          AfxMessageBox("Missing area name in properties file " + strFileName +
            " : " + strLine, MB_EXCLAME);
        else
          mWinApp->mLowDoseDlg.SetSearchName(strItems[1]);
      } else if (MatchNoCase("FileOptionsMode")) {
        B3DCLAMP(itemInt[1], 0, 2);
        if (itemInt[1] > 1)
          itemInt[1] = MRC_MODE_USHORT;
        defFileOpt->mode = itemInt[1];
      } else if (MatchNoCase("STEMFileMode")) {
        B3DCLAMP(itemInt[1], 0, 2);
        if (itemInt[1] > 1)
          itemInt[1] = MRC_MODE_USHORT;
        mDocWnd->SetSTEMfileMode(itemInt[1]);
      } else if (MatchNoCase("FileOptionsExtraFlags"))
        defFileOpt->typext = (short)itemInt[1];
      else if (MatchNoCase("FileOptionsMaxSections"))
        defFileOpt->maxSec = itemInt[1];
      else if (MatchNoCase("FileOptionsUnsignedOption"))
        defFileOpt->unsignOpt = itemInt[1];
      else if (MatchNoCase("FileOptionsSignToUnsignOption"))
        defFileOpt->signToUnsignOpt = itemInt[1];
      else if (MatchNoCase("FileOptionsFileType"))
        defFileOpt->fileType = B3DCHOICE(itemInt[1] > 1, STORE_TYPE_HDF,
          itemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC);
      else if (MatchNoCase("MontageAutodocOptions")) {
        defFileOpt->separateForMont = itemInt[1] != 0;
        defFileOpt->montUseMdoc = itemInt[1] % 2 != 0;
        defFileOpt->montFileType = (itemInt[1] / 2) % 2 != 0 ? STORE_TYPE_ADOC :
          STORE_TYPE_MRC;
        if (itemInt[1] == 8 || itemInt[1] == 9)
          defFileOpt->montFileType = STORE_TYPE_HDF;
      } else if (MatchNoCase("TotalMemoryLimitMB"))
        mWinApp->SetMemoryLimit((float)(1000000. * itemInt[1]));
      else if (MatchNoCase("NoScopeControlOnStartup"))
        mWinApp->SetShowRemoteControl(false);

      else if (MatchNoCase("SingleTecnaiObject")) {
      } else if (MatchNoCase("CookerScreenTiltDelay")) {
      } else if (MatchNoCase("AcquireAtItemsExtraTasks")) {
        for (index = 1; index < MAX_TOKENS; index++) {
          if (itemEmpty[index])
            break;
          extraTasks->push_back(itemInt[index]);
        }
      } else if (MatchNoCase("BackgroundSocketToFEI")) {
        SetNumFEIChannels(itemInt[1] ? 4 : 3);
      } else if (MatchNoCase("SkipUtapiServices")) {
        ShortVec *skips = mWinApp->mScope->GetSkipUtapiServices();
        for (index = 1; index < MAX_TOKENS; index++) {
          if (itemEmpty[index])
            break;
          skips->push_back(itemInt[index]);
        }
      } else if (MatchNoCase("UseTEMScripting")) {
        scope->SetUseTEMScripting(itemInt[1]);
        //JEOLscope = false;
      } else 
        recognized15 = false;
      
      if (recognized || recognized15) {
        recognized = true;
      }
#define PROP_TEST_SECT2
#include "PropertyTests.h"
#undef PROP_TEST_SECT2
      else if (MatchNoCase("IlluminatedAreaLimits"))
        scope->SetIllumAreaLimits(itemFlt[1], itemFlt[2]);
      else if (MatchNoCase("C2ApertureSizes")) {
        ind = mWinApp->mBeamAssessor->GetNumC2Apertures();
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty() || ind >= MAX_SPOT_SIZE)
            break;
          C2apertures[ind++] = itemInt[index];
        }
        mWinApp->mBeamAssessor->SetNumC2Apertures(ind);

      } else if (MatchNoCase("ApertureSizes")) {
        ShortVec sizes;
        for (index = 1; index < MAX_TOKENS; index++) {
          if (itemEmpty[index])
            break;
          sizes.push_back(itemInt[index]);
        }
        apertureSizes->push_back(sizes);

      } else if (MatchNoCase("LowestSTEMnonLMmag")) {
        scope->SetLowestSTEMnonLMmag(0, itemInt[1]);
        if (!itemEmpty[2])
          scope->SetLowestSTEMnonLMmag(1, itemInt[2]);
      } else if (MatchNoCase("OtherShiftBoundaries")) {
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty())
            break;
          scope->AddShiftBoundary(atoi((LPCTSTR)strItems[index]));
        }

      } else if (MatchNoCase("BeamShiftBoundaries")) {
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty())
            break;
          bsBoundaries->push_back(itemInt[index]);
        }

      } else if (MatchNoCase("BeamShiftCalAlphaFactors")) {
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty())
            break;
          alphaFacs[index - 1] = itemFlt[index];
        }

      } else if (MatchNoCase("STEMdefocusToDeltaZ"))
        mWinApp->mFocusManager->SetSTEMdefocusToDelZ(itemFlt[1],itemFlt[1]);
      else if (MatchNoCase("IntensityToC2Factor")) {
      }
      else if (MatchNoCase("StageLimits")) {

        // Stage limits: allow any number of values, 0 for default
        for (index = 0; index < 4; index++) {
          if (strItems[index + 1].IsEmpty())
            break;
          float value = (float)atof(strItems[index + 1]);
          if (value)
            mWinApp->mScope->SetStageLimit(index, value);
        }

      } else if (MatchNoCase("WatchGauge")) {
        if (strItems[3].IsEmpty()) {
          AfxMessageBox("Missing threshold values in properties file " + strFileName 
            + " : " + strLine , MB_EXCLAME);
          break;
        }
        if (scope->mNumGauges == MAX_GAUGE_WATCH) {
          AfxMessageBox("Too many gauges to watch in properties file " + strFileName,
            MB_EXCLAME);
          break;
        }
        scope->mGaugeNames[scope->mNumGauges] = strItems[1];
        scope->mYellowThresh[scope->mNumGauges] = itemDbl[2];
        scope->mRedThresh[scope->mNumGauges++] = itemDbl[3];

      } else if (MatchNoCase("ShowIntensityCalStatus")) {
        mWinApp->mScopeStatus.SetShowIntensityCal(itemInt[1] != 0);
      }
      else if (MatchNoCase("PiezoScaling")) {
        if (itemEmpty[6]) {
          AfxMessageBox("Not enough values on line in properties file " + strFileName 
            + " : " + strLine , MB_EXCLAME);
        } else {
          pzScale.plugNum = itemInt[1];
          pzScale.piezoNum = itemInt[2];
          pzScale.zAxis = itemInt[3];
          pzScale.minPos = itemFlt[4];
          pzScale.maxPos = itemFlt[5];
          pzScale.unitsPerMicron = itemFlt[6];
          piezoScalings->Add(pzScale);
        }
      } else if (MatchNoCase("UsePiezoForLDAxis")) {
        mWinApp->mLowDoseDlg.SetAxisPiezoPlugNum(itemInt[1]);
        mWinApp->mLowDoseDlg.SetAxisPiezoNum(itemInt[2]);
      } else if (MatchNoCase("PiezoFullDelay")) {
        mWinApp->mLowDoseDlg.SetPiezoFullDelay(itemFlt[1]);
        if (!itemEmpty[2])
          mWinApp->mLowDoseDlg.SetFocusPiezoDelayFac(itemFlt[2]);
        if (!itemEmpty[3])
          mWinApp->mLowDoseDlg.SetTVPiezoDelayFac(itemFlt[3]);
      }
      else if (MatchNoCase("PanelConstants")) {
        mWinApp->SetToolTitleHeight(itemInt[1]);
        mWinApp->SetToolExtraHeight(itemEmpty[2] ? 0 : itemInt[2]);
        mWinApp->SetToolExtraWidth(itemEmpty[3] ? 0 : itemInt[3]);
      }
      else if (MatchNoCase("AstigmatismComaDelays")) {
        mWinApp->mAutoTuning->SetPostAstigDelay(B3DMAX(1, itemInt[1]));
        if (!itemEmpty[2])
          mWinApp->mAutoTuning->SetPostFocusDelay(B3DMAX(1, itemInt[2]));
        if (!itemEmpty[3])
          mWinApp->mAutoTuning->SetPostBeamTiltDelay(B3DMAX(1, itemInt[2]));
      } else if (MatchNoCase("AstigmatismParams")) {
        mWinApp->mAutoTuning->SetAstigBeamTilt(itemFlt[1]);
        if (!itemEmpty[2])
          mWinApp->mAutoTuning->SetAstigToApply(itemFlt[2]);
        if (!itemEmpty[3])
          mWinApp->mAutoTuning->SetAstigIterationThresh(itemFlt[3]);
        if (!itemEmpty[4])
          mWinApp->mAutoTuning->SetAstigCenterFocus(itemFlt[4]);
        if (!itemEmpty[5])
          mWinApp->mAutoTuning->SetAstigFocusRange(itemFlt[5]);
      } else if (MatchNoCase("ComaParams")) {
        mWinApp->mAutoTuning->SetMaxComaBeamTilt(itemFlt[1]);
        if (!itemEmpty[2])
          mWinApp->mAutoTuning->SetCalComaFocus(itemFlt[2]);
      } else if (MatchNoCase("ZbyGFocusScalings")) {
        vec = mWinApp->mParticleTasks->GetZBGFocusScalings();
        vec->clear();
        for (index = 1; index < MAX_TOKENS; index++) {
          if (itemEmpty[index])
            break;
          vec->push_back(itemFlt[index]);
        }
      } else if (MatchNoCase("MontageInitialPieces")) {
        mDocWnd->SetDefaultMontXpieces(itemInt[1]);
        if (itemInt[2] > 0)
          mDocWnd->SetDefaultMontYpieces(itemInt[2]);
      }
      else if (MatchNoCase("StageMontageMaxError")) {
        mWinApp->mMontageController->SetMaxStageError(itemFlt[1]);
        mWinApp->mMontageController->SetStopOnStageError(itemInt[2] != 0);
      }
      else if (MatchNoCase("MontageFilterR1R2S1S2") || 
        MatchNoCase("MontFilterSet2R1R2S1S2")) {
          index = MatchNoCase("MontageFilterR1R2S1S2") ? 0 : 1;
          if (itemEmpty[4])
            AfxMessageBox("Four numbers are required in properties file " + strFileName 
            + " for line: " + strLine , MB_EXCLAME);
          else
            mWinApp->mMontageController->SetXcorrFilter(index, itemFlt[1], 
            itemFlt[2], itemFlt[3], itemFlt[4]);
      } else
        recognized2 = false;

      if (recognized || recognized2) {
        recognized = true;
      }
#define PROP_TEST_SECT30
#include "PropertyTests.h"
#undef PROP_TEST_SECT30
      else
        recognized30 = false;

      if (recognized || recognized30) {
        recognized = true;
      }
#define PROP_TEST_SECT35
#include "PropertyTests.h"
#undef PROP_TEST_SECT35

      else if (MatchNoCase("JeolSwitchSTEMsleep")) {
        scope->SetJeolSwitchSTEMsleep(itemInt[1]);
        if (!itemEmpty[2])
          scope->SetJeolSwitchTEMsleep(itemInt[2]);
      } else if (MatchNoCase("JeolControlsBeamValve"))
        scope->SetNoColumnValve(itemInt[1] == 0);
      else if (MatchNoCase("JeolIndexForMagMode"))
        CEMscope::SetJeolIndForMagMode(itemInt[1]);
      else if (MatchNoCase("JeolPostMagChangeDelay")) {
        index = 0;
        if (!strItems[2].IsEmpty())
          index = itemInt[2];
        scope->SetJeolPostMagDelay(itemInt[1], index);

      } else if (MatchNoCase("JeolLensRelaxProgram")) {
        ind = sizeof(lensNormMap) / sizeof(short);
        if (itemInt[1] < 1 || itemInt[1] > ind) {
          message.Format("Index value (%d) must be between 1 and %d in property "
            "line:\n", itemInt[1], ind);
          AfxMessageBox(message + strLine, MB_EXCLAME);
        } else {
          relax.normIndex = lensNormMap[itemInt[1] - 1];
          relax.numLens = 0;
          relax.numSteps = (short)itemInt[2];
          relax.delay = itemInt[3];
          for (ind = 0; ind < MAX_LENS_TO_RELAX; ind++) {
            if (itemEmpty[2 * ind + 5])
              break;
            relax.lensTypes[relax.numLens] = (short)itemInt[2 * ind + 4];
            relax.amplitudes[relax.numLens++] = itemFlt[2 * ind + 5];
          }
          if (!itemEmpty[2 * ind + 5]) {
            message.Format("More that %d lenses were entered in property line:\n%s\n\n"
              "Request that developers make the array size larger",
              MAX_LENS_TO_RELAX, (LPCTSTR)strLine);
            AfxMessageBox(message);
          }
          relaxData->Add(relax);
        }

      } else if (MatchNoCase("JeolLowDoseFLCSeries")) {
        if (itemInt[1] < 0 || itemInt[1] > SEARCH_AREA) {
          message.Format("Low dose set index (%d) must be between 0 and 2 in property "
            "line:\n", itemInt[1]);
          AfxMessageBox(message + strLine, MB_EXCLAME);
        } else if (itemInt[2] < 0 || itemInt[2] > SEARCH_AREA) {
          message.Format("Low dose area value (%d) must be between 0 and 4 in property "
            "line:\n", itemInt[2]);
          AfxMessageBox(message + strLine, MB_EXCLAME);
        } else {
          FLCseq.setIndex = (short)itemInt[1];
          FLCseq.ldArea = (short)itemInt[2];
          FLCseq.numLens = 0;
          for (ind = 0; ind < MAX_FLC_FOR_AREA; ind++) {
            if (itemEmpty[2 * ind + 4])
              break;
            FLCseq.lens[FLCseq.numLens] = (short)itemInt[2 * ind + 3];
            FLCseq.value[FLCseq.numLens++] = itemFlt[2 * ind + 4];
          }
          if (!itemEmpty[2 * ind + 4]) {
            message.Format("More that %d lenses were entered in property line:\n%s\n\n"
              "Request that developers make the array size larger",
              MAX_FLC_FOR_AREA, (LPCTSTR)strLine);
            AfxMessageBox(message);
          }
          FLCsequences->Add(FLCseq);
        }

      } else if (MatchNoCase("ImageDetectorIDs")) {
        index = 1;
        while (!itemEmpty[index])
          scope->AddImageDetector(itemInt[index++]);
      }
      else if (MatchNoCase("MappedToDSchannel") || MatchNoCase("DetectorBlocks") || 
        MatchNoCase("MutuallyExcludeDetectors")) {
          index = 1;
          chanSet.mappedTo = -1;
          chanSet.numChans = 0;
          if (!MatchNoCase("MutuallyExcludeDetectors")) {
            chanSet.mappedTo = itemInt[1];
            index = 2;
          }
          while (!itemEmpty[index] && chanSet.numChans < MAX_STEM_CHANNELS)
            chanSet.channels[chanSet.numChans++] = itemInt[index++];
          if (chanSet.numChans) {
            if (MatchNoCase("DetectorBlocks"))
              blockSets->Add(chanSet);
            else
              channelSets->Add(chanSet);
          }

      } else if (MatchNoCase("CEOSServerIP"))
        camera->SetCEOSserverIP(strItems[1]);
      else if (MatchNoCase("EDMServerIP"))
        camera->SetEDMserverIP(strItems[1]);

      // Hitachi
      else if (MatchNoCase("HitachiIPaddress"))
        hitachi->IPaddress = strItems[1];
      else if (MatchNoCase("HitachiPort"))
        hitachi->port = strItems[1];
      else if (MatchNoCase("HitachiBeamBlankAxis"))
        hitachi->beamBlankAxis = itemInt[1];
      else if (MatchNoCase("HitachiBeamBlankDelta"))
        hitachi->beamBlankDelta = itemFlt[1];
      else if (MatchNoCase("HitachiPAforHRmodeIS"))
        hitachi->usePAforHRmodeIS = itemInt[1];
      else if (MatchNoCase("HitachiTiltSpeed"))
        hitachi->stageTiltSpeed = itemInt[1];
      else if (MatchNoCase("HitachiStageSpeed"))
        hitachi->stageXYSpeed = itemInt[1];
      else if (MatchNoCase("HitachiImageShiftToMicrons"))
        hitachi->imageShiftToUm = itemFlt[1];
      else if (MatchNoCase("HitachiBeamShiftToMicrons"))
        hitachi->beamShiftToUm = itemFlt[1];
      else if (MatchNoCase("HitachiObjectiveToMicrons")) {
        hitachi->objectiveToUm[0] = itemFlt[1];
        hitachi->objectiveToUm[2] = itemFlt[1];
        if (!itemEmpty[2])
          hitachi->objectiveToUm[2] = itemFlt[2];
      } else if (MatchNoCase("HitachiI1ToMicrons"))
        hitachi->objectiveToUm[1] = itemFlt[1];
      else if (MatchNoCase("HitachiScreenArea"))
        hitachi->screenAreaSqCm = itemFlt[1];


      else if (MatchNoCase("WalkUpMaxInterval") || MatchNoCase("WalkUpMinInterval"))
        err = 0;
      else
        recognized35 = false;

      if (recognized || recognized35) {
        recognized = true;
      }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
      else
        recognized4 = false;

      if (recognized || recognized4) {
        recognized = true;
      } else if (MatchNoCase("TSXFitInterval"))
        mTSParam->fitIntervalX = itemFlt[1];
      else if (MatchNoCase("TSYFitInterval"))
        mTSParam->fitIntervalY = itemFlt[1];
      else if (MatchNoCase("TSZFitInterval"))
        mTSParam->fitIntervalZ = itemFlt[1];
      else if (MatchNoCase("TSXMinForQuadratic"))
        mTSParam->minFitXQuadratic = itemInt[1];
      else if (MatchNoCase("TSYMinForQuadratic"))
        mTSParam->minFitYQuadratic = itemInt[1];
      else if (MatchNoCase("TSBidirAnchorMinField")) {
        tsController->SetMinFieldBidirAnchor(itemFlt[1]);
        if (!itemEmpty[2])
          tsController->SetAlarmBidirFieldSize(itemFlt[2]);
      } else if (MatchNoCase("BeamCalMinCurrent") || MatchNoCase("BeamCalMinFactor") ||
        MatchNoCase("BeamCalMaxFactor") || MatchNoCase("BeamCalPostSettingDelay") ||
               MatchNoCase("BeamCalBigSettingDelay") || MatchNoCase("BeamCalNumReadingsLo")
        || MatchNoCase("BeamCalInterReadingDelay") ||
               MatchNoCase("BeamCalNumReadingsHi") || MatchNoCase("BeamCalExtraRangeAtMinMag")
        || MatchNoCase("BeamCalAveragingLoHiCriterion") ||
        MatchNoCase("BeamCalMinMagIndNeeded"))
        err = 0;

      else if (MatchNoCase("DMAlignZLPCriteria")) {
        camera->SetNumZLPAlignChanges(itemInt[1]);
        if (!itemEmpty[2])
          camera->SetMinZLPAlignInterval(itemFlt[2]);
      } else if (MatchNoCase("FilterPixelMatchingFactor"))
        mFilterParam->binFactor = itemFlt[1];
      else if (MatchNoCase("MaximumSlitWidth"))
        mFilterParam->maxWidth = itemFlt[1];
      else if (MatchNoCase("GIFadjustsForSlitWidth"))
        mFilterParam->adjustForSlitWidth = itemInt[1] == 0;
      else if (MatchNoCase("OmegaCanDoNegativeShift"))
        mFilterParam->positiveLossOnly = itemInt[1] > 0 ? 0 : 1;
      else if (MatchNoCase("EnergyShiftCalLowestMag"))
        err = 0;
      else if (MatchNoCase("PrintStartupInfo"))
        mWinApp->SetStartupInfo(itemInt[1] != 0);
      else if (MatchNoCase("NoExceptionHandler"))
        mWinApp->SetNoExceptionHandler(itemInt[1]);
      else if (MatchNoCase("CatalaseAxisLengths")) {
        mWinApp->mProcessImage->SetShortCatalaseNM(itemFlt[1]);
        mWinApp->mProcessImage->SetLongCatalaseNM(itemFlt[2]);

      } else if (MatchNoCase("GainReferencePath")) {
        StripItems(strLine, 1, message);
        mWinApp->mGainRefMaker->SetRefPath(message);

      } else if (MatchNoCase("DigitalMicrographReferencePath")) {
        StripItems(strLine, 1, message);
        mWinApp->mGainRefMaker->SetDMRefPath(message);
      } else if (MatchNoCase("RemoteDMReferencePath")) {
        StripItems(strLine, 1, message);
        mWinApp->mGainRefMaker->SetRemoteRefPath(message);

      } else if (MatchNoCase("SeparateGainReferenceKVs")) {
        kvList = mWinApp->mGainRefMaker->GetRefKVList();
        for (ind = 0; ind < MAX_GAINREF_KVS; ind++) {
          if (strItems[ind + 1].IsEmpty())
            break;
          kvList[ind] = atoi((LPCTSTR)strItems[ind + 1]);
        }
        mWinApp->mGainRefMaker->SetNumRefKVs(ind);

      } else if (MatchNoCase("MRCHeaderTitle")) {
        StripItems(strLine, 1, message);
        message = message.Left(45);
        mDocWnd->SetTitle(message);

      } else if (MatchNoCase("FrameFileTitle")) {
        StripItems(strLine, 1, message);
        message.Replace("\\n", "\n");
        mDocWnd->SetFrameTitle(message);

      } else if (MatchNoCase("GlobalAutodocEntry")) {
        ind = 2;
        if (strItems[2] == "=")
          ind = 3;
        if (strItems[ind].IsEmpty()) {
          AfxMessageBox("Invalid key-value pair entry in property entry:\n\n" + strLine,
            MB_EXCLAME);
        } else {
          StripItems(strLine, ind, message);
          globalKeys->Add(strItems[1]);
          globalValues->Add(message);
        }

      } else if (MatchNoCase("LogBookPathName")) {
        StripItems(strLine, 1, message);
        mDocWnd->SetLogBook(message);

      } else if (MatchNoCase("StartupMessage")) {
        StripItems(strLine, 1, message);
        message.Replace("\\n", "\r\n");
        mWinApp->SetStartupMessage(message);

      } else if (MatchNoCase("ErrorColor") || MatchNoCase("WarningColor") ||
        MatchNoCase("InsertedColor") || MatchNoCase("DebugColor") || 
        MatchNoCase("VerboseColor")) {
        ind = ERROR_COLOR_IND;
        if (MatchNoCase("WarningColor"))
          ind = WARNING_COLOR_IND;
        else if (MatchNoCase("InsertedColor"))
          ind = INSERTED_COLOR_IND;
        else if (MatchNoCase("DebugColor"))
          ind = DEBUG_COLOR_IND;
        else if (MatchNoCase("VerboseColor"))
          ind = VERBOSE_COLOR_IND;
        if (itemEmpty[3]) {
          B3DCLAMP(itemInt[1], 0, MAX_STOCK_COLORS - 1);
          for (index = 0; index < 3; index++)
            palette[ind * 3 + index] = palette[itemInt[1] * 3 + index];
        } else {
          for (index = 0; index < 3; index++) {
            B3DCLAMP(itemInt[index + 1], 0, 255);
            palette[ind * 3 + index] = itemInt[index + 1];
          }
        }

      } else if (MatchNoCase("PluginPath")) {
        StripItems(strLine, 1, message);
        mDocWnd->SetPluginPath(message);

      } else if (MatchNoCase("PluginPath2")) {
        StripItems(strLine, 1, message);
        mDocWnd->SetPluginPath2(message);
      } else if (MatchNoCase("PluginPath2-32")) {
#ifndef _WIN64
        StripItems(strLine, 1, message);
        mDocWnd->SetPluginPath2(message);
#endif
      } else if (MatchNoCase("PluginPath2-64")) {
#ifdef _WIN64
        StripItems(strLine, 1, message);
        mDocWnd->SetPluginPath2(message);
#endif

      } else if (MatchNoCase("ExternalTool")) {
        StripItems(strLine, 1, message);
        mWinApp->mExternalTools->AddTool(message);
      
      } else if (MatchNoCase("ToolCommand")) {
        if (itemEmpty[2]) {
          AfxMessageBox("ToolCommand property entry must have a number then a command");
        } else {
          StripItems(strLine, 2, message);
          mWinApp->mExternalTools->AddCommand(itemInt[1] - 1, message);
        }

      } else if (MatchNoCase("ToolArguments")) {
         if (itemEmpty[2]) {
          AfxMessageBox("ToolArguments property entry must have a number then an argument"
            " list");
        } else {
          StripItems(strLine, 2, message);
          mWinApp->mExternalTools->AddArgString(itemInt[1] - 1, message);
        }

      } else if (MatchNoCase("CtfplotterPath")) {
        StripItems(strLine, 1, message);
        mWinApp->mExternalTools->SetCtfplotterPath(message);

      } else if (MatchNoCase("FFTCircleRadii")) {
        for (ind = 1; ind < MAX_FFT_CIRCLES; ind++) {
          if (itemEmpty[ind])
            break;
          radii[ind - 1] = itemFlt[ind];
        }
        mWinApp->mProcessImage->SetNumCircles(ind - 1);

      } else if (MatchNoCase("FFTZeroParams")) {
       mWinApp->mProcessImage->SetSphericalAber((float)B3DMAX(0.01, itemDbl[1]));
       if (!itemEmpty[2])
         mWinApp->mProcessImage->SetNumFFTZeros(itemInt[2]);
       if (!itemEmpty[3])
         mWinApp->mProcessImage->SetAmpRatio(itemFlt[3]);

     } else if (MatchNoCase("ImportStageToImage")) {
       index = navParams->numImportXforms;
       if (index >= MAX_IMPORT_XFORMS) {
         AfxMessageBox("Too many import stage-to-image transformations for arrays",
            MB_EXCLAME);
       } else {
         navParams->importXform[index].xpx = itemFlt[1];
         navParams->importXform[index].xpy = itemFlt[2];
         navParams->importXform[index].ypx = itemFlt[3];
         navParams->importXform[index].ypy = itemFlt[4];
         if (itemEmpty[5])
           navParams->xformName[index].Format("%6g %6g %6g %6g", itemDbl[1], itemDbl[2],
           itemDbl[3], itemDbl[4]);
         else
           StripItems(strLine, 5, navParams->xformName[index]);
         navParams->numImportXforms++;
       }

     } else if (MatchNoCase("ImportTiffXField") || MatchNoCase("ImportTiffYField") || 
       MatchNoCase("ImportTiffIDField")) {
       index = itemInt[1];
       if (index < 0 || index >= MAX_IMPORT_XFORMS) {
         AfxMessageBox("Import transform # in Tiff Field definition is out of range", 
           MB_EXCLAME);
       } else {
         if (MatchNoCase("ImportTiffXField"))
           tfield = &navParams->xField[index];
         else if (MatchNoCase("ImportTiffYField"))
           tfield = &navParams->yField[index];
         else {
           tfield = &navParams->idField[index];
           navParams->xformID[index] = strItems[5];
         }
         tfield->tag = itemInt[2];
         if (strItems[3] == "S")
           tfield->type = 1;
         else if (strItems[3] == "I")
           tfield->type = 2;
         else if (strItems[3] == "F")
           tfield->type = 3;
         else if (strItems[3] == "D")
           tfield->type = 4;
         else if (strItems[3] == "C")
           tfield->type = 5;
         else
           tfield->type = itemInt[3];
         tfield->tokenNum = itemInt[4];
       }

     } else if (MatchNoCase("ImportSubtractFromTIFFStage")) {
       navParams->importXbase = itemDbl[1];
       navParams->importYbase = itemDbl[2];
     } else if (MatchNoCase("GridInPolygonBoxFraction")) {
       navParams->gridInPolyBoxFrac = itemFlt[1];
     } else if (MatchNoCase("NavigatorStageBacklash")) {
       navParams->stageBacklash = itemFlt[1];
     } else if (MatchNoCase("SamePositionTolerance")) {
        scope->SetBacklashTolerance(itemFlt[1]);
     } else if (MatchNoCase("NavigatorMaxMontageIS")) {
       navParams->maxMontageIS = itemFlt[1];
     } else if (MatchNoCase("NavigatorMaxLMMontageIS")) {
       navParams->maxLMMontageIS = itemFlt[1];
     } else if (MatchNoCase("FitMontageWithFullFrames")) {
       navParams->fitMontWithFullFrames = itemFlt[1];
     } else if (MatchNoCase("MaxReconnectsInNavAcquire")) {
       navParams->maxReconnectsInAcq = itemInt[1];
     } else if (MatchNoCase("SMTPServer")) {
       mWinApp->mMailer->SetServer(strItems[1]);
     } else if (MatchNoCase("SendMailFrom")) {
       mWinApp->mMailer->SetMailFrom(strItems[1]);
     } else if (MatchNoCase("SMTPPort")) {
       unsigned short int port = itemInt[1];
       mrc_swap_shorts((b3dInt16 *)&port, 1);
       mWinApp->mMailer->SetPort(port);

     } else if (MatchNoCase("MagnificationTable") || MatchNoCase("STEMMagTable")) {
       nMags = itemInt[1];
       col = MatchNoCase("MagnificationTable") ? 1 : 0;
       for (int line = 0; line < nMags; line++) {
         err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt, 
           MAX_TOKENS);
         if (err)
           break;
         err = 1;
         if (col && strItems[2].IsEmpty() || !col && strItems[1].IsEmpty())
           break;
         index = itemInt[0];
         if (index <= 0 || index >= MAX_MAGS)
           break;
         if (col) {
           mMagTab[index].mag = itemInt[1];
           mMagTab[index].tecnaiRotation = itemFlt[2];
           mMagTab[index].screenMag = (strItems[3].IsEmpty() || strItems[3] == "0") ?
             mMagTab[index].mag : itemInt[3];
           mMagTab[index].EFTEMmag = (strItems[4].IsEmpty() || strItems[4] == "0") ?
             mMagTab[index].mag : itemInt[4];
           mMagTab[index].EFTEMtecnaiRotation = strItems[5].IsEmpty() ?
             mMagTab[index].tecnaiRotation : itemFlt[5];
           mMagTab[index].EFTEMscreenMag = (strItems[6].IsEmpty() || strItems[6] == "0") ?
             mMagTab[index].EFTEMmag : itemInt[6];
         } else {
           mMagTab[index].STEMmag = itemDbl[1];
         }
         err = 0;
       }
       if (err > 0) {
         AfxMessageBox("Error reading magnification table, line\n" +
           strLine, MB_EXCLAME);
         retval = err;
         break;
       }

     } else if (MatchNoCase("CameraLengthTable")) {
       nMags = itemInt[1];
       for (int line = 0; line < nMags; line++) {
         err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt, 
           MAX_TOKENS);
         if (err)
           break;
         err = 1;
         if (strItems[1].IsEmpty())
           break;
         index = itemInt[0];
         if (index <= 0 || index >= MAX_CAMLENS)
           break;
         camLengths[index] = itemInt[1];
         err = 0;
         if (!strItems[2].IsEmpty())
           camLenCal[index] = itemFlt[2];
         if (!strItems[3].IsEmpty())
           diffRotations[index] = itemFlt[3];
       }
       if (err > 0) {
         AfxMessageBox("Error reading camera length table, line\n" +
           strLine, MB_EXCLAME);
         retval = err;
         break;
       } else
         mWinApp->mScope->SetNumCameraLengths(nMags, -1);

     } else if (MatchNoCase("NumberOfCameraLengths")) {
       mWinApp->mScope->SetNumCameraLengths(itemInt[1], 
         strItems[2].IsEmpty() ? -1 : itemInt[2]);

     } else if (MatchNoCase("FocusTickTable")) {
       nMags = itemInt[1];
       for (int line = 0; line < nMags; line++) {
         err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt, 
           MAX_TOKENS);
         if (err)
           break;
         err = 1;
         if (strItems[1].IsEmpty())
           break;
         index = itemInt[0];
         if (index <= 0 || index >= MAX_MAGS)
           break;
         mMagTab[index].focusTicks = itemInt[1];
         err = 0;
       }
       if (err > 0) {
         AfxMessageBox("Error reading focus tick table, line\n" +
           strLine, MB_EXCLAME);
         retval = err;
         break;
       }

     } else if (MatchNoCase("ImageShiftDelays")) {
       nMags = itemInt[1];
       if (nMags > MAX_IS_DELAYS) {
         AfxMessageBox("Too many Image Shift delays for arrays", MB_EXCLAME);
         retval = 1;
         break;
       }
       float *ISmoved = mShiftManager->GetISmoved();
       float *ISdelay = mShiftManager->GetISdelayNeeded();
       for (int line = 0; line < nMags; line++) {
         err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt, 
           MAX_TOKENS);
         if (err)
           break;
         if (strItems[1].IsEmpty()) {
           err = 1;
           break;
         }
         ISmoved[line] = itemFlt[0];
         ISdelay[line] = itemFlt[1];
       }
       if (err > 0) {
         AfxMessageBox("Error reading image shift delays, line\n" +
           strLine, MB_EXCLAME);
         retval = err;
         break;
       } else
         mShiftManager->SetNumISdelays(nMags);

     } else if (!strItems[0].IsEmpty() && !recognized && !recognized35)
       AfxMessageBox("Unrecognized entry in properties file " + strFileName
         + " : " + strLine, MB_EXCLAME);

    }
    if (err > 0)
      retval = err;

    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }

  if (!mDupMessage.IsEmpty()) {
    mDupMessage = "These items are entered more than once in property file " +
      strFileName + "\r\n   "
      "(only the last entry has any effect):\r\n" + mDupMessage;
    AfxMessageBox(mDupMessage, MB_EXCLAME);
  }
  mCheckForComments = false;

  // Put the lowest M mode mag on the boundary list once whether it is default or entered
  mWinApp->mScope->AddShiftBoundary(mWinApp->mScope->GetLowestMModeMagInd());
  return retval;
}
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#undef DBL_PROP_TEST

// Read main calibration file
int CParameterIO::ReadCalibration(CString strFileName)
{
  int retval = 0;
  int probe, tmpAper[4] = {0, 0, 0, 0}, err;
  int i, nCal, index, camera, focInd, beamInd, spot, freeInd, minSpot[2], maxSpot[2];
  BOOL oneIntensity, gotSpotAper = false;
  double intensity;
  ScaleMat IStoBS;
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  float itemFlt[MAX_TOKENS];
  BeamTable *beamTables = mWinApp->mBeamAssessor->GetBeamTables();
  BeamTable *beamTab;
  SpotTable *spotTables = mWinApp->mBeamAssessor->GetSpotTables();
  SpotTable tmpTable[2];
  int *crossCalAper = mWinApp->mBeamAssessor->GetCrossCalAperture();
  int *spotCalAper = mWinApp->mBeamAssessor->GetSpotCalAperture();
  float *alphaBeamShifts = mWinApp->mScope->GetAlphaBeamShifts();
  float *alphaBeamTilts = mWinApp->mScope->GetAlphaBeamTilts();
  double *spotBeamShifts = mWinApp->mScope->GetSpotBeamShifts();
  double *LMfocTab = mWinApp->mScope->GetLMFocusTable();
  float *lowIAlims = mWinApp->mScope->GetCalLowIllumAreaLim();
  float *highIAlims = mWinApp->mScope->GetCalHighIllumAreaLim();
  CArray<STEMFocusZTable, STEMFocusZTable> *focusZtables =
    mWinApp->mFocusManager->GetSFfocusZtables();
  CArray <AstigCalib, AstigCalib> *astigCals = mWinApp->mAutoTuning->GetAstigCals();
  AstigCalib astig;
  CArray <ComaCalib, ComaCalib> *comaCals = mWinApp->mAutoTuning->GetComaCals();
  ComaCalib coma;
  CArray <CtfBasedCalib, CtfBasedCalib> *ctfAstigCals =
    mWinApp->mAutoTuning->GetCtfBasedCals();
  CtfBasedCalib ctfCal;
  CArray<ParallelIllum, ParallelIllum> *parIllums =
    mWinApp->mBeamAssessor->GetParIllumArray();
  ParallelIllum parallelIllum;
  HighFocusCalArray *focusMagCals;
  HighFocusMagCal focCal;
  HitachiParams *hParams = mWinApp->GetHitachiParams();
  STEMFocusZTable sfZtable;
  FocusTable focTable;

  try {
    // Open the file for reading, verify that it is a calibration file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadAndParse(strLine, strItems, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (CheckForByteOrderMark(strItems[0], "SerialEMCalibrations", strFileName,
      "calibration")) {
      retval = 1;
    }

    while (retval == 0 && 
           (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                                 MAX_TOKENS)) == 0) {
      // A stub for something complex
      if (NAME_IS("CameraProperties")) {
        int iset = itemInt[1];
        while ((err = ReadAndParse(strLine, strItems, MAX_TOKENS)) == 0) {
          if (NAME_IS("EndCameraProperties"))
            break;
          else {
            // Unrecognized camera parameter      
          }

        }
        if (err > 0) {
          retval = err;
          break;
        }

      } else if (NAME_IS("ImageShiftMatrix")) {
        nCal = itemInt[1];
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          camera = itemInt[1];
          if (index < 1 || index >= MAX_MAGS || camera < 0 
            || camera >= MAX_CAMERAS) {
            err = 1;
            break;
          }
          ScaleMat *sm = &mMagTab[index].matIS[camera];
          sm->xpx = itemFlt[2];
          sm->xpy = itemFlt[3];
          sm->ypx = itemFlt[4];
          sm->ypy = itemFlt[5];
        }
        if (err > 0) {
          AfxMessageBox("Error reading image shift calibrations, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }

      } else if (NAME_IS("StageToCameraMatrix")) {
        index = itemInt[1];
        camera = itemInt[2];
        if (index < 1 || index >= MAX_MAGS || camera < 0 
          || camera >= MAX_CAMERAS || strItems[6].IsEmpty()) {
          err = 1;
        } else {
          ScaleMat *sm = &mMagTab[index].matStage[camera];
          sm->xpx = itemFlt[3];
          sm->xpy = itemFlt[4];
          sm->ypx = itemFlt[5];
          sm->ypy = itemFlt[6];
          if (!strItems[8].IsEmpty())
            mMagTab[index].stageCalFocus[camera] = itemFlt[7];
        }

      } else if (NAME_IS("HighFocusMagCal") || NAME_IS("HighFocusISCal")) {
        focusMagCals = NAME_IS("HighFocusISCal") ?
          mShiftManager->GetFocusISCals() :
          mShiftManager->GetFocusMagCals();

        focCal.spot = itemInt[1];
        focCal.probeMode = itemInt[2];
        focCal.defocus = itemFlt[3];
        focCal.intensity = itemDbl[4];
        focCal.scale = itemFlt[5];
        focCal.rotation = itemFlt[6];
        focCal.crossover = itemEmpty[7] ? 0. : itemDbl[7];
        focCal.measuredAperture = itemEmpty[8] ? 0 : itemInt[8];
        focCal.magIndex = itemEmpty[9] ? 0 : itemInt[9];
        focusMagCals->Add(focCal);

      } else if (NAME_IS("ParallelIllum")) {
        parallelIllum.intensity = itemDbl[1];
        parallelIllum.spotSize = itemInt[2];
        parallelIllum.probeOrAlpha = itemInt[3];
        parallelIllum.crossover = itemDbl[4];
        parallelIllum.measuredAperture = itemInt[5];
        parIllums->Add(parallelIllum);

      } else if (NAME_IS("IntensityToC2Factor")) {
        mWinApp->mScope->SetC2IntensityFactor(1, itemFlt[1]);
        if (!itemEmpty[2])
          mWinApp->mScope->SetC2IntensityFactor(0, itemFlt[2]);

      } else if (NAME_IS("C2SpotOffset")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_SPOT_SIZE || itemEmpty[2]) {
          err = 1;
        } else {
          mWinApp->mScope->SetC2SpotOffset(index, 1, itemFlt[2]);
          if (!itemEmpty[3])
            mWinApp->mScope->SetC2SpotOffset(index, 0, itemFlt[3]);
        }

      } else if (NAME_IS("CrossoverIntensity")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_SPOT_SIZE || itemEmpty[2])
          err = 1;
        else {
          mWinApp->mScope->SetCrossover(index, 1, itemDbl[2]);
          if (!itemEmpty[3])
            mWinApp->mScope->SetCrossover(index, 0, itemDbl[3]);
        }

      } else if (NAME_IS("CrossCalAtC2Aperture")) {
        crossCalAper[1] = itemInt[1];
        if (!itemEmpty[2])
          crossCalAper[0] = itemInt[2];
 
      } else if (NAME_IS("FocusCalibration")) {
        index = itemInt[1];
        camera = itemInt[2];
        nCal = itemInt[6];
        beamInd = 0;
        freeInd = 1;
        spot = -999;
        if (!strItems[8].IsEmpty())
          beamInd = itemInt[7];
        if (!strItems[9].IsEmpty())
          freeInd = itemInt[8];
        if (!strItems[10].IsEmpty())
          spot = itemInt[9];
        if (index < 1 || index >= MAX_MAGS || camera < 0 
          || camera >= MAX_CAMERAS || beamInd < 0 || beamInd > 3) {
          nCal = 0;
          err = 1;
        } else {

          // If mag and camera OK, build up new entry
          focTable.magInd = index;
          focTable.camera = camera;
          focTable.direction = beamInd;
          focTable.probeMode = freeInd;
          focTable.alpha = spot;
          focTable.numPoints = nCal;
          focTable.slopeX = itemFlt[3];
          focTable.slopeY = itemFlt[4];
          focTable.beamTilt = itemDbl[5];
          focTable.calibrated = 0;
          focTable.shiftX.resize(nCal);
          focTable.shiftY.resize(nCal);
          focTable.defocus.resize(nCal);
        }

        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          focTable.defocus[i] = itemFlt[0];
          focTable.shiftX[i] = itemFlt[1];
          focTable.shiftY[i] = itemFlt[2];
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading focus calibrations, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }
        mFocTab->Add(focTable);

      } else if (NAME_IS("STEMnormalizedSlope")) {
        mWinApp->mFocusManager->SetSFnormalizedSlope(0, itemFlt[1]);
        mWinApp->mFocusManager->SetSFnormalizedSlope(1, itemFlt[2]);

      } else if (NAME_IS("STEMfocusVersusZ")) {
        sfZtable.numPoints = itemInt[1];
        sfZtable.spotSize = itemInt[2];
        sfZtable.probeMode = itemInt[3];
        for (i = 0; i < sfZtable.numPoints; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          sfZtable.stageZ[i] = itemFlt[0];
          sfZtable.defocus[i] = itemFlt[1];
          sfZtable.absFocus[i] = itemFlt[2];
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading defocus versus Z table, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }
        focusZtables->Add(sfZtable);

      } else if (NAME_IS("AstigmatismCalib")) {
        astig.magInd = itemInt[1];
        astig.probeMode = itemInt[2];
        astig.alpha = itemInt[3];
        astig.defocus = itemFlt[4]; 
        astig.beamTilt = itemFlt[5];
        astig.focusMat.xpx = itemFlt[6];
        astig.focusMat.xpy = itemFlt[7];
        astig.focusMat.ypx = itemFlt[8];
        astig.focusMat.ypy = itemFlt[9];
        astig.astigXmat.xpx = itemFlt[10];
        astig.astigXmat.xpy = itemFlt[11];
        astig.astigXmat.ypx = itemFlt[12];
        astig.astigXmat.ypy = itemFlt[13];
        astig.astigYmat.xpx = itemFlt[14];
        astig.astigYmat.xpy = itemFlt[15];
        astig.astigYmat.ypx = itemFlt[16];
        astig.astigYmat.ypy = itemFlt[17];
        astigCals->Add(astig);
 
      } else if (NAME_IS("ComaCalib")) {
        coma.magInd = itemInt[1];
        coma.probeMode = itemInt[2];
        coma.alpha = itemInt[3];
        coma.defocus = itemFlt[4]; 
        coma.beamTilt = itemFlt[5];
        coma.comaXmat.xpx = itemFlt[6];
        coma.comaXmat.xpy = itemFlt[7];
        coma.comaXmat.ypx = itemFlt[8];
        coma.comaXmat.ypy = itemFlt[9];
        coma.comaYmat.xpx = itemFlt[10];
        coma.comaYmat.xpy = itemFlt[11];
        coma.comaYmat.ypx = itemFlt[12];
        coma.comaYmat.ypy = itemFlt[13];
        comaCals->Add(coma);

      } else if (NAME_IS("CtfAstigCalib")) {
        ctfCal.comaType = false;
        ctfCal.numFits = 4;
        ctfCal.magInd = itemInt[1];
        ctfCal.amplitude = itemFlt[2];
        ctfCal.fitValues[0] = itemFlt[3];
        ctfCal.fitValues[1] = itemFlt[4];
        ctfCal.fitValues[2] = itemFlt[5];
        ctfCal.fitValues[3] = itemFlt[6];
        ctfCal.fitValues[4] = itemFlt[7];
        ctfCal.fitValues[5] = itemFlt[8];
        ctfCal.fitValues[6] = itemFlt[9];
        ctfCal.fitValues[7] = itemFlt[10];
        ctfCal.fitValues[8] = itemFlt[11];
        ctfCal.fitValues[9] = itemFlt[12];
        ctfCal.fitValues[10] = itemFlt[13];
        ctfCal.fitValues[11] = itemFlt[14];
        ctfAstigCals->Add(ctfCal);

      } else if (NAME_IS("BeamIntensityTable")) {
        nCal = itemInt[1];
        index = itemInt[2];
        spot = itemInt[3];
        probe = 1;
        if (!itemEmpty[8])
          probe = itemInt[7];
        if (index < 1 || index >= MAX_MAGS || spot < 1 || nCal < 1 || probe < 0 || 
          probe > 1) {
            nCal = 0;
            err = 1;
        } else {

          // If mag and spot OK, get the next entry for beam cals
          beamInd = mWinApp->mBeamAssessor->GetNumTables();
          freeInd = mWinApp->mBeamAssessor->GetFreeIndex();
          if (beamInd >= MAX_INTENSITY_TABLES || freeInd > 0.9 * MAX_INTENSITY_ARRAYS) {
            AfxMessageBox("Too many beam calibrations for arrays", MB_EXCLAME);
            err = 2;
            nCal = 0;
          } else {
            mWinApp->mBeamAssessor->SetNumTables(beamInd + 1);
            beamTab = &beamTables[beamInd];
            beamTab->intensities = &beamTables[0].intensities[freeInd];
            beamTab->currents = &beamTables[0].currents[freeInd];
            beamTab->logCurrents = &beamTables[0].logCurrents[freeInd];
            mWinApp->mBeamAssessor->SetFreeIndex(freeInd + nCal);
          }
          if (nCal) {
            beamTab->numIntensities = nCal;
            beamTab->magIndex = index;
            beamTab->spotSize = spot;
            beamTab->probeMode = probe;
            beamTab->dontExtrapFlags = itemInt[4];
            intensity = itemDbl[5];
            // 3/26/07: fix for bad forward migration: it was testing for 5 (the mag)
            // and putting it in crossover, so now test for 5 = 6 and repair to 0
            if (strItems[6].IsEmpty() || 
              (int)floor(intensity + 0.5) == itemInt[6])
              beamTab->crossover = 0.;
            else
              beamTab->crossover = intensity;
            if (strItems[7].IsEmpty())  // Last item is the mag
              beamTab->measuredAperture = 0;
            else
              beamTab->measuredAperture = itemInt[6];
          }
        }

        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          beamTab->currents[i] = itemFlt[0];
          beamTab->intensities[i] = itemDbl[1];
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading beam calibrations, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }
        if (mWinApp->mBeamAssessor->CheckCalForZeroIntensities(beamTables[beamInd], 
          "After reading in from the calibration file", 2))
          beamTab->numIntensities = 0;
        mWinApp->mBeamAssessor->SortAndTakeLogs(beamTab, false);

      } else if (NAME_IS("BeamShiftCalibration")) {

        // Old form has no mag, newer form has a mag index in first spot and mag at end
        // Default probe mode to 1 if it is not present yet
        i = 1;
        index = 0;
        nCal = -999;
        beamInd = 1;
        focInd = 0;
        if (!strItems[5].IsEmpty())
          index = atoi((LPCTSTR)strItems[i++]);
        IStoBS.xpx = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.xpy = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.ypx = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.ypy = itemFlt[i++];
        if (!strItems[7].IsEmpty())
          nCal = atoi((LPCTSTR)strItems[6]);
        if (!strItems[8].IsEmpty())
          beamInd = atoi((LPCTSTR)strItems[7]);
        if (!strItems[9].IsEmpty())
          focInd = atoi((LPCTSTR)strItems[8]);
        mShiftManager->SetBeamShiftCal(IStoBS, index, nCal, beamInd, focInd);
        
       } else if (NAME_IS("StageStretchXform")) {
         IStoBS.xpx = itemFlt[1];
         IStoBS.xpy = itemFlt[2];
         IStoBS.ypx = itemFlt[3];
         IStoBS.ypy = itemFlt[4];
         mShiftManager->SetStageStretchXform(IStoBS);
       } else if (NAME_IS("FilterMagShifts")) {
        nCal = itemInt[1];
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          if (index < 1 || index >= MAX_MAGS) {
            err = 1;
            break;
          } else {
            mFilterParam->magShifts[index] = itemFlt[1];
          }
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading filter mag-dependent shifts, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }

      } else if (NAME_IS("NeutralImageShifts") || NAME_IS("GIFNeutralIS") || 
        NAME_IS("HitachiBaseFocus")) {
        nCal = itemInt[1];
        focInd = (NAME_IS("GIFNeutralIS")) ? 1 : 0;
        if (NAME_IS("HitachiBaseFocus"))
          focInd = -1;
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          if (index < 1 || index >= MAX_MAGS) {
            err = 1;
            break;
          } else {
            if (focInd < 0) {
              hParams->baseFocus[index] = itemInt[1];
            } else {
              mMagTab[index].neutralISX[focInd] = itemFlt[1];
              mMagTab[index].neutralISY[focInd] = itemFlt[2];
            }
          }
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox(focInd < 0 ? "Error reading base focus values, line\n" :
            "Error reading neutral image shift values, line\n" + strLine, MB_EXCLAME);
          retval = err;
          break;
        }
 
      } else if (NAME_IS("AlphaBeamShifts") || NAME_IS("AlphaBeamTilts")) {
        nCal = itemInt[1];
        index = NAME_IS("AlphaBeamShifts") ? 1 : 0;
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          if (i < MAX_ALPHAS && index) {
            alphaBeamShifts[2 * i] = itemFlt[0];
            alphaBeamShifts[2 * i + 1] = itemFlt[1];
          } else if (i < MAX_ALPHAS) {
            alphaBeamTilts[2 * i] = itemFlt[0];
            alphaBeamTilts[2 * i + 1] = itemFlt[1];
          }
        }
        if (index)
          mWinApp->mScope->SetNumAlphaBeamShifts(B3DMIN(i, MAX_ALPHAS));
        else
          mWinApp->mScope->SetNumAlphaBeamTilts(B3DMIN(i, MAX_ALPHAS));
        if (err > 0) {
          retval = err;
          break;
        }

      } else if (NAME_IS("CameraISOffset")) {
        index = itemInt[1];
        B3DCLAMP(index, 0, MAX_CAMERAS - 1);
        mWinApp->mCamera->SetCameraISOffset(index, itemFlt[2], itemFlt[3]);
      } else if (NAME_IS("SpotBeamShifts")) {
        for (index = 0; index < 2; index++) {
          minSpot[index] = itemEmpty[1 + 2 * index] ? 0 : itemInt[1 + 2 * index];
          maxSpot[index] = itemEmpty[2 + 2 * index] ? 0 : itemInt[2 + 2 * index];
        }
        for (index = 0; index < 2; index++) {
          if (maxSpot[index] > minSpot[index]) {
            for (i = minSpot[index]; i <= maxSpot[index]; i++) {
              spot = 2 * index * (MAX_SPOT_SIZE + 1);
              err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl,
                                   itemFlt, MAX_TOKENS);
              if (err)
                break;
              spotBeamShifts[spot + i * 2] = itemDbl[0];
              spotBeamShifts[spot + i * 2 + 1] = itemDbl[1];
            }
            if (err)
              break;
            mWinApp->mScope->SetMinMaxBeamShiftSpots(index, minSpot[index],
              maxSpot[index]);
          }
        }
        if (err > 0) {
          retval = err;
          break;
        }

      } else if (NAME_IS("ImageShiftOffsets")) {
        nCal = itemInt[1];
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          camera = itemInt[1];
          if (index < 1 || index >= MAX_MAGS || camera < 0 || camera > 1) {
            err = 1;
            break;
          } else {
            mMagTab[index].calOffsetISX[camera] = itemFlt[2];
            mMagTab[index].calOffsetISY[camera] = itemFlt[3];
          }
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading image shift offsets, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }

      } else if (NAME_IS("GIFShiftOffset")) {
        index = itemInt[1];
        camera = itemInt[2];
        if (index < 2 || index >= MAX_MAGS || camera < 2 || camera >= MAX_MAGS || 
          strItems[4].IsEmpty()) {
            retval = 1;
            AfxMessageBox("Error reading GIF image shift offset, line\n" +
            strLine, MB_EXCLAME);
            break;
        }
        mWinApp->mShiftCalibrator->SetGIFofsFromMag(index);
        mWinApp->mShiftCalibrator->SetGIFofsToMag(index);
        mWinApp->mShiftCalibrator->SetGIFofsISX(itemDbl[3]);
        mWinApp->mShiftCalibrator->SetGIFofsISY(itemDbl[4]);

      } else if (NAME_IS("SpotIntensities")) {

        // Clear out the temporary tables
        for (i = 0; i <= MAX_SPOT_SIZE; i++) {
          tmpTable[0].ratio[i] = tmpTable[1].ratio[i] = 0;
          tmpTable[0].intensity[i] = tmpTable[1].intensity[i] = 0;
          tmpTable[0].crossover[i] = tmpTable[1].crossover[i] = 0;
        }

        // Read the table, keeping track of whether micro or nano side has non-zero entry
        nCal = itemInt[1];
        oneIntensity = !strItems[2].IsEmpty();
        if (oneIntensity)
          intensity = itemDbl[2];
        tmpTable[0].probeMode = 1;
        tmpTable[1].probeMode = 0;
        beamInd = 0;
        freeInd = 0;
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          if (index < 1 || index > MAX_SPOT_SIZE) {
            err = 1;
            break;
          }
          tmpTable[0].ratio[index] = itemFlt[1];
          if (itemDbl[1])
            beamInd++;
          if (oneIntensity)
            tmpTable[0].intensity[index] = intensity;
          else
            tmpTable[0].intensity[index] = itemDbl[2];
          if (strItems[3].IsEmpty())
            tmpTable[0].crossover[index] = 0.;
          else
            tmpTable[0].crossover[index] = itemDbl[3];
          if (!itemEmpty[6]) {
            tmpTable[1].ratio[index] = itemFlt[4];
            tmpTable[1].intensity[index] = itemDbl[5];
            tmpTable[1].crossover[index] = itemDbl[6];
            if (itemDbl[4])
              freeInd++;
          }
        }

        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading spot intensities, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }

        // Add the microprobe table then the nanoprobe table (if any) to the collection
        index = mWinApp->mBeamAssessor->GetNumSpotTables();
        if (beamInd && index < 4) {
          spotTables[index] = tmpTable[0];
          mWinApp->mBeamAssessor->SetNumSpotTables(index + 1);
        }
        index = mWinApp->mBeamAssessor->GetNumSpotTables();
        if (freeInd && index < 4) {
          spotTables[index] = tmpTable[1];
          mWinApp->mBeamAssessor->SetNumSpotTables(index + 1);
        }

      } else if (NAME_IS("SpotCalAtC2Aperture")) {
        tmpAper[0] = itemInt[1];
        tmpAper[1] = itemInt[2];
        if (!itemEmpty[4]) {
          tmpAper[2] = itemInt[3];
          tmpAper[3] = itemInt[4];
        }
        gotSpotAper = true;
        
      } else if (NAME_IS("IllumAreaLimits")) {
        nCal = itemInt[1];
        for (i = 1; i <= B3DMIN(nCal, MAX_SPOT_SIZE); i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt,
                               MAX_TOKENS);
          if (err)
            break;
          lowIAlims[i * 4] = itemFlt[0];
          highIAlims[i * 4] = itemFlt[1];
          lowIAlims[i * 4 + 1] = itemFlt[2];
          highIAlims[i * 4 + 1] = itemFlt[3];
          lowIAlims[i * 4 + 2] = itemFlt[4];
          highIAlims[i * 4 + 2] = itemFlt[5];
          lowIAlims[i * 4 + 3] = itemFlt[6];
          highIAlims[i * 4 + 3] = itemFlt[7];
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading illuminated area limits, line\n" +
              strLine, MB_EXCLAME);
          retval = err;
          break;
        }


      } else if (NAME_IS("StandardLMFocus")) {
        index = itemInt[1];
        if (index < 1 || index >= MAX_MAGS || strItems[3].IsEmpty()) {
          retval = 1;
          AfxMessageBox("Error reading standard LM focus, line\n" +
            strLine, MB_EXCLAME);
          break;
        }
        LMfocTab[2 * index + 1] = itemDbl[2];
        if (!itemEmpty[4])
          LMfocTab[2 * index] = itemDbl[3];

      } else if (!strItems[0].IsEmpty())
        AfxMessageBox("Unrecognized entry in calibration file " + strFileName 
        + " : " + strLine , MB_EXCLAME);
    }
    if (err > 0)
      retval = err;

    
      
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }

  // Assign spot calibration apertures first to microprobe then to nanoprobe spot tables
  if (gotSpotAper) {
    freeInd = 0;
    for (probe = 1; probe >= 0; probe--)
      for (index = 0; index < mWinApp->mBeamAssessor->GetNumSpotTables(); index++)
        if (spotTables[index].probeMode == probe)
          spotCalAper[index] = tmpAper[freeInd++];
  }

  return retval;
}

void CParameterIO::WriteCalibration(CString strFileName)
{
  int i, j, nCal, nCal2, ind, k, probe, nCalN[4], minSpot[2], maxSpot[2];
  float offsetX, offsetY;
  int indMicro[2] = {-1, -1}, indNano[2] = {-1, -1};
  CString string;
  int err = 0;
  double intScaled, cross[2], ratio[2], intensity[2];
  int curAperture = mWinApp->mBeamAssessor->GetCurrentAperture();
  int *crossCalAper = mWinApp->mBeamAssessor->GetCrossCalAperture();
  int *spotCalAper = mWinApp->mBeamAssessor->GetSpotCalAperture();
  ScaleMat stageStr = mShiftManager->GetStageStretchXform();
  ScaleMat *IStoBS = mShiftManager->GetBeamShiftMatrices();
  BeamTable *beamTables = mWinApp->mBeamAssessor->GetBeamTables();
  BeamTable *btp;
  SpotTable *spotTables = mWinApp->mBeamAssessor->GetSpotTables();
  ShortVec *beamMags = mShiftManager->GetBeamCalMagInd();
  ShortVec *beamAlphas = mShiftManager->GetBeamCalAlpha();
  ShortVec *beamProbes = mShiftManager->GetBeamCalProbe();
  ShortVec *beamRetains = mShiftManager->GetBeamCalRetain();
  float *alphaBeamShifts = mWinApp->mScope->GetAlphaBeamShifts();
  float *alphaBeamTilts = mWinApp->mScope->GetAlphaBeamTilts();
  double *spotBeamShifts = mWinApp->mScope->GetSpotBeamShifts();
  double *LMfocTab = mWinApp->mScope->GetLMFocusTable();
  float *lowIAlims = mWinApp->mScope->GetCalLowIllumAreaLim();
  float *highIAlims = mWinApp->mScope->GetCalHighIllumAreaLim();
  CArray<STEMFocusZTable, STEMFocusZTable> *focusZtables =
    mWinApp->mFocusManager->GetSFfocusZtables();
  CArray <AstigCalib, AstigCalib> *astigCals = mWinApp->mAutoTuning->GetAstigCals();
  AstigCalib astig;
  CArray <ComaCalib, ComaCalib> *comaCals = mWinApp->mAutoTuning->GetComaCals();
  ComaCalib coma;
  CArray <CtfBasedCalib, CtfBasedCalib> *ctfAstigCals =
    mWinApp->mAutoTuning->GetCtfBasedCals();
  CtfBasedCalib ctfCal;
  CArray<ParallelIllum, ParallelIllum> *parIllums =
    mWinApp->mBeamAssessor->GetParIllumArray();
  ParallelIllum parallelIllum;
  HighFocusCalArray *focusMagCals;
  HighFocusMagCal focCal;
  HitachiParams *hParams = mWinApp->GetHitachiParams();
  FocusTable focTable;

  try {
    // Open the file for writing, 
    mFile = new CStdioFile(strFileName, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);

    mFile->WriteString("SerialEMCalibrations\n");

    // Write any calibrated image shift matrices
    nCal = 0;
    for (i = 1; i < MAX_MAGS; i++)
      for (j = 0; j < MAX_CAMERAS; j++)
        if (mMagTab[i].matIS[j].xpx != 0. || mMagTab[i].matIS[j].xpy != 0.)
          nCal++;
    if (nCal) {
      WriteInt("ImageShiftMatrix", nCal);

      for (i = 1; i < MAX_MAGS; i++)
        for (j = 0; j < MAX_CAMERAS; j++) {
          if (mMagTab[i].matIS[j].xpx != 0. || mMagTab[i].matIS[j].xpy != 0.) {
            string.Format("%d %d %f %f %f %f   %d\n", i, j, 
              mMagTab[i].matIS[j].xpx, mMagTab[i].matIS[j].xpy,
              mMagTab[i].matIS[j].ypx, mMagTab[i].matIS[j].ypy, MagForCamera(j, i));
            mFile->WriteString(string);
          }
        }
    }
  
    // Write stage calibrations if any
    for (i = 1; i < MAX_MAGS; i++)
      for (j = 0; j < MAX_CAMERAS; j++) {
        if (mMagTab[i].matStage[j].xpx != 0. || mMagTab[i].matStage[j].xpy != 0.) {
          string.Format("StageToCameraMatrix %d %d %f %f %f %f   %f   %d\n", i, j, 
            mMagTab[i].matStage[j].xpx, mMagTab[i].matStage[j].xpy,
            mMagTab[i].matStage[j].ypx, mMagTab[i].matStage[j].ypy, 
            mMagTab[i].stageCalFocus[j], MagForCamera(j, i));
          mFile->WriteString(string);
        }
      }

    // Write high focus mag calibrations
    for (j = 0; j < 2; j++) {
      focusMagCals = j ? mShiftManager->GetFocusISCals() :
        mShiftManager->GetFocusMagCals();
      for (i = 0; i < focusMagCals->GetSize(); i++) {
        focCal = focusMagCals->GetAt(i);
        string.Format("%s %d %d %f %f %f %f %f %d %d\n", j ? "HighFocusISCal" :
          "HighFocusMagCal", focCal.spot,
          focCal.probeMode, focCal.defocus, mWinApp->mScope->IntensityAfterApertureChange(
            focCal.intensity, curAperture, focCal.measuredAperture, focCal.spot,
            focCal.probeMode), focCal.scale,
          focCal.rotation, mWinApp->mScope->IntensityAfterApertureChange(
            focCal.crossover, curAperture, focCal.measuredAperture, focCal.spot,
            focCal.probeMode), focCal.measuredAperture, focCal.magIndex);
        mFile->WriteString(string);
      }
    }

    // Write parallel illums
    for (i = 0; i < parIllums->GetSize(); i++) {
      parallelIllum = parIllums->GetAt(i);
      string.Format("ParallelIllum %f %d %d %f %d\n", parallelIllum.intensity,
        parallelIllum.spotSize, parallelIllum.probeOrAlpha, parallelIllum.crossover,
        parallelIllum.measuredAperture);
      mFile->WriteString(string);
    }

    // Write intensity to C2 factors
    string.Format("IntensityToC2Factor %f %f\n", mWinApp->mScope->GetC2IntensityFactor(1),
      mWinApp->mScope->GetC2IntensityFactor(0));
    mFile->WriteString(string);
    for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
      string.Format("C2SpotOffset %d %f %f\n", i, 
        mWinApp->mScope->GetC2SpotOffset(i, 1), mWinApp->mScope->GetC2SpotOffset(i, 0));
      mFile->WriteString(string);
    }

    // Write crossover intensities
    for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
      for (probe = 0; probe < 2; probe++) {
        cross[probe] = mWinApp->mScope->GetCrossover(i, probe);
        if (cross[probe])
          cross[probe] = mWinApp->mScope->IntensityAfterApertureChange(cross[probe],
          curAperture, crossCalAper[probe], i, probe);
      }
      string.Format("CrossoverIntensity %d %f %f\n", i, cross[1], cross[0]);
      mFile->WriteString(string);
    }
    if (crossCalAper[0] || crossCalAper[1]) {
      string.Format("CrossCalAtC2Aperture %d %d\n", crossCalAper[1], crossCalAper[0]);
      mFile->WriteString(string);
    }

    // Write focus calibrations
    for (ind = 0; ind < mFocTab->GetSize(); ind++) {
      focTable = mFocTab->GetAt(ind);
      string.Format("FocusCalibration %d %d %f %f %.2f %d %d %d %d   %d\n",
        focTable.magInd, focTable.camera, focTable.slopeX, focTable.slopeY,
        focTable.beamTilt, focTable.numPoints, focTable.direction, focTable.probeMode,
        focTable.alpha, MagForCamera(focTable.camera, focTable.magInd));
      mFile->WriteString(string);
      for (k = 0; k < focTable.numPoints; k++) {
        string.Format("%f %f %f\n", focTable.defocus[k], focTable.shiftX[k], 
          focTable.shiftY[k]);
        mFile->WriteString(string);
      }
    }
    string.Format("STEMnormalizedSlope %f %f\n", 
      mWinApp->mFocusManager->GetSFnormalizedSlope(0),
      mWinApp->mFocusManager->GetSFnormalizedSlope(1));
    mFile->WriteString(string);

    // Write focus versus Z tables
    for (i = 0; i < focusZtables->GetSize(); i++) {
      STEMFocusZTable sfZtable = focusZtables->GetAt(i);
      string.Format("STEMfocusVersusZ %d %d %d\n", sfZtable.numPoints, sfZtable.spotSize,
        sfZtable.probeMode);
      mFile->WriteString(string);
      for (k = 0; k < sfZtable.numPoints; k++) {
        string.Format("%f %f %f\n", sfZtable.stageZ[k], sfZtable.defocus[k],
          sfZtable.absFocus[k]);
        mFile->WriteString(string);
      }
    }

    // Write BTID-based astigmatism calibrations
    for (i = 0; i < astigCals->GetSize(); i++) {
      astig = astigCals->GetAt(i);
      string.Format("AstigmatismCalib %d %d %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f"
        "   %d\n", astig.magInd, astig.probeMode, astig.alpha, astig.defocus, 
        astig.beamTilt, astig.focusMat.xpx, astig.focusMat.xpy, astig.focusMat.ypx, 
        astig.focusMat.ypy, astig.astigXmat.xpx, astig.astigXmat.xpy, astig.astigXmat.ypx, 
        astig.astigXmat.ypy, astig.astigYmat.xpx, astig.astigYmat.xpy, 
        astig.astigYmat.ypx, astig.astigYmat.ypy, mMagTab[astig.magInd].mag);
      mFile->WriteString(string);
    }

    // Write coma calibrations
    for (i = 0; i < comaCals->GetSize(); i++) {
      coma = comaCals->GetAt(i);
      string.Format("ComaCalib %d %d %d %f %f %f %f %f %f %f %f %f %f   %d\n", 
        coma.magInd, coma.probeMode, coma.alpha, coma.defocus, 
        coma.beamTilt, coma.comaXmat.xpx, coma.comaXmat.xpy, coma.comaXmat.ypx, 
        coma.comaXmat.ypy, coma.comaYmat.xpx, coma.comaYmat.xpy, 
        coma.comaYmat.ypx, coma.comaYmat.ypy, mMagTab[coma.magInd].mag);
      mFile->WriteString(string);
    }

    // Write ctf-based astigmatism calibrations
    for (i = 0; i < ctfAstigCals->GetSize(); i++) {
      ctfCal = ctfAstigCals->GetAt(i);
      if (ctfCal.comaType)
        continue;
      string.Format("CtfAstigCalib %d %f %f %f %f %f %f %f %f %f %f %f %f %f   %d\n", 
        ctfCal.magInd, ctfCal.amplitude, ctfCal.fitValues[0], ctfCal.fitValues[1], 
        ctfCal.fitValues[2], ctfCal.fitValues[3], ctfCal.fitValues[4],ctfCal.fitValues[5], 
        ctfCal.fitValues[6], ctfCal.fitValues[7], ctfCal.fitValues[8],ctfCal.fitValues[9], 
        ctfCal.fitValues[10], ctfCal.fitValues[11], mMagTab[ctfCal.magInd].mag);
      mFile->WriteString(string);
    }

    // Write beam calibrations
    for (i = 0; i < mWinApp->mBeamAssessor->GetNumTables(); i++) {
      btp = &beamTables[i];
      if (btp->numIntensities && mWinApp->mBeamAssessor->CheckCalForZeroIntensities(
        beamTables[i], "Before saving to the calibration file", 0))
        btp->numIntensities = 0;
      if (btp->numIntensities) {
        cross[0] = btp->crossover;
        if (cross[0])
          cross[0] = mWinApp->mScope->IntensityAfterApertureChange(cross[0], curAperture, 
            btp->measuredAperture, btp->spotSize, btp->probeMode);
        string.Format("BeamIntensityTable %d %d %d %d %f %d %d   %d\n", 
          btp->numIntensities, btp->magIndex,
          btp->spotSize, btp->dontExtrapFlags, cross[0],
          btp->measuredAperture, btp->probeMode, mMagTab[btp->magIndex].mag);
        mFile->WriteString(string);
        nCal = 0;
        for (k = 0; k < btp->numIntensities; k++) {
          intScaled = mWinApp->mScope->IntensityAfterApertureChange(btp->intensities[k], 
            curAperture, btp->measuredAperture, btp->spotSize, btp->probeMode);
          string.Format("%f %f\n", btp->currents[k], intScaled);
          mFile->WriteString(string);
          if (intScaled)
            nCal = 1;
        }
        if (!nCal) {
          nCal = btp->numIntensities;
          mWinApp->mBeamAssessor->CheckCalForZeroIntensities(beamTables[i], 
            "After scaling back to the original aperture size and writing to file", 1);
          btp->numIntensities = nCal;
        }
      }
    }

    for (i = 0; i < mShiftManager->GetNumBeamShiftCals(); i++) {
      nCal = beamMags->at(i) < 0 ? mMagTab[-beamMags->at(i)].EFTEMmag : 
        mMagTab[beamMags->at(i)].mag;
      string.Format("BeamShiftCalibration %d %f %f %f %f %d %d %d  %d\n",
        beamMags->at(i), IStoBS[i].xpx, IStoBS[i].xpy, IStoBS[i].ypx, IStoBS[i].ypy, 
        beamAlphas->at(i), beamProbes->at(i), beamRetains->at(i), nCal);
      mFile->WriteString(string);
    }

    if (stageStr.xpx) {
      string.Format("StageStretchXform %f %f %f %f\n", stageStr.xpx, stageStr.xpy, 
        stageStr.ypx, stageStr.ypy);
      mFile->WriteString(string);
    }
      
    // Write any filter mag shifts
    nCal = 0;
    for (i = 1; i < MAX_MAGS; i++)
      if (mFilterParam->magShifts[i] > -900.)
        nCal++;
    if (nCal) {
      WriteInt("FilterMagShifts", nCal);
      for (i = 1; i < MAX_MAGS; i++)
        if (mFilterParam->magShifts[i] > -900.) {
          string.Format("%d %f\n", i, mFilterParam->magShifts[i]);
          mFile->WriteString(string);
        }
    }

    // Write any nonzero image shift neutral or offsets
    nCalN[0] = nCalN[1] = 0;
    nCal2 = 0;
    for (i = 1; i < MAX_MAGS; i++) {
      for (ind = 0; ind < 2; ind++) {
        if (mMagTab[i].calOffsetISX[ind] || mMagTab[i].calOffsetISY[ind])
          nCal2++;
        if (mMagTab[i].neutralISX[ind] || mMagTab[i].neutralISY[ind])
          nCalN[ind]++;
      }
    }

    for (ind = 0; ind < 2; ind++) {
      if (nCalN[ind]) {
        WriteInt(ind ? "GIFNeutralIS" : "NeutralImageShifts", nCalN[ind]);
        for (i = 1; i < MAX_MAGS; i++)
          if (mMagTab[i].neutralISX[ind] || mMagTab[i].neutralISY[ind]) {
            string.Format("%d %f %f\n", i, mMagTab[i].neutralISX[ind], 
              mMagTab[i].neutralISY[ind]);
            mFile->WriteString(string);
          }
      }
    }
    if (nCal2) {
      WriteInt("ImageShiftOffsets", nCal2);
      for (ind = 0; ind < 2; ind++)
        for (i = 1; i < MAX_MAGS; i++)
          if (mMagTab[i].calOffsetISX[ind] || mMagTab[i].calOffsetISY[ind]) {
            string.Format("%d %d %f %f\n", i, ind, mMagTab[i].calOffsetISX[ind], 
              mMagTab[i].calOffsetISY[ind]);
            mFile->WriteString(string);
        }
    }
    nCal = mWinApp->mShiftCalibrator->GetGIFofsFromMag();
    if (nCal) {
      string.Format("GIFShiftOffset %d %d %f %f\n", nCal, 
        mWinApp->mShiftCalibrator->GetGIFofsToMag(), 
        mWinApp->mShiftCalibrator->GetGIFofsISX(),
        mWinApp->mShiftCalibrator->GetGIFofsISY());
      mFile->WriteString(string);
    }

    // Write Hitachi base focus
    if (HitachiScope) {
      for (nCal2 = 0; nCal2 < MAX_MAGS - 1; nCal2++)
        if (!mMagTab[nCal2 + 1].mag)
          break;
      WriteInt("HitachiBaseFocus", nCal2);
      for (i = 1; i <= nCal2; i++) {
        string.Format("%d %d\n", i, hParams->baseFocus[i]);
        mFile->WriteString(string);
      }
    } 

    // Write alpha beam shifts and tilts
    nCal = mWinApp->mScope->GetNumAlphaBeamShifts();
    if (nCal) {
      WriteInt("AlphaBeamShifts", nCal);
      for (i = 0; i < nCal; i++) {
        string.Format("%f %f\n", alphaBeamShifts[2 * i], alphaBeamShifts[2 * i + 1]);
        mFile->WriteString(string);
      }
    }
    for (i = 0; i < MAX_CAMERAS; i++) {
      mWinApp->mCamera->GetCameraISOffset(i, offsetX, offsetY);
      if (offsetX != 0. || offsetY != 0.) {
        string.Format("CameraISOffset %d %f %f\n", i, offsetX, offsetY);
        mFile->WriteString(string);
      }
    }
    nCal = mWinApp->mScope->GetNumAlphaBeamTilts();
    if (nCal) {
      WriteInt("AlphaBeamTilts", nCal);
      for (i = 0; i < nCal; i++) {
        string.Format("%f %f\n", alphaBeamTilts[2 * i], alphaBeamTilts[2 * i + 1]);
        mFile->WriteString(string);
      }
    }

    // Write spot beam shifts
    mWinApp->mScope->GetMinMaxBeamShiftSpots(0, minSpot[0], maxSpot[0]);
    mWinApp->mScope->GetMinMaxBeamShiftSpots(1, minSpot[1], maxSpot[1]);
    if (maxSpot[0] > minSpot[0]) {
      string.Format("SpotBeamShifts %d %d %d %d\n", minSpot[0], maxSpot[0], minSpot[1], 
        maxSpot[1]);
      mFile->WriteString(string);
      for (nCal = 0; nCal < 2; nCal++) {
        k = 2 * nCal * (MAX_SPOT_SIZE + 1);
        if (maxSpot[nCal] > minSpot[nCal]) {
          for (ind = minSpot[nCal]; ind <= maxSpot[nCal]; ind++) {
            string.Format("%f %f\n", spotBeamShifts[k + 2 * ind], 
              spotBeamShifts[k + 2 * ind + 1]);
            mFile->WriteString(string);
          }
        }
      }
    }

    // Write any nonzero spot intensities present for either microprobe or nanoprobe
    // Get indexes of first and second one of each type
    probe = nCal2 = 0;
    for (ind = 0; ind < mWinApp->mBeamAssessor->GetNumSpotTables(); ind++) {
      if (spotTables[ind].probeMode && probe < 2)
        indMicro[probe++] = ind;
      else if (!spotTables[ind].probeMode && nCal2 < 2)
        indNano[nCal2++] = ind;
    }

    // For each potential pair of cal's, count up possible indices with ratios
    for (k = 0; k < B3DMAX(probe, nCal2); k++) {
      nCal = 0;
      ind = indMicro[k];
      j = indNano[k];
      for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++)
        if ((ind >= 0 && spotTables[ind].ratio[i]) || (j >= 0 && spotTables[j].ratio[i]))
          nCal++;
      string.Format("SpotIntensities %d\n", nCal);
      mFile->WriteString(string);

      // Do each spot, gathering data for each mode
      for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
        ind = indMicro[k];
        for (j = 0; j < 2; j++) {
          ratio[j] = 0.;
          intensity[j] = 0.;
          cross[j] = 0.;
          if (ind >= 0 && spotTables[ind].ratio[i]) {
            ratio[j] = spotTables[ind].ratio[i];
            cross[j] = spotTables[ind].crossover[i];
            if (cross[j])
              cross[j] = mWinApp->mScope->IntensityAfterApertureChange(cross[j], 
              curAperture, spotCalAper[ind], i, j);
            intensity[j] = mWinApp->mScope->IntensityAfterApertureChange(
              spotTables[ind].intensity[i], curAperture, spotCalAper[ind], i, j);
          }
          ind = indNano[k];
        }

        // Write line if either is nonzero
        if (ratio[0] || ratio[1]) {
          string.Format("%d %f %f %f %f %f %f\n", i, ratio[0], intensity[0], cross[0], 
            ratio[1], intensity[1], cross[1]);
          mFile->WriteString(string);
        }
      }
    }

    // Put out the apertures in order micro then nano.  Keep this right after spot
    // intensities, it uses probe and nCal2 from above
    if (spotCalAper[0] || spotCalAper[1] || spotCalAper[2] || spotCalAper[3]) {
      for (ind = 0; ind < 4; ind++)
        nCalN[ind]= 0;
      ind = 0;
      if (probe)
        nCalN[ind++] = spotCalAper[indMicro[0]];
      if (probe > 1)
        nCalN[ind++] = spotCalAper[indMicro[1]];
      if (nCal2)
        nCalN[ind++] = spotCalAper[indNano[0]];
      if (nCal2 > 1)
        nCalN[ind++] = spotCalAper[indNano[1]];
      string.Format("SpotCalAtC2Aperture %d %d %d %d\n", nCalN[0], nCalN[1],
        nCalN[2], nCalN[3]);
      mFile->WriteString(string);
    }

    // Write illuminated area limits
    string.Format("IllumAreaLimits %d\n", mWinApp->mScope->GetNumSpotSizes());
    mFile->WriteString(string);
    for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
      string.Format("%f %f %f %f %f %f %f %f\n", lowIAlims[4 * i], highIAlims[4 * i],
        lowIAlims[4 * i + 1], highIAlims[4 * i + 1], lowIAlims[4 * i + 2], 
        highIAlims[4 * i + 2], lowIAlims[4 * i + 3], highIAlims[4 * i + 3]);
      mFile->WriteString(string);
    }

    // Write any Standard LM Focus entries
    for (i = 1; i < MAX_MAGS; i++) {
      if (LMfocTab[2 * i] > -900. || LMfocTab[2 * i + 1] > -900.) {
        string.Format("StandardLMFocus %d %f %f %d\n", i, LMfocTab[2 * i + 1], 
          LMfocTab[2 * i], mMagTab[i].mag); 
        mFile->WriteString(string);
      }
    }

    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing calibrations to file " + strFileName;
    AfxMessageBox(message, MB_EXCLAME);
    err = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  if (!err)
    mWinApp->AppendToLog("Calibrations saved to " + strFileName, LOG_MESSAGE_IF_CLOSED);
}


// Read the short-term calibration file (for dose calibration)
int CParameterIO::ReadShortTermCal(CString strFileName, BOOL ignoreCals)
{
  int retval = 0;
  int err, index, cam, inTime;
  bool anyDoseCal = false, calExpired = false;
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  float itemFlt[MAX_TOKENS];
  int *times = mWinApp->mScope->GetLastLongOpTimes();
  std::map<std::string, int> *customTimes = mWinApp->mMacroProcessor->GetCustomTimeMap();
  int *DEdarkRefTimes = mWinApp->mGainRefMaker->GetLastDEdarkRefTime();
  int timeStamp = mWinApp->MinuteTimeStamp();
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  FilterParams *filtP = mWinApp->GetFilterParams();
  NavParams *navP = mWinApp->GetNavParams();
  ShortVec *pixSizCamera, *pixSizMagInd, *addedRotation;
  std::vector<float> *pixelSizes, *gridRotations;
  std::string sstr;
  mWinApp->mProcessImage->GetPixelArrays(&pixSizCamera, &pixSizMagInd, &addedRotation,
    &pixelSizes, &gridRotations);

  try {
    // Open the file for reading, verify that it is a calibration file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadAndParse(strLine, strItems, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (CheckForByteOrderMark(strItems[0], "SerialEMShortTermCal", strFileName,
      "short term calibration")) {
      retval = 1;
    }
    while (retval == 0 && 
           (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, itemFlt, 
                            MAX_TOKENS)) == 0) {

        // Save the nav autosave filename if it belongs to this user
      if (NAME_IS("NavAutosave")) {
        CString name = getenv("USERNAME");
        name = name.Trim();
        name.Replace(" ", "_");
        if (strItems[1] == name)
          StripItems(strLine, 2, navP->autosaveFile);

      } else if (NAME_IS("FakeMagIndex")) {
        if (itemInt[1] >= 0 && itemInt[1] < MAX_MAGS)
          mWinApp->mScope->SetFakeMagIndex(itemInt[1]);
      } else if (NAME_IS("FakeScreenPos")) {
          mWinApp->mScope->SetFakeScreenPos(itemInt[1]);
      } else if (NAME_IS("LastC2Aperture")) {
        mWinApp->mBeamAssessor->ScaleTablesForAperture(itemInt[1], true);

      } else if (NAME_IS("LastLongOpTimes")) {
        for (index = 1; index <= MAX_LONG_OPERATIONS && !itemEmpty[index]; index++)
          times[index - 1] = itemInt[index];
      } else if (NAME_IS("LastCustomTime")) {
        if (!itemEmpty[2] && itemInt[2] > 0 &&
          timeStamp - itemInt[2] < MAX_CUSTOM_INTERVAL) {
          sstr = (LPCTSTR)strItems[1];
          customTimes->insert(std::pair<std::string, int>(sstr, itemInt[2]));
        }

      } else if (NAME_IS("FEGFlashCount")) {
        mWinApp->mScope->SetFegFlashCounter(itemInt[1]);

      } else if (ignoreCals) {

      } else if (NAME_IS("DoseCalibration")) {
        index = itemInt[1] * 2 + itemInt[2];
        index = 2 * index + B3DCHOICE(itemEmpty[6], 1, itemInt[6]);
        if (index < 0 || index >= 4 * MAX_SPOT_SIZE || strItems[5].IsEmpty()) {
          AfxMessageBox("Incorrect entry in short term calibration file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          doseTables[index].timeStamp = itemInt[5];
          doseTables[index].currentAperture = itemEmpty[7] ? 0 : itemInt[7];
          if (timeStamp - doseTables[index].timeStamp <
            60 * mWinApp->GetDoseLifetimeHours()) {
            doseTables[index].intensity = itemDbl[3];
            doseTables[index].dose = itemDbl[4];
            anyDoseCal = true;
          } else if (timeStamp - doseTables[index].timeStamp < 7 * 24 * 60) {
            calExpired = true;
          }
        }

      } else if (NAME_IS("FilterAlignment")) {
        filtP->cumulNonRunTime = itemInt[5] + timeStamp - 
          itemInt[6];

        // To use a filter alignment, the cumulative non run time must be less than
        // the limit and it should either be the same user or an omega filter
        if (filtP->cumulNonRunTime < FILTER_LIFETIME_MINUTES && 
          (mWinApp->mScope->GetHasOmegaFilter() || strItems[7] == getenv("USERNAME"))) {
          filtP->refineZLPOffset = itemFlt[1];
          filtP->alignedSlitWidth = itemFlt[2];
          filtP->alignedMagInd = itemInt[3];
          filtP->alignZLPTimeStamp = itemDbl[4];
          filtP->usedOldAlign = true;
        }
        mDocWnd->SetShortTermNotSaved();

      } else if (NAME_IS("LastFeiZLPshift")) {
        if (filtP->usedOldAlign)
          filtP->lastFeiZLPshift = itemDbl[1];

      } else if (NAME_IS("ImageShiftMatrix") || NAME_IS("StageCalibration")) {
        inTime = itemInt[1];
        index = itemInt[2];
        cam = itemInt[3];
        if (index <= 0 || index >= MAX_MAGS || cam < 0 || cam >= MAX_CAMERAS) {
          AfxMessageBox("Incorrect entry in short term calibration file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else if (timeStamp - inTime < 60 * IS_STAGE_LIFETIME_HOURS) {
          if (NAME_IS("ImageShiftMatrix")) {
            mMagTab[index].calibrated[cam] = inTime;
            mMagTab[index].matIS[cam].xpx = itemFlt[4];
            mMagTab[index].matIS[cam].xpy = itemFlt[5];
            mMagTab[index].matIS[cam].ypx = itemFlt[6];
            mMagTab[index].matIS[cam].ypy = itemFlt[7];
          } else {
            mMagTab[index].stageCalibrated[cam] = inTime;
            mMagTab[index].matStage[cam].xpx = itemFlt[4];
            mMagTab[index].matStage[cam].xpy = itemFlt[5];
            mMagTab[index].matStage[cam].ypx = itemFlt[6];
            mMagTab[index].matStage[cam].ypy = itemFlt[7];
            mMagTab[index].stageCalFocus[cam] = itemFlt[8];
          }
        }

      } else if (NAME_IS("FoundPixelSize")) {
        inTime = itemInt[1];
        if (timeStamp - inTime < 60 * PIXEL_LIFETIME_HOURS) {
          index = mWinApp->mProcessImage->GetPixelTimeStamp();
          mWinApp->mProcessImage->SetPixelTimeStamp(B3DMAX(itemInt[1], index));
          pixSizCamera->push_back((int)itemInt[2]);
          pixSizMagInd->push_back((int)itemInt[3]);
          addedRotation->push_back((int)itemInt[4]);
          pixelSizes->push_back(itemFlt[5]);
          gridRotations->push_back(itemFlt[6]);
        }

      } else if (!strItems[0].IsEmpty())
        mWinApp->AppendToLog("Unrecognized entry in short term calibration file " + 
          strFileName + " : " + strLine);
    }
    if (err > 0)
      retval = err;
      
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  if (!anyDoseCal && calExpired)
    mWinApp->AppendToLog("The dose calibration has expired; you can recalibrate it with"
      " Tasks - Calibrate Electron Dose");
  return retval;
}


void CParameterIO::WriteShortTermCal(CString strFileName)
{
  int i, j, k, index;
  int *times = mWinApp->mScope->GetLastLongOpTimes();
  std::map<std::string, int> *customTimes = mWinApp->mMacroProcessor->GetCustomTimeMap();
  CString oneState, oneTime;
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  FilterParams *filtP = mWinApp->GetFilterParams();
  NavParams *navP = mWinApp->GetNavParams();
  int *DEdarkRefTimes = mWinApp->mGainRefMaker->GetLastDEdarkRefTime();
  int err = 0;
  int foundPix = mWinApp->mProcessImage->GetPixelTimeStamp();
  ShortVec *pixSizCamera, *pixSizMagInd, *addedRotation;
  std::vector<float> *pixelSizes, *gridRotations;
  mWinApp->mProcessImage->GetPixelArrays(&pixSizCamera, &pixSizMagInd, &addedRotation,
    &pixelSizes, &gridRotations);

  try {
    // Open the file for writing, 
    mFile = new CStdioFile(strFileName, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);

    mFile->WriteString("SerialEMShortTermCal\n");
    for (i = 1; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
      for (j = 0; j < 2; j++) {
        for (k = 0; k < 2; k++) {
          index = i * 4 + j * 2 + k;
          if (doseTables[index].dose) {
            oneState.Format("DoseCalibration %d %d %f %f %d %d %d\n", i, j, 
              doseTables[index].intensity, doseTables[index].dose, 
              doseTables[index].timeStamp, k, doseTables[index].currentAperture);
            mFile->WriteString(oneState);
          }
        }
      }
    }

    if (filtP->alignedSlitWidth) {
      oneState.Format("FilterAlignment %f %f %d %f %d %d %s\n", filtP->refineZLPOffset,
        filtP->alignedSlitWidth, filtP->alignedMagInd, filtP->alignZLPTimeStamp,
        filtP->cumulNonRunTime, mWinApp->MinuteTimeStamp(), getenv("USERNAME"));
      mFile->WriteString(oneState);
    }
    if (filtP->lastFeiZLPshift > EXTRA_VALUE_TEST) {
      oneState.Format("LastFeiZLPshift %f\n", filtP->lastFeiZLPshift);
      mFile->WriteString(oneState);
    }

    for (i = 1; i < MAX_MAGS; i++) {
      for (j = 0; j < MAX_CAMERAS; j++) {
        if (mMagTab[i].calibrated[j]) {
          oneState.Format("ImageShiftMatrix %d %d %d %f %f %f %f\n", 
            mMagTab[i].calibrated[j], 
            i, j, mMagTab[i].matIS[j].xpx, mMagTab[i].matIS[j].xpy,
            mMagTab[i].matIS[j].ypx, mMagTab[i].matIS[j].ypy);
          mFile->WriteString(oneState);
        }
        if (mMagTab[i].stageCalibrated[j]) {
          oneState.Format("StageCalibration %d %d %d %f %f %f %f %f\n",  
            mMagTab[i].stageCalibrated[j], i, j, mMagTab[i].matStage[j].xpx, 
            mMagTab[i].matStage[j].xpy, mMagTab[i].matStage[j].ypx, 
            mMagTab[i].matStage[j].ypy, mMagTab[i].stageCalFocus[j]);
          mFile->WriteString(oneState);
        }
      }
    }

    for (i = 0; i < (int)pixSizCamera->size(); i++) {
      oneState.Format("FoundPixelSize %d %d %d %d %f %f\n", foundPix, pixSizCamera->at(i),
        pixSizMagInd->at(i), addedRotation->at(i), pixelSizes->at(i), 
        gridRotations->at(i));
      mFile->WriteString(oneState);
    }

    if (!navP->autosaveFile.IsEmpty()) {
      CString name = getenv("USERNAME");
      name = name.Trim();
      name.Replace(" ", "_");
      oneState.Format("NavAutosave %s %s\n", (LPCTSTR)name, (LPCTSTR)navP->autosaveFile);
      mFile->WriteString(oneState);
    }

    if (mWinApp->mScope->GetNoScope()) {
      WriteInt("FakeMagIndex", mWinApp->mScope->GetFakeMagIndex());
      WriteInt("FakeScreenPos", mWinApp->mScope->GetFakeScreenPos());
    }

    err = mWinApp->mBeamAssessor->GetCurrentAperture();
    if (err)
      WriteInt("LastC2Aperture", err);

    oneState = "LastLongOpTimes";
    err = 0;
    for (i = 0; i < MAX_LONG_OPERATIONS; i++) {
      if (times[i])
        err = 1;
      oneTime.Format(" %d", times[i]);
      oneState += oneTime;
    }
    if (err > 0)
      mFile->WriteString(oneState + "\n");

    if (mWinApp->mScope->GetScopeCanFlashFEG())
      WriteInt("FEGFlashCount", mWinApp->mScope->GetFegFlashCounter());
    
    for (std::map<std::string, int>::iterator it = customTimes->begin();
      it != customTimes->end(); it++) {
      oneState.Format("LastCustomTime %s %d\n", it->first.c_str(), it->second);
      mFile->WriteString(oneState);
    }

    if (DEdarkRefTimes[0] > 0 || DEdarkRefTimes[1] > 0) {
      oneState.Format("LastDEdarkRefTimes %d %d\n", DEdarkRefTimes[0], DEdarkRefTimes[1]);
        mFile->WriteString(oneState);
    }

    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing short-term calibrations to file " + strFileName;
    AfxMessageBox(message, MB_EXCLAME);
    err = 1;
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
}

  
int CParameterIO::ReadFlybackTimes(CString & fileName)
{
  CArray<FlybackTime, FlybackTime> *fbArray = mWinApp->mCalibTiming->GetFlybackArray();
  FlybackTime fbTime;
  int retval = 0;
  int err;
  CString strLine, dateTime;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];

  try {
    // Open the file for reading, verify that it is a flyback time file
    mFile = new CStdioFile(fileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadAndParse(strLine, strItems, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (strItems[0] != "SerialEMFlybackTimes") {
      retval = 1;
      AfxMessageBox("File not recognized as a flyback time file", MB_EXCLAME);
    }
    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                            MAX_TOKENS)) == 0) {
        if (itemInt[2] <= 0 || itemInt[3] <= 0 || itemEmpty[7]) {
          AfxMessageBox("Incorrect entry in flyback time file "
            + fileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          dateTime = strItems[0] + " " + strItems[1];
          strncpy(fbTime.dateTime, (LPCTSTR)dateTime, DATETIME_SIZE);
          fbTime.dateTime[DATETIME_SIZE - 1] = 0x00;
          fbTime.binning = itemInt[2];
          fbTime.xSize = itemInt[3];
          fbTime.magIndex = itemInt[4];
          fbTime.exposure = (float)itemDbl[5];
          fbTime.flybackTime = (float)itemDbl[6];
          fbTime.startupDelay = (float)itemDbl[7];
          fbArray->Add(fbTime);
        }
      
    }
    if (err > 0)
      retval = err;
      
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    retval = 1;
  } 
  delete mFile;
  mFile = NULL;
  return retval;
}

void CParameterIO::WriteFlybackTimes(CString & fileName)
{
  CArray<FlybackTime, FlybackTime> *fbArray = mWinApp->mCalibTiming->GetFlybackArray();
  FlybackTime fbTime;
  CString strn;

  if (!fbArray->GetSize())
    return;
  try {

    // Open the file for writing, and wrire each line
    mFile = new CStdioFile(fileName, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);
    mFile->WriteString("SerialEMFlybackTimes\n");
    for (int i = 0; i < fbArray->GetSize(); i++) {
      fbTime = fbArray->GetAt(i);
      strn.Format("%s %d %4d %2d %6.3f %6.1f %.3f\n", fbTime.dateTime, fbTime.binning, 
        fbTime.xSize, fbTime.magIndex, fbTime.exposure, fbTime.flybackTime, 
        fbTime.startupDelay);
      mFile->WriteString(strn);
    }
    mFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing measured flyback times to file " + fileName;
    AfxMessageBox(message, MB_EXCLAME);
  } 
  delete mFile;
  mFile = NULL;
}


void CParameterIO::SetDefaultCameraControls(int which, ControlSet *cs,
                      int cameraSizeX, int cameraSizeY)
{
  int minSize = cameraSizeX < cameraSizeY ? cameraSizeX : cameraSizeY;
  int minBin = minSize / 2048;
  if (minBin < 1)
    minBin = 1;

  switch(which) {
  case VIEW_CONSET:
  case SEARCH_CONSET:
    // Default values for VIEW Control Set
    InitializeControlSet(cs, cameraSizeX, cameraSizeY);
    cs->binning = minSize / 512;
    cs->exposure = 0.05f;
    cs->shuttering = USE_DUAL_SHUTTER;
    cs++;
    break;
    
  case FOCUS_CONSET:
    // Default values for focus Control Set
    InitializeControlSet(cs, cameraSizeX, cameraSizeY);
    cs->binning = minBin;
    cs->exposure = 0.5f;
    cs->shuttering = USE_DUAL_SHUTTER;
    cs->left = cameraSizeX / 4;
    cs->top = cameraSizeY * 3 / 8;
    cs->right =cameraSizeX * 3 / 4;
    cs->bottom = cameraSizeY * 5 / 8;
    cs++;
    break;
    
  case TRIAL_CONSET:
    // Default values for Trial Control Set
    InitializeControlSet(cs, cameraSizeX, cameraSizeY);
    cs->binning = minSize / 512;
    cs->exposure = 0.05f;
    cs->shuttering = USE_DUAL_SHUTTER;
    cs++;
    break;
    
  case RECORD_CONSET:
  case MONT_USER_CONSET:
    // Default values for Record 2 Control Set
    InitializeControlSet(cs, cameraSizeX, cameraSizeY);
    cs->binning = minBin;  
    cs->exposure = 1.0f;
    cs->shuttering = USE_DUAL_SHUTTER;
    cs->numAverage = 10;
    cs++;
    break;
    
  case PREVIEW_CONSET:
    // Default values for Preview Control Set
    InitializeControlSet(cs, cameraSizeX, cameraSizeY);
    cs->binning = minSize / 512;
    cs->exposure = 0.01f;
    cs->shuttering = USE_BEAM_BLANK;
    break;
  }
}

// Overall defaults that apply to all or almost all consets
void CParameterIO::InitializeControlSet(ControlSet * cs, int sizeX, int sizeY)
{
    cs->mode = SINGLE_FRAME;
    cs->processing = GAIN_NORMALIZED;
    cs->forceDark = 0;
    cs->onceDark = 0;
    cs->drift = 0.0;
    cs->left = 0;
    cs->top = 0;
    cs->right = sizeX;
    cs->bottom = sizeY;
    cs->averageDark = 0;
    cs->averageOnce = 0;
    cs->numAverage = 4;
    cs->removeXrays = 0;
    cs->K2ReadMode = K2_LINEAR_MODE;
    cs->doseFrac = 0;
    cs->alignFrames = 0;
    cs->useFrameAlign = 1;
    cs->frameTime = 0.04f;
    cs->filterType = 0;
    cs->sumK2Frames = 0;
    cs->numSkipBefore = 0;
    cs->numSkipAfter = 0;
    cs->DEsumCount = 0;
}


///////////////////////////////////////////////////////////////////////////////////
// UTILITIES

int CParameterIO::ReadAndParse(CString &strLine, CString *strItems, int maxItems, 
  bool useQuotes)
{
  return ReadAndParse(mFile, strLine, strItems, maxItems, useQuotes);
}

// Read a line from the given file into strLine and parse into strItems, at most maxItems 
// Returns 0 for something read, -1 for EOF, and 1 for error
int CParameterIO::ReadAndParse(CStdioFile *file, CString &strLine, CString *strItems, 
  int maxItems, bool useQuotes)
{
  try {
    if (!file->ReadString(strLine))
      return -1;          // return for EOF
  }
  catch (CFileException *err) {
    err->Delete();
    return 1;                       // return for error
  }

  return ParseString(strLine, strItems, maxItems, useQuotes);
}

// Read and parse a line and fill in empty, int, and double arrays, and an optional float
// array.  Empty items are assigned 0
int CParameterIO::ReadSuperParse(CString &strLine, CString *strItems, BOOL *itemEmpty,
  int *itemInt, double *itemDbl, int maxItems)
{
  return ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, NULL, maxItems);
}

int CParameterIO::ReadSuperParse(CString &strLine, CString *strItems, BOOL *itemEmpty, 
  int *itemInt, double *itemDbl, float *itemFlt, int maxItems)
{
  int err = ReadAndParse(strLine, strItems, maxItems);
  for (int i = 0; i < maxItems; i++) {
    itemEmpty[i] = strItems[i].IsEmpty();
    itemDbl[i] = 0.;
    itemInt[i] = 0;
    if (!itemEmpty[i]) {
      itemDbl[i] = atof(strItems[i]);
      itemInt[i] = atoi(strItems[i]);
    }
    if (itemFlt)
      itemFlt[i] = (float)itemDbl[i];
  }
  return err;
}

// Parse a string into separate items in an array of CStrings
// AllowComment = 1 allows comment in first token, > 1 allows any comment
int CParameterIO::ParseString(CString strLine, CString *strItems, int maxItems, 
  bool useQuotes, int allowComment)
{
  int i;

  // Make all strings empty
  for (i = 0; i < maxItems; i++)
    strItems[i] = "";
  CString strCopy = strLine;

  // Get one token after another until the copy is empty
  for (i = 0; i < maxItems; i++) {
    FindToken(strCopy, strItems[i], useQuotes, (allowComment && !i) || allowComment > 1);
    if (strCopy.IsEmpty())
      return 0;
  }

  return 2; // Error if there are too many items
}

void CParameterIO::FindToken(CString &strCopy, CString &strItem, bool useQuotes,
  bool allowComment)
{
  int index, quoteInd, closeInd, spaceInd, tabInd;
  bool stripQuotes = false;
  strItem = "";
  char *quoteChar = "\'";

  // Trim white space at left; done if no more string
  strCopy.TrimLeft();
  if (strCopy.IsEmpty())
    return;

  // If we have a comment, done
  if (!allowComment && strCopy.Find('#') == 0) {
    strCopy = "";
    return;
  }

  // Find next space or tab
  index = strCopy.FindOneOf(" \t");

  // If parsing quotes, see if there is one before this space and not at end of line
  if (useQuotes) {
    quoteInd = strCopy.Find(quoteChar);
    if (quoteInd >= 0 && (quoteInd < index || index < 0) && 
      quoteInd < strCopy.GetLength() - 1) {

      // Get closing quote: ignore if none
      closeInd = strCopy.Find(quoteChar, quoteInd + 1);
      if (closeInd > 0) {

        // Find next space or tab, replace index and set flag
        spaceInd = strCopy.Find(' ', closeInd);
        tabInd = strCopy.Find('\t', closeInd);
        if (spaceInd > 0 && tabInd > 0)
          index = B3DMIN(spaceInd, tabInd);
        else if (spaceInd > 0)
          index = spaceInd;
        else
          index = tabInd;
        stripQuotes = true;
      }
    }
  }

  // If there are no more, then the item is the rest of string - but trim off cr-lf
  if (index < 0) {
    strItem = strCopy;
    strItem.TrimRight(" \t\r\n");
    strCopy = "";
  } else {

    // There must be one down the line, split the string
    strItem = strCopy.Left(index);
    strCopy = strCopy.Right(strCopy.GetLength() - index);
  }

  // Get rid of the quotes
  if (stripQuotes)
    strItem.Replace(quoteChar, "");
}

// Take some items off the front of a line and return the rest as one string, trimmed
// AllowComment = 1 allows comment in first token, > 1 allows any comment
void CParameterIO::StripItems(CString strLine, int numItems, CString & strCopy,
  bool keepIndent, int allowComment)
{
  CString strItem;
  strCopy = strLine;
  for (int i = 0; i < numItems; i++) {
    FindToken(strCopy, strItem, false, (allowComment && !i) || allowComment > 1);
    if (strCopy.IsEmpty())
      return;
  }
  if (!keepIndent)
    strCopy.TrimLeft();
  else if (strCopy.GetLength() > 0 && strCopy.GetAt(0) == ' ')
    strCopy = strCopy.Mid(1);
  strCopy.TrimRight(" \t\r\n");
  if (mCheckForComments && strCopy.Find(" #") >= 0)
    mPropsWithComments += "\r\n" + strLine;
}

// Check that the file starts with the given tag.  Check for UTF-8 and remove that for
// checking.  Check for other encoding BOM's from Wikipedia and give specific message
// Otherwise give generic message
int CParameterIO::CheckForByteOrderMark(CString &item0, const char * tag,
  CString &filename, const char *descrip)
{
  CString mess;
  int len = item0.GetLength();
  unsigned char first = (unsigned char)item0.GetAt(0);
  if (len >= 3 && first == 0xEF && (unsigned char)item0.GetAt(1) == 0xBB && 
    (unsigned char)item0.GetAt(2) == 0xBF)
    item0 = item0.Mid(3);
  if (item0 == tag)
    return 0;
  if (first == 0xFE || first == 0xFF || first == 0xF7 || first == 0xDD || first == 0x0E
    || first == 0xFB || first == 0x84 || 
    (!first && len > 1 && (unsigned char)item0.GetAt(2) == 0xFE))
    mess.Format("%s is in a strange encoding and cannot be read as a %s file",
    (LPCTSTR)filename, descrip);
  else {
    if (!strlen(tag))
      return 0;
    mess.Format("%s does not start with %s and cannot be read as a %s file",
      (LPCTSTR)filename, tag, descrip);
  }
  AfxMessageBox(mess, MB_EXCLAME);
  return 1;
}

// Test for characters above 127 in properties
void CParameterIO::CheckForSpecialChars(CString & strLine)
{
  int ind, len = strLine.GetLength();
  const unsigned char *linePtr = (const unsigned char *)(LPCTSTR)strLine;
  for (ind = 0; ind < len; ind++) {
    if (linePtr[ind] == '#' || linePtr[ind] == '\r' || linePtr[ind] == '\n')
      return;
    if (linePtr[ind] > 127) {
      AfxMessageBox("Warning: there is a special or unicode character in line\n" +
        strLine + "\n\n" + "Properties may not be read correctly", MB_EXCLAME);
      return;
    }
  }
}

void CParameterIO::WritePlacement(const char *string, int open, WINDOWPLACEMENT *place)
{
  CString oneState;
  if (place->rcNormalPosition.right == NO_PLACEMENT)
    return;
  oneState.Format("%s %d %d %d %d %d %d %d %d %d %d\n", string,
    open, place->showCmd,
    place->ptMaxPosition.x, place->ptMaxPosition.y,
    place->ptMinPosition.x, place->ptMinPosition.y,
    place->rcNormalPosition.left, place->rcNormalPosition.top,
    place->rcNormalPosition.right, place->rcNormalPosition.bottom);
  mFile->WriteString(oneState);
}

// Read one macro after "Macro" has been found
int CParameterIO::ReadOneMacro(int iset, CString &strLine, CString *strItems, 
  int maxMacros)
{
  int err;
  if (iset < 0)
    return 1;
  CString *macros = mWinApp->GetMacros();

  // If the set number is past the max in the version that wrote the file, it is a 
  // one-line script and should be shifted to be in same relative spot here
  if (mMaxReadInMacros && iset >= mMaxReadInMacros)
    iset += MAX_MACROS - mMaxReadInMacros;

  // For forward compatibility, it is not an error to have too many macros; just flush it
  if (iset < maxMacros)
    macros[iset] = "";
  while ((err = ReadAndParse(strLine, strItems, 4)) == 0 || err == 2) {

    // Error 2 is too many items, which is fine, but reset it to 0
    err = 0;
    if (NAME_IS("EndMacro"))
      break;
    strLine.TrimRight("\r\n");
    if (iset < maxMacros)
      macros[iset] += strLine+ "\r\n";
  }
  if (iset < MAX_MACROS) {
    if (mWinApp->mMacroEditer[iset])
      mWinApp->mMacroEditer[iset]->TransferMacro(false);
    else
      mWinApp->mMacroProcessor->ScanForName(iset);
  }
  return err;
}

// Write all macros to the current file
void CParameterIO::WriteAllMacros(int numWrite)
{
  CString *macros = mWinApp->GetMacros();
  CString macCopy;
  WriteInt("MaxMacros", MAX_MACROS);
  for (int i = 0; i < numWrite; i++) {
    if (!macros[i].IsEmpty()) {
      WriteInt("Macro", i);
      macCopy = macros[i];

      // For writing with WriteString, need to remove \r
      macCopy.Replace("\r\n", "\n");
      macCopy.TrimRight("\r\n");
      mFile->WriteString(macCopy);
      mFile->WriteString("\nEndMacro\n");
    }
  }
}

void CParameterIO::WriteString(CString format, CString strValue)
{
  CString strOut;
  format += "\t%s\n";
  strOut.Format(format, strValue);
  mFile->WriteString(strOut);
}

void CParameterIO::WriteInt(CString format, int iVal)
{
  CString strOut;
  format += "\t%d\n";
  strOut.Format(format, iVal);
  mFile->WriteString(strOut);
}

void CParameterIO::WriteFloat(CString format, float fVal)
{
  CString strOut;
  format += "\t%f\n";
  strOut.Format(format, fVal);
  mFile->WriteString(strOut);
}

void CParameterIO::WriteDouble(CString format, double dVal)
{
  CString strOut;
  format += "\t%f\n";
  strOut.Format(format, dVal);
  mFile->WriteString(strOut);
}


// Convert an entry list to a single string.  Type = 1 for a set of doubles, 
// 2 for int,double, 3 for int, 4 for short.  precision is # of decimal places
CString CParameterIO::EntryListToString(int type, int precision, int numVals, int *iVals, 
                                        double *dVals, short *sVals)
{
  char format[10];
  CString str = "";
  CString str2;
  if (type == 1)
    sprintf(format, "%%.%df", precision);
  else if (type == 2)
    sprintf(format, "%%d,%%.%df", precision);

  for (int i = 0; i < numVals; i++) {
    if (type == 1) {
      str2.Format(format, dVals[i]);
    } else if (type == 2) {
      str2.Format(format, iVals[i], dVals[i]);
    } else if (type == 4) {
      str2.Format("%d", sVals[i]);
    } else
      str2.Format("%d", iVals[i]);
  if (type < 3)
      str2.TrimRight('0');
    if (i)
      str += " ";
    str += str2;
  }
  return str;
}

// Convert a string containing an entry list to list of values.  Type = 1 for doubles 
// only, 2 for int,double, 3 for int.  Returns 2 for too many entries, 1 for format error
// such as too few values in entry, -1 for format warning (possibly extra values)
int CParameterIO::StringToEntryList(int type, CString str, int &numVals, int *iVals, 
  double *dVals, int maxVals, bool splitCommas, short *sVals)
{
  int err, ind, ind2;
  int twocomma = 0;
  double temp;
  CString *strItems = new CString[maxVals];
  CString strBit;
  if (splitCommas)
    str.Replace(',', ' ');
  err = ParseString(str, strItems, maxVals);
  numVals = 0;
  for (int i = 0; i < maxVals; i++) {
    if (strItems[i].IsEmpty())
      break;
    ind = strItems[i].Find(',');
    if (type != 2) {
      if (ind == 0) {
        err = 1;
        break;
      }
      if (ind > 0 && ind < strItems[i].GetLength() - 1)
        twocomma = -1;
      if (type == 1)
        dVals[numVals++] = atof((LPCTSTR)strItems[i]);
      else if (type == 4)
        sVals[numVals++] = (short)atoi((LPCTSTR)strItems[i]);
      else
        iVals[numVals++] = atoi((LPCTSTR)strItems[i]);
    } else {

      // Error 1 if there are not two numbers there
      ind2 = strItems[i].Find(',', ind + 1);
      if (ind <= 0 || ind == strItems[i].GetLength() - 1 || ind2 == ind + 1) {
        err = 1;
        break;
      }

      // Return -1 if there are multiple commas
      if (ind2 > 0)
        twocomma = -1;
      else
        ind2 = strItems[i].GetLength();

      // Sscanf did not work!
      strBit = strItems[i].Left(ind);
      temp = atof((LPCTSTR)strBit);
      strItems[i] = strItems[i].Mid(ind + 1, ind2 - ind - 1);
      dVals[numVals] = atof((LPCTSTR)strItems[i]);
      iVals[numVals++] = (int)temp;
    }
  }
  delete [] strItems;
  return err ? err: twocomma;
}

void CParameterIO::ReadBadColumns(CString *strItems, UShortVec &badColumnStart, 
                                  ShortVec &badColumnWidth)
{
  CString message;
  int col, index = 0;
  while (!strItems[index + 1].IsEmpty()) {
    col = atoi((LPCTSTR)strItems[index + 1]);
    CorDefAddBadColumn(col, badColumnStart, badColumnWidth);
    index++;
  }
}

void CParameterIO::ReadPartialBad(CString *strItems, int *itemInt, UShortVec &partialBadCol, 
                                  ShortVec &partialBadWidth, UShortVec &partialBadStartY, 
                                  UShortVec &partialBadEndY,
                                  const char *colText, CString &strLine)
{
  CString message;

  if (strItems[4].IsEmpty() || !strItems[5].IsEmpty()) {
    message.Format("Partial bad %s entry needs exactly four numbers "
      "in %s", colText, strLine);
    AfxMessageBox(message, MB_EXCLAME);
  } else {
    CorDefAddPartialBadCol(&itemInt[1], partialBadCol, partialBadWidth, partialBadStartY,
      partialBadEndY);
  }
}

// Report any options set in the special options submenu
void CParameterIO::ReportSpecialOptions(void)
{
  bool any = false;
  CString mess;
  if (mWinApp->mTSController->GetSkipBeamShiftOnAlign())
    PrintAnOption(any, "Special option is set to skip beam shift when aligning "
      "during tilt series");
  if (mWinApp->mScope->GetUsePiezoForLDaxis())
     PrintAnOption(any, "Special option is set to use piezo movement for Low Dose"
       " on-axis shift");
  if (mShiftManager->GetDisableAutoTrim())
    PrintAnOption(any, "Special option is set to disable automatic trimming of dark "
    "borders in Autoalign for some cases");
  if (mWinApp->mTSController->GetAllowContinuous())
    PrintAnOption(any, "Special option is set to allow continuous mode in tilt series");
  if (mWinApp->mFocusManager->GetNormalizeViaView())
    PrintAnOption(any, "Special option is set to normalize through View before "
    "autofocus in Low Dose");
  if (mWinApp->mScope->GetAdjustFocusForProbe())
     PrintAnOption(any, "Special option is set to adjust focus when changing between "
     "nanoprobe and microprobe");
  if (mWinApp->mCamera->GetNoNormOfDSdoseFrac())
    PrintAnOption(any, "Special option is set for no gain normalization of returned"
    " sum for dark-subtracted K2 dose frac shots");
  if (mWinApp->mCamera->GetAntialiasBinning() <= 0)
    PrintAnOption(any, "Special option is set NOT to use antialiased reduction instead"
      " of binning for all K2/K3 shots");
  if (mWinApp->mScope->GetNormAllOnMagChange())
    PrintfToLog("Special option is set to normalize all lenses on %s", 
    mWinApp->mScope->GetNormAllOnMagChange() > 1 ? "all mag changes" : 
    "mag changes within LM");
  if (mWinApp->mScope->GetSkipBlankingInLowDose())
    PrintAnOption(any, "Special option is set to skip blanking in Low Dose when the "
      "screen is up");
  if (mWinApp->mScope->GetIdleTimeToCloseValves() > 0)
    PrintfToLog("Special option is set to turn off beam after %d minutes of inactivity",
      mWinApp->mScope->GetIdleTimeToCloseValves());
  if (mWinApp->mScope->GetIdleTimeToStopEmission() > 0)
    PrintfToLog("Special option is set to turn off emission after %d minutes of "
      "inactivity", mWinApp->mScope->GetIdleTimeToStopEmission());
  if (mWinApp->mComplexTasks->GetTasksUseViewNotSearch())
    PrintAnOption(any, "Special option is set to use View for tasks in Low Dose even if"
      " Search has more appropriate magnification");
  if (mWinApp->mLowDoseDlg.GetTieFocusTrialPos())
    PrintAnOption(any, "Special option is set to keep Low Dose Trial and Focus at same"
      " position");
  if (mWinApp->GetStartingProgram() && any && !mWinApp->GetMinimizedStartup())
    mWinApp->AppendToLog(" ");
}

void CParameterIO::PrintAnOption(bool &anyDone, const char * mess)
{
  if (mWinApp->GetStartingProgram() && !anyDone && !mWinApp->GetMinimizedStartup())
    mWinApp->AppendToLog(" ");
  mWinApp->AppendToLog(mess, mWinApp->GetMinimizedStartup() ? LOG_SWALLOW_IF_CLOSED :
    LOG_OPEN_IF_CLOSED);
  anyDone = true;
}

// Outputs a short or float vector in as many lines as it takes
void CParameterIO::OutputVector(const char *key, int size, ShortVec *shorts, 
  FloatVec *floats)
{
  CString oneState, entry;
  int numLines, numWrite, numLeft, j, i, indOut = 0;
  if (!size)
    return;
  numLines = (size + MAX_TOKENS - 3) / (MAX_TOKENS - 2);
  for (j = 0; j < numLines; j++) {
    oneState = key;
    numLeft = size - j * (MAX_TOKENS - 2);
    numWrite = B3DMIN(MAX_TOKENS - 2, numLeft);
    for (i = 0; i < numWrite; i++) {
      if (shorts)
        entry.Format(" %d", shorts->at(indOut++));
      else
        entry.Format(" %f", floats->at(indOut++));
      oneState += entry;
    }
    mFile->WriteString(oneState + "\n");
  }
}

#define WRITE_INDEXED_LIST(a) { \
  CString line, val; \
  line = keyword; \
  for (int ind = 0; ind < numVal; ind++) { \
    val.Format(a, values[ind]);  \
    line += val;   \
  }  \
  mFile->WriteString(line + "\n");  \
}

void CParameterIO::WriteIndexedInts(const char *keyword, int *values, int numVal)
  WRITE_INDEXED_LIST(" %d");

void CParameterIO::WriteIndexedFloats(const char *keyword, float *values, int numVal)
  WRITE_INDEXED_LIST(" %f");

// Fills a float array with items for each binning
void CParameterIO::StoreFloatsPerBinning(CString *strItems, const char *descrip, int iset,
  CString &strFileName, float *values)
{
  CString message, cam;
  int index = 0;
  while (!strItems[index + 1].IsEmpty() && index < MAX_BINNINGS) {
    values[index] = 
      (float)atof((LPCTSTR)strItems[index + 1]);
    if (values[index] <= 0.) {
      if (iset >= 0)
        cam.Format(" for camera %d", iset);
      message.Format("Inappropriate %s (%f)%s\n"
        "in properties file %s", descrip, values[index], (LPCTSTR)cam, strFileName);
      AfxMessageBox(message, MB_EXCLAME);
      values[index] = 1.;
    }
    index++;
  }
}

// Macros and Function for setting a property value from script
#define INT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c(B3DNINT(value));
#define BOOL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c(value != 0.);
#define FLOAT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c((float)value);
#define DBL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c(value);
int CParameterIO::MacroSetProperty(CString name, double value)
{
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES
  bool recognized = true, recognized2 = true, recognized30 = true, recognized35 = true;
  if (false) {
  }
#define PROP_TEST_SECT1
#include "PropertyTests.h"
#undef PROP_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define PROP_TEST_SECT2
#include "PropertyTests.h"
#undef PROP_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
    recognized = true;
  }
#define PROP_TEST_SECT30
#include "PropertyTests.h"
#undef PROP_TEST_SECT30
  else
    recognized30 = false;

  if (recognized || recognized30) {
    recognized = true;
  }
#define PROP_TEST_SECT35
#include "PropertyTests.h"
#undef PROP_TEST_SECT35
  else
    recognized35 = false;

  if (recognized || recognized35) {
    recognized = true;
  }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
  else {
    SEMMessageBox(name + " is not a recognized property or cannot be set by script "
      "command");
    return 1;
  }
  return 0;
}
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#undef DBL_PROP_TEST

// Macros and Function for setting a property value from menu
#define INT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    ival = b##Get##c();       \
    if (KGetOneInt(propStr + a, ival)) { \
      b##Set##c(ival); \
      ivalIn = ival;   \
    }  \
  }
#define BOOL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    ival = b##Get##c() ? 1 : 0;       \
    if (KGetOneInt(boolStr + a, ival)) { \
      b##Set##c(ival != 0); \
      ivalIn = ival;   \
    }  \
  }
#define FLOAT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    fval = b##Get##c();       \
    if (KGetOneFloat(propStr + a, fval, 2)) { \
      b##Set##c(fval); \
      fvalIn = fval;  \
    }  \
  }
#define DBL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    fval = (float)b##Get##c();       \
    if (KGetOneFloat(propStr + a, fval, 2)) { \
      b##Set##c((double)fval);  \
      fvalIn = fval;  \
    }  \
  }

void CParameterIO::UserSetProperty(void)
{
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES
  CString propStr = "Enter value for property ";
  CString boolStr = "Enter 0 or 1 for property ";
  CString name;
  int ival = 0, ivalIn = -1000000000;
  float fval = 0., fvalIn = EXTRA_NO_VALUE;
  bool recognized = true, recognized2 = true, recognized30 = true, recognized35 = true;
  if (!KGetOneString("Enter full name of property to set (case insensitive):", name))
    return;
  if (false) {
  }
#define PROP_TEST_SECT1
#include "PropertyTests.h"
#undef PROP_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define PROP_TEST_SECT2
#include "PropertyTests.h"
#undef PROP_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
    recognized = true;
  }
#define PROP_TEST_SECT30
#include "PropertyTests.h"
#undef PROP_TEST_SECT30
  else
    recognized30 = false;

  if (recognized || recognized30) {
    recognized = true;
  }
#define PROP_TEST_SECT35
#include "PropertyTests.h"
#undef PROP_TEST_SECT35
  else
    recognized35 = false;

  if (recognized || recognized35) {
  }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
  else {
    AfxMessageBox(name + " is not a recognized property or cannot be set by this "
      "command");
  }
  if (ivalIn > -1000000000)
    PrintfToLog("Property %s set to %d", (LPCTSTR)name, ival);
  if (fvalIn > EXTRA_VALUE_TEST)
    PrintfToLog("Property %s set to %f", (LPCTSTR)name, fval);
}

// Macros and function for setting a camera property
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#undef DBL_PROP_TEST
#define INT_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    camP->b = (int)value;
#define BOOL_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    camP->b = value != 0;
#define FLOAT_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    camP->b = (float)value;

int CParameterIO::MacroSetCamProperty(CameraParameters *camP, CString &name, double value)
{
  bool recognized = true;
  if (false) {
  }
#define CAM_PROP_SECT1
#include "PropertyTests.h"
#undef CAM_PROP_SECT1
  else
    recognized = false;
  if (recognized) {
    recognized = true;
  }
#define CAM_PROP_SECT2
#include "PropertyTests.h"
#undef CAM_PROP_SECT2
  else {
    return 1;
  }
  return 0;
}
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST


// Macros and Function for getting a property value from script
#define INT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    value = b##Get##c();
#define BOOL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    value = b##Get##c();
#define FLOAT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    value = b##Get##c();
#define DBL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    value = b##Get##c();
int CParameterIO::MacroGetProperty(CString name, double &value)
{
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES
  bool recognized = true, recognized2 = true, recognized30 = true, recognized35 = true;
  if (false) {
  }
#define PROP_TEST_SECT1
#include "PropertyTests.h"
#undef PROP_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define PROP_TEST_SECT2
#include "PropertyTests.h"
#undef PROP_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
    recognized = true;
  }
#define PROP_TEST_SECT30
#include "PropertyTests.h"
#undef PROP_TEST_SECT30
  else
    recognized30 = false;

  if (recognized || recognized30) {
    recognized = true;
  }
#define PROP_TEST_SECT35
#include "PropertyTests.h"
#undef PROP_TEST_SECT35
  else
    recognized35 = false;

  if (recognized || recognized35) {
  }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
  else {
    return 1;
  }
  return 0;
}

// Macros and function for getting a camera property
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#undef DBL_PROP_TEST
#define INT_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = camP->b;
#define BOOL_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = camP->b;
#define FLOAT_PROP_TEST(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = camP->b;

int CParameterIO::MacroGetCamProperty(CameraParameters *camP, CString &name, 
  double &value)
{
  bool recognized = true;
  if (false) {
  }
#define CAM_PROP_SECT1
#include "PropertyTests.h"
#undef CAM_PROP_SECT1
  else
    recognized = false;
  if (recognized) {
    recognized = true;
  }
#define CAM_PROP_SECT2
#include "PropertyTests.h"
#undef CAM_PROP_SECT2
  else {
    return 1;
  }
  return 0;
}
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST


// Macros and function for getting a user setting from a macro
#define INT_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    value = b##Get##c();
#define BOOL_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
   value = b##Get##c() ? 1: 0;
#define FLOAT_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0)   \
    value = b##Get##c();
#define DOUBLE_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0)   \
    value = b##Get##c();
#define INT_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = b;
#define BOOL_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = (b != 0) ? 1. : 0.;
#define FLOAT_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    value = b;

int CParameterIO::MacroGetSetting(CString name, double &value)
{
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  bool recognized = true, recognized15 = true, recognized2 = true, recognized25 = true;
  if (false) {
  }
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define SET_TEST_SECT15
#include "SettingsTests.h"
#undef SET_TEST_SECT15
  else
    recognized15 = false;

  if (recognized || recognized15) {
    recognized = true;
  }
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
    recognized = true;
  }
#define SET_TEST_SECT25
#include "SettingsTests.h"
#undef SET_TEST_SECT25
  else
    recognized25 = false;

  if (recognized || recognized25) {
    recognized = true;
  }
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3
  else {
    return 1;
  }
  return 0;
}
#undef INT_SETT_GETSET
#undef BOOL_SETT_GETSET
#undef FLOAT_SETT_GETSET
#undef DOUBLE_SETT_GETSET
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN


// Macros and function for changing a user setting from a macro
#define INT_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c(ival);
#define BOOL_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0) \
    b##Set##c(ival != 0);
#define FLOAT_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0)   \
    b##Set##c((float)value);
#define DOUBLE_SETT_GETSET(a, b, c) \
  else if (name.CompareNoCase(a) == 0)   \
    b##Set##c(value);
#define INT_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    b = ival;
#define BOOL_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    b = ival != 0;
#define FLOAT_SETT_ASSIGN(a, b) \
  else if (name.CompareNoCase(a) == 0) \
    b = (float)value;

int CParameterIO::MacroSetSetting(CString name, double value)
{
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  int ival;
  double valCopy = value;
  B3DCLAMP(valCopy, -2.147e9, 2.147e9);
  ival = B3DNINT(valCopy);
  bool recognized = true, recognized15 = true, recognized2 = true, recognized25 = true;
  if (false) {
  }
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define SET_TEST_SECT15
#include "SettingsTests.h"
#undef SET_TEST_SECT15
  else
    recognized15 = false;

  if (recognized || recognized15) {
    recognized = true;
  }
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
  else
    recognized2 = false;

  if (recognized || recognized2) {
    recognized = true;
  }
#define SET_TEST_SECT25
#include "SettingsTests.h"
#undef SET_TEST_SECT25
  else
    recognized25 = false;

  if (recognized || recognized25) {
    recognized = true;
  }
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3
  else {
    return 1;
  }
  return 0;
}
#undef INT_SETT_GETSET
#undef BOOL_SETT_GETSET
#undef FLOAT_SETT_GETSET
#undef DOUBLE_SETT_GETSET
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN
