#if !defined(AFX_TSCONTROLLER_H__3C25C073_8C9A_4FCE_84A1_7754FB23C28E__INCLUDED_)
#define AFX_TSCONTROLLER_H__3C25C073_8C9A_4FCE_84A1_7754FB23C28E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TSController.h : header file
//
#include "TiltSeriesParam.h"
#include "EMscope.h"

#define MAX_TS_ACTIONS 65
#define MAX_TS_TILTS 720
#define MAX_COSINE_POWER 5
enum {BIDIR_NONE = 0, BIDIR_FIRST_PART, BIDIR_RETURN_PHASE, BIDIR_SECOND_PART};
enum {TSMACRO_PRE_TRACK1 = 1, TSMACRO_PRE_FOCUS, TSMACRO_PRE_TRACK2, TSMACRO_PRE_RECORD};
#define MAX_TSMACRO_STEPS 5
enum {DEFSUM_LOOP, DEFSUM_NORMAL_STOP, DEFSUM_ERROR_STOP, DEFSUM_TERM_ERROR};

/////////////////////////////////////////////////////////////////////////////
// CTSController command target

class DLL_IM_EX CTSController : public CCmdTarget
{
  // DECLARE_DYNCREATE(CTSController)
public:
  CTSController();           // protected constructor used by dynamic creation
  ~CTSController();

  // Attributes
public:

  // Operations
public:
  int SaveExtraImage(int index, char *message);
  void SetProtections();
  int CheckExtraFiles();
  int LookupProtectedFiles();
  void SetExtraOutput();
  BOOL AutoAlignAndTest(int bufNum, int smallPad, char *shotName, bool scaling = false);
  int SetupTiltSeries(int future = 0, int futureLDstate = -1, int ldMagIndex = 0);
  BOOL CanBackUp();
  GetSetMember(BOOL, Verbose)
  GetSetMember(BOOL, AutosaveLog)
  GetSetMember(BOOL, DebugMode)
  double AddToDoseSum(int which);
  BOOL UsableRefInA(double angle, int magIndex);
  BOOL WalkupCanTakeAnchor(double curAngle, float anchorAngle, float targetAngle);
  BOOL NextActionIs(int nextIndex, int action, int testStop = 0);
  BOOL NextActionIsReally(int nextIndex, int action, int testStop = 0);
  void DebugDump(CString message);
  BOOL CheckLDTrialMoved();
  void ResetPoleAngle() {mPoleAngle = mDirection * 90.;};
  SetMember(float, DefaultStartAngle)
  SetMember(float, MaxUsableAngleDiff)
  SetMember(float, BadShotCrit)
  SetMember(float, BadLowMagCrit)
  GetSetMember(float, MaxTiltError)
  SetMember(float, LowMagFieldFrac)
  SetMember(float, StageMovedTolerance)
  SetMember(float, UserFocusChangeTol)
  SetMember(float, FitDropErrorRatio)
  SetMember(float, FitDropBackoffRatio)
  SetMember(int, MaxImageFailures)
  SetMember(int, MaxPositionFailures)
  SetMember(int, MaxDisturbValidChange)
  SetMember(int, MaxDropAsShiftDisturbed)
  SetMember(int, MaxDropAsFocusDisturbed)
  SetMember(int, MinFitXAfterDrop)
  SetMember(int, MinFitYAfterDrop)
  SetMember(int, MinFitZAfterDrop)
  GetSetMember(int, AutoTerminatePolicy)
  GetSetMember(int, TiltFailPolicy)
  GetSetMember(int, DimRetryPolicy)
  GetSetMember(int, LDRecordLossPolicy)
  GetSetMember(int, LDDimRecordPolicy)
  GetSetMember(int, TermNotAskOnDim)
  GetSetMember(int, SendEmail)
  GetMember(int, LastSucceeded)
  GetMember(BOOL, TiltedToZero)
  GetMember(BOOL, Postponed)
  GetSetMember(float, MessageBoxValveTime)
  GetSetMember(BOOL, MessageBoxCloseValves)
  GetSetMember(BOOL, AutoSavePolicy)
  SetMember(BOOL, ErrorEmailSent)
  GetSetMember(float, ExpSeriesStep)
  GetSetMember(BOOL, ExpSeriesFixNumFrames);
  GetMember(BOOL, ChangeRecExp);
  GetSetMember(BOOL, SkipBeamShiftOnAlign);
  SetMember(int, MaxDelayAfterTilt);
  GetMember(CString, BidirAnchorName);
  GetSetMember(BOOL, EndOnHighDimImage);
  GetSetMember(int, DimEndsAbsAngle);
  GetSetMember(int, DimEndsAngleDiff);
  GetSetMember(float, MinFieldBidirAnchor);
  GetSetMember(float, AlarmBidirFieldSize);
  GetSetMember(BOOL, EarlyK2RecordReturn);
  GetSetMember(BOOL, CallTSplugins);
  GetMember(int, BidirSeriesPhase);
  GetSetMember(BOOL, AllowContinuous);
  GetSetMember(BOOL, AutosaveXYZ);
  GetMember(int, TiltIndex);
  GetSetMember(int, MacroToRun);
  GetSetMember(BOOL, RunMacroInTS);
  GetSetMember(int, StepAfterMacro);
  GetMember(BOOL, RunningMacro);
  GetMember(BOOL, FrameAlignInIMOD);
  GetSetMember(BOOL, FixedNumFrames);
  GetSetMember(BOOL, TermOnHighExposure);
  GetSetMember(float, MaxExposureIncrease);
  SetMember(bool, CallFromThreeChoice); 
  SetMember(CString, TCBoxYesText);
  SetMember(CString, TCBoxNoText);
  SetMember(CString, TCBoxCancelText);
  SetMember(int, TCBoxDefault);
  SetMember(float, TrialCenterMaxRadFrac);
  double GetCumulativeDose();

