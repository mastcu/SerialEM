#pragma once

///////////////////////////////////////////////////////////////////
// file:  DirectElectronCamera.h
//
// This is the header file that defines the functions that 
// will be implemented Spectral4kCamera.cpp.  The functions
// wrap the estential 
// 
//
// author of SerialEM: David Masternarde
// 
// LC1100 camera integration by Direct Electron
// 2009
// 
///////////////////////////////////////////////////////////////////


// DNM: switch to include fixed tlh until the tlb can be updated properly

#include "soliosfor4000.tlh"
//#import "SoliosFor4000.tlb" no_namespace named_guids raw_interfaces_only
#include "fsm_memorylib.tlh"
//#import "FSM_MEMORYLib.tlb" no_namespace named_guids raw_interfaces_only
#include "LC1100CamSink.h"
#include <map>

struct DEPluginFuncs;

// Data for live mode thread
struct LiveThreadData {
  DEPluginFuncs *DeServer;
  unsigned short *buffers[2];
  unsigned short *rotBuf;
  bool returnedImage[2];
  int outBufInd;
  int inSizeX, inSizeY;
  int operation;
  int divideBy2;
  bool quitLiveMode;
  int electronCounting;
};


//This was discovered to be the appropriate origin size for
//when the camera was in bin 4 mode.  
#define ORIGIN_X_BIN4 48
#define ORIGIN_Y_BIN4 0

// The camera only supports binning 2 and 4.  
// For binning 1 and 2 uses the origin x,y of 50,0.
#define ORIGIN_X 50

// The following image offsets are based on what was
// determined and defined in the .dcf file
// For binning 1 and 2 uses the origin x,y of 50,0.
#define IMAGE_X_OFFSET 2048
#define IMAGE_Y_OFFSET 2056


// The max size of the chip is
// 4112 x 4096 but I have rotated it 
// so the image looks like what you see on the
// phosphor screen.  Therefore the width and height 
// are defined as such.
#define MAX_IMAGE_WIDTH 4096
#define MAX_IMAGE_HEIGHT 4112
#define MAX_IMAGE_SIZE (4096*4112)

//Definitions of the registers
#define PRE_EXP_DELAY 12
//Definition of attenuation/sensitivity gain setting
#define ATTENUATION_PARAM 11
#define DSI_TIME 10
#define CAMERA_TEMP 0
#define BACK_PLATE_TEMP 1 
#define CAMERA_PRESSURE 3

//shutter close delay
#define SHUTTER_DELAY 11
#define WINDOW_HEATER 18
#define INSRUMENT_MODEL 0
#define SERIAL_HEAD 1
#define SHUTTER_CLOSE_DELAY 11
#define PRE_EXPOSE_DELAY 12
#define CCD_TEMP_SETPOINT 14

// Defines for normal, gain, and dark; gain does not work in a call to set mode
#define DE_GAIN_IMAGE 2
#define DE_DARK_IMAGE 1
#define DE_NORMAL_IMAGE 0
#define DE_COOL_DOWN 1
#define DE_WARM_UP 0

// Strings needed in multiple places
#define DE_LCNAME "LC1100"
#define DE_CAM_STATE_RETRACTED "Retracted"
#define DE_CAM_STATE_INSERTED "Extended"
#define DE_KEEP_COVER_OPEN "Always Open During Operation"
#define DE_OPEN_CLOSE_COVER "Open/Close During Each Exposure"
#define DE_PROP_AUTOSAVE_DIR "Autosave Directory"

// DE server versions for capabilities
// Version 2 moved to major, minor, rev, build so numbers are 100 times bigger
#define DE_CAN_SAVE_MRC           1000715
#define DE_HAS_REPEAT_REF         1000768
#define DE_SUFFIX_KEEPS_PT      999999999
#define DE_CAN_SET_FOLDER         1000943
#define DE_ALL_NORM_IN_SERVER     1001243
#define DE_ABORT_WORKS          999999999
#define DE_HAS_NEW_MOT_COR_PROP 201000000
#define DE_HAS_AUTO_PIXEL_DEPTH 203000000
#define DE_HAS_MOVIE_COR_ENABLE 203000000
#define DE_ROI_IS_ON_CHIP       201150000
#define DE_HAS_ONE_GAIN_STATUS  201000000
#define DE_NO_COUNTING_PREFIX   201000000
#define DE_AUTOSAVE_RENAMES1    203000000
#define DE_RETURNS_FRAME_PATH   205000000
#define DE_HAS_API2             205000000
#define DE_DETECTOR_TEMP_IS_INT 205380000
#define DE_AUTOSAVE_RENAMES2    207000000

//The following define the different
//gain/dsi setting modes for the LC1100
#define MODE_0 0
#define MODE_1 1
#define MODE_2 2

#include "afxmt.h"

