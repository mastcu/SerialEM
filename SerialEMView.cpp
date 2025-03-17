//race('1' SerialEMView.cpp:      Standard MFC MDI component, does the image display;
//                          one instance manages the main image window that 
//                          displays the stack of image buffers, and others
//                          can be created for secondary image windows
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//

#include "stdafx.h"
#include "SerialEM.h"
#include "afxtempl.h"
#include "SerialEMDoc.h"
#include ".\SerialEMView.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "EMbufferWindow.h"
#include "EMbufferManager.h"
#include "ShiftManager.h"
#include "CameraController.h"
#include "ProcessImage.h"
#include "MapDrawItem.h"
#include "NavigatorDlg.h"
#include "LogWindow.h"
#include "NavHelper.h"
#include "HoleFinderDlg.h"
#include "MultiHoleCombiner.h"
#include "AutoContouringDlg.h"
#include "ScreenShotDialog.h"
#include "AutoTuning.h"
#include "ComaVsISCalDlg.h"
#include "ParticleTasks.h"
#include "MacroProcessor.h"
#include "MultiTSTasks.h"
#include "MultiGridTasks.h"
#include "RemoteControl.h"
#include "ExternalTools.h"
#include "Image\KStoreIMOD.h"
#include "Shared\ctffind.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"
#include "XFolderDialog/XWinVer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static VOID CALLBACK MovieProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

#define USER_PT_CLICK_TIME 500
#define PAN_THRESH 5
static double zoomvals[] =
{0.01, 0.02, 0.04, 0.06, 0.09, 0.125, 0.185, 0.25, 0.37, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0,
  4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0};

#define MAXZOOMI 21

bool CSerialEMView::mTakingSnapshot = false;
SnapshotData *CSerialEMView::mSnapshotData = NULL;

UINT_PTR CSerialEMView::mMovieTimerID = 0;
int CSerialEMView::mMovieDir = 1;
int CSerialEMView::mMovieInterval = 250;
bool CSerialEMView::mNewMovieInterval = false;


/////////////////////////////////////////////////////////////////////////////
// CSerialEMView

IMPLEMENT_DYNCREATE(CSerialEMView, CView)

BEGIN_MESSAGE_MAP(CSerialEMView, CView)
  //{{AFX_MSG_MAP(CSerialEMView)
  ON_WM_LBUTTONDOWN()
  ON_WM_MBUTTONDOWN()
  ON_WM_MOUSEMOVE()
  ON_WM_MOUSEWHEEL()
  ON_WM_KEYDOWN()
  ON_WM_SYSKEYDOWN()
  ON_WM_KEYUP()
  ON_WM_RBUTTONDOWN()
  ON_WM_LBUTTONUP()
  ON_WM_LBUTTONDBLCLK()
  ON_WM_MBUTTONDBLCLK()
  ON_WM_ERASEBKGND()
  ON_WM_RBUTTONUP()
  ON_WM_HELPINFO()
	ON_WM_DESTROY()
  ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSerialEMView construction/destruction

CSerialEMView::CSerialEMView()
{
  SEMBuildTime(__DATE__, __TIME__);
  mImBufs = NULL;
  mImBufNumber = 1;
  mImBufArraySize = 0;
  mResizingToFit = false;
  mZoom = 1.0;
  m_iOffsetX = 0;
  m_iOffsetY = 0;
  mNonMapPanX = 0;
  mNonMapPanY = 0;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mShiftManager = mWinApp->mShiftManager;
  mFirstDraw = true;
  mMadeFonts = false;
  mMouseShifting = false;
  mMainWindow = false;
  mStackWindow = false;
  mFFTWindow = false;
  mShiftPressed = false;
  mCtrlPressed = false;
  mPanning = false;  
  mViewEffectiveZoom = 0.;
  mSearchEffectiveZoom = 0.;
  mScaleParams.draw = true;
  mScaleParams.white = false;
  mScaleParams.minLength = 50;
  mScaleParams.thickness = 5;
  mScaleParams.vertical = false;
  mScaleParams.position = 0;
  mScaleParams.indentX = 30;
  mScaleParams.indentY = 25;
  mScaleParams.useCustom = false;
  mScaleParams.customVal = 25;
  mScaledFont = NULL;
  mZoomAroundPoint = false;
  mFlashNextDisplay = false;
  mWheelDeltaPending = 0;
  mLastWinSizeX = 0;
  mLastWinSizeY = 0;
  mDoingMontSnapshot = 0;
}

CSerialEMView::~CSerialEMView()
{
  if (mImBufs && !mMainWindow && !mFFTWindow) {
    if (mImBufArraySize > 1)
      delete [] mImBufs;
    else
      delete mImBufs;
  }
}

