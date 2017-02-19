#include "stdafx.h"

#ifdef NCMIR_CAMERA

#include <fstream>
#include <string>
#include <stdio.h>
#include <list>
using namespace std;
#include ".\ncmir4kcamera.h"
#define IMAGEMAGICKPATH "C:\\ImageMagick-6.6.2-0\\"
//#define cimg_use_magick
//#define cimg_imagemagick_path IMAGEMAGICKPATH
#include <Magick++.h>
using namespace Magick;




static CImg<unsigned short> map_img;

NCMIR4kCamera::NCMIR4kCamera(void)
{
}

NCMIR4kCamera::~NCMIR4kCamera(void)
{
	uninitialize();
}

int NCMIR4kCamera::uninitialize()
{
	//4k camera instance
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{

		if(m_FSMObject)
		{
			m_FSMObject->FreeShort();
			//Release Memory object
			m_FSMObject->Release();
		}
	 
		if(m_CaptureInterface)
			m_CaptureInterface->Release();
		if(sink)
		{
			delete sink;
		}
		sink = NULL;
		m_FSMObject=NULL;
		m_CaptureInterface=NULL;
	// CoUninitialize();
	}
	return 1;

}

//tells the difference between the 4k camera vs the 8k camera
void NCMIR4kCamera::setCamType(int TYPE)
{

	//helps to determine whether we are 
	m_CamType = TYPE;


}

int NCMIR4kCamera::initialize()
{

	//4K camera init instance.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{

		HRESULT hr;
		hr=	CoCreateInstance(CLSID_CoCameraCtrl, NULL, CLSCTX_INPROC_SERVER,
				IID_ICameraCtrl, (void **)&m_CaptureInterface);
		if(!SUCCEEDED(hr)) return -1;
		hr =CoCreateInstance(CLSID_FSM_MemoryObject, NULL, CLSCTX_INPROC_SERVER,
				IID_IFSM_MemoryObject, (void **)&m_FSMObject);
		if(!SUCCEEDED(hr)) return -1;
		
		//read in a properties file to know how to assign
		//Read in the information as this is very critical 
		int check;
		check = readCameraProperties();


		if(check==-1)
			return -1; //couldnt read the properties so return.
		
		//At this point it would be good to check the pressure and temperature of the
		//camera before proceding
		readCameraParams();
		CString openingMessage("");
		CString temp;
		temp.Format("Camera has a Pressure: %3.2f  and Temperature:  %3.2f C\n",camera_Pressure,camera_Temperature);
		openingMessage += temp;

		AfxMessageBox(openingMessage);

		m_CaptureInterface->SetDCFPath(0,0,dcf_file);
		m_FSMObject->AllocateShort(MAX_IMAGE_SIZE, 1);
		m_FSMObject->GetDataPtrShort(0, &m_pCapturedDataShort);
		m_CaptureInterface->SetImageBuffer((IFSM_MemoryObject*)m_FSMObject);
	  //(IFSM_MemoryObject*)

		sink = new CNCMIR4kSink(m_CaptureInterface,m_pCapturedDataShort);
		
		
		m_CaptureInterface->SetParamTablePath(filename,tablefilename);
		m_CaptureInterface->SetConfigTablePath(filename,tablefilename);
		m_CaptureInterface->ReadoutParam_RecAllFromHost();
		
		//Lets readout from the camera....
		m_CaptureInterface->Status_TransToHost();
		m_CaptureInterface->ConfigParam_TransToHost();
		m_CaptureInterface->ReadoutParam_TransAllToHost();

		//Turn on Window Heater, it really should always be on
		m_CaptureInterface->put_ConfigParam(WINDOW_HEATER,1);
		m_CaptureInterface->ConfigParam_RecAllFromHost();
		//Just initially disply the Configuration parameters
		long param;
		m_CaptureInterface->get_ConfigParam(INSRUMENT_MODEL,&param);
		instrument_model = param;
		m_CaptureInterface->get_ConfigParam(SERIAL_HEAD,&param);
		head_serial = param;
		m_CaptureInterface->get_ConfigParam(SHUTTER_CLOSE_DELAY,&param);
		shutter_close_delay = param;
		m_CaptureInterface->get_ConfigParam(PRE_EXPOSE_DELAY,&param);
		pre_expose_delay = param;
		m_CaptureInterface->get_ConfigParam(CCD_TEMP_SETPOINT,&param);
		ccd_temp_setPoint = (float)((param/10) - 273.15);
		m_CaptureInterface->get_ConfigParam(WINDOW_HEATER,&param);
		window_heater = param;

	}//end of 4K camera init instance.
		

		mURLPath="";

		return 1;
}


