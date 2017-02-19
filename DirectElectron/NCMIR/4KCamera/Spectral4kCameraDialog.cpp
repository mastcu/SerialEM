// Spectral4kCameraDialog.cpp : implementation file
//

#include "stdafx.h"

#ifdef NCMIR_CAMERA

#include "../../../SerialEM.h"
#include "Spectral4kCameraDialog.h"


// Spectral4kCameraDialog dialog
#define UPDATE_CAMERA_STATUS (WM_APP +1)
// Spectral4kCameraDialog dialog
#define FIREFOX_PATH "\"C:\\Program Files\\Mozilla Firefox\\firefox.exe\" "
#include "../../../EMscope.h"
#include ".\spectral4kcameradialog.h"

static bool UPDATE_CAM_PANEL =false;


IMPLEMENT_DYNAMIC(Spectral4kCameraDialog, CDialog)
Spectral4kCameraDialog::Spectral4kCameraDialog(CWnd* pParent /*=NULL*/)
	: CBaseDlg(Spectral4kCameraDialog::IDD, pParent)
{
}

Spectral4kCameraDialog::~Spectral4kCameraDialog()
{
	UPDATE_CAM_PANEL = false;

	if(m_serialPortChiller.IsOpen()==true)
	{
		m_serialPortChiller.Close();
		
	}
}

struct ThreadParam
{
	HWND mDlg;
};

UINT Camera_4k_StatusThreadProc(LPVOID pParam)
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
void Spectral4kCameraDialog::setScopeReference(CEMscope* mScope)
{
	microscope = mScope;
}


void Spectral4kCameraDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}
LRESULT Spectral4kCameraDialog::OnUpdateCameraStatus(WPARAM wp, LPARAM lp)
{
	

	try{
		if(camera)
		{
			camera->readCameraParams();
		}

		if(m_serialPortChiller.IsOpen()==true)
			readChillerInformation();
		
		CString value;
		value.Format("%0.2f",camera->getCameraTemp());
		(( CWnd* ) GetDlgItem( IDC_CAMTEMP ) )->SetWindowText( value );
		value.Format("%0.2f",camera->getCameraBackplateTemp());
		(( CWnd* ) GetDlgItem( IDC_CAMBACKPLATETEMP ) )->SetWindowText( value );
		value.Format("%0.2d",camera->getCameraPressure());
		(( CWnd* ) GetDlgItem( IDC_CAMPRESSURE ) )->SetWindowText( value );
		value.Format("%0.2f",(float)camera->getCamVoltage()/1000.0);
		(( CWnd* ) GetDlgItem( IDC_CamVoltage ) )->SetWindowText( value );
		  if(camera->getWindowHeaterStatus()==1)
			value = "ON";
		  else
			value = "OFF";
  
		(( CWnd* ) GetDlgItem( IDC_WindowHeater ) )->SetWindowText( value );
		value.Format("%0.2f",m_chiller_flow_rate);
		(( CWnd* ) GetDlgItem( IDC_CHILLERFLOW ) )->SetWindowText( value );
		value.Format("%d",camera->getDSITime());
		(( CWnd* ) GetDlgItem( IDC_DSI_Sample_Time ) )->SetWindowText( value );
	
		CTime time = CTime::GetCurrentTime();
		value.Format("%.2d.%.2d.%d %.2d:%.2d:%.2d", time.GetDay(), time.GetMonth(), time.GetYear(), time.GetHour(), time.GetMinute(),time.GetSecond());
		(( CWnd* ) GetDlgItem( IDC_TIME_STAMP ) )->SetWindowText( value );

		CString previousPath;
		(( CWnd* ) GetDlgItem( IDC_WEB_URL ) )->GetWindowText(previousPath);
		CString urlPath = camera->getURLPath();
		if(urlPath != previousPath)
		{
			char systemPath[450];
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
			sprintf(systemPath,"%s%s",FIREFOX_PATH,urlPath);
			//AfxMessageBox(systemPath);
			//::system(systemPath);
		}
		(( CWnd* ) GetDlgItem( IDC_WEB_URL ) )->SetWindowText( urlPath );

	  
		UpdateData(false);
	}
	catch(...){
		//AfxMessageBox("Problem reading in temp and pressure from camera.");
	}

	return 1;
}

