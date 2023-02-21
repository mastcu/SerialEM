// EMmontageController.h: interface for the EMmontageController class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EMMONTAGECONTROLLER_H__F50578E7_DA31_4D56_B16C_C01548ED89E2__INCLUDED_)
#define AFX_EMMONTAGECONTROLLER_H__F50578E7_DA31_4D56_B16C_C01548ED89E2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MONT_NOT_TRIAL 0
#define MONT_TRIAL_IMAGE 1
#define MONT_TRIAL_PRECOOK 2

#include <vector>
#include "EMscope.h"

struct MultiShotParams;

// Structure for storing limits to frame sizes in fitted montages
struct MontLimits
{
  int camera;           // Camera #
  int magInd;           // Mag at which limit applies
  int top, left, bottom, right;    // Unbinned usable area
};

struct XCorrThreadData
{
  float *lowerPatch[2];
  float *upperPatch[2];
  int *XYbox[2];
  int *XYpieceSize;
  int *XYoverlap;
  int *Xsmooth, *Ysmooth, *Xpad, *Ypad;
  float *lowerPad;
  float *upperPad;
  float *lowerCopy;
  int numXcorrPeaks;
  float *CTFp[2];
  float *delta;
  int *numExtra[2];
  int XCorrBinning;
  int *maxLongShift;
  char *debugStr;
  int debugLen;
  int debugLevel;
  int lower[2], upper[2], idir[2], pieceIndex;
  float xPeak[2], yPeak[2];
  float xFirst[2], yFirst[2];
  float CCCmax[2];
  float trimmedMaxSD[2];
  float alternShifts[2][4];
};

class EMmontageController
{
 public:
	 int SetPrescanBinning(int tryVal, int dir);
	 int SetMaxPrescanBinning();
	 int GetMontageActiveCamera(MontParam *param);
	 bool CameraNotFeasible(MontParam *param = NULL);
	 void SetNextPieceIndexXY();
	 int NextPieceIndex();
	 void SetupOverlapBoxes();
   float TreatAsGridMap();
  EMmontageController();
  ~EMmontageController();
  void Initialize();
  GetMember(BOOL, Montaging)
  BOOL DoingMontage() {return (mPieceIndex >= 0);};
  int StartMontage(int inTrial, BOOL inReadMont, float cookDwellTime = 0., 
    int cookSkip = 0, bool skipColumn = false, float cookSpeed = 0.);
  int ReadMontage(int inSect = NO_SUPPLIED_SECTION, MontParam *inParam = NULL,
    KImageStore *inStoreMRC = NULL, BOOL centerOnly = false, BOOL synchronous = false,
  int bufToCopyTo = -1);
  void StopMontage(int error = 0);
  int SavePiece();
  void ProcessXCorrResult();
  int AddToActualErrors();
  void MaintainErrorSums(int nSum, int pieceIndex);
  static UINT XCorrProc(LPVOID param);
  int WaitForXCorrProc(int timeout);
  void SetMontaging(BOOL inVal);
  void PieceCleanup(int error);
  int DoNextPiece(int param);
  void GetCurrentCoords(int &outX, int &outY, int &outZ);
  int GetSectionToDo() {return mPieceSavedAt[mPieceIndex];};
  int LowerIndex(int inIndex, int inDir);
  int UpperIndex(int inIndex, int inDir);
  void CleanPatches();
  BOOL GetLastFailed() {return mLastFailed;};
  SetMember(float, RequiredBWMean)
  GetSetMember(float, StageBacklash)
  GetMember(BOOL, Focusing)
  SetMember(float, MaxStageError)
  SetMember(BOOL, StopOnStageError)
  GetSetMember(int, CountLimit)
  GetSetMember(BOOL, ShootFilmIfDark)
  GetSetMember(BOOL, LimitMontToUsable)
  GetSetMember(int, DuplicateRetryLimit)
  GetSetMember(int, NumOverwritePieces)
  GetMember(int, NumPieces)
  GetSetMember(BOOL, AlignToNearestPiece)
  GetSetMember(int, ISrealignInterval)
  GetSetMember(BOOL, UseTrialSetInISrealign);
  GetSetMember(int, DriftRepeatLimit);
  GetSetMember(float, DriftRepeatDelay);
  GetSetMember(BOOL, AutosaveLog);
  GetSetMember(float, MinOverlapForZigzag);
  GetSetMember(BOOL, DivFilterSet1By2);
  GetSetMember(BOOL, DivFilterSet2By2);
  GetSetMember(BOOL, UseFilterSet2);
  GetSetMember(BOOL, UseSet2InLD);
  GetSetMember(bool, RedoCorrOnRead);
  SetMember(bool, NeedBoxSetup);
  GetMember(BOOL, UseContinuousMode);
  GetSetMember(float, GridMapCutoffInvUm);
  GetSetMember(float, GridMapMinSizeMm);
  GetSetMember(int, MacroToRun);
  GetSetMember(bool, RunningMacro);
  GetSetMember(BOOL, AllowHQMontInLD);
  void SetBaseISXY(double inX, double inY) {mBaseISX = inX; mBaseISY = inY;};
  void SetXcorrFilter(int ind, float r1, float r2, float s1, float s2) {mSloppyRadius1[ind] = r1;
    mRadius2[ind] = r2; mSigma1[ind] = s1; mSigma2[ind] = s2;};
  void GetLastBacklash(float &outX, float &outY);
  double GetRemainingTime();
  CArray<MontLimits, MontLimits> *GetMontageLimits() {return &mMontageLimits;};

