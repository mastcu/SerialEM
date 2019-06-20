// KImageStore.cpp:       A base class for image storage, inherits KImageBase
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer
// Adapted for Tecnai, DNM, 2-16-02

#include "stdafx.h"
#include ".\KImageStore.h"
#include ".\KStoreADOC.h"
#include "KImageScale.h"
#include "../SerialEM.h"
#include "..\Shared\autodoc.h"
#include "..\Shared\b3dutil.h"


KImageStore::KImageStore(CString inFilename)
  : KImageBase()
{
  CommonInit();
  mFilename = inFilename;

  // Pick most generic set of flags: create if necessary, do not truncate
  // if it exists
  mFile = new CFile(inFilename, CFile::modeCreate | 
    CFile::modeNoTruncate | CFile::modeReadWrite |CFile::shareDenyWrite);
}

KImageStore::KImageStore(CFile *inFile)
  : KImageBase()
{
  CommonInit();
  mFile = inFile;
  mFilename = inFile->GetFilePath();
}

KImageStore::KImageStore()
  : KImageBase()
{
  CommonInit();
  mFilename          = "";
  mFile              = NULL;
}

void KImageStore::CommonInit(void)
{
  mMin = 0;
  mMax = 255;
  mStoreType = STORE_TYPE_MRC;
  mAdocIndex = -1;
  mMontCoordsInMdoc = false;
  mFracIntTrunc = 0.;
  mNumTitles = 0;
  mTitles = "";
  mOpenMarkerExt = NULL;
}

KImageStore::~KImageStore()
{
  Close();
}

// Accumulate titles in order to output as description for TIFF file
int KImageStore::AddTitle(const char *inTitle)
{
  CString copy;
  if (mNumTitles >= MRC_NLABELS)
    return 1;
  copy = inTitle;
  if (copy.GetLength() > MRC_LABEL_SIZE)
    copy = copy.Left(MRC_LABEL_SIZE);
  if (mNumTitles)
    mTitles += "\n";
  mTitles += copy;
  mNumTitles++;
  return 0;

}

int KImageStore::Open(CFile *inFile)
{
  if (mFile) {
    mFile->Close();
    delete mFile;
  }
  mFile = inFile;
  return 0;
}
int KImageStore::Open(CFile *inFile, int inWidth, int inHeight, int inDepth)
{
  Open(inFile);
  mWidth = inWidth;
  mHeight = inHeight;
  mDepth  = inDepth;
  return 0;
}

void KImageStore::Close()
{
  if (mOpenMarkerExt) {
    UtilRemoveFile(getFilePath() + mOpenMarkerExt);
    B3DFREE(mOpenMarkerExt);
  }
  if (mFile) {
    mFile->Close();
    delete mFile;
  }
  if (mAdocIndex >= 0 && AdocAcquireMutex()) {
    AdocClear(mAdocIndex);
    AdocReleaseMutex();
  }
  mAdocIndex = -1;
  mFilename          = "";
  mFile              = NULL;
}

int KImageStore::Flush()
{
  if (mFile)
    mFile->Flush();
  return 0;
}

// Create a lock file to last as long as this file is open with given extension
int KImageStore::MarkAsOpenWithFile(const char *extension)
{
  FILE *fp;
  if (!mOpenMarkerExt) {
    mOpenMarkerExt = _strdup(extension);
    if (!mOpenMarkerExt)
      return 1;
    fp = fopen((LPCTSTR)(getFilePath() + extension), "w");
    if (fp) {
      fclose(fp);
      return 0;
    }
    B3DFREE(mOpenMarkerExt);
  }
  return 1;
}

// Compute a simple weighted sum of the filename characters as a good enough checksum
// for determining if something is the same file
unsigned int KImageStore::getChecksum()
{
  unsigned int checksum = 0;
  int fib = 1;
  for (int ind = 0; ind < mFilename.GetLength(); ind++) {
    fib += ind;
    checksum += fib * (int)mFilename.GetAt(ind);
  }
  return checksum;
}

void KImageStore::setTime(int inTime)
{
  mOrigin.t = mCur.t = inTime;
}
void KImageStore::setSection(int inSection)
{
  mOrigin.z = mCur.z = inSection;
}