class DirectElectronCamera
{
public:
	DirectElectronCamera(int camType,int index);
	~DirectElectronCamera(void);
	int initialize(CString camName, int camIndex);
	int readCameraProperties();
	void readinTempandPressure();
	int setBinning(int x, int y, int sizex, int sizey, int hardwareBin);
	int AcquireImage(float seconds);
	int AcquireDarkImage(float seconds);
	void StopAcquistion();
	int setPreExposureTime(double preExpSeconds);
	bool isImageAcquistionDone();
	unsigned short* getImageData();
	int setOrigin(int x, int y);
	int reAllocateMemory(int mem);
	int setDebugMode();
	int unInitialize();
	int copyImageData(unsigned short *image4k, long &ImageSizeX, long &ImageSizeY, 
    int divideBy2);
	float getCameraTemp();
	float getCameraBackplateTemp();
	int getCameraPressure();
	void setGainSetting(short index);
	long getInstrumentModel();
    long getSerialNumber();
    long getShutterDelay();
    long getPreExposureDelay();
    float getCCDSetPoint();
    long getWindowHeaterStatus();
	void setShutterDelay(int delay);
	long getValue(int i);
	int turnWindowHeaterON();
	int turnWindowHeatherOFF();
	int turnTEC_ON();
	int turnTEC_OFF();

	//functions below added for DE12 and future
	int initDEServer();
	int initializeDECamera(CString camName, int camIndex);
  void FinishCameraSelection(bool initialCall, CameraParameters *camP);
	int setROI(int offset_x, int offset_y, int xsize, int ysize, int hardwareROI);
  int SetLiveMode(int mode);
	void setCameraName(CString name);
	int insertCamera();
	int retractCamera();
	HRESULT setAutoSaveRaw(int state);
	HRESULT setAutoSaveSummedImages(int state);
	float getWaterLineTemp();
	float getTECValue();
	float getFaradayPlateValue();
	float getColdFingerTemp();
	HRESULT WarmUPCamera();
	HRESULT CoolDownCamera();
	int setCorrectionMode(int mode, int readMode);
	CString getCameraInsertionState();
	CString getCurrentCameraName(){return m_DE_CurrentCamera;};
  BOOL CurrentIsDE12();
  BOOL CurrentIsSurvey();
	bool getIntProperty(CString name, int &value);
	bool getFloatProperty(CString name, float &value);
	bool getStringProperty(CString name, CString &value);
	void setIntProperty(CString name,int value);
	void setStringProperty(CString name,CString value);
	void setDoubleProperty(CString name,double value);
  bool setStringWithError(const char *name, const char *value);
	int SetFramesPerSecond(double value);
  int SetAllAutoSaves(int autoSaveFlags, int sumCount, CString suffix, CString saveDir, 
    bool counting);
  int OperationForRotateFlip(int DErotation, int flip);
  void WaitForInsertionState(const char *state);
  int IsCameraInserted();
  void InitializeLastSettings();
  int SetExposureTimeAndMode(float seconds, int mode);
  int SetCountingParams(int readMode, double scaling, double FPS);
  int SetAlignInServer(int alignFrames);
  void SetImageExtraData(EMimageExtra *extra, float nameTimeout, bool allowNamePrediction,
    bool &nameValid);
  GetMember(bool, _DE_CONNECTED);
  GetMember(int, CamType);
  GetMember(int, InitializingIndex);
  GetMember(int, ServerVersion);
  GetMember(bool, API2Server);
  GetMember(int, LastLiveMode);
  GetMember(int, CurCamIndex);
  GetMember(bool, NormAllInServer);
  GetMember(int, NumLeftServerRef);
  GetSetMember(CString, PathAndDatasetName);
  GetSetMember(CString, LastErrorString);
  void SetAndTraceErrorString(CString str);

  bool ServerIsLocal() {return mDE_SERVER_IP == "127.0.0.1";};
  void SetupServerReference(int repeat, int mode) {mRepeatForServerRef = repeat;
    mModeForServerRef = mode; mNumLeftServerRef = repeat;};
  bool justSetDoubleProperty(const char * propName, double value);
  bool justSetIntProperty(const char * propName, int value);
  CString ErrorTrace(char *fmt, ...);
  void SetSoftwareAndServerVersion(std::string & propValue);
  bool IsApolloCamera();
  bool CheckMapForValidState(int checksum, std::map<int, int> &map, int minuteNow);
  void AddValidStateToMap(int checksum, std::map<int, int> &map, int minuteNow);
  bool IsReferenceValid(int checksum, std::map<int, int> &map, int minuteNow, 
    const char *propStr, const char *messStr);
  bool GetPreviousDatasetName(float timeout, int ageLimitSec, bool predictName, CString &name);
  void FormatFramesWritten(BOOL saveSums, CString &str);
  void VerifyLastSetNameIfPredicted(float timeout);
  float GetLastCountScaling();

private:
	int mCamType;
  int mCurCamIndex;  // Index of current or last selected camera
	//determine if we are using the DE client/server model
	bool m_DE_CLIENT_SERVER;
	unsigned short *m_pCapturedDataShort;
	BSTR m_filename, m_tablefilename, m_dcf_file;
  CSerialEMApp *mWinApp;
  CameraParameters *mCamParams;
  static UINT LiveProc(LPVOID pParam);

