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
#include "SerialEMDoc.h"
#include "ParameterIO.h"
#include "NavigatorDlg.h"
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
  return RunCreateProcess(mCommands[index], mArgStrings[index], false);
}

// Run a command by its title if it can be found
int CExternalTools::RunToolCommand(CString &title, CString extraArgs, int extraPlace)
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
    return RunCreateProcess(mCommands[strVal - 1], extraArgs, false);
  }
  SEMMessageBox("No external tool menu entry matches " + title, MB_EXCLAME);
  return 1;
}

// Actually do the work of starting the process
int CExternalTools::RunCreateProcess(CString &command, CString argString, 
  bool leaveHandles)
{
  STARTUPINFO sinfo;
  bool isBatch = (command.Right(4)).CompareNoCase(".bat") == 0 || 
    (command.Right(4)).CompareNoCase(".cmd") == 0;
  TCHAR systemDirPath[MAX_PATH];
  CString report, outFile;
  char *cmdString;
  int retval, index, ind2;
  bool stdErrToFile = false;
  HANDLE hFile = NULL;
  SECURITY_ATTRIBUTES sa;
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

  // Look for standard redirection character and open a file if needed
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

  // set up the process arguments
  ZeroMemory( &sinfo, sizeof(STARTUPINFO) );
  sinfo.cb = sizeof(STARTUPINFO);
  ZeroMemory( &mExtProcInfo, sizeof(PROCESS_INFORMATION) );
  CString cstrCmd;
  report = "\"" + command + "\"";
  if (!argString.IsEmpty())
    report += " " + argString;

  // Attach the output file
  if (hFile) {
    sinfo.hStdOutput = hFile;
    if (stdErrToFile)
      sinfo.hStdError = hFile;
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
    return 1;
  }
  retval = CreateProcess(isBatch  ? (LPCTSTR)cstrCmd : NULL, cmdString, NULL, NULL, TRUE,
    (isBatch ? 0 : DETACHED_PROCESS) | CREATE_NO_WINDOW, NULL, NULL,
    &sinfo, &mExtProcInfo);
  if (!retval) {
    if (hFile)
      CloseHandle(hFile);
    report.Format("CreateProcess failed to start process %s with error %d",
        (LPCTSTR)command, GetLastError());
      SEMMessageBox(report);
  } else {

    // Give it a second to finish with the command string
    WaitForSingleObject(mExtProcInfo.hProcess, 1000);
    if (hFile)
      CloseHandle(hFile);
    if (!leaveHandles) {
      CloseHandle(mExtProcInfo.hProcess);
      CloseHandle(mExtProcInfo.hThread);
    }
  }
  free(cmdString);
  return retval ? 0 : 1;
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
  CString nameSuffix, CString &filename)
{
  ImodImageFile *iiFile;
  int err;
  EMimageBuffer *imBuf = mWinApp->GetImBufs() + bufInd;
  int nx, ny, pixSize, channels, kbSize;
  if (bufInd < 0 || bufInd >= MAX_BUFFERS || !imBuf->mImage) {
    filename = "Incorrect buffer index or no image in buffer";
    return NULL;
  }
  imBuf->mImage->getSize(nx, ny);
  if (dataSizeForMode(imBuf->mImage->getType(), &pixSize, &channels) < 0 || channels> 1) {
    filename = "Unsupported data type for saving to shared memory file";
    return NULL;
  }
  kbSize = (pixSize * nx * ny) / 1024 + 3;
  filename.Format("%s%d_%s", SHR_MEM_NAME_TAG, kbSize, (LPCTSTR)nameSuffix);
  iiFile = iiNew();
  if (!iiFile) {
    filename = "Error creating new ImodImageFile structure";
    return NULL;
  }
  if (iiShrMemCreate((LPCTSTR)filename, iiFile)) {
    filename.Format("Error creating shared memory file of size %d kBytes");
    free(iiFile);
    return NULL;
  }
  err = mWinApp->mDocWnd->SaveToOtherFile(bufInd, STORE_TYPE_IIMRC, 0, &filename);
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

// Compose a command for running Ctfplotter with the given set of parameters and the 
// shared memory file whose full name is in memFile, returns the arguments in memFile or
// an error string on error; returns the ctfplotter command in command
int CExternalTools::MakeCtfplotterCommand(CString &memFile, int bufInd,
  float tiltOffset, float defStart, float defEnd, int astigPhase, float phase, 
  int resolTune, float cropPixel, float fitStart, float fitEnd, CString &command)
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs() + bufInd;
  CString args, str;
  float tiltAngle, axisAngle;

  // Get important information from the image buffer
  float imPixel = 1000.f * mWinApp->mShiftManager->GetPixelSize(imBuf);
  if (!imPixel) {
    memFile = "No pixel size is available for running Ctfplotter on this image";
    return 1;
  }
  if (!imBuf->GetAxisAngle(axisAngle)) {
    memFile = "No tilt axis rotation angle is available for running "
      "Ctfplotter on this image";
    return 1;
  }
  axisAngle -= 90.;
  if (!imBuf->GetTiltAngle(tiltAngle)) {
    memFile = "No tilt angle is available for running Ctfplotter on this image";
    return 1;
  }

  // Make the command  The path may eventually be required
  if (mCtfplotterPath.IsEmpty())
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
  if (fabs(defStart - defEnd) < 0.01)
    str.Format(" -expDef %.0f", -1000. * defStart);
  else
    str.Format(" -scan %.0f,%.0f", -1000. * defStart, -1000. * defEnd);
  args += str;

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
      resolTune = mLastPSresol;
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
  args += " > ctfplotter.log";
  memFile = args;

  // Save so we know what was fit
  mFitAstigPhase = astigPhase;
  mLastStartPhase = phase;
  mDidAutotune = resolTune == 1;
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
  defocus = -0.001f * (float)saved->defocus;
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
    if (!cropSpec)
      mLastCropPixel = 0.;
    if (err) {
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

  return 0;
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
