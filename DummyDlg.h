#if !defined(AFX_DUMMYDLG_H__F9B30613_C6BB_4129_809A_C6DEBA256B18__INCLUDED_)
#define AFX_DUMMYDLG_H__F9B30613_C6BB_4129_809A_C6DEBA256B18__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DummyDlg.h : header file
//

#include "SerialEMDoc.h"

/////////////////////////////////////////////////////////////////////////////
// CDummyDlg dialog

class CDummyDlg : public CDialog
{
// Construction
public:
	CDummyDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDummyDlg)
	enum { IDD = IDD_DUMMYDLG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDummyDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDummyDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
  MyFileDlgThreadData *mfdTDp;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DUMMYDLG_H__F9B30613_C6BB_4129_809A_C6DEBA256B18__INCLUDED_)
