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
#include "ShiftManager.h"
#include "ProcessImage.h"
#include "ExternalTools.h"
#include "EMbufferManager.h"
#include "SerialEMDoc.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"
#include "PluginManager.h"
#include "Shared/iimage.h"
#include "Shared/b3dutil.h"
#include "Shared/ctfutils.h"
#include "Shared/autodoc.h"

#define MAX_LINE 100
#define DEFOCUS_FILE "single.defocus"

CExternalTools::CExternalTools(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mNumToolsAdded = 0;
  mPopupMenu = NULL;
  mLastPSresol = 0;
  mLastCtfplotPixel = 0.;
  mAllowWindow = false;
  mCheckedForIMOD = false;
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
  return RunCreateProcess(mCommands[index], mArgStrings[index], false, "");
}

// Run a command by its title if it can be found
int CExternalTools::RunToolCommand(CString &title, CString extraArgs, int extraPlace, 
  CString inputStr)
{
  int strVal = atoi((LPCTSTR)title);
  for (int ind = 0; ind < mNumToolsAdded; ind++)
    if (!title.CompareNoCase(mTitles[ind])) {
      strVal = ind + 1;
      break;
    }
  if (strVal > 0 && strVal <= mNumToolsAdded) {
    if (extraArgs.IsEmpty()) {
      extraArgs = mArgStrings[strVal - 1];
    } else if (extraPlace > 0) {
      extraArgs = mArgStrings[strVal - 1] + " " + extraArgs;
    } else if (extraPlace < 0) {
      extraArgs = extraArgs + " " + mArgStrings[strVal - 1];
    }
    return RunCreateProcess(mCommands[strVal - 1], extraArgs, false, inputStr);
  }
  SEMMessageBox("No external tool menu entry matches " + title, MB_EXCLAME);
  return 1;
}

