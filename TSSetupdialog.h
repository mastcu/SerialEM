#if !defined(AFX_TSSETUPDIALOG_H__026F1402_5811_4754_BCC6_98F2135E362E__INCLUDED_)
#define AFX_TSSETUPDIALOG_H__026F1402_5811_4754_BCC6_98F2135E362E__INCLUDED_

#include "TiltSeriesParam.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TSSetupDialog.h : header file
//

#include "BaseDlg.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// CTSSetupDialog dialog

class CTSSetupDialog : public CBaseDlg
{
// Construction
public:
	void ManageRefineZLP();
	void ManageInitialActions();
	void ManageFocusIntervals();
  void ManageCameras();
  int RegularOrEFTEMMag(int magIndex);
  void ManageNewTrackDiff();
  void ManageTaperLine();
  void SetTotalTilts();
  void SetPixelSize();
  void ManageAnchorBuffer();
  TiltSeriesParam mTSParam;
  CTSSetupDialog(CWnd* pParent = NULL);   // standard constructor
  BOOL mDoingTS;
  int mBidirStage;
  BOOL mPostpone;
  BOOL mSingleStep;
  BOOL mLowDoseMode;
  BOOL mCanFindEucentricity;
  double mCurrentAngle;
  int mMeanInBufferA;           // Equivalent record counts of image in buffer A
  int mFuture;
  BOOL mMontaging;              // Should be set by the caller
  float mMaxTiltAngle;
  float mMaxDelayAfterTilt;

// Dialog Data
  //{{AFX_DATA(CTSSetupDialog)
	enum { IDD = IDD_TILTSERIES };
	CButton	m_butAverageDark;
	CButton	m_butCloseValves;
	CEdit	m_editBigShift;
	CButton	m_butRefineZLP;
	CEdit	m_editZlpInterval;
	CStatic	m_statMinutes;
	CButton	m_butIntensitySetAtZero;
	CStatic	m_statCurrentCounts;
	CButton	m_butLimitToCurrent;
	CSpinButtonCtrl	m_sbcCosinePower;
	CEdit	m_editCheckFocus;
	CButton	m_butAlwaysFocus;
	CButton	m_butSkipAutofocus;
	CStatic	m_statCheckDeg;
	CStatic	m_statCheckEvery;
	CStatic	m_statAlwaysFocus;
	CButton	m_butPreviewBeforeRef;
	CButton	m_butLimitIntensity;
	CStatic	m_statTimesActual;
	CButton	m_butCheckAutofocus;
  CButton m_butLimitAbsFocus;
  CButton m_butLimitDeltaFocus;
  BOOL m_bLimitDeltaFocus;
  BOOL m_bLimitAbsFocus;
  CEdit m_editFocusMaxDelta;
  float m_fFocusMaxDelta;
  CStatic m_statDeltaMicrons;
	CEdit	m_editMinFocusAccuracy;
  CButton m_butAlignTrackOnly;
  CStatic m_statPercent;
  CStatic m_statNewTrack;
  CEdit m_editNewTrackDiff;
  CButton m_butConstantBeam;
  CButton m_butIntensityMean;
  CButton m_butIntensityCosine;
  CButton m_statBeamBox;
  CButton m_butCurrentBox;
  CButton m_butUseAforRef;
  CEdit m_editTaperAngle;
  CEdit m_editTaperCounts;
  CButton m_butRampBeam;
  CStatic m_statTaperFta;
  CStatic m_statTaperDeg;
  CEdit m_editAlwaysAngle;
  CStatic m_statLowMag;
  CEdit m_editRepeatRecord;
  CEdit m_editRepeatFocus;
  CEdit m_editCounts;
  CButton m_butUseAnchor;
  CButton m_butWalkBox;
  CStatic m_statStartAt;
  CStatic m_statRecordMag;
  CStatic m_statMagLabel;
  CStatic m_statBinning;
  CStatic m_statBinLabel;
  CStatic m_statbasicInc;
  CStatic m_statAnchorDeg;
  CStatic m_statAnchorBuf;
  CSpinButtonCtrl m_sbcTrackMag;
  CSpinButtonCtrl m_sbcRecordMag;
  CSpinButtonCtrl m_sbcBin;
  CSpinButtonCtrl m_sbcAnchor;
  CButton m_butRefineEucen;
  CButton m_butLowMagTrack;
  CButton m_butLeaveAnchor;
  CEdit m_editStartAngle;
  CEdit m_editIncrement;
  CEdit m_editAnchor;
  CButton m_butCosineInc;
  BOOL  m_bCosineInc;
  float m_fAnchorAngle;
  float m_fBeamTilt;
  float m_fCheckFocus;
  int   m_iCounts;
  float m_fEndAngle;
  float m_fIncrement;
  float m_fLimitIS;
  float m_fRepeatFocus;
  float m_fStartAngle;
  float m_fTiltDelay;
  BOOL  m_bLeaveAnchor;
  BOOL  m_bLowMagTrack;
  int   m_iBeamControl;
  BOOL  m_bRefineEucen;
  BOOL  m_bRepeatFocus;
  BOOL  m_bRepeatRecord;
  CString m_strBinning;
  CString m_strLowMag;
  CString m_strPixel;
  CString m_strRecordMag;
  CString m_strTotalTilt;
  BOOL  m_bUseAnchor;
  float m_fRepeatRecord;
  float m_fTargetDefocus;
  BOOL  m_bUseAForRef;
  BOOL  m_bAlwaysFocus;
  float m_fAlwaysAngle;
  BOOL  m_bRampBeam;
  float m_fTaperAngle;
  int   m_iTaperCounts;
  float m_fNewTrackDiff;
  BOOL  m_bAlignTrackOnly;
  float m_fPredictErrorXY;
  float m_fPredictErrorZ;
  int   m_iCamera;
	BOOL	m_bCheckAutofocus;
	float	m_fMinFocusAccuracy;
	BOOL	m_bLimitIntensity;
	BOOL	m_bPreviewBeforeRef;
	BOOL	m_bSkipAutofocus;
	float	m_fFocusOffset;
	int		m_iTrackAfterFocus;
	BOOL	m_bLimitToCurrent;
	CString	m_strTotalDose;
	BOOL	m_bIntensitySetAtZero;
	BOOL	m_bRefineZLP;
	int		m_iZlpInterval;
	CString	m_statMilliRad;
	BOOL	m_bStopOnBigShift;
	float	m_fBigShift;
	BOOL	m_bCloseValves;
	BOOL	m_bManualTrack;
	BOOL	m_bAverageDark;
  CButton m_butDoBidir;
  BOOL  m_bDoBidir;
  CButton m_butBidirWalkBack;
  BOOL  m_bBidirWalkBack;
  CButton m_butBidirUseView;
  BOOL  m_bBidirUseView;
  CStatic m_statBDMagLabel;
  CStatic m_statBidirMag;
  CString m_strBidirMag;
  CSpinButtonCtrl m_sbcBidirMag;
  CEdit m_editBidirAngle;
  float m_fBidirAngle;
  CStatic m_statBidirFieldSize;
  CString m_strBidirFieldSize;
  CSpinButtonCtrl m_sbcEarlyFrames;
  CButton m_butDoEarlyReturn;
  BOOL m_bDoEarlyReturn;
  CString m_strNumEarlyFrames;
  CStatic m_statNumEarlyFrames;
  CStatic m_statFrameLabel;
   
