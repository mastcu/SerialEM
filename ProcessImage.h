#if !defined(AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_)
#define AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessImage.h : header file
//

#define MAX_FFT_CIRCLES 8

// Flags for FindPixelSize, are passed together with flags to findAutocorrPeaks
// So they need to be distinct
#define FIND_PIX_NO_TRIM     0x100
#define FIND_PIX_NO_DISPLAY  0x200
#define FIND_PIX_NO_TARGET   0x400

struct CtffindParams;
class CCtffindParamDlg;

enum { PROC_ADD_IMAGES, PROC_SUBTRACT_IMAGES, PROC_MULTIPLY_IMAGES, PROC_DIVIDE_IMAGES,
PROC_COMPUTE_THICKNESS};

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
  GetSetMember(int, XRayCritIterations)
  GetSetMember(float, XRayCritIncrease)
  GetSetMember(float, InnerXRayDistance)
  GetSetMember(float, OuterXRayDistance)
  void RemoveXRays(CameraParameters *param, void *array, int type, int binning, int top,
                     int left, int bottom, int right, float absCrit,
                     float numSDCrit, int useBothCrit, BOOL verbose);
  float EquivalentRecordMean(int bufNum);
  float ForeshortenedMean(int bufNum);
  int ForeshortenedSubareaMean(int bufNum, float fracOrSizeX, float fracOrSizeY,
    bool useShift, float &mean, CString *message);
  void GetFFT(int binning, int capFlag);
  void GetFFT(EMimageBuffer *imBuf, int binning, int capFlag);
  void NewProcessedImage(EMimageBuffer *imBuf, short int *brray, int type, int nx, int ny,
    double moreBinning, int capFlag = BUFFER_PROCESSED, bool fftWindow = false, int toBufNum = 0, bool display = true);
  void RotateImage(BOOL bLeft);
  int FilterImage(EMimageBuffer *imBuf, int outImBuf, float sigma1, float sigma2, float radius1, float radius2, bool display = true);
  int FilterImage(KImage *image, float **outArray, float sigma1, float sigma2, float radius1, float radius2);
  int CombineImages(int bufNum1, int bufNum2, int outBufNum, int operation);
  int TransformToOtherMag(EMimageBuffer *imBuf, int outBufNum, int magInd, CString &errStr, int camera = -1,
    int binning = -1, float xcen = -1., float ycen = -1., int xsize = -1, int ysize = -1, 
    float otherScale = 1., float otherRotation = 0., float *xform = NULL);
  int TransformToOtherMag(EMimageBuffer *imBuf, EMimageBuffer *otherBuf, int outBufNum, CString &errStr, 
    float xcen = -1., float ycen = -1., int xsize = -1, int ysize = -1, float *xform = NULL);
  int ScaleImage(EMimageBuffer *imBuf, int outBufNum, float factor, float offset, bool retainType);
  int PasteImages(EMimageBuffer *imBuf1, EMimageBuffer *imBuf2, int outBufNum, bool vertical);
  BOOL GetLiveFFT() {return mLiveFFT;};
  GetSetMember(BOOL, CircleOnLiveFFT)
  GetSetMember(bool, SideBySideFFT);
  GetSetMember(bool, AutoSingleFFT);
  GetSetMember(int, NumCircles)
  GetSetMember(float, GridLinesPerMM)
  SetMember(float, ShortCatalaseNM);
  SetMember(float, LongCatalaseNM);
  GetSetMember(BOOL, CatalaseForPixel);
  float *GetFFTCircleRadii() {return &mFFTCircleRadii[0];};
  GetSetMember(CString, OverlayChannels);
  GetSetMember(int, PixelTimeStamp);
  GetSetMember(int, NumFFTZeros);
  GetSetMember(float, AmpRatio);
  GetSetMember(float, SphericalAber);
  GetSetMember(float, PlatePhase);
  GetSetMember(float, FixedRingDefocus);
  GetSetMember(float, ReductionFactor);
  GetSetMember(BOOL, CtffindOnClick);
  GetSetMember(float, CtfFitFocusRangeFac);
  GetSetMember(int, SlowerCtfFit);
  GetSetMember(int, ExtraCtfStats);
  GetSetMember(int, DrawExtraCtfRings);
  GetSetMember(float, UserMaxCtfFitRes);
  GetSetMember(float, DefaultMaxCtfFitRes);
  GetSetMember(float, TestCtfPixelSize);
  GetSetMember(float, MinCtfFitResIfPhase);
  GetSetMember(int, CtfMinPhase);
  GetSetMember(int, CtfMaxPhase);
  GetSetMember(int, CtfExpectedPhase);
  GetSetMember(BOOL, CtfFindPhaseOnClick);
  GetSetMember(BOOL, CtfFixAstigForPhase);
  GetSetMember(float, FindBeamOutsideFrac);
  GetMember(float, BeamShiftFromImage);
  GetSetMember(float, ThicknessCoefficient);
  SetMember(float, NextThicknessCoeff);
  GetMember(int, BufIndForCtffind);
  GetSetMember(BOOL, ClickUseCtfplotter);
  GetSetMember(BOOL, TuneUseCtfplotter);
  GetSetMember(int, RunningCtfplotter);
  GetSetMember(float, MinCtfplotterPixel);


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

