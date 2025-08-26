// SerialEMDoc.h : interface of the CSerialEMDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALEMDOC_H__A0C8625F_02C5_4A68_9EAF_11F5AC5BA286__INCLUDED_)
#define AFX_SERIALEMDOC_H__A0C8625F_02C5_4A68_9EAF_11F5AC5BA286__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "FileOptions.h"

class CReadFileDlg;

enum {MRC_OPEN_NOERR = 0, MRC_OPEN_CANCEL, MRC_OPEN_ALREADY_OPEN,
MRC_OPEN_ERROR, MRC_OPEN_NOTMRC, MRC_OPEN_ADOC, MRC_OPEN_HDF};
#define USE_ACTIVE_BUF -999

DLL_IM_EX const char *SEMDefaultSysPath(void);

class DLL_IM_EX CSerialEMDoc : public CDocument
{
protected: // create from serialization only
  CSerialEMDoc();
  DECLARE_DYNCREATE(CSerialEMDoc)

    // Attributes
    public:

  // Operations
public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSerialEMDoc)
public:
  virtual BOOL OnNewDocument();
  virtual void Serialize(CArchive& ar);
  //}}AFX_VIRTUAL

  // Implementation
public:
  MontParam * GetStoreMontParam(int which);
	BOOL StoreIsMontage(int which);
	KImageStore * GetStoreMRC(int which);
	int LookupProtectedStore(int which);
	int SaveBufferToFile(int bufNum, int fileNum, int inSect = -1);
	int StoreIndexFromName(CString name);
	void RestoreCurrentFile();
  void NewPointerForCurrentStore(KImageStore *inStore);
	bool FileAlreadyOpen(CString filename, CString message);
	void CloseAllStores();
  GetMember(int, NumStores)
  GetMember(int, CurrentStore)
  int GetSaveOnNewMap() {return mCurrentStore < 0 ? 0 : mStoreList[mCurrentStore].saveOnNewMap;};
  void SetSaveOnNewMap(int inVal);
	int SetToProtectedStore(int which);
	void EndStoreProtection(int which = -1);
	void ProtectStore(int which);
	void SetCurrentStore(int which);
	void AddCurrentStore();
	void SwitchToFile(int which);
	void LeaveCurrentFile();
	void ReadNewSettingsFile(CString newSettings);
	void PostSettingsRead();
  void ManageReadInCurrentDir();
  CString GetCurrentSettingsDir();
  void DirFromCurrentOrSettingsFile(const CString &curFile, CString &direc);
	void PreSettingsRead();
  int CheckBgkdScript();
  GetMember(CTime, StartupTime)
  GetMember(CString, OriginalCwd)
  void SetShortTermNotSaved() {mShortTermNotSaved = true;};
  void SaveCalibrations();
	void SaveShortTermCal();
	void AppendToProgramLog(BOOL starting);
  //GetSetMember(BOOL, AutoSaveNav);
  GetMember(BOOL, AutoSaveNav);
  void SetAutoSaveNav(BOOL inVal);
  GetSetMember(BOOL, AutoSaveSettings);
  GetSetMember(BOOL, IgnoreShortTerm);
  SetMember(int, DefaultMontXpieces);
  SetMember(int, DefaultMontYpieces);
  GetSetMember(double, OverlapFraction);
  GetMember(int, MaxMontagePieces);
  GetMember(int, MinMontageOverlap);
  GetMember(float, MaxOverlapFraction);
  void SetMaxOverlapFraction(float inVal) { B3DCLAMP(inVal, 0.25f, 0.75f); mMaxOverlapFraction = inVal; };
  SetMember(int, STEMfileMode);
  GetSetMember(int, STEMunsignOpt);
  GetSetMember(BOOL, SkipFileDlg);
  SetMember(bool, DeferWritingFrameMdoc);
  GetSetMember(bool, ReadScriptPack);

	void AutoSaveFiles();
	int AppendToLogBook(CString inString, CString title);
  GetSetMember(CString, Title);
  GetSetMember(CString, FrameTitle);
  int DoOpenNewFile(CString filename = "");
  void DoCloseFile();
  void ManageExposure(bool noMessage);
  int GetMontageParamsAndFile(int frameSet, int xNframes = -1, int yNframes = -1,
                              CString filename = "");
  void InitMontParamsForDialog(MontParam *param, int frameSet, int xNframes = -1,
    int yNframes = -1, CString filename = "");
  int OpenMontageDialog(BOOL locked);
  FileOptions *GetDefFileOpt() {return &mDefFileOpt;};
  FileOptions *GetFileOpt() {return &mFileOpt;};
  FileOptions *GetOtherFileOpt() {return &mOtherFileOpt;};
  int SaveSettingsOnExit();
  void ManageBackupFile(CString strFile, BOOL &bBackedUp);
  void ManageScriptPackBackup();
  int OfferToSaveSettings(CString strWhy);
  SetMember(CString, LogBook);
  GetMember(CString, SystemPath);
  GetMember(CString, FullSystemDir);
  void SetSystemPath(CString sysPath);
  GetMember(CString, SysPathForSettings);
  GetMember(CString, CurrentSettingsPath);
  GetMember(CString, OriginalSettingsPath);
  GetMember(BOOL, SettingsOpen);
  SetMember(CString, CurrentDirReadIn);
  GetSetMember(CString, PluginPath)
  GetSetMember(CString, PluginPath2)
  GetMember(CString, FlybackName);
  GetMember(int, DfltUseMdoc);
  GetMember(int, FrameAdocIndex);
  GetMember(bool, HDFsupported);
  GetSetMember(CString, CurScriptPackPath);
  GetSetMember(BOOL, ScriptPackBackedUp);
  SetMember(CString, SettingsName);
  GetSetMember(CString, BasicModeFile);
  GetSetMember(BOOL, AbandonSettings);
  CArray<CString, CString> *GetGlobalAdocKeys() { return &mGlobalAdocKeys; };
  CArray<CString, CString> *GetGlobalAdocValues() { return &mGlobalAdocValues; };
  void SetDfltUseMdoc(int inval);
  void ReadSetPropCalFiles();
  void FixSettingsForIALimitCal();
  void SaveActiveBuffer();
  void SaveRegularBuffer();
  int SettingsSaveAs();
  int ExtSaveSettings();
  void SetPointers(EMbufferManager *inManager, CEMbufferWindow *inWindow);
  virtual ~CSerialEMDoc();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif
  KImageStore * OpenSaveFile(FileOptions *fileOptp);
  WINDOWPLACEMENT * GetReadDlgPlacement(void);
  FileOptions *GetStoreFileOptions(int which);

  CReadFileDlg *mReadFileDlg;

