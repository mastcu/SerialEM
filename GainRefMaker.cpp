// GainRefMaker.cpp:      Acquires a gain reference and makes gain references
//                         for the camera controller from stored gain references
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\GainRefMaker.h"
#include "CameraController.h"
#include "GainRefDlg.h"
#include "ProcessImage.h"
#include "BeamAssessor.h"
#include "GatanSocket.h"
#include "Utilities\XCorr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define BINNING_SIZE 4000
#define EXPOSURE_MAX 10.
#define GAINREF_MAX_VAL 20.f
#define KV_CHECK_SECONDS 120.
#define REF_FIRST_SHOT  1
#define REF_SECOND_SHOT 2
#define REAL_FRAME_SHOT 3

/////////////////////////////////////////////////////////////////////////////
// CGainRefMaker

//IMPLEMENT_DYNCREATE(CGainRefMaker, CCmdTarget)

CGainRefMaker::CGainRefMaker()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mRefPath = "";
  mDMRefPath = "";
  mRemoteRefPath = "";

  InitializeRefArrays(); 
  mFrameCount = 0;
  mBackedUp = false;
  mCalibrateDose = true;
  mStoreMRC = NULL;
  mArray = NULL;
  mNumRefKVs = 0;
  mHTValue = 0;
  mDMrefAskPolicy = DMREF_ASK_IF_NEWER;
  mIgnoreHigherRefs = false;
  mUseOlderBinned2 = false;
  mTakingRefImages = false;
  mPreparingGainRef = false;
}

CGainRefMaker::~CGainRefMaker()
{
  DeleteReferences();
}

// Delete any internal references
void CGainRefMaker::DeleteReferences(void)
{
 for (int i = 0; i < MAX_CAMERAS; i++) {
    for (int j = 0; j < mNumBinnings[i]; j++) {
      if (mGainRef[i][j])
        delete mGainRef[i][j];
    }
  }
}

// Initialize all the arrays describing references
void CGainRefMaker::InitializeRefArrays(void)
{
  CameraParameters *camP = mWinApp->GetCamParams();
  mCamera = mWinApp->mCamera;
  for (int i = 0; i < MAX_CAMERAS; i++) {
    int numbase = camP[i].binnings[0] == 1 ? 2 : 1;
    mDMRefChecked[i] = false;
    mWarnedNoDM[i] = false;
    camP[i].numExtraGainRefs = B3DMAX(0, 
      B3DMIN(camP[i].numBinnings - numbase, camP[i].numExtraGainRefs));
    mNumBinnings[i] = numbase + camP[i].numExtraGainRefs;
    for (int j = 0; j < mNumBinnings[i] + 1; j++) {
      mRefExists[i][j] = FALSE;
      mGainRef[i][j] = NULL;
      mUseDMRef[i][j] = 0;
    }
  }
}

// Set the flag that DM reference needs checking
void CGainRefMaker::DMRefsNeedChecking()
{
  for (int i = 0; i < MAX_CAMERAS; i++)
    mDMRefChecked[i] = false;
}


BEGIN_MESSAGE_MAP(CGainRefMaker, CCmdTarget)
  //{{AFX_MSG_MAP(CGainRefMaker)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGainRefMaker message handlers

