#pragma once
#include "NavHelper.h"

#define MAX_PERIPHERAL_SHOTS 18

class CZbyGSetupDlg;

enum { WFD_USE_TRIAL, WFD_USE_FOCUS, WFD_WITHIN_AUTOFOC };
enum { DW_FAILED_TOO_LONG = 1, DW_FAILED_AUTOALIGN, DW_FAILED_AUTOFOCUS,
  DW_FAILED_TOO_DIM, DW_FAILED_NO_INFO, DW_EXTERNAL_STOP };

struct DriftWaitParams {
  int measureType;              // Type of image/operation to assess with
  float driftRate;              // Drift rate in nm/sec
  BOOL useAngstroms;            // Show in Angtroms not nm
  float interval;               // Interval between tests in sec
  float maxWaitTime;            // Maximum time to wait
  int failureAction;            // 0 to go on, 1 for error
  BOOL setTrialParams;          // Flag to set trial parameters
  float exposure;               // Trial exposure
  int binning;                  // Trial binning, user-displayed value
  BOOL changeIS;                // Flag to apply image shift in each alignment
  BOOL usePriorAutofocus;       // Flag to skip if just prior autofocus is below threshold
  float priorAutofocusRate;     // Threshold drift rate for previous autofocus
};

struct ZbyGParams {
  int lowDoseArea;          // Area (V or F) or -1 if not low dose
  int magIndex;             // Magnification
  double intensity;         // C2/IA value
  int spotSize;             // Spot size
  int probeOrAlpha;         // Probe mode or alpha
  int camera;               // Camera number
  float focusOffset;        // Consolidated offset for running 
  float beamTilt;           // Beam tilt for autofocus measure
  float targetDefocus;      // Measured defocus in cal/target for running
  double standardFocus;     // Absolute focus value
};

