#include "stdafx.h"
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "SEMUtilities.h"
#include "..\SerialEM.h"
#include "..\SerialEMDoc.h"
#include "..\Image\KStoreIMOD.h"
#include "..\EMscope.h"
#include "..\FalconHelper.h"
#include "..\CameraController.h"
#include "..\EMBufferManager.h"
#include "..\Shared\SEMCCDDefines.h"
#include "..\Shared\autodoc.h"
#include "..\Shared\iimage.h"
#include "..\Shared\framealign.h"


static CSerialEMApp *sWinApp;

static const char *FindMessage(int code, int *allCodes, char **messages);

static HANDLE sAutodocMutex = NULL;
#define ADOC_MUTEX_WAIT 10000

void SEMUtilInitialize()
{
  sWinApp = (CSerialEMApp *)AfxGetApp();
}


// Error codes from DM plugin
static int SEMCCDcodes[] =
  {IMAGE_NOT_FOUND, WRONG_DATA_TYPE, DM_CALL_EXCEPTION, NO_STACK_ID, STACK_NOT_3D,
   FILE_OPEN_ERROR, SEEK_ERROR, WRITE_DATA_ERROR, HEADER_ERROR, ROTBUF_MEMORY_ERROR,
   DIR_ALREADY_EXISTS, DIR_CREATE_ERROR, DIR_NOT_EXIST, SAVEDIR_IS_FILE, DIR_NOT_WRITABLE,
   FILE_ALREADY_EXISTS, QUIT_DURING_SAVE, OPEN_DEFECTS_ERROR, WRITE_DEFECTS_ERROR, 
   THREAD_ERROR, EARLY_RET_WITH_SYNC, CONTINUOUS_ENDED, BAD_SUM_LIST, BAD_ANTIALIAS_PARAM,
   CLIENT_SCRIPT_ERROR, GENERAL_SCRIPT_ERROR, GETTING_DEFECTS_ERROR, 
   DS_CHANNEL_NOT_ACQUIRED, NO_DEFERRED_SUM, NO_GPU_AVAILABLE, DEFECT_PARSE_ERROR, 
   GAIN_REF_LOAD_ERROR, BAD_FRAME_REDUCE_PARAM, FRAMEALI_INITIALIZE, FRAMEALI_NEXT_FRAME,
   FRAMEALI_FINISH_ALIGN, MAKECOM_BAD_PARAM, MAKECOM_NO_REL_PATH, OPEN_COM_ERROR,
   WRITE_COM_ERROR, OPEN_MDOC_ERROR, WRITE_MDOC_ERROR, COPY_MDOC_ERROR, 
   FRAMEALI_BAD_SUBSET, OPEN_SAVED_LIST_ERR, WRITE_SAVED_LIST_ERR, GRAB_AND_SKIP_ERR, 
   FRAMEDOC_NO_SAVING, FRAMEDOC_OPEN_ERR, FRAMEDOC_WRITE_ERR, NULL_IMAGE, 
   FRAMETS_NO_ANGLES, VIEW_IS_ACTIVE, 0};

static char *SEMCCDmessages[] =
{"Image not found from ID", "Image has unexpected data type",
"An exception occurred calling a DM function", "No ID was returned for image stack",
"Image stack is not marked as 3D",
  "Error opening new file", "Error seeking to position in file", "Error writing data to file",
 "Error writing header to file", "Error allocating memory",
 "Directory for saving multiple files already exists",
 "Error creating directory for multiple files", "Directory for saving stack file does "
 "not exist and\ncannot be created (does parent directory exist?)",
 "The specified path is a file, not a directory", "You are not able to write to the directory",
 "The output file already exists",
 "DigitalMicrograph exited while stack was being processed", "Error opening file for saving defect list",
 "Error writing file with defect list", "Error starting thread for acquisition",
 "You can not do early return with synchronous saving in GMS >= 2.31",
 "The continuous acquisition ended unexpectedly",
 "The frame summing list has some bad feature and is unusable",
 "Antialias reduction was requested with a bad parameter or in continuous mode",
 "An error occurred running a script sent from the program",
 "An error occurred running a script to perform operations in DM",
 "Error getting defect list", "The requested channel was not acquired",
 "No deferred sum was saved", "No GPU is available after restarting DM",
 "An error occurred parsing the defect list", "An error occurred loading the gain reference",
 "Image reduction in framealign was requested with bad size parameters",
 "An error occurred initializing frame alignment",
 "An error occurred when passing the next frame to framealign",
 "An error occurred when finishing the frame alignment",
 "You cannot write an align com file when saving one file per image or aligning in the plugin",
 "The align com file needs to be written to the same drive as the frames",
 "Error opening align com file", "Error writing align com file",
 "Error opening file to write mdoc text", "Error writing mdoc text to file",
 "Error copying mdoc file to save directory",
 "Subset parameters do not define a subset of at least 2 frames",
 "Error opening file to write list of saved frames",
 "Error writing file with list of saved frames",
 "Program error: a grab stack was specified and that is not allowed with frame skipping"
 , "Cannot save frame stack mdoc file, there is no current information about a frame"
 " stack", "Error opening frame stack mdoc file", "Error writing frame stack mdoc file",
 "DM returned a NULL pointer for an image or frame",
"The call to SetupFileSaving specified making tilt sums or using dynamic threshold but"
" included no tilt angles",
"The continuous View is active in DM and must be stopped to take an image for SerialEM"
};

static char *noDesc = "No error description available";

static const char *FindMessage(int code, int *allCodes, char **messages)
{
  int ind;
  for (ind = 0; ind < 1000; ind++) {
    if (!allCodes[ind] || messages[ind][0] == 0x00)
      return noDesc;
    if (code == allCodes[ind])
      return messages[ind];
  }
  return noDesc;
}

const char *SEMCCDErrorMessage(int code)
{
  return FindMessage(code, SEMCCDcodes, SEMCCDmessages);
}

// Gets the montage control set for the given parameters.  
// If trueSet is true return the true set index, otherwise the set to be used.  
// lowDose should be 0 or > 0 and is optional, the default is the current state
int MontageConSetNum(MontParam *param, bool trueSet, int lowDose)
{ 
  LowDoseParams *ldp = sWinApp->GetLowDoseParams();
  int set = RECORD_CONSET;
  if (lowDose < 0)
    lowDose = sWinApp->LowDoseMode() ? 1 : 0;
  if (lowDose && param->useViewInLowDose)
    set = VIEW_CONSET;
  else if (lowDose && param->useSearchInLowDose && ldp[SEARCH_AREA].magIndex)
    set = SEARCH_CONSET;
  else if (lowDose && param->usePrevInLowDose && ldp[RECORD_CONSET].magIndex)
    set = PREVIEW_CONSET;
  else if (lowDose && param->useMultiShot && ldp[RECORD_CONSET].magIndex)
    set = RECORD_CONSET;
  else if (!sWinApp->GetUseRecordForMontage() && param->useMontMapParams)
    set = MONT_USER_CONSET;
  if (!trueSet && set == SEARCH_CONSET && sWinApp->GetUseViewForSearch())
    set = VIEW_CONSET;
  return set;
}

