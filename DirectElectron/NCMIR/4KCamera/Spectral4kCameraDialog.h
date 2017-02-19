#pragma once


// Spectral4kCameraDialog dialog

#include "../../../BaseDlg.h"

#include "NCMIR4kCamera.h"
// Spectral4kCameraDialog dialog
#include "./SerialPortLibrary/Serial.h"
#include "afxwin.h"

//Define the GAIN settings that are necessary
#define NO_GAIN_SETTINGS 0 
#define MEDIUM_GAIN_SETTING 1
#define HIGH_GAIN_SETTINGS 2



class Spectral4kCameraDialog : public CBaseDlg
{
	DECLARE_DYNAMIC(Spectral4kCameraDialog)

public:
	Spectral4kCameraDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~Spectral4kCameraDialog();
	void Update(void);
	void setCameraReference(NCMIR4kCamera* cam);
	void setScopeReference(CEMscope* mScope);
	void readChillerInformation();
	void readTempProbeInfo();
	void LogFastemMesg(CString str);
	afx_msg LRESULT OnUpdateCameraStatus(WPARAM, LPARAM);

// Dialog Data
	enum { IDD = IDD_NCMIR4K };
private:
	NCMIR4kCamera* camera;
	float m_chiller_flow_rate;
	float m_roomTemperature;
	float m_roomHumidity;
	CSerial m_serialPortChiller;
	CSerial m_tempProbe;
	LONG    lChillerError;
	bool m_ChillerConnectionSuccess;
	LONG    ltempProbeError;
	bool m_tempProbeConnectionSuccess;
	
	CEMscope* microscope;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCbnSelchangeAttencombo();
	afx_msg void OnBnClickedBlankbutton();
	afx_msg void OnBnClickedUnblankbutton();
};