// Actually do the work of starting the process
int CExternalTools::RunCreateProcess(CString &command, CString argString, 
  bool leaveHandles, CString inputString)
{
  STARTUPINFO sinfo;
  bool isBatch = (command.Right(4)).CompareNoCase(".bat") == 0 || 
    (command.Right(4)).CompareNoCase(".cmd") == 0;
  TCHAR systemDirPath[MAX_PATH];
  CString report, outFile, inFile;
  char *cmdString;
  BOOL retval;
  int index, ind2;
  bool stdErrToFile = false;
  HANDLE hFile = NULL, hInFile = NULL;
  SECURITY_ATTRIBUTES sa;
  HANDLE hChildStd_IN_Rd = NULL;
  HANDLE hChildStd_IN_Wr = NULL;
  DWORD dwWrite, dwWritten;
  CString saveAutodoc, saveIMODdir;
  bool pipeInput = !inputString.IsEmpty();
  bool localCtfplot = !mLocalCtfplotPath.IsEmpty() &&
    command.Find(mLocalCtfplotPath) == 0;
  bool isCtfplotter = command.Find("ctfplotter") >= 0;
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  // Do substitutions in the argString
  if (argString.Find("%imagefile%") >= 0) {
    if (!mWinApp->mStoreMRC) {
      SEMMessageBox("Cannot substitute for %imagefile% in argument\n"
        "list for running a process; there is no open image file");
      return 1;
    }
    SubstituteAndQuote(argString, "%imagefile%", mWinApp->mStoreMRC->getFilePath());
  }
  if (argString.Find("%navfile%") >= 0) {
    if (!mWinApp->mNavigator) {
      SEMMessageBox("Cannot substitute for %navfile% in argument\n"
        "list for running a process; the Navigator is not open");
      return 1;
    }
    report = mWinApp->mNavigator->GetCurrentNavFile();
    if (report.IsEmpty()) {
      SEMMessageBox("Cannot substitute for %navfile% in argument\n"
        "list for running a process; there is no current Navigator file");
      return 1;
    }
    SubstituteAndQuote(argString, "%navfile%", report);
  }
  if (argString.Find("%currentdir%") >= 0) {
    SubstituteAndQuote(argString, "%currentdir%", CString(_getcwd(NULL, MAX_PATH)));
  }

  // Look for standard output redirection character and open a file if needed
  index = argString.ReverseFind('>');
  if (index >= 0 && index < argString.GetLength() - 1) {
    ind2 = index + 1;
    if (argString.GetAt(ind2) == '&') {
      ind2++;
      stdErrToFile = true;
    }
    outFile = argString.Mid(ind2);
    outFile = outFile.Trim();
    if (index)
      argString = argString.Left(index);
    else
      argString = "";
    hFile = CreateFile(_T(outFile),
      FILE_WRITE_DATA,
      FILE_SHARE_READ,
      &sa,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      report.Format("Error (%d) opening output file for redirection of process output",
        GetLastError());
      SEMMessageBox(report);
      return 1;
    }
  }

  // Then look for standard input redirection, open that file
  index = argString.ReverseFind('<');
  if (index >= 0 && index < argString.GetLength() - 1) {
    if (pipeInput) {
      SEMMessageBox("RunCreateProcess was called with a string to pipe to the input\n"
        "but the command string contains a < for input from a file");
      CloseFileHandles(hInFile, hFile);
      return 1;
    }
    ind2 = index + 1;
    inFile = argString.Mid(ind2);
    inFile = inFile.Trim();
    if (index)
      argString = argString.Left(index);
    else
      argString = "";
    hInFile = CreateFile(_T(inFile), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInFile == INVALID_HANDLE_VALUE) {
      report.Format("Error (%d) opening input file for redirection into process",
        GetLastError());
      SEMMessageBox(report);
      CloseFileHandles(hInFile, hFile);
      return 1;
    }
  }

  // set up the process arguments
  ZeroMemory( &sinfo, sizeof(STARTUPINFO) );
  sinfo.cb = sizeof(STARTUPINFO);
  ZeroMemory( &mExtProcInfo, sizeof(PROCESS_INFORMATION) );
  CString cstrCmd;
  report = "\"" + command + "\"";
  if (!argString.IsEmpty())
    report += " " + argString;

  if (pipeInput) {
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &sa, 0)) {
      SEMMessageBox("Error creating pipe for input to external process");
      CloseFileHandles(hInFile, hFile);
      return 1;
    }
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
      SEMMessageBox("Error running SetHandleInformation on input pipe to external "
        "process");
      CloseFileHandles(hInFile, hFile);
      CloseFileHandles(hChildStd_IN_Rd, hChildStd_IN_Wr);
      return 1;
    }
    sinfo.hStdInput = hChildStd_IN_Rd;
    sinfo.dwFlags |= STARTF_USESTDHANDLES;
  }

  // Attach the output file
  if (hFile || hInFile) {
    if (hFile) {
      sinfo.hStdOutput = hFile;
      if (stdErrToFile)
        sinfo.hStdError = hFile;
    }
    if (hInFile)
      sinfo.hStdInput = hInFile;
    sinfo.dwFlags |= STARTF_USESTDHANDLES;
  }

  // To run a .bat or .cmd, it needs the full path to cmd.exe as the command string
  // and the argument string needs to be /C followed by a completely quoted argument list
  // That way both the batch filename can be quoted and the argument list can have quotes
  if (isBatch) {
    GetSystemDirectory(systemDirPath, MAX_PATH);
    cstrCmd = systemDirPath;
    cstrCmd += "\\cmd.exe";
    report = "/C " + ("\"" + report + "\"");
    SEMTrace('1', "%s", (LPCTSTR)cstrCmd);
  }
  SEMTrace('1', "%s", (LPCTSTR)report);
  cmdString = strdup((LPCTSTR)report);
  if (!cmdString) {
    SEMMessageBox("Memory error composing string for process and arguments when trying to"
      " run a process");
    CloseFileHandles(hInFile, hFile);
    CloseFileHandles(hChildStd_IN_Rd, hChildStd_IN_Wr);
    return 1;
  }

  if (!m3dmodAutodocDir.IsEmpty() || localCtfplot) {
    if (getenv("AUTODOC_DIR"))
      saveAutodoc = getenv("AUTODOC_DIR");
    _putenv((LPCTSTR)("AUTODOC_DIR=" + B3DCHOICE(localCtfplot, mLocalCtfplotPath,
      m3dmodAutodocDir)));
    if (!m3dmodAutodocDir.IsEmpty() && m3dmodAutodocDir.Find("3dmod") > 
      m3dmodAutodocDir.GetLength() - 7 && getenv("IMOD_DIR")) {
      saveIMODdir = getenv("IMOD_DIR");
      _putenv("IMOD_DIR=");
    }
  }
  if (isCtfplotter)
    _putenv("PIP_PRINT_ENTRIES=1");

  // RUN THE PROCESS
  retval = CreateProcess(isBatch  ? (LPCTSTR)cstrCmd : NULL, cmdString, NULL, NULL, TRUE,
    (isBatch ? 0 : DETACHED_PROCESS) | (mAllowWindow ? 0 : CREATE_NO_WINDOW), NULL, NULL,
    &sinfo, &mExtProcInfo);

  // Restore 
  if (!saveAutodoc.IsEmpty()) {
    _putenv((LPCTSTR)("AUTODOC_DIR=" + saveAutodoc));
  }
  if (!saveIMODdir.IsEmpty())
    _putenv((LPCTSTR)("IMOD_DIR=" + saveIMODdir));
  if (isCtfplotter)
    _putenv("PIP_PRINT_ENTRIES=");

  if (!retval) {
    CloseFileHandles(hInFile, hFile);
    CloseFileHandles(hChildStd_IN_Rd, hChildStd_IN_Wr);
    report.Format("CreateProcess failed to start process %s with error %d",
        (LPCTSTR)command, GetLastError());
      SEMMessageBox(report);
  } else {

    if (pipeInput) {
      CloseHandle(hChildStd_IN_Rd);
      dwWrite = inputString.GetLength();
      retval = WriteFile(hChildStd_IN_Wr, (LPCTSTR)inputString, dwWrite, &dwWritten, 
        NULL);
      CloseHandle(hChildStd_IN_Wr);
      if (!retval) {
        CloseFileHandles(hInFile, hFile);
        CloseFileHandles(mExtProcInfo.hProcess, mExtProcInfo.hThread);
        report.Format("Writing input pipe to process %s failed with error %d",
          (LPCTSTR)command, GetLastError());
        SEMMessageBox(report);
      }
    }

    if (retval) {

      // Give it a second to finish with the command string
      WaitForSingleObject(mExtProcInfo.hProcess, pipeInput ? 250 : 1000);
      CloseFileHandles(hInFile, hFile);
      if (!leaveHandles) {
        CloseHandle(mExtProcInfo.hProcess);
        CloseHandle(mExtProcInfo.hThread);
      }
    }
  }
  free(cmdString);
  return retval ? 0 : 1;
}

