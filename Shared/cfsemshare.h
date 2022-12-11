/*   cfsemshare.h   - functions in multiple files, mostly shared between C and
 *                      Fortran and/or IMOD and SerialEM
 *
 *   Copyright (C) 1995-2020 by the Regents of the University of Colorado.
 *
 *   $Id$
 */                                                                           

#ifndef CFSEMSHARE_H
#define CFSEMSHARE_H

#include "mrcslice.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef F77FUNCAP
#define icalc_angles ICALC_ANGLES
#define metro METRO
#define montXCSetDistWeightHalfFall MONTXCSETDISTWEIGHTHALFFALL
#define montXCGetLastTrimmedMaxSD MONTXCGETLASTTRIMMEDMAXSD
#else
#define metro metro_
#define montXCSetDistWeightHalfFall montxcsetdistweighthalffall_
#define montXCGetLastTrimmedMaxSD montxcgetlasttrimmedmaxsd_
#ifdef G77__HACK
#define icalc_angles icalc_angles__
#else
#define icalc_angles icalc_angles_
#endif
#endif

#define MAX_MBS_SCALES 16
#define MAX_DUAL_AMOEBA_VAR  16

  /* parselist.c  - for parsing a list of integers */
  int *parselist (const char *line, int *nlist);

  /* writelist.c  - for converting and writing a list as ranges */
  int writeList(int *list, int numList, int lineLen);
  char *listToString(int *list, int numList);

  /* amoeba.c - simplex minimization routine */
  void amoeba(float *p, float *y, int mp, int ndim, float ftol, 
              void (*funk)(float *, float *), int *iterP, float *ptol,
              int *iloP);
  void amoebaInit(float *p, float *y, int mp, int ndim, float delfac, 
                  float ptolFac, float *a, float *da, 
                  void (*funk)(float *, float *), float *ptol);
  void dualAmoeba(float *y, int ndim, float delfac, float *ptolFacs, float *ftolFacs, 
                  float *a, float *da, void (*funk)(float *, float *), int *iterP);

  /* samplemeansd.c - for computing mean and SD quickly by sampling */
  int sampleMinMaxMeanSD(unsigned char **image, int type, int nx, int ny, 
                         float sample, int ixStart, int iyStart, int nxUse, int nyUse,
                         float *mean, float *sd, float *amin, float *amax);
  int sampleMeanSD(unsigned char **image, int type, int nx, int ny,
                   float sample, int nxMatt, int myMatt, int nxUse, int nyUse,
                   float *mean, float *sd);
  int sampleMinMaxMean(unsigned char **image, int type, int nx, int ny, 
                       float sample, int ixStart, int iyStart, int nxUse, int nyUse,
                       float *mean, float *amin, float *amax);
  int sampleMeanOnly(unsigned char **image, int type, int nx, int ny, float sample,
                     int ixStart, int iyStart, int nxUse, int nyUse, float *mean);
  int typeForSampleMean(int mrcMode);
  int getSampleOfArray(void *image, int mode, int nx, int ny, float sampleFrac,
                       int ixStart, int iyStart, int nxUse, int nyUse,
                       float filltoExclude, float *samples, int maxSamples, 
                       int *numSamples);

  /* pctstretch.c - for computing percentile limits quickly by sampling */
  int percentileStretch(unsigned char **image, int mode, int nx, int ny, float sample,
                        int ixStart, int iyStart, int nxUse, int nyUse, 
                        float pctLo, float pctHi, float *scaleLo, float *scaleHi);

  /* colormap.c */
  int *cmapStandardRamp(void);
  int *cmapInvertedRamp(void);
  int cmapConvertRamp(int *rampData, unsigned char table[3][256]);
  int cmapReadConvert(char *filename, unsigned char table[3][256]);

  /* cubinterp.c */
  void cubinterp(float *array, float *bray, int nxa, int nya, int nxb, int nyb,
                 float amat[2][2], float xc, float yc, float xt, float yt,
                 float scale, float dmean, int linear);

  /* reduce_by_binning.c */
  int extractAndBinIntoArray(void *array, int type, int nxDim, int xStart, int xEnd,
                             int yStart, int yEnd, int nbin, void *brray, int nxBdim,
                             int bxOffset, int byOffset, int keepByte, int *nxr,
                             int *nyr);
  int extractWithBinning(void *array, int type, int nxDim, int xStart, int xEnd,
                         int yStart, int yEnd, int nbin, void *brray, int keepByte, 
                         int *nxr, int *nyr);
  int reduceByBinning(void *array, int type, int nxin, int nyin, int nbin, 
                      void *brray, int keepByte, int *nxr, int *nyr);
  void binIntoSlice(float *array, int nxDim, float *brray, int nxBin, int nyBin,
                    int binFacX, int binFacY, float zWeight);
  void repackFloatImage(void *brray, void *array, int nxin, int xStart, int xEnd,
                        int yStart, int yEnd);

  /* filtxcorr.c */
  int niceFrame(int num, int idnum, int limit);
  void XCorrSetCTF(float sigma1, float sigma2, float radius1, float radius2,
                   float *ctf, int nx, int ny, float *delta);
  void XCorrSetCTFnoScl(float sigma1, float sigma2, float radius1,
                        float radius2, float *ctf, int nx,int ny,
                        float *delta, int *nsizeOut);
  void XCorrFilterPart(float *fft, float *array, int nx, int ny, float *ctf, 
                       float delta);
  void doseWeightFilter(float startDose, float endDose, float pixelSize, float afac, 
                        float bfac, float cfac, float doseScale, float *ctf, int numVals,
                        float maxFreq, float *delta);
  void doseFilterValue(float startDose, float endDose, float frequency, float afac,
                       float bfac, float cfac, float doseScale, float *atten);
  void XCorrMeanZero(float *array, int nxdim, int nx, int ny);
  void XCorrPeakFind(float *array, int nxdim, int ny, float  *xpeak,
                     float *ypeak, float *peak, int maxpeaks);
  void XCorrPeakFindWidth(float *array, int nxdim, int ny, float  *xpeak, float *ypeak,
                          float *peak, float *width, float *widthSD, int maxpeaks, 
                          float minStrength);
  void setPeakFindLimits(int limXlo, int limXhi, int limYlo, int limYhi, int useEllipse);
  int findManyXCorrPeaks(float *array, int nxdim, int ny, int ixOffset, 
                         int iyOffset, float *xpeak, float *ypeak, float *peak,
                         int maxPeaks, int maxGrow, int *numFound);
  int findSpacedXCorrPeaks(float *array, int nxdim, int ixMin, int ixMax, int iyMin,
                           int iyMax, float *xpeak, float *ypeak, float *peak,
                           int maxPeaks, float minSpacing, int *numPeaks,
                           float minStrength);
  double parabolicFitPosition(float y1, float y2, float y3);
  void conjugateProduct(float *array, float *brray, int nx, int ny);
  double XCorrCCCoefficient(float *array, float *brray, int nxdim, int nx,
                            int ny, float xpeak, float ypeak, int nxpad,
                            int nypad, int *nsum);
  double CCCoefficientTwoPads(float *array, float *brray, int nxdim, int nx, int ny,
                              float xpeak, float ypeak, int nxpadA, int nypadA,
                              int nxpadB, int nypadB, int minPixels, int *nsum);
  double subareaCCCoefficient(float *array, float *brray, int nxdim, int xstart, int xend,
                              int ystart, int yend, int delx, int dely);
  double weightedCCCoefficient(float *array, float *brray, int nxDim, int ix0, int ix1,
                               int iy0, int iy1, int delX, int delY, float *aWeights,
                               float *bWeights, int nxWgt, int binning, int wgtXoffset,
                               int wgtYoffset);
  double weightedCorrFromSums(double aSum, double aSumSq, double bsum, double bSumSq,
                              double abSum, double wSum, double *sumArray,
                              const char *descrip);
  void sliceGaussianKernel(float *mat, int dim, float sigma);
  void scaledGaussianKernel(float *mat, int *dim, int limit, float sigma);
  void applyKernelFilter(float *array, float *brray, int nxdim, int nx, int ny,
                         float *mat, int kdim);
  void wrapFFTslice(float *array, float *tmpArray, int nx, int ny, int direction);
  int indicesForFFTwrap(int ny, int direction, int *iyOut, int *iyLow, int *iyHigh);
  void fourierShiftImage(float *fft, int nxPad, int nyPad, float dx, float dy,
                         float *temp);
  void fourierReduceImage(float *fftIn, int nxrIn, int nyrIn, float *fftOut, int nxrOut,
                          int nyrOut, float dxIn,float dyIn, float *temp);
  void fourierExpandImage(float *fftIn, int nxrIn, int nyrIn, float *fftOut, int nxrOut,
                          int nyrOut, float dxIn,float dyIn, float *temp);
  void fourierRingCorr(float *ffta, float *fftb, int nxReal, int ny, float *ringCorrs,
                       int maxRings, float deltaR, float *temp);
  int fourierCropSizes(int size, float factor, float padFrac, int minPad, int niceLimit,
                       int *fullPadSize, int *cropPadSize, float *actualFac);
    
  /* taperpad.c */
  void sliceTaperOutPad(void *array, int type, int nxbox, int nybox, 
                        float *brray, int nxdim, int nx, int ny, int ifmean,
                        float dmeanin);
  void sliceTaperInPad(void *array, int type, int nxdimin, int ix0, int ix1,
                       int iy0, int iy1, float *brray, int nxdim, int nx,
                       int ny, int nxtap, int nytap);
  double sliceEdgeMean(float *array, int nxdim, int ixlo, int ixhi, int iylo,
                       int iyhi);
  float imageEdgeMean(void *array, int type, int nxdim, int ixlo, int ixhi, int iylo, 
                      int iyhi);
  double sliceEdgeMedian(float *array, int nxdim, int ixlo, int ixhi, int iylo,
                         int iyhi, int meanOfSides);
  void sliceSplitFill(float *array, int nxbox, int nybox, float *brray,
                      int nxdim, int nx, int ny, int iffill, float fillin);
  void sliceSmoothOutPad(void *array, int type, int nxbox, int nybox, 
                           float *brray, int nxdim, int nx, int ny);
  void sliceNoiseTaperPad(void *array, int type, int nxbox, int nybox, float *brray,
                          int nxdim, int nx, int ny, int noiseLength, int noiseRows,
                          float *temp);

  /* taperatfill.c */
  int sliceTaperAtFill(Islice *sl, int ntaper, int inside);
  int taperAtFill(void *array, int type, int nx, int ny, int ntaper, int inside);
  int getLastTaperFillValue(float *value);
  void sliceFindFillValue(Islice *sl, float *fillvalP, int *longestP, int *longixP,
                          int *longiyP, int *dirP);
  int sliceReplaceFill(Islice *sl, int median, int haveFill, float *oldFill, 
                       float *newFill);
  
  /* circlefit.c */
  int circleThrough3Pts(float x1, float y1, float x2, float y2, float x3, 
                             float y3, float *rad, float *xc, float *yc);
  int fitSphere(float *xpt, float *ypt, float *zpt, int numPts,
                     float *rad, float *xcen, float *ycen, float *zcen,
                     float *rmsErr);
  int fitSphereWgt(float *xpt, float *ypt, float *zpt, float *weights,
                   int numPts, float *rad, float *xcen, float *ycen,
                   float *zcen, float *rmsErr);

  /* insidecontour.c */
  int InsideContour(float *ptX, float *ptY, int np, float x, float y);

  /* scaledsobel.c */
  int scaledSobel(float *inImage, int nxin, int nyin, float scaleFac, 
                  float minInterp, int linear, float center, float *outImage,
                  int *nxout, int *nyout, float *xOffset, float *yOffset);

  /* histogram.c */
  void kernelHistogram(float *values, int numVals, float *bins, int numBins,
                       float firstVal, float lastVal, float h, int verbose);
  int scanHistogram(float *bins, int numBins, float firstVal, float lastVal,
                    float scanBot, float scanTop, int findPeaks, float *dip,
                    float *peakBelow, float *peakAbove);
  int findHistogramDip(float *values, int numVals, int minGuess, float *bins,
                       int numBins, float firstVal, float lastVal, 
                       float *histDip, float *peakBelow, float *peakAbove,
                       int verbose);
  /* simplestat.c */
  void avgSD(float *x, int n, float *avg, float *sd, float *sem);
  void sumsToAvgSD(float sx, float sxsq, int n, float *avg, float *sd);
  void sumsToAvgSDdbl(double sx8, double sxsq8, int n1, int n2, float *avg,
                      float *sd);
  void sumsToAvgSDallDbl(double sx8, double sxsq8, int n1, int n2, double *avg,
                         double *sd);
  void arrayMinMaxMean(float *array, int nx, int ny, int ix0, int ix1, int iy0, int iy1,
                       float *dmin, float *dmax, float *dmean);
  float imageSubareaMean(void *array, int type, int nxDim, int ix0, int ix1, int iy0,
                         int iy1);
  void arrayMinMaxMeanSd(float *array, int nx, int ny, int ix0, int ix1, int iy0, int iy1,
                         float *dmin, float *dmax, double *sumDbl, double *sumSqDbl,
                         float *avg, float *SD);
  void scaleArrayForMode(float *array, int nxDim, int mode, int nx1, int nx2, int ny1, 
                         int ny2, float *dmin, float *dmax, float *dmean);
  void lsFit(float *x, float *y, int num, float *slope, float *intcp,
             float *ro);
  void lsFitPred(float *x, float *y, int n, float *slope, float *bint,
                 float *ro, float *sa, float *sb, float *se,
                 float xpred, float *ypred, float *prederr);
  void lsFit2(float *x1, float *x2, float *y, int n, float *a, float *b,
              float *c);
  void lsFit2Pred(float *x1, float *x2, float *y, int n, float *a, float *b, 
                  float *c, float x1pred, float x2pred, float *ypred,
                  float *prederr);
  void lsFit3(float *x1, float *x2, float *x3, float *y, int n, float *a1, 
              float *a2, float *a3, float *c);
  void eigenSort(double *val, double *vec, int n, int rowStride, int colStride,
                 int useAbs);

  /* robuststat.c */
  void rsSortInts(int *x, int n);
  void rsSortFloats(float *x, int n);
  void rsSortIndexedFloats(float *x, int *index, int n);
  void rsSetSortIndexOffset(int offset);
  void rsMedianOfSorted(float *x, int n, float *median);
  void rsMedian(float *x, int n, float *tmp, float *median);
  void rsMADN(float *x, int n, float median, float *tmp, float *MADN);
  void rsFastMedian(float *x, int n, float *tmp, float *median);
  void rsFastMedianInPlace(float *x, int n, float *median);
  void rsPercentileOfSorted(float *x, int n, float fraction, float *pctile);
  void rsFastMADN(float *x, int n, float median, float *tmp, float *MADN);
  void rsMadMedianOutliers(float *x, int n, float kcrit, float *out);
  void rsTrimmedMean(float *x, int n, float gamma, float *xsort, 
                     float *trmean);
  void rsTrimmedMeanOfSorted(float *x, int n, float gamma, float *trmean);

  /* amat_to_rotamgstr.c */
  void amatToRotmagstr(float a11, float a12, float a21, float a22, 
                         float *theta, float *smag, float *str, float *phi);
  void rotmagstrToAmat(float theta, float smag, float str, float phi,
                       float *a11, float *a12, float *a21, float *a22);
  void amatToRotMag(float a11, float a12, float a21, float a22, float *theta, 
                    float *ydtheta, float *smag, float *ydmag);
  void rotMagToAmat(float theta, float ydtheta, float smag, float ydmag, float *a11, 
                    float *a12, float *a21, float *a22);

  /* percentile.c */
  float percentileFloat(int s, float *r, int num);
  int percentileInt(int s, int *r, int num);

  /* convexbound.c */
  void convexBound(float *sx, float *syin, int npnts, float fracomit,
                   float pad, float *bx, float *by, int *nvert, float *xcen,
                   float *ycen, int maxverts);

  /* beadfind.c */
  void makeModelBead(int boxSize, float beadSize, float *array);
  double beadIntegral(float *array, int nxdim, int nx, int ny, float rCenter,
                      float rInner, float rOuter, float xcen, float ycen,
                      float *cenmean, float *annmean, float *temp, 
                      float annPct, float *median);

  /* statfuncs.f */
  double tValue(double signif, int ndf);
  double fValue(double signif, int ndf1, int ndf2);
  double errFunc(double x);
  double incompBeta(double a, double b, double x);
  double betaFunc(double p, double q);
  double gammaFunc(double x);
  double lnGamma(double x);
  float gaussianDeviate(int seed);

  /* surfacesort.c */
  int surfaceSort(float *xyz, int numPts, int markersInGroup, int *group);
  int setSurfSortParam(int which, float value);

  /* gaussj.c */
  int gaussj(float *a, int n, int np, float *b, int m, int mp);
  int gaussjDet(float *a, int n, int np, float *b, int m, int mp, float *determ);

  /* metro.c */
  typedef void (*MetroFunct)(int *, float *, float *, float *);
  void metroSearch(int n, float *X, MetroFunct funct, float *f, float *G, 
                   float stepInitial, float epsilon, int limitIn, int *ier, float *H,
                   int *numIter, float rmsScale);

  /* find_piece_shifts.c */
  int findPieceShifts
  (int *ivarpc, int nvar, int *indvar, int *ixpclist, int *iypclist, 
   float *dxedge, float *dyedge, int idir, int *pieceLower, int *pieceUpper, 
   int *ifskipEdge, int edgeStep, float *dxyvar, int varStep, int *edgelower,
   int *edgeupper, int pcStep, float *work, int fort, int leaveInd, int skipCrit,
   float robustCrit, float critMaxMove, float critMoveDiff, int maxIter,
   int numAvgForTest, int intervalForTest, int *numIter, float *wErrMean, 
   float *wErrMax);
  int findpieceshifts
  (int *ivarpc, int *nvar, int *indvar, int *ixpclist, int *iypclist, 
   float *dxedge, float *dyedge, int *idir, int *pieceLower, int *pieceUpper, 
   int *ifskipEdge, int *edgeStep, float *dxyvar, int *varStep, int *edgeLower,
   int *edgeUpper, int *pcStep, float *work, int *fort, int *leaveInd, 
   int *skipCrit, float *robustCrit, float *critMaxMove, float *critMoveDiff,
   int *maxIter, int *numAvgForTest, int *intervalForTest, int *numIter,
   float *wErrMean, float *wErrMax);
  int pickAlternativeShifts(int *ivarpc, int nvar, int *indvar, float *dxedge,
                            float *dyedge, int *pieceLower, int *pieceUpper,
                            int *ifskipEdge, int edgeStep, int *edgeLower, 
                            int *edgeUpper, int pcStep, int fort, float *altDxys,
                            int numAlts, int altIxy, float errThresh, float reduceFac,
                            float newThresh, int *fixedEdges, int *numFixed);
  
  /* zoomdown.c */
  int selectZoomFilter(int type, double zoom, int *outWidth);
  int selectZoomFilterXY(int type, double xzoom, double yzoom, int *outWidthX, 
                         int *outWidthY);
  void setZoomValueScaling(float factor);
  int zoomWithFilter(unsigned char **slines, int sXsize, int sYsize, float sXoff,
                     float sYoff, int dXsize, int dYsize, int dXdim, int dXoff, int dtype,
                     void *outData, b3dUInt32 *cindex, unsigned char *bindex);
  int zoomFiltInterp(float *array, float *bray, int nxa, int nya, int nxb, int nyb,
                     float xc, float yc, float xt, float yt, float dmean);
  double zoomFiltValue(float radius);
  double zoomRawFiltValue(float radius);

  /* linearxforms.f */
  void xfUnit(float *f, float val, int rows);
  void xfCopy(float *f1, int rows1, float *f2, int rows2);
  void xfMult(float *f1, float *f2, float *prod, int rows);
  void xfInvert(float *f, float *finv, int rows);
  void xfApply(float *f, float xcen, float ycen, float x, float y, float *xp, float *yp,
               int rows);
  void anglesToMatrix(float *angles, float *matrix, int rows);
  int matrixToAngles(float *matrix, double *x, double *y, double *z, int rows);
  void icalc_angles(float *angles, float *matrix);
  void invertMatrix(float *m1, float *m2);
  int readOneXform(FILE *fp, float *xf);
  int readAllXforms(FILE *fp, float *xforms, int maxRead, int *numRead);
  void exitFromXFReadError(int ierr, const char *descrip);

  /* piecefuncs.c */
  int checkPieceList(int *pclist, int stride, int npclist, int redfac, int nframe,
                     int *minpiece, int *npieces, int *noverlap);
  void adjustPieceOverlap(int *pclist, int stride, int npclist, int nframe, int minpiece,
                          int noverlap, int newOverlap);
  void fillListOfPieceZ(int *izPcList, int nPcList, int *listZ, int *numListZ);
  int readPieceList(const char *pieceFile, int *ixPcList, int *iyPcList, int *izPcList, 
                    int *nPcList, int maxPieces);

  /* regression.c */
  void statMatrices(float *x, int xsize, int colFast, int m, int msize, int ndata,
                    float *sx, float *ss, float *ssd, float *d, float *r, float *xm,
                    float *sd, int ifdisp);
  int multRegress(float *x, int xsize, int colFast, int m, int ndata, int nbcol,
                  int wgtcol, float *b, int bsize, float *c, float *xm, float *sd,
                  float *work);
  int robustRegress(float *x, int xsize, int colFast, int m, int ndata, int nbcol,
                    float *b, int bsize, float *c, float *xm, float *sd, float *work,
                    float kfactor, int *numIter, int maxIter, int maxZeroWgt,
                    float maxChange, float maxOscill);
  int weightedPolyFit(float *x, float *y, float *weight, int ndata, int order,
                      float *slopes, float *intcpt, float *work);
  int polynomialFit(float *x, float *y, int ndata, int order, float *slopes, 
                    float *intcpt, float *work);    
  int robustPolyFit(float *x, float *y, int ndata, int order, float *slopes,
                    float *intcpt, float *work, float kfactor, int *numIter, int maxIter,
                    int maxZeroWgt);
  int robustPolySmooth(float *x, float *yIn, int ndata, int order, float *yOut, 
                       int numFit, int minFit, float *work, float kfactor, int maxIter, 
                       int maxZeroWgt, float *weights);

  /* findtransform.c */
  int findTransform(float *xMat, int mSize, int icolX, int numPoints, float xcen,
                    float yCen, int ifTrans, int ifRotrans, int ifDev, float *xf,
                    float *devAvg, float *devSd, float *devMax, int *ipntMax);


  /* minimize1D.c */
  int minimize1D(float curPosition, float curValue, float initialStep, int numScanSteps,
                 int *numCutsDone, float *brackets, float *nextPosition);

  /* multibinstat */
  int multiBinSetup(int binning[][3], int boxSize[][3], int boxSpacing[][3],
                    int numScales, int startCoord[3], int endCoord[3], int boxStart[][3], 
                    int numBoxes[][3], int *bufferStartInds, int *statStartInds);
  int multiBinStats(int binning[][3], int boxSize[][3], int boxSpacing[][3], 
                    int numScales, int startCoord[3], int endCoord[3], int boxStart[][3], 
                    int numBoxes[][3], int *bufferStartInds, int *statStartInds, 
                    float *buffer, float *means, float *SDs, int *funcData,
                    int (*getSliceFunc)(int *, int *, float *));
  void makeStandardDevMap(float *array, int nxDim, int ixStart, int ixEnd, int iyStart, 
                          int iyEnd, int binning, int boxSize, float *sdArr, 
                          float *sumArr, float *sqrArr, int *xOffset, int *yOffset);

  /* montagexcorr */
