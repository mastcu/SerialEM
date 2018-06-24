#if !defined(AFX_CAMERASETUPDLG_H__FBFAD3CE_12DE_4401_B29E_B9946A85408D__INCLUDED_)
#define AFX_CAMERASETUPDLG_H__FBFAD3CE_12DE_4401_B29E_B9946A85408D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CameraSetupDlg.h : header file
//
#include "BaseDlg.h"
#include "afxcmn.h"
#include "afxwin.h"

#define NUM_CAMSETUP_PANELS 9
/////////////////////////////////////////////////////////////////////////////
// CCameraSetupDlg dialog

struct CameraParameters;
class CCameraController;

class CCameraSetupDlg : public CBaseDlg
{
// Construction
public:
	void ManageDose();
	void ManageBinnedSize();
  void ManageCamera();
  void SetAdjustedSize(int deltaX, int deltaY);
  void ManageDrift(bool useMinRate = false);
  CString * mModeNames;
  void SetFractionalSize(int fracX, int fracY);
  void UnloadDialogToConset();
  void LoadConsetToDialog();
  int mCurrentSet;
  ControlSet *mConSets;
  ControlSet *mCurSet;
  CCameraController *mCamera;
  CCameraSetupDlg(CWnd* pParent = NULL);   // standard constructor
  CameraParameters *mCamParam;
  int mCameraSizeX, mCameraSizeY;
  float mBuiltInSettling;
  float mMinimumSettling;
  int mNumBinnings;
  int mBinnings[MAX_BINNINGS]; 
  BOOL mMontaging;
  BOOL mStartedTS;
  BOOL mDMsettlingOK;
  BOOL mDMbeamShutterOK;
  BOOL mSquareForBitLess;
  float mMinDualSettling;
  float mMinExposure;
  int mLastCamera;
  int *mActiveCameraList;
  int mNumCameras;
  int mCurrentCamera;
  int mTietzType;
  int mDE_Type;  //DE addition
  BOOL mGatanType;
  int mAMTtype;
  int mFEItype;
  BOOL mPluginType;
  BOOL mBinningEnabled;
  int mCoordScaling;
  int mCanProcess;
  BOOL mPlugCanPreExpose;
  int mNumPlugShutters;
  BOOL mTietzCanPreExpose;
  int mTietzBlocks;
  bool mChangedBinning;
  BOOL mAcquireAndReopen;
  BOOL mClosing;
  WINDOWPLACEMENT mPlacement;
  BOOL mUserSaveFrames;

    

// Dialog Data
  //{{AFX_DATA(CCameraSetupDlg)
	enum { IDD = IDD_CAMERASETUP };
	CButton	m_butRemoveXrays;
	CButton	m_butAverageDark;
	CEdit	m_editAverage;
	CEdit	m_editIntegration;
	CSpinButtonCtrl	m_spinAverage;
	CSpinButtonCtrl	m_spinIntegration;
	CStatic	m_timesText;
	CButton	m_butUpdateDose;
	CStatic	m_statElecDose;
	CButton	m_butRecenter;
	CButton	m_butSwapXY;
  CStatic m_statCopyCamera;
  CButton m_statCamera;
  CButton m_butCopyCamera;
  CButton m_butSmaller10;
  CButton m_butBigger10;
  CButton m_butABitLess;
  CButton m_butWideQuarter;
  CButton m_butWideHalf;
  CButton m_butQuarter;
  CButton m_butHalf;
  CButton m_butFull;
  CStatic m_statBox;
  CStatic m_statBigMode;
  CEdit m_driftEdit;
  CStatic m_driftText2;
  CStatic m_driftText1;
  int   m_iBinning;
  int   m_iContSingle;
  int   m_iProcessing;
  int   m_iControlSet;
  int   m_iShuttering;
  CString m_sBigText;
  BOOL  m_bDarkAlways;
  BOOL  m_bDarkNext;
  int   m_eBottom;
  int   m_eTop;
  int   m_eLeft;
  int   m_eRight;
  float m_eExposure;
  float m_eSettling;
  int   m_iCamera;
  CString m_strCopyCamera;
	CString	m_strElecDose;
	int		m_iAverageTimes;
	int		m_iIntegration;
	BOOL	m_bAverageDark;
	BOOL	m_bRemoveXrays;
  BOOL  m_bMatchPixel;
  BOOL  m_bMatchIntensity;
	//}}AFX_DATA


// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CCameraSetupDlg)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

// Implementation
protected:

