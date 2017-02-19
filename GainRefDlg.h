#if !defined(AFX_GAINREFDLG_H__FACA1DFC_E9F7_423D_89FC_579404191520__INCLUDED_)
#define AFX_GAINREFDLG_H__FACA1DFC_E9F7_423D_89FC_579404191520__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GainRefDlg.h : header file
//

#include "BaseDlg.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// CGainRefDlg dialog

class CGainRefDlg : public CBaseDlg
{
// Construction
public:
	void ManageExistLine();
	CGainRefDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CGainRefDlg)
	enum { IDD = IDD_GAINREFERENCE };
	CStatic	m_statTimes;
	CSpinButtonCtrl	m_spinAverage;
	CEdit	m_editNumAverage;
	CButton	m_butAverageDark;
	CButton	m_butCalDose;
	CSpinButtonCtrl	m_sbcTarget;
	CSpinButtonCtrl	m_sbcFrames;
	int		m_iFrames;
	int		m_iTarget;
	int		m_iBinning;
	CString	m_strExisting;
	CString	m_strOddRef;
	BOOL	m_bCalDose;
	BOOL	m_bAverageDark;
	int		m_iNumAverage;
	//}}AFX_DATA

  BOOL mRefExists[20];
  int mNumBinnings;
  int mBinnings[20];
  CString mName;
  int mCamera;
  int mIndOffset;
  BOOL m_bGainCalibrated;
  BOOL mCanAutosave;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGainRefDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGainRefDlg)
	afx_msg void OnKillfocusEditframes();
	afx_msg void OnKillfocusEdittarget();
	afx_msg void OnRadiobin();
	afx_msg void OnDeltaposSpinframes(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpintarget(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnCheckCalDose();
	afx_msg void OnKillfocusGrEditaverage();
	afx_msg void OnDeltaposGrSpinaverage(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:

public:
  CButton m_butAutosaveRaw;
  BOOL m_bAutosaveRaw;
  afx_msg void OnAutosaveRaw();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GAINREFDLG_H__FACA1DFC_E9F7_423D_89FC_579404191520__INCLUDED_)
