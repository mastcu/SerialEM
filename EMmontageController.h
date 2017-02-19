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
  EMmontageController();
  ~EMmontageController();
  void Initialize();
  GetMember(BOOL, Montaging)
  BOOL DoingMontage() {return (mPieceIndex >= 0);};
  int StartMontage(int inTrial, BOOL inReadMont, float cookDwellTime = 0.); 
  int ReadMontage(int inSect = NO_SUPPLIED_SECTION, MontParam *inParam = NULL, 
    KImageStore *inStoreMRC = NULL, BOOL centerOnly = false, BOOL synchronous = false);
  void StopMontage(int error = 0);
  void SavePiece();
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
  SetMember(BOOL, ShootFilmIfDark)
  GetSetMember(BOOL, LimitMontToUsable)
  SetMember(int, DuplicateRetryLimit)
  GetSetMember(int, NumOverwritePieces)
  GetMember(int, NumPieces)
  SetMember(BOOL, AlignToNearestPiece)
  SetMember(int, DriftRepeatLimit);
  SetMember(float, DriftRepeatDelay);
  SetMember(BOOL, AutosaveLog);
  SetMember(float, MinOverlapForZigzag);
  GetSetMember(BOOL, DivFilterSet1By2);
  GetSetMember(BOOL, DivFilterSet2By2);
  GetSetMember(BOOL, UseFilterSet2);
  GetSetMember(BOOL, UseSet2InLD);
  GetSetMember(bool, RedoCorrOnRead);
  SetMember(bool, NeedBoxSetup);
  void SetXcorrFilter(int ind, float r1, float r2, float s1, float s2) {mSloppyRadius1[ind] = r1;
    mRadius2[ind] = r2; mSigma1[ind] = s1; mSigma2[ind] = s2;};
  void GetLastBacklash(float &outX, float &outY);
  double GetRemainingTime();

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
  BOOL mMontaging;
  std::vector<int>   mMontageX, mMontageY;
  std::vector<int>   mPieceSavedAt;
  int   mPieceIndex;
  int   mPieceX, mPieceY;
  int   mNumPieces;               // Number of pieces in montage
  int   mMontCenterX, mMontCenterY;
  float mFocusPitchX, mFocusPitchY;
  KImageStore *mReadStoreMRC;
  int mAction;
  int mNumActions;

  BOOL mReadingMontage;
  int mTrialMontage;
  BOOL mCenterOnly;               // Flag to do montage center only when reading in
  BOOL mSynchronous;              // Flag to read in sysnchronously
  BOOL mReadResetBoxes;           // Flag to reset boxes after read
  BOOL mNeedToFillMini;           // Flag that mini should be filled with first mean
  BOOL mNeedToFillCenter;         // Flag that center should be filled
  int mMiniFillVal;               // Fill value from mean of first piece
  int mReadSavedZ;                // Saved Z value to reset after read
  MontParam mTrialParam;
  double mBaseISX, mBaseISY;      // Base image shifts
  double mBaseStageX, mBaseStageY;  // Starting stage position
  double mBaseFocus;              // Base focus
  ScaleMat mBinv;                 // Matrix for getting from pixels to Image Shift
  StageMoveInfo mMoveInfo;        // Structure for stage moves
  BOOL mMovingStage;              // Flag that stage is being moved
  BOOL mRestoringStage;           // Flag for final stage move at end
  BOOL mFocusing;                 // Flag for focusing after stage move
  float mStageBacklash;           // Basic backlash for moving stage

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
  float *mLowerPatch[MAX_CORR_PIECES][2];  // Address of patch when piece is lower
  float *mUpperPatch[MAX_CORR_PIECES][2];  // Address of patch when piece is upper
  // For given piece, amount that upper piece from it is displaced from this piece
  float  mUpperShiftX[MAX_CORR_PIECES][2], mUpperShiftY[MAX_CORR_PIECES][2];    
  float  mUpperFirstX[MAX_CORR_PIECES][2], mUpperFirstY[MAX_CORR_PIECES][2];    
  float mPatchCCC[MAX_CORR_PIECES][2];
  std::vector<float> mActualErrorX, mActualErrorY; // Error from true position
  std::vector<float> mAcquiredStageX, mAcquiredStageY; // Actual position at which acquired
  std::vector<float> mStageOffsetX, mStageOffsetY; // Stage offset from nominal to actual
  float mBmat[2 * MAX_CORR_PIECES];    // Matrix needed for solving for shifts
  float mPredictedErrorX, mPredictedErrorY;   // Predicted error to correct for
  float mErrorSumX, mErrorSumY;   // Sum of actual errors
  int   mNumErrSum;               // Number added into sum
  short int *mMiniData;           // Array for mini view
  unsigned char *mMiniByte;
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
  int mNumTooDim;                 // Number of frames too dim
  BOOL mLastFailed;               // Flag that last capture did not complete
  BOOL mVerySloppy;               // Flag to do big correlations with special parameters
  int mNumToSkip;                 // Number of pieces to skip
  std::vector<int> mSkipIndex;   // Index of pieces to skip
  std::vector<int> mPieceToVar;  // Index from piece number to variable number
  std::vector<int> mVarToPiece;  // Index from variable number to piece number
  std::vector<int> mIndSequence;   // Sequence of piece indexes to do
  std::vector<int> mFocusBlockInd; // Index to focus block for first piece in block
  std::vector<int> mBlockCenX, mBlockCenY;  // stage position for focusing
  std::vector<int> mAdjacentForIS; // Piece index of adjacent piece for IS realignment
  std::vector<int> mColumnStartY; // Y piece index (from 0) at which column starts for each X
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
  float mDefocusForCal;
  BOOL mUseContinuousMode;        // Flag to use camera continuous acquisition
  double mStartTime;              // Tick time when it started
  double mLastElapsed;            // Elapsed time and mNumDoing values when 
  int mLastNumDoing;               // GetRemainingTime was last called
  int mInitialNumDoing;

