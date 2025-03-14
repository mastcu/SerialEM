#include "stdafx.h"
#include ".\KStoreIMOD.h"
#include "KStoreMRC.h"
#include "../SerialEM.h"
#include "..\Shared\iimage.h"
#include "..\Shared\b3dutil.h"
#include "..\EMimageExtra.h"
#include "..\Shared\autodoc.h"
#include "KStoreADOC.h"

// Opens an IMOD type file for reading
KStoreIMOD::KStoreIMOD(CString filename)
  : KImageStore()
{
  char *vers;
  CommonInit();
  mIIfile = iiOpen((char *)((LPCTSTR)filename), "rb+");
  if (!mIIfile)
    return;
  mFilename = filename;
  mMode = mIIfile->mode;
  mMin = mIIfile->amin;
  mMax = mIIfile->amax;
  mWidth = mIIfile->nx;
  mHeight = mIIfile->ny;
  mDepth = mIIfile->nz;
  if (mIIfile->xscale != 1. || mIIfile->yscale != 1. || mIIfile->zscale != 1.) {
    mPixelSpacing = mIIfile->xscale;
    mHasPixelSpacing = 0;
  }
  mMeanSum = mIIfile->amean * mDepth;
  mNumWritten = mDepth;
  if (mIIfile->file == IIFILE_HDF) {
    mStoreType = STORE_TYPE_HDF;
    mAdocIndex = mIIfile->adocIndex;
    if (AdocGetMutexSetCurrent(mAdocIndex) >= 0) {
      if (!AdocGetString(ADOC_GLOBAL, 0, ADOC_PROG_VERS, &vers)) {
        mWrittenByVersion = ((CSerialEMApp *)AfxGetApp())->GetIntegerVersion(vers);
        free(vers);
      }
      AdocReleaseMutex();
    }
  }
}

// Open a new IMOD type file for writing a single TIFF, JPEG, or MRC image, or open HDF
KStoreIMOD::KStoreIMOD(CString inFilename, FileOptions inFileOpt)
  : KImageStore()
{
  int fileType = inFileOpt.useMont() ? inFileOpt.montFileType : inFileOpt.fileType;
  bool isJpeg = fileType == STORE_TYPE_JPEG;
  bool isHdf = fileType == STORE_TYPE_HDF;
  bool isMrc = fileType == STORE_TYPE_IIMRC;
  char *compEnvVar;
  int zipLevel = 5, varLevel;
  int iiType = IIFILE_TIFF;
  FILE *fp;
  CommonInit();
  mFileOpt = inFileOpt;
  mFilename = inFilename;
  if (fileType == STORE_TYPE_ADOC)
    fileType = STORE_TYPE_TIFF;
  if (fileType != STORE_TYPE_TIFF && !isJpeg && !isHdf && !isMrc)
    return;
  if ((mFileOpt.compression == COMPRESS_JPEG || isJpeg) && !isMrc)
    mFileOpt.mode = MRC_MODE_BYTE;
  if (isHdf)
    iiType = IIFILE_HDF;
  else if (isJpeg)
    iiType = IIFILE_JPEG;
  if (isMrc) {
    iiType = IIFILE_MRC;
    mStoreType = STORE_TYPE_IIMRC;
    fp = iiFOpen((LPCTSTR)inFilename, "wb");
    if (!fp)
      return;
    mIIfile = iiLookupFileFromFP(fp);
    if (!mIIfile)
      iiFClose(fp);
  } else {
    mIIfile = iiOpenNew(inFilename, isHdf ? "rw" : "w", iiType);
  }
  if (!mIIfile)
    return;

  mWrittenByVersion = ((CSerialEMApp *)AfxGetApp())->GetIntegerVersion();

  // Handle HDF, consult environment variable about compression level IF it is selected
  if (isHdf) {
    mStoreType = STORE_TYPE_HDF;
    mAdocIndex = mIIfile->adocIndex;
    mMontCoordsInMdoc = true;
    mIIfile->hdfCompression = 0;
    if (inFileOpt.hdfCompression == COMPRESS_ZIP) {
      compEnvVar = getenv("IMOD_HDF_COMPRESSION");
      if (compEnvVar) {
        varLevel = atoi(compEnvVar);
        if (varLevel > 0)
          zipLevel = varLevel;
      }
      mIIfile->hdfCompression = zipLevel;
    }
  }
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

BOOL KStoreIMOD::FileOK()
{
  return (mIIfile != NULL);
}

bool KStoreIMOD::fileIsShrMem()
{
  return mIIfile && mIIfile->file == IIFILE_SHR_MEM;
}

KImage  *KStoreIMOD::getRect(void)
{
  KImage *retVal;
  KImageShort *theSImage;
  KImageFloat *theFImage;
  KImageRGB *theCImage;
  char *theData;
  int i;
  mPixSize = lookupPixSize(mMode);
  size_t secSize = (size_t)mWidth * mHeight * mPixSize;
  
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
  if (mAdocIndex >= 0) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return retVal;
    KStoreADOC::LoadExtraFromValues(retVal, i, ADOC_ZVALUE, mCur.z);
    AdocReleaseMutex();
  }
  return retVal;
}

