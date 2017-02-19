#pragma once


#import "CameraLink.tlb" no_namespace named_guids raw_interfaces_only

#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>


#define cimg_use_tiff 
#include "CImg.h"
using namespace cimg_library;

#define FLATFIELD_8k 0
#define DARK_8k 1
#define CORRECTED_8k 2
#define UNPROCESSED_8k 3

extern _ATL_FUNC_INFO OnCurrentStatusInfo8kCam3,OnImageCapturedInfo8kCam3,OnCaptureAvailableInfo8kCam3;


class Sink8kCam3 : public IDispEventSimpleImpl<1,Sink8kCam3,&DIID__ICameraCtrlEvents>
{
public:

	BEGIN_SINK_MAP(Sink8kCam3)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 1, OnCurrentStatus8kCam3, &OnCurrentStatusInfo8kCam3)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 2, OnImageCaptured8kCam3, &OnImageCapturedInfo8kCam3)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 3, OnCaptureAvailable8kCam3, &OnCaptureAvailableInfo8kCam3)
	END_SINK_MAP()

	void __stdcall Sink8kCam3::OnCurrentStatus8kCam3();
	void __stdcall Sink8kCam3::OnImageCaptured8kCam3(long imagesizex,long imagesizey,unsigned short * imagedata);
	void __stdcall Sink8kCam3::OnCaptureAvailable8kCam3();

	unsigned short* getData();
	void setDataIsNotReady();
	bool checkDataReady();
	Sink8kCam3(ICameraCtrl* pcamera,unsigned short * data);
	Sink8kCam3();
	~Sink8kCam3(void);
	void rotateandMirror();
	int getImageSizeX();
	int getImageSizeY();
	void setCropPad(int xPad,int yPad);
	void saveImage(CString path, CString basename,long ccdb_id,int count,CString timestamp);
	void setImageType(int type);
	int getImageType();
	float getMValue();
	float getMValue_2x();
	void setMValue_2x(float value);
	void setMValue(float value);
	float getMValue_4x();
	void setMValue_4x(float value);
	void factorCalculations();
	void setPathtoGainImages(CString path);
	
private:
	ICameraCtrl* m_pCameraCam3;
	unsigned short * m_pCapturedDataShortCam3;
	int IMAGESIZEX,IMAGESIZEY;
	bool Data_Avail;
	int XPAD,YPAD;
	int IMAGETYPE;
	float M_VALUE_CAMERA;
	float M_VALUE_CAMERA_2x;
	float M_VALUE_CAMERA_4x;
	//CImg<unsigned short> flatfield1x;
	//CImg<unsigned short> darkfield1x;
	//CImg<unsigned short> flatfield2x;
	//CImg<unsigned short> darkfield2x;
	//CImg<unsigned short> flatfield4x;
	//CImg<unsigned short> darkfield4x;
	unsigned short*  accumulatorBIAS_camera3;
	unsigned short*  accumulatorFLAT_camera3;
	CString mPathtoGain;
};