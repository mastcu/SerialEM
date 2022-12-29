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
  mDamagedNz = 0;
  mIncompleteMontZ = 0;
  mStoreType = STORE_TYPE_MRC;
  mAdocIndex = -1;
  mMontCoordsInMdoc = false;
  mFracIntTrunc = 0.;
  mNumTitles = 0;
  mTitles = "";
  mOpenMarkerExt = NULL;
  mMeanSum = 0.;
  mNumWritten = 0;
  mPixelSpacing = 1.;
  mHasPixelSpacing = false;
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

void KImageStore::SetPixelSpacing(float pixel)
{
  mPixelSpacing = pixel;
  mHasPixelSpacing = true;
  if (mAdocIndex < 0 || AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return;
  AdocSetFloat(ADOC_GLOBAL, 0, ADOC_PIXEL, pixel);
  AdocReleaseMutex();
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

// Set the global items in an autodoc if one is defined
int KImageStore::InitializeAdocGlobalItems(bool needMutex, BOOL isMontage, 
  const char *filename)
{
  if (mAdocIndex >= 0) {
    if (needMutex && AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 14;
    if (mStoreType != STORE_TYPE_ADOC) {
      RELEASE_RETURN_ON_ERR(AdocSetKeyValue(ADOC_GLOBAL, 0, ADOC_SINGLE, filename), 15);
    }
    RELEASE_RETURN_ON_ERR(AdocSetTwoIntegers(ADOC_GLOBAL, 0, ADOC_SIZE, mWidth, mHeight)
      , 15);
    if (isMontage) {
      RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, 1), 15);
    }
    RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_MODE, mMode), 15);
    AdocReleaseMutex();
  }
  return 0;
}

// Common functionality for checking whether a new autodoc section is needed, recomputing
// min/max/mean of remaining from metadata when overwriting, then getting the min/max/mean
// and managing the passed variables for them
bool KImageStore::CheckNewSectionManageMMM(KImage *inImage, int inSect, char *idata,
  int &nz, float &amin, float &amax, float &amean)
{
  char sectext[10];
  int i, sectInd, newWritten;
  float fMin, fMax, tMin, tMax, tMean;
  float newMin = 1.e37f, newMax = -1.e37f;
  double theMean, newSum = 0.;
  EMimageExtra *extra = inImage->GetUserData();

  // Test for whether need new autodoc section before changing depth; definitely need
  // one if this section has never been written before
  bool needNewSect = inSect >= mDepth;

  // If there is an autodoc, look up the section to find out for sure if it needs one
  if (!needNewSect && mAdocIndex >= 0 && AdocGetMutexSetCurrent(mAdocIndex) >= 0) {
    sprintf(sectext, "%d", inSect);
    sectInd = AdocLookupSection(ADOC_ZVALUE, sectext);
    if (sectInd < 0) {
      needNewSect = sectInd == -1;
    } else {

      // And if there is an existing section, we are overwriting, so recompute the
      // min/max/meanSum of the rest of the sections so values will be correct after
      // the overwrite
      newWritten = AdocGetNumberOfSections(ADOC_ZVALUE);
      for (i = 0; i < newWritten; i++) {
        if (i == sectInd)
          continue;
        if (AdocGetThreeFloats(ADOC_ZVALUE, i, ADOC_MINMAXMEAN, &tMin, &tMax, &tMean)) {

          // Give up if any error
          newWritten = 0;
          break;
        }
        newSum += tMean;
        ACCUM_MIN(newMin, tMin);
        ACCUM_MAX(newMax, tMax);
      }
      if (newWritten > 0) {
        mNumWritten = newWritten - 1;
        mMeanSum = newSum;
        amin = newMin;
        amax = newMax;
      }

    }
    AdocReleaseMutex();
  }

  // Get the min, max, and mean
  minMaxMean(idata, fMin, fMax, theMean);
  inImage->setMinMaxMean(fMin, fMax, (float)theMean);
  if (extra) {
    extra->mMin = fMin;
    extra->mMax = fMax;
    extra->mMean = (float)theMean;
  }

  ACCUM_MIN(amin, fMin);
  ACCUM_MAX(amax, fMax);
  
  // Adjust file size for section past the end of current size, and update the depth
  if (inSect >= nz)
    nz = inSect + 1;
  mDepth = nz;

  // adjust mean sum and mean
  mMeanSum += theMean;
  mNumWritten++;
  amean = (float)(mMeanSum / mNumWritten);

  return needNewSect;
}