int KStoreIMOD::CheckMontage(MontParam * inParam)
{
  if (mAdocIndex < 0)
    return 0;
  if (!CheckAdocForMontage(inParam))
    return 0;
  return KImageStore::CheckMontage(inParam, mWidth, mHeight, mDepth);
}

int KStoreIMOD::getPcoord(int inSect, int & outX, int & outY, int & outZ, bool gotMutex)
{
  if (mAdocIndex < 0)
    return -1;
  return GetPCoordFromAdoc(ADOC_ZVALUE, inSect, outX, outY, outZ, gotMutex);
}

int KStoreIMOD::getStageCoord(int inSect, double & outX, double & outY)
{
  if (mAdocIndex < 0)
    return -1;
  return GetStageCoordFromAdoc(ADOC_ZVALUE, inSect, outX, outY);
}

int KStoreIMOD::ReorderPieceZCoords(int * sectOrder)
{
  return ReorderZCoordsInAdoc(ADOC_ZVALUE, sectOrder, mIIfile->nz);
}

void KStoreIMOD::Close(void)
{
  CString str;
  if (mIIfile && mStoreType == STORE_TYPE_HDF) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0) {
      SEMMessageBox("Failed to get mutex for writing header of HDF file");
    } else {
      if (iiWriteHeader(mIIfile)) {
        str.Format("Error writing header of HDF file:\n%s", b3dGetError());
        SEMMessageBox(str);
      }
      AdocReleaseMutex();
    }
  }
  if (mIIfile && mStoreType == STORE_TYPE_IIMRC) {
    if (mIIfile->file == IIFILE_SHR_MEM && iiWriteHeader(mIIfile)) {
      str.Format("Error writing header to shared memory file:\n%s", b3dGetError());
      SEMMessageBox(str);
    }
    if (mIIfile->file == IIFILE_MRC && mrc_head_write(mIIfile->fp, 
      (MrcHeader *)mIIfile->header)) {
      str.Format("Error writing header of MRC file:\n%s", b3dGetError());
      SEMMessageBox(str);
    }
  }

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

