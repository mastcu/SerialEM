// ShiftManager.h: interface for the CShiftManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SHIFTMANAGER_H__FF4BB03C_5BFC_4142_BD0F_9A1A257FABE9__INCLUDED_)
#define AFX_SHIFTMANAGER_H__FF4BB03C_5BFC_4142_BD0F_9A1A257FABE9__INCLUDED_

//#include "MontageParam.h" // Added by ClassView
#include "MagTable.h"   // Added by ClassView
#include "EMscope.h"
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_IS_DELAYS 30
#define MAX_REL_ROT 10

#define NOTRIM_LOWDOSE_TS     1
#define NOTRIM_LOWDOSE_ALL    2
#define NOTRIM_TASKS_TS       4
#define NOTRIM_TASKS_ALL      8
#define NOTRIM_REALIGN_ITEM  16


// Globals
ScaleMat MatMul(ScaleMat aa, ScaleMat bb);
ScaleMat MatInv(ScaleMat aa);

// Structure for keeping transforms that align rotated images, for deriving stretch
struct RotStretchXform {
  int camera;
  int magInd;
  ScaleMat mat;
};

// Structure for keeping measurements of shifts between sets
struct STEMinterSetShifts {
  int binning[5];
  int magInd[5];
  int sizeX[5];
  int sizeY[5];
  float exposure[5];
  float shiftX[5];
  float shiftY[5];
};

struct HighFocusMagCal {
  int spot;
  float defocus;
  double intensity;
  int probeMode;
  float scale;
  float rotation;
  double crossover;
  int measuredAperture;
};

