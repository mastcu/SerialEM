// KStoreMRC.cpp:         A class for saving and reading MRC image files
// //
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer

#include "stdafx.h"
#include "KStoreMRC.h"
#include "KImageScale.h"
#include "../SerialEM.h"
#include "..\Shared\b3dutil.h"
#include "..\Shared\autodoc.h"
#include "..\Shared\iimage.h"
#include "KStoreADOC.h"

const int MRCheadSize = 1024;

// Initialize the header with mostly zeroes
HeaderMRC::HeaderMRC()
{
  int i;
  nx = ny = nz = mode = 0;
  mxst = myst = mzst = 0;
  mx = my = mz = 0;
  dx = dy = dz = 0.0f;
  alpha = beta = gamma = 90.0f;
  mapc  = 1;
  mapr  = 2;
  maps  = 3;
  nspg  = 0;
  next  = 0;
  oldId    = 0;
  unused = 0;
  extType[0] = 'S';
  extType[1] = 'E';
  extType[2] = 'R';
  extType[3] = 'I';
  // File is conforming because it has 0 origin, but this has to get zeroed for BYTE/RGB
  nversion = 20140;    
  nbytext  = 0;
  typext = 0;
  type = lensid = n1 = n2 = v1 = v2 = 0;
  xo = yo = zo = 0.0;
  cmap[0] = 'M';
  cmap[1] = 'A';
  cmap[2] = 'P';
  cmap[3] = ' ';
  stamp[0] = 68;
  stamp[1] = 68;
  stamp[2] = stamp[3] = 0;
  imodStamp = IMOD_MRC_STAMP;
  imodFlags = 0;
  rms = -1.;
  nttl = 0;
  for (i = 0; i < 20; i++)
    blank[i] = 0;
  for (i = 0; i < 20; i++)
    blank2[i] = 0;
  for (i = 0; i < 6; i++)
    tiltAngles[i] = 0.;
  memset(&labels[0], 0, 800);
}

// Initialize the header appropriately for the nx, ny, and nz values
void HeaderMRC::Init(int inX, int inY, int inZ, int inMode)
{
    mx = nx = inX;
    my = ny = inY;
    mz = nz = inZ;
     
    mode = inMode;
    if (mode == MRC_MODE_BYTE || mode == MRC_MODE_RGB)
      nversion = 0;
     
    dx = (float)nx;
    dy = (float)ny;
    dz = (float)nz;
     
    
    amin  = 1.e38f;
    amean = 0.0f;
    amax  = -1.e38f;
     
}

// Check whether  a file is MRC based upon nx, ny, nz, mode and filesize
BOOL KStoreMRC::IsMRC(CFile *inFile)
{
  double fileSize, imageSize;
  BOOL sizeError = false;
  if (inFile == NULL) return false;
  
  // Try to get the file size; if there is an error, set to-1 and see if
  // everything elese is consistent with that
  try {
    fileSize = (double)inFile->GetLength();
    if (fileSize < MRCheadSize) return false;
  }
  catch(CFileException *perr) {
    perr->Delete();
    sizeError = true;
  }
  try {
      
    Int32 nx, ny, nz, mode, mapc, mapr, maps;
    inFile->Read(&nx, 4);   
    inFile->Read(&ny, 4);   
    inFile->Read(&nz, 4);   
    inFile->Read(&mode, 4);
    inFile->Seek(48, CFile::current);
    inFile->Read(&mapc, 4);
    inFile->Read(&mapr, 4);
    inFile->Read(&maps, 4);

  
    if ((nx <= 0) || (ny <= 0) || (nz <= 0) 
      || (mode < 0) || (mode > 16) || 
      (nx > 65535 && ny > 65535 && nz > 65535) ||
      mapc < 0 || mapc > 4 || mapr < 0 || mapr > 4 || maps < 0 || maps > 4)
        return false;
  
    int pixSize = lookupPixSize(mode);
    imageSize = (double)nx * ny * pixSize;
    // If the filesize gave an error, require the z size to be within
    // 3 or 4 of what would make the file be 4 GB
    if (sizeError) {
      if (nz < (int)(4294000000 / imageSize) - 4)
        return false;
    } else {
      imageSize *= nz;
      imageSize += MRCheadSize;
      if (fileSize < imageSize) return false;
    }   
    return true;
  
  }
  
  catch(CFileException *perr) {
    perr->Delete();
    return false;
  } 
}

