// ExternalTools.cpp:  Maintains menu and runs commands for starting an external process
//
// Copyright (C) 2003-2019 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <direct.h>
#include "SerialEM.h"
#include "ExternalTools.h"
#include "SerialEMDoc.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"


CExternalTools::CExternalTools(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mNumToolsAdded = 0;
  mPopupMenu = NULL;
}


CExternalTools::~CExternalTools(void)
{
}

// Add a tool to the arrays
void CExternalTools::AddTool(CString &title)
{
  if (mNumToolsAdded == 25)
    AfxMessageBox("Only 25 External Tools can be added", MB_EXCLAME);
  mNumToolsAdded++;
  mTitles.Add(title);
  mCommands.Add(CString());
  mArgStrings.Add(CString());
}

// Indexes here are all numbered from 0.  The user will see indexes numbered from 1 in
// property entries and script calls
// Add the command associated with the item
int CExternalTools::AddCommand(int index, CString &command)
{
  CString mess;
  if (index < 0 || index >= mNumToolsAdded) {
    mess.Format("The ToolCommand entry # %d is numbered wrong or\n"
      "placed wrong; only %d ExternalTool items have been entered so far", index + 1,
      mNumToolsAdded);
    AfxMessageBox(mess, MB_EXCLAME);
    return 1;
  }
  mCommands[index] = command;
  return 0;
}

// Add the argument string (optional) for the item
int CExternalTools::AddArgString(int index, CString &argString)
{
  CString mess;
  if (index < 0 || index >= mNumToolsAdded) {
    mess.Format("The ToolArguments entry # %d is numbered wrong or\n"
      "placed wrong; only %d ExternalTool items have been entered so far", index + 1,
      mNumToolsAdded);
    AfxMessageBox(mess, MB_EXCLAME);
    return 1;
  }
  mArgStrings[index] = argString;
  return 0;
}

// After startup, adjust the menu
void CExternalTools::AddMenuItems()
{
  int ind;
  CString mess;
  UINT nID;
  CMenu *menu = mWinApp->m_pMainWnd->GetMenu()->GetSubMenu(10);
  if (mNumToolsAdded > 0) {

    // Modify the existing text then remove the rest of the items
    for (ind = 0; ind < MAX_EXTERNAL_TOOLS; ind++) {
      nID = (UINT)(ID_EXTERNAL_TOOL1 + ind);
      if (ind < mNumToolsAdded) {
        if (mCommands[ind].IsEmpty()) {
          mess.Format("The property \"ExternalTool %s\" is does not have\n"
            "a corresponding entry \"ToolCommand %d\"", mTitles[ind], ind + 1);
          AfxMessageBox(mess, MB_EXCLAME);
        }
        menu->ModifyMenu(nID, MF_BYCOMMAND | MF_STRING, nID, (LPCTSTR)mTitles[ind]);
      } else
          menu->DeleteMenu(nID, MF_BYCOMMAND);
    }
  } else {

    // Or remove the whole menu!
    mWinApp->m_pMainWnd->GetMenu()->DeleteMenu(10, MF_BYPOSITION);
  }
  mWinApp->m_pMainWnd->DrawMenuBar();
}

// Run a command by its index
int CExternalTools::RunToolCommand(int index)
{
  if (index < 0 || index >= mNumToolsAdded)
    return 1;
  return RunCreateProcess(mCommands[index], mArgStrings[index]);
}

// Run a command by its title if it can be found
int CExternalTools::RunToolCommand(CString &title)
{
  int strVal = atoi((LPCTSTR)title);
  for (int ind = 0; ind < mNumToolsAdded; ind++)
    if (!title.CompareNoCase(mTitles[ind]))
      return RunCreateProcess(mCommands[ind], mArgStrings[ind]);
  if (strVal > 0 && strVal <= mNumToolsAdded)
    return RunCreateProcess(mCommands[strVal - 1], mArgStrings[strVal - 1]);
  SEMMessageBox("No external tool menu entry matches " + title, MB_EXCLAME);
  return 1;
}

