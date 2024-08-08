// A GENERAL CONTAINER FOR PARAMETER STRUCTURES

#ifndef CONTROLSET_H
#define CONTROLSET_H
#include "Shared\CorrectDefects.h"
typedef std::vector <float> FloatVec;

#define UNPROCESSED 0
#define DARK_SUBTRACTED 1
#define GAIN_NORMALIZED 2
#define SINGLE_FRAME 0
#define CONTINUOUS 1
#define USE_BEAM_BLANK 0
#define USE_FILM_SHUTTER 1
#define USE_DUAL_SHUTTER 2
#define FRAMEALI_PAIRWISE_NUM  0
#define FRAMEALI_HALF_PAIRWISE  1
#define FRAMEALI_ALL_PAIRWISE  2
#define FRAMEALI_ACCUM_REF  3
#define FRAMES_CAN_BE_SAVED 1
#define FRAMES_CAN_BE_ALIGNED 2

#define MAX_BINNINGS 10
#define MAX_HOT_COLUMNS 200
#define MAX_HOT_PIXELS 400
#define MAX_IMPORT_XFORMS 16
#define MAX_STEM_CHANNELS 8
#define DATETIME_SIZE 21
#define K2_LINEAR_MODE 0
#define K2_COUNTING_MODE 1
#define K2_SUPERRES_MODE 2
#define DE_SAVE_FRAMES   0x1
#define DE_SAVE_SUMS     0x2
#define DE_SAVE_FINAL    0x4
#define DE_SAVE_COUNTING 0x8
#define DE_SAVE_MASTER   0x10
#define DE_SAVE_SINGLE   0x20
#define MAX_SPOT_SIZE  13
#define MAX_CAMLENS 60

struct ControlSet {
  int mode;
  int processing;
  int forceDark;
  int onceDark;
  int shuttering;
  int binning;
  float exposure;
  float drift;
  int left;
  int top;
  int right;
  int bottom;
  int numAverage;
  int averageDark;
  int averageOnce;
  int removeXrays;
  int channelIndex[MAX_STEM_CHANNELS];
  int lineSync;       // For line sync in DigiScan
  int dynamicFocus;
  int boostMag;       // For boostin mag in STEM focus AND for DE hardware binning
  int magAllShots;    // For choice to boost mag on all shots AND for DE hardware ROI
  int integration;
  int correctDrift;   // Flag to use internal drift correction of camera
  int K2ReadMode;     // Linear/counting/super-res for K2, DE, and Falcon; diff for 1View
  int doseFrac;
  float frameTime;
  int alignFrames;
  int useFrameAlign;  // 1 to align in SerialEM[CCD], 2 to write alignframes pcm file
  int faParamSetInd;
  int saveFrames;
  int filterType;
  int sumK2Frames;    // Flag that K2 summing is active; AND DE counting mode dose frac #
  ShortVec summedFrameList;
  FloatVec userFrameFractions;
  FloatVec userSubframeFractions;
  int numSkipBefore;  // Also used for prepixel time in microseconds in Tietz STEM
  int numSkipAfter;
  int DEsumCount;     // DE sum count only for linear mode
};

