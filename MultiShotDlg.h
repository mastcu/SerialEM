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
  bool AdjustingISValues() {return mSteppingAdjusting > 0; };

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
  CShiftManager *mShiftManager;
  int mPanelStart[7];
  int mNumInPanel[7];
  bool mRecordingRegular;
  bool mRecordingCustom;
  int mSteppingAdjusting;
  std::vector<double> mSavedISX, mSavedISY;
  BOOL mSavedMouseStage;
  double mRecordStageX, mRecordStageY;
  double mStartingISX, mStartingISY;
  bool mDisabledDialog;
  int mNavPointIncrement;
  bool mWarnedIStoNav;
  int mLastMagIndex;
  double mLastIntensity;
  double mLastDrawTime;
  int mSavedLDForCamera;
  int mAreaSaved;
  float mSavedDefOffset;
  LowDoseParams mSavedLDParams;

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
  void ManageHexGrid();
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
  void ManagePanels(bool adjust = false);
  CButton m_butSaveIS;
  CButton m_butEndPattern;
  CButton m_butAbort;
  afx_msg void OnButSaveIs();
  afx_msg void OnButEndPattern();
  afx_msg void OnButAbort();
  CStatic m_statSaveInstructions;
  bool RecordingHoles() {return mRecordingRegular || mRecordingCustom || mSteppingAdjusting;};
  void StopRecording(void);
  void StartRecording(const char *instruct);
  void UpdateSettings(void);
  CEdit m_editExtraDelay;
  CButton m_butCancel;
  CButton m_butIStoPt;
  afx_msg void OnButIsToPt();
  void UpdateMultiDisplay(int magInd, double intensity);
  BOOL mLastPanelStates[7];
  afx_msg void OnSaveRecord();
  afx_msg void OnAdjustBeamTilt();
  CButton m_butOmit3x3Corners;
  BOOL m_bOmit3x3Corners;
  afx_msg void OnOmit3x3Corners();
  BOOL m_bDoSecondRing;
  CStatic m_statNum2Shots;
  CStatic m_statRing2Shots;
  CStatic m_statRing2Um;
  CEdit m_editRing2Dist;
  CSpinButtonCtrl m_sbcRing2Num;
  afx_msg void OnDoSecondRing();
  afx_msg void OnDeltaposSpinRing2Num(NMHDR *pNMHDR, LRESULT *pResult);
  float m_fRing2Dist;
  afx_msg void OnKillfocusEditRing2Dist();
  CString m_strNum2Shots;
  CButton m_butDoSecondRing;
  CButton m_butUseLastHoleVecs;
  CButton m_butStepAdjust;
  afx_msg void OnButUseLastHoleVecs();
  afx_msg void OnButStepAdjust();
  CButton m_butCalBTvsIS;
  CButton m_butTestMultishot;
  afx_msg void OnButCalBtVsIs();
  afx_msg void OnButTestMultishot();
  CButton m_butHexGrid;
  BOOL m_bHexGrid;
  afx_msg void OnCheckHexGrid();
  CString m_strAdjustStatus;
  CButton m_butUseMapVectors;
  afx_msg void OnButUseMapVectors();
  int ConfirmReplacingShiftVectors(const char *kind, BOOL hex);
  CButton m_butApplyAdjustment;
  afx_msg void OnButApplyAdjustment();
};
