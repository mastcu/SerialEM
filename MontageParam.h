#ifndef MONTAGEPARAM_H
#define MONTAGEPARAM_H
#include <vector>

#define MAX_MONT_PIECES 65535
#define MIN_Y_MONT_REALIGN 5
#define MINI_NO_PIECE -30000

struct MontParam {
  int xFrame, yFrame;      // Frame size
  int xNframes, yNframes;  // # of frames in X and Y
  int xOverlap, yOverlap;  // overlap
  int binning;             // binning
  int magIndex;            // index in mag table
  int cameraIndex;         // index to camera # to use out of active list
  int zCurrent;            // Current Z value
  int zMax;                // Maximum Z in file
  int overviewBinning;     // Binning for overview
  int prescanBinning;      // Binning for prescan images
  int maxPrescanBin;       // Initial, maximum binning for prescan
  int maxOverviewBin;      // Initial, maximum binning for overview
  BOOL moveStage;          // Use stage movement instead of image shift
  BOOL settingsUsed;       // Mag and binning were used for acquisition
  BOOL warnedCalOpen;      // User was warned about calibration when file opened
  BOOL warnedCalAcquire;   // User was warned about calibration on acquire
  BOOL warnedBinChange;    // User was warned about changing binning
  BOOL warnedMagChange;    // User was warned about changing mag
  BOOL warnedCamChange;    // User was warned about changing camera
  BOOL warnedStageChange;  // User was warned about changing move stage
  BOOL warnedConSetBin;    // User was warned about binning different from parameter set
  BOOL offerToMakeMap;     // Ask user whether to make a map from montage
  BOOL adjustFocus;        // Change focus for column
  BOOL correctDrift;       // Use drift correction
  BOOL focusAfterStage;    // Autofocus after each stage move
  BOOL repeatFocus;        // Repeat focus until drift is low enough
  float driftLimit;        // Limiting drift amount in nm/sec
  BOOL showOverview;       // Show overview at end
  BOOL shiftInOverview;    // Shift pieces when putting into overview 
  BOOL verySloppy;         // Use correlation parameters for very sloppy montages
  int numToSkip;           // Number of pieces to skip
  std::vector<short int> skipPieceX;    // Piece numbers to skip, numbered from 0
  std::vector<short int> skipPieceY;
  int fitToPolygonID;       // ID of polygon it was fit to
  BOOL ignoreSkipList;      // User can select to take full rectangle
  int insideNavItem;        // Navigator item to take frames inside of
  BOOL skipOutsidePoly;     // Flag to skip frames outside that item
  BOOL wasFitToPolygon;     // Flag if it was fit to a polygon
  float minOverlapFactor;   // Minimum overlap factor for stage montage
  float minMicronsOverlap;  // And minimum overlap in microns
  float byteMinScale;       // Scaling factors for converting to bytes when read
  float byteMaxScale;
  BOOL useMontMapParams;    // Flag to use the montage mapping parameters
  BOOL useViewInLowDose;    // Flag to use View parameters in low dose mode
  BOOL useSearchInLowDose;  // Flag to use Search parameters/area in low dose
  BOOL setupInLowDose;      // Flag that params were set up in low dose
  BOOL useContinuousMode;   // Flag to use continuous mode if possible
  float continDelayFactor;  // Delay (settling) factor for continuous mode
  BOOL noDriftCorr;         // Turn off drift correction for non HQ stage montage
  BOOL noHQDriftCorr;       // Turn off drift correction for HQ stage montage
  BOOL useHqParams;         // Enable all HQ-type params
  BOOL focusInBlocks;       // Focus in blocks
  int focusBlockSize;       // Size of blocks for focusing in when doing stage
  BOOL realignToPiece;      // Periodically realign to a full piece
  int realignInterval;      // Interval at which to do it
  float maxAlignFrac;       // Maximum fraction of field to allow alignment
  float hqDelayTime;        // Extra long delay after moving stage
  BOOL ISrealign;           // Realign with image shift
  float maxRealignIS;       // Maximum image shift for realigning
  BOOL useAnchorWithIS;     // Flag to have a center anchor image
  int anchorMagInd;         // Mag index for anchor
  BOOL skipCorrelations;    // Flag to skip the correlation step
  BOOL skipRecReblanks;     // Flag to skip the reblank in Record readout
  BOOL forFullMontage;      // Flag that it should use stage positions saved next
  float fullMontStageX;
  float fullMontStageY;
  int refCount;             // Reference count for multiple use
};

// Structure to keep track of offsets applied in making montage overview
struct MiniOffsets {
  std::vector<short int> offsetX;   // The offsets: True offset of piece / minizoom with
  std::vector<short int> offsetY;   // Y inverted; indexed by ixpc * nYframes + iypc
  int xNframes, yNframes;          // # of frames in X and Y
  int xDelta, yDelta;      // Spacing between effective frame boundaries
  int xFrame, yFrame;      // frame sizes
  int xBase, yBase;        // base values for boundaries
  bool subsetLoaded;       // Only a subset of frames was loaded
};

#endif
