#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"


// CNavImportDlg dialog

class CNavImportDlg : public CBaseDlg
{

public:
	CNavImportDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavImportDlg();

// Dialog Data
	enum { IDD = IDD_NAVIMPORT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  CString m_strSection;
  CEdit m_editSection;
  int m_iSection;
  float m_fStageX;
  float m_fStageY;
  CString m_strRegNum;
  CComboBox m_comboXform;
  int mRegNum;
  int mNumSections;
  int mXformNum;
  BOOL mOverlayOK;
  afx_msg void OnDeltaposSpinRegnum(NMHDR *pNMHDR, LRESULT *pResult);
  CString m_strSecLabel;
  CSpinButtonCtrl m_sbcRegNum;
  afx_msg void OnKillfocusEdit();
  CButton m_butOverlay;
  BOOL m_bOverlay;
  CEdit m_editOverlay;
  CString m_strOverlay;
  CStatic m_statOverlay1;
  CStatic m_statOverlay2;
  afx_msg void OnCheckOverlay();
};
