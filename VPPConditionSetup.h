#pragma once
#include "afxwin.h"


// CVPPConditionSetup dialog

class CVPPConditionSetup : public CBaseDlg
{

public:
	CVPPConditionSetup(CWnd* pParent = NULL);   // standard constructor
	virtual ~CVPPConditionSetup();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VPP_EXPOSE_SETUP };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void OnOK();
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  CEMscope *mScope;
  CMultiTSTasks *mMultiTasks;
  MagTable *mMagTab;
  CString m_strMag;
  CString m_strSpot;
  CString m_strIntensity;
  CStatic m_statMag;
  CStatic m_statSpot;
  CStatic m_statIntensity;
  VppConditionParams mSavedParams;   // Saved values of settings
  bool mHasAlpha;
  BOOL mLastLowDose;


public:
  VppConditionParams mParams;  // Resident values of settings

  afx_msg void OnRLDrecord();
  int m_iWhichSettings;
  CButton m_butSetTrackState;
  BOOL m_bSetTrackState;
  afx_msg void OnSetTrackState();
  CStatic m_statAlpha;
  CString m_strAlpha;
  CStatic m_statProbeMode;
  CString m_strProbeMode;
  int m_iCookTime;
  BOOL m_bCondAtNavPoint;
  afx_msg void OnCondAtNavPoint();
  CButton m_butWithLabel;
  CButton m_butWithNote;
  int m_iLabelOrNote;
  CEdit m_editNavText;
  CString m_strNavText;
  CStatic m_statStarting;
  void ManageSettingEnables();
  void ManageNavEnables();
  void DisplaySettings();
  void LiveUpdate(int magInd, int spotSize, int probe, double intensity, int alpha);
  void SetScopeState(bool restore);
  afx_msg void OnCloseAndGo();
  CSpinButtonCtrl m_sbcCharge;
  int m_iCharge;
  int m_iTimeInstead;
  CSpinButtonCtrl m_sbcCookTime;
  CEdit m_editCookTime;
  CEdit m_editCookCharge;
  afx_msg void OnRcondCharge();
  afx_msg void OnRwithLabel();
  afx_msg void OnDeltaposSpinPPcondCharge(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinPPcondTime(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butPPcondGo;
  void StoreParameters();
  int m_iPostMoveDelay;
  CButton m_butNextAndGo;
  afx_msg void OnCloseNextAndGo();
  afx_msg void OnKillfocusPPcondCharge();
  afx_msg void OnKillfocusPPcondTime();
};