// Acquire a new gain reference
void CGainRefMaker::AcquireGainRef() 
{
  int fracField, tsizeX, tsizeY;
  int i, arrSize, numOdd = 0, numUsableOdd = 0;
  FileOptions fileOpt;
  CFileStatus status;

  // Get the control set and parameters for this work; check for KV change
  mConSet = mWinApp->GetConSets() + TRACK_CONSET;
  mCurrentCamera = mWinApp->GetCurrentCamera(); 
  mParam = mWinApp->GetCamParams() + mCurrentCamera;  
  mCamera = mWinApp->mCamera;
  fracField = mParam->subareasBad ? 1 : 8;
  CheckChangedKV();
  mSearchedRefs = false;
  FindExistingReferences();
  
  // Open the gain ref dialog to determine parameters
  CGainRefDlg dlg;

  // Set the dialog binning index based on last binning that was used
  dlg.mIndOffset = mParam->binnings[0] != 1 ? -1 : 0;
  dlg.m_iBinning = -1 - dlg.mIndOffset;
  for (i = 0; i < mNumBinnings[mCurrentCamera]; i++) {
    dlg.m_iBinning++;
    if (mParam->binnings[i] >= mParam->gainRefBinning)
      break;
  }

  dlg.m_iFrames = mParam->gainRefFrames;
  dlg.m_iTarget = mParam->gainRefTarget;
  dlg.mBinnings[0] = 1;
  dlg.mBinnings[1] = 2;
  for (i = 0; i < mNumBinnings[mCurrentCamera]; i++) {
    dlg.mRefExists[i] = mRefExists[mCurrentCamera][i];
    dlg.mBinnings[i] = mParam->binnings[i];
  }
  dlg.mNumBinnings = mNumBinnings[mCurrentCamera];
  dlg.mName = mParam->name;
  dlg.m_bCalDose = mCalibrateDose;
  dlg.m_bAverageDark = mParam->gainRefAverageDark > 0;
  dlg.m_iNumAverage = mParam->gainRefNumDarkAvg;
  dlg.mCamera = mCurrentCamera;
  dlg.m_bGainCalibrated = mParam->countsPerElectron > 0.;
  dlg.mCanAutosave = mWinApp->mDEToolDlg.CanSaveFrames(mParam);
  dlg.m_bAutosaveRaw = mParam->gainRefSaveRaw;
  if (!mParam->GatanCam)
    dlg.m_strOddRef = "cannot be taken - no unbinned reference";

  // Evaluate number of odd binnings and number that can be taken with refs
  for (i = 0; i < mParam->numBinnings; i++) {
    if (mParam->binnings[i] % 2) {
      numOdd++;
      if ((mRefExists[mCurrentCamera][0] && mParam->binnings[0] == 1) || 
        mRefExists[mCurrentCamera][i])
        numUsableOdd++;
    }
  }

  // The messages are already initialized for no usable odds, so cover all or partial
  if (numOdd == numUsableOdd) {
    if (mNumBinnings[mCurrentCamera] > 2 && numOdd > 1)
      dlg.m_strOddRef = "will use an existing reference)";
    else  
      dlg.m_strOddRef = "will use the existing unbinned reference)";
  } else if (numUsableOdd) {
    if (!mParam->GatanCam)
      dlg.m_strOddRef = "cannot be taken for some binnings";
    else 
      dlg.m_strOddRef = "will use a DM reference for some binnings";
  }

  // If not done before, set binning, and set it to two for large camera or if no 1
  if (dlg.m_iBinning < 0)
    dlg.m_iBinning = mParam->sizeX > BINNING_SIZE || mParam->sizeY > BINNING_SIZE || 
    mParam->binnings[0] != 1 ? 1 : 0;
  dlg.m_iBinning = B3DMIN(dlg.m_iBinning, mNumBinnings[mCurrentCamera] - 1);
  if (dlg.DoModal() != IDOK)
    return;

  // Get parameters back
  mParam->gainRefBinning = dlg.m_iBinning < 2 ? dlg.m_iBinning + 1 :
    mParam->binnings[dlg.m_iBinning + dlg.mIndOffset];
  mRefBinInd = dlg.m_iBinning + dlg.mIndOffset;
  mParam->gainRefFrames = dlg.m_iFrames;
  mParam->gainRefTarget = dlg.m_iTarget;
  mCalibrateDose = dlg.m_bCalDose;
  mParam->gainRefAverageDark = dlg.m_bAverageDark ? 1 : 0;
  mParam->gainRefNumDarkAvg = dlg.m_iNumAverage;
  mParam->gainRefSaveRaw = dlg.m_bAutosaveRaw ? 1 : 0;

  if (FEIscope && mCalibrateDose && dlg.m_bGainCalibrated && 
    mParam->gainRefBinning <= 2) {
    mWinApp->mScope->NormalizeCondenser();
    if (!mWinApp->GetSkipGainRefWarning())
      AfxMessageBox("The spot was normalized.\n\nCheck size and centering of beam.",
        MB_EXCLAME);
  }

  // Set up a control set to acquire unbinned ?? 1/8 field DS 0.1 sec image
  mConSet->exposure = 0.1f;
  mConSet->drift = 0;
  mConSet->binning = B3DMIN(2, mParam->gainRefBinning);
  mConSet->forceDark = 1;
  mConSet->processing = DARK_SUBTRACTED;
  mConSet->mode = SINGLE_FRAME;
  mConSet->saveFrames = 0;
  mConSet->shuttering = mParam->DMbeamShutterOK ? USE_BEAM_BLANK : USE_FILM_SHUTTER;
  mConSet->left = ((fracField - 1) * mParam->sizeX) / (2 * fracField);
  mConSet->right = ((fracField + 1) * mParam->sizeX) / (2 * fracField);
  mConSet->top = ((fracField - 1) * mParam->sizeY) / (2 * fracField);
  mConSet->bottom = ((fracField + 1) * mParam->sizeY) / (2 * fracField);

  // Get the coordinates and size right before modifying the modulo values for full frame
  tsizeX = mConSet->right - mConSet->left;
  tsizeY = mConSet->bottom - mConSet->top;
  mCamera->AdjustSizes(tsizeX, mParam->sizeX, mParam->moduloX, mConSet->left, 
    mConSet->right, tsizeY, mParam->sizeY, mParam->moduloY, mConSet->top, mConSet->bottom,
    1);

  mModuloSaveX = mParam->moduloX;
  mModuloSaveY = mParam->moduloY;
  arrSize = mParam->sizeX * mParam->sizeY / 
    (mParam->gainRefBinning * mParam->gainRefBinning);
  NewArray(mArray,float,arrSize);
  if (!mArray) {
    AfxMessageBox("Failed to get memory for gain reference", MB_EXCLAME);
    return;
  }
  for (i = 0; i < arrSize; i++)
    mArray[i] = 0.;

  // Rename file now to flush out errors
  mFileName = ComposeRefName(mParam->gainRefBinning);
  mBackupName = mFileName.Left(mFileName.GetLength() - 3) + "bak";
  if (CFile::GetStatus((LPCTSTR)mFileName, status)) {
    if (UtilRenameFile(mFileName, mBackupName)) {
      StopAcquiringRef();
      return;
    } 
    mBackedUp = true;
  }

  // Create output file also
  fileOpt.maxSec = 1;
  fileOpt.mode = 2;
  fileOpt.typext = 0;
  fileOpt.useMdoc = false;
  fileOpt.montageInMdoc = false;
  fileOpt.fileType = STORE_TYPE_MRC;
  mStoreMRC = new KStoreMRC(mFileName, fileOpt);
  if (mStoreMRC == NULL || !mStoreMRC->FileOK()) {
    AfxMessageBox("Error opening file for gain reference", MB_EXCLAME);
    if (mStoreMRC)
      delete mStoreMRC;
    mStoreMRC = NULL;
    StopAcquiringRef();
    return;
  }

  // Get expected size, modify camera params to allow full images but constrain
  // four port camera to even images, or retain constraints if the even size flag set
  mExpectedX = mParam->sizeX / mParam->gainRefBinning;
  mExpectedY = mParam->sizeY / mParam->gainRefBinning;
  if (mParam->TietzType && mParam->TietzBlocks) {
    i = mParam->TietzBlocks / mParam->gainRefBinning;
    mExpectedX = i * (mExpectedX / i);
    mExpectedY = i * (mExpectedY / i);
  }
  if (mParam->moduloX > 0) {
    if (mParam->refSizeEvenX <= 0)
      mParam->moduloX = (mParam->fourPort ? 2 : 1);
    if (mParam->refSizeEvenY <= 0)
      mParam->moduloY = (mParam->fourPort ? 2 : 1);
    mExpectedX = (mExpectedX / mParam->moduloX) * mParam->moduloX;
    mExpectedY = (mExpectedY / mParam->moduloY) * mParam->moduloY;
  }

  mFrameCount = mParam->gainRefFrames;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "ACQUIRING GAIN REF");
  mWinApp->SetStatusText(MEDIUM_PANE, "FINDING TARGET");
  mPreparingGainRef = true;

  mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, AcquiringRefDone,
    AcquiringRefError, REF_FIRST_SHOT, 0);

}