// Convenience function to close both file handles, or any pair of them, leaving them NULL
void CExternalTools::CloseFileHandles(HANDLE &hInFile, HANDLE &hOutFile)
{
  if (hOutFile != NULL && hOutFile != INVALID_HANDLE_VALUE)
    CloseHandle(hOutFile);
  hOutFile = NULL;
  if (hInFile != NULL && hInFile != INVALID_HANDLE_VALUE)
    CloseHandle(hInFile);
  hInFile = NULL;
}

// Substitute every occurrence of the keyword in the argumnet string, quoting the whole 
// word that it is embedded in if it does not already have a quote at either end, and
// if doQuotes (default true) allows this.
void CExternalTools::SubstituteAndQuote(CString &argString, const char *keyword,
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

// Save the given buffer a shared memory file  with the given suffix after the standard
// name; returns the full name in filename and the iiFile for the shared memory object,
// which must be deleted after finishing the process using the file
ImodImageFile *CExternalTools::SaveBufferToSharedMemory(int bufInd, 
  CString nameSuffix, CString &filename, float reduction)
{
  ImodImageFile *iiFile;
  int err;
  EMimageBuffer saveBuf;
  EMimageBuffer *imBuf = mWinApp->GetImBufs() + bufInd;
  FileOptions *otherOpt = mWinApp->mDocWnd->GetOtherFileOpt();
  int nx, ny, pixSize, channels, kbSize, modeSave = otherOpt->mode;
  if (bufInd < 0 || bufInd >= MAX_BUFFERS || !imBuf->mImage) {
    filename = "Incorrect buffer index or no image in buffer";
    return NULL;
  }
  if (dataSizeForMode(imBuf->mImage->getType(), &pixSize, &channels) < 0 || channels> 1) {
    filename = "Unsupported data type for saving to shared memory file";
    return NULL;
  }
  iiFile = iiNew();
  if (!iiFile) {
    filename = "Error creating new ImodImageFile structure";
    return NULL;
  }

  if (reduction > 1.) {
    mWinApp->mBufferManager->CopyImBuf(imBuf, &saveBuf, false);
    if (mWinApp->mProcessImage->ReduceImage(imBuf, reduction, &filename, bufInd, false)) {
      free(iiFile);
      return NULL;
    }
  }

  imBuf->mImage->getSize(nx, ny);
  kbSize = (pixSize * nx * ny) / 1024 + 3;
  filename.Format("%s%d_%s", SHR_MEM_NAME_TAG, kbSize, (LPCTSTR)nameSuffix);
  if (iiShrMemCreate((LPCTSTR)filename, iiFile)) {
    filename.Format("Error creating shared memory file of size %d kBytes");
    free(iiFile);
    if (reduction > 1.)
      mWinApp->mBufferManager->CopyImBuf(&saveBuf, imBuf, false);
    return NULL;
  }
  otherOpt->mode = imBuf->mImage->getType();
  err = mWinApp->mDocWnd->SaveToOtherFile(bufInd, STORE_TYPE_IIMRC, 0, &filename);
  otherOpt->mode = modeSave;
  if (reduction > 1.)
    mWinApp->mBufferManager->CopyImBuf(&saveBuf, imBuf, false);
  if (err) {
    if (err == 1)
      filename = "Background save error prevented saving to shared memory file";
    else
      filename.Format("Error %s shared memory file", err == 2 ? "opening" : "saving to");
    iiDelete(iiFile);
    return NULL;
  }
  return iiFile;
}

//////////////////////////////////////////////////
// CTFPLOTTER FUNCTIONS
//////////////////////////////////////////////////

// Compose a command for running Ctfplotter with the given set of parameters and the 
// shared memory file whose full name is in memFile, returns the arguments in memFile or
// an error string on error; returns the ctfplotter command in command
int CExternalTools::MakeCtfplotterCommand(CString &memFile, float reduction, int bufInd,
  float tiltOffset, float defStart, float defEnd, float defExpect, int astigPhase, 
  float phase, int resolTune, float cropPixel, float fitStart, float fitEnd, int saveType,
  CString &saveName, CString &command)
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs() + bufInd;
  CString args, str;
  float tiltAngle, axisAngle;

  CheckForIMODPath();

  // Get important information from the image buffer: use pixel being fit in shared mem
  float imPixel = 1000.f * mWinApp->mShiftManager->GetPixelSize(imBuf) * reduction;
  if (!imPixel) {
    memFile = "No pixel size is available for running Ctfplotter on this image";
    return 1;
  }
  if (!imBuf->GetTiltAngle(tiltAngle)) {
    memFile = "No tilt angle is available for running Ctfplotter on this image";
    return 1;
  }

  // Get tilt axis angle, allow it not to exist if tilt is small
  if (imBuf->GetAxisAngle(axisAngle)) {
    axisAngle -= 90.;
  } else {
    axisAngle = 0;
    if (fabs(tiltAngle) > 1.) {
      memFile = "A tilt axis rotation angle is needed for running "
        "Ctfplotter on this image, which is tilted > 1 degree, and none is available";
      return 1;
    }
  }

  // Make the command  The path may eventually be required
  if (!mLocalCtfplotPath.IsEmpty() && !saveType)
    command = mLocalCtfplotPath + "\\ctfplotter.exe";
  else if (mCtfplotterPath.IsEmpty())
    command = "ctfplotter.exe";
  else
    command = mCtfplotterPath + "\\ctfplotter.exe";

  // Start the arguments with the required ones
  args.Format("-save -autoFit 0,0 -truncate 1 -defFn %s -single %.2f "
    "-aAngle %.2f -taOffset %.2f -degPhase %.2f -find %d,%d,0 -volt %d"
    " -cs %.3f -ampContrast %.3f -input %s", DEFOCUS_FILE, tiltAngle, 
    axisAngle, 
    tiltOffset, phase, (astigPhase & 1) ? 1 : 0, (astigPhase & 2) ? 1 : 0,
    B3DNINT(mWinApp->mProcessImage->GetRecentVoltage()),
    mWinApp->mProcessImage->GetSphericalAber(), mWinApp->mProcessImage->GetAmpRatio(),
    (LPCTSTR)memFile);
  if (fabs(defStart - defEnd) < 0.01) {
    str.Format(" -expDef %.0f", -1000. * defStart);
    args += str;
  } else {
    str.Format(" -scan %.0f,%.0f", -1000. * defStart, -1000. * defEnd);
    args += str;
    if (defExpect < 0.) {
      str.Format(" -expDef %.0f", -1000. * defExpect);
      args += str;
    }
  }

  // Handle PS resolution or tuning value, and other params
  if (resolTune == 1) {
    args += " -tune";
  } else {
    if (resolTune < 0) {
      if (!mLastPSresol) {
        memFile = "There are no parameters from a previous autotuning";
        return 1;
      }
      fitStart = mLastFitStart;
      fitEnd = mLastFitEnd;
      cropPixel = mLastCropPixel;

      // If no cropping happened, scale fit back up to pixel size in the reduced image
      // and scale for change in pixel size
      if (!cropPixel && mLastTunedPixel > 0.) {
        fitStart *= mLastTunedReduction * imPixel / mLastTunedPixel;
        fitEnd = B3DMIN(0.475f, mLastTunedReduction * fitEnd * imPixel / mLastTunedPixel);
      }
      resolTune = mLastPSresol;
    } else if (!cropPixel && reduction > 1) {
      
      // If cropping not specifed here, scale passed in 1/pixel values by reduction
      if (fitStart > 0 && fitStart < 1.)
        fitStart *= reduction;
      if (fitEnd > 0. && fitEnd < 1.)
        fitEnd = B3DMIN(0.475f, fitEnd * reduction);
    }
    if (resolTune) {
      str.Format(" -psRes %d", resolTune);
      args += str;
    }
    if (cropPixel >= imPixel) {
      str.Format(" -crop %.3f", cropPixel);
      args += str;
    }
    if (fitEnd > 0) {
      str.Format(" -frequency %.3f,%.3f", fitStart, fitEnd);
      args += str;
    }
  }
  if (mWinApp->mShiftManager->GetStageInvertsZAxis())
    args += " -invert";
  if (saveType) {
    args += saveType > 1 ? " -tiff 1" : " -png 1";
    args += " -snap " + saveName;
  }
  args += " > ctfplotter.log";
  memFile = args;

  // Save so we know what was fit
  mFitAstigPhase = astigPhase;
  mLastStartPhase = phase;
  mDidAutotune = resolTune == 1;
  mLastCtfplotPixel = imPixel;
  mLastReduction = reduction;
  return 0;
}