struct CameraParameters {
  int sizeX;
  int sizeY;
  BOOL unsignedImages;     // For a 16-bit camera
  BOOL returnsFloats;      // For a plugin that does or can return float images
  int moduloX;             // readout must be multiple of these
  int moduloY;             // or negative means restricted sizes
  BOOL coordsModulo;       // Coordinates must be modulo of values
  BOOL squareSubareas;     // Flag that subareas must be square
  int refSizeEvenX;        // Flags that reference size must be even in X or Y
  int refSizeEvenY;        // 1 to make them even, 2 to also expect even from DM
  int halfSizeX[MAX_BINNINGS];   // Actual half and quarter sizes for FEI cameras
  int halfSizeY[MAX_BINNINGS];
  int quarterSizeX[MAX_BINNINGS];
  int quarterSizeY[MAX_BINNINGS];
  BOOL fourPort;           // Four-port readout camera
  int leftOffset;          // Offsets to add to the final coordinates sent to camera
  int topOffset;    
  int TietzType;           // Number of Tietz camera type if not 0
  BOOL failedToInitialize; // Flag that Tietz camera failed to initialize
  int AMTtype;             // Flag for AMT camera
  int FEItype;             // Type of FEI camera
  float ReadoutInterval;   // Camera-specific readout interval to cover different Falcons
  unsigned int CamFlags;   // Flags for FEI camera in advanced scripting
  int DE_camType;     	   // Flag to determine DE camera type	
  int DE_indexType;   	   // Index to look up the proper DE camera type
  int DE_ImageRot;		     // Provide an angle to rotate the DE image
  int DE_ImageInvertX;     // Decides whether to invert the X axis on the DE image 1==invert.
  int DE_ServerDarkImage;  // Decides whether the DEServer will provide the Dark image
  CString DEServerIP;      // Server ip that the client will use to connect with
  int DE_ServerReadPort;   // Read port used with DE server.
  int DE_ServerWritePort;  // Write port used with the DE server.
  float DE_FramesPerSec;   // USER's setting for the frames per second for this camera
  float DE_CountingFPS;    // User's setting for FPS in counting mode
  float DE_MaxFrameRate;   // Maximum frame rate from server
  CString DE_AutosaveDir;  // Top folder for autosaves
  int alsoInsertCamera;    // Other camera to insert to make sure blanking is OK
  int samePhysicalCamera;  // Camera on same insertion state
  int eagleIndex;          // Index from looking up eagle camera by name
  BOOL GatanCam;           // Convenience flag for Gatan types
  BOOL DMCamera;           // And for all cameras with DMCamera interface
  BOOL STEMcamera;         // Flag that it is a STEM camera
  int K2Type;              // Flag for K2 initially
  int OneViewType;         // Flag for OneView camera
  BOOL useSocket;          // Flag to use socket instead of COM for DMcamera
  int beamShutter;         // "Alternate Shutter" value for beam only
  BOOL setAltShutterNC;    // Flag to initialize to alternate shutter normally closed
  int onlyOneShutter;      // Flag that there is only one shutter, or shutter to use for Eagle
  BOOL TietzCanPreExpose;  // Flag that shutter mode exists for pre-exposure
  int restoreBBmode;       // Flag to restore BB shutter mode after pre-exposure
  int TietzGainIndex;      // Gain index, 1, 2, ... up to number of gains
  int LowestGainIndex;     // Lowest allowed gain index
  int TietzImageGeometry;  // Bit sum for setting image geometry (flip, rotation)
  int TietzBlocks;         // Block size for Tietz block readout camera
  CString TietzFlatfieldDir;  // Directory for flatfield data
  int dropFrameFlags;      // Flags value with number of frames to drop and start/end
  int cropFullSizeImages;  // 1 to do subarea by crop when flatfielding, 2 to do so always
  float builtInSettling;   // Clear time
  float pixelMicrons;      // Actual pixel size in microns
  float magRatio;          // Mag relative to reported film mag
  float extraRotation;     // Additional rotation relative to calibrated camera
  int numBinnings;         // Number of binnings available
  int binnings[MAX_BINNINGS];   // Available binnings
  int numOrigBinnings;     // Original # of binnings for FEI STEM
  int origBinnings[MAX_BINNINGS];
  float gainFactor[MAX_BINNINGS];   // Relative gain factors for each binning
  int autoGainAtBinning;   // Binning at which to start automatic gain factors
  int falcon3ScalePower;   // Power for Falcon 3 scaling
  float falconEventScaling;  // Value to scale aligned EER sums by instead of count/elec
  CString falconRefDir;    // Reference directory for this camera
  CString falconFramePath; // Local Frame path for this camera
  int falconVariant;      // Separate flag for Falcon 4i variant (for now)
  float linear2CountingRatio;  // Ratio of raw counts in linear mode to counting mode
  float linearOffset;      // Amount to subtract before scaling (K3)
  float K3CDSLinearRatio;  // ratio of counts with CDS on to off
  int numExtraGainRefs;    // Number of extra gain refs
  int processHere;         // Per-camera flag for whether to normalize in SerialEM
  BOOL retractable;        // Is camera retractable
  BOOL canBlock;           // Run-time flag for whether it might be inserted
  float insertDelay;       // Delay time for insertion, in seconds
  float retractDelay;      // Delay time for retraction, in seconds
  int insertingRetracts;   // Other camera retracted by inserting this one
  int order;               // Order of camera in beam path
  CString name;            // Supplied name for camera
  CString DMRefName;       // File name of DM's reference
  CString countingRefForK2;  // Name of counting and super-resolution references
  CString superResRefForK2;
  BOOL GIF;                // Flag that EFTEM needs to be run, etc
  BOOL hasTVCamera;        // Has a TV camera that needs to be retracted
  int useTVToUnblank;      // TV needs to be put in to keep from blanking other cameras
  int filterIsFEI;         // Flag that filter is an FEI (Selectris)
  BOOL checkTemperature;   // Check whether temperature is stable before first acquire
  BOOL sideMount;          // No need to raise screen for side mount camera
  int canTakeFrames;       // Flag that camera can take frames for save or align
  float minFrameTime[MAX_BINNINGS];      // Minimum frame time, potentially per binning
  float frameTimeDivisor[MAX_BINNINGS];  // Frame times must be multiple of
  CString dirForFrameSaving; // Folder to save frames in
  BOOL useGPUforAlign[2];  // Flags to use GPU for frame alignment;
  float startupDelay;      // Delay time between start of blanking thread and shot
  float startDelayPerFrame; // Additional time to add per frame in K2 dose fractionation
  BOOL DMbeamShutterOK;    // Flag that we can direct DM to use beam shutter
  BOOL DMsettlingOK;       // Flag that we can set DM's awful drift settling
  BOOL DMopenShutterOK;    // Flag that it is OK to use OpenShutter
  float extraUnblankTime;  // Extra exposure time that occurs with our blanking
  float extraBeamTime;     // Amount to add to exposure before blanking at end
  float minimumDrift;      // Minimum drift settling time allowed
  float maximumDrift;      // Maximum settling (Eagle camera parameter)
  float minBlankedExposure; // Minimum camera exposure time when blanking inside exposure
  float extraOpenShutterTime; // Extra time that beam is on when shutter opened before shot
  float deadTime;            // Longest exposure time that gives effectively no counts
  float minExposure;         // Minimum allowed exposure
  float maxExposure;       // Maximum exposure time if non-zero
  float postBlankerDelay;    // Delay after operating blanker as shutter or for dark ref
  float postIntensityTimeout;  // Timeout to set after changing intensity
  CameraDefects defects;     // Structure with all camera defects
  CameraDefects *origDefects;  // Copy of original defects before DM defect list merged
  std::string defectString;  // String conversion of defects for output by plugin
  int defStringRotFlip;      // Rotation/flip of current defect string
  BOOL defNameHasFrameFile;  // Flag that defect string was composed with a filename
  int numHotColumns;                // Number of hot columns (exclude when removing X-rays)
  int hotColumn[MAX_HOT_COLUMNS];   // Hot columns
  std::vector<int> hotPixelX;    // Hot pixel X and Y coordinates
  std::vector<int> hotPixelY;
  int maxXRayDiameter;        // Maximum diameter of X ray in pixels
  BOOL hotPixImodCoord;       // Hot pixels are numbered from 1, lower left at 1,1
  float darkXRayAbsCrit;      // Absolute deviation criterion for X rays in Dark refs
  float darkXRayNumSDCrit;    // Number of SD's criterion
  int darkXRayBothCrit;       // 1 to require meeting both criteria
  float imageXRayAbsCrit;     // Absolute deviation criterion for X rays in gain refs
  float imageXRayNumSDCrit;   // Number of SD's criterion
  int imageXRayBothCrit;      // 1 to require meeting both criteria
  int showImageXRayBox;      // Flag to show X-ray removal box in dialog
  int gainRefFrames;          // Number of frames for gain reference
  int gainRefTarget;          // Target counts
  int gainRefBinning;         // Binning for gain ref: 1 or 2
  int gainRefSaveRaw;         // Save raw frames for DE camera
  int gainRefAverageDark;     // Average dark reference
  int gainRefNumDarkAvg;      // Number of times to average
  int TSAverageDark;          // Average dark reference in tilt series
  int numBinnedOffsets;       // Number of special x/y offsets for different binnings
  int offsetBinning[MAX_BINNINGS];    // The binning for this offset
  int offsetRefBinning[MAX_BINNINGS]; // The binning of the stored gain reference
  int binnedOffsetX[MAX_BINNINGS];    // The offsets in unbinned pixels
  int binnedOffsetY[MAX_BINNINGS];
  BOOL useMinDDDBinning;      // Flag to use the min DD binning for autofocus
  float countsPerElectron;    // Camera gain in counts per electron
  float unscaledCountsPerElec;  // Original value read in from properties
  int corrections;            // Corrections to do in DM, or -1 for default
  int sizeCheckSwapped;       // Flag that sizes are swapped in size check, or to skip it
  int subareasBad;            // Flag that subsets may give artifacts (Stokes Tietz)
  int centeredOnly;           // Flag that areas are to be centered
  float matchFactor;          // Factor for pixel matching between nonGIF cameras
  int useContinuousMode;      // Flag to use fast-continuous mode for Gatan camera
  BOOL setContinuousReadout;  // Flag to set continuous readout
  int continuousQuality;      // Positive to set a quality level in continuous mode
  BOOL useFastAcquireObject;  // Flag to use fast acquire object in GMS 2
  int balanceHalves;          // Flag to do balance-halves correction in continuous mode
  int halfBoundary;           // Unbinned pixel above boundary for balancing halves
  int ifHorizontalBoundary;   // If boundary is horizontal
  CString pluginName;         // Name for a plugin camera
  int cameraNumber;           // Camera number to select by
  int noShutter;              // No shutter at all, use blanking
  int pluginCanProcess;        // Flags that plugin camera can return processed (1=ds+2=gn)
  BOOL canPreExpose;          // Flag that the camera can manage pre-exposure
  CString shutterLabel1;      // Labels for shutter options in setup dialog
  CString shutterLabel2;
  CString shutterLabel3;
  int rotationFlip;           // Rotation and flip for STEM or Gatan or plugin camera
  int imageRotation;          // Amount this rotates images for STEM plus 180 if needed
  int DMrotationFlip;         // Rotation and flip that DM is supposed to be set for
  int setRestoreRotFlip;      // 2 to set Gatan/restore it at end, 1 just to set; 0 query
  int rotFlipToRestore;       // Actual value to restore on exit if not -1
  int postActionsOK;          // Per-camera specification if post-actions allowed
  int taskTargetSize;         // Target size to bin to for tasks
  FloatVec doseTabCounts;     // Dose rate scaling table: mean electron counts 
  FloatVec doseTabRates;      // ... and dose rates
  float doseTabBase;
  float doseTabScale;
  float doseTabConst;
  float specToCamDoseFac;     // Last measured scaling from specimen to camera dose rate
  float addToExposure;        // Constant to add to exposure time 
  float addToEERExposure;     // Additional amount to add when saving EER
  int JeolDetectorID;         // Detector ID to be selected/deselected when switching
  BOOL invertFocusRamp;       // Lazy way out: flag to invert direction of dynamic focus
  int numChannels;
  CString channelName[MAX_STEM_CHANNELS];  // Detector or official camera names for FEI,
  CString detectorName[MAX_STEM_CHANNELS]; //   or memory mapping area name for Tietz
  BOOL availableChan[MAX_STEM_CHANNELS];
  BOOL needShotToInsert[MAX_STEM_CHANNELS];
  int maxChannels;            // Maximum channels at once
  float minPixelTime;         // Minimum pixel time in microseconds
  double maxPixelTime;        // Maximum pixel time in microseconds (if non-zero)
  double pixelTimeIncrement;  // Increment between pixel times in microseconds (if not 0)
  int maxIntegration;         // Maximum integration level (0 for none available)
  float maxScanRate;          // Minimum scan rate in um / ms
  float advisableScanRate;    // Alternate advised minimum rate
  float basicFlyback;         // Flyback hard-coded for FEI or read from digiscan
  float addedFlyback;         // Increment calibrated and added as property
  float flyback;              // Sum for convenience
  BOOL subareaInCorner;       // FEI bug
  int minMultiChanBinning[MAX_STEM_CHANNELS];   // For JEOL multichannel limitations
};

