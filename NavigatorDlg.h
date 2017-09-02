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

class CMontageSetupDlg;
struct TiltSeriesParam;
class CNavAcquireDlg;

#define MAX_CURRENT_REG 99
#define MAX_SAVED_REGXFORM 10
#define MIN_INTERNAL_ID 100000
enum NavAcquireTypes {ACQUIRE_TAKE_MAP = 0, ACQUIRE_IMAGE_ONLY, ACQUIRE_RUN_MACRO,
  ACQUIRE_DO_TS};
enum NavRegTypes {NAVREG_UNUSED, NAVREG_REGULAR, NAVREG_IMPORT};
enum NavNewFileTypes {NAVFILE_ITEM = 0, NAVFILE_GROUP, NAVFILE_TS};
enum NavArrayTypes {NAVARRAY_FILEOPT, NAVARRAY_MONTPARAM, NAVARRAY_TSPARAM, 
  NAVARRAY_STATE};
// DO NOT REARRANGE/RENUMBER
enum NavExtTypes {NAVEXT_ON_MAP = 0, NAVEXT_ON_ALIMONT, NAVEXT_ON_VSMONT, 
  NAVEXT_ON_PIECE};
enum NavExtErrors {NEXERR_MULTIPLE_ENTRY = 0, NEXERR_NO_DRAWN_ON, NEXERR_NO_MAP_WITH_ID,
  NEXERR_MAP_NOT_MONT, NEXERR_ACCESS_FILE, EXTERR_NO_PIECE_ON, EXTERR_NO_ALI_COORDS, 
  EXTERR_NO_PC_COORD, EXTERR_NO_MDOC, EXTERR_BAD_MDOC_IND};

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
  BOOL FittingMontage() {return mMontItem != NULL;};
  int FitMontageToItem(MontParam * montParam, int binning, int magIndex, 
    BOOL forceStage);
	void SetupSuperMontage(BOOL skewed);
	void FullMontage();
  void AutoSave();
	BOOL ConvertIStoStageIncrement(int magInd, int camera, double ISX, double ISY, 
    float angle, float &stageX, float &stageY);
	void MoveListSelection(int direction);
	void SetupSkipList(MontParam * montParam);
	int GotoNextAcquireArea();
	void StopAcquiring(BOOL testMacro = false);
	void AcquireCleanup(int error);
	int TaskAcquireBusy();
	void AcquireNextTask(int param);
	void AcquireAreas();
	BOOL AcquireOK(bool tiltSeries = false, int startInd = 0, int endInd = -1);
  int NewMap(bool unsuitableOK = false);
  void DeleteItem() {OnDeleteitem();};
	void CornerMontage();
	int PolygonMontage(CMontageSetupDlg *montDlg);
	void TransformPts();
  void DoClose() {OnCancel();};
  BOOL GetAcquiring() {return mAcquireIndex >= 0;};
  int GetNumberOfItems() {return (int)mItemArray.GetSize();};
	BOOL SetCurrentItem(bool groupOK = false);
	BOOL CornerMontageOK();
	BOOL TransformOK();
  BOOL NoDrawing() {return !(mAddingPoints || mAddingPoly || mMovingItem);};
	int GetItemType();
  void LoadNavFile(bool checkAutosave, bool mergeFile);
	int SetupMontage(CMapDrawItem *item, CMontageSetupDlg *montDlg);
	void XfApply(ScaleMat a, float *dxy, float inX, float inY, float &outX, float &outY);
	int DoSaveAs();
	int DoSave();
	int AskIfSave(CString reason);
	CString NextTabField(CString inStr, int &index);
	int GetNavFilename(BOOL openFile, DWORD flags, bool mergeFile);
	void OpenAndWriteFile(bool autosave);
	BOOL BackspacePressed();
	BOOL BufferStageToImage(EMimageBuffer *imBuf, ScaleMat &aMat, float &delX, 
    float &delY);
	void ComputeStageToImage(EMimageBuffer *imBuf, float stageX, float stageY, 
    BOOL needAddIS, ScaleMat &aMat, float &delX, float &delY);
	void ShiftItemPoints(CMapDrawItem *item, float delX, float delY);
	void Redraw();
	void Update();
	int MoveStage(int axisBits);
	void SetCurrentStagePos(int index);
	CMapDrawItem *MakeNewItem(int groupID);
	void OnListItemDrag(int oldIndex, int newIndex);
	void SetRegPtNum(int inVal);
	int GetFreeRegPtNum(int registration, int proposed);
	void SetCurrentRegFromMax();
	CString FormatCoordinate(float inVal, int maxLen);
	void ManageListScroll();
	void ManageCurrentControls();
	void FillListBox(bool skipManage = false);
	void UpdateListString(int index);
	void ItemToListString(int index, CString &string);
	BOOL UserMousePoint(EMimageBuffer *imBuf, float inX, float inY, BOOL nearCenter, int button);
	CArray<CMapDrawItem *, CMapDrawItem *> *GetMapDrawItems(EMimageBuffer *imBuf, 
    ScaleMat &aMat, float &delX, float &delY, BOOL &drawAllReg, CMapDrawItem **acquireBox);
	CNavigatorDlg(CWnd* pParent = NULL);   // standard constructor
  int GetCurrentRegistration() {return mCurrentRegistration;};
  CArray<CMapDrawItem *, CMapDrawItem *> *GetItemArray() {return &mItemArray;};
  CArray<ScheduledFile *, ScheduledFile *> *GetScheduledFiles() {return &mGroupFiles;};
  CArray<FileOptions *, FileOptions *> *GetFileOptArray() {return &mFileOptArray;};
  CArray<TiltSeriesParam *, TiltSeriesParam *> *GetTSparamArray() {return &mTSparamArray;};
  CArray<MontParam *, MontParam *> *GetMontParArray() {return &mMontParArray;};
  CArray<StateParams *, StateParams *> *GetAcqStateArray() {return &mAcqStateArray;};
	void OnOK();
  GetMember(int, MarkerShiftReg)
  GetMember(int, AcquireEnded)
  GetMember(BOOL, LoadingMap)
  GetMember(int, NumSavedRegXforms)
  SetMember(int, SuperCoordIndex)
  GetMember(BOOL, StartedTS)
  GetMember(BOOL, FirstInGroup)
  SetMember(int, SkipBacklashType);
  GetSetMember(int, CurListSel)
  GetMember(int, NumAcquired);
  SetMember(CString, ExtraFileSuffix);
  GetMember(int, FoundItem);
  SetMember(bool, SkipAcquiringItem);
  SetMember(int, GroupIDtoSkip);
  SetMember(BOOL, SkipStageMoveInAcquire);
  GetSetMember(bool, PausedAcquire);
  void ResumeFromPause() {mResumedFromPause = true; mPausedAcquire = false;};
  BOOL InEditMode() {return m_bEditMode;};
  BOOL TakingMousePoints() {return m_bEditMode || mAddingPoints || mAddingPoly || mMovingItem;};
  std::set<int> *GetSelectedItems() {return &mSelectedItems;};
  void SetNextFileIndices(int fileIn, int montIn) {mNextFileOptInd = fileIn; mNextMontParInd = montIn;};
  int GetCurrentIndex() {return mCurrentItem;};

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
	afx_msg void OnSelendokCombocolor();
	afx_msg void OnCheckAcquire();
	afx_msg void OnDblclkListviewer();
	afx_msg void OnGotoMarker();
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
  CCameraController *mCamera;

  int mListBorderX, mListBorderY;
  int mNoteBorderX;  int mNoteHeight;
  int mLineBorderX;
  int mFilenameBorderX;
  BOOL mInitialized;
  CMapDrawItem *mItem;
  CArray<CMapDrawItem *, CMapDrawItem *> mItemArray;
  CArray<ScheduledFile *, ScheduledFile *> mGroupFiles;
  CArray<FileOptions *, FileOptions *> mFileOptArray;
  CArray<TiltSeriesParam *, TiltSeriesParam *> mTSparamArray;
  CArray<MontParam *, MontParam *> mMontParArray;
  CArray<StateParams *, StateParams *> mAcqStateArray;
  std::vector<int> mListToItem;
  std::vector<int> mItemToList;
  std::set<int> mSetOfIDs;
  int mCurrentRegistration;
  int mCurrentItem;         // Current item
  int mCurListSel;          // Current list selection in collapsed mode
  std::set<int> mSelectedItems;  // Set of selected items indexes
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
  int mEndingAcquireIndex;
  BOOL mSaveAlignOnSave;
  BOOL mSaveProtectRecord;
  BOOL mNavBackedUp;        // Flag for managing whether to create a .bak
  int mSuperNumX, mSuperNumY;  // Number of super frames in X and Y
  int mSuperOverlap;           // Pixels of overlap
  int mAcqActions[12];        // List of actions to do
  int mNumAcqActions;         // Number of actions
  int mAcqActionIndex;        // Current action index
  int mNumAcquired;          // Number actually acquired
  BOOL mStartedTS;           // Flag that started a tilt series
  bool mPausedAcquire;       // Flag that acquire is paused
  bool mResumedFromPause;    // Flag set when resume happens

  int mFrameLimitX, mFrameLimitY;         // Frame limits for montage setup
  CMapDrawItem *mMontItem;  // Item used
  CMapDrawItem *mMontItemCam;  // Item used with camera coordinates
  float mCamCenX, mCamCenY; // Center and extra for making skip list
  float mExtraX, mExtraY;
  ScaleMat mPolyToCamMat;   // Matrix used to get from polygon to camera coordinates
  int mMarkerShiftReg;      // Registration at which shift to marker occurred, for undo
  float mMarkerShiftX, mMarkerShiftY;  // Shift that was applied
  int mAcquireEnded;        // Flag that End Nav or Pause was pressed when running macro
  KImageStore *mLoadStoreMRC; // Variables to save state for asynchronous loading of map
  BOOL mReadingOther;
  int mCurStore;
  int mOriginalStore;
  float mUseWidth, mUseHeight;
  int mOverviewBinSave;
  BOOL mLoadingMap;         // Flag that map is being loaded
  int mShiftAIndex;        // Starting index for doing shift A
  int mShiftDIndex;        // Starting index for doing shift D
  int mNumSavedRegXforms;  // Number of saved reg-to-reg transforms
  int mLastXformToReg;     // Registration it was transformed to
  ScaleMat mMatSaved[MAX_SAVED_REGXFORM];
  float mDxySaved[MAX_SAVED_REGXFORM][2];
  int mFromRegSaved[MAX_SAVED_REGXFORM];
  CString mSavedListHeader;
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
  CString mExtraFileToClose;   // Name of extra file opened by Acquire
  CString mExtraFileSuffix; // Suffix for extra file to open when Acquire opens a file
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
  float mCenX, mCenY;       // Variables for getting from index to grid image position
  float mIncX1, mIncX2, mIncY1, mIncY2;
  bool mLastSelectWasCurrent;  // Flag that last mouse up was on current point
  double mLastAcqDoneTime;  // Time when last acquire item finished
  double mElapsedAcqTime;   // Cumulative elapsed time since start of acquire
  int mNumDoneAcq;          // Number of acquire points done (including failures)
  int mInitialNumAcquire;   // Initial number to be acquired
  bool mSkipAcquiringItem;  // Flag pre-macro can set to skip the item
  double mLastMontLeft;     // Last remaining montage time
  double mLastTimePerItem;  // Last time per item for that montage time
  int mGroupIDtoSkip;       // ID for skipping a group during acquire
  BOOL mSkipStageMoveInAcquire;  // Skip stage moves: flag used during acquisition

