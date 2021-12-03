#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"
#include "NavHelper.h"


// CNavAcquireDlg dialog

class CNavAcquireDlg : public CBaseDlg
{

public:
	CNavAcquireDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavAcquireDlg();
  void CloseWindow();

// Dialog Data
	enum { IDD = IDD_NAVACQUIRE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
  virtual void OnCancel();
  void DoCancel();
  virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);

	DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[5];
  int mNumInPanel[5];
  int mAllOrders[2][NAA_MAX_ACTIONS];
  int *mCurrentOrder;
  int mNumActions;
  int mNumShownActs;
  int mShownPosToIndex[NAA_MAX_ACTIONS];
  int mUnhiddenPosMap[NAA_MAX_ACTIONS];
  NavAcqAction *mMasterActions;
  NavAcqAction mAllActions[2][NAA_MAX_ACTIONS];
  NavAcqAction *mActions;
  int mCurActSelected;
  int mFirstPosAfterTask;
  int mOKtoAdjustBT;
  int *mMasterOrder;
  CFont mBoldFont;

public:
  int CheckActionOrder(int *order);
  int m_iAcquireChoice;
  NavAcqParams *mParam;
  NavAcqParams *mMasterParam;
  NavAcqParams mAllCurParam[2];
  int mPostponed;
  int mMacroNum;
  int mPremacNum;
  int mPostmacNum;
  int mNumArrayItems;
  int mNumAcqBeforeFile;
  int mNumFilesToOpen;
  int mLastNonTStype;
  CString mOutputFile;
  BOOL mIfMontOutput;
  afx_msg void OnNaAcquiremap();
  void ManageMacro(void);
  CButton m_butCloseValves;
  CButton m_butAcquireTS;
  CButton m_butDoMultishot;
  BOOL m_bCloseValves;
  CButton m_butSendEmail;
  BOOL m_bSendEmail;
  BOOL mAnyAcquirePoints;
  BOOL mAnyTSpoints;
  CButton m_butAcquireMap;
  CButton m_butRunMacro;
  void LoadTSdependentToDlg(void);
  void UnloadDialogToCurParams();
  void LoadParamsToDialog();
  void UnloadTSdependentFromDlg(int acquireType);
  CComboBox m_comboPremacro;
  CComboBox m_comboPostmacro;
  afx_msg void OnSelendokComboPremacro();
  afx_msg void OnSelendokComboPostmacro();
  CComboBox m_comboMacro;
  afx_msg void OnSelendokComboMacro();
  CString m_strSavingFate;
  CString m_strFileSavingInto;
  void ManageOutputFile(void);
  BOOL m_bDoSubset;
  afx_msg void OnDoSubset();
  CEdit m_editSubsetStart;
  int m_iSubsetStart;
  CEdit m_editSubsetEnd;
  int m_iSubsetEnd;
  afx_msg void OnKillfocusSubsetStart();
  afx_msg void OnKillfocusSubsetEnd();
  void ManageEnables(void);
  void ExternalUpdate();
  void ManageForSubset(void);
  void BuildActionSection(void);
  void FormatTimingString(int actInd, int posInd);
  void NewActionSelected(int posInd);
  void ManageTimingEnables();
  CButton m_butSkipInitialMove;
  BOOL m_bSkipInitialMove;
  BOOL m_bSkipZmoves;
  afx_msg void OnSkipInitialMove();
  afx_msg void OnButPostpone();
  int m_iSelectedPos;
  int m_iTimingType;
  afx_msg void OnRadioTimingType();
  afx_msg void OnRadioSelectAction(UINT nID);
  afx_msg void OnCheckRunAction(UINT nID);
  afx_msg void OnButSetupAction(UINT nID);
  CEdit m_editEveryN;
  int m_iEveryNitems;
  CSpinButtonCtrl m_sbcEveryN;
  CEdit m_editAfterMinutes;
  int m_iAfterMinutes;
  CEdit m_editWhenMoved;
  float m_fWhenMoved;
  afx_msg void OnEnKillfocusEditEveryN();
  afx_msg void OnEnKillfocusEditAfterMinutes();
  afx_msg void OnEnKillfocusEditWhenMoved();
  BOOL m_bRunAtOther;
  afx_msg void OnNaRunAtOther();
  int m_iGotoLabelNote;
  CEdit m_editGotoItem;
  CString m_strGotoItem;
  afx_msg void OnEnKillfocusEditGotoItem();
  int CheckNearestItemText();
  CButton m_butMoveUp;
  CButton m_butMoveDown;
  afx_msg void OnButMoveUp();
  afx_msg void OnButMoveDown();
  void MoveAction(int dir);
  afx_msg void OnButHideAction();
  afx_msg void OnButShowAll();
  afx_msg void OnButDefaultOrder();
  BOOL m_bRunAfterTask;
  afx_msg void OnNaRunAfterTask();
  afx_msg void OnRgotoLabel();
  CButton m_butSetupMultishot;
  afx_msg void OnNaSetupMultishot();
  BOOL m_bCycleDefocus;
  afx_msg void OnDeltaposSpinEveryN(NMHDR *pNMHDR, LRESULT *pResult);
  CEdit m_editCycleFrom;
  float m_fCycleFrom;
  CEdit m_editCycleTo;
  float m_fCycleTo;
  CString m_strCycleUm;
  CSpinButtonCtrl m_sbcCycleDef;
  afx_msg void OnNaCycleDefocus();
  afx_msg void OnDeltaposSpinCycleDef(NMHDR *pNMHDR, LRESULT *pResult);
  afx_msg void OnNaEarlyReturn();
  CEdit m_editEarlyFrames;
  int m_iEarlyFrames;
  CStatic m_statActionGroup;
  CStatic m_statSelectedGroup;
  BOOL m_bEarlyReturn;
  CButton m_butEarlyReturn;
  BOOL m_bNoMBoxOnError;
  afx_msg void OnNaSkipSaving();
  CButton m_butSkipSaving;
  BOOL m_bSkipSaving;
  afx_msg void OnNaHideUnused();
  BOOL m_bHideUnusedActions;
  CButton m_butAdjustBtforIS;
  BOOL m_bAdjustBTforIS;
  int m_iCurParamSet;
  afx_msg void OnRadioCurParamSet();
  CButton m_butHybridRealign;
  BOOL m_bHybridRealign;
  BOOL m_bRelaxStage;
  afx_msg void OnSaveParams();
  afx_msg void OnReadParams();
  BOOL m_bHideUnselectedOpts;
  afx_msg void OnHideUnselectedOptions();
  CButton m_butOKGO;
  int m_iMapWithViewSearch;
  afx_msg void OnMapWithRecViewSearch();
  CButton m_butSaveAsMap;
  BOOL m_bSaveAsMap;
  afx_msg void OnSaveAsMap();
  void AcquireTypeToOptions(int acqType);
  int OptionsToAcquireType();
};
