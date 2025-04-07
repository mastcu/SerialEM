// FocusManager.h: interface for the CFocusManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FOCUSMANAGER_H__CA46C954_8EBF_4D29_95A1_2386A5EF747E__INCLUDED_)
#define AFX_FOCUSMANAGER_H__CA46C954_8EBF_4D29_95A1_2386A5EF747E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum FocusTasks {FOCUS_AUTOFOCUS = 1, FOCUS_CALIBRATE, FOCUS_REPORT, FOCUS_EXISTING,
FOCUS_SHOW_CORR, FOCUS_SHOW_STRETCH, FOCUS_CAL_ASTIG, FOCUS_ASTIGMATISM, FOCUS_COMA_FREE};

enum {FOCUS_ABORT_INCONSISTENT = 1, FOCUS_ABORT_ABS_LIMIT, FOCUS_ABORT_DELTA_LIMIT,
FOCUS_ABORT_NEAR_ZERO, FOCUS_ABORT_TOO_MANY_ITERS};

#define MAX_FOCUS_CALIBRATIONS 40
#define MAX_STEMFOC_STEPS  25

#define MAX_FOCUS_Z_LEVELS 31
struct STEMFocusZTable {
  int spotSize;
  int probeMode;
  int numPoints;
  float stageZ[MAX_FOCUS_Z_LEVELS];
  float defocus[MAX_FOCUS_Z_LEVELS];
  float absFocus[MAX_FOCUS_Z_LEVELS];
};

class CFocusManager : public CCmdTarget
{
public:
  BOOL CheckingAccuracy() {return mFCindex >= 0;};
  GetSetMember(float, CheckDelta)
  static void TaskCheckDone(int param);
  static int TaskCheckBusy();
  void CheckDone(int param);
  void CheckAccuracy(int logging);
  BOOL GetAccuracyFactors(float &outPlus, float &outMinus);
  GetMember(float, CurrentDefocus);
  GetMember(BOOL, LastFailed);
  GetMember(int, LastAborted);
  SetMember(float, RequiredBWMean);
  GetMember(float, LastAutofocusDiff);
  GetMember(DWORD, LastDriftImageTime);
  GetMember(float, RatioOfCalSlopes);
  GetMember(float, DistPastEndOfCal);
  void FocusTasksFinished();
  void ModifyAllCalibrations(ScaleMat delMat, int iCam, int direction = -1);
  void TransformFocusCal(FocusTable *inCal, FocusTable *outCal, ScaleMat delMat);
  CArray<FocusTable, FocusTable> *GetFocusTable() {return &mFocTab;};
  GetSetMember(BOOL, Verbose)