// Get the results from running Ctfplotter and compose a string and warning or error mess
int CExternalTools::ReadCtfplotterResults(float &defocus, float &astig, float &angle, 
  float &phase, int &ifAstigPhase, float &fitToFreq, float &CCC, CString &results, 
  CString &errString)
{
  int defVersion, versFlags, adocInd, err = 0, cropSpec;
  SavedDefocus *saved;
  CString str;

  // Read the result with the standard routine
  Ilist *savedList = readDefocusFile(DEFOCUS_FILE, defVersion, versFlags);
  if (!savedList) {
    errString = b3dGetError();
    return 1;
  }
  ifAstigPhase = mFitAstigPhase;
  phase = 0.;
  angle = 0.;
  astig = 0.;
  fitToFreq = 0.;
  CCC = 0.;
  if (!ilistSize(savedList)) {
    errString = "Ctfplotter saved no defocus values in " + CString(DEFOCUS_FILE);
    return 1;
  }
  saved = (SavedDefocus *)ilistItem(savedList, 0);
  defocus = -(float)saved->defocus;
  if (ifAstigPhase & 1) {
    defocus = -0.5f * (float)(saved->defocus + saved->defocus2);
    astig = (float)(saved->defocus - saved->defocus2);
    angle = (float)saved->astigAngle;
  }
  if (ifAstigPhase & 2)
    phase = (float)(saved->platePhase / DTOR);

  // Read the info file and look for tuning values
  adocInd = AdocRead("ctfplotter.info");
  if (adocInd < 0) {
    errString = "Failed to read ctfplotter.info for fitting information";
    if (mDidAutotune)
      mLastPSresol = 0;
    return -1;
  }

  if (mDidAutotune) {
    err += AdocGetInteger(ADOC_GLOBAL_NAME, 0, "PsResolution", &mLastPSresol);
    err += AdocGetInteger(ADOC_GLOBAL_NAME, 0, "CropSpectrum", &cropSpec);
    err += AdocGetFloat(ADOC_GLOBAL_NAME, 0, "PixelToCrop", &mLastCropPixel);
    err += AdocGetFloat(ADOC_GLOBAL_NAME, 0, "FittingStart", &mLastFitStart);
    err += AdocGetFloat(ADOC_GLOBAL_NAME, 0, "FittingEnd", &mLastFitEnd);
    mLastFitStart /= 2.f;
    mLastFitEnd /= 2.f;
    mLastTunedPixel = mLastCtfplotPixel;
    mLastTunedReduction = mLastReduction;

    // If there was no cropping, store fitting range in original pixel size
    if (!cropSpec) {
      mLastCropPixel = 0.;
      mLastAngstromStart = mLastCtfplotPixel / mLastFitStart;
      mLastAngstromEnd = mLastCtfplotPixel / mLastFitEnd;
      mLastFitStart /= mLastReduction;
      mLastFitEnd /= mLastReduction;
    } else {
      mLastAngstromStart = 10.f * mLastCropPixel / mLastFitStart;
      mLastAngstromEnd = 10.f * mLastCropPixel / mLastFitEnd;
    }
    if (err) {
      AdocClear(adocInd);
      errString = "Some parameters from autotuning were not found in ctfplotter.info";
      mLastPSresol = 0;
      return -1;
    }
  }

  // And stats of the fit
  AdocGetFloat(ADOC_GLOBAL_NAME, 0, "LastFitToResolution", &fitToFreq);
  AdocGetFloat(ADOC_GLOBAL_NAME, 0, "OverallCCC", &CCC);

  // Compose result string with whatever is available
  results.Format("Ctfplotter: defocus: %.3f,  astig: %.3f um,  angle: %.1f",
    defocus, astig, angle);
  if (ifAstigPhase & 2) {
    str.Format(",  phase shift %.1f deg", phase);
    results += str;
  } else if (mLastStartPhase) {
    str.Format(",  fixed phase %.1f deg", mLastStartPhase);
    results += str;
  }
  if (CCC) {
    str.Format(",  CCC %.4f", CCC);
    results += str;
  }
  if (fitToFreq) {
    str.Format(",   fit to %.1f A", fitToFreq);
    results += str;
  }
  AdocClear(adocInd);
  return 0;
}

