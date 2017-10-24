// DirectElectronToolDlg.cpp : Manages the control panel for DE cameras
//

#include "stdafx.h"
#include "../SerialEM.h"
#include "DirectElectronToolDlg.h"
#include "DirectElectronCamera.h"
#include "..\CameraController.h"
#include "..\MacroProcessor.h"

// DirectElectronToolDlg dialog

static VOID CALLBACK StatusUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, 
                                      DWORD dwTime);

IMPLEMENT_DYNAMIC(DirectElectronToolDlg, CToolDlg)

DirectElectronToolDlg::DirectElectronToolDlg(CWnd *pParent /*=NULL*/)
  : CToolDlg(DirectElectronToolDlg::IDD, pParent)
{
  mStatusUpdateID = 0;
  mSuppressDebug = 0;
  mLastProtCoverSet = -1;
  mProtCoverChoice = 1;
  mProtCoverDelay = -1;
  mFormatForAutoSave = -1;
  mChangingCooling = false;
  mStatusUpdateID = 0;
  mTemperSetpoint = -999;
}

DirectElectronToolDlg::~DirectElectronToolDlg()
{
}

// Set pointer to DE camera module
void DirectElectronToolDlg::setCameraReference(DirectElectronCamera *cam)
{
  mDECamera = cam;
}

void DirectElectronToolDlg::KillUpdateTimer()
{
  if (mStatusUpdateID)
    ::KillTimer(NULL, mStatusUpdateID);
}


// Function to update the state of various items from the server upon call or periodically
// when called from the update proc
void DirectElectronToolDlg::updateDEToolDlgPanel(bool initialCall)
{
  if (!mDECamera)
    return;
  int index = mDECamera->GetInitializingIndex();
  if (index < 0)
    index = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + index;
  BOOL curIsDE = camParam->DE_camType > 0;
  if (!initialCall && curIsDE && mWinApp->mCamera->CameraBusy())
    return;
  if (GetDebugOutput('D')) {
    if (mSuppressDebug)
      mSuppressDebug = 1;
  } else {
    mSuppressDebug = 0;
  }
  try {
    CString value = mDECamera->getCurrentCameraName();
    BOOL isDE12 = mDECamera->CurrentIsDE12();
    BOOL isSurvey = mDECamera->CurrentIsSurvey();
    BOOL isEither = isDE12 || isSurvey;
    ((CWnd *)GetDlgItem(ID_DE_camName))->SetWindowText(value);

    float temp_float = 0.0;
    int temp_int = 1;

    // Update Temperature
    if (!isSurvey) {
      value.Format("%0.2f", mDECamera->getCameraTemp());
      ((CWnd *) GetDlgItem(IDC_DDD_Temp))->SetWindowText(value);
    }

    // update temperature setpoint if no property entered
    if (isDE12) {
      if (mTemperSetpoint <= -999) {
        if (mDECamera->getIntProperty("Temperature Control - Setpoint (Celsius)", 
          temp_int)) {
            value.Format("Setpoint(C): %d", temp_int);
            ((CWnd *) GetDlgItem(ID_DE_temperSet))->SetWindowText(value);
            mTemperSetpoint = temp_int;
        }
      }

      // Read temperature control
      if (!mChangingCooling && mDECamera->getStringProperty("Temperature Control", value))
        ((CButton *) GetDlgItem(IDC_COOLCAM))->SetCheck(value == "Cool Down" ? 1 : 0);
    }

    if (isEither) {
      // update waterline temp
      value.Format("Water line(C): %0.1f", mDECamera->getWaterLineTemp());
      ((CWnd *) GetDlgItem(ID_DE_waterlineTemp))->SetWindowText(value);

      // read "Camera Position Status"
      if (mDECamera->getStringProperty("Camera Position Status", value))
        ((CButton *) GetDlgItem(IDC_DE_insertCam))->SetCheck(value == 
          DE_CAM_STATE_INSERTED ? 1 : 0);

      // Update Auto save address
      if (mDECamera->getStringProperty("Autosave Directory", value)) {
        ((CWnd *) GetDlgItem(ID_DE_saveDir))->SetWindowText(value);
        mAutosaveDir = value;
      }

      // Update file format if no user value yet
      if (mFormatForAutoSave < 0) {
        if (mDECamera->getStringProperty("Autosave Frames - Format", value)) {
          CComboBox   *correctComboBox = ((CComboBox *) GetDlgItem(ID_DE_autosaveF));
          temp_int = value == "TIFF" ? 0 : 1;
          correctComboBox->SetCurSel(temp_int);  //0==TIFF and 1==HDF5 or MRC
          mFormatForAutoSave = temp_int;
        }
      }

      // Update Max FPS
      if (camParam->DE_MaxFrameRate > 0.) {
        value.Format("%0.2f", camParam->DE_MaxFrameRate);
        ((CWnd *) GetDlgItem(ID_DE_maxfps))->SetWindowText(value);
      }
    }

    // Update Current FPS if there is no user value yet
    if (isEither && curIsDE && camParam->DE_FramesPerSec <= 0) {
      if (mDECamera->getFloatProperty("Frames Per Second", temp_float)) {
        value.Format("%0.2f", temp_float);
        ((CWnd *) GetDlgItem(ID_DE_currfps))->SetWindowText(value);
        camParam->DE_FramesPerSec = temp_float;
      }
    }

    // Update Protection Cover Delay if no system value
    if (isDE12 && mProtCoverDelay < 0) {
      if (mDECamera->getIntProperty("Protection Cover Delay (milliseconds)", temp_int)) {
        value.Format("%d", temp_int);
        ((CWnd *) GetDlgItem(ID_DE_coverDelay))->SetWindowText(value);
        mProtCoverDelay = temp_int;
      }
    }

  } catch (...) {}
  if (GetDebugOutput('D'))
    mSuppressDebug = -1;
}

