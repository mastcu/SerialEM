#include "stdafx.h"
#include ".\sink8kcam2.h"



_ATL_FUNC_INFO OnCurrentStatusInfo8kCam2 = {CC_STDCALL, VT_EMPTY, 0,0};
_ATL_FUNC_INFO OnImageCapturedInfo8kCam2 = {CC_STDCALL, VT_EMPTY, 3, VT_I4,VT_I4,VT_I4};
_ATL_FUNC_INFO OnCaptureAvailableInfo8kCam2 = {CC_STDCALL, VT_EMPTY, 0, 0};





Sink8kCam2::Sink8kCam2(void){}
Sink8kCam2::Sink8kCam2(ICameraCtrl* pcamera,unsigned short * data)
{
	m_pCapturedDataShortCam2 = data;
	m_pCameraCam2 = pcamera;
	m_pCameraCam2->AddRef();
	DispEventAdvise((IUnknown*)m_pCameraCam2);
}

Sink8kCam2::~Sink8kCam2(void)
{
	if(m_pCameraCam2)
		m_pCameraCam2->Release();
}

void Sink8kCam2::setPathtoGainImages(CString path)
{
	mPathtoGain = path;
	CImg<unsigned short> flatfield1x;
	CImg<unsigned short> darkfield1x;
	CImg<unsigned short> flatfield2x;
	CImg<unsigned short> darkfield2x;
	CImg<unsigned short> flatfield4x;
	CImg<unsigned short> darkfield4x;

	path.Format("%sflatfield_1x_camera2.tif",mPathtoGain);
	flatfield1x.load(path);
	path.Format("%sdarkfield_1x_camera2.tif",mPathtoGain);
	darkfield1x.load(path);
	M_VALUE_CAMERA = flatfield1x.mean();

	path.Format("%sflatfield_2x_camera2.tif",mPathtoGain);
	flatfield2x.load(path);
	path.Format("%sdarkfield_2x_camera2.tif",mPathtoGain);
	darkfield2x.load(path);
	M_VALUE_CAMERA_2x = flatfield2x.mean();

	path.Format("%sflatfield_4x_camera2.tif",mPathtoGain);
	flatfield4x.load(path);
	path.Format("%sdarkfield_4x_camera2.tif",mPathtoGain);
	darkfield4x.load(path);
	M_VALUE_CAMERA_4x = flatfield4x.mean();

}
void Sink8kCam2::setImageType(int type){IMAGETYPE = type;}
int  Sink8kCam2::getImageType(){ return IMAGETYPE;}
float Sink8kCam2::getMValue(){return M_VALUE_CAMERA;}
void Sink8kCam2::setMValue(float value){M_VALUE_CAMERA = value;}
float Sink8kCam2::getMValue_2x(){return M_VALUE_CAMERA_2x;}
void Sink8kCam2::setMValue_2x(float value){M_VALUE_CAMERA_2x = value;}
float Sink8kCam2::getMValue_4x(){return M_VALUE_CAMERA_4x;}
void Sink8kCam2::setMValue_4x(float value){M_VALUE_CAMERA_4x = value;}

