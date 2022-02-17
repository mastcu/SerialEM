// ProcessImage.cpp:      Mostly has functions in the Process menu, also has
//                          code for getting image means and for correcting 
//                          image defects
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include ".\ProcessImage.h"
#include "Utilities\XCorr.h"
#include "Shared\b3dutil.h"
#include "Shared\mrcslice.h"
#include "Shared\ctffind.h"
#include "Utilities\KGetOne.h"
#include "SerialEMView.h"
#include "SerialEMDoc.h"
#include "MultiTSTasks.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "BeamAssessor.h"
#include "CameraController.h"
#include "EMscope.h"
#include "NavigatorDlg.h"
#include "GainRefMaker.h"
#include "CtffindParamDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define KV_CHECK_SECONDS 60.
static void ctffindPrintFunc(const char *strMessage);
static int ctffindDumpFunc(const char *filename, float *data, int xsize, int ysize);

/////////////////////////////////////////////////////////////////////////////
// CProcessImage

// IMPLEMENT_DYNCREATE(CProcessImage, CCmdTarget)

CProcessImage::CProcessImage()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mInnerXRayDistance = 1.2f;
  mOuterXRayDistance = 2.1f;
  mXRayCritIterations = 3;
  mXRayCritIncrease = 1.;
  mLiveFFT = false;
  mMoveBeamStamp = -1.;
  mCircleOnLiveFFT = false;
  mSideBySideFFT = false;
  mAutoSingleFFT = true;
  mNumCircles = 1;
  mFFTCircleRadii[0] = 0.33f;
  mGridLinesPerMM = 2160;
  mOverlayChannels = "ABA";
  mPixelTargetSize = 0;
  mFindBeamOutsideFrac = 0.15f;
  mCatalaseForPixel = false;
  mShortCatalaseNM = 6.85f;
  mLongCatalaseNM = 8.75f;
  mGridMeshSize = 0;
  mPixelTimeStamp = 0;
  mPlatePhase = 0.;
  mNumFFTZeros = 5;
  mAmpRatio = 0.07f;
  mSphericalAber = 2.5;   // It is very insensitive to Cs!
  mKVTime = -1.;
  mFixedRingDefocus = 0.;
  mReductionFactor = 4.;
  mCtffindOnClick = false;
  mCtfFitFocusRangeFac = 2.5f;
  mSlowerCtfFit = true;
  mExtraCtfStats = false;
  mDrawExtraCtfRings = false;
  mUserMaxCtfFitRes = 0.;
  mDefaultMaxCtfFitRes = 0.;
  mTestCtfPixelSize = 0.;
  mMinCtfFitResIfPhase = 0.;
  mCtffindParamDlg = NULL;
  mCtffindDlgPlace.rcNormalPosition.right = 0;
  mCtfMinPhase = 30;
  mCtfMaxPhase = 120;
  mCtfFindPhaseOnClick = false;
  mCtfFixAstigForPhase = true;
  ctffindSetPrintFunc(ctffindPrintFunc);
  ctffindSetSliceWriteFunc(ctffindDumpFunc);
}

CProcessImage::~CProcessImage()
{
}

void CProcessImage::Initialize(void)
{
  mShiftManager = mWinApp->mShiftManager;
  mCamera = mWinApp->mCamera;
  mScope = mWinApp->mScope;
  if (!mDefaultMaxCtfFitRes)
    mDefaultMaxCtfFitRes = GetRecentVoltage() > 125. ? 5.f : 10.f;
}


BEGIN_MESSAGE_MAP(CProcessImage, CCmdTarget)
  //{{AFX_MSG_MAP(CProcessImage)
  ON_COMMAND(ID_PROCESS_MINMAXMEAN, OnProcessMinmaxmean)
  ON_COMMAND(ID_PROCESS_FFT, OnProcessFft)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_FFT, OnUpdateProcess)
  ON_COMMAND(ID_PROCESS_BINNEDFFT, OnProcessBinnedfft)
  ON_COMMAND(ID_PROCESS_ROTATELEFT, OnProcessRotateleft)
  ON_COMMAND(ID_PROCESS_ROTATERIGHT, OnProcessRotateright)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_MINMAXMEAN, OnUpdateProcessMinmaxmean)
  ON_COMMAND(ID_PROCESS_ZEROTILTMEAN, OnProcessZerotiltmean)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_ZEROTILTMEAN, OnUpdateProcessZerotiltmean)
  ON_COMMAND(ID_PROCESS_SETINTENSITY, OnProcessSetintensity)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_SETINTENSITY, OnUpdateProcessSetintensity)
  ON_COMMAND(ID_PROCESS_MOVEBEAM, OnProcessMovebeam)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_MOVEBEAM, OnUpdateProcessMovebeam)
  ON_COMMAND(ID_PROCESS_FIXDARKXRAYS, OnProcessFixdarkxrays)
  ON_COMMAND(ID_PROCESS_FIXIMAGEXRAYS, OnProcessFiximagexrays)
  ON_COMMAND(ID_PROCESS_SETDARKCRITERIA, OnSetdarkcriteria)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_SETDARKCRITERIA, OnUpdateSetcriteria)
  ON_COMMAND(ID_PROCESS_SETIMAGECRITERIA, OnSetimagecriteria)
	ON_COMMAND(ID_PROCESS_SHOWCROSSCORR, OnProcessShowcrosscorr)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_BINNEDFFT, OnUpdateProcess)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_ROTATELEFT, OnUpdateProcess)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_ROTATERIGHT, OnUpdateProcess)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_FIXDARKXRAYS, OnUpdateProcessNoRGB)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_FIXIMAGEXRAYS, OnUpdateProcessNoRGB)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_SETIMAGECRITERIA, OnUpdateSetcriteria)
	ON_UPDATE_COMMAND_UI(ID_PROCESS_SHOWCROSSCORR, OnUpdateProcessShowcrosscorr)
	ON_UPDATE_COMMAND_UI(ID_PROCESS_AUTOCORRELATION, OnUpdateProcessAutocorrelation)
	//}}AFX_MSG_MAP
  ON_COMMAND(ID_PROCESS_LIVEFFT, OnProcessLivefft)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_LIVEFFT, OnUpdateProcessLivefft)
  ON_COMMAND(ID_TASKS_CENTERBEAM, OnTasksCenterbeam)
  ON_UPDATE_COMMAND_UI(ID_TASKS_CENTERBEAM, OnUpdateTasksCenterbeam)
  ON_COMMAND(ID_PROCESS_CIRCLEONLIVEFFT, OnProcessCircleonlivefft)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_CIRCLEONLIVEFFT, OnUpdateProcessCircleonlivefft)
  ON_COMMAND(ID_PROCESS_FINDPIXELSIZE, OnProcessFindpixelsize)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_FINDPIXELSIZE, OnUpdateProcessFindpixelsize)
  ON_COMMAND(ID_PROCESS_PIXELSIZEFROMMARKER, OnProcessPixelsizefrommarker)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_PIXELSIZEFROMMARKER, OnUpdateProcessPixelsizefrommarker)
  ON_COMMAND(ID_PROCESS_MAKECOLOROVERLAY, OnProcessMakecoloroverlay)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_MAKECOLOROVERLAY, OnUpdateProcessMakecoloroverlay)
  ON_COMMAND(ID_PROCESS_CROPIMAGE, OnProcessCropimage)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_CROPIMAGE, OnUpdateProcessCropimage)
  ON_COMMAND(ID_PROCESS_LISTRELATIVEROTATIONS, OnListRelativeRotations)
  ON_COMMAND(ID_PROCESS_SETBINNEDSIZE, OnProcessSetBinnedSize)
  ON_COMMAND(ID_PROCESS_AUTOCORRELATION, OnProcessAutocorrelation)
  ON_COMMAND(ID_PIXELSIZE_ADD_TO_ROTATION, OnPixelsizeAddToRotation)
  ON_UPDATE_COMMAND_UI(ID_PIXELSIZE_ADD_TO_ROTATION, OnUpdateProcess)
  ON_COMMAND(ID_TASKS_SET_DOSE_RATE, OnSetDoseRate)
  ON_UPDATE_COMMAND_UI(ID_TASKS_SET_DOSE_RATE, OnUpdateSetDoseRate)
  ON_COMMAND(ID_PIXELSIZE_CATALASE_CRYSTAL, OnPixelsizeCatalaseCrystal)
  ON_UPDATE_COMMAND_UI(ID_PIXELSIZE_CATALASE_CRYSTAL, OnUpdatePixelsizeCatalaseCrystal)
  ON_COMMAND(ID_PROCESS_CROP_AVERAGE, OnProcessCropAverage)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_CROP_AVERAGE, OnUpdateProcessCropAverage)
  ON_COMMAND(ID_PIXELSIZE_MESHFORGRIDBARS, OnMeshForGridBars)
  ON_COMMAND(ID_PROCESS_SIDE_BY_SIDE, OnProcessSideBySide)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_SIDE_BY_SIDE, OnUpdateProcessSideBySide)
  ON_COMMAND(ID_PROCESS_AUTOMATICFFTS, OnProcessAutomaticFFTs)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_AUTOMATICFFTS, OnUpdateProcessAutomaticFFTs)
  ON_COMMAND(ID_PROCESS_SETDEFOCUSFORCIRCLES, OnProcessSetDefocusForCircles)
  ON_COMMAND(ID_PROCESS_REDUCEIMAGE, OnProcessReduceimage)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_REDUCEIMAGE, OnUpdateProcess)
  ON_COMMAND(ID_PROCESS_DOCTFFINDFITONCLICK, OnProcessDoCtffindFitOnClick)
  ON_UPDATE_COMMAND_UI(ID_PROCESS_DOCTFFINDFITONCLICK, OnUpdateProcessDoCtffindFitOnClick)
  ON_COMMAND(ID_PROCESS_SETCTFFINDOPTIONS, OnProcessSetCtffindOptions)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProcessImage message handlers

void CProcessImage::OnProcessMinmaxmean() 
{
  float min, max, mean, sd, pixel;
  int nx, ny, left, right, top, bottom;
  CString report, rep2 = "";

  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  KImage *image = imBuf->mImage;
  CameraParameters *camP = mWinApp->GetCamParams() + imBuf->mCamera;
  if (!image) 
    return;
  EMimageExtra *extra = (EMimageExtra *)image->GetUserData();
  nx = image->getWidth();
  ny = image->getHeight();
  image->Lock();
  if (imBuf->mHasUserLine && imBuf->mDrawUserBox) {
    left = (int)floor((double)B3DMIN(imBuf->mUserPtX, imBuf->mLineEndX) + 1);
    right = (int)ceil((double)B3DMAX(imBuf->mUserPtX, imBuf->mLineEndX) - 1);
    top = (int)floor((double)B3DMIN(imBuf->mUserPtY, imBuf->mLineEndY) + 1);
    bottom = (int)ceil((double)B3DMAX(imBuf->mUserPtY, imBuf->mLineEndY) - 1);
    ProcMinMaxMeanSD(image->getData(), image->getType(), nx, ny, left, right, top, 
      bottom, &mean, &min, &max, &sd);
    rep2.Format("  in %d x %d subarea, (%d,%d) to (%d,%d): ", right + 1 - left, 
      bottom + 1 - top, left, top, right, bottom);
  } else
    ProcMinMaxMeanSD(image->getData(), image->getType(), nx, ny, 0, nx - 1, 0, ny - 1,
      &mean, &min, &max, &sd);
  image->UnLock();
  report.Format("Min = %.1f, max = %.1f, mean = %.2f, SD = %.2f", min, max, mean, sd);
  if (rep2.IsEmpty()) {
    pixel = 1000.f * mShiftManager->GetPixelSize(imBuf);
    if (pixel)
      rep2.Format("    Pixel size = " + mWinApp->PixelFormat(pixel), pixel);
  }
  report += rep2;

  // Add the dose rate for a direct detector
  if (imBuf->mCamera >= 0 && mCamera->IsDirectDetector(camP) &&  
    !DoseRateFromMean(imBuf, mean, pixel)) {
      sd = (float)(mean / (imBuf->mBinning * imBuf->mBinning * extra->mExposure / 
        ((CamHasDoubledBinnings(camP) || 
        (camP->DE_camType && (camP->CamFlags & DE_APOLLO_CAMERA))) ? 4. : 1.)));
      rep2.Format("\r\n    %.3f electrons (%.2f counts) per physical pixel per second", 
        pixel, sd);
      report += rep2;
  }
  mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
}

void CProcessImage::OnUpdateProcessMinmaxmean(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  pCmdUI->Enable(imBuf->mImage != NULL && imBuf->mImage->getMode() == kGray);  
}

// Set up the FFT with the given binning
void CProcessImage::GetFFT(int binning, int capFlag)
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  GetFFT(imBuf, binning, capFlag);
}

void CProcessImage::GetFFT(EMimageBuffer *imBuf, int binning, int capFlag)
{
  KImage *image = imBuf->mImage;
  float *fftarray;
  short int *brray;
  int nx = image->getWidth();
  int ny = image->getHeight();
  int bigBrray = image->getType() == kFLOAT ? 2 : 1;

  // Get padded size of a square frame for image
  int nFinalSize = (nx > ny ? nx : ny) /binning;
  int nPadSize = XCorrNiceFrame(nFinalSize, 2, 19);

  // Get memory for the real FFT and for the scaled int image
  NewArray2(fftarray, float, nPadSize, (nPadSize + 2));
  NewArray2(brray, short int, nFinalSize, nFinalSize * bigBrray);
  if (!fftarray || !brray) {
    SEMMessageBox("Failed to get memory for doing FFT", MB_EXCLAME);
    if (fftarray)
      delete [] fftarray;
    if (brray)
      delete [] brray;
    return;
  }

  image->Lock();
  BeginWaitCursor();
  ProcFFT(image->getData(), image->getType(), nx, ny, binning, fftarray, brray, 
    nPadSize, nFinalSize);
  image->UnLock();
  delete [] fftarray;
  NewProcessedImage(imBuf, brray, kSHORT, nFinalSize, nFinalSize, binning, capFlag, 
    mSideBySideFFT);
  EndWaitCursor();
}

// Unbinned FFT
void CProcessImage::OnProcessFft() 
{
  GetFFT(1, BUFFER_FFT);  
}

void CProcessImage::OnUpdateProcess(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  pCmdUI->Enable(imBuf->mImage && !mWinApp->DoingTasks());  
}

void CProcessImage::OnUpdateProcessNoRGB(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  pCmdUI->Enable(imBuf->mImage && imBuf->mImage->getMode() == kGray && 
    !mWinApp->DoingTasks());  
}

// Bin to 1024 first, but limit binning to between 2 and 4
void CProcessImage::OnProcessBinnedfft() 
{
  int targetSize = 1024;
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  int nx = imBuf->mImage->getWidth();
  int ny = imBuf->mImage->getHeight();
  if (ny > nx)
    nx = ny;
  int binning = (nx + targetSize - 1) / targetSize;
  if (binning < 2)
    binning = 2;
  if (binning > 4)
    binning = 4;
  GetFFT(binning, BUFFER_FFT);
}

void CProcessImage::OnProcessLivefft()
{
  mLiveFFT = !mLiveFFT;
  if (mCamera->DoingContinuousAcquire())
    mWinApp->SetStatusText(mLiveFFT ? MEDIUM_PANE : COMPLEX_PANE, "");
}

void CProcessImage::OnUpdateProcessLivefft(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mLiveFFT ? 1 : 0);
}

void CProcessImage::OnProcessCircleonlivefft()
{
  mCircleOnLiveFFT = !mCircleOnLiveFFT;
}

void CProcessImage::OnUpdateProcessCircleonlivefft(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mCircleOnLiveFFT ? 1 : 0);
}

void CProcessImage::OnProcessSideBySide()
{
  mSideBySideFFT = !mSideBySideFFT;
  if (!mSideBySideFFT && mWinApp->mFFTView)
    mWinApp->mFFTView->CloseFrame();
}


void CProcessImage::OnUpdateProcessSideBySide(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mSideBySideFFT ? 1 : 0);
}


void CProcessImage::OnProcessAutomaticFFTs()
{
  mAutoSingleFFT = !mAutoSingleFFT;
}


void CProcessImage::OnUpdateProcessAutomaticFFTs(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mSideBySideFFT);
  pCmdUI->SetCheck(mAutoSingleFFT ? 1 : 0);
}

void CProcessImage::OnProcessSetDefocusForCircles()
{
  float defocus = -mFixedRingDefocus;
  if(KGetOneFloat("Fixed defocus at which to draw circles (-microns):", defocus, 1))
    mFixedRingDefocus = -(float)defocus;
}


void CProcessImage::OnProcessDoCtffindFitOnClick()
{
  mCtffindOnClick = !mCtffindOnClick;
}


void CProcessImage::OnUpdateProcessDoCtffindFitOnClick(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(true);
  pCmdUI->SetCheck(mCtffindOnClick ? 1 : 0);
}

// Open the Ctffind option window if it is not open, leave it open
void CProcessImage::OnProcessSetCtffindOptions()
{
  if (mCtffindParamDlg)
    return;
  mCtffindParamDlg = new CCtffindParamDlg;
  mCtffindParamDlg->Create(IDD_CTFFINDDLG);
  mWinApp->SetPlacementFixSize(mCtffindParamDlg, &mCtffindDlgPlace);
  mWinApp->RestoreViewFocus();
}

WINDOWPLACEMENT *CProcessImage::GetCtffindPlacement(void)
{
  if (mCtffindParamDlg) {
    mCtffindParamDlg->GetWindowPlacement(&mCtffindDlgPlace);
  }
  return &mCtffindDlgPlace;
}

void CProcessImage::OnProcessRotateleft() 
{
  RotateImage(true);  
}

void CProcessImage::OnProcessRotateright() 
{
  RotateImage(false); 
}


void CProcessImage::RotateImage(BOOL bLeft)
{
  short int *brray;
  float *frray;
  unsigned char *ubray;
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  KImage *image = imBuf->mImage;
  int nx = image->getWidth();
  int ny = image->getHeight();
  int type = image->getType();
  /*static int operation = 0, invert = 0;
  int nxout, nyout;*/
  DWORD start = GetTickCount();
  if (type == kRGB) {
    NewArray2(ubray, unsigned char, 3 * nx, ny);
    brray = (short int *)ubray;
  } else if (type != kFLOAT) {
    NewArray2(brray, short int, nx, ny);
  } else {
    NewArray2(frray, float, nx, ny);
    brray = (short int *)frray;
  }
  if (!brray) {
    SEMMessageBox("Failed to get memory for rotated image", MB_EXCLAME);
    return;
  }
  
  image->Lock();
  if (bLeft)
    ProcRotateLeft(image->getData(), type, nx, ny, brray);
  else
    ProcRotateRight(image->getData(), type, nx, ny, brray);
  image->UnLock();
  NewProcessedImage(imBuf, brray, type == kUBYTE ? kSHORT : type, ny, nx, 1);
  /*ProcRotateFlip((short int *)image->getData(), type, nx, ny, operation, invert, brray, &nxout, &nyout);
  image->UnLock();
  NewProcessedImage(imBuf, brray, type == kUBYTE ? kSHORT : type, nxout, nyout, 1);
  if (invert)
    operation = (operation + 1) % 8;
  invert = 1 - invert;
  SEMTrace('1',"Time %f", SEMTickInterval((double)start));*/
}

