

// SerialEMDoc.cpp:       Standard MFC MDI component, which contains all the 
//                          message handlers for opening, reading and writing
//                          image files and settings files
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "direct.h"
#include "afxadv.h"

#include ".\SerialEMDoc.h"
#include "SerialEMView.h"
#include "ParameterIO.h"
#include "FilePropDlg.h"
#include "EMbufferManager.h"
#include "EMbufferWindow.h"
#include "Utilities\KGetOne.h"
#include "Image\KStoreIMOD.h"
#include "Image\KStoreADOC.h"
#include "Shared\autodoc.h"
#include "MontageSetupDlg.h"
#include "EMmontageController.h"
#include "ShiftManager.h"
#include "EMscope.h"
#include "MacroEditer.h"
#include "GainRefMaker.h"
#include "NavigatorDlg.h"
#include "TSController.h"
#include "LogWindow.h"
#include "ComplexTasks.h"
#include "CameraController.h"
#include "CalibCameraTiming.h"
#include "TSController.h"
#include "MacroProcessor.h"
#include "ProcessImage.h"
#include "ReadFileDlg.h"
#include "NavHelper.h"
#include "MultiShotDlg.h"
#include "MainFrm.h"
#include "StateDlg.h"
#include "DummyDlg.h"
#include "XFolderDialog\XWinVer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DEFAULT_SYSTEM_PATH "C:\\Program Files\\SerialEM\\"
#define SETTINGS_NAME  "SerialEMsettings.txt"
#define SYSTEM_SETTINGS_NAME  "SEMsystemSettings.txt"
#define CALIBRATION_NAME "SerialEMcalibrations.txt"
#define PROPERTIES_NAME "SerialEMproperties.txt"
#define PROGRAM_LOG "SEMRunLog.txt"
#define SHORT_TERM_NAME "SEMshortTermCal.txt"
#define FLYBACK_NAME "SEMflybackTimes.txt"
#define OPEN_TS_EXT ".openTS"
#define MAX_SETTINGS_MRU 4

static char *defaultSysPath = NULL;
const char *SEMDefaultSysPath(void)
{
  return defaultSysPath;
}

/////////////////////////////////////////////////////////////////////////////
// CSerialEMDoc

IMPLEMENT_DYNCREATE(CSerialEMDoc, CDocument)

BEGIN_MESSAGE_MAP(CSerialEMDoc, CDocument)
  //{{AFX_MSG_MAP(CSerialEMDoc)
  ON_COMMAND(IDM_FILE_OPENNEW, OnFileOpennew)
  ON_UPDATE_COMMAND_UI(IDM_FILE_OPENNEW, OnUpdateFileOpennew)
  ON_COMMAND(IDM_FILE_OPENOLD, OnFileOpenold)
  ON_UPDATE_COMMAND_UI(IDM_FILE_OPENOLD, OnUpdateFileOpenold)
  ON_COMMAND(IDM_FILE_MONTAGESETUP, OnFileMontagesetup)
  ON_UPDATE_COMMAND_UI(IDM_FILE_MONTAGESETUP, OnUpdateFileMontagesetup)
  ON_COMMAND(IDM_FILE_CLOSE, OnFileClose)
  ON_UPDATE_COMMAND_UI(IDM_FILE_CLOSE, OnUpdateFileClose)
  ON_COMMAND(IDM_FILE_SAVE, OnFileSave)
  ON_UPDATE_COMMAND_UI(IDM_FILE_SAVE, OnUpdateFileSave)
  ON_COMMAND(IDM_FILE_SAVEACTIVE, OnFileSaveactive)
  ON_UPDATE_COMMAND_UI(IDM_FILE_SAVEACTIVE, OnUpdateFileSaveactive)
  ON_COMMAND(IDM_FILE_SAVEOTHER, OnFileSaveother)
  ON_UPDATE_COMMAND_UI(IDM_FILE_SAVEOTHER, OnUpdateFileSaveother)
  ON_COMMAND(IDM_FILE_TRUNCATION, OnFileTruncation)
  ON_UPDATE_COMMAND_UI(IDM_FILE_TRUNCATION, OnUpdateFileTruncation)
  ON_COMMAND(IDM_FILE_READ, OnFileRead)
  ON_COMMAND(IDM_FILE_READOTHER, OnFileReadother)
  ON_UPDATE_COMMAND_UI(IDM_FILE_READOTHER, OnUpdateFileReadother)
  ON_UPDATE_COMMAND_UI(ID_WINDOW_NEW, OnUpdateWindowNew)
  ON_UPDATE_COMMAND_UI(IDM_FILE_READ, OnUpdateFileRead)
  ON_COMMAND(ID_SETTINGS_OPEN, OnSettingsOpen)
  ON_COMMAND(ID_SETTINGS_READAGAIN, OnSettingsReadagain)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_READAGAIN, OnUpdateSettingsReadagain)
  ON_COMMAND(ID_SETTINGS_SAVE, OnSettingsSave)
  ON_COMMAND(ID_SETTINGS_SAVEAS, OnSettingsSaveas)
  ON_COMMAND(ID_SETTINGS_CLOSE, OnSettingsClose)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_CLOSE, OnUpdateSettingsClose)
  ON_COMMAND(ID_SETTINGS_READDEFAULTS, OnSettingsReaddefaults)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_READDEFAULTS, OnUpdateSettingsReaddefaults)
  ON_COMMAND(ID_SETTINGS_SAVECALIBRATIONS, OnSettingsSavecalibrations)
  ON_COMMAND(ID_FILE_READPIECE, OnFileReadpiece)
  ON_UPDATE_COMMAND_UI(ID_FILE_READPIECE, OnUpdateFileReadpiece)
  ON_COMMAND(ID_FILE_OVERWRITE, OnFileOverwrite)
  ON_UPDATE_COMMAND_UI(ID_FILE_OVERWRITE, OnUpdateFileOverwrite)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_SAVECALIBRATIONS, OnUpdateSettingsSavecalibrations)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_OPEN, OnUpdateSettingsOpen)
	ON_COMMAND(ID_FILE_SET16BITPOLICY, OnFileSet16bitpolicy)
	ON_UPDATE_COMMAND_UI(ID_FILE_SET16BITPOLICY, OnUpdateFileSet16bitpolicy)
	ON_COMMAND(ID_SETTINGS_AUTOSAVE, OnSettingsAutosave)
	ON_UPDATE_COMMAND_UI(ID_SETTINGS_AUTOSAVE, OnUpdateSettingsAutosave)
	ON_COMMAND(ID_FILE_SETSIGNEDPOLICY, OnFileSetsignedpolicy)
	ON_UPDATE_COMMAND_UI(ID_FILE_SETSIGNEDPOLICY, OnUpdateFileSetsignedpolicy)
	ON_COMMAND(ID_FILE_NEWMONTAGE, OnFileNewmontage)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEWMONTAGE, OnUpdateFileNewmontage)
	//}}AFX_MSG_MAP
  ON_COMMAND_RANGE(ID_SETTINGS_MRU_FILE1, ID_SETTINGS_MRU_FILE8, OnSettingsRecent)
  ON_UPDATE_COMMAND_UI_RANGE(ID_SETTINGS_MRU_FILE1, ID_SETTINGS_MRU_FILE8, OnUpdateSettingsRecent)
  ON_COMMAND(ID_FILE_OPEN_MDOC, OnFileOpenMdoc)
  ON_COMMAND(ID_FILE_CLOSE_MDOC, OnFileCloseMdoc)
  ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE_MDOC, OnUpdateFileCloseMdoc)
  ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_MDOC, OnUpdateFileOpenMdoc)
  ON_COMMAND(ID_FILE_SKIPFILEPROPERTIESDIALOG, OnSkipFilePropertiesDlg)
  ON_UPDATE_COMMAND_UI(ID_FILE_SKIPFILEPROPERTIESDIALOG, OnUpdateSkipFilePropertiesDlg)
  ON_COMMAND(ID_SETTINGS_DISCARDONEXIT, &CSerialEMDoc::OnSettingsDiscardOnExit)
  ON_UPDATE_COMMAND_UI(ID_SETTINGS_DISCARDONEXIT, &CSerialEMDoc::OnUpdateSettingsDiscardOnExit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSerialEMDoc construction/destruction

CSerialEMDoc::CSerialEMDoc()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mDefFileOpt.mode = 1;
  mDefFileOpt.typext = TILT_MASK | INTENSITY_MASK;
  mDefFileOpt.maxSec = 360;
  mDefFileOpt.pctTruncLo = 0.2f;
  mDefFileOpt.pctTruncHi = 0.2f;
  mDefFileOpt.unsignOpt = TRUNCATE_UNSIGNED;
  mDefFileOpt.signToUnsignOpt = TRUNCATE_SIGNED;
  mDefFileOpt.fileType = STORE_TYPE_MRC;
  mDefFileOpt.compression = COMPRESS_NONE;
  mDefFileOpt.useMdoc = false;
  mDfltUseMdoc = -1;
  mDefFileOpt.montageInMdoc = false;
  mDefFileOpt.separateForMont = true;
  mDefFileOpt.montFileType = STORE_TYPE_MRC;
  mDefFileOpt.montUseMdoc = true;
  mOtherFileOpt.pctTruncLo = 0.2f;
  mOtherFileOpt.pctTruncHi = 0.5f;
  mOtherFileOpt.fileType = STORE_TYPE_MRC;
  mOtherFileOpt.compression = COMPRESS_NONE;
  mShowFileDlgOnce = true;
  mSkipFileDlg = true;
  mSTEMfileMode = 1;
  mSTEMunsignOpt = SHIFT_UNSIGNED;
  mMaxTrunc = 2.5f;
  mSystemPath = DEFAULT_SYSTEM_PATH;
  mPluginPath = DEFAULT_SYSTEM_PATH"Plugins";
  mSettingsName = SETTINGS_NAME;
  mPropertiesName = PROPERTIES_NAME;
  mCalibrationName = CALIBRATION_NAME;
  mSystemSettingsName = SYSTEM_SETTINGS_NAME;
  mShortTermName = SHORT_TERM_NAME;
  mFlybackName = FLYBACK_NAME;
  mShortTermNotSaved = false;
  mShortTermBackedUp = false;
  mSettingsBackedUp = false;
  mCalibBackedUp = false;
  mFlybackBackedUp = false;
  mIgnoreShortTerm = false;
  mMinMontageOverlap = 16;
  mMaxMontagePieces = 2000;
  mOverlapFraction = 0.1;
  mMaxOverlapFraction = 0.5f;
  mDefaultMontXpieces = 1;
  mDefaultMontYpieces = 1;
  mMinAnchorField = 20.;
  mTitle = "Digitized by Gatan Camera on TF30";
  mAutoSaveSettings = false;
  mAutoSaveNav = true;
  mNumStores = 0;
  mCurrentStore = -1;
  mRecentSettings = new CRecentFileList(0, "MRUList", "Settings%d", MAX_SETTINGS_MRU, 80);
  mRecentSettings->ReadList();
  mReadFileDlg = NULL;
  mReadDlgPlace.rcNormalPosition.right = 0;
  mFrameAdocIndex = -1;
  CalibrationWasDone(CAL_DONE_CLEAR_ALL);
  mNumCalsAskThresh[CAL_DONE_IS] = 3;
  mNumCalsAskThresh[CAL_DONE_FOCUS] = 2;
  mNumCalsAskThresh[CAL_DONE_STAGE] = 2;
  mNumCalsAskThresh[CAL_DONE_BEAM] = 1;
  mNumCalsAskThresh[CAL_DONE_SPOT] = 1;
  mAbandonSettings = false;
}

CSerialEMDoc::~CSerialEMDoc()
{
  mRecentSettings->WriteList();
  delete mRecentSettings;
  if (defaultSysPath)
    free(defaultSysPath);
  if (mFrameAdocIndex >= 0)
    AdocClear(mFrameAdocIndex);
}

BOOL CSerialEMDoc::OnNewDocument()
{
  if (!CDocument::OnNewDocument())
    return FALSE;

  // TODO: add reinitialization code here
  // (SDI documents will reuse this document)

  return TRUE;
}

void CSerialEMDoc::SetPointers(EMbufferManager *inManager, CEMbufferWindow *inWindow)
{
  mBufferManager = inManager;
  mBufferWindow = inWindow;
}

/////////////////////////////////////////////////////////////////////////////
// CSerialEMDoc serialization

void CSerialEMDoc::Serialize(CArchive& ar)
{
  if (ar.IsStoring())
  {
    // TODO: add storing code here
  }
  else
  {
    // TODO: add loading code here
  }
}

/////////////////////////////////////////////////////////////////////////////
// CSerialEMDoc diagnostics

#ifdef _DEBUG
void CSerialEMDoc::AssertValid() const
{
  CDocument::AssertValid();
}

