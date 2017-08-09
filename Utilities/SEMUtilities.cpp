#include "stdafx.h"
#include "SEMUtilities.h"
#include "..\SerialEM.h"
#include "..\EMscope.h"
#include "..\CameraController.h"
#include "..\EMBufferManager.h"
#include "..\Shared\SEMCCDDefines.h"
#include "..\Shared\autodoc.h"


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
   FRAMEALI_BAD_SUBSET, 0};

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
   "Subset parameters do not define a subset of at least 2 frames"
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
    lowDose = sWinApp->LowDoseMode();
  if (lowDose && param->useViewInLowDose)
    set = VIEW_CONSET;
  else if (lowDose && param->useSearchInLowDose && ldp[SEARCH_AREA].magIndex)
    set = SEARCH_CONSET;
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

// Clean up a thread after an error: suspend and delete thread
void UtilThreadCleanup(CWinThread **threadpp)
{
  if (*threadpp) {
    (*threadpp)->SuspendThread();
    delete *threadpp;
    *threadpp = NULL;
  }
}

// Open an existing file and return a KStoreMRC
KStoreMRC *UtilOpenOldMRCFile(CString filename)
{
  CFile *file;
  KStoreMRC *storeMRC = NULL;

  try {
    file = new CFile(filename, CFile::modeReadWrite |CFile::shareDenyWrite);
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
  KStoreMRC *storeMRC = UtilOpenOldMRCFile(filename);
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

// Remove a file and ignore errors
void UtilRemoveFile(CString filename)
{
  CFileStatus status;
  try {
    if (CFile::GetStatus((LPCTSTR)filename, status))
      CFile::Remove((LPCTSTR)filename);
  }
  catch (CFileException *cerr) {
    delete cerr;
  }
}

// Rename a file, removing an existing copy first, with stock or supplied error message
int UtilRenameFile(CString fromName, CString toName, const char *message)
{
  CFileStatus status;
  CString mess;
  try {
    if (CFile::GetStatus((LPCTSTR)toName, status))
      CFile::Remove(toName);
    CFile::Rename(fromName, toName);
  }
  catch (CFileException *err) {
    err->Delete();
    if (message)
      mess = message;
    else
      mess = "Error attempting to rename " + fromName + " to " + toName;
    SEMMessageBox(mess, MB_EXCLAME);
    return 1;
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

// Append a component to a string with the given separator if the string is not empty
void UtilAppendWithSeparator(CString &filename, CString toAdd, const char *sep)
{
  if (!filename.IsEmpty())
    filename += CString(sep);
  filename += toAdd;
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
  int i, lastMag, thisMag, limlo = 1, limhi = MAX_MAGS - 1;
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
  lastMag = MagOrEFTEMmag(camParams->GIF, limhi, ifSTEM);
  for (i = limhi; i >= limlo; i--) {
    if (!ifSTEM && secondary && scope->GetSecondaryMode() && i == secondary - 1)
      i = scope->GetLowestMModeMagInd() - 1;
    thisMag = MagOrEFTEMmag(camParams->GIF, i, ifSTEM);
    if (fabs((double)magval - thisMag) < 1. || fabs((double)magval - thisMag) < 
      0.35 * fabs((double)lastMag - thisMag))
      return i;
    if (fabs((double)magval - lastMag) < 0.35 * fabs((double)lastMag - thisMag))
      return i + 1;
    lastMag = thisMag;
  }
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
void ConstrainWindowPlacement(int *left, int *top, int *right, int *bottom)
{
  static bool firstTime = true;
  static int deskWidth, deskHeight, deskLeft, deskTop;
  if (firstTime) {
    deskWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    deskHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    deskLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    deskTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    firstTime = false;
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
void ConstrainWindowPlacement(WINDOWPLACEMENT *place)
{
  ConstrainWindowPlacement((int *)&place->rcNormalPosition.left, 
    (int *)&place->rcNormalPosition.top, (int *)&place->rcNormalPosition.right, 
    (int *)&place->rcNormalPosition.bottom);
}

/*
 * Routines from alignframes.ccp in IMOD
 */
// Return the bytes needed for full, sum, and align images
void UtilGetPadSizesBytes(int nx, int ny, float fullTaperFrac, int sumBin, int alignBin,
                      float &fullPadSize, float &sumPadSize, float &alignPadSize)
{
  fullPadSize = (float)(4. * (1. + 2. * fullTaperFrac) * (1. + 2. * fullTaperFrac) * 
    nx * ny);
  sumPadSize = (float)(fullPadSize / (sumBin * sumBin));
  alignPadSize = (float)(4. * nx * ny / ((float)alignBin * alignBin));
}

// Return the memory needed for aligning and summing on the GPU based on the conditions
void UtilGpuMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize,
                    int numAllVsAll, int nzAlign, int refineAtEnd, int groupSize,
                    float &needForGpuSum, float &needForGpuAli)
{
  int numHoldAlign;

  // The FFTs require as much memory as the image, so allow for the biggest
  needForGpuSum = fullPadSize + sumPadSize + B3DMAX(fullPadSize, sumPadSize);

  // Get needs for aligning on GPU
  // Simple cumulative sum needs all 5 arrays and FFT work area
  // Refining needs all arrays too plus one for each frame
  // Otherwise only 3 arrays and FFT and number of rolling frames
  if (!numAllVsAll && !refineAtEnd)
    numHoldAlign = 6;
  else if (refineAtEnd)
    numHoldAlign = 6 + nzAlign;
  else
    numHoldAlign = 4 + B3DMIN(numAllVsAll, nzAlign);
  if (groupSize > 1) {

    // If doing groups, refinement requires space for the group sums, and
    // no refinement requires space for groupSize single frames
    if (refineAtEnd)
      numHoldAlign += B3DMIN(numAllVsAll, nzAlign);
    else
      numHoldAlign += groupSize;
  }
  needForGpuAli = numHoldAlign * alignPadSize;
}

// Return the computer memory in GB needed based on the conditions
float UtilTotalMemoryNeeds(float fullPadSize, float sumPadSize, float alignPadSize,
                       int numAllVsAll, int nzAlign, int refineAtEnd, int numBinTests,
                       int numFiltTests, int hybridShifts, int groupSize, int doSpline,
                       int gpuFlags, int deferSum, int testMode, int startAssess,
                       bool &sumInOnePass)
{
  int numHoldFull, numHoldAlign;
  float memTot;

  // Make sums as you go if there is one binning and no subset assessment
  sumInOnePass = numBinTests == 1 && !testMode && startAssess < 0;
  numHoldFull = numHoldAlign = B3DMIN(numAllVsAll, nzAlign);
  if (numAllVsAll) {
    if (refineAtEnd)
      numHoldAlign = nzAlign;
    if (groupSize > 1 && refineAtEnd)
      numHoldAlign += numAllVsAll;
    else if (groupSize > 1)
      numHoldAlign += groupSize;
  } else if (refineAtEnd) {
    numHoldAlign = nzAlign;
  }
  if (gpuFlags & GPU_FOR_ALIGNING)
    numHoldAlign = 0;
  if ((!hybridShifts && numFiltTests > 1 && sumInOnePass && numAllVsAll) ||
      refineAtEnd || deferSum || doSpline)
    numHoldFull = nzAlign;
  if ((numBinTests > 1 || testMode) && numAllVsAll && startAssess < 0)
    numHoldFull = 0;
  memTot = (float)((2. * sumPadSize + (numHoldAlign + 4.) * alignPadSize + 
    (numHoldFull + 1.) * fullPadSize) / (1024. * 1024. * 1024.));
  return memTot;
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
int UtilFindValidFrameAliParams(int readMode, int whereAlign, int curIndex,
  int &newIndex, CString *message)
{
  CString str;
  int ind, retval = 0, numValid = 0, firstValid = -1;
  CArray<FrameAliParams, FrameAliParams> *params = sWinApp->mCamera->GetFrameAliParams();
  FrameAliParams *paramData = params->GetData();
  FrameAliParams *faParam = &paramData[curIndex];
  newIndex = curIndex;
  if (!whereAlign || curIndex < 0 || curIndex >= (int)params->GetSize())
    return 0;

  // First check, is the selected on valid with its restrictions
  if (faParam->sizeRestriction && 
    !BOOL_EQUIV(readMode != SUPERRES_MODE, faParam->sizeRestriction != SUPERRES_MODE)) {
      retval += 1;
      if (message) {
        str.Format("The camera parameters are for %s images but the selected\r\n"
          "alignment parameters are marked as only for %s images.", 
          readMode == SUPERRES_MODE ? "8K" : "4K",
          faParam->sizeRestriction == SUPERRES_MODE ? "8K" : "4K");
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
    if ((!faParam->sizeRestriction || 
      BOOL_EQUIV(readMode != SUPERRES_MODE, faParam->sizeRestriction != SUPERRES_MODE))
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