struct FrameAliParams {
  int strategy;               // Choice of how many pairwise or if cumulative ref
  int aliBinning;             // Binning for alignment
  BOOL binToTarget;           // Flag to bin to target size instead
  int targetSize;            // And target size
  int numAllVsAll;            // All-vs-all for strategy 0 only
  float rad2Filt1, rad2Filt2, rad2Filt3, rad2Filt4;   // Up to 4 filters just in case
  BOOL hybridShifts;          // Use hybrid shifts with multiple filters        
  float sigmaRatio;           // Fixed ratio of sigma to radius
  float refRadius2;           // Radius 2 for refine
  BOOL doRefine;              // Whether to refine
  int refineIter;             // Iterations of refinement
  BOOL useGroups;             // Whether to use groups
  int groupSize;              // Group size if so
  BOOL doSmooth;              // Whether to smooth at all
  int smoothThresh;           // Threshold number of frames for smoothing
  int shiftLimit;             // Limit on pixels of shift
  BOOL truncate;              // Whether to truncate
  float truncLimit;           // And limit if so
  int antialiasType;          // Antialias reduction type
  float stopIterBelow;        // Threshold for stopping iterations
  BOOL groupRefine;           // Falg to refine with groups
  BOOL keepPrecision;         // Flag to keep precision in alignment
  BOOL outputFloatSums;       // Flag to save aligned sums as floats
  BOOL alignSubset;           // Flag to align a subset of frames
  int subsetStart, subsetEnd; // Start and end of range, numbered from 1
  int whereRestriction;       // 1 for only in plugin, 2 for only in IMOD
  int sizeRestriction;        // 1 for only 4K, 2 for only 8K
  int EERsuperRes;            // 0, 1, or 2 for super-resolution reading EER data
  CString name;               // Name to show
  bool multAliBinByEERfac;    // RUN-TIME: multiply align bin by EER super-resolution fac
};

