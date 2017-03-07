
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
#include "DeInterface.Win32.h"

using namespace std;

#include "../EMScope.h"
#include "../TSController.h"
#include "../ShiftManager.h"
#include "../EMmontageController.h"
#include "../ProcessImage.h"
#include "../PluginManager.h"
#include "../CameraController.h"
#include "..\GainRefMaker.h"
#include "..\Utilities\XCorr.h"

#include "DirectElectronCamPanel.h"

// Property strings used more than once
#define psCamPosition "Camera Position"
#define psSave "Save"
#define psNoSave  "Discard"

///////////////////////////////////////////////////////////////////
// Added a special properties files for reading properties specific to the LC1100.
// This was also placed in the typical
// These properties include reading the .dcf , .set, .cfg , the COM port number
// that the camera is connected to, whether the peltier device should turn on
// upon initialization of the camera, and finally a flag to trigger
// whether there should be some basic interpolation applied to
///////////////////////////////////////////////////////////////////
#define NUM_OF_LINES 6
#define FILE_PROPERTIES "C:\\Program Files\\SerialEM\\LC1100Properties.txt"


bool FIX_COLUMNS = false;

//Customized panel:
//The dialog window that is specific to non-DDD cameras
static DirectElectronCamPanel *DE_panel = NULL;

static HANDLE sLiveMutex = NULL;
#define LIVE_MUTEX_WAIT 2000
static void CleanupLiveMode(LiveThreadData *td);

///////////////////////////////////////////////////////////////////
// Constructor of the spectral camera controlling class.
// It will initialize the references that are used to communicate
// with the camera, handle the memory management, along with
// the callback
///////////////////////////////////////////////////////////////////
DirectElectronCamera::DirectElectronCamera(int camType, int index)
{

  mCamType = camType;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mCamParams = mWinApp->GetCamParams();

  // This will need to be false if the server can ever be connected to from multiple 
  // sources or can restart without our knowledge, and it should be false if server delays
  // are eliminated
  mTrustLastSettings = true;

  // DNM: need to initialize these because uninitialize is called by destructor
  m_sink = NULL;
  m_FSMObject = NULL;
  m_LCCaptureInterface = NULL;
  m_DE_CONNECTED = false;
  mServerVersion = 0;

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
  mLastXoffset = mLastYoffset = -1;
  mLastROIsizeX = mLastROIsizeY = -1;
  mLastXimageSize = mLastYimageSize = -1;
  mLastXbinning = mLastYbinning = -1;
  mLastExposureMode = -999;
  mLastExposureTime = -1.;
  mLastPreExposure = -1.;
  mLastProcessing = -1;
  mLastLiveMode = -1;
}


///////////////////////////////////////////////////////////////////
//Destructor to invoke the UnInitialize routine
///////////////////////////////////////////////////////////////////
DirectElectronCamera::~DirectElectronCamera(void)
{
  unInitialize();
}

///////////////////////////////////////////////////////////////////
//unInitialize() routine will check for available references
// to the pointers that were allocated to control the camera.
// if they actually point to some memory address go ahead and
// properly deallocate.
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::unInitialize()
{
  if (m_DE_CLIENT_SERVER) {
    if (mDeServer) {
      if (m_DE_CONNECTED) {
        if (!GetDebugOutput('W'))
          mDeServer->setProperty(psCamPosition, DE_CAM_STATE_RETRACTED);
        mDeServer->close();
      }
    }
    mDeServer = NULL;
  } else {

    if (m_FSMObject) {
      m_FSMObject->FreeShort();
      //Release Memory object
      m_FSMObject->Release();
    }
    if (m_LCCaptureInterface)
      m_LCCaptureInterface->Release();
    if (m_sink) {
      delete m_sink;
    }

    m_sink = NULL;
    m_FSMObject = NULL;
    m_LCCaptureInterface = NULL;

  }
  return 1;
}


///////////////////////////////////////////////////////////////////
//initialize() routine.  Will create the necessary instances
// for communicating with the camera, will read out the camera properties
// from the specral4k and also initialize the camera to its "default"
// settings.  This means setting the appropriate image format
// and binning settings etc.
//
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::initialize(CString camName, int camIndex)
{
  if (m_DE_CLIENT_SERVER) {

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
    check = initializeDECamera(camName);
    mInitializingIndex = -1;
    if (check == -1)
      return check;

  } else {

    HRESULT hr =  CoCreateInstance(CLSID_CoSoliosFor4000, NULL, CLSCTX_INPROC_SERVER,
                                   IID_ICameraCtrl, (void **)&m_LCCaptureInterface);
    if (SEMTestHResult(hr, " getting instance of SoliosFor4000 for DE camera"))
      return -1;
    hr = CoCreateInstance(CLSID_FSM_MemoryObject, NULL, CLSCTX_INPROC_SERVER,
                          IID_IFSM_MemoryObject, (void **)&m_FSMObject);
    if (SEMTestHResult(hr, " getting instance of FSM_MemoryObject for DE camera"))
      return -1;

    //read in a properties file to know how to assign
    //Read in the information as this is very critical
    int check;
    check = readCameraProperties();


    if (check == -1) {
      ErrorTrace("ERROR reading camera properties for DE camera");
      return -1; //couldnt read the properties so return.
    }

    //sets the path of the dcf file.
    m_LCCaptureInterface->SetDCFPath(0, 0, m_dcf_file);
    //allocate the memory to store the image.
    m_FSMObject->AllocateShort(MAX_IMAGE_SIZE, 1);
    m_FSMObject->GetDataPtrShort(0, &m_pCapturedDataShort);

    //Properly Cast to set the image buffer.
    m_LCCaptureInterface->SetImageBuffer((IFSM_MemoryObject *)m_FSMObject);

    m_sink = new CLC1100CamSink(m_LCCaptureInterface, m_pCapturedDataShort);

    //read in the file that represents all the parameters from the camera
    m_LCCaptureInterface->SetParamTablePath(m_filename, m_tablefilename);
    //read the file that has the configuration parameters specific to the camera
    //See the Direct Electron Camera Manual on the LC1100 for all the details for
    //the parameters
    m_LCCaptureInterface->SetConfigTablePath(m_filename, m_tablefilename);
    //m_CaptureInterface->ReadoutParam_RecAllFromHost();
    //m_CaptureInterface->ConfigParam_RecAllFromHost();

    //read out all the config params
    m_LCCaptureInterface->ConfigParam_TransToHost();
    m_LCCaptureInterface->Status_TransToHost();
    //read out the status params

    m_LCCaptureInterface->SetOrigin(ORIGIN_X, 0);


    //make sure window heater is on
    m_LCCaptureInterface->put_ConfigParam(WINDOW_HEATER, 1);

    //Just initially disply the Configuration parameters
    long param;
    m_LCCaptureInterface->get_ConfigParam(INSRUMENT_MODEL, &param);
    m_instrument_model = param;
    m_LCCaptureInterface->get_ConfigParam(SERIAL_HEAD, &param);
    m_head_serial = param;
    m_LCCaptureInterface->get_ConfigParam(SHUTTER_CLOSE_DELAY, &param);
    m_shutter_close_delay = param;
    m_LCCaptureInterface->get_ConfigParam(PRE_EXPOSE_DELAY, &param);
    m_pre_expose_delay = param;
    m_LCCaptureInterface->get_ConfigParam(CCD_TEMP_SETPOINT, &param);
    m_ccd_temp_setPoint = (float)((param / 10) - 273.15);
    m_LCCaptureInterface->get_ConfigParam(WINDOW_HEATER, &param);
    m_window_heater = param;


    //make sure window heater is on
    m_LCCaptureInterface->put_ConfigParam(WINDOW_HEATER, 1);
    //update the camera
    m_LCCaptureInterface->ConfigParam_RecAllFromHost();

    //trigger the peltier device to turn on.
    m_LCCaptureInterface->ExternalRelayTurnOn();


    //At this point it would be good to check the pressure and temperature of the
    //camera before proceding
    readinTempandPressure();

    if (!DE_panel) {
      DE_panel = new DirectElectronCamPanel();
      DE_panel->setCameraReference(this);
      DE_panel->Create(IDD_DirectElectronDialog);
      DE_panel->ShowWindow(SW_SHOW);

    }

  }

  return 1;
}

