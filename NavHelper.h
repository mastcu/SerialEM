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
class CComaVsISCalDlg;
class CAutoContouringDlg;
class CMultiGridDlg;
class CMGSettingsManagerDlg;

#define MULTI_IN_HOLE       0x1
#define MULTI_HOLES         0x2
#define MULTI_TEST_IMAGE    0x4
#define MULTI_TEST_COMA     0x8
#define MULTI_FORCE_REGULAR 0x10
#define MULTI_FORCE_CUSTOM  0x20
#define MULTI_NO_3X3_CROSS  0x40
#define MULTI_DO_CROSS_3X3  0x80
#define MULTI_NO_HEX_CENTER 0x100
#define MULTI_DO_HEX_CENTER 0x200

#define REALI2ITEM_JUST_MOVE   0x1

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
  int doEarlyReturn;     // 1 for early return on last, 2 for all, 3 for full sum on first
  int numEarlyFrames;    // Number of frames in early return
  BOOL saveRecord;       // Whether to save Record shot
  BOOL adjustBeamTilt;   // Whether to adjust beam tilt if possible
  int inHoleOrMultiHole; // 1 to do pattern in hole, 2 to do multi-hole, 3 to do both
  int holeMagIndex[2];   // Mag at which hole positions are measured for regular and hex
  BOOL useCustomHoles;   // Use the list of custom holes
  BOOL doHexArray;       // Flag to do hexagonal array
  float holeDelayFactor; // Factor by which to increase regular IS delay between holes
  double holeISXspacing[2];  // Hole spacing in each direction
  double holeISYspacing[2];
  double hexISXspacing[3];  // Basic vectors for hex grid pattern - keep separate
  double hexISYspacing[3];
  float tiltOfHoleArray[2];    // Tilt angle at which regular and hex array defined
  float holeFinderAngle;    // Angle found in hole finder associated with regular array
  int numHoles[2];       // Number of holes in each direction
  int numHexRings;       // NUmber of rings of hex
  BOOL skipCornersOf3x3; // Take cross pattern when it is 3x3
  BOOL skipHexCenter;    // Drop center hole of hex pattern
  int customMagIndex;    // Mag index at which custom holes defined
  float tiltOfCustomHoles;  // Tilt angle at which custom holes defined
  FloatVec customHoleX;  // For custom holes, list of hole IS relative
  FloatVec customHoleY;  // to item point
  FloatVec customDefocus;  // Optional defocus change to impose
  float beamDiam;        // Beam diameter: display only
  BOOL useIllumArea;     // Whether to use illuminated area for drawing diameter
  int stepAdjOtherMag;   // Parameters in Setp to and Adjust IS: other magnification
  int stepAdjLDarea;     // Low dose area
  BOOL stepAdjSetDefOff; // Whether to change defocus offset
  int stepAdjDefOffset;  // Defocus offset to use
  int stepAdjWhichMag;   // Which kind of mag to use: current, recorded at, or other
  BOOL stepAdjTakeImage; // Flag to take an image of current area
  BOOL stepAdjSetPrevExp;  // Flag to set Preview exposure
  float stepAdjPrevExp;  // Exposure to use
  BOOL doAutoAdjustment; // Flag to do autoamtic adjustment if conditions allow it
  int autoAdjMethod;     // 0 for xcorr, 1 for hole find & center
  float autoAdjHoleSize; // Hole size needed either way
  float autoAdjLimitFrac;  // Limit to shift as fraction of hole diameter 
  int origMagOfArray[2]; // Negative of mag when defined, positive if adjusted
  int origMagOfCustom;
  int xformFromMag;      // Mag that adjusting transform is from and to
  int xformToMag;
  ScaleMat adjustingXform;  // Transform for adjustment
  int xformMinuteTime;   // Minute time stamp when it was last done

  // Runtime for undo capability: values to restore
  int canUndoRectOrHex;    // 0 if not, 1 for regular, 2 for hex
  int prevHoleMag;
  int prevOrigMag;
  int prevXformFromMag;
  int prevXformToMag;
  double prevXspacing[3];
  double prevYspacing[3];
  ScaleMat prevAdjXform;
  int prevMinuteTime;
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
  float blackFracCutoff;  // Cutoff for excluding holes with high # of black outliers
  float edgeDistCutoff;   // Cutoff for distance from edge of convex hull
  BOOL showExcluded;      // Show the excluded holes
  int layoutType;         // How to order nav points
  BOOL hexagonalArray;    // Flag for hex grid
  float hexDiameter;      // Set diameter and spacing for hex
  float hexSpacing;
  BOOL useHexDiagonals;   // Flag to select best 1/3 of holes
};

