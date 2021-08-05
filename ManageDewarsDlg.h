#pragma once
#include "afxwin.h"


// CManageDewarsDlg dialog

class CManageDewarsDlg : public CBaseDlg
{

public:
	CManageDewarsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CManageDewarsDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MANAGE_DEWARS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual BOOL OnInitDialog();
  virtual void OnOK();

	DECLARE_MESSAGE_MAP()

private:
  int mPanelStart[6];
  int mNumInPanel[6];

public:
  BOOL m_bCheckPVPrunning;
  afx_msg void OnCheckPvpRunning();
  BOOL m_bRunBufferCycle;
  afx_msg void OnRunBufferCycle();
  CEdit m_editBufferTime;
  int m_iBufferTime;
  BOOL m_bRunAutoloader;
  afx_msg void OnRunAutoloaderCycle();
  CEdit m_editAutoloaderTime;
  int m_iAutoloaderTime;
  BOOL m_bRefillDewars;
  afx_msg void OnRefillDewars();
  afx_msg void OnEnKillfocusEditBox();
  CEdit m_editRefillTime;
  float m_fRefillTime;
  BOOL m_bCheckDewars;
  afx_msg void OnCheckDewars();
  CEdit m_editPauseBefore;
  float m_fPauseBefore;
  CButton m_butStartRefill;
  BOOL m_bStartRefill;
  afx_msg void OnStartRefill();
  CEdit m_editStartIfBelow;
  float m_fStartIfBelow;
  CEdit m_editWaitAfter;
  float m_fWaitAfter;
  CButton m_butDoCheckBeforeTask;
  afx_msg void OnDoChecksJustBefore();
  void ManageEnables();
  BOOL m_bDoCheckJustBefore;
};
