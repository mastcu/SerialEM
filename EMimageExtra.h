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

#define MDOC_FLOAT(nam, ini, tst, sym, str) \
  float nam;
#define MDOC_INTEGER(nam, ini, sym, str) \
  int nam;
#define MDOC_TWO_FLOATS(nam1, nam2, ini, tst, sym, str) \
  float nam1, nam2;
#define MDOC_STRING(nam, sym, str) \
  CString nam;

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
  int m_iMontageX, m_iMontageY, m_iMontageZ;
  int mSuperMontX, mSuperMontY;
  float mMin, mMax, mMean;

#include "Image\MdocDefines.h"

  static const int Bytes[EXTRA_NTYPES];
  void ValuesIntoShorts(void);
  void ValuesFromShorts(int typext);
};
#undef MDOC_FLOAT
#undef MDOC_TWO_FLOATS
#undef MDOC_STRING
#undef MDOC_INTEGER

#endif