///////////////////////////////////////////////////////////////////
// Ensures that there is a proper connection between the client
// and the server.
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::initDEServer()
{
  CString token;
  int curpos = 0;
  if (!mDeServer) {
    SEMTrace('D', "No DE plugin was loaded");
    m_DE_CONNECTED = false;
    return -1;
  }

  SEMTrace('D', "Supposed readPort %d, write port: %d and de server: %s", mDE_READPORT, 
    mDE_WRITEPORT, mDE_SERVER_IP);

  if (mDE_READPORT <= 0 || mDE_WRITEPORT <= 0 || mDE_SERVER_IP == "") {
    AfxMessageBox("You did not properly setup the DE Server Properties!!!");
    SEMTrace('D', "ERROR: You did not properly setup the DE server properties, double "
      "check SerialEMProperties.txt");
    m_DE_CONNECTED = false;
    return -1;
  }

  if (mDeServer->connect((LPCTSTR)mDE_SERVER_IP, mDE_READPORT, mDE_WRITEPORT)) {
    SEMTrace('D', "Successfully connected to the DE Server.");
    m_DE_CONNECTED = true;
  } else {
    AfxMessageBox("Could NOT connect to DE server. \n Be sure the Direct Electron "
      "server IS running\nand MicroManager is NOT running!");
    ErrorTrace("ERROR: Could NOT connect to the DE server.");
    m_DE_CONNECTED = false;
    return -1;
  }

  //Read out the possible list of cameras that have been setup:
  std::vector<std::string> cameras;
  if (!mDeServer->getCameraNames(&cameras)) {
    CString str = ErrorTrace("ERROR: Could NOT retrieve any of the DE camera names.");
    AfxMessageBox(str);
    return -1;
  }

  CString listCameras = "Camera list:  \r\n";

  for (int i = 0; i < (int)cameras.size(); i++)
    listCameras += CString(cameras[i].c_str()) + "\r\n";

  SEMTrace('D', "Camera list: %s", listCameras);

  std::string propValue;
  if (mDeServer->getProperty(std::string("Server Software Version"), &propValue)) {
    mSoftwareVersion = propValue.c_str();

    // Convert to a single number, 1000000 * major * 10000 * minor + build
    token = mSoftwareVersion.Tokenize(".", curpos);
    if (!token.IsEmpty()) {
      mServerVersion = 1000000 * atoi((LPCTSTR)token);
      token = mSoftwareVersion.Tokenize(".", curpos);
      if (!token.IsEmpty()) {
        mServerVersion += 10000 * atoi((LPCTSTR)token);
        token = mSoftwareVersion.Tokenize(".", curpos);
        if (!token.IsEmpty())
          mServerVersion += atoi((LPCTSTR)token);
      }
    }
    if (mServerVersion >= DE_CAN_SAVE_MRC)
      mWinApp->mDEToolDlg.SwitchSaveOptToMRC();
  } else {
    token = ErrorTrace("Call to get software version failed; try restarting SerialEM");
    AfxMessageBox(token);
  }

  return 1;
}

// Initialize a particular DE camera
int DirectElectronCamera::initializeDECamera(CString camName)
{
  //read in a properties file to know how to assign
  //Read in the information as this is very critical
  if (m_DE_CONNECTED) {
    CString str;
    CString tmp;
    bool result;

    result = mDeServer->setCameraName((LPCTSTR)camName);
    m_DE_CurrentCamera = camName;

    SEMTrace('D', "Camera name set to : %s", (LPCTSTR)camName);

    if (!result) {
      str = ErrorTrace("Could not set the camera name: %s", (LPCTSTR)camName);
      AfxMessageBox(str);
      return -1;
    }

    //By default, set it to uncorrected raw images
    setCorrectionMode(DE_UNCORRECTED_RAW_MODE);
    setStringProperty("Autosave Raw Frames - Save Correction", "Disable");
    setStringProperty("Autosave Sum Frames - Save Correction", "Disable");
    setIntProperty("Autosave Sum Frames - Ignored Frames", 0);

    if (GetDebugOutput('D')) {
      double startTime = GetTickCount();
      CString listProps;
      listProps += camName;
      listProps += "\r\n\r\n";

      //read out all the possible properties when starting in Debug mode
      std::vector<std::string> camProps;
      if (!mDeServer->getProperties(&camProps)) {
        str = ErrorTrace("Could not get properties for the camera named : %s", 
          (LPCTSTR)camName);
        AfxMessageBox(str);
        return -1;
      }

      for (int i = 0; i < (int)camProps.size(); i++) {

        listProps += camProps[i].c_str();
        std::string propValue;
        if (mDeServer->getProperty(camProps[i], &propValue))
          listProps += CString("    Value: ") + propValue.c_str() + "\r\n";

      }

      str += "\r\nProperties for : " + listProps;
      str += "\r\n\r\n";
      SEMTrace('D', "%s", str);
      SEMTrace('D', "Time to get properties %.3f", SEMTickInterval(startTime) / 1000.);
    }

    FinishCameraSelection(true);
  }

  return 1;
}