// Update the state of the camera inserted checkbox
void DirectElectronToolDlg::UpdateInsertion(int state)
{
  ((CButton *)GetDlgItem(IDC_DE_insertCam))->SetCheck(state);
}

// Convenience functions for whether frame-saving is possible and whether there is a
// a frame time to constrain exposure times
BOOL DirectElectronToolDlg::CanSaveFrames(CameraParameters *param)
{
  return (param->DE_camType == DE_12 || param->DE_camType == DE_12_Survey);
}

BOOL DirectElectronToolDlg::HasFrameTime(CameraParameters *param)
{
  return (param->DE_camType == DE_12 || param->DE_camType == DE_12_Survey);
}

// The status update procedure run on a timer
static VOID CALLBACK StatusUpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, 
                                      DWORD dwTime)
{
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  winApp->mDEToolDlg.updateDEToolDlgPanel(false);
}

//Currently a function that I have not implemented yet.
void DirectElectronToolDlg::setUpDialogNames(CString names[], int totalcams)
{
  //pass reference to the Dialogs.
  /*mDETabControl.InitDialogs(names,totalcams);
  for(int i=0;i<totalcams;i++)
  {
    mDETabControl.InsertItem(0,names[i]);

  }
  mDETabControl.ActivateTabDialogs();*/
}

BOOL DirectElectronToolDlg::OnInitDialog()
{

  CToolDlg::OnInitDialog();
  mHdfMrcSaveOption = "HDF5";

  // Start a timer for periodic updates
  mStatusUpdateID = ::SetTimer(NULL, 1, 10000, StatusUpdateProc);

  CString value;
  CComboBox   *correctComboBox = ((CComboBox *) GetDlgItem(ID_DE_protectMode));
  value = "Always Open during operation";
  correctComboBox->AddString(value);
  value = "Open/Close for each exposure";
  correctComboBox->AddString(value);
  correctComboBox->SetCurSel(mProtCoverChoice);

  correctComboBox = ((CComboBox *) GetDlgItem(ID_DE_autosaveF));
  value = "TIFF";
  correctComboBox->AddString(value);
  correctComboBox->AddString(mHdfMrcSaveOption);

  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE

}

void DirectElectronToolDlg::SwitchSaveOptToMRC()
{
  CComboBox *comboBox = ((CComboBox *) GetDlgItem(ID_DE_autosaveF));
  mHdfMrcSaveOption = "MRC";
  comboBox->DeleteString(1);
  comboBox->InsertString(1, "MRC");
}

