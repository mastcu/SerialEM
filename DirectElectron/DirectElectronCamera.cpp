
///////////////////////////////////////////////////////////////////
// file:  DirectElectronCamera.cpp
// Main file for controlling Direct Electron cameras.
//
// Comments about originally supported cameras: 
// This is the main file that wraps the functionality of the LC 1100.
// The camera control is based on two COM modules:
// FSM_MemoryLib.dll and SoliosFor4000.dll.  The FSM_MemoryLib.dll
// deals with the MIL libs for reading the image from the Solios
// capture card and the SoliosFor4000.dll COM lib deals with
// writing and receiving the commands of the camera.
//
//
// author of SerialEM: David Mastronarde
//
// LC1100 camera integration by Direct Electron
// 2009
//
///////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <fstream>
#include <string>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include "../SerialEM.h"

#include "DirectElectronCamera.h"
#include "DE.h"
#include "DeInterface.Win32.h"

using namespace std;

#include "../EMScope.h"
#include "../TSController.h"
#include "../ShiftManager.h"
#include "../EMmontageController.h"
#include "../ProcessImage.h"
#include "../MacroProcessor.h"
#include "../PluginManager.h"
#include "../CameraController.h"
#include "..\GainRefMaker.h"
#include "..\Utilities\XCorr.h"

// UPDATE Docs/DEserverProperties.txt FOR ADDED OR DISCONTINUED PROPERTIES

// Property strings used more than once but only here
static const char *psCamPosition = "Camera Position";
static const char *psCamPositionCtrl = "Camera Position Control";
static const char *psSave = "Save";
static const char *psNoSave = "Discard";
static const char *psReadoutDelay = "Sensor Readout Delay (milliseconds)";
static const char *psServerVersion = "Server Software Version";
static const char *psHardwareBin = "Sensor Hardware Binning";
static const char *psBinMode = "Binning Mode";
static const char *psROIMode = "ROI Mode";
static const char *psHardwareROI = "Sensor Hardware ROI";
static const char *psHWandSW = "Hardware and Software";
static const char *psSWonly = "Software Only";
static const char *psAutoRepeatRef = "Auto Repeat Reference - Multiple Acquisitions";
static const char *psEnable = "Enable";
static const char *psDisable = "Disable";
static const char *psMotionCor = "Motion Correction";
static const char *psMotionCorNew = "Image Processing - Motion Correction";
static const char *psCountsPerElec = "Electron Counting - Counts Per Electron";
static const char *psCountsPerEvent = "Event Counting - Value Per Event";
static const char *psADUsPerElectron = "ADUs Per Electron";
static const char *psFramesPerSec = "Frames Per Second";

// These are concatenated into multiple strings
#define DE_PROP_COUNTING "Electron Counting"
#define DE_PROP_AUTOMOVIE "Autosave Movie - "
#define DE_PROP_AUTORAW "Autosave Raw Frames - "
#define DE_PROP_AUTOSUM "Autosave Sum Frames - "


bool FIX_COLUMNS = false;
#define VALID_REF_MINUTES 5

static HANDLE sLiveMutex = NULL;
#define LIVE_MUTEX_WAIT 2000
static void CleanupLiveMode(LiveThreadData *td);
static int sUsingAPI2;                      // Flag to use API2: 2 if API2-only plugin


///////////////////////////////////////////////////////////////////
// Constructor of the spectral camera controlling class.
// It will initialize the references that are used to communicate
// with the camera, handle the memory management, along with
// the callback
///////////////////////////////////////////////////////////////////
DirectElectronCamera::DirectElectronCamera(int camType, int index)
{

  mCamType = camType;
  mCurCamIndex = -1;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mCamParams = mWinApp->GetCamParams();

  // This will need to be false if the server can ever be connected to from multiple 
  // sources or can restart without our knowledge, and it should be false if server delays
  // are eliminated
  mTrustLastSettings = true;

  m_DE_CONNECTED = false;
  mServerVersion = 0;
  mAPI2Server = false;
  mRepeatForServerRef = 0;
  mSetNamePredictionAgeLimit = 300;
  mEnabledSTEM = false;
  mStartedScan = false;
  m_STOPPING_ACQUISITION = false;

  // mDEsameCam eliminated, main controller manages all insertions/retractions sensibly

  //client/server model
  mDeServer = NULL;
  mInitializingIndex = -1;

  // DO some live thread initializations
  mLiveThread = NULL;
  sLiveMutex = CreateMutex(0, 0, 0);
  mLiveTD.buffers[0] = mLiveTD.buffers[1] = mLiveTD.rotBuf = NULL;
  mLiveTD.outBufInd = -2;

  /*Any Camera type that is greater than 2 represents that we are using the DE client
    server archietcture.  We just need one instance of the interface.  Then
    we can use the setCamera function to switch between camera systems. */
  if (mCamType >= 2) {
    m_DE_CLIENT_SERVER = true;
    if (mWinApp->mPluginManager->GetDEplugIndex() < 0) {
      AfxMessageBox("The Direct Electron camera interface plugin was not loaded.\n\n"
        "The DE camera will not be available");
    } else {

      mDeServer = mWinApp->mPluginManager->GetDEcamFuncs();

      // Set the server and rotation properties from the camParam
      CameraParameters *camP = &mCamParams[index];
      mDE_SERVER_IP = camP->DEServerIP;
      mDE_READPORT = camP->DE_ServerReadPort;
      mDE_WRITEPORT = camP->DE_ServerWritePort;
      m_DE_ImageRot = camP->DE_ImageRot;
      m_DE_ImageInvertX = camP->DE_ImageInvertX;

      InitializeLastSettings();
      SEMTrace('D', "DEServer is %s ReadPort:%d WritePort:%d ImageRotation:%d "
        "ImageInversion %d", mDE_SERVER_IP, mDE_READPORT, mDE_WRITEPORT, m_DE_ImageRot, 
        m_DE_ImageInvertX);
    }

  } else {
    m_DE_CLIENT_SERVER = false;
  }
}

// Initialize all of the saved settings that will be used to reduce calls to server
// If the situation is modified to tolerate server restarts and SerialEM can detect this,
// then this is all that needs to be called when that happens
void DirectElectronCamera::InitializeLastSettings()
{
  mLastSaveFlags = -1;
  mLastSumCount = -1;
  mLastSaveDir = "";
  mLastSuffix = "";
  mLastXoffset = mLastYoffset = -1;
  mLastROIsizeX = mLastROIsizeY = -1;
  mLastXimageSize = mLastYimageSize = -1;
  mLastXbinning = mLastYbinning = -1;
  mLastExposureMode = -999;
  mLastExposureTime = -1.;
  mLastPreExposure = -1.;
  mLastProcessing = -1;
  mLastNormCounting = -1;
  mLastMovieCorrEnabled = -1;
  mLastUnnormBits = -1;
  mLastElectronCounting = -1;
  mLastSuperResolution = -1;
  mLastFPS = -1.;
  mLastLiveMode = sUsingAPI2 ? 0 : -1;
  mLastServerAlign = -1;
  mLastUseHardwareBin = -1;
  mLastUseHardwareROI = -1;
  mLastAutoRepeatRef = -1;
  mElecCountsScaled = 1;
}


///////////////////////////////////////////////////////////////////
//Destructor to invoke the UnInitialize routine
///////////////////////////////////////////////////////////////////
DirectElectronCamera::~DirectElectronCamera(void)
{
  unInitialize();
}

///////////////////////////////////////////////////////////////////
//unInitialize() routine will disconnect from the server after retracting camera unless
// W debug output is set
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::unInitialize()
{
  if (mDeServer) {
    if (m_DE_CONNECTED) {
      if (!GetDebugOutput('W'))
        mDeServer->setProperty(mAPI2Server ? psCamPositionCtrl : psCamPosition,
          mAPI2Server ? "Retract" : DE_CAM_STATE_RETRACTED);
      mDeServer->close();
    }
  }
  mDeServer = NULL;
  return 1;
}


///////////////////////////////////////////////////////////////////
//initialize() routine.  Will initialize the server and set the current camera
//
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::initialize(CString camName, int camIndex)
{
  if (!m_DE_CLIENT_SERVER) {
    AfxMessageBox("The LC1100 is no longer supported");
    return -1;
  }
  sUsingAPI2 = SEMUseAPI2ForDE();

  //pull the reference to the ToolDialog box for DE.
  //We need to give our dialogs the camera reference.
  mWinApp->mDEToolDlg.setCameraReference(this);

  //initialize the DE server.
  int check = -1;
  if (!m_DE_CONNECTED) {
    check = initDEServer();
    if (check == -1)
      return check;
  }
  mInitializingIndex = camIndex;
  mCamType = mCamParams[camIndex].DE_camType;
  check = initializeDECamera(camName, camIndex);
  mInitializingIndex = -1;
  if (check == -1)
    return check;
  mCurCamIndex = camIndex;


  return 1;
}

///////////////////////////////////////////////////////////////////
// Ensures that there is a proper connection between the client
// and the server.
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::initDEServer()
{
  if (!mDeServer) {
    SEMTrace('D', "No DE plugin was loaded");
    m_DE_CONNECTED = false;
    return -1;
  }

  SEMTrace('D', "Supposed readPort %d, write port: %d and de server: %s API: %d",
    mDE_READPORT, mDE_WRITEPORT, mDE_SERVER_IP, sUsingAPI2 ? 2 : 1);

  if ((!sUsingAPI2 && (mDE_READPORT <= 0 || mDE_WRITEPORT <= 0)) || mDE_SERVER_IP == "") {
    AfxMessageBox("You did not properly setup the DE Server Properties!!!");
    SEMTrace('D', "ERROR: You did not properly setup the DE server properties, double "
      "check SerialEMProperties.txt");
    m_DE_CONNECTED = false;
    return -1;
  }
  if (sUsingAPI2 && mDE_SERVER_IP == "127.0.0.1")
    mDE_SERVER_IP = "localhost";

  if (mDeServer->connectDE((LPCTSTR)mDE_SERVER_IP, mDE_READPORT, mDE_WRITEPORT)) {
    SEMTrace('D', "Successfully connected to the DE Server.");
    m_DE_CONNECTED = true;
  } else {
    AfxMessageBox("Could NOT connect to DE server. \nMake sure the Direct Electron "
      "server is running.!");
    ErrorTrace("ERROR: Could NOT connect to the DE server.");
    m_DE_CONNECTED = false;
    return -1;
  }

  //Read out the possible list of cameras that have been setup:
  StringVec cameras;
  if (!mDeServer->getCameraNames(&cameras)) {
    CString str = ErrorTrace("ERROR: Could NOT retrieve any of the DE camera names.");
    AfxMessageBox(str);
    return -1;
  }

  CString listCameras = "Camera list:  \r\n";

  for (int i = 0; i < (int)cameras.size(); i++)
    listCameras += CString(cameras[i].c_str()) + "\r\n";

  SEMTrace('D', "%s", (LPCTSTR)listCameras);

  std::string propValue;
  if (mDeServer->getProperty(std::string(psServerVersion), &propValue))
    SetSoftwareAndServerVersion(propValue);

  // 5/5/25: Removed restriction on 32 bit version with API2
  return 1;
}

