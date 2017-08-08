// CalibCameraTiming.h: interface for the CCalibCameraTiming class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CALIBCAMERATIMING_H__57771EDA_E438_4D06_B975_31716490CE47__INCLUDED_)
#define AFX_CALIBCAMERATIMING_H__57771EDA_E438_4D06_B975_31716490CE47__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_RANGE 50
#define MAX_DEAD_TIMES  7
enum {FLYBACK_ERROR = 0, FLYBACK_NONE, FLYBACK_EXACT, FLYBACK_INTERP, FLYBACK_EXTRAP, 
FLYBACK_SINGLE};

// Flyback time table
struct FlybackTime {
  char dateTime[DATETIME_SIZE];       // Time stamp for when it was acquired
  int binning;          // Binning of image
  int xSize;            // X size (sufficient for FEI subareas)
  int magIndex;         // The mag index
  float exposure;       // Exposure time
  float flybackTime;    // measured microseconds of flyback
  float startupDelay;   // Startup delay from fit also
};

class CCalibCameraTiming  
{
public:
	void StopCalDead();
	void CalDeadCleanup(int param);
	void CalDeadNextTask();
	void CalibrateDeadTime();
  BOOL DoingDeadTime() {return mDeadIndex >= 0;};
  void SetExposure(float inVal) {mExposure = inVal;};
  void SetStartupDelay(float inVal) {mStartupDelay = inVal;};
  BOOL Calibrating() {return mCalIndex >= 0;};
	void StopCalTiming();
	void CalTimeCleanup(int param);
	void CalTimeNextTask();
	void CalibrateTiming(int setNum, float exposure, bool confirmOK);
  CArray<FlybackTime, FlybackTime> *GetFlybackArray() {return &mFlybackArray;};
  GetSetMember(BOOL, NeedToSaveFlybacks);
	CCalibCameraTiming();
	virtual ~CCalibCameraTiming();
  SetMember(float, K2InitialStartup);

private:

  CCameraController *mCamera;
  ControlSet *mConSet;
  EMimageBuffer *mImBufs;
  CSerialEMApp * mWinApp;
  CameraParameters *mCamParam;
  float mExtraTimeSave;       // Save real values of timing parameters
  float mStartupSave;
  float mMinDriftSave;
  float mMinBlankSave;
  float mBuiltInSave;
  int mNoShutterSave;
  float mExtraBeamTime;       // Parameters to use during tests
  float mStartupDelay;
  float mMinimumDrift;
  float mK2InitialStartup;    // Initial delay to use for K2
  float mBuiltInDecrement;    // Amount to reduce built-in-settling
  float mBlankCycleTime;      // Time it took to blank and unblank
  float mLowerRefRatio;       // Reference ratio from first point below 1/4 way down
  float mExposure;            // Exposure time for tests
  float mDefRegExposure;      // Default or last exposures for regular or STEM
  float mDefSTEMexposure;
  BOOL mQuickFlyback;         // Quick flyback mode
  int mNumQuickDelays;        // Number of delays to test
  int mLowQuickDelays;        // Number of delays to test for low exposures
  int mLowQuickShots;         // Number of shots at each for low exposures
  int mHighQuickDelays;       // Number of delays to test for High exposures
  int mHighQuickShots;        // Number of shots at each for High exposures
  float mLowHighThreshold;    // Exposure time for switching between low and high 
  float mFirstQuickStartup;   // First startup
  bool mConfirmOK;            // Flag for whether to confirm saving 
  int mCalIndex;              // Index for repetitive shots
  float *mMeanValues;         // Array to save values in
  int mNumRefShots;           // Number of reference shots
  int mNumTestShots;          // Number of test shots
  int mDefRegTestShots;       // Defaults for regular and STEM
  int mDefSTEMTestShots;
  BOOL mDoingRefs;            // Flag for doing reference shots
  CString mLineString;        // String for line of output
  float mMeanReference;       // Mean reference value
  int mNumRangeShots;         // Number of shots per startup delay in finding delay range
  BOOL mDoingRange;           // Flag that range is being found
  float mInitialStartup;      // First startup delay when finding range
  float mStartupIncrement;    // Increment between delays in range-finding
  int mRangeIndex;            // Index while finding range
  float mRangeMeans[MAX_RANGE];   // Means not at extremes while finding range
  float mRangeStartups[MAX_RANGE];  // startup values
  int mDeadIndex;             // Index for dead time calibration
  float mDeadExposure[MAX_DEAD_TIMES];
  float mDeadCounts[MAX_DEAD_TIMES];
  float mDeadStartExp;        // Starting exposure time
  float mDeadExpInc;          // Increment exposure
  int mNumDeadExp;            // Number of exposures
  BOOL mStartupOnly;          // Flag that we are measuring a startup with initial blank
  int mSTEMcamera;            // 0 or 1 for STEM camera
  float mBlankMean;           // Value of blank image for STEM
  float mFlyback;             // Computed flyback time for STEM
  CArray<FlybackTime, FlybackTime> mFlybackArray;
  BOOL mNeedToSaveFlybacks;          // Flag that flybacks need saving
  int mMatchedFlybackInd;     // Index of one that was matched in last lookup
public:
  int FlybackTimeFromTable(int binning, int xSize, int magIndex, float exposure, 
    float & flyback, float & startup, CString *message = NULL);
};

#endif // !defined(AFX_CALIBCAMERATIMING_H__57771EDA_E438_4D06_B975_31716490CE47__INCLUDED_)