// Returns the low dose area that would be used for a montage in low dose mode
int MontageLDAreaIndex(MontParam *param) 
{
  int set = MontageConSetNum(param, true, 1);
  return sWinApp->mCamera->ConSetToLDArea(set);
}

// Returns true if the montage parameters specifying using multishot and it is OK
// lowDose should be 0 or > 0 and is optional, the default is the current state
bool UseMultishotForMontage(MontParam *param, int lowDose)
{
  LowDoseParams *ldp = sWinApp->GetLowDoseParams();
  if (lowDose < 0)
    lowDose = sWinApp->LowDoseMode() ? 1 : 0;

  // View and Search take precedence in case they are set
  return (lowDose > 0 && param->useMultiShot && ldp[RECORD_CONSET].magIndex &&
    !param->useViewInLowDose && !param->useSearchInLowDose);
}

bool UtilOKtoAllocate(int numBytes)
{
  float memoryLim = sWinApp->GetMemoryLimit();
  if (!memoryLim)
    return true;
  return (sWinApp->mBufferWindow.MemorySummary() + numBytes < memoryLim);
}

// Function to return state of a thread and optional exitCode and clean it up if done
int UtilThreadBusy(CWinThread **threadpp, DWORD *exitPtr)
{
  DWORD exitCode;
  // Assess the state of the thread, if any
  if (*threadpp) {
    GetExitCodeThread((*threadpp)->m_hThread, &exitCode);
    if (exitPtr)
      *exitPtr = exitCode;
    if (exitCode == STILL_ACTIVE)
      return 1;
    
    // If it is no longer active, delete it
    delete *threadpp;
    *threadpp = NULL;

    if (!exitCode)
      return 0;
    else
      return -1;
  }
  return 0;
}

// Clean up a thread after an error: suspend, terminate, and delete thread
// The terminate will make the system release any mutexes held and mark as abandoned
void UtilThreadCleanup(CWinThread **threadpp)
{
  if (*threadpp) {
    (*threadpp)->SuspendThread();
    TerminateThread((*threadpp)->m_hThread, 0);
    delete *threadpp;
    *threadpp = NULL;
  }
}

// Open an existing file and return a KStoreMRC
KImageStore *UtilOpenOldMRCFile(CString filename)
{
  CFile *file;
  KStoreMRC *storeMRC = NULL;
  KStoreIMOD *storeHDF = NULL;

  if (sWinApp->mDocWnd->GetHDFsupported() && iiTestIfHDF((char *)(LPCTSTR)filename) == 1){
    storeHDF = new KStoreIMOD(filename);
    if (storeHDF && storeHDF->FileOK())
      return storeHDF;
    delete storeHDF;
    return NULL;
  }
  try {
    file = new CFile(filename, CFile::modeReadWrite |CFile::shareDenyWrite | 
      CFile::modeNoInherit);
  }
  catch (CFileException *err) {
    err->Delete();
    return NULL;
  }
  storeMRC = new KStoreMRC(file);
  if (!storeMRC || !storeMRC->FileOK()) {
    delete storeMRC;
    return NULL;
  }
  return storeMRC;
}

// Open an existing file and read the first image int the read buffer
int UtilOpenFileReadImage(CString filename, CString descrip)
{
  KImageStore *storeMRC = UtilOpenOldMRCFile(filename);
  if (!storeMRC) {
    SEMMessageBox("Cannot reopen the file with " + descrip + " image.", MB_EXCLAME);
    return 1;
  }

  // Reread the image
  int retval = sWinApp->mBufferManager->ReadFromFile(storeMRC, 0, 0);
  if (retval)
    SEMMessageBox("Error rereading the " + descrip + " image.", MB_EXCLAME);
  delete storeMRC;
  return retval;
}

// Open a file, save a single image to it from buffer A, and close it
int UtilSaveSingleImage(CString filename, CString descrip, bool useMdoc)
{
  EMimageBuffer *imBufs = sWinApp->GetImBufs();
  FileOptions fileOpt;
  KStoreMRC *storeMRC;
  imBufs->mCaptured = BUFFER_TRACKING;
  fileOpt.maxSec = 1;
  fileOpt.mode = imBufs->mImage->getType() == kUSHORT ? MRC_MODE_USHORT : MRC_MODE_SHORT;
  fileOpt.typext = VOLT_XY_MASK | TILT_MASK;
  fileOpt.useMdoc = useMdoc;
  fileOpt.leaveExistingMdoc = false;
  fileOpt.montageInMdoc = false;
  fileOpt.fileType = STORE_TYPE_MRC;
  storeMRC = new KStoreMRC(filename, fileOpt);
  if (storeMRC == NULL || !storeMRC->FileOK()) {
    SEMMessageBox("Error opening file for " + descrip + " image", MB_EXCLAME);
    delete storeMRC;
    return 1;
  }
  int retval = storeMRC->AppendImage(imBufs->mImage);
  if (retval)
    SEMMessageBox("Error writing " + descrip + " image to file", MB_EXCLAME);
  delete storeMRC;
  return retval;
}

// Check that available space on the disk is enough for what is needed by given operation
int UtilCheckDiskFreeSpace(float needed, const char * operation)
{
  CString direc, file;
  ULARGE_INTEGER available;
  if (!sWinApp->mStoreMRC)
    return 0;
  UtilSplitPath(sWinApp->mStoreMRC->getFilePath(), direc, file);
  if (direc.IsEmpty())
    return 0;
  if (direc.Right(1) != "\\")
    direc.Append("\\");
  if (GetDiskFreeSpaceEx((LPCTSTR)direc, &available, NULL, NULL)) {
    if (available.QuadPart * 0.99 < needed) {
      file.Format("There does not seem to be enough disk space to take this"
        " %s\n\nDo you want to stop and do something about this?", operation);
      if (SEMMessageBox(file, MB_QUESTION, true, IDYES) == IDYES)
        return 1;
    }
  }
  return 0;
}

// Remove a file and ignore errors
void UtilRemoveFile(CString filename)
{
  CFileStatus status;
  int numRetry = 5;
  for (int retry = 0; retry < numRetry; retry++) {
    try {
      if (CFile::GetStatus((LPCTSTR)filename, status))
        CFile::Remove((LPCTSTR)filename);
      return;
    }
    catch (CFileException *cerr) {
      cerr->Delete();
      if (retry < numRetry - 1)
        Sleep(500);
    }
  }
}