BOOL CSerialEMView::PreCreateWindow(CREATESTRUCT& cs)
{
  int iBordTop, iBordLeft, iBordBottom, iBordRight, type, width;
  static int windowNum = 1;
  CString str;

  // The parent is good for getting the rect but it is not really the mainFrame - it is
  // the client window under the mainframe.  So we have to get menu from THE mainFrame
  CChildFrame *childFrame = (CChildFrame *)CWnd::FromHandle(cs.hwndParent);
  CWnd *client = (CWnd *)childFrame->GetParent();
  CRect rect;
  client->GetClientRect(&rect);

  type = mWinApp->GetNewViewProperties(this, iBordLeft, iBordTop, iBordRight, iBordBottom,
    mImBufs, mImBufNumber, mImBufIndex);
  if (type != 1 && mWinApp->mMainFrame->GetRebuiltMenu()) {
    childFrame->SetSharedMenu(mWinApp->mMainFrame->GetRebuiltMenu());
  }
  if (type == 1) {
  
    // If this is the static window, set the size to fill the client area
    childFrame->SetWindowPos(NULL, rect.left + iBordLeft, rect.top + iBordTop,
      rect.Width() - iBordLeft - iBordRight, 
      rect.Height() - iBordTop - iBordBottom, SWP_NOZORDER);
    childFrame->SetStaticFrame();
    childFrame->SetWindowText("Main Window");
    mMainWindow = true;
  } else if (type == 3) {
    width = (rect.Width() - iBordLeft - iBordRight);
    childFrame->SetWindowPos(NULL, rect.left + iBordLeft, rect.top + iBordTop,
      width, rect.Height() - iBordTop - iBordBottom, SWP_NOZORDER);
    childFrame->SetWindowText("FFT");
    mFFTWindow = true;
  } else {
    //  Otherwise use the size that is passed in, put in lower right corner
    if (iBordRight > rect.Width() - iBordLeft)
      iBordRight = rect.Width() - iBordLeft;
    childFrame->SetWindowPos(NULL, rect.right - iBordRight, rect.bottom - iBordBottom,
      iBordRight, iBordBottom, SWP_NOZORDER);

    if (type == 2) {
      mImBufArraySize = 1;
      str = mWinApp->mMacroProcessor->GetScriptWindowTitle();
      if (str.IsEmpty())
        str = mWinApp->m_strTitle;
      if (mWinApp->mMultiTSTasks->GetAssessingRange())
        str = "Tilt Range Sample";
      mStackWindow = true;
    }

    // Get window title
    if (str.IsEmpty()) {
      str.Format("Window %d", windowNum);
      windowNum++;
      KGetOneString("Title for new window:", str);
    }
    childFrame->SetWindowText(str);
    mWinApp->ViewOpening();
   // So that the frame can close us up properly
    //childFrame->SetChildView(this);
  }

  if (mFFTWindow) {
    mCreateTime = GetTickCount();
    mFFTresizeCount = 0;
  }
  return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CSerialEMView drawing

void CSerialEMView::OnDraw(CDC* pDC)
{
  CSerialEMDoc* pDoc = GetDocument();
  ASSERT_VALID(pDoc);
  DrawImage();
}

/////////////////////////////////////////////////////////////////////////////
// CSerialEMView diagnostics

#ifdef _DEBUG
void CSerialEMView::AssertValid() const
{
  CView::AssertValid();
}

void CSerialEMView::Dump(CDumpContext& dc) const
{
  CView::Dump(dc);
}

CSerialEMDoc* CSerialEMView::GetDocument() // non-debug version is inline
{
  ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSerialEMDoc)));
  return (CSerialEMDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSerialEMView message handlers

void CSerialEMView::OnDestroy()
{
  if (mMainWindow)
    mWinApp->mMainView = NULL;
  else
    mWinApp->ViewClosing(mStackWindow, mFFTWindow, this);
  CView::OnDestroy();
}

void CSerialEMView::CloseFrame()
{
  mWinApp->mActiveView = mWinApp->mMainView;
  CChildFrame *childFrame = (CChildFrame *)GetParent();
  childFrame->OnClose();
}

#define DSB_SCALE(a) B3DNINT(sd->sizeScaling * (a))
#define DSB_DPI_SCALE(a) mWinApp->ScaleValueForDPI(sizeScaling * (a))


// Takes an image snapshot and saves it, managing all the changes needed before and
// after calling the general draw routine
// NOTE: Hans Windohoff worked out how to draw to a bitmap and changed the draw routines
// to work with CDC instead of CClientDC
int CSerialEMView::TakeSnapshot(float zoomBy, float sizeScaling, int skipExtra,
  int fileType, int compression, int quality, CString &filename)
{
  CRect rectWin;
  EMimageBuffer *imBuf = NULL;
  KImage      *imageRect;
  unsigned int pixelLimit = 32768 * 16384;
  CClientDC cdcWin(this);
  int nxImage, nyImage, nxDrawn, nyDrawn, nxWin, nyWin, err = 0;
  int xyReduction;
  SnapshotData *sd;
  double wallStart = wallTime();
  
  // Get window size
  GetClientRect(&rectWin);
  nxWin = rectWin.Width();
  nyWin = rectWin.Height();


  // Make sure there is an image and get its size
  if (mImBufs) {
    imBuf = &mImBufs[mImBufIndex];
    imageRect = imBuf->mImage;
  }
  if (!imBuf || !imageRect)
    return 1;
  imageRect->getSize(nxImage, nyImage);

  mSnapshotData = new SnapshotData;
  sd = mSnapshotData;
  sd->view = this;

  // Fill in the fileOpt;
  sd->filename = filename;
  sd->fileOpt.compression = compression;
  sd->fileOpt.fileType = fileType;
  sd->fileOpt.jpegQuality = quality;
  sd->fileOpt.montageInMdoc = false;
  sd->fileOpt.typext = 0;
  
  sd->fullBuf = NULL;
  sd->buffer = NULL;
  sd->oldBitmap = NULL;
  sd->xOffsetSave = m_iOffsetX;
  sd->yOffsetSave = m_iOffsetY;
  sd->xReduction = 1;
  sd->yReduction = 1;
  sd->nxFull = 0;
  sd->sizeScaling = sizeScaling;
  sd->skipExtra = skipExtra;
  sd->zoomSave = mZoom;
  sd->pixel = mShiftManager->GetPixelSize(imBuf) * 1.e4f;


  // For 1:1 whole image drawing, set size to fill image and zoom to 1
  if (zoomBy <= 0) {
    sd->zoomUse = 1.;
    sd->memBMX = nxImage;
    sd->memBMY = nyImage;
  } else {

    // Otherwise, draw the component of the image in the window by taking the min of the
    // zoomed image size and window size; then zoom up by desired factor
    nxDrawn = B3DMIN((int)(mZoom * nxImage), nxWin);
    nyDrawn = B3DMIN((int)(mZoom * nyImage), nyWin);
    sd->memBMX = (DWORD)zoomBy *nxDrawn;
    sd->memBMY = (DWORD)zoomBy *nyDrawn;
    sd->zoomUse = mZoom * zoomBy;
  }

  // Scale sizes by change in zoom, assign to sizeScaling so macros work right
  if (sd->sizeScaling <= 0.)
    sizeScaling = sd->sizeScaling = (float)B3DMAX(1., sd->zoomUse / mZoom);

  // The # of bytes in the bitmap is limited 
  if ((size_t)sd->memBMX * sd->memBMY > pixelLimit) {
    if (sd->zoomUse != 1.)
      return 5;
    xyReduction = ((size_t)sd->memBMX * sd->memBMY + pixelLimit - 1) / pixelLimit;
    sd->xReduction = (int)ceil(sqrt((double)xyReduction));
    B3DCLAMP(sd->xReduction, 1, xyReduction);
    sd->yReduction = (xyReduction + sd->xReduction - 1) / sd->xReduction;
    sd->nxFull = sd->memBMX;
    sd->nyFull = sd->memBMY;
    SetupMontageStarts(sd->nxFull, sd->xReduction, sd->memBMX, sd->xStarts, 
      sd->xOffsets, 1);
    SetupMontageStarts(sd->nyFull, sd->yReduction, sd->memBMY, sd->yStarts, 
      sd->yOffsets, -1);
  }

  // Get compatible device context to window
  sd->memDC.CreateCompatibleDC(&cdcWin);
 
  // Get bitmap of the desired size and make sure it is 24 or 32 bit
  if (!sd->bitmap.CreateCompatibleBitmap(&cdcWin, sd->memBMX, sd->memBMY))
    err = 2;

  if (!err && !sd->bitmap.GetObject(sizeof(BITMAP), &sd->bm))
    err = 3;
  if (!err && sd->bm.bmBitsPixel != 24 && sd->bm.bmBitsPixel != 32)
    err = 4;

  // Get buffer to copy into 
  if (!err && sd->nxFull) {
    NewArray2(sd->fullBuf, char, sd->nxFull, sd->nyFull * 3);
    if (!sd->fullBuf)
      err = 2;
  }
  if (err) {
    CleanupSnapshotData();
    return err;
  }
  NewArray2(sd->buffer, char, sd->bm.bmWidthBytes, sd->memBMY);
  if (!sd->buffer) {
    CleanupSnapshotData();
    return 2;
  }

  sd->oldBitmap = (CBitmap *)sd->memDC.SelectObject(&sd->bitmap);

  // Adjust various things for size scaling here
  mZoom = sd->zoomUse;
  if (sd->sizeScaling > 1.) {
    sd->barParamSaved = mScaleParams;
    mScaleParams.minLength = DSB_SCALE(mScaleParams.minLength);
    mScaleParams.thickness = DSB_SCALE(mScaleParams.thickness);
    mScaleParams.indentX = DSB_SCALE(mScaleParams.indentX);
    mScaleParams.indentY = DSB_SCALE(mScaleParams.indentY);
    CString fontName = Is2000OrGreater() ? "Microsoft Sans Serif" : "MS Sans Serif";
    mScaledFont = new CFont();
    mScaledLabelFont = new CFont();
    mScaledFont->CreateFont(DSB_DPI_SCALE(36), 0, 0, 0,
      FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, fontName);
    mScaledLabelFont->CreateFont(DSB_DPI_SCALE(24), 0,
      0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, fontName);
  }

  // Start the draw
  if (sd->xReduction * sd->yReduction == 1) {
    SnapshotNextTask(0);
  } else {
    mTakingSnapshot = true;
    mWinApp->SetStatusText(SIMPLE_PANE, "TAKING SNAPSHOT");
    mWinApp->UpdateBufferWindows();
    mWinApp->AddIdleTask(TASK_SNAPSHOT_TO_BUF, 0, 0);
  }

  return 0;
}

// Do any cleanup tasks and delete the snapshot data structure, which handles its members
void CSerialEMView::CleanupSnapshotData()
{
  SnapshotData *sd = mSnapshotData;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  if (!sd)
    return;
  if (sd->oldBitmap)
    sd->memDC.SelectObject(sd->oldBitmap);
  delete [] sd->buffer;
  delete [] sd->fullBuf;
  sd->view->RestoreMembersFromSnapshot();
  B3DDELETE(mSnapshotData);
  if (mTakingSnapshot) {
    mTakingSnapshot = false;
    winApp->SetStatusText(SIMPLE_PANE, "");
    winApp->UpdateBufferWindows();
  }
}

void CSerialEMView::RestoreMembersFromSnapshot()
{
  SnapshotData *sd = mSnapshotData;
  mZoom = sd->zoomSave;
  m_iOffsetX = sd->xOffsetSave;
  m_iOffsetY = sd->yOffsetSave;
  if (mScaledFont) {
    mScaleParams = sd->barParamSaved;
    B3DDELETE(mScaledFont);
    delete mScaledLabelFont;
  }
}

// Compute the memory window size and starting index in image and the display offsets for
// a montage along one axis
void CSerialEMView::SetupMontageStarts(int image, int red, DWORD &win, IntVec &starts,
  IntVec &offsets, int offSign)
{
  int extra, rem, overlap, ind;
  starts.push_back(0);
  if (red == 1) {
    win = image;
    offsets.push_back(0);
    return;
  }
  win = (image + red - 1) / red;
  win = 4 * ((win + 3) / 4);
  extra = red * win - image;
  overlap = extra / (red - 1);
  rem = extra % (red - 1);
  for (ind = 1; ind < red; ind++)
    starts.push_back(starts[ind - 1] + win - (overlap + (ind <= rem ? 1 : 0)));
  for (ind = 0; ind < red; ind++)
    offsets.push_back(offSign * (image - (2 * starts[ind] + (int)win)) / 2);
}

// Next task for snapshot: take a panel, and start save if done
void CSerialEMView::SnapshotNextTask(int shotNum)
{
  int iy, ix, inAdd, err, ixShot, iyShot;
  bool drew, lastPiece;
  char *bufIn, *bufOut;
  int addDPI = mWinApp->GetAddDPItoSnapshots();
  CString report;
  SnapshotData *sd = mSnapshotData;
  if (!sd)
    return;

  // Get the window rectangle
  CRect rect(0, 0, sd->memBMX, sd->memBMY);
  iyShot = shotNum / sd->xReduction;
  ixShot = shotNum % sd->xReduction;
  lastPiece = ixShot == sd->xReduction - 1 && iyShot == sd->yReduction - 1;
  if (sd->nxFull) {
    mDoingMontSnapshot = lastPiece ? 2 : 1;
    mXSrc = sd->xStarts[ixShot];
    mYSrc = sd->nyFull - (sd->memBMY + sd->yStarts[iyShot]);
    m_iOffsetX = sd->xOffsets[ixShot];
    m_iOffsetY = sd->yOffsets[iyShot];
  }
  drew = DrawToScreenOrBuffer(sd->memDC, sd->memDC.m_hDC, rect, sd->sizeScaling, 
    sd->skipExtra, true);
  err = drew ? 0 : 1;
  mDoingMontSnapshot = 0;

  // Get the buffer copy
  if (!err && !sd->bitmap.GetBitmapBits(sd->bm.bmWidthBytes * sd->memBMY, sd->buffer))
    err = 3;
  if (err) {
    CScreenShotDialog::GetSnapshotError(err, report);
    SEMMessageBox(report);
    CleanupSnapshotData();
    return;
  }

  // Pack the buffer to contiguous RGB or into part of fullBuf: it is BGR or BRGA
  bufOut = sd->buffer;
  inAdd = sd->bm.bmBitsPixel / 8;
  for (iy = 0; iy < (int)sd->memBMY; iy++) {
    bufIn = sd->buffer + iy * sd->bm.bmWidthBytes;
    if (sd->nxFull)
      bufOut = sd->fullBuf + ((iy + sd->yStarts[iyShot]) * 
      (size_t)sd->nxFull + sd->xStarts[ixShot]) * 3;
    for (ix = 0; ix < (int)sd->memBMX; ix++) {
      *bufOut++ = *(bufIn + 2);
      *bufOut++ = *(bufIn + 1);
      *bufOut++ = *bufIn;
      bufIn += inAdd;
    }
  }

  shotNum++;
  if (shotNum < sd->xReduction * sd->yReduction) {
    mTakingSnapshot = true;
    if (shotNum == 1) {
      mWinApp->SetStatusText(SIMPLE_PANE, "TAKING SNAPSHOT");
      mWinApp->UpdateBufferWindows();
    }
    mWinApp->AddIdleTask(TASK_SNAPSHOT_TO_BUF, shotNum, 0);
    return;
  }


  // Put it in a KImage and save it
  KImageRGB *image = new KImageRGB();
  if (sd->nxFull) {
    image->useData(sd->fullBuf, sd->nxFull, sd->nyFull);
    DELETE_ARR(sd->buffer);
    sd->fullBuf = NULL;
  } else {
    image->useData(sd->buffer, sd->memBMX, sd->memBMY);
    sd->buffer = NULL;
  }
  if ((sd->pixel && addDPI > 0) || addDPI > 1) {
    EMimageExtra *extra = new EMimageExtra();
    if (addDPI > 1)
      extra->mPixel = (float)(25400. / addDPI);
    else
      extra->mPixel = sd->pixel / (float)sd->zoomUse;
    image->SetUserData(extra);
  }

  KStoreIMOD *store = new KStoreIMOD(sd->filename, sd->fileOpt);
  CleanupSnapshotData();
  if (shotNum > 1)
    mWinApp->UpdateBufferWindows();

  if (!store->FileOK()) {
    SEMMessageBox(CString("Error opening file for snapshot: ") + b3dGetError());
    delete image;
    delete store;
    return;
  }

  mWinApp->mBufferManager->CheckAsyncSaving();
  mWinApp->mBufferManager->StartAsyncSave(store, image, 0, 3);
}


// Entry point for a regular image draw to screen
void CSerialEMView::DrawImage(void)
{
  if (mWinApp->GetInRestoreViewFocus() || 
    (mWinApp->mNavHelper && mWinApp->mNavHelper->GetDoingMultipleFiles()))
    return;
  CRect rect;
  GetClientRect(rect);
  CClientDC cdcWin(this);

  // Get the device context for the bitmap drawing calls
  HDC hdc = ::GetDC(m_hWnd); // handle to device context
  DrawToScreenOrBuffer(cdcWin, hdc, rect, 1., 0, false);
  ::ReleaseDC(m_hWnd, hdc);
  mWinApp->SetLastActivityTime(GetTickCount());
}

// Main drawing routine to screen or buffer.  Skipextra is one to skip some items plus
// 2 to skip navigator items plus 4 to skip the scale bar
bool CSerialEMView::DrawToScreenOrBuffer(CDC &cdc, HDC &hdc, CRect &rect, 
  float sizeScaling, int skipExtra, bool toBuffer)
{
  int iSrcWidth, iSrcHeight, iDestWidth, iDestHeight, crossLen, xSrc, ySrc, needWidth;
  int tmpWidth, tmpHeight, ierr, ifilt, numLines, thick, loop, ix, iy, group, numGroups;
  int adjSave;
  float imXcenter, imYcenter, halfXwin, halfYwin, tempX, tempY, ptX, ptY;
  float comaXcen, comaYcen;
  float minXstage = 1.e30f, maxXstage = -1.e30f, minYstage = 1.e30f, maxYstage = -1.e30f;
  float cenX, cenY, scale, rotation, filtMean = 128., filtSD, boost, targetSD = 40.;
  float crossXoffset = 0., crossYoffset = 0., minLimX, minLimY, maxLimX, maxLimY;
  double defocus;
  unsigned char **filtPtrs;
  int zoomFilters[] = {5, 4, 1, 0};  // lanczos3, 2, Blackman, box
  int numZoomFilt = sizeof(zoomFilters) / sizeof(int);
  int zoomWidthCrit = 20;
  bool bufferOK, drawUserPt, filtering = false, saveCurHolePos = false;
  BOOL drawIncluded, drawExcluded, bufIsFFT = false;
  CNavigatorDlg *navigator = mWinApp->mNavigator;
  CProcessImage *processImg = mWinApp->mProcessImage;
  MapItemArray *itemArray = NULL;
  MapItemArray *polyArray;
  CMapDrawItem *item, *polygon;
  int *showGroup;
  COLORREF contColors[MAX_AUTOCONT_GROUPS] = {RGB(220,0,0), RGB(245,160,0), 
    RGB(255,255,0), RGB(0,255,0), RGB(0,255,255), RGB(0,0,255), RGB(178,107,210),
    RGB(255,0,255)};
  FloatVec zeroRadii, zeroRadii2;
  FloatVec *curHoleXYpos, *xHoleCens, *yHoleCens;
  IntVec *curHoleIndex, *pieceOn;
  ShortVec *holeExcludes, *polyGroups;
  CFont *useFont, *useLabelFont;
  CPoint point;
  LowDoseParams *ldp;
  CString letString, firstLabel, lastLabel, fontName;
  ScaleMat aInv, isToCamNoFoc, isToCam;
  double transX, transY;
  COLORREF bkgColor = RGB(48, 0, 48);
  COLORREF flashColor = RGB(192, 192, 0), lowExcludeColor = RGB(0, 255, 255);
  COLORREF includeColor = RGB(255, 0, 160), highExcludeColor = RGB(0, 100, 255);
  COLORREF userIncludeColor = RGB(255, 160, 0), userExcludeColor = RGB(0, 180, 80);
  int scaled5 = DSB_DPI_SCALE(5);
  int scaled10 = DSB_DPI_SCALE(10);
  int scaled140 = DSB_DPI_SCALE(140);
  int thick1 = DSB_DPI_SCALE(1);
  int thick2 = DSB_DPI_SCALE(2.);

  // If this is the first draw, set up the zoom optimally
  // 7/7/03: no longer need to fix main frame window title; set child titles
  if (mFirstDraw && !(mWinApp->GetStartingProgram() && 
    (!mImBufs || !mImBufs[mImBufIndex].mImage))) {
    FindEffectiveZoom();
    CChildFrame *childFrame = (CChildFrame *)GetParent();
    mFirstDraw = false;
    if (mFFTWindow)
      ((CMDIChildWnd *)(mWinApp->mMainView->GetParent()))->MDIActivate();
  }
 
  EMimageBuffer *imBuf = NULL;
  KImage      *imageRect;
  if (mImBufs) {
    imBuf = &mImBufs[mImBufIndex];
    imageRect = imBuf->mImage;
    bufIsFFT = mFFTWindow || imBuf->mCaptured == BUFFER_FFT ||
      imBuf->mCaptured == BUFFER_LIVE_FFT || imBuf->mCaptured == BUFFER_AUTOCOR_OVERVIEW;
  }

  if (mWinApp->mNavigator) {
    saveCurHolePos = mWinApp->mNavigator->GetHolePositionVectors(&curHoleXYpos,
      &curHoleIndex);
    curHoleXYpos->clear();
    curHoleIndex->clear();
  }

  // If there is no image, fill the whole area
  if (mFlashNextDisplay || imBuf == NULL || imageRect == NULL) {
    cdc.FillSolidRect(rect, mFlashNextDisplay ? mFlashColor : bkgColor);
    mFlashNextDisplay = false;
    return false;
  }

  // Get fonts for drawing letter in corner and other text on first real draw
  if (!mMadeFonts) {
    
    fontName = Is2000OrGreater() ? "Microsoft Sans Serif" : "MS Sans Serif";
    mFont.CreateFont(mWinApp->ScaleValueForDPI(36), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, fontName);
    mLabelFont.CreateFont(mWinApp->ScaleValueForDPI(24), 0, 0, 0, FW_MEDIUM,
      0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
      CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
      FF_DONTCARE, fontName);
    mMadeFonts = true;

    // Also scale the scale bar constants
    mScaleParams.minLength = mWinApp->ScaleValueForDPI(mScaleParams.minLength);
    mScaleParams.thickness = mWinApp->ScaleValueForDPI(mScaleParams.thickness);
    mScaleParams.indentX = mWinApp->ScaleValueForDPI(mScaleParams.indentX);
    mScaleParams.indentY = mWinApp->ScaleValueForDPI(mScaleParams.indentY);

  }
  useFont = sizeScaling > 1. ? mScaledFont : &mFont;
  useLabelFont = sizeScaling > 1. ? mScaledLabelFont : &mLabelFont;

  // Update the pixmap and its scaling and image data
  imBuf->UpdatePixMap();
  KPixMap *pixMap = imBuf->mPixMap;

  // Loop twice if zooming around point; get offset first time, then determine image
  // position of the same window position and adjust offset to bring image back there
  for (loop = 0; loop < (mZoomAroundPoint ? 2 : 1); loop++) {
    if (loop && mZoomAroundPoint &&
      ConvertMousePoint(&rect, imageRect, &mZoomupPoint, cenX, cenY)) {
      m_iOffsetX += B3DNINT(cenX - mZoomupImageX);
      m_iOffsetY -= B3DNINT(cenY - mZoomupImageY);
    }

    if (mDoingMontSnapshot) {
      mXDest = 0;
      mYDest = 0;
      iSrcWidth = rect.Width();
      iSrcHeight = rect.Height();
    } else {

      // Set the image and window offsets for drawing, given sizes and mouse shifts
      // This routine thinks these things are bottom-based
      b3dSetImageOffset(rect.Width(), imageRect->getWidth(), mZoom, &iSrcWidth,
        &m_iOffsetX, &mXDest, &mXSrc);
      if (!m_iOffsetX && mXSrc > 0) {
        crossXoffset = (float)((imageRect->getWidth() / 2. - mXSrc) * mZoom -
          rect.Width() / 2.);
        if (crossXoffset > 0.5 * mZoom) {
          mXSrc++;
          crossXoffset -= (float)mZoom;
        }
      }
      mXDest += rect.left;
      b3dSetImageOffset(rect.Height(), imageRect->getHeight(), mZoom, &iSrcHeight,
        &m_iOffsetY, &mYDest, &mYSrc);
      if (!m_iOffsetY && mYSrc > 0)
        crossYoffset = (float)((imageRect->getHeight() / 2. - mYSrc) * mZoom -
          rect.Height() / 2.);
      if (crossYoffset > 0.5 * mZoom)
        crossYoffset -= (float)mZoom;
    }
  }

  mZoomAroundPoint = false;
  if (!toBuffer) {
    if (imBuf->mMapID) {
      imBuf->mPanX = m_iOffsetX;
      imBuf->mPanY = m_iOffsetY;
    } else {
      mNonMapPanX = m_iOffsetX;
      mNonMapPanY = m_iOffsetY;
    }
  }

  // Workaround to problem that it displays the top of the array instead of the bottom
  // when source is at 0,0 and Y image size is bigger than display
  if (iSrcHeight < imageRect->getHeight() && !mXSrc && !mYSrc) {
    mYSrc++;
    m_iOffsetY--;
  }
  mYDest += rect.top;  // but this is top-based in the call
  KImage *bitImage = pixMap->getImRectPtr();
  
  // Set variables for draw from pixmap, then see if criteria for zoom filter are met
  iDestWidth = (int)floor(mZoom * iSrcWidth + 0.5);
  iDestHeight = (int)floor(mZoom * iSrcHeight + 0.5);
  xSrc = mXSrc;
  ySrc = mYSrc;
  if (mZoom < 0.8 && mWinApp->mBufferManager->GetAntialias() && bitImage && 
    bitImage->getRowData(0) && !mPanning) {

    // Get truncated width to avoid error from filter routine
    // Get a pixmap if needed, and set the image levels in there
    tmpWidth = (int)floor(mZoom * iSrcWidth);
    tmpHeight = (int)floor(mZoom * iSrcHeight);
    if (!imBuf->mFiltPixMap)
      imBuf->mFiltPixMap = new KPixMap();

    // The width has to be padded.  The pixmap image is reusable as is if it exists and
    // its type and size match, and if the zoom and pan match when it was created
    KImage *filtImage = imBuf->mFiltPixMap->getImRectPtr();
    needWidth = 4 * ((tmpWidth + 3) / 4);
    bool sizeTypeOK = filtImage && filtImage->getType() == bitImage->getType() &&  
      filtImage->getWidth() == needWidth && filtImage->getHeight() == tmpHeight;
    bool byteMap = bitImage->getType() == kUBYTE;
    filtering = true;
    if (!sizeTypeOK || imBuf->mFiltZoom != mZoom || imBuf->mFiltXpan != m_iOffsetX ||
      imBuf->mFiltYpan != m_iOffsetY) {

      // If the image doesn't match up, get a new pixmap image if needed or reuse
      if (!sizeTypeOK) {
        if (UtilOKtoAllocate((byteMap ? 1 : 3) * needWidth * tmpHeight)) {
          if (byteMap)
            filtImage = new KImage(needWidth, tmpHeight);
          else
            filtImage = new KImageRGB(needWidth, tmpHeight);
        } else {
          filtImage = new KImage();
          mWinApp->AppendToLog("There is not enough memory for filtering array; "
            "delete some buffers");
        }
      } else {  
        filtImage = new KImage(filtImage);
      }

      // Select a filter as in 3dmod
      for (ifilt = 0; ifilt < numZoomFilt; ifilt++) {
        ierr = selectZoomFilter(zoomFilters[ifilt], mZoom, &numLines);
        if (!ierr && (numLines * tmpWidth * tmpHeight) / (250000 * numOMPthreads(8)) <=
          zoomWidthCrit)
          break;
      }

      // Get the line points and get the zoomed down image
      // The ySrc is from the bottom of the image, which works because of the negative
      // height in the BMI, but need to get it to be top line in image here
      filtPtrs = makeLinePointers(bitImage->getRowData(0), bitImage->getWidth(), 
        bitImage->getHeight(), byteMap ? 1 : 3);
      if (filtImage->getWidth() && filtPtrs && !ierr &&
        !zoomWithFilter(filtPtrs, bitImage->getWidth(), bitImage->getHeight(), 
          (float)mXSrc, (float)(bitImage->getHeight() - 1 - (mYSrc + iSrcHeight - 1)),
          tmpWidth, tmpHeight, needWidth, 0, byteMap ? SLICE_MODE_BYTE : SLICE_MODE_RGB, 
          filtImage->getRowData(0), NULL, NULL)) {
        imBuf->mFiltPixMap->useRect(filtImage, true);
      } else {
        filtering = false;
      }
      delete filtImage;
      B3DFREE(filtPtrs);
    }

    // If all worked out, commit to using this as the pixmap
    if (filtering) {
      bitImage = imBuf->mFiltPixMap->getImRectPtr();
      pixMap = imBuf->mFiltPixMap;
      boost = 1.;
      if (byteMap && !(imBuf->mCaptured == BUFFER_FFT || 
        imBuf->mCaptured == BUFFER_LIVE_FFT)) {
          CorDefSampleMeanSD(bitImage->getRowData(0), SLICE_MODE_BYTE, needWidth, 
            tmpWidth, tmpHeight, &filtMean, &filtSD);
          if (filtSD < targetSD)
            boost = targetSD / filtSD;
      }
      pixMap->setLevels(imBuf->mImageScale->GetBrightness(), 
        imBuf->mImageScale->GetContrast(), imBuf->mImageScale->GetInverted(), 
        imBuf->mImageScale->GetFalseColor(), boost, filtMean);
      xSrc = 0;
      ySrc = 0;
      iDestWidth = tmpWidth;
      iDestHeight = tmpHeight;
      iSrcWidth = tmpWidth;
      iSrcHeight = tmpHeight;
      imBuf->mFiltZoom = mZoom;
      imBuf->mFiltXpan = m_iOffsetX;
      imBuf->mFiltYpan = m_iOffsetY;
    }
  }

  // Recover pixmap image memory if not filtering
  if (!filtering && imBuf->mFiltPixMap)
    imBuf->mFiltPixMap->doneWithRect();

  // Draw the image data in the window
  char *cPixels = NULL;
  if (bitImage)
    cPixels = bitImage->getRowData(0);
  BITMAPINFO *bMI = pixMap->getBMInfo();
  SetStretchBltMode(hdc, COLORONCOLOR);
  if (cPixels) {
    StretchDIBits(hdc, mXDest, mYDest, iDestWidth, iDestHeight, xSrc, ySrc, iSrcWidth,
      iSrcHeight, (VOID *)cPixels, bMI, DIB_RGB_COLORS, SRCCOPY);
  } else {
     cdc.FillSolidRect(rect, bkgColor);
     mWinApp->AppendToLog("There is not enough memory to display this image; "
       "you need to delete some buffers");
  }

  // Fill sides if needed
  if (mYDest > rect.top)
    cdc.FillSolidRect(rect.left, rect.top, rect.Width(), mYDest - rect.top, bkgColor);
  if (mYDest + iDestHeight < rect.bottom)
    cdc.FillSolidRect(rect.left, mYDest + iDestHeight, rect.Width(), 
      rect.bottom - (mYDest + iDestHeight), bkgColor);
  if (mXDest > rect.left)
    cdc.FillSolidRect(rect.left, mYDest, mXDest - rect.left, iDestHeight, bkgColor);
  if (mXDest + iDestWidth < rect.right)
    cdc.FillSolidRect(mXDest + iDestWidth, mYDest, rect.right - (mXDest + iDestWidth),
      iDestHeight, bkgColor);
  
  // Draw crosshairs if mouse shifting is underway
  if (!toBuffer&& (mMouseShifting || mWinApp->mBufferManager->GetDrawCrosshairs() || 
    (navigator && navigator->MovingMapItem()))) {
    CPen pnSolidPen (PS_SOLID, 1, mShiftManager->GetShiftingDefineArea() ?
      RGB(0, 255, 0) : RGB(255, 0, 0));
    crossLen = (rect.Width() < rect.Height() ? rect.Width() : rect.Height()) / 5;
    CPoint point = rect.CenterPoint();
    point.x += B3DNINT(crossXoffset);
    point.y += B3DNINT(crossYoffset);
    DrawCross(&cdc, &pnSolidPen, point, crossLen);
  }

  if (mMouseShifting) {
    // Check for user point in autoalign buffer
    int autoIndex = mWinApp->mBufferManager->AutoalignBufferIndex();
    EMimageBuffer *autoBuf = &mImBufs[autoIndex];
    KImage *autoIm = autoBuf->mImage;
    if (autoIm && autoBuf->mHasUserPt) {
      float shiftX, shiftY;
      autoIm->getShifts(shiftX, shiftY);
      CPen pnRedPen (PS_SOLID, 2, RGB(255, 0, 0));
      CPoint point;
      crossLen = 7;

      // Get the projection positions by referencing to center of each image
      float projX = autoBuf->mUserPtX + shiftX - autoIm->getWidth() / 2 + 
        imBuf->mImage->getWidth() / 2;
      float projY = autoBuf->mUserPtY + shiftY - autoIm->getHeight() / 2 + 
        imBuf->mImage->getHeight() / 2;
      
      // the rest is from below
      float pixMapY = imBuf->mImage->getHeight() - projY;
      point.x = (int)((projX - mXSrc) * mZoom + mXDest);
      point.y = rect.Height() - 1 - (int)((pixMapY- mYSrc) * mZoom + mYDest) +
        ZoomedPixelYAdjustment(&rect, imBuf->mImage);
      DrawCross(&cdc, &pnRedPen, point, crossLen);
    }
  }

  // Draw user point if one is set, otherwise draw line if that flag is set
  drawUserPt = imBuf->mHasUserPt || imBuf->mIllegalUserPt;
  if ((drawUserPt || imBuf->mHasUserLine) && !(skipExtra & 1)) {
    CPen pnSolidPen (PS_SOLID, thick2, RGB(0, 255, 0));
    CPoint point, point2;
    crossLen = DSB_DPI_SCALE(drawUserPt ? 7 : 1);
    MakeDrawPoint(&rect, imBuf->mImage, imBuf->mUserPtX, imBuf->mUserPtY, &point);
    if (!imBuf->mDrawUserBox) 
      DrawCross(&cdc, &pnSolidPen, point, crossLen);
    if (!drawUserPt) {
      MakeDrawPoint(&rect, imBuf->mImage, imBuf->mLineEndX, imBuf->mLineEndY, &point2);
      if (!imBuf->mDrawUserBox)
        DrawCross(&cdc, &pnSolidPen, point2, crossLen);
      CPen pnSolid1(PS_SOLID, thick1, RGB(0, 255, 0));
      CPen *pOldPen = cdc.SelectObject(&pnSolid1);
      cdc.MoveTo(point);
      if (imBuf->mDrawUserBox) {
        cdc.LineTo(point.x, point2.y);
        cdc.LineTo(point2);
        cdc.LineTo(point2.x, point.y);
        cdc.LineTo(point);
      } else {
        cdc.LineTo(point2);
      }
      cdc.SelectObject(pOldPen);
    }
  }

  // Draw letter in corner if this is main window, or number for multibuffer window
  int scaleCrit = mWinApp->mBufferManager->GetDrawScaleBar();
  if (!toBuffer && (mImBufNumber > 1 || mImBufArraySize)) {
    CFont *def_font = cdc.SelectObject(useFont);
    int defMode = cdc.SetBkMode(TRANSPARENT);
    COLORREF defColor = cdc.SetTextColor(RGB(255, 0, 0));
    if (mMainWindow || mFFTWindow) {
      char letter = 'A' + mImBufIndex;
      letString = letter;
      if (mFFTWindow)
        letString += "F";
      cdc.TextOut(scaled5, scaled5, letString);
      if (imBuf->mCaptured > 0 && imBuf->mConSetUsed >= 0 && 
        imBuf->mConSetUsed < NUMBER_OF_USER_CONSETS && imBuf->mLowDoseArea) {
          cdc.SelectObject(useLabelFont);
          letString = CString(" - ") + (mWinApp->GetModeNames())[imBuf->mConSetUsed];
          cdc.TextOut(mWinApp->ScaleValueForDPI(25), scaled10, letString);
      }
      if (!mFFTWindow && !mStackWindow && scaleCrit > 0 &&
        mImBufIndex <= mWinApp->mBufferManager->GetShiftsOnAcquire()) {
        cdc.SelectObject(useLabelFont);
        cdc.TextOut(scaled5, mWinApp->ScaleValueForDPI(35), "Rolling");
      }
 
      // Dose rate output for direct detector and channel name for STEM
      bufferOK = !imBuf->IsProcessed() && (imBuf->mCaptured > 0 || imBuf->ImageWasReadIn()
        ||imBuf->mCaptured == BUFFER_CALIBRATION || imBuf->mCaptured == BUFFER_TRACKING ||
        imBuf->mCaptured == BUFFER_MONTAGE_CENTER || imBuf->mCaptured == BUFFER_ANCHOR ||
        imBuf->mCaptured == BUFFER_MONTAGE_PIECE || 
        (imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW && imBuf->mMapID));
      if (mMainWindow && imBuf->mCamera >= 0 && scaleCrit > 0 && !mDoingMontSnapshot &&
        (bufferOK || mWinApp->GetNumActiveCameras() > 1)) {
          CameraParameters *camP = mWinApp->GetCamParams() + imBuf->mCamera;
          cdc.SetTextColor(RGB(0, 255, 40));
          cdc.SelectObject(useLabelFont);
          if (imBuf->mSampleMean > EXTRA_VALUE_TEST && bufferOK && !camP->STEMcamera &&
            !mWinApp->mProcessImage->DoseRateFromMean(imBuf, imBuf->mSampleMean, boost)) {
              if (mWinApp->mCamera->IsDirectDetector(camP)) {
                letString.Format("%.2f e/physpix/s at camera", boost);
                cdc.TextOut(scaled140, scaled10, letString);
              } else if (mWinApp->mScope->GetNoScope() && imBuf->mMagInd > 0) {
                letString.Format("%.3f e/sq A at camera", boost / 
                  pow(10000. * mShiftManager->GetPixelSize(imBuf), 2.));
                cdc.TextOut(scaled140, scaled10, letString);
              }
          }
          if (mWinApp->GetNumActiveCameras() > 1)
            cdc.TextOut(mWinApp->ScaleValueForDPI(420), scaled10, camP->name);
          if (bufferOK && camP->STEMcamera && camP->numChannels > 1) {
            EMimageExtra *extra = imBuf->mImage->GetUserData();
            if (extra && !extra->mChannelName.IsEmpty())
              cdc.TextOut(scaled140, scaled10, extra->mChannelName);
          }
          if (imBuf->mMapID && mWinApp->mMultiGridTasks->GetGridMapLabel(imBuf->mMapID,
            letString))
            cdc.TextOut(scaled140, mWinApp->ScaleValueForDPI(35), letString);

          // Image black-white and mean if available
          if (imBuf->mImageScale) {
            letString.Format("B - W: %s - %s",
              (LPCTSTR)FormattedNumber(imBuf->mImageScale->GetMinScale(), "", 0, 3, 
                1.99f),
              (LPCTSTR)FormattedNumber(imBuf->mImageScale->GetMaxScale(), "", 0, 3, 
                1.99f));
            if (imBuf->mSampleMean > EXTRA_VALUE_TEST) {
              firstLabel.Format("  mean: %s", (LPCTSTR)FormattedNumber(imBuf->mSampleMean,
                "", 0, 3, 1.99f));
              letString += firstLabel;
            }
            if (mWinApp->mCamera->IsDirectDetector(camP) && imBuf->mK2ReadMode >= 0)
              letString += imBuf->mK2ReadMode > 0 ? "  counted" : "  linear";
            cdc.TextOut(mWinApp->ScaleValueForDPI(25), 
              rect.Height() - mWinApp->ScaleValueForDPI(40), letString);
          }
      }
    } else {
      cdc.SelectObject(useLabelFont);
      if (mImBufs[mImBufIndex].mSecNumber < 0)
        letString.Format("%d: tilt = %.1f", mImBufIndex + 1, 
        mImBufs[mImBufIndex].mTiltAngleOrig);
      else
        letString.Format("%d: Z = %d", mImBufIndex + 1, mImBufs[mImBufIndex].mSecNumber);
      cdc.TextOut(scaled5, scaled5, letString);
    }
    cdc.SelectObject(def_font);
    cdc.SetTextColor(defColor);
    cdc.SetBkMode(defMode);
  }

  // Get navigator array now so that we can tell if there is an LD area draw
  // But if the image is an FFT, see if there are any points drawn on FFT
  if (navigator) {
    itemArray = GetMapItemsForImageCoords(imBuf, false);
    if (itemArray && bufIsFFT) {
      iy = 0;
      for (ix = 0; ix < (int)itemArray->GetSize(); ix++) {
        item = itemArray->GetAt(ix);
        if (item->mFlags & NAV_FLAG_DRAWN_ON_FFT) {
          iy++;
          break;
        }
      }

      // If there are no FFT points, set as no array so drawing etc gets skipped
      if (!iy) {
        itemArray = NULL;
        delete mAcquireBox;
        mAcquireBox = NULL;
      }
    }
  }
  mDrewLDAreasAtNavPt = itemArray && ((mAcquireBox && mAcquireBox->mNumPoints == 1) ||
    navigator->GetShowingLDareas()) && !bufIsFFT;

  if (mMainWindow) {

    // If this is a view image in low dose, draw record and trial/focus areas as long as
    // there won't be one around Nav point
    bufferOK = (imBuf->mCaptured > 0 || imBuf->ImageWasReadIn() || 
      imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW) &&
      (imBuf->mConSetUsed == VIEW_CONSET || imBuf->mConSetUsed == SEARCH_CONSET) &&
      imBuf->mLowDoseArea && !(skipExtra & 1);
    if (bufferOK && !mDrewLDAreasAtNavPt) {
      DrawLowDoseAreas(cdc, rect, imBuf, 0., 0., thick1, -2);
    }

    // Draw tilt axis if option set or when defining LD area on View
    if (((bufferOK && mWinApp->mLowDoseDlg.m_iDefineArea > 0) || 
      (mWinApp->mBufferManager->GetDrawTiltAxis() && 
      (imBuf->mCaptured > 0 || imBuf->ImageWasReadIn() || 
        imBuf->mCaptured == BUFFER_MONTAGE_OVERVIEW || 
        imBuf->mCaptured == BUFFER_MONTAGE_PRESCAN || 
        imBuf->mCaptured == BUFFER_MONTAGE_CENTER) && !(skipExtra & 1))) &&
      imBuf->mCamera >= 0 && (imBuf->mMagInd > 0 || 
      (imBuf->GetTimeStamp(ix) && imBuf->GetAxisAngle(tempY) && ix > 116846126 && 
        (!imBuf->ImageWasReadIn() || imBuf->mWrittenByVersion >= 40100)))) {
      tempX = (float)(0.75 * B3DMIN(rect.Width(), rect.Height()));
      CPoint point = rect.CenterPoint();
      if (imBuf->mMagInd > 0) {
        tempY = 0.;
        if (imBuf->mMagInd >= mWinApp->mScope->GetLowestMModeMagInd())
          mShiftManager->GetScaleAndRotationForFocus(imBuf, ptX, tempY);
        tempY += (float)mShiftManager->GetImageRotation(imBuf->mCamera,
          imBuf->mMagInd);
      }
      if (imBuf->mRotAngle)
        tempY += imBuf->mInverted ? -imBuf->mRotAngle : imBuf->mRotAngle;
      ptX = (float)cos(DTOR * tempY) * tempX;
      ptY = -(float)sin(DTOR * tempY) * tempX;
      CPen pnSolidPen(PS_SOLID, thick1, RGB(255, 255, 0));
      CPen *pOldPen = cdc.SelectObject(&pnSolidPen);

      // Make our own dashes
      point.x -= B3DNINT(0.48 * ptX);
      point.y -= B3DNINT(0.48 * ptY);;
      for (loop = 0; loop < 10; loop++) {
        cdc.MoveTo(point);
        point.x += B3DNINT(0.06 * ptX);
        point.y += B3DNINT(0.06 * ptY);
        cdc.LineTo(point);
        point.x += B3DNINT(0.04 * ptX);
        point.y += B3DNINT(0.04 * ptY);
      }
    }
  }

  // Draw rings at CTF zeros determined by ctffind
  if ((mMainWindow || mFFTWindow) && !(skipExtra & 1) && (imBuf->mCaptured == BUFFER_FFT
    || imBuf->mCaptured == BUFFER_LIVE_FFT && !mWinApp->mCamera->DoingContinuousAcquire())
    && imBuf->mCtfFocus1 > 0 && !mDoingMontSnapshot) {
      double defocus = imBuf->mCtfFocus1;
      float pixel = 1000.f * mShiftManager->GetPixelSize(imBuf);
      if (processImg->GetTestCtfPixelSize())
        pixel = processImg->GetTestCtfPixelSize();
      boost = processImg->GetDrawExtraCtfRings() > 0 ? imBuf->mMaxRingFreq : 0.f;
      processImg->DefocusFromPointAndZeros(0., 0, pixel, boost, &zeroRadii, defocus, 
        imBuf->mCtfPhase);
      defocus = imBuf->mCtfFocus2;
      processImg->DefocusFromPointAndZeros(0., 0, pixel, boost, &zeroRadii2, defocus, 
        imBuf->mCtfPhase);
      cenX = imBuf->mImage->getWidth() / 2.f;
      cenY = imBuf->mImage->getHeight() / 2.f;

      // Dashed lines can only be drawn with thickness 1: no DPI scaling
      CPen pnDashPen (PS_DASHDOT, 1, RGB(255, 255, 0));
      for (int zr = 0; zr < (int)B3DMIN(zeroRadii.size(), zeroRadii2.size()); zr++)
        DrawEllipse(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, 
        zeroRadii[zr] * cenX, zeroRadii2[zr] * cenX, imBuf->mCtfAngle, 
        zr >= processImg->GetNumFFTZeros());


  // Draw rings at zeros on an FFT if clicked and configured
  } else if ((mMainWindow || mFFTWindow) && !(skipExtra & 1) && !mDoingMontSnapshot &&
    processImg->GetFFTZeroRadiiAndDefocus(imBuf, &zeroRadii, defocus)) {
      cenX = imBuf->mImage->getWidth() / 2.f;
      cenY = imBuf->mImage->getHeight() / 2.f;

      // Dashes etc do not work above thickness 1, but don't even work with that when
      // drawing to a buffer
      CPen pnDashPen (PS_DASHDOT, 1, RGB(0, 255, 0));
      for (int zr = 0; zr < (int)zeroRadii.size(); zr++)
        DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, 
        zeroRadii[zr] * cenX);

  // Otherwise Draw circle on Live FFT
  } else if ((mMainWindow || mFFTWindow) && !(skipExtra & 1) && !mDoingMontSnapshot &&
    imBuf->mCaptured == BUFFER_LIVE_FFT && processImg->GetCircleOnLiveFFT()) {
      float *radii = processImg->GetFFTCircleRadii();
      float pixel = 1000.f * mShiftManager->GetPixelSize(imBuf);
      double defocus = processImg->GetFixedRingDefocus();
      cenX = imBuf->mImage->getWidth() / 2.f;
      cenY = imBuf->mImage->getHeight() / 2.f;
      CPen pnDashPen (PS_DASHDOT, 1, RGB(0, 255, 0));
      if (defocus > 0 && pixel > 0.) {
        processImg->DefocusFromPointAndZeros(0., 0, pixel, 0., &zeroRadii, 
          defocus);
        for (int zr = 0; zr < (int)zeroRadii.size(); zr++)
          DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, 
            zeroRadii[zr] * cenX);
      } else {
        for (int ir = 0; ir < processImg->GetNumCircles(); ir++)
          DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, radii[ir] * 
            cenX);
      }
  }

  // Scale bar if window big enough
  if (scaleCrit > 0 && rect.Width() >= scaleCrit && !(skipExtra & 4) &&
    imBuf->mCamera >= 0 && imBuf->mMagInd && imBuf->mBinning && (!imBuf->IsProcessed()
      || imBuf->mCaptured == BUFFER_PROCESSED) && mDoingMontSnapshot != 1 && 
    (imBuf->mCaptured != 0 || (!imBuf->mCaptured && imBuf->GetSaveCopyFlag() >= 0)) && 
    imBuf->mCaptured != BUFFER_STACK_IMAGE)
    DrawScaleBar(&cdc, &rect, imBuf, sizeScaling);

  // Draw coma vs IS cal locations
  if (imBuf->mCaptured != BUFFER_FFT && imBuf->mCaptured != BUFFER_LIVE_FFT && 
    mWinApp->mNavHelper->mComaVsISCalDlg && imBuf->mLowDoseArea &&
    mWinApp->LowDoseMode() && imBuf->mBinning > 0 && imBuf->mCamera >= 0 &&
    IS_SET_VIEW_OR_SEARCH(imBuf->mConSetUsed)) {
    ldp = mWinApp->GetLowDoseParams() + RECORD_CONSET;

    // What did not work: IS to specimen * specimen to stage * focus-adjusted stage to cam
    // But this does work much better:
    mShiftManager->GetScaleAndRotationForFocus(imBuf, scale, rotation);
    isToCamNoFoc = mShiftManager->IStoGivenCamera(imBuf->mMagInd,
      imBuf->mCamera);
    isToCam = mShiftManager->MatScaleRotate(isToCamNoFoc, scale, rotation);
    cenX = comaXcen = imBuf->mImage->getWidth() / 2.f;
    cenY = comaYcen = imBuf->mImage->getHeight() / 2.f;
    if (imBuf->mHasUserPt) {
      comaXcen = imBuf->mUserPtX;
      comaYcen = imBuf->mUserPtY;
    }
    boost = 0.;
    for (ix = 0; ix < 4; ix++) {
      mWinApp->mAutoTuning->GetComaVsISVector(ldp->magIndex,
        mWinApp->mNavHelper->mComaVsISCalDlg->m_fDistance,
        mWinApp->mNavHelper->mComaVsISCalDlg->m_iRotation, ix, tempX, tempY);
      mShiftManager->TransferGeneralIS(ldp->magIndex, tempX, tempY,
        imBuf->mMagInd, transX, transY);
      ApplyScaleMatrix(isToCam, (float)transX, (float)transY, 
        ptX, ptY);
      ptX /= (float)imBuf->mBinning;
      ptY /= (float)imBuf->mBinning;
      DrawLowDoseAreas(cdc, rect, imBuf, (comaXcen - cenX) + ptX, (comaYcen - cenY) - ptY,
        thick2, -1, ix);
      boost += sqrtf(ptX * ptX + ptY * ptY) / 4.f;
    }
    CPen pnSolidPen(PS_SOLID, thick2, RGB(0, 255, 0));
    DrawCircle(&cdc, &pnSolidPen, &rect, imBuf->mImage, comaXcen, comaYcen, boost);
  }

  // Various tests for skipping navigator display
  if (!itemArray || (skipExtra & 2) || (imBuf->GetUncroppedSize(ix, iy, &ierr) && 
    ix > 0 && ierr < 1)) {
    if (itemArray)
      delete mAcquireBox;
    return true;
  }

  // Determine stage coordinate limits of image.  This is IS-adjusted Nav stage positions
  aInv = MatInv(mAmat);
  for (ix = 0; ix <= 1; ix ++) {
    for (iy = 0; iy <= 1; iy ++) {
      ApplyScaleMatrix(aInv, ix * imageRect->getWidth() - mDelX,
        iy * imageRect->getHeight() - mDelY, tempX, tempY);
      ACCUM_MIN(minXstage, tempX);
      ACCUM_MIN(minYstage, tempY);
      ACCUM_MAX(maxXstage, tempX);
      ACCUM_MAX(maxYstage, tempY);
    }
  }

  // Get the stage limits of the image, see if the image goes outside them and if so draw
  // a red dashed rectangle
  minLimX = mWinApp->mScope->GetStageLimit(STAGE_MIN_X);
  minLimY = mWinApp->mScope->GetStageLimit(STAGE_MIN_Y);
  maxLimX = mWinApp->mScope->GetStageLimit(STAGE_MAX_X);
  maxLimY = mWinApp->mScope->GetStageLimit(STAGE_MAX_Y);
  if (!bufIsFFT && (minXstage < minLimX || maxXstage > maxLimX ||
    minYstage < minLimY || maxYstage > maxLimY)) {
    FloatVec limCornX, limCornY;
    CPen pnDashPen(PS_DASHDOT, 1, RGB(255, 0, 0));
    limCornX.push_back(minLimX);
    limCornY.push_back(minLimY);
    limCornX.push_back(maxLimX);
    limCornY.push_back(minLimY);
    limCornX.push_back(maxLimX);
    limCornY.push_back(maxLimY);
    limCornX.push_back(minLimX);
    limCornY.push_back(maxLimY);
    CPen *pOldPen = cdc.SelectObject(&pnDashPen);
    DrawVectorPolygon(cdc, &rect, NULL, imBuf, limCornX, limCornY, 0., 0., 0., 0., NULL, 
      NULL);
    cdc.SelectObject(pOldPen);
  }

  // Need the stage limits of the window instead for limiting drawing
  minXstage = 1.e30f, maxXstage = -1.e30f, minYstage = 1.e30f, maxYstage = -1.e30f;
  imXcenter = (float)(imageRect->getWidth() / 2. - m_iOffsetX);
  imYcenter = (float)(imageRect->getHeight() / 2. + m_iOffsetY);
  halfXwin = (float)(rect.Width() / (2 * mZoom));
  halfYwin = (float)(rect.Height() / (2 * mZoom));
  for (ix = -1; ix <= 1; ix += 2) {
    for (iy = -1; iy <= 1; iy += 2) {
      mWinApp->mShiftManager->ApplyScaleMatrix(aInv, imXcenter + ix * halfXwin - mDelX,
        imYcenter + iy * halfYwin - mDelY, tempX, tempY);
      ACCUM_MIN(minXstage, tempX);
      ACCUM_MIN(minYstage, tempY);
      ACCUM_MAX(maxXstage, tempX);
      ACCUM_MAX(maxYstage, tempY);
    }
  }

  tempX = 0.05f * (maxXstage - minXstage);
  tempY = 0.05f * (maxYstage - minYstage);
  minXstage -= tempX;
  maxXstage += tempX;
  minYstage -= tempY;
  maxYstage += tempY;
  mStageErrX = mStageErrY = 0.;

  // Draw hole finder points in two colors
  if (!bufIsFFT && mWinApp->mNavHelper->mHoleFinderDlg->GetHolePositions(&xHoleCens, 
    &yHoleCens, &pieceOn, &holeExcludes, drawIncluded, drawExcluded)) {
    CPen pnIncludePen(PS_SOLID, thick2, includeColor);
    CPen pnLowExclPen(PS_SOLID, thick2, lowExcludeColor);
    CPen pnHighExclPen(PS_SOLID, thick2, highExcludeColor);
    CPen pnUserExclPen(PS_SOLID, thick2, userExcludeColor);
    CPen pnUserInclPen(PS_SOLID, thick2, userIncludeColor);
    CPen *holePens[5] = {&pnUserInclPen, &pnIncludePen, &pnLowExclPen,
      &pnHighExclPen, &pnUserExclPen};
    short exclude;
    crossLen = DSB_DPI_SCALE(9);
    iy = (int)pieceOn->size();
    for (ix = 0; ix < (int)xHoleCens->size(); ix++) {
      tempX = xHoleCens->at(ix);
      tempY = yHoleCens->at(ix);
      exclude = holeExcludes->at(ix);
      if ((exclude > 0 && !drawExcluded) || (exclude <= 0 && !drawIncluded) ||
        exclude < -1 || exclude > 3 ||
        tempX < minXstage || tempX > maxXstage || tempY < minYstage || tempY > maxYstage)
        continue;
      StageToImage(imBuf, tempX, tempY, ptX, ptY, iy ? pieceOn->at(ix) : -1);
      MakeDrawPoint(&rect, imBuf->mImage, ptX, ptY, &point);
      DrawCross(&cdc, holePens[1 + exclude], point, crossLen);
    }
  }

  // Draw autocontour polygons
  if (!bufIsFFT)
    polyArray = mWinApp->mNavHelper->mAutoContouringDlg->GetPolyArrayToDraw(&polyGroups,
      &holeExcludes, numGroups, &showGroup);
  if (!bufIsFFT && polyArray) {
    for (group = 0; group < numGroups; group++) {
      if (!showGroup[group])
        continue;
      ix = group % MAX_AUTOCONT_GROUPS;
      if (mWinApp->mNavHelper->GetReverseAutocontColors())
        ix = (MAX_AUTOCONT_GROUPS - 1) - ix;
      CPen polyPen(PS_SOLID, thick2, CMapDrawItem::GetContourColor(ix));
      CPen *pOldPen = cdc.SelectObject(&polyPen);
      for (ix = 0; ix < (int)polyArray->GetSize(); ix++) {
        float delPtX = 0., delPtY = 0.;
        if (holeExcludes->at(ix) || polyGroups->at(ix) != group)
          continue;
        item = polyArray->GetAt(ix);
        if (!item)
          continue;
        adjSave = mAdjustPt;
        GetSingleAdjustmentForItem(imBuf, item, delPtX, delPtY);
        DrawMapItemBox(cdc, &rect, item, imBuf, item->mNumPoints, 0., 0., delPtX, delPtY,
          NULL, NULL);
        mAdjustPt = adjSave;
      }
      cdc.SelectObject(pOldPen);
    }
  }

  // Now draw navigator items
  float lastStageX, lastStageY, tiltAngle, acquireRadii[2], labelDistThresh = 40.;
  int lastGroupID = -1, lastGroupSize, size, numPoints, pieceDrawnOn;
  int regMatch = imBuf->mRegistration ? 
    imBuf->mRegistration : navigator->GetCurrentRegistration();
  std::set<int> *selectedItems = navigator->GetSelectedItems();
  bool highlight, draw, doInHole, drawSkips;
  CMapDrawItem holeItem;
  MultiShotParams *msParams;
  int msNumXholes = 0, msNumYholes = 0, useXholes, useYholes;
  int numFullSpecHoles, numSpecHoles, lastSpecialXholes = 0, lastSpecialYholes = 0;
  FloatVec specialFullISX, specialFullISY, delISX, delISY;
  IntVec fullHoleIndex, holeIndex;
  int currentIndex = navigator->GetCurrentOrAcquireItem(item);
  if (!navigator->GetAcquiring())
    item = navigator->GetSingleSelectedItem(&currentIndex);
  int currentGroup = (currentIndex >= 0 && item != NULL) ? item->mGroupID : -1;
  int groupThresh = mWinApp->mNavHelper->GetPointLabelDrawThresh();
  bool showCurPtAcquire = !imBuf->mHasUserPt && mAcquireBox;
  bool doMultiHole = mWinApp->mNavHelper->MultipleHolesAreSelected();
  bool checkUndos = mWinApp->mNavHelper->mCombineHoles->OKtoUndoCombine();
  BOOL useMultiShot = ((mWinApp->mNavHelper->GetEnableMultiShot() & 1) &&
    navigator->m_bShowAcquireArea) || mWinApp->mNavHelper->mMultiShotDlg;
  if (useMultiShot) {
    msParams = mWinApp->mNavHelper->GetMultiShotParams();
    doInHole = (msParams->inHoleOrMultiHole & MULTI_IN_HOLE) > 0;
    useMultiShot = doInHole || doMultiHole;
    if (doMultiHole)
      mWinApp->mNavHelper->GetNumHolesFromParam(msNumXholes, msNumYholes, ix);
  }
  bool showMultiOnAll = useMultiShot && (mWinApp->mNavHelper->GetEnableMultiShot() & 2) &&
    !(mDrewLDAreasAtNavPt && navigator->m_bShowAcquireArea);
  bool showOnlyCombined = mWinApp->mNavHelper->mMultiCombinerDlg &&
    !mWinApp->mNavHelper->GetMHCenableMultiDisplay();
  if (useMultiShot && !imBuf->GetTiltAngle(tiltAngle))
    tiltAngle = -999.;

  FloatVec drawnXinHole, drawnYinHole, drawnXallHole, drawnYallHole;
  FloatVec convXinHole, convYinHole, convXallHole, convYallHole;
  FloatVec skippedX, skippedY, acquireXhole, acquireYhole, convXacquire, convYacquire;
  ShortVec skippedDraw;
  float area, side, skipPad;
  CMapDrawItem skipItem, triangleItem;
  ScaleMat is2st;
  int holeMag = mWinApp->mNavigator->GetMagIndForHoles();

  for (int iDraw = -1; iDraw < itemArray->GetSize(); iDraw++) {
    adjSave = mAdjustPt;
    float delPtX = 0., delPtY = 0.;

    if (iDraw < 0) {
      if (!mAcquireBox || bufIsFFT)
        continue;
      item = mAcquireBox;
      thick = 1;
      highlight = false;
      numPoints = B3DMIN(5, item->mNumPoints);
    } else {
      item = itemArray->GetAt(iDraw);
      if (!item)
        continue;
      thick = (item->IsPoint()  || (item->IsPolygon() && item->mGroupID)) ? 3 : 2;
      highlight = selectedItems->count(iDraw) > 0;
      thick = highlight ? thick : 1;
      numPoints = item->mNumPoints;
    }
    thick = DSB_DPI_SCALE(thick);

    if (!item->mNumPoints || (!item->mDraw && iDraw >= 0) || 
      (item->mRegistration != regMatch && !mDrawAllReg && iDraw >= 0) || 
      !BOOL_EQUIV(bufIsFFT, (item->mFlags & NAV_FLAG_DRAWN_ON_FFT) != 0))
      continue;
    pieceDrawnOn = (imBuf->mMapID && item->mDrawnOnMapID == imBuf->mMapID) ?
      item->mPieceDrawnOn : -1;

    SetStageErrIfRealignedOnMap(imBuf, item);
    crossLen = DSB_DPI_SCALE(item->IsPoint() ? 9 : 5);
    CPen pnSolidPen(PS_SOLID, thick, item->GetColor(highlight));

    // Single point
    if (item->mNumPoints == 1) {
      if (item->mPtX[0] < minXstage || item->mPtX[0] > maxXstage ||
        item->mPtY[0] < minYstage || item->mPtY[0] > maxYstage)
        continue;
      if (mWinApp->mParticleTasks->ItemIsEmptyMultishot(item))
        continue;
      StageToImage(imBuf, item->mPtX[0], item->mPtY[0], ptX, ptY, pieceDrawnOn);

      // Draw low dose areas around current point
      if (mDrewLDAreasAtNavPt && (iDraw < 0 || 
        ((item->mAcquire || item->mTSparamIndex >= 0) && navigator->m_bShowAcquireArea))){
        cenX = imBuf->mImage->getWidth() / 2.f;
        cenY = imBuf->mImage->getHeight() / 2.f;
        DrawLowDoseAreas(cdc, rect, imBuf, ptX - cenX, ptY - cenY, thick1, iDraw);
        if (iDraw < 0) {
          mNavLDAreasXcenter = ptX;
          mNavLDAreasYcenter = ptY;
        }
      }

      // Otherwise draw a cross at a single point
      if (!mDrewLDAreasAtNavPt || iDraw >= 0) {
        MakeDrawPoint(&rect, imBuf->mImage, ptX, ptY, &point);
        DrawCross(&cdc, &pnSolidPen, point, crossLen);
      }
    } else {

      // For a map, find the adjust that applies to the center, turn off adjustment for
      // the corner points, and adjust them all by the center adjustment to get a square
      if (item->IsMap() || (iDraw < 0 && !useMultiShot && !imBuf->mHasUserPt) || 
        (item->IsPolygon() && item->mGroupID))
        GetSingleAdjustmentForItem(imBuf, item, delPtX, delPtY);

      // Draw lines if there is more than one point
      CPen *pOldPen = cdc.SelectObject(&pnSolidPen);
      if (!(iDraw < 0 && useMultiShot))
        DrawMapItemBox(cdc, &rect, item, imBuf, numPoints, 0., 0., delPtX, delPtY, NULL,
        NULL);
      if (item->IsMap() && item->mFitToPolygonID && item->mMapSection > 0){
        polygon = mWinApp->mNavigator->FindItemWithMapID(item->mFitToPolygonID, false);
        if (polygon && polygon->mDraw > 0) {
          CPen pnDashPen(PS_DASHDOT, 1, polygon->GetColor(false));
          cdc.SelectObject(&pnDashPen);
          DrawMapItemBox(cdc, &rect, polygon, imBuf, polygon->mNumPoints, 
            item->mStageX - polygon->mStageX, item->mStageY - polygon->mStageY,
            delPtX, delPtY, NULL, NULL);
          cdc.SelectObject(&pnSolidPen);
        }
      }

      // DRAW MULTI-SHOT PATTERN
      if (iDraw < 0 && useMultiShot && !mWinApp->mNavHelper->mComaVsISCalDlg) {
        float holeXoffset = 0, holeYoffset = 0;
        int inHoleEnd = item->mNumPoints - 2;
        int inHoleStart = inHoleEnd - B3DCHOICE(doInHole, msParams->numShots[0] + 
          (msParams->doSecondRing ? msParams->numShots[1] : 0) , 0);
        GetSingleAdjustmentForItem(imBuf, item, delPtX, delPtY);

        // If there is multishot in hole, draw them either centered or in the last hole
        if (doInHole) {
          holeXoffset = doMultiHole ? item->mPtX[inHoleStart - 1] : 0;
          holeYoffset = doMultiHole ? item->mPtY[inHoleStart - 1] : 0;

          // Draw rectangles in the off-center positions, then center if needed
          for (int pt = inHoleStart; pt < inHoleEnd; pt++)
            DrawMapItemBox(cdc, &rect, item, imBuf, numPoints, 
              item->mPtX[pt] - item->mStageX + holeXoffset, 
              item->mPtY[pt] - item->mStageY + holeYoffset, delPtX, delPtY, 
              &drawnXinHole, &drawnYinHole);
          if (msParams->doCenter)
            DrawMapItemBox(cdc, &rect, item, imBuf, numPoints, holeXoffset, holeYoffset,
              delPtX, delPtY, &drawnXinHole, &drawnYinHole);

          // Adjust the list of drawn points to be relative to their center after adding
          // absolute points to the list of multihole points
          for (int pt = 0; pt < (int)drawnXinHole.size(); pt++) {
            drawnXallHole.push_back(drawnXinHole[pt]);
            drawnYallHole.push_back(drawnYinHole[pt]);
            drawnXinHole[pt] -= holeXoffset;
            drawnYinHole[pt] -= holeYoffset;
          }

          // Get the convex boundary
          convXinHole.resize(drawnXinHole.size());
          convYinHole.resize(drawnYinHole.size());
          if (drawnXinHole.size() > 0) {
            convexBound(&drawnXinHole[0], &drawnYinHole[0], (int)drawnXinHole.size(), 0.,
              0., &convXinHole[0], &convYinHole[0], &size, &ptX, &ptY,
              (int)drawnXinHole.size());
            convXinHole.resize(B3DMAX(0, size));
            convYinHole.resize(B3DMAX(0, size));
          }
        }

        // If there are hole positions without multi-shot in hole, then need basic box
        // in each position with current pen
        // Hole positions are relative and multi in hole positions are absolute
        if (doMultiHole && !doInHole) {
          for (int pt = numPoints; pt < inHoleStart; pt++)
            DrawMapItemBox(cdc, &rect, item, imBuf, numPoints, item->mPtX[pt],
              item->mPtY[pt], delPtX, delPtY, &drawnXallHole, &drawnYallHole);
        } else if (doMultiHole) {
          for (int pt = numPoints; pt < inHoleStart - 1; pt++)
            DrawVectorPolygon(cdc, &rect, item, imBuf, convXinHole, convYinHole, 
            item->mPtX[pt], item->mPtY[pt], delPtX, delPtY, &drawnXallHole, 
            &drawnYallHole);
        }

        // Adjust all-hole vectors to item center and get the convex boundary
        for (int pt = 0; pt < (int)drawnXallHole.size(); pt++) {
          drawnXallHole[pt] -= item->mStageX;
          drawnYallHole[pt] -= item->mStageY;
        }

        // Get the convex boundary
        convXallHole.resize(drawnXallHole.size());
        convYallHole.resize(drawnYallHole.size());
        if (drawnXallHole.size() > 0) {
          convexBound(&drawnXallHole[0], &drawnYallHole[0], (int)drawnXallHole.size(), 0.,
            0., &convXallHole[0], &convYallHole[0], &size, &ptX, &ptY,
            (int)drawnXallHole.size());
          convXallHole.resize(B3DMAX(0, size));
          convYallHole.resize(B3DMAX(0, size));
        }

        // Setup to draw circles, switch color and draw them
        // Get radii of Record and multishot in hole
        StageToImage(imBuf, item->mStageX, item->mStageY, cenX, cenY);
        for (int pt = item->mNumPoints - 2; pt < item->mNumPoints; pt++) {
          StageToImage(imBuf, item->mPtX[pt], item->mPtY[pt], ptX, ptY);
          acquireRadii[pt + 2 - item->mNumPoints] = sqrtf((ptX - cenX) * (ptX - cenX) + 
            (ptY - cenY) * (ptY - cenY));
        }
        CPen circlePen(PS_SOLID, thick1, COLORREF(
          RGB(mWinApp->mParticleTasks->GetLastHolesWereAdjusted() ? 255 : 0, 255, 0)));

        // Now if doing multiholes, draw a circle on each that is either acquire or
        // multi inhole radius
        if (doMultiHole && item->mDraw) {
          size = inHoleStart;
          ix = numPoints;

          // Limit to 3 rings of circles, account for swapping of a point into the first
          // spot
          if (msParams->doHexArray && size - ix > 37) {
            ACCUM_MIN(size, numPoints + 37);
            ix++;
          }
          for (int pt = ix; pt < inHoleStart; pt++) {
            StageToImage(imBuf, item->mPtX[pt] + item->mStageX, 
              item->mPtY[pt] + item->mStageY, ptX, ptY);
            DrawCircle(&cdc, &circlePen, &rect, imBuf->mImage, ptX + delPtX, ptY + delPtY, 
              doInHole ? acquireRadii[1] : acquireRadii[0]);
            if (pt >= size - 1)
              pt = inHoleStart - 1;
          }
        }

        // Inside a hole, circle around each area
        if (doInHole && item->mDraw) {
          for (int pt = inHoleStart; pt < inHoleEnd; pt++) {
            StageToImage(imBuf, item->mPtX[pt] + holeXoffset, 
              item->mPtY[pt] + holeYoffset, ptX, ptY);
            DrawCircle(&cdc, &circlePen, &rect, imBuf->mImage, ptX + delPtX, ptY + delPtY,
              acquireRadii[0]);
          }
          StageToImage(imBuf, item->mStageX + holeXoffset, item->mStageY + holeYoffset,
            ptX, ptY);
          if (msParams->doCenter)
            DrawCircle(&cdc, &circlePen, &rect, imBuf->mImage, ptX + delPtX, ptY + delPtY,
              acquireRadii[0]);
        }
      }  // END OF MULTISHOT PATTERN
      cdc.SelectObject(pOldPen);
    }

    // Draw polygons for full acquire area on all acquire points if selected, or on
    // current/selected points
    if (iDraw >= 0 && item->mAcquire && item->mNumPoints == 1 && mAcquireBox &&
      (showMultiOnAll &&
      (!showOnlyCombined || (item->mNumXholes != 0 && item->mNumYholes != 0) ||
        (checkUndos && 
          mWinApp->mNavHelper->mCombineHoles->IsItemInUndoList(item->mMapID))) ||
        (showCurPtAcquire && highlight && doMultiHole))) {
      GetSingleAdjustmentForItem(imBuf, item, delPtX, delPtY);
      CPen pnAcquire(PS_SOLID, thick1, item->GetColor(highlight));
      CPen *pOldPen = cdc.SelectObject(&pnAcquire);
      mAcquireBox->mDraw = true;
      if (useMultiShot) {

        // One-time computation of the triangle area for skipped points
        if (!triangleItem.mNumPoints) {
          area = 0.2f;
          if (doInHole && (int)convXinHole.size() > 2)
            area = 0.67f * navigator->ContourArea(&convXinHole[0], &convYinHole[0],
            (int)convXinHole.size());
          else if (mAcquireBox && mAcquireBox->mNumPoints > 2)
            area = navigator->ContourArea(mAcquireBox->mPtX, mAcquireBox->mPtY,
              B3DMIN(5, mAcquireBox->mNumPoints));
          skipPad = 0.5f * sqrtf(area);
          tempX = sqrtf(3.f) / 2.f;
          side = sqrtf(area / tempX);
          is2st = MatMul(mShiftManager->IStoCamera(holeMag),
            MatInv(mShiftManager->StageToCamera(mWinApp->GetCurrentCamera(), holeMag)));

          // prefereably, base it all on the interhole distance
          if (is2st.xpx && holeMag) {
            size = (item->mNumXholes > 0 && item->mNumYholes == -1) ? 1 : 0;
            mShiftManager->ApplyScaleMatrix(is2st,
              size ? msParams->hexISXspacing[0] : msParams->holeISXspacing[0],
              size ? msParams->hexISXspacing[1] : msParams->holeISXspacing[1], ptX, ptY);
            skipPad = 0.5f * sqrtf(ptX * ptX + ptY * ptY);
            side = 1.25f * skipPad;
          }
          cenX = side / 2.f;
          cenY = side * tempX / 3.f;
          triangleItem.AppendPoint(-cenX, -cenY);
          triangleItem.AppendPoint(side - cenX, -cenY);
          triangleItem.AppendPoint(side / 2.f - cenX, side * tempX - cenY);
          triangleItem.AppendPoint(-cenX, -cenY);
        }

        // If this point has special hole pattern and does not match the default from the
        // params, need to get specific list of holes for it
        numSpecHoles = 0;
        drawSkips = false;
        useXholes = (doMultiHole && item->mNumXholes) ? item->mNumXholes :
          msNumXholes;
        useYholes = (doMultiHole && item->mNumYholes) ? item->mNumYholes :
          msNumYholes;
        if (useXholes && useYholes && (useXholes != msNumXholes ||
          useYholes != msNumYholes || item->mNumSkipHoles || highlight)) {

          // Get full hole list if this doesn't match last one used
          if (useXholes != lastSpecialXholes ||
            useYholes != lastSpecialYholes) {
            numFullSpecHoles = mWinApp->mParticleTasks->GetHolePositions(specialFullISX,
              specialFullISY, fullHoleIndex, mWinApp->mNavigator->GetMagIndForHoles(),
              mWinApp->mNavigator->GetCameraForHoles(), useXholes,
              useYholes, tiltAngle, false);
            lastSpecialXholes = useXholes;
            lastSpecialYholes = useYholes;
          }

          // Reduce it by any skipped ones
          numSpecHoles = numFullSpecHoles;
          delISX = specialFullISX;
          delISY = specialFullISY;
          holeIndex = fullHoleIndex;

          // Keep track of skipped holes 
          acquireXhole.clear();
          acquireYhole.clear();
          skippedX.clear();
          skippedY.clear();
          if (item->mNumSkipHoles) {
            mWinApp->mParticleTasks->SkipHolesInList(delISX, delISY, holeIndex,
              item->mSkipHolePos, item->mNumSkipHoles, numSpecHoles, &skippedX,
              &skippedY);
          }
          holeItem.mNumPoints = 0;
          mWinApp->mNavigator->AddHolePositionsToItemPts(delISX, delISY, holeIndex, false,
            numSpecHoles, &holeItem);

          // Put skipped holes into an item too
          skipItem.mNumPoints = 0;
          if (skippedX.size())
            mWinApp->mNavigator->AddHolePositionsToItemPts(skippedX, skippedY,
              holeIndex, false, -(int)skippedX.size(), &skipItem);
        }
        if (numSpecHoles) {

          // Draw the special holes
          mAcquireBox->mDraw = true;
          if (highlight && saveCurHolePos)
            *curHoleIndex = holeIndex;
          for (int hole = 0; hole < numSpecHoles; hole++) {

            // First get their actual coordinates for saving current pos and pos of ones
            // not being drawn because of boundary display
            ptX = item->mStageX + holeItem.mPtX[hole];
            ptY = item->mStageY + holeItem.mPtY[hole];
            if (highlight && saveCurHolePos) {
              curHoleXYpos->push_back(item->mStageX + holeItem.mPtX[hole]);
              curHoleXYpos->push_back(item->mStageY + holeItem.mPtY[hole]);
            }
            acquireXhole.push_back(ptX);
            acquireYhole.push_back(ptY);
          }

          if (!highlight) {

            // Get the convex boundary of the ones being acquired
            convXacquire.resize(numSpecHoles);
            convYacquire.resize(numSpecHoles);
            convexBound(&acquireXhole[0], &acquireYhole[0], numSpecHoles, 0., skipPad,
              &convXacquire[0], &convYacquire[0], &size, &ptX, &ptY, numSpecHoles);
            convXacquire.resize(size);
            convYacquire.resize(size);
            skippedDraw.resize(skipItem.mNumPoints, 0);

            // Count up number actually to be drawn
            ix = 0;
            for (int hole = 0; hole < skipItem.mNumPoints; hole++) {
              ptX = item->mStageX + skipItem.mPtX[hole];
              ptY = item->mStageY + skipItem.mPtY[hole];
              if (InsideContour(&convXacquire[0], &convYacquire[0], size, ptX, ptY)) {
                skippedDraw[hole] = 1;
                ix++;
              }
            }
            drawSkips = ix < numSpecHoles / 2 && size > 2;
          }

          if (!drawSkips) {

            // Subtract acquire box center so it works for drawing in-hole points and 
            // item box using acquire box
            for (int hole = 0; hole < numSpecHoles; hole++) {
              ptX = acquireXhole[hole] - mAcquireBox->mStageX;
              ptY = acquireYhole[hole] - mAcquireBox->mStageY;
              if (doInHole) {
                DrawVectorPolygon(cdc, &rect, item, imBuf, convXinHole, convYinHole, ptX,
                  ptY, delPtX, delPtY, NULL, NULL);
              } else {
                DrawMapItemBox(cdc, &rect, mAcquireBox, imBuf,
                  B3DMIN(5, mAcquireBox->mNumPoints), ptX, ptY, delPtX, delPtY, NULL, 
                  NULL);
              }
            }
          }
        }

        // Draw the full convex bound if it is a standard pattern with nothing skipped
        if ((!numSpecHoles || numSpecHoles == numFullSpecHoles) &&
          useXholes == msNumXholes && useYholes == msNumYholes) {
          DrawVectorPolygon(cdc, &rect, item, imBuf, convXallHole, convYallHole,
            item->mStageX, item->mStageY, delPtX, delPtY, NULL, NULL);
        } else {

          // Otherwise,  the convex boundary of the ones being acquired
          if (size > 2) 
            DrawVectorPolygon(cdc, &rect, item, imBuf, convXacquire, convYacquire,
              0., 0., delPtX, delPtY, NULL, NULL);

          // Draw skipped points 
          if (drawSkips) {
            CPen pnSkipped(PS_SOLID, thick1, RGB(0, 255, 255));
            cdc.SelectObject(&pnSkipped);
            for (int hole = 0; hole < skipItem.mNumPoints; hole++) {
              ptX = item->mStageX + skipItem.mPtX[hole];
              ptY = item->mStageY + skipItem.mPtY[hole];
              if (!size || InsideContour(&convXacquire[0], &convYacquire[0], size, ptX, ptY))
                DrawMapItemBox(cdc, &rect, &triangleItem, imBuf, 4, ptX, ptY, delPtX,
                  delPtY, NULL, NULL);
            }
          }
        }
      } else {

        // if not multishot, draw acquire box
        DrawMapItemBox(cdc, &rect, mAcquireBox, imBuf,
          B3DMIN(5, mAcquireBox->mNumPoints), item->mStageX - mAcquireBox->mStageX,
          item->mStageY - mAcquireBox->mStageY, delPtX, delPtY, NULL, NULL);
        cdc.SelectObject(pOldPen);
      }
      cdc.SelectObject(pOldPen);
    }
    mAdjustPt = adjSave;

    // Draw the label if any
    if (iDraw >= 0 && !item->mLabel.IsEmpty() && navigator->m_bDrawLabels) {

      // The rule is to draw first and last in a group and up to 5 around the current
      // point if the group is above the threshold size
      draw = true;
      if (groupThresh > 0 && !item->IsMap() && (iDraw < currentIndex - 2
        || iDraw > currentIndex + 2 || item->mGroupID != currentGroup) && 
        item->mGroupID > 0) {
        if (item->mGroupID == lastGroupID) {
          size = lastGroupSize;
        } else {
          size = lastGroupSize = navigator->CountItemsInGroup(item->mGroupID,
            firstLabel, lastLabel, ierr);
          lastGroupID = item->mGroupID;
        }
        draw = size <= groupThresh || item->mLabel == firstLabel || 
          item->mLabel == lastLabel || (iDraw > 0 && item->IsPoint() &&
            sqrt(pow((double)lastStageX - item->mStageX, 2.) + 
          pow((double)lastStageY - item->mStageY, 2.)) > labelDistThresh);
      }
      if (draw) {
        StageToImage(imBuf, item->mStageX, item->mStageY, ptX, ptY, pieceDrawnOn);
        MakeDrawPoint(&rect, imBuf->mImage, ptX, ptY, &point);
        if (item->mNumPoints == 1)
          point = point + CPoint(10, 0);
        CFont *def_font = cdc.SelectObject(useLabelFont);
        int defMode = cdc.SetBkMode(TRANSPARENT);
        COLORREF defColor = cdc.SetTextColor(item->GetColor(highlight));
        cdc.TextOut(point.x, point.y, item->mLabel);
        cdc.SelectObject(def_font);
        cdc.SetTextColor(defColor);
        cdc.SetBkMode(defMode);
      }
      lastStageX = item->mStageX;
      lastStageY = item->mStageY;
    }
  }
  delete mAcquireBox;

  return true;
}// end of  CSerialEMView::DrawImage


