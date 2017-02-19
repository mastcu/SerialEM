#if !defined(AFX_TSBACKUPDLG_H__5A4008F8_9D64_4C71_ABF4_D7476F4A8C6A__INCLUDED_)
#define AFX_TSBACKUPDLG_H__5A4008F8_9D64_4C71_ABF4_D7476F4A8C6A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TSBackupDlg.h : header file
//
#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CTSBackupDlg dialog

class CTSBackupDlg : public CBaseDlg
{
// Construction
public:
	CTSBackupDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTSBackupDlg)
	enum { IDD = IDD_TSBACKUP };
	CListBox	m_listZTilt;
	//}}AFX_DATA

	int m_iSelection;
	int m_iNumEntries;
	int m_iBaseZ;
	float *m_fpTilts;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTSBackupDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTSBackupDlg)
	afx_msg void OnDblclkListztilt();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TSBACKUPDLG_H__5A4008F8_9D64_4C71_ABF4_D7476F4A8C6A__INCLUDED_)