void CGainRefMaker::AcquiringRefNextTask(int param)
{
  CString paneStr;
  int nx, ny, binning, i, j, timeout, outSizeX, outSizeY, top, left, bottom, right;
  float curMean, mean, binRat, exposure, targetExposure, minVal;
  double sum = 0.;
  double sum2 = 0.;
  double tsum;
  float *arrpt, *inpt;
  unsigned short int *usdata;
  short int *sdata;

  mImBufs->mCaptured = BUFFER_CALIBRATION;
  if (!mFrameCount)
    return;

  KImage *image = mImBufs->mImage;

  switch (param) {
  case REF_FIRST_SHOT:
  
    curMean = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs);
    if (curMean < 5) {
      BeamTooWeak();
      return;
    }
    targetExposure = mConSet->exposure * mParam->gainRefTarget / curMean;
    if (targetExposure > EXPOSURE_MAX)
      targetExposure = EXPOSURE_MAX;
  
    // Find a binning that is big but will keep the exposure above 0.1 sec
    binning = mConSet->binning;
    exposure = targetExposure;
    for (i = 1; i < mParam->numBinnings; i++) {
      binRat = (float)mConSet->binning / (float)mParam->binnings[i];
      if (targetExposure * binRat * binRat > 0.1) {
        exposure = targetExposure * binRat * binRat;
        binning = mParam->binnings[i];
      } else
        break;
    }

    SEMTrace('r', "First shot exposure %.3f, mean %.1f; target exposure %.3f\r\n"
      " second shot exposure %.3f at binning %d", mConSet->exposure, curMean, 
      targetExposure, exposure, binning);
    mConSet->left = 0;
    mConSet->right = mParam->sizeX;
    mConSet->top = 0;
    mConSet->bottom = mParam->sizeY;
    mConSet->exposure = exposure;
    mConSet->binning = binning;
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, AcquiringRefDone, 
      AcquiringRefError, REF_SECOND_SHOT, 0);
    return;

  case REF_SECOND_SHOT:
  
    // Binned image: adjust the exposure again for the results
    curMean = (float)mWinApp->mProcessImage->WholeImageMean(mImBufs) * 
      mWinApp->GetGainFactor(mCurrentCamera, mParam->gainRefBinning) /
      mWinApp->GetGainFactor(mCurrentCamera, mConSet->binning);
    binRat = (float)mConSet->binning / (float)mParam->gainRefBinning;
    mConSet->exposure *= binRat * binRat * mParam->gainRefTarget / curMean;

    SEMTrace('r', "Second shot mean %.1f; final exposure %.3f", curMean,
      mConSet->exposure);
    if (mConSet->exposure > EXPOSURE_MAX) {
      BeamTooWeak();
      return;
    }

    mConSet->binning = mParam->gainRefBinning;
    mConSet->forceDark = 0;
    mConSet->onceDark = 1;
    mConSet->numAverage = mParam->gainRefNumDarkAvg;
    mConSet->averageOnce = mParam->gainRefAverageDark;
    if (mWinApp->mDEToolDlg.CanSaveFrames(mParam) && mParam->gainRefSaveRaw) {
      mConSet->saveFrames = DE_SAVE_SUMS;   // This will save a sum of all frames
      mConSet->DEsumCount = 0;
    }
    timeout = 120000 * (1 + (mConSet->averageOnce ? mConSet->numAverage : 1));
    mTakingRefImages = true;
    break;

  case REAL_FRAME_SHOT:

    // Check the image size to avoid problems on summing
    nx = image->getWidth();
    ny = image->getHeight();
    if (nx != mExpectedX || ny != mExpectedY) {
      AfxMessageBox("The acquired image is not the expected size\n\n"
        "Gain reference aborted.", MB_EXCLAME);
      StopAcquiringRef();
      return;
    }

    // Add the data into the real array
    image->Lock();
    switch (image->getType()) {
    case kSHORT:
      sdata = (short int *)image->getData();
      for (i = 0; i < nx * ny; i++)
        mArray[i] += sdata[i];
      break;

    case kUSHORT:
      usdata = (unsigned short int *)image->getData();
      for (i = 0; i < nx * ny; i++)
        mArray[i] += usdata[i];
      break;
    }
    image->UnLock();
    mFrameCount--;
    timeout = 120000;
    break;

  }

  // Get next frame unless done
  if (mFrameCount) {
    paneStr.Format("%.1f sec exp., %d frames left", mConSet->exposure, mFrameCount);
    mWinApp->SetStatusText(MEDIUM_PANE, paneStr);
    mCamera->InitiateCapture(TRACK_CONSET);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, AcquiringRefDone, 
      AcquiringRefError, REAL_FRAME_SHOT, 0);
    return;
  }
                                            
  // Try to calibrate the dose if selected and binning <= 2
  if (mCalibrateDose && mParam->countsPerElectron > 0. && mParam->gainRefBinning <= 2)
    mWinApp->mBeamAssessor->CalibrateElectronDose(false);

  // Divide array by its mean
  for (i = 0; i < nx * ny; i++)
    sum += mArray[i];
  arrpt = mArray;
  for (i = 0; i < ny; i++) {
    tsum = 0.;
    for (j = 0; j < nx; j++)
      tsum += *arrpt++;
    sum2 += tsum;
  }

  mean = (float)(sum2 / (nx * ny));
  SEMTrace('r', "quick sum %.0f  careful sum %.0f  mean %.1f", sum, sum2, mean);
  minVal = mean / GAINREF_MAX_VAL;
  for (i = 0; i < nx * ny; i++)
    mArray[i] = (mArray[i] > minVal) ? mean / mArray[i] : GAINREF_MAX_VAL;

  // Repack the block-Tietz reference and pad with 1's
  if (mParam->TietzType && mParam->TietzBlocks) {
    outSizeX = mParam->sizeX / mParam->gainRefBinning;
    outSizeY = mParam->sizeY / mParam->gainRefBinning;
    arrpt = mArray + outSizeX * outSizeY  - 1;
    inpt = mArray + nx * ny - 1;
    mCamera->GetLastCoords(top, left, bottom, right);
    for (j = outSizeY; j > bottom; j--)
      for (i = 0; i < outSizeX; i++)
        *arrpt-- = 1.;
    for (j = bottom; j > top; j--) {
      for (i = outSizeX; i > right; i--)
        *arrpt-- = 1.;
      for (i = right; i > left; i--)
        *arrpt-- = *inpt--;
      for (i = left; i > 0; i--)
        *arrpt-- = 1.;
    }
    for (j = top; j > 0; j--)
      for (i = 0; i < outSizeX; i++)
        *arrpt-- = 1.;
    nx = outSizeX;
    ny = outSizeY;
  }

  // Save the image to file
  KImageFloat *fImage = new KImageFloat();
  if (!fImage) {
    StopAcquiringRef();
    return;
  }
  fImage->useData((char *)mArray, nx, ny);
  int err = mStoreMRC->AppendImage((KImage *)fImage);
  fImage->detachData();
  delete fImage;
  if (err) {
      AfxMessageBox("Error saving gain reference to file\n\n" +
        mBackedUp ? "The previous reference is being restored." : "", MB_EXCLAME);
      StopAcquiringRef();
      return;
  }

  mBackedUp = false;

  // Eliminate previous gain references; eliminate binned by 2 if this is unbinned
  if (mGainRef[mCurrentCamera][mRefBinInd])
    delete mGainRef[mCurrentCamera][mRefBinInd];
  if (!mRefBinInd && mGainRef[mCurrentCamera][1]) {
    delete mGainRef[mCurrentCamera][1];
    mGainRef[mCurrentCamera][1] = NULL;
  }
  mCamera->DeleteReferences(GAIN_REFERENCE, false);

  // Clear flag(s) for using DM gain reference instead
  mUseDMRef[mCurrentCamera][mRefBinInd] = 0;
  if (!mRefBinInd)
    mUseDMRef[mCurrentCamera][1] = 0;

  // Try to convert the data, and put data in storage places
  usdata = NULL;
  if (mCamera->GetScaledGainRefMax())
    NewArray(usdata,unsigned short,nx * ny);
  if (ProcConvertGainRef(mArray, usdata, nx * ny, mCamera->GetScaledGainRefMax(),
    mCamera->GetMinGainRefBits(), &mGainRefBits[mCurrentCamera][mRefBinInd])) {
    if (usdata)
      delete [] usdata;
    mGainRef[mCurrentCamera][mRefBinInd] = mArray;
    mByteSize[mCurrentCamera][mRefBinInd] = 4;
    mArray = NULL;
  } else {
     mGainRef[mCurrentCamera][mRefBinInd] = usdata;
     mByteSize[mCurrentCamera][mRefBinInd] = 2;
  }

  mRefExists[mCurrentCamera][mRefBinInd] = true;
  mRefTime[mCurrentCamera][mRefBinInd] = CTime::GetCurrentTime();
  mCamera->ShowReference(NEW_GAIN_REFERENCE + mParam->gainRefBinning);
  AfxMessageBox("The gain reference has been\r\n"
      "successfully acquired and saved.", MB_EXCLAME);
  StopAcquiringRef();
}