// Create a new file.  Create a blank header and  allow 
// for extra information past regular header
KStoreMRC::KStoreMRC(CString inFilename , FileOptions inFileOpt)
  : KImageStore()
{
  CString mdocName;
  long headSize = MRCheadSize;
  mHead = new HeaderMRC();
  mExtra = NULL;
  mPixelSpacing = 1.;
  mMontCoordsInMdoc = false;
  mNumWritten = 0;
  mMeanSum = 0.;

  mFileOpt = inFileOpt;

  // Figure out amount of extra header needed
  mHead->typext = inFileOpt.typext;
  mHead->nbytext = 0;
  for (int i=0; i <  EXTRA_NTYPES; i++) {
    if ((( 1  << i) & mHead->typext) != 0)
      mHead->nbytext += EMimageExtra::Bytes[i];
  }
  int needed = mHead->nbytext * mFileOpt.maxSec;
  int nBlocks = (needed + MRCheadSize - 1) / MRCheadSize;
  mHead->next = nBlocks * MRCheadSize;

  try {

    if (mHead->next) {
      NewArray(mExtra, char, mHead->next);

      // mFile being NULL is the only sign of failure being returned
      if (mExtra == NULL)
        return;
      for (int i = 0; i < mHead->next; i++)
        mExtra[i] = 0x00;
    }

    mHeadSize = MRCheadSize + mHead->next;
    mMode = mFileOpt.mode;
    if (mMode == MRC_MODE_BYTE)
      mHead->nversion = 0;
    mFilename = inFilename;
    mPixSize = lookupPixSize(mMode);
  
    // Open file with creation and truncation
    // The user has already confirmed that it is OK to overwrite
    mFile = new CFile(inFilename, CFile::modeCreate | 
       CFile::modeReadWrite |CFile::shareDenyWrite);

    Seek(0, CFile::begin);
    Write(mHead, headSize);
    if (mHead->next)
      Write(mExtra, mHead->next);

    // Back up an existing mdoc file regardless of whether a new one is requested
    // Then try to create a new autodoc if needed
    mdocName = getAdocName();
    imodBackupFile((char *)(LPCTSTR)mdocName);
    if ((inFileOpt.useMont() ? inFileOpt.montUseMdoc : inFileOpt.useMdoc) || 
      inFileOpt.montageInMdoc) {
        if (!AdocAcquireMutex()) {
          Close();
          return;
        }
        mAdocIndex = AdocNew();
        AdocReleaseMutex();
        if (mAdocIndex < 0) {
          Close();
          return;
        }
        mMontCoordsInMdoc = inFileOpt.montageInMdoc;
    }

  }
  // Any kind of error, close file and set mFile to NULL
  catch(CFileException *perr) {
    Close();
    perr->Delete();
    return;
  } 
}

