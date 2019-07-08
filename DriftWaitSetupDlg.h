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
public:
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
};
