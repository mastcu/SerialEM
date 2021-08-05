#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// CNavRealignDlg dialog
#include "NavHelper.h"

class CNavRealignDlg : public CBaseDlg
{

public:
	CNavRealignDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavRealignDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NAVREALIGN };
#endif


protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();

	DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[3];
  int mNumInPanel[3];
  NavAlignParams *mMasterParams;
  NavAlignParams mParams;
  int mMaxNumResets;
  int mLoadBuf;

public:
  bool mForRealignOnly;
  afx_msg void OnEnKillfocusEditMapLabel();
  CString m_strMapLabel;
  afx_msg void OnMakeTemplateMap();
  CButton m_butMakeMap;
  CString m_strSelectedBuf;
  float m_fAlignShift;
  afx_msg void OnEnKillfocusEditMaxShift();
  afx_msg void OnCheckResetIs();
  CEdit m_editResetIS;
  float m_fResetThresh;
  CStatic m_statRepeatReset;
  CString m_strRepeatReset;
  CStatic m_statResetTimes;
  CSpinButtonCtrl m_sbcResetTimes;
  afx_msg void OnDeltaposSpinResetTimes(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnDeltaposSpinTemplateBuf(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butLeaveISzero;
  BOOL m_bLeaveISzero;
  afx_msg void OnCheckLeaveIsZero();
  void ManageMap();
  void ManageResetIS();
  CSpinButtonCtrl m_sbcTemplateBuf;
  BOOL m_bResetIS;
};