// Filter the image in the given buffer with the standard filter parameters
int CProcessImage::FilterImage(EMimageBuffer *imBuf, int outBufNum, float sigma1,
  float sigma2, float radius1, float radius2)
{
  KImage *image = imBuf->mImage;
  int nx, ny, nxpad, nypad, nxdim, dir = 1;
  float ctf[8193], delta, *brray;
  float padFrac = 0.05f;
  if (!image)
    return -1;
  if (image->getType() == kRGB) {
    SEMMessageBox("You cannot filter an RGB image");
    return 1;
  }
  image->getSize(nx, ny);
  nxpad = XCorrNiceFrame((int)((1. + padFrac) * nx), 2, 19);
  nypad = XCorrNiceFrame((int)((1. + padFrac) * ny), 2, 19);
  nxdim = nxpad + 2;
  NewArray2(brray, float, nxdim, nypad);
  if (!brray) {
    SEMMessageBox("Failed to get memory for filtering image");
    return 1;
  }
  XCorrSetCTF(sigma1, sigma2, radius1, radius2, ctf, nxpad, nypad, &delta);
  image->Lock();
  XCorrFilter((float *)image->getData(), image->getType(), nx, ny, brray, nxpad, nypad, 
    delta, ctf);
  image->UnLock();

  // If the destination buffer has no image, copy the source first to keep 
  // NewprocessedImage happy
  if (!mImBufs[outBufNum].mImage)
    mWinApp->mBufferManager->CopyImBuf(imBuf, &mImBufs[outBufNum], false);
  NewProcessedImage(imBuf, (short *)brray, kFLOAT, nx, ny, 1, -1, false, outBufNum);
  return 0;
}

// Do arithmetic operation between two images
int CProcessImage::CombineImages(int bufNum1, int bufNum2, int outBufNum, int operation)
{
  KImage *image1 = mImBufs[bufNum1].mImage;
  KImage *image2 = mImBufs[bufNum2].mImage;
  int type1 = image1->getType();
  int type2 = image2->getType();
  int nx, ny, ind;
  float *inArray1, *inArray2, *outArray;
  if (!image1 || !image2)
    return -1;
  image1->getSize(nx, ny);
  if (type1 == kRGB || type2 == kRGB) {
    SEMMessageBox("You cannot do arithmetic operations on RGB images");
    return 1;
  }
  if (image2->getWidth() != nx || image2->getHeight() != ny) {
    SEMMessageBox("You cannot do arithmetic operations on different-sized images");
    return 1;
  }

  // Get float array for result
  image1->Lock();
  image2->Lock();
  NewArray2(outArray, float, nx, ny);
  if (outArray) {

    //Either assign this as the input array if 1 already float, or copy 1 into the array
    if (type1 == kFLOAT) {
      inArray1 = (float *)image1->getData();
    } else {
      sliceTaperOutPad(image1->getData(), type1, nx, ny, outArray, nx, nx, ny, 1, 0.);
      inArray1 = outArray;
    }

    // If 2 is already float use it as input, otherwise get array and copy to it
    if (type2 == kFLOAT) {
      inArray2 = (float *)image2->getData();
    } else {
      NewArray2(inArray2, float, nx, ny);
      if (inArray2)
        sliceTaperOutPad(image2->getData(), type2, nx, ny, inArray2, nx, nx, ny, 1, 0.);
      else
        delete[] outArray;
    }
  }
  if (!outArray || !inArray2) {
    SEMMessageBox("Failed to get memory for combining two images");
    image1->UnLock();
    image2->UnLock();
    return 1;
  }

  // Do the operation
  switch (operation) {
  case PROC_ADD_IMAGES:
    for (ind = 0; ind < nx * ny; ind++)
      outArray[ind] = inArray1[ind] + inArray2[ind];
    break;

  case PROC_SUBTRACT_IMAGES:
    for (ind = 0; ind < nx * ny; ind++)
      outArray[ind] = inArray1[ind] - inArray2[ind];
    break;

  case PROC_MULTIPLY_IMAGES:
    for (ind = 0; ind < nx * ny; ind++)
      outArray[ind] = inArray1[ind] * inArray2[ind];
    break;

  case PROC_DIVIDE_IMAGES:
    for (ind = 0; ind < nx * ny; ind++) {
      if (inArray2[ind] != 0.)
        outArray[ind] = inArray1[ind] / inArray2[ind];
      else
        outArray[ind] = 0.;
    }
    break;
  }

  // Clean up and replace the buffer
  image1->UnLock();
  image2->UnLock();
  if (type2 != kFLOAT)
    delete[] inArray2;

  if (!mImBufs[outBufNum].mImage)
    mWinApp->mBufferManager->CopyImBuf(&mImBufs[bufNum1], &mImBufs[outBufNum], false);
  NewProcessedImage(&mImBufs[bufNum1], (short *)outArray, kFLOAT, nx, ny, 1, -1, false, 
    outBufNum);
  return 0;
}

// Multiple by a factor and add an offset to the image in imBuf, put in the indicated
// output buffer, keeping the same type if retainType is true
int CProcessImage::ScaleImage(EMimageBuffer *imBuf, int outBufNum, float factor, 
  float offset, bool retainType)
{
  KImage *image = imBuf->mImage;
  int nx, ny, type, newType = kFLOAT, dataSize = 4, numChan = 1;
  unsigned char *array;
  void *inArray;
  if (!image)
    return -1;
  type = image->getType();
  if (type == kRGB && !retainType) {
    SEMMessageBox("You cannot scale an RGB image into a floating point image");
    return 1;
  }

  // Going to scale into a new array regardless, get one of the right size
  if (retainType) {
    dataSizeForMode(type, &dataSize, &numChan);
    newType = type;
  }
  image->getSize(nx, ny);
  NewArray2(array, unsigned char, nx, ny * dataSize * numChan);
  if (!array) {
    SEMMessageBox("Failed to get memory for scaling image");
    return 1;
  }

  // Convert the data into the float array here if not staying the same type
  image->Lock();
  inArray = image->getData();
  if (!retainType) {
    sliceTaperOutPad(inArray, type, nx, ny, (float *)array, nx, nx, ny, 1, 0.);
    inArray = array;
  }

  // Scale and get into output buffer
  ProcScaleImage(inArray, type == kRGB ? type : newType, nx * numChan, ny, factor, offset,
    array);
  image->UnLock();
  if (!mImBufs[outBufNum].mImage)
    mWinApp->mBufferManager->CopyImBuf(imBuf, &mImBufs[outBufNum], false);
  NewProcessedImage(imBuf, (short *)array, newType, nx, ny, 1, -1, false, outBufNum);
  return 0;
}

// Copy two images into one side by side or top/bottom
int CProcessImage::PasteImages(EMimageBuffer *imBuf1, EMimageBuffer *imBuf2, 
  int outBufNum, bool vertical)
{
  EMimageBuffer copy1, copy2;
  KImage *image1 = imBuf1->mImage;
  KImage *image2 = imBuf2->mImage;
  int type1 = image1->getType();
  int type2 = image2->getType();
  int nx1, ny1, nx2, ny2, nxOut, nyOut;
  unsigned char *outBuf;
  double moreBin;
  if (!image1 || !image2)
    return -1;
  if (type1 == kRGB || type2 == kRGB) {
    SEMMessageBox("You cannot paste together RGB images");
    return 1;
  }

  // Convert each to byte first - the size may change to be a multiple of 4
  if (type1 != kUBYTE) {
    if (mWinApp->mBufferManager->CopyImBuf(imBuf1, &copy1, false))
      return 1;
    copy1.UpdatePixMap();
    if (!copy1.ConvertToByte(0., 0.)) {
      SEMMessageBox("Cannot convert first buffer to bytes for pasting images");
      return 1;
    }
    image1 = copy1.mImage;
    image1->getSize(nx1, ny1);
  }
  if (type2 != kUBYTE) {
    if (mWinApp->mBufferManager->CopyImBuf(imBuf2, &copy2, false))
      return 1;
    copy2.UpdatePixMap();
    if (!copy2.ConvertToByte(0., 0.)) {
      SEMMessageBox("Cannot convert second buffer to bytes for pasting images");
      return 1;
    }
  }

  // Set sizes and output buffer
  image1->getSize(nx1, ny1);
  image2->getSize(nx2, ny2);
  if (vertical) {
    nxOut = B3DMAX(nx1, nx2);
    nyOut = ny1 + ny2;
  } else {
    nxOut = nx1 + nx2;
    nyOut = B3DMAX(ny1, ny2);
  }
  NewArray2(outBuf, unsigned char, nxOut, nyOut);
  if (!outBuf) {
    SEMMessageBox("Failed to get array for for pasting images");
    return 1;
  }

  // Call the processing routine
  image1->Lock();
  image2->Lock();
  ProcPasteByteImages((unsigned char *)image1->getData(), nx1, ny1,
    (unsigned char *)image2->getData(), nx2, ny2, outBuf, vertical);
  image1->UnLock();
  image2->UnLock();

  // Put it in the output buffer as usual, give it less binning so it displays fully
  // at same zoom
  if (!mImBufs[outBufNum].mImage)
    mWinApp->mBufferManager->CopyImBuf(imBuf1, &mImBufs[outBufNum], false);
  moreBin = 1. / B3DMAX(nxOut / (float)nx1, nyOut / (float)ny1);
  NewProcessedImage(imBuf1, (short *)outBuf, kUBYTE, nxOut, nyOut, moreBin, -1, false,
    outBufNum);
  return 0;
}

// Common routine for putting a newly processed image based on the given buffer into 
// buffer A or one specified by toBufNum
void CProcessImage::NewProcessedImage(EMimageBuffer *imBuf, short *brray, int type,
                                      int nx, int ny, double moreBinning, int capFlag,
                                      bool fftWindow, int toBufNum)
{
  EMbufferManager *bufferManager = mWinApp->mBufferManager;
  EMimageBuffer *toImBuf = B3DCHOICE(fftWindow, mWinApp->GetFFTBufs(), mImBufs) +toBufNum;
  BOOL hasUserPtSave = toImBuf->mHasUserPt;
  float userPtXsave = toImBuf->mUserPtX;
  float userPtYsave = toImBuf->mUserPtY;
  int ind;

  // If imBuf has no image, switch to the buffer we are going to, then find any image
  if (!imBuf->mImage)
    imBuf = toImBuf;
  if (!imBuf->mImage) {
    for (ind = 0; ind < B3DCHOICE(fftWindow, MAX_FFT_BUFFERS, MAX_BUFFERS); ind++) {
      imBuf = B3DCHOICE(fftWindow, mWinApp->GetFFTBufs(), mImBufs) + ind;
      if (imBuf->mImage)
        break;
    }
  }

  // Make a temporary copy of the imBuf because it may get displaced on the roll
  EMimageBuffer imBufTmp;
  bufferManager->CopyImBuf(imBuf, &imBufTmp, false);

  // Roll the buffers just like on acquire if going to A
  int nRoll = B3DCHOICE(fftWindow, MAX_FFT_BUFFERS - 1, 
    bufferManager->GetShiftsOnAcquire());
  if ((!toImBuf->GetSaveCopyFlag() && toImBuf->mCaptured > 0 && mLiveFFT &&
    toImBuf == mImBufs) || toBufNum > 0)
    nRoll = 0;
  for (int i = nRoll; i > 0 ; i--)
    bufferManager->CopyImBuf(&toImBuf[i - 1], &toImBuf[i]);

  // Make sure it's OK to destroy A now: if not delete data
  if (!fftWindow && !bufferManager->OKtoDestroy(toBufNum, "Processing this image")) {
    delete [] brray;
    return;
  }

  // Copy the imbuf to inherit any neat properties not managed by replaceImage
  imBufTmp.mCaptured = BUFFER_PROCESSED;
  bufferManager->CopyImBuf(&imBufTmp, toImBuf, false);

  // Replace the image, again cleaning up on failure
  if (bufferManager->ReplaceImage((char *)brray, type, nx, ny, toBufNum, capFlag,
    imBuf->mConSetUsed, fftWindow) ) {
    delete [] brray;
    return;
  }

  // Fix the binning, transfer any extra data, and redisplay
  toImBuf->mBinning = B3DNINT(imBufTmp.mBinning * moreBinning);
  toImBuf->mPixelSize = imBufTmp.mPixelSize * (float)moreBinning;
  if (imBufTmp.mImage) {
    toImBuf->mImage->SetUserData(imBufTmp.mImage->GetUserData());
    imBufTmp.mImage->SetUserData(NULL);
  }
  imBufTmp.DeleteImage();

  // Restore user point position and status for a live FFT
  if (mLiveFFT) {
    toImBuf->mHasUserPt = hasUserPtSave;
    toImBuf->mUserPtX = userPtXsave;
    toImBuf->mUserPtY = userPtYsave;
  }

  mWinApp->SetCurrentBuffer(toBufNum, fftWindow);
}

void CProcessImage::OnProcessCropimage()
{
  int top, left, bottom, right;
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  left = (int)floor((double)B3DMIN(imBuf->mUserPtX, imBuf->mLineEndX) + 1);
  right = (int)ceil((double)B3DMAX(imBuf->mUserPtX, imBuf->mLineEndX) - 1);
  top = (int)floor((double)B3DMIN(imBuf->mUserPtY, imBuf->mLineEndY) + 1);
  bottom = (int)ceil((double)B3DMAX(imBuf->mUserPtY, imBuf->mLineEndY) - 1);
  CropImage(imBuf, top, left, bottom, right);
}

void CProcessImage::OnUpdateProcessCropimage(CCmdUI *pCmdUI)
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  pCmdUI->Enable(!mWinApp->DoingTasks() && imBuf && imBuf->mHasUserLine);
}

// Crop the image in the given buffer given the inclusive coordinate limits
int CProcessImage::CropImage(EMimageBuffer *imBuf, int top, int left, int bottom, int right)
{
  Islice slice;
  Islice *newsl;
  int nx, ny, mode;
  float cenCrit = 3.f;
  bool centered;
  void *data;
  EMimageExtra *extra;
  KImage *image = imBuf->mImage;
  if (!image)
    return 1;
  image->getSize(nx, ny);
  top = B3DMAX(0, top);
  bottom = B3DMIN(bottom, ny - 1);
  left = B3DMAX(0, left);
  right = B3DMIN(right, nx - 1);

  // Make it even in X
  if ((right + 1 - left) % 2)
    right--;
  if (bottom <= top || right <= left)
    return 2;
  switch (image->getType()) {
    case kUBYTE: mode = SLICE_MODE_BYTE; break;
    case kSHORT: mode = SLICE_MODE_SHORT; break;
    case kUSHORT: mode = SLICE_MODE_USHORT; break;
    case kFLOAT: mode = SLICE_MODE_FLOAT; break;
    case kRGB: mode = SLICE_MODE_RGB; break;
    default:
      return 3;
  }
  image->Lock();
  data = image->getData();
  sliceInit(&slice, nx, ny, mode, data);
  newsl = sliceBox(&slice, left, top, right + 1, bottom + 1);
  image->UnLock();
  if (!newsl)
    return 4;

  centered = fabs((left + right) / 2. - nx / 2.) < cenCrit &&
    fabs((top + bottom) / 2. - ny / 2.) < cenCrit;
  NewProcessedImage(imBuf, newsl->data.s, image->getType(), right + 1 - left,
    bottom + 1 - top, 1, centered ? imBuf->mCaptured : BUFFER_PROCESSED);
  free(newsl);
  extra = mImBufs->mImage->GetUserData();
  if (extra->mUncroppedX < 0)
    extra->mUncroppedX = -extra->mUncroppedX;
  if (extra->mUncroppedY < 0)
    extra->mUncroppedY = -extra->mUncroppedY;

  return 0;
}

// Menu entry to reduce an image
void CProcessImage::OnProcessReduceimage()
{
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  if (!imBuf)
    return;
  if (!KGetOneFloat("Factor (greater than 1) by which to reduce image:",
    mReductionFactor, 1))
    return;
  mReductionFactor = B3DMAX(1.f, mReductionFactor);
  ReduceImage(imBuf, mReductionFactor);
}

// Reduce the image in the given buffer by the given amount, returning an optional error 
// string or just putting up a message box
int CProcessImage::ReduceImage(EMimageBuffer *imBuf, float factor, CString *errStr)
{
  int nx, ny, newX, newY, mode, retval = 0, dataSize, numChan, zoomErr = 0;
  void *data;
  float sxOff, syOff;
  char *filtImage = NULL;
  unsigned char **linePtrs = NULL;
  double zoom;
  CString mess, str;
  const char *messages[] = {"reduction factor not in allowed range",
    "unsupported type of data",
    "failed to allocate memory for arrays",
    "error in zoomWithFilter"};
  
  KImage *image = imBuf->mImage;
  if (errStr)
    *errStr = "";
  if (!image)
    return 1;

  // Get new size and test it; keep X even
  image->getSize(nx, ny);
  newX = 2 * (int)(0.5 * nx / factor);
  newY = (int)(ny / factor);
  if (factor <= 1. || newX < 10. || newY < 10.)
    retval = 2;

  // Use Lanczos 2 for this
  zoom = 1. / factor;
  if (!retval) {
    if ((zoomErr = selectZoomFilter(4, zoom, &mode)) != 0)
      retval = 2;
  }

  // Center the image
  sxOff = (float)(0.5 * (nx - factor * newX));
  syOff = (float)(0.5 * (ny - factor * newY));
  mode = image->getType();
  if (!retval && dataSizeForMode(mode, &dataSize, &numChan))
    retval = 3;
  image->Lock();
  data = image->getData();

  // Get array and line pointers
  if (!retval) {
    NewArray2(filtImage, char, newX, newY * dataSize * numChan);
    linePtrs = makeLinePointers(data, nx, ny, dataSize * numChan);
    if (!filtImage || !linePtrs) {
      retval = 4;
      delete [] filtImage;
      image->UnLock();
    }
  }

  // Do the reduction
  if (!retval) {
    zoomErr = zoomWithFilter(linePtrs, nx, ny, sxOff, syOff, newX, newY, newX, 0,
      mode, filtImage, NULL, NULL);
    image->UnLock();
    if (zoomErr != 0) {
      retval = 5;
      delete [] filtImage;
    } else {
      NewProcessedImage(imBuf, (short *)filtImage, mode, newX, newY, B3DNINT(factor));
    }
  }
  B3DFREE(linePtrs);
  if (retval) {
    mess = CString("Error trying to reduce image: ") + messages[retval - 2];
    if (zoomErr) {
      str.Format(" (error code %d)", zoomErr);
      mess += str;
    }
    if (errStr)
      *errStr = mess;
    else
      SEMMessageBox(mess);
  }
  return retval;
}

// Allocate a new array and copy a scaled correlation into, assign to buffer A
int CProcessImage::CorrelationToBufferA(float *array, int nxpad, int nypad, int binning,
                                        float &corMin, float &corMax)
{
  short int *crray;
  int kx, ky, kxout, kyout;
  corMin = 1.e30f;
  corMax = -1.e30f;
  float corVal;
  NewArray2(crray, short int, nxpad, nypad);
  if (!crray) {
    AfxMessageBox(_T("Error getting buffer to copy correlation into."), MB_EXCLAME);
    return 1;
  }

  // Get min and max
  for (ky = 0; ky < nypad; ky++) {
    for (kx = 0; kx < nxpad; kx++) {
      corVal = array[kx + ky * (nxpad + 2)];
      if (corVal < corMin)
        corMin = corVal;
      if (corVal > corMax)
        corMax = corVal;
    }
  }

  // Copy and center array
  for (ky = 0; ky < nypad; ky++) {
    kyout = (ky + nypad / 2) % nypad;
    for (kx = 0; kx < nxpad; kx++) {
      kxout = (kx + nxpad / 2) % nxpad;
      crray[kxout + kyout * nxpad] = (short)(32000. * 
        (array[kx + ky * (nxpad + 2)] - corMin) / (corMax - corMin));
    }
  }
  NewProcessedImage(mImBufs, crray, kSHORT, nxpad, nypad, binning);

  // Set the scale limit to the known max; but save found limit first so autocorrelation
  // can set better value
  if (mImBufs->mImageScale) {
    mCorrMaxScale = mImBufs->mImageScale->GetMaxScale();
    mImBufs->mImageScale->SetMinMax(mImBufs->mImageScale->GetMinScale(), 32000);
    mWinApp->SetCurrentBuffer(0);
  }
  return 0;
}