// Create a storage from an existing file.
KStoreMRC::KStoreMRC(CFile *inFile)
  : KImageStore(inFile)
{
  CFileStatus status;
  CString mdocName;
  char *single;
  int ifmont = 0;
  mHeadSize = MRCheadSize;
  long headSize = MRCheadSize;
  mHead = NULL;
  mExtra = NULL;
  mPixelSpacing = 1.;
  mMontCoordsInMdoc = false;
  HeaderMRC *head = new HeaderMRC;
  double fileSize;
  BOOL sizeError = false;

  try {
    fileSize = (double)mFile->GetLength();
  }
  catch (CFileException *perr) {
    perr->Delete();
    sizeError = true;
  }

  try{
    if (mFile == NULL) 
      throw (-1);
    if (head == NULL) 
      throw (-1);
    if (!sizeError && fileSize < (unsigned int)MRCheadSize)
      throw(-1);
    Seek(0, CFile::begin);
    Read(head, headSize);
    Int32     nx, ny, nz;//, nobs;
    nx = head->nx;
    ny = head->ny;
    nz = head->nz;
    mMode = head->mode;
    
    mMin = (int)head->amin;
    mMax = (int)head->amax;
    mHeadSize += head->next;
    mNumWritten = nz;
    mMeanSum = nz * head->amean;
    
    // Make sure files aren't byte-swapped
    if (nx <= 0 || ny <= 0 || nz <= 0  || (nx > 65535 &&  ny > 65535 && nz > 65535)
      || (mMode < 0) || (mMode > 16))
        throw(-1);

    mPixSize = lookupPixSize(mMode);

    unsigned int imageSize = nx * ny * mPixSize;

    // If the filesize gave an error, require the z size to be within
    // 3 or 4 of what would make the file be 4 GB
    if (sizeError) {
      if (nz < (int)(4294000000 / imageSize) - 4)
        throw (-1);
    } else {
      imageSize *= nz;
      imageSize += mHeadSize;
      if (fileSize < imageSize) 
        throw (-1);
    }   

    
    
    mWidth  = nx;
    mHeight = ny;
    mDepth  = nz;
    
    if (head->next) {
      NewArray(mExtra, char, head->next);
      if (mExtra == NULL) 
        throw(-1);
      Read(mExtra, head->next);
    }
    mHead = head;
    if (nz)
      mPixelSpacing = head->dz / nz;

    // Now see if there is an mdoc file
    mdocName = getAdocName();
    if (CFile::GetStatus((LPCTSTR)mdocName, status) && AdocAcquireMutex()) {
      mAdocIndex = AdocRead((char *)(LPCTSTR)mdocName);
      if (mAdocIndex < 0) {
        AdocReleaseMutex();
        throw(-1);
      }
      if (AdocGetString(ADOC_GLOBAL, 0, ADOC_SINGLE, &single)) {
        AdocClear(mAdocIndex);
        mAdocIndex = -1;
        AdocReleaseMutex();
        throw(-1);
      }
      free(single);

      // Determine if montage coords solely in mdoc
      AdocGetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, &ifmont);
      mMontCoordsInMdoc = (!(mHead->typext & MONTAGE_MASK) && ifmont);
      AdocReleaseMutex();
    }
  } 
  
  catch(CFileException *perr) {
    mPixSize = mWidth = mHeight = mDepth = 0;
    delete head;
    Close();
    perr->Delete();
    return;
  } 
  catch( int ) {
    mPixSize = mWidth = mHeight = mDepth = 0;
    delete head;
    Close();
  }
}

KStoreMRC::~KStoreMRC()
{
  if (mHead != NULL)
    delete mHead;
  if (mExtra != NULL)
    delete [] mExtra;
}

// Add a title to the header; assume it will be saved by the next image write
int KStoreMRC::AddTitle(char *inTitle)
{
  int retval = 0;
  if (mFile == NULL)
    return 1;
  if (mHead->nttl == 10)
    return -1;
  int len = (int)strlen(inTitle);
  memcpy(&mHead->labels[mHead->nttl][0], inTitle, len);
  
  // Pad with spaces, not nulls
  for (; len < 80; len++)
    mHead->labels[mHead->nttl][len] = 0x20;
  mHead->nttl++;

  // Add to mdoc
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 2;
    retval = AdocAddSection(ADOC_TITLE, inTitle);
    AdocReleaseMutex();
  }
  return retval;
}

const char *KStoreMRC::GetTitle(int index)
{
  if (index < 0 || index >= mHead->nttl)
    return NULL;
  return &mHead->labels[index][0];
};

  
int KStoreMRC::AppendImage(KImage *inImage)
{
  if (inImage == NULL) 
    return 1;
   
  // If this is a new image init everything.
  if ((mHead->nx == 0) && (mHead->ny == 0) && (mHead->nz == 0) ){
    inImage->getSize(mWidth, mHeight);  
/*    int mode = 0;   
    int dataType = inImage->getType();
*/    
    // If it is RGB, fix the mode, otherwise it was set from mFileOpt before
    if (inImage->getType() == kRGB) {
      mMode = MRC_MODE_RGB;
      mHead->Init(mWidth, mHeight, 0, mMode);
    } else
      mHead->Init(mWidth, mHeight, 0, mFileOpt.mode);

    // Should not be needed but just in case...
    mPixSize = lookupPixSize(mMode);
    mHead->dx *= mPixelSpacing;
    mHead->dy *= mPixelSpacing;

    if (mAdocIndex >= 0) {
      if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
        return 14;
      RELEASE_RETURN_ON_ERR(AdocSetKeyValue(ADOC_GLOBAL, 0, ADOC_SINGLE, 
        (char *)(LPCTSTR)mFile->GetFileName()), 15);
      RELEASE_RETURN_ON_ERR(AdocSetTwoIntegers(ADOC_GLOBAL, 0, ADOC_SIZE, mWidth, mHeight)
        , 15);
      if ((mFileOpt.typext & MONTAGE_MASK) || mMontCoordsInMdoc) {
        RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, 1), 15);
      }
      RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_MODE, mMode), 15);
      AdocReleaseMutex();
    }

  }
  mCur.z = mHead->nz;
  
    
  // Write image data to end of file.
  int err = WriteSection(inImage, mHead->nz);
  mDepth = mHead->nz;
  return err;
}

