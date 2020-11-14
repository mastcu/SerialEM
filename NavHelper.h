#pragma once
#include <afxtempl.h>

class CMapDrawItem;
struct ScheduledFile;
class CStateDlg;
class CNavRotAlignDlg;
struct TiltSeriesParam;
class CMultiShotDlg;
class CHoleFinderDlg;
class HoleFinder;
class CMultiHoleCombiner;
class CMultiCombinerDlg;

#define MULTI_IN_HOLE   0x1
#define MULTI_HOLES     0x2
#define MULTI_TEST_IMAGE 0x4
#define MULTI_TEST_COMA 0x8

// Structure for keeping track of parameters that enable skipping center align in round 1
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

// Structure for the multiple-record parameters
struct MultiShotParams
{
  float spokeRad[2];     // Distance to off-center shots in main and second ring
  int numShots[2];       // Number of off-center shots in main and second ring
  BOOL doSecondRing;     // Do a second ring of shots
  int doCenter;          // Center shot: -1 before, 0 none, 1 after
  float extraDelay;      // Additional delay after image shift
  int doEarlyReturn;     // 1 for early return on last, 2 for all
  int numEarlyFrames;    // Number of frames in early return
  BOOL saveRecord;       // Whether to save Record shot
  BOOL adjustBeamTilt;   // Whether to adjust beam tilt if possible
  int inHoleOrMultiHole; // 1 to do pattern in hole, 2 to do multi-hole, 3 to do both
  int holeMagIndex;      // Mag at which hole positions are measured
  BOOL useCustomHoles;   // Use the list of custom holes
  float holeDelayFactor; // Factor by which to increase regular IS delay between holes
  double holeISXspacing[2];  // Hole spacing in each direction
  double holeISYspacing[2];
  float tiltOfHoleArray;    // Tilt angle at which regular array defined
  float holeFinderAngle;    // Angle found in hole finder associated with regular array
  int numHoles[2];       // Number of holes in each direction
  BOOL skipCornersOf3x3; // Take cross pattern when it is 3x3
  int customMagIndex;
  float tiltOfCustomHoles;  // Tilt angle at which custom holes defined
  FloatVec customHoleX;  // For custom holes, list of hole IS relative
  FloatVec customHoleY;  // to item point
  float beamDiam;        // Beam diameter: display only
  BOOL useIllumArea;     // Whether to use illuminated area for drawing diameter
};

struct HoleFinderParams
{
  FloatVec sigmas;        // Set of sigmas to try
  FloatVec thresholds;    // Set of median iterations to try
  float spacing;          // hole spacing
  float diameter;         // Hole diameter (microns)
  float maxError;         // Maximum error in microns
  BOOL useBoundary;       // Whether to look for a boundary polygon
  BOOL bracketLast;       // Whether to try to use subset of parameters
  float lowerMeanCutoff;  // Cutoff for the excluding dark holes
  float upperMeanCutoff;  // Cutoff for excluding light holes
  float SDcutoff;         // Cutoff for excluding highly variable holes
  float blackFracCutoff;   // Cutoff for excluding holes with high # of black outliers
  BOOL showExcluded;      // Show the excluded holes
  int layoutType;         // How to order nav points
};

enum SetStateTypes {STATE_NONE = 0, STATE_IMAGING, STATE_MAP_ACQUIRE};