// Compute the mean of the image in the given buffer in the area foreshortened by tilt
float CProcessImage::ForeshortenedMean(int bufNum)
{
  KImage *image = mImBufs[bufNum].mImage;
  if (!image)
    return 1.;
  int nx = image->getWidth();
  int ny = image->getHeight();
  int nxuse = nx;
  int nyuse = ny;
  float tiltAngle;
  double retVal;

  if (mImBufs[bufNum].GetTiltAngle(tiltAngle)) {
    double factor = cos(DTOR * tiltAngle);
    // Foreshorten X if rotation puts tilt axis near Y axis, otherwise foreshorten Y
    if (fabs(tan(DTOR * mShiftManager->GetImageRotation(
      mWinApp->GetCurrentCamera(), mImBufs[bufNum].mMagInd))) > 1.)
      nxuse = (int)(nx * factor);
    else 
      nyuse = (int)(ny * factor);
  }
  image->Lock();
  retVal = ProcImageMean(image->getData(), image->getType(), nx, ny, 
    (nx - nxuse) / 2, (nx + nxuse) / 2 - 1, (ny - nyuse) / 2, (ny + nyuse) / 2 - 1);
  image->UnLock();
  return (float)retVal;
}

// Compute a mean of a tilt-foreshortened subarea of a specified size or fraction of the 
// field, optionally applying stored shift
int CProcessImage::ForeshortenedSubareaMean(int bufNum, float fracOrSizeX, 
  float fracOrSizeY, bool useShift, float &mean, CString *message)
{
  EMimageBuffer *imBuf = &mImBufs[bufNum];
  KImage *image = imBuf->mImage;
  CameraParameters *camP = mWinApp->GetCamParams();
  int nx, ny, nxuse, nyuse, ix0, ix1, iy0, iy1, ixShift, iyShift;
  float tiltAngle, shiftX = 0., shiftY = 0.;
  double factor;

  // Check conditions: need camera and mag in buffer if doing fractions
  if (!image) {
    if (message)
      *message = "There is no image in the buffer passed to ForeshortenedSubareaMean";
    return 1;
  }
  if (imBuf->mCamera < 0 || imBuf->mMagInd <= 0 || 
    (imBuf->mBinning <= 0 && (fracOrSizeX <= 1. || fracOrSizeY <= 0.))) {
    if (message)
      *message = "The image buffer is missing information needed to get the foreshorted "
      "subarea mean";
    return 1;
  }
  if (!imBuf->GetTiltAngle(tiltAngle)) {
    if (message)
      *message = "The image buffer passed to ForeshortenedSubareaMean has no stored tilt "
      "angle";
    return 1;
  }

  // Set up shifts if requested, and set up size to try to use
  image->getSize(nx, ny);
  if (useShift)
    image->getShifts(shiftX, shiftY);
  ixShift = B3DNINT(shiftX);
  iyShift = B3DNINT(shiftY);
  nxuse = B3DNINT(fracOrSizeY);
  if (fracOrSizeY <= 1.)
    nxuse = B3DNINT(fracOrSizeY * camP[imBuf->mCamera].sizeY / imBuf->mBinning);
  nyuse = B3DNINT(fracOrSizeY);
  if (fracOrSizeY <= 1.)
    nyuse = B3DNINT(fracOrSizeY * camP[imBuf->mCamera].sizeY / imBuf->mBinning);
  if (nxuse > nx || nyuse > ny) {
    if (message)
      message->Format("The specified subarea size (%d x %d) is bigger than the image"
        "(%d x %d)", nxuse, nyuse, nx, ny);
    return 1;
  }

  // reduce the size perpendicular to the nearest axis
  factor = cos(DTOR * tiltAngle);
  if (fabs(tan(DTOR * mShiftManager->GetImageRotation(imBuf->mCamera, imBuf->mMagInd)))
    > 1.)
    nxuse = B3DNINT(nxuse * factor);
  else
    nyuse = B3DNINT(nyuse * factor);
  if (nxuse < 10 || nyuse < 10) {
    if (message)
      message->Format("The specified subarea is too small after foreshortening "
        "(%d x %d)", nxuse, nyuse);
    return 1;
  }

  // Get coordinate limits and make sure they are legal
  ix0 = B3DMAX(0, (nx - nxuse) / 2 - ixShift);
  ix1 = B3DMIN((nx + nxuse) / 2 - ixShift - 1, nx - 1);
  iy0 = B3DMAX(0, (ny - nyuse) / 2 - iyShift);
  iy1 = B3DMIN((ny + nyuse) / 2 - iyShift - 1, ny - 1);
  image->Lock();
  mean = (float)ProcImageMean(image->getData(), image->getType(), nx, ny, ix0, ix1, iy0,
    iy1);
  SEMTrace('1',"ForeshortenedSubareaMean using %d x %d, at %d, %d", ix1 + 1 - ix0, 
    iy1 + 1 - iy0, (ix0 + ix1) / 2, (iy0 + iy1) / 2);
  image->UnLock();
  return 0;
}

void CProcessImage::OnProcessZerotiltmean() 
{
  float mean;
  int bufNum = mWinApp->GetImBufIndex();
  mean = ForeshortenedMean(bufNum);
  char bufChar = 'A' + bufNum;
  CString report;
  report.Format("Mean of foreshortened area in Buffer %c \r\n"
    "corresponding to the full area at zero tilt = %.1f", bufChar, mean);
  mWinApp->AppendToLog(report, LOG_MESSAGE_IF_CLOSED);
}

void CProcessImage::OnUpdateProcessZerotiltmean(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mImBufs[mWinApp->GetImBufIndex()].mImage != NULL && 
    mImBufs[mWinApp->GetImBufIndex()].mImage->getMode() == kGray);   
}

void CProcessImage::OnProcessSetintensity() 
{
  DoSetIntensity(false, -1.);
}

int CProcessImage::DoSetIntensity(bool doseRate, float useFactor)
{
  int bufNum = 0;
  int error, ldArea;
  double delta;
  float oldMean, fullMean, factor = 1.;
  CString *modeNames = mWinApp->GetModeNames();
  CString infoLine, query;

  if (mImBufs->IsProcessed()) {
    SEMMessageBox("This command operates on the image in Buffer A\n"
      "and there is a processed image in that buffer.\n\n"
      "Put an acquired image in Buffer A and try again.", MB_EXCLAME);
    return -1;
  }

  // Get mean of full image
  fullMean = (float)WholeImageMean(mImBufs);
  ldArea = mWinApp->LowDoseMode() ? 
    mCamera->ConSetToLDArea(mImBufs->mConSetUsed) : -1;

  // For regular mode or Record/Preview in low dose, get zero-tilt mean and ask for
  // factor or value for it
  if (ldArea < 0 || ldArea == 3) {
    if (doseRate) {
      oldMean = ForeshortenedMean(bufNum);
      DoseRateFromMean(mImBufs, oldMean, oldMean);
      DoseRateFromMean(mImBufs, fullMean, fullMean);
      infoLine.Format("Dose rate in whole image = %.2f; rate in tilt-foreshortened "
        "area = %.2f electrons/physical pixel/sec", fullMean, oldMean);
      query = "Enter desired dose rate for the zero-tilt area:";
      factor = oldMean;
    } else {
      oldMean = EquivalentRecordMean(bufNum);
      infoLine.Format("Mean of whole image = %.1f; implied mean of tilt-foreshortened "
        "area of %s image = %.1f", fullMean, modeNames[3], oldMean);
      query = "Enter factor to change intensity by (a value < 10) or desired mean "
        "(a value > 10) for the zero-tilt area in a " + modeNames[3] + " image :";
    }
  } else {

    // For other areas in low dose, 
    if (doseRate) {
      DoseRateFromMean(mImBufs, fullMean, fullMean);
      infoLine.Format("The dose rate of this %s image = %.2f electrons/physical pixel/sec"
        , modeNames[ldArea], fullMean);
      query = "Enter desired dose rate for the " + modeNames[ldArea] + " area :";
      factor = fullMean;
    } else {
      infoLine.Format("The mean of this %s image = %.1f", modeNames[ldArea], fullMean);
      query = "Enter factor to change intensity by (a value < 10) or desired mean "
        "(a value > 10) for the " + modeNames[ldArea] + " area :";
    }
    oldMean = fullMean;
    if (mWinApp->mMultiTSTasks->AutocenMatchingIntensity())
      ldArea = -1;
  }

  if (useFactor <= 0) {
    if (!KGetOneFloat(infoLine, query, factor, 1))
      return 0;
  } else {
    factor = useFactor;
  }

  if (factor <= 10. && !doseRate)
    delta = factor;
  else
    delta = factor / oldMean;

  error = mWinApp->mBeamAssessor->ChangeBeamStrength(delta, ldArea);
  if (error && useFactor <= 0.) {
    if (error == BEAM_STARTING_OUT_OF_RANGE || error == BEAM_ENDING_OUT_OF_RANGE)
      AfxMessageBox("Warning: attempting to set beam strength beyond"
        " calibrated range", MB_EXCLAME);
    else
      AfxMessageBox("Error trying to change beam strength", MB_EXCLAME);
  }
  return error;
}

void CProcessImage::OnUpdateProcessSetintensity(CCmdUI* pCmdUI) 
{
  DoUpdateSetintensity(pCmdUI, false);
}

void CProcessImage::DoUpdateSetintensity(CCmdUI* pCmdUI, bool doseRate) 
{
  int ldArea, spot, junk, probe;
  double intensity;
  LowDoseParams *ldParm = mWinApp->GetLowDoseParams();
  BOOL enable = false;
  if (mImBufs->mImage != NULL && mImBufs->mExposure && mImBufs->mBinning &&
    !mWinApp->GetSTEMMode() && (!doseRate || mImBufs->mCamera >= 0)) {
    ldArea = mWinApp->LowDoseMode() ? 
      mCamera->ConSetToLDArea(mImBufs->mConSetUsed) : -1;
    if (ldArea < 0 || ldArea == 3) {
      spot = mScope->FastSpotSize();
      intensity = mScope->GetIntensity();
    } else {
      spot = ldParm[ldArea].spotSize;
      intensity = ldParm[ldArea].intensity;
    }
    probe = ldArea < 0 ? mScope->GetProbeMode() : ldParm[ldArea].probeMode;
    enable = !mWinApp->mBeamAssessor->OutOfCalibratedRange(intensity, spot, probe, junk);
    if (doseRate && enable) {
      CameraParameters *camP = mWinApp->GetCamParams() + mImBufs->mCamera;
      enable = mCamera->IsDirectDetector(camP) && camP->countsPerElectron > 0. &&
        mImBufs->mExposure > 0.;
    }
  }
  pCmdUI->Enable(enable);
}

void CProcessImage::OnSetDoseRate()
{
  DoSetIntensity(true, -1.);
}

void CProcessImage::OnUpdateSetDoseRate(CCmdUI *pCmdUI)
{
  DoUpdateSetintensity(pCmdUI, true);
}


float CProcessImage::EquivalentRecordMean(int bufNum)
{
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  float mean = ForeshortenedMean(bufNum);
  float gainRat, binRat, delFac = 1.;
  int camera = mWinApp->GetCurrentCamera();
  if (mImBufs[bufNum].mExposure > 0. && mImBufs[bufNum].mBinning) {
    binRat = (float)conSet->binning / (float)mImBufs[bufNum].mBinning;
    gainRat = mWinApp->GetGainFactor(camera, conSet->binning) /
      mWinApp->GetGainFactor(camera, mImBufs[bufNum].mBinning);
    delFac = (float)(binRat * binRat * gainRat * conSet->exposure /
      mImBufs[bufNum].mExposure);
  }
  return delFac * mean;
}

/////////////////////////////////
//  BEAM MOVING AND CENTERING
/////////////////////////////////
void CProcessImage::OnProcessMovebeam() 
{
  int nx, ny;
  float shiftX, shiftY;

  // Get the shift coordinates relative to the center
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  imBuf->mImage->getSize(nx, ny);
  imBuf->mImage->getShifts(shiftX, shiftY);
  
  // Coordinates in field are sum of image align shift and user point coord
  shiftX += imBuf->mUserPtX - nx / 2;
  shiftY += imBuf->mUserPtY - ny / 2;
  
  // Need negative, probably because we are undoing an existing shift
  MoveBeam(imBuf, -shiftX, -shiftY);
  mMoveBeamStamp = imBuf->mTimeStamp;
}

void CProcessImage::OnUpdateProcessMovebeam(CCmdUI* pCmdUI) 
{
  EnableMoveBeam(pCmdUI, false);
}

// Center beam with edges in active image
void CProcessImage::OnTasksCenterbeam()
{
  CenterBeamFromActiveImage(0., 0.);
}

int CProcessImage::CenterBeamFromActiveImage(double maxRadius, double maxError, 
                                             BOOL useCentroid, double maxMicronShift)
{
  float shiftX, shiftY, fitErr;
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  float xcen, ycen, radius, xcenUse, ycenUse, radUse, fracUse;
  int binning, err, numQuadrant;

  if (useCentroid) {
    KImage *image = imBuf->mImage;
    float nSample = 10000.;
    float matt = 0.1f;
    int ix0, ix1, iy0, iy1;
    int nx = image->getWidth();
    int ny = image->getHeight();
    image->Lock();
    void *data = image->getData();
    int type = image->getType();
    double mean, border = 1.e20;

    // Compute the mean on the 4 edges, outer 1/16 of field; use the lowest
    ix0 = nx / 16;
    ix1 = nx - ix0;
    iy0 = ny / 16;
    iy1 = ny - iy0;
    border = ProcImageMean(data, type, nx, ny, 0, ix0 -1, 0, ny - 1);
    mean = ProcImageMean(data, type, nx, ny, ix1, nx -1, 0, ny - 1);
    border = B3DMIN(border, mean);
    mean = ProcImageMean(data, type, nx, ny, ix0, ix1 - 1, 0, iy0 - 1);
    border = B3DMIN(border, mean);
    mean = ProcImageMean(data, type, nx, ny, ix0, ix1 - 1, iy1, ny - 1);
    border = B3DMIN(border, mean);

    // This had no merit.  It would work if the border could be defined to exclude 
    // points outside the beam reliably, but without that it gives those points more
    // weight by downweighting the points inside the beam.
    /*KImageScale::PctStretch((char *)data, type, nx, ny, nSample, matt, 1.f, 1.f, &xcen, 
      &cenPeak);
    thresh = border + threshFrac * (cenPeak - border); */

    // Get centroid and use it to shift
    ProcCentroid(data, type, nx, ny, 0, nx -1 , 0, ny - 1, border, xcen, ycen);
    shiftX = (float)(nx / 2. - xcen);
    shiftY = (float)(ny / 2. - ycen);
    MoveBeam(imBuf, shiftX, shiftY, maxMicronShift);
    mMoveBeamStamp = imBuf->mTimeStamp;
    image->UnLock();
    return 0;
  }

  // Or analyze from beam edges
  err = FindBeamCenter(imBuf, xcen, ycen, radius, xcenUse, ycenUse, radUse, fracUse, 
    binning, numQuadrant, shiftX, shiftY, fitErr);
  if (err < 0)
    mWinApp->AppendToLog("No beam edges detectable in this image", 
      LOG_MESSAGE_IF_CLOSED);
  else if (err > 0)
    mWinApp->AppendToLog("Error analyzing image for beam edges", LOG_MESSAGE_IF_CLOSED);
  else if (maxRadius > 0. && radius * binning > maxRadius) {
    mWinApp->AppendToLog("Beam radius from fit is greater than allowed radius; beam "
      "was not moved", LOG_OPEN_IF_CLOSED);
    err = 6;
  } else if (maxError > 0. && fitErr > maxError * maxRadius) {
    mWinApp->AppendToLog("Fit to beam edges has greater than allowed error; beam "
      "was not moved", LOG_OPEN_IF_CLOSED);
    err = 7;
  } else {
    MoveBeam(imBuf, shiftX, shiftY, maxMicronShift);
    mMoveBeamStamp = imBuf->mTimeStamp;
  }
  return err;
}

void CProcessImage::OnUpdateTasksCenterbeam(CCmdUI *pCmdUI)
{
  EnableMoveBeam(pCmdUI, true);
}

// Check if move beam or center beam will work
void CProcessImage::EnableMoveBeam(CCmdUI * pCmdUI, bool skipUserPt)
{
 EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
 ScaleMat mat, mat2;
 if (imBuf->mMagInd) {
   mat = mShiftManager->IStoCamera(imBuf->mMagInd);
   mat2 = mShiftManager->GetBeamShiftCal(imBuf->mMagInd);
 }
 pCmdUI->Enable(!mWinApp->DoingTasks() && !mCamera->CameraBusy() && mat.xpx != 0. 
   && mat2.xpx != 0 && imBuf->mImage != NULL && imBuf->mTimeStamp != mMoveBeamStamp &&
   (skipUserPt || imBuf->mHasUserPt) && imBuf->mMagInd && imBuf->mBinning);
}

// Move the beam BY the given shift in X and Y, in pixels in the given buffer image, where
// Y is inverted as typical for image pixels
// Or if imBuf is NULL, move beam by given number of microns in camera coordinates, where
// Y is NOT inverted
int CProcessImage::MoveBeam(EMimageBuffer *imBuf, float shiftX, float shiftY, 
  double maxMicronShift)
{
  double bsTot;
  float cenShiftX = 0., cenShiftY = 0., bsX, bsY;
  int magInd = imBuf != NULL ? imBuf->mMagInd : mScope->GetMagIndex();
  float pixel = mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(), magInd);

  // Invert Y for buffer coordinates here
  if (imBuf) {
    shiftX *= imBuf->mBinning;
    shiftY *= -imBuf->mBinning;
  } else {
    shiftX /= pixel;
    shiftY /= pixel;
  }

  // Get the matrix for going from camera to Beam shift through image shift
  ScaleMat bInv = mShiftManager->CameraToIS(magInd);
  ScaleMat IStoBS = mShiftManager->GetBeamShiftCal(magInd);
  if (!bInv.xpx || !IStoBS.xpx)
    return 1;

  // First compute shift in microns for message, and to test against limit
  if (imBuf) {
    ScaleMat aInv = mShiftManager->CameraToSpecimen(magInd);
    if (mWinApp->mMultiTSTasks->GetAutoCentering())
      mWinApp->mMultiTSTasks->GetCenteringBeamShift(cenShiftX, cenShiftY);
    mShiftManager->ApplyScaleMatrix(aInv, shiftX + cenShiftX / pixel, 
      shiftY + cenShiftY / pixel, bsX, bsY);
    bsTot = sqrt(bsX * bsX + bsY * bsY);
    CString message;
    if (maxMicronShift > 0. && bsTot > maxMicronShift) {
      PrintfToLog("Beam shift of %.3f microns would exceed limit of %.2f; "
        "beam was not moved", bsTot, maxMicronShift);
      return 0;
    } else {
      message.Format("Beam was shifted %.3f microns", bsTot);
      mWinApp->AppendToLog(message, LOG_MESSAGE_IF_CLOSED);
    }
  }

  ScaleMat camToBS = mShiftManager->MatMul(bInv, IStoBS);
  mShiftManager->ApplyScaleMatrix(camToBS, shiftX, shiftY, bsX, bsY);
  mScope->IncBeamShift(bsX, bsY);
  return 0;
}

