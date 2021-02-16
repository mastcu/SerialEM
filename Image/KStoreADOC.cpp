#include "stdafx.h"
#include ".\KStoreAdoc.h"
#include "KStoreIMOD.h"
#include "..\SerialEM.h"
#include "..\Shared\autodoc.h"
#include "..\Shared\iimage.h"
#include "..\EMimageExtra.h"

#define MDOC_FLOAT(nam, ini, tst, sym, str) const char *sym = str;
#define MDOC_INTEGER(nam, ini, sym, str) const char *sym = str;
#define MDOC_TWO_FLOATS(nam1, nam2, ini, tst, sym, str) const char *sym = str;
#define MDOC_STRING(nam, sym, str) const char *sym = str;

namespace AdocDefs {
#include "MdocDefines.h"
}
#undef MDOC_FLOAT
#undef MDOC_TWO_FLOATS
#undef MDOC_STRING
#undef MDOC_INTEGER

// Opens an existing ADOC type file
KStoreADOC::KStoreADOC(CString filename)
  : KImageStore()
{
  int value;
  mStoreType = STORE_TYPE_ADOC;
  mFilename = filename;
  mAdocIndex = -1;

  // Read the Adoc - make sure it is an image series again
  if (AdocAcquireMutex()) {
    mAdocIndex = AdocRead((char *)(LPCTSTR)filename);
    if (mAdocIndex >= 0 && AdocGetInteger(ADOC_GLOBAL, 0, ADOC_SERIES, &value) || 
      value <= 0) {
        AdocClear(mAdocIndex);
        mAdocIndex = -1;
    }

    // If there is depth, there must be a size and a mode
    if (mAdocIndex >= 0) {
      mDepth = AdocGetNumberOfSections(ADOC_IMAGE);
      if (mDepth > 0 && (AdocGetTwoIntegers(ADOC_GLOBAL, 0, ADOC_SIZE, &mWidth, &mHeight) 
        || AdocGetInteger(ADOC_GLOBAL, 0, ADOC_MODE, &mMode))) {
          AdocClear(mAdocIndex);
          mAdocIndex = -1;
      }
    }
    AdocReleaseMutex();
  }
  mFileOpt.mode = mMode;
  mFileOpt.fileType = STORE_TYPE_TIFF;
}

// Open a new ADOC type file with the given options
KStoreADOC::KStoreADOC(CString inFilename , FileOptions inFileOpt)
  : KImageStore()
{
  CFileStatus status;
  CFile *cFile = NULL;
  mStoreType = STORE_TYPE_ADOC;
  mFilename = inFilename;
  mFileOpt = inFileOpt;

  // Need to get and save the full path of the file, first see if it exists
  try {
    if (CFile::GetStatus((LPCTSTR)inFilename, status)) {
      cFile = new CFile(inFilename, CFile::modeRead |CFile::shareDenyWrite);
      mFilename = cFile->GetFilePath();
      cFile->Close();
    } else {

      // Just to get the path!  Create and remove the file.
      cFile = new CFile((LPCTSTR)inFilename, CFile::modeCreate | 
        CFile::modeWrite |CFile::shareDenyWrite);
      mFilename = cFile->GetFilePath();
      cFile->Close();
      CFile::Remove((LPCTSTR)inFilename);
    }
  }
  catch(CFileException *perr) {
    perr->Delete();
  }
  if (cFile)
    delete cFile;

  mMode = mFileOpt.compression == COMPRESS_JPEG ? MRC_MODE_BYTE : mFileOpt.mode;
  mFileOpt.fileType = STORE_TYPE_TIFF;
  mMontCoordsInMdoc = (mFileOpt.typext & MONTAGE_MASK) != 0;
  if (!AdocAcquireMutex())
    return;
  mAdocIndex = AdocNew();
  if (mAdocIndex >= 0)
    AdocSetInteger(ADOC_GLOBAL, 0, ADOC_SERIES, 1);
  AdocReleaseMutex();
}

KStoreADOC::~KStoreADOC(void)
{
  Close();
}

// Test whether a file is an image series autodoc and returns the full path if so
CString KStoreADOC::IsADOC(CString filename)
{
  CString str;
  int index;
  CStdioFile *cFile = NULL;
  CString retval = "";
  try {
    cFile = new CStdioFile(filename, CFile::modeRead |CFile::shareDenyWrite);
    if (cFile->ReadString(str)) {
      str = str.Trim();
      if (str.Left((int)strlen(ADOC_SERIES)) == ADOC_SERIES) {
        index = str.Find('=', 11);
        if (index > 0 || index < str.GetLength() - 1) {
          str = str.Right(str.GetLength() - index - 1);
          if (atoi((LPCTSTR)str) > 0)
            retval = cFile->GetFilePath();
        }
      }
    }
  } 
  catch(CFileException *perr) {
    perr->Delete();
  } 
  if (cFile) {
    cFile->Close();
    delete cFile;
  }
  return retval;
}

void KStoreADOC::AllDone()
{
  AdocDone();
}