class CNavHelper
{
public:
  CNavHelper(void);
  ~CNavHelper(void);
  int RealignToItem(CMapDrawItem * item, BOOL restoreState, float resetISalignCrit,
    int maxNumResetAlign, int leaveZeroIS);
  float PointSegmentDistance(float ptX, float ptY, float x1, float y1, float x2, float y2);
  GetMember(int, Realigning)
  GetSetMember(float, RImaximumIS)
  GetSetMember(float, MaxMarginNeeded)
  GetSetMember(float, MinMarginNeeded)
  GetSetMember(float, MinMarginWanted)
  GetSetMember(int, TypeOfSavedState)
  GetMember(BOOL, AcquiringDual)
  GetSetMember(BOOL, SearchRotAlign)
  GetSetMember(float, RotAlignRange)
  GetSetMember(float, RotAlignCenter)
  GetSetMember(BOOL, ConvertMaps)
  GetSetMember(BOOL, LoadMapsUnbinned)
  GetSetMember(BOOL, WriteNavAsXML);
  GetSetMember(BOOL, TryRealignScaling);
  GetSetMember(BOOL, PlusMinusRIScaling);
  GetSetMember(int, RealignTestOptions);
  GetSetMember(int, AutoBacklashNewMap);
  GetSetMember(float, AutoBacklashMinField);
  GetSetMember(int, PointLabelDrawThresh);
  GetSetMember(BOOL, UseLabelInFilenames);
  GetSetMember(int, EnableMultiShot);
  bool MultipleHolesAreSelected() {return (mMultiShotParams.inHoleOrMultiHole & MULTI_HOLES) &&
    ((mMultiShotParams.useCustomHoles && mMultiShotParams.customMagIndex > 0) || 
    (!mMultiShotParams.useCustomHoles && mMultiShotParams.holeMagIndex > 0));};
  MultiShotParams *GetMultiShotParams() {return &mMultiShotParams;};
  HoleFinderParams *GetHoleFinderParams() { return &mHoleFinderParams; };
  GetSetMember(float, DistWeightThresh);
  GetSetMember(float, RImaxLMfield);
  GetSetMember(int, RIskipCenTimeCrit);
  GetSetMember(float, RIskipCenErrorCrit);
  GetSetMember(float, RIweightSigma);
  GetSetMember(BOOL, RIuseBeamOffsets);
  SetMember(int, ContinuousRealign);
  GetSetMember(float, GridGroupSize);
  GetSetMember(BOOL, DivideIntoGroups);
  GetSetMember(bool, EditReminderPrinted);
  GetSetMember(float, RITiltTolerance);
  GetSetMember(float, RIdefocusChangeLimit);
  GetSetMember(BOOL, RIuseCurrentLDparams);
  GetSetMember(BOOL, CollapseGroups);
  SetMember(int, ExtDrawnOnID);
  GetSetMember(BOOL, SkipMontFitDlgs);
  GetSetMember(bool, DoingMultipleFiles);
  GetSetMember(float, HFtargetDiamPix);
  GetSetMember(int, HFretainFFTs);
  GetSetMember(int, HFminNumForTemplate);
  GetSetMember(float, HFfractionToAverage);
  GetSetMember(float, HFmaxDiamErrFrac);
  GetSetMember(float, HFavgOutlieCrit);
  GetSetMember(float, HFfinalPosOutlieCrit);
  GetSetMember(float, HFfinalNegOutlieCrit);
  GetSetMember(float, HFfinalOutlieRadFrac);
  GetSetMember(float, HFpcToPcSameFrac);
  GetSetMember(float, HFpcToFullSameFrac);
  GetSetMember(float, HFsubstOverlapDistFrac);
  GetSetMember(float, HFusePieceEdgeDistFrac);
  GetSetMember(float, HFaddOverlapFrac);
  GetSetMember(BOOL, MHCenableMultiDisplay);
  GetSetMember(int, MHCcombineType);
  GetSetMember(BOOL, SkipAstigAdjustment);

  void ForceCenterRealign() {mCenterSkipArray.RemoveAll();};

  CStateDlg *mStateDlg;
  CArray<StateParams *, StateParams *> *GetStateArray () {return &mStateArray;};
  CNavRotAlignDlg *mRotAlignDlg;
  CMultiShotDlg *mMultiShotDlg;
  CHoleFinderDlg *mHoleFinderDlg;
  HoleFinder *mFindHoles;
  CMultiHoleCombiner *mCombineHoles;
  CMultiCombinerDlg *mMultiCombinerDlg;

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
  MultiShotParams mMultiShotParams;
  int mEnableMultiShot;
  HoleFinderParams mHoleFinderParams;

  std::vector<int> mPieceSavedAt;  // Sections numbers for existing pieces in montage
  int mSecondRoundID;           // Map ID of map itself for 2nd round alignment
  KImageStore *mMapStore;       // Pointer to file to use
  MontParam *mMapMontP;         // Pointer to montage params to use
  MontParam mMapMontParam;      // Our set of params for non-open file
  int mRIitemInd;               // Index of item being aligned to
  int mRImapID;                 // Map ID of first round map aligned to
  CMapDrawItem *mRIdrawnTargetItem; // True target drawn on second-round map
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
  BOOL mRIuseCurrentLDparams;   // Flag to use current low dose parameters in realign
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
  BOOL mConvertMaps;                 // Flag that Navigator should convert maps to byte
  BOOL mLoadMapsUnbinned;            // Flag that Navigator should load maps unbinned
  BOOL mTryRealignScaling;           // Flag to do realign with scaling centered image
  BOOL mPlusMinusRIScaling;          // Flag to do a search for scaling on both sides of 1
  BOOL mWriteNavAsXML;               // Flag to write Nav files as XML
  int mRealignTestOptions;           // For testing backlash and mont stage error
  int mAutoBacklashNewMap;           // Whether to do backlash for new maps
  float mAutoBacklashMinField;       // FOV for doing backlash routine
  int mPointLabelDrawThresh;         // Threshold group size for drawing point labels
  BOOL mUseLabelInFilenames;         // Flag to use the item label when generating names

