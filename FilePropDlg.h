#if !defined(AFX_FILEPROPDLG_H__685ABCC1_44DD_4F7A_8B35_56A899B84B39__INCLUDED_)
#define AFX_FILEPROPDLG_H__685ABCC1_44DD_4F7A_8B35_56A899B84B39__INCLUDED_

#include "FileOptions.h"	// Added by ClassView
#include "BaseDlg.h"
#include "afxwin.h"
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FilePropDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFilePropDlg dialog

class CFilePropDlg : public CBaseDlg
{
// Construction
public:
	CFilePropDlg(CWnd* pParent = NULL);   // standard constructor
	FileOptions mFileOpt;
  BOOL mAny16Bit;
  bool mShowDlgThisTime;

// Dialog Data
	//{{AFX_DATA(CFilePropDlg)
	enum { IDD = IDD_FILEPROPERTIES };
	CButton	m_butTruncate;
	CButton	m_butDivide;
	CButton	m_butShiftDown;
  CButton m_butSaveUnsigned;
	CButton	m_groupSaving16;
	CStatic	m_statTruncWhite;
	CEdit	m_editTruncWhite;
	CEdit	m_editTruncBlack;
	CStatic	m_statTruncBlack;
	CButton	m_groupTrunc;
	CEdit	m_editMaxSect;
	CStatic	m_statMaxSects;
	CStatic	m_statGenerous;
  CButton m_butNoComp;
  CButton m_butLZWComp;
  CButton m_butZIPComp;
  CButton m_butJPEGComp;
  CButton m_butTiffFile;
  int   m_iCompress;
	int		m_iMaxSects;
	int		m_iByteInt;
	BOOL	m_bStagePos;
	BOOL	m_bTiltAngle;
	BOOL	m_bMag;
	int		m_iTruncBlack;
	int		m_iTruncWhite;
	int		m_iUnsignOpt;
	BOOL	m_bIntensity;
  BOOL  m_bExposure;
  int   m_iFileType;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFilePropDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFilePropDlg)
	afx_msg void OnRbyte();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnKillfocusTruncblackedit();
	afx_msg void OnKillfocusTruncwhiteedit();
	afx_msg void OnKillfocusMaxsectsedit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void ManageStates();
public:
  CStatic m_statCompress;
  CStatic m_statExtended;
  CButton m_butTiltangle;
  CButton m_butIntensity;
  CButton m_butStagePos;
  CButton m_butMagnification;
  CButton m_butExpDose;
  CButton m_butSaveMdoc;
  BOOL m_bSaveMdoc;
  afx_msg void OnSaveMdoc();
  afx_msg void OnRNoCompress();
  CButton m_butSaveByte;
  CButton m_butSaveInteger;
  BOOL m_bSkipFileDlg;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILEPROPDLG_H__685ABCC1_44DD_4F7A_8B35_56A899B84B39__INCLUDED_)
