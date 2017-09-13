// FalconHelper.cpp:  Routines for intermediate frame saving from Falcon camera
//
// Copyright (C) 2013 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <direct.h>
#include <map>
#include "SerialEM.h"
#include "EMscope.h"
#include "SerialEMDoc.h"
#include "FalconHelper.h"
#include "CameraController.h"
#include "ShiftManager.h"
#include "PluginManager.h"
#include "Utilities\XCorr.h"
#include "Shared\b3dutil.h"
#include "Shared\framealign.h"
#include "Shared\SEMCCDDefines.h"

enum {FIF_COMM_ERR = 1, FIF_NO_FILES, FIF_NO_NEW_FILES, FIF_OPEN_OLD, FIF_READ_ERR,
FIF_BAD_SIZE, FIF_MEMORY, FIF_OPEN_NEW, FIF_MISMATCH, FIF_WRITE_ERR, FIF_REMOVE_ERR, 
FIF_NO_MORE_FILES, FIF_BAD_MODE, FIF_CONF_NOT_EXIST, FIF_CONF_OPEN_ERR, 
FIF_CONF_READ_ERR, FIF_CONF_NO_MODE_LINE, FIF_CONF_NOT_INIT, FIF_CONF_BAD_MODE, 
FIF_CONF_WRITE_ERR, FIF_BACKUP_EXISTS, FIF_BACKUP_ERR, FIF_ALIGN_SIZE_ERR, 
FIF_ALIGN_FRAME_ERR, FIF_FINISH_ALIGN_ERR, FIF_LAST_CODE};

static const char *errMess[] = {"Unspecified communication error", 
"No intermediate frames files found in directory",
"No new intermediate frame files found with name in expected format", 
"Error opening intermediate frame file", "Error reading intermediate frame file",
"Cannot find appropriate size information for intermediate frame file",
"Error allocating memory", "Error opening a new MRC file", 
"Sizes or modes do not all match between intermediate frame files", 
"Error writing image to stack file", 
"Error removing the intermediate frame file after writing it successfully to stack",
"There are no more files to read", "The frame files have an unrecognized data mode",
"FalconConfig file does not exist or is in unreadable directory",
"FalconConfig file cannot be opened for reading", "Error reading from FalconConfig file",
"Could not recognize the acquisition mode line in the FalconConfig file",
"Falcon configuration file access not set up: if FEI-SEMServer was restarted, SerialEM "
"needs to be restarted", "No recognized acquisition mode in the FalconConfig file",
"Error rewriting the FalconConfig file",
"The stack file and its backup already exist", 
"Error renaming existing stack file to backup name",
"Frames are not the same size as the image returned to SerialEM and will not be aligned",
"Error aligning a frame in the nextFrame routine"
"Error finishing the frame alignment"
};

static void framePrintFunc(const char *strMessage)
{
  CString str = strMessage;
  str.TrimRight('\n');
  str.Replace("\n", "\r\n");
  SEMTrace('E', "Framealign : %s", (LPCTSTR)str);
}

CFalconHelper::CFalconHelper(void)
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mNumStartLines = -1;
  mConfigBackedUp = false;
  mLastSavedDummy = false;
  mRotData = NULL;
  mStoreMRC = NULL;
  mImage = NULL;
  mImage2 = NULL;
  mFrameAli = NULL;
  mWaitInterval = 70;
  mWaitMaxCount = 10;
  mStackingFrames = false;
  mUseGpuForAlign[0] = mUseGpuForAlign[1] = 0;
}

CFalconHelper::~CFalconHelper(void)
{
  delete mFrameAli;
}

// Read the configuration file and save its essential features
void CFalconHelper::Initialize(bool skipConfigs)
{
  CString configFile, str, dir, fname;
  int error = 0;
  long version;
  mCamera = mWinApp->mCamera;
  mPlugFuncs = mWinApp->mScope->GetPlugFuncs();
  if (!mPlugFuncs)
    return;
  mReadoutInterval = mCamera->GetFalconReadoutInterval();
  mFrameAli = new FrameAlign();
  mFrameAli->setPrintFunc(framePrintFunc);
  if (!mFrameAli->gpuAvailable(0, &mGpuMemory, GetDebugOutput('E')))
    mGpuMemory = 0;
  configFile = mCamera->GetFalconFrameConfig();
  if (configFile.IsEmpty() || skipConfigs)
    return;
  error = mPlugFuncs->FIFinitialize((LPCTSTR)configFile, mReadoutInterval);
  if (error) {
    mWinApp->AppendToLog("Error opening or reading from intermediate frame file " +
      configFile);
    mCamera->SetMaxFalconFrames(0);
  }
  if (mWinApp->mScope->GetPluginVersion() < FEI_PLUGIN_MANAGE_FAL_CONF) {
    mCamera->SetCanUseFalconConfig(-1);
    return;
  }

  // If supported, try to get a name for FalconConfig file and initialize its use
  str = mCamera->GetFalconConfigFile();
  if (str.IsEmpty()) {
    UtilSplitPath(configFile, dir, fname);
    str = dir + "\\FalconConfig.xml";
  }
  error = mPlugFuncs->FIFinitFalconConfig((LPCTSTR)str, &version);
  if (error) {
    mWinApp->AppendToLog("Error initializing access to Falcon configuration file:\r\n" +
      str + "\r\n" + GetErrorString(error));
    mCamera->SetCanUseFalconConfig(-1);
  } else if (mCamera->GetCanUseFalconConfig() < 0) {
    mCamera->SetCanUseFalconConfig(version);
  }
}

