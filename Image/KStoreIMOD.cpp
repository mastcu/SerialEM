#include "stdafx.h"
#include ".\KStoreIMOD.h"
#include "KStoreMRC.h"
#include "../SerialEM.h"
#include "..\Shared\iimage.h"
#include "..\Shared\b3dutil.h"
#include "..\EMimageExtra.h"

// Opens an IMOD type file for reading
KStoreIMOD::KStoreIMOD(CString filename)
  : KImageStore()
{
  CommonInit();
  mIIfile = iiOpen((char *)((LPCTSTR)filename), "rb");
  if (!mIIfile)
    return;
  mFilename = filename;
  mMode = mIIfile->mode;
  mMin = (int)mIIfile->amin;
  mMax = (int)mIIfile->amax;
  mWidth = mIIfile->nx;
  mHeight = mIIfile->ny;
  mDepth = mIIfile->nz;
}

// Open a new IMOD type file for writing a TIFF image
KStoreIMOD::KStoreIMOD(CString inFilename , FileOptions inFileOpt)
  : KImageStore()
{
  bool isJpeg = inFileOpt.fileType == STORE_TYPE_JPEG;
  CommonInit();
  mFileOpt = inFileOpt;
  mFilename = inFilename;
  if (mFileOpt.fileType != STORE_TYPE_TIFF && !isJpeg)
    return;
  if (mFileOpt.compression == COMPRESS_JPEG || isJpeg)
    mFileOpt.mode = MRC_MODE_BYTE;
  mIIfile = iiOpenNew(inFilename, "w", isJpeg ? IIFILE_JPEG : IIFILE_TIFF);
  if (!mIIfile)
    return;
}

void KStoreIMOD::CommonInit()
{
  KImageStore::CommonInit();
  b3dSetStoreError(1);
  tiffSuppressErrors();
  tiffSuppressWarnings();
  mStoreType = STORE_TYPE_IMOD;
  mIIfile = NULL;
}

KStoreIMOD::~KStoreIMOD(void)
{
  Close();
}

KImage  *KStoreIMOD::getRect(void)
{
  KImage *retVal;
  KImageShort *theSImage;
  KImageFloat *theFImage;
  KImageRGB *theCImage;
  char *theData;
  mPixSize = lookupPixSize(mMode);
  int secSize = mWidth * mHeight * mPixSize;
  
  if (mCur.z < 0 || mCur.z >= mDepth || mIIfile == NULL)
    return NULL; 
  
  if ((mMode < 0 || mMode > 2) && mMode != MRC_MODE_USHORT && mMode != MRC_MODE_RGB)
    return NULL;
  NewArray(theData, char, secSize);
  if (!theData) 
    return NULL;

  switch(mMode){
    case 0:
      retVal = new KImage();
      retVal->useData(theData, mWidth, mHeight); 
      break;
    case 1:
    case MRC_MODE_USHORT:
      theSImage = new KImageShort();
      theSImage->setType(mMode == 1 ? kSHORT : kUSHORT);
      theSImage->useData(theData, mWidth, mHeight); 
      retVal = (KImage *)theSImage;
      break;
    case 2:
      theFImage = new KImageFloat();
      theFImage->useData(theData, mWidth, mHeight); 
      retVal = (KImage *)theFImage;
      break;
    case MRC_MODE_RGB:
      theCImage = new KImageRGB();
      theCImage->useData(theData, mWidth, mHeight); 
      retVal = (KImage *)theCImage;
      break;
  }
  if (iiReadSection(mIIfile, theData, mCur.z)) {
    delete retVal;
    return NULL;
  }
  retVal->flipY();
  return retVal;
}

void KStoreIMOD::Close(void)
{
  if (mIIfile)
    iiDelete(mIIfile);
  mIIfile = NULL;
  mFilename = "";
  KImageStore::Close();
}

int KStoreIMOD::getTiffField(int tag, void *value) 
{
  if (!mIIfile || mIIfile->file != IIFILE_TIFF)
   return -1;
  return tiffGetField(mIIfile, tag, value);
}

// Here's the scoop: you need to call it with a count argument, but it does not get
// returned with a usable count
int KStoreIMOD::getTiffArray(int tag, b3dUInt16 *count, void *value) 
{
  if (!mIIfile || mIIfile->file != IIFILE_TIFF)
    return -1;
  return tiffGetArray(mIIfile, tag, count, value);
}

