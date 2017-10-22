#pragma once
class CParticleTasks
{
public:
  CParticleTasks(void);
  ~CParticleTasks(void);
  bool DoingMultiShot() {return mMSCurIndex > -2;};

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
  float mMSRadiusOnCam;           // Radius to peripheral shots in camera coords
  double mBaseISX, mBaseISY;      // Starting IS
  double mLastISX, mLastISY;      // Last IS position
  ScaleMat mCamToIS;              // Camera to IS matrix
  int mMSCurIndex;                // Index of shot, or -1 for center before; -2 inactive

public:
  void Initialize(void);
  int StartMultiShot(int numPeripheral, int doCenter, float spokeRad, float extraDelay,
    BOOL saveRec, int ifEarlyReturn, int earlyRetFrames);
  void SetUpMultiShotShift(int shotIndex, BOOL queueIt);
  int StartOneShotOfMulti(void);
  void MultiShotNextTask(int param);
  void StopMultiShot(void);
  void MultiShotCleanup(int error);
};