// Elements of matrix: units of X per X input, etc
struct ScaleMat {
  float xpx;
  float xpy;
  float ypx;
  float ypy;
};

// Check OnCopyArea when add new members
struct LowDoseParams {
  int magIndex;            // Magnification index
  int camLenIndex;         // Camera length index if in diffraction
  int spotSize;            // spot size
  double intensity;        // beam intensity value
  double ISX, ISY;         // Incremental image shifts: use when mode active
  double beamDelX, beamDelY; // Incremental beam shifts for mode
  double axisPosition;     // Distance along axis: use when inactive
  float delayFactor;       // multiplier for image shift delay
  BOOL slitIn;             // Flag that slit is supposed to be in
  float energyLoss;        // Energy loss value, sent to spectrum offset
  float slitWidth;         // slit width
  BOOL zeroLoss;           // Do zero loss, not the energy loss value
  float beamAlpha;         // Alpha value for JEOL
  int probeMode;           // Probe mode for FEI
  double diffFocus;        // Diffraction focus
  double beamTiltDX, beamTiltDY;  // Incremental beam tilts for mode
  int darkFieldMode;       // Dark field mode for FEI
  double dfTiltX, dfTiltY;      // Absolute dark field tilt for F/T
};

// State parameters and an initializer
struct StateParams {
  StateParams() {
    // Zero out items in case it is a low dose set
    energyLoss = slitWidth = 0.;
    intensity = 0.;
    magIndex = spotSize = 0;
    probeMode = -1;
    slitIn = zeroLoss = false;
    ldParams.delayFactor = 0.9f;
    singleContMode = SINGLE_FRAME;
    beamAlpha = -999;
    targetDefocus = -9999.f;
    ldDefocusOffset = -9999.f;
    ldShiftOffsetX = -9999.f;
    camForParams = -1;
    montMapConSet = false;
    focusXoffset = focusYoffset = 0;
    rotateAxis = false;
    axisRotation = 0;
  }
  LowDoseParams ldParams;  // Low dose Record parameters if this is a low dose state
  int lowDose;            // Flag that it is a low dose state
  int camIndex;            // Camera index
  int magIndex;            // Magnification index
  int spotSize;            // spot size
  double intensity;        // beam intensity value
  int probeMode;           // Probe mode for FEI
  int beamAlpha;           // Alpha value for JEOL
  BOOL slitIn;             // Flag that slit is supposed to be in
  float energyLoss;        // Energy loss value, sent to spectrum offset
  float slitWidth;         // slit width
  BOOL zeroLoss;           // Do zero loss, not the energy loss value
  int binning;             // Binning
  int xFrame;              // Binned frame size
  int yFrame;
  float exposure;          // exposure time, drift, and shuttering values
  float drift;
  float frameTime; 
  int shuttering;
  int K2ReadMode;          // Acquisition mode
  int singleContMode;      // Continuous mode and other states that will be canceled
  int doseFrac;            // for a map state and need to be restored; and are now saved
  int saveFrames;          // generally for imaging states
  int processing;          
  int alignFrames;         // Some more acquisition parameters
  int useFrameAlign;
  int faParamSetInd;
  int focusXoffset;        // Offsets from center of focus area, in unbinned pixels (RTO)
  int focusYoffset;
  float focusAxisPos;      // Axis position of focus area
  BOOL rotateAxis;         // Whether axis is rotated from tilt axis
  int axisRotation;        // Amount of rotation, or 0 if not rotated
  // Camera modes for other parameter sets when not a low dose state
  int readModeView, readModeFocus, readModeTrial, readModePrev, readModeSrch,readModeMont;
  float targetDefocus;     // Defocus target or view/search defocus offset; -9999 if none
  float ldDefocusOffset;
  float ldShiftOffsetX;    // View or search shift offset, -9999 if none
  float ldShiftOffsetY;
  BOOL montMapConSet;      // Flag that it is mont-map not Record
  CString name;
  int camForParams;        // Runtime: Camera index to which parameters should be restored
};