////////////////
// 2/15/17: removed unused setTotalFrames, getPreviousDataSetName, getNextDataSetName
///////////////

// Returns the string for the camera insertion state
CString DirectElectronCamera::getCameraInsertionState()
{
  std::string camState = "";
  mDeServer->getProperty(psCamPosition, &camState);

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
void DirectElectronCamera::setCameraName(CString camName)
{

  //Check to make sure we dont set camera name if its
  //already set.
  if (m_DE_CLIENT_SERVER) {

    CameraParameters *camP = &mCamParams[mWinApp->GetCurrentCamera()];

    //set to the current Values from the camera.
    //Just in case in the future we need to
    mDE_SERVER_IP = camP->DEServerIP;
    mDE_READPORT = camP->DE_ServerReadPort;
    mDE_WRITEPORT = camP->DE_ServerWritePort;
    m_DE_ImageRot = camP->DE_ImageRot;
    m_DE_ImageInvertX = camP->DE_ImageInvertX;

    if (m_DE_CurrentCamera == camName) {
      SEMTrace('D', "You have already selected this camera, no need to set it again.");
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

      //its ok to update the panel after sending out any settings that exist
      SEMTrace('D', "Set the camera to:  %s", (LPCTSTR)camName);
      FinishCameraSelection(false);
    }

  }
}

// Common tasks when initializing or selecting a camera
void DirectElectronCamera::FinishCameraSelection(bool initialCall)
{
  if (CurrentIsDE12()) {
    getIntProperty("Sensor Readout Delay (milliseconds)", mReadoutDelay);
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
int DirectElectronCamera::SetAllAutoSaves(int autoSaveFlags, int sumCount, CString suffix)
{
  bool ret1 = true, ret2 = true, ret3 = true, ret4 = true, ret5 = true;
  if (!m_DE_CLIENT_SERVER)
    return 1;
  int saveRaw = (autoSaveFlags & DE_SAVE_FRAMES) ? 1 : 0;
  int saveSums = (autoSaveFlags & DE_SAVE_SUMS) ? 1 : 0;
  int saveFinal = (autoSaveFlags & DE_SAVE_FINAL) ? 1 : 0;
  bool setSaves = mLastSaveFlags < 0 || !mTrustLastSettings;
  if (!saveSums)
    sumCount = 0;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {
    if (setSaves || ((mLastSaveFlags & DE_SAVE_FRAMES) ? 1 : 0) != saveRaw)
      ret1 = mDeServer->setProperty("Autosave Raw Frames", saveRaw ? psSave : psNoSave);
    if (setSaves || ((mLastSaveFlags & DE_SAVE_SUMS) ? 1 : 0) != saveSums)
      ret2 = mDeServer->setProperty("Autosave Summed Image", saveSums ? psSave :psNoSave);
    if (setSaves || ((mLastSaveFlags & DE_SAVE_FINAL) ? 1 : 0) != saveFinal)
      ret3 = mDeServer->setProperty("Autosave Final Image", saveFinal ? psSave :psNoSave);

    if (mLastSumCount < 0 || !mTrustLastSettings || mLastSumCount != sumCount) {
      ret4 = justSetIntProperty("Autosave Sum Frames - Sum Count", sumCount);
    }
    if (setSaves || ((saveRaw || saveSums || saveFinal) && suffix != mLastSuffix)) {
      ret5 = mDeServer->setProperty("Autosave Filename Suffix (User Input)", 
        (LPCTSTR)suffix);
      mLastSuffix = suffix;
    }
    if (!ret1 || !ret2 || !ret3 || !ret4 || !ret5) {
      SEMMessageBox("An error occurred trying to set frame and sum saving to current "
        "selections");
      return 1;
    }
    mLastSaveFlags = autoSaveFlags;
    mLastSumCount = sumCount;
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

    if (m_DE_CLIENT_SERVER) {
      if (fabs(mLastPreExposure - preExpMillisecs) > 0.01 || !mTrustLastSettings) {
        if (!justSetDoubleProperty("Preexposure Time (seconds)", preExpMillisecs / 1000.))
        {
          ErrorTrace("ERROR: Could NOT properly set the pre-exposure time.");
          return 1;
        }
        SEMTrace('D', "Pre-exposure time set to:  %.3f", preExpMillisecs / 1000.);
        mLastPreExposure = preExpMillisecs;
      }
    } else {
      m_LCCaptureInterface->ConfigParam_TransToHost();
      m_LCCaptureInterface->put_ConfigParam(PRE_EXP_DELAY, (long)preExpMillisecs);
      m_LCCaptureInterface->ConfigParam_RecAllFromHost();
    }

    return 0;
  }
  return 1;
}

///////////////////////////////////////////////////////////////////
//copyImageData() routine.  Copy the data from what was acquired
//from the camera into an appropriately sized array for
//SerialEM to use for its acquisition process.  For DE server, this starts the acquire
//
///////////////////////////////////////////////////////////////////

int DirectElectronCamera::copyImageData(unsigned short *image4k, int imageSizeX, 
                                        int imageSizeY, int divideBy2)
{

  if (!m_DE_CLIENT_SERVER && m_STOPPING_ACQUISITION == true) {
    memset(image4k, 0, imageSizeX * imageSizeY * 2);
    m_STOPPING_ACQUISITION = false;
    return -1;
  }

  if (m_DE_CLIENT_SERVER) {
    // Need to take the negative of the rotation because of Y inversion, 90 with the CImg
    // library was producing clockwise rotation of image
    int operation = OperationForRotateFlip(m_DE_ImageRot, m_DE_ImageInvertX);
    int inSizeX, inSizeY, outX, outY, status;
    bool startedThread = false;
    inSizeX = imageSizeX;
    inSizeY = imageSizeY;
    if (mLastLiveMode > 1) {
      if (mLiveTD.outBufInd == -1) {

        // Setup the thread data and allocate buffers
        mLiveTD.DeServer = mDeServer;
        mLiveTD.operation = operation;
        mLiveTD.divideBy2 = divideBy2;
        mLiveTD.inSizeX = inSizeX;
        mLiveTD.inSizeY = inSizeY;
        mLiveTD.quitLiveMode = false;
        mLiveTD.returnedImage[0] =  mLiveTD.returnedImage[1] = false;
        NewArray(mLiveTD.buffers[0], unsigned short, imageSizeX * imageSizeY);
        NewArray(mLiveTD.buffers[1], unsigned short, imageSizeX * imageSizeY);
        if (operation)
          NewArray(mLiveTD.rotBuf, unsigned short, imageSizeX * imageSizeY);
        if (!mLiveTD.buffers[0] || !mLiveTD.buffers[1] || (operation && !mLiveTD.rotBuf)){
          CleanupLiveMode(&mLiveTD);
          SEMTrace('D', "Failed to get memory for live mode buffers");  
          return 1;
        }

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
          SEMTrace('D', "Live mode did not get started in timely manner");
        } else if (status < 0) {
          ErrorTrace("ERROR: Live mode ended with an error");
        } else if (startedThread) {
          SEMTrace('D', "Live mode ended without error right after being started");
        } else {
          SEMTrace('D', "Live mode flag is set after thread was told to quit");
        }
        CleanupLiveMode(&mLiveTD);
        return 1;
      }

      // Loop until the output buffer is new; let SEM kill this thread if it times out?
      for (;;) {
        WaitForSingleObject(sLiveMutex, LIVE_MUTEX_WAIT);
        if (!mLiveTD.returnedImage[mLiveTD.outBufInd]) {
          memcpy(image4k, mLiveTD.buffers[mLiveTD.outBufInd], 2 * inSizeX * inSizeY);
          mLiveTD.returnedImage[mLiveTD.outBufInd] = true;
          ReleaseMutex(sLiveMutex);
          return 0;
        }
        ReleaseMutex(sLiveMutex);
        Sleep(10);
      }
    }

    // Resume code for normal single image acquisition
    unsigned short *useBuf = image4k;
    if (operation) {
      NewArray(useBuf, unsigned short, imageSizeX * imageSizeY);
      if (!useBuf) {
        SEMTrace('D', "Failed to get memory for rotation/flip of DE image");
        return 1;
      }
    }

    SEMTrace('D', "About to get image from DE server now that all properties are set.");

    if (!mDeServer->getImage(useBuf, imageSizeX * imageSizeY * 2)) {
      ErrorTrace("ERROR: Could NOT get the image from DE server");
      if (operation)
        delete useBuf;
      return 1;
    }
    SEMTrace('D', "Got something back from DE Server..");

    // Do the rotation/flip, free array, divide by 2 if needed
    if (operation) {
      ProcRotateFlip((short *)useBuf, kUSHORT, inSizeX, inSizeY, operation, 0, 
        (short *)image4k, &outX, &outY);
      delete useBuf;
    }
    if (divideBy2)
      for (int i = 0; i < imageSizeX * imageSizeY; i++)
        image4k[i] = image4k[i] >> 1;
  }

  else { //LC1100 code

    int xsize = imageSizeX;
    int ysize = imageSizeY;
    int outX, outY;
    SEMTrace('D', "copyImageData: requested size %d x %d, image %d x %d", xsize, ysize,
      m_sink->getImageSizeX(), m_sink->getImageSizeY());

    // There was a transposition of X and Y in copying the data from the array
    // This will produce a transposition around upper-left to lower-right diagonal of an
    // image with inverted Y.  This is the same as flip followed by 90 CCW
    unsigned short *data = m_sink->getData();
    ProcRotateFlip((short *)data, kUSHORT, m_sink->getImageSizeX(), 
      m_sink->getImageSizeY(), 5, 0, (short *)image4k, &outX, &outY);
    if (divideBy2)
      for (int i = 0; i < outX * outY; i++)
        image4k[i] = image4k[i] >> 1;

    // Removed hard-coded column defect correction which failed on subareas in 3.4
  }
  return 0;
}

/*
 * Thread procedure for live mode
 */
UINT DirectElectronCamera::LiveProc(LPVOID pParam)
{
  LiveThreadData *td = (LiveThreadData *)pParam;
  int newInd, outX, outY, retval = 0;
  unsigned short *useBuf, *image4k;

  while (!td->quitLiveMode) {

    // Get the new index and buffer to place data into
    newInd = B3DCHOICE(td->outBufInd < 0, 0, 1 - td->outBufInd);
    image4k = td->buffers[newInd];
    useBuf = td->operation ? td->rotBuf : image4k;
    
    // Get image
    if (!td->DeServer->getImage(useBuf, td->inSizeX * td->inSizeY * 2)) {
      retval = 1;
      break;
    }

    // Process it if needed
    if (td->operation)
      ProcRotateFlip((short *)useBuf, kUSHORT, td->inSizeX, td->inSizeY, td->operation, 0, 
        (short *)image4k, &outX, &outY);
    if (td->divideBy2)
      for (int i = 0; i < td->inSizeX * td->inSizeY; i++)
        image4k[i] = image4k[i] >> 1;

    // Get the mutex and place change the output buffer index, mark as not returned
    WaitForSingleObject(sLiveMutex, LIVE_MUTEX_WAIT);
    td->outBufInd = newInd;
    td->returnedImage[newInd] = false;
    ReleaseMutex(sLiveMutex);
    Sleep(10);
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
int DirectElectronCamera::setBinning(int x, int y, int sizex, int sizey)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {

      // set binning if it does not match last value
      if (x != mLastXbinning || y != mLastYbinning || !mTrustLastSettings) {
        if (!justSetIntProperty(g_Property_DE_BinningX, x) || 
          !justSetIntProperty(g_Property_DE_BinningY, y)) {
            ErrorTrace("ERROR: Could NOT set the binning parameters of X: %d and Y: %d" 
              , x, y);
            return 1;
        } else {
          m_binFactor = x;
          SEMTrace('D', "Binning factors now set to X: %d Y: %d", x, y);
        }
      }

      // set size if it does not match last value
      if (sizex != mLastXimageSize || sizey != mLastYimageSize || !mTrustLastSettings) {
        if (!justSetIntProperty(g_Property_DE_ImageWidth, sizex) || 
          !justSetIntProperty(g_Property_DE_ImageHeight, sizey)) {
            ErrorTrace("ERROR: Could NOT set the dimension parameters of X: %d and"
              " Y: %d", sizex, sizey);
            return 1;
        } else
          SEMTrace('D', "Dimensions set to X: %d Y: %d", sizex, sizey);
      }

      // Save last values regardless
      mLastXbinning = x;
      mLastYbinning = y;
      mLastXimageSize = sizex;
      mLastYimageSize = sizey;

    } else { //LC1100 DE code
      m_LCCaptureInterface->SetBinning(x, y);
      m_binFactor = x;
      int OriginX = 0;
      int OriginY = 0;


      // NOTE hard-coded inversion of X and Y sizes, anticipating a rotation
      int swamp = sizey;
      sizey = sizex;
      sizex = swamp;

      //Had to adjust the size for binning 4.  The origin of the camera
      //has to be (48,0).  Currently there is no support for binning x8.
      if (m_binFactor == 4) {
        OriginX = (IMAGE_X_OFFSET + ORIGIN_X_BIN4) - ((sizex * x) / 2);
        OriginY = (IMAGE_Y_OFFSET - ((sizey * y) / 2));
        m_LCCaptureInterface->SetOrigin(OriginX, OriginY);
      } else { //This was tested and works well for binning x1 and binning x2
        OriginX = (IMAGE_X_OFFSET + ORIGIN_X) - ((sizex * x) / 2);
        OriginY = (IMAGE_Y_OFFSET - ((sizey * y) / 2));
        m_LCCaptureInterface->SetOrigin(OriginX, OriginY);
      }
      m_FSMObject->AllocateShort(sizex * sizey, 1);
      //Reverse the sizes so that when rotated it looks correct.
      m_LCCaptureInterface->SetImageSize((sizex / 2), (sizey / 2));
      SEMTrace('D', "Capture dimensions set to %d x %d", sizex, sizey);

    }

    return 0;
  }
  return 1;
}


// Set size and offset in unbinned coordinates as returned by the server; the caller
// now takes care of setting these correctly from the final user's coordinates
int DirectElectronCamera::setROI(int offset_x, int offset_y, int xsize, int ysize)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    bool ret1 = true, ret2 = true;
    if (m_DE_CLIENT_SERVER) {
      if (offset_x != mLastXoffset || offset_y != mLastYoffset || !mTrustLastSettings) {
        ret1 = justSetIntProperty(g_Property_DE_RoiOffsetX, offset_x) && 
          justSetIntProperty(g_Property_DE_RoiOffsetY, offset_y);
        SEMTrace('D', "ROI settings: offsetX: %d offsetY: %d  ret: %d", offset_x, 
          offset_y, ret1 ? 1 : 0);
      }
      if (xsize != mLastROIsizeX || ysize != mLastROIsizeY || !mTrustLastSettings) {
        ret2 = justSetIntProperty(g_Property_DE_RoiDimensionX, xsize) && 
          justSetIntProperty(g_Property_DE_RoiDimensionY, ysize);
        SEMTrace('D', "ROI settings: xsize: %d ysize: %d  ret: %d", xsize, ysize, 
          ret2 ? 1 : 0);
      }
      if (!ret1 || !ret2)
        return 1;
      mLastXoffset = offset_x;
      mLastYoffset = offset_y;
      mLastROIsizeX = xsize;
      mLastROIsizeY = ysize;
    }

    return 0;
  }
  return 1;
}