void Sink8kCam2::factorCalculations()
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
				flatfield1x(i,j) = (accumulatorFLAT_camera2[k] / 1);
				darkfield1x(i,j) = (accumulatorBIAS_camera2[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera2[k]=0;
				accumulatorBIAS_camera2[k]=0;
				k++;
			}
		M_VALUE_CAMERA = flatfield1x.mean();
		if(accumulatorBIAS_camera2)
			delete accumulatorBIAS_camera2;
		if(accumulatorFLAT_camera2)
			delete accumulatorFLAT_camera2;

		CString path;
		path.Format("%sflatfield_1x_camera2.tif",mPathtoGain);
		flatfield1x.save(path);
		path.Format("%sdarkfield_1x_camera2.tif",mPathtoGain);
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
				flatfield2x(i,j) = (accumulatorFLAT_camera2[k] / 1);
				darkfield2x(i,j) = (accumulatorBIAS_camera2[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera2[k]=0;
				accumulatorBIAS_camera2[k]=0;
				k++;
			}
		M_VALUE_CAMERA_2x = flatfield2x.mean();
		if(accumulatorBIAS_camera2)
			delete accumulatorBIAS_camera2;
		if(accumulatorFLAT_camera2)
			delete accumulatorFLAT_camera2;
		CString path;
		path.Format("%sflatfield_2x_camera2.tif",mPathtoGain);
		flatfield2x.save(path);
		path.Format("%sdarkfield_2x_camera2.tif",mPathtoGain);
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
				flatfield4x(i,j) = (accumulatorFLAT_camera2[k] / 1);
				darkfield4x(i,j) = (accumulatorBIAS_camera2[k] / 1);
				//clear the Flat and bais accumulators
				accumulatorFLAT_camera2[k]=0;
				accumulatorBIAS_camera2[k]=0;
				k++;
			}
		M_VALUE_CAMERA_4x = flatfield4x.mean();
		if(accumulatorBIAS_camera2)
			delete accumulatorBIAS_camera2;
		if(accumulatorFLAT_camera2)
			delete accumulatorFLAT_camera2;
		CString path;
		path.Format("%sflatfield_4x_camera2.tif",mPathtoGain);
		flatfield4x.save(path);
		path.Format("%sdarkfield_4x_camera2.tif",mPathtoGain);
		darkfield4x.save(path);
	}

			

}


void __stdcall Sink8kCam2::OnImageCaptured8kCam2(long imagesizex,long imagesizey,unsigned short * imagedata)
{
	CString imgstr;

	IMAGESIZEX = imagesizex;
	IMAGESIZEY = imagesizey;
	int totalsize = sizeof(imagedata)/sizeof(imagedata[0]);
	imgstr.Format("camera  x: %d y: %d  totalsize %d ",imagesizex,imagesizey,totalsize);
	
	if(IMAGETYPE==FLATFIELD_8k)
	{
		//if(accumulatorFLAT_camera2==NULL)
		accumulatorFLAT_camera2 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorFLAT_camera2[i]=0;}	

		for(int i=0; i < IMAGESIZEX * IMAGESIZEY;i++)
		{		
			accumulatorFLAT_camera2[i] = accumulatorFLAT_camera2[i] + m_pCapturedDataShortCam2[i];	
		}

		Data_Avail = true;
		return;
	}
	//dark field case
	else if(IMAGETYPE==DARK_8k)
	{
		//if(accumulatorBIAS_camera2==NULL)
		accumulatorBIAS_camera2 = new unsigned short [IMAGESIZEX*IMAGESIZEY];
		for(int i=0; i < (IMAGESIZEX * IMAGESIZEY);i++){accumulatorBIAS_camera2[i]=0;}
		for(int i=0; i< IMAGESIZEX * IMAGESIZEY;i++)
		{
			accumulatorBIAS_camera2[i] +=  m_pCapturedDataShortCam2[i];
		}

		Data_Avail = true;
		return;
	}
	
	else if(IMAGETYPE==CORRECTED_8k)
	{
		::Sleep(2000);
		CImg<unsigned short> flatfield1x;
		CImg<unsigned short> darkfield1x;
		CImg<unsigned short> flatfield2x;
		CImg<unsigned short> darkfield2x;
		CImg<unsigned short> flatfield4x;
		CImg<unsigned short> darkfield4x;
		CString path;

		if(IMAGESIZEX >= 4096 && IMAGESIZEY >= 4096)
		{
			
			
			path.Format("%sflatfield_1x_camera2.tif",mPathtoGain);
			flatfield1x.load(path);
			path.Format("%sdarkfield_1x_camera2.tif",mPathtoGain);
			darkfield1x.load(path);
		}

		else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
		{
			path.Format("%sflatfield_2x_camera2.tif",mPathtoGain);
			flatfield2x.load(path);
			path.Format("%sdarkfield_2x_camera2.tif",mPathtoGain);
			darkfield2x.load(path);
		}

		else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
		{
			path.Format("%sflatfield_4x_camera2.tif",mPathtoGain);
			flatfield4x.load(path);
			path.Format("%sdarkfield_4x_camera2.tif",mPathtoGain);
			darkfield4x.load(path);
		}

		CImg<unsigned short> img(m_pCapturedDataShortCam2,IMAGESIZEX,IMAGESIZEY,1,1,true); 
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
						factor1 = (float)(m_pCapturedDataShortCam2[i] - darkfield1x(j,k))* (float)M_VALUE_CAMERA;
						factor2 =(float)((float)factor1/((float)(flatfield1x(j,k) - darkfield1x(j,k))));		
					}
					else if(IMAGESIZEX >= 2048 && IMAGESIZEY >= 2048)
					{
						factor1 = (float)(m_pCapturedDataShortCam2[i] - darkfield2x(j,k))* (float)M_VALUE_CAMERA_2x;
						factor2 =(float)((float)factor1/((float)(flatfield2x(j,k) - darkfield2x(j,k))));		
					}
					else if(IMAGESIZEX >= 1024 && IMAGESIZEY >= 1024)
					{
						factor1 = (float)(m_pCapturedDataShortCam2[i] - darkfield4x(j,k))* (float)M_VALUE_CAMERA_4x;
						factor2 =(float)((float)factor1/((float)(flatfield4x(j,k) - darkfield4x(j,k))));		
					}

					m_pCapturedDataShortCam2[i] = (unsigned short)factor2;
					i = i+1;
				}
				Data_Avail = true;
				return;
	}
	else if(IMAGETYPE==UNPROCESSED_8k){
		for(int k=0; k < IMAGESIZEY*IMAGESIZEX; k++)
		{
		
			unsigned int value= m_pCapturedDataShortCam2[k];
			/*if(value <0)
				value=0;
			else if(value >65000)
				value=65000;*/
			m_pCapturedDataShortCam2[k] = value;
		}
		CImg<unsigned short> img(m_pCapturedDataShortCam2,IMAGESIZEX,IMAGESIZEY,1,1,true); 
		Data_Avail = true;
	}
}