struct AutoContourParams
{
  int targetSizePixels;    // Target for reduction in pixels
  float targetPixSizeUm;   // Target pixel size to reduce to
  int usePixSize;          // Flag to reduce to pixel size
  float minSize;           // Minimum size in um for autocontouring
  float maxSize;           // Maximum size in um
  float relThreshold;      // Relative threshold value
  float absThreshold;      // Absolute threshold
  int useAbsThresh;        // Flag to use absolute threshold
  int numGroups;           // Number of groups to split into
  int groupByMean;         // Flag to group by mean value, not size
  float lowerMeanCutoff;  // Cutoff for the excluding dark squares
  float upperMeanCutoff;  // Cutoff for excluding light squares
  float minSizeCutoff;    // Cutoff for excluding ones too small
  float SDcutoff;         // Cutoff for excluding highly variable squares
  float irregularCutoff;  // Cutoff for excluding based on irregularity
  float borderDistCutoff; // Cutoff for excluding based on distance from border
  BOOL useCurrentPolygon; // Find contours within the current polygon if any
};

struct BaseMarkerShift
{
  float shiftX;           // Shift that was applied
  float shiftY;
  int fromMagInd;         // Mag of Nav item or map it was marked on
  int toMagInd;           // Mag of image with marker point
};

// Structure for an acquisition action (task)
struct NavAcqAction {
  CString name;           // Label for check box
  unsigned int flags;     // Flags for hiding, has setup button
  int timingType;         // Type of timing to use
  int everyNitems;        // Do every N items, 1, 2, etc
  int minutes;            // Minutes between doing it
  float distance;         // Minimum distance between places where it is done
  CString labelOrNote;    // Text to match at start
  int timeLastDone;       // Minute time stamp
  float lastDoneAtX;      // Location where done
  float lastDoneAtY;
};

// Structure for template align and realign to item params
struct NavAlignParams {
  CString templateLabel;     // Label for template map
  int loadAndKeepBuf;        // Buffer to save it in
  float maxAlignShift;       //  Maximum alignments shift
  int maxNumResetIS;         // Maximum number of reset shifts: can be negative to retain
  float resetISthresh;       // Threshold IS for resetting
  BOOL leaveISatZero;        // Flag to leave IS at zero
  float scaledAliMaxRot;     // Maximum rotation in realign to scaled map
  float scaledAliPctChg;     // Maximum percent size change in realign to scaled
  float scaledAliExtraFOV;   // Extra A fields of View to add
  int scaledAliLoadBuf;      // Buffer to keep map in
  BOOL applyInteractive;     // Flag to apply when run R2I from menu or button
};

// BEWARE.  Nav Actions are the "other tasks" in user terminology

// Add new default definition flags to "navActions[index].flags |=" in ParameterIO.cpp
#define NAA_FLAG_HAS_SETUP    (1 << 0)
#define NAA_FLAG_ALWAYS_HIDE  (1 << 1)
#define NAA_FLAG_RUN_IT       (1 << 2)
#define NAA_FLAG_HIDE_IT      (1 << 3)
#define NAA_FLAG_AFTER_ITEM   (1 << 4)
#define NAA_FLAG_OTHER_SITE   (1 << 5)
#define NAA_FLAG_MATCH_NOTE   (1 << 6)
#define NAA_FLAG_ONLY_BEFORE  (1 << 7)
#define NAA_FLAG_EVERYN_ONLY  (1 << 8)
#define NAA_FLAG_ANY_SITE_OK  (1 << 9)
#define NAA_FLAG_HERE_ONLY    (1 << 10)

