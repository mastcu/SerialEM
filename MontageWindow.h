#if !defined(AFX_MONTAGEWINDOW_H__1A11938C_0E00_4A83_A715_813AD9BA07FD__INCLUDED_)
#define AFX_MONTAGEWINDOW_H__1A11938C_0E00_4A83_A715_813AD9BA07FD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MontageWindow.h : header file
//
#include "ToolDlg.h"
#include "MontageParam.h"
#include "afxwin.h"
class EMmontageController;

/////////////////////////////////////////////////////////////////////////////
// CMontageWindow dialog

class CMontageWindow : public CToolDlg
{
// Construction
public:
	void ManageBinning();
	void UpdateSettings();
	void Update();
	void OnOK();
	CMontageWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMontageWindow)
	enum { IDD = IDD_MONTAGECONTROL };
	CButton	m_butVerySloppy;
	CButton	m_butShiftOverview;
	CSpinButtonCtrl	m_sbcViewBin;
	CSpinButtonCtrl	m_sbcScanBin;
	CButton	m_butAdjustFocus;
	CButton	m_butCorrectDrift;
	CStatic	m_statCurrentZ;
	CSpinButtonCtrl	m_sbcZ;
	CEdit	m_editCurrentZ;
	CButton	m_butTrial;
	CButton	m_butStop;
	CButton	m_butStart;
	BOOL	m_bAdjustFocus;
	BOOL	m_bCorrectDrift;
	int		m_iCurrentZ;
	BOOL	m_bShowOverview;
	CString	m_strOverviewBin;
	CString	m_strScanBin;
	BOOL	m_bShiftOverview;
	BOOL	m_bVerySloppy;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMontageWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMontageWindow)
	afx_msg void OnBstart();
	afx_msg void OnBstop();
	afx_msg void OnBtrial();
	afx_msg void OnFocusOrDrift();
	afx_msg void OnDeltaposSpinz(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillfocusEditz();
	virtual BOOL OnInitDialog();
	afx_msg void OnShowOverview();
	afx_msg void OnDeltaposSpinscanbin(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinviewbin(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShiftInOverview();
	afx_msg void OnVerySloppy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	EMmontageController *mMontageController;
	MontParam *mParam;
	CFont mFont;
public:
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MONTAGEWINDOW_H__1A11938C_0E00_4A83_A715_813AD9BA07FD__INCLUDED_)