protected:

  // Generated message map functions
protected:
  //{{AFX_MSG(CSerialEMDoc)
  afx_msg void OnFileOpennew();
  afx_msg void OnUpdateFileOpennew(CCmdUI* pCmdUI);
  afx_msg void OnFileOpenold();
  afx_msg void OnUpdateFileOpenold(CCmdUI* pCmdUI);
  afx_msg void OnFileMontagesetup();
  afx_msg void OnUpdateFileMontagesetup(CCmdUI* pCmdUI);
  afx_msg void OnFileClose();
  afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);
  afx_msg void OnFileSave();
  afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
  afx_msg void OnFileSaveactive();
  afx_msg void OnUpdateFileSaveactive(CCmdUI* pCmdUI);
  afx_msg void OnFileSaveother();
  afx_msg void OnUpdateFileSaveother(CCmdUI* pCmdUI);
  afx_msg void OnFileTruncation();
  afx_msg void OnUpdateFileTruncation(CCmdUI* pCmdUI);
  afx_msg void OnFileRead();
  afx_msg void OnFileReadother();
  afx_msg void OnUpdateFileReadother(CCmdUI* pCmdUI);
  afx_msg void OnUpdateWindowNew(CCmdUI* pCmdUI);
  afx_msg void OnUpdateFileRead(CCmdUI* pCmdUI);
  afx_msg void OnSettingsOpen();
  afx_msg void OnSettingsReadagain();
  afx_msg void OnUpdateSettingsReadagain(CCmdUI* pCmdUI);
  afx_msg void OnSettingsSave();
  afx_msg void OnSettingsSaveas();
  afx_msg void OnSettingsClose();
  afx_msg void OnUpdateSettingsClose(CCmdUI* pCmdUI);
  afx_msg void OnSettingsReaddefaults();
  afx_msg void OnUpdateSettingsReaddefaults(CCmdUI* pCmdUI);
  afx_msg void OnSettingsSavecalibrations();
  afx_msg void OnFileReadpiece();
  afx_msg void OnUpdateFileReadpiece(CCmdUI* pCmdUI);
  afx_msg void OnFileOverwrite();
  afx_msg void OnUpdateFileOverwrite(CCmdUI* pCmdUI);
  afx_msg void OnUpdateSettingsSavecalibrations(CCmdUI* pCmdUI);
  afx_msg void OnUpdateSettingsOpen(CCmdUI* pCmdUI);
	afx_msg void OnFileSet16bitpolicy();
	afx_msg void OnUpdateFileSet16bitpolicy(CCmdUI* pCmdUI);
	afx_msg void OnSettingsAutosave();
	afx_msg void OnUpdateSettingsAutosave(CCmdUI* pCmdUI);
	afx_msg void OnFileSetsignedpolicy();
	afx_msg void OnUpdateFileSetsignedpolicy(CCmdUI* pCmdUI);
	afx_msg void OnFileNewmontage();
	afx_msg void OnUpdateFileNewmontage(CCmdUI* pCmdUI);
	//}}AFX_MSG
  afx_msg void OnSettingsRecent(UINT nID);
  afx_msg void OnUpdateSettingsRecent(CCmdUI* pCmdUI);
  DECLARE_MESSAGE_MAP()

