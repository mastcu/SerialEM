#pragma once
#include "afxcmn.h"
#include "afxwin.h"


// CComaVsISCalDlg dialog

class CComaVsISCalDlg : public CBaseDlg
{

public:
	CComaVsISCalDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CComaVsISCalDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COMA_VS_IS_CAL };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
	DECLARE_MESSAGE_MAP()
public:
  CSpinButtonCtrl m_sbcDistance;
  afx_msg void OnDeltaposSpinDistance(NMHDR *pNMHDR, LRESULT *pResult);
  CEdit m_editDistance;
  float m_fDistance;
  afx_msg void OnKillfocusEditDistance();
  CEdit m_editRotation;
  int m_iRotation;
  afx_msg void OnKillfocusEditRotation();
  CSpinButtonCtrl m_sbcRotation;
  afx_msg void OnDeltaposSpinRotation(NMHDR *pNMHDR, LRESULT *pResult);
  void UpdateEnables();
  CButton m_butCalibrate;
  BOOL m_iNumImages;
  afx_msg void OnSetNumImages();
};