int KStoreADOC::WriteAdoc()
{
  int retval = 0;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 1;
  if (AdocWrite((char *)(LPCTSTR)mFilename) < 0)
    retval = 1;
  AdocReleaseMutex();
  return retval;
}

int KStoreADOC::AddTitle(const char *inTitle)
{
  int retval = AddTitleToAdoc(inTitle);
  if (!retval)
    KImageStore::AddTitle(inTitle);
  return retval;
}

// Append one image to the collection: This must be the first call to write an image
int KStoreADOC::AppendImage(KImage *inImage)
{
  int err;
  if (inImage == NULL) 
    return 1;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 14;

  // If this is a new image initialize size
  if (!mDepth && !mWidth && !mHeight ) {
    inImage->getSize(mWidth, mHeight);
    err = InitializeAdocGlobalItems(false, mFileOpt.isMontage(), NULL);
    if (err)
      return err;
  }
  mCur.z = mDepth;
  AdocReleaseMutex();
    
  // Write image data to end of file.
  err = WriteSection(inImage, mDepth);
  if (AdocAcquireMutex()) {
    mDepth = AdocGetNumberOfSections(ADOC_IMAGE);
    AdocReleaseMutex();
  } else
    mDepth++;
  return err;
}

// Write a section to the collection
int KStoreADOC::WriteSection (KImage *inImage, int inSect)
{
  KStoreIMOD *store;
  CString filename = mFilename;
  CString fullname;
  int dirInd, extInd, err, appended = 0;

  if (inImage == NULL) 
    return 1;
  if (inSect > mDepth)
    return 12;

  // Get name of TIFF file
  dirInd = mFilename.ReverseFind('\\');
  extInd = mFilename.ReverseFind('.');
  if (extInd > dirInd && extInd > 0)
    filename = mFilename.Left(extInd);
  fullname.Format("%s%04d.tif", (LPCTSTR)filename, inSect);

  // Write the section
  store = new KStoreIMOD(fullname, mFileOpt);
  if (!store->FileOK())
    return 13;
  if (mNumTitles > 0)
    tiffAddDescription((LPCTSTR)mTitles);
  mNumTitles = 0;
  mTitles = "";
  err = store->WriteSection(inImage);
  if (err)
    return err;
  delete store;


  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 14;
  if (inSect == mDepth) {
    if (dirInd >= 0)
      fullname = fullname.Right(fullname.GetLength() - dirInd - 1);
    RELEASE_RETURN_ON_ERR(AdocAddSection(ADOC_IMAGE, (char *)(LPCTSTR)fullname) < 0, 15);
    mDepth++;
    appended = 1;
  }
  RELEASE_RETURN_ON_ERR(SetValuesFromExtra(inImage, ADOC_IMAGE, inSect), 8);

  if (inSect && appended)
    err = AdocAppendSection((char *)(LPCTSTR)mFilename);
  else
    err = AdocWrite((char *)(LPCTSTR)mFilename);
  AdocReleaseMutex();
  if (err < 0)
    return 16;
  return 0;
}

// Get an image at the current section setting, including extra data
KImage  *KStoreADOC::getRect(void)
{
  KStoreIMOD *store;
  char *filename;
  CString str;
  int typext, dirInd;
  KImage *retval;
  if (mCur.z < 0 || mCur.z >= mDepth)
    return NULL;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return NULL;
  RELEASE_RETURN_ON_ERR(AdocGetSectionName(ADOC_IMAGE, mCur.z, &filename), NULL);
  str = filename;
  free(filename);
  dirInd = mFilename.ReverseFind('\\');
  if (dirInd >= 0)
    str = mFilename.Left(dirInd + 1) + str;
  store = new KStoreIMOD(str);
  if (!store->FileOK())
    return NULL;

  // Release the mutex for the read and reacquire it after
  AdocReleaseMutex();
  retval = store->getRect();
  delete store;
  if (retval) {
    if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
      return retval;
    LoadExtraFromValues(retval, typext, ADOC_IMAGE, mCur.z);
    AdocReleaseMutex();
  }
  return retval;
}

// Checks whether the file is a montage and fills the parameter set; returns
//   0 if not a montage
//   1 if it is a montage
//   -1 if it is montage and piece list is corrupted
int KStoreADOC::CheckMontage(MontParam *inParam)
{
  if (!mWidth || !mHeight || !mDepth)
    return 0;
  if (!CheckAdocForMontage(inParam))
    return 0;
  return KImageStore::CheckMontage(inParam, mWidth, mHeight, mDepth);
}


// Get the piece coordinates for a section.  Return 0 if OK, -1 if section bad
int KStoreADOC::getPcoord(int inSect, int &outX, int &outY, int &outZ)
{
  return GetPCoordFromAdoc(ADOC_IMAGE, inSect, outX, outY, outZ);
}

// Get the stage coordinates for a section
int KStoreADOC::getStageCoord(int inSect, double &outX, double &outY)
{
  return GetStageCoordFromAdoc(ADOC_IMAGE, inSect, outX, outY);
}

