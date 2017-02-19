#include "stdafx.h"

#include "NCMIR8kcamera.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <Magick++.h>
using namespace Magick;


//Number of lines in a camera properties file
#define NUM_OF_LINES 20

//Number of lines in a remap properties file
#define NUM_OF_REMAP_LINES 5
#define REMAP_FILE_LOCATION "C:\\Program Files\\SerialEM\\8kRemapData.txt"
#define FILE_PROPERTIES "C:\\Program Files\\SerialEM\\Spectral8kProperties.txt"
#define NUM_OF_CAMERAS 4

#define REMAP_CODE_PATH "C:\\Program Files\\NCMIR\\remapQTdeploy\\QTRemapVerificationApp.exe"

static CImg<unsigned short> map_img;
static CImg<unsigned short> map_4k;

enum camSelection {ALL=0,CAM1,CAM2,CAM3,CAM4};

NCMIR8kCamera::NCMIR8kCamera(void){}
NCMIR8kCamera::~NCMIR8kCamera(void){uninitialize();}

int NCMIR8kCamera::uninitialize()
{

	for(int i=0; i< NUM_OF_CAMERAS; i++)
	{
		if(m_FSMObject[i])
		{
			m_FSMObject[i]->FreeShort();
			m_FSMObject[i]->Release();
			m_FSMObject[i] = NULL;
		}
		if(m_CaptureInterface[i])
		{
			m_CaptureInterface[i]->Release();
			m_CaptureInterface[i]=NULL;
		}
		
	}
	/*for(int i=0; i< NUM_OF_CAMERAS; i++)
		if(m_Sink[i])
		{
			delete m_Sink[i];

		}*/

	if(m_SinkCam1)
		delete m_SinkCam1;
	if(m_SinkCam2)
		delete m_SinkCam2;
	if(m_SinkCam3)
		delete m_SinkCam3;
	if(m_SinkCam4)
		delete m_SinkCam4;

	m_SinkCam1 = NULL;
	m_SinkCam2 = NULL;
	m_SinkCam3 = NULL;
	m_SinkCam4 = NULL;
	//if(m_FSMObject8kRemap)
	//	m_FSMObject8kRemap->FreeShort();
	//if(RemapInterface)
	//	RemapInterface->Release();

	//if(image8k)
		//delete image8k;
    
	//CoUninitialize();
	return 1;

}



