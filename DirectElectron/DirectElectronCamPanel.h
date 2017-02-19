#pragma once
#include "./../BaseDlg.h"
#include "./DirectElectronCamera.h"

// DirectElectronCamPanel dialog

//Define the GAIN settings that are necessary
#define NO_GAIN_SETTINGS 0 
#define LOW_GAIN_SETTINGS 1
#define MEDIUM_GAIN_SETTING 2
#define HIGH_GAIN_SETTINGS 3

#define MIN_SHUTTER_DELAY 0
#define MAX_SHUTTER_DELAY 4095

class DirectElectronCamPanel : public CBaseDlg
{
	DECLARE_DYNAMIC(DirectElectronCamPanel)

public:
	DirectElectronCamPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~DirectElectronCamPanel();
	void Update(void);
	void setCameraReference(DirectElectronCamera* cam);
	afx_msg LRESULT OnUpdateCameraStatus(WPARAM, LPARAM);

// Dialog Data
	enum { IDD = IDD_DirectElectronDialog };

private:
	DirectElectronCamera* DEcamera;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();


	DECLARE_MESSAGE_MAP()
public:
	afx_msg void GainSettingChange();
	afx_msg void OnBnClickedShuttersetbutton();
	afx_msg void OnBnClickedReadpressure();
	afx_msg void OnBnClickedWindowheateron();
	afx_msg void OnBnClickedWindowheateroff();
	afx_msg void OnBnClickedTecOn();
	afx_msg void OnBnClickedTecOff();
};
