// EMimageBuffer.cpp     Image buffer class to hold images and associated scope
//                          information
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "SerialEM.h"

// When members are added, these are the places to fix:
// CCameraController::DisplayNewImage
// EMbufferManager::ReadFromFile
// EMbufferManager::ReplaceImage

EMimageBuffer::EMimageBuffer()
{
	mImage = NULL;
	mImageScale = NULL;
	mPixMap = NULL;
  mFiltPixMap = NULL;
  mMiniOffsets = NULL;
  mCurStoreChecksum = 0;
	mImageChanged = 0;
	mSaveCopyp = NULL;
  mChangeWhenSaved = 0;
	mBinning = 0;
  mDivideBinToShow = 1;
  mDividedBy2 = false;
	mMagInd = 0;
	mDefocus = 0.;
  mISX = 0.;
  mISY = 0.;
	mExposure = 0.;
  mBacklashX = 0.;
  mBacklashY = 0.;
	mTimeStamp = 0.;
  mDoseRatePerUBPix = 0.;
  mSampleMean = EXTRA_NO_VALUE;
  mSampleSD = EXTRA_NO_VALUE;
	mUserPtX = 0.;
	mUserPtY = 0.;
	mCamera = -1;
  mPixelSize = 0.;
  mConSetUsed = 0;
  mDynamicFocused = false;
  mStageShiftedByMouse = false;
  mCaptured = BUFFER_UNUSED;
	mHasUserPt = false;
  mHasUserLine = false;
  mDrawUserBox = false;
  mCtfFocus1 = 0.;
  mRegistration = 0;
  mMapID = 0;
  mRotAngle = 0.;
  mInverted = false;
  mZoom = 0.;
  mPanX = 0;
  mPanY = 0;
  mFiltZoom = -1.;
  mLowDoseArea = false;
  mViewDefocus = 0.;
  mProbeMode = -1;
  mTiltAngleOrig = 0.;
  mStage2ImMat.xpx = 0.;
  mStage2ImDelX = 0.;
  mStage2ImDelY = 0.;
}

// For complete destruction of an image buffer
EMimageBuffer::~EMimageBuffer()
{
  DeleteImage();
  DeleteOffsets();
	if (mImageScale)
		delete mImageScale;
	if (mPixMap)
		delete mPixMap;
	if (mFiltPixMap)
		delete mFiltPixMap;
}

// Clear out all pointers in a buffer so it can deleted after simple copy
void EMimageBuffer::ClearForDeletion(void)
{
	mImage = NULL;
	mImageScale = NULL;
	mPixMap = NULL;
	mSaveCopyp = NULL;
  mMiniOffsets = NULL;
  mFiltPixMap = NULL;
}

void EMimageBuffer::DeleteOffsets(void)
{
  if (mMiniOffsets)
    delete mMiniOffsets;
  mMiniOffsets = NULL;
}
	
// The save-copy flag is done in this weird way so that its value is shared by all the 
// buffers that hold a copy of the same image.  Increment is called:
//   for a new single-frame image after clearing out previous flag
//   when an image buffer is copied
//   When stating an asynchronous save, from a null
void EMimageBuffer::IncrementSaveCopy()
{
	// If Flag doesn't exist yet,  create it and set to 1
	if (mSaveCopyp == NULL) {
		mSaveCopyp = new int;
		*mSaveCopyp = 1;
	// Increment if positive, decrement if negative
	} else {
		 if (*mSaveCopyp >= 0)
			(*mSaveCopyp)++;
		else
			(*mSaveCopyp)--;
	}
}

// Reduces the flag toward zero if it exists, and removes it when it reaches zero
// Called on DeleteImage ad when sarting an asynchronous save
void EMimageBuffer::DecrementSaveCopy()
{
	if (mSaveCopyp == NULL)
		return;
	 if (*mSaveCopyp >= 0)
		(*mSaveCopyp)--;
	else
		(*mSaveCopyp)++;
	
	// If flag has reached zero, get rid of it
	if (*mSaveCopyp == 0) {
		delete mSaveCopyp;
		mSaveCopyp = NULL;
	}
}

