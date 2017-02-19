///////////////////////////////////////////////////////////////////
// file:  LC1100CamSink.h
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

#if !defined(AFX_CAMERASINK_H__2BF6EBC8_0495_456D_92B3_AEF32BA75060__INCLUDED_)
#define AFX_CAMERASINK_H__2BF6EBC8_0495_456D_92B3_AEF32BA75060__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>


// DNM: switch to include fixed tlh until the tlb can be updated properly
#include "soliosfor4000.tlh"
//#import "SoliosFor4000.tlb" no_namespace named_guids raw_interfaces_only


extern _ATL_FUNC_INFO OnCurrentStatusInfo,OnImageCapturedInfo,OnCaptureAvailableInfo;


class CLC1100CamSink: public IDispEventSimpleImpl<1,CLC1100CamSink,&DIID__ICoSoliosFor4000Events>
{
public:

	BEGIN_SINK_MAP(CLC1100CamSink)
	SINK_ENTRY_INFO(1, DIID__ICoSoliosFor4000Events, 1, OnCurrentStatus, &OnCurrentStatusInfo)
	SINK_ENTRY_INFO(1, DIID__ICoSoliosFor4000Events, 2, OnImageCaptured, &OnImageCapturedInfo)
	SINK_ENTRY_INFO(1, DIID__ICoSoliosFor4000Events, 3, OnCaptureAvailable, &OnCaptureAvailableInfo)
	END_SINK_MAP()

	CLC1100CamSink();
	CLC1100CamSink(ICameraCtrl* pcamera,unsigned short * data);
	void __stdcall CLC1100CamSink::OnCurrentStatus();
	void __stdcall CLC1100CamSink::OnImageCaptured(long imagesizex,long imagesizey,unsigned short * imagedata);
	void __stdcall CLC1100CamSink::OnCaptureAvailable();
	virtual ~CLC1100CamSink();
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
