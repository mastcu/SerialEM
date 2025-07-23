#pragma once
#include "EMscope.h"
#include "AutoContouringDlg.h"
#include <map>

enum { MG_INVENTORY = 1, MG_LOW_MAG_MAP, MG_REALIGN_RELOADED, MG_RRG_MOVED_STAGE, 
  MG_RRG_TOOK_IMAGE, MG_LOAD_CART, MG_UNLOAD_CART, MG_USER_LOAD, MG_RUN_SEQUENCE,
  MG_RESTORING, MG_SEQ_LOAD, MG_REFINE_REALIGNED, MG_REFINE_MOVED, MG_REFINE_IMAGE
};
enum MultiGridActions {
  MGACT_REMOVE_OBJ_AP, MGACT_SET_CONDENSER_AP, MGACT_REPLACE_OBJ_AP, MGACT_REF_MOVE,
  MGACT_RESTORE_COND_AP, MGACT_LMM_STATE, MGACT_UNLOAD_GRID, MGACT_REF_IMAGE,
  MGACT_SURVEY_STAGE_MOVE, MGACT_SURVEY_IMAGE, MGACT_EUCENTRICITY, MGACT_LMM_CENTER_MOVE,
  MGACT_LOAD_GRID, MGACT_TAKE_LMM, MGACT_REALIGN_TO_LMM, MGACT_AUTOCONTOUR,
  MGACT_MMM_STATE, MGACT_TAKE_MMM, MGACT_FINAL_STATE, MGACT_FINAL_ACQ,
  MGACT_SETUP_MMM_FILES, MGACT_RESTORE_STATE, MGACT_LOW_DOSE_ON, MGACT_LOW_DOSE_OFF,
  MGACT_SET_ZHEIGHT, MGACT_MARKER_SHIFT, MGACT_FOCUS_POS, MGACT_RUN_MACRO,
  MGACT_REFINE_ALIGN, MGACT_RESTORE_C1_AP, MGACT_GRID_DONE
};

#define MAX_LOADER_SLOTS 13

#define MGSTAT_FLAG_LM_MAPPED   0x1
#define MGSTAT_FLAG_TOO_DARK    0x2
#define MGSTAT_FLAG_FAILED      0x4
#define MGSTAT_FLAG_MMM_DONE    0x8
#define MGSTAT_FLAG_ACQ_DONE   0x10
#define MGSTAT_FLAG_NEW_GRID   0x20

class CMultiGridDlg;

// Structures for saving per-grid parameters and enums for indexing the simple arrays
enum MShotIndexes {
  MS_spokeRad0, MS_spokeRad1, MS_numShots0, MS_numShots1, MS_numHoles0,
  MS_numHoles1, MS_doSecondRing, MS_doCenter, MS_inHoleOrMultiHole, MS_numHexRings,
  MS_skipCornersOf3x3, MS_origMagOfArray, MS_holeMagIndex, MS_ISXspacing0, MS_ISYspacing0,
  MS_ISXspacing1, MS_ISYspacing1, MS_ISXspacing2, MS_ISYspacing2, MS_xformFromMag,
  MS_xformToMag, MS_xformXpx, MS_xformXpy, MS_xformYpx, MS_xformYpy, MS_xformMinuteTime, 
  MS_noParam };
struct MGridMultiShotParams
{
  float values[MS_noParam];
};

enum HoleIndexes { HF_spacing, HF_diameter, HF_lowerMeanCutoff, HF_upperMeanCutoff, 
  HF_SDcutoff, HF_blackFracCutoff, HF_edgeDistCutoff, HF_hexagonalArray, HF_noParam };
struct MGridHoleFinderParams
{
  FloatVec sigmas;        // Set of sigmas and median iterations to try
  FloatVec thresholds;    // Set of thresholds to try
  float values[HF_noParam];   // The rest of the parameters
};

enum AutoContIndexes {AC_minSize, AC_maxSize, AC_relThreshold, AC_lowerMeanCutoff, 
  AC_upperMeanCutoff, AC_borderDistCutoff, AC_noParam };
struct MGridAutoContParams
{
  float values[AC_noParam];
};

enum GenParamIndexes {GP_disableAutoTrim, GP_erasePeriodicPeaks, GP_RIErasePeriodicPeaks,
  GEN_noParam};
struct MGridGeneralParams
{
  int values[GEN_noParam];
};