 private:
  EMbufferManager *mBufferManager;
  CCameraController *mCamera;
  CTSController *mTSController;
  CEMscope *mScope;
  ControlSet *mConSets;
  MontParam *mParam;
  MagTable   *mMagTab;
  EMimageBuffer *mImBufs;
  CSerialEMApp * mWinApp;
  CShiftManager *mShiftManager;
  CameraParameters *mCamParams;
  MiniOffsets mMiniOffsets;
  XCorrThreadData mXCTD;
  CWinThread *mXCorrThread;
  BOOL mMontaging;
  IntVec   mMontageX, mMontageY;
  IntVec   mPieceSavedAt;
  int   mPieceIndex;
  int   mPieceX, mPieceY;
  int   mNumPieces;               // Number of pieces in montage
  int   mMontCenterX, mMontCenterY;
  float mFocusPitchX, mFocusPitchY;
  KImageStore *mReadStoreMRC;
  MultiShotParams *mMultiShotParams;
  bool mUsingMultishot;
  int mAdjustedOverlaps[2];
  float mExpectedShiftsX[2];
  float mExpectedShiftsY[2];
  int mAction;
  int mNumActions;

  BOOL mReadingMontage;
  int mTrialMontage;
  BOOL mCenterOnly;               // Flag to do montage center only when reading in
  BOOL mSynchronous;              // Flag to read in sysnchronously
  int mBufToCopyTo;               // Buffer to copy overview to when reading
  BOOL mReadResetBoxes;           // Flag to reset boxes after read
  BOOL mNeedToFillMini;           // Flag that mini should be filled with first mean
  BOOL mNeedToFillCenter;         // Flag that center should be filled
  float mMiniFillVal;             // Fill value from mean of first piece
  int mPcIndForFillVal;           // Piece index at which to get mean when deferred tiling
  int mReadSavedZ;                // Saved Z value to reset after read
  MontParam mTrialParam;
  double mBaseISX, mBaseISY;      // Base image shifts
  double mBaseStageX, mBaseStageY;  // Starting stage position
  double mBaseFocus;              // Base focus
  ScaleMat mBinv;                 // Matrix to get from pixels to Image or Stage Shift
  ScaleMat mCamToIS;              // Pixels to image shift matrix for IS in blocks
  StageMoveInfo mMoveInfo;        // Structure for stage moves
  bool mDoStageMoves;             // Replacement for moveStage in param, in case multishot
  BOOL mMovingStage;              // Flag that stage is being moved
  BOOL mRestoringStage;           // Flag for final stage move at end
  BOOL mFocusing;                 // Flag for focusing after stage move
  float mStageBacklash;           // Basic backlash for moving stage
  bool mImShiftInBlocks;          // Flag that we are image shifting in blocks
  int mFirstResumePiece;          // Index of first piece after resume for block actions
  bool mUsingImageShift;          // Convenience flag for not stage or doing block IS

  int   mXCorrBinning;            // Binning to apply to extracted boxes
  int   mXYpieceSize[2];          // Frame size and overlap in arrays for montXC functions
  int   mXYoverlap[2];            // in X and Y
  int   mXpad[2], mYpad[2];       // Padded size for X and Y edges
  int   mXsmooth[2], mYsmooth[2]; // Size for outside smoothing for X and Y edges

