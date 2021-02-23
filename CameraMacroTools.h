#if !defined(AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_)
#define AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CameraMacroTools.h : header file
//

enum {NO_NAV_RUNNING = 0, NAV_RUNNING_NO_SCRIPT_TS, NAV_PAUSED, NAV_TS_RUNNING,
  NAV_TS_STOPPED, NAV_PRE_TS_RUNNING, NAV_PRE_TS_STOPPED, NAV_SCRIPT_RUNNING,
  NAV_SCRIPT_STOPPED};

class CMacroProcessor;
class CNavigatorDlg;

/////////////////////////////////////////////////////////////////////////////
// CCameraMacroTools dialog

class CCameraMacroTools : public CToolDlg
{
// Construction
public:
	void UpdateSettings();
	void SetMacroLabels();
	void DoingTiltSeries(BOOL ifTS);
	int *GetMacroNumbers() {return &mMacroNumber[0];};
	void DoMacro(int inMacro);
	void DeltaposSpin(NMHDR *pNMHDR, LRESULT *pResult, UINT nID, int iMacro);
	void Update();
  GetSetMember(BOOL, UserStop)
	CCameraMacroTools(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CCameraMacroTools)
	enum { IDD = IDD_CAMERA_MACRO };
	CSpinButtonCtrl	m_sbcMacro3;
	CSpinButtonCtrl	m_sbcMacro2;
	CSpinButtonCtrl	m_sbcMacro1;
	CStatic	m_statTopline;
	CButton	m_butStop;
	CButton	m_butMacro3;
	CButton	m_butMacro2;
	CButton	m_butMacro1;
	CButton	m_butResume;
	CButton	m_butEnd;
	CButton	m_butView;
	CButton	m_butTrial;
	CButton	m_butRecord;
	CButton	m_butSetup;
	CButton	m_butMontage;
	CButton	m_butFocus;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCameraMacroTools)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCameraMacroTools)
	virtual BOOL OnInitDialog();
	afx_msg void OnDeltaposSpinmacro1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinmacro2(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinmacro3(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButmacro1();
	afx_msg void OnButmacro2();
	afx_msg void OnButmacro3();
	afx_msg void OnButstop();
	afx_msg void OnButend();
	afx_msg void OnButresume();
	afx_msg void OnButmontage();
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CMacroProcessor *mMacProcessor;
  CNavigatorDlg *mNav;   // Set by GetNavigatorState
	BOOL mDoingTS;
  BOOL mDoingCalISO;
	CString *mMacros;
	int mMacroNumber[6];
  BOOL mUserStop;     // Flag that user pushed stop
  bool mEnabledSearch;
  bool mEnabledStop;
public:
  void SetOneMacroLabel(int num, UINT nID);
  void MacroNameChanged(int num);
  void CalibratingISoffset(bool ifCal);
  int GetNavigatorState(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_)