// Actually do the work of starting the process
int CExternalTools::RunCreateProcess(CString &command, CString argString)
{
  STARTUPINFO sinfo;
  PROCESS_INFORMATION pinfo;
  bool isBatch = (command.Right(4)).CompareNoCase(".bat") == 0 || 
    (command.Right(4)).CompareNoCase(".cmd") == 0;
  TCHAR systemDirPath[MAX_PATH];
  CString report;
  char *cmdString;
  int retval;

  // Do substitutions in the argString
  if (argString.Find("%imagefile%") >= 0) {
    if (!mWinApp->mStoreMRC) {
      SEMMessageBox("Cannot substitute for %imagefile% in argument\n"
        "list for running a process; there is no open image file");
      return 1;
    }
    SubtituteAndQuote(argString, "%imagefile%", mWinApp->mStoreMRC->getFilePath());
  }
  if (argString.Find("%navfile%") >= 0) {
    if (!mWinApp->mNavigator) {
      SEMMessageBox("Cannot substitute for %navfile% in argument\n"
        "list for running a process; the Navigator is not open");
      return 1;
    }
    report = mWinApp->mNavigator->GetNavFilename();
    if (report.IsEmpty()) {
      SEMMessageBox("Cannot substitute for %navfile% in argument\n"
        "list for running a process; there is no current Navigator file");
      return 1;
    }
    SubtituteAndQuote(argString, "%navfile%", report);
  }
  if (argString.Find("%currentdir%") >= 0) {
    SubtituteAndQuote(argString, "%currentdir%", CString(_getcwd(NULL, MAX_PATH)));
  }

  // set up the process arguments
  ZeroMemory( &sinfo, sizeof(sinfo) );
  sinfo.cb = sizeof(sinfo);
  ZeroMemory( &pinfo, sizeof(pinfo) );
  CString cstrCmd;
  report = "\"" + command + "\"";
  if (!argString.IsEmpty())
    report += " " + argString;

  // To run a .bat or .cmd, it needs the full path to cmd.exe as the command string
  // and the argument string needs to be /C followed by a completely quoted argument list
  // That way both the batch filename can be quoted and the argument list can have quotes
  if (isBatch) {
    GetSystemDirectory(systemDirPath, MAX_PATH);
    cstrCmd = systemDirPath;
    cstrCmd += "\\cmd.exe";
    report = "/C " + ("\"" + report + "\"");
    mWinApp->AppendToLog(cstrCmd);
  }
  mWinApp->AppendToLog(report);
  cmdString = strdup((LPCTSTR)report);
  if (!cmdString) {
    SEMMessageBox("Memory error composing string for process and arguments when trying to"
      " run a process");
    return 1;
  }
  retval = CreateProcess(isBatch  ? (LPCTSTR)cstrCmd : NULL, cmdString, NULL, NULL, FALSE,
    isBatch ? 0 : DETACHED_PROCESS, NULL, NULL,            // Use parent's directory
    &sinfo, &pinfo);
  if(!retval) {
      report.Format("CreateProcess failed to start process %s with error %d", 
        (LPCTSTR)command, GetLastError());
      SEMMessageBox(report);
  } else {

    // Give it a second to finish with the command string...
    WaitForSingleObject(pinfo.hProcess, 1000);
    CloseHandle(pinfo.hProcess);
    CloseHandle(pinfo.hThread);
  }
  free(cmdString);
  return retval ? 0 : 1;
}

// Substitute every occurrence of the keyword in the argumnet string, quoting the whole 
// word that it is embedded in if it does not already have a quote at either end, and
// if doQuotes (default true) allows this.
void CExternalTools::SubtituteAndQuote(CString &argString, const char *keyword,
  CString &replacement, bool doQuotes)
{
  int startInd, spaceInd, prevSpace, nextSpace, keyLen = (int)strlen(keyword);
  CString quote = "\"";
  while ((startInd = argString.Find(keyword)) >= 0) {

    // Find previous space or -1 for start of line
    if (startInd > 0)
      prevSpace = argString.Left(startInd).ReverseFind(' ');
    else
      prevSpace = -1;
    if (doQuotes && argString.GetAt(prevSpace + 1) != quote) {

      // If there is not a quote there, go on to find next sapce or end of line
      spaceInd = nextSpace = argString.Find(' ', startInd + keyLen);
      if (nextSpace < 0)
        nextSpace = argString.GetLength();
      if (argString.GetAt(nextSpace - 1) != quote) {

        // If there is not a quote there also, now insert the quotes and increase index
        if (spaceInd < 0)
          argString += quote;
        else
          argString.Insert(nextSpace, quote);
        argString.Insert(prevSpace + 1, quote);
        startInd++;
      }
    }

    // Remove keyword and insert the replacement
    argString.Delete(startInd, keyLen);
    argString.Insert(startInd, replacement);
  }
}