// Common function for adding a section to autodoc when necessary, keeping sections in
// order, putting data in the autodoc from extra, and writing the autodoc if not HDF
int KImageStore::AddExtraValuesToAdoc(KImage *inImage, int inSect, bool needNewSect, 
  bool &gotMutex)
{
  char sectext[10];
  int sectInd;
  CString mdocName;

  gotMutex = false;
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 14;
    gotMutex = true;

    // Add a new section if test above indicated it was needed
    sprintf(sectext, "%d", inSect);
    if (needNewSect) {
      sectInd = AdocAddSection(ADOC_ZVALUE, sectext);
      if (sectInd < 0)
        return 15;
    } else {

      // otherwise try to look up an existing section, and if there is none, insert one
      // in the right place to keep the sections in order
      sectInd = AdocLookupSection(ADOC_ZVALUE, sectext);
      if (sectInd < -1)
        return 14;
      if (sectInd == -1) {
        sectInd = AdocFindInsertIndex(ADOC_ZVALUE, inSect);
        if (sectInd < 0)
          return 14;
        if (AdocInsertSection(ADOC_ZVALUE, sectInd, sectext) < 0)
          return 15;
      }
    }

    // add all the values, and rewrite the adoc or append to it
    if (KStoreADOC::SetValuesFromExtra(inImage, ADOC_ZVALUE, sectInd))
      return 15;
    if (mStoreType != STORE_TYPE_HDF) {
      mdocName = getAdocName();
      if (sectInd && needNewSect) {
        if (AdocAppendSection((char *)(LPCTSTR)mdocName) < 0)
          return 16;
      } else {
        if (AdocWrite((char *)(LPCTSTR)mdocName) < 0)
          return 16;
      }
    }
  }
  return 0;
}

// Add a title
int KImageStore::AddTitleToAdoc(const char * inTitle)
{
  int retval = 0;
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 2;
    retval = AdocAddSection(ADOC_TITLE, inTitle);
    AdocReleaseMutex();
  }
  return retval < 0 ? 1 : 0;
}

int KImageStore::CheckAdocForMontage(MontParam * inParam)
{
  int ifmont;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 0;
  if ((AdocGetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, &ifmont) && 
    AdocGetInteger(ADOC_GLOBAL, 0, "IMOD." ADOC_ISMONT, &ifmont)) || ifmont <= 0)
    ifmont = 0;
  else
    ifmont = KImageStore::CheckMontage(inParam, mWidth, mHeight, mDepth);
  AdocReleaseMutex();
  return ifmont;
}