// Rename a file, removing an existing copy first, with stock or supplied error message,
// and option to return instead of popping up error.  If the message is non-empty it will
// be substituted for the stock.  If returnMess is true and message is non-NULL it will
// return it.  It also tries up to 5 times in case there is a temporary lock on file
int UtilRenameFile(CString fromName, CString toName, CString *message, bool returnMess)
{
  CFileStatus status;
  CString mess;
  int numRetry = 5;
  for (int retry = 0; retry < numRetry; retry++) {
    try {
      if (CFile::GetStatus((LPCTSTR)toName, status))
        CFile::Remove(toName);
      CFile::Rename(fromName, toName);
      break;
    }
    catch (CFileException *err) {
      err->Delete();
      if (retry < numRetry - 1) {
        Sleep(500);
      } else {
        if (message && !message->IsEmpty()) {
          mess = *message;
        } else {
          mess = "Error attempting to rename " + fromName + " to " + toName;
        }
        if (returnMess && message)
          *message = mess;
        else
          SEMMessageBox(mess, MB_EXCLAME);
        return 1;
      }
    }
  }
  return 0;
}

// Decompose a filename into root name and extension
void UtilSplitExtension(CString filename, CString &root, CString &ext)
{
  int index = filename.ReverseFind('.');
  root = filename;
  ext = "";
  if (index > 0) {
    root = filename.Left(index);
    ext = filename.Right(filename.GetLength() - index);
  }
}

// Decompose a full file path into directory and filename
void UtilSplitPath(CString fullPath, CString &directory, CString &filename)
{
  int index = fullPath.ReverseFind('\\');
  filename = fullPath;
  directory = "";
  if (index >= 0 && index == fullPath.GetLength() - 1) {
    directory = fullPath;
    filename = "";
  } else if (index >= 0) {
    directory = fullPath.Left(index + 1);
    filename = fullPath.Right(fullPath.GetLength() - index - 1);
  }
}
// Returns a relative path from fromDIR to toDir in relPath, returns 1 if no possible
int UtilRelativePath(std::string fromDir, std::string toDir, std::string &relPath)
{
  int ind, fromLen, toLen, findInd;
  fromLen = UtilStandardizePath(fromDir);
  toLen = UtilStandardizePath(toDir);

  // Find first non-matching character
  for (ind = 0; ind < B3DMIN(fromLen, toLen); ind++)
    if (fromDir[ind] != toDir[ind])
      break;
  if (!ind)
    return 1;

  // Switch to index of last match
  ind--;

  // If both at a /, back up
  if (fromDir[ind] == '/' && toDir[ind] == '/') {
    ind--;

    // For windows, do not allow a starting /
    if (ind < 0)
      return 1;

    // if both are either at the end or followed by a /, it is at a directory, but
    // but otherwise need to back up to previous /
  } else if (!((ind == fromLen - 1 || fromDir[ind + 1] == '/') &&
               (ind == toLen - 1 || toDir[ind + 1] == '/'))) {
    ind = (int)fromDir.find_last_of('/', ind);

    // The ind == 0 is specific to Windows to require drive letter at start
    if (ind == std::string::npos || ind == 0)
      return 1;
    ind--;
  }

  // return blank path if strings match to ends
  relPath.clear();
  if (ind == fromLen - 1 && ind == toLen - 1)
    return 0;

  // Start with a ../ for each directory after the match in the FROM directory
  findInd = fromLen - 1;
  while (findInd > ind + 1) {
    relPath = relPath + "../";
    findInd = (int)fromDir.find_last_of('/', findInd);
    if (findInd == std::string::npos || findInd <= ind + 1)
      break;
    findInd--;
  }

  // Then add the path from the match to the end in the TO directory
  if (ind < toLen - 2)
    relPath += toDir.substr(ind + 2) + "/";
  return 0;
}

int UtilRelativePath(CString fromDir, CString toDir, CString &relPath)
{ int err;
  std::string relStr, fromStr = (LPCTSTR)fromDir;
  std::string toStr = (LPCTSTR)toDir;
  err = UtilRelativePath(fromStr, toStr, relStr);
  relPath = relStr.c_str();
  return err;
}

// change \ to /, remove //, and remove trailing /, and return final length
int UtilStandardizePath(std::string &dir)
{
  size_t ind;
  while ((ind = dir.find('\\')) != std::string::npos)
    dir = dir.replace(ind, 1, "/");
  while ((ind = dir.find("//")) != std::string::npos)
    dir = dir.replace(ind, 2, "/");
  ind = dir.length();
  if (ind > 0 && dir[ind - 1] == '/') {
    dir.resize(ind - 1);
    ind--;
  }
  return (int)ind;
}

int UtilStandardizePath(CString &dir)
{
  int ind;
  std::string str = (LPCTSTR)dir;
  ind = UtilStandardizePath(str);
  dir = str.c_str();
  return ind;
}

// Makes a directory even if multiple parents do not exist
int UtilRecursiveMakeDir(CString dir, CString &mess)
{
  CString parent, subdir;
  CFileStatus status;
  UtilSplitPath(dir, parent, subdir);
  int err, len;

  // Either one can be empty if different cases occur: check for a drive only
  if (subdir.IsEmpty() || parent.IsEmpty()) {
    subdir += parent;
    if ((!subdir.IsEmpty() && subdir.GetLength() <= 3 && subdir.GetAt(1) == ':')) {
      mess = "Drive " + subdir + " does not exist; cannot make subdirectory";
      return 1;
    }
  }

  // Strip trailing backslash
  len = parent.GetLength();
  if (len > 0 && parent.GetAt(len - 1) == '\\')
    parent = parent.Left(len - 1);

  // If there is a parent and it does not exist, recurse to create it
  if (!parent.IsEmpty() && !CFile::GetStatus((LPCTSTR)parent, status)) {
    err = UtilRecursiveMakeDir(parent, mess);
    if (err)
      return err;
  }

  // Try to make the directory, ignore error if it (now) exists due to FEI
  if (_mkdir((LPCTSTR)dir)) {
    err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS)
      return 0;
    mess.Format("Error %d creating directory %s", err, (LPCTSTR)dir);
    return 1;
  }
  return 0;
}

