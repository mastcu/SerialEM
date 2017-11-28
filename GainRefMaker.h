#if !defined(AFX_GAINREFMAKER_H__AB5F3E8C_B27E_4D7C_913A_41BD379B7065__INCLUDED_)
#define AFX_GAINREFMAKER_H__AB5F3E8C_B27E_4D7C_913A_41BD379B7065__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GainRefMaker.h : header file
//


#define MAX_GAINREF_KVS 10
#define MAX_EXTRA_REFS 6
#define DMREF_ASK_IF_NEWER 0
#define DMREF_ASK_ALWAYS   1
#define DMREF_ASK_NEVER    2
#define MAX_DE_REF_TYPES  10

/////////////////////////////////////////////////////////////////////////////
// CGainRefMaker command target

class CGainRefMaker : public CCmdTarget
{
  // DECLARE_DYNCREATE(CGainRefMaker)
public:
  CGainRefMaker();
  ~CGainRefMaker();
// Attributes
public:

// Operations
public:
  int *GetRefKVList() {return &mRefKVList[0];};
  SetMember(int, NumRefKVs)
	void GetBinningOffsets(CameraParameters *param, int refBinning, int binning, 
  int &xOffset, int &yOffset);
	BOOL CheckDMReference(int binInd, BOOL optional);
  GetSetMember(CString, DMRefPath)
  GetSetMember(CString, RemoteRefPath)
  GetSetMember(CString, RefPath)
	void DMRefsNeedChecking();
  BOOL GetReference(int binning, void * &gainRef, int &byteSize, int &gainRefBits, 
    int &ownership, int xOffset, int yOffset);
  void StopAcquiringRef();
  void BeamTooWeak();
  CString ComposeRefName(int binning);
  void FindExistingReferences();
  void AcquiringRefNextTask(int param);
  void ErrorCleanup(int error);
  BOOL AcquiringGainRef() {return mFrameCount > 0;};
  void AcquireGainRef();
  GetMember(BOOL, TakingRefImages);
  GetMember(BOOL, PreparingGainRef);
  GetSetMember(BOOL, CalibrateDose)
  GetSetMember(BOOL, IgnoreHigherRefs)
  GetSetMember(BOOL, UseOlderBinned2)
  int *GetDEnumRepeats() {return &mDEnumRepeats[0];};
  float *GetDEexposureTimes() {return &mDEexposureTimes[0];};
  GetSetMember(int, DEuseHardwareBin);
  GetSetMember(int, DElastProcessType);
  GetMember(int, DEcurProcessType);
  GetSetMember(int, DElastReferenceType);
  GetSetMember(int, DMrefAskPolicy);
  int *GetLastDEdarkRefTime() {return &mLastDEdarkRefTime[0];};

  int GainRefBusy();
  void DeleteReferences(void);
  void InitializeRefArrays(void);
  void CheckChangedKV(void);
  double MemoryUsage(void);
  BOOL NeedsUnbinnedRef(int binning);


// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGainRefMaker)
  //}}AFX_VIRTUAL

// Implementation
protected:
  //virtual ~CGainRefMaker();

  // Generated message map functions
  //{{AFX_MSG(CGainRefMaker)
  //}}AFX_MSG

  DECLARE_MESSAGE_MAP()

private:
  CSerialEMApp * mWinApp;
  ControlSet * mConSet;
  EMimageBuffer *mImBufs;
  CCameraController *mCamera;
  CameraParameters *mParam;
  BOOL mRefExists[MAX_CAMERAS][MAX_EXTRA_REFS + 3];  // Properties of the gain ref files (third is DM ref)
  CTime mRefTime[MAX_CAMERAS][MAX_EXTRA_REFS + 3];
  int mByteSize[MAX_CAMERAS][MAX_EXTRA_REFS + 2];    // Properties of gain refs in memory
  void *mGainRef[MAX_CAMERAS][MAX_EXTRA_REFS + 3];
  int mGainRefBits[MAX_CAMERAS][MAX_EXTRA_REFS + 2];
  CString mRefPath;                 // Path to reference
  CString mDMRefPath;               // Path to DM references
  CString mRemoteRefPath;           // Path to Remote Socket references
  int mCurrentCamera;
  float *mArray;                    // array that ref is accumulated into
  int mFrameCount;                  // Counter for frames to go
  CString mFileName;                // Current file name for ref being made
  CString mBackupName;              // Name that it was copied to
  BOOL mBackedUp;                   // Flag that it was copied
  KStoreMRC *mStoreMRC;             // Storage class for current reference
  int mUseDMRef[MAX_CAMERAS][MAX_EXTRA_REFS + 3];    // Flag to use newer DM ref (+1) or check on it (-1)
  BOOL mDMRefChecked[MAX_CAMERAS];  // Flag that DM reference has been checked for time
  BOOL mWarnedNoDM[MAX_CAMERAS];    // Flag for initial warning of no DM reference
  int mNumBinnings[MAX_CAMERAS];    // Number of binnings to have references for
  int mDMind;                       // Index of DM reference for current camera
  BOOL mCalibrateDose;              // Flag to calibrate electron dose
  int mRefKVList[MAX_GAINREF_KVS];  // List of KVs at which to keep separate gain references
  int mNumRefKVs;
  int mHTValue;                     // KV value if at on of the separate KVs, otherwise 0
  int mDMrefAskPolicy;              // Whether and when to ask about using DM ref
  BOOL mIgnoreHigherRefs;           // Ignore refs above binning 2
  BOOL mUseOlderBinned2;            // Use a binned by 2 even if it is older
  int mRefBinInd;                   // Bin index when making a reference
  int mExpectedX, mExpectedY;       // Expected size of reference image
  int mModuloSaveX, mModuloSaveY;   // Saved values of camera modulo
  BOOL mSearchedRefs;               // Flag to prevent double search of references
  BOOL mTakingRefImages;            // Flag that image for reference is being taken
  BOOL mPreparingGainRef;           // Flag that any image for making gain reference is being taken
  int mDEnumRepeats[MAX_DE_REF_TYPES];  // Number of repeats for the various of DE refs
  float mDEexposureTimes[MAX_DE_REF_TYPES]; // Exposure times for the various DE refs
  int mDEuseHardwareBin;            // Use hardware binning by 2 for DE refs
  int mDElastProcessType;           // Last setting of processing type
  int mDElastReferenceType;         // Last setting of reference type
  int mStartingServerFrames;        // Starting number of frames
  int mLastDEdarkRefTime[2];        // Minute time stamps of DE server dark references
  int mDEcurProcessType;            // Processing type actually being done now
  int mDEcurReferenceType;          // Reference type being done

public:
  void UpdateDMReferenceTimes(void);
  bool IsDMReferenceNew(int camNum);
  void MakeRefInDEserver(void);
  void StartDEserverRef(int processType, int referenceType);
  int MakeDEdarkRefIfNeeded(int processType, float hoursSinceLast, CString &message);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GAINREFMAKER_H__AB5F3E8C_B27E_4D7C_913A_41BD379B7065__INCLUDED_)