// Check the frame saving state in FalconConfig and optionally set it
// Output error message if message is non-NULL
int CFalconHelper::CheckFalconConfig(int setState, int &state, const char *message)
{
  long lstate;
  int error = mPlugFuncs->FIFcheckFalconConfig(setState, &lstate);
  if (error && message)
    SEMMessageBox(GetErrorString(error) + CString("\n\n") + message);
  state = lstate;
  if (!error)
    mCamera->SetFrameSavingEnabled(state > 0);
  return error;
}

// Modify the configuration file to save the desired frames, or put out a dummy file
// if no frame-saving is selected
int CFalconHelper::SetupConfigFile(ControlSet &conSet, CString localPath, 
  CString &directory, CString &filename, CString &configFile, BOOL stackingDeferred, 
  CameraParameters *camParams, long &numFrames)
{
  CFileStatus status;
  CString str;
  std::vector<long> readouts;
  long temp = 1;
  long *readPtr = &temp;
  float frameInterval = mCamera->GetFalconReadoutInterval();
  int ind, block;
  bool saveFrames = conSet.saveFrames || (conSet.alignFrames && conSet.useFrameAlign);
  mDoingAdvancedFrames = FCAM_ADVANCED(camParams) != 0;
  if (!mDoingAdvancedFrames) {

    // Check the directory entry and assign local path if needed
    if (directory.IsEmpty()) {
      SEMMessageBox("You must specify a directory for saving the frame images");
      return 1;
    }
    if (localPath.IsEmpty())
      localPath = directory;

    // Create directory if it doesn't exist
    if (saveFrames && !CFile::GetStatus((LPCTSTR)directory, status)) {
      SEMTrace('E', "Directory %s does not exist, creating it", (LPCTSTR)directory);
      if (_mkdir((LPCTSTR)directory)) {
        str.Format("An error occurred creating the directory:\n%s\n"
          "for saving frame images.  Does its parent directory exist?", 
          (LPCTSTR)directory);
        SEMMessageBox(str);
        return 1;
      }
    }
  }

  // Build the list of readouts
  for (block = 0; block < (int)conSet.summedFrameList.size() / 2; block++)
    for (ind = 0; ind < conSet.summedFrameList[block * 2]; ind++)
      readouts.push_back((long)conSet.summedFrameList[block * 2 + 1]);

  numFrames = (int)readouts.size();
  if (numFrames > 0) {
    temp = numFrames;
    readPtr = &readouts[0];
  }
  
  if (mDoingAdvancedFrames) {
    ind = -1;
    //if (camParams->FEItype == FALCON2_TYPE)
      //ind = stackingDeferred ? 1 : 0;
    if (mPlugFuncs->ASIsetupFrames(camParams->eagleIndex, ind, temp, conSet.numSkipBefore,
      readPtr, (LPCTSTR)directory, (LPCTSTR)filename, 0, 0.)) {
      str.Format("Error setting up for frame saving from Falcon:\n%s",
        mPlugFuncs->GetLastErrorString());
      SEMMessageBox(str);
      return 1;
    }

  } else {
    if (mPlugFuncs->FIFsetupConfigFile((LPCTSTR)localPath, stackingDeferred, 
      saveFrames, temp, conSet.numSkipBefore, readPtr)) {
        str.Format("Error opening or writing to intermediate frame configuration file"
          " %s:\n%s", (LPCTSTR)configFile, mPlugFuncs->GetLastErrorString());
        SEMMessageBox(str);
        return 1;
    }
  }

  // Save frame alignment information and set up aligning if doing here
  mUseFrameAlign = 0;
  mJustAlignNotSave = !conSet.saveFrames && conSet.alignFrames && 
    conSet.useFrameAlign == 1;
  if (conSet.alignFrames && conSet.useFrameAlign == 1) {
    mUseFrameAlign = conSet.useFrameAlign;
    mFAparamSetInd = conSet.faParamSetInd;
    return SetupFrameAlignment(conSet, camParams, mGpuMemory, mUseGpuForAlign, numFrames);
  }
  return 0;
}



