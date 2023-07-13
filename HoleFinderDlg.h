#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "NavHelper.h"
#include "MyButton.h"

#define MAX_HOLE_TRIALS 16
class CNavHelper;
class CNavigatorDlg;

// CHoleFinderDlg dialog

class CHoleFinderDlg : public CBaseDlg
{
public:
	CHoleFinderDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CHoleFinderDlg();
  void ManageEnables();
  void ManageSizeSeparation(bool update);
  void SizeAndSpacingToDialog(bool saveOther, bool update);
  void SizeAndSpacingToParam(bool other);
  void UpdateSettings();
  bool GetHolePositions(FloatVec **x, FloatVec **y, IntVec **pcOn, std::vector<short> **exclude,
    BOOL &incl, BOOL &excl);
  bool HaveHolesToDrawOrMakePts();
  bool MouseSelectPoint(EMimageBuffer *imBuf, float inX, float inY, float imDistLim, bool dragging);
  void SetExclusionsAndDraw();
  void SetExclusionsAndDraw(float lowerMeanCutoff, float upperMeanCutoff, float sdCutoff, float blackCutoff,
    float edgeDistCutoff);
  int ConvertHoleToISVectors(int index, bool setVecs, double *xVecs, double *yVecs, CString &errStr);
  GetMember(bool, FindingHoles);
  void GetGridImVecs(float *xVecs, float *yVecs) { xVecs = &mGridImXVecs[0]; yVecs = &mGridImYVecs[0]; };
  void GetGridStageVecs(float *xVecs, float *yVecs) { xVecs = &mGridStageXVecs[0]; yVecs = &mGridStageYVecs[0]; };
  GetMember(float, LastTiltAngle);
  GetMember(int, LastMagIndex);
  GetMember(BOOL, LastWasHexGrid);
  void ScanningNextTask(int param);
  void ScanningCleanup(int error);
  void StopScanning(void);
  void CloseWindow();
  bool IsOpen() { return mIsOpen; };
  bool CheckAndSetNav(const char *message = NULL);
  int DoFindHoles(EMimageBuffer *imBuf);
  int DoMakeNavPoints(int layoutType, float lowerMeanCutoff, float upperMeanCutoff,
    float sdCutoff, float blackCutoff, float edgeDistCutoff);


// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HOLE_FINDER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void OnOK();
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);

	DECLARE_MESSAGE_MAP()

private:
  HoleFinderParams mParams;
  HoleFinderParams *mMasterParams;
  CNavHelper *mHelper;
  CNavigatorDlg *mNav;
  bool mHaveHoles;
  bool mIsOpen;
  float mMeanMin, mMeanMax;
  float mMidMean;
  float mSDmin, mSDmax;
  float mBlackFracMin, mBlackFracMax;
  float mEdgeDistMax;
  double mZstage;
  FloatVec mHoleMeans;
  FloatVec mHoleSDs;
  FloatVec mHoleBlackFracs;
  FloatVec mHoleEdgeDists;
  FloatVec mXcenters, mYcenters;
  FloatVec mXstages, mYstages;
  FloatVec mXinPiece, mYinPiece;
  FloatVec mXmissStage, mYmissStage;
  FloatVec mXmissCen, mYmissCen;
  IntVec mMissPieceOn;
  FloatVec mMissXinPiece, mMissYinPiece;
  IntVec mPieceOn;
  std::vector<short> mExcluded;
  int mLastUserSelectInd;
  bool mLastSelectWasDrag;
  double mSigmas[MAX_HOLE_TRIALS];
  double mThresholds[MAX_HOLE_TRIALS];
  int mIterations[MAX_HOLE_TRIALS];
  int mNumIterations;
  int mNumSigmas;
  int mNumThresholds;
  FloatVec mUseSigmas;
  FloatVec mUseThresholds;
  bool mFindingHoles;
  FloatVec mXboundary, mYboundary;
  float mPixelSize, mReduction;
  int mMontage;
  int mMapID;
  int mRegistration;
  int mBoundPolyID;
  int mAddedGroupID;
  int mIndexOfGroupItem;
  float mBestRadius, mTrueSpacing, mIntensityRad, mErrorMax;
  KImageStore *mImageStore;
  int mCurStore, mBufInd;
  ScaleMat mImToStage;
  ScaleMat mAdjustedStageToCam;
  float mGridImXVecs[3], mGridImYVecs[3];
  float mGridStageXVecs[3], mGridStageYVecs[3];
  BOOL mLastWasHexGrid;
  int mSigInd, mThreshInd, mBestSigInd, mBestThreshInd;
  MontParam mMontParam;
  CMapDrawItem *mNavItem;
  int mFullYsize, mFullBinning;
  int mBufBinning;
  MiniOffsets *mMiniOffsets;
  float mLastHoleSize;
  float mLastHoleSpacing;
  float mLastTiltAngle;
  int mLastMagIndex;
  bool mFindingFromDialog;
  bool mSkipAutoCor;
  CFont *mBoldFont;