float KImageStore::getPixel(KCoord &/*inCoord*/)
{
  return(0.);
}

CString KImageStore::getFilePath()
{
  if (mStoreType == STORE_TYPE_ADOC)
    return mFilename;
  if (!mFile)
    return "";
  return mFile->GetFilePath();
}

CString KImageStore::getAdocName(void)
{
  if (mStoreType == STORE_TYPE_ADOC)
    return mFilename;
  if (!mFile)
    return "";
  return mFile->GetFilePath() + ".mdoc";
}

int KImageStore::lookupPixSize(int inMode)
{
  int pixSize;
  switch(inMode){
  case 1:  pixSize = 2; break;
  case 2:  pixSize = 4; break;
  case 3:  pixSize = 4; break;
  case 4:  pixSize = 8; break;
  case MRC_MODE_USHORT:  pixSize = 2; break;
  case 16: pixSize = 3; break;
  default: pixSize = 1;
  }
  return pixSize;
}

// Get the min, max, and mean of the data in a buffer
void KImageStore::minMaxMean(char * idata, int dataSize, float & fMin, 
                             float & fMax, double & outMean)
{
  float fVal, fSum;
  int theMin, theMax;
  int i, j;
  int theVal;
  unsigned short *usdata;
  short *sdata;
  float *fdata;
  unsigned char *bdata;

  theMin = 66000;
  theMax = -66000;
  fMin = 1.e38f;
  fMax = -1.e38f;
  int tmpSum;
  double theSum = 0;
  sdata = (short *)idata;
  usdata = (unsigned short *)idata;
  bdata =  (unsigned char *) idata;
  fdata = (float *)idata;
  for (j = 0; j < mHeight; j++) {
    tmpSum = 0;

    switch (mMode) {
      case 0:
        for (i = 0; i < mWidth; i++) {
          theVal = *bdata++; 
          if (theVal < theMin) 
            theMin = theVal;
          if (theVal > theMax)
            theMax = theVal;
          tmpSum += theVal;
        }
        theSum += tmpSum;
        break;

      case 1:
        for (i = 0; i < mWidth; i++) {
          theVal = *sdata++; 
          if (theVal < theMin) 
            theMin = theVal;
          if (theVal > theMax)
            theMax = theVal;
          tmpSum += theVal;
        }
        theSum += tmpSum;
        break;

      case 2:
        fSum = 0.;
        for (i = 0; i < mWidth; i++) {
          fVal = *fdata++; 
          if (fVal < fMin) 
            fMin = fVal;
          if (fVal > fMax)
            fMax = fVal;
          fSum += fVal;
        }
        theSum += fSum;
        break;

      case MRC_MODE_USHORT:
        for (i = 0; i < mWidth; i++) {
          theVal = *usdata++; 
          if (theVal < theMin) 
            theMin = theVal;
          if (theVal > theMax)
            theMax = theVal;
          tmpSum += theVal;
        }
        theSum += tmpSum;
        break;

      case MRC_MODE_RGB:
        theMin = 0;
        theMax = 255;
        theSum += 127;
        break;

    }
  }
  outMean = theSum / (mHeight * mWidth);
  if (mMode != 2) {
    fMin = (float)theMin;
    fMax = (float)theMax;
  }

}

