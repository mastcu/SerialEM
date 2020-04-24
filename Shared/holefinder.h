/* 
 * holefinder.h: header for holefinder module.
 * No ID line: shared with SerialEM
 */

#ifndef HOLEFINDER_H
#define HOLEFINDER_H
#include <vector>
#include <queue>

typedef std::vector<float> FloatVec;
typedef std::vector<int> IntVec;
typedef std::vector<short> ShortVec;
typedef std::vector<FloatVec> FloatVecArray;

#define SOBEL_HIST_DIM 1000

#define CACHE_KEEP_DATA   1
#define CACHE_KEEP_BOTH   2
#define CACHE_KEEP_MEDIAN 4
#define CACHE_KEEP_AVGS   8

class HoleFinder {

 public:
  HoleFinder(void);
  ~HoleFinder(void);

  void sobelEdge(float *inputData, unsigned char *sobelDir, float &sobelMin,
                 float &sobelMax);
  int cannyEdge(float sigma, float f1, float f2);

  void setSequenceParams(float diameter, float spacing, bool retain, 
                         float maxError, float fracAvg, int minForAvg, 
                         float maxDiamErrFrac, float avgOutlieCrit,
                         float finalNegOutlieCrit, float finalPosOutlieCrit, 
                         float finalOutlieRadFrac);
  void setRunsInSequence(float *increments, float *widths, int *numCircles, int numScans, 
                         float *sigmas, int numSigmas, float *thresholds, int numThresh);

  int runSequence(int &iSig, float &sigUsed, int &iThresh, float &threshUsed, 
                  FloatVec &xBoundary, FloatVec &yBoundary, int &bestSigInd,
                  int &bestThreshInd, float &bestRadius, float &trueSpacing,
                  FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
                  FloatVec &xMissing, FloatVec &yMissing, FloatVec &xCenClose,
                  FloatVec &yCenClose, FloatVec &peakClose, int &numMissAdded);
  int findCircles(float midRadius, float radiusInc, float width, int numRad, bool retain, 
                  float minSpacing, int useWeak, FloatVec &xBoundary,
                  FloatVec &yBoundary, float &bestRadius,
                  FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals);
  int analyzeNeighbors(FloatVec &xCenVec, FloatVec &yCenVec, FloatVec &peakVals,
                       IntVec &altInds, FloatVec &xCenAlt, FloatVec &yCenAlt,
                       FloatVec &peakAlt,float maxSpacing, float maxError,
                       int reuseGeomBin, 
                       float &trueSpacing, FloatVec &xMissing, FloatVec &yMissing);
  int templateAndAnalyze(float midRadius, bool retain, bool &madeAverage, bool &usedRaw,
                         FloatVec &xBoundary, FloatVec &yBoundary,
                         FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
                         float &trueSpacing, FloatVec &xMissing, FloatVec &yMissing);
  void mergeAlternateLists(FloatVec **xcenPtr, FloatVec **ycenPtr, FloatVec **peakPtr,
                           IntVec &altInds, int domInd, float minSpacing, float maxError);
  int processMontagePiece
    (float sigma, float threshold, float spacing, int pieceNum, FloatVec &xBoundary, 
     FloatVec &yBoundary, float intensityRad, int &numFromCorr);
  void resolvePiecePositions
    (bool removeDups, FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
     FloatVec &xMissing, FloatVec &yMissing, FloatVec &xCenClose, FloatVec &yCenClose,
     FloatVec &peakClose, IntVec &pieceOn, FloatVec &xInPiece, FloatVec &yInPiece,
     FloatVec *holeMeans, FloatVec *holeSDs, FloatVec *holeOutlies);

