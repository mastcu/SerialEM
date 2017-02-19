#pragma once

struct AstigCalib {
  int probeMode;
  int alpha;
  int magInd;
  float defocus;
  float beamTilt;
  ScaleMat astigXmat;
  ScaleMat astigYmat;
  ScaleMat focusMat;
};

struct ComaCalib {
  int probeMode;
  int alpha;
  int magInd;
  float defocus;
  float beamTilt;
  ScaleMat comaXmat;
  ScaleMat comaYmat;
};



class CAutoTuning
{
public:
  CAutoTuning(void);
  ~CAutoTuning(void);
  GetMember(bool, DoingCalAstig);
  GetMember(bool, DoingFixAstig);
  GetMember(bool, DoingComaFree);
  GetSetMember(float, MaxComaBeamTilt);
  GetSetMember(float, CalComaFocus);
  SetMember(int, PostFocusDelay);
  SetMember(int, PostAstigDelay);
  SetMember(int, PostBeamTiltDelay);
  GetSetMember(float, UsersAstigTilt);
  GetSetMember(float, UsersComaTilt);
  GetSetMember(float, AstigToApply);
  GetSetMember(float, AstigBeamTilt);
  GetSetMember(float, AstigCenterFocus);
  GetSetMember(float, AstigFocusRange);
  SetMember(float, AstigIterationThresh);
  CArray <AstigCalib, AstigCalib> *GetAstigCals() {return &mAstigCals;};
  CArray <ComaCalib, ComaCalib> *GetComaCals() {return &mComaCals;};

  bool DoingAutoTune() {return mDoingCalAstig || mDoingFixAstig || mDoingComaFree;};

private:
  CSerialEMApp *mWinApp;
  CEMscope *mScope;
  CFocusManager *mFocusManager;
  CArray <AstigCalib, AstigCalib> mAstigCals;
  CArray <ComaCalib, ComaCalib> mComaCals;
  int mMagIndex;          // Mag of the operation
  ScaleMat mCamToSpec;    // Matrix for converting pixels to nm
  double mSavedBeamTilt;  // Starting values of beam til, defocus, astigmatism
  double mSavedDefocus;
  double mSavedAstigX;
  double mSavedAstigY;
  double mLastAstigX;
  double mLastAstigY;
  int mSavedPostTiltDelay;
  int mSavedTiltDir;
  float mXshift[8][2];
  float mYshift[8][2];
  float mUsersAstigTilt;
  float mAstigToApply;
  float mAstigBeamTilt;
  float mAstigCenterFocus;
  float mAstigFocusRange;
  float mMaxComaBeamTilt;
  float mUsersComaTilt;
  float mCalComaFocus;
  float mInitialDefocus;
  int mOperationInd;
  int mDirectionInd;
  bool mDoingCalAstig;
  bool mDoingFixAstig;
  bool mDoingComaFree;
  bool mCalibrateComa;
  int mPostFocusDelay;
  int mPostAstigDelay;
  int mPostBeamTiltDelay;
  bool mImposeChange;
  float mAstigIterationThresh;
  int mMaxAstigIterations;
  int mNumIterations;
  float mFirstFocusMean;
  float mComaMeanCritFac;

public:
  void CalibrateAstigmatism(void);
  void Initialize(void);
  void GeneralSetup(void);
  void AstigCalNextTask(int param);
  void StopAstigCal(void);
  void AstigCalCleanup(int error);
  void ComaFreeNextTask(int param);
  void StopComaFree(void);
  void ComaFreeCleanup(int error);
  void FixAstigNextTask(int param);
  void StopFixAstig(void);
  void FixAstigCleanup(int error);
  void TuningFocusData(float xShift, float yShift);
  int LookupAstigCal(int probeMode, int alpha, int magInd, bool matchMag = false);
  int LookupComaCal(int probeMode, int alpha, int magInd, bool matchMag = false);
  void PrintMatrix(ScaleMat mat, const char *descrip);
  void CommonStop(void);
  int ComaFreeAlignment(bool calibrate, bool imposeChange);
  int FixAstigmatism(bool imposeChange);
  int SaveAndSetDefocus(float defocus);
  CString TuningCalMessageStart(const char * procedure, const char *ofUpTo, float beamTilt);
  int TestAndSetStigmator(double stigX, double stigY, const char * descrip);
};

