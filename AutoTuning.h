#pragma once

enum {COMA_JUST_MEASURE = 0, COMA_INITIAL_ITERS, COMA_ADD_ONE_ITER, COMA_CONTINUE_RUN};
#define MAX_CAL_CTF_FITS 10

#define MIN_COMA_VS_IS_EXTENT 0.5f
#define MAX_COMA_VS_IS_EXTENT 18.f

#define CTF_SET_EXPOSURE 0x1
#define CTF_SET_DRIFT    0x2
#define CTF_SET_BINNING  0x4
#define CTF_ALIGN_FRAMES 0x8
#define CTF_USE_ALI_SET  0x10
#define CTF_OLD_SETTINGS 0x80

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

struct CtfBasedCalib {
  int magInd;
  bool comaType;
  float amplitude;
  int numFits;
  float fitValues[3 * MAX_CAL_CTF_FITS];
};

struct ComaVsISCalib {
  ScaleMat matrix;
  ScaleMat astigMat;
  int spotSize;
  int magInd;
  int alpha;
  float intensity;
  int probeMode;
  int aperture;
};

class CAutoTuning
{
public:
  CAutoTuning(void);
  ~CAutoTuning(void);
  GetMember(bool, DoingCalAstig);
  GetMember(bool, DoingFixAstig);
  GetMember(bool, DoingComaFree);
  GetMember(bool, DoingCtfBased);
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
  GetSetMember(float, CtfExposure);
  GetSetMember(float, CtfDriftSettling);
  GetSetMember(float, ComaIterationThresh);
  GetSetMember(int, CtfBinning);
  GetSetMember(BOOL, CtfUseFullField);
  GetSetMember(BOOL, CtfDoFullArray);
  GetSetMember(float, TestCtfTuningDefocus);
  GetMember(bool, LastCtfBasedFailed);
  GetSetMember(int, CtfBasedLDareaDelay);
  GetSetMember(float, ComaVsISextent);
  GetSetMember(int, ComaVsISrotation);
  GetSetMember(int, ComaVsISuseFullArray);
  GetSetMember(float, MinCtfBasedDefocus);
  GetSetMember(float, AddToMinForAstigCTF);
  GetMember(float, AstigBacklash);
  GetSetMember(int, BacklashDelay);
  GetSetMember(b3dUInt32, CtfAcqSetFlags);
  GetSetMember(int, NumCtfFrames);
  GetSetMember(int, AlignParamSetNum);
  GetSetMember(BOOL, CtfUseFocusInLD);
  float GetBeamTiltBacklash();
  ComaVsISCalib *GetComaVsIScal() {return &mComaVsIScal;};
  void SetBaseBeamTilt(double inX, double inY) {mBaseBeamTiltX = inX; mBaseBeamTiltY = inY;};

  CArray <AstigCalib, AstigCalib> *GetAstigCals() {return &mAstigCals;};
  CArray <ComaCalib, ComaCalib> *GetComaCals() {return &mComaCals;};
  CArray <CtfBasedCalib, CtfBasedCalib> *GetCtfBasedCals() {return &mCtfBasedCals;};
  bool DoingAutoTune() {return mDoingCalAstig || mDoingFixAstig || mDoingComaFree ||
    mDoingCtfBased || mZemlinIndex >= 0 || mComaVsISindex >= 0;};
  bool DoingZemlin() {return mZemlinIndex >= 0;};
  bool DoingComaVsIS() { return mComaVsISindex >= 0;};
  void GetLastAstigNeeded(float &x, float &y) {x = mLastXStigNeeded; y = mLastYStigNeeded;};
  void GetLastBeamTiltNeeded(float &x, float &y) {x = mLastXTiltNeeded; y = mLastYTiltNeeded;};

private:
  CSerialEMApp *mWinApp;
  CEMscope *mScope;
  CFocusManager *mFocusManager;
  CShiftManager *mShiftManager;
  CArray <AstigCalib, AstigCalib> mAstigCals;
  CArray <ComaCalib, ComaCalib> mComaCals;
  CArray <CtfBasedCalib, CtfBasedCalib> mCtfBasedCals;
  int mMagIndex;          // Mag of the operation
  ScaleMat mCamToSpec;    // Matrix for converting pixels to nm
  ComaVsISCalib mComaVsIScal;
  float mSavedBeamTilt;  // Starting values of beam til, defocus, astigmatism
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
  int mPostAstigDelay;            // Delay after applying stigmator change
  int mPostBeamTiltDelay;         // Delay after applying beam tilt change
  bool mImposeChange;
  float mAstigIterationThresh;    // Threshold for iterating astigmatism
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
  double mCvsISbaseBTX, mCvsISbaseBTY;
  int mComaActionType;
  int mInitialComaIters;
  int mMaxComaIters;
  int mNumComaItersDone;
  double mCumulTiltChangeX, mCumulTiltChangeY;
  int mLastComaMagInd;
  int mLastComaAlpha;
  int mLastComaProbe;
  float mMenuZemlinTilt;

