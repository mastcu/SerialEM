// KStoreMRC.cpp:         A class for saving and reading MRC image files
// //
// Copyright (C) 2003-2026 by the Regents of the University of
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

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

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

  try {
    fileSize = (double)inFile->GetLength();
    if (fileSize < MRCheadSize)
      return false;

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
    if (fileSize < imageSize * nz + MRCheadSize) {
      if (nz < 3 || (nz < 10 && fileSize < imageSize * (nz - 1) + MRCheadSize) ||
        (nz < 20 && fileSize < imageSize * (nz - 2) + MRCheadSize) ||
        (nz >= 20 && fileSize < imageSize * (nz - nz / 10) + MRCheadSize))
        return false;
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
  BOOL makeMdoc = (inFileOpt.useMont() ? inFileOpt.montUseMdoc : inFileOpt.useMdoc) ||
    inFileOpt.montageInMdoc;
  mHead = new HeaderMRC();
  mExtra = NULL;
  mPixelSpacing = 1.;
  mMontCoordsInMdoc = false;
  mNumWritten = 0;
  mMeanSum = 0.;
  mWrittenByVersion = ((CSerialEMApp *)AfxGetApp())->GetIntegerVersion();

  mFileOpt = inFileOpt;

  // Figure out amount of extra header needed
  mHead->typext = (Int16)inFileOpt.typext;
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
    mFile = new CFile(inFilename, CFile::modeCreate | CFile::modeNoInherit |
       CFile::modeReadWrite |CFile::shareDenyWrite);

    Seek(0, CFile::begin);
    Write(mHead, headSize);
    if (mHead->next)
      Write(mExtra, mHead->next);

    // Back up an existing mdoc file regardless of whether a new one is requested
    // unless flag is set to leave one (for frame stack mdoc)
    // Then try to create a new autodoc if needed
    mdocName = getAdocName();
    if (makeMdoc || !inFileOpt.leaveExistingMdoc)
      imodBackupFile((char *)(LPCTSTR)mdocName);
    if (makeMdoc) {
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

  try{
    if (mFile == NULL)
      throw (-1);
    if (head == NULL)
      throw (-1);
    fileSize = (double)mFile->GetLength();
    if (fileSize <= (unsigned int)MRCheadSize)
      throw(-1);
    Seek(0, CFile::begin);
    Read(head, headSize);
    Int32     nx, ny, nz;//, nobs;
    nx = head->nx;
    ny = head->ny;
    nz = head->nz;
    mMode = head->mode;

    mMin = head->amin;
    mMax = head->amax;
    mHeadSize += head->next;
    mNumWritten = nz;
    mMeanSum = nz * head->amean;

    // Make sure files aren't byte-swapped
    if (nx <= 0 || ny <= 0 || nz <= 0  || (nx > 65535 &&  ny > 65535 && nz > 65535)
      || (mMode < 0) || (mMode > 16))
        throw(-1);

    mPixSize = lookupPixSize(mMode);

    double imageSize = nx * ny * mPixSize;

    if (fileSize < imageSize * nz + mHeadSize) {
      if (nz < 3 || (nz < 10 && fileSize < imageSize * (nz - 1) + mHeadSize) ||
        (nz < 20 && fileSize < imageSize * (nz - 2) + mHeadSize) ||
        (nz >= 20 && fileSize < imageSize * (nz - nz / 10) + mHeadSize))
        throw (-1);
      mDepth = (int)((fileSize + 0.5 - mHeadSize) / imageSize);
      mDamagedNz = nz;
      head->nz = mDepth;
    } else
      mDepth = nz;

    mWidth  = nx;
    mHeight = ny;

    if (head->next) {
      NewArray(mExtra, char, head->next);
      if (mExtra == NULL)
        throw(-1);
      Read(mExtra, head->next);
    }
    mHead = head;
    if (head->dx && head->mx > 0) {
      mPixelSpacing = head->dx / head->mx;
      mHasPixelSpacing = true;
    } else if (nz)
      mPixelSpacing = head->dz / nz;

    // Now see if there is an mdoc file
    mdocName = getAdocName();
    if (CFile::GetStatus((LPCTSTR)mdocName, status) && AdocAcquireMutex()) {
      mAdocIndex = AdocRead((char *)(LPCTSTR)mdocName);
      if (mAdocIndex < 0) {
        AdocReleaseMutex();
        throw(-1);
      }

      // Ignore Frame stack mdoc
      if (AdocGetNumberOfSections("FrameSet")) {
        AdocClear(mAdocIndex);
        mAdocIndex = -1;
      } else {

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

        // Get version
        if (!AdocGetString(ADOC_GLOBAL, 0, ADOC_PROG_VERS, &single)) {
          mWrittenByVersion = ((CSerialEMApp *)AfxGetApp())->GetIntegerVersion(single);
          free(single);
        }
      }
      AdocReleaseMutex();
    }
  }

  catch(CFileException *perr) {
    mPixSize = mWidth = mHeight = mDepth = 0;
    delete head;
    mHead = NULL;
    Close();
    perr->Delete();
    return;
  }
  catch( int ) {
    mPixSize = mWidth = mHeight = mDepth = 0;
    delete head;
    mHead = NULL;
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
int KStoreMRC::AddTitle(const char *inTitle)
{
  int retval = 0;
  if (mFile == NULL)
    return 1;
  if (mHead->nttl == 10)
    return -1;
  AddTitleToLabelArray(&mHead->labels[mHead->nttl][0], mHead->nttl, inTitle);

  // Add to mdoc
  return AddTitleToAdoc(inTitle);
}

const char *KStoreMRC::GetTitle(int index)
{
  if (index < 0 || index >= mHead->nttl)
    return NULL;
  return &mHead->labels[index][0];
};


int KStoreMRC::setMode(int inMode)
{
  if (mDepth > 0)
    return 1;
  mMode = inMode;
  return 0;
}

int KStoreMRC::AppendImage(KImage *inImage)
{
  int err;
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
    } else if (inImage->getType() == kFLOAT) {
      mMode = MRC_MODE_FLOAT;
      mHead->Init(mWidth, mHeight, 0, mMode);
    } else
      mHead->Init(mWidth, mHeight, 0, mFileOpt.mode);

    // Should not be needed but just in case...
    mPixSize = lookupPixSize(mMode);
    mHead->dx *= mPixelSpacing;
    mHead->dy *= mPixelSpacing;

    err = InitializeAdocGlobalItems(true, (mFileOpt.typext & MONTAGE_MASK)
      || mMontCoordsInMdoc, (char *)(LPCTSTR)mFile->GetFileName());
    if (err)
      return err;

  }
  mCur.z = mHead->nz;


  // Write image data to end of file.
  err = WriteSection(inImage, mHead->nz);
  mDepth = mHead->nz;
  return err;
}

// Write a section in the indicated place in the file
int KStoreMRC::WriteSection (KImage *inImage, int inSect)
{
  char *idata;
  int i, j;
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

    // Test for whether need new autodoc section before changing depth
    needNewSect = CheckNewSectionManageMMM(inImage, inSect, idata, mHead->nz,
      mHead->amin, mHead->amax, mHead->amean);

    mHead->mz = mHead->nz;
    mHead->dz = (float)mHead->nz * mPixelSpacing;

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

    retval = AddExtraValuesToAdoc(inImage, inSect, needNewSect, gotMutex);
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

// Rewrite the header to the file and the autodoc (e.g., after a label change)
int KStoreMRC::WriteHeader(bool adocAlso)
{
  int err;
  try {
    Seek(0, CFile::begin);
    Write(mHead, MRCheadSize);
  }
  catch (CFileException *perr) {
    perr->Delete();
    return 1;
  }
  if (!adocAlso || mAdocIndex < 0)
    return 0;
  if ((err = AdocGetMutexSetCurrent(mAdocIndex)) < 0)
    return err - 1;
  err = AdocWrite((char *)(LPCTSTR)getAdocName());
  AdocReleaseMutex();
  return err < 0 ? -1 : 0;
}

// Read a section from the file at the current z value
KImage *KStoreMRC::getRect(void)
{
  int width =  mWidth;
  int height = mHeight;
  char *theData;
  size_t secSize = (size_t)width * height * mPixSize;

  if (mCur.z < 0 || mCur.z >= mDepth || mFile == NULL)
    return NULL;
  //Int32 offset  = secSize * mCur.z;

  if ((mMode < 0 || mMode > 2) && mMode != MRC_MODE_USHORT && mMode != MRC_MODE_RGB)
    return NULL;
  NewArray(theData, char, secSize);
  if (!theData)
    return NULL;

  try{
    BigSeek(mHeadSize, width, height * mPixSize * mCur.z, CFile::begin);
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

    Read( theData , (DWORD)secSize);
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
  if (iiMRCcheckPCoord((MrcHeader *)mHead) == 0 && !mMontCoordsInMdoc)
    return 0;
  if (mMontCoordsInMdoc && !CheckAdocForMontage(inParam))
    return 0;
  return KImageStore::CheckMontage(inParam, mHead->nx, mHead->ny, mHead->nz);
}

// There are elusive reports of files marked as montage when they should not be; this is
// called on first save when montaging is not being done to detect and fix it
int KStoreMRC::FixInappropriateMontage()
{
  if ((mHead->typext & MONTAGE_MASK) || montCoordsInAdoc()) {
    mHead->typext &= ~MONTAGE_MASK;
    mMontCoordsInMdoc = false;
    return 1;
  }
  return 0;
}

// Get the piece coordinates for a section.  Return 0 if OK, -1 if section bad
int KStoreMRC::getPcoord(int inSect, int &outX, int &outY, int &outZ, bool gotMutex)
{
  char *edata;
  unsigned short int usTemp;
  short int sTemp;
  int retval = 0;
  int offset = inSect * mHead->nbytext;
  if (mMontCoordsInMdoc) {
    if (GetPCoordFromAdoc(ADOC_ZVALUE, inSect, outX, outY, outZ, gotMutex))
      return -1;
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
// This takes a a map from current section number to new section number
int KStoreMRC::ReorderPieceZCoords(int *sectOrder)
{
  short int sTemp;
  int ind;
  char *edata= (char *)&sTemp;
  char *idata = mExtra + 4;
  if (!mMontCoordsInMdoc) {

    // Extended header is supposed to have montage: make sure the flag is set
    if (!(mHead->typext & MONTAGE_MASK))
      return 1;
    if ((mHead->typext & TILT_MASK) != 0)
      idata +=   EMimageExtra::Bytes[0];

    // Loop through the extra data, fetch out a Z value, map it, put it back
    for (ind = 0; ind < mHead->nz; ind++) {
      edata[0] = *idata;
      edata[1] = *(idata + 1);
      sTemp = (short)sectOrder[sTemp];
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
  return ReorderZCoordsInAdoc(ADOC_ZVALUE, sectOrder, mHead->nz);
}

// Get the stage coordinates for a section
int KStoreMRC::getStageCoord(int inSect, double &outX, double &outY)
{
  EMimageExtra extra;
  if (getExtraData(&extra, inSect) || extra.mStageX == EXTRA_NO_VALUE ||
    extra.mStageY == EXTRA_NO_VALUE) {
    if (!mMontCoordsInMdoc || GetStageCoordFromAdoc(ADOC_ZVALUE, inSect, outX, outY))
      return -1;
  }
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
void KStoreMRC::Write(void *buf, DWORD count)
{
  DWORD nWritten;
  if (!WriteFile((HANDLE)mFile->m_hFile, buf, count, &nWritten, NULL))
		CFileException::ThrowOsError((LONG)::GetLastError());
}

void KStoreMRC::Read(void *buf, DWORD count)
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
  mHead->dx = (float)mHead->nx * pixel;
  mHead->dy = (float)mHead->ny * pixel;
  mHead->dz = (float)mHead->nz * pixel;
  KImageStore::SetPixelSpacing(pixel);
}

