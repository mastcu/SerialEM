#pragma once
#include "NavHelper.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "XSliderCtrl.h"

typedef struct Mod_Object Iobj;
class CMapDrawItem;
#define MAX_AUTOCONT_GROUPS 8

// For passing all the data into and out of the thread proc
struct AutoContData {
  EMimageBuffer *imBuf;
  float targetSizeOrPix;
  float minSize;
  float maxSize;
  float interPeakThresh;
  float useThresh;
  float pixel;
  float singleXcen, singleYcen;
  float singleSizeFac;
  CMapDrawItem *polygon;
  bool needReduce;
  bool imIsBytes;
  bool needRefill;
  Iobj *obj;
  int *tdata;
  unsigned char *fdata;
  int *xlist;
  int *ylist;
  Islice *slRefilled;
  Islice *slReduced;
  unsigned char *idata;
  unsigned char **linePtrs;
  CString errString;
  FloatVec sqrMeans, sqrSDs, sqrSizes, squareness, pctBlackPix, boundDists;
  float medianMean;
  float spacing;
  HoleFinder *holeFinder;
};


// CAutoContouringDlg dialog

class CAutoContouringDlg : public CBaseDlg
{

public:
	CAutoContouringDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAutoContouringDlg();
  void CloseWindow();
  bool IsOpen() { return mIsOpen; };
  void SyncToMasterParams();
  MapItemArray *GetPolyArrayToDraw(ShortVec **groupNums, ShortVec **excluded,
    int &numGroups, int **showGroup);
  void SetExclusionsAndGroups(int groupByMean, float lowerMeanCutoff, float upperMeanCutoff, 
    float minSizeCutoff, float SDcutoff, float irregularCutoff, float borderDistCutoff);
  void SetExclusionsAndGroups();
  bool IsUndoFeasible();
  void ManageAll(bool forBusy);
  void UpdateSettings();

  void AutoContourImage(EMimageBuffer *imBuf, float targetSizeOrPix, float minSize, float maxSize,
    float interPeakThresh, float useThresh, BOOL usePolygon, float xCenter = -1., float yCenter = -1.);
  CMapDrawItem *EligibleBoundaryPolygon(float maxSize);
  static UINT AutoContProc(LPVOID pParam);
  static void FindSingleSubarea(int nbuf, int subSize, float singleCen, int &nsub, int &subStart, int &subEnd);
  static void FindPolySubarea(float polyMin, float polyMax, int nbuf, int subSize, int &nsub, int &subStart, int &subEnd);
  int AutoContBusy();
  void AutoContDone();
  void CleanupAutoCont(int error);
  void StopAutoCont();
  static void SquareStatistics(AutoContData *acd, int nxRed, int nyRed, float minScale, float maxScale, 
    float redFac, float outlieCrit);
  static int FindDistancesFromHull(FloatVec &xCenters, FloatVec & yCenters, FloatVec &xBound,
    FloatVec &yBound, float sizeScale, FloatVec &boundDists, bool useBound = false);
  GetMember(bool, AutoContFailed);
  bool DoingAutoContour() { return mDoingAutocont; };
  bool SinglePolygonMode() { return mIsOpen && m_bMakeOnePoly; };
  void MakeSinglePolygon(EMimageBuffer *imBuf, float xCenter, float yCenter);
  void ManageACEnables();
  GetSetMember(float, SingleSizeFac);


// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUTOCONTOUR };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);