public:
  CCtffindParamDlg *mCtffindParamDlg;

private:
  CSerialEMApp * mWinApp;
  EMimageBuffer *mImBufs;
  CCameraController *mCamera;
  CEMscope *mScope;
  CShiftManager *mShiftManager;
  EMbufferManager *mBufferManager;
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
  float mFindBeamOutsideFrac; // Fraction that periphery has to be below mean
  float mBeamShiftFromImage;  // Last value of micron shift centering from image
  int mGridMeshSize;          // Mesh to calibrate from grid bar spacing
  int mPixelTimeStamp;        // Time stamp of LAST pixel size measurement
  int mNumFFTZeros;
  float mAmpRatio;
  float mSphericalAber;
  double mVoltage;
  double mKVTime;
  float mPlatePhase;           // Phase plate to assume, stored in radians, entered in deg
  float mFixedRingDefocus;
  float mReductionFactor;      // factor for reducing image
  BOOL mCtffindOnClick;        // Flag to do CTF fitting when click FFT
  float mCtfFitFocusRangeFac;  // Factor to divide/multiply by to setting min/max defocus
  int mSlowerCtfFit;           // Flag for 2D fits
  int mExtraCtfStats;          // Compute extra statistics
  int mDrawExtraCtfRings;      // Draw as many rings as resolution
  float mUserMaxCtfFitRes;     // Users (setting) value of max resolution, in Angstroms
  float mDefaultMaxCtfFitRes;  // Default value if no setting
  float mTestCtfPixelSize;     // A read-in value to replace buffer value for testing
  float mMinCtfFitResIfPhase;  // Minimum resolution when there is phase shift
  WINDOWPLACEMENT mCtffindDlgPlace;
  int mCtfMinPhase;
  int mCtfMaxPhase;
  int mCtfExpectedPhase;        // Value for Ctfplotter
  BOOL mCtfFindPhaseOnClick;
  BOOL mCtfFixAstigForPhase;
  float mThicknessCoefficient;  // Coefficient in thickness calculation 
  float mNextThicknessCoeff;    // Value to use on next call
  int mBufIndForCtffind;        // Index of buffer that can be saved if crash
  CtffindParams *mCurCtffindParams;   // Pointer to params being used in call
  BOOL mClickUseCtfplotter;     // Use Ctfplotter for fit on click
  BOOL mTuneUseCtfplotter;      // Use ctfplotter for tuning operations
  int mRunningCtfplotter;       // 1 if running, -1 is external signal to stop
  ImodImageFile *mShrMemIIFile; // File created with buffer in shared memory
  float mMinCtfplotterPixel;    // Minimum pixel size, reduce to this in shared mem file
 
