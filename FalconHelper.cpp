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
#include "SerialEM.h"
#include "EMscope.h"
#include "SerialEMDoc.h"
#include "FalconHelper.h"
#include "CameraController.h"
#include "ShiftManager.h"
#include "PluginManager.h"
#include "Utilities\XCorr.h"
#include <map>
#include "Shared\b3dutil.h"
#include "framealign.h"

enum {FIF_COMM_ERR = 1, FIF_NO_FILES, FIF_NO_NEW_FILES, FIF_OPEN_OLD, FIF_READ_ERR,
FIF_BAD_SIZE, FIF_MEMORY, FIF_OPEN_NEW, FIF_MISMATCH, FIF_WRITE_ERR, FIF_REMOVE_ERR, 
FIF_NO_MORE_FILES, FIF_BAD_MODE, FIF_CONF_NOT_EXIST, FIF_CONF_OPEN_ERR, 
FIF_CONF_READ_ERR, FIF_CONF_NO_MODE_LINE, FIF_CONF_NOT_INIT, FIF_CONF_BAD_MODE, 
FIF_CONF_WRITE_ERR, FIF_BACKUP_EXISTS, FIF_BACKUP_ERR, FIF_LAST_CODE};

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
"Error renaming existing stack file to backup name"
};

static FrameAlign sFrameAli;

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
  mWaitInterval = 70;
  mWaitMaxCount = 10;
  mStackingFrames = false;
}

