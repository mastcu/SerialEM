#pragma once
#include "kimagestore.h"
#include "..\EMimageExtra.h"

#define MDOC_FLOAT(nam, ini, tst, sym, str) extern const char *sym;
#define MDOC_INTEGER(nam, ini, sym, str) extern const char *sym;
#define MDOC_TWO_FLOATS(nam1, nam2, ini, tst, sym, str) extern const char *sym;
#define MDOC_TWO_INTS(nam1, nam2, ini, sym, str) extern const char *sym;
#define MDOC_STRING(nam, sym, str) extern const char *sym;

namespace AdocDefs {
#include "MdocDefines.h"
}
#undef MDOC_FLOAT
#undef MDOC_TWO_FLOATS
#undef MDOC_STRING
#undef MDOC_INTEGER
#undef MDOC_TWO_INTS

using namespace AdocDefs;

#define ADOC_MODE "DataMode"
#define ADOC_GLOBAL "PreData"
#define ADOC_SIZE "ImageSize"
#define ADOC_ISMONT "Montage"
#define ADOC_IMAGE "Image"
#define ADOC_SERIES "ImageSeries"
#define ADOC_SINGLE "ImageFile"
#define ADOC_PCOORD "PieceCoordinates"
#define ADOC_SUPER "SuperMontCoords"
#define ADOC_TITLE "T"
#define ADOC_ZVALUE "ZValue"
#define ADOC_MINMAXMEAN "MinMaxMean"
#define ADOC_XEDGE "XedgeDxy"
#define ADOC_YEDGE "YedgeDxy"
#define ADOC_XEDGEVS "XedgeDxyVS"
#define ADOC_YEDGEVS "YedgeDxyVS"
#define ADOC_ALI_COORD "AlignedPieceCoords"
#define ADOC_ALI_COORDVS "AlignedPieceCoordsVS"
#define ADOC_STAGEOFF "StageOffsets"
#define ADOC_ADJ_PIXEL "AdjustedPixelSpacing"
#define ADOC_ADJ_OVERLAPS "AdjustedOverlaps"
#define ADOC_ADJ_ROTATION "AdjustedRotation"
#define ADOC_MONT_SECT "MontSection"
#define ADOC_BUF_ISXY "BufISXY"
#define ADOC_PROBEMODE "ProbeMode"
#define ADOC_STAGE_MONT "MoveStage"
#define ADOC_CONSET_USED "ConSetUsed"
#define ADOC_MONT_BACKLASH "MontBacklash"
#define ADOC_VALID_BACKLASH "ValidBacklash"
#define ADOC_MONT_SIZE "FullMontSize"
#define ADOC_MONT_FRAMES "FullMontNumFrames"
#define ADOC_FIT_POLY_ID "FitToPolyID"
#define ADOC_DRIFT "DriftSettling"
#define ADOC_CAM_MODES  "CameraModes"
#define ADOC_FOCUS_OFFSET "FocusOffset"
#define ADOC_NET_VIEW_SHIFT "NetViewShifts"
#define ADOC_VIEW_BEAM_SHIFT "ViewBeamShifts"
#define ADOC_VIEW_BEAM_TILT "ViewBeamTilts"
#define ADOC_VIEW_DEFOCUS "ViewDefocus"
#define ADOC_ALPHA "Alpha"
#define ADOC_FILTER "FilterState"
#define ADOC_FRAMETS_ST_END "FrameTSStartEndFrames"

class KStoreADOC :
  public KImageStore
{
public:
  KStoreADOC(CString filename);
  KStoreADOC(CString filename, FileOptions inFileOpt);
  ~KStoreADOC(void);
  virtual int     AppendImage(KImage *inImage);
	virtual int     WriteSection(KImage * inImage, int inSect);
  virtual int     AddTitle(const char *inTitle);
  virtual int     setMode(int inMode);
  virtual int     CheckMontage(MontParam *inParam);
  virtual int     getPcoord(int inSect, int &outX, int &outY, int &outZ, bool gotMutex = false);
  virtual int     getStageCoord(int inSect, double &outX, double &outY);
  virtual int     ReorderPieceZCoords(int *sectOrder);
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
