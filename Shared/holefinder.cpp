/*
 *  holefinder.cpp - Class for finding regularly spaced holes in carbon films
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 2020 by the Regents of the University of 
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 *  Holefinder was started from Hough transform code obtained from
 *  https://cci.lbl.gov/cctbx_sources/spotfinder/core_toolbox/hough.cpp
 *  which had numerous bugs and inefficiencies.  A few variable and function names
 *  remain from that code.
 *
 *  No ID line: shared with SerialEM
 */
#include <algorithm>
#include "b3dutil.h"
#include "iimage.h"
#include "sliceproc.h"
#include "cppdefs.h"
#include "holefinder.h"
#include "cfft.h"

#define DISTANCE(a, b) sqrt((a) * (a) + (b) * (b))

#define SIDES_ANGLE_ERROR(len1, len2, ang) \
  (float)B3DMAX(0, (len1 * len1 + len2 * len2 - 2.* len1 * len2 * cos(ang * RADIANS_PER_DEGREE)));

#define RETURN_IF_ERR(a) err = (a); if (err) return err;

enum {ERR_MEMORY = 1, ERR_SELECT, ERR_BAD_MODE, ERR_ZOOM_RANGE, ERR_NOT_INIT, 
      ERR_NO_TEMPLATE, ERR_TEMPLATE_PARAM, ERR_NO_CUTOFFS};

static const char *errStrings[] = 
  {"Unknown error from HoleFinder", "Allocating memory", "Selecting zoom-down filter",
   "Data mode of input must be byte, integer, or float", 
   "Coordinates for image reduction are out of range", "Initialize was not called first",
   "No average template was made", "Radius does not match for making template or # "
   "radii is not 1", "NULL cutoff pointers passed to removeOutliers"};

/*
 * Constructor: initialize pointers, set some defaults
 */
HoleFinder::HoleFinder()
{
  mRawData = NULL;
  mFiltData = NULL;
  mSobelGrad = NULL;
  mEdgeData = NULL;
  mDataFFT = NULL;
  mCrossCorr = NULL;
  mEdgeAverage = NULL;
  mRawAverage = NULL;
  mSobelDir = NULL;
  mXsize = 0;
  mXpadSize = 0;

  // 5: Minumum number of total points for making an average
  mMinNumForAvg = 5;
  // 6: Fraction of points to average if there enough; more will be done if < minimum
  mFracToAvg = 0.5;
  // 7: Maximum change between nominal and found diameter allowed; above this it do next 
  // set of circles at the nominal diameter instead of the found one
  mMaxDiamErrFrac = 0.2f;
  // 8: Criterion for omitting positive outliers in mean value from the raw average
  mAvgOutlieCrit = 4.5;
  // 9: Criterion for removal of positive mean outliers after the final grid analysis
  mFinalPosOutlieCrit = 4.5;
  // 10: Criterion for removal of negative mean outliers after the final grid analysis
  mFinalNegOutlieCrit = 9.0;
  // 11: Fraction of radius to take pixels from for this outlier removal
  mFinalOutlieRadFrac = 0.9f;
  // 12: Fraction of average spacing below which points found on separate montage pieces  
  // are considered to be the same
  mPcToPcSameFrac = 0.5;
  // 13: Fraction of average spacing below which a point found on a montage piece is
  // considered to be the same as a point found in the full image
  mPcToFullSameFrac = 0.5;
  // 14: Fraction of radius used as criterion distance from edge of overlap zone for 
  // substituting a point found on a piece for the same point on the full image; 0 means
  // a point right on edge qualifies, a positive value admits points not in overlap
  mSubstOverlapDistFrac = 0.;
  // 15: Fraction of radius used as criterion distance from edge of image for using a 
  // point found only on a piece (adding or substituting it)
  mUsePieceEdgeDistFrac = 0.25;
  // 16: Maximum fractional extent into overlap for adding a point from a piece not found 
  // on the full image (and not eliminated because the it was identified as the same as 
  // one on the overlapping piece)
  mAddOverlapFrac = 0.75;

  // Minimum increase from # of points from edge average correlation to # of points 
  // from raw average correlation required to make new average and iterate (no entry)
  mMinBoostToIter = 1.05f;
  mLastCutoffsValid = false;
  mVerbose = 0;
  mDebugImages = 0;
  mWallFFT = mWallPeak = mWallFilter = 0.;
  mWallCanny = mWallCircle = mWallConj = 0;
}

HoleFinder::~HoleFinder()
{
  //PRINT3(mWallFFT, mWallPeak, mWallFilter);
  //PRINT3(mWallCanny, mWallCircle, mWallConj);
  clearAll();
}

/*
 * Free up the images in the cache of circle FFTs
 */
void HoleFinder::clearCircleCache()
{
  for (int ind = 0; ind < (int)mCircleFFTs.size(); ind++)
    free(mCircleFFTs[ind]);
  mCircleFFTs.clear();
  mCircleRadii.clear();
}

/*
 * Free up other data that are retained for maximum efficiency
 */
void HoleFinder::clearData()
{
  B3DFREE(mRawData);
  B3DFREE(mFiltData);
  B3DFREE(mSobelGrad);
  B3DFREE(mSobelDir);
  B3DFREE(mEdgeData);
  B3DFREE(mDataFFT);
  B3DFREE(mCrossCorr);
  mXsize = 0;
}

/*
 * Free everything and reset some things
 */
void HoleFinder::clearAll()
{
  clearCircleCache();
  clearData();
  B3DFREE(mRawAverage);
  B3DFREE(mEdgeAverage);
  mXpadSize = 0;
  mWallFFT = mWallPeak = mWallFilter = 0.;  
  mWallCanny = mWallCircle = mWallConj = 0;
}

const char *HoleFinder::returnErrorString(int err)
{
  if (err < 0 || err >= sizeof(errStrings) / sizeof(char *))
    err = 0;
  return errStrings[err];
}

/*
 * Initialize the finder for a new image in inputData, with MRC/SLICE_MODE in mode, 
 * size in nxIn, nyIn, and reduction to be applied (value >= 1).
 * maxRadToAnalyze is the maximum radius that will ever be analyzed, which determines th
 * padded size for FFTs.  It is in reduced pixels.
 * keepCache would be 1 to keep basic intermediate images between calls to findCircles,
 * 2 to keep FFTs of circles as well (when the retain flag is also passed on a call to 
 * findCircles), plus 4 to keep a median filter image.
 */
int HoleFinder::initialize(void *inputData, int mode, int nxIn, int nyIn, float reduction,
                           float maxRadToAnalyze, int keepCache)
{
  Islice slice;
  bool reducing = reduction > 1.;
  int nx, ny, xPadSize, yPadSize, dataSize, chan, err;
  unsigned char **linePtrs = NULL;
  bool keepMedian = (keepCache & CACHE_KEEP_MEDIAN) != 0;
  bool keepAvgs = (keepCache & CACHE_KEEP_AVGS) != 0;
  keepCache &= 3;

  // Determine data size if reducing, and the offset, and get the reducing filter
  if (reducing) {
    nx = (int)(nxIn / reduction);
    ny = (int)(nyIn / reduction);
    mRedXoffset = int((nxIn - nx * reduction) / 2.); 
    mRedYoffset = int((nyIn - ny * reduction) / 2.); 
    if (mVerbose)
      PRINT3(reduction, nx, ny);
    if (selectZoomFilter(4, 1. / reduction, &chan))
      return ERR_SELECT;
    mReduction = reduction;
  } else {

    // Or set up for no reducing
    nx = nxIn;
    ny = nyIn;
    mRedXoffset = 0;
    mRedYoffset = 0;
    mReduction = 1.;
  }

  // Get padded sizes based on maximum radius
  xPadSize = niceFrame(2 * (nx / 2) + B3DNINT(2. * maxRadToAnalyze) + 4, 2,
                       niceFFTlimit());
  yPadSize = niceFrame(2 * (ny / 2) + B3DNINT(2. * maxRadToAnalyze) + 4, 2,
                       niceFFTlimit());
  mPadXdim = xPadSize + 2;
  mPadXoffset = (xPadSize - nx) / 2;
  mPadYoffset = (yPadSize - ny) / 2;
  if (dataSizeForMode(mode, &dataSize, &chan))
    return ERR_BAD_MODE;

  // Get allocations if data has changed size
  err = 0;
  if (nx != mXsize || ny != mYsize) {
    clearData();
    if (reducing || mode == MRC_MODE_FLOAT) {
      mRawData = (float *)malloc(nx * ny * dataSize);
      if (!mRawData)
        err = ERR_MEMORY;
    }

    if (!err) {
      mSobelGrad = B3DMALLOC(float, nx * ny);
      mEdgeData = B3DMALLOC(unsigned char, nx * ny);
      if (keepCache)
        mSobelDir = B3DMALLOC(unsigned char, nx * ny);
      if (keepMedian)
        mFiltData = B3DMALLOC(float, nx * ny);
      if (!mEdgeData || !mSobelGrad || (keepCache && !mSobelDir) || 
          (keepMedian && !mFiltData))
        err = ERR_MEMORY;
    }
  }
  if (!keepCache)
    B3DFREE(mSobelDir);
  if (!keepMedian)
    B3DFREE(mFiltData);
  
  // Clear out pad-dependent items if that size has changed
  if (!err && (xPadSize != mXpadSize || yPadSize != mYpadSize)) {
    clearCircleCache();
    B3DFREE(mDataFFT);
    B3DFREE(mCrossCorr);
  }

  // Allocate pad arrays: FFT and also corr array if keeping any cache
  if (!err && !mDataFFT) {
    mDataFFT = B3DMALLOC(float, mPadXdim * yPadSize);
    if (!mDataFFT)
      err = ERR_MEMORY;
  }
  if (!err && keepCache && !mCrossCorr) {
    mCrossCorr = B3DMALLOC(float, mPadXdim * yPadSize);
    if (!mCrossCorr)
      err = ERR_MEMORY;
  }
  if (!keepCache)
    B3DFREE(mCrossCorr);
  if (keepCache < 2)
    clearCircleCache();
  
  // If not reducing, either copy float data or convert the raw data to float and assign
  if (!err && !reducing) {
    if (mode == MRC_MODE_FLOAT) {
      memcpy(mRawData, inputData, nx * ny * sizeof(float));
    } else {
      B3DFREE(mRawData);
      if (sliceInit(&slice, nx, ny, mode, inputData) || sliceFloatEx(&slice, 0) < 0)
        err = ERR_MEMORY;
      else
        mRawData = slice.data.f;
    }
  } else if (!err) {

    // If reducing, make line pointers and reduce
    linePtrs = makeLinePointers(inputData, nxIn, nyIn, dataSize);
    if (!linePtrs) {
      err = ERR_MEMORY;
    } else {
      err = zoomWithFilter(linePtrs, nxIn, nyIn, (float)mRedXoffset, (float)mRedYoffset, 
                           nx, ny, nx, 0, mode, mRawData, NULL, NULL);
      free(linePtrs);

      // Only errors 4 and 5 should happen
      if (err == 4)
        err = ERR_ZOOM_RANGE;
      else if (err)
        err = ERR_MEMORY;
    }
    if (!err && mode != MRC_MODE_FLOAT) {
      if (sliceInit(&slice, nx, ny, mode, mRawData) || sliceFloatEx(&slice, 1) < 0)
        err = ERR_MEMORY;
      else
        mRawData = slice.data.f;
    }
  }
  if (mDebugImages)
    mrcWriteImageToFile("reduced.mrc", mRawData, MRC_MODE_FLOAT, nx, ny);

  // Bail out if error, or 
  if (err) {
    clearAll();
    return err;
  }

  mHaveDataFFT = false;
  if (!keepAvgs) {
    mHaveRawAvgFFT = false;
    mHaveEdgeAvgFFT = false;
  }
  mKeepAvgFFTs = keepAvgs;
  mLastSigmaForCanny = -999.;
  mXsize = nx;
  mYsize = ny;
  mXpadSize = xPadSize;
  mYpadSize = yPadSize;
  mRawDataSize = nx * ny;
  mKeepCache = keepCache;
  return 0;
}

/*
 * Set the parameters for running the high-level sequence routine
 * diameter of holes in reduced pixels
 * spacing between holes in reduced pixels
 * maxError, maximum error from local predicted position, in original pixels
 * For other parameters, pass -1 to use the default in this module
 */
void HoleFinder::setSequenceParams
(float diameter, float spacing, bool retain, float maxError, float fracAvg, int minForAvg,
 float maxDiamErrFrac, float avgOutlieCrit, float finalNegOutlieCrit, 
 float finalPosOutlieCrit, float finalOutlieRadFrac)
{
  mSpacing = spacing;
  mRetainFFTs = retain;
  mMaxError = maxError;
  mDiameter = diameter;
  if (fracAvg > 0)
    mFracToAvg = fracAvg;
  if (minForAvg > 0)
    mMinNumForAvg = minForAvg;
  if (maxDiamErrFrac > 0.)
    mMaxDiamErrFrac = maxDiamErrFrac;
  if (avgOutlieCrit > 0.)
    mAvgOutlieCrit = avgOutlieCrit;
  if (finalNegOutlieCrit >= 0.)
    mFinalNegOutlieCrit = finalNegOutlieCrit;
  if (finalPosOutlieCrit >= 0.)
    mFinalPosOutlieCrit = finalPosOutlieCrit;
  if (finalOutlieRadFrac > 0)
    mFinalOutlieRadFrac = finalOutlieRadFrac;
}

/*
 * Pass variables determining what analyses are run in the sequence
 * increments, widths, and numCircles are step sizes, thicknesses, and number of steps
 * to pass to findCircles
 * numScans is the number of those parameters to run
 * sigmas has the set of numSigmas values to try, in pixels
 * thresholds has numThresh values to try, in percent of brightest edge pixels
 */
void HoleFinder::setRunsInSequence
(float *increments, float *widths, int *numCircles, int numScans, float *sigmas,
 int numSigmas, float *thresholds, int numThresh)
{
  mWidths = widths;
  mIncrements = increments;
  mNumCircles = numCircles;
  mNumScans = numScans;
  mSigmas = sigmas;
  mNumSigmas = numSigmas;
  mThresholds = thresholds;
  mNumThresh = numThresh;
}

/*
 * Run a sequence of edge correlations with different thicknesses to find best radius and
 * initial positions followed by correlating with a template based on averaging the edge 
 * or the raw image, analyzing positions and angle to find the regular grid of points.
 * Do this at an array of sigma and threshold values and pick the best one for the final
 * correlations and analysis.
 * Set iSig and iThresh 0 on the first call and continue to call until it no longer
 * returns a -1.  sigUsed and threshUsed are returned with the values used on the
 * call, or the final values.
 * xBoundary, yBoundary have a possible boundary contour
 * bestSigInd and bestThreshInd are returned with the index of the best sigma and 
 * threshold values
 * bestRadius is returned in reduced pixels
 * trueSpacing is returned in original pixels
 * xCenters, yCenters, peakVals are the found peaks and their correlation strengths
 * xMissing, yMissing are predicted positions that have no point
 * xCenClose, yCenClose, peakClose are possible peak positions within twice the error
 * limit of the predicted position
 * numMissAdded is returned with the number of pissing points added after correlating
 * with weak edge signals.
 */
