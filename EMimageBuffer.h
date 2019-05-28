#ifndef EMIMAGEBUFFER_H
#define EMIMAGEBUFFER_H

#include "Image\KImage.h"
#include "Image\KImageStore.h"
#include "Image\KImageScale.h"
#include "Image\KPixMap.h"
#include "ControlSet.h"
#include "MontageParam.h"

class EMimageBuffer
{
public:
	BOOL GetStagePosition(float &X, float &Y);
	bool GetStageZ(float &Z);
  BOOL GetTiltAngle(float &angle);
  EMimageBuffer();
  ~EMimageBuffer();

  virtual void SetImageChanged(int inVal) {mImageChanged = inVal;}
  virtual int GetImageChanged() {return mImageChanged;}
  virtual int GetSaveCopyFlag();
  virtual void IncrementSaveCopy();
  virtual void DecrementSaveCopy();
  virtual void NegateSaveCopy();
  virtual void DeleteImage();
  float GetUnbinnedBWMeanPerSec();
  BOOL IsMontageOverview();
  BOOL IsMontageCenter();
  BOOL ConvertToByte(float minScale, float maxScale);

  KImage          *mImage;
  KImageScale *mImageScale;
  KPixMap     *mPixMap;
  KImageScale  mLastScale;
  MiniOffsets *mMiniOffsets;
  KPixMap     *mFiltPixMap;
  unsigned int mCurStoreChecksum;
  int *mSaveCopyp;
  int mSecNumber;
  int mChangeWhenSaved;    // How the data were modified when saved
  int mConSetUsed;
  int mCaptured;
  int mBinning;
  int mDivideBinToShow;   // Value to divide binning by to display it
  BOOL mDividedBy2;
  int mCamera;
  int mMagInd;
  float mDefocus;
  double mISX;
  double mISY;
  float mExposure;
  int mK2ReadMode;
  float mBacklashX, mBacklashY;
  double mTimeStamp;
  float mSampleMean;
  float mSampleSD;
  float mDoseRatePerUBPix;  // Dose rate per unbinned pixel
  float mEffectiveBin;  // Set for a montage overview to make it display nicely
  int mOverviewBin;     // Binning of a montage overview
  BOOL mDynamicFocused;
  BOOL mStageShiftedByMouse;  // Flag that the shift resulted in a stage shift
  BOOL mHasUserPt;
  float mUserPtX;
  float mUserPtY;
  BOOL mHasUserLine;
  BOOL mDrawUserBox;
  float mLineEndX;
  float mLineEndY;
  float mCtfFocus1, mCtfFocus2;
  float mCtfAngle;
  float mCtfPhase;     // In radians
  float mMaxRingFreq;
  double mZoom;
  int mPanX, mPanY;
  int mFiltXpan, mFiltYpan;   // Pan and zoom at which filtered pixmap was made
  double mFiltZoom;
  BOOL mLowDoseArea;
  float mViewDefocus;
  int mProbeMode;
  int mRegistration;
  int mMapID;           // Map ID.  Items below are ignored if this is 0
  float mRotAngle;      // Rotation angle for a loaded map
  BOOL mInverted;       // and a flag to keep track of inversion
  float mUseWidth;      // Width and height to actually use when scaling
  float mUseHeight;
  int mLoadWidth;       // Size of current load from file before rotation
  int mLoadHeight;
  float mTiltAngleOrig; // Tilt angle from original image
  ScaleMat mStage2ImMat;
  float mStage2ImDelX;
  float mStage2ImDelY;
  
protected:
  int mImageChanged;
public:
  void ClearForDeletion(void);
  void DeleteOffsets(void);
  BOOL IsProcessed(void);
  void UpdatePixMap(void);
  bool IsImageDuplicate(EMimageBuffer * otherBuf);
  bool IsImageDuplicate(KImage *image);
  bool GetAxisAngle(float &angle);
  bool GetDefocus(float & focus);
  bool GetSpotSize(int &spot);
  bool GetIntensity(double & intensity);
  CString BinningText(void);
};

#endif
