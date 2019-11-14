#pragma once
#include "EMScope.h"
#include "afxtempl.h"
// Header for MultiTSTasks.cpp

class EMimageBuffer;
class CEMscope;
class CCameraController;
class CShiftManager;
class CBeamAssessor;
class EMbufferManager;
struct StageMoveInfo;
class CTSViewRange;
class CAutocenSetupDlg;

enum { ACTRACK_TO_TRIAL = 1, ACTRACK_START_CONTIN };
enum { VPPCOND_RECORD = 0, VPPCOND_TRIAL, VPPCOND_SETTINGS };

class CMultiTSTasks
{
public:
  CMultiTSTasks(void);
  ~CMultiTSTasks(void);

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CComplexTasks *mComplexTasks;
  CShiftManager *mShiftManager;
  CBeamAssessor *mBeamAssessor;
  EMbufferManager *mBufferManager;

  CookParams *mCkParams;
  CArray<AutocenParams *, AutocenParams *> mAcParamArray;
  int mCkMagInd;          // Saved values of mag, spot, intensity, tilt angle
  int mCkSpot;
  double mCkIntensity;
  double mCkTiltAngle;
  double mDoseStart;      // Starting value of the cumulative dose
  double mDoseTime;       // Time at whoich started waiting
  int mNumDoseReports;    // Number of interim reports
  double mDoseNextReport; // dose/time for next report
  float mMinCookField;    // Minimum field in microns for aligning after cooking
  float mCkISLimit;       // Image shift limit after aligning
  bool mCkDoTilted;       // Flag to do tilted
  BOOL mCkLowDose;        // Save and restore low dose mode
  BOOL mCooking;          // Flags for actions undertaken
  BOOL mAcquiring;
  BOOL mMovingScreen;
  BOOL mWaitingForDose;
  BOOL mTilting;
  BOOL mResettingShift;
  BOOL mLoweredMag;       // Keep track if mag was lowered
  BOOL mDidReset;         // Keep track if already did a reset
  int mDoseTimeout;       // Timeout to set for getting dose
  double mExpectedMin;    // Expected time to set in pane
  int mScreenBusy;        // Values for exit status of dual operations
  int mStageBusy;
  int mCkNumAlignSteps;   // Number of steps in scan of scaling values
  float mCkAlignStep;     // Step size for coarse scan
  int mCkNumStepCuts;     // Number of times to cut step
  int mAcSavedMagInd;     // Saved state values when reopening autocen setup after test
  int mAcSavedSpot;
  int mAcSavedProbe;
  double mAcSavedIntensity;
  int mAcSavedScreen;
  double mAcLDTrialIntensity;  // Saved low dose intensity
  double mAcISX, mAcISY;  // Image shift values before running autocen
  int mAcUseCentroid;    // Flag that current operation should use centroid
  BOOL mAutoCentering;    // Flag for taking image when autocentering
  int mAcPostSettingDelay;  // Delay after setting intensity
  AutocenParams * mAcUseParam;  // Save param for later use
  int mAcSavedLDDownArea;  // Saved value of low dose down area
  float mAcMaxShift;      // maximum shift in microns, reject movement if higher
  float mAcShiftedBeamX;  // Amount of beam shift in microns on camera imposed
  float mAcShiftedBeamY;
  WINDOWPLACEMENT mAutocenDlgPlace;
  WINDOWPLACEMENT mVPPConditionPlace;
  int mRangeConsets[6];   // Consets for range finding, regular and low dose
  BOOL mTrZeroDegConSet;  // Control set for zero-degree shots
  float mMinTrZeroDegField;  // Minimum field size for those shots
  BOOL mTrPostActions;    // Flag to do post-actions
  BOOL mAssessingRange;   // Flag that it is active
  RangeFinderParams *mTrParams;
  int mLowDose;           // as index for params
  BOOL mTrDoingEucen;
  BOOL mTrWalkingUp;
  BOOL mTrTilting;
  BOOL mTrFocusing;
  int mTrDirection;        // -1 for minus, +1 for plus
  double mTrCurTilt;        // Tilt we were supposed to get to or are about to go to
  float mTrLastTilt;       // Last tilt, and the one before that
  float mTrPrevTilt;
  float mTrCenterMean;      // Center mean of last image
  float mTrPreviousMean;    // Center mean of previous image from that
  float mTrMeanCrit;        // Criteria for stopping on change in image center mean
  float mTrPrevMeanCrit;
  float mTrZeroMean;        // Mean of zero-deg shot
  float mTrFirstMeanCrit;   // Criterion for stopping on first picture
  float mTrMaxAngleErr;     // Maximum amount by which angle can fail to reach target
  BOOL mTrTiltNeeded;       // Flag that tilt was needed
  int mStackWinMaxSave;     // Saved value of buffer manager's stack window size limit
  int mMaxTrStackMemory;    // Maximum memeory allowed for stack window
  double mTrISXstart, mTrISYstart;   // Starting image shift and stage position
  double mTrStageX, mTrStageY;
  BOOL mNeedStageRestore;   // flag that a stage restore is needed when returning to zero
  int mShiftsOnAcquireSave; // Saved user's value of buffer rolling
  BOOL mTrDidStopMessage;   // Flag so there is only one message on stopping
  int mTrZeroRefBuf;        // Buffer to keep zero-deg ref in
  float mTrUserLowAngle;    // Low and high angles saved by user or detected by routine
  float mTrUserHighAngle;
  float mTrAutoLowAngle;
  float mTrAutoHighAngle;
  double mTiltRangeLowStamp;   // Tick time stamp when routine finished on minus
  double mTiltRangeHighStamp;  // Tick time stamp when routine finished on plus
  BOOL mEndTiltRange;          // Flag that user wants to end the direction

