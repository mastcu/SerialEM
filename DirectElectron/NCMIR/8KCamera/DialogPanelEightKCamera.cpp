// DialogPanelEightKCamera.cpp : implementation file
//

#include "stdafx.h"
#include "../../../SerialEM.h"
#include "DialogPanelEightKCamera.h"


// DialogPanelEightKCamera dialog

#include "../../../SerialEMDoc.h"
#include "DialogPanelEightKCamera.h"

#define UPDATE_CAMERA_STATUS (WM_APP +1)
// Spectral4kCameraDialog dialog
#define FIREFOX_PATH "\"C:\\Program Files\\Mozilla Firefox\\firefox.exe\" "

#include "../../../EMscope.h"
//#include ".\dialogpaneleightkcamera.h"


static bool UPDATE_CAM_PANEL =false;

IMPLEMENT_DYNAMIC(DialogPanelEightKCamera, CDialog)
DialogPanelEightKCamera::DialogPanelEightKCamera(CWnd* pParent /*=NULL*/)
	: CBaseDlg(DialogPanelEightKCamera::IDD, pParent)
{
}

DialogPanelEightKCamera::~DialogPanelEightKCamera()
{
	UPDATE_CAM_PANEL = false;
}

struct ThreadParam
{
	HWND mDlg;
};

UINT Camera_8k_StatusThreadProc(LPVOID pParam)
{
	ThreadParam* p = static_cast<ThreadParam*> (pParam);
	while(UPDATE_CAM_PANEL==true)
	{
		::SendMessage(p->mDlg,UPDATE_CAMERA_STATUS,0,0);
		::Sleep(5000);

	}

	delete p;
	return TRUE;

}

/*void DialogPanelEightKCamera::OnBnClickedDarkacquire()
{
	// TODO: Add your control notification handler code here
	camera->setBinning(1,1,8096,8096);
	camera->AcquireDarkImage(1);
	AfxMessageBox("Done with Dark");
}

void DialogPanelEightKCamera::OnBnClickedFlatacquire()
{
	camera->setBinning(1,1,8096,8096);
	camera->setImageType(FLATFIELD_8k);
	camera->AcquireImage(1);
	AfxMessageBox("Done with Flats");
}

void DialogPanelEightKCamera::OnBnClickedSavedarkflat()
{
	camera->saveDarkFlats();
	AfxMessageBox("Done saving data");
}*/

void DialogPanelEightKCamera::setCameraReference(NCMIR8kCamera* cam)
{
	camera = cam;
}

void DialogPanelEightKCamera::setScopeReference(CEMscope* mScope)
{
	microscope = mScope;
}