public:
	void AdjustShiftInCenter(MontParam *param, float &shiftX, float &shiftY);
  int ListMontagePieces(KImageStore * storeMRC, MontParam * param, int zValue, 
    std::vector<int> &pieceSavedAt);
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
     float &bMean, float &bMax, float &aMean, float &aMax, float &wMean, float &wMax);
  int AutodocShiftStorage(bool write, float * upperShiftX, float * upperShiftY);
  void NextMiniZoomAndBorder(int &borderTry);
  void RetilePieces();
  void SetAcquiredStagePos(int piece, double stageX, double stageY, double ISX, 
    double ISY);
  void FindAdjacentForISrealign(void);
  void GetAdjustmentsForISrealign(int piece, double & stageAdjX, double & stageAdjY, 
    double & adjISX, double & adjISY, int & ubpixOffX, int &ubpixOffY);
  int RealignToAnchor(void);
  int AutodocStageOffsetIO(bool write, int pieceInd);
  bool CalNeededForISrealign(MontParam * param);
  void GetColumnBacklash(float & backlashX, float & backlashY);
  void LimitSizesToUsable(CameraParameters * cam, int & xsize, int & ysize, int binning);
  void LimitOneSizeToUsable(int &size, int camSize, int start, int end, 
                                         int binning, int modulo);
  void extractPatch(void *data, int type, int ixy, int *ind0, int *ind1, float **patch);
  void SetFramesForCenterOnly(int indX1, int indX2, int indY1, int indY2);
  int CookingDwellBusy(void);
  void AdjustFilter(int index);
  int MapParamsToAutodoc(void);
  void MapParamsToOverview(int sectInd);
  int AccessMontSectInAdoc(KImageStore *store, int secNum);
};

#endif // !defined(AFX_EMMONTAGECONTROLLER_H__F50578E7_DA31_4D56_B16C_C01548ED89E2__INCLUDED_)
