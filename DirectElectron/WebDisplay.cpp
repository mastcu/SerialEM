#include "stdafx.h"
#include ".\webdisplay.h"


#include <fstream>
#include <string>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>



#include "../SerialEM.h"
#include "../EMScope.h"
#include "../TSController.h"
#include "../ShiftManager.h"
#include "../EMmontageController.h"
#include "../ProcessImage.h"


#include <Magick++.h>
//#include <magick/MagickCore.h>
using namespace Magick;


using namespace std;

//Write special metaData file used for CCDB.      
ofstream ccdbMetaDataFile;
CString MetaDataFile;
long m_CCDBid;

std::list<Image> tiltList;

WebDisplay::WebDisplay()
{

	//mApachePath.Format("C:\\data");
	mApacheURL.Format("C:\\data");

	m_CCDBid = 0;
	mCCDBInstName.Format("?");

	int checkApache;	
	checkApache = readWebProperties();

	if(checkApache==false)
	{
		//mApachePath.Format("C:\thumbnails");
		mApacheURL.Format("C:\thumbnails");

	}
	



}

WebDisplay::~WebDisplay(void)
{
}


int WebDisplay::readWebProperties()
{

		string web_properties[WEB_LINE_NUMBERS];	
		ifstream inFile;	
		inFile.open(WEB_FILE_PROPERTIES);
	
		if (!inFile) {	 
			AfxMessageBox("Could not read WEB properties file, ensure WebProperties.txt exists!", MB_OK | MB_ICONEXCLAMATION);	 
			SEMTrace('D', "ERROR: Could NOT read the web properties file.");
			return -1;   
	
		}	
		if (inFile.is_open())	
		{	 	 
			int i=0; 
			try{	
				while (i < WEB_LINE_NUMBERS)	
				{   	
					getline (inFile,web_properties[i++]);	
				}
				inFile.close();
		
			}catch(...) 
			{	
				return -1;
			}
	
		}
	
		
		//http://scopes.crbs.ucsd.edu/J3200  the url preview path
		mApacheURL = web_properties[0].c_str();
		//name of the instrument: JEOL JEM3200EF
		mCCDBInstName = web_properties[1].c_str();
		//jane.crbs.ucsd.edu    
		mTxBrServerName = web_properties[2].c_str();
		//the txbr port:  7654
		mTxBrPortNumber = atoi(web_properties[3].c_str());
		//number that represents the instrument for TXBR to use
		mTxBrInstName = atoi(web_properties[4].c_str());
		//the path of where the data of the volume thumbnails are written to
		mTxBrPathDir = web_properties[5].c_str();
		//the path of where the data ends up in data jail
		mTxBrDataJailPath = web_properties[6].c_str();

		//mRsyncUser,mRsyncServer
		mRsyncUser = web_properties[7].c_str();
		mRsyncServer = web_properties[8].c_str();
		//the path of the rsync installation files
		//C:/Program Files/SerialEM/SerialEMcopy
		mRsyncBinary = web_properties[9].c_str();
		//C:/SerialEMcopy
		mRsyncKeyPath = web_properties[10].c_str();
		// /ncmirdata5/scopes/J3200
		mRysncServerPath = web_properties[11].c_str();
		

		return 1;
	
}