int KImageStore::GetPCoordFromAdoc(const char *sectName, int inSect, int &outX, int &outY,
  int &outZ, bool gotMutex)
{
  int adocSect = inSect, retval = 0;
  if (!gotMutex && AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return -1;
  if (mStoreType == STORE_TYPE_HDF)
    adocSect = AdocLookupByNameValue(sectName, inSect);
  if (adocSect < 0)
    retval = -1;
  else if (AdocGetThreeIntegers(sectName, adocSect, ADOC_PCOORD, &outX, &outY, &outZ))
    retval = -1;
  if (!gotMutex)
    AdocReleaseMutex();
  return retval;
}

int KImageStore::GetStageCoordFromAdoc(const char *sectName, int inSect, double & outX,
  double & outY)
{
  float X, Y;
  int adocSect = inSect, retval = 0;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return -1;
  if (mStoreType == STORE_TYPE_HDF)
    adocSect = AdocLookupByNameValue(sectName, inSect);
  if (adocSect < 0) {
    retval = -1;
  } else if (AdocGetTwoFloats(sectName, adocSect, ADOC_STAGE, &X, &Y)) {
    retval = -1;
  } else {
    outX = X;
    outY = Y;
  }
  AdocReleaseMutex();
  return retval;
}

// This takes a map from current section number to new section number
int KImageStore::ReorderZCoordsInAdoc(const char *sectName, int *sectOrder, int nz)
{
  int ind, pcX, pcY, pcZ, adocSect, retval = 0;
  CString mdocName;

  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 3;
    for (ind = 0; ind < nz; ind++) {
      if (mStoreType == STORE_TYPE_HDF)
        adocSect = AdocLookupByNameValue(sectName, ind);
      else
        adocSect = ind;
      RELEASE_RETURN_ON_ERR(adocSect < 0, 7);
      RELEASE_RETURN_ON_ERR(AdocGetThreeIntegers(sectName, adocSect, ADOC_PCOORD, &pcX,
        &pcY, &pcZ), 4);
      pcZ = sectOrder[pcZ];
      RELEASE_RETURN_ON_ERR(AdocSetThreeIntegers(sectName, adocSect, ADOC_PCOORD, pcX,
        pcY, pcZ), 5);
    }
    if (mStoreType != STORE_TYPE_HDF) {
      mdocName = getAdocName();
      if (AdocWrite((char *)(LPCTSTR)mdocName) < 0)
        retval = 6;
    }
    AdocReleaseMutex();
  }
  return retval;
}

void KImageStore::AddTitleToLabelArray(char *label, int &numTitle, 
  const char *inTitle)
{
  int len = (int)strlen(inTitle);
  len = B3DMIN(len, MRC_LABEL_SIZE);
  memcpy(&label[0], inTitle, len);

  // Pad with spaces, not nulls
  for (; len < 80; len++)
    label[len] = 0x20;
  numTitle++;

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

int KImageStore::FixInappropriateMontage()
{
  if (montCoordsInAdoc()) {
    mMontCoordsInMdoc = false;
    return 1;
  }
  return 0;
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
void KImageStore::minMaxMean(char * idata, float & fMin, 
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
  size_t dataSize = (size_t)mWidth * mHeight * mPixSize;
  
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
  int iz, xMax, yMax, zMax, badPcZ = -1;
  int ixp, iyp, izp, xMin, xNonZeroMin, yMin, yNonZeroMin, miss;
  BOOL xOK, yOK;
  int missCheck = 100;
  bool gotMutex = mAdocIndex >= 0 && (mStoreType != STORE_TYPE_MRC || montCoordsInAdoc());
  if (gotMutex && AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return -1;

  // Find absolute minimum, and minimum above zero  
  xMin = 1000000;
  xNonZeroMin = 1000000;
  yMin = xMin;
  yNonZeroMin = xMin;
  xMax = yMax = zMax = -1;
  if (mDamagedNz && !getPcoord(nz, ixp, iyp, izp, gotMutex) && izp > 0)
    badPcZ = izp;
  for (iz = 0; iz < nz; iz++) {
    if (getPcoord(iz, ixp, iyp, izp, gotMutex)) {
      if (gotMutex)
        AdocReleaseMutex();
      return -1;
    }
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
    if (badPcZ > 0 && izp >= badPcZ)
      mIncompleteMontZ = badPcZ;
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
      getPcoord(iz, ixp, iyp, izp, gotMutex);
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
      getPcoord(iz, ixp, iyp, izp, gotMutex);
      if (iyp % (yNonZeroMin / miss))
        break;
    }
    if (iz >= nz) {
      yOK = true;
      yNonZeroMin /= miss;
      break;
    }
  }
  if (gotMutex)
    AdocReleaseMutex();

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