  WINDOWPLACEMENT mRotAlignPlace;
  WINDOWPLACEMENT mMultiShotPlace;
  WINDOWPLACEMENT mMultiCombinerPlace;
  WINDOWPLACEMENT mHoleFinderPlace;
  double mRIdefocusOffsetSet;   // Defocus offset set in realign to item
  int mRIalphaSet;              // Alpha value set in realign to item
  int mRIalphaSaved;            // Alpha value that it was before
  float mRIbeamShiftSetX;       // Beam shift and tilt imposed in realign to item
  float mRIbeamShiftSetY;
  float mRIbeamTiltSetX;
  float mRIbeamTiltSetY;
  float mRInetViewShiftX;       // Stage equivalent of underlying view shift
  float mRInetViewShiftY;
  float mRIviewShiftChangeX;    // Difference between original and current shift
  float mRIviewShiftChangeY;
  float mRIcalShiftX;           // The adjustment to be applied by AdjustAndMoveStage
  float mRIcalShiftY;
  int mRIconSetNum;
  bool mRIstayingInLD;
  float mRIresetISCritForAlign; // Criterion for doing a reset shift - realign operation
  int mRIresetISmaxNumAlign;    // Maximum times to do rest shift - realign
  int mRIresetISleaveZero;      // Whether to leave IS at zero
  int mRIresetISnumDone;        // Counter for number done
  bool mRIresetISneedsAlign;    // Whether the reset needs an alignment after it
  LowDoseParams mRIsavedLDparam;
  float mRIareaDefocusChange;   // Change in defocus offset when staying in low dose
  float mRIdefocusChangeLimit;  // Maximum defocus change for it to stay in LD
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
  BOOL mSkipMontFitDlgs;         // Setting in file properties dialog to skip dialogs
  bool mDoingMultipleFiles;      // Flag to avoid "no longer inherits" messages
  float mHFtargetDiamPix;        // Hole finder parameters: see holefinder source
  int mHFretainFFTs;
  int mHFminNumForTemplate;
  float mHFfractionToAverage;
  float mHFmaxDiamErrFrac;
  float mHFavgOutlieCrit;
  float mHFfinalPosOutlieCrit;
  float mHFfinalNegOutlieCrit;
  float mHFfinalOutlieRadFrac;
  float mHFpcToPcSameFrac;
  float mHFpcToFullSameFrac;
  float mHFsubstOverlapDistFrac;
  float mHFusePieceEdgeDistFrac;
  float mHFaddOverlapFrac;
  FloatVec mHFwidths;
  FloatVec mHFincrements;
  IntVec mHFnumCircles;
  int mMHCcombineType;           // MultiHoleCombine way to pick points (COMBINE_...)
  BOOL mMHCenableMultiDisplay;   // Option to show multi-shot on all before combining pts
  BOOL mSkipAstigAdjustment;     // Property to skip the astigmatism when adjusting for IS


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
  int SetToMapImagingState(CMapDrawItem * item, bool setCurFile, BOOL hideLDoff = FALSE);
  int RestoreFromMapState(void);
  void ChangeAllBufferRegistrations(int mapID, int fromReg, int toReg);
  CString NextAutoFilename(CString inStr, CString oldLabel = "", CString newLabel = "");
  int NewAcquireFile(int itemNum, int fileType, ScheduledFile *sched);
  int SetFileProperties(int itemNum, int fileType, ScheduledFile *sched, 
    bool fromFilePropButton, bool skipFitDlgs);
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
  int MakeDualMap(CMapDrawItem *item);
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
  void GetViewOffsets(CMapDrawItem * item, float & netShiftX, float & netShiftY, float & beamShiftX, float & beamShiftY, float & beamTiltX, float & beamTiltY, int area = VIEW_CONSET);
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
  static int TaskRealignBusy(void);
  void StartResetISorFinish(int magInd);
  void OpenMultishotDlg(void);
  WINDOWPLACEMENT *GetMultiShotPlacement(bool update);
  int RotateMultiShotVectors(MultiShotParams *params, float angle, bool custom);
  void OpenHoleFinder(void);
  WINDOWPLACEMENT *GetHoleFinderPlacement(void);
  void OpenMultiCombiner(void);
  WINDOWPLACEMENT *GetMultiCombinerPlacement();
  void SaveLDFocusPosition(bool saveIt, float & axisPos, BOOL & rotateAxis, int & axisRotation, 
    int & xOffset, int & yOffset, bool traceIt);
  void SetLDFocusPosition(int camIndex, float axisPos, BOOL rotateAxis, int axisRotation, 
    int xOffset, int yOffset, const char *descrip);
  int CheckTiltSeriesAngles(int paramInd, float start, float end, float bidir, CString &errMess);
  bool AnyMontageMapsInNavTable();
  int FindMapIDforReadInImage(CString filename, int secNum);
  int SetUserValue(CMapDrawItem *item, int number, CString &value);
  int GetUserValue(CMapDrawItem *item, int number, CString &value);
  bool ModifySubareaForOffset(int camera, int xOffset, int yOffset, int &left,
    int &top, int &right, int &bottom);
  void FindFocusPosForCurrentItem(StateParams & state, bool justLDstate);
  int RealignToDrawnOnMap(CMapDrawItem *item, BOOL restoreState);
  bool GetNumHolesFromParam(int &xnum, int &ynum);
  void GetHFscanVectors(FloatVec **widths, FloatVec **increments, IntVec **numCircles) 
  {*widths = &mHFwidths; *increments = &mHFincrements; *numCircles = &mHFnumCircles;};

};