// Makes flag negative if it is positive, or -1 if it does not exist.  Called by
//   ConverToByte to transfer the saved property to a converted buffer
//   SaveImageBuffer and OverwriteImage when they save an image
void EMimageBuffer::NegateSaveCopy()
{
	// If Flag doesn't exist yet,  create it and set to 1
	if (mSaveCopyp == NULL) {
		mSaveCopyp = new int;
		*mSaveCopyp = 1;
	}
	if (*mSaveCopyp > 0)
		*mSaveCopyp = - *mSaveCopyp;
}

// Returns 0 if the falg doesn't exist, or its value
int EMimageBuffer::GetSaveCopyFlag()
{
	if (mSaveCopyp == NULL)
		return 0;
	else
		return *mSaveCopyp;
}

// Returns true if an image was read in from file
bool EMimageBuffer::ImageWasReadIn() {
  return (!mCaptured && !GetSaveCopyFlag()); 
}

// Delete the image in the buffer if it exists
void EMimageBuffer::DeleteImage()
{
  if (mImage != NULL) {
    DecrementSaveCopy();   // decrement the flag and
    mSaveCopyp = NULL;     // null out our reference to it
    delete mImage;         // delete the image
    mImage = NULL;
  }
}

// Return the mean of the black and white levels in the ImageScale,
// adjusted to be per unbinned pixel per second
float EMimageBuffer::GetUnbinnedBWMeanPerSec()
{
	float bwMean;
	if (!mImageScale)
		return 0;
	bwMean = 0.5f * (mImageScale->GetMinScale() + mImageScale->GetMaxScale());
	if (mExposure && mBinning)
		bwMean /= mBinning * mBinning * mExposure;
	return bwMean;
}

// Return various values from the extra data in the image, return fakse if not available
BOOL EMimageBuffer::GetTiltAngle(float &angle)
{
	angle = 0.;
	if (!mImage)
		return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->m_fTilt == EXTRA_NO_VALUE)
		return false;
	angle = extra->m_fTilt;
	return true;
}

bool EMimageBuffer::GetAxisAngle(float &angle)
{
	angle = 0.;
	if (!mImage)
		return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->mAxisAngle == EXTRA_NO_VALUE)
		return false;
	angle = extra->mAxisAngle;
	return true;
}

BOOL EMimageBuffer::GetStagePosition(float &X, float &Y)
{
  X = 0.;
  Y = 0.;
	if (!mImage)
		return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->mStageX == EXTRA_NO_VALUE || extra->mStageY == EXTRA_NO_VALUE)
		return false;
  X = extra->mStageX;
  Y = extra->mStageY;
  return true;
} 

bool EMimageBuffer::GetStageZ(float &Z)
{
  Z = 0.;
	if (!mImage)
    return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->mStageZ == EXTRA_NO_VALUE)
		return false;
  Z = extra->mStageZ;
  return true;
}

bool EMimageBuffer::GetDefocus(float &focus)
{
  focus = 0.;
	if (!mImage)
    return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->mDefocus == EXTRA_NO_VALUE)
		return false;
  focus = extra->mDefocus;
  return true;
}

bool EMimageBuffer::GetSpotSize(int &spot)
{
  spot = 0;
	if (!mImage)
    return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->mSpotSize == EXTRA_NO_VALUE)
    return false;
  spot = extra->mSpotSize;
  return true;
}

bool EMimageBuffer::GetIntensity(double & intensity)
{
  intensity = 0.;
  if (!mImage)
    return false;
	EMimageExtra *extra = mImage->GetUserData();
	if (!extra || extra->m_fIntensity == EXTRA_NO_VALUE)
    return false;
  intensity = extra->m_fIntensity;
  return true;
}

BOOL EMimageBuffer::IsMontageOverview()
{
  return mCaptured == BUFFER_MONTAGE_OVERVIEW ||
    mCaptured == BUFFER_PRESCAN_OVERVIEW;
}

BOOL EMimageBuffer::IsMontageCenter()
{
  return mCaptured == BUFFER_MONTAGE_CENTER ||
    mCaptured == BUFFER_PRESCAN_CENTER;
}