int NCMIR8kCamera::initialize()
{
	HRESULT hr;
	//image8k = new unsigned short[MAX_IMAGE_FIELD];
	//read in a properties file to know how to assign
	//Read in the information as this is very critical 
	int check;
	check = readCameraProperties();
	if(check==-1)
		return -1; //couldnt read the properties so return.

	//check = readRemapProperties();
	//need to eventually read cross correlation numbers
	
	a[0].x_Offset = 3703;
	a[0].y_Offset = -8;
	a[1].x_Offset = 3724;
	a[1].y_Offset = 3747;
	a[2].x_Offset = -15;
	a[2].y_Offset = 3720;





	/*
	CoCreateInstance(CLSID_FSM_MemoryObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IFSM_MemoryObject, (void **)&m_FSMObject8kRemap);

	//need to have room for 4 images
	m_FSMObject8kRemap->AllocateShort(MAX_INDIVIDUAL_IMAGE_SIZE, 4);*/


	int dcfX,dcfY;
	dcfX = dcfY = 0;
	for(int i=0; i< NUM_OF_CAMERAS; i++)
	{
		hr=	CoCreateInstance(CLSID_CoCameraCtrl, NULL, CLSCTX_INPROC_SERVER,
			IID_ICameraCtrl, (void **)&m_CaptureInterface[i]);
		if(!SUCCEEDED(hr)) return -1;

		hr = CoCreateInstance(CLSID_FSM_MemoryObject, NULL, CLSCTX_INPROC_SERVER,
			IID_IFSM_MemoryObject, (void **)&m_FSMObject[i]);
		if(!SUCCEEDED(hr)) return -1;
		m_CaptureInterface[i]->SetDCFPath(dcfX,dcfY,dcf_file[i]);
		//adjust the array to load each one into a separate DCF path
		if(dcfY==1){dcfX = 1; dcfY = 0;}
		else if(dcfY==0){dcfY = 1;}

		m_FSMObject[i]->AllocateShort(MAX_INDIVIDUAL_IMAGE_SIZE, 1);
		m_FSMObject[i]->GetDataPtrShort(0, &m_pCapturedDataShort[i]);

		//m_FSMObject8kRemap->GetDataPtrShort(i, &m_pCapturedDataShort[i]);
		//m_CaptureInterface[i]->SetImageBuffer(m_FSMObject8kRemap);
		m_CaptureInterface[i]->SetImageBuffer(m_FSMObject[i]);
	
		if(i==0)
			m_SinkCam1 = new Sink8k(m_CaptureInterface[i],m_pCapturedDataShort[i]);
		else if(i==1)
			m_SinkCam2 = new Sink8kCam2(m_CaptureInterface[i],m_pCapturedDataShort[i]);
		else if(i==2)
			m_SinkCam3 = new Sink8kCam3(m_CaptureInterface[i],m_pCapturedDataShort[i]);
		else if(i==3)
			m_SinkCam4 = new Sink8kCam4(m_CaptureInterface[i],m_pCapturedDataShort[i]);


		m_CaptureInterface[i]->put_COMPort(camera_port_number[i]);
		if(peltier_cooler[i]=="ON")
				m_CaptureInterface[i]->ExternalRelayTurnOn();
			else
				m_CaptureInterface[i]->ExternalRelayTurnOff();

		m_CaptureInterface[i]->SetOrigin(16,0);
		m_CaptureInterface[i]->SetParamTablePath(filename[i],tablefilename[i]);
		m_CaptureInterface[i]->SetConfigTablePath(filename[i],tablefilename[i]);

		m_CaptureInterface[i]->AbortReadout();
		m_CaptureInterface[i]->ClearFrame();
		//read out everything from the camera and place it in the
		//current registers
		m_CaptureInterface[i]->Status_TransToHost();
		m_CaptureInterface[i]->ConfigParam_TransToHost();
		m_CaptureInterface[i]->ReadoutParam_TransAllToHost();

		long param;
		m_CaptureInterface[i]->get_ConfigParam(INSRUMENT_MODEL,&param);
		m_instrument_model[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(SERIAL_HEAD,&param);
		m_head_serial[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(SHUTTER_CLOSE_DELAY,&param);
		m_shutter_close_delay[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(PRE_EXPOSE_DELAY,&param);
		m_pre_expose_delay[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(CCD_TEMP_SETPOINT,&param);
		m_ccd_temp_setPoint[i] = (float)((param/10) - 273.15);
		m_CaptureInterface[i]->get_ConfigParam(CAMERA_VOLTAGE,&param);
		m_camera_Voltage[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(CAMERA_DSI_TIME,&param);
		m_dsi_time[i] = param;
		m_CaptureInterface[i]->get_ConfigParam(WINDOW_HEATER,&param);
		m_window_heater[i] = param;

		//Turn on Window Heater, it really should always be on
		m_CaptureInterface[i]->put_ConfigParam(WINDOW_HEATER,1);
		m_CaptureInterface[i]->put_ConfigParam(CCD_TEMP_SETPOINT,2230);
		m_CaptureInterface[i]->ConfigParam_RecAllFromHost();
	}

	//At this point it would be good to check the pressure and temperature of the
	//camera before proceding
	readinTempandPressure();
	CString openingMessage("");
	for(int i=0;i < NUM_OF_CAMERAS;i++)
	{
		CString temp;
		temp.Format("Camera %d has a Pressure: %d  and Temperature:  %0.2f C\n",i+1,m_CameraPressures[i],m_CameraTemperatures[i]);
		openingMessage += temp;
	}


	AfxMessageBox(openingMessage);


	//give it some initial value.
	mflat_avg_value = 5000;
	m_SinkCam1->setMValue(mflat_avg_value);
	m_SinkCam2->setMValue(mflat_avg_value);
	m_SinkCam3->setMValue(mflat_avg_value);
	m_SinkCam4->setMValue(mflat_avg_value);


	//SET up interfaces for remapping the images.
	//Get Remap Interface


	return 1;
}


void NCMIR8kCamera::setImageType(int type)
{
	m_SinkCam1->setImageType(type);
	m_SinkCam2->setImageType(type);
	m_SinkCam3->setImageType(type);
	m_SinkCam4->setImageType(type);
}

void NCMIR8kCamera::setPathtoGainImages(CString path)
{
	mflatFieldPath = path;
	m_SinkCam1->setPathtoGainImages(path);
	m_SinkCam2->setPathtoGainImages(path);
	m_SinkCam3->setPathtoGainImages(path);
	m_SinkCam4->setPathtoGainImages(path);
}

void NCMIR8kCamera::setApachePath(CString ApachePath,CString ApacheURL)
{
	mApachePath = ApachePath;
	mApacheURL = ApacheURL;
}

void NCMIR8kCamera::CalcTrueMean()
{

	mflat_avg_value = (m_SinkCam1->getMValue() + m_SinkCam2->getMValue() + m_SinkCam3->getMValue() + m_SinkCam4->getMValue())/4;
	mflat_avg_value_2x = (m_SinkCam1->getMValue_2x() + m_SinkCam2->getMValue_2x() + m_SinkCam3->getMValue_2x() + m_SinkCam4->getMValue_2x())/4;
	mflat_avg_value_4x = (m_SinkCam1->getMValue_4x() + m_SinkCam2->getMValue_4x() + m_SinkCam3->getMValue_4x() + m_SinkCam4->getMValue_4x())/4;
	
	m_SinkCam1->setMValue(mflat_avg_value);
	m_SinkCam2->setMValue(mflat_avg_value);
	m_SinkCam3->setMValue(mflat_avg_value);
	m_SinkCam4->setMValue(mflat_avg_value);
	m_SinkCam1->setMValue_2x(mflat_avg_value_2x);
	m_SinkCam2->setMValue_2x(mflat_avg_value_2x);
	m_SinkCam3->setMValue_2x(mflat_avg_value_2x);
	m_SinkCam4->setMValue_2x(mflat_avg_value_2x);
	m_SinkCam1->setMValue_4x(mflat_avg_value_4x);
	m_SinkCam2->setMValue_4x(mflat_avg_value_4x);
	m_SinkCam3->setMValue_4x(mflat_avg_value_4x);
	m_SinkCam4->setMValue_4x(mflat_avg_value_4x);
}

void NCMIR8kCamera::setMetaDataThumbNail(double x,double y,double z,double defocus,CString magIndex,double TiltAngle)
{
	mCurrx = x;
	mCurry = y;
	mCurrz = z;
	mCurrdefocus = defocus;
	mCurrMagindex = magIndex;
	mCurrTiltAngle = TiltAngle;
}

void NCMIR8kCamera::setTiltSeriesInfo(BOOL isTiltSeries)
{
	misTiltSeries = isTiltSeries;

}
	

void NCMIR8kCamera::setMontageInfo(BOOL isMontage, int xNframes, int yNframes)
{
	misMontage = isMontage;
	mXNframes = xNframes;
	mYNframes = yNframes;

}

void NCMIR8kCamera::SaveImages(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle,unsigned short* image,int sizeX,int sizeY, double meanCounts)
{

	//Build up the path to save the data
	CString directoryPath;		
	directoryPath.Format("%s\\CCDBid_%ld_%s",path,ccdbID,basename);
	CreateDirectory((LPCTSTR)directoryPath,NULL);

	CString apachePath;
	apachePath.Format("%s\\CCDBid_%ld_%s",mApachePath,ccdbID,basename);
	CreateDirectory((LPCTSTR)apachePath,NULL);

	CString urlPath;
	urlPath.Format("%s/CCDBid_%ld_%s.html",mApacheURL,ccdbID,basename);
	mURLPath = urlPath;
	
	CString allImagesurlPath;
	allImagesurlPath.Format("%s/CCDBid_%ld_%s_allimages.html",mApacheURL,ccdbID,basename);

	CString montageurlPath;
	montageurlPath.Format("%s/CCDBid_%ld_%s_montage.html",mApacheURL,ccdbID,basename);
	
	CString allImageshtmlPath;
	allImageshtmlPath.Format("%s\\CCDBid_%ld_%s_allimages.html",mApachePath,ccdbID,basename);
	CString montagehtmlPath;
	montagehtmlPath.Format("%s\\CCDBid_%ld_%s_montage.html",mApachePath,ccdbID,basename);




	CTime ctdt = CTime::GetCurrentTime();
	CString timeStamp;
    timeStamp.Format("_%4d%02d%02d%02d%02d%02d", ctdt.GetYear(), ctdt.GetMonth(), ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());


	//CImg<unsigned char> thumbnail = map_img.get_resize(256,256);
	char thumbnailpath[180];
	sprintf(thumbnailpath,"%s\\CCDBid_%ld_%s_%03d.jpg",apachePath,ccdbID,basename,sectionNumber);
	CImg<unsigned short> image2save;
	image2save.assign(image,sizeX,sizeY);



	if(misMontage)
	{
		char montage_thumbnailpath[180];
		sprintf(montage_thumbnailpath,"%s\\CCDBid_%ld_%s_%03d_montage.jpg",apachePath,ccdbID,basename,sectionNumber);
		CImg<unsigned short> montage_normalized_thumbnail(128,128);
		montage_normalized_thumbnail.assign(map_img.get_resize(128,128));
		montage_normalized_thumbnail.normalize(0,65000);

		Image montage_thumb;
		montage_thumb.size( "128x128");
		montage_thumb.depth(16);
		montage_thumb.type(GrayscaleType);

		PixelPacket *montageimagepixels = montage_thumb.getPixels(0, 0, 128, 128);

		for(int i=0 ;i <128*128;i++)
		{
			montageimagepixels->red = montage_normalized_thumbnail[i];
			montageimagepixels->blue = montage_normalized_thumbnail[i];
			montageimagepixels->green = montage_normalized_thumbnail[i];
			montageimagepixels++;
		}
		montage_thumb.syncPixels();
		montage_thumb.write(montage_thumbnailpath);

	}

	unsigned short white[] = { 65000,65000,65000 }; 
	unsigned short black[] = {0,0,0};
	CImg<unsigned short> normalized_thumbnail(256,256);
	normalized_thumbnail.assign(image2save.get_resize(256,256));
	normalized_thumbnail.normalize(0,65000);
	char thumbnailMessage[400];
	sprintf(thumbnailMessage,"Name: %s CCDB ID: %ld\nSec: %d Tilt: %0.2f%c Mean: %0.1f Time: %02d\:%02d\:%02d Temp: %0.2f%c %0.2f%c %0.2f%c %0.2f%c\nPres: %ld %ld %ld %ld",basename,ccdbID,sectionNumber,TiltAngle,0x00B0,meanCounts,ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(),m_CameraTemperatures[0],0x00B0,m_CameraTemperatures[1],0x00B0,m_CameraTemperatures[2],0x00B0,m_CameraTemperatures[3],0x00B0,m_CameraPressures[0],m_CameraPressures[1],m_CameraPressures[2],m_CameraPressures[3]);
	char thumbnailMessage1[200];
	sprintf(thumbnailMessage1,"X:%0.2f Y:%0.2f Z:%0.2f Mag:%s DF:%0.2f",mCurrx,mCurry,mCurrz,mCurrMagindex,mCurrdefocus);
	normalized_thumbnail.draw_text(thumbnailMessage,0,0,white,black,1.0);
	normalized_thumbnail.draw_text(thumbnailMessage1,0,245,white,black,1.0);

	//normalized_thumbnail.display();
	
	Image thumb;
	thumb.size( "256x256");
	thumb.depth(16);
	thumb.type(GrayscaleType);

	PixelPacket *imagepixels = thumb.getPixels(0, 0, 256, 256);

	for(int i=0 ;i <256*256;i++)
	{
		imagepixels->red = normalized_thumbnail[i];
		imagepixels->blue = normalized_thumbnail[i];
		imagepixels->green = normalized_thumbnail[i];
		imagepixels++;
	}
	thumb.syncPixels();
	thumb.write(thumbnailpath);
	



	ofstream fout;
	CString htmlPath;
	htmlPath.Format("%s\\CCDBid_%ld_%s.html",mApachePath,ccdbID,basename);
	fout.open(htmlPath,ios::out);
	fout<<"<html>"<<endl;
    fout<<"<head><script type=\"text/JavaScript\">\n<!--\nfunction timedRefresh(timeoutPeriod) {";
	fout<<"setTimeout(\"location.reload(true);\",timeoutPeriod);}\n// -->";
	fout<<"\n</script>"<<endl;
	fout<<"<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" media=\"screen\" />";
	fout<<"</head><body onload=\"JavaScript:timedRefresh(15000);\">";

	fout<<"<h1>"<<"CCDB ID: "<<ccdbID<<" BaseName:  "<<basename<<"</h1><br/>"<<endl;
	fout<<"<h2>"<<ctdt.GetMonth()<<"/"<<ctdt.GetDay()<<"/"<< ctdt.GetYear()<<"</h2><br/>"<<endl;
	fout<<"<br/><br/><br/><br/>"<<endl;
	
	CString time;
	time.Format("Time: %02d\:%02d\:%02d",ctdt.GetHour(), ctdt.GetMinute(),ctdt.GetSecond());
	fout<<"<h3>"<<time<<"</h3><br/>";
	fout<<"<h3>"<<"Current Status: </h3><br/>"<<endl;
	fout<<"<h3>"<<"Has taken "<<sectionNumber+1<<" total image(s) so far.</h3><br/>"<<endl;

	if(misTiltSeries)
	{
		fout<<"<h3>"<<"Doing Tilt Series....</h3><br/>"<<endl;
		if(sectionNumber > 0)
		{
			
			std::list<Image> imagesList;
			char writeImagesLocation[200];
			sprintf(writeImagesLocation,"%s\\animatedGIF.gif",apachePath);
			for(int i=0;i <sectionNumber+1;i++)
			{
				char readImagesLocation[200];
				sprintf(readImagesLocation,"%s\\CCDBid_%ld_%s_%03d.jpg",apachePath,ccdbID,basename,i);
				imagesList.push_back(Image(readImagesLocation));

			}
			writeImages( imagesList.begin(), imagesList.end(), writeImagesLocation); 
			CString animatedGifPath;
			animatedGifPath.Format("CCDBid_%ld_%s/animatedGIF.gif",ccdbID,basename);
			fout<<"<img src=\""<<animatedGifPath<<"\"/>"<<endl;
			fout<<"<br/><br/>"<<endl;
		}

	
	}
	if(misMontage)
		fout<<"<h3>"<<"Doing Montage of size:  X:"<<mXNframes<<"  Y:"<<mYNframes<<"</h3><br/>"<<endl;



	CString heater[4];
	short heaterValue;
	for(int i =0;i<4;i++)
	{
		heaterValue = m_window_heater[i];
		if(heaterValue==1)
			heater[i].Format("ON");
		else
			heater[i].Format("OFF");
	}
		
	fout<<"<h3>"<<"Camera Model:    Cam1: "<<m_instrument_model[0]<<" Cam2: "<<m_instrument_model[1]<<" Cam3: "<<m_instrument_model[2]<<" Cam4:" <<m_instrument_model[3]<<"</h3><br/>"<<endl;
	fout<<"<h3>"<<"Serial Number:   Cam1: "<<m_head_serial[0]<<" Cam2: "<<m_head_serial[1]<<" Cam3: "<<m_head_serial[2]<<" Cam4: "<<m_head_serial[3]<<"</h3><br/>"<<endl;
	fout<<"<h3>"<<"Window Heater:   Cam1: "<<heater[0]<<" Cam2: "<<heater[1]<<" Cam3: "<<heater[2]<<" Cam4: "<<heater[3]<<"</h3><br/>"<<endl;
	fout<<"<h3>"<<"Camera Temp(C):  Cam1: "<<m_CameraTemperatures[0]<<" Cam2: "<<m_CameraTemperatures[1]<<" Cam3: "<<m_CameraTemperatures[2]<<" Cam4: "<<m_CameraTemperatures[3]<<"</h3><br/>"<<endl;
	fout<<"<h3>"<<"Camera Pressure: Cam1: "<<m_CameraPressures[0]<<" Cam2: "<<m_CameraPressures[1]<<" Cam3: "<<m_CameraPressures[2]<<" Cam4: "<<m_CameraPressures[3]<<"</h3><br/>"<<endl;
	
	for(int i=0;i<4;i++)
	{
		if((m_CameraTemperatures[i] < -50) || (m_CameraPressures[i] < 10000))
			fout<<"<h3> Camera "<< i+1<<" is working properly</h3><br/>"<<endl;
		else
			fout<<"<h3>PROBLEM WITH CAMERA "<<i+1<<" </h3><br/>"<<endl;
	}
	
	fout<<"<br/><br/><br/><br/>"<<endl;
	



	fout<<"<h4>Most Recent Image (with the last 10 images following it):</h4><br/>"<<endl;
	fout<<"<h4>View All images: <a href=\""<<allImagesurlPath<<"\">all images</a></h4><br/>"<<endl;
	if(misMontage)
		fout<<"<h4>Montage Link: <a href=\""<<montageurlPath<<"\">Montage Link</a></h4><br/>"<<endl;




	int row_count=0;
	int total_image_display = 0;

	for(int i = sectionNumber; i >= 0; i--, total_image_display++)
	{

		if(total_image_display >=10) break;

		CString imagepath;
		imagepath.Format("CCDBid_%ld_%s/CCDBid_%ld_%s_%03d.jpg\"",ccdbID,basename,ccdbID,basename,i);
		fout<<"<img src=\""<<imagepath<<"/>"<<endl;
		if(row_count==3)
		{
			fout<<"<br/><br/>"<<endl;
			row_count =0;
		}
		else row_count++;
	}

	fout<<"</body></html>"<<endl;
	fout.flush();
	fout.close();

	//montage Page if we are doing one
	if(misMontage)
	{
		fout.open(montagehtmlPath,ios::out);
		fout<<"<html>"<<endl;
		fout<<"<head><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" media=\"screen\" />";
		fout<<"</head>";

	
		int count;
		count = mXNframes;
		int x = mXNframes;
		for(int i= 0 ;i < mXNframes; i++)
		{
			for(int j=0; j < mYNframes; j++)
			{
				CString imagepath;
				imagepath.Format("CCDBid_%ld_%s/CCDBid_%ld_%s_%03d_montage.jpg\"",ccdbID,basename,ccdbID,basename,count);
				fout<<"<img src=\""<<imagepath<<"/>"<<endl;
				count = x + mYNframes;
			}	
			x--;
			fout<<"<br/><br/>"<<endl;
		}
		fout<<"</body></html>"<<endl;
		fout.flush();
		fout.close();


	}

	//all images Page
	fout.open(allImageshtmlPath,ios::out);
	fout<<"<html>"<<endl;
    fout<<"<head><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" media=\"screen\" />";
	fout<<"</head>";

	fout<<"<h1>"<<"All images:</h1><br/>"<<endl;
	row_count=0;

	for(int i = 0; i <= sectionNumber; i++)
	{
		CString imagepath;
		imagepath.Format("CCDBid_%ld_%s/CCDBid_%ld_%s_%03d.jpg\"",ccdbID,basename,ccdbID,basename,i);
		fout<<"<img src=\""<<imagepath<<"/>"<<endl;
		if(row_count==3)
		{
			fout<<"<br/><br/>"<<endl;
			row_count =0;
		}
		else row_count++;
	}

	fout<<"</body></html>"<<endl;
	fout.flush();
	fout.close();


	m_SinkCam1->saveImage(directoryPath,basename,ccdbID,sectionNumber,timeStamp);
	m_SinkCam2->saveImage(directoryPath,basename,ccdbID,sectionNumber,timeStamp);
	m_SinkCam3->saveImage(directoryPath,basename,ccdbID,sectionNumber,timeStamp);
	m_SinkCam4->saveImage(directoryPath,basename,ccdbID,sectionNumber,timeStamp);

	//should we launch the remap thread?  Only if its the
	//the full 4k x 4k
/*
	if(mRemapState==1)
	{
		//launch a thread to run the remapping code
		//pass in four images and save the result in the same place
		char command[512];
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		std::memset(&pi, 0, sizeof(PROCESS_INFORMATION));
		std::memset(&si, 0, sizeof(STARTUPINFO));
		GetStartupInfo(&si);
		si.cb = sizeof(si);
		si.wShowWindow = SW_HIDE;
		si.dwFlags |= SW_HIDE;
		const BOOL res = CreateProcess(NULL,(LPTSTR)command,0,0,FALSE,0,0,0,&si,&pi);



	}*/

}



void NCMIR8kCamera::readCameraParams()
{
	long param;
	CSingleLock slock(&m_mutex);
		
	if(slock.Lock(1000))
	{
		for(int i=0; i< NUM_OF_CAMERAS;i++)
		{
			//readout from the camera first...
			m_CaptureInterface[i]->Status_TransToHost();
			m_CaptureInterface[i]->ConfigParam_TransToHost();
			//now you have the current values
			m_CaptureInterface[i]->get_StatusParam(CAMERA_TEMP,&param);
			m_CameraTemperatures[i] = (float)((param/10) - 273.15);
			m_CaptureInterface[i]->get_StatusParam(BACK_PLATE_TEMP,&param);
			m_Camera_BackPlateTemp[i] = (float)((param/10) - 273.15);
			m_CaptureInterface[i]->get_StatusParam(CAMERA_PRESSURE,&param);
			m_CameraPressures[i] = (float)param;
			m_CaptureInterface[i]->get_StatusParam(CAMERA_VOLTAGE,&param);
			m_camera_Voltage[i] = param;
			m_CaptureInterface[i]->get_ConfigParam(WINDOW_HEATER,&param);
			m_window_heater[i] = param;
			m_CaptureInterface[i]->get_ReadoutParam(CAMERA_DSI_TIME,&param);
			m_dsi_time[i] = param;
		}
	}
	return;
	

}

float* NCMIR8kCamera::getCameraTemp(){return m_CameraTemperatures;}
float* NCMIR8kCamera::getCameraBackplateTemp(){return m_Camera_BackPlateTemp;}
long* NCMIR8kCamera::getCameraPressure(){return m_CameraPressures;}
int NCMIR8kCamera::readCameraProperties()
{
	    string  camera_properties[NUM_OF_LINES];
		ifstream inFile;
		inFile.open(FILE_PROPERTIES);

		if (!inFile) {
			 AfxMessageBox("Could not read Spectral 8k properties file, ensure Spectral8kProperties.txt exists!", MB_OK | MB_ICONEXCLAMATION);
			 return -1;   
		}
		if (inFile.is_open())
		{	 
			 int i=0;

			 try{
				while (i < NUM_OF_LINES)
				{   
					getline (inFile,camera_properties[i++]);	
				}
				inFile.close();
			 }catch(...)
			 {
				return -1;
			 }
		}

		int k=0;
		for(int i=0; i< NUM_OF_CAMERAS;i++)
		{
			//should read in :  C:\\camerafiles\\SI1100.dcf
			dcf_file[i] = SysAllocString(A2BSTR(camera_properties[k++].c_str()));
			//should read in :  C:\\camerafiles\\1100-106 250kHz.set
			filename[i] = SysAllocString(A2BSTR(camera_properties[k++].c_str()));
			//should read in a C:\\camerafiles\\x4839A+2.str
			tablefilename[i] = SysAllocString(A2BSTR(camera_properties[k++].c_str()));
			//should read in the port number, typically 3
			camera_port_number[i] = atoi(camera_properties[k++].c_str() );
			//Determine if the TEC should automatically turn on upon init
			//IF "ON" is not found then will proceed to turn off the TEC cooler.
			peltier_cooler[i] = camera_properties[k++];

		}
		return 1;
}

void NCMIR8kCamera::readinTempandPressure()
{
	long param;
	for(int i=0; i< NUM_OF_CAMERAS;i++)
	{
		m_CaptureInterface[i]->get_StatusParam(0,&param);
		m_CameraTemperatures[i] = (float)((param/10) - 273.15);
		m_CaptureInterface[i]->get_StatusParam(2,&param);
		m_CameraPressures[i] = (float)param;
	}
}

void NCMIR8kCamera::applyCropPad(int xPad, int yPad)
{
	XPAD = xPad;
	YPAD = yPad;
}

int NCMIR8kCamera::setBinning(int x, int y,int sizeX, int sizeY)
{

	CSingleLock slock(&m_mutex);
	if(slock.Lock(1000))
	{
		int IMAGE_SIZE_OFFSET = 4096;
		IMAGE_SIZE_OFFSET = IMAGE_SIZE_OFFSET/x;

		

		for(int i=0; i < NUM_OF_CAMERAS;i++)
		{
			
			/*if(m_FSMObject[i])
			{
				m_FSMObject[i]->FreeShort();
				m_FSMObject[i]->Release();
				
			}*/
			//need to actually take a bigger size image, but then later crop.
			sizeX = (sizeX/2 + (IMAGE_SIZE_OFFSET - sizeX/2))*2;
			sizeY = (sizeY/2 + (IMAGE_SIZE_OFFSET - sizeY/2))*2;
			m_CaptureInterface[i]->SetImageSize((sizeX/4),(sizeY/4));
			m_CaptureInterface[i]->SetBinning(x,y);
			mbinFactor = x;

			//to support various sizes.
			//YUP magic numbers are bad, but in this case it makes sense
			//to see the numbers off the bat.  
			int OriginX = (2048 + 16) - ((sizeX/2 * x) / 2);
			int OriginY = (2048 - ((sizeY/2 *y )/ 2));

			m_CaptureInterface[i]->SetOrigin( OriginX,OriginY);
			m_FSMObject[i]->AllocateShort((sizeX/2 * sizeY/2),1);
			
			//TEMP
			//m_FSMObject[i]->GetDataPtrShort(0, &m_pCapturedDataShort[i]);
			//m_CaptureInterface[i]->SetImageBuffer(m_FSMObject[i]);
			
		}

		for(int i=0 ; i<3;i++)
		{
			offset[i].x_Offset = a[i].x_Offset / mbinFactor;
			offset[i].y_Offset = a[i].y_Offset / mbinFactor;
		}
		
	}

	return 1;

}

int NCMIR8kCamera::setImageSize(int sizeX, int sizeY)
{
	CSingleLock slock(&m_mutex);
	
	if(slock.Lock(1000))
	{
		for(int i=0; i < NUM_OF_CAMERAS;i++)
			m_CaptureInterface[i]->SetImageSize(sizeX, sizeY);
	}
	
	return 1;
}

void NCMIR8kCamera::setShutter(IShutterBoxPtr s)
{
	shutter = s;
}

int NCMIR8kCamera::AcquireImage(float seconds)
{

	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{
		m_SinkCam1->setDataIsNotReady();
		m_SinkCam2->setDataIsNotReady();
		m_SinkCam3->setDataIsNotReady();
		m_SinkCam4->setDataIsNotReady();

		ACQUIRING_DATA = true;
		STOPPING_ACQUSITION = false;

	
		
		for(int i=0;i< NUM_OF_CAMERAS;i++)
		{
			m_CaptureInterface[i]->put_ExposureTime((long)((float)(seconds * 1000)));
			m_CaptureInterface[i]->CCD_ReadAndExpose();
		
		}	
		if(shutter) 
		{
			//::Sleep(100);
			/*shutter->ShutterMode(1);
			shutter->SendCommand(22);
			shutter->ShutterMode(0);
			shutter->SendCommand(22); 

			float time2sleep = (seconds*1000 - float(1000/seconds))
			if(time2sleep <= 0)
				::Sleep(seconds*1000);
			else
				::Sleep(time2sleep);
			shutter->ShutterMode(1);
			shutter->SendCommand(22); */
		}
		while(!isImageAcquistionDone()) //wait until the image is finish acquiring
					;
	}

	return 1;
}

int NCMIR8kCamera::AcquireDarkImage(float seconds)
{

	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{	
		//this is also a dark image
		m_SinkCam1->setImageType(DARK_8k);
		m_SinkCam1->setDataIsNotReady();
		m_SinkCam2->setImageType(DARK_8k);
		m_SinkCam2->setDataIsNotReady();
		m_SinkCam3->setImageType(DARK_8k);
		m_SinkCam3->setDataIsNotReady();
		m_SinkCam4->setImageType(DARK_8k);
		m_SinkCam4->setDataIsNotReady();

		ACQUIRING_DATA = true;
		STOPPING_ACQUSITION = false;
		for(int i=0;i<NUM_OF_CAMERAS;i++)
		{
			
			m_CaptureInterface[i]->put_ExposureTime((long)((float)seconds *1000));
			m_CaptureInterface[i]->CCD_ReadNoExpose();
		}

		while(!isImageAcquistionDone()) //wait until the image is finish acquiring
					;
	}

	return 1;

}

void NCMIR8kCamera::StopAcquistion()
{


	for(int i=0;i<NUM_OF_CAMERAS;i++)
	{
		m_CaptureInterface[i]->AbortReadout();
		m_CaptureInterface[i]->ClearFrame();
	}

	STOPPING_ACQUSITION = true;
	
}

/*Is all the data ready from the four cameras??*/
bool NCMIR8kCamera::isImageAcquistionDone()
{
	
	bool check;

	if(STOPPING_ACQUSITION==true)
	{	
		return true;

	}
	else
	{
		check = m_SinkCam1->checkDataReady();
		if(check == false)
			return false;
		check = m_SinkCam2->checkDataReady();
		if(check == false)
			return false;
		check = m_SinkCam3->checkDataReady();
			if(check == false)
				return false;
		check = m_SinkCam4->checkDataReady();
			if(check == false)
				return false;
	}
}

int NCMIR8kCamera::setOrigin(int x, int y)
{
	for(int i=0;i<NUM_OF_CAMERAS;i++)
		m_CaptureInterface[i]->SetOrigin(x,y);
	return 1;
}


void NCMIR8kCamera::saveDarkFlats()
{
	m_SinkCam1->factorCalculations();
	m_SinkCam2->factorCalculations();
	m_SinkCam3->factorCalculations();
	m_SinkCam4->factorCalculations();

	mflat_avg_value = (m_SinkCam1->getMValue() + m_SinkCam2->getMValue() + m_SinkCam3->getMValue() + m_SinkCam4->getMValue())/4;

	m_SinkCam1->setMValue(mflat_avg_value);
	m_SinkCam2->setMValue(mflat_avg_value);
	m_SinkCam3->setMValue(mflat_avg_value);
	m_SinkCam4->setMValue(mflat_avg_value);

	CString note;
	note.Format("Calculated mean value for your flatfield is: %f",mflat_avg_value);
	//AfxMessageBox(note);

}

float NCMIR8kCamera::getMeanFlatFieldValue(){return mflat_avg_value;}
CString NCMIR8kCamera::getFlatFieldLocation(){return mflatFieldPath;}


int NCMIR8kCamera::reAllocateMemory(int mem)
{
	for(int i=0;i<NUM_OF_CAMERAS;i++)
	{
		long datasize;

		//m_FSMObject[i]->get_DataSize(&datasize);

		//if( datasize > 0)
			//m_FSMObject[i]->FreeShort();

		m_FSMObject[i]->AllocateShort(mem,1);
		m_FSMObject[i]->GetDataPtrShort(0, &m_pCapturedDataShort[i]);
		m_CaptureInterface[i]->SetImageBuffer(m_FSMObject[i]);
	}
		
		//m_FSMObject8kRemap->AllocateShort(mem,4);
	return 1;
}

void NCMIR8kCamera::setGainSetting(short index)
{
	//4K camera init instance.
	//NOTE: below the determination of the DSI time and gain
	//attenuation was from the Camera Report manual.
	
	CSingleLock slock(&m_mutex);
	
	if(slock.Lock(1000))
	{
		for(int i=0;i<NUM_OF_CAMERAS;i++)
		{
			int dsi_time_setting;
			m_CaptureInterface[i]->put_ReadoutParam(ATTENUATION_PARAM, 0);
			if(index==0) 
			{
				dsi_time_setting = LOW_DSI_TIME;
				m_CaptureInterface[i]->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}
			else if(index==1) 
			{
				dsi_time_setting = MED_DSI_TIME;
				m_CaptureInterface[i]->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}
			else if(index==2) 
			{
				dsi_time_setting = HIGH_DSI_TIME;
				m_CaptureInterface[i]->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}
			//update the Camera with the latest readout values
			m_CaptureInterface[i]->ReadoutParam_RecAllFromHost();
		}
	}	
}



int NCMIR8kCamera::copyImageData(unsigned short *image8k, int ImageSizeX, int ImageSizeY, int LeftC, int TopC, int BottomC, int RightC)
{
	//CImg<unsigned short> map_img(image8k,ImageSizeX,ImageSizeY,1,1,true); 
	if(STOPPING_ACQUSITION==true)
	{
		
		for(int i=0; i < ImageSizeY; i++)
			for(int j=0; j < ImageSizeX; j++)
				image8k[j*ImageSizeX + i] = 0;


		STOPPING_ACQUSITION = false;
		return 1;
	}

	int mode = getImageMode();	
	int cam2use=0;
		
	//Focus Param
	if(mode == 1)
		cam2use= getTargetFocusCam();
	else if(mode == 2) //Trial Param		
		cam2use = getTargetTrialCam();
	else if(mode == 3 || mode==5) //Record and tracking..		
		cam2use = getTargetTrialCam();
	//check to see if we are just returning one image

	map_img.assign(image8k,ImageSizeX,ImageSizeY,1,1,true);
	int index = 0;
	unsigned short* pointer2data;

	int Width = ImageSizeX;
	int Height = ImageSizeY;

	pointer2data = m_SinkCam1->getData();
	int width = m_SinkCam1->getImageSizeX();
	int height = m_SinkCam1->getImageSizeY();

	int j=0,k=0;
	map_4k.assign(pointer2data,width,height,1,1,true);
	
	int cam1width,cam1height;
	int cam2width,cam2height;
	int cam3width,cam3height;
	int cam4width,cam4height;
	

	cam1width = m_SinkCam1->getImageSizeX();
	cam1height = m_SinkCam1->getImageSizeY();
	cam2width = m_SinkCam2->getImageSizeX();
	cam2height = m_SinkCam2->getImageSizeY();
	cam3width = m_SinkCam3->getImageSizeX();
	cam3height = m_SinkCam3->getImageSizeY();
	cam4width = m_SinkCam4->getImageSizeX();
	cam4height = m_SinkCam4->getImageSizeY();

	int CROP_OFFSET = 28;
	int CAMERA_SIZE = 4044;
	int IMAGE2XEXTRAOFFSET = 14;
	int IMAGE2XOFFSET = 3780;
	int IMAGE4YOFFSET = 3806;
	int IMAGE3XOFFSET = 3800;
	int IMAGE3YOFFSET = 3810;
	

	CROP_OFFSET = CROP_OFFSET/mbinFactor;
	CAMERA_SIZE = CAMERA_SIZE/mbinFactor;
	IMAGE2XEXTRAOFFSET = IMAGE2XEXTRAOFFSET/mbinFactor;
	IMAGE2XOFFSET = IMAGE2XOFFSET/mbinFactor;
	IMAGE4YOFFSET = IMAGE4YOFFSET/mbinFactor;
	IMAGE3XOFFSET = IMAGE3XOFFSET/mbinFactor;
	IMAGE3YOFFSET = IMAGE3YOFFSET/mbinFactor;

	//RECORD
	//If just selecting one camera.
	//if(mode != 3  && cam2use > 0){
	if(cam2use > 0){
		
		//SEMTrace('K', "8K: About to assign image 1 to the 8k image");
		if(cam2use == CAM1) map_4k.assign(m_SinkCam1->getData(),cam1width,cam1height,1,1,true);
		if(cam2use == CAM2) map_4k.assign(m_SinkCam2->getData(),cam2width,cam2height,1,1,true);
		if(cam2use == CAM3) map_4k.assign(m_SinkCam3->getData(),cam3width,cam3height,1,1,true);	
		if(cam2use == CAM4) map_4k.assign(m_SinkCam4->getData(),cam4width,cam4height,1,1,true);
			
		//Correct logic for x,y
		for(int i = 0, int top = TopC; i < ImageSizeY;i++,top++)//TopC is Y
			for(int j = 0, int bottom = LeftC; j < ImageSizeX;j++,bottom++) //LeftC is X	
				map_img(j,i) = map_4k(bottom + CROP_OFFSET,top + CROP_OFFSET);
			
		SEMTrace('K', "8K: Single camera selection ALL done assigning. Time to render.");
		return 1;	
	}


		SEMTrace('K', "8K: About to assign image 1 to the 8k image");	
		map_4k.assign(m_SinkCam1->getData(),cam1width,cam1height,1,1,true);

		
		//map_4k.display();
		//IMAGE 1
		for(int y = 0, int top= TopC; top < (CAMERA_SIZE); y++,top++)
			for(int x = 0,int left = LeftC; left < (CAMERA_SIZE);x++,left++)
				map_img(x,y) = map_4k(left + CROP_OFFSET + IMAGE2XEXTRAOFFSET,top + CROP_OFFSET);//negative value on cam2.
		//move over +7 for image4 along with +14 as normal....
		//map_img.display();
		SEMTrace('K', "8K: Done assigning image1.  About to start on image 2");

		//map_img.display();
		//IMAGE 2
		map_4k.assign(m_SinkCam2->getData(),cam2width,cam2height,1,1,true);
		for(int y=0, int top = TopC; top < (CAMERA_SIZE); y++, top++)		
			for(int x=0; x < (CAMERA_SIZE + IMAGE2XEXTRAOFFSET-LeftC); x++)// j < CAMERA_SIZE + IMAGE2XEXTRAOFFSET; j++)
				map_img(x + IMAGE2XOFFSET-LeftC, y) = map_4k(x + CROP_OFFSET,top + CROP_OFFSET);
				//map_img(j+IMAGE2XOFFSET,i) = map_4k(j + CROP_OFFSET,i + CROP_OFFSET);
		//map_img.display();

		SEMTrace('K', "8K: Done with image 2 . About to assign image 3 to the 8k image");
	
	//	map_img.display();
		//IMAGE 3
		
		map_4k.assign(m_SinkCam3->getData(),cam3width,cam3height,1,1,true);
		for(int y=0; y < CAMERA_SIZE-TopC;y++)// i < CAMERA_SIZE; i++)
			for(int x=0; x < (CAMERA_SIZE - LeftC); x++)
				if(mbinFactor==1)
					map_img(x+3800-LeftC,y+3790-TopC) = map_4k(x+28,y+28);//3790
				else
					map_img(x + IMAGE3XOFFSET-LeftC,y+ IMAGE3YOFFSET-TopC) = map_4k(x + CROP_OFFSET,y + CROP_OFFSET);

		SEMTrace('K', "8K: Done with image 3 . About to assign image 4 to the 8k image");
		//map_img.display();
			//IMAGE 4

		map_4k.assign(m_SinkCam4->getData(),cam4width,cam4height,1,1,true);
		for(int y=0;y<((CAMERA_SIZE-TopC));y++) //<CAMERA_SIZE ;y++)
			for(int x=0,int left = LeftC;x < CAMERA_SIZE - 20-LeftC; x++,left++)
				map_img(x,y+IMAGE4YOFFSET-TopC) = map_4k(left + CROP_OFFSET,y + CROP_OFFSET);
		//because we did a plus 7 on image 1, no need to do it for image4
		//map_img.display();

		SEMTrace('K', "8K: Done with image 4 .  ALL done assigning. Time to render.");

		
		ACQUIRING_DATA = false;
		
//TEMP
	/*	if(m_FSMObject[i])
		{
			m_FSMObject[i]->FreeShort();
			m_FSMObject[i]->Release();
			m_FSMObject[i] = NULL;
		}*/
		//map_img.display();
		return 1;

	//}
	/*else{
	
		//IMAGE 1
		for(int i=0;i<4044;i++)
			for(int j=0;j<4044;j++)
				map_img(j,i) = map_4k(j+28+14,i+28);//negative value on cam2.
		//map_img.display();
		//IMAGE 2
		pointer2data = m_SinkCam2->getData();
		map_4k.assign(pointer2data,cam2width,cam2height,1,1,true);
		for(int i=0;i<4044;i++)
			for(int j=0;j<4044+14;j++)
				map_img(j+IMAGE2XOFFSET,i) = map_4k(j+28,i+28);
		//map_img.display();
		//IMAGE 4
		pointer2data = m_SinkCam4->getData();
		map_4k.assign(pointer2data,cam4width,cam4height,1,1,true);
		for(int i=0;i<4044;i++)
			for(int j=0;j < 4044; j++)
				map_img(j,i+IMAGE4YOFFSET) = map_4k(j+28,i+28);
		//IMAGE 3
		//map_img.display();
		pointer2data = m_SinkCam3->getData();
		map_4k.assign(pointer2data,cam3width,cam3height,1,1,true);
		for(int i=0; i < 4044; i++)
			for(int j=0; j < 4044; j++)
				map_img(j+3800,i+3790) = map_4k(j+28,i+28);//3790
		ACQUIRING_DATA = false;
	}*/



	return 1;

}



int NCMIR8kCamera::setDebugMode()
{
	debug_mode = 1;
	return 1;
}


long* NCMIR8kCamera::getCamVoltage()
{
	return m_camera_Voltage;
}

long* NCMIR8kCamera::getInstrumentModel()
{
	return m_instrument_model;
}

long* NCMIR8kCamera::getSerialNumber()
{
	return m_head_serial;
}

long* NCMIR8kCamera::getShutterDelay()
{
	return m_shutter_close_delay;
}

long* NCMIR8kCamera::getPreExposureDelay()
{
	return m_pre_expose_delay;
}
    
float* NCMIR8kCamera::getCCDSetPoint()
{
	return m_ccd_temp_setPoint;
}
 
long* NCMIR8kCamera::getWindowHeaterStatus()
{
	return m_window_heater;
}
long* NCMIR8kCamera::getDSITime()
{
	return m_dsi_time;
}

/*******************************REMAPPING CODE******************************************/

int NCMIR8kCamera::readRemapProperties()
{
	string  remap_properties[NUM_OF_REMAP_LINES];
	ifstream inFile;
	inFile.open(REMAP_FILE_LOCATION);

	//Just have some default values of the remap parameters set just in case
	//for some reason the reading from the file fails

	int coords[4][10] = {{15, 16, 3793, -5, 23, 3776, 3795, 3784, 3770, 3770},
	{189, 36, 3961, 9, 194, 3805, 3987, 3802, 3770, 3770},
	{198, 211, 3974, 206, 201, 3990, 3984, 3983, 3770, 3770},
	{6, 197, 3800, 195, 20, 3990, 3802, 3975, 3770, 3770}};
	
	int i=0;
	for(int k = 0; k < 4 ; k++, i=0)
	{
		camCoords[k].XUL = coords[k][i++];
		camCoords[k].YUL = coords[k][i++];
		camCoords[k].XUR = coords[k][i++];
		camCoords[k].YUR = coords[k][i++];
		camCoords[k].XLL = coords[k][i++];
		camCoords[k].YLL = coords[k][i++];
		camCoords[k].XLR = coords[k][i++];
		camCoords[k].YLR = coords[k][i++];
	}

	if (!inFile) {
			 AfxMessageBox("Could not read Remap properties file, ensure 8kRemapData.txt exists, will resort to default values!", MB_OK | MB_ICONEXCLAMATION);
			 return 1;   
	}

	if (inFile.is_open())
	{	 
		int i=0;

		try{

			vector<string> tokens;
			vector<string>::iterator pos;
			const string delimiters = ",";
			while (i < NUM_OF_REMAP_LINES)
			{   
				getline (inFile,remap_properties[i]);	
				// Skip delimiters at beginning.
				string::size_type lastPos = remap_properties[i].find_first_not_of(delimiters, 0);
				// Find first "non-delimiter".
				string::size_type pos = remap_properties[i].find_first_of(delimiters, lastPos);

				while (string::npos != pos || string::npos != lastPos)
				{
					int position = pos - 2;
                    if(position < 0) pos - 1;
					// Found a token, add it to the vector.
					tokens.push_back(remap_properties[i].substr(lastPos, position));
					// Skip delimiters.  Note the "not_of"
					lastPos = remap_properties[i].find_first_not_of(delimiters, pos);
					// Find next "non-delimiter"
					pos = remap_properties[i].find_first_of(delimiters, lastPos);
				}
				i++;
			}
			inFile.close();

			int i=0;
			for(int k = 0; k < 4 ; k++, i=0)
			{
				char buffer[128];
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].XUL = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].YUL = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].XUR = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].YUR = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].XLL = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].YLL = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].XLR = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
				camCoords[k].YLR = atoi(buffer);
				strcpy(buffer, tokens[i++].c_str());
			}
			 
		}
		catch(...)
		{
			return -1;
		}
	}

	
	return 1;
}