int CFalconHelper::SetupFrameAlignment(ControlSet &conSet, CameraParameters *camParams, 
  float gpuMemory, int *useGPU, int numFrames)
{
  CString str;
  int nx, ny, numFilters = 0, ind, deferGpuSum = 0;
  float radius2[4], sigma2[4], sigma1 = 0.03f; 
  float fullTaperFrac = 0.02f;
  float trimFrac = fullTaperFrac;
  float taperFrac = 0.1f;
  float kFactor = 4.5f;
  float maxMaxWeight = 0.1f;
  float totAliMem = 0.,cpuMemFrac = 0.75f;
  float maxMemory, memoryLimit;
  int numAliFrames, numAllVsAll, groupSize, refineIter, doSpline, gpuFlags = 0;
  FrameAliParams param;
  CArray<FrameAliParams, FrameAliParams> *faParams = 
    mCamera->GetFrameAliParams();
  if (conSet.faParamSetInd < 0 || conSet.faParamSetInd >= faParams->GetSize()) {
    str.Format("The index of the parameter set chosen for frame alignment, %d, is out "
      "of range", conSet.faParamSetInd);
    SEMMessageBox(str);
    return 1;
  }

  if (conSet.useFrameAlign > 1)
    return 0;

  SEMTrace('E', "Start Frame alignment inialization");
  // get param, size of images, and subset limits if any
  param = faParams->GetAt(conSet.faParamSetInd);
  nx = (conSet.right - conSet.left) / conSet.binning;
  ny = (conSet.bottom - conSet.top) / conSet.binning;
  mAlignSubset = false;
  numAliFrames = numFrames;
  if (param.alignSubset && conSet.saveFrames) {
    mAlignStart = B3DMAX(1, param.subsetStart);
    mAlignEnd = B3DMIN(numFrames, param.subsetEnd);
    if (mAlignEnd - mAlignStart > 0) {
      numAliFrames = mAlignEnd + 1 - mAlignStart;
      mAlignSubset = true;
    }
  }

  // Get some other parameters
  numAllVsAll = mCamera->NumAllVsAllFromFAparam(param, numAliFrames, groupSize, 
    refineIter, doSpline, numFilters, radius2);
  for (ind = 0; ind < numFilters; ind++)
    sigma2[ind] = (float)(0.001 * B3DNINT(1000. * param.sigmaRatio * radius2[ind]));

  // Set a memory limit
  memoryLimit = (float)mWinApp->GetMemoryLimit();
  if (!memoryLimit) {
    memoryLimit = (float)b3dPhysicalMemory();
#ifndef _WIN64
    memoryLimit = B3DMIN(memoryLimit, 3.e9f);
#endif
    memoryLimit *= cpuMemFrac;
  }
  maxMemory = memoryLimit - (float)mWinApp->mBufferWindow.MemorySummary();

  // Evaluate all memory needs. PROBLEM: binned data and alignment binning
  totAliMem = UtilEvaluateGpuCapability(nx, ny, 1,
    param, numAllVsAll, numAliFrames, refineIter, groupSize, numFilters, doSpline, 
    useGPU[0] ? mGpuMemory : 0., maxMemory, gpuFlags, deferGpuSum, mGettingFRC);
  if (totAliMem > maxMemory)
    PrintfToLog("WARNING: With current parameters, memory needed for aligning "
    "(%.1f GB) exceeds allowed\r\n  memory usage (%.1f GB), controlled by MemoryLimit"
    " property if it is set", totAliMem, maxMemory);

  ind = mFrameAli->initialize(1, param.aliBinning, trimFrac, numAllVsAll, refineIter, 
    param.hybridShifts, (deferGpuSum | doSpline) ? 1 : 0, groupSize, nx, ny, 
    fullTaperFrac, taperFrac, param.antialiasType, 0., radius2, sigma1, sigma2, 
    numFilters, param.shiftLimit, kFactor, maxMaxWeight, 0, numFrames, gpuFlags, 
    GetDebugOutput('E') ? 11 : 0);
  if (ind) {
    str.Format("The framealign routine failed to initialize (error %d)", ind);
    mFrameAli->cleanup();
    SEMMessageBox(str);
    return 1;
  }
  SEMTrace('E', "Frame alignment initialized");
  return 0;
}


// Stack the frames, possibly located in localPath, into a file with the given root name
// the given directory, with potential division by a factor, rotation, and flip
// For advanced interface, this is called for aligning only, and localPath should be the 
// full path to the input stack file
int CFalconHelper::StackFrames(CString localPath, CString &directory, CString &rootname, 
  long divideBy2, int divideBy, int rotateFlip, int aliSumRotFlip, float pixel, 
  BOOL doAsync, bool readLocally, int aliParamInd, long &numStacked, 
  CameraThreadData *camTD)
{
  int ind, retval = 0;
  CFileStatus status;
  CString str, str2;
  CFile file;
  mDeletePrev = false;
  if (localPath.IsEmpty())
    localPath = directory;

  // Copy arguments to members and set up to do one call per frame
  mLocalPath = localPath;
  mDirectory = directory;
  mRootname = rootname;
  mDivideBy = divideBy;
  mDivideBy2 = divideBy2;
  mRotateFlip = rotateFlip;
  mAliSumRotFlip = aliSumRotFlip;
  mPixel = pixel;
  mReadLocally = readLocally;
  mCamTD = camTD;
  mAliComParamInd  = aliParamInd;

  numStacked = 0;
  mNumStacked = 0;
  mNumAligned = 0;
  mAlignError = 0;
  mNx = -1;
  mNy = -1;
  mDeleteList.clear();
  mStartedAsyncSave = -1;
  str = mDirectory + "\\" + mRootname + ".mrc";
  SEMTrace('E', "In stackFrames  local %d %s", readLocally ? 1 : 0, (LPCTSTR)localPath);

    // Open a file locally if flag set for that
  if (readLocally) {
    mFrameFP = fopen((LPCTSTR)localPath, "rb");
    if (!mFrameFP) {
      SEMTrace('E', "Error opening frame stack at local path: %s", (LPCTSTR)localPath);
      return FIF_OPEN_OLD;
    }
    if (mrc_head_read(mFrameFP, &mMrcHeader)) {
      fclose(mFrameFP);
      return FIF_OPEN_OLD;
    }
    mNumFiles = mMrcHeader.nz;
    mHeadNums[0] = mMrcHeader.nx;
    mHeadNums[1] = mMrcHeader.ny;
    mMrcHeader.yInverted = 0;

  } else {

    // Get the file map or open the file
    retval = mPlugFuncs->FIFbuildFileMap((LPCTSTR)localPath, &mNumFiles, &mHeadNums[0],
      &mHeadNums[1]);
    if (retval)
      return retval;
  }

  // Check if file exists and if there is already a backup file
  if (!mJustAlignNotSave && !mDoingAdvancedFrames) {
    if (CFile::GetStatus((LPCTSTR)str, status)) {
      if (CFile::GetStatus((LPCTSTR)(str + "~"), status))
        return FIF_BACKUP_EXISTS;
      if (imodBackupFile((LPCTSTR)str))
        return FIF_BACKUP_ERR;
      PrintfToLog("Stack file %s already exists; making it a backup by adding ~", 
        (LPCTSTR)str);
    }
  }

  // Save the last directory a frame file was in
  if (!mJustAlignNotSave)
    UtilSplitPath(mDoingAdvancedFrames ? localPath : str, mLastFrameDir, str2);

  // Start asynchronous looping on files
  mFileInd = 0;
  if (doAsync) {
    for (ind = 0; ind < mNumFiles; ind++)
      mDeleteList.push_back(0);
    mWaitStart = GetTickCount();
    mWaitCount = 0;
    mStackingFrames = true;
    mWinApp->AddIdleTask(TASK_STACK_FALCON, 1, 0);
    numStacked = mNumFiles;
    return 0;
  }

  // Loop on images; stop on fatal errors or on no more files
  while (mFileInd < mNumFiles) {
    StackNextTask(0);
    if (!mNumStacked && !mNumAligned && 
      (mStackError == FIF_MEMORY || mStackError == FIF_OPEN_NEW)) {
        retval = mStackError;
        break;
    }
    if (mStackError == FIF_NO_MORE_FILES)
      break;
  }
  numStacked = mNumStacked;
  CleanupAndFinishAlign(!mJustAlignNotSave && !mDoingAdvancedFrames, 0);
  return retval;
}

