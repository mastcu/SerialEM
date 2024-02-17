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
	int GetFileName(BOOL oldFile, UINT flags, LPCTSTR offerName);
	int ReadAndAppend();
	int OpenAndWriteFile(UINT flags, int length, int offset);
	int UpdateSaveFile(BOOL createIfNone, CString stackName = "", BOOL replace = false, 
    BOOL overwrite = false);
  int AskIfSave(CString strReason);
  int Save();
  int SaveAs();
  int SaveAndOfferName();
  int DoSave(CString oldFile = "");
  SetMember(BOOL, Unsaved);
  GetMember(CString, SaveFile);
  GetMember(CString, LastFilePath);
  GetMember(int, TypeOfFileSaved);
  void Append(CString &inString, int lineFlags);
  void Append(CString &inString, int lineFlags, int colorIndex, int style);
  void Append(CString &inString, int lineFlags, int red, int green, int blue, int style);
  int SetNextLineColorStyle(int colorIndex, int style);
  int SetNextLineColorStyle(int red, int green, int blue, int style);
  void DoAppend(CString &inString, int lineFlags, int red, int green, int blue, int style);
  void UpdateAndScrollToEnd();
  void FlushDeferredLines();
  int GetNumVisibleLines();
  int StringIndexToRichEditSel(CString &str, int index);
  CLogWindow(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
  //{{AFX_DATA(CLogWindow)
  enum { IDD = IDD_LOGWINDOW };
  CRichEditCtrl m_editWindow;
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
  afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);

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
  CString mDeferredLines;    // String to buffer lines for output
  int mNumDeferred;          // Number of lines or additions to string
  double mStartedDeferTime;  // Time when this started being accumulate
  int mMaxLinesToDefer;      // Maximum number of lines to hold
  float mMaxSecondsToDefer;  // And maximum time to hold lines
  int mPrunedLength;         // Cumulative length pruned from start of text string
  ULONGLONG mPrunedPosition; // Position of file where pruned length was written
  ULONGLONG mLastPosition;   // Last one saved to
  CString mTempPruneName;    // Temporary (or not) file for saving pruned if no file set
  int mNextRed, mNextGreen, mNextBlue;
  int mNextStyle;
  unsigned char *mPalette;
  long mLastAppendSel;        // Selection index after last append
  int mTypeOfFileSaved;       // 0 not saved yet, 1 as text, 0 as rtf

public:
  BOOL SaveFileNotOnStack(CString stackName);
  void CloseLog(void) {OnCancel();};
  CEdit m_editDummy;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LOGWINDOW_H__388EE9A3_26B0_4864_AD88_69AAC92727EA__INCLUDED_)