int NCMIR8kCamera::remapData(unsigned short *image8k, int ImageSizeX, int ImageSizeY)
{
	int index=0;
	int Width = ImageSizeX;
	int Height = ImageSizeY;
	
	int CROP_NUM_OF_PIXELS = 100;
	int CROP_X = CROP_NUM_OF_PIXELS/2;
	int CROP_Y = CROP_NUM_OF_PIXELS/2;


	CROP_X = CROP_X / mbinFactor;
	CROP_Y = CROP_Y / mbinFactor;
	
	CImg<unsigned short> map_img(image8k,Width,Height,1,1,true); 

	index = 0;
	unsigned short* pointer2data;

	pointer2data = m_SinkCam1->getData();
	int width = m_SinkCam1->getImageSizeX();
	int height = m_SinkCam1->getImageSizeY();

	int j=0,k=0;

	CImg<unsigned short> map_4k(pointer2data,width,height,1,1,true);

	int cam1width,cam1height;
	int cam2width,cam2height;
	int cam3width,cam3height;
	int cam4width,cam4height;

	cam1width = m_SinkCam1->getImageSizeX();
	cam1height = m_SinkCam1->getImageSizeY();
	cam2width = m_SinkCam2->getImageSizeX();
	cam2height = m_SinkCam2->getImageSizeY();
	cam3width = m_SinkCam3->getImageSizeX();
	cam3height = m_SinkCam3->getImageSizeY();
	cam4width = m_SinkCam4->getImageSizeX();
	cam4height = m_SinkCam4->getImageSizeY();

	//sanity check
	if(cam1width > Width)
	{
		cam1width = cam1width/8;
		cam1height = cam1height/8;
		cam2width = cam2width/8;
		cam2height = cam2height/8;
		cam3width = cam3width/8;
		cam3height = cam3height/8;
		cam4width = cam4width/8;
		cam4height = cam4height/8;
		CROP_X = CROP_X/8;
		CROP_Y = CROP_Y/8;

		for(int i=0 ; i<3;i++)
		{
		offset[i].x_Offset = a[i].x_Offset / 8;
		offset[i].y_Offset = a[i].y_Offset / 8;
		}
	}




	int width1= (cam1width - offset[2].x_Offset) + cam2width - (cam1width - offset[0].x_Offset);
	int width2 = (cam4width - offset[2].x_Offset) + cam3width - (cam4width -offset[1].x_Offset);

	int height1 = cam1height + cam3height - (cam1height - offset[1].y_Offset);
	int height2 = cam1height + cam4height - (cam1height - offset[2].y_Offset);

	int stitched_image_width = 0;
	int stitched_image_height = 0;

	if(width1 > width2)
		stitched_image_width = width2;
	else
		stitched_image_width = width1;

	if(height1 > height2)
		stitched_image_height = height2;
	else
		stitched_image_height = height1;

	int yOffset = ((offset[2].y_Offset - offset[1].y_Offset) - offset[0].y_Offset -1);


	for(int i=0;i<cam1height - CROP_Y;i++)
		for(int j=0;j<cam1width - CROP_X - offset[2].x_Offset;j++)
			map_img(j,i) = map_4k(j+offset[2].x_Offset + CROP_X,i + CROP_Y);

	//map_img.display();

	pointer2data = m_SinkCam3->getData();
	map_4k.assign(pointer2data,cam3width,cam3height,1,1,true);


	for(int i=0; i < cam3height - CROP_Y *2; i++)
		for(int j=0; j < cam3width - CROP_X*2; j++)
			map_img(j + offset[1].x_Offset-offset[2].x_Offset,i + offset[1].y_Offset) = map_4k(j + CROP_X,i + CROP_Y);

	//map_img.display();
	

	pointer2data = m_SinkCam2->getData();
	map_4k.assign(pointer2data,cam2width,cam2height,1,1,true);

	for(int i=0;i<cam2height - abs(offset[0].y_Offset) - CROP_Y*2;i++)
		for(int j=0;j<cam2width - offset[2].x_Offset - CROP_X*2;j++)
			map_img(j + offset[0].x_Offset-offset[2].x_Offset,i) = map_4k(j + CROP_X,i + abs(offset[0].y_Offset) + CROP_Y);


	//map_img.display();

	pointer2data = m_SinkCam4->getData();
	map_4k.assign(pointer2data,cam4width,cam4height,1,1,true);

	for(int i=0;i<cam4height-yOffset - CROP_Y*2;i++)
		for(int j=0;j < cam4width - CROP_X*2; j++)
			map_img(j,i + offset[2].y_Offset) = map_4k(j + CROP_X,i + CROP_Y);


	//map_img.display();
/*
	//Camera1
	for(int s=0; s < height - (YPAD*2);s++,k++)
		for(int t=0, j=0; t < width - (XPAD*2); t++,j++)
		{
			map_img(j,k) = map_4k(s + XPAD ,t + YPAD);
			//map_img(j,k) = pointer2data[t*width + s];
			//image8k[j*ImageSizeX+k] = pointer2data[t*width + s];
		}

	//map_img.display();
	//clear camera 
	m_FSMObject[0]->AllocateShort(50, 1);
	//m_FSMObject[0]->FreeShort();

	pointer2data = m_SinkCam2->getData();
	width = m_SinkCam2->getImageSizeX();
	height = m_SinkCam2->getImageSizeY();
	map_4k.assign(pointer2data,width,height,1,1,true);

	k=ImageSizeY/2;


	
	int xCam2Offset = 143;
	int yCam2Offset = 1;

	xCam2Offset = 0;
	yCam2Offset = 0;
	//CAMERA 2
	for(int s = 0; s < height-(YPAD*2) - yCam2Offset;s++,k++)
		for(int t=0,j=0; t < width-(XPAD*2) - xCam2Offset;t++,j++)
		{
			map_img(j,k) = map_4k(s + XPAD+xCam2Offset,t + YPAD + yCam2Offset);
			//map_img(j,k) = pointer2data[t*width + s];
			//image8k[j*ImageSizeX+k] = pointer2data[t*width + s];
		}

	map_img.display();
	//clear camera memory
	m_FSMObject[1]->AllocateShort(50, 1);
	//m_FSMObject[1]->Free();

	pointer2data = m_SinkCam3->getData();
	

	width = m_SinkCam3->getImageSizeX();
	height = m_SinkCam3->getImageSizeY();
	map_4k.assign(pointer2data,width,height,1,1,true);
	
	//CAMERA 3
	for(int s = 0,k=ImageSizeY/2; s < height - (YPAD*2);s++,k++)
		for(int t=0,j=ImageSizeY/2;t < width - (XPAD*2);t++,j++)
		{
			map_img(j,k) = map_4k(s + XPAD ,t + YPAD);
			//map_img(j,k) = pointer2data[t*width + s];
			//image8k[j*ImageSizeX+k] = pointer2data[t*width + s];
		}

	//clear camera memory
	m_FSMObject[2]->AllocateShort(50, 1);
	//m_FSMObject[2]->Free();

	pointer2data = m_SinkCam4->getData();
	map_4k.assign(pointer2data,width,height,1,1,true);
	width = m_SinkCam4->getImageSizeX();
	height = m_SinkCam4->getImageSizeY();
	for(int s = 0,k=0/2;s<height - (YPAD*2);s++,k++)
		for(int t=0,j=ImageSizeY/2;t<width - (XPAD*2);t++,j++)
		{
			map_img(j,k) = map_4k(s + XPAD ,t + YPAD);
			//map_img(j,k) = pointer2data[t*width + s];
			//image8k[j*ImageSizeX+k] = pointer2data[t*width + s];
		}


	//clear camera memory
	//m_FSMObject[3]->Free();
	m_FSMObject[2]->AllocateShort(50, 1);
	
*/

	/*
	HRESULT hResult=CoCreateInstance(CLSID_CoRemap8K, NULL, CLSCTX_INPROC_SERVER,
		IID_ICoRemap8K, (void **)&RemapInterface);

	CoCreateInstance(CLSID_FSM_MemoryObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IFSM_MemoryObject, (void **)&m_FSMObject8kRemap);
	//allocate data buffer 
	m_FSMObject8kRemap->AllocateShort(Width * Height, 4);	
	//get data buffer address, after calling this function you can use m_pCapturedDataShort[IMAGE_SIZE]
	for( int i=0; i<4; i++){
		m_FSMObject8kRemap->GetDataPtrShort(i, &m_pCapturedDataShort[i]);
		memset(m_pCapturedDataShort[i], NULL, Width * Height);
	
		//Quick point to transfer the data to setup a run
		unsigned short* transfer;
		if(i==0)
			transfer = m_SinkCam1->getData();
		else if(i==1)
			transfer = m_SinkCam2->getData();
		else if(i==2)
			transfer = m_SinkCam3->getData();
		else if(i==3)
			transfer = m_SinkCam4->getData();

		int index=0;
		for(int k=0;k<Height;k++)
			for(int j=0;j<Width;j++)
				m_pCapturedDataShort[i][k*Width+j] = transfer[index++];
	}

	
	RemapInterface->SetImageBuffer(m_FSMObject8kRemap);
	//new update code 1-13-09
	RemapInterface->SetSrcImageSize(Width, Height); 

	//should parse from the file this information

	//set camera parameter from CCD camera param:[cameraNo, XUL, YUL, XUR, YUR, XLL, YLL, XLR,YLR]
	//  These sets of numbers represent the parameters as of 2/1/09 
	RemapInterface->SetSrcImageInformation(1, 7, 20, 3782, 0, 16, 3777, 3784, 3786, 3770, 3770);
	RemapInterface->SetSrcImageInformation(2, 173, 31, 3947, 1, 176, 3799, 3975, 3798, 3770, 3770);
	RemapInterface->SetSrcImageInformation(3, 185, 212, 3971, 203, 193, 4002, 3985, 3988, 3770, 3770);
	RemapInterface->SetSrcImageInformation(4, -4, 197, 3791, 194, 11, 3990, 3794, 3976, 3770, 3770);

	//set remaped image parameters for remap8k.dll
	//ulWidth*ulHeight is temp&max size
	SimpleImageParameters simpleImage;
	simpleImage.ulNumberOfFrame = 1;
	simpleImage.ulNumberOfChannel = 1;
	simpleImage.ulWidth = 5000;
	simpleImage.ulHeight = 5000;
	simpleImage.ulBitDepth = 16;

	for(int i=0; i<4; i++){
		CoCreateInstance(CLSID_SimpleImage, NULL, CLSCTX_INPROC_SERVER,
		IID_ISimpleImage, (void **)&m_SimpleImage[i]);
		//allocate data buffer 
		m_SimpleImage[i]->AllocateSimpleImageBuffer(simpleImage);
		//get data buffer address, after calling this function you can use m_pRemapData[4][simpleImage.ulWidth*simpleImage.ulHeight]
		m_SimpleImage[i]->GetImagePtr(&m_pRemapData[i]);
		//set output image buffer to remap8k.dll
		RemapInterface->SetSimpleImageBuffer(i, m_SimpleImage[i]);
	}

	//remap 4 images
	RemapInterface->RemapAll();
*/
	//Rect rect;

	
	/*
	int *data;

	for(i=0; i	< NUM_OF_CAMERAS; i++){
	//get real remaped size
	//ulLeft&ulTop are always 0 @ this version
	//max ulRight is simpleImage.ulWidth 
	//max ulBottom is simpleImage.ulHeight 
		m_SimpleImage[i]->GetROI(&rect);
		int width = rect.ulRight - rect.ulLeft;
		int height = rect.ulBottom - rect.ulTop;

		data = (int*)m_pRemapData[i];

		fprintf(stderr,"%ld:rect.ulRight %ld rect.ulLeft %ld:rect.ulBottom %ld rect.ulTop\n\n",rect.ulRight,rect.ulLeft,rect.ulBottom,rect.ulTop,i);
		fprintf(stderr,"%ld is width and %ld is height for image %d\n\n",width,height,i);

		//allocate proper memory for each image, since each image can change.

		if(i==0){
			//image1.resize(width,height);
			for(int k=rect.ulTop; k<rect.ulBottom; k++){
				for(int j=rect.ulLeft; j<rect.ulRight; j++){
					//image1.data[k*width+j] = data[k*width+j];
					//write it to the final image as well
					//image8k[k*width+j] = data[k*width+j];
				}
			}
			//image1.display();
		}

		if(i==1)
		{
			//image2.resize(width,height);
			for(int k=rect.ulTop; k<rect.ulBottom; k++){
				for(int j=rect.ulLeft; j<rect.ulRight; j++){
					//image2.data[k*width+j] = data[k*width+j];
					//image8k[k*width+j] = data[k*width+j];
				}
			}
			//image2.display();

		}

		if(i==2)
		{
			//image3.resize(width,height);
			for(int k=rect.ulTop; k<rect.ulBottom; k++){
				for(int j=rect.ulLeft; j<rect.ulRight; j++){
					//image3.data[k*width+j] = data[k*width+j];
					//image8k[k*width+j] = data[k*width+j];
				}
			}
			//image3.display();

		}
		if(i==3)
		{
			//image4.resize(width,height);
			for(int k=rect.ulTop; k<rect.ulBottom; k++){
				for(int j=rect.ulLeft; j<rect.ulRight; j++){
					//image4.data[k*width+j] = data[k*width+j];
					//image8k[k*width+j] = data[k*width+j];
				}
			}
			//image4.display();

		}

	
	}*///end of remapping for loop
   // m_FSMObject8kRemap->FreeShort();
	//m_FSMObject8kRemap->Release();
    //RemapInterface->Release();

	return 1;
}