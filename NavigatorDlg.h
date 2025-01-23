#if !defined(AFX_NAVIGATORDLG_H__F2EDDA5D_BA58_45CC_8458_5CF199EB5007__INCLUDED_)
#define AFX_NAVIGATORDLG_H__F2EDDA5D_BA58_45CC_8458_5CF199EB5007__INCLUDED_

#include "MapDrawItem.h"	// Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// NavigatorDlg.h : header file
//

#include "BaseDlg.h"
#include "afxwin.h"
#include <afxtempl.h>
#include <vector>
#include <set>
#include "NavHelper.h"

class CMontageSetupDlg;
struct TiltSeriesParam;
class CNavAcquireDlg;
class CMacCmd;
struct NavAcqAction;
typedef struct Mod_Object Iobj;

#define MAX_CURRENT_REG 99
#define MAX_SAVED_REGXFORM 10
#define MIN_INTERNAL_ID 100000
#define NUM_ITEM_COLORS  6
#define MAX_LABEL_SIZE  16
#define MAX_NAV_USER_VALUES 8

enum NavAcquireTypes {ACQUIRE_TAKE_MAP = 0, ACQUIRE_IMAGE_ONLY, ACQUIRE_RUN_MACRO,
  ACQUIRE_DO_TS, ACQUIRE_MULTISHOT};
enum NavRegTypes {NAVREG_UNUSED, NAVREG_REGULAR, NAVREG_IMPORT};
enum NavNewFileTypes {NAVFILE_ITEM = 0, NAVFILE_GROUP, NAVFILE_TS};
enum NavArrayTypes {NAVARRAY_FILEOPT, NAVARRAY_MONTPARAM, NAVARRAY_TSPARAM, 
  NAVARRAY_STATE};
enum NavSourceTypes {
  NAVACQ_SRC_MACRO = 0, NAVACQ_SRC_MENU, NAVACQ_SRC_MG_SET_MMM,
  NAVACQ_SRC_MG_SET_ACQ, NAVACQ_SRC_MG_RUN_MMM, NAVACQ_SRC_MG_RUN_ACQ
};

// DO NOT REARRANGE/RENUMBER
enum NavExtTypes {NAVEXT_ON_MAP = 0, NAVEXT_ON_ALIMONT, NAVEXT_ON_VSMONT, 
  NAVEXT_ON_PIECE};
enum NavExtErrors {NEXERR_MULTIPLE_ENTRY = 0, NEXERR_NO_DRAWN_ON, NEXERR_NO_MAP_WITH_ID,
  NEXERR_MAP_NOT_MONT, NEXERR_ACCESS_FILE, EXTERR_NO_PIECE_ON, EXTERR_NO_ALI_COORDS, 
  EXTERR_NO_PC_COORD, EXTERR_NO_MDOC, EXTERR_BAD_MDOC_IND};

enum MontSetupSource { SETUPMONT_FROM_MACRO = 1, SETUPMONT_MG_FULL_GRID, 
  SETUPMONT_MG_LM_NBYN, SETUPMONT_MG_POLYGON, SETUPMONT_MG_MMM_NBYN };

struct ScheduledFile {
  CString filename;
  int groupID;
  int filePropIndex;
  int montParamIndex;
  int stateIndex;
} ;

class CMyDragListBox : public CDragListBox
{
public:
  CMyDragListBox();

  virtual void Dropped( int nSrcIndex, CPoint pt );
};


/////////////////////////////////////////////////////////////////////////////
// CNavigatorDlg dialog