int KStoreADOC::ReorderPieceZCoords(int * sectOrder)
{
  return ReorderZCoordsInAdoc(ADOC_IMAGE, sectOrder, mDepth);
}

#define MDOC_FLOAT(nam, ini, tst, sym, str) \
  if (extra->nam > tst && AdocSetFloat(sectName, index, sym, extra->nam)) return 1;
#define MDOC_INTEGER(nam, ini, sym, str) \
  if (extra->nam > ini && AdocSetInteger(sectName, index, sym, extra->nam)) return 1;
#define MDOC_TWO_FLOATS(nam1, nam2, ini, tst, sym, str) \
  if (extra->nam1 > tst && AdocSetTwoFloats(sectName, index, sym, extra->nam1, extra->nam2)) return 1;
#define MDOC_STRING(nam, sym, str) \
  if (!extra->nam.IsEmpty() && AdocSetKeyValue(sectName, index, sym, (LPCTSTR)extra->nam)) return 1;

// Write out the values that have been placed in the image extra structure
// to the given section of the current autodoc
// Assumes current adoc is set and mutex is claimed
int KStoreADOC::SetValuesFromExtra(KImage *inImage, char *sectName, int index)
{
  if (!inImage || !inImage->GetUserData())
    return 0;
  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();

  // BE SURE TO ADD NEW VALUES TO THE LOAD FUNCTION SO THEY AREN'T LOST IN BIDIR SERIES
  if (extra->m_iMontageX >= 0) {
    if (AdocSetThreeIntegers(sectName, index, ADOC_PCOORD, 
      extra->m_iMontageX, extra->m_iMontageY, extra->m_iMontageZ))
      return 1;
    if (extra->mSuperMontX >= 0 && AdocSetTwoIntegers(sectName, index, ADOC_SUPER,
      extra->mSuperMontX, extra->mSuperMontY))
      return 1;
  }

  if (extra->mMax > EXTRA_VALUE_TEST && AdocSetThreeFloats(sectName, index,
    ADOC_MINMAXMEAN, extra->mMin, extra->mMax, extra->mMean))
    return 1;

#include "MdocDefines.h"

  return 0;
}

#undef MDOC_FLOAT
#undef MDOC_TWO_FLOATS
#undef MDOC_STRING
#undef MDOC_INTEGER

// Get values from the Adoc and populate an extra structure, creating one if needed
// typext is returned with the masks for the values gotten
// Assumes current adoc is set and mutex is claimed
int KStoreADOC::LoadExtraFromValues(KImage *inImage, int &typext, char *sectName, 
                                    int index)
{
  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();
  typext = 0;
  if (!AdocGetNumberOfKeys(sectName, index))
    return 0;
  if (!extra) {
    extra = new EMimageExtra;
    if (!extra)
      return 1;
    inImage->SetUserData(extra);
  }
  KStoreADOC::LoadExtraFromValues(extra, typext, sectName, index);
  return 0;
}

#define MDOC_FLOAT(nam, ini, tst, sym, str) \
  AdocGetFloat(sectName, index, sym, &extra->nam);
#define MDOC_INTEGER(nam, ini, sym, str) \
  AdocGetInteger(sectName, index, sym, &extra->nam);
#define MDOC_TWO_FLOATS(nam1, nam2, ini, tst, sym, str) \
  AdocGetTwoFloats(sectName, index, sym, &extra->nam1, &extra->nam2);
#define MDOC_STRING(nam, sym, str) \
  if (AdocGetString(sectName, index, sym, &frameDir) == 0) { \
    extra->nam = frameDir;    \
    free(frameDir);                     \
  }

int KStoreADOC::LoadExtraFromValues(EMimageExtra *extra, int &typext, char *sectName, 
                                    int index)
{
  char *frameDir;
  if (!AdocGetFloat(sectName, index, ADOC_TILT, &extra->m_fTilt))
    typext |= TILT_MASK;
  if (!AdocGetThreeIntegers(sectName, index, ADOC_PCOORD, &extra->m_iMontageX, 
    &extra->m_iMontageY, &extra->m_iMontageZ))
    typext |= MONTAGE_MASK;
  if (!AdocGetTwoFloats(sectName, index, ADOC_STAGE, &extra->mStageX, &extra->mStageY))
    typext |= VOLT_XY_MASK;
  if (!AdocGetFloat(sectName, index, ADOC_INTENSITY, &extra->m_fIntensity))
    typext |= INTENSITY_MASK;
  if (!AdocGetInteger(sectName, index, ADOC_MAG, &extra->m_iMag))
    typext |= MAG100_MASK;
  if (!AdocGetFloat(sectName, index, ADOC_DOSE, &extra->m_fDose))
    typext |= DOSE_MASK;
  AdocGetThreeFloats(sectName, index, ADOC_MINMAXMEAN, &extra->mMin, &extra->mMax,
    &extra->mMean);

#include "MdocDefines.h"
  
  return 0;
}
#undef MDOC_FLOAT
#undef MDOC_TWO_FLOATS
#undef MDOC_STRING
#undef MDOC_INTEGER
