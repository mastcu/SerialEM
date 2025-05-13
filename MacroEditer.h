#include "afxwin.h"
#include "BaseDlg.h"
#if !defined(AFX_MACROEDITER_H__8A1B3AE8_7117_46A6_9561_5A97D31CE9C0__INCLUDED_)
#define AFX_MACROEDITER_H__8A1B3AE8_7117_46A6_9561_5A97D31CE9C0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MacroEditer.h : header file
//

class CMacCmd;

/////////////////////////////////////////////////////////////////////////////
// CMacroEditer dialog

class CMacroEditer : public CBaseDlg
{
// Construction
public:
  CMacroEditer(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
  //{{AFX_DATA(CMacroEditer)
	enum { IDD = IDD_MACRO };
  CButton m_butPrevMacro;
  CButton m_butNextMacro;
  CButton m_butSaveAs;
  CButton m_butSave;
  CButton m_butRun;
  CButton m_butLoad;
  CButton m_butCancel;
  CButton m_butOK;
  CEdit m_editMacro;
  CString m_strMacro;
	//}}AFX_DATA

  CString m_strTitle;
  int m_iMacroNumber;

// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMacroEditer)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

// Implementation
public:
  BOOL OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
  void TransferMacro(BOOL fromEditor);
  void UpdateButtons();
  void SetTitle(void);
  int AdjacentMacro(int dir);
  void SwitchMacro(int dir);

protected:

  // Generated message map functions
  //{{AFX_MSG(CMacroEditer)
  virtual BOOL OnInitDialog();
  virtual BOOL PreTranslateMessage(MSG* pMsg);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  virtual void OnOK();
  virtual void OnCancel();
  afx_msg void OnRunmacro();
  afx_msg void OnSavemacro();
  afx_msg void OnSavemacroas();
  afx_msg void OnLoadmacro();
  afx_msg void OnBnClickedToprevmacro();
  afx_msg void OnBnClickedTonextmacro();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()

private:
  void DoSave();
  int m_iBorderY;
  int m_iBorderX;
  BOOL mInitialized;
  int m_iOKoffset;
  int m_iOKLeft;
  int m_iCancelLeft;
  int m_iRunLeft;
  int m_iHelpLeft;
  int m_iSaveOffset;
  int m_iLoadLeft;
  int m_iSaveLeft;
  int m_iSaveAsLeft;
  int m_iToLineLeft;
  int m_iEditOffset;
  int m_iEditLeft;
  int m_iCompLeft;
  int m_iCompOffset;
  CSerialEMApp *mWinApp;
  CMacCmd * mProcessor;
  CMacroEditer **mEditer;
  CString *mMyMacro;
  CString *mSaveFile;  // Full path of save file
  CString *mFileName;  // Filename only for title bar
  CString *mMacroName; // Macro name
  CString mLastFindString;
  bool mLoadUncommitted;   // Flag that script loaded from file hasn't been transferred
  int mLineForSignature;

public:
  afx_msg void OnEnChangeEditmacro();
  CButton m_butShiftUp;
  CButton m_butShiftDown;
  afx_msg void OnShiftMacroUp();
  afx_msg void OnShiftMacroDown();
  void ShiftMacro(int dir);
  void AdjustForNewNumber(int newNum);
  CString m_strCompletions;
  CStatic m_statCompletions;
  CButton m_butFindInMacro;
  afx_msg void OnFindInMacro();
  void EnsureLineVisible(int line);
  static bool FindIndentAndMatch(CString &strMacro, int lineStart, int limit, const char ** keywords, int numKeys, 
    int & curIndent);
  static void HandleCompletionsAndIndent(CString &strMacro, CString &strCompletions,
    int &sel2, bool &setCompletions, bool &completing, bool oneLine, int &lineForSignature, int curLineNum);
  static int IndentCurrentLine(CString &strMacro, int &sel2, bool isPython);
  static bool CheckForPythonAndImport(CString &strMacro, CString &importName);
  static bool GetPrevLineIndexes(CString &strMacro, int curStart, bool isPython, 
    int &indStart, int &indEnd, bool &isContinued);
  static void ListDebugKeyLetters();
  void JustCloseWindow(void);
  afx_msg void OnButToMacroLine();
  void SelectAndShowLine(int lineFromOne);
  CButton m_butToMacroLine;
  static CFont mMonoFont;
  static CFont mDefaultFont;
  static int mHasMonoFont;
  static void MakeMonoFont(CWnd *edit);
  CStatic m_statIndent;
  CButton m_butFixIndent;
  CButton m_butAddIndent;
  CButton m_butRemoveIndent;
  afx_msg void OnButFixIndent();
  afx_msg void OnButAddIndent();
  afx_msg void OnButRemoveIndent();
  int GetLineSelectLimits(int &startInd, int &endInd, int &indentSize);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MACROEDITER_H__8A1B3AE8_7117_46A6_9561_5A97D31CE9C0__INCLUDED_)
