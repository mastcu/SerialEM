#ifndef SHARED_JEOL_DEFINES
#define SHARED_JEOL_DEFINES

#define JUPD_MAG          (1)
#define JUPD_STAGE_POS    (1 << 1)
#define JUPD_STAGE_STAT   (1 << 2)
#define JUPD_SCREEN       (1 << 3)
#define JUPD_SMALL_SCREEN (1 << 4)
#define JUPD_CURRENT      (1 << 5)
#define JUPD_INTENSITY    (1 << 6)
#define JUPD_SPOT         (1 << 7)
#define JUPD_IS           (1 << 8)
#define JUPD_FOCUS        (1 << 9)
#define JUPD_ENERGY       (1 << 10)
#define JUPD_ENERGY_ON    (1 << 11)
#define JUPD_SLIT_IN      (1 << 12)
#define JUPD_SLIT_WIDTH   (1 << 13)
#define JUPD_SPECTRUM     (1 << 14)
#define JUPD_ALPHA        (1 << 15)
#define JUPD_DET_INSERTED (1 << 16)
#define JUPD_DET_SELECTED (1 << 17)
#define JUPD_BEAM_STATE   (1 << 18)
#define JUPD_GIF_MODE     (1 << 19)

#define IS_RING_MAX  20
#define MAX_DETECTORS 8

#define JEOL_MAG1_MODE  0
#define JEOL_LOWMAG_MODE  2
#define JEOL_SAMAG_MODE 3
#define JEOL_DIFF_MODE  4
#define JEOL_STEM_AMAGMODE 3
#define JEOL_STEM_MAGMODE 2
#define JEOL_STEM_LOWMAG 1
#define JEOL_STEM_DIFF  4
enum {JEOL_MDS_OFF = 0, JEOL_MDS_SEARCH, JEOL_MDS_FOCUS, JEOL_MDS_PHOTOSET};

#define JEOL_HAS_NITROGEN_CLASS (1)
#define JEOL_HAS_EXTRA_APERTURES  (1 << 1)
#define JEOL_SEQUENTIAL_RELAX    (1 << 2)
#define JEOL_CL3_FOCUS_LM_STEM   (1 << 3)
#define JEOL_SET_CL3_LMSTEM_FOCUS (1 << 4)

// Standard conversions from signed real to nearest integer for JEOL calls
#define NINT8000(a) (long)floor((a) + 0x8000 + 0.5)
#define LONGNINT(a) (long)floor((a) + 0.5)

struct JeolStateData {
  double tiltAngle;  // x-axis tilt, in degrees
  int stageStatus;
  int magIndex;
  int magValue;
  int magMode;
  int camLenIndex;
  double camLenValue;   // Converted to meters
  int spotSize;
  int alpha;
  int objective;
  int objectiveFine;
  int objectiveMini;
  double defocus;
  double current;
  double rawIntensity;
  double stageX, stageY, stageZ;   // microns
  double ISX, ISY;
  int screenPos;
  int smallScreen;
  short int detectorIDs[MAX_DETECTORS];
  short int detInserted[MAX_DETECTORS];
  short int detSelected[MAX_DETECTORS];
  int numDetectors;
  double energyShift;
  float slitWidth;
  BOOL shiftOn;
  BOOL slitIn;
  BOOL spectroscopy;
  int JeolEFTEM;
  BOOL JeolSTEM;
  BOOL STEMchanging;
  BOOL suspendUpdate;
  BOOL magInvalid;
  BOOL comReady;

  BOOL bDataIsGood;
  BOOL eventDataIsGood;
  int updateSleep;
  int terminate;
  int lowSequence;
  int highSequence;
  int lowHighCycle;
  int lowHigh;
  int highLowRatio;
  int doneHigh;
  int doneLow;
  int numHigh;
  int numLow;
  int numDo;
  int numSequence;
  BOOL doLower;
  int lowerSequence;
  unsigned int lowerFlags;
  unsigned int lowFlags;
  unsigned int highFlags;
  unsigned int lowFlagsNew;
  unsigned int highFlagsNew;
  BOOL updateByEvent;
  BOOL setUpdateByEvent;
  double CLA1_to_um;
  double ringISX[IS_RING_MAX], ringISY[IS_RING_MAX];
  double ringMagTime[IS_RING_MAX], ringISTime[IS_RING_MAX];
  int ringMag[IS_RING_MAX];
  int ringCrossInd[IS_RING_MAX];
  int magRingIndex;
  int ISRingIndex;
  int postMagStageDelay;
  BOOL blockStage;
  double stageTimeout;
  BOOL usePLforIS;
  BOOL useCLA2forSTEM;
  BOOL doMiniInLowMag;
  int lowestNonLMmag;
  int lowestSTEMnonLMmag;
  int mainDetectorID;
  BOOL Jeol1230;
  double internalMagTime;
  int skipMagEventTime;
  double internalIStime;
  int skipISeventTime;
  long baseFocus;
  long miniBaseFocus;
  long fineBaseFocus;
  long coarseBaseFocus;
  double changedMagTime;
  int changedMagInd;
  int valveOrFilament;
  int pairedDetectorID;
  BOOL relaxingLenses;
  unsigned int relaxStartTime;
  unsigned int relaxEndTime;
  double rampupStartTime;
  long StemCL3BaseFocus;
  int CL3;
  // Used to keep track if actual structure is smaller than what was built against
  int lastMember; 
};

struct JeolParams {
  BOOL hasOmegaFilter;
  BOOL STEMunitsX;
  BOOL scanningMags;
  int indForMagMode;
  int secondaryModeInd;
  int lowestSecondaryMag;
  double CLA1_to_um;
  BOOL noColumnValve;
  float stageRounding;
  BOOL reportsLargeScreen;
  BOOL screenByEvent;
  BOOL scopeHasSTEM;
  BOOL simulationMode;
  int postMagDelay;
  double STEMdefocusFac;
  double OLfine_to_um;
  double OM_to_um;
  int STEMrotation;
  int initializeJeolDelay;
  int magEventWait;
  int useGIFmodeCalls;
  int flags;
  int flashFegTimeout;
  int fillNitrogenTimeout;
  int emissionTimeout;
  int beamRampupTimeout;
  int gaugeSwitchDelay;
  float StemLMCL3ToUm;
  // Used to keep track if actual structure is smaller than what was built against
  int lastMember; 
};

#endif
