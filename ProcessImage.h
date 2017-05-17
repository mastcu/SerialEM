#if !defined(AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_)
#define AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessImage.h : header file
//

#define MAX_FFT_CIRCLES 8

/////////////////////////////////////////////////////////////////////////////
// CProcessImage command target

class DLL_IM_EX CProcessImage : public CCmdTarget
{
  // DECLARE_DYNCREATE(CProcessImage)
public:
  CProcessImage();           // protected constructor used by dynamic creation
  ~CProcessImage();

// Attributes
public:

// Operations
public:
  double WholeImageMean(EMimageBuffer *imBuf);
  float UnbinnedSpotMeanPerSec(EMimageBuffer *imBuf);
	void UpdateBWCriterion(EMimageBuffer *imBuf, float &crit, float derate);
	BOOL ImageTooWeak(EMimageBuffer *imBuf, float crit);
  void FixXRaysInBuffer(BOOL doImage);
  void SetXRayCritIterations(int inVal) {mXRayCritIterations = inVal;};
  void SetXRayCritIncrease(float inVal) {mXRayCritIncrease = inVal;};
  void SetInnerXRayDistance(float inVal) {mInnerXRayDistance = inVal;};
  void SetOuterXRayDistance(float inVal) {mOuterXRayDistance = inVal;};
  void RemoveXRays(CameraParameters *param, void *array, int type, int binning, int top, 
                     int left, int bottom, int right, float absCrit, 
                     float numSDCrit, int useBothCrit, BOOL verbose);
  float EquivalentRecordMean(int bufNum);
  float ForeshortenedMean(int nufNum);
  void GetFFT(int binning, int capFlag);
  void GetFFT(EMimageBuffer *imBuf, int binning, int capFlag);
  void NewProcessedImage(EMimageBuffer *imBuf, short int *brray, int type, int nx, int ny,
    int moreBinning, int capFlag = BUFFER_PROCESSED, bool fftWindow = false);
  void RotateImage(BOOL bLeft);
  BOOL GetLiveFFT() {return mLiveFFT;};
  GetSetMember(BOOL, CircleOnLiveFFT)
  GetSetMember(bool, SideBySideFFT);
  GetSetMember(bool, AutoSingleFFT);
  GetSetMember(int, NumCircles)
  SetMember(float, GridLinesPerMM)
  SetMember(float, ShortCatalaseNM);
  SetMember(float, LongCatalaseNM);
  float *GetFFTCircleRadii() {return &mFFTCircleRadii[0];};
  GetSetMember(CString, OverlayChannels);
  GetSetMember(int, PixelTimeStamp);
  SetMember(int, NumFFTZeros);
  SetMember(float, AmpRatio);
  SetMember(float, SphericalAber);
  GetSetMember(float, PlatePhase);
  GetSetMember(float, FixedRingDefocus);

// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CProcessImage)
  //}}AFX_VIRTUAL

// Implementation
protected:
  //virtual ~CProcessImage();

  // Generated message map functions
  //{{AFX_MSG(CProcessImage)
  afx_msg void OnProcessFft();
  afx_msg void OnUpdateProcess(CCmdUI* pCmdUI);
  afx_msg void OnUpdateProcessNoRGB(CCmdUI* pCmdUI);
  afx_msg void OnProcessBinnedfft();
  afx_msg void OnProcessRotateleft();
  afx_msg void OnProcessRotateright();
  afx_msg void OnUpdateProcessMinmaxmean(CCmdUI* pCmdUI);
  afx_msg void OnProcessZerotiltmean();
  afx_msg void OnUpdateProcessZerotiltmean(CCmdUI* pCmdUI);
  afx_msg void OnProcessSetintensity();
  afx_msg void OnUpdateProcessSetintensity(CCmdUI* pCmdUI);
  afx_msg void OnProcessMovebeam();
  afx_msg void OnUpdateProcessMovebeam(CCmdUI* pCmdUI);
  afx_msg void OnProcessFixdarkxrays();
  afx_msg void OnProcessFiximagexrays();
  afx_msg void OnSetdarkcriteria();
  afx_msg void OnUpdateSetcriteria(CCmdUI* pCmdUI);
  afx_msg void OnSetimagecriteria();
	afx_msg void OnProcessShowcrosscorr();
	afx_msg void OnUpdateProcessShowcrosscorr(CCmdUI* pCmdUI);
	//}}AFX_MSG