void CSerialEMDoc::Dump(CDumpContext& dc) const
{
  CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSerialEMDoc commands

// Open new file: just call OpenSaveFile with current options
void CSerialEMDoc::OnFileOpennew() 
{
  DoOpenNewFile();
}

int CSerialEMDoc::DoOpenNewFile(CString filename)
{
  if (mNumStores >= MAX_STORES) {
    AfxMessageBox("Cannot open new file; there are too many files open already",
      MB_EXCLAME);
    return 1;
  }
  if (mBufferManager->CheckAsyncSaving())
    return 1;
  LeaveCurrentFile();
  SetFileOptsForSTEM();
  mFileOpt.typext &= ~MONTAGE_MASK;
  mFileOpt.montageInMdoc = false;
  mFileOpt.TIFFallowed = 0;
  if (filename.IsEmpty())
    mWinApp->mStoreMRC = OpenSaveFile(&mFileOpt);
  else
    mWinApp->mStoreMRC = OpenNewFileByName(filename, &mFileOpt);
  RestoreFileOptsFromSTEM();
  if (mWinApp->mStoreMRC) {
    AddCurrentStore();
    return 0;
  } else {
    RestoreCurrentFile();
    return 1;
  }
}

void CSerialEMDoc::OnUpdateFileOpennew(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mNumStores < MAX_STORES);
}

// Open new file to replace the current one
int CSerialEMDoc::OpenNewReplaceCurrent(CString filename, bool useMdoc)
{
  if (mCurrentStore < 0)
    return 1;
  CopyMasterFileOpts(&mStoreList[mCurrentStore].fileOpt, COPY_FROM_MASTER);
  mStoreList[mCurrentStore].fileOpt.useMdoc = useMdoc;
  mWinApp->mStoreMRC = OpenNewFileByName(filename, &mStoreList[mCurrentStore].fileOpt);
  mStoreList[mCurrentStore].store = mWinApp->mStoreMRC;
  if (mWinApp->mStoreMRC)
    return 0;
  DoCloseFile();
  return 2;
}

// Common path for opening the CFile for an old MRC file when user enters filename
int CSerialEMDoc::UserOpenOldMrcCFile(CFile **file, CString &cFilename, bool imodOK)
{
  static char mrcFilter[] = "Image stacks (*.mrc, *.st, *.map, *.idoc, *.adoc)|"
    "*.mrc; *.st; *.map; *.idoc; *.adoc|All files (*.*)|*.*||";
  static char imodFilter[] = "Image files (*.mrc, *.st, *.map, *.tif*, *.dm3, *.dm4)|"
    "*.mrc; *.st; *.map; *.tif*; *.dm3; *.dm4|All files (*.*)|*.*||";
  char *szFilter = imodOK ? imodFilter : mrcFilter;
  if (mBufferManager->CheckAsyncSaving())
    return MRC_OPEN_CANCEL;
  MyFileDialog fileDlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, szFilter);
  if (fileDlg.DoModal() != IDOK)
    return MRC_OPEN_CANCEL;
  cFilename = fileDlg.GetPathName();
  if (cFilename.IsEmpty())
    return MRC_OPEN_CANCEL;
  return OpenOldMrcCFile(file, cFilename, imodOK);
}

// Common path for opening the CFile for an old MRC or other file and checking its type
// Can return MRC_OPEN_ADOC, MRC_OPEN_NOTMRC, or MRC_OPEN_NOERR for valid files
int CSerialEMDoc::OpenOldMrcCFile(CFile **file, CString cFilename, bool imodOK)
{
  if (mBufferManager->CheckAsyncSaving())
    return MRC_OPEN_CANCEL;
  if (FileAlreadyOpen(cFilename, 
    "To operate on it, you need to make it be the current file"))
    return MRC_OPEN_ALREADY_OPEN;
  
  try {
    *file = new CFile(cFilename, CFile::modeReadWrite |CFile::shareDenyWrite);
  }
  catch (CFileException *err) {
    AfxMessageBox("Error: Cannot open the selected file.", MB_EXCLAME);
    err->Delete();
    return MRC_OPEN_ERROR;
  }
  if (!KStoreMRC::IsMRC(*file)) {
    (*file)->Close();
    delete *file;

    if (!(KStoreADOC::IsADOC(cFilename)).IsEmpty())
      return MRC_OPEN_ADOC;

    if (!imodOK)
      AfxMessageBox("Error: File is not an MRC file.", MB_EXCLAME);
    return MRC_OPEN_NOTMRC;
  }
  return MRC_OPEN_NOERR;
}

// Open an old file from menu - user enters filename
void CSerialEMDoc::OnFileOpenold() 
{
  int err;
  CFile *file;
  CString cFilename;
  err = UserOpenOldMrcCFile(&file, cFilename, false);
  if (err != MRC_OPEN_NOERR && err != MRC_OPEN_ADOC)
    return;
  OpenOldFile(file, cFilename, err);
}

// Common route for opening an old file. err is the code from calling OpenOldMrcCFile
int CSerialEMDoc::OpenOldFile(CFile *file, CString cFilename, int err)
{
  MontParam *param;
  CameraParameters *cam;
  NavParams *navp = mWinApp->GetNavParams();
  if (mBufferManager->CheckAsyncSaving())
    return MRC_OPEN_CANCEL;
  LeaveCurrentFile();
  if (err == MRC_OPEN_NOERR) 
    mWinApp->mStoreMRC = new KStoreMRC(file);
  else
    mWinApp->mStoreMRC = new KStoreADOC(cFilename);
  if (mWinApp->mStoreMRC != NULL) {
    if (mWinApp->mStoreMRC->FileOK()) {
      SetFileOptsForSTEM();
      mWinApp->mStoreMRC->SetTruncation(mFileOpt.pctTruncLo, mFileOpt.pctTruncHi);
      mWinApp->mStoreMRC->SetUnsignedOption(mFileOpt.unsignOpt);
      mWinApp->mStoreMRC->SetSignedOption(mFileOpt.signToUnsignOpt);
      mWinApp->mStoreMRC->setName(cFilename);
      mWinApp->mStoreMRC->SetCompression(mFileOpt.compression);
      RestoreFileOptsFromSTEM();

      // Is it a montage?  If so, need to define parameters, etc.
      param = mWinApp->GetMontParam();
      err = mWinApp->mStoreMRC->CheckMontage(param);
      if (err > 0) {

        // First try to get everything from the autodoc
        param->binning = 0;
        int ind, binInd, binning, camInd, magInd, adocInd, conSet, stage = -1;
        int lowDose = -1;
        int *activeList = mWinApp->GetActiveCameraList();
        adocInd = mWinApp->mStoreMRC->GetAdocIndex();
        if (adocInd >= 0 && !AdocAcquireMutex())
          adocInd = -1;
        if (adocInd >= 0) {
          AdocGetInteger(ADOC_MONT_SECT, 0, ADOC_STAGE_MONT, &stage);
          AdocGetTwoIntegers(ADOC_MONT_SECT, 0, ADOC_CONSET_USED, &conSet, &lowDose);
          if (!AdocGetInteger(ADOC_ZVALUE, 0, ADOC_CAMERA, &ind) && 
            !AdocGetInteger(ADOC_ZVALUE, 0, ADOC_BINNING, &binning) &&
            !AdocGetInteger(ADOC_ZVALUE, 0, ADOC_MAGIND, &magInd)) {
              camInd = mWinApp->LookupActiveCamera(ind);
              if (camInd >= 0) {
                cam = mWinApp->GetCamParams() + ind;
                binning *= BinDivisorI(cam);
                if (mWinApp->mStoreMRC->getWidth() * binning <= cam->sizeX &&
                  mWinApp->mStoreMRC->getHeight() * binning <= cam->sizeY)
                  param->binning = binning;
              }
          }
          AdocReleaseMutex();
        }

        // If that fails, fall back to old way: Compute a binning that fits camera size
        // find the biggest binning in the camera for which the image size
        // could still come from that camera
        // Start with current camera and then go through all active in order
        if (!param->binning) {
          for (ind = -1; ind < mWinApp->GetNumActiveCameras(); ind++) {
            camInd = ind < 0 ? mWinApp->GetCurrentActiveCamera() : ind;
            cam = mWinApp->GetCamParams() + activeList[camInd];
            for (binInd = cam->numBinnings - 1; binInd >= 0; binInd--) {
              if (mWinApp->mStoreMRC->getWidth() * cam->binnings[binInd] <= cam->sizeX && 
                mWinApp->mStoreMRC->getHeight() * cam->binnings[binInd] <= cam->sizeY) {
                  param->binning = cam->binnings[binInd];
                  magInd = mWinApp->mScope->GetMagIndex();
                  break;
              }
            }
            if (param->binning)
              break;
          }
        }

        // If a camera and binning are found, set flags and proceed
        if (param->binning) {
          param->cameraIndex = camInd;
          param->magIndex = magInd;
          param->settingsUsed = false;
          param->warnedBinChange = false;
          param->warnedMagChange = false;
          param->warnedCamChange = false;
          param->warnedCalOpen = false;
          param->warnedCalAcquire = false;
          param->warnedConSetBin = false;
          param->offerToMakeMap = false;

          // Set to current state or to proper state for low dose
          if (lowDose < 0) {
            param->setupInLowDose = mWinApp->LowDoseMode();
          } else {
            param->setupInLowDose = lowDose > 0;
            param->useViewInLowDose = lowDose > 0 && conSet == VIEW_CONSET;
            param->useSearchInLowDose = lowDose > 0 && conSet == SEARCH_CONSET;
          }

          // Use previous stage parameter or assert it if montage is above threshold
          if (stage >= 0)
            param->moveStage = stage > 0;
          else
            param->moveStage = FieldAboveStageMoveThreshold(param, lowDose > 0,
              activeList[camInd]); 
          
          OpenMontageDialog(true);
          ManageExposure();
          mWinApp->SetMontaging(true);
        } else
          AfxMessageBox("This is a montaged file but there is no\r"
          "camera that can produce such large images."
          "\r\rMontaging not activated", MB_EXCLAME); 

      } else if (err < 0)
        AfxMessageBox("This is a montaged file but the piece list is corrupted."
        "\r\rMontaging not activated", MB_EXCLAME); 

    } else {
      delete mWinApp->mStoreMRC;
      mWinApp->mStoreMRC = NULL;
    }
  }
  if (mWinApp->mStoreMRC == NULL) {
    AfxMessageBox("Error: Problems opening the selected file.", MB_EXCLAME);
    SwitchToFile(mCurrentStore);
    return MRC_OPEN_ERROR;
  } else
    AddCurrentStore();
  return MRC_OPEN_NOERR;
}

void CSerialEMDoc::OnUpdateFileOpenold(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mNumStores < MAX_STORES);
}

void CSerialEMDoc::OnFileMontagesetup() 
{
  bool locked;
  int xNframes, yNframes, xFrame, yFrame, xOverlap, yOverlap;
  MontParam *param = mWinApp->GetMontParam();
  if (mWinApp->Montaging()) {
    mBufferManager->CheckAsyncSaving();
    locked = mWinApp->mStoreMRC->getDepth() > 0;
    xNframes = param->xNframes;
    yNframes = param->yNframes;
    xFrame = param->xFrame;
    yFrame = param->yFrame;
    xOverlap = param->xOverlap;
    yOverlap = param->yOverlap;
    if (!OpenMontageDialog(locked)) {
      ManageExposure();
      if (!locked && (xNframes != param->xNframes || yNframes != param->yNframes ||
        xFrame != param->xFrame || yFrame != param->yFrame || 
        xOverlap != param->xOverlap || yOverlap != param->yOverlap)) {
        mWinApp->mMontageController->SetMontaging(false);
        mWinApp->mMontageController->SetMontaging(true);
      }
    }

  } else
    GetMontageParamsAndFile(false);

  mBufferWindow->UpdateSaveCopy();
}

void CSerialEMDoc::OnUpdateFileMontagesetup(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(/*!mWinApp->LowDoseMode() && */(
    (!mWinApp->DoingTasks() && mNumStores < MAX_STORES) || mWinApp->Montaging()));
}

void CSerialEMDoc::OnFileNewmontage() 
{
  GetMontageParamsAndFile(false);
  mBufferWindow->UpdateSaveCopy();
}

void CSerialEMDoc::OnUpdateFileNewmontage(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mNumStores < MAX_STORES);
}