void NCMIR4kCamera::setApachePath(CString ApachePath,CString ApacheURL)
{
	mApachePath = ApachePath;
	mApacheURL = ApacheURL;
}


int NCMIR4kCamera::readCameraProperties()
{
	//4K camera init instance.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{
	    string  camera_properties[NUM_OF_LINES];
		ifstream inFile;
		inFile.open(FILE_PROPERTIES);

		if (!inFile) {
			 AfxMessageBox("Could not read NCMIR cam properties file, ensure NCMIRCamProperties.txt exists!", MB_OK | MB_ICONEXCLAMATION);
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

		//should read in :  C:\\camerafiles\\SI1100.dcf
		dcf_file = SysAllocString(A2BSTR(camera_properties[0].c_str()));
		//should read in :  C:\\camerafiles\\1100-106 250kHz.set
		filename = SysAllocString(A2BSTR(camera_properties[1].c_str()));
		//should read in a C:\\camerafiles\\x4839A+2.str
		tablefilename = SysAllocString(A2BSTR(camera_properties[2].c_str()));
		//should read in the port number, typically 3
		camera_port_number = atoi(camera_properties[3].c_str() );
		m_CaptureInterface->put_COMPort(camera_port_number);
		//m_CaptureInterface->put_COMPort(3);
		
		//Determine if the TEC should automatically turn on upon init
		//IF "ON" is not found then will proceed to turn off the TEC cooler.

		if(camera_properties[4]=="ON")
			m_CaptureInterface->ExternalRelayTurnOn();

		else
			m_CaptureInterface->ExternalRelayTurnOff();


		
		m_CaptureInterface->SetOrigin(16,0);

	}//end of instance 4k
		return 1;



}


float NCMIR4kCamera::getCameraTemp()
{


	//return camera_Temperature;
	/*
	long param;

	CSingleLock slock(&m_mutex);
	
	if(slock.Lock(1000))
	{
		//4K camera init instance.
		if(m_CamType == NCMIR_4k_LENS_COUPLED)
		{

			m_CaptureInterface->get_StatusParam(CAMERA_TEMP,&param);
			camera_Temperature = (float)((param/10) - 273.15);
		}

	}*/
	return camera_Temperature;

}




float NCMIR4kCamera::getCameraBackplateTemp()
{
	long param;
	CSingleLock slock(&m_mutex);
	
	if(slock.Lock(1000))
	{
		//4K camera init instance.
		if(m_CamType == NCMIR_4k_LENS_COUPLED)
		{

			m_CaptureInterface->get_StatusParam(BACK_PLATE_TEMP,&param);
		}
	}

	return camera_BackPlateTemp;

}
	

int NCMIR4kCamera::getCameraPressure()
{

	
	return camera_Pressure;
}




void NCMIR4kCamera::readCameraParams()
{
	long param;

	CSingleLock slock(&m_mutex);
	
	if(slock.Lock(1000))
	{
		//4K camera init instance.
		if(m_CamType == NCMIR_4k_LENS_COUPLED)
		{
			//readout from the camera first...
			m_CaptureInterface->Status_TransToHost();
			m_CaptureInterface->ConfigParam_TransToHost();

			m_CaptureInterface->get_StatusParam(CAMERA_TEMP,&param);
			camera_Temperature = (float)((param/10) - 273.15);
			m_CaptureInterface->get_StatusParam(BACK_PLATE_TEMP,&param);
			camera_BackPlateTemp = (float)((param/10) - 273.15);
			m_CaptureInterface->get_StatusParam(CAMERA_PRESSURE,&param);
			camera_Pressure = (float)param;
			m_CaptureInterface->get_StatusParam(CAMERA_VOLTAGE,&param);
			camera_Voltage = param;
			m_CaptureInterface->get_ConfigParam(WINDOW_HEATER,&param);
			window_heater = param;
			m_CaptureInterface->get_ReadoutParam(CAMERA_DSI_TIME,&param);
			dsi_time = param;

			
		}
	}

	return;
	//cameraLock.Unlock();

}