// Check for the location of Ctfplotter/Genhstplt
void CExternalTools::CheckForIMODPath()
{
  CString path;
  const char *checkDirs[4] = {"C:\\Program Files\\3dmod", "C:\\Program Files\\IMOD\\bin",
    "C:\\cygwin\\usr\\local\\IMOD\\bin", "C:\\cygwin64\\usr\\local\\IMOD\\bin"};
  CFileStatus status;
  std::vector<std::string> strList;
  int ind;

  if (mCheckedForIMOD)
    return;
  
  mCheckedForIMOD = true;

  path = mWinApp->mPluginManager->GetExePath();
  if (CFile::GetStatus((LPCTSTR)(path + "\\ctfplotter.exe"), status) &&
    CFile::GetStatus((LPCTSTR)(path + "\\ctfplotter.adoc"), status))
    mLocalCtfplotPath = path;

  if (!mCtfplotterPath.IsEmpty()) {
    if (mCtfplotterPath.Find("3dmod") >= 0 &&
      CFile::GetStatus((LPCTSTR)(mCtfplotterPath + "\\genhstplt.exe"), status))
      m3dmodAutodocDir = mCtfplotterPath;
    return;
  }

  // Look on PATH and accept this if found
  path = getenv("PATH");

  UtilSplitString(path, ";", strList);
  if (path.Find("IMOD\\bin") >= 0 || path.Find("3dmod") >= 0) {
    for (ind = 0; ind < (int)strList.size(); ind++) {
      if (strList[ind].find("IMOD\\bin") >= 0 && CFile::GetStatus((LPCTSTR)
        (strList[ind].c_str() + CString("\\genhstplt.exe")), status)) {
        SEMTrace('1', "Found IMOD on system path");
        return;
      }
      if (strList[ind].find("3dmod") >= 0 && CFile::GetStatus((LPCTSTR)
        (strList[ind].c_str() + CString("\\ctfplotter.exe")), status)) {
        SEMTrace('1', "Found standalone 3dmod on system path");
        if (CFile::GetStatus((LPCTSTR)
          (strList[ind].c_str() + CString("\\ctfplotter.adoc")), status))
          m3dmodAutodocDir = strList[ind].c_str();
        return;
      }
    }
  }

  // Otherwise check the possible locations for standard installations
  for (ind = 0; ind < 4; ind++) {
    if (CFile::GetStatus(checkDirs[ind], status)) {
      if (!ind && !CFile::GetStatus("C:\\Program Files\\3dmod\\genhstplt.exe", status))
        continue;
      if (!ind && CFile::GetStatus("C:\\Program Files\\3dmod\\ctfplotter.adoc", status))
        m3dmodAutodocDir = "C:\\Program Files\\3dmod";
      mCtfplotterPath = checkDirs[ind];
      SEMTrace('1', "Found genhstplt, set CTfplotterPath to %s", checkDirs[ind]);
      break;
    }
  }
}