// Write a section in the indicated place in the file
int KStoreMRC::WriteSection (KImage *inImage, int inSect)
{
  float fMin, fMax, tMin, tMax, tMean;
  double theMean;
  char *idata;
  char sectext[10];
  int i, j, sectInd;
  CString mdocName;
  bool needToReflip, needToDelete, needNewSect, gotMutex = false;
  int retval = 5;

  if (inImage == NULL) 
    return 1;
  if (mFile == NULL) 
    return 2;

  if ((inSect + 1) * mHead->nbytext >  mHead->next)
    return 3;

  if (mWidth != inImage->getWidth() || mHeight != inImage->getHeight())
    return 17;
  
  inImage->Lock();

  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();
  int dataSize = mWidth * mHeight * mPixSize;
  int headSize = MRCheadSize;
  
  idata   = convertForWriting(inImage, true, needToReflip, needToDelete);
  if (!idata) {
    inImage->UnLock();
    return 4;
  }

  try {
    BigSeek(mHeadSize, dataSize, inSect, CFile::begin);
    retval++;       // Error 5 on bigseek, 6 on write
    Write(idata, dataSize);

    // Test for whether need new autodoc section before changing depth; definitely need
    // one if this section has never been written before
    needNewSect = inSect >= mDepth;

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
        float newMin = 1.e37f, newMax = -1.e37f;
        double newSum = 0.;
        int newWritten = AdocGetNumberOfSections(ADOC_ZVALUE);
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
          mHead->amin = newMin;
          mHead->amax = newMax;
        }

      }
      AdocReleaseMutex();
    }

    // Get the min, max, and mean
    minMaxMean(idata, dataSize, fMin, fMax, theMean);
    inImage->setMinMaxMean(fMin, fMax, (float)theMean);
    if (extra) {
      extra->mMin = fMin;
      extra->mMax = fMax;
      extra->mMean = (float)theMean;
    }

    if (fMin < mHead->amin)
      mHead->amin = fMin;
    if (fMax > mHead->amax)
      mHead->amax = fMax;
    
    // Adjust file size for section past the end of current size, and update the depth
    if (inSect >= mHead->nz) 
      mHead->nz = inSect + 1;
    mDepth = mHead->nz;
    mHead->mz = mHead->nz;
    mHead->dz = (float)mHead->nz * mPixelSpacing;

    // adjust mean sum and mean
    mMeanSum += theMean;
    mNumWritten++;
    mHead->amean = (float)(mMeanSum / mNumWritten);
    
    // ReWrite header information.
    retval++;      // Error 7 on head write
    Seek(0, CFile::begin);
    Write(mHead, headSize);

    if (mHead->next) {
      // Write extended header information.
      
      char *edata = (char *)inImage->GetUserData();
      int offset = inSect * mHead->nbytext;
      char *pExtra = mExtra + offset;
      for (i = 0; i < EXTRA_NTYPES; i++) {
        if (((1 << i) & mHead->typext) != 0) {
          for ( j = 0; j < EMimageExtra::Bytes[i]; j++) {
            if (edata) 
              *pExtra++ = *edata++;
            else
              *pExtra++ = 0;
          }
        } else if (edata)
          edata +=  EMimageExtra::Bytes[i];
      }
      retval++;    // Error 8 on extended write
      Seek(headSize + offset, CFile::begin);
      pExtra = mExtra + offset;
      int theExtSize = mHead->nbytext;
      Write(pExtra, theExtSize);
    }

    if (mAdocIndex >= 0) {
      if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
        throw 14;
      gotMutex = true;

      // Add a new section if test above indicated it was needed
      sprintf(sectext, "%d", inSect);
      if (needNewSect) {
        sectInd = AdocAddSection(ADOC_ZVALUE, sectext);
        if (sectInd < 0)
          throw 15;
      } else {

        // otherwise try to look up an existing section, and if there is none, insert one
        // in the right place to keep the sections in order
        sectInd = AdocLookupSection(ADOC_ZVALUE, sectext);
        if (sectInd < -1)
          throw 14;
        if (sectInd == -1) {
          sectInd = AdocFindInsertIndex(ADOC_ZVALUE, inSect);
          if (sectInd < 0)
            throw 14;
          if (AdocInsertSection(ADOC_ZVALUE, sectInd, sectext) < 0)
            throw 15;
        }
      }

      // add all the values, and rewrite the adoc or append to it
      if (KStoreADOC::SetValuesFromExtra(inImage, ADOC_ZVALUE, sectInd))
        throw 15;
      mdocName = getAdocName();
      if (sectInd && needNewSect) {
        if (AdocAppendSection((char *)(LPCTSTR)mdocName) < 0)
          throw 16;
      } else {
        if (AdocWrite((char *)(LPCTSTR)mdocName) < 0)
          throw 16;
      }
    }

    retval = 0;   // Set error to 0 after last success
  }
  catch(CFileException *perr) {
    perr->Delete();
  }
  catch(int errval) {
    retval = errval;
  }
  if (gotMutex)
    AdocReleaseMutex();

  if (needToReflip) {
    inImage->flipY();
    inImage->setNeedToReflip(false);
  }
  if (needToDelete)
    delete [] idata;
  inImage->UnLock();
  return retval;
}

