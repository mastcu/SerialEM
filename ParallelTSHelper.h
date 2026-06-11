#pragma once
#include "afxwin.h"

class CParallelTSHelper;

#define MIN_NUM_POINTS_TO_FIT_PLANE 4

enum { PARALLELTS_ACTION_AUTOFOCUS, PARALLELTS_ACTION_PREVIEW };

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

public:
  GetMember(CMapDrawItem*, CurISTargetItem);
  GetMember(CMapDrawItem*, ParTSitem);
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
  GetMember(int, AreaMapMagInd)
  bool DoingISToTargets() { return mDoingISToTargets; };
  int GetNumSavedTargets() { return (int)mSavedTargetIDs.size(); };
  int ISToTargetsBusy();
  void ClearTargets(bool autofocusOrPreview);
  void ClearSavedTargets();
  void ISToTargetNextTask(int param);
  int ISToNextTarget(int targetIndex);
  int StartShiftToTargets(IntVec targetMapIDs, bool autofocusOrPreview);
  void UpdatePlaneParamsInDlg();
  void PauseParallelTSShift();
  void StopParallelTSShift(bool error = false);
  void ErrorParallelTSShift();
  int SaveAreaMap(CString &err);
  int SaveInitialState();
  int SaveTargetMap(CString &err, bool &saved);
  int SaveTarget(CString &err);
  int FitPlane(float &pretilt, float &xPitch, float &residual,
    CString &mess);
  void ConvexHullLongAxis(FloatVec ptsX, FloatVec ptsY, float *aspectRatio,
    float *longAxis, float anglePrecision = 1.f);
  int AssessISTargetShiftLimit(IntVec indexVec, CString &mess, IntVec *sortedIndexVec = NULL);
  int AssessPtsToFitPlane(int groupID, IntVec &sortedIndexVec, CString &mess);
  int AssessPtsToFitPlane(FloatVec &ptsX, FloatVec &ptsY, FloatVec &ptsZ, CString &mess);
  int AssessPtsToFitPlane(IntVec indexVec, IntVec &sortedIndexVec, CString &mess);
  int AppendNewTargets(IntVec targetMapIDs, CString &mess);
  int GetSavedTargetsInNav(IntVec *navInd, IntVec *indices = NULL);
  int ConvertToParTSItem(CString &err, CMapDrawItem *item = NULL);
  int GetTSparamItem(CMapDrawItem *&item);
  void UpdateTSParams();
  bool CanAdjustISVectors(int fromMag, int toMag, bool multiShot, CString &mess);
  int GetCenterPtID();
};