#pragma once
#include "BaseDlg.h"
#include "afxwin.h"
#include "afxcmn.h"


// CNavAcquireDlg dialog

class CNavAcquireDlg : public CBaseDlg
{

public:
	CNavAcquireDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNavAcquireDlg();

// Dialog Data
	enum { IDD = IDD_NAVACQUIRE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
  int m_iAcquireType;
  NavParams *mParam;
  CButton m_butRestoreState;
  BOOL m_bRestoreState;
  BOOL m_bRoughEucen;
  BOOL m_bRealign;
  BOOL m_bFineEucen;
  BOOL m_bAutofocus;
  bool mPostponed;
  int mMacroNum;
  int mPremacNum;
  int mNumArrayItems;
  int mNumAcqBeforeFile;
  int mNumFilesToOpen;
  int mLastNonTStype;
  CString mOutputFile;
  BOOL mIfMontOutput;
  afx_msg void OnNaRealignitem();
  afx_msg void OnNaAcquiremap();
  void ManageMacro(void);
  CButton m_butCloseValves;
  CButton m_butAcquireTS;
  BOOL m_bCloseValves;
  BOOL m_bCookSpec;
  BOOL m_bCenterBeam;
  CButton m_butSendEmail;
  BOOL m_bSendEmail;
  BOOL mAnyAcquirePoints;
  BOOL mAnyTSpoints;
  CButton m_butAcquireMap;
  CButton m_butRunMacro;
  CButton m_butJustAcquire;
  CButton m_butPremacro;
  BOOL m_bPremacro;
  afx_msg void OnPremacro();
  CButton m_butGroupFocus;
  BOOL m_bGroupFocus;
  afx_msg void OnNaAutofocus();
  void LoadTSdependentToDlg(void);
  void UnloadTSdependentFromDlg(int acquireType);
  CComboBox m_comboPremacro;
  afx_msg void OnSelendokComboPremacro();
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
  void ManageForSubset(void);
  CButton m_butSkipInitialMove;
  BOOL m_bSkipInitialMove;
  afx_msg void OnRoughEucen();
  BOOL m_bSkipZmoves;
  afx_msg void OnCookSpecimen();
  afx_msg void OnFineEucen();
  afx_msg void OnSkipInitialMove();
  afx_msg void OnButPostpone();
};