struct MGridAcqItemsParams
{
  NavAcqAction actions[NAA_MAX_ACTIONS];
  NavAcqParams params;
  int actOrder[NAA_MAX_ACTIONS + 1];
};

enum FocusPosParamIndexes { FP_focusAxisPos, FP_rotateAxis, FP_axisRotation,
  FP_focusXoffset, FP_focusYoffset, FP_noParam };
struct MGridFocusPosParams
{
  float values[FP_noParam];
};

class CMultiGridTasks
{
public:
  CMultiGridTasks();
  ~CMultiGridTasks();
  void Initialize();
  void InitOrClearSessionValues();
  int ReplaceBadCharacters(CString &str);
  CString RootnameFromUsername(JeolCartridgeData &jcd);
  GetSetMember(bool, AllowOptional);
  GetSetMember(bool, ReplaceSpaces);
  GetSetMember(bool, UseCaretToReplace);
  int GetInventory();
  int LoadOrUnloadGrid(int slot, int taskParam);
  void MultiGridNextTask(int param);
  int MultiGridBusy();
  void CleanupMultiGrid(int error);
  void StopMultiGrid();
  int MulGridSeqBusy();
  void CleanupMulGridSeq(int error);
  void StopMulGridSeq();
  void RestoreImposedParams();
  void ReopenMainLog(int idVal);
  void SuspendMulGridSeq();
  void CommonMulGridStop(bool suspend);
  void EndMulGridSeq();
  int ResumeMulGridSeq(int resumeType);
  void UserResumeMulGridSeq();
  void ExternalTaskError(CString &errStr);
  void MGActMessageBox(CString &errStr);
  int RealignReloadedGrid(CMapDrawItem *item, float expectedRot, bool moveInZ,
    float maxRotation, int transformNav, CString &errStr, int jcdInd = -1, bool needState = true);
  int LoadOrReloadMapIfNeeded(CMapDrawItem *item, int maxBin, CString &errStr);
  void CentroidsFromMeansAndPctls(IntVec &ixVec, IntVec &iyVec, FloatVec &xStage, FloatVec &yStage,
    FloatVec &meanVec, FloatVec &midFracs, FloatVec &rangeFracs, float fracThresh, int &meanIXcen, int &meanIYcen,
    float &meanSXcen, float &meanSYcen, int &pctlIXcen, int & pctlIYcen, float &pctlSXcen, float &pctlSYcen,
    int &numPcts, int &numAbove);
  void MaintainClosestFour(float delX, float delY, int pci, float radDist[4], int minInd[4],
    int &numClose);
  bool IsDuplicatePiece(int minInd, int bestInd[5], int startInd, int endInd);
  int MoveStageToTargetPiece();
  void AlignNewReloadImage();
  ScaleMat FindReloadTransform(float dxyOut[2], float &theta, float &residOut);
  void UndoOrRedoMarkerShift(bool redo);
  int AlignGridMapAcrossMags(CMapDrawItem *item, int setNum, float findNearX, float findNearY,
    float maxDiff, CString &errStr);
  void MoveStageForRefineMapAlign();
  void RefineRealignNextTask(int param);
  int StartGridRuns(int LMneedsLD, int MMMneedsLD, int finalNeedsLD, int finalCamera,
    bool undoneOnly);
  void AddToSeqForLMimaging(bool &apForLMM, bool &stateForLMM, int needsLD);
  void AddToSeqForReloadRealign(bool &apForLMM, bool &stateForLMM);
  void AddToSeqForRestoreFromLM(bool &apForLMM, bool &stateForLMM);
  int CheckStatesInRange(int *stateNums, CString *names, int numStates, CString mess);
  void DoNextSequenceAction(int resume);
  int SkipToAction(int mgAct);
  int SkipToNextGrid(CString &errStr);
  void FinishWithGrid();
  void UpdateGridStatusText();
  void OutputGridStartLine();
  CString FullGridFilePath(int gridInd, CString suffix);
  int GridIndexOfCurrentNavFile();
  void NextAutoFilenameIfNeeded(CString &str);
  void CloseMainLogOpenForGrid(const char *suffix);
  int OpenNewMontageFile(MontParam &montP, CString &str);
  int OpenFileForFinalAcq();
  int RealignToGridMap(int jcdInd, bool askIfSave, bool applyLimits, CString &errStr, bool needState = true);
  int PrepareAlignToGridMap(int jcdInd, bool askIfSave, CMapDrawItem **itemP, CString &errStr);
  int RefineGridMapAlignment(int jcdInd, int stateNum, bool askIfSave, CString &errStr);
  int ReloadGridMap(CMapDrawItem *item, int useBin, CString &errStr);
  int OpenNavFileIfNeeded(JeolCartridgeData &jcd);
  int MarkerShiftIndexForMag(int magInd, BaseMarkerShift &baseShift);
  int CheckShiftsForLMmode(int magInd);
  void RestoreState();
  void RestoreFromRefine();
  GetMember(int, DoingMultiGrid);
  GetMember(bool, DoingMulGridSeq);
  GetMember(bool, SuspendedMulGrid);
  GetSetMember(float, RRGMaxRotation);
  MultiGridParams *GetMultiGridParams() { return &mParams; };
  MontParam *GetGridMontParam();
  MontParam *GetMMMmontParam();
  void ChangeStatusFlag(int gridInd, b3dUInt32 flag, int state);
  void CopyAutoContGroups();
  void GridToMultiShotSettings(MGridMultiShotParams &mgParam);
  void GridToMultiShotSettings(MGridMultiShotParams &mgParam, MultiShotParams *msParams);
  void MultiShotToGridSettings(MGridMultiShotParams &mgParam);
  void GridToHoleFinderSettings(MGridHoleFinderParams &mgParam);
  void HoleFinderToGridSettings(MGridHoleFinderParams &mgParam);
  void GridToAutoContSettings(MGridAutoContParams &mgParam);
  void AutoContToGridSettings(MGridAutoContParams &mgParam);
  void GridToGeneralSettings(MGridGeneralParams &mgParam);
  void GeneralToGridSettings(MGridGeneralParams &mgParam);
  void GridToFocusPosSettings(MGridFocusPosParams &mgParam);
  void FocusPosToGridSettings(MGridFocusPosParams &mgParam);
  void TransformStoredVectors(MGridMultiShotParams &mgParam, ScaleMat mat);
  void ApplyVectorsIfSavedAndOK(int jcdInd);
  int LoadAllGridMaps(int startBuf, CString &errStr);
  void ReportRecParams(const char *where);
  bool GetGridMapLabel(int mapID, CString &value);
  int GetCurrentGridID();
  int SaveSessionFile(CString &errStr);
  void SaveSessionFileWarnIfError();
  void ClearSession(bool keepAcqParams = false);
  void TurnOffSubset();
  int LoadSessionFile(bool useLast, CString &errStr);
  void UpdateDialogForSessionFile();
  void IdentifyGridOnStage(int stageID, int &stageInd);
  GetSetMember(float, PctStatMidCrit);
  GetSetMember(float, PctStatRangeCrit);
  GetSetMember(CString, WorkingDir);
  GetSetMember(CString, LastSessionFile);
  SetMember(CString, SessionFilename);
  GetMember(int, NamesLocked);
  GetSetMember(CString, Prefix);
  GetMember(int, UnloadErrorOK);
  GetMember(bool, StartedNavAcquire);
  GetSetMember(int, NumMMMstateCombos);
  GetSetMember(int, NumFinalStateCombos);
  GetMember(BOOL, AppendNames);
  GetMember(BOOL, UseSubdirs);
  SetMember(int, LoadedGridIsAligned);
  GetSetMember(float, RRGmaxCenShift);
  GetSetMember(bool, AdocChanged);
  GetSetMember(BOOL, SkipGridRealign);
  GetSetMember(float, RRGShiftLimitForXform);
  GetSetMember(float, RRGRotLimitForXform);
  GetSetMember(float, RRGResidLimitForXform);
  GetMember(float, ReferenceCounts);
  GetMember(double, RemainingTime);
  GetMember(int, CurrentGrid);
  GetMember(int, NumGridsToRun);
  IntVec *GetLastDfltRunInds() { return &mLastDfltRunInds; };
  GetSetMember(int, LastNumSlots);
  GetSetMember(bool, UseCustomOrder);
  GetSetMember(BOOL, ShowRefineAfterRealign);
  GetSetMember(float, RefineMinField);
  GetMember(int, LMMmagIndex);
  GetSetMember(float, MaxRefineShiftDiff);
  GetMember(bool, UseTwoJeolCondAp);
  GetMember(int, SingleCondenserAp);
  GetMember(double, RunStartTime);
  GetSetMember(int, PostLoadDelay);
  int *GetMontSetupConsetSizes() { return &mMontSetupConsetSizes[0]; };
  IntVec *GetCustomRunDlgInds() {return &mCustomRunDlgInds ; };
  bool WasStoppedInNavRun() { return mStoppedInNavRun && mSuspendedMulGrid; };
  bool WasStoppedInLMMont() { return mStoppedInLMMont && mSuspendedMulGrid; };
  void SetExtErrorOccurred(int inVal);
  bool RunningExternalTask() { return mDoingMulGridSeq && !mInternalError; };
  int *GetAutoContGroups() { return mHaveAutoContGroups ? &mAutoContGroups[0] : NULL; };
  CArray<MGridMultiShotParams, MGridMultiShotParams> *GetMGMShotParamArray() {return &mMGMShotParamArray ; };
  CArray<MGridHoleFinderParams, MGridHoleFinderParams> *GetMGHoleParamArray() { return &mMGHoleParamArray; };
  CArray<MGridAutoContParams, MGridAutoContParams> *GetMGAutoContParamArray() { return &mMGAutoContParamArray; };
  CArray<MGridGeneralParams, MGridGeneralParams> *GetMGGeneralParams() { return &mMGGeneralParamArray; };
  CArray<MGridFocusPosParams, MGridFocusPosParams> *GetMGFocusPosParams() { return &mMGFocusPosParamArray; };
  CArray<MGridAcqItemsParams, MGridAcqItemsParams> *GetMGAcqItemsParamArray() { return &mMGAcqItemsParamArray; };
private:
  CSerialEMApp *mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  CNavHelper *mNavHelper;
  CNavigatorDlg *mNavigator;
  StageMoveInfo mMoveInfo;
  CMultiGridDlg *mMGdlg;
  float mRRGMaxRotation;      // Property for default maximum rotation
  bool mSingleGridMode;       // Flag for no autoloader
  bool mAllowOptional;        // Flag to allow the optional characters, bad in shells
  bool mReplaceSpaces;        // Flag to replace spaces
  bool mUseCaretToReplace;    // Flag t use caret insetad of @ when replacing spaces
  int mDoingMultiGrid;        // General flag for one of the routines running
  bool mDoingMulGridSeq;      // Flag for multiple grid sequence running
  bool mSuspendedMulGrid;     // Flag in a resumable stopped state
  bool mEndWhenGridDone;      // Flag to stop at end of grid
  bool mStoppedAtGridEnd;     // Flag for that kind of stop
  bool mStoppedInNavRun;      // Flag that stopping/suspend was in a Navigator acquire
  bool mStoppedInLMMont;      // Flag that stopping/suspend was in the LM montage
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;
  CArray<JeolCartridgeData, JeolCartridgeData> mCartCopy;
  CArray<StateParams *, StateParams *> *mStateArray;
  CArray<MGridMultiShotParams, MGridMultiShotParams> mMGMShotParamArray;
  CArray<MGridHoleFinderParams, MGridHoleFinderParams> mMGHoleParamArray;
  CArray<MGridAutoContParams, MGridAutoContParams> mMGAutoContParamArray;
  CArray<MGridGeneralParams, MGridGeneralParams> mMGGeneralParamArray;
  CArray<MGridFocusPosParams, MGridFocusPosParams> mMGFocusPosParamArray;
  CArray<MGridAcqItemsParams, MGridAcqItemsParams> mMGAcqItemsParamArray;
  MultiGridParams mParams;    // THE master parameters
  MontParam mGridMontParam;   // Managed copy of montage params for grid mont
  MontParam mMMMmontParam;    // Managed copy of montage params for MMM
  MontParam mSaveMasterMont;  // The programs master montage parameters, saved for restore
  BOOL mInitializedGridMont;  // Flag that LM mont params were initialized by copy or read
  BOOL mInitializedMMMmont;   // Flag that MM mont params were initialized by copy or read
  int mMontSetupConsetSizes[4];   // Frame sizes in conset when LMM or MMM montage set up
  BOOL mSavedLowDose;         // Saved state of low dose at start
  MGridMultiShotParams *mSavedMultiShot;
  MGridHoleFinderParams *mSavedHoleFinder;
  MGridAutoContParams *mSavedAutoCont;
  MGridGeneralParams *mSavedGeneralParams;
  MGridFocusPosParams *mSavedFocusPos;
  CString mSavedDirForFrames; // User's dir for frame saving with final acquire camera
  int mCamNumForFrameDir;     // Camera for which that was saved
  CString mSavedFrameBaseName;
  CString mSavedNumberedFolder;
  CString mSavedNumberedPrefix;
  int mSavedLastFrameNum;
  int mLastSetFrameNum;
  int mCamNumForFocus;
  bool mAdocChanged;          // Flag that .adoc/ cartinfo was changed
  IntVec mCustomRunDlgInds;
  bool mUseCustomOrder;       // Flag that user has set custom order
  IntVec mLastDfltRunInds;    // Default dialog run indexes when dialog was closed
  int mLastNumSlots;          // And total number of slots then
  std::map<int, std::string> mMultiLoadLabelMap;