// Write a section to a TIFF or JPEG file or MRC file
int KStoreIMOD::WriteSection(KImage * inImage)
{
  char *idata;
  bool needToReflip, needToDelete;
  double theMean;
  int retval = 0;
  int resolution = 0;
  bool isJpeg = mFileOpt.fileType == STORE_TYPE_JPEG;
  bool isMrc = mFileOpt.fileType == STORE_TYPE_IIMRC;
  if (inImage == NULL)
    return 1;
  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();
  MrcHeader *header = (MrcHeader *)mIIfile->header;
  if (mIIfile == NULL)
    return 2;
  if (mFileOpt.fileType != STORE_TYPE_TIFF && !isJpeg && !isMrc)
    return 10;
  mIIfile->nx = mWidth = inImage->getWidth();
  mIIfile->ny = mHeight = inImage->getHeight();
  if (inImage->getType() == MRC_MODE_FLOAT) {
    mMode = MRC_MODE_FLOAT;
    if (isJpeg)
      return 20;
  } else
    mMode = inImage->getMode() == kGray ? mFileOpt.mode : MRC_MODE_RGB;
  mPixSize = lookupPixSize(mMode);
  if (!isMrc) 
    mIIfile->file = isJpeg ? IIFILE_JPEG : IIFILE_TIFF;
  int dataSize = mWidth * mHeight * mPixSize;

  if (AssignIIfileTypeAndFormat())
    return 11;

  inImage->Lock();
  idata   = convertForWriting(inImage, isMrc, needToReflip, needToDelete);
  if (!idata) {
    inImage->UnLock();
    return 4;
  }
  if (extra && extra->mPixel > 0)
    resolution = B3DNINT((isJpeg ? 2.54e8 : 1.e8) / extra->mPixel);
  if (isJpeg) {
    mIIfile->mode = mMode;
    if (jpegWriteSection(mIIfile, idata, 1, resolution < 65536 ? resolution : 0,
      mFileOpt.jpegQuality))
      retval = 6;
  } else {
    minMaxMean(idata, mIIfile->amin, mIIfile->amax, theMean);
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

    if (isMrc) {
      header->mode = mMode;
      header->nx = header->mx = mIIfile->nx;
      header->ny = header->my = mIIfile->ny;
      header->amin = mIIfile->amin;
      header->amax = mIIfile->amax;
      header->amean = (float)theMean;
      if (mrc_write_slice(idata, header->fp, header, mIIfile->nz, 'Z')) {
        retval = 6;
        CString str = b3dGetError();
        AfxMessageBox(str, MB_EXCLAME);
      } else {
        if (mIIfile->file == IIFILE_MRC)
          mIIfile->nz++;
        header->nz = header->mz = mIIfile->nz;
        if (extra && extra->mPixel > 0)
          mrc_set_scale(header, extra->mPixel, extra->mPixel, extra->mPixel);
        else if (mHasPixelSpacing)
          mrc_set_scale(header, mPixelSpacing, mPixelSpacing, mPixelSpacing);
      }
    } else {

      // Write the data with given compression; 1 means it is already inverted
      if (tiffWriteSection(mIIfile, idata, mFileOpt.compression, 1, resolution,
        mFileOpt.compression == COMPRESS_JPEG ? mFileOpt.jpegQuality : -1))
        retval = 6;
    }
  }
  if (needToReflip) {
    inImage->flipY();
    inImage->setNeedToReflip(false);
  }
  if (needToDelete)
    delete [] idata;
  inImage->UnLock();
  return retval;
}

int KStoreIMOD::AssignIIfileTypeAndFormat()
{
  mIIfile->format = IIFORMAT_LUMINANCE;
  switch (mMode) {

  case MRC_MODE_BYTE:
    mIIfile->type = IITYPE_UBYTE;
    break;
  case MRC_MODE_SHORT:
    mIIfile->type = IITYPE_SHORT;
    break;
  case MRC_MODE_USHORT:
    mIIfile->type = IITYPE_USHORT;
    break;
  case MRC_MODE_RGB:
    mIIfile->type = IITYPE_UBYTE;
    mIIfile->format = IIFORMAT_RGB;
    break;
  case MRC_MODE_FLOAT:
    mIIfile->type = IITYPE_FLOAT;
    break;
  default:
    return 11;
  }
  return 0;
}

CString KStoreIMOD::getFilePath()
{
  if ((mStoreType == STORE_TYPE_HDF || mStoreType == STORE_TYPE_IMOD) && mIIfile)
    return mIIfile->filename;
  return KImageStore::getFilePath();
}

void KStoreIMOD::UpdateHdfMrcHeader()
{
  MrcHeader *hdata = (MrcHeader *)mIIfile->header;
  int nlablSave = hdata->nlabl;
  iiSimpleFillMrcHeader(mIIfile, (MrcHeader *)mIIfile->header);
  hdata->nlabl = nlablSave;
}

int KStoreIMOD::ReorderHdfStackZvalues(int *sectOrder)
{
  int err;
  if (!mIIfile || mStoreType != STORE_TYPE_HDF)
    return 2;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 3;
  err = iiReorderHDFstack(mIIfile, sectOrder);
  AdocReleaseMutex();
  return err;
}