  DECLARE_MESSAGE_MAP()

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  float mInnerXRayDistance;   // minimum and maximum distances from X ray patch
  float mOuterXRayDistance;   // for neighboring pixels to average
  int mXRayCritIterations;    // Number of times to try oversized patch with higher crit 
  float mXRayCritIncrease;    // Factor that criterion increases per iteration
  BOOL mLiveFFT;              // Do automatic FFT of continuous image
  double mMoveBeamStamp;      // Time stamp of last image used for moving beam
  BOOL mCircleOnLiveFFT;       // Draw circle on Live FFT
  float mFFTCircleRadii[MAX_FFT_CIRCLES];
  int mNumCircles;
  bool mSideBySideFFT;         // Flag to draw FFT side by side with  main window
  bool mAutoSingleFFT;         // Flag to take FFTs automatically for single-frame if side
  float mGridLinesPerMM;
  CString mOverlayChannels;   // Channel buffers for making color overlays
  ShortVec mPixSizCamera;
  ShortVec mPixSizMagInd;
  ShortVec mAddedRotation;
  std::vector<float> mPixelSizes, mGridRotations;
  int mPixelTargetSize;       // Target size for find pixel
  BOOL mCatalaseForPixel;
  float mShortCatalaseNM;
  float mLongCatalaseNM;
  float mCorrMaxScale;
  int mGridMeshSize;          // Mesh to calibrate from grid bar spacing
  int mPixelTimeStamp;        // Time stamp of LAST pixel size measurement
  int mNumFFTZeros;
  float mAmpRatio;
  float mSphericalAber;
  double mVoltage;
  double mKVTime;
  float mPlatePhase;
  float mFixedRingDefocus;


public:
  afx_msg void OnProcessMinmaxmean();
  afx_msg void OnProcessLivefft();
  afx_msg void OnUpdateProcessLivefft(CCmdUI *pCmdUI);
  int FindBeamCenter(EMimageBuffer * imBuf, float & xcen, float & ycen, float &radius,
    float &xcenUse, float &ycenUse, 
    float &radUse, float &fracUse, int &binning, 
    int &numQuadrant, float &shiftX, float &shiftY, float &fitErr);
  afx_msg void OnTasksCenterbeam();
  afx_msg void OnUpdateTasksCenterbeam(CCmdUI *pCmdUI);
  void MoveBeam(EMimageBuffer * imBuf, float shiftX, float shiftY);
  void EnableMoveBeam(CCmdUI * pCmdUI, bool skipUserPt);
  int CenterBeamFromActiveImage(double maxRadius, double maxError, 
    BOOL useCentroid = false);
  afx_msg void OnProcessCircleonlivefft();
  afx_msg void OnUpdateProcessCircleonlivefft(CCmdUI *pCmdUI);
  afx_msg void OnProcessFindpixelsize();
  afx_msg void OnUpdateProcessFindpixelsize(CCmdUI *pCmdUI);
  int CorrelationToBufferA(float * array, int nxpad, int nypad, int binning,
                                        float &corMin, float &corMax);
  void FindPeakSeries(float * Xpeaks, float * Ypeaks, float * peak, int numPeaks, 
    float & delX, float & delY, int & indFar, int & numSeries, int &indNear, float &variation);
  void FindPixelSize(float markedX, float markedY, float minScale, float maxScale);
  afx_msg void OnProcessPixelsizefrommarker();
  afx_msg void OnUpdateProcessPixelsizefrommarker(CCmdUI *pCmdUI);
  int ClosestRightSidePeak(float * Xpeaks, float * Ypeaks, float * peak, int numPeaks, float delX, float delY);
  bool OverlayImages(EMimageBuffer * redBuf, EMimageBuffer * grnBuf, EMimageBuffer * bluBuf);
  afx_msg void OnProcessMakecoloroverlay();
  afx_msg void OnUpdateProcessMakecoloroverlay(CCmdUI *pCmdUI);
  int CropImage(EMimageBuffer *imBuf, int top, int left, int bottom, int right);
  afx_msg void OnProcessCropimage();
  afx_msg void OnUpdateProcessCropimage(CCmdUI *pCmdUI);
  afx_msg void OnListRelativeRotations();
  int MoveBeamByCameraFraction(float shiftX, float shiftY);
  afx_msg void OnProcessSetBinnedSize();
  afx_msg void OnProcessAutocorrelation();
  afx_msg void OnUpdateProcessAutocorrelation(CCmdUI *pCmdUI);
  float GetFoundPixelSize(int camera, int magInd);
  afx_msg void OnPixelsizeAddToRotation();
  int LookupFoundPixelSize(int camera, int magInd);
  afx_msg void OnSetDoseRate();
  afx_msg void OnUpdateSetDoseRate(CCmdUI *pCmdUI);
  void DoUpdateSetintensity(CCmdUI* pCmdUI, bool doseRate);
  void DoSetIntensity(bool doseRate);
  afx_msg void OnPixelsizeCatalaseCrystal();
  afx_msg void OnUpdatePixelsizeCatalaseCrystal(CCmdUI *pCmdUI);
  afx_msg void OnProcessCropAverage();
  afx_msg void OnUpdateProcessCropAverage(CCmdUI *pCmdUI);
  afx_msg void OnMeshForGridBars();
  float PerpendicularLineMedian(float *acorr, int nxPad, int nyPad, float xPeak, 
    float yPeak);
  void GetPixelArrays(ShortVec **camera, ShortVec **magInd, ShortVec **addedRot,
    std::vector<float> **sizes, std::vector<float> **rotations) {*camera = &mPixSizCamera;
    *magInd = &mPixSizMagInd; *addedRot = &mAddedRotation; *sizes = &mPixelSizes; 
    *rotations = &mGridRotations;};
    afx_msg void OnProcessSideBySide();
    afx_msg void OnUpdateProcessSideBySide(CCmdUI *pCmdUI);
    bool GetFFTZeroRadiiAndDefocus(EMimageBuffer * imBuf, FloatVec *radii, double &defocus);
void ModifyFFTPointToFirstZero(EMimageBuffer * imBuf, float & shiftX, float & shiftY);
bool DefocusFromPointAndZeros(double pointRad, int zeroNum, float pixel, FloatVec * radii,
  double & defocus);
afx_msg void OnProcessAutomaticFFTs();
afx_msg void OnUpdateProcessAutomaticFFTs(CCmdUI *pCmdUI);
afx_msg void OnProcessSetPhasePlateShift();
afx_msg void OnProcessSetDefocusForCircles();
float LinearizedDoseRate(int camera, float rawRate);
int FitRatesAndCounts(float *counts, float *rates, int numVals, 
  float &scale, float &intercp, float &base);
int FindDoseRate(float countVal, float *counts, float *rates, int numVals,
                  float scale, float intercp, float base,
                  float &ratioBest, float &doseRate);
int DoseRateFromMean(EMimageBuffer * imBuf, float mean, float & doseRate);
double GetRecentVoltage(bool *valueWasRead = NULL);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_)