// If there are settings and property values, set the server and dialog with them
void DirectElectronToolDlg::ApplyUserSettings()
{
  if (!mDECamera)
    return;
  CString value;
  int index = mDECamera->GetInitializingIndex();
  if (index < 0)
    index = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetCamParams() + index;
  BOOL isDE12 = mDECamera->CurrentIsDE12();
  BOOL isSurvey = mDECamera->CurrentIsSurvey();
  if ((isDE12 || isSurvey) && mFormatForAutoSave >= 0) {
    B3DCLAMP(mFormatForAutoSave, 0, 1);
    ((CComboBox *) GetDlgItem(ID_DE_autosaveF))->SetCurSel(mFormatForAutoSave);
    mDECamera->setStringProperty("Autosave Frames - Format", mFormatForAutoSave ? 
      mHdfMrcSaveOption : "TIFF");
  }
  if (camParam->DE_FramesPerSec > 0) {
    value.Format("%0.2f", camParam->DE_FramesPerSec);
    ((CWnd *) GetDlgItem(ID_DE_currfps))->SetWindowText(value);
    mDECamera->SetFramesPerSecond((double)camParam->DE_FramesPerSec);
  }
  if (isDE12 && mProtCoverDelay >= 0) {
    value.Format("%d", mProtCoverDelay);
    ((CWnd *) GetDlgItem(ID_DE_coverDelay))->SetWindowText(value);
    mDECamera->setIntProperty("Protection Cover Delay (milliseconds)", mProtCoverDelay);
  }
  if (isDE12 && mTemperSetpoint > -999) {
    value.Format("Setpoint(C): %d", mTemperSetpoint);
    ((CWnd *) GetDlgItem(ID_DE_temperSet))->SetWindowText(value);
    mDECamera->setIntProperty("Temperature Control - Setpoint (Celsius)", mTemperSetpoint);
  }
}

// Update the enable/disable state based on what is active, and manage protection cover
void DirectElectronToolDlg::Update()
{
  if (!mDECamera)
    return;

  CameraParameters *camParam = mWinApp->GetCamParams() + mWinApp->GetCurrentCamera();
  BOOL isDE12 = mDECamera->CurrentIsDE12();
  BOOL isSurvey = mDECamera->CurrentIsSurvey();
  BOOL isEither = isDE12 || isSurvey;

  int protCoverNeed = mProtCoverChoice;
  BOOL doingTasks = mWinApp->DoingTasks();
  BOOL busy = doingTasks || mWinApp->mCamera->CameraBusy();
  BOOL thisCamBusy = busy && camParam->DE_camType > 0;

  //The Lens coupled system is different than the direct detection system
  //Certain properties will be disabled or enabled for this or survey sensor
  ((CWnd *) GetDlgItem(IDC_DE_insertCam))->EnableWindow(isEither && !busy);
  ((CWnd *) GetDlgItem(IDC_DDD_Temp))->EnableWindow(!isSurvey);
  ((CWnd *) GetDlgItem(IDC_STAT_DETECTOR))->EnableWindow(!isSurvey);
  ((CWnd *) GetDlgItem(IDC_COOLCAM))->EnableWindow(!thisCamBusy && isDE12);
  ((CWnd *) GetDlgItem(ID_DE_temperSet))->EnableWindow(isDE12);
  ((CWnd *) GetDlgItem(ID_DE_waterlineTemp))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(IDC_STAT_AUTOSAVE_DIR))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(ID_DE_saveDir))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(ID_DE_autosaveF))->EnableWindow(isEither && !thisCamBusy);
  ((CWnd *) GetDlgItem(IDC_STAT_FORMAT_LABEL))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(IDC_STAT_FPSLABEL))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(IDC_STAT_FPSMAX_LABEL))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(ID_DE_maxfps))->EnableWindow(isEither);
  ((CWnd *) GetDlgItem(ID_DE_currfps))->EnableWindow(isEither && !thisCamBusy &&
    mDECamera->GetCurCamIndex() >= 0);
  ((CWnd *) GetDlgItem(IDC_STAT_PROT_COV))->EnableWindow(isDE12);
  ((CWnd *) GetDlgItem(IDC_STAT_PROT_DELAY))->EnableWindow(isDE12);
  ((CWnd *) GetDlgItem(ID_DE_coverDelay))->EnableWindow(!thisCamBusy && isDE12);
  ((CWnd *) GetDlgItem(ID_DE_protectMode))->EnableWindow(!thisCamBusy && isDE12);

  // Handle protection cover, open it for tasks that take images
  if (((mWinApp->DoingImagingTasks() || (mWinApp->mMacroProcessor->DoingMacro() && 
    mWinApp->mMacroProcessor->GetOpenDE12Cover())) && thisCamBusy) || 
    (mWinApp->mCamera->DoingContinuousAcquire() && camParam->DE_camType > 0))
    protCoverNeed = 0;
  if (isDE12 && (!mDECamera->GetLastLiveMode() || !protCoverNeed))
    SetProtectionCoverMode(protCoverNeed);
}

