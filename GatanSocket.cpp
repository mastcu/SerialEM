// GatanSocket.cpp - alternate pathway for contacting DMcamera plugin via socket

#include "stdafx.h"
#include "SerialEM.h"
#include <winsock.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "GatanSocket.h"
#include "Shared\SEMCCDDefines.h"

static int sSind = 0;
static int sInitialized = false;

CGatanSocket::CGatanSocket(void) : CBaseSocket()
{
  SEMBuildTime(__DATE__, __TIME__);
}

CGatanSocket::~CGatanSocket(void)
{
}


// Initialize: start up Winsock and try to open the socket to the server
int CGatanSocket::Initialize(void)
{
  if (sInitialized)
    return 0;

  if (InitializeSocketByID(GATAN_SOCK_ID, &sSind, 0, "Gatan socket server"))
    return 1;

  mHandshakeCode[sSind] = GS_ChunkHandshake;
  sInitialized = true;

  return OpenServerSocket(sSind);
}



/////////////////////////////////////
//   THE Function Calls

// ExecuteScript
int CGatanSocket::ExecuteScript(long size, long script[], long selectCamera,
                               double *retval)
{
  InitializePacking(sSind, GS_ExecuteScript);
  LONG_ARG(sSind, size);
  mLongArray[sSind] = script;
  BOOL_ARG(sSind, selectCamera);
  mNumDblRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *retval = mDoubleArgs[sSind][0];
  return mLongArgs[sSind][0];
}

// SetDebugMode
int CGatanSocket::SetDebugMode(long debug)
{
  return SendOneArgReturnRetVal(sSind, GS_SetDebugMode, debug);
}

// SetDMVersion
int CGatanSocket::SetDMVersion(long version)
{
  return SendOneArgReturnRetVal(sSind, GS_SetDMVersion, version);
}

// SetCurrentCamera
int CGatanSocket::SetCurrentCamera(long camera)
{
  return SendOneArgReturnRetVal(sSind, GS_SetCurrentCamera, camera);
}

