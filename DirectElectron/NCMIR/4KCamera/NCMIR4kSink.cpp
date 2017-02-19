// CameraSink4k.cpp: implementation of the CCameraSink class.
// For retrieving images from the Spectral 4k camera
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NCMIR4kSink.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


_ATL_FUNC_INFO OnCurrentStatusInfoNCMIR4k = {CC_STDCALL, VT_EMPTY, 0,0};
_ATL_FUNC_INFO OnImageCapturedInfoNCMIR4k = {CC_STDCALL, VT_EMPTY, 3, VT_I4,VT_I4,VT_I4};
_ATL_FUNC_INFO OnCaptureAvailableInfoNCMIR4k = {CC_STDCALL, VT_EMPTY, 0, 0};





//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNCMIR4kSink::CNCMIR4kSink()
{

	
}

CNCMIR4kSink::CNCMIR4kSink(ICameraCtrl* pcamera,unsigned short * data)
{
	
	m_pCapturedDataShort = data;
	m_pCamera = pcamera;
	m_pCamera->AddRef();
	DispEventAdvise((IUnknown*)m_pCamera);

}


CNCMIR4kSink::~CNCMIR4kSink()
{
	

	if(m_pCamera)
		m_pCamera->Release();
	//DispEventUnadvise((IUnknown*)m_pCamera);
}



unsigned short* CNCMIR4kSink::getData()
{
	return m_pCapturedDataShort;

}


void CNCMIR4kSink::setDataIsNotReady()
{
	Data_Avail = false;
}

bool CNCMIR4kSink::checkDataReady()
{
	return Data_Avail;
}

void __stdcall CNCMIR4kSink::OnImageCaptured(long imagesizex,long imagesizey,unsigned short * imagedata)
{
	CString imgstr;

	IMAGESIZEX = imagesizex;
	IMAGESIZEY = imagesizey;
	int totalsize = sizeof(imagedata)/sizeof(imagedata[0]);
	imgstr.Format("camera  x: %d y: %d  totalsize %d",imagesizex,imagesizey,totalsize);
	Data_Avail = true;
	//AfxMessageBox(imgstr);

}

int CNCMIR4kSink::getImageSizeX()
{
	return IMAGESIZEX;
}

int CNCMIR4kSink::getImageSizeY()
{
	return IMAGESIZEY;
}

void __stdcall CNCMIR4kSink::OnCaptureAvailable()
{
	
}
void __stdcall CNCMIR4kSink::OnCurrentStatus()
{

	//AfxMessageBox("Current Status");
}



