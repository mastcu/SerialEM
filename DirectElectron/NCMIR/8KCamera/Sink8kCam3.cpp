#include "stdafx.h"
#include ".\sink8kcam3.h"

_ATL_FUNC_INFO OnCurrentStatusInfo8kCam3 = {CC_STDCALL, VT_EMPTY, 0,0};
_ATL_FUNC_INFO OnImageCapturedInfo8kCam3 = {CC_STDCALL, VT_EMPTY, 3, VT_I4,VT_I4,VT_I4};
_ATL_FUNC_INFO OnCaptureAvailableInfo8kCam3 = {CC_STDCALL, VT_EMPTY, 0, 0};

#define IMAGE3_ROTATION 90

Sink8kCam3::Sink8kCam3(void)
{
}

Sink8kCam3::Sink8kCam3(ICameraCtrl* pcamera,unsigned short * data)
{
	m_pCapturedDataShortCam3 = data;
	m_pCameraCam3 = pcamera;
	m_pCameraCam3->AddRef();
	DispEventAdvise((IUnknown*)m_pCameraCam3);
}

Sink8kCam3::~Sink8kCam3(void)
{
	if(m_pCameraCam3)
		m_pCameraCam3->Release();

}


void Sink8kCam3::setPathtoGainImages(CString path)
{
	mPathtoGain = path;
	CImg<unsigned short> flatfield1x;
	CImg<unsigned short> darkfield1x;
	CImg<unsigned short> flatfield2x;
	CImg<unsigned short> darkfield2x;
	CImg<unsigned short> flatfield4x;
	CImg<unsigned short> darkfield4x;
	
	path.Format("%sflatfield_1x_camera3.tif",mPathtoGain);
	flatfield1x.load(path);
	path.Format("%sdarkfield_1x_camera3.tif",mPathtoGain);
	darkfield1x.load(path);
	M_VALUE_CAMERA = flatfield1x.mean();

	path.Format("%sflatfield_2x_camera3.tif",mPathtoGain);
	flatfield2x.load(path);
	path.Format("%sdarkfield_2x_camera3.tif",mPathtoGain);
	darkfield2x.load(path);
	M_VALUE_CAMERA_2x = flatfield2x.mean();

	path.Format("%sflatfield_4x_camera3.tif",mPathtoGain);
	flatfield4x.load(path);
	path.Format("%sdarkfield_4x_camera3.tif",mPathtoGain);
	darkfield4x.load(path);
	M_VALUE_CAMERA_4x = flatfield4x.mean();

}
void Sink8kCam3::setImageType(int type){IMAGETYPE = type;}
int  Sink8kCam3::getImageType(){ return IMAGETYPE;}
float Sink8kCam3::getMValue(){return M_VALUE_CAMERA;}
void Sink8kCam3::setMValue(float value){M_VALUE_CAMERA = value;}
float Sink8kCam3::getMValue_2x(){return M_VALUE_CAMERA_2x;}
void Sink8kCam3::setMValue_2x(float value){M_VALUE_CAMERA_2x = value;}
float Sink8kCam3::getMValue_4x(){return M_VALUE_CAMERA_4x;}
void Sink8kCam3::setMValue_4x(float value){M_VALUE_CAMERA_4x = value;}

