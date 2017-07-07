#pragma once
#include <afxtempl.h>

class CMapDrawItem;
struct ScheduledFile;
class CStateDlg;
class CNavRotAlignDlg;
struct TiltSeriesParam;

// Structure for keeping track pf parameters that enable skipping center align in round 1
struct CenterSkipData
{
  float firstStageX;     // Starting stage position and image shift, piece-dependent
  float firstStageY;
  double firstISX;
  double firstISY;
  int timeStamp;        // Time of last alignment to this center
  float baseDelX;       // invariant components of stage move to a target
  float baseDelY;
};

enum SetStateTypes {STATE_NONE = 0, STATE_IMAGING, STATE_MAP_ACQUIRE};

class CNavHelper
{
public:
  CNavHelper(void);
  ~CNavHelper(void);
  int RealignToItem(CMapDrawItem * item, BOOL restoreState);
  float PointSegmentDistance(float ptX, float ptY, float x1, float y1, float x2, float y2);
  GetMember(int, Realigning)
  SetMember(float, RImaximumIS)
  SetMember(float, MaxMarginNeeded)
  SetMember(float, MinMarginNeeded)
  SetMember(float, MinMarginWanted)
  GetSetMember(int, TypeOfSavedState)
  GetMember(BOOL, AcquiringDual)
  GetSetMember(BOOL, SearchRotAlign)
  GetSetMember(float, RotAlignRange)
  GetSetMember(float, RotAlignCenter)
  SetMember(float, DistWeightThresh);
  SetMember(float, RImaxLMfield);
  SetMember(int, RIskipCenTimeCrit);
  SetMember(float, RIskipCenErrorCrit);
  SetMember(float, RIweightSigma);
  GetSetMember(BOOL, RIuseBeamOffsets);
  SetMember(int, ContinuousRealign);
  GetSetMember(float, GridGroupSize);
  GetSetMember(BOOL, DivideIntoGroups);
  GetSetMember(bool, EditReminderPrinted);
  SetMember(float, RITiltTolerance);
  GetSetMember(BOOL, CollapseGroups);
  SetMember(int, ExtDrawnOnID);
  void ForceCenterRealign() {mCenterSkipArray.RemoveAll();};

  CStateDlg *mStateDlg;
  CArray<StateParams *, StateParams *> *GetStateArray () {return &mStateArray;};
  CNavRotAlignDlg *mRotAlignDlg;

private:
  CSerialEMApp *mWinApp;
  CSerialEMDoc *mDocWnd;
  CEMscope *mScope;
  MontParam *mMontParam;
  MagTable   *mMagTab;
  EMimageBuffer *mImBufs;
  CShiftManager *mShiftManager;
  CameraParameters *mCamParams;
  EMbufferManager *mBufferManager;
  CNavigatorDlg *mNav;
  CArray<CMapDrawItem *, CMapDrawItem *> *mItemArray;
  CArray<ScheduledFile *, ScheduledFile *> *mGroupFiles;
  CArray<FileOptions *, FileOptions *> *mFileOptArray;
  CArray<TiltSeriesParam *, TiltSeriesParam *> *mTSparamArray;
  CArray<MontParam *, MontParam *> *mMontParArray;
  CArray<StateParams *, StateParams *> *mAcqStateArray;
  CArray<StateParams *, StateParams *> mStateArray;
  CCameraController *mCamera;
  WINDOWPLACEMENT mStatePlacement;
  CArray<CenterSkipData, CenterSkipData> mCenterSkipArray;

