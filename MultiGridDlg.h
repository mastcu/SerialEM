#pragma once
#include "afxwin.h"

#define MAX_MULGRID_PANELS 10
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

	DECLARE_MESSAGE_MAP()

private:
  CMultiGridTasks *mMGTasks;
  CEMscope *mScope;
  int mPanelStart[MAX_MULGRID_PANELS];
  int mNumInPanel[MAX_MULGRID_PANELS];
  CArray<JeolCartridgeData, JeolCartridgeData> *mCartInfo;
  int mNumUsedSlots;
  BOOL mPanelStates[MAX_MULGRID_PANELS];
  bool mNamesChanged;

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
  afx_msg void OnEnChangeEditMgPrefix();
  afx_msg void OnEnKillfocusEditName(UINT nID);
  void UpdateEnables();
  void UpdateSettings();
  void ManagePanels();
  void SetRootname();
  void ReloadTable(int resetOrClear);
  afx_msg void OnButMgSetup();
};
