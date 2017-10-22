#pragma once
#include "BaseDlg.h"
#include "afxcmn.h"
#include "afxwin.h"
#include "NavHelper.h"

// CMultiShotDlg dialog

class CMultiShotDlg : public CBaseDlg
{
	DECLARE_DYNAMIC(CMultiShotDlg)

public:
	CMultiShotDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiShotDlg();

// Dialog Data
	enum { IDD = IDD_MULTI_SHOT_SETUP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  MultiShotParams *mActiveParams;
  MultiShotParams mSavedParams;

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
  void UpdateAndUseMSparams(void);
  float m_fExtraDelay;
  CButton m_butSaveRecord;
  afx_msg void OnKillfocusEditEarlyFrames();
  void ManageEnables(void);
};
