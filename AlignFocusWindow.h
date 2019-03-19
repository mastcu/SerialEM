#include "afxwin.h"
#if !defined(AFX_ALIGNFOCUSWINDOW_H__628C0BAD_CC5B_4067_A17C_0E9FBC497BAF__INCLUDED_)
#define AFX_ALIGNFOCUSWINDOW_H__628C0BAD_CC5B_4067_A17C_0E9FBC497BAF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AlignFocusWindow.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAlignFocusWindow dialog

class CAlignFocusWindow : public CToolDlg
{
// Construction
public:
	void UpdateSettings();
	void Update();
	CAlignFocusWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAlignFocusWindow)
	enum { IDD = IDD_ALIGNFOCUS };
	CButton	m_butTrimBorders;
	CButton	m_butCenterOnAxis;
	CButton	m_butResetShift;
	CButton	m_butFocus;
	CButton	m_butClearAlign;
	CButton	m_butAlign;
	BOOL	m_bMouseStage;
	BOOL	m_bCenterOnAxis;
	BOOL	m_bTrimBorders;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAlignFocusWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAlignFocusWindow)
	afx_msg void OnButalign();
	afx_msg void OnButclearalign();
	afx_msg void OnButresetshift();
	virtual BOOL OnInitDialog();
	afx_msg void OnCancelMode();
	afx_msg void OnButfocus();
	afx_msg void OnMousestage();
	afx_msg void OnButstagethresh();
	afx_msg void OnCenteronaxis();
	afx_msg void OnTrimborders();
	afx_msg void OnSetTrimFrac();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  bool mInitialized;

public:
  CString m_strDefTarget;
  CButton m_butApplyISoffset;
  BOOL m_bApplyISoffset;
  afx_msg void OnApplyISoffset();
  CButton m_butMouseStage;
  CStatic m_statDefTarget;
  BOOL m_bCorrectBacklash;
  afx_msg void OnCorrectBacklash();
  void UpdateAutofocus(int magInd);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ALIGNFOCUSWINDOW_H__628C0BAD_CC5B_4067_A17C_0E9FBC497BAF__INCLUDED_)
