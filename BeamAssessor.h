// BeamAssessor.h: interface for the CBeamAssessor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEAMASSESSOR_H__67812E27_EF7F_4AFB_A706_766E3FEA19AA__INCLUDED_)
#define AFX_BEAMASSESSOR_H__67812E27_EF7F_4AFB_A706_766E3FEA19AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "EMscope.h"

#define TARGET_INTENSITY_TABLE_SIZE 30
#define MAX_INTENSITY_TABLES 150
#define MAX_INTENSITY_ARRAYS (MAX_INTENSITY_TABLES * TARGET_INTENSITY_TABLE_SIZE)
#define MAX_INTENSITIES  (MAX_INTENSITY_ARRAYS / 11)

#define BEAM_STRENGTH_NOT_CAL      1
#define BEAM_STRENGTH_WRONG_SPOT   2
#define BEAM_STRENGTH_SCOPE_ERROR  3
#define BEAM_STARTING_OUT_OF_RANGE 4
#define BEAM_ENDING_OUT_OF_RANGE   5
#define BEAM_DONT_EXTRAP_HIGH      1
#define BEAM_DONT_EXTRAP_LOW       2

struct BeamTable {
  int numIntensities;
  int spotSize;
  int magIndex;
  int dontExtrapFlags;
  float minIntensity;
  float maxIntensity;
  double *intensities;
  float *currents;
  float *logCurrents;
  int aboveCross;
  double crossover;   // Crossover calibration when table acquired
  int measuredAperture;  // Aperture at which table was measured
  int probeMode;
};

struct DoseTable {
  int timeStamp;
  double intensity;
  double dose;
  int currentAperture;
};

struct SpotTable {
  double intensity[MAX_SPOT_SIZE + 2];
  float ratio[MAX_SPOT_SIZE + 2];
  double crossover[MAX_SPOT_SIZE + 2];
  int probeMode;
};

class CBeamAssessor
{
 public:
	 int FindSpotTableIndex(int aboveCross, int probe);
	 double FindCurrentRatio(double startIntensity, double inIntensity, int spotSize,
     int aboveCross, int probe);
   BOOL CalibratingSpotIntensity() {return mSpotCalIndex > 0;};
	 void StopSpotCalibration();
	 void SpotCalCleanup(int error);
	 void SpotCalImage(int param);
	 void CalibrateSpotIntensity();
	 void CalSetupNextMag(float extraDelta);
   GetSetMember(double, InitialIncrement)
   GetSetMember(double, SpacingFactor)
   GetSetMember(float, MinExposure)
   GetSetMember(float, MaxExposure)
   GetSetMember(int, PostSettingDelay);
   GetSetMember(int, MinCounts)
   GetSetMember(int, MaxCounts)
	 BOOL SetAndTestIntensity();
	 BOOL CanDoubleBinning();
	 void MakeControlSet(int fracField, bool useCounting);
	 void CalIntensityCCDCleanup(int error);
	 void CalIntensityImage(int param);
	 void CalIntensityCCD();
   void ListIntensityCalibrations();
   void ListSpotCalibrations();
  int GetFreeIndex() {return mFreeIndex;};
  void SetFreeIndex(int inVal) {mFreeIndex = inVal;};
  int GetAboveCrossover(int spotSize, double intensity, int probe = -1);
  void AssignCrossovers();
  DoseTable *GetDoseTables() {return &mDoseTables[0][0][0];};
  SpotTable *GetSpotTables() {return &mSpotTables[0];};
  void CalibrateElectronDose(BOOL interactive);
  double GetCurrentElectronDose(int conSetNum);
  BOOL LookupCurrent(int numIntensities, double inIntensity, double &newLog);
  double GetElectronDose(int spotSize, double inIntensity, float exposure, int probe = -1);
  BOOL CalibratingBeamShift() {return mShiftCalIndex >= 0;};
  void ShiftCalImage();
  int FindCenterForShiftCal(int edgeDivisor, int &size);
  void StoreBeamShiftCal(int magInd, int retain);
  void StopShiftCalibration();
  void ShiftCalCleanup(int error);
  void RefineBeamShiftCal();
  void RefineShiftCalImage();
  void CalibrateBeamShift();
  GetSetMember(float, CalMinField)
  GetSetMember(float, ExtraRangeAtMinMag)
  void Initialize();
  void SortAndTakeLogs(BeamTable *inTable, BOOL printLog);
  int GetNumTables() {return mNumTables;};
  void SetNumSpotTables(int inVal) {mNumSpotTables = inVal;};
  int GetNumSpotTables() {return mNumSpotTables;};
  void SetNumTables(int inVal) {mNumTables = inVal;};
  int AssessBeamChange(double startIntensity, int spotSize, int probe, double inFactor,
                                    double &newIntensity, double &outFactor);
  int AssessBeamChange(double inFactor, double &outIntensity, double &outFactor,
             int lowDoseArea);
  BOOL IntensityCalibrated();
  void FitIntensityCurve(int indStart, int nFit, float &a, float &b, float &con);
  void FitCurrentCurve(int indStart, int nFit, float &a, float &b, float &con);
  int ChangeBeamStrength(double inFactor, int lowDoseArea);
  void StopCalibratingIntensity() {mStoppingCal = true;};
  BOOL CalibratingIntensity() {return mCalibratingIntensity;};
  BeamTable *GetBeamTables() {return mBeamTables;};
  GetMember(int, CurrentAperture);
  GetSetMember(int, NumC2Apertures);
  GetSetMember(BOOL, UseTrialSettling);
  int *GetCrossCalAperture(void) {return &mCrossCalAperture[0];};
  int *GetSpotCalAperture(void) {return &mSpotCalAperture[0];};
  int *GetC2Apertures(void) {return &mC2Apertures[0];};
  GetSetMember(BOOL, FavorMagChanges);
  float *GetBSCalAlphaFactors() { return &mBSCalAlphaFactors[0];};
  CBeamAssessor();
  virtual ~CBeamAssessor();