  int initialize(void *inputData, int mode, int nx, int ny, float reduction, 
                 float maxRadToAnalyze, int cacheFlags);
  void clearCircleCache();
  void clearData();
  void clearAll();
  const char *returnErrorString(int err);
  void setVerbose(int inVal) {mVerbose = inVal;};
  void setDebugImages(int inVal) {mDebugImages = inVal;};
  int makeTemplate(FloatVec &xCenVec, FloatVec &yCenVec, 
                   int numAverage, float separation, bool rawAverage);
  int rescueMissingPoints(FloatVec &xCenOrig, FloatVec &yCenOrig, FloatVec &peakOrig,
                          FloatVec &xMissing, FloatVec &yMissing, FloatVec &xCenWeak,
                          FloatVec &yCenWeak, FloatVec &peakWeak, float maxError, 
                          int &numAdded);
  int holeIntensityStats(float radius, FloatVec &xCenters, FloatVec &yCenters, 
                         bool reduced, bool smoothedSD, FloatVec *means, FloatVec *SDs,
                         FloatVec *gradients, float outlieCrit = 2.5);
  int removeOutliers
    (FloatVec &xCenters, FloatVec &yCenters, FloatVec *peakVals, FloatVec &values,
     float negCrit, float posCrit, int &numNeg, int &numPos, bool useCutVals = false,
     float *negCutVal = NULL, float *posCutVal = NULL);
  void dumpPoints(const char *filename, FloatVec &xCenters, FloatVec &yCenters, 
                  FloatVec &peakVals, int iz = 3);
  void setMontPieceVectors
    (FloatVecArray *pieceXcenVec, FloatVecArray *pieceYcenVec,
     FloatVecArray *piecePeakVec, FloatVecArray *pieceMeanVec,
     FloatVecArray *pieceSDsVec, FloatVecArray *pieceOutlieVec, IntVec *xpcAliCoords,
     IntVec *ypcAliCoords, IntVec *pieceIndex,
     IntVec *tileXnum, IntVec *tileYnum);
  void setMontageParams
    (int numXpieces, int numYpieces, int rawXsize, int rawYsize, int fullBinning, 
     float pcToPcSameFrac, float pcToFullSameFrac, float substOverlapDistFrac, 
     float usePieceEdgeDistFrac, float addOverlapFrac);
  void assignGridPositions(FloatVec &xCenters, FloatVec &yCenters, ShortVec &gridX,
                           ShortVec &gridY, float &avgAngle, float &avgLen);
  void getGridVectors(float &gridXdX, float &gridXdY, float &gridYdX, float &gridYdY);
  
 private:
  void addToSampleAndQueue(int jx, int iy);
  float goodAngle(float angle, float limit = 90.);
  void corrPointToFullImage(float xIn, float yIn, float &xOut, float &yOut);
  float lengthAndAngle(float baseAng, float dx, float dy, float &angle);
  void scalePositionVectors(FloatVec &xpos, FloatVec &ypos, float scaling);
  void distancesFromOverlap(float xpos, float ypos, int piece, int neigh,
                            float &xZoneDist, float &yZoneDist, int &xWidth, int &yWidth);
  void minDistToOverlapZone(float xpos, float ypos, int piece, 
                            float &minZoneDist, float &maxZoneFrac);
  float minDistanceFromEdge(float xpos, float ypos, int piece);
  bool pointsCloseInBox(float xpos, float ypos, float xneigh, float yneigh, float crit,
                        float circleCritSq);

