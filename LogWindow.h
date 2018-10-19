#if !defined(AFX_LOGWINDOW_H__388EE9A3_26B0_4864_AD88_69AAC92727EA__INCLUDED_)
#define AFX_LOGWINDOW_H__388EE9A3_26B0_4864_AD88_69AAC92727EA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LogWindow.h : header file
//
#include "afxwin.h"
#include "BaseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CLogWindow dialog

class CLogWindow : public CBaseDlg
{
// Construction
public:
	int GetFileName(BOOL oldFile, UINT flags);
	int ReadAndAppend();
	int OpenAndWriteFile(UINT flags, int length, int offset);
	int UpdateSaveFile(BOOL createIfNone, CString stackName = "");
  int AskIfSave(CString strReason);
  int Save();
  int SaveAs();
  int DoSave();
  SetMember(BOOL, Unsaved);
  GetMember(CString, SaveFile);
  GetMember(CString, LastFilePath);
  void Append(CString inString, int lineFlags);
  CLogWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
  //{{AFX_DATA(CLogWindow)
  enum { IDD = IDD_LOGWINDOW };
  CEdit m_editWindow;
  CString m_strLog;
  //}}AFX_DATA


// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CLogWindow)
  protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

// Implementation
protected:

  // Generated message map functions
  //{{AFX_MSG(CLogWindow)
  afx_msg void OnCancel();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  virtual BOOL OnInitDialog();
	afx_msg void OnMove(int x, int y);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
private:
  int m_iBorderY;
  int m_iBorderX;
  BOOL mUnsaved;
  BOOL mInitialized;
  CString mSaveFile; 
  int mLastSavedLength;
  int mAppendedLength;
  CString mLastStackname;
  CString mLastFilePath;
public:
  BOOL SaveFileNotOnStack(CString stackName);
  void CloseLog(void) {OnCancel();};
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOGWINDOW_H__388EE9A3_26B0_4864_AD88_69AAC92727EA__INCLUDED_)