// Initialize a particular DE camera, getting its properties and setting flags
int DirectElectronCamera::initializeDECamera(CString camName, int camIndex)
{
  //read in a properties file to know how to assign
  //Read in the information as this is very critical
  if (m_DE_CONNECTED) {
    CString str;
    CString tmp, tmp2;
    std::string propValue;
    bool result, sawHWROI = false, sawHWbinning = false, hasHDRlicense = false;
    bool hasCountingLicense = false, noPreset = true;
    BOOL debug = GetDebugOutput('D'), scanDebug = GetDebugOutput('!');
    const char *propsToCheck[] = {DE_PROP_COUNTING, psMotionCor, psMotionCorNew, 
      "Temperature Control - Setpoint (Celsius)", 
      "Temperature - Cool Down Setpoint (Celsius)", psReadoutDelay, psHardwareBin,
      psBinMode, psHardwareROI, psROIMode, psCountsPerElec,
      psCountsPerEvent, psADUsPerElectron, "ADUs Per Electron Bin1x", 
      "Electron Counting - Super Resolution", "Event Counting - Super Resolution",
      "Readout - Hardware HDR", "Readout - HDR"};
    unsigned int flagsToSet[] = {DE_CAM_CAN_COUNT, DE_CAM_CAN_ALIGN, DE_CAM_CAN_ALIGN, 
      DE_HAS_TEMP_SET_PT, DE_HAS_TEMP_SET_PT, DE_HAS_READOUT_DELAY, DE_HAS_HARDWARE_BIN,
      DE_HAS_HARDWARE_BIN, DE_HAS_HARDWARE_ROI, DE_HAS_HARDWARE_ROI,
      DE_SCALES_ELEC_COUNTS, DE_SCALES_ELEC_COUNTS, DE_HARDWARE_SCALES, 
      DE_HARDWARE_SCALES, DE_CAN_SAVE_SUPERRES, DE_CAN_SAVE_SUPERRES, DE_HAS_HARDWARE_HDR,
      DE_HAS_HARDWARE_HDR};
    int numFlags = sizeof(flagsToSet) / sizeof(unsigned int);

    result = mDeServer->setCameraName((LPCTSTR)camName);
    m_DE_CurrentCamera = camName;

    SEMTrace('D', "Camera name set to : %s", (LPCTSTR)camName);

    if (!result) {
      str = ErrorTrace("Could not set the camera name: %s", (LPCTSTR)camName);
      AfxMessageBox(str);
      return -1;
    }

    double startTime = GetTickCount();
    CString listProps;
    listProps += camName;
    listProps += "\r\n\r\n";

    // read out all the possible properties and report when starting in Debug mode
    StringVec camProps;
    if (!mDeServer->getProperties(&camProps)) {
      str = ErrorTrace("Could not get properties for the camera named : %s", 
        (LPCTSTR)camName);
      AfxMessageBox(str);
      return -1;
    }
    SEMTrace('D', "Time to get properties %.3f", SEMTickInterval(startTime) / 1000.);

    // Clear out the flags in the structure, then set them if seen
    for (int j = 0; j < numFlags; j++)
      mCamParams[camIndex].CamFlags &= ~flagsToSet[j];
    for (int i = 0; i < (int)camProps.size(); i++) {
      for (int j = 0; j < numFlags; j++)
        if (!camProps[i].compare(propsToCheck[j]))
          mCamParams[camIndex].CamFlags |= flagsToSet[j];
      if (!camProps[i].compare("Hardware Binning X"))
        sawHWbinning = true;
      if (!camProps[i].compare("Hardware ROI Size X"))
        sawHWROI = true;
      if (!camProps[i].compare("License - Event Counting")) {
        if (mDeServer->getProperty(camProps[i], &propValue) &&
          propValue.find("Valid") == 0)
          hasCountingLicense = true;
      }
      if (!camProps[i].compare("License - HDR Readout Modes")) {
        if (mDeServer->getProperty(camProps[i], &propValue) &&
          propValue.find("Valid") == 0)
          hasHDRlicense = true;
      }
      if (!camProps[i].compare("Preset - List") &&
        mDeServer->getProperty(camProps[i], &propValue)) {
        UtilSplitString(propValue, ",", mCamPresets);
        if (!mWinApp->mScope->GetSimulationMode()) {
          for (int j = 0; j < (int)mCamPresets.size(); j++)
            if (mCamPresets[j] == "Custom" || mCamPresets[j] == "Last Used Parameters")
              VEC_REMOVE_AT(mCamPresets, j);
        }
      }
      if (!camProps[i].compare("Scan - Preset List") &&
        mDeServer->getProperty(camProps[i], &propValue)) {
        UtilSplitString(propValue, ",", mScanPresets);
        for (int j = 0; j < (int)mScanPresets.size(); j++)
          if (mScanPresets[j] == "Last Used Parameters")
            VEC_REMOVE_AT(mScanPresets, j);
      }
      if (!camProps[i].compare("Preset - Current") &&
        mDeServer->getProperty(camProps[i], &propValue)) {
        noPreset = propValue == "Custom";
      }

      if (debug && (camProps[i].find("Scan") != 0 || scanDebug)) {
        listProps += camProps[i].c_str();

        // The one that crashes
        if (camProps[i].find("Preset - Properties") == 0 && noPreset)
          listProps += "\r\n";
        else if (mDeServer->getProperty(camProps[i], &propValue))
          listProps += CString("    Value: ") + propValue.c_str() + "\r\n";
      }
    }

    // Try to get server version again after all that if it didn't work before
    if (!mServerVersion) {
      if (mDeServer->getProperty(psServerVersion, &propValue)) {
        SetSoftwareAndServerVersion(propValue);
      } else {
        str = ErrorTrace("Call to get software version failed; try restarting SerialEM");
        AfxMessageBox(str);
        mServerVersion = 1;
      }
    }

    // Clear super-res flag if server no longer supports it
    // Use licenses now as the ruling factor
    if (mServerVersion >= DE_NO_MORE_SUPER_RES)
      mCamParams[camIndex].CamFlags &= ~DE_CAN_SAVE_SUPERRES;
    if (mServerVersion >= DE_HAS_LICENSE_PROPS) {
      if (hasCountingLicense)
        mCamParams[camIndex].CamFlags |= DE_CAM_CAN_COUNT;
      else
        mCamParams[camIndex].CamFlags &= ~DE_CAM_CAN_COUNT;
      if (!hasHDRlicense)
        mCamParams[camIndex].CamFlags &= ~DE_HAS_HARDWARE_HDR;
    }

    if (debug) {
      if (!GetDebugOutput('*')) {
        SEMAppendToLog("\r\nAdd * (asterisk) to DebugOutput to get full list of camera "
          "properties, ! for Scan too\r\n");
      } else {
        str += "\r\nProperties for : " + listProps;
        str += "\r\n\r\n";
        PrintfToLog("%s", str);
      }
    }

    // If saw HW ROI for 2.7 on, check the features and turn it on if either X or Y is
    // available (both may not be)
    if (mServerVersion >= DE_AUTOSAVE_RENAMES2) {
      if (sawHWROI) {
        if ((mDeServer->getProperty("Feature - HW ROI X", &propValue) &&
          propValue == "On") || (mDeServer->getProperty("Feature - HW ROI Y",
            &propValue) && propValue == "On"))
          mCamParams[camIndex].CamFlags |= DE_HAS_HARDWARE_ROI;
      }

      // Check if there is a non-empty string for hardware bin
      // and supersede the counting detection with this feature value
      if (sawHWbinning && mDeServer->getProperty("Hardware Binning X", &propValue) &&
        propValue.length() > 0)
        mCamParams[camIndex].CamFlags |= DE_HAS_HARDWARE_BIN;
      /* 8/9/25: Philip says there can be a feature listed but no license, so this is
      worthless
      if (mDeServer->getProperty("Feature - Counting", &propValue)) {
        if (propValue == "On")
          mCamParams[camIndex].CamFlags |= DE_CAM_CAN_COUNT;
        else 
          mCamParams[camIndex].CamFlags &= ~DE_CAM_CAN_COUNT;
      }*/
    }

    // Set some properties that are not going to be managed
    mNormAllInServer = mServerVersion >= DE_ALL_NORM_IN_SERVER;
    if (mNormAllInServer) {
      mCamParams[camIndex].CamFlags |= DE_NORM_IN_SERVER; 
      if (!IsApolloCamera())
        setIntProperty(mAPI2Server ?
          "Readout - Frames to Ignore" : DE_PROP_AUTOMOVIE"Ignored Frames", 0);
      //setStringProperty("Backward Compatibility", psDisable);
      // Leave this for engineers to be able to set depending on what is best
      //if (mCamParams[camIndex].CamFlags & DE_CAM_CAN_COUNT)
        //setStringProperty(DE_PROP_COUNTING" - Apply Pre-Counting Gain", psEnable);

      // Hardware binning bins by averaging so set the software binning to match
      if (mCamParams[camIndex].CamFlags & DE_HAS_HARDWARE_BIN)
        setStringProperty(mAPI2Server ? "Binning Method" : "Binning Algorithm", 
          "Average");

      // The default is 1 for the final image to average frames based on the number of
      // frames summed last set; this provides sums instead
      setIntProperty(mAPI2Server ? "Autosave Final Image Sum Count" :
        "Autosave Final Image - Sum Count", -1);
    } else {
      setStringProperty(DE_PROP_AUTORAW"Save Correction", psDisable);  
      setStringProperty(DE_PROP_AUTOSUM"Save Correction", psDisable);
      setIntProperty(DE_PROP_AUTOSUM"Ignored Frames", 0);
    }
    if (mServerVersion >= DE_HAS_AUTO_PIXEL_DEPTH)
      setStringProperty(mAPI2Server ? 
        "Autosave Integrated Movie Pixel Format" : DE_PROP_AUTOMOVIE"Pixel Depth",
        "Auto");

    // TODO - what defines this?
    if (mServerVersion >= DE_FRAME_IS_IMAGE_SIZE)
      mCamParams[camIndex].CamFlags |= DE_FRAMES_MATCH_IMAGE;

    getFloatProperty("Frames Per Second (Max)", mCamParams[camIndex].DE_MaxFrameRate);
    B3DCLAMP(mCamParams[camIndex].DE_MaxFrameRate, 1.f, 5000.f);
    if (mServerVersion >= DE_HAS_REPEAT_REF && mServerVersion < DE_AUTOSAVE_RENAMES2) {
      setStringProperty(psAutoRepeatRef, psDisable);
      mLastAutoRepeatRef = 0;
    }

    if (IsApolloCamera()) {
      if (mCamParams[camIndex].CamFlags & DE_HARDWARE_SCALES)
        getIntProperty(psADUsPerElectron, mElecCountsScaled);
      else
        mElecCountsScaled = 16;
      getFloatProperty(psFramesPerSec, mCamParams[camIndex].DE_FramesPerSec);
      mCamParams[camIndex].DE_CountingFPS = mCamParams[camIndex].DE_FramesPerSec;
    } else if (mCamParams[camIndex].CamFlags & DE_SCALES_ELEC_COUNTS)
      getIntProperty(mAPI2Server ? psCountsPerEvent : psCountsPerElec, mElecCountsScaled);

    // Make sure that if an autosave dir can be set and one is in properties, it is there
    // or can be created
    if (mServerVersion >= DE_CAN_SET_FOLDER) {
      if (ServerIsLocal() && !mCamParams[camIndex].DE_AutosaveDir.IsEmpty() &&
       CreateFrameDirIfNeeded(mCamParams[camIndex].DE_AutosaveDir, &str, 'D')) {
         PrintfToLog("WARNING: %s", (LPCTSTR)str);
         mCamParams[camIndex].DE_AutosaveDir = "";
      }

      // If the autosave dir property is (now or originally) empty, strip the dir from the
      // server back to the
      // camera name and save that as the dir if possible; otherwise clear it out
      if (mCamParams[camIndex].DE_AutosaveDir.IsEmpty()) { 
        if (getStringProperty(DE_PROP_AUTOSAVE_DIR, str)) {
          str.Replace('/', '\\');
          int len;
          while ((len = str.GetLength()) > 3) {
            if (str.GetAt(len - 1) == '\\')
              str = str.Left(len - 1);
            UtilSplitPath(str, tmp, tmp2);
            if (!tmp2.Compare(camName)) {
              mCamParams[camIndex].DE_AutosaveDir = str;
              break;
            }
            str = tmp;
          }
        }
      }
    } else
      mCamParams[camIndex].DE_AutosaveDir = "";

    //If frame folder doesn't have to be the autosave folder, but is not yet assigned, 
    //initialize it to be the autosave folder with possible subfolder
    if (CanIgnoreAutosaveFolder() && mCamParams[camIndex].dirForFrameSaving.IsEmpty()) {
      mCamParams[camIndex].dirForFrameSaving = mCamParams[camIndex].DE_AutosaveDir;
      str = mWinApp->mCamera->GetDirForDEFrames();
      if (!str.IsEmpty())
        mCamParams[camIndex].dirForFrameSaving += "\\" + str;
    }
      

    // Set that we can align if server is local and frames are normalized
    if (ServerIsLocal() && mServerVersion >= DE_ALL_NORM_IN_SERVER)
      mCamParams[camIndex].CamFlags |= DE_WE_CAN_ALIGN;

    FinishCameraSelection(true, &mCamParams[camIndex]);
  }

  return 1;
}

//Check if Direct Electron Camera can save frames to folder outside autosave folder
bool DirectElectronCamera::CanIgnoreAutosaveFolder()
{
  return GetServerVersion() >= DE_CAN_SET_FOLDER && ServerIsLocal();
}

// Save the string for the software version and convert it to a number
void DirectElectronCamera::SetSoftwareAndServerVersion(std::string &propValue)
{
  CString token;
  int third, curpos = 0;
  mSoftwareVersion = propValue.c_str();

  // Convert to a single number, 1000000 * major * 10000 * minor + build
  token = mSoftwareVersion.Tokenize(".", curpos);
  if (!token.IsEmpty()) {
    mServerVersion = 1000000 * atoi((LPCTSTR)token);
    token = mSoftwareVersion.Tokenize(".", curpos);
    if (!token.IsEmpty()) {
      mServerVersion += 10000 * atoi((LPCTSTR)token);
      token = mSoftwareVersion.Tokenize(".", curpos);
      if (!token.IsEmpty()) {
        third = atoi((LPCTSTR)token);
        token = mSoftwareVersion.Tokenize(".", curpos);
        if (token.IsEmpty())
          mServerVersion += third;
        else
          mServerVersion = 100 * mServerVersion + 10000 * third + atoi((LPCTSTR)token);
      }
    }
  }
  if (mServerVersion >= DE_CAN_SAVE_MRC)
    mWinApp->mDEToolDlg.SwitchSaveOptToMRC();
  mAPI2Server = mServerVersion >= DE_HAS_API2;
}

// Return the current names of virtual detectors in a string vector, returns 1 if they
// are not all findable
int DirectElectronCamera::GetVirtualDetectorNames(StringVec &strList, ShortVec &enabled)
{
  int ind;
  CString cstr, value, keys;
  keys = mWinApp->GetDebugKeys();
  if (keys.Find('D') >= 0)
    mWinApp->SetDebugOutput('0');
  strList.clear();
  enabled.clear();
  mCurVirtShape.clear();
  for (ind = 0; ind < NUM_VIRTUAL_DET; ind++) {

    // Get name for each
    cstr.Format("Scan - Virtual Detector %d Name", ind);
    if (getStringProperty(cstr, value))
      strList.push_back((LPCTSTR)value);
    else
      break;

    // Get the shape, set enabled if not Off and save current shape for restoring
    cstr.Format("Scan - Virtual Detector %d Shape", ind);
    if (getStringProperty(cstr, value)) {
      enabled.push_back(value == "Off" ? 0 : 1);
      mCurVirtShape.push_back((LPCTSTR)value);
    } else
      break;
  }
  if (keys.Find('D') >= 0)
    mWinApp->SetDebugOutput(keys);
  return ind < NUM_VIRTUAL_DET ? 1 : 0;
}

bool DirectElectronCamera::IsApolloCamera()
{
  CameraParameters *camP = mCamParams +
    (mInitializingIndex >= 0 ? mInitializingIndex : mCurCamIndex);
  return (camP->CamFlags & DE_APOLLO_CAMERA) != 0;
}

////////////////
// 2/15/17: removed unused setTotalFrames, getPreviousDataSetName, getNextDataSetName
///////////////

// Returns the string for the camera insertion state
CString DirectElectronCamera::getCameraInsertionState()
{
  std::string camState = "";
  mDeServer->getProperty(mAPI2Server ? "Camera Position Status" : psCamPosition, 
    &camState);

  m_Camera_InsertionState = camState.c_str();

  return m_Camera_InsertionState;
}

// Return 1 if the camera is inserted, 0 if not, and update the tool panel
int DirectElectronCamera::IsCameraInserted()
{
  int retval = getCameraInsertionState() == CString(DE_CAM_STATE_INSERTED) ? 1 : 0;
  mWinApp->mDEToolDlg.UpdateInsertion(retval);
  return retval;
}


///////////////////////////////////////////////////////////////////
//setCameraName() routine for selecting a camera after initialization

///////////////////////////////////////////////////////////////////
void DirectElectronCamera::setCameraName(CString camName, int index, BOOL ifSTEM)
{

  //Check to make sure we dont set camera name if its
  //already set.

  CameraParameters *camP = &mCamParams[index];

  //set to the current Values from the camera.
  //Just in case in the future we need to
  mDE_SERVER_IP = camP->DEServerIP;
  mDE_READPORT = camP->DE_ServerReadPort;
  mDE_WRITEPORT = camP->DE_ServerWritePort;
  m_DE_ImageRot = camP->DE_ImageRot;
  m_DE_ImageInvertX = camP->DE_ImageInvertX;

  if (!BOOL_EQUIV(ifSTEM, mEnabledSTEM)) {
    if (setStringWithError("Scan - Enable", ifSTEM ? "On" : "Off"))
      mEnabledSTEM = ifSTEM;
    else
      PrintfToLog("WARNING: Could not %able Scan: %s", ifSTEM ? "en" : "dis", 
      (LPCTSTR)mLastErrorString);
  }

  if (m_DE_CurrentCamera == camName) {
    return;
  }

  if (!mDeServer->setCameraName((LPCTSTR)camName)) {
    CString str = ErrorTrace("Could NOT set the camera name to %s in the DE server.",
      (LPCTSTR)camName);
    AfxMessageBox(str);
  } else {

    //Now set the new camera.
    m_DE_CurrentCamera = camName;
    mCamType = camP->DE_camType;
    mCurCamIndex = mWinApp->GetCurrentCamera();

    //its ok to update the panel after sending out any settings that exist
    SEMTrace('D', "Set the camera to:  %s", (LPCTSTR)camName);
    FinishCameraSelection(false, camP);
  }
}