  std::vector<int> mPieceSavedAt;  // Sections numbers for existing pieces in montage
  int mSecondRoundID;           // Map ID of map itself for 2nd round alignment
  KImageStore *mMapStore;       // Pointer to file to use
  MontParam *mMapMontP;         // Pointer to montage params to use
  MontParam mMapMontParam;      // Our set of params for non-open file
  int mRIitemInd;               // Index of item being aligned to
  int mRImapID;                 // Map ID of first round map aligned to
  ScaleMat mRImat;              // Scalemat of the map being aligned to
  float mRIdelX, mRIdelY;       // Scaling offsets
  ScaleMat mRIrMat;             // Rotation matrix for rotating the map
  int mRIix, mRIiy;             // Piece number of piece containing target
  int mRIixSafer, mRIiySafer;   // Piece number of initial safer piece
  int mCurStoreInd;             // Store index if map being aligned is an open file
  float mRIrotAngle;            // Rotation angle of map being aligned to
  BOOL mRIinverted;             // Inversion state
  int mRealigning;              // Flag that realign is happening, with round 1 or 2
  float mRItargetX, mRItargetY; // Target stage position possibly adjusted for lower mag
  float mRIabsTargetX;          // Absolute target stage position of last align
  float mRIabsTargetY;
  float mRIfirstStageX;         // Stage position used for going to first piece
  float mRIfirstStageY;
  double mRIfirstISX;           // IS to apply during first round and take away at end
  double mRIfirstISY;
  double mRIleaveISX;           // And IS to leave if leaving LD with Balance Shifts
  double mRIleaveISY;
  float mRImaximumIS;           // Maximum image shift to apply in first round
  float mRImaxLMfield;          // Maximum field of view for realigning to LM map
  bool mRItryScaling;           // Flag to do scaling in this realign
  BOOL mUseMontCenter;          // Flag to align to montage center instead of a piece
  float mExpectedXshift;        // Expected shift to feed to autoalign in align to target
  float mExpectedYshift;
  float mRIscaling;             // Scaling to feed it too
  float mRItargMontErrX;        // Montage errors of target position
  float mRItargMontErrY;
  int mCenSkipIndex;            // Index to current entry in center skip array
  int mRIskipCenTimeCrit;       // Criterion time for reusing center alignment
  float mRIskipCenErrorCrit;    // Criterion error for stopping reuse
  float mRIweightSigma;         // Sigma for weighting by probability in corr at target
  int mTypeOfSavedState;        // Type of saved state: 1 imaging, 2 map acquire
  int mSavedLowDoseArea;        // Low dose area when saved state
  StateParams mSavedState;      // State that was saved
  float mMaxMarginNeeded;       // Maximum center to edge distance for preferring a map
  float mMinMarginWanted;
  float mMinMarginNeeded;       // Minimum center to edge distance for doing realign
  float mDistWeightThresh;      // Threshold for weighting CCC by distance
  float mStageErrX, mStageErrY; // Stage error of the last align
  float mLocalErrX, mLocalErrY; // Second round component of stage error
  int mRegistrationAtErr;       // Registration at which that was obtained
  int mRInumRounds;             // Number of rounds
  float mPreviousErrX;          // Defined stage error to adjust for when aligning
  float mPreviousErrY;          // In initial round and for adjusting coordinates
  float mPrevLocalErrX;         // To add to stage move to target only in second round
  float mPrevLocalErrY;
  BOOL mUseMontStageError;      // Flag to use stage error in getting stage pos of piece
  float mGridLimits[4];         // User's limits for full grid montage
  ControlSet mSavedConset;      // Save the record conset when setting a map state
  CString mSavedStoreName;      // Save name of current open file when setting state
  CString mNewStoreName;        // File name for new store created
  BOOL mLastTypeWasMont;        // Keep track of setting from montage query
  BOOL mLastMontFitToPoly;      // And from whether it was fit to a poly
  BOOL mAcquiringDual;          // Flag that it is acquiring dual map
  int mMapStateItemID;          // ID of item that was used to get into map state
  int mSavedMapStateID;         // Saved value when switch to dual map item
  int mIndexAfterDual;          // Index to set after dual is done
  BOOL mSearchRotAlign;         // Flag to do rotation search
  float mRotAlignRange;         // Range of search
  float mRotAlignCenter;        // Center angle for rotation
  WINDOWPLACEMENT mRotAlignPlace;
  double mRIdefocusOffsetSet;   // Defocus offset set in realign to item
  int mRIalphaSet;              // Alpha value set in realign to item
  int mRIalphaSaved;            // Alpha value that it was before
  float mRIbeamShiftSetX;       // Beam shift and tilt imposed in realign to item
  float mRIbeamShiftSetY;
  float mRIbeamTiltSetX;
  float mRIbeamTiltSetY;
  float mRInetViewShiftX;
  float mRInetViewShiftY;
  float mRIcalShiftX;           // The adjustment to be applied by AdjustAndMoveStage
  float mRIcalShiftY;
  int mRIconSetNum;
  bool mRIstayingInLD;
  LowDoseParams mRIsavedLDparam;
  BOOL mRIuseBeamOffsets;       // Flag for whether to use the offsets
  float mTestParams[4];         // Parameters that can be set from macro
  int mContinuousRealign;       // Flag to use continuous mode; > 1 to leave it running
  int mRIContinuousMode;        // Copy of flag for this run of realign
  float mRITiltTolerance;       // Maximum tilt difference for not tilting before realign
  float mGridGroupSize;         // Radius in microns for adding points in groups
  BOOL mDivideIntoGroups;       // Flag for whether to do it
  bool mEditReminderPrinted;     // Flag that reminder printed when edit mode turned on
  BOOL mCollapseGroups;         // To keep "collapse" setting for dialog
  int mExtDrawnOnID;            // ID of current map for external points
  float  mExtUseWidth, mExtUseHeight;    // Parameters of current map for external items
  int mExtXframe, mExtYframe;
  int mExtXspacing, mExtYspacing;
  float mExtLoadWidth, mExtLoadHeight;
  ScaleMat mExtInv;              // Image to stage coordinate transform
  double mExtDelX, mExtDelY;
  MiniOffsets mExtOffsets;       // Offset values for aligned montage map
  int mExtTypeOfOffsets;         // Type of offsets loaded there


public:
  void PrepareToReimageMap(CMapDrawItem * item, MontParam * param, ControlSet * conSet,
    int baseNum, BOOL hideLDoff);
  void StagePositionOfPiece(MontParam * param, ScaleMat aMat, float delX, float delY, 
    int ix, int iy, float &stageX, float & stageY, float &montErrX, float &montErrY);
  void RealignNextTask(int param);
  void RealignCleanup(int error);
  void StopRealigning(void);
  int RotateForAligning(int bufNum);
  int TransformBuffer(EMimageBuffer * imBuf, ScaleMat sizingMat, int sizingWidth,
    int sizingHeight, float sizingFrac, ScaleMat rMat);
  void StartSecondRound(void);
  float PieceToEdgeDistance(MontParam * montP, int ixPiece, int iyPiece);
  int GetLastStageError(float &stageErrX, float &stageErrY, float &localErrX,
    float &localErrY);
  int GetLastErrorTarget(float & targetX, float & targetY);
  void NavOpeningOrClosing(bool open);
  float *GetGridLimits() {return &mGridLimits[0];};
  void Initialize(void);
  void RestoreSavedState(void);
  int SetToMapImagingState(CMapDrawItem * item, bool setCurFile);
  int RestoreFromMapState(void);
  void ChangeAllBufferRegistrations(int mapID, int fromReg, int toReg);
  CString NextAutoFilename(CString inStr);
  int NewAcquireFile(int itemNum, int fileType, ScheduledFile *sched);
  int SetFileProperties(int itemNum, int fileType, ScheduledFile *sched);
  int SetOrChangeFilename(int itemNum, int fileType, ScheduledFile *sched);
  void RemoveFromArray(int which, int index);
  void ChangeRefCount(int which, int index, int dir);
  void RecomputeArrayRefs(void);
  int SetTSParams(int itemNum);
  bool EarlierItemSharesRef(int itemNum, int refInd, int which, int &shareInd);
  void EndAcquireOrNewFile(CMapDrawItem * item, bool endGroupFile = false);
  ScheduledFile * GetFileTypeAndSchedule(CMapDrawItem * item, int & fileType);
  void DeleteArrays(void);
  int FindMapForRealigning(CMapDrawItem * inItem, BOOL restoreState);
  void StoreCurrentStateInParam(StateParams * param, BOOL lowdose, 
    bool saveLDfocusPos);
  void StoreMapStateInParam(CMapDrawItem * item, MontParam *montP, int baseNum, 
    StateParams * param);
  void SetStateFromParam(StateParams *param, ControlSet *conSet, int baseNum, 
    BOOL hideLDoff = FALSE);
  void SaveCurrentState(int type, bool saveLDfocusPos);
  StateParams* NewStateParam(bool navAcquire);
  StateParams * ExistingStateParam(int index, bool navAcquire);
  void OpenStateDialog(void);
  WINDOWPLACEMENT * GetStatePlacement(void);
  void UpdateStateDlg(void);
  void ClearStateArray(void);
  void MakeDualMap(CMapDrawItem *item);
  int DualMapBusy(void);
  void DualMapDone(int param);
  void DualMapCleanup(int error);
  void StopDualMap(void);
  int AssessAcquireProblems(int startInd, int endInd);
  void ListFilesToOpen(void);
  bool NameToOpenUsed(CString name);
  int AlignWithRotation(int buffer, float centerAngle, float angleRange, 
    float &rotBest, float &shiftXbest, float &shiftYbest, float scaling = 0.);
  int OKtoAlignWithRotation(void);
  void OpenRotAlignDlg(void);
  WINDOWPLACEMENT * GetRotAlignPlacement(void);
  int BufferForRotAlign(int &registration);
  int AlignWithScaling(float & shiftX, float & shiftY, float & scaling);
  int LookupMontStageOffset(KImageStore *storeMRC, MontParam *param, int ix, int iy, 
    std::vector<int> &pieceSavedAt, float &montErrX, float &montErrY);
  void InterpMontStageOffset(KImageStore *imageStore, MontParam *montP, ScaleMat aMat,
    std::vector<int> &pieceSavedAt, float delX, float delY, float &montErrX, float &montErrY);
  void NoLongerInheritMessage(int itemNum, int sharedInd, char * typeText);
  void AddInheritingItems(int num, int prevIndex, int type, CString & listStr);
  void BeInheritedByMessage(int itemNum, CString & listStr, char * typeText);
  void SetTestParams(double *params) {for (int i = 0; i < 4; i++) mTestParams[i] = (float)params[i];};
  int LoadForAlignAtTarget(CMapDrawItem *item);
  void SetMapOffsetsIfAny(CMapDrawItem * item);
  void RestoreMapOffsets();
  int DistAndPiecesForRealign(CMapDrawItem *item, float targetX, float targetY, 
    int pcDrawnOn, int mapInd, 
    ScaleMat &aMat, float &delX, float &delY, float &distMin, int &ixPiece, int &iyPiece, 
    int &ixSafer, int &iySafer, int &imageX, int &imageY, bool keepStore, int &xFrame, 
    int &yFrame, int &binning);
    
  void LoadPieceContainingPoint(CMapDrawItem * ptItem, int mapIndex);
  void StartRealignCapture(bool useContinuous, int nextTask);
  void GetViewOffsets(CMapDrawItem * item, float & netShiftX, float & netShiftY, float & beamShiftX, float & beamShiftY, float & beamTiltX, float & beamTiltY);
  void StateCameraCoords(int camIndex, int xFrame, int yFrame, int binning, int &left, int &right, int &top, int &bottom);
  bool CanStayInLowDose(CMapDrawItem * item, int xFrame, int yFrame, int binning, int & set, float & netShiftX, float & netShiftY, bool forReal);
  void SimpleIStoStage(CMapDrawItem * item, double ISX, double ISY, float &stageX, float &stageY);
  ScaleMat ItemStageToCamera(CMapDrawItem * item);
  void CountAcquireItems(int startInd, int endInd, int & numAcquire, int & numTS);
  void RestoreLowDoseConset(void);
  int ProcessExternalItem(CMapDrawItem * item, int extType);
  int TransformExternalCoords(CMapDrawItem *item, int extType, 
     CMapDrawItem *mapItem, float &fx, float &fy, int &pieceDrawnOn);
  void CleanupFromExternalFileAccess();
};