// Read a section from the file at the current z value
KImage *KStoreMRC::getRect(void)
{
  int width =  mWidth;
  int height = mHeight;
  char *theData;
  Int32 secSize = width * height * mPixSize;
  
  if (mCur.z < 0 || mCur.z >= mDepth || mFile == NULL)
    return NULL; 
  Int32 offset  = secSize * mCur.z;
  
  if ((mMode < 0 || mMode > 2) && mMode != MRC_MODE_USHORT && mMode != MRC_MODE_RGB)
    return NULL;
  NewArray(theData, char, secSize);
  if (!theData) 
    return NULL;

  try{  
    BigSeek(mHeadSize, secSize, mCur.z, CFile::begin);
    KImage *retVal;
    KImageShort *theSImage;
    KImageFloat *theFImage;
    KImageRGB *theCImage;
    switch(mMode){
      case 0:
        retVal = new KImage();
        retVal->useData(theData, width, height); 
        break;
      case 1:
      case MRC_MODE_USHORT:
        theSImage = new KImageShort();
        theSImage->setType(mMode == 1 ? kSHORT : kUSHORT);
        theSImage->useData(theData, width, height); 
        retVal = (KImage *)theSImage;
        break;
      case 2:
        theFImage = new KImageFloat();
        theFImage->useData(theData, width, height); 
        retVal = (KImage *)theFImage;
        break;
      case MRC_MODE_RGB:
        theCImage = new KImageRGB();
        theCImage->useData(theData, width, height); 
        retVal = (KImage *)theCImage;
        break;
    }
    
    Read( theData , secSize);
    retVal->flipY();

    if (mHead->next || mAdocIndex >= 0) {

      // Get extra header info into the record
      EMimageExtra *theExtra = new EMimageExtra;
      if (theExtra == NULL) 
        throw (-1);
      retVal->SetUserData(theExtra);
      if (getExtraData(theExtra, mCur.z))
        throw(-1);
    }
    return retVal;
    
  }
  catch(CFileException *perr) {
    perr->Delete();
    return NULL;
  }
  catch(int ){
    return NULL;
  }
}