  int mCtfActionType;             // 0 to apply, 1 to measure, 2 to use existing images
  bool mDoingCtfBased;            // Flag that CTF-based operations are occurring
  bool mCtfCalibrating;           // Flag for calibrating
  int mCtfComaFree;               // Flag for coma-free: 2 for full array
  b3dUInt32 mCtfAcqSetFlags;      // Flags for which items to set
  float mCtfExposure;             // User's variables to potentially replace Record
  float mCtfDriftSettling;        // exposure, drift, and binning
  int mCtfBinning;
  BOOL mCtfUseFullField;          // And whether to use full field even if Record isn't
  BOOL mCtfDoFullArray;           // Flag to do 10 image array
  int mNumCtfFrames;              // Number of frames to align
  int mAlignParamSetNum;          // Number of alignment parameter set
  BOOL mCtfUseFocusInLD;          // Flag to use focus area in low dose for initial focus
  float mMinCtfBasedDefocus;      // Absolute min and max focus range for doing fits
  float mMaxCtfBasedDefocus;
  float mMinDefocusForMag;        // Minimum and maximum defocus allowed for this mag
  float mMaxDefocusForMag;
  int mMinRingsForCtf;            // Minimum and maximum number of rings allowed in a CTF
  int mMaxRingsForCtf;            // based on thepixel size at current mag
  float mAddToMinForAstigCTF;     // Amount of extra defocus to require for astig cal/coma
  float mCenteredScore;           // Score of first "centered" shot
  float mMinCenteredScore;        // Minimum score required for first "centered" shot
  float mMinPlotterCenScore;      // Minimum score required for first ctfplotter
  float mCenteredDefocus;         // Defocus determined for that shot
  float mMinScoreRatio;           // Minimum ratio for score from starting one
  float mMinPlotterScoreRatio;    // Minimum ratio for ctfplotter
  CtfBasedCalib mCtfCal;          // The current calibration structure
  float mAstigBacklash;           // Amount of backlash to apply to stigmator
  float mBeamTiltBacklash;        // Amount of backlash to apply for beam tilt
  int mBacklashDelay;             // Delay between setting backlash amount an actual value
  float mLastStageX, mLastStageY, mLastStageZ;  // Position, for testing if focus valid
  int mGotFocusMinuteStamp;       // Time when last focus used (it is renewed)
  float mLastMeasuredFocus;       // Focus last measured with autofocus
  bool mSkipMeasuringFocus;       // Flag to skip the autofocus
  ControlSet mSavedRecSet;        // Record set saved before modification
  float mComaIterationThresh;     // Threshold for iteration CTF-based coma correction
  double mLastBeamX, mLastBeamY;  // Last beam tilt values set
  bool mDoingFullArray;           // Flag that full coma array is being done
  float mTestCtfTuningDefocus;    // Initial focus to assign when reading in images
  bool mLastCtfBasedFailed;       // Flag for failure in last CTF-based operation
  BOOL mSkipMessageBox;
  bool mDoingCTFfallover;         // Flag that switched between CTFplotter and ctffind 
  int mCtfBasedLDareaDelay;       // Delay after changing low dose area
  float mLastXStigNeeded;         // Change needed from last astigmatism measurement only
  float mLastYStigNeeded;         // Can be BTID or CTF
  float mLastXTiltNeeded;         // Change needed from last CTF coma measurement only
  float mLastYTiltNeeded;
  float mComaVsISextent;          // Value for extent and angle saved from the dialog
  int mComaVsISrotation;
  float mCVISextentToUse;         // Values to use during calibration
  int mCVISrotationToUse;
  int mComaVsISuseFullArray;      // Full array: -1 regular choice, 0 not to, 1 to do so
  int mCVISfullArrayToUse;        // Value during calibration
  int mComaVsISindex;
  float mComaVsISAppliedISX[4];
  float mComaVsISAppliedISY[4];
  float mComaVsISXTiltNeeded[4];
  float mComaVsISYTiltNeeded[4];
  float mComaVsISXAstigNeeded[4];
  float mComaVsISYAstigNeeded[4];

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
  void BacklashedBeamTilt(double btX, double btY, bool doBacklash);
  void BacklashedStigmator(double astX, double astY, bool doBacklash);
  int MakeZemlinTableau(float beamTilt, int panelSize, int cropSize);
  void ZemlinNextTask(int param);
  void StopZemlin(void);
  void ZemlinCleanup(int error);
  void ReportMisalignment(const char *prefix, double misAlignX, double misAlignY);
  float DefocusFromCtfFit(float * fitValues, float angle);
  float DefocusDiffFromTwoCtfFits(float * fitValues, int index1, int index2, float angle);
  void AstigCoefficientsFromCtfFits(float * fitValues, float angle, float astig,  float &xCoeff, float &yCoeff);
  int CtfBasedAstigmatismComa(int comaFree, bool calibrate, int actionType, int leaveIS, BOOL noMessageBox);
  void SetRecordConSetForCTF();
  void CtfBasedNextTask(int tparm);
  void ErrorInCtfBased(const char *mess, bool tryFallover);
  void StopCtfBased(bool restore = true, bool failed = true);
  void CtfBasedCleanup(int error);
  int LookupCtfBasedCal(bool coma, int magInd, bool matchMag, float *rotateImage = NULL);
  float ComaSlopeFromCalibration(float *fitValues, int index, float angle,
  float beamTilt, float *factors);
  int SetupCtfAcquireParams(bool fromCheck);
  int CheckAndSetupCtfAcquireParams(const char *operation, bool fromSetup);
  int CalibrateComaVsImageShift(float extent, int rotation, int useFullArray);
  void ComaVsISNextTask(int param);
  void ComaVsISCleanup(int error);
  void StopComaVsISCal(void);
  void GetComaVsISVector(int magInd, float extent, int rotation, int posIndex, float &delISX,
    float &delISY);
};