struct FilterParams {
  BOOL slitIn;             // Flag that slit is supposed to be in
  float energyLoss;        // Energy loss value, sent to spectrum offset
  float slitWidth;         // slit width
  BOOL zeroLoss;           // Do zero loss, not the energy loss value
  BOOL autoMag;            // Change mag automatically when screen is lowered
  BOOL autoCamera;         // Change camera and EFTEM mode together
  int firstGIFCamera;
  int firstRegularCamera;
  BOOL matchPixel;         // Change mag index to match pixel size
  BOOL matchIntensity;     // Change intensity to maintain electrons/pixel
  float binFactor;         // binning factor to apply to GIF camera in matching pixel
  BOOL doMagShifts;        // Flag to do mag-dependent shifts
  float currentMagShift;   // Shift to apply now
  int alignedMagInd;       // Mag at which shift is zero
  float alignedSlitWidth;  // Width when ZLP was aligned
  float magShifts[MAX_MAGS];  // Table of shifts
  float refineZLPOffset;   // Offset to apply from refine ZLP
  double alignZLPTimeStamp; // Time (ticks/1000) at which ZLP aligned or refined
  double lastFeiZLPshift;   // Zero loss peak adjustment from FEI interface
  BOOL adjustForSlitWidth;    // Flag to adjust for slit width changes
  int positiveLossOnly;     // Flag that filter has only positive energy shifts
  BOOL refineWithTrial;     // Flag to refine with Trial in Low Dose
  float minLoss, maxLoss;     // Minimum and maximum allowed energy loss
  float minWidth, maxWidth;   // Minimum and maximum slit width
  int cumulNonRunTime;      // cumulative non run time since align/refine zlp
  BOOL usedOldAlign;        // Flag that alignment from a previous session was used
};