private:
  CEMbufferWindow * mBufferWindow;
  EMbufferManager * mBufferManager;
  CSerialEMApp * mWinApp;
  CRecentFileList *mRecentSettings;
  CArray<StoreData, StoreData> mStoreList;  // List of open stores (image files)
  int mNumStores;                // Number of open stores
  int mCurrentStore;             // Current one

  FileOptions  mFileOpt;
  FileOptions  mDefFileOpt;
  FileOptions  mOtherFileOpt;    // File options for other file
  float   mMaxTrunc;

  CParameterIO *mParamIO;
  CArray<CString, CString> mGlobalAdocKeys;     // Key-value pairs to add to global 
  CArray<CString, CString> mGlobalAdocValues;   // section of autodoc
  CString mSystemPath;           // Path for system settings & properties
  CString mSysPathForSettings;   // String to rewrite into settnigs file
  CString mFullSystemDir;        // Full system directory path
  CString mSettingsName;         // Root name of default settings files
  CString mSystemSettingsName;   // Root name of system settings file
  CString mCurrentSettingsPath;  // Full name of current settings file
  CString mOriginalSettingsPath; // Full name of settings file read in at startup
  CString mCurrentDirReadIn;     // Current directory read from settings
  CString mPropertiesName;       // Root name of properties file
  CString mCalibrationName;      // Root name of calibration file
  CString mOriginalCwd;          // Original working directory
  CString mTitle;                // Title string
  CString mFrameTitle;           // Title for frame files
  CString mLogBook;              // Name of file to log tilt series in
  CString mShortTermName;        // Name of file for short term calibrations (dose)
  CString mPluginPath;           // Path name for plugins; replaces PF/SEM/Plugins
  CString mPluginPath2;          // Second path name for plugins; additional location
  CString mFlybackName;          // Name of flyback time file
  CString mCurScriptPackPath;    // Full name of current script package file
  BOOL mSettingsOpen;            // flag that settings file open
  BOOL mSettingsReadable;        // flag that settings file is readable
  BOOL mSysSettingsReadable;     // Flag that system file is rereadable
  int  mTrueLDArea;              // Saved Low dose area when reading settings
  BOOL mCalibBackedUp;           // Calibration file was backup in this session
  BOOL mSettingsBackedUp;        // Current settings file was backed up this session
  BOOL mScriptPackBackedUp;      // Current script package is backed up
  BOOL mAutoSaveSettings;        // Flag to autosave settings
  BOOL mAutoSaveNav;             // Flag to do autosave of navigator
  BOOL mShortTermBackedUp;       // Flag that short-term cal file is backed up
  BOOL mShortTermNotSaved;       // Flag that there are unsaved changes
  BOOL mIgnoreShortTerm;         // Flag not to read or write short-term cals
  int mNumCalsDone[NUM_CAL_DONE_TYPES];  // Keep track of # of calibrations done
  int mNumCalsAskThresh[NUM_CAL_DONE_TYPES];  // Threshold for each type for ask if save
  BOOL mFlybackBackedUp;         // Flag to keep track if flyback file is backed up
  int mMaxMontagePieces;         // Maximum pieces in one dimension
  int mMinMontageOverlap;        // Minimum overlap
  double mOverlapFraction;       // Fraction of larger dimension to overlap
  int mDefaultMontXpieces;       // Default number of montage pieces
  int mDefaultMontYpieces;
  float mMaxOverlapFraction;     // Maximum allowed overlap as fraction of frame size
  float mMinAnchorField;         // Default minimum field size for montage anchor images
  CTime mStartupTime;            // Program start time
  int mSTEMfileMode;             // STEM values for file properties
  int mSTEMunsignOpt;
  int mNonSTEMunsignOpt;       // Variables to save non-STEM values of file options
  int mNonSTEMfileMode;
  BOOL mSavedNonSTEMprops;       // Flag that they were saved
  WINDOWPLACEMENT mReadDlgPlace;
  int mFrameAdocIndex;            // Index for frame-saving .mdoc file
  int mFrameSetIndex;             // Index for next set to save
  CString mFrameFilename;         // Filename for frame mdoc
  int mDfltUseMdoc;              // Default setting for whether to use mdoc
  int mLastFrameSect;            // Index of last section in frame mdoc
  int mLastWrittenFrameSect;     // Sect index of last set written to file
  bool mDeferWritingFrameMdoc;   // Flag to wait until write call and do last section only
  BOOL mSkipFileDlg;             // Settings flag to skip the file dialog
  bool mShowFileDlgOnce;         // Flag to be able to show it once
  BOOL mAbandonSettings;         // Flag not to save settings on exit or autosave
  bool mReadScriptPack;          // Flag that a settings file had a script package path
  CString mBasicModeFile;        // File with disables/hides for basic mode
  CString mPreReadBasicFile;     // Filename before reading settings
  bool mHDFsupported;            // Flag that HDF files are supported

