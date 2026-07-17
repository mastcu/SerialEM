#pragma once
#include "afxwin.h"
#include "NavHelper.h"

class CParallelTSHelper;

#define MIN_NUM_POINTS_TO_FIT_PLANE 4
#define MIN_NUM_POINTS_FOR_PTSADJUST 5

enum { PARALLELTS_ACTION_AUTOFOCUS, PARALLELTS_ACTION_PREVIEW, PARALLELTS_ACTION_ADJUST};

class CParallelTSHelper
{
public:
  CParallelTSHelper(void);
  ~CParallelTSHelper(void);
  void Initialize(void);

private:
  CSerialEMApp *mWinApp;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  CNavHelper *mNavHelper;
  CParallelTSDlg *mParallelTSDlg;
  ParallelTSOptions *mParTSopts;
  ParallelTSOptions mSavedParTSopts;

  double mBaseISX, mBaseISY;      // Starting IS
  double mLastISX, mLastISY;      // Last IS position
  double mBaseBeamTiltX, mBaseBeamTiltY;  // Starting beam tilt
  double mCenterBeamTiltX, mCenterBeamTiltY;  // Beam tilt to set compensation from
  double mBaseAstigX, mBaseAstigY;  // Starting beam tilt
  double mCenterAstigX, mCenterAstigY;  // Beam tilt to set compensation from
  double mRecordStageX, mRecordStageY;
  double mBaseStageX, mBaseStageY;
  float mCenterStageX, mCenterStageY;
  double mCenterISX, mCenterISY;
  double mTiltDuringFit;
  
  bool mAdjustBeamTilt;
  int mMagIndex;
  float mPretilt;
  float mXpitch;

  int mISTargetIter;
  int mStartIndex;
  std::vector<double> mISTargetISX;
  std::vector<double> mISTargetISY;
  FloatVec mISTargetSSX;
  FloatVec mISTargetSSY;
  FloatVec mISTargetDefocus;
  IntVec mISTargetPointIDs;             //Provided map IDs of target points
  IntVec mPreviewMapIDs;
  IntVec mPrevMapSectNums;
  IntVec mSavedTargetIDs;               //Map IDs of target points that were saved
  FloatVec mPrevMapShiftX;
  FloatVec mPrevMapShiftY;
  std::vector<double> mPreRefineISX;
  std::vector<double> mPreRefineISY;
  bool mDoNextShift;
  int mActionAtTarget;
  bool mAtTarget;
  bool mLastActionFailed;
  BOOL mSavedMouseStage;
  float mBaseDefocus;
  bool mDoingISToTargets;
  bool mAlignedToFirstISTarget;
  bool mInitialStateSaved;
  CMapDrawItem *mCurISTargetItem;

  int mAreaMapID;
  int mAreaMapMagInd;
  CString mAreaMapFileName;
  CString mTargetMapFileName;
  float mMappingTilt;
  int mSavedTSparamIndex;

  CMapDrawItem *mParTSitem;
  ParallelTSParam mParTSParam;
  ScaleMat mOldAdjustingXform;
  ScaleMat mAdjustingXform;

public:
  GetMember(CMapDrawItem*, CurISTargetItem);
  GetSetMember(CMapDrawItem*, ParTSitem);
  GetMember(IntVec, ISTargetPointIDs);
  GetMember(IntVec, SavedTargetIDs);
  GetMember(IntVec, PreviewMapIDs);
  GetMember(IntVec, PrevMapSectNums);
  GetMember(std::vector<double>, ISTargetISX);
  GetMember(std::vector<double>, ISTargetISY);
  GetMember(int, AreaMapID);
  GetSetMember(int, SavedTSparamIndex);
  GetMember(ParallelTSParam, ParTSParam);
  GetSetMember(float, Pretilt);
  GetSetMember(float, Xpitch);
  GetMember(int, AreaMapMagInd);
  bool DoingISToTargets() { return mDoingISToTargets; };
  int GetNumSavedTargets() { return (int)mSavedTargetIDs.size(); };
  int ISToTargetsBusy();
  void ClearTargets(bool autofocus);
  void ClearSavedTargets();
  void ISToTargetNextTask(int param);
  int StartShiftToTargets(IntVec targetMapIDs, int actionType);
  void StopParallelTSShift(bool error = false);
  int SaveAreaMap(CString &err);
  int AssessISTargetShiftLimit(IntVec indexVec, IntVec &farInd, 
    IntVec *sortedIndexVec = NULL);
  int AssessPtsToFitPlane(FloatVec &ptsX, FloatVec &ptsY, FloatVec &ptsZ, CString &mess);
  int AssessPtsToFitPlane(IntVec indexVec, IntVec &sortedIndexVec, CString &mess);
  int GetSavedTargetsInNav(IntVec *navInd, IntVec *indices = NULL);
  int ConvertToParTSItem(CString &err, CMapDrawItem *item = NULL);
  int GetTSparamItem(CMapDrawItem *&item);
  void UpdateTSParams();
  bool CanAdjustISVectors(int fromMag, bool multiShot, CString &mess);
  int GetCenterPtID();
  int GetISVectors(int groupID, CString &err);
  int ComputeAdjustingTransform(CString &err);
  void SaveAdjustingTransform();
  void DeleteTargetsFromNav();
  void DeleteTargetMapsFromNav();
  void UpdateSpecAngles(float pretilt, float xPitch);
  int PruneDeletedTargets();

private:
  int ISToNextTarget(int targetID, CString &err);
  void UpdatePlaneParamsInDlg();
  void PauseParallelTSShift();
  int SaveInitialState(CString &err);
  int SaveTargetMap(CString &err, bool &saved);
  int SaveTarget(CString &err);
  int FitPlane(float &pretilt, float &xPitch, float &residual,
    CString &mess);
  void ConvexHullLongAxis(FloatVec ptsX, FloatVec ptsY, float *aspectRatio,
    float *longAxis, float anglePrecision = 1.f);
  int AppendNewTargets(IntVec targetMapIDs, CString &mess);
};