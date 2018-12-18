// MacroEditer.cpp:      Macro editing window
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
#include ".\MacroEditer.h"
#include "MacroToolbar.h"
#include "MacroProcessor.h"
#include "Utilities\KGetOne.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMacroEditer dialog


CMacroEditer::CMacroEditer(CWnd* pParent /*=NULL*/)
  : CBaseDlg(CMacroEditer::IDD, pParent)
  , m_strCompletions(_T("Tab or  `  to complete command"))
{
  //{{AFX_DATA_INIT(CMacroEditer)
  m_strMacro = _T("");
  //}}AFX_DATA_INIT
  m_strTitle = "";
  mInitialized = false;
  mNonModal = true;
  mLoadUncommitted = false;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mSaveFile = mWinApp->GetMacroSaveFiles();
  mFileName = mWinApp->GetMacroFileNames();
  mMacroName = mWinApp->GetMacroNames();
  mProcessor = mWinApp->mMacroProcessor;
  mEditer = &mWinApp->mMacroEditer[0];
}


void CMacroEditer::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CMacroEditer)
  DDX_Control(pDX, IDC_SAVEMACROAS, m_butSaveAs);
  DDX_Control(pDX, IDC_SAVEMACRO, m_butSave);
  DDX_Control(pDX, IDC_RUNMACRO, m_butRun);
  DDX_Control(pDX, IDC_LOADMACRO, m_butLoad);
  DDX_Control(pDX, IDCANCEL, m_butCancel);
  DDX_Control(pDX, IDOK, m_butOK);
  DDX_Control(pDX, IDC_EDITMACRO, m_editMacro);
  DDX_Text(pDX, IDC_EDITMACRO, m_strMacro);
  DDV_MaxChars(pDX, m_strMacro, 100000);
  //}}AFX_DATA_MAP
  DDX_Control(pDX, IDC_TOPREVMACRO, m_butPrevMacro);
  DDX_Control(pDX, IDC_TONEXTMACRO, m_butNextMacro);
  DDX_Control(pDX, IDC_SHIFT_MACRO_UP, m_butShiftUp);
  DDX_Control(pDX, IDC_SHIFT_MACRO_DOWN, m_butShiftDown);
  DDX_Text(pDX, IDC_STAT_COMPLETIONS, m_strCompletions);
  DDX_Control(pDX, IDC_STAT_COMPLETIONS, m_statCompletions);
  DDX_Control(pDX, IDC_FIND_IN_MACRO, m_butFindInMacro);
}


BEGIN_MESSAGE_MAP(CMacroEditer, CBaseDlg)
  //{{AFX_MSG_MAP(CMacroEditer)
  ON_WM_SIZE()
  ON_BN_CLICKED(IDC_RUNMACRO, OnRunmacro)
  ON_BN_CLICKED(IDC_SAVEMACRO, OnSavemacro)
  ON_BN_CLICKED(IDC_SAVEMACROAS, OnSavemacroas)
  ON_BN_CLICKED(IDC_LOADMACRO, OnLoadmacro)
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_TOPREVMACRO, OnBnClickedToprevmacro)
  ON_BN_CLICKED(IDC_TONEXTMACRO, OnBnClickedTonextmacro)
  ON_EN_CHANGE(IDC_EDITMACRO, OnEnChangeEditmacro)
  ON_BN_CLICKED(IDC_SHIFT_MACRO_UP, &CMacroEditer::OnShiftMacroUp)
  ON_BN_CLICKED(IDC_SHIFT_MACRO_DOWN, &CMacroEditer::OnShiftMacroDown)
  ON_BN_CLICKED(IDC_FIND_IN_MACRO, &CMacroEditer::OnFindInMacro)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMacroEditer message handlers

