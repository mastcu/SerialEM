#ifndef EMIMAGEEXTRA_H
#define EMIMAGEEXTRA_H

#define TILT_MASK  0x0001
#define MONTAGE_MASK 0x0002
#define VOLT_XY_MASK 0x0004
#define MAG100_MASK 0x0008
#define INTENSITY_MASK 0x0010
#define DOSE_MASK      0x0020
#define EXTRA_NTYPES  6
#define EXTRA_NO_VALUE -1.e8
#define EXTRA_VALUE_TEST -9.e7

class EMimageExtra
{ 
public:
  EMimageExtra();
  int SetDose(float dose) {return FloatToShorts(dose, mDoseLo, mDoseHi);};
  static int FloatToShorts(float inVal, short int &outLo, short int &outHi);
  
  // Contiguous data written to header as shorts based on flags
  short mTilt;
  unsigned short mMontageX, mMontageY;
  short mMontageZ;
  short mVoltageX, mVoltageY;
  short mMag100;
  short mIntensity;
  short mDoseLo, mDoseHi;

  // True values of data
  float m_fTilt;
  int m_iMontageX, m_iMontageY, m_iMontageZ;
  float mStageX, mStageY, mStageZ;
  int m_iMag;
  float m_fIntensity;
  float m_fDose; 
  int mSuperMontX, mSuperMontY;
  float mPixel;
  int mSpotSize;
  float mDefocus;
  float mISX, mISY;
  float mAxisAngle;
  float mExposure;
  float mBinning;
  int mCamera;
  int mDividedBy2;
  int mReadMode;
  int mMagIndex;
  float mCountsPerElectron;
  float mMin, mMax, mMean;
  float mTargetDefocus;
  float mPriorRecordDose;
  CString mSubFramePath;
  int mNumSubFrames;
  CString mFrameDosesCounts;
  CString mDateTime;
  CString mNavLabel;
  CString mChannelName;
  CString mDE12Version;
  float mPreExposeTime;
  int mNumDE12Frames;
  float mDE12FPS;
  CString mDE12Position;
  CString mDE12CoverMode;
  int mCoverDelay;
  float mTemperature;
  float mFaraday;
  CString mSensorSerial;
  int mReadoutDelay;
  int mIgnoredFrames;
  float slitWidth;
  float energyLoss;
  
  static const int Bytes[EXTRA_NTYPES];
  void ValuesIntoShorts(void);
  void ValuesFromShorts(int typext);
};

#endif