  // Bidirectional File copy (mBfc) variables
  CString mBfcNameFirstHalf;      // Filename for first half
  KStoreMRC *mBfcStoreFirstHalf;  // Store that it is open on
  CString mBfcAdocFirstHalf;      // Name of mdoc file if any
  int mBfcNumToCopy;              // Number of sections to copy
  int mBfcCopyIndex;              // Index of next section to copy
  int mBfcDoingCopy;              // 1 if doing synchronous, -1 if doing asynchronous
  bool mBfcStopFlag;              // One-shot flag to stop either kind of copy
  IntVec mBfcSectOrder;           // Section numbers to copy in order
  KImageStore *mBfcSortedStore;   // Output file for sorted images
  bool mBfcReorderWholeFile;      // Flag to close dose-symmetric output file when done

  // Bidirection anchor image (mBai) variables
  int mBaiSavedViewMag;           // View mag if used a lower mag for anchor
  double mBaiSavedIntensity;      // View intensity if used a lower mag
  double mBaiISX, mBaiISY;        // Image shift of anchor image
  StageMoveInfo mBaiMoveInfo;     // Stage position, backlash state for returning
  BOOL mSkipNextBeamShift;        // Flag to skip beam shift on align or image shift


  // Phase plate conditioning (mVpp variables)
  VppConditionParams mVppParams;

public:
  CTSViewRange *mRangeViewer;
  void Initialize(void);
  int StartCooker(void);
  void CookerNextTask(int param);
  int TaskCookerBusy(void);
  void CookerCleanup(int error);
  void StopCooker(void);
  GetMember(BOOL, Cooking);
  GetMember(BOOL, AutoCentering);
  GetSetMember(float, MinCookField);
  GetSetMember(float, CkISLimit);
  int *GetRangeConsets() {return &mRangeConsets[0];};
  GetMember(BOOL, AssessingRange)
  GetMember(float, TrUserLowAngle)
  GetMember(float, TrUserHighAngle)
  GetMember(double, TiltRangeLowStamp)
  GetMember(double, TiltRangeHighStamp)
  SetMember(BOOL, EndTiltRange);
  SetMember(BOOL, SkipNextBeamShift);
  GetSetMember(int, CkNumAlignSteps);
  GetSetMember(float, CkAlignStep);
  GetSetMember(int, CkNumStepCuts);
  BOOL BidirCopyPending() {return mBfcCopyIndex >= 0;};
  int DoingBidirCopy() {return mBfcDoingCopy;};
  bool DoingAnchorImage() {return mBaiSavedViewMag >= -1;};
  