void WebDisplay::writeOutCCDBmetaData(CString inFilename, CString Basename, int CCDBid)
{
	  
	//it will be ok to place the mrc file into the CCDBID folder.
    std::string basename;
    std::string path(inFilename);
    std::string::size_type lastSlashPos = path.find_last_of('\\');
    basename=path.substr(lastSlashPos+1,path.size()-1);
    std::string filepath;
    filepath = path.substr(0,lastSlashPos);
    CString Path(filepath.c_str());
    CString BaseName(basename.c_str());
      
    //create the directory if it doesnt exist
    CString directoryPath;      
    directoryPath.Format("%s\\CCDBid_%ld_%s",Path,CCDBid,BaseName);
    //CreateDirectory((LPCTSTR)directoryPath,NULL);
    //modify the writing of the mrc file.
    //inFilename.Format("%s\\CCDBid_%ld_%s\\%s",Path,CCDBid,BaseName,BaseName);  
    MetaDataFile.Format("%s\\CCDB_MetaData\\%s_instrumentMetaData.txt",inFilename,Basename);
  
	//Are we doing a Tilt Series?  
	CSerialEMApp *mWinApp = (CSerialEMApp *)AfxGetApp();  
	ccdbMetaDataFile.open(MetaDataFile);

	ccdbMetaDataFile<<"Imaging parameter="<<"EM"<<std::endl<<std::endl;	
	ccdbMetaDataFile<<"Microscopy product type="<<"TEM"<<std::endl<<std::endl;
	ccdbMetaDataFile<<"Imaging_Type="<<"TEM"<<std::endl<<std::endl;
	ccdbMetaDataFile<<"Instrument Type="<<mCCDBInstName<<std::endl<<std::endl;	
	ccdbMetaDataFile<<"Acc_Voltage="<<mWinApp->mScope->GetHTValue()<<"keV"<<endl<<endl;

	CameraParameters* camParams = mWinApp->GetCamParams();
	CString name = camParams[mWinApp->GetCurrentCamera()].name;
	ccdbMetaDataFile<<"Cam_type="<<name<<std::endl<<std::endl;
	int pixel_pitch = (int)camParams[mWinApp->GetCurrentCamera()].pixelMicrons;
	ccdbMetaDataFile<<"pixel_pitch="<<pixel_pitch<<" microns"<<std::endl<<std::endl;

  
	float pixelSize = mWinApp->mShiftManager->GetPixelSize(mWinApp->GetCurrentCamera(),mWinApp->mScope->GetMagIndex());
	ccdbMetaDataFile<<"PIXEL_SIZE_X="<<pixelSize<<"um"<<endl<<endl;
	ccdbMetaDataFile<<"PIXEL_SIZE_Y="<<pixelSize<<"um"<<endl<<endl;
    
	int dimX = camParams[mWinApp->GetCurrentCamera()].sizeX;
	int dimY = camParams[mWinApp->GetCurrentCamera()].sizeY;
	
	ccdbMetaDataFile<<"Dimension X="<<dimX<<" pixels"<<endl<<endl;
	ccdbMetaDataFile<<"Dimension Y="<<dimY<<" pixels"<<endl<<endl;

  
	float sizeX = (float)(pixelSize * dimX);
	float sizeY = (float)(pixelSize * dimY);
	
 
	ccdbMetaDataFile<<"Size X="<<sizeX<<" um"<<endl<<endl;
	ccdbMetaDataFile<<"Size Y="<<sizeY<<" um"<<endl<<endl; 
	ccdbMetaDataFile<<"MAG="<<mWinApp->mScope->GetMagnification()<<endl<<endl;
	BOOL isTiltSeries = mWinApp->mTSController->DoingTiltSeries();  
    
	//Are we doing a Mosaic?  
	BOOL isMontage = mWinApp->mMontageController->DoingMontage();  
	int xNframes,yNframes;  

 
	//Tilt Increment
	TiltSeriesParam*  tsParams = mWinApp->mTSController->GetTiltSeriesParam();  
  
	float tiltIncrement = tsParams->tiltIncrement;  
	CString tiltIncrementStr;        
	tiltIncrementStr.Format("%0.2f",tiltIncrement);

  
	CString tiltRangeStr;
  
	float startAngle = tsParams->startingTilt; 
	float endAngle = tsParams->endingTilt;  
	tiltRangeStr.Format("%0.2f deg to %0.2f deg",startAngle,endAngle);

  
	CString montageStr;    
	CString productType("");

	MontParam *montP = mWinApp->GetMontParam(); 
	xNframes = montP->xNframes;  
	yNframes = montP->yNframes;
	montageStr.Format("%d x %d", xNframes, yNframes);

  
	if(isTiltSeries && isMontage)
	{	  
		productType.Format("Montage TiltSeries(%d X %d)",xNframes,yNframes);
		ccdbMetaDataFile<<"Tilt_Increment="<<tiltIncrementStr<<endl<<endl;
		ccdbMetaDataFile<<"Tilt_Range="<<tiltRangeStr<<endl<<endl;      
		ccdbMetaDataFile<<"3D_Tilt_Montage_size="<<montageStr<<endl<<endl;  
	}  
  
	else if(isMontage)  
	{
		ccdbMetaDataFile<<"2D_Stage_Montage_size="<<montageStr<<endl<<endl;      
		productType.Format("Montage(%dX%d)",xNframes,yNframes);  
	}    
	else if(isTiltSeries){   
	  productType.Format("Tilt Series");
	  ccdbMetaDataFile<<"Tilt_Increment="<<tiltIncrementStr<<endl<<endl;      
	  ccdbMetaDataFile<<"Tilt_Range="<<tiltRangeStr<<endl<<endl;  
	}  
	else {     
		productType.Format("Survey");  
	}   
	ccdbMetaDataFile<<"Acquisition Type="<<productType<<endl<<endl;   
	ccdbMetaDataFile.close();

}