// Move the beam by a fraction of field of view on the current camera
// Returns 1 if beam shift or image shift is not calibrated
int CProcessImage::MoveBeamByCameraFraction(float shiftX, float shiftY, bool uncalOK)
{
  double bsX, bsY, pixel;
  int magInd = mScope->FastMagIndex();
  int cam = mWinApp->GetCurrentCamera();
  CameraParameters *camp = mWinApp->GetCamParams() + cam;
  ScaleMat bInv = mShiftManager->CameraToIS(magInd);
  ScaleMat IStoBS = mShiftManager->GetBeamShiftCal(magInd);
  float camSize = (float)sqrt((double)camp->sizeX * camp->sizeY);
  shiftX *= camSize;
  shiftY *= camSize;
  if (!bInv.xpx || !IStoBS.xpx) {
    if (!uncalOK)
      return 1;
    pixel = mShiftManager->GetPixelSize(cam, magInd);
    if (JEOLscope)
      pixel *= mScope->FastMagIndex() < mScope->GetLowestMModeMagInd() ?
        mScope->GetJeol_LMCLA1_to_um() : mScope->GetJeol_CLA1_to_um();
    mScope->IncBeamShift(pixel * shiftX, pixel * shiftY);
    return 0;
  }
  ScaleMat camToBS = mShiftManager->MatMul(bInv, IStoBS);
  if (!shiftX && !shiftY)
    return 0;
  bsX = camToBS.xpx * shiftX + camToBS.xpy * shiftY;
  bsY = camToBS.ypx * shiftX + camToBS.ypy * shiftY;
  mScope->IncBeamShift(bsX, bsY);
  return 0;
}

// Compute the beam center and amount to shift to center the beam from the given image
// Fitted values for radius and center, and values actually used, are returned, all in
// coordinates binned by the factor, while the shift is in image coordinates
#define MAX_CPTS 370
int CProcessImage::FindBeamCenter(EMimageBuffer * imBuf, float & xcen, float & ycen,
                                  float &radius, float &xcenUse, float &ycenUse, 
                                  float &radUse, float &fracUse, int &binning, 
                                  int &numQuadrant, float &shiftX, float &shiftY, 
                                  float &rmsErr)
{
  int targetSize = 512;
  int smallSize, type, sizeX, sizeY, ix0, ix1, iy0, iy1, numPts, ipt1, ipt2, i;
  int nquad1, nquad2, nquad3, nquad4, failed3Pt, failed, iter;
  float centX, centY, meanVal, outCrit, edgeCrit, zcen, edgeCenX, edgeCenY;
  float arcLen, maxOutFrac, maxOut, delX, delY, fracX, fracY, minOut;
  float minArcToUseFit = 20.;
  float edgeFrac = 0.4f;
  float angleInc = 4.f;
  float minOutsideFrac = 0.025f;
  float Ktune = 4.685f;   // From Matlab documentation for bisquare weighting
  double wgtSum, xcenLast, ycenLast, gradPower1 = 2., gradPower2 = 0.5;
  int border = 4;
  int boxLen = 8;
  int boxWidth = 3;
  int maxIter = 10;
  float dx, dy, dist, dist2, maxDist, median, MADN, uu;
  KImage *image;
  short *data;
  float xx[MAX_CPTS], yy[MAX_CPTS], angle[MAX_CPTS], grad[MAX_CPTS], weight[MAX_CPTS];
  float resid[MAX_CPTS], temp[MAX_CPTS];
  BOOL oneQuadrant;

  if (!imBuf || !imBuf->mImage)
    return 1;
  image = imBuf->mImage;
  type = image->getType();
  if (type != kSHORT && type != kUSHORT && type != kFLOAT)
    return 2;
  sizeX = image->getWidth();
  sizeY = image->getHeight();
  rmsErr = 0.;

  // Determine binning to get small dimension to target size
  smallSize = sizeX < sizeY ? sizeX : sizeY;
  for (binning = 1; binning < 64; binning++)
    if (smallSize / binning <= targetSize)
      break;

  // Bin data into new array if necessary
  image->Lock();
  if (binning > 1) {
    NewArray(data, short int, (type == kFLOAT ? 2 : 1) *((size_t)sizeX * sizeY ) / 
      (binning * binning));
    if (!data)
      return 3;
    XCorrBinByN((void *)image->getData(), type, sizeX, sizeY, binning, data);
    sizeX /= binning;
    sizeY /= binning;
    smallSize /= binning;
  } else {
    data = (short *)image->getData();
  }

  // Find centroid of image with zero base, and mean in a quarter centered on that
  ProcCentroid(data, type, sizeX, sizeY, 0, sizeX - 1, 0, sizeY - 1, 0., centX, centY);
  ix0 = (int)B3DMAX(centX - smallSize / 8, 0);
  ix1 = (int)B3DMIN(centX + smallSize / 8, sizeX - 1);
  iy0 = (int)B3DMAX(centY - smallSize / 8, 0);
  iy1 = (int)B3DMIN(centY + smallSize / 8, sizeY - 1);
  meanVal = (float)ProcImageMean(data, type, sizeX, sizeY, ix0, ix1, iy0, iy1);
  outCrit = mFindBeamOutsideFrac * meanVal;
  edgeCrit = edgeFrac * meanVal;
  SEMTrace('p', "Centroid %.1f %.1f  mean %.0f", centX, centY, meanVal); 

  // Get points along edge then determine if one quadrant or more and get edge centroid
  numPts = ProcFindCircleEdges(data, type, sizeX, sizeY, centX, centY, border, angleInc,
    boxLen, boxWidth, outCrit, edgeCrit, xx, yy, angle, grad);
  image->UnLock();
  if (binning > 1)
    delete [] data;
  if (!numPts)
    return -1;

  nquad1 = nquad2 = nquad3 = nquad4 = 0;
  edgeCenX = edgeCenY = 0.;
  wgtSum = 0.;
  for (i = 0; i < numPts; i++) {
    edgeCenX += xx[i] / numPts;
    edgeCenY += yy[i] / numPts;
    weight[i] = (float)pow((double)grad[i], gradPower1);
    wgtSum += weight[i];
    if (angle[i] < -90.)
      nquad2++;
    else if (angle[i] < 0.)
      nquad1++;
    else if (angle[i] < 90.)
      nquad4++;
    else
      nquad3++;
  }
  for (i = 0; i < numPts; i++)
    weight[i] *= (float)(numPts / wgtSum);

  numQuadrant = (nquad1 ? 1:0) + (nquad2 ? 1:0) + (nquad3 ? 1:0) + (nquad4 ? 1:0);
  oneQuadrant = numQuadrant == 1;
  SEMTrace('p', "# of points in quadrants 1 to 4: %d %d %d %d", nquad1, nquad2, 
    nquad3, nquad4);

  // If one quadrant, use the center point instead
  if (oneQuadrant) {
    edgeCenX = xx[numPts / 2];
    edgeCenY = yy[numPts / 2];
    arcLen = angle[numPts - 1] - angle[0];
  }

  // Get points along edge then find three points well separated
  if (numPts >= 3) {
    maxDist = 0.;
    for (i = 1; i < numPts; i++) {
      dx = xx[i] - xx[0];
      dy = yy[i] - yy[0];
      dist = dx * dx + dy * dy;
      if (dist > maxDist) {
        ipt1 = i;
        maxDist = dist;
      }
    }
    maxDist = 0;
    for (i = 1; i < numPts; i++) {
      dx = xx[i] - xx[0];
      dy = yy[i] - yy[0];
      dist = dx * dx + dy * dy;
      dx = xx[i] - xx[ipt1];
      dy = yy[i] - yy[ipt1];
      dist2 = dx * dx + dy * dy;
      if (B3DMIN(dist, dist2) > maxDist) {
        ipt2 = i;
        maxDist = B3DMIN(dist, dist2);
      }
    }

    // DO the 3-point fit then the fit to all points
    failed3Pt = circleThrough3Pts(xx[0], yy[0], xx[ipt1], yy[ipt1], xx[ipt2], yy[ipt2], 
      &radius, &xcen, &ycen);

    // If that failed for points from more than one quadrant, give up
    if (failed3Pt && !oneQuadrant)
      return 5;
    SEMTrace('p', "Three point fit radius %.1f  center %.1f  %.1f", radius, xcen, ycen);
    if (numPts > 3 && !failed3Pt) {

      // Do full fit with initial weighting and then robust fitting with bisquare
      // weighting times a reduced gradient weighting
      for (iter = 0; iter < maxIter; iter++) {
        xcenLast = xcen;
        ycenLast = ycen;
        fitSphereWgt(xx, yy, NULL, weight, numPts, &radius, &xcen, &ycen, &zcen,&rmsErr);
        SEMTrace('p', "Amoeba fit to %d points, radius %.1f  center %.1f  %.1f   "
          "error %.2f", numPts, radius, xcen, ycen, rmsErr);
        if (fabs(xcen - xcenLast) < 0.1 && fabs(ycen - ycenLast) < 0.1)
          break;

        // Get absolute deviation residuals and their MADN
        for (i = 0; i < numPts; i++) {
          dx = xx[i] - xcen;
          dy = yy[i] - ycen;
          resid[i] = (float)fabs(sqrt(dx * dx + dy * dy) - radius);
        }
        rsMedian(resid, numPts, temp, &median);
        rsMADN(resid, numPts, median, temp, &MADN);
        wgtSum = 0.;
        for (i = 0; i < numPts; i++) {
          uu = resid[i] / (Ktune * MADN);
          if (uu < 1.)
            weight[i] = (float)((1 - uu * uu) * (1 - uu * uu) * 
            pow((double)grad[i], gradPower2));
          else
            weight[i] = 0.;
          wgtSum += weight[i];
        }
        for (i = 0; i < numPts; i++)
          weight[i] *= (float)(numPts / wgtSum);
      }
    }
  }

  // For one quadrant and arc length below criterion compute a minimum radius based on 
  // two adjacent corners of the image
  if (oneQuadrant && (failed3Pt || arcLen < minArcToUseFit)) {
    if (nquad1 || nquad3)
      failed = circleThrough3Pts(0., 0., (float)sizeX, (float)sizeY, edgeCenX, 
        edgeCenY, &radUse, &xcenUse, &ycenUse);
    else
      failed = circleThrough3Pts((float)sizeX, 0., 0., (float)sizeY, edgeCenX, 
        edgeCenY, &radUse, &xcenUse, &ycenUse);
    if (failed)
      return 5;
    SEMTrace('p', "Fit to edge center and corners gives minimal radius %.1f  center %.1f"
      "  %.1f (arc %.1f)", radUse, xcenUse, ycenUse, arcLen);
    maxOutFrac = 0.1f;
    if (numPts < 3 || failed3Pt) {
      xcen = xcenUse;
      ycen = ycenUse;
      radius = radUse;
    }
    minOut = minOutsideFrac * smallSize;
  } else {

    // Otherwise copy the fitted values, and ramp outside fraction up to 0.5 for 
    // one quadrant and arc length up to 90
    radUse = radius;
    xcenUse = xcen;
    ycenUse = ycen;
    maxOutFrac = 0.5;
    minOut = 0.;
    if (oneQuadrant)
      maxOutFrac = 0.1f + 0.4f * (arcLen - minArcToUseFit) / (90.f - minArcToUseFit);
  }

  // Compute the fraction of the move to allow by finding what fraction would bring
  // the center point out to the outside limits (either the max one or the min one)
  maxOut = maxOutFrac * smallSize;
  if (minOut > 0.)
    maxOut = minOut;
  delX = (float)(sizeX / 2. - xcenUse);
  delY = (float)(sizeY / 2. - ycenUse);
  fracX = fracY = 1.;
  if (delX > 0.5)
    fracX = (float)(sizeX + maxOut - edgeCenX) / delX;
  else if (delX < -0.5)
    fracX = -(maxOut + edgeCenX) / delX;
  if (delY > 0.5)
    fracY = (float)(sizeY + maxOut - edgeCenY) / delY;
  else if (delY < -0.5)
    fracY = -(maxOut + edgeCenY) / delY;
  fracUse = B3DMIN(fracX, fracY);

  // For point and corner estimate, make fraction > 1 to get a minimum distance out
  // For regular fit, make fraction < 1 to constrain to the maximum distance out
  if (minOut > 0.)
    fracUse = B3DMAX(fracUse, 1.f);
  else
    fracUse = B3DMIN(fracUse, 1.f);
  SEMTrace('p', "Use fraction %.1f of shifts %.1f  %.1f", fracUse, delX, delY);

  // The shift to do must be scaled up by the binning
  shiftX = (float)(binning * fracUse * delX);
  shiftY = (float)(binning * fracUse * delY);

  return 0;
}

///////////////////////////////////////////////////////////////////////
//  DEFECT CORRECTION
///////////////////////////////////////////////////////////////////////

#define MAX_REPLACED  40
void CProcessImage::RemoveXRays(CameraParameters *param, void *array, int type,
                                int binning, int top, int left, int bottom, int right, 
                                float absCrit, float numSDCrit, int useBothCrit, 
                                BOOL verbose)
{
  int nReplaced, nPatches, nSkipped, nTruncated;
  float sdev;
  int numHotColumns = 0;
  int numHotPixels = 0;
  int hotColumn[MAX_HOT_COLUMNS];
  int hotPixelX[MAX_HOT_PIXELS], hotPixelY[MAX_HOT_PIXELS];
  int replacedX[MAX_REPLACED], replacedY[MAX_REPLACED];
  int i, col, row, maxSize, maxPixels, xOffset, yOffset;
  int sizeX = right - left;
  int sizeY = bottom - top;

  if (absCrit <= 0. && numSDCrit <= 0.)
    return;
  // Make table of hot columns in binned subset image
  mWinApp->mGainRefMaker->GetBinningOffsets(param, 1, binning, xOffset, yOffset);
  for (i = 0; i < param->numHotColumns; i++) {
    col = (param->hotColumn[i] - xOffset) / binning - left;
    if (col >= 0 && col < sizeX)
      hotColumn[numHotColumns++] = col;
  }

  // Make table of hot pixels in binned subset area
  for (i = 0; i < (int)param->hotPixelX.size(); i++) {
    if (param->hotPixImodCoord) {
      col = (param->hotPixelX[i] - 1 - xOffset) / binning - left;
      row = (param->sizeY - param->hotPixelY[i] - yOffset) / binning - top;
     } else {
      col = (param->hotPixelX[i] - xOffset) / binning - left;
      row = (param->hotPixelY[i] - yOffset) / binning - top;
    }
    if (col >= 0 && col < sizeX && row >= 0 && row < sizeY) {
      hotPixelX[numHotPixels] = col;
      hotPixelY[numHotPixels++] = row;
    }
  }

  // Compute max size of a patch
  maxSize = param->maxXRayDiameter / binning + 1;
  if (param->maxXRayDiameter % binning > 1)
    maxSize++;
  maxPixels = (int)(0.25 * 3.142 * maxSize * maxSize + 0.5);

  DWORD startTime = GetTickCount();
  nPatches = ProcRemoveXRays(array, type, sizeX, sizeY, numHotColumns, hotColumn, 
    numHotPixels,
    hotPixelX, hotPixelY, maxPixels, mInnerXRayDistance, mOuterXRayDistance, absCrit,
    numSDCrit, useBothCrit, mXRayCritIterations, mXRayCritIncrease, &sdev, &nReplaced,
    &nSkipped, &nTruncated, replacedX, replacedY, MAX_REPLACED);
  int elapsed = GetTickCount() - startTime;
  if (verbose) {
    CString report, fragment;
    report.Format("SD = %.2f  Time = %d  Replaced %d pixels in %d patches\r\n"
      "Truncated %d patches, and skipped %d pixels in oversized patches", 
      sdev, elapsed, nReplaced, nPatches, nTruncated, nSkipped);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    if (nPatches > MAX_REPLACED)
      nPatches = MAX_REPLACED;
    report = "";
    for (i = 0; i < nPatches; i++) {
      fragment.Format("%6d %6d     ", replacedX[i], replacedY[i]);
      report += fragment;
      if (i % 5 == 4 || i == nPatches - 1) {
        mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
        report = "";
      }
    }
  }
}


void CProcessImage::OnProcessFixdarkxrays() 
{
  FixXRaysInBuffer(false);  
}


void CProcessImage::OnProcessFiximagexrays() 
{
  FixXRaysInBuffer(true); 
}

void CProcessImage::FixXRaysInBuffer(BOOL doImage)
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  EMimageBuffer *imBuf = mWinApp->GetActiveNonStackImBuf();
  short int *brray;
  KImage *image = imBuf->mImage;
  int nx = image->getWidth();
  int ny = image->getHeight();
  NewArray2(brray, short int, nx, ny);
  float critFac = param->unsignedImages && mCamera->GetDivideBy2() ? 2.f : 1.f;
  if (!brray) {
    AfxMessageBox("Failed to get memory for corrected image", MB_EXCLAME);
    return;
  }
  int binning = imBuf->mBinning;
  int top = 0;
  int left = 0;
  int right = nx;
  int bottom = ny;
  image->Lock();
  memcpy(brray, image->getData(), 2 * nx * ny);
  image->UnLock();
  
  if (!binning) {
    binning = 1;
    KGetOneInt("Binning is unknown for this image; enter binning:", binning);
    if (binning <= 0)
      return;
    if (binning * nx < param->sizeX) {
      left = (param->sizeX - binning * nx) / 2;
      right = param->sizeX - left;
    }
    if (binning * ny < param->sizeY) {
      top = (param->sizeY - binning * ny) / 2;
      bottom = param->sizeY - top;
    }

  }
  RemoveXRays(param, brray, image->getType(), binning, top, left, bottom,
    right, (doImage ? param->imageXRayAbsCrit : param->darkXRayAbsCrit) / critFac,
    doImage ? param->imageXRayNumSDCrit : param->darkXRayNumSDCrit,
    doImage ? param->imageXRayBothCrit : param->darkXRayBothCrit, 1);

  NewProcessedImage(imBuf, brray, image->getType(), nx, ny, 1);
}

void CProcessImage::OnSetdarkcriteria() 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  KGetOneFloat("Absolute deviation from mean for X-ray in dark reference (0 to not use)",
    param->darkXRayAbsCrit, 0);
  KGetOneFloat("Criterion number of standard deviations from mean (0 to not use)",
    param->darkXRayNumSDCrit, 1);
  KGetOneInt("0 to remove spot above either criterion, 1 to require both criteria",
    param->darkXRayBothCrit);
}

void CProcessImage::OnSetimagecriteria() 
{
  CameraParameters *param = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  KGetOneFloat("Absolute deviation from mean for X-ray in image (0 to not use)",
    param->imageXRayAbsCrit, 0);
  KGetOneFloat("Criterion number of standard deviations from mean (0 to not use)",
    param->imageXRayNumSDCrit, 1);
  KGetOneInt("0 to remove spot above either criterion, 1 to require both criteria",
    param->imageXRayBothCrit);
}

void CProcessImage::OnUpdateSetcriteria(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mCamera->CameraBusy()); 
}


// Test for whether true spot mean meets criterion
BOOL CProcessImage::ImageTooWeak(EMimageBuffer *imBuf, float crit)
{
  KImage *image = imBuf->mImage;
  int nx = image->getWidth();
  int ny = image->getHeight();
  double BWmean = UnbinnedSpotMeanPerSec(imBuf);
  /*double BWmean = imBuf->GetUnbinnedBWMeanPerSec();
  if (BWmean < crit) {

    // 2/12/04: switch to using a foreshortened mean
    BWmean = ForeshortenedMean((int)(imBuf - mImBufs));
    if (imBuf->mBinning && imBuf->mExposure)
      BWmean /= imBuf->mBinning * imBuf->mBinning * imBuf->mExposure;
  } */
  return BWmean < crit;
}

// Update a criterion; use true spot mean not Black/white mean
void CProcessImage::UpdateBWCriterion(EMimageBuffer *imBuf, float &crit, float derate)
{
  KImage *image = imBuf->mImage;
  int nx = image->getWidth();
  int ny = image->getHeight();
  double BWmean = derate * UnbinnedSpotMeanPerSec(imBuf);
  /*double BWmean = derate * imBuf->GetUnbinnedBWMeanPerSec();
  
  // 2/12/04: make it base first criterion on a real mean; switch to foreshortened mean
  if (BWmean < 0.75 * crit || BWmean > 1.33 * crit) {
 //   BWmean = derate * ProcImageMean(image->getData(), image->getType(), nx, ny, 0, 
 //     nx - 1, 0, ny - 1);
    BWmean = derate * ForeshortenedMean((int)(imBuf - mImBufs));
    if (imBuf->mBinning && imBuf->mExposure)
      BWmean /= imBuf->mBinning * imBuf->mBinning * imBuf->mExposure;
  }
  */
  crit = (float)BWmean;
}