  void TiltAndMoveScreen(bool tilt, double angle, bool move, int position);
  int AlignWithScaling(int buffer, bool doImShift, float &scaleMax, float startScale = 0.,
    float rotation = 0.);
  void StartTrackingShot(bool lowerMag, int nextAction);
  void RestoreScopeState(void);
  CArray<AutocenParams *, AutocenParams *> *GetAutocenParams() {return &mAcParamArray;};
  AutocenParams *GetAutocenSettings(int camera, int magInd, int spotSize, int probe,
    double roughInt, bool & synthesized, int &bestMag, int &bestSpot);
  void AddAutocenParams(AutocenParams *params);
  int DeleteAutocenParams(int camera, int magInd, int spot, int probe, double roughInt);
  AutocenParams *LookupAutocenParams(int camera, int magInd, int spot, int probe, double roughInt, 
    int &index);
  void SetupAutocenter();
  void MakeAutocenConset(AutocenParams * param);
  int AutocenterBeam(float maxShift = 0.);
  void AutocenNextTask(int param);
  void AutocenCleanup(int error);
  void StopAutocen(void);
  void TimeToSimplePane(double time);
  void RangeFinder();
  void TiltRangeNextTask(int param);
  int TiltRangeBusy(void);
  void TiltRangeCleanup(int error);
  void StopTiltRange(void);
  void StartZeroDegShot(int nextAction);
  void WalkupOrTilt(void);
  BOOL IsTiltNeeded(float curMean);
  void TiltRangeCapture(float curMean);
  void SetupStageReturnInfo(StageMoveInfo * smi);
  void FinishTiltRange(void);
  bool MeansDropped(float curMean);
  void RestoreFromZeroShot(void);
  void LogStopMessage(CString str);
  void StoreLimitAngle(bool userAuto, float angle);
  bool AutocenParamExists(int camera, int magInd, int probe);
  int SetupBidirFileCopy(int zmax);
  int StartBidirFileCopy(bool synchronous);
  void StopBidirFileCopy(void);
  void CleanupBidirFileCopy(void);
  void ResetFromBidirFileCopy(void);
  void AsyncCopySucceeded(bool restart);
  int BidirAnchorImage(int magInd, BOOL useView, bool alignNotSave, bool setImageShifts);
  void StopAnchorImage();
  void BidirAnchorCleanup(int error);
  void BidirAnchorNextTask(int param);
  int InvertFileInZ(int zmax, float *tiltAngles = NULL, bool synchronous = false);
  void BidirFileCopyClearFlags(void);
  void SetAnchorStageXYMove(StageMoveInfo & smi);
  void TestAutocenAcquire();
  void AutocenTestAcquireDone();
  void AutocenClosing();
  bool AutocenTrackingState(int changing = 0);
  bool AutocenMatchingIntensity(int changing = 0);
  WINDOWPLACEMENT *GetAutocenPlacement(void);
  void VPPConditionClosing(int OKorGo);
  WINDOWPLACEMENT *GetConditionPlacement(void);
  void ShiftBeamForCentering(AutocenParams * param);
  void GetCenteringBeamShift(float &cenShiftX, float &cenShiftY) {
    cenShiftX = mAcShiftedBeamX; cenShiftY = mAcShiftedBeamY;};
  void SetupVPPConditioning();
  void SaveScopeStateInVppParams(VppConditionParams *params);
  void SetScopeStateFromVppParams(VppConditionParams *params);
};