int KStoreMRC::getExtraData(EMimageExtra * extra, int section)
{
  int i, j, retval = 0;
  char *idata = (char *)extra;
  char *bdata = mExtra + section * mHead->nbytext;
  if (mHead->next) {
    for (i = 0; i < EXTRA_NTYPES; i++) {
      if (((1 << i) & mHead->typext) != 0) {
        for (j = 0; j < EMimageExtra::Bytes[i]; j++)
          *idata++ = *bdata++;
      } else
        idata +=  EMimageExtra::Bytes[i];
    }
    extra->ValuesFromShorts(mHead->typext);
  }

  // Overlay header data with mdoc data
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return(-1);
    if (KStoreADOC::LoadExtraFromValues(extra, i, ADOC_ZVALUE, section))
      retval = -1;
    AdocReleaseMutex();
  }
  return retval;
}


// Get a pixel value.  Unused so far, but maintained with new calls
float KStoreMRC::getPixel(KCoord &inCoord)
{
  float thePixel = 0.;
  int thePixSize = mPixSize;
    int width =  mWidth;
  int height = mHeight;
  Int32 secSize = width * height * mPixSize;
  Int32 offset  = secSize * inCoord.z;
  offset = width * inCoord.y * mPixSize;
  offset += inCoord.x * mPixSize;
  
  try {
    BigSeek(mHeadSize + offset, secSize, inCoord.z, CFile::begin);
    switch(mMode){
    case 0:
      unsigned char ucharPixel;
      Read(&ucharPixel, thePixSize);
      thePixel = ucharPixel;
      break;
    case 1:
      short shortPixel;
      Read(&shortPixel, thePixSize);
      thePixel = shortPixel;
      break;
    case MRC_MODE_USHORT:
      unsigned short ushortPixel;
      Read(&ushortPixel, thePixSize);
      thePixel = ushortPixel;
      break;
    case 2:
      float floatPixel;
      Read(&floatPixel, thePixSize);
      thePixel = floatPixel;
      break;
    default:
      return 0;
      break;  
    }
  }
  catch(CFileException *perr) {
    perr->Delete();
  }
  return(thePixel);
}

// Checks whether the file is a montage and fills the parameter set; returns
//   0 if not a montage
//   1 if it is a montage
//   -1 if it is montage and piece list is corrupted
int KStoreMRC::CheckMontage(MontParam *inParam)
{
  int ifmont;
  if (iiMRCcheckPCoord((MrcHeader *)mHead) == 0 && !mMontCoordsInMdoc)
    return 0;
  if (mMontCoordsInMdoc) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 0;
    if (AdocGetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, &ifmont) || ifmont <= 0)
      ifmont = 0;
    AdocReleaseMutex();
    if (!ifmont)
      return 0;
  }
  return KImageStore::CheckMontage(inParam, mHead->nx, mHead->ny, mHead->nz);  
}

// Get the piece coordinates for a section.  Return 0 if OK, -1 if section bad
int KStoreMRC::getPcoord(int inSect, int &outX, int &outY, int &outZ)
{
  char *edata;
  unsigned short int usTemp;
  short int sTemp;
  int retval = 0;
  int offset = inSect * mHead->nbytext;
  if (mMontCoordsInMdoc) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return -1;
    if (AdocGetThreeIntegers(ADOC_ZVALUE, inSect, ADOC_PCOORD, &outX, &outY, &outZ))
      retval = -1;
    AdocReleaseMutex();
  } else {
    if ((mHead->typext & TILT_MASK) != 0) 
      offset +=   EMimageExtra::Bytes[0];
    if (inSect < 0 || offset >= mHead->next)
      return -1;
    char *idata = mExtra + offset;
    edata = (char *)&usTemp;
    edata[0] = *idata++;
    edata[1] = *idata++;
    outX = usTemp;
    edata[0] = *idata++;
    edata[1] = *idata++;
    outY = usTemp;
    edata = (char *)&sTemp;
    edata[0] = *idata++;
    edata[1] = *idata++;
    outZ = sTemp;
  }
  return retval;
}