float CProcessImage::UnbinnedSpotMeanPerSec(EMimageBuffer *imBuf)
{
  float recordFrac = 0.33f;;
  float spotFrac = 0.5f;;
  float bufFrac = 0.5f;
  double mean;
  KImage *image = imBuf->mImage;
  if (!image || !imBuf->mBinning || imBuf->mExposure < 1.e-4 || 
    imBuf->IsProcessed() || imBuf->mConSetUsed < 0 ||
    imBuf->mConSetUsed >= MAX_CONSETS)
    return 0.;
  int nx = image->getWidth();
  int ny = image->getHeight();
  int spotLeft, spotRight, spotTop, spotBottom, spotCenX, spotCenY;
  int bufLeft, bufRight, bufTop, bufBottom, nxBuf, nyBuf, bufArea;
  int nxSpot, nySpot, delxy, selectArea, ix0, ix1, iy0, iy1;
  ControlSet *recordSet = mWinApp->GetConSets() + 3;
  ControlSet *conSet = mWinApp->GetConSets() + imBuf->mConSetUsed;
  if (conSet->exposure < 1.e-4)
    return 0.;

  // Compute the spot area from the fraction of the record area
  nxSpot = (int)(recordFrac * (recordSet->right - recordSet->left));
  delxy = (recordSet->right - recordSet->left -nxSpot) / 2;
  spotLeft = recordSet->left + delxy;
  spotRight = recordSet->right - delxy;
  nySpot = (int)(recordFrac * (recordSet->bottom - recordSet->top));
  delxy = (recordSet->bottom - recordSet->top - nySpot) / 2;
  spotTop = recordSet->top + delxy;
  spotBottom = recordSet->bottom - delxy;
  spotCenX = (recordSet->right + recordSet->left) / 2;
  spotCenY = (recordSet->bottom + recordSet->top) / 2;

  // Get the intersection of this buffer's area with the spot area
  bufLeft = conSet->left > spotLeft ? conSet->left : spotLeft;
  bufRight = conSet->right < spotRight ? conSet->right : spotRight;
  bufTop = conSet->top > spotTop ? conSet->top : spotTop;
  bufBottom = conSet->bottom < spotBottom ? conSet->bottom : spotBottom;
  nyBuf = conSet->bottom - conSet->top;
  nxBuf = conSet->right - conSet->left;
  bufArea = nxBuf * nyBuf;
  selectArea = (bufBottom - bufTop) * (bufRight - bufLeft);
  
  // If there is no intersection, or if its area is both less than a fraction
  // of the spot area and less than a fraction of the buffer area, compute
  // most centered fractional portion of buffer area
  if (bufBottom <= bufTop || bufRight <= bufLeft || 
    (selectArea < spotFrac * (spotBottom - spotTop) * (spotRight - spotLeft) &&
    selectArea < bufFrac * bufArea)) {
    nxSpot = (int)sqrt(bufFrac * bufArea);
    nySpot = nxSpot;
    if (nxSpot > nxBuf) {
      nxSpot = nxBuf;
      nySpot = (int)(bufFrac * bufArea) / nxBuf;
    }
    if (nySpot > nyBuf) {
      nySpot = nyBuf;
      nxSpot = (int)(bufFrac * bufArea) / nyBuf;
    }

    // Get centered coordinates then shift to fit into buffer coordinates
    bufLeft = spotCenX - nxSpot / 2;
    bufRight = spotCenX + nxSpot / 2;
    delxy = 0;
    if (bufLeft < conSet->left)
      delxy = conSet->left - bufLeft;
    if (bufRight > conSet->right)
      delxy = conSet->right - bufRight;
    bufLeft += delxy;
    bufRight += delxy;
    bufTop = spotCenY - nySpot / 2;
    bufBottom = spotCenY + nySpot / 2;
    delxy = 0;
    if (bufTop < conSet->top)
      delxy = conSet->top - bufTop;
    if (bufBottom > conSet->bottom)
      delxy = conSet->bottom - bufBottom;
    bufTop += delxy;
    bufBottom += delxy;
  }

  // Compute coordinates to average
  ix0 = (bufLeft - conSet->left ) / imBuf->mBinning;
  ix1 = (bufRight - conSet->left ) / imBuf->mBinning - 1;
  iy0 = (bufTop - conSet->top ) / imBuf->mBinning;
  iy1 = (bufBottom - conSet->top ) / imBuf->mBinning - 1;
  if (ix0 < 0)
    ix0 = 0;
  if (ix1 >= nx)
    ix1 = nx - 1;
  if (iy0 < 0)
    iy0 = 0;
  if (iy1 >= ny)
    iy1 = ny - 1;

  image->Lock();
  mean = ProcImageMean(image->getData(), image->getType(), nx, ny, ix0, ix1, iy0, iy1) /
    (imBuf->mBinning * imBuf->mBinning * imBuf->mExposure * 
    mWinApp->GetGainFactor(imBuf->mCamera, imBuf->mBinning));
  image->UnLock();
  return (float)mean;
}

// Simply get the mean of the whole image in the given buffer
double CProcessImage::WholeImageMean(EMimageBuffer *imBuf)
{
  double retval;
  KImage *image = imBuf->mImage;
  if (!image)
    return 0.;
  int nx = image->getWidth();
  int ny = image->getHeight();
  image->Lock();
  retval = ProcImageMean(image->getData(), image->getType(), 
    nx, ny, 0, nx - 1, 0, ny - 1);
  image->UnLock();
  return retval;
}

void CProcessImage::OnProcessShowcrosscorr() 
{
  mShiftManager->AutoAlign(0, 0, false, true);	
}

void CProcessImage::OnUpdateProcessShowcrosscorr(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mImBufs->mImage && !mWinApp->DoingTasks() && 
    !(mWinApp->mBufferManager->GetAlignToB() && 
    mWinApp->mBufferManager->GetShiftsOnAcquire() < 2)); 
}

void CProcessImage::OnProcessAutocorrelation()
{
  float sigma1save = mShiftManager->GetSigma1();
  float sigma2save = mShiftManager->GetSigma2();
  float radius2save = mShiftManager->GetRadius2();
  mShiftManager->SetSigma1(0.);
  mShiftManager->SetSigma2(0.);
  mShiftManager->SetRadius2(0.);
  mShiftManager->AutoAlign(-1, 0, false, true);

  // Fix the scaling, 32000 is not really good
  if (mImBufs->mImageScale) {
    mImBufs->mImageScale->SetMinMax(mImBufs->mImageScale->GetMinScale(), mCorrMaxScale);
    mWinApp->SetCurrentBuffer(0);
  }
  mShiftManager->SetSigma1(sigma1save);
  mShiftManager->SetSigma2(sigma2save);
  mShiftManager->SetRadius2(radius2save);
}

void CProcessImage::OnUpdateProcessAutocorrelation(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mImBufs->mImage && !mWinApp->DoingTasks()); 
}

void CProcessImage::OnUpdateProcessMakecoloroverlay(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mCamera->CameraBusy());
}

/////////////////////////////////////////
//   PIXEL SIZE FINDING ROUTINES
/////////////////////////////////////////

void CProcessImage::OnProcessFindpixelsize()
{
  float vectors[4], dist = 0.;
  FindPixelSize(0., 0., 0., 0., 0, 0, dist, vectors);
}

void CProcessImage::OnUpdateProcessFindpixelsize(CCmdUI *pCmdUI)
{
  int cap = mImBufs->mCaptured;
  pCmdUI->Enable(mImBufs->mImage && mImBufs->mImage->getMode() == kGray && 
    !mWinApp->DoingTasks() && cap != BUFFER_PROCESSED &&
    cap != BUFFER_FFT && cap != BUFFER_LIVE_FFT); 
}

void CProcessImage::OnProcessPixelsizefrommarker()
{
  float minScale = 0, maxScale = 0; 
  float vectors[4], dist = 0.;
  KImage *image = mImBufs->mImage;
  float markedX = mImBufs->mUserPtX - (float)image->getWidth() / 2.f - 0.5f;
  float markedY = mImBufs->mUserPtY - (float)image->getHeight() / 2.f - 0.5f;
  if (mImBufs->mImageScale) {
    minScale = mImBufs->mImageScale->GetMinScale();
    maxScale = mImBufs->mImageScale->GetMaxScale();
  }
  mWinApp->mBufferManager->CopyImageBuffer(1, 0);
  FindPixelSize(markedX, markedY, minScale, maxScale, 0, 0, dist, vectors);
}

void CProcessImage::OnUpdateProcessPixelsizefrommarker(CCmdUI *pCmdUI)
{
  int cap = mImBufs[1].mCaptured;
  pCmdUI->Enable(mImBufs->mImage && !mWinApp->DoingTasks() && 
    mImBufs->mCaptured == BUFFER_PROCESSED && mImBufs->mHasUserPt && 
    mImBufs[1].mImage && mImBufs[1].mImage->getMode() == kGray && cap != BUFFER_PROCESSED &&
    cap != BUFFER_FFT && cap != BUFFER_LIVE_FFT); 
}

void CProcessImage::OnProcessSetBinnedSize()
{
  KGetOneInt("Target size to bin images to for finding pixel size, ", 
    "or 0 for automatic target size based on expected # of blocks:",
    mPixelTargetSize);
  B3DCLAMP(mPixelTargetSize, 0, 2048);
}

void CProcessImage::OnPixelsizeCatalaseCrystal()
{
  mCatalaseForPixel = !mCatalaseForPixel;
}

void CProcessImage::OnUpdatePixelsizeCatalaseCrystal(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mCatalaseForPixel ? 1 : 0);
}

void CProcessImage::OnMeshForGridBars()
{
  KGetOneInt("Enter a mesh size to measure pixel size from the spacing of grid bars",
    "   or enter 0 not to (the standard replica grating is 400):", mGridMeshSize);
}

#define MAX_GRID_PEAKS 10000
#define MAX_SCAN 64
#define MAX_AUTO_TARGET 7
#define MAX_MESS_BUF 200

// Find a pixel size either de novo or from point marked in autocorrelation
// Pass in spacing non-zero to override catalase, grid mesh, and grid lines/mm
int CProcessImage::FindPixelSize(float markedX, float markedY, float minScale, 
  float maxScale, int bufInd, int findFlags, float &spacing, float *vectors)
{
  void *data;
  float *array;
  short int *temp;
  float CTF[8193], Xpeaks[MAX_GRID_PEAKS], Ypeaks[MAX_GRID_PEAKS], peak[MAX_GRID_PEAKS];
  int numPeaks = MAX_GRID_PEAKS;
  float sigma1 = 0.02f;
  float passRatio = 0.3f;
  float cenFrac = 0.2f;
  float testFrac = 0.02f;
  float trimFrac = 0.01f;
  float taperFrac = 0.05f;
  int numScan = 8;
  float minDivDist = 3.;
  float totMaxCrit = 0.5f;
  float totDistCrit = 0.75f;
  float perpMedCrit = 0.2f;
  int targetMinBlocks[MAX_AUTO_TARGET] = {170, 125, 12, 8, 5, 3, 0};
  int autoTargetSizes[MAX_AUTO_TARGET] = {2048, 1536, 1024, 768, 512, 384, 256};
  double gridNM = B3DCHOICE(spacing != 0, 1000. / spacing, 1000000. / mGridLinesPerMM);
  float delta, corMin, corMax, tryX;
  int trimX, trimY, nxPad, nyPad, ix1, iy1, nxTaper, nyTaper, ind1, ind2, numDiv;
  double dist, pixel;
  float angle, dist1, dist2;
  double pixel1, pixel2;
  CString report, str;
  int num[2];
  KImage *image = mImBufs[bufInd].mImage;
  if (!image) {
    SEMMessageBox("There is no image in buffer passed to FindPixelSize routine");
    return 1;
  }
  int nx = image->getWidth();
  int ny = image->getHeight();
  int camera = mImBufs->mCamera;
  int size, testSize, delTrim, minTrim;
  bool readIn = mImBufs->GetSaveCopyFlag() >= 0 && mImBufs->mCaptured == 0;
  int imType = image->getType();
  int needBin = 1;
  int targetSize = mPixelTargetSize;
  float catalFac = (!spacing && mCatalaseForPixel) ? 
    2.f * mLongCatalaseNM / mShortCatalaseNM : 1.f;
  CameraParameters *param = mWinApp->GetCamParams();
  MagTable *magTab = mWinApp->GetMagTable();
  int binningShown = mImBufs->mBinning / mImBufs->mDivideBinToShow;
  image->Lock();
  data = image->getData();
  bool doCatalase = (!spacing && mCatalaseForPixel) && mGridMeshSize <= 0;
  float effectiveBin = mImBufs[bufInd].mEffectiveBin;
  int bufBin = mImBufs[bufInd].mBinning;
  bool isMontOverview = mImBufs[bufInd].IsMontageOverview() && effectiveBin > 0.;
  int magInd = mImBufs->mMagInd;
  char messBuf[MAX_MESS_BUF];
  bool printTarget = !(findFlags & FIND_PIX_NO_TARGET);

  if (!spacing && mGridMeshSize > 0)
    gridNM = 2.54e7 / mGridMeshSize;

  // Handle automatic target selection
  if (!targetSize) {
    pixel = mShiftManager->GetPixelSize(mImBufs);
    if (mGridMeshSize > 0) {
      targetSize = 512;
      if (printTarget)
        PrintfToLog("Setting target binned size to 512;\r\n   there is no automatic "
          "target size selection when using grid bars");
    } else if (doCatalase) {
      targetSize = 1024;
      if (printTarget)
        PrintfToLog("Setting target binned size to 1024;\r\n  there is no automatic "
          "target size selection when using catalase");
    } else if (pixel <= 0.) {
      targetSize = 512;
      if (printTarget)
        PrintfToLog("Setting target binned size to 512;\r\n   the read-in image lacks "
          "information needed for automatic target size selection");
    } else {
      numDiv = (int)(pixel * B3DMIN(nx, ny) / (0.001 * gridNM));
      for (ind1 = 0; ind1 < MAX_AUTO_TARGET; ind1++)
        if (numDiv >= targetMinBlocks[ind1])
          break;
      targetSize = autoTargetSizes[ind1];
      if (printTarget)
        PrintfToLog("Setting target binned size to %d because image is expected to have "
          "%d repeats", targetSize, numDiv);
    }
  }

  // First bin down to target size if needed
  while (B3DMAX(nx, ny) > targetSize * needBin)
    needBin++;
  if (needBin > 1) {
    NewArray2(temp, short int, (nx / needBin), (ny / needBin) * 
      (imType == kFLOAT ? 2 : 1));
    if (!temp) {
      SEMMessageBox("Failed to get memory for binned image", MB_EXCLAME);
      return 1;
    }
    XCorrBinByN(data, imType, nx, ny, needBin, temp);
    nx /= needBin;
    ny /= needBin;
    if (imType == kUBYTE || imType == kRGB)
      imType = kSHORT;
    data = temp;
  }

  // Find trimming in case it is inside grid bars
  size = B3DMIN(nx, ny);
  testSize = (int)(testFrac * size);
  delTrim = testSize / 2;
  minTrim = (int)(trimFrac * size);
  trimX = trimY = minTrim;
  if (!(findFlags & FIND_PIX_NO_TRIM))
    ProcTrimCircle(data, imType, nx, ny, cenFrac, testSize, delTrim, minTrim,
      minTrim, passRatio, true, trimX, trimY, ix1, iy1);
  ix1 = nx - trimX - 1;
  iy1 = ny - trimY - 1;

  // Get padded size and array
  nxPad = XCorrNiceFrame(2 * (ix1 + 1 - trimX), 2, 19);
  nyPad = XCorrNiceFrame(2 * (iy1 + 1 - trimY), 2, 19);
  NewArray2(array, float, (nxPad + 2), nyPad);
  if (!array) {
    SEMMessageBox("Failed to get memory for autocorrelation array", MB_EXCLAME);
    return 1;
  }

  // Get tapering and pad the data
  nxTaper = (int)(taperFrac * (ix1 + 1 - trimX));
  nyTaper = (int)(taperFrac * (iy1 + 1 - trimY));
  XCorrTaperInPad(data, imType, nx, trimX, ix1, trimY, iy1, array,
        nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);
  image->UnLock();
  if (needBin > 1)
    delete [] temp;
  
  // Set up the filter and get the cross-correlation and find peaks
  XCorrSetCTF(sigma1, 0.f, 0.f, 0.f, CTF, nxPad, nyPad, &delta);
  XCorrCrossCorr(array, array, nxPad, nyPad, delta, CTF);

  ind2 = findAutoCorrPeaks(array, nxPad, nyPad, &Xpeaks[0], &Ypeaks[0], &peak[0], numPeaks,
    doCatalase ? 16 : 64, catalFac, findFlags & FIND_PIX_NO_WAFFLE, markedX, markedY, 
    &dist1, &dist2, &angle, vectors, num, &ind1, &messBuf[0], MAX_MESS_BUF);

  // Now take care of display regardless, display point
  if (!(findFlags & FIND_PIX_NO_DISPLAY) &&
    !CorrelationToBufferA(array, nxPad, nyPad, 1, corMin, corMax)) {
    if (ind2 >= 0) {
      delta = 32000.f * (peak[1] - corMin) / (corMax - corMin);
      delta = delta + B3DMIN(0.1f * (32000.f - delta), 0.2f * delta);
      if (mImBufs->mImageScale) {
        if (!minScale && !maxScale) {
          minScale = mImBufs->mImageScale->GetMinScale();
          maxScale = delta;
        }
        mImBufs->mImageScale->SetMinMax(minScale, maxScale);
      }
      mImBufs->mUserPtX = nxPad / 2 + Xpeaks[ind1] + 0.5f;
      mImBufs->mUserPtY = nyPad / 2 + Ypeaks[ind1] + 0.5f;
      mImBufs->mHasUserPt = true;
      mImBufs->mHasUserLine = false;
      mImBufs->mDrawUserBox = false;

      // For some reason, the display gives boxes the same size if I don't adjust binning
      if (isMontOverview) {
        mImBufs->mCaptured = BUFFER_AUTOCOR_OVERVIEW;
        mImBufs->mEffectiveBin =(float)(effectiveBin * needBin);
      }
      mImBufs->mBinning = bufBin * needBin;
    }
    mWinApp->SetCurrentBuffer(0);
  }
  delete [] array;

  if (ind2) {
    SEMMessageBox(messBuf);
    return 1;
  }

  dist1 *= needBin;
  dist2 *= needBin;
  for (ind1 = 0; ind1 < 4; ind1++)
    vectors[ind1] *= needBin;
  spacing = (dist1 + dist2) / 2.f;
  if (findFlags & FIND_PIX_NO_WAFFLE)
    return 0;

  if (doCatalase) {
    pixel1 = mShortCatalaseNM / dist1;
    pixel2 = 2. * mLongCatalaseNM / dist2;
    pixel = (pixel1 + pixel2) / 2.;
    report.Format("Pixel = %.4g from %d short intervals or %.4g from %d long intervals; "
      "(binned) pixel = %.4g nm, grid angle = %.2f deg", pixel1, num[0], pixel2, num[1],
      pixel, angle);
  } else {
    dist = (dist1 + dist2) / 2.;
    pixel = (gridNM / dist);
    report.Format("From %d intervals of %.2f and %d intervals of %.2f, (binned) pixel ="
      " %.4g nm, grid angle = %.2f deg", num[0], dist1, num[1], dist2, pixel, angle);
  }
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  if (magInd && camera >= 0 && binningShown &&
    magTab[magInd].pixelSize[camera]) {
      dist = 0.001 * pixel / 
        (mImBufs[1].mBinning * magTab[magInd].pixelSize[camera]);
      PrintfToLog("   this is %.3f times previous %s pixel size", dist, 
        magTab[magInd].pixDerived[camera] ? "derived" : "measured");
  }

  if (binningShown) {
    pixel /= binningShown;
    ind1 = (magInd && camera >= 0) ? 
      MagForCamera(camera, magInd) : 0;
    report.Format("Camera %d, %dx, %snbinned pixel = %.4g nm", camera, ind1, 
      readIn ? "APPARENT u" : "U", pixel);
    if (doCatalase) {
      str.Format(" (%.4g nm short axis only)", pixel1 / binningShown);
      report += str;
    }
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    if (magInd && !readIn && camera >= 0) {
      report.Format("%d   %.4g   %d", magInd, pixel, ind1);
      mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    }
    if (magInd && camera >= 0) {
      ind1 = LookupFoundPixelSize(camera, magInd);
      if (ind1 < 0) {
        mPixSizMagInd.push_back(magInd);
        mPixSizCamera.push_back(camera);
        mPixelSizes.push_back((float)pixel);
        mGridRotations.push_back((float)angle);

        // Try to adjust this added rotation if previous mag was measured
        ind2 = LookupFoundPixelSize(camera, magInd - 1);
        ind1 = 0;
        if (FEIscope && ind2 >= 0 && magInd > 0 && magTab[magInd - 1].mag > 0) {
          dist1 = angle - mGridRotations[ind2];
          if (fabs(dist1) < 45.) {
            tryX = B3DCHOICE(param[camera].GIF, 
              magTab[magInd].EFTEMtecnaiRotation - magTab[magInd - 1].EFTEMtecnaiRotation,
              magTab[magInd].tecnaiRotation - magTab[magInd - 1].tecnaiRotation);
            ind1 = 90 * B3DNINT(UtilGoodAngle(tryX - dist1) / 90.);
            if (ind1)
              PrintfToLog("Set added rotation to %d for this mag based on rotations"
                " in the magnification table", ind1);
          }
        }
        mAddedRotation.push_back(ind1);

        // And try to adjust the NEXT added rotation if that was measured
        ind2 = LookupFoundPixelSize(camera, magInd + 1);
        if (FEIscope && ind2 >= 0 && magTab[magInd + 1].mag > 0 && !mAddedRotation[ind2]){
          dist1 = mGridRotations[ind2] - angle;
          if (fabs(dist1) < 45.) {
            tryX = B3DCHOICE(param[camera].GIF, 
              magTab[magInd + 1].EFTEMtecnaiRotation - magTab[magInd].EFTEMtecnaiRotation,
              magTab[magInd + 1].tecnaiRotation - magTab[magInd].tecnaiRotation);
            ind1 = 90 * B3DNINT(UtilGoodAngle(tryX - dist1) / 90.);
            if (ind1) {
              PrintfToLog("Set added rotation to %d for next higher mag based on "
                "rotations in the magnification table", ind1);
              mAddedRotation[ind2] = ind1;
            }
          }
        }
     } else {
        mPixelSizes[ind1] = (float)pixel;
        mGridRotations[ind1] = (float)angle;
      }
      mPixelTimeStamp = mWinApp->MinuteTimeStamp();
      mWinApp->mDocWnd->SetShortTermNotSaved();
    }
  }
  return 0;
}

