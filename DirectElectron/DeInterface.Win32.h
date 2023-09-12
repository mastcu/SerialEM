#ifndef DEINTERFACE_WIN32_H
#define DEINTERFACE_WIN32_H

#include <vector>
#include <string>

#ifdef DEINTERFACEWIN32_EXPORTS
#	define DEINTERFACEWIN32_API __declspec(dllexport)
#else
#	define DEINTERFACEWIN32_API __declspec(dllimport)
#endif

// Direct Electron Specific Property Labels.  These properties
// are expected for each camera.
const char* g_Property_DE_ImageWidth = "Image Size X";
const char* g_Property_DE_ImageHeight = "Image Size Y";
const char* g_Property_DE_RoiOffsetX = "ROI Offset X";
const char* g_Property_DE_RoiOffsetY = "ROI Offset Y";
const char* g_Property_DE_RoiDimensionX = "ROI Dimension X";
const char* g_Property_DE_RoiDimensionY = "ROI Dimension Y";
const char* g_Property_DE_SensorSizeX = "Sensor Size X";
const char* g_Property_DE_SensorSizeY = "Sensor Size Y";
const char* g_Property_DE_FrameTimeout = "Image Acquisition Timeout (seconds)";
const char* g_Property_DE_AcquisitionMode = "Acquisition Mode";
const char* g_Property_DE_Acquisition_LiveMode = "Live Mode";
const char* g_Property_DE_Acquisition_SingleCapture = "Single Capture";
const char* g_Property_DE_Acquisition_BurstMode = "Burst Mode";

// Optional properties but expected by Micro Manager
const char* g_Property_DE_PixelSizeX = "Pixel Size X";
const char* g_Property_DE_PixelSizeY = "Pixel Size Y";
const char* g_Property_DE_ExposureTime = "Exposure Time (seconds)";
const char* g_Property_DE_BinningX = "Binning X";
const char* g_Property_DE_BinningY = "Binning Y";

#ifndef DE_H

// forward declaration of proxy
class DEProtoProxy;
class DEError;

// This class is exported from the DeInterface.Win32.dll
class DEINTERFACEWIN32_API DeInterfaceWin32 
{
public:
	DeInterfaceWin32(void);
	~DeInterfaceWin32(void);

	bool connect(const char* ip, int rPort, int wPort);
	bool close();
	bool isConnected();

	// Config Functions
	void setParamTimeout(unsigned int seconds);
	void setImageTimeout(unsigned int seconds);

	// Set Functions
	bool setProperty(std::string prop, std::string val);
	bool setProperty(const char *prop, const char *val);
	bool setCameraName(std::string name);		
	bool setCameraName(const char *name);		

	// Get Functions
	bool getImage(void *image, unsigned int length);
	unsigned int getNumCameras();
	bool getSelectedCamera(std::string &name);
	unsigned int getNumProperties();
	bool getCameraName(unsigned int index, char* name, unsigned int length);
	bool getIsInLiveMode();
	bool getPropertyName(unsigned int index, char *name, unsigned int length);
	bool getCameraNames(std::vector<std::string> &cameras);
	bool getProperties(std::vector<std::string> &properties);
//	bool getPropertySettings(std::string prop, PropertyHelper& settings);
	bool getTypedProperty(std::string label, std::string &val);
	bool getTypedProperty(std::string label, float &val);
	bool getTypedProperty(std::string label, int &val);
	bool getProperty(const char *label, char *val, int length);
	bool getProperty(std::string label, std::string &val);
	bool setLiveMode(bool enable);

	DEError getLastError();
	int getLastErrorCode();
	std::string getLastErrorDescription();
private:
	DEProtoProxy *proxy;
	bool connected;
};
#endif  // not DE_H
#endif // DEINTERFACE_WIN32_H