// For drawing a map rectangle or other shape that should not be distorted by montage 
// adjusts at each point, get the adjustment, return it, and turn off further adjustments
// for this item
void CSerialEMView::GetSingleAdjustmentForItem(EMimageBuffer *imBuf, CMapDrawItem *item, 
  float &delPtX, float &delPtY)
{
  float ptX, ptY;
  int drawnOn = (imBuf->mMapID && item->mDrawnOnMapID == imBuf->mMapID) ?
    item->mPieceDrawnOn : -1;
  StageToImage(imBuf, item->mStageX, item->mStageY, delPtX, delPtY, drawnOn);
  mAdjustPt = -1;
  StageToImage(imBuf, item->mStageX, item->mStageY, ptX, ptY, drawnOn);
  delPtX -= ptX;
  delPtY -= ptY;
}

// Draw a box for a map item with a given stage offset from the item points and with a
// common montage adjustment, potentially saving the points in vectors
void CSerialEMView::DrawMapItemBox(CDC &cdc, CRect *rect, CMapDrawItem *item, 
  EMimageBuffer *imBuf, int numPoints, float delXstage, float delYstage, float delPtX, 
  float delPtY, FloatVec *drawnX, FloatVec *drawnY)
{
  CPoint point;
  float stX, stY, ptX, ptY;
  for (int bpt = 0; bpt < numPoints; bpt++) {
    stX = item->mPtX[bpt] + delXstage;
    stY = item->mPtY[bpt] + delYstage;
    if (drawnX && drawnY) {
      drawnX->push_back(stX);
      drawnY->push_back(stY);
    }
    if (item->mDraw) {
      StageToImage(imBuf, stX, stY, ptX, ptY);
      MakeDrawPoint(rect, imBuf->mImage, ptX + delPtX, ptY + delPtY, &point);
      if (bpt)
        cdc.LineTo(point);
      else
        cdc.MoveTo(point);
    }
  }
}