#define NAA_MAX_ACTIONS    25
#define NAA_NUM_EXTRA_ACTS  6
#define DOING_ACTION(a) ((mAcqActions[a].flags & NAA_FLAG_RUN_IT) ? 1 : 0)
#define SET_ACTION(p, a, s) setOrClearFlags(&p[a].flags, NAA_FLAG_RUN_IT, (s) ? 1 : 0);
#define NAA_MACRO_AT_START -1

enum {NAA_EVERY_N_ITEMS = 0, NAA_GROUP_START, NAA_GROUP_END, NAA_AFTER_TIME, 
  NAA_IF_SEPARATED};

enum {
  NAACT_ROUGH_EUCEN, NAACT_CEN_BEAM, NAACT_REALIGN_ITEM, NAACT_COOK_SPEC,
  NAACT_FINE_EUCEN, NAACT_EUCEN_BY_FOCUS, NAACT_AUTOFOCUS, NAACT_COMA_FREE,
  NAACT_ASTIGMATISM, NAACT_CONDITION_VPP, NAACT_HW_DARK_REF, NAACT_WAIT_DRIFT,
  NAACT_ALIGN_TEMPLATE, NAACT_REFINE_ZLP, NAACT_RUN_PREMACRO, NAACT_RUN_POSTMACRO,
  NAACT_FLASH_FEG, NAACT_CHECK_DEWARS, NAACT_HOLE_FINDER, NAACT_RUN_EX_MACRO,
  NAACT_EX_CEN_BEAM, NAACT_EX_ALIGN_TEMPLATE, NAACT_EX_RESERVED1, NAACT_EX_RESERVED2,
  NAACT_EX_RESERVED3, // Add navActions here
  ACQ_FIND_SETUP_NEXT, ACQ_OPEN_FILE, ACQ_ACQUIRE, ACQ_DO_MULTISHOT, ACQ_RUN_MACRO,
  ACQ_START_TS, ACQ_MOVE_TO_AREA, ACQ_BACKLASH, ACQ_MOVE_ELSEWHERE, ACQ_RELAX_STAGE,
  ACQ_MAX_STEPS // End with this
};

enum SetStateTypes {STATE_NONE = 0, STATE_IMAGING, STATE_MAP_ACQUIRE};

class CNavHelper
{
public:
  CNavHelper(void);
  ~CNavHelper(void);
  int RealignToItem(CMapDrawItem * item, BOOL restoreState, float resetISalignCrit,
    int maxNumResetAlign, int leaveZeroIS, int realiFlags, int setForScaled);
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
  AutoContourParams *GetAutocontourParams() { return &mAutoContourParams; };
  NavAlignParams *GetNavAlignParams() {return &mNavAlignParams;};
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
  GetSetMember(BOOL, ShowTableIndexes);
  SetMember(int, ExtDrawnOnID);
  GetSetMember(BOOL, SkipMontFitDlgs);
  GetSetMember(int, DoingMultipleFiles);
  GetSetMember(int, DoingMultiGridFiles);
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
  GetSetMember(BOOL, MHCturnOffOutsidePoly);
  GetSetMember(int, MHCdelOrTurnOffIfFew);
  GetSetMember(int, MHCthreshNumHoles);
  GetSetMember(BOOL, MHCskipAveragingPos);
  GetSetMember(int, SkipAstigAdjustment);
  GetSetMember(int, CurAcqParamIndex);
  GetMember(int, NumAcqActions);
  GetMember(int, RIconSetNum);
  GetMember(bool, RIstayingInLD);
  GetMember(bool, SettingState);
  GetSetMember(int, OKtoUseHoleVectors);
  GetSetMember(int, MarkerShiftSaveType);
  GetSetMember(int, MarkerShiftApplyWhich);
  GetSetMember(BOOL, ReverseAutocontColors);
  GetSetMember(BOOL, KeepColorsForPolygons);
  GetSetMember(float, MaxMontReuseWaste);
  SetMember(bool, RISkipNextZMove);
  GetSetMember(BOOL, RIErasePeriodicPeaks);
  GetSetMember(float, ScaledAliDfltMaxRot);
  GetSetMember(float, ScaledAliDfltPctChg);
  float GetScaledRealignPctChg() { return mNavAlignParams.scaledAliPctChg < 0 ? mScaledAliDfltPctChg : mNavAlignParams.scaledAliPctChg; };
  float GetScaledRealignMaxRot() { return mNavAlignParams.scaledAliMaxRot < 0 ? mScaledAliDfltMaxRot : mNavAlignParams.scaledAliMaxRot; };
  GetSetMember(float, RISkipItemPosMinField);
  GetSetMember(BOOL, ShowStateNumbers);
  SetMember(CString, FirstMontFilename);
  GetMember(bool, OpenedMultiGrid);
  GetSetMember(float, MinAutoAdjHexRatio);
  GetSetMember(float, MinAutoAdjSquareRatio);
  GetSetMember(float, MaxAutoAdjHoleRatio);
  GetSetMember(int, MinAutoAdjCropSize);
  IntVec *GetExtraTaskList() { return &mExtraTaskList; };
  float *GetLastUsedHoleISvecs() {return &mLastUsedHoleISvecs[0] ; };