	DECLARE_MESSAGE_MAP()

private:
  void DialogToParams();
  void ParamsToDialog();
  void ManagePostEnables(bool forBusy);
  void ManageGroupSelectors(int set);
  AutoContourParams mParams;
  AutoContourParams *mMasterParams;
  CNavHelper *mHelper;
  CNavigatorDlg *mNav;
  bool mHaveConts;
  bool mIsOpen;
  AutoContData mAutoContData;   // Structure for passing data to autocontouring
  CWinThread *mAutoContThread;
  bool mAutoContFailed;         // Flag cleared at successful completion
  bool mFindingFromDialog;
  bool mDoingAutocont;
  float mSingleSizeFac;
  float mStatMinSize;
  float mStatMaxSize;
  float mStatMinMean;
  float mStatMaxMean;
  float mMedianMean;
  float mStatMinSD;
  float mStatMaxSD;
  float mStatMaxBoundDist;
  float mStatMaxSquareness;
  float mStatMinSquareness;
  MapItemArray mPolyArray;
  ShortVec mGroupNums;
  ShortVec mExcluded;
  int mShowGroup[MAX_AUTOCONT_GROUPS];
  int mNumInGroup[MAX_AUTOCONT_GROUPS];
  CFont *mBoldFont;
  IntVec mFirstConvertedIndex;
  IntVec mLastConvertedIndex;
  IntVec mFirstMapID;
  IntVec mLastMapID;
  std::vector<IntVec> mConvertedInds;
  int mCurSingleMapID;
  int mSingleGroupID;
  bool mDidSingleMessage;

public:
  bool mOpenedFromMultiGrid;
  int m_iReduceToWhich;
  int m_iThreshType;
  afx_msg void OnRRelativeThresh();
  afx_msg void OnRToPixels();
  int m_iGroupType;
  afx_msg void OnRGroupBySize();
  int m_iReducePixels;
  afx_msg void OnKillfocusEditToPixels();
  afx_msg void OnKillfocusEditToPixSize();
  afx_msg void OnKillfocusEditRelThresh();
  afx_msg void OnKillfocusEditAbsThresh();
  CSpinButtonCtrl m_sbcNumGroups;
  afx_msg void OnDeltaposSpinNumGroups(NMHDR *pNMHDR, LRESULT *pResult);
  void ExternalSetGroups(int numGroups, int which, int *showGroups, int numShow);
  afx_msg void OnButMakeContours();
  afx_msg void OnButClearData();
  afx_msg void OnCheckShowGroup(UINT nID);
  afx_msg void OnButCreatePolys();
  int ExternalCreatePolys(float lowerMeanCutoff,
    float upperMeanCutoff, float minSizeCutoff, float SDcutoff, float irregularCutoff,
    float borderDistCutoff, CString &mess);
  int DoCreatePolys(CString &mess, bool doAll);
  afx_msg void OnButUndoPolys();
  int GetSquareStats(float &minMean, float &maxMean, float &medianMean);
  int *GetShowGroup() {return &mShowGroup[0] ; };
  CString m_strLowerMean;
  CString m_strUpperMean;
  CString m_strMinSize;
  CString m_strBorderDist;
  afx_msg void OnKillfocusEditMinMean();
  afx_msg void OnKillfocusEditMaxMean();
  afx_msg void OnKillfocusEditMinSize();
  afx_msg void OnKillfocusEditBorderDist();
  CString m_strIrregularity;
  CString m_strSquareSD;
  CString m_strMinLowerMean;
  CString m_strMinUpperMean;
  CString m_strMinMinSize;
  CString m_strMinIrregular;
  CString m_strMinSquareSD;
  CString m_strMinBorderDist;
  CString m_strMaxLowerMean;
  CString m_strMaxUpperMean;
  CString m_strMaxMinSize;
  CString m_strMaxIrregular;
  CString m_strMaxSquareSD;
  CString m_strMaxBorderDist;
  float m_fReduceToPixSize;
  float m_fRelThresh;
  float m_fAbsThresh;
  float m_fACminSize;
  float m_fACmaxSize;
  afx_msg void OnKillfocusEditACMinsize();
  afx_msg void OnKillfocusEditACMaxsize();
  CXSliderCtrl m_sliderUpperMean;
  CXSliderCtrl m_sliderMinSize;
  CXSliderCtrl m_sliderIrregular;
  CXSliderCtrl m_sliderSquareSD;
  CXSliderCtrl m_sliderLowerMean;
  CXSliderCtrl m_sliderBorderDist;
  int m_intUpperMean;
  int m_intMinSize;
  int m_intIrregular;
  int m_intSquareSD;
  int m_intLowerMean;
  int m_intBorderDist;
  int mNumGroups;
  CString m_strShowGroups;
  CStatic m_statShowGroups;
  BOOL m_bMakeOnePoly;
  afx_msg void OnCheckMakeOnePoly();
  BOOL m_bInsidePolygon;
  afx_msg void OnCheckInsidePolygon();
};
