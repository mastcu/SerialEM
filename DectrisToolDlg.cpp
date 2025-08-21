// DectrisToolDlg.cpp : Control panel for DECTRIS camera
//

#include "stdafx.h"
#include "SerialEM.h"
#include "DectrisToolDlg.h"
#include "CameraController.h"
#include "EMscope.h"
#include "DectrisSettingsDlg.h"
#include "PluginManager.h"

// CDectrisToolDlg dialog
static VOID CALLBACK StatusUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent,
  DWORD dwTime);

CDectrisToolDlg::CDectrisToolDlg(CWnd* pParent /*=NULL*/)
	: CToolDlg(IDD_DECTRIS_TOOLDLG, pParent)
  , m_strHVstateValue(_T(""))
  , m_strSensorPos(_T(""))
  , m_strStatus(_T(""))
{
  mUpdateInterval = 1000;
  mStatusUpdateID = 0;
  mInitializeThread = NULL;
  mDoingFlatfield = false;
}

CDectrisToolDlg::~CDectrisToolDlg()
{
}

void CDectrisToolDlg::KillUpdateTimer()
{
  if (mStatusUpdateID)
    ::KillTimer(NULL, mStatusUpdateID);
  mStatusUpdateID = 0;
}

void CDectrisToolDlg::DoDataExchange(CDataExchange* pDX)
{
  CToolDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STAT_HV_STATE, m_strHVstateValue);
  DDX_Text(pDX, IDC_STAT_SENSOR_POS, m_strSensorPos);
  DDX_Text(pDX, IDC_STAT_STATUS, m_strStatus);
}


BEGIN_MESSAGE_MAP(CDectrisToolDlg, CToolDlg)
  ON_BN_CLICKED(IDC_BUT_ACQUIRE_FLATFIELD, OnButAcquireFlatfield)
  ON_BN_CLICKED(IDC_BUT_ADVANCED_SET, OnButAdvancedSet)
  ON_BN_CLICKED(IDC_BUT_INIT_DETECTOR, OnButInitDetector)
  ON_BN_CLICKED(IDC_BUT_TOGGLE_HV, OnButToggleHv)
END_MESSAGE_MAP()


BOOL CDectrisToolDlg::OnInitDialog()
{
  CToolDlg::OnInitDialog();
  mStatusUpdateID = ::SetTimer(NULL, 1, mUpdateInterval, StatusUpdateProc);
  mDTD.dectrisPlugFuncs = mWinApp->mPluginManager->GetDectrisFuncs();
  UpdateEnables();
  
  mInitialized = true;

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void CDectrisToolDlg::OnOK()
{
  mWinApp->RestoreViewFocus();
}

// CDectrisToolDlg message handlers

// Start the acquisition of a flatfield
void CDectrisToolDlg::OnButAcquireFlatfield()
{
  int index;
  ControlSet *conSets = mWinApp->GetConSets();
  CameraParameters *camParams = GetDectrisCamParams(index);
  if (index < 0 || !camParams || camParams->failedToInitialize)
    return;
  index = mDTD.dectrisPlugFuncs->AcquireFlatfield(&mFlatExposure, &mNumFlatAcquires);
  if (index > 0)
    return;
  if (index < 0) {
    AfxMessageBox("The call to start a DECTRIS flatfield acquisition returned with an"
      " error", MB_EXCLAME);
    return;
  }
  mDoingFlatfield = true;
  mFlatCount = 0;
  mWinApp->UpdateBufferWindows();
  FlatfieldNextTask();
}

// Open dialog for advanced settings
void CDectrisToolDlg::OnButAdvancedSet()
{
  CDectrisSettingsDlg dlg;
  int ind;
  CameraParameters *camParams = GetDectrisCamParams(ind);
  dlg.mDectrisTypeInd = camParams->DectrisType - 1;
  dlg.mDectrisPlugFuncs = mWinApp->mPluginManager->GetDectrisFuncs();
  dlg.DoModal();
  mWinApp->RestoreViewFocus();
}

// Initialize the detector
void CDectrisToolDlg::OnButInitDetector()
{
  int index;
  CameraParameters *camParams = GetDectrisCamParams(index);
  if (index < 0 || !camParams || camParams->failedToInitialize)
    return;
  mDTD.camPlugFuncs = mWinApp->mCamera->GetOneCamPlugFuncs(index);
  if (!mDTD.camPlugFuncs)
    return;
  mInitializeThread = AfxBeginThread(InitializeProc, &mDTD, THREAD_PRIORITY_NORMAL, 0,
    CREATE_SUSPENDED);
  mInitializeThread->m_bAutoDelete = false;
  mInitializeThread->ResumeThread();
  mWinApp->AddIdleTask(TASK_DECTRIS_INITIALIZE, 0, 600 * 1000);
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "INITIALIZING");
  mWinApp->RestoreViewFocus();
}

