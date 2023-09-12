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

#ifdef DE_EXPORTS
#	define DE_API __declspec(dllexport)
#else
#	define DE_API __declspec(dllimport)
#endif

namespace DE
{
	enum class FrameType
	{
        SINGLE_FRAME_INTEGRATED = 5,  // integrated frame
        SINGLE_FRAME_COUNTED    = 6,  // single counted frame
        MOVIE_INTEGRATED        = 7,  // fractionated integrated frame
        MOVIE_COUNTED           = 8,  // fractionated counted frame
        TOTAL_SUM_INTEGRATED    = 9,  // sum of all integrated frame
        TOTAL_SUM_COUNTED       = 10, // sum of all counted frame
        CUMULATIVE_INTEGRATED   = 11, // cumulative integrted frame
        CUMULATIVE_COUNTED      = 12, // cumulative counted frame
        VIRTUALIMAGE1           = 13, // virtual image built with mask 1
        VIRTUALIMAGE2           = 14, // virtual image built with mask 2
        VIRTUALIMAGE3           = 15, // virtual image built with mask 3
        VIRTUALIMAGE4           = 16, // virtual image built with mask 4
        NONE					= 17  // For returning without an image or stats and without waiting for frames to be written to the disk when acquisition is completed 
	};

	enum class PixelFormat
	{
		UINT8   = 1,
		UINT16  = 5,
		FLOAT32 = 13,
		AUTO    = -1
	};

	struct ImageAttributes
	{
		int		centerX				= 0;		// In:  x coordinate of the center of the original image to pan to
		int		centerY				= 0;		// In:  y coordinate of the center of the original image to pan to
		float	zoom				= 1.0f;		// In:  zoom level on the returned image
		int		windowWidth			= 0;		// In:  Width of the returned image in pixels
		int		windowHeight		= 0;		// In:  Height of the returned image in pixels
		bool	fft					= false;	// In:  request to return the FFT of the image
		bool	linearStretch		= true;		// In:  linearly stretch the contrast of pixel values to cover the output pixel value range 
		float	outlierPercentage	= 2.0f;		// In:  Percentage of outlier pixels at each end of the image histogram to ignore during contrast stretching if linearStretch is true.
		bool    buffered            = false;    // In:
		int		frameWidth			= 0;		// Out: Width of the processed image in pixels before panning and zooming. This matches the size of the saved images.
		int		frameHeight			= 0;		// Out: Height of the processed image in pixels before panning and zooming. This matches the size of the saved images.
		char	datasetName[32]		= {0};		// Out: Data set name that the retrieved frame belongs to. Each repeat of acquisition has its own unique dataset name. e.g. 20210902_00081
		int		acqIndex			= 0;		// Out: Zero-based acquisition index. Incremented after each repeat of acquisition, up to one less of the numberOfAcquisitions parameter of the StartAcquisition call
		bool	acqFinished			= false;	// Out: Indicates whether the current acquisition is finished.
		int		imageIndex			= 0;		// Out: Zero-based index within the acquisition that the retrieved frame belongs to
		int		frameCount			= 0;		// Out: Number of frames summed up to make the returned image
		float	imageMin			= 0;		// Out: Minimum pixel value of the retrieved image.
		float	imageMax			= 0;		// Out: Maximum pixel value of the retrieved image.
		float	imageMean			= 0;		// Out: Mean pixel values of the retrieved image.
		float	imageStd			= 0;		// Out: Standard deviation of pixel values of the retrieved image.
		float	eppix				= 0;		// Out: The mean total electrons per pixel calculated for the acquisition.
		float	eps					= 0;		// Out: The mean total electrons per second over the entire readout area for the acquisition.
		float	eppixps				= 0;		// Out: The mean electrons per pixel per second calculated for the acquisition.
		float	epa2				= 0;		// Out: The mean total electrons per square Ångström calculated for the acquisition.
		float	eppixpf				= 0;		// Out: The mean electrons per pixel per frame calculated for the acquisition.
		int		underExposureRate	= 0;		// Out: Percentage of pixels under the Statistics - Underexposure Mark (percentage) property value.
		int		overExposureRate	= 0;		// Out: Percentage of pixels over the Statistics - Overexposure Mark (percentage) property value.
		double  timestamp           = 0;        // Out: Timestamp in seconds since the Unix epoch time when the frame arrived at the computer.
	};

	struct Histogram
	{
		float			min			= -1;		// In/Out: minimum histogram value. If -1, it'll be calculated
		float			max			= -1;		// In/Out: maximum histogram value. If -1, it'll be calculated
		int				bins		= 0;		// In:     number of bins requested, 0 meas histogram data is not requested
		int				bufferSize	= 0;		// In:     must be >= bins * 4
		unsigned int *  bufferPtr	= nullptr;	// In/Out: pointer to buffer to store histogram data
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
		bool                             StartAcquisition(int acquisitionCount);
		bool                             StopAcquisition();
		bool                             GetResult(void * imageBufferPtr, unsigned int imageBufferSize, FrameType frameType, PixelFormat * pixelFormat, ImageAttributes * imageAttributes = nullptr, Histogram * histogram = nullptr);

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
		bool        setLiveMode(bool enable) { return false; }
		bool        getIsInLiveMode() { return false; }
		int         getLastErrorCode();
		std::string getLastErrorDescription();
	};
}
#endif // DE_H