	//}}AFX_DATA


// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CTSSetupDialog)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

// Implementation
protected:

  // Generated message map functions
  //{{AFX_MSG(CTSSetupDialog)
  virtual BOOL OnInitDialog();
  afx_msg void OnAngleChanges();
  afx_msg void OnDeltaposSpinrecordmag(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnDeltaposSpinanchor(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnDeltaposSpinbin(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnDeltaposSpintrackmag(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnLowmagtrack();
  afx_msg void OnLeaveanchor();
  afx_msg void OnUseanchor();
  afx_msg void OnRepeatrecord();
  afx_msg void OnRepeatfocus();
  afx_msg void OnRintensitymean();
  virtual void OnOK();
  afx_msg void OnPostpone();
  afx_msg void OnSinglestep();
  afx_msg void OnAlwaysfocus();
  afx_msg void OnRampbeam();
  afx_msg void OnLowdosemode();
  afx_msg void OnAligntrackonly();
  afx_msg void OnRcamera();
	afx_msg void OnCheckautofocus();
	afx_msg void OnSkipautofocus();
	afx_msg void OnDeltaposSpincosinepower(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLimittocurrent();
	afx_msg void OnKillfocus();
	afx_msg void OnAutoRefineZlp();
	afx_msg void OnStoponbigshift();
	afx_msg void OnAveragerecref();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()

private:
  MagTable *mMagTab;
  CameraParameters *mCamParams;
  int mCurrentCamera;
  BOOL mSTEMindex;
  int mProbeMode;
  int mBinning;
  int mMagIndex[3];
  int mLowMagIndex[3];
  int mBidirMagInd[3];
  int mAnchorBuf;
  BOOL mChangeRecExposure;   // User's value of this setting
  CFont mLittleFont;
  CFont mTitleFont;
  CFont mBoldFont;
  int *mActiveCameraList;
  int mNumCameras;
  int mCosPower;
  int mPanelStart[NUM_TSS_PANELS];
  int mNumInPanel[NUM_TSS_PANELS];
public:
  CButton m_butPostpone;
  CButton m_butGo;
  CButton m_butSingleLoop;
  CButton m_butCenterBeam;
  BOOL m_bCenterBeam;
  int mPanelOpen[NUM_TSS_PANELS];
  void ManagePanels(void);
  afx_msg void OnOpenClose(UINT nID);
  afx_msg void OnNextPanel(UINT nID);
  afx_msg void OnPrevPanel(UINT nID);
  afx_msg void OnButTssLeft();
  afx_msg void OnButTssRight();
  void FixButtonStyle(UINT nID);
  CButton m_butPrev;
  CButton m_butNext;
  CButton m_butLeft;
  CButton m_butRight;
  CButton m_butFull;
  CStatic m_statVertline;
  afx_msg void OnButTssPrev();
  afx_msg void OnButTssNext();
  afx_msg void OnButTssFull();
  int PanelDisplayType(void);
  void InitializePanels(int type);
  CButton m_butManualTrack;
  CButton m_butSetChanges;
  BOOL m_bVaryParams;
  afx_msg void OnButSetChanges();
  afx_msg void OnCheckVaryParams();
  CButton m_butChangeAllExp;
  BOOL m_bChangeAllExp;
  afx_msg void OnChangeAllExp();
  CButton m_butChangeSettling;
  BOOL m_bChangeSettling;
  CButton m_butChangeExposure;
  BOOL m_bChangeExposure;
  afx_msg void OnChangeExposure();
  void ManageExposures(void);
  CButton m_butVaryParams;
  CEdit m_editFocusTarget;
  CButton m_butAutocenInterval;
  BOOL m_bAutocenInterval;
  CEdit m_editAutocenInterval;
  int m_iAutocenInterval;
  CStatic m_statACminutes;
  CStatic m_statACdegrees;
  CButton m_butAutocenCrossing;
  BOOL m_bAutocenCrossing;
  float m_fAutocenAngle;
  CEdit m_editAutocenAngle;
  void ManageAutocen(void);
  afx_msg void OnAutocenAtCrossing();
  CButton m_butSwapAngles;
  afx_msg void OnButSwapAngles();
  void ManageCamDependents(void);
  CEdit m_editFocusOffset;
  CEdit m_editBeamTilt;
  int MagIndWithClosestFieldSize(int oldCam, int oldMag, int newCam, int newSTEM);
  CButton m_butConfirmLowDoseRepeat;
  BOOL m_bConfirmLowDoseRepeat;
  CString m_strInterset;
  CStatic m_statInterset;
  void ManageIntersetStatus(void);
  afx_msg void OnDoBidir();
  afx_msg void OnBidirUseView();
  afx_msg void OnDeltaposSpinBidirMag(NMHDR *pNMHDR, LRESULT *pResult);
  void ManageBidirectional(void);
  void ConstrainBidirAngle(bool warningIfShift, bool anglesChanged);
  void ManageAnchorMag(int camera);
afx_msg void OnCheckLimitDeltaFocus();
afx_msg void OnDoEarlyReturn();
afx_msg void OnDeltaposSpinEarlyFrames(NMHDR *pNMHDR, LRESULT *pResult);
void ManageEarlyReturn(void);
CButton m_butUseDoseSym;
afx_msg void OnUseDoseSym();
BOOL m_bUseDoseSym;
CButton m_butSetupDoseSym;
afx_msg void OnButSetupDoseSym();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TSSETUPDIALOG_H__026F1402_5811_4754_BCC6_98F2135E362E__INCLUDED_)