// Output the relative rotations between mags with pixel size measurements
void CProcessImage::OnListRelativeRotations()
{
  CString mess = "";
  CString camMess;
  float diff, alternate;
  int mag, cam, lastInd, vecInd, xmag;
  for (cam = 0; cam < MAX_CAMERAS; cam++) {
    camMess.Format("\r\nFor camera # %d:\r\n", cam);
    lastInd = -1;
    for (mag = 1; mag < MAX_MAGS; mag++) {
      vecInd = LookupFoundPixelSize(cam, mag);
      if (vecInd >= 0) { 
        xmag = MagForCamera(cam, mag);
        if (lastInd >= 0) {
          diff = mGridRotations[vecInd] - mGridRotations[lastInd];
          if (diff < 45 && diff > -45) {
            mess.Format("RotationAndPixel %2d %7.2f  999 %7.4g   # %d, p=%.0f", mag, 
              UtilGoodAngle(diff + mAddedRotation[vecInd]), 
              mPixelSizes[vecInd], xmag, xmag * mPixelSizes[vecInd]);
          } else {
            if (diff > 45)
              alternate = diff + mAddedRotation[vecInd] - 90.f;
            else
              alternate = diff + mAddedRotation[vecInd] + 90.f;
            mess.Format("# Verify ambiguous relative rotation between mags "
              "# %d and # %d and edit line below\r\n"
              "RotationAndPixel  %2d  %.2f  (or %.2f?)  999  %.4g   # %d, p=%.0f", 
              (mag - 1), mag, mag, UtilGoodAngle(diff + mAddedRotation[vecInd]), 
              UtilGoodAngle(alternate), mPixelSizes[vecInd], xmag,
              xmag * mPixelSizes[vecInd]);
          } 
        } else {
          mess.Format("RotationAndPixel %2d   999    999 %7.4g   # %d, p=%.0f", mag, 
            mPixelSizes[vecInd], xmag, xmag * mPixelSizes[vecInd]);
        }
        if (!camMess.IsEmpty()) {
          mWinApp->AppendToLog(camMess);
          camMess = "";
        }
        mWinApp->AppendToLog(mess);
      }
      lastInd = vecInd;
    }
  }
  if (mess.IsEmpty())
    AfxMessageBox("No relative rotations are available", MB_OK | MB_ICONINFORMATION);
  else
    mWinApp->AppendToLog("\r\n\"p\" (mag * pixel size) is expected to be similar "
      "at all magnifications, re-check pixel size calibrations of outliers");
}

// Add to the rotation for one pixel size (is this worth it?)
void CProcessImage::OnPixelsizeAddToRotation()
{
  int camera = mWinApp->GetCurrentCamera(), magInd = 0, angle = 0;
  if (mWinApp->GetActiveCamListSize() > 1 &&
    !KGetOneInt("Camera number for which to add the rotation:", camera))
    return;
  if (!KGetOneInt("Magnification index at which the rotation occurs:", magInd))
    return;
  int vecInd = LookupFoundPixelSize(camera, magInd);
  if (vecInd < 0) {  
    AfxMessageBox("The pixel size has not been measured for this camera and mag"
      " since the program was started", MB_EXCLAME);
    return;
  }
  if (!KGetOneInt("Enter approximate angle by which an image at this mag is rotated "
    "relative to the next lower mag", 
    "I.e, enter a multiple of +/-90 to add to relative rotation:", angle))
    return;
  mAddedRotation[vecInd] = (B3DNINT(angle / 90.) * 90);
}

// Return a pixel size from current calibrations if any, or 0
float CProcessImage::GetFoundPixelSize(int camera, int magInd)
{ 
  int vecInd = LookupFoundPixelSize(camera, magInd);
  if (vecInd < 0)
    return 0.;
  return mPixelSizes[vecInd];
}

// Find the index of a measurement at the given mag
int CProcessImage::LookupFoundPixelSize(int camera, int magInd)
{
  for (int vecInd = 0; vecInd < (int)mPixSizCamera.size(); vecInd++)
    if (mPixSizCamera[vecInd] == camera && mPixSizMagInd[vecInd] == magInd)
      return vecInd;
  return -1;
}

/////////////////////////////////////////
//   COLOR OVERLAY ROUTINES
/////////////////////////////////////////

// Combines 1-3 images into a single color image using their scaled pixmaps
// returns false for an error
bool CProcessImage::OverlayImages(EMimageBuffer *redBuf, EMimageBuffer *grnBuf, 
                                  EMimageBuffer *bluBuf)
{
  CString str;
  int i, ix, iy, xsize = -1, ysize = -1;
  char *color[3] = {"red", "green", "blue"};
  EMimageBuffer *imbufs[3], *firstBuf;
  unsigned char *array, *inptr, *outptr;
  KImage *rect;
  BOOL gotScale = false;
  ScaleMat aMat;
  float delX, delY;
  imbufs[0] = redBuf;
  imbufs[1] = grnBuf;
  imbufs[2] = bluBuf;
  
  // Check the images for existence, gray scale, and matching sizes
  for (i = 0; i < 3; i++) {
    if (!imbufs[i])
      continue;
    if (!imbufs[i]->mImage) {
      str.Format("Cannot overlay these images.\r\n"
        "There is no image in the buffer for the %s channel", color[i]);
      AfxMessageBox(str, MB_EXCLAME);
      return false;
    }

    if (imbufs[i]->mImage->getMode() != kGray) {
      str.Format("Cannot overlay these images.\r\n"
        "The one for the %s channel is already a color image", color[i]);
      AfxMessageBox(str, MB_EXCLAME);
      return false;
    }
    imbufs[i]->UpdatePixMap();

    if (xsize < 0) {
      xsize = imbufs[i]->mImage->getWidth();
      ysize = imbufs[i]->mImage->getHeight();
      firstBuf = imbufs[i];
    } else if (xsize != imbufs[i]->mImage->getWidth() ||
      ysize != imbufs[i]->mImage->getHeight()) {
      AfxMessageBox("Cannot overlay these images; they are not all the same size", 
        MB_EXCLAME);
      return false;
    }
  }

  if (xsize < 0) {
    AfxMessageBox("There is no image in any of the buffers to overlay", MB_EXCLAME);
    return false;
  }

  NewArray2(array, unsigned char, 3 * xsize, ysize);
  if (!array) {
    AfxMessageBox("Failed to get memory for overlaying the images", MB_EXCLAME);
    return false;
  }
  for (ix = 0; ix < 3 * xsize * ysize; ix++)
    array[ix] = 0;

  for (i = 0; i < 3; i++) {
    if (!imbufs[i])
      continue;
    if (!gotScale)
      gotScale = mWinApp->mNavigator->BufferStageToImage(imbufs[i], aMat, delX, delY);
    rect = imbufs[i]->mPixMap->getImRectPtr();
    rect->Lock();

    for (iy = 0; iy < ysize; iy++) {
      inptr = (unsigned char *)rect->getRowData(iy);
      outptr = &array[3 * iy * xsize] + i;
      for (ix = 0; ix < xsize; ix++) {
        *outptr = inptr[ix];
        outptr += 3;
      }
    }
    rect->UnLock();
  }

  NewProcessedImage(firstBuf, (short *)array, SLICE_MODE_RGB, xsize, ysize, 1);
  if (gotScale) {
    mImBufs->mStage2ImMat = aMat;
    mImBufs->mStage2ImDelX = delX;
    mImBufs->mStage2ImDelY = delY;
  }

  return true;
}

void CProcessImage::OnProcessMakecoloroverlay()
{
  EMimageBuffer *imbuf[3];
  int i, bufind;
  if (!KGetOneString("Examples for two color overlays: ABA = magenta && green; "
    "AAB = yellow && blue; AB0 = red && green",
    "Enter a buffer letter or 0 for each color channel (red, green, blue):", mOverlayChannels))
    return;
  i = mOverlayChannels.GetLength();
  if (i != 3) {
    AfxMessageBox("You did not enter 3 letters or 0", MB_EXCLAME);
    if (i > 3)
      mOverlayChannels.Truncate(3);
    else if (i == 2) 
      mOverlayChannels += '0';
    else
      mOverlayChannels = "ABA";
    return;
  }
  mOverlayChannels = mOverlayChannels.MakeUpper();
  for (i = 0; i < 3; i++) {
    bufind = (int)mOverlayChannels.GetAt(i) - (int)'A';
    if (bufind == (int)'0' - (int)'A')
      imbuf[i] = NULL;
    else if (bufind < 0 || bufind >= MAX_BUFFERS) {
      AfxMessageBox("You did not enter 3 letters in range of buffer numbers, or 0", 
        MB_EXCLAME);
      mOverlayChannels = "ABA";
      return;
    } else {
      imbuf[i] = &mImBufs[bufind];
    }
  }
  OverlayImages(imbuf[0], imbuf[1], imbuf[2]);

}

// This didn't work with my test images
void CProcessImage::OnProcessCropAverage()
{
  int boxXsize, boxYsize, top, left, nx, ny, nxPad, nyPad, nxTaper, nyTaper, numAvg, mode;
  int groupID, ind, loop, pad, csize, dsize, xpeakLimit, ypeakLimit;
  size_t boxDim, corrDim, i;
  CNavigatorDlg *nav = mWinApp->mNavigator;
  float taperFrac = 0.1f;
  float padFrac = 0.1f;
  float findLimitFrac = 0.1f;
  float peak, xpeak, ypeak, dxSum, dySum, ctfDelta, ptX, ptY, edge;
  float unitMat[2][2] = {1., 0., 0., 1.};
  CString label, lastlab;
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  KImage *image = imBuf->mImage;
  int zero = 0, one = 1, error = 0;
  float sigma1 = 0.03f, sigma2 = 0.05f, radius1 = 0., radius2 = 0.3f;
  float ctf[8193];
  void *data;
  float *refArray, *boxArray, *sumArray, *tmpArray;
  short *finalArray;
  unsigned short *usFinal;
  std::vector<Islice *> boxSlices, xformSlices;
  std::vector<float> xShifts, yShifts;
  IntVec itemIndex;
  Islice bufSlice;
  Islice *boxSlice;
  CMapDrawItem *item;
  CArray<CMapDrawItem *, CMapDrawItem *>*itemArray = 
    mWinApp->mMainView->GetMapItemsForImageCoords(imBuf, true);

  if (!itemArray) {
    AfxMessageBox("An error occurred getting the Navigator item array", MB_EXCLAME);
    return;
  }
  mode = image->getType();
  if (sliceModeIfReal(mode) < 0) {
    AfxMessageBox("You cannot average a color image", MB_EXCLAME);
    return;
  }
  mrc_getdcsize(mode, &dsize, &csize);

  image->getSize(nx, ny);
  boxXsize = (int)ceil((double)B3DMAX(imBuf->mUserPtX, imBuf->mLineEndX)) + 1 -
    (int)floor((double)B3DMIN(imBuf->mUserPtX, imBuf->mLineEndX));
  boxYsize = (int)ceil((double)B3DMAX(imBuf->mUserPtY, imBuf->mLineEndY)) + 1 -
    (int)floor((double)B3DMIN(imBuf->mUserPtY, imBuf->mLineEndY));
  boxXsize = 2 * ((boxXsize + 1) / 2);
  if (boxXsize < 10 || boxYsize < 10 || boxXsize > nx || boxYsize > ny) {
    AfxMessageBox("The box size or the image is too small", MB_EXCLAME);
    return;
  }
  xpeakLimit = B3DNINT(findLimitFrac * boxXsize);
  ypeakLimit = B3DNINT(findLimitFrac * boxYsize);

  groupID = nav->OKtoAverageCrops();
  numAvg = nav->CountItemsInGroup(groupID, label, lastlab, top, &itemIndex);

  // Get the image and set the slice mean
  image->Lock();
  data = image->getData();
  sliceInit(&bufSlice, nx, ny, mode, data);
  ProcSampleMeanSD(data, mode, nx, ny, &bufSlice.mean, &peak);

  // Extract all the boxes
  for (ind = 0; ind < numAvg; ind++) {
    item = itemArray->GetAt(itemIndex[ind]);
    mWinApp->mMainView->GetItemImageCoords(imBuf, item, ptX, ptY);
    left = B3DNINT(ptX - boxXsize / 2.);
    top = B3DNINT(ptY - boxYsize / 2.);
    boxSlice = sliceCreate(boxXsize, boxYsize, MRC_MODE_FLOAT);
    if (boxSlice) {
      xformSlices.push_back(boxSlice);
      boxSlice = sliceBox(&bufSlice, left, top, left + boxXsize, top + boxYsize);
    }
    if (!boxSlice || sliceFloat(boxSlice)) {
      for (i = 0; i < (unsigned int)ind; i++)
        sliceFree(boxSlices[i]);
      for (i = 0; i < (int)xformSlices.size(); i++)
        sliceFree(xformSlices[i]);
      AfxMessageBox("Failed to get memory for boxing out items", MB_EXCLAME);
      image->UnLock();
      return;
    }
    boxSlices.push_back(boxSlice);
  }
  image->UnLock();

  // Set up tapering/padding and get arrays
  pad = B3DMAX(4, (int)(padFrac * boxXsize));
  nxPad = XCorrNiceFrame(boxYsize + 2 * pad, 2, 19);
  pad = B3DMAX(4, (int)(padFrac * boxYsize));
  nyPad = XCorrNiceFrame(boxYsize + 2 * pad, 2, 19);
  nxTaper = B3DMAX(4, (int)(taperFrac * boxXsize));
  nyTaper = B3DMAX(4, (int)(taperFrac * boxYsize));
  boxDim = (size_t)boxXsize * boxYsize;
  corrDim = ((size_t)nxPad + 2) * nyPad;
  NewArray(sumArray, float, boxDim);
  NewArray(tmpArray, float, boxDim);
  NewArray(finalArray, short, boxDim);
  NewArray(refArray, float, corrDim);
  NewArray(boxArray, float, corrDim);
  if (!refArray || !boxArray || !sumArray || !tmpArray || !finalArray) {
    AfxMessageBox("Failed to get arrays for averaging", MB_EXCLAME);
    error = 1;
  }
  XCorrSetCTF(sigma1, sigma2, radius1, radius2, ctf, nxPad, nyPad, &ctfDelta);


  // Loop twice, first aligning to the first one, then aligning to a sum
  for (loop = 0; loop < 3 && !error; loop++) {
    dxSum = dySum = 0.;
    xShifts.clear();
    yShifts.clear();
    for (ind = 0; ind < numAvg; ind++) {
      XCorrTaperInPad(boxSlices[ind]->data.f, MRC_MODE_FLOAT, boxXsize, 0, boxXsize - 1, 
        0, boxYsize - 1, boxArray, nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);

      // First loop, copy padded box to reference array for first item
      if (!ind && !loop)
        memcpy(refArray, boxArray, 4 * corrDim);

      // Otherwise get reference as sum minus this transformed box
      if (loop) {
        for (i = 0; i < boxDim; i++)
          tmpArray[i] = sumArray[i] - xformSlices[ind]->data.f[i];
        XCorrTaperInPad(tmpArray, MRC_MODE_FLOAT, boxXsize, 0, boxXsize - 1, 0, 
          boxYsize - 1, refArray, nxPad + 2, nxPad, nyPad, nxTaper, nyTaper);
      }
       
      // Prepare the reference once or each time
      if (loop || !ind) {
        XCorrMeanZero(refArray, nxPad + 2, nxPad, nyPad);
        twoDfft(refArray, &nxPad, &nyPad, &zero);
        XCorrFilterPart(refArray, refArray, nxPad, nyPad, ctf, ctfDelta);
      }

      // Except first time, transform box, get product, back-transform, find peak
      xpeak = ypeak = 0.;
      if (loop) { // || ind) {
        XCorrMeanZero(boxArray, nxPad + 2, nxPad, nyPad);
        twoDfft(boxArray, &nxPad, &nyPad, &zero);
        conjugateProduct(boxArray, refArray, nxPad, nyPad);
        twoDfft(boxArray, &nxPad, &nyPad, &zero);
        setPeakFindLimits(-xpeakLimit, xpeakLimit, -ypeakLimit, ypeakLimit, 1);
        XCorrPeakFind(boxArray, nxPad + 2, nyPad, &xpeak, &ypeak, &peak, 1);
      }
      xShifts.push_back(-xpeak);
      yShifts.push_back(-ypeak);
      dxSum -= xpeak;
      dySum -= ypeak;
      SEMTrace('1', "Loop %d  pt %d  shift %.2f  %.2f", loop, ind, -xpeak, -ypeak);
    }
    if (error)
      break;

    // Get the average; adjust shifts by their mean
    memset(sumArray, 0, 4 * boxDim);
    for (ind = 0; ind < numAvg; ind++) {
      xShifts[ind] -= dxSum / numAvg;
      yShifts[ind] -= dySum / numAvg;

      // Transform the box into xform slice and add into sum
      edge = (float)sliceEdgeMean(boxSlices[ind]->data.f, boxXsize, 0, boxXsize - 1, 0,
        boxYsize - 1);
      cubinterp(boxSlices[ind]->data.f, xformSlices[ind]->data.f, boxXsize, boxYsize, 
        boxXsize, boxYsize, unitMat, boxXsize / 2.f, boxYsize / 2.f, 
        xShifts[ind], yShifts[ind], 1., edge, 1);
      for (i = 0; i < boxDim; i++)
        sumArray[i] += xformSlices[ind]->data.f[i] / numAvg;
    }
  }

  if (!error) {
    if (mode == MRC_MODE_USHORT) {
      usFinal = (unsigned short *)finalArray;
      for (i = 0; i < boxDim; i++)
        usFinal[i] = (unsigned short)sumArray[i];
    } else {
      mode = MRC_MODE_SHORT;
      for (i = 0; i < boxDim; i++)
        finalArray[i] = (short)sumArray[i];
    }
    NewProcessedImage(imBuf, finalArray, mode, boxXsize, boxYsize, 1, BUFFER_PROCESSED);
    finalArray = NULL;
  }

  // Cleanup at end or after error
  delete [] sumArray;
  delete [] tmpArray;
  delete [] boxArray;
  delete [] refArray;
  delete [] finalArray;
  for (ind = 0; ind < numAvg; ind++) {
    sliceFree(boxSlices[ind]);
    sliceFree(xformSlices[ind]);
  }
 }
 
