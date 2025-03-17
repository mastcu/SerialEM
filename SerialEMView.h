// SerialEMView.h : interface of the CSerialEMView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALEMVIEW_H__61488E01_3674_4400_89E7_0FDDE27CC3ED__INCLUDED_)
#define AFX_SERIALEMVIEW_H__61488E01_3674_4400_89E7_0FDDE27CC3ED__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CMapDrawItem;
class CShiftManager;

// Scale bar structure from 3dmod
typedef struct scale_bar {
  bool draw;
  bool white;
  int minLength;
  int thickness;
  bool vertical;
  int position;
  int indentX;
  int indentY;
  bool useCustom;
  int customVal;
} ScaleBar;

struct SnapshotData {
  DWORD memBMX, memBMY;
  CDC memDC;
  BITMAP bm;
  CBitmap bitmap;
  CBitmap *oldBitmap;
  FileOptions fileOpt;
  char *buffer;
  char *fullBuf;
  int xOffsetSave, yOffsetSave;
  int xReduction, yReduction, nxFull, nyFull;
  IntVec xStarts, yStarts, xOffsets, yOffsets;
  double zoomSave, zoomUse;
  float pixel;
  ScaleBar barParamSaved;
  float sizeScaling;
  int skipExtra;
  CString filename;
  CSerialEMView *view;
};

class DLL_IM_EX CSerialEMView : public CView
{
protected: // create from serialization only
  CSerialEMView();
  DECLARE_DYNCREATE(CSerialEMView)

  // Attributes
public:
  CSerialEMDoc* GetDocument();

  // Operations
public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSerialEMView)
public:
  virtual void OnDraw(CDC* pDC);  // overridden to draw this view
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
  virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
  //}}AFX_VIRTUAL

  // Implementation
public:
  void StopBeingActiveStack() { mStackWindow = false; };
  void StageToImage(EMimageBuffer *imBuf, float inX, float inY, float &outX, float &outY,
    int pcInd = -1, float *pcCenDist = NULL);
  void ExternalStageToImage(EMimageBuffer *imBuf, ScaleMat &aMat, float delX, float delY, 
    float inX, float inY, float &outX, float &outY, float *pcCenDist = NULL);
  BOOL ConvertMousePoint(CRect *rect, KImage *image, CPoint *point, float &outX, float &outY);
  void MakeDrawPoint(CRect *rect, KImage *image, float inX, float inY, CPoint *point, bool skipShift = false);
  void DrawCross(CDC *cdc, CPen *pNewPen, CPoint point, int crossLen);
  void DrawCircle(CDC *cdc, CPen *pNewPen, CRect *rect, KImage *image, float cenX,
    float cenY, float radius, bool skipShift = false);
  void DrawEllipse(CDC *cdc, CPen *pNewPen, CRect *rect, KImage *image, float cenX,
    float cenY, float radius1, float radius2, float angle, bool drawHalf);
  float GetBufferBinning();
  void FindEffectiveZoom();
  void SetCurrentBuffer(int inIndex);
  void TryToChangeBuffer(int iTrial, int iDir);
  void ResizeToFit(int iBordLeft, int iBordTop, int iBordRight, int iBordBottom, int useHalf);
  void NewImageScale();
  void SetZoom(double inZoom);
  void ZoomDown();
  void ZoomUp();
  SetMember(bool, FlashNextDisplay);
  SetMember(COLORREF, FlashColor)
  GetMember(bool, DrewLDAreasAtNavPt);
  GetMember(int, ImBufNumber);
  static GetMember(bool, TakingSnapshot);
  void SetOffsets(int inX, int inY) { m_iOffsetX = inX, m_iOffsetY = inY; };
  void GetCenterForLDAreas(float &xcen, float &ycen) { xcen = mNavLDAreasXcenter; ycen = mNavLDAreasYcenter; };
  virtual ~CSerialEMView();
  double GetZoom() {return mZoom; };
  int GetImBufIndex() {return mImBufIndex; };
  void SetImBufIndex(int inImBufIndex) {mImBufIndex = inImBufIndex; };
  void SetImBufs(EMimageBuffer *inImBufs, int inNumber) {mImBufs = inImBufs; mImBufNumber = inNumber;};
  void CloseFrame();
  int TakeSnapshot(float zoomBy, float sizeScaling, int skipExtra, int fileType, int compression, int quality, CString &filename);
  static void CleanupSnapshotData();
  void RestoreMembersFromSnapshot();
  void SetupMontageStarts(int image, int red, DWORD &win, IntVec &starts, IntVec &offsets, int offSign);
  void SnapshotNextTask(int shotNum);
  bool DrawToScreenOrBuffer(CDC &cdc, HDC &hdc, CRect &rect, float sizeScaling, int skipExtra, bool toBuffer);
  void DrawImage(void);
  double b3dStepPixelZoom(double czoom, int step);
  void b3dSetImageOffset(int winsize,     /* window size in wpixels.          */
                         int imsize,      /* image size in ipixels            */
                         double zoom,     /* zoom factor.                     */
                         int *drawsize,   /* size drawn in ipixels            */
                         int *offset,     /* offset from center in ipixels.   */
                         int *woff,       /* window offset in wpixels.        */
                         int *doff);       /* data offset in ipixels           */
  EMimageBuffer *GetActiveImBuf();
  static SnapshotData *mSnapshotData;   // Structure for snapshot data,to be new/deleted
  static UINT_PTR mMovieTimerID;   // ID if running a movie
  static int mMovieDir;            // -1 or +1 for current direction
  static int mMovieInterval;       // Interval for movie steps in msec
  static bool mNewMovieInterval;   // Flag to restart with new interval

