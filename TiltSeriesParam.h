#ifndef TILTSERIESPARAM_H
#define TILTSERIESPARAM_H

#define TRACK_BEFORE_FOCUS 0
#define TRACK_AFTER_FOCUS 1
#define TRACK_BEFORE_AND_AFTER 2
#define BEAM_CONSTANT           0
#define BEAM_INTENSITY_FOR_MEAN 1
#define BEAM_INVERSE_COSINE     2
#define MAX_TS_SIMULTANEOUS_CHAN  6
enum {NO_TS_EXTRA_RECORD, TS_FOCUS_SERIES, TS_FILTER_SERIES, TS_OPPOSITE_TRIAL, 
TS_OTHER_CHANNELS};
enum {TS_VARY_EXPOSURE = 0, TS_VARY_DRIFT, TS_VARY_FOCUS, TS_VARY_ENERGY, TS_VARY_SLIT,
TS_VARY_FRAME_TIME};

struct VaryInSeries {
  float angle;
  BOOL plusMinus;
  BOOL linear;
  short int type;
  float value;
};

struct TiltSeriesParam {
  float startingTilt;          // Starting tilt angle
  float endingTilt;            // Ending tilt angle
  float tiltIncrement;         // Basic increment
  BOOL cosineTilt;             // Use cosine increments
  float lastStartingTilt;      // Starting tilt angle of last tilt series
  float tiltDelay;
  int magIndex[3];             // Magnification index, indexed by STEM index
  BOOL magLocked;              // Flag that mag is locked for montaging
  int binning;                 // binning
  int lowMagIndex[3];          // mag index for tracking, indexed by STEM index
  BOOL trackLowMag;
  int cameraIndex;             // camera index into active camera list
  int probeMode;               // Probe mode index for STEM
  int beamControl;             // option for beam control
  int meanCounts;              // target counts for intensity
  BOOL taperCounts;            // Taper counts above a certain angle
  int highCounts;              // Counts to achieve at high tilt
  float taperAngle;            // Angle above which to taper
  BOOL limitIntensity;         // flag to limit to intensity at start of series
  BOOL limitToCurrent;         // flag to limit to intensity to current value
  int cosinePower;             // Power for cosine change of intensity
  BOOL intensitySetAtZero;     // Flag to change intensity by cosine if starting at tilt
  BOOL refineEucen;            // Refine eucentricity if starting at 0
  BOOL leaveAnchor;            // flag to leave anchor when walk up
  float anchorTilt;            // target angle for the anchor
  BOOL useAnchor;              // flag to use anchor that user left
  BOOL refIsInA;               // flag that buffer A has a reference
  int anchorBuffer;            // Buffer number of anchor
  float ISlimit;               // Limit for image shift
  double beamTilt;             // specified beam tilt
  float targetDefocus;         // Target for autofocus
  float autofocusOffset;       // Offset for doing autofocus
  float focusCheckInterval;    // Max interval to check focus
  BOOL skipAutofocus;          // Flag to skip autofocus completely
  BOOL limitDefocusChange;     // Whether to limit defocus changes
  float defocusChangeLimit;    // Maximum focus change in autofocus
  BOOL applyAbsFocusLimits;    // Whether to apply the absolute focus limits
  BOOL repeatRecord;           // Flag to repeat if too far out
  BOOL confirmLowDoseRepeat;        // Flag to allow repeating in low dose too
  float maxRecordShift;        // Maximum error to allow
  BOOL stopOnBigAlignShift;    // Flag to stop if alignment shift exceeds criterion
  float maxAlignShift;         // Maximum shift as fraction of image size
  BOOL repeatAutofocus;        // Flag to repeat focus if off
  float maxFocusDiff;          // maximum difference from prediction
  float fitIntervalX;          // maximum range of tilts to include in X fits
  float fitIntervalY;          // maximum range of tilts to include in Y fits
  float fitIntervalZ;          // maximum range of tilts to include in Z fits
  float maxPredictErrorXY;     // Errors greater than this fraction of field force track
  float maxPredictErrorZ[3];   // Errors in Z prediction bigger than this force focus
  int minFitXQuadratic;        // Minimum points to fit quadratic to X
  int minFitYQuadratic;        // Minimum points to fit quadratic to Y
  BOOL alwaysFocusHigh;        // Flag to always focus above a given angle
  float alwaysFocusAngle;      // Angle above which to always focus
  BOOL checkAutofocus;         // Flag to check autofocus in startup sequence
  float minFocusAccuracy;      // Minimum focus accuracy value required not to stop
  BOOL alignTrackOnly;         // Flag not to use record for alignment in LD mode
  float maxRefDisagreement;    // Fraction disagreement that will force new track ref
  BOOL previewBeforeNewRef;    // Flag to take preview and align before getting new ref
  int trackAfterFocus;         // whether to track before or after focus or both
  BOOL refineZLP;              // Flag to refine the ZLP periodically
  int zlpInterval;             // Interval in minutes between ZLP refines
  BOOL centerBeamFromTrial;    // Flag to center beam in low dose from trial image
  BOOL closeValvesAtEnd;       // Flag to close valves at end
  BOOL manualTracking;         // Flag for manual tracking - stop after align
  int extraRecordType;         // 0 = none, 1 = focus, 2 = filter
  int numExtraFocus;           // Number of extra focus values
  double extraFocus[MAX_EXTRA_RECORDS];   // Relative focus changes
  int numExtraFilter;          // Number of extra filter settings
  int extraSlits[MAX_EXTRA_RECORDS];      // -1 slit open; 0 default, or slit width
  double extraLosses[MAX_EXTRA_RECORDS];  // Relative changes in energy loss
  int numExtraChannels;        // Number of extra channels to acquire in separate shots
  int extraChannels[MAX_STEM_CHANNELS];    // Channels numbered from 1
  BOOL stackRecords;           // Stack Records in separate window
  int stackBinSize;            // Maximum binned size in stack
  BOOL extraSetSpot;           // Flag to set spot size in extra records
  BOOL extraSetExposure;       // Flag to set exposure/drift in extra records
  int numExtraExposures;       // Number of extra exposures
  double extraExposures[MAX_EXTRA_RECORDS];  // new exposures
  float extraDrift;            // one new drift
  float extraDeltaIntensity;   // Change in intensity
  int extraSpotSize;           // New spot size
  BOOL extraSetBinning;        // Flag to set binning for extra trials
  int extraBinIndex;           // New binning
  VaryInSeries varyArray[MAX_TS_VARIES];
  int numVaryItems;            // Array for scheduled changes and number in array
  BOOL doVariations;           // Flag to use the changes
  BOOL changeRecExposure;         // Flag to change exposure time instead of beam intensity
  BOOL changeAllExposures;     // Flag to change all the exposure times not just Record
  BOOL changeSettling;         // Flag to change drift settling proportionally
  BOOL cenBeamPeriodically;    // Flag to recenter beam periodically
  int cenBeamInterval;         // Interval in minutes
  BOOL cenBeamAbove;           // Center beam going above an angle
  float cenBeamAngle;          // The angle at which to do it
  BOOL doBidirectional;        // Flag to do bidirectional series
  float bidirAngle;            // Starting angle of bidirectional series
  int bidirAnchorMagInd[6];    // Mag to use for anchor image, out and in low dose
  BOOL anchorBidirWithView;    // Flag to anchor with View in Low Dose
  BOOL walkBackForBidir;       // Use walkup to return to starting angle
  BOOL retainBidirAnchor;      // Flag to store second image and keep file
  BOOL doDoseSymmetric;        // Flag to do alternating images on both sides of start
  int dosymBaseGroupSize;      // Initial size of group to do on each side
  BOOL dosymIncreaseGroups;    // Flag to use parameters for increasing group size
  int dosymGroupIncInterval;   // Interval between groups at which to increase group size
  int dosymGroupIncAmount;     // Amount to increase group size each time
  float dosymIncStartAngle;    // Angle above which to increment group size
  BOOL dosymDoRunToEnd;        // Flag to go to endabove an angle
  float dosymRunToEndAngle;    // Angle above which to finish the side
  BOOL dosymAnchorIfRunToEnd;  // Use View anchor between 1st and 2nd part when run to end
  int dosymMinUniForAnchor;    // Minimum size of first unidirectional part to use anchor
  BOOL doEarlyReturn;          // Flag to do early return from K2
  int earlyReturnNumFrames;    // Number of frames to return with
  BOOL initialized;            // Setup has been run
  int refCount;                // Reference count for multiple use
};

#endif
