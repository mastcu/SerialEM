// StageMoveTool.cpp : A tool window for moving the stage by increments
//

#include "stdafx.h"
#include "SerialEM.h"
#include "CameraController.h"
#include "StageMoveTool.h"
#include "EMscope.h"
#include "ShiftManager.h"


// CStageMoveTool dialog

CStageMoveTool::CStageMoveTool(CWnd* pParent /*=NULL*/)
	: CBaseDlg(CStageMoveTool::IDD, pParent)
  , m_strStageX(_T(""))
  , m_strStageY(_T(""))
  , m_bImageAfterMove(FALSE)
{
  mNonModal = true;
  mOuterFrames = 2.;
  mLastUpdateX = 9999.;
  mLastUpdateY = 9999.;
  mGoingToAcquire = false;
  mLastImageSet = -1;
  mLastBusy = -1;
  mCamera = mWinApp->mCamera;
}

CStageMoveTool::~CStageMoveTool()
{
}

void CStageMoveTool::DoDataExchange(CDataExchange* pDX)
{
  CBaseDlg::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_STATSTAGEx, m_strStageX);
  DDX_Text(pDX, IDC_STATSTAGEY, m_strStageY);
  DDX_Control(pDX, IDC_STATSTAGEx, m_statStageX);
  DDX_Control(pDX, IDC_IMAGE_AFTER_MOVE, m_butImageAfterMove);
  DDX_Check(pDX, IDC_IMAGE_AFTER_MOVE, m_bImageAfterMove);
}


BEGIN_MESSAGE_MAP(CStageMoveTool, CBaseDlg)
  ON_WM_PAINT()
  ON_WM_LBUTTONDOWN()
  ON_BN_CLICKED(IDC_IMAGE_AFTER_MOVE, &CStageMoveTool::OnImageAfterMove)
END_MESSAGE_MAP()


// CStageMoveTool message handlers
BOOL CStageMoveTool::OnInitDialog()
{
  CBaseDlg::OnInitDialog();
  Invalidate();
  mWinApp->RestoreViewFocus();
  return FALSE;
}

void CStageMoveTool::PostNcDestroy() 
{
  delete this;  
  CDialog::PostNcDestroy();
}

void CStageMoveTool::OnCancel() 
{
  mWinApp->GetStageToolPlacement();
  mWinApp->mStageMoveTool = NULL;
  DestroyWindow();
}

void CStageMoveTool::OnImageAfterMove()
{
  UpdateData(true);
  mWinApp->RestoreViewFocus();
}

void CStageMoveTool::OnPaint()
{
  float relativeSizes[4] = {0.25, 0.625, 1.0, 1.375};
  COLORREF innerColor = RGB(0, 0, 160);
  COLORREF outerColor = RGB(192, 0, 0);
  COLORREF disableColor = RGB(128,128, 128);
  COLORREF fillColor = RGB(128, 128, 128);
  bool busy = TooBusyToMove();
  mLastBusy = busy ? 1 : 0;

  CPaintDC dc(this); // device context for painting
  CRect winRect;
  CRect statRect;
  CRect clientRect;
  int boxWidth, boxHeight, xcen, ycen, ring, delx = 0, dely = 0, lastDelx, lastDely;

  // Tried to have a square box like in the camera setup dialog, but it came through as
  // as non-square in screen pixels!  So find out a width and height of usable area
  // within the client rectangle and in the window above the label, take the minimum and
  // allow some border pixels
  GetWindowRect(&winRect);
  GetClientRect(&clientRect);
  m_butImageAfterMove.GetWindowRect(&statRect);
  boxWidth = clientRect.Width();
  boxHeight = statRect.top - winRect.top - 
    (winRect.Height() - clientRect.Height());
  xcen = boxWidth / 2;
  ycen = boxHeight / 2; 
  boxWidth = boxHeight = B3DMIN(boxWidth, boxHeight) - 10;

  // Define pens and a brush
  CPen pnSolid1(PS_SOLID, 1, busy ? disableColor : innerColor);
  CPen pnSolid2(PS_SOLID, 2, busy ? disableColor : innerColor);
  CPen pnOuter(PS_SOLID, 1, busy ? disableColor : outerColor);
  CPen pnDots(PS_DOT, 1, busy ? disableColor : outerColor);
  CBrush brush;
  brush.CreateSolidBrush(fillColor);
  CPen *oldPen = dc.SelectObject(&pnSolid1);
  CBrush *oldBrush = dc.SelectObject(&brush);

  // Draw the rings
  for (ring = 0; ring < 4; ring++) {
    if (ring == 2)
      dc.SelectObject(pnSolid2);
    else if (ring == 3)
      dc.SelectObject(pnOuter);

    lastDelx = delx;
    lastDely = dely;
    delx = B3DNINT(0.5 * boxWidth * relativeSizes[ring] / relativeSizes[3]);
    dely = B3DNINT(0.5 * boxHeight * relativeSizes[ring] / relativeSizes[3]);
    if (!ring) {
      dc.Ellipse(xcen - delx, ycen - dely, xcen + delx, ycen + dely);
      dc.SelectObject(oldBrush);
      mInnerRadius = delx;
    } else 
      dc.Arc(xcen - delx, ycen - dely, xcen + delx, ycen + dely, xcen - delx, ycen,
        xcen - delx, ycen);
  }

  // Draw dotted lines to outer ring
  dc.SelectObject(pnDots);
  dc.MoveTo(xcen + lastDelx + 1, ycen);
  dc.LineTo(xcen + delx, ycen);
  dc.MoveTo(xcen - lastDelx - 1, ycen);
  dc.LineTo(xcen - delx, ycen);
  dc.MoveTo(xcen, ycen + lastDely + 1);
  dc.LineTo(xcen, ycen + dely);
  dc.MoveTo(xcen, ycen - lastDely - 1);
  dc.LineTo(xcen, ycen - dely);
  dc.SelectObject(oldPen);
  //SEMTrace('1',"cen %d %d  del %d %d last %d %d", xcen, ycen, delx, dely, lastDelx, 
    //lastDely);

  // Save coordinates
  mXcenter = xcen;
  mYcenter = ycen;
  mUnitRadius = lastDelx;
  mOuterRadius = delx;
}