// Common tasks when initializing or selecting a camera
void DirectElectronCamera::FinishCameraSelection(bool initialCall, CameraParameters *camP)
{
  if (CurrentIsDE12()) {
    if ((camP->CamFlags & DE_HAS_READOUT_DELAY) && !(camP->CamFlags & DE_APOLLO_CAMERA))
      getIntProperty(psReadoutDelay, mReadoutDelay);
    else 
      mReadoutDelay = -1;
    getStringProperty("Sensor Module SN", mDE12SensorSN);
  }
  mWinApp->mDEToolDlg.ApplyUserSettings();
  mWinApp->mDEToolDlg.updateDEToolDlgPanel(initialCall);
}

// Functions to indicate if camera is a DE12, a survey, or a superset of such cameras
BOOL DirectElectronCamera::CurrentIsDE12() 
{
  return mCamType == DE_12;
}
BOOL DirectElectronCamera::CurrentIsSurvey() 
{
  return mCamType == DE_12_Survey;
}


///////////////////////////////////////////////////////////////////
// SetAllAutoSaves sets the properties for the three kinds of saving, keeping track of
// their previous settings and setting a value to the server only if it changes
//
int DirectElectronCamera::SetAllAutoSaves(int autoSaveFlags, int sumCount, CString suffix,
  CString saveDir, bool counting)
{
  bool ret1 = true, ret2 = true, ret3 = true, ret4 = true, ret5 = true, ret6 = true;
  bool ret7 = true;

  // With old server, should get DE_SAVE_FRAMES to save single frames and DE_SAVE_SUMS
  // to save sums that are NOT single frames
  // With new server, should get DE_SAVE_SUMS for any kind of processed movie saving,
  // and DE_SAVE_FRAMES only to save raw frames too
  int saveRaw = (autoSaveFlags & DE_SAVE_FRAMES) ? 1 : 0;
  int saveSums = (autoSaveFlags & DE_SAVE_SUMS) ? 1 : 0;
  int saveFinal = (autoSaveFlags & DE_SAVE_FINAL) ? 1 : 0;
  bool setSaves = (mLastSaveFlags < 0 || !mTrustLastSettings) && !mLiveThread;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {
    if (setSaves || ((mLastSaveFlags & DE_SAVE_FRAMES) ? 1 : 0) != saveRaw) {
      /*if (mAPI2Server)
        ret1 = mDeServer->setProperty("Autosave Single Frames", saveRaw ? "On" : "Off");
      else*/
      if (mServerVersion < DE_AUTOSAVE_RENAMES2) {
        ret1 = mDeServer->setProperty("Autosave Raw Frames", saveRaw ? psSave : psNoSave);
        if (!ret1)
          mWinApp->AppendToLog(ErrorTrace("Error setting save raw/single"));
      }
    }
    if (setSaves || ((mLastSaveFlags & DE_SAVE_FINAL) ? 1 : 0) != saveFinal)
      ret3 = mDeServer->setProperty("Autosave Final Image", saveFinal ? psSave :psNoSave);
    if (setSaves || ((mLastSaveFlags & DE_SAVE_SUMS) ? 1 : 0) != saveSums)
      ret2 = mDeServer->setProperty(mNormAllInServer ? "Autosave Movie" : 
      "Autosave Summed Image", saveSums ? psSave : psNoSave);
    SEMTrace('D', "SetAllAutoSaves: setSaves %d  saveSums %d saveFinal %d", 
      setSaves ? 1 : 0, saveSums, saveFinal);
    if (saveSums && (mLastSumCount < 0 || !mTrustLastSettings || 
      mLastSumCount != sumCount) && !mLiveThread) {
        if (mNormAllInServer)
          ret4 = justSetIntProperty(mAPI2Server ? "Autosave Movie Sum Count" :
          B3DCHOICE(counting && !IsApolloCamera(),
            "Electron Counting - Dose Fractionation Number of Frames", 
           
            DE_PROP_AUTOMOVIE"Sum Count"), sumCount);
        else
          ret4 = justSetIntProperty(DE_PROP_AUTOSUM"Sum Count", sumCount);
        if (ret4)
          mLastSumCount = sumCount;
    }

    if (setSaves || ((saveRaw || saveSums || saveFinal) && suffix != mLastSuffix)) {
      ret5 = mDeServer->setProperty(mAPI2Server ? 
        "Autosave Filename Suffix" : "Autosave Filename Suffix (User Input)",
        (LPCTSTR)suffix);
      if (ret5)
        mLastSuffix = suffix;
    }
    if (!saveDir.IsEmpty() && (setSaves || ((saveRaw || saveSums || saveFinal) && 
      saveDir != mLastSaveDir))) {
        ret6 = mDeServer->setProperty(DE_PROP_AUTOSAVE_DIR, (LPCTSTR)saveDir);
        if (ret6)
          mLastSaveDir = saveDir;
    }
    if (!ret1 || !ret2 || !ret3 || !ret4 || !ret5 || !ret6 || !ret7) {
      SEMMessageBox("An error occurred trying to set frame and sum saving to current "
        "selections");
      return 1;
    }
    mLastSaveFlags = autoSaveFlags;
    return 0;
  }
  SEMMessageBox("Failed to obtain mutex for setting frame and sum saving properties");
  return 1;
}

///////////////////////////////////////////////////////////////////
//setPreExposureTime() routine.  This function sets the
//proper preexposure time that will be used with the Direct Electron
//beamblanker box.
//
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::setPreExposureTime(double preExpMillisecs)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if ((fabs(mLastPreExposure - preExpMillisecs) > 0.01 || !mTrustLastSettings) && 
      !mLiveThread) {
      if (!justSetDoubleProperty(mAPI2Server ? "Pre-Exposure Time (seconds)" :
        "Preexposure Time (seconds)", preExpMillisecs / 1000.))
      {
        mLastErrorString = ErrorTrace("ERROR: Could NOT set the pre-exposure time"
          " to %.3f", preExpMillisecs / 1000.);
        return 1;
      }
      SEMTrace('D', "Pre-exposure time set to:  %.3f", preExpMillisecs / 1000.);
      mLastPreExposure = preExpMillisecs;
    }

    return 0;
  }
  return 1;
}

///////////////////////////////////////////////////////////////////
//AcquireImageData() routine.  Set up and start the acquisition, then copy the data from
// from what was acquired from the camera into an appropriately sized array for
//SerialEM to use for its acquisition process. 
//
///////////////////////////////////////////////////////////////////

int DirectElectronCamera::AcquireImageData(unsigned short *image4k, long &imageSizeX,
  long &imageSizeY, int divideBy2)
{
  CString valStr;
  int actualSizeX, actualSizeY;
  double startTime = GetTickCount();
  bool api2Reference = mAPI2Server && mRepeatForServerRef > 0;
  bool imageOK;
  if (!m_DE_CLIENT_SERVER && m_STOPPING_ACQUISITION == true) {
    memset(image4k, 0, imageSizeX * imageSizeY * 2);
    m_STOPPING_ACQUISITION = false;
    return -1;
  }

  m_STOPPING_ACQUISITION = false;

  // Need to take the negative of the rotation because of Y inversion, 90 with the CImg
    // library was producing clockwise rotation of image
  int operation = OperationForRotateFlip(m_DE_ImageRot, m_DE_ImageInvertX);
  int inSizeX, inSizeY, outX, outY;
  inSizeX = imageSizeX;
  inSizeY = imageSizeY;

  // CONTINUOUS (LIVE) MODE STARTUP OR ACQUISITION
  if (mLastLiveMode > 1) {
    mLiveTD.frameType = (int)DE::FrameType::SUMTOTAL;
    return SetupLiveAndGetImage(image4k, inSizeX, inSizeY, divideBy2, operation);
  }

  // Resume code for NORMAL SINGLE IMAGE ACQUISITION

  // Get a checksum of current conditions and time stamp
  int checksum = B3DNINT(1000 * mLastExposureTime) + B3DNINT(1005 * mLastPreExposure) +
    B3DNINT(105. * mLastFPS) + 17 * mLastXbinning + 23 * mLastXoffset +
    29 * mLastYoffset + 31 * mLastROIsizeX + 37 * mLastROIsizeY + 41 * mLastXimageSize +
    43 * mLastYimageSize + 47 * mLastElectronCounting + 53 * mLastUseHardwareBin +
    59 * mLastSuperResolution + 61 * mLastUseHardwareROI;
  int minuteNow = mWinApp->MinuteTimeStamp();

  // Check exposure time and FPS if needed
  if (!CheckMapForValidState(checksum, mExpFPSchecks, minuteNow)) {
    float maxFPS = mLastFPS, maxExp = mLastExposureTime;
    if (getFloatProperty("Frames Per Second (Max)", maxFPS))
      mCamParams[mCurCamIndex].DE_MaxFrameRate = maxFPS;
    getFloatProperty("Exposure Time Max (seconds)", maxExp);
    if (mLastExposureTime > maxExp) {
      mLastErrorString.Format("The exposure time of %.3f exceeds the maximum allowed "
        "under current conditions (%.3f)", mLastExposureTime, maxExp);
      SEMTrace('D', "%s", (LPCTSTR)mLastErrorString);
      return 1;
    }
    if (mLastFPS > maxFPS + 1) 
      SEMTrace('0', "WARNING: The specified frames/sec of %.2f has been truncated"
        " to %.2f, the maximum allowed under current conditions", mLastFPS, maxFPS);

    AddValidStateToMap(checksum, mExpFPSchecks, minuteNow);
  }

  // Check dark reference if any processing or counting mode and older server
  if ((mLastProcessing != UNPROCESSED || 
    (mLastElectronCounting > 0 && mServerVersion < DE_NO_REF_IF_UNNORMED)) &&
    !IsReferenceValid(checksum, mDarkChecks, minuteNow, mAPI2Server ?
      "Reference - Dark" : "Correction Mode Dark Reference Status", "dark"))
    return 1;

  // Check gain reference if normalized or counting mode and older server
  if ((mLastProcessing == GAIN_NORMALIZED ||
    (mLastElectronCounting > 0 && mServerVersion < DE_NO_REF_IF_UNNORMED)) &&
    !IsReferenceValid(checksum, mGainChecks, minuteNow, mAPI2Server ?
      "Reference - Gain" : "Correction Mode Gain Reference Status", "gain"))
    return 1;

  // Check counting gain reference if normalized and counting mode
  if (mServerVersion < DE_HAS_ONE_GAIN_STATUS &&
    (mLastNormCounting > 0 && mNumLeftServerRef <= 0 && mLastElectronCounting > 0) &&
    !IsReferenceValid(checksum, mCountGainChecks, minuteNow,
      "Correction Mode Counting Gain Reference Status", "post-counting gain"))
    return 1;

  unsigned short *useBuf = image4k;
  if (operation && !api2Reference) {
    NewArray2(useBuf, unsigned short, imageSizeX, imageSizeY);
    if (!useBuf) {
      SetAndTraceErrorString("Failed to get memory for rotation/flip of DE image");
      return 1;
    }
  }

  SEMTrace('D', "Getting image from DE server now that all preconditions are set.");

  // THIS IS THE ACTUAL IMAGE ACQUISITION AT LAST
  if (sUsingAPI2) {
    if (!mDeServer->StartAcquisition(api2Reference ? mNumLeftServerRef : 1)) {
      mLastErrorString = ErrorTrace("ERROR: Could not start %s acquisition",
        api2Reference ? "reference" : "image");
      mDateInPrevSetName = 0;
      return 1;
    }
    if (!api2Reference) {
      DE::ImageAttributes attributes;
      DE::PixelFormat pixForm = DE::PixelFormat::UINT16;
      attributes.stretchType = DE::ContrastStretchType::NONE;
      imageOK = mDeServer->GetResult(useBuf, imageSizeX * imageSizeY * 2, 
        DE::FrameType::SUMTOTAL, &pixForm, &attributes);
    }
  } else {
    imageOK = mDeServer->getImage(useBuf, imageSizeX * imageSizeY * 2);
  }
  if (!api2Reference && !imageOK) {
    mLastErrorString = ErrorTrace("ERROR: Could NOT get the image from DE server");
    if (operation)
      delete useBuf;
    mDateInPrevSetName = 0;
    return 1;
  }
  SEMTrace('D', api2Reference ? " Started reference acquisitions" :
    "Got something back from DE Server..");

  // Try to read back the actual image size from these READ-ONLY properties and if it 
  // is different, truncate the Y size if necessary and set the return size from actual
  if (!api2Reference && mDeServer->getIntProperty(mServerVersion >= DE_HAS_API2 ? 
    "Image Size X (pixels)" : g_Property_DE_ImageWidth, &actualSizeX)
    && mDeServer->getIntProperty(mServerVersion >= DE_HAS_API2 ?
      "Image Size Y (pixels)" : g_Property_DE_ImageHeight, &actualSizeY) &&
    (actualSizeX != imageSizeX || actualSizeY != imageSizeY)) {
    if (actualSizeX * actualSizeY > imageSizeX * imageSizeY)
      actualSizeY = (imageSizeX * imageSizeY) / actualSizeX;
    imageSizeX = actualSizeX;
    imageSizeY = actualSizeY;
    inSizeX = imageSizeX;
    inSizeY = imageSizeY;
  }

  // If doing ref in server, loop until all the acquisitions are done
  double maxInterval = 2. * SEMTickInterval(startTime);
  maxInterval = B3DMAX(maxInterval, 15000. * mLastExposureTime);
  while (mNumLeftServerRef > 0 && !m_STOPPING_ACQUISITION) {
    int remaining;
    bool ret1;
    startTime = GetTickCount();
    mDateInPrevSetName = 0;
    while (SEMTickInterval(startTime) < maxInterval && !m_STOPPING_ACQUISITION) {
      ret1 = mDeServer->getIntProperty(api2Reference ?
        "Remaining Number of Acquisitions" :
        "Auto Repeat Reference - Remaining Acquisitions", &remaining);
      if (ret1 && remaining != mNumLeftServerRef)
        break;
      Sleep(100);
    }
    if (m_STOPPING_ACQUISITION)
      break;
    if (remaining != mNumLeftServerRef)
      mNumLeftServerRef = remaining;
    else {
      mLastErrorString = ret1 ? "Time out getting server reference; remaining "
        "acquisitions did not drop fast enough" : "Error getting remaining acquisitions"
        " when getting server reference";
      SEMTrace('D', "%s", (LPCTSTR)mLastErrorString);
      if (operation)
        delete useBuf;
      return 1;
    }
  }

  // This will now be cleared when STOP button is disabled
  mTrustLastSettings = true;

  if (api2Reference) {
    memset(useBuf, 0, imageSizeX * imageSizeY * 2);
    return 0;
  }

  // Do the rotation/flip, free array, divide by 2 if needed or scale counting image
  if (operation) {
    ProcRotateFlip((short *)useBuf, kUSHORT, inSizeX, inSizeY, operation, 0,
      (short *)image4k, &outX, &outY);
    delete useBuf;
  }
  if (mLastElectronCounting > 0 && mCountScaling != 1.) {
    float scale = mCountScaling * (divideBy2 ? 0.5f : 1.f);
    int val, maxVal = divideBy2 ? 32767 : 65535;
    if (mLastNormCounting && mElecCountsScaled > 0)
      scale /= mElecCountsScaled;
    SEMTrace('D', "mcs %f mlastnorm %d melecsc %d  scale %f", mCountScaling,
      mLastNormCounting ? 1 : 0, mElecCountsScaled, scale);
    for (int i = 0; i < imageSizeX * imageSizeY; i++) {
      val = (int)(scale * (float)image4k[i] + 0.5f);
      B3DCLAMP(val, 0, maxVal);
      image4k[i] = val;
    }

  } else if (divideBy2) {
    for (int i = 0; i < imageSizeX * imageSizeY; i++)
      image4k[i] = image4k[i] >> 1;
  }
  return 0;
}