  int mKeepCache;                // Overall flag for keeping things in caches
  bool mKeepAvgFFTs;             // Flag that average FFTs need to be reusable
  bool mHaveDataFFT;             // Flag that FFT has been taken of edge or raw data
  bool mHaveRawAvgFFT;           // Flag the FFT has been taken of raw average
  bool mHaveEdgeAvgFFT;          // Flag the FFT has been taken of edge average
  int mXsize, mYsize;            // x,y dimensions of image
  int mXpadSize, mYpadSize;      // Padded size of image
  int mPadXdim;                  // X dimension of padded image
  int mRedXoffset, mRedYoffset;  // Offset of reduced image inside original
  int mPadXoffset, mPadYoffset;  // Offset of reduced image inside padded image
  int mRawDataSize;              // xsize * ysize
  float mReduction;              // Reduction factor >= 1
  float *mRawData;               // Reduced image
  float *mFiltData;              // Median-filtered data, held for more iterations
  float *mSobelGrad;             // Gradient data from sobel filter
  unsigned char *mSobelDir;      // Direction values
  unsigned char *mEdgeData;      // Edge image
  float *mCrossCorr;             // Array for cross-correlation
  float *mDataFFT;               // FFT of data (edge image or data)
  IntVec mSampleX, mSampleY;     // Vector of strong edge positions
  std::queue<int> mStrongQueue;     // Queue of strong edge positions to check
  std::vector<float *> mCircleFFTs; // Cache of circle FFTs
  FloatVec mCircleRadii;            // Their radii
  FloatVec mCircleWidths;           // And widths
  float mLastSigmaForCanny;       // Sigma for last filter used
  float mSobelMin;                // Min Value from sobel filter
  float mHistScale;               // Scaling of histogram of sobel values
  int mHistogram[SOBEL_HIST_DIM]; // Histogram for sobel values
  float mPeakAngBelow, mPeakAngAbove;  // Two angles of grid
  float mAvgLenBelow, mAvgLenAbove;    // The grid lengths
  float mJustAngBelow, mJustAngAbove;  // Values from calling just to find spacing
  float mJustLenBelow, mJustLenAbove;
  float mLastBestRadius;          // Last radius found from scans
  float *mEdgeAverage;            // Average from edge image
  float *mRawAverage;             // Average from raw image
  int mAvgBoxSize;                // Size of average (after padding)
  float mRadiusForEdgeAvg;        // Radius at which average was made
  int mUsedWeakEdges;             // Last kind of image or average (1 weak, 2 raw average)
  int mVerbose;                   // For debug printed output
  int mDebugImages;               // For debug image output
  float mFracToAvg;               // Fraction of points to average from edge image
  int mMinNumForAvg;              // Minimum # needed to do average
  float mMinBoostToIter;          // Minimum increase in # of points for iterating average
  float mMaxDiamErrFrac;          // Maximum difference between nominal and found diameter
  float mAvgOutlieCrit;           // Criterion for removing bright outliers from average
  float mFinalPosOutlieCrit;      // Criterion for removing bright holes from final set
  float mFinalNegOutlieCrit;      // Criterion for removing dark holes from final set
  float mFinalOutlieRadFrac;      // Fraction of radius to use for these final removals
  float mLastNegCutoffVal;        // Actual mean cutoff for negative crit from full image
  float mLastPosCutoffVal;        // Actual mean cutoff for positive crit from full image
  bool mLastCutoffsValid;         // Flag that these cutoff values are usable/valid
  float mSpacing;                 // Expected Spacing in reduced pixels
  bool mRetainFFTs;               // Flag to retain FFTs of circles
  float mMaxError;                // Maximum error between predicted and found
  float mDiameter;                // Expected diameter in reduced pixels
  float *mWidths;                 // Array of widths to scan
  float *mIncrements;             // Array of increments in each scan
  int *mNumCircles;               // Array of number of circles each scan
  int mNumScans;                  // Number of scans to run
  float *mSigmas;                 // Array of sigmas/iterations to try
  int mNumSigmas;                 // Number of sigmas
  float *mThresholds;             // Array of thresholds to try
  int mNumThresh;                 // Number of thresholds
  int mMaxFound;                  // Variables for finding the best sigma/threshold
  int mMinMissing;                // calls to runSequence
  float mRadAtBest;
  FloatVecArray *mPieceXcenVec;   // Pointers to vector arrays for piece analysis:
  FloatVecArray *mPieceYcenVec;   // Position, peak values, and whichever statistics are
  FloatVecArray *mPiecePeakVec;   // wanted
  FloatVecArray *mPieceMeanVec;
  FloatVecArray *mPieceSDsVec;
  FloatVecArray *mPieceOutlieVec;
  IntVec *mXpcAliCoords;          // Aligned piece coordinates for the current section
  IntVec *mYpcAliCoords;          // Indexed by piece # being analyzed, as for previous
  IntVec *mPieceIndex;            // Map to piece # on this section from x, y indexes
  IntVec *mTileXnum, *mTileYnum;  // Piece index in regular montage array
  float mPieceSpacing;            // Spacing between pieces in raw piece coordinates
  int mPieceXsize, mPieceYsize;   // Size of raw piece images
  int mNumXpieces, mNumYpieces;   // Number of pieces in X and Y
  int mFullBinning;               // Binning of full image relative to pieces
  float mPcToPcSameFrac;          // see constructor
  float mPcToFullSameFrac;        // see constructor
  float mSubstOverlapDistFrac;    // see constructor
  float mUsePieceEdgeDistFrac;    // see constructor
  float mAddOverlapFrac;          // see constructor
  int mLastDominantInd;           // Index of dominant type of average in full image
  double mWallStart;
  double mWallFFT;
  double mWallPeak;
  double mWallFilter;
  double mWallCanny;
  double mWallCircle;
  double mWallConj;
};

#endif
