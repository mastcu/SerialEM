// LogWindow.cpp:        The log window
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "SerialEMDoc.h"
#include "MacroProcessor.h"
#include "MacroEditer.h"
#include ".\LogWindow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLogWindow dialog


CLogWindow::CLogWindow(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CLogWindow::IDD, pParent)
{
  //{{AFX_DATA_INIT(CLogWindow)
  m_strLog = _T("");
  //}}AFX_DATA_INIT
  mNonModal = true;
  mUnsaved = false;
  mInitialized = false;
  mSaveFile = "";
  mLastSavedLength = 0;
  mLastStackname = "";
  mNumDeferred = 0;
  mDeferredLines = "";
  mMaxLinesToDefer = 100;
  mMaxSecondsToDefer = 2.;
}


void CLogWindow::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CLogWindow)
  DDX_Control(pDX, IDC_LOGEDIT, m_editWindow);
  DDX_Text(pDX, IDC_LOGEDIT, m_strLog);
	DDV_MaxChars(pDX, m_strLog, 25000000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLogWindow, CBaseDlg)
  //{{AFX_MSG_MAP(CLogWindow)
  ON_WM_SIZE()
  ON_WM_CLOSE()
	ON_WM_MOVE()
  ON_WM_ACTIVATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLogWindow message handlers

// External call to add text
void CLogWindow::Append(CString &inString, int lineFlags)
{
  bool deferring = (mWinApp->mMacroProcessor->DoingMacro() || 
    mWinApp->mMacroProcessor->GetNonMacroDeferring()) &&
    mWinApp->mMacroProcessor->GetDeferLogUpdates();
  bool replacing = (lineFlags & 2) != 0;

  // Flush any deferred lines if it is not happening, if line replacement, or too many
  // lines or too long has passed
  if (mNumDeferred > 0 && (!deferring || replacing || mNumDeferred > mMaxLinesToDefer ||
    SEMTickInterval(mStartedDeferTime) > 1000. * mMaxSecondsToDefer))
    FlushDeferredLines();

  // Defer by adding to string and line count, save start time
  if (deferring && !replacing) {
    if (!mNumDeferred)
      mStartedDeferTime = GetTickCount();
    mDeferredLines += inString;
    mNumDeferred++;
    return;
  }
  DoAppend(inString, lineFlags);
}

// The real routine for adding text to the log
void CLogWindow::DoAppend(CString &inString, int lineFlags)
{
  int oldLen, newLen, lastEnd;
  UpdateData(true);
  oldLen = m_strLog.GetLength();
  if (lineFlags & 2) {
    lastEnd = m_strLog.ReverseFind('\n');
    if (lastEnd > 0)
      m_strLog = m_strLog.Left(lastEnd + 1);
  }
  m_strLog += _T(inString);
  UpdateData(false);
  mUnsaved = true;
  newLen = m_strLog.GetLength();
  mAppendedLength += newLen - oldLen;

  // Scroll to the end - still can't get cursor there
  int line = m_editWindow.GetLineCount();
  m_editWindow.LineScroll(line);

  if (mWinApp->GetContinuousSaveLog()) {
    if (mSaveFile.IsEmpty() || newLen != mAppendedLength + mLastSavedLength)
      Save();
    else
      OpenAndWriteFile(CFile::modeWrite |CFile::shareDenyWrite, 
        mAppendedLength, mLastSavedLength);
  }
}

void CLogWindow::FlushDeferredLines()
{
  if (mNumDeferred > 0 || !mDeferredLines.IsEmpty()) {
    DoAppend(mDeferredLines, 0);
    mNumDeferred = 0;
    mDeferredLines = "";
  }
}

int CLogWindow::SaveAs()
{
  mLastStackname = "";
  if (GetFileName(FALSE, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT))
    return 1;
  return (DoSave());
}

