// EMbufferManager.cpp   Manages buffer copying, saving, and reading from file
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
// Adapted for Tecnai, DNM 2/25/02

#include "stdafx.h"
#include <direct.h>
#include "Utilities\KGetOne.h"

#include ".\EMbufferManager.h"
#include "Image\KStoreIMOD.h"
#include "Image\KStoreADOC.h"
//#include "EMmontageWindow.h"
#include "EMmontageController.h"
#include "SerialEMDoc.h"
#include "SerialEMView.h"
#include "EMScope.h"
#include "MultiTSTasks.h"
#include "ShiftManager.h"
#include "NavigatorDlg.h"
#include "MacroProcessor.h"
#include "CameraController.h"
#include "TSController.h"
#include "Utilities\XCorr.h"
#include "Shared\iimage.h"


EMbufferManager::EMbufferManager(CString *inModeNamep, EMimageBuffer *inImBufs)
{
  SEMBuildTime(__DATE__, __TIME__);
  // Set initial radio button behavior
  mShiftsOnAcquire = 1; // Shift A to B, not B to C
  mCopyOnSave = 0;    // Save into C
  mBufToSave = 0;   // Save A buffer
  mBufToReadInto = 3;   // Read into C
  mFifthCopyToBuf = 4;  // Default is E
  mAlignToB = false;   // Special flag to align to B
  // Confirm destruction of different modes
  for (int i = 0; i < MAX_CONSETS; i++)
    mConfirmDestroy[i] = 0;
  mConfirmDestroy[3] = 1;
  mModeNames = inModeNamep;
  mAlignOnSave = 0;   // Do align on saves
  mAutoZoom = 1;    // Do autozoom
  mFixedZoomStep = 1; // Zoom by constant steps
  mDrawScaleBar = 1;  // Draw scale bars
  mAntialias = 1;     // Do antialias filter for zoom < 1
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mOtherFile = "";
  mImBufsp = inImBufs;
  mStackWinMaxXY = 512;
  mRotateAxisAngle = false;
  mDrawCrosshairs = false;
  mUnsignedTruncLimit = 0.01f;
  mSaveAsynchronously = true;
  mSavingThread = NULL;
  mDoingAsyncSave = false;
  mAsyncFromImage = false;
  mImageAsyncFailed = false;
  mNextSecToRead = NO_SUPPLIED_SECTION;
}

EMbufferManager::~EMbufferManager()
{
  iiDeleteCheckList();
  iiDeleteRawCheckList();
}

// The old copy routine for going from one regular buffer to another
int EMbufferManager::CopyImageBuffer(int inFrom, int inTo)
{
  if (inFrom == inTo)
    return 0;
  EMimageBuffer *fromBuf = mImBufsp + inFrom;
  EMimageBuffer *toBuf = mImBufsp + inTo;

  // Call the new routine with the ImBufs
  return CopyImBuf(fromBuf, toBuf);
}

// Copy from one ImBuf to another
int EMbufferManager::CopyImBuf(EMimageBuffer *fromBuf, EMimageBuffer *toBuf, 
                               BOOL display)
{
  if (fromBuf->mImage == NULL || fromBuf == toBuf) 
    return 1;

  int inTo = MainImBufIndex(toBuf);
  if (inTo >= 0 && !OKtoDestroy(inTo, "A buffer copy"))
    return 1;

  // Dismiss the pixmap image if the new image is a different size, dismiss the filtered
  // pixmap image regardless, then delete the destination image
  if (toBuf->mImage && toBuf->mPixMap && 
    (toBuf->mImage->getWidth() != fromBuf->mImage->getWidth() ||
    toBuf->mImage->getHeight() != fromBuf->mImage->getHeight()))
    toBuf->mPixMap->doneWithRect();
  if (toBuf->mFiltPixMap)
    toBuf->mFiltPixMap->doneWithRect();
  toBuf->DeleteImage();
  toBuf->DeleteOffsets();

  // Copy all member elements of fromBuf, but save and restore pixmaps and imagescale
  KImageScale *saveImageScale = toBuf->mImageScale;
  KPixMap *savePixMap = toBuf->mPixMap;
  KPixMap *saveFiltMap = toBuf->mFiltPixMap;
  *toBuf = *fromBuf;
  toBuf->mImageScale = saveImageScale;
  toBuf->mPixMap = savePixMap;
  toBuf->mFiltPixMap = saveFiltMap;

  // Make a new copy of the offsets if they exist
  if (fromBuf->mMiniOffsets) {
    toBuf->mMiniOffsets = new MiniOffsets;
    *(toBuf->mMiniOffsets) = *(fromBuf->mMiniOffsets);
  }

  int type = fromBuf->mImage->getType();
  // Make a new image structure that shares the image data
  // This will also make a fresh, unique copy of the extra data
  switch (type) {
    case kUBYTE:
      toBuf->mImage = new KImage(fromBuf->mImage);
      break;
    case kSHORT:
    case kUSHORT:
      toBuf->mImage = new KImageShort(fromBuf->mImage);
      break;
    case kFLOAT:
      toBuf->mImage = new KImageFloat(fromBuf->mImage);
      break;
   case kRGB:
      toBuf->mImage = new KImageRGB(fromBuf->mImage);
      break;
    default:
      return 1;
      break;
  }

  // Allocate a new space for extra data and copy it over
  /* EMimageExtra *fromExtra = (EMimageExtra *)fromBuf->mImage->GetUserData();
  if (fromExtra) {
    EMimageExtra *extra = new EMimageExtra;
    toBuf->mImage->SetUserData((char *) extra);
    *extra = *fromExtra; 
  } */

  // Now create a new KImageScale if needed, copy over unless something is being added to
  // the FFT stack from elsewhere
  if (fromBuf->mImageScale != NULL && (toBuf->mImageScale == NULL || 
    !(mWinApp->mFFTView && !mWinApp->mFFTView->IsBufferInStack(fromBuf) && 
    mWinApp->mFFTView->IsBufferInStack(toBuf)))) {
      if (toBuf->mImageScale == NULL)
        toBuf->mImageScale = new KImageScale;
      *(toBuf->mImageScale) = *(fromBuf->mImageScale);
  }

  toBuf->SetImageChanged(1);

  // Maintain the SaveCopy flag
  if (fromBuf->GetSaveCopyFlag())
    fromBuf->IncrementSaveCopy();

  //toBuf->mImage->setShifts(0, 0);

  int current = mWinApp->GetImBufIndex();

  // If the copy was into the current display, draw it regardless
  // Otherwise draw a new window if that is the target
  if (current == inTo && display)
    mWinApp->mMainView->SetCurrentBuffer(inTo);
  else if (inTo < 0 && display)
    mWinApp->mActiveView->DrawImage();

  // need to update status window
  mWinApp->UpdateBufferWindows();

  // Have active window tell imagelevel window what happened unless it is the stack view
  // We don't want stack view to keep putting 0 - 255 out in the levels
  if (mWinApp->mActiveView != mWinApp->mStackView)
    mWinApp->mActiveView->NewImageScale();

  return 0;
}


