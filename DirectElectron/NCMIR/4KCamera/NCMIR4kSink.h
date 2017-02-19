// CameraSink.h: interface for the CCameraSink class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAMERASINK_H__2BF6EBC8_0495_456D_92B3_AEF32BA75060__INCLUDED_)
#define AFX_CAMERASINK_H__2BF6EBC8_0495_456D_92B3_AEF32BA75060__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>


#import "CameraLink.tlb" no_namespace named_guids raw_interfaces_only


extern _ATL_FUNC_INFO OnCurrentStatusInfoNCMIR4k,OnImageCapturedInfoNCMIR4k,OnCaptureAvailableInfoNCMIR4k;


class CNCMIR4kSink: public IDispEventSimpleImpl<1,CNCMIR4kSink,&DIID__ICameraCtrlEvents>
{
public:

	BEGIN_SINK_MAP(CNCMIR4kSink)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 1, OnCurrentStatus, &OnCurrentStatusInfoNCMIR4k)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 2, OnImageCaptured, &OnImageCapturedInfoNCMIR4k)
	SINK_ENTRY_INFO(1, DIID__ICameraCtrlEvents, 3, OnCaptureAvailable, &OnCaptureAvailableInfoNCMIR4k)
	END_SINK_MAP()

	CNCMIR4kSink();
	CNCMIR4kSink(ICameraCtrl* pcamera,unsigned short * data);
	void __stdcall CNCMIR4kSink::OnCurrentStatus();
	void __stdcall CNCMIR4kSink::OnImageCaptured(long imagesizex,long imagesizey,unsigned short * imagedata);
	void __stdcall CNCMIR4kSink::OnCaptureAvailable();
	virtual ~CNCMIR4kSink();
	unsigned short* getData();
	//bool DataIsReady();
	//bool DataReceived();
	void setDataIsNotReady();
	bool checkDataReady();
	int getImageSizeX();
	int getImageSizeY();

private:
	ICameraCtrl* m_pCamera;
	unsigned short * m_pCapturedDataShort;
	int IMAGESIZEX,IMAGESIZEY;
	bool Data_Avail;

};



#endif // !defined(AFX_CAMERASINK_H__2BF6EBC8_0495_456D_92B3_AEF32BA75060__INCLUDED_)
