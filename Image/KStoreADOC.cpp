#include "stdafx.h"
#include ".\KStoreAdoc.h"
#include "KStoreIMOD.h"
#include "..\SerialEM.h"
#include "..\Shared\autodoc.h"
#include "..\Shared\iimage.h"
#include "..\EMimageExtra.h"


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

  mMode = mFileOpt.mode;
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
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 1;
  KImageStore::AddTitle(inTitle);
  int retval = AdocAddSection(ADOC_TITLE, inTitle);
  AdocReleaseMutex();
  return retval;
}
void KStoreADOC::SetPixelSpacing(float pixel)
{
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return;
  AdocSetFloat(ADOC_GLOBAL, 0, ADOC_PIXEL, pixel);
  AdocReleaseMutex();
}

// Append one image to the collection: This must be the first call to write an image
int KStoreADOC::AppendImage(KImage *inImage)
{
  if (inImage == NULL) 
    return 1;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 14;

  // If this is a new image initialize size
  if (!mDepth && !mWidth && !mHeight ){
    inImage->getSize(mWidth, mHeight);
    RELEASE_RETURN_ON_ERR(AdocSetTwoIntegers(ADOC_GLOBAL, 0, ADOC_SIZE, mWidth, mHeight),
      15);
    if (mFileOpt.isMontage()) {
      RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, 1), 15);
    }
    RELEASE_RETURN_ON_ERR(AdocSetInteger(ADOC_GLOBAL, 0, ADOC_MODE, mMode), 15);
  }
  mCur.z = mDepth;
  AdocReleaseMutex();
    
  // Write image data to end of file.
  int err = WriteSection(inImage, mDepth);
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
  int ifmont;
  if (!mWidth || !mHeight || !mDepth)
    return 0;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return 0;
  if (AdocGetInteger(ADOC_GLOBAL, 0, ADOC_ISMONT, &ifmont) || ifmont <= 0)
    ifmont = 0;
  else
    ifmont = KImageStore::CheckMontage(inParam, mWidth, mHeight, mDepth);
  AdocReleaseMutex();
  return ifmont;
}


// Get the piece coordinates for a section.  Return 0 if OK, -1 if section bad
int KStoreADOC::getPcoord(int inSect, int &outX, int &outY, int &outZ)
{
  int retval = 0;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return -1;
  if (AdocGetThreeIntegers(ADOC_IMAGE, inSect, ADOC_PCOORD, &outX, &outY, &outZ))
    retval = -1;
  AdocReleaseMutex();
  return retval;
}

// Get the stage coordinates for a section
int KStoreADOC::getStageCoord(int inSect, double &outX, double &outY)
{
  float X, Y;
  int retval = 0;
  if (AdocGetMutexSetCurrent(mAdocIndex) < 0)
    return -1;
  if (AdocGetTwoFloats(ADOC_IMAGE, inSect, ADOC_STAGE, &X, &Y)) {
    retval = -1;
  } else {
    outX = X;
    outY = Y;
  }
  AdocReleaseMutex();
  return retval;
}

