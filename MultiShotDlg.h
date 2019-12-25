#pragma once
#include "BaseDlg.h"
#include "afxcmn.h"
#include "afxwin.h"
#include "NavHelper.h"

// CMultiShotDlg dialog

class CMultiShotDlg : public CBaseDlg
{

public:
	CMultiShotDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiShotDlg();
  bool RecordingISValues() {return mRecordingCustom || mRecordingRegular;};

// Dialog Data
	enum { IDD = IDD_MULTI_SHOT_SETUP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()

private:
  MultiShotParams *mActiveParams;
  MultiShotParams mSavedParams;
  int mPanelStart[7];
  int mNumInPanel[7];
  bool mRecordingRegular;
  bool mRecordingCustom;
  std::vector<double> mSavedISX, mSavedISY;
  BOOL mSavedMouseStage;
  double mRecordStageX, mRecordStageY;
  bool mDisabledDialog;
  int mNavPointIncrement;
  bool mWarnedIStoNav;
  int mLastMagIndex;
  double mLastIntensity;
  double mLastDrawTime;
  int mSavedLDForCamera;
  LowDoseParams mSavedLDRecParams;

public:
  bool mCanReturnEarly;
  BOOL mHasIlluminatedArea;
  CString m_strNumShots;
  CSpinButtonCtrl m_sbcNumShots;
  afx_msg void OnDeltaposSpinNumShots(NMHDR *pNMHDR, LRESULT *pResult);
  float m_fSpokeDist;
  afx_msg void OnCheckEarlyReturn();
  CEdit m_editEarlyFrames;
  int m_iEarlyFrames;
  BOOL m_bSaveRecord;
  CButton m_butUseIllumArea;
  BOOL m_bUseIllumArea;
  CStatic m_statBeamDiam;
  CStatic m_statBeamMicrons;
  CEdit m_editBeamDiam;
  float m_fBeamDiam;
  afx_msg void OnRnoCenter();
  afx_msg void OnKillfocusEditSpokeDist();
  afx_msg void OnKillfocusEditBeamDiam();
  afx_msg void OnCheckUseIllumArea();
  int m_iCenterShot;
  CStatic m_statNumEarly;
  CButton m_butNoEarly;
  int m_iEarlyReturn;
  afx_msg void OnRnoEarly();
  void UpdateAndUseMSparams(bool draw = true);
  float m_fExtraDelay;
  CButton m_butSaveRecord;
  afx_msg void OnKillfocusEditEarlyFrames();
  void ManageEnables(void);
  CButton m_butAdjustBeamTilt;
  BOOL m_bAdjustBeamTilt;
  CStatic m_statComaIScal;
  CStatic m_statComaConditions;
  BOOL m_bDoShotsInHoles;
  BOOL m_bDoMultipleHoles;
  CStatic m_statNumXholes;
  CStatic m_statNumYholes;
  CString m_strNumYholes;
  CString m_strNumXholes;
  CSpinButtonCtrl m_sbcNumXholes;
  CSpinButtonCtrl m_sbcNumYholes;
  CButton m_butSetRegular;
  CButton m_butSetCustom;
  CButton m_butUseCustom;
  BOOL m_bUseCustom;
  CStatic m_statRegular;
  CStatic m_statSpacing;
  afx_msg void OnDoMultipleHoles();
  afx_msg void OnUseCustom();
  afx_msg void OnDeltaposSpinNumXHoles(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinNumYHoles(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnSetRegular();
  afx_msg void OnSetCustom();
  afx_msg void OnDoShotsInHole();
  CButton m_butNoCenter;
  CButton m_butCenterBefore;
  CButton m_butCenterAfter;
  CEdit m_editSpokeDist;
  CStatic m_statNumPeripheral;
  CStatic m_statCenterGroup;
  CStatic m_statNumShots;
  CStatic m_statCenterDist;
  CStatic m_statCenterUm;
  CStatic m_statHoleDelayFac;
  CEdit m_editHoleDelayFac;
  float m_fHoleDelayFac;
  void ManagePanels(void);
  CButton m_butSaveIS;
  CButton m_butEndPattern;
  CButton m_butAbort;
  afx_msg void OnButSaveIs();
  afx_msg void OnButEndPattern();
  afx_msg void OnButAbort();
  CStatic m_statSaveInstructions;
  bool RecordingHoles() {return mRecordingRegular || mRecordingCustom;};
  void StopRecording(void);
  void StartRecording(const char *instruct);
  void UpdateSettings(void);
  CEdit m_editExtraDelay;
  CButton m_butCancel;
  CButton m_butIStoPt;
  afx_msg void OnButIsToPt();
  void UpdateMultiDisplay(int magInd, double intensity);
  BOOL mLastPanelStates[7];
};