// Unused
int DirectElectronCamera::setImageSize(int sizeX, int sizeY)
{

  if (m_DE_CLIENT_SERVER) {
    if (sizeX != mLastXimageSize || sizeY != mLastYimageSize || !mTrustLastSettings) {
      if (!justSetIntProperty(g_Property_DE_ImageWidth, sizeX) || 
        !justSetIntProperty(g_Property_DE_ImageHeight, sizeY)) {
          ErrorTrace("Could NOT set the dimension parameters of X: %d and Y: %d", 
            sizeX, sizeY);
          return 1;
      }
      SEMTrace('D', "Dimensions set to X: %d Y: %d", sizeX, sizeY);
      mLastXimageSize = sizeX;
      mLastYimageSize = sizeY;
    }
  } else {
    m_LCCaptureInterface->SetImageSize(sizeX, sizeY);
  }

  return 0;
}


///////////////////////////////////////////////////////////////////
//AcquireImage() routine.  Function will read in the passed
//exposure time and execute an acquire (except for DE server).  The notification
//of when the camera has finished acquired
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::AcquireImage(float seconds)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {
      if (SetExposureTimeAndMode(seconds, 
        mWinApp->mGainRefMaker->GetTakingRefImages() ? DE_GAIN_IMAGE : DE_NORMAL_IMAGE))
        return 1;

    } else {  // LC1100
      m_LCCaptureInterface->ConfigParam_TransToHost();
      long atten;
      long dsi_time;

      m_LCCaptureInterface->get_ReadoutParam(ATTENUATION_PARAM, &atten);
      m_LCCaptureInterface->get_ReadoutParam(DSI_TIME, &dsi_time);
      SEMTrace('D', "DE_CAMERA: In AquireImage:  attenuation_param %d and DSI_TIME %d", 
        atten, dsi_time);
      m_sink->setDataIsNotReady();
      m_STOPPING_ACQUISITION = false;
      m_LCCaptureInterface->put_ExposureTime((long)((float)(seconds * 1000)));
      m_LCCaptureInterface->CCD_ReadAndExpose();

      while (!isImageAcquistionDone()) //wait until the image is finish acquiring
        ;

    }
  }
  return 0;
}


