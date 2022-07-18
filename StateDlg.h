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
  int mSetStateIndex[MAX_SAVED_STATE_IND + 1];
  BOOL mInitialized;
  CArray<StateParams *, StateParams *> *mStateArray;
  CNavHelper *mHelper;
  StateParams *mParam;
  int mNumInPanel[2];
  int mPanelStart[2];
  bool mAdjustingPanels;
  bool mWarnedSharedParams;
  bool mWarnedNoMontMap;
  bool mRemindedToGoTo;
  int mCamOfSetState;

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

  afx_msg void OnEnChangeStatename();
  afx_msg void OnSelchangeListStates();
  afx_msg void OnDblclkListStates();
  afx_msg void OnButAddCurState();
  afx_msg void OnButAddNavState();
  afx_msg void OnButDelState();
  afx_msg void OnButSetImState();
  afx_msg void OnButSetMapState();
  afx_msg void OnButRestoreState();
  afx_msg void OnButForgetState();
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
  void StateToListString(StateParams *state, CString &string, const char *sep, int index);
  afx_msg void OnButUpdateState();
  CButton m_butUpdate;
  void DisableUpdateButton(void);
  void UpdateHiding(void);
  CButton m_butSaveDefocus;
  afx_msg void OnButSaveDefocus();
  CString m_strPriorSummary;
  CButton m_butAddMontMap;
  afx_msg void OnButAddMontMap();
};