void CGainRefMaker::AcquiringRefError(int error)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mGainRefMaker->ErrorCleanup(error);
}

void CGainRefMaker::ErrorCleanup(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    AfxMessageBox(_T("Time out acquiring gain reference"), MB_EXCLAME);
  StopAcquiringRef();
  mWinApp->ErrorOccurred(error);
}

void CGainRefMaker::AcquiringRefDone(int param)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mGainRefMaker->AcquiringRefNextTask(param);
}

void CGainRefMaker::BeamTooWeak()
{
  AfxMessageBox("The required exposure time is too high\n\n"
    "Check the beam and increase beam intensity\n or reduce the target counts",
    MB_ICONEXCLAMATION);
  StopAcquiringRef();
}

void CGainRefMaker::StopAcquiringRef()
{
  CFileStatus status;
  mFrameCount = 0;
  mTakingRefImages = false;
  mPreparingGainRef = false;
  if (mArray)
    delete mArray;
  mArray = NULL;
  if (mStoreMRC)
    delete mStoreMRC;
  mStoreMRC = NULL;
  if (mBackedUp) {
    UtilRenameFile(mBackupName, mFileName, 
      "Error attempting to restore backup file or previous reference");
    mBackedUp = false;
  }
  mConSet->saveFrames = 0;
  mParam->moduloX = mModuloSaveX;
  mParam->moduloY = mModuloSaveY;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(COMPLEX_PANE, "");
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

// Look up the existing references for the current camera if it hasn't happened before
void CGainRefMaker::FindExistingReferences()
{
  CFileStatus status;
  CString name;
  BOOL ok;

  if(mSearchedRefs)
    return;

  SEMTrace('r', "Looking for existing references");
  for (int i = 0; i < mNumBinnings[mCurrentCamera]; i++) {
    name = ComposeRefName(mParam->binnings[i]);
    ok = CFile::GetStatus((LPCTSTR)name, status);
    if ((mRefExists[mCurrentCamera][i] = (ok && status.m_size > 1024)))
      mRefTime[mCurrentCamera][i] = status.m_mtime;
  }
  mSearchedRefs = true;
}

// Compose a name for the reference for a given camera
CString CGainRefMaker::ComposeRefName(int binning)
{
  CString name = mParam->name;
  CString kvStr;
  name.Replace(' ', '_');
  if (mHTValue) {
    kvStr.Format("-%d", mHTValue);
    name += kvStr;
  }
  kvStr.Format("x%d.ref", binning);
  name += kvStr;
  if (!mRefPath.IsEmpty())
    name = mRefPath + "\\" + name;
  return name;
}

//////////////////////////////////////////////////////////////////////////////////////
// GET A REFERENCE AT THE GIVEN BINNING.  Returns the reference in gainRef, properties
// in  byteSize and gainRefBits, and ownership 1 if the caller is responsible for
// deleting it.  Returns 0 for success, 1 if a reference could be sought from DM,
// or -1 if at a different KV
int CGainRefMaker::GetReference(int binning, void *&gainRef, int &byteSize, 
                                int &gainRefBits, int &ownership, int xOffset, 
                                int yOffset)
{
  int needInd = -1, needBin, errval;
  KStoreMRC *storeMRC;
  CFile *file;
  KImageFloat *image;
  int i, nx, ny, moreBin, binnedSize, scaleBits, modulo;
  int nxBinned, nyBinned;
  unsigned short int *usdata;
  short int *sdata;
  CString report;

  mCurrentCamera = mWinApp->GetCurrentCamera(); 
  mParam = mWinApp->GetCamParams() + mCurrentCamera;  
  mCamera = mWinApp->mCamera;
  mDMind = mNumBinnings[mCurrentCamera];
  mSearchedRefs = false;

  CheckChangedKV();
  errval = mHTValue ? -1 : 1;

  // Check for HIGHER BINNED REFERENCE unless ignoring - first find matching binning
  if (binning > 2 && !mIgnoreHigherRefs && !(xOffset || yOffset)) {
    for (i = 0; i < mNumBinnings[mCurrentCamera]; i++) {
      if (binning == mParam->binnings[i]) {

        // Check for whether to use DM reference, return if get positive indication
        // Otherwise go on to binned/unbinned selection
        if (CheckDMReference(i, true)) {
          if (mUseDMRef[mCurrentCamera][i] > 0)
            return errval;
        } else {

          // Seek out own reference: if not loaded, make sure it exists, and if it
          // exists, set up to use it
          if (!mGainRef[mCurrentCamera][i])
            FindExistingReferences(); 
          if (mRefExists[mCurrentCamera][i])
            needInd = i;
        }
      }
    }
  }

  // Now go through binning 1 vs. 2 logic if not set for higher binned ref
  if (needInd < 0) {

    // ODD BINNING with no binning 1, needs a reference from DM
    if (mParam->binnings[0] > 1 && binning % 2)
      return errval;

    // ODD BINNING needs unbinned reference regardless; binning offset can be used to
    // force even binning to use it also; first reference (bin 2 but index 0) is also 
    // needed if there is no binning 1
    if (binning % 2 || NeedsUnbinnedRef(binning) || mParam->binnings[0] > 1 || xOffset ||
      yOffset) {
      needInd = 0;
      if (CheckDMReference(needInd, false))
        return errval;

      // If not loaded, make sure it exists
      if (!mGainRef[mCurrentCamera][0]) {
        FindExistingReferences();
        if (!mRefExists[mCurrentCamera][0])
          return errval;
      }

    // EVEN BINNING AND BIN 1 EXISTS
    // If the binned reference is loaded, it must be younger or preferred so use it
    // And if it is already identified that DM ref should be used, use it
    } else if (mGainRef[mCurrentCamera][1] || mUseDMRef[mCurrentCamera][1] > 0)
      needInd = 1;

    // If the unbinned reference is loaded or it is already identified that DM ref should
    // be used, and either it is the only one that exists, or it is younger than bin x 2
    // and the bin x2 is not preferred, then use it
    else if ((mGainRef[mCurrentCamera][0] || mUseDMRef[mCurrentCamera][0] > 0) && 
      (!mRefExists[mCurrentCamera][1] ||
      (mRefTime[mCurrentCamera][0] > mRefTime[mCurrentCamera][1] && !mUseOlderBinned2)))
      needInd = 0;

    // Remaining case: the binned reference is not loaded, and either the unbinned
    // reference is not loaded or it is older than binned or not preferred to it
    // need to evaluate current state
    else {
      FindExistingReferences();

      // See if user selects to use DM ref with even binnings
      if (CheckDMReference(1, true) && mUseDMRef[mCurrentCamera][1] > 0)
        return errval;

      // Again, unbinned exists and either the other does not or is older and not
      // preferred, use unbinned
      if (mRefExists[mCurrentCamera][0] && (!mRefExists[mCurrentCamera][1] ||
        (mRefTime[mCurrentCamera][0] > mRefTime[mCurrentCamera][1] && 
        !mUseOlderBinned2)))
        needInd = 0; 

      // Otherwise, if binned ref exists, use it
      else if (mRefExists[mCurrentCamera][1])
        needInd = 1;

      // If neither exists, can still use unbinned if it is loaded!
      else if (mGainRef[mCurrentCamera][0])
        needInd = 0;

      // Out of luck
      else 
        return errval;
    }

    // After all of that logic, check if DM ref should get used instead
    if (CheckDMReference(needInd, false))
      return errval;
  }

  needBin = mParam->binnings[needInd];

  // Enforce even sizes for 4-port camera, and apply modulo in addition if called for
  modulo = mParam->fourPort ? 2 : 1;
  i = mParam->refSizeEvenX ? mParam->moduloX : modulo;
  nx = ((mParam->sizeX / needBin) / i) * i;
  i = mParam->refSizeEvenY ? mParam->moduloY : modulo;
  ny = ((mParam->sizeY / needBin) / i) * i;

  // Load the needed gain reference if it is  not
  if (!mGainRef[mCurrentCamera][needInd]) {
    SEMTrace('r', "Loading gain reference for binning %d", needBin);
    mFileName = ComposeRefName(needBin);  
    try {
      file = new CFile(mFileName, CFile::modeRead |CFile::shareDenyWrite);
    }
    catch (CFileException *err) {
      AfxMessageBox("Cannot open gain reference file:\n" + mFileName, MB_EXCLAME);
      err->Delete();
      return errval;
    }
    if (!KStoreMRC::IsMRC(file)) {
      AfxMessageBox("Gain reference file does not appear to be MRC:\n" + mFileName, 
        MB_EXCLAME);
      delete file;
      return errval;
    }

    storeMRC = new KStoreMRC(file);
    if (storeMRC->getWidth() != nx || storeMRC->getHeight() != ny) {
      AfxMessageBox("Image in gain reference file is not the right size:\n" + mFileName,
        MB_EXCLAME);
      delete storeMRC;
      return errval;
    }

    storeMRC->setSection(0);
    image = (KImageFloat *)storeMRC->getRect();      
    delete storeMRC;
    if (!image) {
      AfxMessageBox("Error reading gain reference file:\n" + mFileName, MB_EXCLAME);
      return errval;
    }

    // Convert to unsigned short if possible
    image->Lock();
    usdata = NULL;
    if (mCamera->GetScaledGainRefMax())
      NewArray(usdata,unsigned short int,nx * ny);
    if (ProcConvertGainRef((float *)image->getData(), usdata, nx * ny, 
      mCamera->GetScaledGainRefMax(), mCamera->GetMinGainRefBits(), 
      &mGainRefBits[mCurrentCamera][needInd])) {

      // error in conversion: delete int array and detach float array from image
      if (usdata)
        delete [] usdata;
      mGainRef[mCurrentCamera][needInd] = image->getData();
      mByteSize[mCurrentCamera][needInd] = 4;
      image->detachData();
    } else {
      mGainRef[mCurrentCamera][needInd] = usdata;
      mByteSize[mCurrentCamera][needInd] = 2;
    }

    delete image;
  }

  // All set if the binning matches: return array without passing ownership
  byteSize = mByteSize[mCurrentCamera][needInd];
  gainRefBits = mGainRefBits[mCurrentCamera][needInd];
  if (needBin == binning) {
    gainRef = mGainRef[mCurrentCamera][needInd];
    ownership = 0;
    SEMTrace('r', "Returning resident gain ref binning %d  bytesize %d", needBin,
        byteSize);
    return 0;
  }

  // Otherwise need to bin it down; get the needed size
  moreBin = binning / needBin;
  modulo = 1;

  nxBinned = nx / moreBin;
  nyBinned = ny / moreBin;
  if (mParam->fourPort || mParam->refSizeEvenX)
    nxBinned = 2 * ((nx / 2) / moreBin);
  if (mParam->fourPort || mParam->refSizeEvenY)
    nyBinned = 2 * ((ny / 2) / moreBin);

  // Get offsets for these binnings unless already defined
  if (!xOffset && !yOffset)
    GetBinningOffsets(mParam, needBin, binning, xOffset, yOffset);

  // get an array to bin into
  binnedSize = nxBinned * nyBinned;
  NewArray(sdata,short int,binnedSize *(byteSize / 2));
  if (!sdata) {
    AfxMessageBox("Failed to get memory for binned gain reference", MB_EXCLAME);
    return errval;
  }

  // Bin it down
  XCorrBinInverse(mGainRef[mCurrentCamera][needInd], byteSize == 4 ? kFLOAT : kUSHORT,
    nx, ny, xOffset, yOffset, moreBin, nxBinned, nyBinned, sdata);
  
  // If it was shorts, we are all set; return the scaling factor too
  if (byteSize == 2) {
    gainRef = sdata;
    gainRefBits = mGainRefBits[mCurrentCamera][needInd];

  } else {

    // Otherwise, need to try to scale it down as usual
    usdata = NULL;
    if (mCamera->GetScaledGainRefMax())
      NewArray(usdata,unsigned short int,binnedSize);
    if (ProcConvertGainRef((float *)sdata, usdata, binnedSize, 
      mCamera->GetScaledGainRefMax(), mCamera->GetMinGainRefBits(), &scaleBits)) {

      // Failure: delete int array and return floats
      if (usdata)
        delete [] usdata;
      gainRef = sdata;
    } else {

      // Success: return ints and the scaling factor
      byteSize = 2;
      gainRef = usdata;
      gainRefBits = scaleBits;
      delete [] sdata;
    }
  }

  // Return the array and pass ownership to controller
  SEMTrace('r', "Passing on gain ref with binning %d, bytesize %d, based "
    "on binning %d, offsets %d %d", binning, byteSize, needBin, xOffset, yOffset);
  ownership = 1;
  return 0;
}


// Check the status of a DM gain reference, compare to times of our own gain
// references, and if necessary ask user whether to use a newer DM reference
BOOL CGainRefMaker::CheckDMReference(int binInd, BOOL optional)
{
  CString name;

  if (!mParam->GatanCam)
    return false;

  // Try not checking if we have already selected to use this reference
  if (mUseDMRef[mCurrentCamera][binInd] > 0)
    return true;

  // First check the DM reference itself if is not
  if (!mDMRefChecked[mCurrentCamera]) {
    UpdateDMReferenceTimes();
    mDMRefChecked[mCurrentCamera] = true;
  }

  // If the reference does not exist or the flag is not set to assess, then done
  if (!mRefExists[mCurrentCamera][mDMind] || !mUseDMRef[mCurrentCamera][binInd]) {
    mUseDMRef[mCurrentCamera][binInd] = 0;
    return false;
  }

  // If SEM reference does not exist, try to find, and if it still doesn't, just return
  if (!mRefExists[mCurrentCamera][binInd])
    FindExistingReferences();
  if (!mRefExists[mCurrentCamera][binInd]) {

    // If we must have this reference, set up to use DM
    if (!optional) {
      mUseDMRef[mCurrentCamera][binInd] = 1;
      return true;
    }

    // Otherwise, if always asking, then ask for this binning
    if (mDMrefAskPolicy == DMREF_ASK_ALWAYS) {
      name.Format("Do you want to use the DigitalMicrograph gain reference\n"
        "instead of a less binned SerialEM gain reference for binning %d?", 
        mParam->binnings[binInd]);
      if (AfxMessageBox(name, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        mUseDMRef[mCurrentCamera][binInd] = 1;
        return true;
      }
    }

    // Otherwise it is optional so let selector go on
    return false;
  }

  // At this point the default is to use an SEM reference
  mUseDMRef[mCurrentCamera][binInd] = 0;

  // If the SEM reference is newer than the DM reference, or not asking, just use it
  if ((mRefTime[mCurrentCamera][binInd] > mRefTime[mCurrentCamera][mDMind] && 
    mDMrefAskPolicy == DMREF_ASK_IF_NEWER) || mDMrefAskPolicy == DMREF_ASK_NEVER)
    return false;

  // Finally, get the user choice of what to do if DM is newer or always asking
  if (mDMrefAskPolicy == DMREF_ASK_ALWAYS) {
    name.Format("Do you want to use the DigitalMicrograph gain reference\n"
      "instead of the SerialEM gain reference for binning %d?", 
      mParam->binnings[binInd]);
  } else {
    name = "The unbinned";
    if (binInd)
      name.Format("The binned by %d", mParam->binnings[binInd]);
    name += " SerialEM gain "
      "reference is older\n than the DigitalMicrograph gain reference\n\n"
      "Do you want to use the newer DigitalMicrograph gain reference?";
  }
  if (AfxMessageBox(name, MB_YESNO | MB_ICONQUESTION) == IDYES) {

    // If they want to use the DM reference, clear out SEM reference
    mUseDMRef[mCurrentCamera][binInd] = 1;
    if (mGainRef[mCurrentCamera][binInd])
      delete mGainRef[mCurrentCamera][binInd];
    mGainRef[mCurrentCamera][binInd] = NULL;
  }

  return mUseDMRef[mCurrentCamera][binInd] > 0;
}

// Looks for references to determine their existence and time and issues various messages
void CGainRefMaker::UpdateDMReferenceTimes(void)
{
  CFileStatus status;
  CString name;
  CString refPath = mDMRefPath;
  CTime cmtime;
  __time64_t mtime;
  BOOL exists;
  int i;
  if (mParam->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID) && 
    !mRemoteRefPath.IsEmpty())
    refPath = mRemoteRefPath;

  // If the names aren't there or we are at different KV, existence will never be set
  if (!refPath.IsEmpty() && !mParam->DMRefName.IsEmpty() && !mHTValue) {
    name = refPath + "\\" + mParam->DMRefName;
    SEMTrace('r', "Looking for DM gain reference");

    // Look for counting reference for K2
    if (mParam->K2Type)
      name = mCamera->MakeFullDMRefName(mParam, ".m2.");
    if (mParam->useSocket && CBaseSocket::ServerIsRemote(GATAN_SOCK_ID)) {
      i = mWinApp->mGatanSocket->CheckReferenceTime((LPCTSTR)name, &mtime);
      exists = i == 0;
      if (i < 0 && !mWarnedNoDM[mCurrentCamera]) {
        name.Format("WARNING: An error occurred (%d) trying to find out about the "
          "DigitalMicrograph\r\n  gain reference file  %s on the remote socket server",
          i, (LPCTSTR)mParam->DMRefName);
        mWinApp->AppendToLog(name);
        mWarnedNoDM[mCurrentCamera] = true;
      }
      if (exists) {
        try {
          cmtime = CTime(mtime);
        }
        catch (...) {
          exists = FALSE;
        }
      }

    } else {
      exists = CFile::GetStatus((LPCTSTR)name, status);
      if (exists)
        cmtime = status.m_mtime;
    }
    if (exists) {

      // The reference exists.  Set that each binning needs checking if it
      // did not exist before or if it is newer than last time we saw it
      if (!mRefExists[mCurrentCamera][mDMind] || 
        mRefTime[mCurrentCamera][mDMind] < cmtime) {
          for (i = 0; i < mNumBinnings[mCurrentCamera]; i++)
            mUseDMRef[mCurrentCamera][i] = -1;
      }

      // Set that it exists and record the time
      mRefExists[mCurrentCamera][mDMind] = true;
      mRefTime[mCurrentCamera][mDMind] = cmtime;
    } else {

      // If not OK, need to clear existence flag
      mRefExists[mCurrentCamera][mDMind] = false;
      if (!mWarnedNoDM[mCurrentCamera]) {
        name.Format("WARNING: Did not find DigitalMicrograph gain reference file:   %s"
          "\r\n    in folder:   %s\r\n    Are the name and location correct in the "
          "SerialEM properties file?", (LPCTSTR)mParam->DMRefName,(LPCTSTR)refPath);
        mWinApp->AppendToLog(name);
        mWarnedNoDM[mCurrentCamera] = true;
      }
    }
  }
}

// Returns true if the reference for the current camera is new
bool CGainRefMaker::IsDMReferenceNew(int camNum)
{
  CString str;
  mCurrentCamera = camNum;
  mParam = mWinApp->GetCamParams() + mCurrentCamera;
  mDMind = mNumBinnings[mCurrentCamera];
  BOOL didExist = mRefExists[mCurrentCamera][mDMind];
  CTime oldTime = mRefTime[mCurrentCamera][mDMind];
  UpdateDMReferenceTimes();
  str.Format("IsDMReferenceNew: didexist %d exist %d  times", didExist ? 1 : 0, 
    mRefExists[camNum][mDMind] ? 1:0);
  str += oldTime.Format(" %B %d, %Y %H:%M:%S") + "   " + 
    mRefTime[camNum][mDMind].Format("%B %d, %Y %H:%M:%S");
  SEMTrace('r', "%s", (LPCTSTR)str);

  // This does not really need to be restored...
  mCurrentCamera = mWinApp->GetCurrentCamera(); 
  return (mRefExists[camNum][mDMind] && (!didExist || mRefTime[camNum][mDMind] > oldTime));
}

// Return the binning offsets for the given camera, for going from a reference
// binning to the given binning
void CGainRefMaker::GetBinningOffsets(CameraParameters *param, int refBinning, 
                                      int binning, int &xOffset, int &yOffset)
{
  int i;
  int nx = param->sizeX / refBinning;
  int ny = param->sizeY / refBinning;
  int moreBin = binning / refBinning;
  xOffset = 0;
  yOffset = 0;

  // Start with built-in offset for 4-port camera, override if properties entered
  if (param->fourPort && !(param->TietzType && param->TietzBlocks)) {
    xOffset = (nx / 2) % moreBin;
    yOffset = (ny / 2) % moreBin;
  }
  for (i = 0; i < param->numBinnedOffsets; i++) {
    if (param->offsetRefBinning[i] == refBinning && param->offsetBinning[i] == binning) {
      xOffset = param->binnedOffsetX[i];
      yOffset = param->binnedOffsetY[i];
      break;
    }
  }
}

// Check for a change in the KV setting and re-initialize if it changes
void CGainRefMaker::CheckChangedKV(void)
{
  // If there is a list of KVs at which to keep a separate gain reference, 
  // First get a KV value if interval has expired, then check if close to one on list
  double curKV;
  int newHT;
  bool wasRead;
  mCamera = mWinApp->mCamera;
  if (mNumRefKVs > 0) {
    curKV = mWinApp->mProcessImage->GetRecentVoltage(&wasRead);
    if (wasRead) {
      SEMTrace('r', "Checking for change in KV");

      // Evaluate whether at separate KV value, and get non-zero value if so
      newHT = 0;
      for (int i = 0; i < mNumRefKVs; i++) {
        if (mRefKVList[i] > 0 && fabs(curKV - mRefKVList[i]) / mRefKVList[i] < 0.025) {
          newHT = mRefKVList[i];
          break;
        }
      }

      // If the distinct KV value has changed, clear out all reference information
      if (mHTValue != newHT) {
        SEMTrace('r', "Clearing out references for KV change");
        mHTValue = newHT;
        DeleteReferences();
        InitializeRefArrays();
        mCamera->DeleteReferences(GAIN_REFERENCE, false);
      }
    }
  }
}

// Add up the memory usage of all the resident references
double CGainRefMaker::MemoryUsage(void)
{
  CameraParameters *param = mWinApp->GetCamParams();
  double size, sum = 0;
  for (int i = 0; i < MAX_CAMERAS; i++) {
    for (int j = 0; j < 2; j++) {
      if (mGainRef[i][j]) {
        size = mByteSize[i][j] * param[i].sizeX * param[i].sizeY / (j ? 4 : 1);
        sum += size;
        //SEMTrace('1', "Ref Maker: cam %d binind %d size %.2f", i, j, size / 1.e6);
      }
    }
  }
  return sum;
}

// Test binning offsets  to see if this binning can't use binned x 2 ref
BOOL CGainRefMaker::NeedsUnbinnedRef(int binning)
{
  int xoffset, yoffset;
  if (binning < 2)
    return true;
  GetBinningOffsets(mParam, 2, binning, xoffset, yoffset);
  return (xoffset > 900 || yoffset > 900);
}