///////////////////////////////////////////////////////////////////
//AcquireDarkImage() routine.  Function will read in the Bias
//image, except for DE Server!.  This function takes into account the exposure time so
//its more appropriate to state that it reads in the BIAS image.
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::AcquireDarkImage(float seconds)
{

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {
      if (SetExposureTimeAndMode(seconds, DE_DARK_IMAGE))
        return 1;

    } else {
      //read out the configs.
      m_LCCaptureInterface->ConfigParam_TransToHost();
      long atten;
      long dsi_time;
      m_LCCaptureInterface->get_ReadoutParam(ATTENUATION_PARAM, &atten);
      m_LCCaptureInterface->get_ReadoutParam(DSI_TIME, &dsi_time);
      SEMTrace('D', "DE_CAMERA: In AquireDarkImage:  attenuation_param %d and DSI_TIME %d"
        , atten, dsi_time);
      m_sink->setDataIsNotReady();
      m_STOPPING_ACQUISITION = false;
      m_LCCaptureInterface->put_ExposureTime((long)((float)(seconds * 1000)));
      m_LCCaptureInterface->CCD_ReadNoExpose();

      while (!isImageAcquistionDone()) //wait until the image is finish acquiring
        ;
    }

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

  if (fabs((double)(seconds - mLastExposureTime)) > 1.e-4 || !mTrustLastSettings) {
    if (!justSetDoubleProperty(g_Property_DE_ExposureTime, seconds)) {
      ErrorTrace("ERROR: Could NOT set the exposure time of %f", seconds);
      return 1;
    }
    SEMTrace('D', "Exposure time set to %.4f", seconds);
  }

  if (mLastExposureMode != mode || !mTrustLastSettings) {
    if (!mDeServer->setProperty("Exposure Mode", modeName[mode])) {
      ErrorTrace("ERROR: Could NOT set the exposure mode of a %s DE image.",
        modeName[mode]);
      return 1;
    } 
    SEMTrace('D', "Exposure mode set to:  %s", modeName[mode]);
  }
  mLastExposureTime = seconds;
  mLastExposureMode = mode;
  return 0;
}