// Main routine for getting montage parameters and opening a new file.
// frameSet indicates if the frame is already set up, in which case LeaveCurrentFile must
// be called before modifying frame parameters and calling this routine
// If frameset is false, the number of frames will be set from xNframes, yNframes, which
// default to -1.  If filename is not empty (the default), the file will be opened
// with all dialogs bypassed and calibration warnings will be suppressed
int CSerialEMDoc::GetMontageParamsAndFile(BOOL frameSet, int xNframes, int yNframes,
                                          CString filename)
{
  // Set up the montage parameters based on record parameters
  MontParam *param = mWinApp->GetMontParam();
  if (mBufferManager->CheckAsyncSaving())
    return 1;
  if (!frameSet)
    LeaveCurrentFile();
  InitMontParamsForDialog(param, frameSet, xNframes, yNframes, filename);
  
  if (filename.IsEmpty() && OpenMontageDialog(false)) {
    RestoreCurrentFile();
    return 1;
  }
  
  // Now open file
  SetFileOptsForSTEM();
  mFileOpt.mode = B3DMAX(mFileOpt.mode, 1);
  mFileOpt.typext &= ~MONTAGE_MASK;
  mFileOpt.montageInMdoc = false;
  if ((param->xNframes - 1) * (param->xFrame - param->xOverlap) > 65536 ||
    (param->yNframes - 1) * (param->yFrame - param->yOverlap) > 65536)
    mFileOpt.montageInMdoc = true;
  else
    mFileOpt.typext |= MONTAGE_MASK;
  if (param->moveStage)
    mFileOpt.typext |= VOLT_XY_MASK;
  mFileOpt.TIFFallowed = 0;
  if (filename.IsEmpty())
    mWinApp->mStoreMRC = OpenSaveFile(&mFileOpt);
  else
    mWinApp->mStoreMRC = OpenNewFileByName(filename, &mFileOpt);
  RestoreFileOptsFromSTEM();
  if (!mWinApp->mStoreMRC) {
    RestoreCurrentFile();
    return 1;
  }

  // If all is good, set up montaging in the controller
  ManageExposure();
  mWinApp->SetMontaging(true);
  AddCurrentStore();
  return 0;
}

// Routine for initializing the given montage parameters prior to running the dialog.
// frameSet indicates if the frame is already set up.
// If frameset is false, the number of frames will be set from xNframes, yNframes, which
// default to -1, in which case the default number of montage pieces are used
void CSerialEMDoc::InitMontParamsForDialog(MontParam *param, BOOL frameSet, int xNframes, 
                                           int yNframes, CString filename)
{
  int setNum = MontageConSetNum(param, false);
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  param->magIndex = mWinApp->mScope->GetMagIndex();
  if (mWinApp->LowDoseMode())
    param->magIndex = ldp[MontageLDAreaIndex(param)].magIndex;
  if (!frameSet) {
    MontParamInitFromConSet(param, setNum);
    param->xNframes = xNframes > 0 ? xNframes : mDefaultMontXpieces;
    param->yNframes = yNframes > 0 ? yNframes : mDefaultMontYpieces;
    param->numToSkip = 0;
  }
  param->zCurrent = 0;
  param->zMax = -1;
  param->cameraIndex = mWinApp->GetCurrentActiveCamera();
  param->settingsUsed = false;
  param->warnedBinChange = false;
  param->warnedMagChange = false;
  param->warnedCamChange = false;
  param->warnedConSetBin = false;
  param->ignoreSkipList = false;
  param->skipOutsidePoly = false;
  param->wasFitToPolygon = false;
  param->forFullMontage = false;
  param->setupInLowDose = mWinApp->LowDoseMode();
  param->insideNavItem = -1;
  param->warnedCalOpen = !filename.IsEmpty();
  param->warnedCalAcquire = !filename.IsEmpty();
  param->offerToMakeMap = frameSet;
  param->byteMinScale = 0.;
  param->byteMaxScale = 0.;
  if (param->anchorMagInd <= 0)
    param->anchorMagInd = mWinApp->mComplexTasks->FindMaxMagInd(mMinAnchorField, -2);

}

// Do the part of the initialization that depends on the control set
// setNum should be the conset number that will be used unless true conset has been synced
void CSerialEMDoc::MontParamInitFromConSet(MontParam *param, int setNum)
{
  int curCam = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + curCam;
  ControlSet *cs = mWinApp->GetConSets() + setNum;
  param->binning = cs->binning;
  if (CamHasDoubledBinnings(camParam) && cs->binning == 1)
    param->binning = 2;
  param->xFrame = (cs->right - cs->left) / param->binning;
  param->yFrame = (cs->bottom - cs->top) / param->binning;
  int maxSize = param->xFrame > param->yFrame ? param->xFrame : param->yFrame;
  param->xOverlap = (int)floor(mOverlapFraction * maxSize + 0.5);
  param->xOverlap += param->xOverlap % 2;
  param->yOverlap = param->xOverlap;

  // Set up to use stage if appropriate
  param->moveStage = FieldAboveStageMoveThreshold(param, mWinApp->LowDoseMode(), curCam);
}

// Tests whether montage is in LM or if a 2x2 montage will trip the maximum IS message
bool CSerialEMDoc::FieldAboveStageMoveThreshold(MontParam *param, BOOL lowDose, 
  int camInd)
{
  NavParams *navp = mWinApp->GetNavParams();
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  int magInd = param->magIndex;
  if (lowDose)
    magInd = ldp[MontageLDAreaIndex(param)].magIndex;
  return magInd < mWinApp->mScope->GetLowestNonLMmag() ||
    0.7 * (B3DMAX(param->xFrame, param->yFrame) - param->xOverlap) * 
    param->binning * mWinApp->mShiftManager->GetPixelSize(camInd, magInd) > 
    navp->maxMontageIS;
}

// Open the montage dialog: pass it the standard montage parameters, take them
// back if no cancel
int CSerialEMDoc::OpenMontageDialog(BOOL locked)
{
  LowDoseParams *ldp = mWinApp->GetLowDoseParams();
  CString *modeName = mWinApp->GetModeNames();
  CString mess;
  CMontageSetupDlg montDlg;
  MontParam *param = mWinApp->GetMontParam();
  montDlg.mParam = *param;
  if (mWinApp->LowDoseMode() && (!ldp[VIEW_CONSET].magIndex || 
    !ldp[RECORD_CONSET].magIndex)) {
      mess.Format("Low dose parameters need to be set up for both the %s\n"
        "and %s areas before starting a montage in Low Dose mode", 
        (LPCTSTR)modeName[RECORD_CONSET], (LPCTSTR)modeName[VIEW_CONSET]);
      SEMMessageBox(mess);
      return 1;
  }

  // Lock sizes if already montaging
  montDlg.mSizeLocked = locked;
  montDlg.mConstrainSize = mWinApp->Montaging() && 
    !mWinApp->mStoreMRC->montCoordsInAdoc();
  if (montDlg.DoModal() != IDOK)
    return 1;
  *param = montDlg.mParam;

  // 5/11/18: No longer set sloppy parameter by whether stage is being moved
  mWinApp->mMontageWindow.UpdateSettings();
  mWinApp->mLowDoseDlg.Update();
  return 0;
}

void CSerialEMDoc::ManageExposure()
{
  MontParam *param = mWinApp->GetMontParam();
  int *activeList = mWinApp->GetActiveCameraList();
  int camera = activeList[mWinApp->mMontageController->GetMontageActiveCamera(param)];
  int setNum = MontageConSetNum(param, false, param->setupInLowDose ? 1 : 0);
  ControlSet *cs = mWinApp->GetCamConSets() + camera * MAX_CONSETS + setNum;
  CString *modeName = mWinApp->GetModeNames();
  CameraParameters *camParam = mWinApp->GetCamParams() + camera;

  // If binning changed, set it in record set and adjust exposure at least
  // Note this overrides the drift-adjusting code in montage controller
  if (param->binning != cs->binning) {
     if (param->setupInLowDose) {
       float binDiv = BinDivisorF(camParam);
       PrintfToLog("WARNING: This montage was set up with binning %g, different from "
         "the binning (%g) in the %s parameter set\r\n"
         "   Because this is a low-dose montage, the parameter set will be left as it was,"
         "\r\n   and the exposure time there will be assumed to work with both binnings",
         param->binning / binDiv, cs->binning / binDiv, modeName[setNum]);
    } else {
      float ratio = (float)param->binning / cs->binning;
      cs->binning = param->binning;
      cs->exposure = B3DMAX(cs->exposure / (ratio * ratio), camParam->minExposure);
      SEMMessageBox("The binning in the " + modeName[setNum] 
        + " parameter set has been changed\n"
        "to match your selection, and the exposure time has been\n"
        "changed to give the same number of counts per binned pixel.\n\n"
        "You should now adjust the exposure and drift settling as\n"
        "needed to obtain good images.", MB_EXCLAME);
    }
    if (camera == mWinApp->GetCurrentCamera())
      mWinApp->CopyConSets(camera); 
  }

}

// Close file by deleting it and managing the store list
void CSerialEMDoc::OnFileClose() 
{
  DoCloseFile();
}

void CSerialEMDoc::DoCloseFile()
{
  if (mWinApp->mStoreMRC && mBufferManager->CheckAsyncSaving())
    return;
  if (mCurrentStore >= 0 && mStoreList[mCurrentStore].hasOpenTSfile)
    CFile::Remove(mStoreList[mCurrentStore].store->getFilePath() + OPEN_TS_EXT);
  delete mWinApp->mStoreMRC;
  mWinApp->mStoreMRC = NULL;

  // Turn off montaging and remove parameters
  if (mWinApp->Montaging()) {
    mWinApp->SetMontaging(false);
    delete mStoreList[mCurrentStore].montParam;
  }

  // pack the store list down
  mNumStores--;
  for (int i = mCurrentStore; i < mNumStores; i++)
    mStoreList[i] = mStoreList[i + 1];
  mCurrentStore--;
  ManageSaveSingle();

  // If there are other files still, fix store index if < 0 and switch to it
  if (mNumStores) {
    if (mCurrentStore < 0) 
      mCurrentStore = 0;
    SwitchToFile(mCurrentStore);
  } else
    mWinApp->SetTitleFile("");
  mWinApp->UpdateBufferWindows();
}

void CSerialEMDoc::OnUpdateFileClose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->mMacroProcessor->DoingMacro() &&
    mWinApp->mStoreMRC != NULL && mStoreList[mCurrentStore].protectNum < 0);  
}

// Efficiently close all the files for exiting
void CSerialEMDoc::CloseAllStores()
{
  for (int i = 0; i < mNumStores; i++) {
    if (mStoreList[i].montage)
      delete mStoreList[i].montParam;
    delete mStoreList[i].store;
  }
}

// Switches between Save single and Save to Other if there is an open file
void CSerialEMDoc::ManageSaveSingle(void)
{
  UtilModifyMenuItem(0, IDM_FILE_SAVEOTHER,
    mNumStores ? "Sa&ve to Other..." : "Sa&ve Single...");
}

// Regular save of regular buffer
void CSerialEMDoc::OnFileSave() 
{
  EMimageBuffer *imBuf = mWinApp->GetImBufs();
  KImageStore *store = GetStoreForSaving(imBuf->mImage->getType());
  if (!store)
    return;
  if (mBufferManager->CheckAsyncSaving())
    return;
  if (mBufferManager->GetAlignOnSave() && store->getDepth() > 0)
    mWinApp->mShiftManager->AutoAlign(0, 1);

  mBufferManager->SaveImageBuffer(store);
  mWinApp->UpdateBufferWindows();    // only status window needs it but...
  if (store->getStoreType() == STORE_TYPE_IMOD)
    delete store;
}

// Check whether regular file save can occur
void CSerialEMDoc::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
  BOOL bEnable = !mWinApp->DoingComplexTasks() && !mWinApp->Montaging() && 
    mBufferManager->IsBufferSavable(mWinApp->GetImBufs());
  pCmdUI->Enable(bEnable);
}

// Save of active view
void CSerialEMDoc::OnFileSaveactive() 
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  KImageStore *store = GetStoreForSaving(imBuf->mImage->getType());
  if (!store)
    return;

  // Save and restore buffer to save
  int bufOld = mBufferManager->GetBufToSave();
  mBufferManager->SetBufToSave(-1);
  mBufferManager->SaveImageBuffer(store);
  mBufferManager->SetBufToSave(bufOld);
  // Update buffer status window
  mWinApp->UpdateBufferWindows(); 
  if (store->getStoreType() == STORE_TYPE_IMOD)
    delete store;
}

// Returns the current store or a new one, which can be a TIFF store
KImageStore *CSerialEMDoc::GetStoreForSaving(int type)
{ 
  KImageStore *store;
  int modeSave;
  if (mBufferManager->CheckAsyncSaving())
    return NULL;
  if (mWinApp->mStoreMRC)
    return (KImageStore *)mWinApp->mStoreMRC;

  // If file not open, open it
  SetFileOptsForSTEM();
  mFileOpt.typext &= ~MONTAGE_MASK;
  mFileOpt.montageInMdoc = false;
  mFileOpt.TIFFallowed = 1;
  modeSave = mFileOpt.mode;
  if (type == kFLOAT)
    mFileOpt.mode = MRC_MODE_FLOAT;
  store = OpenSaveFile(&mFileOpt);
  if (type == kFLOAT)
    mFileOpt.mode = modeSave;
  RestoreFileOptsFromSTEM();
  if (!store)
    return NULL;
  if (store->getStoreType() == STORE_TYPE_MRC || 
    store->getStoreType() == STORE_TYPE_ADOC) {
    mWinApp->mStoreMRC = store;
    AddCurrentStore();
  }
  return store;
}

