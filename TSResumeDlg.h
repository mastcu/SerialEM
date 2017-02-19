#if !defined(AFX_TSRESUMEDLG_H__2B67F285_FAF8_466D_A347_76E6CB61CDB4__INCLUDED_)
#define AFX_TSRESUMEDLG_H__2B67F285_FAF8_466D_A347_76E6CB61CDB4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TSResumeDlg.h : header file
//

#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CTSResumeDlg dialog

class CTSResumeDlg : public CBaseDlg
{
// Construction
public:
	CTSResumeDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTSResumeDlg)
	enum { IDD = IDD_TSRESUME };
	CButton	m_butResume;
	CButton	m_butRadio2;
	CButton	m_butRadio3;
	CButton	m_butRadio4;
	CButton	m_butRadio5;
	CButton	m_butRadio6;
	CButton	m_butRadio7;
	CStatic	m_statStatus;
	BOOL	m_bUseNewRef;
	CString	m_strStatus;
	int		m_iResumeOption;
	//}}AFX_DATA

	CString m_strRecordName;
	int m_iSingleStep;
	BOOL m_bMontaging;
	BOOL m_bRecordInA;
	BOOL m_bTiltable;
  BOOL m_bChangeOK;
	BOOL m_bLowMagAvailable;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTSResumeDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTSResumeDlg)
	afx_msg void OnSingleloop();
	afx_msg void OnSinglestep();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TSRESUMEDLG_H__2B67F285_FAF8_466D_A347_76E6CB61CDB4__INCLUDED_)