  bool GetBidirStartAngle(float &outVal) {outVal = mTSParam.bidirAngle; return mStartedTS && mTSParam.doBidirectional;};

  void BestPredFit(float *x1fit, float *x2fit, float *yfit, int &nFit, int &nDrop,
           int minFit, int minQuadratic, float &slope, float &slope2, float &intcp, 
           float &roCorr, float predX, float &predY, float &stdError);
  BOOL UsableLowMagRef(double angle);
  void NewLowDoseRef(int inBuf);
  TiltSeriesParam *GetTiltSeriesParam() {return &mTSParam;};
  BOOL TerminateOnExit();
  BOOL OKtoBackUpTilt() {return mTiltIndex > 0;};
  void IncActIndex();
  void AssessNextTilt(BOOL preTest);
  void SetFileZ();
  void Terminate();
  void BackUpSeries();
  void Resume();
  int CommonResume(int inSingle, int external, bool stoppingPart = false);
  void EndLoop();
  BOOL IsResumable();
  int FitAndPredict(int nvar, int npnt, float &rsq, int npred);
  void ComputePredictions(double angle);
  void StartBackingUp(int inStep);
  void EndControl(BOOL terminating);
  void ExternalStop(int error);
  void CommonStop(BOOL terminating);
  void ErrorStop();
  void NormalStop();
  BOOL DoingTiltSeries() {return mDoingTS || mBackingUpTilt > 0;};
  BOOL StartedTiltSeries() {return mStartedTS;};
  BOOL SetIntensity(double delFac, double angle);
  BOOL SetIntensityForMean(double angle);
  bool GetTypeVaries(int i) {return mTSParam.doVariations && mTypeVaries[i];};
  void RestoreMag();
  void AcquireLowMagImage(BOOL restoreMag);
  void ClearActionFlags();
  int TiltSeriesBusy();
  void TiltSeriesError(int error);
  void NextAction(int param);
  void FillActOrder(int *inActions);
  void Initialize();

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CTSController)
  //}}AFX_VIRTUAL

  // Implementation
protected:

  // Generated message map functions
  //{{AFX_MSG(CTSController)
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()

