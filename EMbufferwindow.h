#if !defined(AFX_EMBUFFERWINDOW_H__395A9DB8_6F05_4769_8DC8_FDCBD5D09FE5__INCLUDED_)
#define AFX_EMBUFFERWINDOW_H__395A9DB8_6F05_4769_8DC8_FDCBD5D09FE5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EMbufferWindow.h : header file
//
#include "ToolDlg.h"
#include "afxwin.h"
class EMbufferManager;
//#include "EMbufferManager.h"

/////////////////////////////////////////////////////////////////////////////
// CEMbufferWINDOW dialog

class CEMbufferWindow : public CToolDlg
{
// Construction
public:
	void UpdateSettings();
	void ProtectBuffer(int inBuf, BOOL inProtect);
	void UpdateSaveCopy();
	void SetSpinText(UINT inID, int newVal);
	CEMbufferWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEMbufferWindow)
	enum { IDD = IDD_BUFFERCONTROL };
	CStatic	m_statToFile;
	CSpinButtonCtrl	m_sbcToFile;
	CButton	m_butAlignToB;
	CButton	m_butConfirmDestroy;
	CButton	m_butSaveActive;
	CButton	m_butSaveA;
	CButton	m_butCopyNew;
	CButton	m_butCopyD;
	CButton	m_butCopyC;
	CButton	m_butCopyB;
	CButton	m_butCopyAny;
	CButton	m_butCopyA;
	CSpinButtonCtrl	m_sbcRead;
	CSpinButtonCtrl	m_sbcRoll;
	CSpinButtonCtrl	m_sbcCopy;
	BOOL	m_bAlignOnSave;
	BOOL	m_bConfirmDestroy;
	BOOL	m_bCopyOnSave;
	BOOL	m_bAlignToB;
	CString	m_strToFile;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEMbufferWindow)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEMbufferWindow)
	afx_msg void OnBcopya();
	afx_msg void OnBcopyb();
	afx_msg void OnBcopyc();
	afx_msg void OnBcopyd();
	afx_msg void OnBcopyany();
	afx_msg void OnBcopynew();
	afx_msg void OnBsavea();
	afx_msg void OnDeltaposSpincopy(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinroll(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinread(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnConfirmdestroy();
	afx_msg void OnBsaveactive();
	virtual BOOL OnInitDialog();
	afx_msg void OnAligntob();
	afx_msg void OnDeltaposSpintofile(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	EMimageBuffer * mImBufs;
	void ManageCopyOnSave(int inVal);
	EMbufferManager * mBufferManager;
	void DoCopyBuffer(int iWhich);
public:
  CButton m_butDelete;
  afx_msg void OnClickedDeletebuf();
  int ActiveBufNumIfDeletable(void);
  CString m_strMemory;
  double MemorySummary(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EMBUFFERWINDOW_H__395A9DB8_6F05_4769_8DC8_FDCBD5D09FE5__INCLUDED_)
