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
#include "EMscope.h"
#include ".\MacroEditer.h"
#include "MacroToolbar.h"
#include "MacroProcessor.h"
#include "ParameterIO.h"
#include "Utilities\KGetOne.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CFont CMacroEditer::mMonoFont;
CFont CMacroEditer::mDefaultFont;
int CMacroEditer::mHasMonoFont = -1;

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
  DDX_Control(pDX, IDC_BUT_TO_MACRO_LINE, m_butToMacroLine);
  DDX_Control(pDX, IDC_STAT_INDENT, m_statIndent);
  DDX_Control(pDX, IDC_BUT_FIX_INDENT, m_butFixIndent);
  DDX_Control(pDX, IDC_BUT_ADD_INDENT, m_butAddIndent);
  DDX_Control(pDX, IDC_BUT_REMOVE_INDENT, m_butRemoveIndent);
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
  ON_BN_CLICKED(IDC_SHIFT_MACRO_UP, OnShiftMacroUp)
  ON_BN_CLICKED(IDC_SHIFT_MACRO_DOWN, OnShiftMacroDown)
  ON_BN_CLICKED(IDC_FIND_IN_MACRO, OnFindInMacro)
  ON_BN_CLICKED(IDC_BUT_TO_MACRO_LINE, OnButToMacroLine)
  ON_BN_CLICKED(IDC_BUT_FIX_INDENT, OnButFixIndent)
  ON_BN_CLICKED(IDC_BUT_ADD_INDENT, OnButAddIndent)
  ON_BN_CLICKED(IDC_BUT_REMOVE_INDENT, OnButRemoveIndent)
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
  
  MakeMonoFont(&m_editMacro);
  if (mHasMonoFont > 0 && mProcessor->GetUseMonoFont())
    m_editMacro.SetFont(&mMonoFont);

  m_editMacro.GetWindowRect(editRect);
  m_iBorderX = clientRect.Width() - editRect.Width();
  m_iBorderY = clientRect.Height() - editRect.Height();

  GetWindowRect(wndRect);
  int iXoffset = (wndRect.Width() - clientRect.Width()) / 2;
  int iyAdd = (iXoffset - wndRect.top) - (wndRect.Height() - clientRect.Height()) -
    editRect.Height();
  m_iEditLeft = editRect.left - wndRect.left - iXoffset;
  m_iEditOffset = (editRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset;

  m_butOK.GetWindowRect(OKRect);
  m_iOKoffset = OKRect.top + iyAdd;
  m_iOKLeft = OKRect.left - wndRect.left - iXoffset;
  m_butCancel.GetWindowRect(OKRect);
  m_iCancelLeft = OKRect.left - wndRect.left - iXoffset;
  m_butRun.GetWindowRect(OKRect);
  m_iRunLeft = OKRect.left - wndRect.left - iXoffset;
  m_butHelp.GetWindowRect(OKRect);
  m_iHelpLeft = OKRect.left - wndRect.left - iXoffset;
  
  m_butLoad.GetWindowRect(OKRect);
  m_iLoadLeft = OKRect.left - wndRect.left - iXoffset;
  m_butSave.GetWindowRect(OKRect);
  m_iSaveOffset = OKRect.top + iyAdd;
  m_iSaveLeft = OKRect.left - wndRect.left - iXoffset;
  m_butToMacroLine.GetWindowRect(OKRect);
  m_iToLineLeft = OKRect.left - wndRect.left - iXoffset;
  m_butSaveAs.GetWindowRect(OKRect);
  m_iSaveAsLeft = OKRect.left - wndRect.left - iXoffset;

  m_statCompletions.GetWindowRect(OKRect);
  m_iCompOffset = (OKRect.top - wndRect.top) - (wndRect.Height() - clientRect.Height())
    + iXoffset;
  m_iCompLeft = OKRect.left - wndRect.left - iXoffset;
 
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

// Make the monospaced and default font for an editor or log window, whoever calls first
void CMacroEditer::MakeMonoFont(CWnd *edit)
{
  CFont *font;
  LOGFONT logFont;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  CString propFont = winApp->mMacroProcessor->GetMonoFontName();
  const char *tryNames[] = {"Lucida Console", "Consolas", "Lucida Sans Typewriter",
    "Courier New"};
  int ind, height, lastFont = sizeof(tryNames) / sizeof(const char *) - 1;

  if (mHasMonoFont < 0) {
    font = edit->GetFont();
    font->GetLogFont(&logFont);
    height = logFont.lfHeight;
    mHasMonoFont = 0;
    if (mDefaultFont.CreateFontIndirect(&logFont)) {
      for (ind = -1; ind <= lastFont; ind++) {
        if (ind < 0 && propFont.IsEmpty())
          continue;
        if (mMonoFont.CreateFont(logFont.lfHeight, 0, 0, 0,
          ind < lastFont ? logFont.lfWeight : FW_SEMIBOLD,
          0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | (ind < lastFont ? 0 : FF_DONTCARE),
          ind < 0 ? (LPCTSTR)propFont : tryNames[ind])) {
          mMonoFont.GetLogFont(&logFont);

          // This doesn't work!  It will tell you it got what you asked for and end up
          // with Courier
          if (ind < lastFont) {
            if (strstr(logFont.lfFaceName, "Courier")) {
              SEMTrace('1', "Got %s asking for %s", logFont.lfFaceName, tryNames[ind]);
              continue;
            }
          }
          mHasMonoFont = 1;
          SEMTrace('1', "Got %s asking for %s", logFont.lfFaceName,
            ind < 0 ? (LPCTSTR)propFont : tryNames[ind]);
          break;
        }
      }
    }
  }
}

void CMacroEditer::OnSize(UINT nType, int cx, int cy) 
{
  CDialog::OnSize(nType, cx, cy);
  CRect rect;
  bool dropIndents = !mProcessor->GetShowIndentButtons();
  //int showFlag = dropIndents ? SWP_HIDEWINDOW : SWP_SHOWWINDOW;
  int showFlag = dropIndents ? SW_HIDE : SW_SHOW;
  int dropAdjust = dropIndents ? m_iSaveOffset - m_iOKoffset : 0;
  
  if (!mInitialized)
    return;

  int newX = cx - m_iBorderX;
  int newY = cy - m_iBorderY;
  if (dropIndents)
    newY += dropAdjust;
  if (newX < 1)
    newX = 1;
  if (newY < 1)
    newY = 1;
  // An offset is not a top....
  m_editMacro.SetWindowPos(NULL, m_iEditLeft, m_iEditOffset - dropAdjust, newX, newY, 
    SWP_NOZORDER);
  m_statCompletions.GetWindowRect(rect);
  m_statCompletions.SetWindowPos(NULL, m_iCompLeft, m_iCompOffset - dropAdjust, newX, 
    rect.Height(), SWP_NOZORDER);
  m_statCompletions.RedrawWindow();

  newY += m_iOKoffset - dropAdjust;
  m_butRun.SetWindowPos(NULL, m_iRunLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butCancel.SetWindowPos(NULL, m_iCancelLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butOK.SetWindowPos(NULL, m_iOKLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butLoad.SetWindowPos(NULL, m_iLoadLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butHelp.SetWindowPos(NULL, m_iHelpLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  newY += m_iSaveOffset - m_iOKoffset;
  m_butFindInMacro.SetWindowPos(NULL, m_iRunLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butToMacroLine.SetWindowPos(NULL, m_iToLineLeft, newY, 0, 0, 
    SWP_NOZORDER | SWP_NOSIZE);
  m_butSave.SetWindowPos(NULL, m_iSaveLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_butSaveAs.SetWindowPos(NULL, m_iSaveAsLeft, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
  m_statIndent.ShowWindow(showFlag);
  m_butFixIndent.ShowWindow(showFlag);
  m_butAddIndent.ShowWindow(showFlag);
  m_butRemoveIndent.ShowWindow(showFlag);
}


void CMacroEditer::PostNcDestroy() 
{
  mEditer[m_iMacroNumber] = NULL;
  if (!mWinApp->GetAppExiting())
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
  static bool ctrlPressed = false;
  static double ctrlTime = 0.;
  float expireTime = 3000.f;
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    pMsg->wParam = VK_OEM_3;
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_CONTROL) {
    ctrlPressed = true;
    ctrlTime = GetTickCount();
  }
  if (pMsg->message == WM_KEYUP && pMsg->wParam == VK_CONTROL)
    ctrlPressed = false;
  if (pMsg->message == WM_KEYUP && pMsg->wParam == 'A' && ctrlPressed && 
    SEMTickInterval(ctrlTime) < expireTime)
    m_editMacro.SetSel(0xFFFF0000);
  if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && ctrlPressed &&
    SEMTickInterval(ctrlTime) < expireTime && m_butRun.IsWindowEnabled()) {
    OnRunmacro();
    pMsg->wParam = 0x00;
  }
  return CDialog::PreTranslateMessage(pMsg);
}

// Run the macro
// As long as no tasks are being done, pass text into macro and run it
void CMacroEditer::OnRunmacro() 
{
  mWinApp->RestoreViewFocus();
  if (!mProcessor->MacroRunnable(m_iMacroNumber)) {
    FixButtonFocus(m_butRun);
    m_editMacro.SetFocus();
    return;
  }
  TransferMacro(true);
  mProcessor->Run(m_iMacroNumber);
  FixButtonFocus(m_butRun);
  m_editMacro.SetFocus();
  mWinApp->RestoreViewFocus();
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
  static char szFilter[] = "Script files (*.txt, *.py)|*.txt; *.py|All files (*.*)|*.*||";
  CString cPathname, filename;
  if (mWinApp->mDocWnd->GetTextFileName(true, false, cPathname, &filename, NULL,
    &szFilter[0]))
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
  int sel1, sel2;
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
  
  EnsureLineVisible(m_editMacro.LineFromChar(sel1));
}

// Make sure the given line is on the screen
void CMacroEditer::EnsureLineVisible(int sel2)
{
  CRect editRect;
  CSize size;
  int  numLines, sel1 = m_editMacro.GetFirstVisibleLine();
  CDC* pDC = m_editMacro.GetDC();
  size = pDC->GetTextExtent("serialem");
  m_editMacro.ReleaseDC(pDC);
  m_editMacro.GetRect(&editRect);
  numLines = editRect.Height() / size.cy;
  if (sel2 < sel1 + 1 || sel2 > sel1 + numLines - 2)
    m_editMacro.LineScroll(sel2 - (sel1 + 1));
  m_editMacro.SetFocus();
}

// Go to a line in the macro
void CMacroEditer::OnButToMacroLine()
{
  int line;
  line = B3DMAX(1, mProcessor->GetLastPythonErrorLine());
  if (!KGetOneInt("Script line number to go to:", line))
    return;
  if (line < 1 || line > m_editMacro.GetLineCount()) {
    AfxMessageBox("Line number out of range");
    return;
  }
  SelectAndShowLine(line);
}

// External function for going to a line
void CMacroEditer::SelectAndShowLine(int lineFromOne)
{
  int charInd = m_editMacro.LineIndex(lineFromOne - 1);
  m_editMacro.SetSel(charInd, charInd + 3);
  EnsureLineVisible(lineFromOne - 1);
}

// Fix the indentation
void CMacroEditer::OnButFixIndent()
{
  int startInd, endInd, ind, size, currentInd, numSpace, fix, line;
  int numOneSpace = 0, numBig = 0, numAmbig = 0;
  bool even, fixAll = true;
  CString strLine, bit;
  ShortVec fixIndent;
  if (GetLineSelectLimits(startInd, endInd, size))
    return;

  // Go through the lines computing how much to fix by and counting types of fixes
  even = (size % 2) == 0;
  currentInd = startInd;
  while (currentInd < endInd) {
    numSpace = 0;
    for (ind = currentInd; ind < endInd; ind++) {
      if (m_strMacro.GetAt(ind) == ' ')
        numSpace++;
      else
        break;
    }
    fix = numSpace % size;
    if (fix == size / 2 && even) {
      numAmbig++;
      fixIndent.push_back(0);
    } else {
      if (fix > size / 2)
        fix = size - fix;
      else
        fix = -fix;
      if (B3DABS(fix) == 1)
        numOneSpace++;
      else if (fix)
        numBig++;
      fixIndent.push_back(fix);
    }
    mProcessor->GetNextLine(&m_strMacro, currentInd, strLine, true);
  }

  // Find out what they want to do if there are big ones or ambiguous ones
  if (numAmbig || numBig) {
    strLine = "";
    if (numOneSpace)
      strLine.Format("%d of %d lines are off by one space", numOneSpace,
        fixIndent.size());
    if (numBig) {
      bit.Format("%d lines are off by more than one space but fixable", numBig);
      if (!strLine.IsEmpty())
        strLine += '\n';
      strLine += bit;
    }
    if (numAmbig) {
      bit.Format("%d lines are off by half the indent size and unfixable", numAmbig);
      if (strLine.IsEmpty()) {
        AfxMessageBox(bit, MB_EXCLAME);
        return;
      }
      strLine += '\n' + bit;
    }
    if (numOneSpace && numBig) {
      strLine += "\n\nPress:\n\n\"One Space Only\" to fix just the ones off by one space"
        "\n\n\"Multiple Spaces\" to fix ones off by multiple spaces also";
      if (numAmbig)
        strLine += "\n\n\"Cancel\" to fix nothing and examine the situation.";
      ind = SEMThreeChoiceBox(strLine, "One Space Only", "Multiple Spaces",
        numAmbig ? "Cancel" : "", numAmbig ? MB_YESNOCANCEL : MB_YESNO);
      if (ind == IDCANCEL)
        return;
      fixAll = ind == IDNO;
    } else {
      if (AfxMessageBox(strLine + "\n\nDo you want to fix whatever can be fixed?",
        MB_QUESTION) == IDNO)
        return;
    }
  }

  // Proceed with the fixes
  line = 0;
  currentInd = startInd;
  while (currentInd < endInd) {
    fix = fixIndent[line];
    if (fixAll || B3DABS(fix) == 1) {
      if (fix < 0) {
        m_strMacro.Delete(currentInd, -fix);
      } else if (fix > 0) {
        for (ind = 0; ind < fix; ind++)
          m_strMacro.Insert(currentInd, ' ');
      }
      endInd += fix;
    }
    mProcessor->GetNextLine(&m_strMacro, currentInd, strLine, true);
    line++;
  }

  UpdateData(false);
  m_editMacro.SetSel(startInd, endInd);
  mProcessor->ScanForName(m_iMacroNumber);
}

// Add one unit of indentation
void CMacroEditer::OnButAddIndent()
{
  int startInd, endInd, ind, size, currentInd;
  CString indentStr, strLine;
  if (GetLineSelectLimits(startInd, endInd, size))
    return;

  // Make indent string and add to each line
  for (ind = 0; ind < size; ind++)
    indentStr += " ";
  currentInd = startInd;
  while (currentInd < endInd) {
    m_strMacro.Insert(currentInd, indentStr);
    endInd += size;
    mProcessor->GetNextLine(&m_strMacro, currentInd, strLine, true);
  }
  UpdateData(false);
  m_editMacro.SetSel(startInd, endInd);
  mProcessor->ScanForName(m_iMacroNumber);
}

// Remove one unit of indentation
void CMacroEditer::OnButRemoveIndent()
{
  int startInd, endInd, ind, size, currentInd;
  CString indentStr, strLine;
  if (GetLineSelectLimits(startInd, endInd, size))
    return;

  // First make sure each line has that much indentation
  for (ind = 0; ind < size; ind++)
    indentStr += " ";
  currentInd = startInd;
  while (currentInd < endInd) {
    ind = m_strMacro.Find(indentStr, currentInd);
    if (ind != currentInd) {
      AfxMessageBox("Not all lines in the selected region are indented by the indentation"
        " size");
      return;
    }
    mProcessor->GetNextLine(&m_strMacro, currentInd, strLine, true);
  }

  // Then remove characters
  currentInd = startInd;
  while (currentInd < endInd) {
    m_strMacro.Delete(currentInd, size);
    endInd -= size;
    mProcessor->GetNextLine(&m_strMacro, currentInd, strLine, true);
  }
  UpdateData(false);
  m_editMacro.SetSel(startInd, endInd);
  mProcessor->ScanForName(m_iMacroNumber);
}

// Get string indexes at the start of lines at the beginning and end of selection,
// return 1 if no selection or indent size not set
int CMacroEditer::GetLineSelectLimits(int &startInd, int &endInd, int &indentSize)
{
  int sel1, sel2, ind, jnd;
  UpdateData(true);
  m_editMacro.GetSel(sel1, sel2);
  indentSize = mWinApp->mMacroProcessor->GetAutoIndentSize();
  if (sel1 < 0 || sel2 < 0 || !indentSize)
    return 1;
  if (sel2 && (m_strMacro.GetAt(sel2 - 1) == '\r' || m_strMacro.GetAt(sel2 - 1) == '\n'))
    endInd = sel2;
  else {
    ind = m_strMacro.Find("\r", sel2);
    jnd = m_strMacro.Find("\n", sel2);
    if (ind < 0 && jnd < 0)
      endInd = m_strMacro.GetLength();
    else
      endInd = B3DMAX(ind, jnd) + 1;
  }

  for (ind = sel1; ind > 0; ind--)
    if (m_strMacro.GetAt(ind - 1) == '\r' || m_strMacro.GetAt(ind - 1) == '\n')
      break;
  startInd = ind;
  return 0;
}

// OK button should be disabled if any macro is running, and Run disabled if
// any tasks are being done
void CMacroEditer::UpdateButtons()
{
  BOOL inactive = !mProcessor->DoingMacro();
  m_butOK.EnableWindow(inactive);
  m_butRun.EnableWindow(!mWinApp->DoingTasks() && !mWinApp->StartedTiltSeries() && 
    !mWinApp->mScope->GetMovingStage());
  m_editMacro.EnableWindow(inactive);
  m_butLoad.EnableWindow(inactive);
  m_butPrevMacro.EnableWindow(inactive && AdjacentMacro(-1) >= 0);
  m_butNextMacro.EnableWindow(inactive && AdjacentMacro(1) >= 0);
  inactive = inactive && (!mWinApp->GetShiftScriptOnlyInAdmin() || 
    mWinApp->GetAdministrator());
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
  int sel1, sel2, firstVis, initialLength;
  bool setCompletions, completing, rejecting = false;
  int readOnlyStart = mProcessor->GetReadOnlyStart(m_iMacroNumber);
  m_editMacro.GetSel(sel1, sel2);
  if (readOnlyStart >= 0 && !mWinApp->GetAdministrator()) {
    if (sel2 >= readOnlyStart) {
      rejecting = true;
      sel2 = readOnlyStart;
    } else
      initialLength = m_strMacro.GetLength();
  }
  firstVis = m_editMacro.GetFirstVisibleLine();
  if (!rejecting) {
    UpdateData(true);
    HandleCompletionsAndIndent(m_strMacro, m_strCompletions, sel2, setCompletions,
      completing, false);
    if (setCompletions)
      SetDlgItemText(IDC_STAT_COMPLETIONS, m_strCompletions);
  }
  if (completing || rejecting) {
    UpdateData(false);
    m_editMacro.SetSel(sel2, sel2);
    sel2 = m_editMacro.GetFirstVisibleLine();
    if (sel2 != firstVis)
      m_editMacro.LineScroll(firstVis - sel2);
  }
  if (readOnlyStart >= 0 && !rejecting)
    mProcessor->SetReadOnlyStart(m_iMacroNumber, readOnlyStart + m_strMacro.GetLength() -
      initialLength);
}

// Process a change in an edit control and do 'backtick' completion or list completions,
// and indentation of the current line
void CMacroEditer::HandleCompletionsAndIndent(CString &strMacro, CString &strCompletions,
  int &sel2, bool &setCompletions, bool &completing, bool oneLine)
{

  int sel1, delStart, numDel, i, numCommands, lineStart, numMatch, saveStart;
  int indentSize, prevStart, needIndent, curIndent;
  int maxCompletions = 10;
  unsigned int lenMatch;
  char ch;
  short *matchList;
  bool matched, atWordEnd, foundText, isPython, hasSpace = false, needsNamespace = false;
  bool isBlank, afterBlank, isContinued, afterColon, isKeyword  = false;
  bool removeIndent = false;
  int prevEnd, backStart, backEnd, afterContinue;
  CmdItem *cmdList;
  CString substr, importName, nameSpace;
  const char *first, *other;
  const char *prevKeys[] = {"LOOP", "IF", "ELSE", "ELSEIF", "FUNCTION", "TRY", "CATCH"};
  const char *curKeys[] = {"ENDLOOP", "ELSE", "ELSEIF", "ENDIF", "ENDFUNCTION", "CATCH",
    "ENDTRY"};
  const char *curPythKeys[] = {"EXCEPT", "ELSE:", "ELIF"};
  char *pythKeywords[] = {"FOR", "IF", "ELSE:", "ELIF", "TRY:", "WHILE", "DEF", "RETURN",
    "GLOBAL", "BREAK", "CONTINUE", "EXCEPT:", "PASS", "WITH", "FINALLY", "CLASS", 
    "RAISE"};
  int numPrevKeys = sizeof(prevKeys) / sizeof(const char *);
  int numCurKeys = sizeof(curKeys) / sizeof(const char *);
  int numCurPyth = sizeof(curPythKeys) / sizeof(const char *);
  int numPythKeywords = sizeof(pythKeywords) / sizeof(const char *);
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();

  completing = false;
  setCompletions = false;
  if (sel2 <= 0)
    return;
  completing = strMacro.GetAt(sel2-1) == '`';
  isPython = CheckForPythonAndImport(strMacro, importName);
  if (isPython && GetAsyncKeyState(VK_SHIFT) / 2 != 0 &&
    strMacro.GetAt(sel2 - 1) == '~')
    completing = removeIndent = true;

  // Set up to delete this character
  numDel = 1;
  delStart = sel2 - (completing ? 1 : 0);
  cmdList = winApp->mMacroProcessor->GetCommandList(numCommands);
  matchList = new short[numCommands];

  // Make sure we are at the end of a word, also find out if there is a following space
  atWordEnd = sel2 >= strMacro.GetLength();
  if (!atWordEnd) {
    ch = strMacro.GetAt(sel2);
    hasSpace = (!isPython && (ch == ' ' || ch == '\t')) || (isPython && ch == '(');
    atWordEnd = hasSpace || ch == '\n' || ch == '\r';
  }

  // Find the beginning of the line or a previous space/tab
  if (atWordEnd) {
    saveStart = -1;
    for (lineStart = sel2 - 1; lineStart > 0; lineStart--) {
      ch = strMacro.GetAt(lineStart - 1);
      if (ch == '\n' || ch == '\r' || (isPython && ch == '=') || (oneLine && ch == ';'))
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

  // Skip comments
  if (lineStart >= 0 && strMacro.GetAt(lineStart) == '#')
    lineStart = -1;

  // For python, find a namespace component and if it matches serialem name, set the
  // start after that; if it doesn't, skip it
  else if (isPython && delStart > lineStart) {
    needsNamespace = !importName.IsEmpty();
    substr = strMacro.Mid(lineStart, delStart - lineStart);
    i = substr.Find(".");
    if (i > 0) {
      nameSpace = substr.Left(i);
      if (nameSpace == importName) {
        lineStart += i + 1;
        needsNamespace = false;
      } else {
        lineStart = -1;
      }
    }
  }

  while (lineStart >=0 && lineStart < delStart) {

    // Extract the string before this point, make upper case and make a list of matches
    lenMatch = delStart - lineStart;
    if (lenMatch <= 0)
      break;
    substr = strMacro.Mid(lineStart, lenMatch);
    if (substr == "\r" || substr == "\n")
      break;
    substr.MakeUpper();
    if (isPython) {

      // For python, exclude keywords then test if it is allowed command and compare
      for (i = 0; i < numPythKeywords; i++) {
        if (strstr(pythKeywords[i], (LPCTSTR)substr) == pythKeywords[i]) {
          isKeyword = true;
          break;
        }
      }

      if (isKeyword) {
        numDel = 1;
        break;
      }

      for (i = 0; i < numCommands; i++)
        if (!(cmdList[i].arithAllowed & 8) && cmdList[i].cmd.find(substr) == 0)
          matchList[numMatch++] = i;

    } else {

      // for regular compare to string
      for (i = CME_EXIT; i < numCommands; i++)
        if (cmdList[i].cmd.find(substr) == 0 && 
          !CMacroProcessor::mPythonOnlyCmdSet.count(i))
          matchList[numMatch++] = i;
    }

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
        substr += B3DCHOICE(isPython, "()", " ");
    }
    if (needsNamespace) {
      substr = importName + "." + substr;
      lenMatch += importName.GetLength() + 1;
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
      
      // Find start of previous non-continuation line with text
      // Keep track of number of previous continuation lines, whether previous 
      // nonblank line ends with a : for python, and whether previous line is blank
      isBlank = GetPrevLineIndexes(strMacro, lineStart, isPython, prevStart, prevEnd, 
        isContinued);
      afterBlank = isBlank;
      foundText = !isBlank && prevStart >= 0;
      afterContinue = isContinued ? 1 : 0;
      afterColon = isPython && foundText && strMacro.GetAt(prevEnd) == ':';
      if (prevStart > 0) {
        for (;;) {
          isBlank = GetPrevLineIndexes(strMacro, prevStart, isPython, backStart, backEnd,
            isContinued);

          // Stop if at start already or we have found nonblank line and this not a
          // continuation line
          if (backStart < 0 || (foundText && !isContinued))
            break;

          // Or, if we are after a continuation line and it is not the first one,
          // keep track of that and stop because we retain the indent
          if (afterContinue && isContinued) {
            afterContinue++;
            break;
          }

          // Otherwise adopt this as potentially previous line and keep track if found
          // a non-blank line and if the first non-blank line ended in :
          prevStart = backStart;
          prevEnd = backEnd;
          if (!isBlank) {
            if (isPython && !foundText && strMacro.GetAt(prevEnd) == ':')
              afterColon = true;
            foundText = true;
          }
        }
      }
       
      // Get the previous line indent and add if it matches a keyword, or is after a
      // colon, or if this is the first continuation line
      needIndent = 0;
      if (foundText && (FindIndentAndMatch(strMacro, prevStart, lineStart, prevKeys, 
        (isPython || afterContinue) ? 0 : numPrevKeys, needIndent) || afterColon ||
        afterContinue == 1))
        needIndent += indentSize;

      // Get the current line indent and subtract from needed indent if keyword matches
      // Also allow toggling if a Python line is after a blank line and is already
      // at the sam eindent as last line
      sel1 = strMacro.Find('\r', sel2);
      i = strMacro.Find('\n', sel2);
      if (sel1 < 0 && i < 0)
        sel1 = strMacro.GetLength();
      else
        sel1 = B3DMAX(sel1, i);
      if (FindIndentAndMatch(strMacro, lineStart, sel1, isPython ? curPythKeys : curKeys,
        B3DCHOICE(afterContinue, 0, isPython ? numCurPyth : numCurKeys), curIndent) || 
        (isPython && curIndent == needIndent && afterBlank && !afterColon))
        needIndent = B3DMAX(0, needIndent - indentSize);
      if (removeIndent)
        needIndent = B3DMAX(0, curIndent - indentSize);

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

// Check the start of a script for #!pyth... and importing of serialem
bool CMacroEditer::CheckForPythonAndImport(CString &strMacro, CString &importName)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  int currentInd = 0, length = strMacro.GetLength();
  CString strLine, strItems[4];
  bool isPython = false;
  importName = "";
  while (currentInd < length) {
    winApp->mMacroProcessor->GetNextLine(&strMacro, currentInd, strLine, true);

    // Look at first line for a match, if not, return
    if (!isPython) {
      strLine.MakeLower();
      isPython = strLine.Find("#!pyth") == 0;
      if (!isPython)
        return false;
      continue;
    }
 
    // Skip comments or includes
    strItems[0] = strLine;
    strItems[0].TrimLeft();
    strItems[0].TrimRight(" \r\n");
    if (strItems[0].Find("#") == 0 || strItems[0].IsEmpty()) {
      if (strItems[0].Find("#serialemPrefix") < 0)
        continue;
      winApp->mParamIO->ParseString(strLine, strItems, 4, false, true);
      importName = strItems[1];
      return true;
    }

    // If non-import line is found, give up
    if (strLine.Find("import") < 0)
      return true;

    // No namespace if importing this way
    if (strLine.Find("from serialem import *") >= 0)
      return true;

    // If importing serialem, see if an alias
    if (strLine.Find("import serialem") >= 0) {
      importName = "serialem";
      winApp->mParamIO->ParseString(strLine, strItems, 4);
      if (strItems[2] == "as" && !strItems[3].IsEmpty())
        importName = strItems[3];
      return true;
    }
  }
  return isPython;
}

// Get the starting and ending indexes of the text on line before curStart, set 
// isContinued if line ends with " \" or "\" if Python, and return true if the line
// is blank
bool CMacroEditer::GetPrevLineIndexes(CString &strMacro, int curStart, bool isPython,
  int &indStart, int &indEnd, bool &isContinued)
{
  int ind, numEndings = 0;
  char ch;
  bool isBlank = true;
  indStart = indEnd = -1;
  isContinued = false;
  if (!curStart)
    return false;
  while (curStart > 0) {
    ch = strMacro.GetAt(curStart - 1);

    // Look for line endings
    if (ch == '\r' || ch == '\n') {
      numEndings++;

      // Blank line
      if (numEndings > 2 && indEnd < 0) {
        indStart = curStart;
        indEnd = curStart - 1;
        break;
      }

      // If end already found, this is start
      if (indEnd >= 0) {
        indStart = curStart;
        break;
      }

      // If not a line ending, record if this is end of line
    } else if (indEnd < 0) {
      indEnd = curStart - 1;
    }
    curStart--;
  }
  if (!curStart)
    indStart = 0;
  for (ind = indStart; ind <= indEnd; ind++) {
    ch = strMacro.GetAt(ind);
    if (ch != ' ' && ch != '\t') {
      isBlank = false;
      break;
    }
  }
  isContinued = indEnd > indStart && strMacro.GetAt(indEnd) == '\\' && (isPython || 
    strMacro.GetAt(indEnd - 1) == ' ');
  return isBlank;
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
