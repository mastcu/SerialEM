#if !defined(AFX_IMAGELEVELDLG_H__11A4A056_8BD5_417A_B430_0C8CA4E6951A__INCLUDED_)
#define AFX_IMAGELEVELDLG_H__11A4A056_8BD5_417A_B430_0C8CA4E6951A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ImageLevelDlg.h : header file
//
class CSerialEMApp;

#include "ToolDlg.h"
#include "afxwin.h"
#include "afxcmn.h"

/////////////////////////////////////////////////////////////////////////////
// CImageLevelDlg dialog

class CImageLevelDlg : public CToolDlg
{
// Construction
public:
	void SetEditBoxes();
	void UpdateSettings();
	void NewZoom(double inZoom);
	void NewImageScale(KImageScale *inImageScale);
	void ProcessEditBoxes();
	void OnOK();
	void AnalyzeImage();
	CImageLevelDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CImageLevelDlg)
	enum { IDD = IDD_IMAGELEVEL };
	CButton	m_butTruncation;
	CStatic	m_statWhite;
	CButton	m_butAreaFrac;
	CButton	m_butAutozoom;
	CStatic	m_statZoom;
	CStatic	m_statTitle;
	CStatic	m_statCon;
	CStatic	m_statBlack;
	CStatic	m_statBri;
	CSpinButtonCtrl	m_sbcZoom;
	CEdit	m_eZoom;
	CEdit	m_eWhiteLevel;
	CEdit	m_eBlackLevel;
	CSliderCtrl	m_scContrast;
	CSliderCtrl	m_scBright;
	int		mBrightSlider;
	int		mContrastSlider;
	CString	m_editZoom;
	BOOL	m_bAutozoom;
	CString	m_strWhite;
	CString	m_strBlack;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CImageLevelDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CImageLevelDlg)
	afx_msg void OnTruncation();
	afx_msg void OnAreafraction();
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnKillfocusBW();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnDeltaposSpinzoom(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillfocusEditzoom();
	afx_msg void OnAutozoom();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	int mOpenClosed;
	float mPctLo, mPctHi;
  float mWhiteLevel, mBlackLevel;
  float mSampleMin, mSampleMax;
	float mAreaFrac;
	CFont mFont;
public:
  afx_msg void OnScalebar();
  void ToggleExtraInfo(void);
  BOOL m_bScaleBars;
  BOOL m_bCrosshairs;
  CButton m_butScaleBars;
  CButton m_butCrosshairs;
  afx_msg void OnCrosshairs();
  void ToggleCrosshairs();
  BOOL m_bAntialias;
  afx_msg void OnAntialias();
  CButton m_butAntialias;
  BOOL m_bInvertCon;
  afx_msg void OnInvertContrast();
  CButton m_butInvertCon;
  void ToggleInvertContrast(void);
  CSliderCtrl m_scBlack;
  int mBlackSlider;
  CSliderCtrl m_scWhite;
  int mWhiteSlider;
  void ProcessNewBlackWhite(void);
  BOOL m_bTiltAxis;
  afx_msg void OnTiltaxis();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IMAGELEVELDLG_H__11A4A056_8BD5_417A_B430_0C8CA4E6951A__INCLUDED_)