// Tests whether a file can be created in a directory with read/write permission
bool UtilIsDirectoryUsable(CString &dir, int &error)
{
  CString tempFile = dir + "\\SEMtestWritableDir.txt";
  error = 0;
  HANDLE tdir = CreateFile((LPCSTR)tempFile, GENERIC_READ | GENERIC_WRITE, 
    0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  error = GetLastError();
  if (tdir == INVALID_HANDLE_VALUE)
    return false;
  CloseHandle(tdir);
  UtilRemoveFile(tempFile);
  return true;
}

// Returns true if the file exists, to save having to look up if GetStatus returns true!
BOOL UtilFileExists(CString filename)
{
  CFileStatus status;
  return CFile::GetStatus(filename, status);
}

// Append a component to a string with the given separator if the string is not empty
void UtilAppendWithSeparator(CString &filename, CString toAdd, const char *sep)
{
  if (!filename.IsEmpty())
    filename += CString(sep);
  filename += toAdd;
}

// Check for illegal characters in Windows filenames, possibly allowing slash (1) and a
// colon after a drive letter (2)
char UtilCheckIllegalChars(CString &filename, int slashOrDriveOK, CString descrip)
{
  char bad = 0;
  int ind = filename.FindOneOf("?*\"<>|");
  if (ind >= 0) {
    bad = filename.GetAt(ind);
  } else {
    ind = filename.Find(":", slashOrDriveOK > 1 ? 2 : 0);
    if (ind >= 0 || filename.GetAt(0) == ':')
      bad = ':';
    else if (slashOrDriveOK)
      return 0;
    ind = filename.FindOneOf("/\\");
    if (ind >= 0)
      bad = filename.GetAt(ind);
  }
  if (bad && !descrip.IsEmpty())
    AfxMessageBox(descrip + " contains the character " + CString(bad) +
      ", which is not allowed in filenames", MB_EXCLAME);
  return bad;
}

// Split a CString into std::string vector
void UtilSplitString(CString fullStr, const char *delim, 
  std::vector<std::string> &strList)
{
  int ind, lenDelim = (int)strlen(delim);
  strList.clear();
  while ((ind = fullStr.Find(delim)) >= 0) {
    strList.push_back((LPCTSTR)fullStr.Left(ind));
    fullStr = fullStr.Mid(ind + lenDelim);
  }
  strList.push_back((LPCTSTR)fullStr);
}

void UtilSplitString(std::string stdStr, const char *delim, 
  std::vector<std::string> &strList)
{
  CString fullStr = stdStr.c_str();
  UtilSplitString(fullStr, delim, strList);
}

// Remove zeros from end of a string ending in a floating point number
void UtilTrimTrailingZeros(CString & str)
{
  int length, trim = 0;
  if (str.Find('.')) {
    length = str.GetLength();
    while (str.GetAt(length - trim - 1) == '0')
      trim++;
    if (str.GetAt(length - trim - 1) == '.')
      trim++;
    if (trim)
      str = str.Left(length - trim);
  }
}

// Creates a directory for frame-saving if it does not already exist, checks if parent
// does not exist; returns error in errStr if nonNULL or does debug output on given letter
int CreateFrameDirIfNeeded(CString &directory, CString *errStr, char debug)
{
  CFileStatus status;
  CString parent, tmp;
  int len;
  if (directory.IsEmpty()) {
    tmp = "Directory name is empty in CreateFrameDirIfNeeded";
    if (errStr)
      *errStr = tmp;
    else
      SEMTrace(debug, "%s", (LPCTSTR)tmp);
    return 1;
  }
  if (!CFile::GetStatus((LPCTSTR)directory, status)) {
    SEMTrace(debug, "Directory %s does not exist, creating it", (LPCTSTR)directory);
    if (_mkdir((LPCTSTR)directory)) {
      directory.Replace('/', '\\');
      len = directory.GetLength();
      if (directory.GetAt(len - 1) == '\\')
        directory = directory.Left(len - 1);
      UtilSplitPath(directory, parent, tmp);
      tmp.Format("An error occurred creating the directory:\r\n   %s\r\n"
        "   for saving frame images %s", (LPCTSTR)directory,
        CFile::GetStatus((LPCTSTR)parent, status) ? 
        "even though its parent directory exists" : 
        "because its parent directory does not exist");
      if (errStr)
        *errStr = tmp;
      else
        SEMTrace(debug, "%s", (LPCTSTR)tmp);
      return 1;
    }
  }
  return 0;
}

// Compute inclusive limits "start" and "end" of group at index groupInd when dividing 
// a total of numTotal items into numGroups groups as evenly as possible, with remainder
// distributed among the first groups
void UtilBalancedGroupLimits(int numTotal, int numGroups, int groupInd, int &start, 
                             int &end)
{
  int base = numTotal / numGroups;
  int rem = numTotal % numGroups;
  start = groupInd * base + B3DMIN(groupInd, rem);
  end = (groupInd + 1) * base + B3DMIN(groupInd + 1, rem) - 1;
}

// Get an interpolated peak position after finishing a search with a given step size
void UtilInterpolatePeak(FloatVec &vecPos, FloatVec &vecPeak, float step, 
  float peakmax, float &posBest)
{
  int ist;
  float peakBelow = -1.e30f, peakAbove = -1.e30f;

  // Find the values at positions below and above the peak
  for (ist = 0; ist < (int)vecPeak.size(); ist++) {
    if (fabs(vecPos[ist] - (posBest - step)) < 0.1 * step)
      peakBelow = vecPeak[ist];
    if (fabs(vecPos[ist] - (posBest + step)) < 0.1 * step)
      peakAbove = vecPeak[ist];
  }
  if (peakBelow > -1.e29 && peakAbove > -1.e29)
    posBest += step * (float)parabolicFitPosition(peakBelow, peakmax, peakAbove);
}

// Return mag value for mag index
int MagOrEFTEMmag(BOOL GIFmode, int index, BOOL STEMcam)
{
  MagTable *magTab = sWinApp->GetMagTable();
  if (STEMcam)
    return B3DNINT(magTab[index].STEMmag);
  return GIFmode ? magTab[index].EFTEMmag : magTab[index].mag;
}

int MagForCamera(CameraParameters *camParam, int index)
{
  return MagOrEFTEMmag(camParam->GIF, index, camParam->STEMcamera);
}
int MagForCamera(int camera, int index)
{
  CameraParameters *camParam = sWinApp->GetCamParams() + camera;
  return MagForCamera(camParam, index);
}

// Or find mag index for mag value; camera defaults to -1 for current camera
int FindIndexForMagValue(int magval, int camera, int magInd)
{
  int i, minInd, thisMag, diff, minMag, minDiff, limlo = 1, limhi = MAX_MAGS - 1;
  CEMscope *scope = sWinApp->mScope;
  bool inverted = false;
  int secondary = scope->GetLowestSecondaryMag();
  if (camera < 0)
    camera = sWinApp->GetCurrentCamera();
  CameraParameters *camParams = sWinApp->GetCamParams() + camera;
  BOOL ifSTEM = scope->GetSTEMmode();

  // For FEI STEM mode, get the right range and stay out of LM
  if (FEIscope && ifSTEM) {
    limlo = scope->GetLowestSTEMnonLMmag(scope->GetProbeMode());
    if (!scope->GetProbeMode())
      limhi = scope->GetLowestMicroSTEMmag() - 1;
  }
  if (!ifSTEM && secondary && !scope->GetSecondaryMode())
    limhi = secondary - 1;

  if ((magInd < -1 || magInd > 0) && scope->GetUseInvertedMagRange()) {
    if (magInd < 0)
      magInd = scope->GetMagIndex();
    inverted = UtilMagInInvertedRange(magInd, camParams->GIF);
    if (inverted)
      UtilInvertedMagRangeLimits(camParams->GIF, limlo, limhi);
  } 

  // Otherwise exclude the inverted top end Krios mags
  if (!secondary && !ifSTEM && !inverted) {
    for (i = limhi; i > scope->GetLowestMModeMagInd(); i--) {
      if (MagOrEFTEMmag(camParams->GIF, i, ifSTEM) > 
        MagOrEFTEMmag(camParams->GIF, i - 1, ifSTEM))
          break;
      limhi--;
    }
  }

  // Look up mags from high to low to avoid a LM mag that duplicates Mag mode mag
  // Just look for the nearest mag first
  minInd = -1;
  for (i = limhi; i >= limlo; i--) {
    if (!ifSTEM && secondary && scope->GetSecondaryMode() && i == secondary - 1)
      i = scope->GetLowestMModeMagInd() - 1;
    thisMag = MagOrEFTEMmag(camParams->GIF, i, ifSTEM);
    diff = magval > thisMag ? (magval - thisMag) : (thisMag - magval);
    if (minInd < 0 || diff < minDiff) {
      minDiff = diff;
      minInd = i;
      minMag = thisMag;
    }
  }

  // Now require it not be midway between mags, compare to spacing between mags
  if (minInd == limhi)
    i = minInd - 1;
  else if (minInd == limlo)
    i = minInd + 1;
  else
    i = magval > minMag ? 1 : -1;
  thisMag = MagOrEFTEMmag(camParams->GIF, i, ifSTEM);
  if (minDiff < 0.35 * fabs((double)thisMag - minMag))
    return minInd;
  return 0;
}

// Return inclusive limits for an inverted mag range at top of table and true if it
// exists, for EFTEM or not
bool UtilInvertedMagRangeLimits(BOOL EFTEM, int &lowInd, int &highInd)
{
  MagTable *magTab = sWinApp->GetMagTable();
  int ind, mag, lastMag, lastInd = -1, prevInd = -1;
  highInd = -1;
  lowInd = -1;
  for (ind = MAX_MAGS - 1; ind > 0; ind--) {
    mag = B3DCHOICE(EFTEM, magTab[ind].EFTEMmag, magTab[ind].mag);

    // Skip any 0 mags, record first non-zero mag, otherwise check if this mag is less
    // than last one
    if (!mag)
      continue;
    if (highInd < 0)
      highInd = ind;
    else if (lastMag > mag) {
      lowInd = prevInd;
      break;
    }

    // Roll the last and previous index, save last mag
    prevInd = lastInd;
    lastInd = ind;
    lastMag = mag;
  }
  return highInd >= lowInd && lowInd > 0;
}

// Return true if the given mag is in the invered mag range for EFTEM or not
bool UtilMagInInvertedRange(int magInd, BOOL EFTEM)
{
  int limLow, limHigh;
  if (!UtilInvertedMagRangeLimits(EFTEM, limLow, limHigh))
    return false;
  return magInd >= limLow && magInd <= limHigh;
}

// Return a divisor for a binning to apply for a camera with super-resolution mode
int BinDivisorI(CameraParameters *camParam)
{
  return ((camParam->K2Type || DECTRIS_WITH_SUPER_RES(camParam))? 2 : 1);
}

float BinDivisorF(CameraParameters *camParam)
{
  return (camParam->K2Type || DECTRIS_WITH_SUPER_RES(camParam) ? 2.f : 1.f);
}

// Return true if a camera has its binnings doubled to allow for super-res images
bool CamHasDoubledBinnings(CameraParameters *camParam)
{
  return (camParam->K2Type > 0) || DECTRIS_WITH_SUPER_RES(camParam);
}

// Returns the index in the CamLowDoseParams for the given camera (-1 for current)
int CamLDParamIndex(int camera)
{
  if (camera < 0)
    camera = sWinApp->GetCurrentCamera();
  return CamLDParamIndex(sWinApp->GetCamParams() + camera);
}

int CamLDParamIndex(CameraParameters *camParam)
{
  if (camParam->GIF)
    return 1;
  if (camParam->STEMcamera)
    return 2;
  return 0;
}

// Constrain a window position to be on the desktop, first by shifting it then by
// squeezing it as necessary
void ConstrainWindowPlacement(int *left, int *top, int *right, int *bottom, bool scale)
{
  static bool firstTime = true;
  static int deskWidth, deskHeight, deskLeft, deskTop;
  float ratio;
  if (firstTime) {
    deskWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    deskHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    deskLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    deskTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    firstTime = false;
  }
  if (*right == NO_PLACEMENT)
    return;
  if (scale && sWinApp->GetLastSystemDPI()) {
    ratio = (float)sWinApp->GetSystemDPI() / (float)sWinApp->GetLastSystemDPI();
    *right = B3DNINT(*right * ratio);
    *bottom = B3DNINT(*bottom * ratio);
    *left = B3DNINT(*left * ratio);
    *top = B3DNINT(*top * ratio);
  }

  // Shift to left if right side is past edge
  if (*right > deskLeft + deskWidth) {
    *left -= *right - (deskLeft + deskWidth);
    *right = deskLeft + deskWidth;
  }

  // If left side is (now) past edge, shift to right but constrain right edge
  if (*left < deskLeft) {
    *right = B3DMIN(deskLeft + deskWidth, *right + deskLeft - *left);
    *left = deskLeft;
  }

  if (*bottom > deskTop + deskHeight) {
    *top -= *bottom - (deskTop + deskHeight);
    *bottom = deskTop + deskHeight;
  }
  if (*top < deskTop) {
    *bottom = B3DMIN(deskTop + deskHeight, *bottom + deskTop - *top);
    *top = deskTop;
  }
}

// Constrain window to be on a desktop in the given placement
void ConstrainWindowPlacement(WINDOWPLACEMENT *place, bool scale)
{
  ConstrainWindowPlacement((int *)&place->rcNormalPosition.left, 
    (int *)&place->rcNormalPosition.top, (int *)&place->rcNormalPosition.right, 
    (int *)&place->rcNormalPosition.bottom, scale);
}

/*
 * Evaluation needed for any frame alignmnet, base on code from alignframes.ccp in IMOD
 */
float UtilEvaluateGpuCapability(int nx, int ny, int dataSize, bool gainNorm, bool defects,
  int sumBinning, FrameAliParams &faParam, int numAllVsAll, int numAliFrames, 
  int refineIter, int groupSize, int numFilt, int doSpline, double gpuMemory, 
  double maxMemory, int &gpuFlags, int &deferGpuSum, bool &gettingFRC)
{
  float fullPadSize, sumPadSize, alignPadSize;
  float needForGpuSum, needForGpuAli;

  // 500 MB can be used with a large monitor, allow one more float array worth of space as
  // overhead to represent possible difficulty of finding contiguous memory for big FFTs
  float gpuUnusable = 6.e8f + (float)(4. * nx * ny);
  float totAliMem = 0., needed = 0., temp, gpuUsableMem, gpuFracMem = 0.85f;
  float fullTaperFrac = 0.02f;
  bool bdum, sumWithAlign;
  bool doTrunc = (faParam.truncate && faParam.truncLimit > 0);
  bool needPreprocess = gainNorm || defects || doTrunc;
  int ind, numHoldFull, fullDataSize = sizeof(float);
  int aliBinning = UtilGetFrameAlignBinning(faParam, nx, ny);

  // Get memory for components then evaluate the GPU needs
  FrameAlign::getPadSizesBytes(nx, ny, fullTaperFrac, sumBinning, aliBinning, 
    fullPadSize, sumPadSize, alignPadSize);
  gettingFRC = sWinApp->mCamera->GetNumFrameAliLogLines() > 2;

  if (gpuMemory > 0) {
    FrameAlign::gpuMemoryNeeds(fullPadSize, sumPadSize, alignPadSize, numAllVsAll, 
      numAliFrames, refineIter, groupSize, needForGpuSum, needForGpuAli);
    gpuUnusable = B3DMAX(gpuUnusable, (float)gpuMemory * (1.f - gpuFracMem));
    gpuUsableMem = (float)gpuMemory - gpuUnusable;
    sumWithAlign = (faParam.hybridShifts || numFilt == 1) && !doSpline && !refineIter;

    // Summing is top priority
    if (needForGpuSum < gpuUsableMem) {
      needed = needForGpuSum;
      gpuFlags = GPU_FOR_SUMMING;
    } else {
      PrintfToLog("Insufficient memory on GPU to use it for summing (%.0f MB needed "
        "of %.0f MB total)", needForGpuSum / 1.048e6, gpuMemory / 1.048e6);
    }

    // Call this just to get number of frames that need to be held
    FrameAlign::totalMemoryNeeds(fullPadSize, sizeof(float), sumPadSize, alignPadSize, 
          numAllVsAll, numAliFrames, refineIter, 1, numFilt, faParam.hybridShifts, 
          groupSize, doSpline, GPU_FOR_ALIGNING, 0, 0, -1, bdum, numHoldFull);

    // If summing is supposed to be done with align and alignment would fit but both
    // would not, get total memory needs and make sure THAT fits
    if (sumWithAlign && needForGpuAli < gpuUsableMem && needForGpuAli + needed >
      gpuUsableMem) {
        totAliMem =  FrameAlign::totalMemoryNeeds(fullPadSize, sizeof(float), sumPadSize, 
          alignPadSize, numAllVsAll, numAliFrames, refineIter, 1, numFilt, 
          faParam.hybridShifts, groupSize, doSpline, GPU_FOR_ALIGNING, 1, 0, -1, bdum,
          numHoldFull);
        if (totAliMem < maxMemory) {
          sumWithAlign = false;
          deferGpuSum = 1;
        }
    }

    // If summing is done with aligning add the usage for summing if any
    if (sumWithAlign)
      needForGpuAli += needed;

    // Decide if alignment can be done there
    if (needForGpuAli > gpuUsableMem) {
      PrintfToLog("Insufficient memory on GPU to do alignment (%.0f MB needed"
        " of %.0f MB total)", needForGpuAli / 1.048e6, gpuMemory / 1.048e6);
    } else {
      if (sumWithAlign)
        needed = needForGpuAli;
      gpuFlags |= GPU_FOR_ALIGNING;
    }

    // Now if summing, see if there is room for even/odd
    if (gettingFRC && (gpuFlags & GPU_FOR_SUMMING)) {
      if (needed + sumPadSize > gpuUsableMem) {
        gettingFRC = false;
      } else {
        gpuFlags |= GPU_DO_EVEN_ODD;
      }
    }

    if (gpuFlags) {
      temp = needed;
      needed += FrameAlign::findPreprocPadGpuFlags(nx, ny, dataSize, 
        aliBinning, gainNorm, defects, doTrunc, 
        B3DMAX(numHoldFull, 1), gpuUsableMem - needed, 
        0.5f * (1.f - gpuFracMem) * gpuUsableMem, gpuFlags, gpuFlags);
      SEMTrace('a', "GPU mem total %.1f  usable %.1f  need ali/sum %.1f  initial %.1f  "
        "total %.1f", gpuMemory/1.e6, gpuUsableMem/1.e6, temp/1.e6, (needed - temp)/1.e6,
        needed/1.e6);
      ind = (gpuFlags >> GPU_STACK_LIM_SHIFT) & GPU_STACK_LIM_MASK;
      if (gpuFlags & (GPU_DO_NOISE_TAPER | GPU_DO_BIN_PAD))
        SEMTrace('a', "%s  %s  %s  %s %s %d on GPU", (gpuFlags & GPU_DO_NOISE_TAPER) ?
        "noise-pad" : "", (gpuFlags & GPU_DO_BIN_PAD) ? "bin-pad" : "",
        (gpuFlags & GPU_DO_PREPROCESS) ? "preprocess" : "",
        (gpuFlags & STACK_FULL_ON_GPU) ? "stack" : "",
        (gpuFlags & GPU_STACK_LIMITED) ? "limit" : "   ",
        (gpuFlags & GPU_STACK_LIMITED) ? ind : 0);

      // Can stack smaller size if doing either operation on GPU and either there is
      // no preprocess or preprocessing is on GPU
      if ((gpuFlags & (GPU_DO_BIN_PAD | GPU_DO_NOISE_TAPER)) &&
        (!needPreprocess || (gpuFlags & GPU_DO_PREPROCESS)))
        fullDataSize = dataSize;
    }

  }
  return FrameAlign::totalMemoryNeeds(fullPadSize, fullDataSize, sumPadSize, alignPadSize,
    numAllVsAll, numAliFrames, refineIter, 1, numFilt, faParam.hybridShifts, groupSize, 
    doSpline, gpuFlags, deferGpuSum, 0, -1, bdum, numHoldFull);
}

// Return the alignment binning for the given parameters and frame size
int UtilGetFrameAlignBinning(FrameAliParams &param, int frameSizeX, 
  int frameSizeY)
{
  int superFac = 1;
  if (!param.binToTarget || param.targetSize < 100) {
    if (param.multAliBinByEERfac)
      superFac = sWinApp->mFalconHelper->GetEERsuperFactor(param.EERsuperRes);
    return param.aliBinning * superFac;
  }
  return B3DNINT(sqrt((double)frameSizeX * frameSizeY) / param.targetSize);
}

// Returns FALSE when the mutex could not be acquired or created
BOOL AdocAcquireMutex()
{
  // This wait is ridiculously long since the mutex is not held while reading or
  // writing image data
  int wait = ADOC_MUTEX_WAIT;
  if (sWinApp->mBufferManager->GetDoingAsyncSave())
    wait = B3DMAX(wait, sWinApp->mBufferManager->GetAsyncTimeout());
  if (!sAutodocMutex)
    sAutodocMutex = CreateMutex(0, 0, 0);
  if (!sAutodocMutex)
    return FALSE;
  if (WaitForSingleObject(sAutodocMutex, ADOC_MUTEX_WAIT) == WAIT_TIMEOUT) {
    SEMTrace('y', "Failed to acquire adoc access mutex for thread %d", 
      GetCurrentThreadId());
    return FALSE;
  }
  if (GetDebugOutput('*'))
    SEMTrace('y', "Acquired adoc access mutex for thread %d", GetCurrentThreadId());
  return TRUE;
}

// Returns -2 if mutex could not be acquired, or -1 if current index can't be set
int AdocGetMutexSetCurrent(int index)
{
  if (!AdocAcquireMutex())
    return -2;
  if (AdocSetCurrent(index) < 0) {
    SEMTrace('y', "Failed to set adoc index to %d", index);
    AdocReleaseMutex();
    return -1;
  }
  return 0;
}

void AdocReleaseMutex()
{
  if (!sAutodocMutex)
    return;
  ReleaseMutex(sAutodocMutex);
  if (GetDebugOutput('*'))
    SEMTrace('y', "Released adoc access mutex for thread %d", GetCurrentThreadId());
}

// Computes a new spinner value and returns true if it is out of range, sets pResult
// appropriately
bool NewSpinnerValue(NMHDR *pNMHDR, LRESULT *pResult, int oldVal, int lowerLim,
  int upperLim, int &newVal)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  int newTmp = oldVal + pNMUpDown->iDelta;
  if (newTmp < lowerLim || newTmp > upperLim) {
    if (oldVal < lowerLim) {
      newTmp = lowerLim;
    } else if (oldVal > upperLim) {
      newTmp = upperLim;
    } else {
      *pResult = 1;
      return true;
    }
  }
  newVal = newTmp;
  *pResult = 0;
  return false;
}