// Common operations for starting a live or continuous STEM mode and getting an image
// from the buffer once it is started
int DirectElectronCamera::SetupLiveAndGetImage(unsigned short *image4k, int inSizeX, 
  int inSizeY, int divideBy2, int operation)
{
  int outX, status;
  bool startedThread = false;
  if (mLiveTD.outBufInd == -1) {

    // Setup the thread data and allocate buffers
    mLiveTD.DeServer = mDeServer;
    mLiveTD.operation = operation;
    mLiveTD.divideBy2 = divideBy2;
    mLiveTD.inSizeX = inSizeX;
    mLiveTD.inSizeY = inSizeY;
    mLiveTD.quitLiveMode = false;
    mLiveTD.electronCounting = mLastElectronCounting;
    mLiveTD.returnedImage[0] = mLiveTD.returnedImage[1] = false;
    NewArray2(mLiveTD.buffers[0], unsigned short, inSizeX, inSizeY);
    NewArray2(mLiveTD.buffers[1], unsigned short, inSizeX, inSizeY);
    if (operation)
      NewArray2(mLiveTD.rotBuf, unsigned short, inSizeX, inSizeY);
    if (!mLiveTD.buffers[0] || !mLiveTD.buffers[1] || (operation && !mLiveTD.rotBuf)) {
      CleanupLiveMode(&mLiveTD);
      SEMTrace('D', "Failed to get memory for live mode buffers");
      return 1;
    }
    mLiveTD.errString = "";

    // Start the thread as usual
    mLiveThread = AfxBeginThread(LiveProc, &mLiveTD, THREAD_PRIORITY_NORMAL,
      0, CREATE_SUSPENDED);
    mLiveThread->m_bAutoDelete = false;
    mLiveThread->ResumeThread();

    // Loop until it has gotten an image: 
    for (outX = 0; outX < 100; outX++) {
      Sleep(100);
      if (mLiveTD.outBufInd != -1)
        break;
    }
    startedThread = true;
  }

  // Clean up if it didn't start correctly; or check status and error out
  // if there is a problem
  if (mLiveTD.outBufInd < 0) {
    if (UtilThreadBusy(&mLiveThread) > 0)
      UtilThreadCleanup(&mLiveThread);
    status = -1;
  } else
    status = UtilThreadBusy(&mLiveThread);
  if (status <= 0) {
    if (mLiveTD.outBufInd == -1) {
      SetAndTraceErrorString("Live mode did not get started in timely manner");
    } else if (status < 0) {
      mLastErrorString = "ERROR: Live mode ended with an error:\r\n" +
        mLiveTD.errString;
    } else if (startedThread) {
      SetAndTraceErrorString("Live mode ended without error right after being "
        "started");
    } else {
      SetAndTraceErrorString("Live mode flag is set after thread was told to quit");
    }
    CleanupLiveMode(&mLiveTD);
    return 1;
  }

  // Loop until the output buffer is new; let SEM kill this thread if it times out?
  for (;;) {
    WaitForSingleObject(sLiveMutex, LIVE_MUTEX_WAIT);
    if (mLiveTD.outBufInd < 0) {
      ReleaseMutex(sLiveMutex);
      mLastErrorString = "ERROR: Live mode ended";
      if (!mLiveTD.errString.IsEmpty())
        mLastErrorString += " with an error:\r\n" + mLiveTD.errString;
      CleanupLiveMode(&mLiveTD);
      return 1;
    }
    if (!mLiveTD.returnedImage[mLiveTD.outBufInd]) {
      SEMTrace('D', "Returning new buffer %d", mLiveTD.outBufInd);
      memcpy(image4k, mLiveTD.buffers[mLiveTD.outBufInd], 2 * inSizeX * inSizeY);
      mLiveTD.returnedImage[mLiveTD.outBufInd] = true;
      ReleaseMutex(sLiveMutex);
      return 0;
    }
    ReleaseMutex(sLiveMutex);
    Sleep(10);
  }
  return 0;
}

/*
 * Thread procedure for live mode
 */
UINT DirectElectronCamera::LiveProc(LPVOID pParam)
{
  LiveThreadData *td = (LiveThreadData *)pParam;
  int newInd, acqIndex, outX, outY, retval = 0;
  int numAcquis = 1000;
  unsigned short *useBuf, *image4k;
  std::string stdStr;
  CString str2;
  BOOL needStart = sUsingAPI2, imageOK;

  while (!td->quitLiveMode) {

    if (needStart)
      acqIndex = -1;
    if (needStart && !td->DeServer->StartAcquisition(numAcquis)) {
      str2 = "StartAcquisition failed";
      retval = 1;
      break;
    }

    // Get the new index and buffer to place data into
    newInd = B3DCHOICE(td->outBufInd < 0, 0, 1 - td->outBufInd);
    image4k = td->buffers[newInd];
    useBuf = td->operation ? td->rotBuf : image4k;
    
    // Get image
    double start = GetTickCount();
    if (sUsingAPI2) {
      DE::ImageAttributes attributes;
      DE::PixelFormat pixForm = DE::PixelFormat::UINT16;
      attributes.stretchType = DE::ContrastStretchType::NONE;
      for (;;) {
        imageOK = td->DeServer->GetResult(useBuf, td->inSizeX * td->inSizeY * 2,
          (DE::FrameType)td->frameType, &pixForm, &attributes);
        if (!imageOK) {
          str2 = "GetResult failed";
          break;
        } else if (acqIndex != attributes.acqIndex || attributes.acqFinished) {
          break;
        }
        Sleep(10);
      }
      needStart = attributes.acqIndex == numAcquis - 1 || attributes.acqFinished;
      acqIndex = attributes.acqIndex;
    } else {
      imageOK = td->DeServer->getImage(useBuf, td->inSizeX * td->inSizeY * 2);
      if (!imageOK) {
        str2 = "getImage failed";
      }
    }
    if (!imageOK) {
      retval = 1;
      break;
    }
    double getTime = SEMTickInterval(start);
    start = GetTickCount();

    // Process it if needed
    if (td->operation)
      ProcRotateFlip((short *)useBuf, kUSHORT, td->inSizeX, td->inSizeY, td->operation, 0, 
        (short *)image4k, &outX, &outY);
    if (td->divideBy2)
      for (int i = 0; i < td->inSizeX * td->inSizeY; i++)
        image4k[i] = image4k[i] >> 1;
    double procTime = SEMTickInterval(start);
    start = GetTickCount();
    // Get the mutex and place change the output buffer index, mark as not returned
    WaitForSingleObject(sLiveMutex, LIVE_MUTEX_WAIT);
    td->outBufInd = newInd;
    td->returnedImage[newInd] = false;
    ReleaseMutex(sLiveMutex);
    SEMTrace('D', "Got frame index %d: %.0f acquire, %.0f process, %.0f for mutex", 
      acqIndex, getTime, procTime, SEMTickInterval(start));
    Sleep(10);
  }
  if (retval) {
    td->DeServer->getLastErrorDescription(&stdStr);
    td->errString.Format("%s, error code %d %s %s", (LPCTSTR)str2,
      td->DeServer->getLastErrorCode(),
      stdStr.c_str() != NULL ? ": " : "", stdStr.c_str() != NULL ? stdStr.c_str() : "");
    SEMTrace('D', "%s", td->errString);
  }

  // Shut down and clean up completely when done
  WaitForSingleObject(sLiveMutex, LIVE_MUTEX_WAIT);
  CleanupLiveMode(td);
  ReleaseMutex(sLiveMutex);
  return retval;
}

// Common place to turn off live mode and clear out the thread data
static void CleanupLiveMode(LiveThreadData *td)
{
  if (sUsingAPI2)
    td->DeServer->abortAcquisition();
  else
    td->DeServer->setLiveMode(false);
  td->outBufInd = -2;
  for (int outX = 0; outX < 2; outX++) {
    delete td->buffers[0];
    td->buffers[0] = NULL;
  }
  delete td->rotBuf;
  td->rotBuf = NULL;
}


///////////////////////////////////////////////////////////////////
//setBinning() routine, used to set the appropriate binning and binned image size
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::setBinning(int x, int y, int sizex, int sizey, int hardwareBin)
{
  bool useOld = true;
  int retVal;
  CSingleLock slock(&m_mutex);

  // Software binning is now additive to the hardware binning, so needs to be less
  int lessBin = hardwareBin > 0 && mServerVersion >= DE_AUTOSAVE_RENAMES2 ? 2 : 1;

  if (slock.Lock(1000)) {

    if (sUsingAPI2 > 1 && hardwareBin >= 0) {
      if ((hardwareBin != mLastUseHardwareBin || x != mLastXbinning ||
        y != mLastYbinning || !mTrustLastSettings) && !mLiveThread) {
        retVal = mDeServer->SetBinning(x, y, hardwareBin);
        if (retVal > 0) {
          mLastErrorString = ErrorTrace("ERROR calling SetBinning with %d %d %d",
            x, y, hardwareBin);
          return 1;
        } else if (!retVal) {
          SEMTrace('D', "Called SetBinning with %d %d %d", x, y, hardwareBin);
          useOld = false;
        }
      } else {
        return 0;
      }
    }

    // Set hardware binning first and if it has changed, force setting new binning
    if (useOld && hardwareBin >= 0 && (hardwareBin != mLastUseHardwareBin || 
      !mTrustLastSettings) && !mLiveThread) {
      SEMTrace('D', "SetBinning set hw bin %d", hardwareBin);
      if (mServerVersion < DE_HAS_API2) {
        if (!setStringWithError(psHardwareBin, hardwareBin > 0 ? psEnable : psDisable))
          return 1;
      } else if (mServerVersion < DE_AUTOSAVE_RENAMES2) {
        if (!setStringWithError(psBinMode, hardwareBin > 0 ? psHWandSW : psSWonly))
          return 1;
      } else {
        if (!justSetIntProperty("Hardware Binning X", hardwareBin > 0 ? 2 : 1) ||
          !justSetIntProperty("Hardware Binning Y", hardwareBin > 0 ? 2 : 1)) {
          mLastErrorString = ErrorTrace("ERROR: Could NOT set the hardware binning");
          return 1;
        }
        mLastROIsizeX = mLastXoffset = -1;
      }
      mLastXbinning = -1;
    }
    mLastUseHardwareBin = hardwareBin;

    // set binning if it does not match last value
    if (useOld && (x != mLastXbinning || y != mLastYbinning || !mTrustLastSettings) && 
      !mLiveThread) {
      if (!justSetIntProperty(g_Property_DE_BinningX, x / lessBin) ||
        !justSetIntProperty(g_Property_DE_BinningY, y / lessBin)) {
        mLastErrorString = ErrorTrace("ERROR: Could NOT set the software binning "
          "parameters of X: %d and Y: %d", x / lessBin, y / lessBin);
        return 1;
      } else {
        m_binFactor = x;
        SEMTrace('D', "Software binning factors now set to X: %d Y: %d x %d lb %d", 
          x / lessBin, y / lessBin, x, lessBin);
      }
    }

    // Save last values regardless
    mLastXbinning = x;
    mLastYbinning = y;

    // DO NOT set size, those properties are read-only

    mLastXimageSize = sizex;
    mLastYimageSize = sizey;

    return 0;
  }
  return 1;
}

// Set the parameters related to counting/super-resolution, including frames per second
// Readmode can be negative to set FPS only
int DirectElectronCamera::SetCountingParams(int readMode, double scaling, double FPS)
{
  CSingleLock slock(&m_mutex);
  CameraParameters *camP = mCamParams + mCurCamIndex;
  bool superRes = readMode == SUPERRES_MODE;
  bool hasHDR = (camP->CamFlags & DE_HAS_HARDWARE_HDR) != 0;
  mCountScaling = (float)scaling;
  if (slock.Lock(1000)) {
    if (!IsApolloCamera() && (camP->CamFlags & DE_CAM_CAN_COUNT)) {
      if (((readMode == LINEAR_MODE && mLastElectronCounting != 0) ||
        (readMode > 0 && mLastElectronCounting <= 0) || 
        (hasHDR && !BOOL_EQUIV(superRes, mLastSuperResolution)) || !mTrustLastSettings) &&
        !mLiveThread) {
        if (mAPI2Server) {
          if (!setStringWithError("Image Processing - Mode", B3DCHOICE(readMode > 0,
            (hasHDR && superRes) ? "HDR Counting" : "Counting", "Integrating")))
            return 1;
        } else {
          if (!setStringWithError(DE_PROP_COUNTING, readMode > 0 ? psEnable : psDisable))
            return 1;
        }
        mLastElectronCounting = readMode > 0 ? 1 : 0;
        if (hasHDR)
          mLastSuperResolution = superRes ? 1 : 0;
      }
      if (((readMode == COUNTING_MODE && mLastSuperResolution != 0) ||
        (superRes && mLastSuperResolution <= 0) || !mTrustLastSettings) && !mLiveThread &&
        (camP->CamFlags & DE_CAN_SAVE_SUPERRES)) {
        if (!setStringWithError(mAPI2Server ? "Event Counting - Super Resolution" :
          DE_PROP_COUNTING" - Super Resolution", superRes ? psEnable : psDisable))
          return 1;
        mLastSuperResolution = superRes ? 1 : 0;
      }
    }
    if (!IsApolloCamera() && (fabs(FPS - mLastFPS) > 1.e-3 || !mTrustLastSettings) &&
      !mLiveThread && SetFramesPerSecond(FPS))
      return 1;
  }
  return 0;
}