LRESULT DialogPanelEightKCamera::OnUpdateCameraStatus(WPARAM wp, LPARAM lp)
{
	try{
		if(camera)
		{
			camera->readCameraParams();
		}
		float* values;
		long*  ret_values;
		values = camera->getCameraTemp();
		CString value;
		value.Format("%0.2f",values[0]);
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP_CAM1 ) )->SetWindowText( value );
		value.Format("%0.2f",values[1]);
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP_CAM2 ) )->SetWindowText( value );
		value.Format("%0.2f",values[2]);
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP_CAM3 ) )->SetWindowText( value );
		value.Format("%0.2f",values[3]);
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP_CAM4 ) )->SetWindowText( value );
		
		values = camera->getCameraBackplateTemp();
		value.Format("%0.2f",values[0]);
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP_CAM1 ) )->SetWindowText( value );
		value.Format("%0.2f",values[1]);
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP_CAM2 ) )->SetWindowText( value );
		value.Format("%0.2f",values[2]);
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP_CAM3 ) )->SetWindowText( value );
		value.Format("%0.2f",values[3]);
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP_CAM4 ) )->SetWindowText( value );

		ret_values = camera->getCameraPressure();
		value.Format("%ld",ret_values[0]);
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE_CAM1 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[1]);
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE_CAM2 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[2]);
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE_CAM3 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[3]);
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE_CAM4 ) )->SetWindowText( value );

		ret_values = camera->getDSITime();
		value.Format("%ld",ret_values[0]);
		(( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM1 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[1]);
		(( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM2 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[2]);
		(( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM3 ) )->SetWindowText( value );
		value.Format("%ld",ret_values[3]);
		(( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM4 ) )->SetWindowText( value );
		CString previousPath;
		(( CWnd* ) GetDlgItem( IDC_EDIT_URL ) )->GetWindowText(previousPath);
		CString urlPath = camera->getURLPath();

		if(urlPath != previousPath)
		{
			//char systemPath[1000];
			CString string;
			string.Format("%s%s",FIREFOX_PATH,urlPath);
		
  
			LPTSTR command = string.GetBuffer(0);
			
			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			std::memset(&pi, 0, sizeof(PROCESS_INFORMATION));
			std::memset(&si, 0, sizeof(STARTUPINFO));
			GetStartupInfo(&si);
			si.cb = sizeof(si);
			si.wShowWindow = SW_HIDE;
			si.dwFlags |= SW_HIDE;
			const BOOL res = CreateProcess(NULL,(LPTSTR)command,0,0,FALSE,0,0,0,&si,&pi);
			/*if (res) {
				WaitForSingleObject(pi.hProcess, INFINITE);
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}*/ 
			string.ReleaseBuffer();
			//sprintf(systemPath,"%s%s",FIREFOX_PATH,urlPath);
			//AfxMessageBox(systemPath);
			//::system(systemPath);
		}



		(( CWnd* ) GetDlgItem( IDC_EDIT_URL ) )->SetWindowText( urlPath );
		
		CTime time = CTime::GetCurrentTime();
		value.Format("%.2d.%.2d.%d %.2d:%.2d:%.2d", time.GetDay(), time.GetMonth(), time.GetYear(), time.GetHour(), time.GetMinute(),time.GetSecond());
		(( CWnd* ) GetDlgItem( IDC_TIME_STAMP ) )->SetWindowText( value );
	  
		UpdateData(false);
	}
	catch(...){
		//AfxMessageBox("Problem reading in temp and pressure from camera.");
	}

	return 1;
}