// Convert the data array as needed based on image and file mode and policies,
// making a new array if necessary.  Set needFlipped if the image needs to be
// flipped for storage; needToReflip is set true if the image itself needs to
// be reflipped when done; needToDelete is set true if a new array is made
// The image should be locked before calling this routine
char *KImageStore::convertForWriting(KImage *inImage, bool needFlipped, 
                                     bool &needToReflip, bool &needToDelete)
{
  unsigned short *usdata;
  unsigned short uval;
  short *sdata;
  unsigned char *bdata;
  int i, j, jstart, jend, jdir, numTrunc = 0;
  int theVal;
  int dataSize = mWidth * mHeight * mPixSize;
  
  int theType = inImage->getType();
  char *idata   = (char *)inImage->getData();
  needToReflip = false;
  needToDelete = true;
  if (needFlipped) {
    jstart = mHeight - 1;
    jend = 0;
    jdir = -1;
  } else {
    jstart = 0;
    jend = mHeight - 1;
    jdir = 1;
  }

  if ( (theType == kUBYTE && mMode == 0) ||
    (theType == kSHORT && mMode == 1) || 
    (theType == kUSHORT && mMode == MRC_MODE_USHORT) || 
    (theType == kFLOAT && mMode == 2) ||
    (theType == kRGB && mMode == MRC_MODE_RGB) ) {

    // Type of data matches mode: just flip the data
    if (needFlipped) {
      inImage->flipY();
      needToReflip = true;
      inImage->setNeedToReflip(true);
    }
    needToDelete = false;

  } else if (theType == kUSHORT && mMode == 1) {

    // Unsigned short going to mode 1 needs to be treated in the selected fashion
    NewArray(idata, char, dataSize);
    if (!idata)
      return NULL;
    sdata = (short *)idata;
    for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
      usdata =  (unsigned short *) inImage->getRowData(j);
      switch (mFileOpt.unsignOpt) {
        case TRUNCATE_UNSIGNED:
          for (i = 0; i < mWidth; i++) {
            uval = *usdata++;
            if (uval > 32767) {
              uval = 32767;
              numTrunc++;
            }
            *sdata++ = (short)uval;
          }
          break;

        case DIVIDE_UNSIGNED:
          for (i = 0; i < mWidth; i++)
            *sdata++ = (short)(*usdata++ >> 1);
          break;

        case SHIFT_UNSIGNED:
          for (i = 0; i < mWidth; i++)
            *sdata++ = (short)((int)*usdata++ - 32768);
          break;
      }
    }

  } else if (theType == kSHORT && mMode == MRC_MODE_USHORT) {

    // Integer data going to unsigned needs to be truncated or shifted up
    NewArray(idata, char, dataSize);
    if (!idata)
      return NULL;
    usdata = (unsigned short *)idata;
    for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
      sdata =  (short *) inImage->getRowData(j);
      switch (mFileOpt.signToUnsignOpt) {
        case TRUNCATE_SIGNED:
          for (i = 0; i < mWidth; i++) {
            theVal = *(sdata++);
            if (theVal < 0) {
              theVal = 0;
              numTrunc++;
            }
            *(usdata++) = (unsigned short)theVal;
          }
          break;

        case SHIFT_SIGNED:
          for (i = 0; i < mWidth; i++)
            *usdata++ = (unsigned short)((int)*sdata++ + 32768);
          break;
      }
    }

  } else if (theType == kUBYTE) {

    // Byte data but integer storage: need an integer copy, unsigned irrelevant
    // Flip it as copy it over
    NewArray(idata, char, dataSize);
    if (!idata)
      return NULL;
    short *ptr = (short *)idata;
    for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
      bdata =  (unsigned char *) inImage->getRowData(j);
      for (i = 0; i < mWidth; i++)
        *ptr++ = *bdata++;
    }
  } else if (theType == kSHORT || theType == kUSHORT) {

    // Otherwise, integer data that must go to bytes
    // Need to find storage limits
    float scaleLo, scaleHi;
    unsigned char **lines = makeLinePointers(idata, mWidth, mHeight, 2);
    if (!lines)
      return NULL;
    percentileStretch(lines, theType, mWidth, mHeight, 1.0, 0, 0, mWidth, mHeight,
      mFileOpt.pctTruncLo, mFileOpt.pctTruncHi, &scaleLo, &scaleHi);
    free(lines);

    // Again, flip data while scaling it 
    int theMin = (int)scaleLo;
    int theRange = (int)(scaleHi - scaleLo);
    NewArray(idata, char, dataSize);
    if (!idata)
      return NULL;
    bdata = (unsigned char *)idata;
    for (j = jstart; jdir * (j - jend) <= 0; j += jdir) {
      if (theType == kSHORT) {
        sdata = (short *)inImage->getRowData(j);
        for (i = 0; i < mWidth; i++) {
          theVal =  ( (sdata[i] - theMin) * 256) / theRange;
          if (theVal < 0) theVal =0;
          if (theVal > 255) theVal = 255;
          *bdata++ = theVal; 
        }

      } else {
        usdata = (unsigned short *)inImage->getRowData(j);
        for (i = 0; i < mWidth; i++) {
          theVal =  ( (usdata[i] - theMin) * 256) / theRange;
          if (theVal < 0) theVal =0;
          if (theVal > 255) theVal = 255;
          *bdata++ = theVal; 
        }
      }
    }

    // It is an error not to fit one of these conditions
  } else
    return NULL;
  mFracIntTrunc = (float)((float)numTrunc / (mWidth * mHeight));
  return idata;
}