BOOL EMimageBuffer::IsProcessed(void)
{
  return mCaptured == BUFFER_PROCESSED || mCaptured == BUFFER_FFT || 
    mCaptured == BUFFER_LIVE_FFT || mCaptured == BUFFER_AUTOCOR_OVERVIEW;
}

// Convert the image to a byte image with given scaling
BOOL EMimageBuffer::ConvertToByte(float minScale, float maxScale)
{
  int type, saveFlag;
  float shiftX, shiftY;
  KImage *byteImage;

  // Require an image, image scale, pixmap, and short image
  if (!mImage || !mImageScale || !mPixMap)
    return false;
  type = mImage->getType();
  if (type != kUSHORT && type != kSHORT && type != kFLOAT)
    return false;

  // Save shifts.  Get scale from last display if none passed in.
  mImage->getShifts(shiftX, shiftY);
  if (minScale == 0. && maxScale == 0.) {
    minScale = mLastScale.GetMinScale();
    maxScale = mLastScale.GetMaxScale();
  }

  // If shifts are non-zero or scale does not match or no pixmap image, get a new pixmap
  if (shiftX != 0. || shiftY != 0. || minScale != mLastScale.GetMinScale() 
    || maxScale != mLastScale.GetMaxScale() || !mPixMap->getImRectPtr()) {
    mImage->setShifts(0., 0.);
    mImageScale->SetMinMax(minScale, maxScale);
    mPixMap->setLevels(*mImageScale);
    SetImageChanged(1);
    mPixMap->useRect(mImage);
  }

  // Make a copy of the pixmap image structure and transfer extra data
  // Find out if image saved; delete existing image, restore save flag
  byteImage = new KImage(mPixMap->getImRectPtr());
  byteImage->SetUserData(mImage->GetUserData());
  mImage->SetUserData(NULL);
  saveFlag = GetSaveCopyFlag();
  DeleteImage();
  if (saveFlag < 0)
    NegateSaveCopy();

  // Replace the existing image, free the pixmap, set the scale and restore the shifts
  mPixMap->doneWithRect();
  mImage = byteImage;
  mImageScale->SetMinMax(0., 255.);
  mImageScale->SetSampleMinMax(0., 255.);
  mImage->setShifts(shiftX, shiftY);
  mSampleMean = EXTRA_NO_VALUE;
  SetImageChanged(1);
  return true;
}

// Update the PixMap member based on new image or new scaling
void EMimageBuffer::UpdatePixMap(void)
{

 // Fetch the pixmap structure; make new one if needed
  if (mPixMap == NULL)
    mPixMap = new KPixMap();

  // Set the color table for the display
  mPixMap->setLevels(*mImageScale);

  // Check the image scaling and if it has changed or if image has
  // changed, refill the pixmap, invalidate filtered pixmap
  if (GetImageChanged() != 0 || mImageScale->GetMinScale() != mLastScale.GetMinScale() ||
    mImageScale->GetMaxScale() != mLastScale.GetMaxScale() || !mPixMap->getImRectPtr()) {
    mPixMap->useRect(mImage); 
    mFiltZoom = -1;
  }

  mLastScale = *mImageScale;
  SetImageChanged(0);
}

bool EMimageBuffer::IsImageDuplicate(EMimageBuffer *otherBuf)
{
  return IsImageDuplicate(otherBuf->mImage); 
}

bool EMimageBuffer::IsImageDuplicate(KImage *image)
{
  int nx, ny, type, i;
  bool retval = true;
  short int *data, *other;
  if (!image || !mImage)
    return false;
  mImage->getSize(nx, ny);
  type = mImage->getType();
  if (image->getWidth() != nx || image->getHeight() != ny || image->getType() != type)
    return false;
  image->Lock();
  other = (short int *)image->getData();
  mImage->Lock();
  data = (short int *)mImage->getData();
  for (i = 0; i < nx * ny; i++) {
    if (other[i] != data[i]) {
      retval = false;
      break;
    }
  }
  image->UnLock();
  mImage->UnLock();
  return retval;
}

// Return a string to use for binning of the image buffer
CString EMimageBuffer::BinningText(void)
{ 
  return ((CSerialEMApp *)AfxGetApp())->BinningText(mBinning, mDivideBinToShow);
}
