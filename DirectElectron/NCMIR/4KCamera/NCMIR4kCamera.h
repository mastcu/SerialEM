#pragma once

#import "CameraLink.tlb" no_namespace named_guids raw_interfaces_only
//#import "FSM_MEMORYLib.tlb" no_namespace named_guids raw_interfaces_only
#include "FSM_MEMORYLib.tlh"

#import "../../../EMShutter.tlb" no_namespace named_guids raw_interfaces_only


#define NCMIR_4k_LENS_COUPLED 1
#define NCMIR_8k_LENS_COUPLED 2
#define MAX_IMAGE_WIDTH 4096
#define MAX_IMAGE_HEIGHT 4096
#define MAX_IMAGE_SIZE (4096*4096)


#define NUM_OF_LINES 5
#define FILE_PROPERTIES "C:\\Program Files\\SerialEM\\NCMIRCamProperties.txt"

#define CROP_SIZE 3500
#define WINDOW_HEATER 18
#define INSRUMENT_MODEL 0
#define SERIAL_HEAD 1
#define SHUTTER_CLOSE_DELAY 11
#define PRE_EXPOSE_DELAY 12
#define CCD_TEMP_SETPOINT 14
#define ATTENUATION_PARAM 11
#define CAMERA_VOLTAGE 14
#define CAMERA_DSI_TIME 10

#define CAMERA_TEMP 0
#define BACK_PLATE_TEMP 1 
#define CAMERA_PRESSURE 2


#define CAMERA_PRESSURE_CHECK 12000
#define CAMERA_TEMPERATURE_CHECK 25
#define CHILLER_FLOW_RATE_CHECK .5

#define LOW_DSI_TIME 12
#define MED_DSI_TIME 49
#define HIGH_DSI_TIME 149



#include "NCMIR4kSink.h"



#include "CImg.h"
using namespace cimg_library;

#include "afxmt.h"


class NCMIR4kCamera
{
public:
	NCMIR4kCamera(void);
	~NCMIR4kCamera(void);
	int initialize();
	int readCameraProperties();
	void readCameraParams();
	int setBinning(int x, int y, int sizeX, int sizeY);
	int setImageSize(int sizeX, int sizeY);
	int AcquireImage(float seconds);
	int AcquireDarkImage(float seconds);
	void StopAcquistion();
	bool isImageAcquistionDone();
	unsigned short* getImageData();
	int setOrigin(int x, int y);
	int reAllocateMemory(int mem);
	int setDebugMode();
	int uninitialize();
	int copyImageData(unsigned short *image4k, int ImageSizeX, int ImageSizeY);
	float getCameraTemp();
	float getCameraBackplateTemp();
	int getCameraPressure();
	long getCamVoltage();
	void windowHeaterOff();
	void windowHeaterOn();

	//tells the difference between the 4k camera vs the 8k camera
	void setCamType(int TYPE);

	void setGainSetting(short index);
	CString getCurrentGainSetting();

	long getInstrumentModel();
    long getSerialNumber();
    long getShutterDelay();
    long getPreExposureDelay();
    float getCCDSetPoint();
    long getWindowHeaterStatus();
	long getDSITime();
	CString getURLPath(){return mURLPath;}
	void setApachePath(CString ApachePath,CString ApacheURL);
	void setMetaDataThumbNail(double x,double y,double z,double defocus,CString index,double TiltAngle);
	void SaveThumNailImage(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle,unsigned short* image,int sizeX,int sizeY, double meanCounts);
	void setTiltSeriesInfo(BOOL isTiltSeries);
	void setMontageInfo(BOOL isMontage, int xNframes, int yNframes);

	void setShutterBox(IShutterBoxPtr shutterBox){shutter = shutterBox;};
private:
	unsigned short *m_pCapturedDataShort;
	BSTR filename,tablefilename, dcf_file;
	ICameraCtrl *m_CaptureInterface;
	IFSM_MemoryObject *m_FSMObject;
	int m_Camera_Pressure;
	float m_CameraTemperature;
	CNCMIR4kSink* sink;
	int camera_port_number;
	float camera_Temperature;
	float camera_Pressure;
	float camera_BackPlateTemp;
	short debug_mode;
	int binFactor;
	bool STOPPING_ACQUSITION;
	bool ACQUIRING_DATA;

	IShutterBoxPtr shutter;

	//Configuration Parameters
	long instrument_model;
	long head_serial;
	long shutter_close_delay;
	long pre_expose_delay;
	float ccd_temp_setPoint;
	long window_heater;
	long camera_Voltage;
	long dsi_time;

	//determine the camera type
	short m_CamType;
	CString mApachePath;
	CString mApacheURL;
	CString mURLPath;
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

	CMutex m_mutex;
	
	//CCriticalSection cameraLock;
	
};