// Draw a polygon from points in a vector, with the given adjustment to the stage
// positions, a common montage adjustment, and potentially storing in more vectors
void CSerialEMView::DrawVectorPolygon(CDC &cdc, CRect *rect, CMapDrawItem *item,
  EMimageBuffer *imBuf, FloatVec &convX, FloatVec &convY, float delXstage,float delYstage, 
  float delPtX, float delPtY, FloatVec *drawnX, FloatVec *drawnY)
{
  CPoint point;
  float stX, stY, ptX, ptY;
  int numPoints = (int)convX.size();
  if (!numPoints)
    return;
  for (int bpt = 0; bpt <= numPoints; bpt++) {
    stX = convX[bpt % numPoints] + delXstage;
    stY = convY[bpt % numPoints] + delYstage;
    if (drawnX && drawnY) {
      drawnX->push_back(stX);
      drawnY->push_back(stY);
    }
    if (!item || item->mDraw) {
      StageToImage(imBuf, stX, stY, ptX, ptY);
      MakeDrawPoint(rect, imBuf->mImage, ptX + delPtX, ptY + delPtY, &point);
      if (bpt)
        cdc.LineTo(point);
      else
        cdc.MoveTo(point);
    }
  }
}

void CSerialEMView::MakeDrawPoint(CRect *rect, KImage *image, float inX, float inY, 
                                  CPoint *point, bool skipShift)
{
  float shiftX = 0., shiftY = 0.;
  if (!skipShift)
    image->getShifts(shiftX, shiftY);

  // Need to convert from image coordinates to pixmap coordinate, hence add shifts
  // Need to invert the Y pixmap coordinate, apply equation, then invert resulting
  // Y window coordinate
  float pixMapY = image->getHeight() - (inY + shiftY);
  point->x = (int)((inX + shiftX - mXSrc) * mZoom + mXDest);
  point->y = rect->Height() - 1 - (int)((pixMapY- mYSrc) * mZoom + mYDest) +
    ZoomedPixelYAdjustment(rect, image);
}