// Stack the next frame or finish up if none: param is > 0 for asynchronous stacking
void CFalconHelper::StackNextTask(int param)
{
  int ind, val;
  short *outPtr, *outData;
  char title[80];
  CString str;
  FileOptions fileOpt;
  bool saving = !mJustAlignNotSave && !mDoingAdvancedFrames;
  int rotateForSave = saving ? mRotateFlip : 0;
  int skipAlign = 0;
  mStackError = 0;
  if (mUseFrameAlign && mAlignSubset && mFileInd < mAlignStart - 1)
    skipAlign = -1;
  if (mUseFrameAlign && mAlignSubset && mFileInd > mAlignEnd - 1)
    skipAlign = 1;

  if (param && (mFileInd >= mNumFiles || (mDoingAdvancedFrames && skipAlign > 1) ||
    (mUseFrameAlign && !mCamera->GetStartedFalconAlign()))) {
    CleanupAndFinishAlign(saving, param);
    return;
  }

  if (!mStackError && mNx < 0) {

    // Set size, get array for reading and new KImage
    mNx = mHeadNums[0];
    mNy = mHeadNums[1];
    if (rotateForSave)
      NewArray(mRotData, short, mNx * mNy);
    mImage = new KImageShort((rotateForSave % 2) ? mNy : mNx,
      (rotateForSave % 2) ? mNx : mNy);
    if (!mDivideBy2 && !mDoingAdvancedFrames)
      mImage->setType(kUSHORT);
    mUseImage2 = 0;
    if (mUseFrameAlign && (mCamTD->DMSizeX != ((mAliSumRotFlip % 2) ? mNy : mNx) || 
      mCamTD->DMSizeY != ((mAliSumRotFlip % 2) ? mNx : mNy))) {
        mAlignError = FIF_ALIGN_SIZE_ERR;
        mFrameAli->cleanup();
        if (!saving) {
          CleanupAndFinishAlign(saving, param);
          return;
        }
    }

    // Here is the test for whether to save asynchronously also, hereafter mImage2 is
    // the non-NULL when doing so.  If this is bad locally, could change to 
    // mUsePlugin && mPlugFuncs->FIFcleanUp2 && CBaseSocket::ServerIsRemote(FEI_SOCK_ID)
    if (saving && param && mWinApp->mBufferManager->GetSaveAsynchronously()) {
      mImage2 = new KImageShort((mRotateFlip % 2) ? mNy : mNx, 
        (mRotateFlip % 2) ? mNx : mNy);
      if (!mDivideBy2)
        mImage2->setType(kUSHORT);
      mUseImage2 = -1;
    }
    if (!mImage->getData() || (mImage2 && !mImage2->getData()) || 
      (rotateForSave && !mRotData)) {
        mStackError = FIF_MEMORY;

    } else if (saving) {

      // Set up file options and get storage
      fileOpt.maxSec = 1;
      fileOpt.mode = mDivideBy2 ? MRC_MODE_SHORT : MRC_MODE_USHORT;
      fileOpt.typext = 0;
      fileOpt.useMdoc = false;
      fileOpt.montageInMdoc = false;
      fileOpt.fileType = STORE_TYPE_MRC;
      mStoreMRC = new KStoreMRC(mDirectory + "\\" + mRootname + ".mrc", fileOpt);
      if (mStoreMRC == NULL || !mStoreMRC->FileOK()) {
        mStackError = FIF_OPEN_NEW;
      } else {
        mWinApp->mDocWnd->MakeSerialEMTitle(CString("Falcon intermediate images"), title);
        mStoreMRC->AddTitle(title);
        mStoreMRC->SetPixelSpacing(mPixel);
      }
    }
    if (mStackError) {
      CleanupStacking();
      if (param)
        PrintfToLog("Error %d occurred trying to stack frames: %s", mStackError, 
        errMess[mStackError - 1]);
      return;
    }

  }
  outData = (short *)(mUseImage2 > 0 ? mImage2->getData() : mImage->getData());
  outPtr = rotateForSave ? mRotData : outData;

  // Read the data locally or from plugin
  // Can skip frames not being aligned if just reading directly if doing advanced and
  // past the end
  if (!mStackError) {
    if (mReadLocally) {
      if (!skipAlign && mrc_read_slice(outPtr, mFrameFP, &mMrcHeader, mFileInd, 'Z'))
        mStackError = FIF_READ_ERR;
    } else if (!(mDoingAdvancedFrames && skipAlign > 0)) {
      mStackError = mPlugFuncs->FIFgetNextFrame(outPtr, mDivideBy2, mDivideBy, 
        mDeletePrev);
    }
    mDeletePrev = false;
  }

  // Do alignment unless skipping that
  if (!mStackError && mUseFrameAlign && !mAlignError && !skipAlign) {
     ind = mFrameAli->nextFrame(outPtr, mImage->getMode(), NULL, mNx, mNy, 
      NULL, 0.,  // Truncation limit...
      NULL, 0, 0, 1, 0., 0.);
     if (ind) {
       SEMTrace('1', "Error %d calling framealign nextFrame on frame %d", ind, 
         mFileInd + 1);
       mAlignError = FIF_ALIGN_FRAME_ERR;
       mFrameAli->cleanup();
     }
     if (ind && !saving) {
       CleanupAndFinishAlign(saving, param);
       return;
     }
     if (!mAlignError) {
       mNumAligned++;
      mDeletePrev = true;
     }
  }

  // Write the image
  if (!mStackError && saving) {
    if (mRotateFlip) {
      ProcRotateFlip((short *)mRotData, mDivideBy2 ? kSHORT : kUSHORT, mNx, mNy, 
        mRotateFlip, 0, outData, &ind, &val);
    }
    if (mImage2) {

      // Check async saving first,
      CheckAsyncSave();
      // then Start a new async save and swap to other image
      mStartedAsyncSave = mFileInd;
      mWinApp->mBufferManager->StartAsyncSave(mStoreMRC, 
        mUseImage2 > 0 ? mImage2 : mImage, -1);
      mUseImage2 = -mUseImage2;
      ind = 0;

    } else {
      ind = mStoreMRC->AppendImage(mImage);
    }
    if (ind) {
      mStackError = FIF_WRITE_ERR;
    } else {

      // Delete the FEI image now that it is safely stacked, unless asynchronous
      if (!mImage2) {
        mNumStacked++;
        mDeletePrev = true;
      }
      SEMTrace('1', "Frame %d %s added to stack", mFileInd + 1, 
        mImage2 ? "started being" : "");
    }
  }

  // For errors beside no more files, report the error and go for the next frame
  if (mStackError && mStackError != FIF_NO_MORE_FILES)
    SEMTrace('1', "Error dealing with frame %d: %s", mFileInd + 1, 
      errMess[mStackError - 1]);
  mFileInd++;
  if (!saving && skipAlign > 1)
    mStackError = FIF_NO_MORE_FILES;
  if (param && mStackError != FIF_NO_MORE_FILES && (!mStackError || saving)) {
    mWaitStart = GetTickCount();
    mWaitCount = mFileInd < mNumFiles ? 0 : mWaitMaxCount + 1;
    mWinApp->AddIdleTask(TASK_STACK_FALCON, 1, 0);
  }

  // But for no more files, clean up and report result
  if (mStackError == FIF_NO_MORE_FILES)
    CleanupAndFinishAlign(saving, param);
}