// Toggle the live mode: 0 is off, 1 is on with no thread, 2 is with thread
int DirectElectronCamera::SetLiveMode(int mode)
{
  CSingleLock slock(&m_mutex);
  int status, ind;

  // If the mutex failed (?!), just drop back to no thread
  if (mode > 1 && !sLiveMutex)
    mode = 1;

  if (slock.Lock(1000)) {
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
      if (!mDeServer->setLiveMode(mode > 0)) {
        ErrorTrace("ERROR: Could NOT set the live mode %s.", mode ? "ON" : "OFF");
        return 1;
      } 
      SEMTrace('D', "Live mode turned %s", mode ? "ON" : "OFF");
      mLastLiveMode = mode;
    }
    return 0;
  }
  return 1;
}



///////////////////////////////////////////////////////////////////
//StopAcquistion() routine.  Function will stop the camera
//from continuing the acquistion.  This will also clear out the
//the acquistion frame for future acquires.
///////////////////////////////////////////////////////////////////
void DirectElectronCamera::StopAcquistion()
{
  if (m_DE_CLIENT_SERVER) {
  } else {
    m_LCCaptureInterface->AbortReadout();
    m_LCCaptureInterface->ClearFrame();
    m_STOPPING_ACQUISITION = true;
  }
}



///////////////////////////////////////////////////////////////////
//isImageAcquistionDone() routine.  Routine used to determine when its appropriate to 
// get image, except for DE Server where it does nothing!
///////////////////////////////////////////////////////////////////
bool DirectElectronCamera::isImageAcquistionDone()
{
  if (m_DE_CLIENT_SERVER) {
    return true;

  } else {
    if (m_STOPPING_ACQUISITION == true)
      return true;
    else
      return m_sink->checkDataReady();
  }

  return false;
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
  long param;

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {
      float temp = 10;
      if (!mDeServer->getFloatProperty("Temperature - Detector (Celsius)", &temp)) {
        CString str = ErrorTrace("ERROR: Could NOT get the Temperature of DE camera ");
        //AfxMessageBox(str);
      }
      m_camera_Temperature = temp;

    }

    else {
      m_LCCaptureInterface->get_StatusParam(CAMERA_TEMP, &param);
      m_camera_Temperature = (float)((param / 10) - 273.15);
    }

  }
  return m_camera_Temperature;
}


// Inserts camera and waits until done
int DirectElectronCamera::insertCamera()
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {
     if (!mDeServer->setProperty(psCamPosition, DE_CAM_STATE_INSERTED)) {
        CString str = ErrorTrace("ERROR: Could NOT insert the DE camera ");
        //AfxMessageBox(str);
      } else {
        WaitForInsertionState(DE_CAM_STATE_INSERTED);
        return 0;
      }
    } else //LC1100 camera return
      return 0;
  }
  return 1;
}