class CNavigatorDlg : public CBaseDlg
{
// Construction
public:
  CNavAcquireDlg *mNavAcquireDlg;
  BOOL FittingMontage() {return mMontItem != NULL;};
  int FitMontageToItem(MontParam * montParam, int binning, int magIndex, 
    BOOL forceStage, float overlapFac, int iCam, BOOL lowDose);
	void SetupSuperMontage(BOOL skewed);
	int FullMontage(bool skipDlg, float overlapFac, int source = 0);
  void AutoSave();
  int SaveAndClearTable(bool askIfSave = false);
	void MoveListSelection(int direction);
	void SetupSkipList(MontParam * montParam);
	int FindAndSetupNextAcquireArea();
  int GotoCurrentAcquireArea(bool imposeBacklash);
	void StopAcquiring(BOOL testMacro = false);
  void SetItemSuccessfullyAcquired(CMapDrawItem *item);
  void RestoreBeamTiltIfSaved();
	void AcquireCleanup(int error);
	int TaskAcquireBusy();
	void AcquireNextTask(int param);
  void AcquireAreas(int source, bool dlgClosing, bool useTempParams);
  void ManageAcquireDlgCleanup(bool fromMenu, bool dlgClosing);
  void AcquireDlgClosing();
  BOOL AcquireOK(bool tiltSeries = false, int startInd = 0, int endInd = -1);
  int NewMap(bool unsuitableOK = false, int addOrReplaceNote = 0, CString *newNote = NULL);
  void DeleteItem() {OnDeleteitem();};
	void CornerMontage();
	int PolygonMontage(CMontageSetupDlg *montDlg, bool skipSetupDlg, int itemInd = -1, 
    float overlapFac = 0., int source = 0);
	void TransformPts();
  void DoClose() {OnCancel();};
  BOOL GetAcquiring() {return mAcquireIndex >= 0;};
  int GetNumberOfItems() {return (int)mItemArray.GetSize();};
	BOOL SetCurrentItem(bool groupOK = false);
	BOOL CornerMontageOK();
	BOOL TransformOK();
  BOOL NoDrawing() {return !(mAddingPoints || mAddingPoly || mMovingItem);};
	int GetItemType();
  int LoadNavFile(bool checkAutosave, bool mergeFile, CString *inFilename = NULL);
  void FinishMontParamLoad(MontParam *montParam, int ind1, int &numAdocErr);
	int SetupMontage(CMapDrawItem *item, CMontageSetupDlg *montDlg, bool skipSetupDlg, 
    float overlapFac = 0., int source = 0);
	void XfApply(ScaleMat a, float *dxy, float inX, float inY, float &outX, float &outY);
	int DoSaveAs();
	int DoSave(bool autoSave);
	int AskIfSave(CString reason);
	CString NextTabField(CString inStr, int &index);
	int GetNavFilename(BOOL openFile, DWORD flags, bool mergeFile);
	void OpenAndWriteFile(bool autosave);
	BOOL BackspacePressed();
	BOOL BufferStageToImage(EMimageBuffer *imBuf, ScaleMat &aMat, float &delX, 
    float &delY);
	void ShiftItemPoints(CMapDrawItem *item, float delX, float delY);
	void Redraw();
	void Update();
  CString FormatProgress(int numDone, int numTot, int numLeft, const char *text);
	int MoveStage(int axisBits, bool justCheck = false);
	void SetCurrentStagePos(int index);
	CMapDrawItem *MakeNewItem(int groupID);
  void FinishSingleDeletion(CMapDrawItem *item, int delIndex, int listInd, 
    bool multipleInGroup, int groupStart);
  void ExternalDeleteItem(CMapDrawItem *item, int delIndex);
  void SetGroupAcquireFlags(int groupID, BOOL acquire);
  void SetPolygonCenterToMidpoint(CMapDrawItem *item);
  void UpdateForCurrentItemChange();
	void OnListItemDrag(int oldIndex, int newIndex);
	void SetRegPtNum(int inVal);
	int GetFreeRegPtNum(int registration, int proposed);
	void SetCurrentRegFromMax();
	CString FormatCoordinate(float inVal, int maxLen);
	void ManageListScroll();
	void ManageCurrentControls();
	void FillListBox(bool skipManage = false, bool keepSel = false);
	void UpdateListString(int index);
	void ItemToListString(int index, CString &string);
	BOOL UserMousePoint(EMimageBuffer *imBuf, float inX, float inY, BOOL nearCenter, int button);
  bool ConvertMousePoint(EMimageBuffer *imBuf, float &inX, float &inY, float &stageX,
    float &stageY, ScaleMat &aInv, float &delX, float &delY, float &xInPiece, float &yInPiece,
    int &pieceIndex);
  bool SelectNearestPoint(EMimageBuffer *imBuf, float stageX, float stageY, ScaleMat aInv,
    float delX, float delY, bool ctrlKey, float distLim);
  void GetSelectionLimits(EMimageBuffer *imBuf, ScaleMat aInv, float delX, float delY, float selXlimit[4], float selYlimit[4],
    float selXwindow[4], float selYwindow[4]);
  bool MouseDragSelectPoint(EMimageBuffer *imBuf, float inX, float inY, float imDistLim);
  bool OKtoMouseSelect() {return !(mAddingPoints || mAddingPoly || mMovingItem || mNavAcquireDlg); };
	MapItemArray *GetMapDrawItems(EMimageBuffer *imBuf, 
    ScaleMat &aMat, float &delX, float &delY, BOOL &drawAllReg, CMapDrawItem **acquireBox);
  void AddHolePositionsToItemPts(FloatVec &delISX, FloatVec &delISY, IntVec &holeIndex, 
    bool custom, int numHoles, CMapDrawItem *item);
	CNavigatorDlg(CWnd* pParent = NULL);   // standard constructor
  int GetCurrentRegistration() {return mCurrentRegistration;};
  MapItemArray *GetItemArray() {return &mItemArray;};
  CArray<ScheduledFile *, ScheduledFile *> *GetScheduledFiles() {return &mGroupFiles;};
  CArray<FileOptions *, FileOptions *> *GetFileOptArray() {return &mFileOptArray;};
  CArray<TiltSeriesParam *, TiltSeriesParam *> *GetTSparamArray() {return &mTSparamArray;};
  CArray<MontParam *, MontParam *> *GetMontParArray() {return &mMontParArray;};
  CArray<StateParams *, StateParams *> *GetAcqStateArray() {return &mAcqStateArray;};
	void OnOK();
  GetMember(int, MarkerShiftReg);
  void GetLastMarkerShift(float &outX, float &outY) { outX = mMarkerShiftX; outY = mMarkerShiftY; };
  GetMember(int, AcquireEnded)
  GetMember(BOOL, LoadingMap)
  GetMember(int, NumSavedRegXforms)
  SetMember(int, SuperCoordIndex)
  GetMember(BOOL, StartedTS)
  GetMember(BOOL, FirstInGroup)
  SetMember(int, SkipBacklashType);
  GetSetMember(int, CurListSel)
  GetMember(int, NumAcquired);
  GetMember(bool, RealignedInAcquire);
  void SetExtraFileSuffixes(CString *items, int numItems);
  GetMember(int, FoundItem);
  SetMember(bool, SkipAcquiringItem);
  SetMember(int, GroupIDtoSkip);
  SetMember(BOOL, SkipStageMoveInAcquire);
  GetSetMember(bool, PausedAcquire);
  GetMember(int, AcquireIndex);
  GetMember(int, MagIndForHoles);
  GetMember(int, CameraForHoles);
  GetMember(int, EndingAcquireIndex);
  GetMember(int, FocusCycleCounter);
  GetMember(bool, DidEucentricity);
  GetMember(bool, ShowingLDareas);
  SetMember(int, RunScriptAfterNextAcq);
  GetMember(int, ScriptToRunAtEnd);
  GetSetMember(int, NumDoneAcq);
  GetMember(bool, UseTempAcqParams);
  GetMember(bool, IgnoreUpdates);
  SetMember(bool, ReloadTableOnNextAdd);
  void SetCurAcqParmActions(int which) { mAcqParm = mWinApp->GetNavAcqParams(which); mAcqActions = mHelper->GetAcqActions(which); };
  bool OKtoCloseNav();