BOOL Spectral4kCameraDialog::OnInitDialog()
{
  CBaseDlg::OnInitDialog();


  //try to initialize serialPort communications
   m_ChillerConnectionSuccess = true;

   // Attempt to open the serial port (COM1)
   lChillerError = m_serialPortChiller.Open(_T("COM1"),0,0,false);
   if (lChillerError != ERROR_SUCCESS)
		m_ChillerConnectionSuccess = false;

	// Setup the serial port (9600,N81) using hardware handshaking
  lChillerError = m_serialPortChiller.Setup(CSerial::EBaud9600,CSerial::EData8,CSerial::EParNone,CSerial::EStop1);
	if (lChillerError != ERROR_SUCCESS)
		m_ChillerConnectionSuccess = false;
		

  lChillerError = m_serialPortChiller.Write("SE0\r");
	if (lChillerError != ERROR_SUCCESS)
		m_ChillerConnectionSuccess = false;

  if(lChillerError==ERROR_SUCCESS)
	readChillerInformation();


  //we want to start updates 
  UPDATE_CAM_PANEL = true;

  //read in the values from the camera and keep updating
  //the current status of the camera
  ThreadParam* param = new ThreadParam;
  param->mDlg = m_hWnd;  // A handle, not a dangerous 'this'
  AfxBeginThread(Camera_4k_StatusThreadProc, param);
  param = 0; // The other thread shall delete it


  /*Set all the Temperature and related Camera labels to fonts that 
  are easy to see*/
  CFont *m_Font = new CFont;
  m_Font->CreatePointFont(110, "Verdana");

  CStatic *Label = (( CStatic* ) GetDlgItem( IDC_CAMPRESSURE ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_CAMTEMP ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_CAMBACKPLATETEMP ) );
  Label->SetFont(m_Font);
  Label = (( CStatic* ) GetDlgItem( IDC_TIME_STAMP ) );
  Label->SetFont(m_Font);

  //Disable the Close button so user can not close dialog box.
  CMenu* mnu = this->GetSystemMenu(FALSE);
  mnu->EnableMenuItem( SC_CLOSE, MF_BYCOMMAND|MF_GRAYED);

  //Setup the Configuration Parameters
  CString value;
  value.Format("%ld",camera->getInstrumentModel());
  (( CWnd* ) GetDlgItem( IDC_INST_MODEL ) )->SetWindowText( value );
  value.Format("%ld",camera->getSerialNumber());
  (( CWnd* ) GetDlgItem( IDC_HEAD_SERIAL ) )->SetWindowText( value );
  value.Format("%ld",camera->getShutterDelay());
  (( CWnd* ) GetDlgItem( IDC_Shutter_Delay ) )->SetWindowText( value );
  value.Format("%ld",camera->getPreExposureDelay());
  (( CWnd* ) GetDlgItem( IDC_PreExposureDelay ) )->SetWindowText( value );
  value.Format("%0.2f",camera->getCCDSetPoint());
  (( CWnd* ) GetDlgItem( IDC_CCDSetPoint ) )->SetWindowText( value );


  value.Format("%0.2f",m_chiller_flow_rate);
  (( CWnd* ) GetDlgItem( IDC_CHILLERFLOW ) )->SetWindowText( value );
  
  if(camera->getWindowHeaterStatus()==1)
	  value = "ON";
  else
	  value = "OFF";
  
  (( CWnd* ) GetDlgItem( IDC_WindowHeater ) )->SetWindowText( value );


  //Set the default setting for the attenuation, set the correct gain settings.
  CComboBox   *attenComboBox = (( CComboBox  * ) GetDlgItem( IDC_AttenCombo ) );

  attenComboBox->SetCurSel(NO_GAIN_SETTINGS);
  camera->setGainSetting(NO_GAIN_SETTINGS);

  CString gainMsg;
  gainMsg += "You are currently set at the lowest GAIN setting also known as MODE 0. This is standard at NCMIR. (This is good if you are not doing LOW dose or cryo)\n ";
  gainMsg += "This results in being able to use the entire dynamic range of the CCD (full well of the CCD).\n";
  

  AfxMessageBox(gainMsg);

  //m_CCDB_ID = 0;

  (( CWnd* ) GetDlgItem( IDC_WEB_URL ) )->SetWindowText( "" );


  EnableToolTips(true);
  UpdateData(false);
  return FALSE;
}


void Spectral4kCameraDialog::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

void Spectral4kCameraDialog::setCameraReference(NCMIR4kCamera* cam)
{

	camera = cam;
}