// Returns pointer to the buffer to save, either selected one
// or the one being displayed, or A if montaging
EMimageBuffer *EMbufferManager::GetSaveBuffer()
{
  if (mWinApp->Montaging() && !mWinApp->SavingOther())
    return mImBufsp;
  if (mBufToSave >= 0)
    return ( mImBufsp + mBufToSave );
  return (mWinApp->mActiveView->GetActiveImBuf());
}

// Finds out if buffer is savable; i.e. there must be an image, and
// either there is no open file, or there is an open file and its dimensions
// match the image dimensions and we are not montaging
BOOL EMbufferManager::IsBufferSavable(EMimageBuffer *toBuf)
{
  KImageStore *inStoreMRC = mWinApp->mStoreMRC;
  if (toBuf->mImage == NULL)
    return false;
  if (inStoreMRC == NULL)
    return true;
  if (mWinApp->Montaging())
    return false;
  return ((inStoreMRC->getWidth() == 0 && toBuf->mImage->getType() != kFLOAT) || 
    (toBuf->mImage->getWidth() == inStoreMRC->getWidth() &&
    toBuf->mImage->getHeight() == inStoreMRC->getHeight() &&
    ((toBuf->mImage->getMode() == kGray && inStoreMRC->getMode() != MRC_MODE_RGB) ||
    (toBuf->mImage->getMode() == kRGBmode && inStoreMRC->getMode() == MRC_MODE_RGB)) &&
    ((toBuf->mImage->getType() == kFLOAT ? 1 : 0) == 
    (inStoreMRC->getMode() == MRC_MODE_FLOAT ? 1 : 0))));
}

// Finds out if buffer is OK to destroy: if it has no image, or save flag is not 1,
// or no confirmation needed to destroy, or we're montaging or calibrating,
// it's fine.  Otherwise do confirmation with the text passed in.
BOOL EMbufferManager::OKtoDestroy(int inWhich, char *inMessage)
{
  EMimageBuffer *toBuf = mImBufsp + inWhich;
  if (toBuf->mImage == NULL || toBuf->GetSaveCopyFlag() != 1 ||
      !mConfirmDestroy[toBuf->mConSetUsed] ||
      mWinApp->Montaging() || mWinApp->DoingTasks())
    return true;
  char buf = 'A' + inWhich;
  CString text;
  text.Format("There is an UNSAVED image in Buffer %c, captured"
      " in %s mode.\r%s will destroy this image.\r"
      "Do you really want to do this?\r\r(To turn off this warning,"
      " see \"Options\" in the Buffer Manager Window.)",
      buf, mModeNames[toBuf->mConSetUsed], inMessage);
  return (AfxMessageBox(text, MB_YESNO | MB_ICONQUESTION) == IDYES);
} 

BOOL EMbufferManager::DoesBufferExist (int inWhich)
{
  return ( (mImBufsp + inWhich)->mImage != NULL);
}

int EMbufferManager::CheckSaveConditions(KImageStore *inStoreMRC, EMimageBuffer *toBuf)
{
  if (CheckAsyncSaving())
    return 1;
  if (inStoreMRC == NULL) { 
    SEMMessageBox("Error saving image: no file open");
    return 1;
  }

  // Skip ALL tests if doing a macro
  if (mWinApp->mMacroProcessor->DoingMacro())
    return 0;
  if (toBuf->GetSaveCopyFlag() < 0) {
    if(AfxMessageBox("This image has already been saved to a file.\r\r"
      "Are you sure you want to save it again?", MB_YESNO | MB_ICONQUESTION) == IDNO)
      return -1;
  }
  if (!mWinApp->mMontageController->DoingMontage() && toBuf->GetSaveCopyFlag() == 0 && 
    (toBuf->mCaptured > 0)) {
      if(AfxMessageBox("This image was from a continuous capture.\r\r"
        "Are you sure you want to save it?", MB_YESNO | MB_ICONQUESTION) == IDNO)
        return -1;
  }
  if (toBuf->GetSaveCopyFlag() == 0 && (toBuf->mCaptured < 0)) {
    if(AfxMessageBox("This image has been processed.\r\r"
      "Are you sure you want to save it?", MB_YESNO | MB_ICONQUESTION) == IDNO)
      return -1;
  }
  if (toBuf->GetSaveCopyFlag() == 0 && !toBuf->mCaptured) {
    if(AfxMessageBox("This image was read in from a file.\r\r"
      "Are you sure you want to save it?", MB_YESNO | MB_ICONQUESTION) == IDNO)
      return -1;
  }
  return 0;
}