class CShiftManager
{
public:
  GetMember(int, NumBeamShiftCals)
  ScaleMat *GetBeamShiftMatrices() {return &mIStoBS[0];};
  ShortVec *GetBeamCalMagInd() {return &mBeamCalMagInd;};
  ShortVec *GetBeamCalAlpha() {return &mBeamCalAlpha;};
  ShortVec *GetBeamCalProbe() {return &mBeamCalProbe;};
  GetSetMember(BOOL, InvertStageXAxis)
  GetSetMember(int, StageInvertsZAxis)
  void ResetAllTimeouts();
  ScaleMat CameraToIS(int magInd);
  ScaleMat CameraToSpecimen(int magInd);
  GetSetMember(BOOL, TrimDarkBorders)
  BOOL CanTransferIS(int magFrom, int magTo, BOOL STEMcamera = false);
	void TransferGeneralIS(int fromMag, double fromX, double fromY, int toMag, double &toX,
    double &toY, int toCam = -1);
  double TransferImageRotation(double fromAngle, int fromCam, int fromMag,
                               int toCam, int toMag);
  void SetBeamShiftCal(ScaleMat inMat, int inMag, int inAlpha, int inProbe);
  ScaleMat GetBeamShiftCal(int magInd, int inAlpha = -3333, int inProbe = -1);
  float PredictISDelay(double ISX, double ISY);
  float ComputeISDelay(double delX, double delY);
  ScaleMat FallbackSpecimenToStage(double adjustX, double adjustY);
  ScaleMat SpecimenToStage(double adjustX, double adjustY);
  double RadialShiftOnSpecimen(double inISX, double inISY, int inMagInd);
  UINT GetAdjustedTiltDelay(double tiltChange);
  BOOL ResettingIS() {return mResettingIS || mStartedStageMove;};
  void ResetISDone();
  void ResetISCleanup(int error);
  GetSetMember(float, TiltDelay)
  GetSetMember(float, RegularShiftLimit)
  GetSetMember(float, LowMagShiftLimit)
  GetSetMember(BOOL, MouseMoveStage)
  GetSetMember(float, MouseStageThresh)
  GetSetMember(float, MouseStageAbsThresh)
  GetMember(BOOL, ShiftingDefineArea)
  GetSetMember(float, TrimFrac)
  GetSetMember(float, TaperFrac)
  GetSetMember(float, Sigma1)
  GetSetMember(float, Radius2)
  GetSetMember(float, Sigma2)
  GetSetMember(int, NormalizationDelay)
  GetSetMember(int, LowMagNormDelay);
  GetSetMember(int, StageMovedDelay)
  GetSetMember(float, DefocusZFactor)
  SetMember(int, StageDelayToUse);
  int ResetImageShift(BOOL bDoBacklash, BOOL bAdjustScale, int waitTime = 5000,
    float relaxation = 0.);
  float *GetISmoved() {return &mISmoved[0];};
  float *GetISdelayNeeded() {return &mISdelayNeeded[0];};
  SetMember(int, NumISdelays)
  GetSetMember(float, DelayPerMagDoubling)
  void SetISTimeOut(float inDelay);
  void SetGeneralTimeOut(UINT inTime);
  void SetGeneralTimeOut(UINT inTicks, int interval);
  UINT GetGeneralTimeOut(int whichSet);
  float GetLastISDelay();
  void EndMouseShifting(int index);
  void StartMouseShifting(BOOL shiftPressed, int index);
  ScaleMat IStoGivenCamera(int inMagInd, int inCamera);
  double GetImageRotation(int inCamera, int inMagInd);
  int AutoAlign(int bufIndex, int inSmallPad, BOOL doImShift = true, BOOL showCor = false,
    float *peakVal = NULL, float expectedXshift = 0., float expectedYshift = 0.,
    float conical = 0., float scaling = 0., float rotation = 0., float *CCC = NULL,
    float *fracPix = NULL, BOOL trimOutput = true, float *xShiftOut = NULL,
    float *yShiftOut = NULL, float probSigma = 0.);
  void AutoalignCleanup();
  BOOL MemoryError(BOOL inTest);
  BOOL ImageShiftIsOK(double newX, double newY, BOOL incremental);
  int SetAlignShifts(float inX, float inY, BOOL incremental, EMimageBuffer *imBuf,
    BOOL doImShift = true);
  void Initialize();
  ScaleMat AdjustedISmatrix(int iCamCal, int iMagCal, int iCamWant, int iMagWant);
  float GetCalibratedImageRotation(int inCamera, int inMagIndex);
  float GetPixelSize(int inCamera, int inMagIndex);
  ScaleMat SpecimenToCamera(int inCamera, int inMagInd);
  ScaleMat AveragedStageToCamera(int inCamera, int inMagInd);
  ScaleMat StageToCamera(int inCamera, int inMagInd);
  ScaleMat IStoCamera(int inMagInd);
  ScaleMat IStoSpecimen(int inMagInd, int toCam = -1);
  CShiftManager();
  virtual ~CShiftManager();
  GetSetMember(float, GlobalExtraRotation)
  GetSetMember(float, RoughISscale)
  GetSetMember(float, STEMRoughISscale)
  float GetLMRoughISscale() {return mLMRoughISscale ? mLMRoughISscale : mRoughISscale;};
  SetMember(float, LMRoughISscale)
  GetSetMember(int, NumRelRotations)
  GetMember(BOOL, AnyCalPixelSizes);
  GetSetMember(BOOL, LastTimeoutWasIS);
  ScaleMat MatMul(ScaleMat aa, ScaleMat bb);
  ScaleMat MatInv(ScaleMat aa);
  RelRotations *GetRelRotations() {return &mRelRotations[0];};
  GetSetMember(int, MinTiltDelay)
  GetSetMember(ScaleMat, StageStretchXform)
  GetSetMember(float, ISdelayScaleFactor)
  GetSetMember(BOOL, BacklashMouseAndISR)
  GetSetMember(int, DisableAutoTrim);
  GetSetMember(float, HitachiStageSpecAngle);
  GetMember(BOOL, MouseShifting);
  CArray<RotStretchXform, RotStretchXform> *GetRotXforms() {return &mRotXforms;};
  STEMinterSetShifts *GetSTEMinterSetShifts() {return &mInterSetShifts;};
  CArray<HighFocusMagCal, HighFocusMagCal> *GetFocusMagCals() {return &mFocusMagCals;};

private:
  CSerialEMApp * mWinApp;
  MagTable *mMagTab;
  ControlSet * mConSets;
  CString * mModeNames;
  EMimageBuffer *mImBufs;
  CArray<RotStretchXform, RotStretchXform> mRotXforms;
  CArray<HighFocusMagCal, HighFocusMagCal> mFocusMagCals;
  ScaleMat mStageStretchXform;
  STEMinterSetShifts mInterSetShifts;
  int *mActiveCameraList;
  float mGlobalExtraRotation;  // Image rotation to add to that reported by microscope
  float mRoughISscale;         // Starting scale for microns per unit of image shift
  float mLMRoughISscale;       // Starting scale for low mag
  float mSTEMRoughISscale;     // And for STEM
  int mMagIndex;
  CameraParameters *mCamParams;
  int mCurrentCamera;
  CEMscope *mScope;
  CCameraController *mCamera;
  EMbufferManager *mBufferManager;
  float mRegularShiftLimit;    // Limit of image shift on specimen in regular mags
  float mLowMagShiftLimit;     // Limit of image shift on specimen in low mag (LM)
  int mMaxNumPeaksToEval;      // Maximum number to evaluate
  float mPeakStrengthToEval;   // Minimum fraction of highest peak strength to evaluate
  float *mPeakHere;            // Temporary arrays for peak data
  float *mXpeaksHere;
  float *mYpeaksHere;
  BOOL mTrimDarkBorders;       // Flag to trim borders in Autoalign - default in low dose
  float mTrimFrac;             // Trimming fraction for correlation
  float mTaperFrac;            // Fraction of area to taper inside for correlation
  float mSigma1;               // Low frequency cutoff sigma
  float mRadius2;              // High frequency start of rolloff
  float mSigma2;               // High frequency rolloff sigma
  float *mArray, *mBrray, *mCrray;
  BOOL mDeleteA, mDeleteC;
  void *mDataA, *mDataC;
  KImage *mImA, *mImC;
  float mCTFa[8193];           // CTF for autoalign
  float *mTmplXpeaks;          // Variables used by autoalign for template correlation
  float *mTmplYpeaks;
  float *mTmplPeak;
  KImage *mTmplImage;
  int mTmplBinning;
  BOOL mMouseShifting;         // Flag that image is shifting with mouse, ImShift deferred
  float mMouseStartX;          // Starting shifts in the image when button down
  float mMouseStartY;
  BOOL mMouseEnding;           // Flag that mouse has been released
  BOOL mShiftPressed;          // Flag for shift key pressed at start of mouse move
  BOOL mMouseMoveStage;        // Flag to move stage for big mouse moves
  float mMouseStageThresh;     // Threshold fraction of camera field for stage move
  float mMouseStageAbsThresh;  // Absolute threshold in microns for stage move
  BOOL mShiftingDefineArea;    // Flag that Low dose define area is being shifted
  BOOL mAnyCalPixelSizes;      // Derivation level corresponding to full fallback
  int mLastISDelay;            // Recommended delay time for settling last image shift
  UINT mISTimeOut;             // Time for end of delay
  BOOL mLastTimeoutWasIS;      // Keep track if last timeout set came from an IS
  int mNumISdelays;            // Number of delay times in table
  float mISmoved[MAX_IS_DELAYS];  // Image shift movements in microns on specimen
  float mISdelayNeeded[MAX_IS_DELAYS];
  float mDelayPerMagDoubling;  // Extra delay per halving of Record pixel below 1 nm
  float mISdelayScaleFactor;   // Overall factor for scaling delay
  float mDefocusZFactor;       // Fudge factor to get from actual Z to defocus change
  BOOL mInvertStageXAxis;      // Flag that the X axis of stage coordinates is inverted
  int mStageInvertsZAxis;      // Flag that Z axis in inverted (Hitachi)
  int mNormalizationDelay;     // Delay after normalizing lenses, in msec
  int mLowMagNormDelay;        // Delay after normalizing in LM, or 0 to use same
  int mStageMovedDelay;        // Delay after moving stage, in msec
  int mStageDelayToUse;        // Value to use: Set AFTER the stage move
  StageMoveInfo mMoveInfo;     // coordinates for resetting shift
  BOOL mBacklashMouseAndISR;   // Flag to correct backlash in mouse move or reset IS
  BOOL mResettingIS;           // flag that reset is being done
  BOOL mStartedStageMove;      // Flag that stage movement was started from mouse shift
  float mTiltDelay;            // Tilt delay value
  double mResetStageMoveX;     // last stage displacement computes by ResetImageShift
  double mResetStageMoveY;
  double mResetStageAdjustX;   // Adjustment factor applied last time to stage moves
  double mResetStageAdjustY;
  ScaleMat mIStoBS[MAX_MAGS];  // Beam shift calibrations
  ShortVec mBeamCalMagInd;     // Mags that they were done at
  ShortVec mBeamCalAlpha;      // And alpha values
  ShortVec mBeamCalProbe;      // And probe modes
  int mNumBeamShiftCals;       // NUmber of calibrations stored
  int mNumRelRotations;        // Number of relative rotations
  RelRotations mRelRotations[MAX_REL_ROT];
  int mMinTiltDelay;           // Minimum tilt delay
  int mDisableAutoTrim;        // Flags for kinds of trimming in autoalign to disable
  float mHitachiStageSpecAngle;  // Angle to add to account for specimen X (tilt) onn stage Y
  int mLastAlignXTrimA, mLastAlignYTrimA;  // Trimming in last align with original binning
  int mLastAlignXTrimRef, mLastAlignYTrimRef;
  float mLastFocusForMagCal;
  int mLastApertureForMagCal;

public:
  void PropagateRotations(void);
  int NearestIScalMag(int inMag, int iCam, BOOL crossBorders);
  void PickMagForFallback(int & calMag, int & regCam);
  void PropagatePixelSizes(void);
  double TransferPixelSize(int fromMag, int fromCam, int toMag, int toCam);
  float NearbyFilmCameraRatio(int inMag, int inCam, int derived);
  double GoodAngle(double angle);
  void ShiftCoordinatesToOverlap(int widthA, int widthC, int extra, int & baseXshift, int & ix0A, int & ix1A, int & ix0C, int & ix1C);
  ScaleMat StretchCorrectedRotation(int camera, int magInd, float rotation);
  ScaleMat StretchCorrectedRotation(float rotation);
  ScaleMat UnderlyingStretch(ScaleMat raMat, float &smagMean, double & thetad, float & str, double & alpha);
  bool GetDefocusMagAndRot(int spot, int probeMode, double intensity, float defocus,
    float & scale, float & rotation, float &nearestFocus, double nearC2Dist[2], int nearC2ind[2], int &numNearC2);
  void AddHighFocusMagCal(int spot, int probeMode, double intensity, float defocus,
    float scale, float rotation);
  bool ImposeImageShiftOnScope(float delX, float delY, int magInd, int camera,
    BOOL incremental, BOOL mouseShifting);
  float GetPixelSize(EMimageBuffer * imBuf);
  int FindBoostedMagIndex(int magInd, int boostMag);
  bool ShiftAdjustmentForSet(int conSet, int magInd, float & shiftX, float & shiftY,
    int camera = -1, int recBin = -1);
  void MaintainOrImposeBacklash(StageMoveInfo *smi, double delX, double delY, BOOL doBacklash);
  void SetPeaksToEvaluate(int maxPeaks, float minStrength);
  UINT AddIntervalToTickTime(UINT ticks, int interval);
  void GetLastAlignTrims(int &xA, int &yA, int &xRef, int &yRef) {xA = mLastAlignXTrimA;
  yA = mLastAlignYTrimA; xRef = mLastAlignXTrimRef; yRef = mLastAlignYTrimRef;};
  ScaleMat FocusAdjustedStageToCamera(int inCamera, int inMagInd, int spot, int probe,
     double intensity, float defocus);
  ScaleMat FocusAdjustedStageToCamera(EMimageBuffer *imBuf);
  bool GetScaleAndRotationForFocus(EMimageBuffer * imBuf, float &scale, float &rotation);
  ScaleMat MatScaleRotate(ScaleMat aMat, float scale, float rotation);
  void AdjustStageToCameraForTilt(ScaleMat & aMat, float angle);
  void AdjustCameraToStageForTilt(ScaleMat & aMat, float angle);
  void AdjustStageMoveAndClearIS(int camera, int magInd, double &delStageX,
    double & delStageY, ScaleMat bInv);
  void ApplyScaleMatrix(ScaleMat & mat, float xFrom, float yFrom, float & xTo, float & yTo, bool incremental = false);
  void ListBeamShiftCals();
};

#endif // !defined(AFX_SHIFTMANAGER_H__FF4BB03C_5BFC_4142_BD0F_9A1A257FABE9__INCLUDED_)