BOOL DialogPanelEightKCamera::OnInitDialog()
{
  CBaseDlg::OnInitDialog();

  //we want to start updates 
  UPDATE_CAM_PANEL = true;

  //read in the values from the camera and keep updating
  //the current status of the camera
  ThreadParam* param = new ThreadParam;
  param->mDlg = m_hWnd;  // A handle, not a dangerous 'this'
  AfxBeginThread(Camera_8k_StatusThreadProc, param);
  param = 0; // The other thread shall delete it


  /*Set all the Temperature and related Camera labels to fonts that 
  are easy to see*/
  CFont *m_Font = new CFont;
  m_Font->CreatePointFont(110, "Verdana");

  //Disable the Close button so user can not close dialog box.
  CMenu* mnu = this->GetSystemMenu(FALSE);
  mnu->EnableMenuItem( SC_CLOSE, MF_BYCOMMAND|MF_GRAYED);

  //Setup the Configuration Parameters
  CString value;
  float* values;
  long*  ret_values;

  ret_values = camera->getInstrumentModel();

  value.Format("%d",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL_CAM1 ) )->SetWindowText( value );
  value.Format("%d",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL_CAM2 ) )->SetWindowText( value );
  value.Format("%d",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL_CAM3 ) )->SetWindowText( value );
  value.Format("%d",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL_CAM4 ) )->SetWindowText( value );

  ret_values = camera->getSerialNumber();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL_CAM1 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL_CAM2 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL_CAM3 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL_CAM4 ) )->SetWindowText( value );

  ret_values = camera->getShutterDelay();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay_CAM1 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay_CAM2 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay_CAM3 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay_CAM4 ) )->SetWindowText( value );
 
  ret_values = camera->getPreExposureDelay();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM1 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM2 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM3 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM4 ) )->SetWindowText( value );
  values = camera->getCCDSetPoint();
  value.Format("%0.2f",values[0]);
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint_CAM1 ) )->SetWindowText( value );
  value.Format("%0.2f",values[1]);
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint_CAM2 ) )->SetWindowText( value );
  value.Format("%0.2f",values[2]);
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint_CAM3 ) )->SetWindowText( value );
  value.Format("%0.2f",values[3]);
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint_CAM4 ) )->SetWindowText( value );

  ret_values = camera->getCamVoltage();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_CamVoltage_CAM1))->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_CamVoltage_CAM2))->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_CamVoltage_CAM3))->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_CamVoltage_CAM4))->SetWindowText( value );

  ret_values = camera->getDSITime();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM1))->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM2))->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM3))->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time_CAM4))->SetWindowText( value );

  ret_values = camera->getPreExposureDelay();
  value.Format("%ld",ret_values[0]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM1 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[1]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM2 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[2]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM3 ) )->SetWindowText( value );
  value.Format("%ld",ret_values[3]);
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay_CAM4 ) )->SetWindowText( value );

  ret_values = camera->getWindowHeaterStatus();
  if(ret_values[0]==1) value = "ON"; else value = "OFF";  
  (( CWnd* ) GetDlgItem( IDC_WindowHeater_CAM1 ) )->SetWindowText( value );
  if(ret_values[1]==1) value = "ON"; else value = "OFF";
  (( CWnd* ) GetDlgItem( IDC_WindowHeater_CAM2 ) )->SetWindowText( value );
  if(ret_values[2]==1) value = "ON"; else value = "OFF";
  (( CWnd* ) GetDlgItem( IDC_WindowHeater_CAM3 ) )->SetWindowText( value );
  if(ret_values[3]==1) value = "ON"; else value = "OFF";
  (( CWnd* ) GetDlgItem( IDC_WindowHeater_CAM4 ) )->SetWindowText( value );

  CEdit* exposure_Time_edit = (CEdit*)GetDlgItem(IDC_CORR_SEC);
  exposure_Time_edit->SetWindowText("1.00"); 


  //Set the default setting for the attenuation, set the correct gain settings.
  CComboBox   *attenComboBox = (( CComboBox  * ) GetDlgItem( IDC_AttenCombo ) );

  attenComboBox->SetCurSel(NO_GAIN_SETTINGS);
  camera->setGainSetting(NO_GAIN_SETTINGS);

  CComboBox   *focusComboBox = (( CComboBox  * ) GetDlgItem( IDC_FocusCameraSelection ) );
  focusComboBox->SetCurSel(0);  //SET all camera selection
   
  CComboBox   *trialComboBox = (( CComboBox  * ) GetDlgItem( IDC_TrialSelectionCamera ) );
  trialComboBox->SetCurSel(0);  //SET all camera selection


  CString gainMsg;
  gainMsg += "You are currently set at the lowest GAIN setting also known as MODE 0. This is standard at NCMIR. (This is good if you are not doing LOW dose or cryo)\n ";
  gainMsg += "This results in being able to use the entire dynamic range of the CCD (full well of the CCD).\n";
  

  //AfxMessageBox(gainMsg);


  //set the ccdb id to zero initially.
  //m_CCDB_ID = 0;

  CButton *button = new CButton;
  button = reinterpret_cast<CButton *>(GetDlgItem(IDC_CORRECTED));
  button->SetCheck(1);
  int state = button->GetCheck();
  if(state==0){camera->setImageType(UNPROCESSED_8k);}	
  else if(state==1){camera->setImageType(CORRECTED_8k);}

  
  button = reinterpret_cast<CButton *>(GetDlgItem(IDC_RemapImages));
  state = button->GetCheck();
  camera->setRemapImages(state);







  EnableToolTips(true);
  UpdateData(false);
  return FALSE;
}

void DialogPanelEightKCamera::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

void DialogPanelEightKCamera::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(DialogPanelEightKCamera, CDialog)
	ON_CBN_SELCHANGE(IDC_FocusCameraSelection, FocusCameraSelection)
	ON_CBN_SELCHANGE(IDC_TrialSelectionCamera, TrialCameraSelection)
	ON_CBN_SELCHANGE(IDC_AttenCombo, GainSettingChange)
	ON_BN_CLICKED(IDC_RunFlat8K, OnBnClickedRunflat8k)
	ON_BN_CLICKED(IDC_CORRECTED, CorrectedCheckBox_Changed)
	ON_BN_CLICKED(IDC_RemapImages, RemapImagesCheckBoxEvent)
