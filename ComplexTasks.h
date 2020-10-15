// ComplexTasks.h: interface for the CComplexTasks class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COMPLEXTASKS_H__83D1F77E_0AB8_47A4_B1C7_C4C29586A770__INCLUDED_)
#define AFX_COMPLEXTASKS_H__83D1F77E_0AB8_47A4_B1C7_C4C29586A770__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "EMscope.h"

#define FIND_EUCENTRICITY_COARSE   1
#define FIND_EUCENTRICITY_FINE     2
#define REFINE_EUCENTRICITY_ALIGN  4
#define MAX_FINE_STEPS       10
#define MAX_MAG_STACK      4

class CComplexTasks : public CCmdTarget
{
 public:
  BOOL DoingTiltAfterMove() {return mDoingTASM;};
  GetSetMember(float, MinTASMField)
  void StopTiltAfterMove();
  void TASMNextTask(int param);
  void TASMCleanup(int error);
  void TiltAfterStageMove(float angle, bool reverseTilt);
  BOOL DoingBacklashAdjust() {return mDoingBASP > 0;};
  GetSetMember(float, MinBASPField);
  void StopBacklashAdjust();
  void BASPNextTask(int param);
  void BASPCleanup(int error);
  GetSetMember(float, MinTaskExposure)
  GetSetMember(BOOL, RSRAUseTrialInLDMode)
  GetSetMember(BOOL, WalkUseViewInLD);
  GetSetMember(BOOL, Verbose)
  GetSetMember(float, RSRAUserCriterion)
  double GetAndClearDose();
  void StartCaptureAddDose(int conSet);
  BOOL InLowerMag();
  int FindMaxMagInd(float inField, int curMag = -1);
  GetSetMember(float, MinLMSlitWidth)
  GetSetMember(float, SlitSafetyFactor)
  GetSetMember(float, MinWalkField)
  GetSetMember(int, MaxRSRAIterations)
  GetSetMember(float, RSRACriterion)
  GetSetMember(float, RSRAHigherMagCriterion)

  GetSetMember(float, MaxWalkInterval)
  GetSetMember(float, MinWalkInterval)
  GetSetMember(float, WalkShiftLimit)
  GetSetMember(float, WULowDoseISLimit)
  GetSetMember(float, WalkTarget)

  GetSetMember(float, MinRSRAField)
  GetSetMember(float, MinRTField)
  GetSetMember(float, MinFECoarseField)
  GetSetMember(float, MinFEFineField)
  GetSetMember(float, MinFEFineAlignField)

  GetSetMember(float, FEBacklashZ)
  GetSetMember(double, FEInitialAngle)
  GetSetMember(double, FEInitialIncrement)
  GetSetMember(double, FEResetISThreshold)
  GetSetMember(double, FEMaxTilt)
  GetSetMember(double, FEMaxIncrement)
  GetSetMember(float, FETargetShift)
  GetSetMember(float, FEMaxIncrementChange)
  GetSetMember(int, FEIterationLimit)
  GetSetMember(double, FEMaxFineIS)
  SetMember(BOOL, SkipNextBeamShift);
  GetSetMember(BOOL, FEUseTrialInLD)
  GetMember(BOOL, WalkDidResetShift)
  GetMember(BOOL, LastWalkCompleted);
  GetSetMember(float, ZMicronsPerDialMark);
  GetMember(float, LastAxisOffset);
  GetMember(bool, HitachiWithoutZ);
  GetSetMember(float, StageTimeoutFactor);
  GetMember(double, OnAxisDose);
  GetSetMember(float, WalkSTEMfocusInterval);
  GetSetMember(int, EucenRestoreStageXY);
  GetMember(int, LowMagConSet);
  GetSetMember(BOOL, UseTrialSize);
  void GetBacklashDelta(float &deltaX, float &deltaY) {deltaX = mBASPDeltaX; deltaY = mBASPDeltaY;};

  float GetTiltBacklash() {return mRTThreshold;};
  void SetTiltBacklash(float inVal) {mRTThreshold = inVal;};
  BOOL DoingComplexTasks();
  BOOL DoingTasks();
  void EucentricityFineCapture();
  void DoubleMoveStage(double finalZ, float backlashZ, BOOL doZ, double finalTilt,
                       float backlashTilt, BOOL doTilt, int nextAction);
  void RestoreMagIfNeeded();
  void LowerMagIfNeeded(int maxMagInd, float calIntSafetyFac, float intZoomSafetyFac,
    int conSetNum);
  void MakeTrackingConSet(ControlSet *theSet, int targetSize, int baseConset = TRIAL_CONSET);
  BOOL DoingEucentricity() {return mDoingEucentricity;};
  void StopEucentricity();
  void EucentricityCleanup(int error);
  void EucentricityNextTask(int param);
  void FindEucentricity(int coarseFine);
  BOOL ReversingTilt() {return mReversingTilt;};
  void StopReverseTilt();
  void ReverseTiltCleanup(int error);
  void ReverseTiltNextTask(int param);
  BOOL ReverseTilt(int inDirection);
  BOOL DoingWalkUp() {return mWalkIndex >= 0;};
  void StopWalkUp();
  void WalkUpCleanup(int error);
  static int TaskWalkUpBusy();
  void WalkUpNextTask(int param);
  BOOL WalkUp(float targetAngle, int anchorBuf, float anchorAngle);
  BOOL DoingResetShiftRealign() {return mDoingRSRA;};
  void StopResetShiftRealign();
  void Initialize();
  void RSRACleanup(int error);
  static int TaskRSRABusy();
  void RSRANextTask(int param);
  void ResetShiftRealign();
  CComplexTasks();
  virtual ~CComplexTasks();

