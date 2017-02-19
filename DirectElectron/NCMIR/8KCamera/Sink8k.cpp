#include "stdafx.h"
#include ".\sink8k.h"

_ATL_FUNC_INFO OnCurrentStatusInfo8k = {CC_STDCALL, VT_EMPTY, 0,0};
_ATL_FUNC_INFO OnImageCapturedInfo8k = {CC_STDCALL, VT_EMPTY, 3, VT_I4,VT_I4,VT_I4};
_ATL_FUNC_INFO OnCaptureAvailableInfo8k = {CC_STDCALL, VT_EMPTY, 0, 0};


#define cimg_use_tiff 
#include "CImg.h"
using namespace cimg_library;

#define IMAGE1_ROTATION -90
CImg<unsigned short> savable1; 


Sink8k::Sink8k(void)
{
}

Sink8k::Sink8k(ICameraCtrl* pcamera,unsigned short * data)
{
	m_pCapturedDataShort = data;
	m_pCamera = pcamera;
	m_pCamera->AddRef();
	DispEventAdvise((IUnknown*)m_pCamera);
}

Sink8k::~Sink8k(void)
{
	if(m_pCamera)
		m_pCamera->Release();
}

void Sink8k::setPathtoGainImages(CString path)
{
	mPathtoGain = path;

	CImg<unsigned short> flatfield1x;
	CImg<unsigned short> darkfield1x;
	CImg<unsigned short> flatfield2x;
	CImg<unsigned short> darkfield2x;
	CImg<unsigned short> flatfield4x;
	CImg<unsigned short> darkfield4x;


	path.Format("%sflatfield_1x_camera1.tif",mPathtoGain);
	flatfield1x.load(path);
	path.Format("%sdarkfield_1x_camera1.tif",mPathtoGain);
	darkfield1x.load(path);
	M_VALUE_CAMERA = flatfield1x.mean();
	
	path.Format("%sflatfield_2x_camera1.tif",mPathtoGain);
	flatfield2x.load(path);
	path.Format("%sdarkfield_2x_camera1.tif",mPathtoGain);
	darkfield2x.load(path);
	M_VALUE_CAMERA_2x = flatfield2x.mean();

	path.Format("%sflatfield_4x_camera1.tif",mPathtoGain);
	flatfield4x.load(path);
	path.Format("%sdarkfield_4x_camera1.tif",mPathtoGain);
	darkfield4x.load(path);
	M_VALUE_CAMERA_4x = flatfield4x.mean();



}
void Sink8k::setImageType(int type){IMAGETYPE = type;}
int  Sink8k::getImageType(){ return IMAGETYPE;}
unsigned short* Sink8k::getData(){return m_pCapturedDataShort;}
void Sink8k::setDataIsNotReady(){Data_Avail = false;}

float Sink8k::getMValue(){return M_VALUE_CAMERA;}
void Sink8k::setMValue(float value){M_VALUE_CAMERA = value;}
float Sink8k::getMValue_2x(){return M_VALUE_CAMERA_2x;}
void Sink8k::setMValue_2x(float value){M_VALUE_CAMERA_2x = value;}
float Sink8k::getMValue_4x(){return M_VALUE_CAMERA_4x;}
void Sink8k::setMValue_4x(float value){M_VALUE_CAMERA_4x = value;}