// Variant for the case where the spinner holds the correct value
bool NewSpinnerValue(NMHDR *pNMHDR, LRESULT *pResult, int lowerLim, int upperLim,
  int &newVal)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  return NewSpinnerValue(pNMHDR, pResult, pNMUpDown->iPos, lowerLim, upperLim, newVal);
}

// From: http://dotnetbutchering.blogspot.com/2008/03/vc-mfc-how-to-set-combobox-dropdown.html
void SetDropDownHeight(CComboBox* pMyComboBox, int itemsToShow)
{
  //Get rectangles
  CRect rctComboBox, rctDropDown;
  pMyComboBox->GetClientRect(&rctComboBox); //Combo rect
  pMyComboBox->GetDroppedControlRect(&rctDropDown);  //DropDownList rect  
  int itemHeight = pMyComboBox->GetItemHeight(-1); //Get Item height
  pMyComboBox->GetParent()->ScreenToClient(&rctDropDown); //Converts coordinates
  rctDropDown.bottom = rctDropDown.top + rctComboBox.Height() + itemHeight*itemsToShow; //Set height
  pMyComboBox->MoveWindow(&rctDropDown); //enable changes
}

// Loads macro names in to a combo box, using the long names if present if useLong is
// true and putting None as first entry if addNone true
void LoadMacrosIntoDropDown(CComboBox &combo, bool useLong, bool addNone)
{
  CString str;
  CString *names = sWinApp->GetMacroNames();
  CString *longNames = sWinApp->GetLongMacroNames();
  if (addNone)
    combo.AddString("None");
  for (int ind = 0; ind < MAX_MACROS; ind++) {
    if (useLong && !longNames[ind].IsEmpty())
      str = longNames[ind];
    else if (names[ind].IsEmpty())
      str.Format("# %d", ind + 1);
    else
      str = names[ind];
    combo.AddString((LPCTSTR)str);
  }
  SetDropDownHeight(&combo, B3DMIN(MAX_DROPDOWN_TO_SHOW, MAX_MACROS + (addNone ? 1 : 0)));
}