// Get file name for new or existing file
int CLogWindow::GetFileName(BOOL oldFile, UINT flags)
{
  static char BASED_CODE szFilter[] = 
    "Log files (*.log)|*.log|All files (*.*)|*.*||";

  MyFileDialog fileDlg(oldFile, ".log", NULL, flags, szFilter);

  int result = fileDlg.DoModal();
  mWinApp->RestoreViewFocus();
  if (result != IDOK)
    return 1;

  mSaveFile = fileDlg.GetPathName();
  if (mSaveFile.IsEmpty())
    return 1;
  int trimCount = mSaveFile.GetLength() - (mSaveFile.ReverseFind('\\') + 1);
  CString trimmedName = mSaveFile.Right(trimCount);
  SetWindowText("Log:  " + trimmedName);
  return 0;
}


int CLogWindow::Save()
{
  if (mSaveFile.IsEmpty())
    return (SaveAs());
  else
    return (DoSave());
}

int CLogWindow::DoSave()
{
  FlushDeferredLines();
  UpdateData(true);
  int length = m_strLog.GetLength();
  return (OpenAndWriteFile(CFile::modeCreate | CFile::modeWrite |CFile::shareDenyWrite,
    length, 0));
}

// Flush new text to the save file, asking for a new one if createifNone is true and no
// stack name is supplied, or deriving a name from the stack name
int CLogWindow::UpdateSaveFile(BOOL createIfNone, CString stackName, BOOL replace,
  BOOL overwrite)
{
  int dirInd, extInd, fileNo;
  CString name;
  CFileStatus status;
  FlushDeferredLines();
  if (mSaveFile.IsEmpty() || (createIfNone && !stackName.IsEmpty() && replace)) {
    if (createIfNone) {
      if (stackName.IsEmpty())
        return (SaveAs());

      // Given a stack name, find extension if any and strip it off
      mLastStackname = stackName;
      dirInd = stackName.ReverseFind('\\');
      extInd = stackName.ReverseFind('.');
      if (extInd > dirInd && extInd > 0)
        stackName = stackName.Left(extInd);

      // Use name as is now if overwrite flag is set
      if (overwrite) {
        mSaveFile = stackName + ".log";
      } else {

        // Othewise loop until finding a file name that does not exist
        for (fileNo = 0; fileNo < 1000; fileNo++) {
          if (fileNo)
            name.Format("%s-%d.log", (LPCTSTR)stackName, fileNo);
          else
            name.Format("%s.log", (LPCTSTR)stackName, fileNo);
          if (!CFile::GetStatus((LPCTSTR)name, status))
            break;
        }
        if (fileNo == 1000)
          return 1;
        mSaveFile = name;
      }

      // Set up the window text, and do the save
      dirInd = mSaveFile.GetLength() - (mSaveFile.ReverseFind('\\') + 1);
      name = mSaveFile.Right(dirInd);
      SetWindowText("Log:  " + name);
      return (DoSave());
    }
    return 0;
  }

  if (!mUnsaved)
    return 0;

  // If the appended length matches that last saved and current length, do an append
  // otherwise rewrite from scratch
  UpdateData(true);
  int length = m_strLog.GetLength();
  if (length != mLastSavedLength + mAppendedLength)
    return (DoSave());

  return (OpenAndWriteFile(CFile::modeWrite |CFile::shareDenyWrite, 
    mAppendedLength, mLastSavedLength));
}

// Open the given file and write it
int CLogWindow::OpenAndWriteFile(UINT flags, int length, int offset)
{
  CFile *cFile = NULL;
  int retval = 0;
  mLastFilePath = "";
  try {
    // Open the file for writing 
    cFile = new CFile(mSaveFile, flags);
    cFile->SeekToEnd();

    cFile->Write((void *)((LPCTSTR)m_strLog + offset), length);
    mUnsaved = false;
    mLastSavedLength = length + offset;
    mAppendedLength = 0;
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing log to file " + mSaveFile;
    SEMMessageBox(message, MB_EXCLAME);
    retval = -1;
  } 
  if (cFile) {
    mLastFilePath = cFile->GetFilePath();
    cFile->Close();
    delete cFile;
  }
    
  return retval;

}