// QueueScript
int CGatanSocket::QueueScript(long size, long script[])
{
  InitializePacking(sSind, GS_QueueScript);
  LONG_ARG(sSind, size);
  mLongArray[sSind] = script;
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// GetAcquiredImage
int CGatanSocket::GetAcquiredImage(short array[], long *arrSize, long *width, 
                                  long *height, long processing, double exposure,
                                  long binning, long top, long left, long bottom, 
                                  long right, long shutter, double settling, 
                                  long shutterDelay, long divideBy2, long corrections)
{
  InitializePacking(sSind, GS_GetAcquiredImage);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  LONG_ARG(sSind, processing);
  LONG_ARG(sSind, binning);
  LONG_ARG(sSind, top);
  LONG_ARG(sSind, left);
  LONG_ARG(sSind, bottom);
  LONG_ARG(sSind, right);
  LONG_ARG(sSind, shutter);
  LONG_ARG(sSind, shutterDelay);
  LONG_ARG(sSind, divideBy2);
  LONG_ARG(sSind, corrections);
  DOUBLE_ARG(sSind, exposure);
  DOUBLE_ARG(sSind, settling);
  return SendAndReceiveForImage(sSind, array, arrSize, width, height, 2);
}

// GetDarkReference
int CGatanSocket::GetDarkReference(short array[], long *arrSize, long *width, long *height,
                                  double exposure, long binning, long top, long left,
                                  long bottom, long right, long shutter, double settling,
                                  long divideBy2, long corrections)
{
  InitializePacking(sSind, GS_GetDarkReference);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  LONG_ARG(sSind, binning);
  LONG_ARG(sSind, top);
  LONG_ARG(sSind, left);
  LONG_ARG(sSind, bottom);
  LONG_ARG(sSind, right);
  LONG_ARG(sSind, shutter);
  LONG_ARG(sSind, divideBy2);
  LONG_ARG(sSind, corrections);
  DOUBLE_ARG(sSind, exposure);
  DOUBLE_ARG(sSind, settling);
  return SendAndReceiveForImage(sSind, array, arrSize, width, height, 2);
}
 
// GetGainReference
int CGatanSocket::GetGainReference(float array[], long *arrSize, long *width, long *height,
                                  long binning)
{
  InitializePacking(sSind, GS_GetGainReference);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  LONG_ARG(sSind, binning);
  return SendAndReceiveForImage(sSind, (short *)array, arrSize, width, height, 4);
}

// selectCamera
int CGatanSocket::selectCamera(long camera)
{
  return SendOneArgReturnRetVal(sSind, GS_SelectCamera, camera);
}

// SetReadMode
int CGatanSocket::SetReadMode(long mode, double scaling)
{
  InitializePacking(sSind, GS_SetReadMode);
  LONG_ARG(sSind, mode);
  DOUBLE_ARG(sSind, scaling);
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// SetK2Parameters
int CGatanSocket::SetK2Parameters(long readMode, double scaling, long hardwareProc,
                                  long doseFrac, double frameTime, long alignFrames,
                                  long saveFrames, long filtSize, long filter[])
{
  InitializePacking(sSind, GS_SetK2Parameters);
  LONG_ARG(sSind, readMode);
  DOUBLE_ARG(sSind, scaling);
  LONG_ARG(sSind, hardwareProc);
  BOOL_ARG(sSind, doseFrac);
  DOUBLE_ARG(sSind, frameTime);
  BOOL_ARG(sSind, alignFrames);
  BOOL_ARG(sSind, saveFrames);
  LONG_ARG(sSind, filtSize);
  mLongArray[sSind] = filter;
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// SetK2Parameters2
int CGatanSocket::SetK2Parameters2(long readMode, double scaling, long hardwareProc,
                                   long doseFrac, double frameTime, long alignFrames,
                                   long saveFrames, long rotationFlip, long flags,
                                   double dummy1, double dummy2, double dummy3, 
                                   double dummy4, long filtSize, long filter[])
{
  InitializePacking(sSind, GS_SetK2Parameters2);
  LONG_ARG(sSind, readMode);
  DOUBLE_ARG(sSind, scaling);
  LONG_ARG(sSind, hardwareProc);
  BOOL_ARG(sSind, doseFrac);
  DOUBLE_ARG(sSind, frameTime);
  BOOL_ARG(sSind, alignFrames);
  BOOL_ARG(sSind, saveFrames);
  LONG_ARG(sSind, rotationFlip);
  LONG_ARG(sSind, flags);
  DOUBLE_ARG(sSind, dummy1);
  DOUBLE_ARG(sSind, dummy2);
  DOUBLE_ARG(sSind, dummy3);
  DOUBLE_ARG(sSind, dummy4);
  LONG_ARG(sSind, filtSize);
  mLongArray[sSind] = filter;
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// SetupFileSaving
int CGatanSocket::SetupFileSaving2(long rotationFlip, long filePerImage, double pixelSize,
                                   long flags, double dummy1, double dummy2,
                                   double dummy3, double dummy4, long nameSize, 
                                   long names[], long *error)
{
  InitializePacking(sSind, GS_SetupFileSaving2);
  LONG_ARG(sSind, rotationFlip);
  BOOL_ARG(sSind, filePerImage);
  DOUBLE_ARG(sSind, pixelSize);
  LONG_ARG(sSind, flags);
  DOUBLE_ARG(sSind, dummy1);
  DOUBLE_ARG(sSind, dummy2);
  DOUBLE_ARG(sSind, dummy3);
  DOUBLE_ARG(sSind, dummy4);
  LONG_ARG(sSind, nameSize);
  mLongArray[sSind] = names;
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *error = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// GetFileSaveResult
int CGatanSocket::GetFileSaveResult(long *numSaved, long *error)
{
  InitializePacking(sSind, GS_GetFileSaveResult);
  mNumLongRecv[sSind] = 2;
  SendAndReceiveArgs(sSind);
  *numSaved = mLongArgs[sSind][1];
  *error = mLongArgs[sSind][2];
  return mLongArgs[sSind][0];
}

// GetDefectList
int CGatanSocket::GetDefectList(short xyPairs[], long *arrSize, long *numPoint,
                                long *numTotal)
{
  InitializePacking(sSind, GS_GetDefectList);
  LONG_ARG(sSind, *arrSize);
  return SendAndReceiveForImage(sSind, xyPairs, arrSize, numPoint, numTotal, 2);
}

// IsGpuAvailable
int CGatanSocket::IsGpuAvailable(long gpuNum, long *available, double *gpuMemory)
{
  InitializePacking(sSind, GS_IsGpuAvailable);
  LONG_ARG(sSind, gpuNum);
  mNumLongRecv[sSind] = 1;
  mNumDblRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *available = mLongArgs[sSind][1];
  *gpuMemory = mDoubleArgs[sSind][0];
  return mLongArgs[sSind][0];
}

// SetupFrameAligning
int CGatanSocket::SetupFrameAligning(long aliBinning, double rad2Filt1, 
    double rad2Filt2, double rad2Filt3, double sigma2Ratio, 
    double truncLimit, long alignFlags, long gpuFlags, long numAllVsAll, long groupSize, 
    long shiftLimit, long antialiasType, long refineIter, double stopIterBelow, 
    double refRad2, long nSumAndGrab, long dumInt1, long dumInt2, double dumDbl1, 
    long stringSize, long *strings, long *error)
{
  InitializePacking(sSind, GS_SetupFrameAligning);
  LONG_ARG(sSind, aliBinning);
  DOUBLE_ARG(sSind, rad2Filt1); 
  DOUBLE_ARG(sSind, rad2Filt2); 
  DOUBLE_ARG(sSind, rad2Filt3); 
  DOUBLE_ARG(sSind, sigma2Ratio); 
  DOUBLE_ARG(sSind, truncLimit); 
  LONG_ARG(sSind, alignFlags); 
  LONG_ARG(sSind, gpuFlags); 
  LONG_ARG(sSind, numAllVsAll); 
  LONG_ARG(sSind, groupSize); 
  LONG_ARG(sSind, shiftLimit); 
  LONG_ARG(sSind, antialiasType); 
  LONG_ARG(sSind, refineIter); 
  DOUBLE_ARG(sSind, stopIterBelow); 
  DOUBLE_ARG(sSind, refRad2); 
  LONG_ARG(sSind, nSumAndGrab); 
  LONG_ARG(sSind, dumInt1); 
  LONG_ARG(sSind, dumInt2); 
  DOUBLE_ARG(sSind, dumDbl1); 
  LONG_ARG(sSind, stringSize);
  mLongArray[sSind] = strings;
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *error = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
 }

// FrameAlignResults
int CGatanSocket::FrameAlignResults(double *rawDist, double *smoothDist, 
    double *resMean, double *maxResMax, double *meanRawMax, double *maxRawMax, 
    long *crossHalf, long *crossQuarter, long *crossEighth, long *halfNyq, 
    long *dumInt1, double *dumDbl1, double *dumDbl2)
{
  InitializePacking(sSind, GS_FrameAlignResults);
  mNumLongRecv[sSind] = 5;
  mNumDblRecv[sSind] = 8;
  SendAndReceiveArgs(sSind);
  *rawDist = mDoubleArgs[sSind][0];
  *smoothDist = mDoubleArgs[sSind][1];
  *resMean = mDoubleArgs[sSind][2];
  *maxResMax = mDoubleArgs[sSind][3];
  *meanRawMax = mDoubleArgs[sSind][4];
  *maxRawMax = mDoubleArgs[sSind][5];
  *crossHalf = mLongArgs[sSind][1];
  *crossQuarter = mLongArgs[sSind][2];
  *crossEighth = mLongArgs[sSind][3];
  *halfNyq = mLongArgs[sSind][4];
  *dumInt1 = mLongArgs[sSind][5];
  *dumDbl1 = mDoubleArgs[sSind][6];
  *dumDbl2 = mDoubleArgs[sSind][7]; 
  return mLongArgs[sSind][0];
}

// MakeAlignComFile
int CGatanSocket::MakeAlignComFile(long flags, long dumInt1, double dumDbl1, 
  double dumDbl2, long stringSize, long *strings, long *error)
{
  InitializePacking(sSind, GS_MakeAlignComFile);
  LONG_ARG(sSind, flags);
  LONG_ARG(sSind, dumInt1);
  LONG_ARG(sSind, stringSize);
  DOUBLE_ARG(sSind, dumDbl1); 
  DOUBLE_ARG(sSind, dumDbl2); 
  mLongArray[sSind] = strings;
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *error = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];

}

// ReturnDeferredSum
int CGatanSocket::ReturnDeferredSum(short array[], long *arrSize, long *width, 
                               long *height)
{
  InitializePacking(sSind, GS_ReturnDeferredSum);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  return SendAndReceiveForImage(sSind, array, arrSize, width, height, 2);
}

// GetNumberOfCameras
int CGatanSocket::GetNumberOfCameras(long *numCameras)
{
  InitializePacking(sSind, GS_GetNumberOfCameras);
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *numCameras = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// IsCameraInserted
int CGatanSocket::IsCameraInserted(long camera, long *inserted)
{
  InitializePacking(sSind, GS_IsCameraInserted);
  LONG_ARG(sSind, camera);
  mNumBoolRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *inserted = mBoolArgs[sSind][0];
  return mLongArgs[sSind][0];
}

// InsertCamera
int CGatanSocket::InsertCamera(long camera, long state)
{
  InitializePacking(sSind, GS_InsertCamera);
  LONG_ARG(sSind, camera);
  BOOL_ARG(sSind, state);
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// GetDMVersion
int CGatanSocket::GetDMVersion(long *version)
{
  InitializePacking(sSind, GS_GetDMVersion);
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *version = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// GetPluginVersion
int CGatanSocket::GetPluginVersion(long *version)
{
  InitializePacking(sSind, GS_GetPluginVersion);
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *version = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// GetLastError
int CGatanSocket::GetLastError(long *error)
{
  InitializePacking(sSind, GS_GetLastError);
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *error = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// GetDMCapabilities
int CGatanSocket::GetDMCapabilities(long *canSelectShutter, long *canSetSettling,
                                   long *openShutterWorks)
{
  InitializePacking(sSind, GS_GetDMCapabilities);
  mNumBoolRecv[sSind] = 3;
  SendAndReceiveArgs(sSind);
  *canSelectShutter = mBoolArgs[sSind][0];
  *canSetSettling = mBoolArgs[sSind][1];
  *openShutterWorks = mBoolArgs[sSind][2];
  return mLongArgs[sSind][0];
}

// SetShutterNormallyClosed
int CGatanSocket::SetShutterNormallyClosed(long camera, long shutter)
{
  InitializePacking(sSind, GS_SetShutterNormallyClosed);
  LONG_ARG(sSind, camera);
  LONG_ARG(sSind, shutter);
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// SetNoDMSettling
int CGatanSocket::SetNoDMSettling(long camera)
{
  return SendOneArgReturnRetVal(sSind, GS_SetNoDMSettling, camera);
}

// GetDSProperties
int CGatanSocket::GetDSProperties(long timeout, double addedFlyback, double margin,
                                 double *flyback, double *lineFreq, 
                                 double *rotOffset, long *doFlip)
{
  InitializePacking(sSind, GS_GetDSProperties);
  LONG_ARG(sSind, timeout);
  DOUBLE_ARG(sSind, addedFlyback);
  DOUBLE_ARG(sSind, margin);
  mNumDblRecv[sSind] = 3;
  mNumLongRecv[sSind] = 1;
  SendAndReceiveArgs(sSind);
  *flyback = mDoubleArgs[sSind][0];
  *lineFreq = mDoubleArgs[sSind][1];
  *rotOffset = mDoubleArgs[sSind][2];
  *doFlip = mLongArgs[sSind][1];
  return mLongArgs[sSind][0];
}

// AcquireDSImage
int CGatanSocket::AcquireDSImage(short array[], long *arrSize, long *width, 
                                long *height, double rotation, double pixelTime, 
                                long lineSync, long continuous, long numChan, 
                                long channels[], long divideBy2)
{
  InitializePacking(sSind, GS_AcquireDSImage);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  LONG_ARG(sSind, lineSync);
  LONG_ARG(sSind, continuous);
  LONG_ARG(sSind, divideBy2);
  LONG_ARG(sSind, numChan);
  mLongArray[sSind] = channels;
  DOUBLE_ARG(sSind, rotation);
  DOUBLE_ARG(sSind, pixelTime);
  return SendAndReceiveForImage(sSind, array, arrSize, width, height, 2);
}

// ReturnDSChannel
int CGatanSocket::ReturnDSChannel(short array[], long *arrSize, long *width, 
                                 long *height, long channel, long divideBy2)
{
  InitializePacking(sSind, GS_ReturnDSChannel);
  LONG_ARG(sSind, *arrSize);
  LONG_ARG(sSind, *width);
  LONG_ARG(sSind, *height);
  LONG_ARG(sSind, channel);
  LONG_ARG(sSind, divideBy2);
  return SendAndReceiveForImage(sSind, array, arrSize, width, height, 2);
}

// StopDSAcquisition
int CGatanSocket::StopDSAcquisition()
{
  InitializePacking(sSind, GS_StopDSAcquisition);
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// StopContinuousCamera
int CGatanSocket::StopContinuousCamera()
{
  InitializePacking(sSind, GS_StopContinuousCamera);
  SendAndReceiveArgs(sSind);
  return mLongArgs[sSind][0];
}

// FreeK2GainReference
int CGatanSocket::FreeK2GainReference(long which)
{
  return SendOneArgReturnRetVal(sSind, GS_FreeK2GainReference, which);
}

// CheckReferenceTime
int CGatanSocket::CheckReferenceTime(const char *filename, __time64_t *mtime)
{
  InitializePacking(sSind, GS_CheckReferenceTime);
  LONG_ARG(sSind, (int)strlen(filename) / 4 + 1);
  mLongArray[sSind] = (long *)filename;
  mNumLongRecv[sSind] = 2;
  SendAndReceiveArgs(sSind);
  if (!mLongArgs[sSind][0]) {
    memcpy(mtime, &mLongArgs[sSind][1], 2 * sizeof(long));
    SEMTrace('r', "%s has time %s\r\n", filename, _ctime64(mtime));
  }
  return mLongArgs[sSind][0];
}