public:
  afx_msg void OnProcessMinmaxmean();
  afx_msg void OnProcessLivefft();
  afx_msg void OnUpdateProcessLivefft(CCmdUI *pCmdUI);
  int FindBeamCenter(EMimageBuffer * imBuf, float & xcen, float & ycen, float &radius,
    float &xcenUse, float &ycenUse,
    float &radUse, float &fracUse, int &binning,
    int &numQuadrant, float &shiftX, float &shiftY, float &fitErr);
  int FindEllipticalBeamParams(EMimageBuffer * imBuf, float & xcen, float & ycen,
    float &longAxis, float &shortAxis, float &axisAngle);
  afx_msg void OnTasksCenterbeam();
  afx_msg void OnUpdateTasksCenterbeam(CCmdUI *pCmdUI);
  int MoveBeam(EMimageBuffer * imBuf, float shiftX, float shiftY, double maxMicronShift = 0.);
  void EnableMoveBeam(CCmdUI * pCmdUI, bool skipUserPt);
  int CenterBeamFromActiveImage(double maxRadius, double maxError,
    BOOL useCentroid = false, double maxMicronShift = 0.);
  void GetCentroidOfBuffer(EMimageBuffer *imBuf, float &xcen, float &ycen, float &shiftX,
    float &shiftY, bool doMoments, double &M11, double &M20, double &mM02);
  afx_msg void OnProcessCircleonlivefft();
  afx_msg void OnUpdateProcessCircleonlivefft(CCmdUI *pCmdUI);
  afx_msg void OnProcessFindpixelsize();
  afx_msg void OnUpdateProcessFindpixelsize(CCmdUI *pCmdUI);
  int CorrelationToBufferA(float * array, int nxpad, int nypad, int binning,
                                        float &corMin, float &corMax);
  int FindPixelSize(float markedX, float markedY, float minScale, float maxScale, int bufInd, 
    int findFlags, float &spacing, float vectors[4]);
  afx_msg void OnProcessPixelsizefrommarker();
  afx_msg void OnUpdateProcessPixelsizefrommarker(CCmdUI *pCmdUI);
  bool OverlayImages(EMimageBuffer * redBuf, EMimageBuffer * grnBuf, EMimageBuffer * bluBuf);
  afx_msg void OnProcessMakecoloroverlay();
  afx_msg void OnUpdateProcessMakecoloroverlay(CCmdUI *pCmdUI);
  int CropImage(EMimageBuffer *imBuf, int top, int left, int bottom, int right);
  int AlignBetweenMagnifications(int toBufNum, float xcen, float ycen, float maxShiftUm,
    float scaleRange, float angleRange, bool doImShift, float &scaleMax, float &rotation, 
    int corrFlags, CString &errStr);
  afx_msg void OnProcessCropimage();
  afx_msg void OnUpdateProcessCropimage(CCmdUI *pCmdUI);
  afx_msg void OnListRelativeRotations();
  int MoveBeamByCameraFraction(float shiftX, float shiftY, bool uncalOK = false);
  afx_msg void OnProcessSetBinnedSize();
  afx_msg void OnProcessAutocorrelation();
  afx_msg void OnUpdateProcessAutocorrelation(CCmdUI *pCmdUI);
  float GetFoundPixelSize(int camera, int magInd);
  afx_msg void OnPixelsizeAddToRotation();
  int LookupFoundPixelSize(int camera, int magInd);
  afx_msg void OnSetDoseRate();
  afx_msg void OnUpdateSetDoseRate(CCmdUI *pCmdUI);
  void DoUpdateSetintensity(CCmdUI* pCmdUI, bool doseRate);
  int DoSetIntensity(bool doseRate, float useFactor);
  afx_msg void OnPixelsizeCatalaseCrystal();
  afx_msg void OnUpdatePixelsizeCatalaseCrystal(CCmdUI *pCmdUI);
  afx_msg void OnProcessCropAverage();
  afx_msg void OnUpdateProcessCropAverage(CCmdUI *pCmdUI);
  afx_msg void OnMeshForGridBars();
  void GetPixelArrays(ShortVec **camera, ShortVec **magInd, ShortVec **addedRot,
    std::vector<float> **sizes, std::vector<float> **rotations) {*camera = &mPixSizCamera;
    *magInd = &mPixSizMagInd; *addedRot = &mAddedRotation; *sizes = &mPixelSizes;
    *rotations = &mGridRotations;};
    afx_msg void OnProcessSideBySide();
    afx_msg void OnUpdateProcessSideBySide(CCmdUI *pCmdUI);
    bool GetFFTZeroRadiiAndDefocus(EMimageBuffer * imBuf, FloatVec *radii, double &defocus,
    float phase = EXTRA_NO_VALUE);
void ModifyFFTPointToFirstZero(EMimageBuffer * imBuf, float & shiftX, float & shiftY);
bool DefocusFromPointAndZeros(double pointRad, int zeroNum, float pixel, float maxRingFreq,
  FloatVec * radii, double & defocus, float phase = EXTRA_NO_VALUE);
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
void GetDiffractionPixelSize(int camera, float &pixel, float &camLen);
float CountsPerElectronForImBuf(EMimageBuffer * imBuf);
int ReduceImage(EMimageBuffer *imBuf, float factor, CString *errStr = NULL, int toBufInd = 0, bool display = true);
afx_msg void OnProcessReduceimage();
int RunCtffind(EMimageBuffer *imBuf, CtffindParams &params, float results_array[7], bool skipOutput = false);
void SaveCtffindCrashImage(CString &message);
int InitializeCtffindParams(EMimageBuffer * imBuf, CtffindParams & params);
int MakeCtfplotterShrMemFile(int bufInd, CString &filename, float &reduction);
void DeletePlotterShrMemFile();
int RunCtfplotterOnBuffer(CString &filename, CString &command, int timeOut);
afx_msg void OnProcessDoCtffindFitOnClick();
afx_msg void OnUpdateProcessDoCtffindFitOnClick(CCmdUI *pCmdUI);
afx_msg void OnProcessSetCtffindOptions();
void SetCtffindParamsForDefocus(CtffindParams & param, double defocus, bool justMinRes);
void Initialize(void);
float GetMaxCtfFitRes(void);
WINDOWPLACEMENT *GetCtffindPlacement(void);
int CheckForBadStripe(EMimageBuffer * imBuf, int horizontal, int &numNear);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSIMAGE_H__648E61B4_B32B_4FF8_A3F7_2C8C1EF62C01__INCLUDED_)
