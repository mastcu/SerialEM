// ExternalTool.h: header file for CExternalTools class
#pragma once

#define MAX_EXTERNAL_TOOLS 25
class CExternalTools
{
public:
  CExternalTools(void);
  ~CExternalTools(void);
  GetMember(int, NumToolsAdded);

private:
  CSerialEMApp *mWinApp;
  int mNumToolsAdded;
  CMenu *mPopupMenu;
  CArray<CString, CString> mTitles;
  CArray<CString, CString> mCommands;
  CArray<CString, CString> mArgStrings;
public:
  void AddTool(CString &title);
  int AddCommand(int index, CString &command);
  int AddArgString(int index, CString &argString);
  int RunToolCommand(CString &title, CString extraArgs, int extraPlace);
  int RunToolCommand(int index);
  int RunCreateProcess(CString &command, CString argString);
  void AddMenuItems();
  void SubtituteAndQuote(CString &argString, const char *keyword, CString &replacement,
    bool doQuotes = true);
};