void CSerialEMDoc::OnUpdateFileSaveactive(CCmdUI* pCmdUI) 
{
  BOOL bEnable = !mWinApp->DoingComplexTasks() && !mWinApp->Montaging() && 
    mBufferManager->IsBufferSavable(mWinApp->mActiveView->GetActiveImBuf());
  pCmdUI->Enable(bEnable);
}

// Overwrite existing image in file
void CSerialEMDoc::OnFileOverwrite() 
{
  CString str;
  int numPiece = mWinApp->mMontageController->GetNumOverwritePieces();
  if (mBufferManager->CheckAsyncSaving())
    return;
  if (mWinApp->Montaging()) {
    str.Format("Enter %d or higher to start the next montage from the beginning even"
      " if it is resumable", mWinApp->mMontageController->GetNumPieces());
    if (KGetOneInt("Number of pieces to overwrite if the next montage is resumable:", 
      numPiece))
      mWinApp->mMontageController->SetNumOverwritePieces(numPiece);
    return;
  }
  if (mBufferManager->GetAlignOnSave() && mWinApp->mStoreMRC->getDepth() > 0)
    mWinApp->mShiftManager->AutoAlign(0, 1);
  mBufferManager->OverwriteImage(mWinApp->mStoreMRC); 
  mWinApp->UpdateBufferWindows();
}

void CSerialEMDoc::OnUpdateFileOverwrite(CCmdUI* pCmdUI) 
{
  MontParam *montp = mWinApp->GetMontParam();
  BOOL bEnable = !mWinApp->DoingTasks() && !mWinApp->DoingComplexTasks() && 
    mWinApp->mStoreMRC && (!mWinApp->Montaging() || montp->moveStage);
  if (bEnable)
    bEnable = mWinApp->mStoreMRC->getDepth() > 0 && (mWinApp->Montaging() ||
      mBufferManager->IsBufferSavable(mWinApp->GetImBufs()));
  pCmdUI->Enable(bEnable);
}

// Save active view into another file
void CSerialEMDoc::OnFileSaveother() 
{
  SaveToOtherFile(-1, -1, -1, NULL);
}

// Saves a single image in given buffer to a file of the specified type
// Returns 1 on no image or async save error, 2 on fail to open, 3 on fail to write
int CSerialEMDoc::SaveToOtherFile(int buffer, int fileType, int compression, 
  CString *fileName)
{
  int compSave, typeSave, modeSave, err;
  KImageStore *store;
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  if (buffer >= 0)
    imBuf = mWinApp->GetImBufs() + buffer;
  modeSave = mOtherFileOpt.mode;
  typeSave = mOtherFileOpt.fileType;
  compSave = mOtherFileOpt.compression;
  mOtherFileOpt.TIFFallowed = 1;
  if (mBufferManager->CheckAsyncSaving() || !imBuf->mImage)
    return 1;

  // Save float buffer as float, copy in type and compression if specified
  if (imBuf->mImage->getType() == kFLOAT)
    mOtherFileOpt.mode = MRC_MODE_FLOAT;
  if (fileType >= 0)
    mOtherFileOpt.fileType = fileType;
  if (compression >= 0)
    mOtherFileOpt.compression = compression;

  // If filename passed in, turn an ADOC into a single tiff, open file and restore things
  if (fileName) {
   if (mOtherFileOpt.fileType == STORE_TYPE_ADOC)
      mOtherFileOpt.fileType = STORE_TYPE_TIFF;
    store = OpenNewFileByName(*fileName, &mOtherFileOpt);
    mOtherFileOpt.fileType = typeSave;
    mOtherFileOpt.compression = compSave;
  } else {

    // Otherwise, regular file options and file chooser route
    store = OpenSaveFile(&mOtherFileOpt);
  }
  if (imBuf->mImage->getType() == kFLOAT)
    mOtherFileOpt.mode = modeSave;
  if (!store || !store->FileOK()) {
    delete store;
    return 2;
  }

  // Save and restore buffer to save and copy on save
  int bufOld = mBufferManager->GetBufToSave();
  int copyOld = mBufferManager->GetCopyOnSave();
  mBufferManager->SetBufToSave(buffer);
  mBufferManager->SetCopyOnSave(0);
  mWinApp->SetSavingOther(true);

  err = mBufferManager->SaveImageBuffer(store);
  delete store;
  mWinApp->SetSavingOther(false);
  mBufferManager->SetBufToSave(bufOld);
  mBufferManager->SetCopyOnSave(copyOld);
  mWinApp->UpdateBufferWindows(); 
  return err ? 3 : 0;
}

void CSerialEMDoc::OnUpdateFileSaveother(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBuf = mWinApp->mActiveView->GetActiveImBuf();
  pCmdUI->Enable(!mWinApp->DoingTasks() && imBuf->mImage); 
}

void CSerialEMDoc::OnFileTruncation() 
{
  float number = mFileOpt.pctTruncLo;
  if (KGetOneFloat("Percent of pixels to truncate as black:", number, 2)) {
    if (number > mMaxTrunc)
        number = mMaxTrunc;
  } else
    return;
  if (KGetOneFloat("Percent of pixels to truncate as white:", mFileOpt.pctTruncHi, 2)) {
    if (mFileOpt.pctTruncHi > mMaxTrunc)
      mFileOpt.pctTruncHi = mMaxTrunc;
    mFileOpt.pctTruncLo = number;
    mWinApp->mStoreMRC->SetTruncation(mFileOpt.pctTruncLo, mFileOpt.pctTruncHi);
  }
}

void CSerialEMDoc::OnUpdateFileTruncation(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mWinApp->mStoreMRC != NULL && mFileOpt.mode ==0);  
}

// Change the treatment of unsigned integers, needed if file is reopened
void CSerialEMDoc::OnFileSet16bitpolicy() 
{
  SetFileOptsForSTEM();
  if (!KGetOneInt("Enter 0 to truncate unsigned integers at 32767, 1 to divide by 2, "
    "2 to subtract 32768:", mFileOpt.unsignOpt))
    return;
  B3DCLAMP(mFileOpt.unsignOpt, 0, 2);
  mWinApp->mStoreMRC->SetUnsignedOption(mFileOpt.unsignOpt);
  RestoreFileOptsFromSTEM();
}

// Enable the option if there are any 16-bit cameras and not dividing by 2, or
// if there are any unsigned buffers even if a file is not open
void CSerialEMDoc::OnUpdateFileSet16bitpolicy(CCmdUI* pCmdUI) 
{
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  BOOL bEnable = mWinApp->mStoreMRC && mWinApp->GetAny16BitCameras() && 
    mWinApp->mCamera->GetDivideBy2() <= 0;
  for (int i = 0; i < MAX_BUFFERS && !bEnable; i++)
    bEnable = imBufs[i].mImage != NULL && imBufs[i].mImage->getType() == kUSHORT;
  pCmdUI->Enable(bEnable);
}

// Change the treatment of signed data saved to unsigned files
void CSerialEMDoc::OnFileSetsignedpolicy() 
{
  if (!KGetOneInt("Enter 0 to truncate negative numbers at 0, or 1 to "
    "add 32768, when saving to unsigned mode file:", mFileOpt.signToUnsignOpt))
    return;
  B3DCLAMP(mFileOpt.signToUnsignOpt, 0, 1);
  mWinApp->mStoreMRC->SetSignedOption(mFileOpt.signToUnsignOpt);
}

// Enable if there is a currently open file 
void CSerialEMDoc::OnUpdateFileSetsignedpolicy(CCmdUI* pCmdUI) 
{
  BOOL bEnable = mWinApp->mStoreMRC && mWinApp->mStoreMRC->getMode() == MRC_MODE_USHORT;
  pCmdUI->Enable(bEnable);
}

// Toggle skipping through the file properties dialog
void CSerialEMDoc::OnSkipFilePropertiesDlg()
{
  mSkipFileDlg = !mSkipFileDlg;
}

void CSerialEMDoc::OnUpdateSkipFilePropertiesDlg(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mSkipFileDlg);
}

// Reading from either the current file or a random file
void CSerialEMDoc::OnFileRead() 
{
  MontParam *param = mWinApp->GetMontParam();
  int section;
  if (mBufferManager->CheckAsyncSaving())
    return;
  
  // If no current file is open, treat this like reading other file
  if (!mWinApp->mStoreMRC) {
    OnFileReadother();
    return;
  }
  section = mWinApp->Montaging() ? param->zMax : (mWinApp->mStoreMRC->getDepth() - 1);

  // If only one section, go off and read it
  if (!section) {
    if (mWinApp->Montaging())
      mWinApp->mMontageController->ReadMontage(0);
    else
      OnFileReadpiece();
    return;
  }

  // Otherwise raise or open the file read dialog
  if (mReadFileDlg) {
    mReadFileDlg->BringWindowToTop();
    return;
  }
  mReadFileDlg = new CReadFileDlg();
  mReadFileDlg->m_iSection = section;
  mReadFileDlg->Create(IDD_READFILEDLG);
  if (mReadDlgPlace.rcNormalPosition.right > 0)
    mWinApp->SetPlacementFixSize(mReadFileDlg, &mReadDlgPlace);
  else
    mReadFileDlg->SetWindowPos(&CWnd::wndTopMost, 500, 400, 100, 100, 
      SWP_NOSIZE | SWP_SHOWWINDOW);
}

WINDOWPLACEMENT * CSerialEMDoc::GetReadDlgPlacement(void)
{
  if (mReadFileDlg)
    mReadFileDlg->GetWindowPlacement(&mReadDlgPlace);
  return &mReadDlgPlace;
}

void CSerialEMDoc::OnUpdateFileRead(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && !mWinApp->mCamera->CameraBusy() && 
    (!mWinApp->mStoreMRC || mWinApp->mStoreMRC->getDepth() > 0));
}

// This will read a single frame from a file, montaged or not
void CSerialEMDoc::OnFileReadpiece() 
{
  mBufferManager->ReadFromFile(mWinApp->mStoreMRC);
  mWinApp->DrawReadInImage();
}

// Condition for reading a piece: montage file with data in it
void CSerialEMDoc::OnUpdateFileReadpiece(CCmdUI* pCmdUI) 
{
  MontParam *param = mWinApp->GetMontParam();
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->Montaging() && param->zMax >= 0 &&
    !mWinApp->mCamera->CameraBusy());
}

void CSerialEMDoc::OnFileReadother() 
{
  int err = mBufferManager->ReadOtherFile();
  if (err != READ_MONTAGE_OK) {
    mWinApp->SetImBufIndex(mBufferManager->GetBufToReadInto());
    mWinApp->mMainView->SetCurrentBuffer(mBufferManager->GetBufToReadInto());
  }
  mWinApp->RestoreViewFocus();
}

void CSerialEMDoc::OnUpdateFileReadother(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingTasks() && mWinApp->mStoreMRC != NULL && 
    !mWinApp->mCamera->CameraBusy()); 
}

// Open a new file to save into
KImageStore * CSerialEMDoc::OpenSaveFile(FileOptions *fileOptp)
{
  int err;
  CString cFilename;
  if (FilePropForSaveFile(fileOptp, false)) {
    mWinApp->RestoreViewFocus();
    return NULL;
  }

  err = FilenameForSaveFile(fileOptp->useMont() ? 
    fileOptp->montFileType : fileOptp->fileType, NULL, cFilename);
  mWinApp->RestoreViewFocus();
  if (err)
    return NULL;
  
  return OpenNewFileByName(cFilename, fileOptp);
}

// Runs the file properties dialog with the given options, returns -1 if user cancels
int CSerialEMDoc::FilePropForSaveFile(FileOptions * fileOptp, bool openAnyway)
{
  EMimageBuffer *imBufs = mWinApp->GetImBufs();

  // Get file property dialog, load properties, and do dialog
  // Enable the 16-bit options if there is a camera that can make 16-bit or if
  // there is an unsigned short buffer
  CFilePropDlg propDlg;
  propDlg.mFileOpt = *fileOptp;
  propDlg.mShowDlgThisTime = mShowFileDlgOnce || openAnyway;
  propDlg.m_bSkipFileDlg = mSkipFileDlg;
  mShowFileDlgOnce = false;
  propDlg.mAny16Bit = mWinApp->GetAny16BitCameras() && 
    (mWinApp->mCamera->GetDivideBy2() <= 0);
  for (int i = 0; i < MAX_BUFFERS && !propDlg.mAny16Bit; i++)
    propDlg.mAny16Bit = imBufs[i].mImage && imBufs[i].mImage->getType() == kUSHORT;
  
  // Reset the mode to 1 if the unsigned mode option will not be available
  if (!propDlg.mAny16Bit && propDlg.mFileOpt.mode == MRC_MODE_USHORT)
    propDlg.mFileOpt.mode = 1;
  if (propDlg.DoModal() != IDOK)
    return -1;

  // unload properties from the dialog even if the rest of this aborts
  *fileOptp = propDlg.mFileOpt;
  mSkipFileDlg = propDlg.m_bSkipFileDlg;
  return 0;
}