// Toggle the HV state
void CDectrisToolDlg::OnButToggleHv()
{
  if (mDTD.dectrisPlugFuncs->RequestHVToggle())
    AfxMessageBox("The request to toggle the HV returned with an error", MB_EXCLAME);
  mWinApp->RestoreViewFocus();
}

// Update button states; for flatfield it must be current camera
void CDectrisToolDlg::UpdateEnables()
{
  if (!mInitialized)
    return;
  bool camOK, tasks = mWinApp->DoingTasks() || mWinApp->mCamera->CameraBusy();
  int index;
  CameraParameters *camP = GetDectrisCamParams(index);
 
  camOK = camP != NULL && index >= 0 && !camP->failedToInitialize;
  EnableDlgItem(IDC_BUT_ACQUIRE_FLATFIELD, !tasks && camOK && 
    (!mWinApp->mScope->StageBusy() || mWinApp->mScope->GetSimulationMode()) &&
    index == mWinApp->GetCurrentCamera());
  EnableDlgItem(IDC_BUT_ADVANCED_SET, !tasks && camOK);
  EnableDlgItem(IDC_BUT_INIT_DETECTOR, !tasks && camOK);
  EnableDlgItem(IDC_BUT_TOGGLE_HV, !tasks && camOK);
}

// Returns params for the active camera or first DECTRIS camera in active list, and 
// returns camera number (parameter index) in index
CameraParameters *CDectrisToolDlg::GetDectrisCamParams(int &index)
{
  CameraParameters *camParams = mWinApp->GetCamParams();
  int *actList = mWinApp->GetActiveCameraList();
  int curCam = mWinApp->GetCurrentCamera();
  if (!camParams->DectrisType) {
    curCam = -1;
    for (int ind = 0; ind < mWinApp->GetActiveCamListSize(); ind++) {
      if (camParams[actList[ind]].DectrisType) {
        curCam = actList[ind];
        break;
      }
    }
  }
  index = curCam;
  return curCam >= 0 ? &camParams[curCam] : NULL;
}

// Update timer callback and function
static VOID CALLBACK StatusUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent,
  DWORD dwTime)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mDectrisToolDlg.UpdateToolDlg();
}

void CDectrisToolDlg::UpdateToolDlg()
{
  int index;
  std::string status;
  const char *positions[5] = {"", "retracted", "inserted", "moving", "collision"};
  CameraParameters *camParams = GetDectrisCamParams(index);
  if (!mInitialized)
    return;
  if (!camParams || index < 0 || camParams->failedToInitialize || 
    !mWinApp->mCamera->GetInitialized() || !mDTD.dectrisPlugFuncs)
    return;
  mDTD.dectrisPlugFuncs->GetDetectorStatus(&status);
  m_strStatus = status.c_str();
  if (camParams->retractable) {
    mDTD.camPlugFuncs = mWinApp->mCamera->GetOneCamPlugFuncs(index);
    index = mDTD.camPlugFuncs->IsCameraInserted(camParams->cameraNumber);
    B3DCLAMP(index, -1, 3);
    m_strSensorPos = positions[index + 1];
  }
  ShowDlgItem(IDC_STAT_SENSOR, camParams->retractable);
  ShowDlgItem(IDC_STAT_SENSOR_POS, camParams->retractable);
  mDTD.dectrisPlugFuncs->GetHVStatus(&status);
  m_strHVstateValue.Format("HV: %s", status.c_str());
  UpdateData(false);
}