#define MONTXC_MAX_PEAKS  100
#define MONTXC_MAX_DEBUG_LINE 90
  void montXCBasicSizes(int ixy, int nbin,  int indentXC, int *nxyPiece, int *nxyOverlap, 
                        float aspectMax, float extraWidth, float padFrac, int niceLimit,
                        int *indentUse, int *nxyBox, int *numExtra, int *nxPad, 
                        int *nyPad, int *maxLongShift);
  void montXCIndsAndCTF(int ixy, int *nxyPiece, int *nxyOverlap, int *nxyBox, int nbin, 
                        int indentUse, int *numExtra, int nxPad, int nyPad, int numSmooth,
                        float sigma1, float sigma2, float radius1, float radius2, 
                        int evalCCC, int *ind0Lower, int *ind1Lower, int *ind0Upper,
                        int *ind1Upper, int *nxSmooth, int *nySmooth, float *ctf,
                        float *delta);
  int montXCFindBinning(int maxBin, int targetSize, int indentXC, int *nxyPiece,
                        int *nxyOverlap, float aspectMax, float extraWidth, float padFrac,
                        int niceLimit, int *numPaddedPix, int *numBoxedPix);
  int montXCFindBinning2(int maxBin, int targetSize, int indentXC, int ixy, int *nxyPiece,
                         int *nxyOverlap, int *expectedShift, float aspectMax,
                         float extraWidth, float padFrac, int niceLimit,
                         int *numPaddedPix, int *numBoxedPix);
 void montXCorrEdge(float *lowerIn, float *upperIn, int *nxyBox, int *nxyPiece, 
                     int *nxyOverlap, int nxSmooth, int nySmooth, int nxPad, int nyPad,
                     float *lowerPad, float *upperPad, float *lowerCopy,
                     int numXcorrPeaks, int legacy, float *ctf, float delta,
                     int *numExtra, int nbin, int ixy,
                     int maxLongShift, int weightCCC, float *xDisplace, float *yDisplace,
                     float *CCC, void (*twoDfft)(float *, int *, int *, int *),
                     void (*dumpEdge)(float *, int *, int *, int *, int *, int *), 
                     char *debugStr, int debugLen, int debugLevel);
  void montXCFindBestCorr(float *array, float *brray, int nxDim, int nx, int ny, 
                          int nxTrim,
                          int nyTrim, int ixStart, int ixEnd, int iyStart, int iyEnd,
                          float *delX, float *delY, float *corr, float maxDist,
                          float *aWeights, float *bWeights, int nxWgt, int binning,
                          int wgtXoffset, int wgtYoffset, float threshCCC,
                          double *bestSumArr);
  void montXCSetDistWeightHalfFall(float *inVal);
  double montXCGetLastTrimmedMaxSD();
  void montXCGetLastRunnersUp(float *disps, int maxPairs);


  /* sdsearch */
  void montBigSearch(float *array, float *brray, int nx, int ny, int ixBox0,
                     int iyBox0,int ixBox1, int iyBox1, float *dxMin, float *dyMin,
                     float *sdMin, float *ddenMin, int numIter, int limStep);
  void montSdCalc(float *array, float *brray, int nx, int ny, int ixBox0, int iyBox0,
                  int ixBox1, int iyBox1, float dx, float dy, float *sd, float *dden);

  /* gcvspl */
  int gcvspl(double *x, double *y, int yDim, double *wgtx, double *wgty, int mOrder,
             int numVal, int numYcol, int mode, double val, double *coeff, int coeffDim, 
             double *work, int *ier);
  double splder(int derivOrder, int mOrder, int numVal, double tVal, double *x,
                double *coeff, int *nearInd, double *work);

  /* spectrumscaled */
  int spectrumScaled(void *image, int type, int nx, int ny, void *spectrum, int padSize, 
                     int finalSize, int bkgdGray, float truncDiam, int filtType,
                     void (*twoDfft)(float *, int *, int *, int *));
  void makeAmplitudeSpectrum(float *fftArray, float *spectrum, int padSize, int outXdim);

  /* extraheader.c */
  int getExtraHeaderTilts(char *array, int numExtraBytes, int nbytes, int iflags, int nz,
                           float * tilt, int *numTilts, int maxTilts, int *izPiece);
  int getExtraHeaderItems(char *array, int numExtraBytes, int nbytes, int iflags, int nz,
                           int itype, float *val1, float *val2, int *numVals, int maxVals,
                           int *izPiece);
  int getMetadataItems(int indAdoc, int iTypeAdoc, int nz, int iTypeData, float *val1,
                        float *val2, int *numVals, int *numFound, int maxVals,
                        int *izPiece);
  int getMetadataByKey(int indAdoc, int TypeAdoc, int nz, const char *key, int ivalType,
                        float *val1, float *val2, float *val3, char **valString,
                        int *numVals, int *numFound, int maxVals, int *izPiece);
  int getExtraHeaderPieces(char *array, int numExtraBytes, int nbytes, int iflags,
                            int nz, int *ixPiece, int *iyPiece, int *izPiece,
                            int *numPieces, int maxPiece);
  int getMetadataPieces(int index, int itype, int nz, int *ixPiece, int *iyPiece,
                         int *izPiece, int maxPiece, int *numFound);
  int getMetadataWeightingDoses(int indAdoc, int iTypeAdoc, int nz, int *izPiece, 
                                int bidirNumInvert, float *priorDose, float *secDose);
  void priorDosesFromImageDoses(float *secDose, int nz, int bidirNumInvert,
                                float *priorDose);
  double SEMshortsToFloat(short low, short ihigh);
  int getExtraHeaderValue(void *extHead, int offset, int type, unsigned char *bval,
                          short *sval, int *ival, float *fval, double *dval);
  int getExtraHeaderSecOffset(void *extHead, int extSize, int numInt, int numReal,
                           int izSect, int *offset, int *size);
  int getExtraHeaderMaxSecSize(void *extHead, int extSize, int numInt, int numReal,
                               int izSect, int *maxSize);
  int copyExtraHeaderSection(void *extraIn, int sizeIn, void *extraOut, int sizeOut,
                             int numInt, int numReal, int izSect, int *cumulBytesOut);
  double getFeiExtHeadAngleScale(void *extHead);

  /* rotateflip.c */
  int rotateFlipImage(void *array, int mode, int nx, int ny, int operation, 
                      int leftHanded, int invertAfter, int invertCon, void *brray,
                      int *nxout, int *nyout, int numThreads);
  
  /* autocorrpeaks */
  int findAutoCorrPeaks(float *array, int nxPad, int nyPad, float *Xpeaks, float *Ypeaks,
                        float *peak, int numPeaks, int maxScan, float catalFac, 
                        int noWaffle, float markedX, float markedY, float *dist1Ptr,
                        float *dist2Ptr, float *angle, float vectors[4], int num[2],
                        int *nearInd, char *messBuf, int bufSize);

  /* winversion.c */
  int isWindows2000();
  int isWindowsXP();
  int isWindowsVista();
  int isWindows7();
  int isWindows8();
  int isWindows10();
  int isWindowsVersion(int major, int opMajor, int minor, int opMinor);

  /* readlinevalues.c */
#define RLFV_SEPARATE_LINES  1
  int readLinesForValues(FILE *fp, int *numToGetP, int valSize, char *line, int maxLine,
                         int flags, const char *types, ...);
  void exitFromValueReadError(int err, const char *descrip);

  /* gettiltangles.c */
  void getTiltAngles(int *numViews, float *tilt, int limTilt);
  void readTiltFile(int *numViews, char *filename, float *tilt, int limTilt);

  /* projectpixel.c */
  void maxCornerDistFromCen(float angle, float *dist);
  void rayLengthAtDistFromCen(float cosAng, float sinAng, float dist, float *length);
  void rayPixelIntersectingArea(float angle, float cenToCenDist, int rayWidth, 
                                float delDist, float *area);
  void makeRayAreaLookupTable(float angle, int rayWidth, int numDists, float delForArea,
                              int *delRayInd, int *numRaysHit, float *areas);
  
#ifdef __cplusplus
}
#endif


#endif