// Looks for bad stripes in an image at the specified correlation, returns the number of
// transitions found; for K2/K3, finds out how many are close to expected locations
int CProcessImage::CheckForBadStripe(EMimageBuffer *imBuf, int horizontal, int &numNear)
{
  int nx, ny, numSums, iy, ix, isum, isumSq, type, idiff, retval = 0;
  int csizeX, csizeY, tLeft, tTop, tBot, tRight, tsizeX, tsizeY, k2Ind = -1;
  int indent = 8, numRoll = 32, nearCrit = 4;
  int firstExpected[2] = {511, 576};
  int deltaExpected[2] = {512, 576};
  int *intSums, *intSumSq;
  float *colDiffs, *temp;
  float crit = 18.;   // SD's about mean using trimmed mean/SD
  float mean, SD, sem, median = 0., MADN = 1.;
  double allSum = 0.;
  bool lastHigh = false;
  unsigned short *usdata, *usdata2;
  short *data, *data2;
  CameraParameters *camParam; 
  ControlSet *cset, *conSets = mWinApp->GetCamConSets();
  numNear = 0;
  if (!imBuf || !imBuf->mImage)
    return 0;
  KImage *image = imBuf->mImage;
  type = image->getType();
  if (type != kSHORT && type != kUSHORT)
    return 0;

  // For a K2/K3 camera, get the control set
  if (imBuf->mCamera >= 0 && imBuf->mConSetUsed >= 0 && imBuf->mBinning > 0) {
    camParam = mWinApp->GetCamParams() + imBuf->mCamera;
    if (camParam->K2Type) {
      k2Ind = camParam->K2Type == K3_TYPE ? 1 : 0;
      cset = &conSets[imBuf->mConSetUsed + imBuf->mCamera * MAX_CONSETS];
    }
  }

  // Get arrays for sums and Z scores
  image->getSize(nx, ny);
  numSums = (horizontal ? ny : nx) - 2;
  intSums = new int[numSums];
  intSumSq = new int[numSums];
  colDiffs = new float[numSums];
  temp = new float[numSums];
  for (ix = 0; ix < numSums; ix++) {
    intSums[ix] = 0;
    intSumSq[ix] = 0;
  }

  // Add up the difference across two pixels in the columns or rows
  image->Lock();
  for (iy = 0; iy < ny - (horizontal ? 2 : 0); iy++) {
    data = (short *)image->getRowData(iy);
    usdata = (unsigned short *)data;
    if (horizontal) {
      isum = isumSq = 0;
      if (iy == ny - 1)
        continue;
      data2 = (short *)image->getRowData(iy + 2);
      usdata2 = (unsigned short *)data2;
      if (type == kSHORT) {
        for (ix = 0; ix < nx; ix++) {
          idiff = (int)*data++ - (int)*data2++;
          isumSq += idiff * idiff;
          isum += idiff;
        }
      } else {
        for (ix = 0; ix < nx; ix++) {
          idiff = (int)*usdata++ - (int)*usdata2++;
          isumSq += idiff * idiff;
          isum += idiff;
        }
      }
      intSums[iy] = isum;
      intSumSq[iy] = isumSq;
    } else {
      if (type == kSHORT) {
        for (ix = 0; ix < nx - 2; ix++) {
          idiff = (int)data[ix] - (int)data[ix + 2];
          intSumSq[ix] += idiff * idiff;
          intSums[ix] += idiff;
        }
      } else {
        for (ix = 0; ix < nx - 2; ix++) {
          idiff = (int)usdata[ix] - (int)usdata[ix + 2];
          intSumSq[ix] += idiff * idiff;
          intSums[ix] += idiff;
        }
      }
    }
  }
  image->UnLock();

  // Get a mean and SD of the differences and get a Z score
  for (ix = 0; ix < numSums; ix++) {
    sumsToAvgSD((float)intSums[ix], (float)intSumSq[ix], (horizontal ? nx : ny), &mean, 
      &SD);
    colDiffs[ix] = (float)fabs(mean / B3DMAX(1.e-6, SD));
  }
  /*avgSD(colDiffs, numSums, &leftMean, &leftSD, &sem);
  rsMedian(colDiffs, numSums, temp, &median);
  rsMADN(colDiffs, numSums, median, temp, &MADN);
  PrintfToLog("mean diff Z %.3f  sd  %.3f  med %.3f  MADN %.3f", mean, SD, median,MADN);*/

  // Sort them and exclude the expected number of higher points from 4 transitions
  // Without this the SD is horribly skewed upwards by a few transitions
  // The MADN is actually close to the trimmed SD but usuing the SD may be more 
  // representative of the high end of the range below actual transitions
  memcpy(temp, colDiffs, 4 * numSums);
  rsSortFloats(temp, numSums);
  avgSD(temp, numSums - 12, &mean, &SD, &sem);
  //PrintfToLog("Trimmed mean diff Z %.3f  sd  %.3f", mean, SD);

  // Search for Z scores above the criterion number SD's from the mean
  for (ix = indent; ix < numSums - indent; ix++) {
    if ((colDiffs[ix] - mean) > crit * SD) {
      /*PrintfToLog("ix = %d, zdiff %.2f  Z of that %.2f", ix, colDiffs[ix], 
        (colDiffs[ix] - mean) / SD); */

      // Only record the first one in a contiguous series
      if (!lastHigh) {
        if (k2Ind >= 0) {

          // Get the location of this line on the chip as tLeft
          tLeft = cset->left / imBuf->mBinning;
          tTop = cset->top / imBuf->mBinning;
          tRight = cset->right / imBuf->mBinning;
          tBot = cset->bottom / imBuf->mBinning;

          // Need to set both sides to the actual coordinate of the line since either one
          // may end up as the left coordinate after the rotation/flip
          if (horizontal) {
            tTop += ix + 1;
            tBot = tTop;
          } else {
            tLeft += ix + 1;
            tRight = tLeft;
          }
          csizeX = camParam->sizeX;
          csizeY = camParam->sizeY;
          tsizeX = tRight - tLeft;
          tsizeY = tBot - tTop;
          if (camParam->rotationFlip)
            CorDefUserToRotFlipCCD(camParam->rotationFlip, imBuf->mBinning, csizeX, csizeY,
              tsizeX, tsizeY, tTop, tLeft, tBot, tRight);

          // Test positions for proximity
          for (iy = 0; iy < 20; iy++) {
            idiff = (firstExpected[k2Ind] + iy * deltaExpected[k2Ind]) / imBuf->mBinning;
            if (fabs((double)tLeft - idiff) < nearCrit)
              numNear++;
            if (idiff - tLeft > tsizeX)
              break;
          }
        }
        retval++;
      }
      lastHigh = true;
    } else
      lastHigh = false;
  }

  // Clean up and return
  delete[] temp;
  delete[] intSums;
  delete[] intSumSq;
  delete[] colDiffs;
  return retval;
}

void CProcessImage::OnUpdateProcessCropAverage(CCmdUI *pCmdUI)
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(!mWinApp->DoingTasks() && imBuf && imBuf->mImage && imBuf->mHasUserLine
    && mWinApp->mNavigator && mWinApp->mNavigator->OKtoAverageCrops());
}

///////////////////////////////////////////////////////
//  FFT zero routines
///////////////////////////////////////////////////////

// Returns the defocus of the user point, if the buffer is an FFT and parameters are
// provided for drawing rings at zeros.  Returns ring radii as fractions of Nyquist if
// radii is not NULL.
bool CProcessImage::GetFFTZeroRadiiAndDefocus(EMimageBuffer *imBuf, FloatVec *radii, 
  double &defocus, float phase)
{
  double pointRad, xcen, ycen, dx, dy;
  int nx, ny;
  if (!(imBuf->mCaptured == BUFFER_FFT || imBuf->mCaptured == BUFFER_LIVE_FFT) || 
    mSphericalAber <= 0. || !mNumFFTZeros || !imBuf->mHasUserPt || !imBuf->mImage)
    return false;
  float pixel = 1000.f * mShiftManager->GetPixelSize(imBuf);
  if (mTestCtfPixelSize)
    pixel = mTestCtfPixelSize;
  if (!pixel)
    return false;
  imBuf->mImage->getSize(nx, ny);
  xcen = nx / 2.;
  ycen = ny / 2.;
  if (fabs(imBuf->mUserPtX - xcen) <= 10. && fabs(imBuf->mUserPtY - ycen) <= 10.)
    return false;
  dx = (imBuf->mUserPtX - xcen) / xcen;
  dy = (imBuf->mUserPtY - ycen) / ycen;
  pointRad = sqrt(dx * dx + dy * dy);
  if (pointRad > 1.2)
    return false;
  return DefocusFromPointAndZeros(pointRad, 1, pixel, 0., radii, defocus, phase);
}

// If there are already rings at zeros and en existing user point, takes the values for
// a new mouse click and finds if it near an existing drawn ring; computes the new defocus
// based on the click being at that zero, and adjusts the point to be at first zero for 
// that defocus
void CProcessImage::ModifyFFTPointToFirstZero(EMimageBuffer *imBuf, float &shiftX, 
  float &shiftY)
{
  FloatVec radii, adjRadii;
  int nx, ny, ind, num;
  float lastRad, nextRad;
  double pointRad, critBelow, critAbove, defocus, xcen, ycen, dx, dy;
  if (mNumFFTZeros < 2 || !GetFFTZeroRadiiAndDefocus(imBuf, &radii, defocus))
    return;
  float pixel = 1000.f * mShiftManager->GetPixelSize(imBuf);
  if (mTestCtfPixelSize)
    pixel = mTestCtfPixelSize;
  num = (int)radii.size();
  if (num < 2)
    return;
  imBuf->mImage->getSize(nx, ny);
  xcen = nx / 2.;
  ycen = ny / 2.;
  dx = (shiftX - xcen) / xcen;
  dy = (shiftY - ycen) / ycen;
  pointRad = sqrt(dx * dx + dy * dy);
  lastRad = radii[num - 1];
  nextRad = radii[num - 2];
  for (ind = 1; ind < num; ind++) {
    critBelow = B3DMAX(2.5 / xcen, 0.4 * (radii[ind] - radii[ind - 1]));
    critAbove = B3DMAX(2.5 / xcen, 0.33 * (radii[ind] - radii[ind - 1]));
    if (ind < num - 1)
      critAbove = B3DMAX(2.5 / xcen, 0.4 * (radii[ind + 1] - radii[ind]));
    if ((pointRad >= radii[ind] - critBelow && pointRad <= radii[ind] + critAbove) ||
      (ind == num - 1 && 
      pointRad > B3DCHOICE(ind > 1, 0.5 * (lastRad + nextRad), lastRad) && 
      pointRad < 2 * lastRad - nextRad) || ind == num - 2 && ind &&
      pointRad > nextRad && pointRad < 0.5 * (lastRad + nextRad)) {
      if (!DefocusFromPointAndZeros(pointRad, ind + 1, pixel, 0., &adjRadii, defocus))
        return;
      dx *= adjRadii[0] / pointRad;
      dy *= adjRadii[0] / pointRad;
      shiftX = (float)(xcen + xcen * dx);
      shiftY = (float)(ycen + ycen * dy);
    }
  }
}

// Computes the defocus implied by a given radius (as fraction of Nyquist) for the given
// zero number; returns ring radii if radii is non-NULL.  Defocus is positive in microns
// If zeroNum is zero, it returns rings for a defocus value given in the argument; either
// the number given in mNumFFTZeros or up to maxRingFreq in reciprocal pixels
// Returns false if the radius is in a range where the equations have broken down
// pixel is in nm, radii are in fraction of Nyquist
bool CProcessImage::DefocusFromPointAndZeros(double pointRad, int zeroNum, float pixel,
  float maxRingFreq, FloatVec *radii, double & defocus, float phase)
{
  double wavelength, csOne, csTwo, ampAngle, theta, thetaNew, delz, rootTemp;
  int ind, numZeros = mNumFFTZeros;
  double PI = 3.1415926535;
  GetRecentVoltage();
  if (radii)
    radii->clear();
  if (phase < EXTRA_VALUE_TEST)
    phase = mPlatePhase;

 // This is adopted as closely as possible from defocusfinder in ctfplotter
  wavelength = 1.23984 / sqrt(mVoltage * (mVoltage +  1022.0));
  csOne = sqrt(mSphericalAber * wavelength);
  csTwo = sqrt(sqrt(1000000.0 * mSphericalAber / wavelength));
  ampAngle = 2. * atan(mAmpRatio / sqrt(1. - mAmpRatio * mAmpRatio)) / PI;
  if (zeroNum > 0) {
    theta = (pointRad * wavelength * csTwo) * 0.5 / pixel;

    // Empirically, the defocus given by this equation is wrong when theta > 1
    if (theta > 1.)
      return false;
    defocus = csOne * (pow(theta, 4.) + 2. * zeroNum - ampAngle - 2. * phase / PI) / 
      (2. * theta * theta);
  }
  if (!radii)
    return true;
  delz = defocus / csOne;
  if (zeroNum <= 0 && maxRingFreq > 0)
    numZeros = 50;
  for (ind = 0; ind < numZeros; ind++) {
    rootTemp = delz * delz + ampAngle + 2. * phase / PI - 2. * (ind + 1);
    if (rootTemp < 0. || sqrt(rootTemp) > delz)
      return true;
    thetaNew = sqrt(delz - sqrt(rootTemp));

    // And if the first zero for the computed defocus does not regenerate
    if (zeroNum == 1 && !ind && fabs(thetaNew - theta) > 1.e-5 * theta)
      return false;
    if (zeroNum <= 0 && maxRingFreq > 0 && 
      thetaNew * pixel / (wavelength * csTwo) > maxRingFreq)
      break;
    radii->push_back((float)(thetaNew * pixel * 2.0 / (wavelength * csTwo)));
  }
  return true;
}

// Returns microscope voltage, reading it only sparingly from scope
double CProcessImage::GetRecentVoltage(bool *valueWasRead)
{
  if (valueWasRead)
    *valueWasRead = false;
  if (mKVTime < 0 || SEMTickInterval(mKVTime) > 1000 * KV_CHECK_SECONDS) {
    mKVTime = GetTickCount();
    mVoltage = mScope->GetHTValue();
    if (valueWasRead)
      *valueWasRead = true;
  }
  return mVoltage;
}

double SEMRecentVoltage()
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (winApp->mProcessImage && winApp->mScope)
    return winApp->mProcessImage->GetRecentVoltage();
  return 0.;
}

// Return the pixel size on the camera in diffraction mode for the given camera at the
// current voltage and camera length
void CProcessImage::GetDiffractionPixelSize(int camera, float &pixel, float &camLen)
{
  float wavelength;
  int *camLens = mWinApp->GetCamLenTable();
  float *camLenCal = mWinApp->GetCamLenCalibrated();
  int index = mScope->GetLastCamLenIndex();
  CameraParameters *camParam;

  // Camera length from scope is in meters and from the table is in mm
  camLen = (float)(1000. * mScope->GetLastCameraLength());
  if (camera < 0 || camera >= MAX_CAMERAS)
    return;
  if (index >= 0 || index <= MAX_CAMLENS) {
    if (camLens[index] > 0.)
      camLen = (float)camLens[index];
    if (camLenCal[index] > 0)
      camLen = camLenCal[index];
  }
  camParam = mWinApp->GetCamParams() + camera;
  GetRecentVoltage();
  if (camParam->pixelMicrons <= 0. || mVoltage <= 0. || camLen <= 0.)
    return;

  // The voltage is in KV, the wavelength is in nm, the pixel size is microns, the
  // camera length is in mm
  // The factor is 10e-3 to get pixel size from um to mm, at which point we would have
  // 1/nm, and 0.1 to get from 1/nm to 1/A
  wavelength = (float)(1.241 / sqrt(mVoltage * (mVoltage + 1022.0)));
  pixel = 1.e-4f * camParam->pixelMicrons / (wavelength * camLen);
}

/*
 * CTFFIND SUPPORT FUNCTIONS
 */
// Print and image dump functions for debugging
void ctffindPrintFunc(const char *strMessage)
{
  CString str = strMessage;
  str.TrimRight('\n');
  str.TrimLeft('\n');
  str.Replace("\n", "\r\n");
  PrintfToLog("Ctffind : %s", (LPCTSTR)str);
}

int ctffindDumpFunc(const char *filename, float *data, int xsize, int ysize)
{
  Islice slice;
  sliceInit(&slice, xsize, ysize, MRC_MODE_FLOAT, data);
  return sliceWriteMRCfile((char *)filename, &slice);
}

// Initialize parameters as well as possible given an image, but not a defocus
// The notable defaults here are:
//   box size 256
//   slow search on
//   no phase shift
//   no extra stats
// phase search step 0.1, defocus step 500
//   If there is an image, minimum resolution to 0.05/pixel but limited to 50 A
//                         maximum resoltuion 0.3/pixel, or 0.35/pixel for direct detector
int CProcessImage::InitializeCtffindParams(EMimageBuffer *imBuf, CtffindParams &params)
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  GetRecentVoltage();
  params.acceleration_voltage = (float)mVoltage;
  params.spherical_aberration = mSphericalAber;
  params.amplitude_contrast = mAmpRatio;
  params.box_size = 256;
  params.defocus_search_step = 500.;
  params.astigmatism_tolerance = -100.;
  params.slower_search = true;
  params.astigmatism_is_known = false;
  params.find_additional_phase_shift = false;
  params.minimum_additional_phase_shift = 0.;
  params.maximum_additional_phase_shift = 0.;
  params.additional_phase_shift_search_step = 0.1f;
  params.compute_extra_stats = false;
  if (imBuf) {
    params.pixel_size_of_input_image = mShiftManager->GetPixelSize(imBuf) *
      10000.f;
    if (mTestCtfPixelSize)
      params.pixel_size_of_input_image = mTestCtfPixelSize * 10.f;
    if (!params.pixel_size_of_input_image)
      return 1;
    params.minimum_resolution = params.pixel_size_of_input_image / 0.05f;
    B3DCLAMP(params.minimum_resolution, 13.f, 50.f);
    params.maximum_resolution = params.pixel_size_of_input_image / 0.3f;
    if (imBuf->mCamera >= 0 && mCamera->IsDirectDetector
      (&camParams[imBuf->mCamera]))
      params.maximum_resolution = params.pixel_size_of_input_image / 0.35f;
    ACCUM_MAX(params.maximum_resolution, GetMaxCtfFitRes());
  }
  return 0;
}