// Modify a menu item given its submenu # and ID
void UtilModifyMenuItem(const char *popupName, UINT itemID, const char *newText)
{
  CMenu *menu, *mainMenu;
  CString name;
  mainMenu = sWinApp->m_pMainWnd->GetMenu();
  for (int ind = 0; ind < (int)mainMenu->GetMenuItemCount(); ind++) {
    UtilGetMenuString(mainMenu, ind, name, MF_BYPOSITION);
    if (!name.Compare(popupName)) {
      menu = mainMenu->GetSubMenu(ind);
      menu->ModifyMenu(itemID, MF_BYCOMMAND | MF_STRING, itemID, newText);
      sWinApp->m_pMainWnd->DrawMenuBar();
      return;
    }
  }
}

// Get a menu string and strip the accelerator
void UtilGetMenuString(CMenu *menu, int position, CString &name, UINT nFlags)
{
  menu->GetMenuString(position, name, nFlags);
  name.Replace("&&", "|");
  name.Replace("&", "");
  name.Replace("|", "&");
}

bool UtilCamRadiosNeedSmallFont(CButton *radio) 
{
  CameraParameters *camParam = sWinApp->GetCamParams();
  int *activeList = sWinApp->GetActiveCameraList();
  CRect butrect;
  radio->GetWindowRect(&butrect);
  CClientDC dc(radio);
  dc.SelectObject(radio->GetFont());
  for (int ind = 0; ind < sWinApp->GetNumActiveCameras(); ind++) {
    CSize size = dc.GetTextExtent(camParam[activeList[ind]].name);
    if (size.cx >= butrect.Width() - (20 * sWinApp->GetSystemDPI()) / 120)
      return true;
  }
  return false;
}