void WebDisplay::saveMontageResizeImage(char* inData,int inX,int inY)
{
    CSerialEMApp *mWinApp = (CSerialEMApp *)AfxGetApp();
    EMimageBuffer* mImBufs = mWinApp->GetImBufs();
    double meanCounts = mWinApp->mProcessImage->WholeImageMean(mImBufs);
    SaveThumNailImage(mPath,mBaseName,m_CCDBid,mSectionNumber,mTiltAngle,(unsigned short*)inData,inX,inY,meanCounts);   

}

void WebDisplay:: extractInstrumentFileMeta(CString FilePath, int sectionNumber)
{

    CString saveStr;
    std::string basename;
    std::string path(FilePath);
    std::string::size_type lastSlashPos=path.find_last_of('\\'); 
    basename=path.substr(lastSlashPos+1,path.size()-1);
    std::string filepath; 
    filepath = path.substr(0,lastSlashPos);
    CString Path(filepath.c_str());
    CString BaseName(basename.c_str());

    mPath = Path;
    mBaseName = BaseName;
    mSectionNumber = sectionNumber;

    //saveStr.Format("the file to save to is:  %s with name %s with section number %d",Path,BaseName, inStore->getDepth()); 
   
    //get the latest CCDBID to work with
    getCCDBID();

    //write out the ccdbmetadatafile
    writeOutCCDBmetaData(Path, BaseName, m_CCDBid);
 
    CSerialEMApp *mWinApp = (CSerialEMApp *)AfxGetApp(); 
    mTiltAngle = (float)mWinApp->mScope->GetTiltAngle();   
    double x,y,z;   
    double defocus;   
    int index;   
    mWinApp->mScope->GetStagePosition(x,y,z);   
    defocus = mWinApp->mScope->GetDefocus();   
    mWinApp->mScope->GetFocus();   
    index = mWinApp->mScope->GetMagIndex();
       
    
    CString magValue;

#ifdef NCMIR_CAMERA
    magValue = mWinApp->mScope->GetFasTEMMag();
#else   
    double magV = mWinApp->mScope->GetMagnification();
    magValue.Format("%0.2f",magV);
#endif
           
    //Are we doing a Tilt Series?   
    BOOL isTiltSeries = mWinApp->mTSController->DoingTiltSeries();

   
   
    if(isTiltSeries)   
    {
            //CString status =  mWinApp->GetStatusText(COMPLEX_PANE);   
    }   
    //Are we doing a Mosaic?   
    BOOL isMontage = mWinApp->mMontageController->DoingMontage();   
    int xNframes,yNframes;   
    if(isMontage)   
    {   
        MontParam *montP = mWinApp->GetMontParam();   
        xNframes = montP->xNframes;   
        yNframes = montP->yNframes;       
    }
    setTiltSeriesInfo(isTiltSeries);
    setMontageInfo(isMontage, xNframes, yNframes);   
    setMetaDataThumbNail(x,y,z,defocus,magValue,mTiltAngle);


}

void WebDisplay::saveResizeImage()
{


	CSerialEMApp *mWinApp = (CSerialEMApp *)AfxGetApp();
		
	EMimageBuffer* mImBufs = mWinApp->GetImBufs();
	CameraParameters* camParams = mWinApp->GetCamParams();
  	
	int dimX = mImBufs->mImage->getWidth();
	int dimY = mImBufs->mImage->getHeight();
	//int dimX = camParams[mWinApp->GetCurrentCamera()].sizeX;
	//int dimY = camParams[mWinApp->GetCurrentCamera()].sizeY;

	double meanCounts = mWinApp->mProcessImage->WholeImageMean(mImBufs);
	SaveThumNailImage(mPath,mBaseName,m_CCDBid,mSectionNumber,mTiltAngle,( unsigned short*)mImBufs->mImage->getData(),dimX, dimY,meanCounts);    

}



void WebDisplay::setMetaDataThumbNail(double x,double y,double z,double defocus,CString magIndex,double TiltAngle)
{
	mCurrx = x;
	mCurry = y;
	mCurrz = z;
	mCurrdefocus = defocus;
	mCurrMagindex = magIndex;
	mCurrTiltAngle = TiltAngle;
}

void WebDisplay::setTiltSeriesInfo(BOOL isTiltSeries)
{
	misTiltSeries = isTiltSeries;

}
	