  int *GetAcqActDefaultOrder() { return &mAcqActDefaultOrder[0]; };
  int *GetAcqActCurrentOrder(int which) { return &mAcqActCurrentOrder[which][0]; };
  NavAcqAction *GetAcqActions(int which) {return &mAllAcqActions[which][0] ; };

  void ForceCenterRealign() {mCenterSkipArray.RemoveAll();};

  CStateDlg *mStateDlg;
  CArray<StateParams *, StateParams *> *GetStateArray () {return &mStateArray;};
  CArray<BaseMarkerShift, BaseMarkerShift> *GetMarkerShiftArray() { return &mMarkerShiftArray; };
  CNavRotAlignDlg *mRotAlignDlg;
  CMultiShotDlg *mMultiShotDlg;
  CHoleFinderDlg *mHoleFinderDlg;
  HoleFinder *mFindHoles;
  CMultiHoleCombiner *mCombineHoles;
  CMultiCombinerDlg *mMultiCombinerDlg;
  CComaVsISCalDlg *mComaVsISCalDlg;
  CAutoContouringDlg *mAutoContouringDlg;
  CMultiGridDlg *mMultiGridDlg;
  CMGSettingsManagerDlg *mMGSettingsDlg;

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
  MapItemArray *mItemArray;
  CArray<ScheduledFile *, ScheduledFile *> *mGroupFiles;
  CArray<FileOptions *, FileOptions *> *mFileOptArray;
  CArray<TiltSeriesParam *, TiltSeriesParam *> *mTSparamArray;
  CArray<MontParam *, MontParam *> *mMontParArray;
  CArray<StateParams *, StateParams *> *mAcqStateArray;
  CArray<StateParams *, StateParams *> mStateArray;
  CArray<StateParams, StateParams> mSavedStates;
  CCameraController *mCamera;
  WINDOWPLACEMENT mStatePlacement;
  CArray<CenterSkipData, CenterSkipData> mCenterSkipArray;
  CArray<BaseMarkerShift, BaseMarkerShift> mMarkerShiftArray;
  MultiShotParams mMultiShotParams;
  int mEnableMultiShot;
  HoleFinderParams mHoleFinderParams;
  AutoContourParams mAutoContourParams;
  NavAcqAction mAllAcqActions[3][NAA_MAX_ACTIONS];
  NavAcqAction *mAcqActions;
  NavAlignParams mNavAlignParams;
  StateParams mPriorState;