  float DefocusFromCal(FocusTable *focCal, float inX, float inY,
                float &minDist);
  int GetFocusCal(int inMag, int inCam, int probeMode, int alpha, FocusTable &focCal);
  void Initialize();
  CFocusManager();
  ~CFocusManager();
  BOOL GetTripleMode() {return mTripleMode;};
  SetMember(BOOL, TripleMode);
  GetMember(float, BeamTilt);
  GetSetMember(float, LMBeamTilt);
  void SetBeamTilt(float inVal, const char *from = NULL);
  void SetNextTiltOffset(double inX, double inY) {mNextXtiltOffset = inX, mNextYtiltOffset = inY;};
  GetMember(float, TargetDefocus)
  GetSetMember(float, DefocusOffset)
  GetSetMember(float, RefocusThreshold)
  GetSetMember(float, AbortThreshold);
  GetSetMember(float, MaxPeakDistanceRatio)
  GetSetMember(float, PadFrac)
  GetSetMember(float, TaperFrac)
  GetSetMember(float, Sigma1)
  GetSetMember(float, Radius2)
  GetSetMember(float, Sigma2)
  SetMember(BOOL, UseOppositeLDArea);
  GetSetMember(BOOL, NormalizeViaView);
  GetSetMember(int, ViewNormDelay);
  GetSetMember(int, PostTiltDelay);
  GetSetMember(BOOL, SFshowBestAtEnd);
  GetSetMember(float, SFVZbacklashZ);
  GetMember(int, TiltDirection);
  GetSetMember(int, DDDminBinning);
  GetSetMember(float, SFcalRange);
  void SetTiltDirection(int inVal);
  void SetSTEMdefocusToDelZ(float in1, float in2) {mSTEMdefocusToDelZ[0] = in1; if (in2) mSTEMdefocusToDelZ[1] = in2;};
  float GetSTEMdefocusToDelZ(int spotSize, int probeMode = -1, double absFocus = -1.);
  int GetSTEMFocusProbeOrIndex();
  CArray<STEMFocusZTable, STEMFocusZTable> *GetSFfocusZtables() {return &mSFfocusZtables;};
  BOOL DoingFocus() {return (mFocusIndex >= 0 || mNumRotavs >= 0);};
  void CalFocusStart(bool doSparse);
  void SetWorkingBeamTilt(int magInd);
  BOOL DoingSTEMfocusVsZ() {return mSFVZindex > -3;};
  void CalFocusData(float inX, float inY);
  void AutoFocusStart(int inChange, int useViewInLD = 0, int iterNum = 1);
  void AutoFocusData(float inX, float inY);
  BOOL FocusReady(int magInd = -1, bool *calibrated = NULL);
  void DetectFocus(int inWhere, int useViewInLD = 0);
  void FocusDone();
  static void TaskFocusDone(int param);
  static void TaskFocusError(int error);
  void StopFocusing();
  void SetEucenAbsFocusParams(double minAbs, double maxAbs, float minDef, float maxDef,
    BOOL useAbs, BOOL testOff) {mEucenMinAbsFocus = minAbs; mEucenMaxAbsFocus = maxAbs;
    mEucenMinDefocus = minDef; mEucenMaxDefocus = maxDef; mUseEucenAbsLimits = useAbs;
    mTestOffsetEucenAbs = testOff;};
  GetMember(double, EucenMinAbsFocus);
  GetMember(double, EucenMaxAbsFocus);
  GetMember(float, EucenMinDefocus);
  GetMember(float, EucenMaxDefocus);
  GetSetMember(BOOL, UseEucenAbsLimits);
  GetMember(BOOL, TestOffsetEucenAbs);
  GetMember(float, LastTargetFocus);
  GetMember(float, LastScopeFocus);

  void SetBaseBeamTilt(double inX, double inY) {mBaseTiltX = inX; mBaseTiltY = inY;};


protected:

  // Generated message map functions
  //{{AFX_MSG(CFocusManager)
  afx_msg void OnCalibrationAutofocus();
  afx_msg void OnFocusAutofocus();
  afx_msg void OnUpdateFocusAutofocus(CCmdUI* pCmdUI);
  afx_msg void OnUpdateMeasureDefocus(CCmdUI* pCmdUI);
  afx_msg void OnFocusDriftprotection();
  afx_msg void OnUpdateFocusDriftprotection(CCmdUI* pCmdUI);
  afx_msg void OnFocusMeasuredefocus();
  afx_msg void OnFocusMovecenter();
  afx_msg void OnUpdateFocusMovecenter(CCmdUI* pCmdUI);
  afx_msg void OnFocusReportshiftdrift();
  afx_msg void OnFocusSetbeamtilt();
  afx_msg void OnFocusSettarget();
  afx_msg void OnFocusSetthreshold();
  afx_msg void OnCalibrationSetfocusrange();
  afx_msg void OnFocusVerbose();
  afx_msg void OnUpdateFocusVerbose(CCmdUI* pCmdUI);
  afx_msg void OnFocusCheckautofocus();
  afx_msg void OnFocusReportonexisting();
  afx_msg void OnUpdateFocusReportonexisting(CCmdUI* pCmdUI);
  afx_msg void OnFocusSetoffset();
  afx_msg void OnUpdateNoTasks(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNoTasksNoSTEM(CCmdUI* pCmdUI);
  afx_msg void OnFocusResetdefocus();
  afx_msg void OnNormalizeViaView();
  afx_msg void OnUpdateNormalizeViaView(CCmdUI* pCmdUI);
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()

private:
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  ControlSet * mConSets;
  CString * mModeNames;
  EMimageBuffer *mImBufs;
  int mMagIndex;
  CameraParameters *mCamParams;
  int mCurrentCamera;
  CEMscope *mScope;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  CShiftManager *mShiftManager;
  CArray<STEMFocusZTable, STEMFocusZTable> mSFfocusZtables;

  int mFocusIndex;          // Counter for triple shots in DetectFocus
  int mNumShots;            // Actual number of shots to do for a detect-focus
  BOOL mTripleMode;         // flag for doing 3 shots for autofocus
  int mFocusWhere;          // Flag for where to send focus shifts
  int mUseViewInLD;         // Flag with which AutofocusTArt and detectFocus called
  int mFCindex;             // Counter for calibration series of calls
  int mFocusMag;            // Mag index for focusing work;
  int mFocusProbe;          // Probe mode for measuring defocus
  int mFocusAlpha;          // Alpha setting
  int mFocusSetNum;         // Control set #
  bool mUsingExisting;      // Flag for any mode using existing images
  float *mFocusBuf[5];
  double mBufTimeStamp[3];  // For storing the time stamp when the image comes in
  float mFCsx, mFCsxy1, mFCsy1, mFCsxsq; // Sums for n-point line fit
  float mFCsxy2, mFCsy2;
  int mNumCalLevels;        // Number of focus levels to get calibration at
  int mSmoothFit;           // Number of points to fit over for smoothing
  float mCalRange;          // Total range over which to calibrate
  float mCalOffset;         // Offset from current focus
  float mFalconCalLimit;    // Limit to assume for Falcon
  float mSparseLowFocus;    // Underfocus end of sparse curve
  float mSparseHighFocus;   // Overfocus end of sparse curve
  float mSparseDelta;       // Interval in sparse region
  float mSparseBTFactor;    // Factor to reduce beam tilt by for sparse region
  int mNumSparseBelow;      // Number to do in underfocus sparse region
  int mNumSparseAbove;      // Number to do in overfocus sparse region
  int mTotalCalLevels;      // Real number of calibration levels
  double mCalDelta;         // amount to change defocus for each trial
  double mCalDefocus;       // Relative defocus during calibration
  float mTargetDefocus;     // Value to set defocus at after autofocus
  float mDefocusOffset;     // Offset to apply when measuring defocus
  double mAppliedOffset;    // Offset that has been applied
  float mCurrentDefocus;    // Measured defocus value
  double mOriginalDefocus;  // Original defocus value, in case it needs to bail out
  int mAutofocusIterNum;    // Iteration number of autofocus
  float mLastAutofocusDiff; // Change appplied in last autofocus
  bool mLastWasOpposite;    // Flag set when a large focus change crosses zero
  float mRatioOfCalSlopes;  // Last ratio of slope at defocus point to slope at zero
  float mDistPastEndOfCal;  // Distance of last defocus past end of cal
  BOOL mNormalizeViaView;   // Flag to normalize through View area in low dose
  int mViewNormDelay;       // Msec delay abetween View and Focus areas
  int mDoChangeFocus;
  float mMaxPeakDistanceRatio;  // Max ratio between distance from target rejecting 0 peak
  float mRefocusThreshold;   // Focus change at which to reiterate
  float mAbortThreshold;     // Focus change at which to apply abort logic when iterating
  int mTiltDirection;        // Tilt direction, 45-degree increments, 0 = X
  float mFracTiltX, mFracTiltY;    // Fraction of each coil to use
  double mBaseTiltX, mBaseTiltY;   // Existing tilts
  float mBeamTilt;
  float mLMBeamTilt;         // Beam tilt for LM, or 0 for same
  float mWorkingBT;          // Value to use in current focus detection
  float mCalSavedBeamTilt;   // Base beam tilt value used in cal
  double mNextXtiltOffset;   // Offset from center of beam tilt on next run
  double mNextYtiltOffset;
  int mPostTiltDelay;        // Delay after tilting, in milliseconds
  float mCTFa[8193];
  FocusTable mNewCal;        // Holding structure while doing new cal
  CArray<FocusTable, FocusTable> mFocTab;  // The table of calibrations
  float mRequiredBWMean;     // Black/white mean per unbinned sec required
  BOOL mLastFailed;          // Flag that last capture did not complete
  int mLastAborted;          // Aborted: 1 if inconsistent, 2 or 3 if exceed abs/rel limit
  float mLastTargetFocus;    // Target for last autofocus
  float mLastScopeFocus;     // And scope value it was set to
  BOOL mVerbose;
  BOOL mAccuracyMeasured;    // Flag that accuracy of focus calibration was measured
  float mPlusAccuracy;       // Measured defocus change over actual change for + change
  float mMinusAccuracy;      // Measured defocus change over actual change for - change
  float mCheckFirstMeasured; // Defocus value measured before changing defocus
  int mCheckLogging;         // Where to print result
  float mCheckDelta;         // Defocus delta value
  float mPadFrac;            // Padding fraction for correlation
  float mTaperFrac;          // Fraction of area to taper inside for correlation
  float mSigma1;             // Low frequency cutoff sigma
  float mRadius2;            // High frequency start of rolloff
  float mSigma2;             // High frequency rolloff sigma
  int mDDDminBinning;        // Minimum total binning/scaling for direct detector images
  float mLastDriftX, mLastDriftY;   // Drift estimates in last focus
  float mLastNmPerSec;       // Drift usefully scaled
  BOOL mDriftStored;         // Flag that drift was stored
  DWORD mLastDriftImageTime; // CameraController time of last image when drift stored
  BOOL mUseOppositeLDArea;   // Flag to use opposite area on next focus
  float mNextMinDeltaFocus;  // Defocus change and absolute focus limits on next autofocus
  float mNextMaxDeltaFocus;
  double mNextMinAbsFocus;
  double mNextMaxAbsFocus;
  float mUseMinDeltaFocus;  // Limits to use on the current autofocus
  float mUseMaxDeltaFocus;
  double mUseMinAbsFocus;
  double mUseMaxAbsFocus;
  double mEucenMinAbsFocus;
  double mEucenMaxAbsFocus;
  float mEucenMinDefocus;
  float mEucenMaxDefocus;
  BOOL mUseEucenAbsLimits;
  BOOL mTestOffsetEucenAbs;
  int mNumNearZeroCorr;     // Number of correlations that wrer near 0 schift
  float mCumulFocusChange;  // Sum of focus changes made through iterations
  int mNumFullChangeIters;  // Number of iterations that made a full change
  float mRFTbkgdStart;      // Frequencies for start and end of background range
  float mRFTbkgdEnd;
  float mRFTtotPowStart;    // Frequency range for measuring total power
  float mRFTtotPowEnd;
  float mRFTfitStart;        // Frequency range for fitting curves to each other
  float mRFTfitEnd;
  int mRFTnumPoints;          // Number of points in spectra
  float mRFTpowers[MAX_STEMFOC_STEPS];      // Total power values
  float mRFTbackgrounds[MAX_STEMFOC_STEPS]; //  Background values
  float mRFTdefocus[MAX_STEMFOC_STEPS];     // relative defocus
  float *mRFTrotavs[MAX_STEMFOC_STEPS];     // The power spectra
  int mNumRotavs;            // Number of spectra stored - also marker for focus active
  int mSFnumCalSteps;        // Number of steps to take calibrating slope
  float mSFcalRange;         // Range over which to calibrate
  double mSFbaseFocus;       // Starting focus for cal or autofocus
  float mSFbacklash;         // Backlash value for applying focus
  int mSFtask;               // To indicate cal versus focus and stage in focus
  int mSFmaxPowerInd;        // Index of curve with max total power
  float *mSincCurve;         // Sinc-like curve
  int mNumSincPts;           // Number of points and nodes
  int mNumSincNodes;
  float mSFnormalizedSlope[2];  // Calibration value for slope in um/um
  int mSFstepLimit;          // Limit on number of coarse steps
  bool mSFcoarseOnly;        // Flag for doing coarse steps only
  float mSFtargetSize;       // Target step is filtering size units
  float mSFmidFocusStep;     // Coarse focus step, ideal value
  float mSFminFocusStep;     // Min and max coarse step sizes
  float mSFmaxFocusStep;
  float mBackupSlope;
  bool mModeWasContinuous;   // To keep track of whether it was in continuous mode
  float mSFmaxTiltedSizeRange; // Maximum rangge of sizes in a tilted strip
  float mSFminTiltedFraction;  // Minimum fraction of image to use when tilted
  EMimageBuffer *mSFbestImBuf;  // To keep a copy of best image for showing at end
  BOOL mSFshowBestAtEnd;        // Flag to show best focused image at end
  double mSFVZbaseFocus;
  double mSFVZbaseZ;
  double mSFVZtestFocus;
  double mSFVZtestZ;
  float mSTEMdefocusToDelZ[2];   // Scaling from a focus value (microns) to actual delta Z
  float mSFVZdefocusToDeltaZ;  // provisional or found value for running routine
  int mSFVZindex;              // Index in process, -2, -1, or actual index
  BOOL mSFVZmovingStage;
  float mSFVZrangeInZ;
  int mSFVZnumLevels;
  STEMFocusZTable mSFVZtable;
  float mSFVZbacklashZ;
  float mSFVZinitialDelZ;

public:
  afx_msg void OnFocusShowexistingcorr();
  int CurrentDefocusFromShift(float inX, float inY, float &defocus, float &rawDefocus);
  int CheckZeroPeak(float * xPeak, float * yPeak, float * peakVal, int binning);
  int GetLastDrift(double & lastX, double & lastY);
  afx_msg void OnUpdateFocusSettarget(CCmdUI *pCmdUI);
  void SetTargetDefocus(float val);
  int RotationAveragedSpectrum(EMimageBuffer * imBuf, float * rotav, float & background,
    float & totPower);
  void CalSTEMfocus(void);
  void StartSTEMfocusShot(void);
  void STEMfocusShot(int param);
  void STEMfocusCleanup(int error);
  void StopSTEMfocus(void);
  bool DoingSTEMfocus() {return mNumRotavs >= 0;};
  int MakeSincLikeCurve(void);
  float GetSFnormalizedSlope(int i) {return mSFnormalizedSlope[i];};
  void SetSFnormalizedSlope(int i, float val) {mSFnormalizedSlope[i] = val;};
  afx_msg void OnUpdateFocusCheckautofocus(CCmdUI *pCmdUI);
  afx_msg void OnUpdateFocusShowexistingcorr(CCmdUI *pCmdUI);
  double StepForSTEMFocus(int multiple);
  int RotavIndexForDefocus(double defocus);
  afx_msg void OnStemFocusVsZ();
  afx_msg void OnUpdateStemFocusVsZ(CCmdUI *pCmdUI);
  void ChangeZandDefocus(double relativeZ, float backlash);
  void FocusVsZnextTask(int param);
  int FocusVsZBusy(void);
  void FocusVsZcleanup(int error);
  void StopFocusVsZ(void);
  int FindFocusZTable(int spotSize, int probeMode);
  float LookupZFromAbsFocus(int tableInd, float absFocus);
  float LookupDefocusFromZ(int tableInd, float Z);
  afx_msg void OnFocusSetTiltDirection();
  afx_msg void OnAutofocusListCalibrations();
afx_msg void OnUpdateFocusUseAbsoluteLimits(CCmdUI *pCmdUI);
afx_msg void OnFocusUseAbsoluteLimits();
afx_msg void OnFocusSetAbsoluteLimits();
  int LookupFocusCal(int magInd, int camera, int direction, int probeMode, int alpha, bool matchNoAlpha);
  void NextFocusAbsoluteLimits(double min, double max) {mNextMinAbsFocus = min; mNextMaxAbsFocus = max;};
  void NextFocusChangeLimits(float min, float max) {mNextMinDeltaFocus = min; mNextMaxDeltaFocus = max;};
  afx_msg void OnFocusSetDddMinBinning();
  afx_msg void OnFocusSetFilterCutoffs();
  afx_msg void OnShowExistingStretch();
float EstimatedBeamTiltScaling(void);
afx_msg void OnUpdateLimitOffsetDefocus(CCmdUI *pCmdUI);
afx_msg void OnLimitOffsetDefocus();
afx_msg void OnExtendedAutofocus();
afx_msg void OnSetExtendedRange();
};

#endif // !defined(AFX_FOCUSMANAGER_H__CA46C954_8EBF_4D29_95A1_2386A5EF747E__INCLUDED_)