int HoleFinder::runSequence
(int &iSig, float &sigUsed, int &iThresh, float &threshUsed, FloatVec &xBoundary,
 FloatVec &yBoundary, int &bestSigInd, int &bestThreshInd, float &bestRadius,
 float &trueSpacing, FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
 FloatVec &xMissing, FloatVec &yMissing, FloatVec &xCenClose, FloatVec &yCenClose,
 FloatVec &peakClose, int &numMissAdded)
{
  int scan, err, numNeg, numPos;
  float midRadius, radInc;
  bool madeAverage, usedRaw, varying = mNumSigmas > 1 || mNumThresh > 1;
  FloatVec holeMeans;

  if (!mRawData)
    return ERR_NOT_INIT;

  // Run the variations: loop on sigma and thresholds
  if (!iSig && !iThresh) {
    xMissing.clear();
    xCenClose.clear();
    bestSigInd = -1;
  }
  
  midRadius = mDiameter / 2.f;
      
  // Get the edges
  err = cannyEdge(mSigmas[iSig], 1.f - 0.02f * mThresholds[iThresh], 
                  1.f - 0.01f * mThresholds[iThresh]);
  if (err)
    return err;

  // Find the circles in series of scans
  for (scan = 0; scan < mNumScans; scan++) {
    radInc = mIncrements[scan];
    err = findCircles(midRadius, radInc, mWidths[scan], mNumCircles[scan], 
                      mRetainFFTs && !scan, 0.75f * mSpacing, 0, 
                      xBoundary, yBoundary, bestRadius, xCenters,
                      yCenters, peakVals);
    if (err)
      return err;
    if (fabs((midRadius - bestRadius) / midRadius) <= mMaxDiamErrFrac)
      midRadius = bestRadius;
  }

  // Make template if possible and analyze the grid
  err = templateAndAnalyze(bestRadius, mRetainFFTs && !varying, madeAverage, usedRaw,
                           xBoundary, yBoundary, xCenters, yCenters,
                           peakVals, trueSpacing, xMissing, yMissing);
  if (err)
    return err;

  // Take new best parameters
  if (mVerbose && varying)
    PRINT4(mSigmas[iSig], mThresholds[iThresh], xCenters.size(), xMissing.size());
  threshUsed = mThresholds[iThresh];
  sigUsed = mSigmas[iSig];
  if (bestSigInd < 0 || (int)xCenters.size() > mMaxFound || 
      (xCenters.size() == mMaxFound && (int)xMissing.size() < mMinMissing)) {
    mMaxFound = (int)xCenters.size();
    mMinMissing = (int)xMissing.size();
    bestSigInd = iSig;
    bestThreshInd = iThresh;
    mRadAtBest = bestRadius;
  } 
  iThresh++;
  if (iThresh < mNumThresh)
    return -1;
  iSig++;
  iThresh = 0;
  if (iSig < mNumSigmas)
    return -1;

  midRadius = mRadAtBest;
  if (mVerbose && varying)
    PRINT2(mSigmas[bestSigInd], mThresholds[bestThreshInd]);
  sigUsed = mSigmas[bestSigInd];
  threshUsed = mThresholds[bestThreshInd];

  // Get the edges and repeat final analysis unless the last one made is for best values
  if (bestSigInd != mNumSigmas - 1 || bestThreshInd != mNumThresh - 1) {
    err = cannyEdge(mSigmas[bestSigInd], 1.f - 0.02f * mThresholds[bestThreshInd], 
                    1.f - 0.01f * mThresholds[bestThreshInd]);
    if (err)
      return err;

    err = findCircles(midRadius, radInc, mWidths[mNumScans - 1], 1, false,
                      0.75f * mSpacing, 0, xBoundary, yBoundary, bestRadius, xCenters,
                      yCenters, peakVals);
    if (err)
      return err;

    //  do template and sort out neighbors
    err = templateAndAnalyze(midRadius, mRetainFFTs, madeAverage, usedRaw, xBoundary,
                             yBoundary, xCenters, yCenters, peakVals, trueSpacing, 
                             xMissing, yMissing);
    if (err)
      return err;
  }

  numMissAdded = 0;
  if (madeAverage) {
    if (mVerbose)
      printf("Finding circles with weak edges for rescue (%d missing)\n", 
             (int)xMissing.size());
    err = findCircles(midRadius, 1., -1, 1, false, 0.75f * mSpacing, 1, xBoundary,
                    yBoundary, bestRadius, xCenClose, yCenClose, peakClose);
    if (err)
      return err;

    err = rescueMissingPoints(xCenters, yCenters, peakVals, xMissing, yMissing,
                              xCenClose, yCenClose, peakClose, mMaxError, numMissAdded);
  }

  if (mVerbose)
    printf("Added %d points, still %d missing\n", numMissAdded, (int)xMissing.size());
  
  // Get rid of outliers again
  mLastCutoffsValid = false;
  if (xCenters.size() >= 10 && (mFinalPosOutlieCrit > 0. || mFinalNegOutlieCrit > 0.)) {
    holeIntensityStats(mFinalOutlieRadFrac * bestRadius, xCenters, yCenters, false,
                       false, &holeMeans, NULL, NULL);
    RETURN_IF_ERR(removeOutliers(xCenters, yCenters, &peakVals, holeMeans, 
                                 mFinalNegOutlieCrit, mFinalPosOutlieCrit, numNeg, 
                                 numPos, false, &mLastNegCutoffVal, &mLastPosCutoffVal));
    if (mVerbose)
      printf("%d positive and %d negative outliers removed\n", numPos, numNeg);
    mLastCutoffsValid = true;
  }
  return err;
}

/*
 * Makes an average from the edge image finds circles from that, and runs the grid 
 * analysis to get the real set of points.  Then it tries to get an average from the raw
 * image and find circles with that, followed by grid analysis.  Merges the output of
 * those two approaches and analyzes again to get the best of both worlds.
 * midRadius is the radius to use in a findCircles call to correlate with the template,
 * in reduced pixels.
 * madeAverage is returned with whether and average was made, and usedRaw if one was made
 * from raw images.
 * xBoundary, yBoundary have the possible boundary contour
 * xCenters, yCenters, peakVals are passed in with the previously found peaks and their 
 * correlation strengths and returned with the resulting points
 * xMissing, yMissing are returned with predicted positions that have no point
 * trueSpacing is returned with the found spacing in original pixels
 * All vectors contain original pixels
 */
int HoleFinder::templateAndAnalyze
(float midRadius, bool retain, bool &madeAverage, bool &usedRaw, FloatVec &xBoundary,
 FloatVec &yBoundary, FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
 float &trueSpacing, FloatVec &xMissing, FloatVec &yMissing)
{
  int numAvg, err, loop, domInd, domSize, dom, numPos;
  float bestRadius, fracToAvg = mFracToAvg;
  float trueSpaceRaw, maxSpacing = 1.33f * mSpacing * mReduction;
  float minSpacing = 0.75f * mSpacing;
  FloatVec xCenFCedge,yCenFCedge, peakFCedge;
  FloatVec xSaveMiss, ySaveMiss, xCenRawCorr, yCenRawCorr, peakRawCorr;
  FloatVec xMissRawCorr, yMissRawCorr, xCenFCraw, yCenFCraw, peakFCraw;
  FloatVec xForTemplate, yForTemplate, holeMeans;
  IntVec altInds;
  FloatVec *xcenPtr[2] = {&xCenFCedge, &xCenFCraw};
  FloatVec *ycenPtr[2] = {&yCenFCedge, &yCenFCraw};
  FloatVec *peakPtr[2] = {&peakFCedge, &peakFCraw};
  bool enoughLeft;
  float firstAngBelow, firstAngAbove, firstLenBelow, firstLenAbove;

  mWallStart = wallTime();
  usedRaw = false;
  mLastDominantInd = -1;

  // Make template and correlate with that if enough points
  madeAverage = (int)xCenters.size() >= mMinNumForAvg;
  if (madeAverage) {
    numAvg = B3DNINT(fracToAvg * xCenters.size());
    if (numAvg < mMinNumForAvg)
      numAvg = B3DNINT((1. - 0.5 * (1. - fracToAvg)) * xCenters.size());
    err = makeTemplate(xCenters, yCenters, numAvg, mSpacing, false);
    if (err)
      return err;
    err = findCircles(midRadius, 1., -1., 1, retain, minSpacing, 0, xBoundary,
                      yBoundary, bestRadius, xCenters, yCenters, peakVals);
    if (err)
      return err;
    xCenFCedge = xCenters;
    yCenFCedge = yCenters;
    peakFCedge = peakVals;
  }
  //printf("Analyzing neighbors %s making average\n", madeAverage? "after" : "without");
  
  // Analyze for regular grid
  //dumpPoints("edgeorig.txt", xCenters, yCenters, peakVals);
  if (xCenters.size() < 2) {
    trueSpacing = 0.;
    return 0;
  }
  err = analyzeNeighbors(xCenters, yCenters, peakVals, altInds, xCenFCraw, yCenFCraw,
                         peakFCraw, maxSpacing, mMaxError, 0, trueSpacing, xMissing,
                         yMissing);
  firstAngBelow = mPeakAngBelow;
  firstAngAbove = mPeakAngAbove;
  firstLenBelow = mAvgLenBelow;
  firstLenAbove = mAvgLenAbove;
  mWallConj += wallTime() - mWallStart;

  //dumpPoints("edgeana.txt", xCenters, yCenters, peakVals);

  // Now make a template from the raw image if there are enough for that, or just use
  // the last one if there is one around
  enoughLeft = (int)xCenters.size() >= mMinNumForAvg;
  if (madeAverage && (enoughLeft || mRawAverage)) {
    xCenRawCorr = xCenters;
    yCenRawCorr = yCenters;
    fracToAvg = 0.95f;
    for (loop = 0; loop < (enoughLeft ? 2 : 1); loop++) {

      // If enough, make template
      if (enoughLeft) {
        xForTemplate = xCenRawCorr;
        yForTemplate = yCenRawCorr;

        // First remove positive outliers to try to pull in more holes with ice when there
        // are a lot of empty one
        if ((int)xCenRawCorr.size() >= B3DMAX(10, 2 * mMinNumForAvg)) {
          holeIntensityStats(bestRadius * 0.9f, xForTemplate, yForTemplate, false,
                             false, &holeMeans, NULL, NULL);
          RETURN_IF_ERR(removeOutliers(xForTemplate, yForTemplate, NULL, holeMeans, 0., 
                                       mAvgOutlieCrit, dom, numPos));
          if (mVerbose && numPos)
            printf("Removed %d outliers for template\n", numPos);
        }

        // Get number to average after that
        numAvg = B3DNINT(fracToAvg * xForTemplate.size());
        if (numAvg < mMinNumForAvg)
          numAvg = B3DNINT((1. - 2. * (1. - fracToAvg)) * xForTemplate.size());
        err = makeTemplate(xForTemplate, yForTemplate, numAvg, mSpacing, true);
        if (err)
          return err;
      }

      // Find points with the template and analyze
      err = findCircles(midRadius, 1., -2., 1, retain, minSpacing, 2, xBoundary,
                        yBoundary, bestRadius, xCenRawCorr, yCenRawCorr, peakRawCorr);
      if (err)
        return err;
      xCenFCraw = xCenRawCorr;
      yCenFCraw = yCenRawCorr;
      peakFCraw = peakRawCorr;
      //dumpPoints("raworig.txt", xCenRawCorr, yCenRawCorr, peakRawCorr);
      if (xCenRawCorr.size() > 0)
        err = analyzeNeighbors(xCenRawCorr, yCenRawCorr, peakRawCorr, altInds, xCenFCraw,
                             yCenFCraw, peakFCraw, maxSpacing,
                             mMaxError, 0, trueSpaceRaw, xMissRawCorr, yMissRawCorr);
      if (err)
        return err;
      //dumpPoints("rawana.txt", xCenRawCorr, yCenRawCorr, peakRawCorr);

      // Loop again if this increased the # available enough
      if (!loop && xCenRawCorr.size() < mMinBoostToIter * xCenters.size())
        break;
    }

    domInd = xCenRawCorr.size() >= xCenters.size() ? 1 : 0;
    mLastDominantInd = domInd;
    if (mVerbose)
      PRINT3(domInd, xCenRawCorr.size(), xCenters.size());

    // Now merge the two original lists from before the analysis, 
    // merge into the dominant list if not too close to an existing point
    domSize = (int)xcenPtr[domInd]->size();
    mergeAlternateLists(xcenPtr, ycenPtr, peakPtr, altInds, domInd, minSpacing, 
                        mMaxError);

    // If nothing merged in, take the dominant set, namely copy the data if it is 1
    if (xcenPtr[domInd]->size() == domSize) {
      if (domInd) {
        xCenters = xCenRawCorr;
        yCenters = yCenRawCorr;
        peakVals = peakRawCorr;
        xMissing = xMissRawCorr;
        yMissing = yMissRawCorr;
        trueSpacing = trueSpaceRaw;
        usedRaw = true;
      } else {
        mPeakAngBelow = firstAngBelow;
        mPeakAngAbove = firstAngAbove;
        mAvgLenBelow = firstLenBelow;
        mAvgLenAbove = firstLenAbove;
      }
    } else {

      // Otherwise analyze the merged list and assign it to the output
      //dumpPoints("mergeorig.txt", *xcenPtr[domInd], *ycenPtr[domInd], *peakPtr[domInd]);
      RETURN_IF_ERR(analyzeNeighbors(*xcenPtr[domInd], *ycenPtr[domInd], *peakPtr[domInd],
                                     altInds, *xcenPtr[1 - domInd], *ycenPtr[1 - domInd],
                                     *peakPtr[1 - domInd], maxSpacing, mMaxError, 0, 
                                     trueSpacing, xMissing, yMissing));
      //dumpPoints("mergeana.txt", *xcenPtr[domInd], *ycenPtr[domInd], *peakPtr[domInd]);
      //printf("Analyzed merged, got %d\n", xcenPtr[domInd]->size());
      xCenters = *xcenPtr[domInd];
      yCenters = *ycenPtr[domInd];
      peakVals = *peakPtr[domInd];
      usedRaw = true;
    }
  }

  return err;
}

/*
 * Merges the "alternate" list of points into the "dominant" one that is being accepted as
 * best, by adding points that are farther than the minSpacing (in reduced pixels). 
 * Points that are sufficiently close (within 4 times maxError, which is in the same
 * pixels as the points) are indexed in altInd for access as alternatives during analysis
 */
void HoleFinder::mergeAlternateLists(FloatVec **xcenPtr, FloatVec **ycenPtr,
                                     FloatVec **peakPtr, IntVec &altInds, int domInd,
                                     float minSpacing, float maxError)
{
  int domSize, dom, ind;
  float dx, dy, xx, yy, spaceCrit, errCrit, distSq;
  bool doMerge;
  domSize = (int)xcenPtr[domInd]->size();
  spaceCrit = pow(minSpacing * mReduction, 2.f);
  errCrit = (float)pow(4. * maxError, 2.);
  errCrit = B3DMIN(errCrit, spaceCrit);
  altInds.resize(domSize, -1);
  for (ind = 0; ind < (int)xcenPtr[1 - domInd]->size(); ind++) {
    doMerge = true;
    xx = xcenPtr[1 - domInd]->at(ind);
    yy = ycenPtr[1 - domInd]->at(ind);
    for (dom = 0; dom < domSize; dom++) {
      dx = xcenPtr[domInd]->at(dom) - xx;
      dy = ycenPtr[domInd]->at(dom) - yy;
      distSq = dx * dx + dy * dy;

      // Keep track of point in the other set that may be useful alternative
      if (distSq < errCrit)
        altInds[dom] = ind;

      // Throw away point that is too close
      if (distSq < spaceCrit) {
        doMerge = false;
        break;
      }
    }

    // Or merge the point
    if (doMerge) {
      xcenPtr[domInd]->push_back(xx);
      ycenPtr[domInd]->push_back(yy);
      peakPtr[domInd]->push_back(peakPtr[1 - domInd]->at(ind));
      altInds.push_back(-1);
    }
  }
}

/*
 * Pass pointers to various vectors needed when analyzing a montage piece.
 * pieceMeanVec, pieceSDsVec, and pieceOutlieVec can be NULL
 * xpcAliCoords, ypcAliCoords should be a set of coordinates for the section being
 * analyzed and indexed by the same piece number that the other arrays use.
 * pieceIndex is a map from the piece at (ix, iy) to the piece numbers for this section
 */
void HoleFinder::setMontPieceVectors
(FloatVecArray *pieceXcenVec, FloatVecArray *pieceYcenVec, FloatVecArray *piecePeakVec,
 FloatVecArray *pieceMeanVec, FloatVecArray *pieceSDsVec, 
 FloatVecArray *pieceOutlieVec, IntVec *xpcAliCoords, IntVec *ypcAliCoords,
 IntVec *pieceIndex, IntVec *tileXnum, IntVec *tileYnum)
{
  mPieceXcenVec = pieceXcenVec;
  mPieceYcenVec = pieceYcenVec;
  mPiecePeakVec = piecePeakVec;
  mPieceMeanVec = pieceMeanVec;
  mPieceSDsVec = pieceSDsVec;
  mPieceOutlieVec = pieceOutlieVec;
  mXpcAliCoords = xpcAliCoords;
  mYpcAliCoords = ypcAliCoords;
  mPieceIndex = pieceIndex;
  mTileXnum = tileXnum;
  mTileYnum = tileYnum;
}

/*
 * Set common parameters needed for piece analysis: number of pieces in X and Y,
 * piece size in raw images, binning of full image relative to pieces.
 * Pass < 0 for other parameters to use defaults in this module
 */
void HoleFinder::setMontageParams
(int numXpieces, int numYpieces, int rawXsize, int rawYsize, int fullBinning,
 float pcToPcSameFrac, float pcToFullSameFrac, float substOverlapDistFrac, 
 float usePieceEdgeDistFrac, float addOverlapFrac)
{
  mPieceXsize = rawXsize;
  mPieceYsize = rawYsize;
  mNumXpieces = numXpieces;
  mNumYpieces = numYpieces;
  mFullBinning = fullBinning;
  if (pcToPcSameFrac >= 1.)
    mPcToPcSameFrac = pcToPcSameFrac;
  if (pcToFullSameFrac >= 1.)
    mPcToFullSameFrac = pcToFullSameFrac;
  if (substOverlapDistFrac > -1.)
    mSubstOverlapDistFrac = substOverlapDistFrac;
  if (usePieceEdgeDistFrac > -1.)
  mUsePieceEdgeDistFrac = usePieceEdgeDistFrac;
  if (addOverlapFrac > -1.)
    mAddOverlapFrac = addOverlapFrac;
}


/*
 * Filter a montage piece with the given sigma and threshold, run findCircle with current
 * templates on it, analyze the grid, apply the full-image outlier cutoffs, and
 * get statistics for whatever vectors were supplied
 * spacing is in reduced pixels as before, it will be stored in piece pixels as well
 */