// Extract either a double or a string from a TIFF field
int KStoreIMOD::getTiffValue(int tag, int type, int tokenNum, double &dvalue, 
                             CString &valstring)
{
  unsigned short usval = 0;
  unsigned int uintval = 0;
  double dval = 0.;
  float fval = 0.;
  unsigned short *usptr;
  unsigned int *uintptr;
  double *dptr;
  float *fptr;
  b3dUInt16 count;

  char *sptr = NULL;
  //CString sepstr = sep;
  //CString token;
  int err;
  //int curpos = 0;
  if (type == TIFFTAG_STRING) {
    err = getTiffField(tag, &sptr);
    if (err <= 0)
      return err;
    CString str(sptr);
    //if (tokenNum <= 0) {
    valstring = str;
    /*} else {
      for (int i = 0; i < tokenNum; i++) {
        token = str.Tokenize(sepstr, curpos);
        if (token.IsEmpty())
          break;
      }
      valstring = token;
    } */

  } else if (type == TIFFTAG_UINT16) {
    if (!tokenNum) {
      err = getTiffField(tag, &usval);
      dvalue = usval;
    } else {
      err = getTiffArray(tag, &count, &usptr);
      dvalue = usptr[tokenNum - 1];
    }
  } else if (type == TIFFTAG_UINT32) {
    if (!tokenNum) {
      err = getTiffField(tag, &uintval);
      dvalue = uintval;
    } else {
      err = getTiffArray(tag, &count, &uintptr);
      dvalue = uintptr[tokenNum - 1];
    }
  } else if (type == TIFFTAG_FLOAT) {
    if (!tokenNum) {
      err = getTiffField(tag, &fval);
      dvalue = fval;
    } else {
      err = getTiffArray(tag, &count, &fptr);
      dvalue = fptr[tokenNum - 1];
    }
  } else if (type == TIFFTAG_DOUBLE) {
    if (!tokenNum) {
      err = getTiffField(tag, &dval);
      dvalue = dval;
    } else {
      err = getTiffArray(tag, &count, &dptr);
      dvalue = dptr[tokenNum - 1];
    }
  }
  return err;
}

// Write a section to the TIFF file
int KStoreIMOD::WriteSection(KImage * inImage)
{
  char *idata;
  bool needToReflip, needToDelete;
  double theMean;
  int retval = 0;
  int resolution = 0;
  bool isJpeg = mFileOpt.fileType == STORE_TYPE_JPEG;
  if (inImage == NULL) 
    return 1;
  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();
  if (mIIfile == NULL)
    return 2;
  if (mFileOpt.fileType != STORE_TYPE_TIFF && !isJpeg)
    return 10;
  mIIfile->nx = mWidth = inImage->getWidth();
  mIIfile->ny = mHeight = inImage->getHeight();
  mMode = inImage->getMode() == kGray ? mFileOpt.mode : MRC_MODE_RGB;
  mPixSize = lookupPixSize(mMode);
  mIIfile->format = IIFORMAT_LUMINANCE;
  mIIfile->file = isJpeg ? IIFILE_JPEG : IIFILE_TIFF;
  mIIfile->type = IITYPE_UBYTE;
  int dataSize = mWidth * mHeight * mPixSize;

  switch(mMode){

  case MRC_MODE_BYTE:
    break;
  case MRC_MODE_SHORT:
    mIIfile->type = IITYPE_SHORT;
    break;
  case MRC_MODE_USHORT:
    mIIfile->type = IITYPE_USHORT;
    break;
  case MRC_MODE_RGB:
    mIIfile->format = IIFORMAT_RGB;
    break;
  case MRC_MODE_FLOAT:
    mIIfile->type = IITYPE_FLOAT;
    break;
  default:
    return 11;
  }

  inImage->Lock();
  idata   = convertForWriting(inImage, false, needToReflip, needToDelete);
  if (!idata) {
    inImage->UnLock();
    return 4;
  }
  if (extra && extra->mPixel > 0)
    resolution = B3DNINT(2.54e8 / extra->mPixel);
  if (isJpeg) {
    if (jpegWriteSection(mIIfile, idata, 1, resolution, -1))
      retval = 6;
  } else {
    minMaxMean(idata, dataSize, mIIfile->amin, mIIfile->amax, theMean);
    inImage->setMinMaxMean(mIIfile->amin, mIIfile->amax, (float)theMean);
    if (extra) {
      extra->mMin = mIIfile->amin;
      extra->mMax = mIIfile->amax;
      extra->mMean = (float)theMean;
    }

    if (mNumTitles > 0)
      tiffAddDescription((LPCTSTR)mTitles);
    mNumTitles = 0;
    mTitles = "";

    // Write the data with given compression; 1 means it is already inverted
    if (tiffWriteSection(mIIfile, idata, mFileOpt.compression, 1, resolution, -1))
      retval = 6;
  }
  if (needToDelete)
    delete [] idata;
  inImage->UnLock();
  return retval;
}