// Given any angle, returns a value between -180 and 180.
double UtilGoodAngle(double angle)
{
  while (angle <= -180.)
    angle += 360.;
  while (angle > 180.)
    angle -= 360.;
  return angle;
}

// Finds a frame alignment parameter that fits the given restriction on size and alignment
// Return value is 0 if originally selected one or just one other one is valid, 1-3 if
// none are valid, and -1 to -3 if new one is one of several that are valid
int UtilFindValidFrameAliParams(CameraParameters *camParam, int readMode, 
  bool takeK3Binned, int whereAlign, int curIndex, int &newIndex, CString *message)
{
  CString str;
  int ind, sizeRestriction, retval = 0, numValid = 0, firstValid = -1;
  CArray<FrameAliParams, FrameAliParams> *params = sWinApp->mCamera->GetFrameAliParams();
  FrameAliParams *paramData = params->GetData();
  FrameAliParams *faParam = &paramData[curIndex];
  newIndex = curIndex;
  if (!whereAlign || curIndex < 0 || curIndex >= (int)params->GetSize())
    return 0;

  // The passed in boolean must combine whether the set is gain normalized, the setting of 
  // taking K3 binned, and whether frames are to be saved unnormalized anyway
  if (camParam->K2Type == K3_TYPE && readMode != LINEAR_MODE)
    readMode = B3DCHOICE(takeK3Binned, COUNTING_MODE, SUPERRES_MODE);

  // First check, is the selected on valid with its restrictions
  sizeRestriction = camParam->canTakeFrames ? 0 : faParam->sizeRestriction;
  if (sizeRestriction && sWinApp->GetAnySuperResMode() && 
    !BOOL_EQUIV(readMode != SUPERRES_MODE, sizeRestriction != SUPERRES_MODE)) {
      retval += 1;
      if (message) {
        str.Format("The camera parameters will give %sresolution frames but\r\n"
          "the selected alignment parameters are marked as only for %sresolution frames.", 
          readMode == SUPERRES_MODE ? "super-" : "normal ",
          sizeRestriction == SUPERRES_MODE ? "super-" : "normal ");
        *message += str;
      }
  }
  if (faParam->whereRestriction && 
    !BOOL_EQUIV(whereAlign < 2, faParam->whereRestriction < 2)) {
      retval += 2;
      if (message) {
        if (!(*message).IsEmpty())
          *message += "\r\n\r\n";
        str.Format("The alignment is set to be done %s but the selected\r\n"
          "alignment parameters are marked as only for alignment %s.", 
          whereAlign < 2 ? "in the plugin" : "with IMOD",
          faParam->whereRestriction < 2 ? "in the plugin" : "with IMOD");
        *message += str;
      }
  }
  if (!retval)
    return 0;

  for (ind = 0; ind < (int)params->GetSize(); ind++) {
    faParam = &paramData[ind];
    sizeRestriction = camParam->canTakeFrames ? 0 : faParam->sizeRestriction;
    if ((!sizeRestriction || !sWinApp->GetAnySuperResMode() ||
      BOOL_EQUIV(readMode != SUPERRES_MODE, sizeRestriction != SUPERRES_MODE))
      && (!faParam->whereRestriction || 
      BOOL_EQUIV(whereAlign < 2, faParam->whereRestriction < 2))) {
        numValid++;
        if (firstValid < 0)
          firstValid = ind;
    }
  }

  // Return 1 with current selection if nothing else is valid, or first valid one with
  // 0 if it is the only one or -1 if it is just the first
  if (firstValid < 0) {
    if (message)
      *message += "\r\n\r\nThere are no other alignment parameter sets that fit "
      "these conditions.";
    return retval;
  }
  newIndex = firstValid;
  if (numValid > 1 && message)
    *message += "\r\n\r\nThere are several other alignment parameter sets that fit these"
        " conditions.";
  else if (message)
    *message += "\r\n\r\nThere is one other alignment parameter set that fits these"
        " conditions.";
  return numValid > 1 ? -retval : 0;
}