int EMbufferManager::SaveImageBuffer(KImageStore *inStore, bool skipCheck, int inSect)
{
  KStoreIMOD *tiffStore;
  EMimageExtra *extra;
  double axisRot;
  float axis, bidirAngle;
  int spot, err, check, cam, mag, oldDivided;
  EMimageBuffer *toBuf = GetSaveBuffer();
  if (!skipCheck) {
    check = CheckSaveConditions(inStore, toBuf);
    if (check)
      return check;
  }


  // First time, set pixel in Angstroms, add a title with tilt axis rotation
  if (!inStore->getDepth() && toBuf->mMagInd && toBuf->mBinning) {
    if (inStore->getStoreType() != STORE_TYPE_IMOD) {
      cam = toBuf->mCamera >= 0 ? toBuf->mCamera : mWinApp->GetCurrentCamera();
      mag = toBuf->mMagInd ? toBuf->mMagInd : mWinApp->mScope->FastMagIndex();
      float pixel = mWinApp->mShiftManager->GetPixelSize(cam, mag);
      inStore->SetPixelSpacing((float)(10000. * B3DMAX(1, toBuf->mBinning) * pixel));
    }

    // 11/6/06: Removed 180 degree rotations for angles beyond 90!  Preserve true 
    // polarity. But 12/4/06: needed to subtract not add the 90 degrees to represent 
    // Y axis rotation
    if (toBuf->GetAxisAngle(axis)) {
      axisRot = axis - 90.;
    } else {
      axisRot = mWinApp->mShiftManager->GetImageRotation(cam, mag) - 90.;
    }
    if (!toBuf->GetSpotSize(spot))
      spot = mWinApp->mScope->FastSpotSize();
    char title[80];
    CString str, bidir;
    if (mRotateAxisAngle)
      axisRot += 180.;
    axisRot = UtilGoodAngle(axisRot);

    // Separate with spaces because etomo would end up with a comma after the binning
    str.Format("    Tilt axis angle = %.1f, binning = %s  spot = %d  camera = %d", 
      axisRot, toBuf->BinningText(), spot, cam);
    if (mWinApp->mTSController->GetBidirStartAngle(bidirAngle)) {
      bidir.Format("  bidir = %.1f", bidirAngle);
      str += bidir;
    }
    sprintf(title, "%s", (LPCTSTR)str); 
    inStore->AddTitle(title);
  }
  toBuf->mCurStoreChecksum = 0;
  if (inStore == mWinApp->mStoreMRC)
    toBuf->mCurStoreChecksum = mWinApp->mStoreMRC->getChecksum();

  // Set flags before saving so mDivided can be in the mdoc
  extra = SetChangeWhenSaved(toBuf, inStore, oldDivided);
  if (inStore->getStoreType() != STORE_TYPE_IMOD) {
    err = 1;
    int secnum = inStore->getDepth();
    if (mSaveAsynchronously && !mWinApp->SavingOther()) {
      err = StartAsyncSave(inStore, toBuf, inSect);
      if (err)
        mWinApp->AppendToLog("Insufficient memory to start a background save; "
        "trying a synchronous save");
    }
    if (err) {
      if (inSect < 0)
        err = inStore->AppendImage(toBuf->mImage);
      else
        err = inStore->WriteSection(toBuf->mImage, inSect);

      // flush buffers to protect from crashes
      if (!err)
        inStore->Flush();
    }
    if (!err)
      toBuf->mSecNumber = inSect < 0 ? secnum : inSect;
  } else {
    tiffStore = (KStoreIMOD *)inStore;
    
    err = tiffStore->WriteSection(toBuf->mImage);
    if (!err)
      toBuf->mSecNumber = 0;
  }

  // Restore flag in extra; it is used for dose rates etc.
  if (extra)
    extra->mDividedBy2 = oldDivided;
  if (err) {
    ReportError(err);
    return 1;
  }

  toBuf->NegateSaveCopy();
  check = MainImBufIndex(toBuf);
  if (mCopyOnSave > 0 && !mWinApp->Montaging() && !mWinApp->SavingOther() && check >= 0)
    CopyImageBuffer(check, mCopyOnSave);
  mWinApp->UpdateBufferWindows();
  if (inStore->getLastIntTruncation() > mUnsignedTruncLimit && 
    toBuf->mImage->getType() == kUSHORT) {
      CString message;
      message.Format("%.1f%% of pixel values were truncated when saving\r\n"
        "this unsigned image to a signed integer file\r\n\r\n"
        "You need to save these data to an unsigned (mode 6) file or\r\n"
        "choose to divide by 2 or subtract 32768 when saving to a signed file.",
        100. * inStore->getLastIntTruncation());
      SEMMessageBox(message, MB_EXCLAME);
      return 1;
  }
  return 0;
}

int EMbufferManager::OverwriteImage(KImageStore *inStoreMRC, int inSect)
{
  EMimageExtra *extra;
  EMimageBuffer *toBuf = GetSaveBuffer();
  int oldDivided;
  int check = CheckSaveConditions(inStoreMRC, toBuf);
  if (check)
    return check;

//  if (inStoreMRC == NULL)
//    return 1;
  int limit = inStoreMRC->getDepth() - 1;
  int number = limit;

  if (inSect >= 0)
    number = inSect;

  // Here, even if only one section, ask for # to give user chance to cancel

  else if (KGetOneInt("Section number to overwrite:", number) ) {
    if (number > limit || number < 0) {
      AfxMessageBox("Illegal section number; image not saved.", MB_EXCLAME);
      return 1;
    }
  } else 
    return -1;
  
  int err = 1;
  extra = SetChangeWhenSaved(toBuf, inStoreMRC, oldDivided);
  if (mSaveAsynchronously && !mWinApp->SavingOther())
    err = StartAsyncSave(inStoreMRC, toBuf, number);
  if (err)
    err = inStoreMRC->WriteSection(toBuf->mImage, number);
  if (extra)
    extra->mDividedBy2 = oldDivided;
  if (err) {
    ReportError(err);
    return 1;
  }
  toBuf->NegateSaveCopy();
  toBuf->mSecNumber = number;
  if (mCopyOnSave > 0  && !mWinApp->Montaging())
    CopyImageBuffer((int)(toBuf - mImBufsp), mCopyOnSave);
  mWinApp->UpdateBufferWindows();
  return 0;
}