int HoleFinder::processMontagePiece
(float sigma, float threshold, float spacing, int pieceNum, FloatVec &xBoundary,
 FloatVec &yBoundary, float intensityRad, int &numFromCorr)
{
  FloatVec xBoundOnPc, yBoundOnPc;
  float bestRadius, trueSpacing;
  float minSpacing = 0.75f * spacing, maxSpacing;
  int ind, err, pcXoffset, pcYoffset, domSize, numNeg, numPos;
  int domInd = mLastDominantInd;
  FloatVec xCenFCedge,yCenFCedge, peakFCedge;
  FloatVec xCenRawCorr, yCenRawCorr, peakRawCorr;
  FloatVec xMissRawCorr, yMissRawCorr, xCenFCraw, yCenFCraw, peakFCraw;
  FloatVec holeMeans, xMissing, yMissing;
  IntVec altInds;
  FloatVec *xcenPtr[2] = {&xCenFCedge, &xCenFCraw};
  FloatVec *ycenPtr[2] = {&yCenFCedge, &yCenFCraw};
  FloatVec *peakPtr[2] = {&peakFCedge, &peakFCraw};
  bool retain = mKeepCache > 1 && mKeepAvgFFTs;

  mPieceSpacing = spacing * mReduction;
  maxSpacing = 1.33f * mPieceSpacing;
  if (!mRawData)
    return ERR_NOT_INIT;
  if (!mEdgeAverage || (domInd >= 0 && !mRawAverage))
    return ERR_NO_TEMPLATE;

  // Adjust the boundary contour to coords on piece
  pcXoffset = mXpcAliCoords->at(pieceNum);
  pcYoffset = mYpcAliCoords->at(pieceNum);
  if (xBoundary.size()) {
    for (ind = 0; ind < (int)xBoundary.size(); ind++) {
      xBoundOnPc.push_back(xBoundary[ind] - pcXoffset);
      yBoundOnPc.push_back(yBoundary[ind] - pcYoffset);
    }
  }

  // Get edges and find circles from edge average, then from raw average if done before
  RETURN_IF_ERR(cannyEdge(sigma, 1.f - 0.02f * threshold, 1.f - 0.01f * threshold));
  
  RETURN_IF_ERR(findCircles(mRadiusForEdgeAvg, 1., -1., 1, retain, minSpacing, 0, 
                            xBoundOnPc, yBoundOnPc, bestRadius, xCenFCedge, yCenFCedge,
                            peakFCedge));
  if (domInd >= 0) {
    RETURN_IF_ERR(findCircles(mRadiusForEdgeAvg, 1., -2., 1, retain, minSpacing, 2, 
                              xBoundOnPc, yBoundOnPc, bestRadius, xCenFCraw, yCenFCraw,
                              peakFCraw));
    mergeAlternateLists(xcenPtr, ycenPtr, peakPtr, altInds, domInd, minSpacing, 
                        mMaxError * mFullBinning);
  } else
    domInd = 0;

  // Do the standard analysis of grid
  numFromCorr = (int)xcenPtr[domInd]->size();
  RETURN_IF_ERR(analyzeNeighbors(*xcenPtr[domInd], *ycenPtr[domInd], *peakPtr[domInd],
                                 altInds, *xcenPtr[1 - domInd], *ycenPtr[1 - domInd],
                                 *peakPtr[1 - domInd], maxSpacing, 
                                 mMaxError * mFullBinning, 
                                 mFullBinning, trueSpacing, xMissing, yMissing));

  // Apply same cutoffs for outliers as in global image
  if (mLastCutoffsValid && xcenPtr[domInd]->size()) {
    holeMeans.resize(xcenPtr[domInd]->size());
    holeIntensityStats(mFinalOutlieRadFrac * mRadiusForEdgeAvg, *xcenPtr[domInd], 
                       *ycenPtr[domInd], false, false, &holeMeans, NULL, NULL);
    RETURN_IF_ERR(removeOutliers(*xcenPtr[domInd], *ycenPtr[domInd], peakPtr[domInd],
                                 holeMeans, mFinalNegOutlieCrit, mFinalPosOutlieCrit,
                                 numNeg, numPos, true, &mLastNegCutoffVal, 
                                 &mLastPosCutoffVal));
  }

  domSize = (int)xcenPtr[domInd]->size();
  if (!domSize)
    return 0;
    
  // resize all the vectors, convert to global unbinned coordinates and get stats
  mPieceXcenVec->at(pieceNum).resize(domSize);
  mPieceYcenVec->at(pieceNum).resize(domSize);
  mPiecePeakVec->at(pieceNum).resize(domSize);
  if (mPieceMeanVec)
    mPieceMeanVec->at(pieceNum).resize(domSize);
  if (mPieceSDsVec)
    mPieceSDsVec->at(pieceNum).resize(domSize);
  if (mPieceOutlieVec)
    mPieceOutlieVec->at(pieceNum).resize(domSize);
  for (ind = 0; ind < domSize; ind++) {
    mPieceXcenVec->at(pieceNum)[ind] = xcenPtr[domInd]->at(ind) + pcXoffset;
    mPieceYcenVec->at(pieceNum)[ind] = ycenPtr[domInd]->at(ind) + pcYoffset;
    mPiecePeakVec->at(pieceNum)[ind] = peakPtr[domInd]->at(ind);
  }

  if (intensityRad > 0 && (mPieceMeanVec || mPieceSDsVec || mPieceOutlieVec)) {
    holeIntensityStats(intensityRad, *xcenPtr[domInd], *ycenPtr[domInd], false, true,
                       mPieceMeanVec ? &mPieceMeanVec->at(pieceNum) : NULL,
                       mPieceSDsVec ? &mPieceSDsVec->at(pieceNum) : NULL,
                       mPieceOutlieVec ? &mPieceOutlieVec->at(pieceNum) : NULL);
  }
  return 0;
}

/*
 * Use the hole positions found on all the pieces to add to positions from the full image
 * and to correct the positions within overlap zones in case pieces did not line up
 * there.
 */