// Repond to possible change in settings
void DirectElectronToolDlg::UpdateSettings()
{
  ApplyUserSettings();
  Update();
}

void DirectElectronToolDlg::OnOK()
{
  OnEnKillfocusDecurrfps();
  OnEnKillfocusDecoverdelay();
}

// Return the FPS value in the text box
double DirectElectronToolDlg::GetFramesPerSecond(void)
{
  CString value =  "0";
  ((CWnd *) GetDlgItem(ID_DE_currfps))->GetWindowText(value);
  return atof(value);
}

// Change the state the protection if it does not match the incoming state
void DirectElectronToolDlg::SetProtectionCoverMode(int nIndex)
{
  if (nIndex == mLastProtCoverSet || !mDECamera)
    return;
  CString stringname = "Protection Cover Operation Mode";
  CString value = nIndex ? DE_OPEN_CLOSE_COVER : DE_KEEP_COVER_OPEN;
  mDECamera->setStringProperty(stringname, value);
  SEMTrace('D', "Protection cover set to %s", (LPCTSTR)value);
  mLastProtCoverSet = nIndex;
}

void DirectElectronToolDlg::DoDataExchange(CDataExchange *pDX)
{
  CDialog::DoDataExchange(pDX);
  //DDX_Control(pDX, IDC_NCMIRITEM2, mDETabControl);
}


BEGIN_MESSAGE_MAP(DirectElectronToolDlg, CToolDlg)
  ON_CBN_SELCHANGE(ID_DE_protectMode, Modechange)
  ON_BN_CLICKED(IDC_COOLCAM, &DirectElectronToolDlg::OnBnClickedCoolcam)
  ON_BN_CLICKED(IDC_DE_insertCam, &DirectElectronToolDlg::OnBnClickedinsert)
  ON_EN_KILLFOCUS(ID_DE_currfps, &DirectElectronToolDlg::OnEnKillfocusDecurrfps)
  ON_EN_KILLFOCUS(ID_DE_coverDelay, &DirectElectronToolDlg::OnEnKillfocusDecoverdelay)
  ON_CBN_SELCHANGE(ID_DE_autosaveF, FormatChange)
END_MESSAGE_MAP()


// DirectElectronToolDlg message handlers

// Change in selected protection cover mode
void DirectElectronToolDlg::Modechange()
{
  mWinApp->RestoreViewFocus();
  CComboBox  *corrComboBox = ((CComboBox *) GetDlgItem(ID_DE_protectMode));
  //Determine the index of the level that needs to be set for the attenuation
  int nIndex = corrComboBox->GetCurSel();

  //Sanity check, should never happen!
  if (nIndex > 1) {
    nIndex = 1;
  }
  SetProtectionCoverMode(nIndex);
  mProtCoverChoice = nIndex;
}

// Change in file format selection
void DirectElectronToolDlg::FormatChange()
{
  mWinApp->RestoreViewFocus();
  CComboBox  *corrComboBox = ((CComboBox *) GetDlgItem(ID_DE_autosaveF));
  int nIndex = corrComboBox->GetCurSel();

  CString stringname = "Autosave Frames - Format";
  if (nIndex == 0) {
    mDECamera->setStringProperty(stringname, "TIFF");
  } else if (nIndex == 1) {
    mDECamera->setStringProperty(stringname, mHdfMrcSaveOption);
  }
  CString value;
  mDECamera->getStringProperty(stringname, value);
  if (value != (nIndex ? mHdfMrcSaveOption : "TIFF")) {
    nIndex = 1 - nIndex;
    corrComboBox->SetCurSel(nIndex);
  }
  mFormatForAutoSave = nIndex;
}

