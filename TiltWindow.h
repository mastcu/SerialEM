#if !defined(AFX_TILTWINDOW_H__5DBD326D_7072_4EC6_9DE2_B5A788F8FEDB__INCLUDED_)
#define AFX_TILTWINDOW_H__5DBD326D_7072_4EC6_9DE2_B5A788F8FEDB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TiltWindow.h : header file
//
#include "ToolDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CTiltWindow dialog

class CTiltWindow : public CToolDlg
{
// Construction
public:
	void UpdateSettings();
	void EnableTiltButtons();
	void UpdateEnables();
	void TiltUpdate(double inTilt, BOOL bReady);
	CTiltWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTiltWindow)
	enum { IDD = IDD_TILTCONTROL };
	CButton	m_butSetDelay;
	CButton	m_butCosine;
	CButton	m_butSetIncrement;
	CButton	m_butUp;
	CButton	m_butTo;
	CButton	m_butDown;
	BOOL	m_bCosineTilt;
	CString	m_sTiltText;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTiltWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTiltWindow)
	afx_msg void OnTiltUp();
	afx_msg void OnTiltDown();
	afx_msg void OnTiltTo();
	afx_msg void OnTiltIncrement();
	afx_msg void OnCosineTilt();
	virtual BOOL OnInitDialog();
	afx_msg void OnTiltDelay();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	BOOL mProgramReady;		// Last state of program readiness to allow stage move
	double mTiltAngle;		// Last tilt angle received
	BOOL mStageReady;		// Last state of stage readiness
	CFont mFont;

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TILTWINDOW_H__5DBD326D_7072_4EC6_9DE2_B5A788F8FEDB__INCLUDED_)