END_MESSAGE_MAP()


// DialogPanelEightKCamera message handlers

void DialogPanelEightKCamera::FocusCameraSelection()
{
	// TODO: Add your control notification handler code here
	// TODO: Add your control notification handler code here
	CComboBox   *focusComboBox = (( CComboBox  * ) GetDlgItem( IDC_FocusCameraSelection ) );
	//Determine the index of the level that needs to be set for the attenuation
	int nIndex = focusComboBox->GetCurSel();
	camera->setTargetFocusCam(nIndex);

	CString strMessage;

	if(nIndex==0)
		strMessage.Format("You will use all four cameras for your Focus setting.");
	else
		strMessage.Format("You have selected Camera %d to use for focusing" , nIndex);
	AfxMessageBox(strMessage);
}

void DialogPanelEightKCamera::TrialCameraSelection()
{
	// TODO: Add your control notification handler code here

	CComboBox   *trialComboBox = (( CComboBox  * ) GetDlgItem( IDC_TrialSelectionCamera ) );
	//Determine the index of the level that needs to be set for the attenuation
	int nIndex = trialComboBox->GetCurSel();
	camera->setTargetTrialCam(nIndex);

	CString strMessage;
	if(nIndex==0)
		strMessage.Format("You will use all four cameras for your Trial setting.");
	else
		strMessage.Format("You have selected Camera %d to use for tracking" , nIndex);
	AfxMessageBox(strMessage);
}

void DialogPanelEightKCamera::GainSettingChange()
{
	// TODO: Add your control notification handler code here
		// TODO: Add your control notification handler code here
	CComboBox   *attenComboBox = (( CComboBox  * ) GetDlgItem( IDC_AttenCombo ) );
	//Determine the index of the level that needs to be set for the attenuation
	int nIndex = attenComboBox->GetCurSel();

	//Sanity check, should never happen!
	if(nIndex > 2)
	{
		nIndex = 2;
	}

	CString attenMsg;

	//lowest Gain settings.
	if( nIndex == NO_GAIN_SETTINGS)
	{
		attenMsg += "You have chosen to use MODE  0 which allows you to take advantage of the full dynamic range of the camera\n ";
		attenMsg += "This means you will use the full CCD well, you will most likely have to use a longer exposure (3-4 seconds) as well as having the current density\n";
		attenMsg += "at a reasonable value.  You should double check your mean and standard deviation to see if meets your expectations.";
		
	}

	if(nIndex == MEDIUM_GAIN_SETTING || nIndex == HIGH_GAIN_SETTINGS)
	{
		attenMsg += "You have chosen a MEDIUM/HIGH GAIN/Attenuation setting \n ";
		attenMsg += "This gives you the best signal to noise ratio for low beam intensity situations.\n";
		attenMsg += "This is ideal for when you are using a short exposure time with the current density .";
		
	}

	//Gain Settings are stated
	//as  0 = Mode 0, 1 = Mode 1, 2 = Mode 2
	camera->setGainSetting(nIndex);

	AfxMessageBox(attenMsg);

	CString flatfieldMsg;
	flatfieldMsg.Format("Also you will need to acquire new flatfields and bias images when you change the Gain settings.  So dont forget to do this!!");
	AfxMessageBox(flatfieldMsg);
}