void Sink8kCam3::factorCalculations()
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
				flatfield1x(i,j) = (accumulatorFLAT_camera3[k] / 1);
				darkfield1x(i,j) = (accumulatorBIAS_camera3[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera3[k]=0;
				accumulatorBIAS_camera3[k]=0;
				k++;
			}
		M_VALUE_CAMERA = flatfield1x.mean();
		if(accumulatorBIAS_camera3)
			delete accumulatorBIAS_camera3;
		if(accumulatorFLAT_camera3)
			delete accumulatorFLAT_camera3;

		CString path;
		path.Format("%sflatfield_1x_camera3.tif",mPathtoGain);
		flatfield1x.save(path);
		path.Format("%sdarkfield_1x_camera3.tif",mPathtoGain);
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
				flatfield2x(i,j) = (accumulatorFLAT_camera3[k] / 1);
				darkfield2x(i,j) = (accumulatorBIAS_camera3[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera3[k]=0;
				accumulatorBIAS_camera3[k]=0;
				k++;
			}
		M_VALUE_CAMERA_2x = flatfield2x.mean();
		if(accumulatorBIAS_camera3)
			delete accumulatorBIAS_camera3;
		if(accumulatorFLAT_camera3)
			delete accumulatorFLAT_camera3;
		CString path;
		path.Format("%sflatfield_2x_camera3.tif",mPathtoGain);
		flatfield2x.save(path);
		path.Format("%sdarkfield_2x_camera3.tif",mPathtoGain);
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
				flatfield4x(i,j) = (accumulatorFLAT_camera3[k] / 1);
				darkfield4x(i,j) = (accumulatorBIAS_camera3[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera3[k]=0;
				accumulatorBIAS_camera3[k]=0;
				k++;
			}
		M_VALUE_CAMERA_4x = flatfield4x.mean();
		if(accumulatorBIAS_camera3)
			delete accumulatorBIAS_camera3;
		if(accumulatorFLAT_camera3)
			delete accumulatorFLAT_camera3;
		CString path;
		path.Format("%sflatfield_4x_camera3.tif",mPathtoGain);
		flatfield4x.save(path);
		path.Format("%sdarkfield_4x_camera3.tif",mPathtoGain);
		darkfield4x.save(path);
	}
		

}


void Sink8kCam3::saveImage(CString abspath, CString basename,long ccdb_id,int count,CString timeStamp)
{
	
	CString savepath;
	CImg<unsigned short> img(m_pCapturedDataShortCam3,IMAGESIZEX,IMAGESIZEY,1,1,true); 
	savepath.Format("%s\\CCDBid_%ld_%s_%s_%03d_cam3.tif",abspath,ccdb_id,timeStamp,basename,count);
	img.save(savepath);

}

void __stdcall Sink8kCam3::OnImageCaptured8kCam3(long imagesizex,long imagesizey,unsigned short * imagedata)
{
	CString imgstr;

	IMAGESIZEX = imagesizex;
	IMAGESIZEY = imagesizey;
	int totalsize = sizeof(imagedata)/sizeof(imagedata[0]);
	imgstr.Format("camera  x: %d y: %d  totalsize %d",imagesizex,imagesizey,totalsize);
	
	if(IMAGETYPE==FLATFIELD_8k)
	{
		
		accumulatorFLAT_camera3 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorFLAT_camera3[i]=0;}	
		for(int i=0; i < IMAGESIZEX * IMAGESIZEY;i++)
		{		
			accumulatorFLAT_camera3[i] = accumulatorFLAT_camera3[i] + m_pCapturedDataShortCam3[i];	
		}
		Data_Avail = true;
		return;
	}
	//dark field case
	else if(IMAGETYPE==DARK_8k)
	{
		accumulatorBIAS_camera3 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorBIAS_camera3[i]=0;}	
		for(int i=0; i< IMAGESIZEX * IMAGESIZEY;i++)
		{
			accumulatorBIAS_camera3[i] += m_pCapturedDataShortCam3[i];
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
			
			
			path.Format("%sflatfield_1x_camera3.tif",mPathtoGain);
			flatfield1x.load(path);
			path.Format("%sdarkfield_1x_camera3.tif",mPathtoGain);
			darkfield1x.load(path);
		}

		else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
		{
			path.Format("%sflatfield_2x_camera3.tif",mPathtoGain);
			flatfield2x.load(path);
			path.Format("%sdarkfield_2x_camera3.tif",mPathtoGain);
			darkfield2x.load(path);
		}

		else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
		{
			path.Format("%sflatfield_4x_camera3.tif",mPathtoGain);
			flatfield4x.load(path);
			path.Format("%sdarkfield_4x_camera3.tif",mPathtoGain);
			darkfield4x.load(path);
		}

		CImg<unsigned short> img(m_pCapturedDataShortCam3,IMAGESIZEX,IMAGESIZEY,1,1,true); 
		int i=0;
			for(int j=0;j<IMAGESIZEX;j++)
				for(int k=0;k<IMAGESIZEX;k++)
				{
					//WORKING
					unsigned short correctedvalue;		
					//avg of everyone
					float factor1,factor2;

					if(IMAGESIZEX >= 4096 && IMAGESIZEY >= 4096)
					{
						factor1 = (float)(m_pCapturedDataShortCam3[i] - darkfield1x(j,k))* (float)M_VALUE_CAMERA;
						factor2 =(float)((float)factor1/((float)(flatfield1x(j,k) - darkfield1x(j,k))));		
					}

					else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
					{
						factor1 = (float)(m_pCapturedDataShortCam3[i] - darkfield2x(j,k))* (float)M_VALUE_CAMERA_2x;
						factor2 =(float)((float)factor1/((float)(flatfield2x(j,k) - darkfield2x(j,k))));		
					}
					else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
					{
						factor1 = (float)(m_pCapturedDataShortCam3[i] - darkfield4x(j,k))* (float)M_VALUE_CAMERA_4x;
						factor2 =(float)((float)factor1/((float)(flatfield4x(j,k) - darkfield4x(j,k))));		
					}

					/*if(factor2 <0)
						factor2=0;
					else if(factor2 >65000)
						factor2=65000;*/
					//img(j,k) = (unsigned short)factor2;
					m_pCapturedDataShortCam3[i] = (unsigned short)factor2;
					
					i = i+1;
				}

		img.rotate(IMAGE3_ROTATION);
		//m_pCapturedDataShortCam3 = img.data;
		Data_Avail = true;
		return;

	}
	else if(IMAGETYPE==UNPROCESSED_8k){
		CImg<unsigned short> img(m_pCapturedDataShortCam3,IMAGESIZEX,IMAGESIZEY,1,1,true); 
		img.rotate(IMAGE3_ROTATION);
		int index = 0;
		for(int i=0; i < IMAGESIZEY; i++)
			for(int j=0; j < IMAGESIZEX; j++)
			{
				/*if(img(j,i) <0)
					img(j,i)=0;
				else if(img(j,i) >65000)
					img(j,i)=65000;*/
				m_pCapturedDataShortCam3[index++] = img(j,i);
		
			}
		//savable1.assign(m_pCapturedDataShort,IMAGESIZEX,IMAGESIZEY,1,1,true);
		Data_Avail = true;
	}
	
	return;
}

unsigned short* Sink8kCam3::getData(){return m_pCapturedDataShortCam3;}
void Sink8kCam3::setDataIsNotReady(){Data_Avail = false;}
bool Sink8kCam3::checkDataReady(){return Data_Avail;}
int Sink8kCam3::getImageSizeX(){return IMAGESIZEX;}
int Sink8kCam3::getImageSizeY(){return IMAGESIZEY;}
void __stdcall Sink8kCam3::OnCaptureAvailable8kCam3(){	}
void __stdcall Sink8kCam3::OnCurrentStatus8kCam3(){	}