// This is ALMOST a copy of what is in IMOD, had to change it to store errors instead of
// exiting

/*
* Reads a defocus file whose name is in fnDefocus and stores the values
* in an Ilist of SavedDefocus structures, eliminating duplications if any.
* The return value is the Ilist, which may be empty if the file does not
* exist.  The version number is returned in defVersion, the flags in versFlags.
* Astigmatism angles are converted to angles if necessary, and phase plate shift is
* converted to radians.
*/
Ilist *readDefocusFile(const char *fnDefocus, int &defVersion, int &versFlags)
{
  FILE *fp;
  SavedDefocus saved;
  char line[MAX_LINE];
  int nchar, versTmp;
  bool readAstig = false, readPhase = false, readCuton = false;
  float langtmp, hangtmp, defoctmp, defoc2tmp, astigtmp, phasetmp, cutontmp;
  float angleSign = 1.;
  Ilist *lstSaved = ilistNew(sizeof(SavedDefocus), 10);
  if (!lstSaved) {
    b3dError(stderr, "Allocating list for angles and defocuses");
    return NULL;
  }
  fp = fopen(fnDefocus, "r");
  defVersion = 0;
  versFlags = 0;
  versTmp = 0;
  if (fp) {
    while (1) {
      nchar = fgetline(fp, line, MAX_LINE);
      if (nchar == -2)
        break;
      if (nchar == -1) {
        b3dError(stderr, "Error reading defocus file %s", fnDefocus);
        ilistDelete(lstSaved);
        return NULL;
      }
      if (nchar) {
        defoc2tmp = astigtmp = phasetmp = cutontmp = 0.;
        if (readAstig && readPhase && readCuton) {
          sscanf(line, "%d %d %f %f %f %f %f %f %f", &saved.startingSlice,
            &saved.endingSlice, &langtmp, &hangtmp, &defoctmp, &defoc2tmp, &astigtmp,
            &phasetmp, &cutontmp);
        } else if (readAstig && readPhase) {
          sscanf(line, "%d %d %f %f %f %f %f %f", &saved.startingSlice, &saved.endingSlice
            , &langtmp, &hangtmp, &defoctmp, &defoc2tmp, &astigtmp, &phasetmp);
        } else if (readAstig) {
          sscanf(line, "%d %d %f %f %f %f %f", &saved.startingSlice, &saved.endingSlice
            , &langtmp, &hangtmp, &defoctmp, &defoc2tmp, &astigtmp);
        } else if (readPhase && readCuton) {
          sscanf(line, "%d %d %f %f %f %f %f", &saved.startingSlice, &saved.endingSlice
            , &langtmp, &hangtmp, &defoctmp, &phasetmp, &cutontmp);
        } else if (readPhase) {
          sscanf(line, "%d %d %f %f %f %f", &saved.startingSlice, &saved.endingSlice
            , &langtmp, &hangtmp, &defoctmp, &phasetmp);
        } else {
          sscanf(line, "%d %d %f %f %f %d", &saved.startingSlice, &saved.endingSlice,
            &langtmp, &hangtmp, &defoctmp, &versTmp);
        }
        if (!ilistSize(lstSaved) && !defVersion) {
          defVersion = versTmp;
          if (defVersion > 2) {
            versFlags = saved.startingSlice;
            readAstig = (versFlags & DEF_FILE_HAS_ASTIG) != 0;
            readPhase = (versFlags & DEF_FILE_HAS_PHASE) != 0;
            readCuton = (versFlags & DEF_FILE_HAS_CUT_ON) != 0;
            if (versFlags & DEF_FILE_INVERT_ANGLES)
              angleSign = -1.;
            continue;
          }
        }
        saved.lAngle = angleSign * langtmp;
        saved.hAngle = angleSign * hangtmp;
        saved.defocus = defoctmp / 1000.;
        saved.startingSlice--;
        saved.endingSlice--;
        if (versFlags & DEF_FILE_ASTIG_IN_RAD)
          astigtmp /= (float)RADIANS_PER_DEGREE;
        astigtmp = (float)angleWithinLimits(astigtmp, -90.f, 90.f);
        if (!(versFlags & DEF_FILE_PHASE_IN_RAD))
          phasetmp *= (float)RADIANS_PER_DEGREE;
        saved.defocus2 = defoc2tmp / 1000.;
        saved.astigAngle = astigtmp;
        if (saved.defocus2 > saved.defocus) {
          saved.defocus2 = saved.defocus;
          saved.defocus = defoc2tmp / 1000.;
          saved.astigAngle -= 90.;
        }
        saved.platePhase = fabs(phasetmp);
        saved.cutOnFreq = cutontmp;
        if (addItemToDefocusList(lstSaved, saved) < 0) {
          ilistDelete(lstSaved);
          return NULL;
        }
      }
      if (nchar < 0)
        break;
    }
    fclose(fp);
  }
  return lstSaved;
}

