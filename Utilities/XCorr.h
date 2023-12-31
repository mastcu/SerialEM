#pragma once
#ifndef DLL_IM_EX
#define DLL_IM_EX  _declspec(dllexport)
#endif

#include "..\Shared\cfsemshare.h"
#define StatLSFit2Pred lsFit2Pred
#define StatLSFit2 lsFit2

void twoDfft(float *array, int *nxpad, int *nypad, int *dir);
void DLL_IM_EX XCorrCrossCorr(float *array, float *brray, int nxpad, int nypad, 
		float deltap, float *ctfp, float *crray = NULL);
void DLL_IM_EX XCorrRealCorr(float *array, float *brray, int nxpad, int nypad, int maxdist,
	float deltap, float *ctfp, float *peak);
void DLL_IM_EX XCorrTripleCorr(float *array, float *brray, float *crray, int nxpad, int nypad, 
	float deltap, float *ctfp);
bool DLL_IM_EX XCorrPeriodicCorr(float *array, float *brray, float *crray, int nxPad, int nyPad,
  float deltap, float *ctfp, float tiltAngles[2], float axisAngle, int eraseOnly, float boostMinNums);
void DLL_IM_EX XCorrFilter(float *array, int type, int nxin, int nyin, float *brray, int nxpad, int nypad,
                 float delta, float *ctf);
void DLL_IM_EX ProcScaleImage(void *array, int type, int nx, int ny, float factor, float offset,
  void *brray);
int DLL_IM_EX XCorrNiceFrame(int num, int idnum, int limit);
void DLL_IM_EX XCorrBinBy2(void *array, int type, int nxin, int nyin, short int *brray);
void DLL_IM_EX XCorrBinByN(void *array, int type, int nxin, int nyin, int nbin, short int *brray);
void DLL_IM_EX XCorrBinInverse(void *array, int type, int nxin, int nyin, int ixofs, int iyofs,
                     int nbin, int nxout, int nyout, void *brray);
float DLL_IM_EX XCorrOneInverseBin(void *array, int type, int nxin, int nyin, int ixofs, int iyofs, 
                     int nbin, int ixout, int iyout);
void DLL_IM_EX XCorrFillTaperGap(short int *data, int type, int nx, int ny, float dmean, int ix0, int ix1,
					   int idirx, int iy0, int iy1, int idiry, int ntaper);
void DLL_IM_EX XCorrStretch(void *array, int type, int nx, int ny, float stretch, float axis,
				  void *brray, float *a11, float *a12, float *a21, float *a22);
void DLL_IM_EX XCorrFastInterp(void *array, int type, void *bray, int nxa, int nya,
		int nxb, int nyb, float amat11, float amat12, float amat21,
		float amat22, float xc, float yc, float xt, float yt);

void DLL_IM_EX ProcSampleMeanSD(void *array, int type, int nx, int ny, float *mean, float *sd);
double DLL_IM_EX ProcImageMean(void *array, int type, int nx, int ny, int ix0, int ix1,
					 int iy0, int iy1);
double DLL_IM_EX ProcImageMeanCircle(void *array, int type, int nx, int ny, int cx, int cy,
  int radius);
void DLL_IM_EX ProcMinMaxMeanSD(void *array, int type, int nx, int ny, int ix0, int ix1,
					 int iy0, int iy1, float *mean, float *min, float *max, float *sd);
void DLL_IM_EX ProcCentroid(void *array, int type, int nx, int ny, int ix0, int ix1,
					 int iy0, int iy1, double baseval, float &xcen, float &ycen, double thresh = 1.e30);
void DLL_IM_EX ProcMomentsAboveThreshold(void *array, int type, int nx, int ny, int ix0, int ix1,
  int iy0, int iy1, double thresh, float xcen, float ycen, double &M11,
  double &M20, double &M02);
void DLL_IM_EX ProcRotateLeft(void *array, int type, int nx, int ny, short int *brray);
void DLL_IM_EX ProcRotateRight(void *array, int type, int nx, int ny, short int *brray);
void DLL_IM_EX ProcShiftInPlace(short int *array, int type, int nx, int ny, int shiftX, int shiftY,
                      float fill);
void DLL_IM_EX ProcRotateFlip(short int *array, int type, int nx, int ny, int operation, int invert,
                    short int *brray, int *nxout, int *nyout);