struct TiffField {
  int tag;
  int type;
  char separator; // Separator character
  int tokenNum;   // Number of token within field, numbered from 1
};

// Navigator parameters to remain resident in winApp
// Acquisition parameters
struct NavAcqParams {
  int acquireType;         // Type of current acquisition on areas
  int nonTSacquireType;    // User's latest value for a non-TS acquire type
  int macroIndex;          // Macro index numbered from 1
  int preMacroInd;         // Pre-macro index also from 1
  int postMacroInd;         // Post-macro index also from 1
  int preMacroIndNonTS;    // Pre-macro index for non tilt series actions
  int postMacroIndNonTS;    // Post-macro index for non tilt series actions
  BOOL runPremacro;         // Run script before TS
  BOOL runPostmacro;        // run script after TS
  BOOL runPremacroNonTS;    // Run script before nonTS
  BOOL runPostmacroNonTS;   // run script after nonTS
  BOOL skipInitialMove;     // Skip final move now
  BOOL skipZmoves;          // Skip Z moves
  BOOL restoreOnRealign;    // Restore state after realign, scripts
  BOOL closeValves;         // Close valves at end
  BOOL sendEmail;           // send email at end for TS
  BOOL sendEmailNonTS;      // Send email at end for nonTS
  BOOL cycleDefocus;        // Flag to cycle defocus
  float cycleDefFrom;       // Starting value
  float cycleDefTo;         // Ending value
  int cycleSteps;           // Steps WITHIN cycle from start to end
  BOOL earlyReturn;         // do early return
  int numEarlyFrames;       // Number of frames, -1 for all
  BOOL noMBoxOnError;       // No message box, keep going on errors
  BOOL skipSaving;          // Skip saving to file
  BOOL hideUnusedActs;      // Hide unused tasks
  int acqDlgSelectedPos;    // Currently selected task for editing
  float focusChangeLimit;   // Limit on focus change
  int DEdarkRefOperatingMode;  // Linear or counting
  BOOL highFlashIfOK;       // Flag that a high flash is OK to do
  BOOL astigByBTID;         // Use autofocus type BTID for astigmatism
  BOOL adjustBTforIS;       // Adjust BT and astig to compensate IS
  BOOL relaxStage;          // Relax stage from backlashed position
  BOOL hybridRealign;       // Just move to target in realign if skipping first round
  BOOL hideUnselectedOpts;  // Hide options not checked
  int mapWithViewSearch;    // Take single images with different areas
  BOOL saveAsMapChoice;     // Flag that option to save images/montages as maps is checked
  BOOL retractCameras;      // Flag to retract cameras at end
  BOOL runHoleCombiner;     // Flag to run the multi-hole combiner after hole finder
  BOOL useMapHoleVectors;   // Flag to use hole vectors in new drawn-on map for multishot
  int endMacroInd;          // Macro to run at end of acquisition
  BOOL runEndMacro;         // Flag for whether to run it
  BOOL realignToScaledMap;  // Flag to do Realign to Item to scaled map
  int conSetForScaledAli;   // Control set to use for it
  BOOL skipZinRunAtNearest; // Skip Z move when running task at nearest item
};

