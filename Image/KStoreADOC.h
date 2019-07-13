#pragma once
#include "kimagestore.h"
#include "..\EMimageExtra.h"

#define ADOC_MODE "DataMode"
#define ADOC_GLOBAL "PreData"
#define ADOC_SIZE "ImageSize"
#define ADOC_ISMONT "Montage"
#define ADOC_IMAGE "Image"
#define ADOC_SERIES "ImageSeries"
#define ADOC_SINGLE "ImageFile"
#define ADOC_TILT "TiltAngle"
#define ADOC_PCOORD "PieceCoordinates"
#define ADOC_STAGE "StagePosition"
#define ADOC_STAGEZ "StageZ"
#define ADOC_MAG "Magnification"
#define ADOC_INTENSITY "Intensity"
#define ADOC_SUPER "SuperMontCoords"
#define ADOC_PIXEL "PixelSpacing"
#define ADOC_TITLE "T"
#define ADOC_DOSE "ExposureDose"
#define ADOC_SPOT "SpotSize"
#define ADOC_DEFOCUS "Defocus"
#define ADOC_TARGET "TargetDefocus"
#define ADOC_IMAGESHIFT "ImageShift"
#define ADOC_AXIS "RotationAngle"
#define ADOC_EXPOSURE "ExposureTime"
#define ADOC_BINNING "Binning"
#define ADOC_CAMERA "CameraIndex"
#define ADOC_DIVBY2 "DividedBy2"
#define ADOC_READ_MODE "OperatingMode"
#define ADOC_CDS_MODE "UsingCDS"
#define ADOC_MAGIND "MagIndex"
#define ADOC_CONSET "LowDoseConSet"
#define ADOC_ZVALUE "ZValue"
#define ADOC_COUNT_ELEC "CountsPerElectron"
#define ADOC_MINMAXMEAN "MinMaxMean"
#define ADOC_PRIOR_DOSE "PriorRecordDose"
#define ADOC_XEDGE "XedgeDxy"
#define ADOC_YEDGE "YedgeDxy"
#define ADOC_XEDGEVS "XedgeDxyVS"
#define ADOC_YEDGEVS "YedgeDxyVS"
#define ADOC_ALI_COORD "AlignedPieceCoords"
#define ADOC_ALI_COORDVS "AlignedPieceCoordsVS"
#define ADOC_STAGEOFF "StageOffsets"
#define ADOC_FRAME_PATH "SubFramePath"
#define ADOC_NUM_FRAMES "NumSubFrames"
#define ADOC_DOSES_COUNTS "FrameDosesAndNumber"
#define ADOC_DATE_TIME "DateTime"
#define ADOC_CHAN_NAME "ChannelName"
#define ADOC_SLIT_LOSS "FilterSlitAndLoss"
#define ADOC_DE12_VERSION "DE12-ServerSoftwareVersion"
#define ADOC_DE12_PREEXPOSE "DE12-PreexposureTime(s)"
#define ADOC_DE12_TOTAL_FRAMES "DE12-TotalNumberOfFrames"
#define ADOC_DE12_FPS "DE12-FramesPerSecond"
#define ADOC_CAM_POSITION "DE12-CameraPosition"
#define ADOC_COVER_MODE "DE12-ProtectionCoverMode"
#define ADOC_COVER_DELAY "DE12-ProtectionCoverOpenDelay(ms)"
#define ADOC_TEMPERATURE "DE12-TemperatureDetector(C)"
#define ADOC_FARADAY "DE12-FaradayPlatePeakReading(pA/cm2)"
#define ADOC_SENSOR_SERIAL "DE12-SensorModuleSerialNumber"
#define ADOC_READOUT_DELAY "DE12-SensorReadoutDelay(ms)"
#define ADOC_IGNORED_FRAMES "DE12-IgnoredFramesInSummedImage"
#define ADOC_MONT_SECT "MontSection"
#define ADOC_BUF_ISXY "BufISXY"
#define ADOC_PROBEMODE "ProbeMode"
#define ADOC_STAGE_MONT "MoveStage"
#define ADOC_CONSET_USED "ConSetUsed"
#define ADOC_MONT_BACKLASH "MontBacklash"
#define ADOC_VALID_BACKLASH "ValidBacklash"
#define ADOC_DRIFT "DriftSettling"
#define ADOC_CAM_MODES  "CameraModes"
#define ADOC_FOCUS_OFFSET "FocusOffset"
#define ADOC_NET_VIEW_SHIFT "NetViewShifts"
#define ADOC_VIEW_BEAM_SHIFT "ViewBeamShifts"
#define ADOC_VIEW_BEAM_TILT "ViewBeamTilts"
#define ADOC_ALPHA "Alpha"
#define ADOC_FILTER "FilterState"
#define ADOC_VOLTAGE "Voltage"
#define ADOC_NAV_LABEL "NavigatorLabel"
#define ADOC_MULTI_POS "MultishotHoleAndPosition"

class KStoreADOC :
  public KImageStore
{
public:
  KStoreADOC(CString filename);
  KStoreADOC(CString filename, FileOptions inFileOpt);
  ~KStoreADOC(void);
  virtual int     AppendImage(KImage *inImage);
	virtual int     WriteSection(KImage * inImage, int inSect);
  virtual	void    SetPixelSpacing(float pixel);
  virtual int     AddTitle(const char *inTitle);
  virtual int     CheckMontage(MontParam *inParam); 
  virtual int     getPcoord(int inSect, int &outX, int &outY, int &outZ);
  virtual int     getStageCoord(int inSect, double &outX, double &outY);
  virtual KImage  *getRect(void);
	virtual BOOL    FileOK() { return (mAdocIndex >= 0); };
	static  CString IsADOC(CString filename);
  virtual int     WriteAdoc();
  static  int     SetValuesFromExtra(KImage *inImage, char *sectName, int index);
  static  int     LoadExtraFromValues(KImage *inImage, int &typext, char *sectName, 
    int index);
  static  int     LoadExtraFromValues(EMimageExtra *extra, int &typext, char *sectName, 
    int index);
  static  void    AllDone();

};