// If in STEM mode, save out the nonSTEM options and copy in STEM options
void CSerialEMDoc::SetFileOptsForSTEM(void)
{
  mSavedNonSTEMprops = mWinApp->GetSTEMMode();
  if (mSavedNonSTEMprops) {
    mNonSTEMfileMode = mFileOpt.mode;
    mNonSTEMunsignOpt = mFileOpt.unsignOpt;
    mFileOpt.mode = mSTEMfileMode;
    mFileOpt.unsignOpt = mSTEMunsignOpt;
  }
}

// Restore nonSTEM options if they were saved and save out the STEM choices
void CSerialEMDoc::RestoreFileOptsFromSTEM(void)
{
  if (mSavedNonSTEMprops) {
    mSTEMfileMode = mFileOpt.mode;
    mSTEMunsignOpt = mFileOpt.unsignOpt;
    mFileOpt.mode = mNonSTEMfileMode;
    mFileOpt.unsignOpt = mNonSTEMunsignOpt;
  }
}

// Copy file options as appropriate for STEM mode or not
void CSerialEMDoc::CopyMasterFileOpts(FileOptions *fileOptp, int fromTo)
{
  SetFileOptsForSTEM();
  if (fromTo == COPY_FROM_MASTER)
    *fileOptp = mFileOpt;
  else
    mFileOpt = *fileOptp;
  RestoreFileOptsFromSTEM();
}

// Get a save file name with filters/extension set from the fileType member of file
// fileOpts and with given starting filename (NULL for none), returns -1 if user cancels
int CSerialEMDoc::FilenameForSaveFile(int fileType, LPCTSTR lpszFileName, 
                                      CString & cFilename)
{
  static char BASED_CODE szFilter[] = 
    "MRC image stacks (*.mrc, *.st, *.map)|*.mrc; *.st; *.map|All files (*.*)|*.*||";
  static char BASED_CODE tiffFilter[] = 
    "TIFF files (*.tif)|*.tif|All files (*.*)|*.*||";
  static char BASED_CODE jpegFilter[] = 
    "JPEG files (*.jpg)|*.jpg|All files (*.*)|*.*||";
  static char BASED_CODE adocFilter[] = 
    "Image Autodoc files (*.idoc)|*.idoc|All files (*.*)|*.*||";
  char *mrcExceptions[] = {".st", ".map"};
  char *extensions[] = {".tif", ".idoc", ".mrc", ".jpg"};
  char *filter = &szFilter[0];

  // If a filename is supplied, do not add an extension to it
  char *ext = lpszFileName ? NULL : extensions[2];
  int len, numExceptions = sizeof(mrcExceptions) / sizeof(char *);
  CString tail;
  if (fileType == STORE_TYPE_TIFF) {
    filter = &tiffFilter[0];
    ext = extensions[0];
  } else if (fileType == STORE_TYPE_ADOC) {
    filter = &adocFilter[0];
    ext = extensions[1];
  } else if (fileType == STORE_TYPE_JPEG) {
    filter = &jpegFilter[0];
    ext = extensions[3];
  }

  // It will attach the default extension unless the user enteres a RECOGNIZED extension
  MyFileDialog fileDlg(FALSE, ext, lpszFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
    filter);
  if (fileDlg.DoModal() != IDOK)
    return -1;
  
  // Thus strip the .mrc if one of the exceptions is entered
  cFilename = fileDlg.GetPathName();
  if (fileType == STORE_TYPE_MRC && ext) {
    for (int ind = 0; ind < numExceptions; ind++) {
      tail = CString(mrcExceptions[ind]) + ".mrc";
      len = tail.GetLength();
      if (cFilename.GetLength() > len && cFilename.Right(len) == tail) {
        cFilename = cFilename.Left(cFilename.GetLength() - 4);
        break;
      }
    }
  }
  return 0;
}


// Open a new file given the name and file options
KImageStore *CSerialEMDoc::OpenNewFileByName(CString cFilename, FileOptions *fileOptp)
{
  MontParam *param = mWinApp->GetMontParam();
  KImageStore *store;
  if (FileAlreadyOpen(cFilename, 
    "A new file was not opened; you need to close the existing one first."))
    return NULL;

  if (mBufferManager->CheckAsyncSaving())
    return NULL;

  // Set up title
  char title[80];
  MakeSerialEMTitle(mTitle, title);

  if (fileOptp->TIFFallowed && (fileOptp->fileType == STORE_TYPE_TIFF ||
    fileOptp->fileType == STORE_TYPE_JPEG)) {

    // TIFF file
    KStoreIMOD *tiffStore = new KStoreIMOD(cFilename, *fileOptp);
    store = (KImageStore *)tiffStore;
    if (fileOptp->fileType == STORE_TYPE_TIFF)
      store->AddTitle(title);
    
  } else {
    
    if ((fileOptp->useMont() ? fileOptp->montFileType : fileOptp->fileType)
      == STORE_TYPE_ADOC) {

      // ADOC series
      KStoreADOC *storeADOC = new KStoreADOC(cFilename, *fileOptp);
      store = (KImageStore *)storeADOC;
    } else {

      // MRC file
      // Need to adjust maximum number of frames by the montage size
      // and adjust it back after using it to prevent persistence
      if (fileOptp->isMontage())
        fileOptp->maxSec *= param->xNframes * param->yNframes;

      KStoreMRC *storeMRC = new KStoreMRC(cFilename, *fileOptp);
      if (fileOptp->isMontage())
        fileOptp->maxSec /= param->xNframes * param->yNframes;
      store = (KImageStore *)storeMRC;
    }

    // Test if OK, etc
    if (store == NULL || !store->FileOK()) {
      AfxMessageBox("Error Opening File", MB_EXCLAME);
      mWinApp->RestoreViewFocus();
      if (store)
        delete store;
      return NULL;
    }

    store->AddTitle(title);
    mBufferWindow->UpdateSaveCopy();
  }
  return store;
}

static char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
            "Aug", "Sep", "Oct", "Nov", "Dec"};

// Return a string based on the date and time
CString CSerialEMDoc::DateTimeForFrameSaving(void)
{
  CString date, time;
  DateTimeComponents(date, time);
  return date + "_" + time;
}

// Return the two components for date-time based filenames
void CSerialEMDoc::DateTimeComponents(CString &date, CString &time)
{
  CTime ctDateTime = CTime::GetCurrentTime();
  date.Format("%s%02d", months[ctDateTime.GetMonth() - 1], ctDateTime.GetDay());
  time.Format("%02d.%02d.%02d", ctDateTime.GetHour(), ctDateTime.GetMinute(), 
    ctDateTime.GetSecond());
}

// Return the date-time string for title format
CString CSerialEMDoc::DateTimeForTitle(void)
{
  CString str;
  CTime ctDateTime = CTime::GetCurrentTime();
  str.Format("%02d-%s-%02d  %02d:%02d:%02d", ctDateTime.GetDay(), 
    months[ctDateTime.GetMonth() - 1], ctDateTime.GetYear() % 100,
    ctDateTime.GetHour(), ctDateTime.GetMinute(), ctDateTime.GetSecond());
  return str;
}

// Compose a full title string with the given one contained
void CSerialEMDoc::MakeSerialEMTitle(CString &titleStr, char *fullTitle)
{
  CString str = DateTimeForTitle();
  sprintf(fullTitle, "SerialEM: %-45s %s    ", (LPCTSTR)titleStr, (LPCTSTR)str);
}


// Public access to the file save functions
void CSerialEMDoc::SaveRegularBuffer()
{
  OnFileSave();
}

void CSerialEMDoc::SaveActiveBuffer()
{
  OnFileSaveactive();
}

void CSerialEMDoc::OnUpdateWindowNew(CCmdUI* pCmdUI) 
{
  BOOL bEnable = (mWinApp->mActiveView->GetActiveImBuf()->mImage != NULL);
  pCmdUI->Enable(bEnable);      
}

// MULTIPLE FILE ROUTINES
//
// Copy data from current file to the list before leaving it
void CSerialEMDoc::LeaveCurrentFile()
{
  MontParam *param = mWinApp->GetMontParam();
  if (mCurrentStore < 0)
    return;
  CopyMasterFileOpts(&mStoreList[mCurrentStore].fileOpt, COPY_FROM_MASTER);
  if (mWinApp->Montaging()) {
    mWinApp->SetMontaging(false);
    *mStoreList[mCurrentStore].montParam = *param;
  }
}

// Set a file on the list as the current file; or ignore for -1
void CSerialEMDoc::SwitchToFile(int which)
{
  if (which < 0)
    return;
  MontParam *param = mWinApp->GetMontParam();
  mCurrentStore = which;
  mWinApp->mStoreMRC = mStoreList[which].store;
  CopyMasterFileOpts(&mStoreList[which].fileOpt, COPY_TO_MASTER);
  if (mStoreList[which].montage) {
    *param = *mStoreList[which].montParam;
    mWinApp->SetMontaging(true);

    // 7/19/17: Removed turning off low dose if camera "not feasible": inappropriate when
    // accessing a map file and irrelevant
    mWinApp->mMontageWindow.UpdateSettings();
  }
  ComposeTitlebarLine();
  mWinApp->UpdateBufferWindows();
}

// Add the current image file to the store list
void CSerialEMDoc::AddCurrentStore()
{
  MontParam *param = mWinApp->GetMontParam();
  StoreData *file = &mStoreList[mNumStores++];
  mCurrentStore = mNumStores - 1;
  file->store = mWinApp->mStoreMRC;
  CopyMasterFileOpts(&file->fileOpt, COPY_FROM_MASTER);
  file->montage = mWinApp->Montaging() != 0;
  file->protectNum = -1;
  file->saveOnNewMap = -1;
  file->hasOpenTSfile = false;
  file->montParam = NULL;
  if (file->montage) {
    file->montParam = new MontParam;
    *file->montParam = *param;
  }
  ComposeTitlebarLine();
  mWinApp->UpdateBufferWindows();
  ManageSaveSingle();
}

// Switch to a different file from an existing one
void CSerialEMDoc::SetCurrentStore(int which)
{
  if (which == mCurrentStore)
    return;
  LeaveCurrentFile();
  SwitchToFile(which);
}

KImageStore * CSerialEMDoc::GetStoreMRC(int which)
{
  if (which < 0 || which >= mNumStores)
    return NULL;
  return mStoreList[which].store;
}


MontParam * CSerialEMDoc::GetStoreMontParam(int which)
{
  if (which < 0 || which >= mNumStores || !mStoreList[which].montage)
    return NULL;
  return mStoreList[which].montParam;
}


BOOL CSerialEMDoc::StoreIsMontage(int which)
{
  if (which < 0 || which >= mNumStores)
    return false;
  return mStoreList[which].montage;
}

// Restore current file after aborting definition of a new one
void CSerialEMDoc::RestoreCurrentFile()
{
  SwitchToFile(mCurrentStore);
}

// Protect this file from being closed and also create a ...openTS file
void CSerialEMDoc::ProtectStore(int which)
{
  FILE *fp;
  mStoreList[which].protectNum = which;
  if (!mStoreList[which].hasOpenTSfile) {
    fp = fopen((LPCTSTR)(mStoreList[which].store->getFilePath() + OPEN_TS_EXT), "w");
    if (fp) {
      fclose(fp);
      mStoreList[which].hasOpenTSfile = true;
    }
  }
}

// Remove protection flag for one file, or for all if which < 0 (default)
void CSerialEMDoc::EndStoreProtection(int which)
{
  for (int i = 0; i < mNumStores; i++)
    if (which < 0 || mStoreList[i].protectNum == which)
      mStoreList[i].protectNum = -1;
}

// Find actual file number of a protected file
int CSerialEMDoc::LookupProtectedStore(int which)
{
  for (int i = 0; i < mNumStores; i++) {
    if (mStoreList[i].protectNum == which)
      return i;
  }
  return -1;
}

// Switch to a file previously protected; look up the original number
int CSerialEMDoc::SetToProtectedStore(int which)
{
  for (int i = 0; i < mNumStores; i++) {
    if (mStoreList[i].protectNum == which) {
      SetCurrentStore(i);
      return 0;
    }
  }
  return 1;
}

// Check if filename is already in the list and give error message if so
bool CSerialEMDoc::FileAlreadyOpen(CString filename, CString message)
{
  CString str;
  int i = StoreIndexFromName(filename);
  if (i < 0)
    return false;
  str.Format("This file is already open (image file #%d).\n\n%s", i+1, (LPCTSTR)message);
  AfxMessageBox(str, MB_EXCLAME);
  return true;
}