 protected:

  // Generated message map functions
  //{{AFX_MSG(CComplexTasks)
  afx_msg void OnReverseTilt();
  afx_msg void OnWalkup();
  afx_msg void OnResetRealign();
  afx_msg void OnTasksFinerealign();
  afx_msg void OnTasksVerbose();
  afx_msg void OnUpdateTasksVerbose(CCmdUI* pCmdUI);
  afx_msg void OnTasksWalkupanchor();
  afx_msg void OnUpdateNoTasksNoTS(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNoTasks(CCmdUI* pCmdUI);
  afx_msg void OnTasksSettiltaxisoffset();
  afx_msg void OnTasksSetiterationlimit();
  afx_msg void OnTasksUsetrialinld();
  afx_msg void OnUpdateTasksUsetrialinld(CCmdUI* pCmdUI);
  //}}AFX_MSG
  afx_msg void OnEucentricity(UINT nID);

  DECLARE_MESSAGE_MAP()

 private:
  CSerialEMApp * mWinApp;
  ControlSet * mConSets;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CTSController *mTSController;
  EMbufferManager *mBufferManager;
  CShiftManager *mShiftManager;
  BOOL mVerbose;
  int mLowMagConSet;              // control used for low mag tracking shots
  BOOL mUseTrialSize;             // Flag to use current trial size instead of full-field

  float mMinRSRAField;            // Minimum field size for reset-realign
  int mSavedMagInd[MAX_MAG_STACK];         // Mag at which procedure started
  double mSavedIntensity[MAX_MAG_STACK];   // Intensity at start if changed by beam calibrations
  BOOL mUsersIntensityZoom[MAX_MAG_STACK]; // Whether user has intensity zoom on
  float mSavedExposure[MAX_MAG_STACK];     // Exposure time prior to change
  float mSavedSlitWidth[MAX_MAG_STACK];    // Saved slit width, in case had to open
  int mConSetModified[MAX_MAG_STACK];      // Control set whose exposure was saved
  float mCumulSafetyFactor;       // Cumulative intensity change safety factor
  float mMinTaskExposure;         // Minimum exposure time to use when dropping it
  int mMagStackInd;               // Index to position on mag stack
  float mMinLMSlitWidth;            // Minimum slit width allowed if lowering mag
  float mSlitSafetyFactor;        // Extra amount to change intensity if opening slit
  double mTotalDose;               // Accumulate dose here
  double mOnAxisDose;             // And keep a sub-sum of View/Preview dose
  BOOL mSkipNextBeamShift;        // Flag to skip beam shift on align in next RT or TASM

  BOOL mDoingRSRA;
  int mMaxRSRAIterations;         // Maximum number of iterations for reset
  int mRSRAIteration;             // Counter for iterating
  float mRSRACriterion;           // Criterion IS for iterating
  BOOL mRSRAActPostExposure;
  float mRSRAHigherMagCriterion;  // criterion for going one higher mag step
  BOOL mRSRAUseTrialInLDMode;     // Use trial in low dose mode
  BOOL mRSRAWarnedUseTrial;       // Warned user about using trial
  float mRSRAUserCriterion;       // User's criterion for iterating
  BOOL mPretilted;                // flag if tilted in post-actions
  BOOL mNeedTiltAfterReset;       // flag that TASM must be done after reset shift in

  BOOL mDoingTASM;
  float mMinTASMField;            // Minimum field for doing tilt after stage move (0 OK)
  float mTASMAngle;               // Angle to tilt to

  int mDoingBASP;
  float mMinBASPField;            // Minimum field for backlash adjust
  float mBASPBackX, mBASPBackY;   // Backlash to apply
  float mBASPDeltaX, mBASPDeltaY; // For saving starting position and leaving delta values

  int mNumWalkAngles;             // Number of angles
  double mWalkStartAngle;         // Starting angle
  double *mWalkAngles;            // Array for angles
  float mMaxWalkInterval;         // Minimum and maximum
  float mMinWalkInterval;
  float mMinWalkIntDflt;          // Defaults
  float mMaxWalkIntDflt;
  float mMinWalkField;            // Minimum field size for walk up
  float mWalkTarget;              // User's target for walk-up angle
  int mWalkIndex;                 // Index to next angle to tilt to
  BOOL mWUActPostExposure;
  int mWalkAlignBuffer;           // Buffer to use for alignment
  int mWUSavedShiftsOnAcquire;    // Saved value of Shifts on acquire if have to use C
  float mWalkShiftLimit;          // Limit for image shift on specimen
  float mWULowDoseISLimit;        // Limit in low dose mode
  int mWUAnchorBuf;               // Buffer to copy an anchor image to
  int mWUAnchorIndex;             // Index of closest angle to requested one
  BOOL mWUGettingAnchor;          // Flag that anchor was gotten at restored mag
  int mWalkMagInd;                // Actual mag walk is being done at
  int mWalkConSet;                // Control set number used for walkup
  BOOL mWalkDidResetShift;        // Flag that reset shift was done during walkup
  float mWalkSTEMfocusInterval;   // Tilt interval at which to autofocus in STEM
  float mWULastFocusAngle;        // Last angle at which it autofocused
  BOOL mWalkUseViewInLD;          // Flag to use view in low dose
  BOOL mLastWalkCompleted;        // Flag that last walk-up finished

  int mReversingTilt;
  int mMaxRTMagInd;               // Maximum mag for reversing tilt
  float mMinRTField;              // Minimum field size
  double mRTStartAngle;           // Angle to start and end up at
  double mRTReverseAngle;         // Angle to go to and return
  float mRTThreshold;             // Amount needed to eliminate backlash
  BOOL mRTActPostExposure;

  BOOL mDoingEucentricity;
  int mMaxFECoarseMagInd;         // Mag index for coarse eucentricty
  int mMaxFEFineMagInd;           // and fine
  int mMaxFEFineAlignMag;         // fine with alignment
  float mMinFECoarseField;        // Minimum field size for coarse eucentricity
  float mMinFEFineField;          // and fine
  float mMinFEFineAlignField;     // fine with alignment
  double mFEUsersAngle;           // Starting angle for fine align/realign
  float mFEBacklashZ;             // Amount to overshoot Z changes
  double mFEInitialAngle;
  double mFECurrentZ;
  double mFECurrentAngle;
  double mFEReferenceAngle;
  double mFECoarseIncrement;
  double mFEInitialIncrement;
  double mFEResetISThreshold;     // Threshold above which image shift should be reset
  double mFEMaxTilt;              // maximum tilt angle
  double mFEMaxIncrement;         // Maximum increment
  float mFETargetShift;           // Shift to try to reach by tilting
  float mFEMaxIncrementChange;    // Maximum factor to change increment
  int mFECoarseFine;              // Saved flags from external call
  int mFEFineIndex;               // index to current angle
  BOOL mFEActPostExposure;        // flag for using post actions
  double mFETargetAngles[MAX_FINE_STEPS];
  double mFEFineAngles[MAX_FINE_STEPS];
  float mFEFineShifts[MAX_FINE_STEPS];
  float mAngleSines[MAX_FINE_STEPS];
  float mAngleCosines[MAX_FINE_STEPS];
  int mFENumFineSteps;
  BOOL mRepeatFine;               // Flag to repeat fine sequence
  int mFEIterationLimit;          // Allowed iterations of fine sequence
  int mFEIterationCount;          // Number of iterations
  double mFEMaxFineIS;            // Maximum image shift in fine
  float mCumMovedX;
  float mLastAxisOffset;          // Axis offset in last run of fine eucentricity
  BOOL mFEUseTrialInLD;           // Flag to use trial in low dose for fine eucentricity
  BOOL mFEWarnedUseTrial;         // Flag that they were warned
  float mZMicronsPerDialMark;     // Scale on Hitachi scope
  float mManualHitachiBacklash;   // Amount to target for manual adjustment
  float mStageTimeoutFactor;      // Multiplier to idle task timeout for stage moves
  bool mHitachiWithoutZ;          // Flag that it is a Hitachi with no Z
  int mTiltingBack;               // Set to 1 when tilting back, or -1 when done or error
  int mEucenRestoreStageXY;       // 1 to restore XY in fine eucentricity, 2 for both
  double mStageXtoRestore;        // Position to restore
  double mStageYtoRestore;

public:
  afx_msg void OnTasksSetincrements();
  afx_msg void OnTrialInLdRefine();
  afx_msg void OnUpdateTrialInLdRefine(CCmdUI *pCmdUI);
  int EucentricityBusy(void);
  void BacklashAdjustStagePos(float backX, float backY, bool callNav, bool showC);
  afx_msg void OnTasksUseViewInLowdose();
  afx_msg void OnUpdateTasksUseViewInLowdose(CCmdUI *pCmdUI);
  afx_msg void OnUpdateNoTasksNoTSNoHitachi(CCmdUI *pCmdUI);
  void ReportManualZChange(float delZ, const char *roughFine);
};

#endif // !defined(AFX_COMPLEXTASKS_H__83D1F77E_0AB8_47A4_B1C7_C4C29586A770__INCLUDED_)