void CFalconHelper::CleanupAndFinishAlign(bool saving, int async)
{
  CString comPath;
  int err = -1;
  if (mAlignError && mJustAlignNotSave)
    mDeletePrev = false;
  CleanupStacking();
  if (mUseFrameAlign) {
    if (mAlignError) {
      if (!saving)
        mStackError = mAlignError;
    } else {
      FinishFrameAlignment();
    }
    if (async)
      mCamera->DisplayNewImage(true);
  } else if (mAliComParamInd >= 0 && mNumStacked > 1) {
    comPath = (mCamera->GetComPathIsFramePath() ? mDirectory : 
      mCamera->GetAlignFramesComPath()) + '\\' + mRootname + ".pcm";
    err = WriteAlignComFile(mDirectory + "\\" + mRootname + ".mrc", comPath, 
      mAliComParamInd, mUseGpuForAlign[1], false);
  }
  if (saving)
    PrintfToLog("%d of %d frames were stacked successfully%s", mNumStacked, mNumFiles,
      err == 0 ? " and com file for aligning was written" : "");
  if (err > 0)
    PrintfToLog("WARNING: The com file for aligning was not saved: %s",
      SEMCCDErrorMessage(err));
}

void CFalconHelper::FinishFrameAlignment(void)
{
  CArray<FrameAliParams, FrameAliParams> *faParams = 
    mCamera->GetFrameAliParams();
  FrameAliParams param = faParams->GetAt(mFAparamSetInd);
  int ind, bestFilt, val;
  float resMean[5], smoothDist[5], rawDist[5], resSD[5], meanResMax[5];
  float maxResMax[5], meanRawMax[5], maxRawMax[5], ringCorrs[26], frcDelta = 0.02f;
  float refSigma = (float)(0.001 * B3DNINT(1000. * param.refRadius2 * 
    param.sigmaRatio));
  float crossHalf, crossQuarter, crossEighth, halfNyq;
  float *aliSum = mFrameAli->getFullWorkArray();
  float *xShifts = new float [mNumAligned + 10];
  float *yShifts = new float [mNumAligned + 10];
  short *sAliSum = mAliSumRotFlip ? (short *)aliSum : mCamTD->Array[0];
  unsigned short *usAliSum = (unsigned short *)aliSum;

  ind = mFrameAli->finishAlignAndSum(param.refRadius2, refSigma, param.stopIterBelow,
    param.groupRefine, (mNumAligned >= param.smoothThresh && param.doSmooth) ? 1 : 0,
    aliSum, xShifts, yShifts, xShifts, yShifts,
    mGettingFRC ? ringCorrs : NULL, frcDelta, bestFilt, smoothDist, rawDist,
    resMean, resSD, meanResMax, maxResMax, meanRawMax, maxRawMax);
  if (!ind)  {
    mCamTD->FaResMean = resMean[bestFilt];
    mCamTD->FaMaxResMax = maxResMax[bestFilt];
    mCamTD->FaMeanRawMax = meanRawMax[bestFilt];
    mCamTD->FaMaxRawMax = maxRawMax[bestFilt];
    mCamTD->FaRawDist = rawDist[bestFilt];
    mCamTD->FaSmoothDist = smoothDist[bestFilt];
    if (mGettingFRC) {
      mFrameAli->analyzeFRCcrossings(ringCorrs, frcDelta, crossHalf, crossQuarter, 
        crossEighth, halfNyq);
      mCamTD->FaCrossHalf = B3DNINT(K2FA_FRC_INT_SCALE * crossHalf); 
      mCamTD->FaCrossQuarter = B3DNINT(K2FA_FRC_INT_SCALE * crossQuarter);
      mCamTD->FaCrossEighth = B3DNINT(K2FA_FRC_INT_SCALE * crossEighth);
      mCamTD->FaHalfNyq = B3DNINT(K2FA_FRC_INT_SCALE * halfNyq);
    } else {
      mCamTD->FaCrossHalf = mCamTD->FaCrossQuarter = mCamTD->FaCrossEighth = 
        mCamTD->FaHalfNyq = 0;
    }

    // Only old Falcon 2 frames were divided, the advanced scripting frames were
    // just read in and still need that treatment (but are they all signed now?)
    if (mDoingAdvancedFrames && mDivideBy2) {
      for (ind = 0; ind < mNx * mNy; ind++) {
        val = B3DNINT(aliSum[ind] / mDivideBy);
        B3DCLAMP(val, -32768, 32767);
        sAliSum[ind] = (short)val;
      }
    } else {
      for (ind = 0; ind < mNx * mNy; ind++) {
        val = B3DNINT(aliSum[ind]);
        B3DCLAMP(val, 0, 65535);
        usAliSum[ind] = (unsigned short)val;
      }
    }
    if (mAliSumRotFlip)
      ProcRotateFlip(sAliSum, mDivideBy2 ? kSHORT : kUSHORT, mNx, mNy, 
        mAliSumRotFlip, 0, mCamTD->Array[0], &ind, &val);
  } else {
    SEMTrace('1', "Error %d finishing frame alignment", ind);
    mAlignError = FIF_FINISH_ALIGN_ERR;
  }
  mFrameAli->cleanup();
}