	//DE properties
	//added for client/server model.
  DEPluginFuncs *mDeServer;
	ICameraCtrl *m_LCCaptureInterface;
	IFSM_MemoryObject *m_FSMObject;
	CLC1100CamSink* m_sink;

	int m_camera_port_number;
	float m_camera_Temperature;
	long m_camera_Pressure;
	float m_camera_BackPlateTemp;
	short m_debug_mode;
	int m_binFactor;
	bool m_STOPPING_ACQUISITION;

	//Configuration Parameters
	long m_instrument_model;
	long m_head_serial;
	long m_shutter_close_delay;
	long m_pre_expose_delay;
	float m_ccd_temp_setPoint;
	long m_window_heater;

	CMutex m_mutex;

	//DE server settings.
	bool m_DE_CONNECTED;
	CString m_DE_CurrentCamera;
  CWinThread *mLiveThread;
  LiveThreadData mLiveTD;

	CString mDE_SERVER_IP;
	int mDE_WRITEPORT;
	int mDE_READPORT;

  // State variables: last values set in server.  Keep in same order as in init function
  int mLastSaveFlags;                   // Flags for types of frames to save
  int mLastSumCount;                    // Sum count for saving summed frames
  CString mLastSuffix, mLastSaveDir;    // Last filename components and directory
  int mLastXoffset, mLastYoffset;       // Offset set in ROI routine
  int mLastROIsizeX, mLastROIsizeY;     // Size set in ROI routine
  int mLastXimageSize, mLastYimageSize; // Image sizes (unused)
  int mLastXbinning, mLastYbinning;     // Binnning values (should be the same)
  int mLastExposureMode;                // Normal, Dark, Gain
  float mLastExposureTime;              // Exposure time
  double mLastPreExposure;              // Pre-exposure
  int mLastProcessing;                  // uncorrected, dark, or dark/gain (linear mode)
  int mLastMovieCorrEnabled;            // Whether movie correction was enabled
  int mLastNormCounting;                // Post-counting normalization of saved frames
  int mLastUnnormBits;                  // Bits output in unnormalized movie
  int mLastElectronCounting;            // Electron counting of either kind is 1, test > 0
  int mLastSuperResolution;             // Super-resolution is a separate flag
  float mLastFPS;                       // Frames per second
  int mLastLiveMode;                    // Whether live mode was one
  int mLastServerAlign;                 // Motion correction
  int mLastUseHardwareBin;              // Hardware binning
  int mLastUseHardwareROI;              // Hardware ROI
  int mLastAutoRepeatRef;               // Auto repeat reference enable setting
  BOOL mTrustLastSettings;              // Whether to trust all those values in server

  float mCountScaling;                  // Scaling for counting mode
  int mElecCountsScaled;              // Factor by which counts are scaled when received
  int mReadoutDelay;                    // Milliseconds of readout delay set
  bool mNormAllInServer;                // Whether to normalize all images in server
  CString mSoftwareVersion;             // String for server version
  CString mDE12SensorSN;
  int mServerVersion;                   // 1000000 * major + 10000 * middle + minor
  bool mAPI2Server;                     // A common fact to want to test
  int mInitializingIndex;               // Index of camera being initialized
  int mRepeatForServerRef;              // Number of repeats for doing reference in server
  int mModeForServerRef;                // Processing mode to set for server reference
  int mNumLeftServerRef;                // Running tally of number left to do
  int mDateInPrevSetName;               // 20yymmdd part of set name
  int mNumberInPrevSetName;             // Sequential number part of set name
  double mTimeOfPrevSetName;            // Tick time when last valid set name was gotten
  int mSetNamePredictionAgeLimit;       // Number of sec that a predicted name is valid
  CString mLastPredictedSetName;        // Store name for checking when it is predicted
  CString mPathAndDatasetName;          // The root for all output files
  CString mLastErrorString;             // Error string stored for CamController to access

  // Maps of acquisition parameter checksum and minute time stamp at which validated
  std::map<int, int> mDarkChecks, mGainChecks, mCountGainChecks, mExpFPSchecks;

	CString m_Camera_InsertionState;      // Current description of state

	//added to support flexibilty of rotating the image and inverting the X axis.
	int m_DE_ImageRot;		   // Provide an angle to rotate the DE image 
	int m_DE_ImageInvertX;     // Decides whether to invert the X axis on the DE image 1==invert.

};