void Sink8kCam2::saveImage(CString abspath, CString basename,long ccdb_id,int count,CString timeStamp)
{
	
	int index=0;
	CString savepath;
	CImg<unsigned short> img1 ((unsigned short*) m_pCapturedDataShortCam2,IMAGESIZEX,IMAGESIZEY,1);
	savepath.Format("%s\\CCDBid_%ld_%s_%s_%03d_cam2.tif",abspath,ccdb_id,timeStamp,basename,count);
	img1.save(savepath);

}


void Sink8kCam2::setCropPad(int xPad,int yPad)
{
	XPAD = xPad;
	YPAD = yPad;

}

void Sink8kCam2::rotateandMirror()
{
    //CImg<unsigned short> img2((unsigned short*) m_pCapturedDataShortCam2,IMAGESIZEX,IMAGESIZEY,1);
	//img2.mirror('x');
	int index=0;
		CImg<unsigned short> img1 ( IMAGESIZEX,IMAGESIZEY,1);
		for(int j=0;j<IMAGESIZEX;j++)
			for(int k=0;k<IMAGESIZEY;k++)
			{
				img1(j,k) = (unsigned short)m_pCapturedDataShortCam2[index++];
			}
		img1.rotate(90);
		img1.mirror('y');
		
		index=0;
		for(int j=0;j<IMAGESIZEX;j++)
			for(int k=0;k<IMAGESIZEY;k++)
			{
				(unsigned short)m_pCapturedDataShortCam2[index++] = img1(j,k);
			}

}
unsigned short* Sink8kCam2::getData(){return m_pCapturedDataShortCam2;}
void Sink8kCam2::setDataIsNotReady(){Data_Avail = false;}
bool Sink8kCam2::checkDataReady(){return Data_Avail;}
int Sink8kCam2::getImageSizeX(){return IMAGESIZEX;}
int Sink8kCam2::getImageSizeY(){return IMAGESIZEY;}
void __stdcall Sink8kCam2::OnCaptureAvailable8kCam2(){}
void __stdcall Sink8kCam2::OnCurrentStatus8kCam2(){}