// Look for a name in list and return its index, or -1 if none
int CSerialEMDoc::StoreIndexFromName(CString name)
{
  for (int i = 0; i < mNumStores; i++) {

    // Special case, the current store is being replaced and it has been closed; skip it
    if (i == mCurrentStore && !mWinApp->mStoreMRC)
      continue;
    if (!name.CompareNoCase(mStoreList[i].store->getName()))
      return i;
  }
  return -1;
}

// Make a title with all the open filenames in it
void CSerialEMDoc::ComposeTitlebarLine(void)
{
  CString dir, filename, title;
  if (!mNumStores || !mWinApp->mStoreMRC) {
    mWinApp->SetTitleFile("");
    return;
  }
  UtilSplitPath(mWinApp->mStoreMRC->getName(), dir, title);
  mWinApp->m_strTitle = title;
  if (mNumStores > 1) {
    dir.Format(" (#%d)", mCurrentStore + 1);
    title += dir;
    for (int ind = 0; ind < mNumStores; ind++) {
      if (ind != mCurrentStore) {
        UtilSplitPath(mStoreList[ind].store->getName(), dir, filename);
        dir.Format("   %d: ", ind + 1);
        title += dir + filename;
      }
    }
  }
  mWinApp->SetTitleFile(title);
}

// Set the save on new map member in the current store
void CSerialEMDoc::SetSaveOnNewMap(int inVal)
{
  if (mCurrentStore >= 0)
    mStoreList[mCurrentStore].saveOnNewMap = inVal;
}

// Save a buffer to file; bufNum -1 for current save buffer, fileNum -1 for current file
// Appends to file with inSect = -1 ( default value), or writes indicated section
int CSerialEMDoc::SaveBufferToFile(int bufNum, int fileNum, int inSect)
{
  int err;
  CString str;
  EMimageBuffer *imBufs = mWinApp->GetImBufs();
  int origBuf = mBufferManager->GetBufToSave();
  int useBuf = bufNum < 0 ? origBuf : bufNum;
  char letter = 'A' + (char)useBuf;
  int useFile = fileNum < 0 ? mCurrentStore : fileNum;
  if (mBufferManager->CheckAsyncSaving())
    return 1;
  if (useBuf < 0 || useBuf >= MAX_BUFFERS) {
    mWinApp->mTSController->TSMessageBox(
      "Requested buffer number to save is out of range");
    return 1;
  }
  if (useFile < 0 || useFile >= mNumStores || mStoreList[useFile].montage) {
    str.Format("Requested file number to save into, %d,\nis out of range or refers to"
      " a montage", useFile + 1);
    mWinApp->mTSController->TSMessageBox(str);
    return 1;
  }
  if (!imBufs[useBuf].mImage || (mStoreList[useFile].store->getWidth() && 
    (imBufs[useBuf].mImage->getWidth() != mStoreList[useFile].store->getWidth() ||
    imBufs[useBuf].mImage->getHeight() != mStoreList[useFile].store->getHeight()))) {
    str.Format("Buffer %c either has no image in it or cannot be saved to file #%d",
      letter, useFile + 1);
    mWinApp->mTSController->TSMessageBox(str);
    return 1;
  }

  // Set saving other flag if it is not the current file - this makes it work right
  // when montaging and prevents copy on save
  mWinApp->SetSavingOther(useFile != mCurrentStore);
  if (bufNum >= 0)
    mBufferManager->SetBufToSave(bufNum);
  err = mBufferManager->SaveImageBuffer(mStoreList[useFile].store, false, inSect);
  if (bufNum >= 0)
    mBufferManager->SetBufToSave(origBuf);
  mWinApp->SetSavingOther(false);
  return err;
}


/////////////////////////////////////////////////////////////////////////////
// settings and properties file IO

// Initial read of all files
void CSerialEMDoc::ReadSetPropCalFiles()
{
  int err, trial;
  mSettingsReadable = false;
  mSettingsOpen = false;
  mSysSettingsReadable = false;
  mParamIO = mWinApp->mParamIO;
  CFileStatus status;
  mCurrentSettingsPath = "";
  mOriginalCwd = "";
  CString strSys, firstSys;

  // Adjust the system path for shared application data for Windows Vista/7
  if (IsVersion(6, VER_GREATER_EQUAL, 0, VER_GREATER_EQUAL))
    mSystemPath = "C:\\ProgramData\\SerialEM";
  defaultSysPath = _strdup((LPCTSTR)mSystemPath);
  mSysPathForSettings = mSystemPath;

  // Get the current directory, then try to load local settings
  char *cwd = _getcwd(NULL, _MAX_PATH);
  if (cwd) {
    CString strCwd = cwd;
    
    // append \ if necessary
    if (strCwd.GetAt(strCwd.GetLength() - 1) != '\\')
      strCwd += '\\';

    // compose full filename and get status
    mCurrentSettingsPath = strCwd + mSettingsName;
    if (CFile::GetStatus((LPCTSTR)mCurrentSettingsPath, status)) {
      
      // If OK, try to read file: if get -1, only system path was read
      err = mParamIO->ReadSettings(mCurrentSettingsPath);
      mSettingsReadable = !err;
      if (err > 0)
        AfxMessageBox("Error reading local settings file", MB_EXCLAME);
    }
    mOriginalCwd.Format("%s", cwd);
    free(cwd);
    mSettingsOpen = true;
  } else
    AfxMessageBox("Error getting current working directory", MB_EXCLAME);

  if (mSettingsReadable)
    mRecentSettings->Add((LPCTSTR)mCurrentSettingsPath);

  // Find out status of system file
  strSys = mSystemPath + "\\" + mSystemSettingsName;
  mSysSettingsReadable = CFile::GetStatus((LPCTSTR)strSys, status);

  // If local file not read in full, try for system file
  if (!mSettingsReadable && mSysSettingsReadable) {
    if (mParamIO->ReadSettings(strSys)) {
      AfxMessageBox("Error reading system settings file", MB_EXCLAME);
      mSysSettingsReadable = false;
    }
  }

  // Give message about results
  if (!mSettingsReadable && mSysSettingsReadable)
    AfxMessageBox("Settings read from system default file, not local file", MB_EXCLAME);
  else if (!mSettingsReadable)
    AfxMessageBox("Neither system default nor local settings file found", MB_EXCLAME);

  // Read properties; but first set gain reference path to system path
  // Try the current path which may have come from settings file, then try the
  // default path in case it has changed
  for (trial = 0; trial < 2; trial++) {
    mWinApp->mGainRefMaker->SetRefPath(mSystemPath);
    strSys = mSystemPath + "\\" + mPropertiesName;
    if (!trial)
      firstSys = strSys;
    if (CFile::GetStatus((LPCTSTR)strSys, status)) {
      if (mParamIO->ReadProperties(strSys)) {
        if (trial)
          AfxMessageBox("Error reading properties file", MB_EXCLAME);
      } else
        break;
    } else if (trial)
      AfxMessageBox("No properties file found", MB_EXCLAME);
    mSystemPath = defaultSysPath;

    // 12/5/14: Stop doing this.  Report the problem and leave it
    //mSysPathForSettings = mSystemPath;
  }
  if (trial == 1) {
    AfxMessageBox("Because of problems reading properties from " + firstSys + ",\n"
      "properties were read from " + strSys + " instead\n\n"
      "You should either fix that first property file,\n"
      "or stop trying to access that file by changing the\n"
      "SystemPath entry in " + mCurrentSettingsPath + "\n"
      "or by changing the properties in the shortcut used to start SerialEM", MB_EXCLAME);
  }

  // Set the file options from defaults; set other file options EXCEPT the managed ones
  mFileOpt = mDefFileOpt;
  CopyDefaultToOtherFileOpts();
    
  // Read calibrations
  strSys = mSystemPath + "\\" + mCalibrationName;
  if (CFile::GetStatus((LPCTSTR)strSys, status)) {
    if (mParamIO->ReadCalibration(strSys))
      AfxMessageBox("Error reading calibration file", MB_EXCLAME);
  } else
    AfxMessageBox("No calibration file found", MB_EXCLAME);

  // Read short-term calibrations unless ignoring them
  strSys = mSystemPath + "\\" + mShortTermName;
  if (CFile::GetStatus((LPCTSTR)strSys, status)) {
    if (mParamIO->ReadShortTermCal(strSys, mIgnoreShortTerm))
      AfxMessageBox("Error reading short-term calibration file", MB_EXCLAME);
  }

  // Read flyback times if they exist
  strSys = CString(defaultSysPath) + "\\" + mFlybackName;
  if (CFile::GetStatus((LPCTSTR)strSys, status)) {
    if (mParamIO->ReadFlybackTimes(strSys))
      AfxMessageBox("Error reading flyback time file", MB_EXCLAME);
  }

  // Once calibrations are read, need to reset the IS offsets
  mWinApp->mScope->SetApplyISoffset(mWinApp->mScope->GetApplyISoffset());
}

// Set the flag for whether to use an mdoc or not and copy to file options
void CSerialEMDoc::SetDfltUseMdoc(int value) 
{
  mDfltUseMdoc = value; 
  mDefFileOpt.useMdoc = value > 0;
  mFileOpt = mDefFileOpt;
  CopyDefaultToOtherFileOpts();
};

// Save managed members in other options, copy from default and restore them
void CSerialEMDoc::CopyDefaultToOtherFileOpts(void)
{
  int typeSave, compSave;
  float loSave, hiSave;
  typeSave = mOtherFileOpt.fileType;
  compSave = mOtherFileOpt.compression;
  loSave = mOtherFileOpt.pctTruncLo;
  hiSave = mOtherFileOpt.pctTruncHi;
  mOtherFileOpt = mDefFileOpt;
  mOtherFileOpt.fileType = typeSave;
  mOtherFileOpt.compression = compSave;
  mOtherFileOpt.pctTruncLo = loSave;
  mOtherFileOpt.pctTruncHi = hiSave;
}

// Set system path from line in settings file: save it as the path that will be rewritten,
// then modify from command line if possible
void CSerialEMDoc::SetSystemPath(CString sysPath)
{
  CString strSys, sub = mWinApp->GetSysSubpath();
  CFileStatus status;
  mSystemPath = sysPath;
  mSysPathForSettings = sysPath;

  // See if a command line argument can modify the system path; look for properties file
  if (!sub.IsEmpty()) {
    strSys = mSystemPath + "\\" + sub + "\\" + mPropertiesName;
    if (CFile::GetStatus((LPCTSTR)strSys, status))
      mSystemPath += "\\" + sub;
  }
}

// SETTINGS FILE HANDLING

// Open a new settings file
void CSerialEMDoc::OnSettingsOpen() 
{
  if (mSettingsOpen) {
    if (OfferToSaveSettings("open a new settings file?") > 0)
      return;
  }

  CString newSettings;
  if (GetTextFileName(true, true, newSettings)) 
    return;

  ReadNewSettingsFile(newSettings);
}

// Utility function to open a text file, optionally in the original directory
int CSerialEMDoc::GetTextFileName(bool openOld, bool originalDir, CString &pathname,
                                  CString *filename)
{
  static char szFilter[] = "Text files (*.txt)|*.txt|All files (*.*)|*.*||";
  MyFileDialog fileDlg(openOld, ".txt", NULL, OFN_HIDEREADONLY, szFilter);

  // Set the original directory.  Note this no longer works in Windows 7
  if (originalDir && !mOriginalCwd.IsEmpty())
    fileDlg.mfdTD.lpstrInitialDir = (LPCTSTR)mOriginalCwd;
  int result = fileDlg.DoModal();
  mWinApp->RestoreViewFocus();
  if (result != IDOK)
    return 1;

  pathname = fileDlg.GetPathName();
  if (filename && !pathname.IsEmpty())
    *filename = fileDlg.GetFileName();
  return pathname.IsEmpty() ? 1 : 0;
}

// Common path reading a different settings file
void CSerialEMDoc::ReadNewSettingsFile(CString newSettings)
{
  PreSettingsRead();

  int err = mParamIO->ReadSettings(newSettings);

  // If error, leave things as they were 
  if (err > 0) {
    AfxMessageBox("Error reading settings file; retaining previous file if any", 
        MB_EXCLAME);
  } else {
    // Commit to change. if just sparse, mark as open but unreadable
    mSettingsReadable = true;
    mRecentSettings->Add(newSettings);
    mSettingsOpen = err <= 0; 
    mCurrentSettingsPath = newSettings;
    mSettingsBackedUp = false;
  }

  PostSettingsRead();
}

void CSerialEMDoc::OnUpdateSettingsOpen(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingComplexTasks());    
}