class CParticleTasks
{
public:
  CParticleTasks(void);
  ~CParticleTasks(void);
  bool DoingMultiShot() { return mMSCurIndex > -2; };
  void SetBaseBeamTilt(double inX, double inY) { mBaseBeamTiltX = inX; mBaseBeamTiltY = inY; };
  void SetBaseAstig(double inX, double inY) { mBaseAstigX = inX; mBaseAstigY = inY; };
  void GetLastHoleStagePos(double &stageX, double &stageY) {
    stageX = mMSLastHoleStageX;
    stageY = mMSLastHoleStageY;
  };
  void GetLastHoleImageShift(float &ISX, float &ISY) { ISX = mMSLastHoleISX; ISY = mMSLastHoleISY; };
  CZbyGSetupDlg *mZbyGsetupDlg;
  CArray<ZbyGParams> *GetZbyGcalArray() { return &mZbyGcalArray; };
  GetSetMember(float, ZBGIterThreshold);
  GetSetMember(int, ZBGMaxTotalChange);
  GetSetMember(BOOL, ZbyGUseViewInLD);
  GetSetMember(int, ZbyGViewSubarea);
  GetSetMember(float, MSinHoleStartAngle);
  GetSetMember(int, MSHolePatternType);
  GetMember(int, ZBGMeasuringFocus);
  GetMember(bool, DVDoingDewarVac);
  GetMember(bool, ATLastFailed);
  GetMember(bool, MSLastFailed);
  GetSetMember(int, MSMacroToRun);
  GetSetMember(BOOL, MSRunMacro);
  GetSetMember(BOOL, MSDropPlusFromName);
  GetMember(BOOL, MSRunningMacro);
  FloatVec *GetZBGFocusScalings() { return &mZBGFocusScalings; };
  SetMember(MultiShotParams *, NextMSParams);
  GetSetMember(BOOL, LastHolesWereAdjusted);
  SetMember(int, NextMSUseNavItem);
  GetSetMember(float, MSminTiltToCompensate);
  GetMember(bool, DoingPrevPrescan);

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CComplexTasks *mComplexTasks;
  CShiftManager *mShiftManager;
  CFocusManager *mFocusManager;
  CNavHelper *mNavHelper;
  ControlSet *mRecConSet;
  int mMSNumPeripheral;           // Number of shots around the center, total
  int mMSNumInRing[2];            // Number in each ring
  int mMSDoCenter;                // -1 to do center before, 1 to do it after
  int mMSIfEarlyReturn;           // 1 early return last shot, 2 always, 3 all but first
  int mMSEarlyRetFrames;          // Number of frames in early return
  float mMSExtraDelay;            // Extra delay after image shift
  BOOL mMSSaveRecord;             // Save each image to file
  bool mMSImageReturned;          // Flag that image should be returned to save
  BOOL mActPostExposure;          // Post-actions enabled
  BOOL mMSAdjustBeamTilt;
  bool mMSAdjustAstig;
  int mMSSkipAstigBT;             // Convenient copy of value from NavHelper
  float mMSRadiusOnCam[2];        // Radius to peripheral shots in camera coords
  BOOL mMSUseCustomHoles;         // Flag to do custom holes during acquisition
  BOOL mMSNoCornersOf3x3;         // Flag for cross pattern if 3x3 during acquisition
  BOOL mMSNoHexCenter;            // Flag for no center of hex when acquiring
  double mBaseISX, mBaseISY;      // Starting IS
  double mLastISX, mLastISY;      // Last IS position
  double mBaseBeamTiltX, mBaseBeamTiltY;  // Starting beam tilt
  double mCenterBeamTiltX, mCenterBeamTiltY;  // Beam tilt to set compensation from
  double mBaseAstigX, mBaseAstigY;  // Starting beam tilt
  double mCenterAstigX, mCenterAstigY;  // Beam tilt to set compensation from
  ScaleMat mCamToIS;              // Camera to IS matrix
  int mMSCurIndex;                // Index of shot, or -1 for center before; -2 inactive
  int mMSHoleIndex;               // Current hole index if any
  int mMSNumHoles;                // Number of holes to do
  int mNextMSUseNavItem;          // Index of nav item to use on next multishot
  BOOL mMSDoingHexGrid;           // Flag that it is doing hex array
  BOOL mMSUseHoleDelay;           // Flag to use extra hole delay instead of regular one
  FloatVec mMSHoleISX, mMSHoleISY;  // IS values for all the holes
  IntVec mMSPosIndex;              // Position index of hole being acquired
  int mMSLastShotIndex;           // Last index of multishots in hole
  MultiShotParams *mMSParams;      // Pointer to params from NavHelper
  MultiShotParams *mNextMSParams;  // Temporary pointer to params to use for next run
  bool mMSsaveToMontage;           // Flag to save through montage savePiece
  BOOL mMSDropPlusFromName;        // Flag to drop + character from hole name if UTAPI
  int mMSTestRun;
  int mMagIndex;                   // Mag index for the run
  int mSavedAlignFlag;
  double mMSLastHoleStageX;        // Equivalent stage position of last hole center
  double mMSLastHoleStageY;
  float mMSLastHoleISX, mMSLastHoleISY;   // Simply save positions of last hole center
  float mPeripheralRotation;       // Base rotation angle of peripheral shots
  float mMSDefocusTanFac;          // Tangent of tilt angle times defocus factor if adjust
  ScaleMat mIStoSpec;              // Transform for adjusting defocus
  double mMSBaseDefocus;           // Defocus at center
  int mMSDefocusIndex;             // Index of focus to set
  bool mMSLastFailed;              // Flag if last one did not run to completion
  float mMSinHoleStartAngle;       // Value controlling peripheral rotation
  int mMSNumSepFiles;              // Number of separate files: -1 none, 0 define them
  int mMSFirstSepFile;             // Number of first separate file when created
  int mMSHolePatternType;          // 0 for zigzag, 1 for raster, 2 for spiral
  BOOL mLastHolesWereAdjusted;     // Flag that GetHolePositions applied inverse adjust
  int mMSMacroToRun;               // Number of macro to run instead of Record
  BOOL mMSRunMacro;                // User flag to run the macro
  bool mMSDoStartMacro;            // Runtime flag to do it.
  bool mMSRunningMacro;            // Flag that it is started
  float mMSminTiltToCompensate;    // Minimum tilt above which it will compensate focus

  DriftWaitParams mWDDfltParams;   // Resident parameters
  DriftWaitParams mWDParm;         // Run-time parameters
  double mWDInitialStartTime;      // Time when task started
  double mWDLastStartTime;         // Time when last acquisition started
  int mWaitingForDrift;            // Flag/counter that we are waiting
  bool mWDSleeping;                // Flag for being in sleep phase
  BOOL mWDSavedTripleMode;         // Saved value of user setting
  float mWDRefocusThreshold;       // Saved value of autofocus refocus threshold 
  int mWDLastFailed;               // Failure code from last run
  float mWDLastDriftRate;          // Drift rate reached in last run
  bool mWDUpdateStatus;            // Flag to update status pane with drift rate
  float mWDRequiredMean;           // Required mean for focus or trial shots
  float mWDFocusChangeLimit;       // Change limit to apply in autofocusing
  int mWDLastIterations;           // Iterations in last run