// Invert the Z values of montage coordinates for a bidirectional tilt series
int KStoreMRC::InvertPieceZCoords(int zmax)
{
  short int sTemp;
  int ind, pcX, pcY, pcZ, retval = 0;
  char *edata= (char *)&sTemp;
  char *idata = mExtra + 4;
  CString mdocName = getAdocName();
  if (!mMontCoordsInMdoc) {

    // Extended header is supposed to have montage: make sure the flag is set
    if (!(mHead->typext & MONTAGE_MASK))
      return 1;
    if ((mHead->typext & TILT_MASK) != 0) 
      idata +=   EMimageExtra::Bytes[0];

    // Loop through the extra data, fetch out a Z avlue, invert it, put it back
    for (ind = 0; ind < mHead->nz; ind++) {
      edata[0] = *idata;
      edata[1] = *(idata + 1);
      sTemp = zmax - 1 - sTemp;
      *idata = edata[0];
      *(idata + 1) = edata[1];
      idata += mHead->nbytext;
    }

    // Rewrite the whole extended header
    try {
      Seek(MRCheadSize, CFile::begin);
      Write(mExtra, mHead->nz * mHead->nbytext);
    }
    catch(CFileException *perr) {
      perr->Delete();
      return 2;
    } 
  }

  // If there is an autodoc, now loop through it
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return 3;
    for (ind = 0; ind < mHead->nz; ind++) {
      RELEASE_RETURN_ON_ERR(AdocGetThreeIntegers(ADOC_ZVALUE, ind, ADOC_PCOORD, &pcX, 
        &pcY, &pcZ), 4);
      pcZ = zmax - 1 - pcZ;
      RELEASE_RETURN_ON_ERR(AdocSetThreeIntegers(ADOC_ZVALUE, ind, ADOC_PCOORD, pcX, pcY, 
        pcZ), 5);
    }
    if (AdocWrite((char *)(LPCTSTR)mdocName) < 0)
      retval = 6;
    AdocReleaseMutex();
  }
  return retval;
}

// Get the stage coordinates for a section
int KStoreMRC::getStageCoord(int inSect, double &outX, double &outY)
{
  EMimageExtra extra;
  if (getExtraData(&extra, inSect))
    return -1;
  if (extra.mStageX == EXTRA_NO_VALUE || extra.mStageY == EXTRA_NO_VALUE)
    return -1;
  outX = extra.mStageX;
  outY = extra.mStageY;
  return 0;
}

// A set of routines to operate on the file handle with SDK routines
// since the cheating method of BigSeek did not work
void KStoreMRC::BigSeek(int base, int size1, int size2, UINT flag)
{
  LARGE_INTEGER li;
  __int64 offset = base + (__int64)size1 * (__int64)size2;
  DWORD origin = FILE_BEGIN;
  if (flag == CFile::current)
    origin = FILE_CURRENT;

  li.QuadPart = offset;
  li.LowPart = SetFilePointer ((HANDLE)mFile->m_hFile, li.LowPart, &li.HighPart, origin);

  if (li.LowPart == (DWORD)-1 && GetLastError() != NO_ERROR)
		CFileException::ThrowOsError((LONG)::GetLastError());
}

// Read and Write routines may not be needed but CFile could get unhappy if the position
// is outside its 32 bit bounds
void KStoreMRC::Write(void *buf, int count)
{
  DWORD nWritten;
  if (!WriteFile((HANDLE)mFile->m_hFile, buf, count, &nWritten, NULL))
		CFileException::ThrowOsError((LONG)::GetLastError());
}

void KStoreMRC::Read(void *buf, int count)
{
  DWORD nRead;
  if (!ReadFile((HANDLE)mFile->m_hFile, buf, count, &nRead, NULL))
		CFileException::ThrowOsError((LONG)::GetLastError());
}

void KStoreMRC::Seek(int offset, UINT flag)
{
  BigSeek(offset, 0, 0, flag);
}

void KStoreMRC::SetPixelSpacing(float pixel)
{
  if (!mHead)
    return;
  mPixelSpacing = pixel;
  mHead->dx = (float)mHead->nx * pixel;
  mHead->dy = (float)mHead->ny * pixel;
  mHead->dz = (float)mHead->nz * pixel;
  if (mAdocIndex < 0 || AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return;
  AdocSetFloat(ADOC_GLOBAL, 0, ADOC_PIXEL, pixel);
  AdocReleaseMutex();
}