void WebDisplay::setMontageInfo(BOOL isMontage, int xNframes, int yNframes)
{
	misMontage = isMontage;
	mXNframes = xNframes;
	mYNframes = yNframes;

}

void WebDisplay::getCCDBID()
{
	 
	CString lastMPID = _T("");
    CString lastUser = _T("");
    HKEY hKey;
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T(CCDB_REG_KEY_LOCATION),
                0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        TCHAR szData[2048];
        TCHAR szData1[2048];
        DWORD dwKeyDataType;
        DWORD dwDataBufSize = 2048;
        DWORD dwDataBufSize1 = 2048;
        if (::RegQueryValueEx(hKey, _T(LAST_MPID_CREATED), NULL, &dwKeyDataType,
                (LPBYTE) &szData, &dwDataBufSize) == ERROR_SUCCESS)
        {
            switch ( dwKeyDataType )
            {
                case REG_SZ:
                    lastMPID = CString(szData);
                    //break;
            }
        }
        if (::RegQueryValueEx(hKey, _T(LAST_USER), NULL, &dwKeyDataType,
                (LPBYTE) &szData1, &dwDataBufSize1) == ERROR_SUCCESS)
        {
            switch ( dwKeyDataType )
            {
                case REG_SZ:
                    lastUser = CString(szData1);
                    //break;
            }
        }
        ::RegCloseKey( hKey );
    }

	
    m_CCDBid = atol(lastMPID);
    if(m_CCDBid <= 0)
    {
        //no id
        AfxMessageBox("Warning: You did not provide a CCDB ID.");
        m_CCDBid = 0;
        return;

    }
    else
    {
        
        CString mesg;
        mesg.Format("The MPID of: %s by user %s has been set.",lastMPID,lastUser);
        //AfxMessageBox(mesg);


    }

}





