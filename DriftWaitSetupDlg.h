#pragma once
#include "afxwin.h"
#include "ParticleTasks.h"
#include "afxcmn.h"


// CDriftWaitSetupDlg dialog

class CDriftWaitSetupDlg : public CBaseDlg
{

public:
	CDriftWaitSetupDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDriftWaitSetupDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DRIFTWAITSETUP };
#endif

protected:
  virtual void OnOK();
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
private:
  DriftWaitParams mParams;
  int mBinIndex;
  int mBinDivisor;
  CameraParameters *mCamParam;
  int mPanelStart[3];
  int mNumInPanel[3];

public:
  TiltSeriesParam *mTSparam;
  int m_iMeasureType;
  afx_msg void OnMeasureType();
  CButton m_butSetTrial;
  BOOL m_bSetTrial;
  afx_msg void OnCheckSetTrial();
  CStatic m_statTrialExp;
  CStatic m_statExpSec;
  CStatic m_statTrialBinning;
  CString m_strTrialBinning;
  CSpinButtonCtrl m_sbcTrialBin;
  afx_msg void OnDeltaposSpinTrialBin(NMHDR *pNMHDR, LRESULT *pResult);
  CEdit m_editTrialExp;
  float m_fTrialExp;
  float m_fInterval;
  float m_fDriftRate;
  float m_fMaxWait;
  BOOL m_bThrowError;
  BOOL m_bUseAngstroms;
  afx_msg void OnCheckUseAngstroms();


  void ManageTrialEntries();
  CString m_strRateUnits;
  CButton m_butChangeIS;
  BOOL m_bChangeIS;
  BOOL m_bAtReversals;
  BOOL m_bIgnoreAngles;
  int m_iAngleChoice;
  CEdit m_editRunAbove;
  CEdit m_editRunBelow;
  float m_fRunAbove;
  float m_fRunBelow;
  afx_msg void OnRadioAngle();
  CButton m_butUsePriorFocus;
  BOOL m_bUsePriorAutofocus;
  afx_msg void OnUsePriorAutofocus();
  CEdit m_editPriorThresh;
  float m_fPriorThresh;
  CString m_strPriorUnits;
};