int NCMIR4kCamera::copyImageData(unsigned short *image4k, int ImageSizeX, int ImageSizeY)
{

	if(STOPPING_ACQUSITION==true)
	{
		for(int i=0; i < ImageSizeY; i++)
			for(int j=0; j < ImageSizeX; j++)
				image4k[j*ImageSizeX + i] = 0;

		STOPPING_ACQUSITION = false;
		return 1;
	}


	//4K camera init instance.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{

		//map the image for serialEM to map_image
		map_img.assign(image4k,ImageSizeX,ImageSizeY,1,1,true);


		//pull and copy the data so SerialEM can read it in.
		CImg<unsigned short> map_4k(sink->getImageSizeX(),sink->getImageSizeY(),1,1);
		//map the image for serialEM to map_image
		//map_4k.assign(image4k,ImageSizeX,ImageSizeY,1,1,true);

		//Here is an important check. We need to make sure 

		unsigned short* data = sink->getData();
		for(int y=0;y<sink->getImageSizeY();y++)
			for(int x=0; x < sink->getImageSizeX(); x++)
				//map_4k(sink->getImageSizeY() - y,x) = data[x*sink->getImageSizeX() + y];
				map_4k(x,y) = data[x*sink->getImageSizeX() + y];

		//try to see for quicker rotate by 90
		//map_4k(sink->getImageSizeX() - y,x) = data[x*sink->getImageSizeX() + y];

		map_4k.rotate(90);

		//map_4k.display();

		

		for(int i=0; i < ImageSizeY; i++)
			for(int j=0; j < ImageSizeX; j++)
				image4k[j*ImageSizeX + i] = map_4k(j,i);



		/*int z = ImageSizeY * ImageSizeX;
		
			unsigned short* data = sink->getData();
			for(int i=0; i < ImageSizeY; i++)
				for(int j=0; j < ImageSizeX; j++)
					//image4k[j*ImageSizeX + i] = data[j*sink->getImageSizeX() + i];
					image4k[z--] = data[j*sink->getImageSizeX() + i];
		*/

		ACQUIRING_DATA = false;
	

		//cameraLock.Unlock();
	}//end of 4k instance

	return 1;
}


int NCMIR4kCamera::setBinning(int x, int y, int sizeX, int sizeY)
{

	//4K camera init instance.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{

		CSingleLock slock(&m_mutex);
	
		if(slock.Lock(1000))
		{
			m_CaptureInterface->SetImageSize((sizeX/2),(sizeY/2));
			m_CaptureInterface->SetBinning(x,y);
			binFactor = x;

			//to support various sizes.
			//YUP magic numbers are bad, but in this case it makes sense
			//to see the numbers off the bat.  
			int OriginX = (2048 + 16) - ((sizeX * x) / 2);
			int OriginY = (2048 - ((sizeY *y )/ 2));

			m_CaptureInterface->SetOrigin( OriginX,OriginX-16);
			m_FSMObject->AllocateShort((sizeX * sizeY),1);
		}
		
	}

	//Just for debugging purposes
	//CString info;
	//info.Format("The  size being setup is x:  %d  y:  %d  binning: %d with arrSize %ld", td->DMSizeX,td->DMSizeY,td->Binning,arrSize);
	//AfxMessageBox(info);
	//info.Format("The  actual size being setup is x:  %d  y:  %d", actualX, actualY);
	//AfxMessageBox(info);


	return 1;

}

int NCMIR4kCamera::setImageSize(int sizeX, int sizeY)
{

	//4K camera init instance.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{
		CSingleLock slock(&m_mutex);
	
		if(slock.Lock(1000))
		{
			m_CaptureInterface->SetImageSize(sizeX/2, sizeY/2);
		}
	}
	return 1;
}