// "Busy" test for whether waiting before stacking next frame
int CFalconHelper::StackingWaiting()
{
  if (SEMTickInterval(mWaitStart) > mWaitInterval || 
    mWaitCount > mWaitMaxCount)
    return 0;
  mWaitCount++;
  return 1;
}

// Check async saving before a new save or at the end
// and if it was one of ours and did not fail, set the flag for deletion.
void CFalconHelper::CheckAsyncSave(void)
{
  mWinApp->mBufferManager->CheckAsyncSaving();
  if (mStartedAsyncSave >= 0) {
    if (mWinApp->mBufferManager->GetImageAsyncFailed())
      SEMTrace('1', "Error in asynchronous save for frame %d", 
      mStartedAsyncSave + 1);
    else {
      mDeleteList[mStartedAsyncSave] = 1;
      mNumStacked++;
    }
  }

}

// Clean up memory when done stacking, plus delete files when asynchronous saving used
void CFalconHelper::CleanupStacking(void)
{
  std::map<int, std::string>::iterator iter;
  CString str;

  if (mReadLocally) {
    fclose(mFrameFP);
    mFrameFP = NULL;
    if (mJustAlignNotSave) {
      SEMTrace('E', "Removing %s and .xml after alignment", (LPCTSTR)mLocalPath);
      UtilRemoveFile(mLocalPath);
      UtilRemoveFile(mLocalPath.Left(mLocalPath.GetLength() - 3) + "xml");
    }
  } else {

    // If asynchronous save was used, the need to check on it and then do special cleanup
    // of all files that were saved successfully  
    if (mImage2) {
      CheckAsyncSave();
      mPlugFuncs->FIFcleanUp2(mNumFiles, &(mDeleteList[0]));

      // Or do regular cleanup through plugin
    } else
      mPlugFuncs->FIFcleanUp(mDeletePrev);
  }
  mDeletePrev = false;
  delete mStoreMRC;
  delete mRotData;
  delete mImage;
  delete mImage2;
  mRotData = NULL;
  mStoreMRC = NULL;
  mImage = NULL;
  mImage2 = NULL;
  mStackingFrames = false;
}

// Add any new frames to the file map under a unique number
int CFalconHelper::BuildFileMap(CString localPath, CString &directory)
{
  if (localPath.IsEmpty())
    localPath = directory;

  return mPlugFuncs->FIFbuildFileMap((LPCTSTR)localPath, &mNumFiles, &mHeadNums[0],
    &mHeadNums[1]);
}

const char * CFalconHelper::GetErrorString(int code)
{
  if (code <= 0 || code >= FIF_LAST_CODE)
    return "";
  return errMess[code - 1];
}