void HoleFinder::resolvePiecePositions
(bool removeDups, FloatVec &xCenters, FloatVec &yCenters, FloatVec &peakVals,
 FloatVec &xMissing, FloatVec &yMissing, FloatVec &xCenClose, FloatVec &yCenClose,
 FloatVec &peakClose, IntVec &pieceOn, FloatVec &xInPiece, FloatVec &yInPiece,
 FloatVec *holeMeans, FloatVec *holeSDs, FloatVec *holeOutlies)
{
  std::vector< std::vector<bool> > eligible;
  float radius = mReduction * mLastBestRadius;
  float pcToPcSameCrit = mPieceSpacing * mPcToPcSameFrac;
  float pcToFullSameCrit = mPieceSpacing * mPcToFullSameFrac;
  float useEdgeDist = radius * mUsePieceEdgeDistFrac;
  float substOverlapDist = radius * mSubstOverlapDistFrac;
  float critSq, xpos, ypos, xZoneDist, yZoneDist;
  float xneigh, yneigh, minZoneDist, maxEdgeDist, dist, maxZoneFrac;
  int ind, jnd, piece, neigh, xt, yt, numPieces, numFullOrig, zoneXwidth, zoneYwidth;
  int pieceAtMax, outInd;
  bool notClose;

  numPieces = (int)mTileXnum->size();
  numFullOrig = (int)xCenters.size();
  pieceOn.clear();
  xInPiece.clear();
  yInPiece.clear();
  pieceOn.resize(numFullOrig, -1);
  xInPiece.resize(numFullOrig, -1);
  yInPiece.resize(numFullOrig, -1);

  eligible.resize(mTileXnum->size());
  for (ind = 0; ind < numPieces; ind++)
    if (mPieceXcenVec->at(ind).size())
      eligible[ind].resize(mPieceXcenVec->at(ind).size(), true);

  // Analyze everything in unbinned coordinates
  scalePositionVectors(xCenters, yCenters, (float)mFullBinning);
  scalePositionVectors(xMissing, yMissing, (float)mFullBinning);
  scalePositionVectors(xCenClose, yCenClose, (float)mFullBinning);

  // Loop through each piece looking at points still eligible
  critSq = 2.f * pcToPcSameCrit * pcToPcSameCrit;
  for (piece = 0; piece < numPieces; piece++) {
    for (ind = 0; ind < (int)mPieceXcenVec->at(piece).size(); ind++) {
      if (eligible[piece][ind]) {
        xpos = mPieceXcenVec->at(piece)[ind];
        ypos = mPieceYcenVec->at(piece)[ind];
        
        // Loop over adjacent tiles
        for (yt = B3DMAX(0, mTileYnum->at(piece) - 1); eligible[piece][ind] &&
               yt <= (B3DMIN(mNumYpieces - 1, mTileYnum->at(piece) + 1)); yt++) {
          for (xt = B3DMAX(0, mTileXnum->at(piece) - 1); eligible[piece][ind] &&
                 xt <= (B3DMIN(mNumXpieces - 1, mTileXnum->at(piece) + 1)); xt++) {
            neigh = mPieceIndex->at(xt + yt * mNumXpieces);
            if (neigh == piece || neigh < 0)
              continue;

            // Get distance from overlap zone with each neighbor: if it is too big, there
            // is nothing to test
            distancesFromOverlap(xpos, ypos, piece, neigh, xZoneDist, yZoneDist, 
                                 zoneXwidth, zoneYwidth);
            if (xZoneDist > radius && yZoneDist > radius)
              continue;

            // Now loop on points in tile to find close one
            for (jnd = 0; jnd < (int)mPieceXcenVec->at(neigh).size(); jnd++) {
              if (eligible[neigh][jnd]) {
                xneigh = mPieceXcenVec->at(neigh)[jnd];
                yneigh = mPieceYcenVec->at(neigh)[jnd];
                if (pointsCloseInBox(xpos, ypos, xneigh, yneigh, pcToPcSameCrit, 
                                        critSq)) {
                  
                  // The points are ostensibly the same: see which is more interior
                  // and eliminate the other
                  if (minDistanceFromEdge(xpos, ypos, piece) >= 
                      minDistanceFromEdge(xneigh, yneigh, neigh)) {
                    eligible[neigh][jnd] = false;
                    if (mVerbose)
                      printf("Eliminated %.1f %.1f in %d in favor of %.1f %.1f in %d\n",
                             xneigh, yneigh, neigh, xpos, ypos, piece);
                  } else {
                    eligible[piece][ind] = false;
                    if (mVerbose)
                      printf("Eliminated %.1f %.1f in %d in favor of %.1f %.1f in %d\n",
                             xpos, ypos, piece, xneigh, yneigh, neigh);
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Now loop again finding out if there is a close point in the global set
  critSq = 2.f * pcToFullSameCrit * pcToFullSameCrit;
  for (piece = 0; piece < numPieces; piece++) {
    outInd = 0;
    for (ind = 0; ind < (int)mPieceXcenVec->at(piece).size(); ind++) {
      if (eligible[piece][ind]) {
        xpos = mPieceXcenVec->at(piece)[ind];
        ypos = mPieceYcenVec->at(piece)[ind];

        // Find minimum distance to an overlap zone and whether it is less than
        // halfway into any
        minDistToOverlapZone(xpos, ypos, piece, minZoneDist, maxZoneFrac);

        // Look for a close point
        notClose = true;
        for (jnd = 0; jnd < numFullOrig; jnd++) {
          if (pointsCloseInBox(xpos, ypos, xCenters[jnd], yCenters[jnd], pcToFullSameCrit,
                               critSq)) {
            notClose = false;

            // Substitute the point if it is in overlap zone by given criterion (0?)
            // and not past halfway into overlap zone and inside the piece
            if (minZoneDist < substOverlapDist && maxZoneFrac < 0.5 && 
                minDistanceFromEdge(xpos, ypos, piece) > useEdgeDist) {
              xCenters[jnd] = xpos;
              yCenters[jnd] = ypos;
              peakVals[jnd] = mPiecePeakVec->at(piece)[ind];

              // pieceOn follows the bizarre SerialEM Nav file convention, in columns
              pieceOn[jnd] = mTileYnum->at(piece) + mNumYpieces * mTileXnum->at(piece);
              xInPiece[jnd] = xpos - mXpcAliCoords->at(piece);
              yInPiece[jnd] = ypos - mYpcAliCoords->at(piece);
              if (holeMeans && holeMeans->size() && mPieceMeanVec)
                (*holeMeans)[jnd] = mPieceMeanVec->at(piece)[ind];
              if (holeSDs && holeSDs->size() && mPieceSDsVec)
                (*holeSDs)[jnd] = mPieceSDsVec->at(piece)[ind];
              if (holeOutlies && holeOutlies->size() && mPieceOutlieVec)
                (*holeOutlies)[jnd] = mPieceOutlieVec->at(piece)[ind];
            }
            break;
          }
        }

        // If no close point found, need to add it if it is not past criterion fraction
        // into an overlap and inside the piece
        if (notClose && maxZoneFrac < mAddOverlapFrac && 
            minDistanceFromEdge(xpos, ypos, piece) > useEdgeDist) {

          // Insist that there be an adjacent point before doing this
          for (jnd = 0; jnd < numFullOrig; jnd++)
            if (pointsCloseInBox(xpos, ypos, xCenters[jnd], yCenters[jnd], 1.2f* mSpacing,
                                 2.88f * mSpacing * mSpacing))
              notClose = false;
          if (!notClose) {
            xCenters.push_back(xpos);
            yCenters.push_back(ypos);
            peakVals.push_back(mPiecePeakVec->at(piece)[ind]);
            pieceOn.push_back(mTileYnum->at(piece) + mNumYpieces * mTileXnum->at(piece));
            xInPiece.push_back(xpos - mXpcAliCoords->at(piece));
            yInPiece.push_back(ypos - mYpcAliCoords->at(piece));
            if (holeMeans && holeMeans->size() && mPieceMeanVec)
              holeMeans->push_back(mPieceMeanVec->at(piece)[ind]);
            if (holeSDs && holeSDs->size() && mPieceSDsVec)
              holeSDs->push_back(mPieceSDsVec->at(piece)[ind]);
            if (holeOutlies && holeOutlies->size() && mPieceOutlieVec)
              holeOutlies->push_back(mPieceOutlieVec->at(piece)[ind]);

            // Eliminate a close missing or "close" point
            for (jnd = 0; jnd < (int)xMissing.size(); jnd++) {
              if (pointsCloseInBox(xpos, ypos, xMissing[jnd], yMissing[jnd],
                                   pcToFullSameCrit, critSq)) {
                xMissing.erase(xMissing.begin() + jnd);
                yMissing.erase(yMissing.begin() + jnd);
                break;
              }
            }
            for (jnd = 0; jnd < (int)xCenClose.size(); jnd++) {
              if (pointsCloseInBox(xpos, ypos, xCenClose[jnd], yCenClose[jnd],
                                   pcToFullSameCrit, critSq)) {
                xCenClose.erase(xCenClose.begin() + jnd);
                yCenClose.erase(yCenClose.begin() + jnd);
                peakClose.erase(peakClose.begin() + jnd);
                break;
              }
            }
          }
        }

        // repack arrays with valid points
        if (removeDups) {
          mPieceXcenVec->at(piece)[outInd] = mPieceXcenVec->at(piece)[ind];
          mPieceYcenVec->at(piece)[outInd] = mPieceYcenVec->at(piece)[ind];
          mPiecePeakVec->at(piece)[outInd++] = mPiecePeakVec->at(piece)[ind];
        }
      }
    }
    if (removeDups) {
      mPieceXcenVec->at(piece).resize(outInd);
      mPieceYcenVec->at(piece).resize(outInd);
      mPiecePeakVec->at(piece).resize(outInd);
    }
  }

  // Now assign piece and position in piece to remaining full points
  for (ind = 0; ind < numFullOrig; ind++) {
    if (pieceOn[ind] >= 0)
      continue;
    pieceAtMax = -1;
    maxEdgeDist = -1.e30f;
    xpos = xCenters[ind];
    ypos = yCenters[ind];
    for (piece = 0; piece < numPieces; piece++) {
      dist = minDistanceFromEdge(xpos, ypos, piece);
      if (dist > 0.) {
        minDistToOverlapZone(xpos, ypos, piece, minZoneDist, maxZoneFrac);
        if (maxZoneFrac < 0.5) {
          pieceOn[ind] = mTileYnum->at(piece) + mNumYpieces * mTileXnum->at(piece);
          xInPiece[ind] = xpos - mXpcAliCoords->at(piece);
          yInPiece[ind] = ypos - mYpcAliCoords->at(piece);
          break;
        }
      }
      if (dist > maxEdgeDist) {
        maxEdgeDist = dist;
        pieceAtMax = piece;
      }
    }

    if (pieceOn[ind] < 0) {
      pieceOn[ind] = mTileYnum->at(pieceAtMax) + mNumYpieces * mTileXnum->at(pieceAtMax);
      if (minDistanceFromEdge(xpos, ypos, pieceAtMax) > 0.) {
        xInPiece[ind] = xpos - mXpcAliCoords->at(pieceAtMax);
        yInPiece[ind] = ypos - mYpcAliCoords->at(pieceAtMax);
      }
    }
  }

  scalePositionVectors(xCenters, yCenters, (float)(1. / mFullBinning));
  scalePositionVectors(xMissing, yMissing, (float)(1. / mFullBinning));
  scalePositionVectors(xCenClose, yCenClose, (float)(1. / mFullBinning));
}

bool HoleFinder::pointsCloseInBox(float xpos, float ypos, float xneigh, float yneigh, 
                                     float crit, float circleCritSq)
{
  float xposRot, yposRot, xnRot, ynRot, rsin, rcos;
  float dx = xpos - xneigh;
  float dy = ypos - yneigh;
  if (dx * dx + dy * dy >= circleCritSq)
    return false;
  rcos = (float)cos(RADIANS_PER_DEGREE * mPeakAngBelow);
  rsin = -(float)sin(RADIANS_PER_DEGREE * mPeakAngBelow);
  xposRot = rcos * xpos - rsin * ypos;
  yposRot = rsin * xpos + rcos * ypos;
  xnRot = rcos * xneigh - rsin * yneigh;
  ynRot = rsin *xneigh + rcos * yneigh;
  return (fabs(xnRot - xposRot) < crit && fabs(ynRot - yposRot) < crit);
}

void HoleFinder::distancesFromOverlap(float xpos, float ypos, int piece, int neigh,
                                      float &xZoneDist, float &yZoneDist, int &xWidth,
                                      int &yWidth)
{
  xZoneDist = (float)mPieceXsize;
  yZoneDist = (float)mPieceYsize;
  if (mTileXnum->at(neigh) > mTileXnum->at(piece)) {
    xWidth = mPieceXsize - (mXpcAliCoords->at(neigh) - mXpcAliCoords->at(piece));
    xZoneDist = mXpcAliCoords->at(neigh) - xpos;
  } else if (mTileXnum->at(neigh) < mTileXnum->at(piece)) {
    xWidth = mPieceXsize - (mXpcAliCoords->at(piece) - mXpcAliCoords->at(neigh));
    xZoneDist = xpos - (mXpcAliCoords->at(neigh) + mPieceXsize);
  }
  if (mTileYnum->at(neigh) > mTileYnum->at(piece)) {
    yWidth = mPieceYsize - (mYpcAliCoords->at(neigh) - mYpcAliCoords->at(piece));
    yZoneDist = mYpcAliCoords->at(neigh) - ypos;
  } else if (mTileYnum->at(neigh) < mTileYnum->at(piece)) {
    yWidth = mPieceYsize - (mYpcAliCoords->at(piece) - mYpcAliCoords->at(neigh));
    yZoneDist = ypos - (mYpcAliCoords->at(neigh) + mPieceYsize);
  }
}

// Find minimum distance to an overlap zone and whether it is less than
// halfway into any
void HoleFinder::minDistToOverlapZone(float xpos, float ypos, int piece, 
                                      float &minZoneDist, float &maxZoneFrac)
{
  int neigh, xt, yt, zoneXwidth, zoneYwidth;
  float xZoneDist, yZoneDist;

  minZoneDist = (float)mPieceXsize;
  maxZoneFrac = -1.;
  for (yt = B3DMAX(0, mTileYnum->at(piece) - 1); 
       yt <= (B3DMIN(mNumYpieces - 1, mTileYnum->at(piece) + 1)); yt++) {
    for (xt = B3DMAX(0, mTileXnum->at(piece) - 1); 
         xt <= (B3DMIN(mNumXpieces - 1, mTileXnum->at(piece) + 1)); xt++) {
      neigh = mPieceIndex->at(xt + yt * mNumXpieces);
      if (neigh == piece || neigh < 0)
        continue;
      distancesFromOverlap(xpos, ypos, piece, neigh, xZoneDist, yZoneDist, 
                           zoneXwidth, zoneYwidth);
      ACCUM_MIN(minZoneDist, xZoneDist);
      ACCUM_MIN(minZoneDist, yZoneDist);
      if (xZoneDist < 0)
        ACCUM_MAX(maxZoneFrac, -xZoneDist / zoneXwidth);
      if (yZoneDist < 0)
        ACCUM_MAX(maxZoneFrac, -yZoneDist / zoneYwidth);
    }
  }
}

float HoleFinder::minDistanceFromEdge(float xpos, float ypos, int piece)
{
  float distMin = xpos - mXpcAliCoords->at(piece);
  ACCUM_MIN(distMin, mXpcAliCoords->at(piece) + mPieceXsize - xpos);
  ACCUM_MIN(distMin, ypos - mYpcAliCoords->at(piece));
  ACCUM_MIN(distMin, mYpcAliCoords->at(piece) + mPieceYsize - ypos);
  return distMin;
}

void HoleFinder::scalePositionVectors(FloatVec &xpos, FloatVec &ypos, float scaling)
{
  for (int ind = 0; ind < (int)xpos.size(); ind++) {
    xpos[ind] *= scaling;
    ypos[ind] *= scaling;
  }
}

/* ==========================================================================
   edge detection using Canny method
   1) reduce noise by applying Gaussian filter
   2) find edges using Sobel operator
   3) highlight edges with non-maximum suppression using gradient direction
   4) Mark edges as strong or weak using thresholds
   5) Make weak points adjacent to strong ones also strong (hysteresis)
       
   sigma: the sigma for the Gaussian blur
   f1: the lower fraction of pixels to be excluded (<1.0)
   f2: the higher fraction of pixels to be excluded (<1.0, f2 > f1)
     --------------------------------------------------------------------------
*/
int HoleFinder::cannyEdge(float sigma, float f1, float f2) 
{
  float t1, t2, num1, num2, sobelMax;
  int cumul, newCumul, bin, ix, iy, ind, i, numIter, lastIter, iter;
  float *sobelIn, *fptrTemp;
  int weak = 128;
  const int kernelLimit = 9;
  float mat[kernelLimit * kernelLimit];
  int filterRadius;
  const int maxThreads = 8;
  IntVec threadSX[maxThreads], threadSY[maxThreads];
  unsigned char *edgeData = mEdgeData;
  unsigned char *sobelDir = mKeepCache ? mSobelDir : mEdgeData;
  float *sobelGrad = mSobelGrad;
  int threadNum, numThreads, xsize = mXsize, ysize = mYsize;
  Istack stack;
  Islice inSlice, outSlice;
  Islice *slPtr = &inSlice;
  char gaussBuf[80];

  if (!mRawData)
    return ERR_NOT_INIT;

  mSampleX.clear();
  mSampleY.clear();
  
  // apply Gaussian filter
  mWallStart = wallTime();
  if (sigma > 0)
    scaledGaussianKernel(mat, &filterRadius, kernelLimit, sigma);
  else
    filterRadius = 3;
  if (!mKeepCache || fabs(sigma - mLastSigmaForCanny) > 1.e-5) {
    if (sigma > 0) {
      applyKernelFilter(mRawData, mDataFFT, mXsize, mXsize, mYsize, mat, filterRadius);
      sobelIn = mDataFFT;
      strcpy(gaussBuf, "gaussian.mrc");

    } else {
      stack.vol = &slPtr;
      stack.zsize = 1;
      numIter = -B3DNINT(sigma);
      lastIter = -B3DNINT(mLastSigmaForCanny);
      sliceInit(&inSlice, mXsize, mYsize, SLICE_MODE_FLOAT, mSobelGrad);
      sliceInit(&outSlice, mXsize, mYsize, SLICE_MODE_FLOAT, mDataFFT);
      if (mFiltData) {

        // If there is a dedicated buffer, start with the raw data and filter between 
        // the buffer and mDataFTT
        if (lastIter <= 0 || numIter <= lastIter) {
          inSlice.data.f = mRawData;
          outSlice.data.f = mFiltData;
          for (iter = 0; iter < numIter; iter++) {
            if (sliceMedianFilter(&outSlice, &stack, 3))
              return ERR_MEMORY;
            if (!iter)
              inSlice.data.f = mDataFFT;
            B3DSWAP(inSlice.data.f, outSlice.data.f, fptrTemp);
          }
          lastIter = 0;
        } else {

          // Or if starting from previous iteration, start with filt buffer
          inSlice.data.f = mFiltData;
          outSlice.data.f = mDataFFT;
          for (iter = lastIter; iter < numIter; iter++) {
            if (sliceMedianFilter(&outSlice, &stack, 3))
              return ERR_MEMORY;
            B3DSWAP(inSlice.data.f, outSlice.data.f, fptrTemp);
          }
        }

        // Have to copy the data if it didn't end up there
        if (inSlice.data.f != mFiltData)
          memcpy(mFiltData, mDataFFT, mRawDataSize * sizeof(float));
        sobelIn = mFiltData;
      } else {

        // No dedicated buffer: filter between mDataFFT and mSobelGrad but end up in
        // mDataFFT
        inSlice.data.f = mRawData;
        outSlice.data.f = (numIter % 2) ? mDataFFT : mSobelGrad;
        for (iter = 0; iter < numIter; iter++) {
          if (sliceMedianFilter(&outSlice, &stack, 3))
            return ERR_MEMORY;
          if (!iter)
            inSlice.data.f = (numIter % 2) ? mSobelGrad : mDataFFT;
          B3DSWAP(inSlice.data.f, outSlice.data.f, fptrTemp);
        }
        sobelIn = mDataFFT;
        lastIter = 0;
      }
      sprintf(gaussBuf, "median-%d-%d.mrc", lastIter, numIter);
    }
    mWallFilter += wallTime() - mWallStart;
    
    if (mDebugImages)
      mrcWriteImageToFile(gaussBuf, mDataFFT, SLICE_MODE_FLOAT, mXsize, mYsize);
    
    // find edges using Sobel operator
    sobelEdge(sobelIn, sobelDir, mSobelMin, sobelMax);
    if (mDebugImages) {
      mrcWriteImageToFile("sobel.mrc", mSobelGrad, SLICE_MODE_FLOAT, mXsize, mYsize);
      mrcWriteImageToFile("sobelDir.mrc", sobelDir, SLICE_MODE_BYTE, mXsize, mYsize);
    }
    
    // Expand the min/max to avoid clamping bin after histogram scaling
    t1 = (sobelMax - mSobelMin) / SOBEL_HIST_DIM;
    mSobelMin -= t1;
    sobelMax += t1;
    mHistScale = SOBEL_HIST_DIM / (sobelMax - mSobelMin);
    if (mVerbose)
      PRINT3(mSobelMin, sobelMax, mHistScale);
    
    memset(mHistogram, 0, SOBEL_HIST_DIM * sizeof(int));
    for (i = 0; i < mRawDataSize; i++) {
      bin = (int)((mSobelGrad[i] - mSobelMin) * mHistScale);
      mHistogram[bin]++;
    }
    /*for (ind = 0; ind < SOBEL_HIST_DIM; ind++) {
      printf(" %5d", mHistogram[ind]);
      if ((ind + 1) % 14 == 0)
      printf("\n");
      }
      printf("\n");*/
    mLastSigmaForCanny = sigma;
  }

  cumul = 0;
  num1 = f1 * mRawDataSize;
  num2 = f2 * mRawDataSize;
  t1 = t2 = mSobelMin - 10.f;

  for (bin = 0; bin < SOBEL_HIST_DIM; bin++) {
    newCumul = cumul + mHistogram[bin];
    if (cumul <= num1 && newCumul > num1) {
      t1 = mSobelMin + (bin + (num1 - cumul) / mHistogram[bin]) / mHistScale;
      if (t2 >= mSobelMin)
        break;
    }
    if (cumul <= num2 && newCumul > num2) {
      t2 = mSobelMin + (bin + (num2 - cumul) / mHistogram[bin]) / mHistScale;
      if (t1 >= mSobelMin)
        break;
    }
    cumul = newCumul;
  }
  if (mVerbose)
    PRINT2(t1, t2);

  // Clear out edges of mEdgeData
  for (ix = 0; ix < mXsize; ix++) {
    for (iy = 0; iy < filterRadius; iy++) {
      mEdgeData[iy * mXsize + ix] = 0;
      mEdgeData[((mYsize - iy) - 1)  * mXsize + ix] = 0;
    }
  }
  for (iy = 0; iy < mYsize; iy++) {
    for (ix = 0; ix < filterRadius; ix++) {
      mEdgeData[iy * mXsize + ix] = 0;
      mEdgeData[iy * mXsize + (mXsize - ix) - 1] = 0;
    }
  }

  numThreads = B3DMIN(maxThreads, B3DNINT(0.0022 * sqrt((double)mXsize * mYsize)));
  numThreads = numOMPthreads(maxThreads);
                 
  // highlight edges with non-maximum suppression
#pragma omp parallel for default (none) num_threads(numThreads)         \
  shared(filterRadius, xsize, ysize, sobelGrad, sobelDir, edgeData, t1, t2, weak, \
         threadSX, threadSY)                                            \
  private(ix, iy, ind, threadNum)
  for (iy = filterRadius; iy < ysize-filterRadius; iy++) {
    threadNum = b3dOMPthreadNum();
    for (ix = filterRadius; ix < xsize-filterRadius; ix++) {
      ind = iy * xsize + ix;

      // check gradient strength along edge direction
      if (sobelGrad[ind] < t1) { // remove if less than t1
        edgeData[ind] = 0;
      } else if (sobelDir[ind] == 0 && // check horizontal line
                 (sobelGrad[ind] < sobelGrad[ind + 1] ||
                  sobelGrad[ind] < sobelGrad[ind - 1])) {
        edgeData[ind] = 0;
      } else if (sobelDir[ind] == 45 && // check diagonal line (45)
                 (sobelGrad[ind] < sobelGrad[ind + xsize + 1] ||
                  sobelGrad[ind] < sobelGrad[(ind - xsize) - 1])) {
        edgeData[ind] = 0;
      } else if (sobelDir[ind] == 90 && // check vertical line
                 (sobelGrad[ind] < sobelGrad[ind + xsize] ||
                  sobelGrad[ind] < sobelGrad[ind - xsize])) {
        edgeData[ind] = 0;
      } else if (sobelDir[ind] == 135 && // check diagonal line (135)
                 (sobelGrad[ind] < sobelGrad[(ind - xsize) + 1] ||
                  sobelGrad[ind] < sobelGrad[(ind+xsize) - 1])) {
        edgeData[ind] = 0;
      } else {
        if (sobelGrad[ind] > t2) {  // add to edge if larger than t2 unless it is isolated
          if (sobelGrad[ind + 1] >= t1 || sobelGrad[ind - 1] >= t1 ||
              sobelGrad[ind + xsize + 1] >= t1 || sobelGrad[ind + xsize - 1] >= t1 ||
              sobelGrad[(ind - xsize) + 1] >= t1 || sobelGrad[(ind - xsize) - 1] >= t1 ||
              sobelGrad[ind + xsize] >= t1 || sobelGrad[ind - xsize] >= t1) {
            edgeData[ind] = 255;
            threadSX[threadNum].push_back(ix);
            threadSY[threadNum].push_back(iy);
          } else {
            edgeData[ind] = (weak + 255) / 2;
          }
        } else { 
          edgeData[ind] = weak;   // or mark as weak for hysteresis
        }
      }
    }
  }
  if (mDebugImages)
    mrcWriteImageToFile("twoLevel.mrc", mEdgeData, MRC_MODE_BYTE, mXsize, mYsize);

  // Consolidate samples and build queue
  for (threadNum = 0; threadNum < numThreads; threadNum++) {
    ix = (int)threadSX[threadNum].size();
    for (ind = 0; ind < ix; ind++)
      addToSampleAndQueue(threadSX[threadNum][ind], threadSY[threadNum][ind]);
  }
  
  // Hysteresis: grow the strong edge by adding weak neighbors
  while (mStrongQueue.size() > 0) {
    bin = mStrongQueue.front();
    mStrongQueue.pop();
    ix = mSampleX[bin];
    iy = mSampleY[bin]; 
    ind = mXsize * iy + ix;

    // check if neighbors are weak
    if (mEdgeData[(ind - mXsize) - 1] == weak)
      addToSampleAndQueue(ix - 1, iy - 1);
    if (mEdgeData[(ind - mXsize)] == weak) 
      addToSampleAndQueue(ix, iy - 1);
    if (mEdgeData[(ind - mXsize) + 1] == weak) 
      addToSampleAndQueue(ix + 1, iy - 1);
    if (mEdgeData[ind - 1] == weak)  
      addToSampleAndQueue(ix - 1, iy);
    if (mEdgeData[ind + 1] == weak) 
      addToSampleAndQueue(ix + 1, iy);
    if (mEdgeData[ind + mXsize - 1] == weak) 
      addToSampleAndQueue(ix - 1, iy + 1);
    if (mEdgeData[ind + mXsize] == weak) 
      addToSampleAndQueue(ix, iy + 1);
    if (mEdgeData[ind + mXsize + 1] == weak) 
      addToSampleAndQueue(ix + 1, iy + 1);
  }

  if (mDebugImages)
    mrcWriteImageToFile("final.mrc", mEdgeData, MRC_MODE_BYTE, mXsize, mYsize);

  mHaveDataFFT = false;
  mHaveEdgeAvgFFT = false;
  mUsedWeakEdges = 0;
  mWallCanny += wallTime() - mWallStart;
  return 0;
}

/*
 * Mark one point as strong and add it to the queue of strong points to be checked
 */
void HoleFinder::addToSampleAndQueue(int ix, int iy)
{
  mEdgeData[iy * mXsize + ix] = 255;
  if (ix > 0 && ix < mXsize - 1 && iy > 0 && iy < mYsize - 1)
    mStrongQueue.push((int)mSampleX.size());
  mSampleX.push_back(ix);
  mSampleY.push_back(iy);
}

/* ==========================================================================
   find edges using Sobel operator
   Applies the x and y operators to the image in inputData and stores the edge gradient
   strength in mSobelGrad and the edge direction in sobelDir.
   --------------------------------------------------------------------------
*/
void HoleFinder::sobelEdge(float *inputData, unsigned char *sobelDir, float &sobelMin,
                           float &sobelMax) {
  float xsum = 0, ysum= 0., tempTheta= 0., tmin = 0., tmax = 0., center = 3.;
  int iy, ix = 0, ind, numThreads, threadNum = 0;
  float *newData = mSobelGrad;
  int xsize = mXsize, ysize = mYsize;
  const int maxThreads = 16;
  float threadMin[maxThreads], threadMax[maxThreads];

  memset(mSobelGrad, 0, mRawDataSize * sizeof(float));
  memset(sobelDir, 0, mRawDataSize);

  // Set up threads
  numThreads = B3DMIN(maxThreads, B3DNINT(0.04 * sqrt((double)xsize * ysize)));
  numThreads = numOMPthreads(numThreads);
  for (ind = 0; ind < numThreads; ind++) {
    threadMin[ind] = 1.e37f;
    threadMax[ind] = -1.e37f;
  }

  // loop over image
#pragma omp parallel for default (none) num_threads(numThreads)          \
  shared(inputData, center, xsize, ysize, newData, sobelDir, threadMin, threadMax) \
  private(ix, iy, ind, xsum, ysum, tempTheta, threadNum, tmin, tmax)
  for (iy = 3; iy < ysize - 3; iy++) {
    threadNum = b3dOMPthreadNum();
    tmin = 1.e37f;
    tmax = -1.e37f;
    for (ix = 3; ix < xsize - 3; ix++) {
      ind = ix + iy * xsize;
      xsum = (inputData[ind - xsize - 1] + center * inputData[ind - 1] +
              inputData[ind + xsize - 1]) -
        (inputData[ind - xsize + 1] + center * inputData[ind + 1] +
         inputData[ind + xsize + 1]);
      ysum = (inputData[ind - xsize - 1] + center * inputData[ind - xsize] +
              inputData[ind - xsize + 1]) -
        (inputData[ind + xsize - 1] + center * inputData[ind + xsize] +
         inputData[ind + xsize + 1]);
      newData[ind] = sqrtf(ysum * ysum + xsum * xsum);
      ACCUM_MIN(tmin, newData[ind]);
      ACCUM_MAX(tmax, newData[ind]);
      
      // classify edge
      tempTheta = (float)(atan2f(ysum, xsum) / RADIANS_PER_DEGREE);
      sobelDir[ind] = (((int)((tempTheta + 202.5) / 45.)) % 4) * 45;
    }
    ACCUM_MIN(threadMin[threadNum], tmin);
    ACCUM_MAX(threadMax[threadNum], tmax);
  }

  sobelMin = 0.;
  sobelMax = -1.e37f;
  for (ind = 0; ind < numThreads; ind++) {
    ACCUM_MIN(sobelMin, threadMin[ind]);
    ACCUM_MAX(sobelMax, threadMax[ind]);
  }
}

/*
 * Main routine to find circles by correlation of the edge image with either a circle
 * template or an average template from the edge image or the raw image.
 * midRadius is the middle radius to try
 * radiusInc is the increment between radii
 * width is the thickness of circles in pixels, or -1 to use the current average template
 * numRad is the maximum number of of radii to try
 * retain indicates whether to keep circle FFTs, provided the keepCache argument was 2
 * minSpacing is the minimum spacing allowed between detected centers
 * Input values midRadius, radiusInc, width, and minSpacing are in pixels
 * useWeak would be 1 to use weak edges in the cross-correlation or 2 when using raw avg
 * xBoundary, yBoundary optionally contain a boundary contour
 * Boundary points are in original pixels
 * bestRadius is the interpolated value of the best-correlating radius, in reduced pixels
 * xCenters, yCenters are the detected centers in original pixels
 * peakVals has the correlation strengths
 * When width < 0, numRad must be 1 and the midRadius must match what was used to make
 * the average template
 */
int HoleFinder::findCircles(float midRadius, float radiusInc, float width, int numRad,
                            bool retain, float minSpacing, int useWeak,
                            FloatVec &xBoundary, FloatVec &yBoundary,
                            float &bestRadius, FloatVec &xCenters, FloatVec &yCenters,
                            FloatVec &peakVals)
{
  float *circTemplate = NULL;
  bool usedCache, stopPlus, stopMinus;
  int radInd, maxRad, ind, ix, iy, maxPoints, minRadPoints, maxRadPoints = 0;
  int numPoints, numAdd, bestInd, edgeThresh, vecOffset, numMeasured = 0;
  int nextDir, minIndDone, maxIndDone, vecInd, sumInd, templateInd;
  int minXpeakFind, maxXpeakFind, minYpeakFind, maxYpeakFind, blockSize;
  int numFromMean, numFromSD, err, numStrong;
  float innerCrit, outerCrit, sum, radius, cornDist, dx, dy, sumMax, weight;
  float boundXmin, boundXmax, boundYmin, boundYmax, strongMean, norm;
  float *crossCorr = mDataFFT;
  bool useRawAvg = width < -1;
  bool useEdgeAvg = width < 0 && !useRawAvg;
  float *boxAverage = useRawAvg ? mRawAverage : mEdgeAverage;
  int boxSize = useRawAvg ? mRawBoxSize : mAvgBoxSize;
  float cacheTol = 0.05f;
  float meanOutlieCrit = 4.5f, sdOutlieCrit = 4.5f;
  float weakDimCrit = 0.05f, maxWeakDimFrac = 0.6f, strongMeanMinRatio = 5.f;
  FloatVec tempXcenters, tempYcenters, tempPeaks, peakSums, radii, holeMeans, holeSDs;
  FloatVec weakDims;
  std::vector<FloatVec> xCenVec, yCenVec, peakVec;
  double findStart = wallTime();
  if (!mRawData)
    return ERR_NOT_INIT;

  // Anticipate what block size the peak finder will use, set default limits
  blockSize = B3DMAX(1, (int)(minSpacing / sqrt(2.)));
  minXpeakFind = 1;
  maxXpeakFind = mXpadSize - 2;
  minYpeakFind = 1;
  maxYpeakFind = mYpadSize - 2;
  
  // Adjust limits for boundary contours
  if (xBoundary.size()) {
    boundXmin = *std::min_element(xBoundary.begin(), xBoundary.end());
    boundYmin = *std::min_element(yBoundary.begin(), yBoundary.end());
    boundXmax = *std::max_element(xBoundary.begin(), xBoundary.end());
    boundYmax = *std::max_element(yBoundary.begin(), yBoundary.end());
    boundXmin = floor((boundXmin - mRedXoffset) / mReduction + mPadXoffset);
    boundYmin = floor((boundYmin - mRedXoffset) / mReduction + mPadXoffset);
    boundXmax = ceil((boundXmax - mRedXoffset) / mReduction + mPadXoffset);
    boundYmax = ceil((boundYmax - mRedXoffset) / mReduction + mPadXoffset);
    ACCUM_MAX(minXpeakFind, (int)boundXmin);
    ACCUM_MIN(maxXpeakFind, (int)boundXmax);
    ACCUM_MAX(minYpeakFind, (int)boundYmin);
    ACCUM_MIN(maxYpeakFind, (int)boundYmax);
  }

  if (maxXpeakFind - minXpeakFind < 10 || maxYpeakFind - minYpeakFind < 10)
    return 0;

  // Max points is just the number of blocks that will fit in X time the number in Y
  maxPoints = ((maxXpeakFind + 1 - minXpeakFind) / blockSize + 1) *
    ((maxYpeakFind + 1 - minYpeakFind) / blockSize + 1) + 4;
  minRadPoints = 2 * maxPoints;
  edgeThresh = useWeak ? 120 : 250;
  if (useWeak != mUsedWeakEdges)
    mHaveDataFFT = false;
  
  // Set up vectors to hold the points and to hold the biggest possible number of
  // search positions
  tempXcenters.resize(maxPoints);
  tempYcenters.resize(maxPoints);
  tempPeaks.resize(maxPoints);
  ind = 2 * numRad + 1;
  xCenVec.resize(ind);
  yCenVec.resize(ind);
  peakVec.resize(ind);
  peakSums.resize(ind);
  radii.resize(ind);

  if (width < 0 && (fabs(midRadius - mRadiusForEdgeAvg) > 0.01 || numRad > 1))
    return ERR_TEMPLATE_PARAM;
  if (width < 0 && !boxAverage)
    return ERR_NO_TEMPLATE;

  vecOffset = numRad / 2;
  radInd = numRad / 2;
  sumMax = -1.e30f;
  minIndDone = maxIndDone = radInd + vecOffset;

  // Loop on radii from middle
  while (numMeasured < numRad) {
    usedCache = false;
    vecInd = radInd + vecOffset;
    radius = (float)(midRadius + radInd * radiusInc - 0.5 * radiusInc * (numRad - 1));
    innerCrit = (float)((radius - width / 2.) * (radius - width / 2.));
    outerCrit = (float)((radius + width / 2.) * (radius + width / 2.));
    radii[vecInd] = radius;

    // Look for radius/width in cache and adopt it
    templateInd = -1;
    for (ind = 0; ind < (int)mCircleRadii.size(); ind++) {

      // Keep track of average template slot and slip out if it is not valid
      if (mCircleWidths[ind] < 0) {
        if (!mKeepAvgFFTs || (mCircleWidths[ind] < -1 && useRawAvg) ||
            (mCircleWidths[ind] < 0 && mCircleWidths[ind] >= -1 && useEdgeAvg))
          templateInd = ind;
        if ((useRawAvg && !mHaveRawAvgFFT) || (useEdgeAvg && !mHaveEdgeAvgFFT))
          break;
      }
      if (fabs(mCircleRadii[ind] - radius) <= cacheTol && 
          fabs(mCircleWidths[ind] - width) <= cacheTol) {
        free(circTemplate);
        circTemplate = mCircleFFTs[ind];
        usedCache = true;
        break;
      }
    }

    // If no cache FFT found, make array if needed
    if (!usedCache) {

      // Reuse memory for average-based template
      if (width < 0. && templateInd >= 0) {
        free(circTemplate);
        circTemplate = mCircleFFTs[templateInd];
        usedCache = true;
      }
      if (!circTemplate) {
        circTemplate = B3DMALLOC(float, mPadXdim * mYpadSize);
        if (!circTemplate)
          return -1;
      }

      // Fill the template and take its FFT
      if (width < 0.) {
        sliceSplitFill(boxAverage, boxSize, boxSize, circTemplate, mPadXdim,
                       mXpadSize, mYpadSize, 0, 0.);
        if (useRawAvg)
          mHaveRawAvgFFT = true;
        else
          mHaveEdgeAvgFFT = true;
        if ((useRawAvg && mDebugImages == 3) || (useEdgeAvg && mDebugImages == 2))
          mrcWriteImageToFile("splitfill.mrc", circTemplate, 2, mPadXdim, mYpadSize);
      } else {

        memset(circTemplate, 0, mPadXdim * mYpadSize * sizeof(float));
        maxRad = (int)ceil(radius + width / 2.) + 1;
        weight = (float)sqrt(midRadius / radius);
        for (iy = 0; iy <= maxRad; iy++) {
          for (ix = 0; ix <= maxRad; ix++) {
            cornDist = (float)(pow(ix + 0.5, 2.) + pow(iy + 0.5, 2.));
            if (cornDist >= innerCrit && cornDist < outerCrit) {
              circTemplate[iy * mPadXdim + ix] = weight;
              circTemplate[iy * mPadXdim + mXpadSize - ix - 1] = weight;
              circTemplate[(mYpadSize - iy - 1) * mPadXdim + ix] = weight;
              circTemplate[(mYpadSize - iy - 1) * mPadXdim + mXpadSize - ix - 1] = weight;
            }
          }
        }
      }

      mWallStart = wallTime();
      todfftc(circTemplate, mXpadSize, mYpadSize, 0);
      mWallFFT += wallTime() - mWallStart;
    }

    // If the FFT haven't been taken of the data, do that now 
    if (!mHaveDataFFT) {
      if (useWeak > 1) {
        sliceTaperOutPad(mRawData, SLICE_MODE_FLOAT, mXsize, mYsize, mDataFFT, mPadXdim,
                         mXpadSize, mYpadSize, 0, 0.);
      } else {
        memset(mDataFFT, 0, mPadXdim * mYpadSize * sizeof(float));
        for (iy = 0; iy < mYsize; iy++) {
          for (ix = 0; ix < mXsize; ix++) {
            if (mEdgeData[iy * mXsize + ix] > edgeThresh)
              mDataFFT[(iy + mPadYoffset) * mPadXdim + ix + mPadXoffset] = 1.;
          }
        }
      }
      mWallStart = wallTime();
      todfftc(mDataFFT, mXpadSize, mYpadSize, 0);
      mWallFFT += wallTime() - mWallStart;
      mHaveDataFFT = true;
      mUsedWeakEdges = useWeak;
    }
    
    // If keeping FFT array, copy the data FFT to that and use it, otherwise mark
    // data as consumed
    if (mKeepCache) {
      memcpy(mCrossCorr, mDataFFT, mPadXdim * mYpadSize * sizeof(float));
      crossCorr = mCrossCorr;
    } else 
      mHaveDataFFT = false;

    // Finish the cross-correlation
    conjugateProduct(crossCorr, circTemplate, mXpadSize, mYpadSize);
    mWallStart = wallTime();
    todfftc(crossCorr, mXpadSize, mYpadSize, 1);
    mWallFFT += wallTime() - mWallStart;

    // If keeping templates in this cache, add to cache, NULL out circTemplate to avoid
    // freeing anything in cache
    if (mKeepCache > 1 && !usedCache && retain) {
      mCircleFFTs.push_back(circTemplate);
      mCircleRadii.push_back(radius);
      mCircleWidths.push_back(width);
      circTemplate = NULL;
    }
    if (usedCache)
      circTemplate = NULL;

    // Find peaks
    mWallStart = wallTime();
    findSpacedXCorrPeaks(crossCorr, mPadXdim, minXpeakFind, maxXpeakFind,
                         minYpeakFind, maxYpeakFind, &tempXcenters[0], &tempYcenters[0],
                         &tempPeaks[0], maxPoints, minSpacing, &numPoints, 0.025f);
    /*FILE *fp;
    if (    radInd == numRad /2 && width > 0) {
    fp = fopen("spaced.txt", "w");
    for (ind = 0; ind < numPoints; ind++) {
      if (tempPeaks[ind] >= -1.e29)
        fprintf(fp, "%.2f %.2f 0 %g\n", tempXcenters[ind], tempYcenters[ind], 
                tempPeaks[ind]);
    }
    fclose(fp);
    }*/
    
    mWallPeak += wallTime() - mWallStart;
    if (((width > 0 && mDebugImages == 1) || (useRawAvg && mDebugImages == 3)
         || (useEdgeAvg && mDebugImages == 2)) && useWeak != 1) {
      mrcWriteImageToFile("crosscorr.mrc", crossCorr, 2, mPadXdim, mYpadSize);
    }

    // Eliminate peaks outside the boundary
    numAdd = numPoints;
    if (xBoundary.size()) {
      for (ind = 0; ind < numPoints; ind++) {
        if (tempPeaks[ind] >= -1.e29) {
          corrPointToFullImage(tempXcenters[ind], tempYcenters[ind], dx, dy);
          if (!InsideContour(&xBoundary[0], &yBoundary[0], (int)xBoundary.size(), dx, dy))
            tempPeaks[ind] = -1.e30f;
        }
      }

    } else if (numPoints > 3) {
      
      // Or eliminate outside the original image
      for (ind = 0; ind < numPoints; ind++) {
        if (tempPeaks[ind] >= -1.e29) {
          if (tempXcenters[ind] < mPadXoffset || tempXcenters[ind] >= mXsize + mPadXoffset
              || tempYcenters[ind] < mPadYoffset || 
              tempYcenters[ind] >= mYsize + mPadYoffset)
            tempPeaks[ind] = -1.e30f;
        }
      }
    }

    // Find out how many points are left
    numAdd = 0;
    for (ind = 0; ind < numPoints; ind++)
      if (tempPeaks[ind] >= -1.e29)
        numAdd++;

    /*for (ind = 0; ind < 50; ind++) {
      printf("%.2f %.2f %f   %.2f %.2f %f\n", tempXcenters[ind], tempYcenters[ind],
             tempPeaks[ind], tempXcenters2[ind], tempYcenters2[ind], tempPeaks2[ind]);
             }*/

    // Pack the points into the array for this radius
    xCenVec[vecInd].resize(numAdd);
    yCenVec[vecInd].resize(numAdd);
    peakVec[vecInd].resize(numAdd);
    numAdd = 0;
    for (ind = 0; ind < numPoints; ind++) {
      if (tempPeaks[ind] >= -1.e29) {
        xCenVec[vecInd][numAdd] = tempXcenters[ind];
        yCenVec[vecInd][numAdd] = tempYcenters[ind];
        peakVec[vecInd][numAdd++] = tempPeaks[ind];
      } 
    }

    // Eliminate intensity outliers
    if (numAdd >= 10) {
      holeIntensityStats(radius / 2.f, xCenVec[vecInd], yCenVec[vecInd], true, false,
                         &holeMeans, &holeSDs, NULL);

      // But first, use this overall mean product of peak strength, intensity and SD
      // to count up ones above a certain ratio to the highest value
      norm = -1.e20f;
      strongMean = 0.;
      numStrong = 0;
      weakDims.resize(numAdd);
      for (ind = 0; ind < numAdd; ind++) {
        weakDims[ind] = (float)pow(fabs((double)peakVec[vecInd][ind] * holeMeans[ind] * 
                                 holeSDs[ind]), 0.3333);
        ACCUM_MAX(norm, weakDims[ind]);
      }
      for (ind = 0; ind < numAdd; ind++) {
        weakDims[ind] /= norm;
        if (weakDims[ind] > weakDimCrit) {
          numStrong++;
          strongMean += weakDims[ind];
        }
      }

      // If the mean is enough higher than the criterion and the # of weak ones are
      // not too numerous, get rid of the weak ones
      strongMean /= numStrong;
      sumInd = 0;
      ind = numAdd - numStrong;
      if (strongMean / weakDimCrit > strongMeanMinRatio && ind > 0 &&
          ind < maxWeakDimFrac * numAdd) {
        for (ind = 0; ind < numAdd; ind++) {
          if (weakDims[ind] > weakDimCrit) {
            xCenVec[vecInd][sumInd] = xCenVec[vecInd][ind];
            yCenVec[vecInd][sumInd] = yCenVec[vecInd][ind];
            peakVec[vecInd][sumInd++] = peakVec[vecInd][ind];
          }
        }
        xCenVec[vecInd].resize(numStrong);
        yCenVec[vecInd].resize(numStrong);
        peakVec[vecInd].resize(numStrong);
        numAdd = sumInd;
      }
      
      // Now proceed with outlier removal
      RETURN_IF_ERR(removeOutliers(xCenVec[vecInd], yCenVec[vecInd], &peakVec[vecInd],
                                   holeMeans, 4.5f, 0., numFromMean, ind));
      RETURN_IF_ERR(removeOutliers(xCenVec[vecInd], yCenVec[vecInd], &peakVec[vecInd],
                                   holeSDs, 4.5f, 0., numFromSD, ind));
      if (mVerbose)
        printf("%d mean and %d SD outliers\n", numFromMean, numFromSD);
      numAdd = (int)xCenVec[vecInd].size();
    }

    ACCUM_MIN(minRadPoints, numAdd);
    ACCUM_MAX(maxRadPoints, numAdd);
    ACCUM_MIN(minIndDone, vecInd);
    ACCUM_MAX(maxIndDone, vecInd);

    // Re-evaluate all sums based on current range of points found
    numPoints = B3DMAX(1, B3DMIN(maxRadPoints / 4, minRadPoints / 2));
    sumMax = -1.e30f;
    for (sumInd = minIndDone; sumInd <= maxIndDone; sumInd++) {
      sum = 0.;
      for (ind = 0; ind < B3DMIN((int)peakVec[sumInd].size(), numPoints); ind++)
        sum += peakVec[sumInd][ind];
      if (sum > sumMax) {
        bestInd = sumInd;
        sumMax = sum;
        bestRadius = radii[sumInd];
      }
      peakSums[sumInd] = sum;
    }

    // Decide on direction and new index value
    numMeasured++;

    // Stop in a direction if it is monotonic from the peak for two steps
    // And stop measuring when that happens both ways
    stopPlus = stopMinus = false;
    if (bestInd - 2 >= minIndDone && peakSums[bestInd - 2] < peakSums[bestInd - 1])
      stopMinus = true;
    if (bestInd + 2 <= maxIndDone && peakSums[bestInd + 2] < peakSums[bestInd + 1])
      stopPlus = true;
    if (stopPlus && stopMinus)
      break;

    // Go from an end if the other direction is stopped or best is at that end
    // Or go on the side with fewer measurements from the peak
    // Or go on the side with a higher value
    if (stopMinus || bestInd == maxIndDone) {
      nextDir = 1;
    } else if (stopPlus || bestInd == minIndDone) {
      nextDir = -1;
    } else if (bestInd - minIndDone > maxIndDone - bestInd) {
      nextDir = 1;
    } else if (maxIndDone - bestInd > bestInd - minIndDone) {
      nextDir = -1;
    } else if (peakSums[minIndDone] > peakSums[maxIndDone]) {
      nextDir = -1;
    } else       {
      nextDir = 1;
    } 
    radInd = B3DCHOICE(nextDir > 0, maxIndDone + 1, minIndDone - 1) - vecOffset;
  }

  if (mVerbose) {
    for (vecInd = minIndDone; vecInd <= maxIndDone; vecInd++)
      printf("Radius %.1f  peaks %d  corr sum %f\n", radii[vecInd], 
             (int)peakVec[vecInd].size(), peakSums[vecInd]);
  }

  if ((mKeepCache < 2 || !retain) && !usedCache && circTemplate) {
    free(circTemplate);
  }

  numPoints = B3DMAX(1, B3DMIN(maxRadPoints / 4, minRadPoints / 2));
  if (mVerbose && numRad > 1)
    PRINT3(minRadPoints, maxRadPoints, numPoints);

  if (bestInd > minIndDone && bestInd < maxIndDone) {
    sum = (float)parabolicFitPosition(peakSums[bestInd - 1], peakSums[bestInd], 
                               peakSums[bestInd + 1]);
    bestRadius += sum * radiusInc;
  }

  // Return scaled points
  if (mVerbose && numRad > 1)
    PRINT1(bestRadius);
  numPoints = (int)xCenVec[bestInd].size();
  xCenters.resize(numPoints);
  yCenters.resize(numPoints);
  peakVals.resize(numPoints);
  for (ind = 0; ind < numPoints; ind++) {
    corrPointToFullImage(xCenVec[bestInd][ind], yCenVec[bestInd][ind], xCenters[ind],
                         yCenters[ind]);
    peakVals[ind] = peakVec[bestInd][ind] / peakVec[bestInd][0];
  }
  mLastBestRadius = bestRadius;
  mWallCircle += wallTime() - findStart;
  return 0;
}

void HoleFinder::corrPointToFullImage(float xIn, float yIn, float &xOut, float &yOut)
{
  xOut = mReduction * (xIn - mPadXoffset) + mRedXoffset;
  yOut = mReduction * (yIn - mPadYoffset) + mRedYoffset;
}

/*
 * Analyzes a set of detected points for ones that fall on a regular grid, does a local
 * prediction of where each point in the interior of the grid should be, eliminates points
 * that differ from the prediction by maxError, and returns a set of missing positions
 * maxSpacing is the maximum spacing between adjacent points to allow, in original pixels
 * maxError is maximum error between a point and predicted position, in original pixels
 * xCenters, yCenters, peakVals are passed in with the previously found peaks and their
 * correlation strengths and returned with the resulting points
 * altInd is a set of indexes from a point in the main arrays to a nearby point in the
 * alternative set specified in xCenAlt, yCenAlt, peakAlt
 * xMissing, yMissing are returned with predicted positions that have no point
 * trueSpacing is returned with the found spacing in original pixels
 * All vectors contain original pixels
 * All position vectors are in original pixels.
 * Returned value for trueSpacing in original pixels
 * Set maxSpacing <= 0 just to find the average lengths and angles, which are then
 * available only as vectors by calling getGridVectors, while previously found values are
 * saved and restored into mPeakAng... and mAvgLen...  The units of the centers are then
 * arbitrary.
 */
int HoleFinder::analyzeNeighbors
(FloatVec &xCenVec, FloatVec &yCenVec, FloatVec &peakVals, IntVec &altInds,
 FloatVec &xCenAlt, FloatVec &yCenAlt, FloatVec &peakAlt, float maxSpacing,
 float maxError, int reuseGeomBin, float &trueSpacing, FloatVec &xMissing,
 FloatVec &yMissing)
{
  FloatVec connLengths, connAngles, connReducedAng, connErrors, lenBelow, lenAbove;
  IntVec connPoint1, connPoint2, gridVec;
  std::vector<short> ptGridX, ptGridY, connQuadrant;
  std::vector<IntVec> ptConnLists;
  std::queue<int> connQueue;
  int ind, jnd, numBelow, numAbove, numPoints = (int)xCenVec.size();
  int minGridX, minGridY, maxGridX, maxGridY, numInGroup, numConn, num, bin, maxInBin;
  float maxSpaceSq, dx, dy, distSq, spacing, angle;
  float coarseAngle, avgLenAbove, avgLenBelow, minDist, altAngAbove, altAngBelow;
  float saveLenBelow, saveLenAbove, saveAngBelow, saveAngAbove, avgLen;
  float sd, sem, errBelow, errAbove, error;
  int ptFrom, ptTo, sign, setGridVal, ix, iy, jx, jy, jxCen, jyCen, jxStart, jyStart;
  int jyEnd, jxEnd, bestXcen, bestYcen, offset, bestOffset, bestNum, con;
  int gridXdim, gridYdim, numFit, connPt1, connPt2, numAltBelow, numAltAbove;
  float angForAlt, lenForAlt, altLen, errAlt, bestError, altLenBelow, altLenAbove;
  float aFit, bFit, cFit, xPred, yPred, predErr;
  bool doFit, testAlt = altInds.size() > 0;
  bool justFindSpacing = maxSpacing <= 0.;
  int coarseHist[10] = {0,0,0,0,0,0,0,0,0,0};
  float maxConnError = maxError * 1.8f;
  int minGroupSize = 3;
  int fitLen = 5;
  const int maxFit = 100, maxCol = 6;
  float xmat[maxCol][maxFit];
  float *fitX = xmat[2], *fitY = xmat[3], *fitDelIX = xmat[0], *fitDelIY = xmat[1];
  float solution[2][maxCol], fitWork[2 * maxFit + maxCol * maxCol], fitMean[maxCol];
  float fitSD[maxCol], fitConst[2], kFactor = 4.68f, maxChange = 0.02f, maxOscill = 0.05f;
  int delGridX[4] = {1, 0, -1, 0}, delGridY[4] = {0, 1, 0, -1};
  int numIter, maxIter = 30, minForRobust = 6;
  const int numKernBins = 180;
  const int noGridVal = 32767, hadGridVal = 32765, rejectGridVal = 32766;

  ptConnLists.resize(numPoints);
  xMissing.clear();
  yMissing.clear();

  // If need to find spacing, get median of nearest neighbor distance, then mean of 
  // all distances 
  if (justFindSpacing && numPoints > 1) {
    saveLenBelow = mAvgLenBelow;
    saveLenAbove = mAvgLenAbove;
    saveAngBelow = mPeakAngBelow;
    saveAngAbove = mPeakAngAbove;

    // Find nearest neighbor distance for each point
    for (ind = 0; ind < numPoints - 1; ind++) {
      minDist = 1.e30f;
      for (jnd = ind + 1; jnd < numPoints; jnd++) {
        dx = xCenVec[ind] - xCenVec[jnd];
        dy = yCenVec[ind] - yCenVec[jnd];
        distSq = dx * dx + dy * dy;
        ACCUM_MIN(minDist, distSq);
      }
      lenBelow.push_back(sqrtf(minDist));
    }

    // Get median or mean and estimate the spacing and maximum
    if (lenBelow.size() > 3)
      rsFastMedianInPlace(&lenBelow[0], (int)lenBelow.size(), &spacing);
    else if (lenBelow.size())
      avgSD(&lenBelow[0], (int)lenBelow.size(), &spacing, &sd, &sem);
    maxSpacing = 1.33f * spacing;
    maxSpaceSq = maxSpacing * maxSpacing;

    // Get all distances less the maximum and get refined spacing and maximum
    lenBelow.clear();
    for (ind = 0; ind < numPoints - 1; ind++) {
      for (jnd = ind + 1; jnd < numPoints; jnd++) {
        dx = xCenVec[ind] - xCenVec[jnd];
        dy = yCenVec[ind] - yCenVec[jnd];
        distSq = dx * dx + dy * dy;
        if (distSq <= maxSpaceSq)
          lenBelow.push_back(sqrtf(distSq));
      }
    }
    avgSD(&lenBelow[0], (int)lenBelow.size(), &spacing, &sd, &sem);
    maxSpacing = 1.33f * spacing;
    lenBelow.clear();
  }
  
  // Make list of connections within the maximum spacing
  maxSpaceSq = maxSpacing * maxSpacing;
  for (ind = 0; ind < numPoints - 1; ind++) {
    for (jnd = ind + 1; jnd < numPoints; jnd++) {
      dx = xCenVec[ind] - xCenVec[jnd];
      dy = yCenVec[ind] - yCenVec[jnd];
      distSq = dx * dx + dy * dy;
      if (distSq <= maxSpaceSq) {
        spacing = sqrt(distSq);
        angle = (float)(atan2(dy, dx) / RADIANS_PER_DEGREE);
        connLengths.push_back(spacing);
        connAngles.push_back(angle);
        connReducedAng.push_back(goodAngle(angle));
        connPoint1.push_back(ind);
        connPoint2.push_back(jnd);
      }
    }
  }

  numConn = (int)connAngles.size();
  if (numConn == 1)
    minGroupSize = 2;
  if (reuseGeomBin == 0) {

    // Make a collapsed histogram for finding the angle between 0 and 90
    for (ind = 0; ind < numConn; ind++) {
      angle = connReducedAng[ind];
      if (angle < 0)
        angle += 90.;
      bin = B3DMIN((int)(angle / 10.), 8);
      coarseHist[bin]++;
    }

    // Find peak of that histogram
    maxInBin = 0;
    for (ind = 0; ind < 9; ind++) {
      if (coarseHist[ind] > maxInBin) {
        maxInBin = coarseHist[ind];
        coarseAngle = (float)((ind * 10.) + 5.);
      }
    }

    // Do a simple mean of angles within some distance of this peak or the one 90 degrees 
    // away, iterating to work from an angle more refined than that initial bin estimate 
    mPeakAngAbove = coarseAngle;
    mPeakAngBelow = coarseAngle - 90.f;
    refineAnglesGetLengths(connReducedAng, connLengths, mPeakAngBelow, mPeakAngAbove,
      mAvgLenBelow, mAvgLenAbove, numBelow, numAbove);

    // Do the same thing for the angle 45 degrees away
    if (mPeakAngAbove >= 45.f) {
      altAngAbove = mPeakAngAbove - 45.f;
      altAngBelow = mPeakAngBelow - 45.f;
    } else {
      altAngAbove = mPeakAngAbove + 45.f;
      altAngBelow = mPeakAngBelow + 45.f;
    }
    refineAnglesGetLengths(connReducedAng, connLengths, altAngBelow, altAngAbove,
      altLenBelow, altLenAbove, numAltBelow, numAltAbove);

    // An ad hoc test for when the 45-degree peak, if any, is the one to use: it has more
    // points and is not much farther away, or it has enough points and is closer by
    // roughly sqrt(2)
    avgLen = 0.5f * (mAvgLenBelow + mAvgLenAbove);
    altLen = 0.5f * (altLenBelow + altLenAbove);
    if ((numAltBelow + numAltAbove > 1.05 * (numBelow + numAbove) &&
      altLen < 1.05 * avgLen) ||
      (numAltBelow + numAltAbove > 0.5 * (numBelow + numAbove) &&
        fabs(altLen - avgLen / 1.414) < 0.1 * altLen)) {
      mAvgLenBelow = altLenBelow;
      mAvgLenAbove = altLenAbove;
      mPeakAngBelow = altAngBelow;
      mPeakAngAbove = altAngAbove;
    }

    avgLenBelow = mAvgLenBelow;
    avgLenAbove = mAvgLenAbove;
  } else {
    avgLenBelow = reuseGeomBin * mAvgLenBelow;
    avgLenAbove = reuseGeomBin * mAvgLenAbove;    
  }

  trueSpacing = 0.5f * (avgLenBelow + avgLenAbove);
  if (justFindSpacing) {
    mJustLenBelow = mAvgLenBelow;
    mJustLenAbove = mAvgLenAbove;
    mJustAngBelow = mPeakAngBelow;
    mJustAngAbove = mPeakAngAbove;
    mAvgLenBelow = saveLenBelow;
    mAvgLenAbove = saveLenAbove;
    mPeakAngBelow = saveAngBelow;
    mPeakAngAbove= saveAngAbove;

    return 0;
  }

  // Compute errors and directions
  connErrors.resize(numConn);
  connQuadrant.resize(numConn);
  for (ind = 0; ind < numConn; ind++) {
    angle = goodAngle(connReducedAng[ind] - mPeakAngBelow);
    errBelow = SIDES_ANGLE_ERROR(connLengths[ind], avgLenBelow, angle);
    angle = goodAngle(connReducedAng[ind] - mPeakAngAbove);
    errAbove = SIDES_ANGLE_ERROR(connLengths[ind], avgLenAbove, angle);
    bestError = B3DMIN(errBelow, errAbove);
    connPt1 = connPoint1[ind];
    connPt2 = connPoint2[ind];

    // Test alternative connections, but assume the angle choice is correct
    if (testAlt && (altInds[connPt1] >= 0 || altInds[connPt2] >= 0)) {
      angForAlt = errBelow < errAbove ? mPeakAngBelow : mPeakAngAbove;
      lenForAlt = errBelow < errAbove ? avgLenBelow : avgLenAbove;
      if (altInds[connPt1] >= 0) {
        altLen = lengthAndAngle(angForAlt, xCenAlt[altInds[connPt1]] - xCenVec[connPt2], 
                                yCenAlt[altInds[connPt1]] - yCenVec[connPt2], angle);
        errAlt = SIDES_ANGLE_ERROR(altLen, lenForAlt, angle);
        //if (bestError > maxConnError * maxConnError && sqrt(errAlt) <= maxConnError)
        //PRINT2(bestError, errAlt);
        ACCUM_MIN(bestError, errAlt);
      }
      if (altInds[connPt2] >= 0) {
        altLen = lengthAndAngle(angForAlt, xCenVec[connPt1] - xCenAlt[altInds[connPt2]],  
                                yCenVec[connPt1] - yCenAlt[altInds[connPt2]], angle);
        errAlt = SIDES_ANGLE_ERROR(altLen, lenForAlt, angle);
        //if (bestError > maxConnError * maxConnError && sqrt(errAlt) <= maxConnError)
        //PRINT2(bestError, errAlt);
        ACCUM_MIN(bestError, errAlt);
        if (altInds[connPt1] >= 0) {
          altLen = lengthAndAngle(angForAlt, 
                                  xCenAlt[altInds[connPt1]] - xCenAlt[altInds[connPt2]],  
                                  yCenAlt[altInds[connPt1]] - yCenAlt[altInds[connPt2]], 
                                  angle);
          errAlt = SIDES_ANGLE_ERROR(altLen, lenForAlt, angle);
          //if (bestError > maxConnError * maxConnError && sqrt(errAlt) <= maxConnError)
          //PRINT2(bestError, errAlt);
          ACCUM_MIN(bestError, errAlt);
        }
      }
    }
    error = sqrt(bestError);
    connErrors[ind] = error;
    angle = goodAngle(connAngles[ind] + 45 - mPeakAngAbove, 180);
    jnd = (int)((angle + 180.) / 90.);
    B3DCLAMP(jnd, 0, 3);
    connQuadrant[ind] = jnd;

    // If connection is a good one, put it on lists for the two points
    if (error <= maxConnError) {
      ptConnLists[connPoint1[ind]].push_back(ind);
      ptConnLists[connPoint2[ind]].push_back(ind);
    }
  }
    
  ptGridX.resize(numPoints, noGridVal);
  ptGridY.resize(numPoints, noGridVal);
  
  // Loop to find next point that has connectors and is not assigned
  for (ind = 0; ind < numPoints; ind++) {
    if (ptConnLists[ind].size() && ptGridX[ind] == noGridVal) {
      minGridX = minGridY = maxGridX = maxGridY = 0;
      numInGroup = 1;

      // Assign it 0,0 and put its connectors on queue
      ptGridX[ind] = 0;
      ptGridY[ind] = 0;
      for (jnd = 0; jnd < (int)ptConnLists[ind].size(); jnd++) {
        connQueue.push(ptConnLists[ind][jnd]);
        //printf("add conn %d\n", ptConnLists[ind][jnd]);
      }

      // Now process queue, which will grow faster than we can assign indexes
      while (connQueue.size() > 0) {
        con = connQueue.front();
        connQueue.pop();
        sign = 0;
        //printf("process conn %d\n", con);

        // If one point is unassigned, determine the from, to and sign for propagating
        if (ptGridX[connPoint1[con]] < hadGridVal && 
            ptGridX[connPoint2[con]] == noGridVal) {
          ptFrom = connPoint1[con];
          ptTo = connPoint2[con];
          sign = 1;
        } else if (ptGridX[connPoint1[con]] == noGridVal && 
                   ptGridX[connPoint2[con]] < hadGridVal) {
          ptFrom = connPoint2[con];
          ptTo = connPoint1[con];
          sign = -1;
        }

        // Propagate the index to the unassigned point
        if (sign) {
          ptGridX[ptTo] = ptGridX[ptFrom] + sign * delGridX[connQuadrant[con]];
          ptGridY[ptTo] = ptGridY[ptFrom] + sign * delGridY[connQuadrant[con]];
          ACCUM_MIN(minGridX, ptGridX[ptTo]);
          ACCUM_MIN(minGridY, ptGridY[ptTo]);
          ACCUM_MAX(maxGridX, ptGridX[ptTo]);
          ACCUM_MAX(maxGridY, ptGridY[ptTo]);
          //printf("Assign %d,%d to %d\n", ptGridX[ptTo], ptGridY[ptTo], ptTo);
      
          for (jnd = 0; jnd < (int)ptConnLists[ptTo].size(); jnd++) {
            if (ptConnLists[ptTo][jnd] != con) {
              connQueue.push(ptConnLists[ptTo][jnd]);
              //printf("add conn %d\n", ptConnLists[ptTo][jnd]);
            }
          }
          numInGroup++;
        }
      }

      // The queue is empty, all possible connections have been made from that starting 
      // point.  Set up the grid if there are enough points, otherwise reset the points
      if (numInGroup < minGroupSize) {
        setGridVal = rejectGridVal;

      } else {
        gridXdim = maxGridX + 1 - minGridX;
        gridYdim = maxGridY + 1 - minGridY;
        gridVec.clear();
        gridVec.resize(gridXdim * gridYdim, -1);
        if (mVerbose > 1)
          PRINT3(numInGroup, gridXdim, gridYdim);
        
        // Fill the grid with indexes
        for (jnd = 0; jnd < numPoints; jnd++) {
          if (ptGridX[jnd] < hadGridVal) {
            ix = ptGridX[jnd] - minGridX;
            iy = ptGridY[jnd] - minGridY;
            gridVec[ix + iy * gridXdim] = jnd;
          }
        }

        // Find empty positions with at least 3 neighbors
        for (iy = 0; iy < gridYdim; iy++) {
          for (ix = 0; ix < gridXdim; ix++) {
            doFit = true;
            if (gridVec[ix + iy * gridXdim] < 0) {
              
              // Empty position: count neighbors and fit if at least 3
              num = 0;
              if ((ix > 0 && gridVec[ix + iy * gridXdim - 1] >= 0) ||
                  (ix > 1 && gridVec[ix + iy * gridXdim - 2] >= 0))
                num++;
              if ((iy > 0 && gridVec[ix + (iy - 1) * gridXdim] >= 0) ||
                  (iy > 1 && gridVec[ix + (iy - 2) * gridXdim] >= 0))
                num++;
              if ((ix < gridXdim - 1 && gridVec[ix + iy * gridXdim + 1] >= 0) ||
                  (ix < gridXdim - 2 && gridVec[ix + iy * gridXdim + 2] >= 0))
                  num++;
              if ((iy < gridYdim - 1 && gridVec[ix + (iy + 1) * gridXdim] >= 0) ||
                  (iy < gridYdim - 2 && gridVec[ix + (iy + 2) * gridXdim] >= 0))
                num++;
              doFit = num >= 3;
              //PRINT4(ix,iy,num,doFit);
            }

            // Fitting: First evaluate the best center that contains the most points
            if (doFit) {
              
              bestNum = -1;
              for (jyCen = iy - fitLen / 2; jyCen <= iy + fitLen / 2; jyCen++) {
                for (jxCen = ix - fitLen / 2; jxCen <= ix + fitLen / 2; jxCen++) {
                  num = 0;
                  for (jy = jyCen - fitLen / 2; jy <= jyCen + fitLen / 2; jy++)
                    for (jx = jxCen - fitLen / 2; jx <= jxCen + fitLen / 2; jx++)
                      if (jx >= 0 && jx < gridXdim && jy >= 0 && jy < gridYdim &&
                          jx != ix && jy != iy && gridVec[jx + jy * gridXdim] >= 0)
                        num++;

                  offset = (jxCen - ix) * (jxCen - ix) + (jyCen - iy) * (jyCen - iy);
                  if (num > bestNum || (num == bestNum && offset < bestOffset)) {
                    bestNum = num;
                    bestOffset = offset;
                    bestXcen = jxCen;
                    bestYcen = jyCen;
                  }
                }
              }
              //PRINT4(ix, iy, bestXcen, bestYcen);

              // Set up the range around that center and load points, including this pos
              jxStart = B3DMAX(0, bestXcen - fitLen / 2);
              jxEnd = B3DMIN(gridXdim - 1, jxStart + fitLen - 1);
              jxStart = B3DMAX(0, jxEnd + 1 - fitLen);
              jyStart = B3DMAX(0, bestYcen - fitLen / 2);
              jyEnd = B3DMIN(gridYdim - 1, jyStart + fitLen - 1);
              jyStart = B3DMAX(0, jyEnd + 1 - fitLen);
              numFit = 0;
              for (jy = jyStart; jy <= jyEnd; jy++) {
                for (jx = jxStart; jx <= jxEnd; jx++) {
                  jnd = gridVec[jx + jy * gridXdim];
                  //if (jx != ix && jy != iy && jnd >= 0) {
                  if (jnd >= 0) {
                    fitDelIX[numFit] = (float)(jx - bestXcen);
                    fitDelIY[numFit] = (float)(jy - bestYcen);
                    fitX[numFit] = xCenVec[jnd];
                    fitY[numFit++] = yCenVec[jnd];
                    }
                }
              }

              // Do the fits

              jnd = 1;
              if (numFit >= minForRobust) {
                jnd = robustRegress(&xmat[0][0], maxFit, 0, 2, numFit, 2, 
                                    &solution[0][0], maxCol, fitConst, fitMean, fitSD,
                                    fitWork, kFactor, &numIter, maxIter, numFit / 6, 
                                    maxChange, maxOscill);
                if (!jnd) {
                  xPred = solution[0][0] * (ix - bestXcen) + 
                    solution[0][1] * (iy - bestYcen) + fitConst[0];
                  yPred = solution[1][0] * (ix - bestXcen) + 
                    solution[1][1] * (iy - bestYcen) + fitConst[1];
                }
              }
              if (jnd) {
                lsFit2Pred(fitDelIX, fitDelIY, fitX, numFit, &aFit, &bFit, &cFit, 
                  (float)(ix - bestXcen), (float)(iy - bestYcen), &xPred, &predErr);
                lsFit2Pred(fitDelIX, fitDelIY, fitY, numFit, &aFit, &bFit, &cFit, 
                  (float)(ix - bestXcen), (float)(iy - bestYcen), &yPred, &predErr);
              }
              jnd = gridVec[ix + iy * gridXdim];
              if (jnd >= 0) {
                bestError = DISTANCE(xCenVec[jnd] - xPred, yCenVec[jnd] - yPred);
                if (testAlt && altInds[jnd] >= 0) {
                  spacing = DISTANCE(xCenAlt[altInds[jnd]] - xPred, 
                                     yCenAlt[altInds[jnd]] - yPred);
                  if (spacing < bestError && spacing <= maxError) {
                    xCenVec[jnd] = xCenAlt[altInds[jnd]];
                    yCenVec[jnd] = yCenAlt[altInds[jnd]];
                    peakVals[jnd] = peakAlt[altInds[jnd]];
                    //PRINT2(spacing, bestError);
                    ACCUM_MIN(bestError, spacing);
                  }
                }
                if (jnd < 0 || bestError > maxError) {
                  xMissing.push_back(xPred);
                  yMissing.push_back(yPred);
                  if (jnd >= 0) {
                    //float dist = DISTANCE(xCenVec[jnd] - xPred, yCenVec[jnd] - yPred);
                    //PRINT4(xCenVec[jnd], xPred, yCenVec[jnd], yPred);
                    //PRINT2(dist,maxError);
                    ptGridX[jnd] = rejectGridVal;
                  }
                }
              }
            }
          }
        }
        setGridVal = hadGridVal;
      }

      // Prepare to find more points by marking this connected set as processed
      for (jnd = 0; jnd < numPoints; jnd++) {
        //PRINT2(jnd, ptGridX[jnd]);
        if (ptGridX[jnd] < hadGridVal)
          ptGridX[jnd] = setGridVal;
      }
    }
  }

  // Repack the position vectors for rejected positions
  jnd = 0;
  for (ind = 0; ind < numPoints; ind++) {
    if (ptGridX[ind] == hadGridVal) {
      xCenVec[jnd] = xCenVec[ind];
      yCenVec[jnd] = yCenVec[ind];
      peakVals[jnd++] = peakVals[ind];
    }
  }
  xCenVec.resize(jnd);
  yCenVec.resize(jnd);
  peakVals.resize(jnd);
  return 0;
}

// working from the peak angle in the coarse histogram, find the number, angle between,
// and length of connectors near that angle or 90 degrees away
void HoleFinder::refineAnglesGetLengths(FloatVec &connReducedAng, FloatVec &connLengths,
  float &peakAngBelow, float &peakAngAbove, float &avgLenBelow, float &avgLenAbove,
  int &numBelow, int &numAbove)
{
  int loop, ind, numConn = (int)connReducedAng.size();
  float angle, diffBelow, diffAbove, sd, sem;
  float maxAngDiff = 10., maxCoarseDiff = 12.5f;
  FloatVec lenBelow, lenAbove;

  for (loop = 0; loop < 2; loop++) {
    numBelow = 0;
    numAbove = 0;
    diffBelow = 0.;
    diffAbove = 0.;
    for (ind = 0; ind < numConn; ind++) {
      angle = goodAngle(connReducedAng[ind] - peakAngAbove);
      if (fabs(angle) < maxCoarseDiff) {
        numAbove++;
        diffAbove += angle;
      } else {
        angle = goodAngle(connReducedAng[ind] - peakAngBelow);
        if (fabs(angle) < maxCoarseDiff) {
          numBelow++;
          diffBelow += angle;
        }
      }
    }
    if (numBelow > 0)
      peakAngBelow += diffBelow / numBelow;
    if (numAbove > 0)
      peakAngAbove += diffAbove / numAbove;
    else
      peakAngAbove = peakAngBelow + 90.f;
    if (!numBelow)
      peakAngBelow = peakAngAbove - 90.f;
  }

  // Get separate median for the two directions
 // Refine the angle first by 
  for (loop = 0; loop < 2; loop++) {
    for (ind = 0; ind < numConn; ind++) {
      angle = goodAngle(connReducedAng[ind] - peakAngBelow);
      if (fabs(angle) < maxAngDiff) {
        lenBelow.push_back(loop ? connLengths[ind] : angle);
      } else {
        angle = goodAngle(connReducedAng[ind] - peakAngAbove);
        if (fabs(angle) < maxAngDiff)
          lenAbove.push_back(loop ? connLengths[ind] : angle);
      }
    }

    if (lenAbove.size() > 3)
      rsFastMedianInPlace(&lenAbove[0], (int)lenAbove.size(), &avgLenAbove);
    else if (lenAbove.size())
      avgSD(&lenAbove[0], (int)lenAbove.size(), &avgLenAbove, &sd, &sem);
    if (lenBelow.size() > 3)
      rsFastMedianInPlace(&lenBelow[0], (int)lenBelow.size(), &avgLenBelow);
    else if (lenBelow.size())
      avgSD(&lenBelow[0], (int)lenBelow.size(), &avgLenBelow, &sd, &sem);
    else
      avgLenBelow = avgLenAbove;
    if (!lenAbove.size())
      avgLenAbove = avgLenBelow;

    if (!loop) {
      /*for (jnd = 0; jnd < lenBelow.size(); jnd++)
      printf("%.2f\n", lenBelow[jnd]);
      for (jnd = 0; jnd < lenAbove.size(); jnd++)
      printf("%.2f\n", lenAbove[jnd]); */
      peakAngBelow = goodAngle(peakAngBelow + avgLenBelow);
      peakAngAbove = goodAngle(peakAngAbove + avgLenAbove);
      lenAbove.clear();
      lenBelow.clear();
    }
  }
  numBelow = (int)lenBelow.size();
  numAbove = (int)lenAbove.size();
  if (peakAngBelow > peakAngAbove) {
    B3DSWAP(peakAngBelow, peakAngAbove, sem);
    B3DSWAP(avgLenBelow, avgLenAbove, sem);
    B3DSWAP(numBelow, numAbove, ind);
  }
  if (mVerbose) {
    //PRINT4(peakAngBelow, avgLenBelow, peakAngAbove, avgLenAbove);
  }
}

/*
 * Returns the given angle rotated by +2 or -2 * limit to be between -limit and +limit
 * (default for limit is 90).
 */
float HoleFinder::goodAngle(float angle, float limit)
{
  if (angle <= -limit)
    angle += 2.f * limit;
  if (angle > limit)
    angle -= 2.f * limit;
  return angle;
}

/*
 * returns the length of the vector dx,dy and the angle between it and baseAng
 */
float HoleFinder::lengthAndAngle(float baseAng, float dx, float dy, float &angle)
{
  angle = goodAngle((float)(atan2(dy, dx) / RADIANS_PER_DEGREE - baseAng));
  return sqrt(dx * dx + dy * dy);
}

/*
 * Makes a template for correction by averaging either the edge image or the raw image
 * xCenVec, yCenVec are the arrays of positions,
 * numAvg is the number to average
 * separation is the distance between adjacent points in reduced pixels
 * rawAverage is the flag to average the raw instead of edge image
 */
int HoleFinder::makeTemplate(FloatVec &xCenVec, FloatVec &yCenVec,
                             int numAverage, float separation, bool rawAverage)
{
  float boxReal = 1.8f * (separation - mLastBestRadius) / sqrt(2.f);
  int ind, jnd, boxSq, imgBoxSize;
  float *tempBox, *boxAverage;
  float *tempData = mKeepCache ? mCrossCorr : mSobelGrad;
  if (rawAverage)
    tempData = mRawData;

  imgBoxSize = B3DNINT(boxReal / 2.) * 2;
  mAvgBoxSize = imgBoxSize + 16;
  boxSq = mAvgBoxSize * mAvgBoxSize;
  tempBox = B3DMALLOC(float, boxSq);
  if (!tempBox)
    return 1;
  if (!rawAverage) {
    for (ind = 0; ind < mRawDataSize; ind++) {
      if (mEdgeData[ind] == 255)
        tempData[ind] = 1.;
      else
        tempData[ind] = 0.;
    }
    B3DFREE(mEdgeAverage);
    mEdgeAverage = B3DMALLOC(float, boxSq);
    boxAverage = mEdgeAverage;
  } else {
    B3DFREE(mRawAverage);
    mRawAverage = B3DMALLOC(float, boxSq);
    boxAverage = mRawAverage;
    mRawBoxSize = mAvgBoxSize;
  }
  if (!boxAverage)
    return 1;
  memset(boxAverage, 0, 4 * boxSq);
  float amat[2][2] = {{1., 0.}, {0., 1.}};
  for (ind = 0; ind < numAverage; ind++) {
    cubinterp(tempData, tempBox, mXsize, mYsize, imgBoxSize, imgBoxSize, amat, 
              (xCenVec[ind] - mRedXoffset) / mReduction, 
              (yCenVec[ind] - mRedYoffset) / mReduction , 0., 0., 1., 0., 0);
    for (jnd = 0; jnd < boxSq; jnd++)
      boxAverage[jnd] += tempBox[jnd];
  }
  for (jnd = 0; jnd < imgBoxSize * imgBoxSize; jnd++) {
    boxAverage[jnd] /= numAverage;
  }
  sliceTaperOutPad(boxAverage, SLICE_MODE_FLOAT, imgBoxSize, imgBoxSize, boxAverage,
                   mAvgBoxSize, mAvgBoxSize, mAvgBoxSize, 0, 0.);
  if (mDebugImages)
    mrcWriteImageToFile(rawAverage ? "rawavg.mrc" : "edgeavg.mrc", boxAverage, 2, 
                        mAvgBoxSize, mAvgBoxSize);
  mRadiusForEdgeAvg = mLastBestRadius;
  if (rawAverage)
    mHaveRawAvgFFT = false;
  else
    mHaveEdgeAvgFFT = false;
  free(tempBox);
  return 0;
}


/*
 * Takes a set of points found by correlating with an edge image including the weak edge 
 * points and matches them with a list of missing points.  If one is below the error
 * limit, it add the weak point to the main set of points and removes it from the 
 * missing lists
 * xCenOrig, yCenOrig, peakOrig  are the original detected points
 * xMissing, yMissing  are list of missing points
 * xCenWeak, yCenWeak, peakWeak  are points found by correlating with the weak edge image
 * All vectors are in original pixels
 * maxError is the maximum error, in original pixels
 * numAdded is returned with the number added
 */
int HoleFinder::rescueMissingPoints(FloatVec &xCenOrig, FloatVec &yCenOrig,
                                    FloatVec &peakOrig, FloatVec &xMissing, 
                                    FloatVec &yMissing, FloatVec &xCenWeak,
                                    FloatVec &yCenWeak, FloatVec &peakWeak,
                                    float maxError, int &numAdded)
{
  int weak, miss, ind;
  IntVec fartherInds;
  float dist;
  numAdded = 0;
  if (!mRawData)
    return ERR_NOT_INIT;
  for (miss = (int)xMissing.size() - 1; miss >= 0; miss--) {
    for (weak = 0; weak < (int)xCenWeak.size(); weak++) {
      dist = DISTANCE(xMissing[miss] - xCenWeak[weak], yMissing[miss] - yCenWeak[weak]);
      if (dist < maxError) {
        xCenOrig.push_back(xCenWeak[weak]);
        yCenOrig.push_back(yCenWeak[weak]);
        peakOrig.push_back(peakWeak[weak]);
        xMissing.erase(xMissing.begin() + miss);
        yMissing.erase(yMissing.begin() + miss);
        numAdded++;
        break;
      } else if (dist < 2 * maxError) {
        
        fartherInds.push_back(weak);
        break;
      }
    }
  }

  // Repack the weak peak vectors that are close but not close enough
  if (fartherInds.size())
    std::sort(fartherInds.begin(), fartherInds.end()); 
  for (ind = 0; ind < (int)fartherInds.size(); ind++) {
    miss = fartherInds[ind];
    xCenWeak[ind] = xCenWeak[miss];
    yCenWeak[ind] = yCenWeak[miss];
    peakWeak[ind] = peakWeak[miss];
  } 
  xCenWeak.resize(ind);
  yCenWeak.resize(ind);
  peakWeak.resize(ind);
  return 0;
}


/*
 * Computes mean, SD and/or fraction of outlying black pixels within a circle around 
 * each given position
 * Radius is the actual radius around each point to include in analysis, in reduced pixels
 * xCenters, yCenters are the points to analyze
 * set reduced true if they are in reduced pixels in a padded correlation image, otherwise
 * they must be in original pixels
 * set smooothSD true to measure SD from smoothed values by box-car averaging each 3x3
 * set of pixels
 * Pass means non-NULL to return means
 * Pass SDs non-null to return SDs
 * Pass outliers non-null to return fraction of smoothed pixels that are negative outliers
 * with criterion outlieCrit (default 2.5)
 */
int HoleFinder::holeIntensityStats(float radius, FloatVec &xCenters, FloatVec &yCenters,
                                   bool reduced, bool smoothedSD, FloatVec *means,
                                   FloatVec *SDs, FloatVec *outliers, float outlieCrit) 
{
  int ind, ix, iy, ixStart, iyStart, ixEnd, iyEnd, nsum, numPoints, xbase, numBelow;
  float xcen, ycen, avg, sd, val, sem, fracLow;
  double sum, sumsq;
  FloatVec tmpVec, valVec;

  if (!mRawData)
    return ERR_NOT_INIT;
  numPoints = (int)xCenters.size();
  if (means)
    means->resize(numPoints);
  if (SDs)
    SDs->resize(numPoints);
  if (outliers)
    outliers->resize(numPoints);
  for (ind = 0; ind < (int)xCenters.size(); ind++) {
    sum = 0.;
    sumsq = 0.;
    nsum = 0;
    avg = 0.;
    sd = 0.;
    valVec.clear();
    if (reduced) {
      xcen = (float)((xCenters[ind] - mPadXoffset) - 0.5);
      ycen = (float)((yCenters[ind] - mPadYoffset) - 0.5);
    } else {
      xcen = (float)((xCenters[ind] - mRedXoffset) / mReduction - 0.5);
      ycen = (float)((yCenters[ind] - mRedYoffset) / mReduction - 0.5);
    }
    ixStart = B3DMAX(1, (int)(xcen - radius - 1));
    ixEnd = B3DMIN(mXsize - 2, (int)(xcen + radius + 1));
    iyStart = B3DMAX(1, (int)(ycen - radius - 1));
    iyEnd = B3DMIN(mYsize - 2, (int)(ycen + radius + 1));
    for (iy = iyStart; iy <= iyEnd; iy++) {
      for (ix = ixStart; ix <= ixEnd; ix++) {
        if (DISTANCE(ix - xcen, iy - ycen) <= radius) {
          xbase = ix + iy * mXsize;
          val = mRawData[xbase];
          sum += val;
          sumsq += val * val;
          nsum++;
          if (outliers || (SDs && smoothedSD)) {
            val += mRawData[xbase - 1] + mRawData[xbase + 1] + mRawData[xbase + mXsize] +
              mRawData[xbase + mXsize - 1] + mRawData[xbase + mXsize + 1] +
              mRawData[xbase - mXsize] + mRawData[xbase - mXsize - 1] + 
              mRawData[xbase - mXsize + 1];
            val /= 9;
            valVec.push_back(val);
          }
        }
      }
    }
    if (nsum) {
      sumsToAvgSDdbl(sum, sumsq, nsum, 1, &avg, &sd);
    }
    if (means)
      (*means)[ind] = avg;
    if (SDs) {
      if (smoothedSD && nsum)
        avgSD(&valVec[0], nsum, &avg, &sd, &sem);
      (*SDs)[ind] = sd;
   } 
    if (outliers) {
      fracLow = 0.;
      if (nsum >= 10) {
        tmpVec.resize(nsum);
        rsMadMedianOutliers(&valVec[0], nsum, outlieCrit, &tmpVec[0]);
        numBelow = 0;
        for (ix = 0; ix < nsum; ix++)
          if (tmpVec[ix] < 0)
            numBelow++;
        fracLow = (float)numBelow / nsum;

      }
      (*outliers)[ind] = fracLow;
    }
  }
  return 0;
}

/*
 * Removes points that have outlying values for the give set of values
 * xCenters, yCenters are the positions of the points, and peakVals are their 
 * correlation peak values, or can be NULL if there are none
 * values has the mean, SD, or other statistic
 * crit is the criterion for rsMadMedianOutliers
 * direction is the sum of 1 to remove negative and 2 to remove positive outliers
 * numNeg and numPos are returned with the number of negative and positive removals
 */
int HoleFinder::removeOutliers
(FloatVec &xCenters, FloatVec &yCenters, FloatVec *peakVals, FloatVec &values,
 float negCrit, float posCrit, int &numNeg, int &numPos, bool useCutVals,
 float *negCutVal, float *posCutVal)
{
  int ind, outInd, numPoints = (int)xCenters.size();
  float median, MADN, posCutoff, negCutoff;
  FloatVec outlie;
  bool doNeg = negCrit > 0.;
  bool doPos = posCrit > 0.;
  numPos = numNeg = 0;
  if (useCutVals) {
    if ((doNeg && !negCutVal) || (doPos && !posCutVal))
      return ERR_NO_CUTOFFS;
    if (negCutVal)
      negCutoff = *negCutVal;
    if (posCutVal)
      posCutoff = *posCutVal;
  } else {
    outlie.resize(numPoints);
    rsMedian(&values[0], numPoints, &outlie[0], &median);
    rsMADN(&values[0], numPoints, median, &outlie[0], &MADN);
    posCutoff = median + posCrit * MADN;
    negCutoff = median - negCrit * MADN;
    if (negCutVal)
      *negCutVal = negCutoff;
    if (posCutVal)
      *posCutVal = posCutoff;
  }
      
  outInd = 0;
  for (ind = 0; ind < numPoints; ind++) {
    if (doPos && values[ind] > posCutoff) {
      numPos++;
    } else if (doNeg && values[ind] < negCutoff) {
      numNeg++;
    } else {
      if (peakVals)
        (*peakVals)[outInd] = (*peakVals)[ind];
      xCenters[outInd] = xCenters[ind];
      yCenters[outInd++] = yCenters[ind];
    }
  }
  xCenters.resize(outInd);
  yCenters.resize(outInd);
  if (peakVals)
    peakVals->resize(outInd);
  return 0;
}

/*
 * Write a text file with points.  Can be sent a Z argument, or the default iz can be
 * changed in holefinder.h
 */
void HoleFinder::dumpPoints(const char *filename, FloatVec &xCenters, FloatVec &yCenters, 
                            FloatVec &peakVals, int iz)
{
 FILE *fp = fopen(filename, "w");
 for (int ind = 0; ind < (int)xCenters.size(); ind++) {
   if (peakVals[ind] >= -1.e29)
     fprintf(fp, "%.2f %.2f %d %g\n", xCenters[ind], yCenters[ind], iz, peakVals[ind]);
 }
 fclose(fp);
}


/*
 * Given a collection of positions, determines their indexes on a regular grid, given the
 * angle and spacing of the grid.  Pass the angle as -999 or the spacing as -1 to use
 * and return the average positive angle or spacing from analyzeNeighbors.  GridX and 
 * gridY are returned with indexes numbered from 0
 */
#define ASSIGN_ADD_TO_QUEUE(nd, x, y) \
  gridX[nd] = x;                      \
  gridY[nd] = y;                                \
  crossInd[(x) + xdim * (y)] = nd;              \
  neighQueue.push(nd);

void HoleFinder::assignGridPositions
(FloatVec &xCenters, FloatVec &yCenters, ShortVec &gridX, ShortVec &gridY, 
 float &avgAngle, float &avgLen)
{
  FloatVec xrot, yrot;
  ShortVec xPrelim, yPrelim;
  IntVec indPrelim, crossInd;
  std::queue<int> neighQueue;
  int ix, iy, ixCen, iyCen, jnd, xdim, ydim, ind, numPoints = (int)xCenters.size();
  int cenInd, indMin, idx, idy, delMin, indXbase, indYbase;
  float rcos, rsin, xmin, xmax, ymin, ymax, dist, minDist;
  float xcen, ycen, xbase, ybase;

  gridX.clear();
  gridY.clear();
  gridX.resize(numPoints, -1);
  gridY.resize(numPoints, -1);

  if (avgAngle < -180)
    avgAngle = (mPeakAngBelow + 90.f + mPeakAngAbove) / 2.f;
  if (avgLen < 0)
    avgLen = (mAvgLenAbove + mAvgLenBelow) / 2.f;
  rcos = (float)cos(RADIANS_PER_DEGREE * avgAngle);
  rsin = -(float)sin(RADIANS_PER_DEGREE * avgAngle);
  
  // Rotate the points to be square to the grid
  xrot.resize(numPoints);
  yrot.resize(numPoints);
  xmin = ymin = 1.e30f;
  xmax = ymax = -1.e30f;
  for (ind = 0; ind < numPoints; ind++) {
    xrot[ind] = rcos * xCenters[ind] - rsin * yCenters[ind];
    yrot[ind] = rsin * xCenters[ind] + rcos * yCenters[ind];
    ACCUM_MIN(xmin, xrot[ind]);
    ACCUM_MIN(ymin, yrot[ind]);
    ACCUM_MAX(xmax, xrot[ind]);
    ACCUM_MAX(ymax, yrot[ind]);
  }

  // Set up preliminary array and parameters
  xdim = (int)(2. * (xmax - xmin) / avgLen + 2);
  ydim = (int)(2. * (ymax - ymin) / avgLen + 2);
  indPrelim.resize(xdim * ydim, -1);
  xPrelim.resize(numPoints);
  yPrelim.resize(numPoints);
  xbase = xmin - 0.5f * avgLen;
  ybase = ymin - 0.5f * avgLen;
  indXbase = (int)((xdim - ((xmax - xmin) / avgLen)) / 2.);
  indYbase = (int)((ydim - ((ymax - ymin) / avgLen)) / 2.);
  xcen = (xmax + xmin) / 2.f;
  ycen = (ymax + ymin) / 2.f;
  minDist = 1.e30f;

  // assign preliminary indexes, plus find middle.  All points have a prelim index that
  // may be off, but the cross-index is not reliable
  for (ind = 0; ind < numPoints; ind++) {
    xPrelim[ind] = indXbase + B3DNINT((xrot[ind] - xbase) / avgLen);
    yPrelim[ind] = indYbase + B3DNINT((yrot[ind] - ybase) / avgLen);
    indPrelim[xPrelim[ind] + yPrelim[ind] * xdim] = ind;
    dist = DISTANCE(xrot[ind] - xcen, yrot[ind] - ycen);
    if (dist < minDist) {
      indMin = ind;
      minDist = dist;
    }
  }

  // Start a queue with the point nearest the middle, assign it its own preliminary index
  crossInd.resize(xdim * ydim, -1);
  ASSIGN_ADD_TO_QUEUE(indMin, xPrelim[indMin], yPrelim[indMin]);

  // Process points on queue
  while (neighQueue.size() > 0) {
    cenInd = neighQueue.front();
    neighQueue.pop();
    ixCen = gridX[cenInd];
    iyCen = gridY[cenInd];
    xcen = xrot[cenInd];
    ycen = yrot[cenInd];
    
    // Phase 1: use prelim index to find neighboring points and check each one to see
    // if it a nearest neighbor of current point
    for (iy = B3DMAX(0, iyCen - 2); iy <= B3DMIN(ydim - 1, iyCen + 2); iy++) {
      for (ix = B3DMAX(0, ixCen - 2); ix <= B3DMIN(xdim - 1, ixCen + 2); ix++) {
        if (ix == ixCen && iy == iyCen)
          continue;
        ind = indPrelim[ix + iy * xdim];
        if (ind >= 0 && gridX[ind] < 0) {
          idx = B3DNINT((xrot[ind] - xcen) / avgLen);
          idy = B3DNINT((yrot[ind] - ycen) / avgLen);
          if (idx >= -1 && idx <= 1 && idy >= -1 && idy <= 1) {

            // Assign and add to queue
            ASSIGN_ADD_TO_QUEUE(ind, ixCen + idx, iyCen + idy);
          }
        }
      }
    }

    // Phase 2: check each position and if empty search for match among all points
    for (iy = B3DMAX(0, iyCen - 1); iy <= B3DMIN(ydim - 1, iyCen + 1); iy++) {
      for (ix = B3DMAX(0, ixCen - 1); ix <= B3DMIN(xdim - 1, ixCen + 1); ix++) {
        if (crossInd[ix + xdim * iy] < 0) {
          for (ind = 0; ind < numPoints; ind++) {
            if (gridX[ind] < 0 && B3DABS(ix - xPrelim[ind]) <= 2 && 
                B3DABS(iy - yPrelim[ind]) <= 2) {
              idx = B3DNINT((xrot[ind] - xcen) / avgLen);
              idy = B3DNINT((yrot[ind] - ycen) / avgLen);
              if (B3DABS(ix + idx) <= 1 && B3DABS(iy + idy) <= 1) {
                ASSIGN_ADD_TO_QUEUE(ind, ix + idx, iy + idy);
                break;
              }
            }
          }
        }
      }
    }

    // If queue is now empty, find closest approach between unassigned and assigned point
    // to restart it
    delMin = xdim + ydim;
    indMin = -1;
    if (!neighQueue.size()) {
      for (ind = 0; ind < numPoints; ind ++) {
        if (gridX[ind] < 0) {
          for (jnd = 0; jnd < numPoints; jnd++) {
            if (gridX[jnd] >= 0) {
              idx = B3DABS(xPrelim[ind] - xPrelim[jnd]);
              idy = B3DABS(yPrelim[ind] - yPrelim[jnd]);
              if (B3DMAX(idx, idy) < delMin) {
                delMin = B3DMAX(idx, idy);
                indMin = ind;
                idx = B3DNINT((xrot[ind] - xrot[jnd]) / avgLen);
                idy = B3DNINT((yrot[ind] - yrot[jnd]) / avgLen);
                ixCen = gridX[jnd] + idx;
                iyCen = gridY[jnd] + idy;
              }
            }
          }
        }
      }
      if (indMin >= 0) {
        ASSIGN_ADD_TO_QUEUE(indMin, ixCen, iyCen);
      }
    }
  }

  // All have indexes, now need to shift them to 0
  idx = *std::min_element(gridX.begin(), gridX.end());
  idy = *std::min_element(gridY.begin(), gridY.end());
  for (ind = 0; ind < numPoints; ind ++) {
    gridX[ind] -= idx;
    gridY[ind] -= idy;
  }
}

/*
 * Convert the average lengths and angles found from a call to analyzeNeighbors just to
 * find spacing into two vectors with the amount of movement dX,
 * dY per step in gridX or gridY.  X corresponds to the vector "above" and Y to the
 * vector "below" rotated by 180
 */
void HoleFinder::getGridVectors(float &gridXdX, float &gridXdY, float &gridYdX, 
  float &gridYdY, float &avgAngle, float &avgLen)
{
  gridXdX = mJustLenAbove * (float)cos(RADIANS_PER_DEGREE * mJustAngAbove);
  gridYdX = mJustLenAbove * (float)sin(RADIANS_PER_DEGREE * mJustAngAbove);
  gridXdY = mJustLenBelow * (float)cos(RADIANS_PER_DEGREE * (mJustAngBelow + 180.));
  gridYdY = mJustLenBelow * (float)sin(RADIANS_PER_DEGREE * (mJustAngBelow + 180.));
  avgAngle = 0.5f * (mJustAngBelow + 90.f + mJustAngAbove);
  avgLen = 0.5f * (mJustLenBelow + mJustLenAbove);
}
