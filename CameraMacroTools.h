#if !defined(AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_)
#define AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CameraMacroTools.h : header file
//

#include "MyButton.h"

#define NUM_CAM_MAC_PANELS 5
#define NUM_SPINNER_MACROS (3 * (NUM_CAM_MAC_PANELS - 1))
enum {NO_NAV_RUNNING = 0, NAV_RUNNING_NO_SCRIPT_TS, NAV_PAUSED, NAV_TS_RUNNING,
  NAV_TS_STOPPED, NAV_PRE_TS_RUNNING, NAV_PRE_TS_STOPPED, NAV_SCRIPT_RUNNING,
  NAV_SCRIPT_STOPPED};

class CMacCmd;
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
  void UpdateHiding(void);
  GetSetMember(BOOL, UserStop);
  GetMember(bool, DeferredUserStop);
	CCameraMacroTools(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CCameraMacroTools)
	enum { IDD = IDD_CAMERA_MACRO };
	CSpinButtonCtrl	m_sbcMacro3;
	CSpinButtonCtrl	m_sbcMacro2;
	CSpinButtonCtrl	m_sbcMacro1;
	CStatic	m_statTopline;
	CMyButton	m_butStop;
	CMyButton	m_butMacro3;
	CMyButton	m_butMacro2;
	CMyButton	m_butMacro1;
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
  afx_msg void OnDeltaposSpinmacroN(UINT nID, NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnButmacro1();
	afx_msg void OnButmacro2();
  afx_msg void OnButmacro3();
  afx_msg void OnButmacroN(UINT nID);
  afx_msg void OnButstop();
	afx_msg void OnButend();
	afx_msg void OnButresume();
	afx_msg void OnButmontage();
	afx_msg void OnPaint();
  afx_msg void OnMacBut1Draw(NMHDR *pNotifyStruct, LRESULT *result);
  afx_msg void OnMacBut2Draw(NMHDR *pNotifyStruct, LRESULT *result);
  afx_msg void OnMacBut3Draw(NMHDR *pNotifyStruct, LRESULT *result);
  afx_msg void OnMacButNDraw(UINT nID, NMHDR *pNotifyStruct, LRESULT *result);
  //}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CMacCmd *mMacProcessor;
  CNavigatorDlg *mNav;   // Set by GetNavigatorState
	BOOL mDoingTS;
  BOOL mDoingCalISO;
	CString *mMacros;
	int mMacroNumber[NUM_SPINNER_MACROS];
  BOOL mUserStop;     // Flag that user pushed stop
  bool mEnabledSearch;
  bool mDeferredUserStop;
  bool mMediumWasEmpty;
  int mLastRowsShows;
  int mPanelStart[NUM_CAM_MAC_PANELS];
  int mNumInPanel[NUM_CAM_MAC_PANELS];
public:
  void HandleMacroRightClick(CMyButton *but, int index, bool openOK);
  void SetOneMacroLabel(int num, UINT nID);
  void MacroNameChanged(int num);
  void ManagePanels();
  void CalibratingISoffset(bool ifCal);
  int GetNavigatorState(void);
  void DoUserStop(void);
  afx_msg void OnButSetupCam();
  afx_msg void OnButVFTR(UINT nID);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CAMERAMACROTOOLS_H__A69B36EC_7BDB_42EC_BF5B_17C2F2A66073__INCLUDED_)