  int mZBGMaxIterations;           // Maximum iterations for doing Z by G
  int mZBGMaxTotalChange;          // Maximum total accumulated change
  float mZBGMaxErrorFactor;        // Maximum implied error in previous move
  float mZBGMinErrorFactor;        // Minimum implied error in previous move
  float mZBGMinMoveForErrFac;      // Minimum previous move for evaluating factor
  int mZBGIterationNum;            // Counter and flag that it is running
  float mZBGIterThreshold;         // Threshold move for stopping the iterations
  float mZBGSkipLastThresh;        // Threshold delta Z for skipping the last move
  BOOL mZbyGUseViewInLD;           // Setting in dialog for using View in Low Dose
  int mZbyGViewSubarea;            // 0 for current area,1/4, 3/8, 1/2
  double mZBGInitialFocus;         // For saving and restoring focus
  float mZBGTargetDefocus;         // Measured defocus to reach
  float mZBGFocusOffset;           // Offset set from the param being used
  double mZBGInitialIntensity;     // For saving and restoring intensity out of low dose
  BOOL mZBGUsingView;              // Flag that View is being used
  int mZBGMeasuringFocus;          // Flag that it is running autofocus, 2 for calibrating
  float mZBGTotalChange;           // Total Z change so far
  float mZBGLastChange;            // Change on last iteration
  bool mZBGFinalMove;              // Flag that it us taking the final move
  CString mZBGErrorMess;           // Error message if there is a an error or bad focus
  WINDOWPLACEMENT mZbyGPlacement;
  CArray<ZbyGParams> mZbyGcalArray;  // Array of calibrations
  BOOL mZBGCalWithEnteredOffset;   // Keep track of "Cal with focus offset" state in dialog
  float mZBGCalOffsetToUse;        // And value of offset
  BOOL mZBGCalUseBeamTilt;         // Keep track of "Cal with beam tilt" state in dialog
  float mZBGCalBeamTiltToUse;      // And value of beam tilt
  ZbyGParams mZBGCalParam;         // Param saved at start of calibration
  int mZBGIndexOfCal;              // Its index if it is an existing one
  int mZBGNumCalIterations;        // Number of iterations to do for cal
  float mZBGSavedOffset;           // Saved offset and beam tilts for autofocus
  float mZBGSavedBeamTilt;
  int mZBGLowDoseAreaSaved;         // Flag that low dose area is saved
  LowDoseParams mZBGSavedLDparam;   // Saved parameters for low dose area
  int mZBGSavedTop, mZBGSavedLeft;  // Saved coordinates from view conset
  int mZBGSavedBottom, mZBGSavedRight;
  float mZBGSavedViewDefocus;       // Saved View offset
  FloatVec mZBGFocusScalings;       // List of measured focus values and scaling factor

  int mATIterationNum;              // Current iteration number in resetting shift
  NavAlignParams mATParams;         // Parameters
  bool mATResettingShift;           // Flag that it is resetting shift
  int mATSavedShiftsOnAcq;          // Saved value for rolling buffers
  bool mATFinishing;                // Flag that it is in the last round of reset
  bool mATLastFailed;               // Failure flag
  BOOL mATDidSaveState;             // Flag that starting state was saved
  int mATsizeX, mATsizeY;           // Size of template, for cropping image