void DLL_IM_EX ProcSimpleFlipY(void *array, int rowBytes, int height);
void DLL_IM_EX ProcFFT(void *array, int type, int nx, int ny, int binning, float *fftarray, 
			 short int *brray, int nPadSize, int nFinalSize);
double DLL_IM_EX ProcFFTMagnitude(float *array, int nx, int ny, int ix, int iy);
void DLL_IM_EX ProcRotAvFFT(void *array, int type, int nxdim, int ix0, int ix1, int iy0, int iy1,
                  float *fftarray, int nxpad, int nypad, float *rotav, int *ninRing, 
                  int numRotPts);
int DLL_IM_EX ProcTrimCircle(void *array, int type, int nx, int ny, float cenFrac, int testSize,
          int delTrim, int minTrimX, int minTrimY, float passRatio, bool centered,
          int &trimX, int &trimY, int &trimXright, int &trimYtop);
int DLL_IM_EX ProcEvaluateTrim(void *array, int type, int nx, int ny, int testSize, int trimXleft,
            int trimYbot, int trimXright, int trimYtop, double critMean);
int DLL_IM_EX ProcFindCircleEdges(void *array, int type, int nx, int ny, float xcen, float ycen,
                        int border, float angleInc, int boxLen, int boxWidth, 
                        float outCrit, float edgeCrit, float *xx, float *yy, 
                        float *angle, float *grad);
int DLL_IM_EX ProcGainNormalize(void *image, int type, int nxFull, int top, int left, int bottom,
                       int right, short int *dark1, double exp1, short int *dark2, 
                       double exp2, double exp, int darkScale, int darkByteSize, void *gainRef, int gainBytes, 
                       int gainBits);
int DLL_IM_EX ProcDarkSubtract(void *image, int type, int nx, int ny, short int *ref1, 
                           double exp1, short int *ref2, double exp2, double exp, int darkScale);
int DLL_IM_EX ProcConvertGainRef(float *fGain, unsigned short *usGain, int arrSize, int scaledMax,
					   int minBits, int *bits);
float DLL_IM_EX ProcGetPixel(void *array, int type, int nx, int ix, int iy);
int DLL_IM_EX ProcReplacePixels(void *array, int type, int nx, int ny, int nHotCol, int *hotCol,
					int nHotPixel, int *hotX, int *hotY, int maxPixel, float minDist, float maxDist,
          double crit, int ix, int iy, int *listX, int *listY);
int DLL_IM_EX ProcRemoveXRays(void *array, int type, int nx, int ny, int nHotCol, int *hotCol,
					int nHotPixel, int *hotX, int *hotY, int maxPixel, float minDist, float maxDist, 
          float absCrit, float sdCrit, int eitherBoth, int critIterations, float critFac,
          float *sdev, int *nReplaced, 
          int *nSkipped, int *nTruncated, int *replacedX, int *replacedY, int replacedSize);
void DLL_IM_EX ProcBalanceHalves(void *array, int type, int nx, int ny, int top, int left, 
                       int boundary, int ifY);
void DLL_IM_EX ProcPasteByteImages(unsigned char *first, int nx1, int ny1, unsigned char *second,
  int nx2, int ny2, unsigned char *outImage, bool vertical);

int DLL_IM_EX StatGaussj(float *a, int n, int np, float *b, int m, int mp);
int DLL_IM_EX StatMultr(float *x, int *xsiz, int *mpin, int *ng, float *xm, float *sd, 
	   float *b, float *b1, float *c, float *rsq, float *f, float *invmat);
void DLL_IM_EX StatCorrel(float *x, int xsiz, int m, int msiz, int *ng, float *sx, float *ss,
	     float *ssd, float *d, float *r, float *xm, float *sd, int *ifdisp);
void DLL_IM_EX StatLSFit(float *x, float *y, int num, float &slope, float &intcp);
void DLL_IM_EX StatLSFitPred(float *x, float *y, int n, float *slope, float *bint, float *ro,
               float xpred, float *ypred, float *prederr);
void DLL_IM_EX XCorrTaperInPad(void *array, int type, int nxdimin, int ix0, int ix1,
                       int iy0, int iy1, float *brray, int nxdim, int nx,
                       int ny, int nxtap, int nytap);