// Draw Record and area being defined for low dose
void CSerialEMView::DrawLowDoseAreas(CDC &cdc, CRect &rect, EMimageBuffer *imBuf, 
  float xOffset, float yOffset, int thick, int curInd, int recOnly)
{
  COLORREF areaColors[6] = {RGB(255, 255, 0), RGB(255, 0, 0), RGB(0, 255, 0), 
    RGB(255, 255, 0), RGB(255, 128, 0), RGB(255, 0, 0)};
  float cornerX[4], cornerY[4], cenX, cenY, radius;
  CPoint point;
  StateParams state;
  int newInd;
  bool useDash = mDrewLDAreasAtNavPt && recOnly < 0;
  state.camIndex = mWinApp->GetCurrentCamera();
  if (mWinApp->mNavigator && mWinApp->mNavigator->GetSingleSelectedItem(&newInd) && 
    (curInd == -1 || curInd == newInd) && recOnly < 0) {
    curInd = newInd;
    useDash = false;
  }
  if (recOnly < 0)
    mWinApp->mNavHelper->FindFocusPosForCurrentItem(state, !mDrewLDAreasAtNavPt,
      imBuf->mRegistration, curInd);
  for (int type = (recOnly >= 0 ? 1 : 0); type < 2; type++) {
    int area = mWinApp->mLowDoseDlg.DrawAreaOnView(type + (recOnly >= 0 ? 1 : 0), imBuf,
      state, cornerX, cornerY, cenX, cenY, radius);
    if (area) {
      CPen pnSolidPen(useDash ? PS_DASHDOT : PS_SOLID, mDrewLDAreasAtNavPt ? 1 : thick, 
        areaColors[(recOnly >= 0 ? recOnly : 0) + area - 1]);
      CPen *pOldPen = cdc.SelectObject(&pnSolidPen);
      for (int pt = 0; pt < 5; pt++) {
        MakeDrawPoint(&rect, imBuf->mImage, cornerX[pt % 4] + xOffset, 
          cornerY[pt % 4] + yOffset, &point, true);
        if (pt)
          cdc.LineTo(point);
        else
          cdc.MoveTo(point);
      }
      cdc.SelectObject(pOldPen);

      // Draw circle around center for area being defined
      if ((!type || recOnly >= 0) && !mWinApp->GetSTEMMode())
        DrawCircle(&cdc, &pnSolidPen, &rect, imBuf->mImage, cenX + xOffset, 
          cenY + yOffset, radius, true);
    }
  }
}

// Since coordinates are referenced to the bottom of the image when computing their
// position in zoomed image, this adjustment is needed to account for fact that full
// pixels are drawn at top of image.  In image smaller than the window, when the border
// is odd the bigger part is at the top not the bottom
int CSerialEMView::ZoomedPixelYAdjustment(CRect *rect, KImage *image)
{
  float pixInWin = (float)(rect->Height() / mZoom);
  if (pixInWin < image->getHeight())
    return (int)(mZoom * (1. - (pixInWin - (int)pixInWin)));
  return -((rect->Height() - (int)(mZoom * image->getHeight())) % 2);
}

// Call to set up the use of StageToImage to get image coordinates of nav points
MapItemArray *CSerialEMView::GetMapItemsForImageCoords(
  EMimageBuffer *imBuf, bool deleteAcqBox)
{
  MapItemArray *itemArray = mWinApp->mNavigator->GetMapDrawItems
    (imBuf, mAmat, mDelX, mDelY, mDrawAllReg, &mAcquireBox);
  if (deleteAcqBox)
    delete mAcquireBox;
  if (!itemArray)
    return NULL;
  mAdjustPt = mWinApp->mNavHelper->PrepareMontAdjustments(imBuf, mRmat, mRinv, mRdelX,
    mRdelY);
  return itemArray;
}

// Set the stage error if this item has been aligned on the given map
void CSerialEMView::SetStageErrIfRealignedOnMap(EMimageBuffer *imBuf, CMapDrawItem *item)
{
  mStageErrX = mStageErrY = 0.;
  if (imBuf->mMapID && (imBuf->mMapID == item->mRealignedID)) {
    mStageErrX = item->mRealignErrX;
    mStageErrY = item->mRealignErrY;
  }
}

// External call to get image coordinates after setting it up with call above
void CSerialEMView::GetItemImageCoords(EMimageBuffer *imBuf, CMapDrawItem *item, 
                                       float &outX, float &outY, int pcInd)
{
  SetStageErrIfRealignedOnMap(imBuf, item);
  StageToImage(imBuf, item->mStageX, item->mStageY, outX, outY, pcInd);
}

// Transform stage position to image position, with optional adjustment in montage
void CSerialEMView::StageToImage(EMimageBuffer *imBuf, float inX, float inY, float &outX,
                                 float &outY, int pcInd, float *pcCenDist)
{
  MiniOffsets *mini = imBuf->mMiniOffsets;
  float testX, testY, delX, delY;
  int ixPiece, iyPiece, index, ind2, ix, iy, minDist, dist;
  outX = mAmat.xpx * (inX - mStageErrX) + mAmat.xpy * (inY - mStageErrY) + mDelX;
  outY = mAmat.ypx * (inX - mStageErrX) + mAmat.ypy * (inY - mStageErrY) + mDelY;
  if (mAdjustPt < 0) {

    // Optional return of distance from piece or image center
    if (pcCenDist)
      *pcCenDist = sqrtf(powf(outX - (float)imBuf->mImage->getWidth() / 2.f, 2.f) +
        powf(outY - (float)imBuf->mImage->getHeight() / 2.f, 2.f));
    return;
  }
  testX = outX;
  testY = outY;

  // Back rotate position to original image
  if (mAdjustPt) {
    testX = mRinv.xpx * (outX - mRdelX) + mRinv.xpy * (outY - mRdelY);
    testY = mRinv.ypx * (outX - mRdelX) + mRinv.ypy * (outY - mRdelY);
  }

  // The incoming coordinates are inverted in Y.  These montage piece indices are inverted
  // in Y from the normal right-hand numbering

  // Use piece drawn on if one is supplied; it is in right-handed numbering
  if (pcInd >= 0 && pcInd < (int)mini->offsetX.size() && !mini->subsetLoaded) {
    index = pcInd;
    ixPiece = pcInd / mini->yNframes;
    iyPiece = mini->yNframes - 1 - pcInd % mini->yNframes;
  } else {

    // Or look up the piece it is in
    ixPiece = (int)(testX - mini->xBase) / mini->xDelta;
    B3DCLAMP(ixPiece, 0, mini->xNframes - 1);
    iyPiece = (int)(testY - mini->yBase) / mini->yDelta;
    B3DCLAMP(iyPiece, 0, mini->yNframes - 1);
    index = (ixPiece + 1) * mini->yNframes - 1 - iyPiece;
  }
  if (mini->offsetX[index] == MINI_NO_PIECE) {

    // If necessary, find closest existing piece
    minDist = 10000000;
    for (ix = 0; ix < mini->xNframes; ix++) {
      for (iy = 0; iy < mini->yNframes; iy++) {
        ind2 = (ix + 1) * mini->yNframes - 1 - iy;
        if (mini->offsetX[ind2] == MINI_NO_PIECE)
          continue;
        dist = (ix - ixPiece) * (ix - ixPiece) + (iy - iyPiece) * (iy - iyPiece);
        if (dist < minDist) {
          minDist = dist;
          index = ind2;
        }
      }
    }
    if (minDist >= 1000000)
      return;
  } 

  // Compute center distance before offsetting and rotating back
  if (pcCenDist) {
    ixPiece = index / mini->yNframes;
    iyPiece = mini->yNframes - 1 - index % mini->yNframes;
    delX = testX - (float)(ixPiece * mini->xDelta + mini->xBase + mini->xDelta / 2.);
    delY = testY - (float)(iyPiece * mini->yDelta + mini->yBase + mini->yDelta / 2.);
    *pcCenDist = sqrtf(delX * delX + delY * delY);
  }

  // Offset the point, rotate it back
  testX += mini->offsetX[index];
  testY += mini->offsetY[index];
  outX = testX;
  outY = testY;

  if (mAdjustPt) {
    outX = mRmat.xpx * testX + mRmat.xpy * testY + mRdelX;
    outY = mRmat.ypx * testX + mRmat.ypy * testY + mRdelY;
  }
}

// External call for one conversion needs to be supplied with the transformation, which
// is stored, then it sets up montage adjustments and can call StageToImage
void CSerialEMView::ExternalStageToImage(EMimageBuffer *imBuf, ScaleMat &aMat, 
  float delX, float delY, float inX, float inY, float &outX, float &outY, 
  float *pcCenDist)
{
  mStageErrX = mStageErrY = 0.;
  mAmat = aMat;
  mDelX = delX;
  mDelY = delY;
  mAdjustPt = mWinApp->mNavHelper->PrepareMontAdjustments(imBuf, mRmat, mRinv, mRdelX,
    mRdelY);
  StageToImage(imBuf, inX, inY, outX, outY, -1, pcCenDist);
}

// Draw a cross with the given pen and length
void CSerialEMView::DrawCross(CDC *cdc, CPen *pNewPen, CPoint point, int crossLen)
{
  CPen *pOldPen = cdc->SelectObject(pNewPen);
  point.x -= crossLen;
  cdc->MoveTo(point);
  point.x += 2 * crossLen;
  cdc->LineTo(point);
  point.x -= crossLen;
  point.y -= crossLen;
  cdc->MoveTo(point);
  point.y += 2 * crossLen;
  cdc->LineTo(point);
  cdc->SelectObject(pOldPen);
}

// Draw a circle with the given pen
void CSerialEMView::DrawCircle(CDC *cdc, CPen *pNewPen, CRect *rect, KImage *image,
                               float cenX, float cenY, float radius, bool skipShift)
{
  CPoint point;
  float ptX, ptY;
  CPen *pOldPen;
  if (pNewPen)
    pOldPen = cdc->SelectObject(pNewPen);
  for (int pt = 0; pt < 91; pt++) {
    ptX = (float)(cenX + radius * cos(DTOR * pt * 4));
    ptY = (float)(cenY + radius * sin(DTOR * pt * 4));
    MakeDrawPoint(rect, image, ptX, ptY, &point, skipShift);
    if (pt)
      cdc->LineTo(point);
    else
      cdc->MoveTo(point);
  }

  if (pNewPen)
    cdc->SelectObject(pOldPen);
}

// Draw an ellipse with the given pen, given two radii and the right-handed angle of
// radius 1 (i.e., take negative of angle to draw in inverted coordinates)
void CSerialEMView::DrawEllipse(CDC *cdc, CPen *pNewPen, CRect *rect, KImage *image,
  float cenX, float cenY, float radius1, float radius2, float angle, bool drawHalf)
{
  CPoint point;
  float ptX, ptY, xOnAx, yOnAx;
  CPen *pOldPen = cdc->SelectObject(pNewPen);
  float sinAng = (float)sin(-DTOR * angle);
  float cosAng = (float)cos(-DTOR * angle);
  for (int pt = 0; pt < (drawHalf ? 46 : 91); pt++) {
    xOnAx = (float)(radius1 * cos(DTOR * (pt * 4 + angle - 90.)));
    yOnAx = (float)(radius2 * sin(DTOR * (pt * 4 + angle - 90.)));
    ptX = cenX + xOnAx * cosAng - yOnAx * sinAng;
    ptY = cenY + xOnAx * sinAng + yOnAx * cosAng;
    MakeDrawPoint(rect, image, ptX, ptY, &point);
    if (pt)
      cdc->LineTo(point);
    else
      cdc->MoveTo(point);
  }

  cdc->SelectObject(pOldPen);
}

// Draw a scale bar in appropriate images (code doctored from 3dmod)
void CSerialEMView::DrawScaleBar(CDC *cdc, CRect *rect, EMimageBuffer *imBuf,
  float sizeScaling)
{
  double expon, minlen, loglen, normlen, custlen, pixsize;
  float truelen;
  int xst, yst, pixlen, xsize, ysize;
  int winx = rect->Width();
  int winy = rect->Height();
  CString label;

  if (!mScaleParams.draw)
    return;
  pixsize = mShiftManager->GetPixelSize(imBuf);

  // Get minimum length in units, then reduce that to number between 0 and 1.   
  minlen = pixsize * mScaleParams.minLength / mZoom;
  loglen = log10(minlen);
  expon = floor(loglen);
  normlen = pow(10., loglen - expon);

  // If a custom length is specified, just use it; adjust by 10 either way      
  if (mScaleParams.useCustom) {
    custlen = mScaleParams.customVal / 10.;
    if (custlen < normlen)
      custlen *= 10.;
    if (custlen >= 10. * normlen)
      custlen /= 10.;
    normlen = custlen;
  } else {

    // Otherwise set to next higher standard number                             
    if (normlen < 2.)
      normlen = 2.;
    else if (normlen < 5.)
      normlen = 5.;
    else
      normlen = 10.;
  }

  // Get real length then pixel length, starting points                         
  truelen = (float)(normlen * pow(10., expon));
  pixlen = B3DNINT(truelen * mZoom / pixsize);
  xsize = mScaleParams.vertical ? mScaleParams.thickness : pixlen;
  ysize = mScaleParams.vertical ? pixlen : mScaleParams.thickness;
  xst = mScaleParams.indentX;
  if (mScaleParams.position == 0 || mScaleParams.position == 3)
    xst = winx - mScaleParams.indentX - xsize;
  yst = winy - mScaleParams.indentY;
  if (mScaleParams.position == 2 || mScaleParams.position == 3)
    yst = winy - mScaleParams.indentY - ysize;

  cdc->FillSolidRect(xst, yst, xsize, ysize, RGB(255,255,252));
  cdc->FillSolidRect(xst, yst + ysize, xsize, ysize, RGB(0, 0, 0));

  if (truelen < 1.)
    label.Format("%g nm", truelen * 1000.);
  else
    label.Format("%g um", truelen);
  CFont *def_font = cdc->SelectObject(sizeScaling > 1. ? mScaledFont : &mFont);
  int defMode = cdc->SetBkMode(TRANSPARENT);
  COLORREF defColor = cdc->SetTextColor(RGB(0, 0, 0));
  CSize labsize = cdc->GetTextExtent(label);

  //cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 4, yst - ysize - labsize.cy, label);
  cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - DSB_DPI_SCALE(10),
    yst - ysize - labsize.cy - DSB_DPI_SCALE(4), label);
  cdc->SetTextColor(RGB(255, 255, 255));
  //cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 1, yst - ysize - labsize.cy - 3, label);
  cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - DSB_DPI_SCALE(8),
    yst - ysize - labsize.cy - DSB_DPI_SCALE(6), label);
  cdc->SelectObject(def_font);
  cdc->SetTextColor(defColor);
  cdc->SetBkMode(defMode);
}