// Coolin button clicked, warm or cool the camera
void DirectElectronToolDlg::OnBnClickedCoolcam()
{
  CString value = "";
  HRESULT hr = S_OK;
  CButton *button = (CButton*)GetDlgItem(IDC_COOLCAM);
  int state = button->GetCheck();
  int newState = state;
  mWinApp->RestoreViewFocus();
  mChangingCooling = true;
  if (mDECamera) {
    mDECamera->getStringProperty("Temperature Control", value);
    if (state && value.Compare("Cool Down") != 0) {
      if (AfxMessageBox("You are about to COOL down the camera. Are you sure?", 
        MB_YESNO | MB_ICONWARNING) == IDYES)
          hr = mDECamera->CoolDownCamera();
      else
        newState = 0;
    } else if (!state && value.Compare("Warm Up") != 0) {
      if (AfxMessageBox("You are about to WARM up the camera. Are you sure?", 
        MB_YESNO | MB_ICONWARNING) == IDYES)
          hr = mDECamera->WarmUPCamera();
      else
        newState = 1;
    }
    if (hr != S_OK) {
      value = mDECamera->ErrorTrace("Error changing cooling state of camera");
      SEMMessageBox(value);
      SEMErrorOccurred(1);
      newState = state ? 0 : 1;
    }
  }
  if (newState != state)
    button->SetCheck(newState);
  mChangingCooling = false;
}

// Insert or retract camera 
void DirectElectronToolDlg::OnBnClickedinsert()
{
  CString value = "";
  int result;
  CButton *button = (CButton*)GetDlgItem(IDC_DE_insertCam);
  int state = button->GetCheck();
  int newState = state;
  mWinApp->RestoreViewFocus();
  if (mDECamera) {
    mDECamera->getStringProperty("Camera Position Status", value);
    if (state && value.Compare("Extended") != 0) {
      result = mDECamera->insertCamera();
    } else if (!state && value.Compare("Retracted") != 0) {
      result = mDECamera->retractCamera();
    }
    if (result) {
      value = mDECamera->ErrorTrace("Error changing insertion state of camera");
      SEMMessageBox(value);
      SEMErrorOccurred(1);
      newState = state ? 0 : 1;
    }
  }
  if (newState != state)
    button->SetCheck(newState);
}

// Possible new value in the FPS box
void DirectElectronToolDlg::OnEnKillfocusDecurrfps()
{
  float maxFPS, temp_float = 0;
  if (!mDECamera)
    return;
  CString value;
  if (!(mDECamera->CurrentIsDE12() || mDECamera->CurrentIsSurvey()))
    return;
  double currentFPS = GetFramesPerSecond();
  mWinApp->RestoreViewFocus();
  mDECamera->getFloatProperty("Frames Per Second (Max)", maxFPS);
  if (currentFPS < maxFPS + 1.)
    currentFPS = B3DMIN(currentFPS, maxFPS);
  if (currentFPS >= 0 && currentFPS <= maxFPS) {

    // Read current setup. Change value if they are different from the Edittext
    mDECamera->getFloatProperty("Frames Per Second", temp_float);
    if (abs(temp_float - currentFPS) > 0.001) {
      mDECamera->SetFramesPerSecond(currentFPS);
    }
    value.Format("%0.2f", currentFPS);

  } else {
    value.Format("Please provide a valid frames per second between 0 to %.2f", maxFPS);
    AfxMessageBox(value);
    mDECamera->getFloatProperty("Frames Per Second", temp_float);
    value.Format("%0.2f", temp_float);
  }
  ((CWnd *) GetDlgItem(ID_DE_currfps))->SetWindowText(value);
}

// Possible new value in the cover delay box
void DirectElectronToolDlg::OnEnKillfocusDecoverdelay()
{
  if (!mDECamera || !mDECamera->CurrentIsDE12())
    return;
  CString value;
  mWinApp->RestoreViewFocus();
  ((CWnd *) GetDlgItem(ID_DE_coverDelay))->GetWindowText(value);
  int temp_int = atoi(value);
  if (temp_int >= 0 && temp_int <= 999) {

    // Read current setup. Change value if they are different from the Edittext
    int compate_int = 0;
    mDECamera->getIntProperty("Protection Cover Delay (milliseconds)", compate_int);
    if (compate_int != temp_int) {
      mDECamera->setIntProperty("Protection Cover Delay (milliseconds)", temp_int);
      mProtCoverDelay = temp_int;
    }
  } else {
    AfxMessageBox("Please provide a reasonable protection cover delay between 0 to 999"
      " milliseconds. ");
    mDECamera->getIntProperty("Protection Cover Delay (milliseconds)", temp_int);
    value.Format("%d", temp_int);
    ((CWnd *) GetDlgItem(ID_DE_coverDelay))->SetWindowText(value);
  }
}