// Reread settings file
void CSerialEMDoc::OnSettingsReadagain() 
{
  PreSettingsRead();

  if (mParamIO->ReadSettings(mCurrentSettingsPath)) {
    AfxMessageBox("Error rereading settings file", MB_EXCLAME);
    mSettingsReadable = false;
  }

  PostSettingsRead();
}

void CSerialEMDoc::OnUpdateSettingsReadagain(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!mWinApp->DoingComplexTasks() && mSettingsReadable && mSettingsOpen);
}

// Save command, which can be save as if no file is open
void CSerialEMDoc::OnSettingsSave() 
{
  if (!mSettingsOpen) {
      OnSettingsSaveas();
      return;
  }
  ManageBackupFile(mCurrentSettingsPath, mSettingsBackedUp);
  mParamIO->WriteSettings(mCurrentSettingsPath);
  SaveShortTermCal();
  mAbandonSettings = false;
}

// Save to a new specified file
void CSerialEMDoc::OnSettingsSaveas()
{
  SettingsSaveAs();
}

int CSerialEMDoc::SettingsSaveAs() 
{
  CString newFile;
  if (GetTextFileName(false, true, newFile))
    return 1;

  // Force a backup if it exists
  mSettingsBackedUp = false;    
  ManageBackupFile(newFile, mSettingsBackedUp);
  mAbandonSettings = false;

  mParamIO->WriteSettings(newFile);

  // Upon final success, make this the current open file
  mCurrentSettingsPath = newFile;
  mRecentSettings->Add(newFile);
  mSettingsOpen = true;
  mSettingsReadable = true;
  SaveShortTermCal();
  return 0;
}

// Close just means mark as not open for saving
void CSerialEMDoc::OnSettingsClose() 
{
  if (mSettingsOpen) {
    if (OfferToSaveSettings("close this settings file?") > 0)
      return;
  }
  mSettingsOpen = false;
  mCurrentSettingsPath = "";
}

void CSerialEMDoc::OnUpdateSettingsClose(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mSettingsOpen);
}

void CSerialEMDoc::OnSettingsReaddefaults() 
{
  if (mSettingsOpen) {
    if (OfferToSaveSettings("read the system settings file?") > 0)
      return;
  }
  PreSettingsRead();
  CString strSys = mSystemPath + "\\" + mSystemSettingsName;
  if (mParamIO->ReadSettings(strSys))
    AfxMessageBox("Error reading system settings file", MB_EXCLAME);
  PostSettingsRead();
}

void CSerialEMDoc::OnUpdateSettingsReaddefaults(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mSysSettingsReadable && !mWinApp->DoingComplexTasks());  
}

void CSerialEMDoc::OnSettingsAutosave() 
{
  mAutoSaveSettings = !mAutoSaveSettings;	
}

void CSerialEMDoc::OnUpdateSettingsAutosave(CCmdUI* pCmdUI) 
{
 	pCmdUI->SetCheck(mAutoSaveSettings);
}

void CSerialEMDoc::OnSettingsRecent(UINT nID)
{

  // Get the index, return if out of range or doing complex tasks
  int index = nID - ID_SETTINGS_MRU_FILE1;
  if (mWinApp->DoingComplexTasks() || index < 0 || index >= mRecentSettings->GetSize())
    return;

  // To make it convenient, simply save the settings if they are still open
  if (mSettingsOpen && !mCurrentSettingsPath.IsEmpty())
    mParamIO->WriteSettings(mCurrentSettingsPath);
  ReadNewSettingsFile(mRecentSettings->m_arrNames[index]);
}

void CSerialEMDoc::OnUpdateSettingsRecent(CCmdUI* pCmdUI)
{
  mRecentSettings->UpdateMenu(pCmdUI);
}

void CSerialEMDoc::OnSettingsDiscardOnExit()
{
  mAbandonSettings = !mAbandonSettings;
}

void CSerialEMDoc::OnUpdateSettingsDiscardOnExit(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(mAbandonSettings);
}

// Find out whether user wants to save settings before next action
int CSerialEMDoc::OfferToSaveSettings(CString strWhy)
{
  int result;
  if (mAbandonSettings)
    return 0;
  CString message = "Do you want to save your settings "
    "before proceeding\n to " + strWhy;
  result = AfxMessageBox(message, MB_ICONQUESTION | MB_YESNOCANCEL);
  if (result == IDNO)
    return 0;
  if (result == IDCANCEL)
    return 1;

  // The user wants to save
  if (mSettingsOpen && !mCurrentSettingsPath.IsEmpty()) {
    mParamIO->WriteSettings(mCurrentSettingsPath);
    return 0;
  } else
    return SettingsSaveAs();

}


void CSerialEMDoc::PreSettingsRead()
{
  // If low dose is on, go to view area to avoid confusing shifts
  mTrueLDArea = mWinApp->mScope->GetLowDoseArea();
  if (mWinApp->LowDoseMode())
    mWinApp->mScope->GotoLowDoseArea(0);
}

void CSerialEMDoc::PostSettingsRead()
{
  int *initialState = mWinApp->GetInitialDlgState();
  RECT *dlgPlacements = mWinApp->GetDlgPlacements();
  int *colorIndex = mWinApp->GetDlgColorIndex();
  BOOL saveRemote = mWinApp->GetShowRemoteControl();
  mAbandonSettings = false;

  // Copy low dose params, restore low dose area, copy camera params and update windows
  mWinApp->CopyCameraToCurrentLDP();
  if (mWinApp->LowDoseMode())
    mWinApp->mScope->GotoLowDoseArea(mTrueLDArea);
  mWinApp->CopyConSets(mWinApp->GetCurrentCamera());

  mWinApp->UpdateWindowSettings();
  if (mWinApp->mNavHelper->mStateDlg)
    mWinApp->mNavHelper->mStateDlg->UpdateSettings();
  if (mWinApp->mNavHelper->mMultiShotDlg)
    mWinApp->mNavHelper->mMultiShotDlg->UpdateSettings();
  if (!mWinApp->GetStartingProgram()) {
    mWinApp->SetShowRemoteControl(saveRemote);
    ((CMainFrame *)(mWinApp->m_pMainWnd))->InitializeDialogPositions(initialState, 
      dlgPlacements, colorIndex);
    mWinApp->OpenOrCloseMacroEditors();
  }
}


// Make previous version a backup if it exists and this hasn't been done before
void CSerialEMDoc::ManageBackupFile(CString strFile, BOOL &bBackedUp)
{
  CFileStatus status;
  if (bBackedUp)
    return;
  
  // Mark as backed up regardless of the outcome of these actions
  bBackedUp = true;

  // If file does not exist already, skip
  if (!CFile::GetStatus((LPCTSTR)strFile, status))
    return;

  // Compose backup name
  CString strBackup;
  int index = strFile.ReverseFind('.');
  if (index < 0)
    strBackup = strFile + ".bak";
  else
    strBackup = strFile.Left(index) + ".bak";

  // If it is the same file, skip out
  if (strFile.CompareNoCase(strBackup) == 0)
    return;

  UtilRenameFile(strFile, strBackup);
}

// On exit, offer to save if no open file, allowing a cancel
// or just save to open file
int CSerialEMDoc::SaveSettingsOnExit()
{
  int result, ind;
  const char *calType[NUM_CAL_DONE_TYPES] = {"image shift", "stage shift", "focus", 
    "beam intensity", "spot intensity"};
  CString str, str2;
  SaveShortTermCal();
  if (mWinApp->GetAdministrator() && mWinApp->CalibrationsNotSaved()) {
    result = AfxMessageBox("There are unsaved calibrations.\n"
      "Do you want to save them before exiting?", MB_ICONQUESTION | MB_YESNOCANCEL);
    if (result == IDCANCEL)
      return 1;
    if (result == IDYES)
      OnSettingsSavecalibrations();
  }

  if (!mWinApp->GetAdministrator() && mWinApp->CalibrationsNotSaved()) {
    result = 0;
    str = "There are unsaved calibrations (";
    for (ind = 0; ind < NUM_CAL_DONE_TYPES; ind++) {
      if (mNumCalsDone[ind] > 0) {
        str2.Format(" %d %s", mNumCalsDone[ind], calType[ind]);
        str += str2;
      }
      if (mNumCalsDone[ind] >= mNumCalsAskThresh[ind])
        result = 1;
    }
    if (result) {
      str += ")\n\nDo you want to save them before exiting?";
      result = AfxMessageBox(str, MB_QUESTION);
      if (result == IDYES)
        AfxMessageBox("You need to turn on Administrator mode in the Calibration menu "
        "in order to enable saving calibrations", MB_OK | MB_ICONINFORMATION);
      if (result != IDNO)
        return 1;
    }
  }

  if (mAbandonSettings)
    return 0;

  if (!mSettingsOpen)
    return (OfferToSaveSettings("exit the program?"));

  OnSettingsSave();
  return 0;
}

// Save the calibrations
void CSerialEMDoc::OnSettingsSavecalibrations() 
{
  char *cwd = NULL;

  // Need to return to original directory because system path could be relative
  if (!mOriginalCwd.IsEmpty()) {
    cwd = _getcwd(NULL, _MAX_PATH);
    _chdir((LPCTSTR)mOriginalCwd);
  }
  CString strSys = mSystemPath + "\\" + mCalibrationName;
  ManageBackupFile(strSys, mCalibBackedUp);
  mParamIO->WriteCalibration(strSys);
  mWinApp->SetCalibrationsNotSaved(false);
  SaveShortTermCal();
  CalibrationWasDone(CAL_DONE_CLEAR_ALL);
  if (cwd) {
    _chdir(cwd);
    free(cwd);
  }
}


void CSerialEMDoc::OnUpdateSettingsSavecalibrations(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(mWinApp->GetAdministrator());  
}

// Keep track of 
void CSerialEMDoc::CalibrationWasDone(int type)
{
  if (type == CAL_DONE_CLEAR_ALL)
    for (int ind = 0; ind < NUM_CAL_DONE_TYPES; ind++)
      mNumCalsDone[ind] = 0;
  if (type < 0 || type >= NUM_CAL_DONE_TYPES)
    return;
  mNumCalsDone[type]++;
  mWinApp->SetCalibrationsNotSaved(true);
}

// Append an entry to the log book file; return 1 if no log book defined, -1 if error
int CSerialEMDoc::AppendToLogBook(CString inString, CString title)
{
  CStdioFile *cFile = NULL;
  int retval = 0;
  if (mLogBook.IsEmpty())
    return 1;
  CString name = mLogBook;

  try {
    // Open the file for writing 
    cFile = new CStdioFile(name, CFile::modeCreate | CFile::modeNoTruncate |
      CFile::modeWrite | CFile::shareDenyWrite);
    cFile->SeekToEnd();
    if (!cFile->GetPosition())
      cFile->WriteString(title);

    cFile->WriteString(inString);
  }
  catch(CFileException *perr) {
    perr->Delete();
    CString message = "Error writing to logbook: " + name;
    AfxMessageBox(message, MB_EXCLAME);
    retval = -1;
  } 
  if (cFile) {
    cFile->Close();
    delete cFile;
  }
  return retval;
}

// Periodically save navigator, log, and settings, if user selected; save short term cal
void CSerialEMDoc::AutoSaveFiles()
{
  if (mWinApp->mNavigator && mAutoSaveNav)
    mWinApp->mNavigator->AutoSave();
  if (mAutoSaveSettings && mSettingsOpen && !mAbandonSettings &&
    !mWinApp->mTSController->StartedTiltSeries())
      OnSettingsSave();
  if (mWinApp->mTSController->GetAutosaveLog() && mWinApp->mLogWindow)
    mWinApp->mLogWindow->UpdateSaveFile(false);
  SaveShortTermCal();
}


void CSerialEMDoc::AppendToProgramLog(BOOL starting)
{
  CStdioFile *cFile = NULL;
  CString name = CString(defaultSysPath) + "\\" PROGRAM_LOG;
  CString string;
  CTime ctdt = CTime::GetCurrentTime();
  CTimeSpan ts;

  if (starting) {
    string.Format("%4d/%02d/%02d %02d:%02d:%02d %s start\n", ctdt.GetYear(), 
      ctdt.GetMonth(), ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(),
      getenv("USERNAME"));
    mStartupTime = ctdt;
  } else {
    ts = ctdt - mStartupTime;
    string.Format("%4d/%02d/%02d %02d:%02d:%02d exit\t%d\n", ctdt.GetYear(), 
      ctdt.GetMonth(), ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(), 
      ts.GetTotalSeconds());
  }

  try {
    // Open the file for writing 
    cFile = new CStdioFile(name, CFile::modeCreate | CFile::modeNoTruncate |
      CFile::modeWrite | CFile::shareDenyWrite);
    cFile->SeekToEnd();
    cFile->WriteString(string);
  }
  catch(CFileException *perr) {
    perr->Delete();
  } 
  if (cFile) {
    cFile->Close();
    delete cFile;
  }
}