int DirectElectronCamera::SetAlignInServer(int alignFrames)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if ((alignFrames != mLastServerAlign || !mTrustLastSettings) && !mLiveThread) {
      if (!setStringWithError(
        mServerVersion >= DE_HAS_NEW_MOT_COR_PROP ? psMotionCorNew : psMotionCor, 
        alignFrames > 0 ? psEnable : psDisable))
        return 1;
      mLastServerAlign = alignFrames;
    }
  }
  return 0;
}


// Set size and offset in unbinned coordinates as returned by the server; the caller
// now takes care of setting these correctly from the final user's coordinates
int DirectElectronCamera::setROI(int offset_x, int offset_y, int xsize, int ysize,
  int hardwareROI)
{
  CSingleLock slock(&m_mutex);
  bool newProp = mServerVersion >= DE_AUTOSAVE_RENAMES2;
  CameraParameters *camP = mCamParams + mCurCamIndex;
  if (slock.Lock(1000)) {

    bool needOffset, needSize, ret1 = true, ret2 = true, ret3 = true, needOld = true;
    int hwXoff, hwYoff, hwXsize, hwYsize, swXoff, swYoff, swXsize, swYsize, xbin, ybin;
    int retVal;

    // Set hardware ROI first and if it has changed, force setting new ROI
    needOffset = offset_x != mLastXoffset || offset_y != mLastYoffset;
    needSize = xsize != mLastROIsizeX || ysize != mLastROIsizeY;
    if (sUsingAPI2 > 1 && (hardwareROI != mLastUseHardwareROI || (needSize || needOffset)
      || !mTrustLastSettings) && !mLiveThread) {
      retVal = mDeServer->SetROI(offset_x, offset_y, xsize, ysize, hardwareROI);
      if (retVal > 0) {
        mLastErrorString = ErrorTrace("ERROR calling SetROI with %d %d %d %d %d",
          offset_x, offset_y, xsize, ysize, hardwareROI);
        return 1;
      } else if (!retVal) {
        needOld = false;
        mLastUseHardwareROI = hardwareROI;
        SEMTrace('D', "Called SetROI with %d %d %d %d %d",
          offset_x, offset_y, xsize, ysize, hardwareROI);
      }
    }

    if (needOld) {

      // Old logic to set hardware ROI if it exists, then software
      if (hardwareROI >= 0 && (hardwareROI != mLastUseHardwareROI ||
        (((newProp && hardwareROI > 0) || sUsingAPI2) && (needSize || needOffset)) ||
        !mTrustLastSettings) && !mLiveThread) {

        if (mServerVersion < DE_HAS_API2) {
          if (!setStringWithError(psHardwareROI, hardwareROI > 0 ? psEnable : psDisable))
            return 1;
        } else if (!newProp) {
          if (!setStringWithError(psROIMode, hardwareROI > 0 ? psHWandSW : psSWonly))
            return 1;
        } else {

          // New API: follow same procedure of setting 0 offset, then size, then offset
          if (hardwareROI != mLastUseHardwareROI || (needSize && needOffset)) {
            ret3 = justSetIntProperty("Hardware ROI Offset X", 0)
              && justSetIntProperty("Hardware ROI Offset Y", 0);
            SEMTrace('D', "Hardware ROI offset set to 0 before setting size and offset,"
              " ret: %d", ret3 ? 1 : 0);
          }

          // It is either the specified ROI if hardware, or the full size if not
          hwXoff = hardwareROI > 0 ? offset_x : 0;
          hwYoff = hardwareROI > 0 ? offset_y : 0;
          hwXsize = hardwareROI > 0 ? xsize : camP->sizeX;
          hwYsize = hardwareROI > 0 ? ysize : camP->sizeY;
          if (hardwareROI != mLastUseHardwareROI || needSize) {
            ret2 = justSetIntProperty("Hardware ROI Size X",
              hwXsize) && justSetIntProperty("Hardware ROI Size Y", hwYsize);
            SEMTrace('D', "Hardware ROI settings: xsize: %d ysize: %d  ret: %d", hwXsize,
              hwYsize, ret2 ? 1 : 0);
          }
          if (hardwareROI != mLastUseHardwareROI || needOffset) {
            ret1 = justSetIntProperty("Hardware ROI Offset X",
              hwXoff) && justSetIntProperty("Hardware ROI Offset Y", hwYoff);
            SEMTrace('D', "Hardware ROI settings: xsize: %d ysize: %d  ret: %d", hwXoff,
              hwYoff, ret1 ? 1 : 0);
          }
          if (!ret1 || !ret2 || !ret3) {
            mLastErrorString.Format("Error setting hardware offset and size "
              "(%d - %d - %d)", ret1, ret2, ret3);
            mLastXoffset = mLastROIsizeX = -1;
            return 1;
          }

        }
        mLastXoffset = mLastROIsizeX = -1;
        mLastUseHardwareROI = hardwareROI;
      }

      // Now do software ROI that depends on hardware ROI and binning in new API
      // EValuate what needs setting; of both need to be set, it may be best practice to
      // set to offset to 0 first so the new size is always valid
      needOffset = (offset_x != mLastXoffset || offset_y != mLastYoffset ||
        !mTrustLastSettings) && !mLiveThread;
      needSize = (xsize != mLastROIsizeX || ysize != mLastROIsizeY ||
        !mTrustLastSettings) && !mLiveThread;
      if (needOffset && needSize) {
        ret3 = justSetIntProperty(newProp ? "Crop Offset X" : g_Property_DE_RoiOffsetX,
          0)
          && justSetIntProperty(newProp ? "Crop Offset Y" : g_Property_DE_RoiOffsetY, 0);
        SEMTrace('D', "ROI offset set to 0 before setting size and offset, ret: %d",
          ret3 ? 1 : 0);
      }

      // Set to passed ROI or full subarea of HW ROI, but adjust for hardware binning
      xbin = (newProp && mLastUseHardwareBin > 0) ? 2 : 1;
      ybin = (newProp && mLastUseHardwareBin > 0) ? 2 : 1;
      swXoff = (newProp && hardwareROI > 0) ? 0 : offset_x / xbin;
      swYoff = (newProp && hardwareROI > 0) ? 0 : offset_y / ybin;
      swXsize = xsize / xbin;
      swYsize = ysize / ybin;

      // Set size
      if (needSize) {
        ret2 = justSetIntProperty(newProp ? "Crop Size X" : g_Property_DE_RoiDimensionX,
          swXsize) && justSetIntProperty(newProp ? "Crop Size Y" :
            g_Property_DE_RoiDimensionY, swYsize);
        SEMTrace('D', "ROI settings: xsize: %d ysize: %d  ret: %d", swXsize, swYsize,
          ret2 ? 1 : 0);
      }

      //Set offset
      if (needOffset) {
        ret1 = justSetIntProperty(newProp ? "Crop Offset X" : g_Property_DE_RoiOffsetX,
          swXoff) && justSetIntProperty(newProp ?
            "Crop Offset Y" : g_Property_DE_RoiOffsetY, swYoff);
        SEMTrace('D', "ROI settings: offsetX: %d offsetY: %d  ret: %d", swXoff,
          swYoff, ret1 ? 1 : 0);
      }
      if (!ret1 || !ret2 || !ret3) {
        mLastErrorString.Format("Error setting offset and size for region of interest "
          "(%d - %d - %d)", ret1, ret2, ret3);
        mLastXoffset = mLastROIsizeX = -1;
        return 1;
      }
    }
    mLastXoffset = offset_x;
    mLastYoffset = offset_y;
    mLastROIsizeX = xsize;
    mLastROIsizeY = ysize;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////
//SetImageExposureAndMode() routine. 
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::SetImageExposureAndMode(float seconds)
{
  CSingleLock slock(&m_mutex);
  int mode;

  if (slock.Lock(1000)) {

    if (mRepeatForServerRef > 0)
      mode = mModeForServerRef;
    else
      mode = mWinApp->mGainRefMaker->GetTakingRefImages() ? DE_GAIN_IMAGE :
      DE_NORMAL_IMAGE;
    if (SetExposureTimeAndMode(seconds, mode))
      return 1;

  }
  return 0;
}


///////////////////////////////////////////////////////////////////
//SetDarkExposureAndMode() routine.  
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::SetDarkExposureAndMode(float seconds)
{

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (SetExposureTimeAndMode(seconds, DE_DARK_IMAGE))
      return 1;

  }
  return 0;
}

// Common function to set exposure time and mode for either a normal or dark image
int DirectElectronCamera::SetExposureTimeAndMode(float seconds, int mode)
{
  const char *modeName[3] = {"Normal", "Dark", "Gain"};
  if (mode < 0 || mode > 2) {
    SEMTrace('D', "SetExposureTimeAndMode called with mode out of range");
    return 1;
  }

  if ((fabs((double)(seconds - mLastExposureTime)) > 1.e-4 || !mTrustLastSettings) &&
    !mLiveThread) {
    if (!justSetDoubleProperty(g_Property_DE_ExposureTime, seconds)) {
      mLastErrorString = ErrorTrace("ERROR: Could NOT set the exposure time to %.3f",
        seconds);
      return 1;
    }
    SEMTrace('D', "Exposure time set to %.4f", seconds);
  }
  mLastExposureTime = seconds;

  if ((mLastExposureMode != mode || !mTrustLastSettings) && !mLiveThread) {
    if (!setStringWithError("Exposure Mode", modeName[mode])) 
      return 1;
    SEMTrace('D', "Exposure mode set to %s", modeName[mode]);
  }
  mLastExposureMode = mode;

  if (mLastAutoRepeatRef >= 0 && (mLastAutoRepeatRef != (mRepeatForServerRef > 0 ? 1 : 0)
    || !mTrustLastSettings) && !mLiveThread) {
      if (!setStringWithError(psAutoRepeatRef, mRepeatForServerRef > 0 ? 
        psEnable : psDisable))
          return 1;
      mLastAutoRepeatRef = mRepeatForServerRef > 0 ? 1 : 0;
  }

  if (mRepeatForServerRef > 0 && !mAPI2Server) {
    if (!justSetIntProperty("Auto Repeat Reference - Total Number of Acquisitions", 
      mRepeatForServerRef)) {
        mLastErrorString = ErrorTrace("ERROR: Could NOT set the auto repeat # of acquires"
          " to %d", mRepeatForServerRef);
        return 1;
    }
  }
  return 0;
}

// Toggle the live mode: 0 is off, 1 is on with no thread, 2 is with thread
// This would work for STEM because there is no actual live call in API2
int DirectElectronCamera::SetLiveMode(int mode, bool skipLock)
{
  CSingleLock slock(&m_mutex);
  int status, ind;

  // If the mutex failed (?!), just drop back to no thread
  if (mode > 1 && !sLiveMutex)
    mode = 1;

  if (skipLock || slock.Lock(1000)) {
    if (!sUsingAPI2 && (mLastLiveMode < 0 || !mTrustLastSettings) && 
      mDeServer->getIsInLiveMode)
      mLastLiveMode = mDeServer->getIsInLiveMode() ? 1 : 0;
    if (mode != mLastLiveMode) {
      if (mLastLiveMode > 1) {

        // Turning off thread, make sure it is running and tell it to quit, then wait
        status = UtilThreadBusy(&mLiveThread);
        if (status > 0) {
          mLiveTD.quitLiveMode = true;
          for (ind = 0; ind < 100; ind++) {
            Sleep(100);
            status = UtilThreadBusy(&mLiveThread);
            if (status <= 0)
              break;
          }
          if (UtilThreadBusy(&mLiveThread) > 0) {
            UtilThreadCleanup(&mLiveThread);
            CleanupLiveMode(&mLiveTD);
          }
        }
      } 
      if (mode > 1) {

        // Turning on thread live mode, set index, let the shot take care of the rest
        mLiveTD.outBufInd = -1;
      }

      // Set the new mode.
      if (!sUsingAPI2) {
        if (!mDeServer->setLiveMode(mode > 0)) {
          mLastErrorString = ErrorTrace("ERROR: Could NOT set the live mode %s.",
            mode ? "ON" : "OFF");
          return 1;
        }
      }
      SEMTrace('D', "Live mode turned %s (%d)", mode ? "ON" : "OFF", mode);
      mLastLiveMode = mode;
      mWinApp->mDEToolDlg.Update();
    }
    return 0;
  }
  return 1;
}



///////////////////////////////////////////////////////////////////
//StopAcquisition() routine.  Function will stop the camera
//from continuing the acquistion.  This will also clear out the
//the acquistion frame for future acquires.
///////////////////////////////////////////////////////////////////
void DirectElectronCamera::StopAcquisition()
{
  if (mServerVersion >= DE_ABORT_WORKS || sUsingAPI2) {
    mDeServer->abortAcquisition();
    m_STOPPING_ACQUISITION = true;
    if (mStartedScan) {
      RestoreVirtualShapes(false);
      mStartedScan = false;
    }
  } 
}

int DirectElectronCamera::setDebugMode()
{
  m_debug_mode = 1;
  return 1;
}

///////////////////////////////////////////////////////////////////
//getCameraTemp() routine.  Function will return the current
//Temperature of the camera in degrees Celcius.
///////////////////////////////////////////////////////////////////
float DirectElectronCamera::getCameraTemp()
{
  bool success;

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    float temp = 10;
    int itemp = 10;
    if (mServerVersion < 205390000) {
      success = mDeServer->getFloatProperty("Temperature - Detector (Celsius)", &temp);
    } else {
      success = mDeServer->getIntProperty("Temperature - Detector (Celsius)", &itemp);
      temp = (float)itemp;
    }
    if (!success)
      CString str = ErrorTrace("ERROR: Could NOT get the Temperature of DE camera ");
    //AfxMessageBox(str); 
    m_camera_Temperature = temp;

  }

  return m_camera_Temperature;
}


// Inserts camera and waits until done
int DirectElectronCamera::insertCamera()
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {

    if (!mDeServer->setProperty(mAPI2Server ? psCamPositionCtrl : psCamPosition,
      mAPI2Server ? "Extend" : DE_CAM_STATE_INSERTED)) {
      CString str = ErrorTrace("ERROR: Could NOT insert the DE camera ");
      //AfxMessageBox(str);
    } else {
      WaitForInsertionState(DE_CAM_STATE_INSERTED);
      return 0;

    }
  }
  return 1;
}