public:
  KImageStore * OpenNewFileByName(CString cFilename, FileOptions * fileOptp);
  int UserOpenOldMrcCFile(CFile ** file, CString &cFilename, bool imodOK);
  int OpenOldMrcCFile(CFile **file, CString cFilename, bool imodOK);
  int OpenOldFile(CFile *file, CString cFilename, int err, bool skipMontDlg);
  KImageStore * GetStoreForSaving(int type);
  int FilePropForSaveFile(FileOptions * fileOptp, int openAnyway);
  int FilenameForSaveFile(int fileType, LPCTSTR lpszFileName, CString & cFilename);
  void ManageSaveSingle(void);
  void SetFileOptsForSTEM(void);
  void RestoreFileOptsFromSTEM(void);
  void CopyMasterFileOpts(FileOptions * fileOptp, int fromTo);
  void MontParamInitFromConSet(MontParam *param, int setNum, float overlapFrac = 0.);
  void MontParamInitFromFrame(MontParam *param, int camNum, int xFrame, int yFrame, float overlapFrac);
  CString DateTimeForFrameSaving(void);
  void MakeSerialEMTitle(CString & titleStr, char * fullTitle);
  int OpenNewReplaceCurrent(CString filename, bool useMdoc, int fileType);
  CString DateTimeForTitle(bool year4digits = false);
  const char **GetMonthStrings();
  afx_msg void OnFileOpenMdoc();
  afx_msg void OnFileCloseMdoc();
  afx_msg void OnUpdateFileCloseMdoc(CCmdUI *pCmdUI);
  int SaveFrameDataInMdoc(KImage * image);
  afx_msg void OnUpdateFileOpenMdoc(CCmdUI *pCmdUI);
  int GetTextFileName(bool openOld, bool originalDir, CString &pathname,
    CString *filename = NULL, CString *initialDir = NULL, const char *filter = NULL, bool allowOverwrite = false);
  void DateTimeComponents(CString & date, CString & time, BOOL numericDate, bool unique = true);
  int AddValueToFrameMdoc(CString key, CString value);
  int WriteFrameMdoc(void);
  void SetInitialDirToCurrentDir();
  const char *GetInitialDir();
  int UpdateLastMdocFrame(KImage * image);
  void ComposeTitlebarLine(void);
  void CalibrationWasDone(int type);
  int SaveToOtherFile(int buffer, int fileType, int compression, CString *filename);
  bool FieldAboveStageMoveThreshold(MontParam *param, BOOL lowDose, int camInd);
  afx_msg void OnSkipFilePropertiesDlg();
  afx_msg void OnUpdateSkipFilePropertiesDlg(CCmdUI *pCmdUI);