  // Generated message map functions
  //{{AFX_MSG(CCameraSetupDlg)
  afx_msg void OnButfull();
  afx_msg void OnButhalf();
  afx_msg void OnButquarter();
  afx_msg void OnButwidehalf();
  afx_msg void OnButwidequarter();
  afx_msg void OnShuttering();
  afx_msg void OnConSet();
  virtual void OnOK();
  virtual void OnCancel();
  afx_msg void OnChangeCoord();
  virtual BOOL OnInitDialog();
  afx_msg void OnPaint();
  afx_msg void OnKillfocusLeft();
  afx_msg void OnKillfocusRight();
  afx_msg void OnKillfocusTop();
  afx_msg void OnKillfocusBottom();
  afx_msg void OnKillfocusEditexposure();
  afx_msg void OnKillfocusEditIntegration();
  afx_msg void OnKillfocusEditFrameTime();
  afx_msg void OnKillfocusEditsettling();
  afx_msg void OnButsmaller10();
  afx_msg void OnButbigger10();
  afx_msg void OnButabitless();
  afx_msg void OnButcopycamera();
  afx_msg void OnRcamera();
	afx_msg void OnBinning();
	afx_msg void OnContSingle();
	afx_msg void OnButrecenter();
	afx_msg void OnDeltaposSpinupdown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinleftright(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButupdatedose();
	afx_msg void OnDeltaposSpinaverage(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinIntegration(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillfocusEditaverage();
	afx_msg void OnProcessing();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
private:
  void DrawBox();
  int mPanelStart[NUM_CAMSETUP_PANELS];
  int mNumInPanel[NUM_CAMSETUP_PANELS];
  CameraParameters *mParam;   // The current param
  int mAreaWidth;
  int mAreaBaseHeight;
  int mAreaSTEMHeight;
  int mBinFullHeight;
  int mBinWidth;
  int mProcWidth;
  int mProcBaseHeight;
  int mProcSTEMHeight;
  bool mFalconCanSave;
  bool mWeCanAlignFalcon;
  int mDoseFracTableInd;
  int mDoseFracLeftOrig;
  int mDoseFracWidthOrig;
  int mDoseFracHeight;
  int mDoseFracLeftFalcon;
  ShortVec mSummedFrameList;
  FloatVec mUserFrameFrac;
  FloatVec mUserSubframeFrac;
  int mNumSkipBefore;
  int mNumSkipAfter;
  int mMaxIntegration;
  int mPosButtonIndex[6];
  int mUpperPosShift;
  int mLowerPosShift;
  bool mShiftedPosButtons;
  int mAreaTwoRowHeight;
  bool mDEweCanAlign;
  float mSaveLinearFPS;
  float mSaveCountingFPS;
  BOOL mLowDoseMode;

public:
  afx_msg void OnAcquireReopen();
  CSpinButtonCtrl m_sbcUpDown;
  CSpinButtonCtrl m_sbcLeftRight;
  CEdit m_editTop;
  CEdit m_editLeft;
  CEdit m_editBottom;
  CEdit m_editRight;
  CButton m_butDarkNext;
  CButton m_butDarkAlways;
  CButton m_butMatchPixel;
  CButton m_butMatchIntensity;
  afx_msg void OnSwapXY();
  bool AdjustCoords(int binning, bool alwaysUpdate = false);
  CEdit m_editExposure;
  CButton m_butLineSync;
  BOOL m_bLineSync;
  afx_msg void OnSelendokChan1();
  afx_msg void OnSelendokChan2();
  afx_msg void OnSelendokChan3();
  afx_msg void OnSelendokChan4();
  afx_msg void OnSelendokChan5();
  afx_msg void OnSelendokChan6();
  afx_msg void OnSelendokChan7();
  afx_msg void OnSelendokChan8();
  void NewChannelSel(int which);
  afx_msg void OnLinesync();
  void ManageTimingAvailable(void);
  CStatic m_statArea;
  CButton m_butDynFocus;
  BOOL m_bDynFocus;
  CString m_strScanRate;
  CButton m_butmaxScanRate;
  afx_msg void OnmaxScanRate();
  CStatic m_statBinning;
  CStatic m_statMinimumDrift;
  CString m_strSettling;
  CButton m_butKeepPixel;
  BOOL m_bKeepPixel;
  CStatic m_statProcessing;
  CStatic m_statIntegration;
  CStatic m_statExpTimesInt;
  CButton m_butUnprocessed;
  CButton m_butDarkSubtract;
  CButton m_butGainNormalize;
  CButton m_butBoostMag3;
  CButton m_butBoostMag4;
  CStatic m_statTimingAvail;
  CString m_strTimingAvail;
  int GetFEIflybackTime(float & flyback);
  afx_msg void OnDynfocus();
  afx_msg void OnK2Mode();
  afx_msg void OnDEMode();
  int m_iK2Mode;
  int m_iDEMode;
  BOOL m_bDoseFracMode;
  CStatic m_statFrameTime;
  CEdit m_editFrameTime;
  float m_fFrameTime;
  CStatic m_statFrameSec;
  CButton m_butAlignDoseFrac;
  BOOL m_bAlignDoseFrac;
  afx_msg void OnDoseFracMode();
  afx_msg void OnAlignDoseFrac();
  void ManageDoseFrac(void);
  void ComposeWhereAlign(CString &str);
  void ManageSizeAndPositionButtons(BOOL disableAll);
  BOOL m_bSaveFrames;
  CButton m_butSaveFrames;
  CButton m_butSetSaveFolder;
  CButton m_butDESetSaveFolder;
  CButton m_butDESaveFrames;
  CButton m_butDESaveMaster;
  CButton m_butDESaveFinal;
  CButton m_butDEalignFrames;
  CButton m_butDEsetupAlign;
  CButton m_butDEsuperRes;
  CStatic m_statDEframeTime;
  CStatic m_statDEframeSec;
  CStatic m_statDEwhereAlign;
  CStatic m_statDEsumNum;
  CEdit m_editDEframeTime;
  CSpinButtonCtrl m_spinDEsumNum;
  float m_fDEframeTime;
  float m_fDEfps;
  afx_msg void OnSaveFrames();
  afx_msg void OnSetSaveFolder();
  afx_msg void OnDESetSaveFolder();
  void ManageAntialias(void);
  CString m_strDoseRate;
  float ManageExposure(bool updateIfChange = true);
  CButton m_butSetupFalconFrames;
  afx_msg void OnSetupFalconFrames();
  void ManageDarkRefs(void);
  BOOL m_bDEsaveMaster;
  BOOL m_bDEsaveFrames;
  BOOL m_bDEsaveFinal;
  BOOL m_bDEalignFrames;
  CEdit m_editSumCount;
  int m_iSumCount;
  CStatic m_statNumDEraw;
  CStatic m_statNumDEsums;
  CString m_strNumDEraw;
  CString m_strNumDEsums;
  afx_msg void OnDeSaveFrames();
  afx_msg void OnDeAlignFrames();
  afx_msg void OnKillfocusEditDeSumCount();
  CButton m_butDoseFracMode;
  CButton m_butFileOptions;
  CButton m_butNameSuffix;
  afx_msg void OnButFileOptions();
  CStatic m_statSaveSummary;
  CStatic m_statAlignSummary;
  void ManageK2SaveSummary(void);
  CString m_strExpTimesInt;
  void ManageIntegration();
  BOOL m_bCorrectDrift;
  CButton m_butCorrectDrift;
  BOOL m_bUseHardwareROI;
  CButton m_butUseHardwareROI;
  BOOL m_bSaveK2Sums;
  CButton m_butSaveFrameSums;
  CButton m_butSetupK2FrameSums;
  afx_msg void OnSaveK2FrameSums();
  afx_msg void OnSetupK2FrameSums();
afx_msg void OnDeltaposSpinDeSumNum(NMHDR *pNMHDR, LRESULT *pResult);
afx_msg void OnKillfocusEditDeFPS();
afx_msg void OnKillfocusDeFrameTime();
  afx_msg void OnAlwaysAntialias();
  CButton m_butAlwaysAntialias;
  BOOL m_bAlwaysAntialias;
  CFont mBoldFont;
  CFont mLittleFont;
  CStatic m_statNormDSDF;
  CButton m_butSetupAlign;
  afx_msg void OnButSetupAlign();
  CStatic m_statWhereAlign;
  CStatic m_statIntermediateOnOff;
void ManageDEpanel(void);
afx_msg void OnDeSaveMaster();
  void CheckFalconFrameSumList(void);
float RoundedDEframeTime(float frameTime);
int GetMagIndexForCamAndSet(void);
float ActualFrameTime(float roundedTime);
void ManageK2Binning(void);
  BOOL m_bTakeK3Binned;
  afx_msg void OnTakeK3Binned();
  CButton m_butTakeK3Binned;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CAMERASETUPDLG_H__FBFAD3CE_12DE_4401_B29E_B9946A85408D__INCLUDED_)