// Retracts camera and waits until done
int DirectElectronCamera::retractCamera()
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {

    if (!mDeServer->setProperty(mAPI2Server ? psCamPositionCtrl : psCamPosition,
      mAPI2Server ? "Retract" : DE_CAM_STATE_RETRACTED)) {
      CString str = ErrorTrace("ERROR: Could NOT retract the DE camera ");
      //AfxMessageBox(str);
    } else {
      WaitForInsertionState(DE_CAM_STATE_RETRACTED);
      return 0;
    }
  }
  return 1;
}

// Waits for the insertion state to reach the desired state
void DirectElectronCamera::WaitForInsertionState(const char *wantState)
{
  int num_tries = 0;
  int maxTime = 10000;
  int sleepTime = 200;
  CString state = getCameraInsertionState();
  while (num_tries < maxTime / sleepTime && state != CString(wantState)) {
    num_tries++;
    Sleep(sleepTime);
    state = getCameraInsertionState();
  }
}


// Unused
float DirectElectronCamera::getColdFingerTemp()
{
  float temp;
  CString str;
  if (!mDeServer->getFloatProperty("Temperature - Cold Finger (Celsius)", &temp)) {

    str = ErrorTrace("ERROR: Could NOT get the Cold Finger Temperature.");
    //AfxMessageBox(str);
  } else {

  }

  return temp;
}

// Returns the water line temperature
float DirectElectronCamera::getWaterLineTemp()
{
  float temp = 0.;
  CString str;

  if (!mDeServer->getFloatProperty("Temperature - Water Line (Celsius)", &temp)) {

    str.Format("Could NOT get the Water Line Temperature.");
    //AfxMessageBox(str);
    //SEMTrace('D', "ERROR: %s",str);
  } else {

  }

  return temp;
}

// Not used now
float DirectElectronCamera::getTECValue()
{
  float tec = 0.;
  CString str;
  if (!mDeServer->getFloatProperty("Temperature - TEC current (Ampere)", &tec)) {

    str.Format("Could NOT get the TEC Value.");
    //AfxMessageBox(str);
    //SEMTrace('D', "ERROR: %s",str);
  } else {

  }
  return tec;
}

// 2/15/17: Removed unused getFaradayPlateValue


// Initiate warming of the camera
HRESULT DirectElectronCamera::WarmUPCamera()
{

  HRESULT hr = S_OK;
  CString str;
  if (!mDeServer->setProperty(mAPI2Server ? 
    "Temperature - Control" : "Temperature Control", "Warm Up")) {

    str = ErrorTrace("ERROR: Could NOT WARM UP the camera.");
    AfxMessageBox(str);
    return S_FALSE;
  } else {
    str.Format("Warming up the camera....");
    AfxMessageBox(str);

  }
  return hr;
}

// Initiate cooling of the camera
HRESULT DirectElectronCamera::CoolDownCamera()
{

  HRESULT hr = S_OK;
  CString str;

  if (!mDeServer->setProperty(mAPI2Server ?
    "Temperature - Control" : "Temperature Control", "Cool Down")) {

    str = ErrorTrace("ERROR: Could NOT COOL DOWN the camera.");
    AfxMessageBox(str);
    return S_FALSE;
  } else {
    str.Format("Cooling down the camera. Please wait at least 5 minutes for the "
      "temperature to stabilize...");
    AfxMessageBox(str);

  }
  return hr;
}

// Set the correction mode with the given value, plus 4 to disable movie corrections
int DirectElectronCamera::setCorrectionMode(int nIndex, int readMode)
{
  const char *corrections[3] = {"Uncorrected Raw", "Dark Corrected", 
    "Gain and Dark Corrected"};
  const char *newCorrections[3] = {"None", "Dark", "Dark and Gain"};
  CString str;
  int normCount, normDoseFrac = 1, movieCorrEnable = 1;
  int bits = (readMode == SUPERRES_MODE) ? 8 : 16;
  int corrSet = -1;

  // This seems not to happen in camcontroller
  if (nIndex & 4) {
    movieCorrEnable = 0;
    nIndex -= 4;
  }
  if (nIndex < 0 || nIndex > 2)
    return 1;

  // For frames, set gain correction based on the selected correction in the new version
  // And require dark/gain references for unnorm counting modes
  if (mNormAllInServer) {
    normDoseFrac = nIndex / 2;
    if (readMode > 0) {

      // 6/11/25: do not change processing for counting mode, it should do Dark not
      // not Dark and Gain
      //nIndex = 2;  
      normCount = normDoseFrac;
      if (mRepeatForServerRef > 0)
        normCount = 0;
      normDoseFrac = 1;
    }
  }

  if (readMode > 0 && (normCount != mLastNormCounting || !mTrustLastSettings) && 
    !IsApolloCamera() && !mLiveThread) {
    // 6/11/25 All server versions should use post-counting gain. We set the post-counting
    // gain to a read-only property in the latest DE-MC. And SerialEM does not need to set
    // this property for all server versions.
    /* if (!setStringWithError(mAPI2Server ? 
      "Event Counting - Apply Post-Counting Gain" :
      DE_PROP_COUNTING" - Apply Post-Counting Gain", normCount ? psEnable : psDisable))
        return 1;*/
    mLastNormCounting = normCount;
  }

  // Set the output bits if they are not normalized
  if (mLastSaveFlags > 0 && mNormAllInServer && mServerVersion < DE_HAS_AUTO_PIXEL_DEPTH
    && (!normDoseFrac || nIndex < 2 || (readMode > 0 && !normCount)) && 
    (bits != mLastUnnormBits || !mTrustLastSettings) && !mLiveThread) {
      str.Format("%dbit", bits);
      if (!setStringWithError(DE_PROP_AUTOMOVIE"Format for Unnormalized Movie",
        (LPCTSTR)str))
        return 1;
      mLastUnnormBits = bits;
  }

  // Set the general correction mode
  if ((nIndex != mLastProcessing || !mTrustLastSettings) && !mLiveThread) {
    if (!mAPI2Server) {
      if (!setStringWithError("Correction Mode", corrections[nIndex]))
        return 1;
    } else {
      if (!setStringWithError("Image Processing - Flatfield Correction",
        newCorrections[nIndex]))
        return 1;
    }
    corrSet = nIndex;
  }
  mLastProcessing = nIndex;

  // Set the movie correction for newer server
  if (mServerVersion >= DE_HAS_MOVIE_COR_ENABLE &&
    (movieCorrEnable != mLastMovieCorrEnabled || !mTrustLastSettings) && !mLiveThread) {
    if (!mAPI2Server) {
      if (!setStringWithError(DE_PROP_AUTOMOVIE"Image Correction",
      movieCorrEnable ? psEnable : psDisable))
      return 1;
    } else if (corrSet < 0) {
      if (!setStringWithError("Image Processing - Flatfield Correction",
        movieCorrEnable ? newCorrections[nIndex] : "None"))
        return 1;
    }
    mLastMovieCorrEnabled = movieCorrEnable;
  }
  return 0;
}
///////////////
// 2/15/17 Removed unused setAutoSaveSumCount, getAutoSaveSumCount, 
// setAutoSaveSumIgnoreFrames, getAutoSaveSumIgnoreFrames
///////////////


/////////////////////////////////////////////////////////
//  Routines to get the three kinds of properties, with locking and error reports
// They return true for success
/////////////////////////////////////////////////////////

// Get int
bool DirectElectronCamera::getIntProperty(CString name, int &value)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!mDeServer->getIntProperty((LPCTSTR)name, &value)) {
        ErrorTrace("ERROR: Could not get int property %s", (LPCTSTR)name);
      } else {
        if (!mWinApp->mDEToolDlg.SuppressDebug())
          SEMTrace('D', "%s: %d", (LPCTSTR)name, value);
        return true;
      }
    }
  }
  return false;
}

// Get Float
bool DirectElectronCamera::getFloatProperty(CString name, float &value)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!mDeServer->getFloatProperty((LPCTSTR)name, &value)) {
        ErrorTrace("ERROR: Could not get float property %s", (LPCTSTR)name);
      } else {
        if (!mWinApp->mDEToolDlg.SuppressDebug())
          SEMTrace('D', "%s: %f", (LPCTSTR)name, value);
        return true;
      }
    }
  }
  return false;
}

// Get String
bool DirectElectronCamera::getStringProperty(CString name, CString &value)
{
  std::string valStr;
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!mDeServer->getProperty(std::string((LPCTSTR)name), &valStr)) {
        ErrorTrace("ERROR: Could not get string property %s", (LPCTSTR)name);
      } else {
        value = valStr.c_str();
        if (!mWinApp->mDEToolDlg.SuppressDebug())
          SEMTrace('D', "%s: %s", (LPCTSTR)name, (LPCTSTR)value);
        return true;
      }
    }
  }
  return false;
}

/////////////////////////////////////////////////////////
// Routines to set properties given string, int or float/double
// The first two return true for success
/////////////////////////////////////////////////////////

// Just convert an integer property to string and set it, return result
bool DirectElectronCamera::justSetIntProperty(const char * propName, int value)
{
  char buffer[20];
  sprintf(buffer, "%d", value);
  return mDeServer->setProperty(propName, buffer);
}

// Just convert a double property to string and set it, return result
bool DirectElectronCamera::justSetDoubleProperty(const char * propName, double value)
{
  CString str;
  str.Format("%f", value);
  return mDeServer->setProperty(propName, (LPCTSTR)str);
}


// Set Integer: get lock, report error
void DirectElectronCamera::setIntProperty(CString name, int value)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!justSetIntProperty((LPCTSTR)name, value))
        ErrorTrace("ERROR: Could not set property %s to %d", (LPCTSTR)name, value);
    }
  }
}

// Set double: get lock, report error
void DirectElectronCamera::setDoubleProperty(CString name, double value)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!justSetDoubleProperty((LPCTSTR)name, value))
        ErrorTrace("ERROR: Could not set property %s to %f", (LPCTSTR)name, value);
    }
  }
}

// Set string: get lock, report error
void DirectElectronCamera::setStringProperty(CString name, CString value)
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      if (!mDeServer->setProperty((LPCTSTR)name, (LPCTSTR)value))
        ErrorTrace("ERROR: Could not set property %s to %s", (LPCTSTR)name, 
          (LPCTSTR)value);
    }
  }
}

// Set a string property from inside a lock with complete trace output and return status
bool DirectElectronCamera::setStringWithError(const char *name, const char *value) 
{
  if (!mDeServer->setProperty(name, value)) {
    mLastErrorString = ErrorTrace("ERROR: Could not set property %s to %s", name, value);
    return false;
  }
  SEMTrace('D', "Set property  %s to %s", name, value);
  return true;
}

// Composes an error string with the given arguments, appends error code and description
// if possible, does a trace with D, and returns the full string
CString DirectElectronCamera::ErrorTrace(char *fmt, ...)
{
  va_list args;
  CString str, str2;
  std::string stdStr;
  va_start(args, fmt);
  VarArgToCString(str, fmt, args);
  va_end(args);
  if (mDeServer) {
    mDeServer->getLastErrorDescription(&stdStr);
    if (mDeServer->getLastErrorCode() == 15 && stdStr == "OK")
      stdStr = "Property may not exist, check spelling and case";
    str2.Format("\r\n   Error code %d%s%s", mDeServer->getLastErrorCode(), 
      stdStr.c_str() != NULL ? ": " : "", stdStr.c_str() != NULL ? stdStr.c_str() : "");
    str += str2;
  }
  SEMTrace('D', "%s", (LPCTSTR)str);
  return str;
}

/*
Error codes for API2:
NoError                  = 0,
AckPacketNotReceived     = 1,
ConnectToCameraFailed    = 2,
ConnectionFailed         = 3,
ExceptionCatched         = 4,
GetPropertySettingFailed = 5,
GetPropertyFailed        = 6,
GetListPropertiesFailed  = 7,
InvalidParameter         = 8,
ImageHeaderMissing       = 9,
ImageSizeIncorrect       = 10,
ImageNotReceived         = 11,
NoValuesReturned         = 12,
ParametersDoNotMatch     = 13,
ReceiveFromServerFailed  = 14,
SendCommandFailed        = 15,
UnableToParseMsg         = 16,
InvalidFrameCount        = 17,
Timeout                  = 18,
Undefined                = 19,
ReturnChangedPropsFailed = 20,
SendVirtualMaskFailed    = 21,
FunctionAccessDenied     = 22,
SendScanXYArrayFailed    = 23,
*/

void DirectElectronCamera::SetAndTraceErrorString(CString str)
{
  mLastErrorString = str;
  SEMTrace('D', "%s", (LPCTSTR)str);
}

// Routine to set the frames per second, also takes care of readout delay of 1 frame for
// DE 12 and sets the current value in camera parameters
int DirectElectronCamera::SetFramesPerSecond(double value)
{
  bool ret1 = true, ret2 = true;
  CameraParameters *camP = mCamParams + 
    (mInitializingIndex >= 0 ? mInitializingIndex : mCurCamIndex);
  bool apollo = (camP->CamFlags & DE_APOLLO_CAMERA) != 0;
  if (apollo)
    return 0;
  SEMTrace('D', "cam index %d  flags %x", mCurCamIndex, camP->CamFlags);
  int readoutDelay = (int)floor(1000. / value);
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      ret1 = justSetDoubleProperty(psFramesPerSec, value);
      if (ret1) {
        mLastFPS = (float)value;
        if (mCurCamIndex >= 0 && !(camP->CamFlags && DE_CAM_CAN_COUNT))
          camP->DE_FramesPerSec = (float)value;
      }
      if (CurrentIsDE12()) {
        if ((camP->CamFlags & DE_HAS_READOUT_DELAY) && !apollo) {
          ret2 = justSetIntProperty(psReadoutDelay, readoutDelay);
        } else {
          if (!apollo)
            ret2 = justSetIntProperty("Ignore Number of Frames", 1);
          else
            ret2 = 1;
          readoutDelay = -1;
        }
        if (ret1 && ret2) {
          SEMTrace('D', "DE FPS set to %f, readout delay to %d", value, readoutDelay);
          mReadoutDelay = readoutDelay;
        } else {
          mLastErrorString = ErrorTrace("Error setting %s%s%s", !ret1 ? "FPS" : "", 
            !ret1 && !ret2 ? " and " : "", !ret2 ? "readoutdelay/ignored frames" : "");
        }
      }
    }
  }
  return (ret1 && ret2) ? 0 : 1;
}

// Converts the rotation and flipping parameters here into an operation value for
// using ProcRotateFlip or transforming coordinates
int DirectElectronCamera::OperationForRotateFlip(int DErotation, int flip)
{
  int operation = -DErotation;
  while (operation < 0)
    operation += 360;
  operation = (operation / 90) % 4;
  if (flip)
    operation += 8;
  return operation;
}