// Adjust image offset to be in range and set the offsets for drawing on one axis
void CSerialEMView::b3dSetImageOffset(int winsize,     /* window size in wpixels.          */
                       int imsize,      /* image size in ipixels            */
                       double zoom,     /* zoom factor.                     */
                       int *drawsize,   /* size drawn in ipixels            */
                       int *offset,     /* offset from center in ipixels.   */
                       int *woff,       /* window offset in wpixels.        */
                       int *doff)       /* data offset in ipixels           */
{

  /* Fits completely inside of window. */
  if ( (imsize * zoom) < winsize ){
    *drawsize = imsize;
    *woff     = (int)(( winsize - (imsize * zoom)) / 2);
    *doff     = 0;
    *offset   = 0;  // NEW: eliminate the shift if it fits
    
  }else{
    /* Draw sub image. */
    *woff = 0;
    *drawsize = (int)(((double)winsize) / zoom);
    *doff = (int)((imsize / 2 ) - (winsize / zoom / 2));
    *doff -= *offset;
    
    /* Offset in lower corner. */
    if (*doff < 0){
      
      /* maxborder. */
      int maxwoff = 0;     // was winsize/6;
      *woff = (int)(-(*doff) * zoom);
      if (*woff > maxwoff)
        *woff = maxwoff;
      *doff = 0;
      *drawsize = (int)(((double)(winsize - (*woff))) / zoom);
      *offset   = (int)(((float)imsize*0.5f) -
        ((((float)winsize*0.5f) - (float)*woff)/(float)zoom));
      
      /* try and fill corners. */
      if (*drawsize < (imsize-1)) (*drawsize)++;
      //PrintfToLog("ds do offset wo %d %d %d %d", *drawsize, *doff, *offset, *woff);
      return;
    }
    /* Offset in upper corner */
    if ( (*doff + *drawsize) > (imsize - 1)){
      
      /* The minimum drawsize. */
      int minds = (int)((winsize * 1.0)/zoom);  // was * 0.8333333
      
      *drawsize = imsize - *doff;
      
      if (*drawsize < minds){
        *drawsize = minds;
        *doff     = imsize - *drawsize;
        *offset   = (int)(((float)imsize * 0.5) - (float)(*doff) -
          (((float)winsize*0.5f)/(float)zoom) - 2.0f);
      }
      //PrintfToLog("ds do offset wo %d %d %d %d", *drawsize, *doff, *offset, *woff);
      
      return;
    }
    if (*drawsize < (imsize-1)) (*drawsize)++;
  }
  //PrintfToLog("ds do offset wo %d %d %d %d", *drawsize, *doff, *offset, *woff);
  return;
}

double CSerialEMView::b3dStepPixelZoom(double czoom, int step)
{
  double *zv = zoomvals;
  int i=0;
  int zoomi;
  
  if (mWinApp->mBufferManager->GetFixedZoomStep()) {
    if (step < 0) {
      if (czoom < zv[0])
        return czoom;
      return czoom / sqrt(2.);
    } else {
      if (czoom > zv[MAXZOOMI])
        return czoom;
      return czoom * sqrt(2.);
    }
  }
  if (step > 0) {
  /* DNM: need to cast both to floats because czoom was stored as a
    float and some comparisons can fail on PC */
    for(i = 0; i < MAXZOOMI; i++)
      if ((float)zv[i] > (float)czoom) break;
      zoomi = (i - 1) + step;
      
  } else {
    
    for(i = MAXZOOMI; i >= 0; i--)
      if ((float)zv[i] < (float)czoom) break;
      zoomi = (i + 1) + step;
  }
  
  if (zoomi < 0) zoomi = 0;
  if (zoomi > MAXZOOMI) zoomi = MAXZOOMI;
  /* printf ("%f %d %d %f\n", czoom, step, zoomi, zv[zoomi]); */
  return(zv[zoomi]);
}

void CSerialEMView::ZoomUp()
{
  double oldZoom = mZoom;
  mZoom = b3dStepPixelZoom(mZoom, 1);

  // Nice idea but too unpredictable, no good way to not have marker, etc
  /*// Expand around a drawn point if there is one
  if (mZoom > oldZoom && mImBufs) {
    EMimageBuffer *imBuf = &mImBufs[mImBufIndex];
    if (imBuf->mHasUserPt && !imBuf->mHasUserLine && imBuf->mImage) {
      m_iOffsetX = -B3DNINT(imBuf->mUserPtX - imBuf->mImage->getWidth() / 2.);
      m_iOffsetY = B3DNINT(imBuf->mUserPtY - imBuf->mImage->getHeight() / 2.);
    }
  } */
  NewZoom();
}

void CSerialEMView::ZoomDown()
{
  mZoom = b3dStepPixelZoom(mZoom, -1);
  NewZoom();
}

void CSerialEMView::SetZoom(double inZoom)
{
  mZoom = B3DMIN(20.0, B3DMAX(0.002, inZoom));
  NewZoom();
}

// Notify image level window of new zoom
void CSerialEMView::NewZoom()
{
  double effZoom;
  float binning;
  if (mImBufs[mImBufIndex].mImage)
    DrawImage();
  if (mImBufs[mImBufIndex].mImageScale)
    mWinApp->mImageLevel.NewZoom(mZoom);
  
  // Set effective zoom unless it is a map
  binning = GetBufferBinning();
  if (binning > 0. && !mImBufs[mImBufIndex].mMapID) {
    effZoom = mZoom / binning;

    // Set separately for View, Search, and general
    if (mImBufs[mImBufIndex].mLowDoseArea &&
      mImBufs[mImBufIndex].mConSetUsed == VIEW_CONSET) {
      mViewEffectiveZoom = effZoom;
    } else if (mImBufs[mImBufIndex].mLowDoseArea &&
      mImBufs[mImBufIndex].mConSetUsed == SEARCH_CONSET) {
      mSearchEffectiveZoom = effZoom;
    } else {
      mEffectiveZoom = effZoom;
    }
  }
  mImBufs[mImBufIndex].mZoom = mZoom;
}

// Left button down
// Capture the mouse; save the current point
void CSerialEMView::OnLButtonDown(UINT nFlags, CPoint point) 
{
  SetCapture();
  m_iPrevMX = point.x;  // Integer values for panning
  m_iPrevMY = point.y;
  mMouseDownTime = GetTickCount();  // Get start time and set flag
  mDrawingLine = false;   // that it may not be a or line draw
  CView::OnLButtonDown(nFlags, point);
}

// Record information for zooming around the mouse point
void CSerialEMView::SetupZoomAroundPoint(CPoint *point)
{
  CRect rect;
  GetClientRect(&rect);
  mZoomAroundPoint = mImBufs && ConvertMousePoint(&rect, mImBufs[mImBufIndex].mImage,
    point, mZoomupImageX, mZoomupImageY);
  mZoomupPoint = *point;
}

// Left double click goes to Navigator for editing points
void CSerialEMView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
  if (mWinApp->mNavigator &&
    !(GetAsyncKeyState(VK_CONTROL) / 2) && mWinApp->mNavigator->InEditMode())
    mWinApp->mNavigator->MouseDoubleClick(VK_LBUTTON);
}

// Middle double click goes to Navigator for loading map
void CSerialEMView::OnMButtonDblClk(UINT nFlags, CPoint point)
{
  if (mWinApp->mNavigator && (GetAsyncKeyState(VK_SHIFT) / 2) &&
    !(GetAsyncKeyState(VK_CONTROL) / 2))
    mWinApp->mNavigator->MouseDoubleClick(VK_MBUTTON);
}

// Release mouse if it was captured by this window
void CSerialEMView::OnLButtonUp(UINT nFlags, CPoint point) 
{
  float shiftX, shiftY, angle, pixScale, crossLen;
  int imX, imY;
  double defocus;
  BOOL legal, used = false, showStage = false;
  CString lenstr;
  bool shiftKey = GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  bool ctrlKey = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  EMimageBuffer *imBuf;
  CProcessImage *processImg = mWinApp->mProcessImage;
  CRect rect;
  FloatVec radii;
  mNavUsedLastLButtonUp = false;
  if (GetCapture() == this) {
    ReleaseCapture();
    if (mImBufs && !mPanning && !mDrawingLine &&
      SEMTickInterval(mMouseDownTime) <= USER_PT_CLICK_TIME) {
      
      // If the up event is within the click time and we haven't started panning,
      // get the user point and redraw
      imBuf = &mImBufs[mImBufIndex];
      GetClientRect(&rect);
      legal = ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY);

      if (mWinApp->mNavHelper->mHoleFinderDlg->HaveHolesToDrawOrMakePts() &&
        !shiftKey && ctrlKey) {
        crossLen = (float)(2 * mWinApp->ScaleValueForDPI(9) / (mZoom < 1 ? mZoom : 1.));
        mNavUsedLastLButtonUp = mWinApp->mNavHelper->mHoleFinderDlg->MouseSelectPoint(
          imBuf, shiftX, shiftY, crossLen, false);
      }

      // Navigator gets first crack if it is either a legal point or a valid hit for
      // selection
      if (!mNavUsedLastLButtonUp && ((!mDrewLDAreasAtNavPt || (!shiftKey && ctrlKey)) && 
        mWinApp->mNavigator &&
        (legal || (!shiftKey && (ctrlKey || mWinApp->mNavigator->InEditMode())) ||
        (shiftKey && mWinApp->mNavigator->InEditMode()))))
          mNavUsedLastLButtonUp = mWinApp->mNavigator->UserMousePoint(imBuf, shiftX,
            shiftY, mPointNearImageCenter, VK_LBUTTON);
      if (legal && !mNavUsedLastLButtonUp) {
          
        // If the point is legal and was not used by navigator, commit it and output 
        // stats.  But first see if low dose needs to snap it to axis and act on it
        mWinApp->mLowDoseDlg.UserPointChange(shiftX, shiftY, imBuf);
        if ((mMainWindow || mFFTWindow) && !imBuf->mCtfFocus1)
          processImg->ModifyFFTPointToFirstZero(imBuf, shiftX, shiftY);
        imBuf->mHasUserPt = !(GetAsyncKeyState(VK_SHIFT) / 2);
        imBuf->mHasUserLine = false;
        imBuf->mDrawUserBox = false;
        imBuf->mIllegalUserPt = false;
        imBuf->mUserPtX = shiftX;
        imBuf->mUserPtY = shiftY;
        mWinApp->mLowDoseDlg.Update();
        if ((mMainWindow || mFFTWindow) && 
          processImg->GetFFTZeroRadiiAndDefocus(imBuf, &radii, defocus)) {
            lenstr.Format("Defocus -%.2f um", defocus);
            imBuf->mCtfFocus1 = 0.;
            imX = 0;
            if (processImg->GetCtffindOnClick() &&
              !mWinApp->mCamera->DoingContinuousAcquire()) { 
              imX = FitCtfAtMarkedPoint(imBuf, lenstr, defocus, radii);
            }
            if (processImg->GetPlatePhase() > 0.001 || imX > 0)
              lenstr += ": PP";
            mWinApp->SetStatusText(MEDIUM_PANE, lenstr);
        } else {
          imX = (int)imBuf->mUserPtX;
          imY = (int)imBuf->mUserPtY;
          ShowImageValue(imBuf->mImage, imX, imY, MEDIUM_PANE);
          showStage = true;
        }
        if (mWinApp->mNavigator)
          mWinApp->mNavigator->UpdateAddMarker();
      }
      if (!legal && !mNavUsedLastLButtonUp) {
        if (!(GetAsyncKeyState(VK_SHIFT) / 2)) {
          imBuf->mHasUserPt = false;
          imBuf->mHasUserLine = false;
          imBuf->mDrawUserBox = false;
          imBuf->mUserPtX = shiftX;
          imBuf->mUserPtY = shiftY;
          imBuf->mIllegalUserPt = true;
          showStage = true;
        } else
          imBuf->mIllegalUserPt = false;
        if (mWinApp->mNavigator)
          mWinApp->mNavigator->UpdateAddMarker();
        used = true;
      }
      if (legal || used)
        DrawImage();
      if (mWinApp->mNavigator && showStage) {
        if (mWinApp->mNavigator->MarkerStagePosition(imBuf, shiftX, shiftY)) {
          lenstr.Format("%.2f, %.2f um", shiftX, shiftY);
          mWinApp->SetStatusText(SIMPLE_PANE, lenstr, true);
        }
      }
    } else if (mImBufs && mDrawingLine) {

      // If done drawing a line, output the length to log window
      imBuf = &mImBufs[mImBufIndex];
      if (!imBuf->mDrawUserBox) {
        if (mWinApp->mLowDoseDlg.CanActOnUserPointChange(imBuf, shiftX, shiftY)) {
          shiftX += imBuf->mLineEndX - imBuf->mUserPtX;
          shiftY += imBuf->mLineEndY - imBuf->mUserPtY;
          imBuf->mHasUserPt = true;
          imBuf->mHasUserLine = false;
          imBuf->mIllegalUserPt = false;
          mWinApp->mLowDoseDlg.UserPointChange(shiftX, shiftY, imBuf);
          imBuf->mUserPtX = shiftX;
          imBuf->mUserPtY = shiftY;
          mWinApp->mLowDoseDlg.Update();
          DrawImage();
        } else {
          GetLineLength(imBuf, shiftX, shiftY, angle);
          if (shiftY) {
            pixScale = shiftY >= 1000. ? 0.001f : 1.f;
            lenstr.Format("Drawn line is %.1f pixels, " + mWinApp->PixelFormat(shiftY,
              pixScale) + "   at %.1f degrees", shiftX, shiftY * pixScale, angle);
          } else
            lenstr.Format("Drawn line is %.1f pixels    at %.1f degrees", shiftX, angle);
          mWinApp->AppendToLog(lenstr, LOG_SWALLOW_IF_CLOSED);
        }
      }
    } else if (mPanning && mZoom < 0.8 && mWinApp->mBufferManager->GetAntialias()) {

      // If we were panning and suspended antialias drawing, do antialias draw now
      mPanning = false;
      DrawImage();
    } 
    mPanning = false;
  }
  CView::OnLButtonUp(nFlags, point);
}


int CSerialEMView::FitCtfAtMarkedPoint(EMimageBuffer *imBuf, CString &lenstr,
  double &defocus, FloatVec &radii)
{
  int imX, imY, loop, astigPhase = 1;
  float phase = 0., plotterPhase = 0., fitStart = -1.;
  float plotterDef, astig, angle, fitToFreq, CCC, reduction;
  double mmDefocus, newRad;
  CProcessImage *processImg = mWinApp->mProcessImage;
  EMimageBuffer *mainImBufs = mWinApp->GetImBufs();
  CtffindParams param;
  CString command, filename, saveName, results;
  BOOL usePlotter = processImg->GetClickUseCtfplotter();
  float resultsArray[7];
  for (imX = 0; imX < MAX_BUFFERS; imX++)
    if (imBuf->mTimeStamp == mainImBufs[imX].mTimeStamp && imBuf != &mainImBufs[imX])
      break;

  // For an FFT, run Ctffind or ctfplotter if option selected
  if (imX >= MAX_BUFFERS)
    return -1;
  if (processImg->InitializeCtffindParams(&mainImBufs[imX], param))
    return -2;

  // Adjust search parameters based on options
  param.slower_search = processImg->GetSlowerCtfFit() > 0;
  param.compute_extra_stats = processImg->GetExtraCtfStats() > 0;

  // Turn off astigmatism if Ctrl key down
  if (GetAsyncKeyState(VK_CONTROL) / 2) {
    param.astigmatism_is_known = true;
    param.known_astigmatism = 0.;
    param.known_astigmatism_angle = 0.;
    astigPhase = 0;
  }

  // Assumed phase is stored in radians, passed here in radians and 
  // the params to ctffind are in radians
  if (processImg->GetPlatePhase() > 0.001 || processImg->GetCtfFindPhaseOnClick()) {
    param.minimum_additional_phase_shift =
      param.maximum_additional_phase_shift = (float)(processImg->GetPlatePhase());
    param.find_additional_phase_shift = true;
    plotterPhase = (float)(processImg->GetPlatePhase() / DTOR);
    if (processImg->GetMinCtfFitResIfPhase() > 0.)
      fitStart = processImg->GetMinCtfFitResIfPhase();

    // But the min and max for fitting are stored as degrees
    if (processImg->GetCtfFindPhaseOnClick()) {
      astigPhase += 2;
      param.minimum_additional_phase_shift = (float)(processImg->GetCtfMinPhase() * DTOR);
      param.maximum_additional_phase_shift = (float)(processImg->GetCtfMaxPhase() * DTOR);
      if (processImg->GetCtfFixAstigForPhase()) {
        param.astigmatism_is_known = true;
        param.known_astigmatism = 0.;
        param.known_astigmatism_angle = 0.;
        astigPhase -= 1;
      }
    }

    // Get defocus and radii from middle of phase search range for setting the params
    phase = (param.minimum_additional_phase_shift + param.maximum_additional_phase_shift)
      / 2.f;
    if (usePlotter && processImg->GetCtfFindPhaseOnClick()) {
      plotterPhase = (float)processImg->GetCtfExpectedPhase();
      phase = (float)(plotterPhase * DTOR);
      param.minimum_additional_phase_shift = phase - (float)(60. * DTOR);
      param.maximum_additional_phase_shift = phase + (float)(60. * DTOR);

    }
    processImg->GetFFTZeroRadiiAndDefocus(imBuf, &radii, defocus, phase);

  }
  processImg->SetCtffindParamsForDefocus(param, defocus, false);

  // Use radii of first and second zeros to determine a smaller defocus
  // range from that at the second zero to that halfway of the zero
  // spacing below the first
  if (radii.size() > 1) {
    imY = imBuf->mImage->getWidth() / 2;
    for (loop = 0; loop < (param.find_additional_phase_shift ? 2 : 1); loop++) {
      phase = EXTRA_NO_VALUE;
      if (param.find_additional_phase_shift)
        phase = loop ? param.maximum_additional_phase_shift :
        param.minimum_additional_phase_shift;
      newRad = radii[0] -
        B3DMAX(10. / (imY * mZoom), (usePlotter ? 2.0 : 1.0) * (radii[1] - radii[0]));
      if (mWinApp->mProcessImage->DefocusFromPointAndZeros(newRad, 1,
        param.pixel_size_of_input_image / 10.f, 0., NULL, mmDefocus, phase))
        ACCUM_MIN(param.maximum_defocus, (float)(mmDefocus * 10000.));
      newRad = radii[0] +
        B3DMAX(10. / (imY * mZoom), (usePlotter ? 1.0 : 0.5) * (radii[1] - radii[0]));
      if (mWinApp->mProcessImage->DefocusFromPointAndZeros(newRad, 1,
        param.pixel_size_of_input_image / 10.f, 0., NULL, mmDefocus, phase))
        ACCUM_MAX(param.minimum_defocus, (float)(mmDefocus * 10000.));
    }
  }

  imBuf->mMaxRingFreq = 0.;
  if (usePlotter) {

    // CTFPLOTTER:  Tried wider defocus range and sending expected defocus, it did not
    // help with highly astigmatic images (> 0.8 micron?)
    if (processImg->MakeCtfplotterShrMemFile(imX, filename, reduction)) {
      mWinApp->AppendToLog(filename);
      return -3;
    }

    if (mWinApp->mExternalTools->MakeCtfplotterCommand(filename, reduction, imX, 0.,
      -param.minimum_defocus / 10000.f, -param.maximum_defocus / 10000.f, 0.,//-defocus,
      astigPhase, plotterPhase, 1, 0., fitStart, 0., 0, saveName, command)) {
      mWinApp->AppendToLog(command);
      processImg->DeletePlotterShrMemFile();
      return -3;
    }
    if (processImg->RunCtfplotterOnBuffer(filename, command, 5000)) {
      mWinApp->AppendToLog(command);
      return -3;
    }
    if (mWinApp->mExternalTools->ReadCtfplotterResults(plotterDef, astig, angle, phase,
      astigPhase, fitToFreq, CCC, results, command)) {
      mWinApp->AppendToLog(command);
      return -3;
    }

    // Store the result for use in tuning etc.
    mWinApp->AppendToLog(results);
    imBuf->mCtfFocus1 = -plotterDef + astig / 2.f;
    imBuf->mCtfFocus2 = -plotterDef - astig / 2.f;
    imBuf->mCtfAngle = angle;
    imBuf->mCtfPhase = (float)(param.find_additional_phase_shift ? phase * DTOR :
      EXTRA_NO_VALUE);
    lenstr.Format("Defocus %.2f  astig %.3f um", plotterDef, astig);
    if (fitToFreq > 0)
      imBuf->mMaxRingFreq = param.pixel_size_of_input_image / fitToFreq;

  } else {

    // CTFFIND
    // Boost search step for big defocus ranges
    if (param.slower_search)
      ACCUM_MAX(param.defocus_search_step,
      (param.maximum_defocus - param.minimum_defocus) / 100.f);
    SEMTrace('1', "Search defocus range %.0f  %.0f  step %.0f  resolution "
      "range %.1f %.1f", param.minimum_defocus, param.maximum_defocus,
      param.defocus_search_step, param.minimum_resolution,
      param.maximum_resolution);

    if (processImg->RunCtffind(&mainImBufs[imX], param, resultsArray))
      return -3;
    imBuf->mCtfFocus1 = resultsArray[0] / 10000.f;
    imBuf->mCtfFocus2 = resultsArray[1] / 10000.f;
    imBuf->mCtfAngle = resultsArray[2];
    imBuf->mCtfPhase = param.find_additional_phase_shift ? resultsArray[3] :
      (float)EXTRA_NO_VALUE;
    lenstr.Format("Defocus -%.2f  astig %.3f um", (imBuf->mCtfFocus1 +
      imBuf->mCtfFocus2) / 2., imBuf->mCtfFocus1 - imBuf->mCtfFocus2);
    if (param.compute_extra_stats && resultsArray[5] > 0.)
      imBuf->mMaxRingFreq = param.pixel_size_of_input_image /
      resultsArray[5];
  }

  return param.find_additional_phase_shift ? 1 : 0;
}