  CString GetCurrentNavFile() {return mNavFilename;};
  int GetNumNavItems() {return (int)mItemArray.GetSize();};
  int ResumeFromPause();
  BOOL InEditMode() {return m_bEditMode;};
  BOOL TakingMousePoints() {return m_bEditMode || mAddingPoints || mAddingPoly || mMovingItem;};
  std::set<int> *GetSelectedItems() {return &mSelectedItems;};
  void SetNextFileIndices(int fileIn, int montIn) {mNextFileOptInd = fileIn; mNextMontParInd = montIn;};
  int GetCurrentIndex() {return mCurrentItem;};
  bool DoingNewFileRange() { return mFRangeIndex >= 0; };

// Dialog Data
	//{{AFX_DATA(CNavigatorDlg)
	enum { IDD = IDD_NAVIGATOR };
	CButton	m_butGotoMarker;
	CButton	m_butAcquire;
	CButton	m_butNewMap;
	CButton	m_butAddStagePos;
	CButton	m_butDeleteItem;
	CButton	m_butGotoXy;
	CButton	m_butGotoPoint;
	CStatic	m_statRegPtNum;
	CSpinButtonCtrl	m_sbcCurrentReg;
	CSpinButtonCtrl	m_sbcRegPtNum;
	CButton	m_butMoveItem;
	CButton	m_butLoadMap;
	CButton	m_butDrawPoly;
	CButton	m_butDrawPts;
	CButton	m_butCorner;
	CEdit	m_editPtNote;
	CButton	m_butRegPoint;
	CMyDragListBox	m_listViewer;
  CButton m_butRotate;
  BOOL m_bRotate;
	BOOL	m_bRegPoint;
	CString	m_strLabel;
	CString	m_strPtNote;
	BOOL	m_bCorner;
	CString	m_strCurrentReg;
	CString	m_strRegPtNum;
	BOOL	m_bDrawOne;
	int		m_iColor;
	BOOL	m_bAcquire;
	BOOL	m_bDrawNone;
	BOOL	m_bDrawAllReg;
  BOOL m_bCollapseGroups;     // Flag for collapsed group display
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNavigatorDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNavigatorDlg)
	afx_msg void OnAddStagePos();
	afx_msg void OnCheckRegpoint();
	afx_msg void OnCheckcorner();
	afx_msg void OnDeleteitem();
	afx_msg void OnDrawPoints();
	afx_msg void OnDrawPolygon();
	afx_msg void OnGotoPoint();
	afx_msg void OnSelchangeListviewer();
	afx_msg void OnLoadMap();
	afx_msg void OnMoveItem();
	afx_msg void OnNewMap();
	afx_msg void OnDeltaposSpinRegptNum(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpincurrentReg(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnChangeEditPtlabel();
	afx_msg void OnChangeEditPtnote();
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnCancel();
	afx_msg void OnDrawOne();
	afx_msg void OnGotoXy();
	afx_msg void OnMove(int x, int y);
  afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
  afx_msg void OnSelendokCombocolor();
	afx_msg void OnCheckAcquire();
  afx_msg void OnDblclkListviewer();
  afx_msg void OnSetFocusListviewer();
	afx_msg void OnDrawNone();
  afx_msg void OnCheckrotate();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
  CSerialEMDoc *mDocWnd;
  CEMscope *mScope;
  MontParam *mMontParam;
  MagTable   *mMagTab;
  EMimageBuffer *mImBufs;
  CShiftManager *mShiftManager;
  CameraParameters *mCamParams;
  EMbufferManager *mBufferManager;
  CNavHelper *mHelper;
  NavParams *mParam;
  NavAcqParams *mAcqParm;
  CCameraController *mCamera;
  CLowDoseDlg *mLowDoseDlg;
  CMacCmd *mMacroProcessor;

  int mListBorderX, mListBorderY;
  int mNoteBorderX;  int mNoteHeight;
  int mLineBorderX;
  int mHeaderBorderX, mHeaderHeight;
  int mFilenameBorderX;
  CString mHeaderStart;
  BOOL mInitialized;
  CMapDrawItem *mItem;
  CMapDrawItem *mLoadItem;
  MapItemArray mItemArray;
  CArray<ScheduledFile *, ScheduledFile *> mGroupFiles;
  CArray<FileOptions *, FileOptions *> mFileOptArray;
  CArray<TiltSeriesParam *, TiltSeriesParam *> mTSparamArray;
  CArray<MontParam *, MontParam *> mMontParArray;
  CArray<StateParams *, StateParams *> mAcqStateArray;
  NavAcqAction *mAcqActions;
  bool mUseTempAcqParams;
  bool mUseTSprePostMacros;
  std::vector<int> mListToItem;
  std::vector<int> mItemToList;
  std::set<int> mSetOfIDs;
  int mCurrentRegistration;
  int mCurrentItem;         // Current item
  int mCurListSel;          // Current list selection in collapsed mode
  std::set<int> mSelectedItems;  // Set of selected items indexes
  int mNumDigitsForIndex;   // Number of digits when showing indexes
  int mRegPointNum;
  int mNewItemNum;
  BOOL mAddingPoints;
  int mAddingPoly;
  int mNumberBeforeAdd;
  int mAddPointID;          // Group ID for points being added
  BOOL mMovingItem;
  CString mNavFilename;
  CString mMergeName;
  BOOL mChanged;
  int mAcquireIndex;
  int mStartingAcquireIndex;
  int mEndingAcquireIndex;
  bool mResetAcquireIndex;   // Flag that it needs to be set to start for new registration
  BOOL mSaveAlignOnSave;
  BOOL mSaveProtectRecord;
  BOOL mNavBackedUp;        // Flag for managing whether to create a .bak
  BOOL mPreCombineBackedUp; // Flag for precombined holes backed up
  int mSuperNumX, mSuperNumY;  // Number of super frames in X and Y
  int mSuperOverlap;           // Pixels of overlap
  short mAcqSteps[ACQ_MAX_STEPS * 2];        // List of actions to do
  int mNumAcqSteps;         // Number of actions
  int mAcqStepIndex;        // Current action index
  int mNumAcquired;          // Number actually acquired
  int mAcquireCounter;       // Cumulative number used for items at intervals
  float mSavedTargetDefocus; // Stored value of target 
  int mFocusCycleCounter;    // Counter for keeping track of focus steps
  bool mDoingEarlyReturn;    // Flag that early return is to be set
  bool mSkippingSave;        // Flag that save is to be skipped
  BOOL mStartedTS;           // Flag that started a tilt series
  bool mPausedAcquire;       // Flag that acquire is paused
  bool mResumedFromPause;    // Flag set when resume happens
  int mMontCurrentAtPause;   // -1 no current file, 0 for single, 1 for montage at pause
  bool mRealignedInAcquire;  // Flag if realign to item done during acquire
  double mSavedBeamTiltX;    // Values to restore when compensating for beam tilt
  double mSavedBeamTiltY;
  double mSavedAstigX;       // And astigmatism
  double mSavedAstigY;

  int mFrameLimitX, mFrameLimitY;         // Frame limits for montage setup
  CMapDrawItem *mMontItem;  // Item used
  CMapDrawItem *mMontItemCam;  // Item used with camera coordinates
  float mCamCenX, mCamCenY; // Center and extra for making skip list
  float mExtraX, mExtraY;
  ScaleMat mPolyToCamMat;   // Matrix used to get from polygon to camera coordinates
  int mMarkerShiftReg;      // Registration at which shift to marker occurred, for undo
  float mMarkerShiftX, mMarkerShiftY;  // Shift that was applied
  int mMarkerShiftType;     // Type of shift, > 0 if not all at reg
  int mMarkerShiftMagInd;   // Mag index
  int mMarkerShiftCohortID; // ID
  int mAcquireEnded;        // Flag that End Nav or Pause was pressed when running macro
  KImageStore *mLoadStoreMRC; // Variables to save state for asynchronous loading of map
  BOOL mReadingOther;
  int mCurStore;
  int mOriginalStore;
  float mUseWidth, mUseHeight;
  int mOverviewBinSave;
  BOOL mLoadingMap;         // Flag that map is being loaded
  int mBufToLoadInto;      // Buffer being loaded to
  BOOL mShowAfterLoad;     // Flag to display after loading map
  int mShiftAIndex;        // Starting index for doing shift A
  int mShiftDIndex;        // Starting index for doing shift D
  int mShiftTIndex;        // Starting index for doing shift T
  int mShiftNIndex;        // Starting index for doing shift N
  int mShiftVIndex;        // Starting index for doing shift V
  int mShiftHIndex;        // Starting index for doing shift H
  int mRangeGroupEnd;      // Ending index of range if group
  bool mInRangeDelete;     // Flag that Delete is being called from range deletion
  int mFRangeNumOff;       // Variables needed to run new files in range as task
  bool mFRangeAllOn;
  int mFRangeInterval;
  int mFRangeStart;
  int mFRangeEnd;
  int mFRangeIndex;
  IntVec mFRangeMontInds;
  bool mFRangeAnyPoly;
  int mFRangeCurrItemSave;
  int mFRangeCurrSelSave;
  int mNumSavedRegXforms;  // Number of saved reg-to-reg transforms
  int mLastXformToReg;     // Registration it was transformed to
  ScaleMat mMatSaved[MAX_SAVED_REGXFORM];
  float mDxySaved[MAX_SAVED_REGXFORM][2];
  int mFromRegSaved[MAX_SAVED_REGXFORM];
  int mSuperCoordIndex;     // Item index to return supermont coordinates for
  double mShiftByAlignStamp;  // Image time stamp and align shift for last
  float mShiftByAlignX;     // shift by align operation
  float mShiftByAlignY;
  int mFoundItem;           // Index of last item found by ID
  int mDualMapID;           // ID of map selected for dual mapping
  BOOL mEmailWasSent;       // Flag that an email was sent, to avoid duplicates
  BOOL mSaveCollapsed;      // Save state of collapsed flag during acquires
  int mLastGroupID;         // Group ID if last item acquired
  BOOL mFirstInGroup;       // Flag for first item in (collapsed) group during acquire
  int mLastTransformPairs;  // Number of pairs for last transformation computed
  BOOL mRemoveItemOnly;     // Flag for delete to remove only one item
  double mLastScopeStageX;  // Last stage pos gotten from scope
  double mLastScopeStageY;
  float mRequestedStageX;   // Stage position requested in last move
  float mRequestedStageY;
  int mLastMoveStageID;     // ID assigned to item stage was last moved to
  int mBacklashItemInd;     // Index of item backlash adjustment being run for
  int mSkipBacklashType;    // 1 to skip if it means ask, 2 to skip unconditionally
  BOOL mRawStageIsMovable;
  float mMovedRawStageX;
  float mMovedRawStageY;
  int mNextFileOptInd;      // File and mont param index for file to be opened on the
  int mNextMontParInd;      // item being acquired, set in GoToNextAcquirearea
  CString mAcquireOpenedFile;  // Name of file opened by Acquire
  bool mUsedPreExistingMont;  // Flag that an already open montage was acquired to
  CString mExtraFileToClose[MAX_STORES - 1];   // Name of extra files opened by Acquire
  int mNumExtraFilesToClose;   // Number of files to close
  CString mExtraFileSuffix[MAX_STORES - 1]; // Suffix for extra files to open when Acquire opens a file
  int mNumExtraFileSuffixes; // Number of suffixes stored
  int mDrawnOnMontBufInd;   // Buffer index of map grid points were drawn on
  int mPieceGridPointOn;    // Montage piece for last point converted in GridImageToStage
  float mGridPtXinPiece;      // Coordinates in piece
  float mGridPtYinPiece;
  float mLast5ptIncX1;      // Parameters of the last 5-point geometry for adding grid
  float mLast5ptIncX2;
  float mLast5ptIncY1;
  float mLast5ptIncY2;
  int mLast5ptEnd1;
  int mLast5ptEnd2;
  int mLast5ptRegis;        // Registration at which it was done
  bool mLast5ptOnImage;     // Whether it was done with image coordinates
  float mLast5ptOffsetX;    // Offset from corner of pattern to center
  float mLast5ptOffsetY;
  float mCenX, mCenY;       // Variables for getting from index to grid image position
  float mIncX1, mIncX2, mIncY1, mIncY2;
  bool mSwapAxesAddingPoints;  // Make first axis the inner loop for away from focus
  bool mLastSelectWasCurrent;  // Flag that last mouse up was on current point
  double mLastAcquireTicks;  // Time of last acquisition, for reporting interval
  double mLastAcqDoneTime;  // Time when last acquire item finished the full cycle
  double mElapsedAcqTime;   // Cumulative elapsed time since start of acquire
  int mNumDoneAcq;          // Number of acquire points done (including failures)
  int mNumTotalShotsAcq;    // Total number of shots in multishot
  int mInitialNumAcquire;   // Initial number to be acquired
  int mInitialTotalShots;   // Initial total shots
  bool mSkipAcquiringItem;  // Flag pre-macro can set to skip the item
  double mLastMontLeft;     // Last remaining montage time
  double mLastTimePerItem;  // Last time per item for that montage time
  int mGroupIDtoSkip;       // ID for skipping a group during acquire
  BOOL mSkipStageMoveInAcquire;  // Skip stage moves: flag used during acquisition
  bool mAcqDlgPostponed;    // Flag that acquire dialog closed with postpone
  int mPostponedSubsetStart; // Subset values when postponed
  int mPostponedSubsetEnd;
  BOOL mPostposedDoSubset;
  bool mAcqCycleDefocus;     // Flag that acquire is actually cycling focus given settings
  int mLastMapForVectors;    // For keeping track of using map hole vectors
  bool mSettingUpFullMont;   // Flag so that fitting can not add extra area if skewed
  float mMaxAngleExtraFullMont;  // Maximum angle at which extra size will be added
  int mMinNewFileInterval;   // Interval at which to add new files when using Shift N
  bool mLastGridSetAcquire;  // Whether turned on a aqcuire in last grid of points
  bool mLastGridFillItem;    // Whether last grid of points filled a specified item
  int mLastGridPatternPoly;  // -1 if no valid last grid, 0 for pattern, 1 for polygon
  float mLastGridInSpacing;  // Spacing entered for regular fill of polygon
  int mLastGridAwayFromFocus;  // Whether last grid of points was laid out away from focus
  float mLastGridPolyArea;     // Area of polygon/map in ast grid of points filling one
  bool mEditFocusEnabled;      // Keep track if Edit Focus is enabled, required for drawing
  bool mShowingLDareas;        // Flag if conditions are set for showing all areas
  ScaleMat mIStoStageForHoles; // Matrix for converting multihole IS values to stage
  int mMagIndForHoles;         // The mag index and camera needed when working with 
  int mCameraForHoles;         // hole pattern of current point
  FloatVec mCurItemHoleXYpos;  // Stage positions of the holes drawn around current point
  IntVec mCurItemHoleIndex;    // And the hole index for that item, needed for deletion
  float mMultiDelStageX, mMultiDelStageY;  // Stage position for shift double click deletion
  float mMapDblClickScale;     // Scale microns to pixel for double click map load
  FloatVec *mHFxCenters;
  FloatVec *mHFyCenters;
  FloatVec *mHFxInPiece;
  FloatVec *mHFyInPiece;
  IntVec *mHFpieceOn;
  ShortVec *mHFgridXpos;
  ShortVec *mHFgridYpos;
  std::vector<short> *mHFexclude;
  bool mAddingFoundHoles;
  bool mDeferAddingToViewer;
  int mStepIndForDewarCheck;   // The step at which to do dewar check just before acquire
  bool mDidEucentricity;       // Flag that eucentricity was done one of 3 ways
  bool mWillDoTemplateAlign;   // Flag this will be done, so realign to item can do hybrid
  bool mWillRelaxStage;        // Flag this will be done, so move can impose backlash
  int mAcqMadeMapWithID;       // Map ID of map made on this cycle
  int mRetValFromMultishot;    // Return value has negative of number being gotten
  int mRunScriptAfterNextAcq;  // Script # to run at end of next acquisition if completed
  int mScriptToRunAtEnd;       // Script # to run when current acquisition ends
  bool mRetractAtAcqEnd;
  int mNumAcqFilesLeftOpen;    // Flag that some files were left open
  int mNumReconnectsInAcq;     // Number of reconnections that occurred during acquire
  bool mReconnectedInAcq;      // flag that reconnect happened
  double mReconnectStartTime;  // Time when reconnect started
  int mListHeaderTop;          // Common top and individual left positions for top labels
  int mListHeaderLefts[9];
  int mFileRangeForMultiGrid;  // Flag that new file over range is done for multiple grids
  bool mIgnoreUpdates;         // Flag to prevent update of Nav Acquire when getting file
  int mNumWrongMapWarnings;    // # of warnings given when nav table not right for map
  bool mReloadTableOnNextAdd;  // Flag set by multigrid to work around vanishing map bug

public:
  afx_msg void OnGotoMarker();
  BOOL RegistrationChangeOK(void);
  int ChangeItemRegistration(void);
  int ChangeItemRegistration(int index, int newReg, CString &str);
  void AdjustAndMoveStage(float stageX, float stageY, float stageZ, int axisbits, 
    float backX = -999., float backY = -999., double leaveISX = 0., double leaveISY = 0.,
    float tiltAngle = 0.);
  int RotateMap(EMimageBuffer * imBuf, BOOL redraw);
  CMapDrawItem * FindItemWithMapID(int mapID, bool requireMap = true, bool matchGroup = false);
  float RotationFromStageMatrices(ScaleMat curMat, ScaleMat mapMat, BOOL &inverted);
  int AccessMapFile(CMapDrawItem * item, KImageStore *&storeMRC, int & curStore, MontParam *&montP, 
    float & useWidth, float & useHeight, bool readWrite = false);
  int RealignToCurrentItem(BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS, int realiFlags, int setForScaled);
  void GetAdjustedStagePos(float & stageX, float & stageY, float & stageZ);
  void AdjustISandMagForStageConversion(int &magInd, double &ISX, double &ISY);
  void ShiftToMarker(void);
  void UndoShiftToMarker(void);
  int ShiftItemsAtRegistration(float shiftX, float shiftY, int registration, int saveOrRestore = 0);
  int ShiftCohortOfItems(float shiftX, float shiftY, int registration, int magInd, int cohortID, 
    bool useAll, bool wasShifted, int saveOrRestore);
  void ShiftItemAndPoints(float shiftX, float shiftY, CMapDrawItem *item, int itemInd, int &numShift);
  void FinishLoadMap(void);
  void LoadMapCleanup(void);
  CButton m_butRealign;
  afx_msg void OnRealigntoitem();
  int GetCurrentOrAcquireItem(CMapDrawItem *&item);
  int GetMapOrMapDrawnOn(int index, CMapDrawItem *&item, CString &mess);
  int RealignToOtherItem(int index, BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS, int realiFlags, int setForScaled);
  int RealignToAnItem(CMapDrawItem * item, BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS, int realiFlags, int setForScaled);
  CMapDrawItem * GetOtherNavItem(int index);
  BOOL mMovingStage;
  BOOL StartedMacro(void);
  void RemoveAutosaveFile(void);
  void SavePreCombineFile(void);
  int GetStockFilename(void);
  int MoveToItem(int index, BOOL skipZ = false);
  void ProcessAkey(BOOL ctrl, BOOL shift);
  void AddCirclePolygon(float radius);
  int DoAddCirclePolygon(float radius, float stageX, float stageY, BOOL useMarker, CString &mess);
  BOOL SupermontLabel(CMapDrawItem * item, int & startNum, int & ix, int & iy);
  BOOL CurrentIsInGroup(void);
  void DeleteGroup(bool collapsedGroup);
  void PolygonSupermontage(void);
  void PolygonToCameraCoords(CMapDrawItem * item, int camera, int magIndex, 
    int adjustForFocusSet, float &xMin, float &xMax, float &yMin, float &yMax);
  bool IsFrameNeeded(CMapDrawItem * item, int xFrame, int yFrame, float xOverlap, 
    float yOverlap, float xExtra, float yExtra, float xOffset, float yOffset, int ix, 
    int iy, int xNframes, int yNframes, float &xMid, float &yMid);
  CMapDrawItem * GetCurrentItem(void);
  int RegistrationUseType(int reg);
  void ImportMap(void);
  void TransformOneItem(CMapDrawItem * item, ScaleMat aM, float * dxy, int fromReg, int toReg);
  void UndoTransform(void);
  BOOL ImBufIsImportedMap(EMimageBuffer * imBuf);
  BOOL CurrentIsImported(void);
  void ScheduleNewFile(void);
  int CountItemsInGroup(int curID, CString & label, CString & lastlab, int &numAcquire, 
    IntVec *indexVec = NULL);
  void CleanSchedule(void);
  BOOL OKtoAddGrid(bool likeLast);
  void AddGridOfPoints(bool likeLast);
  int MakeGridOrFoundPoints(int jstart, int jend, int jdir, int kstart,
    int kend, int dir, CMapDrawItem *poly, float spacing, float jSpacing, float kSpacing,
    int registration, float stageZ, float xcen, float ycen,
    float delX, float delY, bool acquire, ScaleMat &aInv, float groupExtent,
    bool awayFromFocus, int drawnOnID, bool likeLast);
  void InterPointVectors(CMapDrawItem **gitem, float * vecx, float * vecy, float * length,
    int i, int num);
  void SetupForAwayFromFocus(float incStageX1, float incStageY1, float incStageX2, float incStageY2, 
    int &jstart, int &jend, int &jdir, int &kstart, int &kend, int &dir);
  CStatic m_statListHeader;
  void GetSuperCoords(int & superX, int &superY);
  int ShiftItemsByAlign(void);
  int DoLoadMap(bool synchronous, CMapDrawItem *item, int bufToReadInto, BOOL display = true);
  CButton m_butTiltSeries;
  BOOL m_bTiltSeries;
  CButton m_butFileAtItem;
  BOOL m_bFileAtItem;
  CButton m_butGroupFile;
  BOOL m_bGroupFile;
  CStatic m_statFilename;
  CString m_strFilename;
  CButton m_butFileProps;
  CButton m_butScopeState;
  CButton m_butTSparams;
  CButton m_butFilename;
  CButton m_butUpdateZ;
  afx_msg void OnCheckTiltSeries();
  afx_msg void OnCheckFileatitem();
  afx_msg void OnCheckFileatgroup();
  afx_msg void OnButFileprops();
  afx_msg void OnButState();
  afx_msg void OnButTsparams();
  afx_msg void OnButFilename();
  afx_msg void OnButUpdatez();
  int GroupScheduledIndex(int ID);
  CEdit m_editPtLabel;
  CComboBox m_comboColor;
  CButton m_butDrawOne;
  bool UpdateIfItem(void);
  CButton m_checkDualMap;
  BOOL m_bDualMap;
  CButton m_butDualMap;
  afx_msg void OnCheckDualMap();
  afx_msg void OnButDualMap();
  int DoMakeDualMap();
  void SendEmailIfNeeded(void);
  CString m_strItemNum;
  int OpenFileIfNeeded(CMapDrawItem * item, bool stateOnly);
  void UpdateGroupStrings(int groupID);
  void ToggleGroupAcquire(bool collapsedGroup);
  void MakeListMappings(void);
  bool GetCollapsedGroupLimits(int listInd, int &start, int &end);
  afx_msg void OnCollapseGroups();
  bool IndexOfSingleOrFirstInGroup(int listInd, int & itemInd);
  int GetCurrentOrGroupItem(CMapDrawItem *& item);
  void SetCollapsing(BOOL state);
  CButton m_butCollapse;
  afx_msg void OnKillfocusEditBox();
  int DoUpdateZ(int index, int ifGroup);
  void SetAcquireEnded(int state);
  int TransformToCurrentReg(int reg, ScaleMat aM, float *dxy, int regDrawnOn, int curDrawnOn);
  int TransformFromRotAlign(int reg, ScaleMat aM, float *dxy);
  void ProcessCKey(void);
  void ProcessRKey();
  ScaleMat * XformForStageStretch(bool usingIt);
  bool LineInsideContour(float * ptsX, float * ptsY, int numPoints, float xStart, float yStart, float xEnd, float yEnd);
  void FillItemWithCornerPoints(CMapDrawItem *itmp);
  void PolygonFromCorners(void);
  void FinishMultipleDeletion(void);
  bool BacklashForItem(CMapDrawItem *item, float & backX, float & backY);
  void MontErrForItem(CMapDrawItem * item, float & montErrX, float & montErrY);
  void CheckRawStageMatches(void);
  void TransferBacklash(EMimageBuffer * imBuf, CMapDrawItem * item);
  void BacklashFinished(float deltaX, float deltaY);
  bool OKtoAdjustBacklash(bool fastStage);
  void AdjustBacklash(int index = -1, bool showC = false);
  void MarkerStagePosition(EMimageBuffer * imBuf, ScaleMat aMat, float delX, float delY, 
    float & stageX, float & stageY, int useLineEnd = 0, int *pcInd = NULL,
    float *xInPiece = NULL, float *yInPiece = NULL);
  BOOL MarkerStagePosition(EMimageBuffer * imBuf, float & stageX, float & stageY);
  BOOL m_bShowAcquireArea;
  CButton m_butAddMarker;
  afx_msg void OnAddMarker();
  afx_msg void OnShowAcquireArea();
  CMapDrawItem *AddPointMarkedOnBuffer(EMimageBuffer * imBuf, float stageX, float stageY,
    int groupID);
  void SetupItemMarkedOnBuffer(EMimageBuffer *imBuf, CMapDrawItem *item);
  int AddImagePositionOnBuffer(EMimageBuffer * imBuf, float imageX, float imageY, float stageZ, int groupID);
  int AddPolygonFromImagePositions(EMimageBuffer * imBuf, float *imageX, float *imageY, int numPts, float stageZ);
  void AddItemFromStagePositions(float *imageX, float *imageY, int numPts, float stageZ, int groupID);
  int ImodObjectToPolygons(EMimageBuffer *imBuf, Iobj *obj, MapItemArray &polyArray);
  void AddAutocontPolygons(MapItemArray &polyArray, ShortVec &excluded,
    ShortVec &groupNums, int *groupShown, int numGroups, int &firstID, int &lastID, IntVec &indsInPoly);
  void AddSingleAutocontPolygon(MapItemArray &polyArray, int groupID);
  void UndoAutocontPolyAddition(MapItemArray &polyArray, int numRemove, IntVec &indsInPoly);
  void RefillAfterAutocontPolys();
  bool OKtoAddMarkerPoint(bool justAdd, bool fromMacro = false);
  void UpdateAddMarker(void);
  bool RawStageIsRevisable(bool fastStage);
  bool MovingMapItem(void);
  int TargetLDAreaForRealign(void);
  int OKtoAverageCrops(void);
  CMapDrawItem *FindMontMapDrawnOn(CMapDrawItem * item);
  void StageOrImageCoords(CMapDrawItem *item, float &posX, float &posY);
  void GridImageToStage(ScaleMat aInv, float delX, float delY, float posX, float posY, 
    float &stageX, float &stageY);
  int AddFoundHoles(FloatVec *xCenters, FloatVec *yCenters, std::vector<short> *exclude, FloatVec *xInPiece,
    FloatVec *yInPiece, IntVec *pieceOn, ShortVec *gridXpos, ShortVec *gridYpos,
    float spacing, float diameter, float incStageX1, float incStageY1, float incStageX2, float incStageY2, int registration, int mapID, float stageZ,
    CMapDrawItem *poly, int layoutType);
  int FindBufferWithMontMap(int mapID);
  int FindBufferWithMapID(int mapID);
  int CheckIfMapIsInMultigridNav(int mapID);
  CButton m_butEditMode;
  BOOL m_bEditMode;
  afx_msg void OnEditMode();
  CMapDrawItem *GetSingleSelectedItem(int *index = NULL);
  void AddPointOnGrid(int j, int k, CMapDrawItem *poly, int registration, int groupID, 
    int startnum, int &num, int drawnOnID, float stageZ, float spacing, 
    float delX, float delY, bool acquire, ScaleMat &aInv);
  void GridStagePos(int j, int k, float delX, float delY, ScaleMat &aInv, float &xx, 
    float &yy);
  bool BoxedPointOutsidePolygon(CMapDrawItem *poly, float xx, float yy, float spacing);
  void CloseFileOpenedByAcquire(void);
  CMapDrawItem *FindItemWithString(CString & string, BOOL ifNote, bool caseSensitive = false);
  void MouseDoubleClick(int button);
  void ProcessDKey(void);
  void ProcessTKey(void);
  void ProcessNKey(void);
  void ProcessVKey(void);
  void ProcessHKey(void);
  int ProcessRangeKey(const char *key, int &shiftIndex, int &start, int &end);
  void ClearRangeKeys();
  void ToggleNewFileOverRange(int start, int end, int forMultiGrid = 0);
  void NewFileRangeNextTask(int param);
  BOOL m_bDrawLabels;
  afx_msg void OnDrawLabels();
  void ManageNumDoneAcquired(void);
  int SetCurrentRegistration(int newReg);
  int MakeUniqueID(void);
  void EvaluateAcquiresForDlg(CNavAcquireDlg *dlg, bool fromMultiGrid = false);
  int OKtoSkipStageMove(NavAcqParams *param);
  int OKtoSkipStageMove(NavAcqAction *actions, int acqType);
  int EndAcquireWithMessage(void);
  void SkipToNextItemInAcquire(CMapDrawItem *item, const char *failedStep, bool skipManage = false);
  int GetCurrentGroupSizeAndPoints(int maxPoints, float * stageX, float * stageY, 
    float *defocusOffset);
void MoveStageOrDoImageShift(int axisBits);
void SetCurrentSelection(int listInd);
int SetSelectedItem(int itemInd, bool redraw = true);
int LimitsOfContiguousGroup(int itemInd, int &groupStart, int & groupEnd);
void IStoXYandAdvance(int &direction);
CMapDrawItem *FindNextAcquireItem(int &index);
bool IsItemToBeAcquired(CMapDrawItem *item, bool &skippingGroup);
float ContourArea(float *ptx, float *pty, int numPoints);
void GetCurrentNavDir(CString &navPath);
void SetCurrentNavFile(CString &inFile);
void UpdateHiding();
void ManageListHeader(CString str = "");
CButton m_butNavFocusPos;
afx_msg void OnButNavFocusPos();
CButton m_butEditFocus;
BOOL m_bEditFocus;
afx_msg void OnEditFocus();
void SetChanged(BOOL inVal);
void AddFocusAreaPoint(bool drawFirst);
bool AtSamePosAsHigherMagMapInRange(int itemInd, int startInd, int endInd);
bool GetHolePositionVectors(FloatVec **xypos, IntVec **index);
CButton m_butTableIndexes;
BOOL m_bTableIndexes;
afx_msg void OnTableIndexes();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NAVIGATORDLG_H__F2EDDA5D_BA58_45CC_8458_5CF199EB5007__INCLUDED_)
