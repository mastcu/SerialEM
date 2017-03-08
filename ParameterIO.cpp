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
#include "FilterTasks.h"
#include "NavigatorDlg.h"
#include "NavHelper.h"
#include "EMmontageController.h"
#include "MultiTSTasks.h"
#include "Mailer.h"
#include "BaseSocket.h"
#include "AutoTuning.h"
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
#define DOSE_LIFETIME_HOURS  24
#define FILTER_LIFETIME_MINUTES  60
#define IS_STAGE_LIFETIME_HOURS 22
#define PIXEL_LIFETIME_HOURS 24

// 5/27/06: Properties read in under 5 ms even with this test
#define MatchNoCase(a) (strItems[0].CompareNoCase(a) == 0)
#define NAME_IS(a) (strItems[0] == (a))

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParameterIO::CParameterIO()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
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

int CParameterIO::ReadSettings(CString strFileName)
{
  int retval = 0;
  int err;
  int index, mag, spot, GIF;
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  ControlSet *cs;
  CString strLine, strCopy, unrecognized = "";
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  int nLines = 0;
  FileOptions *defFileOpt = mWinApp->mDocWnd->GetDefFileOpt();
  int *initialDlgState = mWinApp->GetInitialDlgState();
  int *macroButtonNumbers = mWinApp->mCameraMacroTools.GetMacroNumbers();
  float pctLo, pctHi;
  float *gridLim = mWinApp->mNavHelper->GetGridLimits();
  WINDOWPLACEMENT winPlace;
  WINDOWPLACEMENT *place;
  RECT *dlgPlacements = mWinApp->GetDlgPlacements();
  NavParams *navParams = mWinApp->GetNavParams();
  CookParams *cookParams = mWinApp->GetCookParams();
  AutocenParams acParams;
  AutocenParams *acParmP;
  RangeFinderParams *tsrParams = mWinApp->GetTSRangeParams();
  int *tssPanelStates = mWinApp->GetTssPanelStates();
  BOOL recognized, recognized2;
  LowDoseParams *ldp;
  StateParams *stateP;
  CArray<StateParams *, StateParams *> *stateArray = mWinApp->mNavHelper->GetStateArray();
  mWinApp->mNavHelper->ClearStateArray();
  CArray<FrameAliParams, FrameAliParams> *faParamArray =
    mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams faParam, *faData;
  BOOL *useGPU4K2Ali = mWinApp->mCamera->GetUseGPUforK2Align();
  int faLastFileIndex = -1, faLastArrayIndex = -1;
  mWinApp->mCamera->SetFrameAliDefaults(faParam, "4K default set", 4, 0.06f);
  mWinApp->SetAbsoluteDlgIndex(false);

  try {
    // Open the file for reading, verify that it is a settings file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (strItems[0] != "SerialEMSettings") {
      retval = 1;
      AfxMessageBox("File not recognized as a settings file", MB_EXCLAME);
    }

    // Clear out low dose params now in case there are none in the file
    mWinApp->InitializeLDParams();

    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS))
           == 0) {
          recognized = true;
          recognized2 = true;
      if (NAME_IS("SystemPath")) {

        // There could be multiple words - have to assume 
        // separated by spaces
        CString path = strItems[1];
        index = 2;
        while (!strItems[index].IsEmpty() && index < MAX_TOKENS)
          path += " " + strItems[index++];
        mWinApp->mDocWnd->SetSystemPath(path);

      } else if (NAME_IS("CameraControlSet")) {
        int iset = itemInt[1];
        int iCam = 0;
        if (!strItems[2].IsEmpty())
          iCam = itemInt[2];
        cs = &mConSets[iset + iCam * MAX_CONSETS];
        while ((err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                                     MAX_TOKENS)) == 0) {
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
            cs->exposure = (float)itemDbl[1];
          else if (NAME_IS("DriftSettling"))
            cs->drift = (float)itemDbl[1];
          else if (NAME_IS("LineSync"))
            cs->lineSync = itemInt[1];
          else if (NAME_IS("DynamicFocus"))
            cs->dynamicFocus = itemInt[1];
          else if (NAME_IS("K2ReadMode"))
            cs->K2ReadMode = itemInt[1];
          else if (NAME_IS("DoseFracMode"))
            cs->doseFrac = itemInt[1];
          else if (NAME_IS("FrameTime"))
            cs->frameTime = (float)itemDbl[1];
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
            cs->numSkipBefore = itemInt[1];
          else if (NAME_IS("SkipFramesAfter"))
            cs->numSkipAfter = itemInt[1];
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
            for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
              cs->summedFrameList.push_back(itemInt[index]);
          } else if (NAME_IS("UserFrameFracs")) {
            for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
              cs->userFrameFractions.push_back((float)itemDbl[index]);
          } else if (NAME_IS("UserSubframeFracs")) {
            for (index = 1; index < MAX_TOKENS && !itemEmpty[index]; index++)
              cs->userSubframeFractions.push_back((float)itemDbl[index]);
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
            CString message;
            message.Format("For camera parameter set %d: %s\n", iset, strLine);
            unrecognized += message;
          }

        }
        if (err > 0) {
          retval = err;
          break;
        }

      } else if (NAME_IS("Macro")) {
        err = ReadOneMacro(itemInt[1], strLine, strItems);
        if (err > 0) {
          retval = err;
          break;
        }

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
        if (!itemEmpty[2] && itemInt[1] >= 0 && itemInt[1] < MAX_CAMERAS)
        mCamParam[itemInt[1]].DE_FramesPerSec = (float)itemDbl[2];
      } else if (NAME_IS("DEAutosaveFormat")) {
        mWinApp->mDEToolDlg.SetFormatForAutoSave(itemInt[1]);
      } else if (NAME_IS("PercentDisplayTruncationLo")) {
        mWinApp->GetDisplayTruncation(pctLo, pctHi);
        pctLo = (float)itemDbl[1];
        mWinApp->SetDisplayTruncation(pctLo, pctHi);
      } else if (NAME_IS("PercentDisplayTruncationHi")) {
        mWinApp->GetDisplayTruncation(pctLo, pctHi);
        pctHi = (float)itemDbl[1];
        mWinApp->SetDisplayTruncation(pctLo, pctHi);
      } else if (NAME_IS("CopyToBufferOnSave")) {
      } else if (NAME_IS("ProtectRecordImages")) {
        mBufferManager->SetConfirmBeforeDestroy(3, itemInt[1]);
      } else if (NAME_IS("AlignOnSave")) {
      } else if (NAME_IS("MontageUseContinuous")) {
        montParam->useContinuousMode = itemInt[1] != 0;
        montParam->continDelayFactor = (float)itemDbl[2];
      } else if (NAME_IS("MontageNoDriftCorr")) {
        montParam->noDriftCorr = itemInt[1] != 0;
        montParam->noHQDriftCorr = itemInt[2] != 0;
      } else if (NAME_IS("MontageFilterOptions")) {
        mWinApp->mMontageController->SetUseFilterSet2(itemInt[1] != 0);
        mWinApp->mMontageController->SetUseSet2InLD(itemInt[2] != 0);
        mWinApp->mMontageController->SetDivFilterSet1By2(itemInt[3] != 0);
        mWinApp->mMontageController->SetDivFilterSet2By2(itemInt[4] != 0);
      } else if (NAME_IS("TIFFcompression")) {
        defFileOpt->compression = itemInt[1];
      } else if (NAME_IS("AutofocusEucenAbsParams")) {
        mWinApp->mFocusManager->SetEucenAbsFocusParams(itemDbl[1], itemDbl[2], 
        (float)itemDbl[3], (float)itemDbl[4], itemInt[5] != 0, itemInt[6] != 0);
      } else if (NAME_IS("AssessMultiplePeaksInAlign")) {
      } 
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
      else
        recognized = false;
    
      if (recognized) {
      
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
        mWinApp->SetAutoBacklashNewMap(itemInt[1]);
        mWinApp->SetAutoBacklashMinField((float)itemDbl[2]);
      } else if (NAME_IS("NavigatorAcquireParams")) {
        navParams->acqAutofocus = itemInt[1] != 0;
        navParams->acqFineEucen = itemInt[2] != 0;
        navParams->acqRealign = itemInt[3] != 0;
        navParams->acqRestoreOnRealign = itemInt[4] != 0;
        navParams->acqRoughEucen = itemInt[5] != 0;
        navParams->nonTSacquireType = itemInt[6];
        navParams->macroIndex = itemInt[7];
        if (!strItems[8].IsEmpty())
          navParams->acqCloseValves = itemInt[8] != 0;
        if (!strItems[9].IsEmpty())
          navParams->acqSendEmail = itemInt[9] != 0;
        if (!strItems[10].IsEmpty())
          navParams->acqFocusOnceInGroup = itemInt[10] != 0;
        if (!strItems[11].IsEmpty()) {
          navParams->preMacroInd = itemInt[11];
          navParams->acqRunPremacro = itemInt[12] != 0;
          navParams->preMacroIndNonTS = itemInt[13];
          navParams->acqRunPremacroNonTS = itemInt[14] != 0;
          navParams->acqSendEmailNonTS = itemInt[15] != 0;
        }

      } else if (NAME_IS("CookerParams")) {
        cookParams->magIndex = itemInt[1];
        cookParams->spotSize = itemInt[2];
        cookParams->intensity = itemDbl[3];
        cookParams->targetDose = itemInt[4];
        cookParams->timeInstead = itemInt[5] != 0;
        cookParams->minutes = (float)itemDbl[6];
        cookParams->trackImage = itemInt[7] != 0;
        cookParams->cookAtTilt = itemInt[8] != 0;
        cookParams->tiltAngle = (float)itemDbl[9];

      } else if (NAME_IS("AutocenterParams")) {
        acParmP = &acParams;
        acParmP->camera = itemInt[1];
        acParmP->magIndex = itemInt[2];
        acParmP->spotSize = itemInt[3];
        acParmP->intensity = itemDbl[4];
        acParmP->binning = itemInt[5];
        acParmP->exposure = itemDbl[6];
        acParmP->useCentroid = itemInt[7];
        acParmP->probeMode = itemInt[8] < 0 ? 1 : itemInt[8];
        mWinApp->mMultiTSTasks->AddAutocenParams(acParmP);

      } else if (NAME_IS("RangeFinderParams")) {
        index = B3DMIN(1, B3DMAX(0, itemInt[1]));
        tsrParams[index].eucentricity = itemInt[2] != 0;
        tsrParams[index].walkup = itemInt[3] != 0;
        tsrParams[index].autofocus = itemInt[4] != 0;
        tsrParams[index].imageType = itemInt[5];
        tsrParams[index].startAngle = (float)itemDbl[6];
        tsrParams[index].endAngle = (float)itemDbl[7];
        tsrParams[index].angleInc = (float)itemDbl[8];
        tsrParams[index].direction = itemInt[9];

      } else if (NAME_IS("NavigatorStockFile"))
        StripItems(strLine, 1, navParams->stockFile);
      else if (NAME_IS("ProcessOverlayChannels"))
        mWinApp->mProcessImage->SetOverlayChannels(strItems[1]);
      else if (NAME_IS("ImportOverlayChannels"))
        navParams->overlayChannels = strItems[1];
      else if (NAME_IS("RemoteControlParams")) {
        mWinApp->SetShowRemoteControl(itemInt[1] != 0);
        mWinApp->mRemoteControl.SetBeamIncrement((float)itemDbl[2]);
        mWinApp->mRemoteControl.SetIntensityIncrement((float)itemDbl[3]);
        mWinApp->mRemoteControl.m_bMagIntensity = itemInt[4] != 0;

      // Tool dialog placements and states now work when reading settings
      } else if (NAME_IS("ToolDialogStates") || NAME_IS("ToolDialogStates2")) {
        index = 0;
        mWinApp->SetAbsoluteDlgIndex(NAME_IS("ToolDialogStates2"));
        while (!strItems[index + 1].IsEmpty() && index < MAX_DIALOGS) {
          initialDlgState[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
      } else if (NAME_IS("ToolDialogPlacement")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_DIALOGS || strItems[5].IsEmpty()) {
            AfxMessageBox("Error in panel placement line in settings file "
              + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          ConstrainWindowPlacement(&itemInt[2], &itemInt[3], &itemInt[4], &itemInt[5]);
          dlgPlacements[index].left = itemInt[2];
          dlgPlacements[index].top = itemInt[3];
          dlgPlacements[index].right = itemInt[4];
          dlgPlacements[index].bottom = itemInt[5];
        }

      } else if (NAME_IS("MacroButtonNumbers")) {
        index = 0;
        while (!strItems[index + 1].IsEmpty()) {
          macroButtonNumbers[index] = atoi((LPCTSTR)strItems[index + 1]);
          index++;
        }
      } else if (NAME_IS("MacroToolbarButtons")) {
        mWinApp->mMacroProcessor->SetNumToolButtons(itemInt[1]);
        if (!itemEmpty[2])
          mWinApp->mMacroProcessor->SetToolButHeight(itemInt[2]);

      } else if (NAME_IS("WindowPlacement")) {
        mWinApp->GetWindowPlacement(&winPlace);
        winPlace.showCmd = itemInt[1];
        if (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
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
        ConstrainWindowPlacement(&winPlace);
        if (winPlace.rcNormalPosition.right > 0 && 
          winPlace.rcNormalPosition.bottom > 0)
          mWinApp->SetWindowPlacement(&winPlace);

      } else if (NAME_IS("LogPlacement") || NAME_IS("NavigatorPlacement") ||
        NAME_IS("MeterPlacement") || NAME_IS("DosePlacement") || 
        NAME_IS("RotAlignPlacement") ||NAME_IS("StageToolPlacement") ||
        NAME_IS("ReadDlgPlacement") || NAME_IS("StatePlacement") ||
        NAME_IS("MacroToolPlacement") || NAME_IS("MacroEditerPlacement")) {
        if (strItems[10].IsEmpty()) {
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
          else if (NAME_IS("StatePlacement")) {
            mWinApp->SetOpenStateWithNav(itemInt[1] != 0);
            place = mWinApp->mNavHelper->GetStatePlacement();
          } else if (NAME_IS("ReadDlgPlacement"))
            place = mWinApp->mDocWnd->GetReadDlgPlacement();
          else if (NAME_IS("StageToolPlacement"))
            place = mWinApp->GetStageToolPlacement();
          else if (NAME_IS("MacroEditerPlacement"))
            place = mWinApp->mMacroProcessor->GetEditerPlacement();
          else if (NAME_IS("MacroToolPlacement")) {
            mWinApp->SetReopenMacroToolbar(itemInt[1] != 0);
            place = mWinApp->mMacroProcessor->GetToolPlacement();
          } else if (NAME_IS("LogPlacement")) {
            mWinApp->SetReopenLog(itemInt[1] != 0);
            place = mWinApp->GetLogPlacement();
          }
          place->showCmd = itemInt[2];
          place->ptMaxPosition.x = itemInt[3];
          place->ptMaxPosition.y = itemInt[4];
          place->ptMinPosition.x = itemInt[5];
          place->ptMinPosition.y = itemInt[6];
          place->rcNormalPosition.left = itemInt[7];
          place->rcNormalPosition.top = itemInt[8];
          place->rcNormalPosition.right = itemInt[9];
          place->rcNormalPosition.bottom = atoi((LPCTSTR)strItems[10]);
          ConstrainWindowPlacement(place);
        }

      } else if (NAME_IS("StateParameters")) {
        stateP = mWinApp->mNavHelper->NewStateParam(false);
        stateP->lowDose = -itemInt[1];
        stateP->camIndex = itemInt[2];
        stateP->magIndex = itemInt[3];
        stateP->spotSize = itemInt[4];
        stateP->intensity = itemDbl[5];
        stateP->slitIn = itemInt[6] != 0;
        stateP->energyLoss = (float)itemDbl[7];
        stateP->slitWidth = (float)itemDbl[8];
        stateP->zeroLoss = itemInt[9] != 0;
        stateP->binning = itemInt[10];
        stateP->xFrame = itemInt[11];
        stateP->yFrame = itemInt[12];
        stateP->exposure = (float)itemDbl[13];
        stateP->drift = (float)itemDbl[14];
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
        stateP->focusAxisPos = EXTRA_NO_VALUE;

      } else if (NAME_IS("StateName")) {
        index = itemInt[1];
        if (index < 0 || index >= stateArray->GetSize()) {
          AfxMessageBox("Index out of range in state name line in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
       } else {
          stateP = stateArray->GetAt(index);
          StripItems(strLine, 2, stateP->name);
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
            stateP->lowDose = 1;
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
          ldp->probeMode = 1;
          if (!itemEmpty[6]) {
            ldp->slitIn = itemInt[7] != 0;
            ldp->slitWidth = (float)itemDbl[8];
            ldp->energyLoss = (float)itemDbl[9];
          }
          if (!itemEmpty[10]) 
            ldp->zeroLoss = itemInt[10] != 0;
          if (!itemEmpty[11]) {
            ldp->beamDelX = itemDbl[11];
            ldp->beamDelY = itemDbl[12];
          }
          if (!itemEmpty[15]) {
            ldp->beamAlpha = (float)itemDbl[13];
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
        }

      } else if (NAME_IS("ImageXrayCriteria")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_CAMERAS || strItems[4].IsEmpty()) {
          AfxMessageBox("Incorrect entry in settings file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          mCamParam[index].imageXRayAbsCrit = (float)itemDbl[2];
          mCamParam[index].imageXRayNumSDCrit = (float)itemDbl[3];
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
      } else if (NAME_IS("LowDoseViewShift")) {
        mWinApp->mLowDoseDlg.mViewShiftX = itemDbl[1];
        mWinApp->mLowDoseDlg.mViewShiftY = itemDbl[2];
      } else if(NAME_IS("ResetRealignIterationCriterion")) {
        mWinApp->mComplexTasks->SetRSRAUserCriterion((float)itemDbl[1]);
      } else
        recognized2 = false;

      if (recognized || recognized2) {

      }
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3

      else if (NAME_IS("TiltSeriesLowMagIndex")) {
        mTSParam->lowMagIndex[0] = itemInt[1];
        mTSParam->lowMagIndex[1] = itemInt[2];
        mTSParam->lowMagIndex[2] = itemInt[3];
      
      // Retired 2/13/04 - moved to properties
      // Dose cal moved to short term cal 12/21/05
      // Retired shoot after autofocus 12/22/06
      } else if (NAME_IS("TiltSeriesXFitInterval") ||
        NAME_IS("TiltSeriesYFitInterval") ||
        NAME_IS("TiltSeriesZFitInterval") ||
        NAME_IS("TiltSeriesXMinForQuadratic") ||
        NAME_IS("TiltSeriesYMinForQuadratic") ||
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
          mTSParam->varyArray[mTSParam->numVaryItems].angle = (float)itemDbl[1];
          mTSParam->varyArray[mTSParam->numVaryItems].plusMinus = itemInt[2] != 0;
          mTSParam->varyArray[mTSParam->numVaryItems].linear = itemInt[3] != 0;
          mTSParam->varyArray[mTSParam->numVaryItems].type = itemInt[4];
          mTSParam->varyArray[mTSParam->numVaryItems++].value = (float)itemDbl[5];
        }
      } else if (NAME_IS("TiltSeriesBidirParams")) {
        mTSParam->doBidirectional = itemInt[1] != 0;
        mTSParam->bidirAngle = (float)itemDbl[2];
        mTSParam->anchorBidirWithView = itemInt[3] != 0;
        mTSParam->walkBackForBidir = itemInt[4] != 0;
        if (!itemEmpty[5])
          mTSParam->retainBidirAnchor = itemInt[5] != 0;
      } else if (NAME_IS("TiltSeriesBDAnchorMags")) {
        for (index = 0; index < 6; index++)
          mTSParam->bidirAnchorMagInd[index] = itemInt[index + 1];
      } else if (NAME_IS("TiltSeriesRunMacro")) {
        mWinApp->mTSController->SetRunMacroInTS(itemInt[1] != 0);
        mWinApp->mTSController->SetMacroToRun(itemInt[2]), 
        mWinApp->mTSController->SetStepAfterMacro(itemInt[3]);
      } else if (NAME_IS("TSTermOnHighExposure")) {
        mWinApp->mTSController->SetTermOnHighExposure(itemInt[1] != 0);
        mWinApp->mTSController->SetMaxExposureIncrease((float)itemDbl[2]);
      } else if (MatchNoCase("TiltSeriesBidirDimPolicy")) {
        mWinApp->mTSController->SetEndOnHighDimImage(itemInt[1] != 0);
        mWinApp->mTSController->SetDimEndsAbsAngle(itemInt[2]);
        mWinApp->mTSController->SetDimEndsAngleDiff(itemInt[3]);
      } else if (MatchNoCase("CloseValvesDuringMessageBox")) {
        mWinApp->mTSController->SetMessageBoxCloseValves(itemInt[1] != 0);
        mWinApp->mTSController->SetMessageBoxValveTime((float)itemDbl[2]);
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
        faData->rad2Filt1 = (float)itemDbl[5];
        faData->rad2Filt2 = (float)itemDbl[6];
        faData->rad2Filt3 = (float)itemDbl[7];
        faData->rad2Filt4 = (float)itemDbl[8]; 
        faData->hybridShifts = itemInt[9] != 0;
        faData->sigmaRatio = (float)itemDbl[10];
        faData->refRadius2 = (float)itemDbl[11]; 
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
            faData->truncLimit = (float)itemDbl[3];
            faData->antialiasType = itemInt[4]; 
            faData->stopIterBelow = (float)itemDbl[5];
            faData->groupRefine = itemInt[6] != 0;
            faData->keepPrecision = itemInt[7] != 0;
            faData->outputFloatSums = itemInt[8] != 0;
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
  if (!faParamArray->GetSize()) {
    mWinApp->mCamera->SetFrameAliDefaults(faParam, "4K default set", 4, 0.06f);
    faParamArray->Add(faParam);
    mWinApp->mCamera->SetFrameAliDefaults(faParam, "8K default set", 6, 0.05f);
    faParamArray->Add(faParam);
  }
  
  // Remove state params that are supposed to be low dose but don't have LD param
  err = 0;
  for (index = (int)stateArray->GetSize() - 1; index >= 0; index--) {
    stateP = stateArray->GetAt(index);
    if (stateP->lowDose < 0) {
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
  FileOptions *fileOpt = mWinApp->mDocWnd->GetFileOpt();
#define SETTINGS_MODULES
#include "SettingsTests.h"
#undef SETTINGS_MODULES
  CString *macros = mWinApp->GetMacros();
  DialogTable *dlgTable = mWinApp->GetDialogTable();
  int *dlgColorIndex = mWinApp->GetDlgColorIndex();
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
  WINDOWPLACEMENT *readPlace = mWinApp->mDocWnd->GetReadDlgPlacement();
  WINDOWPLACEMENT *stageToolPlace = mWinApp->GetStageToolPlacement();
  WINDOWPLACEMENT *rotAlignPlace = mWinApp->mNavHelper->GetRotAlignPlacement();
  int *macroButtonNumbers = mWinApp->mCameraMacroTools.GetMacroNumbers();
  mWinApp->CopyCurrentToCameraLDP();
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  NavParams *navParams = mWinApp->GetNavParams();
  CookParams *cookParams = mWinApp->GetCookParams();
  CArray<AutocenParams *, AutocenParams *> *autocenArray = 
    mWinApp->mMultiTSTasks->GetAutocenParams();
  RangeFinderParams *tsrParams = mWinApp->GetTSRangeParams();
  int *tssPanelStates = mWinApp->GetTssPanelStates();
  AutocenParams *acParams, *acParmP;
  LowDoseParams *ldp;
  StateParams *stateP;
  CArray<StateParams *, StateParams *> *stateArray = mWinApp->mNavHelper->GetStateArray();
  CArray<FrameAliParams, FrameAliParams> *faParamArray =
    mWinApp->mCamera->GetFrameAliParams();
  FrameAliParams faParam;
  BOOL *useGPU4K2Ali = mWinApp->mCamera->GetUseGPUforK2Align();

  try {
    // Open the file for writing, 
    mFile = new CStdioFile(strFileName, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);

    mFile->WriteString("SerialEMSettings\n");
    WriteString("SystemPath", mWinApp->mDocWnd->GetSysPathForSettings());

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
        oneState = "ChannelIndex";
        for (i =0; i < MAX_STEM_CHANNELS; i++) {
          macCopy.Format(" %d", cs->channelIndex[i]);
          oneState += macCopy;
        }
        mFile->WriteString(oneState + "\n");
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
        oneState.Format("DE12FPS %d %f\n", iCam, mCamParam[iCam].DE_FramesPerSec);
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
    oneState.Format("AutofocusEucenAbsParams %f %f %f %f %d %d\n", 
      focusMan->GetEucenMinAbsFocus(), focusMan->GetEucenMaxAbsFocus(),
      focusMan->GetEucenMinDefocus(), focusMan->GetEucenMaxDefocus(),
      focusMan->GetUseEucenAbsLimits() ? 1 : 0, focusMan->GetTestOffsetEucenAbs() ? 1 :0);
    mFile->WriteString(oneState);
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
#define SET_TEST_SECT3
#include "SettingsTests.h"
#undef SET_TEST_SECT3

    WriteAllMacros();

    oneState.Format("AutoBacklashNewMap %d %f\n", mWinApp->GetAutoBacklashNewMap(),
      mWinApp->GetAutoBacklashMinField());
    mFile->WriteString(oneState);
    oneState.Format("NavigatorAcquireParams %d %d %d %d %d %d %d %d %d %d %d %d %d %d"
      " %d\n", navParams->acqAutofocus ? 1 : 0, navParams->acqFineEucen ? 1 : 0,
      navParams->acqRealign ? 1 : 0, navParams->acqRestoreOnRealign ? 1 : 0,
      navParams->acqRoughEucen ? 1 : 0, navParams->nonTSacquireType, 
      navParams->macroIndex, navParams->acqCloseValves ? 1 : 0, 
      navParams->acqSendEmail ? 1 : 0, navParams->acqFocusOnceInGroup ? 1 : 0,
      navParams->preMacroInd, navParams->acqRunPremacro ? 1 : 0, 
      navParams->preMacroIndNonTS, navParams->acqRunPremacroNonTS ? 1 : 0, 
      navParams->acqSendEmailNonTS ? 1 : 0);
    mFile->WriteString(oneState);
    oneState.Format("CookerParams %d %d %f %d %d %f %d %d %f -999 -999\n", 
      cookParams->magIndex, cookParams->spotSize, cookParams->intensity, 
      cookParams->targetDose, cookParams->timeInstead ? 1 : 0, cookParams->minutes,
      cookParams->trackImage ? 1 : 0, 
      cookParams->cookAtTilt ? 1 : 0, cookParams->tiltAngle);
    mFile->WriteString(oneState);
    if (!navParams->stockFile.IsEmpty()) {
      oneState.Format("NavigatorStockFile %s\n", navParams->stockFile);
      mFile->WriteString(oneState);
    }
    for (i = 0; i < autocenArray->GetSize(); i++) {
      acParams = autocenArray->GetAt(i);
      if (acParams->intensity >= 0.) {
        acParmP = mWinApp->mMultiTSTasks->LookupAutocenParams(acParams->camera, 
          acParams->magIndex, acParams->spotSize, acParams->probeMode, 
          acParams->intensity, j);
        if (j == i) {
          oneState.Format("AutocenterParams %d %d %d %f %d %f %d %d -999\n", 
            acParams->camera, acParams->magIndex, acParams->spotSize, acParams->intensity,
            acParams->binning, acParams->exposure, acParams->useCentroid ? 1 : 0, 
            acParams->probeMode);
          mFile->WriteString(oneState);
        }
      }
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

    // Save states of tool windows, but set montage closed
    for (i = 0; i < MAX_DIALOGS; i++) {
      j = mWinApp->LookupToolDlgIndex(i);
      int state = j >= 0 ? dlgTable[j].state : 0;
      if (i == MONTAGE_DIALOG_INDEX)
        state &= ~1;
      oneState.Format(" %d", state);
      states += oneState;
    }
    states += "\n";
    mFile->WriteString(states);

    for (i = 0; i < mWinApp->GetNumToolDlg(); i++) {
      if (dlgTable[i].state & TOOL_FLOATDOCK) {
        dlgTable[i].pDialog->GetWindowPlacement(&winPlace);
        oneState.Format("ToolDialogPlacement %d %d %d %d %d\n", dlgColorIndex[i], 
          winPlace.rcNormalPosition.left, winPlace.rcNormalPosition.top,
          winPlace.rcNormalPosition.right, winPlace.rcNormalPosition.bottom);
        mFile->WriteString(oneState);
      }
    }

    oneState.Format("MacroButtonNumbers %d %d %d\n", macroButtonNumbers[0],
      macroButtonNumbers[1], macroButtonNumbers[2]);
    mFile->WriteString(oneState);
    oneState.Format("RemoteControlParams %d %f %f %d\n", mWinApp->GetShowRemoteControl() ? 
      1 : 0, mWinApp->mRemoteControl.GetBeamIncrement(), 
      mWinApp->mRemoteControl.GetIntensityIncrement(), 
      mWinApp->mRemoteControl.m_bMagIntensity ? 1: 0);
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
    WritePlacement("ReadDlgPlacement", 0, readPlace);
    WritePlacement("StageToolPlacement", 0, stageToolPlace);
    WritePlacement("MacroToolPlacement", mWinApp->mMacroToolbar ? 1 : 0, toolPlace);
    WritePlacement("MacroEditerPlacement", 0, 
      mWinApp->mMacroProcessor->FindEditerPlacement());
    oneState.Format("MacroToolbarButtons %d %d\n", 
      mWinApp->mMacroProcessor->GetNumToolButtons(), 
      mWinApp->mMacroProcessor->GetToolButHeight());
    mFile->WriteString(oneState);

    // Save stored states BEFORE low dose params
    for (i = 0; i < stateArray->GetSize(); i++) {
      stateP = stateArray->GetAt(i);
      oneState.Format("StateParameters %d %d %d %d %f %d %f %f %d %d %d %d %f %f "
        "%d %d %d %f %d %d %d %d %d %d\n", stateP->lowDose, stateP->camIndex, 
        stateP->magIndex, 
        stateP->spotSize, stateP->intensity, stateP->slitIn ? 1 : 0, stateP->energyLoss,
        stateP->slitWidth, stateP->zeroLoss ? 1 : 0, stateP->binning, stateP->xFrame, 
        stateP->yFrame, stateP->exposure, stateP->drift, stateP->shuttering,
        stateP->K2ReadMode, stateP->probeMode, stateP->frameTime, stateP->doseFrac, 
        stateP->saveFrames, stateP->processing, stateP->alignFrames, 
        stateP->useFrameAlign, stateP->faParamSetInd);
        mFile->WriteString(oneState);
        if (!stateP->name.IsEmpty()) {
          oneState.Format("StateName %d %s\n", i, (LPCTSTR)stateP->name);
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

        // Set number, mag or -cam length, spot, intensity, axis offset, 
        // 0-2 for regular/GIF/STEM or 3 for state, slit in, slit width, energy loss,
        // zero loss, beam X offset, beam Y offset, alpha, diffraction focus, beam tilt X,
        // beam tilt Y
        oneState.Format("LowDoseParameters %d %d %d %f %f %d %d %f %f %d %f %f %f %f %f"
          " %f %d %d %f %f\n",
          ldi, (ldp->magIndex ? ldp->magIndex : -ldp->camLenIndex), ldp->spotSize, 
          ldp->intensity, ldp->axisPosition, ldj, ldp->slitIn ? 1 : 0, ldp->slitWidth, 
          ldp->energyLoss, ldp->zeroLoss, ldp->beamDelX, ldp->beamDelY, ldp->beamAlpha, 
          ldp->diffFocus, ldp->beamTiltDX, ldp->beamTiltDY, ldp->probeMode, 
          ldp->darkFieldMode, ldp->dfTiltX, ldp->dfTiltY);
        mFile->WriteString(oneState);
      }
    }
    oneState.Format("LowDoseViewShift %f %f\n", mWinApp->mLowDoseDlg.mViewShiftX, 
      mWinApp->mLowDoseDlg.mViewShiftY); 
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
    oneState = "";
    for (i = 0; i < 6; i++) {
      macCopy.Format(" %d", mTSParam->bidirAnchorMagInd[i]);
      oneState += macCopy;
    }
    WriteString("TiltSeriesBDAnchorMags", oneState);
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
    oneState = "TSSetupPanelStates ";
    for (i = 0; i < NUM_TSS_PANELS - 1; i++) {
      macCopy.Format(" %d", tssPanelStates[i]);
      oneState += macCopy;
    }
    oneState += "\n";
    mFile->WriteString(oneState);
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
    oneState.Format("GridMapLimits %f %f %f %f\n", gridLim[0], gridLim[1], gridLim[2], 
      gridLim[3]);
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
      oneState.Format("FrameAlignParams2 %d %d %f %d %f %d %d %d\n", i, 
        faParam.truncate ? 1 : 0, faParam.truncLimit, faParam.antialiasType, 
        faParam.stopIterBelow, faParam.groupRefine ? 1 : 0, faParam.keepPrecision ? 1 : 0,
        faParam.outputFloatSums ? 1 : 0);
      mFile->WriteString(oneState);
      oneState.Format("FrameAliSetName %d %s\n", i, (LPCTSTR)faParam.name);
      mFile->WriteString(oneState);
    }
    oneState.Format("FrameAliGlobals %d %d %d %d %d\n", useGPU4K2Ali[0] ? 1 : 0, 
      useGPU4K2Ali[1] ? 1 : 0, useGPU4K2Ali[2] ? 1 : 0, 
      mWinApp->GetFrameAlignMoreOpen() ? 1 : 0,
      mWinApp->mCamera->GetAlignWholeSeriesInIMOD() ? 1 : 0); 
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


}
#undef INT_SETT_GETSET
#undef BOOL_SETT_GETSET
#undef FLOAT_SETT_GETSET
#undef DOUBLE_SETT_GETSET
#undef INT_SETT_ASSIGN
#undef BOOL_SETT_ASSIGN
#undef FLOAT_SETT_ASSIGN

// Read macros from a file with the given name
void CParameterIO::ReadMacrosFromFile(CString filename)
{
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  int retval = 0, numLoaded = 0;
  int err;
  try {
    mFile = new CStdioFile(filename, CFile::modeRead | CFile::shareDenyWrite);

    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
      MAX_TOKENS)) == 0) {
        if (NAME_IS("Macro")) {
          err = ReadOneMacro(itemInt[1], strLine, strItems);
          if (err > 0) {
            retval = err;
            break;
          }
          numLoaded++;
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
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
  if (retval)
    AfxMessageBox("An error occurred reading macros from the file", MB_EXCLAME);
  else {
    strLine.Format("Loaded %d macros from the file", numLoaded);
    AfxMessageBox(strLine, MB_OK | MB_ICONINFORMATION);
  }
}

// Write macros to a file with the given name
void CParameterIO::WriteMacrosToFile(CString filename)
{
  try {

    // Open the file for writing, 
    mFile = new CStdioFile(filename, CFile::modeCreate | CFile::modeWrite | 
      CFile::shareDenyWrite);
    WriteAllMacros();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing macros to file " + filename;
    AfxMessageBox(message, MB_EXCLAME);
  } 
  if (mFile) {
    delete mFile;
    mFile = NULL;
  }
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
  int *C2apertures = mWinApp->mBeamAssessor->GetC2Apertures();
  float *radii = mWinApp->mProcessImage->GetFFTCircleRadii();
  BOOL recognized, recognized2, recognized3, recognizedc, recognizedc1;
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  CString message;
  CameraParameters *mCamParam = mWinApp->GetCamParams();
  CameraParameters *camP;
  FileOptions *defFileOpt = mWinApp->mDocWnd->GetDefFileOpt();
  MacroControl *macControl = mWinApp->GetMacControl();
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES  
  NavParams *navParams = mWinApp->GetNavParams();
  RelRotations *relRotations = mWinApp->mShiftManager->GetRelRotations();
  CArray<RotStretchXform, RotStretchXform> *rotXformArray = 
    mWinApp->mShiftManager->GetRotXforms();
  CArray<ChannelSet, ChannelSet> *channelSets = camera->GetChannelSets();
  CArray<ChannelSet, ChannelSet> *blockSets = scope->GetBlockedChannels();
  CArray<PiezoScaling, PiezoScaling> *piezoScalings = 
    mWinApp->mPiezoControl->GetScalings();
  CString *K2FilterNames = camera->GetK2FilterNames();
  HitachiParams *hitachi = mWinApp->GetHitachiParams();
  RotStretchXform rotXform;
  PiezoScaling pzScale;
  ChannelSet chanSet;
  TiffField *tfield;
  int camEntered[MAX_CAMERAS];
  std::set<std::string> genPropSet;
  std::set<std::string> camPropSets[MAX_CAMERAS];
  std::string tmpg[] = {"cameraproperties", "importstagetoimage", "importtiffxfield", 
    "importtiffyfield", "importtiffidfield", "k2filtername", "controlsetname", 
    "othershiftboundaries", "watchgauge", "mappedtodschannel", "detectorblocks", 
    "mutuallyexcludedetectors", "socketserverip", "socketserveripif64", 
    "socketserverport", "socketserverportif64"};
  std::set<std::string> dupOKgenProps(tmpg, tmpg + sizeof(tmpg) / sizeof(tmpg[0]));
  std::string tmpc[] = {"hotpixels", "rotationandpixel", "detectorname", "channelname",
    "rotationstretchxform", "specialrelativerotation", "binningoffset", "hotcolumns",
    "partialbadcolumn", "badcolumns", "partialbadrow", "badrows", "badpixels"};
  std::set<std::string> dupOKcamProps(tmpc, tmpc + sizeof(tmpc) / sizeof(tmpc[0]));
  std::string tmpnv[] = {"endcameraproperties"};
  std::set<std::string> noValOKprops(tmpnv, tmpnv + sizeof(tmpnv) / sizeof(tmpnv[0]));
  memset(&camEntered[0], 0, MAX_CAMERAS * sizeof(int));

#define INT_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemInt[1]);
#define BOOL_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemInt[1] != 0);
#define FLOAT_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c((float)itemDbl[1]);
#define DBL_PROP_TEST(a, b, c) \
  else if (strItems[0].CompareNoCase(a) == 0) \
    b##Set##c(itemDbl[1]);
  
  try {
    // Open the file for reading, verify that it is a properties file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadAndParse(strLine, strItems, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (strItems[0] != "SerialEMProperties") {
      retval = 1;
      AfxMessageBox("File not recognized as a properties file", MB_EXCLAME);
    }

    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                            MAX_TOKENS)) == 0) {
      recognized = true;
      recognized2 = true;
      recognized3 = true;
      recognizedc = true;

      message = strItems[0];
      std::string propLower = (LPCTSTR)message.MakeLower();
      if (!itemEmpty[0]) {
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

      if (MatchNoCase("CameraProperties")) {
        int iset = itemInt[1];
        if (iset < 0 || iset >= MAX_CAMERAS) {
          message.Format("Index in \"CameraProperties %d\" is out of range", iset);
          AfxMessageBox(message, MB_EXCLAME);
          retval = 1;
          break;
        }
        camP = &mCamParam[iset];
        if (camEntered[iset]) {
            message.Format("More than one entry in properties file for camera %d", iset);
            AfxMessageBox(message, MB_EXCLAME);
        }
        camEntered[iset] = 1;
        while ((err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                                     MAX_TOKENS)) == 0) {
          message = strItems[0];
          std::string propLower = (LPCTSTR)message.MakeLower();
          if (!itemEmpty[0]) {
            if (camPropSets[iset].count(propLower) && !dupOKcamProps.count(propLower)) {
              message.Format("%s (property for camera # %d)\r\n", (LPCTSTR)strItems[0],
                iset);
              mDupMessage += message;
            } else
              camPropSets[iset].insert(propLower);
          }

          recognizedc1 = true;
          if (MatchNoCase("EndCameraProperties"))
            break;
          else if (MatchNoCase("CameraSizeX"))
            camP->sizeX = itemInt[1];
          else if (MatchNoCase("CameraSizeY"))
            camP->sizeY = itemInt[1];
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
          } else if (MatchNoCase("MakesUnsignedImages"))
           camP->unsignedImages = itemInt[1] != 0;
          else if (MatchNoCase("FourPortReadout"))
            camP->fourPort = itemInt[1] != 0;
          else if (MatchNoCase("CoordsMustBeMultiple"))
            camP->coordsModulo = itemInt[1] != 0;
          else if (MatchNoCase("GainRefXsizeEven"))
            camP->refSizeEvenX = itemInt[1];
          else if (MatchNoCase("GainRefYsizeEven"))
            camP->refSizeEvenY = itemInt[1];
          else if (MatchNoCase("XMustBeMultipleOf")) {
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
            } else {

              // Otherwise, type is encoded as negative modulo, #2 or #5 means no subareas
              camP->moduloX = -itemInt[1];
              camP->moduloY = camP->moduloX;
              if (camP->moduloX == -2 || camP->moduloX == -5) {
                camP->subareasBad = 2;
              }
            }
          }  else if (MatchNoCase("BasicCorrections"))
            camP->corrections = itemInt[1];
          else if (MatchNoCase("SizeCheckSwapped"))
            camP->sizeCheckSwapped = itemInt[1];
          else if (MatchNoCase("K2Type"))
            camP->K2Type = itemInt[1];
          else if (MatchNoCase("OneViewType")) {
            camP->OneViewType = itemInt[1];
          } else if (MatchNoCase("UseSocket"))
            camP->useSocket = itemInt[1] != 0;
          else if (MatchNoCase("FEICameraType")) {
            camP->FEItype = itemInt[1];
            camP->moduloX = camP->moduloY = -1;
          } else if (MatchNoCase("AMTCameraType"))
            camP->AMTtype = itemInt[1];
          else if (MatchNoCase("DECameraType")) {

            // Direct Electron camera is centered only except DE-12
            camP->DE_camType = itemInt[1];
            camP->centeredOnly = itemInt[1] >= 2 ? 0 : 1;
          } else if(MatchNoCase("DEImageRotation"))
            camP->DE_ImageRot = itemInt[1];
          else if(MatchNoCase("DEImageInvertXAxis"))
            camP->DE_ImageInvertX = itemInt[1]; 
          else if(MatchNoCase("DECameraServerIP"))
            StripItems(strLine, 1,  camP->DEServerIP);
          else if(MatchNoCase("DECameraReadPort"))
            camP->DE_ServerReadPort = itemInt[1]; 
          else if(MatchNoCase("DECameraWritePort"))
            camP->DE_ServerWritePort = itemInt[1]; 
          else if(MatchNoCase("AlsoInsertCamera"))
            camP->alsoInsertCamera = itemInt[1]; 
          else if(MatchNoCase("SamePhysicalCamera"))
            camP->samePhysicalCamera = itemInt[1]; 
          else if (MatchNoCase("TietzCameraType")) {
            camP->TietzType = itemInt[1];
            if (itemInt[1] == 8 || itemInt[1] == 11 || itemInt[1] == 12)
              camP->TietzBlocks = 1024;
          } else if (MatchNoCase("TietzCanPreExpose"))
            camP->TietzCanPreExpose = itemInt[1] != 0;
          else if (MatchNoCase("TietzRestoreBBmode"))
            camP->restoreBBmode = itemInt[1];
          else if (MatchNoCase("TietzGainIndex"))
            camP->TietzGainIndex = itemInt[1];
          else if (MatchNoCase("TietzImageGeometry"))
            camP->TietzImageGeometry = itemInt[1];
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
          else if (MatchNoCase("PluginCameraIndex"))
            camP->cameraNumber = itemInt[1];
          else if (MatchNoCase("ShutterLabel1"))
            StripItems(strLine, 1, camP->shutterLabel1);
          else if (MatchNoCase("ShutterLabel2"))
            StripItems(strLine, 1, camP->shutterLabel2);
          else if (MatchNoCase("ShutterLabel3"))
            StripItems(strLine, 1, camP->shutterLabel3);
          else if (MatchNoCase("PluginCanProcess"))
            camP->canDoProcessing = itemInt[1];
          else if (MatchNoCase("PluginCanPreExpose"))
            camP->canPreExpose = itemInt[1] != 0;
          else if (MatchNoCase("RotationAndFlip"))
            camP->rotationFlip = itemInt[1];
          else if (MatchNoCase("DMRotationAndFlip"))
            camP->DMrotationFlip = itemInt[1];
          else if (MatchNoCase("SetRestoreDMRotFlip"))
            camP->setRestoreRotFlip = itemInt[1];
          else if (MatchNoCase("TaskTargetSize"))
            camP->taskTargetSize = itemInt[1];
          else if (MatchNoCase("ImageRotation"))
            camP->imageRotation = itemInt[1];
          else if (MatchNoCase("InvertFocusRamp"))
            camP->invertFocusRamp = itemInt[1] != 0;
          else if (MatchNoCase("STEMCamera"))
            camP->STEMcamera = itemInt[1] != 0;
          else if (MatchNoCase("AddedFlybackTime"))
            camP->addedFlyback = (float)itemDbl[1];
          else if (MatchNoCase("MinimumPixelTime"))
            camP->minPixelTime = (float)itemDbl[1];
          else if (MatchNoCase("MaximumScanRate")) {
            camP->maxScanRate = (float)itemDbl[1];
            if (itemEmpty[2])
              camP->advisableScanRate = camP->maxScanRate;
            else
              camP->advisableScanRate = (float)itemDbl[2];
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
          } else if (MatchNoCase("NoShutter"))
            camP->noShutter = itemInt[1];
          else if (MatchNoCase("OnlyOneShutter"))
            camP->onlyOneShutter = itemInt[1];
          else if (MatchNoCase("BeamBlankShutter"))
            camP->beamShutter = itemInt[1];
          else if (MatchNoCase("SetAlternateShutterNormallyClosed"))
            camP->setAltShutterNC = itemInt[1] != 0;
          else if (MatchNoCase("CanDriveDMBeamShutter"))
            camP->DMbeamShutterOK = itemInt[1] != 0;
          else if (MatchNoCase("CanDriveDMDriftSettling"))
            camP->DMsettlingOK = itemInt[1] != 0;
          else if (MatchNoCase("CanUseDMOpenShutter"))
            camP->DMopenShutterOK = itemInt[1] != 0;
          else if (MatchNoCase("BuiltInSettling"))
            camP->builtInSettling = (float)itemDbl[1];
          else if (MatchNoCase("StartupDelay"))
            camP->startupDelay = (float)itemDbl[1];
          else if (MatchNoCase("ExtraUnblankTime"))
            camP->extraUnblankTime = 
              (float)itemDbl[1];
          else if (MatchNoCase("ExtraOpenShutterTime"))
            camP->extraOpenShutterTime = 
              (float)itemDbl[1];
          else if (MatchNoCase("ExtraBeamTime"))
            camP->extraBeamTime = (float)itemDbl[1];
          else if (MatchNoCase("MinimumDriftSettling"))
            camP->minimumDrift = (float)itemDbl[1];
          else if (MatchNoCase("MinimumBlankedExposure"))
            camP->minBlankedExposure = 
              (float)itemDbl[1];
          else if (MatchNoCase("ShutterDeadTime"))
            camP->deadTime = (float)itemDbl[1];
          else if (MatchNoCase("MinimumExposure"))
            camP->minExposure = (float)itemDbl[1];
          else 
            recognizedc1 = false;

          if (recognizedc1) {

          } else if (MatchNoCase("InsertionDelay"))
            camP->insertDelay = (float)itemDbl[1];
          else if (MatchNoCase("RetractionDelay"))
            camP->retractDelay = (float)itemDbl[1];
          else if (MatchNoCase("PostBlankerDelay"))
            camP->postBlankerDelay = (float)itemDbl[1];
          else if (MatchNoCase("Retractable"))
            camP->retractable = itemInt[1] != 0;
          else if (MatchNoCase("GIF"))
            camP->GIF = itemInt[1] != 0;
          else if (MatchNoCase("HasTVCamera"))
            camP->hasTVCamera = itemInt[1] != 0;
          else if (MatchNoCase("InsertTVToUnblank"))
            camP->useTVToUnblank = itemInt[1];
          else if (MatchNoCase("SideMounted"))
            camP->sideMount = itemInt[1] != 0;
          else if (MatchNoCase("CheckStableTemperature"))
            camP->checkTemperature = itemInt[1] != 0;
          else if (MatchNoCase("Order"))
            camP->order = itemInt[1];
          else if (MatchNoCase("FilmToCameraMagnification"))
            camP->magRatio = (float)itemDbl[1];
          else if (MatchNoCase("PixelSizeInMicrons"))
            camP->pixelMicrons = (float)itemDbl[1];
          else if (MatchNoCase("CountsPerElectron"))
            camP->countsPerElectron = (float)itemDbl[1];
          else if (MatchNoCase("ExtraRotation"))
            camP->extraRotation = (float)itemDbl[1];
          else if (MatchNoCase("ExtraGainReferences"))
            camP->numExtraGainRefs = itemInt[1];
          else if (MatchNoCase("NormalizeInSerialEM"))
            camP->processHere = itemInt[1];
          else if (MatchNoCase("RotationAndPixel")) {
            magInd = itemInt[1];
            if (magInd < 1 || magInd >= MAX_MAGS) {
              message.Format("Mag index out of range in \"RotationAndPixel %d\" line for"
                "camera %d\nin properties file %s", magInd, iset, strFileName);
              AfxMessageBox(message, MB_EXCLAME);
            } else {
              mMagTab[magInd].deltaRotation[iset] = (float)itemDbl[2];
              mMagTab[magInd].rotation[iset] = (float)itemDbl[3];
              mMagTab[magInd].pixelSize[iset] = (float)(0.001 * itemDbl[4]);
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
          } else if (MatchNoCase("OneViewMinExposures")) {
            StoreFloatsPerBinning(strItems, "OneView minimum exposure", -1, strFileName, 
              camera->GetOneViewMinExposure());
          } else if (MatchNoCase("OneViewDeltaExposures")) {
            StoreFloatsPerBinning(strItems, "OneView exposure increment", -1, strFileName, 
              camera->GetOneViewDeltaExposure());
          } else if (MatchNoCase("Name")) {
            StripItems(strLine, 1, camP->name);

          } else if (MatchNoCase("DMGainReferenceName")) {
            StripItems(strLine, 1, camP->DMRefName);

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
              "for camera %d: %s",  iset, strLine);
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
            ind = mWinApp->mShiftManager->GetNumRelRotations();
            if (strItems[3].IsEmpty()) {
             message.Format("Relative rotation entry needs three numbers\n"
              "for camera %d: %s",  iset, strLine);
              AfxMessageBox(message, MB_EXCLAME);
            } else if (ind >= MAX_REL_ROT) {
                AfxMessageBox("Too many special relative rotations for arrays",
                  MB_EXCLAME);
            } else {
              relRotations[ind].camera = iset;
              relRotations[ind].fromMag = itemInt[1];
              relRotations[ind].toMag = itemInt[2];
              relRotations[ind].rotation = itemDbl[3];
              mWinApp->mShiftManager->SetNumRelRotations(ind + 1);
            }


          } else if (MatchNoCase("RotationStretchXform")) {
            rotXform.camera = iset;          
            rotXform.magInd = itemInt[1];
            rotXform.mat.xpx = (float)itemDbl[2];
            rotXform.mat.xpy = (float)itemDbl[3];
            rotXform.mat.ypx = (float)itemDbl[4];
            rotXform.mat.ypy = (float)itemDbl[5];
            rotXformArray->Add(rotXform);
          } else if (MatchNoCase("HotPixelsAreImodCoords"))
            camP->hotPixImodCoord = itemInt[1];
          else if (MatchNoCase("DarkXRayAbsoluteCriterion"))
            camP->darkXRayAbsCrit = (float)itemDbl[1];
          else if (MatchNoCase("DarkXRaySDCriterion"))
            camP->darkXRayNumSDCrit = (float)itemDbl[1];
          else if (MatchNoCase("DarkXRayRequireBothCriteria"))
            camP->darkXRayBothCrit = itemInt[1];
          else if (MatchNoCase("MaximumXRayDiameter"))
            camP->maxXRayDiameter = itemInt[1];
          else if (MatchNoCase("ShowRemoveXRaysBox"))
            camP->showImageXRayBox = itemInt[1];
          else if (MatchNoCase("PixelMatchFactor"))
            camP->matchFactor = (float)itemDbl[1];
          else if (MatchNoCase("DoseRateTable")) {
            nMags = itemInt[1];
            scale = itemEmpty[2] ? 1.f : (float)itemDbl[2];
            if (nMags <= 0 || scale <= 0) {
              message.Format("Incorrect DoseRateTable entry for camera %d: %s",  
                iset, strLine);
              AfxMessageBox(message, MB_EXCLAME);
         
            } else {
              err = 0;
              for (ind = 0; ind < nMags; ind++) {
                if (ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                                     MAX_TOKENS) || itemEmpty[1]) {
                  err++;
                } else {
                  camP->doseTabCounts.push_back((float)itemDbl[0] / scale);
                  camP->doseTabRates.push_back((float)itemDbl[1]);
                }
              }
              if (err) {
                message.Format("%d errors occurred reading the Dose Rate table for "
                  "camera %d",  err, iset);
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

      if (recognizedc) {

      } else if (MatchNoCase("NumberOfCameras"))
        err = 0;
      else if (MatchNoCase("DigitalMicrographVersion"))
        err = 0;
#define PROP_TEST_SECT1
#include "PropertyTests.h"
#undef PROP_TEST_SECT1
      else if (MatchNoCase("GainNormalizeInSerialEM"))
        mWinApp->SetProcessHere(itemInt[1] != 0);
      else if (MatchNoCase("ReferenceMemoryLimitMB"))
        camera->SetRefMemoryLimit(1000000 * itemInt[1]);
      else if (MatchNoCase("DarkRefMaxMeanOrSD")) {
        camera->SetDarkMaxMeanCrit((float)itemDbl[1]);
        if (!itemEmpty[2])
          camera->SetDarkMaxSDcrit((float)itemDbl[2]);
      }
      else if (MatchNoCase("K2FilterName")) {
        ind = camera->GetNumK2Filters();
        if (ind < MAX_K2_FILTERS && ind > 0) {
          StripItems(strLine, 1, K2FilterNames[ind]);
          camera->SetNumK2Filters(ind + 1);
        } else
          AfxMessageBox("Too many K2 filter names for arrays", MB_EXCLAME);
      } else if (MatchNoCase("K2SuperResReference")) {
        StripItems(strLine, 1, message);
        camera->SetSuperResRef(message);
      } else if (MatchNoCase("K2CountingReference")) {
        StripItems(strLine, 1, message);
        camera->SetCountingRef(message);
      }
      else if (MatchNoCase("K2PackRawFramesDefault")) {
        if (camera->GetSaveRawPacked() < 0)
          camera->SetSaveRawPacked(itemInt[1] != 0 ? 1 : 0);
      }
      else if (MatchNoCase("DEProtectionCoverOpen"))
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

      } else if (MatchNoCase("DefaultGIFCamera"))
        camera->SetDefaultGIFCamera(itemInt[1]);
      else if (MatchNoCase("DefaultRegularCamera"))
        camera->SetDefaultRegularCamera(itemInt[1]);
      else if (MatchNoCase("DefaultCameraDivide16BitBy2")) {
        if (camera->GetDivideBy2() < 0)
          camera->SetDivideBy2(itemInt[1]);
      } else if (MatchNoCase("StartCameraInDebugMode"))
        camera->SetDebugMode(itemInt[1] != 0);
      else if (MatchNoCase("ExitOnBadScopeStartup"))
        mWinApp->SetExitOnScopeError(itemInt[1] != 0);

      else if (MatchNoCase("DigiScanFlipAndRotation")) {
        camera->SetDSshouldFlip(itemInt[1]);
        camera->SetDSglobalRotOffset((float)itemDbl[2]);
      } else if (MatchNoCase("GatanServerIP"))
        CBaseSocket::SetServerIP(GATAN_SOCK_ID, strItems[1]);
      else if (MatchNoCase("GatanServerPort"))
        CBaseSocket::SetServerPort(GATAN_SOCK_ID, itemInt[1]);
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

      } else if (MatchNoCase("ControlSetName")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_CONSETS || strItems[2].IsEmpty())
           AfxMessageBox("Index out of range or missing control set name in properties"
             " file " + strFileName + " : " + strLine , MB_EXCLAME);
        else
          mModeNames[index] = strItems[2];

      } else if (MatchNoCase("SearchAreaName")) {
        if (strItems[1].IsEmpty())
           AfxMessageBox("Missing area name in properties file " + strFileName + 
           " : " + strLine , MB_EXCLAME);
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
        mWinApp->mDocWnd->SetSTEMfileMode(itemInt[1]);
      } else if (MatchNoCase("FileOptionsExtraFlags"))
        defFileOpt->typext = (short)itemInt[1];
      else if (MatchNoCase("FileOptionsMaxSections"))
        defFileOpt->maxSec = itemInt[1];
      else if (MatchNoCase("FileOptionsPixelsTruncatedLo"))
        defFileOpt->nTruncLo = itemInt[1];
      else if (MatchNoCase("FileOptionsPixelsTruncatedHi"))
        defFileOpt->nTruncHi = itemInt[1];
      else if (MatchNoCase("FileOptionsUnsignedOption"))
        defFileOpt->unsignOpt = itemInt[1];
      else if (MatchNoCase("FileOptionsSignToUnsignOption"))
        defFileOpt->signToUnsignOpt = itemInt[1];
      else if (MatchNoCase("FileOptionsFileType"))
        defFileOpt->fileType = itemInt[1] != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
      else if (MatchNoCase("MontageAutodocOptions")) {
        defFileOpt->separateForMont = itemInt[1] != 0;
        defFileOpt->montUseMdoc = itemInt[1] % 2 != 0;
        defFileOpt->montFileType = (itemInt[1] / 2) % 2 != 0 ? STORE_TYPE_ADOC : STORE_TYPE_MRC;
      }
      else if (MatchNoCase("TotalMemoryLimitMB"))
        mWinApp->SetMemoryLimit((float)(1000000. * itemInt[1]));
      
      else if (MatchNoCase("SingleTecnaiObject")) {
      } else if (MatchNoCase("CookerScreenTiltDelay")) {
      } else if (MatchNoCase("NoScope"))
        scope->SetNoScope(itemInt[1]);
      else if (MatchNoCase("BackgroundSocketToFEI"))
        SetNumFEIChannels(itemInt[1] ? 4 : 3);
      else if (MatchNoCase("UseTEMScripting")) {
        scope->SetUseTEMScripting(itemInt[1]);
        //JEOLscope = false;
      } else if (MatchNoCase("ScopeIsFEI")) {
        //JEOLscope = false;
      } else if (MatchNoCase("ScopeIsJEOL")) {
        //JEOLscope = true;
      } else 
        recognized = false;
      
      if (recognized || recognizedc) {

      }
#define PROP_TEST_SECT2
#include "PropertyTests.h"
#undef PROP_TEST_SECT2
      else if (MatchNoCase("IlluminatedAreaLimits"))
        scope->SetIllumAreaLimits((float)itemDbl[1], (float)itemDbl[2]);
      else if (MatchNoCase("C2ApertureSizes")) {
        ind = mWinApp->mBeamAssessor->GetNumC2Apertures();
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty() || ind >= MAX_SPOT_SIZE)
            break;
          C2apertures[ind++] = itemInt[index];
        }
        mWinApp->mBeamAssessor->SetNumC2Apertures(ind);

      }
      else if (MatchNoCase("LowestSTEMnonLMmag")) {
        scope->SetLowestSTEMnonLMmag(0, itemInt[1]);
        if (!itemEmpty[2])
          scope->SetLowestSTEMnonLMmag(1, itemInt[2]);
      } else if (MatchNoCase("OtherShiftBoundaries")) {
        for (index = 1; index < MAX_TOKENS; index++) {
          if (strItems[index].IsEmpty())
            break;
          scope->AddShiftBoundary(atoi((LPCTSTR)strItems[index]));
        }

      }
      else if (MatchNoCase("STEMdefocusToDeltaZ"))
        mWinApp->mFocusManager->SetSTEMdefocusToDelZ((float)itemDbl[1],(float)itemDbl[1]);
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
          pzScale.minPos = (float)itemDbl[4];
          pzScale.maxPos = (float)itemDbl[5];
          pzScale.unitsPerMicron = (float)itemDbl[6];
          piezoScalings->Add(pzScale);
        }
      } else if (MatchNoCase("UsePiezoForLDAxis")) {
        mWinApp->mLowDoseDlg.SetAxisPiezoPlugNum(itemInt[1]);
        mWinApp->mLowDoseDlg.SetAxisPiezoNum(itemInt[2]);
      } else if (MatchNoCase("PiezoFullDelay")) {
        mWinApp->mLowDoseDlg.SetPiezoFullDelay((float)itemDbl[1]);
        if (!itemEmpty[2])
          mWinApp->mLowDoseDlg.SetFocusPiezoDelayFac((float)itemDbl[2]);
        if (!itemEmpty[3])
          mWinApp->mLowDoseDlg.SetTVPiezoDelayFac((float)itemDbl[3]);
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
        mWinApp->mAutoTuning->SetAstigBeamTilt((float)itemDbl[1]);
        if (!itemEmpty[2])
          mWinApp->mAutoTuning->SetAstigToApply((float)itemDbl[2]);
        if (!itemEmpty[3])
          mWinApp->mAutoTuning->SetAstigIterationThresh((float)itemDbl[3]);
        if (!itemEmpty[4])
          mWinApp->mAutoTuning->SetAstigCenterFocus((float)itemDbl[4]);
        if (!itemEmpty[5])
          mWinApp->mAutoTuning->SetAstigFocusRange((float)itemDbl[5]);
      } else if (MatchNoCase("ComaParams")) {
        mWinApp->mAutoTuning->SetMaxComaBeamTilt((float)itemDbl[1]);
        if (!itemEmpty[2])
          mWinApp->mAutoTuning->SetCalComaFocus((float)itemDbl[2]);
      }
      else if (MatchNoCase("MontageInitialPieces")) {
        mWinApp->mDocWnd->SetDefaultMontXpieces(itemInt[1]);
        if (itemInt[2] > 0)
          mWinApp->mDocWnd->SetDefaultMontYpieces(itemInt[2]);
      }
      else if (MatchNoCase("StageMontageMaxError")) {
        mWinApp->mMontageController->SetMaxStageError((float)itemDbl[1]);
        mWinApp->mMontageController->SetStopOnStageError(itemInt[2] != 0);
      }
      else if (MatchNoCase("MontageFilterR1R2S1S2") || 
        MatchNoCase("MontFilterSet2R1R2S1S2")) {
          index = MatchNoCase("MontageFilterR1R2S1S2") ? 0 : 1;
          if (itemEmpty[4])
            AfxMessageBox("Four numbers are required in properties file " + strFileName 
            + " for line: " + strLine , MB_EXCLAME);
          else
            mWinApp->mMontageController->SetXcorrFilter(index, (float)itemDbl[1], 
            (float)itemDbl[2], (float)itemDbl[3], (float)itemDbl[4]);
      } else
        recognized2 = false;

      if (recognized || recognized2 || recognizedc) {

      }
#define PROP_TEST_SECT3
#include "PropertyTests.h"
#undef PROP_TEST_SECT3
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

      }

      // Hitachi
      else if (MatchNoCase("HitachiIPaddress"))
        hitachi->IPaddress = strItems[1];
      else if (MatchNoCase("HitachiPort"))
        hitachi->port = strItems[1];
      else if (MatchNoCase("HitachiBeamBlankAxis"))
        hitachi->beamBlankAxis = itemInt[1];
      else if (MatchNoCase("HitachiBeamBlankDelta"))
        hitachi->beamBlankDelta = (float)itemDbl[1];
      else if (MatchNoCase("HitachiPAforHRmodeIS"))
        hitachi->usePAforHRmodeIS = itemInt[1] != 0;
      else if (MatchNoCase("HitachiTiltSpeed"))
        hitachi->stageTiltSpeed = itemInt[1];
      else if (MatchNoCase("HitachiStageSpeed"))
        hitachi->stageXYSpeed = itemInt[1];
      else if (MatchNoCase("HitachiImageShiftToMicrons"))
        hitachi->imageShiftToUm = (float)itemDbl[1];
      else if (MatchNoCase("HitachiBeamShiftToMicrons"))
        hitachi->beamShiftToUm = (float)itemDbl[1];
      else if (MatchNoCase("HitachiObjectiveToMicrons")) {
        hitachi->objectiveToUm[0] = (float)itemDbl[1];
        hitachi->objectiveToUm[2] = (float)itemDbl[1];
        if (!itemEmpty[2])
          hitachi->objectiveToUm[2] = (float)itemDbl[2];
      } else if (MatchNoCase("HitachiI1ToMicrons"))
        hitachi->objectiveToUm[1] = (float)itemDbl[1];
      else if (MatchNoCase("HitachiScreenArea"))
        hitachi->screenAreaSqCm = (float)itemDbl[1];


      else if (MatchNoCase("WalkUpMaxInterval") || MatchNoCase("WalkUpMinInterval"))
        err = 0;
      else
        recognized3 = false;

      if (recognized || recognized2 || recognized3 || recognizedc) {

      }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
      else if (MatchNoCase("TSXFitInterval"))
        mTSParam->fitIntervalX = (float)itemDbl[1];
      else if (MatchNoCase("TSYFitInterval"))
        mTSParam->fitIntervalY = (float)itemDbl[1];
      else if (MatchNoCase("TSZFitInterval"))
        mTSParam->fitIntervalZ = (float)itemDbl[1];
      else if (MatchNoCase("TSXMinForQuadratic"))
        mTSParam->minFitXQuadratic = itemInt[1];
      else if (MatchNoCase("TSYMinForQuadratic"))
        mTSParam->minFitYQuadratic = itemInt[1];
      else if (MatchNoCase("TSBidirAnchorMinField")) {
        tsController->SetMinFieldBidirAnchor((float)itemDbl[1]);
        if (!itemEmpty[2])
          tsController->SetAlarmBidirFieldSize((float)itemDbl[2]);
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
          camera->SetMinZLPAlignInterval((float)itemDbl[2]);
      } else if (MatchNoCase("FilterPixelMatchingFactor"))
        mFilterParam->binFactor = (float)itemDbl[1];
      else if (MatchNoCase("EnergyShiftCalLowestMag"))
        err = 0;
      else if (MatchNoCase("PrintStartupInfo"))
        mWinApp->SetStartupInfo(itemInt[1] != 0);
      else if (MatchNoCase("NoExceptionHandler"))
        mWinApp->SetNoExceptionHandler(itemInt[1]);
      else if (MatchNoCase("CatalaseAxisLengths")) {
        mWinApp->mProcessImage->SetShortCatalaseNM((float)itemDbl[1]);
        mWinApp->mProcessImage->SetLongCatalaseNM((float)itemDbl[2]);

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
        mWinApp->mDocWnd->SetTitle(message);

      } else if (MatchNoCase("LogBookPathName")) {
        StripItems(strLine, 1, message);
        mWinApp->mDocWnd->SetLogBook(message);

      } else if (MatchNoCase("StartupMessage")) {
        StripItems(strLine, 1, message);
        message.Replace("\\n", "\r\n");
        mWinApp->SetStartupMessage(message);

      } else if (MatchNoCase("PluginPath")) {
        StripItems(strLine, 1, message);
        mWinApp->mDocWnd->SetPluginPath(message);

      } else if (MatchNoCase("FFTCircleRadii")) {
        for (ind = 1; ind < MAX_FFT_CIRCLES; ind++) {
          if (itemEmpty[ind])
            break;
          radii[ind - 1] = (float)itemDbl[ind];
        }
        mWinApp->mProcessImage->SetNumCircles(ind - 1);

     } else if (MatchNoCase("FFTZeroParams")) {
       mWinApp->mProcessImage->SetSphericalAber((float)B3DMAX(0.01, itemDbl[1]));
       if (!itemEmpty[2])
         mWinApp->mProcessImage->SetNumFFTZeros(itemInt[2]);
       if (!itemEmpty[3])
         mWinApp->mProcessImage->SetAmpRatio((float)itemDbl[3]);

     } else if (MatchNoCase("ImportStageToImage")) {
       index = navParams->numImportXforms;
       if (index >= MAX_IMPORT_XFORMS) {
         AfxMessageBox("Too many import stage-to-image transformations for arrays",
            MB_EXCLAME);
       } else {
         navParams->importXform[index].xpx = (float)itemDbl[1];
         navParams->importXform[index].xpy = (float)itemDbl[2];
         navParams->importXform[index].ypx = (float)itemDbl[3];
         navParams->importXform[index].ypy = (float)itemDbl[4];
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
       navParams->gridInPolyBoxFrac = (float)itemDbl[1];
     } else if (MatchNoCase("NavigatorStageBacklash")) {
       navParams->stageBacklash = (float)itemDbl[1];
     } else if (MatchNoCase("SamePositionTolerance")) {
        scope->SetBacklashTolerance((float)itemDbl[1]);
     } else if (MatchNoCase("NavigatorMaxMontageIS")) {
       navParams->maxMontageIS = (float)itemDbl[1];
     } else if (MatchNoCase("NavigatorMaxLMMontageIS")) {
       navParams->maxLMMontageIS = (float)itemDbl[1];
     } else if (MatchNoCase("FitMontageWithFullFrames")) {
       navParams->fitMontWithFullFrames = (float)itemDbl[1];
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
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
            mMagTab[index].tecnaiRotation = (float)itemDbl[2];
            mMagTab[index].screenMag = (strItems[3].IsEmpty() || strItems[3] == "0") ? 
              mMagTab[index].mag : itemInt[3];
            mMagTab[index].EFTEMmag = (strItems[4].IsEmpty() || strItems[4] == "0") ?
              mMagTab[index].mag : itemInt[4];
            mMagTab[index].EFTEMtecnaiRotation = strItems[5].IsEmpty() ?
              mMagTab[index].tecnaiRotation : (float)itemDbl[5];
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
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
        }
        if (err > 0) {
          AfxMessageBox("Error reading camera length table, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }

      } else if (MatchNoCase("FocusTickTable")) {
        nMags = itemInt[1];
        for (int line = 0; line < nMags; line++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
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
        float *ISmoved = mWinApp->mShiftManager->GetISmoved();
        float *ISdelay = mWinApp->mShiftManager->GetISdelayNeeded();
        for (int line = 0; line < nMags; line++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          if (strItems[1].IsEmpty()) {
            err = 1;
            break;
          }
          ISmoved[line] = (float)itemDbl[0];
          ISdelay[line] = (float)itemDbl[1];
        }
        if (err > 0) {
          AfxMessageBox("Error reading image shift delays, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        } else
          mWinApp->mShiftManager->SetNumISdelays(nMags);
      
      } else if (!strItems[0].IsEmpty() && !recognized && !recognized2 && !recognized3)
        AfxMessageBox("Unrecognized entry in properties file " + strFileName 
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

  if (!mDupMessage.IsEmpty()) {
    mDupMessage = "These items are entered more than once in property file " +
      strFileName + "\r\n   "
      "(only the last entry has any effect):\r\n" + mDupMessage;
    AfxMessageBox(mDupMessage, MB_EXCLAME);
  }

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
  CArray<STEMFocusZTable, STEMFocusZTable> *focusZtables = 
    mWinApp->mFocusManager->GetSFfocusZtables();
  CArray <AstigCalib, AstigCalib> *astigCals = mWinApp->mAutoTuning->GetAstigCals();
  AstigCalib astig;
  CArray <ComaCalib, ComaCalib> *comaCals = mWinApp->mAutoTuning->GetComaCals();
  ComaCalib coma;
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals =
    mWinApp->mShiftManager->GetFocusMagCals();
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
    else if (strItems[0] != "SerialEMCalibrations") {
      retval = 1;
      AfxMessageBox("File not recognized as a calibrations file", MB_EXCLAME);
    }

    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
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
          sm->xpx = (float)itemDbl[2];
          sm->xpy = (float)itemDbl[3];
          sm->ypx = (float)itemDbl[4];
          sm->ypy = (float)itemDbl[5];
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
          sm->xpx = (float)itemDbl[3];
          sm->xpy = (float)itemDbl[4];
          sm->ypx = (float)itemDbl[5];
          sm->ypy = (float)itemDbl[6];
          if (!strItems[8].IsEmpty())
            mMagTab[index].stageCalFocus[camera] = (float)itemDbl[7];
        }

      } else if (NAME_IS("HighFocusMagCal")) {
        focCal.spot = itemInt[1];
        focCal.probeMode = itemInt[2];
        focCal.defocus = (float)itemDbl[3];
        focCal.intensity = itemDbl[4];
        focCal.scale = (float)itemDbl[5];
        focCal.rotation = (float)itemDbl[6];
        focCal.crossover = itemEmpty[7] ? 0. : itemDbl[7];
        focCal.measuredAperture = itemEmpty[8] ? 0 : itemInt[8];
        focusMagCals->Add(focCal);

      } else if (NAME_IS("IntensityToC2Factor")) {
        mWinApp->mScope->SetC2IntensityFactor(1, (float)itemDbl[1]);
        if (!itemEmpty[2])
          mWinApp->mScope->SetC2IntensityFactor(0, (float)itemDbl[2]);

      } else if (NAME_IS("C2SpotOffset")) {
        index = itemInt[1];
        if (index < 0 || index >= MAX_SPOT_SIZE || itemEmpty[2]) {
          err = 1;
        } else {
          mWinApp->mScope->SetC2SpotOffset(index, 1, (float)itemDbl[2]);
          if (!itemEmpty[3])
            mWinApp->mScope->SetC2SpotOffset(index, 0, (float)itemDbl[3]);
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
        if (!strItems[8].IsEmpty())
          beamInd = itemInt[7];
        if (!strItems[9].IsEmpty())
          freeInd = itemInt[8];
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
          focTable.numPoints = nCal;
          focTable.slopeX = (float)itemDbl[3];
          focTable.slopeY = (float)itemDbl[4];
          focTable.beamTilt = itemDbl[5];
          focTable.calibrated = 0;
        }

        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          focTable.defocus[i] = (float)itemDbl[0];
          focTable.shiftX[i] = (float)itemDbl[1];
          focTable.shiftY[i] = (float)itemDbl[2];
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
        mWinApp->mFocusManager->SetSFnormalizedSlope(0, (float)itemDbl[1]);
        mWinApp->mFocusManager->SetSFnormalizedSlope(1, (float)itemDbl[2]);

      } else if (NAME_IS("STEMfocusVersusZ")) {
        sfZtable.numPoints = itemInt[1];
        sfZtable.spotSize = itemInt[2];
        sfZtable.probeMode = itemInt[3];
        for (i = 0; i < sfZtable.numPoints; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          sfZtable.stageZ[i] = (float)itemDbl[0];
          sfZtable.defocus[i] = (float)itemDbl[1];
          sfZtable.absFocus[i] = (float)itemDbl[2];
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
        astig.defocus = (float)itemDbl[4]; 
        astig.beamTilt = (float)itemDbl[5];
        astig.focusMat.xpx = (float)itemDbl[6];
        astig.focusMat.xpy = (float)itemDbl[7];
        astig.focusMat.ypx = (float)itemDbl[8];
        astig.focusMat.ypy = (float)itemDbl[9];
        astig.astigXmat.xpx = (float)itemDbl[10];
        astig.astigXmat.xpy = (float)itemDbl[11];
        astig.astigXmat.ypx = (float)itemDbl[12];
        astig.astigXmat.ypy = (float)itemDbl[13];
        astig.astigYmat.xpx = (float)itemDbl[14];
        astig.astigYmat.xpy = (float)itemDbl[15];
        astig.astigYmat.ypx = (float)itemDbl[16];
        astig.astigYmat.ypy = (float)itemDbl[17];
        astigCals->Add(astig);
 
      } else if (NAME_IS("ComaCalib")) {
        coma.magInd = itemInt[1];
        coma.probeMode = itemInt[2];
        coma.alpha = itemInt[3];
        coma.defocus = (float)itemDbl[4]; 
        coma.beamTilt = (float)itemDbl[5];
        coma.comaXmat.xpx = (float)itemDbl[6];
        coma.comaXmat.xpy = (float)itemDbl[7];
        coma.comaXmat.ypx = (float)itemDbl[8];
        coma.comaXmat.ypy = (float)itemDbl[9];
        coma.comaYmat.xpx = (float)itemDbl[10];
        coma.comaYmat.xpy = (float)itemDbl[11];
        coma.comaYmat.ypx = (float)itemDbl[12];
        coma.comaYmat.ypy = (float)itemDbl[13];
        comaCals->Add(coma);

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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          beamTab->currents[i] = (float)itemDbl[0];
          beamTab->intensities[i] = itemDbl[1];
        }
        if (err > 0) {
          if (err == 1)
            AfxMessageBox("Error reading beam calibrations, line\n" +
            strLine, MB_EXCLAME);
          retval = err;
          break;
        }
        mWinApp->mBeamAssessor->SortAndTakeLogs(beamTab, false);

      } else if (NAME_IS("BeamShiftCalibration")) {

        // Old form has no mag, newer form has a mag index in first spot and mag at end
        // Default probe mode to 1 if it is not present yet
        i = 1;
        index = 0;
        nCal = -999;
        beamInd = 1;
        if (!strItems[5].IsEmpty())
          index = atoi((LPCTSTR)strItems[i++]);
        IStoBS.xpx = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.xpy = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.ypx = (float)atof((LPCTSTR)strItems[i++]);
        IStoBS.ypy = (float)itemDbl[i++];
        if (!strItems[7].IsEmpty())
          nCal = atoi((LPCTSTR)strItems[6]);
        if (!strItems[8].IsEmpty())
          beamInd = atoi((LPCTSTR)strItems[7]);
        mWinApp->mShiftManager->SetBeamShiftCal(IStoBS, index, nCal, beamInd);
        
       } else if (NAME_IS("StageStretchXform")) {
         IStoBS.xpx = (float)itemDbl[1];
         IStoBS.xpy = (float)itemDbl[2];
         IStoBS.ypx = (float)itemDbl[3];
         IStoBS.ypy = (float)itemDbl[4];
         mWinApp->mShiftManager->SetStageStretchXform(IStoBS);
       } else if (NAME_IS("FilterMagShifts")) {
        nCal = itemInt[1];
        for (i = 0; i < nCal; i++) {
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          if (index < 1 || index >= MAX_MAGS) {
            err = 1;
            break;
          } else {
            mFilterParam->magShifts[index] = (float)itemDbl[1];
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
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
              mMagTab[index].neutralISX[focInd] = (float)itemDbl[1];
              mMagTab[index].neutralISY[focInd] = (float)itemDbl[2];
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          if (i < MAX_ALPHAS && index) {
            alphaBeamShifts[2 * i] = (float)itemDbl[0];
            alphaBeamShifts[2 * i + 1] = (float)itemDbl[1];
          } else if (i < MAX_ALPHAS) {
            alphaBeamTilts[2 * i] = (float)itemDbl[0];
            alphaBeamTilts[2 * i + 1] = (float)itemDbl[1];
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
                MAX_TOKENS);
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          camera = itemInt[1];
          if (index < 1 || index >= MAX_MAGS || camera < 0 || camera > 1) {
            err = 1;
            break;
          } else {
            mMagTab[index].calOffsetISX[camera] = (float)itemDbl[2];
            mMagTab[index].calOffsetISY[camera] = (float)itemDbl[3];
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
        for (i = 0; i <= mWinApp->mScope->GetNumSpotSizes(); i++) {
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
          err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, MAX_TOKENS);
          if (err)
            break;
          index = itemInt[0];
          if (index < 1 || index > MAX_SPOT_SIZE) {
            err = 1;
            break;
          }
          tmpTable[0].ratio[index] = (float)itemDbl[1];
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
            tmpTable[1].ratio[index] = (float)itemDbl[4];
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
  int indMicro[2] = {-1, -1}, indNano[2] = {-1, -1};
  CString string;
  int err = 0;
  double cross[2], ratio[2], intensity[2];
  int curAperture = mWinApp->mBeamAssessor->GetCurrentAperture();
  int *crossCalAper = mWinApp->mBeamAssessor->GetCrossCalAperture();
  int *spotCalAper = mWinApp->mBeamAssessor->GetSpotCalAperture();
  ScaleMat stageStr = mWinApp->mShiftManager->GetStageStretchXform();
  ScaleMat *IStoBS = mWinApp->mShiftManager->GetBeamShiftMatrices();
  BeamTable *beamTables = mWinApp->mBeamAssessor->GetBeamTables();
  BeamTable *btp;
  SpotTable *spotTables = mWinApp->mBeamAssessor->GetSpotTables();
  ShortVec *beamMags = mWinApp->mShiftManager->GetBeamCalMagInd();
  ShortVec *beamAlphas = mWinApp->mShiftManager->GetBeamCalAlpha();
  ShortVec *beamProbes = mWinApp->mShiftManager->GetBeamCalProbe();
  float *alphaBeamShifts = mWinApp->mScope->GetAlphaBeamShifts();
  float *alphaBeamTilts = mWinApp->mScope->GetAlphaBeamTilts();
  double *spotBeamShifts = mWinApp->mScope->GetSpotBeamShifts();
  double *LMfocTab = mWinApp->mScope->GetLMFocusTable();
  CArray<STEMFocusZTable, STEMFocusZTable> *focusZtables = 
    mWinApp->mFocusManager->GetSFfocusZtables();
  CArray <AstigCalib, AstigCalib> *astigCals = mWinApp->mAutoTuning->GetAstigCals();
  AstigCalib astig;
  CArray <ComaCalib, ComaCalib> *comaCals = mWinApp->mAutoTuning->GetComaCals();
  ComaCalib coma;
  CArray<HighFocusMagCal, HighFocusMagCal> *focusMagCals = 
    mWinApp->mShiftManager->GetFocusMagCals();
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
    for (i = 0; i < focusMagCals->GetSize(); i++) {
      focCal = focusMagCals->GetAt(i);
      string.Format("HighFocusMagCal %d %d %f %f %f %f %f %d\n", focCal.spot, 
        focCal.probeMode, focCal.defocus, mWinApp->mScope->IntensityAfterApertureChange(
        focCal.intensity, curAperture, focCal.measuredAperture), focCal.scale, 
        focCal.rotation, mWinApp->mScope->IntensityAfterApertureChange(
        focCal.crossover, curAperture, focCal.measuredAperture), focCal.measuredAperture);
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
          curAperture, crossCalAper[probe]);
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
      string.Format("FocusCalibration %d %d %f %f %.2f %d %d %d   %d\n",
        focTable.magInd, focTable.camera, focTable.slopeX, focTable.slopeY,
        focTable.beamTilt, focTable.numPoints, focTable.direction, focTable.probeMode,
        MagForCamera(focTable.camera, focTable.magInd));
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

    // Write astigmatism calibrations
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

    // Write beam calibrations
    for (i = 0; i < mWinApp->mBeamAssessor->GetNumTables(); i++) {
      btp = &beamTables[i];
      if (btp->numIntensities) {
        cross[0] = btp->crossover;
        if (cross[0])
          cross[0] = mWinApp->mScope->IntensityAfterApertureChange(cross[0], curAperture, 
            btp->measuredAperture);
        string.Format("BeamIntensityTable %d %d %d %d %f %d %d   %d\n", 
          btp->numIntensities, btp->magIndex,
          btp->spotSize, btp->dontExtrapFlags, cross[0],
          btp->measuredAperture, btp->probeMode, mMagTab[btp->magIndex].mag);
        mFile->WriteString(string);
        for (k = 0; k < btp->numIntensities; k++) {
          string.Format("%f %f\n", btp->currents[k], 
            mWinApp->mScope->IntensityAfterApertureChange(btp->intensities[k], 
            curAperture, btp->measuredAperture));
          mFile->WriteString(string);
        }
      }
    }

    for (i = 0; i < mWinApp->mShiftManager->GetNumBeamShiftCals(); i++) {
      nCal = beamMags->at(i) < 0 ? mMagTab[-beamMags->at(i)].EFTEMmag : 
        mMagTab[beamMags->at(i)].mag;
      string.Format("BeamShiftCalibration %d %f %f %f %f %d %d  %d\n",
        beamMags->at(i), IStoBS[i].xpx, IStoBS[i].xpy, IStoBS[i].ypx, IStoBS[i].ypy, 
        beamAlphas->at(i), beamProbes->at(i), nCal);
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
      else if (nCal2 < 2)
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
              curAperture, spotCalAper[ind]);
            intensity[j] = mWinApp->mScope->IntensityAfterApertureChange(
              spotTables[ind].intensity[i], curAperture, spotCalAper[ind]);
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
    AfxMessageBox("Calibrations saved to file " + strFileName, MB_EXCLAME);
}


// Read the short-term calibration file (for dose calibration)
int CParameterIO::ReadShortTermCal(CString strFileName)
{
  int retval = 0;
  int err, index, cam, inTime;
  CString strLine;
  CString strItems[MAX_TOKENS];
  BOOL itemEmpty[MAX_TOKENS];
  int itemInt[MAX_TOKENS];
  double itemDbl[MAX_TOKENS];
  int *times = mWinApp->mScope->GetLastLongOpTimes();
  int timeStamp = mWinApp->MinuteTimeStamp();
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  FilterParams *filtP = mWinApp->GetFilterParams();
  NavParams *navP = mWinApp->GetNavParams();
  ShortVec *pixSizCamera, *pixSizMagInd, *addedRotation;
  std::vector<float> *pixelSizes, *gridRotations;
  mWinApp->mProcessImage->GetPixelArrays(&pixSizCamera, &pixSizMagInd, &addedRotation,
    &pixelSizes, &gridRotations);

  try {
    // Open the file for reading, verify that it is a calibration file
    mFile = new CStdioFile(strFileName,
      CFile::modeRead |CFile::shareDenyWrite);
    
    err = ReadAndParse(strLine, strItems, MAX_TOKENS);
    if (err)
      retval = 1;
    else if (strItems[0] != "SerialEMShortTermCal") {
      retval = 1;
      AfxMessageBox("File not recognized as a short-term calibrations file", MB_EXCLAME);
    }
    while (retval == 0 && 
      (err = ReadSuperParse(strLine, strItems, itemEmpty, itemInt, itemDbl, 
                            MAX_TOKENS)) == 0) {

      if (NAME_IS("DoseCalibration")) {
        index = itemInt[1] * 2 + itemInt[2];
        index = 2 * index + B3DCHOICE(itemEmpty[6], 1, itemInt[6]);
        if (index < 0 || index >= 4 * MAX_SPOT_SIZE || strItems[5].IsEmpty()) {
          AfxMessageBox("Incorrect entry in short term calibration file "
            + strFileName + " :\n" + strLine, MB_EXCLAME);
        } else {
          doseTables[index].timeStamp = itemInt[5];
          doseTables[index].currentAperture = itemEmpty[7] ? 0 : itemInt[7];
          if (timeStamp - doseTables[index].timeStamp < 60 * DOSE_LIFETIME_HOURS) {
            doseTables[index].intensity = itemDbl[3];
            doseTables[index].dose = itemDbl[4];
          }
        }

      } else if (NAME_IS("FilterAlignment")) {
        filtP->cumulNonRunTime = itemInt[5] + timeStamp - 
          itemInt[6];

        // To use a filter alignment, the cumulative non run time must be less than
        // the limit and it should either be the same user or an omega filter
        if (filtP->cumulNonRunTime < FILTER_LIFETIME_MINUTES && 
          (mWinApp->mScope->GetHasOmegaFilter() || strItems[7] == getenv("USERNAME"))) {
          filtP->refineZLPOffset = (float)itemDbl[1];
          filtP->alignedSlitWidth = (float)itemDbl[2];
          filtP->alignedMagInd = itemInt[3];
          filtP->alignZLPTimeStamp = itemDbl[4];
          filtP->usedOldAlign = true;
        }
        mWinApp->mDocWnd->SetShortTermNotSaved();

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
            mMagTab[index].matIS[cam].xpx = (float)itemDbl[4];
            mMagTab[index].matIS[cam].xpy = (float)itemDbl[5];
            mMagTab[index].matIS[cam].ypx = (float)itemDbl[6];
            mMagTab[index].matIS[cam].ypy = (float)itemDbl[7];
          } else {
            mMagTab[index].stageCalibrated[cam] = inTime;
            mMagTab[index].matStage[cam].xpx = (float)itemDbl[4];
            mMagTab[index].matStage[cam].xpy = (float)itemDbl[5];
            mMagTab[index].matStage[cam].ypx = (float)itemDbl[6];
            mMagTab[index].matStage[cam].ypy = (float)itemDbl[7];
            mMagTab[index].stageCalFocus[cam] = (float)itemDbl[8];
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
          pixelSizes->push_back((float)itemDbl[5]);
          gridRotations->push_back((float)itemDbl[6]);
        }

        // Save the nav autosave filename if it belongs to this user
      } else if (NAME_IS("NavAutosave")) {
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

      } else if (!strItems[0].IsEmpty())
        AfxMessageBox("Unrecognized entry in short term calibration file " + strFileName
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

  return retval;
}


void CParameterIO::WriteShortTermCal(CString strFileName)
{
  int i, j, k, index;
  int *times = mWinApp->mScope->GetLastLongOpTimes();
  CString oneState, oneTime;
  DoseTable *doseTables = mWinApp->mBeamAssessor->GetDoseTables();
  FilterParams *filtP = mWinApp->GetFilterParams();
  NavParams *navP = mWinApp->GetNavParams();
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

int CParameterIO::ReadAndParse(CString &strLine, CString *strItems, int maxItems)
{
  return ReadAndParse(mFile, strLine, strItems, maxItems);
}

int CParameterIO::ReadAndParse(CStdioFile *file, CString &strLine, CString *strItems, 
  int maxItems)
{
  try {
    if (!file->ReadString(strLine))
      return -1;          // return for EOF
  }
  catch (CFileException *err) {
    err->Delete();
    return 1;                       // return for error
  }

  return ParseString(strLine, strItems, maxItems);
}

// Read an parse a line and fill in empty, int, and double arrays
int CParameterIO::ReadSuperParse(CString &strLine, CString *strItems, BOOL *itemEmpty,
                                 int *itemInt, double *itemDbl, int maxItems)
{
  int err = ReadAndParse(strLine, strItems, maxItems);
  for (int i = 0; i < maxItems; i++) {
    itemEmpty[i] = strItems[i].IsEmpty();
    if (!itemEmpty[i]) {
      itemDbl[i] = atof(strItems[i]);
      itemInt[i] = atoi(strItems[i]);
    }
  }
  return err;
}

// Parse a string into separate items in an array of CStrings
int CParameterIO::ParseString(CString strLine, CString *strItems, int maxItems)
{
  int i;

  // Make all strings empty
  for (i = 0; i < maxItems; i++)
    strItems[i] = "";
  CString strCopy = strLine;

  // Get one token after another until the copy is empty
  for (i = 0; i < maxItems; i++) {
    FindToken(strCopy, strItems[i]);
    if (strCopy.IsEmpty())
      return 0;
  }

  return 2; // Error if there are too many items
}

void CParameterIO::FindToken(CString &strCopy, CString &strItem)
{
  int index;
  strItem = "";

  // Trim white space at left; done if no more string
  strCopy.TrimLeft();
  if (strCopy.IsEmpty())
    return;

  // If we have a comment, done
  if (strCopy.Find('#') == 0) {
    strCopy = "";
    return;
  }

  // Find next space or tab
  index = strCopy.FindOneOf(" \t");

  // If there are no more, then the item is the rest of string - but trim off cr-lf
  if (index < 0) {
    strItem = strCopy;
    strItem.TrimRight(" \t\r\n");
    strCopy = "";
    return;
  }

  // There must be one down the line, split the string
  strItem = strCopy.Left(index);
  strCopy = strCopy.Right(strCopy.GetLength() - index);
}

// Take some items off the front of a line and return the rest as one string, trimmed
void CParameterIO::StripItems(CString strLine, int numItems, CString & strCopy)
{
  CString strItem;
  strCopy = strLine;
  for (int i = 0; i < numItems; i++) {
    FindToken(strCopy, strItem);
    if (strCopy.IsEmpty())
      return;
  }
  strCopy.TrimLeft();
  strCopy.TrimRight(" \t\r\n");
}


void CParameterIO::WritePlacement(char *string, int open, WINDOWPLACEMENT *place)
{
  CString oneState;
  if (place->rcNormalPosition.right <= 0)
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
int CParameterIO::ReadOneMacro(int iset, CString &strLine, CString *strItems)
{
  int err;
  if (iset < 0 || iset >= MAX_MACROS)
    return 1;
  CString *macros = mWinApp->GetMacros();
  macros[iset] = "";
  while ((err = ReadAndParse(strLine, strItems, MAX_TOKENS)) == 0) {
    if (NAME_IS("EndMacro"))
      break;
    strLine.TrimRight("\r\n");
    macros[iset] += strLine+ "\r\n";
  }
  if (mWinApp->mMacroEditer[iset])
    mWinApp->mMacroEditer[iset]->TransferMacro(false);
  else
    mWinApp->mMacroProcessor->ScanForName(iset);
  return err;
}

// Write all macros to the current file
void CParameterIO::WriteAllMacros(void)
{
  CString *macros = mWinApp->GetMacros();
  CString macCopy;
  for (int i = 0; i < MAX_MACROS; i++) {
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
// 2 for int,double, 3 for int.  precision is # of decimal places
CString CParameterIO::EntryListToString(int type, int precision, int numVals, int *iVals, 
                                        double *dVals)
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
int CParameterIO::StringToEntryList(int type, CString str, int &numVals, 
                                    int *iVals, double *dVals, int maxVals)
{
  int err, ind, ind2;
  int twocomma = 0;
  double temp;
  CString *strItems = new CString[maxVals];
  CString strBit;
  err = ParseString(str, strItems, maxVals);
  numVals = 0;
  for (int i = 0; i < maxVals; i++) {
    if (strItems[i].IsEmpty())
      break;
    ind = strItems[i].Find(',');
    if (type % 2) {
      if (ind == 0) {
        err = 1;
        break;
      }
      if (ind > 0 && ind < strItems[i].GetLength() - 1)
        twocomma = -1;
      if (type == 1)
        dVals[numVals++] = atof((LPCTSTR)strItems[i]);
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
  if (mWinApp->mTSController->GetSkipBeamShiftOnAlign())
    mWinApp->AppendToLog("Special option is set to skip beam shift when aligning "
      "during tilt series");
  if (mWinApp->mScope->GetUsePiezoForLDaxis())
     mWinApp->AppendToLog("Special option is set to use piezo movement for Low Dose"
       " on-axis shift");
  if (mWinApp->mShiftManager->GetDisableAutoTrim())
    mWinApp->AppendToLog("Special option is set to disable automatic trimming of dark "
    "borders in Autoalign for some cases");
  if (mWinApp->mTSController->GetAllowContinuous())
    mWinApp->AppendToLog("Special option is set to allow continuous mode in tilt series");
  if (mWinApp->mFocusManager->GetNormalizeViaView())
    mWinApp->AppendToLog("Special option is set to normalize through View before "
    "autofocus in Low Dose");
  if (mWinApp->mScope->GetAdjustFocusForProbe())
     mWinApp->AppendToLog("Special option is set to adjust focus when changing between "
     "nanoprobe and microprobe");
  if (mWinApp->mCamera->GetNoNormOfDSdoseFrac())
    mWinApp->AppendToLog("Special option is set for no gain normalization of returned"
    " sum for dark-subtracted K2 dose frac shots");
}

// Outputs a short or float vector in as many lines as it takes
void CParameterIO::OutputVector(const char *key, int size, ShortVec *shorts, 
  FloatVec *floats)
{
  CString oneState, entry;
  int ldj, ldi, j, i;
  if (!size)
    return;
  ldj = (size + MAX_TOKENS - 3) / (MAX_TOKENS - 2);
  ldi = 0;
  for (j = 0; j < ldj; j++) {
    oneState = key;
    ldi = size - ldj * (MAX_TOKENS - 2);
    i = B3DMIN(MAX_TOKENS - 2, ldi);
    for (i = 0; i < MAX_TOKENS - 2; i++) {
      if (i >= size)
        break;
      if (shorts)
        entry.Format(" %d", shorts->at(i));
      else
        entry.Format(" %f", floats->at(i));
      oneState += entry;
    }
    mFile->WriteString(oneState + "\n");
  }
}

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
  bool recognized = true, recognized2 = true, recognized3 = true;
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
  }
#define PROP_TEST_SECT3
#include "PropertyTests.h"
#undef PROP_TEST_SECT3
  else 
    recognized3 = false;
      
  if (recognized || recognized2 || recognized3) {
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
    if (KGetOneInt(propStr + a, ival)) \
      b##Set##c(ival); \
  }
#define BOOL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    if (KGetOneInt(boolStr + a, ival)) \
      b##Set##c(ival != 0); \
  }
#define FLOAT_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    if (KGetOneFloat(propStr + a, fval, 2)) \
      b##Set##c(fval); \
  }
#define DBL_PROP_TEST(a, b, c) \
  else if (name.CompareNoCase(a) == 0) { \
    if (KGetOneFloat(propStr + a, fval, 2)) \
      b##Set##c((double)fval); \
  }

void CParameterIO::UserSetProperty(void)
{
#define PROP_MODULES
#include "PropertyTests.h"
#undef PROP_MODULES
  CString propStr = "Enter value for property ";
  CString boolStr = "Enter 0 or 1 for property ";
  CString name;
  int ival = 0;
  float fval = 0.;
  bool recognized = true, recognized2 = true, recognized3 = true;
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
  }
#define PROP_TEST_SECT3
#include "PropertyTests.h"
#undef PROP_TEST_SECT3
  else 
    recognized3 = false;
      
  if (recognized || recognized2 || recognized3) {
  }
#define PROP_TEST_SECT4
#include "PropertyTests.h"
#undef PROP_TEST_SECT4
  else {
    AfxMessageBox(name + " is not a recognized property or cannot be set by this "
      "command");
  }
}
#undef INT_PROP_TEST
#undef BOOL_PROP_TEST
#undef FLOAT_PROP_TEST
#undef DBL_PROP_TEST

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
  bool recognized = true, recognized2 = true;
  if (false) {
  }
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
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
  B3DCLAMP(valCopy, 2.147e9, 2.147e9);
  ival = B3DNINT(valCopy);
  bool recognized = true, recognized2 = true;
  if (false) {
  }
#define SET_TEST_SECT1
#include "SettingsTests.h"
#undef SET_TEST_SECT1
  else 
    recognized = false;
      
  if (recognized) {
  }
#define SET_TEST_SECT2
#include "SettingsTests.h"
#undef SET_TEST_SECT2
  else 
    recognized2 = false;
      
  if (recognized || recognized2) {
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