// Convert a mouse point to an image coordinate, returning true if it is inside image
BOOL CSerialEMView::ConvertMousePoint(CRect *rect, KImage *image, CPoint *point, 
                                      float &outX, float &outY)
{
  float shiftX, shiftY;
  float centerTol = 5.;
  if (!image)
    return false;

  // The problem is that we want the point coordinates to be with inverted Y
  // while the border-start (Src-Dest) equations are written in terms of 
  // non-inverted coordinate system.  Thus, need to invert the Y mouse, compute and scale
  // offset and invert back.  Inversion of integer mouse coordinates uses height - 1,
  // but inversion of floating image coords uses height because they range from 0-height
  int my = rect->Height() - 1 - (point->y - ZoomedPixelYAdjustment(rect, image));
  image->getShifts(shiftX, shiftY);
  outX = (float)((point->x + 0.5 - mXDest) / mZoom + mXSrc) - shiftX;
  outY = (float)(image->getHeight() -
    ((my + 0.5 - mYDest) / mZoom + mYSrc)) - shiftY;
  mPointNearImageCenter = fabs(outX - image->getWidth() / 2.) / mZoom < centerTol &&
    fabs(outY - image->getHeight() / 2.) / mZoom < centerTol;
  return (outX >= 0 && outX < image->getWidth() &&
    outY >= 0 && outY < image->getHeight());
}

// Return the image coordinates of the corners of the window
void CSerialEMView::WindowCornersInImageCoords(EMimageBuffer *imBuf, float *xCorner,
                                               float *yCorner)
{
  CRect rect;
  CPoint point;
  GetClientRect(&rect);
  point.x = 0;
  point.y = 0;
  ConvertMousePoint(&rect, imBuf->mImage, &point, xCorner[0], yCorner[0]);
  point.x = rect.Width();
  ConvertMousePoint(&rect, imBuf->mImage, &point, xCorner[1], yCorner[1]);
  point.y = rect.Height();
  ConvertMousePoint(&rect, imBuf->mImage, &point, xCorner[2], yCorner[2]);
  point.x = 0;
  ConvertMousePoint(&rect, imBuf->mImage, &point, xCorner[3], yCorner[3]);
}

// Middle button down: zoom up if control, or add a point in edit mode
void CSerialEMView::OnMButtonDown(UINT nFlags, CPoint point) 
{
  float shiftX, shiftY, pixel;
  EMimageBuffer *imBuf;
  CString str;
  CRect rect;
  BOOL legal, isFFT;
  bool ctrl = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  bool shifted = GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  if (ctrl && !shifted) {
    SetupZoomAroundPoint(&point);
    ZoomUp();
  } else if (mImBufs && mImBufs[mImBufIndex].mImage) {
    GetClientRect(&rect);
    imBuf = &mImBufs[mImBufIndex];
    legal = ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY);
    isFFT = imBuf->mCaptured == BUFFER_FFT || imBuf->mCaptured == BUFFER_LIVE_FFT;
    if (ctrl && shifted && legal && mMainWindow && 
      mWinApp->mNavHelper->mAutoContouringDlg->SinglePolygonMode() &&
      !mWinApp->DoingTasks()) {
      mWinApp->mNavHelper->mAutoContouringDlg->MakeSinglePolygon(imBuf, shiftX, shiftY);
    } else if (legal && (isFFT || (mWinApp->mNavigator && 
      ((mWinApp->mNavigator->TakingMousePoints() && !shifted) || (shifted && !ctrl))))) {
      if (isFFT) {
        pixel = mShiftManager->GetPixelSize(imBuf);
        if (pixel) {
          shiftY = (float)(sqrt(pow(shiftX - imBuf->mImage->getWidth() / 2., 2.) +
            pow(shiftY - imBuf->mImage->getHeight() / 2., 2.)) /
            imBuf->mImage->getWidth());
          shiftX = 10000.f * pixel / B3DMAX(0.0001f, shiftY);
          str.Format("Freq. %.3g/A   Period %.1f A", 1. / shiftX, shiftX);
          mWinApp->SetStatusText(MEDIUM_PANE, str);
        }
      } else {
        mWinApp->mNavigator->UserMousePoint(imBuf, shiftX, shiftY, mPointNearImageCenter
          , VK_MBUTTON);
        DrawImage();
      }
    }
  }

  CView::OnMButtonDown(nFlags, point);
}

// Right button down: zoom down if control
// Otherwise, store values for image shifting, and either start a clock in edit mode
// or just start shifting; notify shifter if this is the A buffer
void CSerialEMView::OnRButtonDown(UINT nFlags, CPoint point) 
{
  BOOL shiftKey = ::GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  BOOL ctrlKey = ::GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  if (ctrlKey && !shiftKey) {
    SetupZoomAroundPoint(&point);
    ZoomDown();
  } else {
    SetCapture();
    m_iPrevMX = point.x;  // Integer values for panning
    m_iPrevMY = point.y;
    m_dPrevMX = point.x;  // Double values to keep better track in image shift
    m_dPrevMY = point.y;
    if (mWinApp->mNavigator && mWinApp->mNavigator->TakingMousePoints()) {
      mMouseDownTime = GetTickCount();  // Get start time
    } else if (mMainWindow) {
      mShiftManager->StartMouseShifting(shiftKey, mImBufIndex);
      mMouseShifting = true;
    }
  }

  CView::OnRButtonDown(nFlags, point);
}

// Right button up: notify shifter that it is time to shift, or do nav edit mode move
void CSerialEMView::OnRButtonUp(UINT nFlags, CPoint point) 
{
  float shiftX, shiftY;
  EMimageBuffer *imBuf;
  CRect rect;
  if (GetCapture() == this) {
    ReleaseCapture();
    if (mMainWindow && mMouseShifting) {
      mShiftManager->EndMouseShifting(mImBufIndex);
      mMouseShifting = false;
      DrawImage();
    } else if (mWinApp->mNavigator && mWinApp->mNavigator->TakingMousePoints() && mImBufs
      && !(GetAsyncKeyState(VK_SHIFT) / 2) && !(GetAsyncKeyState(VK_CONTROL) / 2)) {
        imBuf = &mImBufs[mImBufIndex];
        GetClientRect(&rect);
        if (ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY)) {
          mWinApp->mNavigator->UserMousePoint(imBuf, shiftX, shiftY, 
            mPointNearImageCenter, VK_RBUTTON);
          DrawImage();
        }
  
    }
  }

  CView::OnRButtonUp(nFlags, point);
}

// Mouse move: process many situations
void CSerialEMView::OnMouseMove(UINT nFlags, CPoint point) 
{
  int iDx, iDy, pDx, pDy;
  double zoomFac = mZoom < 1 ? mZoom : 1.;
  float shiftX, shiftY, prevX, prevY, angle, pixScale, crossLen;
  BOOL shiftKey = ::GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  BOOL ctrlKey = ::GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  bool navEdit = mWinApp->mNavigator && mWinApp->mNavigator->m_bEditMode;
  bool haveHoles = mWinApp->mNavHelper->mHoleFinderDlg->HaveHolesToDrawOrMakePts();
  bool dragSelect = (navEdit || haveHoles) && ctrlKey &&
    !shiftKey && (nFlags & MK_LBUTTON) && !mDrawingLine;
  EMimageBuffer *imBuf;
  CRect rect;
  CString lenstr;
  if (mImBufs != NULL && GetCapture() == this && (!ctrlKey || shiftKey || dragSelect)) {
    if (nFlags & MK_RBUTTON) {

      // Right button: if there is a change in position, and mouse shifting is either
      // underway or the change is big enough or enough time has passed, then shift
      iDx = (int) floor((point.x - m_dPrevMX) / mZoom + 0.5);
      iDy = (int) floor((point.y - m_dPrevMY) / mZoom + 0.5);
      if ((iDx || iDy) && (mMouseShifting || 
        (mWinApp->mNavigator && mWinApp->mNavigator->TakingMousePoints() && 
        SEMTickInterval(mMouseDownTime) > USER_PT_CLICK_TIME) ||
        iDx < -PAN_THRESH || iDx > PAN_THRESH || 
        iDy < -PAN_THRESH || iDy > PAN_THRESH)) {
          if (mMainWindow) {
            if (!mMouseShifting) {

              // Initiate shifting if not underway yet
              mShiftManager->StartMouseShifting(shiftKey, mImBufIndex);
              mMouseShifting = true;
            }
            mShiftManager->SetAlignShifts((float)iDx, (float)iDy, true, 
              &mImBufs[mImBufIndex]);
          }
          m_iPrevMX = point.x;
          m_iPrevMY = point.y;
          m_dPrevMX += iDx * mZoom;
          m_dPrevMY += iDy * mZoom;
      }
    
    } else {

      // Pan 1:1 with image for zooms below 1, and 1:1 with pixels for zooms above 1
      pDx = point.x - m_iPrevMX;
      iDx = B3DNINT(pDx / zoomFac);
      pDy = point.y - m_iPrevMY;
      iDy = -B3DNINT(pDy / zoomFac);

      // Take this as a panning move if there is a move, and either panning
      // has already started, or click time is expired, or the move is greater
      // than a threshold
      if ((iDx || iDy) && (mPanning || mDrawingLine || dragSelect ||
        GetTickCount() - mMouseDownTime > USER_PT_CLICK_TIME || 
        pDx < -PAN_THRESH || pDx > PAN_THRESH || 
        pDy < -PAN_THRESH || pDy > PAN_THRESH)) {

        // Start or continue panning if panning already or no shift key
        GetClientRect(&rect);
        imBuf = &mImBufs[mImBufIndex];
        if (mPanning || (!mDrawingLine && !shiftKey && !dragSelect)) {
          mPanning = true;
          m_iOffsetX += iDx;
          m_iOffsetY += iDy;
          m_iPrevMX += B3DNINT(zoomFac * iDx);
          m_iPrevMY -= B3DNINT(zoomFac * iDy);
          DrawImage();
        } else if (ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY) || 
          dragSelect) {

          if (dragSelect) {
            crossLen = (float)(2 * mWinApp->ScaleValueForDPI(9) / zoomFac);
            if (haveHoles)
              navEdit = !mWinApp->mNavHelper->mHoleFinderDlg->MouseSelectPoint(
                imBuf, shiftX, shiftY, crossLen, true);
            if (navEdit)
              mWinApp->mNavigator->MouseDragSelectPoint(imBuf, shiftX, shiftY, crossLen);

            // Otherwise start or continue drawing a line if point is within image
          } else if (!mDrawingLine && !(imBuf->mCaptured == BUFFER_FFT || 
            imBuf->mCaptured == BUFFER_LIVE_FFT)) {
            CPoint prev(m_iPrevMX, m_iPrevMY);
            if (!ConvertMousePoint(&rect, imBuf->mImage, &prev, prevX, prevY)) {
              prevX = shiftX;
              prevY = shiftY;
            }
            mDrawingLine = true;
            imBuf->mHasUserPt = false;
            imBuf->mHasUserLine = true;
            imBuf->mIllegalUserPt = false;
            imBuf->mCtfFocus1 = 0.;
            imBuf->mUserPtX = prevX;
            imBuf->mUserPtY = prevY;
            if (mWinApp->mNavigator)
              mWinApp->mNavigator->UpdateAddMarker();
            mWinApp->mLowDoseDlg.Update();
          }
          if (mDrawingLine) {
            imBuf->mLineEndX = shiftX;
            imBuf->mLineEndY = shiftY;
            imBuf->mDrawUserBox = ctrlKey;
            if (ctrlKey) {
              GetUserBoxSize(imBuf, iDx, iDy, shiftX, shiftY);
              if (shiftX) {
                pixScale = B3DMIN(shiftX, shiftY) >= 1000. ? 0.001f : 1.f;
                lenstr.Format("%d x %d: " + mWinApp->PixelFormat(shiftX, pixScale) + 
                  " x " + mWinApp->PixelFormat(shiftY, pixScale), iDx, iDy, 
                  shiftX * pixScale, shiftY * pixScale);
              } else
                lenstr.Format("%d x %d pixels", iDx, iDy);
            } else {
              GetLineLength(imBuf, shiftX, shiftY, angle);
              if (shiftY) { 
                pixScale = shiftY >= 1000. ? 0.001f : 1.f;
                lenstr.Format("%.1f pix, " + mWinApp->PixelFormat(shiftY, pixScale) + 
                  " at %.1f deg", shiftX, shiftY * pixScale, angle);
              } else
                lenstr.Format("%.1f pix at %.1f deg", shiftX, angle);
            }
            mWinApp->SetStatusText(MEDIUM_PANE, lenstr);
            DrawImage();
          }
        }
      }
    }

  // If not a specific action, do continuous update in complex pane
  } else if (!mWinApp->DoingComplexTasks() || !mWinApp->DoingTasks() || 
    mWinApp->GetJustChangingLDarea() || mWinApp->GetJustDoingSynchro()) {
    EMimageBuffer *imBuf = &mImBufs[mImBufIndex];
    CRect rect;
    GetClientRect(&rect);
    int imX, imY;
    float shiftX, shiftY;
    if (ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY)) {
      imX = (int)shiftX;
      imY = (int)shiftY;
      ShowImageValue(imBuf->mImage, imX, imY, 
        !mWinApp->DoingComplexTasks() ? COMPLEX_PANE : MEDIUM_PANE);
    }
  }
  
  CView::OnMouseMove(nFlags, point);
}

// Zoom image with mouse wheel
BOOL CSerialEMView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
  int clicksPerZoom = 3, deltaSign = mWinApp->GetReverseWheelZoom() ? 1 : -1;
  double stepZoom, lastStep;
  int numClicks, step = 1;
  BOOL inWin, inLog = false, inNav = false;
  CRect rect;
  if (nFlags & MK_CONTROL)
    return false;

  // Just in case there are continuously scrolling devices, accumulate changes and respond
  // only when the standard threshold is exceeded
  mWheelDeltaPending += zDelta;
  if (B3DABS(mWheelDeltaPending) >= WHEEL_DELTA) {

    // Derive an equivalent number of clicks and set the step sign
    numClicks = B3DNINT(B3DABS(mWheelDeltaPending) / (double)WHEEL_DELTA);
    if (zDelta < 0) {
      numClicks = -numClicks;
      step = -1;
    }

    // Determine if in the image window, nav, or log window
    GetWindowRect(rect);
    inWin = rect.PtInRect(pt);
    if (!inWin) {
      if (mWinApp->mNavigator) {
        mWinApp->mNavigator->GetWindowRect(rect);
        inNav = rect.PtInRect(pt);
      }
      if (mWinApp->mLogWindow) {
        mWinApp->mLogWindow->GetWindowRect(rect);
        inLog = rect.PtInRect(pt);
      }

      // If not in window and in nav and either not in log or nav was last clicked, do nav
      if (inNav && (!inLog || mWinApp->GetNavOrLogHadFocus() > 0)) {
        int top = mWinApp->mNavigator->m_listViewer.GetTopIndex();
        mWinApp->mNavigator->m_listViewer.SetTopIndex(top - numClicks);
        mWheelDeltaPending = 0;
        return true;
      }

      // If not in window and in log and not in nav or log has focus last, do log
      if (inLog && (!inNav || mWinApp->GetNavOrLogHadFocus() < 0)) {
        mWinApp->mLogWindow->m_editWindow.LineScroll(-numClicks);
        mWheelDeltaPending = 0;
        return true;
      }

      // Otherwise do nothing if not in window
      return false;
    }

     // Leave the difference pending and skip if at end of range
    mWheelDeltaPending -= numClicks * WHEEL_DELTA;
    if ((mZoom <= zoomvals[0] && step * deltaSign < 0) || 
      (mZoom >= zoomvals[MAXZOOMI] && step * deltaSign > 0)) {
      mWheelDeltaPending = 0;
      return true;
    }
 
    // Determine next step and previous one and make a zoom change that will give full
    // step in given number of clicks
    stepZoom = b3dStepPixelZoom(mZoom, step * deltaSign);
    lastStep = b3dStepPixelZoom(stepZoom, -step * deltaSign);
    mZoom *= pow(stepZoom / lastStep, fabs(numClicks / (double)clicksPerZoom));
    if ((mZoom < stepZoom && step * deltaSign < 0) ||
      (mZoom > stepZoom && step * deltaSign > 0))
      mZoom = stepZoom;
    NewZoom();
  }
  return true;
}

#define VK_MINUS 189
#define VK_EQUALS 187
void CSerialEMView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  int iTrial, iDir, iBuf;
  float arrowInc = mWinApp->mRemoteControl.GetBeamIncrement();
  float brightInc = mWinApp->mRemoteControl.GetIntensityIncrement();
  float fdx =0., fdy = 0.;
  EMimageBuffer *mainBufs = mWinApp->GetImBufs();
  char cChar = char(nChar);
  bool mainOrFFT = mMainWindow || mFFTWindow;
  bool ctrl = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  bool shift = GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  bool navCanProcess = mWinApp->mNavigator && !mWinApp->mNavigator->mNavAcquireDlg;
  mWinApp->mMacroProcessor->SetKeyPressed((int)cChar);

  // Keep track of ctrl and shift (ONLY IF WE GET KEYUP TOO)
  /*if (nChar == VK_CONTROL)
    mCtrlPressed = true;
  if (nChar == VK_SHIFT)
    mShiftPressed = true; */
  if (!BOOL_EQUIV(ctrl, mCtrlPressed))
    mWinApp->mRemoteControl.CtrlChanged(ctrl);
  if (!BOOL_EQUIV(shift, mShiftPressed))
    mWinApp->mRemoteControl.ShiftChanged(shift);
  mCtrlPressed = ctrl;
  mShiftPressed = shift;
  SEMTrace('k', "Code %u, char %c  ctrl %d shift %d", nChar, cChar, mCtrlPressed,
    mShiftPressed);

  // Zoom up and down with these non standard keys
  if (nChar == VK_MINUS) {
    ZoomDown();
  } else if (nChar == VK_EQUALS) {
    ZoomUp();

    // Invert contrast
  } else if (nChar == VK_F11) {
    mWinApp->mImageLevel.ToggleInvertContrast();

  } else if (nChar == VK_F12) {
    mWinApp->mImageLevel.ToggleFalseColor();

    // Beam shift with shifted arrow keys; change increment with < or >
  } else if (mShiftPressed && nChar >= VK_LEFT && nChar <= VK_DOWN) {
    if (nChar % 2)
      fdx = (float)((nChar - 38.) * arrowInc);
    else
      fdy = (float)((39. - nChar) * arrowInc);
    mWinApp->mProcessImage->MoveBeamByCameraFraction(fdx, fdy, true);

  } else if ((shift && !ctrl) && (nChar == VK_OEM_COMMA || nChar == VK_OEM_PERIOD)) {
    arrowInc *= (nChar == VK_OEM_COMMA ? 0.707107f : 1.414214f);
    mWinApp->mRemoteControl.SetBeamIncrement(arrowInc);

    // Change percent C2 with Ctrl arrow keys and increment with Ctrl , .
  } else if (mCtrlPressed && (nChar == VK_UP || nChar == VK_DOWN)) {
    mWinApp->mRemoteControl.ChangeIntensityByIncrement(39 - nChar); 

  } else if ((ctrl && !shift) && (nChar == VK_OEM_COMMA || nChar == VK_OEM_PERIOD)) {
    brightInc *= (nChar == VK_OEM_COMMA ? 0.707107f : 1.414214f);
    mWinApp->mRemoteControl.SetIntensityIncrement(brightInc);
     
    // If navigator open let arrows be used to move in list
  } else if (mWinApp->mNavigator && (nChar == VK_UP || nChar == VK_DOWN)) {
    mWinApp->mNavigator->MoveListSelection(nChar == VK_UP ? -1 : 1);

  } else if (navCanProcess && cChar == 'A' && (!mCtrlPressed || mShiftPressed)) {
    mWinApp->mNavigator->ProcessAkey(mCtrlPressed, mShiftPressed);
  } else if (navCanProcess && cChar == 'C' && !mCtrlPressed && !mShiftPressed) {
    mWinApp->mNavigator->ProcessCKey();
  } else if (navCanProcess && cChar == 'R' && !mCtrlPressed && !mShiftPressed) {
    mWinApp->mNavigator->ProcessRKey();
  } else if (navCanProcess && cChar == 'D' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessDKey();
  } else if (navCanProcess && cChar == 'T' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessTKey();
  } else if (navCanProcess && cChar == 'N' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessNKey();
  } else if (navCanProcess && cChar == 'V' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessVKey();
  } else if (navCanProcess && cChar == 'H' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessHKey();

  } else if (cChar == 'X' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mImageLevel.ToggleExtraInfo();
  } else if (cChar == 'H' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mImageLevel.ToggleCrosshairs();

    // Otherwise use arrow keys to adjust offset
  } else if (nChar >= VK_LEFT && nChar <= VK_DOWN) {
    int iDx = 0;
    int iDy = 0;
    if (nChar % 2)
      iDx = nChar - 38;
    else
      iDy = 39 - nChar;
    m_iOffsetX += iDx;
    m_iOffsetY += iDy;
    DrawImage();

  // PageUp, PageDn, Home, End move in multibuffer window
  } else if (nChar >= VK_PRIOR && nChar <= VK_HOME && mImBufNumber > 1) {
    if ((mainOrFFT && nChar == VK_PRIOR) ||  (!mainOrFFT && nChar == VK_NEXT)) {
      // Page up: go toward 0 buffer
      iTrial = mImBufIndex - 1;
      iDir = -1;
    } else if ((!mainOrFFT && nChar == VK_PRIOR) || (mainOrFFT && nChar == VK_NEXT)) {
      // Page down: go toward last buffer
      iTrial = mImBufIndex + 1;
      iDir = 1;
    } else if ((mainOrFFT && nChar == VK_END) || (!mainOrFFT && nChar == VK_HOME)) {
      // End: go to last, search back from there
      iTrial = mImBufNumber - 1;
      iDir = -1;
    } else if ((!mainOrFFT && nChar == VK_END) || (mainOrFFT && nChar == VK_HOME)) {
      // Home: go to first, search forward from there
      iTrial = 0;
      iDir = 1;
    }
    mWinApp->SetBufToggleCount(0);
    TryToChangeBuffer(iTrial, iDir);

    // Insert key to get to Autoalign image
  } else if (nChar == VK_INSERT) {
    mWinApp->SetBufToggleCount(0);
    iBuf = mWinApp->mBufferManager->AutoalignBufferIndex();
    if (mainBufs[iBuf].mImage)
      mWinApp->SetCurrentBuffer(iBuf);

  // Delete key to get to read-in image
  } else if (nChar == VK_DELETE) {
    mWinApp->SetBufToggleCount(0);
    iBuf = mWinApp->mBufferManager->GetBufToReadInto();
    if (iBuf >= 0 && mainBufs[iBuf].mImage)
      mWinApp->SetCurrentBuffer(iBuf);

  // Backspace key to delete in navigator
  } else if (nChar == VK_BACK && mMainWindow && mWinApp->mNavigator) {
    mWinApp->mNavigator->BackspacePressed();

  // Ctrl-B to toggle blank when down
  } else if (cChar == 'B' && GetAsyncKeyState(VK_CONTROL) / 2 &&
    !mWinApp->DoingTasks()) {
    mWinApp->mLowDoseDlg.ToggleBlankWhenDown();

  // Start/stop movie in stack window, adjust speed
  } else if (nChar == 'M' && ctrl && shift && 
    (mWinApp->mStackView || mWinApp->mLastStackView)) {
    ToggleStackMovie();
  } else if (nChar == VK_OEM_PERIOD && ctrl && shift) {
    ChangeMovieInterval(0.707107f);
  } else if (nChar == VK_OEM_COMMA && ctrl && shift) {
    ChangeMovieInterval(1.414214f);
  }
    

  CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

