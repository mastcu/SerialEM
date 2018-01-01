#include "stdafx.h"
#include <math.h>
#include ".\EMimageExtra.h"

// This must be synced to entries in b3dHeaderItemBytes in IMOD/libimod/b3dutil.c
// Supposedly the next ones are "reserved" to be 2, 4, 2, 4, 2.
const int EMimageExtra::Bytes[EXTRA_NTYPES] = {2, 6, 4, 2, 2, 4};

EMimageExtra::EMimageExtra()
{
	mTilt = 0;
	mMontageX = mMontageY = mMontageZ = 0;
	mVoltageX = mVoltageY = 0;
	mMag100 = 0;
  mIntensity = 0;
  mDoseLo = mDoseHi = 0;
  m_fTilt = EXTRA_NO_VALUE;
  m_iMontageX = m_iMontageY = m_iMontageZ = -1;
  mStageX = mStageY = mStageZ = EXTRA_NO_VALUE;
  m_iMag = 0;
  m_fIntensity = -1.;
  m_fDose = -1.;
  mSuperMontX = mSuperMontY = -1;
  mPixel = 0.;
  mSpotSize = 0;
  mDefocus = EXTRA_NO_VALUE;
  mISX = mISY = EXTRA_NO_VALUE;
  mAxisAngle = EXTRA_NO_VALUE;
  mExposure = -1.;
  mBinning = 0.;
  mCamera = -1;
  mDividedBy2 = -1;
  mMagIndex = -1;
  mCountsPerElectron = -1.;
  mMin = mMax = mMean = EXTRA_NO_VALUE;
  mTargetDefocus = EXTRA_NO_VALUE;
  mNavLabel = "";
  mPriorRecordDose = -1;
  mSubFramePath = "";
  mNumSubFrames = -1;
  mFrameDosesCounts = "";
  mChannelName = "";
  mDE12Version = "";
  mPreExposeTime = -1.;
  mNumDE12Frames = -1;
  mDE12FPS = -1.;
  mDE12Position = "";
  mDE12CoverMode = "";
  mCoverDelay = -1;
  mTemperature = EXTRA_NO_VALUE;
  mFaraday = EXTRA_NO_VALUE;
  mSensorSerial = "";
  mReadoutDelay = -1;
  mIgnoredFrames = -1;
}

// Convert a float to two shorts: the number is converted to an integer between
// 327600 and 3276000 and an exponent as a power of 10;
// the low word is this number divided by 100 times the sign of the value
// the high word is this number modulo 100 plus 100 * |exponent|, with the sign of 
// the exponent
int EMimageExtra::FloatToShorts(float inVal, short int &outLo, short int &outHi)
{
  int expon, scaled, signVal = 1, signExp = 1;

  outLo = outHi = 0;
  if (!inVal)
    return 0;
  if (inVal < 0) {
    inVal = -inVal;
    signVal = -1;
  }
  expon = (int)floor(log((double)inVal) / log(2.) - 22.);
  scaled = (int)floor(inVal / pow(2., (double)expon));
  if (expon < 0) {
    expon = -expon;
    signExp = -1;
  }
  if (expon > 127)
    return 1;
  outLo = signVal * (scaled / 256);
  outHi = signExp * (expon * 256 + scaled % 256);
  return 0;
}

void EMimageExtra::ValuesIntoShorts(void)
{
  if (m_fTilt > EXTRA_VALUE_TEST)
    mTilt = (short int)floor(100. * m_fTilt + 0.5);
  if (m_iMontageX >= 0 && m_iMontageX < 65536 && m_iMontageY < 65536) {
    mMontageX = (unsigned short int)m_iMontageX;
    mMontageY = (unsigned short int)m_iMontageY;
    mMontageZ = (short int)m_iMontageZ;
  }
  if (mStageX > EXTRA_VALUE_TEST) {
    mVoltageX = (short int)floor(25. * mStageX + 0.5);
    mVoltageY = (short int)floor(25. * mStageY + 0.5);
  }
  if (m_iMag > 0)
    mMag100 = (short int)floor(0.01 * m_iMag + 0.5);
  if (m_fIntensity >= 0.)
    mIntensity = (short int)floor(25000. * m_fIntensity + 0.5);
  if (m_fDose >= 0.)
    SetDose(m_fDose);
}

void EMimageExtra::ValuesFromShorts(int typext)
{
  if (typext & TILT_MASK)
    m_fTilt = (float)(mTilt / 100.);
  if (typext & MONTAGE_MASK) {
    m_iMontageX = mMontageX;
    m_iMontageY = mMontageY;
    m_iMontageZ = mMontageZ;
  }
  if (typext & VOLT_XY_MASK) {
    mStageX = (float)(mVoltageX / 25.);
    mStageY = (float)(mVoltageY / 25.);
  }
  if (typext & MAG100_MASK)
    m_iMag = 100 * mMag100;
  if (typext & INTENSITY_MASK)
    m_fIntensity = (float)(mIntensity / 25000.);

  // Skip dose!
}
