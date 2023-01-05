#pragma once
#include "MagTable.h"
#include <set>

#define MAX_SHIFTS  30
class CEMscope;
struct STEMinterSetShifts;

class CShiftCalibrator
{
public:
  CShiftCalibrator(void);
  ~CShiftCalibrator(void);
  GetSetMember(float, StageCalBacklash)
  GetSetMember(float, MaxLMCalShift)
  GetSetMember(float, MaxCalShift)
  GetSetMember(int,GIFofsFromMag)
  GetSetMember(int, GIFofsToMag)
  GetSetMember(double, GIFofsISX)
  GetSetMember(double, GIFofsISY)
  GetSetMember(BOOL, WholeCorrForStageCal);
  GetSetMember(float, StageCycleX)
  GetSetMember(float, StageCycleY)
  GetSetMember(float, CalISOstageLimit)
  GetSetMember(float, MaxStageCalExtent)
  GetSetMember(BOOL, UseTrialSize)
  GetSetMember(BOOL, UseTrialBinning)
  GetMember(double, ISStimeStampLast);
  SetMember(bool, SkipLensNormNextIScal);
  GetSetMember(int, CalToggleInterval);
  GetSetMember(int, NumCalToggles);
  GetSetMember(float, MinFieldSixPointCal);
  BOOL CalibratingIS() {return mShiftIndex >= 0;};
  void Initialize();
  void CalibrateIS(int ifRep, BOOL calStage, BOOL fromMacro);
  void DoNextShift();
  void ShiftCleanup(int error);
  void ShiftDone();
  void StopCalibrating();
  void CalibrateISoffset(void);
  void CalISoffsetNextMag(void);
  void CalISoffsetDone(BOOL ifStop);
  BOOL CalibratingOffset() {return mCalISOindex >= 0;};

private:
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  ControlSet * mConSets;
  CString * mModeNames;
  CameraParameters *mCamParams;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  FilterParams *mFiltParam;
  CShiftManager *mSM;