void CStageMoveTool::OnLButtonDown(UINT nFlags, CPoint point)
{
  float radius, fracX, fracY, pixels, xpix, ypix;
  int delx = point.x - mXcenter;
  int dely = point.y - mYcenter;
  ScaleMat bMat, bInv;
  int camera = mWinApp->GetCurrentCamera();
  CameraParameters *camParam = mWinApp->GetActiveCamParam();
  int frameSize = B3DMIN(camParam->sizeX, camParam->sizeY);
  CDialog::OnLButtonDown(nFlags, point);
  mWinApp->RestoreViewFocus();
  
  //SEMTrace('1', "DOWN  %d %d", point.x, point.y);
  if (TooBusyToMove())
    return;
  radius = (float)sqrt((double)delx * delx + dely * dely);
  fracX = delx / radius;
  fracY = -dely / radius;
  if (radius <= mInnerRadius || radius > mOuterRadius)
    return;
  if (radius > mUnitRadius)
    radius = (mOuterFrames * (radius - mUnitRadius) / (mOuterRadius - mUnitRadius)) *
      mUnitRadius;
  pixels = frameSize * radius / mUnitRadius;
  xpix = fracX * pixels;
  ypix = fracY * pixels;
  bMat = mWinApp->mShiftManager->StageToCamera(camera, mWinApp->mScope->GetMagIndex());
  bInv = MatInv(bMat);
  mDelStageX = -(bInv.xpx * xpix + bInv.xpy * ypix);
  mDelStageY = -(bInv.ypx * xpix + bInv.ypy * ypix);
  StartStageMove();
}

void CStageMoveTool::Update(void)
{
  if (mLastBusy != (TooBusyToMove() ? 1 : 0))
    Invalidate();
  m_butImageAfterMove.EnableWindow(!TooBusyToMove());
}

void CStageMoveTool::UpdateStage(double stageX, double stageY)
{
  CString *names = mWinApp->GetModeNames();
  int setNum = mWinApp->LowDoseMode() ? VIEW_CONSET : TRIAL_CONSET;
  if (setNum != mLastImageSet)
    m_butImageAfterMove.SetWindowText("Take " + names[setNum] + " after move");
  mLastImageSet = setNum;
  if (fabs(stageX - mLastUpdateX) < 3.e-3 && fabs(stageY - mLastUpdateY) < 3.e-3)
    return;
  m_strStageX.Format("%.2f", stageX);
  m_strStageY.Format("%.2f", stageY);
  mLastUpdateX = stageX;
  mLastUpdateY = stageY;
  UpdateData(false);
}

bool CStageMoveTool::TooBusyToMove(void)
{
  return (!mGoingToAcquire && mWinApp->DoingTasks()) || mWinApp->StartedTiltSeries() || 
    (mCamera->CameraBusy() && !mCamera->DoingContinuousAcquire()) ||
    mWinApp->mScope->StageBusy();
}

void CStageMoveTool::StartStageMove(void)
{
  StageMoveInfo smi;
  mWinApp->mScope->GetStagePosition(smi.x, smi.y, smi.z);
  smi.x += mDelStageX;
  smi.y += mDelStageY;
  smi.axisBits = axisXY;
  mGoingToAcquire = m_bImageAfterMove && !mCamera->DoingContinuousAcquire();
  mWinApp->mScope->MoveStage(smi);
  mWinApp->AddIdleTask(CEMscope::TaskStageBusy, TASK_STAGE_TOOL, 0, 120000);
}

void CStageMoveTool::StageToolNextTask(void)
{
  if (TooBusyToMove()) {
    mGoingToAcquire = false;
    return;
  }
  if (mGoingToAcquire) {
    mGoingToAcquire = false;
    mCamera->InitiateCapture(mLastImageSet);
    mWinApp->AddIdleTask(CCameraController::TaskCameraBusy, TASK_STAGE_TOOL, 0, 0);
    return;
  }
  if (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) / 2
    != 0)
    StartStageMove();
}

void CStageMoveTool::StopNextAcquire(void)
{
  mGoingToAcquire = false;
}
