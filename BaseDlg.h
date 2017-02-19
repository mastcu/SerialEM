#if !defined(AFX_BASEDLG_H__CC943E90_5555_4774_977F_682E408500B8__INCLUDED_)
#define AFX_BASEDLG_H__CC943E90_5555_4774_977F_682E408500B8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BaseDlg.h : header file
//
#include "setdpi.h"
#define PANEL_END -1
#define TABLE_END -2
#define MAX_SKIP_IDS 8

/////////////////////////////////////////////////////////////////////////////
// CBaseDlg dialog

class CBaseDlg : public CDialog
{
// Construction
public:
	CBaseDlg(UINT inIDD, CWnd* pParent = NULL);   // standard constructor
  BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );

// Dialog Data
	//{{AFX_DATA(CBaseDlg)
	//enum { IDD = _UNKNOWN_RESOURCE_ID_ };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBaseDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CBaseDlg)
		// NOTE: the ClassWizard will add member functions here
	afx_msg void OnButhelp();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  UINT mIDD;      // Dialog ID
  CSetDPI mSetDPI;
protected:
  CSerialEMApp *mWinApp;
  BOOL mNonModal;
  int mNumPanels;
  int mBasicWidth;
  int mIDsToHide[MAX_SKIP_IDS];
  int mNumIDsToHide;
  int mSavedTopPos;
  int mIDToSaveTop;
public:
  CButton m_butHelp;
  void ReplaceWindowText(CWnd * wnd, const char * fromText, CString toText);
  void ReplaceDlgItemText(int nID, const char * fromText, CString toText);
  void SetupPanelTables(int * idTable, int * leftTable, int * topTable, int * numInPanel,
    int * panelStart);
  void AdjustPanels(BOOL *states, int *idTable, int *leftTable, int *topTable, 
                                int *numInPanel, int *panelStart, int numCameras);
  void EnableDlgItem(int nID, BOOL enable);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BASEDLG_H__CC943E90_5555_4774_977F_682E408500B8__INCLUDED_)