 private:
  CSerialEMApp *mWinApp;
  CEMscope *mScope;
  CCameraController *mCamera;
  CameraParameters *mCamParam;
  CShiftManager *mShiftManager;
  int mNumTables;         // Number of tables filled
  double *mIntensities;    // Pointers for current table
  float *mCurrents;
  float *mLogCurrents;
  int mTableInd;
  BeamTable *mCalTable;
  BeamTable mBeamTables[MAX_INTENSITY_TABLES];
  double mAllIntensities[MAX_INTENSITY_ARRAYS];
  float mAllCurrents[MAX_INTENSITY_ARRAYS];
  float mAllLogs[MAX_INTENSITY_ARRAYS];
  int mTrials[MAX_INTENSITIES];
  int mFreeIndex;         // Index to next free sopt in "All" arrays
  int mCalSpotSize;       // Spot size of current calibration
  int mCalMagInd;         // Starting mag index for calibrating
  float mCalMinField;     // Minimum field of view calibration is required for
  float mExtraRangeAtMinMag;      // Amount to extend curve at minimum mag
  BOOL mUsersIntensityZoom;       // Whether user has intensity zoom on
  double mStartIntensity; // Global starting intensity
  double mStartCurrent;   // Starting current for this mag series
  double mChangeNeeded;   // Change still needing to be covered
  double mSpacingFactor;  // Factor for spacing between currents
  double mTestIncrement;  // Increment for current test shot from last intensity
  double mInitialIncrement;  // Initial increment
  BOOL mUseTrialSettling; // Use Trial settling and shutter for the intensity cal
  double mTestIntensity;  // Intensity for current test shot
  int mNumIntensities;    // Number of intensities so far
  int mMinCounts;         // Minimum counts allowed in image
  int mMaxCounts;         // Maximum/target counts allowed in image
  int mCntK2MinCounts;    // Values for K2
  int mCntK2MaxCounts;
  int mLinK2MinCounts;    // Linear mode value
  int mUseMaxCounts;      // Values to use when running
  int mUseMinCounts;
  float mExposure;        // Exposure time
  int mBinning;           // Binning
  float mMinExposure;     // Minimum exposure time
  float mMaxExposure;     // Minimum exposure time
  float mCntK2MinExposure;  // Values for K2
  float mCntK2MaxExposure;
  float mUseMaxExposure;  // Value to use when running
  BOOL mFavorMagChanges;  // Flag to change mag frequently like with a K2
  double mIntervalTolerance;  // Variation in interval allowed
  int mNumFail;           // Number of failures to get desired interval (trials > 10)
  int mNumTry;            // Number of trials to get desired interval
  double mMatchSum;       // Sum for taking mean before and after changing conditions
  int mNumMatchBefore;    // Number of images to take for match before change
  int mNumMatchAfter;     // Number of images to take for match after
  int mMatchInd;          // Number taken so far
  double mMatchFactor;    // Factor for unbinned counts/sec, adjusted upon changes
  int mPostSettingDelay;  // Delay after setting intensity
  int mNextMagInd;        // Mag index for next mag down
  double mMagChgMaxCurrent;  // Maximum current for doing mag change
  double mMagChgMaxCounts;   // Maximum counts for doing mag change
  double mNextMagMaxCurrent;  // Maximum current for switch to next mag if dose rate low
  int mCalChangeType;     // Flag for type of change being done
  float mMagMaxDelCurrent;   // Maximum change in current allowed over a mag change
  float mMagExtraDelta;   // Extra change in current from first mag (for spreading beam)
  bool mDropBinningOnMagChg;  // Flag to drop binning on mag change for K2
  double mLastIncrement;   // For keeping track of whether to work out hysteresis
  bool mCountingK2;        //Flag for counting K2/K3 camera
  float mMinDoseRate;      // Values for min and max dose rate to stay within, different
  float mMaxDoseRate;      // for K2 and K3
  int mLastNumIntensities;
  CWinThread *mIntensityThread;
  BOOL mCalibratingIntensity;
  BOOL mStoppingCal;      // Flag to stop calibration via CCD
  LowDoseParams *mLDParam;
  BOOL mShiftCalIndex;    // Index for beam shift calibration
  bool mRefiningShiftCal; // Flag that beam shift cal is being refined
  bool mUseEdgeForRefine; // Get beam center from edges for refining cal
  double mBaseShiftX;     // Base beam shift during calibration
  double mBaseShiftY;
  double mBaseISX;        // Base image shift when refining
  double mBaseISY;
  double mBorderMean;     // Mean of border in image of centered spot
  float mBSCcenX[13];      // Centroid coordinates of spot or center from edge analysis
  float mBSCcenY[13];
  float mBSCshiftX[12];    // NEXT beam shift value; or recorded value when refining
  float mBSCshiftY[12];
  float mRBSCshiftISX[12]; // NEXT image shift value, and recorded value when refining
  float mRBSCshiftISY[12];
  ScaleMat mIStoBScal;
  float mRBSCmaxShift;     // Maximum distance to shift in microns
  float mBSCalAlphaFactors[MAX_ALPHAS];
  DoseTable mDoseTables[MAX_SPOT_SIZE + 2][2][2];
  int mNumDoseTables;
  int mNumSpotTables;
  SpotTable mSpotTables[4];
  int mSpotCalIndex;       // Spot size for current image in spot intensity calibration
  int mNumSpotsToCal;      // Number of spots to calibrate
  int mFirstSpotToCal;       // starting index for spot cal
  double mTempIntensities[MAX_SPOT_SIZE + 2];
  float mTempCounts[MAX_SPOT_SIZE + 2];
  int mSpotCalStartSpot;  // Starting spot size
  int mCurrentAperture;   // Keep track of aperture number for Titan
  int mC2Apertures[MAX_SPOT_SIZE + 2];
  int mNumC2Apertures;    // List of available aperture sizes
  int mCrossCalAperture[2];  // Apertures at which crossover and spot size calibrated
  int mSpotCalAperture[4];
  int mNumExpectedCals;   // Number of non-empty calibrations expected to exist
  BOOL mSaveUseK3CDS;     // Saved value to avoid CDS mode for calibrating

public:
  void CalibrateCrossover(void);
  double GetCurrentElectronDose(int camera, int setNum, int & spotSize,
    double & intensity);
  double GetCurrentElectronDose(int camera, int setNum, float csExposure, float csDrift,
    int & spotSize, double & intensity);
  int SetTableAccessAndLimits(int bestTable, double &leftExtrapLimit,
                              double &rightExtrapLimit, double &diffSign);
  int FindBestTable(int spotSize, double startIntensity, int probe);
  int OutOfCalibratedRange(double startIntensity, int spotSize, int probe, int & polarity,
    bool warnIfExtrap = false);
  void ScaleTablesForAperture(int currentAp, bool fromCurrent);
  int RequestApertureSize(void);
  void InitialSetupForAperture(void);
  void CalibrateAlphaBeamShifts(void);
  void CalibrateSpotBeamShifts(void);
  int SetAndCheckSpotSize(int newSize, BOOL normalize = FALSE);
  int CountNonEmptyBeamCals(void);
  int CheckCalForZeroIntensities(BeamTable &table, const char *message, int postMessType);
};

#endif // !defined(AFX_BEAMASSESSOR_H__67812E27_EF7F_4AFB_A706_766E3FEA19AA__INCLUDED_)