  BOOL mActPostExposure;       // Flag for doing  post-actions in current task
  int mMagIndex;
  int mCurrentCamera;
  double mBaseISX, mBaseISY;   // Starting image shift values
  BOOL mFirstCal;              // Flag that there is no previous cal data
  ScaleMat mFirstMat;          // Matrix from first calibration
  bool mSixPointIScal;         // Flag for doing 6-point instead of 9-point cal
  float mMinFieldSixPointCal;  // Min field size for using 6-point cal
  bool mCalIsHighFocus;        // Calibration is being done for high defocus
  float mHighDefocusForIS;     // Focus applied for high defocus IS cal
  int mLastISFocusTimeStamp;   // Time when last hi focus IS cal was done
  float mLastISZeroFocus;      // Value of 0 focus at that time
  bool mWarnedHighFocusNotCal; // Flag the warning was given about base cal not done
  int mCalIndex;               // Mag index
  int mShiftIndex;             // Index into calibration list
  int mNumShiftCal;            // Number of images to do for calibration
  double mCalOutX[MAX_SHIFTS];          // IS coordinates for calibration
  double mCalOutY[MAX_SHIFTS];
  int mCalCamSizeX;            // Camera size during calibration
  int mCalCamSizeY;
  float mMaxCalShift;          // Upper limit for shift when calibrating
  float mMaxLMCalShift;        // Upper limit for shift when calibrating in low mag
  float mCalDelayFactor;       // Factor for scaling delays in calibration
  int mCalBinning;             // binning for calibration
  BOOL mCalStage;              // Flag that we doing stage instead of IS cal
  BOOL mCalMovingStage;        // Flag for alternating between stage moves and shots
  float mStageCalBacklash;     // Backlash amount for stage calibrating
  int mNumStageShiftsX;        // Number of shifts to do in X and Y
  int mNumStageShiftsY;
  float mStageCycleX;          // Cycle length of stage variations, which defines
  float mStageCycleY;          // optimal range over which to calibrate stage
  float mMaxStageCalExtent;    // Maximum extent of stage calibration
  BOOL mWholeCorrForStageCal;  // Use whole-image correlation for stage cals
  int mBaseShiftX[MAX_SHIFTS];          // Expected pixel shifts in calibrating
  int mBaseShiftY[MAX_SHIFTS];
  float mTotalShiftX[MAX_SHIFTS], mTotalShiftY[MAX_SHIFTS];  // Pixel shifts found
  float mCTFa[8193];           // CTF for autoalign
  float mCCCmin, mCCCsum;      // Minimum and sum of CCC's
  std::set<int> mMagsTransFrom;  // List of mags that IS cal was transferred from
  BOOL mFocusModified;         // Only modify focus cals once after a request to do so
  int mCalISOindex;            // Index of image shift offset calibration
  double mCalISX[MAX_MAGS];    // Image shift values recorded during calibration steps
  double mCalISY[MAX_MAGS];
  double mCalISOstageX, mCalISOstageY, mCalISOstageZ;    // Stage position at start,
  int mGIFofsFromMag;          // Regular mag at which GIF offset measured
  int mGIFofsToMag;            // EFETM mag at which GIF offset measured
  double mGIFofsISX;           // Offset
  double mGIFofsISY;
  int mZeroOffsetMode;         // Mode (EFTEM or regular) where one offset should be zero
  BOOL mSavedMouseStage;       // Saved value of whether mouse shift moves stage
  float mCalISOstageLimit;     // Limit on stage movement during IS offset cal
  BOOL mTakeTrial;             // Take trial on each new mag
  int mFirstGIFCamera;         // Actual not active camera numbers
  int mFirstRegularCamera;
  int mLowestISOmag;           // Lowest mag index at which to do IS offset cal
  BOOL mUseTrialSize;          // Use Trial size for shift calibrations
  BOOL mUseTrialBinning;       // Use Trial binning for shift calibrations
  BOOL mFromMacro;             // Flag to suppress messages, running from macro
  int mISSindex;               // Index in interset shift measurement
  STEMinterSetShifts *mISSshifts;  // Pointer to the shifts
  int mISSmagAllShotsSave;     // Saved user's setting of mag All shots
  int mISSsetLast;             // Last set it was done on
  double mISStimeStampLast;     // Time stamp of that image
  double mISSscaleLast;        // Scale and binning used
  int mISSbinningLast;
  bool mSkipLensNormNextIScal; // Flag to skip lens normalization on next IS cal
  bool mGaveFocusMagMessage;
  int mCalToggleInterval;      // MSec interval between toggling images
  int mNumCalToggles;          // Number of times to toggle

public:
	void SetFirstCameras();
  BOOL CheckStageGetShift(void);
  void CalibrateEFTEMoffset(void);
  void ConvertIS(int fromCam, int fromMag, double fromISX, double fromISY, int toCam,
    int toMag, double &toISX, double &toISY);
  int GetZeroOffsetMode(void);
  void AdjustNonzeroMode(int mode);
  int MagRangeWithoutIScal(int camera);
  BOOL BothModesHaveIScal(void);
  void OptimizeStageSteps(float range, double &xOut, int & numCal);
  float MatrixMaximum(ScaleMat *mat);
  int MeasureInterSetShift(int fromTS);
  void InterSetShiftNextTask(int param);
  void StopInterSetShift(void);
  void InterSetShiftCleanup(int error);
  BOOL DoingInterSetShift() {return mISSindex >= 0;};
  void SaveLastInterSetShift(void);
  void ReviseOrCancelInterSetShifts(void);
  bool MatrixIsAsymmetric(ScaleMat *mat, float symCrit);
  void CalibrateHighDefocus(void);
  int CalibrateISatHighDefocus(bool interactive, float curFocus);
  void FinishHighFocusIScal();
};