// Write out the values that have been placed in the image extra structure
// to the given section of the current autodoc
// Assumes current adoc is set and mutex is claimed
int KStoreADOC::SetValuesFromExtra(KImage *inImage, char *sectName, int index)
{
  if (!inImage || !inImage->GetUserData())
    return 0;
  EMimageExtra *extra = (EMimageExtra *)inImage->GetUserData();

  // BE SURE TO ADD NEW VALUES TO THE LOAD FUNCTION SO THEY AREN'T LOST IN BIDIR SERIES
  if (extra->m_fTilt > EXTRA_VALUE_TEST && 
    AdocSetFloat(sectName, index, ADOC_TILT, extra->m_fTilt))
    return 1;
  if (extra->m_iMontageX >= 0) {
    if (AdocSetThreeIntegers(sectName, index, ADOC_PCOORD, 
      extra->m_iMontageX, extra->m_iMontageY, extra->m_iMontageZ))
      return 1;
    if (extra->mSuperMontX >= 0 && AdocSetTwoIntegers(sectName, index, ADOC_SUPER,
      extra->mSuperMontX, extra->mSuperMontY))
      return 1;
  }
  if (extra->mStageX > EXTRA_VALUE_TEST && AdocSetTwoFloats(sectName, index, ADOC_STAGE, 
    extra->mStageX, extra->mStageY))
    return 1;
  if (extra->mStageZ > EXTRA_VALUE_TEST && AdocSetFloat(sectName, index, ADOC_STAGEZ, 
    extra->mStageZ))
    return 1;
  if (extra->m_iMag > 0 && AdocSetInteger(sectName, index, ADOC_MAG, extra->m_iMag))
    return 1;
  if (extra->m_fIntensity >= 0. &&
    AdocSetFloat(sectName, index, ADOC_INTENSITY, extra->m_fIntensity))
    return 1;
  if (extra->m_fDose >= 0. && AdocSetFloat(sectName, index, ADOC_DOSE, extra->m_fDose))
    return 1;
  if (extra->mPixel > 0. && AdocSetFloat(sectName, index, ADOC_PIXEL, extra->mPixel))
    return 1;
  if (extra->mSpotSize > 0 && 
    AdocSetInteger(sectName, index, ADOC_SPOT, extra->mSpotSize))
    return 1;
  if (extra->mDefocus > EXTRA_VALUE_TEST &&
    AdocSetFloat(sectName, index, ADOC_DEFOCUS, extra->mDefocus))
    return 1;
  if (extra->mISX > EXTRA_VALUE_TEST && 
    AdocSetTwoFloats(sectName, index, ADOC_IMAGESHIFT, extra->mISX, extra->mISY))
    return 1;
  if (extra->mAxisAngle > EXTRA_VALUE_TEST &&
    AdocSetFloat(sectName, index, ADOC_AXIS, extra->mAxisAngle))
    return 1;
  if (extra->mExposure >= 0. && 
    AdocSetFloat(sectName, index, ADOC_EXPOSURE, extra->mExposure))
    return 1;
  if (extra->mBinning > 0. && 
    AdocSetFloat(sectName, index, ADOC_BINNING, extra->mBinning))
    return 1;
  if (extra->mCamera >= 0 && 
    AdocSetInteger(sectName, index, ADOC_CAMERA, extra->mCamera))
    return 1;
  if (extra->mDividedBy2 >= 0 && 
    AdocSetInteger(sectName, index, ADOC_DIVBY2, extra->mDividedBy2))
    return 1;
  if (extra->mMagIndex >= 0 && 
    AdocSetInteger(sectName, index, ADOC_MAGIND, extra->mMagIndex))
    return 1;
  if (extra->mCountsPerElectron >= 0. && AdocSetFloat(sectName, index, ADOC_COUNT_ELEC, 
    extra->mCountsPerElectron))
    return 1;
  if (extra->mMax > EXTRA_VALUE_TEST && AdocSetThreeFloats(sectName, index,
    ADOC_MINMAXMEAN, extra->mMin, extra->mMax, extra->mMean))
    return 1;
  if (extra->mTargetDefocus > EXTRA_VALUE_TEST &&
    AdocSetFloat(sectName, index, ADOC_TARGET, extra->mTargetDefocus))
    return 1;
  if (extra->mPriorRecordDose >= 0. && AdocSetFloat(sectName, index, ADOC_PRIOR_DOSE,
    extra->mPriorRecordDose))
    return 1;
  if (!extra->mSubFramePath.IsEmpty() && AdocSetKeyValue(sectName, index, ADOC_FRAME_PATH, 
    (LPCTSTR)extra->mSubFramePath))
    return 1;
  if (extra->mNumSubFrames > 0 && 
    AdocSetInteger(sectName, index, ADOC_NUM_FRAMES, extra->mNumSubFrames))
    return 1;
  if (!extra->mFrameDosesCounts.IsEmpty() && AdocSetKeyValue(sectName, index, 
    ADOC_DOSES_COUNTS, (LPCTSTR)extra->mFrameDosesCounts))
    return 1;
  if (!extra->mDateTime.IsEmpty() && AdocSetKeyValue(sectName, index, ADOC_DATE_TIME, 
    (LPCTSTR)extra->mDateTime))
    return 1;
  if (!extra->mNavLabel.IsEmpty() && AdocSetKeyValue(sectName, index, ADOC_NAV_LABEL, 
    (LPCTSTR)extra->mNavLabel))
    return 1;
  if (!extra->mChannelName.IsEmpty() && AdocSetKeyValue(sectName, index, ADOC_CHAN_NAME, 
    (LPCTSTR)extra->mChannelName))
    return 1;
  if (!extra->mDE12Version.IsEmpty() && AdocSetKeyValue(sectName, index, 
    ADOC_DE12_VERSION, (char *)(LPCTSTR)extra->mDE12Version))
    return 1;
  if (extra->mPreExposeTime > -1. &&
    AdocSetFloat(sectName, index, ADOC_DE12_PREEXPOSE, extra->mPreExposeTime))
    return 1;
  if (extra->mNumDE12Frames > -1 && 
    AdocSetInteger(sectName, index, ADOC_DE12_TOTAL_FRAMES, extra->mNumDE12Frames))
    return 1;
  if (extra->mDE12FPS > -1. &&
    AdocSetFloat(sectName, index, ADOC_DE12_FPS, extra->mDE12FPS))
    return 1;
  if (!extra->mDE12Position.IsEmpty() && AdocSetKeyValue(sectName, index,
    ADOC_CAM_POSITION, (char *)(LPCTSTR)extra->mDE12Position))
    return 1;
  if (!extra->mDE12CoverMode.IsEmpty() && AdocSetKeyValue(sectName, index, 
    ADOC_COVER_MODE, (char *)(LPCTSTR)extra->mDE12CoverMode))
    return 1;
  if (extra->mCoverDelay > -1 &&
    AdocSetInteger(sectName, index, ADOC_COVER_DELAY, extra->mCoverDelay))
    return 1;
  if (extra->mTemperature > EXTRA_VALUE_TEST &&
    AdocSetFloat(sectName, index, ADOC_TEMPERATURE, extra->mTemperature))
    return 1;
  if (extra->mFaraday > EXTRA_VALUE_TEST &&
    AdocSetFloat(sectName, index, ADOC_FARADAY, extra->mFaraday))
    return 1;
  if (!extra->mSensorSerial.IsEmpty() && AdocSetKeyValue(sectName, index, 
    ADOC_SENSOR_SERIAL, (char *)(LPCTSTR)extra->mSensorSerial))
    return 1;
  if (extra->mReadoutDelay > -1. &&
    AdocSetInteger(sectName, index, ADOC_READOUT_DELAY, extra->mReadoutDelay))
    return 1;
  if (extra->mIgnoredFrames > -1 && 
    AdocSetInteger(sectName, index, ADOC_IGNORED_FRAMES, extra->mIgnoredFrames))
    return 1;

  return 0;
}

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