// Catch Alt keys and Alt itself: pass it on for anything but Alt or if nothing is running
void CSerialEMView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
  char cChar = char(nChar);
  SEMTrace('k', "SysKey code %u, char %c", nChar, cChar);
  if (nChar != VK_MENU || 
    (!mWinApp->DoingTasks() && !(mWinApp->mCamera && mWinApp->mCamera->CameraBusy())))
    CView::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

void CSerialEMView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  bool ctrl = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  if (!BOOL_EQUIV(ctrl, mCtrlPressed))
    mWinApp->mRemoteControl.CtrlChanged(ctrl);
  mCtrlPressed = ctrl;
  CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

void CSerialEMView::OnActivateView(BOOL bActivate, CView* pActivateView, 
  CView* pDeactiveView) 
{
  if (bActivate && !mWinApp->mMainFrame->GetClosingProgram()) {
    // Yes it can inform the App that it is in charge; also set scale
    mWinApp->SetActiveView(this);
    NewImageScale();
    NewZoom();
    if (mWinApp->mBufferWindow)
      mWinApp->mBufferWindow.UpdateSaveCopy();
  }
  CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

// Intercept the Erase Background messages and return TRUE to indicate that
// no erasure is needed; we paint the whole window.
BOOL CSerialEMView::OnEraseBkgnd(CDC* pDC) 
{
  return TRUE;
  // return CView::OnEraseBkgnd(pDC);
}

EMimageBuffer *CSerialEMView::GetActiveImBuf()
{
  if (!mImBufs || mImBufIndex < 0 || mImBufIndex >= mImBufNumber)
    return NULL;
  return &mImBufs[mImBufIndex];
}

// Inform the image level dialog of a new scale if this is the active view
void CSerialEMView::NewImageScale()
{
  if (mImBufs[mImBufIndex].mImageScale && mWinApp->mActiveView == this)
    mWinApp->mImageLevel.NewImageScale(mImBufs[mImBufIndex].mImageScale);
}

// Resize to fit client area with borders - should only happen for main and FFT windows
void CSerialEMView::ResizeToFit(int iBordLeft, int iBordTop, int iBordRight, 
                int iBordBottom, int useHalf)
{
  CChildFrame *childFrame = (CChildFrame *)GetParent();
  CMainFrame *mainFrame = (CMainFrame *)childFrame->GetParent();
  CRect rect;
  int useWidth, useLeft, fullWidth;
  mainFrame->GetClientRect(&rect);
  useWidth = fullWidth = rect.Width() - iBordLeft - iBordRight;
  useLeft = rect.left + iBordLeft;
  if (useHalf)
    useWidth = (int)(useWidth * mWinApp->GetMainFFTsplitFrac());
  if (useHalf > 0) {
    useLeft += useWidth;
    useWidth = fullWidth - useWidth;
  }

  mResizingToFit = true;
  childFrame->SetWindowPos(NULL, useLeft, rect.top + iBordTop, useWidth, 
      rect.Height() - iBordTop - iBordBottom, SWP_NOZORDER);
  mResizingToFit = false;
}

// Report a resize event other than one caused by ResizeToFit or one of the first two
// right after opening the FFT window
void CSerialEMView::OnSize(UINT nType, int cx, int cy)
{
  CRect rect;
  float xRatio, yRatio;
  CView::OnSize(nType, cx, cy);
  if (mFFTWindow)
    mFFTresizeCount++;

  // Adjust zoom instead resizing to fit
  if (mLastWinSizeX > 0 && cx > 0 && cy > 0 && mMainWindow) {
    xRatio = (float)cx / mLastWinSizeX;
    yRatio = (float)cy / mLastWinSizeY;
    if ((xRatio >= 1. ? xRatio : 1. / xRatio) < (yRatio >= 1. ? yRatio : 1. / yRatio))
      xRatio = yRatio;
    mZoom *= xRatio;
    mEffectiveZoom *= xRatio;
    mLastWinSizeX = cx;
    mLastWinSizeY = cy;
    for (int ind = 0; ind < MAX_BUFFERS; ind++)
      mImBufs[ind].mZoom *= xRatio;
  }
  
  if (!mResizingToFit && (mMainWindow || mFFTWindow) && 
    !(mFFTWindow && SEMTickInterval(mCreateTime) < 1000. && mFFTresizeCount < 3)) {
    GetWindowRect(&rect);
    mWinApp->MainViewResizing(rect, mFFTWindow);
  }
}

// Try to change to the buffer iTrial, moving in direction iDir
void CSerialEMView::TryToChangeBuffer(int iTrial, int iDir)
{
  if (iTrial < 0)
    iTrial = 0;
  if (iTrial >= mImBufNumber)
    iTrial = mImBufNumber;

  // Take the first buffer in the given direction that has an image
  // If none is found, do not change anything
  if (iTrial != mImBufIndex) {
    while (iTrial >= 0 && iTrial < mImBufNumber) {
      if (mImBufs[iTrial].mImage) {
        SetCurrentBuffer(iTrial);
        break;
      }
      iTrial += iDir;
    }
  }
}

// This entry for setting the current index should take care of
// all necessary update calls.
void CSerialEMView::SetCurrentBuffer(int inIndex)
{
  float binning;
  mImBufIndex = inIndex;
  if (mFirstDraw || mBasisSizeX < 0)
    FindEffectiveZoom();

  // Autozoom: if there is a binning, set the zoom based on the effective zoom
  // times the binning; notify level window of new zoom
  binning = GetBufferBinning();
  if (mWinApp->mBufferManager->GetAutoZoom() && binning > 0.) {

    // But if it is a map with a defined zoom, or View or Search, use previous zoom
    if (mImBufs[mImBufIndex].mMapID && mImBufs[mImBufIndex].mZoom > 0.001)
      mZoom = mImBufs[mImBufIndex].mZoom;
    else if (mImBufs[mImBufIndex].mLowDoseArea &&
      mImBufs[mImBufIndex].mConSetUsed == VIEW_CONSET && mViewEffectiveZoom > 0.) {
      mZoom = mViewEffectiveZoom * binning;
    } else if (mImBufs[mImBufIndex].mLowDoseArea &&
      mImBufs[mImBufIndex].mConSetUsed == SEARCH_CONSET && mSearchEffectiveZoom > 0.) {
      mZoom = mSearchEffectiveZoom * binning;
    } else {
      mZoom = mEffectiveZoom * binning;
    }

    if (mImBufs[mImBufIndex].mImageScale)
      mWinApp->mImageLevel.NewZoom(mZoom);
  }

  mImBufs[mImBufIndex].mZoom = mZoom;

  // Take pan from the buffer for map, from stored value for non-map
  if (mImBufs[mImBufIndex].mMapID) {
    m_iOffsetX = mImBufs[mImBufIndex].mPanX;
    m_iOffsetY = mImBufs[mImBufIndex].mPanY;
  } else {
    m_iOffsetX = mNonMapPanX;
    m_iOffsetY = mNonMapPanY;
  }
  DrawImage();
  if (mMainWindow || mFFTWindow)
    mWinApp->SetImBufIndex(mImBufIndex, mFFTWindow);
  NewImageScale();
  if (mMainWindow)
    mWinApp->UpdateBufferWindows(); 
}

// The effective zoom is the zoom that would make a full-sized unbinned image from the 
// current camera fill the current window size
void CSerialEMView::FindEffectiveZoom()
{
  double critErr = 0.05;  // Criterion error for using a preset zoom
  float binning = 1.;
  CameraParameters *camParam = mWinApp->GetCamParams();
  int bufCam = mImBufs[mImBufIndex].mCamera;
  int curCam = B3DCHOICE(bufCam < 0 || camParam[bufCam].sizeX <= 0,
    mWinApp->GetCurrentCamera(), bufCam);
  int sizeX, sizeY;
  double ratioX, ratioY;
  int i;

  // Get the window rectangle
  CRect rect;
  GetClientRect(&rect);

  // Save basis sizes used for this computation
  mBasisSizeX = camParam[curCam].sizeX;
  mBasisSizeY = camParam[curCam].sizeY;
  mLastWinSizeX = rect.Width();
  mLastWinSizeY = rect.Height();

  // Get the binning if there is an image
  if (mImBufs[mImBufIndex].mImage && GetBufferBinning() > 0.)
    binning = GetBufferBinning();

  // If this is the main view or somehow there is no image, base size on camera
  if (mMainWindow || !mImBufs->mImage) {
    sizeX = mBasisSizeX;
    sizeY = mBasisSizeY;
  } else {
    // Otherwise take the image size times binning
    sizeX = (int)(mImBufs->mImage->getWidth() * binning + 0.5);
    sizeY = (int)(mImBufs->mImage->getHeight() * binning + 0.5);
  }

  // Take the smaller ratio of window to image size as the one to fit
  ratioX = (double)rect.Width() / sizeX;
  ratioY = (double)rect.Height() / sizeY;
  if (ratioY < ratioX)
    ratioX = ratioY;

  // Take a preset zoom if it close enough
  mEffectiveZoom = 0.;
  for (i = 0; i <= MAXZOOMI; i++) {
    double err = ratioX / zoomvals[i] - 1.;
    if (err < 0.)
      err = -err;
    if (err < critErr) {
      mEffectiveZoom = zoomvals[i];
      break;
    }
  }

  // Otherwise take the exact ratio
  if (mEffectiveZoom == 0.)
    mEffectiveZoom = ratioX;

  // Now set the actual zoom too
  mZoom = mEffectiveZoom * binning;
  if (mImBufs[mImBufIndex].mImageScale)
    mWinApp->mImageLevel.NewZoom(mZoom);
  mImBufs[mImBufIndex].mZoom = mZoom;
}

// Get the binning of the buffer to use when multiplying by effective zoom, which may or
// may not be based on effective binning, and is adjusted by ratio between original 
// camera basis size and size of current camera
float CSerialEMView::GetBufferBinning()
{
  float bX, bY;
  EMimageBuffer *imBuf = &mImBufs[mImBufIndex];
  int iCam = imBuf->mCamera < 0 ? mWinApp->GetCurrentCamera() : imBuf->mCamera;
  CameraParameters *camP = mWinApp->GetCamParams();
  bool useEffective = imBuf->IsMontageOverview() || 
    imBuf->mCaptured == BUFFER_AUTOCOR_OVERVIEW ||
    (mWinApp->mNavigator && mWinApp->mNavigator->ImBufIsImportedMap(imBuf)) ||
    (imBuf->mCamera && camP[iCam].STEMcamera && imBuf->mConSetUsed == FOCUS_CONSET &&
    imBuf->mCaptured > 0);
  float binning = (float) (useEffective ? imBuf->mEffectiveBin : imBuf->mBinning);
  bX = (binning * mBasisSizeX) / camP[iCam].sizeX;
  bY = (binning * mBasisSizeY) / camP[iCam].sizeY;
  return bX < bY ? bX : bY;
}

// 7/7/03: Eliminate task to refresh title of main frame


int CSerialEMView::AddBufferToStack(EMimageBuffer * imBuf, int angleOrder)
{
  EMimageBuffer *newBufs;
  int i, ind = mImBufNumber;

  if (!mImBufArraySize)
    return -1;

  // First look for a matching section number
  for (i = 0; i < mImBufNumber; i++) {
    if ((imBuf->mSecNumber >= 0 && imBuf->mSecNumber == mImBufs[i].mSecNumber) ||
      (imBuf->mSecNumber < -1 && i == -imBuf->mSecNumber - 2)) {
      mImBufs[i].DeleteImage();
      delete mImBufs[i].mPixMap;
      delete mImBufs[i].mFiltPixMap;
      delete mImBufs[i].mImageScale;
      ind = i;
      break;
    }
  }

  // If need bigger array, get it, copy elements, clear out old, and delete it
  if (ind >= mImBufArraySize) {
    NewArray(newBufs, EMimageBuffer, mImBufArraySize + 10);

    // If it fails just clean up the imbuf completely
    if (!newBufs) {
      delete imBuf;
      return 1;
    }
    for (i = 0; i < mImBufNumber; i++) {
      newBufs[i] = mImBufs[i];
      mImBufs[i].ClearForDeletion();
    }
    if (mImBufArraySize > 1)
      delete [] mImBufs;
    else
      delete mImBufs;
    mImBufs = newBufs;
    mImBufArraySize += 10;
  }

  // If an angle order is specified, add it before the appropriate one
  if (angleOrder && ind == mImBufNumber) {
    for (i = 0; i < mImBufNumber; i++)
      if (angleOrder * (mImBufs[i].mTiltAngleOrig - imBuf->mTiltAngleOrig) > 0)
        break;
    ind = i;
    for (i = mImBufNumber; i > ind; i--)
      mImBufs[i] = mImBufs[i - 1];
    mImBufNumber++;
  } else if (ind == mImBufNumber)
    mImBufNumber++;

  // Add to the array and clean up imbuf
  mImBufs[ind] = *imBuf;
  SetCurrentBuffer(ind);
  imBuf->ClearForDeletion();
  delete imBuf;
  return 0;
}

// Return the memory usage of all buffers
double CSerialEMView::BufferMemoryUsage(CPtrArray *refArray)
{
  double size, sum = 0.;
  int byteSize, type;
  int *reference;
  int found;
  KImage *image;
  for (int i = 0; i < mImBufNumber; i++) {

    // Loop three time, first with the image then with the pixmaps
    for (int j = 0 ; j < 3; j++) {
      image = NULL;
      if (!j)
        image = mImBufs[i].mImage;
      if (j == 1 && mImBufs[i].mPixMap)
        image = mImBufs[i].mPixMap->getImRectPtr();
      if (j == 2 && mImBufs[i].mFiltPixMap)
        image = mImBufs[i].mFiltPixMap->getImRectPtr();

      if (image) {
        reference = image->getRefPtr();

        // If the reference pointer is non-null, look it up in the array
        found = 0;
        if (reference) {
          for (int k = 0; k < refArray->GetSize(); k++) {
            if (reference == (int *)refArray->GetAt(k)) {
              found = 1;
              break;
            }
          }

          // If not found add it to the array
          if (!found)
            refArray->Add(reference);
        }

        // Count the image if not already found
        if (!found) {
          type = image->getType();
          byteSize  = 2;
          if (type == kUBYTE)
            byteSize = 1;
          if (type == kRGB)
            byteSize = 3;
          if (type == kFLOAT)
            byteSize = 4;
          size = byteSize * image->getWidth() * image->getHeight();
          sum += size;
          /*SEMTrace('1', "buf %d iter %d ref %x type %d size %.2f", i, j, 
            image->getRefPtr(), type, size/1.e6); */
        }
      }
    }
  }
  return sum;
}

// Look for all buffers with matching ID and registrations and change to new registration
void CSerialEMView::ChangeAllRegistrations(int mapID, int fromReg, int toReg)
{
  for (int i = 0; i < mImBufNumber; i++) {
    if (mImBufs[i].mMapID == mapID && mImBufs[i].mRegistration == fromReg)
      mImBufs[i].mRegistration = toReg;
  }
}

// Find the image value at the given point and display it in the given pane
void CSerialEMView::ShowImageValue(KImage * image, int imX, int imY, int pane)
{
  float val;
  int red, green, blue;
  CString message;
  image->Lock();
  if (image->getMode() == kGray) {
    val = image->getPixel(imX, imY);
    message.Format("(%d, %d) = %g", imX, imY, val);
  } else {
    ((KImageRGB *)image)->getPixel(imX, imY, red, green, blue);
    message.Format("(%d, %d) = %d,%d,%d", imX, imY, red, green, blue);
  }
  image->UnLock();
  mWinApp->SetStatusText(pane, message);
}

void CSerialEMView::GetLineLength(EMimageBuffer *imBuf, float &pixels, float &nanometers,
                                  float &angle, bool skipAngleAndjust)
{
  double dx = imBuf->mLineEndX - imBuf->mUserPtX;
  double dy = imBuf->mUserPtY - imBuf->mLineEndY;
  float focusRot;
  pixels = (float)sqrt(dx * dx + dy * dy);
  nanometers = (float)(1000. * mShiftManager->GetPixelSize(imBuf, &focusRot) * 
    pixels);
  angle = 0.;
  if (pixels)
    angle = (float)(atan2(dy, dx) / DTOR) - (skipAngleAndjust ? 0.f : focusRot);
}

void CSerialEMView::GetUserBoxSize(EMimageBuffer *imBuf, int & nx, int & ny, float & xnm,
                                   float & ynm)
{
  int top, left, bottom, right;
  float pixSize;
  left = (int)floor((double)B3DMIN(imBuf->mUserPtX, imBuf->mLineEndX) + 1);
  right = (int)ceil((double)B3DMAX(imBuf->mUserPtX, imBuf->mLineEndX) - 1);
  top = (int)floor((double)B3DMIN(imBuf->mUserPtY, imBuf->mLineEndY) + 1);
  bottom = (int)ceil((double)B3DMAX(imBuf->mUserPtY, imBuf->mLineEndY) - 1);
  nx = right + 1 - left;
  ny = bottom + 1 - top;
  if (nx % 2)
    nx--;
  pixSize = 1000.f * mShiftManager->GetPixelSize(imBuf);
  xnm = (float)(nx * pixSize);
  ynm = (float)(ny * pixSize);
}

// Returns true if the given buffer is part of this view's stack
bool CSerialEMView::IsBufferInStack(EMimageBuffer *imBuf)
{
  for (int ind = 0; ind < mImBufNumber; ind++)
    if (imBuf == &mImBufs[ind])
      return true;
  return false;
}

void CSerialEMView::ToggleStackMovie()
{
  if ((!mStackWindow && mWinApp->mLastStackView != this)|| mImBufNumber < 2)
    return;
  if (mMovieTimerID) {
    CSerialEMView::KillMovieTimer();
  } else {
    mMovieTimerID = ::SetTimer(NULL, 1, mMovieInterval, MovieProc);
  }
}

void CSerialEMView::KillMovieTimer()
{
  ::KillTimer(NULL, mMovieTimerID);
  mMovieTimerID = 0;
}

void CSerialEMView::ChangeMovieInterval(float delta)
{
  mMovieInterval = B3DNINT(mMovieInterval * delta);
  B3DCLAMP(mMovieInterval, 50, 5000);
  mNewMovieInterval = true;
}

static VOID CALLBACK MovieProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  int curInd, lastBuf, dir = CSerialEMView::mMovieDir;
  CSerialEMApp *winApp = (CSerialEMApp *)AfxGetApp();
  CSerialEMView *stack = winApp->mStackView ? winApp->mStackView : winApp->mLastStackView;
  if (!stack) {
    CSerialEMView::KillMovieTimer();
    return;
  }
  if (CSerialEMView::mNewMovieInterval) {
    CSerialEMView::KillMovieTimer();
    CSerialEMView::mMovieTimerID = ::SetTimer(NULL, 1, CSerialEMView::mMovieInterval, 
      MovieProc);
    CSerialEMView::mNewMovieInterval = false;
  }
  curInd = stack->GetImBufIndex();
  lastBuf = stack->GetImBufNumber() - 1;
  if (curInd == 0 && dir < 0 || curInd >= lastBuf && dir > 0)
    CSerialEMView::mMovieDir = -dir;
  stack->SetCurrentBuffer(B3DMAX(0, B3DMIN(lastBuf, curInd + CSerialEMView::mMovieDir)));
}