// Set flag for type of modification that occurred to the data when saved
// Returns an extra structure and previous value if mDividedBy2 was set
EMimageExtra *EMbufferManager::SetChangeWhenSaved(EMimageBuffer *imBuf, 
  KImageStore *inStore, int &oldDivided)
{
  EMimageExtra *extra = NULL;
  imBuf->mChangeWhenSaved = 0;
  if (inStore->getStoreType() != STORE_TYPE_MRC && 
    inStore->getStoreType() != STORE_TYPE_IMOD)
    return NULL;
  if (imBuf->mImage->getType() == kSHORT && inStore->getMode() == MRC_MODE_USHORT && 
    inStore->getSignToUnsignOpt() == SHIFT_SIGNED) {
      imBuf->mChangeWhenSaved = SIGNED_SHIFTED;
  } else if (imBuf->mImage->getType() == kUSHORT && inStore->getMode() == MRC_MODE_SHORT){
    if (inStore->getUnsignOpt() == SHIFT_UNSIGNED)
      imBuf->mChangeWhenSaved = SHIFT_UNSIGNED;
    else if (inStore->getUnsignOpt() == DIVIDE_UNSIGNED) {
      imBuf->mChangeWhenSaved = DIVIDE_UNSIGNED;
      extra = imBuf->mImage->GetUserData();
      if (extra) {
        oldDivided = extra->mDividedBy2; 
        extra->mDividedBy2 = 1;
      }
    }
  }
  return extra;
}

// Get Filename for reading other file
int EMbufferManager::ReadOtherFile()
{
  int err;
  CString mess;
  static char BASED_CODE szFilter[] = "Image files (*.mrc, *.st, *.map, *.tif*, *.dm3,"
    " *.dm4)|*.mrc; *.st; *.map; *.tif*; *.dm3; *.dm4|All files (*.*)|*.*||";
  LPCTSTR lpszFileName = NULL;
  if (!mOtherFile.IsEmpty())
    lpszFileName = mOtherFile;
  MyFileDialog fileDlg(TRUE, NULL, lpszFileName, OFN_HIDEREADONLY, szFilter);
  if (fileDlg.DoModal() == IDOK) {
    mOtherFile = fileDlg.GetPathName();
    if (mWinApp->mDocWnd->FileAlreadyOpen(mOtherFile, 
      "To read from it, you need to make it be the current file."))
      return -1;
    err = RereadOtherFile(mess);
    if (err && !mess.IsEmpty())
      SEMMessageBox(mess);
    return err;
  }
  return -1;
}

// Read from other file after the standard file reply record is gotten
int EMbufferManager::RereadOtherFile(CString &message)
{
  CFile *file;
  CString fullpath;
  int err = 0;

  // Filename exists?
  if (mOtherFile.IsEmpty()) {
    message = "Error: Other file can no longer be opened.";
    return 1;
  }

  // Open binary file
  try {
    file = new CFile(mOtherFile, CFile::modeRead |CFile::shareDenyWrite);
  }
  catch (CFileException *error) {
    message = "Error: Cannot open the selected file.";
    error->Delete();
    return 1;
  }

  // Check if MRC
  err = 1;
  if (KStoreMRC::IsMRC(file)) {
    mOtherStoreMRC = new KStoreMRC(file);
    err = 0;
  } else {

    // Close file and check for ADOC, use full path to open if so
    file->Close();
    delete file;
    fullpath = KStoreADOC::IsADOC(mOtherFile);
    if (!fullpath.IsEmpty()) {
      mOtherStoreMRC = new KStoreADOC(fullpath);
      err = 0;
    }
  }

  // If things are OK either way, read
  if (!err) {
    if (mOtherStoreMRC->FileOK())
      err = ReadFromFile(mOtherStoreMRC, NO_SUPPLIED_SECTION, -1, false, false, &message);
    else {
      err = READ_MONTAGE_OK * 2;
      message = "The apparent MRC or ADOC file is not actually a valid file";
    }
    if (err == READ_MONTAGE_OK) {
      mWinApp->AddIdleTask(NULL, TASK_DEL_OTHER_STORE, 0, 0);
      return err;
    }
    delete mOtherStoreMRC;     // This closes the file and deletes the file object

  } else {

    // Otherwise now try IMOD file
    KStoreIMOD *storeIMOD = new KStoreIMOD(mOtherFile);
    if (storeIMOD && storeIMOD->FileOK()) {
      err = ReadFromFile(storeIMOD);
      delete storeIMOD;

    } else {
      if (storeIMOD)
        delete storeIMOD;
      message = "The file is neither an MRC file nor\nany other type that the"
        " program can read.";
      return 1;
    }
  }

  // Common actions after attempting to read from file
  if (err) {
    if (err > 0)    // If its a hard error, set this file as no good
      mOtherFile = "";
    return err;
  }
  EMimageBuffer *toBuf = mImBufsp + mBufToReadInto;
  toBuf->mSecNumber = -toBuf->mSecNumber - 1;
  return err;
}

