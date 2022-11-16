#if !defined(AFX_TOOLDLG_H__2CC8BDE7_E77D_443C_B828_8657B720D9EC__INCLUDED_)
#define AFX_TOOLDLG_H__2CC8BDE7_E77D_443C_B828_8657B720D9EC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ToolDlg.h : header file
//
#include "setdpi.h"
#include "BaseDlg.h"

#define TOOL_OPENCLOSED 1
#define TOOL_FULLOPEN   2
#define TOOL_FLOATDOCK  4

class CSerialEMApp;

/////////////////////////////////////////////////////////////////////////////
// CToolDlg dialog

class CToolDlg : public CBaseDlg
{
// Construction
public:
	void SetBorderColor(COLORREF inColor);
	void ToggleState(int inFlag);
	virtual int GetMidHeight();
	CToolDlg(UINT inIDD, CWnd* pParent = NULL);   // standard constructor
	void SetOpenClosed(int inState);
  virtual void ManagePanels();

// Dialog Data
	//{{AFX_DATA(CToolDlg)
//	enum { IDD = _UNKNOWN_RESOURCE_ID_ };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CToolDlg)
	afx_msg void OnButopen();
	afx_msg void OnButmore();
	virtual BOOL OnInitDialog();
	afx_msg void OnCancel();
	afx_msg void OnButfloat();
	afx_msg void OnMove(int x, int y);
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	CSerialEMApp * mWinApp;
	CFont *mLittleFont;
	CString mBigFontName;	// System-dependent name for nice font
	COLORREF mBorderColor;	// Color for border
  UINT mIDD;      // Dialog ID
  CSetDPI mSetDPI;
	CFont mButFont;		// Needs a member font to stay resident
  int mPanelStart[3];
  int mNumInPanel[3];
  int *mIdTable;
  int *mLeftTable;
  int *mTopTable;
  int *mHeightTable;

private:
	int mMidHeight;	  // The height when midway open
	int mState;			// The state flags
public:
  bool mInitialized;
  void DrawSideBorders(CPaintDC & dc);
  void DrawButtonOutline(CPaintDC & dc, CWnd * but, int thickness, COLORREF color);
  int CurrentButHeight(CButton *butMore);
  bool HideOrShowOptions(int showHide);
  virtual void UpdateHiding(void);
  GetMember(int, State);
  void SetupPanels(int * idTable, int * leftTable, int * topTable,
    int *heightTable = NULL, int sortStart = 0);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLDLG_H__2CC8BDE7_E77D_443C_B828_8657B720D9EC__INCLUDED_)
