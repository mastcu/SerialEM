#pragma once

#include <fstream>
#include <string>
#include <stdio.h>
#include <vector>  

using namespace std;

#include "../../../CameraController.h"

#import "CameraLink.tlb" no_namespace named_guids raw_interfaces_only
#import "FSM_MEMORYLib.tlb" no_namespace named_guids raw_interfaces_only
//#include "FSM_MemoryLib.tlh"
//import "FSM_MemoryLib.tlb" no_namespace named_guids raw_interfaces_only
//#import "Remap8K.tlb" no_namespace named_guids raw_interfaces_only
//#import "FSM_MultiImageObjectLib.tlb" no_namespace named_guids raw_interfaces_only

//#import "../EMShutter.tlb" no_namespace named_guids raw_interfaces_only
//#include "EMShutter.tlh"
//IShutterBoxPtr ptrShutter;

#define MAX_IMAGE_FIELD (7992*7992)
//#define MAX_INDIVIDUAL_IMAGE_SIZE (3996*3996)
//#define MAX_HEIGHT 3996
//#define MAX_WIDTH 3996
#define MAX_INDIVIDUAL_IMAGE_SIZE (4096*4096)
#define MAX_HEIGHT 4096
#define MAX_WIDTH 4096


#define FLATFIELD_8k 0
#define DARK_8k 1
#define CORRECTED_8k 2
#define UNPROCESSED_8k 3

//defines relatied to remapping data
#define HORIZONTAL_STITCH 0
#define VERTICAL_STITCH 1
#define LEFT_SIDE_PATCH 1
#define RIGHT_SIDE_PATCH 2
#define TEMPLATE_X1 20
#define TEMPLATE_X2 TEMPLATE_X1 + 110
#define TEMPLATE_Y1 512
#define TEMPLATE_Y2 2

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

#define LOW_DSI_TIME 12
#define MED_DSI_TIME 49
#define HIGH_DSI_TIME 149

#include "CImg.h"
using namespace cimg_library;
#include "Sink8k.h"
#include "Sink8kCam2.h"
#include "Sink8kCam3.h"
#include "Sink8kCam4.h"

#include "afxmt.h"


struct RemapCoords
{
	int XUL;
	int YUL;
	int XUR;
	int YUR;
	int XLL;
	int YLL;
	int XLR;
	int YLR;
};

struct CrossCorrOffSets
{
	int x_Offset;
	int y_Offset;

};

class NCMIR8kCamera
{
public:
	NCMIR8kCamera(void);
	~NCMIR8kCamera(void);
	int initialize();
	int readCameraProperties();
	int readRemapProperties();
	void readinTempandPressure();
	int setBinning(int x, int y, int sizeX, int sizeY);
	int setImageSize(int sizeX, int sizeY);
	void readCameraParams();
	int copyImageData(unsigned short *image8k, int ImageSizeX, int ImageSizeY, int LeftC, int TopC, int BottomC, int RightC);
	float* getCameraTemp();
	float* getCameraBackplateTemp();
	long* getCameraPressure();
	long* getCamVoltage();
	void windowHeaterOff();
	void windowHeaterOn();
	long* getInstrumentModel();
    long* getSerialNumber();
    long* getShutterDelay();
    long* getPreExposureDelay();
    float* getCCDSetPoint();
    long* getWindowHeaterStatus();
	long* getDSITime();
	void setShutter(IShutterBoxPtr shutter);
	int AcquireImage(float seconds);
	int AcquireDarkImage(float seconds);
	void StopAcquistion();
	bool isImageAcquistionDone();
	int setOrigin(int x, int y);
	int reAllocateMemory(int mem);
	int setDebugMode();
	int uninitialize();
	int remapData(unsigned short* array, int ImageSizeX, int ImageSizeY);
	void applyCropPad(int xPad, int yPad);
	void setGainSetting(short index);
	void SaveImages(CString path,CString basename,long ccdbID,int sectionNumber, float TiltAngle,unsigned short* image,int sizeX,int sizeY, double meanCounts);	
	void saveDarkFlats();
	void setImageType(int type);
	void setPathtoGainImages(CString path);
	void setApachePath(CString ApachePath,CString ApacheURL);
	float getMeanFlatFieldValue();
	CString getFlatFieldLocation();
	void CalcTrueMean();
	CString getURLPath(){return mURLPath;}
	void setMetaDataThumbNail(double x,double y,double z,double defocus,CString magIndex,double TiltAngle);
	void setTiltSeriesInfo(BOOL isTiltSeries);
	void setMontageInfo(BOOL isMontage, int xNframes, int yNframes);
	void setRemapImages(int state){mRemapState = state;}
	//selecting the proper camera
	void setTargetFocusCam(int nIndex){m_TargetFocusCamera = nIndex;};
	int getTargetFocusCam(){return m_TargetFocusCamera;};
	void setTargetTrialCam(int nIndex){m_TargetTrialCamera = nIndex;};
	int getTargetTrialCam(){return m_TargetTrialCamera;};
	//set the mode of the current camera:  Record, Trail, Focus etc
	//what mode are we in?			
	//1==FOCUS, 2==TRIAL, 3==RECORD.
	void setImageMode(int inSet){m_ImageMode = inSet;};
	int getImageMode(){return m_ImageMode;};

private:

	unsigned short *m_pCapturedDataShort[4];
	BSTR filename[4];
	BSTR tablefilename[4];
	BSTR dcf_file[4];
	int camera_port_number[4];
	string peltier_cooler[4];
	ICameraCtrl *m_CaptureInterface[4];
	IFSM_MemoryObject *m_FSMObject[4];
	long m_CameraPressures[4];
	float m_CameraTemperatures[4];
	float m_Camera_BackPlateTemp[4];
	//Configuration Parameters
	long m_instrument_model[4];
	long m_head_serial[4];
	long m_shutter_close_delay[4];
	long m_pre_expose_delay[4];
	float m_ccd_temp_setPoint[4];
	long m_window_heater[4];
	long m_camera_Voltage[4];
	long m_dsi_time[4];
	Sink8k* m_SinkCam1;
	Sink8kCam2* m_SinkCam2;
	Sink8kCam3* m_SinkCam3;
	Sink8kCam4* m_SinkCam4;
	int debug_mode;
	//extra remapping code
	//ICoRemap8K *RemapInterface;
	//ISimpleImage *m_SimpleImage[4];
	unsigned long *m_pRemapData[4];
	RemapCoords camCoords[4];
	CrossCorrOffSets offset[3];
	CrossCorrOffSets a[3];
	//done to ensure proper cropping
	int XPAD;
	int YPAD;
	int mbinFactor;
	bool STOPPING_ACQUSITION;
	bool ACQUIRING_DATA;
	float mflat_avg_value; 
	float mflat_avg_value_2x;
	float mflat_avg_value_4x;
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
	int mRemapState;  
	IShutterBoxPtr shutter;
	int m_TargetFocusCamera;
	int m_TargetTrialCamera;
	int m_ImageMode;


	CString mflatFieldPath;
	CMutex m_mutex;
};