public:
  BOOL RegistrationChangeOK(void);
  int ChangeItemRegistration(void);
  int ChangeItemRegistration(int index, int newReg, CString &str);
  void AdjustAndMoveStage(float stageX, float stageY, float stageZ, int axisbits, 
    float backX = -999., float backY = -999., double leaveISX = 0., double leaveISY = 0.,
    float tiltAngle = 0.);
  int RotateMap(EMimageBuffer * imBuf, BOOL redraw);
  CMapDrawItem * FindItemWithMapID(int mapID, bool requireMap = true);
  ScaleMat GetRotationMatrix(float rotAngle, BOOL inverted);
  int PrepareMontAdjustments(EMimageBuffer * imBuf, ScaleMat & rMat, ScaleMat & rInv, float & rDelX, float & rDelY);
  void AdjustMontImagePos(EMimageBuffer * imBuf, float & inX, float & inY, int *pcInd = NULL, 
    float *xInPiece = NULL, float *yInPiece = NULL);
  float RotationFromStageMatrices(ScaleMat curMat, ScaleMat mapMat, BOOL &inverted);
  int AccessMapFile(CMapDrawItem * item, KImageStore *&storeMRC, int & curStore, MontParam *&montP, 
    float & useWidth, float & useHeight, bool readWrite = false);
  int RealignToCurrentItem(BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS);
  void GetAdjustedStagePos(float & stageX, float & stageY, float & stageZ);
  void ShiftToMarker(void);
  void UndoShiftToMarker(void);
  int ShiftItemsAtRegistration(float shiftX, float shiftY, int registration);
  void FinishLoadMap(void);
  void LoadMapCleanup(void);
  CButton m_butRealign;
  afx_msg void OnRealigntoitem();
  int GetCurrentOrAcquireItem(CMapDrawItem *&item);
  int RealignToOtherItem(int index, BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS);
  int RealignToAnItem(CMapDrawItem * item, BOOL restore, float resetISalignCrit, 
    int maxNumResetAlign, int leaveZeroIS);
  CMapDrawItem * GetOtherNavItem(int index);
  BOOL mMovingStage;
  BOOL StartedMacro(void);
  void RemoveAutosaveFile(void);
  int GetStockFilename(void);
  int MoveToItem(int index);
  void ProcessAkey(BOOL ctrl, BOOL shift);
  void AddCirclePolygon(float radius);
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
  BOOL OKtoAddGrid(void);
  void AddGridOfPoints(void);
  void InterPointVectors(CMapDrawItem **gitem, float * vecx, float * vecy, double * length, 
    int i, int num);
  CStatic m_statListHeader;
  void GetSuperCoords(int & superX, int &superY);
  int ShiftItemsByAlign(void);
  int DoLoadMap(bool synchronous);
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
    float & stageX, float & stageY, BOOL useLineEnd = false, int *pcInd = NULL,
    float *xInPiece = NULL, float *yInPiece = NULL);
  BOOL m_bShowAcquireArea;
  CButton m_butAddMarker;
  afx_msg void OnAddMarker();
  afx_msg void OnShowAcquireArea();
  CMapDrawItem *AddPointMarkedOnBuffer(EMimageBuffer * imBuf, float stageX, float stageY,
    int groupID);
  bool OKtoAddMarkerPoint(void);
  void UpdateAddMarker(void);
  bool RawStageIsRevisable(bool fastStage);
  bool MovingMapItem(void);
  int TargetLDAreaForRealign(void);
  int OKtoAverageCrops(void);
  CMapDrawItem *FindMontMapDrawnOn(CMapDrawItem * item);
  int OffsetMontImagePos(MiniOffsets *mini, int xPcStart, int xPcEnd,
    int yPcStart, int yPcEnd, float &testX, float &testY, int &pcInd, float &xInPiece, 
    float &yInPiece);
  void StageOrImageCoords(CMapDrawItem *item, float &posX, float &posY);
  void GridImageToStage(ScaleMat aInv, float delX, float delY, float posX, float posY, 
    float &stageX, float &stageY);
  int FindBufferWithMontMap(int mapID);
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
  CMapDrawItem *FindItemWithString(CString & string, BOOL ifNote);
  void MouseDoubleClick(int button);
  void ProcessDKey(void);
  BOOL m_bDrawLabels;
  afx_msg void OnDrawLabels();
  void ManageNumDoneAcquired(void);
  int SetCurrentRegistration(int newReg);
  int MakeUniqueID(void);
  void EvaluateAcquiresForDlg(CNavAcquireDlg *dlg);
  int OKtoSkipStageMove(NavParams * param);
  int OKtoSkipStageMove(BOOL roughEucen, BOOL realign, BOOL cook, BOOL fineEucen, 
    BOOL focus, int acqType);
  void EndAcquireWithMessage(void);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NAVIGATORDLG_H__F2EDDA5D_BA58_45CC_8458_5CF199EB5007__INCLUDED_)
