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

#define DE_LCNAME "LC1100"
#define DE_CAM_STATE_RETRACTED "Retracted"
#define DE_CAM_STATE_INSERTED "Extended"
#define DE_KEEP_COVER_OPEN "Always Open During Operation"
#define DE_OPEN_CLOSE_COVER "Open/Close During Each Exposure"

#define DE_CAN_SAVE_MRC 1000715
#define DE_HAS_REPEAT_REF 1000768
#define DE_SUFFIX_KEEPS_PT 9999999

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
	int setBinning(int x, int y, int sizex, int sizey);
	int setImageSize(int sizeX, int sizeY);
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
	int copyImageData(unsigned short *image4k, int ImageSizeX, int ImageSizeY, 
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
  void FinishCameraSelection(bool initialCall);
	int setROI(int offset_x, int offset_y, int xsize, int ysize);
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
	HRESULT setCorrectionMode(int mode);
	void setTotalFrames(int frames);
	CString getCameraInsertionState();
	CString getPreviousDataSetName();
	CString getNextDataSetName();
	CString getCurrentCameraName(){return m_DE_CurrentCamera;};
  BOOL CurrentIsDE12();
  BOOL CurrentIsSurvey();
	int setAutoSaveSumCount(int frameCount);
	int getAutoSaveSumCount();
	int setAutoSaveSumIgnoreFrames(int frameCount);
	int getAutoSaveSumIgnoreFrames();
	bool getIntProperty(CString name, int &value);
	bool getFloatProperty(CString name, float &value);
	bool getStringProperty(CString name, CString &value);
	void setIntProperty(CString name,int value);
	void setStringProperty(CString name,CString value);
	void setDoubleProperty(CString name,double value);
	void SetFramesPerSecond(double value);
  int SetAllAutoSaves(int autoSaveFlags, int sumCount, CString suffix);
  int OperationForRotateFlip(int DErotation, int flip);
  void WaitForInsertionState(const char *state);
  int IsCameraInserted();
  void InitializeLastSettings();
  int SetExposureTimeAndMode(float seconds, int mode);
  void SetImageExtraData(EMimageExtra *extra);
  GetMember(int, CamType);
  GetMember(int, InitializingIndex);
  GetMember(int, ServerVersion);
  GetMember(int, LastLiveMode);
  GetMember(int, CurCamIndex);
  bool justSetDoubleProperty(const char * propName, double value);
  bool justSetIntProperty(const char * propName, int value);
  CString MakeErrorString(CString str);
  CString ErrorTrace(char *fmt, ...);

/*#ifdef NCMIR_CCDB
	//void SaveImage(CString path, CString basename,int sectionNumber);
	void getCCDBID();
	void saveResizeImage(CString path, CString basename,int sectionNumber);
	CString getURLPath(){return mURLPath;}
	void setApachePath(CString ApachePath,CString ApacheURL);
	void setMetaDataThumbNail(double x,double y,double z,double defocus,CString index,double TiltAngle);
	void SaveThumNailImage(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle,unsigned short* image,int sizeX,int sizeY, double meanCounts);
	void setTiltSeriesInfo(BOOL isTiltSeries);	
	void setMontageInfo(BOOL isMontage, int xNframes, int yNframes);

	void setApachePath(CString apachepath){mApachePath = apachepath;}	
	void setApacheUrl(CString apacheurl){mApacheURL = apacheurl;}	
	CString getApachePath(){return mApachePath;}	
	CString getApacheUrl(){return mApacheURL;}
	int readWebProperties();
	//special metadata file used for rysnc and CCDB to extract info
	void writeOutCCDBmetaData(CString inFilename, CString Basename, int CCDBid);
#endif

*/

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

	//added for DE12 support
	bool m_DE_CONNECTED;
	CString m_DE_CurrentCamera;
  CWinThread *mLiveThread;
  LiveThreadData mLiveTD;

	//DE settings.
	CString mDE_SERVER_IP;
	int mDE_WRITEPORT;
	int mDE_READPORT;
  int mLastSaveFlags;
  int mLastSumCount;
  CString mLastSuffix;
  int mLastXoffset, mLastYoffset;
  int mLastROIsizeX, mLastROIsizeY;
  int mLastXimageSize, mLastYimageSize;
  int mLastXbinning, mLastYbinning;
  int mLastExposureMode;
  float mLastExposureTime;
  int mLastProcessing;
  int mLastLiveMode;
  BOOL mTrustLastSettings;
  double mLastPreExposure;
  int mReadoutDelay;
  CString mSoftwareVersion;
  CString mDE12SensorSN;
  int mServerVersion;
  int mInitializingIndex;

	CString m_Camera_InsertionState;
	CString m_DE_PreviousDataSet;
	CString m_DE_NextDataSet;

	//added to support flexibilty of rotating the image and inverting the X axis.
	int m_DE_ImageRot;		   // Provide an angle to rotate the DE image 
	int m_DE_ImageInvertX;     // Decides whether to invert the X axis on the DE image 1==invert.

/*#ifdef NCMIR_CCDB
	CString mApachePath;
	CString mApacheURL;
	CString mCCDBInstName;
	CString mURLPath;

	CString mTxBrServerName;
	int mTxBrPortNumber;	
	int mTxBrInstName;
	CString mTxBrPathDir;
	CString mTxBrDataJailPath;
	CString mlocalDatajailpath;

	double mCurrx;
	double mCurry;
	double mCurrz;
	double mCurrdefocus;
	CString mCurrMagindex;
	double mCurrTiltAngle;
	BOOL misMontage;
	int mXNframes;
	int mYNframes;
	BOOL misTiltSeries;
	long m_CCDBid;

#endif
*/

};