// Save the short-term calibrations - this is called when settings or calibrations are
// saved or upon exit
void CSerialEMDoc::SaveShortTermCal()
{
  CString filename;
  char *cwd = NULL;

  // Might as well save flybacks here too; it is called at good times
  if (mWinApp->mCalibTiming->GetNeedToSaveFlybacks()) {
    filename = CString(defaultSysPath) + "\\" + mFlybackName;
    ManageBackupFile(filename, mFlybackBackedUp);
    mParamIO->WriteFlybackTimes(filename);
    mWinApp->mCalibTiming->SetNeedToSaveFlybacks(false);
  }

  // 4/22/12: changed && to ||
  if (!mShortTermNotSaved)
    return;
  if (!mOriginalCwd.IsEmpty()) {
    cwd = _getcwd(NULL, _MAX_PATH);
    _chdir((LPCTSTR)mOriginalCwd);
  }
  filename = mSystemPath + "\\" + mShortTermName;
  ManageBackupFile(filename, mShortTermBackedUp);
  mParamIO->WriteShortTermCal(filename);
  mShortTermNotSaved = false;
  if (cwd) {
    _chdir(cwd);
    free(cwd);
  }
}

//////////////////////////////////////////////
// SAVED-FRAMES MDOC FILE

// Opens the file: gets the name and gets a new adoc
void CSerialEMDoc::OnFileOpenMdoc()
{
  static char BASED_CODE szFilter[] = 
    "Mdoc files (*.mdoc)|*.mdoc|All files (*.*)|*.*||";
  MyFileDialog fileDlg(FALSE, ".mdoc", NULL, OFN_HIDEREADONLY, szFilter);
  int result = fileDlg.DoModal();
  mWinApp->RestoreViewFocus();
  if (result != IDOK)
    return;
  mFrameFilename = fileDlg.GetPathName();
  if (mFrameFilename.IsEmpty())
    return;
  DoOpenFrameMdoc(mFrameFilename);
}

// Actually opens the mdoc and sets up parameters
int CSerialEMDoc::DoOpenFrameMdoc(CString &filename)
{
  CString mess;
  mFrameFilename = filename;
  if (AdocAcquireMutex()) {
    mFrameAdocIndex = AdocNew();
    if (AddTitlesToFrameMdoc(mess))
      SEMMessageBox(mess, MB_EXCLAME);
    AdocReleaseMutex();
  }
  mFrameSetIndex = 0;
  mLastFrameSect = -1;
  mLastWrittenFrameSect = -1;
  mDeferWritingFrameMdoc = false;
  if (mFrameAdocIndex < 0) {
    SEMMessageBox("Failed to get a new autodoc structure for the .mdoc file", MB_EXCLAME);
    return 1;
  }
  return 0;
}

void CSerialEMDoc::OnUpdateFileOpenMdoc(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mFrameAdocIndex < 0);
}

// Add the standard SerialEM title and frame file title(s) to an mdoc
int CSerialEMDoc::AddTitlesToFrameMdoc(CString &message)
{
  char title[80];
  CString str, line;
  int ind, tInd = 1;
  MakeSerialEMTitle(mTitle, title);
  if (AdocSetKeyValue(ADOC_GLOBAL, 0, "T", title)) {
    message = "adding title to autodoc";  
    return 1;
  }
  str = mFrameTitle;
  while (!str.IsEmpty()) {
    line = str;
    ind = line.Find('\n');
    if (ind >= 0) {
      line = str.Left(ind);
      str = str.Right(str.GetLength() - (ind + 1));
    } else
      str = "";
    if (!line.IsEmpty()) {
      sprintf(title, "T%d", tInd);
      tInd++;
      if (AdocSetKeyValue(ADOC_GLOBAL, 0, title, (LPCTSTR)line)) {
        message = "adding title to autodoc";  
        return 1;
      }
    }
  }
  if (AdocSetFloat(ADOC_GLOBAL, 0, ADOC_VOLTAGE, 
    (float)mWinApp->mProcessImage->GetRecentVoltage())) {
      message = "adding voltage to autodoc";  
      return 1;
  }
  return 0;
}

// Closes the file: clears it out and sets index -1
void CSerialEMDoc::OnFileCloseMdoc()
{
  if (AdocAcquireMutex()) {
    AdocClear(mFrameAdocIndex);
    AdocReleaseMutex();
  }
  mFrameAdocIndex = -1;
}

void CSerialEMDoc::OnUpdateFileCloseMdoc(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mFrameAdocIndex >= 0);
}

// Saves extra data from the image into the autodoc file
int CSerialEMDoc::SaveFrameDataInMdoc(KImage *image)
{
  int err = 0, sectInd;
  char setNum[15];
  if (mFrameAdocIndex < 0)
    return 0;
  if (AdocGetMutexSetCurrent(mFrameAdocIndex) < 0)
    return 1;
  sprintf(setNum, "%d", mFrameSetIndex);
  sectInd = AdocAddSection("FrameSet", setNum);
  if (sectInd < 0) {
    AdocReleaseMutex();
    return 2;
  }
  mLastFrameSect = sectInd;
  RELEASE_RETURN_ON_ERR(KStoreADOC::SetValuesFromExtra(image, "FrameSet", sectInd), 2);
  if (!mDeferWritingFrameMdoc) {
    if (mFrameSetIndex)
      err = AdocAppendSection((LPCTSTR)mFrameFilename);
    else
      err = AdocWrite((LPCTSTR)mFrameFilename);
    mLastWrittenFrameSect = sectInd;
  }
  AdocReleaseMutex();
  mFrameSetIndex++;
  if (err < 0)
    return 3;
  return 0;

}

// Updates the last section added to mdoc, presumably from deferred sum
int CSerialEMDoc::UpdateLastMdocFrame(KImage *image)
{
  int retval = 0;
  if (mFrameAdocIndex < 0)
    return 0;
  if (AdocGetMutexSetCurrent(mFrameAdocIndex) < 0)
    return 4;
  if (mLastFrameSect < 0)
    return 1;
  RELEASE_RETURN_ON_ERR(KStoreADOC::SetValuesFromExtra(image, "FrameSet", mLastFrameSect),
    2);
  if (!(mDeferWritingFrameMdoc && mLastFrameSect == mLastWrittenFrameSect + 1)) {
    if (AdocWrite((LPCTSTR)mFrameFilename) < 0)
      retval = 3;
    mLastWrittenFrameSect = mLastFrameSect;
    mDeferWritingFrameMdoc = false;
  }
  AdocReleaseMutex();
  return retval;
}

// Adds a key-value pair to the last section of the frame mdoc file
int CSerialEMDoc::AddValueToFrameMdoc(CString key, CString value)
{
  int retval = 0;
  if (mFrameAdocIndex < 0)
    return 1;
  if (AdocGetMutexSetCurrent(mFrameAdocIndex) < 0)
    return 2;
  if (AdocSetKeyValue(mLastFrameSect < 0 ? ADOC_GLOBAL : "FrameSet", 
    mLastFrameSect < 0 ? 0 : mLastFrameSect, (LPCTSTR)key, (LPCTSTR)value))
    retval = 4;
  AdocReleaseMutex();
  return retval;
}

// Writes the frame mdoc file
int CSerialEMDoc::WriteFrameMdoc(void)
{
  int retval = 0;
  if (mFrameAdocIndex < 0)
    return 1;
  if (AdocGetMutexSetCurrent(mFrameAdocIndex) < 0)
    return 2;
  if (mDeferWritingFrameMdoc && mLastFrameSect == mLastWrittenFrameSect + 1) {
    if (AdocAppendSection((LPCTSTR)mFrameFilename) < 0)
      retval = 5;
  } else {
    if (AdocWrite((LPCTSTR)mFrameFilename) < 0)
      retval = 5;
  }
  mLastWrittenFrameSect = mLastFrameSect;
  mDeferWritingFrameMdoc = false;
  AdocReleaseMutex();
  return retval;
}

////////////////////////////////////////////////////////////////////////
// FILE DIALOG MESS

static char emptyString[] = "";

MyFileDialog::MyFileDialog(BOOL bOpenFileDialog, LPCTSTR lpszDefExt, 
    LPCTSTR lpszFileName, DWORD dwFlags,
    LPCTSTR lpszFilter, CWnd* pParentWnd)
{
#ifdef USE_SDK_FILEDLG

  // If using SDK file dialog, save all the values in structure
  mfdTD.bOpenFileDialog = bOpenFileDialog;
  mfdTD.lpszDefExt = lpszDefExt;
  mfdTD.lpszFileName = lpszFileName;
  mfdTD.dwFlags = dwFlags;
  mfdTD.lpszFilter = lpszFilter;
#else

  // Otherwise construct CFileDialog object
  mFileDlg = new CFileDialog(bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags,
    lpszFilter, pParentWnd);

  // For windows XP, add a dumb notice to the Open dialog.  Not needed in Win 7.
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (bOpenFileDialog && (IsXP() || IsVersion(6, VER_EQUAL, 0, VER_EQUAL))) {
    OPENFILENAME &ofn = mFileDlg->GetOFN();
    ofn.lpstrTitle = "Open    -    Push \"Open\" to see files after changing filter";
  }
#endif
  mfdTD.lpstrInitialDir = (LPCTSTR)emptyString;
}

MyFileDialog::~MyFileDialog()
{
#ifndef USE_SDK_FILEDLG
   if (mFileDlg)
     delete mFileDlg;
#endif
}

// Function for the DoModal operation
int MyFileDialog::DoModal()
{
#ifdef USE_SDK_FILEDLG
#ifdef USE_DUMMYDLG
  CDummyDlg dummy;
  dummy.mfdTDp = &mfdTD;
  return dummy.DoModal();
#else
  // Run SDK file dialog in thread here
  mfdTD.done = false;
  MyFileDlgThread *dlgThread = (MyFileDlgThread *)AfxBeginThread(
    RUNTIME_CLASS(MyFileDlgThread), THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
  dlgThread->mfdTDp = &mfdTD;
  dlgThread->ResumeThread();
  while (!mfdTD.done) {
    Sleep(50);
  }
  return mfdTD.retval;
#endif

#else

  // Use standard CFileDialog
  char *startDir = _getcwd(NULL, MAX_PATH);
  OPENFILENAME &ofn = mFileDlg->GetOFN();
  ofn.lpstrInitialDir = mfdTD.lpstrInitialDir;
  int retval = (int)mFileDlg->DoModal();
  mfdTD.pathName = mFileDlg->GetPathName();
  mfdTD.fileName = mFileDlg->GetFileName();
  if (startDir) {
    _chdir(startDir);
    free(startDir);
  }
  return retval;
#endif
}

#ifdef USE_SDK_FILEDLG

IMPLEMENT_DYNCREATE(MyFileDlgThread, CWinThread)

BOOL MyFileDlgThread::InitInstance()
{
  mfdTDp->retval = RunSdkFileDlg(mfdTDp);
  mfdTDp->done = true;
  return true;
}
  
// Common routine to run the SDK file dialog
int RunSdkFileDlg(MyFileDlgThreadData *mfdTDp)
{
  OPENFILENAME ofn;       // common dialog box structure
  char szFile[520];       // buffer for file name plus path
  char szFileTitle[520];       // buffer for file name
  char szFilter[100];       // buffer for filters
  BOOL retval;
  unsigned int i = 0;
  
  // Replace | characters with nulls as required by file dialog
  if (mfdTDp->lpszFilter) {
    for (i = 0; i < strlen(mfdTDp->lpszFilter); i++) {
      if (i == sizeof(szFilter) - 2)
        break;
      szFilter[i] = mfdTDp->lpszFilter[i];
      if (szFilter[i] == '|')
        szFilter[i] = 0x00;
    }
  }
  szFilter[i++] = 0x00;
  szFilter[i++] = 0x00;

  // Initialize OPENFILENAME
  memset(&ofn, 0, sizeof(OPENFILENAME));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = szFilter;
    
  ofn.lpstrDefExt = mfdTDp->lpszDefExt;
  
  if (mfdTDp->lpszFileName != NULL) 
    lstrcpyn(szFile, mfdTDp->lpszFileName, sizeof(szFile));
  else 
    szFile[0] = 0x00;
  
  ofn.lpstrFileTitle = szFileTitle;
  ofn.nMaxFileTitle = sizeof(szFileTitle);
  ofn.lpstrInitialDir = mfdTDp->lpstrInitialDir;
  ofn.Flags = mfdTDp->dwFlags;
  
  // Display the Open dialog box. 
  
  if (mfdTDp->bOpenFileDialog)
    retval = ::GetOpenFileName(&ofn);
  else
    retval = ::GetSaveFileName(&ofn);
  if (retval) {
    mfdTDp->pathName = szFile;
    mfdTDp->fileName = szFileTitle;
  }
  
  return retval ? IDOK : IDCANCEL;
}
#endif
