#pragma once
#include "NavHelper.h"

enum { WFD_USE_TRIAL, WFD_USE_FOCUS, WFD_BETWEEN_AUTOFOC, WFD_WITHIN_AUTOFOC };

struct DriftWaitParams {
  int measureType;              // Type of image/operation to assess with
  float driftRate;              // Drift rate in nm/sec
  BOOL useAngstroms;            // Show in Angtroms not nm
  float interval;               // Interval between tests in sec
  float maxWaitTime;            // Maximum time to wait
  int failureAction;            // 0 to go on, 1 for error
  BOOL setTrialParams;          // Flag to set trial parameters
  float exposure;               // Trial exposure
  int binning;                  // Trial binning
  BOOL changeIS;                // Flag to apply image shift in each alignment
};

class CParticleTasks
{
public:
  CParticleTasks(void);
  ~CParticleTasks(void);
  bool DoingMultiShot() { return mMSCurIndex > -2; };
  void SetBaseBeamTilt(double inX, double inY) { mBaseBeamTiltX = inX; mBaseBeamTiltY = inY; };

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CComplexTasks *mComplexTasks;
  CShiftManager *mShiftManager;
  CFocusManager *mFocusManager;
  int mMSNumPeripheral;           // Number of shots around the center
  int mMSDoCenter;                // -1 to do center before, 1 to do it after
  float mMSSpokeRad;              // Radius from center to periheral shots
  int mMSIfEarlyReturn;           // 1 to do early return on last shot, 2 always
  int mMSEarlyRetFrames;          // Number of frames in early return
  float mMSExtraDelay;            // Extra delay after image shift
  BOOL mMSSaveRecord;             // Save each image to file
  BOOL mActPostExposure;          // Post-actions enabled
  BOOL mMSAdjustBeamTilt;
  float mMSRadiusOnCam;           // Radius to peripheral shots in camera coords
  double mBaseISX, mBaseISY;      // Starting IS
  double mLastISX, mLastISY;      // Last IS position
  double mBaseBeamTiltX, mBaseBeamTiltY;  // Starting beam tilt
  double mCenterBeamTiltX, mCenterBeamTiltY;  // Beam tilt to set compensation from
  ScaleMat mCamToIS;              // Camera to IS matrix
  int mMSCurIndex;                // Index of shot, or -1 for center before; -2 inactive
  int mMSHoleIndex;               // Current hole index if any
  int mMSNumHoles;                // Number of holes to do
  bool mMSUseHoleDelay;           // Flag to use extra hole delay instead of regular one
  FloatVec mMSHoleISX, mMSHoleISY;  // IS values for all the holes
  int mMSLastShotIndex;           // Last index of multishots in hole
  MultiShotParams *mMSParams;      // Pointer to params from NavHelper
  int mMSTestRun;
  int mMagIndex;                   // Mag index for the run

  DriftWaitParams mWDDfltParams;   // Resident parameters
  DriftWaitParams mWDParm;         // Run-time parameters
  double mWDInitialStartTime;      // Time when task started
  double mWDLastStartTime;         // Time when last acquisition started
  int mWaitingForDrift;            // Flag/counter that we are waiting
  bool mWDSleeping;                // Flag for being in sleep phase
  EMimageBuffer *mWDFocusBuffer;   // Buffer to save last autofocus image in
  BOOL mWDSavedTripleMode;         // Saved value of user setting
  int mWDLastFailed;               // Failure code from last run
  float mWDLastDriftRate;          // Drift rate reached in last run
  bool mWDUpdateStatus;            // Flag to update status pane with drift rate
  
public:
  void Initialize(void);
  int StartMultiShot(int numPeripheral, int doCenter, float spokeRad, float extraDelay,
    BOOL saveRec, int ifEarlyReturn, int earlyRetFrames, BOOL adjustBT, int inHoleOrMulti);
  void SetUpMultiShotShift(int shotIndex, int holeIndex, BOOL queueIt);
  int StartOneShotOfMulti(void);
  void MultiShotNextTask(int param);
  void StopMultiShot(void);
  void MultiShotCleanup(int error);
  bool GetNextShotAndHole(int &nextShot, int &nextHole);
  int GetHolePositions(FloatVec & delIsX, FloatVec & delISY, int magInd, int camera);
  int MultiShotBusy(void);
  bool CurrentHoleAndPosition(int &curHole, int &curPos);
  int WaitForDrift(DriftWaitParams &param);
  void WaitForDriftNextTask(int param);
  void StopWaitForDrift(void);
  void WaitForDriftCleanup(int error);
  int WaitForDriftBusy(void);
  GetMember(int, WaitingForDrift);
  GetMember(int, WDLastFailed);
  GetMember(float, WDLastDriftRate);
  void DriftWaitFailed(int type, CString reason);
  CString FormatDrift(float drift);
  DriftWaitParams *GetDriftWaitParams() {return &mWDDfltParams;};
};

