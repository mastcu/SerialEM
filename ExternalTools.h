// ExternalTool.h: header file for CExternalTools class
#pragma once

#define MAX_EXTERNAL_TOOLS 25
class CExternalTools
{
public:
  CExternalTools(void);
  ~CExternalTools(void);
  GetMember(int, NumToolsAdded);
  GetMember(int, LastPSresol);
  GetMember(float, LastCropPixel);
  GetMember(float, LastFitStart);
  GetMember(float, LastFitEnd);
  GetSetMember(CString, CtfplotterPath);
  GetSetMember(BOOL, AllowWindow);
  PROCESS_INFORMATION mExtProcInfo;


private:
  CSerialEMApp *mWinApp;
  int mNumToolsAdded;
  CMenu *mPopupMenu;
  CArray<CString, CString> mTitles;
  CArray<CString, CString> mCommands;
  CArray<CString, CString> mArgStrings;
  CString mCtfplotterPath;
  int mLastPSresol;
  float mLastCropPixel;
  float mLastFitStart;
  float mLastFitEnd;
  int mFitAstigPhase;
  bool mDidAutotune;
  float mLastStartPhase;
  BOOL mAllowWindow;
public:
  void AddTool(CString &title);
  int AddCommand(int index, CString &command);
  int AddArgString(int index, CString &argString);
  int RunToolCommand(CString &title, CString extraArgs, int extraPlace);
  int RunToolCommand(int index);
  int RunCreateProcess(CString &command, CString argString, bool leaveHandles);
  void CloseFileHandles(HANDLE &hInFile, HANDLE &hOutFile);
  void AddMenuItems();
  void SubstituteAndQuote(CString &argString, const char *keyword, CString &replacement,
    bool doQuotes = true);
  ImodImageFile *SaveBufferToSharedMemory(int bufInd, CString nameSuffix, CString &filename);
  int MakeCtfplotterCommand(CString &memFile, int bufInd, float tiltOffset, 
    float defStart, float defEnd, int astigPhase, float phase,
    int resolTune, float cropPixel, float fitStart, float fitEnd, CString &command);
  int ReadCtfplotterResults(float &defocus, float &astig, float &angle, float &phase,
    int &ifAstigPhase, float &fitFreq, float &CCC, CString &results, CString &errString);
};