// Checks the piece coords and fills parameter set; returns -1 if something is wrong,
// or 1 if OK
int KImageStore::CheckMontage(MontParam *inParam, int nx, int ny, int nz)
{
  int iz, xMax, yMax, zMax;
  int ixp, iyp, izp, xMin, xNonZeroMin, yMin, yNonZeroMin, miss;
  BOOL xOK, yOK;
  int missCheck = 100;

  // Find absolute minimum, and minimum above zero  
  xMin = 1000000;
  xNonZeroMin = 1000000;
  yMin = xMin;
  yNonZeroMin = xMin;
  xMax = yMax = zMax = -1;
  for (iz = 0; iz < nz; iz++) {
    if (getPcoord(iz, ixp, iyp, izp))
      return -1;
    if (ixp < xMin)
      xMin = ixp;
    if (ixp > 0 && ixp < xNonZeroMin)
      xNonZeroMin = ixp;
    if (iyp < yMin)
      yMin = iyp;
    if (iyp > 0 && iyp < yNonZeroMin)
      yNonZeroMin = iyp;
    if (ixp > xMax)
      xMax = ixp;
    if (iyp > yMax)
      yMax = iyp;
    if (izp > zMax)
      zMax = izp;
  }
  
  // Scan all values to make sure they are multiples of the non-zero minima
  // or an even divisor of the minima, in the latter case that is the basic interval
  xOK = yOK = false;
  for (miss = 1; miss <= missCheck; miss++) {
    if (xNonZeroMin % miss)
      continue;
    for (iz = 0; iz < nz; iz++) {
      getPcoord(iz, ixp, iyp, izp);
      if (ixp % (xNonZeroMin / miss))
        break;
    }
    if (iz >= nz) {
      xOK = true;
      xNonZeroMin /= miss;
      break;
    }
  }

  for (miss = 1; miss <= missCheck; miss++) {
    if (yNonZeroMin % miss)
      continue;
    for (iz = 0; iz < nz; iz++) {
      getPcoord(iz, ixp, iyp, izp);
      if (iyp % (yNonZeroMin / miss))
        break;
    }
    if (iz >= nz) {
      yOK = true;
      yNonZeroMin /= miss;
      break;
    }
  }

  if (!xOK || !yOK)
    return -1;
  
  // Passed all the tests: return the values in the Montage parameter set
  inParam->xFrame = nx;
  inParam->yFrame = ny;
  inParam->xNframes = xMax / xNonZeroMin + 1;
  inParam->yNframes = yMax / yNonZeroMin + 1;
  if (inParam->xNframes > 1)
    inParam->xOverlap = nx - xNonZeroMin;
  else
    inParam->xOverlap = 0;
  if (inParam->yNframes > 1)
    inParam->yOverlap = ny - yNonZeroMin;
  else
    inParam->yOverlap = 0;
  inParam->zMax = zMax;
  inParam->zCurrent = zMax + 1;
  return 1;
}

// Return image shift from the autodoc if any
int KImageStore::getImageShift(int inSect, double &outISX, double &outISY)
{
  float ISX = 0., ISY = 0.;
  int retval = 0;
  outISX = outISY = 0.;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return(-1);
  if (AdocGetTwoFloats(mStoreType == STORE_TYPE_ADOC ? ADOC_IMAGE : ADOC_ZVALUE, inSect, 
    ADOC_IMAGESHIFT, &ISX, &ISY))
    retval = -1;
  AdocReleaseMutex();
  outISX = ISX;
  outISY = ISY;
  return retval;
}
