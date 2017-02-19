#if !defined(AFX_MACROCONTROLDLG_H__8B779EB0_622E_4B38_B7BE_191BAF43520D__INCLUDED_)
#define AFX_MACROCONTROLDLG_H__8B779EB0_622E_4B38_B7BE_191BAF43520D__INCLUDED_

#include "MacroControl.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MacroControlDlg.h : header file
//
#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CMacroControlDlg dialog

class CMacroControlDlg : public CBaseDlg
{
// Construction
public:
	MacroControl mControl;
	CMacroControlDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMacroControlDlg)
	enum { IDD = IDD_MACROCONTROL };
	CEdit	m_editWhiteLevel;
	CEdit	m_editTiltUp;
	CEdit	m_editTiltDown;
	CEdit	m_editRuns;
	CEdit	m_editMontError;
	CEdit	m_editImageShift;
	CString	m_strImageShift;
	CString	m_strMontError;
	int		m_intRuns;
	CString	m_strTiltDown;
	CString	m_strTiltUp;
	int		m_intWhiteLevel;
	BOOL	m_bImageShift;
	BOOL	m_bMontError;
	BOOL	m_bRuns;
	BOOL	m_bTiltDown;
	BOOL	m_bTiltUp;
	BOOL	m_bWhiteLevel;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMacroControlDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMacroControlDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnKillfocusEditImageshift();
	afx_msg void OnKillfocusEditMonterror();
	afx_msg void OnKillfocusEditRuns();
	afx_msg void OnKillfocusEditTiltdown();
	afx_msg void OnKillfocusEditTiltup();
	afx_msg void OnKillfocusEditWhitelevel();
	afx_msg void OnLimitImageshift();
	afx_msg void OnLimitMonterror();
	afx_msg void OnLimitRuns();
	afx_msg void OnLimitTiltdown();
	afx_msg void OnLimitTiltup();
	afx_msg void OnLimitWhitelevel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MACROCONTROLDLG_H__8B779EB0_622E_4B38_B7BE_191BAF43520D__INCLUDED_)