// Retracts camera and waits until done
int DirectElectronCamera::retractCamera()
{
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {

    if (!mDeServer->setProperty(psCamPosition, DE_CAM_STATE_RETRACTED)) {
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
  if (!mDeServer->setProperty("Temperature Control", "Warm Up")) {

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

  if (!mDeServer->setProperty("Temperature Control", "Cool Down")) {

    str = ErrorTrace("ERROR: Could COOL DOWN UP the camera.");
    AfxMessageBox(str);
    return S_FALSE;
  } else {
    str.Format("Cooling down the camera. Please wait at least 5 minutes for the "
      "temperature to stabilize...");
    AfxMessageBox(str);

  }
  return hr;
}

// Set the correction mode with the given value
HRESULT DirectElectronCamera::setCorrectionMode(int nIndex)
{
  const char *corrections[3] = {"Uncorrected Raw", "Dark Corrected", 
    "Gain and Dark Corrected"};
  HRESULT hr = S_OK;

  CString str;
  if (nIndex < 0 || nIndex > 2) 
    return S_FALSE;
  if (nIndex != mLastProcessing || !mTrustLastSettings) {
    if (!mDeServer->setProperty("Correction Mode", corrections[nIndex])) {

      str = ErrorTrace("Could not set the Correction mode of the camera.");
      AfxMessageBox(str);
      return S_FALSE;
    }
    mLastProcessing = nIndex;
  }

  return hr;
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
    str2.Format("\r\n   Error code %d%s%s", mDeServer->getLastErrorCode(), 
      stdStr.c_str() != NULL ? ": " : "", stdStr.c_str() != NULL ? stdStr.c_str() : "");
    str += str2;
  }
  SEMTrace('D', "%s", (LPCTSTR)str);
  return str;
}


// Routine to set the frames per second, also takes care of readout delay of 1 frame for
// DE 12 and sets the current value in camera parameters
void DirectElectronCamera::SetFramesPerSecond(double value)
{
  CameraParameters *camP = mCamParams + mWinApp->GetCurrentCamera();
  int readoutDelay = (int)floor(1000. / value);
  CSingleLock slock(&m_mutex);
  if (slock.Lock(1000)) {
    if (mDeServer) {
      justSetDoubleProperty("Frames Per Second", value);
      camP->DE_FramesPerSec = (float)value;
      if (CurrentIsDE12()) {
        justSetIntProperty("Sensor Readout Delay (milliseconds)", readoutDelay);
        SEMTrace('D', "DE12 FPS set to %f, readout delay to %d", value, readoutDelay);
        mReadoutDelay = readoutDelay;
      }
    }
  }
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

// Fill in the metadata for the mdoc file; use existing values where possible
void DirectElectronCamera::SetImageExtraData(EMimageExtra *extra)
{
  CString str;
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
  extra->mReadoutDelay = mReadoutDelay;
  getFloatProperty("Faraday Plate - Last Exposure Peak Density (pA/cm^2)", 
    extra->mFaraday);

  // Autosave path and frames...
  if (mLastSaveFlags & (DE_SAVE_FRAMES | DE_SAVE_SUMS)) {
    getStringProperty("Autosave Frames - Previous Dataset Name", str);
    extra->mSubFramePath = mWinApp->mDEToolDlg.GetAutosaveDir() + "\\" + str;
    str.Format("Autosave %s Frames - Frames Written in Last Exposure", 
      (mLastSaveFlags & DE_SAVE_FRAMES) ? "Raw" : "Sum");
    getIntProperty(str, extra->mNumSubFrames);
  }
}

/////////////////////////////////////////////////////////////////////////////////////
// LC1100 only calls
/////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////
//readCameraProperties() routine.  This function reads in
//the properties that were determined useful for the user
//to manipulate.  See the Spectral4kProperties.txt for the actual
//properties.
//
//
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::readCameraProperties()
{
  if (m_DE_CLIENT_SERVER) {
  } else {
    string  camera_properties[NUM_OF_LINES];
    ifstream inFile;
    inFile.open(FILE_PROPERTIES);

    if (!inFile) {
      AfxMessageBox("Could not read properties file, ensure a properties file exists!", 
        MB_OK | MB_ICONEXCLAMATION);
      return -1;
    }
    if (inFile.is_open()) {


      int i = 0;

      try {
        while (i < NUM_OF_LINES) {
          getline(inFile, camera_properties[i++]);
        }
        inFile.close();
      } catch (...) {
        return -1;
      }
    }

    //should read in :  C:\\camerafiles\\SI1100.dcf
    m_dcf_file = SysAllocString(A2BSTR(camera_properties[0].c_str()));
    //should read in :  C:\\camerafiles\\1100-106 250kHz.set
    m_filename = SysAllocString(A2BSTR(camera_properties[1].c_str()));
    //should read in a C:\\camerafiles\\x4839A+2.str
    m_tablefilename = SysAllocString(A2BSTR(camera_properties[2].c_str()));
    //should read in the port number, typically 3
    m_camera_port_number = atoi(camera_properties[3].c_str());
    m_LCCaptureInterface->put_COMPort(m_camera_port_number);

    //Determine if the TEC should automatically turn on upon init
    //IF "ON" is not found then will proceed to turn off the TEC cooler.

    if (camera_properties[4] == "ON")
      m_LCCaptureInterface->ExternalRelayTurnOn();

    else
      m_LCCaptureInterface->ExternalRelayTurnOff();



    if (camera_properties[5] == "FIX_COLUMNS")
      FIX_COLUMNS = true;
    else
      FIX_COLUMNS = false;


  }
  return 1;
}

// unused
int DirectElectronCamera::setOrigin(int x, int y)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {
    } else
      m_LCCaptureInterface->SetOrigin(x, y);
  }
  return 1;
}



int DirectElectronCamera::reAllocateMemory(int mem)
{
  if (m_DE_CLIENT_SERVER) {
  } else
    m_FSMObject->AllocateShort(mem, 1);
  return 1;
}

unsigned short *DirectElectronCamera::getImageData()
{
  return m_sink->getData();
}

void DirectElectronCamera::setGainSetting(short index)
{

  /*here are the parameters for the three gain settings for camera S/N 142 (University of Queensland):

  Mode     DSI time     Attenuation     Gain     Pixel clock     Noise (electrons approx)
  00          0                0         Low        2000 KHz      15
  01          17               0         Med        1000 KHz       10
  02          67               3         High       500 KHz         7
  */
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (index == MODE_0) {
      m_LCCaptureInterface->put_ReadoutParam(ATTENUATION_PARAM, 0);
      m_LCCaptureInterface->put_ReadoutParam(DSI_TIME, 0);

    } else if (index == MODE_1) {
      m_LCCaptureInterface->put_ReadoutParam(ATTENUATION_PARAM, 0);
      m_LCCaptureInterface->put_ReadoutParam(DSI_TIME, 17);
    } else if (index == MODE_2) {
      m_LCCaptureInterface->put_ReadoutParam(ATTENUATION_PARAM, 3);
      m_LCCaptureInterface->put_ReadoutParam(DSI_TIME, 67);

    } else {
      AfxMessageBox("Could not properly set a mode for the Gain Setting. Please recheck your selection");
      return;

    }
    //update the camera with the latests readout Parameters.

    SEMTrace('D', "DE_CAMERA: about to readout form the camera.");
    m_LCCaptureInterface->ReadoutParam_RecAllFromHost();
    SEMTrace('D', "DE_CAMERA: done reading out form the camera.");

    //read out the configs.
    m_LCCaptureInterface->ConfigParam_TransToHost();
    long atten;
    long dsi_time;
    m_LCCaptureInterface->get_ReadoutParam(ATTENUATION_PARAM, &atten);
    m_LCCaptureInterface->get_ReadoutParam(DSI_TIME, &dsi_time);

    SEMTrace('D', "DE_CAMERA: after sending commands to the camera reading back these are the results for mode %d:  attenuation_param %d and DSI_TIME %d", index, atten, dsi_time);
  }
}

