// DirectElectronCamPanel.cpp : implementation file
//

#include "stdafx.h"
#include "../SerialEM.h"
#include "DirectElectronCamPanel.h"
#include ".\directelectroncampanel.h"

#define UPDATE_CAMERA_STATUS (WM_APP +1)
// DirectElectronCamPanel dialog
static bool DE_UPDATE_PANEL = false;



IMPLEMENT_DYNAMIC(DirectElectronCamPanel, CDialog)
DirectElectronCamPanel::DirectElectronCamPanel(CWnd* pParent /*=NULL*/)
	: CBaseDlg(DirectElectronCamPanel::IDD, pParent)
{
	mNonModal = true;
}

DirectElectronCamPanel::~DirectElectronCamPanel()
{
	DE_UPDATE_PANEL = false;
}

struct ThreadParam
{
	HWND mDlg;
};


void DirectElectronCamPanel::OnOK()
{
	UpdateData(TRUE);

}
	
void DirectElectronCamPanel::OnCancel()	
{	
	UpdateData(TRUE);
}

///////////////////////////////////////////////////////////////////
//DirectElectronCamera_StatusThreadProc() routine.  Function will 
//run as a sepearte thread and update the dialog with the latest
//parameters 
///////////////////////////////////////////////////////////////////

UINT DirectElectronCamera_StatusThreadProc(LPVOID pParam)
{
	ThreadParam* p = static_cast<ThreadParam*> (pParam);
	while(DE_UPDATE_PANEL==true)
	{
		::SendMessage(p->mDlg,UPDATE_CAMERA_STATUS,0,0);
		::Sleep(2000);

	}

	delete p;
	return TRUE;

}


///////////////////////////////////////////////////////////////////
// OnUpdateCameraStatus() routine.  Will continue in a loop
// to read in the temperature and pressure of the camera.
// this will continue to run until the program terminates
///////////////////////////////////////////////////////////////////

LRESULT DirectElectronCamPanel::OnUpdateCameraStatus(WPARAM wp, LPARAM lp)
{
	

	try{


		DEcamera->readinTempandPressure();
	
		CString value;
		value.Format("%0.2f",DEcamera->getCameraTemp());
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP ) )->SetWindowText( value );
		value.Format("%0.2f",DEcamera->getCameraBackplateTemp());
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP ) )->SetWindowText( value );
		value.Format("%0.2d",DEcamera->getCameraPressure());
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE ) )->SetWindowText( value );

		value.Format("%0.2d",DEcamera->getShutterDelay());
		(( CWnd* ) GetDlgItem( IDC_Shutter_Delay ) )->SetWindowText( value );

		value.Format("%0.2d",DEcamera->getPreExposureDelay());
		(( CWnd* ) GetDlgItem( IDC_PreExposureDelay ) )->SetWindowText( value );
		

		if(DEcamera->getWindowHeaterStatus()==1)
			value = "ON";
		else
			value = "OFF";
		(( CWnd* ) GetDlgItem( IDC_WindowHeater ) )->SetWindowText( value );



		CTime time = CTime::GetCurrentTime();
		value.Format("%.2d.%.2d.%d %.2d:%.2d:%.2d", time.GetDay(), time.GetMonth(), time.GetYear(), time.GetHour(), time.GetMinute(),time.GetSecond());
		(( CWnd* ) GetDlgItem( IDC_TIME_STAMP ) )->SetWindowText( value );

	}
	catch(...){
		//AfxMessageBox(".");
	}


  
	UpdateData(false);

	return 1;
}



///////////////////////////////////////////////////////////////////
// OnInitDialog() routine.  Initialize the dialog and place the
// all the relevant information of the camera on the panel.
///////////////////////////////////////////////////////////////////
BOOL DirectElectronCamPanel::OnInitDialog()
{
  CBaseDlg::OnInitDialog();


  //we will want to continually run the thread.
  DE_UPDATE_PANEL = true;

  //read in the values from the camera and keep updating
  //the current status of the camera
  ThreadParam* param = new ThreadParam;
  param->mDlg = m_hWnd;  // A handle, not a dangerous 'this'
  AfxBeginThread(DirectElectronCamera_StatusThreadProc, param);
  param = 0; // The other thread shall delete it

  /*Set all the Temperature and related Camera labels to fonts that 
  are easy to see*/
  CFont *m_Font = new CFont;
  m_Font->CreatePointFont(80, "Verdana");

  CStatic *Label = (( CStatic* ) GetDlgItem( IDC_CAMPRESSURE ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_CAMTEMP ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_CAMBACKPLATETEMP ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_TIME_STAMP ) );
  Label->SetFont(m_Font);
 
  Label = (( CStatic* ) GetDlgItem( IDC_INST_MODEL ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_HEAD_SERIAL ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_Shutter_Delay ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_PreExposureDelay ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_HEAD_SERIAL ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_CCDSetPoint ) );
  Label->SetFont(m_Font);



  //Disable the Close button so user can not close dialog box.
  CMenu* mnu = this->GetSystemMenu(FALSE);
  mnu->EnableMenuItem( SC_CLOSE, MF_BYCOMMAND|MF_GRAYED);

  //Setup the Configuration Parameters
  CString value;
  value.Format("%ld",DEcamera->getInstrumentModel());
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL ) )->SetWindowText( value );
  value.Format("%ld",DEcamera->getSerialNumber());
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL ) )->SetWindowText( value );
  value.Format("%ld",DEcamera->getShutterDelay());
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay ) )->SetWindowText( value );
  value.Format("%ld",DEcamera->getPreExposureDelay());
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay ) )->SetWindowText( value );
  value.Format("%0.2f",DEcamera->getCCDSetPoint());
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint ) )->SetWindowText( value );

  
  if(DEcamera->getWindowHeaterStatus()==1)
	  value = "ON";
  else
	  value = "OFF";
  
  (( CWnd* ) GetDlgItem( IDC_WindowHeater ) )->SetWindowText( value );


  //Set the default setting for the attenuation, set the correct gain settings.
  CComboBox   *attenComboBox = (( CComboBox  * ) GetDlgItem( IDC_AttenCombo ) );

  attenComboBox->SetCurSel(MODE_0);

  //Default to Mode 0
  SEMTrace('D', "DE_CAMERA: Mode 0 is being set.");
  DEcamera->setGainSetting(MODE_0);

  CString gainMsg;
  gainMsg += "The camera is currently set to Mode 0";
  

  //AfxMessageBox(gainMsg);


  EnableToolTips(true);
  UpdateData(false);
  return FALSE;
}