private:
  CSerialEMApp * mWinApp;
  CSerialEMDoc *mDocWnd;
  ControlSet * mConSets;
  EMimageBuffer *mImBufs;
  MontParam *mMontParam;
  int *mActiveCameraList;
  FilterParams *mFilterParams;
  CameraParameters *mCamParams;   // For active camera parameters after starting
  LowDoseParams *mLDParam;
  CString *mModeNames;
  CEMscope *mScope;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  CShiftManager *mShiftManager;
  CFocusManager *mFocusManager;
  EMmontageController *mMontageController;
  CComplexTasks *mComplexTasks;
  CMultiTSTasks *mMultiTSTasks;
  CBeamAssessor *mBeamAssessor;
  CProcessImage *mProcessImage;

  TiltSeriesParam mTSParam;   // The master parameter set
  BOOL mDoingTS;              // Flag that series is active
  BOOL mStartedTS;            // Flag that series has been started
  BOOL mPostponed;            // Flag that parameters edited but series not started
  BOOL mVerbose;              // Flag for verbose output
  BOOL mDebugMode;            // Flag for debug output
  BOOL mAutosaveLog;          // Flag to autosave log file after each image save
  BOOL mAutosaveXYZ;          // Flag to save XYZ file after every series
  BOOL mLowDoseMode;          // Program is in low dose mode
  float mStartAngleCrit1;     // Angle above which to assume it is desired start angle
  float mStartAngleCrit2;     // Angle above which to assume same sign for start angle
  float mDefaultStartAngle;   // Default starting angle to offer when not at high tilt
  int mActOrder[MAX_TS_ACTIONS];  //Array with the order for each action in sequence
  int mNumActTotal;           // Total actions in sequence
  int mNumActLoop;            // Actions in regular loop
  int mNumActStartup;         // Actions for startup
  int mActIndex;              // Loop index and state variable
  int mActLoopEnd;            // ending value for loop
  int mSTEMindex;              // Flag for STEM and index into mag settings
  BOOL mMontaging;            // local flag for montaging
  BOOL mActPostExposure;      // local flag, set on each resumption
  ScaleMat mCmat;             // IS to Specimen matrix
  ScaleMat mCinv;             // Specimen to IS
  BOOL mStoreExtra[MAX_EXTRA_SAVES];      // Flags to save extra output
  int mProtectedFiles[MAX_EXTRA_SAVES];  // Indexes of files as protected
  int mExtraFiles[MAX_EXTRA_SAVES];      // Indexes of actual files to save to
  int mSwitchedToMontFrom;    // Flag that switched to mentage for extra record; store #
  int mProtectedStore;        // File number that was protected for main output
  int mCurrentStore;          // Current storage file number
  int mNumExtraRecords;       // Number of extra record images with current parameters
  int mExtraRecIndex;         // Current index of extra records
  BOOL mTookExtraRec;         // Flag that an extra record was taken
  BOOL mInExtraRecState;       // Flag that state was set for an extra record
  EMimageBuffer *mSavedBufForExtra;   // Copy of starting record to be restored at end
  BOOL mWarnedNoExtraFilter;  // Flag that user was told no filtering would happen
  BOOL mWarnedNoExtraChannels; // Flag that user was told no extra channels would be saved
  int mSimultaneousChans[3];  // Channels to be acquired simultaneously
  int mNumSimultaneousChans;  // Number being acquired that way
  int mExtraOverwriteSec;
  int mExtraFreeZtoWrite;
  ShortVec mNumSavedExtras;
  ShortVec mExtraStartingZ;

  int mBidirSeriesPhase;      // Flag and stage number for bidirectional series
  float mStartingTilt;        // Starting and ending angles for whole or partial series
  float mEndingTilt;
  double mDefocusBase;        // Base state before taking extra records
  BOOL mSlitStateBase;
  BOOL mZeroLossBase;
  float mEnergyLossBase;
  float mSlitWidthBase;
  int mSpotSizeBase;
  double mIntensityBase;
  float mExposureBase;
  float mDriftBase;
  int mBinningBase;
  int mChannelIndBase;
  int mSaveShiftsOnAcquire;   // Save user's state
  int mSaveBufToReadInto;
  BOOL mSaveCopyOnSave;
  BOOL mSaveAlignOnSave;
  int mSaveProtectRecord;
  BOOL mSaveAlignToB;
  int mSaveAverageDarkRef;
  float mSaveExposures[MAX_CONSETS];
  float mSaveDrifts[MAX_CONSETS];
  float mSaveFrameTimes[MAX_CONSETS];
  int mSaveContinuous[MAX_CONSETS];
  ShortVec mSaveFrameList;
  float mSaveEnergyLoss;
  float mSaveSlitWidth;
  BOOL mSaveZeroLoss;
  BOOL mSaveSlitIn;
  float mSaveTargetDefocus;
  BOOL mSaveInterpDarkRef;
  int mTypeVaries[MAX_VARY_TYPES];  // Flag that a type of setting varies in series
  float mZeroDegValues[MAX_VARY_TYPES];   // The zero-degree values for variations
  BOOL mChangeRecExp;         // Flags for whether exposures or drifts are changing
  BOOL mChangeAllExp;
  BOOL mChangeRecDrift;
  BOOL mChangeAllDrift;
  BOOL mUseExpForIntensity;   // Flag for whether changing exposure instead of intensity
  BOOL mVaryFilter;           // Flag for whether varying filter settings
  BOOL mVaryFrameTime;        // Flag for whether varying frame time
  BOOL mFrameAlignInIMOD;     // Flag to put out mdoc and com file to align TS frames
  double mCenBeamTimeStamp;   // Time beam center was last done, or start of series
  BOOL mCrossedBeamCenAngle;  // Flag that 
  int mAlignBuf;              // Alignment buffer
  int mReadBuf;               // Read buffer
  int mExtraRefBuf;           // For low mag or other ref
  int mAnchorBuf;             // Buf for anchor
  float mAnchorAngle;         // Actual angle of anchor image
  BOOL mCanFindEucentricity;  // Flag that start was near enough to zero to do eucen.
  BOOL mStartingOut;          // Flag for initial loop
  BOOL mDidWalkup;            // Flag that a walkup was done
  BOOL mWalkupImageUsable;    // Flag that walkup should leave a good reference
  BOOL mFindingEucentricity;  // Flags for all the possible actions that need testing
  BOOL mWalkingUp;
  BOOL mReversingTilt;
  BOOL mCheckingAutofocus;
  BOOL mDoingMontage;
  BOOL mAcquiringImage;
  BOOL mResettingShift;
  BOOL mTilting;
  BOOL mFocusing;
  BOOL mRefiningZLP;
  BOOL mCenteringBeam;        // Flag that autocenter is running
  BOOL mTakingBidirAnchor;    // Flag for taking image to be anchor or align with anchor
  BOOL mInvertingFile;        // Flag that file is being inverted synchronously
  BOOL mRunningMacro;         // Flag that macro was run
  BOOL mDoingPlugCall;        // Flag that a plugin call was started
  BOOL mNeedDeferredSum;      // Flag that early return was taken and deferred sum needed
  BOOL mGettingDeferredSum;   // Flag that a deferred sum is being retrieved
  int mSecForDeferredSum;     // Section in which the sum needs to be saved
  int mDeferredSumStackSec;   // And section in the stack window
  int mWhereDefSumStarted;    // Caller code for starting to get deferred sum
  int mSingleStep;            // -1 for single-step mode or 1 for single-loop
  BOOL mUserStop;             // Set if user requested stop
  BOOL mErrorStop;            // Set if system reports an error
  BOOL mStartupStop;          // Set to do single-loop stop at end of startup
  int mExternalControl;       // Flag that series was started under external control
  int mTerminateOnError;      // Flag to terminate instead of giving message box on error
  BOOL mTermAtLoopEnd;        // Flag to terminate on loop end when stopping
  BOOL mAlreadyTerminated;    // Flag to prevent double termination after error
  BOOL mReachedEndAngle;      // Flag that we are stopping because it is at end angle
  BOOL mFinishEmailSent;      // Flag that an email for being at end was sent
  BOOL mErrorEmailSent;       // Flag that an email was already sent on this error
  BOOL mInStartup;            // Flag that we are in startup sequence
  BOOL mInInitialChecks;      // Flag that we are in initial checks before starting/doing
  BOOL mUserPresent;          // Flag that user seems to be present
  BOOL mTermFromMenu;         // Flag that we are terminating from menu
  BOOL mHaveAnchor;           // flag that there is an anchor in the anchor buffer
  BOOL mHaveStartRef;         // flag that starting reference exists
  BOOL mHaveZeroTiltTrack;    // Flag to align to track image in A before starting
  BOOL mHaveZeroTiltPreview;  // Flag to align to preview image in A before starting
  BOOL mBigRecord;            // Flag that record image is bigger than 2K
  double mOriginalAngle;      // Angle before starting
  int mDirection;             // direction of tilting
  BOOL mTrackLowMag;          // Working flag for low mag tracking with all conditions
  BOOL mNeedLowMagTrack;      // Flag that a low mag tracking shot is needed next round
  BOOL mHaveLowMagRef;        // Flag that a low mag reference exists
  BOOL mUsableLowMagRef;      // Flag that it is actually usable at current tilt
  double mLowMagRefTilt;      // Tilt angle of the low mag ref
  float mMaxUsableAngleDiff;  // Maximum angle difference for a usable low mag ref
  BOOL mInLowMag;             // Flag that we are at low mag
  BOOL mNeedRegularTrack;     // Flag that need a regular track on this round
  BOOL mNeedFocus;            // Flag that need autofocus on this round
  BOOL mExtraRefShifted;      // Flag that low-dose track or low mag ref has been shifted
  int mOverwriteSec;          // Section number to overwrite
  BOOL mStopAtLoopEnd;        // Flag to end at logical end of loop
  double mCurrentTilt;        // Current tilt angle
  BOOL mNeedGoodAngle;        // Flag that angle must be obtained anew
  double mNextTilt;           // Next tilt angle
  double mPoleAngle;          // Angle at which presumed pole touch occurred
  double mShotAngle;          // The actual angle at which record/montage begun
  BOOL mNeedTilt;             // Flag that tilt is (still) needed
  BOOL mPreTilted;            // Tilt was done on record image
  float mBWMeanPerSec[MAX_LOWDOSE_SETS]; // Expected mean of black & white levels, unbinned, /sec
  float mBadShotCrit;         // Criterion for bad shot as fraction of expected mean 
  float mBadLowMagCrit;       // Criterion fraction for low mag shot
  int mImageFailures;         // Number of consecutive image failures
  int mMaxImageFailures;      // Maximum number allowed
  BOOL mCheckFocusPrediction; // Check for whether focus is close enough to prediction
  float mPredictedFocus;      // predicted focus for this tilt
  float mPredictedX;          // predicted X and Y specimen positions
  float mPredictedY;
  float mPredictedZ;          // Z = focus - target
  BOOL mHaveFocusPrediction;  // Flags that predictions exist
  BOOL mHaveXYPrediction;
  int mPositionFailures;      // Number of consecutive failures in Record position
  int mMaxPositionFailures;   // Maximum number allowed
  int mBackingUpTilt;         // Indicator of 2-step process of backing up
  float mMaxTiltError;        // Maximum amount result could be off after a good tilt
  float mLastIncrement;       // Last actual tilt increment
  double mStopStageX;         // Stage position when controller stopped
  double mStopStageY;
  double mStopStageZ;
  double mStopDefocus;        // Actual defocus when stopped
  double mStopTargetDefocus;  // Target when stopped
  double mStopLDTrialDelX;    // Displacement of trial area when stopped
  double mStopLDTrialDelY;
  BOOL mLDTrialWasMoved;      // Flag that area was moved
  int mBaseZ;                 // Starting Z of image file
  int mTiltIndex;             // Index of current tilt before its data are stored
  int mHighestIndex;          // Highest value of tilt index, if backed up already
  int mLastStartedIndex;      // Index when last resumed
  int mLastRedoIndex;         // Index when a record had to be redone
  float mMaxEucentricityAngle;  // Criterion angle for doing eucentricity
  float mTiltAngles[MAX_TS_TILTS];
  float mSpecimenX[MAX_TS_TILTS];
  float mSpecimenY[MAX_TS_TILTS];
  float mSpecimenZ[MAX_TS_TILTS];
  unsigned char mDisturbance[MAX_TS_TILTS];
  float mAlignShiftX[MAX_TS_TILTS];
  float mAlignShiftY[MAX_TS_TILTS];
  BOOL mDidFocus[MAX_TS_TILTS];
  float mTimes[MAX_TS_TILTS];
  float mTrueStartTilt;       // Actual starting tilt angle
  int mMaxDisturbValidChange; // Max disturbances to pass over looking for valid change
  int mMaxDropAsShiftDisturbed;   // Max # of points to drop from X/Y after disturbance
  int mMaxDropAsFocusDisturbed;   // Max # of points to drop from Z after disturbance
  float mLowMagFieldFrac;     // Fraction of tracking field that trips a low mag track
  int mBackingUpZ;            // Z value to load if backing up
  float mStageMovedTolerance; // change in stage position that counts as a user move
  float mUserFocusChangeTol;  // change in focus - target that implies user change
  double mLastLDTrackAngle;   // Angle of last low dose track
  int mNumTrackStack;         // Size of stack for low dose tracking shots
  int mTrackStackBuf;         // First buffer of stack
  float mLastMean;            // Last mean computed for intensity
  BOOL mHaveRecordRef;        // flag that reference for Record autoalign exists
  BOOL mHaveTrackRef;         // flag that reference for LD tracking is to be used
  BOOL mStackedLDRef;         // flag that current reference was already put on stack
  int mRestoredTrackBuf;      // Buffer from which track was restored when backed up
  BOOL mDidRegularTrack;      // Flag that a track was done before record on this round
  BOOL mRoughLowMagRef;       // Flag that we are getting, or got, an unaligned low mag ref
  BOOL mNeedFirstTrackRef;    // Flag to save first track reference for bidir series
  int mMinFitXAfterDrop;      // Minimum to fit if points are to be dropped from fit
  int mMinFitYAfterDrop;
  int mMinFitZAfterDrop;
  float mFitDropErrorRatio;   // Reduction in standard error (>1) needed to drop points
  float mFitDropBackoffRatio; // Max increase from minimum error in adding points back
  BOOL mBeamShutterJitters;   // Flag that DM beam shutter is not OK and times vary
  float mBeamShutterError;    // Amount of error
  BOOL mSkipFocus;            // Flag to skip focus
  double mFirstIntensity;     // C2 intensity for first image of tilt series
  double mStartingIntensity;  // C2 intensity before series started
  double mOriginalIntensity;  // C2 intensity before anything happened
  int mIntensityPolarity;     // +/- 1 for relation between C2 and beam intensity
  double mStartingExposure;    // Corresponding exposure values
  double mFirstExposure;
  float mExpSeriesStep;        // Exposure series setp factor for variations dialog
  BOOL mExpSeriesFixNumFrames; // Flag for fixed number of frames checkbox
  double mDoseSums[5];        // The first one has task total, last one has task on-axis
  BOOL mCloseValvesOnStop;    // Flag to close valves onnext stop
  int mOneShotCloseValves;    // Intermediate flag to make sure valves closed only once
  BOOL mUseNewRefOnResume;    // Flag to use A as reference when manual tracking
  BOOL mDidResetIS;           // Flag that image shift reset was done (needed on 1st cycle)
  int mNeedTiltAfterMove;     // Need to call tilt after stage move: 1 for TASMfield, -1 RT field
  BOOL mDidTiltAfterMove;     // Flag that the routine was run for a tilt
  BOOL mAutocenteringBeam;    // Flag for autocentering one way or another
  double mAutocenStamp;       // Time stamp for last autocentering
  BOOL mDidCrossingAutocen;   // Flag that the autocen for crossing an angle was done
  int mTiltFailPolicy;        // Policies under external control: indexes to Yes, no, cancel
  int mDimRetryPolicy;
  int mLDRecordLossPolicy;
  int mLDDimRecordPolicy;
  int mPolicyID[3];
  int mTermNotAskOnDim;       // Flag to terminate on dim image even when policy is for dialog
  int mAutoTerminatePolicy;   // Overall policy on whether to terminate or do dialog on error
  BOOL mAutoSavePolicy;       // Flag to do autosave AND save to new log
  int mSendEmail;             // Flag to send email at end of series
  double mSetupOpenedStamp;   // Time stamp when setup was last opened
  int mLastSucceeded;         // Flag that last tilt series succeeded: 1 term from menu, 2 term automatically
  BOOL mTiltedToZero;         // Flag that it started a tilt back to zero at end of external series
  float mMessageBoxValveTime; // Time after which to close valves when message box put up 
  BOOL mMessageBoxCloseValves; // Flag to close message box after a time
  CString mFinalReport;       // Report string to go in email
  BOOL mSkipBeamShiftOnAlign;  // Test flag for not shifting beam during autoaligns
  CString mBidirAnchorName;    // Filename for bidirectional anchor
  BOOL mKeepActIndexSame;      // Flag that mActIndex should not be advanced
  int mPartStartTiltIndex;     // tilt index at start of first or second part
  BOOL mEndOnHighDimImage;     // Flag to end first half or whole series if dim image high
  int mDimEndsAbsAngle;        // Absolute angle above which this happens
  int mDimEndsAngleDiff;       // And difference from ending tilt
  BOOL mLastBidirReturnTilt;   // Flag for the last tilt on return
  float mMinFieldBidirAnchor;  // Initial minimum field size for anchor
  float mAlarmBidirFieldSize;  // Size of field below which to warn or alarm user
  BOOL mWalkBackForBidir;      // Flag for whether to return with walk up
  float mStepForBidirReturn;   // Step size when returning in tilt steps
  int mMaxDelayAfterTilt;      // Maximum delay for tilt during TS, in seconds
  BOOL mEarlyK2RecordReturn;   // Flag to return early if possible from Record Dose frac
  BOOL mCallTSplugins;         // Flag to control whether plugins get called
  BOOL mAllowContinuous;       // Flag to allow continuous mode in images
  bool mAnyContinuous;         // Flag if there are any continuous to restore  
  int mMacroToRun;             // Number of macro to run before Record
  BOOL mRunMacroInTS;          // Flag to run the macro
  int mStepAfterMacro;         // Run macro before this step
  int mTableOfSteps[MAX_TSMACRO_STEPS];
  bool mDoRunMacro;            // Flag that macro variables pass tests
  BOOL mFixedNumFrames;        // Flag to keep # of K2 frames fixed as exposure varies
  BOOL mTermOnHighExposure;    // Flag to terminate part or TS if exposure/intensity high
  float mMaxExposureIncrease;  // Maximum ratio of current cumulative change to minimum
  float mAccumDeltaForMean;    // Accumulated change in exposure since start
  float mMinAccumDeltaForMean; // Minimum of accumulated change in exposure
  bool mStoppedForAccumDelta;  // Keep track if stop, only stop once
  bool mCallFromThreeChoice;   // Flag that three-choice box is calling TSMessageBox
  CString mTCBoxYesText;       // Text for two or three buttons
  CString mTCBoxNoText;
  CString mTCBoxCancelText;
  int mTCBoxDefault;           // Default button
  float mTrialCenterMaxRadFrac; // Maximum radius as fraction of diagonal to center with T
  