BOOL CMacroEditer::OnInitDialog() 
{
  WINDOWPLACEMENT *place = mProcessor->GetEditerPlacement() + m_iMacroNumber;
  CBaseDlg::OnInitDialog();
  
  // Get initial size of client area and edit box to determine borders for resizing
  CRect wndRect, editRect, OKRect, clientRect;
  GetClientRect(clientRect);
  m_editMacro.GetWindowRect(editRect);
  m_iBorderX = clientRect.Width() - editRect.Width();
  m_iBorderY = clientRect.Height() - editRect.Height();

  GetWindowRect(wndRect);
  int iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  m_butOK.GetWindowRect(OKRect);
  m_iOKoffset = (OKRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset - editRect.Height();
  m_iOKLeft = OKRect.left - wndRect.left - iXoffset;
  m_butCancel.GetWindowRect(OKRect);
  m_iCancelLeft = OKRect.left - wndRect.left - iXoffset;
  m_butRun.GetWindowRect(OKRect);
  m_iRunLeft = OKRect.left - wndRect.left - iXoffset;
  m_butHelp.GetWindowRect(OKRect);
  m_iHelpLeft = OKRect.left - wndRect.left - iXoffset;
  
  m_butLoad.GetWindowRect(OKRect);
  m_iLoadOffset = (OKRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset - editRect.Height();
  m_iLoadLeft = OKRect.left - wndRect.left - iXoffset;
  m_butSave.GetWindowRect(OKRect);
  m_iSaveLeft = OKRect.left - wndRect.left - iXoffset;

  mInitialized = true;
  mMyMacro = mWinApp->GetMacros() + m_iMacroNumber;
  CEdit *editBox = (CEdit *)GetDlgItem(IDC_EDITMACRO);
  GotoDlgCtrl(editBox);
  editBox->SetSel(0, 0);
  SetTitle();
  mWinApp->UpdateAllEditers();
  mWinApp->RestoreViewFocus();
  mWinApp->UpdateMacroButtons();

  if (place->rcNormalPosition.right > 0)
    SetWindowPlacement(place);

  return FALSE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CMacroEditer::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);
  CRect rect;
  
  if (!mInitialized)
    return;

  int newX = cx - m_iBorderX;
  int newY = cy - m_iBorderY;
  if (newX < 1)
    newX = 1;
  if (newY < 1)
    newY = 1;
  m_editMacro.SetWindowPos(NULL, 0, 0, newX, newY, SWP_NOZORDER | SWP_NOMOVE);
  m_statCompletions.GetWindowRect(rect);
  m_statCompletions.SetWindowPos(NULL, 0, 0, newX, rect.Height(), 
    SWP_NOZORDER | SWP_NOMOVE);
  m_statCompletions.RedrawWindow();

  newY += m_iOKoffset;
  m_butRun.SetWindowPos(NULL, m_iRunLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butCancel.SetWindowPos(NULL, m_iCancelLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butOK.SetWindowPos(NULL, m_iOKLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butHelp.SetWindowPos(NULL, m_iHelpLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  newY += m_iLoadOffset - m_iOKoffset;
  m_butFindInMacro.SetWindowPos(NULL, m_iRunLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butLoad.SetWindowPos(NULL, m_iLoadLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butSave.SetWindowPos(NULL, m_iSaveLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butSaveAs.SetWindowPos(NULL, m_iOKLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

}

void CMacroEditer::PostNcDestroy() 
{
  mEditer[m_iMacroNumber] = NULL;
  mWinApp->UpdateAllEditers();
  delete this;  
  CDialog::PostNcDestroy();
}

// For these two overrides, do not call the base class, and destroy the window
void CMacroEditer::OnOK() 
{
  // Copy the macro back as long as there isn't a macro being done
  // (this button should get disabled in that case)
  if (!mProcessor->DoingMacro()) {
    TransferMacro(true);
    mProcessor->SetNonResumable();
  }
  mEditer[m_iMacroNumber] = NULL;
  mWinApp->UpdateMacroButtons();
  GetWindowPlacement(mProcessor->GetEditerPlacement() + m_iMacroNumber);
  DestroyWindow();
}

void CMacroEditer::OnCancel() 
{
  if (mProcessor->ScanForName(m_iMacroNumber))
    mWinApp->mCameraMacroTools.MacroNameChanged(m_iMacroNumber);
  mEditer[m_iMacroNumber] = NULL;
  if (mLoadUncommitted) {
    mSaveFile[m_iMacroNumber] = "";
    mFileName[m_iMacroNumber] = "";
  }
  mWinApp->UpdateMacroButtons();
  GetWindowPlacement(mProcessor->GetEditerPlacement() + m_iMacroNumber);
  DestroyWindow();
}

// Set pointer to NULL and close window: this is called when reading new settings and 
// the old state has already been saved or abandoned
void CMacroEditer::JustCloseWindow(void)
{
  mEditer[m_iMacroNumber] = NULL;
  DestroyWindow();
}

// Translate tab to our completion character.  It is good on US standard keyboards only
// But there don't seem to be other options
BOOL CMacroEditer::PreTranslateMessage(MSG* pMsg)
{
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    pMsg->wParam = VK_OEM_3;
  return CDialog::PreTranslateMessage(pMsg);
}

// Run the macro
// As long as no tasks are being done, pass text into macro and run it
void CMacroEditer::OnRunmacro() 
{
  mWinApp->RestoreViewFocus();
  if (!mProcessor->MacroRunnable(m_iMacroNumber))
    return;
  TransferMacro(true);
  mProcessor->Run(m_iMacroNumber);
}

void CMacroEditer::OnBnClickedToprevmacro()
{
  SwitchMacro(-1);
}

void CMacroEditer::OnBnClickedTonextmacro()
{
  SwitchMacro(1);
}

void CMacroEditer::OnShiftMacroUp()
{
  ShiftMacro(-1);
}

void CMacroEditer::OnShiftMacroDown()
{
  ShiftMacro(1);
}

void CMacroEditer::OnSavemacro() 
{
  if (mSaveFile[m_iMacroNumber].IsEmpty())
    OnSavemacroas();
  else
    DoSave();
  mWinApp->RestoreViewFocus();
}

void CMacroEditer::OnSavemacroas() 
{
  UpdateData(true);

  // 4/29/14: Switched to function.  It never tried to go to original directory and that
  // will stop working Windows 7, so forget it
  if (mWinApp->mDocWnd->GetTextFileName(false, false, mSaveFile[m_iMacroNumber],
    &mFileName[m_iMacroNumber]))
    return;
  DoSave();
}

// Actually do the save
void CMacroEditer::DoSave()
{
  CFile *cFile = NULL;
  UpdateData(true);
  if (mProcessor->ScanForName(m_iMacroNumber, &m_strMacro))
    mWinApp->mCameraMacroTools.MacroNameChanged(m_iMacroNumber);
  try {
    // Open the file for writing, 
    cFile = new CFile(mSaveFile[m_iMacroNumber], CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);

    cFile->Write((void *)(LPCTSTR)m_strMacro, m_strMacro.GetLength());
    cFile->Close();
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing script to file " + mSaveFile[m_iMacroNumber];
    AfxMessageBox(message, MB_EXCLAME);
    mSaveFile[m_iMacroNumber] = "";
    mFileName[m_iMacroNumber] = "";
  } 
  if (cFile)
    delete cFile;
  SetTitle();
}

void CMacroEditer::OnLoadmacro() 
{
  CFile *file = NULL;
  unsigned char *buffer = NULL;
  int memErr = 0;
  int fileErr = 0;
  CString cPathname, filename;
  if (mWinApp->mDocWnd->GetTextFileName(true, false, cPathname, &filename))
    return;
  
  try {
    file = new CFile(cPathname, CFile::modeRead | CFile::shareDenyWrite);
    int size = (int)file->GetLength();
    buffer = new unsigned char[size + 5];
    if (!buffer) throw 1;
    file->Read((void *)buffer, size);
    buffer[size] = 0x00;

    // Set the macro into edit window but not into master macro, allowing cancel to occur
    // Thus scan this string for name, not the master macro
    m_strMacro = buffer;
    UpdateData(false);
    mSaveFile[m_iMacroNumber] = cPathname;
    mFileName[m_iMacroNumber] = filename;
    mLoadUncommitted = true;
    if (mProcessor->ScanForName(m_iMacroNumber, &m_strMacro))
      mWinApp->mCameraMacroTools.MacroNameChanged(m_iMacroNumber);
    SetTitle();
  }
  catch (CFileException *err) {
    AfxMessageBox("Error: Cannot read from the selected file.", MB_EXCLAME);
    err->Delete();
  }
  catch (CMemoryException *err) {
    memErr = 1;
    err->Delete();
  }
  catch (int) {
    memErr = 1;
  }
  if (memErr)
    AfxMessageBox("Error getting memory for strings", MB_EXCLAME);
      
  if (buffer)
    delete buffer;

  if (file)
    delete file;
}

// Find a string in the macro
void CMacroEditer::OnFindInMacro()
{
  int sel1, sel2, numLines;
  CRect editRect;
  CSize size;
  CString macro, find;
  if (!KGetOneString("Text is not case-sensitive", "Text to find in script:", 
    mLastFindString) || mLastFindString.IsEmpty())
    return;
  m_editMacro.GetSel(sel1, sel2);
  sel2 = B3DMAX(0, sel2);

  // Make upper case copies for find
  find = mLastFindString;
  find.MakeUpper();
  macro = m_strMacro;
  macro.MakeUpper();

  // Look fro here t end then from the start
  sel1 = macro.Find(find, sel2);
  if (sel1 < 0)
    sel1 = macro.Find(find, 0);
  if (sel1 < 0) {
    AfxMessageBox("Text not found", MB_EXCLAME);
    return;
  }
  sel2 = sel1 + mLastFindString.GetLength();
  m_editMacro.SetSel(sel1, sel2);

  // Make sure the line is on the screen
  sel2 = m_editMacro.LineFromChar(sel1);
  sel1 = m_editMacro.GetFirstVisibleLine();
  CDC* pDC = m_editMacro.GetDC();
  size = pDC->GetTextExtent(mLastFindString);
  m_editMacro.ReleaseDC(pDC);
  m_editMacro.GetRect(&editRect);
  numLines = editRect.Height() / size.cy;
  if (sel2 < sel1 + 1 || sel2 > sel1 + numLines - 2)
    m_editMacro.LineScroll(sel2 - (sel1 + 1));
  m_editMacro.SetFocus();
}

// OK button should be disabled if any macro is running, and Run disabled if
// any tasks are being done
void CMacroEditer::UpdateButtons()
{
  BOOL inactive = !mProcessor->DoingMacro();
  m_butOK.EnableWindow(inactive);
  m_butRun.EnableWindow(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries());
  m_editMacro.EnableWindow(inactive);
  m_butLoad.EnableWindow(inactive);
  m_butPrevMacro.EnableWindow(inactive && AdjacentMacro(-1) >= 0);
  m_butNextMacro.EnableWindow(inactive && AdjacentMacro(1) >= 0);
  m_butShiftDown.EnableWindow(inactive && m_iMacroNumber < MAX_MACROS - 1);
  m_butShiftUp.EnableWindow(inactive && m_iMacroNumber > 0);
}

// Get the text from the window into the local variable then
// transfer that to the main programs macro
void CMacroEditer::TransferMacro(BOOL fromEditor)
{
  if (fromEditor) {
    UpdateData(true);
    *mMyMacro = m_strMacro;
    mLoadUncommitted = false;
  } else {
    m_strMacro = *mMyMacro;
    UpdateData(false);
  }
  if (mProcessor->ScanForName(m_iMacroNumber)) {
    SetTitle();
    mWinApp->mCameraMacroTools.MacroNameChanged(m_iMacroNumber);
  }
}

BOOL CMacroEditer::OnToolTipNotify( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXT *pTTT = (TOOLTIPTEXT *)pNMHDR;
    int nID;
    if (pTTT->uFlags & TTF_IDISHWND)
    {
        // idFrom is actually the HWND of the tool
        nID = ::GetDlgCtrlID((HWND)pNMHDR->idFrom);
        if(nID)
        {
            pTTT->lpszText = MAKEINTRESOURCE(nID);
            pTTT->hinst = AfxGetResourceHandle();
            return(TRUE);
        }
    }
    return(FALSE);
}

// Set up title as the macro number plus a filename
void CMacroEditer::SetTitle(void)
{
  m_strTitle.Format("Script %d", m_iMacroNumber + 1);
  if (!mMacroName[m_iMacroNumber].IsEmpty())
    m_strTitle += ": " + mMacroName[m_iMacroNumber];
  if (!mFileName[m_iMacroNumber].IsEmpty())
    m_strTitle += " - " + mFileName[m_iMacroNumber];
  SetWindowText(m_strTitle);
}

// Find an adjacent unedited macro in the given direction (+1/-1)
int CMacroEditer::AdjacentMacro(int dir)
{
  for (int i = m_iMacroNumber + dir; i >= 0 && i < MAX_MACROS; i += dir)
    if (!mEditer[i])
      return i;
  return -1;
}

// Switch the editor to a macro in the given direction
void CMacroEditer::SwitchMacro(int dir)
{
  int newNum = AdjacentMacro(dir);
  mWinApp->RestoreViewFocus();
  if (newNum < 0)
    return;
  TransferMacro(true);
  mEditer[newNum] = mEditer[m_iMacroNumber];
  mEditer[m_iMacroNumber] = NULL;
  AdjustForNewNumber(newNum);
  mWinApp->UpdateAllEditers();
  mWinApp->UpdateMacroButtons();
}

// Actually shift a macro in the given direction and switch editor(s) too
void CMacroEditer::ShiftMacro(int dir)
{
  CString *macros = mWinApp->GetMacros();
  int newNum = m_iMacroNumber + dir;
  mWinApp->RestoreViewFocus();
  if (newNum < 0 || newNum >= MAX_MACROS)
    return;

  // Transfer both macros
  TransferMacro(true);
  if (mEditer[newNum])
    mEditer[newNum]->TransferMacro(true);

  // Swap the strings in the macro array and the macro editor pointers
  CString tempStr = macros[m_iMacroNumber];
  macros[m_iMacroNumber] = macros[newNum];
  macros[newNum] = tempStr;
  CMacroEditer *tempEd = mEditer[m_iMacroNumber];
  mEditer[m_iMacroNumber] = mEditer[newNum];
  mEditer[newNum] = tempEd;
  tempStr = mFileName[m_iMacroNumber];
  mFileName[m_iMacroNumber] = mFileName[newNum];
  mFileName[newNum] = tempStr;
  tempStr = mSaveFile[m_iMacroNumber];
  mSaveFile[m_iMacroNumber] = mSaveFile[newNum];
  mSaveFile[newNum] = tempStr;
 
  // Fix up each editor: m_iMacroNumber now refers to the other macro
  if (mEditer[m_iMacroNumber])
    mEditer[m_iMacroNumber]->AdjustForNewNumber(m_iMacroNumber);
  else {
    mProcessor->ScanForName(m_iMacroNumber);
    mWinApp->mCameraMacroTools.MacroNameChanged(m_iMacroNumber);
  }
  AdjustForNewNumber(newNum);
  mWinApp->UpdateAllEditers();
  mWinApp->UpdateMacroButtons();
}

void CMacroEditer::AdjustForNewNumber(int newNum)
{
  m_iMacroNumber = newNum;
  mMyMacro = mWinApp->GetMacros() + m_iMacroNumber;
  TransferMacro(false);
  SetTitle();
}

// The text has changed: call static function and update
void CMacroEditer::OnEnChangeEditmacro()
{
  int sel1, sel2, firstVis;
  bool setCompletions, completing;
  UpdateData(true);
  m_editMacro.GetSel(sel1, sel2);
  firstVis = m_editMacro.GetFirstVisibleLine();
  HandleCompletionsAndIndent(m_strMacro, m_strCompletions, sel2, setCompletions, 
    completing);
  if (setCompletions)
    SetDlgItemText(IDC_STAT_COMPLETIONS, m_strCompletions);
  if (completing) {
    UpdateData(false);
    m_editMacro.SetSel(sel2, sel2);
    sel2 = m_editMacro.GetFirstVisibleLine();
    if (sel2 != firstVis)
      m_editMacro.LineScroll(firstVis - sel2);
  }
}

// Process a change in an edit control and do 'backtick' completion or list completions,
// and indentation of the current line
void CMacroEditer::HandleCompletionsAndIndent(CString &strMacro, CString &strCompletions,
  int &sel2, bool &setCompletions, bool &completing)
{

  int sel1, delStart, numDel, i, numCommands, lineStart, numMatch, saveStart;
  int indentSize, prevStart, needIndent, curIndent;
  int maxCompletions = 10;
  unsigned int lenMatch;
  char ch;
  short *matchList;
  bool matched, atWordEnd, foundText, hasSpace = false;
  CmdItem *cmdList;
  CString substr;
  const char *first, *other;
  const char *prevKeys[] = {"LOOP", "IF", "ELSE", "ELSEIF", "FUNCTION"};
  const char *curKeys[] = {"ENDLOOP", "ELSE", "ELSEIF", "ENDIF", "ENDFUNCTION"};
  int numPrevKeys = sizeof(prevKeys) / sizeof(const char *);
  int numCurKeys = sizeof(curKeys) / sizeof(const char *);
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();

  completing = false;
  setCompletions = false;
  if (sel2 <= 0)
    return;
  completing = strMacro.GetAt(sel2-1) == '`';

  // Set up to delete this character
  numDel = 1;
  delStart = sel2 - (completing ? 1 : 0);
  cmdList = winApp->mMacroProcessor->GetCommandList(numCommands);
  matchList = new short[numCommands];

  // Make sure we are at the end of a word, also find out if there is a following space
  atWordEnd = sel2 >= strMacro.GetLength();
  if (!atWordEnd) {
    ch = strMacro.GetAt(sel2);
    hasSpace = ch == ' ' || ch == '\t';
    atWordEnd = hasSpace || ch == '\n' || ch == '\r';
  }

  // Find the beginning of the line or a previous space/tab
  if (atWordEnd) {
    saveStart = -1;
    for (lineStart = sel2 - 1; lineStart > 0; lineStart--) {
      ch = strMacro.GetAt(lineStart - 1);
      if (ch == '\n' || ch == '\r')
        break;

      // Save the position of the first character after white space
      if (ch == ' ' || ch == '\t') {
        if (saveStart < 0)
          saveStart = lineStart;

        // If a position has been saved and a non white space is found, then it is
        // not a command
      } else if (saveStart >= 0) {
        lineStart = -1;
        break;
      }
    }

    // If position was saved, set it as start
    if (lineStart >= 0 && saveStart >= 0)
      lineStart = saveStart;
  } else
    lineStart = -1;

  // Wipe out the completion line unless it hasn't been changed from original
  numMatch = 0;
  if (strCompletions.Left(6).Compare("Tab or"))
    strCompletions = "";
  if (lineStart >= 0 && strMacro.GetAt(lineStart) == '#')
    lineStart = -1;
  while (lineStart >=0 && lineStart < delStart) {

    // Extract the string before this point, make upper case, and make a list of matches
    lenMatch = delStart - lineStart;
    if (lenMatch <= 0)
      break;
    substr = strMacro.Mid(lineStart, lenMatch);
    substr.MakeUpper();
    if (substr == "\r" || substr == "\n")
      break;
    for (i = 0; i < numCommands; i++)
      if (cmdList[i].cmd.find(substr) == 0)
        matchList[numMatch++] = i;

    // If there are no matches, try one less character
    if (!numMatch) {
      delStart--;
      numDel++;
      continue;
    }

    // That's all we need to list completions
    if (!completing)
      break;

    // If there are multiple matches, find the point where they diverge
    if (numMatch > 1) {
      matched = true;
      first = cmdList[matchList[0]].cmd.c_str();
      while (matched && strlen(first) > lenMatch) {
        ch = first[lenMatch];

        // Check each one against the first and see if other is too short or different
        for (i = 1; i < numMatch; i++) {
          other = cmdList[matchList[i]].cmd.c_str();
          if (strlen(other) <= lenMatch || other[lenMatch] != ch) {
            matched = false;
            break;
          }
        }

        // If they still match, extend the length of the match and loop again
        if (matched)
          lenMatch++;
      }

      // Once they don't match, extract this component from the first string
      substr = CString(cmdList[matchList[0]].mixedCase).Left(lenMatch);
    } else {

      // For a single match, use the entire string plus a space if there is not one
      substr = CString(cmdList[matchList[0]].mixedCase);
      lenMatch = substr.GetLength() + 1;
      if (!hasSpace)
        substr += " ";
    }

    // So now adjust the deletion to take out the whole command for replacement
    numDel += delStart - lineStart;
    delStart = lineStart;
    break;
  }

  // List completions if not completing, or if the command is still not complete
  if ((!completing && numMatch) || numMatch > 1) {
    strCompletions = "";
    for (i = 0; i < B3DMIN(maxCompletions, numMatch); i++)
      strCompletions += CString(cmdList[matchList[i]].mixedCase) + "  ";
    if (numMatch > maxCompletions)
      strCompletions += " ...";
    setCompletions = true;
  }

  if (completing) {

    // Delete the indicated amount and adjust caret
    strMacro.Delete(delStart, numDel);
    sel2 -= numDel;

    // Then insert a matching string if any and adjust caret
    if (numMatch) {
      strMacro.Insert(delStart, substr);
      sel2 += lenMatch;
    }

    // Indentation: 
    indentSize = winApp->mMacroProcessor->GetAutoIndentSize();
    if (indentSize > 0) {

      // Find start of current line
      for (lineStart = sel2; lineStart > 0; lineStart--) {
        ch = strMacro.GetAt(lineStart - 1);
        if (ch == '\n' || ch == '\r')
          break;
      }
      
      // Find start of previous line with text
      foundText = false;
      for (prevStart = lineStart; prevStart > 0; prevStart--) {
        ch = strMacro.GetAt(prevStart - 1);
        if (!foundText && !(ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t'))
          foundText = true;
        if (foundText && (ch == '\n' || ch == '\r'))
          break;
      }

      // Get the previous line indent and add if it matches a keyword
      needIndent = 0;
      if (foundText && FindIndentAndMatch(strMacro, prevStart, lineStart, prevKeys, 
        numPrevKeys, needIndent))
        needIndent += indentSize;

      // Get the current line indent and subtract from needed indent if keyword matches
      sel1 = strMacro.Find('\r', sel2);
      i = strMacro.Find('\n', sel2);
      if (sel1 < 0 && i < 0)
        sel1 = strMacro.GetLength();
      else
        sel1 = B3DMAX(sel1, i);
      if (FindIndentAndMatch(strMacro, lineStart, sel1, curKeys, numCurKeys, curIndent))
        needIndent = B3DMAX(0, needIndent - indentSize);

      // Adjust by deletion or addition
      numDel = curIndent - needIndent;
      if (numDel > 0) {
        strMacro.Delete(lineStart, numDel);
      } else {
        for (i = 0; i < -numDel; i++)
          strMacro.Insert(lineStart, " ");
      }
      sel2 -=numDel;
    }

  }
  delete [] matchList;
}

// Finds the number of current spaces indenting the line at lineStart and looks for a
// match of the first word with one of the keywords
bool CMacroEditer::FindIndentAndMatch(CString &strMacro, int lineStart, int limit, 
  const char **keywords, int numKeys, int &curIndent)
{
  char ch;
  CString token;
  int indStart, indEnd;

  // Count spaces at start of line and get to start of whatever else
  curIndent = 0;
  for (indStart = lineStart; indStart < limit; indStart++) {
    if (strMacro.GetAt(indStart) == ' ')
      curIndent++;
    else
      break;
  }

  // Find end of token if any
  for (indEnd = indStart; indEnd < limit; indEnd++) {
    ch = strMacro.GetAt(indEnd);
    if (ch == ' ' || ch == '\r' || ch == '\n')
      break;
  }
  if (indEnd == indStart)
    return false;

  // Extract and test for match
  token = strMacro.Mid(indStart, indEnd - indStart);
  token.MakeUpper();
  for (indEnd = 0; indEnd < numKeys; indEnd++) 
    if (!token.Compare(keywords[indEnd]))
      return true;
  return false;
}