#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

private:
  void NewZoom();
  CSerialEMApp * mWinApp;
  CShiftManager *mShiftManager;
  BOOL  mFirstDraw;              // Flag that first draw is being done
  bool  mMadeFonts;              // Flag that fonts were created
  double mZoom;
  double mEffectiveZoom;         // Effective zoom for unbinned images
  double mViewEffectiveZoom;     // Separate effective zooms for low dose view/search
  double mSearchEffectiveZoom;
  EMimageBuffer *mImBufs;
  BOOL mMainWindow;              // Flag that this is the main window
  BOOL mStackWindow;             // Flag that this is THE active stack view
  bool mFFTWindow;               // Flag that this is the FFT window
  bool mResizingToFit;           // Flag that a resize to fit is causing OnSize
  double mCreateTime;            // Timer for avoiding initial resizes of FFT
  int mFFTresizeCount;           // And counter
  int m_iPrevMX, m_iPrevMY;      // Previous mouse X and Y
  int m_iOffsetX, m_iOffsetY;    // Image offsets in window
  int mNonMapPanX, mNonMapPanY;  // Offset for non-map buffers
  int mImBufIndex;               // Index of current buffer
  int mImBufNumber;              // Number of image buffers
  int mImBufArraySize;           // Size of array, if this is stack view
  double m_dPrevMX, m_dPrevMY;   // Double versions of previous mouse positions
  BOOL mMouseShifting;
  int mXSrc, mYSrc;              // Starting image draw coordinates (doff, start)
  int mXDest, mYDest;            // Starting draw coordinates in window (woff, border)
  DWORD mMouseDownTime;          // Time when mouse went down
  BOOL mPanning;                 // Flag that mouse down is really a pan not a user point click
  BOOL mDrawingLine;             // Flag that we are drawing a line not panning
  CFont mFont;                   // Font for letter drawing
  CFont mLabelFont;              // Font for label drawing
  CFont *mScaledFont;            // Font when drawing with scaled snapshot
  CFont *mScaledLabelFont;       // Label font when drawing with scaled snapshot
  int mBasisSizeX, mBasisSizeY;  // Camera sizes when effective zoom first computed
  int mLastWinSizeX;             // Size at which effective zoom and zoom were set
  int mLastWinSizeY;
  ScaleMat mAmat;                // Stage to image matrix
  float mDelX, mDelY;            // Offsets in stage to camera transform
  ScaleMat mRmat, mRinv;         // Rotation matrices for adjust montage positions
  float mRdelX, mRdelY;          // Offsets in the rotation
  int mAdjustPt;                 // Flag for whether montage adjustment is needed
  float mStageErrX, mStageErrY;  // Stage errors to subtract from an item when drawing
  BOOL mCtrlPressed;             // Keep track of ctrl and shift keys
  BOOL mShiftPressed;
  ScaleBar mScaleParams;         // Scale bar parameters
  BOOL mPointNearImageCenter;    // Flag that mouse point was near center of image
  BOOL mDrawAllReg;              // Keep track if drawing all registrations
  CMapDrawItem *mAcquireBox;     // Pointer to temporary acquire box item
  CPoint mZoomupPoint;           // Mouse point to zoom up around
  float mZoomupImageX;           // Image point at that position before zoom
  float mZoomupImageY;
  bool mZoomAroundPoint;         // Flag to do zooming around a point
  bool mFlashNextDisplay;        // Flag to draw flash screen on next draw
  COLORREF mFlashColor;          // Color to use for flashing screen
  int mWheelDeltaPending;        // Delta sum below threshold
  BOOL mNavUsedLastLButtonUp;    // Flag that Navigator used last button up event
  bool mDrewLDAreasAtNavPt;      // Flag that low dose areas were drawn at a Navigator pt
  float mNavLDAreasXcenter;      // Image position around which areas were drawn
  float mNavLDAreasYcenter;
  int mDoingMontSnapshot;        // Flag that montage snapshot is being done, 2 for last
  static bool mTakingSnapshot;   // Flag set only when idle task occurs between calls