#define ADOC_GET_STRING(a, b) \
  if (AdocGetString(sectName, index, a, &frameDir) == 0) { \
    extra->b = frameDir;    \
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
  AdocGetFloat(sectName, index, ADOC_STAGEZ, &extra->mStageZ);
  AdocGetFloat(sectName, index, ADOC_PIXEL, &extra->mPixel);
  AdocGetInteger(sectName, index, ADOC_SPOT, &extra->mSpotSize);
  AdocGetFloat(sectName, index, ADOC_DEFOCUS, &extra->mDefocus);
  AdocGetTwoFloats(sectName, index, ADOC_IMAGESHIFT, &extra->mISX, &extra->mISY);
  AdocGetFloat(sectName, index, ADOC_AXIS, &extra->mAxisAngle);
  AdocGetFloat(sectName, index, ADOC_EXPOSURE, &extra->mExposure);
  AdocGetFloat(sectName, index, ADOC_BINNING, &extra->mBinning);
  AdocGetInteger(sectName, index, ADOC_CAMERA, &extra->mCamera);
  AdocGetInteger(sectName, index, ADOC_DIVBY2, &extra->mDividedBy2);
  AdocGetInteger(sectName, index, ADOC_MAGIND, &extra->mMagIndex);
  AdocGetFloat(sectName, index, ADOC_COUNT_ELEC, &extra->mCountsPerElectron);
  AdocGetThreeFloats(sectName, index, ADOC_MINMAXMEAN, &extra->mMin, &extra->mMax,
    &extra->mMean);
  AdocGetFloat(sectName, index, ADOC_TARGET, &extra->mTargetDefocus);
  AdocGetFloat(sectName, index, ADOC_PRIOR_DOSE, &extra->mPriorRecordDose);
  ADOC_GET_STRING(ADOC_FRAME_PATH, mSubFramePath);
  AdocGetInteger(sectName, index, ADOC_NUM_FRAMES, &extra->mNumSubFrames);
  ADOC_GET_STRING(ADOC_DOSES_COUNTS, mFrameDosesCounts);
  ADOC_GET_STRING(ADOC_DATE_TIME, mDateTime);
  ADOC_GET_STRING(ADOC_NAV_LABEL, mNavLabel);
  ADOC_GET_STRING(ADOC_CHAN_NAME, mChannelName);
  
  //Skip most DE12 items
  AdocGetFloat(sectName, index, ADOC_DE12_PREEXPOSE, &extra->mPreExposeTime);
  AdocGetInteger(sectName, index, ADOC_DE12_TOTAL_FRAMES, &extra->mNumDE12Frames);
  AdocGetFloat(sectName, index, ADOC_DE12_FPS, &extra->mDE12FPS);
  AdocGetFloat(sectName, index, ADOC_FARADAY, &extra->mFaraday);
  extra->ValuesIntoShorts();
  return 0;
}