// Set or adjust some more parameters with a known target defocus in positive microns
void CProcessImage::SetCtffindParamsForDefocus(CtffindParams &param, double defocus, 
  bool justMinRes)
{
  FloatVec radii;
  if (!justMinRes) {
    param.minimum_defocus = (float)(10000. * B3DMAX(0.3, defocus / mCtfFitFocusRangeFac));
    param.maximum_defocus = (float)(10000. * B3DMIN(defocus * mCtfFitFocusRangeFac, 
      defocus + 5.));
  }
  DefocusFromPointAndZeros(0., 0, param.pixel_size_of_input_image / 10.f, 0., &radii, 
    defocus);
  
  // Make sure minimum resolution is below first zero, and if it gets lower than the
  // default, potentially lower the maximum resolution to limit fitting range
  if (radii.size() > 0)
    ACCUM_MAX(param.minimum_resolution, param.pixel_size_of_input_image / 
      (0.4f * radii[0]));
  if (param.minimum_resolution > 50.)
    ACCUM_MAX(param.maximum_resolution, param.minimum_resolution / 5.f);
  ACCUM_MAX(param.maximum_resolution, GetMaxCtfFitRes());
  if (param.find_additional_phase_shift && mMinCtfFitResIfPhase > 0. && 
    mMinCtfFitResIfPhase < param.minimum_resolution)
    param.minimum_resolution = B3DMAX(mMinCtfFitResIfPhase, 
      1.5f * param.maximum_resolution);
}

// Run
int CProcessImage::RunCtffind(EMimageBuffer *imBuf, CtffindParams &params,
  float results_array[7])
{
  float *spectrum;
  //float *rotationalAvg, *normalizedAvg, *fitCurve;
  float lastBinFreq, resampleRes;
  float pixelSave = params.pixel_size_of_input_image;
  float minFracOfNyquistForMaxRes = 0.6f;
  CString mess, str;
  double wallStart = wallTime();
  int numPoints, padSize, err, useBox, start, end, val;
  KImage *image = imBuf->mImage;
  if (!image)
    return 1;

  // Determine whether to resample the power spectrum from a larger box
  resampleRes = minFracOfNyquistForMaxRes * params.maximum_resolution;
  useBox = params.box_size;
  if (resampleRes > params.pixel_size_of_input_image * 2.)
    useBox = 2 * (B3DNINT(1. + 0.5 * params.box_size * resampleRes /
      params.pixel_size_of_input_image) / 2);


  // Get the scaled spectrum; just flip image first and restore at end to make it match
  // IMOD and ctffind expectations
  NewArray2(spectrum, float, useBox, (useBox + 2));
  if (!spectrum)
    return 1;
  image->Lock();
  padSize = B3DMAX(image->getWidth(), image->getHeight());
  padSize = XCorrNiceFrame(padSize, 2, 19);
  image->flipY();
  err = spectrumScaled(image->getData(), image->getType(), image->getWidth(), 
    image->getHeight(), spectrum, -padSize, useBox, 0, 0., -1, twoDfft);
  if (err) 
    PrintfToLog("Error %d calling spectrumScaled", err);
  image->flipY();
  image->UnLock();
  if (!err && useBox > params.box_size) {
    start = (useBox - params.box_size) / 2;
    end = start + params.box_size - 1;
    err = extractAndBinIntoArray(spectrum, MRC_MODE_FLOAT, useBox + 2, start, end, start,
       end, 1, spectrum, params.box_size + 2, 0, 0, 0, &val, &val);
    if (err)
      PrintfToLog("Error %d extracting reduced spectrum from larger box to smaller", err);
    params.pixel_size_of_input_image *= (float)useBox / (float)params.box_size;
  }

  // Run the fit and report results
  if (!err && ctffind(params, spectrum, params.box_size + 2, results_array, NULL,
                 NULL, NULL, numPoints, lastBinFreq)) {
    mess.Format("Ctffind: defocus: %.3f,  astig: %.3f um,  angle: %.1f,  ",  
      -(results_array[0] + results_array[1]) / 20000., 
      (results_array[0] - results_array[1]) / 10000., results_array[2]);
    if (params.find_additional_phase_shift) {
      str.Format("%s %.1f deg,  ", params.minimum_additional_phase_shift <
        params.maximum_additional_phase_shift ? "phase shift" : "fixed phase",
        results_array[3] / DTOR);
      mess += str;
    }
    str.Format("score %.4f", results_array[4]);
    mess += str;
    if (params.compute_extra_stats) {
      str.Format(",   fit to %.1f A", results_array[5]);
      mess += str;
      /*if (results_array[6])
        PrintfToLog("Antialiasing detected at %.1f", results_array[6]);*/
    }
    mWinApp->AppendToLog(mess);
    //PrintfToLog("Elapsed time %.3f sec", wallTime() - wallStart);
  } else {
    err = 1;
  }
  delete [] spectrum;
  params.pixel_size_of_input_image = pixelSave;
  return err;
}

// DOSE RATE AND LINEARIZATION ROUTINES

// Return a dose rate in electrons per unbinned pixel per second given the mean value
// in the given buffer; returns 2 if it cannot be computed; returns a linearized dose
// rate for a K2 camera
int CProcessImage::DoseRateFromMean(EMimageBuffer *imBuf, float mean, float &doseRate)
{
  float countsPerElectron;
  if (!imBuf || !imBuf->mImage)
    return 1;
  CameraParameters *camParam = mWinApp->GetCamParams() + imBuf->mCamera;
  countsPerElectron = CountsPerElectronForImBuf(imBuf);
  if (!countsPerElectron)
    return 2;
  doseRate = (float)(mean / (countsPerElectron * imBuf->mExposure * 
    imBuf->mBinning * imBuf->mBinning / ((CamHasDoubledBinnings(camParam) || 
    (camParam->DE_camType && (camParam->CamFlags & DE_APOLLO_CAMERA))) ? 4. : 1.)));
  if ((camParam->K2Type || IS_FALCON3_OR_4(camParam)) && imBuf->mK2ReadMode > 0)
    doseRate = LinearizedDoseRate(imBuf->mCamera, doseRate);
  if (imBuf->mDoseRatePerUBPix > 0.)
    doseRate = imBuf->mDoseRatePerUBPix;
  return 0;
}

// Returns the actual counts per electron for the image in a buffer, accounting for the
// gain factor and other factors as needed
float CProcessImage::CountsPerElectronForImBuf(EMimageBuffer * imBuf)
{
  float countsPerElectron, gainFac = 1.;
  if (!imBuf || !imBuf->mImage)
    return 0.;
  EMimageExtra *extra = (EMimageExtra *)imBuf->mImage->GetUserData();
  if (!extra || imBuf->mCamera < 0 || imBuf->mBinning < 1 || extra->mExposure <= 0)
    return 0.;
  CameraParameters *camParam = mWinApp->GetCamParams() + imBuf->mCamera;
  countsPerElectron = camParam->countsPerElectron;
  if (camParam->K2Type != K3_TYPE && imBuf->mK2ReadMode > 0)
    countsPerElectron = mCamera->GetCountScaling(camParam);
  if (countsPerElectron <= 0.)
    return 0.;

  // Use the total divisions by 2 if available and large, but then skip the gain factors
  // if they were imposed by autogain, since they are in the total divisions. 
  // Otherwise, stick with the product of divided counts and gain factor
  if (extra->mDividedBy2 > 1)
    countsPerElectron /= powf(2.f, (float)extra->mDividedBy2);
  else if (imBuf->mDividedBy2)
    countsPerElectron /= 2.f;
  if (extra->mDividedBy2 <= 1 && !camParam->autoGainAtBinning)
    gainFac = mWinApp->GetGainFactor(imBuf->mCamera, imBuf->mBinning);
  return gainFac * countsPerElectron;
}

// Computes a linearized dose rate for a K2 or Falcon camera.  Will test camera type,
// but caller needs to test if image was taken in counting mode
float CProcessImage::LinearizedDoseRate(int camera, float rawRate)
{
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;
  const float K2counts200KV[] = {1.010f, 2.093f, 3.112f, 4.520f, 6.355f, 8.095f, 9.902f, 
    13.288f, 17.325f, 19.424f, 22.683f, 26.42f, 28.63f, 29.53f, 30.16f};
  const float K2rates200KV[] = {0.909f, 1.950f, 2.984f, 4.491f, 6.583f, 8.705f, 11.071f, 
    16.107f, 23.707f, 28.698f, 38.486f, 53.90f, 65.78f, 71.37f, 75.54f};
  const float K2counts300KV[] = {0.731f, 1.703f, 2.724f, 3.517f, 5.279f, 7.336f, 9.950f,
    13.014f, 16.514f, 21.21f, 25.32f, 28.20f, 29.25f};
  const float K2rates300KV[] = {0.789f, 1.890f, 3.107f, 4.092f, 6.388f, 9.252f, 13.213f,
    18.480f, 25.744f, 39.14f, 56.29f, 72.61f, 79.76f};
   const float K3counts200KV[] = {2.998f, 3.589f, 4.887f, 6.687f, 9.386f, 11.849f, 14.381f,
    17.529f, 20.673f, 23.612f, 26.410f, 29.450f, 31.929f, 34.500f, 36.979f, 39.313f,
    41.685f, 43.987f, 46.835f, 49.421f, 51.901f, 54.197f, 56.508f, 59.514f, 62.553f,
    64.054f, 68.587f, 70.656f, 72.707f, 74.697f, 76.611f, 78.490f};
  const float K3rates200KV[] = {2.956f, 3.617f, 5.078f, 7.132f, 10.281f, 13.230f, 16.341f,
    20.329f, 24.452f, 28.442f, 32.373f, 36.797f, 40.533f, 44.539f, 48.536f, 52.427f,
    56.519f, 60.633f, 65.931f, 70.959f, 75.993f, 80.853f, 85.959f, 92.952f, 100.477f,
    104.378f, 117.021f, 123.277f, 129.833f, 136.559f, 143.420f, 150.570f};

  const float Falcon3Counts200KV[] = {0.103f, 0.119f, 0.159f, 0.199f, 0.259f, 0.352f,
    0.418f, 0.502f, 0.575f, 0.663f, 0.734f, 0.818f, 0.875f, 0.94f, 1.007f, 1.075f,
    1.123f, 1.157f, 1.189f, 1.219f, 1.248f};
  const float Falcon3Rates200KV[] = {0.101f, 0.119f, 0.161f, 0.201f, 0.271f, 0.377f,
    0.461f, 0.577f, 0.693f, 0.838f, 0.981f, 1.159f, 1.302f, 1.486f, 1.711f, 1.968f,
    2.226f, 2.422f, 2.651f, 2.894f, 3.191f};
  const float Falcon4Counts200KV[] = {0.493f, 0.964f, 1.421f, 1.863f, 2.291f, 2.706f,
    3.108f, 3.499f, 3.878f, 4.246f, 4.599f, 4.943f, 5.277f, 5.599f, 5.911f, 6.214f,
    6.792f, 7.331f, 7.836f, 8.313f, 8.757f, 9.175f, 9.611f, 9.95f, 10.274f, 10.594f,
    10.894f, 11.309f, 11.8f, 12.229f, 12.691f, 12.937f, 13.143f};
  const float Falcon4Rates200KV[] = {0.495f, 0.993f, 1.494f, 1.993f, 2.496f, 2.998f,
    3.500f, 4.011f, 4.517f, 5.032f, 5.539f, 6.053f, 6.571f, 7.090f, 7.607f, 8.128f,
    9.176f, 10.236f, 11.308f, 12.393f, 13.490f, 14.605f, 15.885f, 16.953f, 18.069f,
    19.265f, 20.489f, 22.391f, 25.064f, 27.945f, 31.989f, 34.777f, 37.706f};
  const float Falcon4Counts300KV[] = {0.309f, 0.521f, 0.737f,
    1.062f, 1.387f, 1.928f, 2.357f, 2.784f, 3.31f, 3.93f, 4.623f, 5.357f,
    5.868f, 6.229f, 6.597f, 7.099f, 7.872f, 8.635f, 9.335f, 10.093f, 10.904f,
    11.551f, 12.214f, 12.731f, 13.155f, 13.677f, 14.095f, 14.357f, 14.562f, 14.729f};
  const float Falcon4Rates300KV[] = {0.311f, 0.53f, 0.756f,
    1.107f, 1.463f, 2.077f, 2.576f, 3.096f, 3.752f, 4.569f, 5.538f, 6.631f,
    7.434f, 8.03f, 8.66f, 9.565f, 11.071f, 12.714f, 14.403f, 16.463f, 19.035f,
    21.459f, 24.426f, 27.234f, 30.f, 34.4f, 39.275f, 43.48f, 48.019f, 53.144f};

  const float *countsArr, *ratesArr;
  int numVals;
  float doseRate, ratio;

  if (camera < 0 || !(camParam->K2Type || IS_FALCON3_OR_4(camParam)))
    return rawRate;

  // First time for a camera, in no table read in from properties, assign the default
  // based on voltage
  if (!camParam->doseTabCounts.size()) {
    if (camParam->K2Type == K3_TYPE) {
        countsArr = &K3counts200KV[0];
        ratesArr = &K3rates200KV[0];
        numVals = sizeof(K3counts200KV) / sizeof(float);
    } else if (camParam->K2Type) {
      if (mScope->GetHTValue() > 250) {
        countsArr = &K2counts300KV[0];
        ratesArr = &K2rates300KV[0];
        numVals = sizeof(K2counts300KV) / sizeof(float);
      } else {
        countsArr = &K2counts200KV[0];
        ratesArr = &K2rates200KV[0];
        numVals = sizeof(K2counts200KV) / sizeof(float);
      }
    } else if (camParam->FEItype == FALCON3_TYPE) {
      countsArr = &Falcon3Counts200KV[0];
      ratesArr = &Falcon3Rates200KV[0];
      numVals = sizeof(Falcon3Counts200KV) / sizeof(float);
    } else {
      if (mScope->GetHTValue() > 250) {
        countsArr = &Falcon4Counts300KV[0];
        ratesArr = &Falcon4Rates300KV[0];
        numVals = sizeof(Falcon4Counts300KV) / sizeof(float);
      } else {
        countsArr = &Falcon4Counts200KV[0];
        ratesArr = &Falcon4Rates200KV[0];
        numVals = sizeof(Falcon4Counts200KV) / sizeof(float);
      }
    }
    camParam->doseTabCounts.insert(camParam->doseTabCounts.end(), countsArr, 
      countsArr + numVals);
    camParam->doseTabRates.insert(camParam->doseTabRates.end(), ratesArr, 
      ratesArr + numVals);
  }

  // If fit has not been done yet, fit to the table
  if (!camParam->doseTabScale) {
    FitRatesAndCounts(&camParam->doseTabCounts[0], &camParam->doseTabRates[0], 
      (int)camParam->doseTabRates.size(), camParam->doseTabScale, 
      camParam->doseTabConst, camParam->doseTabBase);
  }

  // Call dose rate routine for lookup
  FindDoseRate(rawRate, &camParam->doseTabCounts[0], &camParam->doseTabRates[0], 
    (int)camParam->doseTabRates.size(), camParam->doseTabScale, 
      camParam->doseTabConst, camParam->doseTabBase, ratio, doseRate);
  return doseRate;
}

// Do an initial fit to the counts and dose rates in the table and store the values
int CProcessImage::FitRatesAndCounts(float *counts, float *rates, int numVals, 
  float &scale, float &intercp, float &base)
{
  float brackets[14];
  int ind, err, numScanSteps, numCutsDone;
  double errSum, errMin;
  float curBase, slope, intcp, ro, initialStep;
  float minRatio = 1.e10;
  float *yy = new float[numVals];
  for (ind = 0; ind < numVals; ind++)
    ACCUM_MIN(minRatio, counts[ind] / rates[ind]);

  // Search for a baseline value that gives the best exponential decay of the
  // attenuation ratio to a baseline
  curBase = 0.;
  numScanSteps = 20;
  initialStep = (float)(0.98 * minRatio / numScanSteps);
  numCutsDone = -1;
  errMin = 1.e30;
  for (;;) {
    for (ind = 0; ind < numVals; ind++)
      yy[ind] = (float)log((double)counts[ind] / rates[ind] - curBase);
    lsFit(rates, yy, numVals, &slope, &intcp, &ro);
    errSum = 0.;
    for (ind = 0; ind < numVals; ind++)
      errSum += pow((double)yy[ind] - slope * rates[ind] - intcp, 2.);
    if (errSum < errMin) {
      errMin = errSum;
      base = curBase;
      scale = slope;
      intercp = intcp;
    }
    err = minimize1D(curBase, (float)errSum, initialStep, numScanSteps, &numCutsDone,
                     brackets, &curBase);
    if (err)
      break;
    if (numCutsDone > 0 && initialStep / pow((double)numCutsDone, 2.) < 0.0001)
      break;
  }
  delete [] yy;
  return err;
}

// Find the linearized rate for the given count value, given the table and its fitting 
// parameters.  Returns the dose rate and the attenuation ratio; return value is an error 
// code from minimize1D
int CProcessImage::FindDoseRate(float countVal, float *counts, float *rates, int numVals,
                  float scale, float intercp, float base,
                  float &ratioBest, float &doseRate)
{
  int indBefore, indAfter;
  float initialStep, diffMin, resBefore, resAfter, frac, ratio1, diff, res;
  float count1, curRate, scanEnd;
  float brackets[14];
  int ind, err = 0, numScanSteps, numCutsDone;

  // Find place in table to interpolate at
  indBefore = -1;
  for (ind = 0; ind < numVals; ind++) {
    if (counts[ind] <= countVal)
      indBefore = ind;
    else
      break;
  }
  indAfter = indBefore + 1;
  indBefore = B3DMAX(0, indBefore);
  indAfter = B3DMIN(numVals - 1, indAfter);
  if (indAfter == indBefore) {
    ratioBest = counts[indBefore] / rates[indBefore];
  } else {

    // Extend the scan 0.1 intervals below and above if possible
    curRate = rates[indBefore];
    numScanSteps = 20;
    if (indBefore > 0) {
      curRate -= 0.1f * (rates[indBefore] - rates[indBefore - 1]);
      numScanSteps += 2;
    }
    scanEnd = rates[indAfter];
    if (indAfter < numVals - 1) {
      scanEnd += 0.1f * (rates[indAfter + 1] - rates[indAfter]);
      numScanSteps += 2;
    }
    initialStep = (scanEnd - curRate) / numScanSteps;
    numCutsDone = -1;
    diffMin = 1.e30f;

    // Get the residual errors at the two ends of the interpolation
    resBefore = counts[indBefore] / rates[indBefore] -
      (exp(scale * rates[indBefore] + intercp) + base);
    resAfter = counts[indAfter] / rates[indAfter] -
      (exp(scale * rates[indAfter] + intercp) + base);

    // For the current dose rate, estimate the attenuation ratio from the fit and an 
    // interpolated residual value, then the error to minimize is the difference between 
    // the implied and actual count value
    for (;;) {
      frac = (curRate - rates[indBefore]) / (rates[indAfter] - rates[indBefore]);
      res = resBefore + frac * (resAfter - resBefore);
      ratio1 = exp(scale * curRate + intercp) + base + res;
      count1 = curRate * ratio1;
      diff = (float)fabs((double)count1 - countVal);
      if (diff < diffMin) {
        diffMin = diff;
        ratioBest = ratio1;
      }
      err = minimize1D(curRate, diff, initialStep, numScanSteps, &numCutsDone,
                     brackets, &curRate);
    if (err)
      break;
    if (numCutsDone > 0 && initialStep / pow((double)numCutsDone, 2.) < 0.001)
      break;
    }
  }
  doseRate = countVal / ratioBest;
  return err; 
}

// Return maximum resolution for fitting, in Angstroms
float CProcessImage::GetMaxCtfFitRes(void)
{
  return (mUserMaxCtfFitRes > 0 ? mUserMaxCtfFitRes : mDefaultMaxCtfFitRes);
}
