#pragma once
#include "NavHelper.h"

class CParticleTasks
{
public:
  CParticleTasks(void);
  ~CParticleTasks(void);
  bool DoingMultiShot() {return mMSCurIndex > -2;};
  void SetBaseBeamTilt(double inX, double inY) {mBaseBeamTiltX = inX; mBaseBeamTiltY = inY;}; 

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CComplexTasks *mComplexTasks;
  CShiftManager *mShiftManager;
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
};