// Read a selected image from file, optionally supplying section number and buffer #
// Here return 1 if there is something wrong with reading from the file,
// or -1 if it is just the users choice or error
int EMbufferManager::ReadFromFile(KImageStore *inStore, int inSect, int inToBuf,
                                  BOOL readPiece, BOOL synchronous, CString *message)
{
  MontParam *masterMont = mWinApp->GetMontParam();
  static MontParam param;
  EMimageExtra *extra;
  ControlSet *conSet = mWinApp->GetConSets() + RECORD_CONSET;
  int xPc, yPc, zPc;
  int pieceX, pieceY, sizeX, sizeY;
  float bufBin;
  KStoreMRC *storeMRC;
  const char *title, *binStr;
  char titleCopy[84];
  int montErr = 0;
  if (message)
    *message = "";

  if (inToBuf < 0)
    inToBuf = mBufToReadInto;
  EMimageBuffer *toBuf = mImBufsp + inToBuf;
  if (CheckAsyncSaving())
    return 1;
  if (!OKtoDestroy(inToBuf, "Reading in an image"))
    return -1;
  if (inStore == NULL) {
    if (message)
      *message = "No file is open to read from.";
    else
      SEMMessageBox("Error: no file is open to read from.");
    return 1;
  }
  int limit = inStore->getDepth() - 1;
  int number = limit;

  // If section supplied in the call or in previous call, use it
  if (mNextSecToRead >= 0) {
    inSect = mNextSecToRead;
    synchronous = false;
    mNextSecToRead = NO_SUPPLIED_SECTION;
  }
  if (inSect != NO_SUPPLIED_SECTION)
    number = inSect;

  if (!readPiece) {

    // IF not reading piece, need to check if this file is a montage;
    if (inStore->getStoreType() == STORE_TYPE_MRC || 
      inStore->getStoreType() == STORE_TYPE_ADOC) {
      param = *masterMont;
      montErr = inStore->CheckMontage(&param);

      // New behavior 2/3/04: unless this is the current file, read whole montage
      // from a montaged file; return code to prevent read buffer from being displayed
      if (montErr > 0 && inStore != mWinApp->mStoreMRC) {
        montErr = mWinApp->mMontageController->ReadMontage(inSect, &param, 
          inStore, false, synchronous);
        if (montErr)
          return montErr;
        return READ_MONTAGE_OK;
      }
    }

    // If not reading montage, now take care of getting section
    if (inSect == NO_SUPPLIED_SECTION) {
      if (montErr > 0) {
        limit = param.zMax;
        number = limit;
      }

      // If there is more than one section, ask for section number
      if (number) {
        if (KGetOneInt("Section # to read in:", number)){
          if (number > limit || number < 0) {
            AfxMessageBox("Illegal section number.", MB_EXCLAME);
            return -1;
          }
        } else
          return -1;
      } 

      // If montage, ask for piece numbers
      if (montErr > 0) {
        pieceX = param.xNframes / 2 +1;
        pieceY = param.yNframes / 2 +1;
        if (pieceX > 1)
          if (!KGetOneInt("Piece # to read in X:", pieceX))
            return -1;
        if (pieceY > 1)
          if (!KGetOneInt("Piece # to read in Y:", pieceY))
            return -1;
        if (pieceX > param.xNframes || pieceX < 1 ||
          pieceY > param.yNframes || pieceY < 1 ) {
            AfxMessageBox("Illegal piece number.", MB_EXCLAME);
            return -1;
          }

          // Search through piece coords for ones matching this piece
          int coordX = (pieceX - 1) * (param.xFrame - param.xOverlap);
          int coordY = (pieceY - 1) * (param.yFrame - param.yOverlap);
          int sec = -1;
          for (int iz = 0; iz < inStore->getDepth(); iz++) {
            inStore->getPcoord(iz, xPc, yPc, zPc);
            if (zPc == number && xPc == coordX && 
              yPc == coordY) {
                sec = iz;
                break;
              }
          }

          if (sec < 0) {
            AfxMessageBox("Piece not found in file.", MB_EXCLAME);
            return -1;
          }
          number = sec;
      } 
    }
  }

  // Get the section
  inStore->setSection(number);
  toBuf->DeleteImage();
  toBuf->DeleteOffsets();
  toBuf->mImage = inStore->getRect();
  if (toBuf->mImage == NULL) {
    if (message)
      *message = "Error reading from file.";
    else
      SEMMessageBox("Error reading from file.");
    return 1;
  }  

  // Get the scaling
  toBuf->mCaptured = 0;
  FindScaling(toBuf);  
  toBuf->SetImageChanged(1);

  toBuf->mSaveCopyp = NULL;
  toBuf->mSecNumber = number;

  // Set defocus zero, assume the current mag
  toBuf->mDefocus = 0.;
  toBuf->mMagInd = 0;
  toBuf->mHasUserPt = false;
  toBuf->mHasUserLine = false;
  toBuf->mTimeStamp = (float)0.001 * GetTickCount();
  toBuf->mChangeWhenSaved = 0;
  toBuf->mMapID = 0;
  toBuf->mStage2ImMat.xpx = 0.;
  toBuf->mRegistration = 0;
  toBuf->mZoom = 0.;
  toBuf->mISX = 0.;
  toBuf->mISY = 0.;
  toBuf->mPanX = 0;
  toBuf->mPanY = 0;
  toBuf->mCurStoreChecksum = 0;
  if (inStore == mWinApp->mStoreMRC)
    toBuf->mCurStoreChecksum = mWinApp->mStoreMRC->getChecksum();

  // If extra values exist, replace these defaults
  // get the binning and camera back from an adoc if possible
  bufBin = 0.;
  toBuf->mCamera = -1;
  extra = (EMimageExtra *)toBuf->mImage->GetUserData();
  if (extra) {
    if (extra->mISX > EXTRA_VALUE_TEST) {
      toBuf->mISX = extra->mISX;
      toBuf->mISY = extra->mISY;
    }
    if (extra->mDefocus > EXTRA_VALUE_TEST)
      toBuf->mDefocus = extra->mDefocus;
    if (extra->mBinning > 0.)
      bufBin = extra->mBinning;
    if (extra->mCamera >= 0 && extra->mCamera < MAX_CAMERAS)
      toBuf->mCamera = extra->mCamera;
    if (extra->mMagIndex >= 0)
      toBuf->mMagInd = extra->mMagIndex;
    if (extra->mExposure >= 0.)
      toBuf->mExposure = extra->mExposure;
  }
  if (mWinApp->mNavigator)
    toBuf->mRegistration = mWinApp->mNavigator->GetCurrentRegistration();

  // Otherwise try to read the binning and camera from the second title of an MRC file
  if ((!bufBin || toBuf->mCamera < 0) && inStore->getStoreType() == STORE_TYPE_MRC) {
    storeMRC = (KStoreMRC *)inStore;
    title = storeMRC->GetTitle(1);
    if (title) {
      strncpy(titleCopy, title, 80);
      titleCopy[80] = 0x00;
      if (!bufBin) {
        binStr = strstr(titleCopy, "binning");
        if (binStr) {
          binStr = strstr(binStr, "=");
          if (binStr)
            bufBin = (float)atof(binStr + 1);
        }
      }
      if (toBuf->mCamera < 0) {
        binStr = strstr(titleCopy, "camera");
        if (binStr) {
          binStr = strstr(binStr, "=");
          if (binStr)
            toBuf->mCamera = atoi(binStr + 1);
        }
      }
    }
  }

  // Otherwise assign camera from current camera, set divide flag, and get internal 
  // binning back
  if (toBuf->mCamera < 0)
    toBuf->mCamera = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams();
  toBuf->mDivideBinToShow = camParam[toBuf->mCamera].K2Type ? 2 : 1;
  toBuf->mBinning = B3DNINT(bufBin * toBuf->mDivideBinToShow);

  // If that fails to get a binning, fall back to the problematic old logic
  if (!toBuf->mBinning) {

    // If this is read from montage, set the binning to montage binning
    if (mWinApp->mMontageController->DoingMontage() || 
      (mWinApp->Montaging() && montErr > 0 )) {
        MontParam *master = mWinApp->GetMontParam();
        toBuf->mBinning = master->binning;
    } else {

      // Otherwise set it based on full field of camera
      int binX = camParam[toBuf->mCamera].sizeX / toBuf->mImage->getWidth();
      int binY = camParam[toBuf->mCamera].sizeY / toBuf->mImage->getHeight();
      toBuf->mBinning = binX < binY ? binX : binY;
      if (!toBuf->mBinning)
        toBuf->mBinning = 1;

      // But if size matches current record size, set it to record binning
      mWinApp->mCamera->AcquiredSize(conSet, toBuf->mCamera, sizeX, sizeY);
      if (toBuf->mImage->getWidth() == sizeX && toBuf->mImage->getHeight() == sizeY)
        toBuf->mBinning = conSet->binning;
    }
  }

  // Now handle mag if it is available in an older file; look up the mag value as long
  // as there are no secondary modes
  if (extra && extra->mMagIndex < 0 && extra->m_iMag > 0 && 
    !mWinApp->mScope->GetLowestSecondaryMag()) {
      for (int ind = MAX_MAGS - 1; ind > 0; ind--) {
        if (fabs((double)extra->m_iMag - MagOrEFTEMmag(camParam[toBuf->mCamera].GIF, ind, 
          camParam[toBuf->mCamera].STEMcamera)) < 1.) {
            toBuf->mMagInd = ind;
            break;
        }
      }
  }
  if (!toBuf->mMagInd)
    toBuf->mMagInd = mWinApp->mScope->GetMagIndex();

  return 0;
}