// Divide the given number of readouts among the current number of frames
void CFalconHelper::DistributeSubframes(ShortVec &summedFrameList, int numReadouts, 
  int newFrames, FloatVec &userFrameFrac, FloatVec &userSubframeFrac)
{
  float fracLeft = 1., sum1 = 0., sum2 = 0.;
  int ind, frames, subframesLeft, framesLeft, numBlocks, blocksLeft, frameTot = 0;
  int perFrame, extra;
  ShortVec framesPerBlock, subframesPerBlock;
  if (!summedFrameList.size())
    return;
  int numFrames = B3DMIN(numReadouts, newFrames);

  // If there are too many fractions, renormalize the smaller number of them
  if (numFrames < (int)userFrameFrac.size()) {
    userFrameFrac.resize(numFrames);
    for (ind = 0; ind < numFrames; ind++) {
      sum1 += userFrameFrac[ind];
      sum2 += userSubframeFrac[ind];
    }
    for (ind = 0; ind < numFrames; ind++) {
      userFrameFrac[ind] /= sum1;
      userSubframeFrac[ind] /= sum2;
    }
  }

  // First apportion total frames among the blocks
  framesLeft = numFrames;
  numBlocks = (int)userFrameFrac.size();
  for (ind = 0; ind < numBlocks; ind++) {
    frames = B3DNINT(B3DMAX(1., userFrameFrac[ind] * framesLeft / fracLeft));
    blocksLeft = numBlocks - 1 - ind;
    if (framesLeft - frames < blocksLeft)
      frames = B3DMAX(1, framesLeft - blocksLeft);
    framesPerBlock.push_back(frames);
    fracLeft -= userFrameFrac[ind];
    framesLeft -= frames;
  }

  // Now distribute subframes among blocks with more constraints
  fracLeft = 1.;
  framesLeft = numFrames;
  subframesLeft = numReadouts;
  for (ind = 0; ind < numBlocks; ind++) {
    frames = B3DNINT(B3DMAX(framesPerBlock[ind], userSubframeFrac[ind] * subframesLeft / 
      fracLeft));
    framesLeft -= framesPerBlock[ind];
    if (subframesLeft - frames < framesLeft)
      frames = B3DMAX(framesPerBlock[ind], subframesLeft - framesLeft);
    subframesPerBlock.push_back(frames);
    fracLeft -= userSubframeFrac[ind];
    subframesLeft -= frames;
  }
 
  // Now there are one or two list entries per block
  summedFrameList.clear();
  for (ind = 0; ind < numBlocks; ind++) {
    perFrame = subframesPerBlock[ind] / framesPerBlock[ind];
    extra = subframesPerBlock[ind] % framesPerBlock[ind];
    summedFrameList.push_back(framesPerBlock[ind] - extra);
    summedFrameList.push_back(perFrame);
    if (extra) {
      summedFrameList.push_back(extra);
      summedFrameList.push_back(perFrame + 1);
    }
  }
}

float CFalconHelper::AdjustForExposure(ShortVec &summedFrameList, int numSkipBefore,
    int numSkipAfter, float exposure, float readoutInterval, FloatVec &userFrameFrac, 
    FloatVec &userSubframeFrac)
{
  int newFrames, curTotal = 0;
  int numFrames = B3DMAX(1, B3DNINT(exposure / readoutInterval) - 
    (numSkipBefore + numSkipAfter));
  newFrames = GetFrameTotals(summedFrameList, curTotal);
  if (numFrames != curTotal)
    DistributeSubframes(summedFrameList, numFrames, newFrames, userFrameFrac, 
    userSubframeFrac);
  return (numFrames + numSkipBefore + numSkipAfter) * readoutInterval;
}

// Adjusts frame sums for exposure if appropriate
float CFalconHelper::AdjustSumsForExposure(CameraParameters *camParams, 
  ControlSet *conSet, float exposure)
{
  bool falconCanSave = IS_FALCON2_OR_3(camParams) && 
    mCamera->GetMaxFalconFrames(camParams) && 
    (mCamera->GetFrameSavingEnabled() || FCAM_ADVANCED(camParams));
  if (falconCanSave || (mCamera->IsK2ConSetSaving(conSet, camParams) && 
    conSet->sumK2Frames))
    return AdjustForExposure(conSet->summedFrameList, 
      falconCanSave ? conSet->numSkipBefore : 0, falconCanSave ? conSet->numSkipAfter : 0, 
      exposure, falconCanSave ? mCamera->GetFalconReadoutInterval() : conSet->frameTime, 
      conSet->userFrameFractions, conSet->userSubframeFractions);
  return exposure;
}

// Returns the number of summed frames given the list, and also the number of camera
// frames in totalSubFrames, all limited by maxFrames if positive (default -1)
int CFalconHelper::GetFrameTotals(ShortVec &summedFrameList, int &totalSubframes,
  int maxFrames)
{
  int frameTot = 0;
  totalSubframes = 0;
  for (int ind = 0; ind < (int)summedFrameList.size() / 2; ind++) {
    for (int sumf = 0; sumf < summedFrameList[ind * 2]; sumf++) {
      frameTot++;
      totalSubframes += summedFrameList[ind * 2 + 1];
      if (maxFrames > 0 && totalSubframes >= maxFrames) {
        totalSubframes = maxFrames;
        return frameTot;
      }
    }
  }
  return frameTot;
}

