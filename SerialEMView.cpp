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
#include "NavHelper.h"
#include "MacroProcessor.h"
#include "MultiTSTasks.h"
#include "RemoteControl.h"
#include "Utilities\KGetOne.h"
#include "Shared\b3dutil.h"
#include "XFolderDialog/XWinVer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define USER_PT_CLICK_TIME 500
#define PAN_THRESH 5
static double zoomvals[] =
{0.01, 0.02, 0.04, 0.06, 0.09, 0.125, 0.185, 0.25, 0.37, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0,
  4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0};

#define MAXZOOMI 21

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
  ON_WM_RBUTTONDOWN()
  ON_WM_LBUTTONUP()
  ON_WM_LBUTTONDBLCLK()
  ON_WM_ERASEBKGND()
  ON_WM_RBUTTONUP()
  ON_WM_HELPINFO()
	ON_WM_DESTROY()
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
  mZoom = 1.0;
  m_iOffsetX = 0;
  m_iOffsetY = 0;
  mNonMapPanX = 0;
  mNonMapPanY = 0;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mFirstDraw = true;
  mMouseShifting = false;
  mMainWindow = false;
  mStackWindow = false;
  mFFTWindow = false;
  mShiftPressed = false;
  mCtrlPressed = false;
  mPanning = false;         
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
  mZoomAroundPoint = false;
  mFlashNextDisplay = false;
  mWheelDeltaPending = 0;
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

  CChildFrame *childFrame = (CChildFrame *)CWnd::FromHandle(cs.hwndParent);
  CMainFrame *mainFrame = (CMainFrame *)childFrame->GetParent();
  CRect rect;
  mainFrame->GetClientRect(&rect);

  type = mWinApp->GetNewViewProperties(this, iBordLeft, iBordTop, iBordRight, iBordBottom,
    mImBufs, mImBufNumber, mImBufIndex);
  if (type == 1) {
  
    // If this is the static window, set the size to fill the client area
    childFrame->SetWindowPos(NULL, rect.left + iBordLeft, rect.top + iBordTop,
      rect.Width() - iBordLeft - iBordRight, 
      rect.Height() - iBordTop - iBordBottom, SWP_NOZORDER);
    childFrame->SetStaticFrame();
    childFrame->SetWindowText("Main Window");
    mMainWindow = true;
  } else if (type == 3) {
    width = (rect.Width() - iBordLeft - iBordRight) / 2;
    childFrame->SetWindowPos(NULL, rect.left + iBordLeft + width, rect.top + iBordTop,
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

  // Get font for drawing letter in corner
  CString fontName;
  fontName = Is2000OrGreater() ? "Microsoft Sans Serif" : "MS Sans Serif";
  mFont.CreateFont(36, 0, 0, 0, FW_MEDIUM,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
    FF_DONTCARE, fontName);
  mLabelFont.CreateFont(24, 0, 0, 0, FW_MEDIUM,
    0, 0, 0, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS,
    CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH |
    FF_DONTCARE, fontName);

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
    mWinApp->ViewClosing(mStackWindow, mFFTWindow);
  CView::OnDestroy();
}

void CSerialEMView::CloseFrame()
{
  CChildFrame *childFrame = (CChildFrame *)GetParent();
  childFrame->OnClose();
}

void CSerialEMView::DrawImage(void)
{
  int iSrcWidth, iSrcHeight, iDestWidth, iDestHeight, crossLen, xSrc, ySrc, needWidth;
  int tmpWidth, tmpHeight, ierr, ifilt, numLines, thick, loop;
  float cenX, cenY, radius, filtMean = 128., filtSD, boost, targetSD = 40.;
  double defocus;
  unsigned char **filtPtrs;
  int zoomFilters[] = {5, 4, 1, 0};  // lanczos3, 2, Blackman, box
  int numZoomFilt = sizeof(zoomFilters) / sizeof(int);
  int zoomWidthCrit = 20;
  bool bufferOK, filtering = false;
  CNavigatorDlg *navigator = mWinApp->mNavigator;
  FloatVec zeroRadii;
  CPoint point;
  CString letString, firstLabel, lastLabel;
  COLORREF bkgColor = RGB(48, 0, 48);
  COLORREF flashColor = RGB(192, 192, 0);
  COLORREF areaColors[3] = {RGB(255, 255, 0), RGB(255, 0, 0), RGB(0, 255, 0)};

  // If this is the first draw, set up the zoom optimally
  // 7/7/03: no longer need to fix main frame window title; set child titles
  if (mFirstDraw) {
    FindEffectiveZoom();
    CChildFrame *childFrame = (CChildFrame *)GetParent();
    mFirstDraw = false;
    if (mFFTWindow)
      ((CMDIChildWnd *)(mWinApp->mMainView->GetParent()))->MDIActivate();
  }
 
  // Get the window rectangle
  CRect rect;
  GetClientRect(&rect);

  // Get a device context for doing fill rectangles
  CClientDC cdc(this);

  EMimageBuffer *imBuf = NULL;
  KImage      *imageRect;
  if (mImBufs) {
    imBuf = &mImBufs[mImBufIndex];
    imageRect = imBuf->mImage;
  }

  // If there is no image, fill the whole area
  if (mFlashNextDisplay || imBuf == NULL || imageRect == NULL) {
    cdc.FillSolidRect(rect, mFlashNextDisplay ? flashColor : bkgColor);
    mFlashNextDisplay = false;
    return;
  }

  // Get the device context for the bitmap drawing calls
  HDC hdc = ::GetDC(m_hWnd); // handle to device context

  // Update the pixmap and its scaling and image data
  imBuf->UpdatePixMap();
  KPixMap *pixMap = imBuf->mPixMap;

  // Loop twice if zooming around point; get offset first time, then determine image
  // position of the same window position and adjust offset to bring image back there
  for (loop = 0; loop < (mZoomAroundPoint ? 2 : 1) ; loop ++) {
    if (loop && mZoomAroundPoint && 
      ConvertMousePoint(&rect, imageRect, &mZoomupPoint, cenX, cenY)) {
        m_iOffsetX += B3DNINT(cenX - mZoomupImageX);
        m_iOffsetY -= B3DNINT(cenY - mZoomupImageY);
    }

    // Set the image and window offsets for drawing, given sizes and mouse shifts
    // This routine thinks these things are bottom-based
    b3dSetImageOffset(rect.Width(), imageRect->getWidth(), mZoom, &iSrcWidth,
      &m_iOffsetX, &mXDest, &mXSrc);
    mXDest += rect.left;
    b3dSetImageOffset(rect.Height(), imageRect->getHeight(), mZoom, &iSrcHeight, 
      &m_iOffsetY, &mYDest, &mYSrc);
  }

  mZoomAroundPoint = false;
  if (imBuf->mMapID) {
    imBuf->mPanX = m_iOffsetX;
    imBuf->mPanY = m_iOffsetY;
  } else {
    mNonMapPanX = m_iOffsetX;
    mNonMapPanY = m_iOffsetY;
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
        imBuf->mFiltPixMap->useRect(filtImage);
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
        imBuf->mImageScale->GetContrast(), imBuf->mImageScale->GetInverted(), boost,
        filtMean);
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

  ::ReleaseDC(m_hWnd, hdc);

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
  if (mMouseShifting || mWinApp->mBufferManager->GetDrawCrosshairs() || 
    (navigator && navigator->MovingMapItem())) {
    CPen pnSolidPen (PS_SOLID, 1, mWinApp->mShiftManager->GetShiftingDefineArea() ?
      RGB(0, 255, 0) : RGB(255, 0, 0));
    crossLen = (rect.Width() < rect.Height() ? rect.Width() : rect.Height()) / 5;
    CPoint point = rect.CenterPoint();
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
  if (imBuf->mHasUserPt || imBuf->mHasUserLine) {
    CPen pnSolidPen (PS_SOLID, 2, RGB(0, 255, 0));
    CPoint point, point2;
    crossLen = imBuf->mHasUserPt ? 7 : 1;
    MakeDrawPoint(&rect, imBuf->mImage, imBuf->mUserPtX, imBuf->mUserPtY, &point);
    if (!imBuf->mDrawUserBox) 
      DrawCross(&cdc, &pnSolidPen, point, crossLen);
    if (!imBuf->mHasUserPt) {
      MakeDrawPoint(&rect, imBuf->mImage, imBuf->mLineEndX, imBuf->mLineEndY, &point2);
      if (imBuf->mDrawUserBox) {
        int left = B3DMIN(point.x, point2.x);
        int right = B3DMAX(point.x, point2.x);
        int top = B3DMIN(point.y, point2.y);
        int bot = B3DMAX(point.y, point2.y);
        cdc.Draw3dRect(left, top, right + 1 - left, bot + 1 - top, RGB(0, 255, 0),
          RGB(0, 255, 0));
      } else {
        DrawCross(&cdc, &pnSolidPen, point2, crossLen);
        CPen pnSolid1 (PS_SOLID, 1, RGB(0, 255, 0));
        CPen *pOldPen = cdc.SelectObject(&pnSolid1);
        cdc.MoveTo(point);
        cdc.LineTo(point2);
        cdc.SelectObject(pOldPen);
      }
    }
  }

  // Draw letter in corner if this is main window, or number for multibuffer window
  int scaleCrit = mWinApp->mBufferManager->GetDrawScaleBar();
  if (mImBufNumber > 1 || mImBufArraySize) {
    CFont *def_font = cdc.SelectObject(&mFont);
    int defMode = cdc.SetBkMode(TRANSPARENT);
    COLORREF defColor = cdc.SetTextColor(RGB(255, 0, 0));
    if (mMainWindow || mFFTWindow) {
      char letter = 'A' + mImBufIndex;
      letString = letter;
      if (mFFTWindow)
        letString += "F";
      cdc.TextOut(5, 5, letString);
      if (imBuf->mCaptured > 0 && imBuf->mConSetUsed >= 0 && 
        imBuf->mConSetUsed < NUMBER_OF_USER_CONSETS && imBuf->mLowDoseArea) {
          cdc.SelectObject(&mLabelFont);
          letString = CString(" - ") + (mWinApp->GetModeNames())[imBuf->mConSetUsed];
          cdc.TextOut(25, 10, letString);
      }

      bufferOK = !imBuf->IsProcessed() && (imBuf->mCaptured > 0 || 
        imBuf->mCaptured == BUFFER_CALIBRATION || imBuf->mCaptured == BUFFER_TRACKING ||
        imBuf->mCaptured == BUFFER_MONTAGE_CENTER || imBuf->mCaptured == BUFFER_ANCHOR ||
        imBuf->mCaptured == BUFFER_MONTAGE_PIECE);
      if (mMainWindow && imBuf->mCamera >= 0 && scaleCrit > 0 && (bufferOK ||
        mWinApp->GetNumActiveCameras() > 1)) {
          cdc.SetTextColor(RGB(0, 255, 40));
          cdc.SelectObject(&mLabelFont);
          CameraParameters *camP = mWinApp->GetCamParams() + imBuf->mCamera;
          if (imBuf->mSampleMean > EXTRA_VALUE_TEST && bufferOK &&
            !mWinApp->mProcessImage->DoseRateFromMean(imBuf, imBuf->mSampleMean, boost)) {
              if (mWinApp->mCamera->IsDirectDetector(camP)) {
                letString.Format("%.2f e/ubpix/s at camera", boost);
                cdc.TextOut(140, 10, letString);
              } else if (mWinApp->mScope->GetNoScope() && imBuf->mMagInd > 0) {
                letString.Format("%.3f e/sq A at camera", boost / 
                  pow(10000. * mWinApp->mShiftManager->GetPixelSize(imBuf), 2.));
                cdc.TextOut(140, 10, letString);
              }
          }
          if (mWinApp->GetNumActiveCameras() > 1)
            cdc.TextOut(420, 10, camP->name);
      }
    } else {
      cdc.SelectObject(&mLabelFont);
      if (mImBufs[mImBufIndex].mSecNumber < 0)
        letString.Format("%d: tilt = %.1f", mImBufIndex + 1, 
        mImBufs[mImBufIndex].mTiltAngleOrig);
      else
        letString.Format("%d: Z = %d", mImBufIndex + 1, mImBufs[mImBufIndex].mSecNumber);
      cdc.TextOut(5, 5, letString);
    }
    cdc.SelectObject(def_font);
    cdc.SetTextColor(defColor);
    cdc.SetBkMode(defMode);
  }

  if (mMainWindow) {

    // If this is a view image in low dose, draw record and trial/focus areas
    if (imBuf->mCaptured > 0 && imBuf->mConSetUsed == 0 && imBuf->mLowDoseArea) {
      float cornerX[4], cornerY[4];
      for (int type  = 0; type < 2; type++) {
        int area = mWinApp->mLowDoseDlg.DrawAreaOnView(type, imBuf,
          cornerX, cornerY, cenX, cenY, radius);
        if (area) {
          CPen pnSolidPen (PS_SOLID, 1, areaColors[area - 1]);
          CPen *pOldPen = cdc.SelectObject(&pnSolidPen);
          for (int pt = 0; pt < 5; pt++) {
            MakeDrawPoint(&rect, imBuf->mImage, cornerX[pt % 4], cornerY[pt % 4], &point);
            if (pt)
              cdc.LineTo(point);
            else
              cdc.MoveTo(point);
          }
          cdc.SelectObject(pOldPen);

          // Draw circle around center for area being defined
          if (!type)
            DrawCircle(&cdc, &pnSolidPen, &rect, imBuf->mImage, cenX, cenY, radius);
        }
      }
    }
  }

  // Draw rings at zeros on an FFT if clicked and configured
  if ((mMainWindow || mFFTWindow) && mWinApp->mProcessImage->GetFFTZeroRadiiAndDefocus(
    imBuf, &zeroRadii, defocus)) {
      cenX = imBuf->mImage->getWidth() / 2.f;
      cenY = imBuf->mImage->getHeight() / 2.f;
      CPen pnDashPen (PS_DASHDOT, 1, RGB(0, 255, 0));
      for (int zr = 0; zr < (int)zeroRadii.size(); zr++)
        DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, 
        zeroRadii[zr] * cenX);

  // Otherwise Draw circle on Live FFT
  } else if ((mMainWindow || mFFTWindow) && imBuf->mCaptured == BUFFER_LIVE_FFT && 
    mWinApp->mProcessImage->GetCircleOnLiveFFT()) {
      float *radii = mWinApp->mProcessImage->GetFFTCircleRadii();
      float pixel = 1000.f * mWinApp->mShiftManager->GetPixelSize(imBuf);
      double defocus = mWinApp->mProcessImage->GetFixedRingDefocus();
      cenX = imBuf->mImage->getWidth() / 2.f;
      cenY = imBuf->mImage->getHeight() / 2.f;
      CPen pnDashPen (PS_DASHDOT, 1, RGB(0, 255, 0));
      if (defocus > 0 && pixel > 0.) {
        mWinApp->mProcessImage->DefocusFromPointAndZeros(0., 0, pixel, &zeroRadii, 
          defocus);
        for (int zr = 0; zr < (int)zeroRadii.size(); zr++)
          DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, 
            zeroRadii[zr] * cenX);
      } else {
        for (int ir = 0; ir < mWinApp->mProcessImage->GetNumCircles(); ir++)
          DrawCircle(&cdc, &pnDashPen, &rect, imBuf->mImage, cenX, cenY, radii[ir] * 
            cenX);
      }
  }

  // Scale bar if window big enough
  if (scaleCrit > 0 && rect.Width() >= scaleCrit && imBuf->mCamera >= 0 &&
    imBuf->mMagInd && imBuf->mBinning && !imBuf->IsProcessed() && imBuf->mCaptured != 0 
    && imBuf->mCaptured != BUFFER_STACK_IMAGE)
    DrawScaleBar(&cdc, &rect, imBuf);

  if (!navigator || mFFTWindow || imBuf->mCaptured == BUFFER_FFT || 
    imBuf->mCaptured == BUFFER_LIVE_FFT || imBuf->mCaptured == BUFFER_AUTOCOR_OVERVIEW)
    return;

  // Now draw navigator items
  float ptX, ptY, lastStageX, lastStageY, labelDistThresh = 40.;
  int lastGroupID = -1, lastGroupSize, size, numPoints;
  CArray<CMapDrawItem *,CMapDrawItem *> *itemArray = GetMapItemsForImageCoords
    (imBuf, false);
  if (!itemArray)
    return;
  int regMatch = imBuf->mRegistration ? 
    imBuf->mRegistration : navigator->GetCurrentRegistration();
  std::set<int> *selectedItems = navigator->GetSelectedItems();
  bool highlight, draw;
  CMapDrawItem *item;
  int currentIndex = navigator->GetCurrentOrAcquireItem(item);
  int currentGroup = (currentIndex >= 0 && item != NULL) ? item->mGroupID : -1;
  int groupThresh = mWinApp->mNavHelper->GetPointLabelDrawThresh();
  for (int iDraw = -1; iDraw < itemArray->GetSize(); iDraw++) {
    if (iDraw < 0) {
      if (!mAcquireBox)
        continue;
      item = mAcquireBox;
      thick = 1;
      highlight = false;
      numPoints = B3DMIN(5, item->mNumPoints);
    } else {
      item = itemArray->GetAt(iDraw);
      thick = item->mType == ITEM_TYPE_POINT ? 3 : 2;
      highlight = selectedItems->count(iDraw) > 0;
      thick = highlight ? thick : 1;
      numPoints = item->mNumPoints;
    }

    if (!item->mNumPoints || !item->mDraw || 
      (item->mRegistration != regMatch && !mDrawAllReg && iDraw >= 0))
      continue;
    SetStageErrIfRealignedOnMap(imBuf, item);
    int crossLen = item->mType == ITEM_TYPE_POINT ? 9 : 5;
    CPen pnSolidPen(PS_SOLID, thick, item->GetColor(highlight));
    if (item->mNumPoints == 1) {

      // Draw a cross at a single point
      StageToImage(imBuf, item->mPtX[0], item->mPtY[0], ptX, ptY, item->mPieceDrawnOn);
      MakeDrawPoint(&rect, imBuf->mImage, ptX, ptY, &point);
      DrawCross(&cdc, &pnSolidPen, point, crossLen);
    } else {

      int adjSave = mAdjustPt;
      float delPtX = 0., delPtY = 0.;

      // For a map, find the adjust that applies to the center, turn off adjustment for
      // the corner points, and adjust them all by the center adjustment to get a square
      if (item->mType == ITEM_TYPE_MAP) {
        StageToImage(imBuf, item->mStageX, item->mStageY, delPtX, delPtY);
        mAdjustPt = -1;
        StageToImage(imBuf, item->mStageX, item->mStageY, ptX, ptY);
        delPtX -= ptX;
        delPtY -= ptY;
      }

      // Draw lines if there is more than one point
      CPen *pOldPen = cdc.SelectObject(&pnSolidPen);
      for (int pt = 0; pt < numPoints; pt++) {
        StageToImage(imBuf, item->mPtX[pt], item->mPtY[pt], ptX, ptY);
        MakeDrawPoint(&rect, imBuf->mImage, ptX + delPtX, ptY + delPtY, &point);
        if (pt)
          cdc.LineTo(point);
        else
          cdc.MoveTo(point);
      }
      if (iDraw < 0 && mWinApp->mNavHelper->GetEnableMultiShot()) {
        MultiShotParams *msParams = mWinApp->mNavHelper->GetMultiShotParams();
        float beamRad;
        StageToImage(imBuf, item->mStageX, item->mStageY, cenX, cenY);
        StageToImage(imBuf, item->mPtX[item->mNumPoints - 1], 
          item->mPtY[item->mNumPoints - 1], ptX, ptY);
        beamRad = (float)sqrt((double)(ptX - cenX) * (ptX - cenX) + 
          (ptY - cenY) * (ptY - cenY));
        CPen circlePen(PS_SOLID, 1, COLORREF(RGB(0, 255, 0)));
        for (int pt = numPoints; pt < item->mNumPoints - 1; pt++) {
          StageToImage(imBuf, item->mPtX[pt], item->mPtY[pt], ptX, ptY);
          DrawCircle(&cdc, &circlePen, &rect, imBuf->mImage, ptX, ptY, beamRad);
        }
        if (msParams->doCenter)
          DrawCircle(&cdc, &circlePen, &rect, imBuf->mImage, cenX, cenY, beamRad);
      }
      cdc.SelectObject(pOldPen);
      mAdjustPt = adjSave;
    }

    // Draw the label if any
    if (iDraw >= 0 && !item->mLabel.IsEmpty() && navigator->m_bDrawLabels) {

      // The rule is to draw first and last in a group and up to 5 around the current
      // point if the group is above the threshold size
      draw = true;
      if (groupThresh > 0 && item->mType == ITEM_TYPE_POINT && (iDraw < currentIndex - 2
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
          item->mLabel == lastLabel || 
          (iDraw > 0 && sqrt(pow((double)lastStageX - item->mStageX, 2.) + 
          pow((double)lastStageY - item->mStageY, 2.)) > labelDistThresh);
      }
      if (draw) {
        StageToImage(imBuf, item->mStageX, item->mStageY, ptX, ptY, item->mPieceDrawnOn);
        MakeDrawPoint(&rect, imBuf->mImage, ptX, ptY, &point);
        if (item->mNumPoints == 1)
          point = point + CPoint(10, 0);
        CFont *def_font = cdc.SelectObject(&mLabelFont);
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

}

void CSerialEMView::MakeDrawPoint(CRect *rect, KImage *image, float inX, float inY, 
                                  CPoint *point)
{
  float shiftX, shiftY;
  image->getShifts(shiftX, shiftY);

  // Need to convert from image coordinates to pixmap coordinate, hence add shifts
  // Need to invert the Y pixmap coordinate, apply equation, then invert resulting
  // Y window coordinate
  float pixMapY = image->getHeight() - (inY + shiftY);
  point->x = (int)((inX + shiftX - mXSrc) * mZoom + mXDest);
  point->y = rect->Height() - 1 - (int)((pixMapY- mYSrc) * mZoom + mYDest) +
    ZoomedPixelYAdjustment(rect, image);
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
CArray<CMapDrawItem *, CMapDrawItem *> *CSerialEMView::GetMapItemsForImageCoords(
  EMimageBuffer *imBuf, bool deleteAcqBox)
{
  CArray<CMapDrawItem *,CMapDrawItem *> *itemArray = mWinApp->mNavigator->GetMapDrawItems
    (imBuf, mAmat, mDelX, mDelY, mDrawAllReg, &mAcquireBox);
  if (deleteAcqBox)
    delete mAcquireBox;
  if (!itemArray)
    return NULL;
  mAdjustPt = mWinApp->mNavigator->PrepareMontAdjustments(imBuf, mRmat, mRinv, mRdelX,
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
                                 float &outY, int pcInd)
{
  MiniOffsets *mini = imBuf->mMiniOffsets;
  float testX, testY;
  int ixPiece, iyPiece, index, ind2, ix, iy, minDist, dist;
  outX = mAmat.xpx * (inX - mStageErrX) + mAmat.xpy * (inY - mStageErrY) + mDelX;
  outY = mAmat.ypx * (inX - mStageErrX) + mAmat.ypy * (inY - mStageErrY) + mDelY;
  if (mAdjustPt < 0)
    return;
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

// Draw a cross with the given pen and length
void CSerialEMView::DrawCross(CClientDC *cdc, CPen *pNewPen, CPoint point, int crossLen)
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
void CSerialEMView::DrawCircle(CClientDC *cdc, CPen *pNewPen, CRect *rect, KImage *image,
                               float cenX, float cenY, float radius)
{
  CPoint point;
  float ptX, ptY;
  CPen *pOldPen = cdc->SelectObject(pNewPen);
  for (int pt = 0; pt < 91; pt++) {
    ptX = (float)(cenX + radius * cos(DTOR * pt * 4));
    ptY = (float)(cenY + radius * sin(DTOR * pt * 4));
    MakeDrawPoint(rect, image, ptX, ptY, &point);
    if (pt)
      cdc->LineTo(point);
    else
      cdc->MoveTo(point);
  }

  cdc->SelectObject(pOldPen);
}

// Draw a scale bar in appropriate images (code doctored from 3dmod)
void CSerialEMView::DrawScaleBar(CClientDC *cdc, CRect *rect, EMimageBuffer *imBuf)
{
  double expon, minlen, loglen, normlen, custlen, pixsize;
  float truelen;
  int xst, yst, pixlen, xsize, ysize;
  int winx = rect->Width();
  int winy = rect->Height();
  CString label;

  if (!mScaleParams.draw)
    return;
  pixsize = mWinApp->mShiftManager->GetPixelSize(imBuf);

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
  CSize labsize = cdc->GetTextExtent(label);

  CFont *def_font = cdc->SelectObject(&mFont);
  int defMode = cdc->SetBkMode(TRANSPARENT);
  COLORREF defColor = cdc->SetTextColor(RGB(0, 0, 0));
  //cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 4, yst - ysize - labsize.cy, label);
  cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 20, yst - ysize - labsize.cy - 10, label);
  cdc->SetTextColor(RGB(255, 255, 255));
  //cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 1, yst - ysize - labsize.cy - 3, label);
  cdc->TextOut(xst + xsize / 2 - labsize.cx / 2 - 18, yst - ysize - labsize.cy - 12, label);
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
      /* printf("ds do offset wo %d %d %d %d\n", *drawsize, *doff, *offset, *woff); */
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
      
      return;
    }
    if (*drawsize < (imsize-1)) (*drawsize)++;
  }
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
  DrawImage();
  if (mImBufs[mImBufIndex].mImageScale)
    mWinApp->mImageLevel.NewZoom(mZoom);
  
  // Set effective zoom unless it is a map
  if (GetBufferBinning() > 0. && !mImBufs[mImBufIndex].mMapID)
    mEffectiveZoom = mZoom / GetBufferBinning();
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

void CSerialEMView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
  if (mWinApp->mNavigator && !(GetAsyncKeyState(VK_SHIFT) / 2) && 
    !(GetAsyncKeyState(VK_CONTROL) / 2) && mWinApp->mNavigator->InEditMode())
      mWinApp->mNavigator->MouseDoubleClick(VK_LBUTTON);
}

// Release mouse if it was captured by this window
void CSerialEMView::OnLButtonUp(UINT nFlags, CPoint point) 
{
  float shiftX, shiftY, angle, pixScale;
  int imX, imY;
  double defocus;
  BOOL legal, used = false;
  CString lenstr;
  EMimageBuffer *imBuf;
  CRect rect;
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

      // Navigator gets first crack if it is either a legal point or a valid hit for
      // selection
      if (mWinApp->mNavigator && (legal || (!(GetAsyncKeyState(VK_SHIFT) / 2) &&
        (GetAsyncKeyState(VK_CONTROL) / 2 || mWinApp->mNavigator->InEditMode()))))
          mNavUsedLastLButtonUp = mWinApp->mNavigator->UserMousePoint(imBuf, shiftX,
            shiftY, mPointNearImageCenter, VK_LBUTTON);
      if (legal && !mNavUsedLastLButtonUp) {
          
        // If the point is legal and was not used by navigator, commit it and output 
        // stats.  But first see if low dose needs to snap it to axis and act on it
        mWinApp->mLowDoseDlg.UserPointChange(shiftX, shiftY, imBuf);
        if (mMainWindow || mFFTWindow)
          mWinApp->mProcessImage->ModifyFFTPointToFirstZero(imBuf, shiftX, shiftY);
        imBuf->mHasUserPt = true;
        imBuf->mHasUserLine = false;
        imBuf->mDrawUserBox = false;
        imBuf->mUserPtX = shiftX;
        imBuf->mUserPtY = shiftY;
        mWinApp->mLowDoseDlg.Update();
        if ((mMainWindow || mFFTWindow) && 
          mWinApp->mProcessImage->GetFFTZeroRadiiAndDefocus(imBuf, NULL, defocus)) {
            lenstr.Format("Defocus -%.2f um", defocus);
            if (mWinApp->mProcessImage->GetPlatePhase() > 0.001)
              lenstr += "  (PP)";
            mWinApp->SetStatusText(MEDIUM_PANE, lenstr);
        } else {
          imX = (int)imBuf->mUserPtX;
          imY = (int)imBuf->mUserPtY;
          ShowImageValue(imBuf->mImage, imX, imY, MEDIUM_PANE);
        }
        if (mWinApp->mNavigator)
          mWinApp->mNavigator->UpdateAddMarker();
      }
      if (legal || used)
        DrawImage();
    } else if (mImBufs && mDrawingLine) {

      // If done drawing a line, output the length to log window
      imBuf = &mImBufs[mImBufIndex];
      if (!imBuf->mDrawUserBox) {
        GetLineLength(imBuf, shiftX, shiftY, angle);
        if (shiftY) {
          pixScale = shiftY >= 1000. ? 0.001f : 1.f;
          lenstr.Format("Drawn line is %.1f pixels, " + mWinApp->PixelFormat(shiftY, 
            pixScale) + "   at %.1f degrees", shiftX, shiftY * pixScale, angle);
        } else
          lenstr.Format("Drawn line is %.1f pixels    at %.1f degrees", shiftX, angle);
        mWinApp->AppendToLog(lenstr, LOG_SWALLOW_IF_CLOSED);
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
  float shiftX, shiftY;
  EMimageBuffer *imBuf;
  CRect rect;
  if (GetAsyncKeyState(VK_CONTROL) / 2 && !(GetAsyncKeyState(VK_SHIFT) / 2)) {
    SetupZoomAroundPoint(&point);
    ZoomUp();
  } else if (mWinApp->mNavigator && mWinApp->mNavigator->TakingMousePoints() && mImBufs &&
    !(GetAsyncKeyState(VK_SHIFT) / 2)) {
      imBuf = &mImBufs[mImBufIndex];
      GetClientRect(&rect);
      if (ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY)) {
        mWinApp->mNavigator->UserMousePoint(imBuf, shiftX, shiftY, mPointNearImageCenter, 
          VK_MBUTTON);
        DrawImage();
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
      mWinApp->mShiftManager->StartMouseShifting(shiftKey, mImBufIndex);
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
      mWinApp->mShiftManager->EndMouseShifting(mImBufIndex);
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
  float shiftX, shiftY, prevX, prevY, angle, pixScale;
  BOOL shiftKey = ::GetAsyncKeyState(VK_SHIFT) / 2 != 0;
  BOOL ctrlKey = ::GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  EMimageBuffer *imBuf;
  CRect rect;
  CString lenstr;
  if (mImBufs != NULL && GetCapture() == this && (!ctrlKey || shiftKey)) {
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
              mWinApp->mShiftManager->StartMouseShifting(shiftKey, mImBufIndex);
              mMouseShifting = true;
            }
            mWinApp->mShiftManager->SetAlignShifts((float)iDx, (float)iDy, true, 
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
      if ((iDx || iDy) && (mPanning || mDrawingLine || 
        GetTickCount() - mMouseDownTime > USER_PT_CLICK_TIME || 
        pDx < -PAN_THRESH || pDx > PAN_THRESH || 
        pDy < -PAN_THRESH || pDy > PAN_THRESH)) {

        // Start or continue panning if panning already or no shift key
        GetClientRect(&rect);
        imBuf = &mImBufs[mImBufIndex];
        if (mPanning || (!mDrawingLine && !shiftKey)) {
          mPanning = true;
          m_iOffsetX += iDx;
          m_iOffsetY += iDy;
          m_iPrevMX += B3DNINT(zoomFac * iDx);
          m_iPrevMY -= B3DNINT(zoomFac * iDy);
          DrawImage();
        } else if (ConvertMousePoint(&rect, imBuf->mImage, &point, shiftX, shiftY)) {

          // Otherwise start or continue drawing a line if point is within image
          if (!mDrawingLine) {
            CPoint prev(m_iPrevMX, m_iPrevMY);
            if (!ConvertMousePoint(&rect, imBuf->mImage, &prev, prevX, prevY)) {
              prevX = shiftX;
              prevY = shiftY;
            }
            mDrawingLine = true;
            imBuf->mHasUserPt = false;
            imBuf->mHasUserLine = true;
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
  } else if (!mWinApp->DoingComplexTasks() || !mWinApp->DoingTasks()) {
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
  int clicksPerZoom = 3, deltaSign = -1;
  double stepZoom, lastStep;
  int numClicks, step = 1;
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
  SEMTrace('k', "Code %u, char %c", nChar, cChar);
  mWinApp->mMacroProcessor->SetKeyPressed((int)cChar);

  // Keep track of ctrl and shift (ONLY IF WE GET KEYUP TOO)
  /*if (nChar == VK_CONTROL)
    mCtrlPressed = true;
  if (nChar == VK_SHIFT)
    mShiftPressed = true; */
  mCtrlPressed = GetAsyncKeyState(VK_CONTROL) / 2 != 0;
  mShiftPressed = GetAsyncKeyState(VK_SHIFT) / 2 != 0;

  // Zoom up and down with these non standard keys
  if (nChar == VK_MINUS) {
    ZoomDown();
  } else if (nChar == VK_EQUALS) {
    ZoomUp();

    // Invert contrast
  } else if (nChar == VK_F11) {
    mWinApp->mImageLevel.ToggleInvertContrast();

    // Beam shift with shifted arrow keys; change increment with < or >
  } else if (mShiftPressed && nChar >= VK_LEFT && nChar <= VK_DOWN) {
    if (nChar % 2)
      fdx = (float)((nChar - 38.) * arrowInc);
    else
      fdy = (float)((39. - nChar) * arrowInc);
    mWinApp->mProcessImage->MoveBeamByCameraFraction(fdx, fdy);

  } else if (mShiftPressed && (nChar == VK_OEM_COMMA || nChar == VK_OEM_PERIOD)) {
    arrowInc *= (nChar == VK_OEM_COMMA ? 0.707107f : 1.414214f);
    mWinApp->mRemoteControl.SetBeamIncrement(arrowInc);

    // Change percent C2 with Ctrl arrow keys and increment with Ctrl , .
  } else if (mCtrlPressed && (nChar == VK_UP || nChar == VK_DOWN)) {
    mWinApp->mRemoteControl.ChangeIntensityByIncrement(39 - nChar); 

  } else if (mCtrlPressed && (nChar == VK_OEM_COMMA || nChar == VK_OEM_PERIOD)) {
    brightInc *= (nChar == VK_OEM_COMMA ? 0.707107f : 1.414214f);
    mWinApp->mRemoteControl.SetIntensityIncrement(brightInc);
     
    // If navigator open let arrows be used to move in list
  } else if (mWinApp->mNavigator && (nChar == VK_UP || nChar == VK_DOWN)) {
    mWinApp->mNavigator->MoveListSelection(nChar == VK_UP ? -1 : 1);

  } else if (mWinApp->mNavigator && cChar == 'A' && (!mCtrlPressed || mShiftPressed)) {
    mWinApp->mNavigator->ProcessAkey(mCtrlPressed, mShiftPressed);
  } else if (mWinApp->mNavigator && cChar == 'C' && !mCtrlPressed && !mShiftPressed) {
    mWinApp->mNavigator->ProcessCKey();
  } else if (mWinApp->mNavigator && cChar == 'D' && !mCtrlPressed && mShiftPressed) {
    mWinApp->mNavigator->ProcessDKey();
 
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
    TryToChangeBuffer(iTrial, iDir);

    // Insert key to get to Autoalign image
  } else if (nChar == VK_INSERT) {
    iBuf = mWinApp->mBufferManager->AutoalignBufferIndex();
    if (mainBufs[iBuf].mImage)
      mWinApp->SetCurrentBuffer(iBuf);

  // Delete key to get to read-in image
  } else if (nChar == VK_DELETE) {
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
  }

  CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CSerialEMView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
  if (bActivate) {
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
  if (!mImBufs)
    return NULL;
  return &mImBufs[mImBufIndex];
}

// Inform the image level dialog of a new scale if this is the active view
void CSerialEMView::NewImageScale()
{
  if (mImBufs[mImBufIndex].mImageScale && mWinApp->mActiveView == this)
    mWinApp->mImageLevel.NewImageScale(mImBufs[mImBufIndex].mImageScale);
}

// Resize to fit client area with borders - should only happen for main window
void CSerialEMView::ResizeToFit(int iBordLeft, int iBordTop, int iBordRight, 
                int iBordBottom, int useHalf)
{
  CChildFrame *childFrame = (CChildFrame *)GetParent();
  CMainFrame *mainFrame = (CMainFrame *)childFrame->GetParent();
  CRect rect;
  int useWidth, useLeft;
  mainFrame->GetClientRect(&rect);
  useWidth = rect.Width() - iBordLeft - iBordRight;
  useLeft = rect.left + iBordLeft;
  if (useHalf)
    useWidth /= 2;
  if (useHalf > 0)
    useLeft += useWidth;

  mFirstDraw = true;
  childFrame->SetWindowPos(NULL, useLeft, rect.top + iBordTop, useWidth, 
      rect.Height() - iBordTop - iBordBottom, SWP_NOZORDER);
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
  mImBufIndex = inIndex;
  if (mFirstDraw || mBasisSizeX < 0)
    FindEffectiveZoom();

  // Autozoom: if there is a binning, set the zoom based on the effective zoom
  // times the binning; notify level window of new zoom
  if (mWinApp->mBufferManager->GetAutoZoom() && GetBufferBinning() > 0.) {

    // But if it is a map with a defined zoom, use previous zoom
    if (mImBufs[mImBufIndex].mMapID && mImBufs[mImBufIndex].mZoom > 0.001)
      mZoom = mImBufs[mImBufIndex].mZoom;
    else
      mZoom = mEffectiveZoom * GetBufferBinning();

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


void CSerialEMView::FindEffectiveZoom()
{
  double critErr = 0.05;  // Criterion error for using a preset zoom
  float binning = 1.;
  CameraParameters *camParam = mWinApp->GetCamParams();
  int curCam = B3DCHOICE(mImBufs[mImBufIndex].mCamera < 0, mWinApp->GetCurrentCamera(),
    mImBufs[mImBufIndex].mCamera);
  int sizeX, sizeY;
  double ratioX, ratioY;
  int i;

  // Get the window rectangle
  CRect rect;
  GetClientRect(&rect);

  // Save basis sizes used for this computation
  mBasisSizeX = camParam[curCam].sizeX;
  mBasisSizeY = camParam[curCam].sizeY;

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


int CSerialEMView::AddBufferToStack(EMimageBuffer * imBuf)
{
  EMimageBuffer *newBufs;
  int i, ind = mImBufNumber;

  if (!mImBufArraySize)
    return -1;

  // First look for a matching section number
  for (i = 0; i < mImBufNumber; i++) {
    if (imBuf->mSecNumber >= 0 && imBuf->mSecNumber == mImBufs[i].mSecNumber) {
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

  // Add to the array and clean up imbuf
  mImBufs[ind] = *imBuf;
  if (ind == mImBufNumber)
    mImBufNumber++;
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
                                  float &angle)
{
  double dx = imBuf->mLineEndX - imBuf->mUserPtX;
  double dy = imBuf->mUserPtY - imBuf->mLineEndY;
  pixels = (float)sqrt(dx * dx + dy * dy);
  nanometers = (float)(1000. * mWinApp->mShiftManager->GetPixelSize(imBuf) * pixels);
  angle = 0.;
  if (pixels)
    angle = (float)(atan2(dy, dx) / DTOR);
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
  pixSize = 1000.f * mWinApp->mShiftManager->GetPixelSize(imBuf);
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