  DewarVacParams mDVParams;         // Params for operation
  bool mDVAlreadyFilling;           // Flag that it is now/already filling
  bool mDVStartedRefill;            // Flag that we started a refill
  bool mDVStartedCycle;             // Flag that we started a buffer cycle
  int mDVWaitForFilling;            // 
  float mDVWaitAfterFilled;         // Amount to wait after the filling
  bool mDVNeedVacRuns;              // Flag that it will need to do call for buffer cycles
  bool mDVNeedPVPCheck;             // Flag that it needs to check PVP
  double mDVStartTime;              // Starting time for a wait
  bool mDVDoingDewarVac;            // Flag routine is working
  BOOL mDVWaitForPVP;               // Flag to test for PVP in busy
  bool mDVUseVibManager;            // Flag to use vibration manager
  bool mDVSetAutoVibOff;            // Flag that auto vibration management turned off
  bool mDVSawFilling;               // Flag that filling occurred during prepare to avoid
  bool mDoingPrevPrescan;

public:
  void Initialize(void);
  int StartMultiShot(int numPeripheral, int doCenter, float spokeRad, int numSecondRing, float spokeRad2, float extraDelay,
    BOOL saveRec, int ifEarlyReturn, int earlyRetFrames, BOOL adjustBT, int inHoleOrMulti);
  int StartMultiShot(MultiShotParams *msParams, CameraParameters *camParams, int testValue);
  void SetUpMultiShotShift(int shotIndex, int holeIndex, BOOL queueIt);
  int StartOneShotOfMulti(void);
  void MultiShotNextTask(int param);
  void StopMultiShot(void);
  void MultiShotCleanup(int error);
  bool GetNextShotAndHole(int &nextShot, int &nextHole);
  int GetHolePositions(FloatVec & delIsX, FloatVec & delISY, IntVec &posIndex, int magInd,
    int camera, int numXholes, int numYholes, float tiltAngle, bool startingMulti = false);
  void AddHolePosition(int ix, int iy, std::vector<double> &fromISX, std::vector<double> &fromISY,
    double xCenISX, double yCenISX, double xCenISY, double ycCenISY, IntVec &posIndex);
  void MakeSpiralPattern(int numX, int numY, IntVec &order);
  void DirectionIndexesForHexSide(int step, int &mainDir, int &mainSign, int &sideDir, int &sideSign);
  void SkipHolesInList(FloatVec &delISX, FloatVec &delISY, IntVec &posIndex,
    unsigned char *skipIndex, int numSkip, int &numHoles, FloatVec *skippedISX = NULL, FloatVec *skippedISY = NULL);
  bool ItemIsEmptyMultishot(CMapDrawItem *item);
  int MultiShotBusy(void);
  bool CurrentHoleAndPosition(CString &strCurPos);
  int OpenSeparateMultiFiles(CString &basename);
  void CloseSeparateMultiFiles();
  int WaitForDrift(DriftWaitParams &param, bool useImageInA,
    float requiredMean = 0., float changeLimit = 0.);
  void WaitForDriftNextTask(int param);
  void StopWaitForDrift(void);
  void WaitForDriftCleanup(int error);
  int WaitForDriftBusy(void);
  GetMember(int, WaitingForDrift);
  GetMember(int, WDLastFailed);
  GetMember(float, WDLastDriftRate);
  GetMember(int, WDLastIterations);
  GetMember(double, WDInitialStartTime);
  void DriftWaitFailed(int type, CString reason);
  CString FormatDrift(float drift);
  DriftWaitParams *GetDriftWaitParams() { return &mWDDfltParams; };
  float GetDriftInterval() { return mWDParm.interval; };
  void StartAutofocus();
  bool DoingZbyG() { return mZBGIterationNum >= 0; };
  int EucentricityFromFocus(int useVinLD);
  void ZbyGNextTask(int param);
  void StopZbyG();
  int ZbyGBusy();
  void ZbyGCleanup(int error);
  void OpenZbyGDialog();
  WINDOWPLACEMENT * GetZbyGPlacement(void);
  void ZbyGSetupClosing();
  ZbyGParams *FindZbyGCalForMagAndArea(int magInd, int ldArea, int camera, int &paramInd,
    int &nearest);
  ZbyGParams *GetZbyGCalAndCheck(int useVinLD, int &magInd, int &ldArea, int &paramInd,
    int &nearest, int &error);
  int GetLDAreaForZbyG(BOOL lowDose, int useVinLD, BOOL &usingView);
  void DoZbyGCalibration(ZbyGParams &param, int calInd, int iterations);
  void PrepareAutofocusForZbyG(ZbyGParams &param, bool saveAndSetLD);
  int AlignToTemplate(NavAlignParams &aliParams);
  int DoingTemplateAlign() { return mATIterationNum >= 0 ? mATIterationNum + 1 : 0; };
  void TemplateAlignNextTask(int param);
  void StopTemplateAlign();
  int TemplateAlignBusy();
  void TemplateAlignCleanup(int error);
  int ManageDewarsVacuum(DewarVacParams &params, int skipOrDoChecks);
  void SetupRefillLongOp(int *longOps, float *hoursArr, int &numLongOps, float hourVal);
  void DewarsVacNextTask(int param);
  void StopDewarsVac();
  int DewarsVacBusy();
  void DewarsVacCleanup(int error);
  int StartPreviewPrescan();
  void StopPreviewPrescan();
};
