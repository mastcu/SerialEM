#pragma once



//#define cimg_use_magick
//#define cimg_imagemagick_path "C:\\Program Files\\ImageMagick-6.7.0-Q16\\convert.exe"
#include "CImg.h"
#include "afxmt.h"
using namespace cimg_library;

#define CCDB_REG_KEY_LOCATION "Software\\NCMIR\\CCDBGetID"
#define LAST_MPID_CREATED "LAST_MPID_CREATED"
#define LAST_USER "LAST_USER"

#define WEB_LINE_NUMBERS 12
#define WEB_FILE_PROPERTIES "C:\\Program Files\\SerialEM\\WebProperties.txt"
/*Sample of web file properties:
C:\\data       
C:\\data
JEOL_JEM4000EX-1
jane.crbs.ucsd.edu
7654
1
/ncmirdata5/scopes     #path of where txbr will write it
/ncmirdata5/rawdata/data/4001   #path of where it is in datajail
C:\\data   #locally what the rsync script looks at to sync data

mApachePath = web_properties[0].c_str();		
mApacheURL = web_properties[1].c_str();
mCCDBInstName = web_properties[2].c_str();
mTxBrServerName = web_properties[3].c_str();
mTxBrPortNumber = web_properties[4].c_str();
mTxBrInstName = web_properties[5].c_str();
mTxBrPathDir = web_properties[6].c_str();
mlocalDatajailpath = web_properties[7].c_str();


//enum SCOPE { J3200, J4000_1, J4000_2, FEI_titan, FEI_spirit };

*/


#include "./NCMIR/TxBr/txbrGSOAPDLL.h"
#include "./NCMIR/TxBr/txbrHeader.h"

class WebDisplay
{
public:
	WebDisplay();
	~WebDisplay(void);
	void getCCDBID();
	void saveResizeImage();
	CString getURLPath(){return mURLPath;}
	void setApachePath(CString ApachePath,CString ApacheURL);
	void setMetaDataThumbNail(double x,double y,double z,double defocus,CString index,double TiltAngle);
	void SaveThumNailImage(CString path, CString basename,long ccdbID, int sectionNumber, float TiltAngle,unsigned short* image,int sizeX,int sizeY, double meanCounts);
	void setTiltSeriesInfo(BOOL isTiltSeries);	
	void setMontageInfo(BOOL isMontage, int xNframes, int yNframes);

	void saveMontageResizeImage(char* inData,int inX,int inY); 
	//void setApachePath(CString apachepath){mApachePath = apachepath;}	
	void setApacheUrl(CString apacheurl){mApacheURL = apacheurl;}	
	//CString getApachePath(){return mApachePath;}	
	CString getApacheUrl(){return mApacheURL;}
	int readWebProperties();
	//special metadata file used for rysnc and CCDB to extract info
	void writeOutCCDBmetaData(CString inFilename, CString Basename, int CCDBid);
	void extractInstrumentFileMeta(CString FilePath, int sectionNumber);

	
private:
	CString mApacheURL;
	CString mCCDBInstName;
	CString mURLPath;

	CString mRsyncUser;
	CString mRsyncServer;
	CString mRsyncBinary;
	CString mRsyncKeyPath;
	CString mRysncServerPath;

	CString mPath;
	CString mBaseName;   
	float mTiltAngle;

	int mSectionNumber;

	CString mTxBrServerName;
	int mTxBrPortNumber;	
	int mTxBrInstName;
	CString mTxBrPathDir;
	CString mTxBrDataJailPath;

	double mCurrx;
	double mCurry;
	double mCurrz;
	double mCurrdefocus;
	CString mCurrMagindex;
	double mCurrTiltAngle;
	BOOL misMontage;
	int mXNframes;
	int mYNframes;
	BOOL misTiltSeries;
	
};