void DialogPanelEightKCamera::OnBnClickedRunflat8k()
{
	CString exposureTimeText;
	float exposure_time = 1.0;
	CEdit* exposure_time_edit = (CEdit*)GetDlgItem(IDC_CORR_SEC);
	exposure_time_edit->GetWindowText(exposureTimeText); 

	//Make sure the user entered in some number for each text Box.
	if(exposureTimeText.IsEmpty() == true) 
	{
		AfxMessageBox("You did not specify a number for the exposure time.");
		return;
	}
	exposure_time = atof(exposureTimeText);
	if(!exposure_time)
	{
		AfxMessageBox("You did not provide a valid number for the exposure time.");
		return;
	}
	if(exposure_time < 0 || exposure_time > 20)
	{
		AfxMessageBox("You need to select an exposure time greater than 0 and less than 20 seconds.");
		return;
	}


	AfxMessageBox("Ensure that you are in an empty part of your grid before clicking ok and proceding. Wait until the routine is done before proceding.");
	
	//screen up...
	microscope->SetScreenPos(0);

	CStatic* status_text = (CStatic*)GetDlgItem(IDC_FLAT_STATUS);
	status_text->SetWindowText("About to take Dark Image at 1x binning ...."); 
	
	//1x binning
	camera->setBinning(1,1,8096,8096);
	camera->AcquireDarkImage(exposure_time);
	status_text->SetWindowText("Done with Dark images about to take FlatField at 1x binning...."); 
	camera->setImageType(FLATFIELD_8k);
	camera->AcquireImage(exposure_time);
	status_text->SetWindowText("Done with FlatField at 1x, performing calculations and saving images....");
	camera->saveDarkFlats();

	//2x binning
	AfxMessageBox("Will now take it at 2x binning...");
	status_text->SetWindowText("About to take Dark Image at 2x binning ...."); 
	camera->setBinning(2,2,4096,4096);
	camera->AcquireDarkImage((float)exposure_time/2);
	status_text->SetWindowText("Done with Dark images about to take FlatField at 2x binning...."); 
	camera->setImageType(FLATFIELD_8k);
	camera->AcquireImage(exposure_time/2);
	camera->saveDarkFlats();

	//4x binning
	AfxMessageBox("Will now take it at 4x binning...");
	status_text->SetWindowText("About to take Dark Image at 4x binning ...."); 
	camera->setBinning(4,4,2048,2048);
	camera->AcquireDarkImage((float)exposure_time/4);
	status_text->SetWindowText("Done with Dark images about to take FlatField at 4x binning...."); 
	camera->setImageType(FLATFIELD_8k);
	camera->AcquireImage(exposure_time/4);
	camera->saveDarkFlats();

	//figure out the real mean that needs to be used.
	camera->CalcTrueMean();

	float mean_value = camera->getMeanFlatFieldValue();
	CString path = camera->getFlatFieldLocation();

	CString message;
	message.Format("Routine complete, the path of your saved images is:  %s \n and your mean value is:   %f.  \n Remember to half your exposure time for 2x bin images.",path,mean_value);
	AfxMessageBox(message);

	CString status;
	status.Format("Mean value of your flatfield is: %f", mean_value);
	status_text->SetWindowText(status);

	CButton *button = new CButton;
	button = reinterpret_cast<CButton *>(GetDlgItem(IDC_CORRECTED));
	int state = button->GetCheck();
	if(state==0){camera->setImageType(UNPROCESSED_8k);}
	else if(state==1){camera->setImageType(CORRECTED_8k);}


	//::SetClipboardText("something");

}

void DialogPanelEightKCamera::CorrectedCheckBox_Changed()
{
	// TODO: Add your control notification handler code here
	
	CButton *button = new CButton;
	button = reinterpret_cast<CButton *>(GetDlgItem(IDC_CORRECTED));
	int state = button->GetCheck();

	if(state==0)
	{
		camera->setImageType(UNPROCESSED_8k);
		AfxMessageBox("You will now be collecting UnProccessed Images!");
	}
	else if(state==1)
	{
		camera->setImageType(CORRECTED_8k);
		AfxMessageBox("You will now be collecting Gain Corrected Images, make sure your Flatfields and Dark Images have been taking today!");
	}
}

void DialogPanelEightKCamera::RemapImagesCheckBoxEvent()
{
	// TODO: Add your control notification handler code here
	CButton *button = new CButton;
	button = reinterpret_cast<CButton *>(GetDlgItem(IDC_RemapImages));
	int state = button->GetCheck();
	camera->setRemapImages(state);
}

void DialogPanelEightKCamera::OnOK()
{
	UpdateData(TRUE);

}
	
void DialogPanelEightKCamera::OnCancel()
{
	UpdateData(TRUE);

}