int EMbufferManager::ReplaceImage(char *inData, int inType, int inX, int inY, 
              int inBuf, int inCap, int inConSet, bool fftBuf)
{
  EMimageBuffer *toBuf = B3DCHOICE(fftBuf, mWinApp->GetFFTBufs(), mImBufsp) + inBuf;
  toBuf->DeleteImage();
  toBuf->DeleteOffsets();
  switch (inType) {
    case kUBYTE:
      toBuf->mImage = new KImage;
      break;
    case kSHORT:
    case kUSHORT:
      toBuf->mImage = new KImageShort;
      toBuf->mImage->setType(inType);
      break;
    case kFLOAT:
      toBuf->mImage = new KImageFloat;
      break;
    case kRGB:
      toBuf->mImage = new KImageRGB;
      break;
    default:
      return 2;
      break;
  }
  toBuf->mImage->useData(inData, inX, inY);

  toBuf->mCaptured = inCap;
  FindScaling(toBuf);  
  toBuf->SetImageChanged(1);

  toBuf->mSaveCopyp = NULL;
  toBuf->mConSetUsed = inConSet;
  toBuf->mSecNumber = 0;
  toBuf->mBinning = 1;
  toBuf->mHasUserPt = false;
  toBuf->mHasUserLine = false;
  toBuf->mMapID = 0;
  toBuf->mStage2ImMat.xpx = 0.;
  toBuf->mZoom = 0.;
  toBuf->mISX = 0.;
  toBuf->mISY = 0.;
  toBuf->mCurStoreChecksum = 0;

  int current = mWinApp->GetImBufIndex(fftBuf);

  // If the copy was into the current display, draw it regardless
  if (current == inBuf) {
    if (fftBuf) {
      if (mWinApp->mFFTView)
        mWinApp->mFFTView->DrawImage();
    } else
      mWinApp->mMainView->DrawImage();
  }

  // need to update status window
  mWinApp->UpdateBufferWindows();

  // Have active window tell imagelevel window what happened
  mWinApp->mActiveView->NewImageScale();

  return 0;
}

// Prepare a buffer to add to a stack window and pass it to App
int EMbufferManager::AddToStackWindow(int bufNum, int maxSize, int secNum, bool convert)
{
  EMimageBuffer *imBuf = &mImBufsp[bufNum];
  KImage *image = imBuf->mImage;
  KImageShort *newImage;
  EMimageBuffer *newBuf;
  int binning;
  short int *array;
  void *data;
  int type, width, height, newWidth, newHeight;

  // Find binning needed to get images small enough
  if (!image)
    return 1;
  type = image->getType();
  width = image->getWidth();
  height = image->getHeight();
  if (maxSize <= 8 || maxSize > mStackWinMaxXY)
    maxSize = mStackWinMaxXY;
  for (binning = 1; binning < 64; binning++) {
    if (width * height / (binning * binning) <= maxSize * maxSize)
      break;
  }

  // Get array for binned data or copy, and bin or copy data into it
  newWidth = width / binning;
  newHeight = height / binning;
  NewArray(array, short int, newWidth * newHeight);
  if (!array)
    return 2;
  image->Lock();
  data = (void *)image->getData();
  if (binning > 1)
    XCorrBinByN(data, type, width, height, binning, array);
  else
    memcpy(array, data, width * height * 2);
  image->UnLock();

  // Make new image and imbuf and get scaling
  newImage = new KImageShort();
  if (type == kUSHORT)
    newImage->setType(kUSHORT);
  newImage->useData((char *)array, newWidth, newHeight);
  newBuf = new EMimageBuffer();
  newBuf->mImage = newImage;
  newBuf->mCaptured = BUFFER_STACK_IMAGE;
  FindScaling(newBuf);
  newBuf->mSecNumber = secNum;
  newBuf->mBinning = imBuf->mBinning * binning;
  newBuf->mConSetUsed = imBuf->mConSetUsed;
  imBuf->GetTiltAngle(newBuf->mTiltAngleOrig);

  // Make pixmap and convert to byte
  if (convert) {
    newBuf->mPixMap = new KPixMap();
    newBuf->ConvertToByte(newBuf->mImageScale->GetMinScale(), 
    newBuf->mImageScale->GetMaxScale());
  }

  // Tell App to add it to the stack view
  return mWinApp->AddToStackView(newBuf);
}