  std::vector<int> mPieceSavedAt;  // Sections numbers for existing pieces in montage
  int mThirdRoundID;           // Map ID of map itself for 2nd round alignment
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
  int mRIautoAlignFlags;        // corrFlags vale for autoalign
  int mRIstartingMagInd;        // Current mag index, saved by FindMapForRealign
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
  BOOL mShowStateNumbers;       // Flag to show state # before name
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
  WINDOWPLACEMENT mAcquireDlgPlace;
  WINDOWPLACEMENT mComaVsISDlgPlace;
  WINDOWPLACEMENT mAutoContDlgPlace;
  WINDOWPLACEMENT mMultiGridPlace;
  WINDOWPLACEMENT mMGSettingsPlace;
  bool mOpenedMultiGrid;              // Flag that it was ever open
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
  bool mRIdidSaveState;
  bool mSettingState;           // Flag set that state or low dose is being set
  float mRIresetISCritForAlign; // Criterion for doing a reset shift - realign operation
  int mRIresetISmaxNumAlign;    // Maximum times to do rest shift - realign
  int mRIresetISleaveZero;      // Whether to leave IS at zero
  int mRIresetISnumDone;        // Counter for number done
  bool mRIresetISneedsAlign;    // Whether the reset needs an alignment after it
  bool mRIresetISAfter3rdRound; // Flag to do reset IS after scaled align
  int mRIResetISmagInd;         // Mag index it is to be done at
  LowDoseParams mRIsavedLDparam;
  float mRIareaDefocusChange;   // Change in defocus offset when staying in low dose
  float mRIdefocusChangeLimit;  // Maximum defocus change for it to stay in LD
  BOOL mRIuseBeamOffsets;       // Flag for whether to use the offsets
  float mTestParams[4];         // Parameters that can be set from macro
  int mContinuousRealign;       // Flag to use continuous mode; > 1 to leave it running
  int mRIContinuousMode;        // Copy of flag for this run of realign
  float mRITiltTolerance;       // Maximum tilt difference for not tilting before realign
  bool mRIJustMoving;           // Flag just to do stage move when skip center
  bool mRISkipNextZMove;        // Flag to skip the Z move in next nav Realign if script
  int mRIbufWithMapToScale;
  int mRISetNumForScaledAlign;
  float mGridGroupSize;         // Radius in microns for adding points in groups
  BOOL mDivideIntoGroups;       // Flag for whether to do it
  bool mEditReminderPrinted;     // Flag that reminder printed when edit mode turned on
  BOOL mCollapseGroups;         // To keep "collapse" setting for dialog
  BOOL mShowTableIndexes;       // To keep "Indexes" setting for dialog
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
  int mDoingMultipleFiles;       // Flag to avoid "no longer inherits" messages
  int mDoingMultiGridFiles;      // Flag to avoid dialogs completely, etc. 
  int mOKtoUseHoleVectors;       // Flag that it is OK to use vectors without confirmation
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
  BOOL mMHCturnOffOutsidePoly;    // Option to remove points outside if using polygon 
  int mMHCdelOrTurnOffIfFew;     // Delete points or turn off combined item if < threshold
  int mMHCthreshNumHoles;        // Threshold # of holes for delete or turn off
  BOOL mMHCskipAveragingPos;     // Flag to skip averaging positions
  int mSkipAstigAdjustment;     // Property to skip the astigmatism when adjusting for IS
  IntVec mSavedMaShMapIDs;       // Saved map marker shift information: map ID
  IntVec mSavedMaShCohortIDs;    // The exiting cohort ID value
  FloatVec mSavedMaShXshift;     // The existing marker shift if any
  FloatVec mSavedMaShYshift;
  int mNumAcqActions;
  static int mAcqActDefaultOrder[NAA_MAX_ACTIONS + 1];    // A modifiable default order
  int mAcqActCurrentOrder[3][NAA_MAX_ACTIONS];    // Current order
  int mCurAcqParamIndex;         // Current acquire param set
  int mMarkerShiftApplyWhich;    // Saved dialog selection for which ones to apply to
  int mMarkerShiftSaveType;      // And whether/how to save shifts  
  BOOL mReverseAutocontColors;   // Flag to use colors in reverse
  BOOL mKeepColorsForPolygons;   // Flag to keep same colors when converting to polygons
  float mMaxMontReuseWaste;      // Maximum fraction of area to waste when reusing montage
  BOOL mRIErasePeriodicPeaks;    // Erase peaks in realign to item
  float mScaledAliDfltMaxRot;    // Default maximum rotation in search of scaled realign
  float mScaledAliDfltPctChg;    // Default maximum % size change in search of scaled realign
  float mRISkipItemPosMinField;  // Min field for skipping second round if scaled realign
  CString mFirstMontFilename;
  float mMinAutoAdjHexRatio;     // Minimum fraction of field hole needs to be for hex
  float mMinAutoAdjSquareRatio;  // and for rectangular arrays
  float mMaxAutoAdjHoleRatio;    // Maximum allowed fraction of field
  int mMinAutoAdjCropSize;       // Minimum number of pixels in image cropped to satisfy min
  float mLastUsedHoleISvecs[6];  // Last set of hole IS vectors used to set multishot IS
  float mMaxISvecMatchScaleDiff; // Maximum difference in scale or rotations from last set
  float mMaxISvecMatchRotDiff;   // of hole IS vectors allowed for permuting vectors
  IntVec mExtraTaskList;       // List of NAACT_EX... indexes to include, plus special

public:
  void PrepareToReimageMap(CMapDrawItem * item, MontParam * param, ControlSet * conSet,
    int baseNum, int hideLDoff, int noFrames);
  void ComputeStageToImage(EMimageBuffer *imBuf, float stageX, float stageY,
    BOOL needAddIS, ScaleMat &aMat, float &delX, float &delY);
  BOOL ConvertIStoStageIncrement(int magInd, int camera, double ISX, double ISY,
    float angle, float &stageX, float &stageY, EMimageBuffer *imBuf = NULL);
  ScaleMat GetRotationMatrix(float rotAngle, BOOL inverted);
  int PrepareMontAdjustments(EMimageBuffer * imBuf, ScaleMat & rMat, ScaleMat & rInv, float & rDelX, float & rDelY);
  void AdjustMontImagePos(EMimageBuffer * imBuf, float & inX, float & inY, int *pcInd = NULL,
    float *xInPiece = NULL, float *yInPiece = NULL);
  int OffsetMontImagePos(MiniOffsets *mini, int xPcStart, int xPcEnd,
    int yPcStart, int yPcEnd, float &testX, float &testY, int &pcInd, float &xInPiece,
    float &yInPiece);
  void StagePositionOfPiece(MontParam * param, ScaleMat aMat, float delX, float delY,
    int ix, int iy, float &stageX, float & stageY, float &montErrX, float &montErrY);
  void RealignNextTask(int param);
  void RealignCleanup(int error);
  void StopRealigning(void);
  void StopAndReportFinish(void);
  void RestoreForStopOrScaledAlign();
  int RotateForAligning(int bufNum, ScaleMat *useMat = NULL);
  int TransformBuffer(EMimageBuffer * imBuf, ScaleMat sizingMat, int sizingWidth,
    int sizingHeight, float sizingFrac, ScaleMat rMat);
  void StartThirdRound(void);
  float PieceToEdgeDistance(MontParam * montP, int ixPiece, int iyPiece);
  int GetLastStageError(float &stageErrX, float &stageErrY, float &localErrX,
    float &localErrY);
  int GetLastErrorTarget(float & targetX, float & targetY);
  void NavOpeningOrClosing(bool open);
  void InitAcqActionFlags(bool opening);
  bool IsExtraTaskIncluded(int ind);
  void UpdateSettings();
  float *GetGridLimits() {return &mGridLimits[0];};
  void Initialize(void);
  void RestoreSavedState(bool skipScope = false);
  int SetToMapImagingState(CMapDrawItem * item, bool setCurFile, int hideLDoff = 0, int noFrames = 1);
  int RestoreFromMapState(void);
  void ChangeAllBufferRegistrations(int mapID, int fromReg, int toReg);
  CString NextAutoFilename(CString inStr, CString oldLabel = "", CString newLabel = "");
  CString DecomposeNumberedName(CString inStr, CString &ext, int &curnum, int &numdig, 
    CString &extra);
  void CheckForSameRootAndNumber(CString &root, CString &ext,
    CString &extra, CString name, int &maxNum, int &numDig);
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
  void EndAllNewFilesSetToOpen();
  ScheduledFile * GetFileTypeAndSchedule(CMapDrawItem * item, int & fileType);
  void DeleteArrays(void);
  int FindMapForRealigning(CMapDrawItem * inItem, BOOL restoreState);
  void StoreCurrentStateInParam(StateParams * param, int lowdose,
    int saveLDfocusPos, int camNum, int saveTargOffs, int saveReadModes = -1);
  void StoreMapStateInParam(CMapDrawItem * item, MontParam *montP, int baseNum,
    StateParams * param);
  void SetStateFromParam(StateParams *param, ControlSet *conSet, int baseNum,
    int hideLDoff = 0, bool skipScope = false);
  void SetConsetsFromParam(StateParams *param, ControlSet *conSet, int baseNum);
  void SaveCurrentState(int type, int saveLDfocusPos, int camNum, int saveTargOffs, BOOL montMap = false,
    int saveReadModes = -1);
  void SaveLowDoseAreaForState(int area, int camNum, bool saveTargOffs, BOOL montMap);
  int AreaFromStateLowDoseValue(StateParams *param, int *setNum);
  void ForgetSavedState(void);
  bool GetSavedPriorState(StateParams &state);
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
  int AssessAcquireForParams(NavAcqParams *navParams, NavAcqAction *acqActions,
    MultiShotParams &multiShotParams, int startInd, int endInd, CString prefix);
  int CheckForBadParamIndexes(CString prefix);
  int GetAcqParamIndexToUse(bool starting = false);
  BOOL IsMultishotSaving(bool *allZeroER = NULL, MultiShotParams *params = NULL);
  void GetMultishotDistAndAngles(MultiShotParams *params, BOOL hexGrid, double dists[3], 
    double &avgDist, double &angle);
  int FindNearestItemMatchingText(float stageX, float stageY, CString &text, bool matchNote);
  BOOL GetNoMessageBoxOnError();
  void ListFilesToOpen(void);
  bool AreAnyFilesSetToOpen() {return mMontParArray->GetSize() > 0 || mTSparamArray->GetSize() > 0 || mFileOptArray->GetSize() > 0;};
  bool NameToOpenUsed(CString name);
  int AlignWithRotation(int buffer, float centerAngle, float angleRange,
    float &rotBest, float &shiftXbest, float &shiftYbest, float scaling = 0., int doPart = 0,
    float *maxPtr = NULL, float shiftLimit = -1.f, int corrFlags = 0);
  int AlignWithScaleAndRotation(int buffer, bool doImshift, float scaleRange, float angleRange,
    float &scaleMax, float &rotation, float shiftLimit, int corrFlags); 
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

