/*
  Copyright (c) 2008-2021 Direct Electron LP. All rights reserved.

  Redistributions of this file is prohibited without express permisstion. 

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
#pragma once

#ifndef DE_H
#define DE_H

#include <vector>
#include <string>
#include <map>

#ifdef DE_EXPORTS
#	define DE_API __declspec(dllexport)
#else
#	define DE_API __declspec(dllimport)
#endif

namespace DE
{
	enum class FrameType
	{
        NONE                                   = 0,    // No image 
		AUTO                                   = 1,    // Server automatically choose the frame type for the final image
		CRUDEFRAME                             = 2,    // For engineering testing
		SINGLEFRAME_RAWLEVEL0                  = 3,    // For engineering testing
		SINGLEFRAME_RAWLEVEL1                  = 4,    // For engineering testing
		SINGLEFRAME_RAWLEVEL2                  = 5,    // For engineering testing
		SINGLEFRAME_RAWOFFCHIPCDS              = 6,    // For engineering testing
		SINGLEFRAME_INTEGRATED                 = 7,    // Single integrated frame
		SINGLEFRAME_COUNTED                    = 8,    // Single counting frame
		SUMINTERMEDIATE                        = 9,    // Fractionated integrated/counting frame
		SUMTOTAL                               = 10,   // Sum of all integrated/counting frame
		CUMULATIVE                             = 11,   // Cumulative integrated/counting frame
		VIRTUAL_MASK0                          = 12,   
		VIRTUAL_MASK1                          = 13,
		VIRTUAL_MASK2                          = 14,
		VIRTUAL_MASK3                          = 15,
		VIRTUAL_MASK4                          = 16,
		VIRTUAL_IMAGE0                         = 17,
		VIRTUAL_IMAGE1                         = 18,
		VIRTUAL_IMAGE2                         = 19,
		VIRTUAL_IMAGE3                         = 20,
		VIRTUAL_IMAGE4                         = 21,
		EXTERNAL_IMAGE1                        = 22,
		EXTERNAL_IMAGE2                        = 23,
		EXTERNAL_IMAGE3                        = 24,
		EXTERNAL_IMAGE4                        = 25,
		DEBUG_INPUT1                           = 26,   // For engineering testing
		DEBUG_INPUT2                           = 27,   // For engineering testing
		DEBUG_CENTROIDINGEVENTMOMENTSX         = 28,   // For engineering testing
		DEBUG_CENTROIDINGEVENTMOMENTSY         = 29,   // For engineering testing
		DEBUG_CENTROIDINGEVENTMOMENTSINTENSITY = 30,   // For engineering testing
		DEBUG_CENTROIDINGEVENTLABELS           = 31,   // For engineering testing
		DEBUG_CENTROIDINGEVENTOTHERDATA        = 32,   // For engineering testing
		DEBUG_DEEPMINX                         = 33,   // For engineering testing
		DEBUG_DEEPMINY                         = 34,   // For engineering testing
		DEBUG_DEEPMAXX                         = 35,   // For engineering testing
		DEBUG_DEEPMAXY                         = 36,   // For engineering testing
		REFERENCE_DARKLEVEL0                   = 37,   // For engineering testing
		REFERENCE_DARKLEVEL1                   = 38,   // For engineering testing
		REFERENCE_DARKLEVEL2                   = 39,   // For engineering testing
		REFERENCE_DARKOFFCHIPCDS               = 40,   // For engineering testing
		REFERENCE_GAININTEGRATINGLEVEL0        = 41,   // For engineering testing
		REFERENCE_GAININTEGRATINGLEVEL1        = 42,   // For engineering testing
		REFERENCE_GAININTEGRATINGLEVEL2        = 43,   // For engineering testing
		REFERENCE_GAINCOUNTING                 = 44,   // For engineering testing
		REFERENCE_GAINFINALINTEGRATINGLEVEL0   = 45,   // For engineering testing
		REFERENCE_GAINFINALCOUNTING            = 46,   // For engineering testing
		REFERENCE_BADPIXELS                    = 47,   // For engineering testing
		REFERENCE_BADPIXELMAP                  = 48,   // For engineering testing
		NUMBER_OF_OPTIONS                      = 49,   // Number of options
		// deprecated frame type 
		CRUDE_FRAME                            =  2,    // for engineering testing        #Same as CRUDEFRAME  
		SINGLE_FRAME_RAW_LEVEL0                =  3,    // for engineering testing        #Same as SINGLEFRAME_RAWLEVEL0
		SINGLE_FRAME_RAW_LEVEL1                =  4,    // for engineering testing        #Same as SINGLEFRAME_RAWLEVEL1
		SINGLE_FRAME_RAW_LEVEL2                =  5,    // for engineering testing        #Same as SINGLEFRAME_RAWLEVEL2
		SINGLE_FRAME_RAW_CDS                   =  6,    // for engineering testing        #Same as SINGLEFRAME_RAWOFFCHIPCDS
		SINGLE_FRAME_INTEGRATED                =  7,    // single integrated frame        #Same as SINGLEFRAME_INTEGRATED
		SINGLE_FRAME_COUNTED                   =  8,    // single counted frame           #same as SINGLEFRAME_COUNTED
		MOVIE_INTEGRATED                       =  9,    // fractionated integrated frame  #same as SUMINTERMEDIATE
		MOVIE_COUNTED                          =  9,    // fractionated counted frame     #same as SUMINTERMEDIATE
		TOTAL_SUM_INTEGRATED                   =  10,   // sum of all integrated frame    #same as SUMTOTAL
		TOTAL_SUM_COUNTED                      =  10,   // sum of all counted frame       #same as SUMTOTAL
		CUMULATIVE_INTEGRATED                  =  11,   // cumulative integrted frame     #same as CUMULATIVE
		CUMULATIVE_COUNTED                     =  11,   // cumulative counted frame       #same as CUMULATIVE
	};

	enum class PixelFormat
	{
		UINT8   = 1,
		UINT16  = 5,
		FLOAT32 = 13,
		AUTO    = -1
	};

	enum class ContrastStretchType {
		NONE         = 0,
		MANUAL       = 1,
		LINEAR       = 2,
		DIFFRACTION  = 3,
		THONRINGS    = 4,
		NATURAL      = 5,
		HIGHCONTRAST = 6,
		WIDERANGE    = 7
	};

	enum class DataType
	{
		DEUndef   = -1,
		//DE1u    = 0,
		DE8u      = 1,
		//DE8uc   = 2,
		//DE8s    = 3,
		//DE8sc   = 4,
		DE16u     = 5,
		//DE16uc  = 6,
		DE16s     = 7,
		//DE16sc  = 8,
		//DE32u   = 9,
		//DE32uc  = 10,
		//DE32s   = 11,
		//DE32sc  = 12,
		DE32f     = 13,
		//DE32fc  = 14,
		//DE64u   = 15,
		//DE64uc  = 16,
		//DE64s   = 17,
		//DE64sc  = 18,
		//DE64f   = 19,
		//DE64fc  = 20
	};

	enum class MovieBufferStatus
	{
		UNKNOWN  = 0,
		FAILED   = 1,
		TIMEOUT  = 3,
		FINISHED = 4,
		OK       = 5
	};

	struct ImageAttributes
	{
		int                    centerX            = 0;                                        // In:  x coordinate of the center of the original image to pan to
		int                    centerY            = 0;			                              // In:  y coordinate of the center of the original image to pan to
		float                  zoom               = 1.0f;		                              // In:  zoom level on the returned image
		int                    windowWidth        = 0;			                              // In:  Width of the returned image in pixels
		int                    windowHeight       = 0;			                              // In:  Height of the returned image in pixels
		bool                   fft                = false;		                              // In:  request to return the FFT of the image
		bool                   linearStretch      = false;                                    //      Deprecated attribute
		ContrastStretchType    stretchType        = ContrastStretchType::LINEAR;              // In:  Contrast Stretch type. It is Linear by default TODO: enum
		float                  manualStretchMin   = 0;                                        // In:  Manual Stretch Minimum is adjusted by the user when the contrast stretch type is Manual.
		float                  manualStretchMax   = 0;                                        // In:  Manual Stretch Maximum is adjusted by the user when the contrast stretch type is Manual.
		float                  manualStretchGamma = 1.0f;                                     // In:  Manual Stretch Gamma is adjusted by the user when the contrast stretch type is Manual.
		float                  outlierPercentage  = 0.1F;                                     // In:  Percentage of outlier pixels at each end of the image histogram to ignore during contrast stretching if linearStretch is true.
		bool                   buffered           = false;		                              //      Deprecated attribute	
		int                    timeoutMsec        = -1;                                       // In:  Timeout in milliseconds for GetResult call.
		int                    frameWidth         = 0;			                              // Out: Width of the processed image in pixels before panning and zooming. This matches the size of the saved images.	
		int                    frameHeight        = 0;			                              // Out: Height of the processed image in pixels before panning and zooming. This matches the size of the saved images.	
		char	                datasetName[32]   = { 0 };		                              // Out: Data set name that the retrieved frame belongs to. Each repeat of acquisition has its own unique dataset name. e.g. 20210902_00081
		int                    acqIndex           = 0;			                              // Out: Zero-based acquisition index. Incremented after each repeat of acquisition, up to one less of the numberOfAcquisitions parameter of the StartAcquisition call
		bool                   acqFinished        = false;		                              // Out: Indicates whether the current acquisition is finished.
		int                    imageIndex         = 0;			                              // Out: Zero-based index within the acquisition that the retrieved frame belongs to
		int                    frameCount         = 0;			                              // Out: Number of frames summed up to make the returned image
		float                  imageMin           = 0;			                              // Out: Minimum pixel value of the retrieved image.
		float                  imageMax           = 0;			                              // Out: Maximum pixel value of the retrieved image.
		float                  imageMean          = 0;			                              // Out: Mean pixel values of the retrieved image.
		float                  imageStd           = 0;			                              // Out: Standard deviation of pixel values of the retrieved image.
		float                  eppix              = 0;			                              // Out: The mean total electrons per pixel calculated for the acquisition.
		float                  eps                = 0;			                              // Out: The mean total electrons per second over the entire readout area for the acquisition.
		float                  eppixps            = 0;			                              // Out: The mean electrons per pixel per second calculated for the acquisition.
		float                  epa2               = 0;			                              // Out: The mean total electrons per square Ångström calculated for the acquisition.
		float                  eppixpf            = 0;			                              // Out: The mean electrons per pixel per frame calculated for the acquisition.
		int                    underExposureRate  = 0;			                              //      Deprecated attribute
		int                    overExposureRate   = 0;                                        //      Deprecated attribute
		double                 timestamp          = 0;                                        // Out: Timestamp in seconds since the Unix epoch time when the frame arrived at the computer.
		float                  autoStretchMin     = 0;                                        // Out: Auto Stretch Minimum is returned by getAsyncOutput in server.
		float                  autoStretchMax     = 0;                                        // Out: Auto Stretch Maximum is returned by getAsyncOutput in server.
		float                  autoStretchGamma   = 1.0f;                                     // Out: Auto Stretch Gamma is returned by getAsyncOutput in server.
	};

	struct Histogram
	{
		float			min			= -1;		// In/Out: minimum histogram value. If -1, it'll be calculated
		float			max			= -1;		// In/Out: maximum histogram value. If -1, it'll be calculated
		int				bins		= 0;		// In:     number of bins requested, 0 meas histogram data is not requested
		int				bufferSize	= 0;		// In:     must be >= bins * 4
		unsigned int *  bufferPtr	= nullptr;	// In/Out: pointer to buffer to store histogram data
	};

	struct MovieBufferInfo
	{
		int headerBytes = 0;
		int imageBufferBytes = 0;
		int frameIndexStartPos = 0;
		int imageStartPos = 0;
		int imageW = 0;
		int imageH = 0;
		int framesInBuffer = 0;
		DataType imageDataType = DataType::DEUndef;
	};

	class DE_API Client
	{
	public:
		Client();
		~Client();

		// API 2.0: Supported by DE-Server 2.5 and up
		bool				             Connect(const char * host = "localhost", int port = 13240);
		bool				             Disconnect();
		const std::vector<std::string> * ListCameras();
		const char *		             GetCurrentCamera();
		bool				             SetCurrentCamera(const char * cameraName);
		const std::vector<std::string> * ListProperties();
		const char *                     GetProperty(const char * name);
		bool                             SetProperty(const char * name, const char * value);
		bool                             SetProperty(const char * name, int value);
		bool                             SetProperty(const char * name, float value);
		bool                             StartAcquisition(int acquisitionCount = 1, bool requestMovieBuffer = false);
		bool                             StopAcquisition();
		bool                             GetResult(void * imageBufferPtr, unsigned int imageBufferSize, FrameType frameType, PixelFormat * pixelFormat, ImageAttributes * imageAttributes = nullptr, Histogram * histogram = nullptr);
		int                              GetErrorCode();
		const char*                      GetErrorDescription();
		bool						     SetSWROI(int offsetX, int offsetY, int sizeX, int sizeY);
		bool						     SetHWROI(int offsetX, int offsetY, int sizeX, int sizeY);
		bool							 GetMovieBufferInfo(MovieBufferInfo * movieBufferInfo);
		MovieBufferStatus                GetMovieBuffer(void * movieBuffer, int movieBufferSize, int * numFrames, int timeoutMsec = 5000);
		void                             SetTimeout(int timeoutMsec); // Not implemented yet

		// API 1.0: Supported  by DE-Server 1.0 and up. Deprecated in DE-Server 2.5
		bool        connect(const char* ip, int rPort, int wPort);
		bool        close();
		bool        isConnected();
		bool        setCameraName(const char * name);
		bool        setProperty(const char * prop, const char * val);
		bool        getImage(void * image, unsigned int bytes);
		bool        getCameraNames(std::vector<std::string> & cameras);
		bool        getProperties(std::vector<std::string> & properties);
		bool        getTypedProperty(std::string label, float & val);
		bool        getTypedProperty(std::string label, int & val);
		bool        getProperty(std::string label, std::string & val);
		bool        abortAcquisition();
		bool        setLiveMode(bool enable);
		bool        getIsInLiveMode();
		int         getLastErrorCode();
		std::string getLastErrorDescription();
	};
}
#endif // DE_H