#define NUM_MESSAGES 20
static char *messages[NUM_MESSAGES] = {
  "No image data", "No file open", "Extended header full", 
  "Memory error getting temporary array", "Error seeking to file position",
  "Error while writing image data", "Error while writing header",
  "Error while writing extended header info", "Mismatch between data type and file mode",
  "Saving as TIFF to wrong file type", "Unsupported data type", 
  "Trying to skip section number in TIFF series", "Error opening new TIFF file", 
  "Error setting current Adoc index", "Error adding values to autodoc", 
  "Error writing autodoc file", "Image is the wrong size to save into this file", 
  "Timeout trying to save to file", "Error reading from first-half file", "Unknown error"
};

CString EMbufferManager::ComposeErrorMessage(int inErr, char *writeType)
{
  CString message;
  if (inErr < 1 || inErr > NUM_MESSAGES)
    inErr = NUM_MESSAGES;
  message.Format("Error writing data %sto file:\r\n%s.", writeType, messages[inErr-1]);
  if (inErr == 3 || (inErr >= 5 && inErr <= 9))
    message += "\r\n\r\nTry closing the file and opening a new one.";
  return message;
}

void EMbufferManager::ReportError (int inErr)
{
  CString message = ComposeErrorMessage(inErr, " ");
  SEMMessageBox(message);
}

// Get the index of a buffer if it is in the main ImBufs, or -1 if not
int EMbufferManager::MainImBufIndex(EMimageBuffer *imBuf)
{
  for (int i = 0; i < MAX_BUFFERS; i++)
    if (imBuf == &mImBufsp[i])
      return i;
  return -1;
} 

// Get the index of the buffer that is the default for autoalign
int EMbufferManager::AutoalignBufferIndex()
{
  if (mAlignToB)
    return 1;
  if (mWinApp->LowDoseMode() && mImBufsp->mCaptured && mImBufsp->mImage && 
    (mImBufsp->mConSetUsed + 1) / 2 != 2)
    return mShiftsOnAcquire + 2;
  return mShiftsOnAcquire + 1;
}

int EMbufferManager::AutoalignBasicIndex()
{
  return(mAlignToB ? 1 : mShiftsOnAcquire + 1);
}

// Create new imageScale if needed then find stretch with standard parameters
void EMbufferManager::FindScaling(EMimageBuffer * imBuf)
{
  float pctLo, pctHi;
  if (imBuf->mImageScale == NULL)
    imBuf->mImageScale = new KImageScale;
  if (imBuf->mImage->getMode() == kRGBmode)
    return;
  mWinApp->GetDisplayTruncation(pctLo, pctHi);
  imBuf->mImageScale->FindPctStretch(imBuf->mImage, pctLo, pctHi,
    mWinApp->GetPctAreaFraction(), 
    B3DCHOICE(imBuf->mCaptured == BUFFER_FFT || imBuf->mCaptured == BUFFER_LIVE_FFT, 
    mWinApp->GetBkgdGrayOfFFT(), 0), mWinApp->GetTruncDiamOfFFT());
}

// Initiate saving to file on a separate thread
int EMbufferManager::StartAsyncSave(KImageStore *store, EMimageBuffer *buf, int section)
{
  KImage *image = buf->mImage;
  char *data;
  int numBytes = image->getRowBytes() * image->getHeight();
  mAsyncTimeout = 10000 + numBytes / 1000;

  // Duplicate the data first, that is a possible failure point
  NewArray(data, char, numBytes);
  if (!data)
    return 4;
  memcpy(data, image->getData(), numBytes);

  // Copy the image buffer then assign copied data and manage the reference counts
  CopyImBuf(buf, &mImBufForSave, false);
  mImBufForSave.mImage->detachData();
  mImBufForSave.mImage->useData(data, image->getWidth(), image->getHeight());
  mImBufForSave.DecrementSaveCopy();
  mImBufForSave.mSaveCopyp = NULL;
  mImBufForSave.IncrementSaveCopy();
  mAsyncFromImage = false;

  // Start the thread; fromStore NULL is sign that this is save
  StartAsyncThread(NULL, store, section, false);
  return 0;
}

// Start an asynchronous save of an image not in an image buffer
int EMbufferManager::StartAsyncSave(KImageStore *store, KImage *image, int section)
{
  int numBytes = image->getRowBytes() * image->getHeight();
  mAsyncTimeout = 10000 + numBytes / 1000;
  mImBufForSave.mImage = image;
  mAsyncFromImage = true;
  mImageAsyncFailed = false;
  StartAsyncThread(NULL, store, section, false);
  return 0;
}

// Start an asynchronous or synchronous copy from one store/section to another
int EMbufferManager::StartAsyncCopy(KImageStore *fromStore, KImageStore *toStore, 
                                     int fromSection, int toSection, bool synchronous)
{
  mSaveTD.fromSection = fromSection;
  mAsyncFromImage = false;
  StartAsyncThread(fromStore, toStore, toSection, synchronous);
  if (!synchronous)
    return 0;
  if (mSaveTD.error) {
    CString message = ComposeErrorMessage(mSaveTD.error, "during copy to invert file ");
    SEMMessageBox(message);
    mWinApp->ErrorOccurred(mSaveTD.error);
  }
  return mSaveTD.error;
}

