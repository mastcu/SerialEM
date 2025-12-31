#pragma once
#include "afxwin.h"

#define MAX_MULGRID_PANELS 11
// CMultiGridDlg dialog

class CMultiGridDlg : public CBaseDlg
{

public:
	CMultiGridDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMultiGridDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MULTI_GRID };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();
  virtual void OnCancel();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void OnPaint();

	DECLARE_MESSAGE_MAP()

private:
  CMultiGridTasks *mMGTasks;
  CEMscope *mScope;
  int mPanelStart[MAX_MULGRID_PANELS];
  int mNumInPanel[MAX_MULGRID_PANELS];
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;
  BOOL mPanelStates[MAX_MULGRID_PANELS];
  CFont mBiggerFont;
  CFont mBigBoldFont;
  int mNumUsedSlots;       // Number of slots in table
  bool mNamesChanged;      // Flag that names have been changed by user
  int mSelectedGrid;       // Index of grid selected by radio push buttons
  bool mSelGridOnStage;    // Flag if selected grid is on the stage
  CFont *mBoldFont;
  CArray<StateParams *, StateParams *> *mStateArray;
  MultiGridParams mParams;  // Local parameter copy
  MultiGridParams *mMasterParams;   // Pointer to master parameters
  IntVec mDlgIndToJCDindex;   // Map from indexes in dialog to indexes in catalogue
  int mOnStageDlgIndex;       // Index in dialog of grid on stage
  bool mGridMontSetupOK;      // Flag that grid montage setup should be OK
  int mGridMontSetAtXpcs, mGridMontSetAtYpcs;    // # of X and Y pieces in last setup
  int mGridMontSetupState;    // State in last setup
  int mGridMontSetupAcqType;  // Image type in last setup
  int mGridMontSetupOverlap;  // Overlap in last LMM setup
  int mMacNumAtEnd;           // Script to run at end
  int mMMMsetupAcqType;       // Imaging type in last MMM setup
  int mMMMsetupMontType;      // Montage/single type in last MMM setup
  int mMMMsetupStateForMont;  // State used for MMM montage in last setup
  bool mMMMsetupOK;           // Flag that MMM setup should be OK
  int mMMMmontWasSetup;       // Flag that MMM montage was set up (interactively)
  int *mMontSetupSizesXY;     // Address of array with sizes in MGTasks
  int mMMMneedsLowDose;       // Whether MMM needs low dose turned off (-1) or on (1)
  int mLMneedsLowDose;        // Whether LMM needs low dose turned off (-1) or on (1)
  bool mEnableRunUndone;      // Flag that Run Undone can be enabled
  int mNumMMMcombos;          // Number of combo boxes being shown
  int mNumFinalCombos;
  int mRightIsOpen;           // Flag that right side is open
  bool mClosedAll;
  BOOL mSavedPanelStates[MAX_MULGRID_PANELS];
  bool mDropFrameOption;      // flag to drop the option to put frames under session
  bool mSingleGridMode;       // Flag for no autoloader
  bool mSettingOrder;          // Flag that order is being set
  IntVec *mCustomRunDlgInds;
  ShortVec mWasAddedToOrder;