void NCMIR4kCamera::setGainSetting(short index)
{
	//4K camera init instance.
	//NOTE: below the determination of the DSI time and gain
	//attenuation was from the Camera Report manual.
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{
		CSingleLock slock(&m_mutex);
	
		if(slock.Lock(1000))
		{
			int dsi_time_setting;

			m_CaptureInterface->put_ReadoutParam(ATTENUATION_PARAM, 0);
			if(index==0) 
			{
				dsi_time_setting = LOW_DSI_TIME;
				m_CaptureInterface->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}

			else if(index==1) 
				
			{
				dsi_time_setting = MED_DSI_TIME;
				m_CaptureInterface->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}
			else if(index==2) 
			{
				dsi_time_setting = HIGH_DSI_TIME;
				m_CaptureInterface->put_ReadoutParam(CAMERA_DSI_TIME, dsi_time_setting);
			}
			
			//update the Camera with the latest readout values

			m_CaptureInterface->ReadoutParam_RecAllFromHost();
		}
	}	
}


int NCMIR4kCamera::AcquireImage(float seconds)
{
	

	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{
		if(m_CamType == NCMIR_4k_LENS_COUPLED)
		{

			sink->setDataIsNotReady();
			STOPPING_ACQUSITION = false;
			m_CaptureInterface->put_ExposureTime((long)((float)(seconds * 1000)));


			shutter->ShutterMode(2);
			shutter->SendCommand(22);

			m_CaptureInterface->CCD_ReadAndExpose();



			if(shutter) 
			{
			
				
				float time2sleep = (seconds*1000 - float(1000/seconds));
				if(time2sleep <= 0)
					::Sleep(seconds*1000);
				else
					::Sleep(time2sleep);
				shutter->ShutterMode(1);
				shutter->SendCommand(22);
			}




			while(!isImageAcquistionDone()) //wait until the image is finish acquiring
				;
		}
	}
	return 1;
}



int NCMIR4kCamera::AcquireDarkImage(float seconds)
{

	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{
		if(m_CamType == NCMIR_4k_LENS_COUPLED)
		{

			ACQUIRING_DATA = true;
			sink->setDataIsNotReady();
			STOPPING_ACQUSITION = false;
			m_CaptureInterface->put_ExposureTime((long)((float)seconds *1000));
			m_CaptureInterface->CCD_ReadNoExpose();

			while(!isImageAcquistionDone()) //wait until the image is finish acquiring
				;
		}
	}
	
	return 1;

}

void NCMIR4kCamera::StopAcquistion()
{
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
	{

		m_CaptureInterface->AbortReadout();
		m_CaptureInterface->ClearFrame();
		STOPPING_ACQUSITION = true;
	}
	//cameraLock.Unlock();
}

bool NCMIR4kCamera::isImageAcquistionDone()
{
	if(STOPPING_ACQUSITION==true)
	{	
		return true;

	}
	else
	{
		return sink->checkDataReady();
	}
}

int NCMIR4kCamera::setOrigin(int x, int y)
{
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
		m_CaptureInterface->SetOrigin(x,y);
	return 1;
}



int NCMIR4kCamera::reAllocateMemory(int mem)
{
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
		m_FSMObject->AllocateShort(mem,1);
	return 1;
}

unsigned short* NCMIR4kCamera::getImageData()
{
	if(m_CamType == NCMIR_4k_LENS_COUPLED)
		return sink->getData();
}

void NCMIR4kCamera::windowHeaterOff()
{
	//Turn on Window Heater, it really should always be on
	
	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{
		m_CaptureInterface->ConfigParam_TransToHost();
		m_CaptureInterface->put_ConfigParam(WINDOW_HEATER,0);
		m_CaptureInterface->ConfigParam_RecAllFromHost();
	}
}

void NCMIR4kCamera::windowHeaterOn()
{
	//Turn on Window Heater, it really should always be on

	CSingleLock slock(&m_mutex);

	if(slock.Lock(1000))
	{
		m_CaptureInterface->ConfigParam_TransToHost();
		m_CaptureInterface->put_ConfigParam(WINDOW_HEATER,1);
		m_CaptureInterface->ConfigParam_RecAllFromHost();
	}
}