// Procedure for initializing
UINT CDectrisToolDlg::InitializeProc(LPVOID pParam)
{
  DectrisThreadData *td = (DectrisThreadData *)pParam;
  int retval = 0;
  td->errString = "";

  retval = td->dectrisPlugFuncs->RequestInitializeDetector();
  if (retval < 0) {
    td->errString = "The call to initialize the DECTRIS detector returned with an"
    " error";
    if (td->camPlugFuncs->GetLastErrorString) {
      td->errString += ": ";
      td->errString += td->camPlugFuncs->GetLastErrorString();
    }
  }

  return retval < 0 ? 1 : 0;
}

// Busy function
int CDectrisToolDlg::InitializeBusy()
{
  return UtilThreadBusy(&mInitializeThread);
}

// Cleanup function for timeout or error return
void CDectrisToolDlg::CleanupInitialize(int error)
{
  if (error == IDLE_TIMEOUT_ERROR) {
    UtilThreadCleanup(&mInitializeThread);
    SEMMessageBox("Timeout initializing DECTRIS detector");
  } else if (!mDTD.errString.IsEmpty()) {
    SEMMessageBox(mDTD.errString);
  }
  InitializeDone();
}

// Finish up restoring state
void CDectrisToolDlg::InitializeDone()
{
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(SIMPLE_PANE, "");
}

// Next task for flatfielding - take an image and update the status bar
void CDectrisToolDlg::FlatfieldNextTask()
{
  ControlSet *conSets = mWinApp->GetConSets();
  CameraParameters *camParams = mWinApp->GetActiveCamParam();
  CString message;
  if (!mDoingFlatfield)
    return;

  // When done, finish up
  if (mFlatCount >= mNumFlatAcquires) {
    FlatfieldDone();
    return;
  }

  conSets[TRACK_CONSET] = conSets[RECORD_CONSET];
  conSets[TRACK_CONSET].exposure = mFlatExposure;
  conSets[TRACK_CONSET].drift = 0;
  conSets[TRACK_CONSET].left = 0;
  conSets[TRACK_CONSET].top = 0;
  conSets[TRACK_CONSET].right = camParams->sizeX;
  conSets[TRACK_CONSET].bottom = camParams->sizeY;
  conSets[TRACK_CONSET].binning = BinDivisorI(camParams);
  conSets[TRACK_CONSET].alignFrames = 0;
  conSets[TRACK_CONSET].saveFrames = 0;
  conSets[TRACK_CONSET].doseFrac = 0;
  conSets[TRACK_CONSET].K2ReadMode = 0;
  conSets[TRACK_CONSET].processing = UNPROCESSED;
  conSets[TRACK_CONSET].mode = SINGLE_FRAME;
  mFlatCount++;
  message.Format("ACQUIRING FLATFIELD %d of %d", mFlatCount, mNumFlatAcquires);
  mWinApp->SetStatusText(MEDIUM_PANE, message);
  mWinApp->mCamera->InitiateCapture(TRACK_CONSET);
  mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_DECTRIS_FLATFIELD, 0, 
    120 * 1000);
}

// Process error during flatfieling
void CDectrisToolDlg::CleanupFlatfield(int error)
{
  if (error == IDLE_TIMEOUT_ERROR)
    SEMMessageBox("Timeout getting flatfield for DECTRIS detector");
  FlatfieldDone();
}

// Finish with faltfielding, including informing the plugin regardless of whether the 
// fullcount was reached
void CDectrisToolDlg::FlatfieldDone()
{
  mDTD.dectrisPlugFuncs->FinishFlatfield();
  mDoingFlatfield = false;
  mWinApp->UpdateBufferWindows();
  mWinApp->SetStatusText(MEDIUM_PANE, "");
}