// Check if a reference is valid from a map of valid reference conditions and from 
// properties; add to map if it is
bool DirectElectronCamera::IsReferenceValid(int checksum, std::map<int, int> &vmap, 
  int minuteNow, const char *propStr, const char *refType)
{
  CString valStr, statStr;
  if (!CheckMapForValidState(checksum, vmap, minuteNow)) {
    if (getStringProperty(propStr, valStr)) {
      if (valStr == "None") {
        valStr.Format("There is no %s reference valid for the current conditions", 
          refType);
        if (mServerVersion >= DE_NO_REF_IF_UNNORMED &&
          getStringProperty("System Status", statStr)) {
          valStr += ".  Message from server:\r\n" + statStr;
        }
        SetAndTraceErrorString(valStr);
        return false;
      }
      AddValidStateToMap(checksum, vmap, minuteNow);
    }
  }
  return true;
}

// Look in the map for the checksum and make sure it was checked recently enough
bool DirectElectronCamera::CheckMapForValidState(int checksum, std::map<int, int> &vmap, 
  int minuteNow)
{
  std::map<int,int>::iterator iter;
  iter = vmap.find(checksum);
  return (iter != vmap.end() && minuteNow - iter->second <= VALID_REF_MINUTES);
}

// Add a validated checksum to the map (updates an existing entry), also trim the map
// for expired ones if it is big enough
void DirectElectronCamera::AddValidStateToMap(int checksum, std::map<int, int> &vmap, 
  int minuteNow)
{
  std::map<int,int>::iterator iter;
  IntVec remList;
  vmap[checksum] = minuteNow;
  if (vmap.size() > 20) {
    for (iter = vmap.begin(); iter != vmap.end(); iter++)
      if (minuteNow - iter->second > VALID_REF_MINUTES)
        remList.push_back(iter->first);
    for (int ind = 0; ind < (int)remList.size(); ind++)
      vmap.erase(remList[ind]);
  }
}

// Fill in the metadata for the mdoc file; use existing values where possible
void DirectElectronCamera::SetImageExtraData(EMimageExtra *extra, float nameTimeout, 
  bool allowNamePrediction, bool &nameConfirmed)
{
  CString str;
  std::string valStr;
  double startTime = GetTickCount();
  BOOL saveSums = mLastSaveFlags & DE_SAVE_SUMS;
  BOOL saveCount = saveSums && mLastElectronCounting > 0;
  extra->mDE12Version = mSoftwareVersion;
  extra->mDE12FPS = (float)mWinApp->mDEToolDlg.GetFramesPerSecond();
  extra->mNumDE12Frames = (int)floor((double)mLastExposureTime * extra->mDE12FPS) + 1;
  extra->mDE12CoverMode = 
    mWinApp->mDEToolDlg.GetLastProtCoverSet() ? DE_OPEN_CLOSE_COVER : DE_KEEP_COVER_OPEN;
  extra->mCoverDelay = mWinApp->mDEToolDlg.GetProtCoverDelay();
  if (CurrentIsDE12()) {
    extra->mTemperature = m_camera_Temperature;
    extra->mSensorSerial = mDE12SensorSN;
  }
  extra->mPreExposeTime = (float)(mLastPreExposure / 1000.);
  if (mReadoutDelay >= 0)
    extra->mReadoutDelay = mReadoutDelay;
  if (!IsApolloCamera())
    getFloatProperty(mAPI2Server ? "Faraday Plate - Peak Intensity (pA/cm^2)" :
      "Faraday Plate - Last Exposure Peak Density (pA/cm^2)", extra->mFaraday);

  // Autosave path and frames...
  if ((mLastSaveFlags & DE_SAVE_FRAMES) || saveSums || saveCount) {
    nameConfirmed = GetPreviousDatasetName(nameTimeout, 
      allowNamePrediction ? mSetNamePredictionAgeLimit : 0, true, str);

    // Get the full root path for cleaning up ancillary files
    str = (mLastSaveDir.IsEmpty() ? mWinApp->mDEToolDlg.GetAutosaveDir():
      mLastSaveDir) + "\\" + str;
    mPathAndDatasetName = str;

    // Try to use new property and fall back to what it is maybe supposed to be
    if (mServerVersion >= DE_RETURNS_FRAME_PATH) {
      if (mDeServer->getProperty(B3DCHOICE(mServerVersion >=  DE_AUTOSAVE_RENAMES2,
        "Autosave Movie Frames File Path",  mLastElectronCounting > 0 && 
        !IsApolloCamera() ? "Autosave Counted Movie Frames File Path" :
        "Autosave Integrated Movie Frames File Path"), &valStr) && valStr.length() > 0) {
        str = valStr.c_str();
      } else {
        if (IsApolloCamera())
          str += "_integrated_movie.mrc";
        else
          str += mLastElectronCounting > 0 ? "_counted_frames.mrc" :
            "_integrated_frames.mrc";
      }

    } else {

      // Or attach the standard suffix and extensions to older names
      if (mWinApp->mDEToolDlg.GetFormatForAutoSave()) {
        if ((!saveCount && mNormAllInServer) ||
          (saveCount && mServerVersion >= DE_NO_COUNTING_PREFIX))
          str += "_movie";
        else if (!saveCount)
          str += saveSums ? "_SumImages" : "_RawImages";
        else if (mLastSuperResolution > 0)
          str += "_super_movie";
        else
          str += "_counting_movie";
        str += CString(mServerVersion >= DE_CAN_SAVE_MRC ? ".mrc" : ".h5");
      }
    }
    extra->mSubFramePath = str;

    // Get the number of frames saved if name was actually gotten
    extra->mNumSubFrames = 0;
    if (nameConfirmed) {
      FormatFramesWritten(saveSums, str);
      if (mAPI2Server) {
        startTime = GetTickCount();
        while (SEMTickInterval(startTime) < 5000.) {
          mDeServer->getProperty("Autosave Status", &valStr);
          if (valStr.find("Finished") == 0 || valStr.find("Stopped") == 0)
            break;
          Sleep(50);
        }
      }
      getIntProperty(str, extra->mNumSubFrames);
    }
  } else if (mLastElectronCounting > 0)
    mNumberInPrevSetName++;
}

// Get the string for the previous dataset name or provide a prediction of it if it is not
// available yet.  If the previous actual name known is older than the ageLimitSec, it
// will retry for as long as given by timeout (in seconds); otherwise it will try only
// once.  If it does not get a name back, and predictName is true, and the date matches
// that of the last known name, it will increment the number and compose the name
bool DirectElectronCamera::GetPreviousDatasetName(float timeout, int ageLimitSec, 
  bool predictName, CString &name)
{
  int numTry = 0, wait = 100, numSub;
  std::string valStr, numStr;
  CSingleLock slock(&m_mutex);
  double startTime;
  bool gotName = false, gotNum;
  CString str;
  BOOL saveSums = mLastSaveFlags & DE_SAVE_SUMS;
  CTime ctdt = CTime::GetCurrentTime();

  // Get the date component of the string as an integer
  int currentDate = ctdt.GetYear() * 10000 + ctdt.GetMonth() * 100 + ctdt.GetDay();
  name = "";
  mLastPredictedSetName= "";
  FormatFramesWritten(saveSums, str);
  numStr = (LPCTSTR)str;
  if (slock.Lock(1000)) {

    // Cancel timeout if known name not too old
    if (mDateInPrevSetName == currentDate && SEMTickInterval(mTimeOfPrevSetName) < 
      1000. * ageLimitSec)
      timeout = 0;
    startTime = GetTickCount();
    gotNum = mLastElectronCounting > 0 || timeout == 0;

    // Timeout is set only for new server, so wait for savingto be completed
    if (timeout > 0) {
      if (mAPI2Server) {
        valStr = "In Progress";
        while (valStr != "Finished" && SEMTickInterval(startTime) < 1000. * timeout) {
          mDeServer->getProperty("Autosave Status", &valStr);
          SleepMsg(100);
        }

      } else {
        valStr = "Not Yet";
        while (valStr == "Not Yet" && SEMTickInterval(startTime) < 1000. * timeout) {
          mDeServer->getProperty(DE_PROP_AUTOMOVIE"Completed", &valStr);
          SleepMsg(100);
        }
      }
    }

    if (mDeServer->getProperty(B3DCHOICE(mAPI2Server, "Dataset Name",
      mServerVersion >= DE_AUTOSAVE_RENAMES1 ? 
      "Autosave Previous Dataset Name" : "Autosave Frames - Previous Dataset Name"),
      &valStr) && valStr.length() > 0) {

        // Valid name: return it and save the time and store the two numeric components
        name = valStr.c_str();
        mTimeOfPrevSetName = GetTickCount();
        mDateInPrevSetName = atoi((LPCTSTR)name.Left(8));
        mNumberInPrevSetName = atoi((LPCTSTR)name.Mid(9, 5));
        SEMTrace('D', "It took %.1f sec to get name %s, timeout %.1f",
          SEMTickInterval(startTime) / 1000., valStr.c_str(), timeout);
        gotName = true;
    }
    if (!gotNum && mDeServer->getIntProperty(numStr, &numSub) && numSub > 0) {
      gotNum = true;
      SEMTrace('D', "It took %.1f sec to get number %d, timeout %.1f", 
        SEMTickInterval(startTime) / 1000., numSub, timeout);
    }

    if (gotName)
      return true;

    // No valid name and prediction called for and date matches:increment number and
    // make up name, 
    if (mDateInPrevSetName == currentDate && predictName) {
      name.Format("%08d_%05d%s%s", mDateInPrevSetName, ++mNumberInPrevSetName, 
        mLastSuffix.IsEmpty() ? "" : "_", (LPCTSTR)mLastSuffix);
      mLastPredictedSetName = name;
    }
  }
  return false;
}

// If the last set name was predicted because it was not available yet, get it now (before
// another shot of any kind) and make sure the prediction was right as well as updating
// the known name
void DirectElectronCamera::VerifyLastSetNameIfPredicted(float timeout)
{
  CString str, predictedSave = mLastPredictedSetName;
  if (mLastPredictedSetName.IsEmpty())
    return;
  if (GetPreviousDatasetName(timeout, mSetNamePredictionAgeLimit / 2, false, str) &&
    str != predictedSave)
    PrintfToLog("WARNING: The root name of the last set of saved frames (%s)\r\n    "
      "differs from what was assumed and reported (%s)", (LPCTSTR)str, 
      (LPCTSTR)predictedSave);
  mLastPredictedSetName = "";
}

// Get the right property string for the current version and conditions
void DirectElectronCamera::FormatFramesWritten(BOOL saveSums, CString &str)
{
  if (mAPI2Server)
    str.Format("Autosave %sMovie Frames Written",
      B3DCHOICE(mServerVersion > DE_AUTOSAVE_RENAMES2, "",
        mLastElectronCounting > 0 && IsApolloCamera() ? "Counted " : "Integrated "));
  else
    str.Format("%sFrames Written in Last Exposure", mNormAllInServer ?
      DE_PROP_AUTOMOVIE : (saveSums ? DE_PROP_AUTOSUM : DE_PROP_AUTORAW));
}

float DirectElectronCamera::GetLastCountScaling()
{
  if (mLastNormCounting && mElecCountsScaled > 0)
    return mCountScaling / mElecCountsScaled;
  return mCountScaling;
}

/*
 * Sets up as many parameters for STEM as are feasible in a call from the main thread
 */
int DirectElectronCamera::SetGeneralSTEMParams(int sizeX, int sizeY, int subSizeX, 
  int subSizeY, int left, int top, double rotation, int patternInd, int saveFlags,
  CString &suffix, CString &saveDir, int presetInd, int pointRepeats, bool gainCorr)
{
  bool ret1 = true, ret2 = true, ret3 = true, ret4 = true, ret5 = true, ret6 = true;
  bool ret7 = true;
  bool saveFinal = (saveFlags & DE_SAVE_FINAL) != 0;
  bool saveFrames = (saveFlags & DE_SAVE_MASTER) != 0;
  bool enable = subSizeX < sizeX || subSizeY < sizeY;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {
    ret1 = justSetIntProperty("Scan - Size X", sizeX);
    ret2 = justSetIntProperty("Scan - Size Y", sizeY);
    ret3 = mDeServer->setProperty("Scan - ROI Enable", enable ? "On" : "Off");
    if (enable) {
      ret4 = justSetIntProperty("Scan - ROI Size X", subSizeX);
      ret5 = justSetIntProperty("Scan - ROI Size Y", subSizeY);
      ret6 = justSetIntProperty("Scan - ROI Offset X", left);
      ret7 = justSetIntProperty("Scan - ROI Offset Y", top);
    }
    if (!ret1 || !ret2 || !ret3 || !ret4 || !ret5 || !ret6 || !ret7) {
      SEMMessageBox("An error occurred trying to set STEM size and subarea selections");
      return 1;
    }
    ret1 = justSetDoubleProperty("Scan - Rotation", rotation);
    ret3 = mDeServer->setProperty("Scan - Preset Current", 
      mScanPresets[patternInd].c_str());
    if (!ret1 || !ret3) {
      SEMMessageBox("An error occurred trying to set STEM rotation or preset");
      return 1;
    }

    // Set up autosave location and 4D stack
    if (saveFrames || saveFinal) {
      ret1 = setStringWithError("Autosave Directory", (LPCTSTR)saveDir);
      ret2 = setStringWithError("Autosave Filename Suffix", (LPCTSTR)suffix);
    }
     if (!ret1 || !ret2 || !ret3 || !ret4) {
      SEMMessageBox("An error occurred setting autosave parameters:\n" + 
        mLastErrorString);
      return 1;
    }

    if (saveFrames) {
      ret1 = justSetIntProperty("Scan - Camera Frames Per Point", pointRepeats);
      if (presetInd < (int)mCamPresets.size())
        ret2 = setStringWithError("Preset - Current", mCamPresets[presetInd].c_str());
      ret3 = setStringWithError("Image Processing - Flatfield Correction", 
        gainCorr ? "Dark and Gain" : "Dark");

      if (!ret1 || !ret2 || !ret3) {
        SEMMessageBox("An error occurred trying to set 4D STEM parameters");
        return 1;
      }
    }

    return 0;
  }
  SEMMessageBox("Failed to obtain mutex for setting STEM size and subarea properties");
  return 1;
}

/*
 * Call to get STEM images from an acquisition thread
 */
