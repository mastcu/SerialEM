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

extern _ATL_FUNC_INFO OnCurrentStatusInfo8kCam2,OnImageCapturedInfo8kCam2,OnCaptureAvailableInfo8kCam2;


class Sink8kCam2 : public IDispEventSimpleImpl<1,Sink8kCam2,&DIID__ICameraCtrlEvents>
{
public:

	BEGIN_SINK_MAP(Sink8kCam2)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 1, OnCurrentStatus8kCam2, &OnCurrentStatusInfo8kCam2)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 2, OnImageCaptured8kCam2, &OnImageCapturedInfo8kCam2)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 3, OnCaptureAvailable8kCam2, &OnCaptureAvailableInfo8kCam2)
	END_SINK_MAP()

	void __stdcall Sink8kCam2::OnCurrentStatus8kCam2();
	void __stdcall Sink8kCam2::OnImageCaptured8kCam2(long imagesizex,long imagesizey,unsigned short * imagedata);
	void __stdcall Sink8kCam2::OnCaptureAvailable8kCam2();

	unsigned short* getData();
	void setDataIsNotReady();
	bool checkDataReady();
	Sink8kCam2(ICameraCtrl* pcamera,unsigned short * data);
	Sink8kCam2();
	~Sink8kCam2(void);
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
	float getMValue_4x();
	void setMValue_4x(float value);
	void setMValue(float value);
	void factorCalculations();
	void setPathtoGainImages(CString path);
private:
	ICameraCtrl* m_pCameraCam2;
	unsigned short * m_pCapturedDataShortCam2;
	int IMAGESIZEX,IMAGESIZEY;
	bool Data_Avail;
	int XPAD, YPAD;
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
	unsigned short*  accumulatorBIAS_camera2;
	unsigned short*  accumulatorFLAT_camera2;
	CString mPathtoGain;

};