void DirectElectronCamPanel::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

void DirectElectronCamPanel::setCameraReference(DirectElectronCamera* cam)
{

	DEcamera = cam;
}



void DirectElectronCamPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(DirectElectronCamPanel, CDialog)
ON_MESSAGE(UPDATE_CAMERA_STATUS, OnUpdateCameraStatus) //Custom message for updating camera status
ON_CBN_SELCHANGE(IDC_AttenCombo, GainSettingChange)
ON_BN_CLICKED(IDC_ShutterSetButton, OnBnClickedShuttersetbutton)
ON_BN_CLICKED(IDC_ReadPressure, OnBnClickedReadpressure)
ON_BN_CLICKED(IDC_NCMIRITEM1, OnBnClickedWindowheateron)
ON_BN_CLICKED(IDC_NCMIRITEM2, OnBnClickedWindowheateroff)
ON_BN_CLICKED(IDC_NCMIRITEM3, OnBnClickedTecOn)
ON_BN_CLICKED(IDC_NCMIRITEM4, OnBnClickedTecOff)
END_MESSAGE_MAP()


// DirectElectronCamPanel message handlers

void DirectElectronCamPanel::GainSettingChange()
{
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
	if( nIndex == MODE_0)
	{
		attenMsg += "You have chosen to use Mode 0 setting.  \n ";
		SEMTrace('D', "DE_CAMERA: Mode 0 is being set.");
	}

	else if(nIndex == MODE_1)
	{
		attenMsg += "You have chosen the MODE1 Attenuation setting .  \n ";
		SEMTrace('D', "DE_CAMERA: Mode 1 is being set.");
	}
		
	else if(nIndex == MODE_2)
	{
		attenMsg += "You have chosen the MODE2 Attenuation setting .  \n ";
		SEMTrace('D', "DE_CAMERA: Mode 2 is being set.");
		
	}

	//Gain Settings are stated
	//as  0 = MODE_0, 1 = MODE_1, 2 = MODE_2
	DEcamera->setGainSetting(nIndex);

	//AfxMessageBox(attenMsg);

	CString flatfieldMsg;
	flatfieldMsg.Format("You will need to acquire new flatfields and bias images when you change the Gain settings.");
	AfxMessageBox(flatfieldMsg);


}

//OnBnClickedShuttersetbutton()
//button will check the user input for a valid shutter close delay in ms
void DirectElectronCamPanel::OnBnClickedShuttersetbutton()
{
	CString shutterCloseStr;
	int shutterDelay;

	CEdit* shutter_close_edit = (CEdit*)GetDlgItem(IDC_ShutterCloseEdit);
	shutter_close_edit->GetWindowText(shutterCloseStr); 

	//Check to make sure the user entered in something
	if((shutterCloseStr.IsEmpty() == true))
	{
		AfxMessageBox("You did not specify a number for the shutter delay!");
		return;
	}

	shutterDelay = atoi(shutterCloseStr);

	if(!shutterDelay || shutterDelay < MIN_SHUTTER_DELAY || shutterDelay > MAX_SHUTTER_DELAY)
	{
		AfxMessageBox("You did not provide a valid number for shutter delay (0-4095) milliseconds.");
		return;
	}

	DEcamera->setShutterDelay(shutterDelay);



}


//just temporary
void DirectElectronCamPanel::OnBnClickedReadpressure()
{

	CString str;

	for(int i = 0 ;i < 22; i++)
	{
		long param = DEcamera->getValue(i);
		
		CString value;
		value.Format("Value number %d is %ld\n",i,param);
		str +=  value;
	}
	AfxMessageBox(str);
}

void DirectElectronCamPanel::OnBnClickedWindowheateron()
{
	DEcamera->turnWindowHeaterON();

}

void DirectElectronCamPanel::OnBnClickedWindowheateroff()
{
	DEcamera->turnWindowHeatherOFF();
}

void DirectElectronCamPanel::OnBnClickedTecOn()
{
	DEcamera->turnTEC_ON();
}

void DirectElectronCamPanel::OnBnClickedTecOff()
{
	DEcamera->turnTEC_OFF();
}