void WebDisplay::SaveThumNailImage(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle, unsigned short* image,int sizeX,int sizeY, double meanCounts)
{
	//ThumbNail size factor
    //Build up the path to save the data
    //Divide by this amount

    int thumbNailFactor=0;
    int cropFactor=0;

	if(sizeX < 512 || sizeY < 512)
        thumbNailFactor = 1;
    else if(sizeX >= 4096 || sizeY >= 4096)
        thumbNailFactor = 12;
	else if(sizeX==2048 ||sizeY==2048)
		thumbNailFactor = 6;
    else
        thumbNailFactor = 2;

  


    int thumbSizeX = sizeX/thumbNailFactor;
    int thumbSizeY = sizeY/thumbNailFactor;


	//we have to clear the tiltList file 
	if(misTiltSeries)
		if(sectionNumber == 0)
			tiltList.clear();

   
    if(misMontage)
    {
        sectionNumber = sectionNumber/(mXNframes*mYNframes);
        //sectionNumber = sectionNumber - 1;
        cropFactor = 50;

    }



	//mApachePath
	//mApachePath represents the physical path of where to put the data
	//so something like :  /ncmirdata5/scopes/J3200
	//mApacheURL
	//is how to render it in a browser with a valid served up path such as:
	//http://scopes.crbs.ucsd.edu/J3200/

	//WIB
	//http://ccdb-portal.crbs.ucsd.edu/WebImageBrowser/cgi-bin/start.pl?volumeID=file:


	CString apachePath;
	//apachePath.Format("%s\\CCDBid_%ld_%s",mApachePath,ccdbID,basename);
	apachePath.Format("%s\\CCDBid_%ld_%s",path,ccdbID,basename);
	CreateDirectory((LPCTSTR)apachePath,NULL);



	CString htmlPath;
	//htmlPath.Format("%s\\CCDBid_%ld_%s.html",mApachePath,ccdbID,basename);
	htmlPath.Format("%s\\CCDBid_%ld_%s.html",apachePath,ccdbID,basename);
	CString urlPath;
	urlPath.Format("%s/CCDBid_%ld_%s.html",mApacheURL,ccdbID,basename);
	//urlPath.Format("%s/CCDBid_%ld_%s.html",path,ccdbID,basename);
	mURLPath = urlPath;
	
	CString allImagesurlPath;
	allImagesurlPath.Format("%s/CCDBid_%ld_%s/CCDBid_%ld_%s_allimages.html",mApacheURL,ccdbID,basename,ccdbID,basename);
	CString allImageshtmlPath;
	//allImageshtmlPath.Format("%s\\CCDBid_%ld_%s_allimages.html",mApachePath,ccdbID,basename);
	allImageshtmlPath.Format("%s\\CCDBid_%ld_%s_allimages.html",apachePath,ccdbID,basename);



	CTime ctdt = CTime::GetCurrentTime();
	CString timeStamp;
    timeStamp.Format("_%4d%02d%02d%02d%02d%02d", ctdt.GetYear(), ctdt.GetMonth(), ctdt.GetDay(), ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());

	char thumbnailpath[1800];
	sprintf(thumbnailpath,"%s\\CCDBid_%ld_%s_%03d.jpg",apachePath,ccdbID,basename,sectionNumber);

	CImg<unsigned short> image2save;
	image2save.assign(image,sizeX,sizeY);
  
	//setup for WIB images
    CString wibDir;
    wibDir.Format("%s\\WIB",apachePath);
    CreateDirectory((LPCTSTR)wibDir,NULL);

    CImg<unsigned short> wib_thumbnail(sizeX,sizeY);
    wib_thumbnail.assign(image2save.get_resize(sizeX,sizeY));
    wib_thumbnail.normalize(0,65000);
    char wib_thumbnailpath[2000];

    sprintf(wib_thumbnailpath,"%s\\CCDBid_%ld_%s_%03d.tif",wibDir,ccdbID,basename,sectionNumber);
    Magick::Image wib_thumb;
    wib_thumb.size(Magick::Geometry(wib_thumbnail.dimx(),wib_thumbnail.dimy()));
    wib_thumb.type(GrayscaleType);
    PixelPacket *wib_imagepixels = wib_thumb.getPixels(0, 0, wib_thumbnail.dimx(),wib_thumbnail.dimy());

    for(int i=0 ; i < wib_thumbnail.dimx()*wib_thumbnail.dimy(); i++)
    {
        wib_imagepixels->red = wib_thumbnail[i];
        wib_imagepixels->blue = wib_thumbnail[i];
        wib_imagepixels->green = wib_thumbnail[i];
        wib_imagepixels++;
    }
    wib_thumb.syncPixels();
    std::list<Image> wibList;           
    wibList.push_back(wib_thumb);           
    writeImages( wibList.begin(), wibList.end(), wib_thumbnailpath); 



	//setup TxBr off
	//TxBr Off
	//Something like C:\data\tmolina\CCDBid_0_blah\srcjpgs
	CString txbrPath;
	txbrPath.Format("%s\\src_jpegs",apachePath);
	CreateDirectory((LPCTSTR)txbrPath,NULL);

	CImg<unsigned short> txbr_thumbnail(thumbSizeX,thumbSizeY);
	txbr_thumbnail.assign(image2save.get_resize(thumbSizeX,thumbSizeY));

	if(misMontage)   
        txbr_thumbnail.crop(cropFactor,cropFactor,thumbSizeX-cropFactor,thumbSizeY-cropFactor,false);


	txbr_thumbnail.normalize(0,65000);
	char txbr_thumbnailpath[2000];

	sprintf(txbr_thumbnailpath,"%s\\CCDBid_%ld_%s_%0.1f.jpg",txbrPath,ccdbID,basename,mCurrTiltAngle);
	Magick::Image txbr_thumb;
	txbr_thumb.size(Magick::Geometry(txbr_thumbnail.dimx(),txbr_thumbnail.dimy()));
	txbr_thumb.type(GrayscaleType);
	PixelPacket *txbr_imagepixels = txbr_thumb.getPixels(0, 0, txbr_thumbnail.dimx(),txbr_thumbnail.dimy());

	for(int i=0 ; i < txbr_thumbnail.dimx()*txbr_thumbnail.dimy(); i++)
	{
		txbr_imagepixels->red = txbr_thumbnail[i];
		txbr_imagepixels->blue = txbr_thumbnail[i];
		txbr_imagepixels->green = txbr_thumbnail[i];
		txbr_imagepixels++;
	}
	txbr_thumb.syncPixels();

	try{
		std::list<Image> imagesList;		
		imagesList.push_back(txbr_thumb);		
		writeImages( imagesList.begin(), imagesList.end(), txbr_thumbnailpath); 	

	}
	catch(Exception e1)
	{
		txbr_thumbnail.save(txbr_thumbnailpath);
	}
	
	unsigned short white[] = { 65000,65000,65000 }; 
	unsigned short black[] = {0,0,0};
	CImg<unsigned short> normalized_thumbnail(thumbSizeX,thumbSizeY);
	normalized_thumbnail.assign(image2save.get_resize(thumbSizeX,thumbSizeY));

	if(misMontage)
    {
        normalized_thumbnail.crop(cropFactor,cropFactor,thumbSizeX-cropFactor,thumbSizeY-cropFactor,false);

    }



	normalized_thumbnail.normalize(0,65000);


	char thumbnailMessage[2000];
	sprintf(thumbnailMessage,"Name- %s CCDB ID- %ld\nSec- %d Tilt-  %0.2f%c Mean- %0.2f \nTime- %02d-%02d-%02d",basename,ccdbID,sectionNumber,TiltAngle,0x00B0,meanCounts,ctdt.GetHour(), ctdt.GetMinute(), ctdt.GetSecond());
	char thumbnailMessage1[2000];
	sprintf(thumbnailMessage1,"X- %0.2f Y- %0.2f Z- %0.2f Mag- %s DF- %0.2f",mCurrx,mCurry,mCurrz,mCurrMagindex,mCurrdefocus);
	normalized_thumbnail.draw_text(thumbnailMessage,1,1,white,black,(unsigned int)1.0);
	normalized_thumbnail.draw_text(thumbnailMessage1,0,thumbSizeY-12,white,black,(unsigned int)1.0);

	
	Image thumb;
	thumb.size( Magick::Geometry(normalized_thumbnail.dimx(),normalized_thumbnail.dimy()));
	thumb.depth(16);
	thumb.type(GrayscaleType);
	

	PixelPacket *imagepixels = thumb.getPixels(0, 0, normalized_thumbnail.dimx(), normalized_thumbnail.dimy());

	for(int i=0 ;i < normalized_thumbnail.dimx() * normalized_thumbnail.dimy();i++)
	{
		imagepixels->red = normalized_thumbnail[i];
		imagepixels->blue = normalized_thumbnail[i];
		imagepixels->green = normalized_thumbnail[i];
		imagepixels++;
	}
	thumb.syncPixels();

	try{
		std::list<Image> imagesList;			
		imagesList.push_back(thumb);
		//add it to gif image, but only if we are doing a tiltSeries
		if(misTiltSeries)
			tiltList.push_back(thumb);
		
		writeImages( imagesList.begin(), imagesList.end(), thumbnailpath); 
	}catch(...){
	
		normalized_thumbnail.save(thumbnailpath);

	}

	ofstream fout;
	
	fout.open(htmlPath,ios::out);
	fout<<"<html>"<<endl;
    fout<<"<head><script type=\"text/JavaScript\">\n<!--\nfunction timedRefresh(timeoutPeriod) {";
	fout<<"setTimeout(\"location.reload(true);\",timeoutPeriod);}\n// -->";
	fout<<"\n</script>"<<endl;
	fout<<"<link rel=\"stylesheet\" type=\"text/css\" href=\""<<mApacheURL<<"/style.css\" media=\"screen\" />";
	fout<<"</head><body onload=\"JavaScript:timedRefresh(30000);\">";


	CSerialEMApp *mWinApp = (CSerialEMApp *)AfxGetApp();  
	CameraParameters* camParams = mWinApp->GetCamParams();
	CString cameraName = camParams[mWinApp->GetCurrentCamera()].name;
	

	fout<<"<h1>"<<"Instrument: "<<mCCDBInstName<<"</h1><br/>"<<endl;
	fout<<"<h1>"<<"Camera:  "<<cameraName<<"</h1><br/>"<<endl;
	fout<<"<h1>"<<"CCDB Microscopy Product ID: "<<ccdbID<<" BaseName:  "<<basename<<"</h1><br/>";
	fout<<"<h2>"<<ctdt.GetMonth()<<"/"<<ctdt.GetDay()<<"/"<< ctdt.GetYear()<<"</h2><br/>"<<endl;
	CString time;
	time.Format("Time %02d-%02d-%02d",ctdt.GetHour(), ctdt.GetMinute(),ctdt.GetSecond());
	fout<<"<h3>"<<time<<"</h3><br/>";
	fout<<"<h3>"<<"Current Status: </h3><br/>"<<endl;
	fout<<"<h3>"<<"Has taken "<<sectionNumber+1<<" total image(s) so far.</h3><br/>"<<endl;


	if(misTiltSeries)
	{
		if(sectionNumber > 0)
		{
			//invoke the webservice to run TxBr.
			enum SCOPE scope_type;
			enum FILE_TYPE file_type;
			//enum SCOPE { J3200, J4000_1, J4000_2, FEI_titan, FEI_spirit };
			scope_type = (SCOPE)mTxBrInstName;
			file_type = JPG;
			//C:\data\ccdbuser\CCDBID_1_a
			//CString jailPath = path.Mid(mlocalDatajailpath.GetLength(),path.GetLength());
			CString jailPath = "";
			jailPath.Replace('\\','/');
			// Convert to a char*
			const size_t newsize = 1000;
			char txbrserver[newsize];
			sprintf(txbrserver,"%s", mTxBrServerName);
			char txbrDir[newsize]; // something like "CCDBid_0_pass15"
			sprintf(txbrDir,"CCDBid_%ld_%s",ccdbID,basename);
			char workingDir[newsize]; //something like "/ncmirdata5/scopes/CCDBid_0_pass27";
			sprintf(workingDir,"%s/%s",mTxBrPathDir,txbrDir);
			char dataJailPath[newsize];	
			sprintf(dataJailPath,"%s/%s",mTxBrDataJailPath,jailPath);
	
			//An example of what it should call:		
			//soap_call_txbr__resetOFFTxbr(&soap, "http://jane.crbs.ucsd.edu:7654", "", "/ncmirdata5/scopes/onthefly1", return_value) == SOAP_OK)		
			//int returnValue = txbr_runOFFTxbr(txbrserver, mTxBrPortNumber, dataJailPath, workingDir,txbrDir, file_type,scope_type);       
			int returnValue = txbr_runOFFTxbr(txbrserver, mTxBrPortNumber, workingDir, workingDir,txbrDir, file_type,scope_type);
		
			if(returnValue==1)
			{
				CString txbrURLResultPage;               
				txbrURLResultPage.Format("%s/CCDBid_%ld_%s/vol_dir/result.html",mApacheURL,ccdbID,basename);            			
				CString txbrZAnim;               	
				txbrZAnim.Format("%s/CCDBid_%ld_%s/vol_dir/Z-anim.gif",mApacheURL,ccdbID,basename);             	
				fout<<"<h3>"<<"TxBr Process has been invoked</h3><br/>"<<endl;
				fout<<"<h3>View Quick Volume reconstruction: <a href=\""<<txbrURLResultPage<<"\">results</a></h4><br/>"<<endl;    
				fout<<"<h3>View Animated volume: <a href=\""<<txbrZAnim<<"\">Z volume</a></h4><br/>"<<endl;
				fout<<"<h3>"<<"TxBr Process has been invoked</h3><br/>"<<endl;
		
			}
			else
			
				fout<<"<h3>"<<"TxBr Process failed to be invoked</h3><br/>"<<endl;


	
		
			fout<<"<h3>"<<"Doing Tilt Series....</h3><br/>"<<endl;	      
		
			char writeImagesLocation[2000];     		
			sprintf(writeImagesLocation,"%s\\animatedGIF.gif",apachePath);         		
			writeImages( tiltList.begin(), tiltList.end(), writeImagesLocation);
       	
			CString animatedGifPath;           	
			animatedGifPath.Format("%s/CCDBid_%ld_%s/animatedGIF.gif",mApacheURL,ccdbID,basename);          	
			fout<<"<img src=\""<<animatedGifPath<<"\"/>"<<endl;    
			fout<<"<br/><br/>"<<endl;
		

		}
		else
			tiltList.clear(); //we must be a brand new file. make sure we do not write anything new

	
	}
	if(misMontage)
		fout<<"<h3>"<<"Doing Montage of size:  X:"<<mXNframes<<"  Y:"<<mYNframes<<"</h3><br/>"<<endl;

		
	fout<<"<br/><br/><br/><br/>"<<endl;
	
	fout<<"<h4>Most Recent Image (with the last 10 images following it):</h4><br/>"<<endl;
	fout<<"<h4>View All images: <a href=\""<<allImagesurlPath<<"\">all images</a></h4><br/>"<<endl;

	int row_count=0;
	int total_image_display = 0;

	for(int i = sectionNumber; i >= 0; i--, total_image_display++)
	{

		if(total_image_display >=10) break;

		
		CString imagepath;
		imagepath.Format("%s/CCDBid_%ld_%s/CCDBid_%ld_%s_%03d.jpg\"",mApacheURL,ccdbID,basename,ccdbID,basename,i);
		  
		//wrap it around a WIB link
		CString wibimagePath;
        wibimagePath.Format("/CCDBid_%ld_%s/WIB/CCDBid_%ld_%s_%03d.tif\"",ccdbID,basename,ccdbID,basename,i);
        CString wibHost;
        wibHost.Format("http://ccdb-dev-portal.crbs.ucsd.edu/WebImageBrowser/cgi-bin/start.pl?volumeID=file:");
        fout<<"<a href=\""<<wibHost<<mRysncServerPath<<wibimagePath<<">";
		
		
		fout<<"<img src=\""<<imagepath<<"/>"<<endl;
		fout<<"</a>"<<endl;
		
		if(row_count==2)
		{
			fout<<"<br/><br/>"<<endl;
			row_count =0;
		}
		else row_count++;
	}
	fout<<"</body></html>"<<endl;
	fout.flush();
	fout.close();


	//all images Page
	fout.open(allImageshtmlPath,ios::out);
	fout<<"<html>"<<endl;
    fout<<"<head><link rel=\"stylesheet\" type=\"text/css\" href=\""<<mApacheURL<<"/style.css\" media=\"screen\" />";
	fout<<"</head>";

	fout<<"<h1>"<<"All images:</h1><br/>"<<endl;
	row_count=0;

	for(int i = 0; i <= sectionNumber; i++)
	{

		CString wibimagePath;
        wibimagePath.Format("/CCDBid_%ld_%s/WIB/CCDBid_%ld_%s_%03d.tif\"",ccdbID,basename,ccdbID,basename,i);
        CString wibHost;
        wibHost.Format("http://ccdb-dev-portal.crbs.ucsd.edu/WebImageBrowser/cgi-bin/start.pl?volumeID=file:");


		CString imagepath;
		imagepath.Format("%s/CCDBid_%ld_%s/CCDBid_%ld_%s_%03d.jpg\"",mApacheURL,ccdbID,basename,ccdbID,basename,i);
		fout<<"<a href=\""<<wibHost<<mRysncServerPath<<wibimagePath<<">";
		fout<<"<img src=\""<<imagepath<<"/>"<<endl;
		fout<<"</a>"<<endl;

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


	//Hopefully everything is saved at this point and its time to rsync everything over
	//build up the system command


	CString rsyncCommand;
    CString rsyncCopyHtml;
    rsyncCommand.Format("\"%s/rsync.exe\" --chmod g+rwx,a+rx -av -e \"ssh -i \"%s/.ssh/id_dsa\"\" ",mRsyncBinary,mRsyncKeyPath);

	//we have to use a unix like file path for this.

	int colonMatch = apachePath.Find(":");
	TCHAR drvLetter = apachePath.GetAt(colonMatch - 1);

	CString windowsDriveLetter;
	windowsDriveLetter.Format("%c:\\",drvLetter);
	CString unixDriveLetter;
	unixDriveLetter.Format("/cygdrive/%c/",drvLetter);
	apachePath.Replace(windowsDriveLetter,unixDriveLetter);

	apachePath.Replace("\\","/");
    rsyncCopyHtml.Format("%s -r \"%s\" %s@%s:%s",rsyncCommand,apachePath,mRsyncUser,mRsyncServer,mRysncServerPath);
    //system(rsyncCopyHtml.GetBuffer());

    PROCESS_INFORMATION pi;   
    STARTUPINFO si;   
    std::memset(&pi, 0, sizeof(PROCESS_INFORMATION));   
    std::memset(&si, 0, sizeof(STARTUPINFO));   
    GetStartupInfo(&si);   
    si.cb = sizeof(si);   
    si.wShowWindow = SW_HIDE;   
    si.dwFlags |= SW_HIDE;
    char command[2000];
    sprintf(command,"%s",rsyncCopyHtml.GetBuffer(0));
    const BOOL res = CreateProcess(NULL,(LPTSTR)command,0,0,FALSE,CREATE_NO_WINDOW,0,0,&si,&pi);

    //update the index file
    rsyncCommand.Format("\"%s/rsync.exe\" --chmod g+rwx,a+rx -a -q -e \"ssh -i \"%s/.ssh/id_dsa\"\" ",mRsyncBinary,mRsyncKeyPath);
    
	
	windowsDriveLetter.Format("%c:\\",drvLetter);
	unixDriveLetter.Format("/cygdrive/%c/",drvLetter);
	apachePath.Replace(windowsDriveLetter,unixDriveLetter);

    apachePath.Replace("\\","/");
    rsyncCopyHtml.Format("%s \"%s/CCDBid_%ld_%s.html\" %s@%s:%s/index.html",rsyncCommand,apachePath,ccdbID,basename,mRsyncUser,mRsyncServer,mRysncServerPath);
    sprintf(command,"%s",rsyncCopyHtml.GetBuffer(0));
    const BOOL res1 = CreateProcess(NULL,(LPTSTR)command,0,0,FALSE,CREATE_NO_WINDOW,0,0,&si,&pi);



}