  int LoadPieceContainingPoint(CMapDrawItem * ptItem, int mapIndex);
  void StartRealignCapture(bool useContinuous, int nextTask);
  void GetViewOffsets(CMapDrawItem * item, float & netShiftX, float & netShiftY, float & beamShiftX, float & beamShiftY, float & beamTiltX, float & beamTiltY, int area = VIEW_CONSET);
  void StateCameraCoords(int camIndex, int xFrame, int yFrame, int binning, int &left, int &right, int &top, int &bottom);
  bool CanStayInLowDose(CMapDrawItem * item, int xFrame, int yFrame, int binning, int & set, float & netShiftX, float & netShiftY, bool forReal);
  void SimpleIStoStage(CMapDrawItem * item, double ISX, double ISY, float &stageX, float &stageY);
  ScaleMat ItemStageToCamera(CMapDrawItem * item);
  void CountAcquireItems(int startInd, int endInd, int & numAcquire, int & numTS);
  void CountHoleAcquires(int startInd, int endInd, int minHoles, int &numCenter, int &numHoles, int &numRecs);
  void RestoreLowDoseConset(void);
  int ProcessExternalItem(CMapDrawItem * item, int extType);
  int TransformExternalCoords(CMapDrawItem *item, int extType,
     CMapDrawItem *mapItem, float &fx, float &fy, int &pieceDrawnOn, float &xInPc, float &yInPc);
  void CleanupFromExternalFileAccess();
  static int TaskRealignBusy(void);
  void StartResetISorFinish(int magInd);
  void OpenMultishotDlg(void);
  WINDOWPLACEMENT *GetMultiShotPlacement(bool update);
  void UpdateMultishotIfOpen(bool draw = true);
  int RotateMultiShotVectors(MultiShotParams *params, float angle, int customOrHex);
  int AdjustMultiShotVectors(MultiShotParams *params, int customOrHex, bool statusOnly, CString &mess);
  void TransformMultiShotVectors(MultiShotParams *params, int customOrHex, ScaleMat &aProd);
  int XformISVecsWithSpecOrStage(float *xVecIn, float *yVecIn, int numVec, ScaleMat mat, 
    bool stage, int magInd, int camera, float *xVecOut, float *yVecOut);
  void AssignNavItemHoleVectors(CMapDrawItem * item, MultiShotParams *msParams = NULL);
  int PermuteISvecsToMatchLastUsed(float *xVecIn, float *yVecIn, int hexInd, 
    float &bestRot, float &maxAngDiff, float &maxScaleDiff, float *xVecOut, float *yVecOut);
  void PermuteISVectors(float *xVecIn, float *yVecIn, int numVec, int rotInd, float *xVecOut, float *yVecOut);
  void SetLastUsedHoleISVecs(float *xVecs, float *yVecs, bool setChanged);
  int OKtoUseNavPtsForVectors(int pattern, int &groupStart, int &groupEnd, ScaleMat *ISmat = NULL,
    CString *reason = NULL);
  int ConfirmReplacingShiftVectors(int kind, int vecType);
  int UseNavPointsForVectors(int pattern, int numXholes, int numYholes);
  void OpenHoleFinder(void);
  WINDOWPLACEMENT *GetHoleFinderPlacement(void);
  void OpenMultiCombiner(void);
  WINDOWPLACEMENT *GetMultiCombinerPlacement();
  void OpenComaVsISCal(void);
  WINDOWPLACEMENT *GetComaVsISDlgPlacement();
  void OpenAutoContouring(bool fromMulti);
  WINDOWPLACEMENT *GetAutoContDlgPlacement(void);
  void OpenMultiGrid(void);
  WINDOWPLACEMENT *GetMultiGridPlacement(void);
  void OpenMGSettingsDlg(int jcdInd);
  WINDOWPLACEMENT *GetMGSettingsPlacement(void);
  WINDOWPLACEMENT *GetAcquireDlgPlacement(bool fromDlg);
  void UpdateAcquireDlgForFileChanges();
  void CopyAcqParamsAndActionsToTemp(int which);
  void CopyAcqParamsAndActionsToTemp(NavAcqAction *useAct, NavAcqParams *useParams, int *useOrder);
  void CopyAcqParamsAndActions(NavAcqAction *useAct, NavAcqParams *useParams, int *useOrder,
    NavAcqAction *actions, NavAcqParams *params, int *order);
  void SaveLDFocusPosition(int saveIt, float & axisPos, BOOL & rotateAxis, int & axisRotation,
    int & xOffset, int & yOffset, bool traceIt);
  void SetLDFocusPosition(int camIndex, float axisPos, BOOL rotateAxis, int axisRotation, 
    int xOffset, int yOffset, const char *descrip, bool forTrial);
  int CheckTiltSeriesAngles(int paramInd, float start, float end, float bidir, CString &errMess);
  bool AnyMontageMapsInNavTable();
  void ModifyMontsForReusability(IntVec &montInds);
  int FindLastFileWithMatchingMontParams(MontParam *param1);
  bool AreOnlyReusableMontagesSetToOpen();
  int FindMapIDforReadInImage(CString filename, int secNum, int ignoreLoad = false);
  int SetUserValue(CMapDrawItem *item, int number, CString &value);
  int GetUserValue(CMapDrawItem *item, int number, CString &value);
  bool ModifySubareaForOffset(int camera, int xOffset, int yOffset, int &left,
    int &top, int &right, int &bottom);
  void FindFocusPosForCurrentItem(StateParams & state, bool justLDstate, int registration, int curInd = -1);
  int RealignToDrawnOnMap(CMapDrawItem *item, BOOL restoreState);
  bool GetNumHolesFromParam(int &xnum, int &ynum, int &numTotal);
  int GetNumHolesForItem(CMapDrawItem *item, int numDefault);
  void ClearSavedMapMarkerShifts();
  void SaveMapMarkerShiftToLists(CMapDrawItem *item, int cohortID, float newXshift,
    float newYshift);
  void RestoreMapMarkerShift(CMapDrawItem *item);
  BaseMarkerShift *FindNearestBaseShift(int fromMag, int toMag);
  bool OKtoApplyBaseMarkerShift();
  void ApplyBaseMarkerShift();
  bool OKtoShiftToMarker();
  void GetHFscanVectors(FloatVec **widths, FloatVec **increments, IntVec **numCircles) 
  {*widths = &mHFwidths; *increments = &mHFincrements; *numCircles = &mHFnumCircles;};

};

