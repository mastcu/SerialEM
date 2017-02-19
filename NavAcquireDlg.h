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
  int mMacroNum;
  int mPremacNum;
  int mNumAcqBeforeFile;
  int mNumFilesToOpen;
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
};
