#pragma once

#define NUM_ONE_LINE_SCRIPT_MSG 4
#define MAX_MSG_CYCLES 50
#define MSG_CYCLE_TIME 5.
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
  void HandleHistoryCompletion(CString &strMacro, CString &strCompletion,
    int &sel2, bool &setCompletion, int curLineNum);
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
  int mLineForSignature;
  int mCurMessageInd;
  double mLastTime;
  CString mMessages[NUM_ONE_LINE_SCRIPT_MSG];
  bool mCycleMessages;
  int mNumMsgCycles;
public:
  CButton m_butRun[MAX_ONE_LINE_SCRIPTS];
  afx_msg void OnEnKillfocusEditOneLine(UINT nID);
  afx_msg void OnEnSetfocusEditOneLine(UINT nID);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
};