// Other Nav parameters
struct NavParams {
  BOOL warnedOnSkipMove;
  CString stockFile;
  int numImportXforms;
  ScaleMat importXform[MAX_IMPORT_XFORMS];
  CString xformName[MAX_IMPORT_XFORMS];
  TiffField xField[MAX_IMPORT_XFORMS];
  TiffField yField[MAX_IMPORT_XFORMS];
  TiffField idField[MAX_IMPORT_XFORMS];
  CString xformID[MAX_IMPORT_XFORMS];
  float gridInPolyBoxFrac;
  float holeInPolyBoxFrac;
  double importXbase, importYbase;
  CString overlayChannels;
  float stageBacklash;
  CString autosaveFile;
  float maxMontageIS;      // Maximum image shift allowed for montaging with IS
  float maxLMMontageIS;    // Maximum shift allowed in low mag
  float fitMontWithFullFrames;  // Fit montage with full frames + frac to increase overlap
  int maxReconnectsInAcq;  // maximum number of scope reconnections during acquire
  int minPtsForCombineInPMM;  // Min points needed for combining in ProcessMultipleMaps
};

// Multigrid parameters
struct MultiGridParams {
  BOOL appendNames;          // Flag to append names to rootname
  BOOL useSubdirectory;      // Flag to use subdirectories for files
  BOOL setLMMstate;          // Flag to set a state for LM map
  int LMMstateType;          // Type of acquisition/state for LM map
  int LMMstateNum;           // Number of state to be set for LM map
  CString LMMstateName;      // Name of state to be set
  BOOL removeObjectiveAp;    // Flag to remove objective aperture for LM imaging
  BOOL setCondenserAp;       // Flag to change condenser aperture for LM imaging
  int condenserApSize;       // Condenser size to set
  int LMMmontType;           // Type of montage to take for LM map
  int LMMnumXpieces;         // Number of pieces in X if fixed size LM map
  int LMMnumYpieces;         // Number of pieces in Y
  BOOL setLMMoverlap;        // Flag to
  int LMMoverlapPct;         // Percent overlap to use
  BOOL autocontour;          // Flag to do autocontouring
  BOOL acquireMMMs;          // Flag to acquire medium mag maps
  BOOL acquireLMMs;          // Flag to acquire low mag maps
  int MMMnumXpieces;         // Number of pieces in X and Y if fixed size
  int MMMnumYpieces;
  int MMMstateType;          // Type of image/state for MM maps
  int MMMstateNums[4];       // State numbers to set
  CString MMMstateNames[4];  // State names to set for MM maps
  int MMMimageType;          // Single image/polygon mont/fixed mont
  BOOL runFinalAcq;          // Flag to do final acquistion
  int finalStateNums[4];     // State numbers for final acquire
  CString finalStateNames[4]; // State names for final acquire
  BOOL framesUnderSession;   // Make frame directories under session or grid directories
};