// Writes text in a string to a text file, returning 1 on error opening or 2 on error 
// writing
int UtilWriteTextFile(CString fileName, CString text)
{
  int retval = 1;
  CStdioFile *cFile = NULL;
  try {
    cFile = new CStdioFile(fileName, CFile::modeCreate | 
      CFile::modeWrite |CFile::shareDenyWrite);
    retval = 2;
    cFile->WriteString(text);
    cFile->Close();
    retval = 0;
  }
  catch(CFileException *perr) {
    perr->Delete();
  } 
  if (cFile)
    delete cFile;
  return retval;
}

// Formats a number with a range of decimal places to achieve a desired precision
// Returns a string with the value followed by the suffix.  minDec and maxDec are
// the minimum and maximum number of decimal places, and switchVal is a number between 1
// and 10 where it will switch between the number of decimal places
CString FormattedNumber(double value, const char *suffix, int minDec, int maxDec,
  float switchVal, bool skipTrim)
{
  CString format, retStr;
  int ndec = maxDec;
  double maxTen = switchVal * pow(10., maxDec);
  if (fabs(value) > 1. / maxTen)
    ndec = (int)(log10(floor(maxTen / fabs(value))));
  B3DCLAMP(ndec, minDec, maxDec);
  format.Format("%%.%df", ndec);
  retStr.Format(format, value);
  if (ndec > 0 && !skipTrim)
    retStr = retStr.TrimRight('0');
  retStr = retStr.TrimRight('.');
  return retStr + suffix;
}

// Sleep for specified, processing messages
BOOL SleepMsg(DWORD dwTime_ms)
{
  DWORD dwStart = GetTickCount();
  DWORD dwElapsed;
  while ((dwElapsed = (DWORD)SEMTickInterval(dwStart)) < dwTime_ms) {
    DWORD dwStatus = MsgWaitForMultipleObjects(0, NULL, FALSE,
      dwTime_ms - dwElapsed, QS_ALLINPUT);

    if (dwStatus == WAIT_OBJECT_0) {
      MSG msg;
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          PostQuitMessage((int)msg.wParam);
          return FALSE; // abandoned due to WM_QUIT
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  return TRUE;
}

// Establishes socket communication at the given address and port
// Modified from HitachiPlugin
SOCKET UtilConnectSocket(CString &ipAddress, int port, CString &errStr, 
  const char *className, const char *description)
{
  SOCKET connectSocket;

  // holds address info for socket to connect to
  struct addrinfo *result = NULL,
    *ptr = NULL,
    hints;
  char portBuf[32];

  // Initialize Winsock through SerialEM
  int iResult = SEMInitializeWinsock();

  if (iResult != 0) {
    errStr = "Failed to initialize winsock from " + CString(className);
    return INVALID_SOCKET;
  }
  errStr = "";

  // set address info
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;	//AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;  //TCP connection!!!	IPPROTO_IPV6;//

                                    //resolve server address and port
  snprintf(portBuf, 32, "%d", port);
  iResult = getaddrinfo((LPCTSTR)ipAddress, portBuf, &hints, &result);

  if (iResult != 0) {
    errStr.Format("%s: getaddrinfo failed with error: %d\n", className,
      iResult);
    return INVALID_SOCKET;
  }

  // "Attempt to connect to an address until one succeeds" is what it said but not what
  // it did
  for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
    if (ptr->ai_protocol != IPPROTO_TCP)
      continue;

    // Create a SOCKET for connecting to server
    connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (connectSocket == INVALID_SOCKET) {
      errStr.Format("%s: socket() failed with error: %ld\n", className,
        WSAGetLastError());
      continue;
    }

    // Connect to server.
    iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

    if (iResult == SOCKET_ERROR) {
      errStr.Format("The connection to the %s server failed"
        " with error %d", description, WSAGetLastError());
      closesocket(connectSocket);
      connectSocket = INVALID_SOCKET;
    } else
      break;
  }

  // no longer need address info for server
  freeaddrinfo(result);
  if (connectSocket == INVALID_SOCKET) {
    if (errStr.IsEmpty())
      errStr.Format("%s: getaddrinto failed to return a TCP connection", className);
    return INVALID_SOCKET;
  }

  // Set the mode of the socket to be nonblocking
  u_long iMode = 1;

  iResult = ioctlsocket(connectSocket, FIONBIO, &iMode);
  if (iResult == SOCKET_ERROR) {
    errStr.Format("%s: ioctlsocket failed with error: %d\n", className,
      WSAGetLastError());
    closesocket(connectSocket);
    connectSocket = INVALID_SOCKET;
    return INVALID_SOCKET;
  }

  //disable nagle
  char value = 1;
  setsockopt(connectSocket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
  return connectSocket;
}

