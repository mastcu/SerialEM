#pragma once
#include "afxwin.h"
#include "BaseDlg.h"
#include "afxtempl.h"

class CNavHelper;
struct StateParams;

// CStateDlg dialog
#define MAX_SAVED_STATE_IND (MAX_LOWDOSE_SETS + 1)
class CStateDlg : public CBaseDlg
{
public:
	CStateDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStateDlg();

// Dialog Data
	enum { IDD = IDD_STATEDLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnCancel();
	afx_msg void OnOK();
	afx_msg void OnMove(int x, int y);

	DECLARE_MESSAGE_MAP()

private:
  int mCurrentItem;
  int mListBorderX, mListBorderY;
  int mNameBorderX;
  int mNameHeight;
  static int mSetStateIndex[MAX_SAVED_STATE_IND + 1];
  BOOL mInitialized;
  static CArray<StateParams *, StateParams *> *mStateArray;
  static CNavHelper *mHelper;
  static StateParams *mParam;
  static CSerialEMApp *winApp;
  int mNumInPanel[2];
  int mPanelStart[2];
  bool mAdjustingPanels;
  static bool mWarnedSharedParams;
  static bool mWarnedNoMontMap;
  static bool mRemindedToGoTo;
  static int mCamOfSetState;

public:
  CButton m_butAddCurState;
  CButton m_butAddNavItemState;
  CButton m_butDelState;
  CEdit m_editName;
  CString m_strName;
  CListBox m_listViewer;
  CButton m_butSetImState;
  CButton m_butSetMapState;
  CButton m_butRestore;
  CButton m_butForget;
  CButton m_butSetSchedState;
  GetMember(int, CurrentItem);
  static int *GetSetStateIndex() {return &mSetStateIndex[0] ; };

  afx_msg void OnEnChangeStatename();
  afx_msg void OnSelchangeListStates();
  afx_msg void OnDblclkListStates();
  afx_msg void OnButAddCurState();
  afx_msg void OnButAddNavState();
  afx_msg void OnButDelState();
  afx_msg void OnButSetImState();
  static int DoSetImState(int stateNum, CString &errStr);
  afx_msg void OnButSetMapState();
  afx_msg void OnButRestoreState();
  static void DoRestoreState(bool skipScope);
  afx_msg void OnButForgetState();
  static int SetStateByNameOrNum(CString name, CString &errStr);
  static int LookupStateByNameOrNum(CString name, int &selInd, CString &errStr);
  void Update(void);
  void UpdateListString(int index);
  void FillListBox(void);
  BOOL SetCurrentParam(void);
  void AddNewStateToList(void);
  void ManageName(void);
  void UpdateSettings(void);
  int CurrentMatchesSetState();
  afx_msg void OnButSetSchedState();
  void StateToListString(int index, CString &string);
  static void StateToListString(StateParams *state, CString &string, const char *sep, int index,
    int *setStateIndex, int numSet, BOOL showNumber);
  afx_msg void OnButUpdateState();
  static void DoUpdateState(int selInd, StateParams *param, int area);
  static int UpdateStateByNameOrNum(CString name, CString &errStr);
  CButton m_butUpdate;
  void DisableUpdateButton(void);
  void UpdateHiding(void);
  CButton m_butSaveDefocus;
  afx_msg void OnButSaveDefocus();
  CString m_strPriorSummary;
  CButton m_butAddMontMap;
  afx_msg void OnButAddMontMap();
  static SetMember(int, CamOfSetState);
  static GetMember(int, CamOfSetState);
  static void SetStaticPointers();

  BOOL m_bShowNumber;
  afx_msg void OnCheckNumber();
  CButton m_butListState;
  afx_msg void OnButListState();
  CButton m_butSetApertures;
  BOOL m_bSetApertures;
  afx_msg void OnCheckSetApertures();
};