void Sink8k::factorCalculations()
{
	if(IMAGESIZEX >= 4096 && IMAGESIZEY >= 4096)
	{
		CImg<unsigned short> flatfield1x;
		CImg<unsigned short> darkfield1x;

		flatfield1x.assign(IMAGESIZEX,IMAGESIZEY);
		darkfield1x.assign(IMAGESIZEX,IMAGESIZEY);
		int k = 0;
		for(int i=0;i < IMAGESIZEX;i++)
			for(int j=0;j < IMAGESIZEX;j++)
			{
				flatfield1x(i,j) = (accumulatorFLAT_camera1[k] / 1);
				darkfield1x(i,j) = (accumulatorBIAS_camera1[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera1[k]=0;
				accumulatorBIAS_camera1[k]=0;
				k++;
			}
		M_VALUE_CAMERA = flatfield1x.mean();
		if(accumulatorBIAS_camera1)
			delete accumulatorBIAS_camera1;
		if(accumulatorFLAT_camera1)
			delete accumulatorFLAT_camera1;

		CString path;
		path.Format("%sflatfield_1x_camera1.tif",mPathtoGain);
		flatfield1x.save(path);
		path.Format("%sdarkfield_1x_camera1.tif",mPathtoGain);
		darkfield1x.save(path);
	}

	else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
	{
		CImg<unsigned short> flatfield2x;
		CImg<unsigned short> darkfield2x;
	
		flatfield2x.assign(IMAGESIZEX,IMAGESIZEY);
		darkfield2x.assign(IMAGESIZEX,IMAGESIZEY);
		int k = 0;
		for(int i=0;i < IMAGESIZEX;i++)
			for(int j=0;j < IMAGESIZEX;j++)
			{
				flatfield2x(i,j) = (accumulatorFLAT_camera1[k] / 1);
				darkfield2x(i,j) = (accumulatorBIAS_camera1[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera1[k]=0;
				accumulatorBIAS_camera1[k]=0;
				k++;
			}
		M_VALUE_CAMERA_2x = flatfield2x.mean();
		if(accumulatorBIAS_camera1)
			delete accumulatorBIAS_camera1;
		if(accumulatorFLAT_camera1)
			delete accumulatorFLAT_camera1;
		CString path;
		path.Format("%sflatfield_2x_camera1.tif",mPathtoGain);
		flatfield2x.save(path);
		path.Format("%sdarkfield_2x_camera1.tif",mPathtoGain);
		darkfield2x.save(path);
	}
	else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
	{
	
		CImg<unsigned short> flatfield4x;
		CImg<unsigned short> darkfield4x;
		flatfield4x.assign(IMAGESIZEX,IMAGESIZEY);
		darkfield4x.assign(IMAGESIZEX,IMAGESIZEY);
		int k = 0;
		for(int i=0;i < IMAGESIZEX;i++)
			for(int j=0;j < IMAGESIZEX;j++)
			{
				flatfield4x(i,j) = (accumulatorFLAT_camera1[k] / 1);
				darkfield4x(i,j) = (accumulatorBIAS_camera1[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera1[k]=0;
				accumulatorBIAS_camera1[k]=0;
				k++;
			}
		M_VALUE_CAMERA_4x = flatfield4x.mean();
		if(accumulatorBIAS_camera1)
			delete accumulatorBIAS_camera1;
		if(accumulatorFLAT_camera1)
			delete accumulatorFLAT_camera1;
		CString path;
		path.Format("%sflatfield_4x_camera1.tif",mPathtoGain);
		flatfield4x.save(path);
		path.Format("%sdarkfield_4x_camera1.tif",mPathtoGain);
		darkfield4x.save(path);
	}

}


void Sink8k::saveImage(CString abspath, CString basename, long ccdb_id,int count,CString timeStamp)
{

	
	CString path; 
	path.Format("%s\\CCDBid_%ld_%s_%s_%03d_cam1.tif",abspath,ccdb_id,timeStamp,basename,count);
	savable1.save(path);

}
int Sink8k::getImageSizeX(){return IMAGESIZEX;}
int Sink8k::getImageSizeY(){return IMAGESIZEY;}
bool Sink8k::checkDataReady(){return Data_Avail;}

void __stdcall Sink8k::OnImageCaptured8k(long imagesizex,long imagesizey,unsigned short * imagedata)
{
	CString imgstr;
	IMAGESIZEX = imagesizex;
	IMAGESIZEY = imagesizey;
	int totalsize = sizeof(imagedata)/sizeof(imagedata[0]);
	imgstr.Format("camera  x: %d y: %d  totalsize %d ",imagesizex,imagesizey,totalsize);
	//AfxMessageBox(imgstr);

	if(IMAGETYPE==FLATFIELD_8k)
	{
		//if(accumulatorFLAT_camera1==NULL)
		accumulatorFLAT_camera1 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorFLAT_camera1[i]=0;}
		for(int i=0; i < IMAGESIZEX * IMAGESIZEY;i++)
		{		
			accumulatorFLAT_camera1[i] = accumulatorFLAT_camera1[i] + m_pCapturedDataShort[i];	
		}
		Data_Avail = true;
		return;
	}
	//dark field case
	else if(IMAGETYPE==DARK_8k)
	{
		//should be the size of the image at either 1x binning or 2x binning
		accumulatorBIAS_camera1 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorBIAS_camera1[i]=0;}
		for(int i=0; i< IMAGESIZEX * IMAGESIZEY;i++)
		{
			accumulatorBIAS_camera1[i] += m_pCapturedDataShort[i];
		}
		Data_Avail = true;
		return;
	
	}
	else if(IMAGETYPE==CORRECTED_8k)
	{
		
		CImg<unsigned short> flatfield1x;
		CImg<unsigned short> darkfield1x;
		CImg<unsigned short> flatfield2x;
		CImg<unsigned short> darkfield2x;
		CImg<unsigned short> flatfield4x;
		CImg<unsigned short> darkfield4x;
		CString path;

		if(IMAGESIZEX >= 4096 && IMAGESIZEY >= 4096)
		{
			
			
			path.Format("%sflatfield_1x_camera1.tif",mPathtoGain);
			flatfield1x.load(path);
			path.Format("%sdarkfield_1x_camera1.tif",mPathtoGain);
			darkfield1x.load(path);
		}

		else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
		{
			path.Format("%sflatfield_2x_camera1.tif",mPathtoGain);
			flatfield2x.load(path);
			path.Format("%sdarkfield_2x_camera1.tif",mPathtoGain);
			darkfield2x.load(path);
		}

		else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
		{
			path.Format("%sflatfield_4x_camera1.tif",mPathtoGain);
			flatfield4x.load(path);
			path.Format("%sdarkfield_4x_camera1.tif",mPathtoGain);
			darkfield4x.load(path);
		}

	
		int i=0;
			for(int j=0;j<IMAGESIZEX;j++)
				for(int k=0;k<IMAGESIZEX;k++)
				{
					//WORKING
					unsigned short correctedvalue;		
					//avg of everyone
					float factor1, factor2;
					
					if(IMAGESIZEX >= 4096 && IMAGESIZEY >= 4096)
					{
						factor1 = (float)(m_pCapturedDataShort[i] - darkfield1x(j,k))* (float)M_VALUE_CAMERA;
						factor2 =(float)((float)factor1/((float)(flatfield1x(j,k) - darkfield1x(j,k))));		
					}
					else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
					{
						//AfxMessageBox("correct");
						factor1 = (float)(m_pCapturedDataShort[i] - darkfield2x(j,k))* (float)M_VALUE_CAMERA_2x;
						factor2 =(float)((float)factor1/((float)(flatfield2x(j,k) - darkfield2x(j,k))));		
					}
					else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
					{
						//AfxMessageBox("correct");
						factor1 = (float)(m_pCapturedDataShort[i] - darkfield4x(j,k))* (float)M_VALUE_CAMERA_4x;
						factor2 =(float)((float)factor1/((float)(flatfield4x(j,k) - darkfield4x(j,k))));		
					}

					
					m_pCapturedDataShort[i] = (unsigned short) factor2;
					i = i + 1;
				}
				CImg<unsigned short> img(m_pCapturedDataShort,IMAGESIZEX,IMAGESIZEY,1,1,true); 
				img.rotate(IMAGE1_ROTATION);
				
				savable1.assign(m_pCapturedDataShort,IMAGESIZEX,IMAGESIZEY,1,1,true);
				Data_Avail = true;
				return;
	}
	else if(IMAGETYPE==UNPROCESSED_8k){
		//quickly rotate it by -90 degrees
		CImg<unsigned short> img(m_pCapturedDataShort,IMAGESIZEX,IMAGESIZEY,1,1,true); 
		img.rotate(IMAGE1_ROTATION);
		int index = 0;
		for(int i=0; i < IMAGESIZEY; i++)
			for(int j=0; j < IMAGESIZEX; j++)
			{
				m_pCapturedDataShort[index++] = img(j,i);
			}
				//m_pCapturedDataShort[j*IMAGESIZEX + i] = img(j,i);
		savable1.assign(m_pCapturedDataShort,IMAGESIZEX,IMAGESIZEY,1,1,true);
		Data_Avail = true;
		return;
	}
	return;
}

void Sink8k::setCropPad(int xPad,int yPad)
{
	XPAD = xPad;
	YPAD = yPad;

}

void __stdcall Sink8k::OnCaptureAvailable8k(){}
void __stdcall Sink8k::OnCurrentStatus8k(){}