// When aligning a subset of summed frames, count up actual frames in the range of the
// subset and compute their exposure time
float CFalconHelper::AlignedSubsetExposure(ShortVec &summedFrameList, float frameTime, 
  int subsetStart, int subsetEnd, int maxFrames)
{
  int ind, sumf, alignedTot = 0, frameTot = 0, totalSubframes = 0;
  for (ind = 0; ind < (int)summedFrameList.size() / 2; ind++) {
    for (sumf = 0; sumf < summedFrameList[ind * 2]; sumf++) {
      frameTot++;
      totalSubframes += summedFrameList[ind * 2 + 1];
      if (frameTot >= subsetStart && frameTot <= subsetEnd)
        alignedTot += summedFrameList[ind * 2 + 1];
      if (maxFrames > 0 && totalSubframes >= maxFrames) {
        alignedTot -= totalSubframes - maxFrames;
        return (float)(alignedTot * frameTime);
      }
      if (frameTot >= subsetEnd)
        return (float)(alignedTot * frameTime);
    }
  }
  return (float)(alignedTot * frameTime);     
}

/*
 * Write a com file for alignment with alignframes
 */
int CFalconHelper::WriteAlignComFile(CString inputFile, CString comName, int faParamInd, 
  int useGPU, bool ifMdoc)
{
  CString comStr, strTemp, aliHead, inputPath, relPath, outputRoot, outputExt, temp,temp2;
  CArray<FrameAliParams, FrameAliParams> *faParams = 
    mCamera->GetFrameAliParams();
  FrameAliParams param; 
  int ind, error = 0;
  float radius2[4];
  int numAllVsAll, groupSize, refineIter, doSpline, numFilters;
  B3DCLAMP(faParamInd, 0, (int)faParams->GetSize() - 1);
  param = faParams->GetAt(faParamInd);
  numAllVsAll = mCamera->NumAllVsAllFromFAparam(param, mNumStacked, groupSize, 
    refineIter, doSpline, numFilters, radius2);

  // Translate the strategy options
  if (param.strategy == FRAMEALI_HALF_PAIRWISE)
    numAllVsAll = -2;
  else if (param.strategy == FRAMEALI_ALL_PAIRWISE)
    numAllVsAll = -1;

  // Do all the easy options
  strTemp.Format("$alignframes -StandardInput\n"
    "UseGPU %d\n"
    "PairwiseFrames %d\n"
    "GroupSize %d\n"
    "AlignAndSumBinning %d 1\n"
    "AntialiasFilter %d\n"
    "RefineAlignment %d\n"
    "StopIterationsAtShift %f\n"
    "ShiftLimit %d\n"
    "MinForSplineSmoothing %d\n"
    "FilterRadius2 %f\n"
    "FilterSigma2 %f\n"
    "VaryFilter %f", useGPU ? 0 : -1, numAllVsAll, param.useGroups ? param.groupSize : 1, 
    param.aliBinning, param.antialiasType, refineIter, param.stopIterBelow,
    param.shiftLimit, param.doSmooth ? param.smoothThresh : 0, radius2[0], 
    radius2[0] * param.sigmaRatio, radius2[0]);
  comStr = strTemp;
  for (ind = 1; ind < numFilters; ind++) {
    if (radius2[ind] > 0) {
      strTemp.Format(" %f", radius2[ind]);
      comStr += strTemp;
    }
  }
  comStr += "\n";
  if (param.refRadius2 > 0) {
    strTemp.Format("RefineRadius2 %f\n", param.refRadius2);
    comStr += strTemp;
  }
  if (param.hybridShifts)
    comStr += "UseHybridShifts 1\n";
  if (param.groupRefine)
    comStr += "RefineWithGroupSums 1\n";
  if (param.outputFloatSums)
    comStr += "ModeToOutput 2\n";
  if (param.alignSubset) {
    strTemp.Format("StartingEndingFrames %d %d\n", param.subsetStart,
      param.subsetEnd);
    comStr += strTemp;
  }

  // If input is an mdoc file, copy it to frame location and strip path from name
  if (ifMdoc) {
    UtilSplitPath(inputFile, temp, temp2);
    temp = mLastFrameDir + '\\' + temp2;
    if (CopyFile((LPCTSTR)inputFile, (LPCTSTR)temp, false))
      return -1;
    inputFile = temp2;
  }

  // get the relative path and make sure it is OK   
  UtilSplitPath(comName, aliHead, temp);
  SEMTrace('E', "com %s  alihead %s  lastframe %s", (LPCTSTR)comName, (LPCTSTR)aliHead, (LPCTSTR)mLastFrameDir);
  if (UtilRelativePath(aliHead, mLastFrameDir, relPath))
    return MAKECOM_NO_REL_PATH;

  // Set input file with the relative path and the right option
  inputPath = relPath + inputFile;
  comStr += (ifMdoc ? "MetadataFile " : "InputFile ") + inputPath + "\n";
  if (ifMdoc && relPath.GetLength())
    comStr += "PathToFramesInMdoc " + relPath + "\n";
  if (ifMdoc)
    comStr += "AdjustAndWriteMdoc 1\n";

  // Get the output file name, take 2 extension for an mdoc or replace .tif by .mrc
  UtilSplitExtension(inputFile, outputRoot, outputExt);
  if (ifMdoc) {
    temp = outputRoot;
    UtilSplitExtension(temp, outputRoot, temp2);
    if (temp2.GetLength())
      outputExt = temp2;
  } else if (outputExt == ".tif" || outputExt == ".mrcs")
    outputExt = ".mrc";
  comStr += "OutputImageFile " + outputRoot + "_ali" + outputExt + "\n";

  // Write the file
  error = UtilWriteTextFile(comName, comStr);
  if (error == 1)
    error = OPEN_COM_ERROR;
  else if (error == 2)
    error = WRITE_COM_ERROR;
  return error;
}
