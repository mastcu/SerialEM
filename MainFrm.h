// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__742C775D_CDED_4003_AF84_5D0113017F26__INCLUDED_)
#define AFX_MAINFRM_H__742C775D_CDED_4003_AF84_5D0113017F26__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CMainFrame : public CMDIFrameWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	void SetDialogOffset(CSerialEMView *mainView);
	BOOL NewTask();
	void InitializeStatusBar();
  void RemoveHiddenItemsFromMenus();
  void RemoveItemsFromOneMenu(CMenu *menu, int level);
	void SetStatusText(int iPane, CString strText);
	void SetDialogPositions();
  void InitializeDialogPositions(int *initialState, RECT *dlgPlacements, 
    int *colorIndex);
	void InitializeDialogTable(DialogTable *inTable, int *initialState, int numDialog,
		COLORREF *borderColors, int *colorIndex, RECT *dlgPlacements);
  void InitializeOneDialog(DialogTable &inTable, int colorIndex, COLORREF *borderColors);
  int RemoveOneDialog(int colorIndex);
  void InsertOneDialog(CToolDlg *dlg, int colorInd, COLORREF *borderColors);
	void DialogChangedState(CToolDlg *inDialog, int inState);
	virtual ~CMainFrame();
  bool GetClosingProgram() {return mClosingProgram;};
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

  // This is allowed to be a public override, saving an extra function for public access
	afx_msg void OnWindowNew();

protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
#ifdef TASK_TIMER_INTERVAL
	afx_msg void OnTimer(UINT nIDEvent);
#endif
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	CFont mFont;
	int mNumDialogs;
	DialogTable * mDialogTable;
	UINT m_nTimer;
	CSerialEMApp *mWinApp;
  int mDialogOffset;
  int mLeftDialogOffset;
  bool mClosingProgram;
public:
  void NewWindow(void);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__742C775D_CDED_4003_AF84_5D0113017F26__INCLUDED_)