public:
	void CenterBeamWithTrial();
  void EvaluateExtraRecord();
  int SetExtraRecordState(void);
  void RestoreFromExtraRec(void);
  void SyncParamToOtherModules(void);
  void SyncOtherModulesToParam(void);
  void CopyTSParam(TiltSeriesParam * fromParam, TiltSeriesParam * toParam);
  void CopyParamToMaster(TiltSeriesParam * param, bool sync);
  void CopyMasterToParam(TiltSeriesParam * param, bool sync);
  void TerminateOnError(void);
  int TSMessageBox(CString message, UINT type = MB_OK | MB_ICONEXCLAMATION, 
    BOOL terminate = true, int retval = 0);
  int StartTiltSeries(BOOL singleStep, int external);
  void SendEmailIfNeeded(BOOL terminating);
  void LeaveInitialChecks(void);
  void ChangeExposure(double &delFac, double angle, double limit = 1.e10);
  int ImposeVariations(double angle);
  void SetTrueStartTiltIfStarting(void);
  int MontageMagIntoTSparam(MontParam * montParam, TiltSeriesParam * tsParam);
  int EndFirstPartOnDimImage(double angle, CString message, BOOL &needRedo, bool highExp);
  void ManageZafterNewExtraRec(void);
  void ManageTerminateMenu(void);
  int LookupActionFromText(const char * actionName);
  void WriteTiltXYZFile(CString *inFile);
  void SetupExtraOverwriteSec(void);
  int CheckAndLimitAbsFocus(void);
  int StartGettingDeferredSum(int fromWhere);
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TSCONTROLLER_H__3C25C073_8C9A_4FCE_84A1_7754FB23C28E__INCLUDED_)
