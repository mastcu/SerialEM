#if !defined(AFX_TSEXTRAFILE_H__A16D831F_FF9B_4BBD_B11F_16E11DD39A69__INCLUDED_)
#define AFX_TSEXTRAFILE_H__A16D831F_FF9B_4BBD_B11F_16E11DD39A69__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TSExtraFile.h : header file
//
#include "BaseDlg.h"
#include "afxcmn.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// CTSExtraFile dialog

class CTSExtraFile : public CBaseDlg
{
// Construction
public:
	int ValidateRecordEntries(int whichRecord);
	void ManageExtraRecords();
	void FormatNumAndName(int index, int statID);
	void DeltaposSpin(int index, int statID, NMHDR *pNMHDR, LRESULT *pResult);
	CTSExtraFile(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTSExtraFile)
	enum { IDD = IDD_TSEXTRAFILE };
	CEdit	m_editEntries;
	CButton	m_butTrial;
	CButton	m_butPreview;
	CButton	m_butFocus;
	CButton	m_butView;
	CSpinButtonCtrl	m_sbcView;
	CSpinButtonCtrl	m_sbcTrial;
	CSpinButtonCtrl	m_sbcRecord;
	CSpinButtonCtrl	m_sbcPreview;
	CSpinButtonCtrl	m_sbcFocus;
	BOOL	m_bSaveView;
	BOOL	m_bSaveFocus;
	BOOL	m_bSavePreview;
	BOOL	m_bSaveTrial;
	CString	m_strExtraRec;
	int		m_iWhichRecord;
	CString	m_strInstruct1;
	CString	m_strInstruct2;
	CString	m_strEntries;
	//}}AFX_DATA

  int mFileIndex[MAX_EXTRA_SAVES];
  BOOL mSaveOn[MAX_EXTRA_SAVES];
  int mNumExtraFocus;
  int mNumExtraExposures;
  int mNumExtraFilter;
  int mNumExtraChannels;
  double mExtraFocus[MAX_EXTRA_RECORDS];
  double mExtraExposures[MAX_EXTRA_RECORDS];
  int mExtraSlits[MAX_EXTRA_RECORDS];
  double mExtraLosses[MAX_EXTRA_RECORDS];
  int mExtraChannels[MAX_STEM_CHANNELS];
  int mMaxStackSizeXY;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTSExtraFile)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTSExtraFile)
	afx_msg void OnDeltaposSpinfocus(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinpreview(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinrecord(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpintrial(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinview(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnRecordRadio();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
  CSpinButtonCtrl m_sbcBinSize;
  CString m_strKBpImage;
  BOOL m_bStackRecord;
  afx_msg void OnDeltaposSpinbinsize(NMHDR *pNMHDR, LRESULT *pResult);
  BOOL m_bSetSpot;
  BOOL m_bSetExposure;
  CStatic m_statNewSpot;
  CString m_strNewSpot;
  CSpinButtonCtrl m_sbcNewSpot;
  CEdit m_editNewExposure;
  CEdit m_editNewDrift;
  CEdit m_editDeltaC2;
  float m_fDeltaC2;
  float m_fNewDrift;
  int mSpotSize;
  int mNumSpotSizes;
  int mBinIndex;
  CameraParameters *mCamParam;
  afx_msg void OnCheckSetspot();
  afx_msg void OnCheckSetexposure();
  afx_msg void OnDeltaposSpinNewspot(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butTrialBin;
  CButton m_butOppositeTrial;
  BOOL m_bTrialBin;
  CStatic m_statTrialBin;
  CString m_strTrialBin;
  CSpinButtonCtrl m_sbcTrialBin;
  afx_msg void OnCheckTrialBin();
  afx_msg void OnDeltaposSpinTrialBin(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butSetSpot;
  CString m_strExposures;
  BOOL m_bKeepBidirAnchor;
  CButton m_butConsecutiveFiles;
  BOOL m_bConsecutiveFiles;
  CString m_statConsecList;
  afx_msg void OnConsecutiveFiles();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TSEXTRAFILE_H__A16D831F_FF9B_4BBD_B11F_16E11DD39A69__INCLUDED_)