// Start the thread for either kind of asynchronous file I/O, or do synchronous copy
void EMbufferManager::StartAsyncThread(KImageStore *fromStore, KImageStore *store, 
                                       int section, bool synchronous)
{
  // Set up thread data
  mSaveTD.error = 0;
  mSaveTD.image = mImBufForSave.mImage;
  mSaveTD.section = section;
  mSaveTD.store = store;
  mSaveTD.fromStore = fromStore;
  if (synchronous) {
    SEMTrace('y', "Doing synchronous copy %d", section);
    SavingProc(&mSaveTD);
    return;
  }
  mSavingThread = AfxBeginThread(SavingProc, &mSaveTD,
    THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED);
  mSavingThread->m_bAutoDelete = false;
  mSavingThread->ResumeThread();
  mAsyncStartTime = GetTickCount();
  mWinApp->AddIdleTask(TASK_ASYNC_SAVE, 0, mAsyncTimeout * (fromStore ? 2 : 1));
  mDoingAsyncSave = true;
  SEMTrace('y', "Launched thread for async %s %d", fromStore ? "copy" : "save", section);
}

// The thread proc for background saving or copying
UINT EMbufferManager::SavingProc(LPVOID pParam)
{
  SaveThreadData *saveTD = (SaveThreadData *)pParam;
  if (saveTD->fromStore) {

    // Copy
    saveTD->fromStore->setSection(saveTD->fromSection);
    KImage *image = saveTD->fromStore->getRect();
    if (!image) {
      saveTD->error = 19;
    } else {
      saveTD->error = saveTD->store->WriteSection(image, saveTD->section);
      delete image;
    }
    
  } else {

    // Save
    if (saveTD->section < 0)
      saveTD->error = saveTD->store->AppendImage(saveTD->image);
    else
      saveTD->error = saveTD->store->WriteSection(saveTD->image, saveTD->section);
  }
  if (!saveTD->error)
    saveTD->store->Flush();
  return saveTD->error;
}

// Checks if the save is still busy
int EMbufferManager::AsyncSaveBusy(void)
{
  DWORD exitCode;
  int retval = UtilThreadBusy(&mSavingThread, &exitCode);
  if (retval < 0)
    SEMTrace('y', "Async save thread exited with code %d", exitCode);
  if (retval < 0 && mAsyncFromImage)
    mImageAsyncFailed = true;
  return retval;
}

// Function called when the task is done on its own, with no waiting by a caller
void EMbufferManager::AsyncSaveDone(void)
{
  AsyncSaveCleanup(0);
  if (mSaveTD.fromStore) {
    SEMTrace('y', "Async copy done, starting next");
    mWinApp->mMultiTSTasks->AsyncCopySucceeded(true);
  } else if (mWinApp->mMultiTSTasks->BidirCopyPending()) {
    SEMTrace('y', "Async save done, starting next async copy");
    mWinApp->mMultiTSTasks->StartBidirFileCopy(false);
  }
}

// Clean up from an ansynchronous save 
void EMbufferManager::AsyncSaveCleanup(int error)
{
  CString message;
  if (!mDoingAsyncSave)
    return;
  UtilThreadCleanup(&mSavingThread);
  if (error || mSaveTD.error) {

    if (!mSaveTD.fromStore && !mAsyncFromImage) {

      // Reinvert image if it still needs it
      if (mImBufForSave.mImage->getNeedToReflip()) {
        mImBufForSave.mImage->flipY();
        mImBufForSave.mImage->setNeedToReflip(false);
      }

      // Copy the image buffer back to the read buffer and display it there
      CopyImBuf(&mImBufForSave, mImBufsp + mBufToReadInto, true);
      mWinApp->mMainView->SetCurrentBuffer(mBufToReadInto);
    }

    // Report error
    if (error == IDLE_TIMEOUT_ERROR && !mSaveTD.error)
      mSaveTD.error = 18;
    message = ComposeErrorMessage(mSaveTD.error, "in the background ");
    if (!mSaveTD.fromStore && !mAsyncFromImage)
      message += "\r\n\r\nThe unsaved image is now in buffer " + 
        CString((char)('A' + mBufToReadInto)) +
        "\r\nAfter fixing the problem if possible, save this image with the"
        " \"Save Active\" button or\r\n" + 
        "\"Save Active\" or  \"Save to Other/Save Single\" commands in the File menu";
    mWinApp->AppendToLog(message);

    SEMMessageBox(message);
    mWinApp->ErrorOccurred(error);
  } else {
    
  }
  
  // Clean up the image buffer
  if (!mAsyncFromImage)
    mImBufForSave.DeleteImage();
  mImBufForSave.mImage = NULL;
  mImBufForSave.DeleteOffsets();
  mDoingAsyncSave = false;
}

// Check if asynchronous saving is in progress and wait for it to be done or timed out
int EMbufferManager::CheckAsyncSaving(void)
{
  CString mess;
  int extraWait = 2000, numChecks = 0;
  if (!mDoingAsyncSave)
    return 0;

  // Loop on checking busy.  Inform of wait after a couple of seconds
  mess.Format("Waiting up to %.0f seconds for background saving to finish...", 
    (mAsyncTimeout + SEMTickInterval(mAsyncStartTime)) / 1000.);
  while (SEMTickInterval(mAsyncStartTime) < mAsyncTimeout + extraWait || numChecks < 20) {
    int busy = AsyncSaveBusy();

    // If done, cleanup and return.  Do not call Done in case it morphs into doing more
    // And cancel the idle task so that Done doesn't get called.
    if (busy <= 0) {
      AsyncSaveCleanup(busy);
      mWinApp->RemoveIdleTask(TASK_ASYNC_SAVE);

      // Inform that copy is done successfully, the false means without a restart
      if (!busy && mSaveTD.fromStore)
        mWinApp->mMultiTSTasks->AsyncCopySucceeded(false);
      return -busy;
    }
    numChecks++;
    if (numChecks * 100 == extraWait)
      mWinApp->AppendToLog(mess);
    Sleep(100);
  }
  
  // If drop out of loop, call the timeout so its message will be first
  AsyncSaveCleanup(IDLE_TIMEOUT_ERROR);
  mWinApp->RemoveIdleTask(TASK_ASYNC_SAVE);
 if (mAsyncFromImage)
   mImageAsyncFailed = true;
  return 1;
}

