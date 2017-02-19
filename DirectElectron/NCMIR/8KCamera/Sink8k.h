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

extern _ATL_FUNC_INFO OnCurrentStatusInfo8k,OnImageCapturedInfo8k,OnCaptureAvailableInfo8k;


class Sink8k : public IDispEventSimpleImpl<1,Sink8k,&DIID__ICameraCtrlEvents>
{
public:

	BEGIN_SINK_MAP(Sink8k)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 1, OnCurrentStatus8k, &OnCurrentStatusInfo8k)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 2, OnImageCaptured8k, &OnImageCapturedInfo8k)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 3, OnCaptureAvailable8k, &OnCaptureAvailableInfo8k)
	END_SINK_MAP()

	void __stdcall Sink8k::OnCurrentStatus8k();
	void __stdcall Sink8k::OnImageCaptured8k(long imagesizex,long imagesizey,unsigned short * imagedata);
	void __stdcall Sink8k::OnCaptureAvailable8k();

	unsigned short* getData();
	void setDataIsNotReady();
	bool checkDataReady();
	Sink8k(ICameraCtrl* pcamera,unsigned short * data);
	Sink8k();
	~Sink8k(void);
	int getImageSizeX();
	int getImageSizeY();
	void setCropPad(int xPad,int yPad);
	void saveImage(CString path, CString basename,long ccdb_id,int count,CString timestamp);
	void setImageType(int type);
	int getImageType();
	float getMValue();
	void setMValue(float value);
	float getMValue_2x();
	void setMValue_2x(float value);
	float getMValue_4x();
	void setMValue_4x(float value);
	void factorCalculations();
	void setPathtoGainImages(CString path);
private:
	ICameraCtrl* m_pCamera;
	unsigned short * m_pCapturedDataShort;
	int IMAGESIZEX,IMAGESIZEY;
	bool Data_Avail;
	int cam_id;
	int bin;
	int XPAD,YPAD;
	int IMAGETYPE;
	float M_VALUE_CAMERA;
	float M_VALUE_CAMERA_2x;
	float M_VALUE_CAMERA_4x;
	CString mPathtoGain;
	//CImg<unsigned short> flatfield1x;
	//CImg<unsigned short> darkfield1x;
	//CImg<unsigned short> flatfield2x;
	//CImg<unsigned short> darkfield2x;
	//CImg<unsigned short> flatfield4x;
	//CImg<unsigned short> darkfield4x;
	unsigned short*  accumulatorBIAS_camera1;
	unsigned short*  accumulatorFLAT_camera1;
};
