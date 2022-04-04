#if !defined(AFX_MONTAGESETUPDLG_H__2B4E7C65_9717_459C_8381_56D154F0FF91__INCLUDED_)
#define AFX_MONTAGESETUPDLG_H__2B4E7C65_9717_459C_8381_56D154F0FF91__INCLUDED_

#include "MontageParam.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MontageSetupDlg.h : header file
//

#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"

/////////////////////////////////////////////////////////////////////////////
// CMontageSetupDlg dialog

class CMontageSetupDlg : public CBaseDlg
{
// Construction
public:
	void ManageCameras();
	BOOL SizeIsFeasible(int camInd, int binning);
	void ValidateEdits();
	void UpdateSizes();
	BOOL mSizeLocked;
	MontParam mParam;
	CMontageSetupDlg(CWnd* pParent = NULL);   // standard constructor
	int mMaxPieces;		// Maximum pieces in X or Y
	int mMinOverlap;	// Minimum overlap
  BOOL mConstrainSize;
  BOOL mLowDoseMode;

// Dialog Data
	//{{AFX_DATA(CMontageSetupDlg)
	enum { IDD = IDD_MONTAGESETUP };
	CButton	m_butIgnoreSkips;
	CButton	m_butCameraBox;
	CButton	m_butUpdate;
	CEdit	m_editXoverlap;
	CEdit	m_editYoverlap;
	CStatic	m_statY3;
	CEdit	m_editYsize;
	CEdit	m_editXsize;
	CEdit	m_editYnFrames;
	CEdit	m_editXnFrames;
	CStatic	m_statOverlap;
	CStatic	m_statPieceSize;
	CStatic	m_statNPiece;
	CStatic	m_statY2;
	CStatic	m_statY1;
	CSpinButtonCtrl	m_sbcMag;
	CSpinButtonCtrl	m_sbcBinning;
	CSpinButtonCtrl	m_sbcYnFrames;
	CSpinButtonCtrl	m_sbcXnFrames;
	int		m_iXnFrames;
	int		m_iYnFrames;
	int		m_iXsize;
	int		m_iYsize;
	CString	m_strTotalArea;
	CString	m_strPixelSize;
	CString	m_strMag;
	CString	m_strBinning;
	int		m_iXoverlap;
	int		m_iYoverlap;
	CString	m_strTotalPixels;
	int		m_iCamera;
	BOOL	m_bMoveStage;
	BOOL	m_bIgnoreSkips;
  CStatic m_statMinOvLabel;
  CString m_strMinOvValue;
  CStatic m_statMinOvValue;
  CSpinButtonCtrl m_sbcMinOv;
  BOOL m_bSkipOutside;
  CEdit m_editSkipOutside;
  int m_iInsideNavItem;
  CButton m_butOfferMap;
  BOOL m_bOfferMap;
  CButton m_butUseHq;
  BOOL m_bUseHq;
  BOOL m_bFocusAll;
  BOOL m_bFocusBlocks;
  //CButton m_butRealign;
  //BOOL m_bRealign;
  BOOL m_bSkipCorr;
  CStatic m_statBlockSize;
  CString m_strBlockSize;
  CStatic m_statBlockPieces;
  CSpinButtonCtrl m_sbcBlockSize;
  //CEdit m_editRealign;
  //int m_iRealignInterval;
  //CStatic m_statRealignPieces;
  CEdit m_editDelay;
  float m_fDelay;
  CStatic m_statMinAnd;
  CStatic m_statMinMicron;
  CEdit m_editMinMicron;
  float m_fMinMicron;

	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMontageSetupDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMontageSetupDlg)
	afx_msg void OnKillfocusEdits();
	afx_msg void OnDeltaposSpinbin(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinmag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinxframes(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinyframes(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnUpdate();
	afx_msg void OnRcamera();
	afx_msg void OnMovestage();
  afx_msg void OnCheckSkipOutside();
  afx_msg void OnCheckOfferMap();
  afx_msg void OnDeltaposSpinminov(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnCheckUseHqSettings();
  afx_msg void OnCheckFocusEach();
  afx_msg void OnCheckFocusBlocks();
  afx_msg void OnDeltaposSpinBlockSize(NMHDR *pNMHDR, LRESULT *pResult);
  //afx_msg void OnCheckRealign();
  afx_msg void OnCheckIsRealign();
  //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	MagTable *mMagTab;
	CameraParameters *mCamParams;
	int mCurrentCamera;
  int mSizeScaling;
	int *mActiveCameraList;
	int mNumCameras;
  BOOL mLDsetNum;
  bool mMismatchedModes;
  LowDoseParams *mLdp;
  BOOL mForceStage;
  BOOL mFittingItem;   // Flag that this is being run from Navigator for fitting
  int mCamSizeX, mCamSizeY;
  float mMaxOverFrac;
  bool mNoneFeasible;
  BOOL mCamFeasible[MAX_CAMERAS];
  int mPanelStart[5];
  int mNumInPanel[5];
  int mLastRecordMoveStage;
  int mXoverlap, mYoverlap;

public:
  int CheckNavigatorFit(int magIndex, int binning, float minOverlap = -1.f, 
    float minMicrons = -1.f, BOOL switchingVinLD = FALSE);
  void LoadParamData(BOOL setPos);
  void ManageSizeFields(void);
  void LoadOverlapBoxes();
  void UnloadOverlapBoxes();
  void ManageStageAndGeometry(BOOL reposition);
  void UnloadParamData(void);
  void MinOverlapsInPixels(int & minOverX, int & minOverY);
  CButton m_butResetOverlaps;
  afx_msg void OnButResetOverlaps();
  afx_msg void OnEnKillfocusEditMinOverlap();
  CButton m_butFocusEach;
  CButton m_butBlockFocus;
  CStatic m_statMaxAlign;
  CEdit m_editMaxAlign;
  int m_iMaxAlign;

  CButton m_butISrealign;
  BOOL m_bISrealign;
  CStatic m_statISmicrons;
  CEdit m_editISlimit;
  float m_fISlimit;

  CButton m_butUseAnchor;
  BOOL m_bUseAnchor;
  CStatic m_statAnchorMag;
  CString m_strAnchorMag;
  CSpinButtonCtrl m_sbcAnchorMag;
  afx_msg void OnCheckUseAnchor();
  afx_msg void OnDeltaposSpinAnchorMag(NMHDR *pNMHDR, LRESULT *pResult);
  CButton m_butRepeatFocus;
  BOOL m_bRepeatFocus;
  CEdit m_editNmPerSec;
  float m_fNmPerSec;
  CStatic m_statNmPerSec;
  afx_msg void OnCheckRepeatFocus();
  BOOL m_bUseViewInLowDose;
  afx_msg void OnUseViewInLowdose();
  CButton m_butUseViewInLD;
  BOOL m_bSkipReblank;
  CButton m_butNoDriftCorr;
  BOOL m_bNoDriftCorr;
  CButton m_butUseContinMode;
  BOOL m_bUseContinMode;
  CEdit m_editContinDelayFac;
  float m_fContinDelayFac;
  afx_msg void OnCheckContinuousMode();
  void ManageCamAndStageEnables(void);
  CButton m_butUseSearchInLD;
  BOOL m_bUseSearchInLD;
  afx_msg void OnUseSearchInLD();
  void UseViewOrSearchInLD(BOOL & otherFlag, BOOL &secondFlag);
  CButton m_butUseMontMapParams;
  BOOL m_bUseMontMapParams;
  afx_msg void OnUseMontMapParams();
  CButton m_butUseMultishot;
  BOOL m_bUseMultishot;
  afx_msg void OnUseMultishot();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MONTAGESETUPDLG_H__2B4E7C65_9717_459C_8381_56D154F0FF91__INCLUDED_)
