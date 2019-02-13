#pragma once


// COneLineScript dialog

#pragma once
#include "afxwin.h"
#include "BaseDlg.h"

class COneLineScript : public CBaseDlg
{

public:
	COneLineScript(CWnd* pParent = NULL);   // standard constructor
	virtual ~COneLineScript();

// Dialog Data
	enum { IDD = IDD_ONELINESCRIPT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
  virtual void PostNcDestroy();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
public:
  CString m_strCompletions;
  CEdit m_editOneLine;
  CString m_strOneLine;
  afx_msg void OnEnChangeEditOneLine();
  CString *mMacros;
  void Update();
private:
  int m_iRunLeftOrig, m_iRunTop;
  int m_iEditXorig, m_iEditHeight;
  int m_iWinXorig;
  bool mInitialized;
public:
  CButton m_butRun;
};
