#pragma once

// Forward declarations were used instead of including DirectElectronCamera.h because
// every module including it had a bloated object file, and every module includes this
// file here.  But this was due to Cimg, which is now included in DirectElectronCamera.cpp
// instead, and so it is now OK to include DirectElectronCamera.h
class DirectElectronCamera;

#define NO_GAIN_SETTINGS 0
#define LOW_GAIN_SETTINGS 1
#define MEDIUM_GAIN_SETTING 2
#define HIGH_GAIN_SETTINGS 3

#define MIN_SHUTTER_DELAY 0
#define MAX_SHUTTER_DELAY 4095

#define DE_UNCORRECTED_RAW_MODE 0
#define DE_CORRECTED_DARK_MODE 1

// DirectElectronToolDlg dialog

class DirectElectronToolDlg : public CToolDlg
{
	DECLARE_DYNAMIC(DirectElectronToolDlg)

public:
	DirectElectronToolDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~DirectElectronToolDlg();
	void setCameraReference(DirectElectronCamera* cam);
	void setUpDialogNames(CString names[], int totalcams);
	void InitDialogs();
	void ActivateTabDialogs();
	void OnOK();
	void UpdateSettings();
	void Update();
	void updateDEToolDlgPanel(bool initialCall);
  bool SuppressDebug() {return mSuppressDebug > 0;};
  void UpdateInsertion(int state);
  void SetProtectionCoverMode(int nIndex);
  void ApplyUserSettings();
  void KillUpdateTimer();
  BOOL CanSaveFrames(CameraParameters *param);
  BOOL HasFrameTime(CameraParameters *param);
  void SwitchSaveOptToMRC();
  GetSetMember(int, ProtCoverChoice);
  GetSetMember(int, ProtCoverDelay);
  GetSetMember(int, FormatForAutoSave);
  GetMember(int, LastProtCoverSet);
  GetMember(CString, AutosaveDir);
  GetSetMember(int, TemperSetpoint);

  // Dialog Data
	enum { IDD = IDD_DETOOLDLG };

private:
	DirectElectronCamera* mDECamera;
  UINT_PTR mStatusUpdateID;
  int mSuppressDebug;
  bool mChangingCooling;
  int mProtCoverChoice;
  int mLastProtCoverSet;
  int mProtCoverDelay;
  int mFormatForAutoSave;
  int mTemperSetpoint;
  CString mAutosaveDir;
  CString mHdfMrcSaveOption;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	//DEcameraTabsController mDETabControl;
	afx_msg void Modechange();
	afx_msg void OnBnClickedCoolcam();
	afx_msg void OnBnClickedinsert();

//	afx_msg void OnEnMaxtextDesumcount();

	afx_msg void OnEnKillfocusDecurrfps();
	afx_msg void OnEnKillfocusDepreexptime();
	afx_msg void OnEnKillfocusDesensordelay();
  afx_msg void FormatChange();
  double GetFramesPerSecond(void);
  const char *FileFormatProperty();
};