// Open an existing file, read it in an place before current log contents
int CLogWindow::ReadAndAppend()
{
  DWORD length;
  CFile *cFile = NULL;
  char *buffer = NULL;
  int retval = 0;
  int nread;
  UpdateData(true);
  CString oldSave = mSaveFile;
  if (GetFileName(TRUE, OFN_HIDEREADONLY)) {
    mSaveFile = oldSave;
    return 1;
  }
  if (mSaveFile == oldSave)
    return 0;
  try {

    // Open the file for reading, get buffer, read into it and terminate string
    cFile = new CFile(mSaveFile, CFile::modeRead);
    length = (int)cFile->GetLength();
    buffer = new char[length + 10];
    nread = cFile->Read(buffer, length + 9);
    buffer[nread] = 0x00;
    m_strLog = CString(buffer) + m_strLog;
    UpdateData(false);
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error reading log from file " + mSaveFile;
    AfxMessageBox(message, MB_EXCLAME);
    retval = -1;
  }
  catch (CMemoryException *perr) {
    perr->Delete();
    AfxMessageBox("Memory error trying to read old log", MB_EXCLAME);
    retval = -1;
  }
  if (cFile) {
    cFile->Close();
    delete cFile;
  }
  if (buffer)
    delete [] buffer;

  // In case of error revert to previous file
  if (retval)
    mSaveFile = oldSave;
  else {
    retval = DoSave();
    mLastStackname = "";
  }
  return retval;
}


int CLogWindow::AskIfSave(CString strReason)
{
  if (!mUnsaved)
    return 0;

  UINT action = MessageBox("Save Log Window before " + strReason, NULL, 
    MB_YESNOCANCEL | MB_ICONQUESTION);
  if (action == IDCANCEL)
    return 1;
  else if (action == IDNO)
    return 0;
  return Save();
}

void CLogWindow::PostNcDestroy() 
{
  delete this;
  CDialog::PostNcDestroy();
}

// This override takes care of the closing situation, OnClose is now redundant
void CLogWindow::OnCancel()
{
  if (AskIfSave("closing?"))
    return;
  // DO NOT call base class, it will leak memory
  //CBaseDlg::OnCancel();
  mWinApp->LogClosing();
  DestroyWindow();
}

void CLogWindow::OnSize(UINT nType, int cx, int cy) 
{
  CBaseDlg::OnSize(nType, cx, cy);
  
  if (!mInitialized)
    return;

  int newX = cx - m_iBorderX;
  int newY = cy - m_iBorderY;
  if (newX < 1)
    newX = 1;
  if (newY < 1)
    newY = 1;
  m_editWindow.SetWindowPos(NULL, 0, 0, newX, newY, SWP_NOZORDER | SWP_NOMOVE);
  mWinApp->RestoreViewFocus();
}
	
void CLogWindow::OnMove(int x, int y) 
{
	CBaseDlg::OnMove(x, y);
  if (mInitialized)
    mWinApp->RestoreViewFocus();
}

void CLogWindow::OnActivate(UINT nState, CWnd * pWndOther, BOOL bMinimized)
{
  CBaseDlg::OnActivate(nState, pWndOther, bMinimized);
  if (nState != WA_INACTIVE)
    mWinApp->SetNavOrLogHadFocus(-1);
}

BOOL CLogWindow::OnInitDialog() 
{
  CBaseDlg::OnInitDialog();
  CMacroEditer::MakeMonoFont(&m_editWindow);
  if (CMacroEditer::mHasMonoFont > 0 && mWinApp->GetMonospacedLog())
    m_editWindow.SetFont(&CMacroEditer::mMonoFont);
  
  // Get initial size of client area and edit box to determine borders for resizing
  CRect wndRect, editRect;
  GetClientRect(wndRect);
  m_editWindow.GetWindowRect(editRect);
  m_iBorderX = wndRect.Width() - editRect.Width();
  m_iBorderY = wndRect.Height() - editRect.Height();
  mInitialized = true;

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CLogWindow::SaveFileNotOnStack(CString stackName)
{
  return (!mSaveFile.IsEmpty() && stackName != mLastStackname);
}