// Cooker parameters
struct CookParams {
  int spotSize;
  int magIndex;
  double intensity;
  int targetDose;
  BOOL trackImage;    // Flag to track position before and after
  BOOL cookAtTilt;    // Flag to cook at tilt
  float tiltAngle;    // Angle to tilt to
  BOOL timeInstead;   // Flag to go for total time instead
  float minutes;      // Minutes to do instead
};

// Autocenter beam parameters
struct AutocenParams {
  int camera;
  int magIndex;
  int spotSize;
  int probeMode;
  double intensity;
  int binning;
  float exposure;
  BOOL useCentroid;
  BOOL shiftBeamForCen;
  float beamShiftUm;
  float addedShiftX, addedShiftY;
};

// Tilt range finder params
struct RangeFinderParams {
  BOOL eucentricity;    // Flag to do eucentricity first
  BOOL walkup;          // Flag to do walkup
  BOOL autofocus;       // Autofocus at low angle
  int imageType;        // Type of image to acquire
  float startAngle;     // Starting, ending angle, increment
  float endAngle;
  float angleInc;
  int direction;        // 0 for both, 1 minus, 2 plus
};

// Phase plate conditioning params
struct VppConditionParams {
  int magIndex;         // Scope state
  int spotSize;       
  double intensity;
  int alpha;
  int probeMode;
  int seconds;          // Total time to expose
  int nanoCoulombs;     // Or total charge
  BOOL timeInstead;     // Flag to go for total time instead
  int whichSettings;    // Whether to use Record, Trial, or separate settings
  BOOL useNearestNav;   // Flag to go to a nav point for conditioning
  CString navText;      // String to match at start of note or label
  BOOL useNavNote;      // Flag to use note instead of label
  int postMoveDelay;    // Seconds of delay after move to next plate position
  int camLenIndex;      // Run-time storage if saved state is in diffraction
  int lowDoseArea;      // And run-time saved low dose area
};

// A set of mutually exclusive channels, possibly switched into or mapped to one DS channel
struct ChannelSet {
  int numChans;               // Number of channels
  int mappedTo;               // DS Channel mapped to
  int channels[MAX_STEM_CHANNELS];   // List of channels
};

#define HITACHI_IS_HT7800     1
#define HITACHI_HAS_STAGEZ  0x2

// Hitachi  microscope params
struct HitachiParams {
  std::string IPaddress;
  std::string port;
  int beamBlankAxis;
  float beamBlankDelta;
  int usePAforHRmodeIS;
  float imageShiftToUm;
  float beamShiftToUm;
  float beamTiltScale;
  float objectiveToUm[3];
  float screenAreaSqCm;
  int stageTiltSpeed;
  int stageXYSpeed;
  int magTable[MAX_MAGS];
  int camLenTable[MAX_CAMLENS];
  int baseFocus[MAX_MAGS];
  int lowestNonLMmag;
  int lowestSecondaryMag;
  BOOL usingSharedMem;
  int flags;
  int lastMember;
};

struct ScreenShotParams {
  int imageScaleType;
  float imageScaling;
  BOOL ifScaleSizes;
  float sizeScaling;
  int fileType;
  int compression;
  int jpegQuality;
  int skipOverlays;
  CString lastFilePath;
};

// Parameters for managing dewars and vacuum events
struct DewarVacParams
{
  BOOL checkPVP;             // Check if PVP is running
  BOOL runBufferCycle;       // Run buffer cycle at interval
  int bufferTimeMin;         // Interval to run at
  BOOL runAutoloaderCycle;   // Run autoloader buffer cycle at interval
  int autoloaderTimeMin;     // Interval to run at
  BOOL refillDewars;         // Refill at interval
  float dewarTimeHours;      // Time between refills
  BOOL checkDewars;          // Check status of dewar filling
  float pauseBeforeMin;      // Pause time before filling
  BOOL startRefill;          // Start a refill if remaining time is low enough
  float startIntervalMin;    // Start a refill if within this time
  float postFillWaitMin;     // Wait time after filling
  BOOL doChecksBeforeTask;   // Separate when the checks are done from when filling is
};

#endif
