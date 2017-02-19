#pragma once


// DialogPanelEightKCamera dialog

#include "../../../BaseDlg.h"

#include "NCMIR8kCamera.h"
// DialogPanelEightKCamera dialog

//Define the GAIN settings that are necessary
#define NO_GAIN_SETTINGS 0 
#define MEDIUM_GAIN_SETTING 1
#define HIGH_GAIN_SETTINGS 2

class DialogPanelEightKCamera : public CBaseDlg
{
	DECLARE_DYNAMIC(DialogPanelEightKCamera)

public:
	DialogPanelEightKCamera(CWnd* pParent = NULL);   // standard constructor
	virtual ~DialogPanelEightKCamera();
	void Update(void);
	void setCameraReference(NCMIR8kCamera* cam);
	void setScopeReference(CEMscope* mScope);
	void readChillerInformation();
	void readTempProbeInfo();
	afx_msg LRESULT OnUpdateCameraStatus(WPARAM, LPARAM);
	//static long getCurrentCCDBID();
	//static void setCurrentCCDBID(long id);

private:
	NCMIR8kCamera* camera;
	CEMscope* microscope;
public:

// Dialog Data
	enum { IDD = IDD_8KDialog };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void FocusCameraSelection();
	afx_msg void TrialCameraSelection();
	afx_msg void GainSettingChange();
	afx_msg void OnBnClickedRunflat8k();
	afx_msg void CorrectedCheckBox_Changed();
	afx_msg void RemapImagesCheckBoxEvent();
};
