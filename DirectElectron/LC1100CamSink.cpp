///////////////////////////////////////////////////////////////////
// file:  LC1100CamSink.cpp
// 
//This file is setup to be used for receiving the callbacks
//from the LC1100 
// 
//
// author of SerialEM: David Masternarde
// 
// LC1100 camera integration by Direct Electron
// 2009
// 
///////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "LC1100CamSink.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


_ATL_FUNC_INFO OnCurrentStatusInfo = {CC_STDCALL, VT_EMPTY, 0,0};
_ATL_FUNC_INFO OnImageCapturedInfo = {CC_STDCALL, VT_EMPTY, 3, VT_I4,VT_I4,VT_I4};
_ATL_FUNC_INFO OnCaptureAvailableInfo = {CC_STDCALL, VT_EMPTY, 0, 0};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLC1100CamSink::CLC1100CamSink()
{

	
}

CLC1100CamSink::CLC1100CamSink(ICameraCtrl* pcamera,unsigned short * data)
{
	
	m_pCapturedDataShort = data;
	m_pCamera = pcamera;
	m_pCamera->AddRef();
	DispEventAdvise((IUnknown*)m_pCamera);

}


CLC1100CamSink::~CLC1100CamSink()
{
	if(m_pCamera)
		m_pCamera->Release();
	//DispEventUnadvise((IUnknown*)m_pCamera);
}



unsigned short* CLC1100CamSink::getData()
{
	return m_pCapturedDataShort;

}


void CLC1100CamSink::setDataIsNotReady()
{
	Data_Avail = false;
}

bool CLC1100CamSink::checkDataReady()
{
	return Data_Avail;
}

void __stdcall CLC1100CamSink::OnImageCaptured(long imagesizex,long imagesizey,unsigned short * imagedata)
{
	CString imgstr;

	IMAGESIZEX = imagesizex;
	IMAGESIZEY = imagesizey;
	int totalsize = sizeof(imagedata)/sizeof(imagedata[0]);
	imgstr.Format("camera  x: %d y: %d  totalsize %d",imagesizex,imagesizey,totalsize);
	Data_Avail = true;
	//AfxMessageBox(imgstr);

}

int CLC1100CamSink::getImageSizeX()
{
	return IMAGESIZEX;
}

int CLC1100CamSink::getImageSizeY()
{
	return IMAGESIZEY;
}

void __stdcall CLC1100CamSink::OnCaptureAvailable()
{


	
}
void __stdcall CLC1100CamSink::OnCurrentStatus()
{
	//AfxMessageBox("Current Status");
}



