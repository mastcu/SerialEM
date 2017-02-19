#include "afxwin.h"
#if !defined(AFX_ASKONEDLG_H__86E274DF_F0C2_46DB_90F2_852195C3428B__INCLUDED_)
#define AFX_ASKONEDLG_H__86E274DF_F0C2_46DB_90F2_852195C3428B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AskOneDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAskOneDlg dialog

class CAskOneDlg : public CDialog
{
// Construction
public:
	//BOOL OnInitDialog();
	int m_iExtraWidth;
	CAskOneDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAskOneDlg)
	enum { IDD = IDD_ASKONE_DIALOG };
	CStatic	m_statInfoLine;
	CButton	m_butOK;
	CButton	m_butCancel;
	CStatic	m_statText;
	CEdit	m_pEditBox;
	CString	m_sEditString;
	CString	m_sTextString;
	CString	m_strInfoLine;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAskOneDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAskOneDlg)
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
  CButton m_butBrowse;
  CString m_strBrowserTitle;
  afx_msg void OnBrowse();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ASKONEDLG_H__86E274DF_F0C2_46DB_90F2_852195C3428B__INCLUDED_)