public:
  CString m_strCurrentDir;
  CString m_strRootname;
  BOOL m_bAppendNames;
  BOOL m_bUseSubdirs;
  CEdit m_editPrefix;
  CString m_strPrefix;
  afx_msg void OnCheckMgAppendName();
  afx_msg void OnButMgGetNames();
  afx_msg void OnButMgInventory();
  afx_msg void OnButMgResetNames();
  afx_msg void OnButMgClearNames();
  afx_msg void OnButSetCurrentDir();
  GetSetMember(int, NumUsedSlots);
  GetMember(IntVec, DlgIndToJCDindex);
  GetMember(int, OnStageDlgIndex);
  void CloseWindow();

  afx_msg void OnEnChangeEditMgPrefix();
  afx_msg void OnEnKillfocusEditName(UINT nID);
  JeolCartridgeData FindCartDataForDlgIndex(int ind);
  void UpdateEnables();
  void CheckIfAnyUndoneToRun();
  void ManageFrameDirOption();
  void UpdateSettings();
  void UpdateCurrentDir();
  void ManageInventory(int locked);
  void ManagePanels();
  void ParamsToDialog();
  void SetAllComboBoxesFromNameOrNum();
  void SetFinalStateComboBoxes();
  void RepackStatesIfNeeded(int &numBoxes, int *stateNums, CString *stateNames);
  void DropComboBoxes(int numBoxes, UINT *comboIDs, UINT statID);
  void SetComboBoxFromNameOrNum(CComboBox &combo, int &num, CString &name, int addForNone);
  int FindUniqueStateFromName(CString &name);
  void DialogToParams();
  void GetFinalStateFomCombos(int *stateNums, CString *stateNames, int skipInd);
  void SyncToMasterParams();
  void GetStateFromComboBox(CComboBox &combo, int &num, CString &name, int addForNone);
  int GetListOfGridsToRun(ShortVec &jcdInds, ShortVec &slotNums);
  void DefaultRunDlgIndexes(IntVec &dlgInds);
  void DisplayRunOrder();
  int LookupRunOrder(IntVec &runDlgInds, int dlgInd);
  void SetRootname();
  void ReloadTable(int resetOrClear, int checkRuns);
  CString UserNameWithNote(CString &userName, CString &note);
  afx_msg void OnButMgSetup();
  afx_msg void OnRadioMulgridSelect(UINT nID);
  afx_msg void OnButLoadGrid();
  afx_msg void OnButRealignToGridMap();
  afx_msg void OnRadioLMMSearch();
  int m_iLMMacquireType;
  afx_msg void OnCheckSetLmmState();
  CComboBox m_comboLMMstate;

  BOOL m_bSetCondenser;
  BOOL m_bRemoveObjective;
  CEdit m_editCondenser;
  int m_iCondenserSize;
  afx_msg void OnEnKillfocusEditCondenser();
  afx_msg void OnButSetupLmmMont();
  int DoSetupLMMmont(bool skipDlg);
  StateParams *GetOrRefindState(int &stateNum, CString &stateName, const char *descrip,
  CComboBox &combo);
  int CheckMultipleStates(CComboBox **comboArr, int *stateNums, CString *stateNames, 
    CString descrip, int &numStates, int &camera, StateParams **states, int *lowDose, int *magInds,
    CString *errStr = NULL);
  int CheckFinalStates(int &numStates, int &camera, CString *errStr = NULL);
  void DeduceStateForMMM(int numStates, int camera, StateParams **states, int *lowDose, 
    int *magInds, bool &useLD, bool &useSearch, bool &useView, int &useMontMap, int &useCamera, 
    int &useMag, int &useBin, int &stateForMont);
  BOOL m_bUseMontOverlap;
  afx_msg void OnCheckUseOverlap();
  int m_iMontOverlap;
  afx_msg void OnEnKillfocusEditOverlap();
  int m_iXpieces;
  afx_msg void OnEnKillfocusEditMontXpcs();
  int m_iYpieces;
  afx_msg void OnEnKillfocusEditMontNumYpcs();
  int m_iLMMmontType;
  afx_msg void OnRfullGridMont();
  afx_msg void OnButMgLowMagMaps();
  afx_msg void OnButMgMediumMagMaps();
  afx_msg void OnButMgFinalAcq();
  afx_msg void OnSelendokComboLmmState();
  CComboBox m_comboMMMstate1;
  afx_msg void OnSelendokComboMmmState1();
  CComboBox m_comboMMMstate2;
  afx_msg void OnSelendokComboMmmState2();
  CComboBox m_comboMMMstate3;
  afx_msg void OnSelendokComboMmmState3();
  afx_msg void OnSelendokComboMmmState4();
  afx_msg void OnSelendokComboFinalState1();
  afx_msg void OnSelendokComboFinalState2();
  afx_msg void OnSelendokComboFinalState3();
  afx_msg void OnSelendokComboFinalState4();
  void ProcessFinalStateCombo(CComboBox &combo, int index);
  void ProcessMultiStateCombo(CComboBox &combo, int *stateNums, CString *stateNames,
    int index, int gridID);
  void HandlePerGridStateChange();
  CComboBox m_comboFinalState3;
  CComboBox m_comboFinalState2;
  CComboBox m_comboFinalState1;
  CStatic m_statMgSetup;
  afx_msg void OnButSetupFinalAcq();
  afx_msg void OnButSetupMapping();
  afx_msg void OnButSetupPolymont();
  int m_iSingleVsPolyMont;
  afx_msg void OnRsingleImage();
  int m_iMMMacquireType;
  afx_msg void OnRmmmSearch();
  int SetupMMMacquire(bool skipDlg);
  void SaveMontSetupSize(MontParam *montP, BOOL lowDose, int &setupSizeX, int &setupSizeY);
  bool MontSetupSizesMatch(MontParam *montP, BOOL lowDose, int setupSizeX, int setupSizeY);
  int CheckRefineOptions(float &FOV, int &ldArea, int &setNum, StateParams **state, 
    int &numXpiece, int &numYpiece, CString errStr);
  int ManageRefineFOV();
  void ManageCondenserUnits();
  void LoadStatesInComboBox(CComboBox &combo, bool addNone);
  void LoadAllComboBoxes();
  void NewGridOnStage(int jcdInd);
  void InitForSingleGrid();
  void SetStatusText(int jcdInd);
  BOOL m_bSetLMMstate;
  BOOL m_bAutocontour;
  afx_msg void OnCheckRunLmms();
  CButton m_butTakeLMMs;
  BOOL m_bTakeLMMs;
  afx_msg void OnCheckTakeMmms();
  CButton m_butTakeMMMs;
  BOOL m_bTakeMMMs;
  afx_msg void OnCheckRunFinalAcq();
  CButton m_butRunFinalAcq;
  BOOL m_bRunFinalAcq;
  afx_msg void OnCheckSetCondenser();
  afx_msg void OnCheckAutocontour();
  CStatic m_statMgPrefix;
  afx_msg void OnButSetupAutocont();
  afx_msg void OnButStartRun();
  void DoStartRun(bool undoneOnly);
  CComboBox m_comboMMMstate4;
  CComboBox m_comboFinalState4;
  afx_msg void OnCheckMulgridRun(UINT nID);
  afx_msg void OnButRunUndone();
  CStatic m_statCurrentDir;
  afx_msg void OnButOpenNav();
  CString NavFileIfExistsAndNotLoaded();
  afx_msg void OnButSetGridType();
  CButton m_butCloseRight;
  afx_msg void OnButCloseRight();
  afx_msg void OnCheckSetFinalByGrid();
  BOOL m_bSetFinalByGrid;
  afx_msg void OnButRevertToGlobal();
  BOOL m_bFramesUnderSession;
  afx_msg void OnCheckFramesUnderSession();
  afx_msg void OnCheckMgUseSubdirs();
  afx_msg void OnButCloseAll();
  afx_msg void OnCheckToggleAll();
  BOOL m_bToggleAll;
  CString m_strNote;
  afx_msg void OnEnKillfocusEditGridNote();
  afx_msg void OnButSetOrder();
  afx_msg void OnButResetOrder();
  int GetNumAddedToOrder();
  afx_msg void OnButOpenLogs();
  bool FindLogFiles(std::vector<std::string> *strList, bool returnAll);
  CComboBox m_comboEndScript;
  BOOL m_bScriptAtEnd;
  afx_msg void OnCheckScriptAtEnd();
  afx_msg void OnSelendokComboEndScript();
  BOOL m_bRefineRealign;
  afx_msg void OnCheckMgRefine();
  BOOL m_iRefineRealign;
  afx_msg void OnRRefineRealign();
  CComboBox m_comboRefineState;
  afx_msg void OnSelendokComboRefineState();
  CString m_strRefineFOV;
  int m_iC1orC2;
  afx_msg void OnRsetC1Aperture();
  afx_msg void OnRvectorsFromMaps();
  int m_iVectorSource;
};