void Spectral4kCameraDialog::readTempProbeInfo()
{
	
	DWORD dwBytesRead = 0;
    BYTE  abBufferTemp[100];
	BYTE  abBufferHum[100];
	//read temperature

	//m_tempProbe.Purge();
	m_tempProbe.Write("T\r");


	do
    {
        // Read data from the COM-port
        m_tempProbe.Read(abBufferTemp,sizeof(abBufferTemp),&dwBytesRead);
        if (dwBytesRead > 0)
        {
            //fprintf(stdout,"%d", abBuffer);
        }
    }
    while (dwBytesRead == sizeof(abBufferTemp));

	if(abBufferTemp[0]=='+')
	{
		char stringVal[100];
		sprintf(stringVal,"%c%c.%c",abBufferTemp[1],abBufferTemp[2],abBufferTemp[4]);
		m_roomTemperature = atof(stringVal);

	}
	else if(abBufferTemp[2]=='%')
	{
		char stringVal[100];
		sprintf(stringVal,"%c%c",abBufferTemp[0],abBufferTemp[1]);
		m_roomHumidity = atof(stringVal);

	}
	else
		m_roomTemperature = -1;

	m_tempProbe.Purge();

	m_tempProbe.Write("H\r");

	do
    {
        // Read data from the COM-port
        m_tempProbe.Read(abBufferHum,sizeof(abBufferHum),&dwBytesRead);
        if (dwBytesRead > 0)
        {
            //fprintf(stdout,"%d", abBuffer);
        }
    }
    while (dwBytesRead == sizeof(abBufferHum));
	char stringVal[100];

	if(abBufferHum[2]=='%')
	{
		char stringVal[100];
		sprintf(stringVal,"%c%c",abBufferHum[0],abBufferHum[1]);
		m_roomHumidity = atof(stringVal);

	}
	else if(abBufferHum[0]=='+')
	{
		char stringVal[100];
		sprintf(stringVal,"%c%c.%c",abBufferHum[1],abBufferHum[2],abBufferHum[4]);
		m_roomTemperature = atof(stringVal);

	}
	
	

}


void Spectral4kCameraDialog::readChillerInformation()
{

	DWORD dwBytesRead = 0;
    BYTE  abBuffer[100];


	lChillerError = m_serialPortChiller.Write("RL\r");
	if (lChillerError != ERROR_SUCCESS)
	{
		m_chiller_flow_rate = 0;
		return;
	}


    do
    {
        // Read data from the COM-port
        m_serialPortChiller.Read(abBuffer,sizeof(abBuffer),&dwBytesRead);
        if (dwBytesRead > 0)
        {
            //fprintf(stdout,"%d", abBuffer);
        }
    }
    while (dwBytesRead == sizeof(abBuffer));

	if(abBuffer[0]=='+')
	{

		
		char stringVal[100];
		sprintf(stringVal,"%c.%c",abBuffer[3],abBuffer[5]);
		m_chiller_flow_rate = atof(stringVal);
	}
	else
	{
		m_chiller_flow_rate = -1;
	}
		//m_chiller_flow_rate = atof(stringVal);
}

void Spectral4kCameraDialog::OnCbnSelchangeAttencombo()
{
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

void Spectral4kCameraDialog::OnBnClickedBlankbutton()
{
	// TODO: Add your control notification handler code here

	CButton   *BlankButton = (( CButton  * ) GetDlgItem( IDC_BlankButton ) );
	CButton   *UnBlankButton = (( CButton  * ) GetDlgItem( IDC_UnBlankButton ) );

	BlankButton->EnableWindow(0);
	UnBlankButton->EnableWindow(1);
	
	CString str;
	str.Format("Beam Blanked, Off the screen");
    (( CWnd* ) GetDlgItem( IDC_BLANKSTATE ) )->SetWindowText( str );

	microscope->BlankBeam(true);

	
}

void Spectral4kCameraDialog::OnBnClickedUnblankbutton()
{
	// TODO: Add your control notification handler code here
	CButton   *BlankButton = (( CButton  * ) GetDlgItem( IDC_BlankButton ) );
	CButton   *UnBlankButton = (( CButton  * ) GetDlgItem( IDC_UnBlankButton ) );

	BlankButton->EnableWindow(1);
	UnBlankButton->EnableWindow(0);

	CString str;
	str.Format("Beam UnBlanked, On the screen");
    (( CWnd* ) GetDlgItem( IDC_BLANKSTATE ) )->SetWindowText( str );
	microscope->BlankBeam(false);
}

/*void Spectral4kCameraDialog::OnBnClickedHeateron()
{
	// TODO: Add your control notification handler code here
	camera->windowHeaterOn();
}

void Spectral4kCameraDialog::OnBnClickedHeateroff()
{
	// TODO: Add your control notification handler code here
	camera->windowHeaterOff();
}*/

BEGIN_MESSAGE_MAP(Spectral4kCameraDialog, CDialog)
	ON_CBN_SELCHANGE(IDC_AttenCombo4K, OnCbnSelchangeAttencombo)
	ON_BN_CLICKED(IDC_BlankButton, OnBnClickedBlankbutton)
	ON_BN_CLICKED(IDC_UnBlankButton, OnBnClickedUnblankbutton)
END_MESSAGE_MAP()


// Spectral4kCameraDialog message handlers

#endif