protected:

  // Generated message map functions
protected:
  //{{AFX_MSG(CSerialEMView)
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
  afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
public:
  int AddBufferToStack(EMimageBuffer * imBuf, int angleOrder);
  double BufferMemoryUsage(CPtrArray *refArray);
  void ChangeAllRegistrations(int mapID, int fromReg, int toReg);
  void DrawScaleBar(CDC * cdc, CRect * rect, EMimageBuffer * imBuf, float sizeScaling);
  void ShowImageValue(KImage * image, int imX, int imY, int pane);
  void GetLineLength(EMimageBuffer *imBuf, float &pixels, float &nanometers, float &angle,
    bool skipAngleAndjust = false);
  void GetUserBoxSize(EMimageBuffer *imBuf, int & nx, int & ny, float & xnm, float & ynm);
  int ZoomedPixelYAdjustment(CRect *rect, KImage *image);
  MapItemArray *GetMapItemsForImageCoords(EMimageBuffer *imBuf,
    bool deleteAcqBox);
  void GetItemImageCoords(EMimageBuffer *imBuf, CMapDrawItem *item, float &outX, 
    float &outY, int pcInd = -1);
  void SetStageErrIfRealignedOnMap(EMimageBuffer *imBuf, CMapDrawItem *item);
  void SetupZoomAroundPoint(CPoint * point);
  void WindowCornersInImageCoords(EMimageBuffer *imBuf, float *xCorner, float *yCorner);
  bool IsBufferInStack(EMimageBuffer *imBuf);
  void ToggleStackMovie();
  static void KillMovieTimer();
  void ChangeMovieInterval(float delta);
  void DrawMapItemBox(CDC &cdc, CRect *rect, CMapDrawItem *item, EMimageBuffer *imBuf,
    int numPoints, float delXstage, float delYstage, float delPtX, float delPtY, 
    FloatVec *drawnX, FloatVec *drawnY);
  void GetSingleAdjustmentForItem(EMimageBuffer *imBuf, CMapDrawItem *item, float &delPtX,
    float & delPtY);
  void DrawVectorPolygon(CDC &cdc, CRect *rect, CMapDrawItem *item, EMimageBuffer *imBuf,
    FloatVec &convX, FloatVec &convY, float delXstage, float delYstage, 
    float delPtX, float delPtY, FloatVec *drawnX, FloatVec *drawnY);
  int FitCtfAtMarkedPoint(EMimageBuffer *imBuf, CString &lenstr, double &defocus, FloatVec &radii);
  void DrawLowDoseAreas(CDC &cdc, CRect &rect, EMimageBuffer *imBuf, float xOffset, 
    float yOffset, int thick, int curInd, int recOnly = -1);
};

#ifndef _DEBUG  // debug version in SerialEMView.cpp
inline CSerialEMDoc* CSerialEMView::GetDocument()
   { return (CSerialEMDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERIALEMVIEW_H__61488E01_3674_4400_89E7_0FDDE27CC3ED__INCLUDED_)