void CopyDefaultToOtherFileOpts(void);
afx_msg void OnSettingsDiscardOnExit();
afx_msg void OnUpdateSettingsDiscardOnExit(CCmdUI *pCmdUI);
int AddTitlesToFrameMdoc(CString &message);
int DoOpenFrameMdoc(CString & filename);
void DoCloseFrameMdoc();
afx_msg void OnSettingsBasicmode();
afx_msg void OnUpdateSettingsBasicmode(CCmdUI *pCmdUI);
int DoFileOpenold();
afx_msg void OnCloseAllFiles();
afx_msg void OnUpdateCloseAllFiles(CCmdUI *pCmdUI);
afx_msg void OnReadBasicModeFile();
afx_msg void OnUpdateReadBasicModeFile(CCmdUI *pCmdUI);
afx_msg void OnFileSetCurrentDirectory();
};

// FILE DIALOG CLASS and associated thread class and data

struct MyFileDlgThreadData
{
  BOOL bOpenFileDialog;
  LPCTSTR lpszDefExt;
  LPCTSTR lpszFileName;
  DWORD dwFlags;
  LPCTSTR lpszFilter;
  LPCTSTR lpstrInitialDir;
  CString fileName;
  CString pathName;
  int retval;
  BOOL done;
  BOOL bSetCurrentDir;
};


class MyFileDialog
{
public:
  MyFileDialog(BOOL bOpenFileDialog, LPCTSTR lpszDefExt = NULL,
    LPCTSTR lpszFileName = NULL, DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
    LPCTSTR lpszFilter = NULL, CWnd* pParentWnd = NULL, BOOL setCurrentDir = true);
  ~MyFileDialog();
  int DoModal();
  CString GetFileName() {return mfdTD.fileName;};
  CString GetPathName() {return mfdTD.pathName;};

  MyFileDlgThreadData mfdTD;
#ifndef USE_SDK_FILEDLG
private:
  CFileDialog *mFileDlg;
#endif
};

#ifdef USE_SDK_FILEDLG
#define USE_DUMMYDLG
#define USE_THREAD
int RunSdkFileDlg(MyFileDlgThreadData *mfdTDp);

class MyFileDlgThread : public CWinThread
{
  DECLARE_DYNCREATE(MyFileDlgThread)
public:
  MyFileDlgThread() {};
  BOOL InitInstance();
  MyFileDlgThreadData *mfdTDp;
};
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERIALEMDOC_H__A0C8625F_02C5_4A68_9EAF_11F5AC5BA286__INCLUDED_)