  // For these [2][2] arrays, first index is edge X or Y, second index is X or Y dimension
  int   mXYbox[2][2];             // Size of box to extract from overlap
  int   mNumExtra[2][2];          // Number of extra pixels of overlap width
  int   mLowerInd0[2][2], mLowerInd1[2][2];   // Area to extract in lower piece
  int   mUpperInd0[2][2], mUpperInd1[2][2];   // Area to extract in upper piece
  int   mPadPixels;              // total size of padded arrays
  int   mBoxPixels[2];              // total size of boxed patch arrays for X and Y edges
  int   mMaxLongShift[2];         // Maximum shift in long direction on X and Y edges
  float mCTFx[8193], mCTFy[8193]; // CTF for filtering
  float mCTFa[8193];              // CTF for autoalign
  float *mCTFp[2];                // pointers to CTFs
  float mDelta[2];                // Delta from the CTF
  float mSloppyRadius1[2];        // Filter parameters
  float mRadius2[2];
  float mSigma1[2];
  float mSigma2[2];
  float *mLowerPad;               // Padded arrays needed for processing an edge
  float *mUpperPad;
  float *mLowerCopy;              // Copy of lower padded image, for CCC evaluation
  float *mBinTemp;                // Temporary array for binning

  // These arrays are indexed by piece number and X or Y edge direction
  std::vector<float *> mLowerPatch;  // Address of patch when piece is lower
  std::vector<float *> mUpperPatch;  // Address of patch when piece is upper
  // For given piece, amount that upper piece from it is displaced from this piece
  FloatVec  mUpperShiftX, mUpperShiftY;
  FloatVec  mUpperFirstX, mUpperFirstY;
  FloatVec mPatchCCC;
  FloatVec mTrimmedMaxSDs;
  IntVec mMaxSDToPieceNum;
  IntVec mMaxSDToIxyOfEdge;
  FloatVec mAlternShifts;
  FloatVec mActualErrorX, mActualErrorY; // Error from true position
  FloatVec mAcquiredStageX, mAcquiredStageY; // Actual position at which acquired
  FloatVec mStageOffsetX, mStageOffsetY; // Stage offset from nominal to actual
  FloatVec mBmat;                  // Matrix needed for solving for shifts
  float mPredictedErrorX, mPredictedErrorY;   // Predicted error to correct for
  float mErrorSumX, mErrorSumY;   // Sum of actual errors
  int   mNumErrSum;               // Number added into sum
  short int *mMiniData;           // Array for mini view
  unsigned char *mMiniByte;
  unsigned short *mMiniUshort;
  float *mMiniFloat;
  BOOL  mConvertMini;             // Convert to bytes for mini view
  int   mMiniZoom;                // Zoom factor
  int   mMiniFrameX, mMiniFrameY; // Size of one frame in mini view
  int   mMiniDeltaX, mMiniDeltaY; // Pixels from one frame to next
  int   mMiniSizeX, mMiniSizeY;   // Total size of mini view
  float mMiniEffectiveBin;        // binning to set into imbuf for mini view
  int   mNumCenterSaved;          // Number of images saved for center
  int   mCenterCoordX[4];         // Coordinates of saved pieces
  int   mCenterCoordY[4];
  int   mCenterShift[4][2];
  int   mCenterPiece[4];          // Piece number of saved piece
  KImage *mCenterImage[4];        // KImage's of saved pieces
  short int *mCenterData;         // Array for composed center piece
  float mRequiredBWMean;          // Black/white mean per unbinned sec required
  float mGridMapCutoffInvUm;      // radius2 in 1/um when doing grid map
  float mGridMapMinSizeMm;        // Min linear size in mm for treating as grid map
  bool mTreatingAsGridMap;        // Flag that it is setup with grid map parameters
  int mNumXcorrPeaks;             // Number of peaks to analyze
  int mNumTooDim;                 // Number of frames too dim
  BOOL mLastFailed;               // Flag that last capture did not complete
  BOOL mVerySloppy;               // Flag to do big correlations with special parameters
  BOOL mUseExpectedShifts;        // Weight by deviation from expected shifts
  int mNumToSkip;                 // Number of pieces to skip
  IntVec mSkipIndex;   // Index of pieces to skip
  IntVec mPieceToVar;  // Index from piece number to variable number
  IntVec mVarToPiece;  // Index from variable number to piece number
  IntVec mIndSequence;   // Sequence of piece indexes to do
  IntVec mFocusBlockInd; // Index to focus block for first piece in block
  IntVec mBlockCenX, mBlockCenY;  // stage position for focusing
  IntVec mAdjacentForIS; // Piece index of adjacent piece for IS realignment
  IntVec mColumnStartY; // Y piece index (from 0) at which column starts for each X
  int mSeqIndex;                  // Running index to this sequence array
  int mNextSeqInd;                // Index after asking for next piece index
  int mNumDoing;                   // Number of frame being done, for progress display
  float mMaxStageError;           // Maximum error for a stage move (0 means don't test)
  BOOL mStopOnStageError;         // Flag to stop after 3 tries with stage error
  int mNumStageErrors;            // Counter for number of errors
  int mCountLimit;                // Limit on number of counts
  BOOL mShootFilmIfDark;          // Flag to take film picture on image below limit
  BOOL mShotFilm;                 // Flag that it was done
  BOOL mShootingFilm;             // Flag that it is happening!
  BOOL mLimitMontToUsable;        // Flag to limit montage to usable area
  BOOL mActPostExposure;              // Flag for doing image shift/stage shift during acquire
  BOOL mNeedBacklash;             // Flag that backlash is needed for move to this piece
  BOOL mPreMovedStage;            // Flag that stage move was queued
  int mDuplicateRetryLimit;       // Limit to number of retries for Eagle duplicates
  int mDuplicateRetryCount;       // Current count of retries
  int mRealignInd;                 // Index of piece being realigned to
  int mNumDoneInColumn;
  int mNumSinceRealign;
  int mRealignInterval;           // Run-time value from param interval if doing realign
  BOOL mAlignToNearestPiece;      // Property flag to find nearest piece for realigning to
  int mISrealignInterval;         // Interval at which to do the IS realign
  BOOL mUseTrialSetInISrealign;   // Flag to use trial set as is in IS realign
  BOOL mDoISrealign;              // Run-time flag to do image-shift realignment
  int mSecondHalfStartSeq;        // Sequence index of first piece in second half
  BOOL mUsingAnchor;              // Run-time flag that anchor is being used
  BOOL mNeedAnchor;               // Flag that we need to acquire an anchor on first piece
  BOOL mNeedRealignToAnchor;      // Flag that realign to anchor is still needed
  BOOL mNeedColumnBacklash;       // Flag that we need to measure backlash not get anchor
  BOOL mNeedISforRealign;         // Flag that setting realign IS was deferred
  CString mAnchorFilename;        // Name of anchor file
  double mAnchorStageX;           // Stage position of anchor
  double mAnchorStageY;
  double mBacklashAdjustX;        // Adjustment for upper half images
  double mBacklashAdjustY;
  int mAddedSubbedBacklash;
  double mMoveBackX, mMoveBackY;  // Backlash entries to use in moveinfo
  BOOL mLoweredMag;               // Flag that mag was lowered for anchor
  BOOL mFocusAfterStage;          // Run-time flag to focus after every stage move
  BOOL mFocusInBlocks;            // Run time flag for focusing in blocks
  int mDriftRepeatLimit;          // Limit on number of tries for drift to fall below limit
  int mDriftRepeatCount;          // Count in current round
  float mDriftRepeatDelay;        // Delay in seconds before repeating autofocus
  BOOL mDoCorrelations;           // Run-time flag to do correlations
  int mNumOverwritePieces;        // One-shot number of pieces to overwrite if resumable
  BOOL mAlreadyHaveShifts;        // Flag that edge shifts were read from an adoc
  float mBefErrMean, mBefErrMax;  // Mean and max error before and after and weighted errors
  float mAftErrMean, mAftErrMax;
  float mWgtErrMean, mWgtErrMax;
  int mMiniArrayX, mMiniArrayY;   // Actual size of the mini array with deferred tiling
  int mMiniBorderY;               // Lines of border in the mini array
  BOOL mHaveStageOffsets;         // Flag for working with stage offsets
  BOOL mAutosaveLog;              // Flag to autosave log
  bool mDefinedCenterFrames;      // Flag that centerframes were set
  int mCenterIndX1, mCenterIndX2; // Starting and ending center frames in X
  int mCenterIndY1, mCenterIndY2; // Starting and ending center frames in Y
  bool mAddMiniOffsetToCenter;
  bool mDoZigzagStage;            // Flag that stage is being moved up and down columns
  float mMinOverlapForZigzag;     // Minimum overlap required to use this
  int mDwellTimeMsec;             // Dwell time when precooking
  double mDwellStartTime;         // Starting time of dwell
  BOOL mDivFilterSet1By2;
  BOOL mDivFilterSet2By2;
  BOOL mUseFilterSet2;
  BOOL mUseSet2InLD;
  bool mNeedBoxSetup;
  bool mRedoCorrOnRead;
  float mAdjustmentScale;         // Scale and rotation adjustment from high-focus mag cal
  float mAdjustmentRotation;
  float mDefocusForCal;
  bool mExpectingFloats;          // Flag that images should be floats
  BOOL mUseContinuousMode;        // Flag to use camera continuous acquisition
  int mNumContinuousAlign;        // Number of continuous mode shots to align
  int mNumDropAtContinStart;      // Drift settling: pass to camera when starting continous
  double mStartTime;              // Tick time when it started
  double mLastElapsed;            // Elapsed time and mNumDoing values when
  int mLastNumDoing;               // GetRemainingTime was last called
  int mInitialNumDoing;
  CArray<MontLimits, MontLimits> mMontageLimits;
  int mMacroToRun;                // Script, numbered from 1
  bool mRunningMacro;             // Flag that we started a script
  BOOL mAllowHQMontInLD;          // Flag to enable HQ options in low dose

public:
	void AdjustShiftInCenter(MontParam *param, float &shiftX, float &shiftY);
  int ListMontagePieces(KImageStore * storeMRC, MontParam * param, int zValue,
    IntVec &pieceSavedAt);
  void ReadingDone(void);
  void StartStageRestore(void);
  void StageRestoreDone(void);
  int FindPieceForRealigning(int pieceX, int pieceY);
  int PieceIndexFromXY(int pieceX, int pieceY);
  int RealignToExistingPiece(void);
  void RealignNextTask(int param);
  void ComputeMoveToPiece(int pieceInd, BOOL focusBlock, int &iDelX, int &iDelY, double
    &adjISX, double &adjISY);
  int PieceStartsColumn(int index);
  bool PieceNeedsRealigning(int index, bool nextPiece);
  bool AddToSkipListIfUnique(int ix);
  int FindBestShifts(int nvar, float *upperShiftX, float *upperShiftY, float *bMat,
     float &bMean, float &bMax, float &aMean, float &aMax, float &wMean, float &wMax, bool tryAltern);
  int AutodocShiftStorage(bool write, float * upperShiftX, float * upperShiftY);
  void NextMiniZoomAndBorder(int &borderTry);
  void RetilePieces(int miniType);
  void SetAcquiredStagePos(int piece, double stageX, double stageY, double ISX,
    double ISY);
  void FindAdjacentForISrealign(void);
  void GetAdjustmentsForISrealign(int piece, double & stageAdjX, double & stageAdjY,
    double & adjISX, double & adjISY, int & ubpixOffX, int &ubpixOffY);
  int RealignToAnchor(void);
  int AutodocStageOffsetIO(bool write, int pieceInd);
  bool CalNeededForISrealign(MontParam * param);
  void GetColumnBacklash(float & backlashX, float & backlashY);
  void LimitSizesToUsable(CameraParameters * cam, int camNum, int magInd, int & xsize, int & ysize, int binning);
  void LimitOneSizeToUsable(int &size, int camSize, int start, int end,
                                         int binning, int modulo);
  void extractPatch(void *data, int type, int ixy, int *ind0, int *ind1, float **patch);
  void SetFramesForCenterOnly(int indX1, int indX2, int indY1, int indY2);
  int CookingDwellBusy(void);
  void AdjustFilter(int index);
  int MapParamsToAutodoc(void);
  void MapParamsToOverview(int sectInd);
  int AccessMontSectInAdoc(KImageStore *store, int secNum);
  int StoreAlignedCoordsInAdoc(void);
  void SetMiniOffsetsParams(MiniOffsets & mini, int xNframes, int xFrame, int xDelta, int yNframes, int yFrame, int yDelta);
  void AddRemainingTime(CString &report);
  void BlockSizesForNearSquare(int sizeX, int sizeY, int xOverlap, int yOverlap, int blockSize,
    int &numInX, int &numInY);
  int GetCurrentPieceInfo(bool next, int &xPc, int &yPc, int &ixPc, int &iyPc);
  int TestStageError(double ISX, double ISY, double &sterr);
  int SetImageShiftTestClip(double adjISX, double adjISY, float delayFac);
};

#endif // !defined(AFX_EMMONTAGECONTROLLER_H__F50578E7_DA31_4D56_B16C_C01548ED89E2__INCLUDED_)
