#pragma once

enum {COMA_JUST_MEASURE = 0, COMA_INITIAL_ITERS, COMA_ADD_ONE_ITER, COMA_CONTINUE_RUN};

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
  GetSetMember(int, InitialComaIters);
  GetMember(int, NumComaItersDone);
  GetMember(int, LastComaMagInd);
  GetMember(int, LastComaAlpha);
  GetMember(int, LastComaProbe);
  GetSetMember(float, MenuZemlinTilt);
  CArray <AstigCalib, AstigCalib> *GetAstigCals() {return &mAstigCals;};
  CArray <ComaCalib, ComaCalib> *GetComaCals() {return &mComaCals;};

  bool DoingAutoTune() {return mDoingCalAstig || mDoingFixAstig || mDoingComaFree ||
    mZemlinIndex >= 0;};
  bool DoingZemlin() {return mZemlinIndex >= 0;};

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
  int mZemlinIndex;
  float mZemlinBeamTilt;
  int mPanelSize;
  int mPadSize;
  int mCropPix;
  int  mSpecSize;
  short *mMontTableau;
  short *mSpectrum;
  int mTableauSize;
  double mBaseBeamTiltX, mBaseBeamTiltY;
  int mComaActionType;
  int mInitialComaIters;
  int mMaxComaIters;
  int mNumComaItersDone;
  double mCumulTiltChangeX, mCumulTiltChangeY;
  int mLastComaMagInd;
  int mLastComaAlpha;
  int mLastComaProbe;
  float mMenuZemlinTilt;


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
  int ComaFreeAlignment(bool calibrate, int actionType);
  int FixAstigmatism(bool imposeChange);
  int SaveAndSetDefocus(float defocus);
  CString TuningCalMessageStart(const char * procedure, const char *ofUpTo, float beamTilt);
  int TestAndSetStigmator(double stigX, double stigY, const char * descrip);
  int MakeZemlinTableau(float beamTilt, int panelSize, int cropSize);
  void ZemlinNextTask(int param);
  void StopZemlin(void);
  void ZemlinCleanup(int error);
  void ReportMisalignment(const char *prefix, double misAlignX, double misAlignY);
};

