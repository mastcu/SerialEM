#pragma once
#include "BaseSocket.h"

class CGatanSocket : public CBaseSocket
{
public:
  CGatanSocket(void);
  ~CGatanSocket(void);

private:


public:
  int Initialize(void);
  // 6 BOOL arguments in the plugin code turned into longs in the *.tl[ih]
  static int ExecuteScript(long size, long script[], long selectCamera,
                           double *retval);
  static int SetDebugMode(long debug);
  static int SetDMVersion(long version);
  static int SetCurrentCamera(long camera);
  static int QueueScript(long size, long script[]);
  static int GetAcquiredImage(short array[], long *arrSize, long *width, 
                              long *height, long processing, double exposure,
                              long binning, long top, long left, long bottom, 
                              long right, long shutter, double settling, 
                              long shutterDelay, long divideBy2, long corrections);
  static int GetDarkReference(short array[], long *arrSize, long *width, 
                              long *height, double exposure,
                              long binning, long top, long left, long bottom, 
                              long right, long shutter, double settling, long divideBy2, long corrections); 
  static int GetGainReference(float array[], long *arrSize, long *width, long *height, long binning);

  // Yes this is SelectCamera in the plugin code but the *.tl[ih] show it as selectCamera
  static int selectCamera(long camera);
  static int SetReadMode(long mode, double scaling);
  static int SetK2Parameters(long readMode, double scaling, long hardwareProc,
                             long doseFrac, double frameTime, long alignFrames,
                             long saveFrames, long filtSize, long filter[]);
  static int SetK2Parameters2(long readMode, double scaling, long hardwareProc,
                              long doseFrac, double frameTime, long alignFrames,
                              long saveFrames, long rotationFlip, long flags,
                              double dummy1, double dummy2, double dummy3, 
                              double dummy4, long filtSize, long filter[]);
  static int SetupFileSaving2(long rotationFlip, long filePerImage, double pixelSize,
    long flags, double numGrabSum, double frameThresh, double threshFracPlus,
                             double dummy4, long nameSize, long names[], long *error);
  static int GetFileSaveResult(long *numSaved, long *error);
  static int GetDefectList(short xyPairs[], long *arrSize, long *numPoint, long *numTotal);
  static int IsGpuAvailable(long gpuNum, long *available, double *gpuMemory);
  static int SetupFrameAligning(long aliBinning, double rad2Filt1, 
    double rad2Filt2, double rad2Filt3, double sigma2Ratio, 
    double truncLimit, long alignFlags, long gpuFlags, long numAllVsAll, long groupSize, 
    long shiftLimit, long antialiasType, long refineIter, double stopIterBelow, 
    double refRad2, long nSumAndGrab, long frameStartEnd, long frameThreshes, 
    double dumDbl1, long stringSize, long *strings, long *error);
  static int FrameAlignResults(double *rawDist, double *smoothDist, 
    double *resMean, double *maxResMax, double *meanRawMax, double *maxRawMax, 
    long *crossHalf, long *crossQuarter, long *crossEighth, long *halfNyq, 
    long *dumInt1, double *dumDbl1, double *dumDbl2);
  static int MakeAlignComFile(long flags, long dumInt1, double dumDbl1, 
    double dumDbl2, long stringSize, long *strings, long *error);
  static int ReturnDeferredSum(short array[], long *arrSize, long *width, long *height);
  static int GetTiltSumProperties(long *index, long *numFrames, double *angle,
    long *firstSlice, long *lastSlice, long *dumInt1, double *dumDbl1, double *dumDbl2);
  static int GetNumberOfCameras(long *numCameras);
  static int IsCameraInserted(long camera, long *inserted);
  static int InsertCamera(long camera, long state);
  static int GetDMVersion(long *version);
  static int GetDMVersionAndBuild(long *version, long *build);
  static int GetPluginVersion(long *version);
  static int GetLastError(long *error);
  static int GetDMCapabilities(long *canSelectShutter, long *canSetSettling,
                               long *openShutterWorks);
  static int SetShutterNormallyClosed(long camera, long shutter);
  static int SetNoDMSettling(long camera);
  static int GetDSProperties(long timeout, double addedFlyback, double margin,
                             double *flyback, double *lineFreq, 
                             double *rotOffset, long *doFlip);
  static int AcquireDSImage(short array[], long *arrSize, long *width, 
                            long *height, double rotation, double pixelTime, 
                            long lineSync, long continuous, long numChan, 
                            long channels[], long divideBy2);
  static int ReturnDSChannel(short array[], long *arrSize, long *width, 
                             long *height, long channel, long divideBy2);
  static int StopDSAcquisition();
  static int StopContinuousCamera();
  static int FreeK2GainReference(long which);
  static int WaitUntilReady(long which);
  static int GetLastDoseRate(double *dose);
  static int SaveFrameMdoc(int strSize, long buffer[], long flags);
  static int CheckReferenceTime(const char *filename, __time64_t *mtime);
};
