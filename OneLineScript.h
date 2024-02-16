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
  virtual void OnRunClicked(UINT nID);
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  virtual void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
public:
  CString m_strCompletions;
  CStatic m_statCompletions;
  CEdit m_editOneLine[MAX_ONE_LINE_SCRIPTS];
  CString m_strOneLine[MAX_ONE_LINE_SCRIPTS];
  afx_msg void OnEnChangeEditOneLine(UINT nID);
  CString *mMacros;
  void Update();
private:
  int m_iRunLeftOrig;
  int m_iRunTop[MAX_ONE_LINE_SCRIPTS];
  int m_iEditXorig, m_iEditHeight;
  int m_iWinXorig;
  bool mInitialized;
  int mLineWithFocus;
  int m_iCompLeft;
  int m_iCompOffset;
  int m_iCompWidthOrig;
public:
  CButton m_butRun[MAX_ONE_LINE_SCRIPTS];
  afx_msg void OnEnKillfocusEditOneLine(UINT nID);
  afx_msg void OnEnSetfocusEditOneLine(UINT nID);
};
