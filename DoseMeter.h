#if !defined(AFX_DOSEMETER_H__E390FD88_0B66_4823_BBA7_87800B90D677__INCLUDED_)
#define AFX_DOSEMETER_H__E390FD88_0B66_4823_BBA7_87800B90D677__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DoseMeter.h : header file
//
#include "afxwin.h"
#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CDoseMeter dialog

class CDoseMeter : public CBaseDlg
{
// Construction
public:
	CDoseMeter(CWnd* pParent = NULL);   // standard constructor
  double mCumDose;

// Dialog Data
	//{{AFX_DATA(CDoseMeter)
	enum { IDD = IDD_DOSEMETER };
	CStatic	m_statDose;
	CString	m_strDose;
	//}}AFX_DATA

  CString m_strFontName;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDoseMeter)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDoseMeter)
  afx_msg void OnCancel();
	afx_msg void OnButreset();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
  CFont mFont;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DOSEMETER_H__E390FD88_0B66_4823_BBA7_87800B90D677__INCLUDED_)