CFalconHelper::~CFalconHelper(void)
{
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
  configFile = mCamera->GetFalconFrameConfig();
  if (configFile.IsEmpty() || skipConfigs)
    return;
  error = mPlugFuncs->FIFinitialize((LPCTSTR)configFile, mReadoutInterval);
  if (error) {
    mWinApp->AppendToLog("Error opening or reading from intermediate frame file " +
      configFile);
    mWinApp->mCamera->SetMaxFalconFrames(0);
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
  float frameInterval = mWinApp->mCamera->GetFalconReadoutInterval();
  int ind, block;

  if (!FCAM_ADVANCED(camParams)) {

    // Check the directory entry and assign local path if needed
    if (directory.IsEmpty()) {
      SEMMessageBox("You must specify a directory for saving the frame images");
      return 1;
    }
    if (localPath.IsEmpty())
      localPath = directory;

    // Create directory if it doesn't exist
    if (conSet.saveFrames && !CFile::GetStatus((LPCTSTR)directory, status)) {
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
  
  if (FCAM_ADVANCED(camParams)) {
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
      conSet.saveFrames, temp, conSet.numSkipBefore, readPtr)) {
        str.Format("Error opening or writing to intermediate frame configuration file"
          " %s:\n%s", (LPCTSTR)configFile, mPlugFuncs->GetLastErrorString());
        SEMMessageBox(str);
        return 1;
    }
  }
  return 0;
 }

// Stack the frames, possibly located in localPath, into a file with the given root name
// the given directory, with potential division by a factor, rotation, and flip
int CFalconHelper::StackFrames(CString localPath, CString &directory, CString &rootname, 
                               long divideBy2, int divideBy, int rotateFlip, float pixel, 
                               BOOL doAsync, long &numStacked)
{
  int ind, retval = 0;
  CFileStatus status;
  CString str;
  float radius2[4], sigma2[4], sigma1 = 0.03f; 
  float fullTaperFrac = 0.02f;
  float trimFrac = fullTaperFrac;
  float taperFrac = 0.1f;
  float kFactor = 4.5f;
  float maxMaxWeight = 0.1f;
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
  mPixel = pixel;

  numStacked = 0;
  mNumStacked = 0;
  mNx = -1;
  mNy = -1;
  mDeleteList.clear();
  mStartedAsyncSave = -1;

  // Get the file map
  retval = mPlugFuncs->FIFbuildFileMap((LPCTSTR)localPath, &mNumFiles, &mHeadNums[0],
    &mHeadNums[1]);
  if (retval)
    return retval;

  // Check if file exists and if there is already a backup file
  str = mDirectory + "\\" + mRootname + ".mrc";
  if (CFile::GetStatus((LPCTSTR)str, status)) {
    if (CFile::GetStatus((LPCTSTR)(str + "~"), status))
      return FIF_BACKUP_EXISTS;
    if (imodBackupFile((LPCTSTR)str))
      return FIF_BACKUP_ERR;
    PrintfToLog("Stack file %s already exists; making it a backup by adding ~", 
      (LPCTSTR)str);
  }
  radius2[0] = 0.06f;
  sigma2[0] = 0.06f * 0.1f;
  ind = sFrameAli.initialize(1, 2, trimFrac, 
    7, 0, 0, 0, 0, mHeadNums[0],
    mHeadNums[1], fullTaperFrac, taperFrac, 
    4, 0., radius2, sigma1, 
    sigma2, 1, 0, kFactor, maxMaxWeight, 0, mNumFiles, 0, 0);
  if (ind)
    PrintfToLog("Error return %d from fa.initialize", ind);

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
    if (!mNumStacked && (mStackError == FIF_MEMORY || mStackError == FIF_OPEN_NEW)) {
      retval = mStackError;
      break;
    }
    if (mStackError == FIF_NO_MORE_FILES)
      break;
  }
  numStacked = mNumStacked;
  CleanupStacking();
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
  mStackError = 0;
  if (param && mFileInd >= mNumFiles) {
    CleanupStacking();
    PrintfToLog("%d of %d frames were stacked successfully", mNumStacked, mNumFiles);
    return;
  }
  if (!mStackError && mNx < 0) {

    // Set size, get array for reading and new KImage
    mNx = mHeadNums[0];
    mNy = mHeadNums[1];
    if (mRotateFlip)
      NewArray(mRotData, short, mNx * mNy);
    mImage = new KImageShort(mRotateFlip % 2 ? mNy : mNx, mRotateFlip % 2 ? mNx : mNy);
    if (!mDivideBy2)
      mImage->setType(kUSHORT);
    mUseImage2 = 0;

    // Here is the test for whether to save asynchronously also, hereafter mImage2 is
    // the non-NULL when doing so.  If this is bad locally, could change to 
    // mUsePlugin && mPlugFuncs->FIFcleanUp2 && CBaseSocket::ServerIsRemote(FEI_SOCK_ID)
    if (param && mWinApp->mBufferManager->GetSaveAsynchronously()) {
        mImage2 = new KImageShort(mRotateFlip % 2 ? mNy : mNx, 
          mRotateFlip % 2 ? mNx : mNy);
        if (!mDivideBy2)
          mImage2->setType(kUSHORT);
        mUseImage2 = -1;
    }
    if (!mImage->getData() || (mImage2 && !mImage2->getData()) || 
      (mRotateFlip && !mRotData)) {
        mStackError = FIF_MEMORY;

    } else {

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
  outPtr = mRotateFlip ? mRotData : outData;

  if (!mStackError) {
    mStackError = mPlugFuncs->FIFgetNextFrame(outPtr, mDivideBy2, mDivideBy, mDeletePrev);
    mDeletePrev = false;
  }

  // Write the image
  if (!mStackError) {
    ind = sFrameAli.nextFrame(outPtr, mDivideBy2 ? MRC_MODE_SHORT : MRC_MODE_USHORT,
      NULL, 0, 0, NULL, 
      0., NULL, 0,0, 
      1, 0., 0.);
    if (ind)
      PrintfToLog("Error %d from fa.nextFrame", ind);
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
  if (param && mStackError != FIF_NO_MORE_FILES) {
    mWaitStart = GetTickCount();
    mWaitCount = mFileInd < mNumFiles ? 0 : mWaitMaxCount + 1;
    mWinApp->AddIdleTask(TASK_STACK_FALCON, 1, 0);
  }

  // But for no more files, clean up and report result
  if (mStackError == FIF_NO_MORE_FILES) {
    CleanupStacking();
    PrintfToLog("%d of %d frames were stacked successfully", mNumStacked, mNumFiles);
    int err, bestFilt;
    float resMean[5], smoothDist[5], rawDist[5], resSD[5], meanResMax[5];
    float maxResMax[5], meanRawMax[5], maxRawMax[5], ringCorrs[26], frcDelta = 0.02f;
    float refSigma = 0.01f;
    float *aliSum = sFrameAli.getFullWorkArray();
    float *xShifts = new float [mNumStacked + 10];
    float *yShifts = new float [mNumStacked + 10];

    err = sFrameAli.finishAlignAndSum(0.06f, refSigma, 0.1f,
      0, 0, aliSum, xShifts, yShifts, xShifts, yShifts,
      ringCorrs, frcDelta, bestFilt, smoothDist, rawDist,
      resMean, resSD, meanResMax, maxResMax, meanRawMax, maxRawMax);
    if (err)
      PrintfToLog("Framealign failed to finish alignment (error %d)", err);
    delete [] xShifts;
    delete [] yShifts;
    sFrameAli.cleanup();
  }
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

  // If asynchronous save was used, the need to check on it and then do special cleanup
  // of all files that were saved successfully
  if (mImage2) {
    CheckAsyncSave();
    mPlugFuncs->FIFcleanUp2(mNumFiles, &(mDeleteList[0]));

    // Or do regular cleanup through plugin
  } else
    mPlugFuncs->FIFcleanUp(mDeletePrev);
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