/*
* Adds one item to the defocus list, keeping the list in order and
* and avoiding duplicate starting and ending view numbers
*/
int addItemToDefocusList(Ilist *lstSaved, SavedDefocus toSave)
{
  SavedDefocus *item;
  int i, matchInd = -1, insertInd = 0;

  // Look for match or place to insert
  for (i = 0; i < ilistSize(lstSaved); i++) {
    item = (SavedDefocus *)ilistItem(lstSaved, i);
    if (item->startingSlice == toSave.startingSlice &&
      item->endingSlice == toSave.endingSlice) {
      matchInd = i;
      *item = toSave;
      break;
    }
    if (item->lAngle + item->hAngle <= toSave.lAngle + toSave.hAngle)
      insertInd = i + 1;
  }

  // If no match, now insert
  if (matchInd < 0 && ilistInsert(lstSaved, &toSave, insertInd)) {
    b3dError(stderr, "Failed to add item to list of angles and defocuses");
    return -1;
  }
  return matchInd < 0 ? insertInd : matchInd;
}

///////////////////////////////////////////////////////////
// GRAPHING WITH GENHSTPLT
///////////////////////////////////////////////////////////

// Write a data file, generate the commands to make a graph, and run genhstplt
int CExternalTools::StartGraph(std::vector<FloatVec> &values, IntVec &types,
  IntVec typeList, IntVec columnList, IntVec symbolList, CString &axisLabel,
  std::vector<std::string> &keys,
  CString &errString, bool connect, int ordinals, int colors, int xlog, float xbase,
  int ylog, float ybase, float xmin, float xmax, float ymin, float ymax, CString pngName,
  bool saveTiff)
{
  CString dataStr;
  CFileStatus status;
  CString input, line, val, defKeyBase;
  int ind, col, xcol, numCurves, ifTypes = types.size() ? 1 : 0;
  int numInColList, numCol = (int)values.size();
  const char *dataName = "genhstplt.data";
  int numData = (int)values[0].size();
  const int numHues = 12;
  int hues[numHues][3] = {{0, 0, 139},{139, 0, 0},{0, 100, 0},{148, 0, 211},
  {255, 140, 0},{0, 128, 128},{128, 128, 0},{0, 0, 0},{255, 0, 0},{0, 128, 0},
  {255, 0, 255},{0, 255, 255}};
  const int maxSymbols = 14;
  int symbols[maxSymbols] = {9, 7, 5, 8, 13, 14, 1, 11, 3, 10, 6, 2, 12, 4};
  bool newType, noTypeList = !typeList.size(), noColumns = !columnList.size();

  CheckForIMODPath();
  errString = "";
  defKeyBase = ifTypes ? "Type " : "Column ";
  B3DCLAMP(ordinals, 0, 1);
  if (numCol == 1 || columnList.size() == 1)
    ordinals = 1;

  // Check that arrays have matching lengths
  if (ifTypes && types.size() != numData) {
    errString = "There are not the same number of type values as data values";
    return 1;
  }
  if (ifTypes && numCol > 2) {
    errString = "When plotting by types, there can only be two columns of values";
  }

  if (noColumns)
    columnList.push_back(1);
  for (col = 1; col < numCol; col++) {
    if (values[col].size() != numData) {
      errString.Format("There are not the same number of values for column %d as for"
        " column 1 (%d vs. %d)", col + 1, values[col].size(), numData);
      return 1;
    }
    if (noColumns)
      columnList.push_back(col + 1);
  }
  numInColList = (int)columnList.size();

  // Put the data into the lines
  for (ind = 0; ind < numData; ind++) {
    line = "";
    if (ifTypes) {
      line.Format("%d ", types[ind]);

      // Also build a type list if needed
      if (noTypeList) {
        newType = true;
        for (col = 0; col < (int)typeList.size(); col++) {
          if (types[ind] == typeList[col]) {
            newType = false;
            break;
          }
        }
        if (newType)
          typeList.push_back(types[ind]);
      }
    }
    for (col = 0; col < numCol; col++) {
      val.Format("%g ", values[col][ind]);
      line += val;
    }
    dataStr += line + "\r\n";
  }

  // Set up column and type list when no types and multiple curves
  if (!ifTypes && numInColList > 2 - ordinals) {
    ifTypes = -1;
    typeList.clear();
    for (ind = 1 - ordinals; ind < numInColList; ind++)
      typeList.push_back(columnList[ind]);
    xcol = columnList[0];
    columnList.clear();
    if (!ordinals)
      columnList.push_back(1);
    columnList.push_back(2);
  }
  numCurves = ifTypes ? (int)typeList.size() : 1;
  if (numCurves > maxSymbols) {
    errString = "There are too many data columns for symbols available";
    return 1;
  }

  // Wait a second for a previous genhstplt to delete the file before proceeding
  for (ind = 0; ind < 20; ind++) {
    if (!CFile::GetStatus(dataName, status))
      break;
    Sleep(50);
  }

  if (UtilWriteTextFile(dataName, dataStr)) {
    errString = "Error writing a text file with data to graph";
    return 1;
  }

  // Set up default axis label
  if (axisLabel.IsEmpty()) {
    if (ordinals)
      axisLabel = "Data number";
    else
      axisLabel.Format("Column %d", columnList[0]);
  }

  // Set up symbol list
  for (ind = 0; ind < (int)B3DMIN(symbolList.size(), maxSymbols); ind++)
    if (symbolList[ind] >= -1 && symbolList[ind] < 19)
      symbols[ind] = symbolList[ind];

  // Build the awful input list

  input.Format("-1\r\n%d\r\n%d\r\n0\r\n%s\r\n", ifTypes, numCol, dataName);
  if (ifTypes < 0)
    AddSingleValueLine(input, xcol);
  if (ifTypes) {
    AddSingleValueLine(input, -(int)typeList.size());
    for (ind = 0; ind < (int)typeList.size(); ind++) {
      line.Format("%d,%d\r\n", typeList[ind], symbols[ind]);
      input += line;
    }
  } else
    AddSingleValueLine(input, symbols[0]);

  if (numCol > 1)
    AddSingleValueLine(input, columnList[0]);

  // Xlog inputs
  line.Format("%d,%f\r\n", xlog, xbase);
  input += line;
  AddSingleValueLine(input, 0);

  // Set up ordinals then add second column
  if (ordinals)
    AddSingleValueLine(input, 16);
  AddSingleValueLine(input, 1);
  if (numCol > 1)
    AddSingleValueLine(input, columnList[1 - ordinals]);

  // Xlog and Ylog inputs
  line.Format("%d,%f\r\n", ylog, ybase);
  input += line;
  AddSingleValueLine(input, 0);
  AddSingleValueLine(input, -2);
  input += axisLabel + "\r\n";
  AddSingleValueLine(input, numCurves);

  // Keys
  for (ind = 0; ind < B3DMIN((int)keys.size(), numCurves); ind++)
    input += CString(keys[ind].c_str()) + "\r\n";
  for (ind = (int)keys.size(); ind < numCurves; ind++) {
    line.Format("%s%d\r\n", (LPCTSTR)defKeyBase, B3DCHOICE(ifTypes, typeList[ind],
      columnList[ind + 1 - ordinals]));
    input += line;
  }

  // Colors
  if (colors >= 0) {
    AddSingleValueLine(input, -4);
    AddSingleValueLine(input, numCurves);
    ind = 0;
    for (col = 0; col < numCurves; col++) {
      if (col + 1 == colors) {
        line.Format("%d,0,0,0\r\n", col + 1);
      } else {
        if (colors > 0 && !hues[ind][0] && !hues[ind][1] && !hues[ind][2])
          ind = (ind + 1) % numHues;
        line.Format("%d,%d,%d,%d\r\n", col + 1, hues[ind][0], hues[ind][1], hues[ind][2]);
        ind = (ind + 1) % numHues;
      }
      input += line;
    }
  }

  // X and Y ranges
  if (xmin > EXTRA_VALUE_TEST && xmax > EXTRA_VALUE_TEST) {
    AddSingleValueLine(input, -10);
    line.Format("%f,%f\r\n", xmin, xmax);
    input += line;
  }
  if (ymin > EXTRA_VALUE_TEST && ymax > EXTRA_VALUE_TEST) {
    AddSingleValueLine(input, -9);
    line.Format("%f,%f\r\n", ymin, ymax);
    input += line;
  }

  AddSingleValueLine(input, connect ? 17 : 2);
  AddSingleValueLine(input, 0);
  AddSingleValueLine(input, 0);
  AddSingleValueLine(input, -11);
  if (!pngName.IsEmpty()) {
    AddSingleValueLine(input, saveTiff ? -13 : -7);
    input += pngName + "\r\n8\r\n";
  } else
    AddSingleValueLine(input, -8);
  //mWinApp->AppendToLog(input);

  // Make the command 
  if (mCtfplotterPath.IsEmpty())
    line = "genhstplt.exe";
  else
    line = mCtfplotterPath + "\\genhstplt.exe";

  col = RunCreateProcess(line, "", true, input);
  if (!col) {
    mGraphProcessHandles.push_back(mExtProcInfo.hProcess);
    mGraphThreadHandles.push_back(mExtProcInfo.hThread);
  }

  return col;
}

// Close all the graph processes
void CExternalTools::CloseAllGraphs()
{
  for (int ind = 0; ind < (int)mGraphProcessHandles.size(); ind++) {
    if (WaitForSingleObject(mGraphProcessHandles[ind], 1) != WAIT_OBJECT_0)
      TerminateProcess(mGraphProcessHandles[ind], 1);
    CloseHandle(mGraphProcessHandles[ind]);
    CloseHandle(mGraphThreadHandles[ind]);
  }
  mGraphProcessHandles.clear();
  mGraphThreadHandles.clear();
}

// Find out if any graphs open so it can be asked if close on exit
bool CExternalTools::CheckIfGraphsOpen()
{
  for (int ind = 0; ind < (int)mGraphProcessHandles.size(); ind++)
    if (WaitForSingleObject(mGraphProcessHandles[ind], 1) != WAIT_OBJECT_0)
      return true;
  return false;
}

// Add a line with a single integer to the string
void CExternalTools::AddSingleValueLine(CString & input, int value)
{
  CString val;
  val.Format("%d\r\n", value);
  input += val;
}
