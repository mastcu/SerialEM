#if !defined(AFX_FILTERTASKS_H__FC3E29BD_B47E_4C4B_B726_4DFFDA744F02__INCLUDED_)
#define AFX_FILTERTASKS_H__FC3E29BD_B47E_4C4B_B726_4DFFDA744F02__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FilterTasks.h : header file
//



/////////////////////////////////////////////////////////////////////////////
// CFilterTasks command target

class CFilterTasks : public CCmdTarget
{
  //DECLARE_DYNCREATE(CFilterTasks)
 public:
  CFilterTasks();           // protected constructor used by dynamic creation
  ~CFilterTasks();

  // Attributes
 public:

  // Operations
 public:
	 int TestRefineZLPStart(float eStart, CString scanType);
	 int TestCalMagStart(float start);
   void SetShiftCalMinField(float inVal) {mCMSMinField = inVal;};
   float GetShiftCalMinField() {return mCMSMinField;};
   GetSetMember(float, RZlpStepSize)
   GetSetMember(float, RZlpSlitWidth)
   GetSetMember(float, RZlpMinExposure)
  GetSetMember(float, RZlpMeanCrit)
  GetSetMember(BOOL, RZlpRedoInLowDose)
  GetSetMember(BOOL, RZlpLeaveCDSmode);
  SetMember(bool, NextRZlpRedoInLD);
  SetMember(bool, AllowNextRZlpFailure);
  GetMember(bool, LastRZlpFailed);
  BOOL AllocateArrays(int size);
  BOOL RefiningZLP() {return mRZlpIndex >= 0;};
  void CleanupArrays();
  void RefineZLPCleanup(int error);
  void StopRefineZLP();
  void RefineZLPNextTask();
  BOOL RefineZLP(bool interactive = false, int useTrial = 0);
  int InitializeRegularScan();
  void StartAcquisition(DWORD delay, int conSetNum, int taskNum);
  void InitializeScan(float startEnergy);
  BOOL CalibratingMagShift() {return mCurMag > 0;};
  void StopCalMagShift();
  void CalMagShiftCleanup(int error);
  void CalMagShiftNextTask();
  void CalibrateMagShift();
  void MagShiftCalNewMag(int inMag);
  void Initialize();

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFilterTasks)
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(CFilterTasks)
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()

 private:
  CSerialEMApp * mWinApp;
  ControlSet * mConSets;
  EMimageBuffer *mImBufs;
  CEMscope *mScope;
  CCameraController *mCamera;
  CShiftManager *mShiftManager;
  FilterParams *mFiltParams;
  MagTable *mMagTab;
  int mSavedMagInd;              // Mag at which procedure started
  double mSavedIntensity;        // Intensity at start if changed by beam calibrations
  BOOL mUsersIntensityZoom;      // Whether user has intensity zoom on
  BOOL mIntensityZoom;           // Whether intensity zoom is being used for changes
  float mFullExposure;           // Unattenuated exposure time
  FilterParams mSavedParams;     // Pre-existing parameters
  float mCMSSlitWidth;           // Slit with for mag-shift cal
  float mCMSEnergyRange;         // Energy range to scan
  float mCMSStepSize;            // Energy step size
  int mNumMags;                  // Number of mags
  int mLowestMagInd;             // Lowest mag to do
  float mCMSMinField;            // Minimum field of view required (used to set lowest mag)
  int mDirInd;                   // Direction index
  int mMagCount;                 // Loop counter
  int mCurMag;                   // Current mag
  float mEnergyShifts[2][MAX_MAGS];
  float *mEnergies;
  float *mMeanCounts;
  int mEnergyCount;              // Loop counter for scan
  int mNumEnergies;              // Maximum Total number to do
  int mNumScan;                  // Number to scan if no peak found
  float mCurEnergy;              // Current energy
  float mOverallPeak;            // highest counts encountered in procedure
  float mScanPeak;               // highest counts in this scan
  int mPeakIndex;                // index of peak
  float mPeakThreshold;          // Fraction below peak that is considered below concern
  float mMinPeakRatio;           // Fraction of overall peak that is recognized as a scan peak
  float mMinPeakSize;            // Minimum height to consider a peak
  float mGoodPeakSize;           // A height that qualifies absolutely as a peak
  float mBasicIntegrationWidth;  // Width to integrate, minus slit width
  float mFullIntegrationWidth;   // Actual width to integrate
  BOOL mRedoing;                 // Flag that we are redoing a scan
  DWORD mBigEnergyShiftDelay;
  DWORD mLittleEnergyShiftDelay;
  float mRZlpSlitWidth;          // Slit width for refine ZLP
  float mRZlpStepSize;           // Step size
  float mRZlpMinExposure;        // Minimum exposure time, seconds
  int mRZlpConSetNum;            // Control set being used
  ControlSet mRZlpConSetSave;    // Saved control set
  LowDoseParams mLDPsave;        // Saved low dose params
  LowDoseParams *mLDParam;       // Active parameter for doing refine
  LowDoseParams *mLDPcancel;     // Parameter set from which loss cancel is done
  BOOL mLowDoseMode;             // Flag that we are in low dose
  int mRZlpIndex;                // Index for refine ZLP
  float *mDeltaCounts;           // Array for delta counts (last mean - this mean)
  int mRZlpTrial;                // Counter for trials
  float mPeakDelta;              // Peak delta value
  float mPeakMean;               // Peak mean value
  float mB2WpeakDelta;           // Negative peak mean value for black to white transition
  int mB2WpeakIndex;             // Index of negative peak
  float mRZlpDeltaCrit;          // Criterion for final delta as fraction of peak
  float mRZlpMeanCrit;           // Criterion for final mean as fraction of peak
  float mRZlpUserLoss;           // Starting user loss to cancel
  float mRZlpUserCancelFrac;     // Fraction of slit width to cancel automatically
  int mRZlpCancelLDArea;         // Area loss originated from
  BOOL mRZlpRedoInLowDose;       // Flag to iterate upon failure in low dose
  bool mNextRZlpRedoInLD;        // Flag to redo next time
  bool mAllowNextRZlpFailure;    // Flag to not thrpw an error if next one fails
  bool mLastRZlpFailed;          // Flag set on failure
  bool mRZlpRestoreCDSmode;      // Flag to restore CDS mode for a K3 camera
  BOOL mRZlpLeaveCDSmode;        // Flag to leave CDS mode if it is on
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILTERTASKS_H__FC3E29BD_B47E_4C4B_B726_4DFFDA744F02__INCLUDED_)