// Append an image to an HDF file
int KStoreIMOD::AppendImage(KImage *inImage)
{
  int err;
  if (inImage == NULL)
    return 1;

  // If this is a new image init everything.
  if ((mIIfile->nx == 0) && (mIIfile->ny == 0) && (mIIfile->nz == 0)) {
    inImage->getSize(mWidth, mHeight);

    // If it is RGB or float, fix the mode; otherwise it sets from file opt
    if (inImage->getType() == kRGB) {
      mMode = MRC_MODE_RGB;
    } else if (inImage->getType() == kFLOAT) {
      mMode = MRC_MODE_FLOAT;
    } else {
      mMode = inImage->getMode() == kGray ? mFileOpt.mode : MRC_MODE_RGB;
    }
    mIIfile->mode = mMode;
    if (AssignIIfileTypeAndFormat())
      return 11;

    // Should not be needed but just in case...
    mPixSize = lookupPixSize(mMode);
    mIIfile->xscale = mIIfile->yscale = mIIfile->zscale = mPixelSpacing;
    mIIfile->nx = mWidth;
    mIIfile->ny = mHeight;
    mIIfile->urx = mWidth - 1;
    mIIfile->ury = mHeight - 1;
    UpdateHdfMrcHeader();

    err = InitializeAdocGlobalItems(true, mMontCoordsInMdoc, mIIfile->filename);
    if (err)
      return err;

  }
  mCur.z = mIIfile->nz;


  // Write image data to end of file.
  err = WriteSection(inImage, mIIfile->nz);
  mDepth = mIIfile->nz;
  return err;
}

// Write a section to an HDF file
int KStoreIMOD::WriteSection(KImage *inImage, int inSect)
{
  double wallStart, updateTime;
  char *idata;
  int retval = 0;
  bool needToReflip, needToDelete, needNewSect, gotMutex;
  if (inImage == NULL)
    return 1;
  if (!mIIfile || mIIfile->state != IISTATE_READY)
    return 2;

  if (mWidth != inImage->getWidth() || mHeight != inImage->getHeight())
    return 17;

  inImage->Lock();

  idata = convertForWriting(inImage, true, needToReflip, needToDelete);
  if (!idata) {
    inImage->UnLock();
    return 4;
  }

  if (iiWriteSection(mIIfile, idata, inSect)) {
    retval = 6;
  } else {
    needNewSect = CheckNewSectionManageMMM(inImage, inSect, idata, mIIfile->nz,
      mIIfile->amin, mIIfile->amax, mIIfile->amean);
    retval = AddExtraValuesToAdoc(inImage, inSect, needNewSect, gotMutex);
  }
  if (!retval) {
    UpdateHdfMrcHeader();

    // See if header updaing is enabled at all, and if it time to do the next one
    if (mUpdateTimePerSect > 0) {
      mUpdateCounter--;
      if (mUpdateCounter <= 0) {
        mUpdateCounter = 1000000;
        wallStart = wallTime();
        if (iiWriteHeader(mIIfile)) {
          retval = 7;
        } else {
          iiClose(mIIfile);
          if (iiReopen(mIIfile))
            retval = 7;
          else {

            // Set interval to next update based on measured time
            updateTime = wallTime() - wallStart;
            mUpdateCounter = 1;
            if (updateTime >= mUpdateTimePerSect)
              mUpdateCounter = (int)ceil(updateTime / mUpdateTimePerSect);
            SEMTrace('y', "HDF header update time %.3f, counter to next %d", updateTime,
              mUpdateCounter);
          }
        }
      }
    }
  }

  if (needToReflip) {
    inImage->flipY();
    inImage->setNeedToReflip(false);
  }
  if (needToDelete)
    delete[] idata;
  inImage->UnLock();
  if (gotMutex)
    AdocReleaseMutex();
  return retval;
}

int KStoreIMOD::AddTitle(const char *inTitle)
{
  MrcHeader *hdata = (MrcHeader *)mIIfile->header;
  if (KImageStore::AddTitle(inTitle))
    return 1;
  if (mStoreType == STORE_TYPE_HDF || mStoreType == STORE_TYPE_IIMRC) {
    if (hdata->nlabl >= 10)
      return 1;
    AddTitleToLabelArray(&hdata->labels[hdata->nlabl][0], hdata->nlabl, inTitle);
  }
  return 0;
}

int KStoreIMOD::setMode(int inMode)
{
  if (mDepth > 0)
    return 1;
  mMode = inMode;
  return 0;
}