void NCMIR4kCamera::setMetaDataThumbNail(double x,double y,double z,double defocus,CString magIndex,double TiltAngle)
{
	mCurrx = x;
	mCurry = y;
	mCurrz = z;
	mCurrdefocus = defocus;
	mCurrMagindex = magIndex;
	mCurrTiltAngle = TiltAngle;
}

void NCMIR4kCamera::setTiltSeriesInfo(BOOL isTiltSeries)
{
	misTiltSeries = isTiltSeries;

}
	

void NCMIR4kCamera::setMontageInfo(BOOL isMontage, int xNframes, int yNframes)
{
	misMontage = isMontage;
	mXNframes = xNframes;
	mYNframes = yNframes;

}


void NCMIR4kCamera::SaveThumNailImage(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle, unsigned short* image,int sizeX,int sizeY, double meanCounts)
{

	//Build up the path to save the data
	

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

	char thumbnailpath[180];
	sprintf(thumbnailpath,"%s\\CCDBid_%ld_%s_%03d.jpg",apachePath,ccdbID,basename,sectionNumber);

	CImg<unsigned short> image2save;
	image2save.assign(image,sizeX,sizeY);


	if(misMontage)
	{
		char montage_thumbnailpath[180];
		sprintf(montage_thumbnailpath,"%s\\CCDBid_%ld_%s_%03d_montage.jpg",apachePath,ccdbID,basename,sectionNumber);
		CImg<unsigned short> montage_normalized_thumbnail(128,128);
		montage_normalized_thumbnail.assign(image2save.get_resize(128,128));
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
	char thumbnailMessage[200];
	sprintf(thumbnailMessage,"Name: %s CCDB ID: %ld\nSec: %d Tilt: %0.2f%c Mean: %0.2f \nTime: %02d\:%02d\:%02d Temp: %0.2f%c C Pres: %ld",basename,ccdbID,sectionNumber,TiltAngle,0x00B0,meanCounts,ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond(),this->getCameraTemp(),0x00B0,this->getCameraPressure());
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

	fout<<"<h1>"<<"JEOL 4000#1 using 4k x 4k CCD Camera</h1><br/>"<<endl;
	fout<<"<h1>"<<"CCDB Microscopy Product ID: "<<ccdbID<<" BaseName:  "<<basename<<"</h1><br/>";
	fout<<"<h2>"<<ctdt.GetMonth()<<"/"<<ctdt.GetDay()<<"/"<< ctdt.GetYear()<<"</h2><br/>"<<endl;
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


	CString heater;
	if(this->getWindowHeaterStatus()==1)
		heater.Format("ON");
	else
		heater.Format("OFF");
		
	fout<<"<h3>"<<"Camera Model: "<<this->getInstrumentModel()<<" Serial Number: "<<this->getSerialNumber()<<" Window Heater: "<<heater<<"</h3><br/>";
	fout<<"<h3>"<<"Camera Temp(C):  "<<this->getCameraTemp()<<"          Camera Pressure: "<<this->getCameraPressure()<<"</h3><br/>"<<endl;
	if((this->getCameraTemp() < -50) || (this->getCameraPressure() < 10000))
		fout<<"<h3> Camera is working properly</h3><br/>"<<endl;
	else
		fout<<"<h3>PROBLEM WITH CAMERA!!!!!!!!</h3><br/>"<<endl;
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
	
}


int NCMIR4kCamera::setDebugMode()
{
	debug_mode = 1;
	return 1;
}

long NCMIR4kCamera::getCamVoltage()
{
	return camera_Voltage;
}

long NCMIR4kCamera::getInstrumentModel()
{
	return instrument_model;
}

long NCMIR4kCamera::getSerialNumber()
{
	return head_serial;
}

long NCMIR4kCamera::getShutterDelay()
{
	return shutter_close_delay;
}

long NCMIR4kCamera::getPreExposureDelay()
{
	return pre_expose_delay;
}
    
float NCMIR4kCamera::getCCDSetPoint()
{
	return ccd_temp_setPoint;
}
 
long NCMIR4kCamera::getWindowHeaterStatus()
{
	return window_heater;
}
long NCMIR4kCamera::getDSITime()
{
	return dsi_time;
}

#endif