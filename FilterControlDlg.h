#if !defined(AFX_FILTERCONTROLDLG_H__BAEAC419_DB1F_4198_8372_577254D8C6AA__INCLUDED_)
#define AFX_FILTERCONTROLDLG_H__BAEAC419_DB1F_4198_8372_577254D8C6AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FilterControlDlg.h : header file
//
#define MINIMUM_SLIT_WIDTH 5.0f
#define MAXIMUM_SLIT_WIDTH 50.0f
#define MAXIMUM_ENERGY_LOSS  3000.0f
#define MINIMUM_ENERGY_LOSS  -100.f

/////////////////////////////////////////////////////////////////////////////
// CFilterControlDlg dialog

class CFilterControlDlg : public CToolDlg
{
// Construction
public:
  int GetMidHeight();
	double LossToApply();
	void AdjustForZLPAlign();
	void ManageZLPAligned();
	float LookUpShift(int inMag);
	void ManageMagShift();
	void NewMagUpdate(int inMag, BOOL EFTEM);
	void OnOK();
	void UpdateSettings();
	void Update();
	CFilterControlDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFilterControlDlg)
	enum { IDD = IDD_FILTERCONTROL };
	CButton	m_butOffsetSlit;
	CStatic	m_statMore;
	CButton	m_butMore;
	CButton	m_butRefineZLP;
	CStatic	m_statOffset;
	CButton	m_butZeroLoss;
	CButton	m_butZLPAligned;
	CStatic	m_statMagShift;
	CButton	m_butDoMagShifts;
	CButton	m_butMatchIntensity;
	CButton	m_butMatchPixel;
	CStatic	m_statOnOff;
	CButton	m_butSlitIn;
	CButton	m_butEFTEMMode;
	CButton	m_butAutoMag;
	CButton	m_butAutoCamera;
	CSpinButtonCtrl	m_sbcWidth;
	CSpinButtonCtrl	m_sbcLoss;
	CEdit	m_editWidth;
	CEdit	m_editLoss;
	BOOL	m_bAutoMag;
	BOOL	m_bAutoCamera;
	BOOL	m_bEFTEMMode;
	BOOL	m_bSlitIn;
	CString	m_strLoss;
	CString	m_strWidth;
	CString	m_strOnOff;
	BOOL	m_bMatchPixel;
	BOOL	m_bMatchIntensity;
	BOOL	m_bDoMagShifts;
	CString	m_strMagShift;
	CString	m_strOffset;
	BOOL	m_bZeroLoss;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFilterControlDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFilterControlDlg)
	afx_msg void OnKillfocusEdits();
	afx_msg void OnOptionButton();
	afx_msg void OnDeltaposSpinwidth(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinloss(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnEftemmode();
	afx_msg void OnSlitin();
	afx_msg void OnZlpaligned();
	afx_msg void OnDomagshifts();
	afx_msg void OnZeroloss();
	afx_msg void OnRefineZlp();
	afx_msg void OnOffsetSlit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	FilterParams *mParam;
	CFont mFont;
	BOOL mLensEFTEM;
  float mSlitOffset;
  int mLastMagInLM;
  int mWarnedJeolLM;
public:
  BOOL LossOutOfRange(double nominal, double & netLoss);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILTERCONTROLDLG_H__BAEAC419_DB1F_4198_8372_577254D8C6AA__INCLUDED_)
