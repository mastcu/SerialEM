#if !defined(AFX_EMSTATUSWINDOW_H__3E3D8658_6FC8_45DA_B0AA_FC7232A104CF__INCLUDED_)
#define AFX_EMSTATUSWINDOW_H__3E3D8658_6FC8_45DA_B0AA_FC7232A104CF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EMstatusWindow.h : header file
//

#include "ToolDlg.h"
class EMbufferManager;

#define MAX_STATUS  3
/////////////////////////////////////////////////////////////////////////////
// EMstatusWindow dialog

class EMstatusWindow : public CToolDlg
{
// Construction
public:
	void Update();
	EMstatusWindow(CWnd* pParent = NULL);   // standard constructor
  GetSetMember(BOOL, ShowDefocus);

// Dialog Data
	//{{AFX_DATA(EMstatusWindow)
	enum { IDD = IDD_BUFFERSTATUS };
	CSpinButtonCtrl	m_sbcChangeBuf;
	CString	m_sSizeText;
	CString	m_sStageText;
	CString	m_sTiltText;
	CString	m_strDefocus;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(EMstatusWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(EMstatusWindow)
	afx_msg void OnDeltaposSpin(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnCancelMode();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void SetStatusText(int indStat, int whichBuf);
	EMimageBuffer * mImBufs;
	EMbufferManager * mBufferManager;
	CString *mModeNames;
	CStatic *mBufText[MAX_STATUS];
  BOOL mShowDefocus;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EMSTATUSWINDOW_H__3E3D8658_6FC8_45DA_B0AA_FC7232A104CF__INCLUDED_)