  bool mStartedLongOp;        // Flags for actions that were started
  bool mMovedAperture;
  bool mDoingEucentricity;
  bool mDoingMontage;
  bool mAutoContouring;
  bool mRunningMacro;
  bool mRestoringOnError;     // Flag tht it is doing the aperture restore (generally on error)
  bool mSettingUpNavFiles;    // Flag that it is setting up polygon montages files
  bool mStartedNavAcquire;    // Flag that Nav acquire was started
  bool mInternalError;        // Flag to prevent reentrancy on message box errors here
  int *mActiveCameraList;

  
  // RRG = Realign Reloaded Grid
  KImageStore *mRRGstore;        // Map file
  int mRRGcurStore;              // Pre-existing current store
  MontParam mRRGmontParam;       // Montage param for map
  CMapDrawItem *mRRGitem;        // Map item
  int mRRGpieceSec[5];           // Section numbers in file of each piece
  float mRRGxStages[5], mRRGyStages[5];     // Existing stage positions
  float mRRGnewXstage[5], mRRGnewYstage[5]; // Newly found stage positions
  float mRRGexpectedRot;         // Expected rotation, 0 or big
  float mRRGshiftX, mRRGshiftY;  // Shift to apply after rotation transform
  int mRRGcurDirection;          // Current direction: -1 or 0 for center, 1-4 peripheral 
  ScaleMat mRRGrotMat;           // Rotation matrix for current rotation value
  BOOL mRRGdidSaveState;         // Flag if saved state
  float mRRGangleRange;          // Angle range (2x max) for rotation searches
  int mRRGtransformNav;          // 1 to transform nav items, 2 to skip if above limits
  float mRRGbacklashX;           // Backlash to use, from map
  float mRRGbacklashY;
  float mRRGmapZvalue;           // Z to move to from the montage adoc
  float mRRGmaxCenShift;         // Maximum allowed shift at center of grid
  float mRRGmaxInitShift;        // Actual shift allowed in initial correlation
  int mRRGjcdIndex;              // Index in table so multishot vectors can be rotated
  bool mBigRotation;             // Flag if a big rotation is expected
  int mMapBuf;                   // Buffer the map is in
  int mMapCenX, mMapCenY;        // Center coordinate of center piece in loaded map
  int mLoadedGridIsAligned;      // 1 if it is aligned, 0 if not, -1 if it should be asked
  bool mRRGWasAboveXformLimit;   // Flag it wasnt aligned because above limit
  float mRRGShiftLimitForXform;  // Limit to shift in transformation - redo once if over
  float mRRGRotLimitForXform;    // Limit to rotation in transformation
  float mRRGResidLimitForXform;  // Limit to residual in transformation in microns
  int mRefineSetNum;             // Image type for refine alignment
  CMapDrawItem *mRefineItem;     // Map item for refine
  int mRefineStepIndex;          // Index of point being done
  float mMaxRefineDiff;          // Do not apply refine if minimum difference exceeds this
  BOOL mShowRefineAfterRealign;  // Flag to hide the options for this
  float mRefineMinField;         // Minimum field size in microns
  int mRefineUsedLDorState;      // 1 if turned on LD, -1 if set state
  float mMaxSizeForRefinePolys;  // Maximum linear extent of polygons for refine align
  float mMaxRefineShiftDiff;     // Maximum shift difference when running multigrid