///////////////////////////////////////////////////////////////////
//getCameraBackplateTemp() routine.  Function will return the current
//backplate Temperature of the camera in degrees Celcius.
///////////////////////////////////////////////////////////////////
float DirectElectronCamera::getCameraBackplateTemp()
{
  long param;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {
    if (m_DE_CLIENT_SERVER) {

    } else {
      m_LCCaptureInterface->get_StatusParam(BACK_PLATE_TEMP, &param);
      m_camera_BackPlateTemp = (float)((param / 10) - 273.15);
    }

  }

  return m_camera_BackPlateTemp;
}


///////////////////////////////////////////////////////////////////
//getCameraPressure() routine.  Function will return the pressure.
///////////////////////////////////////////////////////////////////
int DirectElectronCamera::getCameraPressure()
{
  long param;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {

    } else {
      m_LCCaptureInterface->get_StatusParam(CAMERA_PRESSURE, &param);
      m_camera_Pressure = (long) param;
    }
  }

  return m_camera_Pressure;
}

///////////////////////////////////////////////////////////////////
//readinTempandPressure() routine.  This function reads in
//the properties of the temperature and pressure. Simple
//conversion is used to convert to celcius for the temp.
//
//
///////////////////////////////////////////////////////////////////
void DirectElectronCamera::readinTempandPressure()
{
  long param;

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    //read out the latest values from the camera
    m_LCCaptureInterface->ConfigParam_TransToHost();
    m_LCCaptureInterface->Status_TransToHost();

    m_LCCaptureInterface->get_StatusParam(CAMERA_TEMP, &param);
    m_camera_Temperature = (float)((param / 10) - 273.15);
    m_LCCaptureInterface->get_StatusParam(BACK_PLATE_TEMP, &param);
    m_camera_BackPlateTemp = (float)((param / 10) - 273.15);
    m_LCCaptureInterface->get_StatusParam(CAMERA_PRESSURE, &param);
    m_camera_Pressure = (long)param;
    m_LCCaptureInterface->get_ConfigParam(PRE_EXPOSE_DELAY, &param);
    m_pre_expose_delay = param;
    m_LCCaptureInterface->get_ConfigParam(SHUTTER_DELAY, &param);
    m_shutter_close_delay = (int)param;

    //m_CaptureInterface->Status_TransToHost();

  }
}


long DirectElectronCamera::getValue(int i)
{

  long param;

  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {

    } else
      m_LCCaptureInterface->get_ConfigParam(i, &param);

  }

  return param;
}

void DirectElectronCamera::setShutterDelay(int delay)
{
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {
    if (m_DE_CLIENT_SERVER) {

    } else {
      m_LCCaptureInterface->ConfigParam_TransToHost();
      m_LCCaptureInterface->put_ConfigParam(SHUTTER_DELAY, delay);
      m_LCCaptureInterface->ConfigParam_RecAllFromHost();
    }
  }
}

long DirectElectronCamera::getInstrumentModel()
{
  return m_instrument_model;
}

long DirectElectronCamera::getSerialNumber()
{
  return m_head_serial;
}

long DirectElectronCamera::getShutterDelay()
{
  return m_shutter_close_delay;
}

long DirectElectronCamera::getPreExposureDelay()
{

  long param;
  CSingleLock slock(&m_mutex);

  if (slock.Lock(1000)) {

    if (m_DE_CLIENT_SERVER) {

    } else {
      m_LCCaptureInterface->ConfigParam_TransToHost();
      m_LCCaptureInterface->get_ConfigParam(PRE_EXPOSE_DELAY, &param);
      m_pre_expose_delay = param;
    }

  }
  return m_pre_expose_delay;
}

float DirectElectronCamera::getCCDSetPoint()
{
  return m_ccd_temp_setPoint;
}

long DirectElectronCamera::getWindowHeaterStatus()
{
  return m_window_heater;
}




int DirectElectronCamera::turnWindowHeaterON()
{
  if (m_LCCaptureInterface && !(m_DE_CLIENT_SERVER)) {
    //read out all the config params
    m_LCCaptureInterface->ConfigParam_TransToHost();
    //make sure window heater is on
    m_LCCaptureInterface->put_ConfigParam(WINDOW_HEATER, 1);
    //update the camera
    m_LCCaptureInterface->ConfigParam_RecAllFromHost();


    AfxMessageBox("You have turned the window heater ON!");

    return 1;

  } else
    return 0;


}

int DirectElectronCamera::turnWindowHeatherOFF()
{
  if (m_LCCaptureInterface && !(m_DE_CLIENT_SERVER)) {
    //read out all the config params
    m_LCCaptureInterface->ConfigParam_TransToHost();
    //make sure window heater is on
    m_LCCaptureInterface->put_ConfigParam(WINDOW_HEATER, 0);
    //update the camera
    m_LCCaptureInterface->ConfigParam_RecAllFromHost();


    AfxMessageBox("You have turned the window heater OFF!");

    return 1;

  } else
    return 0;

}


int DirectElectronCamera::turnTEC_ON()
{

  //trigger the peltier device to turn on.
  if (m_LCCaptureInterface && !(m_DE_CLIENT_SERVER)) {
    m_LCCaptureInterface->ExternalRelayTurnOn();
    AfxMessageBox("You have turned the TEC ON!");
    return 1;
  } else
    return 0;
}

int DirectElectronCamera::turnTEC_OFF()
{
  //trigger the peltier device to turn off.
  if (m_LCCaptureInterface && !(m_DE_CLIENT_SERVER)) {
    m_LCCaptureInterface->ExternalRelayTurnOff();
    AfxMessageBox("You have turned the TEC OFF!");
    return 1;
  } else
    return 0;

}