int DirectElectronCamera::AcquireSTEMImage(unsigned short **arrays, long *channelIndex,
  int numChannels, int sizeX, int sizeY, double pixelTime, int saveFlags, int flags, 
  int &numReturned)
{
  bool saveFinal = (saveFlags & DE_SAVE_FINAL) != 0;
  bool saveFrames = (saveFlags & DE_SAVE_MASTER) != 0;
  bool partialScan = (flags & PLUGCAM_PARTIAL_SCAN) != 0;
  bool divideBy2 = (flags & PLUGCAM_DIVIDE_BY2) != 0;
  bool continuous = (flags & PLUGCAM_CONTINUOUS) != 0;
  bool ret1 = true, ret2 = true, ret3 = true, ret4 = true, ret5 = true, anyVirt = false;
  bool isVirt;
  int ind, acq, chanInd, acquiring, err, chan, virtInd = 0, physInd = 0;
  CString str;
  CameraParameters *camP = mWinApp->GetActiveCamParam();
  CSingleLock slock(&m_mutex);

  acquiring = IsAcquiring();

  // If continuous, first make sure the mutex exists and get frame type to return
  // Continuous is not usable: all it does it return repeatedly with the partial image
  // and set acqFinished, requiring another start
  if (continuous) {
    if (!sLiveMutex) {
      mLastErrorString = "Failed to create the mutex for live mode, continuous mode"
        " not available";
      return 1;
    }
    mLiveTD.frameType = FrameTypeForChannelIndex(channelIndex[0]);

    // If it's already running, get the next image
    if (mLastLiveMode > 1) {
      if (slock.Lock(1000)) {
        err = SetupLiveAndGetImage(arrays[0], sizeX, sizeY, divideBy2 ? 1 : 0, 0);
      } else {
        mLastErrorString = "Failed to get lock for getting next continuous image";
        err = 1;
      }
      if (err) {
        RestoreVirtualShapes(false);
        SetLiveMode(0);
        return 1;
      }
    }
  }

  // If scan was started with partial return, get result
  if (mStartedScan) {
    if (GetSTEMresultImages(arrays, sizeX, sizeY, channelIndex, numChannels,
      acquiring > 0, divideBy2)) {
      numReturned = 0;
      return 1;
    }

    // Finish up if done and return negative
    numReturned = (acquiring ? 1 : -1) * numChannels;
    if (!acquiring) {
      mStartedScan = false;
      RestoreVirtualShapes(false);
    }
    return 0;

  } else if (acquiring > 0 && !continuous) {
    if (slock.Lock(1000)) {
      mDeServer->abortAcquisition();
      slock.Unlock();
    }
  }

  // Now set up all the parameters for a scan
  mVirtChansToRestore.clear();
  mLineMeans.clear();
  mLineMeans.resize(numChannels, -99999);
  mLinesFound.clear();
  mLinesFound.resize(numChannels);

  if (slock.Lock(1000)) {

    if (!setStringWithError("Scan - Enable", "On")) {
      mLastErrorString = "An error occurred making sure the Scan is enabled:\n" +
        mLastErrorString;
      return 1;
    }

    // Find out if any virtual channels
    for (acq = 0; acq < numChannels; acq++)
      if (DETECTOR_IS_VIRTUAL(camP, channelIndex[acq]))
        anyVirt = true;

    // Set up for camera or not based on that
    ret2 = setStringWithError("Scan - Trigger Source", anyVirt ? "Camera (Before Frame)" :
      "DE-FreeScan");
    ret1 = setStringWithError("Scan - Use DE Camera", anyVirt ? "Use Frame Time" : "Off");
    if (!anyVirt) {
      ret3 = justSetDoubleProperty("Scan - Dwell Time (microseconds)", pixelTime);
    } else {
      ret3 = setStringWithError("Autosave Movie", saveFrames ? "On" : "Off");
      if (saveFrames)
        ret4 = setStringWithError("Autosave 4D File Format", "MRC");
      //mWinApp->mDEToolDlg.GetFormatForAutoSave() ? "MRC" : "TIFF");
    }
    ret5 = setStringWithError("Autosave Final Image", saveFinal ? "On" : "Off");
    if (!ret1 || !ret2 || !ret3 || !ret4 || !ret5) {
      mLastErrorString = "An error occurred setting up camera use, dwell time, or file"
        " saving:\n" + mLastErrorString;
      return 1;
    }

    // set up the channels: loop on all possible ones to count physical and virtual
    for (chan = 0; chan < camP->numChannels; chan++) {
      isVirt = DETECTOR_IS_VIRTUAL(camP, chan);
      chanInd = -1;
      for (acq = 0; acq < numChannels; acq++)
        if (chan == channelIndex[acq])
          chanInd = acq;

      // Operations for a virtual channel only if any virtual are included
      if (isVirt && anyVirt) {
        if (saveFinal) {
          str.Format("Autosave Virtual Image %d", virtInd);
          ret1 = setStringWithError((LPCTSTR)str, chanInd < 0 ? "Off" : "On");
        }
        if (chanInd < 0 && mCurVirtShape[virtInd] != "Off") {
          mVirtChansToRestore.push_back(virtInd);
          str.Format("Scan - Virtual Detector %d Shape", virtInd);
          ret2 = setStringWithError((LPCTSTR)str, "Off");
        }
      }

      // Operations for external channels
      if (!isVirt) {
        str.Format("Scan - External Detector %d Enable", physInd + 1);
        ret4 = setStringWithError((LPCTSTR)str, chanInd < 0 ? "Off" : "On");
        if (saveFinal && mServerVersion >= DE_CAN_AUTOSAVE_EXT_DET) {
          str.Format("Autosave External Image %d", physInd + 1);
          ret3 = setStringWithError((LPCTSTR)str, chanInd < 0 ? "Off" : "On");
        }
      }

      // Maintain counts of each detector type
      if (isVirt)
        virtInd++;
      else
        physInd++;
    }

    if (!ret1 || !ret2 || !ret3 || !ret4) {
      RestoreVirtualShapes(true);
      mLastErrorString = "An error occurred setting up channels to acquire or autosave:\n"
       + mLastErrorString;
      return 1;
    }

    // Start continuous mode and get first image
    if (continuous) {
      if (SetLiveMode(2, true)) {
        mLastErrorString = "Failed to get lock in SetLiveMode";
        err = 1;
      }
      err = SetupLiveAndGetImage(arrays[0], sizeX, sizeY, divideBy2 ? 1 : 0, 0);
      if (err) {
        RestoreVirtualShapes(true);
        SetLiveMode(0, true);
      }
      return err;
    } 

    // Or start acquiring one image
    if (!mDeServer->StartAcquisition(1)) {
      mLastErrorString = ErrorTrace("ERROR: Could not start STEM acquisition");
      RestoreVirtualShapes(true);
      return 1;
    }
  } else {
    RestoreVirtualShapes(false);
    mLastErrorString = "Failed to get lock for setting channel parameters and "
      "starting acquire";
    return 1;
  }
  slock.Unlock();

  // Sleep a bit if partial or loop until done
  double startTime = GetTickCount();
  if (partialScan) {
    mStartedScan = true;
    Sleep(100);
  } else {
    while (IsAcquiring() > 0  && SEMTickInterval(startTime) < 70000)
      Sleep(50);
    SEMTrace('D', "Left Acquire loop after %.0f, %d", SEMTickInterval(startTime),
      IsAcquiring());
  }

  // Get result; restore if not partial and no error
  ind = GetSTEMresultImages(arrays, sizeX, sizeY, channelIndex, numChannels, partialScan,
    divideBy2);
  if (!ind && !partialScan)
    RestoreVirtualShapes(false);
  numReturned = numChannels;
  return ind;
}

/*
 * Renable any virtual detectors that were disabled by restoring the original Shape
 */
void DirectElectronCamera::RestoreVirtualShapes(bool skipLock)
{
  CString str, failStr;
  CSingleLock slock(&m_mutex);
  if (skipLock || slock.Lock(1000)) {
    if (mVirtChansToRestore.size() && IsAcquiring(true))
      mDeServer->abortAcquisition();
    for (int ind = 0; ind < (int)mVirtChansToRestore.size(); ind++) {
      if (mVirtChansToRestore[ind] < (int)mCurVirtShape.size()) {
        str.Format("Scan - Virtual Detector %d Shape", mVirtChansToRestore[ind]);
        if (!mDeServer->setProperty((LPCTSTR)str,
          mCurVirtShape[mVirtChansToRestore[ind]].c_str())) {
          str.Format(" %d", mVirtChansToRestore[ind]);
          failStr += str;
        } else
          SEMTrace('D', "Set %s to %s", (LPCTSTR)str, mCurVirtShape[mVirtChansToRestore[ind]].c_str());

      }
    }
    if (!failStr.IsEmpty())
      SEMTrace('0', "WARNING: Failed to reenable virtual detector(s)%s", 
      (LPCTSTR)failStr);
  } else if (mVirtChansToRestore.size())
    SEMTrace('0', "WARNING: Failed to get lock for reenabling virtual detector(s)");
  mVirtChansToRestore.clear();
}

// Test the acquisition status for whether it is acquiring
int DirectElectronCamera::IsAcquiring(bool skipLock)
{
  std::string value;
  CSingleLock slock(&m_mutex);
  if (skipLock || slock.Lock(1000)) {
    if (mDeServer->getProperty("Acquisition Status", &value))
      return (value == "Acquiring") ? 1 : 0;
  }
  return -1;
}

/*
 * Get result from all the channels, determining how to fill partial scans and doing so
 */
int DirectElectronCamera::GetSTEMresultImages(unsigned short **arrays, int sizeX,
  int sizeY, long *channelIndex, int numChannels, bool partial, bool divideBy2)
{
   int imType, spotCheck, base, sum, imX = 0, imY = 0;
  int ind, chan, iy, delta = B3DMIN(sizeX / 5, 10);
  unsigned short fill;
  CSingleLock slock(&m_mutex);

  /*getIntProperty("Image Size X (pixels)", imX);
  getIntProperty("Image Size Y (pixels)", imY);
  SEMTrace('D', "image %d %d  expected %d %d", imX, imY, sizeX, sizeY); */

  if (slock.Lock(1000)) {

    // Find out what channel each one is in turn and get its result 
    for (chan = 0; chan < numChannels; chan++) {

      // Look up channel in virtual then external detectors
      imType = FrameTypeForChannelIndex(channelIndex[chan]);
      if (imType < 0)
        continue;

      // Get the result into the array
      if (partial)
        memset(arrays[chan], 0, sizeX * sizeY * 2);
      DE::ImageAttributes attributes;
      DE::PixelFormat pixForm = DE::PixelFormat::UINT16;
      attributes.stretchType = DE::ContrastStretchType::NONE;
      if (!mDeServer->GetResult(arrays[chan], sizeX * sizeY * 2, (DE::FrameType)imType,
        &pixForm, &attributes)) {
        mLastErrorString = ErrorTrace("Error getting result from channel %d of the %d "
          "selected", chan + 1, numChannels);
        mDeServer->abortAcquisition();
        RestoreVirtualShapes(true);
        mStartedScan = false;
        return 1;
      }

      if (partial) {
        int lastSpot = 0;
        // Search from last line found onwards, check a few pixels in middle
        for (iy = mLinesFound[chan]; iy < sizeY; iy++) {
          base = iy * sizeX + sizeX / 2;
          spotCheck = (int)arrays[chan][base - 2 * delta] + arrays[chan][base - delta] +
            arrays[chan][base] + arrays[chan][base + delta] +
            arrays[chan][base + 2 * delta];
          if (spotCheck == 0) {
            mLinesFound[chan] = B3DMAX(0, iy - 1);
            break;
          }
          lastSpot = spotCheck;
        }

        // Fill if it hasn't gotten to end
        if (iy < sizeY) {

          // If the mean hasn't been found yet and there are enough lines, go back one
          // line and get mean
          if (mLineMeans[chan] < -90000 && iy > B3DMAX(4, sizeY / 40) && 
            mLinesFound[chan] > 1) {
            sum = 0;
            base = (mLinesFound[chan] - 1) * sizeX;
            for (ind = 0; ind < sizeX; ind++)
              sum += arrays[chan][base + ind];
            if (sum / sizeX < 65534)
            mLineMeans[chan] = sum / sizeX;
          }

          // If the mean exists, fill with it to the end
          if (mLineMeans[chan] > -90000) {
            fill = (unsigned short)mLineMeans[chan];
            for (ind = mLinesFound[chan] * sizeX; ind < sizeX * sizeY; ind++)
              arrays[chan][ind] = fill;
          }
        }
      }
      if (divideBy2) {
        for (iy = 0; iy < sizeX * sizeY; iy++)
          arrays[chan][iy] = arrays[chan][iy] >> 1;
      }
    }
    return 0;
  }
  mLastErrorString = "Failed to get lock for getting result image";
  return 1;
}

// Look up the channel index in virtual and external lists and return value to use in
// getResult
int DirectElectronCamera::FrameTypeForChannelIndex(int chanInd)
{
  CameraParameters *camP = mWinApp->GetActiveCamParam();
  int jnd, virtInd = 0, physInd = 0;
  for (jnd = 0; jnd < chanInd; jnd++) {
    if (DETECTOR_IS_VIRTUAL(camP, jnd))
      virtInd++;
    else
      physInd++;
  }
  if (DETECTOR_IS_VIRTUAL(camP, chanInd))
    return (int)DE::FrameType::VIRTUAL_IMAGE0 + virtInd;
  return (int)DE::FrameType::EXTERNAL_IMAGE1 + physInd;
}

/*
 * Given the preset, find out what FPS it will have, return 0 on any errors
 */
float DirectElectronCamera::GetPresetOrCurrentFPS(std::string preset)
{
  CString current, value;
  bool ok;
  float fps = 0.;
  int ind, colon;
  StringVec propList;
  const char *curStr = "Preset - Current";

  // If the preset is Custom, it will be the current FPS, otherwise proceed
  if (preset != "Custom") {

    // Get the current preset - if IT is Custom, we need to save and restore the FPS
    if (!getStringProperty(curStr, current))
      return 0.0f;
    if (current == "Custom" && !getFloatProperty(psFramesPerSec, fps))
      return 0.;

    // Go to the designated preset if needed
    if (current != preset.c_str()) {
      if (!setStringWithError(curStr, preset.c_str()))
        return 0.;
    }

    // Get the properties and restore the current preset and FPS if it was custom
    ok = getStringProperty("Preset - Properties", value);
    if (current != preset.c_str())
      setStringProperty(curStr, current);
    if (current == "Custom")
      justSetDoubleProperty(psFramesPerSec, fps);
    if (!ok)
      return 0.;

    // Split up the comma-separated list, find FPS and colon, return converted value
    UtilSplitString(value, ",", propList);
    for (ind = 0; ind < (int)propList.size(); ind++) {
      if (propList[ind].find(psFramesPerSec) == 0) {
        colon = (int)propList[ind].find(':');
        if (colon < 1)
          return 0.;
        return (float)atof(propList[ind].c_str() + colon + 1);
      }
    }
  }

  // Fall through to getting the FPS and returning it
  if (!fps && !getFloatProperty(psFramesPerSec, fps))
    return 0.;
  return fps;
}