  int mInitialObjApSize;         // Initial size of objective aperture
  int mInitialCondApSize;        // Initial size of condenser aperture
  int mInitialC1CondSize;        // Initial state of C1 aperture if any
  bool mReinsertObjAp;           // Flag that objective aperture needs to be reinserted
  int mRestoreCondAp;            // Flag that condenser aperture needs to be reinserted
  bool mUseTwoJeolCondAp;        // Scope-dependent flag for whether to use & show C1&C2
  int mSingleCondenserAp;         // Number of single aperture if there is just one
  ShortVec mJcdIndsToRun;        // cartridge info indexes of grids to run
  ShortVec mSlotNumsToRun;       // Slot number of cart info +1 of ones to run
  bool mUsingHoleFinder;         // Flag that hole finder is going to be used
  bool mUsingAutoContour;        // Flag that autocontouring will be done
  bool mUsingMultishot;          // Flag that multishot is going to be used
  int mNumGridsToRun;            // Number of grids to run
  ShortVec mActSequence;         // Sequence of actions for all grids
  int mSeqIndex;                 // Index in the action sequence
  float mReferenceCounts;        // Counts in blank image at LMM acquire conditions
  int mSurveyIndex;              // Index to survey position for eucentricity
  int mCurrentGrid;              // Index of current grid in list of ones to run
  bool mStateWasSet;             // Flag that a state was set
  float mEucenXtrials[5];        // Candidate positions for eucentricty 
  float mEucenYtrials[5];
  float mPctMidFracs[4];         // Percentile mid and range fractions at trial positions
  float mPctRangeFracs[4];
  bool mMovedToFirstTrial;       // Flag if already moved to first trial position
  float mPctStatMidCrit;         // Frac of ref counts that midpoint of pctls must exceed
  float mPctStatRangeCrit;       // Frac of ref counts that pange of pctls must exceed
  float mPctUsableFrac;          // Frac of patches passing criteria for piece to be usable
  float mPctRightAwayFrac;       // Frac passing criteria to stop trials and use a position
  int mPctPatchSize;             // Patch size being used for the pctile stats
  bool mDidEucentricity;         // Flag that eucentricity was done
  float mMinFracOfPiecesUsable;  // Criterion fraction of pieces usable for grid to be OK
  CString mExternalFailedStr;    // Error string from SEMMessageBox on external failure
  int mExtErrorOccurred;         // Flag from ErrorOccurred including 0 for user stop
  CString mWorkingDir;           // Possibly locked in values for working directory
  CString mPrefix;               // for name prefix
  BOOL mAppendNames;             // For whether to append names
  BOOL mUseSubdirs;              // For whether to use subdirectories
  CString mSessionFilename;      // Current session filename
  CString mLastSessionFile;      // Name of last session file, for browsing or script load
  BOOL mBackedUpSessionFile;     // Flag tht file was backed up
  BOOL mBackedUpAcqItemsFile;    // Flag file with per-grid acquire parameters backed up
  int mNamesLocked;              // Flag that all those things were locked in for session
  CString mMainLogName;          // Name of main log that is returned to
  int mLMMmagIndex;              // Mag index being used for LMM
  int mUnloadErrorOK;            // Flag that load/unload may give error because unneeded
  int mFailureRetry;             // retry count for failed steps that can be retried
  int mLMMusedStateNum;          // Number and name of state used for grid maps
  CString mLMMusedStateName;
  int mLMMneedsLowDose;          // Flag that LM map needs low dose turned on or off
  int mAutoContGroups[MAX_AUTOCONT_GROUPS];
  bool mHaveAutoContGroups;      // Flag that groups were saved for autocontouring
  bool mOpeningForFinalFailed;   // Flag that a file was needed and opening it failed
  int mNumMMMstateCombos;        // Number of combo boxes to show for MMM
  int mNumFinalStateCombos;      // NUmber of combo boxes to show for final
  bool mSkipEucentricity;        // Flag to skip rough eucentricty for low-tilt stage
  BOOL mSkipGridRealign;         // Property to skip realign after reload
  int mRealignIteration;         // Iteration number if it had to repeat realign
  double mRunStartTime;          // Time that run started
  double mRemainingTime;         // Estimate of remaining time after current grid is done
  int mSkipMarkerShifts;         // User response to query: 1 to skip or -1 to use markers
  bool mNoMarkerShifts;          // Flag to skip in the run for whatever reason
  int mPostLoadDelay;            // Delay time in seconds after loading/unloading grid
};