public:
  float m_fHolesSize;
  afx_msg void OnKillfocusEditHoleSize();
  afx_msg void OnKillfocusEditSpacing();
  float m_fSpacing;
  CString m_strSigmas;
  afx_msg void OnKillfocusEditSigmas();
  CString m_strIterations;
  afx_msg void OnKillfocusEditMedianIters();
  CString m_strThresholds;
  afx_msg void OnKillfocusEditThresholds();
  CButton m_butExcludeOutside;
  BOOL m_bExcludeOutside;
  afx_msg void OnExcludeOutside();
  CButton m_butFindHoles;
  afx_msg void OnButFindHoles();
  CStatic m_statStatusOutput;
  CString m_strStatusOutput;
  CString m_strMinLowerMean;
  CString m_strMaxLowerMean;
  CSliderCtrl m_sliderLowerMean;
  CEdit m_editLowerMean;
  afx_msg void OnKillfocusEditLowerMean();
  CString m_strLowerMean;
  int m_intLowerMean;
  CString m_strMinUpperMean;
  CString m_strMaxUpperMean;
  CSliderCtrl m_sliderUpperMean;
  CEdit m_editUpperMean;
  afx_msg void OnKillfocusEditUpperMean();
  CString m_strUpperMean;
  int m_intUpperMean;
  CString m_strMinSDcutoff;
  CString m_strMaxSDcutoff;
  CSliderCtrl m_sliderSDcutoff;
  int m_intSDcutoff;
  CString m_strSDcutoff;
  CString m_strMinBlackPct;
  CString m_strMaxBlackPct;
  CSliderCtrl m_sliderBlackPct;
  int m_intBlackPct;
  CString m_strBlackPct;
  CButton m_butShowIncluded;
  BOOL m_bShowIncluded;
  afx_msg void OnShowIncluded();
  CButton m_butShowExcluded;
  BOOL m_bShowExcluded;
  afx_msg void OnShowExcluded();
  CButton m_butZigzag;
  CButton m_butFromFocus;
  CButton m_butInGroups;
  int m_iLayoutType;
  afx_msg void OnRadioLayoutType();
  CButton m_butMakeNavPts;
  afx_msg void OnButMakeNavPts();
  CString m_strUmSizeSep;

private:
  void DialogToParams();
  void ParamsToDialog();
  CString ListToString(int type, int num, void *list);
  void InvertVectorInY(FloatVec &vec, int ysize);
public:
  void SyncToMasterParams();
  float m_fMaxError;
  afx_msg void OnKillfocusEditMaxError();
  CButton m_butClearData;
  afx_msg void OnButClearData();
  afx_msg void OnBracketLast();
  CButton m_butBracketLast;
  BOOL m_bBracketLast;
  CButton m_butSetSizeSpace;
  afx_msg void OnButSetSizeSpace();
  CMyButton m_butToggleHoles;
  afx_msg void OnToggleDraw(NMHDR * pNotifyStruct, LRESULT * result);
  CButton m_butHexArray;
  BOOL m_bHexArray;
  afx_msg void OnHexArray();
  CSliderCtrl m_sliderHullDist;
  int m_intHullDist;
  CEdit m_editHullDist;
  CString m_strHullDist;
  CString m_strMaxHullDist;
  afx_msg void OnKillfocusEditHullDist();
};
